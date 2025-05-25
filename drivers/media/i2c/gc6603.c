// SPDX-License-Identifier: GPL-2.0
/*
 * GC6603 driver
 *
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 add poweron function.
 * V0.0X01.0X02 fix mclk issue when probe multiple camera.
 * V0.0X01.0X03 fix gain range.
 * V0.0X01.0X04 add enum_frame_interval function.
 * V0.0X01.0X05 support enum sensor fmt
 * V0.0X01.0X06 support mirror and flip
 * V0.0X01.0X07 add quick stream on/off
 */

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
#include <media/v4l2-fwnode.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_graph.h>

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x07)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define GC6603_LANES			2
#define GC6603_BITS_PER_SAMPLE		10
#define GC6603_LINK_FREQ_516M		(516000000 / 2)
#define GC6603_LINK_FREQ_816M		(816000000 / 2)
#define GC6603_LINK_FREQ_1032M		(1032000000 / 2)

#define GC6603_PIXEL_RATE_MAX		(GC6603_LINK_FREQ_816M * 2 / 10 * 4)

#define GC6603_XVCLK_FREQ		24000000

#define CHIP_ID				0x5623
#define GC6603_REG_CHIP_ID_H		0x03f2
#define GC6603_REG_CHIP_ID_L		0x03f3

#define GC6603_REG_CTRL_MODE		0x0100
#define GC6603_MODE_SW_STANDBY		0x00
#define GC6603_MODE_STREAMING		0x09

#define GC6603_REG_SEXPOSURE_H		0x0200
#define GC6603_REG_SEXPOSURE_L		0x0201
#define GC6603_REG_EXPOSURE_H		0x0202
#define GC6603_REG_EXPOSURE_L		0x0203
#define GC6603_EXPOSURE_MIN		2
#define GC6603_EXPOSURE_STEP		2
#define GC6603_VTS_MAX			0x7fff

#define GC6603_GAIN_MIN			64
#define GC6603_GAIN_MAX			0x20b0
#define GC6603_GAIN_STEP		1
#define GC6603_GAIN_DEFAULT		256

#define GC6603_REG_TEST_PATTERN		0x008c
#define GC6603_TEST_PATTERN_ENABLE	0x11
#define GC6603_TEST_PATTERN_DISABLE	0x10

#define GC6603_REG_VTS_H		0x0340
#define GC6603_REG_VTS_L		0x0341

#define GC6603_FLIP_MIRROR_REG		0x0101
#define GC6603_MIRROR_BIT_MASK		BIT(0)
#define GC6603_FLIP_BIT_MASK		BIT(1)

#define REG_NULL			0xFFFF
#define REG_DELAY			0xFFFE

#define GC6603_REG_VALUE_08BIT		1
#define GC6603_REG_VALUE_16BIT		2
#define GC6603_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define GC6603_NAME			"gc6603"

static const char * const gc6603_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
	"avdd",		/* Analog power */
};

#define GC6603_NUM_SUPPLIES ARRAY_SIZE(gc6603_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct gc6603_mode {
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
	u32 link_freq_idx;
	u32 bpp;
};

struct gc6603 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct gpio_desc	*pwren_gpio;
	struct regulator_bulk_data supplies[GC6603_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*h_flip;
	struct v4l2_ctrl	*v_flip;
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	const struct gc6603_mode *cur_mode;
	const struct gc6603_mode *supported_modes;
	u32			cfg_num;
	u32			module_index;
	u32			cur_vts;
	u32			cur_pixel_rate;
	u32			cur_link_freq;
	struct v4l2_fwnode_endpoint bus_cfg;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	bool			has_init_exp;
};

#define to_gc6603(sd) container_of(sd, struct gc6603, subdev)

static const u32 reg_val_table_hdr[8][8] = {
	//0914, 0915, 0916, 0917, 0225, 0e67, 0e68, 0242    |  CG | 实际倍数|Again dB|
	{ 0x01, 0x00, 0x01, 0x00, 0x0c, 0x11, 0x11, 0x65},//| LCG |   1.000 |   0.000  |
	{ 0x01, 0x00, 0x01, 0x00, 0x00, 0x11, 0x11, 0x65},//| HCG |   2.000 |   6.021  |
	{ 0x03, 0x00, 0x03, 0x00, 0x00, 0x12, 0x12, 0x65},//| HCG |   4.000 |  12.041  |
	{ 0x05, 0x00, 0x05, 0x00, 0x00, 0x14, 0x14, 0x65},//| HCG |   8.000 |  18.062  |
	{ 0x07, 0x00, 0x07, 0x00, 0x00, 0x19, 0x19, 0x65},//| HCG |  16.000 |  24.082  |
	{ 0x05, 0x00, 0x05, 0x00, 0x03, 0x1d, 0x1d, 0x75},//| HCG |  32.000 |  30.103  |
	{ 0x06, 0x00, 0x06, 0x00, 0x03, 0x1e, 0x1e, 0x85},//| HCG |  44.800 |  33.026  |
	{ 0x07, 0x00, 0x07, 0x00, 0x03, 0x22, 0x22, 0x85},//| HCG |  64.000 |  36.124  |
};

//max gain 64x
static const u32 gain_level_table_hdr[9] = {
	64  ,
	128 ,
	256 ,
	512 ,
	1024,
	2048,
	2867,
	4096,
	0xffffffff,
};

static const u8 reg_val_table_liner[29][6] = {
	//0914, 0915, 0225, 0e67, 0e68, 0242    |  CG | 实际倍数|Again dB|
	{ 0x01, 0x00, 0x04, 0x0f, 0x0f, 0x65},//| LCG |   1.000 |  0.000 |
	{ 0x01, 0x05, 0x04, 0x0f, 0x0f, 0x65},//| LCG |   1.176 |  1.408 |
	{ 0x21, 0x09, 0x04, 0x0f, 0x0f, 0x65},//| LCG |   1.362 |  2.682 |
	{ 0xb1, 0x0C, 0x04, 0x0f, 0x0f, 0x65},//| LCG |   1.637 |  4.279 |
	{ 0x01, 0x00, 0x00, 0x0f, 0x0f, 0x65},//| HCG |   2.048 |  6.227 |
	{ 0x01, 0x05, 0x00, 0x0f, 0x0f, 0x65},//| HCG |   2.475 |  7.871 |
	{ 0x21, 0x09, 0x00, 0x0f, 0x0f, 0x65},//| HCG |   2.944 |  9.379 |
	{ 0xb1, 0x0C, 0x00, 0x0f, 0x0f, 0x65},//| HCG |   3.491 | 10.859 |
	{ 0x03, 0x00, 0x00, 0x0f, 0x0f, 0x65},//| HCG |   4.230 | 12.526 |
	{ 0x03, 0x05, 0x00, 0x10, 0x10, 0x65},//| HCG |   4.983 | 13.950 |
	{ 0x23, 0x09, 0x00, 0x11, 0x11, 0x65},//| HCG |   5.847 | 15.338 |
	{ 0xb3, 0x0C, 0x00, 0x12, 0x12, 0x65},//| HCG |   6.941 | 16.829 |
	{ 0x03, 0x10, 0x00, 0x13, 0x13, 0x65},//| HCG |   8.329 | 18.412 |
	{ 0x05, 0x05, 0x00, 0x13, 0x13, 0x65},//| HCG |   9.943 | 19.950 |
	{ 0x25, 0x09, 0x00, 0x13, 0x13, 0x65},//| HCG |  11.834 | 21.462 |
	{ 0xb5, 0x0C, 0x00, 0x14, 0x14, 0x65},//| HCG |  13.948 | 22.890 |
	{ 0x05, 0x10, 0x00, 0x15, 0x15, 0x65},//| HCG |  16.655 | 24.431 |
	{ 0x85, 0x12, 0x00, 0x16, 0x16, 0x65},//| HCG |  19.766 | 25.918 |
	{ 0x95, 0x14, 0x00, 0x17, 0x17, 0x65},//| HCG |  23.248 | 27.328 |
	{ 0x65, 0x16, 0x00, 0x19, 0x19, 0x65},//| HCG |  27.568 | 28.808 |
	{ 0x05, 0x18, 0x00, 0x1a, 0x1a, 0x65},//| HCG |  33.393 | 30.473 |
	{ 0x05, 0x05, 0x01, 0x1b, 0x1b, 0x65},//| HCG |  38.624 | 31.737 |
	{ 0x25, 0x09, 0x01, 0x1c, 0x1c, 0x65},//| HCG |  45.930 | 33.242 |
	{ 0xb5, 0x0C, 0x01, 0x1c, 0x1c, 0x75},//| HCG |  55.021 | 34.811 |
	{ 0x05, 0x10, 0x01, 0x1d, 0x1d, 0x75},//| HCG |  65.578 | 36.335 |
	{ 0x85, 0x12, 0x01, 0x1e, 0x1e, 0x85},//| HCG |  77.942 | 37.835 |
	{ 0x95, 0x14, 0x01, 0x1e, 0x1e, 0x85},//| HCG |  92.419 | 39.315 |
	{ 0x65, 0x16, 0x01, 0x20, 0x20, 0x85},//| HCG | 108.822 | 40.734 |
	{ 0x05, 0x18, 0x01, 0x22, 0x22, 0x85},//| HCG | 130.760 | 42.330 |
};

//max gain 130x
static const u32 gain_level_table_linear[30] = {
	  64,
	  75,
	  87,
	 104,
	 131,
	 158,
	 188,
	 223,
	 270,
	 318,
	 374,
	 444,
	 533,
	 636,
	 757,
	 892,
	1065,
	1265,
	1487,
	1764,
	2137,
	2471,
	2939,
	3521,
	4196,
	4988,
	5914,
	6964,
	8368,
	0xffffffff,
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 1032Mbps, 2lane
 */
static const struct regval gc6603_linear10bit_2560x1440_2lane_regs[] = {
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03fe,0x10},
	{0x0938,0x01},
	{0x0360,0xfd},
	{0x091b,0x1a},
	{0x091c,0x18},
	{0x091e,0x00},
	{0x091d,0x06},
	{0x091f,0x81},
	{0x0920,0xa1},
	{0x0922,0x3a},
	{0x0923,0x10},
	{0x0928,0x00},
	{0x0934,0xb7},
	{0x0935,0x06},
	{0x0936,0x00},
	{0x0937,0x81},
	{0x031b,0x00},
	{0x031c,0x4f},
	{0x031e,0x00},
	{0x03e0,0x00},
	{0x0314,0x10},
	{0x0219,0x47},
	{0x022b,0x10},
	{0x0259,0x08},
	{0x025a,0x44},
	{0x025b,0x10},
	{0x0340,0x08},
	{0x0341,0x66},
	{0x0342,0x03},
	{0x0343,0xe8},
	{0x0346,0x00},
	{0x0347,0x40},
	{0x0348,0x0a},
	{0x0349,0x90},
	{0x034a,0x08},
	{0x034b,0x20},
	{0x034e,0x0a},
	{0x034f,0xc0},
	{0x070c,0x03},
	{0x070d,0x00},
	{0x070e,0x98},
	{0x070f,0x0a},
	{0x0053,0x05},
	{0x0099,0x10},
	{0x009b,0x08},
	{0x0094,0x0a},
	{0x0095,0x80},
	{0x0096,0x08},
	{0x0097,0x00},
	{0x0e4c,0x3c},
	{0x0902,0x0b},
	{0x0903,0x15},
	{0x0904,0x14},
	{0x0907,0x14},
	{0x0908,0x15},
	{0x090e,0x26},
	{0x090f,0x15},
	{0x0244,0x75},
	{0x0724,0x0c},
	{0x0727,0x0c},
	{0x072a,0x18},
	{0x072b,0x19},
	{0x0709,0x40},
	{0x0719,0x40},
	{0x0912,0x01},
	{0x0913,0x00},
	{0x0e66,0x10},
	{0x0e69,0x80},
	{0x0e6a,0xc0},
	{0x0e6b,0x02},
	{0x0223,0x00},
	{0x0e81,0x02},
	{0x0e30,0x00},
	{0x0e33,0x80},
	{0x0242,0x65},
	{0x0361,0xbc},
	{0x0362,0x0f},
	{0x0e34,0x04},
	{0x0e47,0x55},
	{0x0e61,0x0d},
	{0x0e62,0x0d},
	{0x023a,0x05},
	{0x0e64,0x0c},
	{0x0e20,0x0c},
	{0x0e6e,0x50},
	{0x0e6f,0x58},
	{0x0e70,0x24},
	{0x0e71,0x28},
	{0x0e28,0x38},
	{0x0e4d,0x80},
	{0x0245,0x08},
	{0x0240,0x06},
	{0x0e63,0x06},
	{0x0236,0x02},
	{0x0261,0x60},
	{0x0262,0x28},
	{0x0072,0x00},
	{0x0074,0x01},
	{0x0087,0x53},
	{0x0704,0x07},
	{0x0705,0x28},
	{0x0706,0x02},
	{0x0715,0x28},
	{0x0716,0x02},
	{0x0708,0xc0},
	{0x0718,0xc0},
	{0x0076,0x01},
	{0x021a,0x10},
	{0x0052,0x02},
	{0x0448,0x06},
	{0x0449,0x04},
	{0x044a,0x04},
	{0x044b,0x06},
	{0x044c,0x78},
	{0x044d,0x7a},
	{0x044e,0x7a},
	{0x044f,0x78},
	{0x0046,0x30},
	{0x0002,0xa9},
	{0x0005,0x83},
	{0x0006,0x83},
	{0x001a,0x83},
	{0x0075,0x65},
	{0x0202,0x08},
	{0x0203,0x46},
	{0x0914,0x01},
	{0x0915,0x00},
	{0x0225,0x00},
	{0x0e67,0x0f},
	{0x0e68,0x0f},
	{0x0089,0x03},
	{0x0144,0x00},
	{0x0122,0x08},
	{0x0123,0x27},
	{0x0126,0x0a},
	{0x0129,0x08},
	{0x012a,0x0d},
	{0x012b,0x0a},
	{0x0180,0x46},
	{0x0181,0x30},
	{0x0185,0x01},
	{0x0106,0x38},
	{0x010d,0x0d},
	{0x010e,0x20},
	{0x0111,0x2b},
	{0x0112,0x0a},
	{0x0113,0x0a},
	{0x0114,0x01},
	{0x0221,0x05},
	{0x023b,0x13},
	{0x0352,0x70},
	{0x0357,0x00},
	{0x0b00,0x40},
	{0x08ef,0x01},
	{0x03fe,0x00},
	{0x031f,0x01},
	{0x031f,0x00},
	{0x0318,0x0e},
	{0x0a67,0x80},
	{0x0a50,0x41},
	{0x0a51,0x41},
	{0x0a52,0x41},
	{0x0a54,0x26},
	{0x0a55,0x26},
	{0x0a4e,0x0c},
	{0x0a4f,0x0c},
	{0x0a65,0x17},
	{0x0a53,0x00},
	{0x0a98,0x04},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0a67,0x80},
	{0x0023,0x00},
	{0x0025,0x00},
	{0x0028,0x0a},
	{0x0029,0x90},
	{0x002a,0x08},
	{0x002b,0x20},
	{0x0a8b,0x0a},
	{0x0a8a,0x90},
	{0x0a89,0x08},
	{0x0a88,0x20},
	{0x0a70,0x07},
	{0x0a73,0xe0},
	{0x0a80,0x7b},
	{0x0a82,0x00},
	{0x0a83,0x80},
	{0x0a5a,0x80},
	{REG_DELAY, 20},
	{0x05be,0x01},
	{0x0a70,0x00},
	{0x0080,0x02},
	{0x0021,0x40},
	{0x0a67,0x00},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 516Mbps, 4lane
 */
static const struct regval gc6603_linear10bit_2560x1440_4lane_regs[] = {
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03fe,0x10},
	{0x0938,0x01},
	{0x0360,0xfd},
	{0x091b,0x1a},
	{0x091c,0x18},
	{0x091e,0x00},
	{0x091d,0x06},
	{0x091f,0x81},
	{0x0920,0xa1},
	{0x0922,0x3a},
	{0x0923,0x10},
	{0x0928,0x01},
	{0x0934,0xb7},
	{0x0935,0x06},
	{0x0936,0x00},
	{0x0937,0x81},
	{0x031b,0x00},
	{0x031c,0x4f},
	{0x031e,0x00},
	{0x03e0,0x00},
	{0x0314,0x10},
	{0x0219,0x47},
	{0x022b,0x10},
	{0x0259,0x08},
	{0x025a,0x44},
	{0x025b,0x10},
	{0x0340,0x08},
	{0x0341,0x66},
	{0x0342,0x03},
	{0x0343,0xe8},
	{0x0346,0x00},
	{0x0347,0x40},
	{0x0348,0x0a},
	{0x0349,0x90},
	{0x034a,0x08},
	{0x034b,0x20},
	{0x034e,0x0a},
	{0x034f,0xc0},
	{0x070c,0x03},
	{0x070d,0x00},
	{0x070e,0x98},
	{0x070f,0x0a},
	{0x0053,0x05},
	{0x0099,0x10},
	{0x009b,0x08},
	{0x0094,0x0a},
	{0x0095,0x80},
	{0x0096,0x08},
	{0x0097,0x00},
	{0x0e4c,0x3c},
	{0x0902,0x0b},
	{0x0903,0x15},
	{0x0904,0x14},
	{0x0907,0x14},
	{0x0908,0x15},
	{0x090e,0x26},
	{0x090f,0x15},
	{0x0244,0x75},
	{0x0724,0x0c},
	{0x0727,0x0c},
	{0x072a,0x18},
	{0x072b,0x19},
	{0x0709,0x40},
	{0x0719,0x40},
	{0x0912,0x01},
	{0x0913,0x00},
	{0x0e66,0x10},
	{0x0e69,0x80},
	{0x0e6a,0xc0},
	{0x0e6b,0x02},
	{0x0223,0x00},
	{0x0e81,0x02},
	{0x0e30,0x00},
	{0x0e33,0x80},
	{0x0242,0x65},
	{0x0361,0xbc},
	{0x0362,0x0f},
	{0x0e34,0x04},
	{0x0e47,0x55},
	{0x0e61,0x0d},
	{0x0e62,0x0d},
	{0x023a,0x05},
	{0x0e64,0x0c},
	{0x0e20,0x0c},
	{0x0e6e,0x50},
	{0x0e6f,0x58},
	{0x0e70,0x24},
	{0x0e71,0x28},
	{0x0e28,0x38},
	{0x0e4d,0x80},
	{0x0245,0x08},
	{0x0240,0x06},
	{0x0e63,0x06},
	{0x0236,0x02},
	{0x0261,0x60},
	{0x0262,0x28},
	{0x0072,0x00},
	{0x0074,0x01},
	{0x0087,0x53},
	{0x0704,0x07},
	{0x0705,0x28},
	{0x0706,0x02},
	{0x0715,0x28},
	{0x0716,0x02},
	{0x0708,0xc0},
	{0x0718,0xc0},
	{0x0076,0x01},
	{0x021a,0x10},
	{0x0052,0x02},
	{0x0448,0x06},
	{0x0449,0x04},
	{0x044a,0x04},
	{0x044b,0x06},
	{0x044c,0x78},
	{0x044d,0x7a},
	{0x044e,0x7a},
	{0x044f,0x78},
	{0x0046,0x30},
	{0x0002,0xa9},
	{0x0005,0x83},
	{0x0006,0x83},
	{0x001a,0x83},
	{0x0075,0x65},
	{0x0202,0x08},
	{0x0203,0x46},
	{0x0914,0x01},
	{0x0915,0x00},
	{0x0225,0x00},
	{0x0e67,0x0f},
	{0x0e68,0x0f},
	{0x0089,0x03},
	{0x0144,0x00},
	{0x0122,0x03},
	{0x0123,0x27},
	{0x0126,0x05},
	{0x0129,0x03},
	{0x012a,0x0d},
	{0x012b,0x05},
	{0x0180,0x46},
	{0x0181,0xf0},
	{0x0185,0x01},
	{0x0106,0x38},
	{0x010d,0x0d},
	{0x010e,0x20},
	{0x0111,0x2b},
	{0x0112,0x0a},
	{0x0113,0x0a},
	{0x0114,0x03},
	{0x0100,0x09},
	{0x0221,0x05},
	{0x023b,0x13},
	{0x0352,0x70},
	{0x0357,0x00},
	{0x0b00,0x40},
	{0x08ef,0x01},
	{0x03fe,0x00},
	{0x031f,0x01},
	{0x031f,0x00},
	{0x0318,0x0e},
	{0x0a67,0x80},
	{0x0a50,0x41},
	{0x0a51,0x41},
	{0x0a52,0x41},
	{0x0a54,0x26},
	{0x0a55,0x26},
	{0x0a4e,0x0c},
	{0x0a4f,0x0c},
	{0x0a65,0x17},
	{0x0a53,0x00},
	{0x0a98,0x04},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0a67,0x80},
	{0x0023,0x00},
	{0x0025,0x00},
	{0x0028,0x0a},
	{0x0029,0x90},
	{0x002a,0x08},
	{0x002b,0x20},
	{0x0a8b,0x0a},
	{0x0a8a,0x90},
	{0x0a89,0x08},
	{0x0a88,0x20},
	{0x0a70,0x07},
	{0x0a73,0xe0},
	{0x0a80,0x7b},
	{0x0a82,0x00},
	{0x0a83,0x80},
	{0x0a5a,0x80},
	{REG_DELAY, 20},
	{0x05be,0x01},
	{0x0a70,0x00},
	{0x0080,0x02},
	{0x0021,0x40},
	{0x0a67,0x00},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 25fps
 * mipiclk 816Mhz, 4lane
 */
static const struct regval gc6603_hdr10bit_2560x1440_4lane_regs[] = {
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03fe,0x10},
	{0x0938,0x01},
	{0x0360,0xfd},
	{0x091b,0x1a},
	{0x091c,0x00},
	{0x091e,0x01},
	{0x091d,0x16},
	{0x091f,0xae},
	{0x0920,0xa1},
	{0x0922,0x3a},
	{0x0923,0x10},
	{0x0928,0x01},
	{0x0934,0xa7},
	{0x0935,0x16},
	{0x0936,0x00},
	{0x0937,0x88},
	{0x031b,0x00},
	{0x031c,0x4f},
	{0x031e,0x00},
	{0x03e0,0x00},
	{0x0314,0x10},
	{0x0219,0x47},
	{0x022b,0x10},
	{0x0259,0x08},
	{0x025a,0x44},
	{0x025b,0x10},
	{0x0340,0x08},
	{0x0341,0x66},
	{0x0342,0x02},
	{0x0343,0xee},
	{0x0346,0x00},
	{0x0347,0x40},
	{0x0348,0x0a},
	{0x0349,0x90},
	{0x034a,0x08},
	{0x034b,0x20},
	{0x034e,0x0a},
	{0x034f,0xc0},
	{0x070c,0x03},
	{0x070d,0x00},
	{0x070e,0x98},
	{0x070f,0x0a},
	{0x0053,0x05},
	{0x0099,0x10},
	{0x009b,0x08},
	{0x0094,0x0a},
	{0x0095,0x80},
	{0x0096,0x08},
	{0x0097,0x00},
	{0x0e4c,0x3c},
	{0x0902,0x0b},
	{0x0903,0x15},
	{0x0904,0x14},
	{0x0907,0x14},
	{0x0908,0x15},
	{0x090e,0x26},
	{0x090f,0x15},
	{0x0244,0x75},
	{0x0724,0x0c},
	{0x0727,0x0c},
	{0x072a,0x18},
	{0x072b,0x19},
	{0x0709,0x40},
	{0x0719,0x40},
	{0x0912,0x01},
	{0x0913,0x00},
	{0x0e66,0x10},
	{0x0e69,0x80},
	{0x0e6a,0xc0},
	{0x0e6b,0x02},
	{0x0223,0x00},
	{0x0e81,0x02},
	{0x0e30,0x00},
	{0x0e33,0x80},
	{0x0242,0x65},
	{0x0361,0xbc},
	{0x0362,0x0f},
	{0x0e34,0x04},
	{0x0e47,0x55},
	{0x0e61,0x1a},
	{0x0e62,0x1a},
	{0x023a,0x05},
	{0x0e64,0x0c},
	{0x0e20,0x0c},
	{0x0e6e,0x50},
	{0x0e6f,0x58},
	{0x0e70,0x24},
	{0x0e71,0x28},
	{0x0e28,0x48},
	{0x0e4d,0x80},
	{0x0245,0x08},
	{0x0240,0x06},
	{0x0e63,0x06},
	{0x0236,0x02},
	{0x0261,0x60},
	{0x0262,0x28},
	{0x0072,0x00},
	{0x0074,0x01},
	{0x0087,0x53},
	{0x0704,0x07},
	{0x0705,0x28},
	{0x0706,0x02},
	{0x0715,0x28},
	{0x0716,0x02},
	{0x0708,0xc0},
	{0x0718,0xc0},
	{0x0076,0x01},
	{0x021a,0x10},
	{0x0052,0x02},
	{0x0448,0x06},
	{0x0449,0x04},
	{0x044a,0x04},
	{0x044b,0x06},
	{0x044c,0x78},
	{0x044d,0x7a},
	{0x044e,0x7a},
	{0x044f,0x78},
	{0x0046,0x30},
	{0x0002,0xa9},
	{0x0005,0x83},
	{0x0006,0x83},
	{0x001a,0x83},
	{0x0075,0x65},
	{0x0202,0x08},
	{0x0203,0x46},
	{0x0914,0x01},
	{0x0915,0x00},
	{0x0916,0x01},
	{0x0917,0x00},
	{0x0225,0x00},
	{0x0e67,0x11},
	{0x0e68,0x11},
	{0x0089,0x03},
	{0x0144,0x00},
	{0x0122,0x06},
	{0x0123,0x27},
	{0x0126,0x08},
	{0x0129,0x07},
	{0x012a,0x0d},
	{0x012b,0x08},
	{0x0180,0x46},
	{0x0181,0xf0},
	{0x0185,0x01},
	{0x0106,0x38},
	{0x010d,0x0d},
	{0x010e,0x20},
	{0x0111,0x2b},
	{0x0112,0x0a},
	{0x0113,0x0a},
	{0x0114,0x03},
	{0x0100,0x09},
	{0x0221,0x05},
	{0x023b,0x13},
	{0x0352,0x70},
	{0x0357,0x00},
	{0x0b00,0x40},
	{0x0222,0x41},
	{0x0107,0x89},
	{0x0919,0x02},
	{0x023b,0x02},
	{0x0450,0x06},
	{0x0451,0x04},
	{0x0452,0x04},
	{0x0453,0x06},
	{0x0454,0x78},
	{0x0455,0x7a},
	{0x0456,0x7a},
	{0x0457,0x78},
	{0x08ef,0x01},
	{0x03fe,0x00},
	{0x031f,0x01},
	{0x031f,0x00},
	{0x0318,0x0e},
	{0x0a67,0x80},
	{0x0a50,0x41},
	{0x0a51,0x41},
	{0x0a52,0x41},
	{0x0a54,0x26},
	{0x0a55,0x26},
	{0x0a4e,0x0c},
	{0x0a4f,0x0c},
	{0x0a65,0x17},
	{0x0a53,0x00},
	{0x0a98,0x04},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0a67,0x80},
	{0x0023,0x00},
	{0x0025,0x00},
	{0x0028,0x0a},
	{0x0029,0x90},
	{0x002a,0x08},
	{0x002b,0x20},
	{0x0a8b,0x0a},
	{0x0a8a,0x90},
	{0x0a89,0x08},
	{0x0a88,0x20},
	{0x0a70,0x07},
	{0x0a73,0xe0},
	{0x0a80,0x7b},
	{0x0a82,0x00},
	{0x0a83,0x80},
	{0x0a5a,0x80},
	{REG_DELAY, 20},
	{0x05be,0x01},
	{0x0a70,0x00},
	{0x0080,0x02},
	{0x0021,0x40},
	{0x0a67,0x00},
	{REG_NULL, 0x00},
};

static const struct gc6603_mode supported_modes_2lane[] = {
	{
		.width = 2688,
		.height = 2048,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0800,
		.hts_def = 0x03E8 * 4,
		.vts_def = 0x0866,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = gc6603_linear10bit_2560x1440_2lane_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
};

static const struct gc6603_mode supported_modes_4lane[] = {
	{
		.width = 2688,
		.height = 2048,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0800,
		.hts_def = 0x03e8 * 4,
		.vts_def = 0x0866,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = gc6603_linear10bit_2560x1440_4lane_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	}, {
		.width = 2688,
		.height = 2048,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.exp_def = 0x0800,
		.hts_def = 0x02ee *4,
		.vts_def = 0x0866,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = gc6603_hdr10bit_2560x1440_4lane_regs,
		.hdr_mode = HDR_X2,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,//L->csi wr0
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,//M->csi wr2
	},
};

static const s64 link_freq_menu_items[] = {
	GC6603_LINK_FREQ_516M,
	GC6603_LINK_FREQ_816M,
	GC6603_LINK_FREQ_1032M,
};

static const char * const gc6603_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int gc6603_write_reg(struct i2c_client *client, u16 reg,
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

static int gc6603_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		if (regs[i].addr != REG_DELAY)
			ret = gc6603_write_reg(client, regs[i].addr,
					       GC6603_REG_VALUE_08BIT, regs[i].val);
		else
			usleep_range(regs[i].val * 1000, regs[i].val *1010);
	}

	return ret;
}

/* Read registers up to 4 at a time */
static int gc6603_read_reg(struct i2c_client *client, u16 reg,
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

static int gc6603_get_reso_dist(const struct gc6603_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
			abs(mode->height - framefmt->height);
}

static const struct gc6603_mode *
gc6603_find_best_fit(struct gc6603 *gc6603, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < gc6603->cfg_num; i++) {
		dist = gc6603_get_reso_dist(&gc6603->supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &gc6603->supported_modes[cur_best_fit];
}

static int gc6603_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc6603 *gc6603 = to_gc6603(sd);
	const struct gc6603_mode *mode;
	s64 h_blank, vblank_def;
	u8 lanes = gc6603->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&gc6603->mutex);

	mode = gc6603_find_best_fit(gc6603, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gc6603->mutex);
		return -ENOTTY;
#endif
	} else {
		gc6603->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc6603->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(gc6603->vblank, vblank_def,
					 GC6603_VTS_MAX - mode->height,
					 1, vblank_def);

		gc6603->cur_link_freq = mode->link_freq_idx;
		gc6603->cur_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
					 mode->bpp * 2 * lanes;

		__v4l2_ctrl_s_ctrl_int64(gc6603->pixel_rate,
					 gc6603->cur_pixel_rate);
		__v4l2_ctrl_s_ctrl(gc6603->link_freq,
				   gc6603->cur_link_freq);
		gc6603->cur_vts = mode->vts_def;
	}
	mutex_unlock(&gc6603->mutex);

	return 0;
}

static int gc6603_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc6603 *gc6603 = to_gc6603(sd);
	const struct gc6603_mode *mode = gc6603->cur_mode;

	mutex_lock(&gc6603->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gc6603->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&gc6603->mutex);

	return 0;
}

static int gc6603_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct gc6603 *gc6603 = to_gc6603(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = gc6603->cur_mode->bus_fmt;

	return 0;
}

static int gc6603_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct gc6603 *gc6603 = to_gc6603(sd);

	if (fse->index >= gc6603->cfg_num)
		return -EINVAL;

	if (fse->code != gc6603->supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width = gc6603->supported_modes[fse->index].width;
	fse->max_width = gc6603->supported_modes[fse->index].width;
	fse->max_height = gc6603->supported_modes[fse->index].height;
	fse->min_height = gc6603->supported_modes[fse->index].height;

	return 0;
}

static int gc6603_enable_test_pattern(struct gc6603 *gc6603, u32 pattern)
{
	u32 val;

	if (pattern)
		val = GC6603_TEST_PATTERN_ENABLE;
	else
		val = GC6603_TEST_PATTERN_DISABLE;

	return gc6603_write_reg(gc6603->client, GC6603_REG_TEST_PATTERN,
				GC6603_REG_VALUE_08BIT, val);
}

static int gc6603_set_gain_reg_hdr(struct gc6603 *gc6603, u32 gain)
{
	int i;
	int total;
	u32 tol_dig_gain = 0;

	if (gain < 64)
		gain = 64;
	total = sizeof(gain_level_table_hdr) / sizeof(u32) - 1;
	for (i = 0; i < total; i++) {
		if (gain_level_table_hdr[i] <= gain &&
		    gain < gain_level_table_hdr[i + 1])
			break;
	}

	if (i >= total)
		i = total - 1;

	tol_dig_gain = gain * 1024 / gain_level_table_hdr[i];

	gc6603_write_reg(gc6603->client, 0x0914, GC6603_REG_VALUE_08BIT, reg_val_table_hdr[i][0]);
	gc6603_write_reg(gc6603->client, 0x0915, GC6603_REG_VALUE_08BIT, reg_val_table_hdr[i][1]);
	gc6603_write_reg(gc6603->client, 0x0916, GC6603_REG_VALUE_08BIT, reg_val_table_hdr[i][2]);
	gc6603_write_reg(gc6603->client, 0x0917, GC6603_REG_VALUE_08BIT, reg_val_table_hdr[i][3]);
	gc6603_write_reg(gc6603->client, 0x0225, GC6603_REG_VALUE_08BIT, reg_val_table_hdr[i][4]);
	gc6603_write_reg(gc6603->client, 0x0e67, GC6603_REG_VALUE_08BIT, reg_val_table_hdr[i][5]);
	gc6603_write_reg(gc6603->client, 0x0e68, GC6603_REG_VALUE_08BIT, reg_val_table_hdr[i][6]);
	gc6603_write_reg(gc6603->client, 0x0242, GC6603_REG_VALUE_08BIT, reg_val_table_hdr[i][7]);

	gc6603_write_reg(gc6603->client, 0x0064, GC6603_REG_VALUE_08BIT, (tol_dig_gain>>8)&0xff);
	gc6603_write_reg(gc6603->client, 0x0065, GC6603_REG_VALUE_08BIT, (tol_dig_gain&0xff));

	return 0;
}

static int gc6603_set_gain_reg(struct gc6603 *gc6603, u32 gain)
{
	int i;
	int total;
	u32 tol_dig_gain = 0;

	if (gain < 64)
		gain = 64;
	total = sizeof(gain_level_table_linear) / sizeof(u32) - 1;
	for (i = 0; i < total; i++) {
		if (gain_level_table_linear[i] <= gain &&
		    gain < gain_level_table_linear[i + 1])
			break;
	}
	if (i >= total)
		i = total - 1;

	tol_dig_gain = gain * 1024 / gain_level_table_linear[i];
	gc6603_write_reg(gc6603->client, 0x0914, GC6603_REG_VALUE_08BIT, reg_val_table_liner[i][0]);
	gc6603_write_reg(gc6603->client, 0x0915, GC6603_REG_VALUE_08BIT, reg_val_table_liner[i][1]);
	gc6603_write_reg(gc6603->client, 0x0225, GC6603_REG_VALUE_08BIT, reg_val_table_liner[i][2]);
	gc6603_write_reg(gc6603->client, 0x0e67, GC6603_REG_VALUE_08BIT, reg_val_table_liner[i][3]);
	gc6603_write_reg(gc6603->client, 0x0e68, GC6603_REG_VALUE_08BIT, reg_val_table_liner[i][4]);
	gc6603_write_reg(gc6603->client, 0x0242, GC6603_REG_VALUE_08BIT, reg_val_table_liner[i][5]);
	gc6603_write_reg(gc6603->client, 0x0064, GC6603_REG_VALUE_08BIT, (tol_dig_gain>>8)&0xff);
	gc6603_write_reg(gc6603->client, 0x0065, GC6603_REG_VALUE_08BIT, (tol_dig_gain&0xff));

	return 0;
}

/* window_heigth = 1472
 * dummy = 20
 * frame_length = window_heigth + dummy + vb = 1492 + vb
 * s_exp_time < VB
 * s_exp_time + l_exp_time < frame_length
 */
static int gc6603_set_hdrae(struct gc6603 *gc6603,
			    struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	u32 intt_long_l, intt_long_h;
	u32 intt_short_l, intt_short_h;
	u32 gain;

	if (!gc6603->has_init_exp && !gc6603->streaming) {
		gc6603->init_hdrae_exp = *ae;
		gc6603->has_init_exp = true;
		dev_dbg(&gc6603->client->dev, "gc6603 don't stream, record exp for hdr!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&gc6603->client->dev,
		"rev exp req: L_exp: 0x%x, M_exp: 0x%x, S_exp 0x%x,l_gain:0x%x, m_gain: 0x%x, s_gain: 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (gc6603->cur_mode->hdr_mode == HDR_X2)
		l_exp_time = m_exp_time;

	gain = s_a_gain;

	//long exp + short exp < vts
	if (l_exp_time <= 1)
		l_exp_time = 1;

	//short exp < vb && short exp = 2n
	if (s_exp_time < 2)
		s_exp_time = 2;

	if (s_exp_time > gc6603->cur_vts - gc6603->cur_mode->height) {
		dev_err(&gc6603->client->dev, "the s_exp_time is too large.\n");
		s_exp_time = gc6603->cur_vts - gc6603->cur_mode->height;
	}

	if (l_exp_time > gc6603->cur_vts - s_exp_time) {
		dev_err(&gc6603->client->dev, "the l_exp_time is too large.\n");
		l_exp_time = gc6603->cur_vts - s_exp_time;
	}

	intt_long_l = l_exp_time & 0xff;
	intt_long_h = (l_exp_time >> 8) & 0x3f;

	intt_short_l = s_exp_time & 0xff;
	intt_short_h = (s_exp_time >> 8) & 0x3f;

	ret |= gc6603_write_reg(gc6603->client, GC6603_REG_EXPOSURE_H,
			GC6603_REG_VALUE_08BIT,
			intt_long_h);
	ret |= gc6603_write_reg(gc6603->client, GC6603_REG_EXPOSURE_L,
			GC6603_REG_VALUE_08BIT,
			intt_long_l);
	ret |= gc6603_write_reg(gc6603->client, GC6603_REG_SEXPOSURE_H,
			GC6603_REG_VALUE_08BIT,
			intt_short_h);
	ret |= gc6603_write_reg(gc6603->client, GC6603_REG_SEXPOSURE_L,
			GC6603_REG_VALUE_08BIT,
			intt_short_l);

	ret |= gc6603_set_gain_reg_hdr(gc6603, gain);
	return ret;
}

static int gc6603_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc6603 *gc6603 = to_gc6603(sd);
	const struct gc6603_mode *mode = gc6603->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static int gc6603_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct gc6603 *gc6603 = to_gc6603(sd);
	u8 lanes = gc6603->bus_cfg.bus.mipi_csi2.num_data_lanes;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = lanes;

	return 0;
}

static void gc6603_get_module_inf(struct gc6603 *gc6603,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, GC6603_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, gc6603->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, gc6603->len_name, sizeof(inf->base.lens));
}

static int gc6603_get_channel_info(struct gc6603 *gc6603, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = gc6603->cur_mode->vc[ch_info->index];
	ch_info->width = gc6603->cur_mode->width;
	ch_info->height = gc6603->cur_mode->height;
	ch_info->bus_fmt = gc6603->cur_mode->bus_fmt;
	return 0;
}

static long gc6603_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc6603 *gc6603 = to_gc6603(sd);
	struct rkmodule_hdr_cfg *hdr;
	u32 i, h, w;
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;
	u8 lanes = gc6603->bus_cfg.bus.mipi_csi2.num_data_lanes;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gc6603_get_module_inf(gc6603, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = gc6603->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = gc6603->cur_mode->width;
		h = gc6603->cur_mode->height;
		for (i = 0; i < gc6603->cfg_num; i++) {
			if (w == gc6603->supported_modes[i].width &&
			    h == gc6603->supported_modes[i].height &&
			    gc6603->supported_modes[i].hdr_mode == hdr->hdr_mode) {
				gc6603->cur_mode = &gc6603->supported_modes[i];
				break;
			}
		}
		if (i == gc6603->cfg_num) {
			dev_err(&gc6603->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = gc6603->cur_mode->hts_def -
			    gc6603->cur_mode->width;
			h = gc6603->cur_mode->vts_def -
			    gc6603->cur_mode->height;
			__v4l2_ctrl_modify_range(gc6603->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(gc6603->vblank, h,
						 GC6603_VTS_MAX -
						 gc6603->cur_mode->height,
						 1, h);
			gc6603->cur_link_freq = gc6603->cur_mode->link_freq_idx;
			gc6603->cur_pixel_rate = (u32)link_freq_menu_items[gc6603->cur_mode->link_freq_idx] /
						  gc6603->cur_mode->bpp * 2 * lanes;

			__v4l2_ctrl_s_ctrl_int64(gc6603->pixel_rate,
						 gc6603->cur_pixel_rate);
			__v4l2_ctrl_s_ctrl(gc6603->link_freq,
					   gc6603->cur_link_freq);
			gc6603->cur_vts = gc6603->cur_mode->vts_def;
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		ret = gc6603_set_hdrae(gc6603, arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);
		if (stream)
			ret = gc6603_write_reg(gc6603->client, GC6603_REG_CTRL_MODE,
				GC6603_REG_VALUE_08BIT, GC6603_MODE_STREAMING);
		else
			ret = gc6603_write_reg(gc6603->client, GC6603_REG_CTRL_MODE,
				GC6603_REG_VALUE_08BIT, GC6603_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = gc6603_get_channel_info(gc6603, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gc6603_compat_ioctl32(struct v4l2_subdev *sd,
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

		ret = gc6603_ioctl(sd, cmd, inf);
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
			ret = gc6603_ioctl(sd, cmd, cfg);
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

		ret = gc6603_ioctl(sd, cmd, hdr);
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
			ret = gc6603_ioctl(sd, cmd, hdr);
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
			ret = gc6603_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = gc6603_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc6603_ioctl(sd, cmd, ch_info);
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

static int __gc6603_start_stream(struct gc6603 *gc6603)
{
	int ret;

	ret = gc6603_write_array(gc6603->client, gc6603->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&gc6603->ctrl_handler);
	if (gc6603->has_init_exp && gc6603->cur_mode->hdr_mode != NO_HDR) {
		ret = gc6603_ioctl(&gc6603->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&gc6603->init_hdrae_exp);
		if (ret) {
			dev_err(&gc6603->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}
	if (ret)
		return ret;

	ret |= gc6603_write_reg(gc6603->client, GC6603_REG_CTRL_MODE,
				GC6603_REG_VALUE_08BIT, GC6603_MODE_STREAMING);
	return ret;
}

static int __gc6603_stop_stream(struct gc6603 *gc6603)
{
	gc6603->has_init_exp = false;
	return gc6603_write_reg(gc6603->client, GC6603_REG_CTRL_MODE,
				GC6603_REG_VALUE_08BIT, GC6603_MODE_SW_STANDBY);
}

static int gc6603_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc6603 *gc6603 = to_gc6603(sd);
	struct i2c_client *client = gc6603->client;
	int ret = 0;

	mutex_lock(&gc6603->mutex);
	on = !!on;
	if (on == gc6603->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gc6603_start_stream(gc6603);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gc6603_stop_stream(gc6603);
		pm_runtime_put(&client->dev);
	}

	gc6603->streaming = on;

unlock_and_return:
	mutex_unlock(&gc6603->mutex);

	return ret;
}

static int gc6603_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc6603 *gc6603 = to_gc6603(sd);
	struct i2c_client *client = gc6603->client;
	int ret = 0;

	mutex_lock(&gc6603->mutex);

	/* If the power state is not modified - no work to do. */
	if (gc6603->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gc6603->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		gc6603->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc6603->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 gc6603_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, GC6603_XVCLK_FREQ / 1000 / 1000);
}

static int __gc6603_power_on(struct gc6603 *gc6603)
{
	int ret;
	u32 delay_us;
	struct device *dev = &gc6603->client->dev;

	if (!IS_ERR_OR_NULL(gc6603->pins_default)) {
		ret = pinctrl_select_state(gc6603->pinctrl,
					   gc6603->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(gc6603->xvclk, GC6603_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(gc6603->xvclk) != GC6603_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(gc6603->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}
	if (!IS_ERR(gc6603->reset_gpio))
		gpiod_set_value_cansleep(gc6603->reset_gpio, 0);

	if (!IS_ERR(gc6603->pwdn_gpio))
		gpiod_set_value_cansleep(gc6603->pwdn_gpio, 0);

	usleep_range(500, 1000);
	ret = regulator_bulk_enable(GC6603_NUM_SUPPLIES, gc6603->supplies);

	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(gc6603->pwren_gpio))
		gpiod_set_value_cansleep(gc6603->pwren_gpio, 1);

	usleep_range(1000, 1100);
	if (!IS_ERR(gc6603->pwdn_gpio))
		gpiod_set_value_cansleep(gc6603->pwdn_gpio, 1);
	usleep_range(100, 150);
	if (!IS_ERR(gc6603->reset_gpio))
		gpiod_set_value_cansleep(gc6603->reset_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = gc6603_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(gc6603->xvclk);

	return ret;
}

static void __gc6603_power_off(struct gc6603 *gc6603)
{
	int ret;
	struct device *dev = &gc6603->client->dev;

	if (!IS_ERR(gc6603->pwdn_gpio))
		gpiod_set_value_cansleep(gc6603->pwdn_gpio, 0);
	clk_disable_unprepare(gc6603->xvclk);
	if (!IS_ERR(gc6603->reset_gpio))
		gpiod_set_value_cansleep(gc6603->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(gc6603->pins_sleep)) {
		ret = pinctrl_select_state(gc6603->pinctrl,
					   gc6603->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(GC6603_NUM_SUPPLIES, gc6603->supplies);
	if (!IS_ERR(gc6603->pwren_gpio))
		gpiod_set_value_cansleep(gc6603->pwren_gpio, 0);
}

static int gc6603_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc6603 *gc6603 = to_gc6603(sd);

	return __gc6603_power_on(gc6603);
}

static int gc6603_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc6603 *gc6603 = to_gc6603(sd);

	__gc6603_power_off(gc6603);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc6603_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc6603 *gc6603 = to_gc6603(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gc6603_mode *def_mode = &gc6603->supported_modes[0];

	mutex_lock(&gc6603->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&gc6603->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int gc6603_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_interval_enum *fie)
{
	struct gc6603 *gc6603 = to_gc6603(sd);

	if (fie->index >= gc6603->cfg_num)
		return -EINVAL;

	fie->code = gc6603->supported_modes[fie->index].bus_fmt;
	fie->width = gc6603->supported_modes[fie->index].width;
	fie->height = gc6603->supported_modes[fie->index].height;
	fie->interval = gc6603->supported_modes[fie->index].max_fps;
	fie->reserved[0] = gc6603->supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops gc6603_pm_ops = {
	SET_RUNTIME_PM_OPS(gc6603_runtime_suspend,
			   gc6603_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gc6603_internal_ops = {
	.open = gc6603_open,
};
#endif

static const struct v4l2_subdev_core_ops gc6603_core_ops = {
	.s_power = gc6603_s_power,
	.ioctl = gc6603_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc6603_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gc6603_video_ops = {
	.s_stream = gc6603_s_stream,
	.g_frame_interval = gc6603_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc6603_pad_ops = {
	.enum_mbus_code = gc6603_enum_mbus_code,
	.enum_frame_size = gc6603_enum_frame_sizes,
	.enum_frame_interval = gc6603_enum_frame_interval,
	.get_fmt = gc6603_get_fmt,
	.set_fmt = gc6603_set_fmt,
	.get_mbus_config = gc6603_g_mbus_config,
};

static const struct v4l2_subdev_ops gc6603_subdev_ops = {
	.core	= &gc6603_core_ops,
	.video	= &gc6603_video_ops,
	.pad	= &gc6603_pad_ops,
};

static int gc6603_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc6603 *gc6603 = container_of(ctrl->handler,
					     struct gc6603, ctrl_handler);
	struct i2c_client *client = gc6603->client;
	s64 max;
	int ret = 0;

	/*Propagate change of current control to all related controls*/
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/*Update max exposure while meeting expected vblanking*/
		max = gc6603->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(gc6603->exposure,
					 gc6603->exposure->minimum,
					 max,
					 gc6603->exposure->step,
					 gc6603->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = gc6603_write_reg(gc6603->client, GC6603_REG_EXPOSURE_H,
				       GC6603_REG_VALUE_08BIT,
				       ctrl->val >> 8);
		ret |= gc6603_write_reg(gc6603->client, GC6603_REG_EXPOSURE_L,
					GC6603_REG_VALUE_08BIT,
					ctrl->val & 0xfe);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = gc6603_set_gain_reg(gc6603, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		gc6603->cur_vts = ctrl->val + gc6603->cur_mode->height;
		ret = gc6603_write_reg(gc6603->client, GC6603_REG_VTS_H,
				       GC6603_REG_VALUE_08BIT,
				       gc6603->cur_vts >> 8);
		ret |= gc6603_write_reg(gc6603->client, GC6603_REG_VTS_L,
					GC6603_REG_VALUE_08BIT,
					gc6603->cur_vts & 0xff);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = gc6603_enable_test_pattern(gc6603, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gc6603_ctrl_ops = {
	.s_ctrl = gc6603_set_ctrl,
};

static int gc6603_initialize_controls(struct gc6603 *gc6603)
{
	const struct gc6603_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u8 lanes = gc6603->bus_cfg.bus.mipi_csi2.num_data_lanes;

	handler = &gc6603->ctrl_handler;
	mode = gc6603->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &gc6603->mutex;

	gc6603->link_freq = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
						   1, 0, link_freq_menu_items);
	gc6603->cur_link_freq = gc6603->cur_mode->link_freq_idx;
	gc6603->cur_pixel_rate = (u32)link_freq_menu_items[gc6603->cur_mode->link_freq_idx] /
				  gc6603->cur_mode->bpp * 2 * lanes;

	__v4l2_ctrl_s_ctrl(gc6603->link_freq,
			   gc6603->cur_link_freq);

	gc6603->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, GC6603_PIXEL_RATE_MAX, 1, gc6603->cur_pixel_rate);

	h_blank = mode->hts_def - mode->width;
	gc6603->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (gc6603->hblank)
		gc6603->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	gc6603->cur_vts = mode->vts_def;
	gc6603->vblank = v4l2_ctrl_new_std(handler, &gc6603_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   GC6603_VTS_MAX - mode->height,
					    1, vblank_def);

	exposure_max = mode->vts_def - 8;
	gc6603->exposure = v4l2_ctrl_new_std(handler, &gc6603_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     GC6603_EXPOSURE_MIN,
					     exposure_max,
					     GC6603_EXPOSURE_STEP,
					     mode->exp_def);

	gc6603->anal_gain = v4l2_ctrl_new_std(handler, &gc6603_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN,
					      GC6603_GAIN_MIN,
					      GC6603_GAIN_MAX,
					      GC6603_GAIN_STEP,
					      GC6603_GAIN_DEFAULT);

	gc6603->test_pattern =
		v4l2_ctrl_new_std_menu_items(handler,
					     &gc6603_ctrl_ops,
				V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(gc6603_test_pattern_menu) - 1,
				0, 0, gc6603_test_pattern_menu);
	if (handler->error) {
		ret = handler->error;
		dev_err(&gc6603->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc6603->subdev.ctrl_handler = handler;
	gc6603->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gc6603_check_sensor_id(struct gc6603 *gc6603,
				  struct i2c_client *client)
{
	struct device *dev = &gc6603->client->dev;
	u16 id = 0;
	u32 reg_H = 0;
	u32 reg_L = 0;
	int ret;

	ret = gc6603_read_reg(client, GC6603_REG_CHIP_ID_H,
			      GC6603_REG_VALUE_08BIT, &reg_H);
	ret |= gc6603_read_reg(client, GC6603_REG_CHIP_ID_L,
			       GC6603_REG_VALUE_08BIT, &reg_L);

	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (!(reg_H == (CHIP_ID >> 8) || reg_L == (CHIP_ID & 0xff))) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected gc%04x sensor\n", id);
	return 0;
}

static int gc6603_configure_regulators(struct gc6603 *gc6603)
{
	unsigned int i;

	for (i = 0; i < GC6603_NUM_SUPPLIES; i++)
		gc6603->supplies[i].supply = gc6603_supply_names[i];

	return devm_regulator_bulk_get(&gc6603->client->dev,
				       GC6603_NUM_SUPPLIES,
				       gc6603->supplies);
}

static int gc6603_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gc6603 *gc6603;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;
	struct device_node *endpoint;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	gc6603 = devm_kzalloc(dev, sizeof(*gc6603), GFP_KERNEL);
	if (!gc6603)
		return -ENOMEM;

	of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &gc6603->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &gc6603->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &gc6603->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &gc6603->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	gc6603->client = client;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
		&gc6603->bus_cfg);
	of_node_put(endpoint);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}

	if (gc6603->bus_cfg.bus.mipi_csi2.num_data_lanes == 4) {
		gc6603->supported_modes = supported_modes_4lane;
		gc6603->cfg_num = ARRAY_SIZE(supported_modes_4lane);
	} else {
		gc6603->supported_modes = supported_modes_2lane;
		gc6603->cfg_num = ARRAY_SIZE(supported_modes_2lane);
	}
	for (i = 0; i < gc6603->cfg_num; i++) {
		if (hdr_mode == gc6603->supported_modes[i].hdr_mode) {
			gc6603->cur_mode = &gc6603->supported_modes[i];
			break;
		}
	}
	if (i == gc6603->cfg_num)
		gc6603->cur_mode = &gc6603->supported_modes[0];

	gc6603->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(gc6603->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	gc6603->pwren_gpio = devm_gpiod_get(dev, "pwren", GPIOD_OUT_LOW);
	if (IS_ERR(gc6603->pwren_gpio))
		dev_warn(dev, "Failed to get pwren-gpios\n");

	gc6603->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(gc6603->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	gc6603->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(gc6603->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	gc6603->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(gc6603->pinctrl)) {
		gc6603->pins_default =
			pinctrl_lookup_state(gc6603->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(gc6603->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		gc6603->pins_sleep =
			pinctrl_lookup_state(gc6603->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(gc6603->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = gc6603_configure_regulators(gc6603);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&gc6603->mutex);

	sd = &gc6603->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc6603_subdev_ops);
	ret = gc6603_initialize_controls(gc6603);
	if (ret)
		goto err_destroy_mutex;

	ret = __gc6603_power_on(gc6603);
	if (ret)
		goto err_free_handler;

	usleep_range(3000, 4000);

	ret = gc6603_check_sensor_id(gc6603, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc6603_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gc6603->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc6603->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc6603->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc6603->module_index, facing,
		 GC6603_NAME, dev_name(sd->dev));
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
	__gc6603_power_off(gc6603);
err_free_handler:
	v4l2_ctrl_handler_free(&gc6603->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc6603->mutex);

	return ret;
}

static void gc6603_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc6603 *gc6603 = to_gc6603(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gc6603->ctrl_handler);
	mutex_destroy(&gc6603->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc6603_power_off(gc6603);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc6603_of_match[] = {
	{ .compatible = "galaxycore,gc6603" },
	{},
};
MODULE_DEVICE_TABLE(of, gc6603_of_match);
#endif

static const struct i2c_device_id gc6603_match_id[] = {
	{ "galaxycore,gc6603", 0 },
	{ },
};

static struct i2c_driver gc6603_i2c_driver = {
	.driver = {
		.name = GC6603_NAME,
		.pm = &gc6603_pm_ops,
		.of_match_table = of_match_ptr(gc6603_of_match),
	},
	.probe		= &gc6603_probe,
	.remove		= &gc6603_remove,
	.id_table	= gc6603_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc6603_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc6603_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("galaxycore gc6603 sensor driver");
MODULE_LICENSE("GPL");
