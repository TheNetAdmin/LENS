#include "tasks.h"

int pointer_chasing_strided_job(void *arg)
{
	char *this_task_name		 = "TASK_PC_STRIDED";
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	uint64_t region_align		 = ctx->sbi->pc_region_align;
	uint64_t region_size		 = ctx->sbi->pc_region_size;
	uint64_t block_size		 = ctx->sbi->pc_block_size;
	uint64_t strided_size		 = ctx->sbi->strided_size;
	uint8_t *uc_addr		 = get_uc_addr(ctx, overlapped);
	uint8_t *buf			 = uc_addr;
	uint8_t *buf_end		 = buf + GLOBAL_WORKSET;
	uint64_t count			 = GLOBAL_WORKSET / region_size;
	uint64_t repeat			 = ctx->sbi->repeat;
	uint64_t op			 = ctx->sbi->op;
	int hash			 = 0;
	uint64_t dimm_size		 = get_dimm_size();

	long pages, diff;
	int i;
	unsigned ti, tj;
	uint64_t *cindex;
	/* TODO: check if timing overlaps cindex */
	uint64_t *timing = (uint64_t*)(ctx->sbi->rep->virt_addr);
	uint64_t csize;
	int ret;
	unsigned int chasing_func_index;
	static unsigned long flags;
	uint64_t region_skip = region_size;

	chasing_func_entry_t *chasing_funcs = NULL;
	bool back_and_forth = false;

	/* define variables for time measurements */
	PC_VARS;

	/* find pointer chasing benchmark */
	if (op == 4 || op == 5) {
		back_and_forth = true;
	} else {
		back_and_forth = false;
	}

	if (back_and_forth) {
		chasing_funcs = chasing_baf_func_list;
		chasing_func_index = chasing_baf_find_func(block_size);
	} else {
		chasing_funcs = chasing_func_list;
		chasing_func_index = chasing_find_func(block_size);
	}
	if (chasing_func_index == -1) {
		chasing_print_help();
		kr_info("Pointer chasing benchamrk with block size %lld byte not found\n",
			block_size);
		goto pointer_chasing_strided_job_end;
	}

	/* fix strided_size if overlapping */
	if (strided_size < block_size) {
		kr_info("ERROR: strided size (%llu) < block size (%llu)",
			strided_size, block_size);
		return 1;
	}

	/* decide region_skip */
	region_skip = (region_size / block_size) * strided_size;

	if (region_skip % region_align != 0) {
		region_skip = ((region_skip / region_align) + 1) * region_align;
	}

	if (region_skip > dimm_size) {
		kr_info("ERROR: Region skip %llu > usable DIMM size %llu\n",
			region_skip, dimm_size);
		return 1;
	}

	/*
	 * NOTE: to ensure pc-stride always work on only one region, not to
	 *       move to the next region. This is for multi repeats.
	 */
	count = 1;

	/* fill page tables */
	pages = roundup((count + 1) * region_skip, PAGE_SIZE) >> PAGE_SHIFT;
	for (i = 0; i < pages; i++) {
		buf  = uc_addr + (i << PAGE_SHIFT);
		hash = hash ^ *buf;
	}
	buf = uc_addr;

	kr_info("Working set begin: 0x%llx end: 0x%llx, phy-begin: %llx, region_size=%llu, "
		"region_skip=%llu, block_size=%llu, strided_size=%llu, "
		"func=%s, count=%llu\n",
		(uint64_t)buf,
		(uint64_t)buf_end,
		ctx->sbi->phys_addr,
		region_size,
		region_skip,
		block_size,
		strided_size,
		chasing_funcs[chasing_func_index].name,
		count);

	/* generate pointer chasing index in reportfs (in DRAM by default) */
	/* leave 16MiB space for benchamrks to store result data */
	csize  = region_size / block_size;
	cindex = (uint64_t *)((char *)(ctx->sbi->rep->virt_addr) + 16 * 1024 * 1024);
	if (back_and_forth) {
		ret    = init_chasing_baf_index(cindex, csize);
	} else {
		ret    = init_chasing_index(cindex, csize);
	}
	if (ret != 0) {
		pr_info("Chasing index generation failed\n");
		goto pointer_chasing_strided_job_end;
	}
	pr_info("Chasing index generation done\n");

	kr_info("csize %llu\n", csize);
	kr_info("cindex %px\n", cindex);

	/* Init timing buffer */
	TIMING_BUF_INIT(timing);

	/* start benchmark */
	switch (op) {
	case 1:
		/* 1: read after write */
		BENCHMARK_BEGIN(flags);

		PC_BEFORE_WRITE
		chasing_funcs[chasing_func_index].raw_func(
			buf, region_size, strided_size, region_skip, count,
			repeat, cindex, timing);
		asm volatile("mfence \n" :::);
		PC_BEFORE_READ
		PC_AFTER_READ

		PC_STRIDED_PRINT_MEASUREMENT(
			chasing_funcs[chasing_func_index]);
		kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
		CHASING_PRINT_RECORD_TIMING("lat_st", (timing));
		kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
		CHASING_PRINT_RECORD_TIMING("lat_ld", (timing + repeat * 2));
		kr_info("\n");
		BENCHMARK_END(flags);
	break;

	case 2:
		/* 2: write - new pointer chasing order every repeat */
		BENCHMARK_BEGIN(flags);

		for (i = 0; i < repeat; i++) {
			init_chasing_index(cindex, csize);

			PC_BEFORE_WRITE
			chasing_funcs[chasing_func_index].st_func(
				buf, region_size, strided_size, region_skip,
				count, 1, cindex, timing);
			asm volatile("mfence \n" :::);
			PC_BEFORE_READ
			PC_AFTER_READ

			PC_STRIDED_PRINT_MEASUREMENT(
				chasing_funcs[chasing_func_index]);
			kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING_BASE("lat_st", timing, 1, i);
			kr_info("\n");
		}
		BENCHMARK_END(flags);
	break;

	case 3:
		/* 3: raw - new pointer chasing order every repeat */
		BENCHMARK_BEGIN(flags);

		for (i = 0; i < repeat; i++) {
			init_chasing_index(cindex, csize);

			PC_BEFORE_WRITE
			chasing_funcs[chasing_func_index].raw_func(
				buf, region_size, strided_size, region_skip,
				count, 1, cindex, timing);
			asm volatile("mfence \n" :::);
			PC_BEFORE_READ
			PC_AFTER_READ

			PC_STRIDED_PRINT_MEASUREMENT(
				chasing_funcs[chasing_func_index]);
			kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING_BASE("lat_st", timing, 1, i);
			kr_info("\n");
		}
		BENCHMARK_END(flags);
	break;

	case 4:
		/* 4: read and write back and forth */
		BENCHMARK_BEGIN(flags);

		PC_BEFORE_WRITE
		chasing_funcs[chasing_func_index].st_func(
			buf, region_size, strided_size, region_skip, count,
			repeat, cindex, timing);
		asm volatile("mfence \n" :::);
		PC_BEFORE_READ
		chasing_funcs[chasing_func_index].ld_func(
			buf, region_size, strided_size, region_skip, count,
			repeat, cindex, timing + repeat * 2);
		asm volatile("mfence \n" :::);
		PC_AFTER_READ

		PC_STRIDED_PRINT_MEASUREMENT(
			chasing_funcs[chasing_func_index]);
		kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
		CHASING_PRINT_RECORD_TIMING("lat_st", (timing));
		kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
		CHASING_PRINT_RECORD_TIMING("lat_ld", (timing + repeat * 2));
		kr_info("\n");
		BENCHMARK_END(flags);
	break;

	case 5:
		/* 5: read after write back and forth */
		BENCHMARK_BEGIN(flags);

		PC_BEFORE_WRITE
		chasing_funcs[chasing_func_index].raw_func(
			buf, region_size, strided_size, region_skip, count,
			repeat, cindex, timing);
		asm volatile("mfence \n" :::);
		PC_BEFORE_READ
		PC_AFTER_READ

		PC_STRIDED_PRINT_MEASUREMENT(
			chasing_funcs[chasing_func_index]);
		kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
		CHASING_PRINT_RECORD_TIMING("lat_st", (timing));
		kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
		CHASING_PRINT_RECORD_TIMING("lat_ld", (timing + repeat * 2));
		kr_info("\n");
		BENCHMARK_END(flags);
	break;

	case 0:
	default:
		/* 0 (default): read and write */
		BENCHMARK_BEGIN(flags);

		PC_BEFORE_WRITE
		chasing_funcs[chasing_func_index].st_func(
			buf, region_size, strided_size, region_skip, count,
			repeat, cindex, timing);
		asm volatile("mfence \n" :::);
		PC_BEFORE_READ
		chasing_funcs[chasing_func_index].ld_func(
			buf, region_size, strided_size, region_skip, count,
			repeat, cindex, timing + repeat * 2);
		asm volatile("mfence \n" :::);
		PC_AFTER_READ

		PC_STRIDED_PRINT_MEASUREMENT(
			chasing_funcs[chasing_func_index]);
		kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
		CHASING_PRINT_RECORD_TIMING("lat_st", (timing));
		kr_info("[%s] ", chasing_funcs[chasing_func_index].name);
		CHASING_PRINT_RECORD_TIMING("lat_ld", (timing + repeat * 2));
		kr_info("\n");
		BENCHMARK_END(flags);
	break;
	}

pointer_chasing_strided_job_end:
	/* TODO: after adding `repeat` and `op`, etc, the following log print 
	 *       is not unique anymore, needs udpate
	 */
	kr_info("%s_END: %llu-%llu-%llu\n", this_task_name, region_size,
		block_size, strided_size);
	wait_for_stop();
	return 0;
}
