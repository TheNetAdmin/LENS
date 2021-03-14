#include "tasks.h"

/* 
 * Pointer chasing read AND write task.
 * It performs a pointer chasing write with sufficiently large repeat rounds,
 *   then a pointer chasing read.
 *
 * e.g. with 16KB region_size and 64 Byte block_size, this task performs:
 *   0. Record cycle `c_store_start`
 *   1. Run `chasing_stnt_64` function on many consecutive 16KB regions (the
 *        number of total regions is calculated as `count`)
 *   2. Run `mfence`
 *   3. Record cycle `c_load_start`
 *   4. Run `chasing_ldnt_64` function on all the 16KB regions in step 1
 *   5. Run `mfence`
 *   6. Record cycle `c_load_end`
 *
 * In read AND write task, the write and read latencies are measured separately:
 *   write latency is `(c_load_start - c_store_start)/count`
 *   read  latency is `(c_load_end   - c_load_start )/count`
 * 
 * The pointer chasing read AFTER write task is a different task:
 *   `pointer_chasing_read_after_write_job`.
 * 
 * e.g. with 16KB region_size and 64 Byte block_size, the read AFTER write task
 *   performs:
 *   0. Record cycle `c_store_start`
 *   1. Run `chasing_stnt_64` function on ONE 16KB region
 *   2. Run `mfence`
 *   3. Run `chasing_ldnt_64` function on the 16KB region in step 1
 *   4. Run `mfence`
 *   5. Repeat step 1-4 for `count` rounds, on different 16KB regions
 *   6. Record cycle `c_load_start`
 *   7. Record cycle `c_load_end`
 * 
 * In read AFTER write task described above, the read_after_write latency is
 *   measured as `c_load_start - c_store_start`, which is different from the
 *   read AND write task.
 */
int pointer_chasing_read_and_write_job(void *arg)
{
	char *this_task_name		 = "TASK_PC_LD_ST";
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	uint64_t region_size		 = ctx->sbi->pc_region_size;
	uint64_t block_size		 = ctx->sbi->pc_block_size;
	uint8_t *buf			 = ctx->addr;
	uint8_t *buf_end		 = buf + GLOBAL_WORKSET;
	uint64_t count			 = GLOBAL_WORKSET / region_size;
	int hash			 = 0;

	long pages, diff;
	int i;
	uint64_t *cindex;
	uint64_t *timing = (uint64_t*)(ctx->sbi->rep->virt_addr);
	uint64_t csize;
	int ret;
	unsigned int chasing_func_index;
	static unsigned long flags;

	/* define variables for time measurements */
	PC_VARS;

	/* find pointer chasing benchmark */
	chasing_func_index = chasing_find_func(block_size);
	if (chasing_func_index == -1) {
		chasing_print_help();
		kr_info("Pointer chasing benchamrk with block size %lld byte not found\n",
			block_size);
		goto pointer_chasing_read_and_write_job_end;
	}

	/* decide test rounds */
	if (region_size * count > GLOBAL_WORKSET)
		count = GLOBAL_WORKSET / region_size;

	if (count > LATENCY_OPS_COUNT)
		count = LATENCY_OPS_COUNT;

	if (region_size <= 512)
		count *= 8;

	if (region_size >= 16384)
		count /= 2;

	if (region_size >= 32768)
		count /= 2;

	if (region_size >= 64 * 1024)
		count /= 2;

	if (region_size >= 128 * 1024)
		count /= 2;

	if (region_size >= 512 * 1024)
		count /= 2;

	/* fill page tables */
	pages = roundup(count * region_size, PAGE_SIZE) >> PAGE_SHIFT;
	for (i = 0; i < pages; i++) {
		buf = ctx->addr + (i << PAGE_SHIFT);
		hash = hash ^ *buf;
	}
	buf = ctx->addr;

	kr_info("Working set begin: 0x%llx end: 0x%llx, region_size=%lld, "
		"block_size=%lld, func=%s, count=%lld\n",
		(uint64_t)buf, (uint64_t)buf_end, region_size, block_size,
		chasing_func_list[chasing_func_index].name, count);

	/* generate pointer chasing index in reportfs (in DRAM by default) */
	csize = region_size /
		(chasing_func_list[chasing_func_index].block_size);
	cindex = (uint64_t *)((char *)(ctx->sbi->rep->virt_addr) + 16 * 1024 * 1024);
	ret = init_chasing_index(cindex, csize);
	if (ret != 0) {
		pr_info("Chasing index generation failed\n");
		goto pointer_chasing_read_and_write_job_end;
	}

	/* start benchmark */
	BENCHMARK_BEGIN(flags);

	PC_BEFORE_WRITE
	chasing_func_list[chasing_func_index].st_func(
		buf, block_size, region_size, region_size, count, 1, cindex, timing);
	asm volatile("mfence \n" :::);
	PC_BEFORE_READ
	chasing_func_list[chasing_func_index].ld_func(
		buf, block_size, region_size, region_size, count, 1, cindex, timing);
	asm volatile("mfence \n" :::);
	PC_AFTER_READ

	kr_info("csize %llu\n", csize);
	PC_PRINT_MEASUREMENT(chasing_func_list[chasing_func_index]);

	BENCHMARK_END(flags);

pointer_chasing_read_and_write_job_end:
	kr_info("%s_END: %llu-%llu\n", this_task_name, region_size, block_size);
	wait_for_stop();
	return 0;
}
