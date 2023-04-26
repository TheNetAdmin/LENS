#include "tasks.h"

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

	kr_info("Working set begin: 0x%llx end: 0x%llx, size=%ld, stride=%ld, delay=%ld, batch=%ld\n",
		(uint64_t)buf, (uint64_t)region_end, access_size, stride_size, delay, count);

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
