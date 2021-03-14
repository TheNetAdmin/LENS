#ifndef LENS_MICROBENCH_OVERWRITE_H
#define LENS_MICROBENCH_OVERWRITE_H

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/string.h>
#include "../lat.h"

#define LATENCY_LEVELS (11)
typedef struct {
	uint64_t levels[LATENCY_LEVELS];
} overwrite_latency_t;

static const u64 overwrite_latency_level_bound[LATENCY_LEVELS + 1] = {
	0,
	1000,
	5000,
	10000,
	25000,
	50000,
	75000,
	100000,
	150000,
	250000,
	500000,
	1000000,
};

static inline void init_overwrite_latency(overwrite_latency_t *lat)
{
	memset(lat->levels, 0, (LATENCY_LEVELS) * sizeof(uint64_t));
}

static inline void count_overwrite_latency(overwrite_latency_t *lat, u64 cycle)
{
	int i = 0;
	for (i = 0; i < LATENCY_LEVELS; i++) {
		if ((overwrite_latency_level_bound[i] <= cycle) &&
		    (overwrite_latency_level_bound[i + 1] > cycle)) {
			lat->levels[i] += 1;
			break;
		}
	}
	if (unlikely(i == LATENCY_LEVELS)) {
		printk(KERN_WARNING "cycle [%llu] out of range [%llu]\n",
		       cycle,
		       overwrite_latency_level_bound[LATENCY_LEVELS]);
		BUG();
	}
}

#define LATENCY_MSG_BUF_SIZE (4096)
#define latency_msg(buf, fmt, args...)                                         \
	do {                                                                   \
		snprintf(buf + strlen(buf),                                    \
			 LATENCY_MSG_BUF_SIZE - strlen(buf),                   \
			 fmt,                                                  \
			 ##args);                                              \
	} while (0)

static inline void snprintf_latency(char *buf, overwrite_latency_t *lat, int bit_id)
{
	int i = 0;

	buf[0] = '\0';
	latency_msg(buf, "bit_id=%u, latencies=[ ", bit_id);
	for (i = 0; i < LATENCY_LEVELS; i++) {
		latency_msg(buf,
			    "[%7llu-%7llu]: %10llu, ",
			    overwrite_latency_level_bound[i],
			    overwrite_latency_level_bound[i + 1],
			    lat->levels[i]);
	}
	latency_msg(buf, "]\n");
}

static inline void
snprintf_latency_summary(char *buf, overwrite_latency_t *lat, int bit_id)
{
	int		    i;
	overwrite_latency_t summary;

	memcpy(summary.levels, lat->levels, sizeof(summary.levels));
	for (i = LATENCY_LEVELS - 2; i >= 0; i--) {
		summary.levels[i] += summary.levels[i + 1];
	}

	buf[0] = '\0';
	latency_msg(buf, "bit_id=%u, latencies_summary=[ ", bit_id);
	for (i = 0; i < LATENCY_LEVELS; i++) {
		latency_msg(buf,
			    "[%7llu-%7llu]: %10llu, ",
			    overwrite_latency_level_bound[i],
			    overwrite_latency_level_bound[i + 1],
			    summary.levels[i]);
	}
	latency_msg(buf, "]\n");
}

typedef enum {
	OVERWRITE_NTSTORE,
	OVERWRITE_CLWB
} overwrite_flush_type_t;

typedef enum {
	OVERWRITE_BY_BLOCK,
	OVERWRITE_BY_RANGE
} overwrite_warmup_type_t;

typedef struct {
	uint64_t iter;
	uint64_t cycle;
	uint64_t total_cycle;
	uint64_t total_ns;
	overwrite_flush_type_t flush_type;
	overwrite_warmup_type_t warmup_type;
} overwrite_warmup_result_t;

void overwrite_warmup_range_by_block(char *start_addr, char *end_addr, uint64_t skip, uint64_t threshold_cycle, uint64_t delay);
void overwrite_warmup_range_by_range(char *start_addr, char *end_addr, uint64_t skip, uint64_t threshold_cycle, uint64_t delay);
void overwrite_warmup_thread(struct latencyfs_worker_ctx *ctx);
void overwrite_vanilla(char *start_addr, char *end_addr, uint64_t skip, uint64_t count, long *rep_buf);
void overwrite_delay_each(char *start_addr, char *end_addr, uint64_t skip, uint64_t count, long *rep_buf, long delay);
void overwrite_two_points(char *start_addr, char *end_addr, uint64_t count, long *rep_buf, uint64_t distance);
void overwrite_delay_half(char *start_addr, char *end_addr, uint64_t skip, uint64_t count, long *rep_buf, long delay);
void overwrite_delay_per_size(
        char *start_addr,
        char *end_addr,
        uint64_t skip,
        uint64_t count,
        long *rep_buf,
        long delay,
        uint64_t delay_per_byte);
void overwrite_delay_per_size_cpuid(
        char *start_addr,
        char *end_addr,
        uint64_t skip,
        uint64_t count,
        long *rep_buf,
        long delay,
        uint64_t delay_per_byte);
void overwrite_percentile_delay_per_size(
	char *			   start_addr,
	char *			   end_addr,
	uint64_t		   skip,
	uint64_t		   repeat,
	overwrite_latency_t *	   latencies,
	long			   delay,
	uint64_t		   delay_per_byte);
#endif // LENS_MICROBENCH_OVERWRITE_H
