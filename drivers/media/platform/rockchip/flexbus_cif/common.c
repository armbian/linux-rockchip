// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip Flexbus CIF Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <media/videobuf2-cma-sg.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/of_platform.h>
#include "dev.h"
#include "common.h"

MODULE_IMPORT_NS(DMA_BUF);

static void flexbus_cif_init_dummy_vb2(struct flexbus_cif_device *dev,
				struct flexbus_cif_dummy_buffer *buf)
{
	unsigned long attrs = buf->is_need_vaddr ? 0 : DMA_ATTR_NO_KERNEL_MAPPING;

	memset(&buf->vb2_queue, 0, sizeof(struct vb2_queue));
	memset(&buf->vb, 0, sizeof(struct vb2_buffer));
	buf->vb2_queue.gfp_flags = GFP_KERNEL | GFP_DMA32;
	buf->vb2_queue.dma_dir = DMA_BIDIRECTIONAL;
	attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
	buf->vb2_queue.dma_attrs = attrs;
	buf->vb.vb2_queue = &buf->vb2_queue;
}

int flexbus_cif_alloc_buffer(struct flexbus_cif_device *dev,
		       struct flexbus_cif_dummy_buffer *buf)
{
	const struct vb2_mem_ops *g_ops = &vb2_cma_sg_memops;
	struct sg_table	 *sg_tbl;
	void *mem_priv;
	int ret = 0;

	if (!buf->size) {
		ret = -EINVAL;
		goto err;
	}

	flexbus_cif_init_dummy_vb2(dev, buf);
	buf->size = PAGE_ALIGN(buf->size);
	mem_priv = g_ops->alloc(&buf->vb, dev->dev, buf->size);
	if (IS_ERR_OR_NULL(mem_priv)) {
		ret = -ENOMEM;
		goto err;
	}

	buf->mem_priv = mem_priv;
	sg_tbl = (struct sg_table *)g_ops->cookie(&buf->vb, mem_priv);
	buf->dma_addr = sg_dma_address(sg_tbl->sgl);
	g_ops->prepare(mem_priv);
	if (buf->is_need_vaddr)
		buf->vaddr = g_ops->vaddr(&buf->vb, mem_priv);
	if (buf->is_need_dbuf) {
		buf->dbuf = g_ops->get_dmabuf(&buf->vb, mem_priv, O_RDWR);
		if (buf->is_need_dmafd) {
			buf->dma_fd = dma_buf_fd(buf->dbuf, O_CLOEXEC);
			if (buf->dma_fd < 0) {
				dma_buf_put(buf->dbuf);
				ret = buf->dma_fd;
				goto err;
			}
			get_dma_buf(buf->dbuf);
		}
	}
	v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev,
		 "%s buf:0x%x~0x%x size:%d\n", __func__,
		 (u32)buf->dma_addr, (u32)buf->dma_addr + buf->size, buf->size);
	return ret;
err:
	dev_err(dev->dev, "%s failed ret:%d\n", __func__, ret);
	return ret;
}

void flexbus_cif_free_buffer(struct flexbus_cif_device *dev,
			struct flexbus_cif_dummy_buffer *buf)
{
	const struct vb2_mem_ops *g_ops = &vb2_cma_sg_memops;

	if (buf && buf->mem_priv) {
		v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev,
			 "%s buf:0x%x~0x%x\n", __func__,
			 (u32)buf->dma_addr, (u32)buf->dma_addr + buf->size);
		if (buf->dbuf)
			dma_buf_put(buf->dbuf);
		g_ops->put(buf->mem_priv);
		buf->size = 0;
		buf->dbuf = NULL;
		buf->vaddr = NULL;
		buf->mem_priv = NULL;
		buf->is_need_dbuf = false;
		buf->is_need_vaddr = false;
		buf->is_need_dmafd = false;
		buf->is_free = true;
	}
}

