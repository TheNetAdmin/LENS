#include "tasks.h"

static int get_wear_leveling_side_channel_role(struct latencyfs_worker_ctx *ctx)
{
	int role_type;
	switch (ctx->job_id) {
	case 0:
		role_type = 0;
		break;
	case 1:
		role_type = 1;
		break;
	default:
		role_type = -1;
		break;
	}

	LAT_ASSERT_EXIT(role_type != -1);

	return role_type;
}

#define WEAR_LEVELING_BLOCK_SIZE (4096)

void overwrite_warmup_thread(struct latencyfs_worker_ctx *ctx)
{
	uint8_t *uc_addr  = get_uc_addr(ctx, overlapped);
	uint8_t *beg_addr = uc_addr;
	uint8_t *end_addr;
	/*
	 * TODO: sync this addr calculation with side_channel.c by
	 * defining a getter function.
	 */
	if (1 == get_wear_leveling_side_channel_role(ctx)) {
		beg_addr += ctx->sbi->strided_size;
	}
	end_addr = beg_addr + ctx->sbi->pc_region_size;
	overwrite_warmup_range_by_block(
	        beg_addr,
		end_addr,
		WEAR_LEVELING_BLOCK_SIZE,
		ctx->sbi->threshold_cycle,
		ctx->sbi->delay
	);
}

int wear_leveling_side_channel_job(void *arg)
{
	char *this_task_name		 = "TASK_WEAR_LEVELING_SIDE_CHANNEL";
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

	char	 role[10];
	int	 role_type;
	long	 pages, diff;
	int	 i;
	unsigned ti, tj;
	int	 ret;

	if (!validate_address(ctx->sbi, uc_addr)) {
		kr_info("Reset uc_addr to [%px]\n", ctx->addr);
		uc_addr = ctx->addr;
		buf = uc_addr;
	}

	if (!validate_address(ctx->sbi, (void *)buf)) {
		LAT_WARN(false);
		goto wear_leveling_side_channel_job_end;
	}

	kr_info("Send data buffer: 0x%px\n", send_data_buffer);
	if (ctx->job_id == 0) {
		kr_info("Send data:\n");
		for (i = 0; i < total_data_bits / 64; i ++) {
			kr_info("  [%04d]: 0x%016llx\n", i, send_data_buffer[i]);
		}
	}

	/*
	 * Check if the data region is valid, i.e. if the first byte is 0xcc
	 * See gen_cover_data_pattern.py
	 */
	curr_data = 0xff & send_data_buffer[0];
	if(0xcc != curr_data) {
		kr_info("ERROR: The first byte of send data must be 0xcc, but got 0x%llx", curr_data);
		goto wear_leveling_side_channel_job_end;
	}

	/* Determine current threads role, sender/receiver */
	role_type = get_wear_leveling_side_channel_role(ctx);
	switch (role_type)
	{
	case 0:
		strscpy(role, "victim", 7);
		break;
	case 1:
		strscpy(role, "attacker", 9);
		break;
	default:
		LAT_ASSERT_EXIT(false);
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

	kr_info("side_id=%d, role_type=%10s, buf=%px\n",
		ctx->job_id, role, buf);

	ci.ctx             = ctx;
	ci.role_type       = role_type;
	ci.total_data_bits = total_data_bits;
	ci.send_data       = send_data_buffer;
	ci.buf             = buf;
	ci.op              = op;
	ci.sub_op_ready    = &ctx->sub_op_ready;
	ci.sub_op_complete = &ctx->sub_op_complete;
	ci.region_size     = ctx->sbi->pc_region_size;
	ci.strided_size    = strided_size;
	ci.block_size      = ctx->sbi->pc_block_size;
	ci.count           = ctx->sbi->count;
	ci.repeat          = repeat;
	ci.delay           = ctx->sbi->delay;
	ci.delay_per_byte  = ctx->sbi->delay_per_byte;
	ci.threshold_cycle = ctx->sbi->threshold_cycle;
	ci.threshold_iter  = ctx->sbi->threshold_iter;
	ci.attacker_iter   = ctx->sbi->threshold_iter;
	ci.victim_iter     = ctx->sbi->repeat;
	ci.warm_up         = ctx->sbi->warm_up;

	/* Not used, but set for safety */
	ci.region_skip  = ctx->sbi->pc_region_size;
	ci.region_align = ctx->sbi->pc_region_size;

	if (!validate_address(ctx->sbi, ci.buf)) {
		LAT_WARN(false);
		goto wear_leveling_side_channel_job_end;
	}

	switch (op) {
	case 0:
		side_overwrite_delay_per_size(&ci);
		break;
	default:
		kr_info("ERROR: unknown op: %llu\n", op);
		break;
	}

wear_leveling_side_channel_job_end:
	kr_info("%s_END:\n", this_task_name);
	/* NOTE: Important, set this complete so parent waiting can finish */
	complete(&ctx->complete);
	wait_for_stop();
	return 0;
}
