/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */
#ifndef __SOC_ROCKCHIP_IOMMU_H
#define __SOC_ROCKCHIP_IOMMU_H

struct device;
struct iommu_domain;
struct iommu_iotlb_gather;
struct platform_device;

/* 1. dev and pdev is iommu device.
 * 2. store third_iommu data in dev->platform_data.
 */
struct third_iommu_ops_wrap {
	int (*attach_dev)(struct iommu_domain *domain, struct device *dev);
	void (*detach_dev)(struct iommu_domain *domain, struct device *dev);
	int (*map)(struct iommu_domain *domain, unsigned long iova,
		   phys_addr_t paddr, size_t size, int prot, gfp_t gfp,
		   struct device *dev);
	size_t (*unmap)(struct iommu_domain *domain, unsigned long iova,
			size_t size, struct iommu_iotlb_gather *iotlb_gather,
			struct device *dev);
	void (*flush_iotlb_all)(struct iommu_domain *domain,
				struct device *dev);
	phys_addr_t (*iova_to_phys)(struct iommu_domain *domain,
				    dma_addr_t iova, struct device *dev);
	void (*free)(struct iommu_domain *domain, struct device *dev);
	int (*enable)(struct device *dev);
	int (*disable)(struct device *dev);
	void (*shutdown)(struct platform_device *pdev);
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
	int (*probe)(struct platform_device *pdev);
};

#if IS_REACHABLE(CONFIG_ROCKCHIP_IOMMU)
int rockchip_iommu_enable(struct device *dev);
int rockchip_iommu_disable(struct device *dev);
int rockchip_pagefault_done(struct device *master_dev);
void __iomem *rockchip_get_iommu_base(struct device *master_dev, int idx);
bool rockchip_iommu_is_enabled(struct device *dev);
void rockchip_iommu_mask_irq(struct device *dev);
void rockchip_iommu_unmask_irq(struct device *dev);
int rockchip_iommu_force_reset(struct device *dev);
#else
static inline int rockchip_iommu_enable(struct device *dev)
{
	return -ENODEV;
}
static inline int rockchip_iommu_disable(struct device *dev)
{
	return -ENODEV;
}
static inline int rockchip_pagefault_done(struct device *master_dev)
{
	return 0;
}
static inline void __iomem *rockchip_get_iommu_base(struct device *master_dev, int idx)
{
	return NULL;
}
static inline bool rockchip_iommu_is_enabled(struct device *dev)
{
	return false;
}
static inline void rockchip_iommu_mask_irq(struct device *dev)
{
}
static inline void rockchip_iommu_unmask_irq(struct device *dev)
{
}
static inline int rockchip_iommu_force_reset(struct device *dev)
{
	return -ENODEV;
}
#endif

#if IS_ENABLED(CONFIG_ROCKCHIP_MPP_AV1DEC)
struct third_iommu_ops_wrap *av1d_iommu_get_ops(void);
#else
static inline struct third_iommu_ops_wrap *av1d_iommu_get_ops(void)
{
	return NULL;
}
#endif

#endif
