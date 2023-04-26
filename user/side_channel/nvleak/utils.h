#ifndef LENS_UTILS_H
#define LENS_UTILS_H

#include "print_info.h"

#define PC_VARS                                                                \
	uint64_t c_store_start;                                                \
	uint64_t c_load_start;                                                 \
	uint64_t c_load_end;

#define PC_BEFORE_WRITE                                                        \
	c_store_start = rdtscp(&cycle_aux);                                    \
	asm volatile("mfence");

#define PC_BEFORE_READ                                                         \
	c_load_start = rdtscp(&cycle_aux);                                     \
	asm volatile("mfence");

#define PC_AFTER_READ                                                          \
	c_load_end = rdtscp(&cycle_aux);                                       \
	asm volatile("mfence");

#define PC_PRINT_MEASUREMENT(meta)                                             \
	kr_info("buf_addr 0x%llx\n", (uint64_t)buf);                           \
	kr_info("[%s] region_size %ld, block_size %ld, count %ld, "            \
		"cycle %ld - %ld - %ld, "                                      \
		"fence_strategy %s, fence_freq %s.\n",                         \
		meta.name,                                                     \
		region_size,                                                   \
		block_size,                                                    \
		count,                                                         \
		c_store_start,                                                 \
		c_load_start,                                                  \
		c_load_end,                                                    \
		meta.fence_strategy,                                           \
		meta.fence_freq);

#define PC_STRIDED_PRINT_MEASUREMENT(meta)                                                  \
	kr_info("buf_addr 0x%llx\n", (uint64_t)buf);                                        \
	kr_info("[%s] region_size=%lu, block_size=%lu, region_skip=%lu, "                   \
		"stride_size=%lu, count=%lu, "                                              \
		"cycle=%ld:%ld:%ld, fence_strategy=%s, fence_freq=%s, "                     \
		"repeat=%lu, region_align=%lu, "                                            \
		"flush_after_load=%s, flush_after_store=%s, flush_l1=%s, record_timing=%s", \
		meta.name,                                                                  \
		region_size,                                                                \
		block_size,                                                                 \
		region_skip,                                                                \
		strided_size,                                                               \
		count,                                                                      \
		c_store_start,                                                              \
		c_load_start,                                                               \
		c_load_end,                                                                 \
		meta.fence_strategy,                                                        \
		meta.fence_freq,                                                            \
		repeat,                                                                     \
		region_align,                                                               \
		meta.flush_after_load,                                                      \
		meta.flush_after_store,                                                     \
		meta.flush_l1,                                                              \
		meta.record_timing);

#define COVERT_PC_STRIDED_PRINT_MEASUREMENT(meta)                                            \
	kr_info("buf_addr 0x%llx\n", (uint64_t)ci->buf);                                     \
	kr_info("[%s] region_size=%lu, block_size=%lu, region_skip=%lu, "                    \
		"stride_size=%lu, count=%lu, "                                               \
		"cycle=%ld:%ld:%ld, fence_strategy=%s, fence_freq=%s, "                      \
		"repeat=%lu, region_align=%lu, "                                             \
		"flush_after_load=%s, flush_after_store=%s, flush_l1=%s, record_timing=%s, " \
		"total_cycle=%lu, cycle_beg_since_all_beg=%lu",                              \
		meta.name,                                                                   \
		ci->region_size,                                                             \
		ci->block_size,                                                              \
		ci->region_skip,                                                             \
		ci->strided_size,                                                            \
		ci->count,                                                                   \
		c_store_start,                                                               \
		c_load_start,                                                                \
		c_load_end,                                                                  \
		meta.fence_strategy,                                                         \
		meta.fence_freq,                                                             \
		ci->repeat,                                                                  \
		ci->region_align,                                                            \
		meta.flush_after_load,                                                       \
		meta.flush_after_store,                                                      \
		meta.flush_l1,                                                               \
		meta.record_timing,                                                          \
		cycle_end - cycle_beg,                                                       \
		cycle_beg - cycle_all_beg);

#define COVERT_PC_STRIDED_PRINT_MEASUREMENT_CR(meta, cr)                       \
	kr_info("buf_addr 0x%llx\n", (uint64_t)ci->buf);                       \
	kr_info("[%s] region_size=%lu, "                                       \
		"block_size=%lu, "                                             \
		"region_skip=%lu, "                                            \
		"stride_size=%lu, "                                            \
		"count=%lu, "                                                  \
		"cycle=%ld:%ld:%ld, "                                          \
		"fence_strategy=%s, "                                          \
		"fence_freq=%s, "                                              \
		"repeat=%lu, "                                                 \
		"region_align=%lu, "                                           \
		"flush_after_load=%s, "                                        \
		"flush_after_store=%s, "                                       \
		"flush_l1=%s, "                                                \
		"record_timing=%s, "                                           \
		"total_cycle=%lu, "                                            \
		"cycle_beg_since_all_beg=%lu",                                 \
		meta.name,                                                     \
		ci->region_size,                                               \
		ci->block_size,                                                \
		ci->region_skip,                                               \
		ci->strided_size,                                              \
		ci->count,                                                     \
		cr->c_store_start,                                             \
		cr->c_load_start,                                              \
		cr->c_load_end,                                                \
		meta.fence_strategy,                                           \
		meta.fence_freq,                                               \
		ci->repeat,                                                    \
		ci->region_align,                                              \
		meta.flush_after_load,                                         \
		meta.flush_after_store,                                        \
		meta.flush_l1,                                                 \
		meta.record_timing,                                            \
		cr->cycle_end - cr->cycle_beg,                                 \
		cr->cycle_beg - cr->cycle_all_beg);

#define PRINT_CYCLES(cr, ci)                                                   \
	printf("Cycle stats:\n");                                              \
	printf("  [0] cycle_all_beg        : %10lu : %10lu\n",                 \
	       cr->cycle_all_beg,                                              \
	       cr->cycle_all_beg - ci->cycle_global_beg);                      \
	printf("  [1] cycle_beg            : %10lu : %10lu\n",                 \
	       cr->cycle_beg - cr->cycle_all_beg,                              \
	       cr->cycle_beg - cr->cycle_beg);                                 \
	printf("  [2] cycle_timing_init_beg: %10lu : %10lu\n",                 \
	       cr->cycle_timing_init_beg - cr->cycle_all_beg,                  \
	       cr->cycle_timing_init_beg - cr->cycle_beg);                     \
	printf("  [3] cycle_timing_init_end: %10lu : %10lu\n",                 \
	       cr->cycle_timing_init_end - cr->cycle_all_beg,                  \
	       cr->cycle_timing_init_end - cr->cycle_timing_init_beg);         \
	printf("  [4] cycle_store_beg      : %10lu : %10lu\n",                 \
	       cr->c_store_start - cr->cycle_all_beg,                          \
	       cr->c_store_start - cr->cycle_timing_init_end);                 \
	printf("  [5] cycle_load_beg       : %10lu : %10lu\n",                 \
	       cr->c_load_start - cr->cycle_all_beg,                           \
	       cr->c_load_start - cr->c_store_start);                          \
	printf("  [6] cycle_load_end       : %10lu : %10lu\n",                 \
	       cr->c_load_end - cr->cycle_all_beg,                             \
	       cr->c_load_end - cr->c_load_start);                             \
	printf("  [7] cycle_end            : %10lu : %10lu\n",                 \
	       cr->cycle_end - cr->cycle_all_beg,                              \
	       cr->cycle_end - cr->c_load_end);                                \
	printf("  [8] cycle_ddl_end        : %10lu : %10lu\n",                 \
	       cr->cycle_ddl_end - cr->cycle_all_beg,                          \
	       cr->cycle_ddl_end - cr->cycle_end);                             \
	printf("  [9] cycle_stats_end      : %10lu : %10lu\n",                 \
	       cr->cycle_stats_end - cr->cycle_all_beg,                        \
	       cr->cycle_stats_end - cr->cycle_ddl_end);                       \
	if (cr->cycle_beg + ci->iter_cycle_ddl < cr->cycle_end) {              \
		printf("WARNING: iter_cycle_end [%lu] exceeds iter_cycle_ddl " \
		       "[%lu], skipping wait\n",                               \
		       cr->cycle_end,                                          \
		       cr->cycle_beg + ci->iter_cycle_ddl);                    \
	}

#define RECORD_CYCLES(cr)                                                      \
	cr->c_store_start	  = c_store_start;                             \
	cr->c_load_start	  = c_load_start;                              \
	cr->c_load_end		  = c_load_end;                                \
	cr->cycle_all_beg	  = cycle_all_beg;                             \
	cr->cycle_beg		  = cycle_beg;                                 \
	cr->cycle_end		  = cycle_end;                                 \
	cr->cycle_timing_init_beg = cycle_timing_init_beg;                     \
	cr->cycle_timing_init_end = cycle_timing_init_end;                     \
	cr->cycle_ddl_end	  = cycle_ddl_end;                             \
	cr->cycle_stats_end	  = cycle_stats_end;

#define PRINT_INFO(ci)                                                         \
	printf("buf_addr 0x%lx\n\n", (uint64_t)ci->buf);                       \
	printf("buf=%px, "                                                     \
	       "region_size=%lu, "                                             \
	       "region_skip=%lu, "                                             \
	       "block_size=%lu, "                                              \
	       "repeat=%lu, "                                                  \
	       "count=%lu, "                                                   \
	       "chasing_func_index=%lu, "                                      \
	       "strided_size=%lu, "                                            \
	       "region_align=%lu, "                                            \
	       "cindex=%px, "                                                  \
	       "timing=%px, "                                                  \
	       "iter_cycle_ddl=%lu, "                                          \
	       "",                                                             \
	       ci->buf,                                                        \
	       ci->region_size,                                                \
	       ci->region_skip,                                                \
	       ci->block_size,                                                 \
	       ci->repeat,                                                     \
	       ci->count,                                                      \
	       ci->chasing_func_index,                                         \
	       ci->strided_size,                                               \
	       ci->region_align,                                               \
	       ci->cindex,                                                     \
	       ci->timing,                                                     \
	       ci->iter_cycle_ddl);                                            \
	printf("CHASING_FENCE_STRATEGY_ID=%d,", CHASING_FENCE_STRATEGY_ID);    \
	printf("CHASING_FENCE_FREQ_ID=%d,", CHASING_FENCE_FREQ_ID);            \
	printf("CHASING_FLUSH_AFTER_LOAD=%d,", CHASING_FLUSH_AFTER_LOAD);      \
	printf("CHASING_FLUSH_AFTER_STORE=%d,", CHASING_FLUSH_AFTER_STORE);    \
	printf("CHASING_FLUSH_L1=%d,", CHASING_FLUSH_L1);                      \
	printf("CHASING_FLUSH_L1_TYPE=%s,", CHASING_FLUSH_L1_TYPE);            \
	printf("COVERT_CHASING_STORE=%d,", COVERT_CHASING_STORE);              \
	printf("COVERT_CHASING_LOAD=%d\n", COVERT_CHASING_LOAD);


#define CLEAR_CPU_PIPELINE()                                                   \
	asm volatile("nop\n nop\n nop\n nop\n "                                \
		     "nop\n nop\n nop\n nop\n "                                \
		     "nop\n nop\n nop\n nop\n "                                \
		     "nop\n nop\n nop\n nop\n ");

#endif /* LENS_UTILS_H */
