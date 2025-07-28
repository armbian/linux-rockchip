// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2013-2025 ARM Limited. All rights reserved.
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
 * Base kernel core availability APIs
 */

#include <mali_kbase.h>
#include <mali_kbase_pm.h>
#include <backend/gpu/mali_kbase_pm_internal.h>
#include <backend/gpu/mali_kbase_model_linux.h>

#ifdef CONFIG_MALI_VALHALL_DEVFREQ
static void pm_init_cores_enabled_mask(struct kbase_device *kbdev)
{
	struct kbase_pm_backend_data *pm_backend = &kbdev->pm.backend;

	if (kbdev->current_core_mask)
		pm_backend->ca_cores_enabled = kbdev->current_core_mask;
	else
		pm_backend->ca_cores_enabled = kbdev->gpu_props.shader_present;
}

static void pm_init_gov_cores_enabled_mask(struct kbase_device *kbdev)
{
	struct kbase_pm_backend_data *pm_backend = &kbdev->pm.backend;

	if (kbdev->current_core_mask)
		pm_backend->ca_gov_cores_enabled = kbdev->current_core_mask;
	else
		pm_backend->ca_gov_cores_enabled = kbdev->gpu_props.shader_present;
}
#endif

int kbase_pm_ca_init(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_VALHALL_DEVFREQ
	if (kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT))
		pm_init_gov_cores_enabled_mask(kbdev);

	pm_init_cores_enabled_mask(kbdev);
#endif
	return 0;
}

void kbase_pm_ca_term(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

void kbase_pm_ca_set_gov_core_mask_nolock(struct kbase_device *kbdev, enum mask_type core_mask_type,
					  u64 core_mask)
{
	struct kbase_pm_backend_data *pm_backend = &kbdev->pm.backend;

	if (!kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT)) {
		dev_warn(
			kbdev->dev,
			"This function requires Kbase to have access to GOV_CORE_MASK register, cannot proceed\n");
		return;
	}

	lockdep_assert_held(&kbdev->hwaccess_lock);

	/** A value of ZERO means disabling.
	 * When disabling, store the last used mask for re-enabling
	 */
	if (core_mask_type == SYSFS_COREMASK) {
		if (core_mask != 0)
			pm_backend->ca_gov_cores_enabled = core_mask;
		else
#ifdef CONFIG_MALI_VALHALL_DEVFREQ
			pm_backend->ca_gov_cores_enabled = kbdev->current_core_mask;
#else
			pm_backend->ca_gov_cores_enabled =
				kbdev->gpu_props.curr_config.shader_present;
#endif
	}
#ifdef CONFIG_MALI_VALHALL_DEVFREQ
	/* sysfs core mask takes priority over OPP mask when sysfs core mask is set */
	else if (core_mask_type == DEVFREQ_COREMASK) {
		if (core_mask == 0) {
			dev_warn(kbdev->dev,
				 "Required core_mask cannot be zero when sysfs usage disabled\n");
			return;
		}
		/* if sysfs non-zero then no need to re-write value */
		if (kbdev->pm.sysfs_gov_core_mask)
			return;

		pm_backend->ca_gov_cores_enabled = core_mask;
	}
#endif
	/** after all checks, write the to GOV_CORE_MASK register if GPU powered,
	 * otherwise value will be applied on next reboot.
	 */
	if (kbase_io_is_gpu_powered(kbdev))
		kbase_reg_write64(kbdev, GPU_GOVERNOR_ENUM(GOV_CORE_MASK),
				  pm_backend->ca_gov_cores_enabled);
}

void kbase_pm_ca_set_gov_core_mask(struct kbase_device *kbdev, enum mask_type core_mask_type,
				   u64 core_mask)
{
	unsigned long flags;

	if (!kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT)) {
		dev_warn(
			kbdev->dev,
			"This function requires Kbase to have access to GOV_CORE_MASK register, cannot proceed\n");
		return;
	}

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	kbase_pm_ca_set_gov_core_mask_nolock(kbdev, core_mask_type, core_mask);
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
}

#ifdef CONFIG_MALI_VALHALL_DEVFREQ

static int set_core_mask_gov(struct kbase_device *kbdev, u64 core_mask)
{
	if (!kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT)) {
		dev_warn(
			kbdev->dev,
			"This function requires Kbase to have access to GOV_CORE_MASK register, cannot proceed\n");
		return -EIO;
	}

	/* Requires a validity check to ensure we don't try to set cores we do not have */
	if ((core_mask & kbdev->gpu_props.shader_present) != core_mask) {
		dev_err(kbdev->dev,
			"core_mask (%llu) must be a subset of the shader present (%llu)", core_mask,
			kbdev->gpu_props.shader_present);
		return -EINVAL;
	}

	kbase_pm_ca_set_gov_core_mask(kbdev, DEVFREQ_COREMASK, core_mask);

	return 0;
}

static int set_core_mask_legacy(struct kbase_device *kbdev, u64 core_mask)
{
	struct kbase_pm_backend_data *pm_backend = &kbdev->pm.backend;
	u64 old_core_mask = 0;
	unsigned long flags;

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	if (!(core_mask & kbdev->pm.debug_core_mask)) {
		dev_err(kbdev->dev,
			"OPP core mask 0x%llX does not intersect with sysfs debug mask 0x%llX\n",
			core_mask, kbdev->pm.debug_core_mask);
		spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
		return -EINVAL;
	}

	old_core_mask = pm_backend->ca_cores_enabled;
	pm_backend->ca_cores_enabled = core_mask;

	kbase_pm_update_state(kbdev);
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	/* Check if old_core_mask contained the undesired cores and wait
	 * for those cores to get powered down
	 */
	if ((core_mask & old_core_mask) != old_core_mask) {
		if (kbase_pm_wait_for_cores_down_scale(kbdev)) {
			dev_warn(kbdev->dev,
				 "Wait for update of core_mask from %llx to %llx failed",
				 old_core_mask, core_mask);
		}
	}

	return 0;
}

void kbase_devfreq_set_core_mask(struct kbase_device *kbdev, u64 core_mask)
{
	bool mmu_sync_needed = false;
	int err;

	if (!IS_ENABLED(CONFIG_MALI_VALHALL_NO_MALI) &&
	    kbase_hw_has_issue(kbdev, KBASE_HW_ISSUE_GPU2019_3901)) {
		mmu_sync_needed = true;
		down_write(&kbdev->csf.mmu_sync_sem);
	}

	err = kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT) ?
			    set_core_mask_gov(kbdev, core_mask) :
			    set_core_mask_legacy(kbdev, core_mask);
	if (mmu_sync_needed)
		up_write(&kbdev->csf.mmu_sync_sem);

	if (!err)
		dev_dbg(kbdev->dev, "Devfreq policy : new core mask=%llX\n", core_mask);
}
KBASE_EXPORT_TEST_API(kbase_devfreq_set_core_mask);
#endif

u64 kbase_pm_ca_get_debug_core_mask(struct kbase_device *kbdev)
{
	return kbdev->pm.debug_core_mask;
}
KBASE_EXPORT_TEST_API(kbase_pm_ca_get_debug_core_mask);

u64 kbase_pm_ca_get_sysfs_gov_core_mask(struct kbase_device *kbdev)
{
	if (!kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT)) {
		dev_warn(
			kbdev->dev,
			"This function requires Kbase to have access to GOV_CORE_MASK register, cannot proceed\n");
		return 0;
	}

	return kbdev->pm.sysfs_gov_core_mask;
}
KBASE_EXPORT_TEST_API(kbase_pm_ca_get_sysfs_gov_core_mask);

u64 kbase_pm_ca_get_core_mask(struct kbase_device *kbdev)
{
	u64 debug_core_mask = kbase_pm_ca_get_debug_core_mask(kbdev);

	lockdep_assert_held(&kbdev->hwaccess_lock);

#ifdef CONFIG_MALI_VALHALL_DEVFREQ
	/*
	 * Although in the init we let the pm_backend->ca_cores_enabled to be
	 * the max config (it uses the base_gpu_props), at this function we need
	 * to limit it to be a subgroup of the curr config, otherwise the
	 * shaders state machine on the PM does not evolve.
	 */
	return kbdev->gpu_props.curr_config.shader_present & kbdev->pm.backend.ca_cores_enabled &
	       debug_core_mask;
#else
	return kbdev->gpu_props.curr_config.shader_present & debug_core_mask;
#endif
}
KBASE_EXPORT_TEST_API(kbase_pm_ca_get_core_mask);

u64 kbase_pm_ca_get_gov_core_mask(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

	return kbdev->pm.backend.ca_gov_cores_enabled;
}
KBASE_EXPORT_TEST_API(kbase_pm_ca_get_gov_core_mask);

u64 kbase_pm_ca_get_instr_core_mask(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

#if IS_ENABLED(CONFIG_MALI_VALHALL_NO_MALI)
	return (((1ull) << KBASE_DUMMY_MODEL_MAX_SHADER_CORES) - 1);
#else
	return kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_SHADER);
#endif
}
