#ifndef LENS_TASKS_H
#define LENS_TASKS_H

#define _GNU_SOURCE

#include <linux/compiler.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bug.h>
#include <linux/parser.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/magic.h>
#include <linux/random.h>
#include <linux/cred.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <asm/processor.h>
#include <asm/simd.h>
#include <asm/fpu/api.h>
#include <linux/kthread.h>
#include <asm/set_memory.h>
#include <linux/string.h>
#include "lat.h"
#include "memaccess.h"
#include "microbench/chasing.h"
#include "microbench/chasing_baf.h"
#include "latency.h"
#include "microbench/overwrite.h"
#include "microbench/misc.h"
#include "utils.h"
#include "microbench/channel.h"
// #include "lib/pqueue.h"
// #include "lib/pqueue-int.h"
#include <linux/utsname.h>

#ifdef USE_PERF
#include "perf_util.h"
#endif

//#define KSTAT 1

static inline void wait_for_stop(void)
{
	/* Wait until we are told to stop */
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (kthread_should_stop())
			break;
		schedule();
	}
}

#ifdef USE_PERF

#define PERF_CREATE(x) lattest_perf_event_create()
#define PERF_START(x)                                                          \
	lattest_perf_event_reset();                                            \
	lattest_perf_event_enable();
#define PERF_STOP(x)                                                           \
	lattest_perf_event_disable();                                          \
	lattest_perf_event_read();
#define PERF_PRINT(x)                                                          \
	kr_info("Performance counters:\n");                                    \
	lattest_perf_event_print();
#define PERF_RELEASE(x) lattest_perf_event_release();

#else // USE_PERF

#define PERF_CREATE(x)                                                         \
	do {                                                                   \
	} while (0)
#define PERF_START(x)                                                          \
	do {                                                                   \
	} while (0)
#define PERF_STOP(x)                                                           \
	do {                                                                   \
	} while (0)
#define PERF_PRINT(x)                                                          \
	do {                                                                   \
	} while (0)
#define PERF_RELEASE(x)                                                        \
	do {                                                                   \
	} while (0)

#endif // USE_PERF

typedef enum {
	per_thread,
	overlapped
} uc_addr_align_mode_t;

uint8_t *get_uc_addr(struct latencyfs_worker_ctx *ctx, uc_addr_align_mode_t align_mode);
uint64_t get_dimm_size(void);
void print_core_id(void);

#endif /* LENS_TASKS_H */
