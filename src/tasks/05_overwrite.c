#include "tasks.h"
#include "latency.h"

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
