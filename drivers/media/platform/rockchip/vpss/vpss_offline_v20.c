// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include <linux/clk.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <linux/pm_runtime.h>
#include <linux/rk-video-format.h>

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "regs.h"

#include "procfs.h"

#include "vpss_offline_v20.h"
#include "vpss_offline_rockit.h"
#include "vpss_offline_dvbm.h"

struct rkvpss_output_ch {
	u32 ctrl;
	u32 size;
	u32 c_offs;
};

struct rkvpss_offline_buf {
	struct list_head list;
	struct vb2_buffer vb;
	struct vb2_queue vb2_queue;
	int file_id;
	struct dma_buf *dbuf;
	void *mem;
	int dev_id;
	int fd;
	bool alloc;
};

static void init_vb2(struct rkvpss_offline_dev *ofl,
		     struct rkvpss_offline_buf *buf)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	unsigned long attrs = DMA_ATTR_NO_KERNEL_MAPPING;

	if (!buf)
		return;
	memset(&buf->vb, 0, sizeof(buf->vb));
	memset(&buf->vb2_queue, 0, sizeof(buf->vb2_queue));
	buf->vb2_queue.gfp_flags = GFP_KERNEL | GFP_DMA32;
	buf->vb2_queue.dma_dir = DMA_BIDIRECTIONAL;
	if (hw->is_dma_contig)
		attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
	buf->vb2_queue.dma_attrs = attrs;
	buf->vb.vb2_queue = &buf->vb2_queue;
}

static void buf_del(struct rkvpss_offline_dev *ofl,
	     struct dma_buf *dmabuf,
	     int file_id, int id, int fd,
	     bool is_all, bool running)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkvpss_offline_buf *buf, *next;

	mutex_lock(&hw->dev_lock);
	list_for_each_entry_safe(buf, next, &ofl->list, list) {
		if (buf->file_id == file_id && (is_all || buf->fd == fd || buf->dbuf == dmabuf)) {
			if (!is_all && running && buf->alloc)
				break;
			v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
				 "%s file_id:%d dev_id:%d fd:%d dbuf:%p\n",
				 __func__, file_id, buf->dev_id, buf->fd, buf->dbuf);
			if (!buf->alloc) {
				ops->unmap_dmabuf(buf->mem);
				ops->detach_dmabuf(buf->mem);
				dma_buf_put(buf->dbuf);
			} else {
				dma_buf_put(buf->dbuf);
				ops->put(buf->mem);
			}
			buf->file_id = 0;
			buf->mem = NULL;
			buf->dbuf = NULL;
			buf->fd = -1;
			list_del(&buf->list);
			kfree(buf);
			if (!is_all)
				break;
		}
	}
	mutex_unlock(&hw->dev_lock);
}

static struct rkvpss_offline_buf *buf_add(struct rkvpss_offline_dev *ofl,
					  struct dma_buf *dmabuf,
					  int file_id, int id,
					  int fd, int size)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkvpss_offline_buf *buf = NULL, *next = NULL;
	struct dma_buf *dbuf;
	void *mem = NULL;
	bool is_add = true;

	if (dmabuf) {
		get_dma_buf(dmabuf);
		dbuf = dmabuf;
	} else {
		dbuf = dma_buf_get(fd);
	}

	if (IS_ERR_OR_NULL(dbuf)) {
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d invalid dmabuf fd:%d", id, fd);
		return buf;
	}
	if (size && dbuf->size < size) {
		v4l2_err(&ofl->v4l2_dev,
			 "dev_id:%d input fd:%d size error:%zu < %u\n",
			 id, fd, dbuf->size, size);
		dma_buf_put(dbuf);
		return buf;
	}

	mutex_lock(&hw->dev_lock);
	list_for_each_entry_safe(buf, next, &ofl->list, list) {
		if (buf->file_id == file_id && buf->fd == fd && buf->dbuf == dbuf) {
			is_add = false;
			break;
		}
	}

	if (is_add) {
		buf = kzalloc(sizeof(struct rkvpss_offline_buf), GFP_KERNEL);
		if (!buf)
			goto end;
		init_vb2(ofl, buf);

		mem = ops->attach_dmabuf(&buf->vb, hw->dev, dbuf, dbuf->size);
		if (IS_ERR(mem)) {
			v4l2_err(&ofl->v4l2_dev, "failed to attach dmabuf, fd:%d\n", fd);
			dma_buf_put(dbuf);
			kfree(buf);
			buf = NULL;
			goto end;
		}
		if (ops->map_dmabuf(mem)) {
			v4l2_err(&ofl->v4l2_dev, "failed to map, fd:%d\n", fd);
			ops->detach_dmabuf(mem);
			dma_buf_put(dbuf);
			mem = NULL;
			kfree(buf);
			buf = NULL;
			goto end;
		}
		buf->dev_id = id;
		buf->fd = fd;
		buf->file_id = file_id;
		buf->dbuf = dbuf;
		buf->mem = mem;
		buf->alloc = false;
		list_add_tail(&buf->list, &ofl->list);
		v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
			 "%s file_id:%d dev_id:%d fd:%d dbuf:%p size:%d\n", __func__,
			 file_id, id, fd, dbuf, size);
	} else {
		dma_buf_put(dbuf);
	}
end:
	mutex_unlock(&hw->dev_lock);
	return buf;
}

static int internal_buf_alloc(struct rkvpss_offline_dev *ofl,
			      int file_id,
			      struct rkvpss_buf_info *info)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *ops = hw->mem_ops;
	struct rkvpss_offline_buf *buf;
	struct dma_buf *dbuf;
	int fd, i, size;
	void *mem;

	for (i = 0; i < info->buf_cnt; i++) {
		info->buf_fd[i] = -1;
		size = PAGE_ALIGN(info->buf_size[i]);
		if (!size)
			continue;
		buf = kzalloc(sizeof(struct rkvpss_offline_buf), GFP_KERNEL);
		if (!buf)
			goto err;
		init_vb2(ofl, buf);
		mem = ops->alloc(&buf->vb, hw->dev, size);

		if (IS_ERR_OR_NULL(mem)) {
			kfree(buf);
			goto err;
		}
		dbuf = ops->get_dmabuf(&buf->vb, mem, O_RDWR);

		if (IS_ERR_OR_NULL(dbuf)) {
			ops->put(mem);
			kfree(buf);
			goto err;
		}
		fd = dma_buf_fd(dbuf, O_CLOEXEC);
		if (fd < 0) {
			dma_buf_put(dbuf);
			ops->put(mem);
			kfree(buf);
			goto err;
		}
		get_dma_buf(dbuf);

		info->buf_fd[i] = fd;
		buf->fd = fd;
		buf->file_id = file_id;
		buf->dbuf = dbuf;
		buf->mem = mem;
		buf->alloc = true;
		buf->dev_id = info->dev_id;
		ops->prepare(buf->mem);
		mutex_lock(&hw->dev_lock);
		list_add_tail(&buf->list, &ofl->list);
		mutex_unlock(&hw->dev_lock);
		v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
			 "%s file_id:%d dev_id:%d fd:%d dbuf:%p size:%d\n",
			 __func__, file_id, buf->dev_id, fd, dbuf, size);
	}
	return 0;
err:
	for (i -= 1; i >= 0; i--)
		buf_del(ofl, info->dmabufs[i], file_id, info->dev_id, info->buf_fd[i],
			false, false);
	return -ENOMEM;
}

static int external_buf_add(struct rkvpss_offline_dev *ofl,
			    int file_id,
			    struct rkvpss_buf_info *info)
{
	void *mem;
	int i;

	for (i = 0; i < info->buf_cnt; i++) {
		mem = buf_add(ofl, info->dmabufs[i], file_id, info->dev_id, info->buf_fd[i],
			      info->buf_size[i]);
		if (!mem)
			goto err;
	}
	return 0;
err:
	for (i -= 1; i >= 0; i--)
		buf_del(ofl, info->dmabufs[i], file_id, info->dev_id, info->buf_fd[i],
			false, false);
	return -ENOMEM;
}


int rkvpss_ofl_buf_add(struct rkvpss_offline_dev *ofl,
		       int file_id,
		       struct rkvpss_buf_info *info)
{
	int ret;

	if (info->buf_alloc)
		ret = internal_buf_alloc(ofl, file_id, info);
	else
		ret = external_buf_add(ofl, file_id, info);
	return ret;
}

void rkvpss_ofl_buf_del(struct rkvpss_offline_dev *ofl,
			int file_id,
			struct rkvpss_buf_info *info)
{
	int i;

	for (i = 0; i < info->buf_cnt; i++)
		buf_del(ofl, info->dmabufs[i], file_id, info->dev_id, info->buf_fd[i],
			false, false);
}

void rkvpss_ofl_buf_del_by_file(struct rkvpss_offline_dev *ofl,
			int file_id)
{
	buf_del(ofl, NULL, file_id, 0, 0, true, false);
}

static void average_scale_down(struct rkvpss_frame_cfg *frame_cfg,
			   struct rkvpss_offline_dev *ofl,
			   struct rkvpss_output_cfg *cfg, int idx, bool unite, bool left)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	u32 in_w = cfg->crop_width, in_h = cfg->crop_height;
	u32 out_w = cfg->scl_width, out_h = cfg->scl_height;
	u32 reg_base, val, ctrl = 0, clk_mask = 0;

	switch (idx) {
	case RKVPSS_OUTPUT_CH0:
		reg_base = RKVPSS2X_SCALE0_BASE;
		clk_mask = RKVPSS_SCL0_CKG_DIS;
		break;
	case RKVPSS_OUTPUT_CH2:
		reg_base = RKVPSS_SCALE2_BASE;
		clk_mask = RKVPSS_SCL2_CKG_DIS;
		break;
	default:
		return;
	}

	/*config scl clk gate*/
	if (in_w == out_w && in_h == out_h)
		rkvpss_hw_clear_bits(hw, RKVPSS_VPSS_CLK_GATE, clk_mask);
	else
		rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CLK_GATE, clk_mask, clk_mask);

	if (!unite) {
		if (in_w == out_w && in_h == out_h)
			goto end;

		val = in_w | (in_h << 16);
		rkvpss_hw_write(hw, reg_base + 0x8, val);   /* input size */
		val = out_w | (out_h << 16);
		rkvpss_hw_write(hw, reg_base + 0xc, val);   /* output size */

		if (in_w != out_w) {
			val = (out_w - 1) * 65536 / (in_w - 1) + 1;
			rkvpss_hw_write(hw, reg_base + 0x10, val);  /* y_w_fac */
			rkvpss_hw_write(hw, reg_base + 0x14, val);  /* c_w_fac */
			ctrl |= RKVPSS2X_SW_SCL_HY_EN | RKVPSS2X_SW_SCL_HC_EN |
			       RKVPSS2X_SW_AVG_SCALE_H_EN;
		}

		if (in_h != out_h) {
			val = (out_h - 1) * 65536 / (in_h - 1) + 1;
			rkvpss_hw_write(hw, reg_base + 0x18, val);  /* y_h_fac */
			rkvpss_hw_write(hw, reg_base + 0x1c, val);  /* c_h_fac */
			ctrl |= RKVPSS2X_SW_SCL_VY_EN | RKVPSS2X_SW_SCL_VC_EN |
			       RKVPSS2X_SW_AVG_SCALE_V_EN;
		}
	} else {
		u32 in_width = cfg->crop_width / 2;
		u32 out_width = cfg->scl_width / 2;
		u32 overlap = UNITE_ENLARGE;

		if (left) {
			/* Left side processing */
			rkvpss_hw_write(hw, reg_base + 0x50, 0);   /* in_crop_offs */
			rkvpss_hw_write(hw, reg_base + 0x20, 0);   /* hy_offs */
			rkvpss_hw_write(hw, reg_base + 0x24, 0);   /* hc_offs */
			rkvpss_hw_write(hw, reg_base + 0x28, 0);   /* vy_offs */
			rkvpss_hw_write(hw, reg_base + 0x2c, 0);   /* vc_offs */
			rkvpss_hw_write(hw, reg_base + 0x48, 0);   /* hy_offs_mi */
			rkvpss_hw_write(hw, reg_base + 0x4c, 0);   /* hc_offs_mi */

			u32 in_w_for_hw = (in_width == out_width) ? in_width : (in_width + overlap);
			u32 aligned_width = ALIGN(in_w_for_hw, 16);

			val = aligned_width | (cfg->crop_height << 16);
			rkvpss_hw_write(hw, reg_base + 0x8, val);   /* input size */

			aligned_width = ALIGN(out_width, 16);
			val = aligned_width | (cfg->scl_height << 16);
			rkvpss_hw_write(hw, reg_base + 0xc, val);   /* output size */

			ctrl |= RKVPSS2X_SW_SCL_CLIP_EN;
		} else {
			/* Right side processing */
			val = ofl->unite_params[idx].scl_in_crop_w_y |
			      (ofl->unite_params[idx].scl_in_crop_w_c << 4);
			rkvpss_hw_write(hw, reg_base + 0x50, val);   /* in_crop_offs */

			ctrl |= RKVPSS2X_SW_SCL_HPHASE_EN;

			u32 y_phase = ofl->unite_params[idx].y_w_phase;
			u32 c_phase = ofl->unite_params[idx].c_w_phase;

			rkvpss_hw_write(hw, reg_base + 0x20, y_phase);   /* hy_offs */
			rkvpss_hw_write(hw, reg_base + 0x24, c_phase);   /* hc_offs */
			rkvpss_hw_write(hw, reg_base + 0x28, 0);   /* vy_offs */
			rkvpss_hw_write(hw, reg_base + 0x2c, 0);   /* vc_offs */

			val = 16 - (cfg->scl_width / 2 & 0xf);
			if (val == 16)
				val = 0;
			rkvpss_hw_write(hw, reg_base + 0x48, val);   /* hy_offs_mi */
			rkvpss_hw_write(hw, reg_base + 0x4c, val);   /* hc_offs_mi */

			u32 in_w_for_hw = (in_width == out_width) ? in_width : (in_width + overlap);
			u32 aligned_width = ALIGN(in_w_for_hw, 16);

			val = aligned_width | (cfg->crop_height << 16);
			rkvpss_hw_write(hw, reg_base + 0x8, val);   /* input size */

			aligned_width = ALIGN(out_width, 16);
			val = aligned_width | (cfg->scl_height << 16);
			rkvpss_hw_write(hw, reg_base + 0xc, val);   /* output size */

			v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
				"Right side: in_w=%d out_w=%d y_phase=0x%x c_phase=0x%x mi_offset=0x%x\n",
				aligned_width, out_width, y_phase, c_phase, val);

			if (in_width != out_width) {
				ctrl |= RKVPSS2X_SW_SCL_HY_EN | RKVPSS2X_SW_SCL_HC_EN |
				       RKVPSS2X_SW_AVG_SCALE_H_EN;
			}
			if (cfg->crop_height != cfg->scl_height) {
				ctrl |= RKVPSS2X_SW_SCL_VY_EN | RKVPSS2X_SW_SCL_VC_EN |
				       RKVPSS2X_SW_AVG_SCALE_V_EN;
			}

			if (overlap > 0) {
				ctrl |= RKVPSS2X_SW_SCL_CLIP_EN;
				if (aligned_width > out_width)
					ctrl |= RKVPSS2X_SW_SCL_IN_CLIP_EN;
			}
		}

		if (cfg->scl_width != frame_cfg->input.width) {
			val = ofl->unite_params[idx].y_w_fac;
			rkvpss_hw_write(hw, reg_base + 0x10, val);   /* y_w_fac */
			val = ofl->unite_params[idx].c_w_fac;
			rkvpss_hw_write(hw, reg_base + 0x14, val);   /* c_w_fac */
			ctrl |= RKVPSS2X_SW_SCL_HY_EN | RKVPSS2X_SW_SCL_HC_EN |
			       RKVPSS2X_SW_AVG_SCALE_H_EN;
		}
		if (cfg->scl_height != frame_cfg->input.height) {
			val = ofl->unite_params[idx].y_h_fac;
			rkvpss_hw_write(hw, reg_base + 0x18, val);   /* y_h_fac */
			val = ofl->unite_params[idx].c_h_fac;
			rkvpss_hw_write(hw, reg_base + 0x1c, val);   /* c_h_fac */
			ctrl |= RKVPSS2X_SW_SCL_VY_EN | RKVPSS2X_SW_SCL_VC_EN |
			       RKVPSS2X_SW_AVG_SCALE_V_EN;
		}
	}

end:
	rkvpss_hw_write(hw, reg_base, ctrl);
	val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
	rkvpss_hw_write(hw, reg_base + 0x4, val);

	v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
		 "%s unite:%d left:%d hw ch:%d ctrl:0x%x y_src_size:0x%x y_dst_size:0x%x\n",
		 __func__, unite, left, idx,
		 rkvpss_hw_read(hw, reg_base),
		 rkvpss_hw_read(hw, reg_base + 0x8),
		 rkvpss_hw_read(hw, reg_base + 0xc));
}

static void bilinear_scale(struct rkvpss_frame_cfg *frame_cfg,
			   struct rkvpss_offline_dev *ofl,
			   struct rkvpss_output_cfg *cfg, int idx, bool unite, bool left)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	u32 in_w = cfg->crop_width, in_h = cfg->crop_height;
	u32 out_w = cfg->scl_width, out_h = cfg->scl_height;
	u32 reg_base, in_div, out_div, val, ctrl = 0, clk_mask = 0;
	bool yuv420_in = false, yuv422_to_420 = false;

	switch (idx) {
	case RKVPSS_OUTPUT_CH0:
		reg_base = RKVPSS2X_SCALE0_BASE;
		clk_mask = RKVPSS_SCL0_CKG_DIS;
		break;
	case RKVPSS_OUTPUT_CH1:
		reg_base = RKVPSS_SCALE1_BASE;
		clk_mask = RKVPSS_SCL1_CKG_DIS;
		break;
	case RKVPSS_OUTPUT_CH2:
		reg_base = RKVPSS_SCALE2_BASE;
		clk_mask = RKVPSS_SCL2_CKG_DIS;
		break;
	case RKVPSS_OUTPUT_CH3:
		reg_base = RKVPSS_SCALE3_BASE;
		clk_mask = RKVPSS_SCL3_CKG_DIS;
		break;
	case RKVPSS_OUTPUT_CH4:
		reg_base = RKVPSS2X_SCALE4_BASE;
		clk_mask = RKVPSS2X_SCL4_CKG_DIS;
		break;
	case RKVPSS_OUTPUT_CH5:
		reg_base = RKVPSS2X_SCALE5_BASE;
		clk_mask = RKVPSS2X_SCL5_CKG_DIS;
		break;
	default:
		return;
	}

	/*config scl clk gate*/
	if (in_w == out_w && in_h == out_h)
		rkvpss_hw_clear_bits(hw, RKVPSS_VPSS_CLK_GATE, clk_mask);
	else
		rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CLK_GATE, clk_mask, clk_mask);

	if (!unite) {
		if (in_w == out_w && in_h == out_h)
			goto end;

		/* TODO diff for input and output format */
		if (yuv420_in) {
			in_div = 2;
			out_div = 2;
		} else if (yuv422_to_420) {
			in_div = 1;
			out_div = 2;
		} else {
			in_div = 1;
			out_div = 1;
		}

		val = in_w | (in_h << 16);
		rkvpss_hw_write(hw, reg_base + 0x8, val);
		val = out_w | (out_h << 16);
		rkvpss_hw_write(hw, reg_base + 0xc, val);

		if (in_w != out_w) {
			val = (in_w - 1) * 4096 / (out_w - 1);
			rkvpss_hw_write(hw, reg_base + 0x10, val);
			val = (in_w / 2 - 1) * 4096 / (out_w / 2 - 1);
			rkvpss_hw_write(hw, reg_base + 0x14, val);

			ctrl |= RKVPSS_SCL_HY_EN | RKVPSS_SCL_HC_EN;
		}
		if (in_h != out_h) {
			val = (in_h - 1) * 4096 / (out_h - 1);
			rkvpss_hw_write(hw, reg_base + 0x18, val);
			val = (in_h / in_div - 1) * 4096 / (out_h / out_div - 1);
			rkvpss_hw_write(hw, reg_base + 0x1c, val);

			ctrl |= RKVPSS_SCL_VY_EN | RKVPSS_SCL_VC_EN;
		}
	} else {
		if (left) {
			rkvpss_hw_write(hw, reg_base + 0x50, 0);
			rkvpss_hw_write(hw, reg_base + 0x20, 0);
			rkvpss_hw_write(hw, reg_base + 0x24, 0);
			rkvpss_hw_write(hw, reg_base + 0x48, 0);
			rkvpss_hw_write(hw, reg_base + 0x4c, 0);
			if (in_w == out_w)
				val = (cfg->crop_width / 2) | (cfg->crop_height << 16);
			else
				val = (cfg->crop_width / 2 + UNITE_ENLARGE) |
				       (cfg->crop_height << 16);
			rkvpss_hw_write(hw, reg_base + 0x8, val);
			val = cfg->scl_width / 2 | (cfg->scl_height << 16);
			rkvpss_hw_write(hw, reg_base + 0xc, val);
			ctrl |= RKVPSS_SCL_CLIP_EN;
		} else {
			val = ofl->unite_params[idx].scl_in_crop_w_y |
			      (ofl->unite_params[idx].scl_in_crop_w_c << 4);
			rkvpss_hw_write(hw, reg_base + 0x50, val);
			rkvpss_hw_write(hw, reg_base + 0x20, ofl->unite_params[idx].y_w_phase);
			rkvpss_hw_write(hw, reg_base + 0x24, ofl->unite_params[idx].c_w_phase);
			val = cfg->scl_width / 2 - ALIGN_DOWN(cfg->scl_width / 2, 16);
			rkvpss_hw_write(hw, reg_base + 0x48, val);
			rkvpss_hw_write(hw, reg_base + 0x4c, val);
			val = (cfg->crop_width / 2 + ofl->unite_right_enlarge) |
			      (cfg->crop_height << 16);
			rkvpss_hw_write(hw, reg_base + 0x8, val);
			val = cfg->scl_width / 2 | (cfg->scl_height << 16);
			rkvpss_hw_write(hw, reg_base + 0xc, val);
			ctrl |= RKVPSS_SCL_CLIP_EN | RKVPSS_SCL_IN_CLIP_EN;
		}
		if (cfg->scl_width != frame_cfg->input.width) {
			rkvpss_hw_write(hw, reg_base + 0x10, ofl->unite_params[idx].y_w_fac);
			rkvpss_hw_write(hw, reg_base + 0x14, ofl->unite_params[idx].c_w_fac);
			ctrl |= RKVPSS_SCL_HY_EN | RKVPSS_SCL_HC_EN;
		}
		if (cfg->scl_height != frame_cfg->input.height) {
			rkvpss_hw_write(hw, reg_base + 0x18, ofl->unite_params[idx].y_h_fac);
			rkvpss_hw_write(hw, reg_base + 0x1c, ofl->unite_params[idx].c_h_fac);
			ctrl |= RKVPSS_SCL_VY_EN | RKVPSS_SCL_VC_EN;
		}
	}

end:
	rkvpss_hw_write(hw, reg_base, ctrl);
	val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
	rkvpss_hw_write(hw, reg_base + 0x4, val);

	v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
		 "%s unite:%d left:%d hw ch:%d ctrl:0x%x y_src_size:0x%x y_dst_size:0x%x\n",
		 __func__, unite, left, idx,
		 rkvpss_hw_read(hw, reg_base),
		 rkvpss_hw_read(hw, reg_base + 0x8),
		 rkvpss_hw_read(hw, reg_base + 0xc));
}

static void scale_config(struct rkvpss_offline_dev *ofl,
			 struct rkvpss_frame_cfg *cfg, bool unite, bool left)
{
	int i;
	bool is_downscale_w;
	bool is_downscale_h;
	bool use_average;

	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (!cfg->output[i].enable)
			continue;

		is_downscale_w = cfg->output[i].scl_width <= cfg->output[i].crop_width;
		is_downscale_h = cfg->output[i].scl_height <= cfg->output[i].crop_height;
		use_average = is_downscale_w && is_downscale_h;

		if ((i == RKVPSS_OUTPUT_CH0 || i == RKVPSS_OUTPUT_CH2) &&
		    (use_average || cfg->output[i].avg_scl_down)) {
			v4l2_dbg(2, rkvpss_debug, &ofl->v4l2_dev,
				 "CH%d: average_scale_down, unite:%d left:%d\n", i, unite, left);
			average_scale_down(cfg, ofl, &cfg->output[i], i, unite, left);
		} else {
			v4l2_dbg(2, rkvpss_debug, &ofl->v4l2_dev,
				 "CH%d: bilinear_scale, unite:%d left:%d\n", i, unite, left);
			bilinear_scale(cfg, ofl, &cfg->output[i], i, unite, left);
		}
	}
}

static int cmsc_config(struct rkvpss_offline_dev *ofl,
		       struct rkvpss_frame_cfg *cfg, bool unite, bool left)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	struct rkvpss_cmsc_cfg *cmsc_cfg, tmp_cfg = {0};
	struct rkvpss_cmsc_win *win;
	struct rkvpss_cmsc_point *point;
	int i, j, k;
	u32 ch_win_en[RKVPSS_OUT_V20_MAX];
	u32 ch_win_mode[RKVPSS_OUT_V20_MAX];
	u32 win_color[RKVPSS_CMSC_WIN_MAX];
	u32 val, slope, hor, mask, mosaic_block = 0, ctrl = 0;
	u32 hw_in_w, hw_in_h;

	if (!hw->is_ofl_cmsc)
		return 0;

	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		ch_win_en[i] = 0;
		ch_win_mode[i] = 0;
		cmsc_cfg = &cfg->output[i].cmsc;
		for (j = 0; j < RKVPSS_CMSC_WIN_MAX; j++) {
			if (i == 0)
				win_color[j] = 0;
			if (!cmsc_cfg->win[j].win_en)
				continue;
			ch_win_en[i] |= BIT(j);
			ch_win_mode[i] |= cmsc_cfg->win[j].mode ? BIT(j) : 0;
			/** mosaic_block use the last channel **/
			if (cmsc_cfg->win[j].mode && cfg->output[i].enable)
				mosaic_block = cfg->output[i].cmsc.mosaic_block;
			/** window cover all channel consistent **/
			if (!cfg->output[i].cmsc.win[j].mode) {
				win_color[j] = RKVPSS_CMSK_WIN_YUV(
							cfg->output[i].cmsc.win[j].cover_color_y,
							cfg->output[i].cmsc.win[j].cover_color_u,
							cfg->output[i].cmsc.win[j].cover_color_v);
				if (cfg->output[i].cmsc.win[j].cover_color_a > 15)
					cfg->output[i].cmsc.win[j].cover_color_a = 15;
				win_color[j] |= RKVPSS_CMSC_WIN_ALPHA(
							cfg->output[i].cmsc.win[j].cover_color_a);
			}
			for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++)
				tmp_cfg.win[j].point[k] = cmsc_cfg->win[j].point[k];
		}
	}

	/* deal unite left params */
	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (!unite || !left)
			break;
		for (j = 0; j < RKVPSS_CMSC_WIN_MAX; j++) {
			win = &tmp_cfg.win[j];
			if (!(ch_win_en[i] & BIT(j)))
				continue;
			mask = 0;
			for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
				point = &win->point[k];
				if (point->x >= cfg->input.width / 2)
					mask |= BIT(k);
				else
					mask &= ~BIT(k);
			}
			if (mask == 0xf) {
				/** all right **/
				ch_win_en[i] &= ~BIT(j);
			} else if (mask != 0) {
				/** middle  need avoid pentagon **/
				if (win->point[0].x != win->point[3].x ||
				    win->point[1].x != win->point[2].x) {
					ch_win_en[i] &= ~BIT(j);
				} else {
					win->point[1].x = cfg->input.width / 2;
					win->point[2].x = cfg->input.width / 2;
				}
			}
		}
	}

	/* deal unite right params */
	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (!unite || left)
			break;
		for (j = 0; j < RKVPSS_CMSC_WIN_MAX; j++) {
			win = &tmp_cfg.win[j];
			if (!(ch_win_en[i] & BIT(j)))
				continue;
			mask = 0;
			for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
				point = &win->point[k];
				if (point->x <= cfg->input.width / 2)
					mask |= BIT(k);
				else
					mask &= ~BIT(k);
			}
			if (mask == 0xf) {
				/** all left **/
				ch_win_en[i] &= ~BIT(j);
			} else if (mask != 0) {
				/** middle	need avoid pentagon **/
				if (win->point[0].x != win->point[3].x ||
					win->point[1].x != win->point[2].x) {
					ch_win_en[i] &= ~BIT(j);
				} else {
					win->point[0].x = ofl->unite_right_enlarge;
					win->point[3].x = ofl->unite_right_enlarge;
					win->point[1].x = win->point[1].x -
							  (cfg->input.width / 2) +
							  ofl->unite_right_enlarge;
					win->point[2].x = win->point[2].x -
							  (cfg->input.width / 2) +
							  ofl->unite_right_enlarge;
				}
			} else {
				/** all right **/
				win->point[0].x = win->point[0].x -
						  (cfg->input.width / 2) +
						  ofl->unite_right_enlarge;
				win->point[1].x = win->point[1].x -
						  (cfg->input.width / 2) +
						  ofl->unite_right_enlarge;
				win->point[2].x = win->point[2].x -
						  (cfg->input.width / 2) +
						  ofl->unite_right_enlarge;
				win->point[3].x = win->point[3].x -
						  (cfg->input.width / 2) +
						  ofl->unite_right_enlarge;
			}
		}
	}

	for (i = 0; i < RKVPSS_OUTPUT_CH4; i++) {
		if (ch_win_en[i]) {
			ctrl |= RKVPSS_CMSC_EN;
			ctrl |= RKVPSS_CMSC_CHN_EN(i);
		}
		rkvpss_hw_write(hw, RKVPSS_CMSC_CHN0_WIN + i * 4, ch_win_en[i]);
		rkvpss_hw_write(hw, RKVPSS_CMSC_CHN0_MODE + i * 4, ch_win_mode[i]);
		hw_in_w = rkvpss_hw_read(hw, RKVPSS_MI_RD_Y_WIDTH);
		hw_in_h = rkvpss_hw_read(hw, RKVPSS_MI_RD_Y_HEIGHT);
		for (j = 0; j < RKVPSS_CMSC_WIN_MAX; j++) {
			if (!(ch_win_en[i] & BIT(j)))
				continue;
			for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
				if (tmp_cfg.win[j].point[k].x > hw_in_w ||
				    tmp_cfg.win[j].point[k].y > hw_in_h) {
					v4l2_err(&ofl->v4l2_dev,
						 "%s cmsc coordinate error dev_id:%d unite:%u left:%u ch:%d win:%d point:%d x:%u y:%u hw_in_w:%u hw_in_h:%u\n",
						 __func__, cfg->dev_id, unite, left,
						 i, j, k,
						 tmp_cfg.win[j].point[k].x,
						 tmp_cfg.win[j].point[k].y,
						 hw_in_w, hw_in_w);
					return -EINVAL;
				}
				val = RKVPSS_CMSC_WIN_VTX(tmp_cfg.win[j].point[k].x,
							  tmp_cfg.win[j].point[k].y);
				rkvpss_hw_write(hw, RKVPSS_CMSC_WIN0_L0_VTX + k * 8 + j * 32, val);
				rkvpss_cmsc_slop(&tmp_cfg.win[j].point[k],
					 (k + 1 == RKVPSS_CMSC_POINT_MAX) ?
					 &tmp_cfg.win[j].point[0] : &tmp_cfg.win[j].point[k + 1],
					 &slope, &hor);
				val = RKVPSS_CMSC_WIN_SLP(slope, hor);
				rkvpss_hw_write(hw, RKVPSS_CMSC_WIN0_L0_SLP + k * 8 + j * 32, val);
				v4l2_dbg(4, rkvpss_debug, &ofl->v4l2_dev,
					 "%s dev_id:%d, unite:%d left:%d ch:%d win:%d point:%d x:%u y:%u",
					 __func__, cfg->dev_id, unite, left, i, j, k,
					 tmp_cfg.win[j].point[k].x,
					 tmp_cfg.win[j].point[k].y);
			}
			if ((ch_win_mode[i] & BIT(j)))
				continue;
			rkvpss_hw_write(hw, RKVPSS_CMSC_WIN0_PARA + j * 4, win_color[j]);
		}
	}

	/* chn4 cmsc */
	if (cfg->output[RKVPSS_OUTPUT_CH4].cmsc.reuse_ch) {
		mask = RKVPSS2X_CHN4_CMSC_SEL(3);
		val = RKVPSS2X_CHN4_CMSC_SEL(cfg->output[RKVPSS_OUTPUT_CH4].cmsc.reuse_ch);
		rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CTRL, mask, val);
	}

	/* chn5 cmsc */
	if (cfg->output[RKVPSS_OUTPUT_CH5].cmsc.reuse_ch) {
		mask = RKVPSS2X_CHN5_CMSC_SEL(3);
		val = RKVPSS2X_CHN5_CMSC_SEL(cfg->output[RKVPSS_OUTPUT_CH5].cmsc.reuse_ch);
		rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CTRL, mask, val);
	}

	ctrl |= RKVPSS2X_CMSC_BLK_SIZE(mosaic_block);
	rkvpss_hw_write(hw, RKVPSS_CMSC_CTRL, ctrl);

	val = RKVPSS_CMSC_GEN_UPD | RKVPSS_CMSC_FORCE_UPD;
	rkvpss_hw_write(hw, RKVPSS_CMSC_UPDATE, val);

	v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
		 "%s dev_id:%d, unite:%d left:%d hw ctrl:0x%x update_val:0x%x",
		 __func__, cfg->dev_id, unite, left, ctrl, val);

	return 0;
}

static void aspt_config(struct rkvpss_offline_dev *ofl,
			struct rkvpss_frame_cfg *cfg)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	u32 reg_base, val;
	int i;

	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (!cfg->output[i].enable)
			continue;

		switch (i) {
		case RKVPSS_OUTPUT_CH0:
			reg_base = RKVPSS_RATIO0_BASE;
			break;
		case RKVPSS_OUTPUT_CH1:
			reg_base = RKVPSS_RATIO1_BASE;
			break;
		case RKVPSS_OUTPUT_CH2:
			reg_base = RKVPSS_RATIO2_BASE;
			break;
		case RKVPSS_OUTPUT_CH3:
			reg_base = RKVPSS_RATIO3_BASE;
			break;
		case RKVPSS_OUTPUT_CH4:
			reg_base = RKVPSS2X_RATIO4_BASE;
			break;
		case RKVPSS_OUTPUT_CH5:
			reg_base = RKVPSS2X_RATIO5_BASE;
			break;
		default:
			return;
		}

		if (!cfg->output[i].aspt.enable) {
			rkvpss_hw_write(hw, reg_base, 0);
			val = RKVPSS_RATIO_FORCE_UPD | RKVPSS_RATIO_GEN_UPD;
			rkvpss_hw_write(hw, reg_base + 0x4, val);
			continue;
		}
		val = cfg->output[i].scl_width | (cfg->output[i].scl_height << 16);
		rkvpss_hw_write(hw, reg_base + 0x10, val);
		val = cfg->output[i].aspt.width | (cfg->output[i].aspt.height << 16);
		rkvpss_hw_write(hw, reg_base + 0x14, val);
		val = cfg->output[i].aspt.h_offs | (cfg->output[i].aspt.v_offs << 16);
		rkvpss_hw_write(hw, reg_base + 0x18, val);
		val = cfg->output[i].aspt.color_y |
			  (cfg->output[i].aspt.color_u << 16) |
			  (cfg->output[i].aspt.color_v << 24);
		rkvpss_hw_write(hw, reg_base + 0x1c, val);
		rkvpss_hw_write(hw, reg_base, RKVPSS_RATIO_EN);
		val = RKVPSS_RATIO_FORCE_UPD | RKVPSS_RATIO_GEN_UPD;
		rkvpss_hw_write(hw, reg_base + 0x4, val);

		v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
			 "%s hw ch:%d ctrl:0x%x in_size:0x%x out_size:0x%x offset:0x%x\n",
			 __func__, i,
			 rkvpss_hw_read(hw, reg_base),
			 rkvpss_hw_read(hw, reg_base + 0x10),
			 rkvpss_hw_read(hw, reg_base + 0x14),
			 rkvpss_hw_read(hw, reg_base + 0x18));
	}
}

static void add_cfginfo(struct rkvpss_offline_dev *ofl, struct rkvpss_frame_cfg *cfg)
{
	struct rkvpss_ofl_cfginfo *cfginfo = NULL, *new_cfg = NULL, *first_cfg = NULL;
	int i, count = 0;

	new_cfg = kzalloc(sizeof(struct rkvpss_ofl_cfginfo), GFP_KERNEL);
	new_cfg->dev_id = cfg->dev_id;
	new_cfg->sequence = cfg->sequence;
	new_cfg->input.buf_fd = cfg->input.buf_fd;
	new_cfg->input.format = cfg->input.format;
	new_cfg->input.width = cfg->input.width;
	new_cfg->input.height = cfg->input.height;

	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		new_cfg->output[i].enable = cfg->output[i].enable;
		new_cfg->output[i].buf_fd = cfg->output[i].buf_fd;
		new_cfg->output[i].format = cfg->output[i].format;
		new_cfg->output[i].crop_v_offs = cfg->output[i].crop_v_offs;
		new_cfg->output[i].crop_h_offs = cfg->output[i].crop_h_offs;
		new_cfg->output[i].crop_width = cfg->output[i].crop_width;
		new_cfg->output[i].crop_height = cfg->output[i].crop_height;
		new_cfg->output[i].scl_width = cfg->output[i].scl_width;
		new_cfg->output[i].scl_height = cfg->output[i].scl_height;
	}

	mutex_lock(&ofl->ofl_lock);
	list_for_each_entry(cfginfo, &ofl->cfginfo_list, list) {
		count++;
	}
	while (count >= rkvpss_cfginfo_num && count != 0) {
		first_cfg = list_first_entry(&ofl->cfginfo_list, struct rkvpss_ofl_cfginfo, list);
		list_del_init(&first_cfg->list);
		kfree(first_cfg);
		count--;
	}
	if (rkvpss_cfginfo_num)
		list_add_tail(&new_cfg->list, &ofl->cfginfo_list);
	else
		kfree(new_cfg);
	mutex_unlock(&ofl->ofl_lock);
}


static int read_config(struct rkvpss_offline_dev *ofl,
		       int file_id,
		       struct rkvpss_frame_cfg *cfg,
		       bool unite, bool left)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *mem_ops = hw->mem_ops;
	struct sg_table  *sg_tbl;
	struct rkvpss_offline_buf *buf;
	u32 in_ctrl, in_size, in_c_offs, unite_r_offs, val, mask, unite_off = 0, enlarge = 0,
		header_size = 0, payload_size = 0;

	in_c_offs = 0;
	in_ctrl = 0;
	switch (cfg->input.format) {
	case V4L2_PIX_FMT_NV16:
		in_c_offs = cfg->input.ver_stride ?
				cfg->input.stride * cfg->input.ver_stride :
				cfg->input.stride * cfg->input.height;
		in_size = cfg->input.stride * cfg->input.height * 2;
		in_ctrl |= RKVPSS_MI_RD_INPUT_422SP;
		unite_off = 8;
		break;
	case V4L2_PIX_FMT_NV12:
		in_c_offs = cfg->input.ver_stride ?
				cfg->input.stride * cfg->input.ver_stride :
				cfg->input.stride * cfg->input.height;
		in_size = cfg->input.stride * cfg->input.height * 3 / 2;
		in_ctrl |= RKVPSS_MI_RD_INPUT_420SP;
		unite_off = 8;
		break;
	case V4L2_PIX_FMT_NV61:
		in_c_offs = cfg->input.ver_stride ?
				cfg->input.stride * cfg->input.ver_stride :
				cfg->input.stride * cfg->input.height;
		in_size = cfg->input.stride * cfg->input.height * 2;
		in_ctrl |= RKVPSS_MI_RD_INPUT_422SP | RKVPSS_MI_RD_UV_SWAP;
		unite_off = 8;
		break;
	case V4L2_PIX_FMT_NV21:
		in_c_offs = cfg->input.ver_stride ?
				cfg->input.stride * cfg->input.ver_stride :
				cfg->input.stride * cfg->input.height;
		in_size = cfg->input.stride * cfg->input.height * 3 / 2;
		in_ctrl |= RKVPSS_MI_RD_INPUT_420SP | RKVPSS_MI_RD_UV_SWAP;
		unite_off = 8;
		break;
	case V4L2_PIX_FMT_RGB565:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_BGR565;
		unite_off = 16;
		break;
	case V4L2_PIX_FMT_RGB565X:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_BGR565 | RKVPSS_MI_RD_RB_SWAP;
		unite_off = 16;
		break;
	case V4L2_PIX_FMT_RGB24:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_BGR888;
		unite_off = 24;
		break;
	case V4L2_PIX_FMT_BGR24:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_BGR888 | RKVPSS_MI_RD_RB_SWAP;
		unite_off = 24;
		break;
	case V4L2_PIX_FMT_XRGB32:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_ABGR888;
		unite_off = 32;
		break;
	case V4L2_PIX_FMT_XBGR32:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_ABGR888 | RKVPSS_MI_RD_RB_SWAP;
		unite_off = 32;
		break;
	case V4L2_PIX_FMT_RGBX32:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_ABGR888
				| RKVPSS_MI_RD_ALPHA_SWAP;
		unite_off = 32;
		break;
	case V4L2_PIX_FMT_BGRX32:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS_MI_RD_INPUT_ABGR888
				| RKVPSS_MI_RD_RB_SWAP
				| RKVPSS_MI_RD_ALPHA_SWAP;
		unite_off = 32;
		break;
	case V4L2_PIX_FMT_FBC0:
		cfg->input.stride = 0;
		in_c_offs = 0;
		header_size = ((cfg->input.width + 63) / 64) *
			((cfg->input.height + 3) / 4) * 16;
		payload_size = ((cfg->input.width + 63) / 64) * 384 *
			((cfg->input.height + 3) / 4);
		in_size = header_size + payload_size;
		in_ctrl |= RKVPSS_MI_RD_INPUT_420SP;
		break;
	case V4L2_PIX_FMT_FBC2:
		cfg->input.stride = 0;
		in_c_offs = 0;
		header_size = ((cfg->input.width + 63) / 64) *
			((cfg->input.height + 3) / 4) * 16;
		payload_size = ((cfg->input.width + 63) / 64) * 512 *
			((cfg->input.height + 3) / 4);
		in_size = header_size + payload_size;
		in_ctrl |= RKVPSS_MI_RD_INPUT_422SP;
		break;
	case V4L2_PIX_FMT_FBC4:
		cfg->input.stride = 0;
		in_c_offs = 0;
		header_size = ((cfg->input.width + 63) / 64) *
			((cfg->input.height + 3) / 4) * 16;
		payload_size = ((cfg->input.width + 63) / 64) * 768 *
			((cfg->input.height + 3) / 4);
		in_size = header_size + payload_size;
		in_ctrl |= RKVPSS_MI_RD_INPUT_422SP | RKVPSS_MI_RD_FBCD_YUV444_EN;
		break;
	case V4L2_PIX_FMT_TILE420:
		in_c_offs = 0;
		in_size = cfg->input.stride * (cfg->input.height / 4);
		in_ctrl |= RKVPSS_MI_RD_INPUT_420SP;
		break;
	case V4L2_PIX_FMT_TILE422:
		in_c_offs = 0;
		in_size = cfg->input.stride * (cfg->input.height / 4);
		in_ctrl |= RKVPSS_MI_RD_INPUT_422SP;
		break;
	case V4L2_PIX_FMT_UYVY:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS2X_MI_RD_INPUT_UYVY;
		//unite_off  todo
		break;
	case V4L2_PIX_FMT_VYUY:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS2X_MI_RD_INPUT_UYVY | RKVPSS_MI_RD_UV_SWAP;
		//unite_off  todo
		break;
	case V4L2_PIX_FMT_YUYV:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS2X_MI_RD_INPUT_UYVY | RKVPSS_MI_RD_RB_SWAP;
		//unite_off  todo
		break;
	case V4L2_PIX_FMT_YVYU:
		in_size = cfg->input.stride * cfg->input.height;
		in_ctrl |= RKVPSS2X_MI_RD_INPUT_UYVY | RKVPSS_MI_RD_RB_SWAP | RKVPSS_MI_RD_UV_SWAP;
		//unite_off  todo
		break;
	default:
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d no support input format:%c%c%c%c\n",
			 cfg->dev_id, cfg->input.format, cfg->input.format >> 8,
			 cfg->input.format >> 16, cfg->input.format >> 24);
		return -EINVAL;
	}

	buf = buf_add(ofl, cfg->input.dmabuf, file_id, cfg->dev_id, cfg->input.buf_fd, in_size);
	if (!buf)
		return -ENOMEM;

	sg_tbl = mem_ops->cookie(&buf->vb, buf->mem);

	if (!unite) {
		val = cfg->input.width;
		rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_WIDTH, val);
		val = cfg->input.height;
		rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_HEIGHT, val);
		val = sg_dma_address(sg_tbl->sgl);
		rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_BASE, val);
		val += in_c_offs;
		rkvpss_hw_write(hw, RKVPSS_MI_RD_C_BASE, val);
	} else {
		val = cfg->input.height;
		rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_HEIGHT, val);
		ofl->unite_right_enlarge = ALIGN(cfg->input.width / 2, 16) -
					  (cfg->input.width / 2) + 16;

		if (left) {
			if (!cfg->mirror)
				enlarge = UNITE_LEFT_ENLARGE;
			else
				enlarge = ofl->unite_right_enlarge;
			val = cfg->input.width / 2 + enlarge;
			rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_WIDTH, val);
			val = sg_dma_address(sg_tbl->sgl);
			rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_BASE, val);
			val += in_c_offs;
			rkvpss_hw_write(hw, RKVPSS_MI_RD_C_BASE, val);
		} else {
			if (!cfg->mirror)
				enlarge = ofl->unite_right_enlarge;
			else
				enlarge = UNITE_LEFT_ENLARGE;
			val = cfg->input.width / 2 + enlarge;
			rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_WIDTH, val);
			val = (cfg->input.width / 2 - enlarge) * unite_off;
			unite_r_offs = ALIGN_DOWN(val / 8, 16);
			val = sg_dma_address(sg_tbl->sgl) + unite_r_offs;
			rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_BASE, val);
			val += in_c_offs;
			rkvpss_hw_write(hw, RKVPSS_MI_RD_C_BASE, val);
		}
	}

	v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
		 "%s unite:%d left:%d hw width:%d height:%d y_base:0x%x\n",
		 __func__, unite, left,
		 rkvpss_hw_read(hw, RKVPSS_MI_RD_Y_WIDTH),
		 rkvpss_hw_read(hw, RKVPSS_MI_RD_Y_HEIGHT),
		 rkvpss_hw_read(hw, RKVPSS_MI_RD_Y_BASE));

	if (cfg->input.format == V4L2_PIX_FMT_FBC0 ||
	    cfg->input.format == V4L2_PIX_FMT_FBC2 ||
	    cfg->input.format == V4L2_PIX_FMT_FBC4) {
		in_ctrl |= RKVPSS_MI_RD_MODE(2) | RKVPSS_MI_RD_FBCD_OPT_DIS;
		rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_STRIDE, 0);
	} else {
		if (cfg->input.format == V4L2_PIX_FMT_TILE420 ||
			cfg->input.format == V4L2_PIX_FMT_TILE422) {
			in_ctrl |= RKVPSS_MI_RD_MODE(1);
			switch (cfg->input.rotate) {
			case ROTATE_90:
				in_ctrl |= RKVPSS_MI_RD_ROT_90;
				break;
			case ROTATE_180:
				in_ctrl |= RKVPSS_MI_RD_ROT_180;
				break;
			case ROTATE_270:
				in_ctrl |= RKVPSS_MI_RD_ROT_270;
				break;
			default:
				in_ctrl |= RKVPSS_MI_RD_ROT_0;
				break;
			}
		}
		val = cfg->input.stride;
		rkvpss_hw_write(hw, RKVPSS_MI_RD_Y_STRIDE, val);
	}

	/* raster rotate 90 */
	switch (cfg->input.format) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YUYV:
		if (cfg->input.rotate_90)
			in_ctrl |= RKVPSS_MI_RD_MODE(3);
		break;
	default:
		break;
	}

	mask = RKVPSS_MI_RD_GROUP_MODE(3) | RKVPSS_MI_RD_BURST16_LEN;
	rkvpss_hw_set_bits(hw, RKVPSS_MI_RD_CTRL, ~mask, in_ctrl);
	rkvpss_hw_write(hw, RKVPSS_MI_RD_INIT, RKVPSS_MI_RD_FORCE_UPD);

	return 0;
}

static void crop_ch4_5(struct rkvpss_offline_dev *ofl,
			struct rkvpss_frame_cfg *cfg,
			u32 *crop_en,
			bool unite, bool left)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	int i;
	u32 off_reg, size_reg;

	if (!unite) {
		for (i = RKVPSS_OUTPUT_CH4; i < RKVPSS_OUT_V20_MAX; i++) {
			if (!cfg->output[i].enable)
				continue;
			if (i == RKVPSS_OUTPUT_CH4) {
				off_reg = RKVPSS2X_CROP0_4_OFFS;
				size_reg = RKVPSS2X_CROP0_4_SIZE;
			} else {
				off_reg = RKVPSS2X_CROP0_5_OFFS;
				size_reg = RKVPSS2X_CROP0_5_SIZE;
			}
			rkvpss_hw_write(hw, off_reg,
					RKVPSS2X_CROP_OFFS(cfg->output[i].crop_v_offs,
							 cfg->output[i].crop_h_offs));
			rkvpss_hw_write(hw, size_reg,
					RKVPSS2X_CROP_SIZE(cfg->output[i].crop_height,
							 cfg->output[i].crop_width));
			*crop_en |= RKVPSS_CROP_CHN_EN(i);
		}
	} else {
		//1126b todo
	}
}

static void crop_config(struct rkvpss_offline_dev *ofl,
			struct rkvpss_frame_cfg *cfg,
			bool unite, bool left)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	int i;
	u32 reg, val, crop_en;

	crop_en = 0;
	if (!unite) {
		for (i = 0; i < RKVPSS_OUTPUT_CH3; i++) {
			if (!cfg->output[i].enable)
				continue;
			reg = RKVPSS_CROP0_0_H_OFFS;
			val = cfg->output[i].crop_h_offs;
			rkvpss_hw_write(hw, reg + i * 0x10, val);
			reg = RKVPSS_CROP0_0_V_OFFS;
			val = cfg->output[i].crop_v_offs;
			rkvpss_hw_write(hw, reg + i * 0x10, val);
			reg = RKVPSS_CROP0_0_H_SIZE;
			val = cfg->output[i].crop_width;
			rkvpss_hw_write(hw, reg + i * 0x10, val);
			reg = RKVPSS_CROP0_0_V_SIZE;
			val = cfg->output[i].crop_height;
			rkvpss_hw_write(hw, reg + i * 0x10, val);
			crop_en |= RKVPSS_CROP_CHN_EN(i);
		}
	} else {
		if (left) {
			for (i = 0; i < RKVPSS_OUTPUT_CH3; i++) {
				if (!cfg->output[i].enable)
					continue;

				reg = RKVPSS_CROP0_0_H_OFFS;
				val = cfg->output[i].crop_h_offs;
				rkvpss_hw_write(hw, reg + i * 0x10, val);
				reg = RKVPSS_CROP0_0_V_OFFS;
				val = cfg->output[i].crop_v_offs;
				rkvpss_hw_write(hw, reg + i * 0x10, val);
				reg = RKVPSS_CROP0_0_H_SIZE;
				/*if no scale, left don't enlarge*/
				if (cfg->output[i].crop_width == cfg->output[i].scl_width)
					val = cfg->output[i].crop_width / 2;
				else
					val = cfg->output[i].crop_width / 2 + UNITE_LEFT_ENLARGE;
				rkvpss_hw_write(hw, reg + i * 0x10, val);
				reg = RKVPSS_CROP0_0_V_SIZE;
				val = cfg->output[i].crop_height;
				rkvpss_hw_write(hw, reg + i * 0x10, val);
				crop_en |= RKVPSS_CROP_CHN_EN(i);
			}
		} else {
			for (i = 0; i < RKVPSS_OUTPUT_CH3; i++) {
				if (!cfg->output[i].enable)
					continue;
				reg = RKVPSS_CROP0_0_H_OFFS;
				val = ofl->unite_params[i].quad_crop_w;
				rkvpss_hw_write(hw, reg + i * 0x10, val);
				reg = RKVPSS_CROP0_0_V_OFFS;
				val = cfg->output[i].crop_v_offs;
				rkvpss_hw_write(hw, reg + i * 0x10, val);
				reg = RKVPSS_CROP0_0_H_SIZE;
				val = cfg->output[i].crop_width / 2 +
				      ofl->unite_right_enlarge -
				      ofl->unite_params[i].quad_crop_w;
				rkvpss_hw_write(hw, reg + i * 0x10, val);
				reg = RKVPSS_CROP0_0_V_SIZE;
				val = cfg->output[i].crop_height;
				rkvpss_hw_write(hw, reg + i * 0x10, val);
				crop_en |= RKVPSS_CROP_CHN_EN(i);
			}
		}
	}

	crop_ch4_5(ofl, cfg, &crop_en, unite, left);

	rkvpss_hw_write(hw, RKVPSS_CROP0_CTRL, crop_en);
	rkvpss_hw_write(hw, RKVPSS_CROP0_UPDATE, RKVPSS_CROP_FORCE_UPD);

	for (i = 0; i < RKVPSS_OUTPUT_CH3; i++) {
		if (!cfg->output[i].enable)
			continue;
		v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
			 "%s unite:%d left:%d hw ch:%d h_offs:%d v_offs:%d width:%d height:%d\n",
			 __func__, unite, left, i,
			 rkvpss_hw_read(hw, RKVPSS_CROP0_0_H_OFFS + i * 0x10),
			 rkvpss_hw_read(hw, RKVPSS_CROP0_0_V_OFFS + i * 0x10),
			 rkvpss_hw_read(hw, RKVPSS_CROP0_0_H_SIZE + i * 0x10),
			 rkvpss_hw_read(hw, RKVPSS_CROP0_0_V_SIZE + i * 0x10));
	}
}

static int write_config(struct rkvpss_offline_dev *ofl,
			int file_id,
			struct rkvpss_frame_cfg *cfg,
			bool unite, bool left)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	const struct vb2_mem_ops *mem_ops = hw->mem_ops;
	struct sg_table  *sg_tbl;
	struct rkvpss_offline_buf *buf;
	struct rkvpss_output_ch out_ch[RKVPSS_OUT_V20_MAX] = { 0 };
	int i;
	u32 w, h, val, reg, mask, mi_update, flip_en, unite_off = 0,
		header_size = 0, payload_size = 0;
	bool ch_en = false, wr_uv_swap = false;

	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (!hw->is_ofl_ch[i] && cfg->output[i].enable) {
			v4l2_err(&ofl->v4l2_dev,
				 "dev_id:%d ch%d no select for offline mode, set to disable\n",
				 cfg->dev_id, i);
			cfg->output[i].enable = 0;
		}
		if (!cfg->output[i].enable)
			continue;
		ch_en = true;

		if (cfg->output[i].aspt.enable) {
			w = cfg->output[i].aspt.width;
			h = cfg->output[i].aspt.height;
		} else {
			w = cfg->output[i].scl_width;
			h = cfg->output[i].scl_height;
		}
		if (i == RKVPSS_OUTPUT_CH1) {
			bool is_fmt_find = true;

			switch (cfg->output[i].format) {
			case V4L2_PIX_FMT_RGB565:
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_RGB565 |
						  RKVPSS_MI_CHN_WR_RB_SWAP;
				break;
			case V4L2_PIX_FMT_RGB24:
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_RGB888 |
						  RKVPSS_MI_CHN_WR_RB_SWAP;
				break;
			case V4L2_PIX_FMT_RGB565X:
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_RGB565;
				break;
			case V4L2_PIX_FMT_BGR24:
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_RGB888;
				break;
			case V4L2_PIX_FMT_XBGR32:
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_ARGB888 |
						RKVPSS2X_CH1_WR_RGB888_ALPHA(cfg->output[i].alpha);
				break;
			case V4L2_PIX_FMT_XRGB32:
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_ARGB888 |
						RKVPSS_MI_CHN_WR_RB_SWAP |
						RKVPSS2X_CH1_WR_RGB888_ALPHA(cfg->output[i].alpha);
				break;
			default:
				is_fmt_find = false;
			}
			if (is_fmt_find) {
				out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_EN | RKVPSS_MI_CHN_WR_AUTO_UPD;
				out_ch[i].size = cfg->output[i].stride * h;
				continue;
			}
		}
		switch (cfg->output[i].format) {
		case V4L2_PIX_FMT_UYVY:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_422P | RKVPSS_MI_CHN_WR_OUTPUT_YUV422;
			out_ch[i].size = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_NV16:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_42XSP | RKVPSS_MI_CHN_WR_OUTPUT_YUV422;
			out_ch[i].size = cfg->output[i].stride * h * 2;
			out_ch[i].c_offs = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_NV12:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_42XSP | RKVPSS_MI_CHN_WR_OUTPUT_YUV420;
			out_ch[i].size = cfg->output[i].stride * h * 3 / 2;
			out_ch[i].c_offs = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_GREY:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_42XSP | RKVPSS_MI_CHN_WR_OUTPUT_YUV400;
			out_ch[i].size = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_VYUY:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_422P | RKVPSS_MI_CHN_WR_OUTPUT_YUV422;
			out_ch[i].size = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_NV61:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_42XSP | RKVPSS_MI_CHN_WR_OUTPUT_YUV422;
			out_ch[i].size = cfg->output[i].stride * h * 2;
			out_ch[i].c_offs = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_NV21:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_42XSP | RKVPSS_MI_CHN_WR_OUTPUT_YUV420;
			out_ch[i].size = cfg->output[i].stride * h * 3 / 2;
			out_ch[i].c_offs = cfg->output[i].stride * h;
			break;
		case V4L2_PIX_FMT_TILE420:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_YUV420;
			out_ch[i].size = cfg->output[i].stride * (h / 4);
			out_ch[i].c_offs = 0;
			break;
		case V4L2_PIX_FMT_TILE422:
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_YUV422;
			out_ch[i].size = cfg->output[i].stride * (h / 4);
			out_ch[i].c_offs = 0;
			break;
		case V4L2_PIX_FMT_FBC0:
			cfg->output[i].stride = 0;
			header_size = ((cfg->input.width + 63) / 64) *
				((cfg->input.height + 3) / 4) * 16;
			payload_size = ((cfg->input.width + 63) / 64) * 384 *
				((cfg->input.height + 3) / 4);
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_YUV420;
			out_ch[i].size = header_size + payload_size;
			break;
		case V4L2_PIX_FMT_FBC2:
			cfg->input.stride = 0;
			header_size = ((cfg->input.width + 63) / 64) *
				((cfg->input.height + 3) / 4) * 16;
			payload_size = ((cfg->input.width + 63) / 64) * 512 *
				((cfg->input.height + 3) / 4);
			out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_OUTPUT_YUV422;
			out_ch[i].size = header_size + payload_size;
			break;
		default:
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d no support output ch%d format:%c%c%c%c\n",
				 cfg->dev_id, i,
				 cfg->output[i].format, cfg->output[i].format >> 8,
				 cfg->output[i].format >> 16, cfg->output[i].format >> 24);
			return -EINVAL;
		}
		out_ch[i].ctrl |= RKVPSS_MI_CHN_WR_EN | RKVPSS_MI_CHN_WR_AUTO_UPD;

		/* for wrap recalculate size and c_offs*/
		if (cfg->output[i].wrap.enable) {
			out_ch[i].size = (out_ch[i].size / h) * cfg->output[i].wrap.wrap_line;
			if (out_ch[i].c_offs)
				out_ch[i].c_offs = (out_ch[i].c_offs / h) *
						   cfg->output[i].wrap.wrap_line;
		}
	}
	if (!ch_en) {
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d no output channel enable\n", cfg->dev_id);
		return -EINVAL;
	}

	mi_update = 0;
	flip_en = 0;
	mask = 0;
	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (hw->is_ofl_ch[i])
			mask |= RKVPSS_MI_CHN_V_FLIP(i);
		if (!cfg->output[i].enable)
			continue;
		buf = buf_add(ofl, cfg->output[i].dmabuf, file_id, cfg->dev_id,
			      cfg->output[i].buf_fd, out_ch[i].size);
		if (!buf)
			goto free_buf;

		if (unite && !left)
			unite_off = (ALIGN_DOWN(cfg->output[i].scl_width / 2, 16) * 8) / 8;

		if (cfg->output[i].aspt.enable)
			h = cfg->output[i].aspt.height;
		else
			h = cfg->output[i].scl_height;

		sg_tbl = mem_ops->cookie(&buf->vb, buf->mem);

		if (cfg->output[i].format == V4L2_PIX_FMT_FBC0 ||
		    cfg->output[i].format == V4L2_PIX_FMT_FBC2) {
			val = sg_dma_address(sg_tbl->sgl);

			reg = RKVPSS_MI_CHN0_WR_CB_BASE;
			rkvpss_hw_write(hw, reg + i * 0x100, val);

			header_size = ((cfg->input.width + 63) / 64) *
				((cfg->input.height + 3) / 4) * 16;
			val += header_size;
			reg = RKVPSS_MI_CHN0_WR_Y_BASE;
			rkvpss_hw_write(hw, reg + i * 0x100, val);

			cfg->output[i].stride = 0;
			reg = RKVPSS_MI_CHN0_WR_Y_STRIDE;
			val = cfg->output[i].stride;
			rkvpss_hw_write(hw, reg + i * 0x100, val);

			reg = RKVPSS_MI_CHN0_WR_Y_SIZE;
			if (cfg->output[i].format == V4L2_PIX_FMT_FBC0)
				payload_size = ((cfg->input.width + 63) / 64) * 384 *
					((cfg->input.height + 3) / 4);
			else if (cfg->output[i].format == V4L2_PIX_FMT_FBC2)
				payload_size = ((cfg->input.width + 63) / 64) * 512 *
					((cfg->input.height + 3) / 4);

			val = payload_size;
			rkvpss_hw_write(hw, reg + i * 0x100, val);

			reg = RKVPSS_MI_CHN0_WR_CB_SIZE;
			val = header_size;
			rkvpss_hw_write(hw, reg + i * 0x100, val);
		} else {
			val = sg_dma_address(sg_tbl->sgl) + unite_off;
			reg = RKVPSS_MI_CHN0_WR_Y_BASE;
			rkvpss_hw_write(hw, reg + i * 0x100, val);
			reg = RKVPSS_MI_CHN0_WR_CB_BASE;
			val += out_ch[i].c_offs;
			rkvpss_hw_write(hw, reg + i * 0x100, val);

			reg = RKVPSS_MI_CHN0_WR_Y_STRIDE;
			/* If 16-aligned, use stride; otherwise set to 0 */
			if (IS_ALIGNED(cfg->output[i].stride, 16))
				val = cfg->output[i].stride;
			else
				val = 0;
			rkvpss_hw_write(hw, reg + i * 0x100, val);

			reg = RKVPSS_MI_CHN0_WR_Y_SIZE;
			if (cfg->output[i].format == V4L2_PIX_FMT_TILE420 ||
				cfg->output[i].format == V4L2_PIX_FMT_TILE422)
				val = cfg->output[i].stride * (ALIGN(h, 4) / 4);
			else
				val = cfg->output[i].stride * h;

			/* for wrap recalutate y_size*/
			if (cfg->output[i].wrap.enable)
				val = (val / h) * cfg->output[i].wrap.wrap_line;

			rkvpss_hw_write(hw, reg + i * 0x100, val);
			reg = RKVPSS_MI_CHN0_WR_CB_SIZE;
			val = out_ch[i].size - val;
			rkvpss_hw_write(hw, reg + i * 0x100, val);
		}

		reg = RKVPSS_MI_CHN0_WR_CTRL;
		val = out_ch[i].ctrl;
		rkvpss_hw_write(hw, reg + i * 0x100, val);

		if (cfg->output[i].flip &&
		    !(cfg->output[i].format == V4L2_PIX_FMT_TILE420) &&
		    !(cfg->output[i].format == V4L2_PIX_FMT_TILE422)) {
			flip_en |= RKVPSS_MI_CHN_V_FLIP(i);

			switch (cfg->output[i].format) {
			case V4L2_PIX_FMT_RGB565:
			case V4L2_PIX_FMT_RGB565X:
				val = cfg->output[i].stride / 2;
				break;
			case V4L2_PIX_FMT_XBGR32:
			case V4L2_PIX_FMT_XRGB32:
				val = cfg->output[i].stride / 4;
				break;
			default:
				val = cfg->output[i].stride;
				break;
			}
			val = val * h;
			reg = RKVPSS_MI_CHN0_WR_Y_PIC_SIZE;
			rkvpss_hw_write(hw, reg + i * 0x100, val);
		}
		mi_update |= (RKVPSS_MI_CHN0_FORCE_UPD << i);

		v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
		 "%s unite:%d left:%d hw ch:%d y_size:%d y_stride:%d y_pic_size:%d y_base:0x%x",
		 __func__, unite, left, i,
		 rkvpss_hw_read(hw, RKVPSS_MI_CHN0_WR_Y_SIZE + i * 0x100),
		 rkvpss_hw_read(hw, RKVPSS_MI_CHN0_WR_Y_STRIDE + i * 0x100),
		 rkvpss_hw_read(hw, RKVPSS_MI_CHN0_WR_Y_PIC_SIZE + i * 0x100),
		 rkvpss_hw_read(hw, RKVPSS_MI_CHN0_WR_Y_BASE + i * 0x100));
	}


	rkvpss_hw_set_bits(hw, RKVPSS_MI_WR_VFLIP_CTRL, mask, flip_en);

	/* config output uv swap */
	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (cfg->output[i].enable &&
		    (cfg->output[i].format == V4L2_PIX_FMT_VYUY ||
		     cfg->output[i].format == V4L2_PIX_FMT_NV21 ||
		     cfg->output[i].format == V4L2_PIX_FMT_NV61))
			wr_uv_swap = true;
	}
	if (wr_uv_swap) {
		for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
			if (cfg->output[i].enable && (cfg->output[i].format == V4L2_PIX_FMT_UYVY ||
			    cfg->output[i].format == V4L2_PIX_FMT_NV12 ||
			    cfg->output[i].format == V4L2_PIX_FMT_NV16)) {
				v4l2_err(&ofl->v4l2_dev,
					 "dev_id:%d wr_uv_swap need to be consistent\n",
					 cfg->dev_id);
				return -EAGAIN;
			}
		}
	}
	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (cfg->output[i].format == V4L2_PIX_FMT_VYUY ||
		    cfg->output[i].format == V4L2_PIX_FMT_NV21 ||
		    cfg->output[i].format == V4L2_PIX_FMT_NV61) {
			mask = RKVPSS_MI_WR_UV_SWAP;
			val = RKVPSS_MI_WR_UV_SWAP;
			rkvpss_hw_set_bits(hw, RKVPSS_MI_WR_CTRL, mask, val);
			break;
		}
	}

	for (i = 0; i <= RKVPSS_OUTPUT_CH1; i++) {
		if (cfg->output[i].format == V4L2_PIX_FMT_TILE420 ||
		    cfg->output[i].format == V4L2_PIX_FMT_TILE422) {
			mask = RKVPSS_MI_WR_TILE_SEL(3);
			val = RKVPSS_MI_WR_TILE_SEL(i + 1);
			rkvpss_hw_set_bits(hw, RKVPSS_MI_WR_CTRL, mask, val);
		} else if (cfg->output[i].format == V4L2_PIX_FMT_FBC0 ||
			   cfg->output[i].format == V4L2_PIX_FMT_FBC2) {
			mask = RKVPSS2X_SW_MI_WR_FBCE_SEL(3);
			val = RKVPSS2X_SW_MI_WR_FBCE_SEL(i + 1);
			rkvpss_hw_set_bits(hw, RKVPSS2X_MI_WR_FBCE_CTRL, mask, val);

			if (cfg->output[i].aspt.enable) {
				w = cfg->output[i].aspt.width;
				h = cfg->output[i].aspt.height;
			} else {
				w = cfg->output[i].scl_width;
				h = cfg->output[i].scl_height;
			}
			rkvpss_hw_write(hw, RKVPSS2X_MI_WR_FBCE_SIZE,
					RKVPSS2X_SW_WR_FBCE_SIZE(w, h));
			rkvpss_hw_write(hw, RKVPSS2X_MI_WR_FBCE_OFFSET,
					(ALIGN(w, 64) / 64) * (ALIGN(h, 4) / 4) * 16);
		}
	}

	/* need update two for online2 mode */
	rkvpss_hw_write(hw, RKVPSS_MI_WR_INIT, mi_update);
	rkvpss_hw_write(hw, RKVPSS_MI_WR_INIT, mi_update);

	return 0;

free_buf:
	for (i -= 1; i >= 0; i--) {
		if (!cfg->output[i].enable)
			continue;
		buf_del(ofl, cfg->output[i].dmabuf, file_id, cfg->dev_id, cfg->output[i].buf_fd,
			false, true);
	}
	buf_del(ofl, cfg->input.dmabuf, file_id, cfg->dev_id, cfg->input.buf_fd,
		false, true);
	return -ENOMEM;
}

//1126b todo
static void calc_unite_scl_params(struct rkvpss_offline_dev *ofl,
				  struct rkvpss_frame_cfg *cfg)
{
	struct rkvpss_unite_scl_params *params;
	int i;
	u32 right_scl_need_size_y, right_scl_need_size_c;
	u32 left_in_used_size_y, left_in_used_size_c;
	u32 right_fst_position_y, right_fst_position_c;
	u32 right_y_crop_total;
	u32 right_c_crop_total;
	bool is_downscale_w, is_downscale_h, use_average;

	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (cfg->output[i].enable == 0)
			continue;
		params = &ofl->unite_params[i];

		is_downscale_w = cfg->output[i].scl_width <= cfg->output[i].crop_width;
		is_downscale_h = cfg->output[i].scl_height <= cfg->output[i].crop_height;
		use_average = is_downscale_w && is_downscale_h;

		if (use_average && (i == RKVPSS_OUTPUT_CH0 || i == RKVPSS_OUTPUT_CH2)) {
			params->y_w_fac = (cfg->output[i].scl_width - 1) * 65536 /
					  (cfg->output[i].crop_width - 1) + 1;
			params->c_w_fac = (cfg->output[i].scl_width / 2 - 1) * 65536 /
					  (cfg->output[i].crop_width / 2 - 1) + 1;
			params->y_h_fac = (cfg->output[i].scl_height - 1) * 65536 /
					  (cfg->output[i].crop_height - 1) + 1;
			params->c_h_fac = (cfg->output[i].scl_height - 1) * 65536 /
					  (cfg->output[i].crop_height - 1) + 1;

			right_fst_position_y = cfg->output[i].scl_width / 2 *
						   ((cfg->output[i].crop_width - 1) * 4096 / (cfg->output[i].scl_width - 1));
			right_fst_position_c = cfg->output[i].scl_width / 2 / 2 *
						   ((cfg->output[i].crop_width / 2 - 1) * 4096 / (cfg->output[i].scl_width / 2 - 1));
		} else {
			params->y_w_fac = (cfg->output[i].crop_width - 1) * 4096 /
					  (cfg->output[i].scl_width  - 1);
			params->c_w_fac = (cfg->output[i].crop_width / 2 - 1) * 4096 /
					  (cfg->output[i].scl_width / 2 - 1);
			params->y_h_fac = (cfg->output[i].crop_height - 1) * 4096 /
					  (cfg->output[i].scl_height - 1);
			params->c_h_fac = (cfg->output[i].crop_height - 1) * 4096 /
					  (cfg->output[i].scl_height - 1);

			right_fst_position_y = cfg->output[i].scl_width / 2 *
						   params->y_w_fac;
			right_fst_position_c = cfg->output[i].scl_width / 2 / 2 *
						   params->c_w_fac;
		}

		left_in_used_size_y = right_fst_position_y >> 12;
		left_in_used_size_c = (right_fst_position_c >> 12) * 2;

		params->y_w_phase = right_fst_position_y & 0xfff;
		params->c_w_phase = right_fst_position_c & 0xfff;

		right_scl_need_size_y = cfg->output[i].crop_width -
					left_in_used_size_y;
		params->right_scl_need_size_y = right_scl_need_size_y;
		right_scl_need_size_c = cfg->output[i].crop_width -
					left_in_used_size_c;
		params->right_scl_need_size_c = right_scl_need_size_c;

		if (i == 0 && cfg->output[i].crop_width != cfg->output[i].scl_width) {
			right_y_crop_total = cfg->output[i].crop_width / 2 +
					     ofl->unite_right_enlarge -
					     right_scl_need_size_y - 3;
			right_c_crop_total = cfg->output[i].crop_width / 2 +
					     ofl->unite_right_enlarge -
					     right_scl_need_size_c - 6;
		} else {
			right_y_crop_total = cfg->output[i].crop_width / 2 +
					     ofl->unite_right_enlarge -
					     right_scl_need_size_y;
			right_c_crop_total = cfg->output[i].crop_width / 2 +
					     ofl->unite_right_enlarge -
					     right_scl_need_size_c;
		}

		params->quad_crop_w = ALIGN_DOWN(min(right_y_crop_total, right_c_crop_total), 2);

		params->scl_in_crop_w_y = right_y_crop_total - params->quad_crop_w;
		params->scl_in_crop_w_c = right_c_crop_total - params->quad_crop_w;

		if (rkvpss_debug >= 4) {
			v4l2_info(&ofl->v4l2_dev,
				  "%s dev_id:%d seq:%d ch:%d y_w_fac:%u c_w_fac:%u y_h_fac:%u c_h_fac:%u\n",
				  __func__, cfg->dev_id, cfg->sequence, i, params->y_w_fac,
				  params->c_w_fac, params->y_h_fac, params->c_h_fac);
			v4l2_info(&ofl->v4l2_dev,
				  "\t\t\t\t\t\t unite_right_enlarge:%u",
				  ofl->unite_right_enlarge);
			v4l2_info(&ofl->v4l2_dev,
				  "\t\t\t\t\t\t y_w_phase:%u c_w_phase:%u quad_crop_w:%u scl_in_crop_w_y:%u scl_in_crop_w_c:%u\n",
				  params->y_w_phase, params->c_w_phase,
				  params->quad_crop_w,
				  params->scl_in_crop_w_y, params->scl_in_crop_w_c);
			v4l2_info(&ofl->v4l2_dev,
				  "\t\t\t\t\t\t right_scl_need_size_y:%u right_scl_need_size_c:%u\n",
				  params->right_scl_need_size_y,
				  params->right_scl_need_size_c);
		}
	}
}

static bool is_wrap(struct rkvpss_frame_cfg *cfg)
{
	bool is_wrap = false;
	int i;

	for (i = 0; i <= RKVPSS_OUTPUT_CH1; i++) {
		if (cfg->output[i].wrap.enable) {
			is_wrap = true;
			break;
		}
	}

	return is_wrap;
}

static int rkvpss_ofl_run(struct rkvpss_offline_dev *ofl,
			  int file_id,
			  struct rkvpss_frame_cfg *cfg,
			  bool unite, bool left)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	u32 update, val, mask;
	int ret, i;
	bool left_tmp;

	if (!unite || left)
		add_cfginfo(ofl, cfg);

	init_completion(&ofl->cmpl);
	ofl->mode_sel_en = false;

	ret = read_config(ofl, file_id, cfg, unite, left);
	if (ret < 0)
		return ret;

	if (unite && left)
		calc_unite_scl_params(ofl, cfg);

	if (unite && cfg->mirror)
		left_tmp = !left;
	else
		left_tmp = left;

	ret = cmsc_config(ofl, cfg, unite, left_tmp);
	if (ret)
		return ret;

	crop_config(ofl, cfg, unite, left_tmp);
	scale_config(ofl, cfg, unite, left_tmp);
	if (!unite)
		aspt_config(ofl, cfg);
	ret = write_config(ofl, file_id, cfg, unite, left_tmp);
	if (ret < 0)
		return ret;

	mask = 0;
	val = 0;
	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (!hw->is_ofl_ch[i])
			continue;
		mask |= (RKVPSS_ISP2VPSS_CHN0_SEL(3) << i * 2);
		if (cfg->output[i].enable)
			val |= (RKVPSS_ISP2VPSS_CHN0_SEL(1) << i * 2);
	}
	if (hw->is_ofl_cmsc)
		mask |= RKVPSS_ISP2VPSS_ONLINE2_CMSC_EN;
	val |= RKVPSS_ISP2VPSS_ONLINE2;
	mask |= RKVPSS_ISP2VPSS_ONLINE2;
	rkvpss_hw_set_bits(hw, RKVPSS_VPSS_ONLINE, mask, val);

	update = 0;
	mask = hw->is_ofl_cmsc ? RKVPSS_MIR_EN : 0;
	val = (mask && cfg->mirror) ? RKVPSS_MIR_EN : 0;
	if (mask)
		rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CTRL, mask, val);

	/* for wrap */
	for (i = 0; i <= RKVPSS_OUTPUT_CH1; i++) {
		if (!cfg->output[i].wrap.enable)
			continue;
		mask |= RKVPSS_VPSS2ENC_SEL | RKVPSS_VPSS2ENC_PIPE_EN |
			RKVPSS2X_VPSS2ENC_PATH_EN | RKVPSS2X_SENSOR_ID(7);
		val |= RKVPSS_VPSS2ENC_SEL | RKVPSS_VPSS2ENC_PIPE_EN |
		       RKVPSS2X_VPSS2ENC_PATH_EN | RKVPSS2X_SENSOR_ID(cfg->dev_id);
		if (i == 1) {
			mask |= RKVPSS_VPSS2ENC_CNT_SEL;
			val |= RKVPSS_VPSS2ENC_CNT_SEL;
		}
	}
	rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CTRL, mask, val);

	//1126b add RKVPSS_MIR_FORCE_UPD
	update |= RKVPSS_CHN_FORCE_UPD | RKVPSS_CFG_GEN_UPD;
	if (hw->is_ofl_cmsc)
		update |= RKVPSS_MIR_GEN_UPD | RKVPSS_MIR_FORCE_UPD;
	if (is_wrap(cfg))
		update |= RKVPSS_CFG_FORCE_UPD;

	rkvpss_hw_write(hw, RKVPSS_VPSS_UPDATE, update);
	rkvpss_hw_set_bits(hw, RKVPSS_VPSS_IMSC, 0, RKVPSS_ALL_FRM_END);

	if (rkvpss_debug == 6)
		rkvpss_dump_reg(ofl, cfg->sequence, 0x3f24);

	rkvpss_hw_write(hw, RKVPSS_MI_RD_START, RKVPSS_MI_RD_ST);

	if (is_wrap(cfg))
		rkvpss_ofl_dvbm_event(ROCKIT_DVBM_START, cfg->sequence);

	ret = wait_for_completion_timeout(&ofl->cmpl, msecs_to_jiffies(500));
	if (!ret) {
		v4l2_err(&ofl->v4l2_dev, "working timeout\n");
		ret = -EAGAIN;
	} else {
		ret = 0;
	}

	if (is_wrap(cfg))
		rkvpss_ofl_dvbm_event(ROCKIT_DVBM_END, cfg->sequence);

	return ret;
}

int rkvpss_module_get(struct rkvpss_offline_dev *ofl,
		      struct rkvpss_module_sel *get)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	int i, ret = 0;

	mutex_lock(&hw->dev_lock);
	if (hw->is_ofl_cmsc)
		get->mirror_cmsc_en = 1;
	else
		get->mirror_cmsc_en = 0;

	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (hw->is_ofl_ch[i])
			get->ch_en[i] = 1;
		else
			get->ch_en[i] = 0;
	}
	mutex_unlock(&hw->dev_lock);

	return ret;
}

int rkvpss_module_sel(struct rkvpss_offline_dev *ofl,
		      struct rkvpss_module_sel *sel)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	struct rkvpss_device *vpss;
	int i, ret = 0;

	mutex_lock(&hw->dev_lock);

	if (!ofl->mode_sel_en) {
		v4l2_err(&ofl->v4l2_dev, "already set module_sel\n");
		ret = -EINVAL;
		goto unlock;
	}

	for (i = 0; i < hw->dev_num; i++) {
		vpss = hw->vpss[i];
		if (vpss && (vpss->vpss_sdev.state & VPSS_START)) {
			v4l2_err(&ofl->v4l2_dev, "no support set mode when vpss working\n");
			ret = -EINVAL;
			goto unlock;
		}
	}

	hw->is_ofl_cmsc = !!sel->mirror_cmsc_en;
	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++)
		hw->is_ofl_ch[i] = !!sel->ch_en[i];
unlock:
	mutex_unlock(&hw->dev_lock);
	return ret;
}

int rkvpss_check_params(struct rkvpss_offline_dev *ofl,
			struct rkvpss_frame_cfg *cfg,
			bool *unite)
{
	int i, ret = 0, tile_num = 0;
	int scale_w_factor, scale_h_factor;
	int out_width, out_height;

	/* check dev id out of range */
	if (cfg->dev_id >= DEV_NUM_MAX) {
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d is out of range. range[0, %d]\n",
			 cfg->dev_id, DEV_NUM_MAX);
		ret = -EINVAL;
		goto end;
	}

	/* check input format */
	switch (cfg->input.format) {
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_RGBX32:
	case V4L2_PIX_FMT_BGRX32:
	case V4L2_PIX_FMT_FBC0:
	case V4L2_PIX_FMT_FBC2:
	case V4L2_PIX_FMT_FBC4:
	case V4L2_PIX_FMT_TILE420:
	case V4L2_PIX_FMT_TILE422:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
		break;
	default:
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d no support input format:%c%c%c%c\n",
			 cfg->dev_id, cfg->input.format, cfg->input.format >> 8,
			 cfg->input.format >> 16, cfg->input.format >> 24);
		ret = -EINVAL;
		goto end;
	}

	/* check input size */
	if (cfg->input.width > RKVPSS_UNITE_MAX_WIDTH ||
	    cfg->input.height > RKVPSS_UNITE_MAX_HEIGHT ||
	    cfg->input.width < RKVPSS_MIN_WIDTH ||
	    cfg->input.height < RKVPSS_MIN_HEIGHT) {
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d input size not support width:%d height:%d\n",
			 cfg->dev_id, cfg->input.width, cfg->input.height);
		ret = -EINVAL;
		goto end;
	}

	/* check input format alignment */
	if (cfg->input.format == V4L2_PIX_FMT_FBC0 ||
	    cfg->input.format == V4L2_PIX_FMT_FBC2 ||
	    cfg->input.format == V4L2_PIX_FMT_FBC4) {
		if (!IS_ALIGNED(cfg->input.width, 64)) {
			v4l2_err(&ofl->v4l2_dev,
				 "dev_id:%d fbc input width %d is not 64 aligned\n",
				 cfg->dev_id, cfg->input.width);
			ret = -EINVAL;
			goto end;
		}
		if (!IS_ALIGNED(cfg->input.height, 4)) {
			v4l2_err(&ofl->v4l2_dev,
				 "dev_id:%d fbc input height %d is not 4 aligned\n",
				 cfg->dev_id, cfg->input.height);
			ret = -EINVAL;
			goto end;
		}
	} else {
		if (!IS_ALIGNED(cfg->input.stride, 4)) {
			v4l2_err(&ofl->v4l2_dev,
				 "dev_id:%d input stride %d is not 4-byte aligned\n",
				 cfg->dev_id, cfg->input.stride);
			ret = -EINVAL;
			goto end;
		}
	}

	*unite = false;
	if (cfg->input.width > RKVPSS_MAX_WIDTH_V20) {
		*unite = true;
	} else {
		for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
			if (!cfg->output[i].enable)
				continue;
			if (cfg->output[i].scl_width > RKVPSS_MAX_WIDTH_V20) {
				*unite = true;
				break;
			}
		}
	}

	for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
		if (!cfg->output[i].enable)
			continue;
		/* check output format */
		switch (cfg->output[i].format) {
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_GREY:
		case V4L2_PIX_FMT_VYUY:
		case V4L2_PIX_FMT_NV61:
		case V4L2_PIX_FMT_NV21:
			break;
		case V4L2_PIX_FMT_TILE420:
		case V4L2_PIX_FMT_TILE422:
			if (i == RKVPSS_OUTPUT_CH0 || i == RKVPSS_OUTPUT_CH1) {
				tile_num++;
				if (tile_num > 1) {
					v4l2_err(&ofl->v4l2_dev, "dev_id:%d only ch0 or ch1 can tile write\n",
						 cfg->dev_id);
					ret = -EINVAL;
					goto end;
				}
				if (cfg->output[i].flip) {
					v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d tile write no support flip\n",
						 cfg->dev_id, i);
					ret = -EINVAL;
					goto end;
				}
			} else {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d no support output format:%c%c%c%c\n",
					 cfg->dev_id, i, cfg->output[i].format,
					 cfg->output[i].format >> 8,
					 cfg->output[i].format >> 16, cfg->output[i].format >> 24);
				ret = -EINVAL;
				goto end;
			}
			break;
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_RGB24:
		case V4L2_PIX_FMT_RGB565X:
		case V4L2_PIX_FMT_BGR24:
		case V4L2_PIX_FMT_XBGR32:
		case V4L2_PIX_FMT_XRGB32:
			if (i != RKVPSS_OUTPUT_CH1) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d no support output format:%c%c%c%c\n",
					 cfg->dev_id, i, cfg->output[i].format,
					 cfg->output[i].format >> 8,
					 cfg->output[i].format >> 16, cfg->output[i].format >> 24);
				ret = -EINVAL;
				goto end;
			}
			break;
		case V4L2_PIX_FMT_FBC0:
		case V4L2_PIX_FMT_FBC2:
			break;
		default:
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d no support output format:%c%c%c%c\n",
				 cfg->dev_id, i, cfg->output[i].format,
				 cfg->output[i].format >> 8,
				 cfg->output[i].format >> 16, cfg->output[i].format >> 24);
			ret = -EINVAL;
			goto end;
		}

		/* check output format alignment */
		if (cfg->output[i].format == V4L2_PIX_FMT_FBC0 ||
		    cfg->output[i].format == V4L2_PIX_FMT_FBC2 ||
		    cfg->output[i].format == V4L2_PIX_FMT_FBC4) {
			if (!IS_ALIGNED(cfg->input.width, 64)) {
				v4l2_err(&ofl->v4l2_dev,
					 "dev_id:%d ch:%d fbc output width %d is not 64 aligned\n",
					 cfg->dev_id, i, cfg->input.width);
				ret = -EINVAL;
				goto end;
			}
			if (!IS_ALIGNED(cfg->input.height, 4)) {
				v4l2_err(&ofl->v4l2_dev,
					 "dev_id:%d ch:%d fbc output height %d is not 4 aligned\n",
					 cfg->dev_id, i, cfg->input.height);
				ret = -EINVAL;
				goto end;
			}
		} else {
			if (!IS_ALIGNED(cfg->output[i].stride, 4)) {
				v4l2_err(&ofl->v4l2_dev,
					 "dev_id:%d ch:%d output stride %d is not 4-byte aligned\n",
					 cfg->dev_id, i, cfg->output[i].stride);
				ret = -EINVAL;
				goto end;
			}
		}

		/* check output size */
		if (cfg->output[i].aspt.enable) {
			out_width = cfg->output[i].aspt.width;
			out_height = cfg->output[i].aspt.height;
		} else {
			out_width = cfg->output[i].scl_width;
			out_height = cfg->output[i].scl_height;
		}
		if (out_width > RKVPSS_UNITE_MAX_WIDTH ||
			out_height > RKVPSS_UNITE_MAX_HEIGHT ||
			out_width < RKVPSS_MIN_WIDTH ||
			out_height < RKVPSS_MIN_HEIGHT) {
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d output size not support width:%d height:%d\n",
				 cfg->dev_id, i, out_width, out_height);
			ret = -EINVAL;
			goto end;
		}

		if ((out_width % 4) != 0) {
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d output width:%d must be 4-byte aligned\n",
				 cfg->dev_id, i, out_width);
			ret = -EINVAL;
			goto end;
		}

		/* check crop */
		cfg->output[i].crop_h_offs = ALIGN(cfg->output[i].crop_h_offs, 2);
		cfg->output[i].crop_v_offs = ALIGN(cfg->output[i].crop_v_offs, 2);
		cfg->output[i].crop_width = ALIGN(cfg->output[i].crop_width, 2);
		cfg->output[i].crop_height = ALIGN(cfg->output[i].crop_height, 2);
		if (cfg->output[i].crop_width == 0 || cfg->output[i].crop_height == 0)
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d crop output width:%d height:%d can't be zero\n",
				 cfg->dev_id, i,
				 cfg->output[i].crop_width, cfg->output[i].crop_height);
		if ((!cfg->input.rotate || cfg->input.rotate == 2)
		     && !cfg->input.rotate_90) {
			if (cfg->output[i].crop_width + cfg->output[i].crop_h_offs >
			    cfg->input.width) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d inval crop(offs:%d w:%d) input width:%d\n",
					 cfg->dev_id, i, cfg->output[i].crop_h_offs,
					 cfg->output[i].crop_width, cfg->input.width);
				ret = -EINVAL;
				goto end;
			}
			if (cfg->output[i].crop_height + cfg->output[i].crop_v_offs >
			    cfg->input.height) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d inval crop(offs:%d h:%d) input height:%d\n",
					 cfg->dev_id, i, cfg->output[i].crop_v_offs,
					 cfg->output[i].crop_height, cfg->input.height);
				ret = -EINVAL;
				goto end;
			}
		} else {
			if (cfg->output[i].crop_width + cfg->output[i].crop_h_offs >
			    cfg->input.height) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d rotate inval crop(offs:%d w:%d) input height:%d\n",
					 cfg->dev_id, i, cfg->output[i].crop_h_offs,
					 cfg->output[i].crop_width, cfg->input.height);
				ret = -EINVAL;
				goto end;
			}
			if (cfg->output[i].crop_height + cfg->output[i].crop_v_offs >
			    cfg->input.width) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d rotate inval crop(offs:%d h:%d) input width:%d\n",
					 cfg->dev_id, i, cfg->output[i].crop_v_offs,
					 cfg->output[i].crop_height, cfg->input.width);
				ret = -EINVAL;
				goto end;
			}
		}
		if (*unite) {
			if (cfg->output[i].crop_h_offs != (cfg->input.width -
							   (cfg->output[i].crop_h_offs +
							   cfg->output[i].crop_width))) {
				v4l2_err(&ofl->v4l2_dev, " dev_id:%d ch%d unite crop_v need centered crop(h_offs:%d w:%d) input width:%d\n",
					 cfg->dev_id, i, cfg->output[i].crop_h_offs,
					 cfg->output[i].crop_width, cfg->input.width);
				ret = -EINVAL;
				goto end;
			}
		}

		/* check scale */
		scale_w_factor = 32;
		scale_h_factor = 32;

		if (((cfg->output[i].scl_width >= cfg->output[i].crop_width) &&
		     (cfg->output[i].crop_width * scale_w_factor < cfg->output[i].scl_width)) ||
		    ((cfg->output[i].crop_width > cfg->output[i].scl_width) &&
		     (cfg->output[i].scl_width * scale_w_factor < cfg->output[i].crop_width)) ||
		    ((cfg->output[i].scl_height >= cfg->output[i].crop_height) &&
		     (cfg->output[i].crop_height * scale_h_factor < cfg->output[i].scl_height)) ||
		    ((cfg->output[i].crop_height > cfg->output[i].scl_height) &&
		     (cfg->output[i].scl_height * scale_h_factor < cfg->output[i].crop_height))) {
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d scale size error\n",
				 cfg->dev_id, i);
			ret = -EINVAL;
			goto end;
		}

		if (i >= RKVPSS_OUTPUT_CH2) {
			if (cfg->output[i].crop_width != cfg->output[i].scl_width &&
				cfg->output[i].crop_height != cfg->output[i].scl_height) {
				if ((!*unite && cfg->output[i].scl_width > 1920) ||
				    (*unite && cfg->output[i].scl_width > 1920 * 2)) {
					v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch%d single scale max width 1920\n",
						 cfg->dev_id, i);
					ret = -EINVAL;
					goto end;
				}
			}
		}
	}

	/* check rotate */
	switch (cfg->input.rotate) {
	case ROTATE_90:
	case ROTATE_180:
	case ROTATE_270:
		if (cfg->input.format != V4L2_PIX_FMT_TILE420 &&
		    cfg->input.format != V4L2_PIX_FMT_TILE422) {
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d input format:%c%c%c%c not support rotate\n",
				 cfg->dev_id, cfg->input.format,
				 cfg->input.format >> 8,
				 cfg->input.format >> 16, cfg->input.format >> 24);
			ret = -EINVAL;
			goto end;
		}
		break;
	default:
		break;
	}

	/* check wrap */
	if (((cfg->output[0].enable && cfg->output[0].wrap.enable) &&
		 (cfg->output[1].enable && cfg->output[1].wrap.enable)) ||
		(cfg->output[2].enable && cfg->output[2].wrap.enable) ||
		(cfg->output[2].enable && cfg->output[2].wrap.enable)) {
		v4l2_err(&ofl->v4l2_dev, "dev_id:%d only ch0 or ch1 support wrap\n",
					 cfg->dev_id);
		ret = -EINVAL;
		goto end;
	}
	for (i = 0; i <= RKVPSS_OUTPUT_CH1; i++) {
		struct rkvpss_output_cfg *output = &cfg->output[i];

		if (output->aspt.enable)
			out_height = output->aspt.height;
		else
			out_height = output->scl_height;
		if (output->enable && output->wrap.enable) {
			if (output->wrap.wrap_line <= 0 ||
			    output->wrap.wrap_line > out_height) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d wrap_line:%d error\n",
							 cfg->dev_id, i,
							 output->wrap.wrap_line);
				ret = -EINVAL;
				goto end;
			}
			if (output->flip) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d wrap no support flip\n",
							 cfg->dev_id, i);
				ret = -EINVAL;
				goto end;
			}
			switch (output->format) {
			case V4L2_PIX_FMT_TILE420:
			case V4L2_PIX_FMT_TILE422:
			case V4L2_PIX_FMT_RGB565:
			case V4L2_PIX_FMT_RGB24:
			case V4L2_PIX_FMT_RGB565X:
			case V4L2_PIX_FMT_BGR24:
			case V4L2_PIX_FMT_XBGR32:
			case V4L2_PIX_FMT_XRGB32:
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d ch:%d wrap no support output format:%c%c%c%c\n",
					 cfg->dev_id, i, output->format,
					 output->format >> 8,
					 output->format >> 16, output->format >> 24);
				ret = -EINVAL;
				break;
			default:
				break;
			}
		}
	}

	/** unite constraints **/
	if (*unite) {
		if (cfg->input.format == V4L2_PIX_FMT_FBC0 ||
		    cfg->input.format == V4L2_PIX_FMT_FBC2 ||
		    cfg->input.format == V4L2_PIX_FMT_FBC4 ||
		    cfg->input.format == V4L2_PIX_FMT_TILE420 ||
		    cfg->input.format == V4L2_PIX_FMT_TILE422) {
			v4l2_err(&ofl->v4l2_dev, "dev_id:%d unite no support input this format:%c%c%c%c\n",
					 cfg->dev_id, cfg->input.format,
					 cfg->input.format >> 8,
					 cfg->input.format >> 16, cfg->input.format >> 24);
			ret = -EINVAL;
			goto end;
		}
		for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
			if (!cfg->output[i].enable)
				continue;
			if (cfg->output[i].format != V4L2_PIX_FMT_NV12 &&
			    cfg->output[i].format != V4L2_PIX_FMT_NV16 &&
			    cfg->output[i].format != V4L2_PIX_FMT_NV21 &&
			    cfg->output[i].format != V4L2_PIX_FMT_NV61) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d unite no support output this format:%c%c%c%c\n",
					 cfg->dev_id, cfg->output[i].format,
					 cfg->output[i].format >> 8,
					 cfg->output[i].format >> 16, cfg->output[i].format >> 24);
				ret = -EINVAL;
				goto end;
			}

			if (cfg->output[i].aspt.enable) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d unite no support aspt\n",
					 cfg->dev_id);
				ret = -EINVAL;
				goto end;
			}
			if (cfg->output[i].wrap.enable) {
				v4l2_err(&ofl->v4l2_dev, "dev_id:%d unite no support wrap\n",
					 cfg->dev_id);
				ret = -EINVAL;
				goto end;
			}
		}
	}

end:
	return ret;
}

int rkvpss_prepare_run(struct rkvpss_offline_dev *ofl,
		       int file_id,
		       struct rkvpss_frame_cfg *cfg)
{
	int ret = 0;
	bool unite;
	int i, j, k;
	s64 us = 0;
	u64 ns;
	ktime_t t = 0;

	ns = ktime_get_ns();
	ofl->dev_rate[cfg->dev_id].in_rate = ns - ofl->dev_rate[cfg->dev_id].in_timestamp;
	ofl->dev_rate[cfg->dev_id].in_timestamp = ns;

	ret = rkvpss_check_params(ofl, cfg, &unite);
	if (ret < 0)
		goto end;

	/******** show cfg info ********/
	if (rkvpss_debug >= 2) {
		t = ktime_get();

		v4l2_info(&ofl->v4l2_dev,
			  "%s dev_id:%d seq:%d mirror:%d input:%dx%d [%dx%d] buffd:%d format:%c%c%c%c stride:%d rotate:%d\n",
			  __func__, cfg->dev_id, cfg->sequence, cfg->mirror,
			  cfg->input.width, cfg->input.height, cfg->input.stride, cfg->input.ver_stride, cfg->input.buf_fd,
			  cfg->input.format, cfg->input.format >> 8,
			  cfg->input.format >> 16, cfg->input.format >> 24,
			  cfg->input.stride, cfg->input.rotate);
		for (i = 0; i < RKVPSS_OUT_V20_MAX; i++) {
			v4l2_info(&ofl->v4l2_dev,
				  "\t\t\tch%d enable:%d crop:(%d,%d)/%dx%d scl:%dx%d flip:%d buffd:%d format:%c%c%c%c stride:%d\n",
				  i, cfg->output[i].enable,
				  cfg->output[i].crop_h_offs, cfg->output[i].crop_v_offs,
				  cfg->output[i].crop_width, cfg->output[i].crop_height,
				  cfg->output[i].scl_width, cfg->output[i].scl_height,
				  cfg->output[i].flip, cfg->output[i].buf_fd,
				  cfg->output[i].format, cfg->output[i].format >> 8,
				  cfg->output[i].format >> 16, cfg->output[i].format >> 24,
				  cfg->output[i].stride);
			if (rkvpss_debug < 4)
				break;
			if (!cfg->output[i].enable)
				continue;
			v4l2_info(&ofl->v4l2_dev,
				  "\t\t\tcmsc mosaic_block:%d width_ro:%d height_ro:%d\n",
				  cfg->output[i].cmsc.mosaic_block,
				  cfg->output[i].cmsc.width_ro,
				  cfg->output[i].cmsc.height_ro);
			for (j = 0; j < RKVPSS_CMSC_WIN_MAX; j++) {
				if (!cfg->output[i].cmsc.win[j].win_en)
					continue;
				v4l2_info(&ofl->v4l2_dev,
					  "\t\t\t\twin:%d win_en:%u mode:%u color_y:%u color_u:%u color_v:%u color_a:%u\n",
					  j,
					  cfg->output[i].cmsc.win[j].win_en,
					  cfg->output[i].cmsc.win[j].mode,
					  cfg->output[i].cmsc.win[j].cover_color_y,
					  cfg->output[i].cmsc.win[j].cover_color_u,
					  cfg->output[i].cmsc.win[j].cover_color_v,
					  cfg->output[i].cmsc.win[j].cover_color_a);
				for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
					v4l2_info(&ofl->v4l2_dev,
						  "\t\t\t\t\tpoint:%d x:%d y:%d\n",
						  k,
						  cfg->output[i].cmsc.win[j].point[k].x,
						  cfg->output[i].cmsc.win[j].point[k].y);
				}
			}
			v4l2_info(&ofl->v4l2_dev,
				  "\t\t\taspt_en:%d w:%d h:%d h_offs:%d v_offs:%d color_y:%d color_u:%d color_v:%d\n",
				  cfg->output[i].aspt.enable,
				  cfg->output[i].aspt.width,
				  cfg->output[i].aspt.height,
				  cfg->output[i].aspt.h_offs,
				  cfg->output[i].aspt.v_offs,
				  cfg->output[i].aspt.color_y,
				  cfg->output[i].aspt.color_u,
				  cfg->output[i].aspt.color_v);
		}
	}

	if (!unite) {
		ret = rkvpss_ofl_run(ofl, file_id, cfg, false, false);
		if (ret < 0)
			goto end;
	} else {
		ret = rkvpss_ofl_run(ofl, file_id, cfg, true, true);
		if (ret < 0) {
			v4l2_err(&ofl->v4l2_dev, "unite left error\n");
			goto end;
		}
		ret = rkvpss_ofl_run(ofl, file_id, cfg, true, false);
		if (ret < 0) {
			v4l2_err(&ofl->v4l2_dev, "unite right error\n");
			goto end;
		}
	}

	if (rkvpss_debug >= 2) {
		us = ktime_us_delta(ktime_get(), t);
		v4l2_info(&ofl->v4l2_dev,
			  "%s end, time:%lldus\n", __func__, us);
	}

	ns = ktime_get_ns();
	ofl->dev_rate[cfg->dev_id].out_rate = ns - ofl->dev_rate[cfg->dev_id].out_timestamp;
	ofl->dev_rate[cfg->dev_id].out_timestamp = ns;
	ofl->dev_rate[cfg->dev_id].sequence = cfg->sequence;
	ofl->dev_rate[cfg->dev_id].delay = ofl->dev_rate[cfg->dev_id].out_timestamp -
					   ofl->dev_rate[cfg->dev_id].in_timestamp;

end:
	return ret;
}

static int ofl_get_file_id(struct rkvpss_offline_dev *ofl,
			   struct file *file)
{
	void *idr_entity_temp;
	int file_id = 0;
	bool in_idr = false;
	int ret;

	mutex_lock(&ofl->idr_lock);
	idr_for_each_entry(&ofl->file_idr, idr_entity_temp, file_id) {
		if ((void *)file == idr_entity_temp) {
			in_idr = true;
			break;
		}
	}
	mutex_unlock(&ofl->idr_lock);

	if (in_idr)
		ret = file_id;
	else
		ret = 0;

	return ret;
}

static long rkvpss_ofl_wrap_dvbm_init(struct rkvpss_offline_dev *ofl,
		       int file_id, struct rkvpss_frame_cfg *cfg)
{
	const struct vb2_mem_ops *mem_ops = ofl->hw->mem_ops;
	struct dma_buf *dbuf;
	struct rkvpss_offline_buf *buf;
	struct sg_table  *sg_tbl;
	int width, height, wrap_line, i;
	u32 dma_addr;

	for (i = 0; i <= RKVPSS_OUTPUT_CH1; i++) {
		v4l2_dbg(4, rkvpss_debug, &ofl->v4l2_dev, "%s chn:%d enable:%d sequence:%d\n",
			 __func__, i, cfg->output[i].wrap.enable, cfg->sequence);

		if (!cfg->output[i].wrap.enable)
			continue;

		buf = buf_add(ofl, cfg->output[i].dmabuf, file_id, cfg->dev_id,
				  cfg->output[i].buf_fd, 0);
		if (!buf) {
			v4l2_err(&ofl->v4l2_dev, "get wrap_buf fail");
			return -EFAULT;
		}

		sg_tbl = mem_ops->cookie(&buf->vb, buf->mem);
		dma_addr = sg_dma_address(sg_tbl->sgl);

		dbuf = cfg->output[i].dmabuf;

		if (cfg->output[i].aspt.enable)
			height = cfg->output[i].aspt.height;
		else
			height = cfg->output[i].scl_height;

		width = cfg->output[i].stride;

		wrap_line = cfg->output[i].wrap.wrap_line;

		rkvpss_ofl_dvbm_init(ofl, dbuf, dma_addr, wrap_line, width, height, cfg->dev_id);

		v4l2_dbg(4, rkvpss_debug, &ofl->v4l2_dev, "%s file_id:%d dev_id:%d wrap_chn:%d\n",
			 __func__, file_id, cfg->dev_id, i);
	}

	return 0;
}

static void rkvpss_ofl_wrap_dvbm_deinit(struct rkvpss_offline_dev *ofl, int *id)
{
	rkvpss_ofl_dvbm_deinit(ofl, *id);
}

static void rkvpss_ofl_get_wrap_seq(struct rkvpss_offline_dev *ofl, int *seq)
{
	struct rkvpss_hw_dev *hw = ofl->hw;
	u32 mask, val;

	v4l2_dbg(4, rkvpss_debug, &ofl->v4l2_dev, "dev_id:%d\n", *seq);

	mask = RKVPSS_VPSS2ENC_SEL | RKVPSS2X_SENSOR_ID(7);
	val = RKVPSS_VPSS2ENC_SEL | RKVPSS2X_SENSOR_ID(*seq);
	rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CTRL, mask, val);

	if (hw->is_first) {
		hw->is_first = false;
		rkvpss_hw_write(hw, RKVPSS2X_VPSS2ENC_DEBUG, 0x1);
		v4l2_dbg(4, rkvpss_debug, &ofl->v4l2_dev, "wrap is first\n");
	}

	*seq = RKVPSS2X_RO_VPSS2ENC_FRM_CNT(rkvpss_hw_read(ofl->hw, RKVPSS2X_VPSS2ENC_DEBUG));

	v4l2_dbg(4, rkvpss_debug, &ofl->v4l2_dev, "hw frmame coount:%d\n", *seq);
}

long rkvpss_ofl_action(struct rkvpss_offline_dev *ofl,
		       int file_id, unsigned int cmd,  void *arg)
{
	long ret = 0;
	bool unite;

	v4l2_dbg(4, rkvpss_debug, &ofl->v4l2_dev, "%s cmd:%d", __func__, cmd);
	mutex_lock(&ofl->handle_lock);
	ofl->pm_need_wait = true;

	switch (cmd) {
	case RKVPSS_CMD_MODULE_SEL:
		ret = rkvpss_module_sel(ofl, arg);
		break;
	case RKVPSS_CMD_MODULE_GET:
		ret = rkvpss_module_get(ofl, arg);
		break;
	case RKVPSS_CMD_FRAME_HANDLE:
		ret = rkvpss_prepare_run(ofl, file_id, arg);
		break;
	case RKVPSS_CMD_BUF_ADD:
		ret = rkvpss_ofl_buf_add(ofl, file_id, arg);
		break;
	case RKVPSS_CMD_BUF_DEL:
		rkvpss_ofl_buf_del(ofl, file_id, arg);
		break;
	case RKVPSS_CMD_BUF_DEL_BY_FILE:
		rkvpss_ofl_buf_del_by_file(ofl, file_id);
		break;
	case RKVPSS_CMD_CHECKPARAMS:
		ret = rkvpss_check_params(ofl, arg, &unite);
		break;
	case RKVPSS_CMD_WRAP_DVBM_INIT:
		ret = rkvpss_ofl_wrap_dvbm_init(ofl, file_id, arg);
		break;
	case RKVPSS_CMD_WRAP_DVBM_DEINIT:
		rkvpss_ofl_wrap_dvbm_deinit(ofl, arg);
		break;
	case RKVPSS_CMD_GET_WRAP_SEQ:
		rkvpss_ofl_get_wrap_seq(ofl, arg);
		break;
	default:
		ret = -EFAULT;
	}

	ofl->pm_need_wait = false;
	mutex_unlock(&ofl->handle_lock);

	return ret;
}


static long rkvpss_ofl_ioctl(struct file *file, void *fh,
			     bool valid_prio,
			     unsigned int cmd, void *arg)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	long ret = 0;
	int file_id = 0;

	switch (cmd) {
	case RKVPSS_CMD_BUF_DEL_BY_FILE:
		break;
	default:
		if (!arg) {
			ret = -EINVAL;
			goto out;
		}
		break;
	}

	file_id = ofl_get_file_id(ofl, file);
	if (file_id <= 0) {
		ret = -EINVAL;
		goto out;
	}

	ret = rkvpss_ofl_action(ofl, file_id, cmd, arg);

out:
	/* notify hw suspend */
	if (ofl->hw->is_suspend)
		complete(&ofl->pm_cmpl);

	return ret;
}


static const struct v4l2_ioctl_ops offline_ioctl_ops = {
	.vidioc_default = rkvpss_ofl_ioctl,
};

int rkvpss_ofl_add_file_id(struct rkvpss_offline_dev *ofl, void *idr_entity)
{
	void *idr_entity_temp;
	int idr_id = 0, ret = 0;
	bool in_idr = false;

	mutex_lock(&ofl->idr_lock);

	idr_for_each_entry(&ofl->file_idr, idr_entity_temp, idr_id) {
		if (idr_entity == idr_entity_temp) {
			in_idr = true;
			break;
		}
	}
	if (!in_idr)
		ret = idr_alloc(&ofl->file_idr, idr_entity, 1, 0, GFP_KERNEL);

	mutex_unlock(&ofl->idr_lock);

	return ret;
}

void *rkvpss_ofl_del_file_id(struct rkvpss_offline_dev *ofl, struct file *file)
{
	void *ret = NULL;
	int idr_id = 0;

	idr_id = ofl_get_file_id(ofl, file);

	mutex_lock(&ofl->idr_lock);

	if (idr_id)
		ret = idr_remove(&ofl->file_idr, idr_id);

	mutex_unlock(&ofl->idr_lock);

	return ret;
}

static int ofl_open(struct file *file)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	int ret;

	if (!ofl || !ofl->hw || !ofl->hw->is_probe_end) {
		ret = -ENODEV;
		goto end;
	}

	ret = v4l2_fh_open(file);
	if (ret)
		goto end;

	mutex_lock(&ofl->hw->dev_lock);
	ret = pm_runtime_get_sync(ofl->hw->dev);
	mutex_unlock(&ofl->hw->dev_lock);

	if (ret < 0) {
		v4l2_fh_release(file);
		goto end;
	}

	ret = rkvpss_ofl_add_file_id(ofl, (void *)file);
end:
	if (ofl) {
		v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
			 "%s file:%p ret:%d\n", __func__, file, ret);
	}
	return (ret > 0) ? 0 : ret;
}

static int ofl_release(struct file *file)
{
	struct rkvpss_offline_dev *ofl = video_drvdata(file);
	int ret = 0;
	int file_id = 0;

	v4l2_dbg(1, rkvpss_debug, &ofl->v4l2_dev,
		 "%s file:%p\n", __func__, file);

	v4l2_fh_release(file);
	file_id = ofl_get_file_id(ofl, file);
	if (file_id)
		buf_del(ofl, NULL, file_id, 0, 0, true, false);

	mutex_lock(&ofl->hw->dev_lock);
	pm_runtime_put_sync(ofl->hw->dev);
	mutex_unlock(&ofl->hw->dev_lock);

	if (rkvpss_ofl_del_file_id(ofl, file) != (void *)file)
		ret = -EINVAL;

	return ret;
}

static const struct v4l2_file_operations offline_fops = {
	.owner = THIS_MODULE,
	.open = ofl_open,
	.release = ofl_release,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

static const struct video_device offline_videodev = {
	.name = "rkvpss-offline",
	.vfl_dir = VFL_DIR_RX,
	.fops = &offline_fops,
	.ioctl_ops = &offline_ioctl_ops,
	.minor = -1,
	.release = video_device_release_empty,
};

void rkvpss_offline_irq_v20(struct rkvpss_hw_dev *hw, u32 irq)
{
	struct rkvpss_offline_dev *ofl = &hw->ofl_dev;

	v4l2_dbg(3, rkvpss_debug, &ofl->v4l2_dev,
		 "%s 0x%x\n", __func__, irq);

	if (!completion_done(&ofl->cmpl))
		complete(&ofl->cmpl);
}

int rkvpss_register_offline_v20(struct rkvpss_hw_dev *hw)
{
	struct rkvpss_offline_dev *ofl = &hw->ofl_dev;
	struct v4l2_device *v4l2_dev;
	struct video_device *vfd;
	int ret;

	ofl->hw = hw;
	v4l2_dev = &ofl->v4l2_dev;
	strscpy(v4l2_dev->name, offline_videodev.name, sizeof(v4l2_dev->name));
	ret = v4l2_device_register(hw->dev, v4l2_dev);
	if (ret)
		return ret;

	mutex_init(&ofl->apilock);
	ofl->vfd = offline_videodev;
	ofl->mode_sel_en = true;
	vfd = &ofl->vfd;
	vfd->device_caps = V4L2_CAP_STREAMING;
	vfd->v4l2_dev = v4l2_dev;
	ret = video_register_device(vfd, VFL_TYPE_VIDEO, 0);
	if (ret) {
		v4l2_err(v4l2_dev, "Failed to register video device\n");
		goto unreg_v4l2;
	}
	video_set_drvdata(vfd, ofl);
	INIT_LIST_HEAD(&ofl->list);
	INIT_LIST_HEAD(&ofl->cfginfo_list);
	mutex_init(&ofl->ofl_lock);
	mutex_init(&ofl->idr_lock);
	mutex_init(&ofl->handle_lock);
	idr_init(&ofl->file_idr);
	rkvpss_offline_proc_init(ofl);
	ofl->pm_need_wait = false;
	init_completion(&ofl->pm_cmpl);
	rkvpss_ofl_rockit_init(ofl);
	rkvpss_ofl_dvbm_get(ofl);
	return 0;
unreg_v4l2:
	mutex_destroy(&ofl->apilock);
	v4l2_device_unregister(v4l2_dev);
	return ret;
}

void rkvpss_unregister_offline_v20(struct rkvpss_hw_dev *hw)
{
	mutex_destroy(&hw->ofl_dev.apilock);
	video_unregister_device(&hw->ofl_dev.vfd);
	v4l2_device_unregister(&hw->ofl_dev.v4l2_dev);
	mutex_destroy(&hw->ofl_dev.ofl_lock);
	mutex_destroy(&hw->ofl_dev.idr_lock);
	mutex_destroy(&hw->ofl_dev.handle_lock);
	idr_destroy(&hw->ofl_dev.file_idr);
	rkvpss_offline_proc_cleanup(&hw->ofl_dev);
}

