#include <linux/kernel.h>
#include "overwrite.h"
#include "common.h"
#include "lib/pqueue-int.h"
#include "latency.h"
#include "../utils.h"

static void overwrite_warmup_delay(uint64_t delay)
{
	if (delay == 0)
		return;

	asm volatile(
		"xor %%r10, %%r10 \n"
		"LOOP_OVERWRITE_WARMUP_DELAY_EACH_DELAY: \n"
		"inc %%r10 \n"
		"nop \n"
		"cmp %[delay], %%r10 \n"
		"jl LOOP_OVERWRITE_WARMUP_DELAY_EACH_DELAY \n"
		"nop \n"
		:
		: [delay] "r"(delay)
		: "r10"
	);
}

static overwrite_warmup_result_t overwrite_warmup_single(
        char *start_addr,
        char *end_addr,
        overwrite_warmup_type_t warmup_type,
        uint64_t block_size,
        uint64_t threshold_cycle,
        uint64_t delay,
        uint64_t max_iter)
{
	uint64_t cycle_beg, cycle_end;
	struct timespec time_beg, time_end;

	char *curr_addr               = start_addr;
	overwrite_warmup_result_t res = {
		.cycle       = 0,
		.iter        = 0,
		.total_cycle = 0,
		.flush_type  = OVERWRITE_NTSTORE,
		.warmup_type = warmup_type,
	};

	getrawmonotonic(&time_beg);
	cycle_beg = rdtscp_lfence();

	while (res.cycle < threshold_cycle) {
		res.iter++;
		/* TODO: verify this change */
		switch (block_size) {
		case 64:
			res.cycle = repeat_64byte_ntstore(curr_addr);
			break;
		case 256:
			res.cycle      = repeat_256byte_clwb(curr_addr);
			res.flush_type = OVERWRITE_CLWB;
			break;
		default:
			LAT_ASSERT_EXIT(false);
		}

		if (max_iter == res.iter) {
			printk("Max iter reached: %llu\n", max_iter);
			break;
		}

		if (warmup_type == OVERWRITE_BY_RANGE) {
			curr_addr += block_size;
			if (curr_addr >= end_addr)
				curr_addr = start_addr;
		}

		overwrite_warmup_delay(delay);
	}

	cycle_end = rdtscp_lfence();
	getrawmonotonic(&time_end);

	res.total_cycle = cycle_end - cycle_beg;
	res.total_ns    = TIMEDIFF(time_beg, time_end);

	return res;
}

#define PRINT_WARMUP_RESULT()                                                  \
	do {                                                                   \
		printk(KERN_INFO                                               \
		       "overwrite warmup: "                                    \
		       "block size [%llu],\t"                                  \
		       "addr [0x%016llx],\t"                                   \
		       "iter [%llu],\t"                                        \
		       "max iter [%llu],\t"                                    \
		       "achieved cycle [%llu],\t"                              \
		       "threshold cycle [%llu],\t"                             \
		       "total cycle [%llu],\t"                                 \
		       "total ns [%llu],\t"                                    \
		       "flush type [%u],\t"                                    \
		       "warmup type [%u],\t"                                   \
		       "\n",                                                   \
		       block_size,                                             \
		       (uint64_t)curr_addr,                                    \
		       res.iter,                                               \
		       max_iter,                                               \
		       res.cycle,                                              \
		       threshold_cycle,                                        \
		       res.total_cycle,                                        \
		       res.total_ns,                                           \
		       res.flush_type,                                         \
		       res.warmup_type                                         \
		);                                                             \
	} while (0)

void overwrite_warmup_range_by_block(char *start_addr, char *end_addr, uint64_t skip, uint64_t threshold_cycle, uint64_t delay)
{
	const uint64_t max_iter = 1ULL << 18;
	overwrite_warmup_result_t res;
	char	     *curr_addr;
	uint64_t block_size;

	if ((end_addr - start_addr) % 256 == 0)
		block_size = 256;
	else
		block_size = 64;

	for (curr_addr = start_addr; curr_addr < end_addr; curr_addr += skip) {
		res.cycle = 0;
		res.iter  = 0;
		res = overwrite_warmup_single(curr_addr, curr_addr+block_size, OVERWRITE_BY_BLOCK, block_size, threshold_cycle, delay, max_iter);
		PRINT_WARMUP_RESULT();
	}
}

void overwrite_warmup_range_by_range(char *start_addr, char *end_addr, uint64_t skip, uint64_t threshold_cycle, uint64_t delay)
{
	const uint64_t max_iter = 1ULL << 16;
	overwrite_warmup_result_t res;
	char	     *curr_addr = start_addr;
	uint64_t block_size;

	if ((end_addr - start_addr) % 256 == 0)
		block_size = 256;
	else
		block_size = 64;

	res = overwrite_warmup_single(start_addr, end_addr, OVERWRITE_BY_RANGE, block_size, threshold_cycle, delay, max_iter);
	PRINT_WARMUP_RESULT();
}

void overwrite_vanilla(char *start_addr, char *end_addr, uint64_t skip, uint64_t count, long *rep_buf)
{
	char *curr_addr = start_addr;
	uint64_t j	= 0;
	uint64_t region_size = end_addr - start_addr;
	for (j = 0; j < count; j++) {
		if (region_size % 256 == 0) {
			rep_buf[j] = repeat_256byte_clwb(curr_addr);
		} else {
			rep_buf[j] = repeat_64byte_ntstore(curr_addr);
		}
		curr_addr += skip;
		if (curr_addr >= end_addr) {
			curr_addr = start_addr;
		}
	}
}

void overwrite_delay_each(char *start_addr, char *end_addr, uint64_t skip,
			  uint64_t count, long *rep_buf, long delay)
{
	char *curr_addr = start_addr;
	uint64_t j	= 0;
	for (j = 0; j < count; j++) {
		rep_buf[j] = repeat_256byte_ntstore(curr_addr);
		curr_addr += skip;
		if (curr_addr >= end_addr)
			curr_addr = start_addr;
		asm volatile("xor %%r10, %%r10 \n"
			     "LOOP_OVERWRITE_DELAY_EACH_DELAY: \n"
			     "inc %%r10 \n"
			     "nop \n"
			     "cmp %[delay], %%r10 \n"
			     "jl LOOP_OVERWRITE_DELAY_EACH_DELAY \n"
			     "nop \n"
			     :
			     : [delay] "r"(delay)
			     : "r10");
	}
}

void overwrite_two_points(char *start_addr, char *end_addr, uint64_t count,
			  long *rep_buf, uint64_t distance)
{
	char *curr_addr = start_addr;
	uint64_t j	= 0;
	for (j = 0; j < count; j += 2) {
		rep_buf[j]     = repeat_256byte_ntstore(curr_addr);
		rep_buf[j + 1] = repeat_256byte_ntstore(curr_addr + distance);
	}
}

void overwrite_delay_half(char *start_addr, char *end_addr, uint64_t skip,
			  uint64_t count, long *rep_buf, long delay)
{
	char *curr_addr	    = start_addr;
	uint64_t j	    = 0;
	uint64_t first_half = count / 2;
	for (j = 0; j < first_half; j++) {
		rep_buf[j] = repeat_256byte_ntstore(curr_addr);
		curr_addr += skip;
		if (curr_addr >= end_addr)
			curr_addr = start_addr;
	}
	asm volatile("xor %%r10, %%r10 \n"
		     "LOOP_OVERWRITE_DELAY_HALF_DELAY: \n"
		     "inc %%r10 \n"
		     "nop \n"
		     "cmp %[delay], %%r10 \n"
		     "jl LOOP_OVERWRITE_DELAY_HALF_DELAY \n"
		     "nop \n"
		     :
		     : [delay] "r"(delay)
		     : "r10");

	for (j = first_half; j < count; j++) {
		rep_buf[j] = repeat_256byte_ntstore(curr_addr);
		curr_addr += skip;
		if (curr_addr >= end_addr)
			curr_addr = start_addr;
	}
}

void overwrite_delay_per_size(char *start_addr, char *end_addr, uint64_t skip,
			      uint64_t count, long *rep_buf, long delay,
			      uint64_t delay_per_byte)
{
	char *curr_addr	     = start_addr;
	uint64_t j	     = 0;
	uint64_t delay_count = delay_per_byte / 256;
	latency_timing_pair timing_pair;
	for (j = 0; j < count; j++) {
		timing_pair	   = repeat_256byte_ntstore_pair(curr_addr);
		rep_buf[2 * j]	   = timing_pair.beg;
		rep_buf[2 * j + 1] = timing_pair.end;
		curr_addr += skip;
		if (curr_addr >= end_addr)
			curr_addr = start_addr;
		if (delay_count > 0 && (0 == j % delay_count)) {
			asm volatile("xor %%r10, %%r10 \n"
				     "LOOP_OVERWRITE_DELAY_PER_SIZE_DELAY: \n"
				     "inc %%r10 \n"
				     "nop \n"
				     "cmp %[delay], %%r10 \n"
				     "jl LOOP_OVERWRITE_DELAY_PER_SIZE_DELAY \n"
				     "nop \n"
				     :
				     : [delay] "r"(delay)
				     : "r10");
		}
	}
}

void overwrite_percentile_delay_per_size(
	char *			   start_addr,
	char *			   end_addr,
	uint64_t		   skip,
	uint64_t		   repeat,
	overwrite_latency_t *	   latencies,
	long			   delay,
	uint64_t		   delay_per_byte)
{
	char *curr_addr	     = start_addr;
	uint64_t j	     = 0;
	uint64_t i	     = 0;
	uint64_t delay_count = delay_per_byte / 256;
	latency_timing_pair timing_pair;
	uint64_t timing_diff;
	for (j = 0; j < repeat; j++) {
		// printk(KERN_WARNING "curr_addr=0x%016lx, j=%lu\n", (uint64_t)curr_addr, j);
		timing_pair = repeat_256byte_clwb_pair(curr_addr);
		timing_diff = timing_pair.end - timing_pair.beg;

		count_overwrite_latency(latencies, timing_diff);

		/* 
		 * WARNING: the following min_heap code is causing kernel panics.
		 */
		// asm volatile("CPUID"::: "eax","ebx","ecx","edx", "memory");
		// if (lat_heap->nr < lat_heap->size) {
		// 	min_heap_push(lat_heap, &timing_diff, lat_heap_funcs);
		// } else {
		// 	min_heap_pop_push(lat_heap, &timing_diff, lat_heap_funcs);
		// }
		// asm volatile("mfence");
		// asm volatile("CPUID"::: "eax","ebx","ecx","edx", "memory");

		/*
		 * WARNING: The following curr_addr code is causing kernel
		 * panics, I guess. Because once commented, there is no kernel
		 * panicing anymore.
		 *
		 * UPDATE: Not this part causing problem, I still randomly get
		 * kernel panic even when the following lines are commented
		 */
		curr_addr += skip;
		if (curr_addr >= end_addr)
			curr_addr = start_addr;
		if ((delay_count > 0) && (0 == j % delay_count)) {
			for (i = 0; i < delay; i++) {
				asm volatile("nop");
			}
		}
	}
}

void overwrite_delay_per_size_cpuid(char *start_addr, char *end_addr,
				    uint64_t skip, uint64_t count,
				    long *rep_buf, long delay,
				    uint64_t delay_per_byte)
{
	char *curr_addr	     = start_addr;
	uint64_t j	     = 0;
	uint64_t delay_count = delay_per_byte / 256;
	latency_timing_pair timing_pair;
	for (j = 0; j < count; j++) {
		timing_pair    = repeat_256byte_ntstore_pair_cpuid(curr_addr);
		rep_buf[2 * j] = timing_pair.beg;
		rep_buf[2 * j + 1] = timing_pair.end;
		curr_addr += skip;
		if (curr_addr >= end_addr)
			curr_addr = start_addr;
		if (delay_count > 0 && (0 == j % delay_count)) {
			asm volatile(
				"xor %%r10, %%r10 \n"
				"LOOP_OVERWRITE_DELAY_PER_SIZE_CPUID_DELAY: \n"
				"inc %%r10 \n"
				"nop \n"
				"cmp %[delay], %%r10 \n"
				"jl LOOP_OVERWRITE_DELAY_PER_SIZE_CPUID_DELAY \n"
				"nop \n"
				:
				: [delay] "r"(delay)
				: "r10");
		}
	}
}
