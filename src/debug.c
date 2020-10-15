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

#include <stdlib.h>
#include <stdint.h>
#include "memaccess.h"
#include "lat.h"

#define LFS_PERMRAND_ENTRIES 0x1000
uint32_t *lfs_random_array;

void latencyfs_prealloc_global_permutation_array(int size)
{
	int i;
	uint32_t temp, j;
	lfs_random_array = malloc(sizeof(uint32_t) * LFS_PERMRAND_ENTRIES);

	// initial range of numbers
	for (i = 0; i < LFS_PERMRAND_ENTRIES - 1; i++) {
		lfs_random_array[i] = i + 1;
	}

	lfs_random_array[LFS_PERMRAND_ENTRIES - 1] = 0;

	for (i = LFS_PERMRAND_ENTRIES - 1; i >= 0; i--) {
		//generate a random number [0, n-1]
		j = rand();
		j = j % LFS_PERMRAND_ENTRIES;

		//swap the last element with element at random index
		temp = lfs_random_array[i];
		lfs_random_array[i] = lfs_random_array[j];
		lfs_random_array[j] = temp;
	}
	for (i = 0; i < LFS_PERMRAND_ENTRIES; i++) {
		lfs_random_array[i] *= size;
	}
}

int main()
{
	char *buf;
	//	int i;
	uint64_t seed = 0xdeadbeef12345678;
	uint64_t a = 0xfc0;

	buf = malloc(4096 * 1024);
	//	for (i = 0; i < 4096*1024; i++)
	//	{
	//		buf[i]='a';
	//	}

	//	latencyfs_prealloc_global_permutation_array(64);
	sizebw_nt(buf, 64, 128, &seed, a);
	return 0;
}
