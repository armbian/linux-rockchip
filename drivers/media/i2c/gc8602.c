// SPDX-License-Identifier: GPL-2.0
/*
 * GC8602 driver
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 *
 * Based on Rockchip GalaxyCore sensor drivers.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define GC8602_LANES			4
#define GC8602_BITS_PER_SAMPLE		10
#define GC8602_LINK_FREQ_LINEAR		502000000
#define GC8602_LINK_FREQ_HDR		594000000

#define GC8602_PIXEL_RATE_LINEAR	(GC8602_LINK_FREQ_LINEAR * 2 / 10 * 4)
#define GC8602_PIXEL_RATE_HDR		(GC8602_LINK_FREQ_HDR * 2 / 10 * 4)

#define GC8602_XVCLK_FREQ_24M		24000000

#define CHIP_ID				0x8602
#define GC8602_REG_CHIP_ID_H		0x03f0
#define GC8602_REG_CHIP_ID_M		0x03f1

#define GC8602_REG_CTRL_MODE		0x0100
#define GC8602_MODE_SW_STANDBY		0x00
#define GC8602_MODE_STREAMING		0x09

#define GC8602_REG_SEXPOSURE_H		0x0200
#define GC8602_REG_SEXPOSURE_L		0x0201
#define GC8602_REG_EXPOSURE_H		0x0202
#define GC8602_REG_EXPOSURE_L		0x0203
#define GC8602_EXPOSURE_MARGIN		8
#define GC8602_EXPOSURE_MIN		4
#define GC8602_EXPOSURE_STEP		1
#define GC8602_VTS_MAX			0x1fff

#define GC8602_GAIN_MIN			64
#define GC8602_GAIN_MAX			0xffff
#define GC8602_GAIN_STEP		1
#define GC8602_GAIN_DEFAULT		256

#define GC8602_REG_GAIN_T1_H		0x0807
#define GC8602_REG_GAIN_T1_L		0x0808
#define GC8602_REG_GAIN_T2_H		0x0809
#define GC8602_REG_GAIN_T2_L		0x080a

#define GC8602_REG_TEST_PATTERN		0x008c
#define GC8602_TEST_PATTERN_ENABLE	0x11
#define GC8602_TEST_PATTERN_DISABLE	0x0

#define GC8602_REG_VTS_H		0x0340
#define GC8602_REG_VTS_L		0x0341
#define GC8602_REG_HTS_H		0x0342
#define GC8602_REG_HTS_L		0x0343

/* Mirror/flip registers - may need adjustment based on actual sensor */
#define GC8602_OTP_MIRROR_FLIP_REG	0x0a73
#define GC8602_FLIP_MIRROR_REG		0x022c
#define GC8602_FLIP_MIR_MOD_REG		0x0063
#define GC8602_MIRROR_BIT_MASK		BIT(0)
#define GC8602_FLIP_BIT_MASK		BIT(1)

#define REG_DELAY			0x0000
#define REG_NULL			0xFFFF

#define GC8602_REG_VALUE_08BIT		1
#define GC8602_REG_VALUE_16BIT		2
#define GC8602_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define GC8602_NAME			"gc8602"

static const char *const gc8602_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
	"avdd",		/* Analog power */
};

#define GC8602_NUM_SUPPLIES ARRAY_SIZE(gc8602_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct gc8602_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
	u32 mipi_freq_idx;
	u32 bpp;
};

struct gc8602 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct gpio_desc	*pwren_gpio;
	struct regulator_bulk_data supplies[GC8602_NUM_SUPPLIES];

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	struct v4l2_fract	cur_fps;
	bool			streaming;
	bool			power_on;
	const struct gc8602_mode *cur_mode;
	u32			cfg_num;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	u32			cur_pixel_rate;
	u32			cur_link_freq;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	struct cam_sw_info	*cam_sw_inf;
	bool			has_init_exp;
	bool			is_thunderboot;
	bool			is_first_streamoff;
};

#define to_gc8602(sd) container_of(sd, struct gc8602, subdev)

/* Linear mode registers from GC8602 MIPI4l raw10 3840x2160 30fps table. */
static const struct regval gc8602_linear_10bit_3840x2160_30fps_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0331, 0x04},
	{0x0a38, 0x01},
	{0x0a39, 0x72},
	{0x0a34, 0x03},
	{0x0a23, 0x39},
	{0x0a21, 0x33},
	{0x0a39, 0x73},
	{0x0a3a, 0x7d},
	{0x0a35, 0x86},
	{0x0a3b, 0x40},
	{0x0a3c, 0xb9},
	{0x0a36, 0x03},
	{0x0a32, 0x0c},
	{0x0a33, 0x50},
	{0x0a3d, 0x95},
	{0x0a3e, 0xe1},
	{0x0a22, 0x46},
	{0x0a20, 0x04},
	{0x0a24, 0x04},
	{0x0a25, 0x00},
	{0x0a26, 0x87},
	{0x0a21, 0x37},
	{0x0a21, 0x77},
	{0x0ac2, 0x3f},
	{0x0a33, 0x51},
	{0x0a33, 0x71},
	{0x0314, 0x10},
	{0x0320, 0x00},
	{0x0327, 0xc3},
	{0x031c, 0x46},
	{0x0331, 0x07},
	{0x0ab2, 0x7a},
	{0x029b, 0x00},
	{0x0ab3, 0x0e},
	{0x0e04, 0x03},
	{0x0e0e, 0x03},
	{0x0e0c, 0x03},
	{0x0ab4, 0x09},
	{0x0aba, 0xa5},
	{0x0081, 0xdf},
	{0x0abb, 0x05},
	{0x0245, 0x02},
	{0x0006, 0xbb},
	{0x0e10, 0x08},
	{0x0e11, 0xfc},
	{0x0e0e, 0x23},
	{0x0e0d, 0x02},
	{0x02ce, 0x03},
	{0x02cc, 0x03},
	{0x0e16, 0x06},
	{0x02cd, 0x35},
	{0x0275, 0x35},
	{0x0238, 0x00},
	{0x0239, 0x06},
	{0x023a, 0x06},
	{0x02c1, 0xde},
	{0x02c2, 0xde},
	{0x0e68, 0x00},
	{0x0e69, 0x7a},
	{0x0e35, 0x22},
	{0x0e5d, 0x36},
	{0x0e66, 0x00},
	{0x0e67, 0x79},
	{0x0e52, 0x22},
	{0x0e59, 0x22},
	{0x021b, 0x52},
	{0x021c, 0x08},
	{0x0242, 0x06},
	{0x0243, 0x06},
	{0x0331, 0x07},
	{0x0ab4, 0x09},
	{0x0ab6, 0x24},
	{0x0e60, 0x40},
	{0x0e64, 0x00},
	{0x0e65, 0xf0},
	{0x0e44, 0x1e},
	{0x0e1a, 0xc1},
	{0x0e8b, 0x01},
	{0x0c18, 0x40},
	{0x0276, 0x01},
	{0x0357, 0x06},
	{0x0e1a, 0x01},
	{0x0abb, 0x05},
	{0x0e11, 0x0c},
	{0x0e45, 0x0c},
	{0x0e0c, 0xe3},
	{0x0e4d, 0x00},
	{0x0e70, 0x90},
	{0x029b, 0x08},
	{0x029e, 0x23},
	{0x029a, 0x10},
	{0x029d, 0x00},
	{0x0130, 0x50},
	{0x0131, 0x01},
	{0x0274, 0x0b},
	{0x0ab2, 0x7b},
	{0x0212, 0x30},
	{0x0213, 0x04},
	{0x0219, 0x4f},
	{0x0259, 0x08},
	{0x025a, 0x9e},
	{0x0340, 0x11},
	{0x0341, 0x94},
	{0x0342, 0x01},
	{0x0343, 0xf4},
	{0x0217, 0xa0},
	{0x0087, 0x50},
	{0x0346, 0x00},
	{0x0347, 0x28},
	{0x0348, 0x0f},
	{0x0349, 0x10},
	{0x034a, 0x08},
	{0x034b, 0x80},
	{0x0c22, 0x00},
	{0x0c23, 0x04},
	{0x0c24, 0x07},
	{0x0c25, 0x8a},
	{0x034e, 0x0f},
	{0x034f, 0x50},
	{0x0c12, 0x43},
	{0x0c13, 0x43},
	{0x0089, 0x00},
	{0x0004, 0x0f},
	{0x0004, 0x0f},
	{0x0036, 0x00},
	{0x0037, 0x00},
	{0x0038, 0x00},
	{0x0039, 0x00},
	{0x003a, 0x00},
	{0x0082, 0x00},
	{0x0083, 0x00},
	{0x0060, 0x20},
	{0x0002, 0x09},
	{0x000f, 0x00},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x08},
	{0x009b, 0x08},
	{0x0001, 0x0c},
	{0x00b0, 0x06},
	{0x00b1, 0x10},
	{0x00c0, 0x02},
	{0x00c1, 0x20},
	{0x00d0, 0x06},
	{0x00d1, 0x10},
	{0x00b2, 0x08},
	{0x00b3, 0x0e},
	{0x00c2, 0x04},
	{0x00c3, 0x60},
	{0x00d2, 0x06},
	{0x00d3, 0x10},
	{0x00b4, 0x0a},
	{0x00b5, 0x10},
	{0x00c4, 0x04},
	{0x00c5, 0x30},
	{0x00d4, 0x06},
	{0x00d5, 0x10},
	{0x00b6, 0x06},
	{0x00b7, 0x10},
	{0x00c6, 0x04},
	{0x00c7, 0x10},
	{0x00d6, 0x06},
	{0x00d7, 0x10},
	{0x00b8, 0x06},
	{0x00b9, 0x10},
	{0x00c8, 0x06},
	{0x00c9, 0x20},
	{0x00d8, 0x06},
	{0x00d9, 0x10},
	{0x00ba, 0x06},
	{0x00bb, 0x10},
	{0x00ca, 0x06},
	{0x00cb, 0x00},
	{0x00da, 0x06},
	{0x00db, 0x10},
	{0x00bc, 0x06},
	{0x00bd, 0x10},
	{0x00cc, 0x08},
	{0x00cd, 0x10},
	{0x00dc, 0x06},
	{0x00dd, 0x10},
	{0x00be, 0x06},
	{0x00bf, 0x10},
	{0x00ce, 0x08},
	{0x00cf, 0x08},
	{0x00de, 0x06},
	{0x00df, 0x10},
	{0x0008, 0x66},
	{0x0009, 0x58},
	{0x000a, 0x47},
	{0x000b, 0x36},
	{0x0051, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x044c, 0x76},
	{0x044d, 0x76},
	{0x044e, 0x76},
	{0x044f, 0x76},
	{0x0448, 0x09},
	{0x0449, 0x09},
	{0x044a, 0x09},
	{0x044b, 0x09},
	{0x0468, 0x00},
	{0x0046, 0x20},
	{0x0c22, 0x18},
	{0x0c19, 0x04},
	{0x0c1a, 0x4c},
	{0x0c1b, 0xd8},
	{0x0c20, 0x01},
	{0x0c21, 0x00},
	{0x0202, 0x04},
	{0x0203, 0x00},
	{0x005d, 0x80},
	{0x0089, 0x00},
	{0x02ab, 0x00},
	{0x02a9, 0x00},
	{0x0e5c, 0x24},
	{0x0e56, 0x24},
	{0x0e76, 0x00},
	{0x0075, 0x48},
	{0x0800, 0x01},
	{0x0810, 0x01},
	{0x0810, 0x00},
	{0x0801, 0xff},
	{0x0803, 0xc0},
	{0x080b, 0x78},
	{0x0880, 0x00},
	{0x0881, 0x00},
	{0x0885, 0x00},
	{0x0886, 0x40},
	{0x0885, 0x00},
	{0x0886, 0x59},
	{0x0885, 0x00},
	{0x0886, 0x8b},
	{0x0885, 0x00},
	{0x0886, 0xc2},
	{0x0885, 0x01},
	{0x0886, 0x14},
	{0x0885, 0x01},
	{0x0886, 0x7d},
	{0x0885, 0x02},
	{0x0886, 0x19},
	{0x0885, 0x02},
	{0x0886, 0xdc},
	{0x0885, 0x03},
	{0x0886, 0xfe},
	{0x0885, 0x05},
	{0x0886, 0x5c},
	{0x0885, 0x07},
	{0x0886, 0xc5},
	{0x0885, 0x0a},
	{0x0886, 0xa8},
	{0x0885, 0x0e},
	{0x0886, 0xc7},
	{0x0885, 0x13},
	{0x0886, 0xfd},
	{0x0885, 0x1c},
	{0x0886, 0x90},
	{0x0880, 0x00},
	{0x0881, 0x0f},
	{0x0885, 0x02},
	{0x0886, 0xab},
	{0x0885, 0x02},
	{0x0886, 0xa9},
	{0x0885, 0x0e},
	{0x0886, 0x76},
	{0x0885, 0x0e},
	{0x0886, 0x16},
	{0x0885, 0x0e},
	{0x0886, 0x5c},
	{0x0885, 0x0e},
	{0x0886, 0x56},
	{0x0885, 0x02},
	{0x0886, 0xcd},
	{0x0885, 0x00},
	{0x0886, 0x46},
	{0x0880, 0x00},
	{0x0881, 0x17},
	{0x0885, 0x00},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x03},
	{0x0885, 0x25},
	{0x0886, 0x25},
	{0x0885, 0x20},
	{0x0886, 0x35},
	{0x0880, 0x00},
	{0x0881, 0x1b},
	{0x0885, 0x00},
	{0x0886, 0x01},
	{0x0885, 0x06},
	{0x0886, 0x03},
	{0x0885, 0x27},
	{0x0886, 0x27},
	{0x0885, 0x20},
	{0x0886, 0x35},
	{0x0880, 0x00},
	{0x0881, 0x1f},
	{0x0885, 0x00},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x25},
	{0x0886, 0x25},
	{0x0885, 0x20},
	{0x0886, 0x35},
	{0x0880, 0x00},
	{0x0881, 0x23},
	{0x0885, 0x00},
	{0x0886, 0x01},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x27},
	{0x0886, 0x27},
	{0x0885, 0x20},
	{0x0886, 0x35},
	{0x0880, 0x00},
	{0x0881, 0x27},
	{0x0885, 0x00},
	{0x0886, 0x02},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x2a},
	{0x0886, 0x2a},
	{0x0885, 0x20},
	{0x0886, 0x35},
	{0x0880, 0x00},
	{0x0881, 0x2b},
	{0x0885, 0x00},
	{0x0886, 0x03},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x2d},
	{0x0886, 0x2d},
	{0x0885, 0x20},
	{0x0886, 0x35},
	{0x0880, 0x00},
	{0x0881, 0x2f},
	{0x0885, 0x00},
	{0x0886, 0x04},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x31},
	{0x0886, 0x31},
	{0x0885, 0x20},
	{0x0886, 0x35},
	{0x0880, 0x00},
	{0x0881, 0x33},
	{0x0885, 0x00},
	{0x0886, 0x05},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x34},
	{0x0886, 0x34},
	{0x0885, 0x20},
	{0x0886, 0x35},
	{0x0880, 0x00},
	{0x0881, 0x37},
	{0x0885, 0x00},
	{0x0886, 0x06},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x35},
	{0x0886, 0x35},
	{0x0885, 0x20},
	{0x0886, 0x55},
	{0x0880, 0x00},
	{0x0881, 0x3b},
	{0x0885, 0x00},
	{0x0886, 0x07},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x38},
	{0x0886, 0x38},
	{0x0885, 0x20},
	{0x0886, 0x55},
	{0x0880, 0x00},
	{0x0881, 0x3f},
	{0x0885, 0x01},
	{0x0886, 0x04},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x3c},
	{0x0886, 0x3c},
	{0x0885, 0x30},
	{0x0886, 0x55},
	{0x0880, 0x00},
	{0x0881, 0x43},
	{0x0885, 0x01},
	{0x0886, 0x05},
	{0x0885, 0x03},
	{0x0886, 0x00},
	{0x0885, 0x31},
	{0x0886, 0x31},
	{0x0885, 0x40},
	{0x0886, 0x75},
	{0x0880, 0x00},
	{0x0881, 0x47},
	{0x0885, 0x01},
	{0x0886, 0x06},
	{0x0885, 0x02},
	{0x0886, 0x00},
	{0x0885, 0x2f},
	{0x0886, 0x2f},
	{0x0885, 0x50},
	{0x0886, 0x75},
	{0x0880, 0x00},
	{0x0881, 0x4b},
	{0x0885, 0x01},
	{0x0886, 0x07},
	{0x0885, 0x01},
	{0x0886, 0x00},
	{0x0885, 0x2c},
	{0x0886, 0x2c},
	{0x0885, 0x60},
	{0x0886, 0x75},
	{0x0880, 0x00},
	{0x0881, 0x4f},
	{0x0885, 0x01},
	{0x0886, 0x0f},
	{0x0885, 0x01},
	{0x0886, 0x00},
	{0x0885, 0x2d},
	{0x0886, 0x2d},
	{0x0885, 0x80},
	{0x0886, 0x85},
	{0x0803, 0xc1},
	{0x0807, 0x00},
	{0x0808, 0x40},
	{0x0a67, 0x80},
	{0x0a4e, 0x0c},
	{0x0a4f, 0x0c},
	{0x0a54, 0x04},
	{0x0a55, 0x04},
	{0x0a53, 0x00},
	{0x0a7f, 0x05},
	{0x0a7e, 0x0a},
	{0x0a81, 0x0a},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x002d, 0x10},
	{0x0021, 0x44},
	{0x0a90, 0x83},
	{0x0a91, 0x02},
	{0x0a93, 0xe0},
	{0x0a97, 0x01},
	{0x0a98, 0x08},
	{0x0028, 0x0f},
	{0x0029, 0x10},
	{0x002a, 0x08},
	{0x002b, 0x80},
	{0x0a9e, 0x0f},
	{0x0a9f, 0x10},
	{0x0a9b, 0x08},
	{0x0a9c, 0x80},
	{0x0022, 0x00},
	{0x0023, 0x00},
	{0x0024, 0x00},
	{0x0025, 0x00},
	{0x0a5a, 0x80},
	{REG_DELAY, 0x14}, // sleep 20
	{0x05be, 0x01},
	{0x0080, 0x02},
	{0x0020, 0x8c},
	{0x039c, 0x02},
	{0x08e9, 0x01},
	{0x039a, 0x0f},
	{0x0391, 0x3c},
	{0x0392, 0x3c},
	{0x0393, 0x3c},
	{0x0394, 0x3c},
	{0x08ea, 0x00},
	{0x08ea, 0x3c},
	{0x08eb, 0x01},
	{0x08ec, 0x38},
	{0x08ed, 0x0a},
	{0x08eb, 0x07},
	{0x08ec, 0x31},
	{0x08ed, 0x03},
	{0x08eb, 0x03},
	{0x08ec, 0x34},
	{0x08ed, 0x0a},
	{0x08eb, 0x71},
	{0x08ec, 0x33},
	{0x08ed, 0x0a},
	{0x08eb, 0x76},
	{0x08ec, 0x21},
	{0x08ed, 0x0a},
	{0x08eb, 0x77},
	{0x08ec, 0x21},
	{0x08ed, 0x0a},
	{0x08eb, 0x01},
	{0x08ec, 0x36},
	{0x08ed, 0x03},
	{0x08eb, 0x00},
	{0x08ec, 0x36},
	{0x08ed, 0x03},
	{0x08eb, 0x09},
	{0x08ec, 0x00},
	{0x08ed, 0x01},
	{0x08ea, 0x78},
	{0x08ea, 0xb4},
	{0x08eb, 0x08},
	{0x08ec, 0x00},
	{0x08ed, 0x01},
	{0x08eb, 0x73},
	{0x08ec, 0x21},
	{0x08ed, 0x0a},
	{0x08eb, 0x72},
	{0x08ec, 0x21},
	{0x08ed, 0x0a},
	{0x08eb, 0x70},
	{0x08ec, 0x33},
	{0x08ed, 0x0a},
	{0x08eb, 0x02},
	{0x08ec, 0x34},
	{0x08ed, 0x0a},
	{0x08eb, 0x00},
	{0x08ec, 0x31},
	{0x08ed, 0x03},
	{0x08eb, 0x00},
	{0x08ec, 0x38},
	{0x08ed, 0x0a},
	{0x039c, 0x03},
	{0x0100, 0x09},
	{0x0107, 0x89},
	{0x0114, 0x03},
	{0x015a, 0x92},
	{0x015b, 0x4a},
	{0x0111, 0x2b},
	{0x0112, 0x01},
	{0x010d, 0x12},
	{0x010e, 0xc0},
	{0x0124, 0x04},
	{0x0122, 0x0e},
	{0x0123, 0x6b},
	{0x0126, 0x0f},
	{0x0121, 0x1b},
	{0x0129, 0x0d},
	{0x012a, 0x23},
	{0x012b, 0x0f},
	{0x0327, 0x46},
	{0x0336, 0x01},
	{0x0336, 0x00},
	{0x03fe, 0x00},
	{REG_NULL, 0x00},
};

/* HDR mode registers (from release_hdr_v1.3.0_0100_GC8602_MIPI4l_raw10_3840x2160_30fps.txt) */
static const struct regval gc8602_hdr_10bit_3840x2160_30fps_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0331, 0x04},
	{0x0a38, 0x01},
	{0x0a39, 0x72},
	{0x0a34, 0x03},
	{0x0a23, 0x39},
	{0x0a21, 0x33},
	{0x0a39, 0x73},
	{0x0a3a, 0x7d},
	{0x0a35, 0x86},
	{0x0a3b, 0x40},
	{0x0a3c, 0xb9},
	{0x0a36, 0x03},
	{0x0a32, 0x0c},
	{0x0a33, 0x50},
	{0x0a3d, 0x95},
	{0x0a3e, 0xe1},
	{0x0a22, 0x46},
	{0x0a20, 0x04},
	{0x0a24, 0x04},
	{0x0a25, 0x00},
	{0x0a26, 0x87},
	{0x0a21, 0x37},
	{0x0a21, 0x77},
	{0x0ac2, 0x3f},
	{0x0a33, 0x51},
	{0x0a33, 0x71},
	{0x0314, 0x10},
	{0x0320, 0x00},
	{0x0327, 0xc3},
	{0x031c, 0x46},
	{0x0331, 0x07},
	{0x0ab2, 0x7a},
	{0x029b, 0x00},
	{0x0ab3, 0x0e},
	{0x0e04, 0x03},
	{0x0e0e, 0x03},
	{0x0e0c, 0x03},
	{0x0ab4, 0x09},
	{0x0aba, 0xa5},
	{0x0081, 0xdf},
	{0x0abb, 0x05},
	{0x0245, 0x02},
	{0x0006, 0xbb},
	{0x0e10, 0x08},
	{0x0e86, 0x02},
	{0x0e11, 0xfc},
	{0x0e0e, 0x23},
	{0x0e0d, 0x02},
	{0x02ce, 0x03},
	{0x02cc, 0x03},
	{0x0e16, 0x06},
	{0x0e75, 0x06},
	{0x02cd, 0x55},
	{0x0275, 0x55},
	{0x0238, 0x00},
	{0x0239, 0x06},
	{0x023a, 0x06},
	{0x02c1, 0xde},
	{0x02c2, 0xde},
	{0x0e68, 0x00},
	{0x0e69, 0x7a},
	{0x0e35, 0x22},
	{0x0e5d, 0x36},
	{0x0e66, 0x00},
	{0x0e67, 0x79},
	{0x0e52, 0x22},
	{0x0e59, 0x22},
	{0x021b, 0x52},
	{0x021c, 0x08},
	{0x0242, 0x06},
	{0x0243, 0x06},
	{0x0331, 0x07},
	{0x0ab4, 0x09},
	{0x0ab6, 0x24},
	{0x0e60, 0x40},
	{0x0e81, 0x20},
	{0x0e64, 0x00},
	{0x0e65, 0xf0},
	{0x0e83, 0x00},
	{0x0e84, 0xf0},
	{0x0e44, 0x3b},
	{0x0e78, 0x3b},
	{0x0e1a, 0xc1},
	{0x0e80, 0x01},
	{0x0e8b, 0x01},
	{0x0e8c, 0x01},
	{0x0276, 0x01},
	{0x0357, 0x06},
	{0x0212, 0x30},
	{0x0213, 0x04},
	{0x0219, 0x4f},
	{0x0259, 0x08},
	{0x025a, 0x9e},
	{0x0340, 0x08},
	{0x0341, 0xca},
	{0x0342, 0x01},
	{0x0343, 0xf4},
	{0x0217, 0xa0},
	{0x0087, 0x50},
	{0x0346, 0x00},
	{0x0347, 0x28},
	{0x0348, 0x0f},
	{0x0349, 0x10},
	{0x034a, 0x08},
	{0x034b, 0x80},
	{0x0c22, 0x00},
	{0x0c23, 0x04},
	{0x0c24, 0x07},
	{0x0c25, 0x8a},
	{0x034e, 0x0f},
	{0x034f, 0x50},
	{0x0c12, 0x43},
	{0x0c13, 0x43},
	{0x0089, 0x00},
	{0x0004, 0x0f},
	{0x0004, 0x0f},
	{0x0036, 0x00},
	{0x0037, 0x00},
	{0x0038, 0x00},
	{0x0039, 0x00},
	{0x003a, 0x00},
	{0x003b, 0x00},
	{0x003c, 0x00},
	{0x003d, 0x00},
	{0x003e, 0x00},
	{0x003f, 0x00},
	{0x0082, 0x00},
	{0x0083, 0x00},
	{0x0060, 0x20},
	{0x0002, 0x09},
	{0x000f, 0x00},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x08},
	{0x009b, 0x08},
	{0x0222, 0x41},
	{0x023b, 0x00},
	{0x023c, 0x06},
	{0x0216, 0x36},
	{0x0209, 0x09},
	{0x0248, 0x40},
	{0x0001, 0x0c},
	{0x00b0, 0x06},
	{0x00b1, 0x10},
	{0x00c0, 0x02},
	{0x00c1, 0x20},
	{0x00d0, 0x06},
	{0x00d1, 0x10},
	{0x00b2, 0x08},
	{0x00b3, 0x0e},
	{0x00c2, 0x04},
	{0x00c3, 0x60},
	{0x00d2, 0x06},
	{0x00d3, 0x10},
	{0x00b4, 0x0a},
	{0x00b5, 0x10},
	{0x00c4, 0x04},
	{0x00c5, 0x30},
	{0x00d4, 0x06},
	{0x00d5, 0x10},
	{0x00b6, 0x06},
	{0x00b7, 0x10},
	{0x00c6, 0x04},
	{0x00c7, 0x10},
	{0x00d6, 0x06},
	{0x00d7, 0x10},
	{0x00b8, 0x06},
	{0x00b9, 0x10},
	{0x00c8, 0x06},
	{0x00c9, 0x20},
	{0x00d8, 0x06},
	{0x00d9, 0x10},
	{0x00ba, 0x06},
	{0x00bb, 0x10},
	{0x00ca, 0x06},
	{0x00cb, 0x00},
	{0x00da, 0x06},
	{0x00db, 0x10},
	{0x00bc, 0x06},
	{0x00bd, 0x10},
	{0x00cc, 0x08},
	{0x00cd, 0x10},
	{0x00dc, 0x06},
	{0x00dd, 0x10},
	{0x00be, 0x06},
	{0x00bf, 0x10},
	{0x00ce, 0x08},
	{0x00cf, 0x08},
	{0x00de, 0x06},
	{0x00df, 0x10},
	{0x0008, 0x60},
	{0x0009, 0x58},
	{0x000a, 0x47},
	{0x000b, 0x36},
	{0x0051, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x044c, 0x76},
	{0x044d, 0x76},
	{0x044e, 0x76},
	{0x044f, 0x76},
	{0x0448, 0x02},
	{0x0449, 0x02},
	{0x044a, 0x02},
	{0x044b, 0x02},
	{0x0454, 0x76},
	{0x0455, 0x76},
	{0x0456, 0x76},
	{0x0457, 0x76},
	{0x0450, 0x02},
	{0x0451, 0x02},
	{0x0452, 0x02},
	{0x0453, 0x02},
	{0x0468, 0x00},
	{0x0046, 0x20},
	{0x0c22, 0x18},
	{0x0c19, 0x04},
	{0x0c1a, 0x4c},
	{0x0c1b, 0xd8},
	{0x0c20, 0x01},
	{0x0c21, 0x00},
	{0x0202, 0x04},
	{0x0203, 0x00},
	{0x005d, 0x80},
	{0x0089, 0x00},
	{0x02ab, 0x00},
	{0x02a9, 0x00},
	{0x0293, 0x00},
	{0x0294, 0x00},
	{0x0e85, 0x3e},
	{0x0e82, 0x3e},
	{0x0e5c, 0x3e},
	{0x0e56, 0x3e},
	{0x0e77, 0x00},
	{0x0e76, 0x00},
	{0x0075, 0x48},
	{0x0800, 0x01},
	{0x0810, 0x01},
	{0x0810, 0x00},
	{0x0801, 0xff},
	{0x0803, 0xc0},
	{0x080b, 0x56},
	{0x0880, 0x00},
	{0x0881, 0x00},
	{0x0885, 0x00},
	{0x0886, 0x40},
	{0x0885, 0x00},
	{0x0886, 0x58},
	{0x0885, 0x00},
	{0x0886, 0x7f},
	{0x0885, 0x00},
	{0x0886, 0x83},
	{0x0885, 0x00},
	{0x0886, 0xc0},
	{0x0885, 0x01},
	{0x0886, 0x12},
	{0x0885, 0x01},
	{0x0886, 0x7a},
	{0x0885, 0x02},
	{0x0886, 0x15},
	{0x0885, 0x02},
	{0x0886, 0xd9},
	{0x0885, 0x03},
	{0x0886, 0xf7},
	{0x0885, 0x05},
	{0x0886, 0x5c},
	{0x0885, 0x07},
	{0x0886, 0xd3},
	{0x0885, 0x0a},
	{0x0886, 0xb0},
	{0x0885, 0x0e},
	{0x0886, 0xdf},
	{0x0885, 0x14},
	{0x0886, 0x25},
	{0x0880, 0x00},
	{0x0881, 0x0f},
	{0x0885, 0x02},
	{0x0886, 0xab},
	{0x0885, 0x02},
	{0x0886, 0xa9},
	{0x0885, 0x0e},
	{0x0886, 0x76},
	{0x0885, 0x0e},
	{0x0886, 0x16},
	{0x0885, 0x0e},
	{0x0886, 0x44},
	{0x0885, 0x02},
	{0x0886, 0xcd},
	{0x0880, 0x00},
	{0x0881, 0x15},
	{0x0885, 0x00},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x03},
	{0x0885, 0x55},
	{0x0886, 0x3b},
	{0x0880, 0x00},
	{0x0881, 0x18},
	{0x0885, 0x00},
	{0x0886, 0x01},
	{0x0885, 0x06},
	{0x0886, 0x03},
	{0x0885, 0x55},
	{0x0886, 0x39},
	{0x0880, 0x00},
	{0x0881, 0x1b},
	{0x0885, 0x00},
	{0x0886, 0x02},
	{0x0885, 0x06},
	{0x0886, 0x03},
	{0x0885, 0x55},
	{0x0886, 0x37},
	{0x0880, 0x00},
	{0x0881, 0x1e},
	{0x0885, 0x00},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x3b},
	{0x0880, 0x00},
	{0x0881, 0x21},
	{0x0885, 0x00},
	{0x0886, 0x01},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x39},
	{0x0880, 0x00},
	{0x0881, 0x24},
	{0x0885, 0x00},
	{0x0886, 0x02},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x37},
	{0x0880, 0x00},
	{0x0881, 0x27},
	{0x0885, 0x00},
	{0x0886, 0x03},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x34},
	{0x0880, 0x00},
	{0x0881, 0x2a},
	{0x0885, 0x00},
	{0x0886, 0x04},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x31},
	{0x0880, 0x00},
	{0x0881, 0x2d},
	{0x0885, 0x00},
	{0x0886, 0x05},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2d},
	{0x0880, 0x00},
	{0x0881, 0x30},
	{0x0885, 0x00},
	{0x0886, 0x06},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2a},
	{0x0880, 0x00},
	{0x0881, 0x33},
	{0x0885, 0x00},
	{0x0886, 0x07},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x27},
	{0x0880, 0x00},
	{0x0881, 0x36},
	{0x0885, 0x01},
	{0x0886, 0x04},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x23},
	{0x0880, 0x00},
	{0x0881, 0x39},
	{0x0885, 0x01},
	{0x0886, 0x05},
	{0x0885, 0x03},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2a},
	{0x0880, 0x00},
	{0x0881, 0x3c},
	{0x0885, 0x01},
	{0x0886, 0x06},
	{0x0885, 0x02},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2c},
	{0x0880, 0x00},
	{0x0881, 0x3f},
	{0x0885, 0x01},
	{0x0886, 0x07},
	{0x0885, 0x01},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2f},
	{0x080c, 0x0f},
	{0x080d, 0x42},
	{0x0880, 0x00},
	{0x0881, 0x42},
	{0x0885, 0x02},
	{0x0886, 0x93},
	{0x0885, 0x02},
	{0x0886, 0x94},
	{0x0885, 0x0e},
	{0x0886, 0x77},
	{0x0885, 0x0e},
	{0x0886, 0x75},
	{0x0885, 0x0e},
	{0x0886, 0x78},
	{0x0885, 0x02},
	{0x0886, 0x75},
	{0x0880, 0x00},
	{0x0881, 0x48},
	{0x0885, 0x00},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x03},
	{0x0885, 0x55},
	{0x0886, 0x3b},
	{0x0880, 0x00},
	{0x0881, 0x4b},
	{0x0885, 0x01},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x03},
	{0x0885, 0x55},
	{0x0886, 0x39},
	{0x0880, 0x00},
	{0x0881, 0x4e},
	{0x0885, 0x02},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x03},
	{0x0885, 0x55},
	{0x0886, 0x37},
	{0x0880, 0x00},
	{0x0881, 0x51},
	{0x0885, 0x00},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x3b},
	{0x0880, 0x00},
	{0x0881, 0x54},
	{0x0885, 0x01},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x39},
	{0x0880, 0x00},
	{0x0881, 0x57},
	{0x0885, 0x02},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x37},
	{0x0880, 0x00},
	{0x0881, 0x5a},
	{0x0885, 0x03},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x34},
	{0x0880, 0x00},
	{0x0881, 0x5d},
	{0x0885, 0x04},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x31},
	{0x0880, 0x00},
	{0x0881, 0x60},
	{0x0885, 0x05},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2d},
	{0x0880, 0x00},
	{0x0881, 0x63},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2a},
	{0x0880, 0x00},
	{0x0881, 0x66},
	{0x0885, 0x07},
	{0x0886, 0x00},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x27},
	{0x0880, 0x00},
	{0x0881, 0x69},
	{0x0885, 0x04},
	{0x0886, 0x10},
	{0x0885, 0x06},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x23},
	{0x0880, 0x00},
	{0x0881, 0x6c},
	{0x0885, 0x05},
	{0x0886, 0x10},
	{0x0885, 0x03},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2a},
	{0x0880, 0x00},
	{0x0881, 0x6f},
	{0x0885, 0x06},
	{0x0886, 0x10},
	{0x0885, 0x02},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2c},
	{0x0880, 0x00},
	{0x0881, 0x72},
	{0x0885, 0x07},
	{0x0886, 0x10},
	{0x0885, 0x01},
	{0x0886, 0x00},
	{0x0885, 0x55},
	{0x0886, 0x2f},
	{0x0803, 0xc1},
	{0x0807, 0x00},
	{0x0808, 0x40},
	{0x0809, 0x00},
	{0x080a, 0x40},
	{0x0a67, 0x80},
	{0x0a4e, 0x0c},
	{0x0a4f, 0x0c},
	{0x0a54, 0x04},
	{0x0a55, 0x04},
	{0x0a53, 0x00},
	{0x0a7f, 0x05},
	{0x0a7e, 0x0a},
	{0x0a81, 0x0a},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x002d, 0x10},
	{0x0021, 0x44},
	{0x0a90, 0x83},
	{0x0a91, 0x02},
	{0x0a93, 0xe0},
	{0x0a97, 0x01},
	{0x0a98, 0x08},
	{0x0028, 0x0f},
	{0x0029, 0x10},
	{0x002a, 0x08},
	{0x002b, 0x80},
	{0x0a9e, 0x0f},
	{0x0a9f, 0x10},
	{0x0a9b, 0x08},
	{0x0a9c, 0x80},
	{0x0022, 0x00},
	{0x0023, 0x00},
	{0x0024, 0x00},
	{0x0025, 0x00},
	{0x0a5a, 0x80},
	{REG_DELAY, 0x14}, // sleep 20
	{0x05be, 0x01},
	{0x0080, 0x02},
	{0x0020, 0x8c},
	{0x039c, 0x02},
	{0x08e9, 0x01},
	{0x039a, 0x0f},
	{0x0391, 0x3c},
	{0x0392, 0x3c},
	{0x0393, 0x3c},
	{0x0394, 0x3c},
	{0x08ea, 0x00},
	{0x08ea, 0x3c},
	{0x08ea, 0x78},
	{0x08eb, 0x01},
	{0x08ec, 0x38},
	{0x08ed, 0x0a},
	{0x08eb, 0x07},
	{0x08ec, 0x31},
	{0x08ed, 0x03},
	{0x08eb, 0x03},
	{0x08ec, 0x34},
	{0x08ed, 0x0a},
	{0x08eb, 0x71},
	{0x08ec, 0x33},
	{0x08ed, 0x0a},
	{0x08eb, 0x76},
	{0x08ec, 0x21},
	{0x08ed, 0x0a},
	{0x08eb, 0x77},
	{0x08ec, 0x21},
	{0x08ed, 0x0a},
	{0x08eb, 0x01},
	{0x08ec, 0x36},
	{0x08ed, 0x03},
	{0x08eb, 0x00},
	{0x08ec, 0x36},
	{0x08ed, 0x03},
	{0x08eb, 0x09},
	{0x08ec, 0x00},
	{0x08ed, 0x01},
	{0x08ea, 0xb4},
	{0x08eb, 0x08},
	{0x08ec, 0x00},
	{0x08ed, 0x01},
	{0x08eb, 0x73},
	{0x08ec, 0x21},
	{0x08ed, 0x0a},
	{0x08eb, 0x72},
	{0x08ec, 0x21},
	{0x08ed, 0x0a},
	{0x08eb, 0x70},
	{0x08ec, 0x33},
	{0x08ed, 0x0a},
	{0x08eb, 0x02},
	{0x08ec, 0x34},
	{0x08ed, 0x0a},
	{0x08eb, 0x00},
	{0x08ec, 0x31},
	{0x08ed, 0x03},
	{0x08eb, 0x00},
	{0x08ec, 0x38},
	{0x08ed, 0x0a},
	{0x039c, 0x03},
	{0x0100, 0x09},
	{0x0107, 0x89},
	{0x0114, 0x03},
	{0x015a, 0x92},
	{0x015b, 0x4a},
	{0x0111, 0x2b},
	{0x0112, 0x01},
	{0x010d, 0x12},
	{0x010e, 0xc0},
	{0x0124, 0x04},
	{0x0122, 0x0e},
	{0x0123, 0x6b},
	{0x0126, 0x0f},
	{0x0121, 0x1b},
	{0x0129, 0x0d},
	{0x012a, 0x23},
	{0x012b, 0x0f},
	{0x0327, 0x46},
	{0x0336, 0x01},
	{0x0336, 0x00},
	{0x03fe, 0x00},
	{REG_NULL, 0x00},
};

static const struct gc8602_mode supported_modes[] = {
	{
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0100,
		.hts_def = 0x01f4,   // from 0x0342/0x0343 = 0x01f4
		.vts_def = 0x1194,    // from 0x0340/0x0341 = 0x1194
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = gc8602_linear_10bit_3840x2160_30fps_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
		.mipi_freq_idx = 0,
		.bpp = 10,
	},
	{
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0100,
		.hts_def = 0x01f4,   // from HDR mode file: 0x0342/0x0343 = 0x01f4
		.vts_def = 0x08ca,    // from HDR mode file: 0x0340/0x0341 = 0x08ca
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = gc8602_hdr_10bit_3840x2160_30fps_regs,
		.hdr_mode = HDR_X2,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,      // long exposure VC
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,      // short exposure VC
		.mipi_freq_idx = 1,
		.bpp = 10,
	},
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SRGGB10_1X10,
};

static const s64 link_freq_menu_items[] = {
	GC8602_LINK_FREQ_LINEAR,
	GC8602_LINK_FREQ_HDR,
};

static const char *const gc8602_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Helper functions for I2C and register writes (same as gc8613) */
static int gc8602_write_reg(struct i2c_client *client, u16 reg,
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

static int gc8602_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		if (regs[i].addr == REG_DELAY) {
			usleep_range(regs[i].val * 1000,
				     regs[i].val * 1000 + 500);
			continue;
		}

		ret = gc8602_write_reg(client, regs[i].addr,
				       GC8602_REG_VALUE_08BIT, regs[i].val);
	}

	return ret;
}

static int gc8602_read_reg(struct i2c_client *client, u16 reg,
			   unsigned int len, u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

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

/* Gain and exposure setting functions */
static int gc8602_set_gain_linear(struct gc8602 *gc8602, u32 gain)
{
	int ret;

	if (gain < GC8602_GAIN_MIN)
		gain = GC8602_GAIN_MIN;
	if (gain > GC8602_GAIN_MAX)
		gain = GC8602_GAIN_MAX;

	ret = gc8602_write_reg(gc8602->client, GC8602_REG_GAIN_T1_H,
			       GC8602_REG_VALUE_08BIT, gain >> 8);
	if (ret)
		return ret;

	return gc8602_write_reg(gc8602->client, GC8602_REG_GAIN_T1_L,
				GC8602_REG_VALUE_08BIT, gain & 0xff);
}

static int gc8602_set_gain_hdr(struct gc8602 *gc8602, u32 t1_gain, u32 t2_gain)
{
	int ret;

	if (t1_gain < GC8602_GAIN_MIN)
		t1_gain = GC8602_GAIN_MIN;
	if (t1_gain > GC8602_GAIN_MAX)
		t1_gain = GC8602_GAIN_MAX;
	if (t2_gain < GC8602_GAIN_MIN)
		t2_gain = GC8602_GAIN_MIN;
	if (t2_gain > GC8602_GAIN_MAX)
		t2_gain = GC8602_GAIN_MAX;

	ret = gc8602_write_reg(gc8602->client, GC8602_REG_GAIN_T1_H,
			       GC8602_REG_VALUE_08BIT, t1_gain >> 8);
	if (ret)
		return ret;
	ret = gc8602_write_reg(gc8602->client, GC8602_REG_GAIN_T1_L,
			       GC8602_REG_VALUE_08BIT, t1_gain & 0xff);
	if (ret)
		return ret;
	ret = gc8602_write_reg(gc8602->client, GC8602_REG_GAIN_T2_H,
			       GC8602_REG_VALUE_08BIT, t2_gain >> 8);
	if (ret)
		return ret;

	return gc8602_write_reg(gc8602->client, GC8602_REG_GAIN_T2_L,
				GC8602_REG_VALUE_08BIT, t2_gain & 0xff);
}

static int gc8602_set_exposure_linear(struct gc8602 *gc8602, u32 exp)
{
	u32 max_exp;
	int ret;

	max_exp = max_t(u32, gc8602->cur_vts,
			GC8602_EXPOSURE_MIN + GC8602_EXPOSURE_MARGIN);
	max_exp -= GC8602_EXPOSURE_MARGIN;

	if (exp < GC8602_EXPOSURE_MIN)
		exp = GC8602_EXPOSURE_MIN;
	if (exp > max_exp)
		exp = max_exp;

	ret = gc8602_write_reg(gc8602->client, GC8602_REG_EXPOSURE_H,
			       GC8602_REG_VALUE_08BIT, exp >> 8);
	if (ret)
		return ret;

	return gc8602_write_reg(gc8602->client, GC8602_REG_EXPOSURE_L,
				GC8602_REG_VALUE_08BIT, exp & 0xff);
}

static int gc8602_set_exposure_hdr(struct gc8602 *gc8602, u32 long_exp,
				   u32 short_exp)
{
	u32 max_long_exp;
	u32 vblank = 0;
	int ret;

	if (gc8602->cur_vts > gc8602->cur_mode->height)
		vblank = gc8602->cur_vts - gc8602->cur_mode->height;
	vblank = max_t(u32, vblank, GC8602_EXPOSURE_MIN);

	if (long_exp < GC8602_EXPOSURE_MIN)
		long_exp = GC8602_EXPOSURE_MIN;
	if (short_exp < GC8602_EXPOSURE_MIN)
		short_exp = GC8602_EXPOSURE_MIN;
	if (short_exp > vblank)
		short_exp = vblank;

	if (gc8602->cur_vts > short_exp)
		max_long_exp = gc8602->cur_vts - short_exp;
	else
		max_long_exp = GC8602_EXPOSURE_MIN;
	max_long_exp = max_t(u32, max_long_exp, GC8602_EXPOSURE_MIN);
	if (long_exp > max_long_exp)
		long_exp = max_long_exp;

	ret = gc8602_write_reg(gc8602->client, GC8602_REG_EXPOSURE_H,
			       GC8602_REG_VALUE_08BIT, long_exp >> 8);
	if (ret)
		return ret;
	ret = gc8602_write_reg(gc8602->client, GC8602_REG_EXPOSURE_L,
			       GC8602_REG_VALUE_08BIT, long_exp & 0xff);
	if (ret)
		return ret;
	ret = gc8602_write_reg(gc8602->client, GC8602_REG_SEXPOSURE_H,
			       GC8602_REG_VALUE_08BIT, short_exp >> 8);
	if (ret)
		return ret;

	return gc8602_write_reg(gc8602->client, GC8602_REG_SEXPOSURE_L,
				GC8602_REG_VALUE_08BIT, short_exp & 0xff);
}

static int gc8602_set_hdrae(struct gc8602 *gc8602,
			    struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, s_exp_time;
	u32 l_a_gain, s_a_gain;

	if (!ae)
		return -EINVAL;

	if (!gc8602->has_init_exp && !gc8602->streaming) {
		gc8602->init_hdrae_exp = *ae;
		gc8602->has_init_exp = true;
		dev_dbg(&gc8602->client->dev,
			"gc8602 is not streaming, record exp for hdr\n");
		return ret;
	}

	l_exp_time = ae->long_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&gc8602->client->dev,
		"rev exp req: L_exp: 0x%x, S_exp 0x%x, l_gain:0x%x, s_gain:0x%x\n",
		l_exp_time, s_exp_time, l_a_gain, s_a_gain);

	/* For HDR_X2, we only have long and short exposure */
	ret = gc8602_set_exposure_hdr(gc8602, l_exp_time, s_exp_time);
	if (ret)
		return ret;

	return gc8602_set_gain_hdr(gc8602, l_a_gain, s_a_gain);
}

static int gc8602_set_gain_reg(struct gc8602 *gc8602, u32 gain)
{
	/* Linear mode only */
	return gc8602_set_gain_linear(gc8602, gain);
}

/* V4L2 subdev operations (similar to gc8613) */
static int gc8602_get_reso_dist(const struct gc8602_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct gc8602_mode *
gc8602_find_best_fit(struct gc8602 *gc8602, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < gc8602->cfg_num; i++) {
		dist = gc8602_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		} else if (dist == cur_best_fit_dist &&
			   framefmt->code == supported_modes[i].bus_fmt) {
			cur_best_fit = i;
			break;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int gc8602_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc8602 *gc8602 = to_gc8602(sd);
	const struct gc8602_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&gc8602->mutex);

	mode = gc8602_find_best_fit(gc8602, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gc8602->mutex);
		return -ENOTTY;
#endif
	} else {
		gc8602->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc8602->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(gc8602->vblank, vblank_def,
					 GC8602_VTS_MAX - mode->height,
					 1, vblank_def);

		if (mode->hdr_mode == HDR_X2) {
			gc8602->cur_link_freq = 1;
			gc8602->cur_pixel_rate = GC8602_PIXEL_RATE_HDR;
		} else {
			gc8602->cur_link_freq = 0;
			gc8602->cur_pixel_rate = GC8602_PIXEL_RATE_LINEAR;
		}
		__v4l2_ctrl_s_ctrl_int64(gc8602->pixel_rate,
					 gc8602->cur_pixel_rate);
		__v4l2_ctrl_s_ctrl(gc8602->link_freq,
				   gc8602->cur_link_freq);
		gc8602->cur_vts = mode->vts_def;
		gc8602->cur_fps = mode->max_fps;
	}
	mutex_unlock(&gc8602->mutex);

	return 0;
}

static int gc8602_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc8602 *gc8602 = to_gc8602(sd);
	const struct gc8602_mode *mode = gc8602->cur_mode;

	mutex_lock(&gc8602->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gc8602->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&gc8602->mutex);

	return 0;
}

static int gc8602_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int gc8602_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct gc8602 *gc8602 = to_gc8602(sd);

	if (fse->index >= gc8602->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int gc8602_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum *fie)
{
	struct gc8602 *gc8602 = to_gc8602(sd);

	if (fie->index >= gc8602->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

static int gc8602_enable_test_pattern(struct gc8602 *gc8602, u32 pattern)
{
	u32 val;

	if (pattern)
		val = GC8602_TEST_PATTERN_ENABLE;
	else
		val = GC8602_TEST_PATTERN_DISABLE;

	return gc8602_write_reg(gc8602->client, GC8602_REG_TEST_PATTERN,
				GC8602_REG_VALUE_08BIT, val);
}

static int gc8602_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc8602 *gc8602 = to_gc8602(sd);
	const struct gc8602_mode *mode;

	mutex_lock(&gc8602->mutex);
	mode = gc8602->cur_mode;
	if (gc8602->streaming)
		fi->interval = gc8602->cur_fps;
	else
		fi->interval = mode->max_fps;
	mutex_unlock(&gc8602->mutex);

	return 0;
}

static const struct gc8602_mode *gc8602_find_mode(struct gc8602 *gc8602, int fps)
{
	const struct gc8602_mode *mode = NULL;
	const struct gc8602_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < gc8602->cfg_num; i++) {
		mode = &supported_modes[i];
		if (mode->width == gc8602->cur_mode->width &&
		    mode->height == gc8602->cur_mode->height &&
		    mode->hdr_mode == gc8602->cur_mode->hdr_mode &&
		    mode->bus_fmt == gc8602->cur_mode->bus_fmt) {
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

static int gc8602_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc8602 *gc8602 = to_gc8602(sd);
	const struct gc8602_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	int ret = 0;
	int fps;

	mutex_lock(&gc8602->mutex);

	if (gc8602->streaming) {
		ret = -EBUSY;
		goto unlock;
	}

	if (fi->pad != 0) {
		ret = -EINVAL;
		goto unlock;
	}

	if (fract->numerator == 0 || fract->denominator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		ret = -EINVAL;
		goto unlock;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = gc8602_find_mode(gc8602, fps);
	if (!mode) {
		v4l2_err(sd, "couldn't match fi\n");
		ret = -EINVAL;
		goto unlock;
	}

	gc8602->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(gc8602->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(gc8602->vblank, vblank_def,
				 GC8602_VTS_MAX - mode->height,
				 1, vblank_def);
	if (mode->hdr_mode == HDR_X2) {
		gc8602->cur_link_freq = 1;
		gc8602->cur_pixel_rate = GC8602_PIXEL_RATE_HDR;
	} else {
		gc8602->cur_link_freq = 0;
		gc8602->cur_pixel_rate = GC8602_PIXEL_RATE_LINEAR;
	}

	__v4l2_ctrl_s_ctrl_int64(gc8602->pixel_rate,
				 gc8602->cur_pixel_rate);
	__v4l2_ctrl_s_ctrl(gc8602->link_freq,
			   gc8602->cur_link_freq);
	gc8602->cur_fps = mode->max_fps;

unlock:
	mutex_unlock(&gc8602->mutex);

	return ret;
}

static int gc8602_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = GC8602_LANES;

	return 0;
}

static void gc8602_get_module_inf(struct gc8602 *gc8602,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, GC8602_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, gc8602->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, gc8602->len_name, sizeof(inf->base.lens));
}

static int gc8602_get_channel_info(struct gc8602 *gc8602, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = gc8602->cur_mode->vc[ch_info->index];
	ch_info->width = gc8602->cur_mode->width;
	ch_info->height = gc8602->cur_mode->height;
	ch_info->bus_fmt = gc8602->cur_mode->bus_fmt;
	return 0;
}

static long gc8602_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc8602 *gc8602 = to_gc8602(sd);
	struct rkmodule_hdr_cfg *hdr;
	u32 i, h, w;
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;
	int cur_best_fit = -1;
	int cur_best_fit_dist = -1;
	int cur_dist, cur_fps, dst_fps;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gc8602_get_module_inf(gc8602, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = gc8602->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		if (hdr->hdr_mode == gc8602->cur_mode->hdr_mode)
			return 0;
		w = gc8602->cur_mode->width;
		h = gc8602->cur_mode->height;
		dst_fps = DIV_ROUND_CLOSEST(gc8602->cur_mode->max_fps.denominator,
					    gc8602->cur_mode->max_fps.numerator);
		for (i = 0; i < gc8602->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode &&
			    supported_modes[i].bus_fmt == gc8602->cur_mode->bus_fmt) {
				cur_fps = DIV_ROUND_CLOSEST(supported_modes[i].max_fps.denominator,
							    supported_modes[i].max_fps.numerator);
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
			dev_err(&gc8602->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			gc8602->cur_mode = &supported_modes[cur_best_fit];
			w = gc8602->cur_mode->hts_def -
			    gc8602->cur_mode->width;
			h = gc8602->cur_mode->vts_def -
			    gc8602->cur_mode->height;
			__v4l2_ctrl_modify_range(gc8602->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(gc8602->vblank, h,
						 GC8602_VTS_MAX -
						 gc8602->cur_mode->height,
						 1, h);
			if (gc8602->cur_mode->hdr_mode == HDR_X2) {
				gc8602->cur_link_freq = 1;
				gc8602->cur_pixel_rate = GC8602_PIXEL_RATE_HDR;
			} else {
				gc8602->cur_link_freq = 0;
				gc8602->cur_pixel_rate = GC8602_PIXEL_RATE_LINEAR;
			}
			__v4l2_ctrl_s_ctrl_int64(gc8602->pixel_rate,
						 gc8602->cur_pixel_rate);
			__v4l2_ctrl_s_ctrl(gc8602->link_freq,
					   gc8602->cur_link_freq);
			gc8602->cur_vts = gc8602->cur_mode->vts_def;
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		ret = gc8602_set_hdrae(gc8602, arg);
		if (!ret && gc8602->cam_sw_inf)
			memcpy(&gc8602->cam_sw_inf->hdr_ae,
			       (struct preisp_hdrae_exp_s *)(arg),
			       sizeof(struct preisp_hdrae_exp_s));
		break;
	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);

		if (stream)
			ret = gc8602_write_reg(gc8602->client, GC8602_REG_CTRL_MODE,
					       GC8602_REG_VALUE_08BIT, GC8602_MODE_STREAMING);
		else
			ret = gc8602_write_reg(gc8602->client, GC8602_REG_CTRL_MODE,
					       GC8602_REG_VALUE_08BIT, GC8602_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = gc8602_get_channel_info(gc8602, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gc8602_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	long ret;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc8602_ioctl(sd, cmd, inf);
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
			ret = gc8602_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc8602_ioctl(sd, cmd, hdr);
		if (!ret) {
			ret = copy_to_user(up, hdr, sizeof(*hdr));
			if (ret)
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
			ret = gc8602_ioctl(sd, cmd, hdr);
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
			ret = gc8602_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = gc8602_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc8602_ioctl(sd, cmd, ch_info);
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

static int __gc8602_start_stream(struct gc8602 *gc8602)
{
	int ret;

	dev_dbg(&gc8602->client->dev,
		"start stream, width=%u, height=%u, hdr_mode=%u\n",
		gc8602->cur_mode->width, gc8602->cur_mode->height,
		gc8602->cur_mode->hdr_mode);

	if (!gc8602->is_thunderboot) {
		ret = gc8602_write_array(gc8602->client, gc8602->cur_mode->reg_list);
		if (ret)
			return ret;

		dev_dbg(&gc8602->client->dev,
			"write reg array done, start stream\n");

		ret = __v4l2_ctrl_handler_setup(&gc8602->ctrl_handler);
		if (ret)
			return ret;
		if (gc8602->has_init_exp && gc8602->cur_mode->hdr_mode != NO_HDR) {
			ret = gc8602_ioctl(&gc8602->subdev, PREISP_CMD_SET_HDRAE_EXP,
					   &gc8602->init_hdrae_exp);
			if (ret) {
				dev_err(&gc8602->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	} else {
		dev_dbg(&gc8602->client->dev,
			"thunderboot mode, just streaming\n");
	}
	dev_dbg(&gc8602->client->dev,
		"__v4l2_ctrl_handler_setup done, ready to start stream\n");

	ret = gc8602_write_reg(gc8602->client, GC8602_REG_CTRL_MODE,
			       GC8602_REG_VALUE_08BIT, GC8602_MODE_STREAMING);

	dev_dbg(&gc8602->client->dev,
		"write stream done, streaming, ret: %d\n", ret);

	return ret;
}

static int __gc8602_stop_stream(struct gc8602 *gc8602)
{
	gc8602->has_init_exp = false;
	return gc8602_write_reg(gc8602->client, GC8602_REG_CTRL_MODE,
				GC8602_REG_VALUE_08BIT, GC8602_MODE_SW_STANDBY);
}

static int __gc8602_power_on(struct gc8602 *gc8602);
static int gc8602_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc8602 *gc8602 = to_gc8602(sd);
	struct i2c_client *client = gc8602->client;
	int ret = 0;

	mutex_lock(&gc8602->mutex);
	on = !!on;
	if (on == gc8602->streaming)
		goto unlock_and_return;

	if (on) {
		if (gc8602->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			gc8602->is_thunderboot = false;
			ret = __gc8602_power_on(gc8602);
			if (ret)
				goto unlock_and_return;
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gc8602_start_stream(gc8602);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gc8602_stop_stream(gc8602);
		pm_runtime_put(&client->dev);
	}

	gc8602->streaming = on;

unlock_and_return:
	mutex_unlock(&gc8602->mutex);

	return ret;
}

static int gc8602_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc8602 *gc8602 = to_gc8602(sd);
	struct i2c_client *client = gc8602->client;
	int ret = 0;

	mutex_lock(&gc8602->mutex);

	if (gc8602->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gc8602->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		gc8602->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc8602->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 gc8602_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, GC8602_XVCLK_FREQ_24M / 1000 / 1000);
}

static int __gc8602_power_on(struct gc8602 *gc8602)
{
	int ret;
	u32 delay_us;
	struct device *dev = &gc8602->client->dev;

	if (!IS_ERR_OR_NULL(gc8602->pins_default)) {
		ret = pinctrl_select_state(gc8602->pinctrl,
					   gc8602->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(gc8602->xvclk, GC8602_XVCLK_FREQ_24M);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(gc8602->xvclk) != GC8602_XVCLK_FREQ_24M)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(gc8602->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	cam_sw_regulator_bulk_init(gc8602->cam_sw_inf,
				   GC8602_NUM_SUPPLIES, gc8602->supplies);

	if (gc8602->is_thunderboot)
		return 0;

	if (!IS_ERR(gc8602->reset_gpio))
		gpiod_set_value_cansleep(gc8602->reset_gpio, 0);

	if (!IS_ERR(gc8602->pwdn_gpio))
		gpiod_set_value_cansleep(gc8602->pwdn_gpio, 0);

	usleep_range(500, 1000);
	ret = regulator_bulk_enable(GC8602_NUM_SUPPLIES, gc8602->supplies);

	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(gc8602->pwren_gpio))
		gpiod_set_value_cansleep(gc8602->pwren_gpio, 1);

	usleep_range(1000, 1100);
	if (!IS_ERR(gc8602->pwdn_gpio))
		gpiod_set_value_cansleep(gc8602->pwdn_gpio, 1);
	usleep_range(100, 150);
	if (!IS_ERR(gc8602->reset_gpio))
		gpiod_set_value_cansleep(gc8602->reset_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = gc8602_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

err_clk:
	if (!IS_ERR(gc8602->reset_gpio))
		gpiod_direction_output(gc8602->reset_gpio, 0);
disable_clk:
	clk_disable_unprepare(gc8602->xvclk);

	return ret;
}

static void __gc8602_power_off(struct gc8602 *gc8602)
{
	int ret;
	struct device *dev = &gc8602->client->dev;

	clk_disable_unprepare(gc8602->xvclk);
	if (gc8602->is_thunderboot) {
		if (gc8602->is_first_streamoff) {
			gc8602->is_thunderboot = false;
			gc8602->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(gc8602->pwdn_gpio))
		gpiod_set_value_cansleep(gc8602->pwdn_gpio, 0);
	if (!IS_ERR(gc8602->reset_gpio))
		gpiod_set_value_cansleep(gc8602->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(gc8602->pins_sleep)) {
		ret = pinctrl_select_state(gc8602->pinctrl,
					   gc8602->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(GC8602_NUM_SUPPLIES, gc8602->supplies);
	if (!IS_ERR(gc8602->pwren_gpio))
		gpiod_set_value_cansleep(gc8602->pwren_gpio, 0);
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int gc8602_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8602 *gc8602 = to_gc8602(sd);

	cam_sw_prepare_wakeup(gc8602->cam_sw_inf, dev);

	usleep_range(4000, 5000);
	cam_sw_write_array(gc8602->cam_sw_inf);

	ret = __v4l2_ctrl_handler_setup(&gc8602->ctrl_handler);
	if (ret) {
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");
		return ret;
	}

	if (gc8602->has_init_exp && gc8602->cur_mode->hdr_mode != NO_HDR) {
		ret = gc8602_ioctl(&gc8602->subdev, PREISP_CMD_SET_HDRAE_EXP,
				   &gc8602->cam_sw_inf->hdr_ae);
		if (ret) {
			dev_err(&gc8602->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}
	return 0;
}

static int gc8602_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8602 *gc8602 = to_gc8602(sd);

	cam_sw_write_array_cb_init(gc8602->cam_sw_inf, client,
				   (void *)gc8602->cur_mode->reg_list,
				   (sensor_write_array)gc8602_write_array);
	cam_sw_prepare_sleep(gc8602->cam_sw_inf);

	return 0;
}
#else
#define gc8602_resume NULL
#define gc8602_suspend NULL
#endif

static int gc8602_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8602 *gc8602 = to_gc8602(sd);

	return __gc8602_power_on(gc8602);
}

static int gc8602_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8602 *gc8602 = to_gc8602(sd);

	__gc8602_power_off(gc8602);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc8602_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc8602 *gc8602 = to_gc8602(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gc8602_mode *def_mode = &supported_modes[0];

	mutex_lock(&gc8602->mutex);
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;
	mutex_unlock(&gc8602->mutex);

	return 0;
}
#endif

static const struct dev_pm_ops gc8602_pm_ops = {
	SET_RUNTIME_PM_OPS(gc8602_runtime_suspend,
			   gc8602_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(gc8602_suspend, gc8602_resume)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gc8602_internal_ops = {
	.open = gc8602_open,
};
#endif

static const struct v4l2_subdev_core_ops gc8602_core_ops = {
	.s_power = gc8602_s_power,
	.ioctl = gc8602_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc8602_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gc8602_video_ops = {
	.s_stream = gc8602_s_stream,
	.g_frame_interval = gc8602_g_frame_interval,
	.s_frame_interval = gc8602_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc8602_pad_ops = {
	.enum_mbus_code = gc8602_enum_mbus_code,
	.enum_frame_size = gc8602_enum_frame_sizes,
	.enum_frame_interval = gc8602_enum_frame_interval,
	.get_fmt = gc8602_get_fmt,
	.set_fmt = gc8602_set_fmt,
	.get_mbus_config = gc8602_g_mbus_config,
};

static const struct v4l2_subdev_ops gc8602_subdev_ops = {
	.core	= &gc8602_core_ops,
	.video	= &gc8602_video_ops,
	.pad	= &gc8602_pad_ops,
};

static void gc8602_modify_fps_info(struct gc8602 *gc8602)
{
	const struct gc8602_mode *mode = gc8602->cur_mode;

	gc8602->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				      gc8602->cur_vts;
}

static int gc8602_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc8602 *gc8602 = container_of(ctrl->handler,
					     struct gc8602, ctrl_handler);
	struct i2c_client *client = gc8602->client;
	s64 max;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		max = gc8602->cur_mode->height + ctrl->val -
		      GC8602_EXPOSURE_MARGIN;
		__v4l2_ctrl_modify_range(gc8602->exposure,
					 gc8602->exposure->minimum,
					 max,
					 gc8602->exposure->step,
					 gc8602->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (gc8602->cur_mode->hdr_mode != NO_HDR)
			break;
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		ret = gc8602_set_exposure_linear(gc8602, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (gc8602->cur_mode->hdr_mode != NO_HDR)
			break;
		dev_dbg(&client->dev, "set gain 0x%x\n", ctrl->val);
		ret = gc8602_set_gain_reg(gc8602, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		gc8602->cur_vts = ctrl->val + gc8602->cur_mode->height;
		ret = gc8602_write_reg(gc8602->client, GC8602_REG_VTS_H,
				       GC8602_REG_VALUE_08BIT,
				       gc8602->cur_vts >> 8);
		if (ret)
			break;
		ret = gc8602_write_reg(gc8602->client, GC8602_REG_VTS_L,
				       GC8602_REG_VALUE_08BIT,
				       gc8602->cur_vts & 0xff);
		if (ret)
			break;
		if (gc8602->cur_vts != gc8602->cur_mode->vts_def)
			gc8602_modify_fps_info(gc8602);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = gc8602_enable_test_pattern(gc8602, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gc8602_ctrl_ops = {
	.s_ctrl = gc8602_set_ctrl,
};

static int gc8602_initialize_controls(struct gc8602 *gc8602)
{
	const struct gc8602_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &gc8602->ctrl_handler;
	mode = gc8602->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &gc8602->mutex;

	gc8602->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
						   V4L2_CID_LINK_FREQ,
						   1, 0,
						   link_freq_menu_items);
	if (mode->hdr_mode == HDR_X2) {
		gc8602->cur_link_freq = 1;
		gc8602->cur_pixel_rate = GC8602_PIXEL_RATE_HDR;
	} else {
		gc8602->cur_link_freq = 0;
		gc8602->cur_pixel_rate = GC8602_PIXEL_RATE_LINEAR;
	}
	__v4l2_ctrl_s_ctrl(gc8602->link_freq, gc8602->cur_link_freq);

	gc8602->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
					       0, gc8602->cur_pixel_rate,
						   1, gc8602->cur_pixel_rate);

	h_blank = mode->hts_def - mode->width;
	gc8602->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (gc8602->hblank)
		gc8602->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	gc8602->cur_vts = mode->vts_def;
	gc8602->vblank = v4l2_ctrl_new_std(handler, &gc8602_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   GC8602_VTS_MAX - mode->height,
					   1, vblank_def);

	exposure_max = mode->vts_def - 8;
	gc8602->exposure = v4l2_ctrl_new_std(handler, &gc8602_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     GC8602_EXPOSURE_MIN,
					     exposure_max,
					     GC8602_EXPOSURE_STEP,
					     mode->exp_def);

	gc8602->anal_gain = v4l2_ctrl_new_std(handler, &gc8602_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN,
					      GC8602_GAIN_MIN,
					      GC8602_GAIN_MAX,
					      GC8602_GAIN_STEP,
					      GC8602_GAIN_DEFAULT);

	gc8602->test_pattern =
		v4l2_ctrl_new_std_menu_items(handler,
					     &gc8602_ctrl_ops,
					     V4L2_CID_TEST_PATTERN,
					     ARRAY_SIZE(gc8602_test_pattern_menu) - 1,
					     0, 0, gc8602_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_err(&gc8602->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc8602->subdev.ctrl_handler = handler;
	gc8602->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gc8602_check_sensor_id(struct gc8602 *gc8602,
				  struct i2c_client *client)
{
	struct device *dev = &gc8602->client->dev;
	u16 id = 0;
	u32 reg_H = 0;
	u32 reg_M = 0;
	int ret;

	if (gc8602->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = gc8602_read_reg(client, GC8602_REG_CHIP_ID_H,
			      GC8602_REG_VALUE_08BIT, &reg_H);
	if (ret) {
		dev_err(dev, "Failed to read sensor id high byte, ret(%d)\n", ret);
		return ret;
	}

	ret = gc8602_read_reg(client, GC8602_REG_CHIP_ID_M,
			      GC8602_REG_VALUE_08BIT, &reg_M);
	if (ret) {
		dev_err(dev, "Failed to read sensor id, ret(%d)\n", ret);
		return ret;
	}

	id = ((reg_H & 0xff) << 8) | (reg_M & 0xff);
	if (reg_H != (CHIP_ID >> 8) || reg_M != (CHIP_ID & 0xff)) {
		dev_err(dev, "Unexpected sensor id(%04x)\n", id);
		return -ENODEV;
	}
	dev_info(dev, "Detected gc8602 (0x%04x)\n", id);
	return 0;
}

static int gc8602_configure_regulators(struct gc8602 *gc8602)
{
	unsigned int i;

	for (i = 0; i < GC8602_NUM_SUPPLIES; i++)
		gc8602->supplies[i].supply = gc8602_supply_names[i];

	return devm_regulator_bulk_get(&gc8602->client->dev,
				       GC8602_NUM_SUPPLIES,
				       gc8602->supplies);
}

static int gc8602_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gc8602 *gc8602;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x\n",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	gc8602 = devm_kzalloc(dev, sizeof(*gc8602), GFP_KERNEL);
	if (!gc8602)
		return -ENOMEM;

	of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &gc8602->module_index);
	if (ret)
		goto err_module_info;
	ret = of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				      &gc8602->module_facing);
	if (ret)
		goto err_module_info;
	ret = of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				      &gc8602->module_name);
	if (ret)
		goto err_module_info;
	ret = of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				      &gc8602->len_name);
	if (ret)
		goto err_module_info;

	gc8602->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	gc8602->client = client;
	gc8602->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < gc8602->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			gc8602->cur_mode = &supported_modes[i];
			break;
		}
	}
	if (i == gc8602->cfg_num)
		gc8602->cur_mode = &supported_modes[0];

	gc8602->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(gc8602->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	gc8602->pwren_gpio = devm_gpiod_get(dev, "pwren",
					    gc8602->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(gc8602->pwren_gpio))
		dev_warn(dev, "Failed to get pwren-gpios\n");

	gc8602->reset_gpio = devm_gpiod_get(dev, "reset",
					    gc8602->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(gc8602->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	gc8602->pwdn_gpio = devm_gpiod_get(dev, "pwdn",
					   gc8602->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(gc8602->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	gc8602->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(gc8602->pinctrl)) {
		gc8602->pins_default =
			pinctrl_lookup_state(gc8602->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(gc8602->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		gc8602->pins_sleep =
			pinctrl_lookup_state(gc8602->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(gc8602->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = gc8602_configure_regulators(gc8602);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&gc8602->mutex);

	sd = &gc8602->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc8602_subdev_ops);
	ret = gc8602_initialize_controls(gc8602);
	if (ret)
		goto err_destroy_mutex;

	ret = __gc8602_power_on(gc8602);
	if (ret)
		goto err_free_handler;

	usleep_range(3000, 4000);

	ret = gc8602_check_sensor_id(gc8602, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc8602_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gc8602->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc8602->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	if (!gc8602->cam_sw_inf) {
		gc8602->cam_sw_inf = cam_sw_init();
		if (!gc8602->cam_sw_inf) {
			ret = -ENOMEM;
			goto err_clean_entity;
		}
		cam_sw_clk_init(gc8602->cam_sw_inf, gc8602->xvclk, GC8602_XVCLK_FREQ_24M);
		cam_sw_reset_pin_init(gc8602->cam_sw_inf, gc8602->reset_gpio, 0);
		cam_sw_pwdn_pin_init(gc8602->cam_sw_inf, gc8602->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc8602->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc8602->module_index, facing,
		 GC8602_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_deinit_cam_sw;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (gc8602->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_module_info:
	dev_err(dev, "could not get module information!\n");
	return -EINVAL;

err_deinit_cam_sw:
	cam_sw_deinit(gc8602->cam_sw_inf);
err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__gc8602_power_off(gc8602);
err_free_handler:
	v4l2_ctrl_handler_free(&gc8602->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc8602->mutex);

	return ret;
}

static void gc8602_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8602 *gc8602 = to_gc8602(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gc8602->ctrl_handler);
	mutex_destroy(&gc8602->mutex);

	cam_sw_deinit(gc8602->cam_sw_inf);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc8602_power_off(gc8602);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc8602_of_match[] = {
	{ .compatible = "galaxycore,gc8602" },
	{},
};
MODULE_DEVICE_TABLE(of, gc8602_of_match);
#endif

static const struct i2c_device_id gc8602_match_id[] = {
	{ "gc8602", 0 },
	{ },
};

static struct i2c_driver gc8602_i2c_driver = {
	.driver = {
		.name = GC8602_NAME,
		.pm = &gc8602_pm_ops,
		.of_match_table = of_match_ptr(gc8602_of_match),
	},
	.probe		= gc8602_probe,
	.remove		= gc8602_remove,
	.id_table	= gc8602_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc8602_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc8602_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("galaxycore gc8602 sensor driver");
MODULE_LICENSE("GPL");
