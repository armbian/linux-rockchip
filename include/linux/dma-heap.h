/* SPDX-License-Identifier: GPL-2.0 */
/*
 * DMABUF Heaps Allocation Infrastructure
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019 Linaro Ltd.
 */

#ifndef _DMA_HEAPS_H
#define _DMA_HEAPS_H

#include <linux/types.h>
#include <uapi/linux/dma-heap.h>

struct dma_heap;

/**
 * struct dma_heap_ops - ops to operate on a given heap
 * @allocate:	allocate dmabuf and return struct dma_buf ptr
 *
 * allocate returns dmabuf on success, ERR_PTR(-errno) on error.
 */
struct dma_heap_ops {
	struct dma_buf *(*allocate)(struct dma_heap *heap,
				    unsigned long len,
				    u32 fd_flags,
				    u64 heap_flags);
#if IS_ENABLED(CONFIG_NO_GKI)
	int (*get_phys)(struct dma_heap *heap, struct dma_heap_phys_data *phys);
#endif
};

/**
 * struct dma_heap_export_info - information needed to export a new dmabuf heap
 * @name:	used for debugging/device-node name
 * @ops:	ops struct for this heap
 * @priv:	heap exporter private data
 *
 * Information needed to export a new dmabuf heap.
 */
struct dma_heap_export_info {
	const char *name;
	const struct dma_heap_ops *ops;
	void *priv;
};

void *dma_heap_get_drvdata(struct dma_heap *heap);

/**
 * dma_heap_get_dev() - get device struct for the heap
 * @heap: DMA-Heap to retrieve device struct from
 *
 * Returns:
 * The device struct for the heap.
 */
struct device *dma_heap_get_dev(struct dma_heap *heap);

const char *dma_heap_get_name(struct dma_heap *heap);

struct dma_heap *dma_heap_add(const struct dma_heap_export_info *exp_info);

/**
 * dma_heap_find - Returns the registered dma_heap with the specified name
 * @name: Name of the heap to find
 *
 */
struct dma_heap *dma_heap_find(const char *name);

/**
 * dma_heap_buffer_alloc - Allocate dma-buf from a dma_heap
 * @heap:	dma_heap to allocate from
 * @len:	size to allocate
 * @fd_flags:	flags to set on returned dma-buf fd
 * @heap_flags:	flags to pass to the dma heap
 *
 * This is for internal dma-buf allocations only.
 */
struct dma_buf *dma_heap_buffer_alloc(struct dma_heap *heap, size_t len,
				      u32 fd_flags,
				      u64 heap_flags);
#endif /* _DMA_HEAPS_H */
