// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim Remote Sensor Smartsens SC320AT driver
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/compat.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#include "maxim_remote.h"

#define DRIVER_VERSION			KERNEL_VERSION(1, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC320AT_NAME			"sc320at"

#define SC320AT_XVCLK_FREQ		24000000
#define SC320AT_MIPI_FREQ_326M		326000000

#define SC320AT_I2C_ADDR_DEF		0x30

#define REG_NULL			0xFFFF
#define SC320AT_REG_VALUE_08BIT		1

struct i2c_regval {
	u16 reg_addr;
	u8 reg_val;
};

struct sc320at_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 link_freq_idx;
	u32 bpp;
	const struct i2c_regval *reg_list;
};

struct sc320at {
	struct i2c_client *client;
	struct regulator *poc_regulator;

	struct mutex mutex;

	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *anal_gain;
	struct v4l2_ctrl *digi_gain;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *test_pattern;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *h_flip;
	struct v4l2_ctrl *v_flip;
	struct v4l2_fwnode_endpoint bus_cfg;

	bool streaming;
	bool power_on;
	bool hot_plug;
	u8 is_reset;

	const struct sc320at_mode *supported_modes;
	const struct sc320at_mode *cur_mode;
	u32 cfg_modes_num;

	u32 module_index;
	const char *module_facing;
	const char *module_name;
	const char *len_name;

	u8 cam_i2c_addr_def;
	u8 cam_i2c_addr_map;

	struct maxim_remote_ser *remote_ser;
};

static const struct i2c_regval sc320at_1920x1440_regs[] = {
	{ REG_NULL, 0x00 },
};

/*
 * The width and height must be configured to be
 * the same as the current output resolution of the sensor.
 * The input width of the isp needs to be 16 aligned.
 * The input height of the isp needs to be 8 aligned.
 * If the width or height does not meet the alignment rules,
 * you can configure the cropping parameters with the following function to
 * crop out the appropriate resolution.
 * struct v4l2_subdev_pad_ops {
 *	.get_selection
 * }
 */
static const struct sc320at_mode supported_modes[] = {
	{
		.width = 1920,
		.height = 1440,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.link_freq_idx = 0,
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.bpp = 16,
		.reg_list = sc320at_1920x1440_regs,
	}
};

static const s64 link_freq_menu_items[] = {
	SC320AT_MIPI_FREQ_326M,
};

/* Write registers up to 4 at a time */
static int __maybe_unused sc320at_i2c_write_reg(struct i2c_client *client,
					u16 reg_addr, u32 val_len, u32 reg_val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	dev_info(&client->dev, "i2c addr(0x%02x) write: 0x%04x = 0x%08x (%d)\n",
			client->addr, reg_addr, reg_val, val_len);

	if (val_len > 4)
		return -EINVAL;

	buf[0] = reg_addr >> 8;
	buf[1] = reg_addr & 0xff;
	buf_i = 2;

	val_be = cpu_to_be32(reg_val);
	val_p = (u8 *)&val_be;
	val_i = 4 - val_len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, (val_len + 2)) != (val_len + 2)) {
		dev_err(&client->dev,
			"%s: writing register 0x%04x from 0x%02x failed\n",
			__func__, reg_addr, client->addr);
		return -EIO;
	}

	return 0;
}

/* Read registers up to 4 at a time */
static int __maybe_unused sc320at_i2c_read_reg(struct i2c_client *client,
					u16 reg_addr, u32 val_len, u32 *reg_val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg_addr);
	u8 *reg_be_p;
	int ret;

	if (val_len > 4 || !val_len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	reg_be_p = (u8 *)&reg_addr_be;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = reg_be_p;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = val_len;
	msgs[1].buf = &data_be_p[4 - val_len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev,
			"%s: reading register 0x%04x from 0x%02x failed\n",
			__func__, reg_addr, client->addr);
		return -EIO;
	}

	*reg_val = be32_to_cpu(data_be);

#if 0
	dev_info(&client->dev, "i2c addr(0x%02x) read: 0x%04x = 0x%08x (%d)\n",
		client->addr, reg_addr, *reg_val, val_len);
#endif

	return 0;
}

static int __maybe_unused sc320at_i2c_write_array(struct i2c_client *client,
						const struct i2c_regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; (ret == 0) && (regs[i].reg_addr != REG_NULL); i++) {
		ret = sc320at_i2c_write_reg(client, regs[i].reg_addr,
				SC320AT_REG_VALUE_08BIT, regs[i].reg_val);
	}

	return ret;
}

static int __sc320at_power_on(struct sc320at *sc320at)
{
	struct device *dev = &sc320at->client->dev;
	int ret = 0;

	dev_info(dev, "sc320at device power on\n");

	ret = regulator_enable(sc320at->poc_regulator);
	if (ret < 0) {
		dev_err(dev, "Unable to turn PoC regulator on\n");
		return ret;
	}

	return 0;
}

static void __sc320at_power_off(struct sc320at *sc320at)
{
	struct device *dev = &sc320at->client->dev;
	int ret = 0;

	dev_info(dev, "sc320at device power off\n");

	ret = regulator_disable(sc320at->poc_regulator);
	if (ret < 0)
		dev_warn(dev, "Unable to turn PoC regulator off\n");
}

static int sc320at_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = __sc320at_power_on(sc320at);

	return ret;
}

static int sc320at_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);

	__sc320at_power_off(sc320at);

	return 0;
}

static const struct dev_pm_ops sc320at_pm_ops = {
	SET_RUNTIME_PM_OPS(
		sc320at_runtime_suspend, sc320at_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc320at_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
#else
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
#endif
	const struct sc320at_mode *def_mode = &sc320at->supported_modes[0];

	mutex_lock(&sc320at->mutex);

	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc320at->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc320at_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	struct i2c_client *client = sc320at->client;
	int ret = 0;

	mutex_lock(&sc320at->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc320at->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		sc320at->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc320at->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc320at->mutex);

	return ret;
}

static void sc320at_get_module_inf(struct sc320at *sc320at,
					struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC320AT_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc320at->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc320at->len_name, sizeof(inf->base.lens));
}

static void sc320at_get_vicap_rst_inf(struct sc320at *sc320at,
			struct rkmodule_vicap_reset_info *rst_info)
{
	struct i2c_client *client = sc320at->client;

	rst_info->is_reset = sc320at->hot_plug;
	sc320at->hot_plug = false;
	rst_info->src = RKCIF_RESET_SRC_ERR_HOTPLUG;

	dev_info(&client->dev, "%s: rst_info->is_reset:%d.\n",
		__func__, rst_info->is_reset);
}

static void sc320at_set_vicap_rst_inf(struct sc320at *sc320at,
				struct rkmodule_vicap_reset_info rst_info)
{
	sc320at->is_reset = rst_info.is_reset;
}

static long sc320at_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	long ret = 0;

	dev_dbg(&sc320at->client->dev, "ioctl cmd = 0x%08x\n", cmd);

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc320at_get_module_inf(sc320at, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_VICAP_RST_INFO:
		sc320at_get_vicap_rst_inf(sc320at,
			(struct rkmodule_vicap_reset_info *)arg);
		break;
	case RKMODULE_SET_VICAP_RST_INFO:
		sc320at_set_vicap_rst_inf(sc320at,
			*(struct rkmodule_vicap_reset_info *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long sc320at_compat_ioctl32(struct v4l2_subdev *sd, unsigned int cmd,
					unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_vicap_reset_info *vicap_rst_inf;
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc320at_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_VICAP_RST_INFO:
		vicap_rst_inf = kzalloc(sizeof(*vicap_rst_inf), GFP_KERNEL);
		if (!vicap_rst_inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc320at_ioctl(sd, cmd, vicap_rst_inf);
		if (!ret) {
			ret = copy_to_user(up, vicap_rst_inf, sizeof(*vicap_rst_inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(vicap_rst_inf);
		break;
	case RKMODULE_SET_VICAP_RST_INFO:
		vicap_rst_inf = kzalloc(sizeof(*vicap_rst_inf), GFP_KERNEL);
		if (!vicap_rst_inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(vicap_rst_inf, up, sizeof(*vicap_rst_inf));
		if (!ret)
			ret = sc320at_ioctl(sd, cmd, vicap_rst_inf);
		else
			ret = -EFAULT;
		kfree(vicap_rst_inf);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif /* CONFIG_COMPAT */

static int __sc320at_start_stream(struct sc320at *sc320at)
{
	maxim_remote_ser_t *remote_ser = sc320at->remote_ser;
	struct i2c_client *client = sc320at->client;
	struct device *dev = &client->dev;
	int ret = 0;

	if (remote_ser == NULL) {
		dev_err(dev, "%s: remote_ser error\n", __func__);
		return -EINVAL;
	}

	if (remote_ser->ser_ops == NULL) {
		dev_err(dev, "%s: remote_ser ser_ops error\n", __func__);
		return -EINVAL;
	}

	ret = remote_ser->ser_ops->ser_module_init(remote_ser);
	if (ret) {
		dev_err(dev, "%s: remote_ser module_init error\n", __func__);
		return ret;
	}

	ret = sc320at_i2c_write_array(client, sc320at->cur_mode->reg_list);
	if (ret) {
		dev_err(dev, "%s: sc320at i2c write array error\n", __func__);
		return ret;
	}

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&sc320at->ctrl_handler);
	if (ret)
		return ret;

	ret = remote_ser->ser_ops->ser_pclk_detect(remote_ser);
	if (ret) {
		dev_err(dev, "%s: remote_ser pclk_detect error\n", __func__);
		return ret;
	}

	return 0;
}

static int __sc320at_stop_stream(struct sc320at *sc320at)
{
	maxim_remote_ser_t *remote_ser = sc320at->remote_ser;
	struct device *dev = &sc320at->client->dev;
	int ret = 0;

	if (remote_ser == NULL) {
		dev_err(dev, "%s: remote_ser error\n", __func__);
		return -EINVAL;
	}

	if (remote_ser->ser_ops == NULL) {
		dev_err(dev, "%s: remote_ser ser_ops error\n", __func__);
		return -EINVAL;
	}

	ret = remote_ser->ser_ops->ser_module_deinit(remote_ser);
	if (ret) {
		dev_err(dev, "%s: remote_ser module_deinit error\n", __func__);
		return ret;
	}

	return 0;
}

static int sc320at_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	struct i2c_client *client = sc320at->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on = %d\n", __func__, on);

	mutex_lock(&sc320at->mutex);
	on = !!on;
	if (on == sc320at->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __sc320at_start_stream(sc320at);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc320at_stop_stream(sc320at);
		pm_runtime_put(&client->dev);
	}

	sc320at->streaming = on;

unlock_and_return:
	mutex_unlock(&sc320at->mutex);

	return ret;
}

static int sc320at_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *fi)
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	const struct sc320at_mode *mode = sc320at->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sc320at_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
#else
static int sc320at_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = sc320at->cur_mode->bus_fmt;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sc320at_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_size_enum *fse)
#else
static int sc320at_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);

	if (fse->index >= sc320at->cfg_modes_num)
		return -EINVAL;

	if (fse->code != sc320at->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = sc320at->supported_modes[fse->index].width;
	fse->max_width  = sc320at->supported_modes[fse->index].width;
	fse->max_height = sc320at->supported_modes[fse->index].height;
	fse->min_height = sc320at->supported_modes[fse->index].height;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sc320at_enum_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_frame_interval_enum *fie)
#else
static int sc320at_enum_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_frame_interval_enum *fie)
#endif
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);

	if (fie->index >= sc320at->cfg_modes_num)
		return -EINVAL;

	fie->code = sc320at->supported_modes[fie->index].bus_fmt;
	fie->width = sc320at->supported_modes[fie->index].width;
	fie->height = sc320at->supported_modes[fie->index].height;
	fie->interval = sc320at->supported_modes[fie->index].max_fps;

	return 0;
}

static int sc320at_get_reso_dist(const struct sc320at_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc320at_mode *
sc320at_find_best_fit(struct sc320at *sc320at, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < sc320at->cfg_modes_num; i++) {
		dist = sc320at_get_reso_dist(&sc320at->supported_modes[i], framefmt);
		if ((cur_best_fit_dist == -1 || dist < cur_best_fit_dist) &&
			(sc320at->supported_modes[i].bus_fmt == framefmt->code)) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &sc320at->supported_modes[cur_best_fit];
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sc320at_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
#else
static int sc320at_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	struct device *dev = &sc320at->client->dev;
	const struct sc320at_mode *mode;
	u64 link_freq = 0, pixel_rate = 0;
	u8 data_lanes = sc320at->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&sc320at->mutex);

	mode = sc320at_find_best_fit(sc320at, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
	#else
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
	#endif
#else
		mutex_unlock(&sc320at->mutex);
		return -ENOTTY;
#endif
	} else {
		sc320at->cur_mode = mode;

		__v4l2_ctrl_s_ctrl(sc320at->link_freq, mode->link_freq_idx);

		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		link_freq = link_freq_menu_items[mode->link_freq_idx];
		pixel_rate = (u32)link_freq / mode->bpp * 2 * data_lanes;
		__v4l2_ctrl_s_ctrl_int64(sc320at->pixel_rate, pixel_rate);

		dev_info(dev, "mipi_freq_idx = %d, mipi_link_freq = %lld\n",
					mode->link_freq_idx, link_freq);
		dev_info(dev, "pixel_rate = %lld, bpp = %d\n",
					pixel_rate, mode->bpp);
	}

	mutex_unlock(&sc320at->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sc320at_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
#else
static int sc320at_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	const struct sc320at_mode *mode = sc320at->cur_mode;

	mutex_lock(&sc320at->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
	#else
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	#endif
#else
		mutex_unlock(&sc320at->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&sc320at->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sc320at_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
#else
static int sc320at_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
#endif
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = 0;
		sel->r.width = sc320at->cur_mode->width;
		sel->r.top = 0;
		sel->r.height = sc320at->cur_mode->height;
		return 0;
	}

	return -EINVAL;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sc320at_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2 = sc320at->bus_cfg.bus.mipi_csi2;

	return 0;
}
#elif KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static int sc320at_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	u32 val = 0;
	u8 data_lanes = sc320at->bus_cfg.bus.mipi_csi2.num_data_lanes;

	val |= V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	val |= (1 << (data_lanes - 1));

	val |= V4L2_MBUS_CSI2_CHANNEL_3 | V4L2_MBUS_CSI2_CHANNEL_2 |
	       V4L2_MBUS_CSI2_CHANNEL_1 | V4L2_MBUS_CSI2_CHANNEL_0;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}
#else
static int sc320at_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *config)
{
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);
	u32 val = 0;
	u8 data_lanes = sc320at->bus_cfg.bus.mipi_csi2.num_data_lanes;

	val |= V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	val |= (1 << (data_lanes - 1));

	val |= V4L2_MBUS_CSI2_CHANNEL_3 | V4L2_MBUS_CSI2_CHANNEL_2 |
	       V4L2_MBUS_CSI2_CHANNEL_1 | V4L2_MBUS_CSI2_CHANNEL_0;

	config->type = V4L2_MBUS_CSI2;
	config->flags = val;

	return 0;
}
#endif /* LINUX_VERSION_CODE */

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc320at_internal_ops = {
	.open = sc320at_open,
};
#endif

static const struct v4l2_subdev_core_ops sc320at_core_ops = {
	.s_power = sc320at_s_power,
	.ioctl = sc320at_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc320at_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc320at_video_ops = {
	.s_stream = sc320at_s_stream,
	.g_frame_interval = sc320at_g_frame_interval,
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
	.g_mbus_config = sc320at_g_mbus_config,
#endif
};

static const struct v4l2_subdev_pad_ops sc320at_pad_ops = {
	.enum_mbus_code = sc320at_enum_mbus_code,
	.enum_frame_size = sc320at_enum_frame_sizes,
	.enum_frame_interval = sc320at_enum_frame_interval,
	.get_fmt = sc320at_get_fmt,
	.set_fmt = sc320at_set_fmt,
	.get_selection = sc320at_get_selection,
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	.get_mbus_config = sc320at_g_mbus_config,
#endif
};

static const struct v4l2_subdev_ops sc320at_subdev_ops = {
	.core	= &sc320at_core_ops,
	.video	= &sc320at_video_ops,
	.pad	= &sc320at_pad_ops,
};

static int sc320at_initialize_controls(struct sc320at *sc320at)
{
	struct device *dev = &sc320at->client->dev;
	const struct sc320at_mode *mode;
	struct v4l2_ctrl_handler *handler;
	u64 link_freq = 0, pixel_rate = 0;
	u8 data_lanes;

	int ret = 0;

	handler = &sc320at->ctrl_handler;
	mode = sc320at->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 2);
	if (ret)
		return ret;

	handler->lock = &sc320at->mutex;

	/* ctrl handler: link freq */
	sc320at->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_menu_items) - 1, 0,
				link_freq_menu_items);
	__v4l2_ctrl_s_ctrl(sc320at->link_freq, mode->link_freq_idx);
	link_freq = link_freq_menu_items[mode->link_freq_idx];
	dev_info(dev, "mipi_freq_idx = %d, mipi_link_freq = %lld\n",
				mode->link_freq_idx, link_freq);

	/* ctrl handler: pixel rate */
	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	data_lanes = sc320at->bus_cfg.bus.mipi_csi2.num_data_lanes;
	pixel_rate = (u32)link_freq / mode->bpp * 2 * data_lanes;

	sc320at->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
					V4L2_CID_PIXEL_RATE,
					0, pixel_rate, 1, pixel_rate);
	dev_info(dev, "pixel_rate = %lld, bpp = %d\n", pixel_rate, mode->bpp);

	if (handler->error) {
		ret = handler->error;
		dev_err(&sc320at->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc320at->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc320at_parse_dt(struct sc320at *sc320at)
{
	struct device *dev = &sc320at->client->dev;
	struct device_node *of_node = dev->of_node;
	u32 value = 0;
	int ret = 0;

	dev_info(dev, "=== sc320at parse dt ===\n");

	ret = of_property_read_u32(of_node, "cam-i2c-addr-def", &value);
	if (ret == 0) {
		dev_info(dev, "cam-i2c-addr-def property: 0x%x", value);
		sc320at->cam_i2c_addr_def = value;
	} else {
		sc320at->cam_i2c_addr_def = SC320AT_I2C_ADDR_DEF;
	}

	return 0;
}

static int sc320at_mipi_data_lanes_parse(struct sc320at *sc320at)
{
	struct device *dev = &sc320at->client->dev;
	struct device_node *endpoint = NULL;
	u8 mipi_data_lanes;
	int ret = 0;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
		&sc320at->bus_cfg);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}
	mipi_data_lanes = sc320at->bus_cfg.bus.mipi_csi2.num_data_lanes;
	dev_info(dev, "mipi csi2 phy data lanes = %d\n", mipi_data_lanes);

	return 0;
}

static int sc320at_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sc320at *sc320at = NULL;
	struct v4l2_subdev *sd = NULL;
	maxim_remote_ser_t *remote_ser = NULL;
	char facing[2];
	int ret = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x", DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	sc320at = devm_kzalloc(dev, sizeof(*sc320at), GFP_KERNEL);
	if (!sc320at) {
		dev_err(dev, "sc320at probe no memory error\n");
		return -ENOMEM;
	}

	sc320at->client = client;
	sc320at->cam_i2c_addr_map = client->addr;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc320at->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc320at->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc320at->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc320at->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	// poc regulator
	sc320at->poc_regulator = devm_regulator_get(dev, "poc");
	if (IS_ERR(sc320at->poc_regulator)) {
		if (PTR_ERR(sc320at->poc_regulator) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get PoC regulator (%ld)\n",
				PTR_ERR(sc320at->poc_regulator));
		else
			dev_err(dev, "Get PoC regulator deferred\n");

		ret = PTR_ERR(sc320at->poc_regulator);

		return ret;
	}

	sc320at_mipi_data_lanes_parse(sc320at);
	sc320at->supported_modes = supported_modes;
	sc320at->cfg_modes_num = ARRAY_SIZE(supported_modes);
	sc320at->cur_mode = &sc320at->supported_modes[0];

	mutex_init(&sc320at->mutex);

	ret = __sc320at_power_on(sc320at);
	if (ret)
		goto err_destroy_mutex;

	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_enable(dev);

	sd = &sc320at->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc320at_subdev_ops);
	ret = sc320at_initialize_controls(sc320at);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc320at_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

#if defined(CONFIG_MEDIA_CONTROLLER)
	sc320at->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc320at->pad);
	if (ret < 0)
		goto err_free_handler;
#endif

	v4l2_set_subdevdata(sd, sc320at);

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc320at->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc320at->module_index, facing, SC320AT_NAME,
		 dev_name(sd->dev));

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	ret = v4l2_async_register_subdev_sensor(sd);
#else
	ret = v4l2_async_register_subdev_sensor_common(sd);
#endif
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	sc320at_parse_dt(sc320at);

	/* remote serializer bind */
	sc320at->remote_ser = NULL;
	remote_ser = maxim_remote_cam_bind_ser(dev);
	if (remote_ser != NULL) {
		dev_info(dev, "remote serializer bind success\n");

		remote_ser->cam_i2c_addr_def = sc320at->cam_i2c_addr_def;
		remote_ser->cam_i2c_addr_map = sc320at->cam_i2c_addr_map;

		sc320at->remote_ser = remote_ser;
	} else {
		dev_err(dev, "remote serializer bind fail\n");
	}

	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif

err_free_handler:
	v4l2_ctrl_handler_free(&sc320at->ctrl_handler);

err_power_off:
	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);
	__sc320at_power_off(sc320at);

err_destroy_mutex:
	mutex_destroy(&sc320at->mutex);

	return ret;
}

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static int sc320at_remove(struct i2c_client *client)
#else
static void sc320at_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc320at *sc320at = v4l2_get_subdevdata(sd);

	v4l2_async_unregister_subdev(sd);

#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc320at->ctrl_handler);

	mutex_destroy(&sc320at->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc320at_power_off(sc320at);
	pm_runtime_set_suspended(&client->dev);

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id sc320at_of_match[] = {
	{ .compatible = "maxim,smartsens,sc320at" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, sc320at_of_match);

static struct i2c_driver sc320at_i2c_driver = {
	.driver = {
		.name = "maxim-sc320at",
		.pm = &sc320at_pm_ops,
		.of_match_table = of_match_ptr(sc320at_of_match),
	},
	.probe		= &sc320at_probe,
	.remove		= &sc320at_remove,
};

module_i2c_driver(sc320at_i2c_driver);

MODULE_AUTHOR("Cai Wenzhong <cwz@rock-chips.com>");
MODULE_DESCRIPTION("Maxim Remote Sensor Smartsens sc320at Driver");
MODULE_LICENSE("GPL");
