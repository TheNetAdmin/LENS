#include "tasks.h"

int debugging_job(void *arg)
{
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	char *	 this_task_name		 = "TASK_DEBUGGING";
	uint8_t *uc_addr = get_uc_addr(ctx, overlapped);
	channel_info_t ci;

	if (!validate_address(ctx->sbi, uc_addr)) {
		kr_info("Reset uc_addr to [0x%016llx]\n", (uint64_t)ctx->addr);
		uc_addr = ctx->addr;
	}

	switch (ctx->sbi->op) {
	case 1:
		kr_info("Probing write policy.");
		ci.ctx	       = ctx;
		ci.buf	       = uc_addr;
		ci.region_size = ctx->sbi->pc_region_size;
		buffer_write_back_policy(&ci);
		break;

	case 0:
	default:
		kr_info("No debugging op selected, skipping.");
		break;
	}

debugging_job_end:
	kr_info("%s_END:\n", this_task_name);
	wait_for_stop();
	return 0;
}
