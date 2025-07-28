// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2019-2024 ARM Limited. All rights reserved.
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
 * DOC: Mali arbiter interface APIs to share GPU between Virtual Machines
 */

#include <mali_kbase.h>
#include "mali_kbase_arbif.h"
#include <tl/mali_kbase_tracepoints.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include "linux/mali_arbiter_interface.h"

/* Arbiter interface version against which was implemented this module */
#define MALI_REQUIRED_KBASE_ARBITER_INTERFACE_VERSION 5
#if MALI_REQUIRED_KBASE_ARBITER_INTERFACE_VERSION != MALI_ARBITER_INTERFACE_VERSION
#error "Unsupported Mali Arbiter interface version."
#endif


/**
 * kbase_arbif_destroy() - De-init Kbase arbiter interface
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * De-initialise Kbase arbiter interface
 */
void kbase_arbif_destroy(struct kbase_device *kbdev)
{
	struct arbiter_if_dev *arb_if = kbdev->arb.arb_if;

	if (arb_if && arb_if->vm_ops.vm_arb_unregister_dev)
		arb_if->vm_ops.vm_arb_unregister_dev(kbdev->arb.arb_if);

	kbdev->arb.arb_if = NULL;
}

/**
 * kbase_arbif_get_max_config() - Request max config info
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * call back function from arb interface to arbiter requesting max config info
 */
void kbase_arbif_get_max_config(struct kbase_device *kbdev)
{
	struct arbiter_if_dev *arb_if = kbdev->arb.arb_if;

	if (arb_if && arb_if->vm_ops.vm_arb_get_max_config)
		arb_if->vm_ops.vm_arb_get_max_config(arb_if);
}

/**
 * kbase_arbif_gpu_request() - Request GPU from
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * call back function from arb interface to arbiter requesting GPU for VM
 */
void kbase_arbif_gpu_request(struct kbase_device *kbdev)
{
	struct arbiter_if_dev *arb_if = kbdev->arb.arb_if;

	if (arb_if && arb_if->vm_ops.vm_arb_gpu_request) {
		KBASE_TLSTREAM_TL_ARBITER_REQUESTED(kbdev, kbdev);
		KBASE_KTRACE_ADD(kbdev, ARB_GPU_REQUESTED, NULL, 0);
		arb_if->vm_ops.vm_arb_gpu_request(arb_if);
	}
}

/**
 * kbase_arbif_gpu_stopped() - send GPU stopped message to the arbiter
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 * @gpu_required: GPU request flag
 *
 */
void kbase_arbif_gpu_stopped(struct kbase_device *kbdev, u8 gpu_required)
{
	struct arbiter_if_dev *arb_if = kbdev->arb.arb_if;

	if (arb_if && arb_if->vm_ops.vm_arb_gpu_stopped) {
		KBASE_TLSTREAM_TL_ARBITER_STOPPED(kbdev, kbdev);
		KBASE_KTRACE_ADD(kbdev, ARB_GPU_STOPPED, NULL, 0);
		if (gpu_required) {
			KBASE_TLSTREAM_TL_ARBITER_REQUESTED(kbdev, kbdev);
			KBASE_KTRACE_ADD(kbdev, ARB_GPU_REQUESTED, NULL, 0);
		}
		arb_if->vm_ops.vm_arb_gpu_stopped(arb_if, gpu_required);
	}
}

/**
 * kbase_arbif_gpu_active() - Sends a GPU_ACTIVE message to the Arbiter
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Informs the arbiter VM is active
 */
void kbase_arbif_gpu_active(struct kbase_device *kbdev)
{
	struct arbiter_if_dev *arb_if = kbdev->arb.arb_if;

	if (arb_if && arb_if->vm_ops.vm_arb_gpu_active)
		arb_if->vm_ops.vm_arb_gpu_active(arb_if);
}

/**
 * kbase_arbif_gpu_idle() - Inform the arbiter that the VM has gone idle
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Informs the arbiter VM is idle
 */
void kbase_arbif_gpu_idle(struct kbase_device *kbdev)
{
	struct arbiter_if_dev *arb_if = kbdev->arb.arb_if;

	if (arb_if && arb_if->vm_ops.vm_arb_gpu_idle)
		arb_if->vm_ops.vm_arb_gpu_idle(arb_if);
}
