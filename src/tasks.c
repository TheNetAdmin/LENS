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
#include "tasks.h"

char *get_hostname(void)
{
	return utsname()->nodename;
}

uint8_t *get_uc_addr(struct latencyfs_worker_ctx *ctx, uc_addr_align_mode_t align_mode)
{
	uint8_t *addr  = NULL;
	char *hostname = get_hostname();
	if (0 == strcmp(hostname, "AEP2")) {
		addr = (uint8_t *)((((uint64_t)ctx->addr) & 0xffffffc000000000) + 0x4000000000);
	} else if (0 == strcmp(hostname, "sdp")) {
		addr = (uint8_t *)((((uint64_t)ctx->addr) & 0xffffffc000000000) + 0x4000000000);
	} else if (0 == strcmp(hostname, "nv-4")) {
		addr = (uint8_t *)(phys_to_virt(0x2000000000));
	} else if (0 == strcmp(hostname, "lens")) {
		addr = (uint8_t *)(phys_to_virt(0x180000000));
	} else {
		BUG_ON("Unrecognized hostname");
	}

	switch (align_mode)
	{
	case overlapped:
		break;
	case per_thread:
		addr += ctx->sbi->align_size * ctx->job_id;
		break;
	default:
		BUG_ON("Unrecognized align mode");
		break;
	}

	pr_info("get_uc_addr(): hostname=%s, align_size=0x%016llx, orig_addr=0x%px, uc_addr=0x%px, uc_phys_addr=0x%016llx\n",
	        hostname,
		ctx->sbi->align_size,
	        ctx->addr,
	        addr,
	        virt_to_phys((void *)addr));
	return addr;
}

uint64_t get_dimm_size(void)
{
	char *hostname = get_hostname();
	if (0 == strcmp(hostname, "AEP2")) {
		/* 
		 * Cacheable region base=0x000000000 size=0x4000000000
		 * DIMM PMEM13 start address 0x3040000000
		 * --> Cacheable region on PMEM13: 63GiB
		 */
		return (250ULL * GB - 64ULL * GB);
	} else if (0 == strcmp(hostname, "sdp")) {
		/* 
		 * Cacheable region base=0x000000000 size=0x4000000000
		 * DIMM PMEM1 start address 0x3040000000
		 * --> Cacheable region on PMEM13: 63GiB
		 */
		return (250ULL * GB - 64ULL * GB);
	} else if (0 == strcmp(hostname, "nv-4")) {
		return (128ULL * GB - 32ULL * GB);
	} else if (0 == strcmp(hostname, "lens")) {
		return (4UL * GB);
	} else {
		BUG_ON("Unrecognized hostname");
	}
}

void print_core_id(void)
{
	/* https://stackoverflow.com/questions/61349444/linux-kernel-development-how-to-get-physical-core-id */
	unsigned cpu;
	struct cpuinfo_x86 *info;

	cpu  = get_cpu();
	info = &cpu_data(cpu);
	printk("CPU: %u, core: %d, smp_processor: %d\n", cpu, info->cpu_core_id, smp_processor_id());
	put_cpu();
}
