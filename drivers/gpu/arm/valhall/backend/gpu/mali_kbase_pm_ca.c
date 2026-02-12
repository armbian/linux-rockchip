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

int kbase_pm_ca_init(struct kbase_device *kbdev)
{
	/* Initial debug_core_mask value is different based on GOV_CORE_MASK. */
	if (kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT))
		kbdev->pm.debug_core_mask = 0x0;
	else
		kbdev->pm.debug_core_mask = kbdev->gpu_props.shader_present;

#ifdef CONFIG_MALI_VALHALL_DEVFREQ
	if (kbdev->current_core_mask)
		kbdev->pm.backend.ca_cores_enabled = kbdev->current_core_mask;
	else
		kbdev->pm.backend.ca_cores_enabled = kbdev->gpu_props.shader_present;
#else
	kbdev->pm.backend.ca_cores_enabled = kbdev->gpu_props.shader_present;
#endif

	return 0;
}

void kbase_pm_ca_term(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

/**
 * kbase_pm_ca_write_gov_core_mask - Write core mask value to GOV_CORE_MASK register
 * @kbdev: Device pointer.
 *
 * This function is used to change the available core mask as defined via either sysfs or devfreq.
 * hwaccess_lock should be held and should be called only when the GPU supports GOV_CORE_MASK
 */
static void kbase_pm_ca_write_gov_core_mask(struct kbase_device *kbdev)
{
	struct kbase_pm_core_masks all_core_masks;

	if (!kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT)) {
		dev_warn(
			kbdev->dev,
			"This function requires Kbase to have access to GOV_CORE_MASK register, cannot proceed\n");
		return;
	}

	lockdep_assert_held(&kbdev->hwaccess_lock);

	all_core_masks = kbase_pm_ca_get_core_masks(kbdev);
	/**
	 * Write the to GOV_CORE_MASK register if GPU powered,
	 * otherwise value will be applied on next reboot.
	 */
	if (kbase_io_is_gpu_powered(kbdev)) {
		kbase_reg_write64(kbdev, GPU_GOVERNOR_ENUM(GOV_CORE_MASK),
				  all_core_masks.pm_core_mask_desired);
		dev_dbg(kbdev->dev, "PM-CA: Gov-core-mask set to %llX\n",
			all_core_masks.pm_core_mask_desired);
	} else
		dev_dbg(kbdev->dev,
			"PM-CA: Gov-core-mask couldn't be set since power is not up.\n");
}

void kbase_pm_ca_set_core_mask(struct kbase_device *kbdev, enum kbase_core_mask_type core_mask_type,
			       u64 core_mask)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

	/* Update core mask storage based on source */
	switch (core_mask_type) {
	case PM_CA_COREMASK_TYPE_SYSFS:
		kbdev->pm.debug_core_mask = core_mask;
		break;
#ifdef CONFIG_MALI_VALHALL_DEVFREQ
	case PM_CA_COREMASK_TYPE_DEVFREQ:
		kbdev->pm.backend.ca_cores_enabled = core_mask;
		break;
#endif
	case PM_CA_COREMASK_TYPE_REWRITE:
		/* Currently No-op other then writing gov_core_mask again */
		break;
	}

	if (kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT))
		kbase_pm_ca_write_gov_core_mask(kbdev);
}

struct kbase_pm_core_masks kbase_pm_ca_get_core_masks(struct kbase_device *kbdev)
{
	struct kbase_pm_core_masks cur_core_masks;
	const u64 shaders_present = kbdev->gpu_props.curr_config.shader_present;

	lockdep_assert_held(&kbdev->hwaccess_lock);

	/* Raw core masks */
	cur_core_masks.pm_core_mask_debug = kbdev->pm.debug_core_mask;
	cur_core_masks.pm_core_mask_devfreq = kbdev->pm.backend.ca_cores_enabled;

	/* Final core mask calculated from raw core masks */
	if (kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT)) {
		if (cur_core_masks.pm_core_mask_debug != 0x0)
			cur_core_masks.pm_core_mask_desired = cur_core_masks.pm_core_mask_debug;
		else
#ifdef CONFIG_MALI_VALHALL_DEVFREQ
			cur_core_masks.pm_core_mask_desired = cur_core_masks.pm_core_mask_devfreq;
#else
			cur_core_masks.pm_core_mask_desired = shaders_present;
#endif
	} else {
		cur_core_masks.pm_core_mask_desired = cur_core_masks.pm_core_mask_devfreq &
						      cur_core_masks.pm_core_mask_debug;
	}
	cur_core_masks.pm_core_mask_desired &= shaders_present;

	/* Core mask to be written to CFG_ALLOC_EN */
	if (kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT) &&
	    !kbase_pm_no_mcu_core_pwroff(kbdev))
		cur_core_masks.pm_core_mask_alloc_en = shaders_present;
	else
		cur_core_masks.pm_core_mask_alloc_en = cur_core_masks.pm_core_mask_desired;

	return cur_core_masks;
}
KBASE_EXPORT_TEST_API(kbase_pm_ca_get_core_masks);

#ifdef CONFIG_MALI_VALHALL_DEVFREQ
void kbase_devfreq_set_core_mask(struct kbase_device *kbdev, u64 core_mask)
{
	int err = 0;
	unsigned long flags;
	bool mmu_sync_needed = false;
	u64 new_core_mask_alloc_en = 0;
	u64 old_core_mask_alloc_en = 0;
	struct kbase_pm_core_masks all_core_masks;

	/* If a known hardware issue exists, ensure MMU sync is performed */
	if (!IS_ENABLED(CONFIG_MALI_VALHALL_NO_MALI) &&
	    kbase_hw_has_issue(kbdev, KBASE_HW_ISSUE_GPU2019_3901)) {
		mmu_sync_needed = true;
		down_write(&kbdev->csf.mmu_sync_sem);
	}

	/* Validate that the requested core_mask is a subset of the available shaders */
	if ((core_mask & kbdev->gpu_props.shader_present) != core_mask) {
		dev_err(kbdev->dev,
			"core_mask (%llu) must be a subset of the shader present (%llu)", core_mask,
			kbdev->gpu_props.shader_present);
		err = -EINVAL;
		goto cleanup_mmu;
	}

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	/* Get old core mask for GLB_ALLOC_EN */
	all_core_masks = kbase_pm_ca_get_core_masks(kbdev);
	old_core_mask_alloc_en = all_core_masks.pm_core_mask_alloc_en;

	/* For non-GOV cases, ensure the new mask intersects with the debug mask */
	if (!(core_mask & all_core_masks.pm_core_mask_debug) &&
	    !(kbase_hw_has_feature(kbdev, KBASE_HW_FEATURE_GOV_CORE_MASK_SUPPORT))) {
		dev_err(kbdev->dev,
			"OPP core mask 0x%llX does not intersect with sysfs debug mask 0x%llX\n",
			core_mask, all_core_masks.pm_core_mask_debug);
		err = -EINVAL;
		spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
		goto cleanup_mmu;
	}

	/* Update the core masks based on DEVFREQ policy */
	kbase_pm_ca_set_core_mask(kbdev, PM_CA_COREMASK_TYPE_DEVFREQ, core_mask);
	kbase_pm_update_state(kbdev);

	/* Get new core mask for GLB_ALLOC_EN */
	all_core_masks = kbase_pm_ca_get_core_masks(kbdev);
	new_core_mask_alloc_en = all_core_masks.pm_core_mask_alloc_en;
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	/*
	 * Wait for cores to power down if the new core_mask on GLB_ALLOC_EN
	 * excludes cores that were previously desired.
	 */
	if ((new_core_mask_alloc_en & old_core_mask_alloc_en) != old_core_mask_alloc_en) {
		if (kbase_pm_wait_for_cores_down_scale(kbdev)) {
			dev_warn(kbdev->dev,
				 "Wait for update of core_mask from %llx to %llx failed",
				 old_core_mask_alloc_en, core_mask);
		}
	}

cleanup_mmu:
	if (mmu_sync_needed)
		up_write(&kbdev->csf.mmu_sync_sem);

	if (!err)
		dev_dbg(kbdev->dev, "Devfreq policy : new core mask=%llX\n", core_mask);
}
KBASE_EXPORT_TEST_API(kbase_devfreq_set_core_mask);
#endif

u64 kbase_pm_ca_get_instr_core_mask(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

#if IS_ENABLED(CONFIG_MALI_VALHALL_NO_MALI)
	return (((1ull) << KBASE_DUMMY_MODEL_MAX_SHADER_CORES) - 1);
#else
	return kbase_pm_get_ready_cores(kbdev, KBASE_PM_CORE_SHADER);
#endif
}
