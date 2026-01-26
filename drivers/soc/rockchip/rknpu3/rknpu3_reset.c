// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU3 Reset Control Implementation
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <soc/rockchip/rockchip_iommu.h>

#include "rknpu3_reset.h"
#include "rknpu3_drv.h"
#include "rknpu3_utils.h"
#include "rknpu3_config.h"
#include "boot_params.h"

/**
 * rknpu3_hw_core_reset() - Hardware reset NPU core
 * @rknpu3_dev: RKNPU device
 * @core_id: Global core ID
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_hw_core_reset(struct rknpu3_device *rknpu3_dev, uint32_t core_id)
{
	void __iomem *base_addr;
	struct iommu_domain *domain = NULL;
	uint32_t local_id;
	int ret;

	if (core_id >= RKNPU3_GLOBAL_CORE_NUM)
		return -EINVAL;

	if (!rknpu3_dev || !rknpu3_dev->base[core_id])
		return -EINVAL;

	base_addr = rknpu3_dev->base[core_id];

#ifdef FPGA_PLATFORM
	rknpu3_reg_write(base_addr, RKNPU3_RESET_OFFSET, RKNPU3_RESET_VALUE);
	rknpu3_reg_write(base_addr, RKNPU3_RESET_OFFSET, RKNPU3_USE_RKRV_VALUE);
	rknpu3_reg_write(base_addr, RKNPU3_RESET_OFFSET, RKNPU3_UNRESET_VALUE);
#else
	local_id = rknpu3_core_to_local_idx(core_id);

	if (local_id >= RKNPU3_LOCAL_CORE_NUM) {
		dev_err(rknpu3_dev->dev, "invalid core_id %u for reset\n", core_id);
		return -EINVAL;
	}
	if (!rknpu3_dev->srst_a_npu[local_id] && !rknpu3_dev->srst_a_nrv[local_id]) {
		dev_err(rknpu3_dev->dev, "reset control not initialized for core %u\n",
			core_id);
		return -EINVAL;
	}

	/* Assert reset via reset controller */
	if (rknpu3_dev->srst_a_npu[local_id])
		reset_control_assert(rknpu3_dev->srst_a_npu[local_id]);
	if (rknpu3_dev->srst_a_nrv[local_id])
		reset_control_assert(rknpu3_dev->srst_a_nrv[local_id]);

	/* Configure to use RKRV */
	if (rknpu3_dev->npu_grf && !IS_ERR(rknpu3_dev->npu_grf))
		regmap_write(rknpu3_dev->npu_grf, NPU_GRF_USE_RKRV_OFFSET,
			NPU_GRF_USE_RKRV_VALUE);

	udelay(5);

	/* Deassert reset */
	if (rknpu3_dev->srst_a_nrv[local_id])
		reset_control_deassert(rknpu3_dev->srst_a_nrv[local_id]);
	if (rknpu3_dev->srst_a_npu[local_id])
		reset_control_deassert(rknpu3_dev->srst_a_npu[local_id]);

	/* Reset clears IOMMU page table, need to re-attach IOMMU */
	if (rknpu3_dev->iommu_en)
		domain = iommu_get_domain_for_dev(rknpu3_dev->dev);

	if (domain) {
		/* Disable IOMMU */
		ret = rockchip_iommu_disable(rknpu3_dev->dev);
		if (ret)
			return ret;

		/* Re-enable IOMMU */
		ret = rockchip_iommu_enable(rknpu3_dev->dev);
		if (ret)
			return ret;

		dev_dbg(rknpu3_dev->dev, "IOMMU re-enabled after reset\n");
	}

	/* Clear interrupt */
	rknpu3_reg_write(base_addr, 0x60, 0x0);
#endif
	return 0;
}

/**
 * rknpu3_reset_init() - Initialize reset control resources
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_reset_init(struct rknpu3_device *rknpu3_dev)
{
	const struct rknpu3_config *config;
	int i;

	if (!rknpu3_dev || !rknpu3_dev->config)
		return -EINVAL;

	config = rknpu3_dev->config;

	/* Get reset control for each core */
	for (i = 0; i < config->num_resets && i < rknpu3_dev->num_cores; i++) {
		if (config->resets[i].srst_a_npu_name) {
			rknpu3_dev->srst_a_npu[i] = devm_reset_control_get(
				rknpu3_dev->dev, config->resets[i].srst_a_npu_name);
			if (IS_ERR(rknpu3_dev->srst_a_npu[i])) {
				dev_warn(rknpu3_dev->dev, "failed to get reset %s for core %d\n",
					config->resets[i].srst_a_npu_name, i);
				rknpu3_dev->srst_a_npu[i] = NULL;
			} else {
				dev_info(rknpu3_dev->dev, "core %d: got reset control %s\n",
					i, config->resets[i].srst_a_npu_name);
			}
		}

		if (config->resets[i].srst_a_nrv_name) {
			rknpu3_dev->srst_a_nrv[i] = devm_reset_control_get(
				rknpu3_dev->dev, config->resets[i].srst_a_nrv_name);
			if (IS_ERR(rknpu3_dev->srst_a_nrv[i])) {
				dev_warn(rknpu3_dev->dev, "failed to get reset %s for core %d\n",
					config->resets[i].srst_a_nrv_name, i);
				rknpu3_dev->srst_a_nrv[i] = NULL;
			} else {
				dev_info(rknpu3_dev->dev, "core %d: got reset control %s\n",
					i, config->resets[i].srst_a_nrv_name);
			}
		}
	}

	return 0;
}

/**
 * rknpu3_reset_deinit() - Release reset control resources
 * @rknpu3_dev: RKNPU device
 */
void rknpu3_reset_deinit(struct rknpu3_device *rknpu3_dev)
{
}

/**
 * rknpu3_core_module_reset() - Reset core module state (software reset)
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_core_module_reset(struct rknpu3_device *rknpu3_dev)
{
	uint32_t i;
	unsigned long flags;
	uint64_t reset_time;

	if (rknpu3_dev == NULL)
		return RKNPU3_ERROR_INVALID_PARAM;

	/* Acquire lock (interrupt safe) */
	spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);

	/* Reset core status */
	reset_time = rknpu3_get_time_us();
	for (i = 0; i < RKNPU3_LOCAL_CORE_NUM; i++) {
		rknpu3_dev->core_status[i].is_available = true;
		rknpu3_dev->core_status[i].current_task_id = 0;
		rknpu3_dev->core_status[i].current_task = NULL;
		/* Reset utilization statistics fields */
		rknpu3_dev->core_status[i].last_query_time = reset_time;
		rknpu3_dev->core_status[i].last_state_change_time = reset_time;
		rknpu3_dev->core_status[i].busy_time_accum_us = 0;
	}

	/* Release lock */
	spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);

	return RKNPU3_SUCCESS;
}

/**
 * rknpu3_core_reset() - Reset specified NPU core (including hardware reset)
 * @rknpu3_dev: RKNPU device
 * @core_id: Core ID
 * @bin_kaddr: Binary kernel address (for debug printing)
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_core_reset(struct rknpu3_device *rknpu3_dev, uint32_t core_id, uint64_t bin_kaddr)
{
	uint32_t global_core_id;
	struct rknrv_boot_params *boot_params;
	uint32_t pc, entry_point_addr, raw_status;
	uint32_t local_addr;
	int ret;
	unsigned long flags;
	struct rknpu3_core_status *status;
	uint64_t now_us;
	uint64_t reset_time;

	if (rknpu3_dev == NULL)
		return RKNPU3_ERROR_INVALID_PARAM;

	if (core_id >= RKNPU3_LOCAL_CORE_NUM)
		return RKNPU3_ERROR_INVALID_PARAM;

	global_core_id = rknpu3_core_to_global_id(core_id);

	/* Get and print core status */
	if (bin_kaddr != 0) {
		void __iomem *base_addr;

		boot_params = (struct rknrv_boot_params *)bin_kaddr;

		if (rknpu3_dev->base[global_core_id]) {
			base_addr = rknpu3_dev->base[global_core_id];
			pc = rknpu3_reg_read(base_addr, 0x268c);
			entry_point_addr = rknpu3_reg_read(base_addr, 0x2608);
			raw_status = rknpu3_reg_read(base_addr, 0x48);
			local_addr = entry_point_addr - boot_params->entry_point;
			pr_info("Core %d reset: entry_point: 0x%x, pc: 0x%x, raw_status: 0x%x\n",
				global_core_id, boot_params->entry_point,
				pc - local_addr, raw_status);
		}
	}

	/* Acquire lock (interrupt safe) */
	spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);

	/* If currently busy, accumulate busy time first */
	status = &rknpu3_dev->core_status[core_id];
	now_us = rknpu3_get_time_us();
	if (!status->is_available && now_us > status->last_state_change_time)
		status->busy_time_accum_us += (now_us - status->last_state_change_time);

	/* Initialize core status */
	rknpu3_dev->core_status[core_id].is_available = true;
	rknpu3_dev->core_status[core_id].current_task_id = 0;
	rknpu3_dev->core_status[core_id].current_task = NULL;
	/* Reset utilization statistics fields */
	reset_time = rknpu3_get_time_us();
	rknpu3_dev->core_status[core_id].last_query_time = reset_time;
	rknpu3_dev->core_status[core_id].last_state_change_time = reset_time;
	rknpu3_dev->core_status[core_id].busy_time_accum_us = 0;

	/* Release lock */
	spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);

	/* Hardware reset core implementation */
	ret = rknpu3_hw_core_reset(rknpu3_dev, global_core_id);
	if (ret != 0)
		return RKNPU3_ERROR_OPERATION_FAILED;

	return RKNPU3_SUCCESS;
}
