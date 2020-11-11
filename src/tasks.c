/* 
 *  Copyright (c) 2020 Zixuan Wang <zxwang@ucsd.edu>
 *  Copyright (c) 2019 Jian Yang
 *                     Regents of the University of California,
 *                     UCSD Non-Volatile Systems Lab
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include <linux/compiler.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/parser.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/magic.h>
#include <linux/random.h>
#include <linux/cred.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <asm/simd.h>
#include <asm/fpu/api.h>
#include <linux/kthread.h>
#include "lat.h"
#include "memaccess.h"
#include "chasing.h"
#include "latency.h"

#ifdef USE_PERF
#include "perf_util.h"
#endif

//#define KSTAT 1


#define kr_info(string, args...)                                               \
	do {                                                                   \
		printk(KERN_INFO "{%d}" string, ctx->core_id, ##args);         \
	} while (0)

inline void wait_for_stop(void)
{
	/* Wait until we are told to stop */
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (kthread_should_stop())
			break;
		schedule();
	}
}

#define BENCHMARK_BEGIN(flags)                                                 \
	drop_cache();                                                          \
	local_irq_save(flags);                                                 \
	local_irq_disable();                                                   \
	kernel_fpu_begin();

#define BENCHMARK_END(flags)                                                   \
	kernel_fpu_end();                                                      \
	local_irq_restore(flags);                                              \
	local_irq_enable();

#ifdef USE_PERF

#define PERF_CREATE(x) lattest_perf_event_create()
#define PERF_START(x)                                                          \
	lattest_perf_event_reset();                                            \
	lattest_perf_event_enable();
#define PERF_STOP(x)                                                           \
	lattest_perf_event_disable();                                          \
	lattest_perf_event_read();
#define PERF_PRINT(x)                                                          \
	kr_info("Performance counters:\n");                                    \
	lattest_perf_event_print();
#define PERF_RELEASE(x) lattest_perf_event_release();

#else // USE_PERF

#define PERF_CREATE(x)                                                         \
	do {                                                                   \
	} while (0)
#define PERF_START(x)                                                          \
	do {                                                                   \
	} while (0)
#define PERF_STOP(x)                                                           \
	do {                                                                   \
	} while (0)
#define PERF_PRINT(x)                                                          \
	do {                                                                   \
	} while (0)
#define PERF_RELEASE(x)                                                        \
	do {                                                                   \
	} while (0)

#endif // USE_PERF

#define TIMEDIFF(start, end)                                                   \
	((end.tv_sec - start.tv_sec) * 1000000000 +                            \
	 (end.tv_nsec - start.tv_nsec))

/* Latency Test
 * 1 Thread
 * Run a series of microbenchmarks on combination of store, flush and fence operations.
 * Sequential -> Random
 */
int latency_job(void *arg)
{
	int i, j;
	static unsigned long flags;
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	struct latency_sbi *sbi		 = ctx->sbi;
	long *loc			 = (long *)sbi->rep->virt_addr;
	u8 *buf;
	long result;

	long *rbuf;
	int rpages;
#ifdef KSTAT
	long total;
	long average, stddev;
	long min, max;
#endif
	u8 hash = 0;
	int skip;

	kr_info("BASIC_OP at smp id %d\n", smp_processor_id());
	rpages = roundup((2 * BASIC_OPS_TASK_COUNT + 1) * LATENCY_OPS_COUNT *
				 sizeof(long),
			 PAGE_SIZE) >>
		 PAGE_SHIFT;
	/* fill report area page tables */
	for (j = 0; j < rpages; j++) {
		buf  = (u8 *)sbi->rep->virt_addr + (j << PAGE_SHIFT);
		hash = hash ^ *buf;
	}

	/* Sequential */
	for (i = 0; i < BASIC_OPS_TASK_COUNT; i++) {
		/* fill page tables */
		for (j = 0; j < BASIC_OP_POOL_PAGES; j++) {
			buf  = ctx->addr + (j << PAGE_SHIFT);
			hash = hash ^ *buf;
		}

		kr_info("Running %s\n", latency_tasks_str[i]);
		buf = ctx->addr;

		skip = latency_tasks_skip[i]; /* request size */
#ifdef KSTAT
		total = 0;
		min   = 0xffffffff;
		max   = 0;
#endif
		rbuf = (long *)(sbi->rep->virt_addr +
				(i + 1) * LATENCY_OPS_COUNT * sizeof(long));
		BENCHMARK_BEGIN(flags);
		for (j = 0; j < LATENCY_OPS_COUNT; j++) {
			result = bench_func[i](buf);
			buf += skip;
			rbuf[j] = result;
#ifdef KSTAT
			if (result < min)
				min = result;
			if (result > max)
				max = result;
			total += result;
#endif
		}
		BENCHMARK_END(flags);
#ifdef KSTAT
		average = total / LATENCY_OPS_COUNT;
		stddev	= 0;
		for (j = 0; j < LATENCY_OPS_COUNT; j++) {
			stddev += (rbuf[j] - average) * (rbuf[j] - average);
		}
		kr_info("[%d]%s avg %lu, stddev^2 %lu, max %lu, min %lu\n", i,
			latency_tasks_str[i], average,
			stddev / LATENCY_OPS_COUNT, max, min);
#else
		kr_info("[%d]%s Done\n", i, latency_tasks_str[i]);
#endif
	}

	/* Random */
	for (i = 0; i < BASIC_OPS_TASK_COUNT; i++) {
		kr_info("Generating random bytes at %p, size %lx", loc,
			LATENCY_OPS_COUNT * 8);
		get_random_bytes(sbi->rep->virt_addr, LATENCY_OPS_COUNT * 8);

		/* fill page tables */
		for (j = 0; j < BASIC_OP_POOL_PAGES; j++) {
			buf  = ctx->addr + (j << PAGE_SHIFT);
			hash = hash ^ *buf;
		}

		kr_info("Running %s\n", latency_tasks_str[i]);
		buf = ctx->addr;
#ifdef KSTAT
		total = 0;
		min   = 0xffffffff;
		max   = 0;
#endif
		rbuf = (long *)(sbi->rep->virt_addr +
				(i + BASIC_OPS_TASK_COUNT + 1) *
					LATENCY_OPS_COUNT * sizeof(long));
		BENCHMARK_BEGIN(flags);
		for (j = 0; j < LATENCY_OPS_COUNT; j++) {
			result	= bench_func[i](&buf[loc[j] & BASIC_OP_MASK]);
			rbuf[j] = result;
#ifdef KSTAT
			if (result < min)
				min = result;
			if (result > max)
				max = result;
			total += result;
#endif
		}
		BENCHMARK_END(flags);

#ifdef KSTAT
		average = total / LATENCY_OPS_COUNT;
		stddev	= 0;
		for (j = 0; j < LATENCY_OPS_COUNT; j++) {
			stddev += (rbuf[j] - average) * (rbuf[j] - average);
		}
		kr_info("[%d]%s avg %lu, stddev^2 %lu, max %lu, min %lu\n", i,
			latency_tasks_str[i], average,
			stddev / LATENCY_OPS_COUNT, max, min);
#else
		kr_info("[%d]%s done\n", i, latency_tasks_str[i]);
#endif
	}
	kr_info("hash %d", hash);
	kr_info("LATTester_LAT_END:\n");
	wait_for_stop();
	return 0;
}

int strided_latjob(void *arg)
{
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	static unsigned long flags;
	long access_size    = ctx->sbi->access_size;
	long stride_size    = ctx->sbi->strided_size;
	long delay	    = ctx->sbi->delay;
	uint8_t *buf	    = ctx->addr;
	uint8_t *region_end = buf + GLOBAL_WORKSET;
	long count	    = GLOBAL_WORKSET / access_size;
	struct timespec tstart, tend;
	long pages, diff;
	int hash = 0;
	int i;

	PERF_CREATE();

	if (stride_size * count > GLOBAL_WORKSET)
		count = GLOBAL_WORKSET / stride_size;

	if (count > LATENCY_OPS_COUNT)
		count = LATENCY_OPS_COUNT;

	pages = roundup(count * stride_size, PAGE_SIZE) >> PAGE_SHIFT;

	/* fill page tables */
	for (i = 0; i < pages; i++) {
		buf  = ctx->addr + (i << PAGE_SHIFT);
		hash = hash ^ *buf;
	}

	buf = ctx->addr;

	kr_info("Working set begin: %p end: %p, size=%ld, stride=%ld, delay=%ld, batch=%ld\n",
		buf, region_end, access_size, stride_size, delay, count);

	for (i = 0; i < BENCH_SIZE_TASK_COUNT; i++) {
		kr_info("Running %s\n", bench_size_map[i]);
		BENCHMARK_BEGIN(flags);

		getrawmonotonic(&tstart);
		PERF_START();

		lfs_stride_bw[i](buf, access_size, stride_size, delay, count);
		asm volatile("mfence \n" :::);

		PERF_STOP();
		getrawmonotonic(&tend);

		diff = TIMEDIFF(tstart, tend);
		BENCHMARK_END(flags);
		kr_info("[%d]%s count %ld total %ld ns, average %ld ns.\n", i,
			latency_tasks_str[i], count, diff, diff / count);

		PERF_PRINT();
	}
	PERF_RELEASE();
	kr_info("LATTester_LAT_END: %ld-%ld\n", access_size, stride_size);
	wait_for_stop();
	return 0;
}

int sizebw_job(void *arg)
{
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	struct latency_sbi *sbi		 = ctx->sbi;
	int op				 = ctx->sbi->op;
	struct latencyfs_timing *timing	 = &sbi->timing[ctx->core_id];
	int bit_length			 = sbi->bwsize_bit;
	long align_size			 = 1 << bit_length;
	long access_size		 = sbi->access_size;
	long count			 = PERTHREAD_CHECKSTOP / access_size;
	uint8_t *buf			 = ctx->addr;
	uint8_t *region_end		 = buf + PERTHREAD_WORKSET;
	static unsigned long flags;
	unsigned long bitmask;
	unsigned long seed;

	if (sbi->align == ALIGN_GLOBAL) {
		bitmask = (GLOBAL_WORKSET - 1) ^ (align_size - 1);
	} else if (sbi->align == ALIGN_PERTHREAD) {
		bitmask = (PERTHREAD_WORKSET - 1) ^ (align_size - 1);
	} else {
		kr_info("Unsupported align mode %d\n", sbi->align);
		return 0;
	}

	get_random_bytes(&seed, sizeof(unsigned long));
	kr_info("Working set begin: %p end: %p, access_size=%ld, align_size=%ld, batch=%ld, bitmask=%lx, seed=%lx\n",
		buf, region_end, access_size, align_size, count, bitmask, seed);

	BENCHMARK_BEGIN(flags);
	while (1) {
		lfs_size_bw[op](buf, access_size, count, &seed, bitmask);
		timing->v += access_size * count;

		if (kthread_should_stop())
			break;
		schedule(); // Schedule with IRQ disabled?
	}

	BENCHMARK_END(flags);
	wait_for_stop();

	return 0;
}

int strided_bwjob(void *arg)
{
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	struct latency_sbi *sbi		 = ctx->sbi;
	int op				 = ctx->sbi->op;
	long access_size		 = ctx->sbi->access_size;
	long stride_size		 = ctx->sbi->strided_size;
	long delay			 = ctx->sbi->delay;
	struct latencyfs_timing *timing	 = &sbi->timing[ctx->core_id];
	uint8_t *buf			 = ctx->addr;
	uint8_t *region_end		 = buf + PERTHREAD_WORKSET;
	long count			 = PERTHREAD_CHECKSTOP / access_size;
	static unsigned long flags;

	if (stride_size * count > PERTHREAD_WORKSET)
		count = PERTHREAD_WORKSET / stride_size;

	kr_info("Working set begin: %p end: %p, size=%ld, stride=%ld, delay=%ld, batch=%ld\n",
		buf, region_end, access_size, stride_size, delay, count);

	BENCHMARK_BEGIN(flags);

	while (1) {
		lfs_stride_bw[op](buf, access_size, stride_size, delay, count);
		timing->v += access_size * count;
		buf += stride_size * count;
		if (buf + access_size * count >= region_end)
			buf = ctx->addr;

		if (kthread_should_stop())
			break;
		schedule();
	}

	BENCHMARK_END(flags);
	wait_for_stop();

	return 0;
}

int overwrite_job(void *arg)
{
	int j;
	static unsigned long flags;
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	struct latency_sbi *sbi		 = ctx->sbi;
	int skip			 = 256;
	long access_size		 = sbi->access_size;
	// int skip = sbi->strided_size;
	u8 *buf;
	u8 *buf_start;
	u8 *buf_end;

	long *rbuf;
	int rpages;

	u8 hash = 0;

	kr_info("OVERWRITE_OP at smp id %d\n", smp_processor_id());
	rpages = roundup((2 * BASIC_OPS_TASK_COUNT + 1) * LATENCY_OPS_COUNT *
				 sizeof(long),
			 PAGE_SIZE) >>
		 PAGE_SHIFT;
	/* fill report area page tables */
	for (j = 0; j < rpages; j++) {
		buf  = (u8 *)sbi->rep->virt_addr + (j << PAGE_SHIFT);
		hash = hash ^ *buf;
	}

	/* fill page tables */
	for (j = 0; j < BASIC_OP_POOL_PAGES; j++) {
		buf  = ctx->addr + (j << PAGE_SHIFT);
		hash = hash ^ *buf;
	}

	buf	  = ctx->addr + sbi->strided_size;
	buf_start = ctx->addr + sbi->strided_size;
	buf_end	  = buf + access_size;

	rbuf = (long *)(sbi->rep->virt_addr);
	BENCHMARK_BEGIN(flags);
	//for (j = 0; j < 2*LATENCY_OPS_COUNT/(access_size/256); j++)
	for (j = 0; j < 2 * LATENCY_OPS_COUNT; j++) {
		// rbuf[j] = repeat_ntstore(buf, access_size);
		rbuf[j] = repeat_256byte_ntstore(buf);
		buf += skip;
		if (buf >= buf_end)
			buf = buf_start;
	}
	BENCHMARK_END(flags);

	kr_info("Done. hash %d", hash);
	kr_info("TASK_OVERWRITE_END:\n");
	wait_for_stop();
	return 0;
}

#define PC_VARS                                                                \
	struct timespec tstart, tend;                                          \
	unsigned int c_store_start_hi, c_store_start_lo;                       \
	unsigned int c_load_start_hi, c_load_start_lo;                         \
	unsigned int c_load_end_hi, c_load_end_lo;                             \
	unsigned long c_store_start;                                           \
	unsigned long c_load_start, c_load_end;

#define PC_BEFORE_WRITE                                                        \
	getrawmonotonic(&tstart);                                              \
	asm volatile("rdtscp \n\t"                                             \
		     "lfence \n\t"                                             \
		     "mov %%edx, %[hi]\n\t"                                    \
		     "mov %%eax, %[lo]\n\t"                                    \
		     :                                                         \
		     [hi] "=r"(c_store_start_hi), [lo] "=r"(c_store_start_lo)  \
		     :                                                         \
		     : "rdx", "rax", "rcx");

#define PC_BEFORE_READ                                                         \
	asm volatile("rdtscp \n\t"                                             \
		     "lfence \n\t"                                             \
		     "mov %%edx, %[hi]\n\t"                                    \
		     "mov %%eax, %[lo]\n\t"                                    \
		     : [hi] "=r"(c_load_start_hi), [lo] "=r"(c_load_start_lo)  \
		     :                                                         \
		     : "rdx", "rax", "rcx");

#define PC_AFTER_READ                                                          \
	asm volatile("rdtscp \n\t"                                             \
		     "lfence \n\t"                                             \
		     "mov %%edx, %[hi]\n\t"                                    \
		     "mov %%eax, %[lo]\n\t"                                    \
		     : [hi] "=r"(c_load_end_hi), [lo] "=r"(c_load_end_lo)      \
		     :                                                         \
		     : "rdx", "rax", "rcx");                                   \
	getrawmonotonic(&tend);

#define PC_PRINT_MEASUREMENT(job_name)                                         \
	diff = TIMEDIFF(tstart, tend);                                         \
	c_store_start =                                                        \
		(((unsigned long)c_store_start_hi) << 32) | c_store_start_lo;  \
	c_load_start =                                                         \
		(((unsigned long)c_load_start_hi) << 32) | c_load_start_lo;    \
	c_load_end = (((unsigned long)c_load_end_hi) << 32) | c_load_end_lo;   \
	kr_info("buf_addr %p\n", buf);                                         \
	kr_info("[%s] region_size %ld, block_size %ld, count %ld, "            \
		"total %ld ns, average %ld ns, cycle %ld - %ld - %ld, "        \
		"ld_fence %s, st_fence %s.\n",                                 \
		job_name, region_size, block_size, count, diff, diff / count,  \
		c_store_start, c_load_start, c_load_end,                       \
		CHASING_LD_FENCE_TYPE, CHASING_ST_FENCE_TYPE);

/* 
 * Pointer chasing write ONLY task.
 */
int pointer_chasing_write_job(void *arg)
{
	char *this_task_name		 = "TASK_PC_ST";
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	long region_size		 = ctx->sbi->pc_region_size;
	long block_size			 = ctx->sbi->pc_block_size;
	uint8_t *buf			 = ctx->addr;
	uint8_t *buf_end		 = buf + GLOBAL_WORKSET;
	long count			 = GLOBAL_WORKSET / region_size;
	int hash			 = 0;

	long pages, diff;
	int i;
	uint64_t *cindex;
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
		kr_info("Pointer chasing benchamrk with block size %ld byte not found\n",
			block_size);
		goto pointer_chasing_write_job_end;
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

	kr_info("Working set begin: %p end: %p, region_size=%ld, "
		"block_size=%ld, func=%s, count=%ld\n",
		buf, buf_end, region_size, block_size,
		chasing_func_list[chasing_func_index].name, count);

	/* generate pointer chasing index in reportfs (in DRAM by default) */
	csize = region_size /
		(chasing_func_list[chasing_func_index].block_size);
	cindex = (uint64_t *)(ctx->sbi->rep->virt_addr);
	ret = init_chasing_index(cindex, csize);
	if (ret != 0) {
		pr_info("Chasing index generation failed\n");
		goto pointer_chasing_write_job_end;
	}

	/* start benchmark */
	BENCHMARK_BEGIN(flags);

	PC_BEFORE_WRITE
	chasing_func_list[chasing_func_index].st_func(
		buf, region_size, region_size, count, cindex);
	asm volatile("mfence \n" :::);
	PC_BEFORE_READ
	PC_AFTER_READ

	kr_info("csize %llu\n", csize);
	PC_PRINT_MEASUREMENT(chasing_func_list[chasing_func_index].name);

	BENCHMARK_END(flags);

pointer_chasing_write_job_end:
	kr_info("%s_END: %ld-%ld\n", this_task_name, region_size, block_size);
	wait_for_stop();
	return 0;
}

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
	long region_size		 = ctx->sbi->pc_region_size;
	long block_size			 = ctx->sbi->pc_block_size;
	uint8_t *buf			 = ctx->addr;
	uint8_t *buf_end		 = buf + GLOBAL_WORKSET;
	long count			 = GLOBAL_WORKSET / region_size;
	int hash			 = 0;

	long pages, diff;
	int i;
	uint64_t *cindex;
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
		kr_info("Pointer chasing benchamrk with block size %ld byte not found\n",
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

	kr_info("Working set begin: %p end: %p, region_size=%ld, "
		"block_size=%ld, func=%s, count=%ld\n",
		buf, buf_end, region_size, block_size,
		chasing_func_list[chasing_func_index].name, count);

	/* generate pointer chasing index in reportfs (in DRAM by default) */
	csize = region_size /
		(chasing_func_list[chasing_func_index].block_size);
	cindex = (uint64_t *)(ctx->sbi->rep->virt_addr);
	ret = init_chasing_index(cindex, csize);
	if (ret != 0) {
		pr_info("Chasing index generation failed\n");
		goto pointer_chasing_read_and_write_job_end;
	}

	/* start benchmark */
	BENCHMARK_BEGIN(flags);

	PC_BEFORE_WRITE
	chasing_func_list[chasing_func_index].st_func(
		buf, region_size, region_size, count, cindex);
	asm volatile("mfence \n" :::);
	PC_BEFORE_READ
	chasing_func_list[chasing_func_index].ld_func(
		buf, region_size, region_size, count, cindex);
	asm volatile("mfence \n" :::);
	PC_AFTER_READ

	kr_info("csize %llu\n", csize);
	PC_PRINT_MEASUREMENT(chasing_func_list[chasing_func_index].name);

	BENCHMARK_END(flags);

pointer_chasing_read_and_write_job_end:
	kr_info("%s_END: %ld-%ld\n", this_task_name, region_size, block_size);
	wait_for_stop();
	return 0;
}

int pointer_chasing_read_after_write_job(void *arg)
{
	char *this_task_name		 = "TASK_PC_RAW";
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	long region_size		 = ctx->sbi->pc_region_size;
	long block_size			 = ctx->sbi->pc_block_size;
	uint8_t *buf			 = ctx->addr;
	uint8_t *buf_end		 = buf + GLOBAL_WORKSET;
	long count			 = GLOBAL_WORKSET / region_size;
	int hash			 = 0;

	long pages, diff;
	int i;
	uint64_t *cindex;
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
		kr_info("Pointer chasing benchamrk with block size %ld byte not found\n",
			block_size);
		goto pointer_chasing_read_after_write_job_end;
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

	kr_info("Working set begin: %p end: %p, region_size=%ld, "
		"block_size=%ld, func=%s, count=%ld\n",
		buf, buf_end, region_size, block_size,
		chasing_func_list[chasing_func_index].name, count);

	/* generate pointer chasing index in reportfs (in DRAM by default) */
	csize = region_size /
		(chasing_func_list[chasing_func_index].block_size);
	cindex = (uint64_t *)(ctx->sbi->rep->virt_addr);
	ret = init_chasing_index(cindex, csize);
	if (ret != 0) {
		pr_info("Chasing index generation failed\n");
		goto pointer_chasing_read_after_write_job_end;
	}

	/* start benchmark */
	BENCHMARK_BEGIN(flags);

	PC_BEFORE_WRITE
	chasing_func_list[chasing_func_index].raw_func(
		buf, region_size, region_size, count, cindex);
	asm volatile("mfence \n" :::);
	PC_BEFORE_READ
	PC_AFTER_READ

	kr_info("csize %llu\n", csize);
	PC_PRINT_MEASUREMENT(chasing_func_list[chasing_func_index].name);

	BENCHMARK_END(flags);

pointer_chasing_read_after_write_job_end:
	kr_info("%s_END: %ld-%ld\n", this_task_name, region_size, block_size);
	wait_for_stop();
	return 0;
}

int flush_first_job(void *arg)
{
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	static unsigned long flags;
	long access_size		       = ctx->sbi->access_size;
	long stride_size		       = ctx->sbi->strided_size;
	long delay			       = ctx->sbi->delay;
	int op				       = ctx->sbi->op;
	static unsigned long flush_region_size = 64 * 1024;
	unsigned long workset_size	       = GLOBAL_WORKSET;
	uint8_t *buf			       = ctx->addr;
	uint8_t *region_start		       = buf;
	uint8_t *region_end = region_start + workset_size + flush_region_size;
	uint8_t *region_flush;
	long count = workset_size / access_size;
	long pages, diff;
	int hash = 0;
	int i;
	uint64_t *cindex;
	uint64_t csize;
	int ret;
	uint64_t cycle	     = 0;
	int64_t cycle_signed = 0;
	int workload_id	     = 0;
	struct timespec tstart, tend;

	if (stride_size * count > workset_size) {
		count = workset_size / stride_size;
	}

	if (count > LATENCY_OPS_COUNT) {
		count = LATENCY_OPS_COUNT;
	}

	//if (access_size >= 4 * 1024) {
	//    count /= 2;
	//}

	if (access_size >= 8 * 1024) {
		count /= 2;
	}

	if (access_size >= 16 * 1024) {
		count /= 2;
	}

	if (access_size >= 32 * 1024) {
		count /= 2;
	}

	if (access_size >= 64 * 1024) {
		count /= 2;
	}

	if (access_size >= 128 * 1024) {
		count /= 2;
	}

	if (access_size >= 512 * 1024) {
		count /= 2;
	}

	region_flush = region_start + (count + 1) * access_size;

	pages = roundup(count * stride_size, PAGE_SIZE) >> PAGE_SHIFT;

	/* fill page tables */
	for (i = 0; i < pages; i++) {
		buf  = ctx->addr + (i << PAGE_SHIFT);
		hash = hash ^ *buf;
	}

	buf = ctx->addr;

	kr_info("Working set begin: %p end: %p, size=%ld, stride=%ld, delay=%ld, batch=%ld\n",
		region_start, region_end, access_size, stride_size, delay,
		count);

#define CHASING_INIT(x)                                                        \
	csize = access_size / CACHELINE_SIZE;                                  \
	cindex = (uint64_t *)(ctx->sbi->rep->virt_addr);                       \
	ret = init_chasing_index(cindex, csize);                               \
	if (ret != 0)                                                          \
		return ret;

#define FF_JOB_EXEC(func, fence_type)                                          \
	getrawmonotonic(&tstart);                                              \
	cycle = 0;                                                             \
	cycle = func(region_start, access_size, stride_size, count, cindex,    \
		     region_flush);                                            \
	getrawmonotonic(&tend);                                                \
	diff = TIMEDIFF(tstart, tend);                                         \
	cycle_signed = (int64_t)cycle;                                         \
	if (cycle_signed < 0)                                                  \
		cycle = (uint64_t)(-cycle_signed);                             \
	kr_info("[%s] count %ld cycle %llu total %ld ns fence %s.\n", #func,   \
		count, cycle, diff, fence_type);                               \
	workload_id++;

	BENCHMARK_BEGIN(flags);

	CHASING_INIT();
	switch (op) {
	case 0:
		FF_JOB_EXEC(chasing_storent_ff, "mfence");
		break;
	case 1:
		FF_JOB_EXEC(chasing_loadnt_ff, "mfence");
		break;
	case 2:
		FF_JOB_EXEC(chasing_combined_stld_ff, "mfence");
		break;
	default:
		break;
	}

	BENCHMARK_END(flags);

#undef FF_JOB_EXEC
#undef CHASING_INIT

	kr_info("TASK_FF_END: %ld-%ld-%d\n", access_size, stride_size, op);
	wait_for_stop();
	return 0;
}

int seq_bwjob(void *arg)
{
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	struct latency_sbi *sbi		 = ctx->sbi;
	struct latencyfs_timing *timing	 = &sbi->timing[ctx->core_id];
	uint8_t *buf			 = ctx->addr;
	uint8_t *buf_end		 = buf + PERTHREAD_WORKSET;
	int op				 = sbi->op;
	long access_size		 = sbi->access_size; /* set in proc.c */
	long count			 = PERTHREAD_CHECKSTOP / access_size;
	static unsigned long flags;
	long step;

	if (access_size * count > PERTHREAD_WORKSET)
		count = PERTHREAD_WORKSET / access_size;
	step = count * access_size;

	BENCHMARK_BEGIN(flags);
	while (1) {
		lfs_seq_bw[op](buf, buf + step, access_size);
		timing->v += step;
		buf += step;
		if (buf >= buf_end)
			buf = ctx->addr;

		if (kthread_should_stop())
			break;
		schedule();
	}
	BENCHMARK_END(flags);
	wait_for_stop();

	return 0;
}
