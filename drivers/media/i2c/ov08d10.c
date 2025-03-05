// SPDX-License-Identifier: GPL-2.0
/*
 * ov08d10 driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 init version.
 * V0.0X01.0X01
 * 1. add delays in setting to fix probability reg write failed.
 * 2. remove duplicate global register setting.
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define OV08D10_LANES			2
#define OV08D10_BITS_PER_SAMPLE		10
#define OV08D10_MIPI_FREQ_720MHZ	720000000U

/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define OV08D10_PIXEL_RATE		288000000
#define OV08D10_XVCLK_FREQ		24000000

#define CHIP_ID				0x5608
#define OV08D10_REG_CHIP_ID_H		0x00
#define OV08D10_REG_CHIP_ID_L		0x01

#define OV08D10_REG_SET_PAGE		0xfd
#define OV08D10_SET_PAGE_ZERO		0x00

#define OV08D10_REG_CTRL_MODE		0xA0
#define OV08D10_MODE_SW_STANDBY		0x00
#define OV08D10_MODE_STREAMING		0x01

#define OV08D10_REG_EXPOSURE_H		0x02
#define OV08D10_REG_EXPOSURE_M		0x03
#define OV08D10_REG_EXPOSURE_L		0x04
#define OV08D10_FETCH_HIGH_BYTE_EXP(VAL) (((VAL) >> 16) & 0x7F)	/* 7 Bits */
#define OV08D10_FETCH_M_BYTE_EXP(VAL) (((VAL) >> 8) & 0xFF)	/* 8 Bits */
#define OV08D10_FETCH_LOW_BYTE_EXP(VAL)	((VAL) & 0xFF)	/* 8 Bits */
#define	OV08D10_EXPOSURE_MIN		8
#define	OV08D10_EXPOSURE_STEP		1
#define OV08D10_VTS_MAX			0x1fff

#define OV08D10_REG_AGAIN		0x24
#define OV08D10_GAIN_MIN		0x10
#define OV08D10_GAIN_MAX		0xF8
#define OV08D10_GAIN_STEP		1
#define OV08D10_GAIN_DEFAULT		16

#define OV08D10_REG_VBLANK_H		0x05
#define OV08D10_REG_VBLANK_L		0x06
#define OV08D10_UPDATE_REG	0x01

#define REG_NULL			0xFF
#define DELAY_MS			0xFE	/* Array delay token */

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define OV08D10_NAME			"ov08d10"
#define OV08D10_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SBGGR10_1X10

/* use RK_OTP or old mode */
#define RK_OTP
/* 2lane support full 30fps */
#define OV08D10_2LANE_30FPS

static const char * const ov08d10_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Digital core power */
	"dvdd",		/* Analog power */
};

#define OV08D10_NUM_SUPPLIES ARRAY_SIZE(ov08d10_supply_names)

struct regval {
	u8 addr;
	u8 val;
};

struct ov08d10_mode {
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

struct ov08d10 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[OV08D10_NUM_SUPPLIES];
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
	struct mutex		mutex;
	bool			streaming;
	unsigned int		lane_num;
	unsigned int		cfg_num;
	unsigned int		pixel_rate;
	bool			power_on;
	const struct ov08d10_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32 Dgain_ratio;
#ifdef RK_OTP
	struct otp_info		*otp;
#endif
	struct rkmodule_inf	module_inf;
	struct rkmodule_awb_cfg	awb_cfg;
};

#define to_ov08d10(sd) container_of(sd, struct ov08d10, subdev)

/*
 * Xclk 24Mhz
 * global setting
 * resolution =3264x2448 W*H
 * fps = 30
 * line_time = 12.778us
 * min_line = 8
 * ob_value = 64
 * base_gain = 1x
 * ob_value @ max_gain  = 64@15.5x	 xxx @ yyy
 * bayer pattern = BGGR
 */
static const struct regval ov08d10_global_regs_2lane[] = {
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 15fps
 * mipi_datarate per lane 672Mbps
 */
static const struct regval ov08d10_3264x2448_regs_2lane[] = {
	{0xfd,0x00},
	{0x20,0x0e},
	{DELAY_MS, 3}, //Delay 3ms
	{0x20,0x0b},
	{0xfd,0x00},
	{0x11,0x2a},
	{0x14,0x43},
	{0x1e,0x23},
	{0x16,0x82},
	{0x21,0x00},
	{0xfd,0x01},
	{0x12,0x00},
	{0x02,0x00},
	{0x03,0x12},
	{0x04,0x50},
	{0x05,0x00},
	{0x06,0xd0},
	{0x07,0x05},
	{0x21,0x02},
	{0x24,0x30},
	{0x33,0x03},
	{0x01,0x03},
	{0x19,0x10},
	{0x42,0x55},
	{0x43,0x00},
	{0x47,0x07},
	{0x48,0x08},
	{0x4c,0x38},
	{0xb2,0x7e},
	{0xb3,0x7b},
	{0xbd,0x08},
	{0xd2,0x47},
	{0xd3,0x10},
	{0xd4,0x0d},
	{0xd5,0x08},
	{0xd6,0x07},
	{0xb1,0x00},
	{0xb4,0x00},
	{0xb7,0x0a},
	{0xbc,0x44},
	{0xbf,0x42},
	{0xc1,0x10},
	{0xc3,0x24},
	{0xc8,0x03},
	{0xc9,0xf8},
	{0xe1,0x33},
	{0xe2,0xbb},
	{0x51,0x0c},
	{0x52,0x0a},
	{0x57,0x8c},
	{0x59,0x09},
	{0x5a,0x08},
	{0x5e,0x10},
	{0x60,0x02},
	{0x6d,0x5c},
	{0x76,0x16},
	{0x7c,0x11},
	{0x90,0x28},
	{0x91,0x16},
	{0x92,0x1c},
	{0x93,0x24},
	{0x95,0x48},
	{0x9c,0x06},
	{0xca,0x0c},
	{0xce,0x0d},
	{0xfd,0x01},
	{0xc0,0x00},
	{0xdd,0x18},
	{0xde,0x19},
	{0xdf,0x32},
	{0xe0,0x70},
	{0xfd,0x01},
	{0xc2,0x05},
	{0xd7,0x88},
	{0xd8,0x77},
	{0xd9,0x66},
	{0xfd,0x07},
	{0x00,0xf8},
	{0x01,0x2b},
	{0x05,0x40},
	{0x08,0x06},
	{0x09,0x11},
	{0x28,0x6f},
	{0x2a,0x20},
	{0x2b,0x05},
	{0x5e,0x10},
	{0x52,0x00},
	{0x53,0x80},
	{0x54,0x00},
	{0x55,0x80},
	{0x56,0x00},
	{0x57,0x80},
	{0x58,0x00},
	{0x59,0x80},
	{0x5c,0x3f},
	{0xfd,0x02},
	{0x9a,0x30},
	{0xa8,0x02},
	{0xfd,0x02},
	{0xa0,0x00},
	{0xa1,0x08},
	{0xa2,0x09},
	{0xa3,0x90},
	{0xa4,0x00},
	{0xa5,0x08},
	{0xa6,0x0c},
	{0xa7,0xc0},
	{0xfd,0x05},
	{0x04,0x40},
	{0x07,0x00},
	{0x0D,0x01},
	{0x0F,0x01},
	{0x10,0x00},
	{0x11,0x00},
	{0x12,0x0C},
	{0x13,0xCF},
	{0x14,0x00},
	{0x15,0x00},
	{0x18,0x00},
	{0x19,0x00},
	{0xfd,0x00},
	{0x24,0x01},
	{0xc0,0x16}, //add test 0x10, 0x16 0x1a 0x 1f
	{0xc1,0x08},
	{0xc2,0x30},
	{0x8e,0x0c},
	{0x8f,0xc0},
	{0x90,0x09},
	{0x91,0x90},
	{0xb7,0x02},
	{0xfd,0x00},
	{0x20,0x0f},
	{0xe7,0x03},
	{0xe7,0x00},
	{0xfd,0x01},

	{REG_NULL, 0x00},
};

static const struct ov08d10_mode supported_modes_2lane[] = {
	{
		.width = 3264,
		.height = 2448,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0900,
		.hts_def = 0x0858 * 2,
		.vts_def = 0x09c0,
		.mipi_freq_idx = 1,
		.global_reg_list = ov08d10_global_regs_2lane,
		.reg_list = ov08d10_3264x2448_regs_2lane,
		.vc[PAD0] = 0,
	},
};

static const struct ov08d10_mode *supported_modes;

static const s64 link_freq_menu_items[] = {
	OV08D10_MIPI_FREQ_720MHZ,
};

/* Write registers up to 4 at a time */
static int ov08d10_write_reg(struct i2c_client *client, u8 reg, u8 val)
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
		"ov08d10 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int ov08d10_write_array(struct i2c_client *client,
	const struct regval *regs)
{
	u32 i = 0;
	int delay_ms, ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		if (regs[i].addr == DELAY_MS) {
			delay_ms = regs[i].val;
			dev_info(&client->dev, "delay(%d) ms !\n", delay_ms);
			usleep_range(1000 * delay_ms, 1000 * delay_ms + 100);
			continue;
		}
		ret = ov08d10_write_reg(client, regs[i].addr, regs[i].val);
		if (ret)
			dev_err(&client->dev, "%s failed !\n", __func__);
	}
	return ret;
}

/* Read registers up to 4 at a time */
static int ov08d10_read_reg(struct i2c_client *client, u8 reg, u8 *val)
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
		"ov08d10 read reg:0x%x failed !\n", reg);

	return ret;
}

static int ov08d10_get_reso_dist(const struct ov08d10_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		abs(mode->height - framefmt->height);
}

static const struct ov08d10_mode *
ov08d10_find_best_fit(struct ov08d10 *ov08d10,
		     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ov08d10->cfg_num; i++) {
		dist = ov08d10_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int ov08d10_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_format *fmt)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);
	const struct ov08d10_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&ov08d10->mutex);

	mode = ov08d10_find_best_fit(ov08d10, fmt);
	fmt->format.code = OV08D10_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&ov08d10->mutex);
		return -ENOTTY;
#endif
	} else {
		ov08d10->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(ov08d10->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov08d10->vblank, vblank_def,
					 OV08D10_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(ov08d10->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(ov08d10->link_freq, mode->mipi_freq_idx);
	}

	mutex_unlock(&ov08d10->mutex);

	return 0;
}

static int ov08d10_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_format *fmt)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);
	const struct ov08d10_mode *mode = ov08d10->cur_mode;

	mutex_lock(&ov08d10->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&ov08d10->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = OV08D10_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&ov08d10->mutex);

	return 0;
}

static int ov08d10_enum_mbus_code(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = OV08D10_MEDIA_BUS_FMT;

	return 0;
}

static int ov08d10_enum_frame_sizes(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);

	if (fse->index >= ov08d10->cfg_num)
		return -EINVAL;

	if (fse->code != OV08D10_MEDIA_BUS_FMT)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int ov08d10_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *fi)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);
	const struct ov08d10_mode *mode = ov08d10->cur_mode;

	mutex_lock(&ov08d10->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&ov08d10->mutex);

	return 0;
}

#ifdef RK_OTP
static void ov08d10_get_otp(struct otp_info *otp,
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
#endif

static void ov08d10_get_module_inf(struct ov08d10 *ov08d10,
				struct rkmodule_inf *inf)
{
#ifdef RK_OTP
	struct otp_info *otp = ov08d10->otp;
#endif

	strscpy(inf->base.sensor,
		OV08D10_NAME,
		sizeof(inf->base.sensor));
	strscpy(inf->base.module,
		ov08d10->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens,
		ov08d10->len_name,
		sizeof(inf->base.lens));
	if (otp)
		ov08d10_get_otp(otp, inf);
}

static void ov08d10_set_module_inf(struct ov08d10 *ov08d10,
				struct rkmodule_awb_cfg *cfg)
{
	mutex_lock(&ov08d10->mutex);
	memcpy(&ov08d10->awb_cfg, cfg, sizeof(*cfg));
	mutex_unlock(&ov08d10->mutex);
}

static int ov08d10_get_channel_info(struct ov08d10 *ov08d10, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = ov08d10->cur_mode->vc[ch_info->index];
	ch_info->width = ov08d10->cur_mode->width;
	ch_info->height = ov08d10->cur_mode->height;
	ch_info->bus_fmt = OV08D10_MEDIA_BUS_FMT;
	return 0;
}

static long ov08d10_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		ov08d10_get_module_inf(ov08d10, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_AWB_CFG:
		ov08d10_set_module_inf(ov08d10, (struct rkmodule_awb_cfg *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream) {
			ret = ov08d10_write_reg(ov08d10->client,
					       OV08D10_REG_SET_PAGE,
					       OV08D10_SET_PAGE_ZERO);
			if (2 == ov08d10->lane_num) {
				ret |= ov08d10_write_reg(ov08d10->client,
							OV08D10_REG_CTRL_MODE,
							OV08D10_MODE_STREAMING);
			}
		} else {
			ret = ov08d10_write_reg(ov08d10->client,
					       OV08D10_REG_SET_PAGE,
					       OV08D10_SET_PAGE_ZERO);
			ret |= ov08d10_write_reg(ov08d10->client,
						OV08D10_REG_CTRL_MODE,
						OV08D10_MODE_SW_STANDBY);
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = ov08d10_get_channel_info(ov08d10, ch_info);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ov08d10_compat_ioctl32(struct v4l2_subdev *sd,
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

		ret = ov08d10_ioctl(sd, cmd, inf);
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
			ret = ov08d10_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = ov08d10_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = ov08d10_ioctl(sd, cmd, ch_info);
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

static int __ov08d10_start_stream(struct ov08d10 *ov08d10)
{
	int ret;

	ret = ov08d10_write_array(ov08d10->client, ov08d10->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&ov08d10->mutex);
	ret = v4l2_ctrl_handler_setup(&ov08d10->ctrl_handler);
	mutex_lock(&ov08d10->mutex);
	ret |= ov08d10_write_reg(ov08d10->client,
		OV08D10_REG_SET_PAGE,
		OV08D10_SET_PAGE_ZERO);
	if (2 == ov08d10->lane_num) {
		ret |= ov08d10_write_reg(ov08d10->client,
			OV08D10_REG_CTRL_MODE,
			OV08D10_MODE_STREAMING);
	}
	return ret;
}

static int __ov08d10_stop_stream(struct ov08d10 *ov08d10)
{
	int ret;

	ret = ov08d10_write_reg(ov08d10->client,
		OV08D10_REG_SET_PAGE,
		OV08D10_SET_PAGE_ZERO);
	ret |= ov08d10_write_reg(ov08d10->client,
		OV08D10_REG_CTRL_MODE,
		OV08D10_MODE_SW_STANDBY);

	return ret;
}

static int ov08d10_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);
	struct i2c_client *client = ov08d10->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				ov08d10->cur_mode->width,
				ov08d10->cur_mode->height,
		DIV_ROUND_CLOSEST(ov08d10->cur_mode->max_fps.denominator,
		ov08d10->cur_mode->max_fps.numerator));

	mutex_lock(&ov08d10->mutex);
	on = !!on;
	if (on == ov08d10->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __ov08d10_start_stream(ov08d10);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__ov08d10_stop_stream(ov08d10);
		pm_runtime_put(&client->dev);
	}

	ov08d10->streaming = on;

unlock_and_return:
	mutex_unlock(&ov08d10->mutex);

	return ret;
}

static int ov08d10_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);
	struct i2c_client *client = ov08d10->client;
	const struct ov08d10_mode *mode = ov08d10->cur_mode;
	int ret = 0;

	dev_info(&client->dev, "%s(%d) on(%d)\n", __func__, __LINE__, on);
	mutex_lock(&ov08d10->mutex);

	/* If the power state is not modified - no work to do. */
	if (ov08d10->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = ov08d10_write_array(ov08d10->client, mode->global_reg_list);
		if (ret) {
			v4l2_err(sd, "could not set init registers\n");
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ov08d10->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		ov08d10->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&ov08d10->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 ov08d10_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, OV08D10_XVCLK_FREQ / 1000 / 1000);
}

static int ov08d10_enable_regulators(struct ov08d10 *ov08d10,
				    struct regulator_bulk_data *consumers)
{
	int i, j;
	int ret = 0;
	struct device *dev = &ov08d10->client->dev;
	int num_consumers = OV08D10_NUM_SUPPLIES;

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

static int __ov08d10_power_on(struct ov08d10 *ov08d10)
{
	int ret;
	struct device *dev = &ov08d10->client->dev;

	if (!IS_ERR(ov08d10->power_gpio))
		gpiod_set_value_cansleep(ov08d10->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(ov08d10->pins_default)) {
		ret = pinctrl_select_state(ov08d10->pinctrl,
					   ov08d10->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(ov08d10->xvclk, OV08D10_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(ov08d10->xvclk) != OV08D10_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");

	if (!IS_ERR(ov08d10->reset_gpio))
		gpiod_set_value_cansleep(ov08d10->reset_gpio, 1);

	ret = ov08d10_enable_regulators(ov08d10, ov08d10->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	usleep_range(5000, 5100);
	if (!IS_ERR(ov08d10->pwdn_gpio))
		gpiod_set_value_cansleep(ov08d10->pwdn_gpio, 0);

	usleep_range(100, 200);
	ret = clk_prepare_enable(ov08d10->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	usleep_range(8000, 8100);
	if (!IS_ERR(ov08d10->reset_gpio))
		gpiod_set_value_cansleep(ov08d10->reset_gpio, 0);

	return 0;

disable_clk:
	clk_disable_unprepare(ov08d10->xvclk);

	return ret;
}

static void __ov08d10_power_off(struct ov08d10 *ov08d10)
{
	int ret;

	if (!IS_ERR(ov08d10->pwdn_gpio))
		gpiod_set_value_cansleep(ov08d10->pwdn_gpio, 1);

	if (!IS_ERR(ov08d10->reset_gpio))
		gpiod_set_value_cansleep(ov08d10->reset_gpio, 1);

	clk_disable_unprepare(ov08d10->xvclk);
	if (!IS_ERR_OR_NULL(ov08d10->pins_sleep)) {
		ret = pinctrl_select_state(ov08d10->pinctrl,
					   ov08d10->pins_sleep);
		if (ret < 0)
			dev_dbg(&ov08d10->client->dev, "could not set pins\n");
	}
	if (!IS_ERR(ov08d10->power_gpio))
		gpiod_set_value_cansleep(ov08d10->power_gpio, 0);

	regulator_bulk_disable(OV08D10_NUM_SUPPLIES, ov08d10->supplies);
}

static int ov08d10_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08d10 *ov08d10 = to_ov08d10(sd);

	return __ov08d10_power_on(ov08d10);
}

static int ov08d10_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08d10 *ov08d10 = to_ov08d10(sd);

	__ov08d10_power_off(ov08d10);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int ov08d10_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);
	struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct ov08d10_mode *def_mode = &supported_modes[0];

	mutex_lock(&ov08d10->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = OV08D10_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&ov08d10->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int ov08d10_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);

	if (fie->index >= ov08d10->cfg_num)
		return -EINVAL;

	if (fie->code != OV08D10_MEDIA_BUS_FMT)
		return -EINVAL;

	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static int ov08d10_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct ov08d10 *sensor = to_ov08d10(sd);
	struct device *dev = &sensor->client->dev;

	dev_info(dev, "%s(%d) enter!\n", __func__, __LINE__);

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes  = OV08D10_LANES;

	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH 3200
#define DST_HEIGHT 2400
static int ov08d10_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct ov08d10 *ov08d10 = to_ov08d10(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = CROP_START(ov08d10->cur_mode->width, DST_WIDTH);
		sel->r.width = DST_WIDTH;
		sel->r.top = CROP_START(ov08d10->cur_mode->height, DST_HEIGHT);
		sel->r.height = DST_HEIGHT;
		return 0;
	}
	return -EINVAL;
}

static const struct dev_pm_ops ov08d10_pm_ops = {
	SET_RUNTIME_PM_OPS(ov08d10_runtime_suspend,
			ov08d10_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops ov08d10_internal_ops = {
	.open = ov08d10_open,
};
#endif

static const struct v4l2_subdev_core_ops ov08d10_core_ops = {
	.s_power = ov08d10_s_power,
	.ioctl = ov08d10_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ov08d10_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops ov08d10_video_ops = {
	.s_stream = ov08d10_s_stream,
	.g_frame_interval = ov08d10_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops ov08d10_pad_ops = {
	.enum_mbus_code = ov08d10_enum_mbus_code,
	.enum_frame_size = ov08d10_enum_frame_sizes,
	.enum_frame_interval = ov08d10_enum_frame_interval,
	.get_fmt = ov08d10_get_fmt,
	.set_fmt = ov08d10_set_fmt,
	.get_selection = ov08d10_get_selection,
	.get_mbus_config = ov08d10_g_mbus_config,
};

static const struct v4l2_subdev_ops ov08d10_subdev_ops = {
	.core	= &ov08d10_core_ops,
	.video	= &ov08d10_video_ops,
	.pad	= &ov08d10_pad_ops,
};

static int ov08d10_set_exposure_reg(struct ov08d10 *ov08d10, u32 exposure)
{
	int ret = 0;
	u32 cal_shutter = 0;

	cal_shutter = exposure << 1;

	ret = ov08d10_write_reg(ov08d10->client,
		OV08D10_REG_SET_PAGE, 0x01);
	ret |= ov08d10_write_reg(ov08d10->client,
		OV08D10_REG_EXPOSURE_H,
		OV08D10_FETCH_HIGH_BYTE_EXP(cal_shutter));
	ret |= ov08d10_write_reg(ov08d10->client,
		OV08D10_REG_EXPOSURE_M,
		OV08D10_FETCH_M_BYTE_EXP(cal_shutter));
	ret |= ov08d10_write_reg(ov08d10->client,
		OV08D10_REG_EXPOSURE_L,
		OV08D10_FETCH_LOW_BYTE_EXP(cal_shutter));
	ret |= ov08d10_write_reg(ov08d10->client,
		OV08D10_UPDATE_REG,
		0x01);

	return ret;
}

static int ov08d10_set_gain_reg(struct ov08d10 *ov08d10, u32 a_gain)
{
	int ret = 0;

	if (a_gain < 0x10)
		a_gain = 0x10;
	if (a_gain > 0xF8)
		a_gain = 0xF8;

	ret = ov08d10_write_reg(ov08d10->client,
				   OV08D10_REG_SET_PAGE,
				   0x01);
	ret |= ov08d10_write_reg(ov08d10->client,
				   OV08D10_REG_AGAIN,
				   a_gain & 0xFF);

	ret |= ov08d10_write_reg(ov08d10->client,
				   OV08D10_UPDATE_REG,
				   0x01);

	return ret;
}

static int ov08d10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov08d10 *ov08d10 = container_of(ctrl->handler,
					struct ov08d10, ctrl_handler);
	struct i2c_client *client = ov08d10->client;
	s64 max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = ov08d10->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(ov08d10->exposure,
					 ov08d10->exposure->minimum, max,
					 ov08d10->exposure->step,
					 ov08d10->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		dev_dbg(&client->dev, "set exposure value 0x%x\n", ctrl->val);
		ret = ov08d10_set_exposure_reg(ov08d10, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set analog gain value 0x%x\n", ctrl->val);
		ret = ov08d10_set_gain_reg(ov08d10, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vb value 0x%x\n", ctrl->val);
		ret = ov08d10_write_reg(ov08d10->client,
					OV08D10_REG_SET_PAGE,
					OV08D10_SET_PAGE_ZERO);
		ret |= ov08d10_write_reg(ov08d10->client,
					OV08D10_REG_VBLANK_H,
					( ctrl->val >> 8) & 0xff);
		ret |= ov08d10_write_reg(ov08d10->client,
					OV08D10_REG_VBLANK_L,
					 ctrl->val & 0xff);
		ret |= ov08d10_write_reg(ov08d10->client,
					OV08D10_UPDATE_REG,
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

static const struct v4l2_ctrl_ops ov08d10_ctrl_ops = {
	.s_ctrl = ov08d10_set_ctrl,
};

static int ov08d10_initialize_controls(struct ov08d10 *ov08d10)
{
	const struct ov08d10_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &ov08d10->ctrl_handler;
	mode = ov08d10->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &ov08d10->mutex;

	ov08d10->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_menu_items) - 1, 0,
				link_freq_menu_items);
	v4l2_ctrl_s_ctrl(ov08d10->link_freq, mode->mipi_freq_idx);

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			0, ov08d10->pixel_rate, 1, ov08d10->pixel_rate);

	h_blank = mode->hts_def - mode->width;
	ov08d10->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (ov08d10->hblank)
		ov08d10->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	ov08d10->vblank = v4l2_ctrl_new_std(handler, &ov08d10_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				OV08D10_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 16;
	ov08d10->exposure = v4l2_ctrl_new_std(handler, &ov08d10_ctrl_ops,
				V4L2_CID_EXPOSURE, OV08D10_EXPOSURE_MIN,
				exposure_max, OV08D10_EXPOSURE_STEP,
				mode->exp_def);

	ov08d10->anal_gain = v4l2_ctrl_new_std(handler, &ov08d10_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, OV08D10_GAIN_MIN,
				OV08D10_GAIN_MAX, OV08D10_GAIN_STEP,
				OV08D10_GAIN_DEFAULT);
	if (handler->error) {
		ret = handler->error;
		dev_err(&ov08d10->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	ov08d10->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int ov08d10_check_sensor_id(struct ov08d10 *ov08d10,
				struct i2c_client *client)
{
	struct device *dev = &ov08d10->client->dev;
	u16 id = 0;
	u8 reg_H = 0;
	u8 reg_L = 0;
	int ret;

	ret = ov08d10_write_reg(client, 0xfd, 0x00);
	ret |= ov08d10_read_reg(client, OV08D10_REG_CHIP_ID_H, &reg_H);
	ret |= ov08d10_read_reg(client, OV08D10_REG_CHIP_ID_L, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected OV%04x sensor\n", id);
	return ret;
}

static int ov08d10_configure_regulators(struct ov08d10 *ov08d10)
{
	unsigned int i;

	for (i = 0; i < OV08D10_NUM_SUPPLIES; i++)
		ov08d10->supplies[i].supply = ov08d10_supply_names[i];

	return devm_regulator_bulk_get(&ov08d10->client->dev,
		OV08D10_NUM_SUPPLIES,
		ov08d10->supplies);
}

static int ov08d10_parse_of(struct ov08d10 *ov08d10)
{
	struct device *dev = &ov08d10->client->dev;
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

	ov08d10->lane_num = rval;
	if (2 == ov08d10->lane_num) {
		ov08d10->cur_mode = &supported_modes_2lane[0];
		supported_modes = supported_modes_2lane;
		ov08d10->cfg_num = ARRAY_SIZE(supported_modes_2lane);
		/*pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		fps = DIV_ROUND_CLOSEST(ov08d10->cur_mode->max_fps.denominator,
					ov08d10->cur_mode->max_fps.numerator);
		ov08d10->pixel_rate = ov08d10->cur_mode->vts_def *
				     ov08d10->cur_mode->hts_def * fps;
		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
			 ov08d10->lane_num, ov08d10->pixel_rate);
	} else {
		dev_err(dev, "unsupported lane_num(%d)\n", ov08d10->lane_num);
		return -1;
	}

	return 0;
}

static int ov08d10_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct ov08d10 *ov08d10;
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

	ov08d10 = devm_kzalloc(dev, sizeof(*ov08d10), GFP_KERNEL);
	if (!ov08d10)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
		&ov08d10->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
		&ov08d10->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
		&ov08d10->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
		&ov08d10->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}
	ov08d10->client = client;

	ov08d10->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(ov08d10->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	ov08d10->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(ov08d10->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");
	ov08d10->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ov08d10->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	ov08d10->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(ov08d10->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = ov08d10_configure_regulators(ov08d10);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ret = ov08d10_parse_of(ov08d10);
	if (ret != 0)
		return -EINVAL;

	ov08d10->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(ov08d10->pinctrl)) {
		ov08d10->pins_default =
			pinctrl_lookup_state(ov08d10->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(ov08d10->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		ov08d10->pins_sleep =
			pinctrl_lookup_state(ov08d10->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(ov08d10->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&ov08d10->mutex);

	sd = &ov08d10->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov08d10_subdev_ops);
	ret = ov08d10_initialize_controls(ov08d10);
	if (ret)
		goto err_destroy_mutex;

	ret = __ov08d10_power_on(ov08d10);
	if (ret)
		goto err_free_handler;

	ret = ov08d10_check_sensor_id(ov08d10, client);
	if (ret)
		goto err_power_off;
#ifdef RK_OTP
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
				ov08d10->otp = otp_ptr;
			} else {
				ov08d10->otp = NULL;
				devm_kfree(dev, otp_ptr);
				dev_warn(dev, "can not get otp info, skip!\n");
			}
		}
	}
continue_probe:
#endif

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &ov08d10_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	ov08d10->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &ov08d10->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(ov08d10->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 ov08d10->module_index, facing,
		 OV08D10_NAME, dev_name(sd->dev));
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
	__ov08d10_power_off(ov08d10);
err_free_handler:
	v4l2_ctrl_handler_free(&ov08d10->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&ov08d10->mutex);

	return ret;
}

static void ov08d10_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08d10 *ov08d10 = to_ov08d10(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&ov08d10->ctrl_handler);
	mutex_destroy(&ov08d10->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__ov08d10_power_off(ov08d10);
	pm_runtime_set_suspended(&client->dev);

}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ov08d10_of_match[] = {
	{ .compatible = "ovti,ov08d10" },
	{},
};
MODULE_DEVICE_TABLE(of, ov08d10_of_match);
#endif

static const struct i2c_device_id ov08d10_match_id[] = {
	{ "ovti,ov08d10", 0},
	{ },
};

static struct i2c_driver ov08d10_i2c_driver = {
	.driver = {
		.name = OV08D10_NAME,
		.pm = &ov08d10_pm_ops,
		.of_match_table = of_match_ptr(ov08d10_of_match),
	},
	.probe		= &ov08d10_probe,
	.remove		= &ov08d10_remove,
	.id_table	= ov08d10_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&ov08d10_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&ov08d10_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("OmniVision ov08d10 sensor driver");
MODULE_LICENSE("GPL");
