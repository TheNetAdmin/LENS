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

#ifndef COMMON_H
#define COMMON_H

/*
 * Assembly instructions utilize the following registers:
 * rsi: memory address
 * rax, rdx, rcx, r8d and r9d: timing
 * rdx: populating cache-lines
 * ymm0: streaming instructions
 */
#define REGISTERS "rsi", "rax", "rdx", "rcx", "r8", "r9", "ymm0"

#define REGISTERS_NONSSE "rsi", "rax", "rdx", "rcx", "r8", "r9"

/* ymm0: 256-bit register (requires AVX support)
 * vbroadcastsd: VEX.256-bit version (r[0] = r[1] = r[2] = r[3] = v)
 */
#define LOAD_VALUE "vbroadcastsd %[value], %%ymm0 \n"

#define LOAD_ADDR                                                              \
	"mov %[memarea], %%rsi \n"                                             \
	"mfence \n"

#define FLUSH_CACHE_LINE                                                       \
	"clflush 0*32(%%rsi) \n"                                               \
	"clflush 2*32(%%rsi) \n"                                               \
	"clflush 4*32(%%rsi) \n"                                               \
	"clflush 6*32(%%rsi) \n"                                               \
	"mfence \n"

#define LOAD_CACHE_LINE                                                        \
	"movq 0*8(%%rsi), %%rdx \n"                                            \
	"movq 1*8(%%rsi), %%rdx \n"                                            \
	"movq 2*8(%%rsi), %%rdx \n"                                            \
	"movq 3*8(%%rsi), %%rdx \n"                                            \
	"movq 4*8(%%rsi), %%rdx \n"                                            \
	"movq 5*8(%%rsi), %%rdx \n"                                            \
	"movq 6*8(%%rsi), %%rdx \n"                                            \
	"movq 7*8(%%rsi), %%rdx \n"                                            \
	"movq 8*8(%%rsi), %%rdx \n"                                            \
	"movq 9*8(%%rsi), %%rdx \n"                                            \
	"movq 10*8(%%rsi), %%rdx \n"                                           \
	"movq 11*8(%%rsi), %%rdx \n"                                           \
	"movq 12*8(%%rsi), %%rdx \n"                                           \
	"movq 13*8(%%rsi), %%rdx \n"                                           \
	"movq 14*8(%%rsi), %%rdx \n"                                           \
	"movq 15*8(%%rsi), %%rdx \n"                                           \
	"movq 16*8(%%rsi), %%rdx \n"                                           \
	"movq 17*8(%%rsi), %%rdx \n"                                           \
	"movq 18*8(%%rsi), %%rdx \n"                                           \
	"movq 19*8(%%rsi), %%rdx \n"                                           \
	"movq 20*8(%%rsi), %%rdx \n"                                           \
	"movq 21*8(%%rsi), %%rdx \n"                                           \
	"movq 22*8(%%rsi), %%rdx \n"                                           \
	"movq 23*8(%%rsi), %%rdx \n"                                           \
	"movq 24*8(%%rsi), %%rdx \n"                                           \
	"movq 25*8(%%rsi), %%rdx \n"                                           \
	"movq 26*8(%%rsi), %%rdx \n"                                           \
	"movq 27*8(%%rsi), %%rdx \n"                                           \
	"movq 28*8(%%rsi), %%rdx \n"                                           \
	"movq 29*8(%%rsi), %%rdx \n"                                           \
	"movq 30*8(%%rsi), %%rdx \n"                                           \
	"movq 31*8(%%rsi), %%rdx \n"                                           \
	"mfence \n"

#define CLEAR_PIPELINE                                                         \
	"nop \nnop \nnop \nnop \nnop \nnop \n"                                 \
	"nop \nnop \nnop \nnop \nnop \nnop \n"                                 \
	"nop \nnop \nnop \nnop \nnop \nnop \n"                                 \
	"nop \nnop \nnop \nnop \nnop \nnop \n"                                 \
	"nop \nnop \nnop \nnop \nnop \nnop \n"                                 \
	"nop \nnop \nnop \nnop \nnop \nnop \n"                                 \
	"nop \nnop \nnop \nnop \nnop \nnop \n"

/* rdtscp: reads current timestamp to EDX:EAX and also sets ECX
 * higher 32-bits of RAX, RDX and RCX are cleared
 */
#define TIMING_BEG                                                             \
	"rdtscp \n"                                                            \
	"lfence \n"                                                            \
	"mov %%edx, %%r9d \n"                                                  \
	"mov %%eax, %%r8d \n"

/* r9d = old EDX
 * r8d = old EAX
 * Here is what we do to compute t1 and t2:
 * - RDX holds t2
 * - RAX holds t1
 */
#define TIMING_END                                                             \
	"mfence \n"                                                            \
	"rdtscp \n"                                                            \
	"lfence \n"                                                            \
	"shl $32, %%rdx \n"                                                    \
	"or  %%rax, %%rdx \n"                                                  \
	"mov %%r9d, %%eax \n"                                                  \
	"shl $32, %%rax \n"                                                    \
	"or  %%r8, %%rax \n"                                                   \
	"mov %%rax, %[t1] \n"                                                  \
	"mov %%rdx, %[t2] \n"

#define TIMING_END_NOFENCE                                                     \
	"rdtscp \n"                                                            \
	"shl $32, %%rdx \n"                                                    \
	"or  %%rax, %%rdx \n"                                                  \
	"mov %%r9d, %%eax \n"                                                  \
	"shl $32, %%rax \n"                                                    \
	"or  %%r8, %%rax \n"                                                   \
	"mov %%rax, %[t1] \n"                                                  \
	"mov %%rdx, %[t2] \n"

#endif // COMMON_H
