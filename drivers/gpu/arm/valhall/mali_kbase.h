/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 *
 * (C) COPYRIGHT 2010-2024 ARM Limited. All rights reserved.
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

#ifndef _KBASE_H_
#define _KBASE_H_

#include <mali_malisw.h>

#include <mali_kbase_debug.h>

#include <uapi/gpu/arm/valhall/mali_base_kernel.h>
#include <mali_kbase_linux.h>
#include <linux/version_compat_defs_for_valhall.h>

/*
 * Include mali_kbase_defs.h first as this provides types needed by other local
 * header files.
 */
#include "mali_kbase_defs.h"

#include "debug/mali_kbase_debug_ktrace.h"
#include "context/mali_kbase_context.h"
#include "mali_kbase_mem_lowlevel.h"
#include "mali_kbase_mem.h"
#include "mmu/mali_kbase_mmu.h"
#include "mali_kbase_gpu_memory_debugfs.h"
#include "mali_kbase_mem_profile_debugfs.h"
#include "mali_kbase_gpuprops.h"
#include <uapi/gpu/arm/valhall/mali_kbase_ioctl.h>
#include "csf/mali_kbase_debug_csf_fault.h"

#include "ipa/mali_kbase_ipa.h"

#include "csf/mali_kbase_csf.h"

#include "mali_linux_trace.h"

#include <linux/atomic.h>
#include <linux/highmem.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#if (KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE)
#include <linux/sched/mm.h>
#endif
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#define KBASE_DRV_NAME "mali"
#define KBASE_TIMELINE_NAME KBASE_DRV_NAME ".timeline"

/* Physical memory group ID for CSF user I/O.
 */
#define KBASE_MEM_GROUP_CSF_IO BASE_MEM_GROUP_DEFAULT

/* Physical memory group ID for CSF firmware.
 */
#define KBASE_MEM_GROUP_CSF_FW BASE_MEM_GROUP_DEFAULT

/* Physical memory group ID for a special page which can alias several regions.
 */
#define KBASE_MEM_GROUP_SINK BASE_MEM_GROUP_DEFAULT

/*
 * Kernel-side Base (KBase) APIs
 */

struct kbase_device *kbase_device_alloc(void);
/*
 * note: configuration attributes member of kbdev needs to have
 * been setup before calling kbase_device_init
 */

/**
 * kbase_device_misc_init() - Miscellaneous initialization for kbase device
 * @kbdev: Pointer to the kbase device
 *
 * This function must be called only when a kbase device is initialized.
 *
 * Return: 0 on success
 */
int kbase_device_misc_init(struct kbase_device *kbdev);
void kbase_device_misc_term(struct kbase_device *kbdev);
void kbase_device_free(struct kbase_device *kbdev);
int kbase_device_has_feature(struct kbase_device *kbdev, u32 feature);

/* Needed for gator integration and for reporting vsync information */
struct kbase_device *kbase_find_device(int minor);
void kbase_release_device(struct kbase_device *kbdev);

/**
 * kbase_context_get_unmapped_area() - get an address range which is currently
 *                                     unmapped.
 * @kctx: A kernel base context (which has its own GPU address space).
 * @addr: CPU mapped address (set to 0 since MAP_FIXED mapping is not allowed
 *        as Mali GPU driver decides about the mapping).
 * @len: Length of the address range.
 * @pgoff: Page offset within the GPU address space of the kbase context.
 * @flags: Flags for the allocation.
 *
 * Finds the unmapped address range which satisfies requirements specific to
 * GPU and those provided by the call parameters.
 *
 * 1) Requirement for allocations greater than 2MB:
 * - alignment offset is set to 2MB and the alignment mask to 2MB decremented
 * by 1.
 *
 * 2) Requirements imposed for the shader memory alignment:
 * - alignment is decided by the number of GPU pc bits which can be read from
 * GPU properties of the device associated with this kbase context; alignment
 * offset is set to this value in bytes and the alignment mask to the offset
 * decremented by 1.
 * - allocations must not to be at 4GB boundaries. Such cases are indicated
 * by the flag KBASE_REG_GPU_NX not being set (check the flags of the kbase
 * region). 4GB boundaries can be checked against @ref BASE_MEM_MASK_4GB.
 *
 * 3) Requirements imposed for tiler memory alignment, cases indicated by
 * the flag @ref KBASE_REG_TILER_ALIGN_TOP (check the flags of the kbase
 * region):
 * - alignment offset is set to the difference between the kbase region
 * extension (converted from the original value in pages to bytes) and the kbase
 * region initial_commit (also converted from the original value in pages to
 * bytes); alignment mask is set to the kbase region extension in bytes and
 * decremented by 1.
 *
 * Return: if successful, address of the unmapped area aligned as required;
 *         error code (negative) in case of failure;
 */
unsigned long kbase_context_get_unmapped_area(struct kbase_context *kctx, const unsigned long addr,
					      const unsigned long len, const unsigned long pgoff,
					      const unsigned long flags);

/**
 * kbase_get_irqs() - Get GPU interrupts from the device tree.
 *
 * @kbdev: The kbase device structure of the device
 *
 * This function must be called once only when a kbase device is initialized.
 *
 * Return: 0 on success. Error code (negative) on failure.
 */
int kbase_get_irqs(struct kbase_device *kbdev);

int kbase_sysfs_init(struct kbase_device *kbdev);
void kbase_sysfs_term(struct kbase_device *kbdev);

/**
 * kbase_protected_mode_init() - Initialize kbase device for protected mode.
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * This function must be called only when a kbase device is initialized.
 *
 * Return: 0 on success.
 */
int kbase_protected_mode_init(struct kbase_device *kbdev);
void kbase_protected_mode_term(struct kbase_device *kbdev);

/**
 * kbase_device_backend_init() - Performs backend initialization and performs
 * devicetree validation.
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 * If -EPERM is returned, it means the device backend is not supported, but
 * device initialization can continue.
 */
int kbase_device_backend_init(struct kbase_device *kbdev);

/**
 * kbase_device_backend_term() - Performs backend deinitialization and free
 * resources.
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Clean up all the resources
 */
void kbase_device_backend_term(struct kbase_device *kbdev);

int power_control_init(struct kbase_device *kbdev);
void power_control_term(struct kbase_device *kbdev);

#if IS_ENABLED(CONFIG_DEBUG_FS)
void kbase_device_debugfs_term(struct kbase_device *kbdev);
int kbase_device_debugfs_init(struct kbase_device *kbdev);
#else /* CONFIG_DEBUG_FS */
static inline int kbase_device_debugfs_init(struct kbase_device *kbdev)
{
	return 0;
}

static inline void kbase_device_debugfs_term(struct kbase_device *kbdev)
{
}
#endif /* CONFIG_DEBUG_FS */

int registers_map(struct kbase_device *kbdev);
void registers_unmap(struct kbase_device *kbdev);

int kbase_device_coherency_init(struct kbase_device *kbdev);


void kbase_event_post(struct kbase_context *kctx, struct kbase_jd_atom *event);
int kbase_event_pending(struct kbase_context *kctx);
int kbase_event_init(struct kbase_context *kctx);
void kbase_event_close(struct kbase_context *kctx);
void kbase_event_cleanup(struct kbase_context *kctx);
void kbase_event_wakeup(struct kbase_context *kctx);

/**
 * kbasep_jit_alloc_validate() - Validate the JIT allocation info.
 *
 * @kctx:	Pointer to the kbase context within which the JIT
 *		allocation is to be validated.
 * @info:	Pointer to struct @base_jit_alloc_info
 *			which is to be validated.
 * Return: 0 if jit allocation is valid; negative error code otherwise
 */
int kbasep_jit_alloc_validate(struct kbase_context *kctx, struct base_jit_alloc_info *info);

/**
 * kbase_jit_retry_pending_alloc() - Retry blocked just-in-time memory
 *                                   allocations.
 *
 * @kctx:	Pointer to the kbase context within which the just-in-time
 *		memory allocations are to be retried.
 */
void kbase_jit_retry_pending_alloc(struct kbase_context *kctx);

/**
 * kbase_free_user_buffer() - Free memory allocated for struct
 *		@kbase_debug_copy_buffer.
 *
 * @buffer:	Pointer to the memory location allocated for the object
 *		of the type struct @kbase_debug_copy_buffer.
 */
static inline void kbase_free_user_buffer(struct kbase_debug_copy_buffer *buffer)
{
	struct page **pages = buffer->extres_pages;
	uint nr_pages = buffer->nr_extres_pages;

	if (pages) {
		uint i;

		for (i = 0; i < nr_pages; i++) {
			struct page *pg = pages[i];

			if (pg)
				put_page(pg);
		}
		kfree(pages);
	}
}

void kbasep_as_do_poke(struct work_struct *work);

/**
 * kbase_pm_is_suspending - Check whether a system suspend is in progress,
 * or has already been suspended
 *
 * @kbdev: The kbase device structure for the device
 *
 * The caller should ensure that either kbase_device::kbase_pm_device_data::lock is held,
 * or a dmb was executed recently (to ensure the value is most up-to-date).
 * However, without a lock the value could change afterwards.
 *
 * Return: False if a suspend is not in progress, true otherwise,
 */
static inline bool kbase_pm_is_suspending(struct kbase_device *kbdev)
{
	return kbdev->pm.suspending;
}

/**
 * kbase_pm_is_resuming - Check whether System resume of GPU device is in progress.
 *
 * @kbdev: The kbase device structure for the device
 *
 * The caller should ensure that either kbase_device::kbase_pm_device_data::lock is held,
 * or a dmb was executed recently (to ensure the value is most up-to-date).
 * However, without a lock the value could change afterwards.
 *
 * Return: true if System resume is in progress, otherwise false.
 */
static inline bool kbase_pm_is_resuming(struct kbase_device *kbdev)
{
	return kbdev->pm.resuming;
}

/**
 * kbase_pm_is_active - Determine whether the GPU is active
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * This takes into account whether there is an active context reference.
 *
 * Return: true if the GPU is active, false otherwise
 */
static inline bool kbase_pm_is_active(struct kbase_device *kbdev)
{
	return kbdev->pm.active_count > 0;
}

/**
 * kbase_pm_gpu_freq_init() - Find the lowest frequency that the GPU can
 *                            run as using the device tree, then query the
 *                            GPU properties to find out the highest GPU
 *                            frequency and store both of them within the
 *                            @kbase_device.
 * @kbdev: Pointer to kbase device.
 *
 * This function could be called from kbase_clk_rate_trace_manager_init,
 * but is left separate as it can be called as soon as
 * dev_pm_opp_of_add_table() has been called to initialize the OPP table,
 * which occurs in power_control_init().
 *
 * Return: 0 on success, negative error code on failure.
 */
int kbase_pm_gpu_freq_init(struct kbase_device *kbdev);

/**
 * kbase_pm_metrics_start - Start the utilization metrics timer
 * @kbdev: Pointer to the kbase device for which to start the utilization
 *         metrics calculation thread.
 *
 * Start the timer that drives the metrics calculation, runs the custom DVFS.
 */
void kbase_pm_metrics_start(struct kbase_device *kbdev);

/**
 * kbase_pm_metrics_stop - Stop the utilization metrics timer
 * @kbdev: Pointer to the kbase device for which to stop the utilization
 *         metrics calculation thread.
 *
 * Stop the timer that drives the metrics calculation, runs the custom DVFS.
 */
void kbase_pm_metrics_stop(struct kbase_device *kbdev);

/**
 * kbase_pm_handle_runtime_suspend - Handle the runtime suspend of GPU
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * This function is called from the runtime suspend callback function for
 * saving the HW state and powering down GPU, if GPU was in sleep state mode.
 * It does the following steps
 * - Powers up the L2 cache and re-activates the MCU.
 * - Suspend the CSGs
 * - Halts the MCU
 * - Powers down the L2 cache.
 * - Invokes the power_off callback to power down the GPU.
 *
 * Return: 0 if the GPU was already powered down or no error was encountered
 * in the power down, otherwise an error code.
 */
int kbase_pm_handle_runtime_suspend(struct kbase_device *kbdev);

/**
 * kbase_pm_force_mcu_wakeup_after_sleep - Force the wake up of MCU from sleep
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * This function forces the wake up of MCU from sleep state and wait for
 * MCU to become active.
 * It usually gets called from the runtime suspend callback function.
 * It also gets called from the GPU reset handler or at the time of system
 * suspend or when User tries to terminate/suspend the on-slot group.
 *
 * Note: @gpu_wakeup_override flag that forces the reactivation of MCU is
 *       set by this function and it is the caller's responsibility to
 *       clear the flag.
 *
 * Return: 0 if the wake up was successful.
 */
int kbase_pm_force_mcu_wakeup_after_sleep(struct kbase_device *kbdev);

/**
 * kbase_disjoint_init - Initialize the disjoint state
 *
 * @kbdev: The kbase device
 *
 * The disjoint event count and state are both set to zero.
 *
 * Disjoint functions usage:
 *
 * The disjoint event count should be incremented whenever a disjoint event occurs.
 *
 * There are several cases which are regarded as disjoint behavior. Rather than just increment
 * the counter during disjoint events we also increment the counter when jobs may be affected
 * by what the GPU is currently doing. To facilitate this we have the concept of disjoint state.
 *
 * Disjoint state is entered during GPU reset. Increasing the disjoint state also increases
 * the count of disjoint events.
 *
 * The disjoint state is then used to increase the count of disjoint events during job submission
 * and job completion. Any atom submitted or completed while the disjoint state is greater than
 * zero is regarded as a disjoint event.
 *
 * The disjoint event counter is also incremented immediately whenever a job is soft stopped
 * and during context creation.
 *
 * This function must be called only when a kbase device is initialized.
 *
 * Return: 0 on success and non-zero value on failure.
 */
void kbase_disjoint_init(struct kbase_device *kbdev);

/**
 * kbase_disjoint_event - Increase the count of disjoint events
 * called when a disjoint event has happened
 *
 * @kbdev: The kbase device
 */
void kbase_disjoint_event(struct kbase_device *kbdev);

/**
 * kbase_disjoint_event_potential - Increase the count of disjoint events
 * only if the GPU is in a disjoint state
 *
 * @kbdev: The kbase device
 *
 * This should be called when something happens which could be disjoint if the GPU
 * is in a disjoint state. The state refcount keeps track of this.
 */
void kbase_disjoint_event_potential(struct kbase_device *kbdev);

/**
 * kbase_disjoint_event_get - Returns the count of disjoint events
 *
 * @kbdev: The kbase device
 * Return: the count of disjoint events
 */
u32 kbase_disjoint_event_get(struct kbase_device *kbdev);

/**
 * kbase_disjoint_state_up - Increment the refcount state indicating that
 * the GPU is in a disjoint state.
 *
 * @kbdev: The kbase device
 *
 * Also Increment the disjoint event count (calls @ref kbase_disjoint_event)
 * eventually after the disjoint state has completed @ref kbase_disjoint_state_down
 * should be called
 */
void kbase_disjoint_state_up(struct kbase_device *kbdev);

/**
 * kbase_disjoint_state_down - Decrement the refcount state
 *
 * @kbdev: The kbase device
 *
 * Also Increment the disjoint event count (calls @ref kbase_disjoint_event)
 *
 * Called after @ref kbase_disjoint_state_up once the disjoint state is over
 */
void kbase_disjoint_state_down(struct kbase_device *kbdev);

/**
 * kbase_device_pcm_dev_init() - Initialize the priority control manager device
 *
 * @kbdev: Pointer to the structure for the kbase device
 *
 * Pointer to the priority control manager device is retrieved from the device
 * tree and a reference is taken on the module implementing the callbacks for
 * priority control manager operations.
 *
 * Return: 0 if successful, or an error code on failure
 */
int kbase_device_pcm_dev_init(struct kbase_device *const kbdev);

/**
 * kbase_device_pcm_dev_term() - Performs priority control manager device
 *                               deinitialization.
 *
 * @kbdev: Pointer to the structure for the kbase device
 *
 * Reference is released on the module implementing the callbacks for priority
 * control manager operations.
 */
void kbase_device_pcm_dev_term(struct kbase_device *const kbdev);

/**
 * kbasep_adjust_prioritized_process() - Adds or removes the specified PID from
 *                                       the list of prioritized processes.
 *
 * @kbdev: Pointer to the structure for the kbase device
 * @add: True if the process should be prioritized, false otherwise
 * @tgid: The process/thread group ID
 *
 * Return: true if the operation was successful, false otherwise
 */
bool kbasep_adjust_prioritized_process(struct kbase_device *kbdev, bool add, uint32_t tgid);

/**
 * KBASE_DISJOINT_STATE_INTERLEAVED_CONTEXT_COUNT_THRESHOLD - If a job is soft stopped
 * and the number of contexts is >= this value it is reported as a disjoint event
 */
#define KBASE_DISJOINT_STATE_INTERLEAVED_CONTEXT_COUNT_THRESHOLD 2

#if !defined(UINT64_MAX)
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#endif

#if !defined(UINT32_MAX)
#define UINT32_MAX ((uint32_t)0xFFFFFFFFU)
#endif

#endif
