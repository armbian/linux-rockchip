// SPDX-License-Identifier: GPL-2.0
/*
 * os05l10 driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 init sensor driver.
 */
//#define DEBUG
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
#include <linux/of_graph.h>
#include "otp_eeprom.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define OS05L10_LANES			2
#define OS05L10_BITS_PER_SAMPLE		10
#define OS05L10_MIPI_FREQ_420MHZ	420000000U

/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define OS05L10_PIXEL_RATE		288000000
#define OS05L10_XVCLK_FREQ		24000000

#define CHIP_ID				0x104c
#define OS05L10_REG_CHIP_ID_H		0x00
#define OS05L10_REG_CHIP_ID_L		0x01

#define OS05L10_REG_SET_PAGE		0xfd
#define OS05L10_SET_PAGE_ZERO		0x00

#define OS05L10_REG_CTRL_MODE		0xa0
#define OS05L10_MODE_SW_STANDBY		0x00
#define OS05L10_MODE_STREAMING		0x01

#define OS05L10_REG_EXPOSURE_H		0x03
#define OS05L10_REG_EXPOSURE_L		0x04
#define OS05L10_FETCH_HIGH_BYTE_EXP(VAL) (((VAL) >> 8) & 0xFF)	/* 8 Bits */
#define OS05L10_FETCH_LOW_BYTE_EXP(VAL)	((VAL) & 0xFF)	/* 8 Bits */
#define	OS05L10_EXPOSURE_MIN		4
#define	OS05L10_EXPOSURE_STEP		1
#define OS05L10_VTS_MAX			0x1fff

#define OS05L10_REG_AGAIN_H		0x25
#define OS05L10_REG_AGAIN_L		0x24
#define OS05L10_GAIN_MIN		0x10
#define OS05L10_GAIN_MAX		0x1F8
#define OS05L10_GAIN_STEP		1
#define OS05L10_GAIN_DEFAULT		0x20
#define OS05L10_FETCH_H_BYTE_GAIN(VAL) (((VAL) >> 8) & 0x1)	/* 1 Bits */
#define OS05L10_FETCH_L_BYTE_GAIN(VAL)	((VAL) & 0xFF)	/* 8 Bits */

#define OS05L10_REG_TRIGGER		0x01

#define OS05L10_REG_VTS_H		0x05
#define OS05L10_REG_VTS_L		0x06

#define OS05L10_REG_TEST_PATTERN	0x12
#define	OS05L10_TEST_PATTERN_ENABLE	0x01
#define	OS05L10_TEST_PATTERN_DISABLE	0x0

#define OS05L10_FLIP_MIRROR_REG		0x32
#define MIRROR_BIT_MASK			BIT(0)
#define FLIP_BIT_MASK			BIT(1)

#define REG_NULL			0xFF

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define OS05L10_NAME			"os05l10"
#define OS05L10_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SBGGR10_1X10

/* use RK_OTP or old mode */
#define RK_OTP
/* choose 2lane support full 30fps or 15fps */
#define OS05L10_2LANE_30FPS

static const char * const os05l10_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
	"avdd",		/* Analog power */
};

#define OS05L10_NUM_SUPPLIES ARRAY_SIZE(os05l10_supply_names)

struct regval {
	u8 addr;
	u8 val;
};

struct os05l10_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	const struct regval *global_reg_list;
	const struct regval *reg_list;
	u32 vc[PAD_MAX];
};

struct os05l10 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[OS05L10_NUM_SUPPLIES];
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
	struct v4l2_ctrl	*test_pattern;
	struct v4l2_ctrl	*h_flip;
	struct v4l2_ctrl	*v_flip;
	struct mutex		mutex;
	bool			streaming;
	unsigned int		lane_num;
	unsigned int		cfg_num;
	unsigned int		pixel_rate;
	bool			power_on;
	const struct os05l10_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32 Dgain_ratio;
	struct otp_info		*otp;
	struct rkmodule_inf	module_inf;
	struct rkmodule_awb_cfg	awb_cfg;
};

#define to_os05l10(sd) container_of(sd, struct os05l10, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval os05l10_global_regs_2lane[] = {
	{REG_NULL, 0x00},
};

/*
 * Input clock frequency: 24MHz
 * mipi_datarate per lane 840Mbps
 * hts = 400*8, vts=1750
 * pclk=168M row_clk=21M
 * Image output size: 2880x 1620
 * frame rate = 21M/400/1750 = 30fps
 */
static const struct regval os05l10_2880x1620_regs_2lane[] = {
	{0xfd, 0x00},
	{0x20, 0x00},
	{0xfd, 0x00},
	{0x20, 0x2b},
	{0xe7, 0x03},
	{0xe7, 0x00},
	{0xfd, 0x00},
	{0x21, 0x06},
	{0x14, 0x8c},
	{0x18, 0x61},
	{0x19, 0x80},
	{0x1a, 0x06},
	{0x1b, 0x69},
	{0x1c, 0x04},
	{0x1d, 0x02},
	{0xfd, 0x00},
	{0x21, 0x00},

	{0xfd, 0x0f},
	{0x01, 0xe6},
	{0x07, 0x0f},
	{0x08, 0xf0},
	{0x0f, 0x08},
	{0x15, 0x28},
	{0x16, 0x11},
	{0x20, 0x06},
	{0x2e, 0x1c},
	{0x2f, 0x3d},
	{0x30, 0x77},
	{0x31, 0xe5},
	{0x0b, 0x01},
	{0x13, 0x22},
	{0x14, 0xbc},
	{0x2d, 0x0a},
	{0x17, 0x3e},
	{0x1b, 0xe6},
	{0x1c, 0x99},
	{0x1d, 0x99},
	{0x1e, 0x55},
	{0xfd, 0x01},
	{0x02, 0x00},
	{0x03, 0x02},
	{0x04, 0x04},
	{0x06, 0x5c},
	{0x24, 0x80},
	{0x21, 0x00},
	{0x22, 0x40},
	{0x31, 0x00},
	{0x33, 0x03},
	{0x40, 0x30},
	{0x41, 0x0c},
	{0x43, 0x44},
	{0x44, 0x0b},
	{0x46, 0x01},
	{0x48, 0x08},
	{0x4c, 0x10},
	{0x50, 0x0a},
	{0x51, 0x08},
	{0x52, 0x08},
	{0x53, 0x0a},
	{0x54, 0x00},
	{0x55, 0x00},
	{0x57, 0x92},
	{0x58, 0x00},
	{0x59, 0x05},
	{0x5a, 0x05},
	{0x5b, 0x00},
	{0x5c, 0x0b},
	{0x5e, 0x08},
	{0x66, 0x0d},
	{0x69, 0x0d},
	{0x76, 0x0e},
	{0x7a, 0x00},
	{0x7c, 0x03},
	{0x83, 0x03},
	{0x84, 0x25},
	{0x85, 0x03},
	{0x87, 0x03},
	{0x90, 0x33},
	{0x91, 0x1e},
	{0x92, 0x15},
	{0x93, 0x16},
	{0x94, 0x08},
	{0x95, 0x33},
	{0x9c, 0x03},
	{0x9d, 0x13},
	{0x9e, 0x03},
	{0xa0, 0x13},
	{0xa4, 0x01},
	{0xa5, 0x01},
	{0xa7, 0x01},
	{0xc0, 0x09},
	{0xc1, 0x38},
	{0xc8, 0x1f},
	{0xd7, 0x11},
	{0xd9, 0x1c},
	{0xd8, 0x1f},
	{0xda, 0x2f},
	{0xdb, 0x10},
	{0xdd, 0x1b},
	{0xdc, 0x1e},
	{0xde, 0x2d},
	{0xed, 0x33},
	{0xee, 0x33},
	{0x01, 0x01},
	{0xfd, 0x02},
	{0x9a, 0x33},
	{0x05, 0x01},
	{0x0b, 0x0c},
	{0x0c, 0x0f},
	{0xfd, 0x01},
	{0x01, 0x01},
	{0xfd, 0x04},
	{0x19, 0x3f},
	{0x12, 0x00},
	{0xf3, 0x00},
	{0xfd, 0x07},
	{0x10, 0xf0},
	{0x42, 0x00},
	{0x43, 0x76},
	{0x44, 0x00},
	{0x45, 0x76},
	{0x46, 0x00},
	{0x47, 0x76},
	{0x48, 0x00},
	{0x49, 0x76},
	{0xb3, 0x02},
	{0xb4, 0x20},
	{0xb7, 0x02},
	{0xb8, 0x20},
	{0xc5, 0x3a},
	{0xc9, 0x3a},
	{0xcd, 0x3a},
	{0xc6, 0x2b},
	{0xca, 0x2b},
	{0xce, 0x2b},
	{0xc3, 0x0a},
	{0xc7, 0x0a},
	{0xcb, 0x0a},
	{0xc4, 0x12},
	{0xc8, 0x12},
	{0xcc, 0x12},
	{0xcf, 0x11},
	{0xd3, 0x11},
	{0xd7, 0x11},
	{0xd0, 0x0e},
	{0xd4, 0x0e},
	{0xd8, 0x0e},
	{0xbb, 0x7f},
	{0xd1, 0x0f},
	{0xd5, 0x0f},
	{0xd9, 0x0f},
	{0xd2, 0x0a},
	{0xd6, 0x0a},
	{0xda, 0x0a},
	{0xbc, 0x3f},
	{0xfd, 0x03},
	{0x85, 0x03},
	{0x9d, 0x0f},
	{0xba, 0x06},
	{0xfd, 0x01},
	{0xfd, 0x02},
	{0xa1, 0x04},
	{0xa3, 0x54},
	{0xa5, 0x04},
	{0xa7, 0x40},
	{0xfd, 0x00},
	{0x8e, 0x0b},
	{0x8f, 0x40},
	{0x90, 0x06},
	{0x91, 0x54},
	{0x94, 0x08},
	{0x95, 0x09},
	{0x99, 0x08},
	{0x9c, 0x20},
	{0xa4, 0x0c},
	{0x9d, 0x01},
	{0xa5, 0xff},
	{0xa1, 0x04},
	{0xb7, 0x02}, //mipi clk mode:0x00 continue; 0x02 no continue

	{0xc1, 0xee},
	{0xc5, 0x50},
	{0xc4, 0x01},
	{0xa0, 0x00},
	{0xfd, 0x01},
	{0xfd, 0x01},
	{0x01, 0x31},
	{0xfd, 0x00},
	{0x20, 0x1f},
	{0xfd, 0x01},
	{0xfd, 0x01},
	{REG_NULL, 0x00},
};

static const struct os05l10_mode supported_modes_2lane[] = {
	{
		.width = 2880,
		.height = 1620,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0600,
		.hts_def = 0x0190 * 8,
		.vts_def = 0x06d6,
		.mipi_freq_idx = 0,
		.global_reg_list = os05l10_global_regs_2lane,
		.reg_list = os05l10_2880x1620_regs_2lane,
		.vc[PAD0] = 0,
	},
};

static const struct os05l10_mode *supported_modes;

static const s64 link_freq_menu_items[] = {
	OS05L10_MIPI_FREQ_420MHZ,
};

static const char * const os05l10_test_pattern_menu[] = {
	"Disabled",
	"ColorBar"
};

/* Write registers up to 4 at a time */
static int os05l10_write_reg(struct i2c_client *client, u8 reg, u8 val)
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
		"os05l10 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int os05l10_write_array(struct i2c_client *client,
	const struct regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = os05l10_write_reg(client, regs[i].addr, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int os05l10_read_reg(struct i2c_client *client, u8 reg, u8 *val)
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
		"os05l10 read reg:0x%x failed !\n", reg);

	return ret;
}

static int os05l10_get_reso_dist(const struct os05l10_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		abs(mode->height - framefmt->height);
}

static const struct os05l10_mode *
os05l10_find_best_fit(struct os05l10 *os05l10,
		     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < os05l10->cfg_num; i++) {
		dist = os05l10_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int os05l10_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_format *fmt)
{
	struct os05l10 *os05l10 = to_os05l10(sd);
	const struct os05l10_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&os05l10->mutex);

	mode = os05l10_find_best_fit(os05l10, fmt);
	fmt->format.code = OS05L10_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&os05l10->mutex);
		return -ENOTTY;
#endif
	} else {
		os05l10->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(os05l10->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(os05l10->vblank, vblank_def,
					 OS05L10_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(os05l10->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(os05l10->link_freq, mode->mipi_freq_idx);
	}

	mutex_unlock(&os05l10->mutex);

	return 0;
}

static int os05l10_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_format *fmt)
{
	struct os05l10 *os05l10 = to_os05l10(sd);
	const struct os05l10_mode *mode = os05l10->cur_mode;

	mutex_lock(&os05l10->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&os05l10->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = OS05L10_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&os05l10->mutex);

	return 0;
}

static int os05l10_enum_mbus_code(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = OS05L10_MEDIA_BUS_FMT;

	return 0;
}

static int os05l10_enum_frame_sizes(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct os05l10 *os05l10 = to_os05l10(sd);

	if (fse->index >= os05l10->cfg_num)
		return -EINVAL;

	if (fse->code != OS05L10_MEDIA_BUS_FMT)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int os05l10_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *fi)
{
	struct os05l10 *os05l10 = to_os05l10(sd);
	const struct os05l10_mode *mode = os05l10->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static void os05l10_get_otp(struct otp_info *otp,
				   struct rkmodule_inf *inf)
{
	u32 i, j;
	u32 w, h;

	/* awb */
	if (otp->awb_data.flag) {
		inf->awb.flag = 1;
		inf->awb.r_value = otp->awb_data.r_ratio;
		inf->awb.b_value = otp->awb_data.b_ratio;
		inf->awb.gr_value = otp->awb_data.g_ratio;
		inf->awb.gb_value = 0x0;

		inf->awb.golden_r_value = otp->awb_data.r_golden;
		inf->awb.golden_b_value = otp->awb_data.b_golden;
		inf->awb.golden_gr_value = otp->awb_data.g_golden;
		inf->awb.golden_gb_value = 0x0;
	}

	/* lsc */
	if (otp->lsc_data.flag) {
		inf->lsc.flag = 1;
		inf->lsc.width = otp->basic_data.size.width;
		inf->lsc.height = otp->basic_data.size.height;
		inf->lsc.table_size = otp->lsc_data.table_size;

		for (i = 0; i < 289; i++) {
			inf->lsc.lsc_r[i] = (otp->lsc_data.data[i * 2] << 8) |
						 otp->lsc_data.data[i * 2 + 1];
			inf->lsc.lsc_gr[i] = (otp->lsc_data.data[i * 2 + 578] << 8) |
						  otp->lsc_data.data[i * 2 + 579];
			inf->lsc.lsc_gb[i] = (otp->lsc_data.data[i * 2 + 1156] << 8) |
						  otp->lsc_data.data[i * 2 + 1157];
			inf->lsc.lsc_b[i] = (otp->lsc_data.data[i * 2 + 1734] << 8) |
						 otp->lsc_data.data[i * 2 + 1735];
		}
	}

	/* pdaf */
	if (otp->pdaf_data.flag) {
		inf->pdaf.flag = 1;
		inf->pdaf.gainmap_width = otp->pdaf_data.gainmap_width;
		inf->pdaf.gainmap_height = otp->pdaf_data.gainmap_height;
		inf->pdaf.pd_offset = otp->pdaf_data.pd_offset;
		inf->pdaf.dcc_mode = otp->pdaf_data.dcc_mode;
		inf->pdaf.dcc_dir = otp->pdaf_data.dcc_dir;
		inf->pdaf.dccmap_width = otp->pdaf_data.dccmap_width;
		inf->pdaf.dccmap_height = otp->pdaf_data.dccmap_height;
		w = otp->pdaf_data.gainmap_width;
		h = otp->pdaf_data.gainmap_height;
		for (i = 0; i < h; i++) {
			for (j = 0; j < w; j++) {
				inf->pdaf.gainmap[i * w + j] =
					(otp->pdaf_data.gainmap[(i * w + j) * 2] << 8) |
					otp->pdaf_data.gainmap[(i * w + j) * 2 + 1];
			}
		}
		w = otp->pdaf_data.dccmap_width;
		h = otp->pdaf_data.dccmap_height;
		for (i = 0; i < h; i++) {
			for (j = 0; j < w; j++) {
				inf->pdaf.dccmap[i * w + j] =
					(otp->pdaf_data.dccmap[(i * w + j) * 2] << 8) |
					otp->pdaf_data.dccmap[(i * w + j) * 2 + 1];
			}
		}
	}

	/* af */
	if (otp->af_data.flag) {
		inf->af.flag = 1;
		inf->af.dir_cnt = 1;
		inf->af.af_otp[0].vcm_start = otp->af_data.af_inf;
		inf->af.af_otp[0].vcm_end = otp->af_data.af_macro;
		inf->af.af_otp[0].vcm_dir = 0;
	}

}

static void os05l10_get_module_inf(struct os05l10 *os05l10,
				struct rkmodule_inf *inf)
{
	struct otp_info *otp = os05l10->otp;

	strscpy(inf->base.sensor,
		OS05L10_NAME,
		sizeof(inf->base.sensor));
	strscpy(inf->base.module,
		os05l10->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens,
		os05l10->len_name,
		sizeof(inf->base.lens));
	if (otp)
		os05l10_get_otp(otp, inf);
}

static void os05l10_set_module_inf(struct os05l10 *os05l10,
				struct rkmodule_awb_cfg *cfg)
{
	mutex_lock(&os05l10->mutex);
	memcpy(&os05l10->awb_cfg, cfg, sizeof(*cfg));
	mutex_unlock(&os05l10->mutex);
}

static int os05l10_get_channel_info(struct os05l10 *os05l10, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = os05l10->cur_mode->vc[ch_info->index];
	ch_info->width = os05l10->cur_mode->width;
	ch_info->height = os05l10->cur_mode->height;
	ch_info->bus_fmt = OS05L10_MEDIA_BUS_FMT;
	return 0;
}

static long os05l10_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct os05l10 *os05l10 = to_os05l10(sd);
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		os05l10_get_module_inf(os05l10, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_AWB_CFG:
		os05l10_set_module_inf(os05l10, (struct rkmodule_awb_cfg *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);
		if (stream) {
			ret = os05l10_write_reg(os05l10->client,
					       OS05L10_REG_SET_PAGE,
					       0x01);
			ret |= os05l10_write_reg(os05l10->client,
						OS05L10_REG_CTRL_MODE,
						OS05L10_MODE_STREAMING);
		} else {
			ret = os05l10_write_reg(os05l10->client,
					       OS05L10_REG_SET_PAGE,
					       0x01);
			ret |= os05l10_write_reg(os05l10->client,
						OS05L10_REG_CTRL_MODE,
						OS05L10_MODE_SW_STANDBY);
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = os05l10_get_channel_info(os05l10, ch_info);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long os05l10_compat_ioctl32(struct v4l2_subdev *sd,
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

		ret = os05l10_ioctl(sd, cmd, inf);
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
			ret = os05l10_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = os05l10_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = os05l10_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}
#endif

static int __os05l10_start_stream(struct os05l10 *os05l10)
{
	int ret;

	ret = os05l10_write_array(os05l10->client, os05l10->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&os05l10->mutex);
	ret = v4l2_ctrl_handler_setup(&os05l10->ctrl_handler);
	mutex_lock(&os05l10->mutex);
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_SET_PAGE,
		OS05L10_SET_PAGE_ZERO);
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_CTRL_MODE,
		OS05L10_MODE_STREAMING);

	return ret;
}

static int __os05l10_stop_stream(struct os05l10 *os05l10)
{
	int ret;

	ret = os05l10_write_reg(os05l10->client,
		OS05L10_REG_SET_PAGE,
		OS05L10_SET_PAGE_ZERO);
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_CTRL_MODE,
		OS05L10_MODE_STREAMING);


	return ret;
}

static int os05l10_s_stream(struct v4l2_subdev *sd, int on)
{
	struct os05l10 *os05l10 = to_os05l10(sd);
	struct i2c_client *client = os05l10->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				os05l10->cur_mode->width,
				os05l10->cur_mode->height,
		DIV_ROUND_CLOSEST(os05l10->cur_mode->max_fps.denominator,
		os05l10->cur_mode->max_fps.numerator));

	mutex_lock(&os05l10->mutex);
	on = !!on;
	if (on == os05l10->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __os05l10_start_stream(os05l10);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__os05l10_stop_stream(os05l10);
		pm_runtime_put(&client->dev);
	}

	os05l10->streaming = on;

unlock_and_return:
	mutex_unlock(&os05l10->mutex);

	return ret;
}

static int os05l10_s_power(struct v4l2_subdev *sd, int on)
{
	struct os05l10 *os05l10 = to_os05l10(sd);
	struct i2c_client *client = os05l10->client;
	const struct os05l10_mode *mode = os05l10->cur_mode;
	int ret = 0;

	dev_info(&client->dev, "%s(%d) on(%d)\n", __func__, __LINE__, on);
	mutex_lock(&os05l10->mutex);

	/* If the power state is not modified - no work to do. */
	if (os05l10->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = os05l10_write_array(os05l10->client, mode->global_reg_list);
		if (ret) {
			v4l2_err(sd, "could not set init registers\n");
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		os05l10->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		os05l10->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&os05l10->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 os05l10_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, OS05L10_XVCLK_FREQ / 1000 / 1000);
}

static int os05l10_enable_regulators(struct os05l10 *os05l10,
				    struct regulator_bulk_data *consumers)
{
	int i, j;
	int ret = 0;
	struct device *dev = &os05l10->client->dev;
	int num_consumers = OS05L10_NUM_SUPPLIES;

	for (i = 0; i < num_consumers; i++) {

		ret = regulator_enable(consumers[i].consumer);
		if (ret < 0) {
			dev_err(dev, "Failed to enable regulator: %s\n",
				consumers[i].supply);
			goto err;
		}
	}
	return 0;
err:
	for (j = 0; j < i; j++)
		regulator_disable(consumers[j].consumer);

	return ret;
}

static int __os05l10_power_on(struct os05l10 *os05l10)
{
	int ret;
	struct device *dev = &os05l10->client->dev;

	if (!IS_ERR(os05l10->power_gpio))
		gpiod_set_value_cansleep(os05l10->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(os05l10->pins_default)) {
		ret = pinctrl_select_state(os05l10->pinctrl,
					   os05l10->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(os05l10->xvclk, OS05L10_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(os05l10->xvclk) != OS05L10_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");

	if (!IS_ERR(os05l10->reset_gpio))
		gpiod_set_value_cansleep(os05l10->reset_gpio, 0);

	ret = os05l10_enable_regulators(os05l10, os05l10->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	ret = clk_prepare_enable(os05l10->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	usleep_range(1000, 1100);
	if (!IS_ERR(os05l10->pwdn_gpio))
		gpiod_set_value_cansleep(os05l10->pwdn_gpio, 1);

	/*delay from dvdd stable to sensor XSHUTDN pull up*/
	usleep_range(5000, 5500);
	if (!IS_ERR(os05l10->reset_gpio))
		gpiod_set_value_cansleep(os05l10->reset_gpio, 1);

	/*delay from XSHUTDN pull up to  SCCB initialization*/
	usleep_range(8000, 8500);

	return 0;

disable_clk:
	clk_disable_unprepare(os05l10->xvclk);

	return ret;
}

static void __os05l10_power_off(struct os05l10 *os05l10)
{
	int ret;

	if (!IS_ERR(os05l10->pwdn_gpio))
		gpiod_set_value_cansleep(os05l10->pwdn_gpio, 1);

	if (!IS_ERR(os05l10->reset_gpio))
		gpiod_set_value_cansleep(os05l10->reset_gpio, 1);

	clk_disable_unprepare(os05l10->xvclk);
	if (!IS_ERR_OR_NULL(os05l10->pins_sleep)) {
		ret = pinctrl_select_state(os05l10->pinctrl,
					   os05l10->pins_sleep);
		if (ret < 0)
			dev_dbg(&os05l10->client->dev, "could not set pins\n");
	}
	if (!IS_ERR(os05l10->power_gpio))
		gpiod_set_value_cansleep(os05l10->power_gpio, 0);

	regulator_bulk_disable(OS05L10_NUM_SUPPLIES, os05l10->supplies);
}

static int __maybe_unused os05l10_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct os05l10 *os05l10 = to_os05l10(sd);

	return __os05l10_power_on(os05l10);
}

static int __maybe_unused os05l10_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct os05l10 *os05l10 = to_os05l10(sd);

	__os05l10_power_off(os05l10);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int os05l10_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct os05l10 *os05l10 = to_os05l10(sd);
	struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct os05l10_mode *def_mode = &supported_modes[0];

	mutex_lock(&os05l10->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = OS05L10_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&os05l10->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int os05l10_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct os05l10 *os05l10 = to_os05l10(sd);

	if (fie->index >= os05l10->cfg_num)
		return -EINVAL;

	fie->code = OS05L10_MEDIA_BUS_FMT;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static int os05l10_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct os05l10 *sensor = to_os05l10(sd);
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

static const struct dev_pm_ops os05l10_pm_ops = {
	SET_RUNTIME_PM_OPS(os05l10_runtime_suspend,
			os05l10_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops os05l10_internal_ops = {
	.open = os05l10_open,
};
#endif

static const struct v4l2_subdev_core_ops os05l10_core_ops = {
	.s_power = os05l10_s_power,
	.ioctl = os05l10_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = os05l10_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops os05l10_video_ops = {
	.s_stream = os05l10_s_stream,
	.g_frame_interval = os05l10_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops os05l10_pad_ops = {
	.enum_mbus_code = os05l10_enum_mbus_code,
	.enum_frame_size = os05l10_enum_frame_sizes,
	.enum_frame_interval = os05l10_enum_frame_interval,
	.get_fmt = os05l10_get_fmt,
	.set_fmt = os05l10_set_fmt,
	.get_mbus_config = os05l10_g_mbus_config,
};

static const struct v4l2_subdev_ops os05l10_subdev_ops = {
	.core	= &os05l10_core_ops,
	.video	= &os05l10_video_ops,
	.pad	= &os05l10_pad_ops,
};

static int os05l10_set_exposure_reg(struct os05l10 *os05l10, u32 exposure)
{
	int ret = 0;
	u32 cal_shutter = 0;

	cal_shutter = exposure >> 1;
	cal_shutter = cal_shutter << 1;

	ret = os05l10_write_reg(os05l10->client,
		OS05L10_REG_SET_PAGE, 0x01);
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_EXPOSURE_H,
		OS05L10_FETCH_HIGH_BYTE_EXP(cal_shutter));
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_EXPOSURE_L,
		OS05L10_FETCH_LOW_BYTE_EXP(cal_shutter));
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_TRIGGER, 0x01);

	return ret;
}

static int os05l10_set_gain_reg(struct os05l10 *os05l10, u32 a_gain)
{
	int ret = 0;

	ret = os05l10_write_reg(os05l10->client,
		OS05L10_REG_SET_PAGE, 0x01);
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_AGAIN_H,
		OS05L10_FETCH_H_BYTE_GAIN(a_gain));
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_AGAIN_L,
		OS05L10_FETCH_L_BYTE_GAIN(a_gain));
	ret |= os05l10_write_reg(os05l10->client,
		OS05L10_REG_TRIGGER, 0x01);

	return ret;
}

static int os05l10__enable_test_pattern(struct os05l10 *os05l10, u32 pattern)
{
	u32 ret;

	ret = os05l10_write_reg(os05l10->client,
		OS05L10_REG_SET_PAGE, 0x04);

	if (pattern) {
		ret |= os05l10_write_reg(os05l10->client,
			0xF3, 0x02);
		ret |= os05l10_write_reg(os05l10->client,
			OS05L10_REG_TEST_PATTERN, 0x01);
	} else {
		ret |= os05l10_write_reg(os05l10->client,
			0xF3, 0x00);
		ret |= os05l10_write_reg(os05l10->client,
			OS05L10_REG_TEST_PATTERN, 0x00);
	}

	return ret;
}

static int os05l10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct os05l10 *os05l10 = container_of(ctrl->handler,
					struct os05l10, ctrl_handler);
	struct i2c_client *client = os05l10->client;
	s64 max;
	int ret = 0;
	s32 temp;
	u8 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = os05l10->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(os05l10->exposure,
					 os05l10->exposure->minimum, max,
					 os05l10->exposure->step,
					 os05l10->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		dev_dbg(&client->dev, "set exposure value 0x%x\n", ctrl->val);
		ret = os05l10_set_exposure_reg(os05l10, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set analog gain value 0x%x\n", ctrl->val);
		ret = os05l10_set_gain_reg(os05l10, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vb value 0x%x\n", ctrl->val);
		/* VB = VTS - height -38, */
		temp = ctrl->val + os05l10->cur_mode->height - 1620 - 38;

		ret = os05l10_write_reg(os05l10->client,
					OS05L10_REG_SET_PAGE,
					0x01);
		ret |= os05l10_write_reg(os05l10->client,
					OS05L10_REG_VTS_H,
					(temp >> 8) & 0xff);
		ret |= os05l10_write_reg(os05l10->client,
					OS05L10_REG_VTS_L,
					temp & 0xff);
		ret |= os05l10_write_reg(os05l10->client,
					OS05L10_REG_TRIGGER,
					0x01);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = os05l10__enable_test_pattern(os05l10, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		dev_info(&client->dev, "set mirror value 0x%x\n", ctrl->val);

		ret = os05l10_write_reg(os05l10->client,
					OS05L10_REG_SET_PAGE,
					0x01);
		ret |= os05l10_read_reg(client, OS05L10_FLIP_MIRROR_REG, &val);
		if (ctrl->val)
			val |= MIRROR_BIT_MASK;
		else
			val &= ~MIRROR_BIT_MASK;
		ret |= os05l10_write_reg(os05l10->client,
					OS05L10_FLIP_MIRROR_REG,
					val);
		ret |= os05l10_write_reg(os05l10->client,
					OS05L10_REG_TRIGGER,
					0x01);
		break;
	case V4L2_CID_VFLIP:
		dev_info(&client->dev, "set flip value 0x%x\n", ctrl->val);
		ret = os05l10_write_reg(os05l10->client,
					OS05L10_REG_SET_PAGE,
					0x01);
		ret |= os05l10_read_reg(client, OS05L10_FLIP_MIRROR_REG, &val);
		if (ctrl->val)
			val |= FLIP_BIT_MASK;
		else
			val &= ~FLIP_BIT_MASK;
		ret |= os05l10_write_reg(os05l10->client,
					OS05L10_FLIP_MIRROR_REG,
					val);
		ret |= os05l10_write_reg(os05l10->client,
					OS05L10_REG_TRIGGER,
					0x01);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops os05l10_ctrl_ops = {
	.s_ctrl = os05l10_set_ctrl,
};

static int os05l10_initialize_controls(struct os05l10 *os05l10)
{
	const struct os05l10_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &os05l10->ctrl_handler;
	mode = os05l10->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &os05l10->mutex;

	os05l10->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_menu_items) - 1, 0,
				link_freq_menu_items);
	v4l2_ctrl_s_ctrl(os05l10->link_freq, mode->mipi_freq_idx);

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			0, os05l10->pixel_rate, 1, os05l10->pixel_rate);

	h_blank = mode->hts_def - mode->width;
	os05l10->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (os05l10->hblank)
		os05l10->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	os05l10->vblank = v4l2_ctrl_new_std(handler, &os05l10_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				OS05L10_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 4;
	os05l10->exposure = v4l2_ctrl_new_std(handler, &os05l10_ctrl_ops,
				V4L2_CID_EXPOSURE, OS05L10_EXPOSURE_MIN,
				exposure_max, OS05L10_EXPOSURE_STEP,
				mode->exp_def);

	os05l10->anal_gain = v4l2_ctrl_new_std(handler, &os05l10_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, OS05L10_GAIN_MIN,
				OS05L10_GAIN_MAX, OS05L10_GAIN_STEP,
				OS05L10_GAIN_DEFAULT);

	os05l10->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&os05l10_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(os05l10_test_pattern_menu) - 1,
				0, 0, os05l10_test_pattern_menu);

	os05l10->h_flip = v4l2_ctrl_new_std(handler, &os05l10_ctrl_ops,
					    V4L2_CID_HFLIP, 0, 1, 1, 0);

	os05l10->v_flip = v4l2_ctrl_new_std(handler, &os05l10_ctrl_ops,
					    V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&os05l10->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	os05l10->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int os05l10_check_sensor_id(struct os05l10 *os05l10,
				struct i2c_client *client)
{
	struct device *dev = &os05l10->client->dev;
	u16 id = 0;
	u8 reg_H = 0;
	u8 reg_L = 0;
	int ret;

	ret = os05l10_read_reg(client, OS05L10_REG_CHIP_ID_H, &reg_H);
	ret |= os05l10_read_reg(client, OS05L10_REG_CHIP_ID_L, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		//return -ENODEV;
	}
	dev_info(dev, "detected ov%04x sensor\n", id);
	return 0;
}

static int os05l10_configure_regulators(struct os05l10 *os05l10)
{
	unsigned int i;

	for (i = 0; i < OS05L10_NUM_SUPPLIES; i++)
		os05l10->supplies[i].supply = os05l10_supply_names[i];

	return devm_regulator_bulk_get(&os05l10->client->dev,
		OS05L10_NUM_SUPPLIES,
		os05l10->supplies);
}

static int os05l10_parse_of(struct os05l10 *os05l10)
{
	struct device *dev = &os05l10->client->dev;
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

	os05l10->lane_num = rval;
	if (2 == os05l10->lane_num) {
		os05l10->cur_mode = &supported_modes_2lane[0];
		supported_modes = supported_modes_2lane;
		os05l10->cfg_num = ARRAY_SIZE(supported_modes_2lane);
		/*pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		fps = DIV_ROUND_CLOSEST(os05l10->cur_mode->max_fps.denominator,
					os05l10->cur_mode->max_fps.numerator);
		os05l10->pixel_rate = os05l10->cur_mode->vts_def *
				     os05l10->cur_mode->hts_def * fps;
		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
			 os05l10->lane_num, os05l10->pixel_rate);
	} else {
		dev_err(dev, "unsupported lane_num(%d)\n", os05l10->lane_num);
		return -1;
	}

	return 0;
}

static int os05l10_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct os05l10 *os05l10;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	struct device_node *eeprom_ctrl_node;
	struct i2c_client *eeprom_ctrl_client;
	struct v4l2_subdev *eeprom_ctrl;
	struct otp_info *otp_ptr;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	os05l10 = devm_kzalloc(dev, sizeof(*os05l10), GFP_KERNEL);
	if (!os05l10)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
		&os05l10->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
		&os05l10->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
		&os05l10->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
		&os05l10->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}
	os05l10->client = client;

	os05l10->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(os05l10->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	os05l10->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(os05l10->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");
	os05l10->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(os05l10->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	os05l10->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(os05l10->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = os05l10_configure_regulators(os05l10);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ret = os05l10_parse_of(os05l10);
	if (ret != 0)
		return -EINVAL;

	os05l10->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(os05l10->pinctrl)) {
		os05l10->pins_default =
			pinctrl_lookup_state(os05l10->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(os05l10->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		os05l10->pins_sleep =
			pinctrl_lookup_state(os05l10->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(os05l10->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&os05l10->mutex);

	sd = &os05l10->subdev;
	v4l2_i2c_subdev_init(sd, client, &os05l10_subdev_ops);
	ret = os05l10_initialize_controls(os05l10);
	if (ret)
		goto err_destroy_mutex;

	ret = __os05l10_power_on(os05l10);
	if (ret)
		goto err_free_handler;

	ret = os05l10_check_sensor_id(os05l10, client);
	if (ret)
		goto err_power_off;

	eeprom_ctrl_node = of_parse_phandle(node, "eeprom-ctrl", 0);
	if (eeprom_ctrl_node) {
		eeprom_ctrl_client =
			of_find_i2c_device_by_node(eeprom_ctrl_node);
		of_node_put(eeprom_ctrl_node);
		if (IS_ERR_OR_NULL(eeprom_ctrl_client)) {
			dev_err(dev, "can not get node\n");
			goto continue_probe;
		}
		eeprom_ctrl = i2c_get_clientdata(eeprom_ctrl_client);
		if (IS_ERR_OR_NULL(eeprom_ctrl)) {
			dev_err(dev, "can not get eeprom i2c client\n");
		} else {
			otp_ptr = devm_kzalloc(dev, sizeof(*otp_ptr), GFP_KERNEL);
			if (!otp_ptr)
				return -ENOMEM;
			ret = v4l2_subdev_call(eeprom_ctrl,
				core, ioctl, 0, otp_ptr);
			if (!ret) {
				os05l10->otp = otp_ptr;
			} else {
				os05l10->otp = NULL;
				devm_kfree(dev, otp_ptr);
				dev_warn(dev, "can not get otp info, skip!\n");
			}
		}
	}
continue_probe:

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &os05l10_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	os05l10->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &os05l10->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(os05l10->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 os05l10->module_index, facing,
		 OS05L10_NAME, dev_name(sd->dev));
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
	__os05l10_power_off(os05l10);
err_free_handler:
	v4l2_ctrl_handler_free(&os05l10->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&os05l10->mutex);

	return ret;
}

static void os05l10_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct os05l10 *os05l10 = to_os05l10(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&os05l10->ctrl_handler);
	mutex_destroy(&os05l10->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__os05l10_power_off(os05l10);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id os05l10_of_match[] = {
	{ .compatible = "ovti,os05l10" },
	{},
};
MODULE_DEVICE_TABLE(of, os05l10_of_match);
#endif

static const struct i2c_device_id os05l10_match_id[] = {
	{ "ovti,os05l10", 0},
	{ },
};

static struct i2c_driver os05l10_i2c_driver = {
	.driver = {
		.name = OS05L10_NAME,
		.pm = &os05l10_pm_ops,
		.of_match_table = of_match_ptr(os05l10_of_match),
	},
	.probe		= &os05l10_probe,
	.remove		= &os05l10_remove,
	.id_table	= os05l10_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&os05l10_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&os05l10_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("OmniVision os05l10 sensor driver");
MODULE_LICENSE("GPL");
