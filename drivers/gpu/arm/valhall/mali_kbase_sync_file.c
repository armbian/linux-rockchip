// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2012-2024 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

/*
 * Code for supporting explicit Linux fences (CONFIG_SYNC_FILE)
 */
#include "mali_kbase_sync.h"
#include "mali_kbase_fence.h"
#include "mali_kbase.h"

#include <linux/sched.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/anon_inodes.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/sync_file.h>
#include <linux/slab.h>
#include <linux/version_compat_defs_for_valhall.h>

static const struct file_operations stream_fops = { .owner = THIS_MODULE };

int kbase_sync_fence_stream_create(const char *name, int *const out_fd)
{
	if (!out_fd)
		return -EINVAL;

	*out_fd = anon_inode_getfd(name, &stream_fops, NULL, O_RDONLY | O_CLOEXEC);
	if (*out_fd < 0)
		return -EINVAL;

	return 0;
}

int kbase_sync_fence_validate(int fd)
{
	struct dma_fence *fence = sync_file_get_fence(fd);

	if (!fence)
		return -EINVAL;

	dma_fence_put(fence);

	return 0;
}

void kbase_sync_fence_info_get(struct dma_fence *fence, struct kbase_sync_fence_info *info)
{
	int status;
	info->fence = fence;

	/* Translate into the following status, with support for error handling:
	 * < 0 : error
	 * 0 : active
	 * 1 : signaled
	 */
	status = dma_fence_get_status(fence);

	if (status < 0)
		info->status = status; /* signaled with error */
	else if (status > 0)
		info->status = 1; /* signaled with success */
	else
		info->status = 0; /* still active (unsignaled) */

#if (KERNEL_VERSION(5, 1, 0) > LINUX_VERSION_CODE)
	scnprintf(info->name, sizeof(info->name), "%llu#%u", fence->context, fence->seqno);
#else
	scnprintf(info->name, sizeof(info->name), "%llu#%llu", fence->context, fence->seqno);
#endif
}
