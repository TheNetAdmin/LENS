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
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "lat.h"

struct proc_dir_entry *latencyfs_proc;
extern struct latency_sbi *global_sbi;


static const struct latency_option lattest_opts[] =
{
	{"task",           OPT_INT,    'T'},
	{"op",             OPT_INT,    'o'},
	{"parallel",       OPT_INT,    'p'},
	{"runtime",        OPT_INT,    't'},
	{"message",        OPT_STRING, 'm'},
	{"access_size",    OPT_INT,    'a'},
	{"stride_size",    OPT_INT,    's'},
	{"pc_region_size", OPT_INT,    'R'},
	{"pc_block_size",  OPT_INT,    'B'},
	{"bwsize_bit",     OPT_INT,    'b'},
	{"delay",          OPT_INT,    'd'},
	{"write_start",    OPT_INT,    'w'},
	{"write_size",     OPT_INT,    'z'},
	{"align_mode",     OPT_INT,    'l'},
	{"fence_rate",     OPT_INT,    'f'},
	{"clwb_rate",      OPT_INT,    'c'},
	{"probe_count",    OPT_INT,    'r'},
	{NULL,             0,            0}
};

void latencyfs_parse_cmd(struct latency_sbi *sbi, char *cmd)
{
	int op;
	char *optarg;
	unsigned long optint;
	char *message = NULL;
	int threads   = 1;
	int task      = 0;
	int ret       = 0;

	/* Default args */
	sbi->op		    = 0;
	sbi->access_size    = 64;
	sbi->strided_size   = 64;
	sbi->pc_region_size = 64;
	sbi->pc_block_size  = 64;
	sbi->runtime	    = 10;
	sbi->bwsize_bit	    = 6;
	sbi->delay	    = 0;
	sbi->write_start    = 0;
	sbi->write_size	    = 64;
	sbi->clwb_rate	    = 64;
	sbi->fence_rate	    = 64;
	sbi->probe_count    = LATENCY_OPS_COUNT;
	sbi->align	    = ALIGN_PERTHREAD;

	while ((op = latencyfs_getopt("lens", &cmd, lattest_opts, NULL,
				      &optarg, &optint)) != 0) {
		switch (op) {
		case 'T':
			if (optint > 0 && optint < TASK_COUNT)
				task = optint;
			else
				pr_info("Ignore parameter: task = %ld\n",
					optint);
			break;
		case 'o':
			if (optint >= 0 && optint < 4)
				sbi->op = optint;
			else
				pr_info("Ignore parameter: op = %ld\n", optint);
			break;
		case 'p':
			if (optint > 0 && optint < LFS_THREAD_MAX)
				threads = optint;
			else
				pr_info("Ignore parameter: parallel = %ld\n",
					optint);
			break;
		case 't':
			if (optint > 0 && optint < LFS_RUNTIME_MAX)
				sbi->runtime = optint;
			else
				pr_info("Ignore parameter: runtime = %ld\n",
					optint);
			break;
		case 'm':
			message = optarg;
			break;
		case 'a':
			if (optint > 0 && optint <= LFS_ACCESS_MAX)
				sbi->access_size = optint;
			else
				pr_info("Ignore parameter: access_size = %ld\n",
					optint);
			break;
		case 's':
			if (optint >= 0 && optint <= LFS_ACCESS_MAX)
				sbi->strided_size = optint;
			else
				pr_info("Ignore parameter: stride_size = %ld\n",
					optint);
			break;
		case 'R':
			if (optint >= 0 && optint <= LFS_ACCESS_MAX)
				sbi->pc_region_size = optint;
			else
				pr_info("Ignore parameter: pc_region_size = %ld\n",
					optint);
			break;
		case 'B':
			if (optint >= 0 && optint <= LFS_ACCESS_MAX)
				sbi->pc_block_size = optint;
			else
				pr_info("Ignore parameter: pc_block_size = %ld\n",
					optint);
			break;
		case 'b':
			if (optint >= CACHELINE_BITS &&
			    optint <= LFS_ACCESS_MAX_BITS)
				sbi->bwsize_bit = optint;
			else
				pr_info("Ignore parameter: bwsize_bit = %ld\n",
					optint);
			break;
		case 'd':
			if (optint > 0 && optint <= LFS_DELAY_MAX)
				sbi->delay = optint;
			else
				pr_info("Ignore parameter: delay = %ld\n",
					optint);
			break;
		case 'w':
			if (optint > 0 && optint < GLOBAL_WORKSET)
				sbi->write_start = optint;
			else
				pr_info("Ignore parameter: write_start = %ld\n",
					optint);
			break;
		case 'z':
			if (optint > 0 && optint < LFS_ACCESS_MAX)
				sbi->write_size = optint;
			else
				pr_info("Ignore parameter: write_size = %ld\n",
					optint);
			break;
		case 'l':
			if (optint >= 0 && optint < ALIGN_INVALID)
				sbi->align = optint;
			else
				pr_info("Ignore parameter: align_mode = %ld\n",
					optint);
			break;
		case 'f':
			if (optint > 0 && optint <= LFS_ACCESS_MAX)
				sbi->fence_rate = optint;
			else
				pr_info("Ignore parameter: fence_rate = %ld\n",
					optint);
			break;
		case 'c':
			if (optint > 0 && optint <= LFS_ACCESS_MAX)
				sbi->clwb_rate = optint;
			else
				pr_info("Ignore parameter: clwb_rate = %ld\n",
					optint);
			break;
		case 'r':
			// 64GB (access size is 28)
			if (optint > 0 && optint <= (1ULL << 28))
				sbi->probe_count = optint;
			else
				pr_info("Ignore parameter: probe_count = %ld\n",
					optint);
			break;
		default:
			pr_info("unknown opt %s\n", optarg);
			ret = -EINVAL;
			break;
		}
	}

	if (ret)
		goto out;

	if (message == NULL) {
		pr_info("Required parameter: message\n");
		ret = -EINVAL;
		goto out;
	}

	if (task == 0) {
		pr_info("Task not assigned\n");
		ret = -EINVAL;
		goto out;
	}

	/* Sanity checks */
	if (task == TASK_STRIDED_LAT || task == TASK_STRIDED_BW) {
		if (sbi->strided_size < sbi->access_size) {
			pr_info("Error: stride size is smaller than access size.\n");
			ret = -EINVAL;
			goto out;
		}
	}

	if (task == TASK_FLUSH_FIRST) {
		if ((sbi->strided_size != 0) &&
		    (sbi->strided_size < sbi->access_size)) {
			pr_info("Error: stride size (not 0) is smaller than access size.\n");
			ret = -EINVAL;
			goto out;
		}
	}

	if (task == TASK_SIZE_BW) {
		if (!sbi->bwsize_bit) {
			pr_info("SizeBW task requires bwsize_bit.\n");
			ret = -EINVAL;
			goto out;
		}
	}

	if (message)
		pr_info("LENS_START: %s\n", message);
	latencyfs_start_task(sbi, task, threads);
	if (message)
		pr_info("LENS_END: %s\n", message);

	return;

out:
	pr_info("parse error: %d\n", ret);
}

static ssize_t latencyfs_proc_write(struct file *file,
				    const char __user *buffer, size_t count,
				    loff_t *ppos)
{
	char *cmdline;
	int ret = 0;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	cmdline = kmalloc(count, GFP_KERNEL);
	if (!cmdline) {
		pr_err("kmalloc failure\n");
		return -ENOMEM;
	}
	if (copy_from_user(cmdline, buffer, count)) {
		return -EFAULT;
	}

	cmdline[count - 1] = 0;
	pr_info("Command [%s]\n", cmdline);

	if (!global_sbi) {
		pr_err("LatencyFS not mounted.");
		kfree(cmdline);
		return count;
	}

	latencyfs_parse_cmd(global_sbi, cmdline);

	kfree(cmdline);

	module_put(THIS_MODULE);

	if (ret)
		return ret;
	else
		return (int)count;
}

static int latencyfs_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, latencyfs_print_help, inode->i_private);
}

static struct file_operations latencyfs_ops = {
	.owner	 = THIS_MODULE,
	.open	 = latencyfs_proc_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
	.write	 = latencyfs_proc_write,
};

int latencyfs_proc_init(void)
{
	latencyfs_proc = proc_create("lens", 0666, NULL, &latencyfs_ops);
	if (!latencyfs_proc) {
		pr_err("/proc/lens creation failed\n");
		return -ENOMEM;
	}

	return 0;
}
