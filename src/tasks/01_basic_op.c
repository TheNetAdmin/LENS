/*
 * Latency Test
 * 1 Thread
 * Run a series of microbenchmarks on combination of store, flush and fence operations.
 * Sequential -> Random
 */

#include "tasks.h"

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
		kr_info("Generating random bytes at %px, size %lx", loc,
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
