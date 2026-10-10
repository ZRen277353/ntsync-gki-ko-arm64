// SPDX-License-Identifier: GPL-2.0-only
/*
 * KernelSU/GKI integration around the upstream ntsync implementation.
 *
 * The upstream source is vendored in mainline/ so this driver has no build-time
 * dependency on the untracked reference material.
 */

#include <linux/lockdep.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "compat.h"

/* Load its definition before replacing it to suppress the upstream init/exit. */
#undef module_misc_device
#define module_misc_device(miscdevice)
#include "mainline/ntsync.c"
#undef module_misc_device

#include <linux/namei.h>
#include <linux/path.h>
#include <linux/user_namespace.h>
#include <linux/xattr.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
#include <linux/mnt_idmapping.h>
#endif

/*
 * Keep device setup synchronous. Embedding a delayed_work or timer_list in a
 * module makes the running kernel interpret a configuration-dependent
 * structure layout. A mismatch can corrupt the timer wheel during module init.
 */
static void ntsync_fix_perms(void)
{
	static const char context[] = "u:object_r:gpu_device:s0";
	struct path path;
	struct inode *inode;
	int ret;

	ret = kern_path("/dev/ntsync", LOOKUP_FOLLOW, &path);
	if (ret)
		return;

	inode = d_backing_inode(path.dentry);
	if (!inode)
		goto out_path;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 12, 0)
	ret = __vfs_setxattr_noperm(path.dentry, "security.selinux", context,
				      sizeof(context) - 1, 0);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
	ret = __vfs_setxattr_noperm(&init_user_ns, path.dentry,
				      "security.selinux", context,
				      sizeof(context) - 1, 0);
#else
	ret = __vfs_setxattr_noperm(&nop_mnt_idmap, path.dentry,
				      "security.selinux", context,
				      sizeof(context) - 1, 0);
#endif
	if (ret)
		pr_warn("ntsync: failed to set SELinux context: %d\n", ret);

	inode->i_mode = (inode->i_mode & ~S_IALLUGO) | 0666;

out_path:
	path_put(&path);
}

static int __init gki_ntsync_init(void)
{
	int ret;

	ret = misc_register(&ntsync_misc);
	if (ret)
		return ret;

	ntsync_fix_perms();
	return 0;
}

static void __exit gki_ntsync_exit(void)
{
	misc_deregister(&ntsync_misc);
}

module_init(gki_ntsync_init);
module_exit(gki_ntsync_exit);
