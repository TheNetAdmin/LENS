#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define LATENCY_LEVELS (9)
typedef struct {
	uint64_t levels[LATENCY_LEVELS];
	uint64_t cycle_beg;
	uint64_t cycle_end;
	uint64_t cycle_without_delay;
	uint64_t cycle_after_ddl;
} overwrite_latency_t;

static const uint64_t overwrite_latency_level_bound[LATENCY_LEVELS + 1] = {
	0,
	// 1000,
	// 5000,
	// 10000,
	// 15000,
	// 20000,
	25000,
	// 30000,
	// 35000,
	// 40000,
	45000,
	// 50000,
	// 55000,
	65000,
	// 75000,
	85000,
	// 100000,
	150000,
	// 250000,
	500000,
	// 1000000,
	10000000,
	100000000,
};

static inline void init_overwrite_latency(overwrite_latency_t *lat)
{
	memset(lat->levels, 0, (LATENCY_LEVELS) * sizeof(uint64_t));
}

static inline void count_overwrite_latency(overwrite_latency_t *lat, uint64_t cycle)
{
	int i = 0;
	for (i = 0; i < LATENCY_LEVELS; i++) {
		if ((overwrite_latency_level_bound[i] <= cycle) &&
		    (overwrite_latency_level_bound[i + 1] > cycle)) {
			lat->levels[i] += 1;
			break;
		}
	}
	if (i == LATENCY_LEVELS) {
		printf("ERROR: cycle [%lu] out of range [%lu]\n",
		       cycle,
		       overwrite_latency_level_bound[LATENCY_LEVELS]);
		// exit(1);
	}
}

#define LATENCY_MSG_BUF_SIZE (8192)
#define latency_msg(buf, fmt, args...)                                         \
	do {                                                                   \
		snprintf(buf + strlen(buf),                                    \
			 LATENCY_MSG_BUF_SIZE - strlen(buf),                   \
			 fmt,                                                  \
			 ##args);                                              \
	} while (0)


static inline void print_latency(char *buf, overwrite_latency_t *lat)
{
	int i = 0;

	buf[0] = '\0';
	latency_msg(buf, "latencies=[ ");
	for (i = 0; i < LATENCY_LEVELS; i++) {
		latency_msg(buf,
			    "[%7lu-%7lu]: %10lu, ",
			    overwrite_latency_level_bound[i],
			    overwrite_latency_level_bound[i + 1],
			    lat->levels[i]);
	}
	latency_msg(buf, "],");
	latency_msg(buf, "cycle_beg=%lu,", lat->cycle_beg);
	latency_msg(buf, "cycle_end=%lu,", lat->cycle_end);
	latency_msg(buf, "cycle_without_delay=%lu,", lat->cycle_without_delay);
	latency_msg(buf, "cycle_with_delay=%lu,", lat->cycle_end - lat->cycle_beg);
	latency_msg(buf, "cycle_after_ddl=%lu,", lat->cycle_after_ddl);
	latency_msg(buf, "\n");
}

static inline void print_latency_summary(char *buf, overwrite_latency_t *lat)
{
	int i = 0;

	overwrite_latency_t summary;
	memset(&summary, sizeof(overwrite_latency_t), 0);

	buf[0] = '\0';
	latency_msg(buf, "latencies=[ ");
	for (i = LATENCY_LEVELS - 1; i >= 0; i--) {
		summary.levels[i] = lat->levels[i];
		if (i < LATENCY_LEVELS - 1) {
			summary.levels[i] += summary.levels[i+1];
		}
	}
	for (i = 0; i < LATENCY_LEVELS; i++) {
		latency_msg(buf,
			    "[%7lu-%7lu]: %10lu, ",
			    overwrite_latency_level_bound[i],
			    overwrite_latency_level_bound[i + 1],
			    summary.levels[i]);
	}
	latency_msg(buf, "],");
	latency_msg(buf, "cycle_beg=%lu,", lat->cycle_beg);
	latency_msg(buf, "cycle_end=%lu,", lat->cycle_end);
	latency_msg(buf, "cycle_without_delay=%lu,", lat->cycle_without_delay);
	latency_msg(buf, "cycle_with_delay=%lu,", lat->cycle_end - lat->cycle_beg);
	latency_msg(buf, "cycle_after_ddl=%lu,", lat->cycle_after_ddl);
	latency_msg(buf, "\n");
}

#endif /* UTILS_H */
