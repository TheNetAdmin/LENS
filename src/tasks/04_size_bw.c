#include "tasks.h"

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

	if (sbi->align_mode == ALIGN_GLOBAL) {
		bitmask = (GLOBAL_WORKSET - 1) ^ (align_size - 1);
	} else if (sbi->align_mode == ALIGN_PERTHREAD) {
		bitmask = (PERTHREAD_WORKSET - 1) ^ (align_size - 1);
	} else {
		kr_info("Unsupported align mode %d\n", sbi->align_mode);
		return 0;
	}

	get_random_bytes(&seed, sizeof(unsigned long));
	kr_info("Working set begin: 0x%llx end: 0x%llx, access_size=%ld, align_size=%ld, batch=%ld, bitmask=%lx, seed=%lx\n",
		(uint64_t)buf, (uint64_t)region_end, access_size, align_size, count, bitmask, seed);

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
