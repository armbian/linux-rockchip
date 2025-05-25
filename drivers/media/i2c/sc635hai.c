// SPDX-License-Identifier: GPL-2.0
/*
 * sc635hai driver
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 first version
 *  support thunderboot
 *  support sleep wake-up mode
 * V0.0X01.0X02 support 2 lane setting
 */

// #define DEBUG

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_graph.h>
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
#include <media/v4l2-fwnode.h>
#include <linux/pinctrl/consumer.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"
#include "cam-tb-setup.h"
#include "cam-sleep-wakeup.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x02)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC635HAI_BITS_PER_SAMPLE	10
#define SC635HAI_LINK_FREQ_540		540000000	/* 1080Mbps pre lane*/

#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

/* 2 lane */
/* 1080Mbps pre lane */
#define PIXEL_RATE_WITH_540M_10BIT_2L	(SC635HAI_LINK_FREQ_540 * 2 / \
					SC635HAI_BITS_PER_SAMPLE * 2)

/* 4 lane */
/* 1080Mbps pre lane */
#define PIXEL_RATE_WITH_540M_10BIT_4L	(SC635HAI_LINK_FREQ_540 * 2 / \
					SC635HAI_BITS_PER_SAMPLE * 4)

#define SC635HAI_XVCLK_FREQ		27000000

#define CHIP_ID				0xce7c
#define SC635HAI_REG_CHIP_ID		0x3107

#define SC635HAI_REG_MIPI_CTRL		0x3019
#define SC635HAI_MIPI_CTRL_ON		0x00
#define SC635HAI_MIPI_CTRL_OFF		0xff

#define SC635HAI_REG_CTRL_MODE		0x0100
#define SC635HAI_MODE_SW_STANDBY	0x0
#define SC635HAI_MODE_STREAMING		BIT(0)

#define SC635HAI_REG_EXPOSURE_H		0x3e00
#define SC635HAI_REG_EXPOSURE_M		0x3e01
#define SC635HAI_REG_EXPOSURE_L		0x3e02

#define SC635HAI_REG_SEXPOSURE_H	0x3e22
#define SC635HAI_REG_SEXPOSURE_M	0x3e04
#define SC635HAI_REG_SEXPOSURE_L	0x3e05

#define	SC635HAI_EXPOSURE_MIN		2
#define	SC635HAI_EXPOSURE_STEP		1
#define SC635HAI_VTS_MAX		0x1ffff0

#define SC635HAI_REG_DIG_GAIN		0x3e06
#define SC635HAI_REG_DIG_FINE_GAIN	0x3e07
#define SC635HAI_REG_ANA_GAIN		0x3e08
#define SC635HAI_REG_ANA_FINE_GAIN	0x3e09
#define SC635HAI_REG_SDIG_GAIN		0x3e10
#define SC635HAI_REG_SDIG_FINE_GAIN	0x3e11
#define SC635HAI_REG_SANA_GAIN		0x3e12
#define SC635HAI_REG_SANA_FINE_GAIN	0x3e13

#define SC635HAI_GAIN_MIN		0x0020
#define SC635HAI_GAIN_MAX		42230 // 83.79 * 15.75 * 32 = 42230
#define SC635HAI_GAIN_STEP		1
#define SC635HAI_GAIN_DEFAULT		0x0020
#define SC635HAI_LGAIN			0
#define SC635HAI_SGAIN			1

#define SC635HAI_REG_GROUP_HOLD		0x3812
#define SC635HAI_GROUP_HOLD_START	0x00 // start hold
#define SC635HAI_GROUP_HOLD_END		0x30 // release hold
#define SC635HAI_REG_HOLD_DELAY		0x3802 //effective after group hold

/* led strobe mode 1*/
#define SC635HAI_REG_LED_STROBE_EN_M1		0x3362 // 0x00: auto mode; 0x01: manuale mode;
#define SC635HAI_REG_LED_STROBE_OUTPUT_PIN0_M1	0x300a // [2:1, 6], use fsync as output single pin
#define SC635HAI_REG_LED_STROBE_OUTPUT_PIN1_M1	0x3033 // [1]
#define SC635HAI_REG_LED_STROBE_OUTPUT_PIN2_M1	0x3035 // 0x00
#define SC635HAI_REG_LED_STROBE_PUSLE_START_H	0x3382 // start at {16’h320e,16’h320f} – 1 –{16’h3382,16’h3383}
#define SC635HAI_REG_LED_STROBE_PUSLE_START_L	0x3383
#define SC635HAI_REG_LED_STROBE_PUSLE_END_H	0x3386 // end at {16’h320e,16’h320f} – 1 –{16’h3386,16’h3387}
#define SC635HAI_REG_LED_STROBE_PUSLE_END_L	0x3387
/* led strobe mode 2 */
#define SC635HAI_REG_LED_STROBE_EN_M2		0x4d0b	// 0x00: disable; 0x01: enable
#define SC635HAI_REG_LED_STROBE_OUTPUT_PIN0_M2	0x300a // [2:1], use fsync as output single pin
#define SC635HAI_REG_LED_STROBE_OUTPUT_PIN1_M2	0x3033 // [1]
#define SC635HAI_REG_LED_STROBE_OUTPUT_PIN2_M2	0x3035 // 0x00
#define SC635HAI_REG_LED_STROBE_PUSLE_WIDTH_H	0x4d0c // use {16’h320c,16’h320d} as unit
#define SC635HAI_REG_LED_STROBE_PUSLE_WIDTH_L	0x4d0d

#define SC635HAI_REG_TEST_PATTERN	0x4501
#define SC635HAI_TEST_PATTERN_BIT_MASK	BIT(3)	// 0 -normal image; 1 - increasing gradient pattern

/* max frame length 0x1ffff */
#define SC635HAI_REG_VTS_H		0x326d	// [0]
#define SC635HAI_REG_VTS_M		0x320e
#define SC635HAI_REG_VTS_L		0x320f

#define SC635HAI_FLIP_MIRROR_REG	0x3221

#define SC635HAI_FETCH_EXP_H(VAL)	(((VAL) >> 12) & 0xF)
#define SC635HAI_FETCH_EXP_M(VAL)	(((VAL) >> 4) & 0xFF)
#define SC635HAI_FETCH_EXP_L(VAL)	(((VAL) & 0xF) << 4)

#define SC635HAI_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL | 0x06 : VAL & 0xf9)
#define SC635HAI_FETCH_FLIP(VAL, ENABLE)	(ENABLE ? VAL | 0x60 : VAL & 0x9f)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define SC635HAI_REG_VALUE_08BIT	1
#define SC635HAI_REG_VALUE_16BIT	2
#define SC635HAI_REG_VALUE_24BIT	3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define SC635HAI_NAME			"sc635hai"

static const char *const sc635hai_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define SC635HAI_NUM_SUPPLIES ARRAY_SIZE(sc635hai_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct sc635hai_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *global_reg_list;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 mclk;
	u32 link_freq_idx;
	u32 vc[PAD_MAX];
	u8 bpp;
	u32 lanes;
};

struct sc635hai {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[SC635HAI_NUM_SUPPLIES];

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
	const struct sc635hai_mode *supported_modes;
	const struct sc635hai_mode *cur_mode;
	u32			cfg_num;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			standby_hw;
	u32			cur_vts;
	bool			has_init_exp;
	bool			is_thunderboot;
	bool			is_first_streamoff;
	bool			is_standby;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	struct cam_sw_info	*cam_sw_inf;
	struct v4l2_fwnode_endpoint bus_cfg;
	struct rk_light_param	light_param;
};

#define to_sc635hai(sd) container_of(sd, struct sc635hai, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval sc635hai_global_4lane_regs[] = {
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 60fps
 * mipi_datarate per lane 1080Mbps, 4lane
 * linear: 3200x1800
 * Cleaned_0x01_SC635HAI_raw_MIPI_27Minput_4Lane_10bit_1080Mbps_3200x1800_60fps
 */
static const struct regval sc635hai_linear_10_3200x1800_60fps_4lane_regs[] = {
	{0x3105, 0x32},
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x302c, 0x0c},
	{0x302c, 0x00},
	{0x3105, 0x12},
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
	{0x23bc, 0x04},
	{0x23bd, 0x08},
	{0x23be, 0x04},
	{0x23bf, 0x78},
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
	{0x301f, 0x01},
	{0x302c, 0x00},
	{0x30b0, 0x01},
	{0x30b8, 0x44},
	{0x3204, 0x0c},
	{0x3205, 0x87},
	{0x3206, 0x07},
	{0x3207, 0x0f},
	{0x3208, 0x0c},
	{0x3209, 0x80},
	{0x320a, 0x07},
	{0x320b, 0x08},
	{0x320c, 0x03},
	{0x320d, 0xc0},
	{0x320e, 0x07},
	{0x320f, 0x53},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3223, 0xc0},
	{0x3250, 0x00},
	{0x3271, 0x10},
	{0x327f, 0x3f},
	{0x32e0, 0x00},
	{0x3301, 0x12},
	{0x3304, 0x50},
	{0x3305, 0x00},
	{0x3306, 0x70},
	{0x3308, 0x18},
	{0x3309, 0xb0},
	{0x330a, 0x01},
	{0x330b, 0x20},
	{0x331e, 0x39},
	{0x331f, 0x99},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3364, 0x5e},
	{0x338f, 0xa0},
	{0x3393, 0x18},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x12},
	{0x339a, 0x16},
	{0x339b, 0x1e},
	{0x339c, 0x2e},
	{0x33ac, 0x0c},
	{0x33ad, 0x2c},
	{0x33ae, 0x30},
	{0x33af, 0x90},
	{0x33b0, 0x0f},
	{0x33b2, 0x24},
	{0x33b3, 0x10},
	{0x33f8, 0x00},
	{0x33f9, 0x70},
	{0x33fa, 0x00},
	{0x33fb, 0x70},
	{0x349f, 0x03},
	{0x34a8, 0x10},
	{0x34a9, 0x10},
	{0x34aa, 0x01},
	{0x34ab, 0x20},
	{0x34ac, 0x01},
	{0x34ad, 0x20},
	{0x34f9, 0x12},
	{0x3632, 0x6d},
	{0x3633, 0x4d},
	{0x363a, 0x80},
	{0x363b, 0x57},
	{0x363c, 0xd8},
	{0x363d, 0x40},
	{0x3670, 0x42},
	{0x3671, 0x33},
	{0x3672, 0x34},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x69},
	{0x367f, 0x6d},
	{0x3680, 0x8d},
	{0x3681, 0x04},
	{0x3682, 0x08},
	{0x3683, 0x04},
	{0x3684, 0x78},
	{0x3685, 0x80},
	{0x3686, 0x80},
	{0x3687, 0x83},
	{0x3688, 0x82},
	{0x3689, 0x85},
	{0x368a, 0x8b},
	{0x368b, 0x97},
	{0x368c, 0xae},
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
	{0x36ea, 0x14},
	{0x36eb, 0x45},
	{0x36ec, 0x4b},
	{0x36ed, 0x18},
	{0x370f, 0x13},
	{0x3721, 0x6c},
	{0x3722, 0x8b},
	{0x3724, 0xc1},
	{0x3727, 0x24},
	{0x3729, 0xb4},
	{0x37b0, 0x77},
	{0x37b1, 0x77},
	{0x37b2, 0x73},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x38},
	{0x37b7, 0x13},
	{0x37b8, 0x00},
	{0x37b9, 0x00},
	{0x37ba, 0xc4},
	{0x37bb, 0xc4},
	{0x37bc, 0xc4},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x38},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x38},
	{0x37fa, 0x18},
	{0x37fb, 0x55},
	{0x37fc, 0x19},
	{0x37fd, 0x1a},
	{0x3900, 0x05},
	{0x3903, 0x60},
	{0x3905, 0x0d},
	{0x391a, 0x60},
	{0x391b, 0x40},
	{0x391c, 0x26},
	{0x391d, 0x00},
	{0x3926, 0xe0},
	{0x3933, 0x80},
	{0x3934, 0x06},
	{0x3935, 0x00},
	{0x3936, 0x72},
	{0x3937, 0x71},
	{0x3938, 0x75},
	{0x3939, 0x0f},
	{0x393a, 0xf3},
	{0x393b, 0x0f},
	{0x393c, 0xd8},
	{0x393f, 0x80},
	{0x3940, 0x0b},
	{0x3941, 0x00},
	{0x3942, 0x0b},
	{0x3943, 0x7e},
	{0x3944, 0x7f},
	{0x3945, 0x7f},
	{0x3946, 0x7e},
	{0x39dd, 0x00},
	{0x39de, 0x08},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0x74},
	{0x3e02, 0xb0},
	{0x3e03, 0x0b},
	{0x3e08, 0x00},
	{0x3e16, 0x01},
	{0x3e17, 0x54},
	{0x3e18, 0x01},
	{0x3e19, 0x54},
	{0x4402, 0x11},
	{0x450a, 0x80},
	{0x450d, 0x0a},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x4837, 0x1d},
	{0x5000, 0x26},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x08},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x08},
	{0x578d, 0x41},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x58c0, 0x30},
	{0x58c1, 0x28},
	{0x58c2, 0x20},
	{0x58c3, 0x30},
	{0x58c4, 0x28},
	{0x58c5, 0x20},
	{0x58c6, 0x3c},
	{0x58c7, 0x30},
	{0x58c8, 0x28},
	{0x58c9, 0x3c},
	{0x58ca, 0x30},
	{0x58cb, 0x28},
	{0x36e9, 0x24},
	{0x37f9, 0x24},
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 1080Mbps, 4lane
 * hdr2: 3200x1800
 * Cleaned_0x03_SC635HAI_raw_MIPI_27Minput_4Lane_10bit_1080Mbps_3200x1800_30fps.ini
 */
static const struct regval sc635hai_linear_10_3200x1800_30fps_4lane_regs[] = {
	{0x3105, 0x32},
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x302c, 0x0c},
	{0x302c, 0x00},
	{0x3105, 0x12},
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
	{0x23bc, 0x04},
	{0x23bd, 0x08},
	{0x23be, 0x04},
	{0x23bf, 0x78},
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
	{0x301f, 0x03},
	{0x302c, 0x00},
	{0x30b0, 0x01},
	{0x30b8, 0x44},
	{0x3204, 0x0c},
	{0x3205, 0x87},
	{0x3206, 0x07},
	{0x3207, 0x0f},
	{0x3208, 0x0c},
	{0x3209, 0x80},
	{0x320a, 0x07},
	{0x320b, 0x08},
	{0x320c, 0x03},
	{0x320d, 0xc0},
	{0x320e, 0x0e},
	{0x320f, 0xa6},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3223, 0xc0},
	{0x3250, 0x00},
	{0x3271, 0x10},
	{0x327f, 0x3f},
	{0x32e0, 0x00},
	{0x3301, 0x12},
	{0x3304, 0x50},
	{0x3305, 0x00},
	{0x3306, 0x70},
	{0x3308, 0x18},
	{0x3309, 0xb0},
	{0x330a, 0x01},
	{0x330b, 0x20},
	{0x331e, 0x39},
	{0x331f, 0x99},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3364, 0x5e},
	{0x338f, 0xa0},
	{0x3393, 0x18},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x12},
	{0x339a, 0x16},
	{0x339b, 0x1e},
	{0x339c, 0x2e},
	{0x33ac, 0x0c},
	{0x33ad, 0x2c},
	{0x33ae, 0x30},
	{0x33af, 0x90},
	{0x33b0, 0x0f},
	{0x33b2, 0x24},
	{0x33b3, 0x10},
	{0x33f8, 0x00},
	{0x33f9, 0x70},
	{0x33fa, 0x00},
	{0x33fb, 0x70},
	{0x349f, 0x03},
	{0x34a8, 0x10},
	{0x34a9, 0x10},
	{0x34aa, 0x01},
	{0x34ab, 0x20},
	{0x34ac, 0x01},
	{0x34ad, 0x20},
	{0x34f9, 0x12},
	{0x3632, 0x6d},
	{0x3633, 0x4d},
	{0x363a, 0x80},
	{0x363b, 0x57},
	{0x363c, 0xd8},
	{0x363d, 0x40},
	{0x3670, 0x42},
	{0x3671, 0x33},
	{0x3672, 0x34},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x69},
	{0x367f, 0x6d},
	{0x3680, 0x8d},
	{0x3681, 0x04},
	{0x3682, 0x08},
	{0x3683, 0x04},
	{0x3684, 0x78},
	{0x3685, 0x80},
	{0x3686, 0x80},
	{0x3687, 0x83},
	{0x3688, 0x82},
	{0x3689, 0x85},
	{0x368a, 0x8b},
	{0x368b, 0x97},
	{0x368c, 0xae},
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
	{0x36ea, 0x14},
	{0x36eb, 0x45},
	{0x36ec, 0x4b},
	{0x36ed, 0x18},
	{0x370f, 0x13},
	{0x3721, 0x6c},
	{0x3722, 0x8b},
	{0x3724, 0xc1},
	{0x3727, 0x24},
	{0x3729, 0xb4},
	{0x37b0, 0x77},
	{0x37b1, 0x77},
	{0x37b2, 0x73},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x38},
	{0x37b7, 0x13},
	{0x37b8, 0x00},
	{0x37b9, 0x00},
	{0x37ba, 0xc4},
	{0x37bb, 0xc4},
	{0x37bc, 0xc4},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x38},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x38},
	{0x37fa, 0x18},
	{0x37fb, 0x55},
	{0x37fc, 0x19},
	{0x37fd, 0x1a},
	{0x3900, 0x05},
	{0x3903, 0x60},
	{0x3905, 0x0d},
	{0x391a, 0x60},
	{0x391b, 0x40},
	{0x391c, 0x26},
	{0x391d, 0x00},
	{0x3926, 0xe0},
	{0x3933, 0x80},
	{0x3934, 0x06},
	{0x3935, 0x00},
	{0x3936, 0x72},
	{0x3937, 0x71},
	{0x3938, 0x75},
	{0x3939, 0x0f},
	{0x393a, 0xf3},
	{0x393b, 0x0f},
	{0x393c, 0xd8},
	{0x393f, 0x80},
	{0x3940, 0x0b},
	{0x3941, 0x00},
	{0x3942, 0x0b},
	{0x3943, 0x7e},
	{0x3944, 0x7f},
	{0x3945, 0x7f},
	{0x3946, 0x7e},
	{0x39dd, 0x00},
	{0x39de, 0x08},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0xe9},
	{0x3e02, 0xe0},
	{0x3e03, 0x0b},
	{0x3e08, 0x00},
	{0x3e16, 0x01},
	{0x3e17, 0x54},
	{0x3e18, 0x01},
	{0x3e19, 0x54},
	{0x4402, 0x11},
	{0x450a, 0x80},
	{0x450d, 0x0a},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x4837, 0x1d},
	{0x5000, 0x26},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x08},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x08},
	{0x578d, 0x41},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x58c0, 0x30},
	{0x58c1, 0x28},
	{0x58c2, 0x20},
	{0x58c3, 0x30},
	{0x58c4, 0x28},
	{0x58c5, 0x20},
	{0x58c6, 0x3c},
	{0x58c7, 0x30},
	{0x58c8, 0x28},
	{0x58c9, 0x3c},
	{0x58ca, 0x30},
	{0x58cb, 0x28},
	{0x36e9, 0x24},
	{0x37f9, 0x24},
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 720Mbps, 2lane
 * linear: 3200x1800
 * Cleaned_0x13_SC635HAI_raw_MIPI_27Minput_2Lane_10bit_1080Mbps_3200x1800_30fps.ini
 */
static const struct regval sc635hai_linear_10_3200x1800_30fps_2lane_regs[] = {
	{0x3105, 0x32},
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x302c, 0x0c},
	{0x302c, 0x00},
	{0x3105, 0x12},
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
	{0x23bc, 0x04},
	{0x23bd, 0x08},
	{0x23be, 0x04},
	{0x23bf, 0x78},
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
	{0x3018, 0x3b},
	{0x3019, 0x0c},
	{0x301e, 0xf0},
	{0x301f, 0x13},
	{0x302c, 0x00},
	{0x30b0, 0x01},
	{0x30b8, 0x44},
	{0x3204, 0x0c},
	{0x3205, 0x87},
	{0x3206, 0x07},
	{0x3207, 0x0f},
	{0x3208, 0x0c},
	{0x3209, 0x80},
	{0x320a, 0x07},
	{0x320b, 0x08},
	{0x320c, 0x07},
	{0x320d, 0x80},
	{0x320e, 0x07},
	{0x320f, 0x53},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3223, 0xc0},
	{0x3250, 0x00},
	{0x3271, 0x10},
	{0x327f, 0x3f},
	{0x32e0, 0x00},
	{0x3301, 0x12},
	{0x3304, 0x50},
	{0x3305, 0x00},
	{0x3306, 0x70},
	{0x3308, 0x18},
	{0x3309, 0xb0},
	{0x330a, 0x01},
	{0x330b, 0x20},
	{0x331e, 0x39},
	{0x331f, 0x99},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3364, 0x5e},
	{0x338f, 0xa0},
	{0x3393, 0x18},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x12},
	{0x339a, 0x16},
	{0x339b, 0x1e},
	{0x339c, 0x2e},
	{0x33ac, 0x0c},
	{0x33ad, 0x2c},
	{0x33ae, 0x30},
	{0x33af, 0x90},
	{0x33b0, 0x0f},
	{0x33b2, 0x24},
	{0x33b3, 0x10},
	{0x33f8, 0x00},
	{0x33f9, 0x70},
	{0x33fa, 0x00},
	{0x33fb, 0x70},
	{0x349f, 0x03},
	{0x34a8, 0x10},
	{0x34a9, 0x10},
	{0x34aa, 0x01},
	{0x34ab, 0x20},
	{0x34ac, 0x01},
	{0x34ad, 0x20},
	{0x34f9, 0x12},
	{0x3632, 0x6d},
	{0x3633, 0x4d},
	{0x363a, 0x80},
	{0x363b, 0x57},
	{0x363c, 0xd8},
	{0x363d, 0x40},
	{0x3670, 0x42},
	{0x3671, 0x33},
	{0x3672, 0x34},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x69},
	{0x367f, 0x6d},
	{0x3680, 0x8d},
	{0x3681, 0x04},
	{0x3682, 0x08},
	{0x3683, 0x04},
	{0x3684, 0x78},
	{0x3685, 0x80},
	{0x3686, 0x80},
	{0x3687, 0x83},
	{0x3688, 0x82},
	{0x3689, 0x85},
	{0x368a, 0x8b},
	{0x368b, 0x97},
	{0x368c, 0xae},
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
	{0x36ea, 0x14},
	{0x36eb, 0x45},
	{0x36ec, 0x4b},
	{0x36ed, 0x18},
	{0x370f, 0x13},
	{0x3721, 0x6c},
	{0x3722, 0x8b},
	{0x3724, 0xc1},
	{0x3727, 0x24},
	{0x3729, 0xb4},
	{0x37b0, 0x77},
	{0x37b1, 0x77},
	{0x37b2, 0x73},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x38},
	{0x37b7, 0x13},
	{0x37b8, 0x00},
	{0x37b9, 0x00},
	{0x37ba, 0xc4},
	{0x37bb, 0xc4},
	{0x37bc, 0xc4},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x38},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x38},
	{0x37fa, 0x18},
	{0x37fb, 0x55},
	{0x37fc, 0x19},
	{0x37fd, 0x1a},
	{0x3900, 0x05},
	{0x3903, 0x60},
	{0x3905, 0x0d},
	{0x391a, 0x60},
	{0x391b, 0x40},
	{0x391c, 0x26},
	{0x391d, 0x00},
	{0x3926, 0xe0},
	{0x3933, 0x80},
	{0x3934, 0x06},
	{0x3935, 0x00},
	{0x3936, 0x72},
	{0x3937, 0x71},
	{0x3938, 0x75},
	{0x3939, 0x0f},
	{0x393a, 0xf3},
	{0x393b, 0x0f},
	{0x393c, 0xd8},
	{0x393f, 0x80},
	{0x3940, 0x0b},
	{0x3941, 0x00},
	{0x3942, 0x0b},
	{0x3943, 0x7e},
	{0x3944, 0x7f},
	{0x3945, 0x7f},
	{0x3946, 0x7e},
	{0x39dd, 0x00},
	{0x39de, 0x08},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0x74},
	{0x3e02, 0xb0},
	{0x3e03, 0x0b},
	{0x3e08, 0x00},
	{0x3e16, 0x01},
	{0x3e17, 0x54},
	{0x3e18, 0x01},
	{0x3e19, 0x54},
	{0x4402, 0x11},
	{0x450a, 0x80},
	{0x450d, 0x0a},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x4837, 0x1d},
	{0x5000, 0x26},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x08},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x08},
	{0x578d, 0x41},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd2},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x58c0, 0x30},
	{0x58c1, 0x28},
	{0x58c2, 0x20},
	{0x58c3, 0x30},
	{0x58c4, 0x28},
	{0x58c5, 0x20},
	{0x58c6, 0x3c},
	{0x58c7, 0x30},
	{0x58c8, 0x28},
	{0x58c9, 0x3c},
	{0x58ca, 0x30},
	{0x58cb, 0x28},
	{0x36e9, 0x24},
	{0x37f9, 0x24},
	{REG_NULL, 0x00},
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

static const struct sc635hai_mode supported_modes_4lane[] = {
	/* linear 30fps */
	{
		.width = 3200,
		.height = 1800,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0e9e,
		.hts_def = 0x3c0 * 4,	// 3840
		.vts_def = 0x0ea6,	// 3750
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc635hai_global_4lane_regs,
		.reg_list = sc635hai_linear_10_3200x1800_30fps_4lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 0,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 4,
	},
	/* linear 60fps */
	{
		.width = 3200,
		.height = 1800,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.exp_def = 0x074b,
		.hts_def = 0x3c0 * 4,	// 3840
		.vts_def = 0x0753,	// 1875
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc635hai_global_4lane_regs,
		.reg_list = sc635hai_linear_10_3200x1800_60fps_4lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 0,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 4,
	},
};

static const struct sc635hai_mode supported_modes_2lane[] = {
	/* linear 30fps */
	{
		.width = 3200,
		.height = 1800,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x074b,
		.hts_def = 0x780 * 2,	// 3840
		.vts_def = 0x0753,	// 1875
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc635hai_global_4lane_regs,
		.reg_list = sc635hai_linear_10_3200x1800_30fps_2lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 0,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 2,
	}
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,
};

static const s64 link_freq_menu_items[] = {
	SC635HAI_LINK_FREQ_540,	/* 4 lanes */
};

static const char *const sc635hai_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4",
};


static int sc635hai_write_reg(struct i2c_client *client, u16 reg,
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

static int sc635hai_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = sc635hai_write_reg(client, regs[i].addr,
					SC635HAI_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

static int sc635hai_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

/* mode: 0 = lgain  1 = sgain */
static int sc635hai_set_gain_reg(struct sc635hai *sc635hai, u32 gain, int mode)
{
	struct i2c_client *client = sc635hai->client;
	u32 coarse_again = 0, coarse_dgain = 0, fine_again = 0, fine_dgain = 0;
	int ret = 0, gain_factor;

	if (gain <= SC635HAI_GAIN_MIN)
		gain = SC635HAI_GAIN_MIN;
	else if (gain > SC635HAI_GAIN_MAX)
		gain = SC635HAI_GAIN_MAX;

	gain_factor = gain * 1000 / 32;
	if (gain_factor < 2000) {		/* start again,  1.0x - 2.0x, 1000 * 2 = 2000*/
		coarse_again = 0x00;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 1000);
	} else if (gain_factor < 2660) {	/* 2.0x - 2.625x, 1000 * 2.66 = 2660 */
		coarse_again = 0x01;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 2000);
	} else if (gain_factor < 5320) {	/* 2.660x - 5.320x, 1000 * 5.32 = 5320 */
		coarse_again = 0x80;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 2660);
	} else if (gain_factor < 10640) {	/* 5.32x - 10.64x, 1000 * 10.64 = 10640 */
		coarse_again = 0x81;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 5320);
	} else if (gain_factor < 21280) {	/* 10.64x - 21.28x, 1000 * 21.28 = 21280 */
		coarse_again = 0x83;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 10640);
	} else if (gain_factor < 42560) {	/* 21.28x - 42.56x, 1000 * 42.56 = 42560 */
		coarse_again = 0x87;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 21280);
	} else if (gain_factor <= 83790) {	/* 42.56x - 83.79x, 1000 * 83.79 = 83790 */
		coarse_again = 0x8f;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 42560);
	} else if (gain_factor < 83790 * 2) {
		// open dgain begin,  max digital gain 15.875x,
		// the accuracy of the digital fractional gain is 1/32.
		coarse_again = 0x8f;
		coarse_dgain = 0x00;
		fine_again = 0x3f;
		fine_dgain = DIV_ROUND_UP(gain_factor * 128, 83790);
	} else if (gain_factor < 83790 * 4) {
		coarse_again = 0x8f;
		coarse_dgain = 0x01;
		fine_again = 0x3f;
		fine_dgain = DIV_ROUND_UP(gain_factor * 128, 83790 * 2);
	} else if (gain_factor < 83790 * 8) {
		coarse_again = 0x8f;
		coarse_dgain = 0x03;
		fine_again = 0x3f;
		fine_dgain = DIV_ROUND_UP(gain_factor * 128, 83790 * 4);
	} else if (gain_factor < 83790 * 16) {
		coarse_again = 0x8f;
		coarse_dgain = 0x07;
		fine_again = 0x3f;
		fine_dgain = DIV_ROUND_UP(gain_factor * 128, 83790 * 8);
	}
	dev_dbg(&client->dev, "c_again: 0x%x, c_dgain: 0x%x, f_again: 0x%x, f_dgain: 0x%0x\n",
		coarse_again, coarse_dgain, fine_again, fine_dgain);

	if (mode == SC635HAI_LGAIN) {
		ret = sc635hai_write_reg(sc635hai->client,
					SC635HAI_REG_DIG_GAIN,
					SC635HAI_REG_VALUE_08BIT,
					coarse_dgain);
		ret |= sc635hai_write_reg(sc635hai->client,
					 SC635HAI_REG_DIG_FINE_GAIN,
					 SC635HAI_REG_VALUE_08BIT,
					 fine_dgain);
		ret |= sc635hai_write_reg(sc635hai->client,
					 SC635HAI_REG_ANA_GAIN,
					 SC635HAI_REG_VALUE_08BIT,
					 coarse_again);
		ret |= sc635hai_write_reg(sc635hai->client,
					 SC635HAI_REG_ANA_FINE_GAIN,
					 SC635HAI_REG_VALUE_08BIT,
					 fine_again);
	} else {
		ret = sc635hai_write_reg(sc635hai->client,
					SC635HAI_REG_SDIG_GAIN,
					SC635HAI_REG_VALUE_08BIT,
					coarse_dgain);
		ret |= sc635hai_write_reg(sc635hai->client,
					 SC635HAI_REG_SDIG_FINE_GAIN,
					 SC635HAI_REG_VALUE_08BIT,
					 fine_dgain);
		ret |= sc635hai_write_reg(sc635hai->client,
					 SC635HAI_REG_SANA_GAIN,
					 SC635HAI_REG_VALUE_08BIT,
					 coarse_again);
		ret |= sc635hai_write_reg(sc635hai->client,
					 SC635HAI_REG_SANA_FINE_GAIN,
					 SC635HAI_REG_VALUE_08BIT,
					 fine_again);
	}
	return ret;
}

static int sc635hai_set_hdrae(struct sc635hai *sc635hai,
			     struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	u32 l_exp_max = 0;

	if (!sc635hai->has_init_exp && !sc635hai->streaming) {
		sc635hai->init_hdrae_exp = *ae;
		sc635hai->has_init_exp = true;
		dev_dbg(&sc635hai->client->dev, "sc635hai don't stream, record exp for hdr!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&sc635hai->client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (sc635hai->cur_mode->hdr_mode == HDR_X2) {
		//2 stagger
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	/*
	 * manual long exposure time in double-line overlap HDR mode,
	 * register value is in units of one line
	 * (3033[0],3e23~3e24) default value is 0x00c4 from reg list
	 * {326d[0],320e[7:0],320f} -{3033[0],3e23,3e24} - 15
	 */
	l_exp_max = sc635hai->cur_vts - 196 - 16;
	//set exposure
	l_exp_time = l_exp_time * 2;
	s_exp_time = s_exp_time * 2;
	if (l_exp_time > l_exp_max)
		l_exp_time = l_exp_max;

	/*
	 * read regs list to get (3e23~3e24) value, then subtract 11
	 * (3033[0], 3e23~3e24) default value is 0x00c4  from reg list
	 * 184 = (3033[0],3e23~3e24) - 13
	 */
	if (s_exp_time > 184)
		s_exp_time = 184;

	ret = sc635hai_write_reg(sc635hai->client,
				SC635HAI_REG_EXPOSURE_H,
				SC635HAI_REG_VALUE_08BIT,
				SC635HAI_FETCH_EXP_H(l_exp_time));
	ret |= sc635hai_write_reg(sc635hai->client,
				 SC635HAI_REG_EXPOSURE_M,
				 SC635HAI_REG_VALUE_08BIT,
				 SC635HAI_FETCH_EXP_M(l_exp_time));
	ret |= sc635hai_write_reg(sc635hai->client,
				 SC635HAI_REG_EXPOSURE_L,
				 SC635HAI_REG_VALUE_08BIT,
				 SC635HAI_FETCH_EXP_L(l_exp_time));
	ret |= sc635hai_write_reg(sc635hai->client,
				 SC635HAI_REG_SEXPOSURE_M,
				 SC635HAI_REG_VALUE_08BIT,
				 SC635HAI_FETCH_EXP_M(s_exp_time));
	ret |= sc635hai_write_reg(sc635hai->client,
				 SC635HAI_REG_SEXPOSURE_L,
				 SC635HAI_REG_VALUE_08BIT,
				 SC635HAI_FETCH_EXP_L(s_exp_time));

	ret |= sc635hai_set_gain_reg(sc635hai, l_a_gain, SC635HAI_LGAIN);
	ret |= sc635hai_set_gain_reg(sc635hai, s_a_gain, SC635HAI_SGAIN);
	return ret;
}

static int sc635hai_get_reso_dist(const struct sc635hai_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc635hai_mode *
sc635hai_find_best_fit(struct sc635hai *sc635hai, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < sc635hai->cfg_num; i++) {
		dist = sc635hai_get_reso_dist(&sc635hai->supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		} else if (dist == cur_best_fit_dist &&
			   framefmt->code == sc635hai->supported_modes[i].bus_fmt) {
			cur_best_fit = i;
			break;
		}
	}

	return &sc635hai->supported_modes[cur_best_fit];
}

static int sc635hai_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	const struct sc635hai_mode *mode;
	s64 h_blank, vblank_def;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc635hai->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&sc635hai->mutex);

	mode = sc635hai_find_best_fit(sc635hai, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&sc635hai->mutex);
		return -ENOTTY;
#endif
	} else {
		sc635hai->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc635hai->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc635hai->vblank, vblank_def,
					 SC635HAI_VTS_MAX - mode->height,
					 1, vblank_def);
		dst_link_freq = mode->link_freq_idx;
		dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
				 mode->bpp * 2 * lanes;
		__v4l2_ctrl_s_ctrl_int64(sc635hai->pixel_rate,
					 dst_pixel_rate);
		__v4l2_ctrl_s_ctrl(sc635hai->link_freq,
				   dst_link_freq);
		sc635hai->cur_fps = mode->max_fps;
	}

	mutex_unlock(&sc635hai->mutex);

	return 0;
}

static int sc635hai_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	const struct sc635hai_mode *mode = sc635hai->cur_mode;

	mutex_lock(&sc635hai->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&sc635hai->mutex);
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
	mutex_unlock(&sc635hai->mutex);

	return 0;
}

static int sc635hai_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int sc635hai_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);

	if (fse->index >= sc635hai->cfg_num)
		return -EINVAL;

	if (fse->code != sc635hai->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = sc635hai->supported_modes[fse->index].width;
	fse->max_width  = sc635hai->supported_modes[fse->index].width;
	fse->min_height = sc635hai->supported_modes[fse->index].height;
	fse->max_height = sc635hai->supported_modes[fse->index].height;

	return 0;
}

static int sc635hai_enable_test_pattern(struct sc635hai *sc635hai, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc635hai_read_reg(sc635hai->client, SC635HAI_REG_TEST_PATTERN,
			       SC635HAI_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC635HAI_TEST_PATTERN_BIT_MASK;
	else
		val &= ~SC635HAI_TEST_PATTERN_BIT_MASK;

	ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_TEST_PATTERN,
				 SC635HAI_REG_VALUE_08BIT, val);
	return ret;
}

static int sc635hai_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	const struct sc635hai_mode *mode = sc635hai->cur_mode;

	if (sc635hai->streaming)
		fi->interval = sc635hai->cur_fps;
	else
		fi->interval = mode->max_fps;
	return 0;
}

static const struct sc635hai_mode *sc635hai_find_mode(struct sc635hai *sc635hai, int fps)
{
	const struct sc635hai_mode *mode = NULL;
	const struct sc635hai_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < sc635hai->cfg_num; i++) {
		mode = &sc635hai->supported_modes[i];
		if (mode->width == sc635hai->cur_mode->width &&
		    mode->height == sc635hai->cur_mode->height &&
		    mode->hdr_mode == sc635hai->cur_mode->hdr_mode &&
		    mode->bus_fmt == sc635hai->cur_mode->bus_fmt) {
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

static int sc635hai_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	const struct sc635hai_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	int fps;

	if (sc635hai->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = sc635hai_find_mode(sc635hai, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	sc635hai->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(sc635hai->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(sc635hai->vblank, vblank_def,
				 SC635HAI_VTS_MAX - mode->height,
				 1, vblank_def);
	pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
		     mode->bpp * 2 * mode->lanes;

	__v4l2_ctrl_s_ctrl_int64(sc635hai->pixel_rate,
				 pixel_rate);
	__v4l2_ctrl_s_ctrl(sc635hai->link_freq,
			   mode->link_freq_idx);
	sc635hai->cur_fps = mode->max_fps;

	return 0;
}

static int sc635hai_g_mbus_config(struct v4l2_subdev *sd,
				 unsigned int pad_id,
				 struct v4l2_mbus_config *config)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	u8 lanes = sc635hai->bus_cfg.bus.mipi_csi2.num_data_lanes;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = lanes;

	return 0;
}

static void sc635hai_get_module_inf(struct sc635hai *sc635hai,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC635HAI_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc635hai->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc635hai->len_name, sizeof(inf->base.lens));
}

static int sc635hai_set_setting(struct sc635hai *sc635hai, struct rk_sensor_setting *setting)
{
	int i = 0;
	int cur_fps = 0;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	const struct sc635hai_mode *mode = NULL;
	const struct sc635hai_mode *match = NULL;
	u8 lane = sc635hai->bus_cfg.bus.mipi_csi2.num_data_lanes;

	dev_info(&sc635hai->client->dev,
		 "sensor setting: %d x %d, fps:%d fmt:%d, mode:%d\n",
		 setting->width, setting->height,
		 setting->fps, setting->fmt, setting->mode);

	for (i = 0; i < sc635hai->cfg_num; i++) {
		mode = &sc635hai->supported_modes[i];
		if (mode->width == setting->width &&
		    mode->height == setting->height &&
		    mode->hdr_mode == setting->mode &&
		    mode->bus_fmt == setting->fmt) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator,
						    mode->max_fps.numerator);
			if (cur_fps == setting->fps) {
				match = mode;
				break;
			}
		}
	}

	if (match) {
		dev_info(&sc635hai->client->dev, "-----%s: match the support mode, mode idx:%d-----\n",
			 __func__, i);
		sc635hai->cur_mode = mode;

		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc635hai->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc635hai->vblank, vblank_def,
					 SC635HAI_VTS_MAX - mode->height,
					 1, vblank_def);


		__v4l2_ctrl_s_ctrl(sc635hai->link_freq, mode->link_freq_idx);
		pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
			     mode->bpp * 2 * lane;
		__v4l2_ctrl_s_ctrl_int64(sc635hai->pixel_rate, pixel_rate);
		dev_info(&sc635hai->client->dev, "freq_idx:%d pixel_rate:%lld\n",
			 mode->link_freq_idx, pixel_rate);

		sc635hai->cur_vts = mode->vts_def;
		sc635hai->cur_fps = mode->max_fps;

		dev_info(&sc635hai->client->dev, "hts_def:%d cur_vts:%d cur_fps:%d\n",
			 mode->hts_def, mode->vts_def,
			 sc635hai->cur_fps.denominator / sc635hai->cur_fps.numerator);
	} else {
		dev_err(&sc635hai->client->dev, "couldn't match the support modes\n");
		return -EINVAL;
	}

	return 0;
}

static int sc635hai_adjust_time(struct sc635hai *sc635hai)
{
	int ret = 0;
	u32 val;

	/* Read and modify register 0x36e9 */
	ret |= sc635hai_read_reg(sc635hai->client, 0x36e9, SC635HAI_REG_VALUE_08BIT, &val);
	val |= 0x80;
	ret |= sc635hai_write_reg(sc635hai->client, 0x36e9, SC635HAI_REG_VALUE_08BIT, val);

	/* Read and modify register 0x36f9 */
	ret |= sc635hai_read_reg(sc635hai->client, 0x36f9, SC635HAI_REG_VALUE_08BIT, &val);
	val |= 0x80;
	ret |= sc635hai_write_reg(sc635hai->client, 0x36f9, SC635HAI_REG_VALUE_08BIT, val);

	return ret;
}

static long sc635hai_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rk_sensor_setting *setting;
	u32 i, h, w;
	long ret = 0;
	u32 stream = 0;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc635hai->bus_cfg.bus.mipi_csi2.num_data_lanes;
	const struct sc635hai_mode *mode;
	int cur_best_fit = -1;
	int cur_best_fit_dist = -1;
	int cur_dist, cur_fps, dst_fps;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc635hai_get_module_inf(sc635hai, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = sc635hai->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		if (hdr->hdr_mode == sc635hai->cur_mode->hdr_mode)
			return 0;
		w = sc635hai->cur_mode->width;
		h = sc635hai->cur_mode->height;
		dst_fps = DIV_ROUND_CLOSEST(sc635hai->cur_mode->max_fps.denominator,
					    sc635hai->cur_mode->max_fps.numerator);
		for (i = 0; i < sc635hai->cfg_num; i++) {
			if (w == sc635hai->supported_modes[i].width &&
			    h == sc635hai->supported_modes[i].height &&
			    sc635hai->supported_modes[i].hdr_mode == hdr->hdr_mode &&
			    sc635hai->supported_modes[i].bus_fmt == sc635hai->cur_mode->bus_fmt) {
				cur_fps = DIV_ROUND_CLOSEST(sc635hai->supported_modes[i].max_fps.denominator,
							    sc635hai->supported_modes[i].max_fps.numerator);
				cur_dist = abs(cur_fps - dst_fps);
				if (cur_best_fit_dist == -1 || cur_dist < cur_best_fit_dist) {
					cur_best_fit_dist = cur_dist;
					cur_best_fit = i;
				} else if (cur_dist == cur_best_fit_dist) {
					cur_best_fit = i;
					break;
				}
			}
		}
		if (cur_best_fit == -1) {
			dev_err(&sc635hai->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			sc635hai->cur_mode = &sc635hai->supported_modes[cur_best_fit];
			mode = sc635hai->cur_mode;
			w = mode->hts_def - mode->width;
			h = mode->vts_def - mode->height;
			__v4l2_ctrl_modify_range(sc635hai->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(sc635hai->vblank, h,
						 SC635HAI_VTS_MAX - sc635hai->cur_mode->height, 1, h);
			sc635hai->cur_fps = sc635hai->cur_mode->max_fps;

			dst_link_freq = mode->link_freq_idx;
			dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
					 mode->bpp * 2 * lanes;
			__v4l2_ctrl_s_ctrl_int64(sc635hai->pixel_rate,
						 dst_pixel_rate);
			__v4l2_ctrl_s_ctrl(sc635hai->link_freq,
					   dst_link_freq);
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		sc635hai_set_hdrae(sc635hai, arg);
		if (sc635hai->cam_sw_inf)
			memcpy(&sc635hai->cam_sw_inf->hdr_ae, (struct preisp_hdrae_exp_s *)(arg),
			       sizeof(struct preisp_hdrae_exp_s));
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (sc635hai->standby_hw) {	/* hardware standby */
			if (stream) {
				sc635hai->is_standby = false;
				/* pwdn gpio pull up */
				if (!IS_ERR(sc635hai->pwdn_gpio))
					gpiod_set_value_cansleep(sc635hai->pwdn_gpio, 1);
				// Make sure __v4l2_ctrl_handler_setup can be called correctly
				usleep_range(4000, 5000);
				/* mipi clk on */
				ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_MIPI_CTRL,
							 SC635HAI_REG_VALUE_08BIT,
							 SC635HAI_MIPI_CTRL_ON);
				/* adjust timing */
				ret |= sc635hai_adjust_time(sc635hai);

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
				if (__v4l2_ctrl_handler_setup(&sc635hai->ctrl_handler))
					dev_err(&sc635hai->client->dev, "__v4l2_ctrl_handler_setup fail!");
				/* Check if the current mode is HDR and cam sw info is available */
				if (sc635hai->cur_mode->hdr_mode != NO_HDR && sc635hai->cam_sw_inf) {
					ret = sc635hai_ioctl(&sc635hai->subdev,
							    PREISP_CMD_SET_HDRAE_EXP,
							    &sc635hai->cam_sw_inf->hdr_ae);
					if (ret) {
						dev_err(&sc635hai->client->dev,
							"Failed init exp fail in hdr mode\n");
						return ret;
					}

				}
#endif

				/* stream on */
				ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_CTRL_MODE,
							 SC635HAI_REG_VALUE_08BIT,
							 SC635HAI_MODE_STREAMING);
				dev_info(&sc635hai->client->dev,
					 "quickstream, streaming on: exit hw standby mode\n");
			} else {
				/* adjust timing */
				ret |= sc635hai_adjust_time(sc635hai);

				/* stream off */
				ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_CTRL_MODE,
							 SC635HAI_REG_VALUE_08BIT,
							 SC635HAI_MODE_SW_STANDBY);
				/* mipi clk off */
				ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_MIPI_CTRL,
							 SC635HAI_REG_VALUE_08BIT,
							 SC635HAI_MIPI_CTRL_OFF);

				sc635hai->is_standby = true;
				/* pwnd gpio pull down */
				if (!IS_ERR(sc635hai->pwdn_gpio))
					gpiod_set_value_cansleep(sc635hai->pwdn_gpio, 0);
				dev_info(&sc635hai->client->dev,
					 "quickstream, streaming off: enter hw standby mode\n");
			}
		} else {	/* software standby */
			if (stream) {
				ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_MIPI_CTRL,
							 SC635HAI_REG_VALUE_08BIT,
							 SC635HAI_MIPI_CTRL_ON);
				ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_CTRL_MODE,
							 SC635HAI_REG_VALUE_08BIT,
							 SC635HAI_MODE_STREAMING);
				dev_info(&sc635hai->client->dev,
					 "quickstream, streaming on: exit soft standby mode\n");
			} else {
				ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_CTRL_MODE,
							 SC635HAI_REG_VALUE_08BIT,
							 SC635HAI_MODE_SW_STANDBY);
				ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_REG_MIPI_CTRL,
							 SC635HAI_REG_VALUE_08BIT,
							 SC635HAI_MIPI_CTRL_OFF);
				dev_info(&sc635hai->client->dev,
					 "quickstream, streaming off: enter soft standby mode\n");
			}
		}
		break;
	case RKCIS_CMD_SELECT_SETTING:
		setting = (struct rk_sensor_setting *)arg;
		ret = sc635hai_set_setting(sc635hai, setting);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}


#ifdef CONFIG_COMPAT
static long sc635hai_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rk_sensor_setting *setting;
	struct rk_light_param *light_param;
	long ret;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc635hai_ioctl(sd, cmd, inf);
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

		ret = sc635hai_ioctl(sd, cmd, hdr);
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
			ret = sc635hai_ioctl(sd, cmd, hdr);
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
			ret = sc635hai_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = sc635hai_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKCIS_CMD_SELECT_SETTING:
		setting = kzalloc(sizeof(*setting), GFP_KERNEL);
		if (!setting) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(setting, up, sizeof(*setting));
		if (!ret)
			ret = sc635hai_ioctl(sd, cmd, setting);
		else
			ret = -EFAULT;
		kfree(setting);
		break;
	case RKCIS_CMD_FLASH_LIGHT_CTRL:
		light_param = kzalloc(sizeof(*light_param), GFP_KERNEL);
		if (!light_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(light_param, up, sizeof(*light_param));
		if (!ret)
			ret = sc635hai_ioctl(sd, cmd, light_param);
		else
			ret = -EFAULT;
		kfree(light_param);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __sc635hai_start_stream(struct sc635hai *sc635hai)
{
	int ret;

	if (!sc635hai->is_thunderboot) {
		ret = sc635hai_write_array(sc635hai->client, sc635hai->cur_mode->reg_list);
		if (ret)
			return ret;
		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&sc635hai->ctrl_handler);
		if (ret)
			return ret;
		if (sc635hai->has_init_exp && sc635hai->cur_mode->hdr_mode != NO_HDR) {
			ret = sc635hai_ioctl(&sc635hai->subdev, PREISP_CMD_SET_HDRAE_EXP,
					    &sc635hai->init_hdrae_exp);
			if (ret) {
				dev_err(&sc635hai->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	ret = sc635hai_write_reg(sc635hai->client, SC635HAI_REG_CTRL_MODE,
				SC635HAI_REG_VALUE_08BIT, SC635HAI_MODE_STREAMING);
	return ret;
}

static int __sc635hai_stop_stream(struct sc635hai *sc635hai)
{
	sc635hai->has_init_exp = false;
	if (sc635hai->is_thunderboot)
		sc635hai->is_first_streamoff = true;
	return sc635hai_write_reg(sc635hai->client, SC635HAI_REG_CTRL_MODE,
				 SC635HAI_REG_VALUE_08BIT, SC635HAI_MODE_SW_STANDBY);
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 sc635hai_cal_delay(u32 cycles, struct sc635hai *sc635hai)
{
	return DIV_ROUND_UP(cycles, sc635hai->cur_mode->mclk / 1000 / 1000);
}

static int __sc635hai_power_on(struct sc635hai *sc635hai)
{
	int ret;
	u32 delay_us;
	struct device *dev = &sc635hai->client->dev;

	if (!IS_ERR_OR_NULL(sc635hai->pins_default)) {
		ret = pinctrl_select_state(sc635hai->pinctrl,
					   sc635hai->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc635hai->xvclk, sc635hai->cur_mode->mclk);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (%dHz)\n", sc635hai->cur_mode->mclk);
	if (clk_get_rate(sc635hai->xvclk) != sc635hai->cur_mode->mclk)
		dev_warn(dev, "xvclk mismatched, modes are based on %dHz\n",
			 sc635hai->cur_mode->mclk);
	ret = clk_prepare_enable(sc635hai->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	cam_sw_regulator_bulk_init(sc635hai->cam_sw_inf, SC635HAI_NUM_SUPPLIES, sc635hai->supplies);

	if (sc635hai->is_thunderboot)
		return 0;

	if (!IS_ERR(sc635hai->reset_gpio))
		gpiod_set_value_cansleep(sc635hai->reset_gpio, 0);

	ret = regulator_bulk_enable(SC635HAI_NUM_SUPPLIES, sc635hai->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc635hai->reset_gpio))
		gpiod_set_value_cansleep(sc635hai->reset_gpio, 1);

	usleep_range(500, 1000);

	if (!IS_ERR(sc635hai->pwdn_gpio))
		gpiod_set_value_cansleep(sc635hai->pwdn_gpio, 1);

	if (!IS_ERR(sc635hai->reset_gpio))
		usleep_range(6000, 8000);
	else
		usleep_range(12000, 16000);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = sc635hai_cal_delay(8192, sc635hai);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(sc635hai->xvclk);

	return ret;
}

static void __sc635hai_power_off(struct sc635hai *sc635hai)
{
	int ret;
	struct device *dev = &sc635hai->client->dev;

	clk_disable_unprepare(sc635hai->xvclk);
	if (sc635hai->is_thunderboot) {
		if (sc635hai->is_first_streamoff) {
			sc635hai->is_thunderboot = false;
			sc635hai->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(sc635hai->pwdn_gpio))
		gpiod_set_value_cansleep(sc635hai->pwdn_gpio, 0);
	clk_disable_unprepare(sc635hai->xvclk);
	if (!IS_ERR(sc635hai->reset_gpio))
		gpiod_set_value_cansleep(sc635hai->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(sc635hai->pins_sleep)) {
		ret = pinctrl_select_state(sc635hai->pinctrl,
					   sc635hai->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(SC635HAI_NUM_SUPPLIES, sc635hai->supplies);
}

static int sc635hai_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	struct i2c_client *client = sc635hai->client;
	int ret = 0;

	mutex_lock(&sc635hai->mutex);

	on = !!on;
	if (on == sc635hai->streaming)
		goto unlock_and_return;

	if (on) {
		if (sc635hai->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			sc635hai->is_thunderboot = false;
			__sc635hai_power_on(sc635hai);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __sc635hai_start_stream(sc635hai);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc635hai_stop_stream(sc635hai);
		pm_runtime_put(&client->dev);
	}

	sc635hai->streaming = on;
unlock_and_return:
	mutex_unlock(&sc635hai->mutex);
	return ret;
}

static int sc635hai_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	struct i2c_client *client = sc635hai->client;
	int ret = 0;

	mutex_lock(&sc635hai->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc635hai->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!sc635hai->is_thunderboot) {
			ret = sc635hai_write_array(sc635hai->client,
						  sc635hai->cur_mode->global_reg_list);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		sc635hai->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc635hai->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc635hai->mutex);

	return ret;
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int __maybe_unused sc635hai_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc635hai *sc635hai = to_sc635hai(sd);

	if (sc635hai->standby_hw) {
		dev_info(dev, "resume standby!");
		return 0;
	}

	cam_sw_prepare_wakeup(sc635hai->cam_sw_inf, dev);
	usleep_range(4000, 5000);
	cam_sw_write_array(sc635hai->cam_sw_inf);

	if (__v4l2_ctrl_handler_setup(&sc635hai->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (sc635hai->has_init_exp && sc635hai->cur_mode != NO_HDR) {	// hdr mode
		ret = sc635hai_ioctl(&sc635hai->subdev, PREISP_CMD_SET_HDRAE_EXP,
				    &sc635hai->cam_sw_inf->hdr_ae);
		if (ret) {
			dev_err(&sc635hai->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}

	return 0;
}

static int __maybe_unused sc635hai_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc635hai *sc635hai = to_sc635hai(sd);

	if (sc635hai->standby_hw) {
		dev_info(dev, "suspend standby!");
		return 0;
	}

	cam_sw_write_array_cb_init(sc635hai->cam_sw_inf, client,
				   (void *)sc635hai->cur_mode->reg_list,
				   (sensor_write_array)sc635hai_write_array);
	cam_sw_prepare_sleep(sc635hai->cam_sw_inf);

	return 0;
}
#else
#define sc635hai_resume NULL
#define sc635hai_suspend NULL
#endif

static int __maybe_unused sc635hai_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc635hai *sc635hai = to_sc635hai(sd);

	return __sc635hai_power_on(sc635hai);
}

static int __maybe_unused sc635hai_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc635hai *sc635hai = to_sc635hai(sd);

	__sc635hai_power_off(sc635hai);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc635hai_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct sc635hai_mode *def_mode = &sc635hai->supported_modes[0];

	mutex_lock(&sc635hai->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc635hai->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc635hai_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct sc635hai *sc635hai = to_sc635hai(sd);

	if (fie->index >= sc635hai->cfg_num)
		return -EINVAL;

	fie->code = sc635hai->supported_modes[fie->index].bus_fmt;
	fie->width = sc635hai->supported_modes[fie->index].width;
	fie->height = sc635hai->supported_modes[fie->index].height;
	fie->interval = sc635hai->supported_modes[fie->index].max_fps;
	fie->reserved[0] = sc635hai->supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops sc635hai_pm_ops = {
	SET_RUNTIME_PM_OPS(sc635hai_runtime_suspend,
	sc635hai_runtime_resume, NULL)
#ifdef CONFIG_VIDEO_CAM_SLEEP_WAKEUP
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sc635hai_suspend, sc635hai_resume)
#endif
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc635hai_internal_ops = {
	.open = sc635hai_open,
};
#endif

static const struct v4l2_subdev_core_ops sc635hai_core_ops = {
	.s_power = sc635hai_s_power,
	.ioctl = sc635hai_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc635hai_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc635hai_video_ops = {
	.s_stream = sc635hai_s_stream,
	.g_frame_interval = sc635hai_g_frame_interval,
	.s_frame_interval = sc635hai_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops sc635hai_pad_ops = {
	.enum_mbus_code = sc635hai_enum_mbus_code,
	.enum_frame_size = sc635hai_enum_frame_sizes,
	.enum_frame_interval = sc635hai_enum_frame_interval,
	.get_fmt = sc635hai_get_fmt,
	.set_fmt = sc635hai_set_fmt,
	.get_mbus_config = sc635hai_g_mbus_config,
};

static const struct v4l2_subdev_ops sc635hai_subdev_ops = {
	.core	= &sc635hai_core_ops,
	.video	= &sc635hai_video_ops,
	.pad	= &sc635hai_pad_ops,
};

static void sc635hai_modify_fps_info(struct sc635hai *sc635hai)
{
	const struct sc635hai_mode *mode = sc635hai->cur_mode;

	sc635hai->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				       sc635hai->cur_vts;
}

static int sc635hai_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc635hai *sc635hai = container_of(ctrl->handler,
					       struct sc635hai, ctrl_handler);
	struct i2c_client *client = sc635hai->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc635hai->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(sc635hai->exposure,
					 sc635hai->exposure->minimum, max,
					 sc635hai->exposure->step,
					 sc635hai->exposure->default_value);
		break;
	}

	if (sc635hai->standby_hw && sc635hai->is_standby) {
		dev_dbg(&client->dev, "%s: is_standby = true, will return\n", __func__);
		return 0;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		if (sc635hai->cur_mode->hdr_mode == NO_HDR) {
			/* 4 least significant bits of expsoure are fractional part */
			ret = sc635hai_write_reg(sc635hai->client,
						SC635HAI_REG_EXPOSURE_H,
						SC635HAI_REG_VALUE_08BIT,
						SC635HAI_FETCH_EXP_H(ctrl->val));
			ret |= sc635hai_write_reg(sc635hai->client,
						 SC635HAI_REG_EXPOSURE_M,
						 SC635HAI_REG_VALUE_08BIT,
						 SC635HAI_FETCH_EXP_M(ctrl->val));
			ret |= sc635hai_write_reg(sc635hai->client,
						 SC635HAI_REG_EXPOSURE_L,
						 SC635HAI_REG_VALUE_08BIT,
						 SC635HAI_FETCH_EXP_L(ctrl->val));
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set gain 0x%x\n", ctrl->val);
		if (sc635hai->cur_mode->hdr_mode == NO_HDR)
			ret = sc635hai_set_gain_reg(sc635hai, ctrl->val, SC635HAI_LGAIN);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		ret = sc635hai_write_reg(sc635hai->client,
					SC635HAI_REG_VTS_H,
					SC635HAI_REG_VALUE_08BIT,
					0x00);
		ret |= sc635hai_write_reg(sc635hai->client,
					 SC635HAI_REG_VTS_M,
					 SC635HAI_REG_VALUE_08BIT,
					 (ctrl->val + sc635hai->cur_mode->height) >> 8);
		ret |= sc635hai_write_reg(sc635hai->client,
					 SC635HAI_REG_VTS_L,
					 SC635HAI_REG_VALUE_08BIT,
					 (ctrl->val + sc635hai->cur_mode->height) & 0xff);
		sc635hai->cur_vts = ctrl->val + sc635hai->cur_mode->height;
		if (sc635hai->cur_vts != sc635hai->cur_mode->vts_def)
			sc635hai_modify_fps_info(sc635hai);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc635hai_enable_test_pattern(sc635hai, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc635hai_read_reg(sc635hai->client, SC635HAI_FLIP_MIRROR_REG,
				       SC635HAI_REG_VALUE_08BIT, &val);
		ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_FLIP_MIRROR_REG,
					 SC635HAI_REG_VALUE_08BIT,
					 SC635HAI_FETCH_MIRROR(val, ctrl->val));
		break;
	case V4L2_CID_VFLIP:
		ret = sc635hai_read_reg(sc635hai->client, SC635HAI_FLIP_MIRROR_REG,
				       SC635HAI_REG_VALUE_08BIT, &val);
		ret |= sc635hai_write_reg(sc635hai->client, SC635HAI_FLIP_MIRROR_REG,
					 SC635HAI_REG_VALUE_08BIT,
					 SC635HAI_FETCH_FLIP(val, ctrl->val));
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}


static const struct v4l2_ctrl_ops sc635hai_ctrl_ops = {
	.s_ctrl = sc635hai_set_ctrl,
};

static int sc635hai_initialize_controls(struct sc635hai *sc635hai)
{
	const struct sc635hai_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc635hai->bus_cfg.bus.mipi_csi2.num_data_lanes;

	handler = &sc635hai->ctrl_handler;
	mode = sc635hai->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc635hai->mutex;

	sc635hai->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			     V4L2_CID_LINK_FREQ,
			     ARRAY_SIZE(link_freq_menu_items) - 1,
			     0, link_freq_menu_items);
	if (sc635hai->link_freq)
		sc635hai->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	dst_link_freq = mode->link_freq_idx;
	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
			 mode->bpp * 2 * lanes;
	if (lanes == 2) {
		sc635hai->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
							0, PIXEL_RATE_WITH_540M_10BIT_2L,
							1, dst_pixel_rate);
	} else if (lanes == 4) {
		if (mode->hdr_mode == NO_HDR)
			sc635hai->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
								0, PIXEL_RATE_WITH_540M_10BIT_4L,
								1, dst_pixel_rate);
		else if (mode->hdr_mode == HDR_X2)
			sc635hai->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
								0, PIXEL_RATE_WITH_540M_10BIT_4L,
								1, dst_pixel_rate);
	}

	__v4l2_ctrl_s_ctrl(sc635hai->link_freq, dst_link_freq);

	h_blank = mode->hts_def - mode->width;
	sc635hai->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					    h_blank, h_blank, 1, h_blank);
	if (sc635hai->hblank)
		sc635hai->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	sc635hai->vblank = v4l2_ctrl_new_std(handler, &sc635hai_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_def,
					    SC635HAI_VTS_MAX - mode->height,
					    1, vblank_def);
	exposure_max = mode->vts_def - 8;
	sc635hai->exposure = v4l2_ctrl_new_std(handler, &sc635hai_ctrl_ops,
					      V4L2_CID_EXPOSURE, SC635HAI_EXPOSURE_MIN,
					      exposure_max, SC635HAI_EXPOSURE_STEP,
					      mode->exp_def); //Set default exposure
	sc635hai->anal_gain = v4l2_ctrl_new_std(handler, &sc635hai_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN, SC635HAI_GAIN_MIN,
					       SC635HAI_GAIN_MAX, SC635HAI_GAIN_STEP,
					       SC635HAI_GAIN_DEFAULT); //Set default gain
	sc635hai->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&sc635hai_ctrl_ops,
				V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(sc635hai_test_pattern_menu) - 1,
				0, 0, sc635hai_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &sc635hai_ctrl_ops,
			  V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &sc635hai_ctrl_ops,
			  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&sc635hai->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc635hai->subdev.ctrl_handler = handler;
	sc635hai->has_init_exp = false;
	sc635hai->cur_fps = mode->max_fps;
	sc635hai->is_standby = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc635hai_check_sensor_id(struct sc635hai *sc635hai,
				   struct i2c_client *client)
{
	struct device *dev = &sc635hai->client->dev;
	u32 id = 0;
	int ret;

	if (sc635hai->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = sc635hai_read_reg(client, SC635HAI_REG_CHIP_ID,
			       SC635HAI_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected SC635HAI (0x%04x) sensor\n", CHIP_ID);

	return 0;
}

static int sc635hai_configure_regulators(struct sc635hai *sc635hai)
{
	unsigned int i;

	for (i = 0; i < SC635HAI_NUM_SUPPLIES; i++)
		sc635hai->supplies[i].supply = sc635hai_supply_names[i];

	return devm_regulator_bulk_get(&sc635hai->client->dev,
				       SC635HAI_NUM_SUPPLIES,
				       sc635hai->supplies);
}

static int sc635hai_read_module_info(struct sc635hai *sc635hai)
{
	int ret;
	struct device *dev = &sc635hai->client->dev;
	struct device_node *node = dev->of_node;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc635hai->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc635hai->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc635hai->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc635hai->len_name);
	if (ret)
		dev_err(dev, "could not get module information!\n");

	/* Compatible with non-standby mode if this attribute is not configured in dts*/
	of_property_read_u32(node, RKMODULE_CAMERA_STANDBY_HW,
			     &sc635hai->standby_hw);
	dev_info(dev, "sc635hai->standby_hw = %d\n", sc635hai->standby_hw);

	return ret;
}

static int sc635hai_find_modes(struct sc635hai *sc635hai)
{
	int i, ret;
	u32 hdr_mode = 0;
	struct device_node *endpoint;
	struct device *dev = &sc635hai->client->dev;
	struct device_node *node = sc635hai->client->dev.of_node;

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, "Get hdr mode failed! no hdr default\n");
	} else
		dev_warn(dev, "Get hdr mode OK! hdr_mode = %d\n", hdr_mode);

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}
	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
					 &sc635hai->bus_cfg);
	of_node_put(endpoint);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}

	dev_info(dev, "Detect sc635hai lane: %d\n",
		sc635hai->bus_cfg.bus.mipi_csi2.num_data_lanes);
	if (sc635hai->bus_cfg.bus.mipi_csi2.num_data_lanes == 4) {
		sc635hai->supported_modes = supported_modes_4lane;
		sc635hai->cfg_num = ARRAY_SIZE(supported_modes_4lane);
	} else {
		sc635hai->supported_modes = supported_modes_2lane;
		sc635hai->cfg_num = ARRAY_SIZE(supported_modes_2lane);
	}

	for (i = 0; i < sc635hai->cfg_num; i++) {
		if (hdr_mode == sc635hai->supported_modes[i].hdr_mode) {
			sc635hai->cur_mode = &sc635hai->supported_modes[i];
			break;
		}
	}

	if (i == sc635hai->cfg_num)
		sc635hai->cur_mode = &sc635hai->supported_modes[0];

	return 0;
}

static int sc635hai_setup_clocks_and_gpios(struct sc635hai *sc635hai)
{
	struct device *dev = &sc635hai->client->dev;

	sc635hai->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc635hai->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	sc635hai->reset_gpio = devm_gpiod_get(dev, "reset",
					     sc635hai->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(sc635hai->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	sc635hai->pwdn_gpio = devm_gpiod_get(dev, "pwdn",
					    sc635hai->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(sc635hai->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	sc635hai->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc635hai->pinctrl)) {
		sc635hai->pins_default =
			pinctrl_lookup_state(sc635hai->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc635hai->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		sc635hai->pins_sleep =
			pinctrl_lookup_state(sc635hai->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc635hai->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	return 0;
}

static int sc635hai_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc635hai *sc635hai;
	struct v4l2_subdev *sd;

	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	sc635hai = devm_kzalloc(dev, sizeof(*sc635hai), GFP_KERNEL);
	if (!sc635hai)
		return -ENOMEM;

	sc635hai->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);
	sc635hai->client = client;

	ret = sc635hai_read_module_info(sc635hai);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	/* Set current mode based on HDR mode */
	ret = sc635hai_find_modes(sc635hai);
	if (ret) {
		dev_err(dev, "Failed to get modes!\n");
		return -EINVAL;
	}

	/* setup sc635hai clock and gpios*/
	ret = sc635hai_setup_clocks_and_gpios(sc635hai);
	if (ret) {
		dev_err(dev, "Failed to set up clocks and GPIOs\n");
		return ret;
	}

	ret = sc635hai_configure_regulators(sc635hai);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc635hai->mutex);

	sd = &sc635hai->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc635hai_subdev_ops);
	ret = sc635hai_initialize_controls(sc635hai);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc635hai_power_on(sc635hai);
	if (ret)
		goto err_free_handler;

	ret = sc635hai_check_sensor_id(sc635hai, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc635hai_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc635hai->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc635hai->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	if (!sc635hai->cam_sw_inf) {
		sc635hai->cam_sw_inf = cam_sw_init();
		cam_sw_clk_init(sc635hai->cam_sw_inf, sc635hai->xvclk,
				sc635hai->cur_mode->mclk);
		cam_sw_reset_pin_init(sc635hai->cam_sw_inf, sc635hai->reset_gpio, 0);
		cam_sw_pwdn_pin_init(sc635hai->cam_sw_inf, sc635hai->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc635hai->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc635hai->module_index, facing,
		 SC635HAI_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (sc635hai->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__sc635hai_power_off(sc635hai);
err_free_handler:
	v4l2_ctrl_handler_free(&sc635hai->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc635hai->mutex);

	return ret;
}

static void sc635hai_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc635hai *sc635hai = to_sc635hai(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc635hai->ctrl_handler);
	mutex_destroy(&sc635hai->mutex);

	cam_sw_deinit(sc635hai->cam_sw_inf);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc635hai_power_off(sc635hai);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc635hai_of_match[] = {
	{ .compatible = "smartsens,sc635hai" },
	{},
};
MODULE_DEVICE_TABLE(of, sc635hai_of_match);
#endif

static const struct i2c_device_id sc635hai_match_id[] = {
	{ "smartsens,sc635hai", 0 },
	{ },
};

static struct i2c_driver sc635hai_i2c_driver = {
	.driver = {
		.name = SC635HAI_NAME,
		.pm = &sc635hai_pm_ops,
		.of_match_table = of_match_ptr(sc635hai_of_match),
	},
	.probe		= sc635hai_probe,
	.remove		= sc635hai_remove,
	.id_table	= sc635hai_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc635hai_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc635hai_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP) && !defined(CONFIG_INITCALL_ASYNC)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens sc635hai CMOS Image Sensor driver");
MODULE_LICENSE("GPL");
