// SPDX-License-Identifier: GPL-2.0
/*
 * bf2557 driver
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 *
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>

#define DRIVER_VERSION		KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN	V4L2_CID_GAIN
#endif

#define BF2557_REG_VALUE_08BIT	1
#define BF2557_REG_VALUE_16BIT	2
#define BF2557_REG_VALUE_24BIT	3

#define BF2557_LANES		2
#define BF2557_BITS_PER_SAMPLE	10
#define MIPI_FREQ	426000000LL

#define BF2557_MIRROR_NORMAL	0
#define BF2557_MIRROR_H		0
#define BF2557_MIRROR_V		1
#define BF2557_MIRROR_HV	0

#if BF2557_MIRROR_NORMAL
#define BF2557_MIRROR		0x00
#elif BF2557_MIRROR_H
#define BF2557_MIRROR		0x02
#elif BF2557_MIRROR_V
#define BF2557_MIRROR		0x01
#elif BF2557_MIRROR_HV
#define BF2557_MIRROR		0x03
#else
#define BF2557_MIRROR		0x00
#endif

/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define BF2557_PIXEL_RATE	(MIPI_FREQ * 2LL * BF2557_LANES / BF2557_BITS_PER_SAMPLE)
#define BF2557_XVCLK_FREQ	24000000

#define CHIP_ID			0x2557
#define BF2557_REG_CHIP_ID_H	0xfc
#define BF2557_REG_CHIP_ID_L	0xfd
#define SENSOR_ID(_msb, _lsb)	((_msb) << 8 | (_lsb))

#define BF2557_REG_CTRL_MODE	0xf1
#define BF2557_MODE_SW_STANDBY	0x01
#define BF2557_MODE_STREAMING	0x00

#define BF2557_REG_EXPOSURE_H	0x6b
#define BF2557_REG_EXPOSURE_L	0x6c
#define BF2557_FETCH_HIGH_BYTE(VAL)	(((VAL) >> 8) & 0xFF)	/* 8 Bits */
#define BF2557_FETCH_LOW_BYTE(VAL)	((VAL) & 0xFF)	/* 8 Bits */
#define	BF2557_EXPOSURE_MIN	4
#define	BF2557_EXPOSURE_STEP	1
#define BF2557_VTS_MAX		0x7fff

#define BF2557_REG_AGAIN	0x6a

#define BF2557_AGAIN_MIN	0x10
#define BF2557_AGAIN_MAX	0xff
#define BF2557_AGAIN_STEP	1
#define BF2557_AGAIN_DEFAULT	0x10

#define BF2557_REG_VTS_H	0x07
#define BF2557_REG_VTS_L	0x06

#define BF2557_FLIP_MIRROR_REG	0xF2
#define MIRROR_BIT_MASK		BIT(1)
#define FLIP_BIT_MASK		BIT(0)

#define REG_NULL		0xFF

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define BF2557_NAME				"bf2557"
#define BF2557_MEDIA_BUS_FMT	MEDIA_BUS_FMT_SGRBG10_1X10

static const char * const bf2557_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define BF2557_NUM_SUPPLIES ARRAY_SIZE(bf2557_supply_names)


struct regval {
	u8 addr;
	u8 val;
};

struct bf2557_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list;
	const struct regval *global_reg_list;
	u32 mipi_freq_idx;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct bf2557 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[BF2557_NUM_SUPPLIES];
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;
	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_gain;
	struct v4l2_ctrl	*digi_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*link_freq;
	struct v4l2_ctrl	*h_flip;
	struct v4l2_ctrl	*v_flip;
	u8			flip_mirror;
	struct mutex		mutex;
	bool			streaming;
	unsigned int		lane_num;
	unsigned int		cfg_num;
	unsigned int		pixel_rate;
	bool			power_on;
	const struct bf2557_mode *cur_mode;
	const struct bf2557_mode *support_modes;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	struct rkmodule_inf	module_inf;
	struct rkmodule_awb_cfg	awb_cfg;
};

#define to_bf2557(sd) container_of(sd, struct bf2557, subdev)

/*
 * Sensor Information
 * Sensor           : BF2557
 * Date             : 2024-01-19
 * Image size       : 2592 x 1944
 * MCLK/PCLK        : 24MHz /170.4Mhz
 * MIPI speed(Mbps) : 852Mbps x 2Lane
 * Line Length      : 2828
 * Frame Length     : 2008
 * line Time        : 16596ns
 * Max Fps          : 30.00fps
 * BLC offset       : 64code
 */
static const struct regval bf2557_global_regs[] = {

	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 876Mbps
 */
static const struct regval bf2557_2592x1944_regs[] = {
	/* init setting */
	{0xf0, 0x01},
	{0xf2, BF2557_MIRROR},
	{0x02, 0xae},
	{0x12, 0x3e},
	{0x1f, 0x66},
	{0x20, 0x82},
	{0x24, 0x1e},
	{0x26, 0x37},
	{0x2a, 0x01},
	{0x2d, 0x3a},
	{0x2f, 0xe8},
	{0x32, 0x23},
	{0x36, 0x98},
	{0x37, 0xb5},
	{0x3a, 0xd7},
	{0x62, 0x5e},
	{0xe1, 0xf7},
	{0xe2, 0x22},
	{0xe4, 0x02},
	{0xe8, 0x10},
	{0xe9, 0x04},
	{0xec, 0x80},

	{0xea, 0x16},
	{0xeb, 0xd5},
	{0xe7, 0x47},

	{0x0b, 0x86},
	{0x07, 0x00},
	{0x06, 0x26},

	{0x57, 0x40},
	{0x58, 0x40},
	{0x59, 0x40},
	{0x5a, 0x40},

	{0x00, 0x10},
	{0xa0, 0x01},
	{0xe6, 0x02},

	{0xca, 0xa0},
	{0xcb, 0x70},
	{0xcc, 0x04},
	{0xcd, 0x24},
	{0xce, 0x04},
	{0xcf, 0x9c},

	{0x70, 0x08},
	{0x71, 0x07},
	{0x72, 0x16},
	{0x73, 0x09},
	{0x74, 0x08},
	{0x75, 0x06},
	{0x76, 0x30},
	{0x77, 0x02},
	{0x78, 0x10},
	{0x79, 0x0a},

	{0x6a, 0x7f},
	{0x6b, 0x12},
	{0x6c, 0x01},

	/* 2592 x 1944 setting */
	{0xea, 0x16},
	{0xeb, 0xd5},
	{0xe7, 0x47},
	{0x3a, 0xd7},

	{0x0b, 0x86},
	{0x07, 0x00},
	{0x06, 0x26},

	{0x00, 0x10},
	{0xa0, 0x01},
	{0xe6, 0x02},

	{0xca, 0xa0},
	{0xcb, 0x70},
	{0xcc, 0x04},
	{0xcd, 0x24},
	{0xce, 0x04},
	{0xcf, 0x9c},

	{0x70, 0x08},
	{0x71, 0x07},
	{0x72, 0x16},
	{0x73, 0x09},
	{0x74, 0x08},
	{0x75, 0x06},
	{0x76, 0x30},
	{0x77, 0x02},
	{0x78, 0x10},
	{0x79, 0x0a},

	{REG_NULL, 0x00},
};

static const struct bf2557_mode supported_modes_2lane[] = {
	{
		.width = 2592,
		.height = 1944,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.bus_fmt = MEDIA_BUS_FMT_SGRBG10_1X10,
		.exp_def = 0x07C0,
		.hts_def = 0x728 * 3,
		.vts_def = 0x7D8,
		.reg_list = bf2557_2592x1944_regs,
		.global_reg_list = bf2557_global_regs,
		.mipi_freq_idx = 0,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_menu_items[] = {
	MIPI_FREQ,
};

static int bf2557_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg & 0xFF;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		return 0;

	dev_err(&client->dev,
		"bf2557 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int bf2557_write_array(struct i2c_client *client,
	const struct regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		ret = bf2557_write_reg(client, regs[i].addr, regs[i].val);
		if (regs[i].addr == 0xf0) {
			/* Delay 20ms after reset */
			usleep_range(20000, 20500);
		}
	}
	return ret;
}

/* Read registers up to 4 at a time */
static int bf2557_read_reg(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	buf[0] = reg & 0xFF;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret >= 0) {
		*val = buf[0];
		return 0;
	}

	dev_err(&client->dev,
		"bf2557 read reg:0x%x failed !\n", reg);

	return ret;
}

static int bf2557_get_reso_dist(const struct bf2557_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		abs(mode->height - framefmt->height);
}

static const struct bf2557_mode *
bf2557_find_best_fit(struct bf2557 *bf2557,
		     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < bf2557->cfg_num; i++) {
		dist = bf2557_get_reso_dist(&bf2557->support_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &bf2557->support_modes[cur_best_fit];
}

static int bf2557_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_format *fmt)
{
	struct bf2557 *bf2557 = to_bf2557(sd);
	const struct bf2557_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&bf2557->mutex);

	mode = bf2557_find_best_fit(bf2557, fmt);
	fmt->format.code = BF2557_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_state_get_format(sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&bf2557->mutex);
		return -ENOTTY;
#endif
	} else {
		bf2557->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(bf2557->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(bf2557->vblank, vblank_def,
					 BF2557_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(bf2557->link_freq,
				   mode->mipi_freq_idx);
	}
	dev_info(&bf2557->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&bf2557->mutex);

	return 0;
}

static int bf2557_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_format *fmt)
{
	struct bf2557 *bf2557 = to_bf2557(sd);
	const struct bf2557_mode *mode = bf2557->cur_mode;

	mutex_lock(&bf2557->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_state_get_format(sd_state, fmt->pad);
#else
		mutex_unlock(&bf2557->mutex);
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
	mutex_unlock(&bf2557->mutex);

	return 0;
}

static int bf2557_enum_mbus_code(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = BF2557_MEDIA_BUS_FMT;

	return 0;
}

static int bf2557_enum_frame_sizes(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct bf2557 *bf2557 = to_bf2557(sd);

	if (fse->index >= bf2557->cfg_num)
		return -EINVAL;

	fse->min_width  = bf2557->support_modes[fse->index].width;
	fse->max_width  = bf2557->support_modes[fse->index].width;
	fse->max_height = bf2557->support_modes[fse->index].height;
	fse->min_height = bf2557->support_modes[fse->index].height;

	return 0;
}

static int bf2557_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct bf2557 *bf2557 = to_bf2557(sd);
	const struct bf2557_mode *mode = bf2557->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static void bf2557_get_module_inf(struct bf2557 *bf2557,
				struct rkmodule_inf *inf)
{
	strscpy(inf->base.sensor,
		BF2557_NAME,
		sizeof(inf->base.sensor));
	strscpy(inf->base.module,
		bf2557->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens,
		bf2557->len_name,
		sizeof(inf->base.lens));
}

static void bf2557_set_module_inf(struct bf2557 *bf2557,
				struct rkmodule_awb_cfg *cfg)
{
	mutex_lock(&bf2557->mutex);
	memcpy(&bf2557->awb_cfg, cfg, sizeof(*cfg));
	mutex_unlock(&bf2557->mutex);
}

static int bf2557_get_channel_info(struct bf2557 *bf2557, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = bf2557->cur_mode->vc[ch_info->index];
	ch_info->width = bf2557->cur_mode->width;
	ch_info->height = bf2557->cur_mode->height;
	ch_info->bus_fmt = BF2557_MEDIA_BUS_FMT;
	return 0;
}

static long bf2557_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct bf2557 *bf2557 = to_bf2557(sd);
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		bf2557_get_module_inf(bf2557, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_AWB_CFG:
		bf2557_set_module_inf(bf2557, (struct rkmodule_awb_cfg *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);
		if (stream) {
			ret |= bf2557_write_reg(bf2557->client,
						BF2557_REG_CTRL_MODE,
						BF2557_MODE_STREAMING);
		} else {
			ret |= bf2557_write_reg(bf2557->client,
						BF2557_REG_CTRL_MODE,
						BF2557_MODE_SW_STANDBY);
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = bf2557_get_channel_info(bf2557, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long bf2557_compat_ioctl32(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = bf2557_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(cfg, up, sizeof(*cfg));
		if (!ret)
			ret = bf2557_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = bf2557_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = bf2557_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __bf2557_start_stream(struct bf2557 *bf2557)
{
	int ret;

	ret = bf2557_write_array(bf2557->client, bf2557->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&bf2557->mutex);
	ret = v4l2_ctrl_handler_setup(&bf2557->ctrl_handler);
	mutex_lock(&bf2557->mutex);
	ret |= bf2557_write_reg(bf2557->client, 0x01, 0x4a);
	ret |= bf2557_write_reg(bf2557->client,
		BF2557_REG_CTRL_MODE,
		BF2557_MODE_STREAMING);
	ret |= bf2557_write_reg(bf2557->client, 0x01, 0x42);
	return ret;
}

static int __bf2557_stop_stream(struct bf2557 *bf2557)
{
	int ret;

	ret = bf2557_write_reg(bf2557->client,
		BF2557_REG_CTRL_MODE,
		BF2557_MODE_SW_STANDBY);

	return ret;
}

static int bf2557_s_stream(struct v4l2_subdev *sd, int on)
{
	struct bf2557 *bf2557 = to_bf2557(sd);
	struct i2c_client *client = bf2557->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				bf2557->cur_mode->width,
				bf2557->cur_mode->height,
		DIV_ROUND_CLOSEST(bf2557->cur_mode->max_fps.denominator,
		bf2557->cur_mode->max_fps.numerator));

	mutex_lock(&bf2557->mutex);
	on = !!on;
	if (on == bf2557->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __bf2557_start_stream(bf2557);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__bf2557_stop_stream(bf2557);
		pm_runtime_put(&client->dev);
	}

	bf2557->streaming = on;

unlock_and_return:
	mutex_unlock(&bf2557->mutex);

	return ret;
}

static int bf2557_s_power(struct v4l2_subdev *sd, int on)
{
	struct bf2557 *bf2557 = to_bf2557(sd);
	struct i2c_client *client = bf2557->client;
	int ret = 0;

	dev_info(&client->dev, "%s(%d) on(%d)\n", __func__, __LINE__, on);
	mutex_lock(&bf2557->mutex);

	/* If the power state is not modified - no work to do. */
	if (bf2557->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		bf2557->flip_mirror = 0;
		ret = bf2557_write_array(bf2557->client, bf2557->cur_mode->global_reg_list);
		if (ret) {
			v4l2_err(sd, "could not set init registers\n");
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		bf2557->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		bf2557->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&bf2557->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 bf2557_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, BF2557_XVCLK_FREQ / 1000 / 1000);
}

static int __bf2557_power_on(struct bf2557 *bf2557)
{
	int ret;
	u32 delay_us;
	struct device *dev = &bf2557->client->dev;

	if (!IS_ERR(bf2557->power_gpio))
		gpiod_set_value_cansleep(bf2557->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(bf2557->pins_default)) {
		ret = pinctrl_select_state(bf2557->pinctrl,
					   bf2557->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(bf2557->xvclk, BF2557_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(bf2557->xvclk) != BF2557_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(bf2557->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	ret = regulator_bulk_enable(BF2557_NUM_SUPPLIES, bf2557->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}
	usleep_range(2000, 2100);

	if (!IS_ERR(bf2557->reset_gpio))
		gpiod_set_value_cansleep(bf2557->reset_gpio, 0);

	usleep_range(1000, 1100);
	if (!IS_ERR(bf2557->reset_gpio))
		gpiod_set_value_cansleep(bf2557->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(bf2557->pwdn_gpio))
		gpiod_set_value_cansleep(bf2557->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = bf2557_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(bf2557->xvclk);

	return ret;
}

static void __bf2557_power_off(struct bf2557 *bf2557)
{
	int ret;

	if (!IS_ERR(bf2557->pwdn_gpio))
		gpiod_set_value_cansleep(bf2557->pwdn_gpio, 0);
	clk_disable_unprepare(bf2557->xvclk);
	if (!IS_ERR(bf2557->reset_gpio))
		gpiod_set_value_cansleep(bf2557->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(bf2557->pins_sleep)) {
		ret = pinctrl_select_state(bf2557->pinctrl,
					   bf2557->pins_sleep);
		if (ret < 0)
			dev_dbg(&bf2557->client->dev, "could not set pins\n");
	}
	if (!IS_ERR(bf2557->power_gpio))
		gpiod_set_value_cansleep(bf2557->power_gpio, 0);

	regulator_bulk_disable(BF2557_NUM_SUPPLIES, bf2557->supplies);
}

static int bf2557_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf2557 *bf2557 = to_bf2557(sd);

	return __bf2557_power_on(bf2557);
}

static int bf2557_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf2557 *bf2557 = to_bf2557(sd);

	__bf2557_power_off(bf2557);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int bf2557_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct bf2557 *bf2557 = to_bf2557(sd);
	struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_state_get_format(fh->state, 0);
	const struct bf2557_mode *def_mode = &bf2557->support_modes[0];

	mutex_lock(&bf2557->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = BF2557_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&bf2557->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int bf2557_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct bf2557 *bf2557 = to_bf2557(sd);

	if (fie->index >= bf2557->cfg_num)
		return -EINVAL;

	fie->code = BF2557_MEDIA_BUS_FMT;
	fie->width = bf2557->support_modes[fie->index].width;
	fie->height = bf2557->support_modes[fie->index].height;
	fie->interval = bf2557->support_modes[fie->index].max_fps;
	return 0;
}

static int bf2557_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct bf2557 *sensor = to_bf2557(sd);
	struct device *dev = &sensor->client->dev;

	dev_info(dev, "%s(%d) enter!\n", __func__, __LINE__);

	if (2 == sensor->lane_num) {
		config->type = V4L2_MBUS_CSI2_DPHY;
		config->bus.mipi_csi2.num_data_lanes = sensor->lane_num;
	} else {
		dev_err(&sensor->client->dev,
			"unsupported lane_num(%d)\n", sensor->lane_num);
	}
	return 0;
}

static int bf2557_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct bf2557 *bf2557 = to_bf2557(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = 0;
		sel->r.width = bf2557->cur_mode->width;
		sel->r.top = 0;
		sel->r.height = bf2557->cur_mode->height;
		return 0;
	}

	return -EINVAL;
}

static const struct dev_pm_ops bf2557_pm_ops = {
	SET_RUNTIME_PM_OPS(bf2557_runtime_suspend,
			bf2557_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops bf2557_internal_ops = {
	.open = bf2557_open,
};
#endif

static const struct v4l2_subdev_core_ops bf2557_core_ops = {
	.s_power = bf2557_s_power,
	.ioctl = bf2557_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = bf2557_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops bf2557_video_ops = {
	.s_stream = bf2557_s_stream,
};

static const struct v4l2_subdev_pad_ops bf2557_pad_ops = {
	.enum_mbus_code = bf2557_enum_mbus_code,
	.enum_frame_size = bf2557_enum_frame_sizes,
	.enum_frame_interval = bf2557_enum_frame_interval,
	.get_fmt = bf2557_get_fmt,
	.set_fmt = bf2557_set_fmt,
	.get_selection = bf2557_get_selection,
	.get_mbus_config = bf2557_g_mbus_config,
	.get_frame_interval = bf2557_g_frame_interval,
};

static const struct v4l2_subdev_ops bf2557_subdev_ops = {
	.core	= &bf2557_core_ops,
	.video	= &bf2557_video_ops,
	.pad	= &bf2557_pad_ops,
};

static int bf2557_set_exposure_reg(struct bf2557 *bf2557, u32 exposure)
{
	int ret = 0;
	u32 cal_shutter = 0;

	cal_shutter = exposure >> 1;
	cal_shutter = cal_shutter << 1;

	ret |= bf2557_write_reg(bf2557->client,
		BF2557_REG_EXPOSURE_H,
		BF2557_FETCH_HIGH_BYTE(cal_shutter));
	ret |= bf2557_write_reg(bf2557->client,
		BF2557_REG_EXPOSURE_L,
		BF2557_FETCH_LOW_BYTE(cal_shutter));
	return ret;
}

static int bf2557_set_gain_reg(struct bf2557 *bf2557, u32 a_gain)
{
	int ret = 0;
	u32 temp_gain;

	if (a_gain < BF2557_AGAIN_MIN)
		temp_gain = BF2557_AGAIN_MIN;
	else if (a_gain > BF2557_AGAIN_MAX)
		temp_gain = BF2557_AGAIN_MAX;
	else
		temp_gain = a_gain - 1;

	ret |= bf2557_write_reg(bf2557->client,
		BF2557_REG_AGAIN,
		temp_gain);

	return ret;
}

static int bf2557_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct bf2557 *bf2557 = container_of(ctrl->handler,
					struct bf2557, ctrl_handler);
	struct i2c_client *client = bf2557->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = bf2557->cur_mode->height + ctrl->val - 16;
		__v4l2_ctrl_modify_range(bf2557->exposure,
					 bf2557->exposure->minimum, max,
					 bf2557->exposure->step,
					 bf2557->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		dev_dbg(&client->dev, "set exposure value 0x%x\n", ctrl->val);
		ret = bf2557_set_exposure_reg(bf2557, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set analog gain value 0x%x\n", ctrl->val);
		ret = bf2557_set_gain_reg(bf2557, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		/*register {0x07,0x06} = The minimum frame length – 1970 */
		val = ctrl->val + bf2557->cur_mode->height - 1970;
		dev_info(&client->dev, "ctrl->val:%d, actual vb value 0x%x\n",
			 ctrl->val, val);
		ret = bf2557_write_reg(bf2557->client,
					BF2557_REG_VTS_H,
					val >> 8);
		ret |= bf2557_write_reg(bf2557->client,
					BF2557_REG_VTS_L,
					val & 0xff);
		break;
	case V4L2_CID_HFLIP:
		dev_info(&client->dev, "set mirror value 0x%x\n", ctrl->val);

		if (ctrl->val)
			bf2557->flip_mirror |= MIRROR_BIT_MASK;
		else
			bf2557->flip_mirror &= ~MIRROR_BIT_MASK;

		ret |= bf2557_write_reg(bf2557->client, BF2557_FLIP_MIRROR_REG,
					 bf2557->flip_mirror);
		break;
	case V4L2_CID_VFLIP:
		dev_info(&client->dev, "set flip value 0x%x\n", ctrl->val);
		if (ctrl->val)
			bf2557->flip_mirror |= FLIP_BIT_MASK;
		else
			bf2557->flip_mirror &= ~FLIP_BIT_MASK;

		ret |= bf2557_write_reg(bf2557->client, BF2557_FLIP_MIRROR_REG,
					 bf2557->flip_mirror);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops bf2557_ctrl_ops = {
	.s_ctrl = bf2557_set_ctrl,
};

static int bf2557_initialize_controls(struct bf2557 *bf2557)
{
	const struct bf2557_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &bf2557->ctrl_handler;
	mode = bf2557->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &bf2557->mutex;

	bf2557->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ, 2, 0,
				link_freq_menu_items);

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			0, bf2557->pixel_rate, 1, bf2557->pixel_rate);

	__v4l2_ctrl_s_ctrl(bf2557->link_freq,
			   mode->mipi_freq_idx);

	h_blank = mode->hts_def - mode->width;
	bf2557->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (bf2557->hblank)
		bf2557->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	bf2557->vblank = v4l2_ctrl_new_std(handler, &bf2557_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				BF2557_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 4;
	bf2557->exposure = v4l2_ctrl_new_std(handler, &bf2557_ctrl_ops,
				V4L2_CID_EXPOSURE, BF2557_EXPOSURE_MIN,
				exposure_max, BF2557_EXPOSURE_STEP,
				mode->exp_def);

	bf2557->anal_gain = v4l2_ctrl_new_std(handler, &bf2557_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, BF2557_AGAIN_MIN,
				BF2557_AGAIN_MAX, BF2557_AGAIN_STEP,
				BF2557_AGAIN_DEFAULT);
	bf2557->h_flip = v4l2_ctrl_new_std(handler, &bf2557_ctrl_ops,
					   V4L2_CID_HFLIP, 0, 1, 1, 0);

	bf2557->v_flip = v4l2_ctrl_new_std(handler, &bf2557_ctrl_ops,
					   V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&bf2557->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	bf2557->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int bf2557_check_sensor_id(struct bf2557 *bf2557,
				struct i2c_client *client)
{
	struct device *dev = &bf2557->client->dev;
	u32 id = 0;
	u8 reg_H = 0;
	u8 reg_L = 0;
	int ret;

	ret = bf2557_read_reg(client, BF2557_REG_CHIP_ID_H, &reg_H);
	ret |= bf2557_read_reg(client, BF2557_REG_CHIP_ID_L, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected BF%04x sensor\n", id);
	return ret;
}

static int bf2557_configure_regulators(struct bf2557 *bf2557)
{
	unsigned int i;

	for (i = 0; i < BF2557_NUM_SUPPLIES; i++)
		bf2557->supplies[i].supply = bf2557_supply_names[i];

	return devm_regulator_bulk_get(&bf2557->client->dev,
		BF2557_NUM_SUPPLIES,
		bf2557->supplies);
}

static int bf2557_parse_of(struct bf2557 *bf2557)
{
	struct device *dev = &bf2557->client->dev;
	struct device_node *endpoint;
	struct fwnode_handle *fwnode;
	int rval;
	unsigned int fps;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}
	fwnode = of_fwnode_handle(endpoint);
	rval = fwnode_property_read_u32_array(fwnode, "data-lanes", NULL, 0);
	if (rval <= 0) {
		dev_warn(dev, " Get mipi lane num failed!\n");
		return -1;
	}

	bf2557->lane_num = rval;
	if (2 == bf2557->lane_num) {
		bf2557->cur_mode = &supported_modes_2lane[0];
		bf2557->support_modes = supported_modes_2lane;
		bf2557->cfg_num = ARRAY_SIZE(supported_modes_2lane);
		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		fps = DIV_ROUND_CLOSEST(bf2557->cur_mode->max_fps.denominator,
					bf2557->cur_mode->max_fps.numerator);
		bf2557->pixel_rate = bf2557->cur_mode->vts_def *
				     bf2557->cur_mode->hts_def * fps;

		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
			 bf2557->lane_num, bf2557->pixel_rate);
	} else {
		/* TODO*/
		dev_err(dev, "unsupported lane_num(%d)\n", bf2557->lane_num);
		return -1;
	}

	return 0;
}

static int bf2557_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct bf2557 *bf2557;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	bf2557 = devm_kzalloc(dev, sizeof(*bf2557), GFP_KERNEL);
	if (!bf2557)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
		&bf2557->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
		&bf2557->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
		&bf2557->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
		&bf2557->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}
	bf2557->client = client;

	bf2557->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(bf2557->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	bf2557->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(bf2557->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");

	bf2557->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(bf2557->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	bf2557->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(bf2557->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = bf2557_configure_regulators(bf2557);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ret = bf2557_parse_of(bf2557);
	if (ret != 0)
		return -EINVAL;

	bf2557->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(bf2557->pinctrl)) {
		bf2557->pins_default =
			pinctrl_lookup_state(bf2557->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(bf2557->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		bf2557->pins_sleep =
			pinctrl_lookup_state(bf2557->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(bf2557->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&bf2557->mutex);

	sd = &bf2557->subdev;
	v4l2_i2c_subdev_init(sd, client, &bf2557_subdev_ops);
	ret = bf2557_initialize_controls(bf2557);
	if (ret)
		goto err_destroy_mutex;

	ret = __bf2557_power_on(bf2557);
	if (ret)
		goto err_free_handler;

	ret = bf2557_check_sensor_id(bf2557, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &bf2557_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	bf2557->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &bf2557->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(bf2557->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 bf2557->module_index, facing,
		 BF2557_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__bf2557_power_off(bf2557);
err_free_handler:
	v4l2_ctrl_handler_free(&bf2557->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&bf2557->mutex);

	return ret;
}

static void bf2557_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf2557 *bf2557 = to_bf2557(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&bf2557->ctrl_handler);
	mutex_destroy(&bf2557->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__bf2557_power_off(bf2557);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id bf2557_of_match[] = {
	{ .compatible = "byd,bf2557" },
	{},
};
MODULE_DEVICE_TABLE(of, bf2557_of_match);
#endif

static const struct i2c_device_id bf2557_match_id[] = {
	{ "byd,bf2557", 0},
	{ },
};

static struct i2c_driver bf2557_i2c_driver = {
	.driver = {
		.name = BF2557_NAME,
		.pm = &bf2557_pm_ops,
		.of_match_table = of_match_ptr(bf2557_of_match),
	},
	.probe		= bf2557_probe,
	.remove		= bf2557_remove,
	.id_table	= bf2557_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&bf2557_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&bf2557_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("BF2557 CMOS Image Sensor driver");
MODULE_LICENSE("GPL");
