#include "tasks.h"

int wear_leveling_covert_channel_job(void *arg)
{
	char *this_task_name		 = "TASK_WEAR_LEVELING_COVERT_CHANNEL";
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	uint64_t strided_size		 = ctx->sbi->strided_size;
	uint8_t *uc_addr		 = get_uc_addr(ctx, overlapped);
	uint8_t *buf			 = uc_addr;
	uint8_t *buf_end		 = buf + GLOBAL_WORKSET;
	uint64_t repeat			 = ctx->sbi->repeat;
	uint64_t op			 = ctx->sbi->op;
	uint64_t *send_data_buffer	 = ctx->sbi->rep->virt_addr;
	uint64_t curr_data               = 0;
	uint64_t total_data_bits	 = ctx->sbi->access_size;
	int hash			 = 0;

	channel_info_t ci;

	char	 covert_strategy[30];
	char	 role[10];
	int	 role_type;
	long	 pages, diff;
	int	 i;
	unsigned ti, tj;
	int	 ret;

	if (!validate_address(ctx->sbi, uc_addr)) {
		kr_info("Reset uc_addr to [0x%px]\n", ctx->addr);
		uc_addr = ctx->addr;
		buf = uc_addr;
	}

	if (!validate_address(ctx->sbi, (void *)buf)) {
		LAT_WARN(false);
		goto wear_leveling_covert_channel_job_end;
	}

	kr_info("Send data buffer: 0x%px\n", send_data_buffer);
	if (ctx->job_id == 0) {
		kr_info("Send data:\n");
		for (i = 0; i < total_data_bits / 64; i ++) {
			kr_info("  [%04d]: 0x%016llx\n", i, send_data_buffer[i]);
		}
	}

	/* Check if the data region is valid, i.e. if the first byte is 0xcc
	 * See gen_cover_data_pattern.py
	 */
	curr_data = 0xff & send_data_buffer[0];
	if(0xcc != curr_data) {
		kr_info("ERROR: The first byte of victim data must be 0xcc, but got 0x%llx", curr_data);
		goto wear_leveling_covert_channel_job_end;
	}

	/* Determine current threads role, sender/receiver */
	switch (ctx->job_id) {
	case 0:
		strscpy(role, "sender", 7);
		role_type = 0;
		break;
	case 1:
		strscpy(role, "receiver", 9);
		role_type = 1;
		break;
	default:
		strscpy(role, "unknown", 8);
		role_type = -1;
		break;
	}

	/* WARNING: Potential memory leakage, the `strided_size` should not be used here */
	// /* fill pages */
	// pages = roundup(strided_size, PAGE_SIZE) >> PAGE_SHIFT;
	// for (i = 0; i < pages; i++) {
	// 	buf  = uc_addr + (i << PAGE_SHIFT);
	// 	hash = hash ^ *buf;
	// }
	// buf = uc_addr;

	kr_info("Working set begin: 0x%llx end: 0x%llx, phy-begin: %llx, access_size=%llu, "
		"strided_size=%llu, count=%llu, repeat=%llu\n",
		(uint64_t)buf, (uint64_t)buf_end, ctx->sbi->phys_addr, ctx->sbi->access_size,
		strided_size, ctx->sbi->count, repeat);

	kr_info("covert_id=%d, role_type=%10s, buf=0x%px\n",
		ctx->job_id, role, buf);

	ci.ctx		      = ctx;
	ci.role_type	      = role_type;
	ci.total_data_bits    = total_data_bits;
	ci.send_data	      = send_data_buffer;
	ci.buf		      = buf;
	ci.op		      = op;
	ci.sub_op_ready	      = &ctx->sub_op_ready;
	ci.sub_op_complete    = &ctx->sub_op_complete;
	ci.region_size	      = ctx->sbi->pc_region_size;
	ci.strided_size	      = strided_size;
	ci.block_size	      = 4096;
	ci.count	      = ctx->sbi->count;
	ci.repeat	      = repeat;
	ci.delay	      = ctx->sbi->delay;
	ci.delay_per_byte     = ctx->sbi->delay_per_byte;
	ci.warm_up            = ctx->sbi->warm_up;

	/* Not used, but set for safety */
	ci.region_skip  = ctx->sbi->pc_region_size;
	ci.region_align = ctx->sbi->pc_region_size;

	if (!validate_address(ctx->sbi, ci.buf)) {
		LAT_WARN(false);
		goto wear_leveling_covert_channel_job_end;
	}

	switch (op) {
	case 0:
		covert_overwrite_delay_per_size(&ci);
		break;
	default:
		kr_info("ERROR: unknown op: %llu\n", op);
		break;
	}

wear_leveling_covert_channel_job_end:
	kr_info("%s_END:\n", this_task_name);
	/* NOTE: Important, set this complete so upper waiting can finish */
	complete(&ctx->complete);
	wait_for_stop();
	return 0;
}
