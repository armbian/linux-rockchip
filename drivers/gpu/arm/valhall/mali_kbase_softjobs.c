// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2011-2024 ARM Limited. All rights reserved.
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

#include <mali_kbase.h>

#include <linux/dma-buf.h>
#include <asm/cacheflush.h>
#if IS_ENABLED(CONFIG_SYNC_FILE)
#include <mali_kbase_sync.h>
#endif
#include <linux/dma-mapping.h>
#include <uapi/gpu/arm/valhall/mali_base_kernel.h>
#include <mali_kbase_hwaccess_time.h>
#include <mali_kbase_mem_linux.h>
#include <tl/mali_kbase_tracepoints.h>
#include <mali_linux_trace.h>
#include <linux/version.h>
#include <linux/ktime.h>
#include <linux/pfn.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/version_compat_defs_for_valhall.h>

#define KBASEP_JIT_ALLOC_GPU_ADDR_ALIGNMENT ((u32)0x7)

int kbasep_jit_alloc_validate(struct kbase_context *kctx, struct base_jit_alloc_info *info)
{
	size_t j;

	CSTD_UNUSED(kctx);

	/* If the ID is zero, then fail the job */
	if (info->id == 0)
		return -EINVAL;

	/* Sanity check that the PA fits within the VA */
	if (info->va_pages < info->commit_pages)
		return -EINVAL;

	/* Ensure the GPU address is correctly aligned */
	if ((info->gpu_alloc_addr & KBASEP_JIT_ALLOC_GPU_ADDR_ALIGNMENT) != 0)
		return -EINVAL;

	/* Interface version 2 (introduced with kernel driver version 11.5)
	 * onward has padding and a flags member to validate.
	 *
	 * Note: To support earlier versions the extra bytes will have been set
	 * to 0 by the caller.
	 */

	/* Check padding is all zeroed */
	for (j = 0; j < sizeof(info->padding); j++) {
		if (info->padding[j] != 0)
			return -EINVAL;
	}

	/* Only valid flags shall be set */
	if (info->flags & ~(BASE_JIT_ALLOC_VALID_FLAGS))
		return -EINVAL;

#if !MALI_JIT_PRESSURE_LIMIT_BASE
	/* If just-in-time memory allocation pressure limit feature is disabled,
	 * heap_info_gpu_addr must be zeroed-out
	 */
	if (info->heap_info_gpu_addr)
		return -EINVAL;
#endif

	return 0;
}
