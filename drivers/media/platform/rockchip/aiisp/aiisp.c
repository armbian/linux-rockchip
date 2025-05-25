// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include <linux/version.h>
#include <linux/pm_runtime.h>
#include "regs.h"
#include "aiisp.h"

#define RKAIISP_REQ_BUFS_MIN	2
#define RKAIISP_REQ_BUFS_MAX	8

#define CEIL_DOWN(x, y)		(((x) + ((y) - 1)) / (y))
#define FLOOR_BY(v, r)		(((v) / (r)) * (r))
#define CEIL_BY(v, r)		FLOOR_BY(((v) + (r) - 1), (r))

enum AIISP_OP_MODE {
	AIISP_RUN_MODE_SINGLE,
	AIISP_RUN_MODE_COMBO
};

enum AIISP_MODE {
	AIISP_MODE_MODE0,
	AIISP_MODE_MODE1
};

enum AIISP_LEVEL_MODE0 {
	AIISP_LEVEL_MODE_18x18x1x1,
	AIISP_LEVEL_MODE_18x18x3x3G18,
	AIISP_LEVEL_MODE_18x8x3x3,
	AIISP_LEVEL_MODE_18x4x3x3
};

enum AIISP_LEVEL_MODE1 {
	AIISP_LEVEL_MODE_24x24x1x1,
	AIISP_LEVEL_MODE_24x24x3x3G12,
	AIISP_LEVEL_MODE_24x15x3x3,
	AIISP_LEVEL_MODE_24x4x3x3,
	AIISP_LEVEL_MODE_24x24x3x3G8
};

enum AIISP_RD_CHN_DATA_MODE {
	AIISP_CHN_DATA_MODE_0_8BITS,
	AIISP_CHN_DATA_MODE_1_11BITS,
	AIISP_CHN_DATA_MODE_2_16BITS_NAR
};

enum AIISP_OUT_MODE {
	AIISP_OUT_MODE_BYPASS,
	AIISP_OUT_MODE_ADD_MERGE,
	AIISP_OUT_MODE_DIFF_MERGE
};

enum AIISP_M0_MERGE_MODE {
	AIISP_M0_MERGE_MODE_ADD_MERGE,
	AIISP_M0_MERGE_MODE_DIFF_MERGE
};

enum AIISP_SLICE_MODE {
	AIISP_SLICE_MODE_344,
	AIISP_SLICE_MODE_256,
	AIISP_SLICE_MODE_320
};

enum AIISP_CHN_MODE {
	AIISP_CHN_MODE_BYPASS,
	AIISP_CHN_MODE_UPSAMPLE,
	AIISP_CHN_MODE_SPACE2DEPTH
};

enum AIISP_CHN_NUMBER {
	AIISP_CHN_NUMBER_8,
	AIISP_CHN_NUMBER_15
};

// mi chn data mode
static int bits_tab[3] = {8, 11, 16};

// mi channels mode bypass, upsample, s2d
static int num_tab[3] = {1, 2, 1};
static int den_tab[3] = {1, 1, 2};

static int ext_tab[2][3] = {
	{8, 10, 8},
	{8, 16, 8}
};

// dim 0, mode; dim 1, level mode
static int channels_lut[2][5] = {
	{18, 18, 8, 4, -1},
	{24, 24, 15, 4, 24}
};

// mode and op_mode
static int lst_slice_align_tab[2][2] = {
	{8, 8},
	{6, 12}
};

// dim 0: mode; dim 1: mi chn
static int mi_chns_tab[2][7] = {
	{8, 1, 1, 1, 1, 1, 1},
	{15, 1, 1, 1, 1, 1, 1}
};

static int rkaiisp_free_airms_pool(struct rkaiisp_device *aidev);

static void rkaiisp_update_regs(struct rkaiisp_device *aidev, u32 start, u32 end)
{
	struct rkaiisp_hw_dev *hw_dev = aidev->hw_dev;
	void __iomem *base = hw_dev->base_addr;
	u32 i;

	if (end > RKAIISP_SW_REG_SIZE - 4) {
		dev_err(aidev->dev, "%s out of range\n", __func__);
		return;
	}
	for (i = start; i <= end; i += 4) {
		u32 *val = aidev->sw_base_addr + i;
		u32 *flag = aidev->sw_base_addr + i + RKAIISP_SW_REG_SIZE;

		if (*flag == SW_REG_CACHE)
			writel(*val, base + i);
	}
}

void rkaiisp_update_list_reg(struct rkaiisp_device *aidev)
{
	rkaiisp_update_regs(aidev, AIISP_MI_CTRL, AIISP_MI_CTRL);
	rkaiisp_update_regs(aidev, AIISP_MI_SLICE_CTRL, AIISP_MI_MANUAL_CTRL);
	rkaiisp_update_regs(aidev, AIISP_MI_CORE_WIDTH, AIISP_MI_CORE_HEIGHT);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH0_CTRL, AIISP_MI_RD_CH0_BASE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH0_HEIGHT, AIISP_MI_RD_CH0_STRIDE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH1_CTRL, AIISP_MI_RD_CH1_BASE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH1_HEIGHT, AIISP_MI_RD_CH1_STRIDE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH2_CTRL, AIISP_MI_RD_CH2_BASE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH2_HEIGHT, AIISP_MI_RD_CH2_STRIDE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH3_CTRL, AIISP_MI_RD_CH3_BASE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH3_HEIGHT, AIISP_MI_RD_CH3_STRIDE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH4_CTRL, AIISP_MI_RD_CH4_BASE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH4_HEIGHT, AIISP_MI_RD_CH4_STRIDE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH5_CTRL, AIISP_MI_RD_CH5_BASE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH5_HEIGHT, AIISP_MI_RD_CH5_STRIDE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH6_CTRL, AIISP_MI_RD_CH6_BASE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_CH6_HEIGHT, AIISP_MI_RD_CH6_STRIDE);
	rkaiisp_update_regs(aidev, AIISP_MI_RD_KWT_CTRL, AIISP_MI_RD_KWT_STRIDE);
	rkaiisp_update_regs(aidev, AIISP_MI_WR_CTRL, AIISP_MI_WR_CTRL);
	rkaiisp_update_regs(aidev, AIISP_MI_CHN0_WR_CRTL, AIISP_MI_CHN0_WR_STRIDE);

	rkaiisp_update_regs(aidev, AIISP_CORE_CTRL, AIISP_CORE_CTRL);
	rkaiisp_update_regs(aidev, AIISP_CORE_LEVEL_CTRL0, AIISP_CORE_LEVEL_CTRL3);
	rkaiisp_update_regs(aidev, AIISP_CORE_OUT_CTRL, AIISP_CORE_NOISE_LMT);
	rkaiisp_update_regs(aidev, AIISP_CORE_COMP0, AIISP_CORE_COMP16);
	rkaiisp_update_regs(aidev, AIISP_CORE_DECOMP0, AIISP_CORE_DECOMP16);

	rkaiisp_write(aidev, AIISP_MI_IMSC, AIISP_MI_ISR_ALL, true);
	rkaiisp_write(aidev, AIISP_MI_WR_INIT, AIISP_MI_CHN0SELF_FORCE_UPD, true);
	rkaiisp_write(aidev, AIISP_MI_RD_START, AIISP_MI_RD_START_EN, true);
}

static void rkaiisp_dumpreg(struct rkaiisp_device *aidev, u32 start, u32 end)
{
	u32 i, val;

	if (end > RKAIISP_SW_REG_SIZE - 4) {
		dev_err(aidev->dev, "%s out of range\n", __func__);
		return;
	}

	for (i = start; i <= end; i += 4) {
		val = rkaiisp_read(aidev, i, false);
		dev_info(aidev->dev, "%08x: %08x\n", i, val);
	}
}

static void rkaiisp_dump_list_reg(struct rkaiisp_device *aidev)
{
	dev_info(aidev->dev, "frame_id: %d, run_idx: %d\n",
		 aidev->frame_id, aidev->run_idx);

	rkaiisp_dumpreg(aidev, AIISP_CORE_CTRL, AIISP_CORE_NOISE_LMT);
	rkaiisp_dumpreg(aidev, AIISP_CORE_COMP0, AIISP_CORE_DECOMP16);
	rkaiisp_dumpreg(aidev, AIISP_MI_HURRY_CTRL, AIISP_MI_ISR);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_START, AIISP_MI_CORE_HEIGHT);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_CH0_CTRL, AIISP_MI_RD_CH0_STRIDE);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_CH1_CTRL, AIISP_MI_RD_CH1_STRIDE);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_CH2_CTRL, AIISP_MI_RD_CH2_STRIDE);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_CH3_CTRL, AIISP_MI_RD_CH3_STRIDE);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_CH4_CTRL, AIISP_MI_RD_CH4_STRIDE);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_CH5_CTRL, AIISP_MI_RD_CH5_STRIDE);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_CH6_CTRL, AIISP_MI_RD_CH6_STRIDE);
	rkaiisp_dumpreg(aidev, AIISP_MI_RD_KWT_CTRL, AIISP_MI_RD_KWT_STRIDE);
	rkaiisp_dumpreg(aidev, AIISP_MI_WR_CTRL, AIISP_MI_CHN0_WR_STRIDE);
}

static int rkaiisp_buf_get_fd(struct rkaiisp_device *aidev,
			      struct rkaiisp_dummy_buffer *buf, bool try_fd)
{
	const struct vb2_mem_ops *g_ops = aidev->hw_dev->mem_ops;
	bool new_dbuf = false;

	if (!buf || !buf->mem_priv)
		return -EINVAL;
	if (try_fd) {
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
	}

	if (buf->is_need_dbuf && !buf->dmabuf) {
		buf->dmabuf = g_ops->get_dmabuf(&buf->vb, buf->mem_priv, O_RDWR);
		new_dbuf = true;
	}

	if (buf->is_need_dmafd) {
		buf->dma_fd = dma_buf_fd(buf->dmabuf, O_CLOEXEC);
		if (buf->dma_fd < 0) {
			if (new_dbuf) {
				dma_buf_put(buf->dmabuf);
				buf->dmabuf = NULL;
				buf->is_need_dbuf = false;
			}
			buf->is_need_dmafd = false;
			return -EINVAL;
		}
		get_dma_buf(buf->dmabuf);
	}
	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void rkaiisp_init_dummy_vb2(struct rkaiisp_device *dev,
				   struct rkaiisp_dummy_buffer *buf)
{
	unsigned long attrs = 0;

	memset(&buf->vb2_queue, 0, sizeof(buf->vb2_queue));
	memset(&buf->vb, 0, sizeof(buf->vb));
	buf->vb2_queue.gfp_flags = GFP_KERNEL | GFP_DMA32;
	buf->vb2_queue.dma_dir = DMA_BIDIRECTIONAL;
	if (dev->hw_dev->is_dma_contig)
		attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
	buf->vb2_queue.dma_attrs = attrs;
	buf->vb.vb2_queue = &buf->vb2_queue;
}

static int rkaiisp_allow_buffer(struct rkaiisp_device *aidev,
				struct rkaiisp_dummy_buffer *buf)
{
	const struct vb2_mem_ops *mem_ops = aidev->mem_ops;
	struct sg_table *sg_tbl;
	void *mem_priv;
	int ret = 0;

	if (!buf->size) {
		ret = -EINVAL;
		goto err;
	}

	rkaiisp_init_dummy_vb2(aidev, buf);
	buf->size = PAGE_ALIGN(buf->size);
	mem_priv = mem_ops->alloc(&buf->vb, aidev->hw_dev->dev, buf->size);
	if (IS_ERR_OR_NULL(mem_priv)) {
		ret = -ENOMEM;
		goto err;
	}

	buf->mem_priv = mem_priv;
	sg_tbl = (struct sg_table *)mem_ops->cookie(&buf->vb, mem_priv);
	buf->dma_addr = sg_dma_address(sg_tbl->sgl);
	mem_ops->prepare(mem_priv);
	if (buf->is_need_vaddr)
		buf->vaddr = mem_ops->vaddr(&buf->vb, mem_priv);
	ret = rkaiisp_buf_get_fd(aidev, buf, false);
	if (ret < 0) {
		mem_ops->put(buf->mem_priv);
		buf->mem_priv = NULL;
		buf->vaddr = NULL;
		buf->size = 0;
		goto err;
	}
	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		 "%s buf:%pad size:%d\n", __func__,
		 &buf->dma_addr, buf->size);

	return ret;
err:
	dev_err(aidev->dev, "%s failed ret:%d\n", __func__, ret);
	return ret;
}
#else
static int rkaiisp_allow_buffer(struct rkaiisp_device *aidev,
				struct rkaiisp_dummy_buffer *buf)
{
	const struct vb2_mem_ops *mem_ops = aidev->mem_ops;
	struct sg_table *sg_tbl;
	unsigned long attrs = 0;
	void *mem_priv;
	int ret = 0;

	if (!buf->size) {
		ret = -EINVAL;
		goto err;
	}

	if (aidev->hw_dev->is_dma_contig)
		attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
	buf->size = PAGE_ALIGN(buf->size);
	mem_priv = mem_ops->alloc(aidev->hw_dev->dev, attrs, buf->size,
				  DMA_BIDIRECTIONAL, GFP_KERNEL | GFP_DMA32);
	if (IS_ERR_OR_NULL(mem_priv)) {
		ret = -ENOMEM;
		goto err;
	}

	buf->mem_priv = mem_priv;
	sg_tbl = (struct sg_table *)mem_ops->cookie(mem_priv);
	buf->dma_addr = sg_dma_address(sg_tbl->sgl);
	mem_ops->prepare(mem_priv);
	if (buf->is_need_vaddr)
		buf->vaddr = mem_ops->vaddr(mem_priv);
	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		 "%s buf:%pad size:%d\n", __func__,
		 &buf->dma_addr, buf->size);

	return ret;
err:
	dev_err(aidev->dev, "%s failed ret:%d\n", __func__, ret);
	return ret;
}
#endif

static void rkaiisp_free_buffer(struct rkaiisp_device *aidev,
				struct rkaiisp_dummy_buffer *buf)
{
	const struct vb2_mem_ops *mem_ops = aidev->mem_ops;

	if (buf && buf->mem_priv) {
		v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
			 "%s buf:%pad size:%d\n", __func__,
			 &buf->dma_addr, buf->size);

		if (buf->dmabuf)
			dma_buf_put(buf->dmabuf);
		mem_ops->put(buf->mem_priv);
		buf->size = 0;
		buf->vaddr = NULL;
		buf->dmabuf = NULL;
		buf->mem_priv = NULL;
		buf->is_need_dbuf = false;
		buf->is_need_dmafd = false;
	}
}

static void rkaiisp_detach_dmabuf(struct rkaiisp_device *aidev,
				  struct rkaiisp_dummy_buffer *buffer)
{
	if (buffer->dma_fd >= 0) {
		v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
			 "%s buf:%pad size:%d\n", __func__,
			 &buffer->dma_addr, buffer->size);
		dma_buf_unmap_attachment(buffer->dba, buffer->sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(buffer->dmabuf, buffer->dba);
		dma_buf_put(buffer->dmabuf);
		memset(buffer, 0, sizeof(struct rkaiisp_dummy_buffer));
		buffer->dma_fd = -1;
	}
}

static void rkaiisp_free_tempbuf(struct rkaiisp_device *aidev)
{
	rkaiisp_free_buffer(aidev, &aidev->temp_buf[0]);
	rkaiisp_free_buffer(aidev, &aidev->temp_buf[1]);
}

static int rkaiisp_free_pool(struct rkaiisp_device *aidev)
{
	struct rkaiisp_ispbuf_info *ispbuf = &aidev->ispbuf;
	int i;

	if (aidev->exealgo == AIRMS)
		return rkaiisp_free_airms_pool(aidev);

	if (!aidev->init_buf)
		return 0;

	for (i = 0; i < ispbuf->bnr_buf.iir.buf_cnt; i++)
		rkaiisp_detach_dmabuf(aidev, &aidev->iirbuf[i]);

	for (i = 0; i < ispbuf->bnr_buf.u.v35.aipre_gain.buf_cnt; i++)
		rkaiisp_detach_dmabuf(aidev, &aidev->aiprebuf[i]);

	for (i = 0; i < ispbuf->bnr_buf.u.v35.vpsl.buf_cnt; i++)
		rkaiisp_detach_dmabuf(aidev, &aidev->vpslbuf[i]);

	for (i = 0; i < ispbuf->bnr_buf.u.v35.aiisp.buf_cnt; i++)
		rkaiisp_detach_dmabuf(aidev, &aidev->aiispbuf[i]);

	rkaiisp_free_tempbuf(aidev);
	aidev->init_buf = false;
	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"free buf poll\n");
	return 0;
}

static int rkaiisp_attach_dmabuf(struct rkaiisp_device *aidev,
				 struct rkaiisp_dummy_buffer *buffer)
{
	struct dma_buf_attachment *dba;
	struct dma_buf *dmabuf;
	struct sg_table	*sgt;
	int ret = 0;

	dmabuf = dma_buf_get(buffer->dma_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		v4l2_err(&aidev->v4l2_dev, "invalid dmabuf fd:%d", buffer->dma_fd);
		return -EINVAL;
	}
	buffer->dmabuf = dmabuf;
	dba = dma_buf_attach(dmabuf, aidev->hw_dev->dev);
	if (IS_ERR(dba)) {
		dma_buf_put(dmabuf);
		ret = PTR_ERR(dba);
		return ret;
	}
	buffer->dba = dba;
	sgt = dma_buf_map_attachment(dba, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		dma_buf_detach(dmabuf, dba);
		dma_buf_put(dmabuf);
		return ret;
	}
	buffer->sgt = sgt;
	buffer->dma_addr = sg_dma_address(sgt->sgl);
	buffer->size = sg_dma_len(sgt->sgl);
	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		 "%s buf:%pad size:%d\n", __func__,
		 &buffer->dma_addr, buffer->size);

	return ret;
}

static void rkaiisp_calc_outbuf_size(struct rkaiisp_device *aidev, u32 raw_hgt, u32 raw_wid)
{
	int i;

	if (aidev->model_mode == REMOSAIC_MODE)
		return;

	if (aidev->model_mode == SINGLEX2_MODE) {
		for (i = 0; i < RKAIISP_PYRAMID_LAYER_NUM; i++) {
			if (i == 0) {
				aidev->outbuf_size[i * 2 + 0].height = raw_hgt;
				aidev->outbuf_size[i * 2 + 0].width = raw_wid;
				aidev->outbuf_size[i * 2 + 0].channel = 1;
				aidev->outbuf_size[i * 2 + 0].stride = raw_wid;
				aidev->outbuf_size[i * 2 + 1].height = raw_hgt / 2;
				aidev->outbuf_size[i * 2 + 1].width = raw_wid / 2;
				aidev->outbuf_size[i * 2 + 1].channel = 15;
				aidev->outbuf_size[i * 2 + 1].stride = raw_wid * 15;
			} else {
				aidev->outbuf_size[i * 2 + 0].height = raw_hgt / 2;
				aidev->outbuf_size[i * 2 + 0].width = raw_wid / 2;
				aidev->outbuf_size[i * 2 + 0].channel = 15;
				aidev->outbuf_size[i * 2 + 0].stride = raw_wid * 15;
				aidev->outbuf_size[i * 2 + 1].height = raw_hgt / 2;
				aidev->outbuf_size[i * 2 + 1].width = raw_wid / 2;
				aidev->outbuf_size[i * 2 + 1].channel = 15;
				aidev->outbuf_size[i * 2 + 1].stride = raw_wid * 15;
			}

			raw_hgt = CEIL_BY(CEIL_DOWN(raw_hgt, 2), 2);
			raw_wid = CEIL_BY(CEIL_DOWN(raw_wid, 2), 2);
		}
	} else {
		for (i = 0; i < RKAIISP_PYRAMID_LAYER_NUM; i++) {
			if (i == 0) {
				aidev->outbuf_size[i].height = raw_hgt;
				aidev->outbuf_size[i].width = raw_wid;
				aidev->outbuf_size[i].channel = 1;
				aidev->outbuf_size[i].stride = raw_wid;
			} else {
				aidev->outbuf_size[i].height = raw_hgt / 2;
				aidev->outbuf_size[i].width = raw_wid / 2;
				aidev->outbuf_size[i].channel = 15;
				aidev->outbuf_size[i].stride = raw_wid * 15;
			}

			raw_hgt = CEIL_BY(CEIL_DOWN(raw_hgt, 2), 2);
			raw_wid = CEIL_BY(CEIL_DOWN(raw_wid, 2), 2);
		}
	}
}

static int rkaiisp_init_pool(struct rkaiisp_device *aidev, struct rkaiisp_ispbuf_info *ispbuf)
{
	int i, ret = 0;
	u32 stride;

	for (i = 0; i < ispbuf->bnr_buf.iir.buf_cnt; i++) {
		aidev->iirbuf[i].dma_fd = ispbuf->bnr_buf.iir.buf_fd[i];
		ret = rkaiisp_attach_dmabuf(aidev, &aidev->iirbuf[i]);

		if (ret) {
			rkaiisp_free_pool(aidev);
			v4l2_err(&aidev->v4l2_dev, "attach iirbuf failed: %d\n", ret);
			return -EINVAL;
		}
	}
	for (i = 0; i < ispbuf->bnr_buf.u.v35.aipre_gain.buf_cnt; i++) {
		aidev->aiprebuf[i].dma_fd = ispbuf->bnr_buf.u.v35.aipre_gain.buf_fd[i];
		ret = rkaiisp_attach_dmabuf(aidev, &aidev->aiprebuf[i]);

		if (ret) {
			rkaiisp_free_pool(aidev);
			v4l2_err(&aidev->v4l2_dev, "attach aiprebuf failed: %d\n", ret);
			return -EINVAL;
		}
	}
	for (i = 0; i < ispbuf->bnr_buf.u.v35.vpsl.buf_cnt; i++) {
		aidev->vpslbuf[i].dma_fd = ispbuf->bnr_buf.u.v35.vpsl.buf_fd[i];
		ret = rkaiisp_attach_dmabuf(aidev, &aidev->vpslbuf[i]);

		if (ret) {
			rkaiisp_free_pool(aidev);
			v4l2_err(&aidev->v4l2_dev, "attach vpslbuf failed: %d\n", ret);
			return -EINVAL;
		}
	}
	for (i = 0; i < ispbuf->bnr_buf.u.v35.aiisp.buf_cnt; i++) {
		aidev->aiispbuf[i].dma_fd = ispbuf->bnr_buf.u.v35.aiisp.buf_fd[i];
		ret = rkaiisp_attach_dmabuf(aidev, &aidev->aiispbuf[i]);

		if (ret) {
			rkaiisp_free_pool(aidev);
			v4l2_err(&aidev->v4l2_dev, "attach dmabuf failed: %d\n", ret);
			return -EINVAL;
		}
	}

	stride = ((ispbuf->iir_width + 1) / 2 * 15 * 11 + 7) >> 3;
	aidev->temp_buf[0].size = stride * (ispbuf->iir_height + 1) / 2;
	aidev->temp_buf[1].size = aidev->temp_buf[0].size;
	aidev->temp_buf[0].is_need_vaddr = false;
	aidev->temp_buf[0].is_need_dbuf = false;
	aidev->temp_buf[0].is_need_dmafd = false;
	aidev->temp_buf[1].is_need_vaddr = false;
	aidev->temp_buf[1].is_need_dbuf = false;
	aidev->temp_buf[1].is_need_dmafd = false;
	ret = rkaiisp_allow_buffer(aidev, &aidev->temp_buf[0]);
	ret |= rkaiisp_allow_buffer(aidev, &aidev->temp_buf[1]);
	if (ret)
		rkaiisp_free_pool(aidev);

	aidev->ispbuf = *ispbuf;
	aidev->outbuf_idx = 0;
	aidev->init_buf = true;

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev, "init buf poll\n");
	return ret;
}

static int rkaiisp_free_airms_pool(struct rkaiisp_device *aidev)
{
	int i;

	if (!aidev->init_buf)
		return 0;

	for (i = 0; i < aidev->rmsbuf.inbuf_num; i++)
		rkaiisp_free_buffer(aidev, &aidev->rms_inbuf[i]);

	for (i = 0; i < aidev->rmsbuf.outbuf_num; i++)
		rkaiisp_free_buffer(aidev, &aidev->rms_outbuf[i]);

	rkaiisp_free_buffer(aidev, &aidev->sigma_buf);
	rkaiisp_free_buffer(aidev, &aidev->narmap_buf);

	aidev->init_buf = false;
	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"free buf poll\n");
	return 0;
}

static int rkaiisp_init_airms_pool(struct rkaiisp_device *aidev, struct rkaiisp_rmsbuf_info *rmsbuf)
{
	int i, ret = 0;
	u32 size;

	size = rmsbuf->image_width * rmsbuf->image_height * 2;
	rmsbuf->inbuf_num = RKAIISP_MIN(rmsbuf->inbuf_num, RKAIISP_AIRMS_BUF_MAXCNT);
	for (i = 0; i < rmsbuf->inbuf_num; i++) {
		aidev->rms_inbuf[i].size = size;
		aidev->rms_inbuf[i].is_need_vaddr = false;
		aidev->rms_inbuf[i].is_need_dbuf = true;
		aidev->rms_inbuf[i].is_need_dmafd = true;
		ret = rkaiisp_allow_buffer(aidev, &aidev->rms_inbuf[i]);
		if (ret) {
			rkaiisp_free_airms_pool(aidev);
			v4l2_err(&aidev->v4l2_dev, "alloc buf failed: %d\n", ret);
			return -EINVAL;
		}
		rmsbuf->inbuf_fd[i] = aidev->rms_inbuf[i].dma_fd;
	}

	rmsbuf->outbuf_num = RKAIISP_MIN(rmsbuf->outbuf_num, RKAIISP_AIRMS_BUF_MAXCNT);
	for (i = 0; i < rmsbuf->outbuf_num; i++) {
		aidev->rms_outbuf[i].size = size;
		aidev->rms_outbuf[i].is_need_vaddr = false;
		aidev->rms_outbuf[i].is_need_dbuf = true;
		aidev->rms_outbuf[i].is_need_dmafd = true;
		ret = rkaiisp_allow_buffer(aidev, &aidev->rms_outbuf[i]);
		if (ret) {
			rkaiisp_free_airms_pool(aidev);
			v4l2_err(&aidev->v4l2_dev, "alloc buf failed: %d\n", ret);
			return -EINVAL;
		}
		rmsbuf->outbuf_fd[i] = aidev->rms_outbuf[i].dma_fd;
	}

	aidev->sigma_buf.size = rmsbuf->sigma_width * rmsbuf->sigma_height;
	aidev->sigma_buf.is_need_vaddr = false;
	aidev->sigma_buf.is_need_dbuf = false;
	aidev->sigma_buf.is_need_dmafd = false;
	rkaiisp_allow_buffer(aidev, &aidev->sigma_buf);
	aidev->narmap_buf.size = rmsbuf->narmap_width * rmsbuf->narmap_height;
	aidev->narmap_buf.is_need_vaddr = false;
	aidev->narmap_buf.is_need_dbuf = false;
	aidev->narmap_buf.is_need_dmafd = false;
	rkaiisp_allow_buffer(aidev, &aidev->narmap_buf);

	aidev->rmsbuf = *rmsbuf;
	aidev->init_buf = true;

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev, "init buf poll\n");
	return ret;
}

int rkaiisp_queue_ispbuf(struct rkaiisp_device *aidev, union rkaiisp_queue_buf *idxbuf)
{
	struct kfifo *fifo = &aidev->idxbuf_kfifo;
	struct rkaiisp_hw_dev *hw_dev = aidev->hw_dev;
	unsigned long flags = 0;
	int sequence = 0;
	int ret = 0;

	spin_lock_irqsave(&hw_dev->hw_lock, flags);
	if (!aidev->streamon) {
		spin_unlock_irqrestore(&hw_dev->hw_lock, flags);
		v4l2_err(&aidev->v4l2_dev,
			"rkaiisp device is not stream on\n");
		return -EINVAL;
	}

	if (!kfifo_is_full(fifo))
		kfifo_in(fifo, idxbuf, sizeof(union rkaiisp_queue_buf));
	else
		v4l2_err(&aidev->v4l2_dev, "fifo is full\n");

	if (aidev->exealgo == AIBNR)
		sequence = idxbuf->aibnr_st.sequence;
	else if (aidev->exealgo == AIRMS)
		sequence = idxbuf->airms_st.sequence;

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"idxbuf fifo in: %d\n", sequence);

	if (hw_dev->is_idle) {
		hw_dev->cur_dev_id = aidev->dev_id;
		hw_dev->is_idle = false;
		spin_unlock_irqrestore(&hw_dev->hw_lock, flags);
		rkaiisp_trigger(aidev);
	} else {
		spin_unlock_irqrestore(&hw_dev->hw_lock, flags);
	}

	return ret;
}

static void rkaiisp_gen_slice_param(struct rkaiisp_device *aidev,
				    struct rkaiisp_model_cfg *model_cfg, int width)
{
	int left = width;
	int slice_num = 0;
	int slice_idx = 0;
	int slice_mode[8] = {0};
	int lst_slice_len = 0;
	int mi_lst_exp_num = 0;
	int lext_num_sel;
	int slice_align;
	int least_rexp;
	int align_len;
	u32 value;

	if (model_cfg->sw_aiisp_op_mode == AIISP_RUN_MODE_COMBO) {
		while (left > 0) {
			if (slice_idx == 0) {
				if (left >= 408) {
					slice_mode[slice_idx] = AIISP_SLICE_MODE_344;
					left -= 344;
				} else if (left <= 344) {
					lst_slice_len = left;
					slice_mode[slice_idx] = 0;
					left = 0;
				} else {
					slice_mode[slice_idx] = AIISP_SLICE_MODE_256;
					left -= 256;
				}
			} else {
				if (left >= 384) {
					slice_mode[slice_idx] = AIISP_SLICE_MODE_320;
					left -= 320;
				} else if (left <= 344) {
					lst_slice_len = left;
					slice_mode[slice_idx] = 0;
					left = 0;
				} else {
					slice_mode[slice_idx] = AIISP_SLICE_MODE_256;
					left -= 256;
				}
			}
			slice_idx++;
		}
	} else {
		while (left > 0) {
			if (model_cfg->sw_aiisp_mode == AIISP_MODE_MODE0) {
				if (left >= 384) {
					slice_mode[slice_idx] = AIISP_SLICE_MODE_320;
					left -= 320;
				} else if (left <= 344) {
					slice_mode[slice_idx] = 0;
					lst_slice_len = left;
					left = 0;
				} else {
					slice_mode[slice_idx] = AIISP_SLICE_MODE_256;
					left -= 256;
				}
			} else {
				if (left > 344) {
					slice_mode[slice_idx] = AIISP_SLICE_MODE_256;
					left -= 256;
				} else {
					lst_slice_len = left;
					slice_mode[slice_idx] = 0;
					left = 0;
				}
			}
			slice_idx++;
		}
	}

	if (slice_idx >= 1)
		slice_num = slice_idx - 1;
	value = slice_mode[0] |
		slice_mode[1] << 2 |
		slice_mode[2] << 4 |
		slice_mode[3] << 6 |
		slice_mode[4] << 8 |
		slice_mode[5] << 10 |
		slice_mode[6] << 12 |
		slice_mode[7] << 14 |
		slice_num << 24 |
		AIISP_MODE_MODE1 << 30;
	rkaiisp_write(aidev, AIISP_MI_SLICE_CTRL, value, false);

	lext_num_sel = ext_tab[model_cfg->sw_aiisp_op_mode][slice_mode[slice_num]];
	slice_align = lst_slice_align_tab[model_cfg->sw_aiisp_mode][model_cfg->sw_aiisp_op_mode];
	least_rexp = 8;

	align_len = CEIL_BY(lst_slice_len + lext_num_sel + least_rexp, slice_align);
	mi_lst_exp_num = align_len - lext_num_sel - lst_slice_len;

	value = lst_slice_len << 4 | mi_lst_exp_num << 24;
	rkaiisp_write(aidev, AIISP_MI_MANUAL_CTRL, value, false);
}

static int rkaiisp_determine_size(struct rkaiisp_device *aidev,
				  struct rkaiisp_model_cfg *model_cfg)
{
	int i, type, width, height, den, num;
	int n, dex, odd, bits, cols, chns;
	int sw_mi_chn_height_odd[7] = {0};
	int sw_mi_chn_stride[7] = {0};
	int sw_layer_s2d_flag;
	int out_ch_stride;
	int tmp_cols = 0, tmp_rows = 0;
	int base_cols = 0, base_rows = 0;
	int last_lv_mode, dma_wr_width;
	u32 value;

	for (i = 0; i < RKAIISP_MAX_CHANNEL; i++) {
		if (model_cfg->sw_mi_chn_en[i]) {
			if (model_cfg->sw_aiisp_op_mode == AIISP_RUN_MODE_COMBO) {
				if ((i == 1 && model_cfg->sw_mi_chn1_sel) ||
				    (i == 2) ||
				    (i == 3 && model_cfg->sw_mi_chn3_sel)) {
					type = model_cfg->sw_mi_chn_mode[i];
					width = aidev->chn_size[i].width;
					height = aidev->chn_size[i].height;
					den = den_tab[type];
					num = num_tab[type];

					tmp_cols = width * num / den;
					tmp_rows = height * num / den;
				}
			} else {
				if ((i == 0) ||
				    (i == 1 && model_cfg->sw_mi_chn1_sel) ||
				    (i == 2) ||
				    (i == 3 && model_cfg->sw_mi_chn3_sel) ||
				    (i == 4 && !model_cfg->sw_mi_chn1_sel) ||
				    (i == 6 && !model_cfg->sw_mi_chn3_sel)) {
					type = model_cfg->sw_mi_chn_mode[i];
					width = aidev->chn_size[i].width;
					height = aidev->chn_size[i].height;
					den = den_tab[type];
					num = num_tab[type];

					tmp_cols = width * num / den;
					tmp_rows = height * num / den;
				}
			}
		}
	}

	if ((model_cfg->sw_mi_chn_en[1] &&
	     model_cfg->sw_mi_chn_mode[1] == AIISP_CHN_MODE_SPACE2DEPTH) ||
	    (model_cfg->sw_mi_chn_en[2] &&
	     model_cfg->sw_mi_chn_mode[2] == AIISP_CHN_MODE_SPACE2DEPTH))
		sw_layer_s2d_flag = 1;
	else
		sw_layer_s2d_flag = 0;

	base_cols = tmp_cols;
	base_rows = tmp_rows;
	for (n = 0; n < RKAIISP_MAX_CHANNEL; n++) {
		if (model_cfg->sw_mi_chn_en[n] == 0)
			continue;

		dex = 1;

		if (model_cfg->sw_aiisp_op_mode == AIISP_RUN_MODE_COMBO &&
		    model_cfg->sw_aiisp_mode == AIISP_MODE_MODE1 &&
			(n == 0 || n == 4 || n == 5 || n == 6)) {
			dex = 2;
		}

		type = model_cfg->sw_mi_chn_mode[n];
		den = den_tab[type];
		num = num_tab[type];
		odd = base_rows - aidev->chn_size[n].height * num * dex / den != 0 ? 1 : 0;

		sw_mi_chn_height_odd[n] = odd;

		bits = bits_tab[model_cfg->sw_mi_chn_data_mode[n]];
		cols = aidev->chn_size[n].width;
		chns = mi_chns_tab[model_cfg->sw_aiisp_mode][n];

		if (n == 3 && model_cfg->sw_mi_chn3_sel == 0)
			bits = 8;

		sw_mi_chn_stride[n] = CEIL_BY(cols * chns * bits, 16 * 8) / 32;
	}

	last_lv_mode = model_cfg->sw_aiisp_lv_mode[model_cfg->sw_aiisp_level_num - 1];
	dma_wr_width = 0;

	if (model_cfg->sw_aiisp_mode == AIISP_MODE_MODE1) {
		if (last_lv_mode == AIISP_LEVEL_MODE_24x15x3x3)
			dma_wr_width = (base_cols * 15 * 11 + 7) >> 3;
		else if (last_lv_mode == AIISP_LEVEL_MODE_24x4x3x3)
			dma_wr_width = (base_cols * 2 * 16 + 7) >> 3;
	} else {
		if (last_lv_mode == AIISP_LEVEL_MODE_18x8x3x3)
			dma_wr_width = (base_cols * 8 * 11 + 7) >> 3;
		else if (last_lv_mode == AIISP_LEVEL_MODE_18x4x3x3) {
			if (model_cfg->sw_out_mode == AIISP_OUT_MODE_BYPASS)
				dma_wr_width = (base_cols * 2 * 16 + 7) >> 3;
			else
				dma_wr_width = (base_cols * 2 * 8 + 7) >> 3;
		}
	}
	out_ch_stride = CEIL_DOWN(dma_wr_width, 16) * 4;

	// write to hardware
	rkaiisp_write(aidev, AIISP_MI_CORE_HEIGHT, base_rows, false);
	rkaiisp_write(aidev, AIISP_MI_CORE_WIDTH, base_cols * (sw_layer_s2d_flag + 1), false);

	for (i = 0; i < RKAIISP_MAX_CHANNEL; ++i) {
		if (model_cfg->sw_mi_chn_en[i])
			rkaiisp_write(aidev, AIISP_MI_RD_CH0_STRIDE + 0x100 * i,
				      sw_mi_chn_stride[i], false);
	}
	rkaiisp_write(aidev, AIISP_MI_CHN0_WR_STRIDE, out_ch_stride, false);

	if (model_cfg->sw_mi_chn_en[0]) {
		value = AIISP_MI_RD_CH_EN |
			(sw_mi_chn_height_odd[0] << 2) |
			(model_cfg->sw_mi_chn_mode[0] << 4) |
			(AIISP_MI_RD_CH0_GROUP_MODE << 8);
		rkaiisp_write(aidev, AIISP_MI_RD_CH0_CTRL, value, false);
	} else {
		rkaiisp_write(aidev, AIISP_MI_RD_CH0_CTRL, 0, false);
	}
	if (model_cfg->sw_mi_chn_en[1]) {
		value = AIISP_MI_RD_CH_EN |
			(model_cfg->sw_mi_chn1_sel << 1) |
			(sw_mi_chn_height_odd[1] << 2) |
			(model_cfg->sw_mi_chn_mode[1] << 4) |
			(model_cfg->sw_mi_chn_data_mode[1] << 6) |
			(AIISP_MI_RD_CH1_GROUP_MODE << 8);
		rkaiisp_write(aidev, AIISP_MI_RD_CH1_CTRL, value, false);
	} else {
		rkaiisp_write(aidev, AIISP_MI_RD_CH1_CTRL, 0, false);
	}
	if (model_cfg->sw_mi_chn_en[2]) {
		value = AIISP_MI_RD_CH_EN |
			(sw_mi_chn_height_odd[2] << 2) |
			(model_cfg->sw_mi_chn_mode[2] << 4) |
			(model_cfg->sw_mi_chn_data_mode[2] << 6) |
			(AIISP_MI_RD_CH2_GROUP_MODE << 8);
		rkaiisp_write(aidev, AIISP_MI_RD_CH2_CTRL, value, false);
	} else {
		rkaiisp_write(aidev, AIISP_MI_RD_CH2_CTRL, 0, false);
	}
	if (model_cfg->sw_mi_chn_en[3]) {
		value = AIISP_MI_RD_CH_EN |
			(model_cfg->sw_mi_chn3_sel << 1) |
			(sw_mi_chn_height_odd[3] << 2) |
			(model_cfg->sw_mi_chn_data_mode[3] << 6) |
			(AIISP_MI_RD_CH3_GROUP_MODE << 8);
		rkaiisp_write(aidev, AIISP_MI_RD_CH3_CTRL, value, false);
	} else {
		rkaiisp_write(aidev, AIISP_MI_RD_CH3_CTRL, 0, false);
	}
	if (model_cfg->sw_mi_chn_en[4]) {
		value = AIISP_MI_RD_CH_EN |
			(sw_mi_chn_height_odd[4] << 2) |
			(model_cfg->sw_mi_chn_mode[4] << 4) |
			(model_cfg->sw_mi_chn_data_mode[4] << 6) |
			(AIISP_MI_RD_CH4_GROUP_MODE << 8);
		rkaiisp_write(aidev, AIISP_MI_RD_CH4_CTRL, value, false);
	} else {
		rkaiisp_write(aidev, AIISP_MI_RD_CH4_CTRL, 0, false);
	}
	if (model_cfg->sw_mi_chn_en[5]) {
		value = AIISP_MI_RD_CH_EN |
			(sw_mi_chn_height_odd[5] << 2) |
			(model_cfg->sw_mi_chn_mode[5] << 4) |
			(model_cfg->sw_mi_chn_data_mode[5] << 6) |
			(AIISP_MI_RD_CH5_GROUP_MODE << 8);
		rkaiisp_write(aidev, AIISP_MI_RD_CH5_CTRL, value, false);
	} else {
		rkaiisp_write(aidev, AIISP_MI_RD_CH5_CTRL, 0, false);
	}
	if (model_cfg->sw_mi_chn_en[6]) {
		value = AIISP_MI_RD_CH_EN |
			(model_cfg->sw_mi_chn_data_mode[6] << 6) |
			(AIISP_MI_RD_CH6_GROUP_MODE << 8);
		rkaiisp_write(aidev, AIISP_MI_RD_CH6_CTRL, value, false);
	} else {
		rkaiisp_write(aidev, AIISP_MI_RD_CH6_CTRL, 0, false);
	}

	value = (AIISP_MI_WR_GROUP_MODE << 16) |
		AIISP_MI_WR_INIT_BASE_EN | AIISP_MI_WR_INIT_OFFSET_EN;
	rkaiisp_write(aidev, AIISP_MI_WR_CTRL, value, false);
	value = AIISP_MI_CHN0_WR_EN | AIISP_MI_CHN0_WR_AUTOUPD;
	rkaiisp_write(aidev, AIISP_MI_CHN0_WR_CRTL, value, false);

	return tmp_cols;
}

static void rkaiisp_cfg_other_iqparam(struct rkaiisp_device *aidev,
				      struct rkaiisp_other_cfg *other_cfg)
{
	u32 val;
	int i;

	val = other_cfg->sw_neg_noiselimit | other_cfg->sw_pos_noiselimit << 16;
	rkaiisp_write(aidev, AIISP_CORE_NOISE_LMT, val, false);
	for (i = 0; i < 32; i += 2) {
		val = other_cfg->sw_in_comp_y[i] | other_cfg->sw_in_comp_y[i+1] << 16;
		rkaiisp_write(aidev, AIISP_CORE_COMP0 + 2*i, val, false);
	}
	val = other_cfg->sw_in_comp_y[32] | other_cfg->sw_prev_blacklvl << 16;
	rkaiisp_write(aidev, AIISP_CORE_COMP16, val, false);

	for (i = 0; i < 32; i += 2) {
		val = other_cfg->sw_out_decomp_y[i] | other_cfg->sw_out_decomp_y[i+1] << 16;
		rkaiisp_write(aidev, AIISP_CORE_DECOMP0 + 2*i, val, false);
	}
	val = other_cfg->sw_out_decomp_y[32] | other_cfg->sw_post_blacklvl << 16;
	rkaiisp_write(aidev, AIISP_CORE_DECOMP16, val, false);
}

static u32 rkaiisp_config_rdchannel(struct rkaiisp_device *aidev,
				     struct rkaiisp_model_cfg *model_cfg, u32 run_idx)
{
	struct rkaiisp_ispbuf_info *ispbuf = &aidev->ispbuf;
	struct rkaiisp_rmsbuf_info *rmsbuf = &aidev->rmsbuf;
	struct rkaiisp_dummy_buffer *vpsl_buf;
	dma_addr_t dma_addr;
	u32 width, height;
	u32 sig_width = 0;
	int buffer_index;
	int i;

	vpsl_buf = &aidev->vpslbuf[aidev->curr_idxbuf.aibnr_st.vpsl_index];
	for (i = 0; i < 7; i++) {
		if (model_cfg->sw_mi_chn_en[i] == 0)
			continue;

		switch (model_cfg->mi_chn_src[i]) {
		case ISP_IIR:
			width = CEIL_BY(ispbuf->iir_width, 16);
			width = CEIL_BY(width * 9 / 4, 16);
			width = width >> 1;
			height = ispbuf->iir_height;
			dma_addr = aidev->iirbuf[aidev->curr_idxbuf.aibnr_st.iir_index].dma_addr;
			break;
		case VPSL_YRAW_CHN0:
			width  = ispbuf->raw_width[0];
			height = ispbuf->raw_height[0];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_yraw_offs[0];
			break;
		case VPSL_YRAW_CHN1:
			width  = ispbuf->raw_width[1];
			height = ispbuf->raw_height[1];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_yraw_offs[1];
			break;
		case VPSL_YRAW_CHN2:
			width  = ispbuf->raw_width[2];
			height = ispbuf->raw_height[2];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_yraw_offs[2];
			break;
		case VPSL_YRAW_CHN3:
			width  = ispbuf->raw_width[3];
			height = ispbuf->raw_height[3];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_yraw_offs[3];
			break;
		case VPSL_YRAW_CHN4:
			width  = ispbuf->raw_width[4];
			height = ispbuf->raw_height[4];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_yraw_offs[4];
			break;
		case VPSL_YRAW_CHN5:
			width  = ispbuf->raw_width[5];
			height = ispbuf->raw_height[5];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_yraw_offs[5];
			break;
		case VPSL_SIG_CHN0:
			width  = ispbuf->sig_width[0];
			height = ispbuf->sig_height[0];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_sig_offs[0];
			sig_width = width;
			break;
		case VPSL_SIG_CHN1:
			width  = ispbuf->sig_width[1];
			height = ispbuf->sig_height[1];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_sig_offs[1];
			sig_width = width;
			break;
		case VPSL_SIG_CHN2:
			width  = ispbuf->sig_width[2];
			height = ispbuf->sig_height[2];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_sig_offs[2];
			sig_width = width;
			break;
		case VPSL_SIG_CHN3:
			width  = ispbuf->sig_width[3];
			height = ispbuf->sig_height[3];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_sig_offs[3];
			sig_width = width;
			break;
		case VPSL_SIG_CHN4:
			width  = ispbuf->sig_width[4];
			height = ispbuf->sig_height[4];
			dma_addr = vpsl_buf->dma_addr + ispbuf->bnr_buf.u.v35.vpsl_sig_offs[4];
			sig_width = width;
			break;
		case ISP_AIPRE_NARMAP:
			width  = ispbuf->narmap_width;
			height = ispbuf->narmap_height;
			buffer_index = aidev->curr_idxbuf.aibnr_st.aipre_gain_index;
			dma_addr = aidev->aiprebuf[buffer_index].dma_addr;
			break;
		case AIISP_LAST_OUT:
			if (aidev->model_mode == COMBO_MODE) {
				width  = aidev->outbuf_size[aidev->model_runcnt-run_idx+1].width;
				height = aidev->outbuf_size[aidev->model_runcnt-run_idx+1].height;
			} else {
				width  = aidev->outbuf_size[aidev->model_runcnt-run_idx].width;
				height = aidev->outbuf_size[aidev->model_runcnt-run_idx].height;
			}
			dma_addr = aidev->temp_buf[aidev->outbuf_idx].dma_addr;
			break;
		case VICAP_BAYER_RAW:
			width  = rmsbuf->image_width;
			height = rmsbuf->image_height;
			dma_addr = aidev->rms_inbuf[aidev->curr_idxbuf.airms_st.inbuf_idx].dma_addr;
			break;
		case ALLZERO_SIGMA:
			width  = rmsbuf->sigma_width;
			height = rmsbuf->sigma_height;
			dma_addr = aidev->sigma_buf.dma_addr;
			sig_width = width;
			break;
		case ALLZERO_NARMAP:
			width  = rmsbuf->narmap_width;
			height = rmsbuf->narmap_height;
			dma_addr = aidev->narmap_buf.dma_addr;
			break;
		default:
			width  = 0;
			height = 0;
			dma_addr = 0;
			break;
		}

		if (width > 0) {
			aidev->chn_size[i].width  = width;
			aidev->chn_size[i].height = height;
			rkaiisp_write(aidev, AIISP_MI_RD_CH0_BASE + 0x100 * i, dma_addr, false);
			rkaiisp_write(aidev, AIISP_MI_RD_CH0_HEIGHT + 0x100 * i, height, false);

			v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
				"configure channel %d, width %d, height %d, dma_addr %pad\n",
				i, aidev->chn_size[i].width, aidev->chn_size[i].height, &dma_addr);
		}
	}

	return sig_width;
}

static void rkaiisp_run_cfg(struct rkaiisp_device *aidev, u32 run_idx)
{
	struct rkaiisp_ispbuf_info *ispbuf = &aidev->ispbuf;
	struct rkaiisp_params *cur_params;
	struct rkaiisp_model_cfg *model_cfg;
	int lastlv, lv_mode, out_chns, i;
	u32 outbuf_idx, val;
	u32 sw_lastlv_bypass = 0;
	u32 sw_m0_diff_merge = 0;
	u32 iir_stride;
	u32 sig_width;
	dma_addr_t dma_addr;
	int buffer_index;
	int sequence = 0;

	if (aidev->exealgo == AIBNR)
		sequence = aidev->curr_idxbuf.aibnr_st.sequence;
	else if (aidev->exealgo == AIRMS)
		sequence = aidev->curr_idxbuf.airms_st.sequence;

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"run frame id: %d, run_idx: %d\n",
		sequence, run_idx);

	cur_params = (struct rkaiisp_params *)aidev->cur_params->vaddr[0];
	model_cfg = &cur_params->model_cfg[run_idx];

	lastlv = model_cfg->sw_aiisp_level_num - 1;
	lv_mode = model_cfg->sw_aiisp_lv_mode[lastlv];
	out_chns = channels_lut[model_cfg->sw_aiisp_mode][lv_mode];
	if (aidev->model_mode == REMOSAIC_MODE) {
		sig_width = rkaiisp_config_rdchannel(aidev, model_cfg, run_idx);

		dma_addr = aidev->rms_outbuf[aidev->curr_idxbuf.airms_st.outbuf_idx].dma_addr;
		rkaiisp_write(aidev, AIISP_MI_CHN0_WR_BASE, dma_addr, false);

		rkaiisp_gen_slice_param(aidev, model_cfg, sig_width);
		rkaiisp_determine_size(aidev, model_cfg);
	} else if (aidev->model_mode == SINGLEX2_MODE) {
		if (run_idx == 0) {
			sig_width = rkaiisp_config_rdchannel(aidev, model_cfg, run_idx);

			outbuf_idx = 0;
			aidev->outbuf_idx = outbuf_idx;
			rkaiisp_write(aidev, AIISP_MI_CHN0_WR_BASE,
				      aidev->temp_buf[outbuf_idx].dma_addr, false);

			rkaiisp_gen_slice_param(aidev, model_cfg, sig_width);
			rkaiisp_determine_size(aidev, model_cfg);
		} else if (run_idx < aidev->model_runcnt-1) {
			outbuf_idx = aidev->outbuf_idx;
			sig_width = rkaiisp_config_rdchannel(aidev, model_cfg, run_idx);
			rkaiisp_gen_slice_param(aidev, model_cfg, sig_width);
			rkaiisp_determine_size(aidev, model_cfg);

			outbuf_idx = (outbuf_idx + 1) % 2;
			aidev->outbuf_idx = outbuf_idx;
			rkaiisp_write(aidev, AIISP_MI_CHN0_WR_BASE,
				      aidev->temp_buf[outbuf_idx].dma_addr, false);
		} else {
			sig_width = rkaiisp_config_rdchannel(aidev, model_cfg, run_idx);

			buffer_index = aidev->curr_idxbuf.aibnr_st.aiisp_index;
			dma_addr = aidev->aiispbuf[buffer_index].dma_addr;
			rkaiisp_write(aidev, AIISP_MI_CHN0_WR_BASE, dma_addr, false);

			rkaiisp_gen_slice_param(aidev, model_cfg, sig_width);
			rkaiisp_determine_size(aidev, model_cfg);

			iir_stride = CEIL_BY(ispbuf->iir_width, 16);
			iir_stride = CEIL_BY(iir_stride * 9 / 4, 16);
			iir_stride = iir_stride >> 1;
			rkaiisp_write(aidev, AIISP_MI_CHN0_WR_STRIDE, iir_stride / 2, false);
		}
	} else {
		if (run_idx == 0) {
			rkaiisp_config_rdchannel(aidev, model_cfg, run_idx);

			outbuf_idx = 0;
			aidev->outbuf_idx = outbuf_idx;
			rkaiisp_write(aidev, AIISP_MI_CHN0_WR_BASE,
				      aidev->temp_buf[outbuf_idx].dma_addr, false);

			rkaiisp_gen_slice_param(aidev, model_cfg, ispbuf->sig_width[3]);
			rkaiisp_determine_size(aidev, model_cfg);
		} else if (run_idx < aidev->model_runcnt-1) {
			outbuf_idx = aidev->outbuf_idx;
			rkaiisp_config_rdchannel(aidev, model_cfg, run_idx);
			if (run_idx == 1)
				rkaiisp_gen_slice_param(aidev, model_cfg, ispbuf->sig_width[2]);
			else
				rkaiisp_gen_slice_param(aidev, model_cfg, ispbuf->sig_width[1]);
			rkaiisp_determine_size(aidev, model_cfg);

			outbuf_idx = (outbuf_idx + 1) % 2;
			aidev->outbuf_idx = outbuf_idx;
			rkaiisp_write(aidev, AIISP_MI_CHN0_WR_BASE,
				      aidev->temp_buf[outbuf_idx].dma_addr, false);
		} else {
			rkaiisp_config_rdchannel(aidev, model_cfg, run_idx);

			buffer_index = aidev->curr_idxbuf.aibnr_st.aiisp_index;
			dma_addr = aidev->aiispbuf[buffer_index].dma_addr;
			rkaiisp_write(aidev, AIISP_MI_CHN0_WR_BASE, dma_addr, false);

			rkaiisp_gen_slice_param(aidev, model_cfg, ispbuf->sig_width[0]);
			rkaiisp_determine_size(aidev, model_cfg);

			iir_stride = CEIL_BY(ispbuf->iir_width, 16);
			iir_stride = CEIL_BY(iir_stride * 9 / 4, 16);
			iir_stride = iir_stride >> 1;
			rkaiisp_write(aidev, AIISP_MI_CHN0_WR_STRIDE, iir_stride / 2, false);
		}
	}

	cur_params = (struct rkaiisp_params *)aidev->cur_params->vaddr[0];
	val = aidev->cur_params->buff_addr[0] + cur_params->kwt_cfg.kwt_offet[run_idx];
	rkaiisp_write(aidev, AIISP_MI_RD_KWT_BASE, val, false);
	rkaiisp_write(aidev, AIISP_MI_RD_KWT_WIDTH,
		      cur_params->kwt_cfg.kwt_size[run_idx], false);
	rkaiisp_write(aidev, AIISP_MI_RD_KWT_HEIGHT, 1, false);
	rkaiisp_write(aidev, AIISP_MI_RD_KWT_STRIDE,
		      CEIL_DOWN(cur_params->kwt_cfg.kwt_size[run_idx], 4), false);
	rkaiisp_write(aidev, AIISP_MI_RD_KWT_CTRL, AIISP_MI_RD_KWT_EN, false);

	val = (model_cfg->sw_aiisp_mode << 2) |
	      (model_cfg->sw_aiisp_op_mode << 6) |
	      (model_cfg->sw_aiisp_drop_en << 7) |
	      (model_cfg->sw_aiisp_level_num << 8) |
	      (model_cfg->sw_aiisp_l1_level_num << 16);
	rkaiisp_write(aidev, AIISP_CORE_CTRL, val, false);

	for (i = 0; i < 4; ++i) {
		val = (model_cfg->sw_aiisp_lv_active[i * 4 + 0] << 2) |
		      (model_cfg->sw_aiisp_lv_mode[i * 4 + 0] << 4) |
		      (model_cfg->sw_aiisp_lv_active[i * 4 + 1] << 10) |
		      (model_cfg->sw_aiisp_lv_mode[i * 4 + 1] << 12) |
		      (model_cfg->sw_aiisp_lv_active[i * 4 + 2] << 18) |
		      (model_cfg->sw_aiisp_lv_mode[i * 4 + 2] << 20) |
		      (model_cfg->sw_aiisp_lv_active[i * 4 + 3] << 26) |
		      (model_cfg->sw_aiisp_lv_mode[i * 4 + 3] << 28);

		rkaiisp_write(aidev, AIISP_CORE_LEVEL_CTRL0 + i * 4, val, false);
	}

	if ((out_chns == 4 && model_cfg->sw_out_d2s_en == 0))
		sw_lastlv_bypass = 1;
	if (model_cfg->sw_aiisp_mode == 0 && model_cfg->sw_out_mode == AIISP_OUT_MODE_DIFF_MERGE)
		sw_m0_diff_merge = 1;

	val = sw_lastlv_bypass |
	      (sw_m0_diff_merge << 1) |
	      (model_cfg->sw_lastlvlm1_clip8bit << 2);
	rkaiisp_write(aidev, AIISP_CORE_OUT_CTRL, val, false);

	/* rkaiisp_dump_list_reg(aidev); */
}

static int rkaiisp_update_buf(struct rkaiisp_device *aidev)
{
	struct kfifo *fifo = &aidev->idxbuf_kfifo;
	struct rkaiisp_hw_dev *hw_dev = aidev->hw_dev;
	union rkaiisp_queue_buf idxbuf = {0};
	unsigned long flags = 0;
	int sequence = 0;
	int ret = 0;

	spin_lock_irqsave(&hw_dev->hw_lock, flags);
	if (!kfifo_is_empty(fifo))
		ret = kfifo_out(fifo, &idxbuf, sizeof(struct rkisp_aiisp_st));
	if (!ret) {
		ret = -EINVAL;
	} else {
		ret = 0;
		aidev->curr_idxbuf = idxbuf;
		if (aidev->exealgo == AIBNR)
			sequence = aidev->curr_idxbuf.aibnr_st.sequence;
		else if (aidev->exealgo == AIRMS)
			sequence = aidev->curr_idxbuf.airms_st.sequence;

		v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
			"idxbuf fifo out: %d\n", sequence);
	}
	spin_unlock_irqrestore(&hw_dev->hw_lock, flags);

	return ret;
}

static void rkaiisp_run_start(struct rkaiisp_device *aidev)
{
	struct rkaiisp_hw_dev *hw_dev = aidev->hw_dev;

	rkaiisp_write(aidev, AIISP_MI_IMSC, AIISP_MI_ISR_ALL, false);
	rkaiisp_write(aidev, AIISP_MI_WR_INIT, AIISP_MI_CHN0SELF_FORCE_UPD, false);

	if ((aidev->run_idx == 0) && (rkaiisp_showreg != 0))
		aidev->showreg = true;

	if (aidev->showreg)
		rkaiisp_dump_list_reg(aidev);

	if ((aidev->run_idx == aidev->model_runcnt - 1) && aidev->showreg) {
		aidev->showreg = false;
		rkaiisp_showreg = 0;
	}

	rkaiisp_write(aidev, AIISP_MI_RD_START, AIISP_MI_RD_START_EN, false);

	if (!hw_dev->is_single)
		rkaiisp_update_list_reg(aidev);
}

static void rkaiisp_get_new_iqparam(struct rkaiisp_device *aidev)
{
	struct rkaiisp_params *iq_params, *old_params;
	struct rkaiisp_buffer *cur_buf = NULL;
	struct rkaiisp_buffer *done_buf = NULL;
	unsigned int cur_frame_id = aidev->frame_id;
	unsigned long flags = 0;

	spin_lock_irqsave(&aidev->config_lock, flags);
	if (!list_empty(&aidev->params))
		cur_buf = list_first_entry(&aidev->params,
					   struct rkaiisp_buffer, queue);
	if (!cur_buf) {
		spin_unlock_irqrestore(&aidev->config_lock, flags);
		return;
	}

	list_del(&cur_buf->queue);
	iq_params = (struct rkaiisp_params *)cur_buf->vaddr[0];
	if ((!(iq_params->module_update & RKAIISP_MODEL_UPDATE) &&
	    (iq_params->module_update & RKAIISP_OTHER_UPDATE)) && aidev->cur_params) {
		old_params = (struct rkaiisp_params *)aidev->cur_params->vaddr[0];
		old_params->frame_id = iq_params->frame_id;
		old_params->module_update = iq_params->module_update;
		old_params->other_cfg = iq_params->other_cfg;
		done_buf = cur_buf;
	} else {
		done_buf = aidev->cur_params;
		aidev->cur_params = cur_buf;
	}

	if (done_buf) {
		done_buf->vb.sequence = cur_frame_id;
		vb2_buffer_done(&done_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}
	spin_unlock_irqrestore(&aidev->config_lock, flags);

	// configure other params
	if (aidev->cur_params) {
		iq_params = (struct rkaiisp_params *)aidev->cur_params->vaddr[0];

		v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
			"update iq param: %d, module: 0x%x\n",
			iq_params->frame_id, iq_params->module_update);

		aidev->model_mode = iq_params->model_mode;
		aidev->model_runcnt = iq_params->model_runcnt;
		rkaiisp_cfg_other_iqparam(aidev, &iq_params->other_cfg);
	}
}


void rkaiisp_trigger(struct rkaiisp_device *aidev)
{
	struct rkaiisp_ispbuf_info *ispbuf = &aidev->ispbuf;
	int sequence = 0;

	if (aidev->exealgo == AIBNR)
		sequence = aidev->curr_idxbuf.aibnr_st.sequence;
	else if (aidev->exealgo == AIRMS)
		sequence = aidev->curr_idxbuf.airms_st.sequence;

	if (!rkaiisp_update_buf(aidev)) {
		aidev->run_idx = 0;
		aidev->frame_id = sequence;
		aidev->pre_frm_st = aidev->frm_st;
		aidev->frm_st = ktime_get_ns();
		rkaiisp_get_new_iqparam(aidev);
		rkaiisp_calc_outbuf_size(aidev, ispbuf->iir_height, ispbuf->iir_width);
		rkaiisp_run_cfg(aidev, aidev->run_idx);
		aidev->hwstate = HW_RUNNING;
		rkaiisp_run_start(aidev);
	}
}

static void rkaiisp_event_queue(struct rkaiisp_device *aidev, union rkaiisp_queue_buf *idxbuf)
{
	union rkaiisp_queue_buf *rundone;
	struct v4l2_event event = {0};
	int sequence = 0;

	if (aidev->exealgo == AIBNR)
		sequence = idxbuf->aibnr_st.sequence;
	else if (aidev->exealgo == AIRMS)
		sequence = idxbuf->airms_st.sequence;

	if (aidev->subdev.is_subs_evt && aidev->exemode != BOTHEVENT_IN_KERNEL) {
		event.type = RKAIISP_V4L2_EVENT_AIISP_DONE;
		rundone = (union rkaiisp_queue_buf *)&event.u.data[0];
		*rundone = *idxbuf;
		v4l2_event_queue(aidev->subdev.sd.devnode, &event);
		v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
			"aiisp done: %d\n", sequence);
	} else {
		// call isp api to inform
	}
}

int rkaiisp_get_idxbuf_len(struct rkaiisp_device *aidev)
{
	struct kfifo *fifo = &aidev->idxbuf_kfifo;
	int len = 0;

	len = kfifo_len(fifo) / sizeof(union rkaiisp_queue_buf);
	return len;
}

enum rkaiisp_irqhdl_ret rkaiisp_irq_hdl(struct rkaiisp_device *aidev, u32 mi_mis)
{
	union rkaiisp_queue_buf *idxbuf = NULL;
	u64 frm_hdntim = 0;

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"irq val: 0x%x, run_idx %d, model_runcnt %d\n",
		mi_mis, aidev->run_idx, aidev->model_runcnt);

	if (mi_mis & AIISP_MI_ISR_BUSERR) {
		v4l2_err(&aidev->v4l2_dev, "buserr 0x%x\n", mi_mis);
		rkaiisp_write(aidev, AIISP_MI_ICR, AIISP_MI_ISR_BUSERR, true);
		aidev->isr_buserr_cnt++;
	}

	if (!(mi_mis & AIISP_MI_ISR_WREND))
		return NOT_WREND;
	rkaiisp_write(aidev, AIISP_MI_ICR, AIISP_MI_ISR_WREND, true);
	aidev->isr_wrend_cnt++;

	if (aidev->run_idx < aidev->model_runcnt - 1) {
		aidev->run_idx++;
		rkaiisp_run_cfg(aidev, aidev->run_idx);
		rkaiisp_run_start(aidev);
		return CONTINUE_RUN;
	}

	aidev->frm_ed = ktime_get_ns();
	if (aidev->frm_ed > aidev->frm_st) {
		frm_hdntim = aidev->frm_ed - aidev->frm_st;
		aidev->frm_interval = frm_hdntim;
		if (frm_hdntim * rkaiisp_stdfps > NSEC_PER_SEC)
			aidev->frm_oversdtim_cnt++;
	}

	idxbuf = &aidev->curr_idxbuf;
	if (idxbuf)
		rkaiisp_event_queue(aidev, idxbuf);

	aidev->hwstate = HW_STOP;
	if (!aidev->streamon)
		wake_up(&aidev->sync_onoff);

	return RUN_COMPLETE;
}

static inline struct rkaiisp_device *sd_to_aiispdev(struct v4l2_subdev *sd)
{
	return container_of(sd->v4l2_dev, struct rkaiisp_device, v4l2_dev);
}

static int rkaiisp_sd_subs_evt(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub)
{
	struct rkaiisp_device *aidev = sd_to_aiispdev(sd);

	if (sub->type != RKAIISP_V4L2_EVENT_AIISP_DONE)
		return -EINVAL;

	aidev->subdev.is_subs_evt = true;
	return v4l2_event_subscribe(fh, sub, RKAIISP_V4L2_EVENT_ELEMS, NULL);
}

static int rkaiisp_sd_unsubs_evt(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				 struct v4l2_event_subscription *sub)
{
	struct rkaiisp_device *aidev = sd_to_aiispdev(sd);

	if (sub->type != RKAIISP_V4L2_EVENT_AIISP_DONE)
		return -EINVAL;

	aidev->subdev.is_subs_evt = false;
	return v4l2_event_subdev_unsubscribe(sd, fh, sub);
}

static const struct v4l2_subdev_core_ops rkaiisp_core_ops = {
	.subscribe_event = rkaiisp_sd_subs_evt,
	.unsubscribe_event = rkaiisp_sd_unsubs_evt,
};

static const struct v4l2_subdev_ops rkaiisp_sd_ops = {
	.core = &rkaiisp_core_ops,
};

static int rkaiisp_register_subdev(struct rkaiisp_device *aidev,
				   struct v4l2_device *v4l2_dev)
{
	struct rkaiiisp_subdev *subdev = &aidev->subdev;
	struct v4l2_subdev *sd = &subdev->sd;
	int ret;

	v4l2_subdev_init(sd, &rkaiisp_sd_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sd->entity.function = MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN;
	snprintf(sd->name, sizeof(sd->name), RKAIISP_SUBDEV_NAME);

	sd->owner = THIS_MODULE;
	v4l2_set_subdevdata(sd, aidev);

	sd->grp_id = 0;
	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		v4l2_err(sd, "Failed to register subdev\n");
		return ret;
	}

	ret = v4l2_device_register_subdev_nodes(v4l2_dev);
	if (ret < 0) {
		v4l2_err(sd, "Failed to register subdev nodes\n");
		return ret;
	}

	return 0;
}

static void rkaiisp_unregister_subdev(struct rkaiisp_device *aidev)
{
	struct v4l2_subdev *sd = &aidev->subdev.sd;

	v4l2_device_unregister_subdev(sd);
}

static int rkaiisp_enum_fmt_meta_out(struct file *file, void *priv,
					 struct v4l2_fmtdesc *f)
{
	struct video_device *video = video_devdata(file);
	struct rkaiisp_device *aidev = video_get_drvdata(video);

	if (f->index > 0 || f->type != video->queue->type)
		return -EINVAL;

	f->pixelformat = aidev->vdev_fmt.fmt.meta.dataformat;

	return 0;
}

static int rkaiisp_g_fmt_meta_out(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct video_device *video = video_devdata(file);
	struct rkaiisp_device *aidev = video_get_drvdata(video);
	struct v4l2_meta_format *meta = &f->fmt.meta;

	if (f->type != video->queue->type)
		return -EINVAL;

	memset(meta, 0, sizeof(*meta));
	meta->dataformat = aidev->vdev_fmt.fmt.meta.dataformat;
	meta->buffersize = aidev->vdev_fmt.fmt.meta.buffersize;

	return 0;
}

static int rkaiisp_querycap(struct file *file,
				void *priv, struct v4l2_capability *cap)
{
	snprintf(cap->driver, sizeof(cap->driver), "%s", DRIVER_NAME);
	snprintf(cap->card, sizeof(cap->card), "%s", DRIVER_NAME);
	strscpy(cap->bus_info, "platform: " DRIVER_NAME, sizeof(cap->bus_info));

	return 0;
}

static long rkaiisp_ioctl_default(struct file *file, void *fh,
				  bool valid_prio, unsigned int cmd, void *arg)
{
	struct rkaiisp_device *aidev = video_drvdata(file);
	struct rkaiisp_param_info *param_info = (struct rkaiisp_param_info *)arg;
	long ret = -EINVAL;

	switch (cmd) {
	case RKAIISP_CMD_SET_PARAM_INFO:
		if ((param_info->para_size > 0) &&
		    (param_info->max_runcnt > 0) &&
		    (param_info->max_runcnt <= RKAIISP_MAX_RUNCNT)) {
			aidev->exealgo    = param_info->exealgo;
			aidev->exemode    = param_info->exemode;
			aidev->para_size  = param_info->para_size;
			aidev->max_runcnt = param_info->max_runcnt;
			ret = 0;
		} else {
			v4l2_err(&aidev->v4l2_dev,
				"wrong params in set param info, para_size %d, max_runcnt %d\n",
				aidev->para_size, aidev->max_runcnt);
		}
		break;
	case RKAIISP_CMD_INIT_BUFPOOL:
		ret = rkaiisp_init_pool(aidev, arg);
		break;
	case RKAIISP_CMD_FREE_BUFPOOL:
		ret = rkaiisp_free_pool(aidev);
		break;
	case RKAIISP_CMD_QUEUE_BUF:
		ret = rkaiisp_queue_ispbuf(aidev, arg);
		break;
	case RKAIISP_CMD_INIT_AIRMS_BUFPOOL:
		ret = rkaiisp_init_airms_pool(aidev, arg);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/* ISP params video device IOCTLs */
static const struct v4l2_ioctl_ops rkaiisp_ioctl = {
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_enum_fmt_meta_out = rkaiisp_enum_fmt_meta_out,
	.vidioc_g_fmt_meta_out = rkaiisp_g_fmt_meta_out,
	.vidioc_s_fmt_meta_out = rkaiisp_g_fmt_meta_out,
	.vidioc_try_fmt_meta_out = rkaiisp_g_fmt_meta_out,
	.vidioc_querycap = rkaiisp_querycap,
	.vidioc_default = rkaiisp_ioctl_default,
};

static int rkaiisp_vb2_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
				   unsigned int sizes[],
				   struct device *alloc_ctxs[])
{
	struct rkaiisp_device *aidev = vq->drv_priv;

	*num_planes  = 1;
	*num_buffers = clamp_t(u32, *num_buffers,
				RKAIISP_REQ_BUFS_MIN,
				RKAIISP_REQ_BUFS_MAX);

	sizes[0] = sizeof(struct rkaiisp_params) + aidev->para_size * aidev->max_runcnt;
	aidev->vdev_fmt.fmt.meta.buffersize = sizes[0];

	INIT_LIST_HEAD(&aidev->params);

	return 0;
}

static void rkaiisp_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkaiisp_buffer *buf = to_rkaiisp_buffer(vbuf);
	struct vb2_queue *vq = vb->vb2_queue;
	struct rkaiisp_device *aidev = vq->drv_priv;
	struct sg_table *sgt;
	unsigned long flags;

	sgt = vb2_dma_sg_plane_desc(vb, 0);
	buf->vaddr[0] = vb2_plane_vaddr(vb, 0);
	buf->buff_addr[0] = sg_dma_address(sgt->sgl);

	if (buf->vaddr[0])
		if (vb->vb2_queue->mem_ops->prepare)
			vb->vb2_queue->mem_ops->prepare(vb->planes[0].mem_priv);

	spin_lock_irqsave(&aidev->config_lock, flags);
	list_add_tail(&buf->queue, &aidev->params);
	spin_unlock_irqrestore(&aidev->config_lock, flags);

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"queue param buffer\n");
}

static void rkaiisp_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct rkaiisp_device *aidev = vq->drv_priv;
	struct rkaiisp_hw_dev *hw_dev = aidev->hw_dev;
	struct rkaiisp_buffer *parabuf;
	unsigned long flags;
	int i, ret;

	/* stop params input firstly */
	spin_lock_irqsave(&hw_dev->hw_lock, flags);
	if (aidev->streamon) {
		aidev->streamon = false;
		spin_unlock_irqrestore(&hw_dev->hw_lock, flags);
		if (aidev->hwstate == HW_RUNNING) {
			ret = wait_event_timeout(aidev->sync_onoff,
					aidev->hwstate == HW_STOP, msecs_to_jiffies(200));
			if (!ret)
				v4l2_warn(&aidev->v4l2_dev, "%s: wait dev %d stop timeout\n",
					  __func__, aidev->dev_id);
		}
	} else {
		spin_unlock_irqrestore(&hw_dev->hw_lock, flags);
	}

	spin_lock_irqsave(&aidev->config_lock, flags);
	for (i = 0; i < RKAIISP_REQ_BUFS_MAX; i++) {
		if (!list_empty(&aidev->params)) {
			parabuf = list_first_entry(&aidev->params,
						   struct rkaiisp_buffer, queue);
			list_del(&parabuf->queue);
			vb2_buffer_done(&parabuf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		} else {
			break;
		}
	}
	spin_unlock_irqrestore(&aidev->config_lock, flags);

	if (aidev->cur_params) {
		parabuf = aidev->cur_params;
		vb2_buffer_done(&parabuf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		aidev->cur_params = NULL;
	}

	pm_runtime_put_sync(aidev->dev);
	atomic_dec(&hw_dev->refcnt);

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"stop streaming %d, hwstate %d\n", aidev->streamon, aidev->hwstate);
}

static int
rkaiisp_vb2_start_streaming(struct vb2_queue *queue, unsigned int count)
{
	struct rkaiisp_device *aidev = queue->drv_priv;
	struct rkaiisp_hw_dev *hw_dev = aidev->hw_dev;
	unsigned long flags;

	spin_lock_irqsave(&hw_dev->hw_lock, flags);
	aidev->streamon = true;
	kfifo_reset(&aidev->idxbuf_kfifo);
	spin_unlock_irqrestore(&hw_dev->hw_lock, flags);

	pm_runtime_get_sync(aidev->dev);
	atomic_inc(&hw_dev->refcnt);

	aidev->frm_oversdtim_cnt = 0;
	aidev->isr_buserr_cnt = 0;
	aidev->isr_wrend_cnt = 0;

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"start streaming %d\n", aidev->streamon);

	return 0;
}

static const struct vb2_ops rkaiisp_vb2_ops = {
	.queue_setup = rkaiisp_vb2_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_queue = rkaiisp_vb2_buf_queue,
	.start_streaming = rkaiisp_vb2_start_streaming,
	.stop_streaming = rkaiisp_vb2_stop_streaming,

};

static int rkaiisp_fh_open(struct file *file)
{
	struct rkaiisp_device *aidev = video_drvdata(file);
	int ret;

	atomic_inc(&aidev->opencnt);
	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"%s: opencnt %d, init_buf %d\n", __func__,
		atomic_read(&aidev->opencnt),
		aidev->init_buf);

	ret = v4l2_fh_open(file);

	return ret;
}

static int rkaiisp_fop_release(struct file *file)
{
	struct rkaiisp_device *aidev = video_drvdata(file);
	int ret;

	v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
		"%s: opencnt %d, init_buf %d\n", __func__,
		atomic_read(&aidev->opencnt),
		aidev->init_buf);

	ret = vb2_fop_release(file);

	if (!atomic_dec_return(&aidev->opencnt)) {
		mutex_lock(&aidev->apilock);
		rkaiisp_free_pool(aidev);
		mutex_unlock(&aidev->apilock);
	}

	return ret;
}

static struct v4l2_file_operations rkaiisp_fops = {
	.mmap = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.open = rkaiisp_fh_open,
	.release = rkaiisp_fop_release
};

static int
rkaiisp_init_vb2_queue(struct vb2_queue *q,
			struct rkaiisp_device *aidev)
{
	q->type = V4L2_BUF_TYPE_META_OUTPUT;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = aidev;
	q->ops = &rkaiisp_vb2_ops;
	q->mem_ops = aidev->hw_dev->mem_ops;
	q->buf_struct_size = sizeof(struct rkaiisp_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &aidev->apilock;
	q->dev = aidev->hw_dev->dev;
	if (aidev->hw_dev->is_dma_contig)
		q->dma_attrs = DMA_ATTR_FORCE_CONTIGUOUS;

	return vb2_queue_init(q);
}

static void rkaiisp_init_vdev(struct rkaiisp_device *aidev)
{
	aidev->para_size = RKAIISP_DEFAULT_PARASIZE;
	aidev->max_runcnt = RKAIISP_DEFAULT_MAXRUNCNT;
	aidev->vdev_fmt.fmt.meta.dataformat = V4L2_META_FMT_RK_ISP1_PARAMS;
	aidev->vdev_fmt.fmt.meta.buffersize =
		sizeof(struct rkaiisp_params) + aidev->para_size * aidev->max_runcnt;
}

int rkaiisp_register_vdev(struct rkaiisp_device *aidev, struct v4l2_device *v4l2_dev)
{
	int ret;
	struct rkaiisp_vdev_node *node = &aidev->vnode;
	struct video_device *vdev = &node->vdev;

	spin_lock_init(&aidev->config_lock);
	atomic_set(&aidev->opencnt, 0);
	aidev->mem_ops = aidev->hw_dev->mem_ops;

	ret = kfifo_alloc(&aidev->idxbuf_kfifo,
		16 * sizeof(union rkaiisp_queue_buf), GFP_KERNEL);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "Failed to alloc kfifo %d", ret);
		return ret;
	}

	strscpy(vdev->name, "rkaiisp", sizeof(vdev->name));

	vdev->ioctl_ops = &rkaiisp_ioctl;
	vdev->fops = &rkaiisp_fops;
	vdev->release = video_device_release_empty;
	/*
	 * Provide a mutex to v4l2 core. It will be used
	 * to protect all fops and v4l2 ioctls.
	 */
	vdev->lock = &aidev->apilock;
	vdev->v4l2_dev = v4l2_dev;
	vdev->queue = &node->buf_queue;
	vdev->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_META_OUTPUT;
	vdev->vfl_dir = VFL_DIR_TX;
	rkaiisp_init_vb2_queue(vdev->queue, aidev);
	rkaiisp_init_vdev(aidev);
	video_set_drvdata(vdev, aidev);
	init_waitqueue_head(&aidev->sync_onoff);

	node->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&vdev->entity, 0, &node->pad);
	if (ret < 0)
		goto err_release_queue;
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		dev_err(&vdev->dev,
			"could not register Video for Linux device\n");
		goto err_cleanup_media_entity;
	}
	ret = rkaiisp_register_subdev(aidev, v4l2_dev);
	if (ret < 0) {
		dev_err(&vdev->dev,
			"could not register Video for Linux device\n");
		goto err_unregister_device;
	}
	return 0;
err_unregister_device:
	video_unregister_device(vdev);
err_cleanup_media_entity:
	media_entity_cleanup(&vdev->entity);
err_release_queue:
	vb2_queue_release(vdev->queue);
	return ret;
}

void rkaiisp_unregister_vdev(struct rkaiisp_device *aidev)
{
	struct rkaiisp_vdev_node *node = &aidev->vnode;
	struct video_device *vdev = &node->vdev;

	kfifo_free(&aidev->idxbuf_kfifo);
	rkaiisp_unregister_subdev(aidev);
	video_unregister_device(vdev);
	media_entity_cleanup(&vdev->entity);
	vb2_queue_release(vdev->queue);
}

