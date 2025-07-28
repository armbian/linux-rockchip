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

/*
 * Metrics for power management
 */

#include <mali_kbase.h>
#include <mali_kbase_config_defaults.h>
#include <mali_kbase_pm.h>
#include <backend/gpu/mali_kbase_pm_internal.h>

#include "backend/gpu/mali_kbase_clk_rate_trace_mgr.h"
#include <csf/ipa_control/mali_kbase_csf_ipa_control.h>

#include <backend/gpu/mali_kbase_pm_defs.h>
#include <mali_linux_trace.h>

#if defined(CONFIG_MALI_VALHALL_DEVFREQ) || defined(CONFIG_MALI_VALHALL_DVFS)
/* Shift used for kbasep_pm_metrics_data.time_busy/idle - units of (1 << 8) ns
 * This gives a maximum period between samples of 2^(32+8)/100 ns = slightly
 * under 11s. Exceeding this will cause overflow
 */
#define KBASE_PM_TIME_SHIFT 8
#endif

/* To get the GPU_ACTIVE value in nano seconds unit */
#define GPU_ACTIVE_SCALING_FACTOR ((u64)1E9)

/*
 * Possible state transitions
 * ON        -> ON | OFF | STOPPED
 * STOPPED   -> ON | OFF
 * OFF       -> ON
 *
 *
 * ┌─e─┐┌────────────f─────────────┐
 * │   v│                          v
 * └───ON ──a──> STOPPED ──b──> OFF
 *     ^^            │             │
 *     │└──────c─────┘             │
 *     │                           │
 *     └─────────────d─────────────┘
 *
 * Transition effects:
 * a. None
 * b. Timer expires without restart
 * c. Timer is not stopped, timer period is unaffected
 * d. Timer must be restarted
 * e. Callback is executed and the timer is restarted
 * f. Timer is cancelled, or the callback is waited on if currently executing. This is called during
 *    tear-down and should not be subject to a race from an OFF->ON transition
 */
enum dvfs_metric_timer_state { TIMER_OFF, TIMER_STOPPED, TIMER_ON };

#ifdef CONFIG_MALI_VALHALL_DVFS
static enum hrtimer_restart dvfs_callback(struct hrtimer *timer)
{
	struct kbasep_pm_metrics_state *metrics;

	if (WARN_ON(!timer))
		return HRTIMER_NORESTART;

	metrics = container_of(timer, struct kbasep_pm_metrics_state, timer);

	/* Transition (b) to fully off if timer was stopped, don't restart the timer in this case */
	if (atomic_cmpxchg(&metrics->timer_state, TIMER_STOPPED, TIMER_OFF) != TIMER_ON)
		return HRTIMER_NORESTART;

	kbase_pm_get_dvfs_action(metrics->kbdev);

	/* Set the new expiration time and restart (transition e) */
	hrtimer_forward_now(timer, HR_TIMER_DELAY_MSEC(metrics->kbdev->pm.dvfs_period));
	return HRTIMER_RESTART;
}
#endif /* CONFIG_MALI_VALHALL_DVFS */

int kbasep_pm_metrics_init(struct kbase_device *kbdev)
{
	struct kbase_ipa_control_perf_counter perf_counter;
	int err;

	/* One counter group */
	const size_t NUM_PERF_COUNTERS = 1;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	kbdev->pm.backend.metrics.kbdev = kbdev;
	kbdev->pm.backend.metrics.time_period_start = ktime_get_raw();

	perf_counter.scaling_factor = GPU_ACTIVE_SCALING_FACTOR;

	/* Normalize values by GPU frequency */
	perf_counter.gpu_norm = true;

	/* We need the GPU_ACTIVE counter, which is in the CSHW group */
	perf_counter.type = KBASE_IPA_CORE_TYPE_CSHW;

	/* We need the GPU_ACTIVE counter */
	perf_counter.idx = GPU_ACTIVE_CNT_IDX;

	err = kbase_ipa_control_register(kbdev, &perf_counter, NUM_PERF_COUNTERS,
					 &kbdev->pm.backend.metrics.ipa_control_client);
	if (err) {
		dev_err(kbdev->dev, "Failed to register IPA with kbase_ipa_control: err=%d", err);
		return -1;
	}
	spin_lock_init(&kbdev->pm.backend.metrics.lock);

#ifdef CONFIG_MALI_VALHALL_DVFS
	hrtimer_init(&kbdev->pm.backend.metrics.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	kbdev->pm.backend.metrics.timer.function = dvfs_callback;
	kbdev->pm.backend.metrics.initialized = true;
	atomic_set(&kbdev->pm.backend.metrics.timer_state, TIMER_OFF);
	kbase_pm_metrics_start(kbdev);
#endif /* CONFIG_MALI_VALHALL_DVFS */

	/* The sanity check on the GPU_ACTIVE performance counter
	 * is skipped for Juno platforms that have timing problems.
	 */
	kbdev->pm.backend.metrics.skip_gpu_active_sanity_check =
		(kbdev->gpu_props.impl_tech >= THREAD_FEATURES_IMPLEMENTATION_TECHNOLOGY_FPGA);

	return 0;
}
KBASE_EXPORT_TEST_API(kbasep_pm_metrics_init);

void kbasep_pm_metrics_term(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_VALHALL_DVFS
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	/* Cancel the timer, and block if the callback is currently executing (transition f) */
	kbdev->pm.backend.metrics.initialized = false;
	atomic_set(&kbdev->pm.backend.metrics.timer_state, TIMER_OFF);
	hrtimer_cancel(&kbdev->pm.backend.metrics.timer);
#endif /* CONFIG_MALI_VALHALL_DVFS */

	kbase_ipa_control_unregister(kbdev, kbdev->pm.backend.metrics.ipa_control_client);
}

KBASE_EXPORT_TEST_API(kbasep_pm_metrics_term);

/* caller needs to hold kbdev->pm.backend.metrics.lock before calling this
 * function
 */
#if defined(CONFIG_MALI_VALHALL_DEVFREQ) || defined(CONFIG_MALI_VALHALL_DVFS)
static void kbase_pm_get_dvfs_utilisation_calc(struct kbase_device *kbdev)
{
	int err;
	u64 gpu_active_counter;
	u64 protected_time;
	ktime_t now;

	lockdep_assert_held(&kbdev->pm.backend.metrics.lock);

	/* Query IPA_CONTROL for the latest GPU-active and protected-time
	 * info.
	 */
	err = kbase_ipa_control_query(kbdev, kbdev->pm.backend.metrics.ipa_control_client,
				      &gpu_active_counter, 1, &protected_time);

	/* Read the timestamp after reading the GPU_ACTIVE counter value.
	 * This ensures the time gap between the 2 reads is consistent for
	 * a meaningful comparison between the increment of GPU_ACTIVE and
	 * elapsed time. The lock taken inside kbase_ipa_control_query()
	 * function can cause lot of variation.
	 */
	now = ktime_get_raw();

	if (err) {
		dev_err(kbdev->dev, "Failed to query the increment of GPU_ACTIVE counter: err=%d",
			err);
	} else {
		u64 diff_ns;
		s64 diff_ns_signed;
		u32 ns_time;
		ktime_t diff = ktime_sub(now, kbdev->pm.backend.metrics.time_period_start);

		diff_ns_signed = ktime_to_ns(diff);

		if (diff_ns_signed < 0)
			return;

		diff_ns = (u64)diff_ns_signed;

#if !IS_ENABLED(CONFIG_MALI_VALHALL_NO_MALI)
		/* The GPU_ACTIVE counter shouldn't clock-up more time than has
		 * actually elapsed - but still some margin needs to be given
		 * when doing the comparison. There could be some drift between
		 * the CPU and GPU clock.
		 *
		 * Can do the check only in a real driver build, as an arbitrary
		 * value for GPU_ACTIVE can be fed into dummy model in no_mali
		 * configuration which may not correspond to the real elapsed
		 * time.
		 */
		if (!kbdev->pm.backend.metrics.skip_gpu_active_sanity_check) {
			/* The margin is scaled to allow for the worst-case
			 * scenario where the samples are maximally separated,
			 * plus a small offset for sampling errors.
			 */
			u64 const MARGIN_NS =
				IPA_CONTROL_TIMER_DEFAULT_VALUE_MS * NSEC_PER_MSEC * 3 / 2;

			if (gpu_active_counter > (diff_ns + MARGIN_NS)) {
				dev_info(
					kbdev->dev,
					"GPU activity takes longer than time interval: %llu ns > %llu ns",
					(unsigned long long)gpu_active_counter,
					(unsigned long long)diff_ns);
			}
		}
#endif
		/* Calculate time difference in units of 256ns */
		ns_time = (u32)(diff_ns >> KBASE_PM_TIME_SHIFT);

		/* Add protected_time to gpu_active_counter so that time in
		 * protected mode is included in the apparent GPU active time,
		 * then convert it from units of 1ns to units of 256ns, to
		 * match what JM GPUs use. The assumption is made here that the
		 * GPU is 100% busy while in protected mode, so we should add
		 * this since the GPU can't (and thus won't) update these
		 * counters while it's actually in protected mode.
		 *
		 * Perform the add after dividing each value down, to reduce
		 * the chances of overflows.
		 */
		protected_time >>= KBASE_PM_TIME_SHIFT;
		gpu_active_counter >>= KBASE_PM_TIME_SHIFT;
		gpu_active_counter += protected_time;

		/* Ensure the following equations don't go wrong if ns_time is
		 * slightly larger than gpu_active_counter somehow
		 */
		gpu_active_counter = MIN(gpu_active_counter, ns_time);

		kbdev->pm.backend.metrics.values.time_busy += gpu_active_counter;

		kbdev->pm.backend.metrics.values.time_idle += ns_time - gpu_active_counter;

		/* Also make time in protected mode available explicitly,
		 * so users of this data have this info, too.
		 */
		kbdev->pm.backend.metrics.values.time_in_protm += protected_time;
	}

	kbdev->pm.backend.metrics.time_period_start = now;
}
#endif /* defined(CONFIG_MALI_VALHALL_DEVFREQ) || defined(CONFIG_MALI_VALHALL_DVFS) */

#if defined(CONFIG_MALI_VALHALL_DEVFREQ) || defined(CONFIG_MALI_VALHALL_DVFS)
void kbase_pm_get_dvfs_metrics(struct kbase_device *kbdev, struct kbasep_pm_metrics *last,
			       struct kbasep_pm_metrics *diff)
{
	struct kbasep_pm_metrics *cur = &kbdev->pm.backend.metrics.values;
	unsigned long flags;

	spin_lock_irqsave(&kbdev->pm.backend.metrics.lock, flags);
	kbase_pm_get_dvfs_utilisation_calc(kbdev);

	memset(diff, 0, sizeof(*diff));
	diff->time_busy = cur->time_busy - last->time_busy;
	diff->time_idle = cur->time_idle - last->time_idle;

	diff->time_in_protm = cur->time_in_protm - last->time_in_protm;

	*last = *cur;

	spin_unlock_irqrestore(&kbdev->pm.backend.metrics.lock, flags);
}
KBASE_EXPORT_TEST_API(kbase_pm_get_dvfs_metrics);
#endif

#ifdef CONFIG_MALI_VALHALL_DVFS
void kbase_pm_get_dvfs_action(struct kbase_device *kbdev)
{
	int utilisation;
	struct kbasep_pm_metrics *diff;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	diff = &kbdev->pm.backend.metrics.dvfs_diff;

	kbase_pm_get_dvfs_metrics(kbdev, &kbdev->pm.backend.metrics.dvfs_last, diff);

	utilisation = (100 * diff->time_busy) / max(diff->time_busy + diff->time_idle, 1u);

	/* Note that, at present, we don't pass protected-mode time to the
	 * platform here. It's unlikely to be useful, however, as the platform
	 * probably just cares whether the GPU is busy or not; time in
	 * protected mode is already added to busy-time at this point, though,
	 * so we should be good.
	 */
	kbase_platform_dvfs_event(kbdev, utilisation);
}

bool kbase_pm_metrics_is_active(struct kbase_device *kbdev)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	return atomic_read(&kbdev->pm.backend.metrics.timer_state) == TIMER_ON;
}
KBASE_EXPORT_TEST_API(kbase_pm_metrics_is_active);

void kbase_pm_metrics_start(struct kbase_device *kbdev)
{
	struct kbasep_pm_metrics_state *metrics = &kbdev->pm.backend.metrics;

	if (unlikely(!metrics->initialized))
		return;

	/* Transition to ON, from a stopped state (transition c) */
	if (atomic_xchg(&metrics->timer_state, TIMER_ON) == TIMER_OFF)
		/* Start the timer only if it's been fully stopped (transition d)*/
		hrtimer_start(&metrics->timer, HR_TIMER_DELAY_MSEC(kbdev->pm.dvfs_period),
			      HRTIMER_MODE_REL);
}

void kbase_pm_metrics_stop(struct kbase_device *kbdev)
{
	if (unlikely(!kbdev->pm.backend.metrics.initialized))
		return;

	/* Timer is Stopped if its currently on (transition a) */
	atomic_cmpxchg(&kbdev->pm.backend.metrics.timer_state, TIMER_ON, TIMER_STOPPED);
}

#endif /* CONFIG_MALI_VALHALL_DVFS */
