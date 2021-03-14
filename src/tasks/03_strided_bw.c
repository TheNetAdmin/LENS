#include "tasks.h"

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

	kr_info("Working set begin: 0x%llx end: 0x%llx, size=%ld, stride=%ld, delay=%ld, batch=%ld\n",
		(uint64_t)buf, (uint64_t)region_end, access_size, stride_size, delay, count);

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
