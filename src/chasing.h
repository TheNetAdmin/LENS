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

#include <linux/kernel.h>

#ifndef KERNEL_BEGIN
#define KERNEL_BEGIN                                                           \
	do {                                                                   \
	} while (0);
#define KERNEL_END                                                             \
	do {                                                                   \
	} while (0);
#endif

#define CHASING_ST_FENCE_TYPE "mfence"
#define CHASING_LD_FENCE_TYPE "mfence"
#define CHASING_MFENCE_TYPE   "mfence"

#define CHASING_ST_FENCE CHASING_ST_FENCE_TYPE "\n"
#define CHASING_LD_FENCE CHASING_LD_FENCE_TYPE "\n"
#define CHASING_MFENCE   CHASING_MFENCE_TYPE   "\n"


/* 
 * Macros to generate memory access instructions.
 * Use `gcc -E chasing.h` to view macro expansion results.
 * You may use `clang-format` to format the gcc results.
 */
#define CHASING_ST_64_INNER(cl_index)                                          \
	"vmovntdq	%%zmm0, (64 * (" #cl_index "))(%%r9, %%r12)\n"

#define CHASING_ST_64(cl_base) CHASING_ST_64_INNER(cl_base)

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

#define CHASING_ST(PCBLOCK_SIZE, PCBLOCK_SHIFT)                                \
static void chasing_stnt_##PCBLOCK_SIZE(char *start_addr, long size,           \
					long skip, long count,                 \
					uint64_t *cindex)                      \
{                                                                              \
	KERNEL_BEGIN                                                           \
	asm volatile(                                                          \
		"xor    %%r8, %%r8 \n"                 /* r8: access offset  */\
		"xor    %%r11, %%r11 \n"               /* r11: counter       */\
		                                                               \
	"LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_OUTER: \n"                       \
		"lea    (%[start_addr], %%r8), %%r9 \n"/* r9: access loc     */\
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
		                                                               \
	"LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_INNER: \n"                       \
		"vmovq  (%[cindex], %%r12, 8), %%xmm0\n"                       \
		"shl	$" #PCBLOCK_SHIFT ", %%r12\n"                          \
		CHASING_ST_##PCBLOCK_SIZE(0)                                   \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		"movq	%%xmm0, %%r12\n"                                       \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_INNER \n"         \
		CHASING_ST_FENCE                                               \
								               \
		"add    %[skip], %%r8 \n"                                      \
		"inc    %%r11 \n"                                              \
		"cmp    %[count], %%r11 \n"                                    \
		"jl     LOOP_CHASING_ST_NT_" #PCBLOCK_SIZE "_OUTER \n"         \
								               \
		:                                                              \
		: [start_addr] "r"(start_addr),                                \
		  [accesssize] "r"(size), [count] "r"(count),                  \
		  [skip] "r"(skip), [cindex] "r"(cindex)                       \
		: "%r12", "%r11", "%r10", "%r9", "%r8"                         \
	);                                                                     \
	KERNEL_END                                                             \
};

#define CHASING_LD_64_INNER(cl_index)                                          \
	"vmovntdqa	(64 * (" #cl_index "))(%%r9, %%r12), %%zmm0\n"

#define CHASING_LD_64(cl_base)                                                 \
	CHASING_LD_64_INNER(cl_base)

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

#define CHASING_LD(PCBLOCK_SIZE, PCBLOCK_SHIFT)                                \
static void chasing_ldnt_##PCBLOCK_SIZE(char *start_addr, long size,           \
					long skip, long count,                 \
					uint64_t *cindex)                      \
{                                                                              \
	KERNEL_BEGIN                                                           \
	asm volatile(                                                          \
		"xor    %%r8, %%r8 \n"                 /* r8: access offset  */\
		"xor    %%r11, %%r11 \n"               /* r11: counter       */\
		                                                               \
	"LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_OUTER: \n"                       \
		"lea    (%[start_addr], %%r8), %%r9 \n"/* r9: access loc     */\
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
		                                                               \
	"LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_INNER: \n"                       \
		"shl	$" #PCBLOCK_SHIFT ", %%r12\n"                          \
		CHASING_LD_##PCBLOCK_SIZE(0)                                   \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		"movq	%%xmm0, %%r12\n"                                       \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_INNER \n"         \
		CHASING_LD_FENCE                                               \
								               \
		"add    %[skip], %%r8 \n"                                      \
		"inc    %%r11 \n"                                              \
		"cmp    %[count], %%r11 \n"                                    \
								               \
		"jl     LOOP_CHASING_LD_NT_" #PCBLOCK_SIZE "_OUTER \n"         \
								               \
		:                                                              \
		: [start_addr] "r"(start_addr),                                \
		  [accesssize] "r"(size), [count] "r"(count),                  \
		  [skip] "r"(skip), [cindex] "r"(cindex)                       \
		: "%r12", "%r11", "%r10", "%r9", "%r8"                         \
	);                                                                     \
	KERNEL_END                                                             \
}

/* Store-Load Non-Overlapping */
#define CHASING_RDTSCP                                                         \
	"rdtscp\n"                                                             \
	"lfence\n"                                                             \
	"shl	$32, %%rdx\n"                                                  \
	"or	%%rax, %%rdx\n"                                                \
	"mov	%%rdx, %%rax\n"

#define CHASING_SLNO_BEF                                                       \
	CHASING_RDTSCP                                                         \
	"mov	%%rdx, %%r13"

#define CHASING_SLNO_MED                                                       \
	CHASING_ST_FENCE                                                       \
	CHASING_RDTSCP                                                         \
	"sub    %%r13, %%rax\n"                                                \
	"add    %%rax, %[cycle_st]\n"                                          \
	"mov    %%rdx, %%r13\n"

#define CHASING_SLNO_AFT                                                       \
	CHASING_LD_FENCE                                                       \
	CHASING_RDTSCP                                                         \
	"sub    %%r13, %%rax\n"                                                \
	"add    %%rax, %[cycle_ld]\n"                                          \
	"mov    %%rdx, %%r13\n"

#define CHASING_SLNO_64(cl_base)                                               \
	CHASING_SLNO_BEF                                                       \
	CHASING_ST_64(cl_base + 0)                                             \
	CHASING_SLNO_MED                                                       \
	CHASING_LD_64(cl_base + 1)                                             \
	CHASING_SLNO_AFT

#define CHASING_SLNO_128(cl_base)                                              \
	CHASING_SLNO_BEF                                                       \
	CHASING_ST_128(cl_base + 0)                                            \
	CHASING_SLNO_MED                                                       \
	CHASING_LD_128(cl_base + 2)                                            \
	CHASING_SLNO_AFT

#define CHASING_SLNO_256(cl_base)                                              \
	CHASING_SLNO_BEF                                                       \
	CHASING_ST_256(cl_base + 0)                                            \
	CHASING_SLNO_MED                                                       \
	CHASING_LD_256(cl_base + 4)                                            \
	CHASING_SLNO_AFT

#define CHASING_SLNO_512(cl_base)                                              \
	CHASING_SLNO_BEF                                                       \
	CHASING_ST_512(cl_base + 0)                                            \
	CHASING_SLNO_MED                                                       \
	CHASING_LD_512(cl_base + 8)                                            \
	CHASING_SLNO_AFT

#define CHASING_SLNO_1024(cl_base)                                             \
	CHASING_SLNO_BEF                                                       \
	CHASING_ST_1024(cl_base + 0)                                           \
	CHASING_SLNO_MED                                                       \
	CHASING_LD_1024(cl_base + 16)                                          \
	CHASING_SLNO_AFT

#define CHASING_SLNO_2048(cl_base)                                             \
	CHASING_SLNO_BEF                                                       \
	CHASING_ST_2048(cl_base + 0)                                           \
	CHASING_SLNO_MED                                                       \
	CHASING_LD_2048(cl_base + 32)                                          \
	CHASING_SLNO_AFT

#define CHASING_SLNO_4096(cl_base)                                             \
	CHASING_SLNO_BEF                                                       \
	CHASING_ST_4096(cl_base + 0)                                           \
	CHASING_SLNO_MED                                                       \
	CHASING_LD_4096(cl_base + 32)                                          \
	CHASING_SLNO_AFT

#define CHASING_SLNO(PCBLOCK_SIZE, PCBLOCK_SHIFT)                              \
static void chasing_slno_##PCBLOCK_SIZE(char *start_addr, long size,           \
					long skip, long count,                 \
					uint64_t *cindex,                      \
					uint64_t *cycles)                      \
{                                                                              \
	KERNEL_BEGIN                                                           \
	uint64_t cycle_st = 0;                                                 \
	uint64_t cycle_ld = 0;                                                 \
	asm volatile(                                                          \
		"xor    %%r8, %%r8 \n"                 /* r8: access offset  */\
		"xor    %%r11, %%r11 \n"               /* r11: counter       */\
		                                                               \
	"LOOP_CHASING_SLNO_" #PCBLOCK_SIZE "_OUTER: \n"                        \
		"lea    (%[start_addr], %%r8), %%r9 \n"/* r9: access loc     */\
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
		                                                               \
	"LOOP_CHASING_SLNO_" #PCBLOCK_SIZE "_INNER: \n"                        \
		"vmovq  (%[cindex], %%r12, 8), %%xmm0\n"                       \
		"shl	$" #PCBLOCK_SHIFT ", %%r12\n"                          \
		CHASING_SLNO_##PCBLOCK_SIZE(0)                                 \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		"movq	%%xmm0, %%r12\n"                                       \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_SLNO_" #PCBLOCK_SIZE "_INNER \n"          \
		CHASING_LD_FENCE                                               \
								               \
		"add    %[skip], %%r8 \n"                                      \
		"inc    %%r11 \n"                                              \
		"cmp    %[count], %%r11 \n"                                    \
								               \
		"jl     LOOP_CHASING_SLNO_" #PCBLOCK_SIZE "_OUTER \n"          \
								               \
		: [cycle_st] "+r"(cycle_st)[cycle_ld] "+r"(cycle_ld)           \
		: [start_addr] "r"(start_addr),                                \
		  [accesssize] "r"(size), [count] "r"(count),                  \
		  [skip] "r"(skip), [cindex] "r"(cindex)                       \
		: "%rax", "%rdx", "%r13", "%r12", "%r11", "%r10","%r9", "%r8"  \
	);                                                                     \
	KERNEL_END                                                             \
}

#define CHASING_READ_AFTER_WRITE(PCBLOCK_SIZE, PCBLOCK_SHIFT)                  \
static void chasing_read_after_write_##PCBLOCK_SIZE(char *start_addr,          \
						    long size,                 \
						    long skip,                 \
						    long count,	               \
						    uint64_t *cindex)          \
{                                                                              \
	KERNEL_BEGIN                                                           \
	asm volatile(                                                          \
		"xor    %%r8, %%r8 \n"                 /* r8: access offset  */\
		"xor    %%r11, %%r11 \n"               /* r11: counter       */\
		                                                               \
	"LOOP_CHASING_RAW_NT_" #PCBLOCK_SIZE "_OUTER: \n"                      \
		"lea    (%[start_addr], %%r8), %%r9 \n"/* r9: access loc     */\
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
		                                                               \
	"LOOP_CHASING_RAW_NT_WRITE_" #PCBLOCK_SIZE "_INNER: \n"                \
		"vmovq  (%[cindex], %%r12, 8), %%xmm0\n"                       \
		"shl	$" #PCBLOCK_SHIFT ", %%r12\n"                          \
		CHASING_ST_##PCBLOCK_SIZE(0)                                   \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		"movq	%%xmm0, %%r12\n"                                       \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_RAW_NT_WRITE_" #PCBLOCK_SIZE "_INNER \n"  \
		CHASING_MFENCE                                                 \
								               \
		"xor    %%r10, %%r10 \n"               /* r10: accessed size */\
		"xor	%%r12, %%r12 \n"               /* r12: chasing index */\
	"LOOP_CHASING_RAW_NT_READ_" #PCBLOCK_SIZE "_INNER: \n"                 \
		"shl	$" #PCBLOCK_SHIFT ", %%r12\n"                          \
		CHASING_LD_##PCBLOCK_SIZE(0)                                   \
		"add	$" #PCBLOCK_SIZE ", %%r10\n"                           \
		"movq	%%xmm0, %%r12\n"                                       \
								               \
		"cmp    %[accesssize], %%r10 \n"                               \
		"jl     LOOP_CHASING_RAW_NT_READ_" #PCBLOCK_SIZE "_INNER \n"   \
		CHASING_MFENCE                                                 \
									       \
		"add    %[skip], %%r8 \n"                                      \
		"inc    %%r11 \n"                                              \
		"cmp    %[count], %%r11 \n"                                    \
								               \
		"jl     LOOP_CHASING_RAW_NT_" #PCBLOCK_SIZE "_OUTER \n"        \
								               \
		:                                                              \
		: [start_addr] "r"(start_addr),                                \
		  [accesssize] "r"(size), [count] "r"(count),                  \
		  [skip] "r"(skip), [cindex] "r"(cindex)                       \
		: "%r12", "%r11", "%r10", "%r9", "%r8"                         \
	);                                                                     \
	KERNEL_END                                                             \
}

/* Generate pointer chasing load benchmarks */
CHASING_LD(64,    6);
CHASING_LD(128,   7);
CHASING_LD(256,   8);
CHASING_LD(512,   9);
CHASING_LD(1024, 10);
CHASING_LD(2048, 11);
CHASING_LD(4096, 12);

/* Generate pointer chasing store benchmarks */
CHASING_ST(64,    6);
CHASING_ST(128,   7);
CHASING_ST(256,   8);
CHASING_ST(512,   9);
CHASING_ST(1024, 10);
CHASING_ST(2048, 11);
CHASING_ST(4096, 12);

/* Generate pointer chasing read after write benchmarks */
CHASING_READ_AFTER_WRITE(64,    6);
CHASING_READ_AFTER_WRITE(128,   7);
CHASING_READ_AFTER_WRITE(256,   8);
CHASING_READ_AFTER_WRITE(512,   9);
CHASING_READ_AFTER_WRITE(1024, 10);
CHASING_READ_AFTER_WRITE(2048, 11);
CHASING_READ_AFTER_WRITE(4096, 12);

typedef void(*chasing_func_t)(char *start_addr, long size, long skip,
                              long count, uint64_t *cindex);

typedef struct chasing_func_entry {
	char *name;
	unsigned long block_size;
	chasing_func_t ld_func;
	chasing_func_t st_func;
	chasing_func_t raw_func;
} chasing_func_entry_t;

#define CHASING_ENTRY(size)                                                    \
{                                                                              \
	.name        = "pointer-chasing-" #size,                               \
	.block_size  = size,                                                   \
	.ld_func     = chasing_ldnt_##size,                                    \
	.st_func     = chasing_stnt_##size,                                    \
	.raw_func    = chasing_read_after_write_##size,                        \
},

static chasing_func_entry_t chasing_func_list[] = {
	CHASING_ENTRY(64)   // 0
	CHASING_ENTRY(128)  // 1
	CHASING_ENTRY(256)  // 2
	CHASING_ENTRY(512)  // 3
	CHASING_ENTRY(1024) // 4
	CHASING_ENTRY(2048) // 5
	CHASING_ENTRY(4096) // 6
};

void chasing_print_help(void);
int chasing_find_func(unsigned long block_size);

/* 
 * Generate a chasing array at [cindex] memory address. Each array element is a
 *   8 byte pointer (or pointer chasing block index) used by pointer chasing
 *   benchmarks.
 */
int init_chasing_index(uint64_t *cindex, uint64_t csize);

#endif // CHASING_H
