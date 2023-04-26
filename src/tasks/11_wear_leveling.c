#include "tasks.h"

int wear_leveling_job(void *arg)
{
	int j;
	static unsigned long flags;
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	struct latency_sbi *sbi          = ctx->sbi;
	int op                           = sbi->op;
	long delay                       = ctx->sbi->delay;
	long delay_per_byte              = ctx->sbi->delay_per_byte;
	uint8_t *uc_addr                 = get_uc_addr(ctx, overlapped);
	int skip_size                    = 256;
	long access_size                 = sbi->access_size;
	long stride_size                 = sbi->strided_size;
	uint64_t write_count             = 2 * LATENCY_OPS_COUNT;
	uint64_t threshold_cycle         = sbi->threshold_cycle;
	u8 *buf;
	u8 *buf_start;
	u8 *buf_end;

	long *rbuf;
	size_t rpages;
	size_t max_rpages;

	u8 hash = 0;

	rbuf = (long *)(sbi->rep->virt_addr);

	if (sbi->count > 1) {
		write_count = sbi->count;
	}

	kr_info("WEAR_LEVELING_OP at smp id %d\n", smp_processor_id());
	rpages     = roundup((2 * BASIC_OPS_TASK_COUNT + 1) * write_count * sizeof(long), PAGE_SIZE) >> PAGE_SHIFT;
	max_rpages = sbi->rep->initsize >> PAGE_SHIFT;
	if (rpages > max_rpages)
		rpages = max_rpages;

	kr_info("rpages=%lu, rbuf=0x%016llx\n", rpages, (uint64_t)rbuf);
	/* fill report area page tables */
	for (j = 0; j < rpages; j++) {
		buf  = (u8 *)sbi->rep->virt_addr + (j << PAGE_SHIFT);
		hash = hash ^ *buf;
	}

	buf       = uc_addr;
	buf_start = uc_addr;
	buf_end   = buf + access_size;

	kr_info("Working set begin: 0x%llx end: 0x%llx, phy-begin: %llx\n",
	        (uint64_t)buf,
	        (uint64_t)buf_end,
	        ctx->sbi->phys_addr);
	kr_info("Overwrite count=%llu\n", write_count);

	/* fill page tables */
	for (j = 0; j < BASIC_OP_POOL_PAGES; j++) {
		buf  = uc_addr + (j << PAGE_SHIFT);
		hash = hash ^ *buf;
	}

	/* Fix skip size */
	if ((buf_end - buf_start) % 256 != 0){
		skip_size = 64;
		/* TODO: Check implementation with skip_size not 256 */
		LAT_WARN(false);
	}

	BENCHMARK_BEGIN(flags);

	switch (op) {
	case 0:
		overwrite_vanilla(buf_start, buf_end, skip_size, write_count, rbuf);
		break;
	case 1:
		kr_info("Delay: %ld\n", delay);
		overwrite_delay_each(buf_start, buf_end, skip_size, write_count, rbuf, delay);
		break;
	case 2:
		if (buf + stride_size > buf_end) {
			kr_info("Distance (%lu) overflows the buffer size (%lu)!", stride_size, access_size);
			break;
		}
		overwrite_two_points(buf_start, buf_end, write_count, rbuf, stride_size);
		break;
	case 3:
		kr_info("Delay: %ld\n", delay);
		overwrite_delay_half(buf_start, buf_end, skip_size, write_count, rbuf, delay);
		break;
	case 4:
		kr_info("Delay: %ld\n", delay);
		kr_info("Delay per byte: %ld\n", delay_per_byte);
		overwrite_delay_per_size(buf_start, buf_end, skip_size, write_count, rbuf, delay, delay_per_byte);
		break;
	case 5:
		kr_info("Delay: %ld\n", delay);
		kr_info("Delay per byte: %ld\n", delay_per_byte);
		overwrite_delay_per_size_cpuid(buf_start, buf_end, skip_size, write_count, rbuf, delay, delay_per_byte);
		break;
	case 6:
		kr_info(
			"threshold_cycle=%llu, "
			"write_count=%llu, "
			"threshold_cycle=%llu, "
			"\n",
			threshold_cycle,
			write_count,
			threshold_cycle
		);
		for (j = 0; j < sbi->repeat; j++)
			overwrite_warmup_range_by_block(buf_start, buf_end, skip_size, threshold_cycle, 0);
		break;
	case 7:
		kr_info(
			"threshold_cycle=%llu, "
			"write_count=%llu, "
			"threshold_cycle=%llu, "
			"\n",
			threshold_cycle,
			write_count,
			threshold_cycle
		);
		for (j = 0; j < sbi->repeat; j++)
			overwrite_warmup_range_by_range(buf_start, buf_end, skip_size, threshold_cycle, 0);
		break;
	default:
		kr_info("Invalid sub op: %d\n", op);
		break;
	}

	print_core_id();

	BENCHMARK_END(flags);

	kr_info("Done. hash %d", hash);
	kr_info("TASK_WEAR_LEVELING_END:\n");
	wait_for_stop();
	return 0;
}
