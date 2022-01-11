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
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/parser.h>
#include <linux/vfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/mm.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/exportfs.h>
#include <linux/random.h>
#include <linux/cred.h>
#include <linux/list.h>
#include <linux/pfn_t.h>
#include <linux/dax.h>
#include <linux/version.h>

#include "lat.h"

int support_clwb = 0;
static struct report_sbi *g_report_sbi = NULL;

#define X86_FEATURE_CLFLUSHOPT (9 * 32 + 23) /* CLFLUSHOPT instruction */
#define X86_FEATURE_CLWB       (9 * 32 + 24) /* CLWB instruction */

static inline bool arch_has_clwb(void)
{
	return static_cpu_has(X86_FEATURE_CLWB);
}

static struct report_sbi *reportfs_get_sbi(void)
{
	return g_report_sbi;
}
EXPORT_SYMBOL(reportfs_get_sbi);

static int reportfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct report_sbi *sbi;
	void *virt_addr = NULL;
	struct inode *root;
	struct dax_device *dax_dev;
	pfn_t __pfn_t;
	long size;
	int ret;

	sbi = kzalloc(sizeof(struct report_sbi), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;

	if (g_report_sbi) {
		pr_err("Another ReportFS already at VA %p PA %llx\n",
		       g_report_sbi->virt_addr, g_report_sbi->phys_addr);
		return -EEXIST;
	} else {
		g_report_sbi = sbi;
	}

	sb->s_fs_info = sbi;
	sbi->sb = sb;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 6)
	ret = bdev_dax_supported(sb, PAGE_SIZE);
#else
	ret = bdev_dax_supported(sb->s_bdev, PAGE_SIZE);
#endif
	pr_info("%s: dax_supported = %d; bdev->super=0x%p", __func__, ret,
		sb->s_bdev->bd_super);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 6)
	if (ret) {
#else
	if (!ret) {
#endif
		pr_err("device does not support DAX\n");
		return -EINVAL;
	}

	sbi->s_bdev = sb->s_bdev;

	dax_dev = fs_dax_get_by_host(sb->s_bdev->bd_disk->disk_name);
	if (!dax_dev) {
		pr_err("Couldn't retrieve DAX device.\n");
		return -EINVAL;
	}

	size = dax_direct_access(dax_dev, 0, LONG_MAX / PAGE_SIZE, &virt_addr,
				 &__pfn_t) *
	       PAGE_SIZE;
	if (size <= 0) {
		pr_err("direct_access failed\n");
		return -EINVAL;
	}

	sbi->virt_addr = virt_addr;
	sbi->phys_addr = pfn_t_to_pfn(__pfn_t) << PAGE_SHIFT;
	sbi->initsize  = size;

	pr_info("%s: dev %s, phys_addr 0x%llx, virt_addr %p, size %ld\n",
		__func__, sbi->s_bdev->bd_disk->disk_name, sbi->phys_addr,
		sbi->virt_addr, sbi->initsize);

	root = new_inode(sb);
	if (!root) {
		pr_err("root inode allocation failed\n");
		return -ENOMEM;
	}

	root->i_ino = 0;
	root->i_sb  = sb;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0)
	root->i_atime = root->i_mtime = root->i_ctime = current_kernel_time();
#else
	ktime_get_coarse_real_ts64(&root->i_ctime);
	root->i_atime = root->i_mtime = root->i_ctime;
#endif
	inode_init_owner(root, NULL, S_IFDIR);

	sb->s_root = d_make_root(root);
	if (!sb->s_root) {
		pr_err("d_make_root failed\n");
		return -ENOMEM;
	}

	return 0;
}

static struct dentry *reportfs_mount(struct file_system_type *fs_type,
				     int flags, const char *dev_name,
				     void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, reportfs_fill_super);
}

static struct file_system_type reportfs_fs_type = {
	.owner	 = THIS_MODULE,
	.name	 = "ReportFS",
	.mount	 = reportfs_mount,
	.kill_sb = kill_block_super,
};

static int __init init_reportfs(void)
{
	int rc = 0;

	pr_info("%s: %d cpus online\n", __func__, num_online_cpus());
	if (arch_has_clwb())
		support_clwb = 1;

	pr_info("Arch new instructions support: CLWB %s\n",
		support_clwb ? "YES" : "NO");

	rc = register_filesystem(&reportfs_fs_type);
	if (rc)
		return rc;

	return 0;
}

static void __exit exit_reportfs(void)
{
	unregister_filesystem(&reportfs_fs_type);
}

MODULE_LICENSE("GPL");

module_init(init_reportfs);
module_exit(exit_reportfs);
