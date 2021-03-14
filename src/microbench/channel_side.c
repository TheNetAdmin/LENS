#include "channel.h"
#include "chasing.h"
#include "overwrite.h"
#include "../utils.h"
#include "latency.h"

typedef struct {
	int      bit;
	unsigned iters_over_threshold;
	uint64_t iters;
	uint64_t cycle;
} side_overwrite_result_t;

static void init_side_result(side_overwrite_result_t *res)
{
	res->bit                  = -1;
	res->iters_over_threshold = 0;
	res->iters                = 0;
	res->cycle                = 0;
}

static void _side_overwrite_delay_per_size(
        uint8_t  *start_addr,
	uint8_t  *end_addr,
	uint64_t block_size,
        uint64_t total_iter,
        uint64_t threshold_cycle,
        long     delay,
        uint64_t delay_per_byte,
	side_overwrite_result_t *res
)
{
	uint64_t cycle       = 0;
	uint64_t max_cycle   = 0;
	uint64_t i           = 0;
	uint64_t delay_count = delay_per_byte / 256;
	uint8_t  *curr_addr  = start_addr;

	for (i = 0; i < total_iter; i++) {
		if (block_size == 256)
			cycle = repeat_256byte_ntstore(curr_addr);
		else if (block_size == 64)
			cycle = repeat_64byte_ntstore(curr_addr);
		else
			LAT_ASSERT_EXIT(false);
		
		curr_addr += block_size;
		if (curr_addr >= end_addr)
			curr_addr = start_addr;

		if (cycle >= threshold_cycle) {
			res->iters = i;
			res->cycle = cycle;
			res->iters_over_threshold++;
		}
		// if (cycle > max_cycle) {
		// 	max_cycle = cycle;
		// }
		
		if ((delay_count > 0) && (0 == i % delay_count) && (delay > 0)) {
			for (i = 0; i < delay; i++) {
				asm volatile("nop");
			}
		}
	}

	if (res->iters_over_threshold > 0) {
		res->bit = 0;
	} else {
		res->bit = 1;
	}

	// pr_info("DEBUG: [_side_overwrite_delay_per_size] max_cycle=%llu", max_cycle);
}

void side_overwrite_delay_per_size(channel_info_t *ci)
{
	struct latencyfs_worker_ctx *ctx = ci->ctx;

	uint8_t *side_channel;
	uint8_t *bit_0_channel = ci->buf;

	size_t i, j;
	latency_timing_pair timing_pair;
	size_t curr_data;
	unsigned long flags;
	side_overwrite_result_t *side_result_curr;
	side_overwrite_result_t *side_results;

	side_results = (side_overwrite_result_t *)kmalloc(sizeof(side_overwrite_result_t) * ci->total_data_bits, GFP_KERNEL);

	if (ci->role_type == sender) {
		ci->delay = 0;
		ci->delay_per_byte = 256;
	}

	PRINT_CHANNEL_INFO("side_overwrite_delay_per_size", ci);

	LAT_ASSERT_EXIT(0 != (ci->block_size));
	LAT_ASSERT_EXIT(0 == (ci->region_size % ci->block_size));

	switch (ci->role_type) {
	case sender: // victim
		kr_info("send_data_buffer=0x%px, total_data_bits=%lu\n", ci->send_data, ci->total_data_bits);

		side_channel = bit_0_channel;
		for (i = 0; i < ci->total_data_bits; i++) {
			side_result_curr = &(side_results[i]);
			init_side_result(side_result_curr);

			CURR_DATA(curr_data, i);
			kr_info("Waiting to send bit_id=%ld, bit_data=%lu\n", i, curr_data);
			
			if (1 == ci->ctx->sbi->sync_per_iter) {
				wait_for_completion(ci->sub_op_ready);
				init_completion(ci->sub_op_ready);
			}
			
			if (!validate_address(ctx->sbi, (void *)side_channel) ||
			    !validate_address(ctx->sbi, (void *)side_channel + ci->region_size)) {
				LAT_WARN(false);
				goto side_overwrite_delay_per_size_end;
			}

			kr_info("Send bit_data=%1lu, bit_id=%ld, channel=0x%px", curr_data, i, side_channel);

			BENCHMARK_BEGIN(flags);

			if (curr_data == 0x0) {
				_side_overwrite_delay_per_size(
					side_channel,
					side_channel + ci->region_size,
					ci->block_size,
					ci->victim_iter,
					ci->threshold_cycle,
					ci->delay,
					ci->delay_per_byte,
					side_result_curr
				);
			}

			BENCHMARK_END(flags);

			if (1 == ci->ctx->sbi->sync_per_iter) {
				complete(ci->sub_op_complete);
			}
		}
		break;
	case receiver: // attacker
		side_channel = bit_0_channel + ci->strided_size;
		for (i = 0; i < ci->total_data_bits; i++) {
			side_result_curr = &(side_results[i]);
			init_side_result(side_result_curr);

			CURR_DATA(curr_data, i);
			if (1 == ci->ctx->sbi->sync_per_iter) {
				wait_for_completion(ci->sub_op_ready);
				init_completion(ci->sub_op_ready);
			}

			if (!validate_address(ctx->sbi, (void *)side_channel) ||
			    !validate_address(ctx->sbi, (void *)side_channel + ci->region_size)) {
				LAT_WARN(false);
				goto side_overwrite_delay_per_size_end;
			}

			kr_info("Recv bit_id=%ld, channel=0x%px", i, side_channel);

			BENCHMARK_BEGIN(flags);

			_side_overwrite_delay_per_size(
				side_channel,
				side_channel + ci->region_size,
				ci->block_size,
				ci->attacker_iter,
				ci->threshold_cycle,
				ci->delay,
				ci->delay_per_byte,
				side_result_curr
			);

			BENCHMARK_END(flags);

			if (1 == ci->ctx->sbi->sync_per_iter) {
				complete(ci->sub_op_complete);
			}
		}
		break;
	default:
		pr_err("UNKNOWN role_type = %d\n", ci->role_type);
		break;
	}

	/* TODO: print all results */
	for (i = 0; i < ci->total_data_bits; i++) {
		CURR_DATA(curr_data, i);
		side_result_curr = &(side_results[i]);
		kr_info(
			"bit_id=%ld, "
			"origin_data_bit=%1ld, "
			"detect_data_bit=%d, "
			"iters_over_threshold=%d, "
			"cycle_over_threshold=%llu, "
			"iters_to_over_threshold=%llu, "
			,
			i,
			curr_data,
			side_result_curr->bit,
			side_result_curr->iters_over_threshold,
			side_result_curr->cycle,
			side_result_curr->iters
		);
	}

side_overwrite_delay_per_size_end:
	return;
}
