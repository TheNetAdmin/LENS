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

#ifndef __KERNEL__
#include <stdint.h>
#endif

#include "lat.h"
#include "memaccess.h"
#include <linux/string.h>

extern uint32_t *lfs_random_array;

/* Arbitrary sizes w/o clearing pipeline */

// #ifdef __KERNEL__
#ifndef KERNEL_BEGIN
	#define KERNEL_BEGIN do { } while (0);
	#define KERNEL_END do { } while (0);
#endif

// rax(ret), rcx(4), rdx(3), rsi(2), rdi(1), r8(5), r9(6), r10, r11 usable
// r12, r13, r14, r15, rbx, rsp, rbp are callee-saved (need push/pop)


#define SIZEBTNT_MACRO SIZEBTNT_512_AVX512
#define SIZEBTST_MACRO SIZEBTST_512_AVX512
#define SIZEBTLD_MACRO SIZEBTLD_512_AVX512
#define SIZEBTSTFLUSH_MACRO SIZEBTSTFLUSH_512_AVX512

#define CACHEFENCE_FENCE	"sfence \n"
//#define CACHEFENCE_FENCE	"mfence \n"

/* Non-ASM implmentation of LFSR */

#define TAP(a) (((a) == 0) ? 0 : ((1ull) << (((uint64_t)(a)) - (1ull))))

#define RAND_LFSR_DECL(BITS, T1, T2, T3, T4)                                   \
	inline static uint##BITS##_t RandLFSR##BITS(uint##BITS##_t *seed)      \
	{                                                                      \
		const uint##BITS##_t mask =                                    \
			TAP(T1) | TAP(T2) | TAP(T3) | TAP(T4);                 \
		*seed = (*seed >> 1) ^                                         \
			(uint##BITS##_t)(-(*seed & (uint##BITS##_t)(1)) &      \
					 mask);                                \
		return *seed;                                                  \
	}

RAND_LFSR_DECL(64, 64, 63, 61, 60);


#define RandLFSR64                                                             \
	"mov    (%[random]), %%r9 \n"                                          \
	"mov    %%r9, %%r12 \n"                                                \
	"shr    %%r9 \n"                                                       \
	"and    $0x1, %%r12d \n"                                               \
	"neg    %%r12 \n"                                                      \
	"and    %%rcx, %%r12 \n"                                               \
	"xor    %%r9, %%r12 \n"                                                \
	"mov    %%r12, (%[random]) \n"                                         \
	"mov    %%r12, %%r8 \n"                                                \
	"and    %[accessmask], %%r8 \n"


void sizebw_load(char *start_addr, long size, long count, long *rand_seed, long access_mask)
{
	KERNEL_BEGIN
	asm volatile (
		"movabs $0xd800000000000000, %%rcx \n"	/* rcx: bitmask used in LFSR */
		"xor %%r8, %%r8 \n"			/* r8: access offset */
		"xor %%r11, %%r11 \n"			/* r11: access counter */
	// 1
	"LOOP_FRNG_SIZEBWL_RLOOP: \n"			/* outer (counter) loop */
		RandLFSR64				/* LFSR: uses r9, r12 (reusable), rcx (above), fill r8 */
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"			/* r10: accessed size */
	"LOOP_FRNG_SIZEBWL_ONE1: \n"			/* inner (access) loop, unroll 8 times */
		SIZEBTLD_MACRO				/* Access: uses r8[rand_base], r10[size_accessed], r9 */
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWL_ONE1 \n"
		SIZEBTLD_FENCE

	// 2
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWL_ONE2: \n"
		SIZEBTLD_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWL_ONE2 \n"
		SIZEBTLD_FENCE
	// 3
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWL_ONE3: \n"
		SIZEBTLD_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWL_ONE3 \n"
		SIZEBTLD_FENCE
	// 4
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWL_ONE4: \n"
		SIZEBTLD_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWL_ONE4 \n"
		SIZEBTLD_FENCE

		"add $4, %%r11 \n"
		"cmp %[count], %%r11\n"
		"jl LOOP_FRNG_SIZEBWL_RLOOP \n"

		: [random]"=r"(rand_seed)
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), "0"(rand_seed),
		  [accessmask]"r"(access_mask)
		: "%rcx", "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void sizebw_nt(char *start_addr, long size, long count, long *rand_seed, long access_mask)
{
	KERNEL_BEGIN
	asm volatile (
		"movabs $0xd800000000000000, %%rcx \n"
		"xor %%r11, %%r11 \n"
		"movq %[start_addr], %%xmm0 \n"		/* zmm0: read/write register */
	// 1
	"LOOP_FRNG_SIZEBWNT_RLOOP: \n"
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWNT_ONE1: \n"
		SIZEBTNT_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWNT_ONE1 \n"
		SIZEBTST_FENCE

	// 2
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWNT_ONE2: \n"
		SIZEBTNT_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWNT_ONE2 \n"
		SIZEBTST_FENCE
	// 3
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWNT_ONE3: \n"
		SIZEBTNT_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWNT_ONE3 \n"
		SIZEBTST_FENCE
	// 4
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWNT_ONE4: \n"
		SIZEBTNT_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWNT_ONE4 \n"
		SIZEBTST_FENCE

		"add $4, %%r11 \n"
		"cmp %[count], %%r11\n"
		"jl LOOP_FRNG_SIZEBWNT_RLOOP \n"

		: [random]"=r"(rand_seed)
		: [start_addr]"r"(start_addr), [accesssize]"r"(size), 
		  [count]"r"(count), "0"(rand_seed),
		  [accessmask]"r"(access_mask)
		: "%rcx", "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void sizebw_store(char *start_addr, long size, long count, long *rand_seed, long access_mask)
{
	KERNEL_BEGIN
	asm volatile (
		"movabs $0xd800000000000000, %%rcx \n"
		"xor %%r11, %%r11 \n"
		"movq %[start_addr], %%xmm0 \n"		/* zmm0: read/write register */
	// 1
	"LOOP_FRNG_SIZEBWST_RLOOP: \n"
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWST_ONE1: \n"
		SIZEBTST_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWST_ONE1 \n"
		SIZEBTST_FENCE

	// 2
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWST_ONE2: \n"
		SIZEBTST_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWST_ONE2 \n"
		SIZEBTST_FENCE
	// 3
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWST_ONE3: \n"
		SIZEBTST_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWST_ONE3 \n"
		SIZEBTST_FENCE
	// 4
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWST_ONE4: \n"
		SIZEBTST_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWST_ONE4 \n"
		SIZEBTST_FENCE

		"add $4, %%r11 \n"
		"cmp %[count], %%r11\n"
		"jl LOOP_FRNG_SIZEBWST_RLOOP \n"

		: [random]"=r"(rand_seed)
		: [start_addr]"r"(start_addr), [accesssize]"r"(size), 
		  [count]"r"(count), "0"(rand_seed),
		  [accessmask]"r"(access_mask)
		: "%rcx", "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void sizebw_storeclwb(char *start_addr, long size, long count, long *rand_seed, long access_mask)
{
	KERNEL_BEGIN
	asm volatile (
		"movabs $0xd800000000000000, %%rcx \n"
		"xor %%r11, %%r11 \n"
		"movq %[start_addr], %%xmm0 \n"		/* zmm0: read/write register */
	// 1
	"LOOP_FRNG_SIZEBWSTFLUSH_RLOOP: \n"
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWSTFLUSH_ONE1: \n"
		SIZEBTSTFLUSH_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWSTFLUSH_ONE1 \n"
		SIZEBTST_FENCE

	// 2
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWSTFLUSH_ONE2: \n"
		SIZEBTSTFLUSH_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWSTFLUSH_ONE2 \n"
		SIZEBTST_FENCE
	// 3
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWSTFLUSH_ONE3: \n"
		SIZEBTSTFLUSH_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWSTFLUSH_ONE3 \n"
		SIZEBTST_FENCE
	// 4
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
	"LOOP_FRNG_SIZEBWSTFLUSH_ONE4: \n"
		SIZEBTSTFLUSH_MACRO
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_FRNG_SIZEBWSTFLUSH_ONE4 \n"
		SIZEBTST_FENCE

		"add $4, %%r11 \n"
		"cmp %[count], %%r11\n"
		"jl LOOP_FRNG_SIZEBWSTFLUSH_RLOOP \n"

		: [random]"=r"(rand_seed)
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), "0"(rand_seed),
		  [accessmask]"r"(access_mask)
		: "%rcx", "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void stride_load(char *start_addr, long size, long skip, long delay, long count)
{
	KERNEL_BEGIN
	asm volatile (
		"xor %%r8, %%r8 \n"			/* r8: access offset */
		"xor %%r11, %%r11 \n"			/* r11: counter */

	// 1
	"LOOP_STRIDELOAD_OUTER: \n"			/* outer (counter) loop */
		"lea (%[start_addr], %%r8), %%r9 \n"	/* r9: access loc */
		"xor %%r10, %%r10 \n"			/* r10: accessed size */
	"LOOP_STRIDELOAD_INNER: \n"			/* inner (access) loop, unroll 8 times */
		SIZEBTLD_64_AVX512			/* Access: uses r10[size_accessed], r9 */
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_STRIDELOAD_INNER \n"
		SIZEBTLD_FENCE

		"xor %%r10, %%r10 \n"
	"LOOP_STRIDELOAD_DELAY: \n"			/* delay <delay> cycles */
		"inc %%r10 \n"
		"cmp %[delay], %%r10 \n"
		"jl LOOP_STRIDELOAD_DELAY \n"

		"add %[skip], %%r8 \n"
		"inc %%r11 \n"
		"cmp %[count], %%r11 \n"

		"jl LOOP_STRIDELOAD_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [delay]"r"(delay)
		: "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void stride_store(char *start_addr, long size, long skip, long delay, long count)
{
	KERNEL_BEGIN
	asm volatile (
		"xor %%r8, %%r8 \n"			/* r8: access offset */
		"xor %%r11, %%r11 \n"			/* r11: counter */
		"movq %[start_addr], %%xmm0 \n"		/* zmm0: read/write register */
	// 1
	"LOOP_STRIDEST_OUTER: \n"			/* outer (counter) loop */
		"lea (%[start_addr], %%r8), %%r9 \n"	/* r9: access loc */
		"xor %%r10, %%r10 \n"			/* r10: accessed size */
	"LOOP_STRIDEST_INNER: \n"			/* inner (access) loop, unroll 8 times */
		SIZEBTST_64_AVX512			/* Access: uses r10[size_accessed], r9 */
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_STRIDEST_INNER \n"
		SIZEBTST_FENCE

		"xor %%r10, %%r10 \n"
	"LOOP_STRIDEST_DELAY: \n"			/* delay <delay> cycles */
		"inc %%r10 \n"
		"cmp %[delay], %%r10 \n"
		"jl LOOP_STRIDEST_DELAY \n"

		"add %[skip], %%r8 \n"
		"inc %%r11 \n"
		"cmp %[count], %%r11 \n"

		"jl LOOP_STRIDEST_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [delay]"r"(delay)
		: "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}


void stride_storeclwb(char *start_addr, long size, long skip, long delay, long count)
{
	KERNEL_BEGIN
	asm volatile (
		"xor %%r8, %%r8 \n"			/* r8: access offset */
		"xor %%r11, %%r11 \n"			/* r11: counter */
		"movq %[start_addr], %%xmm0 \n"		/* zmm0: read/write register */
	// 1
	"LOOP_STRIDESTFLUSH_OUTER: \n"			/* outer (counter) loop */
		"lea (%[start_addr], %%r8), %%r9 \n"	/* r9: access loc */
		"xor %%r10, %%r10 \n"			/* r10: accessed size */
	"LOOP_STRIDESTFLUSH_INNER: \n"			/* inner (access) loop, unroll 8 times */
		SIZEBTSTFLUSH_64_AVX512			/* Access: uses r10[size_accessed], r9 */
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_STRIDESTFLUSH_INNER \n"
		SIZEBTST_FENCE

		"xor %%r10, %%r10 \n"
	"LOOP_STRIDESTFLUSH_DELAY: \n"			/* delay <delay> cycles */
		"inc %%r10 \n"
		"cmp %[delay], %%r10 \n"
		"jl LOOP_STRIDESTFLUSH_DELAY \n"

		"add %[skip], %%r8 \n"
		"inc %%r11 \n"
		"cmp %[count], %%r11 \n"

		"jl LOOP_STRIDESTFLUSH_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [delay]"r"(delay)
		: "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void stride_loadnt(char *start_addr, long size, long skip, long delay, long count)
{
	KERNEL_BEGIN
	asm volatile (
		"xor %%r8, %%r8 \n"				/* r8: access offset */
		"xor %%r11, %%r11 \n"				/* r11: counter */
		"movq %[start_addr], %%xmm0 \n"			/* zmm0: read/write register */
	// 1
	"LOOP_STRIDELDNT_OUTER: \n"				/* outer (counter) loop */
		"lea (%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */
		"xor %%r10, %%r10 \n"				/* r10: accessed size */
	"LOOP_STRIDELDNT_INNER: \n"				/* inner (access) loop, unroll 8 times */
		"vmovntdqa	0x0(%%r9, %%r10), %%zmm0 \n"	/* Access: uses r10[size_accessed], r9 */
		"add	$0x40, %%r10 \n"
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_STRIDELDNT_INNER \n"
		SIZEBTLD_FENCE

		"xor %%r10, %%r10 \n"
	"LOOP_STRIDELDNT_DELAY: \n"				/* delay <delay> cycles */
		"inc %%r10 \n"
		"cmp %[delay], %%r10 \n"
		"jl LOOP_STRIDELDNT_DELAY \n"

		"add %[skip], %%r8 \n"
		"inc %%r11 \n"
		"cmp %[count], %%r11 \n"

		"jl LOOP_STRIDELDNT_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [delay]"r"(delay)
		: "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void stride_storent(char *start_addr, long size, long skip, long delay, long count)
{
	KERNEL_BEGIN
	asm volatile (
		"xor %%r8, %%r8 \n"				/* r8: access offset */
		"xor %%r11, %%r11 \n"				/* r11: counter */
		"movq %[start_addr], %%xmm0 \n"			/* zmm0: read/write register */
	// 1
	"LOOP_STRIDESTNT_OUTER: \n"				/* outer (counter) loop */
		"lea (%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */
		"xor %%r10, %%r10 \n"				/* r10: accessed size */
	"LOOP_STRIDESTNT_INNER: \n"				/* inner (access) loop, unroll 8 times */
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r10) \n"	/* Access: uses r10[size_accessed], r9 */
		"add	$0x40, %%r10 \n"
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_STRIDESTNT_INNER \n"
		SIZEBTST_FENCE

		"xor %%r10, %%r10 \n"
	"LOOP_STRIDESTNT_DELAY: \n"				/* delay <delay> cycles */
		"inc %%r10 \n"
		"cmp %[delay], %%r10 \n"
		"jl LOOP_STRIDESTNT_DELAY \n"

		"add %[skip], %%r8 \n"
		"inc %%r11 \n"
		"cmp %[count], %%r11 \n"

		"jl LOOP_STRIDESTNT_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [delay]"r"(delay)
		: "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void stride_read_after_write(char *start_addr, long size, long skip, long delay, long count)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"					/* r8: access offset */
		"xor	%%r11, %%r11 \n"				/* r11: counter */
		"movq	%[start_addr], %%xmm0 \n"			/* zmm0: read/write register */

	"LOOP_RAW_OUTER: \n"						/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"			/* r9: access loc */

		"xor	%%r10, %%r10 \n"				/* r10: accessed size */
	"LOOP_RAW_STRIDESTNT_INNER: \n"					/* inner (access) loop, unroll 8 times */
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r10) \n"		/* Access: uses r10[size_accessed], r9 */
		"add	$0x40, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_RAW_STRIDESTNT_INNER \n"
		"mfence \n"

		"xor	%%r10, %%r10 \n"
	"LOOP_RAW_STRIDELDNT_INNER: \n"
		"vmovntdqa	0x0(%%r9, %%r10), %%zmm0\n"
		"add	$0x40, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_RAW_STRIDELDNT_INNER \n"
		"mfence \n"

		"xor	%%r10, %%r10 \n"
	"LOOP_RAW_DELAY: \n"						/* delay <delay> cycles */
		"inc	%%r10 \n"
		"cmp	%[delay], %%r10 \n"
		"jl		LOOP_RAW_DELAY \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl		LOOP_RAW_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [delay]"r"(delay)
		: "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void chasing_storent_256(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"				/* r8: access offset */
		"xor	%%r11, %%r11 \n"			/* r11: counter */
	"LOOP_CHASING_STNT_256_OUTER: \n"			/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */
		"xor	%%r10, %%r10 \n"			/* r10: accessed size */
		"xor	%%r12, %%r12 \n"    			/* r12: chasing index addr */
	"LOOP_CHASING_STNT_256_INNER: \n"
		"movq	(%[cindex], %%r12, 8), %%xmm0\n"
		"shl	$0x08, %%r12\n"
		"vmovntdq	%%zmm0, 0x00(%%r9, %%r12)\n"
		"vmovntdq	%%zmm0, 0x40(%%r9, %%r12)\n"
		"vmovntdq	%%zmm0, 0x80(%%r9, %%r12)\n"
		"vmovntdq	%%zmm0, 0xc0(%%r9, %%r12)\n"
		"add	$0x100, %%r10\n"
		"movq	%%xmm0, %%r12\n"			/* Update to next chasing element */

		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_CHASING_STNT_256_INNER \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl		LOOP_CHASING_STNT_256_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size), [count]"r"(count), [skip]"r"(skip), [cindex]"r"(cindex)
		: "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void chasing_loadnt_256(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"				/* r8: access offset */
		"xor	%%r11, %%r11 \n"			/* r11: counter */
	"LOOP_CHASING_LDNT_256_OUTER: \n"			/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */
		"xor	%%r10, %%r10 \n"			/* r10: accessed size */
		"xor	%%r12, %%r12 \n"			/* r12: chasing index addr */
	"LOOP_CHASING_LDNT_256_INNER: \n"
		"shl	$0x08, %%r12\n"
		"vmovntdqa	0x00(%%r9, %%r12), %%zmm0\n"
		"vmovntdqa	0x40(%%r9, %%r12), %%zmm0\n"
		"vmovntdqa	0x80(%%r9, %%r12), %%zmm0\n"
		"vmovntdqa	0xc0(%%r9, %%r12), %%zmm0\n"
		"movq	%%xmm0, %%r12\n"			/* Update to next chasing element */
		"add	$0x100, %%r10 \n"

		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_LDNT_256_INNER \n"
		SIZEBTLD_FENCE

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl	LOOP_CHASING_LDNT_256_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [cindex]"r"(cindex)
		: "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void chasing_combined_stld_256(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"				/* r8: access offset */
		"xor	%%r11, %%r11 \n"			/* r11: counter */
	"LOOP_CHASING_COMB_256_OUTER: \n"			/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */
		"xor	%%r10, %%r10 \n"			/* r10: accessed size */
		"xor	%%r12, %%r12 \n"			/* r12: chasing index addr */
	"LOOP_CHASING_COMB_256_STNT_INNER: \n"
		"movq	(%[cindex], %%r12, 8), %%xmm0\n"
		"shl	$0x08, %%r12\n"
		"vmovntdq	%%zmm0, 0x00(%%r9, %%r12)\n"
		"vmovntdq	%%zmm0, 0x40(%%r9, %%r12)\n"
		"vmovntdq	%%zmm0, 0x80(%%r9, %%r12)\n"
		"vmovntdq	%%zmm0, 0xc0(%%r9, %%r12)\n"
		"add	$0x100, %%r10\n"
		"movq	%%xmm0, %%r12\n"			/* Update to next chasing element */

		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_COMB_256_STNT_INNER \n"
		"mfence \n"

		"xor	%%r10, %%r10 \n"
		"xor	%%r12, %%r12 \n"
	"LOOP_CHASING_COMB_256_LDNT_INNER: \n"
		"shl	$0x08, %%r12\n"
		"vmovntdqa	0x00(%%r9, %%r12), %%zmm0\n"
		"vmovntdqa	0x40(%%r9, %%r12), %%zmm0\n"
		"vmovntdqa	0x80(%%r9, %%r12), %%zmm0\n"
		"vmovntdqa	0xc0(%%r9, %%r12), %%zmm0\n"
		"movq	%%xmm0, %%r12\n"			/* Update to next chasing element */
		"add	$0x100, %%r10 \n"

		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_COMB_256_LDNT_INNER \n"
		"mfence \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl	LOOP_CHASING_COMB_256_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [cindex]"r"(cindex)
		: "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void chasing_storent(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"				/* r8: access offset */
		"xor	%%r11, %%r11 \n"			/* r11: counter */
	"LOOP_CHASING_STNT_OUTER: \n"				/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */
		"xor	%%r10, %%r10 \n"			/* r10: accessed size */
		"xor	%%r12, %%r12 \n"			/* r12: chasing index addr */
	"LOOP_CHASING_STNT_INNER: \n"
		"movq	(%[cindex], %%r12, 8), %%xmm0\n"
		"shl    $0x06, %%r12\n"
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r12)\n"
		"add	$0x40, %%r10\n"
		"movq	%%xmm0, %%r12\n"			/* Update to next chasing element */

		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_STNT_INNER \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl	LOOP_CHASING_STNT_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [cindex]"r"(cindex)
		: "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void chasing_loadnt(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor    %%r8, %%r8 \n"				/* r8: access offset */
		"xor    %%r11, %%r11 \n"			/* r11: counter */
	"LOOP_CHASING_LDNT_OUTER: \n"				/* outer (counter) loop */
		"lea    (%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */
		"xor    %%r10, %%r10 \n"			/* r10: accessed size */
		"xor	%%r12, %%r12 \n"			/* r12: chasing index addr */
	"LOOP_CHASING_LDNT_INNER: \n"
		"shl    $0x06, %%r12\n"
		"vmovntdqa 0x0(%%r9, %%r12), %%zmm0\n"
		"movq   %%xmm0, %%r12\n"			/* Update to next chasing element */
		"add    $0x40, %%r10 \n"

		"cmp    %[accesssize], %%r10 \n"
		"jl     LOOP_CHASING_LDNT_INNER \n"
		SIZEBTLD_FENCE

		"add    %[skip], %%r8 \n"
		"inc    %%r11 \n"
		"cmp    %[count], %%r11 \n"

		"jl     LOOP_CHASING_LDNT_OUTER \n"

		:
        	: [start_addr]"r"(start_addr), [accesssize]"r"(size), 
		  [count]"r"(count), [skip]"r"(skip), [cindex]"r"(cindex)
        	: "r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void chasing_combined_stld(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"				/* r8: access offset */
		"xor	%%r11, %%r11 \n"			/* r11: counter */
	"LOOP_CHASING_COMB_OUTER: \n"				/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */

		"xor	%%r10, %%r10 \n"			/* r10: accessed size */
		"xor	%%r12, %%r12 \n"			/* r12: chasing index addr */
	"LOOP_CHASING_COMB_STNT_INNER: \n"
		"movq	(%[cindex], %%r12, 8), %%xmm0\n"
		"shl    $0x06, %%r12\n"
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r12)\n"
		"add	$0x40, %%r10\n"
		"movq	%%xmm0, %%r12\n"			/* Update to next chasing element */

		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_COMB_STNT_INNER \n"
		"mfence \n"

		"xor	%%r10, %%r10 \n"
		"xor	%%r12, %%r12 \n"
	"LOOP_CHASING_COMB_LDNT_INNER: \n"
		"shl    $0x06, %%r12\n"
		"vmovntdqa 0x0(%%r9, %%r12), %%zmm0\n"
		"movq   %%xmm0, %%r12\n"			/* Update to next chasing element */
		"add    $0x40, %%r10 \n"

		"cmp    %[accesssize], %%r10 \n"
		"jl     LOOP_CHASING_COMB_LDNT_INNER \n"
		"mfence \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl	LOOP_CHASING_COMB_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [cindex]"r"(cindex)
		: "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void chasing_combined_stld_nofence(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"				/* r8: access offset */
		"xor	%%r11, %%r11 \n"			/* r11: counter */
	"LOOP_CHASING_COMB_NF_OUTER: \n"			/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"		/* r9: access loc */

		"xor	%%r10, %%r10 \n"			/* r10: accessed size */
		"xor	%%r12, %%r12 \n"			/* r12: chasing index addr */
	"LOOP_CHASING_COMB_STNT_NF_INNER: \n"
		"movq	(%[cindex], %%r12, 8), %%xmm0\n"
		"shl    $0x06, %%r12\n"
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r12)\n"
		"add	$0x40, %%r10\n"
		"movq	%%xmm0, %%r12\n"			/* Update to next chasing element */

		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_CHASING_COMB_STNT_NF_INNER \n"
		// "mfence \n"

		"xor	%%r10, %%r10 \n"
		"xor	%%r12, %%r12 \n"
	"LOOP_CHASING_COMB_LDNT_NF_INNER: \n"
		"shl    $0x06, %%r12\n"
		"vmovntdqa 0x0(%%r9, %%r12), %%zmm0\n"
		"movq   %%xmm0, %%r12\n"			/* Update to next chasing element */
		"add    $0x40, %%r10 \n"

		"cmp    %[accesssize], %%r10 \n"
		"jl     LOOP_CHASING_COMB_LDNT_NF_INNER \n"
		"mfence \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl		LOOP_CHASING_COMB_NF_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip), [cindex]"r"(cindex)
		: "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

uint64_t chasing_storent_ff(char *start_addr, long size, long skip, long count, uint64_t *cindex, char *flush_addr)
{
	uint64_t total_cycle = 0;
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"						/* r8: access offset */
		"xor	%%r11, %%r11 \n"					/* r11: counter */
	"LOOP_CHASING_STNT_FF_OUTER: \n"					/* outer (counter) loop */

		"mfence \n"

		/* flush rmw buffer by read-after-write 64KB data   */
		"xor	%%r10, %%r10 \n"					/* r10: flushed size */
		"mov    %[flush_addr], %%r12\n"
	"LOOP_CHASING_STNT_FF_FLUSH_BUF_W: \n"
		"vmovntdq	%%zmm0, (%%r12, %%r10) \n"
		"add	$0x40, %%r10 \n"
		"cmp	$0x10000, %%r10 \n"					/* 16KB rmw buf flush */
		"jl	LOOP_CHASING_STNT_FF_FLUSH_BUF_W \n"
		"mfence \n"

		"xor	%%r10, %%r10 \n"					/* r10: flushed size */
		"mov    %[flush_addr], %%r12\n"
	"LOOP_CHASING_STNT_FF_FLUSH_BUF_R: \n"
		"vmovntdqa	(%%r12, %%r10), %%zmm0 \n"
		"add	$0x40, %%r10 \n"
		"cmp	$0x10000, %%r10 \n"					/* 16KB rmw buf flush */
		"jl	LOOP_CHASING_STNT_FF_FLUSH_BUF_R \n"
		"mfence \n"

	"LOOP_CHASING_STNT_FF_EXEC: \n"						/* execute workload */
        	"mov    %[start_addr], %%r10\n"
		"lea	(%%r10, %%r8), %%r9 \n"					/* r9: access loc */
		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
		"xor	%%r13, %%r13 \n"					/* r13: cycle-start */

		"mfence \n"
		"rdtscp	\n"
		"lfence	\n"
		"shl	$32, %%rdx\n"
		"or 	%%rax, %%rdx\n"
		"mov    %%rdx, %%r13\n"

		"xor	%%r12, %%r12 \n"					/* r12: chasing index addr */
	"LOOP_CHASING_STNT_FF_INNER: \n"
		"movq	(%[cindex], %%r12, 8), %%xmm0 \n"
		"shl    $0x06, %%r12 \n"
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r12) \n"
		"add	$0x40, %%r10 \n"
		"movq	%%xmm0, %%r12 \n"					/* Update to next chasing element */
		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_STNT_FF_INNER \n"

		"mfence \n"
		"rdtscp	\n"
		"lfence	\n"
		"shl	$32, %%rdx\n"
		"or 	%%rax, %%rdx\n"

		"sub    %%r13, %%rdx\n"
		"add    %%rdx, %[cycle]\n"

		"xor	%%r10, %%r10 \n"
		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"
		"jl		LOOP_CHASING_STNT_FF_OUTER \n"

		: [cycle]"+r"(total_cycle)
		: [start_addr]"g"(start_addr), [accesssize]"r"(size),
		  [count]"g"(count), [skip]"g"(skip), [cindex]"r"(cindex),
		  [flush_addr]"g"(flush_addr)
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8", "rax", "rcx",
		  "rdx");
	KERNEL_END
	return total_cycle;
}

uint64_t chasing_loadnt_ff(char *start_addr, long size, long skip, long count, uint64_t *cindex, char *flush_addr)
{
	uint64_t total_cycle = 0;
	KERNEL_BEGIN
	asm volatile (
        /* --------------------------------------- */
        /* Store chasing array first               */
        /* --------------------------------------- */
		"xor	%%r8, %%r8 \n"						/* r8: access offset       */
		"xor	%%r11, %%r11 \n"					/* r11: counter            */
	"LOOP_CHASING_LDNT_STFIRST_FF_OUTER: \n"				/* outer (counter) loop    */
        "mov    %[start_addr], %%r10\n"
		"lea	(%%r10, %%r8), %%r9 \n"					/* r9: access loc          */
		"xor	%%r10, %%r10 \n"					/* r10: accessed size      */
		"xor	%%r12, %%r12 \n"					/* r12: chasing index addr */
	"LOOP_CHASING_LDNT_STFIRST_FF_STNT_INNER: \n"
		"movq	(%[cindex], %%r12, 8), %%xmm0 \n"
		"shl    $0x06, %%r12 \n"
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r12) \n"
		"add	$0x40, %%r10 \n"
		"movq	%%xmm0, %%r12 \n"					/* Update to next chasing element */
		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_LDNT_STFIRST_FF_STNT_INNER \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"
		"jl	LOOP_CHASING_LDNT_STFIRST_FF_OUTER \n"
		"mfence \n"
		/* NOTE: make sure this store is larger than AIT cache */
		/*       so it serves as AIT flush as well             */
		/*       but eventuall we need a AIT flush with        */
		/*       non-interleaved data to make sure it flushes  */

		/* --------------------------------------- */
		/* Measure load latency                    */
		/* --------------------------------------- */
		"xor	%%r8, %%r8 \n"						/* r8: access offset       */
		"xor	%%r11, %%r11 \n"					/* r11: counter            */
	"LOOP_CHASING_LDNT_FF_OUTER: \n"					/* outer (counter) loop    */
 
 		/* flush rmw buffer by read-after-write 64KB data   */
 		"xor	%%r10, %%r10 \n"					/* r10: flushed size       */
		"mov    %[flush_addr], %%r12\n"
	"LOOP_CHASING_LDNT_FF_FLUSH_BUF_W: \n"
 		"vmovntdq	%%zmm0, (%%r12, %%r10) \n"
 		"add	$0x40, %%r10 \n"
 		"cmp	$0x10000, %%r10 \n"					/* 16KB rmw buf flush * 4  */
 		"jl	LOOP_CHASING_LDNT_FF_FLUSH_BUF_W \n"
 		"mfence \n"
 
 		"xor	%%r10, %%r10 \n"					/* r10: flushed size       */
		"mov    %[flush_addr], %%r12\n"
	"LOOP_CHASING_LDNT_FF_FLUSH_BUF_R: \n"
 		"vmovntdqa	(%%r12, %%r10), %%zmm0 \n"
 		"add	$0x40, %%r10 \n"
 		"cmp	$0x10000, %%r10 \n"					/* 16KB rmw buf flush * 4  */
 		"jl	LOOP_CHASING_LDNT_FF_FLUSH_BUF_R \n"
 		"mfence \n"
 
	"LOOP_CHASING_LDNT_FF_EXEC: \n"						/* execute workload        */
		"mov    %[start_addr], %%r10\n"
		"lea	(%%r10, %%r8), %%r9 \n"					/* r9: access loc          */
		"xor	%%r10, %%r10 \n"					/* r10: accessed size      */
		"xor	%%r12, %%r12 \n"					/* r12: chasing index addr */
		"xor	%%r13, %%r13 \n"					/* r13: cycle-start        */

        "mfence \n"
		"rdtscp	\n"
		"lfence	\n"
		"mov	%%edx, %%r13d\n"
		"shl	$32, %%r13\n"
		"or 	%%rax, %%r13\n"

	"LOOP_CHASING_LDNT_FF_INNER: \n"
		"shl    $0x06, %%r12 \n"
		"vmovntdqa	0x0(%%r9, %%r12), %%zmm0 \n"
		"movq	%%xmm0, %%r12 \n"					/* Update to next chasing element */
		"add	$0x40, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_LDNT_FF_INNER \n"

		"mfence \n"
		"rdtscp	\n"
		"lfence	\n"
		"shl	$32, %%rdx\n"
		"or 	%%rax, %%rdx\n"

		"sub	%%r13, %%rdx\n"
		"add	%%rdx, %[cycle]\n"

		"xor	%%r10, %%r10 \n"
		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"
		"jl	LOOP_CHASING_LDNT_FF_OUTER \n"

		: [cycle]"+r"(total_cycle)
		: [start_addr]"g"(start_addr), [accesssize]"r"(size),
		  [count]"g"(count), [skip]"g"(skip), [cindex]"r"(cindex),
		  [flush_addr]"r"(flush_addr)
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8", "rax", "rcx",
		  "rdx");
	KERNEL_END
	return total_cycle;
}

uint64_t chasing_combined_stld_ff(char *start_addr, long size, long skip, long count, uint64_t *cindex, char *flush_addr)
{
	uint64_t total_cycle = 0;
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"						/* r8: access offset */
		"xor	%%r11, %%r11 \n"					/* r11: counter */
	"LOOP_CHASING_COMB_FF_OUTER: \n"					/* outer (counter) loop */

		"mfence \n"
		/* flush rmw buffer by read-after-write 64KB data   */
		"xor	%%r10, %%r10 \n"					/* r10: flushed size */
		"mov    %[flush_addr], %%r12\n"
	"LOOP_CHASING_COMB_FF_FLUSH_BUF_W: \n"
		"vmovntdq	%%zmm0, (%%r12, %%r10) \n"
		"add	$0x40, %%r10 \n"
		"cmp	$0x10000, %%r10 \n"					/* 16KB rmw buf flush * 4 */
		"jl	LOOP_CHASING_COMB_FF_FLUSH_BUF_W \n"
		"mfence \n"

		"xor	%%r10, %%r10 \n"					/* r10: flushed size */
		"mov    %[flush_addr], %%r12\n"
	"LOOP_CHASING_COMB_FF_FLUSH_BUF_R: \n"
		"vmovntdqa	(%%r12, %%r10), %%zmm0 \n"
		"add	$0x40, %%r10 \n"
		"cmp	$0x10000, %%r10 \n"					/* 16KB rmw buf flush * 4 */
		"jl	LOOP_CHASING_COMB_FF_FLUSH_BUF_R \n"
		"mfence \n"

	"LOOP_CHASING_COMB_FF_EXEC: \n"						/* execute workload */
		"mov    %[start_addr], %%r10\n"
		"lea	(%%r10, %%r8), %%r9 \n"					/* r9: access loc */
		"xor	%%r13, %%r13 \n"					/* r13: cycle-start */

		"mfence \n"
		"rdtscp	\n"
		"lfence	\n"
		"mov	%%edx, %%r13d\n"
		"shl	$32, %%r13\n"
		"or 	%%rax, %%r13\n"

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
		"xor	%%r12, %%r12 \n"					/* r12: chasing index addr */
	"LOOP_CHASING_COMB_FF_STNT_INNER: \n"
		"movq	(%[cindex], %%r12, 8), %%xmm0 \n"
		"shl    $0x06, %%r12 \n"
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r12) \n"
		"movq	%%xmm0, %%r12 \n"					/* Update to next chasing element */
		"add	$0x40, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl	LOOP_CHASING_COMB_FF_STNT_INNER \n"
		"mfence \n"

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
		"xor	%%r12, %%r12 \n"					/* r12: chasing index addr */
	"LOOP_CHASING_COMB_FF_LDNT_INNER: \n"
		"shl    $0x06, %%r12\n"
		"vmovntdqa 0x0(%%r9, %%r12), %%zmm0\n"
		"movq   %%xmm0, %%r12\n"					/* Update to next chasing element */
		"add    $0x40, %%r10 \n"
		"cmp    %[accesssize], %%r10 \n"
		"jl     LOOP_CHASING_COMB_FF_LDNT_INNER \n"
		"mfence \n"

		"rdtscp	\n"
		"lfence	\n"
		"shl	$32, %%rdx\n"
		"or 	%%rax, %%rdx\n"

		"sub	%%r13, %%rdx\n"
		"add	%%rdx, %[cycle]\n"

		"xor	%%r10, %%r10 \n"
		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"
		"jl	LOOP_CHASING_COMB_FF_OUTER \n"

		: [cycle]"+r"(total_cycle)
		: [start_addr]"g"(start_addr), [accesssize]"r"(size),
		  [count]"g"(count), [skip]"g"(skip), [cindex]"r"(cindex),
		  [flush_addr]"r"(flush_addr)
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8", "rax", "rcx",
		  "rdx");
	KERNEL_END
	return total_cycle;
}

/* Write-combine queue probe */
void wcqprobe_256(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"						/* r8: access offset */
		"xor	%%r11, %%r11 \n"					/* r11: counter */
	"LOOP_WCQPROBE_OUTER: \n"						/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"				/* r9: access loc */

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
	"LOOP_WCQPROBE_INNER_0: \n"
		"vmovntdq	%%zmm0, 0(%%r9, %%r10)\n"
		"add	$256, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_WCQPROBE_INNER_0 \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
	"LOOP_WCQPROBE_INNER_1: \n"
		"vmovntdq	%%zmm0, 64(%%r9, %%r10)\n"
		"add	$256, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_WCQPROBE_INNER_1 \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
	"LOOP_WCQPROBE_INNER_2: \n"
		"vmovntdq	%%zmm0, 128(%%r9, %%r10)\n"
		"add	$256, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_WCQPROBE_INNER_2 \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
	"LOOP_WCQPROBE_INNER_3: \n"
		"vmovntdq	%%zmm0, 192(%%r9, %%r10)\n"
		"add	$256, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_WCQPROBE_INNER_3 \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl		LOOP_WCQPROBE_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip)
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void rpqprobe_256(char *start_addr, long size, long skip, long count, uint64_t *cindex)
{
	KERNEL_BEGIN
	asm volatile (
		"xor	%%r8, %%r8 \n"						/* r8: access offset */
		"xor	%%r11, %%r11 \n"					/* r11: counter */
	"LOOP_RPQPROBE_OUTER: \n"						/* outer (counter) loop */
		"lea	(%[start_addr], %%r8), %%r9 \n"				/* r9: access loc */

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
	"LOOP_RPQPROBE_INNER_0: \n"
		"vmovntdq	%%zmm0, 0(%%r9, %%r10)\n"
		"add	$256, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_RPQPROBE_INNER_0 \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
	"LOOP_RPQPROBE_INNER_1: \n"
		"vmovntdq	%%zmm0, 64(%%r9, %%r10)\n"
		"add	$256, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_RPQPROBE_INNER_1 \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
	"LOOP_RPQPROBE_INNER_2: \n"
		"vmovntdq	%%zmm0, 128(%%r9, %%r10)\n"
		"add	$256, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_RPQPROBE_INNER_2 \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"					/* r10: accessed size */
	"LOOP_RPQPROBE_INNER_3: \n"
		"vmovntdq	%%zmm0, 192(%%r9, %%r10)\n"
		"add	$256, %%r10 \n"
		"cmp	%[accesssize], %%r10 \n"
		"jl		LOOP_RPQPROBE_INNER_3 \n"
		SIZEBTST_FENCE

		"xor	%%r10, %%r10 \n"

		"add	%[skip], %%r8 \n"
		"inc	%%r11 \n"
		"cmp	%[count], %%r11 \n"

		"jl		LOOP_RPQPROBE_OUTER \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [count]"r"(count), [skip]"r"(skip)
		: "%r13", "%r12", "%r11", "%r10", "%r9", "%r8");
	KERNEL_END
}

void cachefence(char *start_addr, long size, long cache, long fence)
{
	KERNEL_BEGIN
	asm volatile (
		"movq %[start_addr], %%xmm0 \n"
		"xor %%r9, %%r9 \n"						/* r9: offset of write */
	"CACHEFENCE_FENCEBEGIN: \n"
		"xor %%r11, %%r11 \n"						/* r11: fence counter */
	"CACHEFENCE_FLUSHBEGIN: \n"
		"xor %%r10, %%r10 \n"						/* r10: clwb counter */
		//"movq %%r9, %%rdx \n"						/* rdx: flush start offset */
		"leaq (%[start_addr], %%r9), %%rdx \n"
	"CACHEFENCE_WRITEONE: \n"
		"vmovdqa64  %%zmm0, 0x0(%[start_addr], %%r9) \n"		/* Write one addr */
		"add $0x40, %%r9 \n"
		"add $0x40, %%r10 \n"
		"add $0x40, %%r11 \n"
		"cmp %[cache], %%r10 \n"					/* check clwb */
		"jl CACHEFENCE_WRITEONE \n"

		"leaq (%[start_addr], %%r9), %%rcx \n"				/* rcx: flush end offset, rdx->rcx */
		//"add %[start_addr], %%rcx"
	"CACHEFENCE_FLUSHONE: \n"
		"clwb (%%rdx) \n"						/* Flush from rdx to rcx */
		"add $0x40, %%rdx \n"
		"cmp %%rcx, %%rdx \n"
		"jl CACHEFENCE_FLUSHONE \n"

		"cmp %[fence], %%r11 \n"
		"jl CACHEFENCE_FLUSHBEGIN \n"
		CACHEFENCE_FENCE

		"cmp %[accesssize], %%r9 \n"
		"jl CACHEFENCE_FENCEBEGIN \n"

		:
		: [start_addr]"r"(start_addr), [accesssize]"r"(size),
		  [cache]"r"(cache), [fence]"r"(fence)
		: "%rdx", "%rcx", "%r11", "%r10", "%r9");
	KERNEL_END
	return;
}

void cacheprobe(char *start_addr, char *end_addr, long stride)
{
	KERNEL_BEGIN
	asm volatile (
		"mov	%[start_addr], %%r8 \n"
		"movq	%[start_addr], %%xmm0 \n"
	"LOOP_CACHEPROBE: \n"
		"vmovdqa64	%%zmm0, 0x0(%%r8) \n"
		"clflush	(%%r8) \n"
		"vmovdqa64	%%zmm0, 0x40(%%r8) \n"
		"clflush	0x40(%%r8) \n"
		"add	%[stride], %%r8 \n"
		"cmp	%[end_addr], %%r8 \n"
		"jl	LOOP_CACHEPROBE \n"
		"mfence \n"

		:
		: [start_addr]"r"(start_addr), [end_addr]"r"(end_addr),
		  [stride]"r"(stride)
		: "%r8");
	KERNEL_END
	return;
}

void imcprobe(char *start_addr, char *end_addr, long loop)
{
	KERNEL_BEGIN
	asm volatile (
		"xor %%r9, %%r9 \n"
		"movq %[start_addr], %%xmm0 \n"

	"LOOP1_IMCPROBE: \n"
		"mov %[start_addr], %%r8 \n"
	"LOOP2_IMCPROBE: \n"
		"vmovntdq %%zmm0, 0x0(%%r8) \n"
		"add $0x40, %%r8 \n"
		"cmp %[end_addr], %%r8 \n"
		"jl LOOP2_IMCPROBE \n"

		"add $1, %%r9 \n"
		"cmp %[loop], %%r9 \n"
		"jl LOOP1_IMCPROBE \n"

		:
		: [start_addr]"r"(start_addr), [end_addr]"r"(end_addr),
		  [loop]"r"(loop)
		: "%r8", "%r9");
	KERNEL_END
	return;
}


void seq_load(char * start_addr, char *end_addr, long size) {
	KERNEL_BEGIN
	asm volatile (
		"mov	%[start_addr], %%r9 \n"

	"LOOP_SEQLOAD1: \n"
		"xor	%%r8, %%r8 \n"
	"LOOP_SEQLOAD2: \n"
		"vmovntdqa	0x0(%%r9, %%r8), %%zmm0 \n"
		"add	$0x40, %%r8 \n"
		"cmp	%[size], %%r8 \n"
		"jl	LOOP_SEQLOAD2 \n"

		"add	%[size], %%r9 \n"
		"cmp	%[end_addr], %%r9 \n"
		"jl	LOOP_SEQLOAD1 \n"

		:
		: [start_addr]"r"(start_addr), [end_addr]"r"(end_addr),
		  [size]"r"(size)
		: "%r8", "%r9");
	KERNEL_END
	return;
}
void seq_store(char * start_addr, char *end_addr, long size) {
	KERNEL_BEGIN
	asm volatile (
		"mov	%[start_addr], %%r9 \n"
		"movq	%[start_addr], %%xmm0 \n"

	"LOOP_SEQSTORE1: \n"
		"xor	%%r8, %%r8 \n"
	"LOOP_SEQSTORE2: \n"
		"vmovdqa64	%%zmm0,  0x0(%%r9, %%r8) \n"
		"clwb	(%%r9, %%r8) \n"
		"add	$0x40, %%r8 \n"
		"cmp	%[size], %%r8 \n"
		"jl	LOOP_SEQSTORE2 \n"

		"add	%[size], %%r9 \n"
		"cmp	%[end_addr], %%r9 \n"
		"jl	LOOP_SEQSTORE1 \n"

		:
		: [start_addr]"r"(start_addr), [end_addr]"r"(end_addr),
		  [size]"r"(size)
		: "%r8", "%r9");
	KERNEL_END
	return;
}

void seq_clwb(char * start_addr, char *end_addr, long size) {
	KERNEL_BEGIN
	asm volatile (
		"mov	%[start_addr], %%r9 \n"
		"movq	%[start_addr], %%xmm0 \n"

	"LOOP_SEQCLWB1: \n"
		"xor	%%r8, %%r8 \n"
	"LOOP_SEQCLWB2: \n"
		"vmovdqa64	%%zmm0,  0x0(%%r9, %%r8) \n"
		"clwb	(%%r9, %%r8) \n"
		"add	$0x40, %%r8 \n"
		"cmp	%[size], %%r8 \n"
		"jl	LOOP_SEQCLWB2 \n"

		"add	%[size], %%r9 \n"
		"cmp	%[end_addr], %%r9 \n"
		"jl	LOOP_SEQCLWB1 \n"

		:
		: [start_addr]"r"(start_addr), [end_addr]"r"(end_addr),
		  [size]"r"(size)
		: "%r8", "%r9");
	KERNEL_END
}

void seq_nt(char * start_addr, char *end_addr, long size) {
	KERNEL_BEGIN
	asm volatile (
		"mov	%[start_addr], %%r9 \n"
		"movq	%[start_addr], %%xmm0 \n"

	"LOOP_SEQNT1: \n"
		"xor	%%r8, %%r8 \n"
	"LOOP_SEQNT2: \n"
		"vmovntdq	%%zmm0, 0x0(%%r9, %%r8) \n"
		"add	$0x40, %%r8 \n"
		"cmp	%[size], %%r8 \n"
		"jl	LOOP_SEQNT2 \n"

		"add	%[size], %%r9 \n"
		"cmp	%[end_addr], %%r9 \n"
		"jl	LOOP_SEQNT1 \n"

		:
		: [start_addr]"r"(start_addr), [end_addr]"r"(end_addr),
		  [size]"r"(size)
		: "%r8", "%r9");
	KERNEL_END
}

/*
 * repeat benchmarks
 */
uint64_t repeat_ntstore(char *addr, uint64_t size) {
    uint64_t t1 = 0, t2 = 0;
    uint64_t value = 0xC0FFEEEEBABE0000;
    uint64_t batch = size / 256;
    if (batch < 1) batch = 1;
    KERNEL_BEGIN
    asm volatile(
	// LOAD_VALUE
	"vbroadcastsd %[value], %%ymm0 \n"

	// LOAD_ADDR
	"mov %[memarea], %%rsi \n"
	"mfence \n"

	// access batch iteration
	"xor %%r10, %%r10 \n"
	"xor %%r11, %%r11 \n"

	// TIMING_BEG
	"rdtscp \n"
	"lfence \n"
	"mov %%edx, %%r9d \n"
	"mov %%eax, %%r8d \n"

"REPEAT_NTSTORE_LOOP:\n"
	"vmovntpd %%zmm0, 0*64(%%rsi) \n"
	"vmovntpd %%zmm0, 1*64(%%rsi) \n"
	"vmovntpd %%zmm0, 2*64(%%rsi) \n"
	"vmovntpd %%zmm0, 3*64(%%rsi) \n"

	"inc %%r10 \n"
	"add $256, %%rsi \n"
	"cmp %[batch], %%r10 \n"
	"jl REPEAT_NTSTORE_LOOP \n"

	// TIMING_END
	"mfence \n"
	"rdtscp \n"
	"lfence \n"
	"shl $32, %%rdx \n"
	"or  %%rax, %%rdx \n"
	"mov %%r9d, %%eax \n"
	"shl $32, %%rax \n"
	"or  %%r8, %%rax \n"
	"mov %%rax, %[t1] \n"
	"mov %%rdx, %[t2] \n"

	: [t1] "=r" (t1), [t2] "=r" (t2)
	: [memarea] "r" (addr), [value] "m" (value), [batch]"r"(batch)
	: "rsi", "rax", "rcx", "rdx", "r8", "r9", "r10", "zmm0");
    KERNEL_END
    return t2 - t1;
}
