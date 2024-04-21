// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co.Ltd
 * Author: Felix Zeng <felix.zeng@rock-chips.com>
 */

#include "rknpu_iommu.h"

dma_addr_t rknpu_iommu_dma_alloc_iova(struct iommu_domain *domain, size_t size,
				      u64 dma_limit, struct device *dev)
{
	struct rknpu_iommu_dma_cookie *cookie = (void *)domain->iova_cookie;
	struct iova_domain *iovad = &cookie->iovad;
	unsigned long shift, iova_len, iova = 0;
#if (KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE)
	dma_addr_t limit;
#endif

	shift = iova_shift(iovad);
	iova_len = size >> shift;

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	/*
	 * Freeing non-power-of-two-sized allocations back into the IOVA caches
	 * will come back to bite us badly, so we have to waste a bit of space
	 * rounding up anything cacheable to make sure that can't happen. The
	 * order of the unadjusted size will still match upon freeing.
	 */
	if (iova_len < (1 << (IOVA_RANGE_CACHE_MAX_SIZE - 1)))
		iova_len = roundup_pow_of_two(iova_len);
#endif

#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
	dma_limit = min_not_zero(dma_limit, dev->bus_dma_limit);
#else
	if (dev->bus_dma_mask)
		dma_limit &= dev->bus_dma_mask;
#endif

	if (domain->geometry.force_aperture)
		dma_limit =
			min_t(u64, dma_limit, domain->geometry.aperture_end);

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	iova = alloc_iova_fast(iovad, iova_len, dma_limit >> shift, true);
#else
	limit = min_t(dma_addr_t, dma_limit >> shift, iovad->end_pfn);

	iova = alloc_iova_fast(iovad, iova_len, limit, true);
#endif

	return (dma_addr_t)iova << shift;
}

void rknpu_iommu_dma_free_iova(struct rknpu_iommu_dma_cookie *cookie,
			       dma_addr_t iova, size_t size)
{
	struct iova_domain *iovad = &cookie->iovad;

	free_iova_fast(iovad, iova_pfn(iovad, iova), size >> iova_shift(iovad));
}

#if defined(CONFIG_IOMMU_API) && defined(CONFIG_NO_GKI)

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
struct iommu_group {
	struct kobject kobj;
	struct kobject *devices_kobj;
	struct list_head devices;
#ifdef __ANDROID_COMMON_KERNEL__
	struct xarray pasid_array;
#endif
	struct mutex mutex;
	void *iommu_data;
	void (*iommu_data_release)(void *iommu_data);
	char *name;
	int id;
	struct iommu_domain *default_domain;
	struct iommu_domain *blocking_domain;
	struct iommu_domain *domain;
	struct list_head entry;
	unsigned int owner_cnt;
	void *owner;
};
#else
struct iommu_group {
	struct kobject kobj;
	struct kobject *devices_kobj;
	struct list_head devices;
	struct mutex mutex;
	struct blocking_notifier_head notifier;
	void *iommu_data;
	void (*iommu_data_release)(void *iommu_data);
	char *name;
	int id;
	struct iommu_domain *default_domain;
	struct iommu_domain *domain;
	struct list_head entry;
};
#endif

int rknpu_iommu_init_domain(struct rknpu_device *rknpu_dev)
{
	// init domain 0
	if (!rknpu_dev->iommu_domains[0]) {
		rknpu_dev->iommu_domain_id = 0;
		rknpu_dev->iommu_domains[rknpu_dev->iommu_domain_id] =
			iommu_get_domain_for_dev(rknpu_dev->dev);
		rknpu_dev->iommu_domain_num = 1;
	}
	return 0;
}

int rknpu_iommu_switch_domain(struct rknpu_device *rknpu_dev, int domain_id)
{
	struct iommu_domain *src_domain = NULL;
	struct iommu_domain *dst_domain = NULL;
	struct bus_type *bus = NULL;
	int src_domain_id = 0;
	int ret = -EINVAL;

	if (!rknpu_dev->iommu_en)
		return -EINVAL;

	if (domain_id < 0 || domain_id > (RKNPU_MAX_IOMMU_DOMAIN_NUM - 1)) {
		LOG_DEV_ERROR(
			rknpu_dev->dev,
			"invalid iommu domain id: %d, reuse domain id: %d\n",
			domain_id, rknpu_dev->iommu_domain_id);
		return -EINVAL;
	}

	bus = rknpu_dev->dev->bus;
	if (!bus)
		return -EFAULT;

	mutex_lock(&rknpu_dev->domain_lock);

	src_domain_id = rknpu_dev->iommu_domain_id;
	if (domain_id == src_domain_id) {
		mutex_unlock(&rknpu_dev->domain_lock);
		return 0;
	}

	src_domain = iommu_get_domain_for_dev(rknpu_dev->dev);
	if (src_domain != rknpu_dev->iommu_domains[src_domain_id]) {
		LOG_DEV_ERROR(
			rknpu_dev->dev,
			"mismatch domain get from iommu_get_domain_for_dev\n");
		mutex_unlock(&rknpu_dev->domain_lock);
		return -EINVAL;
	}

	dst_domain = rknpu_dev->iommu_domains[domain_id];
	if (dst_domain != NULL) {
		iommu_detach_device(src_domain, rknpu_dev->dev);
		ret = iommu_attach_device(dst_domain, rknpu_dev->dev);
		if (ret) {
			LOG_DEV_ERROR(
				rknpu_dev->dev,
				"failed to attach dst iommu domain, id: %d, ret: %d\n",
				domain_id, ret);
			if (iommu_attach_device(src_domain, rknpu_dev->dev)) {
				LOG_DEV_ERROR(
					rknpu_dev->dev,
					"failed to reattach src iommu domain, id: %d\n",
					src_domain_id);
			}
			mutex_unlock(&rknpu_dev->domain_lock);
			return ret;
		}
		rknpu_dev->iommu_domain_id = domain_id;
	} else {
		uint64_t dma_limit = 1ULL << 32;

		dst_domain = iommu_domain_alloc(bus);
		if (!dst_domain) {
			LOG_DEV_ERROR(rknpu_dev->dev,
				      "failed to allocate iommu domain\n");
			mutex_unlock(&rknpu_dev->domain_lock);
			return -EIO;
		}
		// init domain iova_cookie
		iommu_get_dma_cookie(dst_domain);

		iommu_detach_device(src_domain, rknpu_dev->dev);
		ret = iommu_attach_device(dst_domain, rknpu_dev->dev);
		if (ret) {
			LOG_DEV_ERROR(
				rknpu_dev->dev,
				"failed to attach iommu domain, id: %d, ret: %d\n",
				domain_id, ret);
			iommu_domain_free(dst_domain);
			mutex_unlock(&rknpu_dev->domain_lock);
			return ret;
		}

		// set domain type to dma domain
		dst_domain->type |= __IOMMU_DOMAIN_DMA_API;
		// iommu dma init domain
		iommu_setup_dma_ops(rknpu_dev->dev, 0, dma_limit);

		rknpu_dev->iommu_domain_id = domain_id;
		rknpu_dev->iommu_domains[domain_id] = dst_domain;
		rknpu_dev->iommu_domain_num++;
	}

	// reset default iommu domain
	rknpu_dev->iommu_group->default_domain = dst_domain;

	mutex_unlock(&rknpu_dev->domain_lock);

	LOG_INFO("switch iommu domain from %d to %d\n", src_domain_id,
		 domain_id);

	return ret;
}

void rknpu_iommu_free_domains(struct rknpu_device *rknpu_dev)
{
	int i = 0;

	rknpu_iommu_switch_domain(rknpu_dev, 0);

	for (i = 1; i < RKNPU_MAX_IOMMU_DOMAIN_NUM; i++) {
		struct iommu_domain *domain = rknpu_dev->iommu_domains[i];

		if (domain == NULL)
			continue;

		iommu_detach_device(domain, rknpu_dev->dev);
		iommu_domain_free(domain);

		rknpu_dev->iommu_domains[i] = NULL;
	}
}

#else

int rknpu_iommu_init_domain(struct rknpu_device *rknpu_dev)
{
	return 0;
}

int rknpu_iommu_switch_domain(struct rknpu_device *rknpu_dev, int domain_id)
{
	return 0;
}

void rknpu_iommu_free_domains(struct rknpu_device *rknpu_dev)
{
}

#endif
