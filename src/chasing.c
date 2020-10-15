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

#include "chasing.h"
#include <linux/string.h>

void chasing_print_help(void)
{
	int len = sizeof(chasing_func_list) / sizeof(chasing_func_entry_t);
	int i;

	printk("Available pointer chasing benchmarks: \n");
	printk("\tNo.\tName\t\t\tBlock Size (Byte)\n");
	for (i = 0; i < len; i++)
		printk("\t%d\t%s\t%lu\n", i, chasing_func_list[i].name,
		       chasing_func_list[i].block_size);
}

/* 
 * Find index of a pointer chasing function according to block_size
 * Return -1 if not found
 */
int chasing_find_func(unsigned long block_size)
{
	int ret = -1;
	int i;
	int len = sizeof(chasing_func_list) / sizeof(chasing_func_entry_t);

	for (i = 0; i < len; i++) {
		if (chasing_func_list[i].block_size == block_size) {
			ret = i;
			break;
		}
	}

	return ret;
}

#define RDRAND_MAX_RETRY 32

/* 
 * Generate random number to [rd] within range [0 - range).
 * Return 0 if success, 1 if fail.
 */
inline int get_rand(uint64_t *rd, uint64_t range)
{
	uint8_t ok;
	int i = 0;
	for (i = 0; i < RDRAND_MAX_RETRY; i++) {
		asm volatile(
			"rdrand %0\n\t"
			"setc   %1\n\t"
			: "=r"(*rd), "=qm"(ok)
		);

		if (ok) {
			*rd = *rd % range;
			return 0;
		}
	}

	return 1;
}

/* 
 * Generate a chasing array at [cindex] memory address. Each array element is a
 *   8 byte pointer (or pointer chasing block index) used by pointer chasing
 *   benchmarks.
 */
int init_chasing_index(uint64_t *cindex, uint64_t csize)
{
	uint64_t curr_pos = 0;
	uint64_t next_pos = 0;
	uint64_t i = 0;
	int ret = 0;

	memset(cindex, 0, sizeof(uint64_t) * csize);

	for (i = 0; i < csize - 1; i++) {
		do {
			ret = get_rand(&next_pos, csize);
			if (ret != 0)
				return 1;
		} while ((cindex[next_pos] != 0) || (next_pos == curr_pos));

		cindex[curr_pos] = next_pos;
		curr_pos = next_pos;
	}

	return 0;
}
