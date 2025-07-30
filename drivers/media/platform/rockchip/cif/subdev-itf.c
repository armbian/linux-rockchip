// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip CIF Driver
 *
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#include <linux/pm_runtime.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-fwnode.h>
#include "dev.h"
#include <linux/regulator/consumer.h>
#include <linux/rk-camera-module.h>
#include "common.h"
#include "../../../i2c/cam-tb-setup.h"

static inline struct sditf_priv *to_sditf_priv(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct sditf_priv, sd);
}

void sditf_event_inc_sof(struct sditf_priv *priv)
{
	if (priv) {
		struct v4l2_event event = {
			.type = V4L2_EVENT_FRAME_SYNC,
			.u.frame_sync.frame_sequence =
				atomic_inc_return(&priv->frm_sync_seq) - 1,
		};
		v4l2_event_queue(priv->sd.devnode, &event);
		if (priv->cif_dev->exp_dbg)
			dev_info(priv->dev, "sof %d\n", atomic_read(&priv->frm_sync_seq) - 1);
	}
}

void sditf_event_exposure_notifier(struct sditf_priv *priv,
					   struct sditf_effect_exp *effect_exp)
{
	if (priv) {
		struct v4l2_event event = {
			.type = V4L2_EVENT_EXPOSURE,
		};
		v4l2_event_queue(priv->sd.devnode, &event);
	}
}

u32 sditf_get_sof(struct sditf_priv *priv)
{
	if (priv)
		return atomic_read(&priv->frm_sync_seq) - 1;

	return 0;
}

void sditf_set_sof(struct sditf_priv *priv, u32 seq)
{
	if (priv)
		atomic_set(&priv->frm_sync_seq, seq);
}

static int sditf_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
					     struct v4l2_event_subscription *sub)
{
	if (sub->type == V4L2_EVENT_FRAME_SYNC || sub->type == V4L2_EVENT_EXPOSURE)
		return v4l2_event_subscribe(fh, sub, RKCIF_V4L2_EVENT_ELEMS, NULL);
	else
		return -EINVAL;
}

static void sditf_buffree_work(struct work_struct *work)
{
	struct sditf_work_struct *buffree_work = container_of(work,
							      struct sditf_work_struct,
							      work);
	struct sditf_priv *priv = container_of(buffree_work,
						struct sditf_priv,
						buffree_work);
	struct rkcif_rx_buffer *rx_buf = NULL;
	unsigned long flags;
	LIST_HEAD(local_list);

	spin_lock_irqsave(&priv->cif_dev->buffree_lock, flags);
	list_replace_init(&priv->buf_free_list, &local_list);
	while (!list_empty(&local_list)) {
		rx_buf = list_first_entry(&local_list,
					  struct rkcif_rx_buffer, list_free);
		if (rx_buf) {
			list_del(&rx_buf->list_free);
			rkcif_free_reserved_mem_buf(priv->cif_dev, rx_buf);
			rkcif_free_reserved_mem(rx_buf->shmem.shm_start, rx_buf->shmem.shm_size);
			memset(rx_buf, 0, sizeof(*rx_buf));
			rx_buf->dummy.is_free = true;
		}
	}
	spin_unlock_irqrestore(&priv->cif_dev->buffree_lock, flags);
}

static void sditf_get_hdr_mode(struct sditf_priv *priv)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct rkmodule_hdr_cfg hdr_cfg;
	int ret = 0;

	if (!cif_dev->terminal_sensor.sd)
		rkcif_update_sensor_info(&cif_dev->stream[0]);

	if (cif_dev->terminal_sensor.sd) {
		ret = v4l2_subdev_call(cif_dev->terminal_sensor.sd,
				       core, ioctl,
				       RKMODULE_GET_HDR_CFG,
				       &hdr_cfg);
		if (!ret)
			priv->hdr_cfg = hdr_cfg;
		else
			priv->hdr_cfg.hdr_mode = NO_HDR;
	} else {
		priv->hdr_cfg.hdr_mode = NO_HDR;
	}
}

static int sditf_g_frame_interval(struct v4l2_subdev *sd,
				  struct v4l2_subdev_frame_interval *fi)
{
	struct sditf_priv *priv = to_sditf_priv(sd);
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct v4l2_subdev *sensor_sd;

	if (!cif_dev->terminal_sensor.sd)
		rkcif_update_sensor_info(&cif_dev->stream[0]);

	if (cif_dev->terminal_sensor.sd) {
		sensor_sd = cif_dev->terminal_sensor.sd;
		return v4l2_subdev_call(sensor_sd, video, g_frame_interval, fi);
	}

	return -EINVAL;
}

static int sditf_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
			       struct v4l2_mbus_config *config)
{
	struct sditf_priv *priv = to_sditf_priv(sd);
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct v4l2_subdev *sensor_sd;

	if (!cif_dev->active_sensor)
		rkcif_update_sensor_info(&cif_dev->stream[0]);

	if (cif_dev->active_sensor) {
		sensor_sd = cif_dev->active_sensor->sd;
		return v4l2_subdev_call(sensor_sd, pad, get_mbus_config, 0, config);
	}

	return -EINVAL;
}

static int sditf_get_set_fmt(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *fmt)
{
	struct sditf_priv *priv = to_sditf_priv(sd);
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct v4l2_subdev_selection input_sel;
	struct v4l2_pix_format_mplane pixm;
	const struct cif_output_fmt *out_fmt;
	int ret = -EINVAL;
	bool is_uncompact = false;
	int i = 0;
	int stream_cnt = 0;

	if (!cif_dev->terminal_sensor.sd)
		rkcif_update_sensor_info(&cif_dev->stream[0]);

	if (cif_dev->terminal_sensor.sd) {
		sditf_get_hdr_mode(priv);
		fmt->which = V4L2_SUBDEV_FORMAT_ACTIVE;
		fmt->pad = 0;
		ret = v4l2_subdev_call(cif_dev->terminal_sensor.sd, pad, get_fmt, NULL, fmt);
		if (ret) {
			v4l2_err(&priv->sd,
				 "%s: get sensor format failed\n", __func__);
			return ret;
		}

		input_sel.target = V4L2_SEL_TGT_CROP_BOUNDS;
		input_sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		input_sel.pad = 0;
		ret = v4l2_subdev_call(cif_dev->terminal_sensor.sd,
				       pad, get_selection, NULL,
				       &input_sel);
		if (!ret) {
			fmt->format.width = input_sel.r.width;
			fmt->format.height = input_sel.r.height;
			priv->cap_info.offset_x = input_sel.r.left;
			priv->cap_info.offset_y = input_sel.r.top;
		} else {
			priv->cap_info.offset_x = 0;
			priv->cap_info .offset_y = 0;
		}
		priv->cap_info.width = fmt->format.width;
		priv->cap_info.height = fmt->format.height;
		pixm.pixelformat = rkcif_mbus_pixelcode_to_v4l2(fmt->format.code);
		pixm.width = priv->cap_info.width;
		pixm.height = priv->cap_info.height;

		out_fmt = rkcif_find_output_fmt(NULL, pixm.pixelformat);
		if (priv->toisp_inf.link_mode == TOISP_UNITE &&
		    ((pixm.width / 2 - cif_dev->unite_extend_pixel) * out_fmt->raw_bpp / 8) & 0xf)
			is_uncompact = true;

		v4l2_dbg(1, rkcif_debug, &cif_dev->v4l2_dev,
			"%s, width %d, height %d, hdr mode %d\n",
			__func__, fmt->format.width, fmt->format.height, priv->hdr_cfg.hdr_mode);
		if (priv->hdr_cfg.hdr_mode == NO_HDR ||
		    priv->hdr_cfg.hdr_mode == HDR_COMPR)
			stream_cnt = 1;
		else if (priv->hdr_cfg.hdr_mode == HDR_X2)
			stream_cnt = 2;
		else if (priv->hdr_cfg.hdr_mode == HDR_X3)
			stream_cnt = 3;
		for (i = 0; i < stream_cnt; i++) {
			if (priv->toisp_inf.link_mode == TOISP_UNITE) {
				if (is_uncompact) {
					cif_dev->stream[i].is_compact = false;
					cif_dev->stream[i].is_high_align = true;
				} else {
					cif_dev->stream[i].is_compact = true;
				}
			}
			rkcif_set_fmt(&cif_dev->stream[i], &pixm, false);
		}
	} else {
		if (priv->sensor_sd) {
			fmt->which = V4L2_SUBDEV_FORMAT_ACTIVE;
			fmt->pad = 0;
			ret = v4l2_subdev_call(priv->sensor_sd, pad, get_fmt, NULL, fmt);
			if (ret) {
				v4l2_err(&priv->sd,
					 "%s: get sensor format failed\n", __func__);
				return ret;
			}

			input_sel.target = V4L2_SEL_TGT_CROP_BOUNDS;
			input_sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			input_sel.pad = 0;
			ret = v4l2_subdev_call(priv->sensor_sd,
					       pad, get_selection, NULL,
					       &input_sel);
			if (!ret) {
				fmt->format.width = input_sel.r.width;
				fmt->format.height = input_sel.r.height;
			}
			priv->cap_info.width = fmt->format.width;
			priv->cap_info.height = fmt->format.height;
			pixm.pixelformat = rkcif_mbus_pixelcode_to_v4l2(fmt->format.code);
			pixm.width = priv->cap_info.width;
			pixm.height = priv->cap_info.height;
		} else {
			fmt->which = V4L2_SUBDEV_FORMAT_ACTIVE;
			fmt->pad = 0;
			fmt->format.code = MEDIA_BUS_FMT_SBGGR10_1X10;
			fmt->format.width = 640;
			fmt->format.height = 480;
		}
	}

	return 0;
}

static int sditf_init_buf(struct sditf_priv *priv)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	int ret = 0;

	mutex_lock(&cif_dev->hw_dev->switch_mutex_lock[cif_dev->switch_info.host_idx]);
	if (cif_dev->switch_info.is_use_switch) {
		if (cif_dev->switch_info.is_init_buf ||
		    cif_dev->switch_info.switch_dev->switch_info.is_init_buf) {
			mutex_unlock(&cif_dev->hw_dev->switch_mutex_lock[cif_dev->switch_info.host_idx]);
			return 0;
		}
		cif_dev->switch_info.is_init_buf = true;
	}

	if (priv->hdr_cfg.hdr_mode == HDR_X2) {
		if (priv->mode.rdbk_mode == RKISP_VICAP_RDBK_AUTO) {
			if (cif_dev->is_thunderboot)
				cif_dev->resmem_size /= 2;
			ret = rkcif_init_rx_buf(&cif_dev->stream[0], priv->buf_num);
			if (cif_dev->is_thunderboot)
				cif_dev->resmem_pa += cif_dev->resmem_size;
			ret |= rkcif_init_rx_buf(&cif_dev->stream[1], priv->buf_num);
		} else {
			ret = rkcif_init_rx_buf(&cif_dev->stream[0], priv->buf_num);
			if (priv->mode.rdbk_mode ==  RKISP_VICAP_ONLINE_UNITE)
				ret |= rkcif_init_rx_buf(&cif_dev->stream[1], priv->buf_num);
		}
	} else if (priv->hdr_cfg.hdr_mode == HDR_X3) {
		if (priv->mode.rdbk_mode == RKISP_VICAP_RDBK_AUTO) {
			if (cif_dev->is_thunderboot)
				cif_dev->resmem_size /= 3;
			ret = rkcif_init_rx_buf(&cif_dev->stream[0], priv->buf_num);
			if (cif_dev->is_thunderboot)
				cif_dev->resmem_pa += cif_dev->resmem_size;
			ret |= rkcif_init_rx_buf(&cif_dev->stream[1], priv->buf_num);
			if (cif_dev->is_thunderboot)
				cif_dev->resmem_pa += cif_dev->resmem_size;
			ret |= rkcif_init_rx_buf(&cif_dev->stream[2], priv->buf_num);
		} else {
			ret = rkcif_init_rx_buf(&cif_dev->stream[0], priv->buf_num);
			ret |= rkcif_init_rx_buf(&cif_dev->stream[1], priv->buf_num);
			if (priv->mode.rdbk_mode ==  RKISP_VICAP_ONLINE_UNITE)
				ret |= rkcif_init_rx_buf(&cif_dev->stream[2], priv->buf_num);
		}
	} else {
		if (priv->mode.rdbk_mode == RKISP_VICAP_RDBK_AUTO ||
		    priv->mode.rdbk_mode ==  RKISP_VICAP_ONLINE_UNITE)
			ret = rkcif_init_rx_buf(&cif_dev->stream[0], priv->buf_num);
		else
			ret = -EINVAL;
	}
	priv->is_buf_init = true;
	mutex_unlock(&cif_dev->hw_dev->switch_mutex_lock[cif_dev->switch_info.host_idx]);
	return ret;
}

static void sditf_free_buf(struct sditf_priv *priv)
{
	struct rkcif_device *cif_dev = priv->cif_dev;

	if (priv->hdr_cfg.hdr_mode == HDR_X2) {
		rkcif_free_rx_buf(&cif_dev->stream[0], cif_dev->stream[0].rx_buf_num);
		rkcif_free_rx_buf(&cif_dev->stream[1], cif_dev->stream[1].rx_buf_num);
	} else if (priv->hdr_cfg.hdr_mode == HDR_X3) {
		rkcif_free_rx_buf(&cif_dev->stream[0], cif_dev->stream[0].rx_buf_num);
		rkcif_free_rx_buf(&cif_dev->stream[1], cif_dev->stream[1].rx_buf_num);
		rkcif_free_rx_buf(&cif_dev->stream[2], cif_dev->stream[2].rx_buf_num);
	} else {
		rkcif_free_rx_buf(&cif_dev->stream[0], cif_dev->stream[0].rx_buf_num);
	}
	if (cif_dev->is_thunderboot) {
		cif_dev->wait_line_cache = 0;
		cif_dev->wait_line = 0;
		cif_dev->wait_line_bak = 0;
		cif_dev->is_thunderboot = false;
	}
	priv->is_buf_init = false;
	if (cif_dev->switch_info.is_use_switch)
		cif_dev->switch_info.is_init_buf = false;
}

static int sditf_get_selection(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_selection *sel)
{
	return -EINVAL;
}

static void sditf_reinit_mode(struct sditf_priv *priv, struct rkisp_vicap_mode *mode)
{
	if (mode->rdbk_mode == RKISP_VICAP_RDBK_AIQ) {
		priv->toisp_inf.link_mode = TOISP_NONE;
	} else {
		if (strstr(mode->name, RKISP0_DEVNAME))
			priv->toisp_inf.link_mode = TOISP0;
		else if (strstr(mode->name, RKISP1_DEVNAME))
			priv->toisp_inf.link_mode = TOISP1;
		else if (strstr(mode->name, RKISP_UNITE_DEVNAME))
			priv->toisp_inf.link_mode = TOISP_UNITE;
		else
			priv->toisp_inf.link_mode = TOISP0;
	}

	v4l2_dbg(1, rkcif_debug, &priv->cif_dev->v4l2_dev,
		 "%s, mode->rdbk_mode %d, mode->name %s, link_mode %d\n",
		 __func__, mode->rdbk_mode, mode->name, priv->toisp_inf.link_mode);
}

#ifdef CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_SETUP
static void sditf_select_sensor_setting_for_thunderboot(struct sditf_priv *priv)
{
	struct rkcif_device *dev = priv->cif_dev;
	struct v4l2_subdev_format fmt;
	struct rk_sensor_setting sensor_setting = {0};
	struct v4l2_subdev_frame_interval fi = {0};
	struct rkmodule_hdr_cfg hdr_cfg;
	int width = 0;
	int height = 0;
	int hdr_mode = 0;
	int max_fps = 0;
	int ret = 0;
	bool is_match = false;

	if (!dev->terminal_sensor.sd)
		rkcif_update_sensor_info(&dev->stream[0]);
	if (dev->terminal_sensor.sd) {
		if (priv->mode.dev_id == 0) {
			width = get_rk_cam_w();
			height = get_rk_cam_h();
			hdr_mode = get_rk_cam_hdr();
			max_fps = get_rk_cam1_max_fps();
		} else {
			width = get_rk_cam2_w();
			height = get_rk_cam2_h();
			hdr_mode = get_rk_cam2_hdr();
			max_fps = get_rk_cam2_max_fps();
		}
		fmt.pad = 0;
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		fmt.reserved[0] = 0;
		fmt.format.field = V4L2_FIELD_NONE;
		ret = v4l2_subdev_call(dev->terminal_sensor.sd, pad, get_fmt, NULL, &fmt);
		if (!ret) {
			if (dev->rdbk_debug)
				v4l2_info(&dev->v4l2_dev,
					  "cmdline get %dx%d@%dfps, hdr_mode %d\n",
					  width, height, max_fps, hdr_mode);
			sensor_setting.fmt = fmt.format.code;
			sensor_setting.width = width;
			sensor_setting.height = height;
			sensor_setting.mode = hdr_mode;
			sensor_setting.fps = max_fps;
			ret = v4l2_subdev_call(dev->terminal_sensor.sd,
			       core, ioctl,
			       RKCIS_CMD_SELECT_SETTING,
			       &sensor_setting);
			if (!ret)
				is_match = true;
		}
		if (!is_match) {
			fmt.format.width = width;
			fmt.format.height = height;
			v4l2_subdev_call(dev->terminal_sensor.sd, pad, set_fmt, NULL, &fmt);
			v4l2_subdev_call(dev->terminal_sensor.sd, video, g_frame_interval, &fi);
			fi.interval.numerator = 1;
			fi.interval.denominator = max_fps;
			v4l2_subdev_call(dev->terminal_sensor.sd, video, s_frame_interval, &fi);
			v4l2_subdev_call(dev->terminal_sensor.sd,
					 core, ioctl,
					 RKMODULE_GET_HDR_CFG,
					 &hdr_cfg);
			hdr_cfg.hdr_mode = hdr_mode;
			v4l2_subdev_call(dev->terminal_sensor.sd,
					 core, ioctl,
					 RKMODULE_SET_HDR_CFG,
					 &hdr_cfg);
		}
	}
}
#endif

static struct rkcif_device *rkcif_get_switch_dev(struct rkcif_device *cif_dev)
{
	int i = 0;

	for (i = 0; i < cif_dev->hw_dev->dev_num; i++) {
		if (cif_dev->switch_info.host_idx == cif_dev->hw_dev->cif_dev[i]->switch_info.host_idx &&
		    cif_dev != cif_dev->hw_dev->cif_dev[i])
			return cif_dev->hw_dev->cif_dev[i];
	}
	return NULL;
}

static int rkcif_init_switch_info(struct rkcif_device *cif_dev)
{
	cif_dev->csi_host_idx = cif_dev->switch_info.host_idx;
	cif_dev->switch_info.is_init = true;
	cif_dev->switch_info.switch_dev = rkcif_get_switch_dev(cif_dev);
	if (!cif_dev->switch_info.switch_dev)
		return -EINVAL;
	if (IS_ERR(cif_dev->switch_info.gpio_pin))
		cif_dev->switch_info.gpio_pin = cif_dev->switch_info.switch_dev->switch_info.gpio_pin;
	return 0;
}

static int rkcif_init_switch_infos(struct rkcif_device *cif_dev)
{
	int ret = 0;

	mutex_lock(&cif_dev->hw_dev->switch_mutex_lock[cif_dev->switch_info.host_idx]);
	if (!cif_dev->switch_info.is_init) {
		cif_dev->switch_info.is_active = true;
		ret = rkcif_init_switch_info(cif_dev);
		if (!ret)
			ret = rkcif_init_switch_info(cif_dev->switch_info.switch_dev);
		if (cif_dev->sditf[0]->mode_src.rdbk_mode > RKISP_VICAP_RDBK_AIQ)
			rkcif_switch_change(cif_dev, !!cif_dev->switch_info.gpio_val);
	}
	mutex_unlock(&cif_dev->hw_dev->switch_mutex_lock[cif_dev->switch_info.host_idx]);
	return ret;
}

static void sditf_enable_immediately(struct sditf_priv *priv);
static long sditf_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sditf_priv *priv = to_sditf_priv(sd);
	struct rkisp_vicap_mode *mode;
	struct v4l2_subdev_format fmt;
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct v4l2_subdev *sensor_sd;
	struct rkcif_exp *exp;
	struct rkcif_effect_exp *effect_exposure;
	struct sditf_time *time;
	struct sditf_gain *gain;
	struct sditf_effect_exp *effect_exp;
	struct rkisp_init_buf *pisp_buf_info = NULL;
	int ret = 0;
	int *on = NULL;
	int *connect_id = NULL;
	int sync_type = NO_SYNC_MODE;

	switch (cmd) {
	case RKISP_VICAP_CMD_MODE:
		mode = (struct rkisp_vicap_mode *)arg;
		if (mode->rdbk_mode == RKISP_VICAP_ONLINE_UNITE &&
		    priv->cif_dev->chip_id < CHIP_RV1103B_CIF)
			return -EINVAL;

		ret = v4l2_subdev_call(cif_dev->terminal_sensor.sd,
				       core, ioctl,
				       RKMODULE_GET_SYNC_MODE,
				       &sync_type);
		if (ret || sync_type == NO_SYNC_MODE)
			mode->input.multi_sync = 0;
		else
			mode->input.multi_sync = 1;
		memcpy(&priv->mode_src, mode, sizeof(*mode));

		if (cif_dev->switch_info.is_use_switch)
			ret = rkcif_init_switch_infos(cif_dev);

		if (cif_dev->is_thunderboot &&
		    cif_dev->is_thunderboot_start) {
			if (mode->rdbk_mode < RKISP_VICAP_RDBK_AIQ)
				cif_dev->is_rdbk_to_online = true;
			else
				cif_dev->is_rdbk_to_online = false;
			return 0;
		}
		mutex_lock(&cif_dev->stream_lock);
		memcpy(&priv->mode, mode, sizeof(*mode));
		mutex_unlock(&cif_dev->stream_lock);
		sditf_reinit_mode(priv, &priv->mode);
		if (priv->is_combine_mode)
			mode->input.merge_num = cif_dev->sditf_cnt;
		else
			mode->input.merge_num = 1;
		mode->input.index = priv->combine_index;
#ifdef CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_SETUP
		if (cif_dev->is_thunderboot)
			sditf_select_sensor_setting_for_thunderboot(priv);
#endif
		return 0;
	case RKISP_VICAP_CMD_INIT_BUF:
		pisp_buf_info = (struct rkisp_init_buf *)arg;
		priv->buf_num = pisp_buf_info->buf_cnt;
		priv->cif_dev->fb_res_bufs = pisp_buf_info->buf_cnt;
		sditf_get_set_fmt(&priv->sd, NULL, &fmt);
		if (pisp_buf_info->hdr_wrap_line <= priv->cap_info.height) {
			priv->hdr_wrap_line = pisp_buf_info->hdr_wrap_line;
			v4l2_dbg(1, rkcif_debug, &cif_dev->v4l2_dev,  "hdr_wrap_line %d\n",
				 priv->hdr_wrap_line);
		} else {
			dev_info(priv->dev, "set hdr_wap_line failed, val %d, max %d\n",
				 pisp_buf_info->hdr_wrap_line, priv->cap_info.height);
		}
		if (!priv->is_buf_init)
			ret = sditf_init_buf(priv);
		return ret;
	case RKMODULE_GET_HDR_CFG:
		if (!cif_dev->terminal_sensor.sd)
			rkcif_update_sensor_info(&cif_dev->stream[0]);

		if (cif_dev->terminal_sensor.sd) {
			sensor_sd = cif_dev->terminal_sensor.sd;
			return v4l2_subdev_call(sensor_sd, core, ioctl, cmd, arg);
		}
		break;
	case RKISP_VICAP_CMD_QUICK_STREAM:
		on = (int *)arg;
		if (*on) {
			rkcif_stream_resume(cif_dev, RKCIF_RESUME_ISP);
		} else {
			rkcif_stream_suspend(cif_dev, RKCIF_RESUME_ISP);
			if (cif_dev->chip_id == CHIP_RV1106_CIF)
				sditf_disable_immediately(priv);
		}
		break;
	case RKISP_VICAP_CMD_SET_RESET:
		if (priv->mode.rdbk_mode < RKISP_VICAP_RDBK_AIQ) {
			cif_dev->is_toisp_reset = true;
			return 0;
		}
		break;
	case RKCIF_CMD_SET_EXPOSURE:
		exp = (struct rkcif_exp *)arg;
		time = kzalloc(sizeof(*time), GFP_KERNEL);
		if (!time) {
			ret = -ENOMEM;
			return ret;
		}
		gain = kzalloc(sizeof(*gain), GFP_KERNEL);
		if (!gain) {
			ret = -ENOMEM;
			kfree(time);
			return ret;
		}
		time->time = exp->time;
		gain->gain = exp->gain;
		mutex_lock(&priv->mutex);
		list_add_tail(&time->list, &priv->time_head);
		list_add_tail(&gain->list, &priv->gain_head);
		mutex_unlock(&priv->mutex);
		if (cif_dev->exp_dbg)
			dev_info(priv->dev, "RKCIF_CMD_SET_EXPOSURE %d\n", ret);
		return ret;
	case RKCIF_CMD_GET_EFFECT_EXPOSURE:
		if (!list_empty(&priv->effect_exp_head)) {
			effect_exp = list_first_entry(&priv->effect_exp_head,
						      struct sditf_effect_exp,
						      list);
			if (effect_exp) {
				effect_exposure = (struct rkcif_effect_exp *)arg;
				mutex_lock(&priv->mutex);
				list_del(&effect_exp->list);
				mutex_unlock(&priv->mutex);
				*effect_exposure = effect_exp->exp;
				kfree(effect_exp);
				effect_exp = NULL;
				if (cif_dev->exp_dbg)
					dev_info(priv->dev, "RKCIF_CMD_GET_EFFECT_EXPOSURE seq %d, time 0x%x, gain 0x%x\n",
						 effect_exposure->sequence, effect_exposure->time, effect_exposure->gain);
			}
		} else {
			ret = -EINVAL;
		}
		return ret;
	case RKCIF_CMD_GET_CONNECT_ID:
		connect_id = (int *)arg;
		*connect_id = priv->connect_id;
		return ret;
	case RKISP_VICAP_CMD_HW_LINK:
		on = (int *)arg;
		if (*on) {
			sditf_enable_immediately(priv);
		} else {
			if (priv->mode.rdbk_mode != RKISP_VICAP_ONLINE_MULTI &&
			    priv->mode.rdbk_mode != RKISP_VICAP_ONLINE_UNITE)
				sditf_disable_immediately(priv);
		}
		return 0;
	case RKISP_VICAP_CMD_MULTI_ONLINE:
		if (*(int *)arg)
			priv->is_multi_online = true;
		else
			priv->is_multi_online = false;
		return 0;
	default:
		break;
	}

	return -EINVAL;
}

#ifdef CONFIG_COMPAT
static long sditf_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct sditf_priv *priv = to_sditf_priv(sd);
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct v4l2_subdev *sensor_sd;
	struct rkisp_vicap_mode *mode;
	struct rkmodule_hdr_cfg	*hdr_cfg;
	struct rkcif_exp *exp;
	struct rkcif_effect_exp *effect_expsure;
	int buf_num;
	int ret = 0;
	int on;
	int connect_id = 0;

	switch (cmd) {
	case RKISP_VICAP_CMD_MODE:
		mode = kzalloc(sizeof(*mode), GFP_KERNEL);
		if (!mode) {
			ret = -ENOMEM;
			return ret;
		}
		if (copy_from_user(mode, up, sizeof(*mode))) {
			kfree(mode);
			return -EFAULT;
		}
		ret = sditf_ioctl(sd, cmd, mode);
		kfree(mode);
		return ret;
	case RKISP_VICAP_CMD_INIT_BUF:
		if (copy_from_user(&buf_num, up, sizeof(int)))
			return -EFAULT;
		ret = sditf_ioctl(sd, cmd, &buf_num);
		return ret;
	case RKMODULE_GET_HDR_CFG:
		hdr_cfg = kzalloc(sizeof(*hdr_cfg), GFP_KERNEL);
		if (!hdr_cfg) {
			ret = -ENOMEM;
			return ret;
		}
		ret = sditf_ioctl(sd, cmd, hdr_cfg);
		if (!ret) {
			ret = copy_to_user(up, hdr_cfg, sizeof(*hdr_cfg));
			if (ret)
				ret = -EFAULT;
		}
		kfree(hdr_cfg);
		return ret;
	case RKCIF_CMD_SET_EXPOSURE:
		exp = kzalloc(sizeof(*exp), GFP_KERNEL);
		if (!exp) {
			ret = -ENOMEM;
			return ret;
		}
		if (copy_from_user(exp, up, sizeof(*exp))) {
			kfree(exp);
			return -EFAULT;
		}
		ret = sditf_ioctl(sd, cmd, exp);
		kfree(exp);
		return ret;
	case RKCIF_CMD_GET_EFFECT_EXPOSURE:
		effect_expsure = kzalloc(sizeof(*effect_expsure), GFP_KERNEL);
		if (!effect_expsure) {
			ret = -ENOMEM;
			return ret;
		}
		ret = sditf_ioctl(sd, cmd, effect_expsure);
		if (!ret) {
			ret = copy_to_user(up, effect_expsure, sizeof(*effect_expsure));
			if (ret)
				ret = -EFAULT;
		}
		kfree(effect_expsure);
		return ret;
	case RKCIF_CMD_GET_CONNECT_ID:
		ret = sditf_ioctl(sd, cmd, &connect_id);
		if (!ret) {
			ret = copy_to_user(up, &connect_id, sizeof(int));
			if (ret)
				ret = -EFAULT;
		}
		return ret;
	case RKISP_VICAP_CMD_QUICK_STREAM:
		if (copy_from_user(&on, up, sizeof(int)))
			return -EFAULT;
		ret = sditf_ioctl(sd, cmd, &on);
		return ret;
	case RKISP_VICAP_CMD_SET_RESET:
		ret = sditf_ioctl(sd, cmd, NULL);
		return ret;
	case RKISP_VICAP_CMD_HW_LINK:
		if (copy_from_user(&on, up, sizeof(int)))
			return -EFAULT;
		ret = sditf_ioctl(sd, cmd, &on);
		return ret;
	default:
		break;
	}

	if (!cif_dev->terminal_sensor.sd)
		rkcif_update_sensor_info(&cif_dev->stream[0]);

	if (cif_dev->terminal_sensor.sd) {
		sensor_sd = cif_dev->terminal_sensor.sd;
		return v4l2_subdev_call(sensor_sd, core, compat_ioctl32, cmd, arg);
	}

	return -EINVAL;
}
#endif

static int sditf_channel_enable_rv1103b(struct sditf_priv *priv, int user)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct rkmodule_capture_info *capture_info = &cif_dev->channels[0].capture_info;
	unsigned int ch0 = 0, ch1 = 0, ch2 = 0;
	unsigned int ctrl_ch0 = 0;
	unsigned int ctrl_ch1 = 0;
	unsigned int ctrl_ch2 = 0;
	unsigned int int_en = 0;
	unsigned int offset_x = priv->cap_info.offset_x;
	unsigned int offset_y = priv->cap_info.offset_y;
	unsigned int width = priv->cap_info.width;
	unsigned int height = priv->cap_info.height;
	int csi_idx = cif_dev->csi_host_idx;
	unsigned int read_ctrl_ch0 = 0;
	unsigned int read_ctrl_ch1 = 0;
	unsigned int read_ctrl_ch2 = 0;

	if (capture_info->mode == RKMODULE_MULTI_DEV_COMBINE_ONE &&
	    priv->toisp_inf.link_mode == TOISP_UNITE) {
		if (capture_info->multi_dev.dev_num != 2 ||
		    capture_info->multi_dev.pixel_offset != cif_dev->unite_extend_pixel) {
			v4l2_err(&cif_dev->v4l2_dev,
				 "param error of online mode, combine dev num %d, offset %d\n",
				 capture_info->multi_dev.dev_num,
				 capture_info->multi_dev.pixel_offset);
			return -EINVAL;
		}
		csi_idx = capture_info->multi_dev.dev_idx[user];
	}

	if (priv->hdr_cfg.hdr_mode == NO_HDR ||
	    priv->hdr_cfg.hdr_mode == HDR_COMPR) {
		if (cif_dev->inf_id == RKCIF_MIPI_LVDS)
			ch0 = csi_idx * 4;
		else
			ch0 = 24;//dvp
		ctrl_ch0 = (ch0 << 3) | 0x1;
		if (user == 0)
			int_en = CIF_TOISP0_FS_RK3576(0) | CIF_TOISP0_FE_RK3576(0);
		priv->toisp_inf.ch_info[0].is_valid = true;
		priv->toisp_inf.ch_info[0].id = ch0;
	} else if (priv->hdr_cfg.hdr_mode == HDR_X2) {
		ch0 = cif_dev->csi_host_idx * 4 + 1;
		ch1 = cif_dev->csi_host_idx * 4;
		ctrl_ch0 = (ch0 << 3) | 0x1;
		ctrl_ch1 = (ch1 << 3) | 0x1;
		if (cif_dev->chip_id < CHIP_RK3576_CIF) {
			if (user == 0)
				int_en = CIF_TOISP0_FS(0) | CIF_TOISP0_FS(1) |
					 CIF_TOISP0_FE(0) | CIF_TOISP0_FE(1);
			else
				int_en = CIF_TOISP1_FS(0) | CIF_TOISP1_FS(1) |
					 CIF_TOISP1_FE(0) | CIF_TOISP1_FE(1);
		} else {
			if (user == 0)
				int_en = CIF_TOISP0_FS_RK3576(0) | CIF_TOISP0_FS_RK3576(1) |
					 CIF_TOISP0_FE_RK3576(0) | CIF_TOISP0_FE_RK3576(1);
		}
		priv->toisp_inf.ch_info[0].is_valid = true;
		priv->toisp_inf.ch_info[0].id = ch0;
		priv->toisp_inf.ch_info[1].is_valid = true;
		priv->toisp_inf.ch_info[1].id = ch1;
	} else if (priv->hdr_cfg.hdr_mode == HDR_X3) {
		ch0 = cif_dev->csi_host_idx * 4 + 2;
		ch1 = cif_dev->csi_host_idx * 4 + 1;
		ch2 = cif_dev->csi_host_idx * 4;
		ctrl_ch0 = (ch0 << 3) | 0x1;
		ctrl_ch1 = (ch1 << 3) | 0x1;
		ctrl_ch2 = (ch2 << 3) | 0x1;

		if (user == 0)
			int_en = CIF_TOISP0_FS_RK3576(0) | CIF_TOISP0_FS_RK3576(1) |
				 CIF_TOISP0_FS_RK3576(2) | CIF_TOISP0_FE_RK3576(0) |
				 CIF_TOISP0_FE_RK3576(1) | CIF_TOISP0_FE_RK3576(2);

		priv->toisp_inf.ch_info[0].is_valid = true;
		priv->toisp_inf.ch_info[0].id = ch0;
		priv->toisp_inf.ch_info[1].is_valid = true;
		priv->toisp_inf.ch_info[1].id = ch1;
		priv->toisp_inf.ch_info[2].is_valid = true;
		priv->toisp_inf.ch_info[2].id = ch2;
	}

	if (!width || !height)
		return -EINVAL;

	rkcif_write_register_or(cif_dev, CIF_REG_GLB_INTEN, int_en);

	if (user == 0) {
		if (priv->mode.rdbk_mode == RKISP_VICAP_ONLINE_UNITE) {
			width /= 2;
			width += cif_dev->unite_extend_pixel;
		} else if (priv->toisp_inf.link_mode == TOISP_UNITE) {
			width = priv->cap_info.width / 2 + cif_dev->unite_extend_pixel;
		}
		rkcif_write_register(cif_dev, CIF_REG_TOISP0_CTRL, ctrl_ch0);
		rkcif_write_register(cif_dev, CIF_REG_TOISP0_CROP,
			offset_x | (offset_y << 16));
		rkcif_write_register(cif_dev, CIF_REG_TOISP0_SIZE,
			width | (height << 16));
		if (priv->hdr_cfg.hdr_mode != NO_HDR &&
		    priv->hdr_cfg.hdr_mode != HDR_COMPR) {
			rkcif_write_register(cif_dev, CIF_REG_TOISP0_CH1_CTRL, ctrl_ch1);
			rkcif_write_register(cif_dev, CIF_REG_TOISP0_CH1_CROP,
				offset_x | (offset_y << 16));
			rkcif_write_register(cif_dev, CIF_REG_TOISP0_CH1_SIZE,
				width | (height << 16));
		}
		if (priv->hdr_cfg.hdr_mode == HDR_X3) {
			rkcif_write_register(cif_dev, CIF_REG_TOISP0_CH2_CTRL, ctrl_ch2);
			rkcif_write_register(cif_dev, CIF_REG_TOISP0_CH2_CROP,
				offset_x | (offset_y << 16));
			rkcif_write_register(cif_dev, CIF_REG_TOISP0_CH2_SIZE,
				width | (height << 16));
		}
	}
	read_ctrl_ch0 = rkcif_read_register(cif_dev, CIF_REG_TOISP0_CTRL);
	v4l2_dbg(3, rkcif_debug, &cif_dev->v4l2_dev,  "isp%d, toisp ch0 %d, width %d, height %d, reg w:0x%x r:0x%x\n",
		 user, ch0, width, height, ctrl_ch0, read_ctrl_ch0);
	if (priv->hdr_cfg.hdr_mode != NO_HDR) {
		read_ctrl_ch1 = rkcif_read_register(cif_dev, CIF_REG_TOISP0_CH1_CTRL);
		v4l2_dbg(3, rkcif_debug, &cif_dev->v4l2_dev, "isp%d, toisp ch1 %d, width %d, height %d, reg w:0x%x r:0x%x\n",
			 user, ch1, width, height, ctrl_ch1, read_ctrl_ch1);
	}
	if (priv->hdr_cfg.hdr_mode == HDR_X3) {
		read_ctrl_ch2 = rkcif_read_register(cif_dev, CIF_REG_TOISP0_CH2_CTRL);
		v4l2_dbg(3, rkcif_debug, &cif_dev->v4l2_dev, "isp%d, toisp ch2 %d, width %d, height %d, reg w:0x%x r:0x%x\n",
			 user, ch2, width, height, ctrl_ch2, read_ctrl_ch2);
	}
	return 0;
}

static int sditf_channel_enable(struct sditf_priv *priv, int user)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct rkmodule_capture_info *capture_info = &cif_dev->channels[0].capture_info;
	unsigned int ch0 = 0, ch1 = 0, ch2 = 0;
	unsigned int ctrl_val = 0;
	unsigned int int_en = 0;
	unsigned int offset_x = 0;
	unsigned int offset_y = 0;
	unsigned int width = priv->cap_info.width;
	unsigned int height = priv->cap_info.height;
	int csi_idx = cif_dev->csi_host_idx;

	if (capture_info->mode == RKMODULE_MULTI_DEV_COMBINE_ONE &&
	    priv->toisp_inf.link_mode == TOISP_UNITE) {
		if (capture_info->multi_dev.dev_num != 2 ||
		    capture_info->multi_dev.pixel_offset != cif_dev->unite_extend_pixel) {
			v4l2_err(&cif_dev->v4l2_dev,
				 "param error of online mode, combine dev num %d, offset %d\n",
				 capture_info->multi_dev.dev_num,
				 capture_info->multi_dev.pixel_offset);
			return -EINVAL;
		}
		csi_idx = capture_info->multi_dev.dev_idx[user];
	}

	if (priv->hdr_cfg.hdr_mode == NO_HDR ||
	    priv->hdr_cfg.hdr_mode == HDR_COMPR) {
		if (cif_dev->inf_id == RKCIF_MIPI_LVDS)
			ch0 = csi_idx * 4;
		else
			ch0 = 24;//dvp
		ctrl_val = (ch0 << 3) | 0x1;
		if (cif_dev->chip_id < CHIP_RK3576_CIF) {
			if (user == 0)
				int_en = CIF_TOISP0_FS(0) | CIF_TOISP0_FE(0);
			else
				int_en = CIF_TOISP1_FS(0) | CIF_TOISP1_FE(0);
		} else {
			if (user == 0)
				int_en = CIF_TOISP0_FS_RK3576(0) | CIF_TOISP0_FE_RK3576(0);
		}
		priv->toisp_inf.ch_info[0].is_valid = true;
		priv->toisp_inf.ch_info[0].id = ch0;
	} else if (priv->hdr_cfg.hdr_mode == HDR_X2) {
		ch0 = cif_dev->csi_host_idx * 4 + 1;
		ch1 = cif_dev->csi_host_idx * 4;
		ctrl_val = (ch0 << 3) | 0x1;
		ctrl_val |= (ch1 << 11) | 0x100;
		if (cif_dev->chip_id < CHIP_RK3576_CIF) {
			if (user == 0)
				int_en = CIF_TOISP0_FS(0) | CIF_TOISP0_FS(1) |
					 CIF_TOISP0_FE(0) | CIF_TOISP0_FE(1);
			else
				int_en = CIF_TOISP1_FS(0) | CIF_TOISP1_FS(1) |
					 CIF_TOISP1_FE(0) | CIF_TOISP1_FE(1);
		} else {
			if (user == 0)
				int_en = CIF_TOISP0_FS_RK3576(0) | CIF_TOISP0_FS_RK3576(1) |
					 CIF_TOISP0_FE_RK3576(0) | CIF_TOISP0_FE_RK3576(1);
		}
		priv->toisp_inf.ch_info[0].is_valid = true;
		priv->toisp_inf.ch_info[0].id = ch0;
		priv->toisp_inf.ch_info[1].id = ch1;
	} else if (priv->hdr_cfg.hdr_mode == HDR_X3) {
		ch0 = cif_dev->csi_host_idx * 4 + 2;
		ch1 = cif_dev->csi_host_idx * 4 + 1;
		ch2 = cif_dev->csi_host_idx * 4;
		ctrl_val = (ch0 << 3) | 0x1;
		ctrl_val |= (ch1 << 11) | 0x100;
		ctrl_val |= (ch2 << 19) | 0x10000;
		if (cif_dev->chip_id < CHIP_RK3576_CIF) {
			if (user == 0)
				int_en = CIF_TOISP0_FS(0) | CIF_TOISP0_FS(1) | CIF_TOISP0_FS(2) |
					 CIF_TOISP0_FE(0) | CIF_TOISP0_FE(1) | CIF_TOISP0_FE(2);
			else
				int_en = CIF_TOISP1_FS(0) | CIF_TOISP1_FS(1) | CIF_TOISP1_FS(2) |
					 CIF_TOISP1_FE(0) | CIF_TOISP1_FE(1) | CIF_TOISP1_FE(2);
		} else {
			if (user == 0)
				int_en = CIF_TOISP0_FS_RK3576(0) | CIF_TOISP0_FS_RK3576(1) |
					 CIF_TOISP0_FS_RK3576(2) | CIF_TOISP0_FE_RK3576(0) |
					 CIF_TOISP0_FE_RK3576(1) | CIF_TOISP0_FE_RK3576(2);
		}
		priv->toisp_inf.ch_info[0].is_valid = true;
		priv->toisp_inf.ch_info[0].id = ch0;
		priv->toisp_inf.ch_info[1].id = ch1;
		priv->toisp_inf.ch_info[2].id = ch2;
	}
	if (cif_dev->chip_id > CHIP_RK3562_CIF)
		ctrl_val |= BIT(28);
	if (user == 0) {
		if (priv->toisp_inf.link_mode == TOISP_UNITE)
			width = priv->cap_info.width / 2 + cif_dev->unite_extend_pixel;
		rkcif_write_register(cif_dev, CIF_REG_TOISP0_CTRL, ctrl_val);
		if (width && height) {
			rkcif_write_register(cif_dev, CIF_REG_TOISP0_CROP,
				offset_x | (offset_y << 16));
			rkcif_write_register(cif_dev, CIF_REG_TOISP0_SIZE,
				width | (height << 16));
		} else {
			return -EINVAL;
		}
	} else {
		if (priv->toisp_inf.link_mode == TOISP_UNITE) {
			if (capture_info->mode == RKMODULE_MULTI_DEV_COMBINE_ONE)
				offset_x = 0;
			else
				offset_x = priv->cap_info.width / 2 - cif_dev->unite_extend_pixel;
			width = priv->cap_info.width / 2 + cif_dev->unite_extend_pixel;
		}
		rkcif_write_register(cif_dev, CIF_REG_TOISP1_CTRL, ctrl_val);
		if (width && height) {
			rkcif_write_register(cif_dev, CIF_REG_TOISP1_CROP,
				offset_x | (offset_y << 16));
			rkcif_write_register(cif_dev, CIF_REG_TOISP1_SIZE,
				width | (height << 16));
		} else {
			return -EINVAL;
		}
	}
	v4l2_dbg(3, rkcif_debug, &cif_dev->v4l2_dev, "isp%d, toisp ch0 %d, width %d, height %d, reg 0x%x\n",
		 user, ch0, width, height, ctrl_val);
#if IS_ENABLED(CONFIG_CPU_RV1106)
	rv1106_sdmmc_get_lock();
#endif
	rkcif_write_register_or(cif_dev, CIF_REG_GLB_INTEN, int_en);
#if IS_ENABLED(CONFIG_CPU_RV1106)
	rv1106_sdmmc_put_lock();
#endif
	return 0;
}

static void sditf_channel_disable(struct sditf_priv *priv, int user)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	u32 ctrl_val = 0x10101;

	if (user == 0)
		rkcif_write_register_and(cif_dev, CIF_REG_TOISP0_CTRL, ~ctrl_val);
	else
		rkcif_write_register_and(cif_dev, CIF_REG_TOISP1_CTRL, ~ctrl_val);
}

static void sditf_channel_disable_rv1103b(struct sditf_priv *priv, int user)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	u32 ctrl_val = 0x1;
	u32 read_ctrl_ch0 = 0;

	rkcif_write_register_and(cif_dev, CIF_REG_TOISP0_CTRL, ~ctrl_val);
	read_ctrl_ch0 = rkcif_read_register(cif_dev, CIF_REG_TOISP0_CTRL);
	v4l2_dbg(3, rkcif_debug, &cif_dev->v4l2_dev, "isp%d, toisp disable reg w_and:0x%x r:0x%x\n",
		 user, ~ctrl_val, read_ctrl_ch0);
	if (priv->hdr_cfg.hdr_mode != NO_HDR)
		rkcif_write_register_and(cif_dev, CIF_REG_TOISP0_CH1_CTRL, ~ctrl_val);
	if (priv->hdr_cfg.hdr_mode == HDR_X3)
		rkcif_write_register_and(cif_dev, CIF_REG_TOISP0_CH2_CTRL, ~ctrl_val);
}

static void rkcif_release_unnecessary_buf_for_online(struct rkcif_stream *stream,
						     struct rkcif_rx_buffer *buf)
{
	struct rkcif_device *dev = stream->cifdev;
	struct sditf_priv *priv = dev->sditf[0];
	struct rkcif_rx_buffer *rx_buf = NULL;
	unsigned long flags;
	int i = 0;

	if (!buf)
		buf = stream->last_buf_toisp;
	spin_lock_irqsave(&priv->cif_dev->buffree_lock, flags);
	for (i = 0; i < stream->rx_buf_num; i++) {
		rx_buf = &stream->rx_buf[i];
		if (rx_buf && (!rx_buf->dummy.is_free) && rx_buf != buf) {
			list_add_tail(&rx_buf->list_free, &priv->buf_free_list);
			stream->total_buf_num--;
			atomic_dec(&stream->buf_cnt);
		}
	}
	spin_unlock_irqrestore(&priv->cif_dev->buffree_lock, flags);
	schedule_work(&priv->buffree_work.work);
}

void sditf_change_to_online(struct sditf_priv *priv)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct rkcif_stream *cur_stream = NULL;
	int i = 0;
	int stream_cnt = 0;

	priv->mode = priv->mode_src;
	if (priv->mode.rdbk_mode != RKISP_VICAP_ONLINE_UNITE &&
	    priv->mode.rdbk_mode != RKISP_VICAP_ONLINE_MULTI)
		sditf_enable_immediately(priv);

	if (cif_dev->is_thunderboot) {
		if (priv->hdr_cfg.hdr_mode == HDR_X2) {
			cur_stream = &cif_dev->stream[1];
			cif_dev->stream[0].is_line_wake_up = false;
			cif_dev->stream[1].is_line_wake_up = false;
			stream_cnt = 1;
		} else if (priv->hdr_cfg.hdr_mode == HDR_X3) {
			cur_stream = &cif_dev->stream[2];
			cif_dev->stream[0].is_line_wake_up = false;
			cif_dev->stream[1].is_line_wake_up = false;
			cif_dev->stream[2].is_line_wake_up = false;
			stream_cnt = 2;
		} else {
			cur_stream = &cif_dev->stream[0];
			cif_dev->stream[0].is_line_wake_up = false;
			stream_cnt = 0;
		}

		if (priv->mode.rdbk_mode == RKISP_VICAP_ONLINE_UNITE)
			cur_stream->is_m_online_fb_res = true;
		rkcif_free_rx_buf(cur_stream, cur_stream->rx_buf_num);

		cif_dev->wait_line_cache = 0;
		cif_dev->wait_line = 0;
		cif_dev->wait_line_bak = 0;
		cif_dev->is_thunderboot = false;

		if (priv->mode.rdbk_mode == RKISP_VICAP_ONLINE_UNITE)
			rkcif_reinit_right_half_config(cur_stream);
		for (i = 0; i < stream_cnt; i++)
			rkcif_release_unnecessary_buf_for_online(&cif_dev->stream[i],
								 cif_dev->stream[i].curr_buf_toisp);
	}
}

void sditf_disable_immediately(struct sditf_priv *priv)
{
	if (priv->toisp_inf.link_mode == TOISP0) {
		if (priv->cif_dev->chip_id >= CHIP_RV1103B_CIF)
			sditf_channel_disable_rv1103b(priv, 0);
		else
			sditf_channel_disable(priv, 0);
	} else if (priv->toisp_inf.link_mode == TOISP1) {
		if (priv->cif_dev->chip_id >= CHIP_RV1103B_CIF)
			sditf_channel_disable_rv1103b(priv, 1);
		else
			sditf_channel_disable(priv, 1);
	} else if (priv->toisp_inf.link_mode == TOISP_UNITE) {
		if (priv->cif_dev->chip_id >= CHIP_RV1103B_CIF) {
			sditf_channel_disable_rv1103b(priv, 0);
		} else {
			sditf_channel_disable(priv, 0);
			if (priv->cif_dev->chip_id == CHIP_RK3588_CIF)
				sditf_channel_disable(priv, 1);
		}
	}
	if (priv->cif_dev->switch_info.is_use_switch)
		priv->cif_dev->switch_info.is_active = false;
}

static void sditf_enable_immediately(struct sditf_priv *priv)
{
	if (priv->toisp_inf.link_mode == TOISP0) {
		if (priv->cif_dev->chip_id >= CHIP_RV1103B_CIF)
			sditf_channel_enable_rv1103b(priv, 0);
		else
			sditf_channel_enable(priv, 0);
	} else if (priv->toisp_inf.link_mode == TOISP1) {
		if (priv->cif_dev->chip_id >= CHIP_RV1103B_CIF)
			sditf_channel_enable_rv1103b(priv, 1);
		else
			sditf_channel_enable(priv, 1);
	} else if (priv->toisp_inf.link_mode == TOISP_UNITE) {
		if (priv->cif_dev->chip_id >= CHIP_RV1103B_CIF) {
			sditf_channel_enable_rv1103b(priv, 0);
		} else {
			sditf_channel_enable(priv, 0);
			if (priv->cif_dev->chip_id == CHIP_RK3588_CIF)
				sditf_channel_enable(priv, 1);
		}
	}
	if (priv->cif_dev->switch_info.is_use_switch) {
		rkcif_switch_change(priv->cif_dev, !!priv->cif_dev->switch_info.gpio_val);
		priv->cif_dev->switch_info.is_active = true;
	}
	priv->is_toisp_off = false;
}

static int sditf_start_stream(struct sditf_priv *priv)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct v4l2_subdev_format fmt;
	unsigned int mode = RKCIF_STREAM_MODE_TOISP;
	int stream_cnt = 0;
	int i = 0;

	sditf_get_set_fmt(&priv->sd, NULL, &fmt);
	if (priv->mode.rdbk_mode == RKISP_VICAP_ONLINE) {
		sditf_enable_immediately(priv);
		mode = RKCIF_STREAM_MODE_TOISP;
	} else if (priv->mode.rdbk_mode == RKISP_VICAP_RDBK_AUTO) {
		mode = RKCIF_STREAM_MODE_TOISP_RDBK;
	}

	if (priv->hdr_cfg.hdr_mode == NO_HDR ||
	    priv->hdr_cfg.hdr_mode == HDR_COMPR)
		stream_cnt = 1;
	else if (priv->hdr_cfg.hdr_mode == HDR_X2)
		stream_cnt = 2;
	else if (priv->hdr_cfg.hdr_mode == HDR_X3)
		stream_cnt = 3;

	cif_dev->is_thunderboot_start = true;
	for (i = 0; i < stream_cnt; i++)
		rkcif_do_start_stream(&cif_dev->stream[i], mode);
	INIT_LIST_HEAD(&priv->buf_free_list);
	return 0;
}

static int sditf_stop_stream(struct sditf_priv *priv)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	unsigned int mode = RKCIF_STREAM_MODE_TOISP;
	struct rkcif_hw *hw_dev = cif_dev->hw_dev;
	int stream_cnt = 0;
	int i = 0;
	bool toisp_off = true;

	if (priv->mode.rdbk_mode == RKISP_VICAP_ONLINE)
		mode = RKCIF_STREAM_MODE_TOISP;
	else if (priv->mode.rdbk_mode == RKISP_VICAP_RDBK_AUTO)
		mode = RKCIF_STREAM_MODE_TOISP_RDBK;

	if (priv->hdr_cfg.hdr_mode == NO_HDR ||
	    priv->hdr_cfg.hdr_mode == HDR_COMPR)
		stream_cnt = 1;
	else if (priv->hdr_cfg.hdr_mode == HDR_X2)
		stream_cnt = 2;
	else if (priv->hdr_cfg.hdr_mode == HDR_X3)
		stream_cnt = 3;

	for (i = 0; i < stream_cnt; i++)
		rkcif_do_stop_stream(&cif_dev->stream[i], mode);

	mutex_lock(&hw_dev->dev_lock);
	for (i = 0; i < hw_dev->dev_num; i++) {
		if (atomic_read(&hw_dev->cif_dev[i]->pipe.stream_cnt) != 0) {
			toisp_off = false;
			break;
		}
	}
	mutex_unlock(&hw_dev->dev_lock);
	if (toisp_off)
		sditf_disable_immediately(priv);
	priv->toisp_inf.ch_info[0].is_valid = false;
	priv->toisp_inf.ch_info[1].is_valid = false;
	priv->toisp_inf.ch_info[2].is_valid = false;
	priv->is_toisp_off = true;
	return 0;
}

static int sditf_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sditf_priv *priv = to_sditf_priv(sd);
	struct rkcif_device *cif_dev = priv->cif_dev;
	int ret = 0;

	if (!on && atomic_dec_return(&priv->stream_cnt))
		return 0;

	if (on && atomic_inc_return(&priv->stream_cnt) > 1)
		return 0;

	if (cif_dev->chip_id >= CHIP_RK3588_CIF) {
		if (priv->mode.rdbk_mode == RKISP_VICAP_RDBK_AIQ)
			return 0;
		v4l2_dbg(1, rkcif_debug, &cif_dev->v4l2_dev,
			"%s, toisp mode %d, hdr %d, stream on %d\n",
			__func__, priv->toisp_inf.link_mode, priv->hdr_cfg.hdr_mode, on);
		if (on) {
			ret = sditf_start_stream(priv);
		} else {
			ret = sditf_stop_stream(priv);
			sditf_free_buf(priv);
			priv->mode.rdbk_mode = RKISP_VICAP_RDBK_AIQ;
		}

	}
	if (on && ret)
		atomic_dec(&priv->stream_cnt);
	return ret;
}

static int sditf_s_power(struct v4l2_subdev *sd, int on)
{
	struct sditf_priv *priv = to_sditf_priv(sd);
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct rkcif_vdev_node *node = &cif_dev->stream[0].vnode;
	int ret = 0;

	if (!on && atomic_dec_return(&priv->power_cnt))
		return 0;

	if (on && atomic_inc_return(&priv->power_cnt) > 1)
		return 0;

	if (on)
		rkcif_update_unite_extend_pixel(cif_dev);
	if (cif_dev->chip_id >= CHIP_RK3588_CIF) {
		v4l2_dbg(1, rkcif_debug, &cif_dev->v4l2_dev,
			"%s, toisp mode %d, hdr %d, set power %d\n",
			__func__, priv->toisp_inf.link_mode, priv->hdr_cfg.hdr_mode, on);
		mutex_lock(&cif_dev->stream_lock);
		if (on) {
			ret = pm_runtime_resume_and_get(cif_dev->dev);
			ret |= v4l2_pipeline_pm_get(&node->vdev.entity);
		} else {
			v4l2_pipeline_pm_put(&node->vdev.entity);
			pm_runtime_put_sync(cif_dev->dev);
			priv->mode.rdbk_mode = RKISP_VICAP_RDBK_AIQ;
		}
		ret |= rkcif_sensor_set_power(&cif_dev->stream[0], on);
		v4l2_dbg(1, rkcif_debug, &node->vdev, "s_power %d, entity use_count %d\n",
			  on, node->vdev.entity.use_count);
		mutex_unlock(&cif_dev->stream_lock);
	}
	return ret;
}

static int sditf_check_toolbuf_return(struct rkcif_stream *stream, struct rkcif_rx_buffer *rx_buf)
{
	struct rkcif_tools_vdev *tools_vdev = stream->tools_vdev;
	unsigned long flags;

	if (tools_vdev) {
		spin_lock_irqsave(&stream->tools_vdev->vbq_lock, flags);

		if (rx_buf->use_cnt)
			rx_buf->use_cnt--;
		if (rx_buf->use_cnt) {
			spin_unlock_irqrestore(&stream->tools_vdev->vbq_lock, flags);
			return false;
		}
		spin_unlock_irqrestore(&stream->tools_vdev->vbq_lock, flags);
	}

	return true;
}

static int sditf_s_rx_buffer(struct v4l2_subdev *sd,
			     void *buf, unsigned int *size)
{
	struct sditf_priv *priv = to_sditf_priv(sd);
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct rkcif_stream *stream = NULL;
	struct rkcif_stream *buf_stream = NULL;
	struct rkisp_rx_buf *dbufs;
	struct rkcif_rx_buffer *rx_buf = NULL;
	unsigned long flags, buffree_flags;
	u32 diff_time = 1000000;
	u32 early_time = 0;
	bool is_free = false;

	if (!buf) {
		v4l2_err(&cif_dev->v4l2_dev, "buf is NULL\n");
		return -EINVAL;
	}

	dbufs = buf;
	if (cif_dev->hdr.hdr_mode == NO_HDR) {
		if (dbufs->type == BUF_SHORT)
			stream = &cif_dev->stream[0];
		else
			return -EINVAL;
	} else if (cif_dev->hdr.hdr_mode == HDR_X2) {
		if (dbufs->type == BUF_SHORT)
			stream = &cif_dev->stream[1];
		else if (dbufs->type == BUF_MIDDLE)
			stream = &cif_dev->stream[0];
		else
			return -EINVAL;
	} else if (cif_dev->hdr.hdr_mode == HDR_X3) {
		if (dbufs->type == BUF_SHORT)
			stream = &cif_dev->stream[2];
		else if (dbufs->type == BUF_MIDDLE)
			stream = &cif_dev->stream[1];
		else if (dbufs->type == BUF_LONG)
			stream = &cif_dev->stream[0];
		else
			return -EINVAL;
	}

	if (!stream)
		return -EINVAL;
	buf_stream = stream;
	if (cif_dev->switch_info.is_use_switch &&
	    cif_dev->switch_info.switch_dev->switch_info.is_init_buf)
		buf_stream = &cif_dev->switch_info.switch_dev->stream[stream->id];

	if (dbufs->sequence == 0 &&
	    stream->thunderboot_skip_interval) {
		spin_lock_irqsave(&stream->vbq_lock, flags);
		cif_dev->is_stop_skip = true;
		spin_unlock_irqrestore(&stream->vbq_lock, flags);
	}

	rx_buf = to_cif_rx_buf(dbufs);
	v4l2_dbg(3, rkcif_debug, &cif_dev->v4l2_dev, "type %d buf back to vicap 0x%x, is_switch %d\n",
		 dbufs->type, (u32)rx_buf->dummy.dma_addr, dbufs->is_switch);
	spin_lock_irqsave(&stream->vbq_lock, flags);
	stream->last_rx_buf_idx = dbufs->sequence + 1;
	atomic_inc(&stream->buf_cnt);

	if (stream->total_buf_num > cif_dev->fb_res_bufs &&
	    cif_dev->is_thunderboot &&
	    (dbufs->sequence > 2) &&
	    (!dbufs->is_switch)) {
		spin_lock_irqsave(&cif_dev->buffree_lock, buffree_flags);
		list_add_tail(&rx_buf->list_free, &priv->buf_free_list);
		spin_unlock_irqrestore(&cif_dev->buffree_lock, buffree_flags);
		atomic_dec(&stream->buf_cnt);
		stream->total_buf_num--;
		schedule_work(&priv->buffree_work.work);
		is_free = true;
	}

	if (!is_free && (!dbufs->is_switch) && stream->state == RKCIF_STATE_STREAMING &&
	    sditf_check_toolbuf_return(stream, rx_buf)) {
		list_add_tail(&rx_buf->list, &buf_stream->rx_buf_head);
		rkcif_assign_check_buffer_update_toisp(stream);
		if (cif_dev->resume_mode != RKISP_RTT_MODE_ONE_FRAME &&
		    (!stream->is_pause_stream)) {
			if (!stream->dma_en) {
				stream->to_en_dma = RKCIF_DMAEN_BY_ISP;
				rkcif_enable_dma_capture(stream, true);
				if (atomic_read(&cif_dev->sensor_off)) {
					atomic_set(&cif_dev->sensor_off, 0);
					cif_dev->sensor_work.on = 1;
					rkcif_dphy_quick_stream(stream->cifdev, cif_dev->sensor_work.on);
					schedule_work(&cif_dev->sensor_work.work);
				}
			}
		}
		if (cif_dev->rdbk_debug) {
			u32 offset = 0;

			offset = rx_buf->dummy.size - stream->pixm.plane_fmt[0].bytesperline * 3;
			memset(rx_buf->dummy.vaddr + offset,
			       0x00, stream->pixm.plane_fmt[0].bytesperline * 3);
			if (cif_dev->is_thunderboot ||
			    cif_dev->is_rtt_suspend ||
			    cif_dev->is_aov_reserved)
				dma_sync_single_for_device(cif_dev->hw_dev->dev,
							   rx_buf->dummy.dma_addr + rx_buf->dummy.size -
							   stream->pixm.plane_fmt[0].bytesperline * 3,
							   stream->pixm.plane_fmt[0].bytesperline * 3,
							   DMA_FROM_DEVICE);
			else
				cif_dev->hw_dev->mem_ops->prepare(rx_buf->dummy.mem_priv);
		}
	}
	spin_unlock_irqrestore(&stream->vbq_lock, flags);

	if (dbufs->is_switch && dbufs->type == BUF_SHORT) {
		if (stream->is_in_vblank || !stream->dma_en) {
			sditf_change_to_online(priv);
			rkcif_modify_line_int(stream, false);
			stream->is_line_inten = false;
		} else {
			stream->is_change_toisp = true;
		}
		v4l2_dbg(3, rkcif_debug, &cif_dev->v4l2_dev,
			 "switch to online mode\n");
	}

	spin_lock_irqsave(&stream->cifdev->stream_spinlock, flags);
	stream->is_finish_single_cap = true;
	if (stream->is_wait_single_cap &&
	    (cif_dev->hdr.hdr_mode == NO_HDR ||
	     (cif_dev->hdr.hdr_mode == HDR_X2 && stream->id == 1) ||
	     (cif_dev->hdr.hdr_mode == HDR_X3 && stream->id == 2))) {
		stream->is_wait_single_cap = false;
		spin_unlock_irqrestore(&stream->cifdev->stream_spinlock, flags);
		rkcif_quick_stream_on(cif_dev, true);
	} else {
		spin_unlock_irqrestore(&stream->cifdev->stream_spinlock, flags);
	}

	if (!cif_dev->is_thunderboot ||
	    cif_dev->is_rdbk_to_online == false)
		return 0;

	if (dbufs->runtime_us && cif_dev->early_line == 0) {
		if (!cif_dev->sensor_linetime)
			cif_dev->sensor_linetime = rkcif_get_linetime(stream);
		cif_dev->isp_runtime_max = dbufs->runtime_us;
		if (cif_dev->is_thunderboot)
			diff_time = 200000;
		else
			diff_time = 1000000;
		if (dbufs->runtime_us * 1000 < cif_dev->sensor_linetime * stream->pixm.height &&
		    dbufs->runtime_us * 1000 + cif_dev->sensor_linetime > diff_time)
			early_time = dbufs->runtime_us * 1000 - diff_time;
		else
			early_time = diff_time;
		cif_dev->early_line = div_u64(early_time, cif_dev->sensor_linetime);
		cif_dev->wait_line_cache = stream->pixm.height - cif_dev->early_line;
		if (cif_dev->rdbk_debug &&
		    dbufs->sequence < 15)
			v4l2_info(&cif_dev->v4l2_dev,
				  "%s, isp runtime %d, line time %d, early_line %d, line_intr_cnt %d, seq %d, type %d, dma_addr %x\n",
				  __func__, dbufs->runtime_us, cif_dev->sensor_linetime,
				  cif_dev->early_line, cif_dev->wait_line_cache,
				  dbufs->sequence, dbufs->type, (u32)rx_buf->dummy.dma_addr);
	} else {
		if (dbufs->runtime_us < cif_dev->isp_runtime_max) {
			cif_dev->isp_runtime_max = dbufs->runtime_us;
			if (cif_dev->is_thunderboot)
				diff_time = 200000;
			else
				diff_time = 1000000;
			if (dbufs->runtime_us * 1000 < cif_dev->sensor_linetime * stream->pixm.height &&
			    dbufs->runtime_us * 1000 + cif_dev->sensor_linetime > diff_time)
				early_time = dbufs->runtime_us * 1000 - diff_time;
			else
				early_time = diff_time;
			cif_dev->early_line = div_u64(early_time, cif_dev->sensor_linetime);
			cif_dev->wait_line_cache = stream->pixm.height - cif_dev->early_line;
		}
		if (cif_dev->rdbk_debug &&
		    dbufs->sequence < 15)
			v4l2_info(&cif_dev->v4l2_dev,
				  "isp runtime %d, seq %d, type %d, early_line %d, dma addr %x\n",
				  dbufs->runtime_us, dbufs->sequence, dbufs->type,
				  cif_dev->early_line, (u32)rx_buf->dummy.dma_addr);
	}
	return 0;
}

static const struct v4l2_subdev_pad_ops sditf_subdev_pad_ops = {
	.set_fmt = sditf_get_set_fmt,
	.get_fmt = sditf_get_set_fmt,
	.get_selection = sditf_get_selection,
	.get_mbus_config = sditf_g_mbus_config,
};

static const struct v4l2_subdev_video_ops sditf_video_ops = {
	.g_frame_interval = sditf_g_frame_interval,
	.s_stream = sditf_s_stream,
	.s_rx_buffer = sditf_s_rx_buffer,
};

static const struct v4l2_subdev_core_ops sditf_core_ops = {
	.subscribe_event = sditf_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
	.ioctl = sditf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sditf_compat_ioctl32,
#endif
	.s_power = sditf_s_power,
};

static const struct v4l2_subdev_ops sditf_subdev_ops = {
	.core = &sditf_core_ops,
	.video = &sditf_video_ops,
	.pad = &sditf_subdev_pad_ops,
};

static int rkcif_sditf_attach_cifdev(struct sditf_priv *sditf)
{
	struct device_node *np;
	struct platform_device *pdev;
	struct rkcif_device *cif_dev;

	np = of_parse_phandle(sditf->dev->of_node, "rockchip,cif", 0);
	if (!np || !of_device_is_available(np)) {
		dev_err(sditf->dev, "failed to get cif dev node\n");
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		dev_err(sditf->dev, "failed to get cif dev from node\n");
		return -ENODEV;
	}

	cif_dev = platform_get_drvdata(pdev);
	if (!cif_dev) {
		dev_err(sditf->dev, "failed attach cif dev\n");
		return -EINVAL;
	}

	cif_dev->sditf[cif_dev->sditf_cnt] = sditf;
	sditf->cif_dev = cif_dev;
	sditf->connect_id = cif_dev->sditf_cnt;
	cif_dev->sditf_cnt++;

	return 0;
}

struct sensor_async_subdev {
	struct v4l2_async_subdev asd;
	struct v4l2_mbus_config mbus;
	int lanes;
};

static int sditf_fwnode_parse(struct device *dev,
					  struct v4l2_fwnode_endpoint *vep,
					  struct v4l2_async_subdev *asd)
{
	struct sensor_async_subdev *s_asd =
			container_of(asd, struct sensor_async_subdev, asd);
	struct v4l2_mbus_config *config = &s_asd->mbus;

	if (vep->base.port != 0) {
		dev_info(dev, "sditf has only parse port 0\n");
		return 0;
	}

	if (vep->bus_type == V4L2_MBUS_CSI2_DPHY ||
	    vep->bus_type == V4L2_MBUS_CSI2_CPHY) {
		config->type = vep->bus_type;
		config->bus.mipi_csi2.flags = vep->bus.mipi_csi2.flags;
		s_asd->lanes = vep->bus.mipi_csi2.num_data_lanes;
	} else if (vep->bus_type == V4L2_MBUS_CCP2) {
		config->type = vep->bus_type;
		s_asd->lanes = vep->bus.mipi_csi1.data_lane;
	} else {
		dev_err(dev, "type is not supported\n");
		return -EINVAL;
	}

	return 0;
}

static int rkcif_sditf_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sditf_priv *priv = container_of(ctrl->handler,
					       struct sditf_priv,
					       ctrl_handler);
	struct v4l2_ctrl *sensor_ctrl = NULL;

	switch (ctrl->id) {
	case V4L2_CID_PIXEL_RATE:
		if (priv->cif_dev->terminal_sensor.sd) {
			sensor_ctrl = v4l2_ctrl_find(priv->cif_dev->terminal_sensor.sd->ctrl_handler, V4L2_CID_PIXEL_RATE);
			if (sensor_ctrl) {
				ctrl->val = v4l2_ctrl_g_ctrl_int64(sensor_ctrl);
				__v4l2_ctrl_s_ctrl_int64(priv->pixel_rate, ctrl->val);
				v4l2_dbg(1, rkcif_debug, &priv->cif_dev->v4l2_dev,
					"%s, %s pixel rate %d\n",
					__func__, priv->cif_dev->terminal_sensor.sd->name, ctrl->val);
				return 0;
			} else {
				return -EINVAL;
			}
		}
		return -EINVAL;
	default:
		return -EINVAL;
	}
}

static const struct v4l2_ctrl_ops rkcif_sditf_ctrl_ops = {
	.g_volatile_ctrl = rkcif_sditf_get_ctrl,
};

void sditf_get_default_exp(struct sditf_priv *sditf)
{
	struct v4l2_ctrl *ctrl = NULL;
	struct rkcif_device *dev = sditf->cif_dev;

	if (dev->terminal_sensor.sd == NULL)
		return;
	ctrl = v4l2_ctrl_find(dev->terminal_sensor.sd->ctrl_handler,
			      V4L2_CID_EXPOSURE);
	if (ctrl)
		sditf->cur_time = ctrl->default_value;
	else
		sditf->cur_time = 16;

	ctrl = v4l2_ctrl_find(dev->terminal_sensor.sd->ctrl_handler,
			      V4L2_CID_ANALOGUE_GAIN);
	if (ctrl)
		sditf->cur_gain = ctrl->default_value;
	else
		sditf->cur_gain = 16;

	if (dev->exp_dbg)
		dev_info(sditf->dev, "get default time 0x%x gain 0x%x\n",
			 sditf->cur_time, sditf->cur_gain);
}

static int sditf_notifier_bound(struct v4l2_async_notifier *notifier,
				 struct v4l2_subdev *subdev,
				 struct v4l2_async_subdev *asd)
{
	struct sditf_priv *sditf = container_of(notifier,
					struct sditf_priv, notifier);
	struct media_entity *source_entity, *sink_entity;
	int ret = 0;

	sditf->sensor_sd = subdev;

	if (sditf->num_sensors == 1) {
		v4l2_err(subdev,
			 "%s: the num of subdev is beyond %d\n",
			 __func__, sditf->num_sensors);
		return -EBUSY;
	}

	if (sditf->sd.entity.pads[0].flags & MEDIA_PAD_FL_SINK) {
		source_entity = &subdev->entity;
		sink_entity = &sditf->sd.entity;

		ret = media_create_pad_link(source_entity,
					    0,
					    sink_entity,
					    0,
					    MEDIA_LNK_FL_ENABLED);
		if (ret)
			v4l2_err(&sditf->sd, "failed to create link for %s\n",
				 sditf->sensor_sd->name);
	}
	sditf->sensor_sd = subdev;
	++sditf->num_sensors;

	v4l2_err(subdev, "Async registered subdev\n");

	return 0;
}

static void sditf_notifier_unbind(struct v4l2_async_notifier *notifier,
				       struct v4l2_subdev *sd,
				       struct v4l2_async_subdev *asd)
{
	struct sditf_priv *sditf = container_of(notifier,
						struct sditf_priv,
						notifier);

	sditf->sensor_sd = NULL;
}

static const struct v4l2_async_notifier_operations sditf_notifier_ops = {
	.bound = sditf_notifier_bound,
	.unbind = sditf_notifier_unbind,
};

static int sditf_subdev_notifier(struct sditf_priv *sditf)
{
	struct v4l2_async_notifier *ntf = &sditf->notifier;
	int ret;

	v4l2_async_nf_init(ntf);

	ret = v4l2_async_nf_parse_fwnode_endpoints(sditf->dev,
							 ntf,
							 sizeof(struct sensor_async_subdev),
							 sditf_fwnode_parse);
	if (ret < 0)
		return ret;

	sditf->sd.subdev_notifier = &sditf->notifier;
	sditf->notifier.ops = &sditf_notifier_ops;

	ret = v4l2_async_subdev_nf_register(&sditf->sd, &sditf->notifier);
	if (ret) {
		v4l2_err(&sditf->sd,
			 "failed to register async notifier : %d\n",
			 ret);
		v4l2_async_nf_cleanup(&sditf->notifier);
		return ret;
	}

	return v4l2_async_register_subdev(&sditf->sd);
}

static int sditf_count_port_nodes(struct device_node *root_node)
{
	int count = 0;
	struct device_node *node = NULL;

	for_each_child_of_node(root_node, node) {
		if (of_node_cmp(node->name, "port") == 0)
			count++;
		count += sditf_count_port_nodes(node);
	}
	return count;
}

static int rkcif_subdev_media_init(struct sditf_priv *priv)
{
	struct rkcif_device *cif_dev = priv->cif_dev;
	struct v4l2_ctrl_handler *handler = &priv->ctrl_handler;
	unsigned long flags = V4L2_CTRL_FLAG_VOLATILE;
	int ret;
	int pad_num = 0;

	priv->port_count = sditf_count_port_nodes(priv->dev->of_node);
	if (priv->port_count > 1) {
		priv->pads[0].flags = MEDIA_PAD_FL_SINK;
		priv->pads[1].flags = MEDIA_PAD_FL_SOURCE;
		pad_num = 2;
	} else {
		priv->pads[0].flags = MEDIA_PAD_FL_SOURCE;
		pad_num = 1;
	}
	priv->sd.entity.function = MEDIA_ENT_F_PROC_VIDEO_COMPOSER;
	ret = media_entity_pads_init(&priv->sd.entity, pad_num, priv->pads);
	if (ret < 0)
		return ret;

	ret = v4l2_ctrl_handler_init(handler, 1);
	if (ret)
		return ret;
	priv->pixel_rate = v4l2_ctrl_new_std(handler, &rkcif_sditf_ctrl_ops,
					     V4L2_CID_PIXEL_RATE,
					     0, SDITF_PIXEL_RATE_MAX,
					     1, SDITF_PIXEL_RATE_MAX);
	if (priv->pixel_rate)
		priv->pixel_rate->flags |= flags;
	priv->sd.ctrl_handler = handler;
	if (handler->error) {
		v4l2_ctrl_handler_free(handler);
		return handler->error;
	}

	strncpy(priv->sd.name, dev_name(cif_dev->dev), sizeof(priv->sd.name));
	priv->cap_info.width = 0;
	priv->cap_info.height = 0;
	priv->mode.rdbk_mode = RKISP_VICAP_RDBK_AIQ;
	priv->toisp_inf.link_mode = TOISP_NONE;
	priv->toisp_inf.ch_info[0].is_valid = false;
	priv->toisp_inf.ch_info[1].is_valid = false;
	priv->toisp_inf.ch_info[2].is_valid = false;
	priv->is_toisp_off = true;
	if (priv->port_count > 1)
		sditf_subdev_notifier(priv);
	atomic_set(&priv->power_cnt, 0);
	atomic_set(&priv->stream_cnt, 0);
	INIT_WORK(&priv->buffree_work.work, sditf_buffree_work);
	INIT_LIST_HEAD(&priv->buf_free_list);
	INIT_LIST_HEAD(&priv->time_head);
	INIT_LIST_HEAD(&priv->gain_head);
	INIT_LIST_HEAD(&priv->effect_exp_head);
	priv->frame_idx.cur_frame_idx = 0;
	atomic_set(&priv->frm_sync_seq, 0);
	mutex_init(&priv->mutex);
	priv->hdr_wrap_line = 0;
	priv->is_buf_init = false;
	priv->is_multi_online = false;
	return 0;
}

static int rkcif_subdev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct v4l2_subdev *sd;
	struct sditf_priv *priv;
	struct device_node *node = dev->of_node;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->dev = dev;

	sd = &priv->sd;
	v4l2_subdev_init(sd, &sditf_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	snprintf(sd->name, sizeof(sd->name), "rockchip-cif-sditf");
	sd->dev = dev;

	platform_set_drvdata(pdev, &sd->entity);

	ret = rkcif_sditf_attach_cifdev(priv);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32(node,
				   "rockchip,combine-index",
				   &priv->combine_index);
	if (ret) {
		priv->is_combine_mode = false;
		priv->combine_index = 0;
	} else {
		priv->is_combine_mode = true;
	}
	ret = rkcif_subdev_media_init(priv);
	if (ret < 0)
		return ret;

	pm_runtime_enable(&pdev->dev);
	return 0;
}

static int rkcif_subdev_remove(struct platform_device *pdev)
{
	struct media_entity *me = platform_get_drvdata(pdev);
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(me);

	media_entity_cleanup(&sd->entity);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id rkcif_subdev_match_id[] = {
	{
		.compatible = "rockchip,rkcif-sditf",
	},
	{}
};
MODULE_DEVICE_TABLE(of, rkcif_subdev_match_id);

struct platform_driver rkcif_subdev_driver = {
	.probe = rkcif_subdev_probe,
	.remove = rkcif_subdev_remove,
	.driver = {
		.name = "rkcif_sditf",
		.of_match_table = rkcif_subdev_match_id,
	},
};
EXPORT_SYMBOL(rkcif_subdev_driver);

MODULE_AUTHOR("Rockchip Camera/ISP team");
MODULE_DESCRIPTION("Rockchip CIF platform driver");
MODULE_LICENSE("GPL v2");
