// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF System heap exporter
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2012, 2019, 2020 Linaro Ltd.
 *
 * Also utilizing parts of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 * Author: Simon Xue <xxm@rock-chips.com>
 */

#include <linux/cma.h>
#include <linux/dma-buf.h>
#include <linux/dma-map-ops.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <uapi/linux/rk-dma-heap.h>
#include <linux/proc_fs.h>
#include "../../../mm/cma.h"
#include "rk-dma-heap.h"

struct rk_system_heap {
	struct rk_dma_heap *heap;
};

static int rk_system_heap_remove_pages_list(struct rk_dma_heap *heap,
					    struct page *page)
{
	struct rk_dma_heap_pages_buf *buf;

	mutex_lock(&heap->pages_lock);
	list_for_each_entry(buf, &heap->pages_list, node) {
		if (buf->start == page_to_phys(page)) {
			dma_heap_print("<%s> free pages %ld to system\n",
				       buf->orig_alloc, buf->size);
			list_del(&buf->node);
			kfree(buf->orig_alloc);
			kfree(buf);
			break;
		}
	}
	mutex_unlock(&heap->pages_lock);

	return 0;
}

static int rk_system_heap_add_pages_list(struct rk_dma_heap *heap,
					 struct page *first_page,
					 unsigned long size, const char *name)
{
	struct rk_dma_heap_pages_buf *buf;
	const char *name_tmp;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	INIT_LIST_HEAD(&buf->node);
	if (!name)
		name_tmp = current->comm;
	else
		name_tmp = name;

	buf->orig_alloc = kstrndup(name_tmp, RK_DMA_HEAP_NAME_LEN, GFP_KERNEL);
	if (!buf->orig_alloc) {
		kfree(buf);
		return -ENOMEM;
	}

	buf->size = size;
	buf->start = page_to_phys(first_page);

	mutex_lock(&heap->pages_lock);
	list_add_tail(&buf->node, &heap->pages_list);
	mutex_unlock(&heap->pages_lock);

	dma_heap_print("<%s> alloc %ld from system\n", buf->orig_alloc, size);

	return 0;
}


static int rk_system_heap_allocate_pages(struct rk_dma_heap *heap,
					 struct page **pages,
					 size_t size, gfp_t flags,
					 const char *name)
{
	int ret;
	unsigned int last_page = 0;
	struct page *first_page = NULL;
	unsigned long num_pages = size >> PAGE_SHIFT;
	size_t raw_size = size;

	while (size > 0) {
		struct page *page;
		int order;
		int i;

		order = get_order(size);
		/* Don't over allocate*/
		if ((PAGE_SIZE << order) > size && order > 0)
			order--;

		page = NULL;
		while (!page) {
			page = alloc_pages(GFP_KERNEL | __GFP_ZERO |
					__GFP_NOWARN | flags, order);
			if (page) {
				if (!first_page)
					first_page = page;
				break;
			}

			if (order == 0) {
				while (last_page--)
					__free_page(pages[last_page]);
				return -ENOMEM;
			}
			order--;
		}

		split_page(page, order);
		for (i = 0; i < (1 << order); i++)
			pages[last_page++] = &page[i];

		size -= PAGE_SIZE << order;
	}

	ret = rk_system_heap_add_pages_list(heap, first_page, raw_size, name);
	if (ret)
		goto free_pages;

	rk_dma_heap_total_inc(heap, raw_size);

	return 0;

free_pages:
	while (num_pages--) {
		__free_page(pages[num_pages]);
		pages[num_pages] = NULL;
	}

	return ret;
}

static void rk_system_heap_free_pages(struct rk_dma_heap *heap,
				   struct page **pages, unsigned int num_pages)
{
	/* Need more reasonable way */
	rk_system_heap_remove_pages_list(heap, pages[0]);

	rk_dma_heap_total_dec(heap, num_pages << PAGE_SHIFT);

	while (num_pages--) {
		__free_page(pages[num_pages]);
		pages[num_pages] = NULL;
	}
}

static const struct rk_dma_heap_ops rk_system_heap_ops = {
	.alloc_pages = rk_system_heap_allocate_pages,
	.free_pages = rk_system_heap_free_pages,
};

static int set_heap_dev_dma(struct device *heap_dev)
{
	int err = 0;

	if (!heap_dev)
		return -EINVAL;

	dma_coerce_mask_and_coherent(heap_dev, DMA_BIT_MASK(64));

	if (!heap_dev->dma_parms) {
		heap_dev->dma_parms = devm_kzalloc(heap_dev,
						   sizeof(*heap_dev->dma_parms),
						   GFP_KERNEL);
		if (!heap_dev->dma_parms)
			return -ENOMEM;

		err = dma_set_max_seg_size(heap_dev, (unsigned int)DMA_BIT_MASK(64));
		if (err) {
			devm_kfree(heap_dev, heap_dev->dma_parms);
			dev_err(heap_dev, "Failed to set DMA segment size, err:%d\n", err);
			return err;
		}
	}

	return 0;
}

static int __rk_add_system_heap(void)
{
	struct rk_system_heap *sytem_heap;
	struct rk_dma_heap_export_info exp_info;
	int ret;

	sytem_heap = kzalloc(sizeof(*sytem_heap), GFP_KERNEL);
	if (!sytem_heap)
		return -ENOMEM;

	exp_info.name = "rk-system-heap";
	exp_info.ops = &rk_system_heap_ops;
	exp_info.priv = sytem_heap;
	exp_info.permit_noalloc = true;

	sytem_heap->heap = rk_dma_heap_add(&exp_info);
	if (IS_ERR(sytem_heap->heap)) {
		ret = PTR_ERR(sytem_heap->heap);

		kfree(sytem_heap);
		return ret;
	}

	ret = set_heap_dev_dma(rk_dma_heap_get_dev(sytem_heap->heap));
	if (ret) {
		rk_dma_heap_put(sytem_heap->heap);
		kfree(sytem_heap);
		return ret;
	}

	return 0;
}

static int __init rk_add_system_heap(void)
{
	return __rk_add_system_heap();
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP) && !defined(CONFIG_INITCALL_ASYNC)
subsys_initcall(rk_add_system_heap);
#else
module_init(rk_add_system_heap);
#endif

MODULE_DESCRIPTION("RockChip DMA-BUF SYSTEM Heap");
MODULE_LICENSE("GPL");
