// SPDX-License-Identifier: GPL-2.0
/*
 * SC438HAI driver
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 first version
 */

// #define DEBUG
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <linux/rk-preisp.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"
#include "cam-tb-setup.h"
#include "cam-sleep-wakeup.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC438HAI_LANES			4
#define SC438HAI_BITS_PER_SAMPLE	10
#define SC438HAI_LINK_FREQ_378		378000000

#define PIXEL_RATE_WITH_315M_10BIT	(SC438HAI_LINK_FREQ_378 / SC438HAI_BITS_PER_SAMPLE \
					* 2 * SC438HAI_LANES)

#define SC438HAI_XVCLK_FREQ		27000000

#define CHIP_ID				0xce78
#define SC438HAI_REG_CHIP_ID		0x3107

#define SC438HAI_REG_CTRL_MODE		0x0100
#define SC438HAI_MODE_SW_STANDBY	0x0
#define SC438HAI_MODE_STREAMING		BIT(0)

#define SC438HAI_REG_EXPOSURE_H		0x3e00
#define SC438HAI_REG_EXPOSURE_M		0x3e01
#define SC438HAI_REG_EXPOSURE_L		0x3e02
#define SC438HAI_REG_SEXPOSURE_H	0x3e22
#define SC438HAI_REG_SEXPOSURE_M	0x3e04
#define SC438HAI_REG_SEXPOSURE_L	0x3e05
#define SC438HAI_REG_MAX_SEXP_H		0x3e23
#define SC438HAI_REG_MAX_SEXP_L		0x3e24

#define	SC438HAI_EXPOSURE_MIN		1
#define	SC438HAI_EXPOSURE_STEP		1
#define SC438HAI_VTS_MAX		0x7fff

#define SC438HAI_REG_DIG_GAIN		0x3e06
#define SC438HAI_REG_DIG_FINE_GAIN	0x3e07
#define SC438HAI_REG_ANA_GAIN		0x3e08
#define SC438HAI_REG_ANA_FINE_GAIN	0x3e09
#define SC438HAI_REG_SDIG_GAIN		0x3e10
#define SC438HAI_REG_SDIG_FINE_GAIN	0x3e11
#define SC438HAI_REG_SANA_GAIN		0x3e12
#define SC438HAI_REG_SANA_FINE_GAIN	0x3e13
#define SC438HAI_GAIN_MIN		0x20
#define SC438HAI_GAIN_MAX		40960
#define SC438HAI_GAIN_STEP		1
#define SC438HAI_GAIN_DEFAULT		0x20 // Note that the benchmark is 0x40

#define SC438HAI_REG_GROUP_HOLD		0x3812
#define SC438HAI_GROUP_HOLD_START	0x00
#define SC438HAI_GROUP_HOLD_END		0x30 // Not used

#define SC438HAI_REG_TEST_PATTERN	0x4501
#define SC438HAI_TEST_PATTERN_BIT_MASK	BIT(3)

#define SC438HAI_REG_VTS_H		0x320e
#define SC438HAI_REG_VTS_L		0x320f

#define SC438HAI_FLIP_MIRROR_REG	0x3221

#define SC438HAI_FETCH_EXP_H(VAL)	(((VAL) >> 12) & 0xF)
#define SC438HAI_FETCH_EXP_M(VAL)	(((VAL) >> 4) & 0xFF)
#define SC438HAI_FETCH_EXP_L(VAL)	(((VAL) & 0xF) << 4)

#define SC438HAI_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL | 0x06 : VAL & 0xf9)
#define SC438HAI_FETCH_FLIP(VAL, ENABLE)	(ENABLE ? VAL | 0x60 : VAL & 0x9f)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define SC438HAI_REG_VALUE_08BIT	1
#define SC438HAI_REG_VALUE_16BIT	2
#define SC438HAI_REG_VALUE_24BIT	3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define SC438HAI_NAME			"sc438hai"

static const char *const sc438hai_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define SC438HAI_NUM_SUPPLIES ARRAY_SIZE(sc438hai_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct sc438hai_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 bpp;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 xvclk_freq;
	u32 link_freq_idx;
	u32 vc[PAD_MAX];
};

struct sc438hai {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[SC438HAI_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	struct v4l2_fract	cur_fps;
	bool			streaming;
	bool			power_on;
	const struct sc438hai_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	bool			is_thunderboot;
	bool			is_first_streamoff;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	struct cam_sw_info	*cam_sw_inf;
};

#define to_sc438hai(sd) container_of(sd, struct sc438hai, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval sc438hai_global_regs[] = {
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 756Mbps, 4lane
 * linear
 * full resolution: 2688x1520
 */
static const struct regval sc438hai_linear_10_2688x1520_30fps_regs[] = {
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x23b0, 0x00},
	{0x23b1, 0x08},
	{0x23b2, 0x00},
	{0x23b3, 0x18},
	{0x23b4, 0x00},
	{0x23b5, 0x38},
	{0x23b6, 0x04},
	{0x23b7, 0x08},
	{0x23b8, 0x04},
	{0x23b9, 0x18},
	{0x23ba, 0x04},
	{0x23bb, 0x38},
	{0x23c0, 0x04},
	{0x23c1, 0x00},
	{0x23c2, 0x04},
	{0x23c3, 0x18},
	{0x23c4, 0x04},
	{0x23c5, 0x78},
	{0x23c6, 0x04},
	{0x23c7, 0x08},
	{0x23c8, 0x04},
	{0x23c9, 0x78},
	{0x3018, 0x7b},
	{0x301e, 0xf0},
	{0x301f, 0x0c},
	{0x302c, 0x00},
	{0x30b8, 0x44},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0xd4},
	{0x3204, 0x0a},
	{0x3205, 0x87},
	{0x3206, 0x06},
	{0x3207, 0xcb},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x05},
	{0x320d, 0xdc},
	{0x320e, 0x0c},
	{0x320f, 0x80},
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3223, 0xc0},
	{0x3250, 0x40},
	{0x327f, 0x3f},
	{0x32e0, 0x00},
	{0x3301, 0x1a},
	{0x3302, 0x20},
	{0x3304, 0xc0},
	{0x3306, 0xe0},
	{0x3309, 0xf0},
	{0x330a, 0x01},
	{0x330b, 0xe0},
	{0x330d, 0x10},
	{0x3310, 0x18},
	{0x331e, 0xa9},
	{0x331f, 0xd9},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3393, 0x24},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x14},
	{0x339a, 0x20},
	{0x339b, 0x2c},
	{0x339c, 0x50},
	{0x33ac, 0x10},
	{0x33ad, 0x2c},
	{0x33ae, 0xb0},
	{0x33af, 0xe0},
	{0x33b0, 0x0f},
	{0x33b2, 0x2c},
	{0x33b3, 0x04},
	{0x349f, 0x03},
	{0x34a8, 0x06},
	{0x34a9, 0x08},
	{0x34aa, 0x01},
	{0x34ab, 0xe0},
	{0x34ac, 0x01},
	{0x34ad, 0xe0},
	{0x34f9, 0x0a},
	{0x3631, 0x0f},
	{0x3632, 0x8d},
	{0x3633, 0x4d},
	{0x363b, 0x58},
	{0x363c, 0xb4},
	{0x363d, 0x40},
	{0x3641, 0x08},
	{0x3650, 0x08},
	{0x3651, 0x8f},
	{0x3670, 0x22},
	{0x3671, 0x24},
	{0x3672, 0x26},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x6d},
	{0x367f, 0x6d},
	{0x3680, 0x6d},
	{0x3681, 0x04},
	{0x3682, 0x08},
	{0x3683, 0x04},
	{0x3684, 0x38},
	{0x3685, 0x80},
	{0x3686, 0x81},
	{0x3687, 0x83},
	{0x3688, 0x86},
	{0x3689, 0x88},
	{0x368a, 0x8e},
	{0x368b, 0xa3},
	{0x368c, 0xbb},
	{0x368d, 0x00},
	{0x368e, 0x08},
	{0x368f, 0x00},
	{0x3690, 0x18},
	{0x3691, 0x04},
	{0x3692, 0x00},
	{0x3693, 0x04},
	{0x3694, 0x08},
	{0x3695, 0x04},
	{0x3696, 0x18},
	{0x3697, 0x04},
	{0x3698, 0x38},
	{0x3699, 0x04},
	{0x369a, 0x78},
	{0x36d0, 0x0d},
	{0x36ea, 0x15},
	{0x36eb, 0x04},
	{0x36ec, 0x43},
	{0x36ed, 0x1a},
	{0x370f, 0x13},
	{0x3721, 0x6c},
	{0x3722, 0x8b},
	{0x3724, 0xd1},
	{0x3729, 0x34},
	{0x37b0, 0x77},
	{0x37b1, 0x77},
	{0x37b2, 0x77},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x38},
	{0x37b7, 0x1d},
	{0x37b8, 0x1f},
	{0x37b9, 0x1f},
	{0x37ba, 0x04},
	{0x37bb, 0x04},
	{0x37bc, 0x04},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x38},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x38},
	{0x37fa, 0x20},
	{0x37fb, 0x03},
	{0x37fc, 0x30},
	{0x37fd, 0x16},
	{0x3900, 0x05},
	{0x3901, 0x00},
	{0x3902, 0xc0},
	{0x3903, 0x40},
	{0x3905, 0x2d},
	{0x391a, 0x72},
	{0x391b, 0x39},
	{0x391c, 0x22},
	{0x391d, 0x00},
	{0x391f, 0x41},
	{0x3926, 0xe0},
	{0x3933, 0x80},
	{0x3934, 0x03},
	{0x3935, 0x01},
	{0x3936, 0x00},
	{0x3937, 0x68},
	{0x3938, 0x6c},
	{0x3939, 0x0f},
	{0x393a, 0xf6},
	{0x393b, 0x0f},
	{0x393c, 0x90},
	{0x393d, 0x04},
	{0x393e, 0x00},
	{0x39dd, 0x00},
	{0x39de, 0x06},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0x63},
	{0x3e02, 0x80},
	{0x3e03, 0x0b},
	{0x3e16, 0x01},
	{0x3e17, 0x44},
	{0x3e18, 0x01},
	{0x3e19, 0x44},
	{0x440e, 0x02},
	{0x4509, 0x18},
	{0x450d, 0x07},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x4816, 0x21},
	{0x4837, 0x15},
	{0x5000, 0x06},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x16},
	{0x5788, 0x16},
	{0x5789, 0x15},
	{0x578a, 0x16},
	{0x578b, 0x16},
	{0x578c, 0x15},
	{0x578d, 0x41},
	{0x5790, 0x11},
	{0x5791, 0x0f},
	{0x5792, 0x0f},
	{0x5793, 0x11},
	{0x5794, 0x0f},
	{0x5795, 0x0f},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x36e9, 0x44},
	{0x37f9, 0x44},
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 756Mbps, 4lane
 * hdr2 10bit 2688x1520
 */
static const struct regval sc438hai_hdr2_10_2688x1520_30fps_regs[] = {
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x23b0, 0x00},
	{0x23b1, 0x08},
	{0x23b2, 0x00},
	{0x23b3, 0x18},
	{0x23b4, 0x00},
	{0x23b5, 0x38},
	{0x23b6, 0x04},
	{0x23b7, 0x08},
	{0x23b8, 0x04},
	{0x23b9, 0x18},
	{0x23ba, 0x04},
	{0x23bb, 0x38},
	{0x23c0, 0x04},
	{0x23c1, 0x00},
	{0x23c2, 0x04},
	{0x23c3, 0x18},
	{0x23c4, 0x04},
	{0x23c5, 0x78},
	{0x23c6, 0x04},
	{0x23c7, 0x08},
	{0x23c8, 0x04},
	{0x23c9, 0x78},
	{0x3018, 0x7b},
	{0x301e, 0xf0},
	{0x301f, 0x0d},
	{0x302c, 0x00},
	{0x30b8, 0x44},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0xd4},
	{0x3204, 0x0a},
	{0x3205, 0x87},
	{0x3206, 0x06},
	{0x3207, 0xcb},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x05},
	{0x320d, 0xdc},
	{0x320e, 0x0c},
	{0x320f, 0x80},
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3223, 0xc0},
	{0x3250, 0xff},
	{0x327f, 0x3f},
	{0x3281, 0x01},
	{0x32e0, 0x00},
	{0x3301, 0x1a},
	{0x3302, 0x20},
	{0x3304, 0xc0},
	{0x3306, 0xe0},
	{0x3309, 0xf0},
	{0x330a, 0x01},
	{0x330b, 0xe0},
	{0x330d, 0x10},
	{0x3310, 0x18},
	{0x331e, 0xa9},
	{0x331f, 0xd9},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3393, 0x24},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x14},
	{0x339a, 0x20},
	{0x339b, 0x2c},
	{0x339c, 0x50},
	{0x33ac, 0x10},
	{0x33ad, 0x2c},
	{0x33ae, 0xb0},
	{0x33af, 0xe0},
	{0x33b0, 0x0f},
	{0x33b2, 0x2c},
	{0x33b3, 0x04},
	{0x349f, 0x03},
	{0x34a8, 0x06},
	{0x34a9, 0x08},
	{0x34aa, 0x01},
	{0x34ab, 0xe0},
	{0x34ac, 0x01},
	{0x34ad, 0xe0},
	{0x34f9, 0x0a},
	{0x3631, 0x0f},
	{0x3632, 0x8d},
	{0x3633, 0x4d},
	{0x363b, 0x58},
	{0x363c, 0xb4},
	{0x363d, 0x40},
	{0x3641, 0x08},
	{0x3670, 0x22},
	{0x3671, 0x24},
	{0x3672, 0x26},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x6d},
	{0x367f, 0x6d},
	{0x3680, 0x6d},
	{0x3681, 0x04},
	{0x3682, 0x08},
	{0x3683, 0x04},
	{0x3684, 0x38},
	{0x3685, 0x80},
	{0x3686, 0x81},
	{0x3687, 0x83},
	{0x3688, 0x86},
	{0x3689, 0x88},
	{0x368a, 0x8e},
	{0x368b, 0xa3},
	{0x368c, 0xbb},
	{0x368d, 0x00},
	{0x368e, 0x08},
	{0x368f, 0x00},
	{0x3690, 0x18},
	{0x3691, 0x04},
	{0x3692, 0x00},
	{0x3693, 0x04},
	{0x3694, 0x08},
	{0x3695, 0x04},
	{0x3696, 0x18},
	{0x3697, 0x04},
	{0x3698, 0x38},
	{0x3699, 0x04},
	{0x369a, 0x78},
	{0x36d0, 0x0d},
	{0x36ea, 0x15},
	{0x36eb, 0x04},
	{0x36ec, 0x43},
	{0x36ed, 0x1a},
	{0x370f, 0x13},
	{0x3721, 0x6c},
	{0x3722, 0x8b},
	{0x3724, 0xd1},
	{0x3729, 0x34},
	{0x37b0, 0x77},
	{0x37b1, 0x77},
	{0x37b2, 0x77},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x38},
	{0x37b7, 0x1d},
	{0x37b8, 0x1f},
	{0x37b9, 0x1f},
	{0x37ba, 0x04},
	{0x37bb, 0x04},
	{0x37bc, 0x04},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x38},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x38},
	{0x37fa, 0x20},
	{0x37fb, 0x03},
	{0x37fc, 0x30},
	{0x37fd, 0x16},
	{0x3900, 0x05},
	{0x3901, 0x00},
	{0x3902, 0xc0},
	{0x3903, 0x40},
	{0x3905, 0x2d},
	{0x391a, 0x72},
	{0x391b, 0x39},
	{0x391c, 0x22},
	{0x391d, 0x00},
	{0x391f, 0x41},
	{0x3926, 0xe0},
	{0x3933, 0x80},
	{0x3934, 0x03},
	{0x3935, 0x01},
	{0x3936, 0x00},
	{0x3937, 0x68},
	{0x3938, 0x6c},
	{0x3939, 0x0f},
	{0x393a, 0xf6},
	{0x393b, 0x0f},
	{0x393c, 0x90},
	{0x393d, 0x04},
	{0x393e, 0x00},
	{0x39dd, 0x00},
	{0x39de, 0x06},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0xbb},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e04, 0x0b},
	{0x3e05, 0xb0},
	{0x3e16, 0x01},
	{0x3e17, 0x44},
	{0x3e18, 0x01},
	{0x3e19, 0x44},
	{0x3e23, 0x00},
	{0x3e24, 0xc4},
	{0x440e, 0x02},
	{0x4509, 0x18},
	{0x450d, 0x07},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x4816, 0x21},
	{0x4837, 0x15},
	{0x5000, 0x06},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x16},
	{0x5788, 0x16},
	{0x5789, 0x15},
	{0x578a, 0x16},
	{0x578b, 0x16},
	{0x578c, 0x15},
	{0x578d, 0x41},
	{0x5790, 0x11},
	{0x5791, 0x0f},
	{0x5792, 0x0f},
	{0x5793, 0x11},
	{0x5794, 0x0f},
	{0x5795, 0x0f},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x36e9, 0x44},
	{0x37f9, 0x44},
	{REG_NULL, 0x00},
};

static const struct sc438hai_mode supported_modes[] = {
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0c78,//mark
		.hts_def = 0x5dc * 2,
		.vts_def = 0x0c80,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = sc438hai_linear_10_2688x1520_30fps_regs,
		.hdr_mode = NO_HDR,
		.bpp = 10,
		.xvclk_freq = SC438HAI_XVCLK_FREQ,
		.link_freq_idx = 0,
		.vc[PAD0] = 0,
	},
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0c78,
		.hts_def = 0x05dc * 2,
		.vts_def = 0x0c80,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = sc438hai_hdr2_10_2688x1520_30fps_regs,
		.hdr_mode = HDR_X2,
		.bpp = 10,
		.xvclk_freq = SC438HAI_XVCLK_FREQ,
		.link_freq_idx = 0,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,//L->csi wr0
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,//M->csi wr2
	},
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,
};

static const s64 link_freq_menu_items[] = {
	SC438HAI_LINK_FREQ_378,
};

static const char *const sc438hai_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4",
};

/* Write registers up to 4 at a time */
static int sc438hai_write_reg(struct i2c_client *client, u16 reg,
			      u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;
	return 0;
}

static int sc438hai_write_array(struct i2c_client *client,
				const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = sc438hai_write_reg(client, regs[i].addr,
					 SC438HAI_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int sc438hai_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
			     u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

static int sc438hai_set_gain_reg(struct sc438hai *sc438hai, u32 gain)
{
	struct i2c_client *client = sc438hai->client;
	u32 coarse_again = 0, coarse_dgain = 0, fine_again = 0, fine_dgain = 0;
	int ret = 0, gain_factor;

	if (gain < 32)
		gain = 32;
	else if (gain > SC438HAI_GAIN_MAX)
		gain = SC438HAI_GAIN_MAX;

	gain_factor = gain * 2;
	if (gain_factor < 128) {
		coarse_again = 0x00;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 2;
	} else if (gain_factor <= 160) {
		coarse_again = 0x01;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 4;
	} else if (gain_factor < 320) {
		coarse_again = 0x80;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 5;
	} else if (gain_factor < 640) {
		coarse_again = 0x81;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 10;
	} else if (gain_factor < 1280) {
		coarse_again = 0x83;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 20;
	} else if (gain_factor < 2560) {
		coarse_again = 0x87;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 40;
	} else if (gain_factor < 5120) {
		coarse_again = 0x8f;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 80;
	} else if (gain_factor < 5120 * 2) {
		//dgain start
		coarse_again = 0x8f;
		coarse_dgain = 0x00;
		fine_again = 0x3f;
		fine_dgain = (gain_factor - 5120) / 160 * 4 + 128;
	} else if (gain_factor < 5120 * 4) {
		coarse_again = 0x8f;
		coarse_dgain = 0x01;
		fine_again = 0x3f;
		fine_dgain = (gain_factor - 5120 * 2) / 320 * 4 + 128;
	} else if (gain_factor < 5120 * 8) {
		coarse_again = 0x8f;
		coarse_dgain = 0x03;
		fine_again = 0x3f;
		fine_dgain = (gain_factor - 5120 * 4) / 640 * 4 + 128;
	} else if (gain_factor < 5120 * 16) {
		coarse_again = 0x8f;
		coarse_dgain = 0x07;
		fine_again = 0x3f;
		fine_dgain = (gain_factor - 5120 * 8) / 1280 * 4 + 128;
	}
	dev_dbg(&client->dev, "c_again: 0x%x, c_dgain: 0x%x, f_again: 0x%x, f_dgain: 0x%0x\n",
		coarse_again, coarse_dgain, fine_again, fine_dgain);

	ret = sc438hai_write_reg(sc438hai->client,
				 SC438HAI_REG_DIG_GAIN,
				 SC438HAI_REG_VALUE_08BIT,
				 coarse_dgain);
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_DIG_FINE_GAIN,
				  SC438HAI_REG_VALUE_08BIT,
				  fine_dgain);
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_ANA_GAIN,
				  SC438HAI_REG_VALUE_08BIT,
				  coarse_again);
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_ANA_FINE_GAIN,
				  SC438HAI_REG_VALUE_08BIT,
				  fine_again);
	return ret;
}

static int sc438hai_set_gain_reg_hdr(struct sc438hai *sc438hai, u32 gain)
{
	struct i2c_client *client = sc438hai->client;
	u32 coarse_again = 0, coarse_dgain = 0, fine_again = 0, fine_dgain = 0;
	int ret = 0, gain_factor;

	if (gain < 32)
		gain = 32;
	else if (gain > SC438HAI_GAIN_MAX)
		gain = SC438HAI_GAIN_MAX;

	gain_factor = gain * 2;
	if (gain_factor < 128) {
		coarse_again = 0x00;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 2;
	} else if (gain_factor <= 160) {
		coarse_again = 0x01;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 4;
	} else if (gain_factor < 320) {
		coarse_again = 0x80;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 5;
	} else if (gain_factor < 640) {
		coarse_again = 0x81;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 10;
	} else if (gain_factor < 1280) {
		coarse_again = 0x83;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 20;
	} else if (gain_factor < 2560) {
		coarse_again = 0x87;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor / 40;
	} else if (gain_factor < 5120) {
		coarse_again = 0x8f;
		coarse_dgain = 0x00;
		fine_again = 0x80;
		fine_dgain = gain_factor / 80;
	} else if (gain_factor < 5120 * 2) {
		//dgain start
		coarse_again = 0x8f;
		coarse_dgain = 0x00;
		fine_again = 0x3f;
		fine_dgain = (gain_factor - 5120) / 160 * 4 + 128;
	} else if (gain_factor < 5120 * 4) {
		coarse_again = 0x8f;
		coarse_dgain = 0x01;
		fine_again = 0x3f;
		fine_dgain = (gain_factor - 5120 * 2) / 320 * 4 + 128;
	} else if (gain_factor < 5120 * 8) {
		coarse_again = 0x8f;
		coarse_dgain = 0x03;
		fine_again = 0x3f;
		fine_dgain = (gain_factor - 5120 * 4) / 640 * 4 + 128;
	} else if (gain_factor < 5120 * 16) {
		coarse_again = 0x8f;
		coarse_dgain = 0x07;
		fine_again = 0x3f;
		fine_dgain = (gain_factor - 5120 * 8) / 1280 * 4 + 128;
	}
	dev_dbg(&client->dev, "short c_again: 0x%x, c_dgain: 0x%x, f_again: 0x%x, f_dgain: 0x%0x\n",
		coarse_again, coarse_dgain, fine_again, fine_dgain);

	ret = sc438hai_write_reg(sc438hai->client,
				 SC438HAI_REG_SDIG_GAIN,
				 SC438HAI_REG_VALUE_08BIT,
				 coarse_dgain);
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_SDIG_FINE_GAIN,
				  SC438HAI_REG_VALUE_08BIT,
				  fine_dgain);
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_SANA_GAIN,
				  SC438HAI_REG_VALUE_08BIT,
				  coarse_again);
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_SANA_FINE_GAIN,
				  SC438HAI_REG_VALUE_08BIT,
				  fine_again);
	return ret;
}

static int sc438hai_set_hdrae(struct sc438hai *sc438hai,
			      struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	int shr1 = 0, shr0 = 0;
	u32 fsc = 0;
	u32 max_short_exp = 0;

	if (!sc438hai->has_init_exp && !sc438hai->streaming) {
		sc438hai->init_hdrae_exp = *ae;
		sc438hai->has_init_exp = true;
		dev_dbg(&sc438hai->client->dev, "sc438hai don't stream, record exp for hdr!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&sc438hai->client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (sc438hai->cur_mode->hdr_mode == HDR_X2) {
		//2 stagger
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	ret = sc438hai_set_gain_reg(sc438hai, l_a_gain);

	ret |= sc438hai_set_gain_reg_hdr(sc438hai, s_a_gain);

	fsc = sc438hai->cur_vts;
	shr0 = fsc - l_exp_time;
	shr1 = fsc - s_exp_time;
	//first get the max s_exp_time max
	ret |= sc438hai_read_reg(sc438hai->client, SC438HAI_REG_MAX_SEXP_H,
				 SC438HAI_REG_VALUE_16BIT, &max_short_exp);
	if (shr1 < 2)
		shr1 = 2;
	else if (shr1 > max_short_exp - 9)
		shr1 = max_short_exp - 9;

	if (shr0 < 2)
		shr1 = 2;
	else if (shr1 > fsc - max_short_exp - 11)
		shr1 = fsc - max_short_exp - 11;

	dev_dbg(&sc438hai->client->dev, "shr0=%d,shr1=%d\n", shr0, shr1);
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_EXPOSURE_H,
				  SC438HAI_REG_VALUE_08BIT,
				  SC438HAI_FETCH_EXP_H(shr0));
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_EXPOSURE_M,
				  SC438HAI_REG_VALUE_08BIT,
				  SC438HAI_FETCH_EXP_M(shr0));
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_EXPOSURE_L,
				  SC438HAI_REG_VALUE_08BIT,
				  SC438HAI_FETCH_EXP_L(shr0));
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_SEXPOSURE_H,
				  SC438HAI_REG_VALUE_08BIT,
				  SC438HAI_FETCH_EXP_H(shr1));
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_SEXPOSURE_M,
				  SC438HAI_REG_VALUE_08BIT,
				  SC438HAI_FETCH_EXP_M(shr1));
	ret |= sc438hai_write_reg(sc438hai->client,
				  SC438HAI_REG_SEXPOSURE_L,
				  SC438HAI_REG_VALUE_08BIT,
				  SC438HAI_FETCH_EXP_L(shr1));
	return ret;
}

static int sc438hai_get_reso_dist(const struct sc438hai_mode *mode,
				  struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc438hai_mode *
sc438hai_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = sc438hai_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int sc438hai_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct sc438hai *sc438hai = to_sc438hai(sd);
	const struct sc438hai_mode *mode;
	s64 h_blank, vblank_def;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;

	mutex_lock(&sc438hai->mutex);

	mode = sc438hai_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&sc438hai->mutex);
		return -ENOTTY;
#endif
	} else {
		sc438hai->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc438hai->hblank, h_blank,
				 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc438hai->vblank, vblank_def,
				 SC438HAI_VTS_MAX - mode->height,
				 1, vblank_def);
		dst_link_freq = mode->link_freq_idx;
		dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
				 SC438HAI_BITS_PER_SAMPLE * 2 * SC438HAI_LANES;
		__v4l2_ctrl_s_ctrl_int64(sc438hai->pixel_rate,
				 dst_pixel_rate);
		__v4l2_ctrl_s_ctrl(sc438hai->link_freq,
			   dst_link_freq);
		sc438hai->cur_fps = mode->max_fps;
	}

	mutex_unlock(&sc438hai->mutex);

	return 0;
}

static int sc438hai_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct sc438hai *sc438hai = to_sc438hai(sd);
	const struct sc438hai_mode *mode = sc438hai->cur_mode;

	mutex_lock(&sc438hai->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&sc438hai->mutex);
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
	mutex_unlock(&sc438hai->mutex);

	return 0;
}

static int sc438hai_enum_mbus_code(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int sc438hai_enum_frame_sizes(struct v4l2_subdev *sd,
				     struct v4l2_subdev_state *sd_state,
				     struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int sc438hai_enable_test_pattern(struct sc438hai *sc438hai, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc438hai_read_reg(sc438hai->client, SC438HAI_REG_TEST_PATTERN,
				SC438HAI_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC438HAI_TEST_PATTERN_BIT_MASK;
	else
		val &= ~SC438HAI_TEST_PATTERN_BIT_MASK;

	ret |= sc438hai_write_reg(sc438hai->client, SC438HAI_REG_TEST_PATTERN,
				  SC438HAI_REG_VALUE_08BIT, val);
	return ret;
}

static int sc438hai_g_frame_interval(struct v4l2_subdev *sd,
				     struct v4l2_subdev_frame_interval *fi)
{
	struct sc438hai *sc438hai = to_sc438hai(sd);
	const struct sc438hai_mode *mode = sc438hai->cur_mode;

	if (sc438hai->streaming)
		fi->interval = sc438hai->cur_fps;
	else
		fi->interval = mode->max_fps;
	return 0;
}

static const struct sc438hai_mode *sc438hai_find_mode(struct sc438hai *sc438hai, int fps)
{
	const struct sc438hai_mode *mode = NULL;
	const struct sc438hai_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		mode = &supported_modes[i];
		if (mode->width == sc438hai->cur_mode->width &&
		    mode->height == sc438hai->cur_mode->height &&
		    mode->hdr_mode == sc438hai->cur_mode->hdr_mode &&
		    mode->bus_fmt == sc438hai->cur_mode->bus_fmt) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator,
						    mode->max_fps.numerator);
			if (cur_fps == fps) {
				match = mode;
				break;
			}
		}
	}
	return match;
}

static int sc438hai_s_frame_interval(struct v4l2_subdev *sd,
				     struct v4l2_subdev_frame_interval *fi)
{
	struct sc438hai *sc438hai = to_sc438hai(sd);
	const struct sc438hai_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	int fps;

	if (sc438hai->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = sc438hai_find_mode(sc438hai, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	sc438hai->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(sc438hai->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(sc438hai->vblank, vblank_def,
				 SC438HAI_VTS_MAX - mode->height,
				 1, vblank_def);
	__v4l2_ctrl_s_ctrl(sc438hai->link_freq, mode->link_freq_idx);
	pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
		     mode->bpp * 2 * SC438HAI_LANES;
	__v4l2_ctrl_s_ctrl_int64(sc438hai->pixel_rate, pixel_rate);
	sc438hai->cur_fps = mode->max_fps;

	return 0;
}

static int sc438hai_g_mbus_config(struct v4l2_subdev *sd,
				  unsigned int pad_id,
				  struct v4l2_mbus_config *config)
{
	config->bus.mipi_csi2.num_data_lanes = SC438HAI_LANES;
	config->type = V4L2_MBUS_CSI2_DPHY;

	return 0;
}

static void sc438hai_get_module_inf(struct sc438hai *sc438hai,
				    struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC438HAI_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc438hai->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc438hai->len_name, sizeof(inf->base.lens));
}

static long sc438hai_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc438hai *sc438hai = to_sc438hai(sd);
	struct rkmodule_hdr_cfg *hdr;
	u32 i, h, w;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc438hai_get_module_inf(sc438hai, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = sc438hai->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = sc438hai->cur_mode->width;
		h = sc438hai->cur_mode->height;
		for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode &&
			    supported_modes[i].bus_fmt == sc438hai->cur_mode->bus_fmt) {
				sc438hai->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == ARRAY_SIZE(supported_modes)) {
			dev_err(&sc438hai->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = sc438hai->cur_mode->hts_def - sc438hai->cur_mode->width;
			h = sc438hai->cur_mode->vts_def - sc438hai->cur_mode->height;
			__v4l2_ctrl_modify_range(sc438hai->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(sc438hai->vblank, h,
						 SC438HAI_VTS_MAX - sc438hai->cur_mode->height,
						 1, h);
			sc438hai->cur_fps = sc438hai->cur_mode->max_fps;
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		if (sc438hai->cur_mode->hdr_mode == HDR_X2)
			ret = sc438hai_set_hdrae(sc438hai, arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = sc438hai_write_reg(sc438hai->client, SC438HAI_REG_CTRL_MODE,
						 SC438HAI_REG_VALUE_08BIT, SC438HAI_MODE_STREAMING);
		else
			ret = sc438hai_write_reg(sc438hai->client, SC438HAI_REG_CTRL_MODE,
						 SC438HAI_REG_VALUE_08BIT, SC438HAI_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long sc438hai_compat_ioctl32(struct v4l2_subdev *sd,
				    unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	long ret;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc438hai_ioctl(sd, cmd, inf);
		if (!ret) {
			if (copy_to_user(up, inf, sizeof(*inf)))
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc438hai_ioctl(sd, cmd, hdr);
		if (!ret) {
			if (copy_to_user(up, hdr, sizeof(*hdr)))
				ret = -EFAULT;
		}
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdr, up, sizeof(*hdr));
		if (!ret)
			ret = sc438hai_ioctl(sd, cmd, hdr);
		else
			ret = -EFAULT;
		kfree(hdr);
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		hdrae = kzalloc(sizeof(*hdrae), GFP_KERNEL);
		if (!hdrae) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdrae, up, sizeof(*hdrae));
		if (!ret)
			ret = sc438hai_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = sc438hai_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __sc438hai_start_stream(struct sc438hai *sc438hai)
{
	int ret;

	if (!sc438hai->is_thunderboot) {
		ret = sc438hai_write_array(sc438hai->client, sc438hai->cur_mode->reg_list);
		if (ret) {
			dev_err(&sc438hai->client->dev,
				"write regs fail in start stream\n");
			return ret;
		}
		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&sc438hai->ctrl_handler);
		if (ret) {
			dev_err(&sc438hai->client->dev,
				"setup ctrl handler fail in start stream\n");
			return ret;
		}
		if (sc438hai->has_init_exp && sc438hai->cur_mode->hdr_mode != NO_HDR) {
			ret = sc438hai_ioctl(&sc438hai->subdev, PREISP_CMD_SET_HDRAE_EXP,
					     &sc438hai->init_hdrae_exp);
			if (ret) {
				dev_err(&sc438hai->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	ret = sc438hai_write_reg(sc438hai->client, SC438HAI_REG_CTRL_MODE,
				 SC438HAI_REG_VALUE_08BIT, SC438HAI_MODE_STREAMING);
	return ret;
}

static int __sc438hai_stop_stream(struct sc438hai *sc438hai)
{
	sc438hai->has_init_exp = false;
	if (sc438hai->is_thunderboot) {
		sc438hai->is_first_streamoff = true;
		pm_runtime_put(&sc438hai->client->dev);
	}
	return sc438hai_write_reg(sc438hai->client, SC438HAI_REG_CTRL_MODE,
				  SC438HAI_REG_VALUE_08BIT, SC438HAI_MODE_SW_STANDBY);
}

static int __sc438hai_power_on(struct sc438hai *sc438hai);
static int sc438hai_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc438hai *sc438hai = to_sc438hai(sd);
	struct i2c_client *client = sc438hai->client;
	int ret = 0;

	mutex_lock(&sc438hai->mutex);
	on = !!on;
	if (on == sc438hai->streaming)
		goto unlock_and_return;
	if (on) {
		if (sc438hai->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			sc438hai->is_thunderboot = false;
			__sc438hai_power_on(sc438hai);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __sc438hai_start_stream(sc438hai);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc438hai_stop_stream(sc438hai);
		pm_runtime_put(&client->dev);
	}

	sc438hai->streaming = on;
unlock_and_return:
	mutex_unlock(&sc438hai->mutex);
	return ret;
}

static int sc438hai_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc438hai *sc438hai = to_sc438hai(sd);
	struct i2c_client *client = sc438hai->client;
	int ret = 0;

	mutex_lock(&sc438hai->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc438hai->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!sc438hai->is_thunderboot) {
			ret = sc438hai_write_array(sc438hai->client, sc438hai_global_regs);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		sc438hai->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc438hai->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc438hai->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 sc438hai_cal_delay(u32 cycles, struct sc438hai *sc438hai)
{
	return DIV_ROUND_UP(cycles, sc438hai->cur_mode->xvclk_freq / 1000 / 1000);
}

static int __sc438hai_power_on(struct sc438hai *sc438hai)
{
	int ret;
	u32 delay_us;
	struct device *dev = &sc438hai->client->dev;

	if (!IS_ERR_OR_NULL(sc438hai->pins_default)) {
		ret = pinctrl_select_state(sc438hai->pinctrl,
					   sc438hai->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc438hai->xvclk, sc438hai->cur_mode->xvclk_freq);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (%dHz)\n", sc438hai->cur_mode->xvclk_freq);
	if (clk_get_rate(sc438hai->xvclk) != sc438hai->cur_mode->xvclk_freq)
		dev_warn(dev, "xvclk mismatched, modes are based on %dHz\n",
			 sc438hai->cur_mode->xvclk_freq);
	ret = clk_prepare_enable(sc438hai->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	cam_sw_regulator_bulk_init(sc438hai->cam_sw_inf, SC438HAI_NUM_SUPPLIES, sc438hai->supplies);

	if (sc438hai->is_thunderboot)
		return 0;

	if (!IS_ERR(sc438hai->reset_gpio))
		gpiod_set_value_cansleep(sc438hai->reset_gpio, 0);

	ret = regulator_bulk_enable(SC438HAI_NUM_SUPPLIES, sc438hai->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc438hai->reset_gpio))
		gpiod_set_value_cansleep(sc438hai->reset_gpio, 1);

	usleep_range(500, 1000);

	msleep(20);
	if (!IS_ERR(sc438hai->pwdn_gpio))
		gpiod_set_value_cansleep(sc438hai->pwdn_gpio, 1);

	if (!IS_ERR(sc438hai->reset_gpio))
		usleep_range(6000, 8000);
	else
		usleep_range(12000, 16000);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = sc438hai_cal_delay(8192, sc438hai);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(sc438hai->xvclk);

	return ret;
}

static void __sc438hai_power_off(struct sc438hai *sc438hai)
{
	int ret;
	struct device *dev = &sc438hai->client->dev;

	clk_disable_unprepare(sc438hai->xvclk);
	if (sc438hai->is_thunderboot) {
		if (sc438hai->is_first_streamoff) {
			sc438hai->is_thunderboot = false;
			sc438hai->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(sc438hai->pwdn_gpio))
		gpiod_set_value_cansleep(sc438hai->pwdn_gpio, 0);
	if (!IS_ERR(sc438hai->reset_gpio))
		gpiod_set_value_cansleep(sc438hai->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(sc438hai->pins_sleep)) {
		ret = pinctrl_select_state(sc438hai->pinctrl,
					   sc438hai->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}

	regulator_bulk_disable(SC438HAI_NUM_SUPPLIES, sc438hai->supplies);
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int __maybe_unused sc438hai_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc438hai *sc438hai = to_sc438hai(sd);

	cam_sw_prepare_wakeup(sc438hai->cam_sw_inf, dev);

	usleep_range(4000, 5000);
	cam_sw_write_array(sc438hai->cam_sw_inf);

	if (__v4l2_ctrl_handler_setup(&sc438hai->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (sc438hai->has_init_exp && sc438hai->cur_mode != NO_HDR) {	// hdr mode
		ret = sc438hai_ioctl(&sc438hai->subdev, PREISP_CMD_SET_HDRAE_EXP,
				     &sc438hai->cam_sw_inf->hdr_ae);
		if (ret) {
			dev_err(&sc438hai->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}
	return 0;
}

static int __maybe_unused sc438hai_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc438hai *sc438hai = to_sc438hai(sd);

	cam_sw_write_array_cb_init(sc438hai->cam_sw_inf, client,
				   (void *)sc438hai->cur_mode->reg_list,
				   (sensor_write_array)sc438hai_write_array);
	cam_sw_prepare_sleep(sc438hai->cam_sw_inf);

	return 0;
}
#else
#define sc438hai_resume NULL
#define sc438hai_suspend NULL
#endif

static int __maybe_unused sc438hai_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc438hai *sc438hai = to_sc438hai(sd);

	return __sc438hai_power_on(sc438hai);
}

static int __maybe_unused sc438hai_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc438hai *sc438hai = to_sc438hai(sd);

	__sc438hai_power_off(sc438hai);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc438hai_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc438hai *sc438hai = to_sc438hai(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct sc438hai_mode *def_mode = &supported_modes[0];

	mutex_lock(&sc438hai->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc438hai->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc438hai_enum_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_state *sd_state,
					struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops sc438hai_pm_ops = {
	SET_RUNTIME_PM_OPS(sc438hai_runtime_suspend,
	sc438hai_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sc438hai_suspend, sc438hai_resume)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc438hai_internal_ops = {
	.open = sc438hai_open,
};
#endif

static const struct v4l2_subdev_core_ops sc438hai_core_ops = {
	.s_power = sc438hai_s_power,
	.ioctl = sc438hai_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc438hai_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc438hai_video_ops = {
	.s_stream = sc438hai_s_stream,
	.g_frame_interval = sc438hai_g_frame_interval,
	.s_frame_interval = sc438hai_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops sc438hai_pad_ops = {
	.enum_mbus_code = sc438hai_enum_mbus_code,
	.enum_frame_size = sc438hai_enum_frame_sizes,
	.enum_frame_interval = sc438hai_enum_frame_interval,
	.get_fmt = sc438hai_get_fmt,
	.set_fmt = sc438hai_set_fmt,
	.get_mbus_config = sc438hai_g_mbus_config,
};

static const struct v4l2_subdev_ops sc438hai_subdev_ops = {
	.core	= &sc438hai_core_ops,
	.video	= &sc438hai_video_ops,
	.pad	= &sc438hai_pad_ops,
};

static void sc438hai_modify_fps_info(struct sc438hai *sc438hai)
{
	const struct sc438hai_mode *mode = sc438hai->cur_mode;

	sc438hai->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
					sc438hai->cur_vts;
}

static int sc438hai_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc438hai *sc438hai = container_of(ctrl->handler,
				    struct sc438hai, ctrl_handler);
	struct i2c_client *client = sc438hai->client;
	s64 max;
	int ret = 0;
	u32 val = 0;
	u32 shr0 = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc438hai->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(sc438hai->exposure,
					 sc438hai->exposure->minimum, max,
					 sc438hai->exposure->step,
					 sc438hai->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		if (sc438hai->cur_mode->hdr_mode == NO_HDR) {
			shr0 = ctrl->val;
			/* 4 least significant bits of expsoure are fractional part */
			ret = sc438hai_write_reg(sc438hai->client,
						 SC438HAI_REG_EXPOSURE_H,
						 SC438HAI_REG_VALUE_08BIT,
						 SC438HAI_FETCH_EXP_H(shr0));
			ret |= sc438hai_write_reg(sc438hai->client,
						  SC438HAI_REG_EXPOSURE_M,
						  SC438HAI_REG_VALUE_08BIT,
						  SC438HAI_FETCH_EXP_M(shr0));
			ret |= sc438hai_write_reg(sc438hai->client,
						  SC438HAI_REG_EXPOSURE_L,
						  SC438HAI_REG_VALUE_08BIT,
						  SC438HAI_FETCH_EXP_L(shr0));
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set gain 0x%x\n", ctrl->val);
		if (sc438hai->cur_mode->hdr_mode == NO_HDR)
			ret = sc438hai_set_gain_reg(sc438hai, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		ret = sc438hai_write_reg(sc438hai->client,
					 SC438HAI_REG_VTS_H,
					 SC438HAI_REG_VALUE_08BIT,
					 (ctrl->val + sc438hai->cur_mode->height)
					 >> 8);
		ret |= sc438hai_write_reg(sc438hai->client,
					  SC438HAI_REG_VTS_L,
					  SC438HAI_REG_VALUE_08BIT,
					  (ctrl->val + sc438hai->cur_mode->height)
					  & 0xff);
		sc438hai->cur_vts = ctrl->val + sc438hai->cur_mode->height;
		if (sc438hai->cur_vts != sc438hai->cur_mode->vts_def)
			sc438hai_modify_fps_info(sc438hai);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc438hai_enable_test_pattern(sc438hai, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc438hai_read_reg(sc438hai->client, SC438HAI_FLIP_MIRROR_REG,
					SC438HAI_REG_VALUE_08BIT, &val);
		ret |= sc438hai_write_reg(sc438hai->client, SC438HAI_FLIP_MIRROR_REG,
					  SC438HAI_REG_VALUE_08BIT,
					  SC438HAI_FETCH_MIRROR(val, ctrl->val));
		break;
	case V4L2_CID_VFLIP:
		ret = sc438hai_read_reg(sc438hai->client, SC438HAI_FLIP_MIRROR_REG,
					SC438HAI_REG_VALUE_08BIT, &val);
		ret |= sc438hai_write_reg(sc438hai->client, SC438HAI_FLIP_MIRROR_REG,
					  SC438HAI_REG_VALUE_08BIT,
					  SC438HAI_FETCH_FLIP(val, ctrl->val));
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sc438hai_ctrl_ops = {
	.s_ctrl = sc438hai_set_ctrl,
};

static int sc438hai_initialize_controls(struct sc438hai *sc438hai)
{
	const struct sc438hai_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;

	handler = &sc438hai->ctrl_handler;
	mode = sc438hai->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc438hai->mutex;

	sc438hai->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			      V4L2_CID_LINK_FREQ,
			      ARRAY_SIZE(link_freq_menu_items) - 1, 0, link_freq_menu_items);
	if (sc438hai->link_freq)
		sc438hai->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	dst_link_freq = mode->link_freq_idx;
	dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
			 SC438HAI_BITS_PER_SAMPLE * 2 * SC438HAI_LANES;
	sc438hai->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			       0, PIXEL_RATE_WITH_315M_10BIT, 1, dst_pixel_rate);

	__v4l2_ctrl_s_ctrl(sc438hai->link_freq, dst_link_freq);

	h_blank = mode->hts_def - mode->width;
	sc438hai->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					     h_blank, h_blank, 1, h_blank);
	if (sc438hai->hblank)
		sc438hai->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	sc438hai->vblank = v4l2_ctrl_new_std(handler, &sc438hai_ctrl_ops,
					     V4L2_CID_VBLANK, vblank_def,
					     SC438HAI_VTS_MAX - mode->height,
					     1, vblank_def);
	exposure_max = mode->vts_def - 8;
	sc438hai->exposure = v4l2_ctrl_new_std(handler, &sc438hai_ctrl_ops,
					       V4L2_CID_EXPOSURE, SC438HAI_EXPOSURE_MIN,
					       exposure_max, SC438HAI_EXPOSURE_STEP,
					       mode->exp_def);
	sc438hai->anal_gain = v4l2_ctrl_new_std(handler, &sc438hai_ctrl_ops,
						V4L2_CID_ANALOGUE_GAIN, SC438HAI_GAIN_MIN,
						SC438HAI_GAIN_MAX, SC438HAI_GAIN_STEP,
						SC438HAI_GAIN_DEFAULT);
	sc438hai->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				 &sc438hai_ctrl_ops,
				 V4L2_CID_TEST_PATTERN,
				 ARRAY_SIZE(sc438hai_test_pattern_menu) - 1,
				 0, 0, sc438hai_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &sc438hai_ctrl_ops,
			  V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &sc438hai_ctrl_ops,
			  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&sc438hai->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc438hai->subdev.ctrl_handler = handler;
	sc438hai->has_init_exp = false;
	sc438hai->cur_fps = mode->max_fps;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc438hai_check_sensor_id(struct sc438hai *sc438hai,
				    struct i2c_client *client)
{
	struct device *dev = &sc438hai->client->dev;
	u32 id = 0;
	int ret;

	if (sc438hai->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = sc438hai_read_reg(client, SC438HAI_REG_CHIP_ID,
				SC438HAI_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected SC438HAI sensor(CHIP_ID:0x%06x)\n", CHIP_ID);

	return 0;
}

static int sc438hai_configure_regulators(struct sc438hai *sc438hai)
{
	unsigned int i;

	for (i = 0; i < SC438HAI_NUM_SUPPLIES; i++)
		sc438hai->supplies[i].supply = sc438hai_supply_names[i];

	return devm_regulator_bulk_get(&sc438hai->client->dev,
				       SC438HAI_NUM_SUPPLIES,
				       sc438hai->supplies);
}

static int sc438hai_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sc438hai *sc438hai;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	int i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	sc438hai = devm_kzalloc(dev, sizeof(*sc438hai), GFP_KERNEL);
	if (!sc438hai)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc438hai->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc438hai->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc438hai->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc438hai->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	sc438hai->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	sc438hai->client = client;
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			sc438hai->cur_mode = &supported_modes[i];
			break;
		}
	}

	if (i == ARRAY_SIZE(supported_modes))
		sc438hai->cur_mode = &supported_modes[0];

	sc438hai->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc438hai->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	if (!sc438hai->is_thunderboot)
		sc438hai->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	else
		sc438hai->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(sc438hai->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	if (!sc438hai->is_thunderboot)
		sc438hai->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	else
		sc438hai->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_ASIS);
	if (IS_ERR(sc438hai->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	sc438hai->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc438hai->pinctrl)) {
		sc438hai->pins_default =
			pinctrl_lookup_state(sc438hai->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc438hai->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		sc438hai->pins_sleep =
			pinctrl_lookup_state(sc438hai->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc438hai->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = sc438hai_configure_regulators(sc438hai);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc438hai->mutex);

	sd = &sc438hai->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc438hai_subdev_ops);
	ret = sc438hai_initialize_controls(sc438hai);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc438hai_power_on(sc438hai);
	if (ret)
		goto err_free_handler;

	ret = sc438hai_check_sensor_id(sc438hai, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc438hai_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc438hai->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc438hai->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	if (!sc438hai->cam_sw_inf) {
		sc438hai->cam_sw_inf = cam_sw_init();
		cam_sw_clk_init(sc438hai->cam_sw_inf, sc438hai->xvclk,
				sc438hai->cur_mode->xvclk_freq);
		cam_sw_reset_pin_init(sc438hai->cam_sw_inf, sc438hai->reset_gpio, 0);
		cam_sw_pwdn_pin_init(sc438hai->cam_sw_inf, sc438hai->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc438hai->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc438hai->module_index, facing,
		 SC438HAI_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (sc438hai->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__sc438hai_power_off(sc438hai);
err_free_handler:
	v4l2_ctrl_handler_free(&sc438hai->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc438hai->mutex);

	return ret;
}

static void sc438hai_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc438hai *sc438hai = to_sc438hai(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc438hai->ctrl_handler);
	mutex_destroy(&sc438hai->mutex);

	cam_sw_deinit(sc438hai->cam_sw_inf);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc438hai_power_off(sc438hai);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc438hai_of_match[] = {
	{ .compatible = "smartsens,sc438hai" },
	{},
};
MODULE_DEVICE_TABLE(of, sc438hai_of_match);
#endif

static const struct i2c_device_id sc438hai_match_id[] = {
	{ "smartsens,sc438hai", 0 },
	{ },
};

static struct i2c_driver sc438hai_i2c_driver = {
	.driver = {
		.name = SC438HAI_NAME,
		.pm = &sc438hai_pm_ops,
		.of_match_table = of_match_ptr(sc438hai_of_match),
	},
	.probe		= sc438hai_probe,
	.remove		= sc438hai_remove,
	.id_table	= sc438hai_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc438hai_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc438hai_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens sc438hai sensor driver");
MODULE_LICENSE("GPL");
