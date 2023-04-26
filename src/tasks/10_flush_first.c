#include "tasks.h"

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

	kr_info("Working set begin: 0x%llx end: 0x%llx, size=%ld, stride=%ld, delay=%ld, batch=%ld\n",
		(uint64_t)region_start, (uint64_t)region_end, access_size, stride_size, delay,
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
