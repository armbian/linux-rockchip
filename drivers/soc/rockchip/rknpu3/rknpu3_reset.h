/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 */

#ifndef __RKNPU3_RESET_H__
#define __RKNPU3_RESET_H__

#include "rknpu3_types.h"
#include "rknpu3_config.h"

/* RKNPU global core count */
#define RKNPU3_GLOBAL_CORE_NUM 8

/* Reset related register definitions */
#ifdef FPGA_PLATFORM
#define RKNPU3_RESET_OFFSET      0x10008
#define RKNPU3_RESET_VALUE       0x50000
#define RKNPU3_USE_RKRV_VALUE    0x20002
#define RKNPU3_UNRESET_VALUE     0x50005
#else
#define RKNPU3_RESET_VALUE       0x080c080c      /* NPU reset value */
#define RKNPU3_UNRESET_VALUE     0x080c0000      /* NPU de-reset value */
#define NPU_GRF_USE_RKRV_OFFSET  0x0             /* NPU_GRF use_RKRV register offset */
#define NPU_GRF_USE_RKRV_VALUE   0x00200020     /* Enable RKRV value */
#endif

/**
 * rknpu3_reset_init() - Initialize reset control resources
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_reset_init(struct rknpu3_device *rknpu3_dev);

/**
 * rknpu3_reset_deinit() - Release reset control resources
 * @rknpu3_dev: RKNPU device
 */
void rknpu3_reset_deinit(struct rknpu3_device *rknpu3_dev);

/**
 * rknpu3_core_module_reset() - Reset core module state (software reset)
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_core_module_reset(struct rknpu3_device *rknpu3_dev);

/**
 * rknpu3_core_reset() - Reset specified NPU core (including hardware reset)
 * @rknpu3_dev: RKNPU device
 * @core_id: Core ID
 * @bin_kaddr: Binary kernel address (for debug printing)
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_core_reset(struct rknpu3_device *rknpu3_dev, uint32_t core_id, uint64_t bin_kaddr);

/**
 * rknpu3_hw_core_reset() - Hardware reset NPU core
 * @rknpu3_dev: RKNPU device
 * @core_id: Global core ID
 * @full_reset: Whether to reset srst_a_npu and reinitialize IOMMU
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_hw_core_reset(struct rknpu3_device *rknpu3_dev, uint32_t core_id,
			 bool full_reset);

#endif /* __RKNPU3_RESET_H__ */
