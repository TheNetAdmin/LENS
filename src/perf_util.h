/* 
 *  Copyright (c) 2020 Zixuan Wang <zxwang@ucsd.edu>
 *  Copyright (c) 2019 Jian Yang
 *                     Regents of the University of California,
 *                     UCSD Non-Volatile Systems Lab
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PERF_UTIL_H_
#define _PERF_UTIL_H_

#include <linux/ioctl.h>
#include <linux/perf_event.h>
#include <linux/string.h>

/* There is a `perf_event` in `perf_event.h`
 * So use this name to prevent conflict
 */
struct __perf_event {
	struct perf_event_attr attr;
	u64 value;
	u64 enabled;
	u64 running;
	const char *name;
};

#define PERF_EVENT_READ(e, evt)                                                \
	e->value = perf_event_read_value(evt, &e->enabled, &e->running);

#define PERF_EVENT_PRINT(e)                                                    \
	pr_info("\tPerf counter [%s]\t: %lld\n", e.name, e.value);

#define PERF_EVENT_HW(cfg)                                                     \
	{                                                                      \
		.attr =                                                        \
		{                                                              \
			.type = PERF_TYPE_HARDWARE,                            \
			.config = PERF_COUNT_HW_##cfg,                         \
			.size = sizeof(struct perf_event_attr),                \
			.pinned = 1,                                           \
			.disabled = 1,                                         \
			.exclude_user = 1,                                     \
			.exclude_hv = 1,                                       \
			.exclude_idle = 1,                                     \
		},                                                             \
		.name = "HW." #cfg, .value = 0xffff, .enabled = 0xffff,        \
		.running = 0xffff,                                             \
	}

#define DEFINE_PERF_EVENT_HW(event, cfg)                                       \
	struct __perf_event event = PERF_EVENT_HW(cfg);

#define PERF_EVENT_CACHE(level, op, result)                                    \
	{                                                                      \
		.attr =                                                        \
		{                                                              \
			.type = PERF_TYPE_HW_CACHE,                            \
			.config = (PERF_COUNT_HW_CACHE_##level)                \
				| (PERF_COUNT_HW_CACHE_OP_##op << 8)           \
				| (PERF_COUNT_HW_CACHE_RESULT_##result << 16), \
			.size = sizeof(struct perf_event_attr),                \
			.pinned = 1,                                           \
			.disabled = 1,                                         \
			.exclude_user = 1,                                     \
			.exclude_hv = 1,                                       \
			.exclude_idle = 1,                                     \
		},                                                             \
		.name = "CACHE." #level "." #op "." #result, .value = 0xffff,  \
		.enabled = 0xffff, .running = 0xffff,                          \
	}

#define DEFINE_PERF_EVENT_CACHE(event, level, op, result)                      \
	struct __perf_event event = PERF_EVENT_CACHE(level, op, result);

int lattest_perf_event_create(void);
void lattest_perf_event_read(void);
void lattest_perf_event_enable(void);
void lattest_perf_event_disable(void);
void lattest_perf_event_reset(void);
void lattest_perf_event_release(void);
void lattest_perf_event_print(void);

#endif /* _PERF_UTIL_H_ */
