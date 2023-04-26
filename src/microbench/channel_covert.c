#include "channel.h"
#include "chasing.h"
#include "overwrite.h"
#include "../utils.h"
#include "../lib/pqueue-int.h"

void covert_ptr_chasing_load_and_store(channel_info_t *ci)
{
	PC_VARS;
	size_t i;
	long   diff;

	size_t			     curr_data;
	uint8_t *		     covert_channel;
	static unsigned long	     flags;
	struct latencyfs_worker_ctx *ctx    = ci->ctx;
	uint64_t *		     timing = ci->timing;

	size_t	 ti;
	uint64_t repeat = ci->repeat;

	/* NOTE: assume L2 16 way
	 *       - sender fill one set with 16 buffer blocks
	 *       - receiver fill same set with 16 other blocks
	 */
	kr_info("covert_strategy=ptr_chasing_load_and_store");

	switch (ci->role_type) {
	case 0: // sender
		// send 64 bits
		kr_info("send_data_buffer=0x%px, total_data_bits=%lu\n",
			ci->send_data, ci->total_data_bits);
		for (i = 0; i < ci->total_data_bits; i++) {
			CURR_DATA(curr_data, i);
			kr_info("Waiting to send bit_id=%ld, bit_data=%lu\n", i, curr_data);
			wait_for_completion(ci->sub_op_ready);
			init_completion(ci->sub_op_ready);
			TIMING_BUF_INIT(ci->timing);
			if (curr_data == 0x0) {
				covert_channel = ci->buf;
			} else {
				covert_channel = ci->buf + 4096;
			}
			kr_info("Send bit_data=%1lu, bit_id=%ld, channel=0x%px\n",
				curr_data, i, covert_channel);

			BENCHMARK_BEGIN(flags);

			PC_BEFORE_WRITE
			chasing_func_list[ci->chasing_func_index].st_func(
				covert_channel, ci->region_size,
				ci->strided_size, ci->region_skip, ci->count,
				ci->repeat, ci->cindex, ci->timing);
			asm volatile("mfence \n" :::);
			PC_BEFORE_READ
			chasing_func_list[ci->chasing_func_index].ld_func(
				covert_channel, ci->region_size,
				ci->strided_size, ci->region_skip, ci->count,
				ci->repeat, ci->cindex,
				ci->timing + repeat * 2);
			asm volatile("mfence \n" :::);
			PC_AFTER_READ

			COVERT_PC_STRIDED_PRINT_MEASUREMENT(
				chasing_func_list[ci->chasing_func_index]);
			kr_info("[%s] ",
				chasing_func_list[ci->chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING("lat_st", (timing));
			kr_info("[%s] ",
				chasing_func_list[ci->chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING("lat_ld",
						    (ci->timing + repeat * 2));
			kr_info("\n");
			BENCHMARK_END(flags);
			complete(ci->sub_op_complete);
		}
		break;
	case 1: // receiver
		/* NOTE: for now, receiver only monitors the first set, because
		 *       implementation is simpler, later may need to monitor
		 *       multiple sets
		 */
		for (i = 0; i < ci->total_data_bits; i++) {
			kr_info("Waiting to receive bit_id=%ld\n", i);
			wait_for_completion(ci->sub_op_ready);
			init_completion(ci->sub_op_ready);
			TIMING_BUF_INIT(ci->timing);

			covert_channel = ci->buf;

			kr_info("Recv bit_id=%ld, channel=0x%px\n", i,
				covert_channel);

			BENCHMARK_BEGIN(flags);

			PC_BEFORE_WRITE
			chasing_func_list[ci->chasing_func_index].st_func(
				covert_channel, ci->region_size,
				ci->strided_size, ci->region_skip, ci->count,
				ci->repeat, ci->cindex, ci->timing);
			asm volatile("mfence \n" :::);
			PC_BEFORE_READ
			chasing_func_list[ci->chasing_func_index].ld_func(
				covert_channel, ci->region_size,
				ci->strided_size, ci->region_skip, ci->count,
				ci->repeat, ci->cindex,
				ci->timing + repeat * 2);
			asm volatile("mfence \n" :::);
			PC_AFTER_READ

			COVERT_PC_STRIDED_PRINT_MEASUREMENT(
				chasing_func_list[ci->chasing_func_index]);
			kr_info("[%s] ",
				chasing_func_list[ci->chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING("lat_st", (timing));
			kr_info("[%s] ",
				chasing_func_list[ci->chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING("lat_ld",
						    (ci->timing + repeat * 2));
			kr_info("\n");
			BENCHMARK_END(flags);
			complete(ci->sub_op_complete);
		}
		break;
	default:
		pr_err("UNKNOWN role_type = %d\n", ci->role_type);
		break;
	}

	return;
}

void covert_ptr_chasing_load_only(channel_info_t *ci)
{
	PC_VARS;
	size_t i;
	long   diff;

	size_t			     curr_data;
	uint8_t *		     covert_channel;
	uint8_t *		     bit_0_channel = ci->buf;
	uint8_t *		     bit_1_channel = ci->buf + 4096;
	static unsigned long	     flags;
	struct latencyfs_worker_ctx *ctx    = ci->ctx;
	uint64_t *		     timing = ci->timing;

	size_t	 ti;
	uint64_t repeat = ci->repeat;

	/* NOTE: assume L2 16 way
	 *       - sender fill one set with 16 buffer blocks
	 *       - receiver fill same set with 16 other blocks
	 */
	kr_info("covert_strategy=ptr_chasing_load_only");

	kr_info("Init bit 0 channel: 0x%px", bit_0_channel);
	chasing_func_list[ci->chasing_func_index].st_func(
		bit_0_channel, ci->region_size, ci->strided_size,
		ci->region_skip, ci->count, ci->repeat, ci->cindex, ci->timing);
	asm volatile("mfence \n" :::);

	kr_info("Init bit 1 channel: 0x%px", bit_1_channel);
	chasing_func_list[ci->chasing_func_index].st_func(
		bit_1_channel, ci->region_size, ci->strided_size,
		ci->region_skip, ci->count, ci->repeat, ci->cindex, ci->timing);
	asm volatile("mfence \n" :::);

	switch (ci->role_type) {
	case 0: // sender
		// send 64 bits
		kr_info("send_data_buffer=0x%px, total_data_bits=%lu\n",
			ci->send_data, ci->total_data_bits);

		for (i = 0; i < ci->total_data_bits; i++) {
			CURR_DATA(curr_data, i);
			kr_info("Waiting to send bit_id=%ld, bit_data=%lu\n", i, curr_data);
			wait_for_completion(ci->sub_op_ready);
			init_completion(ci->sub_op_ready);
			TIMING_BUF_INIT(timing);
			if (curr_data == 0x0) {
				covert_channel = bit_0_channel;
			} else {
				covert_channel = bit_1_channel;
			}
			kr_info("Send bit_data=%1lu, bit_id=%ld, channel=0x%px\n",
				curr_data, i, covert_channel);

			BENCHMARK_BEGIN(flags);

			PC_BEFORE_WRITE
			PC_BEFORE_READ
			chasing_func_list[ci->chasing_func_index].ld_func(
				covert_channel, ci->region_size,
				ci->strided_size, ci->region_skip, ci->count,
				ci->repeat, ci->cindex,
				ci->timing + repeat * 2);
			asm volatile("mfence \n" :::);
			PC_AFTER_READ

			COVERT_PC_STRIDED_PRINT_MEASUREMENT(
				chasing_func_list[ci->chasing_func_index]);
			kr_info("[%s] ",
				chasing_func_list[ci->chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING("lat_st", (timing));
			kr_info("[%s] ",
				chasing_func_list[ci->chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING("lat_ld",
						    (ci->timing + repeat * 2));
			kr_info("\n");
			BENCHMARK_END(flags);
			complete(ci->sub_op_complete);
		}
		break;
	case 1: // receiver
		/* NOTE: for now, receiver only monitors the first set, because
		 *       implementation is simpler, later may need to monitor
		 *       multiple sets
		 */
		for (i = 0; i < ci->total_data_bits; i++) {
			kr_info("Waiting to receive bit_id=%ld\n", i);
			wait_for_completion(ci->sub_op_ready);
			init_completion(ci->sub_op_ready);
			TIMING_BUF_INIT(timing);

			covert_channel = bit_0_channel;

			kr_info("Recv bit_id=%ld, channel=0x%px\n", i,
				covert_channel);

			BENCHMARK_BEGIN(flags);

			PC_BEFORE_WRITE
			PC_BEFORE_READ
			chasing_func_list[ci->chasing_func_index].ld_func(
				covert_channel, ci->region_size,
				ci->strided_size, ci->region_skip, ci->count,
				ci->repeat, ci->cindex,
				ci->timing + repeat * 2);
			asm volatile("mfence \n" :::);
			PC_AFTER_READ

			COVERT_PC_STRIDED_PRINT_MEASUREMENT(
				chasing_func_list[ci->chasing_func_index]);
			kr_info("[%s] ",
				chasing_func_list[ci->chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING("lat_st", (timing));
			kr_info("[%s] ",
				chasing_func_list[ci->chasing_func_index].name);
			CHASING_PRINT_RECORD_TIMING("lat_ld",
						    (ci->timing + repeat * 2));
			kr_info("\n");
			BENCHMARK_END(flags);
			complete(ci->sub_op_complete);
		}
		break;
	default:
		pr_err("UNKNOWN role_type = %d\n", ci->role_type);
		break;
	}

	return;
}

#define COVERT_ACCESS(channel)                                                 \
	PC_BEFORE_WRITE                                                        \
	chasing_func_list[ci->chasing_func_index].st_func(                     \
		channel, ci->region_size, ci->strided_size, ci->region_skip,   \
		ci->count, ci->repeat, ci->cindex, timing[j]);                 \
	asm volatile("mfence \n" :::);                                         \
	PC_BEFORE_READ                                                         \
	chasing_func_list[ci->chasing_func_index].ld_func(                     \
		channel, ci->region_size, ci->strided_size, ci->region_skip,   \
		ci->count, ci->repeat, ci->cindex, timing[j] + repeat * 2);    \
	asm volatile("mfence \n" :::);                                         \
	PC_AFTER_READ

#define PRINT_COVERT_RESULT_BOTH_CHANNEL(sub_iter)                             \
	kr_info("[%s] ", chasing_func_list[ci->chasing_func_index].name);      \
	CHASING_PRINT_RECORD_TIMING(("lat_st_it" #sub_iter),                   \
				    (timing[sub_iter]));                       \
	kr_info("[%s] ", chasing_func_list[ci->chasing_func_index].name);      \
	CHASING_PRINT_RECORD_TIMING(("lat_ld_it" #sub_iter),                   \
				    (timing[sub_iter] + repeat * 2));          \
	kr_info("\n");

void covert_ptr_chasing_both_channel_load_and_store(channel_info_t *ci)
{
	PC_VARS;
	size_t i, j;
	long   diff;

	static unsigned long	     flags;
	size_t			     curr_data;
	struct latencyfs_worker_ctx *ctx = ci->ctx;

	uint8_t *covert_channel;
	uint8_t *bit_0_channel = ci->buf;
	uint8_t *bit_1_channel = ci->buf + 4096;

	size_t	 ti;
	uint64_t repeat = ci->repeat;

	/* repeat * 4 should be enough, * 8 to make sure no overlap */
	uint64_t *timing[2];
	timing[0] = ci->timing;
	timing[1] = ci->timing + ci->repeat * 8;

	/* NOTE: assume L2 16 way
	 *       - sender fill one set with 16 buffer blocks
	 *       - receiver fill same set with 16 other blocks
	 */
	kr_info("covert_strategy=ptr_chasing_both_channel_load_and_store");

	switch (ci->role_type) {
	case 0: // sender
		// send 64 bits
		kr_info("send_data_buffer=0x%px, total_data_bits=%lu\n",
			ci->send_data, ci->total_data_bits);
		for (i = 0; i < ci->total_data_bits; i++) {
			CURR_DATA(curr_data, i);
			kr_info("Waiting to send bit_id=%ld, bit_data=%lu\n", i, curr_data);
			wait_for_completion(ci->sub_op_ready);
			init_completion(ci->sub_op_ready);
			TIMING_BUF_INIT(timing[0]);
			TIMING_BUF_INIT(timing[1]);
			if (curr_data == 0x0) {
				covert_channel = bit_0_channel;
			} else {
				covert_channel = bit_1_channel;
			}
			kr_info("Send bit_data=%1lu, bit_id=%ld, channel=0x%px\n",
				curr_data, i, covert_channel);

			BENCHMARK_BEGIN(flags);

			COVERT_ACCESS(covert_channel);
			COVERT_ACCESS(covert_channel);

			COVERT_PC_STRIDED_PRINT_MEASUREMENT(
				chasing_func_list[ci->chasing_func_index]);

			PRINT_COVERT_RESULT_BOTH_CHANNEL(0);
			PRINT_COVERT_RESULT_BOTH_CHANNEL(1);

			BENCHMARK_END(flags);
			complete(ci->sub_op_complete);
		}
		break;
	case 1: // receiver
		/* NOTE: for now, receiver only monitors the first set, because
		 *       implementation is simpler, later may need to monitor
		 *       multiple sets
		 */
		for (i = 0; i < ci->total_data_bits; i++) {
			kr_info("Waiting to receive bit_id=%ld\n", i);
			wait_for_completion(ci->sub_op_ready);
			init_completion(ci->sub_op_ready);
			TIMING_BUF_INIT(timing[0]);
			TIMING_BUF_INIT(timing[1]);

			kr_info("Recv bit_id=%ld, channel=0x%px\n", i,
				covert_channel);

			BENCHMARK_BEGIN(flags);

			COVERT_ACCESS(bit_0_channel);
			COVERT_ACCESS(bit_1_channel);

			COVERT_PC_STRIDED_PRINT_MEASUREMENT(
				chasing_func_list[ci->chasing_func_index]);

			PRINT_COVERT_RESULT_BOTH_CHANNEL(0);
			PRINT_COVERT_RESULT_BOTH_CHANNEL(1);

			BENCHMARK_END(flags);
			complete(ci->sub_op_complete);
		}
		break;
	default:
		pr_err("UNKNOWN role_type = %d\n", ci->role_type);
		break;
	}

	return;
}


#define wear_leveling_sender_skip (64 * 1024)

void covert_overwrite_delay_per_size(channel_info_t *ci)
{
	struct latencyfs_worker_ctx *ctx = ci->ctx;

	uint8_t *covert_channel;
	uint8_t *bit_0_channel = ci->buf;
	uint8_t *bit_1_channel = ci->buf + wear_leveling_sender_skip;

	size_t		    i, j;
	latency_timing_pair timing_pair;
	size_t		    curr_data;
	unsigned long	    flags;

	overwrite_latency_t overwrite_latency;

	/* Message buffer */
	char msg_buf[LATENCY_MSG_BUF_SIZE] = "";

	PRINT_CHANNEL_INFO("covert_overwrite_delay_per_size", ci);

	switch (ci->role_type) {
	case sender: // sender
		kr_info("send_data_buffer=0x%px, total_data_bits=%lu\n",
			ci->send_data, ci->total_data_bits);

		ci->repeat = ci->repeat * 1;

		for (i = 0; i < ci->total_data_bits; i++) {
			init_overwrite_latency(&overwrite_latency);
			CURR_DATA(curr_data, i);
			kr_info("Waiting to send bit_id=%ld, bit_data=%lu\n", i, curr_data);
			if (1 == ci->ctx->sbi->sync_per_iter) {
				wait_for_completion(ci->sub_op_ready);
				init_completion(ci->sub_op_ready);
			}
			if (curr_data == 0x0) {
				covert_channel = bit_0_channel;
			} else {
				covert_channel = bit_1_channel;
			}

			if (!validate_address(ctx->sbi, (void *)covert_channel) ||
			    !validate_address(ctx->sbi, (void *)covert_channel + ci->region_size)) {
				LAT_WARN(false);
				goto covert_overwrite_delay_per_size_end;
			}

			if (i >= 16) {
				complete(ci->sub_op_complete);
				continue;
			}

			kr_info("Send bit_data=%1lu, bit_id=%ld, channel=0x%px",
				curr_data, i, covert_channel);

			BENCHMARK_BEGIN(flags);
			
			if (curr_data == 0x0) {
				overwrite_percentile_delay_per_size(
					covert_channel,
					covert_channel + ci->region_size,
					ci->block_size,
					ci->repeat,
					&overwrite_latency,
					0,
					ci->delay_per_byte
				);
			}

			BENCHMARK_END(flags);

			msg_buf[0] = '\0';
			snprintf_latency(msg_buf, &overwrite_latency, i);
			snprintf_latency_summary(msg_buf, &overwrite_latency, i);
			kr_info("%s", msg_buf);

			if (1 == ci->ctx->sbi->sync_per_iter) {
				complete(ci->sub_op_complete);
			}
		}
		break;
	case receiver: // receiver
		for (i = 0; i < ci->total_data_bits; i++) {
			init_overwrite_latency(&overwrite_latency);
			CURR_DATA(curr_data, i);
			if (1 == ci->ctx->sbi->sync_per_iter) {
				wait_for_completion(ci->sub_op_ready);
				init_completion(ci->sub_op_ready);
			}
			covert_channel = bit_0_channel + ci->strided_size;

			if (!validate_address(ctx->sbi, (void *)covert_channel) ||
			    !validate_address(ctx->sbi, (void *)covert_channel + ci->region_size)) {
				LAT_WARN(false);
				goto covert_overwrite_delay_per_size_end;
			}

			kr_info("Recv bit_id=%ld, channel=0x%px", i, covert_channel);
			
			if (i >= 16) {
				complete(ci->sub_op_complete);
				continue;
			}

			BENCHMARK_BEGIN(flags);
			
			overwrite_percentile_delay_per_size(
				covert_channel,
				covert_channel + ci->region_size,
				ci->block_size,
				ci->repeat,
				&overwrite_latency,
				ci->delay,
				ci->delay_per_byte
			);

			BENCHMARK_END(flags);

			msg_buf[0] = '\0';
			snprintf_latency(msg_buf, &overwrite_latency, i);
			snprintf_latency_summary(msg_buf, &overwrite_latency, i);
			kr_info("%s", msg_buf);

			if (1 == ci->ctx->sbi->sync_per_iter) {
				complete(ci->sub_op_complete);
			}
		}
		break;
	default:
		pr_err("UNKNOWN role_type = %d\n", ci->role_type);
		break;
	}

covert_overwrite_delay_per_size_end:
	return;
}
