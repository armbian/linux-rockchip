// SPDX-License-Identifier: GPL-2.0
/*
 * sc485sl driver
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 first version
 *  support thunderboot
 *  support sleep wake-up mode
 * V0.0X01.0X02 support 4lane 30fps setting
 * V0.0X01.0X03 support 2lane 60fps setting
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x03)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC485SL_BITS_PER_SAMPLE		10
#define SC485SL_LINK_FREQ_396		396000000	/* 792Mbps pre lane*/
#define SC485SL_LINK_FREQ_360		360000000	/* 720Mbps pre lane*/
#define SC485SL_LINK_FREQ_540		540000000	/* 1080Mbps pre lane*/
#define SC485SL_LINK_FREQ_720		720000000	/* 1440Mbps pre lane*/

#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

/* 2 lane */
/* 720Mbps pre lane*/
#define PIXEL_RATE_WITH_360M_10BIT_2L	(SC485SL_LINK_FREQ_360 * 2 * \
					2 / SC485SL_BITS_PER_SAMPLE)

/* 1440Mbps pre lane */
#define PIXEL_RATE_WITH_720M_10BIT_2L	(SC485SL_LINK_FREQ_720 * 2 / \
					SC485SL_BITS_PER_SAMPLE * 2)

/* 4 lane */
/* 720Mbps pre lane*/
#define PIXEL_RATE_WITH_396M_10BIT_4L	(SC485SL_LINK_FREQ_396 * 2 / \
					SC485SL_BITS_PER_SAMPLE * 4)
/* 1080Mbps pre lane */
#define PIXEL_RATE_WITH_540M_10BIT_4L	(SC485SL_LINK_FREQ_540 * 2 / \
					SC485SL_BITS_PER_SAMPLE * 4)

#define SC485SL_XVCLK_FREQ		27000000

#define CHIP_ID				0xbd82
#define SC485SL_REG_CHIP_ID		0x3107

#define SC485SL_REG_MIPI_CTRL		0x3019
#define SC485SL_MIPI_CTRL_ON		0x00
#define SC485SL_MIPI_CTRL_OFF		0xff

#define SC485SL_REG_CTRL_MODE		0x0100
#define SC485SL_MODE_SW_STANDBY		0x0
#define SC485SL_MODE_STREAMING		BIT(0)

#define SC485SL_REG_EXPOSURE_H		0x3e00
#define SC485SL_REG_EXPOSURE_M		0x3e01
#define SC485SL_REG_EXPOSURE_L		0x3e02
#define SC485SL_REG_SEXPOSURE_H		0x3e22
#define SC485SL_REG_SEXPOSURE_M		0x3e04
#define SC485SL_REG_SEXPOSURE_L		0x3e05

#define	SC485SL_EXPOSURE_MIN		1
#define	SC485SL_EXPOSURE_STEP		1
#define SC485SL_VTS_MAX			0x3ffff0

#define SC485SL_REG_DIG_GAIN		0x3e06
#define SC485SL_REG_DIG_FINE_GAIN	0x3e07
#define SC485SL_REG_ANA_GAIN		0x3e08
#define SC485SL_REG_ANA_FINE_GAIN	0x3e09
#define SC485SL_REG_SDIG_GAIN		0x3e10
#define SC485SL_REG_SDIG_FINE_GAIN	0x3e11
#define SC485SL_REG_SANA_GAIN		0x3e12
#define SC485SL_REG_SANA_FINE_GAIN	0x3e13
#define SC485SL_GAIN_MIN		0x0040
#define SC485SL_GAIN_MAX		109133 // 107.415*15.875*64 = 109133
#define SC485SL_GAIN_STEP		1
#define SC485SL_GAIN_DEFAULT		0x0040
#define SC485SL_LGAIN			0
#define SC485SL_SGAIN			1

#define SC485SL_REG_GROUP_HOLD		0x3812
#define SC485SL_GROUP_HOLD_START	0x00 // start hold
#define SC485SL_GROUP_HOLD_END		0x30 // release hold
#define SC485SL_REG_HOLD_DELAY		0x3802 //effective after group hold

/* led strobe mode 1*/
#define SC485SL_REG_LED_STROBE_EN_M1		0x3362 // 0x00: auto mode; 0x01: manuale mode;
#define SC485SL_REG_LED_STROBE_OUTPUT_PIN0_M1	0x300a // [2:1, 6], use fsync as output single pin
#define SC485SL_REG_LED_STROBE_OUTPUT_PIN1_M1	0x3033 // [1]
#define SC485SL_REG_LED_STROBE_OUTPUT_PIN2_M1	0x3035 // 0x00
#define SC485SL_REG_LED_STROBE_PUSLE_START_H	0x3382 // start at {16’h320e,16’h320f} – 1 –{16’h3382,16’h3383}
#define SC485SL_REG_LED_STROBE_PUSLE_START_L	0x3383
#define SC485SL_REG_LED_STROBE_PUSLE_END_H	0x3386 // end at {16’h320e,16’h320f} – 1 –{16’h3386,16’h3387}
#define SC485SL_REG_LED_STROBE_PUSLE_END_L	0x3387
/* led strobe mode 2 */
#define SC485SL_REG_LED_STROBE_EN_M2		0x4d0b	// 0x00: disable; 0x01: enable
#define SC485SL_REG_LED_STROBE_OUTPUT_PIN0_M2	0x300a // [2:1], use fsync as output single pin
#define SC485SL_REG_LED_STROBE_OUTPUT_PIN1_M2	0x3033 // [1]
#define SC485SL_REG_LED_STROBE_OUTPUT_PIN2_M2	0x3035 // 0x00
#define SC485SL_REG_LED_STROBE_PUSLE_WIDTH_H	0x4d0c // use {16’h320c,16’h320d} as unit
#define SC485SL_REG_LED_STROBE_PUSLE_WIDTH_L	0x4d0d

#define SC485SL_REG_TEST_PATTERN	0x4501
#define SC485SL_TEST_PATTERN_BIT_MASK	BIT(3)	// 0 -normal image; 1 - increasing gradient pattern

/* max frame length 0x3ffff0 */
#define SC485SL_REG_VTS_H		0x326d	// [5:0]
#define SC485SL_REG_VTS_M		0x320e
#define SC485SL_REG_VTS_L		0x320f

#define SC485SL_FLIP_MIRROR_REG		0x3221

#define SC485SL_FETCH_EXP_H(VAL)	(((VAL) >> 12) & 0xF)
#define SC485SL_FETCH_EXP_M(VAL)	(((VAL) >> 4) & 0xFF)
#define SC485SL_FETCH_EXP_L(VAL)	(((VAL) & 0xF) << 4)

#define SC485SL_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL | 0x06 : VAL & 0xf9)
#define SC485SL_FETCH_FLIP(VAL, ENABLE)		(ENABLE ? VAL | 0x60 : VAL & 0x9f)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define SC485SL_REG_VALUE_08BIT		1
#define SC485SL_REG_VALUE_16BIT		2
#define SC485SL_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define SC485SL_NAME			"sc485sl"

static const char *const sc485sl_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define SC485SL_NUM_SUPPLIES ARRAY_SIZE(sc485sl_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct sc485sl_mode {
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

struct sc485sl {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[SC485SL_NUM_SUPPLIES];

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
	const struct sc485sl_mode *supported_modes;
	const struct sc485sl_mode *cur_mode;
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
};

#define to_sc485sl(sd) container_of(sd, struct sc485sl, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval sc485sl_global_4lane_regs[] = {
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 720Mbps, 4lane
 * linear: 2688x1520
 * Cleaned_0x02_SC485SL_raw_MIPI_27Minput_4Lane_10bit_792Mbps_2688x1520_30fps_20250418.ini
 */
static const struct regval sc485sl_linear_10_2688x1520_30fps_4lane_regs[] = {
	{0x0103, 0x01},
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
	{0x23bc, 0x04},
	{0x23bd, 0x08},
	{0x23be, 0x04},
	{0x23bf, 0x78},
	{0x23c0, 0x04},
	{0x23c1, 0x00},
	{0x23c2, 0x04},
	{0x23c3, 0x18},
	{0x23c4, 0x04},
	{0x23c5, 0x38},
	{0x23c6, 0x04},
	{0x23c7, 0x08},
	{0x23c8, 0x04},
	{0x23c9, 0x78},
	{0x3018, 0x7a},
	{0x301e, 0xf0},
	{0x301f, 0x02},
	{0x302c, 0x00},
	{0x3032, 0x2c},
	{0x3034, 0xee},
	{0x30b0, 0x01},
	{0x3204, 0x0a},
	{0x3205, 0x8f},
	{0x3206, 0x05},
	{0x3207, 0xff},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x02},
	{0x320d, 0xee},
	{0x320e, 0x0c},
	{0x320f, 0x80},
	{0x3211, 0x08},
	{0x3213, 0x08},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3250, 0x00},
	{0x325f, 0x2d},
	{0x32d1, 0x1d},
	{0x32e0, 0x00},
	{0x3301, 0x0a},
	{0x3304, 0x50},
	{0x3305, 0x00},
	{0x3306, 0x58},
	{0x3308, 0x10},
	{0x3309, 0x70},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330d, 0x08},
	{0x330e, 0x18},
	{0x330f, 0x04},
	{0x3310, 0x04},
	{0x3314, 0x94},
	{0x331e, 0x39},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3347, 0x0f},
	{0x334c, 0x08},
	{0x3364, 0x5e},
	{0x3393, 0x0e},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x0a},
	{0x339a, 0x0c},
	{0x339b, 0x10},
	{0x339c, 0x52},
	{0x33ac, 0x10},
	{0x33ad, 0x1c},
	{0x33ae, 0x40},
	{0x33af, 0x60},
	{0x33b0, 0x0f},
	{0x33b2, 0x32},
	{0x33b3, 0x08},
	{0x33f8, 0x00},
	{0x33f9, 0x58},
	{0x33fa, 0x00},
	{0x33fb, 0x58},
	{0x349f, 0x03},
	{0x34a8, 0x08},
	{0x34a9, 0x00},
	{0x34aa, 0x00},
	{0x34ab, 0xb8},
	{0x34ac, 0x00},
	{0x34ad, 0xb8},
	{0x34f9, 0x00},
	{0x3632, 0x6d},
	{0x3633, 0x41},
	{0x363a, 0x8c},
	{0x363b, 0x5d},
	{0x3670, 0x41},
	{0x3671, 0x42},
	{0x3672, 0x33},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x69},
	{0x367f, 0x6d},
	{0x3680, 0x6d},
	{0x3681, 0x04},
	{0x3682, 0x00},
	{0x3683, 0x04},
	{0x3684, 0x38},
	{0x3685, 0x81},
	{0x3686, 0x84},
	{0x3687, 0x84},
	{0x3688, 0x84},
	{0x3689, 0x83},
	{0x368a, 0x83},
	{0x368b, 0x85},
	{0x368c, 0x88},
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
	{0x36b9, 0x1b},
	{0x36ba, 0x1b},
	{0x36bb, 0x13},
	{0x36bc, 0x04},
	{0x36bd, 0x08},
	{0x36be, 0x04},
	{0x36bf, 0x38},
	{0x36d0, 0x0d},
	{0x36d1, 0x20},
	{0x36ea, 0x16},
	{0x36eb, 0x45},
	{0x36ec, 0x69},
	{0x36ed, 0x98},
	{0x370f, 0x31},
	{0x3722, 0x03},
	{0x3724, 0xc1},
	{0x3727, 0x24},
	{0x3729, 0x84},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0xf3},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x78},
	{0x37b7, 0xa4},
	{0x37b8, 0xf4},
	{0x37b9, 0xb4},
	{0x37ba, 0x08},
	{0x37bb, 0x1c},
	{0x37bc, 0x4e},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x18},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x18},
	{0x37fa, 0x10},
	{0x37fb, 0x55},
	{0x37fc, 0x18},
	{0x37fd, 0x1a},
	{0x3900, 0x05},
	{0x3901, 0x02},
	{0x3903, 0x60},
	{0x3907, 0x01},
	{0x3908, 0x00},
	{0x391a, 0x70},
	{0x391b, 0x46},
	{0x391c, 0x26},
	{0x391d, 0x00},
	{0x391f, 0x61},
	{0x3926, 0xe2},
	{0x3933, 0x80},
	{0x3934, 0x07},
	{0x3935, 0x01},
	{0x3936, 0x00},
	{0x3937, 0x7b},
	{0x3938, 0x7d},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x393b, 0x00},
	{0x393c, 0x00},
	{0x393f, 0x80},
	{0x3940, 0x20},
	{0x3941, 0x00},
	{0x3942, 0x20},
	{0x3943, 0x80},
	{0x3944, 0x80},
	{0x3945, 0x7f},
	{0x3946, 0x80},
	{0x39c9, 0x60},
	{0x39dd, 0x00},
	{0x39de, 0x08},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0xc7},
	{0x3e02, 0x80},
	{0x3e03, 0x0b},
	{0x3e08, 0x00},
	{0x3e16, 0x01},
	{0x3e17, 0xe2},
	{0x3e18, 0x01},
	{0x3e19, 0xe2},
	{0x4402, 0x02},
	{0x4403, 0x08},
	{0x4404, 0x16},
	{0x4405, 0x1d},
	{0x440c, 0x24},
	{0x440d, 0x24},
	{0x440e, 0x1b},
	{0x440f, 0x2d},
	{0x4412, 0x01},
	{0x4424, 0x01},
	{0x450d, 0x08},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x4837, 0x28},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x0c},
	{0x5788, 0x0c},
	{0x5789, 0x0c},
	{0x578a, 0x0c},
	{0x578b, 0x0c},
	{0x578c, 0x0c},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x36e9, 0x44},
	{0x37f9, 0x24},
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 1080Mbps, 4lane
 * hdr2: 2688x1520
 * Cleaned_0x0b_SC485SL_raw_MIPI_27Minput_4Lane_10bit_1080Mbps_2688x1520_30fps_SHDR_VC_20250418.ini
 */
static const struct regval sc485sl_hdr2_10_2688x1520_30fps_4lane_regs[] = {
	{0x0103, 0x01},
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
	{0x23bc, 0x04},
	{0x23bd, 0x08},
	{0x23be, 0x04},
	{0x23bf, 0x78},
	{0x23c0, 0x04},
	{0x23c1, 0x00},
	{0x23c2, 0x04},
	{0x23c3, 0x18},
	{0x23c4, 0x04},
	{0x23c5, 0x38},
	{0x23c6, 0x04},
	{0x23c7, 0x08},
	{0x23c8, 0x04},
	{0x23c9, 0x78},
	{0x3018, 0x7a},
	{0x301e, 0xf0},
	{0x301f, 0x0b},
	{0x302c, 0x00},
	{0x3032, 0x2c},
	{0x3034, 0xee},
	{0x30b0, 0x01},
	{0x3204, 0x0a},
	{0x3205, 0x8f},
	{0x3206, 0x05},
	{0x3207, 0xff},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x02},
	{0x320d, 0xee},
	{0x320e, 0x0c},
	{0x320f, 0x80},
	{0x3211, 0x08},
	{0x3213, 0x08},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3250, 0xff},
	{0x325f, 0x2d},
	{0x3281, 0x01},
	{0x32d1, 0x1d},
	{0x32e0, 0x00},
	{0x3301, 0x0a},
	{0x3304, 0x50},
	{0x3305, 0x00},
	{0x3306, 0x58},
	{0x3308, 0x10},
	{0x3309, 0x70},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330d, 0x08},
	{0x330e, 0x18},
	{0x330f, 0x04},
	{0x3310, 0x04},
	{0x3314, 0x94},
	{0x331e, 0x39},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3347, 0x0f},
	{0x334c, 0x08},
	{0x3364, 0x5e},
	{0x3393, 0x0e},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x0a},
	{0x339a, 0x0c},
	{0x339b, 0x10},
	{0x339c, 0x52},
	{0x33ac, 0x10},
	{0x33ad, 0x1c},
	{0x33ae, 0x40},
	{0x33af, 0x60},
	{0x33b0, 0x0f},
	{0x33b2, 0x32},
	{0x33b3, 0x08},
	{0x33f8, 0x00},
	{0x33f9, 0x58},
	{0x33fa, 0x00},
	{0x33fb, 0x58},
	{0x3432, 0x72},
	{0x349f, 0x03},
	{0x34a8, 0x08},
	{0x34a9, 0x00},
	{0x34aa, 0x00},
	{0x34ab, 0xb8},
	{0x34ac, 0x00},
	{0x34ad, 0xb8},
	{0x34f9, 0x00},
	{0x3632, 0x6d},
	{0x3633, 0x41},
	{0x363a, 0x8c},
	{0x363b, 0x5d},
	{0x3670, 0x41},
	{0x3671, 0x42},
	{0x3672, 0x33},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x69},
	{0x367f, 0x6d},
	{0x3680, 0x6d},
	{0x3681, 0x04},
	{0x3682, 0x00},
	{0x3683, 0x04},
	{0x3684, 0x38},
	{0x3685, 0x81},
	{0x3686, 0x84},
	{0x3687, 0x84},
	{0x3688, 0x84},
	{0x3689, 0x83},
	{0x368a, 0x83},
	{0x368b, 0x85},
	{0x368c, 0x88},
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
	{0x36b9, 0x1b},
	{0x36ba, 0x1b},
	{0x36bb, 0x13},
	{0x36bc, 0x04},
	{0x36bd, 0x08},
	{0x36be, 0x04},
	{0x36bf, 0x38},
	{0x36d0, 0x0d},
	{0x36d1, 0x20},
	{0x36ea, 0x14},
	{0x36eb, 0x45},
	{0x36ec, 0x69},
	{0x36ed, 0x98},
	{0x370f, 0x31},
	{0x3722, 0x03},
	{0x3724, 0xc1},
	{0x3727, 0x24},
	{0x3729, 0x84},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0xf3},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x78},
	{0x37b7, 0xa4},
	{0x37b8, 0xf4},
	{0x37b9, 0xb4},
	{0x37ba, 0x08},
	{0x37bb, 0x1c},
	{0x37bc, 0x4e},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x18},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x18},
	{0x37fa, 0x10},
	{0x37fb, 0x55},
	{0x37fc, 0x18},
	{0x37fd, 0x1a},
	{0x3900, 0x05},
	{0x3901, 0x02},
	{0x3903, 0x60},
	{0x3907, 0x01},
	{0x3908, 0x00},
	{0x391a, 0x70},
	{0x391b, 0x46},
	{0x391c, 0x26},
	{0x391d, 0x00},
	{0x391f, 0x61},
	{0x3926, 0xe2},
	{0x3933, 0x80},
	{0x3934, 0x07},
	{0x3935, 0x01},
	{0x3936, 0x00},
	{0x3937, 0x7b},
	{0x3938, 0x7d},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x393b, 0x00},
	{0x393c, 0x00},
	{0x393f, 0x00},
	{0x3940, 0x20},
	{0x3941, 0x00},
	{0x3942, 0x20},
	{0x3943, 0x80},
	{0x3944, 0x80},
	{0x3945, 0x7f},
	{0x3946, 0x80},
	{0x39c9, 0x60},
	{0x39dd, 0x00},
	{0x39de, 0x08},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0xb9},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e04, 0x0b},
	{0x3e05, 0x90},
	{0x3e08, 0x00},
	{0x3e16, 0x01},
	{0x3e17, 0xe2},
	{0x3e18, 0x01},
	{0x3e19, 0xe2},
	{0x3e23, 0x00},
	{0x3e24, 0xc4},
	{0x4402, 0x02},
	{0x4403, 0x08},
	{0x4404, 0x16},
	{0x4405, 0x1d},
	{0x440c, 0x24},
	{0x440d, 0x24},
	{0x440e, 0x1b},
	{0x440f, 0x2d},
	{0x4412, 0x01},
	{0x4424, 0x01},
	{0x450d, 0x08},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x481f, 0x3c},
	{0x4827, 0x38},
	{0x4837, 0x1e},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x0c},
	{0x5788, 0x0c},
	{0x5789, 0x0c},
	{0x578a, 0x0c},
	{0x578b, 0x0c},
	{0x578c, 0x0c},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x36e9, 0x24},
	{0x37f9, 0x24},
	{REG_NULL, 0x00},
};

static const struct regval sc485sl_global_2lane_regs[] = {
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 720Mbps, 2lane
 * linear: 2688x1520
 * Cleaned_0x0c_SC485SL_raw_MIPI_27Minput_2Lane_10bit_720Mbps_2688x1520_30fps_20250418.ini
 */
static const struct regval sc485sl_linear_10_2688x1520_30fps_2lane_regs[] = {
	{0x0103, 0x01},
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
	{0x23bc, 0x04},
	{0x23bd, 0x08},
	{0x23be, 0x04},
	{0x23bf, 0x78},
	{0x23c0, 0x04},
	{0x23c1, 0x00},
	{0x23c2, 0x04},
	{0x23c3, 0x18},
	{0x23c4, 0x04},
	{0x23c5, 0x38},
	{0x23c6, 0x04},
	{0x23c7, 0x08},
	{0x23c8, 0x04},
	{0x23c9, 0x78},
	{0x3018, 0x3a},
	{0x3019, 0x0c},
	{0x301e, 0xf0},
	{0x301f, 0x0c},
	{0x302c, 0x00},
	{0x3032, 0x2c},
	{0x3034, 0xee},
	{0x30b0, 0x01},
	{0x3204, 0x0a},
	{0x3205, 0x8f},
	{0x3206, 0x05},
	{0x3207, 0xff},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x02},
	{0x320d, 0xee},
	{0x320e, 0x06},
	{0x320f, 0x40},
	{0x3211, 0x08},
	{0x3213, 0x08},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3250, 0x00},
	{0x325f, 0x2d},
	{0x32d1, 0x1d},
	{0x32e0, 0x00},
	{0x3301, 0x0a},
	{0x3304, 0x50},
	{0x3305, 0x00},
	{0x3306, 0x58},
	{0x3308, 0x10},
	{0x3309, 0x70},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330d, 0x08},
	{0x330e, 0x18},
	{0x330f, 0x04},
	{0x3310, 0x04},
	{0x3314, 0x94},
	{0x331e, 0x39},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3347, 0x0f},
	{0x334c, 0x08},
	{0x3364, 0x5e},
	{0x3393, 0x0e},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x0a},
	{0x339a, 0x0c},
	{0x339b, 0x10},
	{0x339c, 0x52},
	{0x33ac, 0x10},
	{0x33ad, 0x1c},
	{0x33ae, 0x40},
	{0x33af, 0x60},
	{0x33b0, 0x0f},
	{0x33b2, 0x32},
	{0x33b3, 0x08},
	{0x33f8, 0x00},
	{0x33f9, 0x58},
	{0x33fa, 0x00},
	{0x33fb, 0x58},
	{0x349f, 0x03},
	{0x34a8, 0x08},
	{0x34a9, 0x00},
	{0x34aa, 0x00},
	{0x34ab, 0xb8},
	{0x34ac, 0x00},
	{0x34ad, 0xb8},
	{0x34f9, 0x00},
	{0x3632, 0x6d},
	{0x3633, 0x41},
	{0x363a, 0x8c},
	{0x363b, 0x5d},
	{0x3670, 0x41},
	{0x3671, 0x42},
	{0x3672, 0x33},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x69},
	{0x367f, 0x6d},
	{0x3680, 0x6d},
	{0x3681, 0x04},
	{0x3682, 0x00},
	{0x3683, 0x04},
	{0x3684, 0x38},
	{0x3685, 0x81},
	{0x3686, 0x84},
	{0x3687, 0x84},
	{0x3688, 0x84},
	{0x3689, 0x83},
	{0x368a, 0x83},
	{0x368b, 0x85},
	{0x368c, 0x88},
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
	{0x36b9, 0x1b},
	{0x36ba, 0x1b},
	{0x36bb, 0x13},
	{0x36bc, 0x04},
	{0x36bd, 0x08},
	{0x36be, 0x04},
	{0x36bf, 0x38},
	{0x36d0, 0x0d},
	{0x36d1, 0x20},
	{0x36ea, 0x14},
	{0x36eb, 0x4f},
	{0x36ec, 0x69},
	{0x36ed, 0x98},
	{0x370f, 0x31},
	{0x3722, 0x03},
	{0x3724, 0xc1},
	{0x3727, 0x24},
	{0x3729, 0x84},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0xf3},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x78},
	{0x37b7, 0xa4},
	{0x37b8, 0xf4},
	{0x37b9, 0xb4},
	{0x37ba, 0x08},
	{0x37bb, 0x1c},
	{0x37bc, 0x4e},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x18},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x18},
	{0x37fa, 0x0c},
	{0x37fb, 0x55},
	{0x37fc, 0x18},
	{0x37fd, 0x1a},
	{0x3900, 0x05},
	{0x3901, 0x02},
	{0x3903, 0x60},
	{0x3907, 0x01},
	{0x3908, 0x00},
	{0x391a, 0x70},
	{0x391b, 0x46},
	{0x391c, 0x26},
	{0x391d, 0x00},
	{0x391f, 0x61},
	{0x3926, 0xe2},
	{0x3933, 0x80},
	{0x3934, 0x07},
	{0x3935, 0x01},
	{0x3936, 0x00},
	{0x3937, 0x7b},
	{0x3938, 0x7d},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x393b, 0x00},
	{0x393c, 0x00},
	{0x393f, 0x80},
	{0x3940, 0x20},
	{0x3941, 0x00},
	{0x3942, 0x20},
	{0x3943, 0x80},
	{0x3944, 0x80},
	{0x3945, 0x7f},
	{0x3946, 0x80},
	{0x39c9, 0x60},
	{0x39dd, 0x00},
	{0x39de, 0x08},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0x63},
	{0x3e02, 0x80},
	{0x3e03, 0x0b},
	{0x3e08, 0x00},
	{0x3e16, 0x01},
	{0x3e17, 0xe2},
	{0x3e18, 0x01},
	{0x3e19, 0xe2},
	{0x4402, 0x01},
	{0x4403, 0x04},
	{0x4404, 0x0b},
	{0x4405, 0x0f},
	{0x440c, 0x12},
	{0x440d, 0x12},
	{0x440e, 0x0e},
	{0x440f, 0x17},
	{0x4412, 0x01},
	{0x4424, 0x01},
	{0x450d, 0x08},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x4837, 0x2c},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x0c},
	{0x5788, 0x0c},
	{0x5789, 0x0c},
	{0x578a, 0x0c},
	{0x578b, 0x0c},
	{0x578c, 0x0c},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
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
 * max_framerate 60fps
 * mipi_datarate per lane 1440Mbps, 2lane
 * linear: 2688x1520
 * Cleaned_0x29_SC485SL_raw_MIPI_27Minput_2Lane_10bit_1440Mbps_2688x1520_60fps_20250310.ini
 */
static const struct regval sc485sl_linear_10_2688x1520_60fps_2lane_regs[] = {
	{0x0103, 0x01},
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
	{0x23bc, 0x04},
	{0x23bd, 0x08},
	{0x23be, 0x04},
	{0x23bf, 0x78},
	{0x23c0, 0x04},
	{0x23c1, 0x00},
	{0x23c2, 0x04},
	{0x23c3, 0x18},
	{0x23c4, 0x04},
	{0x23c5, 0x38},
	{0x23c6, 0x04},
	{0x23c7, 0x08},
	{0x23c8, 0x04},
	{0x23c9, 0x78},
	{0x3018, 0x3a},
	{0x3019, 0x0c},
	{0x301e, 0xf0},
	{0x301f, 0x29},
	{0x302c, 0x00},
	{0x3032, 0x2c},
	{0x3034, 0xee},
	{0x30b0, 0x01},
	{0x3204, 0x0a},
	{0x3205, 0x8f},
	{0x3206, 0x05},
	{0x3207, 0xff},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x02},
	{0x320d, 0xee},
	{0x320e, 0x06},
	{0x320f, 0x40},
	{0x3211, 0x08},
	{0x3213, 0x08},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3250, 0x00},
	{0x325f, 0x2d},
	{0x32d1, 0x1d},
	{0x32e0, 0x00},
	{0x3301, 0x0a},
	{0x3304, 0x50},
	{0x3305, 0x00},
	{0x3306, 0x58},
	{0x3308, 0x10},
	{0x3309, 0x70},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330d, 0x08},
	{0x330e, 0x18},
	{0x330f, 0x04},
	{0x3310, 0x04},
	{0x3314, 0x94},
	{0x331e, 0x39},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3347, 0x0f},
	{0x334c, 0x08},
	{0x3364, 0x5e},
	{0x3393, 0x0e},
	{0x3394, 0x2c},
	{0x3395, 0x3c},
	{0x3399, 0x0a},
	{0x339a, 0x0c},
	{0x339b, 0x10},
	{0x339c, 0x52},
	{0x33ac, 0x10},
	{0x33ad, 0x1c},
	{0x33ae, 0x40},
	{0x33af, 0x60},
	{0x33b0, 0x0f},
	{0x33b2, 0x32},
	{0x33b3, 0x08},
	{0x33f8, 0x00},
	{0x33f9, 0x58},
	{0x33fa, 0x00},
	{0x33fb, 0x58},
	{0x349f, 0x03},
	{0x34a8, 0x08},
	{0x34a9, 0x00},
	{0x34aa, 0x00},
	{0x34ab, 0xb8},
	{0x34ac, 0x00},
	{0x34ad, 0xb8},
	{0x34f9, 0x00},
	{0x3632, 0x6d},
	{0x3633, 0x41},
	{0x363a, 0x8c},
	{0x363b, 0x5d},
	{0x3670, 0x41},
	{0x3671, 0x42},
	{0x3672, 0x33},
	{0x3673, 0x04},
	{0x3674, 0x08},
	{0x3675, 0x04},
	{0x3676, 0x18},
	{0x367e, 0x69},
	{0x367f, 0x6d},
	{0x3680, 0x6d},
	{0x3681, 0x04},
	{0x3682, 0x00},
	{0x3683, 0x04},
	{0x3684, 0x38},
	{0x3685, 0x81},
	{0x3686, 0x84},
	{0x3687, 0x84},
	{0x3688, 0x84},
	{0x3689, 0x83},
	{0x368a, 0x83},
	{0x368b, 0x85},
	{0x368c, 0x88},
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
	{0x36b9, 0x1b},
	{0x36ba, 0x1b},
	{0x36bb, 0x13},
	{0x36bc, 0x04},
	{0x36bd, 0x08},
	{0x36be, 0x04},
	{0x36bf, 0x38},
	{0x36d0, 0x0d},
	{0x36d1, 0x20},
	{0x36ea, 0x10},
	{0x36eb, 0x45},
	{0x36ec, 0x69},
	{0x36ed, 0xa8},
	{0x370f, 0x31},
	{0x3722, 0x03},
	{0x3724, 0xc1},
	{0x3727, 0x24},
	{0x3729, 0x84},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0xf3},
	{0x37b3, 0x04},
	{0x37b4, 0x08},
	{0x37b5, 0x04},
	{0x37b6, 0x78},
	{0x37b7, 0xa4},
	{0x37b8, 0xf4},
	{0x37b9, 0xb4},
	{0x37ba, 0x08},
	{0x37bb, 0x1c},
	{0x37bc, 0x4e},
	{0x37bd, 0x04},
	{0x37be, 0x08},
	{0x37bf, 0x04},
	{0x37c0, 0x18},
	{0x37c1, 0x04},
	{0x37c2, 0x08},
	{0x37c3, 0x04},
	{0x37c4, 0x18},
	{0x37fa, 0x10},
	{0x37fb, 0x55},
	{0x37fc, 0x18},
	{0x37fd, 0x1a},
	{0x3900, 0x05},
	{0x3901, 0x02},
	{0x3903, 0x60},
	{0x3907, 0x01},
	{0x3908, 0x00},
	{0x391a, 0x70},
	{0x391b, 0x46},
	{0x391c, 0x26},
	{0x391d, 0x00},
	{0x391f, 0x61},
	{0x3926, 0xe2},
	{0x3933, 0x80},
	{0x3934, 0x07},
	{0x3935, 0x01},
	{0x3936, 0x00},
	{0x3937, 0x7b},
	{0x3938, 0x7d},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x393b, 0x00},
	{0x393c, 0x00},
	{0x393f, 0x80},
	{0x3940, 0x20},
	{0x3941, 0x00},
	{0x3942, 0x20},
	{0x3943, 0x80},
	{0x3944, 0x80},
	{0x3945, 0x7f},
	{0x3946, 0x80},
	{0x39c9, 0x60},
	{0x39dd, 0x00},
	{0x39de, 0x08},
	{0x39e7, 0x04},
	{0x39e8, 0x04},
	{0x39e9, 0x80},
	{0x3e00, 0x00},
	{0x3e01, 0x63},
	{0x3e02, 0x80},
	{0x3e03, 0x0b},
	{0x3e08, 0x00},
	{0x3e16, 0x01},
	{0x3e17, 0xe2},
	{0x3e18, 0x01},
	{0x3e19, 0xe2},
	{0x4402, 0x02},
	{0x4403, 0x08},
	{0x4404, 0x16},
	{0x4405, 0x1d},
	{0x440c, 0x24},
	{0x440d, 0x24},
	{0x440e, 0x1b},
	{0x440f, 0x2d},
	{0x4412, 0x01},
	{0x4424, 0x01},
	{0x450d, 0x08},
	{0x4800, 0x24},
	{0x480f, 0x03},
	{0x4837, 0x16},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x0c},
	{0x5788, 0x0c},
	{0x5789, 0x0c},
	{0x578a, 0x0c},
	{0x578b, 0x0c},
	{0x578c, 0x0c},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x36e9, 0x44},
	{0x37f9, 0x24},
	// {0x0100, 0x01},
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

static const struct sc485sl_mode supported_modes_4lane[] = {
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0c78,
		.hts_def = 0x2ee * 4,
		.vts_def = 0x0c80,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc485sl_global_4lane_regs,
		.reg_list = sc485sl_linear_10_2688x1520_30fps_4lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 0,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 4,
	},
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0c78,
		.hts_def = 0x2ee * 4,
		.vts_def = 0x0c80,

		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc485sl_global_4lane_regs,
		.reg_list = sc485sl_hdr2_10_2688x1520_30fps_4lane_regs,
		.hdr_mode = HDR_X2,
		.mclk = 27000000,
		.link_freq_idx = 2,
		.bpp = 10,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,//L->csi wr0
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,//M->csi wr2
		.lanes = 4,
	}
};

static const struct sc485sl_mode supported_modes_2lane[] = {
	/* 30fps 2lane */
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0638,
		.hts_def = 0x2ee * 4,
		.vts_def = 0x0640,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc485sl_global_2lane_regs,
		.reg_list = sc485sl_linear_10_2688x1520_30fps_2lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 1,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 2,
	},
	/* 60fps 2lane */
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.exp_def = 0x0638,
		.hts_def = 0x2ee * 4,
		.vts_def = 0x0640,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc485sl_global_2lane_regs,
		.reg_list = sc485sl_linear_10_2688x1520_60fps_2lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 3,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 2,
	},
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,
};

static const s64 link_freq_menu_items[] = {
	SC485SL_LINK_FREQ_396,	/* 4 lanes */
	SC485SL_LINK_FREQ_360,	/* 2 lanes */
	SC485SL_LINK_FREQ_540,	/* 4 lanes */
	SC485SL_LINK_FREQ_720,	/* 2 lanes */
};

static const char *const sc485sl_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4",
};


static int sc485sl_write_reg(struct i2c_client *client, u16 reg,
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

static int sc485sl_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = sc485sl_write_reg(client, regs[i].addr,
					SC485SL_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

static int sc485sl_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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
static int sc485sl_set_gain_reg(struct sc485sl *sc485sl, u32 gain, int mode)
{
	struct i2c_client *client = sc485sl->client;
	u32 coarse_again = 0, coarse_dgain = 0, fine_again = 0, fine_dgain = 0;
	int ret = 0, gain_factor;

	if (gain <= 64)
		gain = 64;
	else if (gain > SC485SL_GAIN_MAX)
		gain = SC485SL_GAIN_MAX;

	gain_factor = gain * 1000 / 64;
	if (gain_factor < 2000) {		/* start again,  1.0x - 2.0x, 1000 * 2 = 2000*/
		coarse_again = 0x00;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 1000);
	} else if (gain_factor < 3410) {	/* 2.0x - 3.41x, 1000 * 3.41 = 3410 */
		coarse_again = 0x01;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 2000);
	} else if (gain_factor < 6820) {	/* 3.41x - 6.82x, 1000 * 6.82 = 6820 */
		coarse_again = 0x80;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 3410);
	} else if (gain_factor < 13640) {	/* 6.82x - 13.64x, 1000 * 13.64 = 13640 */
		coarse_again = 0x81;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 6820);
	} else if (gain_factor < 27280) {	/* 13.64x - 27.28x, 1000 * 27.28 = 27280 */
		coarse_again = 0x83;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 13640);
	} else if (gain_factor < 54560) {	/* 27.28x - 54.56x, 1000 * 54.56 = 54560 */
		coarse_again = 0x87;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 27280);
	} else if (gain_factor <= 107415) {	/* 54.56x - 107.415x, 1000 * 107.415 = 107415 */
		coarse_again = 0x8f;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = DIV_ROUND_UP(gain_factor * 32, 54560);
	} else if (gain_factor < 107415 * 2) {
		// open dgain begin,  max digital gain 15.875x,
		// the accuracy of the digital fractional gain is 1/64.
		coarse_again = 0x8f;
		coarse_dgain = 0x00;
		fine_again = 0x3f;
		fine_dgain = DIV_ROUND_UP(gain_factor * 128, 107415);
	} else if (gain_factor < 107415 * 4) {
		coarse_again = 0x8f;
		coarse_dgain = 0x01;
		fine_again = 0x3f;
		fine_dgain = DIV_ROUND_UP(gain_factor * 128, 107415 * 2);
	} else if (gain_factor < 107415 * 8) {
		coarse_again = 0x8f;
		coarse_dgain = 0x03;
		fine_again = 0x3f;
		fine_dgain = DIV_ROUND_UP(gain_factor * 128, 107415 * 4);
	} else if (gain_factor < 107415 * 16) {
		coarse_again = 0x8f;
		coarse_dgain = 0x07;
		fine_again = 0x3f;
		fine_dgain = DIV_ROUND_UP(gain_factor * 128, 107415 * 8);
	}
	dev_dbg(&client->dev, "c_again: 0x%x, c_dgain: 0x%x, f_again: 0x%x, f_dgain: 0x%0x\n",
		coarse_again, coarse_dgain, fine_again, fine_dgain);

	if (mode == SC485SL_LGAIN) {
		ret = sc485sl_write_reg(sc485sl->client,
					SC485SL_REG_DIG_GAIN,
					SC485SL_REG_VALUE_08BIT,
					coarse_dgain);
		ret |= sc485sl_write_reg(sc485sl->client,
					 SC485SL_REG_DIG_FINE_GAIN,
					 SC485SL_REG_VALUE_08BIT,
					 fine_dgain);
		ret |= sc485sl_write_reg(sc485sl->client,
					 SC485SL_REG_ANA_GAIN,
					 SC485SL_REG_VALUE_08BIT,
					 coarse_again);
		ret |= sc485sl_write_reg(sc485sl->client,
					 SC485SL_REG_ANA_FINE_GAIN,
					 SC485SL_REG_VALUE_08BIT,
					 fine_again);
	} else {
		ret = sc485sl_write_reg(sc485sl->client,
					SC485SL_REG_SDIG_GAIN,
					SC485SL_REG_VALUE_08BIT,
					coarse_dgain);
		ret |= sc485sl_write_reg(sc485sl->client,
					 SC485SL_REG_SDIG_FINE_GAIN,
					 SC485SL_REG_VALUE_08BIT,
					 fine_dgain);
		ret |= sc485sl_write_reg(sc485sl->client,
					 SC485SL_REG_SANA_GAIN,
					 SC485SL_REG_VALUE_08BIT,
					 coarse_again);
		ret |= sc485sl_write_reg(sc485sl->client,
					 SC485SL_REG_SANA_FINE_GAIN,
					 SC485SL_REG_VALUE_08BIT,
					 fine_again);
	}
	return ret;
}

static int sc485sl_set_hdrae(struct sc485sl *sc485sl,
			     struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	u32 l_exp_max = 0;

	if (!sc485sl->has_init_exp && !sc485sl->streaming) {
		sc485sl->init_hdrae_exp = *ae;
		sc485sl->has_init_exp = true;
		dev_dbg(&sc485sl->client->dev, "sc485sl don't stream, record exp for hdr!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&sc485sl->client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (sc485sl->cur_mode->hdr_mode == HDR_X2) {
		//2 stagger
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	// manual long exposure time in double-line overlap HDR mode,
	// register value is in units of one line
	// (3e23~3e24) default value is 0x00c4 from reg list
	l_exp_max = sc485sl->cur_vts - 196 - 14;	// vts - (3e33,3e23~3e24) - 14
	//set exposure
	l_exp_time = l_exp_time * 2;
	s_exp_time = s_exp_time * 2;
	if (l_exp_time > l_exp_max)
		l_exp_time = l_exp_max;

	// read regs list to get (3e23~3e24) value, then subtract 11
	// (3e23~3e24) default value is 0x00c4  from reg list
	if (s_exp_time > 184)	// 184 = (3e33,3e23~3e24) - 12
		s_exp_time = 184;

	ret = sc485sl_write_reg(sc485sl->client,
				SC485SL_REG_EXPOSURE_H,
				SC485SL_REG_VALUE_08BIT,
				SC485SL_FETCH_EXP_H(l_exp_time));
	ret |= sc485sl_write_reg(sc485sl->client,
				 SC485SL_REG_EXPOSURE_M,
				 SC485SL_REG_VALUE_08BIT,
				 SC485SL_FETCH_EXP_M(l_exp_time));
	ret |= sc485sl_write_reg(sc485sl->client,
				 SC485SL_REG_EXPOSURE_L,
				 SC485SL_REG_VALUE_08BIT,
				 SC485SL_FETCH_EXP_L(l_exp_time));
	ret |= sc485sl_write_reg(sc485sl->client,
				 SC485SL_REG_SEXPOSURE_M,
				 SC485SL_REG_VALUE_08BIT,
				 SC485SL_FETCH_EXP_M(s_exp_time));
	ret |= sc485sl_write_reg(sc485sl->client,
				 SC485SL_REG_SEXPOSURE_L,
				 SC485SL_REG_VALUE_08BIT,
				 SC485SL_FETCH_EXP_L(s_exp_time));

	ret |= sc485sl_set_gain_reg(sc485sl, l_a_gain, SC485SL_LGAIN);
	ret |= sc485sl_set_gain_reg(sc485sl, s_a_gain, SC485SL_SGAIN);
	return ret;
}

static int sc485sl_get_reso_dist(const struct sc485sl_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc485sl_mode *
sc485sl_find_best_fit(struct sc485sl *sc485sl, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < sc485sl->cfg_num; i++) {
		dist = sc485sl_get_reso_dist(&sc485sl->supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		} else if (dist == cur_best_fit_dist &&
			   framefmt->code == sc485sl->supported_modes[i].bus_fmt) {
			cur_best_fit = i;
			break;
		}
	}

	return &sc485sl->supported_modes[cur_best_fit];
}

static int sc485sl_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	const struct sc485sl_mode *mode;
	s64 h_blank, vblank_def;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc485sl->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&sc485sl->mutex);

	mode = sc485sl_find_best_fit(sc485sl, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&sc485sl->mutex);
		return -ENOTTY;
#endif
	} else {
		sc485sl->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc485sl->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc485sl->vblank, vblank_def,
					 SC485SL_VTS_MAX - mode->height,
					 1, vblank_def);
		dst_link_freq = mode->link_freq_idx;
		dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
				 mode->bpp * 2 * lanes;
		__v4l2_ctrl_s_ctrl_int64(sc485sl->pixel_rate,
					 dst_pixel_rate);
		__v4l2_ctrl_s_ctrl(sc485sl->link_freq,
				   dst_link_freq);
		sc485sl->cur_fps = mode->max_fps;
	}

	mutex_unlock(&sc485sl->mutex);

	return 0;
}

static int sc485sl_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	const struct sc485sl_mode *mode = sc485sl->cur_mode;

	mutex_lock(&sc485sl->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&sc485sl->mutex);
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
	mutex_unlock(&sc485sl->mutex);

	return 0;
}

static int sc485sl_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int sc485sl_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);

	if (fse->index >= sc485sl->cfg_num)
		return -EINVAL;

	if (fse->code != sc485sl->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = sc485sl->supported_modes[fse->index].width;
	fse->max_width  = sc485sl->supported_modes[fse->index].width;
	fse->min_height = sc485sl->supported_modes[fse->index].height;
	fse->max_height = sc485sl->supported_modes[fse->index].height;

	return 0;
}

static int sc485sl_enable_test_pattern(struct sc485sl *sc485sl, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc485sl_read_reg(sc485sl->client, SC485SL_REG_TEST_PATTERN,
			       SC485SL_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC485SL_TEST_PATTERN_BIT_MASK;
	else
		val &= ~SC485SL_TEST_PATTERN_BIT_MASK;

	ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_TEST_PATTERN,
				 SC485SL_REG_VALUE_08BIT, val);
	return ret;
}

static int sc485sl_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	const struct sc485sl_mode *mode = sc485sl->cur_mode;

	if (sc485sl->streaming)
		fi->interval = sc485sl->cur_fps;
	else
		fi->interval = mode->max_fps;
	return 0;
}

static const struct sc485sl_mode *sc485sl_find_mode(struct sc485sl *sc485sl, int fps)
{
	const struct sc485sl_mode *mode = NULL;
	const struct sc485sl_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < sc485sl->cfg_num; i++) {
		mode = &sc485sl->supported_modes[i];
		if (mode->width == sc485sl->cur_mode->width &&
		    mode->height == sc485sl->cur_mode->height &&
		    mode->hdr_mode == sc485sl->cur_mode->hdr_mode &&
		    mode->bus_fmt == sc485sl->cur_mode->bus_fmt) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator, mode->max_fps.numerator);
			if (cur_fps == fps) {
				match = mode;
				break;
			}
		}
	}
	return match;
}

static int sc485sl_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	const struct sc485sl_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	int fps;

	if (sc485sl->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = sc485sl_find_mode(sc485sl, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	sc485sl->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(sc485sl->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(sc485sl->vblank, vblank_def,
				 SC485SL_VTS_MAX - mode->height,
				 1, vblank_def);
	pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
		     mode->bpp * 2 * mode->lanes;

	__v4l2_ctrl_s_ctrl_int64(sc485sl->pixel_rate,
				 pixel_rate);
	__v4l2_ctrl_s_ctrl(sc485sl->link_freq,
			   mode->link_freq_idx);
	sc485sl->cur_fps = mode->max_fps;

	return 0;
}

static int sc485sl_g_mbus_config(struct v4l2_subdev *sd,
				 unsigned int pad_id,
				 struct v4l2_mbus_config *config)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	u8 lanes = sc485sl->bus_cfg.bus.mipi_csi2.num_data_lanes;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = lanes;

	return 0;
}

static void sc485sl_get_module_inf(struct sc485sl *sc485sl,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC485SL_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc485sl->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc485sl->len_name, sizeof(inf->base.lens));
}

static int sc485sl_set_setting(struct sc485sl *sc485sl, struct rk_sensor_setting *setting)
{
	int i = 0;
	int cur_fps = 0;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	const struct sc485sl_mode *mode = NULL;
	const struct sc485sl_mode *match = NULL;
	u8 lane = sc485sl->bus_cfg.bus.mipi_csi2.num_data_lanes;

	dev_info(&sc485sl->client->dev,
		 "sensor setting: %d x %d, fps:%d fmt:%d, mode:%d\n",
		 setting->width, setting->height,
		 setting->fps, setting->fmt, setting->mode);

	for (i = 0; i < sc485sl->cfg_num; i++) {
		mode = &sc485sl->supported_modes[i];
		if (mode->width == setting->width &&
		    mode->height == setting->height &&
		    mode->hdr_mode == setting->mode &&
		    mode->bus_fmt == setting->fmt) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator, mode->max_fps.numerator);
			if (cur_fps == setting->fps) {
				match = mode;
				break;
			}
		}
	}

	if (match) {
		dev_info(&sc485sl->client->dev, "-----%s: match the support mode, mode idx:%d-----\n",
			 __func__, i);
		sc485sl->cur_mode = mode;

		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc485sl->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc485sl->vblank, vblank_def,
					 SC485SL_VTS_MAX - mode->height,
					 1, vblank_def);


		__v4l2_ctrl_s_ctrl(sc485sl->link_freq, mode->link_freq_idx);
		pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
			     mode->bpp * 2 * lane;
		__v4l2_ctrl_s_ctrl_int64(sc485sl->pixel_rate, pixel_rate);
		dev_info(&sc485sl->client->dev, "freq_idx:%d pixel_rate:%lld\n",
			 mode->link_freq_idx, pixel_rate);

		sc485sl->cur_vts = mode->vts_def;
		sc485sl->cur_fps = mode->max_fps;

		dev_info(&sc485sl->client->dev, "hts_def:%d cur_vts:%d cur_fps:%d\n",
			 mode->hts_def, mode->vts_def,
			 sc485sl->cur_fps.denominator / sc485sl->cur_fps.numerator);
	} else {
		dev_err(&sc485sl->client->dev, "couldn't match the support modes\n");
		return -EINVAL;
	}

	return 0;
}

static long sc485sl_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rk_sensor_setting *setting;
	u32 i, h, w;
	long ret = 0;
	u32 stream = 0;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc485sl->bus_cfg.bus.mipi_csi2.num_data_lanes;
	const struct sc485sl_mode *mode;
	int cur_best_fit = -1;
	int cur_best_fit_dist = -1;
	int cur_dist, cur_fps, dst_fps;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc485sl_get_module_inf(sc485sl, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = sc485sl->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		if (hdr->hdr_mode == sc485sl->cur_mode->hdr_mode)
			return 0;
		w = sc485sl->cur_mode->width;
		h = sc485sl->cur_mode->height;
		dst_fps = DIV_ROUND_CLOSEST(sc485sl->cur_mode->max_fps.denominator,
					    sc485sl->cur_mode->max_fps.numerator);
		for (i = 0; i < sc485sl->cfg_num; i++) {
			if (w == sc485sl->supported_modes[i].width &&
			    h == sc485sl->supported_modes[i].height &&
			    sc485sl->supported_modes[i].hdr_mode == hdr->hdr_mode &&
			    sc485sl->supported_modes[i].bus_fmt == sc485sl->cur_mode->bus_fmt) {
				cur_fps = DIV_ROUND_CLOSEST(sc485sl->supported_modes[i].max_fps.denominator,
							    sc485sl->supported_modes[i].max_fps.numerator);
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
			dev_err(&sc485sl->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			sc485sl->cur_mode = &sc485sl->supported_modes[cur_best_fit];
			mode = sc485sl->cur_mode;
			w = mode->hts_def - mode->width;
			h = mode->vts_def - mode->height;
			__v4l2_ctrl_modify_range(sc485sl->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(sc485sl->vblank, h,
						 SC485SL_VTS_MAX - sc485sl->cur_mode->height, 1, h);
			sc485sl->cur_fps = sc485sl->cur_mode->max_fps;

			dst_link_freq = mode->link_freq_idx;
			dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
					 mode->bpp * 2 * lanes;
			__v4l2_ctrl_s_ctrl_int64(sc485sl->pixel_rate,
						 dst_pixel_rate);
			__v4l2_ctrl_s_ctrl(sc485sl->link_freq,
					   dst_link_freq);
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		sc485sl_set_hdrae(sc485sl, arg);
		if (sc485sl->cam_sw_inf)
			memcpy(&sc485sl->cam_sw_inf->hdr_ae, (struct preisp_hdrae_exp_s *)(arg),
			       sizeof(struct preisp_hdrae_exp_s));
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (sc485sl->standby_hw) {	/* hardware standby */
			if (stream) {
				sc485sl->is_standby = false;
				/* pwdn gpio pull up */
				if (!IS_ERR(sc485sl->pwdn_gpio))
					gpiod_set_value_cansleep(sc485sl->pwdn_gpio, 1);
				// Make sure __v4l2_ctrl_handler_setup can be called correctly
				usleep_range(4000, 5000);
				/* mipi clk on */
				ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_MIPI_CTRL,
							 SC485SL_REG_VALUE_08BIT,
							 SC485SL_MIPI_CTRL_ON);

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
				if (__v4l2_ctrl_handler_setup(&sc485sl->ctrl_handler))
					dev_err(&sc485sl->client->dev, "__v4l2_ctrl_handler_setup fail!");
				/* Check if the current mode is HDR and cam sw info is available */
				if (sc485sl->cur_mode->hdr_mode != NO_HDR && sc485sl->cam_sw_inf) {
					ret = sc485sl_ioctl(&sc485sl->subdev,
							    PREISP_CMD_SET_HDRAE_EXP,
							    &sc485sl->cam_sw_inf->hdr_ae);
					if (ret) {
						dev_err(&sc485sl->client->dev,
							"Failed init exp fail in hdr mode\n");
						return ret;
					}

				}
#endif

				/* stream on */
				ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_CTRL_MODE,
							 SC485SL_REG_VALUE_08BIT,
							 SC485SL_MODE_STREAMING);
				dev_info(&sc485sl->client->dev,
					 "quickstream, streaming on: exit hw standby mode\n");
			} else {
				/* stream off */
				ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_CTRL_MODE,
							 SC485SL_REG_VALUE_08BIT,
							 SC485SL_MODE_SW_STANDBY);
				/* mipi clk off */
				ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_MIPI_CTRL,
							 SC485SL_REG_VALUE_08BIT,
							 SC485SL_MIPI_CTRL_OFF);

				sc485sl->is_standby = true;
				/* pwnd gpio pull down */
				if (!IS_ERR(sc485sl->pwdn_gpio))
					gpiod_set_value_cansleep(sc485sl->pwdn_gpio, 0);
				dev_info(&sc485sl->client->dev,
					 "quickstream, streaming off: enter hw standby mode\n");
			}
		} else {	/* software standby */
			if (stream) {
				ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_MIPI_CTRL,
							 SC485SL_REG_VALUE_08BIT,
							 SC485SL_MIPI_CTRL_ON);
				ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_CTRL_MODE,
							 SC485SL_REG_VALUE_08BIT,
							 SC485SL_MODE_STREAMING);
				dev_info(&sc485sl->client->dev,
					 "quickstream, streaming on: exit soft standby mode\n");
			} else {
				ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_CTRL_MODE,
							 SC485SL_REG_VALUE_08BIT,
							 SC485SL_MODE_SW_STANDBY);
				ret |= sc485sl_write_reg(sc485sl->client, SC485SL_REG_MIPI_CTRL,
							 SC485SL_REG_VALUE_08BIT,
							 SC485SL_MIPI_CTRL_OFF);
				dev_info(&sc485sl->client->dev,
					 "quickstream, streaming off: enter soft standby mode\n");
			}
		}
		break;
	case RKCIS_CMD_SELECT_SETTING:
		setting = (struct rk_sensor_setting *)arg;
		ret = sc485sl_set_setting(sc485sl, setting);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}


#ifdef CONFIG_COMPAT
static long sc485sl_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rk_sensor_setting *setting;
	long ret;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc485sl_ioctl(sd, cmd, inf);
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

		ret = sc485sl_ioctl(sd, cmd, hdr);
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
			ret = sc485sl_ioctl(sd, cmd, hdr);
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
			ret = sc485sl_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = sc485sl_ioctl(sd, cmd, &stream);
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
			ret = sc485sl_ioctl(sd, cmd, setting);
		else
			ret = -EFAULT;
		kfree(setting);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __sc485sl_start_stream(struct sc485sl *sc485sl)
{
	int ret;

	if (!sc485sl->is_thunderboot) {
		ret = sc485sl_write_array(sc485sl->client, sc485sl->cur_mode->reg_list);
		if (ret)
			return ret;
		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&sc485sl->ctrl_handler);
		if (ret)
			return ret;
		if (sc485sl->has_init_exp && sc485sl->cur_mode->hdr_mode != NO_HDR) {
			ret = sc485sl_ioctl(&sc485sl->subdev, PREISP_CMD_SET_HDRAE_EXP,
					    &sc485sl->init_hdrae_exp);
			if (ret) {
				dev_err(&sc485sl->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	ret = sc485sl_write_reg(sc485sl->client, SC485SL_REG_CTRL_MODE,
				SC485SL_REG_VALUE_08BIT, SC485SL_MODE_STREAMING);
	return ret;
}

static int __sc485sl_stop_stream(struct sc485sl *sc485sl)
{
	sc485sl->has_init_exp = false;
	if (sc485sl->is_thunderboot)
		sc485sl->is_first_streamoff = true;
	return sc485sl_write_reg(sc485sl->client, SC485SL_REG_CTRL_MODE,
				 SC485SL_REG_VALUE_08BIT, SC485SL_MODE_SW_STANDBY);
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 sc485sl_cal_delay(u32 cycles, struct sc485sl *sc485sl)
{
	return DIV_ROUND_UP(cycles, sc485sl->cur_mode->mclk / 1000 / 1000);
}

static int __sc485sl_power_on(struct sc485sl *sc485sl)
{
	int ret;
	u32 delay_us;
	struct device *dev = &sc485sl->client->dev;

	if (!IS_ERR_OR_NULL(sc485sl->pins_default)) {
		ret = pinctrl_select_state(sc485sl->pinctrl,
					   sc485sl->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc485sl->xvclk, sc485sl->cur_mode->mclk);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (%dHz)\n", sc485sl->cur_mode->mclk);
	if (clk_get_rate(sc485sl->xvclk) != sc485sl->cur_mode->mclk)
		dev_warn(dev, "xvclk mismatched, modes are based on %dHz\n",
			 sc485sl->cur_mode->mclk);
	ret = clk_prepare_enable(sc485sl->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	cam_sw_regulator_bulk_init(sc485sl->cam_sw_inf, SC485SL_NUM_SUPPLIES, sc485sl->supplies);

	if (sc485sl->is_thunderboot)
		return 0;

	if (!IS_ERR(sc485sl->reset_gpio))
		gpiod_set_value_cansleep(sc485sl->reset_gpio, 0);

	ret = regulator_bulk_enable(SC485SL_NUM_SUPPLIES, sc485sl->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc485sl->reset_gpio))
		gpiod_set_value_cansleep(sc485sl->reset_gpio, 1);

	usleep_range(500, 1000);

	if (!IS_ERR(sc485sl->pwdn_gpio))
		gpiod_set_value_cansleep(sc485sl->pwdn_gpio, 1);

	if (!IS_ERR(sc485sl->reset_gpio))
		usleep_range(6000, 8000);
	else
		usleep_range(12000, 16000);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = sc485sl_cal_delay(8192, sc485sl);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(sc485sl->xvclk);

	return ret;
}

static void __sc485sl_power_off(struct sc485sl *sc485sl)
{
	int ret;
	struct device *dev = &sc485sl->client->dev;

	clk_disable_unprepare(sc485sl->xvclk);
	if (sc485sl->is_thunderboot) {
		if (sc485sl->is_first_streamoff) {
			sc485sl->is_thunderboot = false;
			sc485sl->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(sc485sl->pwdn_gpio))
		gpiod_set_value_cansleep(sc485sl->pwdn_gpio, 0);
	clk_disable_unprepare(sc485sl->xvclk);
	if (!IS_ERR(sc485sl->reset_gpio))
		gpiod_set_value_cansleep(sc485sl->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(sc485sl->pins_sleep)) {
		ret = pinctrl_select_state(sc485sl->pinctrl,
					   sc485sl->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(SC485SL_NUM_SUPPLIES, sc485sl->supplies);
}

static int sc485sl_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	struct i2c_client *client = sc485sl->client;
	int ret = 0;

	mutex_lock(&sc485sl->mutex);

	on = !!on;
	if (on == sc485sl->streaming)
		goto unlock_and_return;

	if (on) {
		if (sc485sl->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			sc485sl->is_thunderboot = false;
			__sc485sl_power_on(sc485sl);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __sc485sl_start_stream(sc485sl);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc485sl_stop_stream(sc485sl);
		pm_runtime_put(&client->dev);
	}

	sc485sl->streaming = on;
unlock_and_return:
	mutex_unlock(&sc485sl->mutex);
	return ret;
}

static int sc485sl_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	struct i2c_client *client = sc485sl->client;
	int ret = 0;

	mutex_lock(&sc485sl->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc485sl->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!sc485sl->is_thunderboot) {
			ret = sc485sl_write_array(sc485sl->client,
						  sc485sl->cur_mode->global_reg_list);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		sc485sl->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc485sl->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc485sl->mutex);

	return ret;
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int __maybe_unused sc485sl_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc485sl *sc485sl = to_sc485sl(sd);

	if (sc485sl->standby_hw) {
		dev_info(dev, "resume standby!");
		return 0;
	}

	cam_sw_prepare_wakeup(sc485sl->cam_sw_inf, dev);
	usleep_range(4000, 5000);
	cam_sw_write_array(sc485sl->cam_sw_inf);

	if (__v4l2_ctrl_handler_setup(&sc485sl->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (sc485sl->has_init_exp && sc485sl->cur_mode != NO_HDR) {	// hdr mode
		ret = sc485sl_ioctl(&sc485sl->subdev, PREISP_CMD_SET_HDRAE_EXP,
				    &sc485sl->cam_sw_inf->hdr_ae);
		if (ret) {
			dev_err(&sc485sl->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}

	return 0;
}

static int __maybe_unused sc485sl_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc485sl *sc485sl = to_sc485sl(sd);

	if (sc485sl->standby_hw) {
		dev_info(dev, "suspend standby!");
		return 0;
	}

	cam_sw_write_array_cb_init(sc485sl->cam_sw_inf, client,
				   (void *)sc485sl->cur_mode->reg_list,
				   (sensor_write_array)sc485sl_write_array);
	cam_sw_prepare_sleep(sc485sl->cam_sw_inf);

	return 0;
}
#else
#define sc485sl_resume NULL
#define sc485sl_suspend NULL
#endif

static int __maybe_unused sc485sl_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc485sl *sc485sl = to_sc485sl(sd);

	return __sc485sl_power_on(sc485sl);
}

static int __maybe_unused sc485sl_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc485sl *sc485sl = to_sc485sl(sd);

	__sc485sl_power_off(sc485sl);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc485sl_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct sc485sl_mode *def_mode = &sc485sl->supported_modes[0];

	mutex_lock(&sc485sl->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc485sl->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc485sl_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct sc485sl *sc485sl = to_sc485sl(sd);

	if (fie->index >= sc485sl->cfg_num)
		return -EINVAL;

	fie->code = sc485sl->supported_modes[fie->index].bus_fmt;
	fie->width = sc485sl->supported_modes[fie->index].width;
	fie->height = sc485sl->supported_modes[fie->index].height;
	fie->interval = sc485sl->supported_modes[fie->index].max_fps;
	fie->reserved[0] = sc485sl->supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops sc485sl_pm_ops = {
	SET_RUNTIME_PM_OPS(sc485sl_runtime_suspend,
	sc485sl_runtime_resume, NULL)
#ifdef CONFIG_VIDEO_CAM_SLEEP_WAKEUP
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sc485sl_suspend, sc485sl_resume)
#endif
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc485sl_internal_ops = {
	.open = sc485sl_open,
};
#endif

static const struct v4l2_subdev_core_ops sc485sl_core_ops = {
	.s_power = sc485sl_s_power,
	.ioctl = sc485sl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc485sl_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc485sl_video_ops = {
	.s_stream = sc485sl_s_stream,
	.g_frame_interval = sc485sl_g_frame_interval,
	.s_frame_interval = sc485sl_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops sc485sl_pad_ops = {
	.enum_mbus_code = sc485sl_enum_mbus_code,
	.enum_frame_size = sc485sl_enum_frame_sizes,
	.enum_frame_interval = sc485sl_enum_frame_interval,
	.get_fmt = sc485sl_get_fmt,
	.set_fmt = sc485sl_set_fmt,
	.get_mbus_config = sc485sl_g_mbus_config,
};

static const struct v4l2_subdev_ops sc485sl_subdev_ops = {
	.core	= &sc485sl_core_ops,
	.video	= &sc485sl_video_ops,
	.pad	= &sc485sl_pad_ops,
};

static void sc485sl_modify_fps_info(struct sc485sl *sc485sl)
{
	const struct sc485sl_mode *mode = sc485sl->cur_mode;

	sc485sl->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				       sc485sl->cur_vts;
}

static int sc485sl_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc485sl *sc485sl = container_of(ctrl->handler,
					       struct sc485sl, ctrl_handler);
	struct i2c_client *client = sc485sl->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc485sl->cur_mode->height + ctrl->val - 5;
		__v4l2_ctrl_modify_range(sc485sl->exposure,
					 sc485sl->exposure->minimum, max,
					 sc485sl->exposure->step,
					 sc485sl->exposure->default_value);
		break;
	}

	if (sc485sl->standby_hw && sc485sl->is_standby) {
		dev_dbg(&client->dev, "%s: is_standby = true, will return\n", __func__);
		return 0;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		if (sc485sl->cur_mode->hdr_mode == NO_HDR) {
			/* exposure use one line as unit */
			/* 4 least significant bits of expsoure are fractional part */
			ret = sc485sl_write_reg(sc485sl->client,
						SC485SL_REG_EXPOSURE_H,
						SC485SL_REG_VALUE_08BIT,
						SC485SL_FETCH_EXP_H(ctrl->val));
			ret |= sc485sl_write_reg(sc485sl->client,
						 SC485SL_REG_EXPOSURE_M,
						 SC485SL_REG_VALUE_08BIT,
						 SC485SL_FETCH_EXP_M(ctrl->val));
			ret |= sc485sl_write_reg(sc485sl->client,
						 SC485SL_REG_EXPOSURE_L,
						 SC485SL_REG_VALUE_08BIT,
						 SC485SL_FETCH_EXP_L(ctrl->val));
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set gain 0x%x\n", ctrl->val);
		if (sc485sl->cur_mode->hdr_mode == NO_HDR)
			ret = sc485sl_set_gain_reg(sc485sl, ctrl->val, SC485SL_LGAIN);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		ret = sc485sl_write_reg(sc485sl->client,
					SC485SL_REG_VTS_H,
					SC485SL_REG_VALUE_08BIT,
					0x00);
		ret |= sc485sl_write_reg(sc485sl->client,
					 SC485SL_REG_VTS_M,
					 SC485SL_REG_VALUE_08BIT,
					 (ctrl->val + sc485sl->cur_mode->height)
					 >> 8);
		ret |= sc485sl_write_reg(sc485sl->client,
					 SC485SL_REG_VTS_L,
					 SC485SL_REG_VALUE_08BIT,
					 (ctrl->val + sc485sl->cur_mode->height)
					 & 0xff);
		sc485sl->cur_vts = ctrl->val + sc485sl->cur_mode->height;
		if (sc485sl->cur_vts != sc485sl->cur_mode->vts_def)
			sc485sl_modify_fps_info(sc485sl);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc485sl_enable_test_pattern(sc485sl, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc485sl_read_reg(sc485sl->client, SC485SL_FLIP_MIRROR_REG,
				       SC485SL_REG_VALUE_08BIT, &val);
		ret |= sc485sl_write_reg(sc485sl->client, SC485SL_FLIP_MIRROR_REG,
					 SC485SL_REG_VALUE_08BIT,
					 SC485SL_FETCH_MIRROR(val, ctrl->val));
		break;
	case V4L2_CID_VFLIP:
		ret = sc485sl_read_reg(sc485sl->client, SC485SL_FLIP_MIRROR_REG,
				       SC485SL_REG_VALUE_08BIT, &val);
		ret |= sc485sl_write_reg(sc485sl->client, SC485SL_FLIP_MIRROR_REG,
					 SC485SL_REG_VALUE_08BIT,
					 SC485SL_FETCH_FLIP(val, ctrl->val));
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}


static const struct v4l2_ctrl_ops sc485sl_ctrl_ops = {
	.s_ctrl = sc485sl_set_ctrl,
};

static int sc485sl_initialize_controls(struct sc485sl *sc485sl)
{
	const struct sc485sl_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc485sl->bus_cfg.bus.mipi_csi2.num_data_lanes;

	handler = &sc485sl->ctrl_handler;
	mode = sc485sl->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc485sl->mutex;

	sc485sl->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			     V4L2_CID_LINK_FREQ,
			     ARRAY_SIZE(link_freq_menu_items) - 1,
			     0, link_freq_menu_items);
	if (sc485sl->link_freq)
		sc485sl->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	dst_link_freq = mode->link_freq_idx;
	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
			 mode->bpp * 2 * lanes;
	if (lanes == 2) {
		if (dst_link_freq == SC485SL_LINK_FREQ_360)
			sc485sl->pixel_rate =
				v4l2_ctrl_new_std(handler, NULL,
						  V4L2_CID_PIXEL_RATE, 0,
						  PIXEL_RATE_WITH_360M_10BIT_2L,
						  1, dst_pixel_rate);
		else if (dst_link_freq == SC485SL_LINK_FREQ_720)
			sc485sl->pixel_rate =
				v4l2_ctrl_new_std(handler, NULL,
						  V4L2_CID_PIXEL_RATE, 0,
						  PIXEL_RATE_WITH_720M_10BIT_2L,
						  1, dst_pixel_rate);


	} else if (lanes == 4) {
		if (mode->hdr_mode == NO_HDR)
			sc485sl->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
								0, PIXEL_RATE_WITH_396M_10BIT_4L,
								1, dst_pixel_rate);
		else if (mode->hdr_mode == HDR_X2)
			sc485sl->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
								0, PIXEL_RATE_WITH_540M_10BIT_4L,
								1, dst_pixel_rate);
	}

	__v4l2_ctrl_s_ctrl(sc485sl->link_freq, dst_link_freq);

	h_blank = mode->hts_def - mode->width;
	sc485sl->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					    h_blank, h_blank, 1, h_blank);
	if (sc485sl->hblank)
		sc485sl->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	sc485sl->vblank = v4l2_ctrl_new_std(handler, &sc485sl_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_def,
					    SC485SL_VTS_MAX - mode->height,
					    1, vblank_def);
	exposure_max = mode->vts_def - 8;
	sc485sl->exposure = v4l2_ctrl_new_std(handler, &sc485sl_ctrl_ops,
					      V4L2_CID_EXPOSURE, SC485SL_EXPOSURE_MIN,
					      exposure_max, SC485SL_EXPOSURE_STEP,
					      mode->exp_def); //Set default exposure
	sc485sl->anal_gain = v4l2_ctrl_new_std(handler, &sc485sl_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN, SC485SL_GAIN_MIN,
					       SC485SL_GAIN_MAX, SC485SL_GAIN_STEP,
					       SC485SL_GAIN_DEFAULT); //Set default gain
	sc485sl->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&sc485sl_ctrl_ops,
				V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(sc485sl_test_pattern_menu) - 1,
				0, 0, sc485sl_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &sc485sl_ctrl_ops,
			  V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &sc485sl_ctrl_ops,
			  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&sc485sl->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc485sl->subdev.ctrl_handler = handler;
	sc485sl->has_init_exp = false;
	sc485sl->cur_fps = mode->max_fps;
	sc485sl->is_standby = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc485sl_check_sensor_id(struct sc485sl *sc485sl,
				   struct i2c_client *client)
{
	struct device *dev = &sc485sl->client->dev;
	u32 id = 0;
	int ret;

	if (sc485sl->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = sc485sl_read_reg(client, SC485SL_REG_CHIP_ID,
			       SC485SL_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected SC485SL (0x%04x) sensor\n", CHIP_ID);

	return 0;
}

static int sc485sl_configure_regulators(struct sc485sl *sc485sl)
{
	unsigned int i;

	for (i = 0; i < SC485SL_NUM_SUPPLIES; i++)
		sc485sl->supplies[i].supply = sc485sl_supply_names[i];

	return devm_regulator_bulk_get(&sc485sl->client->dev,
				       SC485SL_NUM_SUPPLIES,
				       sc485sl->supplies);
}

static int sc485sl_read_module_info(struct sc485sl *sc485sl)
{
	int ret;
	struct device *dev = &sc485sl->client->dev;
	struct device_node *node = dev->of_node;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc485sl->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc485sl->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc485sl->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc485sl->len_name);
	if (ret)
		dev_err(dev, "could not get module information!\n");

	/* Compatible with non-standby mode if this attribute is not configured in dts*/
	of_property_read_u32(node, RKMODULE_CAMERA_STANDBY_HW,
			     &sc485sl->standby_hw);
	dev_info(dev, "sc485sl->standby_hw = %d\n", sc485sl->standby_hw);

	return ret;
}

static int sc485sl_find_modes(struct sc485sl *sc485sl)
{
	int i, ret;
	u32 hdr_mode = 0;
	struct device_node *endpoint;
	struct device *dev = &sc485sl->client->dev;
	struct device_node *node = sc485sl->client->dev.of_node;

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
					 &sc485sl->bus_cfg);
	of_node_put(endpoint);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}

	if (sc485sl->bus_cfg.bus.mipi_csi2.num_data_lanes == 4) {
		sc485sl->supported_modes = supported_modes_4lane;
		sc485sl->cfg_num = ARRAY_SIZE(supported_modes_4lane);
		dev_info(dev, "Detect sc485sl lane: %d\n",
			 sc485sl->bus_cfg.bus.mipi_csi2.num_data_lanes);
	} else {
		sc485sl->supported_modes = supported_modes_2lane;
		sc485sl->cfg_num = ARRAY_SIZE(supported_modes_2lane);
		dev_info(dev, "Detect sc485sl lane: %d\n",
			 sc485sl->bus_cfg.bus.mipi_csi2.num_data_lanes);
	}

	for (i = 0; i < sc485sl->cfg_num; i++) {
		if (hdr_mode == sc485sl->supported_modes[i].hdr_mode) {
			sc485sl->cur_mode = &sc485sl->supported_modes[i];
			break;
		}
	}

	if (i == sc485sl->cfg_num)
		sc485sl->cur_mode = &sc485sl->supported_modes[0];

	return 0;
}

static int sc485sl_setup_clocks_and_gpios(struct sc485sl *sc485sl)
{
	struct device *dev = &sc485sl->client->dev;

	sc485sl->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc485sl->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	sc485sl->reset_gpio = devm_gpiod_get(dev, "reset",
					     sc485sl->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(sc485sl->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	sc485sl->pwdn_gpio = devm_gpiod_get(dev, "pwdn",
					    sc485sl->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(sc485sl->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	sc485sl->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc485sl->pinctrl)) {
		sc485sl->pins_default =
			pinctrl_lookup_state(sc485sl->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc485sl->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		sc485sl->pins_sleep =
			pinctrl_lookup_state(sc485sl->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc485sl->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	return 0;
}

static int sc485sl_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc485sl *sc485sl;
	struct v4l2_subdev *sd;

	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	sc485sl = devm_kzalloc(dev, sizeof(*sc485sl), GFP_KERNEL);
	if (!sc485sl)
		return -ENOMEM;

	sc485sl->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);
	sc485sl->client = client;

	ret = sc485sl_read_module_info(sc485sl);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	/* Set current mode based on HDR mode */
	ret = sc485sl_find_modes(sc485sl);
	if (ret) {
		dev_err(dev, "Failed to get modes!\n");
		return -EINVAL;
	}

	/* setup sc485sl clock and gpios*/
	ret = sc485sl_setup_clocks_and_gpios(sc485sl);
	if (ret) {
		dev_err(dev, "Failed to set up clocks and GPIOs\n");
		return ret;
	}

	ret = sc485sl_configure_regulators(sc485sl);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc485sl->mutex);

	sd = &sc485sl->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc485sl_subdev_ops);
	ret = sc485sl_initialize_controls(sc485sl);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc485sl_power_on(sc485sl);
	if (ret)
		goto err_free_handler;

	ret = sc485sl_check_sensor_id(sc485sl, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc485sl_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc485sl->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc485sl->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	if (!sc485sl->cam_sw_inf) {
		sc485sl->cam_sw_inf = cam_sw_init();
		cam_sw_clk_init(sc485sl->cam_sw_inf, sc485sl->xvclk,
				sc485sl->cur_mode->mclk);
		cam_sw_reset_pin_init(sc485sl->cam_sw_inf, sc485sl->reset_gpio, 0);
		cam_sw_pwdn_pin_init(sc485sl->cam_sw_inf, sc485sl->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc485sl->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc485sl->module_index, facing,
		 SC485SL_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (sc485sl->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__sc485sl_power_off(sc485sl);
err_free_handler:
	v4l2_ctrl_handler_free(&sc485sl->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc485sl->mutex);

	return ret;
}

static void sc485sl_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc485sl *sc485sl = to_sc485sl(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc485sl->ctrl_handler);
	mutex_destroy(&sc485sl->mutex);

	cam_sw_deinit(sc485sl->cam_sw_inf);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc485sl_power_off(sc485sl);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc485sl_of_match[] = {
	{ .compatible = "smartsens,sc485sl" },
	{},
};
MODULE_DEVICE_TABLE(of, sc485sl_of_match);
#endif

static const struct i2c_device_id sc485sl_match_id[] = {
	{ "smartsens,sc485sl", 0 },
	{ },
};

static struct i2c_driver sc485sl_i2c_driver = {
	.driver = {
		.name = SC485SL_NAME,
		.pm = &sc485sl_pm_ops,
		.of_match_table = of_match_ptr(sc485sl_of_match),
	},
	.probe		= sc485sl_probe,
	.remove		= sc485sl_remove,
	.id_table	= sc485sl_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc485sl_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc485sl_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens sc485sl CMOS Image Sensor driver");
MODULE_LICENSE("GPL");
