/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __ROCKCHIP_MPP_OSAL_H__
#define __ROCKCHIP_MPP_OSAL_H__

#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>

struct dma_iommu_mapping;

struct device_node *mpp_dev_of_node(struct device *dev);
void mpp_pm_relax(struct device *dev);
void mpp_pm_stay_awake(struct device *dev);
int mpp_device_init_wakeup(struct device *dev, bool enable);
void mpp_device_add_driver(void *dev, void *drv);
struct dma_iommu_mapping *mpp_arm_iommu_get_mapping(struct device *dev);

#endif
