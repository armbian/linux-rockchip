// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU Memory Management Module
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/math64.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/iosys-map.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/scatterlist.h>

#include "rknpu3_memory.h"
#include "rknpu3_drv.h"
#include "rknpu3_config.h"
#include "rknpu3_core.h"
#include "rknpu3_utils.h"

/* Import DMA_BUF namespace - required for Linux 5.16+ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0)
#include <linux/module.h>
MODULE_IMPORT_NS(DMA_BUF);
#endif

/* Compatibility for different kernel versions of dma_buf_ops */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
/* Linux 5.18+ uses iosys_map */
#include <linux/iosys-map.h>
#define USE_IOSYS_MAP
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
/* Linux 5.11-5.17 uses dma_buf_map */
#include <linux/dma-buf-map.h>
#define USE_DMA_BUF_MAP
#endif

/**
 * struct rknpu3_dma_buf_priv - DMA buffer private data structure
 * @dev: Device pointer
 * @cpu_addr: CPU virtual address
 * @dma_addr: DMA address
 * @size: Buffer size
 * @is_uncached: Whether buffer is uncached
 * @sgt: Scatter-gather table for exported buffer
 * @sgt_mapped: Whether sgt is mapped
 * @lock: Mutex for concurrent access protection
 */
struct rknpu3_dma_buf_priv {
	struct device *dev;
	struct rknpu3_device *rknpu3_dev;
	void *cpu_addr;
	dma_addr_t dma_addr;
	size_t size;
	bool is_uncached;
	struct sg_table *sgt;
	bool sgt_mapped;
	struct mutex lock;
	uint32_t core_id;
	bool stats_updated;
};

/**
 * struct rknpu3_imported_buf - Imported external DMA-BUF private data
 * @dmabuf: DMA-BUF pointer
 * @attach: DMA-BUF attachment
 * @sgt: Scatter-gather table
 * @dma_addr: DMA address
 * @cpu_addr: CPU virtual address
 * @map: Map structure for vmap/vunmap
 * @mapped: Whether buffer is vmapped
 * @size: Buffer size
 * @node: List node
 * @in_list: Whether node is in list
 */
struct rknpu3_imported_buf {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	void *cpu_addr;
	struct iosys_map map;
	bool mapped;
	size_t size;
	struct list_head node;
	bool in_list;
};

/* dma_buf operation: attach */
static int rknpu3_dma_buf_attach(struct dma_buf *dmabuf,
				 struct dma_buf_attachment *attach)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;
	struct sg_table *sgt;

	/* Create independent sg_table for each attachment */
	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;

	/* Save sg_table to attachment private data */
	attach->priv = sgt;

	LOG_DEV_DEBUG(priv->dev, "dma_buf attached: size=%zu\n", priv->size);
	return 0;
}

/* dma_buf operation: detach */
static void rknpu3_dma_buf_detach(struct dma_buf *dmabuf,
				  struct dma_buf_attachment *attach)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;
	struct sg_table *sgt = attach->priv;

	/* Release sg_table - kfree(NULL) is safe */
	kfree(sgt);
	attach->priv = NULL;

	LOG_DEV_DEBUG(priv->dev, "dma_buf detached\n");
}

/* dma_buf operation: map */
static struct sg_table *rknpu3_dma_buf_map(struct dma_buf_attachment *attach,
					   enum dma_data_direction dir)
{
	struct rknpu3_dma_buf_priv *priv = attach->dmabuf->priv;
	struct sg_table *sgt = attach->priv;
	int ret;

	if (!sgt)
		return ERR_PTR(-EINVAL);

	/* If sg_table already allocated, return directly */
	if (sgt->sgl)
		return sgt;

	ret = dma_get_sgtable_attrs(priv->dev, sgt, priv->cpu_addr, priv->dma_addr,
				    priv->size, 0);
	if (ret) {
		LOG_DEV_ERROR(priv->dev, "dma_get_sgtable failed: %d\n", ret);
		return ERR_PTR(ret);
	}

	/* Map to attached device's address space */
	ret = dma_map_sgtable(attach->dev, sgt, dir, 0);
	if (ret) {
		LOG_DEV_ERROR(priv->dev, "dma_map_sgtable failed: %d\n", ret);
		sg_free_table(sgt);
		return ERR_PTR(ret);
	}

	LOG_DEV_DEBUG(priv->dev, "dma_buf mapped: dma_addr=0x%llx size=%zu\n",
		      (u64)priv->dma_addr, priv->size);

	return sgt;
}

/* dma_buf operation: unmap */
static void rknpu3_dma_buf_unmap(struct dma_buf_attachment *attach,
				 struct sg_table *sgt,
				 enum dma_data_direction dir)
{
	struct rknpu3_dma_buf_priv *priv = attach->dmabuf->priv;

	if (!sgt || !sgt->sgl)
		return;

	/* Unmap */
	dma_unmap_sgtable(attach->dev, sgt, dir, 0);

	/* Release sg_table */
	sg_free_table(sgt);

	LOG_DEV_DEBUG(priv->dev, "dma_buf unmapped\n");
}

/* dma_buf operation: release */
static void rknpu3_dma_buf_release(struct dma_buf *dmabuf)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;
	struct rknpu3_device *rknpu3_dev;
	unsigned long flags;

	if (!priv)
		return;

	rknpu3_dev = priv->rknpu3_dev;

	LOG_DEV_DEBUG(priv->dev, "dma_buf releasing: dma_addr=0x%llx size=%zu\n",
		      (u64)priv->dma_addr, priv->size);

	/*
	 * If MEM_DESTROY was not called (process crashed or exited abnormally),
	 * update memory statistics here. The stats_updated flag is set by
	 * rknpu3_memory_free() when MEM_DESTROY is called normally.
	 */
	if (rknpu3_dev) {
		spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);
		if (!priv->stats_updated) {
			priv->stats_updated = true;
			if (rknpu3_dev->used_memory >= priv->size)
				rknpu3_dev->used_memory -= priv->size;
			if (priv->core_id < rknpu3_dev->num_cores &&
			    rknpu3_dev->core_memory_usage[priv->core_id] >= priv->size)
				rknpu3_dev->core_memory_usage[priv->core_id] -= priv->size;
		}
		spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);
	}

	/* Release DMA memory */
	if (priv->is_uncached)
		dma_free_coherent(priv->dev, priv->size, priv->cpu_addr,
				  priv->dma_addr);
	else
		dma_free_wc(priv->dev, priv->size, priv->cpu_addr, priv->dma_addr);

	if (priv->sgt) {
		if (priv->sgt_mapped)
			dma_unmap_sgtable(priv->dev, priv->sgt, DMA_BIDIRECTIONAL,
					  0);
		sg_free_table(priv->sgt);
		kfree(priv->sgt);
		priv->sgt = NULL;
	}

	/* Destroy mutex */
	mutex_destroy(&priv->lock);

	kfree(priv);
}

/* dma_buf operation: mmap */
static int rknpu3_dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;
	unsigned long vm_size = vma->vm_end - vma->vm_start;
	int ret;

	if (!priv)
		return -EINVAL;

	if (vm_size > priv->size) {
		LOG_DEV_ERROR(priv->dev, "mmap size too large: %lu > %zu\n",
			      vm_size, priv->size);
		return -EINVAL;
	}

	/* Choose mapping method based on memory type */
	if (priv->is_uncached)
		ret = dma_mmap_coherent(priv->dev, vma, priv->cpu_addr,
					priv->dma_addr, priv->size);
	else
		ret = dma_mmap_wc(priv->dev, vma, priv->cpu_addr,
				  priv->dma_addr, priv->size);

	if (ret)
		LOG_DEV_ERROR(priv->dev, "dma_mmap failed: %d\n", ret);
	else
		LOG_DEV_DEBUG(priv->dev, "dma_buf mmapped: vm_size=%lu\n", vm_size);

	return ret;
}

/* dma_buf operation: begin_cpu_access - cache sync before CPU access */
static int rknpu3_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction dir)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;

	if (!priv)
		return -EINVAL;

	/* For uncached memory, no sync needed */
	if (priv->is_uncached)
		return 0;

	mutex_lock(&priv->lock);

	/* For cached memory, invalidate cache to read device-written data */
	if (dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL) {
		if (priv->sgt && priv->sgt_mapped)
			dma_sync_sgtable_for_cpu(priv->dev, priv->sgt, dir);
		else
			dma_sync_single_for_cpu(priv->dev, priv->dma_addr,
						priv->size, dir);
		LOG_DEV_DEBUG(priv->dev, "sync for cpu: dir=%d size=%zu\n",
			      dir, priv->size);
	}

	mutex_unlock(&priv->lock);
	return 0;
}

/* dma_buf operation: end_cpu_access - cache sync after CPU access */
static int rknpu3_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction dir)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;

	if (!priv)
		return -EINVAL;

	/* For uncached memory, no sync needed */
	if (priv->is_uncached)
		return 0;

	mutex_lock(&priv->lock);

	/* For cached memory, flush cache to make CPU modifications visible to device */
	if (dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL) {
		if (priv->sgt && priv->sgt_mapped)
			dma_sync_sgtable_for_device(priv->dev, priv->sgt, dir);
		else
			dma_sync_single_for_device(priv->dev, priv->dma_addr,
						   priv->size, dir);
		LOG_DEV_DEBUG(priv->dev, "sync for device: dir=%d size=%zu\n",
			      dir, priv->size);
	}

	mutex_unlock(&priv->lock);
	return 0;
}

/* dma_buf operation: vmap - compatible with different kernel versions */
#if defined(USE_IOSYS_MAP)
/* Linux 5.18+ uses iosys_map */
static int rknpu3_dma_buf_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;

	iosys_map_set_vaddr(map, priv->cpu_addr);
	return 0;
}

static void rknpu3_dma_buf_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
}
#elif defined(USE_DMA_BUF_MAP)
/* Linux 5.11-5.17 uses dma_buf_map */
static int rknpu3_dma_buf_vmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;

	dma_buf_map_set_vaddr(map, priv->cpu_addr);
	return 0;
}

static void rknpu3_dma_buf_vunmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
{
}
#else
/* Linux 5.10 and earlier use pointer */
static void *rknpu3_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct rknpu3_dma_buf_priv *priv = dmabuf->priv;

	return priv->cpu_addr;
}

static void rknpu3_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
}
#endif

static const struct dma_buf_ops rknpu3_dma_buf_ops = {
	.attach = rknpu3_dma_buf_attach,
	.detach = rknpu3_dma_buf_detach,
	.map_dma_buf = rknpu3_dma_buf_map,
	.unmap_dma_buf = rknpu3_dma_buf_unmap,
	.release = rknpu3_dma_buf_release,
	.mmap = rknpu3_dma_buf_mmap,
	.vmap = rknpu3_dma_buf_vmap,
	.vunmap = rknpu3_dma_buf_vunmap,
	.begin_cpu_access = rknpu3_dma_buf_begin_cpu_access,
	.end_cpu_access = rknpu3_dma_buf_end_cpu_access,
};

int rknpu3_memory_module_init(struct rknpu3_device *dev)
{
	struct device_node *mem_node;
	struct resource res;
	u64 reserved_size = 0;
	int i;

	if (!dev)
		return -EINVAL;

	/* IOMMU mode doesn't use reserved memory limit */
	if (!dev->iommu_en) {
		mem_node = of_parse_phandle(dev->dev->of_node, "memory-region", 0);
		if (mem_node) {
			if (!of_address_to_resource(mem_node, 0, &res))
				reserved_size = resource_size(&res);
			of_node_put(mem_node);
		}

		if (reserved_size) {
			dev->total_memory = reserved_size;
			LOG_DEV_INFO(dev->dev, "using reserved memory: %llu bytes\n",
				     (unsigned long long)reserved_size);
		} else {
			dev->total_memory = 0;
		}
	} else {
		dev->total_memory = 0;
		LOG_DEV_INFO(dev->dev, "IOMMU mode: no reserved memory limit\n");
	}
	dev->used_memory = 0;

	/* Initialize per-core memory usage to 0 */
	for (i = 0; i < dev->num_cores; i++)
		dev->core_memory_usage[i] = 0;

	return 0;
}

int rknpu3_memory_alloc(struct rknpu3_device *dev, struct rknpu3_mem_create *mem_create)
{
	void *cpu_addr;
	dma_addr_t dma_addr;
	unsigned long flags;
	struct rknpu3_dma_buf_priv *priv = NULL;
	struct dma_buf *dmabuf = NULL;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	int fd = -1;
	int ret = 0;

	if (!dev || !mem_create || !mem_create->size)
		return -EINVAL;

	if (!(mem_create->flags & RKNPU3_MEM_TYPE_CONTIGUOUS)) {
		LOG_DEV_ERROR(dev->dev, "Only contiguous memory supported\n");
		return -EINVAL;
	}

	/* Only check memory limit in reserved memory mode */
	if (dev->total_memory) {
		u64 new_used;

		spin_lock_irqsave(&dev->dev_lock, flags);
		new_used = dev->used_memory + mem_create->size;
		if (new_used > dev->total_memory) {
			spin_unlock_irqrestore(&dev->dev_lock, flags);
			LOG_DEV_ERROR(dev->dev,
				      "Out of reserved memory: used=%llu size=%llu total=%llu\n",
				      (unsigned long long)dev->used_memory,
				      (unsigned long long)mem_create->size,
				      (unsigned long long)dev->total_memory);
			return -ENOMEM;
		}
		spin_unlock_irqrestore(&dev->dev_lock, flags);
	}

	/* Use DMA API to allocate memory */
	if (mem_create->flags & RKNPU3_MEM_TYPE_UNCACHED)
		cpu_addr = dma_alloc_coherent(dev->dev, mem_create->size,
					      &dma_addr, GFP_KERNEL);
	else
		cpu_addr = dma_alloc_wc(dev->dev, mem_create->size,
					&dma_addr, GFP_KERNEL);

	if (!cpu_addr) {
		LOG_DEV_ERROR(dev->dev, "Failed to allocate memory: size=%llu\n",
			      mem_create->size);
		return -ENOMEM;
	}

	/* Create dma_buf private data */
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_free_dma;
	}

	priv->dev = dev->dev;
	priv->rknpu3_dev = dev;
	priv->cpu_addr = cpu_addr;
	priv->dma_addr = dma_addr;
	priv->size = mem_create->size;
	priv->is_uncached = !!(mem_create->flags & RKNPU3_MEM_TYPE_UNCACHED);
	priv->sgt = NULL;
	priv->sgt_mapped = false;
	priv->core_id = mem_create->core_id;
	priv->stats_updated = false;
	mutex_init(&priv->lock);
	priv->sgt = kzalloc(sizeof(*priv->sgt), GFP_KERNEL);
	if (!priv->sgt) {
		ret = -ENOMEM;
		goto err_free_priv;
	}
	ret = dma_get_sgtable_attrs(dev->dev, priv->sgt, cpu_addr, dma_addr,
				    mem_create->size, 0);
	if (ret) {
		LOG_DEV_WARN(dev->dev, "dma_get_sgtable failed: %d\n", ret);
		kfree(priv->sgt);
		priv->sgt = NULL;
	} else {
		ret = dma_map_sgtable(dev->dev, priv->sgt, DMA_BIDIRECTIONAL,
				      0);
		if (ret) {
			LOG_DEV_WARN(dev->dev, "dma_map_sgtable failed: %d\n", ret);
			sg_free_table(priv->sgt);
			kfree(priv->sgt);
			priv->sgt = NULL;
		} else {
			priv->sgt_mapped = true;
		}
	}

	/* Export as dma_buf */
	exp_info.ops = &rknpu3_dma_buf_ops;
	exp_info.size = mem_create->size;
	exp_info.flags = O_RDWR | O_CLOEXEC;
	exp_info.priv = priv;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		LOG_DEV_ERROR(dev->dev, "Failed to export dma_buf: %d\n", ret);
		goto err_free_priv;
	}

	/* Get file descriptor */
	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0) {
		ret = fd;
		LOG_DEV_ERROR(dev->dev, "Failed to get dma_buf fd: %d\n", ret);
		goto err_put_dmabuf;
	}

	/* Update memory usage statistics */
	spin_lock_irqsave(&dev->dev_lock, flags);
	dev->used_memory += mem_create->size;
	if (mem_create->core_id < dev->num_cores)
		dev->core_memory_usage[mem_create->core_id] += mem_create->size;
	spin_unlock_irqrestore(&dev->dev_lock, flags);

	mem_create->dma_addr = (uint64_t)dma_addr;
	mem_create->virt_kaddr = (uint64_t)(uintptr_t)cpu_addr;
	mem_create->dma_buf_fd = fd;

	LOG_DEV_DEBUG(dev->dev,
		     "memory allocated: size=%llu core_id=%d flags=0x%x dma_addr=0x%llx virt_kaddr=0x%llx dma_buf_fd=%d iommu=%d\n",
		     (unsigned long long)mem_create->size, mem_create->core_id,
		     mem_create->flags, (unsigned long long)mem_create->dma_addr,
		     (unsigned long long)mem_create->virt_kaddr,
		     mem_create->dma_buf_fd, dev->iommu_en);
	return 0;

err_put_dmabuf:
	dma_buf_put(dmabuf);
	return ret;

err_free_priv:
	if (priv) {
		if (priv->sgt) {
			if (priv->sgt_mapped)
				dma_unmap_sgtable(priv->dev, priv->sgt, DMA_BIDIRECTIONAL, 0);
			sg_free_table(priv->sgt);
			kfree(priv->sgt);
		}
		mutex_destroy(&priv->lock);
	}
	kfree(priv);

err_free_dma:
	if (mem_create->flags & RKNPU3_MEM_TYPE_UNCACHED)
		dma_free_coherent(dev->dev, mem_create->size, cpu_addr, dma_addr);
	else
		dma_free_wc(dev->dev, mem_create->size, cpu_addr, dma_addr);
	return ret;
}

int rknpu3_memory_free(struct rknpu3_device *dev, struct rknpu3_mem_create *mem_create)
{
	struct dma_buf *dmabuf;
	struct rknpu3_dma_buf_priv *priv;
	unsigned long flags;
	size_t size;
	uint32_t core_id;

	if (!dev || !mem_create)
		return -EINVAL;

	/*
	 * Get dma_buf from fd to mark stats as updated.
	 * If dma_buf_get() fails, it means:
	 * 1. The fd was closed before MEM_DESTROY (user error or process exit)
	 * 2. The dma_buf has already been released by rknpu3_dma_buf_release()
	 * In both cases, rknpu3_dma_buf_release() has already handled stats
	 * update (if stats_updated was false), so we must NOT update stats
	 * again to avoid underflow.
	 */
	if (mem_create->dma_buf_fd < 0) {
		LOG_DEV_DEBUG(dev->dev,
			      "memory free: invalid fd=%d, skipping stats update\n",
			      mem_create->dma_buf_fd);
		return 0;
	}

	dmabuf = dma_buf_get(mem_create->dma_buf_fd);
	if (IS_ERR(dmabuf)) {
		/*
		 * dma_buf already released - stats were handled by
		 * rknpu3_dma_buf_release(), nothing more to do.
		 */
		LOG_DEV_DEBUG(dev->dev,
			      "memory free: dma_buf already released, fd=%d\n",
			      mem_create->dma_buf_fd);
		return 0;
	}

	/*
	 * Verify this dma_buf was created by us by checking ops.
	 * If it's not ours, don't touch priv or update stats.
	 */
	if (dmabuf->ops != &rknpu3_dma_buf_ops) {
		LOG_DEV_WARN(dev->dev,
			     "memory free: dma_buf not created by rknpu, fd=%d\n",
			     mem_create->dma_buf_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	priv = dmabuf->priv;
	if (!priv) {
		LOG_DEV_WARN(dev->dev,
			     "memory free: dma_buf has no priv data, fd=%d\n",
			     mem_create->dma_buf_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	/*
	 * Use the actual size and core_id from priv, not from user-provided
	 * mem_create, to ensure stats are correctly updated.
	 */
	size = priv->size;
	core_id = priv->core_id;

	/*
	 * Acquire lock before checking/setting stats_updated to prevent race
	 * condition where two threads both see stats_updated=false and both
	 * try to decrement stats.
	 */
	spin_lock_irqsave(&dev->dev_lock, flags);

	/*
	 * Check if stats were already updated (MEM_DESTROY called twice,
	 * or rknpu3_dma_buf_release() already ran). If so, don't decrement
	 * stats again to avoid underflow.
	 */
	if (priv->stats_updated) {
		spin_unlock_irqrestore(&dev->dev_lock, flags);
		LOG_DEV_DEBUG(dev->dev,
			      "memory free: stats already updated, fd=%d size=%zu\n",
			      mem_create->dma_buf_fd, size);
		dma_buf_put(dmabuf);
		return 0;
	}

	/* Mark stats as updated before actually updating */
	priv->stats_updated = true;

	if (dev->used_memory >= size)
		dev->used_memory -= size;
	else
		LOG_DEV_WARN(dev->dev, "memory usage underflow: used=%llu size=%zu\n",
			     (unsigned long long)dev->used_memory, size);

	/* Update corresponding core's memory usage */
	if (core_id < dev->num_cores) {
		if (dev->core_memory_usage[core_id] >= size)
			dev->core_memory_usage[core_id] -= size;
		else
			LOG_DEV_WARN(dev->dev,
				     "core %u memory usage underflow: used=%llu size=%zu\n",
				     core_id,
				     (unsigned long long)dev->core_memory_usage[core_id],
				     size);
	}

	LOG_DEV_DEBUG(dev->dev, "memory freed: total_used=%llu core_%u_used=%llu, size=%zu, fd=%d\n",
		      (unsigned long long)dev->used_memory, core_id,
		      (unsigned long long)(core_id < dev->num_cores ?
					   dev->core_memory_usage[core_id] : 0),
		      size, mem_create->dma_buf_fd);

	spin_unlock_irqrestore(&dev->dev_lock, flags);

	/* Release dma_buf reference after updating stats */
	dma_buf_put(dmabuf);

	return 0;
}

/**
 * rknpu3_dma_buf_sync - Sync DMA buffer via scatter-gather table
 * @dev: Device pointer
 * @sgt: Scatter-gather table
 * @offset: Offset within buffer
 * @length: Length to sync
 * @dir: DMA direction
 * @for_cpu: true for begin_cpu_access, false for end_cpu_access
 *
 * Reference: drivers/rknpu/rknpu3_mem.c rknpu3_dma_buf_sync()
 */
static void rknpu3_dma_buf_sync(struct device *dev, struct sg_table *sgt,
			       u32 offset, u32 length,
			       enum dma_data_direction dir, bool for_cpu)
{
	struct scatterlist *sg;
	dma_addr_t sg_dma_addr;
	unsigned int len = 0;
	int i;

	if (!sgt || !sgt->sgl)
		return;

	sg = sgt->sgl;
	sg_dma_addr = sg_dma_address(sg);

	for_each_sgtable_sg(sgt, sg, i) {
		unsigned int sg_offset, sg_left, size = 0;

		len += sg->length;
		if (len <= offset) {
			sg_dma_addr += sg->length;
			continue;
		}

		sg_left = len - offset;
		sg_offset = sg->length - sg_left;

		size = (length < sg_left) ? length : sg_left;

		if (for_cpu)
			dma_sync_single_range_for_cpu(dev, sg_dma_addr,
						      sg_offset, size, dir);
		else
			dma_sync_single_range_for_device(dev, sg_dma_addr,
							 sg_offset, size, dir);

		offset += size;
		length -= size;
		sg_dma_addr += sg->length;

		if (length == 0)
			break;
	}
}

int rknpu3_memory_sync(struct rknpu3_session *session, struct rknpu3_mem_sync *mem_sync)
{
	struct rknpu3_device *dev;
	struct dma_buf *dmabuf = NULL;
	struct rknpu3_dma_buf_priv *priv = NULL;
	struct rknpu3_imported_buf *imported = NULL;
	struct rknpu3_imported_buf *it;
	struct sg_table *sgt = NULL;

	if (!session || !mem_sync || !mem_sync->size)
		return -EINVAL;

	dev = session->rknpu3_dev;

	if (mem_sync->flags & ~RKNPU3_MEM_SYNC_MASK) {
		LOG_DEV_ERROR(dev->dev, "invalid mem sync flags: 0x%x\n",
			      mem_sync->flags);
		return -EINVAL;
	}

	/* First try to find in session's imported buffers list by dma_addr */
	if (mem_sync->dma_addr) {
		mutex_lock(&session->imported_bufs_lock);
		list_for_each_entry(it, &session->imported_bufs, node) {
			if (it->dma_addr == (dma_addr_t)mem_sync->dma_addr) {
				imported = it;
				break;
			}
		}

		if (imported && imported->sgt) {
			sgt = imported->sgt;
			if (mem_sync->flags & RKNPU3_MEM_SYNC_TO_DEVICE)
				rknpu3_dma_buf_sync(dev->dev, sgt,
						   mem_sync->offset,
						   mem_sync->size,
						   DMA_TO_DEVICE, false);
			if (mem_sync->flags & RKNPU3_MEM_SYNC_FROM_DEVICE)
				rknpu3_dma_buf_sync(dev->dev, sgt,
						   mem_sync->offset,
						   mem_sync->size,
						   DMA_FROM_DEVICE, true);
			mutex_unlock(&session->imported_bufs_lock);
			return 0;
		}
		mutex_unlock(&session->imported_bufs_lock);
	}

	/* Try to find locally allocated buffer via dma_buf_fd */
	if (mem_sync->dma_buf_fd >= 0) {
		dmabuf = dma_buf_get(mem_sync->dma_buf_fd);
		if (!IS_ERR(dmabuf)) {
			priv = dmabuf->priv;
			if (priv) {
				/* For uncached (coherent) memory, no sync needed */
				if (priv->is_uncached) {
					dma_buf_put(dmabuf);
					return 0;
				}

				/* For write-combine memory, sync via sgt */
				sgt = priv->sgt;
				if (sgt && priv->sgt_mapped) {
					if (mem_sync->flags & RKNPU3_MEM_SYNC_TO_DEVICE)
						rknpu3_dma_buf_sync(dev->dev, sgt,
								   mem_sync->offset,
								   mem_sync->size,
								   DMA_TO_DEVICE,
								   false);
					if (mem_sync->flags & RKNPU3_MEM_SYNC_FROM_DEVICE)
						rknpu3_dma_buf_sync(dev->dev, sgt,
								   mem_sync->offset,
								   mem_sync->size,
								   DMA_FROM_DEVICE,
								   true);
				}
			}
			dma_buf_put(dmabuf);
			return 0;
		}
	}

	/* No valid buffer found */
	LOG_DEV_WARN(dev->dev, "sync: buffer not found, dma_addr=0x%llx\n",
		     mem_sync->dma_addr);
	return -EINVAL;
}

void rknpu3_memory_module_deinit(struct rknpu3_device *dev)
{
	unsigned long flags;
	int i;

	if (!dev)
		return;

	/* Acquire lock */
	spin_lock_irqsave(&dev->dev_lock, flags);

	/* Clear memory usage statistics */
	dev->used_memory = 0;
	for (i = 0; i < dev->num_cores; i++)
		dev->core_memory_usage[i] = 0;

	spin_unlock_irqrestore(&dev->dev_lock, flags);
}

int rknpu3_memory_get_all_core_usage(struct rknpu3_device *dev,
				    struct rknpu3_mem_usage *usage)
{
	unsigned long flags;
	int i;
	u64 per_core_total;

	if (!dev || !usage)
		return -EINVAL;

	spin_lock_irqsave(&dev->dev_lock, flags);

	usage->actual_core_count = dev->num_cores;

	/* total_memory of 0 means no limit, display as 0 */
	if (dev->num_cores > 0 && dev->total_memory)
		per_core_total = div_u64(dev->total_memory, dev->num_cores);
	else
		per_core_total = 0;

	for (i = 0; i < dev->num_cores; i++) {
		usage->core_usage[i].core_id = i;
		usage->core_usage[i].total_size = per_core_total;
		usage->core_usage[i].used_size = dev->core_memory_usage[i];
	}

	spin_unlock_irqrestore(&dev->dev_lock, flags);
	return 0;
}

int rknpu3_memory_import_dmabuf(struct rknpu3_session *session,
			       struct rknpu3_mem_import *mem_import)
{
	struct rknpu3_device *dev;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;
	struct rknpu3_imported_buf *imported = NULL;
	dma_addr_t dma_addr;
	int ret = 0;

	if (!session || !mem_import)
		return -EINVAL;

	dev = session->rknpu3_dev;

	if (mem_import->dma_buf_fd < 0)
		return -EINVAL;

	/* Get dma_buf object */
	dmabuf = dma_buf_get(mem_import->dma_buf_fd);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		LOG_DEV_ERROR(dev->dev, "Failed to get dma_buf: %d\n", ret);
		return ret;
	}

	/* Create attachment */
	attach = dma_buf_attach(dmabuf, dev->dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		LOG_DEV_ERROR(dev->dev, "Failed to attach dma_buf: %d\n", ret);
		goto err_put_dmabuf;
	}

	/* Map to DMA address space */
	sgt = dma_buf_map_attachment_unlocked(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		LOG_DEV_ERROR(dev->dev, "Failed to map dma_buf: %d\n", ret);
		goto err_detach;
	}

	/* Get DMA address */
	dma_addr = sg_dma_address(sgt->sgl);
	if (!dma_addr) {
		LOG_DEV_ERROR(dev->dev, "Invalid DMA address\n");
		ret = -EINVAL;
		goto err_unmap;
	}

	if (!dev->iommu_en && sgt->nents > 1) {
		LOG_DEV_ERROR(dev->dev,
			      "Non-IOMMU mode requires contiguous DMA buffer\n");
		ret = -EINVAL;
		goto err_unmap;
	}

	/* Allocate imported buffer management structure */
	imported = kzalloc(sizeof(*imported), GFP_KERNEL);
	if (!imported) {
		ret = -ENOMEM;
		goto err_unmap;
	}

	imported->dmabuf = dmabuf;
	imported->attach = attach;
	imported->sgt = sgt;
	imported->dma_addr = dma_addr;
	imported->size = dmabuf->size;
	INIT_LIST_HEAD(&imported->node);
	imported->in_list = false;

	/* Map to kernel virtual address space */
	ret = dma_buf_vmap_unlocked(dmabuf, &imported->map);
	if (ret == 0) {
		imported->cpu_addr = imported->map.is_iomem ?
				     imported->map.vaddr_iomem :
				     imported->map.vaddr;
		imported->mapped = true;
	} else {
		imported->cpu_addr = NULL;
		imported->mapped = false;
		LOG_DEV_WARN(dev->dev,
			     "dma_buf_vmap_unlocked failed: %d, cpu access not available\n",
			     ret);
		/* Don't block, continue without cpu_addr */
	}

	/* Return info to userspace */
	mem_import->size = dmabuf->size;
	mem_import->dma_addr = (uint64_t)dma_addr;
	mem_import->handle = (uint64_t)(uintptr_t)imported;
	mem_import->virt_kaddr = (uint64_t)(uintptr_t)imported->cpu_addr;

	LOG_DEV_INFO(dev->dev,
		     "Imported dma_buf: fd=%d size=%zu dma_addr=0x%llx\n",
		     mem_import->dma_buf_fd, imported->size, (u64)dma_addr);

	/* Add to session's imported buffer list */
	mutex_lock(&session->imported_bufs_lock);
	list_add_tail(&imported->node, &session->imported_bufs);
	imported->in_list = true;
	mutex_unlock(&session->imported_bufs_lock);

	return 0;

err_unmap:
	dma_buf_unmap_attachment_unlocked(attach, sgt, DMA_BIDIRECTIONAL);
err_detach:
	dma_buf_detach(dmabuf, attach);
err_put_dmabuf:
	dma_buf_put(dmabuf);
	return ret;
}

int rknpu3_memory_release_imported(struct rknpu3_session *session, uint64_t handle)
{
	struct rknpu3_device *dev;
	struct rknpu3_imported_buf *imported = NULL;
	struct rknpu3_imported_buf *it;
	bool found = false;

	if (!session || !handle)
		return -EINVAL;

	dev = session->rknpu3_dev;

	/*
	 * Validate handle by searching in session's imported_bufs list.
	 * This prevents use-after-free if user passes invalid handle.
	 */
	mutex_lock(&session->imported_bufs_lock);
	list_for_each_entry(it, &session->imported_bufs, node) {
		if ((uint64_t)(uintptr_t)it == handle) {
			imported = it;
			found = true;
			list_del_init(&imported->node);
			imported->in_list = false;
			break;
		}
	}
	mutex_unlock(&session->imported_bufs_lock);

	if (!found) {
		LOG_DEV_WARN(dev->dev,
			     "Invalid imported buffer handle: 0x%llx\n", handle);
		return -EINVAL;
	}

	LOG_DEV_DEBUG(dev->dev,
		      "Releasing imported dma_buf: size=%zu dma_addr=0x%llx\n",
		      imported->size, (u64)imported->dma_addr);

	/* Unmap kernel virtual address */
	if (imported->mapped && imported->dmabuf) {
		dma_buf_vunmap_unlocked(imported->dmabuf, &imported->map);
		imported->mapped = false;
		imported->cpu_addr = NULL;
	}

	/* Unmap DMA */
	if (imported->sgt && imported->attach)
		dma_buf_unmap_attachment_unlocked(imported->attach, imported->sgt,
					 DMA_BIDIRECTIONAL);

	/* Detach */
	if (imported->attach && imported->dmabuf)
		dma_buf_detach(imported->dmabuf, imported->attach);

	/* Release reference */
	if (imported->dmabuf)
		dma_buf_put(imported->dmabuf);

	kfree(imported);
	return 0;
}

/**
 * rknpu3_session_release_all_imports() - Release all imported buffers in session
 * @session: Session to clean up
 *
 * This function releases all imported DMA-BUFs associated with a session.
 * Called during session close to clean up any leaked imports.
 */
void rknpu3_session_release_all_imports(struct rknpu3_session *session)
{
	struct rknpu3_device *dev;
	struct rknpu3_imported_buf *imported, *tmp;

	if (!session)
		return;

	dev = session->rknpu3_dev;

	mutex_lock(&session->imported_bufs_lock);
	list_for_each_entry_safe(imported, tmp, &session->imported_bufs, node) {
		LOG_DEV_WARN(dev->dev,
			     "Auto-releasing leaked imported buffer: size=%zu dma_addr=0x%llx\n",
			     imported->size, (u64)imported->dma_addr);

		list_del_init(&imported->node);
		imported->in_list = false;

		/* Unmap kernel virtual address */
		if (imported->mapped && imported->dmabuf)
			dma_buf_vunmap_unlocked(imported->dmabuf, &imported->map);
		if (imported->sgt && imported->attach)
			dma_buf_unmap_attachment_unlocked(imported->attach, imported->sgt,
						 DMA_BIDIRECTIONAL);
		if (imported->attach && imported->dmabuf)
			dma_buf_detach(imported->dmabuf, imported->attach);
		if (imported->dmabuf)
			dma_buf_put(imported->dmabuf);
		kfree(imported);
	}
	mutex_unlock(&session->imported_bufs_lock);
}

/**
 * rknpu3_memory_get_kernel_addr() - Get kernel virtual address by dma_buf_fd
 * @dma_buf_fd: dma_buf file descriptor
 *
 * Return: Kernel virtual address on success, NULL on failure
 */
void *rknpu3_memory_get_kernel_addr(int dma_buf_fd)
{
	struct dma_buf *dmabuf;
	struct rknpu3_dma_buf_priv *priv;

	if (dma_buf_fd < 0)
		return NULL;

	dmabuf = dma_buf_get(dma_buf_fd);
	if (IS_ERR(dmabuf))
		return NULL;

	priv = dmabuf->priv;
	dma_buf_put(dmabuf);

	if (!priv)
		return NULL;

	return priv->cpu_addr;
}
