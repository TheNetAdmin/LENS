#ifndef LENS_UTILS_H
#define LENS_UTILS_H

#include <linux/kernel.h>
#include <linux/time.h>
#include <asm/fpu/api.h>

#ifndef kr_info
#define kr_info(string, args...)                                               \
	do {                                                                   \
		printk(KERN_INFO "{%d}" string, ctx->core_id, ##args);         \
	} while (0)
#endif

#define BENCHMARK_BEGIN(flags)                                                 \
	drop_cache();                                                          \
	local_irq_save(flags);                                                 \
	local_irq_disable();                                                   \
	kernel_fpu_begin();

#define BENCHMARK_END(flags)                                                   \
	kernel_fpu_end();                                                      \
	local_irq_restore(flags);                                              \
	local_irq_enable();

#define TIMEDIFF(start, end)                                                   \
	((end.tv_sec - start.tv_sec) * 1000000000 +                            \
	 (end.tv_nsec - start.tv_nsec))

static inline uint64_t rdtscp_lfence(void)
{
	uint32_t eax, edx;

	asm volatile("rdtscp \n\t"
		     "lfence \n\t"
		     : "=a"(eax), "=d"(edx));
	return ((uint64_t)edx) << 32 | eax;
}

#define PC_VARS                                                                \
	struct timespec tstart, tend;                                          \
	uint64_t	c_st_beg, c_ld_beg, c_ld_end;

#define PC_BEFORE_WRITE                                                        \
	getrawmonotonic(&tstart);                                              \
	c_st_beg = rdtscp_lfence();

#define PC_BEFORE_READ c_ld_beg = rdtscp_lfence();

#define PC_AFTER_READ                                                          \
	c_ld_end = rdtscp_lfence();                                            \
	getrawmonotonic(&tend);

#define PC_PRINT_MEASUREMENT(meta)                                             \
	diff = TIMEDIFF(tstart, tend);                                         \
	kr_info("buf_addr 0x%llx\n", (uint64_t)buf);                           \
	kr_info("[%s] region_size %lld, "                                      \
		"block_size %lld, "                                            \
		"count %lld, "                                                 \
		"total %ld ns, "                                               \
		"average %lld ns, "                                            \
		"cycle %lld - %lld - %lld, "                                   \
		"fence_strategy %s, "                                          \
		"fence_freq %s.\n",                                            \
		meta.name,                                                     \
		region_size,                                                   \
		block_size,                                                    \
		count,                                                         \
		diff,                                                          \
		diff / count,                                                  \
		c_st_beg,                                                      \
		c_ld_beg,                                                      \
		c_ld_end,                                                      \
		meta.fence_strategy,                                           \
		meta.fence_freq);

#define _GET_VAL(val, ptr) (val)
#define _GET_PTR_VAL(val, ptr) (ptr->val)

#define _PC_STRIDED_PRINT_MEASUREMENT(meta, get, ptr)                          \
	diff = TIMEDIFF(tstart, tend);                                         \
	kr_info("buf_addr 0x%llx\n", (uint64_t)get(buf, ptr));                 \
	kr_info("[%s] region_size=%llu, "                                      \
		"block_size=%llu, "                                            \
		"region_skip=%llu, "                                           \
		"stride_size=%llu, "                                           \
		"count=%llu, "                                                 \
		"total=%ld ns, "                                               \
		"average=%lld ns, "                                            \
		"cycle=%lld:%lld:%lld, "                                       \
		"fence_strategy=%s, "                                          \
		"fence_freq=%s, "                                              \
		"repeat=%llu, "                                                \
		"op=%llu, "                                                    \
		"region_align=%llu, "                                          \
		"flush_after_load=%s, "                                        \
		"flush_after_store=%s, "                                       \
		"flush_l1=%s, "                                                \
		"flush_l1_flush_after_load=%s, "                               \
		"record_timing=%s"                                             \
		,                                                              \
		meta.name,                                                     \
		get(region_size, ptr),                                         \
		get(block_size, ptr),                                          \
		get(region_skip, ptr),                                         \
		get(strided_size, ptr),                                        \
		get(count, ptr),                                               \
		diff,                                                          \
		diff / get(count, ptr),                                        \
		c_st_beg,                                                      \
		c_ld_beg,                                                      \
		c_ld_end,                                                      \
		meta.fence_strategy,                                           \
		meta.fence_freq,                                               \
		get(repeat, ptr),                                              \
		get(op, ptr),                                                  \
		get(region_align, ptr),                                        \
		meta.flush_after_load,                                         \
		meta.flush_after_store,                                        \
		meta.flush_l1,                                                 \
		meta.flush_l1_flush_after_load,                                \
		meta.record_timing);

#define PC_STRIDED_PRINT_MEASUREMENT(meta)                                     \
	_PC_STRIDED_PRINT_MEASUREMENT(meta, _GET_VAL, )

#define COVERT_PC_STRIDED_PRINT_MEASUREMENT(meta)                              \
	_PC_STRIDED_PRINT_MEASUREMENT(meta, _GET_PTR_VAL, ci)

#define PRINT_CHANNEL_INFO(func_name, ci)                                      \
	kr_info("buf_addr 0x%llx\n", (uint64_t)ci->buf);                       \
	kr_info("[%s] "                                                        \
		"role_type=%u, "                                               \
		"total_data_bits=%lu, "                                        \
		"send_data=%px, "                                              \
		"buf=%px, "                                                    \
		"op=%llu, "                                                    \
		"region_size=%llu, "                                           \
		"region_skip=%llu, "                                           \
		"block_size=%llu, "                                            \
		"repeat=%llu, "                                                \
		"count=%llu, "                                                 \
		"chasing_func_index=%lu, "                                     \
		"strided_size=%llu, "                                          \
		"region_align=%llu, "                                          \
		"cindex=%px, "                                                 \
		"timing=%px, "                                                 \
		"delay=%llu, "                                                 \
		"delay_per_byte=%llu, "                                        \
		"threshold_cycle=%llu, "                                       \
		"threshold_iter=%llu, "                                        \
		"warm_up=%u, "                                                 \
		"victim_iter=%llu, "                                           \
		"attacker_iter=%llu, "                                         \
		,                                                              \
		func_name,                                                     \
		ci->role_type,                                                 \
		ci->total_data_bits,                                           \
		ci->send_data,                                                 \
		ci->buf,                                                       \
		ci->op,                                                        \
		ci->region_size,                                               \
		ci->region_skip,                                               \
		ci->block_size,                                                \
		ci->repeat,                                                    \
		ci->count,                                                     \
		ci->chasing_func_index,                                        \
		ci->strided_size,                                              \
		ci->region_align,                                              \
		ci->cindex,                                                    \
		ci->timing,                                                    \
		ci->delay,                                                     \
		ci->delay_per_byte,                                            \
		ci->threshold_cycle,                                           \
		ci->threshold_iter,                                            \
		ci->warm_up,                                                   \
		ci->victim_iter,                                               \
		ci->attacker_iter                                              \
	);

#endif /* LENS_UTILS_H */
