#ifndef LENS_MICROBENCH_CHANNEL_H
#define LENS_MICROBENCH_CHANNEL_H

#include <linux/kernel.h>
#include <linux/completion.h>
#include "../lat.h"

typedef enum { sender, receiver } covert_role_t;

typedef struct {
	struct latencyfs_worker_ctx *ctx;
	covert_role_t		     role_type;
	size_t			     total_data_bits;
	uint64_t *		     send_data;
	uint8_t *		     buf;
	uint64_t		     op;
	struct completion *	     sub_op_ready;
	struct completion *	     sub_op_complete;

	/* General info */
	uint64_t  region_size;
	uint64_t  region_skip;
	uint64_t  block_size;
	uint64_t  repeat;
	uint64_t  count;

	/* Pointer chasing info */
	size_t	  chasing_func_index;
	uint64_t  strided_size;
	uint64_t  region_align;
	uint64_t *cindex;
	uint64_t *timing;

	/* Overwrite info */
	uint64_t delay;
	uint64_t delay_per_byte;

	/* Wear-leveling threshold info */
	uint64_t threshold_cycle;
	uint64_t threshold_iter;
	unsigned warm_up;

	/* Side channel info */
	uint64_t victim_iter;
	uint64_t attacker_iter;
} channel_info_t;

#ifndef kr_info
#define kr_info(string, args...)                                               \
	do {                                                                   \
		printk(KERN_INFO "{%d}" string, ctx->core_id, ##args);         \
	} while (0)
#endif

#define CURR_DATA(curr_data, curr_iter)                                        \
	curr_data = ((ci->send_data[curr_iter / 64] >> i) & 0x1);

void covert_ptr_chasing_load_and_store(channel_info_t *ci);
void covert_ptr_chasing_load_only(channel_info_t *ci);
void covert_ptr_chasing_both_channel_load_and_store(channel_info_t *ci);
void covert_overwrite_delay_per_size(channel_info_t *ci);

void side_overwrite_delay_per_size(channel_info_t *ci);

#endif /* LENS_MICROBENCH_CHANNEL_H */
