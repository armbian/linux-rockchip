// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

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
#include "isp_stats_v39.h"
#include "isp_params_v39.h"

#define ISP39_3A_MEAS_DONE BIT(31)

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
rkisp_stats_get_dhaz_stats(struct rkisp_isp_stats_vdev *stats_vdev,
			   struct rkisp39_stat_buffer *pbuf)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_isp_params_vdev *params_vdev = &dev->params_vdev;
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	struct isp39_isp_params_cfg *params_rec = params_vdev->isp39_params;
	struct isp39_dhaz_cfg *arg_rec = &params_rec->others.dhaz_cfg;
	struct isp39_dhaz_stat *dhaz;
	int value, i, j, timeout;

	if (!pbuf)
		return 0;

	value = isp3_stats_read(stats_vdev, ISP3X_DHAZ_CTRL);
	if (value & ISP_DHAZ_ENMUX) {
		dhaz = &pbuf->stat.dhaz;

		value = isp3_stats_read(stats_vdev, ISP39_DHAZ_ADP_RD0);
		dhaz->adp_air_base = (value >> 16) & 0xFFFF;
		dhaz->adp_wt = value & 0xFFFF;

		value = isp3_stats_read(stats_vdev, ISP39_DHAZ_ADP_RD1);
		dhaz->adp_tmax = value & 0xFFFF;

		for (i = 0; i < priv_val->dhaz_blk_num; i++) {
			value = ISP39_DHAZ_IIR_RD_ID(i) | ISP39_DHAZ_IIR_RD_P;
			isp3_stats_write(stats_vdev, ISP39_DHAZ_HIST_RW, value);
			timeout = 5;
			while (timeout--) {
				value = isp3_stats_read(stats_vdev, ISP39_DHAZ_HIST_RW);
				if (value & ISP39_DHAZ_IIR_RDATA_VAL)
					break;
				udelay(2);
			}
			if (timeout < 0) {
				v4l2_warn(&dev->v4l2_dev, "%s read:%d timeout\n", __func__, i);
				goto end;
			}
			for (j = 0; j < ISP39_DHAZ_HIST_IIR_NUM / 2; j++) {
				value = isp3_stats_read(stats_vdev, ISP39_DHAZ_HIST_IIR0 + 4 * j);
				dhaz->hist_iir[i][2 * j] = value & 0xFFFF;
				dhaz->hist_iir[i][2 * j + 1] = value >> 16;
			}
		}
		memcpy(arg_rec->hist_iir, dhaz->hist_iir, sizeof(dhaz->hist_iir));
		pbuf->meas_type |= ISP39_STAT_DHAZ;
	}
end:
	return 0;
}

static int
rkisp_stats_get_bay3d_stats(struct rkisp_isp_stats_vdev *stats_vdev,
			    struct rkisp39_stat_buffer *pbuf)
{
	struct rkisp_isp_params_val_v39 *priv = stats_vdev->dev->params_vdev.priv_val;
	struct isp39_bay3d_stat *bay3d;
	u32 i, value;

	if (!pbuf)
		return 0;
	value = isp3_stats_read(stats_vdev, ISP3X_BAY3D_CTRL);
	if (value & 0x1) {
		bay3d = &pbuf->stat.bay3d;
		value = isp3_stats_read(stats_vdev, ISP39_BAY3D_SIGSUM);
		bay3d->tnr_auto_sigma_count = value;
		for (i = 0; i < ISP39_BAY3D_TNRSIG_NUM / 2; i++) {
			value = isp3_stats_read(stats_vdev, ISP39_BAY3D_TNRSIGYO0 + i * 4);
			bay3d->tnr_auto_sigma_calc[i * 2] = value & 0xfff;
			bay3d->tnr_auto_sigma_calc[i * 2 + 1] = (value >> 16) & 0xfff;
		}
		pbuf->stat.buf_bay3d_iir_index = priv->bay3d_iir_cur_idx;
		pbuf->meas_type |= ISP39_STAT_BAY3D;
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

	if (addr) {
		for (i = 0; i < dev->unite_div; i++) {
			val = addr + i * size;

			rkisp_idx_write(dev, ISP39_W3A_AEBIG_ADDR, val, i, false);

			offset = sizeof(struct isp39_rawae_stat) +
				 sizeof(struct isp39_rawhist_stat);
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
rkisp_stats_info2ddr(struct rkisp_isp_stats_vdev *stats_vdev,
		     struct rkisp39_stat_buffer *pbuf)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
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
			pbuf->meas_type |= ISP39_STAT_INFO2DDR;
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
rkisp_stats_send_meas_fe(struct rkisp_isp_stats_vdev *stats_vdev)
{
	struct rkisp_isp_params_vdev *params_vdev = &stats_vdev->dev->params_vdev;
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp39_stat_buffer *stat_tmp_buf, *stat_buf = NULL;
	struct rkisp_buffer *cur_buf = NULL;
	unsigned long flags = 0;
	u32 cur_frame_id, size = stats_vdev->vdev_fmt.fmt.meta.buffersize;
	u32 val, ris = isp3_stats_read(stats_vdev, ISP3X_ISP_3A_RIS);
	u32 mask = ISP3X_3A_RAWAF | ISP3X_3A_RAWHIST_CH0 | ISP3X_3A_RAWAE_CH0;
	u64 ns;

	if (!dev->is_aiisp_en)
		return;

	if (ris & mask) {
		isp3_stats_write(stats_vdev, ISP3X_ISP_3A_ICR, ris & mask);
		val = isp3_stats_read(stats_vdev, ISP3X_RAWAE_LITE_BASE);
		if (val & ISP39_3A_MEAS_DONE)
			isp3_module_done(stats_vdev, ISP3X_RAWAE_LITE_BASE, val);
		val = isp3_stats_read(stats_vdev, ISP3X_RAWHIST_LITE_BASE);
		if (val & ISP39_3A_MEAS_DONE)
			isp3_module_done(stats_vdev, ISP3X_RAWHIST_LITE_BASE, val);
		val = isp3_stats_read(stats_vdev, ISP3X_RAWAF_CTRL);
		if (val & ISP39_3A_MEAS_DONE)
			isp3_module_done(stats_vdev, ISP3X_RAWAF_CTRL, val);
	}
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
		stat_buf->stat.buf_bay3d_iir_index = -1;
	}
	if (ris & ISP3X_3A_RAWAE_CH0 && stat_buf) {
		memcpy(&stat_buf->stat.rawae0,
		       &stat_tmp_buf->stat.rawae0, sizeof(struct isp39_rawae_stat));
		stat_buf->meas_type |= ISP39_STAT_RAWAE0;
	}
	if (ris & ISP3X_3A_RAWHIST_CH0 && stat_buf) {
		memcpy(&stat_buf->stat.rawhist0,
		       &stat_tmp_buf->stat.rawhist0, sizeof(struct isp39_rawhist_stat));
		stat_buf->meas_type |= ISP39_STAT_RAWHST0;
	}
	if (ris & ISP3X_3A_RAWAF && stat_buf) {
		memcpy(&stat_buf->stat.rawaf,
		       &stat_tmp_buf->stat.rawaf, sizeof(struct isp39_rawaf_stat));
		stat_buf->meas_type |= ISP39_STAT_RAWAF;
	}
	if (stat_buf)
		rkisp_stats_get_bay3d_stats(stats_vdev, stat_buf);
	if (cur_buf) {
		cur_buf->vb.sequence = cur_frame_id;
		cur_buf->vb.vb2_buf.timestamp = ns;
		vb2_set_plane_payload(&cur_buf->vb.vb2_buf, 0, size);
		vb2_buffer_done(&cur_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}
	v4l2_dbg(4, rkisp_debug, &stats_vdev->dev->v4l2_dev,
		 "%s seq:%d params_id:%d ris:0x%x buf:%p meas_type:0x%x\n",
		 __func__,
		 cur_frame_id, params_vdev->cur_fe_frame_id, ris,
		 cur_buf, !stat_buf ? 0 : stat_buf->meas_type);
}

static void
rkisp_stats_send_meas(struct rkisp_isp_stats_vdev *stats_vdev)
{
	struct rkisp_isp_params_vdev *params_vdev = &stats_vdev->dev->params_vdev;
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_buffer *cur_buf = stats_vdev->cur_buf;
	struct rkisp39_stat_buffer *stat_tmp_buf, *cur_stat_buf = NULL;
	unsigned long flags = 0;
	u32 cur_frame_id, size = stats_vdev->vdev_fmt.fmt.meta.buffersize;
	u32 val, ris = isp3_stats_read(stats_vdev, ISP3X_ISP_3A_RIS);
	u32 mask = ISP3X_3A_RAWAE_BIG | ISP3X_3A_RAWHIST_BIG | ISP3X_3A_RAWAWB | ISP3X_3A_DDR_DONE;
	u64 ns;

	if (!dev->is_aiisp_en)
		mask |= ISP3X_3A_RAWAF | ISP3X_3A_RAWHIST_CH0 | ISP3X_3A_RAWAE_CH0;
	if (ris & mask) {
		isp3_stats_write(stats_vdev, ISP3X_ISP_3A_ICR, ris & mask);
		if (dev->is_aiisp_en) {
			val = isp3_stats_read(stats_vdev, ISP3X_RAWAE_BIG1_BASE);
			if (val & ISP39_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWAE_BIG1_BASE, val);
			val = isp3_stats_read(stats_vdev, ISP3X_RAWHIST_BIG1_BASE);
			if (val & ISP39_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWHIST_BIG1_BASE, val);
			val = isp3_stats_read(stats_vdev, ISP3X_RAWAWB_CTRL);
			if (val & ISP39_3A_MEAS_DONE)
				isp3_module_done(stats_vdev, ISP3X_RAWAWB_CTRL, val);
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
		if (cur_buf) {
			cur_stat_buf = cur_buf->vaddr[0];
			cur_stat_buf->frame_id = cur_frame_id;
			cur_stat_buf->params_id = params_vdev->cur_frame_id;
			cur_stat_buf->stat.info2ddr.buf_fd = -1;
			cur_stat_buf->stat.info2ddr.owner = 0;
			cur_stat_buf->stat.buf_bay3d_iir_index = -1;
		}
		/* buffer done when frame of right handle */
		if (dev->unite_div > ISP_UNITE_DIV1) {
			if (dev->unite_index == ISP_UNITE_LEFT) {
				cur_buf = NULL;
			} else if (cur_stat_buf) {
				cur_stat_buf = (void *)cur_stat_buf + size / dev->unite_div;
				cur_stat_buf->frame_id = cur_frame_id;
				cur_stat_buf->params_id = params_vdev->cur_frame_id;
			}
		}

		if (dev->unite_div < ISP_UNITE_DIV2 || dev->unite_index == ISP_UNITE_RIGHT) {
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

	if (ris & (mask & ISP3X_3A_RAWAF) && cur_stat_buf)
		cur_stat_buf->meas_type |= ISP39_STAT_RAWAF;
	if (ris & (mask & ISP3X_3A_RAWAE_CH0) && cur_stat_buf)
		cur_stat_buf->meas_type |= ISP39_STAT_RAWAE0;
	if (ris & (mask & ISP3X_3A_RAWHIST_CH0) && cur_stat_buf)
		cur_stat_buf->meas_type |= ISP39_STAT_RAWHST0;
	if (ris & (mask & ISP3X_3A_RAWAE_BIG) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP39_STAT_RAWAE3;
		if (dev->is_aiisp_en)
			memcpy(&cur_stat_buf->stat.rawae3,
			       &stat_tmp_buf->stat.rawae3, sizeof(struct isp39_rawae_stat));
	}
	if (ris & (mask & ISP3X_3A_RAWHIST_BIG) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP39_STAT_RAWHST3;
		if (dev->is_aiisp_en)
			memcpy(&cur_stat_buf->stat.rawhist3,
			       &stat_tmp_buf->stat.rawhist3, sizeof(struct isp39_rawhist_stat));
	}
	if (ris & (mask & ISP3X_3A_RAWAWB) && cur_stat_buf) {
		cur_stat_buf->meas_type |= ISP39_STAT_RAWAWB;
		if (dev->is_aiisp_en)
			memcpy(&cur_stat_buf->stat.rawawb,
			       &stat_tmp_buf->stat.rawawb, sizeof(struct isp39_rawawb_stat));
	}

	rkisp_stats_get_dhaz_stats(stats_vdev, cur_stat_buf);
	if (!dev->is_aiisp_en)
		rkisp_stats_get_bay3d_stats(stats_vdev, cur_stat_buf);

	if (cur_buf && cur_stat_buf) {
		rkisp_stats_info2ddr(stats_vdev, cur_stat_buf);

		vb2_set_plane_payload(&cur_buf->vb.vb2_buf, 0, size);
		cur_buf->vb.sequence = cur_frame_id;
		cur_buf->vb.vb2_buf.timestamp = ns;
		vb2_buffer_done(&cur_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}
	v4l2_dbg(4, rkisp_debug, &stats_vdev->dev->v4l2_dev,
		 "%s seq:%d params_id:%d ris:0x%x buf:%p meas_type:0x%x\n",
		 __func__,
		 cur_frame_id, params_vdev->cur_frame_id, ris,
		 cur_buf, !cur_stat_buf ? 0 : cur_stat_buf->meas_type);
}

static void
rkisp_stats_isr_v39(struct rkisp_isp_stats_vdev *stats_vdev,
		    u32 isp_ris, u32 isp3a_ris)
{
	rkisp_pdaf_isr(stats_vdev->dev);
	if (isp_ris & ISP3X_BAY3D_FRM_END)
		rkisp_stats_send_meas_fe(stats_vdev);
	if (isp_ris & ISP3X_FRAME)
		rkisp_stats_send_meas(stats_vdev);
}

static void
rkisp_get_stat_size_v39(struct rkisp_isp_stats_vdev *stats_vdev,
			unsigned int sizes[])
{
	int mult = stats_vdev->dev->unite_div;

	sizes[0] = ALIGN(sizeof(struct rkisp39_stat_buffer), 16);
	sizes[0] *= mult;
	stats_vdev->vdev_fmt.fmt.meta.buffersize = sizes[0];
}

static void
rkisp_stats_first_ddr_config_v39(struct rkisp_isp_stats_vdev *stats_vdev)
{
	struct rkisp_device *dev = stats_vdev->dev;
	struct rkisp_pdaf_vdev *pdaf_vdev = dev->pdaf_vdev;
	u32 val, size = 0, div = dev->unite_div;

	if (dev->isp_sdev.in_fmt.fmt_type == FMT_YUV)
		return;

	rkisp_get_stat_size_v39(stats_vdev, &size);
	stats_vdev->stats_buf[0].is_need_vaddr = true;
	stats_vdev->stats_buf[0].size = size;
	if (rkisp_alloc_buffer(dev, &stats_vdev->stats_buf[0]))
		v4l2_warn(&dev->v4l2_dev, "stats alloc buf fail\n");
	else
		memset(stats_vdev->stats_buf[0].vaddr, 0, size);
	if (rkisp_stats_update_buf(stats_vdev) < 0) {
		v4l2_err(&dev->v4l2_dev, "no stats buf to enable w3a\n");
		return;
	}
	rkisp_unite_set_bits(dev, ISP3X_SWS_CFG, 0, ISP3X_3A_DDR_WRITE_EN, false);
	val = ISP39_W3A_EN | ISP39_W3A_FORCE_UPD;
	if (!dev->is_aiisp_en)
		val |= ISP39_W3A_AUTO_CLR_EN;
	if (pdaf_vdev && pdaf_vdev->streaming) {
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
rkisp_stats_next_ddr_config_v39(struct rkisp_isp_stats_vdev *stats_vdev)
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
		if (pdaf_vdev && pdaf_vdev->streaming)
			rkisp_pdaf_update_buf(dev);
	}
}

static struct rkisp_isp_stats_ops rkisp_isp_stats_ops_tbl = {
	.isr_hdl = rkisp_stats_isr_v39,
	.get_stat_size = rkisp_get_stat_size_v39,
	.first_ddr_cfg = rkisp_stats_first_ddr_config_v39,
	.next_ddr_cfg = rkisp_stats_next_ddr_config_v39,
};

void rkisp_init_stats_vdev_v39(struct rkisp_isp_stats_vdev *stats_vdev)
{
	stats_vdev->ops = &rkisp_isp_stats_ops_tbl;
}

void rkisp_uninit_stats_vdev_v39(struct rkisp_isp_stats_vdev *stats_vdev)
{

}
