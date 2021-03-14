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

#include "chasing_common.h"

#define CHASING_ST_CURR_DATA_AVX                                               \
	"vmovq  (%[cindex], %%r12, 8), %%xmm0\n"

#define CHASING_ST_NEXT_ADDR_AVX                                               \
	"movq	%%xmm0, %%r12\n"

#define CHASING_ST_CURR_DATA_64   CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_128  CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_256  CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_512  CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_1024 CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_2048 CHASING_ST_CURR_DATA_AVX
#define CHASING_ST_CURR_DATA_4096 CHASING_ST_CURR_DATA_AVX

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
		: "%zmm0", "%r13", "%r12", "%r11", "%r10", "%r9", "%r8"        \
	);                                                                     \
	KERNEL_END                                                             \
};

#define CHASING_LD_NEXT_ADDR_AVX                                               \
	"movq	%%xmm0, %%r12\n"

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
		: "%zmm0", "%r13", "%r12", "%r11", "%r10", "%r9", "%r8"        \
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
		: "%zmm0", "%r13", "%r12", "%r11", "%r10", "%r9", "%r8"        \
	);                                                                     \
	KERNEL_END                                                             \
}

/* Generate pointer chasing load benchmarks */
CHASING_LD(64	);
CHASING_LD(128	);
CHASING_LD(256	);
CHASING_LD(512	);
CHASING_LD(1024	);
CHASING_LD(2048	);
CHASING_LD(4096	);

/* Generate pointer chasing store benchmarks */
CHASING_ST(64	);
CHASING_ST(128	);
CHASING_ST(256	);
CHASING_ST(512	);
CHASING_ST(1024	);
CHASING_ST(2048	);
CHASING_ST(4096	);

/* Generate pointer chasing read after write benchmarks */
CHASING_READ_AFTER_WRITE(64	);
CHASING_READ_AFTER_WRITE(128	);
CHASING_READ_AFTER_WRITE(256	);
CHASING_READ_AFTER_WRITE(512	);
CHASING_READ_AFTER_WRITE(1024	);
CHASING_READ_AFTER_WRITE(2048	);
CHASING_READ_AFTER_WRITE(4096	);

static chasing_func_entry_t chasing_func_list[] = {
	CHASING_ENTRY(pointer-chasing, 64)   // 0
	CHASING_ENTRY(pointer-chasing, 128)  // 1
	CHASING_ENTRY(pointer-chasing, 256)  // 2
	CHASING_ENTRY(pointer-chasing, 512)  // 3
	CHASING_ENTRY(pointer-chasing, 1024) // 4
	CHASING_ENTRY(pointer-chasing, 2048) // 5
	CHASING_ENTRY(pointer-chasing, 4096) // 6
};

void chasing_print_help(void);
int chasing_find_func(uint64_t block_size);

/* 
 * Generate a chasing array at [cindex] memory address. Each array element is a
 *   8 byte pointer (or pointer chasing block index) used by pointer chasing
 *   benchmarks.
 */
int init_chasing_index(uint64_t *cindex, uint64_t csize);

#endif // CHASING_H
