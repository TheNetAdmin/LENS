/* 
 *  Copyright (c) 2020 Zixuan Wang <zxwang@ucsd.edu>
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

#ifndef CHASING_H
#define CHASING_H

#include "print_info.h"
#include "chasing_config.h"

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


#if CHASING_FLUSH_AFTER_STORE == 0
#define CHASING_FLUSH_AFTER_STORE_TYPE "none"
#define CHASING_STORE_FLUSH(cl_index)					       \
	"\n"
#elif CHASING_FLUSH_AFTER_STORE == 1
#define CHASING_FLUSH_AFTER_STORE_TYPE "clflush"
#define CHASING_STORE_FLUSH(cl_index)					       \
	"clflush	(64 * (" #cl_index "))(%%r9, %%r12)\n"
#else
#define CHASING_FLUSH_AFTER_STORE_TYPE "clflushopt"
#define CHASING_STORE_FLUSH(cl_index)					       \
	"clflushopt	(64 * (" #cl_index "))(%%r9, %%r12)\n"
#endif

/* TIMING_BUF:
 *   0  : temp timing (e.g. beging timing used in latency calc)
 *   1..: latency from round 0..
 */



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
	"movntdqa	(256 *  (	" #block_index "))(%%r9, %%r12), %%xmm0 \n"     \
	"clflush	(256 *  (	" #block_index "))(%%r9, %%r12) \n"

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
	CHASING_PRINT_RECORD_TIMING_BASE(prefix, timing_buf, 0)
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
	"mov	%%rax, (" #TIMING_BUF ", %%r13, 8)\n"		               \
	"pop	%%rdx\n"						       \
	"pop	%%rcx\n"						       \
	"pop	%%rax\n"						       \
	"mfence\n"

#define CHASING_PER_BLOCK_TIMING_BEG(TIMING_BUF)		"\n"
#define CHASING_PER_BLOCK_TIMING_END(TIMING_BUF)		"\n"

#define TIMING_BUF_INIT(timing_buf)                                            \
	do {                                                                   \
		asm volatile("mfence");                                        \
		memset(timing_buf, 0, sizeof(uint64_t) * (ci->repeat * 4));    \
		asm volatile("mfence");                                        \
	} while (0);

#define CHASING_PRINT_RECORD_TIMING_BASE(prefix, timing_buf, cnt, base)        \
	for (ti = 0; ti < cnt; ti++) {                                         \
		kr_info(", %s_%lu=%lu", prefix, ti + base, timing_buf[ti + 1]);\
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
	"movntdq	%%xmm0, (64 * (" #cl_index "))(%%r9, %%r12)\n"         \
	CHASING_STORE_FLUSH(cl_index)

#define CHASING_ST_8_REG(cl_index)                                             \
	"movnt          %%r13, (64 * (" #cl_index "))(%%r9, %%r12)\n"          \
	CHASING_STORE_FLUSH(cl_index)

#define CHASING_ST_8(cl_base)                                                  \
	CHASING_ST_8_REG(cl_base)

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

#define CHASING_ST_CURR_DATA_AVX                                               \
	"movq  (%[cindex], %%r12, 8), %%xmm0\n"

#define CHASING_ST_CURR_DATA_REG                                               \
	"movq   (%[cindex], %%r12, 8), %%r13\n"

#define CHASING_ST_NEXT_ADDR_AVX                                               \
	"movq	%%xmm0, %%r12\n"

#define CHASING_ST_NEXT_ADDR_REG                                               \
	"movq	%%r13, %%r12\n"

#define CHASING_ST_CURR_DATA_8    CHASING_ST_CURR_DATA_REG
#define CHASING_ST_CURR_DATA_64   CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_128  CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_256  CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_512  CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_1024 CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_2048 CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_4096 CHASING_ST_CURR_DATA_AVX

#define CHASING_ST_NEXT_ADDR_8    CHASING_ST_NEXT_ADDR_REG
#define CHASING_ST_NEXT_ADDR_64   CHASING_ST_NEXT_ADDR_AVX
#define CHASING_ST_NEXT_ADDR_128  CHASING_ST_NEXT_ADDR_AVX
#define CHASING_ST_NEXT_ADDR_256  CHASING_ST_NEXT_ADDR_AVX
#define CHASING_ST_NEXT_ADDR_512  CHASING_ST_NEXT_ADDR_AVX
#define CHASING_ST_NEXT_ADDR_1024 CHASING_ST_NEXT_ADDR_AVX
#define CHASING_ST_NEXT_ADDR_2048 CHASING_ST_NEXT_ADDR_AVX
#define CHASING_ST_NEXT_ADDR_4096 CHASING_ST_NEXT_ADDR_AVX

#define CHASING_ST(PCBLOCK_SIZE)                                               \
static void chasing_stnt_##PCBLOCK_SIZE(char *start_addr,                      \
					uint64_t size,                         \
					uint64_t block_skip,                   \
					uint64_t region_skip,                  \
					uint64_t count,                        \
					uint64_t repeat,                       \
					uint64_t *cindex,                      \
					uint64_t *timing)                      \
{                                                                              \
	KERNEL_BEGIN                                                           \
	asm volatile(                                                          \
		"xor    %%r8, %%r8 \n"                 /* r8: access offset  */\
		"xor    %%r11, %%r11 \n"               /* r11: counter       */\
		                                                               \
	"LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_OUTER: \n"                       \
		"lea    (%[start_addr], %%r8), %%r9 \n"/* r9: access loc     */\
		"xor	%%r13, %%r13 \n"               /* r13: repeat count  */\
		                                                               \
	"LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_REPEAT: \n"                      \
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
		                                                               \
		FLUSH_L1(ST, PCBLOCK_SIZE)				       \
		                                                               \
		CHASING_PER_REPEAT_TIMING_BEG(%[timing])                       \
		                                                               \
	"LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_INNER: \n"                       \
		CHASING_PER_BLOCK_TIMING_BEG(%[timing])                        \
		CHASING_ST_CURR_DATA_##PCBLOCK_SIZE                            \
		CHASING_ST_FENCE_BLOCK                                         \
		CHASING_PER_BLOCK_TIMING_END(%[timing])                        \
		"imul   %[block_skip], %%r12\n"                                \
		CHASING_ST_##PCBLOCK_SIZE(0)                                   \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		CHASING_ST_NEXT_ADDR_##PCBLOCK_SIZE                            \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_INNER \n"         \
		CHASING_ST_FENCE_REGION                                        \
								               \
		"inc	%%r13 \n"				               \
		CHASING_PER_REPEAT_TIMING_END(%[timing])                       \
								               \
		"cmp	%[repeat], %%r13 \n"			               \
		"jl     LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_REPEAT \n"        \
								               \
		"add    %[region_skip], %%r8 \n"                               \
		"inc    %%r11 \n"                                              \
		"cmp    %[count], %%r11 \n"                                    \
		"jl     LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_OUTER \n"         \
								               \
		:                                                              \
		: [start_addr]  "r"(start_addr),                               \
		  [accesssize]  "r"(size),                                     \
		  [count]       "r"(count),                                    \
		  [block_skip]  "r"(block_skip),                               \
		  [region_skip] "r"(region_skip),                              \
		  [cindex]      "r"(cindex),                                   \
		  [timing]      "r"(timing),                                   \
		  [repeat]      "r"(repeat)                                    \
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8"        \
	);                                                                     \
	KERNEL_END                                                             \
};

#define CHASING_LD_64_AVX(cl_index)                                            \
	"movntdqa	(64 * (" #cl_index "))(%%r9, %%r12), %%xmm0\n"	       \
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

#define CHASING_LD_NEXT_ADDR_AVX                                               \
	"movq	%%xmm0, %%r12\n"

#define CHASING_LD_NEXT_ADDR_REG                                               \
	"movq	%%r13, %%r12\n"

#define CHASING_LD_NEXT_ADDR_8    CHASING_LD_NEXT_ADDR_REG
#define CHASING_LD_NEXT_ADDR_64   CHASING_LD_NEXT_ADDR_AVX
#define CHASING_LD_NEXT_ADDR_128  CHASING_LD_NEXT_ADDR_AVX
#define CHASING_LD_NEXT_ADDR_256  CHASING_LD_NEXT_ADDR_AVX
#define CHASING_LD_NEXT_ADDR_512  CHASING_LD_NEXT_ADDR_AVX
#define CHASING_LD_NEXT_ADDR_1024 CHASING_LD_NEXT_ADDR_AVX
#define CHASING_LD_NEXT_ADDR_2048 CHASING_LD_NEXT_ADDR_AVX
#define CHASING_LD_NEXT_ADDR_4096 CHASING_LD_NEXT_ADDR_AVX

#define CHASING_LD(PCBLOCK_SIZE)                                               \
static void chasing_ldnt_##PCBLOCK_SIZE(char *start_addr,                      \
					uint64_t size,                         \
					uint64_t block_skip,                   \
					uint64_t region_skip,                  \
					uint64_t count,                        \
					uint64_t repeat,                       \
					uint64_t *cindex,                      \
					uint64_t *timing)                      \
{                                                                              \
	KERNEL_BEGIN                                                           \
	asm volatile(                                                          \
		"xor    %%r8, %%r8 \n"                 /* r8: access offset  */\
		"xor    %%r11, %%r11 \n"               /* r11: counter       */\
		                                                               \
	"LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_OUTER: \n"                       \
		"lea    (%[start_addr], %%r8), %%r9 \n"/* r9: access loc     */\
		"xor	%%r13, %%r13 \n"               /* r13: repeat count  */\
		                                                               \
	"LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_REPEAT: \n"                      \
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
		                                                               \
		FLUSH_L1(LD, PCBLOCK_SIZE)				       \
		                                                               \
		CHASING_PER_REPEAT_TIMING_BEG(%[timing])                       \
		                                                               \
	"LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_INNER: \n"                       \
		"imul   %[block_skip], %%r12\n"                                \
		CHASING_PER_BLOCK_TIMING_BEG(%[timing])                        \
		CHASING_LD_##PCBLOCK_SIZE(0)                                   \
		CHASING_LD_FENCE_BLOCK                                         \
		CHASING_PER_BLOCK_TIMING_END(%[timing])                        \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		CHASING_LD_NEXT_ADDR_##PCBLOCK_SIZE                            \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_INNER \n"         \
		CHASING_LD_FENCE_REGION                                        \
								               \
		"inc	%%r13 \n"				               \
		CHASING_PER_REPEAT_TIMING_END(%[timing])                       \
								               \
		"cmp	%[repeat], %%r13 \n"			               \
		"jl     LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_REPEAT \n"        \
								               \
		"add    %[region_skip], %%r8 \n"                               \
		"inc    %%r11 \n"                                              \
		"cmp    %[count], %%r11 \n"                                    \
								               \
		"jl     LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_OUTER \n"         \
								               \
		:                                                              \
		: [start_addr]  "r"(start_addr),                               \
		  [accesssize]  "r"(size),                                     \
		  [count]       "r"(count),                                    \
		  [block_skip]  "r"(block_skip),                               \
		  [region_skip] "r"(region_skip),                              \
		  [timing]      "r"(timing),                                   \
		  [repeat]      "r"(repeat)                                    \
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8"        \
	);                                                                     \
	KERNEL_END                                                             \
}


#define CHASING_LD_NO_TIMING(PCBLOCK_SIZE)                                     \
static void chasing_ldnt_no_timing_##PCBLOCK_SIZE(char *start_addr,                      \
					uint64_t size,                         \
					uint64_t block_skip,                   \
					uint64_t region_skip,                  \
					uint64_t count,                        \
					uint64_t repeat,                       \
					uint64_t *cindex,                      \
					uint64_t *timing)                      \
{                                                                              \
	KERNEL_BEGIN                                                           \
	asm volatile(                                                          \
		"xor    %%r8, %%r8 \n"                 /* r8: access offset  */\
		"xor    %%r11, %%r11 \n"               /* r11: counter       */\
		                                                               \
	"LOOP_CHASING_LD_NT_NTIM_" #PCBLOCK_SIZE "_OUTER: \n"                       \
		"lea    (%[start_addr], %%r8), %%r9 \n"/* r9: access loc     */\
		"xor	%%r13, %%r13 \n"               /* r13: repeat count  */\
		                                                               \
	"LOOP_CHASING_LD_NT_NTIM_" #PCBLOCK_SIZE "_REPEAT: \n"                      \
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
		                                                               \
		FLUSH_L1(LD, PCBLOCK_SIZE)				       \
		                                                               \
	"LOOP_CHASING_LD_NT_NTIM_" #PCBLOCK_SIZE "_INNER: \n"                       \
		"imul   %[block_skip], %%r12\n"                                \
		CHASING_LD_##PCBLOCK_SIZE(0)                                   \
		CHASING_LD_FENCE_BLOCK                                         \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		CHASING_LD_NEXT_ADDR_##PCBLOCK_SIZE                            \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_LD_NT_NTIM_" #PCBLOCK_SIZE "_INNER \n"         \
		CHASING_LD_FENCE_REGION                                        \
								               \
		"inc	%%r13 \n"				               \
								               \
		"cmp	%[repeat], %%r13 \n"			               \
		"jl     LOOP_CHASING_LD_NT_NTIM_" #PCBLOCK_SIZE "_REPEAT \n"        \
								               \
		"add    %[region_skip], %%r8 \n"                               \
		"inc    %%r11 \n"                                              \
		"cmp    %[count], %%r11 \n"                                    \
								               \
		"jl     LOOP_CHASING_LD_NT_NTIM_" #PCBLOCK_SIZE "_OUTER \n"         \
								               \
		:                                                              \
		: [start_addr]  "r"(start_addr),                               \
		  [accesssize]  "r"(size),                                     \
		  [count]       "r"(count),                                    \
		  [block_skip]  "r"(block_skip),                               \
		  [region_skip] "r"(region_skip),                              \
		  [repeat]      "r"(repeat)                                    \
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8"        \
	);                                                                     \
	KERNEL_END                                                             \
}


#define CHASING_READ_AFTER_WRITE(PCBLOCK_SIZE)                                 \
static void chasing_read_after_write_##PCBLOCK_SIZE(char *start_addr,          \
						    uint64_t size,             \
						    uint64_t block_skip,       \
						    uint64_t region_skip,      \
						    uint64_t count,            \
						    uint64_t repeat,           \
						    uint64_t *cindex,          \
						    uint64_t *timing)          \
{                                                                              \
	KERNEL_BEGIN                                                           \
	asm volatile(                                                          \
		"xor    %%r8, %%r8 \n"                 /* r8: access offset  */\
		"xor    %%r11, %%r11 \n"               /* r11: counter       */\
		                                                               \
	"LOOP_CHASING_RAW_NT_" #PCBLOCK_SIZE "_OUTER: \n"                      \
		"lea    (%[start_addr], %%r8), %%r9 \n"/* r9: access loc     */\
		"xor	%%r13, %%r13 \n"               /* r13: repeat count  */\
		                                                               \
	"LOOP_CHASING_RAW_NT_" #PCBLOCK_SIZE "_REPEAT: \n"                     \
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
		                                                               \
		FLUSH_L1(RAW, PCBLOCK_SIZE)				       \
		                                                               \
		CHASING_PER_REPEAT_TIMING_BEG(%[timing])                       \
		                                                               \
	"LOOP_CHASING_RAW_NT_WRITE_" #PCBLOCK_SIZE "_INNER: \n"                \
		CHASING_ST_CURR_DATA_##PCBLOCK_SIZE                            \
		CHASING_ST_FENCE_BLOCK                                         \
		"imul   %[block_skip], %%r12\n"                                \
		CHASING_ST_##PCBLOCK_SIZE(0)                                   \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		CHASING_ST_NEXT_ADDR_##PCBLOCK_SIZE                            \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_RAW_NT_WRITE_" #PCBLOCK_SIZE "_INNER \n"  \
		CHASING_ST_FENCE_REGION                                        \
								               \
		CHASING_MFENCE					               \
								               \
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
	"LOOP_CHASING_RAW_NT_READ_" #PCBLOCK_SIZE "_INNER: \n"                 \
		"imul   %[block_skip], %%r12\n"                                \
		CHASING_LD_##PCBLOCK_SIZE(0)                                   \
		CHASING_LD_FENCE_BLOCK                                         \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		CHASING_LD_NEXT_ADDR_##PCBLOCK_SIZE                            \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_RAW_NT_READ_" #PCBLOCK_SIZE "_INNER \n"   \
		CHASING_LD_FENCE_REGION                                        \
									       \
		"inc	%%r13 \n"				               \
		CHASING_PER_REPEAT_TIMING_END(%[timing])                       \
									       \
		"cmp	%[repeat], %%r13 \n"			               \
		"jl     LOOP_CHASING_RAW_NT_" #PCBLOCK_SIZE "_REPEAT \n"       \
									       \
		"add    %[region_skip], %%r8 \n"                               \
		"inc    %%r11 \n"                                              \
		"cmp    %[count], %%r11 \n"                                    \
								               \
		"jl     LOOP_CHASING_RAW_NT_" #PCBLOCK_SIZE "_OUTER \n"        \
								               \
		:                                                              \
		: [start_addr]  "r"(start_addr),                               \
		  [accesssize]  "r"(size),                                     \
		  [count]       "r"(count),                                    \
		  [block_skip]  "r"(block_skip),                               \
		  [region_skip] "r"(region_skip),                              \
		  [cindex]      "r"(cindex),                                   \
		  [timing]      "r"(timing),                                   \
		  [repeat]      "r"(repeat)                                    \
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8"        \
	);                                                                     \
	KERNEL_END                                                             \
}

/* Generate pointer chasing load benchmarks */
// CHASING_LD(8	);
CHASING_LD(64	);
CHASING_LD(128	);
CHASING_LD(256	);
CHASING_LD(512	);
CHASING_LD(1024	);
CHASING_LD(2048	);
CHASING_LD(4096	);


/* Generate pointer chasing load benchmarks */
// CHASING_LD(8	);
CHASING_LD_NO_TIMING(64		);
CHASING_LD_NO_TIMING(128	);
CHASING_LD_NO_TIMING(256	);
CHASING_LD_NO_TIMING(512	);
CHASING_LD_NO_TIMING(1024	);
CHASING_LD_NO_TIMING(2048	);
CHASING_LD_NO_TIMING(4096	);

/* Generate pointer chasing store benchmarks */
// CHASING_ST(8	);
CHASING_ST(64	);
CHASING_ST(128	);
CHASING_ST(256	);
CHASING_ST(512	);
CHASING_ST(1024	);
CHASING_ST(2048	);
CHASING_ST(4096	);

/* Generate pointer chasing read after write benchmarks */
// CHASING_READ_AFTER_WRITE(8	);
CHASING_READ_AFTER_WRITE(64	);
CHASING_READ_AFTER_WRITE(128	);
CHASING_READ_AFTER_WRITE(256	);
CHASING_READ_AFTER_WRITE(512	);
CHASING_READ_AFTER_WRITE(1024	);
CHASING_READ_AFTER_WRITE(2048	);
CHASING_READ_AFTER_WRITE(4096	);

typedef void (*chasing_func_t)(char *start_addr,
			       uint64_t size,
			       uint64_t block_skip,
			       uint64_t region_skip,
			       uint64_t count,
			       uint64_t repeat,
			       uint64_t *cindex,
			       uint64_t *timing);

typedef struct chasing_func_entry {
	const char *name;
	const char *fence_strategy;
	const char *fence_freq;
	const char *flush_after_load;
	const char *flush_after_store;
	const char *flush_l1;
	const char *record_timing;
	uint64_t block_size;
	chasing_func_t ld_func;
	chasing_func_t ld_no_timing_func;
	chasing_func_t st_func;
	chasing_func_t raw_func;
} chasing_func_entry_t;

#define CHASING_ENTRY(size)                                                    \
{                                                                              \
	.name             = "pointer-chasing-" #size,                          \
	.fence_strategy   = CHASING_FENCE_STRATEGY,                            \
	.fence_freq       = CHASING_FENCE_FREQ,                                \
	.flush_after_load = CHASING_FLUSH_AFTER_LOAD_TYPE,                     \
	.flush_after_store= CHASING_FLUSH_AFTER_STORE_TYPE,                    \
	.flush_l1         = CHASING_FLUSH_L1_TYPE,                             \
	.record_timing    = CHASING_RECORD_TIMING_TYPE,                        \
	.block_size       = size,                                              \
	.ld_func          = chasing_ldnt_##size,                               \
	.st_func          = chasing_stnt_##size,                               \
	.ld_no_timing_func          = chasing_ldnt_no_timing_##size,                               \
	.raw_func         = chasing_read_after_write_##size,                   \
},

static chasing_func_entry_t chasing_func_list[] = {
	CHASING_ENTRY(64)   // 0
	CHASING_ENTRY(128)  // 1
	CHASING_ENTRY(256)  // 2
	CHASING_ENTRY(512)  // 3
	CHASING_ENTRY(1024) // 4
	CHASING_ENTRY(2048) // 5
	CHASING_ENTRY(4096) // 6
	// CHASING_ENTRY(8)    // 7
};

void chasing_print_help(void);
int chasing_find_func(uint64_t block_size);

/* 
 * Generate a chasing array at [cindex] memory address. Each array element is a
 *   8 byte pointer (or pointer chasing block index) used by pointer chasing
 *   benchmarks.
 */
int init_chasing_index(uint64_t *cindex, uint64_t csize);
void print_chasing_index(uint64_t *cindex, uint64_t csize);

#endif // CHASING_H
