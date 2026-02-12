/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
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

/**
 * DOC: This file contains our internal "API" for explicit fences.
 * It hides the implementation details of the actual explicit fence mechanism
 * used (Android fences or sync file with DMA fences).
 */

#ifndef MALI_KBASE_SYNC_H
#define MALI_KBASE_SYNC_H

#include <linux/fdtable.h>
#include <linux/syscalls.h>
#if IS_ENABLED(CONFIG_SYNC_FILE)
#include <linux/sync_file.h>
#endif

#include <linux/version_compat_defs_for_valhall.h>

/**
 * struct kbase_sync_fence_info - Information about a fence
 * @fence: Pointer to fence (type is void*, as underlaying struct can differ)
 * @name: The name given to this fence when it was created
 * @status: < 0 means error, 0 means active, 1 means signaled
 *
 * Use kbase_sync_fence_in_info_get() or kbase_sync_fence_out_info_get()
 * to get the information.
 */
struct kbase_sync_fence_info {
	void *fence;
	char name[32];
	int status;
};

/**
 * kbase_sync_fence_stream_create() - Create a stream object
 * @name: Name of stream (only used to ease debugging/visualization)
 * @out_fd: A file descriptor representing the created stream object
 *
 * Can map down to a timeline implementation in some implementations.
 * Exposed as a file descriptor.
 * Life-time controlled via the file descriptor:
 * - dup to add a ref
 * - close to remove a ref
 *
 * Return: 0 on success, < 0 on error
 */
int kbase_sync_fence_stream_create(const char *name, int *const out_fd);

/**
 * kbase_sync_fence_validate() - Validate a fd to be a valid fence
 *
 * @fd: File descriptor to check
 *
 * This function is only usable to catch unintentional user errors early,
 * it does not stop malicious code changing the fd after this function returns.
 *
 * Return: 0 if fd is for a valid fence, < 0 if invalid
 */
int kbase_sync_fence_validate(int fd);

#if IS_ENABLED(CONFIG_SYNC_FILE)
void kbase_sync_fence_info_get(struct dma_fence *fence, struct kbase_sync_fence_info *info);
#endif

/**
 * kbase_sync_status_string() - Get string matching @status
 * @status: Value of fence status.
 *
 * Return: Pointer to string describing @status.
 */
const char *kbase_sync_status_string(int status);

#endif /* MALI_KBASE_SYNC_H */
