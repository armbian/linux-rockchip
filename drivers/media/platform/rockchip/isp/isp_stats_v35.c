// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include <linux/delay.h>
#include <linux/kfifo.h>
#include <linux/rk-isp32-config.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include "dev.h"
#include "regs.h"
#include "common.h"
#include "isp_stats.h"
#include "isp_stats_v35.h"
#include "isp_params_v35.h"

#define ISP35_3A_MEAS_DONE BIT(31)

static void isp3_module_done(struct rkisp_isp_stats_vdev *stats_vdev, u32 reg, u32 value)
{
	void __iomem *base = stats_vdev->dev->hw_dev->base_addr;

	writel(value, base + reg);
}

static u32 isp3_stats_read(struct rkisp_isp_stats_vdev *stats_vdev, u32 addr)
{
	return rkisp_read(stats_vdev->dev, addr, true);
}

static void isp3_stats_write(struct rkisp_isp_stats_vdev *stats_vdev,
			     u32 addr, u32 value)
{
	rkisp_write(stats_vdev->dev, addr, value, true);
}

static int
rkisp_stats_get_sharp_stats(struct rkisp_isp_stats_vdev *stats_vdev,
			    struct rkisp35_stat_buffer *pbuf)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_isp_params_vdev *params = &dev->params_vdev;
	struct isp35_isp_params_cfg *params_rec = params->isp35_params + dev->unite_index;
	struct isp35_sharp_cfg *sharp_arg_rec = &params_rec->others.sharp_cfg;
	struct isp33_gic_cfg *gic_arg_rec = &params_rec->others.gic_cfg;
	struct isp33_sharp_stat *sharp_stat = NULL;
	u16 noise_curve[ISP35_SHARP_NOISE_CURVE_NUM];
	u32 i, val, size = sizeof(noise_curve);
	bool is_sharp_curve_mode, is_gic_curve_mode;

	if (pbuf)
		sharp_stat = &pbuf->stat.sharp;

	val = isp3_stats_read(stats_vdev, ISP3X_SHARP_EN);
	if (val & 0x1) {
		is_sharp_curve_mode = !!(val & BIT(8));
		val = isp3_stats_read(stats_vdev, ISP3X_GIC_CONTROL);
		is_gic_curve_mode = (!(val & 1) || !!(val & BIT(3)));
		/* noise_curve_ext noise_curve and bfflt_vsigma_y are of the same size */
		for (i = 0; i < ISP35_SHARP_NOISE_CURVE_NUM / 2; i++) {
			val = isp3_stats_read(stats_vdev, ISP33_SHARP_NOISE_CURVE0 + i * 4);
			noise_curve[i * 2] = val & 0x7ff;
			noise_curve[i * 2 + 1] = (val >> 16) & 0x7ff;
		}
		val = isp3_stats_read(stats_vdev, ISP33_SHARP_NOISE_CURVE8);
		noise_curve[i * 2] = val & 0x7ff;
		if (sharp_stat) {
			pbuf->meas_type |= ISP35_STAT_SHARP;
			memcpy(sharp_stat->noise_curve, noise_curve, size);
		}
		/* save hardware curve for next frame config if resume or multi-sensor */
		if (!is_sharp_curve_mode)
			memcpy(sharp_arg_rec->noise_curve_ext, noise_curve, size);
		if (!is_gic_curve_mode)
			memcpy(gic_arg_rec->bfflt_vsigma_y, noise_curve, size);
	}
	return 0;
}

static int
rkisp_stats_get_bay3d_stats(struct rkisp_isp_stats_vdev *stats_vdev,
			    struct rkisp35_stat_buffer *pbuf)
{
	struct rkisp_isp_params_val_v35 *priv = stats_vdev->dev->params_vdev.priv_val;
	struct isp33_bay3d_stat *bay3d;
	u32 i, val;

	if (!pbuf)
		return 0;
	val = isp3_stats_read(stats_vdev, ISP33_BAY3D_CTRL0);
	if (val & 0x1) {
		bay3d = &pbuf->stat.bay3d;
		val = isp3_stats_read(stats_vdev, ISP33_BAY3D_TNRSUM);
		bay3d->sigma_num = val;
		for (i = 0; i < ISP35_BAY3D_TNRSIG_NUM / 2; i++) {
			val = isp3_stats_read(stats_vdev, ISP33_BAY3D_TNRYO0 + i * 4);
			bay3d->sigma_y[i * 2] = val & 0xfff;
			bay3d->sigma_y[i * 2 + 1] = (val >> 16) & 0xfff;
		}
		pbuf->meas_type |= ISP35_STAT_BAY3D;
		pbuf->stat.buf_bay3d_iir_index = priv->bay3d_iir_cur_idx;
		pbuf->stat.buf_bay3d_ds_index = priv->bay3d_ds_cur_idx;
		pbuf->stat.buf_bay3d_wgt_index = priv->bay3d_wgt_cur_idx;
		pbuf->stat.buf_gain_index = priv->gain_cur_idx;
		pbuf->stat.buf_aipre_gain_index = priv->aipre_gain_cur_idx;
		pbuf->stat.buf_vpsl_index = priv->vpsl_cur_idx;
	}
	return 0;
}

static int
rkisp_stats_get_hist_stats(struct rkisp_isp_stats_vdev *stats_vdev,
			   struct rkisp35_stat_buffer *pbuf)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_isp_params_vdev *params = &dev->params_vdev;
	struct rkisp_isp_params_val_v35 *priv_val = params->priv_val;
	struct isp35_isp_params_cfg *params_rec = params->isp35_params + dev->unite_index;
	struct isp33_hist_cfg *arg_rec = &params_rec->others.hist_cfg;
	struct isp33_hist_stat *hist;
	int val, i, j, timeout;

	val = isp3_stats_read(stats_vdev, ISP33_HIST_CTRL);
	if (val & 0x1) {
		val = isp3_stats_read(stats_vdev, ISP33_HIST_STAB);
		arg_rec->stab_frame_cnt0 = val & 0xf;
		arg_rec->stab_frame_cnt1 = (val & 0xf0) >> 4;
		for (i = 0; i < priv_val->hist_blk_num; i++) {
			val = ISP33_IIR_RD_ID(i) | ISP33_IIR_RD_P;
			isp3_stats_write(stats_vdev, ISP33_HIST_RW, val);
			timeout = 5;
			while (timeout--) {
				val = isp3_stats_read(stats_vdev, ISP33_HIST_RW);
				if (val & ISP33_IIR_RDATA_VAL)
					break;
				udelay(2);
			}
			if (timeout < 0) {
				v4l2_warn(&dev->v4l2_dev, "%s hist read:%d timeout\n", __func__, i);
				return 0;
			}
			for (j = 0; j < ISP35_HIST_IIR_NUM / 2; j++) {
				val = isp3_stats_read(stats_vdev, ISP33_HIST_IIR0 + 4 * j);
				arg_rec->iir[i][2 * j] = val & 0x3FF;
				arg_rec->iir[i][2 * j + 1] = val >> 16;
			}
		}
		if (dev->is_frm_rd)
			arg_rec->iir_wr = true;
		if (pbuf) {
			hist = &pbuf->stat.hist;
			memcpy(hist->iir, arg_rec->iir, sizeof(hist->iir));
			pbuf->meas_type |= ISP35_STAT_HIST;
		}
	}
	return 0;
}

static int
rkisp_stats_get_enh_stats(struct rkisp_isp_stats_vdev *stats_vdev,
			  struct rkisp35_stat_buffer *pbuf)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_isp_params_vdev *params = &dev->params_vdev;
	struct rkisp_isp_params_val_v35 *priv_val = params->priv_val;
	struct isp35_isp_params_cfg *params_rec = params->isp35_params + dev->unite_index;
	struct isp35_enh_cfg *arg_rec = &params_rec->others.enh_cfg;
	struct isp35_enh_stat *enh;
	int val, i, j, timeout;

	val = isp3_stats_read(stats_vdev, ISP33_ENH_CTRL);
	if (val & 0x1) {
		val = isp3_stats_read(stats_vdev, ISP33_ENH_PRE_FRAME);
		arg_rec->pre_wet_frame_cnt0 = val & 0xf;
		arg_rec->pre_wet_frame_cnt1 = (val & 0xf0) >> 4;
		for (i = 0; i < priv_val->enh_row; i++) {
			val = ISP33_IIR_RD_ID(i) | ISP33_IIR_RD_P;
			isp3_stats_write(stats_vdev, ISP33_ENH_IIR_RW, val);
			timeout = 5;
			while (timeout--) {
				val = isp3_stats_read(stats_vdev, ISP33_ENH_IIR_RW);
				if (val & ISP33_IIR_RDATA_VAL)
					break;
				udelay(2);
			}
			if (timeout < 0) {
				v4l2_warn(&dev->v4l2_dev, "%s enh read:%d timeout\n", __func__, i);
				return 0;
			}
			for (j = 0; j < priv_val->enh_col / 4; j++) {
				val = isp3_stats_read(stats_vdev, ISP33_ENH_IIR0 + 4 * j);
				arg_rec->iir[i][4 * j] = val & 0xFF;
				arg_rec->iir[i][4 * j + 1] = (val & 0xff00) >> 8;
				arg_rec->iir[i][4 * j + 2] = (val & 0xff0000) >> 16;
				arg_rec->iir[i][4 * j + 3] = (val & 0xff000000) >> 24;
			}
		}
		if (dev->is_frm_rd)
			arg_rec->iir_wr = true;
		if (pbuf) {
			enh = &pbuf->stat.enh;
			memcpy(enh->iir, arg_rec->iir, sizeof(enh->iir));
			pbuf->meas_type |= ISP35_STAT_ENH;
		}
	}
	return 0;
}

static int
rkisp_stats_update_buf(struct rkisp_isp_stats_vdev *stats_vdev)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_buffer *buf;
	unsigned long flags = 0;
	u32 size = stats_vdev->vdev_fmt.fmt.meta.buffersize / dev->unite_div;
	u32 val, addr = 0, offset = 0;
	int i, ret = 0;

	if (!dev->is_aiisp_en) {
		spin_lock_irqsave(&stats_vdev->rd_lock, flags);
		if (!stats_vdev->nxt_buf && !list_empty(&stats_vdev->stat)) {
			buf = list_first_entry(&stats_vdev->stat,
					       struct rkisp_buffer, queue);
			list_del(&buf->queue);
			stats_vdev->nxt_buf = buf;
		}
		spin_unlock_irqrestore(&stats_vdev->rd_lock, flags);
	}

	if (stats_vdev->nxt_buf) {
		addr = stats_vdev->nxt_buf->buff_addr[0];
		if (!dev->hw_dev->is_single) {
			stats_vdev->cur_buf = stats_vdev->nxt_buf;
			stats_vdev->nxt_buf = NULL;
		}
	} else if (stats_vdev->stats_buf[0].mem_priv) {
		addr = stats_vdev->stats_buf[0].dma_addr;
	} else {
		ret = -EINVAL;
	}

	if (ret != -EINVAL) {
		for (i = 0; i < dev->unite_div; i++) {
			val = addr + i * size;

			rkisp_idx_write(dev, ISP39_W3A_AEBIG_ADDR, val, i, false);

			offset = sizeof(struct isp33_rawae_stat) +
				 sizeof(struct isp33_rawhist_stat);
			val += offset;
			rkisp_idx_write(dev, ISP39_W3A_AE0_ADDR, val, i, false);

			val += offset;
			rkisp_idx_write(dev, ISP39_W3A_AF_ADDR, val, i, false);

			offset = sizeof(struct isp39_rawaf_stat);
			val += offset;
			rkisp_idx_write(dev, ISP39_W3A_AWB_ADDR, val, i, false);
		}
		v4l2_dbg(4, rkisp_debug, &dev->v4l2_dev,
			 "%s BASE:0x%x SHD AEBIG:0x%x AE0:0x%x AF:0x%x AWB:0x%x\n",
			 __func__, addr,
			 isp3_stats_read(stats_vdev, ISP39_W3A_AEBIG_ADDR_SHD),
			 isp3_stats_read(stats_vdev, ISP39_W3A_AE0_ADDR_SHD),
			 isp3_stats_read(stats_vdev, ISP39_W3A_AF_ADDR_SHD),
			 isp3_stats_read(stats_vdev, ISP39_W3A_AWB_ADDR_SHD));
	}
	return ret;
}

static void
rkisp_stats_get_aiawb_stats(struct rkisp_isp_stats_vdev *stats_vdev,
		  struct rkisp35_stat_buffer *pbuf)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_isp_params_vdev *params_vdev = &dev->params_vdev;
	struct rkisp_isp_params_val_v35 *priv_val = params_vdev->priv_val;
	u32 ctrl = rkisp_read(dev, ISP35_AIAWB_CTRL0, false);
	u32 buf_idx, val;

	if (!pbuf || !(ctrl & ISP35_AIAWB_EN) || !priv_val->buf_aiawb_cnt)
		return;
	pbuf->meas_type |= ISP35_STAT_AIAWB;
	buf_idx = priv_val->buf_aiawb_idx;
	pbuf->stat.buf_aiawb_index = priv_val->buf_aiawb[buf_idx].index;
	buf_idx = (buf_idx + 1) % priv_val->buf_aiawb_cnt;
	val = priv_val->buf_aiawb[buf_idx].dma_addr;
	rkisp_write(dev, ISP35_AIAWB_WR_BASE, val, false);
	rkisp_write(dev, ISP35_AIAWB_CTRL0, ctrl | ISP35_AIAWB_SELF_UPD, false);
	priv_val->buf_aiawb_idx = buf_idx;
	v4l2_dbg(4, rkisp_debug, &stats_vdev->dev->v4l2_dev,
		 "aiawb idx:%d next(id:%d 0x%x)\n",
		 pbuf->stat.buf_aiawb_index, buf_idx,
		 isp3_stats_read(stats_vdev, ISP35_AIAWB_WR_BASE_SHD));
}

static void
rkisp_stats_get_awbsync_stats(struct rkisp_isp_stats_vdev *stats_vdev,
		  struct rkisp35_stat_buffer *pbuf)
{
	struct isp35_awbsync_stat *awbsync;
	u32 ctrl = isp3_stats_read(stats_vdev, ISP35_AWBSYNC_CTRL);
	u64 msb, lsb;
	int i;

	if (!(ctrl & ISP35_3A_MEAS_DONE)) {
		v4l2_dbg(1, rkisp_debug, &stats_vdev->dev->v4l2_dev,
			 "%s fail, ctrl:0x%x\n", __func__, ctrl);
		return;
	}
	if (!pbuf)
		goto out;
	awbsync = &pbuf->stat.awbsync;
	for (i = 0; i < ISP35_AWBSYNC_WIN_MAX; i++) {
		msb = isp3_stats_read(stats_vdev, ISP35_AWBSYNC_WIN0_SUMP + i * 0x10);
		awbsync->sump[i] = msb & 0x3ffffff;
		lsb = isp3_stats_read(stats_vdev, ISP35_AWBSYNC_WIN0_SUMR + i * 0x10);
		awbsync->sumr[i] = lsb | ((msb & 0xc0000000) << 2);
		lsb = isp3_stats_read(stats_vdev, ISP35_AWBSYNC_WIN0_SUMG + i * 0x10);
		awbsync->sumg[i] = lsb | ((msb & 0x30000000) << 4);
		lsb = isp3_stats_read(stats_vdev, ISP35_AWBSYNC_WIN0_SUMB + i * 0x10);
		awbsync->sumg[i] = lsb | ((msb & 0xc000000) << 6);
	}
	pbuf->meas_type |= ISP35_STAT_AWBSYNC;
out:
	isp3_module_done(stats_vdev, ISP35_AWBSYNC_CTRL, ctrl);
}

static void
rkisp_stats_info2ddr(struct rkisp_isp_stats_vdev *stats_vdev,
		     struct rkisp35_stat_buffer *pbuf)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_isp_params_val_v35 *priv_val;
	struct rkisp_dummy_buffer *buf;
	int idx, buf_fd = -1;
	u32 reg = 0, ctrl, mask;

	if (dev->is_aiisp_en)
		return;

	priv_val = dev->params_vdev.priv_val;
	if (!priv_val->buf_info_owner && priv_val->buf_info_idx >= 0) {
		priv_val->buf_info_idx = -1;
		rkisp_clear_bits(dev, ISP3X_GAIN_CTRL, ISP3X_GAIN_2DDR_EN, false);
		rkisp_clear_bits(dev, ISP3X_RAWAWB_CTRL, ISP32_RAWAWB_2DDR_PATH_EN, false);
		return;
	}

	if (priv_val->buf_info_owner == RKISP_INFO2DRR_OWNER_GAIN) {
		reg = ISP3X_GAIN_CTRL;
		ctrl = ISP3X_GAIN_2DDR_EN;
		mask = ISP3X_GAIN_2DDR_EN;
	} else {
		reg = ISP3X_RAWAWB_CTRL;
		ctrl = ISP32_RAWAWB_2DDR_PATH_EN;
		mask = ISP32_RAWAWB_2DDR_PATH_EN | ISP32_RAWAWB_2DDR_PATH_DS;
	}

	idx = priv_val->buf_info_idx;
	if (idx >= 0) {
		buf = &priv_val->buf_info[idx];
		rkisp_finish_buffer(dev, buf);
		v4l2_dbg(4, rkisp_debug, &dev->v4l2_dev,
			 "%s data:0x%x 0x%x:0x%x\n", __func__,
			 *(u32 *)buf->vaddr, reg, rkisp_read(dev, reg, true));
		if (*(u32 *)buf->vaddr != RKISP_INFO2DDR_BUF_INIT && pbuf &&
		    (reg != ISP3X_RAWAWB_CTRL ||
		     !(rkisp_read(dev, reg, true) & ISP32_RAWAWB_2DDR_PATH_ERR))) {
			pbuf->stat.info2ddr.buf_fd = buf->dma_fd;
			pbuf->stat.info2ddr.owner = priv_val->buf_info_owner;
			pbuf->meas_type |= ISP35_STAT_INFO2DDR;
			buf_fd = buf->dma_fd;
		} else if (reg == ISP3X_RAWAWB_CTRL &&
			   rkisp_read(dev, reg, true) & ISP32_RAWAWB_2DDR_PATH_ERR) {
			v4l2_warn(&dev->v4l2_dev, "rawawb2ddr path error idx:%d\n", idx);
		} else {
			u32 v0 = rkisp_read(dev, reg, false);
			u32 v1 = rkisp_read_reg_cache(dev, reg);

			if ((v0 & mask) != (v1 & mask))
				rkisp_write(dev, reg, v0 | (v1 & mask), false);
		}

		if (buf_fd == -1)
			return;
	}
	/* get next unused buf to hw */
	for (idx = 0; idx < priv_val->buf_info_cnt; idx++) {
		buf = &priv_val->buf_info[idx];
		if (*(u32 *)buf->vaddr == RKISP_INFO2DDR_BUF_INIT)
			break;
	}

	if (idx == priv_val->buf_info_cnt) {
		rkisp_clear_bits(dev, reg, ctrl, false);
		priv_val->buf_info_idx = -1;
	} else {
		buf = &priv_val->buf_info[idx];
		rkisp_write(dev, ISP3X_MI_GAIN_WR_BASE, buf->dma_addr, false);
		if (dev->hw_dev->is_single)
			rkisp_write(dev, ISP3X_MI_WR_CTRL2, ISP3X_GAINSELF_UPD, true);
		if (priv_val->buf_info_idx < 0)
			rkisp_set_bits(dev, reg, 0, ctrl, false);
		priv_val->buf_info_idx = idx;
	}
}

static void
rkisp_stats_send_meas_fe(struct rkisp_isp_stats_vdev *stats_vdev, u32 w3a_ris)
{
	struct rkisp_isp_params_vdev *params_vdev = &stats_vdev->dev->params_vdev;
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp35_stat_buffer *stat_tmp_buf, *stat_buf = NULL;
	struct rkisp_buffer *cur_buf = NULL;
	unsigned long flags = 0;
	u32 cur_frame_id, size = stats_vdev->vdev_fmt.fmt.meta.buffersize;
	u32 val, mask = 0, ris = isp3_stats_read(stats_vdev, ISP3X_ISP_3A_RIS);
	u64 ns;

	if (!dev->is_aiisp_en)
		return;
	if (priv->is_ae0_fe)
		mask |= ISP3X_3A_RAWAE_CH0 | ISP3X_3A_RAWHIST_CH0;
	if (priv->is_ae3_fe)
		mask |= ISP3X_3A_RAWAE_BIG | ISP3X_3A_RAWHIST_BIG;
	if (priv->is_af_fe)
		mask |= ISP3X_3A_RAWAF;
	if (priv->is_awb_fe)
		mask |= ISP3X_3A_RAWAWB;
	if (priv->is_aiawb_fe)
		mask |= ISP35_AIAWB_DONE;
	if (ris & mask) {
		isp3_stats_write(stats_vdev, ISP3X_ISP_3A_ICR, ris & mask);
		if (ris & (mask & ISP3X_3A_RAWAWB)) {
			val = isp3_stats_read(stats_vdev, ISP3X_RAWAWB_CTRL);
			if (val & ISP35_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWAWB_CTRL, val);
		}
		if (ris & (mask & ISP3X_3A_RAWAF)) {
			val = isp3_stats_read(stats_vdev, ISP3X_RAWAF_CTRL);
			if (val & ISP35_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWAF_CTRL, val);
		}
		if (ris & (mask & ISP3X_3A_RAWAE_CH0)) {
			val = isp3_stats_read(stats_vdev, ISP3X_RAWAE_LITE_BASE);
			if (val & ISP35_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWAE_LITE_BASE, val);
		}
		if (ris & (mask & ISP3X_3A_RAWHIST_CH0)) {
			val = isp3_stats_read(stats_vdev, ISP3X_RAWHIST_LITE_BASE);
			if (val & ISP35_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWHIST_LITE_BASE, val);
		}
		if (ris & (mask & ISP3X_3A_RAWAE_BIG)) {
			val = isp3_stats_read(stats_vdev, ISP3X_RAWAE_BIG1_BASE);
			if (val & ISP35_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWAE_BIG1_BASE, val);
		}
		if (ris & (mask & ISP3X_3A_RAWHIST_BIG)) {
			val = isp3_stats_read(stats_vdev, ISP3X_RAWHIST_BIG1_BASE);
			if (val & ISP35_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWHIST_BIG1_BASE, val);
		}
	}

	if (dev->is_aiisp_l2_st)
		return;

	rkisp_dmarx_get_frame(dev, &cur_frame_id, NULL, &ns, true);
	if (!ns)
		ns = ktime_get_ns();
	spin_lock_irqsave(&stats_vdev->rd_lock, flags);
	if (!list_empty(&stats_vdev->stat)) {
		cur_buf = list_first_entry(&stats_vdev->stat,
					   struct rkisp_buffer, queue);
		list_del(&cur_buf->queue);
	}
	spin_unlock_irqrestore(&stats_vdev->rd_lock, flags);

	if (cur_buf) {
		stat_buf = cur_buf->vaddr[0];
		stat_tmp_buf = stats_vdev->stats_buf[0].vaddr;
		rkisp_finish_buffer(dev, &stats_vdev->stats_buf[0]);

		stat_buf->frame_id = cur_frame_id;
		stat_buf->params_id = params_vdev->cur_fe_frame_id;
		stat_buf->stat.info2ddr.buf_fd = -1;
		stat_buf->stat.info2ddr.owner = 0;
		stat_buf->stat.buf_aiawb_index = -1;
		stat_buf->stat.buf_bay3d_iir_index = -1;
		stat_buf->stat.buf_bay3d_ds_index = -1;
		stat_buf->stat.buf_bay3d_wgt_index = -1;
		stat_buf->stat.buf_aipre_gain_index = -1;
		stat_buf->stat.buf_gain_index = -1;
		stat_buf->stat.buf_vpsl_index = -1;
	}
	if (ris & (mask & ISP3X_3A_RAWAE_CH0) && stat_buf && stat_tmp_buf) {
		memcpy(&stat_buf->stat.rawae0,
		       &stat_tmp_buf->stat.rawae0, sizeof(struct isp33_rawae_stat));
		stat_buf->meas_type |= ISP35_STAT_RAWAE0;
	}
	if (ris & (mask & ISP3X_3A_RAWHIST_CH0) && stat_buf && stat_tmp_buf) {
		memcpy(&stat_buf->stat.rawhist0,
		       &stat_tmp_buf->stat.rawhist0, sizeof(struct isp33_rawhist_stat));
		stat_buf->meas_type |= ISP35_STAT_RAWHST0;
	}
	if (ris & (mask & ISP3X_3A_RAWAE_BIG) && stat_buf && stat_tmp_buf) {
		memcpy(&stat_buf->stat.rawae3,
		       &stat_tmp_buf->stat.rawae3, sizeof(struct isp33_rawae_stat));
		stat_buf->meas_type |= ISP35_STAT_RAWAE3;
	}
	if (ris & (mask & ISP3X_3A_RAWHIST_BIG) && stat_buf && stat_tmp_buf) {
		memcpy(&stat_buf->stat.rawhist3,
		       &stat_tmp_buf->stat.rawhist3, sizeof(struct isp33_rawhist_stat));
		stat_buf->meas_type |= ISP35_STAT_RAWHST3;
	}
	if (ris & (mask & ISP3X_3A_RAWAF) && stat_buf && stat_tmp_buf) {
		memcpy(&stat_buf->stat.rawaf,
		       &stat_tmp_buf->stat.rawaf, sizeof(struct isp39_rawaf_stat));
		stat_buf->meas_type |= ISP35_STAT_RAWAF;
	}
	if (ris & (mask & ISP3X_3A_RAWAWB) && stat_buf && stat_tmp_buf) {
		memcpy(&stat_buf->stat.rawawb,
		       &stat_tmp_buf->stat.rawawb, sizeof(struct isp33_rawawb_stat));
		stat_buf->meas_type |= ISP35_STAT_RAWAWB;
	}
	if (ris & (mask & ISP35_AIAWB_DONE) && stat_buf)
		rkisp_stats_get_aiawb_stats(stats_vdev, stat_buf);
	if (stat_buf)
		rkisp_stats_get_bay3d_stats(stats_vdev, stat_buf);
	if (cur_buf) {
		cur_buf->vb.sequence = cur_frame_id;
		cur_buf->vb.vb2_buf.timestamp = ns;
		vb2_set_plane_payload(&cur_buf->vb.vb2_buf, 0, size);
		vb2_buffer_done(&cur_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}
	v4l2_dbg(4, rkisp_debug, &stats_vdev->dev->v4l2_dev,
		 "%s seq:%d params_id:%d ris:0x%x w3a:0x%x buf:0x%x meas_type:0x%x\n",
		 __func__, cur_frame_id, params_vdev->cur_fe_frame_id, ris, w3a_ris,
		 !cur_buf ? -1 : cur_buf->buff_addr[0],
		 !stat_buf ? 0 : stat_buf->meas_type);
}

static void
rkisp_stats_send_meas(struct rkisp_isp_stats_vdev *stats_vdev, u32 w3a_ris)
{
	struct rkisp_isp_params_vdev *params_vdev = &stats_vdev->dev->params_vdev;
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_buffer *cur_buf = stats_vdev->cur_buf;
	struct rkisp35_stat_buffer *stat_tmp_buf = NULL, *cur_stat_buf = NULL;
	u32 cur_frame_id, size = stats_vdev->vdev_fmt.fmt.meta.buffersize;
	u32 val, mask, ris = isp3_stats_read(stats_vdev, ISP3X_ISP_3A_RIS);
	u64 ns;
	bool is_dummy = false;
	unsigned long flags = 0;

	mask = ISP3X_3A_DDR_DONE;
	if (!dev->is_aiisp_en) {
		mask |= ISP3X_3A_RAWAF | ISP3X_3A_RAWAE_CH0 | ISP3X_3A_RAWHIST_CH0 |
			ISP3X_3A_RAWAE_BIG | ISP3X_3A_RAWHIST_BIG | ISP3X_3A_RAWAWB |
			ISP35_AIAWB_DONE;
	}
	if (dev->is_aiisp_en) {
		if (!priv->is_ae0_fe)
			mask |= ISP3X_3A_RAWHIST_CH0 | ISP3X_3A_RAWAE_CH0;
		if (!priv->is_ae3_fe)
			mask |= ISP3X_3A_RAWAE_BIG | ISP3X_3A_RAWHIST_BIG;
		if (!priv->is_af_fe)
			mask |= ISP3X_3A_RAWAF;
		if (!priv->is_awb_fe)
			mask |= ISP3X_3A_RAWAWB;
		if (!priv->is_aiawb_fe)
			mask |= ISP35_AIAWB_DONE;
	}
	if (ris & mask) {
		isp3_stats_write(stats_vdev, ISP3X_ISP_3A_ICR, ris & mask);
		if (dev->is_aiisp_en) {
			if (ris & (mask & ISP3X_3A_RAWAWB)) {
				val = isp3_stats_read(stats_vdev, ISP3X_RAWAWB_CTRL);
				if (val & ISP35_3A_MEAS_DONE)
					isp3_module_done(stats_vdev, ISP3X_RAWAWB_CTRL, val);
			}
			if (ris & (mask & ISP3X_3A_RAWAF)) {
				val = isp3_stats_read(stats_vdev, ISP3X_RAWAF_CTRL);
				if (val & ISP35_3A_MEAS_DONE)
					isp3_module_done(stats_vdev, ISP3X_RAWAF_CTRL, val);
			}
			if (ris & (mask & ISP3X_3A_RAWAE_CH0)) {
				val = isp3_stats_read(stats_vdev, ISP3X_RAWAE_LITE_BASE);
				if (val & ISP35_3A_MEAS_DONE)
					isp3_module_done(stats_vdev, ISP3X_RAWAE_LITE_BASE, val);
			}
			if (ris & (mask & ISP3X_3A_RAWHIST_CH0)) {
				val = isp3_stats_read(stats_vdev, ISP3X_RAWHIST_LITE_BASE);
				if (val & ISP35_3A_MEAS_DONE)
					isp3_module_done(stats_vdev, ISP3X_RAWHIST_LITE_BASE, val);
			}
			if (ris & (mask & ISP3X_3A_RAWAE_BIG)) {
				val = isp3_stats_read(stats_vdev, ISP3X_RAWAE_BIG1_BASE);
				if (val & ISP35_3A_MEAS_DONE)
					isp3_module_done(stats_vdev, ISP3X_RAWAE_BIG1_BASE, val);
			}
			if (ris & (mask & ISP3X_3A_RAWHIST_BIG)) {
				val = isp3_stats_read(stats_vdev, ISP3X_RAWHIST_BIG1_BASE);
				if (val & ISP35_3A_MEAS_DONE)
					isp3_module_done(stats_vdev, ISP3X_RAWHIST_BIG1_BASE, val);
			}
		}
	}
	rkisp_dmarx_get_frame(dev, &cur_frame_id, NULL, &ns, !dev->is_aiisp_en);
	if (!ns)
		ns = ktime_get_ns();
	if (dev->is_aiisp_en) {
		spin_lock_irqsave(&stats_vdev->rd_lock, flags);
		if (!list_empty(&stats_vdev->stat)) {
			cur_buf = list_first_entry(&stats_vdev->stat,
						   struct rkisp_buffer, queue);
			list_del(&cur_buf->queue);
		}
		spin_unlock_irqrestore(&stats_vdev->rd_lock, flags);
		stat_tmp_buf = stats_vdev->stats_buf[0].vaddr;
		rkisp_finish_buffer(dev, &stats_vdev->stats_buf[0]);
	}
	if (!stats_vdev->rdbk_drop) {
		if (!cur_buf && stats_vdev->stats_buf[0].mem_priv) {
			rkisp_finish_buffer(stats_vdev->dev, &stats_vdev->stats_buf[0]);
			cur_stat_buf = stats_vdev->stats_buf[0].vaddr;
			is_dummy = true;
		} else if (cur_buf) {
			cur_stat_buf = cur_buf->vaddr[0];
		}

		if (dev->unite_index > ISP_UNITE_LEFT && cur_stat_buf)
			cur_stat_buf = (void *)cur_stat_buf + size / dev->unite_div * dev->unite_index;
		if ((dev->unite_div == ISP_UNITE_DIV2 && dev->unite_index != ISP_UNITE_RIGHT) ||
		    (dev->unite_div == ISP_UNITE_DIV4 && dev->unite_index != ISP_UNITE_RIGHT_B)) {
			cur_buf = NULL;
			is_dummy = false;
		}

		if (dev->unite_div < ISP_UNITE_DIV2 ||
		    (dev->unite_div == ISP_UNITE_DIV2 && dev->unite_index == ISP_UNITE_RIGHT) ||
		    (dev->unite_div == ISP_UNITE_DIV4 && dev->unite_index == ISP_UNITE_RIGHT_B)) {
			/* config buf for next frame */
			stats_vdev->cur_buf = NULL;
			if (stats_vdev->nxt_buf) {
				stats_vdev->cur_buf = stats_vdev->nxt_buf;
				stats_vdev->nxt_buf = NULL;
			}
			if (!dev->is_aiisp_en)
				rkisp_stats_update_buf(stats_vdev);
		}
	} else {
		cur_buf = NULL;
	}

	if (cur_stat_buf) {
		cur_stat_buf->frame_id = cur_frame_id;
		cur_stat_buf->params_id = params_vdev->cur_frame_id;
		cur_stat_buf->stat.info2ddr.buf_fd = -1;
		cur_stat_buf->stat.info2ddr.owner = 0;
		cur_stat_buf->stat.buf_aiawb_index = -1;
		cur_stat_buf->stat.buf_bay3d_iir_index = -1;
		cur_stat_buf->stat.buf_bay3d_ds_index = -1;
		cur_stat_buf->stat.buf_bay3d_wgt_index = -1;
		cur_stat_buf->stat.buf_aipre_gain_index = -1;
		cur_stat_buf->stat.buf_gain_index = -1;
		cur_stat_buf->stat.buf_vpsl_index = -1;
	}

	if (ris & (mask & ISP3X_3A_RAWAF) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP35_STAT_RAWAF;
		if (dev->is_aiisp_en && stat_tmp_buf)
			memcpy(&cur_stat_buf->stat.rawaf,
			       &stat_tmp_buf->stat.rawaf, sizeof(struct isp39_rawaf_stat));
	}
	if (ris & (mask & ISP3X_3A_RAWAE_CH0) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP35_STAT_RAWAE0;
		if (dev->is_aiisp_en && stat_tmp_buf)
			memcpy(&cur_stat_buf->stat.rawae0,
			       &stat_tmp_buf->stat.rawae0, sizeof(struct isp33_rawae_stat));
	}
	if (ris & (mask & ISP3X_3A_RAWHIST_CH0) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP35_STAT_RAWHST0;
		if (dev->is_aiisp_en && stat_tmp_buf)
			memcpy(&cur_stat_buf->stat.rawhist0,
			       &stat_tmp_buf->stat.rawhist0, sizeof(struct isp33_rawhist_stat));
	}
	if (ris & (mask & ISP3X_3A_RAWAE_BIG) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP35_STAT_RAWAE3;
		if (dev->is_aiisp_en && stat_tmp_buf)
			memcpy(&cur_stat_buf->stat.rawae3,
			       &stat_tmp_buf->stat.rawae3, sizeof(struct isp33_rawae_stat));
	}
	if (ris & (mask & ISP3X_3A_RAWHIST_BIG) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP35_STAT_RAWHST3;
		if (dev->is_aiisp_en && stat_tmp_buf)
			memcpy(&cur_stat_buf->stat.rawhist3,
			       &stat_tmp_buf->stat.rawhist3, sizeof(struct isp33_rawhist_stat));
	}
	if (ris & (mask & ISP3X_3A_RAWAWB) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP35_STAT_RAWAWB;
		if (dev->is_aiisp_en && stat_tmp_buf)
			memcpy(&cur_stat_buf->stat.rawawb,
			       &stat_tmp_buf->stat.rawawb, sizeof(struct isp33_rawawb_stat));
	}
	if (ris & (mask & ISP35_AIAWB_DONE) && cur_stat_buf)
		rkisp_stats_get_aiawb_stats(stats_vdev, cur_stat_buf);
	if (ris & ISP35_AWBSYNC_DONE && cur_stat_buf)
		rkisp_stats_get_awbsync_stats(stats_vdev, cur_stat_buf);

	if (!dev->is_aiisp_en)
		rkisp_stats_get_bay3d_stats(stats_vdev, cur_stat_buf);
	rkisp_stats_get_sharp_stats(stats_vdev, cur_stat_buf);
	rkisp_stats_get_enh_stats(stats_vdev, cur_stat_buf);
	rkisp_stats_get_hist_stats(stats_vdev, cur_stat_buf);

	if (cur_stat_buf && (dev->is_first_double || dev->is_wait_aiq)) {
		cur_stat_buf->meas_type |= ISP35_STAT_RTT_FST;
		dev_info(dev->dev, "stats seq:%d meas_type:0x%x for fast\n",
			 cur_frame_id, cur_stat_buf->meas_type);
	}

	if (is_dummy) {
		spin_lock_irqsave(&stats_vdev->rd_lock, flags);
		if (!list_empty(&stats_vdev->stat)) {
			cur_buf = list_first_entry(&stats_vdev->stat, struct rkisp_buffer, queue);
			list_del(&cur_buf->queue);
		}
		spin_unlock_irqrestore(&stats_vdev->rd_lock, flags);
		if (cur_buf) {
			memcpy(cur_buf->vaddr[0], stats_vdev->stats_buf[0].vaddr, size);
			cur_stat_buf = cur_buf->vaddr[0];
		}
	}
	if (cur_buf && cur_stat_buf) {
		rkisp_stats_info2ddr(stats_vdev, cur_stat_buf);

		vb2_set_plane_payload(&cur_buf->vb.vb2_buf, 0, size);
		cur_buf->vb.sequence = cur_frame_id;
		cur_buf->vb.vb2_buf.timestamp = ns;
		vb2_buffer_done(&cur_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}
	v4l2_dbg(4, rkisp_debug, &stats_vdev->dev->v4l2_dev,
		 "%s seq:%d params_id:%d ris:0x%x w3a:0x%x buf:0x%x meas_type:0x%x\n",
		 __func__, cur_frame_id, params_vdev->cur_frame_id, ris, w3a_ris,
		 !cur_buf ? -1 : cur_buf->buff_addr[0],
		 !cur_stat_buf ? 0 : cur_stat_buf->meas_type);
}

static void
rkisp_stats_isr_v35(struct rkisp_isp_stats_vdev *stats_vdev,
		    u32 isp_ris, u32 isp3a_ris)
{
	u32 w3a_ris;

	rkisp_pdaf_isr(stats_vdev->dev);

	w3a_ris = rkisp_read(stats_vdev->dev, ISP39_W3A_INT_STAT, true);
	if (w3a_ris) {
		rkisp_write(stats_vdev->dev, ISP39_W3A_INT_STAT, w3a_ris, true);
		if (w3a_ris & ISP39_W3A_INT_ERR_MASK)
			v4l2_err(&stats_vdev->dev->v4l2_dev, "w3a error 0x%x\n", w3a_ris);
	}

	if (isp_ris & ISP3X_BAY3D_FRM_END)
		rkisp_stats_send_meas_fe(stats_vdev, w3a_ris);
	if (isp_ris & ISP3X_FRAME)
		rkisp_stats_send_meas(stats_vdev, w3a_ris);
}

static void
rkisp_get_stat_size_v35(struct rkisp_isp_stats_vdev *stats_vdev,
			unsigned int sizes[])
{
	int mult = stats_vdev->dev->unite_div;

	sizes[0] = ALIGN(sizeof(struct rkisp35_stat_buffer), 16);
	sizes[0] *= mult;
	stats_vdev->vdev_fmt.fmt.meta.buffersize = sizes[0];
}

static int
rkisp_stats_tb_v35(struct rkisp_isp_stats_vdev *stats_vdev,
		   struct rkisp_buffer *stats_buf)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp35_stat_buffer *buf = stats_vdev->stats_buf[0].vaddr;
	u32 size = stats_vdev->vdev_fmt.fmt.meta.buffersize;
	int ret = -EINVAL;

	if (dev->isp_state & ISP_START && stats_buf->vaddr[0] &&
	    buf && !buf->frame_id && buf->meas_type) {
		dev_info(dev->dev, "tb stat seq:%d meas_type:0x%x\n",
			 buf->frame_id, buf->meas_type);
		memcpy(stats_buf->vaddr[0], buf, size);
		stats_buf->vb.sequence = buf->frame_id;
		buf->meas_type = 0;
		ret = 0;
	}
	return ret;
}

static void
rkisp_stats_first_ddr_config_v35(struct rkisp_isp_stats_vdev *stats_vdev)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_pdaf_vdev *pdaf_vdev = dev->pdaf_vdev;
	u32 val, size = 0, div = dev->unite_div;

	if (dev->isp_sdev.in_fmt.fmt_type == FMT_YUV)
		return;

	rkisp_get_stat_size_v35(stats_vdev, &size);
	stats_vdev->stats_buf[0].is_need_vaddr = true;
	stats_vdev->stats_buf[0].size = size;
	if (!stats_vdev->stats_buf[0].mem_priv)
		rkisp_alloc_buffer(dev, &stats_vdev->stats_buf[0]);
	if (!stats_vdev->stats_buf[0].vaddr)
		v4l2_warn(&dev->v4l2_dev, "stats alloc buf fail\n");
	else
		memset(stats_vdev->stats_buf[0].vaddr, 0, size);
	if (rkisp_stats_update_buf(stats_vdev) < 0) {
		v4l2_err(&dev->v4l2_dev, "no stats buf to enable w3a\n");
		return;
	}
	if (dev->hw_dev->is_single)
		rkisp_unite_set_bits(dev, ISP3X_SWS_CFG, 0, ISP3X_3A_DDR_WRITE_EN, false);
	val = rkisp_read(dev, ISP39_W3A_CTRL0, false);
	val &= ~(ISP39_W3A_AUTO_CLR_EN | ISP35_W3A_FORCE_UPD_F);
	val |= ISP39_W3A_EN | ISP39_W3A_FORCE_UPD;
	if (!dev->is_aiisp_en)
		val |= ISP39_W3A_AUTO_CLR_EN;
	else
		val |= ISP35_W3A_FORCE_UPD_F;
	if (pdaf_vdev && pdaf_vdev->streaming &&
	    !(dev->isp_state & ISP_START)) {
		val |= ISP39_W3A_PDAF_EN;
		rkisp_pdaf_update_buf(dev);
		if (pdaf_vdev->next_buf) {
			pdaf_vdev->curr_buf = pdaf_vdev->next_buf;
			pdaf_vdev->next_buf = NULL;
		}
	}
	rkisp_unite_write(dev, ISP39_W3A_CTRL0, val, false);
	rkisp_unite_write(dev, ISP39_W3A_WR_SIZE, size / div, false);
	if (stats_vdev->nxt_buf) {
		stats_vdev->cur_buf = stats_vdev->nxt_buf;
		stats_vdev->nxt_buf = NULL;
	}
}

static void
rkisp_stats_next_ddr_config_v35(struct rkisp_isp_stats_vdev *stats_vdev)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_hw_dev *hw = dev->hw_dev;
	struct rkisp_pdaf_vdev *pdaf_vdev = dev->pdaf_vdev;

	if (!stats_vdev->streamon || dev->isp_sdev.in_fmt.fmt_type == FMT_YUV)
		return;
	/* pingpong buf */
	if (hw->is_single) {
		if (!dev->is_aiisp_en)
			rkisp_stats_update_buf(stats_vdev);
		if (pdaf_vdev && pdaf_vdev->streaming &&
		    !(dev->isp_state & ISP_START))
			rkisp_pdaf_update_buf(dev);
	}
}

static void rkisp_stats_stop_v35(struct rkisp_isp_stats_vdev *stats_vdev)
{
	struct rkisp_device *dev = stats_vdev->dev;
	u32 val, addr;

	/* aiq crash or exit first */
	if (dev->isp_state & ISP_START &&
	    stats_vdev->stats_buf[0].mem_priv) {
		rkisp_stats_update_buf(stats_vdev);
		addr = stats_vdev->stats_buf[0].dma_addr;
		readl_poll_timeout(dev->hw_dev->base_addr + ISP39_W3A_AEBIG_ADDR_SHD,
				   val, val == addr, 5000, 50000);
	}
}

static struct rkisp_isp_stats_ops rkisp_isp_stats_ops_tbl = {
	.isr_hdl = rkisp_stats_isr_v35,
	.get_stat_size = rkisp_get_stat_size_v35,
	.stats_tb = rkisp_stats_tb_v35,
	.first_ddr_cfg = rkisp_stats_first_ddr_config_v35,
	.next_ddr_cfg = rkisp_stats_next_ddr_config_v35,
	.stats_stop = rkisp_stats_stop_v35,
};

void rkisp_init_stats_vdev_v35(struct rkisp_isp_stats_vdev *stats_vdev)
{
	stats_vdev->ops = &rkisp_isp_stats_ops_tbl;
}

void rkisp_uninit_stats_vdev_v35(struct rkisp_isp_stats_vdev *stats_vdev)
{

}
