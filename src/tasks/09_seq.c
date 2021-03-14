#include "tasks.h"

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
