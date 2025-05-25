// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * Author: Huang Lee <Putin.li@rock-chips.com>
 */

#include "rga_dma_buf.h"
#include "rga.h"
#include "rga_common.h"
#include "rga_job.h"
#include "rga_debugger.h"

int rga_buf_size_cal(unsigned long yrgb_addr, unsigned long uv_addr,
		      unsigned long v_addr, int format, uint32_t w,
		      uint32_t h, unsigned long *StartAddr, unsigned long *size)
{
	uint32_t size_yrgb = 0;
	uint32_t size_uv = 0;
	uint32_t size_v = 0;
	uint32_t stride = 0;
	unsigned long start, end;
	uint32_t pageCount;

	switch (format) {
	case RGA_FORMAT_RGBA_8888:
	case RGA_FORMAT_RGBX_8888:
	case RGA_FORMAT_BGRA_8888:
	case RGA_FORMAT_BGRX_8888:
	case RGA_FORMAT_ARGB_8888:
	case RGA_FORMAT_XRGB_8888:
	case RGA_FORMAT_ABGR_8888:
	case RGA_FORMAT_XBGR_8888:
		stride = (w * 4 + 3) & (~3);
		size_yrgb = stride * h;
		start = yrgb_addr >> PAGE_SHIFT;
		end = yrgb_addr + size_yrgb;
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_RGB_888:
	case RGA_FORMAT_BGR_888:
		stride = (w * 3 + 3) & (~3);
		size_yrgb = stride * h;
		start = yrgb_addr >> PAGE_SHIFT;
		end = yrgb_addr + size_yrgb;
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_RGB_565:
	case RGA_FORMAT_RGBA_5551:
	case RGA_FORMAT_RGBA_4444:
	case RGA_FORMAT_BGR_565:
	case RGA_FORMAT_BGRA_5551:
	case RGA_FORMAT_BGRA_4444:
	case RGA_FORMAT_ARGB_5551:
	case RGA_FORMAT_ARGB_4444:
	case RGA_FORMAT_ABGR_5551:
	case RGA_FORMAT_ABGR_4444:
		stride = (w * 2 + 3) & (~3);
		size_yrgb = stride * h;
		start = yrgb_addr >> PAGE_SHIFT;
		end = yrgb_addr + size_yrgb;
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;

		/* YUV FORMAT */
	case RGA_FORMAT_YCbCr_422_SP:
	case RGA_FORMAT_YCrCb_422_SP:
		stride = (w + 3) & (~3);
		size_yrgb = stride * h;
		size_uv = stride * h;
		start = min(yrgb_addr, uv_addr);
		start >>= PAGE_SHIFT;
		end = max((yrgb_addr + size_yrgb), (uv_addr + size_uv));
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_YCbCr_422_P:
	case RGA_FORMAT_YCrCb_422_P:
		stride = (w + 3) & (~3);
		size_yrgb = stride * h;
		size_uv = ((stride >> 1) * h);
		size_v = ((stride >> 1) * h);
		start = min3(yrgb_addr, uv_addr, v_addr);
		start = start >> PAGE_SHIFT;
		end =
			max3((yrgb_addr + size_yrgb), (uv_addr + size_uv),
			(v_addr + size_v));
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_YCbCr_420_SP:
	case RGA_FORMAT_YCrCb_420_SP:
		stride = (w + 3) & (~3);
		size_yrgb = stride * h;
		size_uv = (stride * (h >> 1));
		start = min(yrgb_addr, uv_addr);
		start >>= PAGE_SHIFT;
		end = max((yrgb_addr + size_yrgb), (uv_addr + size_uv));
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_YCbCr_420_P:
	case RGA_FORMAT_YCrCb_420_P:
		stride = (w + 3) & (~3);
		size_yrgb = stride * h;
		size_uv = ((stride >> 1) * (h >> 1));
		size_v = ((stride >> 1) * (h >> 1));
		start = min3(yrgb_addr, uv_addr, v_addr);
		start >>= PAGE_SHIFT;
		end =
			max3((yrgb_addr + size_yrgb), (uv_addr + size_uv),
			(v_addr + size_v));
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_YCbCr_400:
	case RGA_FORMAT_Y8:
		stride = (w + 3) & (~3);
		size_yrgb = stride * h;
		start = yrgb_addr >> PAGE_SHIFT;
		end = yrgb_addr + size_yrgb;
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_Y4:
		stride = ((w + 3) & (~3)) >> 1;
		size_yrgb = stride * h;
		start = yrgb_addr >> PAGE_SHIFT;
		end = yrgb_addr + size_yrgb;
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_YVYU_422:
	case RGA_FORMAT_VYUY_422:
	case RGA_FORMAT_YUYV_422:
	case RGA_FORMAT_UYVY_422:
		stride = (w + 3) & (~3);
		size_yrgb = stride * h;
		size_uv = stride * h;
		start = min(yrgb_addr, uv_addr);
		start >>= PAGE_SHIFT;
		end = max((yrgb_addr + size_yrgb), (uv_addr + size_uv));
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_YVYU_420:
	case RGA_FORMAT_VYUY_420:
	case RGA_FORMAT_YUYV_420:
	case RGA_FORMAT_UYVY_420:
		stride = (w + 3) & (~3);
		size_yrgb = stride * h;
		size_uv = (stride * (h >> 1));
		start = min(yrgb_addr, uv_addr);
		start >>= PAGE_SHIFT;
		end = max((yrgb_addr + size_yrgb), (uv_addr + size_uv));
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	case RGA_FORMAT_YCbCr_420_SP_10B:
	case RGA_FORMAT_YCrCb_420_SP_10B:
		stride = (w + 3) & (~3);
		size_yrgb = stride * h;
		size_uv = (stride * (h >> 1));
		start = min(yrgb_addr, uv_addr);
		start >>= PAGE_SHIFT;
		end = max((yrgb_addr + size_yrgb), (uv_addr + size_uv));
		end = (end + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		pageCount = end - start;
		break;
	default:
		pageCount = 0;
		start = 0;
		break;
	}

	*StartAddr = start;

	if (size != NULL)
		*size = size_yrgb + size_uv + size_v;

	return pageCount;
}

int rga_virtual_memory_check(void *vaddr, u32 w, u32 h, u32 format, int fd)
{
	int bits = 32;
	int temp_data = 0;
	void *one_line = NULL;

	bits = rga_get_format_bits(format);
	if (bits < 0)
		return -1;

	one_line = kzalloc(w * 4, GFP_KERNEL);
	if (!one_line) {
		rga_err("kzalloc fail %s[%d]\n", __func__, __LINE__);
		return 0;
	}

	temp_data = w * (h - 1) * bits >> 3;
	if (fd > 0) {
		rga_log("vaddr is%p, bits is %d, fd check\n", vaddr, bits);
		memcpy(one_line, (char *)vaddr + temp_data, w * bits >> 3);
		rga_log("fd check ok\n");
	} else {
		rga_log("vir addr memory check.\n");
		memcpy((void *)((char *)vaddr + temp_data), one_line,
			 w * bits >> 3);
		rga_log("vir addr check ok.\n");
	}

	kfree(one_line);
	return 0;
}

int rga_dma_memory_check(struct rga_dma_buffer *rga_dma_buffer, struct rga_img_info_t *img)
{
	int ret = 0;
	void *vaddr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	struct iosys_map map;
#endif
	struct dma_buf *dma_buf;

	dma_buf = rga_dma_buffer->dma_buf;

	if (!IS_ERR_OR_NULL(dma_buf)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		ret = dma_buf_vmap(dma_buf, &map);
		vaddr = ret ? NULL : map.vaddr;
#else
		vaddr = dma_buf_vmap(dma_buf);
#endif
		if (vaddr) {
			ret = rga_virtual_memory_check(vaddr, img->vir_w,
				img->vir_h, img->format, img->yrgb_addr);
		} else {
			rga_err("can't vmap the dma buffer!\n");
			return -EINVAL;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		dma_buf_vunmap(dma_buf, &map);
#else
		dma_buf_vunmap(dma_buf, vaddr);
#endif
	}

	return ret;
}

int rga_dma_map_phys_addr(phys_addr_t phys_addr, size_t size, struct rga_dma_buffer *buffer,
			 enum dma_data_direction dir, struct device *map_dev)
{
	dma_addr_t addr;

	addr = dma_map_resource(map_dev, phys_addr, size, dir, 0);
	if (addr == DMA_MAPPING_ERROR) {
		rga_err("dma_map_resouce failed!\n");
		return -EINVAL;
	}

	buffer->dma_addr = addr;
	buffer->dir = dir;
	buffer->size = size;
	buffer->map_dev = map_dev;

	return 0;
}

void rga_dma_unmap_phys_addr(struct rga_dma_buffer *buffer)
{
	dma_unmap_resource(buffer->map_dev, buffer->dma_addr, buffer->size, buffer->dir, 0);
}

int rga_dma_map_sgt(struct sg_table *sgt, struct rga_dma_buffer *buffer,
		    enum dma_data_direction dir, struct device *map_dev)
{
	int i, ret = 0;
	struct scatterlist *sg = NULL;

	ret = dma_map_sg(map_dev, sgt->sgl, sgt->orig_nents, dir);
	if (ret <= 0) {
		rga_err("dma_map_sg failed! ret = %d\n", ret);
		return ret < 0 ? ret : -EINVAL;
	}
	sgt->nents = ret;

	buffer->sgt = sgt;
	buffer->dma_addr = sg_dma_address(sgt->sgl);
	buffer->dir = dir;
	buffer->size = 0;
	for_each_sgtable_sg(sgt, sg, i)
		buffer->size += sg_dma_len(sg);
	buffer->map_dev = map_dev;

	return 0;
}

void rga_dma_unmap_sgt(struct rga_dma_buffer *buffer)
{
	if (!buffer->sgt)
		return;

	dma_unmap_sg(buffer->map_dev,
		     buffer->sgt->sgl,
		     buffer->sgt->orig_nents,
		     buffer->dir);
}

int rga_dma_map_buf(struct dma_buf *dma_buf, struct rga_dma_buffer *rga_dma_buffer,
		    enum dma_data_direction dir, struct device *map_dev)
{
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;
	struct scatterlist *sg = NULL;
	int i, ret = 0;

	if (dma_buf != NULL) {
		get_dma_buf(dma_buf);
	} else {
		rga_err("dma_buf is invalid[%p]\n", dma_buf);
		return -EINVAL;
	}

	attach = dma_buf_attach(dma_buf, map_dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		rga_err("Failed to attach dma_buf, ret[%d]\n", ret);
		goto err_get_attach;
	}

	sgt = dma_buf_map_attachment(attach, dir);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		rga_err("Failed to map attachment, ret[%d]\n", ret);
		goto err_get_sgt;
	}

	rga_dma_buffer->dma_buf = dma_buf;
	rga_dma_buffer->attach = attach;
	rga_dma_buffer->sgt = sgt;
	rga_dma_buffer->dma_addr = sg_dma_address(sgt->sgl);
	rga_dma_buffer->dir = dir;
	rga_dma_buffer->size = 0;
	for_each_sgtable_sg(sgt, sg, i)
		rga_dma_buffer->size += sg_dma_len(sg);
	rga_dma_buffer->map_dev = map_dev;

	return ret;

err_get_sgt:
	if (attach)
		dma_buf_detach(dma_buf, attach);
err_get_attach:
	if (dma_buf)
		dma_buf_put(dma_buf);

	return ret;
}

int rga_dma_map_fd(int fd, struct rga_dma_buffer *rga_dma_buffer,
		   enum dma_data_direction dir, struct device *map_dev)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;
	struct scatterlist *sg = NULL;
	int i, ret = 0;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR(dma_buf)) {
		ret = PTR_ERR(dma_buf);
		rga_err("Fail to get dma_buf from fd[%d], ret[%d]\n", fd, ret);
		return ret;
	}

	attach = dma_buf_attach(dma_buf, map_dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		rga_err("Failed to attach dma_buf, ret[%d]\n", ret);
		goto err_get_attach;
	}

	sgt = dma_buf_map_attachment(attach, dir);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		rga_err("Failed to map attachment, ret[%d]\n", ret);
		goto err_get_sgt;
	}

	rga_dma_buffer->dma_buf = dma_buf;
	rga_dma_buffer->attach = attach;
	rga_dma_buffer->sgt = sgt;
	rga_dma_buffer->dma_addr = sg_dma_address(sgt->sgl);
	rga_dma_buffer->dir = dir;
	rga_dma_buffer->size = 0;
	for_each_sgtable_sg(sgt, sg, i)
		rga_dma_buffer->size += sg_dma_len(sg);
	rga_dma_buffer->map_dev = map_dev;

	return ret;

err_get_sgt:
	if (attach)
		dma_buf_detach(dma_buf, attach);
err_get_attach:
	if (dma_buf)
		dma_buf_put(dma_buf);

	return ret;
}

void rga_dma_unmap_buf(struct rga_dma_buffer *rga_dma_buffer)
{
	if (rga_dma_buffer->attach && rga_dma_buffer->sgt)
		dma_buf_unmap_attachment(rga_dma_buffer->attach,
					 rga_dma_buffer->sgt,
					 rga_dma_buffer->dir);

	if (rga_dma_buffer->attach) {
		dma_buf_detach(rga_dma_buffer->dma_buf, rga_dma_buffer->attach);
		dma_buf_put(rga_dma_buffer->dma_buf);
	}
}

void rga_dma_sync_flush_range(void *pstart, void *pend, struct rga_scheduler_t *scheduler)
{
	dma_sync_single_for_device(scheduler->dev, virt_to_phys(pstart),
				   pend - pstart, DMA_TO_DEVICE);
}

int rga_dma_free(struct rga_dma_buffer *buffer)
{
	if (buffer == NULL) {
		rga_err("rga_dma_buffer is NULL.\n");
		return -EINVAL;
	}

	dma_free_coherent(buffer->map_dev, buffer->size, buffer->vaddr, buffer->dma_addr);
	buffer->vaddr = NULL;
	buffer->dma_addr = 0;
	buffer->iova = 0;
	buffer->size = 0;
	buffer->map_dev = NULL;

	kfree(buffer);

	return 0;
}

struct rga_dma_buffer *rga_dma_alloc_coherent(struct rga_scheduler_t *scheduler,
					      int size)
{
	size_t align_size;
	dma_addr_t dma_addr;
	struct  rga_dma_buffer *buffer;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return NULL;

	align_size = PAGE_ALIGN(size);
	buffer->vaddr = dma_alloc_coherent(scheduler->dev, align_size, &dma_addr, GFP_KERNEL);
	if (!buffer->vaddr)
		goto fail_dma_alloc;

	buffer->size = align_size;
	buffer->dma_addr = dma_addr;
	buffer->map_dev = scheduler->dev;
	if (scheduler->data->mmu == RGA_IOMMU)
		buffer->iova = buffer->dma_addr;

	return buffer;

fail_dma_alloc:
	kfree(buffer);

	return NULL;
}
