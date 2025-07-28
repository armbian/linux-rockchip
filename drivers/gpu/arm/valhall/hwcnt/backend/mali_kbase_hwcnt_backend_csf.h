/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 *
 * (C) COPYRIGHT 2021-2024 ARM Limited. All rights reserved.
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
 * Concrete implementation of mali_kbase_hwcnt_backend interface for CSF
 * backend.
 */

#ifndef _KBASE_HWCNT_BACKEND_CSF_H_
#define _KBASE_HWCNT_BACKEND_CSF_H_

#include "hwcnt/backend/mali_kbase_hwcnt_backend.h"
#include "hwcnt/backend/mali_kbase_hwcnt_backend_csf_if.h"
#include "hwcnt/mali_kbase_hwcnt_watchdog_if.h"
#include "hwcnt/mali_kbase_hwcnt_types.h"
#include <hwcnt/mali_kbase_hwcnt_context.h>

struct kbase_hwcnt_physical_enable_map;
struct kbase_hwcnt_backend_csf;

/**
 * enum kbase_hwcnt_backend_sample_reason - HWC CSF metadata block sample
 *                                          reasons.
 *
 * @SAMPLE_REASON_NONE:            No metadata was enabled or available.
 * @SAMPLE_REASON_BEFORE_PROTM:    The sample was taken just before the GPU enters protected mode.
 * @SAMPLE_REASON_BEFORE_HALT:     The sample was taken just before the GPU enters the HALT state.
 * @SAMPLE_REASON_BEFORE_SLEEP:    The sample was taken just before the GPU enters the SLEEP state.
 * @SAMPLE_REASON_BEFORE_SUSPEND:  The sample was taken just before the GPU enters the SUSPEND
 *                                 state.
 * @SAMPLE_REASON_BEFORE_YIELD:    The sample was taken just before GPU subinstance yields the
 *                                 access window.
 * @SAMPLE_REASON_AFTER_WARM_BOOT: The sample taken after GPU subinstance completed a warm boot.
 */
enum kbase_hwcnt_backend_sample_reason {
	SAMPLE_REASON_NONE,
	SAMPLE_REASON_BEFORE_PROTM,
	SAMPLE_REASON_BEFORE_HALT,
	SAMPLE_REASON_BEFORE_SLEEP,
	SAMPLE_REASON_BEFORE_SUSPEND,
	SAMPLE_REASON_BEFORE_YIELD,
	SAMPLE_REASON_AFTER_WARM_BOOT
};

/**
 * kbase_hwcnt_backend_csf_create() - Create a CSF hardware counter backend
 *                                    interface, with set timer interval.
 * @csf_if:       Non-NULL pointer to a hwcnt backend CSF interface structure
 *                used to create backend interface.
 * @ring_buf_cnt: The buffer count of CSF hwcnt backend, used when allocate ring
 *                buffer, MUST be power of 2.
 * @watchdog_if:  Non-NULL pointer to a hwcnt watchdog interface structure used
 *                to create backend interface.
 * @iface:        Non-NULL pointer to backend interface structure that is filled
 *                in on creation success.
 * @watchdog_timer_interval_ms: Interval in milliseconds between hwcnt samples.
 *
 * Calls to iface->dump_enable_nolock() require the CSF Scheduler IRQ lock.
 *
 * Return: 0 on success, else error code.
 */
int kbase_hwcnt_backend_csf_create(struct kbase_hwcnt_backend_csf_if *csf_if, u32 ring_buf_cnt,
				   struct kbase_hwcnt_watchdog_interface *watchdog_if,
				   struct kbase_hwcnt_backend_interface *iface,
				   u32 watchdog_timer_interval_ms);

/**
 * kbase_hwcnt_backend_csf_metadata_init() - Initialize the metadata for a CSF
 *                                           hardware counter backend.
 * @iface: Non-NULL pointer to backend interface structure
 * Return: 0 on success, else error code.
 */
int kbase_hwcnt_backend_csf_metadata_init(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_metadata_term() - Terminate the metadata for a CSF
 *                                           hardware counter backend.
 * @iface: Non-NULL pointer to backend interface structure.
 */
void kbase_hwcnt_backend_csf_metadata_term(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_ring_buf_term() - Terminate the ring buffers for a CSF
 *                                           hardware counter backend. Must be called while
 *                                           FW is still loaded.
 *
 * @iface: Non-NULL pointer to backend interface structure.
 *
 * If the ring buffer was not freed while HWC backend was present
 * (e.g. due to MCU being powered off), this function will free it.
 */
void kbase_hwcnt_backend_csf_ring_buf_term(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_destroy() - Destroy a CSF hardware counter backend
 *                                     interface.
 * @iface: Pointer to interface to destroy.
 *
 * Can be safely called on an all-zeroed interface, or on an already destroyed
 * interface.
 */
void kbase_hwcnt_backend_csf_destroy(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_protm_entered() - CSF HWC backend function to receive
 *                                           notification that protected mode
 *                                           has been entered.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_protm_entered(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_protm_exited() - CSF HWC backend function to receive
 *                                          notification that protected mode has
 *                                          been exited.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_protm_exited(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_unrecoverable_error() - CSF HWC backend function
 *                                                    called when unrecoverable
 *                                                    errors are detected.
 * @iface: Non-NULL pointer to HWC backend interface.
 *
 * This should be called on encountering errors that can only be recovered from
 * with reset, or that may put HWC logic in state that could result in hang. For
 * example, on bus error, or when FW becomes unresponsive.
 */
void kbase_hwcnt_backend_csf_on_unrecoverable_error(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_before_reset() - CSF HWC backend function to be
 *                                             called immediately before a
 *                                             reset. Takes us out of the
 *                                             unrecoverable error state, if we
 *                                             were in it.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_before_reset(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_set_hw_availability() - CSF HWC backend function to
 *                                                 set current HW configuration.
 *                                                 Can only be called when the MCU is off.
 *
 * @iface: Non-NULL pointer to HWC backend interface.
 * @num_l2_slices: Current number of L2 slices allocated to the GPU.
 * @shader_present: Shader_present of the current configuration.
 * @power_core_mask: Mask containing changed shader core power state.
 */
void kbase_hwcnt_backend_csf_set_hw_availability(struct kbase_hwcnt_backend_interface *iface,
						 size_t num_l2_slices, u64 shader_present,
						 u64 power_core_mask);

/** kbasep_hwcnt_backend_csf_process_enable_map() - Process the enable_map to
 *                                                  guarantee headers are
 *                                                  enabled if any counter is
 *                                                  required.
 * @phys_enable_map: HWC physical enable map to be processed.
 */
void kbasep_hwcnt_backend_csf_process_enable_map(
	struct kbase_hwcnt_physical_enable_map *phys_enable_map);

/**
 * kbase_hwcnt_backend_csf_on_prfcnt_sample() - CSF performance counter sample
 *                                              complete interrupt handler.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_prfcnt_sample(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_prfcnt_threshold() - CSF performance counter
 *                                                 buffer reach threshold
 *                                                 interrupt handler.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_prfcnt_threshold(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_prfcnt_overflow() - CSF performance counter buffer
 *                                                overflow interrupt handler.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_prfcnt_overflow(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_prfcnt_enable() - CSF performance counter enabled
 *                                              interrupt handler.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_prfcnt_enable(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_prfcnt_disable() - CSF performance counter
 *                                               disabled interrupt handler.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_prfcnt_disable(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_after_mcu_off() - CSF HWC backend function to be called immediately
 *                                              after the MCU shut down process is completed,
 *                                              informing the backend that it is no longer valid
 *                                              to send commands to the MCU and that any
 *                                              outstanding commands will not be ACKed.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_after_mcu_off(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_after_mcu_off_reset() - Similar to
 *                                              kbase_hwcnt_backend_csf_on_after_mcu_off().
 *                                              To be called only after a completed reset.
 *                                              This function will notify backend about MCU
 *                                              being powered off only if it hasn't been
 *                                              done yet. This is due to the fact that it's
 *                                              uncertain if the MCU_OFF notification was
 *                                              raised prior to the reset.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_after_mcu_off_reset(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_after_mcu_on() - CSF HWC backend function to be called immediately
 *                                             after the MCU has booted, informing the backend
 *                                             that it is now valid to send commands to the MCU.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_after_mcu_on(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbase_hwcnt_backend_csf_on_before_mcu_cold_boot() - CSF HWC backend function to be called
 *                                                     immediately before a cold MCU boot,
 *                                                     allowing the backend to reset its internal
 *                                                     state machine to match the cold-booted
 *                                                     FW state.
 * @iface: Non-NULL pointer to HWC backend interface.
 */
void kbase_hwcnt_backend_csf_on_before_mcu_cold_boot(struct kbase_hwcnt_backend_interface *iface);

/**
 * kbasep_hwcnt_backend_csf_update_block_state - Update block state of a block instance with
 *                              information from a sample.
 * @backend:                    CSF hardware counter backend.
 * @enable_mask:                Counter enable mask for the block whose state is being updated.
  * @block_idx:                  Index of the block in the dump.
 * @block_state:                Pointer to existing block state of the block whose state is being
 *                              updated.
 * @prev_sample_reason:         The previous sample reason
 * @curr_sample_reason:         The current sample reason
 */
void kbasep_hwcnt_backend_csf_update_block_state(
	struct kbase_hwcnt_backend_csf *backend, const u32 enable_mask, size_t block_idx,
	blk_stt_t *const block_state, enum kbase_hwcnt_backend_sample_reason prev_sample_reason,
	enum kbase_hwcnt_backend_sample_reason curr_sample_reason);

#endif /* _KBASE_HWCNT_BACKEND_CSF_H_ */
