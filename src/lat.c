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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vfs.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/io.h>
#include <linux/mount.h>
#include <linux/mm.h>
#include <linux/bitops.h>
#include <linux/exportfs.h>
#include <linux/list.h>
#include <linux/pfn_t.h>
#include <linux/kthread.h>
#include <linux/dax.h>
#include <linux/delay.h>
#include <linux/version.h>

#include "lat.h"
#include "comp_utils/dax_check.h"
#include "microbench/chasing.h"
#include "microbench/chasing_baf.h"
#include "microbench/overwrite.h"

struct latency_sbi *global_sbi = NULL;

uint32_t *lfs_random_array = NULL;

struct mutex latencyfs_lock;

int support_clwb = 0;

/* Module parameters handled by procfs */
/*
int task = 0;
int op = 0;
int access_size = 64;
int stride_size = 64;
int threads = 1;
int runtime = 60;

module_param(task, int, 0444);
MODULE_PARM_DESC(task, "Task");

module_param(op, int, 0444);
MODULE_PARM_DESC(op, "Operation? 0=Load, 1=Store (require clwb), 2=StoreNT");

module_param(access_size, int, 0444);
MODULE_PARM_DESC(access_size, "Access Size");

module_param(stride_size, int, 0444);
MODULE_PARM_DESC(stride_size, "Strided Size");

module_param(threads, int, 0444);
MODULE_PARM_DESC(threads, "#threads");

module_param(runtime, int, 0444);
MODULE_PARM_DESC(runtime, "runtime");
*/

static struct report_sbi *rep_sbi;

#ifndef X86_FEATURE_CLFLUSHOPT
#define X86_FEATURE_CLFLUSHOPT (9 * 32 + 23) /* CLFLUSHOPT instruction */
#endif
#ifndef X86_FEATURE_CLWB
#define X86_FEATURE_CLWB       (9 * 32 + 24) /* CLWB instruction */
#endif

#define PAGENR_2_ADDR (ctx, pagenr, dimm)                                      \
	ctx->addr + (dimm * pagenr << PAGE_SHIFT)

static inline bool arch_has_clwb(void)
{
	return static_cpu_has(X86_FEATURE_CLWB);
}

extern struct report_sbi *reportfs_get_sbi(void);

int latencyfs_print_help(struct seq_file *seq, void *v)
{
	int i;
	int len_chasing_funcs;

	seq_printf(seq, "LENS task=<task> op=<op> [options]\n");
#ifndef CLWB_SUPPORTED
	seq_printf(seq, "!! Warning: clwb not supported on current platform.\n");
#endif
	seq_printf(seq, "Tasks:\n");
	for (i = 1; i < TASK_COUNT; i++)
		seq_printf(seq, "\t%d: %s %s\n", i, task_desc[i].name, task_desc[i].options);

	seq_printf(seq, "Options:\n");
	seq_printf(seq, "\t [op|o]             Operation: 0=Load, 1=Write(NT) 2=Write(clwb), 3=Write+Flush(clwb+flush) default=0\n");
	seq_printf(seq, "\t [access_size|a]    Access size (bytes), default=64; for covert task: send data size (bit), default=64\n");
	seq_printf(seq, "\t [stride_size|s]    Stride size (bytes), default=64\n");
	seq_printf(seq, "\t [pc_region_size|R] Pointer chasing region size (bytes), default=64\n");
	seq_printf(seq, "\t [pc_block_size|B]  Pointer chasing block size (bytes), default=64\n");
	seq_printf(seq, "\t [align_mode|l]     Align mode: 0 = Global (anywhere), 1 = Per-Thread pool (default) 2 = Per-DIMM pool 3=Per-iMC pool\n");
	seq_printf(seq, "\t                                4 = Per-Channel Group, 5 = [NIOnly!!!]: Global, 6 = [NIOnly!!!]: Per-DIMM\n");
	seq_printf(seq, "\t [delay|d]          Delay (cycles), default=0\n");
	seq_printf(seq, "\t [delay_per_byte|e]     Access size in byte before issuing a delay, default=0\n");
	seq_printf(seq, "\t [parallel|]        Concurrent kthreads, default=1\n");
	seq_printf(seq, "\t [bwsize_bit|b]     Aligned size (2^b bytes), default=6\n");
	seq_printf(seq, "\t [runtime|t]        Runtime (seconds), default=10\n");
	seq_printf(seq, "\t [write_addr|w]     For Straight Write: access location, default=0 \n");
	seq_printf(seq, "\t [write_size|z]     For Straight Write: access size, default=0 \n");
	seq_printf(seq, "\t [fence_rate|f]     Access size before issuing a sfence instruction \n");
	seq_printf(seq, "\t [clwb_rate|c]      Access size before issuing a clwb instruction \n");
	seq_printf(seq, "\t [repeat|r]    Number of test rounds \n");
	seq_printf(seq, "\t [message|m]        A string as a unique ID for the current job \n");

	seq_printf(seq, "Available pointer chasing benchmarks:\n");
	seq_printf(seq, "\tNo.\tName\t\t\tBlock Size (Byte)\n");
	len_chasing_funcs =
		sizeof(chasing_func_list) / sizeof(chasing_func_entry_t);
	for (i = 0; i < len_chasing_funcs; i++)
		seq_printf(seq, "\t%d\t%s\t%llu\n", i, chasing_func_list[i].name,
			   chasing_func_list[i].block_size);

	seq_printf(seq, "Available pointer chasing back and forth benchmarks:\n");
	seq_printf(seq, "\tNo.\tName\t\t\tBlock Size (Byte)\n");
	len_chasing_funcs =
		sizeof(chasing_baf_func_list) / sizeof(chasing_func_entry_t);
	for (i = 0; i < len_chasing_funcs; i++)
		seq_printf(seq, "\t%d\t%s\t%llu\n", i, chasing_baf_func_list[i].name,
			   chasing_baf_func_list[i].block_size);

	return 0;
}

inline void latencyfs_cleanup(struct latency_sbi *sbi)
{
	kfree(sbi->workers);
	kfree(sbi->timing);
	kfree(sbi->ctx);
}

inline void latencyfs_new_threads(struct latency_sbi *sbi, bench_func_t func)
{
	int i;
	struct latencyfs_worker_ctx *ctx;
	int cnt = sbi->worker_cnt;
	uint64_t dimmgroup;
	char kthread_name[20];

	if (sbi->align_mode >= ALIGN_PERDIMM) {
		pr_info("Warning: PerDIMM/PerIMC/PerChannel alignment mode selected, it is only supported on certain tasks\n");
		if (sbi->access_size > 4096)
			pr_info("Warning: PerDIMM alignment with 4K+ accesses will write across DIMMs\n");
		if (sbi->worker_cnt > 6)
			pr_info("Warning: PerDIMM alignment with 6+ threads\n");
	}

	sbi->workers = kmalloc(sizeof(struct task_struct *) * cnt, GFP_KERNEL);
	sbi->ctx =
		kmalloc(sizeof(struct latencyfs_worker_ctx) * cnt, GFP_KERNEL);
	sbi->timing =
		kmalloc(sizeof(struct latencyfs_timing) * cnt, GFP_KERNEL);
	for (i = 0; i < cnt; i++) {
		ctx	     = &sbi->ctx[i];
		ctx->sbi     = sbi;
		ctx->core_id = i;
		ctx->job_id  = i;
		switch (sbi->align_mode) {
		case ALIGN_GLOBAL:
		case ALIGN_PERIMC:
		case ALIGN_NI_GLOBAL:
			ctx->addr = (u8 *)sbi->virt_addr;
			break;
		case ALIGN_PERTHREAD:
			ctx->addr =
				(u8 *)sbi->virt_addr + (i * PERTHREAD_WORKSET);
			break;
		case ALIGN_PERDIMM:
		case ALIGN_PERCHANNELGROUP:
			dimmgroup = i / 6;
			ctx->addr = (u8 *)sbi->virt_addr +
				    (dimmgroup * PERDIMMGROUP_WORKSET);
			break;
		case ALIGN_NI:
			ctx->addr = (u8 *)sbi->virt_addr + ((i % 6) * 6 * 4096);
			pr_info("dimm %d, addr %px", i, ctx->addr);
			break;
		default:
			pr_err("Undefined align mode %d\n", sbi->align_mode);
		}
		sbi->timing[i].v = 0;
		ctx->seed_addr =
			(u8 *)sbi->rep->virt_addr + (i * PERTHREAD_WORKSET);
		sprintf(kthread_name, "lens%d", ctx->core_id);
		sbi->workers[i] =
			kthread_create(func, (void *)ctx, kthread_name);
		kthread_bind(sbi->workers[i], ctx->core_id);
	}
}

inline void latencyfs_new_pair_threads(struct latency_sbi *sbi,
				       bench_func_t	   func)
{
	int			     i;
	struct latencyfs_worker_ctx *ctx;
	int			     cnt = sbi->worker_cnt;
	char			     kthread_name[20];

	if (cnt != 2) {
		pr_err("Number of threads must be 2, but got %d\n", cnt);
		return;
	}

	sbi->workers = kmalloc(sizeof(struct task_struct *) * cnt, GFP_KERNEL);
	sbi->ctx =
		kmalloc(sizeof(struct latencyfs_worker_ctx) * cnt, GFP_KERNEL);
	sbi->timing =
		kmalloc(sizeof(struct latencyfs_timing) * cnt, GFP_KERNEL);
	for (i = 0; i < cnt; i++) {
		ctx	     = &sbi->ctx[i];
		ctx->sbi     = sbi;
		ctx->core_id = i + 1;
		ctx->job_id  = i;
		switch (sbi->align_mode) {
		case ALIGN_BY_SIZE:
			ctx->addr =
				(u8 *)sbi->virt_addr + (i * sbi->align_size);
			pr_info("ctx->addr=%px, align_size = 0x%016llx\n",
				ctx->addr, sbi->align_size);
			break;
		default:
			ctx->addr = (u8 *)sbi->virt_addr;
			pr_info("Undefined align mode %d, default to sbi->virt_addr=%px\n",
				sbi->align_mode, sbi->virt_addr);
			break;
		}
		init_completion(&ctx->complete);
		init_completion(&ctx->sub_op_ready);
		init_completion(&ctx->sub_op_complete);
		sbi->timing[i].v = 0;
		sprintf(kthread_name, "lens[%d]", ctx->job_id);
		sbi->workers[i] =
			kthread_create(func, (void *)ctx, kthread_name);
		kthread_bind(sbi->workers[i], ctx->core_id);
	}
}

void latencyfs_timer_callback(unsigned long data)
{
	//TODO
}

inline void latencyfs_create_seed(struct latency_sbi *sbi)
{
	latencyfs_prealloc_large(sbi->rep->virt_addr,
				 sbi->worker_cnt * PERTHREAD_WORKSET,
				 PERTHREAD_MASK);
	pr_info("Random buffer created.");
	drop_cache();
}

inline void latencyfs_stop_threads(struct latency_sbi *sbi)
{
	int i;
	int cnt = sbi->worker_cnt;

	for (i = 0; i < cnt; i++) {
		kthread_stop(sbi->workers[i]);
	}
}

inline void latencyfs_monitor_threads(struct latency_sbi *sbi)
{
	int i;
	int elapsed = 0;
	int runtime = sbi->runtime;
	u64 last = 0, total;

	latencyfs_create_seed(sbi);

	/* Wake up workers */
	for (i = 0; i < sbi->worker_cnt; i++) {
		wake_up_process(sbi->workers[i]);
	}

	/* Print result every second
	 * Use a timer if accuracy is a concern
	 */
	while (elapsed < runtime) {
		msleep(1000);
		total = 0;
		for (i = 0; i < sbi->worker_cnt; i++) {
			total += sbi->timing[i].v;
		}
		printk(KERN_ALERT "%d\t%lld (%d)\n", elapsed + 1,
		       (total - last) / MB, smp_processor_id());

		last = total;
		elapsed++;
	}
	latencyfs_stop_threads(sbi);
}

inline void latencyfs_monitor_pair_threads(struct latency_sbi *sbi)
{
	int i, j;

	drop_cache();
	// latencyfs_create_seed(sbi);

	/* Wake up workers */
	for (i = 0; i < sbi->worker_cnt; i++) {
		wake_up_process(sbi->workers[i]);
	}

	/*
	 * For pair threads, the sbi->access_size represents the # of bits for
	 * communication
	 */
	if (1 == sbi->sync_per_iter) {
		for (j = 0; j < sbi->access_size; j++) {
			if (1 == sbi->warm_up) {
				// for (i = 0; i < sbi->worker_cnt; i++) {
				// 	overwrite_warmup_thread(&(sbi->ctx[i]));
				// }
				overwrite_warmup_thread(&(sbi->ctx[1]));
			}

			for (i = 0; i < sbi->worker_cnt; i++)
				complete(&sbi->ctx[i].sub_op_ready);

			for (i = 0; i < sbi->worker_cnt; i++)
				wait_for_completion(&sbi->ctx[i].sub_op_complete);

			for (i = 0; i < sbi->worker_cnt; i++)
				init_completion(&sbi->ctx[i].sub_op_complete);
		}
	}

	/* Wait for completion */
	for (i = 0; i < sbi->worker_cnt; i++) {
		wait_for_completion(&sbi->ctx[i].complete);
	}
	latencyfs_stop_threads(sbi);

	/* Print final message */
	pr_info("LENS_THREADS_ALL_FINSIHED");
}

int setup_singlethread_op(struct latency_sbi *sbi, int op)
{
	switch (op) {
	case TASK_BASIC_OP:
		latencyfs_new_threads(sbi, &latency_job);
		break;
	case TASK_OVERWRITE:
		latencyfs_new_threads(sbi, &overwrite_job);
		break;
	case TASK_PC_WRITE:
		latencyfs_new_threads(sbi, &pointer_chasing_write_job);
		break;
	case TASK_PC_READ_AND_WRITE:
		latencyfs_new_threads(sbi, &pointer_chasing_read_and_write_job);
		break;
	case TASK_PC_READ_AFTER_WRITE:
		latencyfs_new_threads(sbi, &pointer_chasing_read_after_write_job);
		break;
	case TASK_FLUSH_FIRST:
		latencyfs_new_threads(sbi, &flush_first_job);
		break;
	case TASK_STRIDED_LAT:
		latencyfs_new_threads(sbi, &strided_latjob);
		break;
	case TASK_WEAR_LEVELING:
		latencyfs_new_threads(sbi, &wear_leveling_job);
		break;
	case TASK_PC_STRIDED:
		latencyfs_new_threads(sbi, &pointer_chasing_strided_job);
		break;
	case TASK_DEBUGGING:
		latencyfs_new_threads(sbi, &debugging_job);
		break;
	default:
		pr_err("Single thread op %d not expected\n", op);
		return -EINVAL;
	}
	wake_up_process(sbi->workers[0]);
	return 0;
}

int setup_multithread_op(struct latency_sbi *sbi, int op)
{
	unsigned long delta = 0;
	switch (op) {
	case TASK_STRIDED_BW: // 3
		latencyfs_new_threads(sbi, &strided_bwjob);
		break;
	case TASK_SIZE_BW: // 4
		//latencyfs_prealloc_global_permutation_array(sbi->access_size);
		delta = PERTHREAD_CHECKSTOP % sbi->access_size;
		latencyfs_new_threads(sbi, &sizebw_job);
		break;
	default:
		pr_err("Multi thread op %d not expected\n", op);
		return -EINVAL;
	}
	latencyfs_monitor_threads(sbi);
	return 0;
}

int setup_multi_pair_thread_op(struct latency_sbi *sbi, int op)
{
	switch (op) {
	case TASK_BUFFER_COVERT_CHANNEL: // 13
		latencyfs_new_pair_threads(sbi, &buffer_covert_channel_job);
		break;
	case TASK_WEAR_LEVELING_COVERT_CHANNEL: // 15
		latencyfs_new_pair_threads(sbi, &wear_leveling_covert_channel_job);
		break;
	case TASK_WEAR_LEVELING_SIDE_CHANNEL: // 16
		latencyfs_new_pair_threads(sbi, &wear_leveling_side_channel_job);
		break;
	default:
		pr_err("Multi pair thread op %d not expected\n", op);
		return -EINVAL;
	}
	latencyfs_monitor_pair_threads(sbi);
	return 0;
}

void latencyfs_start_task(struct latency_sbi *sbi, int task, int threads)
{
	int i, ret = 0;

	sbi->task = task;

	switch (task) {
	case TASK_BASIC_OP:
	case TASK_STRIDED_LAT:
	case TASK_OVERWRITE:
	case TASK_PC_WRITE:
	case TASK_PC_READ_AND_WRITE:
	case TASK_PC_READ_AFTER_WRITE:
	case TASK_FLUSH_FIRST:
	case TASK_WEAR_LEVELING:
	case TASK_PC_STRIDED:
	case TASK_DEBUGGING:
		sbi->worker_cnt = 1;
		ret = setup_singlethread_op(sbi, task);
		break;

	case TASK_STRIDED_BW:
	case TASK_SIZE_BW:
	case TASK_SEQ:
		sbi->worker_cnt = threads;
		ret = setup_multithread_op(sbi, task);
		break;
	
	case TASK_BUFFER_COVERT_CHANNEL:
	case TASK_WEAR_LEVELING_COVERT_CHANNEL:
	case TASK_WEAR_LEVELING_SIDE_CHANNEL:
		if (0 != threads % 2) {
			pr_err("Thread count must be even, not %d\n", threads);
			return;
		}
		sbi->worker_cnt = threads;
		ret = setup_multi_pair_thread_op(sbi, task);
		break;

	case TASK_INVALID:
		pr_err("Remount using task=<id>, tasks:\n");
		for (i = 1; i < TASK_COUNT; i++) {
			pr_err("%d: %s\n", i, task_desc[i].name);
		}
		break;

	default:
		pr_err("Invalid Task\n");
		return;
	}

	if (ret) {
		pr_info("Warning: start task returned %d", ret);
	}
}

static int latencyfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct latency_sbi *sbi;
	void *virt_addr = NULL;
	struct inode *root;
	struct dax_device *dax_dev;
	pfn_t __pfn_t;
	long size;
	int ret;

	if (rep_sbi) {
		pr_err("Already mounted\n");
		return -EEXIST;
	}

	rep_sbi = reportfs_get_sbi();

	if (!rep_sbi) {
		pr_err("Mount ReportFS first\n");
		return -EINVAL;
	}

	sbi = kzalloc(sizeof(struct latency_sbi), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;
	sbi->sb	      = sb;
	sbi->rep      = rep_sbi;

	ret = check_dax(sb, PAGE_SIZE);
	pr_info("%s: dax_supported = %d; bdev->super=0x%px", __func__, ret,
		sb->s_bdev->bd_super);
	if (ret) {
		pr_err("device does not support DAX\n");
		return -EINVAL;
	}

	sbi->s_bdev = sb->s_bdev;

	dax_dev = dax_get_by_host(sb->s_bdev->bd_disk->disk_name);
	if (!dax_dev) {
		pr_err("Couldn't retrieve DAX device.\n");
		return -EINVAL;
	}

	size = dax_direct_access(dax_dev, 0, LONG_MAX / PAGE_SIZE, &virt_addr,
				 &__pfn_t) * PAGE_SIZE;
	if (size <= 0) {
		pr_err("direct_access failed\n");
		return -EINVAL;
	}

	sbi->virt_addr = virt_addr;
	sbi->phys_addr = pfn_t_to_pfn(__pfn_t) << PAGE_SHIFT;
	sbi->initsize  = size;

	pr_info("%s: dev %s, phys_addr 0x%llx, virt_addr 0x%016llx, size %ld\n",
		__func__, sbi->s_bdev->bd_disk->disk_name, (uint64_t)sbi->phys_addr,
		(uint64_t)sbi->virt_addr, sbi->initsize);

	root = new_inode(sb);
	if (!root) {
		pr_err("root inode allocation failed\n");
		return -ENOMEM;
	}

	root->i_ino = 0;
	root->i_sb  = sb;
	pr_info("DEBUG: lat: root->i_ctime[%px]=%llu\n", &root->i_ctime, root->i_ctime.tv_sec);
// #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0)
// 	// ktime_get_ts64(&root->i_ctime);
// 	root->i_ctime = ktime_to_timespec64(ktime_get_real());
// #else
// 	ktime_get_coarse_real_ts64(&root->i_ctime);
// #endif
	pr_info("DEBUG: lat: root->i_ctime[%px]=%llu\n", &root->i_ctime, root->i_ctime.tv_sec);
	// root->i_atime = root->i_mtime = root->i_ctime;
	root->i_atime.tv_sec = root->i_ctime.tv_sec;
	root->i_atime.tv_nsec = root->i_ctime.tv_nsec;
	root->i_mtime.tv_sec = root->i_ctime.tv_sec;
	root->i_mtime.tv_nsec = root->i_ctime.tv_nsec;
	inode_init_owner(root, NULL, S_IFDIR);

	sb->s_root = d_make_root(root);
	if (!sb->s_root) {
		pr_err("d_make_root failed\n");
		return -ENOMEM;
	}

	global_sbi = sbi;

	pr_info("[%s] sb->s_root=0x%px", __func__, sb->s_root);
	pr_info("done");
	// pr_info("DEBUG: dget=%px\n", dget(sb->s_root));

	return 0;
}

static struct dentry *latencyfs_mount(struct file_system_type *fs_type,
				      int flags, const char *dev_name,
				      void *data)
{
	struct dentry *ret;
	if (!dev_name || !*dev_name) {
		BUG_ON("dev_name is NULL\n");
	}
	ret = mount_bdev(fs_type, flags, dev_name, data, latencyfs_fill_super);
	return ret;
}

static struct file_system_type latencyfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "LatencyFS",
	.mount = latencyfs_mount,
	.kill_sb = kill_block_super,
};

static int __init init_latencyfs(void)
{
	int rc = 0;
	struct file *fp;

	mutex_init(&latencyfs_lock);

	pr_info("%s: %d cpus online\n", __func__, num_online_cpus());
	if (arch_has_clwb())
		support_clwb = 1;

	pr_info("Arch new instructions support: CLWB %s\n",
		support_clwb ? "YES" : "NO");

	fp = filp_open("/proc/lens", O_RDONLY, 0);
	if (!PTR_ERR(fp)) {
		pr_err("LENS running, exiting.\n");
		return 0;
	}

	rc = latencyfs_proc_init();
	if (rc)
		return rc;

	rc = register_filesystem(&latencyfs_fs_type);
	if (rc)
		return rc;

	return 0;
}

static void __exit exit_latencyfs(void)
{
	if (lfs_random_array)
		kfree(lfs_random_array);
	remove_proc_entry("lens", NULL);
	unregister_filesystem(&latencyfs_fs_type);
}

MODULE_LICENSE("GPL");

module_init(init_latencyfs);
module_exit(exit_latencyfs);
