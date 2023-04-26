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

#ifndef __LATENCY_H__
#define __LATENCY_H__

#if __KERNEL__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/completion.h>
#include <linux/sched/debug.h>
#endif

#include "common.h"

#define CACHELINE_BITS	  (6)
#define CACHELINE_SIZE	  (64)
#define LATENCY_OPS_COUNT 1048576L

/*
 * Task 1: For random latency tests
 * | OP_COUNT * 8    |
 * +-----------------+---------------+---------------+-----+---------------+
 * |   Random Pool   | Report Task 0 | Report Task 1 | ... | Report Task N |
 * +-----------------+---------------+---------------+-----+---------------+
 *
 * Task 2: Strided latency
 * | OP_COUNT * 8  |
 * +---------------+---------------+-----+---------------+
 * | Report Task 0 | Report Task 1 | ... | Report Task N |
 * +---------------+---------------+-----+---------------+
 *
 * Task 5:Overwrite latency
 * | 2 * OP_COUNT * 8 |
 * +--------------------+
 * | Per-access latency |
 * +--------------------+
 *
 *
 */

#define OPT_NOPARAM                     1
#define OPT_INT	                        2
#define OPT_STRING                      4

/* LATENCY_OPS_COUNT = Subtasks (seq/rand) */
#define BASIC_OP_POOL_BITS	        30
#define BASIC_OP_POOL_SIZE	        (1L << POOL_BITS) /* Size of the test region */
#define BASIC_OP_POOL_PAGE_BIT	        (BASIC_OP_POOL_BITS - PAGE_SHIFT)
#define BASIC_OP_POOL_PAGES	        (1L << BASIC_OP_POOL_PAGE_BIT) /* # Pages */
#define BASIC_OP_POOL_LINE_BITS         (BASIC_OP_POOL_BITS - CACHELINE_BITS)
#define BASIC_OP_MASK                   0x3FFFFFC0 /*0b1{24, POOL_LINE_BITS}0{6, CACHELINE_BITS} */

//#define PRETHREAD_BIT			33 /* 8GB/Thread */
//#define GLOBAL_BIT			38 /* 256GB/Global */
//#define PRETHREAD_BIT			28 /* 256MB/Thread */

#define PRETHREAD_BIT                   31 /* 2GB/Thread */
#define GLOBAL_BIT                      36 /* 64GB/Global */
#define GLOBAL_BIT_NI                   34 /* 16GB/DIMM, 96 GB with software interleaving */
#define DIMM_SIZE_BIT                   38 /* 245GB/DIMM */

#define GLOBAL_WORKSET	                (1ULL << GLOBAL_BIT)
#define PERTHREAD_WORKSET               (1ULL << PRETHREAD_BIT)
#define PERDIMMGROUP_WORKSET            (PERTHREAD_WORKSET * 6)
#define DIMM_SIZE_IDEAL                 (1ULL << DIMM_SIZE_BIT)

//#define PERTHREAD_MASK		0xFFFFFC0 //256M
#define PERTHREAD_MASK                  0x7FFFFFC0 //2G

#define PERTHREAD_CHECKSTOP             (1L << 26) /* yield after wrtting 64MB */

#define MB                              0x100000
#define GB                              0x40000000

#define DIMM_SIZE	                (250UL * GB) /* USABLE MEMORY: 252.4 GB */
#define GLOBAL_WORKSET_NI               (1UL << GLOBAL_BIT_NI)

#define LFS_THREAD_MAX	                48
#define LFS_ACCESS_MAX_BITS             32
//#define LFS_ACCESS_MAX_BITS	        22
#define LFS_ACCESS_MAX                  (1UL << LFS_ACCESS_MAX_BITS)
#define LFS_RUNTIME_MAX                 600
#define LFS_DELAY_MAX			(3UL * 1000 * 1000 * 1000)

#define LFS_PERMRAND_ENTRIES            0x1000
#define LFS_PERMRAND_SIZE               LFS_PERMRAND_ENTRIES * 4
#define LFS_PERMRAND_SIZE_IMM           "$0x4000,"

#define LAT_WARN(x)                                                            \
	if (!(x)) {                                                            \
		dump_stack();                                                  \
		printk(KERN_WARNING "assertion failed %s:%d: %s\n", __FILE__,  \
		       __LINE__, #x);                                          \
	}

#define LAT_ASSERT_EXIT(x)                                                     \
	if (!(x)) {                                                            \
		dump_stack();                                                  \
		printk(KERN_WARNING "assertion failed %s:%d: %s\n", __FILE__,  \
		       __LINE__, #x);                                          \
		BUG_ON("LAT_ASSERT failed");                                   \
	}

#define rdtscp(low, high, aux)                                                 \
	__asm__ __volatile__(".byte 0x0f,0x01,0xf9"                            \
			     : "=a"(low), "=d"(high), "=c"(aux))

/* since Skylake, Intel uses non-inclusive last-level cache,
 * drop cache before testing
 */
static inline void drop_cache(void)
{
	asm volatile("wbinvd" : : : "memory");
	asm volatile("mfence" : : : "memory");
}

typedef enum {
	TASK_INVALID = 0,
	TASK_BASIC_OP,
	TASK_STRIDED_LAT,
	TASK_STRIDED_BW,
	TASK_SIZE_BW,
	TASK_OVERWRITE,
	TASK_PC_WRITE,
	TASK_PC_READ_AND_WRITE,
	TASK_PC_READ_AFTER_WRITE,
	TASK_SEQ,
	TASK_FLUSH_FIRST,
	TASK_WEAR_LEVELING,
	TASK_PC_STRIDED,
	TASK_BUFFER_COVERT_CHANNEL,
	TASK_DEBUGGING,
	TASK_WEAR_LEVELING_COVERT_CHANNEL,
	TASK_WEAR_LEVELING_SIDE_CHANNEL,

	TASK_COUNT
} lattest_task;

typedef struct {
	char *name;
	char *options;
} task_desc_t;

static task_desc_t task_desc[] = {
	{"Invalid",					""},
	{"Basic Latency Test",				"(not configurable)"},
	{"Strided Latency Test ",			"[access_size] [stride_size]"},
	{"Strided Bandwidth Test",			"[access_size] [stride_size] [delay] [parallel] [runtime] [global]"},
	{"Random Size-Bandwidth Test",			"[bwtest_bit] [access_size] [parallel] [runtime] [align_mode]"},
	{"Overwrite Test",				"[access_size]"},
	{"Pointer Chasing Write Test",			"[pc_region_size] [pc_block_size] [op| 0:forth, 1:back-and-forth]"},
	{"Pointer Chasing Read and Write Test",		"[pc_region_size] [pc_block_size] [op| 0:forth, 1:back-and-forth]"},
	{"Pointer Chasing Read after Write Test",	"[pc_region_size] [pc_block_size] [op| 0:forth, 1:back-and-forth]"},
	{"Sequencial Test",				"[access_size], [parallel]"},
	{"Flush First Test",				"[stride_size] [access_size] [op]"},
	{"Wear Leveling Test",				"[access_size] [stride_size] [delay] [delay_per_byte] [op]"},
	{"Pointer Chasing Strided Test",		"[pc_region_size] [pc_block_size] [stride_size] [pc_region_align] [repeat] [op| 0:read_and_write, 1:read_after_write, 2:write-new-order-per-repeat, 3:raw-new-order-per-repeat, 4:read_and_write_back_and_forth, 5:read_after_write_back_and_forth]"},
	{"Buffer Covert Chasnnel Test",			"[pc_region_size] [pc_block_size] [stride_size] [pc_region_align] [repeat] [threads] [align_mode] [align_size]"},
	{"Debugging Test",				""},
	{"Wear Leveling Covert Channel Test",		"[access_size (# of bit sent)] [pc_region_size (wear leveling region size)] [stride_size (i.e. distance)] [delay] [delay_per_byte] [op] [repeat (# accesses per iter)] [count (# top latencies per iter)]"},
	{"Wear Leveling Side Channel Test",		"[access_size (# of bit sent)] [pc_region_size (wear leveling region size)] [stride_size (i.e. distance)] [delay] [delay_per_byte] [op] [repeat (# victim accesses per iter)] [threshold_cycle] [threshold_iter (# attacker accesses per iter)] [warm_up]"},
};

typedef enum {
	ALIGN_GLOBAL = 0, /* Merge with global option */
	ALIGN_PERTHREAD,
	ALIGN_PERDIMM,
	ALIGN_PERIMC,
	ALIGN_PERCHANNELGROUP,
	ALIGN_NI_GLOBAL,
	ALIGN_NI,
	ALIGN_BY_SIZE,

	ALIGN_INVALID
} align_mode;

typedef int(bench_func_t)(void *);

/* Fine if the struct is not cacheline aligned */
struct latencyfs_timing {
	uint64_t v;
	char padding[56]; /* 64 - 8 */
} __attribute((__packed__));

struct report_sbi {
	struct super_block *sb;
	struct block_device *s_bdev;
	void *virt_addr;
	unsigned long initsize;
	uint64_t phys_addr;
};

struct latency_sbi {
	struct super_block * sb;
	struct block_device *s_bdev;

	void *	      virt_addr;
	unsigned long initsize;
	uint64_t      phys_addr;
	uint64_t      write_start;
	uint64_t      write_size;
	uint64_t      repeat;
	uint64_t      count;
	uint64_t      access_size;
	uint64_t      strided_size;
	uint64_t      pc_region_align;
	uint64_t      pc_region_size;
	uint64_t      pc_block_size;
	uint64_t      align_size;
	int	      align_mode;
	int	      clwb_rate;
	int	      fence_rate;
	int	      worker_cnt;
	int	      runtime;
	int	      bwsize_bit;
	uint64_t      delay;
	uint64_t      delay_per_byte;
	int	      op;
	int           sync;
	int           sync_per_iter;
	uint64_t      threshold_cycle;
	uint64_t      threshold_iter;
	int           task;
	unsigned      warm_up;

	struct latencyfs_timing *timing;
	struct report_sbi *rep;
	struct task_struct **workers;
	struct latencyfs_worker_ctx *ctx;
};

struct latencyfs_worker_ctx {
	int		    core_id;
	int		    job_id;
	uint8_t *	    addr;
	uint8_t *	    seed_addr; /* for multi-threaded tasks only */
	struct latency_sbi *sbi;
	struct completion   complete;
	struct completion   sub_op_ready;
	struct completion   sub_op_complete;
};

void print_core_id(void);

int latency_job(void *arg);
int strided_latjob(void *arg);
int strided_bwjob(void *arg);
int sizebw_job(void *arg);
int latencyfs_proc_init(void);
int overwrite_job(void *arg);
int pointer_chasing_write_job(void *arg);
int pointer_chasing_read_and_write_job(void *arg);
int pointer_chasing_strided_job(void *arg);
int pointer_chasing_read_after_write_job(void *arg);
int seq_bwjob(void *arg);
int flush_first_job(void *arg);
int wear_leveling_job(void *arg);
int buffer_covert_channel_job(void *arg);
int debugging_job(void *arg);
int wear_leveling_covert_channel_job(void *arg);
int wear_leveling_side_channel_job(void *arg);

struct latency_option {
	const char *name;
	unsigned int has_arg;
	int val;
};

#if __KERNEL__
/* misc */
inline void latencyfs_prealloc_large(uint8_t *addr, uint64_t size,
				     uint64_t bit_mask);
inline unsigned long fastrand(unsigned long *x, unsigned long *y,
			      unsigned long *z, uint64_t bit_mask);
inline void latencyfs_prealloc_global_permutation_array(int size);

int latencyfs_getopt(const char *caller, char **options,
		     const struct latency_option *opts, char **optopt,
		     char **optarg, unsigned long *value);
void latencyfs_start_task(struct latency_sbi *sbi, int task, int threads);

int latencyfs_print_help(struct seq_file *seq, void *v);

static inline bool validate_address(struct latency_sbi *sbi, void *addr)
{
	uint64_t addr_beg = (uint64_t)sbi->virt_addr;
	uint64_t addr_end = (uint64_t)sbi->virt_addr + sbi->initsize;
	uint64_t addr_cur = (uint64_t)addr;
	if (likely((addr_beg <= addr_cur) && (addr_cur <= addr_end))) {
		return true;
	} else {
		printk(KERN_WARNING
		       "Address [0x%016llx] out of range [0x%016llx]-[0x%016llx]",
		       addr_cur,
		       addr_beg,
		       addr_end);
		WARN_ONCE(true, "Validate address failed");
		return false;
	}
}

static inline void print_regs(void)
{
	struct pt_regs regs;
	// prepare_frametrace(&regs);
	show_regs(&regs);
}
#endif

#endif // __LATENCY_H__
