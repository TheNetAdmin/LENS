/* 
 *  Copyright (c) 2022 Zixuan Wang <zxwang@ucsd.edu>
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

#ifndef CHASING_COMMON_H
#define CHASING_COMMON_H


#ifndef kr_info
#define kr_info(string, args...)                                               \
	do {                                                                   \
		printk(KERN_INFO "{%d}" string, ctx->core_id, ##args);         \
	} while (0)
#endif

#include <linux/kernel.h>

#ifndef KERNEL_BEGIN
#define KERNEL_BEGIN                                                           \
	do {                                                                   \
	} while (0);
#define KERNEL_END                                                             \
	do {                                                                   \
	} while (0);
#endif

#define CHASING_MFENCE   "mfence\n"
#define CHASING_NO_FENCE "nop\n"

#ifndef CHASING_FENCE_STRATEGY_ID
#define CHASING_FENCE_STRATEGY_ID 0
#endif

#if CHASING_FENCE_STRATEGY_ID == 0
#define CHASING_FENCE_STRATEGY "mfence"
#define CHASING_ST_FENCE       "mfence\n"
#define CHASING_LD_FENCE       "mfence\n"
#elif CHASING_FENCE_STRATEGY_ID == 1
#define CHASING_FENCE_STRATEGY "separate"
#define CHASING_ST_FENCE       "sfence\n"
#define CHASING_LD_FENCE       "lfence\n"
#elif CHASING_FENCE_STRATEGY_ID == 2
#define CHASING_FENCE_STRATEGY "cpuid"
#define CHASING_ST_FENCE       "push %%rdx\n push %%rcx\n push %%rbx\n push %%rax\n cpuid\n pop %%rax\n pop %%rbx\n pop %%rcx\n pop %%rdx\n"
#define CHASING_LD_FENCE       "push %%rdx\n push %%rcx\n push %%rbx\n push %%rax\n cpuid\n pop %%rax\n pop %%rbx\n pop %%rcx\n pop %%rdx\n"
#else
#define CHASING_FENCE_STRATEGY "mfence"
#define CHASING_ST_FENCE       "mfence\n"
#define CHASING_LD_FENCE       "mfence\n"
#endif

#ifndef CHASING_FENCE_FREQ_ID
#define CHASING_FENCE_FREQ_ID 0
#endif

#if CHASING_FENCE_FREQ_ID == 0
#define CHASING_FENCE_FREQ "region"
#define CHASING_ST_FENCE_BLOCK  CHASING_NO_FENCE
#define CHASING_LD_FENCE_BLOCK  CHASING_NO_FENCE
#define CHASING_ST_FENCE_REGION CHASING_ST_FENCE
#define CHASING_LD_FENCE_REGION CHASING_LD_FENCE
#elif CHASING_FENCE_FREQ_ID == 1
#define CHASING_FENCE_FREQ "block"
#define CHASING_ST_FENCE_BLOCK  CHASING_ST_FENCE
#define CHASING_LD_FENCE_BLOCK  CHASING_LD_FENCE
#define CHASING_ST_FENCE_REGION CHASING_NO_FENCE
#define CHASING_LD_FENCE_REGION CHASING_NO_FENCE
#else
#define CHASING_FENCE_FREQ "region"
#define CHASING_ST_FENCE_BLOCK  CHASING_NO_FENCE
#define CHASING_LD_FENCE_BLOCK  CHASING_NO_FENCE
#define CHASING_ST_FENCE_REGION CHASING_ST_FENCE
#define CHASING_LD_FENCE_REGION CHASING_LD_FENCE
#endif

#ifndef CHASING_FLUSH_AFTER_LOAD
#define CHASING_FLUSH_AFTER_LOAD 0
#endif

#if CHASING_FLUSH_AFTER_LOAD == 0
#define CHASING_FLUSH_AFTER_LOAD_TYPE "none"
#define CHASING_LOAD_FLUSH(cl_index)					       \
	"\n"
#elif CHASING_FLUSH_AFTER_LOAD == 1
#define CHASING_FLUSH_AFTER_LOAD_TYPE "clflush"
#define CHASING_LOAD_FLUSH(cl_index)					       \
	"clflush	(64 * (" #cl_index "))(%%r9, %%r12)\n"
#else
#define CHASING_FLUSH_AFTER_LOAD_TYPE "clflushopt"
#define CHASING_LOAD_FLUSH(cl_index)					       \
	"clflushopt	(64 * (" #cl_index "))(%%r9, %%r12)\n"
#endif

#ifndef CHASING_FLUSH_AFTER_STORE
#define CHASING_FLUSH_AFTER_STORE 0
#endif

#if CHASING_FLUSH_AFTER_STORE == 0
#define CHASING_FLUSH_AFTER_STORE_TYPE "none"
#define CHASING_STORE_FLUSH(cl_index) "\n"
#elif CHASING_FLUSH_AFTER_STORE == 1
#define CHASING_FLUSH_AFTER_STORE_TYPE "clflush"
#define CHASING_STORE_FLUSH(cl_index)                                          \
	"clflush	(64 * (" #cl_index "))(%%r9, %%r12)\n"
#else
#define CHASING_FLUSH_AFTER_STORE_TYPE "clflushopt"
#define CHASING_STORE_FLUSH(cl_index)                                          \
	"clflushopt	(64 * (" #cl_index "))(%%r9, %%r12)\n"
#endif

#ifndef CHASING_FLUSH_L1_FLUSH_AFTER_LOAD
#define CHASING_FLUSH_L1_FLUSH_AFTER_LOAD 0
#endif

#if CHASING_FLUSH_L1_FLUSH_AFTER_LOAD == 0
#define CHASING_FLUSH_L1_FLUSH_AFTER_LOAD_TYPE "none"
#define CHASING_FLUSH_L1_LOAD_FLUSH(block_index)			       \
	"\n"
#elif CHASING_FLUSH_L1_FLUSH_AFTER_LOAD == 1
#define CHASING_FLUSH_L1_FLUSH_AFTER_LOAD_TYPE "clflush"
#define CHASING_FLUSH_L1_LOAD_FLUSH(block_index)			       \
	"clflush	(256 * (" #block_index "))(%%r9, %%r12)\n"
#else
#define CHASING_FLUSH_L1_FLUSH_AFTER_LOAD_TYPE "clflushopt"
#define CHASING_FLUSH_L1_LOAD_FLUSH(block_index)			       \
	"clflushopt	(256 * (" #block_index "))(%%r9, %%r12)\n"
#endif

/*
 * TIMING_BUF:
 *   0  : temp timing (e.g. beging timing used in latency calc)
 *   1..: latency from round 0..
 */


#ifndef CHASING_FLUSH_L1
#define CHASING_FLUSH_L1 0
#endif

#if CHASING_FLUSH_L1 == 0
#define CHASING_FLUSH_L1_TYPE "none"
#define FLUSH_L1(PCBLOCK_SIZE, LABEL) "\n"
#elif CHASING_FLUSH_L1 == 1
#define CHASING_FLUSH_L1_TYPE "tail_region"
/* Flush L1 buffer with the last region in L2
 *   i.e. 16MB-16KB region
 *   ---> %r9 region start address
 *   ---> %r9+16MiB-16KiB -> %r10 flush start address
 */
#define FLUSH_L1_ONE_BLOCK(block_index)                                        \
	"vmovntdqa	(256 *  (	" #block_index "))(%%r9, %%r12), %%zmm" #block_index "	\n" \
	CHASING_FLUSH_L1_LOAD_FLUSH(block_index)

/* TODO: Use vgather instructions to reduce the zmm registers used? */
#define FLUSH_L1(PCBLOCK_SIZE, LABEL)                                          \
	"CHASING_FLUSH_L1_" #LABEL "_" #PCBLOCK_SIZE "_BEG:\n"                 \
	"cmp	$32768, %[accesssize]\n"                                       \
	"jge	CHASING_FLUSH_L1_" #LABEL "_" #PCBLOCK_SIZE "_END\n"           \
	"mov	$1,     %%r12	\n"                                            \
	"shl	$24,    %%r12	\n"                                            \
	"sub	$16384, %%r12	\n" /* r12: flush start offset */              \
	FLUSH_L1_ONE_BLOCK(0)	FLUSH_L1_ONE_BLOCK(1)                          \
	FLUSH_L1_ONE_BLOCK(2)	FLUSH_L1_ONE_BLOCK(3)                          \
	FLUSH_L1_ONE_BLOCK(4)	FLUSH_L1_ONE_BLOCK(5)                          \
	FLUSH_L1_ONE_BLOCK(6)	FLUSH_L1_ONE_BLOCK(7)                          \
	FLUSH_L1_ONE_BLOCK(8)	FLUSH_L1_ONE_BLOCK(9)                          \
	FLUSH_L1_ONE_BLOCK(10)	FLUSH_L1_ONE_BLOCK(11)                         \
	FLUSH_L1_ONE_BLOCK(12)	FLUSH_L1_ONE_BLOCK(13)                         \
	FLUSH_L1_ONE_BLOCK(14)	FLUSH_L1_ONE_BLOCK(15)                         \
	FLUSH_L1_ONE_BLOCK(16)	FLUSH_L1_ONE_BLOCK(17)                         \
	FLUSH_L1_ONE_BLOCK(18)	FLUSH_L1_ONE_BLOCK(19)                         \
	FLUSH_L1_ONE_BLOCK(20)	FLUSH_L1_ONE_BLOCK(21)                         \
	FLUSH_L1_ONE_BLOCK(22)	FLUSH_L1_ONE_BLOCK(23)                         \
	FLUSH_L1_ONE_BLOCK(24)	FLUSH_L1_ONE_BLOCK(25)                         \
	FLUSH_L1_ONE_BLOCK(26)	FLUSH_L1_ONE_BLOCK(27)                         \
	FLUSH_L1_ONE_BLOCK(28)	FLUSH_L1_ONE_BLOCK(29)                         \
	FLUSH_L1_ONE_BLOCK(30)	FLUSH_L1_ONE_BLOCK(31)                         \
	"add	$8192, %%r12	\n" /* 32 * 256 = 8192 */                      \
	"mfence			\n"                                            \
	FLUSH_L1_ONE_BLOCK(0)	FLUSH_L1_ONE_BLOCK(1)                          \
	FLUSH_L1_ONE_BLOCK(2)	FLUSH_L1_ONE_BLOCK(3)                          \
	FLUSH_L1_ONE_BLOCK(4)	FLUSH_L1_ONE_BLOCK(5)                          \
	FLUSH_L1_ONE_BLOCK(6)	FLUSH_L1_ONE_BLOCK(7)                          \
	FLUSH_L1_ONE_BLOCK(8)	FLUSH_L1_ONE_BLOCK(9)                          \
	FLUSH_L1_ONE_BLOCK(10)	FLUSH_L1_ONE_BLOCK(11)                         \
	FLUSH_L1_ONE_BLOCK(12)	FLUSH_L1_ONE_BLOCK(13)                         \
	FLUSH_L1_ONE_BLOCK(14)	FLUSH_L1_ONE_BLOCK(15)                         \
	FLUSH_L1_ONE_BLOCK(16)	FLUSH_L1_ONE_BLOCK(17)                         \
	FLUSH_L1_ONE_BLOCK(18)	FLUSH_L1_ONE_BLOCK(19)                         \
	FLUSH_L1_ONE_BLOCK(20)	FLUSH_L1_ONE_BLOCK(21)                         \
	FLUSH_L1_ONE_BLOCK(22)	FLUSH_L1_ONE_BLOCK(23)                         \
	FLUSH_L1_ONE_BLOCK(24)	FLUSH_L1_ONE_BLOCK(25)                         \
	FLUSH_L1_ONE_BLOCK(26)	FLUSH_L1_ONE_BLOCK(27)                         \
	FLUSH_L1_ONE_BLOCK(28)	FLUSH_L1_ONE_BLOCK(29)                         \
	FLUSH_L1_ONE_BLOCK(30)	FLUSH_L1_ONE_BLOCK(31)                         \
	"mfence			\n"                                            \
	"CHASING_FLUSH_L1_" #LABEL "_" #PCBLOCK_SIZE "_END:\n"                 \
	"xor	%%r12,  %%r12	\n"

/* Flush L1 buffer mush use per-repeat/per-block timing,
 *  as flush overhead is recorded in upper level cycles
 */
#ifndef CHASING_RECORD_TIMING
#define CHASING_RECORD_TIMING 1
#endif
#else
#endif

#ifndef CHASING_RECORD_TIMING
#define CHASING_RECORD_TIMING 1
#endif

#if CHASING_RECORD_TIMING == 0
#define CHASING_RECORD_TIMING_TYPE				"none"
#define CHASING_PER_REPEAT_TIMING_BEG(TIMING_BUF)		"\n"
#define CHASING_PER_REPEAT_TIMING_END(TIMING_BUF)		"\n"
#define CHASING_PER_BLOCK_TIMING_BEG(TIMING_BUF)		"\n"
#define CHASING_PER_BLOCK_TIMING_END(TIMING_BUF)		"\n"
#define TIMING_BUF_INIT(timing_buf)                                            \
	do {                                                                   \
	} while (0);
#define CHASING_PRINT_RECORD_TIMING_BASE(prefix, timing_buf, cnt, base)        \
	do {                                                                   \
	} while (0);
#define CHASING_PRINT_RECORD_TIMING(prefix, timing_buf)                        \
	CHASING_PRINT_RECORD_TIMING_BASE(prefix, timing_buf, 1, 0)
#elif CHASING_RECORD_TIMING == 1
#define CHASING_RECORD_TIMING_TYPE				"per_repeat"
#define CHASING_PER_REPEAT_TIMING_BEG(TIMING_BUF)			       \
	"push	%%rax\n"						       \
	"push	%%rcx\n"						       \
	"push	%%rdx\n"						       \
	"rdtscp\n"							       \
	"lfence\n"							       \
	"shl	$32, %%rdx\n"						       \
	"or	%%rdx, %%rax\n"						       \
	"mov	%%rax, (" #TIMING_BUF ")\n"				       \
	"pop	%%rdx\n"						       \
	"pop	%%rcx\n"						       \
	"pop	%%rax\n"						       \
	"mfence\n"

#define CHASING_PER_REPEAT_TIMING_END(TIMING_BUF)			       \
	"push	%%rax\n"						       \
	"push	%%rcx\n"						       \
	"push	%%rdx\n"						       \
	"rdtscp\n"							       \
	"lfence\n"							       \
	"shl	$32, %%rdx\n"						       \
	"or	%%rdx, %%rax\n"				/* rax: end */	       \
	"mov	(" #TIMING_BUF "), %%rdx\n"		/* rdx: beg */	       \
	"sub	%%rdx, %%rax\n"				/* rax: rax-rdx */     \
	"add	%%rax, (" #TIMING_BUF ", %%r13, 8)\n"		               \
	"pop	%%rdx\n"						       \
	"pop	%%rcx\n"						       \
	"pop	%%rax\n"						       \
	"mfence\n"

#define CHASING_PER_BLOCK_TIMING_BEG(TIMING_BUF)		"\n"
#define CHASING_PER_BLOCK_TIMING_END(TIMING_BUF)		"\n"

#define TIMING_BUF_INIT(timing_buf)                                            \
	for (ti = 0; ti < repeat * 4; ti++) {                                  \
		timing_buf[ti] = 0;                                            \
	}

#define CHASING_PRINT_RECORD_TIMING_BASE(prefix, timing_buf, cnt, base)        \
	for (ti = 0; ti < cnt; ti++) {                                         \
		kr_info(", %s_%lu=%llu", prefix, (unsigned long)(ti + base), timing_buf[ti + 1]);\
	}
#define CHASING_PRINT_RECORD_TIMING(prefix, timing_buf)                        \
	CHASING_PRINT_RECORD_TIMING_BASE(prefix, timing_buf, repeat, 0)
#else
#define CHASING_RECORD_TIMING_TYPE				"per_block"

#define CHASING_PER_REPEAT_TIMING_BEG(TIMING_BUF)		"\n"
#define CHASING_PER_REPEAT_TIMING_END(TIMING_BUF)		"\n"

#define CHASING_PER_BLOCK_TIMING_BEG(TIMING_BUF)			       \
	"push	%%rax\n"						       \
	"push	%%rcx\n"						       \
	"push	%%rdx\n"						       \
	"rdtscp\n"							       \
	"lfence\n"							       \
	"shl	$32, %%rdx\n"						       \
	"or	%%rdx, %%rax\n"						       \
	"mov	%%rax, (" #TIMING_BUF ")\n"				       \
	"pop	%%rdx\n"						       \
	"pop	%%rcx\n"						       \
	"pop	%%rax\n"						       \
	"mfence\n"

/* offset: 1 + repeat * accesssize + curr_accessize / 64 */
#define CHASING_PER_BLOCK_TIMING_END(TIMING_BUF)			       \
	"push	%%rax\n"						       \
	"push	%%rcx\n"						       \
	"push	%%rdx\n"						       \
	"rdtscp\n"							       \
	"lfence\n"							       \
	"shl	$32, %%rdx\n"						       \
	"or	%%rdx, %%rax\n"				/* rax: end */	       \
	"mov	(" #TIMING_BUF "), %%rdx\n"		/* rdx: beg */	       \
	"sub	%%rdx, %%rax\n"				/* rax: rax-rdx */     \
	"mov	%%r10, %%rcx\n"		/* rcx: curr_accesssize */	       \
	"mov	%%r13, %%rdx\n"		/* rdx: repeat */	 	       \
	"shr	$8, %%rcx\n"		/* curr_accessize / 64 */	       \
	"imul	%[accesssize], %%rdx\n" /* repeat * accesssize */	       \
	"add	%%rcx, %%rdx\n"						       \
	"add	$1, %%rdx\n"						       \
	"add	%%rax, (" #TIMING_BUF ", %%rdx, 8)\n"			       \
	"pop	%%rdx\n"						       \
	"pop	%%rcx\n"						       \
	"pop	%%rax\n"						       \
	"mfence\n"

#define TIMING_BUF_INIT(timing_buf)                                            \
	for (ti = 0; ti < 2 + (region_size / block_size) * repeat; ti++) {     \
		timing_buf[ti] = 0;                                            \
	}

#define CHASING_PRINT_RECORD_TIMING_BASE(prefix, timing_buf, cnt, base)        \
	for (ti = 0; ti < cnt; ti++) {                                         \
		for (tj = 0; tj < (region_size / block_size); tj++) {          \
			pr_info(", %s_%u_%u=%llu", prefix, ti, tj,             \
				timing_buf[ti + 1]);                           \
		}                                                              \
	}
#define CHASING_PRINT_RECORD_TIMING(prefix, timing_buf)                        \
	CHASING_PRINT_RECORD_TIMING_BASE(prefix, timing_buf, repeat, 0)
#endif


/* 
 * Macros to generate memory access instructions.
 * `../scripts/code/expand_macro.sh chasing.h` to view macro expansion results.
 */
#define CHASING_ST_64_AVX(cl_index)                                            \
	"vmovntdq	%%zmm0, (64 * (" #cl_index "))(%%r9, %%r12)\n"         \
	CHASING_STORE_FLUSH(cl_index)

#define CHASING_ST_64(cl_base)                                                 \
	CHASING_ST_64_AVX(cl_base)

#define CHASING_ST_128(cl_base)                                                \
	CHASING_ST_64(cl_base + 0)                                             \
	CHASING_ST_64(cl_base + 1)

#define CHASING_ST_256(cl_base)                                                \
	CHASING_ST_128(cl_base + 0)                                            \
	CHASING_ST_128(cl_base + 2)

#define CHASING_ST_512(cl_base)                                                \
	CHASING_ST_256(cl_base + 0)                                            \
	CHASING_ST_256(cl_base + 4)

#define CHASING_ST_1024(cl_base)                                               \
	CHASING_ST_512(cl_base + 0)                                            \
	CHASING_ST_512(cl_base + 8)

#define CHASING_ST_2048(cl_base)                                               \
	CHASING_ST_1024(cl_base + 0)                                           \
	CHASING_ST_1024(cl_base + 16)

#define CHASING_ST_4096(cl_base)                                               \
	CHASING_ST_2048(cl_base + 0)                                           \
	CHASING_ST_2048(cl_base + 32)

#define CHASING_LD_64_AVX(cl_index)                                            \
	"vmovntdqa	(64 * (" #cl_index "))(%%r9, %%r12), %%zmm0\n"	       \
	CHASING_LOAD_FLUSH(cl_index)

#define CHASING_LD_8_REG(cl_index)                                             \
	"movntdqa       (64 * (" #cl_index "))(%%r9, %%r12), %%r13\n"	       \
	CHASING_LOAD_FLUSH(cl_index)

#define CHASING_LD_8(cl_base)                                                  \
	CHASING_LD_8_REG(cl_base)

#define CHASING_LD_64(cl_base)                                                 \
	CHASING_LD_64_AVX(cl_base)

#define CHASING_LD_128(cl_base)                                                \
	CHASING_LD_64(cl_base + 0)                                             \
	CHASING_LD_64(cl_base + 1)

#define CHASING_LD_256(cl_base)                                                \
	CHASING_LD_128(cl_base + 0)                                            \
	CHASING_LD_128(cl_base + 2)

#define CHASING_LD_512(cl_base)                                                \
	CHASING_LD_256(cl_base + 0)                                            \
	CHASING_LD_256(cl_base + 4)

#define CHASING_LD_1024(cl_base)                                               \
	CHASING_LD_512(cl_base + 0)                                            \
	CHASING_LD_512(cl_base + 8)

#define CHASING_LD_2048(cl_base)                                               \
	CHASING_LD_1024(cl_base + 0)                                           \
	CHASING_LD_1024(cl_base + 16)

#define CHASING_LD_4096(cl_base)                                               \
	CHASING_LD_2048(cl_base + 0)                                           \
	CHASING_LD_2048(cl_base + 32)

typedef void (*chasing_func_t)(char *start_addr,
			       uint64_t size,
			       uint64_t block_skip,
			       uint64_t region_skip,
			       uint64_t count,
			       uint64_t repeat,
			       uint64_t *cindex,
			       uint64_t *timing);

typedef struct chasing_func_entry {
	char *name;
	char *fence_strategy;
	char *fence_freq;
	char *flush_after_load;
	char *flush_after_store;
	char *flush_l1;
	char *flush_l1_flush_after_load;
	char *record_timing;
	uint64_t block_size;
	chasing_func_t ld_func;
	chasing_func_t st_func;
	chasing_func_t raw_func;
} chasing_func_entry_t;

#define _CHASING_ENTRY(prefix, size, func_prefix)                              \
{                                                                              \
	.name                      = #prefix "-" #size,                        \
	.fence_strategy            = CHASING_FENCE_STRATEGY,                   \
	.fence_freq                = CHASING_FENCE_FREQ,                       \
	.flush_after_load          = CHASING_FLUSH_AFTER_LOAD_TYPE,            \
	.flush_after_store          = CHASING_FLUSH_AFTER_STORE_TYPE,          \
	.flush_l1                  = CHASING_FLUSH_L1_TYPE,                    \
	.flush_l1_flush_after_load = CHASING_FLUSH_L1_FLUSH_AFTER_LOAD_TYPE,   \
	.record_timing             = CHASING_RECORD_TIMING_TYPE,               \
	.block_size                = size,                                     \
	.ld_func                   = func_prefix ## _ldnt_             ## size,\
	.st_func                   = func_prefix ## _stnt_             ## size,\
	.raw_func                  = func_prefix ## _read_after_write_ ## size,\
},

#define CHASING_ENTRY(prefix, size)                                            \
	_CHASING_ENTRY(prefix, size, chasing)

#define CHASING_BAF_ENTRY(prefix, size)                                        \
	_CHASING_ENTRY(prefix, size, chasing_baf)

#endif /* CHASING_COMMON_H */
