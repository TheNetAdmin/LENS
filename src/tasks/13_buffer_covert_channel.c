#include "tasks.h"

int buffer_covert_channel_job(void *arg)
{
	char *this_task_name		 = "TASK_BUFFER_COVERT_CHANNEL";
	struct latencyfs_worker_ctx *ctx = (struct latencyfs_worker_ctx *)arg;
	uint64_t region_align		 = ctx->sbi->pc_region_align;
	uint64_t region_size		 = ctx->sbi->pc_region_size;
	uint64_t block_size		 = ctx->sbi->pc_block_size;
	uint64_t strided_size		 = ctx->sbi->strided_size;
	uint8_t *uc_addr		 = get_uc_addr(ctx, per_thread);
	uint8_t *buf			 = uc_addr;
	uint8_t *buf_end		 = buf + GLOBAL_WORKSET;
	uint8_t *covert_channel          = buf;
	uint64_t count			 = GLOBAL_WORKSET / region_size;
	uint64_t repeat			 = ctx->sbi->repeat;
	uint64_t op			 = ctx->sbi->op;
	uint64_t *send_data_buffer	 = ctx->sbi->rep->virt_addr;
	int hash			 = 0;
	channel_info_t		     ci;
	char			     covert_strategy[30];
	char role[10];
	int role_type;

	long pages, diff;
	int i;
	unsigned ti, tj;
	uint64_t *cindex;

	/* ReportFS memory region
	 * 0-128MiB: |covert_send_data|
	 * 128-256MiB: |chasing_array0 16MiB|chasing_array1 16MiB|...|chasing_array7 16MiB|
	 * 256MiB- : |timing0 128MiB|timing1 128MiB|...|
	 */
	enum {
		data_region_size = 128 * 1024 * 1024,
		chasing_array_size_per_worker = 16 * 1024 * 1024,
		timing_buffer_size_per_worker = 128 * 1024 * 1024,
		max_workers = 8, /* TODO: Update this once we have more workers */
	};

	if (!validate_address(ctx->sbi, (void *)buf) ||
	    !validate_address(ctx->sbi, (void *)send_data_buffer)) {
		LAT_WARN(false);
		goto buffer_covert_channel_job_error_end;
	}

	uint64_t *timing =
		(uint64_t *)(ctx->sbi->rep->virt_addr + data_region_size +
			     chasing_array_size_per_worker * max_workers +
			     timing_buffer_size_per_worker * (ctx->job_id));
	uint64_t csize;
	int ret;
	unsigned int chasing_func_index;
	uint64_t region_skip = region_size;

	uint64_t send_data = 0xf0f0f0f0f0f0f0f0;
	// uint64_t send_data	 = 0xcccccccccccccccc;
	// uint64_t send_data	 = 0xaaaaaaaaaaaaaaaa;
	uint64_t curr_data = 0;
	uint64_t total_data_bits = ctx->sbi->access_size;

	/* define variables for time measurements */
	PC_VARS;

	if(ctx->job_id >= max_workers) {
		kr_info("ERROR: job_id (%d) exceeds max_workers (%u)\n", ctx->job_id, max_workers);
		goto buffer_covert_channel_job_error_end;
	}

	kr_info("Send data buffer: 0x%px\n", send_data_buffer);
	if (ctx->job_id == 0) {
		kr_info("Send data:\n");
		for (i = 0; i < total_data_bits / 64; i ++) {
			kr_info("  [%04d]: 0x%016llx\n", i, send_data_buffer[i]);
		}
	}

	/* Check if the data region is valid, i.e. if the first byte is 0xcc
	 * See gen_cover_data_pattern.py
	 */
	curr_data = 0xff & send_data_buffer[0];
	if(0xcc != curr_data) {
		kr_info("ERROR: The first byte of send data must be 0xcc, but got 0x%llx", curr_data);
		goto buffer_covert_channel_job_error_end;
	}

	/* Determine current threads role, sender/receiver */
	switch (ctx->job_id) {
	case 0:
		strscpy(role, "sender", 7);
		role_type = 0;
		break;
	case 1:
		strscpy(role, "receiver", 9);
		role_type = 1;
		break;
	default:
		strscpy(role, "unknown", 8);
		role_type = -1;
		break;
	}

	/* find pointer chasing benchmark */
	chasing_func_index = chasing_find_func(block_size);
	if (chasing_func_index == -1) {
		chasing_print_help();
		kr_info("Pointer chasing benchamrk with block size %lld byte not found\n",
			block_size);
		goto buffer_covert_channel_job_error_end;
	}

	/* fix strided_size if overlapping */
	if (strided_size < block_size) {
		kr_info("strided size (%llu) < block size (%llu), "
			"set strided size to block size\n",
			strided_size, block_size);
		strided_size = block_size;
	}

	/* decide region_skip */
	region_skip = (region_size / block_size) * strided_size;

	if (region_skip % region_align != 0) {
		region_skip = ((region_skip / region_align) + 1) * region_align;
	}

	/* Cacheable region base=0x000000000 size=0x4000000000
	 * DIMM PMEM13 start address 0x3040000000
	 * --> Cacheable region on PMEM13: 63GiB
	 */
	if (region_skip > DIMM_SIZE - 64ULL * 1024 * 1024 * 1024) {
		kr_info("ERROR: Region skip %llu > DIMM size %llu\n",
			region_skip, DIMM_SIZE - 64ULL * 1024 * 1024);
		return 1;
	}
	
	/* NOTE: to ensure pc-stride always work on only one region, not to
	 *       move to the next region. This is for multi repeats.
	 */
	count = 1;

	/* fill page tables */
	pages = roundup((count + 1) * region_skip, PAGE_SIZE) >> PAGE_SHIFT;
	for (i = 0; i < pages; i++) {
		buf  = uc_addr + (i << PAGE_SHIFT);
		hash = hash ^ *buf;
	}
	buf = uc_addr;

	kr_info("Working set begin: 0x%llx end: 0x%llx, phy-begin: %llx, region_size=%llu, "
		"region_skip=%llu, block_size=%llu, strided_size=%llu, "
		"func=%s, count=%llu, repeat=%llu\n",
		(uint64_t)buf, (uint64_t)buf_end, ctx->sbi->phys_addr, region_size, region_skip, block_size,
		strided_size, chasing_func_list[chasing_func_index].name,
		count, repeat);

	/* generate pointer chasing index in reportfs (in DRAM by default) */
	csize  = region_size / block_size;
	cindex = (uint64_t *)((char *)(ctx->sbi->rep->virt_addr) +
			      data_region_size +
			      chasing_array_size_per_worker * ctx->job_id);
	ret    = init_chasing_index(cindex, csize);
	if (ret != 0) {
		pr_info("Chasing index generation failed\n");
		goto buffer_covert_channel_job_error_end;
	}

	kr_info("covert_id=%d, role_type=%10s, buf=%px, csize=%llu, cindex=%px, timing=%px\n",
		ctx->job_id, role, buf, csize, cindex, timing);

	ci.ctx		      = ctx;
	ci.role_type	      = role_type;
	ci.total_data_bits    = total_data_bits;
	ci.send_data	      = send_data_buffer;
	ci.buf		      = buf;
	ci.op		      = op;
	ci.sub_op_ready	      = &ctx->sub_op_ready;
	ci.sub_op_complete    = &ctx->sub_op_complete;
	ci.chasing_func_index = chasing_func_index;
	ci.block_size	      = block_size;
	ci.region_size	      = region_size;
	ci.strided_size	      = strided_size;
	ci.region_skip	      = region_skip;
	ci.region_align	      = region_align;
	ci.count	      = count;
	ci.repeat	      = repeat;
	ci.cindex	      = cindex;
	ci.timing	      = timing;

	switch (op) {
	case 0:
		covert_ptr_chasing_load_and_store(&ci);
		break;
	case 1:
		covert_ptr_chasing_load_only(&ci);
		break;
	case 2:
		covert_ptr_chasing_both_channel_load_and_store(&ci);
		break;
	default:
		for (i = 0; i < ci.total_data_bits; i++) {
			wait_for_completion(&ctx->sub_op_ready);
			init_completion(&ctx->sub_op_ready);
			complete(&ctx->sub_op_complete);
		}
		break;
	}

buffer_covert_channel_job_end:
	/* TODO: after adding `repeat` and `op`, etc, the following log print 
	 *       is not unique anymore, needs udpate
	 */
	kr_info("%s_END: %llu-%llu-%llu\n", this_task_name, region_size,
		block_size, strided_size);
	/* NOTE: Important, set this complete so upper waiting can finish */
	complete(&ctx->complete);
	wait_for_stop();
	return 0;

buffer_covert_channel_job_error_end:
	// for (i = 0; i < ci.total_data_bits; i++) {
	// 	wait_for_completion(ci.sub_op_ready);
	// 	init_completion(ci.sub_op_ready);
	// 	complete(ci.sub_op_complete);
	// }
	complete(&ctx->complete);
	wait_for_stop();
	return 1;
}
