// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim Remote dummy sensor driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */

#include <linux/version.h>
#include <linux/device.h>
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

#define DRIVER_VERSION			KERNEL_VERSION(1, 0x00, 0x03)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SENSOR_NAME			"dummy"

#define SENSOR_CHIP_ID			0x5809
#define SENSOR_REG_CHIP_ID		0x300A

#define SENSOR_REG_CTRL_MODE		0x0100
#define SENSOR_MODE_SW_STANDBY		0x0
#define SENSOR_MODE_STREAMING		BIT(0)

#define SENSOR_VTS_MAX			0x7FFF

#define SENSOR_GAIN_MIN			0x0010
#define SENSOR_GAIN_MAX			0x0F7F
#define SENSOR_GAIN_STEP		0x01
#define SENSOR_GAIN_DEFAULT		0x10

#define SENSOR_EXPOSURE_HCG_MIN		4
#define SENSOR_EXPOSURE_HCG_STEP	1

#define SENSOR_XVCLK_FREQ		24000000

#define SENSOR_LINK_FREQ_500MHZ		500000000

/* I2C default address */
#define SENSOR_I2C_ADDR_DEF		0x30

/* register address: 16bit */
#define SENSOR_REG_ADDR_16BITS		2

/* register value: 8bit or 16bit or 24bit */
#define SENSOR_REG_VALUE_08BIT		1
#define SENSOR_REG_VALUE_16BIT		2
#define SENSOR_REG_VALUE_24BIT		3

/* I2C Array token */
#define REG_NULL			0xFFFF /* Array token: end */
#define REG_DELAY			0xFFEE /* Array token: delay */

struct i2c_regval {
	u16	reg_addr;
	u8	reg_val;
};

struct sensor_mode {
	u32			bus_fmt;
	u32			width;
	u32			height;
	struct v4l2_fract	max_fps;
	u32			hts_def;
	u32			vts_def;
	u32			exp_def;
	u32			link_freq_idx;
	u32			bpp;
	u32			hdr_mode;
	u32			vc[PAD_MAX];

	const struct i2c_regval *reg_list;
};

struct sensor {
	struct i2c_client		*client;
	struct regulator		*poc_regulator; /* PoC */

	struct mutex			mutex;

	struct v4l2_subdev		subdev;
	struct media_pad		pad;
	struct v4l2_ctrl_handler	ctrl_handler;
	struct v4l2_ctrl		*exposure;
	struct v4l2_ctrl		*anal_gain;
	struct v4l2_ctrl		*digi_gain;
	struct v4l2_ctrl		*hblank;
	struct v4l2_ctrl		*vblank;
	struct v4l2_ctrl		*test_pattern;
	struct v4l2_ctrl		*pixel_rate;
	struct v4l2_ctrl		*link_freq;
	struct v4l2_ctrl		*h_flip;
	struct v4l2_ctrl		*v_flip;
	struct v4l2_fwnode_endpoint	bus_cfg;

	bool				streaming;
	bool				power_on;
	bool				hot_plug;
	u8				is_reset;

	const struct sensor_mode	*supported_modes;
	const struct sensor_mode	*cur_mode;
	u32				cfg_modes_num;

	u32				module_index;
	const char			*module_facing;
	const char			*module_name;
	const char			*len_name;

	u8				cam_i2c_addr_def;
	u8				cam_i2c_addr_map;

	maxim_remote_ser_t		*serializer;
};

static const struct i2c_regval sensor_1920x1080_30fps_init_regs[] = {
	{ REG_NULL, 0x00 }
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
static const struct sensor_mode supported_modes[] = {
	{
		.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0038,
		.hts_def = 0x10fe,
		.vts_def = 0x0337,
		.bpp = 16,
		.link_freq_idx = 0,
		.hdr_mode = NO_HDR,
		.reg_list = sensor_1920x1080_30fps_init_regs,
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		.vc[PAD0] = 0,
#else
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
#endif /* LINUX_VERSION_CODE */
	},
};

static const s64 link_freq_menu_items[] = {
	SENSOR_LINK_FREQ_500MHZ,
};

static const char * const sensor_test_pattern_menu[] = {
	"Disabled",
};

/* Write registers up to 4 at a time */
static int __maybe_unused sensor_i2c_write_reg(struct i2c_client *client,
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
static int __maybe_unused sensor_i2c_read_reg(struct i2c_client *client,
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

static int __maybe_unused sensor_i2c_write_array(struct i2c_client *client,
						const struct i2c_regval *regs)
{
	u32 i = 0, delay_us = 0;
	int ret = 0;

	for (i = 0; (ret == 0) && (regs[i].reg_addr != REG_NULL); i++) {
		if (regs[i].reg_addr == REG_DELAY) {
			// delay us
			dev_info(&client->dev, "delay (%d) ms\n", regs[i].reg_val);

			delay_us = regs[i].reg_val * 1000;
			if (delay_us != 0)
				usleep_range(delay_us, delay_us + 100);

			continue;
		}

		ret |= sensor_i2c_write_reg(client, regs[i].reg_addr,
				SENSOR_REG_VALUE_08BIT, regs[i].reg_val);
	}

	return ret;
}

static int sensor_check_chip_id(struct sensor *sensor)
{
	struct i2c_client *client = sensor->client;
	struct device *dev = &client->dev;
	u32 sensor_id = 0;
	int ret = 0, loop = 0;

	for (loop = 0; loop < 3; loop++) {
		if (loop != 0) {
			dev_info(dev, "check sensor id retry (%d)", loop);
			msleep(10);
		}

#if 0 /* TODO */
		ret = sensor_i2c_read_reg(client, SENSOR_REG_CHIP_ID,
					SENSOR_REG_VALUE_16BIT, &sensor_id);
#else
		dev_info(dev, "%s: %d: TODO\n", __func__, __LINE__);
		sensor_id = SENSOR_CHIP_ID;
#endif
		if (ret == 0) {
			if (sensor_id != SENSOR_CHIP_ID) {
				dev_err(dev, "Unexpected sensor\n");
				return -ENODEV;
			} else {
				dev_info(dev, "Detected sensor\n");
				return 0;
			}
		}
	}

	dev_err(dev, "Check sensor id error, ret = %d\n", ret);

	return -ENODEV;
}

static int __sensor_start_stream(struct sensor *sensor)
{
	maxim_remote_ser_t *serializer = sensor->serializer;
	struct i2c_client *client = sensor->client;
	struct device *dev = &client->dev;
	int ret = 0;

	if (serializer == NULL) {
		dev_err(dev, "%s: serializer error\n", __func__);
		return -EINVAL;
	}

	if (serializer->ser_ops == NULL) {
		dev_err(dev, "%s: serializer ser_ops error\n", __func__);
		return -EINVAL;
	}

	ret = serializer->ser_ops->ser_module_init(serializer);
	if (ret) {
		dev_err(dev, "%s: serializer module_init error\n", __func__);
		return ret;
	}

	ret = sensor_check_chip_id(sensor);
	if (ret) {
		dev_err(dev, "%s: sensor check chip id error\n", __func__);
		return ret;
	}

	ret = sensor_i2c_write_array(client, sensor->cur_mode->reg_list);
	if (ret) {
		dev_err(dev, "%s: sensor i2c write array error\n", __func__);
		return ret;
	}

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&sensor->ctrl_handler);
	if (ret)
		return ret;

	/* streaming control register */
#if 0 /* TODO */
	ret = sensor_i2c_write_reg(client,
				SENSOR_REG_CTRL_MODE,
				SENSOR_REG_VALUE_08BIT,
				SENSOR_MODE_STREAMING);
	if (ret) {
		dev_err(dev, "%s: sensor start stream error\n", __func__);
		return ret;
	}
#else
	dev_info(dev, "%s: %d: TODO\n", __func__, __LINE__);
#endif

	/* serializer pclk detect */
	ret = serializer->ser_ops->ser_pclk_detect(serializer);
	if (ret) {
		dev_err(dev, "%s: serializer pclk_detect error\n", __func__);
		return ret;
	}

	return 0;
}

static int __sensor_stop_stream(struct sensor *sensor)
{
	maxim_remote_ser_t *serializer = sensor->serializer;
	struct i2c_client *client = sensor->client;
	struct device *dev = &client->dev;
	int ret = 0;

	/* streaming control register */
#if 0 /* TODO */
	ret = sensor_i2c_write_reg(client,
				SENSOR_REG_CTRL_MODE,
				SENSOR_REG_VALUE_08BIT,
				SENSOR_MODE_SW_STANDBY);
	if (ret) {
		dev_err(dev, "%s: sensor stop stream error\n", __func__);
		return ret;
	}
#else
	dev_info(dev, "%s: %d: TODO\n", __func__, __LINE__);
#endif

	if (serializer == NULL) {
		dev_err(dev, "%s: serializer error\n", __func__);
		return -EINVAL;
	}

	if (serializer->ser_ops == NULL) {
		dev_err(dev, "%s: serializer ser_ops error\n", __func__);
		return -EINVAL;
	}

	ret = serializer->ser_ops->ser_module_deinit(serializer);
	if (ret) {
		dev_err(dev, "%s: serializer module_deinit error\n", __func__);
		return ret;
	}

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	struct i2c_client *client = sensor->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on = %d\n", __func__, on);

	mutex_lock(&sensor->mutex);
	on = !!on;
	if (on == sensor->streaming)
		goto unlock_and_return;

	if (on) {
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
		ret = pm_runtime_resume_and_get(&client->dev);
#else
		ret = pm_runtime_get_sync(&client->dev);
#endif
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __sensor_start_stream(sensor);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sensor_stop_stream(sensor);
		pm_runtime_put(&client->dev);
	}

	sensor->streaming = on;

unlock_and_return:
	mutex_unlock(&sensor->mutex);

	return ret;
}

static int __sensor_power_on(struct sensor *sensor)
{
	struct device *dev = &sensor->client->dev;
	int ret = 0;

	dev_info(dev, "sensor device power on\n");

	ret = regulator_enable(sensor->poc_regulator);
	if (ret < 0) {
		dev_err(dev, "Unable to turn PoC regulator on\n");
		return ret;
	}

	return 0;
}

static void __sensor_power_off(struct sensor *sensor)
{
	struct device *dev = &sensor->client->dev;
	int ret = 0;

	dev_info(dev, "sensor device power off\n");

	ret = regulator_disable(sensor->poc_regulator);
	if (ret < 0)
		dev_warn(dev, "Unable to turn PoC regulator off\n");
}

static int sensor_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = __sensor_power_on(sensor);

	return ret;
}

static int sensor_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sensor *sensor = v4l2_get_subdevdata(sd);

	__sensor_power_off(sensor);

	return 0;
}

static const struct dev_pm_ops sensor_pm_ops = {
	SET_RUNTIME_PM_OPS(
		sensor_runtime_suspend, sensor_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sensor_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
#else
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
#endif
	const struct sensor_mode *def_mode = &sensor->supported_modes[0];

	mutex_lock(&sensor->mutex);

	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sensor->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sensor_s_power(struct v4l2_subdev *sd, int on)
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	struct i2c_client *client = sensor->client;
	int ret = 0;

	mutex_lock(&sensor->mutex);

	/* If the power state is not modified - no work to do. */
	if (sensor->power_on == !!on)
		goto unlock_and_return;

	if (on) {
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
		ret = pm_runtime_resume_and_get(&client->dev);
#else
		ret = pm_runtime_get_sync(&client->dev);
#endif
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		sensor->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sensor->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sensor->mutex);

	return ret;
}

static void sensor_get_module_inf(struct sensor *sensor,
					struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SENSOR_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sensor->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sensor->len_name, sizeof(inf->base.lens));
}

static void sensor_get_vicap_rst_inf(struct sensor *sensor,
			struct rkmodule_vicap_reset_info *rst_info)
{
	struct i2c_client *client = sensor->client;

	rst_info->is_reset = sensor->hot_plug;
	sensor->hot_plug = false;
	rst_info->src = RKCIF_RESET_SRC_ERR_HOTPLUG;

	dev_info(&client->dev, "%s: rst_info->is_reset:%d.\n",
		__func__, rst_info->is_reset);
}

static void sensor_set_vicap_rst_inf(struct sensor *sensor,
				struct rkmodule_vicap_reset_info rst_info)
{
	sensor->is_reset = rst_info.is_reset;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	long ret = 0;

	dev_dbg(&sensor->client->dev, "ioctl cmd = 0x%08x\n", cmd);

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sensor_get_module_inf(sensor, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_VICAP_RST_INFO:
		sensor_get_vicap_rst_inf(sensor,
			(struct rkmodule_vicap_reset_info *)arg);
		break;
	case RKMODULE_SET_VICAP_RST_INFO:
		sensor_set_vicap_rst_inf(sensor,
			*(struct rkmodule_vicap_reset_info *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long sensor_compat_ioctl32(struct v4l2_subdev *sd, unsigned int cmd,
					unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf = NULL;
	struct rkmodule_vicap_reset_info *vicap_rst_inf = NULL;
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sensor_ioctl(sd, cmd, inf);
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

		ret = sensor_ioctl(sd, cmd, vicap_rst_inf);
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
			ret = sensor_ioctl(sd, cmd, vicap_rst_inf);
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

static int sensor_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *fi)
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	const struct sensor_mode *mode = sensor->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sensor_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_mbus_code_enum *code)
#else
static int sensor_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = sensor->cur_mode->bus_fmt;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sensor_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_size_enum *fse)
#else
static int sensor_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);

	if (fse->index >= sensor->cfg_modes_num)
		return -EINVAL;

	if (fse->code != sensor->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = sensor->supported_modes[fse->index].width;
	fse->max_width  = sensor->supported_modes[fse->index].width;
	fse->max_height = sensor->supported_modes[fse->index].height;
	fse->min_height = sensor->supported_modes[fse->index].height;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sensor_enum_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_interval_enum *fie)
#else
static int sensor_enum_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_interval_enum *fie)
#endif
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);

	if (fie->index >= sensor->cfg_modes_num)
		return -EINVAL;

	fie->code = sensor->supported_modes[fie->index].bus_fmt;
	fie->width = sensor->supported_modes[fie->index].width;
	fie->height = sensor->supported_modes[fie->index].height;
	fie->interval = sensor->supported_modes[fie->index].max_fps;

	return 0;
}

static int sensor_get_reso_dist(const struct sensor_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sensor_mode *
sensor_find_best_fit(struct sensor *sensor, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < sensor->cfg_modes_num; i++) {
		dist = sensor_get_reso_dist(&sensor->supported_modes[i], framefmt);
		if ((cur_best_fit_dist == -1 || dist < cur_best_fit_dist) &&
			(sensor->supported_modes[i].bus_fmt == framefmt->code)) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &sensor->supported_modes[cur_best_fit];
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sensor_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
#else
static int sensor_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	struct device *dev = &sensor->client->dev;
	const struct sensor_mode *mode;
	u64 link_freq = 0, pixel_rate = 0;
	s64 h_blank, vblank_def;
	u8 data_lanes = sensor->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&sensor->mutex);

	mode = sensor_find_best_fit(sensor, fmt);
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
		mutex_unlock(&sensor->mutex);
		return -ENOTTY;
#endif
	} else {
		sensor->cur_mode = mode;

		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sensor->hblank,
				h_blank, h_blank, 1, h_blank);

		vblank_def = mode->vts_def - mode->height / 2;
		__v4l2_ctrl_modify_range(sensor->vblank,
				46, mode->height, 1, vblank_def);

		__v4l2_ctrl_s_ctrl(sensor->link_freq, mode->link_freq_idx);

		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		link_freq = link_freq_menu_items[mode->link_freq_idx];
		pixel_rate = (u32)link_freq / mode->bpp * 2 * data_lanes;
		__v4l2_ctrl_s_ctrl_int64(sensor->pixel_rate, pixel_rate);

		dev_info(dev, "mipi_freq_idx = %d, mipi_link_freq = %lld\n",
					mode->link_freq_idx, link_freq);
		dev_info(dev, "pixel_rate = %lld, bpp = %d\n",
					pixel_rate, mode->bpp);
	}

	dev_info(dev, "Set format done!(cur_mode: %d)\n", mode->hdr_mode);

	mutex_unlock(&sensor->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sensor_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
#else
static int sensor_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	const struct sensor_mode *mode = sensor->cur_mode;

	mutex_lock(&sensor->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
	#else
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	#endif
#else
		mutex_unlock(&sensor->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		/* format info: width/height/data type/virctual channel */
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&sensor->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sensor_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
#else
static int sensor_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
#endif
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = 0;
		sel->r.width = sensor->cur_mode->width;
		sel->r.top = 0;
		sel->r.height = sensor->cur_mode->height;
		return 0;
	}

	return -EINVAL;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int sensor_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2 = sensor->bus_cfg.bus.mipi_csi2;

	return 0;
}
#elif KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static int sensor_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	u32 val = 0;
	const struct sensor_mode *mode = sensor->cur_mode;
	u8 data_lanes = sensor->bus_cfg.bus.mipi_csi2.num_data_lanes;
	int i = 0;

	val |= V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	val |= (1 << (data_lanes - 1));

	for (i = 0; i < PAD_MAX; i++)
		val |= (mode->vc[i] & V4L2_MBUS_CSI2_CHANNELS);

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}
#else
static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *config)
{
	struct sensor *sensor = v4l2_get_subdevdata(sd);
	u32 val = 0;
	const struct sensor_mode *mode = sensor->cur_mode;
	u8 data_lanes = sensor->bus_cfg.bus.mipi_csi2.num_data_lanes;
	int i = 0;

	val |= V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	val |= (1 << (data_lanes - 1));

	for (i = 0; i < PAD_MAX; i++)
		val |= (mode->vc[i] & V4L2_MBUS_CSI2_CHANNELS);

	config->type = V4L2_MBUS_CSI2;
	config->flags = val;

	return 0;
}
#endif /* LINUX_VERSION_CODE */

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sensor_internal_ops = {
	.open = sensor_open,
};
#endif

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.s_power = sensor_s_power,
	.ioctl = sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
	.g_frame_interval = sensor_g_frame_interval,
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
	.g_mbus_config = sensor_g_mbus_config,
#endif
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_sizes,
	.enum_frame_interval = sensor_enum_frame_interval,
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
	.get_selection = sensor_get_selection,
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	.get_mbus_config = sensor_g_mbus_config,
#endif
};

static const struct v4l2_subdev_ops sensor_subdev_ops = {
	.core	= &sensor_core_ops,
	.video	= &sensor_video_ops,
	.pad	= &sensor_pad_ops,
};

static int sensor_enable_test_pattern(struct sensor *sensor, u32 pattern)
{
	// TODO

	return 0;
}

static int sensor_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor *sensor = container_of(ctrl->handler,
					struct sensor, ctrl_handler);
	struct i2c_client *client = sensor->client;
	int ret = 0;

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	// i2c can't be accessed before serdes link ok
	if (maxim_remote_ser_is_inited(sensor->serializer) == false) {
		dev_warn(&client->dev, "%s ctrl id = 0x%x before serializer init\n",
			__func__, ctrl->id);
		return 0;
	}

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		// TODO
		dev_info(&client->dev, "%s set exposure: val = 0x%x",
					__func__, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		// TODO
		dev_info(&client->dev, "%s set analog gain: val = 0x%x\n",
					__func__, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		// TODO
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sensor_enable_test_pattern(sensor, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		// TODO
		break;
	case V4L2_CID_VFLIP:
		// TODO
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.s_ctrl = sensor_set_ctrl,
};

static int sensor_initialize_controls(struct sensor *sensor)
{
	struct device *dev = &sensor->client->dev;
	const struct sensor_mode *mode;
	struct v4l2_ctrl_handler *handler;
	u64 link_freq = 0, pixel_rate = 0;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	u8 data_lanes;

	int ret = 0;

	handler = &sensor->ctrl_handler;
	mode = sensor->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;

	handler->lock = &sensor->mutex;

	/* ctrl handler: link freq */
	sensor->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_menu_items) - 1, 0,
				link_freq_menu_items);
	__v4l2_ctrl_s_ctrl(sensor->link_freq, mode->link_freq_idx);
	link_freq = link_freq_menu_items[mode->link_freq_idx];
	dev_info(dev, "mipi_freq_idx = %d, mipi_link_freq = %lld\n",
				mode->link_freq_idx, link_freq);

	/* ctrl handler: pixel rate */
	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	data_lanes = sensor->bus_cfg.bus.mipi_csi2.num_data_lanes;
	pixel_rate = (u32)link_freq / mode->bpp * 2 * data_lanes;

	sensor->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
					V4L2_CID_PIXEL_RATE,
					0, pixel_rate, 1, pixel_rate);
	dev_info(dev, "pixel_rate = %lld, bpp = %d\n", pixel_rate, mode->bpp);

	/* ctrl handler: hblank */
	h_blank = mode->hts_def - mode->width;
	sensor->hblank = v4l2_ctrl_new_std(handler, NULL,
					V4L2_CID_HBLANK,
					h_blank, h_blank, 1, h_blank);
	if (sensor->hblank)
		sensor->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* ctrl handler: vblank */
	vblank_def = mode->vts_def - mode->height / 2;
	sensor->vblank = v4l2_ctrl_new_std(handler, &sensor_ctrl_ops,
					V4L2_CID_VBLANK,
					46, mode->height, 1, vblank_def);

	/* ctrl handler: exposure */
	exposure_max = mode->vts_def - 12;
	sensor->exposure = v4l2_ctrl_new_std(handler, &sensor_ctrl_ops,
					V4L2_CID_EXPOSURE,
					SENSOR_EXPOSURE_HCG_MIN, exposure_max,
					SENSOR_EXPOSURE_HCG_STEP, mode->exp_def);

	/* ctrl handler: test pattern */
	sensor->test_pattern = v4l2_ctrl_new_std_menu_items(handler, &sensor_ctrl_ops,
					V4L2_CID_TEST_PATTERN,
					ARRAY_SIZE(sensor_test_pattern_menu) - 1,
					0, 0, sensor_test_pattern_menu);

	/* ctrl handler: analogue gain */
	sensor->anal_gain = v4l2_ctrl_new_std(handler, &sensor_ctrl_ops,
					V4L2_CID_ANALOGUE_GAIN,
					SENSOR_GAIN_MIN, SENSOR_GAIN_MAX,
					SENSOR_GAIN_STEP, SENSOR_GAIN_DEFAULT);

	/* ctrl handler: hflip */
	sensor->h_flip = v4l2_ctrl_new_std(handler, &sensor_ctrl_ops,
					V4L2_CID_HFLIP, 0, 1, 1, 0);

	/* ctrl handler: vflip */
	sensor->v_flip = v4l2_ctrl_new_std(handler, &sensor_ctrl_ops,
					V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&sensor->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sensor->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sensor_parse_dt(struct sensor *sensor)
{
	struct device *dev = &sensor->client->dev;
	struct device_node *of_node = dev->of_node;
	u32 value = 0;
	int ret = 0;

	dev_info(dev, "=== sensor parse dt ===\n");

	ret = of_property_read_u32(of_node, "cam-i2c-addr-def", &value);
	if (ret == 0) {
		dev_info(dev, "cam-i2c-addr-def property: 0x%x", value);
		sensor->cam_i2c_addr_def = value;
	} else {
		sensor->cam_i2c_addr_def = SENSOR_I2C_ADDR_DEF;
	}

	return 0;
}

static int sensor_bus_type_parse(struct sensor *sensor)
{
	struct device *dev = &sensor->client->dev;
	struct device_node *endpoint = NULL;
	u32 mipi_data_lanes;
	int ret = 0;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
		&sensor->bus_cfg);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}
	dev_info(dev, "bus type = 0x%x\n", sensor->bus_cfg.bus_type);

	if (sensor->bus_cfg.bus_type == V4L2_MBUS_CSI2_DPHY
			|| sensor->bus_cfg.bus_type == V4L2_MBUS_CSI2_CPHY) {
		mipi_data_lanes = sensor->bus_cfg.bus.mipi_csi2.num_data_lanes;
		dev_info(dev, "mipi csi2 phy data lanes = %d\n", mipi_data_lanes);
	}

	return 0;
}

static maxim_remote_ser_t *sensor_get_serializer_by_phandle(struct device *cam_dev)
{
	struct i2c_client *ser_client = NULL;
	struct device_node *ser_node = NULL;
	maxim_remote_ser_t *serializer = NULL;

	/* camera get remote serializer node */
	ser_node = of_parse_phandle(cam_dev->of_node, "cam-remote-ser", 0);
	if (!IS_ERR_OR_NULL(ser_node)) {
		dev_info(cam_dev, "remote serializer node: %pOF\n", ser_node);

		ser_client = of_find_i2c_device_by_node(ser_node);
		of_node_put(ser_node);
		if (!IS_ERR_OR_NULL(ser_client)) {
			serializer = i2c_get_clientdata(ser_client);
			if (!IS_ERR_OR_NULL(serializer))
				return serializer;
			else
				return NULL;
		} else {
			dev_err(cam_dev, "camera find remote serializer client error\n");

			return NULL;
		}
	} else {
		dev_warn(cam_dev, "cam-remote-ser node isn't exist\n");
		return NULL;
	}
}

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sensor *sensor = NULL;
	struct v4l2_subdev *sd = NULL;
	maxim_remote_ser_t *serializer = NULL;
	char facing[2];
	int ret = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x", DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		dev_err(dev, "sensor probe no memory error\n");
		return -ENOMEM;
	}

	sensor->client = client;
	sensor->cam_i2c_addr_map = client->addr;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sensor->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sensor->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sensor->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sensor->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	// poc regulator
	sensor->poc_regulator = devm_regulator_get(dev, "poc");
	if (IS_ERR(sensor->poc_regulator)) {
		if (PTR_ERR(sensor->poc_regulator) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get PoC regulator (%ld)\n",
				PTR_ERR(sensor->poc_regulator));
		else
			dev_err(dev, "Get PoC regulator deferred\n");

		ret = PTR_ERR(sensor->poc_regulator);

		return ret;
	}

	sensor_bus_type_parse(sensor);
	sensor->supported_modes = supported_modes;
	sensor->cfg_modes_num = ARRAY_SIZE(supported_modes);
	sensor->cur_mode = &supported_modes[0];

	mutex_init(&sensor->mutex);

	ret = __sensor_power_on(sensor);
	if (ret)
		goto err_destroy_mutex;

	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_enable(dev);

	sd = &sensor->subdev;
	v4l2_i2c_subdev_init(sd, client, &sensor_subdev_ops);
	ret = sensor_initialize_controls(sensor);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sensor_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

#if defined(CONFIG_MEDIA_CONTROLLER)
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sensor->pad);
	if (ret < 0)
		goto err_free_handler;
#endif

	v4l2_set_subdevdata(sd, sensor);

	memset(facing, 0, sizeof(facing));
	if (strcmp(sensor->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sensor->module_index, facing, SENSOR_NAME,
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

	sensor_parse_dt(sensor);

	/* remote serializer bind */
	serializer = sensor_get_serializer_by_phandle(dev);
	if (serializer != NULL) {
		dev_info(dev, "serializer bind success\n");

		serializer->cam_i2c_addr_def = sensor->cam_i2c_addr_def;
		serializer->cam_i2c_addr_map = sensor->cam_i2c_addr_map;

		sensor->serializer = serializer;
	} else {
		dev_err(dev, "serializer bind fail\n");

		sensor->serializer = NULL;
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
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);

err_power_off:
	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);
	__sensor_power_off(sensor);

err_destroy_mutex:
	mutex_destroy(&sensor->mutex);

	return ret;
}

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static int sensor_remove(struct i2c_client *client)
#else
static void sensor_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sensor *sensor = v4l2_get_subdevdata(sd);

	v4l2_async_unregister_subdev(sd);

#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);

	mutex_destroy(&sensor->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sensor_power_off(sensor);
	pm_runtime_set_suspended(&client->dev);

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id sensor_of_match[] = {
	{ .compatible = "maxim,dummy,sensor" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, sensor_of_match);

static struct i2c_driver sensor_i2c_driver = {
	.driver = {
		.name = "maxim-dummy",
		.pm = &sensor_pm_ops,
		.of_match_table = of_match_ptr(sensor_of_match),
	},
	.probe		= &sensor_probe,
	.remove		= &sensor_remove,
};

module_i2c_driver(sensor_i2c_driver);

MODULE_AUTHOR("Cai Wenzhong <cwz@rock-chips.com>");
MODULE_DESCRIPTION("Maxim Remote Dummy Sensor Driver");
MODULE_LICENSE("GPL");
