// SPDX-License-Identifier: GPL-2.0
/*
 * GC8613 driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 add first implementation.
 * V0.0X01.0X02 add support wake-up/sleep (aov mode).
 * V0.0X01.0X03 add support thunder boot.
 * V0.0X01.0X04 add support linear 3840x2160@30fps for gc8613 ya.
 * V0.0X01.0X05 add support staggered hdr 3840x2160@30fps for gc8613 ya.
 * V0.0X01.0X06 add support linear 3840x2160@40fps for gc8613 ya.
 */

//#define DEBUG

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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x06)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define GC8613_LANES			4
#define GC8613_BITS_PER_SAMPLE		10
#define GC8613_LINK_FREQ_LINEAR		729000000
#define GC8613_LINK_FREQ_HDR		594000000

#define GC8613_PIXEL_RATE_LINEAR	(GC8613_LINK_FREQ_LINEAR * 2 / 10 * 4)
#define GC8613_PIXEL_RATE_DAG		(GC8613_LINK_FREQ_HDR * 2 / 12 * 4)
#define GC8613_PIXEL_RATE_HDR		(GC8613_LINK_FREQ_HDR * 2 / 10 * 4)

#define GC8613_XVCLK_FREQ_24M		24000000

#define CHIP_ID				0x8613
#define GC8613_REG_CHIP_ID_H		0x03f0
#define GC8613_REG_CHIP_ID_M		0x03f1
#define GC8613_REG_CHIP_ID_L		0x03f2

#define GC8613_REG_CTRL_MODE		0x0100
#define GC8613_MODE_SW_STANDBY		0x00
#define GC8613_MODE_STREAMING		0x09

#define GC8613_REG_SEXPOSURE_H		0x0200
#define GC8613_REG_SEXPOSURE_L		0x0201
#define GC8613_REG_EXPOSURE_H		0x0202
#define GC8613_REG_EXPOSURE_L		0x0203
#define GC8613_EXPOSURE_MIN		4
#define GC8613_EXPOSURE_STEP		1
#define GC8613_VTS_MAX			0x1fff

#define GC8613_GAIN_MIN			64
#define GC8613_GAIN_MAX			0xffff
#define GC8613_GAIN_STEP		1
#define GC8613_GAIN_DEFAULT		256

#define GC8613_REG_TEST_PATTERN		0x008c
#define GC8613_TEST_PATTERN_ENABLE	0x11
#define GC8613_TEST_PATTERN_DISABLE	0x0

#define GC8613_REG_VTS_H		0x0340
#define GC8613_REG_VTS_L		0x0341
#define GC8613_REG_HTS_H		0x0342
#define GC8613_REG_HTS_L		0x0343

#define GC8613_OTP_MIRROR_FLIP_REG	0x0a73
#define GC8613_FLIP_MIRROR_REG		0x022c
#define GC8613_FLIP_MIR_MOD_REG		0x0063
#define GC8613_MIRROR_BIT_MASK		BIT(0)
#define GC8613_FLIP_BIT_MASK		BIT(1)

#define REG_DELAY			0x0000
#define REG_NULL			0xFFFF

#define GC8613_REG_VALUE_08BIT		1
#define GC8613_REG_VALUE_16BIT		2
#define GC8613_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define GC8613_NAME			"gc8613"

static const char *const gc8613_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
	"avdd",		/* Analog power */
};

#define GC8613_NUM_SUPPLIES ARRAY_SIZE(gc8613_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct gc8613_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list[2];
	u32 hdr_mode;
	u32 vc[PAD_MAX];
	u32 mipi_freq_idx;
	u32 bpp;
};

struct gc8613 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct gpio_desc	*pwren_gpio;
	struct regulator_bulk_data supplies[GC8613_NUM_SUPPLIES];

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
	struct v4l2_fract	cur_fps;
	bool			streaming;
	bool			power_on;
	const struct gc8613_mode *cur_mode;
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
	bool			is_standby;
	bool			sensor_id;     //0:yn  0x03f2=0x03  1: ya 0x03f2=0x23
};

#define to_gc8613(sd) container_of(sd, struct gc8613, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval gc8613_global_regs[] = {
	{REG_NULL, 0x00},
};

static const u32 gain_level_table_dag_ya[23] = {
	64,
	77,
	96,
	115,
	139,
	155,
	186,
	220,
	260,
	300,
	358,
	433,
	515,
	576,
	682,
	811,
	899,
	1135,
	1329,
	1604,
	1930,
	2293,
	0xffff,
};

static const u32 reg_val_table_dag_ya[22][10] = {
	//614   615	  225	1467  1468  26e	  270	  b8	b9	1447
	{0x00, 0x00, 0x00, 0x3e, 0x3e, 0x76, 0x01, 0x01, 0x00, 0x77},
	{0x90, 0x02, 0x00, 0x3f, 0x3f, 0x76, 0x01, 0x01, 0x0D, 0x77},
	{0x01, 0x00, 0x00, 0x3f, 0x3f, 0x76, 0x01, 0x01, 0x1F, 0x77},
	{0x91, 0x02, 0x00, 0x40, 0x40, 0x77, 0x01, 0x01, 0x33, 0x77},
	{0x02, 0x00, 0x00, 0x41, 0x41, 0x79, 0x01, 0x02, 0x0A, 0x77},
	{0x00, 0x00, 0x00, 0x3e, 0x3e, 0x77, 0x01, 0x02, 0x1B, 0x75},
	{0x90, 0x02, 0x00, 0x3f, 0x3f, 0x77, 0x01, 0x02, 0x3A, 0x75},
	{0x01, 0x00, 0x00, 0x3f, 0x3f, 0x77, 0x01, 0x03, 0x1C, 0x75},
	{0x91, 0x02, 0x00, 0x40, 0x40, 0x71, 0x00, 0x04, 0x03, 0x75},
	{0x02, 0x00, 0x00, 0x41, 0x41, 0x71, 0x00, 0x04, 0x2C, 0x75},
	{0x92, 0x02, 0x00, 0x42, 0x42, 0x73, 0x00, 0x05, 0x26, 0x75},
	{0x03, 0x00, 0x00, 0x43, 0x43, 0x75, 0x00, 0x06, 0x30, 0x75},
	{0x93, 0x02, 0x00, 0x45, 0x45, 0x77, 0x00, 0x08, 0x02, 0x75},
	{0x00, 0x00, 0x01, 0x46, 0x46, 0x79, 0x00, 0x08, 0x3F, 0x75},
	{0x90, 0x02, 0x01, 0x48, 0x48, 0x7b, 0x00, 0x0A, 0x29, 0x75},
	{0x01, 0x00, 0x01, 0x49, 0x49, 0x7c, 0x00, 0x0C, 0x2A, 0x75},
	{0x91, 0x02, 0x01, 0x4a, 0x4a, 0x7d, 0x00, 0x0E, 0x03, 0x75},
	{0x02, 0x00, 0x01, 0x4c, 0x4c, 0x7d, 0x00, 0x11, 0x2E, 0x75},
	{0x92, 0x02, 0x01, 0x4d, 0x4d, 0x7e, 0x00, 0x14, 0x30, 0x75},
	{0x03, 0x00, 0x01, 0x4f, 0x4f, 0x7e, 0x00, 0x19, 0x03, 0x75},
	{0x93, 0x02, 0x01, 0x51, 0x51, 0x7e, 0x00, 0x1E, 0x09, 0x75},
	{0x04, 0x00, 0x01, 0x53, 0x53, 0x7f, 0x00, 0x23, 0x35, 0x75},
};

static const u32 gain_level_table_linear_ya[27] = {
	64,	//1.000000
	73,	//1.156250
	89,	//1.406250
	107,	//1.640625
	126,	//1.968750
	144,	//2.218750
	170,	//2.562500
	204,	//3.125000
	239,	//3.734375
	284,	//4.437500
	338,	//5.328125
	416,	//6.359375
	486,	//7.640625
	543,	//8.312500
	635,	//9.875000
	763,	//12.000000
	899,	//14.828125
	1061,	//16.375000
	1270,	//19.640625
	1521,	//23.437500
	1798,	//27.531250
	2119,	//33.062500
	2552,	//39.375000
	3033,	//47.640625
	3593,	//57.312500
	4216,	//68.234375
	0xffff,
};

static const u32 reg_val_table_linear_ya[26][8] = {
	//0614 0615  0225  1467  1468  00b8  00b9  1447
	{0x00, 0x00, 0x00, 0x07, 0x07, 0x01, 0x00, 0x77},
	{0x90, 0x02, 0x00, 0x07, 0x07, 0x01, 0x09, 0x77},
	{0x01, 0x00, 0x00, 0x08, 0x08, 0x01, 0x19, 0x77},
	{0x91, 0x02, 0x00, 0x08, 0x08, 0x01, 0x2A, 0x77},
	{0x02, 0x00, 0x00, 0x09, 0x09, 0x01, 0x3D, 0x77},
	{0x00, 0x00, 0x00, 0x07, 0x07, 0x02, 0x10, 0x75},
	{0x90, 0x02, 0x00, 0x07, 0x07, 0x02, 0x29, 0x75},
	{0x01, 0x00, 0x00, 0x08, 0x08, 0x03, 0x0B, 0x75},
	{0x91, 0x02, 0x00, 0x08, 0x08, 0x03, 0x2F, 0x75},
	{0x02, 0x00, 0x00, 0x08, 0x08, 0x04, 0x1C, 0x75},
	{0x92, 0x02, 0x00, 0x09, 0x09, 0x05, 0x11, 0x75},
	{0x03, 0x00, 0x00, 0x0a, 0x0a, 0x06, 0x20, 0x75},
	{0x93, 0x02, 0x00, 0x0b, 0x0b, 0x07, 0x25, 0x75},
	{0x00, 0x00, 0x01, 0x0c, 0x0c, 0x08, 0x1E, 0x75},
	{0x90, 0x02, 0x01, 0x0d, 0x0d, 0x09, 0x3B, 0x75},
	{0x01, 0x00, 0x01, 0x0d, 0x0d, 0x0B, 0x3B, 0x75},
	{0x91, 0x02, 0x01, 0x0e, 0x0e, 0x0E, 0x03, 0x75},
	{0x02, 0x00, 0x01, 0x0f, 0x0f, 0x10, 0x25, 0x75},
	{0x92, 0x02, 0x01, 0x10, 0x10, 0x13, 0x35, 0x75},
	{0x03, 0x00, 0x01, 0x11, 0x11, 0x17, 0x30, 0x75},
	{0x93, 0x02, 0x01, 0x13, 0x13, 0x1C, 0x06, 0x75},
	{0x04, 0x00, 0x01, 0x14, 0x14, 0x21, 0x07, 0x75},
	{0x94, 0x02, 0x01, 0x15, 0x15, 0x27, 0x38, 0x75},
	{0x05, 0x00, 0x01, 0x17, 0x17, 0x2F, 0x18, 0x75},
	{0x95, 0x02, 0x01, 0x19, 0x19, 0x38, 0x09, 0x75},
	{0x06, 0x00, 0x01, 0x1a, 0x1a, 0x41, 0x37, 0x75},
};

static const u32 gain_level_table_hdr_ya[27] = {
	64,
	74,
	89,
	104,
	126,
	141,
	164,
	200,
	238,
	283,
	340,
	407,
	489,
	531,
	632,
	768,
	948,
	1047,
	1257,
	1500,
	1761,
	2115,
	2520,
	3049,
	3668,
	4367,
};

static const u32 reg_val_table_hdr_ya[26][8] = {
	//analog gain不分T1 T2
	//0614  0615  0225  1467  1468  00b8  00b9  1447     | CG  |  实际倍数 | Again dB|
	{ 0x00, 0x00, 0x00, 0x0a, 0x0a, 0x01, 0x00, 0x77}, //| LCG |  X1       | 0.00    |
	{ 0x90, 0x02, 0x00, 0x0b, 0x0b, 0x01, 0x0a, 0x77}, //| LCG |  X1.164   | 1.32    |
	{ 0x01, 0x00, 0x00, 0x0c, 0x0c, 0x01, 0x1a, 0x77}, //| LCG |  X1.406   | 2.96    |
	{ 0x91, 0x02, 0x00, 0x0c, 0x0c, 0x01, 0x29, 0x77}, //| LCG |  X1.639   | 4.29    |
	{ 0x02, 0x00, 0x00, 0x0d, 0x0d, 0x01, 0x3e, 0x77}, //| LCG |  X1.969   | 5.89    |
	{ 0x00, 0x00, 0x00, 0x0a, 0x0a, 0x02, 0x0d, 0x75}, //| HCG |  X2.211   | 6.89    |
	{ 0x90, 0x02, 0x00, 0x0b, 0x0b, 0x02, 0x24, 0x75}, //| HCG |  X2.569   | 8.19    |
	{ 0x01, 0x00, 0x00, 0x0c, 0x0c, 0x03, 0x08, 0x75}, //| HCG |  X3.126   | 9.90    |
	{ 0x91, 0x02, 0x00, 0x0c, 0x0c, 0x03, 0x2e, 0x75}, //| HCG |  X3.729   | 11.43   |
	{ 0x02, 0x00, 0x00, 0x0d, 0x0d, 0x04, 0x1b, 0x75}, //| HCG |  X4.430   | 12.93   |
	{ 0x92, 0x02, 0x00, 0x0e, 0x0e, 0x05, 0x14, 0x75}, //| HCG |  X5.325   | 14.53   |
	{ 0x03, 0x00, 0x00, 0x0f, 0x0f, 0x06, 0x17, 0x75}, //| HCG |  X6.365   | 16.08   |
	{ 0x93, 0x02, 0x00, 0x10, 0x10, 0x07, 0x29, 0x75}, //| HCG |  X7.645   | 17.67   |
	{ 0x00, 0x00, 0x01, 0x12, 0x12, 0x08, 0x13, 0x75}, //| HCG |  X8.307   | 18.39   |
	{ 0x90, 0x02, 0x01, 0x12, 0x12, 0x09, 0x38, 0x75}, //| HCG |  X9.879   | 19.89   |
	{ 0x01, 0x00, 0x01, 0x14, 0x14, 0x0c, 0x00, 0x75}, //| HCG |  X12.005  | 21.59   |
	{ 0x91, 0x02, 0x01, 0x14, 0x14, 0x0e, 0x35, 0x75}, //| HCG |  X14.822  | 23.42   |
	{ 0x02, 0x00, 0x01, 0x16, 0x16, 0x10, 0x18, 0x75}, //| HCG |  X16.369  | 24.28   |
	{ 0x92, 0x02, 0x01, 0x17, 0x17, 0x13, 0x29, 0x75}, //| HCG |  X19.645  | 25.86   |
	{ 0x03, 0x00, 0x01, 0x19, 0x19, 0x17, 0x1c, 0x75}, //| HCG |  X23.442  | 27.40   |
	{ 0x93, 0x02, 0x01, 0x1a, 0x1a, 0x1b, 0x22, 0x75}, //| HCG |  X27.528  | 28.80   |
	{ 0x04, 0x00, 0x01, 0x1d, 0x1d, 0x21, 0x04, 0x75}, //| HCG |  X33.062  | 30.39   |
	{ 0x94, 0x02, 0x01, 0x1e, 0x1e, 0x27, 0x18, 0x75}, //| HCG |  X39.379  | 31.91   |
	{ 0x05, 0x00, 0x01, 0x21, 0x21, 0x2f, 0x29, 0x75}, //| HCG |  X47.643  | 33.56   |
	{ 0x95, 0x02, 0x01, 0x22, 0x22, 0x39, 0x0b, 0x75}, //| HCG |  X57.32   | 35.17   |
	{ 0x06, 0x00, 0x01, 0x26, 0x26, 0x44, 0x0f, 0x75}, //| HCG |  X68.24   | 36.68   |
};


static const u32 gain_level_table_dag_yn[23] = {
	64,
	74,
	90,
	105,
	126,
	142,
	164,
	200,
	239,
	284,
	341,
	407,
	489,
	532,
	632,
	768,
	949,
	1048,
	1257,
	1500,
	1762,
	2116,
	0xffff,
};

static const u32 reg_val_table_dag_yn[22][10] = {
	//614	615	 225   1467	 1468  26e	 270   1447	  b8	b9
	{0x00, 0x00, 0x00, 0x46, 0x46, 0x74, 0x02, 0x77, 0x01, 0x00},
	{0x90, 0x02, 0x00, 0x47, 0x47, 0x74, 0x02, 0x77, 0x01, 0x0a},
	{0x01, 0x00, 0x00, 0x47, 0x47, 0x77, 0x02, 0x77, 0x01, 0x1a},
	{0x91, 0x02, 0x00, 0x48, 0x48, 0x77, 0x02, 0x77, 0x01, 0x29},
	{0x02, 0x00, 0x00, 0x48, 0x48, 0x79, 0x02, 0x77, 0x01, 0x3e},
	{0x00, 0x00, 0x00, 0x46, 0x46, 0x74, 0x02, 0x75, 0x02, 0x0d},
	{0x90, 0x02, 0x00, 0x47, 0x47, 0x74, 0x02, 0x75, 0x02, 0x24},
	{0x01, 0x00, 0x00, 0x47, 0x47, 0x77, 0x02, 0x75, 0x03, 0x08},
	{0x91, 0x02, 0x00, 0x48, 0x48, 0x79, 0x02, 0x75, 0x03, 0x2e},
	{0x02, 0x00, 0x00, 0x49, 0x49, 0x7a, 0x02, 0x75, 0x04, 0x1b},
	{0x92, 0x02, 0x00, 0x4b, 0x4b, 0x7b, 0x02, 0x75, 0x05, 0x14},
	{0x03, 0x00, 0x00, 0x4c, 0x4c, 0x7c, 0x02, 0x75, 0x06, 0x17},
	{0x93, 0x02, 0x00, 0x4d, 0x4d, 0x7d, 0x02, 0x75, 0x07, 0x29},
	{0x00, 0x00, 0x01, 0x4f, 0x4f, 0x7e, 0x02, 0x75, 0x08, 0x13},
	{0x90, 0x02, 0x01, 0x50, 0x50, 0x7f, 0x02, 0x75, 0x09, 0x38},
	{0x01, 0x00, 0x01, 0x51, 0x51, 0x7f, 0x02, 0x75, 0x0c, 0x00},
	{0x91, 0x02, 0x01, 0x53, 0x53, 0x7f, 0x02, 0x75, 0x0e, 0x35},
	{0x02, 0x00, 0x01, 0x54, 0x54, 0x7f, 0x02, 0x75, 0x10, 0x18},
	{0x92, 0x02, 0x01, 0x56, 0x56, 0x7f, 0x02, 0x75, 0x13, 0x29},
	{0x03, 0x00, 0x01, 0x58, 0x58, 0x7f, 0x02, 0x75, 0x17, 0x1c},
	{0x93, 0x02, 0x01, 0x5a, 0x5a, 0x7f, 0x01, 0x75, 0x1b, 0x22},
	{0x04, 0x00, 0x01, 0x5c, 0x5c, 0x7f, 0x01, 0x75, 0x21, 0x04},
};

static const u32 gain_level_table_linear_yn[27] = {
	64,
	74,
	90,
	105,
	126,
	142,
	164,
	200,
	239,
	284,
	341,
	407,
	489,
	532,
	632,
	768,
	949,
	1048,
	1257,
	1500,
	1762,
	2116,
	2520,
	3049,
	3668,
	4367,
	0xffff,
};

static const u32 reg_val_table_linear_yn[26][8] = {
//  0614   0615  225   1467	 1468  1447	  b8	b9
	{0x00, 0x00, 0x00, 0x0d, 0x0d, 0x77, 0x01, 0x00},
	{0x90, 0x02, 0x00, 0x0e, 0x0e, 0x77, 0x01, 0x0a},
	{0x01, 0x00, 0x00, 0x0e, 0x0e, 0x77, 0x01, 0x1a},
	{0x91, 0x02, 0x00, 0x0f, 0x0f, 0x77, 0x01, 0x29},
	{0x02, 0x00, 0x00, 0x0f, 0x0f, 0x77, 0x01, 0x3e},
	{0x00, 0x00, 0x00, 0x0d, 0x0d, 0x75, 0x02, 0x0d},
	{0x90, 0x02, 0x00, 0x0d, 0x0d, 0x75, 0x02, 0x24},
	{0x01, 0x00, 0x00, 0x0e, 0x0e, 0x75, 0x03, 0x08},
	{0x91, 0x02, 0x00, 0x0e, 0x0e, 0x75, 0x03, 0x2e},
	{0x02, 0x00, 0x00, 0x0f, 0x0f, 0x75, 0x04, 0x1b},
	{0x92, 0x02, 0x00, 0x0f, 0x0f, 0x75, 0x05, 0x14},
	{0x03, 0x00, 0x00, 0x10, 0x10, 0x75, 0x06, 0x17},
	{0x93, 0x02, 0x00, 0x10, 0x10, 0x75, 0x07, 0x29},
	{0x00, 0x00, 0x01, 0x11, 0x11, 0x75, 0x08, 0x13},
	{0x90, 0x02, 0x01, 0x12, 0x12, 0x75, 0x09, 0x38},
	{0x01, 0x00, 0x01, 0x13, 0x13, 0x75, 0x0c, 0x00},
	{0x91, 0x02, 0x01, 0x14, 0x14, 0x75, 0x0e, 0x35},
	{0x02, 0x00, 0x01, 0x15, 0x15, 0x75, 0x10, 0x18},
	{0x92, 0x02, 0x01, 0x16, 0x16, 0x75, 0x13, 0x29},
	{0x03, 0x00, 0x01, 0x17, 0x17, 0x75, 0x17, 0x1c},
	{0x93, 0x02, 0x01, 0x18, 0x18, 0x75, 0x1b, 0x22},
	{0x04, 0x00, 0x01, 0x19, 0x19, 0x75, 0x21, 0x04},
	{0x94, 0x02, 0x01, 0x1b, 0x1b, 0x75, 0x27, 0x18},
	{0x05, 0x00, 0x01, 0x1d, 0x1d, 0x75, 0x2f, 0x29},
	{0x95, 0x02, 0x01, 0x1e, 0x1e, 0x75, 0x39, 0x0b},
	{0x06, 0x00, 0x01, 0x20, 0x20, 0x75, 0x44, 0x0f},
};


/*
 * version 2.2
 * <MODE_8 type="MODE_8_GC8613_3840*2160_30fps_raw12_4lane_DAG_1001_YA002">
 * mclk 24MHz, mipiclk 1188Mbps, wpclk 216MHz, rpclk 216MHz
 * rowtime 14.52us, vts 2295
 * darksun on, HDR off, fixposition off, DAG on
 * 3467ns
 */
static const struct regval gc8613ya_dag_12bit_3840x2160_30fps_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0a38, 0x01},
	{0x0a20, 0x19},
	{0x061b, 0x17},
	{0x061c, 0x44},
	{0x061d, 0x05},
	{0x061e, 0x5a},
	{0x061f, 0x05},
	{0x0a21, 0x24},
	{0x0a31, 0xc6},
	{0x0a34, 0x40},
	{0x0a35, 0x08},
	{0x0a37, 0x44},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x03},
	{0x0343, 0x10},
	{0x0259, 0x08},
	{0x025a, 0x98},

	//30fps 0x08f8
	{0x0340, 0x08},
	{0x0341, 0xf8},

	//15fps:0x11f0
	//{0x0340, 0x11},
	//{0x0341, 0xf0},

	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0f},
	{0x0349, 0x18},
	{0x034a, 0x08},
	{0x034b, 0x88},
	{0x034f, 0xf0},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x0c},
	{0x009b, 0x0c},
	{0x060c, 0x06},
	{0x060e, 0x20},
	{0x060f, 0x0f},
	{0x070c, 0x06},
	{0x070e, 0x20},
	{0x070f, 0x0f},
	{0x0087, 0x50},
	{0x141b, 0x03},
	{0x0907, 0xd5},
	{0x0909, 0x06},
	{0x0901, 0x0e},
	{0x0902, 0x0b},
	{0x0904, 0x08},
	{0x0908, 0x09},
	{0x0903, 0xc5},
	{0x090c, 0x09},
	{0x0905, 0x10},
	{0x0906, 0x00},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x072b, 0x1c},
	{0x072a, 0x7c},
	{0x073e, 0x40},
	{0x0078, 0x88},
	{0x0268, 0x40},
	{0x0269, 0x44},
	{0x0351, 0x54},
	{0x0618, 0x01},
	{0x1466, 0x45},
	{0x1468, 0x3e},
	{0x1467, 0x3e},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x1469, 0xf0},
	{0x146a, 0xd0},
	{0x146b, 0x03},
	{0x1480, 0x07},
	{0x1481, 0x80},
	{0x1484, 0x0b},
	{0x1485, 0xc0},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1434, 0x04},
	{0x1447, 0x75},
	{0x1470, 0x10},
	{0x1471, 0x13},
	{0x0122, 0x0b},
	{0x0123, 0x30},
	{0x0124, 0x04},
	{0x0125, 0x30},
	{0x0126, 0x0f},
	{0x0127, 0x15},
	{0x0128, 0xa8},
	{0x0129, 0x0c},
	{0x012a, 0x18},
	{0x012b, 0x18},
	{0x1438, 0x00},
	{0x143a, 0x00},
	{0x024b, 0x02},
	{0x0245, 0xc7},
	{0x025b, 0x07},
	{0x02bb, 0x77},
	{0x0612, 0x01},
	{0x0613, 0x24},
	{0x0243, 0x66},
	{0x1467, 0x3e},
	{0x1468, 0x3e},
	{0x0087, 0x53},
	{0x0053, 0x05},
	{0x0089, 0x00},
	{0x0004, 0x0f},
	{0x0002, 0xeb},
	{0x005a, 0x0c},
	{0x0040, 0x83},
	{0x0075, 0x68},
	{0x0205, 0x0c},
	{0x0202, 0x03},
	{0x0203, 0x27},
	{0x061a, 0x02},
	{0x0213, 0x64},
	{0x0265, 0x01},
	{0x0618, 0x05},
	{0x026e, 0x74},
	{0x0270, 0x02},
	{0x0709, 0x00},
	{0x0719, 0x00},
	{0x0812, 0xdb},
	{0x0822, 0x0f},
	{0x0821, 0x18},
	{0x0002, 0xef},
	{0x0813, 0xfb},
	{0x0070, 0x88},
	{0x79cf, 0x01},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0136, 0x00},
	{0x0181, 0xf0},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0106, 0x38},
	{0x010d, 0x80},
	{0x010e, 0x16},
	{0x0111, 0x2c},
	{0x0112, 0x02},
	{0x0114, 0x03},
	{0x0100, 0x09},
	{0x79cf, 0x01},
	{0x0219, 0x47},
	{0x0054, 0x98},
	{0x0076, 0x01},
	{0x0052, 0x02},
	{0x021a, 0x10},
	{0x0430, 0x04},
	{0x0431, 0x04},
	{0x0432, 0x04},
	{0x0433, 0x04},
	{0x0434, 0x81},
	{0x0435, 0x81},
	{0x0436, 0x81},
	{0x0437, 0x81},
	{0x0704, 0x03},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x071d, 0xdc},
	{0x071e, 0x05},
	{0x1469, 0x80},
	//otp  autoload
	{0x031f, 0x01},
	{0x031f, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x04},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0089, 0x00},
	{0x0aa0, 0x00},
	{0x0023, 0x00},
	{0x0022, 0x00},
	{0x0025, 0x00},
	{0x0024, 0x00},
	{0x0028, 0x0f},
	{0x0029, 0x18},
	{0x002a, 0x08},
	{0x002b, 0x88},
	{0x0317, 0x1c},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0xe0},
	{0x0a71, 0x00},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{REG_DELAY, 0x14}, //sleep	20
	{0x0089, 0x00},
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x024b, 0x02},
	{0x0220, 0x80},
	{0x0058, 0x00},
	{0x0059, 0x04},
	{REG_NULL, 0x00},
};

/*
 * version v2.2
 * <MODE_2 type="MODE_2_GC8613_3840*2160_30fps_raw10_4lane_1000_YA002">
 * mclk 24MHz, mipiclk 1004Mbps, wpclk 216MHz, rpclk 200.8MHz
 * rowtime 14.81us, vts 2250,
 * darksun on, HDR off, fixposition off, DAG off
 * 3467ns
 */
static const struct regval __maybe_unused gc8613ya_linear_10bit_3840x2160_30fps_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0a38, 0x01},
	{0x0a20, 0x19},
	{0x061b, 0x17},
	{0x061c, 0x50},
	{0x061d, 0x06},
	{0x061e, 0x87},
	{0x061f, 0x05},
	{0x0a21, 0x10},
	{0x0a30, 0x00},
	{0x0a31, 0xfb},
	{0x0a34, 0x40},
	{0x0a35, 0x08},
	{0x0a37, 0x46},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x03},
	{0x0343, 0x20},
	{0x0259, 0x08},
	{0x025a, 0x96},
	{0x0340, 0x08},
	{0x0341, 0xca},
	{0x0351, 0x00},
	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0f},
	{0x0349, 0x18},
	{0x034a, 0x08},
	{0x034b, 0x88},
	{0x034f, 0xf0},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x0c},
	{0x009b, 0x0c},
	{0x060c, 0x06},
	{0x060e, 0x20},
	{0x060f, 0x0f},
	{0x070c, 0x06},
	{0x070e, 0x20},
	{0x070f, 0x0f},
	{0x0087, 0x50},
	{0x141b, 0x03},
	{0x0907, 0xd5},
	{0x0909, 0x06},
	{0x0901, 0x0e},
	{0x0902, 0x0b},
	{0x0904, 0x08},
	{0x0908, 0x09},
	{0x0903, 0xc5},
	{0x090c, 0x09},
	{0x0905, 0x10},
	{0x0906, 0x00},
	{0x072a, 0x7c},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x072b, 0x1c},
	{0x073e, 0x40},
	{0x0078, 0x88},
	{0x0618, 0x01},
	{0x1466, 0x12},
	{0x1468, 0x07},
	{0x1467, 0x07},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x1469, 0x80},
	{0x146a, 0xc0},
	{0x146b, 0x03},
	{0x1480, 0x02},
	{0x1481, 0x80},
	{0x1484, 0x08},
	{0x1485, 0xc0},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1434, 0x04},
	{0x1447, 0x75},
	{0x1470, 0x10},
	{0x1471, 0x13},
	{0x0122, 0x0b},
	{0x0123, 0x30},
	{0x0124, 0x0b},
	{0x0126, 0x09},
	{0x0129, 0x0b},
	{0x012a, 0x16},
	{0x012b, 0x0a},
	{0x1438, 0x00},
	{0x143a, 0x00},
	{0x024b, 0x02},
	{0x0245, 0xc7},
	{0x025b, 0x07},
	{0x02bb, 0x77},
	{0x0612, 0x01},
	{0x0613, 0x26},
	{0x0243, 0x66},
	{0x0087, 0x53},
	{0x0053, 0x05},
	{0x0089, 0x02},
	{0x0002, 0xeb},
	{0x005a, 0x0c},
	{0x0040, 0x83},
	{0x0075, 0x54},
	{0x0205, 0x0c},
	{0x0202, 0x01},
	{0x0203, 0x27},
	{0x061a, 0x02},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0136, 0x00},
	{0x0181, 0xf0},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0106, 0x38},
	{0x010d, 0xc0},
	{0x010e, 0x12},
	{0x0113, 0x02},
	{0x0114, 0x03},
	{0x0100, 0x09},
	{0x0004, 0x0f},
	{0x0219, 0x47},
	{0x0054, 0x98},
	{0x0076, 0x01},
	{0x0052, 0x02},
	{0x021a, 0x10},
	{0x0430, 0x10},
	{0x0431, 0x10},
	{0x0432, 0x10},
	{0x0433, 0x10},
	{0x0434, 0x6d},
	{0x0435, 0x6d},
	{0x0436, 0x6d},
	{0x0437, 0x6d},
	{0x0704, 0x03},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x071d, 0xdc},
	{0x071e, 0x05}, //otp autoload
	{0x031f, 0x01},
	{0x031f, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x04},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0089, 0x02},
	{0x0aa0, 0x00},
	{0x0023, 0x00},
	{0x0022, 0x00},
	{0x0025, 0x00},
	{0x0024, 0x00},
	{0x0028, 0x0f},
	{0x0029, 0x18},
	{0x002a, 0x08},
	{0x002b, 0x88},
	{0x0317, 0x1c},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0xe0},
	{0x0a71, 0x00},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{REG_DELAY, 0x14}, //sleep 20
	{0x0089, 0x02},
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x024b, 0x02},
	{0x0220, 0x80},
	{0x0058, 0x00},
	{0x0059, 0x04},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 40fps
 * mipi_datarate per lane 1458Mbps, 4lane
 * 10-bit 3840x2160@40fps
 * linear mode
 * release_liner_v2.2.1_04_4lane_raw10_3840x2160_40fps_24mhz_GC8613YA.txt
 */
static const struct regval __maybe_unused gc8613ya_linear_10bit_3840x2160_40fps_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0a38, 0x01},
	{0x0a20, 0x19},
	{0x061b, 0x17},
	{0x061c, 0x44},
	{0x061d, 0x05},
	{0x061e, 0x75},
	{0x061f, 0x04},
	{0x0a21, 0x10},
	{0x0a30, 0x00},
	{0x0a31, 0xf3},
	{0x0a34, 0x40},
	{0x0a35, 0x08},
	{0x0a37, 0x44},
	{0x0314, 0x50},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x03},
	{0x0343, 0xb4},
	{0x0259, 0x08},
	{0x025a, 0x96},
	{0x0340, 0x09},
	{0x0341, 0x0a},
	{0x0351, 0x00},
	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0f},
	{0x0349, 0x18},
	{0x034a, 0x08},
	{0x034b, 0x88},
	{0x034f, 0xf0},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x0c},
	{0x009b, 0x0c},
	{0x060c, 0x06},
	{0x060e, 0x20},
	{0x060f, 0x0f},
	{0x070c, 0x06},
	{0x070e, 0x20},
	{0x070f, 0x0f},
	{0x0087, 0x50},
	{0x141b, 0x03},
	{0x0901, 0x0e},
	{0x0907, 0xd5},
	{0x0909, 0x06},
	{0x0902, 0x0b},
	{0x0904, 0x08},
	{0x0908, 0x09},
	{0x0903, 0xc5},
	{0x090c, 0x09},
	{0x0905, 0x10},
	{0x0906, 0x00},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x072b, 0x1a},
	{0x072a, 0x5e},
	{0x0601, 0x00},
	{0x073e, 0x40},
	{0x0078, 0x88},
	{0x0618, 0x01},
	{0x1466, 0x12},
	{0x1468, 0x07},
	{0x1467, 0x07},
	{0x1478, 0x10},
	{0x1477, 0x10},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x1469, 0x80},
	{0x146a, 0x20},
	{0x146b, 0x03},
	{0x1479, 0x80},
	{0x147a, 0x20},
	{0x147b, 0x03},
	{0x1480, 0x02},
	{0x1481, 0x80},
	{0x1482, 0x02},
	{0x1483, 0x80},
	{0x1484, 0x08},
	{0x1485, 0xc0},
	{0x1486, 0x08},
	{0x1487, 0xc0},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1434, 0x04},
	{0x1447, 0x75},
	{0x140d, 0x04},
	{0x1461, 0x10},
	{0x146c, 0x10},
	{0x146d, 0x10},
	{0x146e, 0x2e},
	{0x146f, 0x30},
	{0x1474, 0x34},
	{0x1470, 0x10},
	{0x1471, 0x13},
	{0x143a, 0x00},
	{0x024b, 0x02},
	{0x0245, 0xc7},
	{0x025b, 0x07},
	{0x02bb, 0x77},
	{0x0612, 0x01},
	{0x0613, 0x26},
	{0x0243, 0x66},
	{0x0087, 0x53},
	{0x0053, 0x05},
	{0x0089, 0x02},
	{0x0002, 0xeb},
	{0x005a, 0x0c},
	{0x0040, 0x83},
	{0x0075, 0x58},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x061a, 0x02},
	{0x0122, 0x12},
	{0x0123, 0x50},
	{0x0126, 0x0f},
	{0x0129, 0x10},
	{0x012a, 0x20},
	{0x012b, 0x10},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0136, 0x00},
	{0x0181, 0xf0},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0106, 0x38},
	{0x010d, 0xc0},
	{0x010e, 0x12},
	{0x0113, 0x02},
	{0x0114, 0x03},
	{0x0100, 0x09},
	{0x0619, 0x01},
	{0x023b, 0x58},
	{0x023e, 0x00},
	{0x023f, 0x84},
	{0x0220, 0x80},
	{0x021b, 0x96},
	{0x0004, 0x0f},
	{0x000e, 0x07},
	{0x0219, 0x47},
	{0x0054, 0x98},
	{0x0076, 0x01},
	{0x0052, 0x02},
	{0x021a, 0x10},
	{0x0430, 0x10},
	{0x0431, 0x10},
	{0x0432, 0x10},
	{0x0433, 0x10},
	{0x0434, 0x6d},
	{0x0435, 0x6d},
	{0x0436, 0x6d},
	{0x0437, 0x6d},
	{0x0704, 0x03},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x071d, 0xdc},
	{0x071e, 0x05}, //otp autoload
	{0x031f, 0x01},
	{0x031f, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x04},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0089, 0x02},
	{0x0aa0, 0x00},
	{0x0023, 0x00},
	{0x0022, 0x00},
	{0x0025, 0x00},
	{0x0024, 0x00},
	{0x0028, 0x0f},
	{0x0029, 0x18},
	{0x002a, 0x08},
	{0x002b, 0x88},
	{0x0317, 0x1c},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0xe0},
	{0x0a71, 0x00},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{REG_DELAY, 0x14}, //sleep 20
	{0x0089, 0x02},
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x0020, 0x01},
	{0x024b, 0x02},
	{0x0220, 0x80},
	{0x0058, 0x00},
	{0x0059, 0x04},
	{REG_NULL, 0x00},
};

//version 2.2
//<MODE_2 type="0A_GC8613_YA_MIPI4L_24M_3840x2160_30fps_raw10_HDR">
//mclk 24MHz, mipiclk 1458Mbps, wpclk 351MHz, rpclk 291.6MHz
//rowtime 14.404us, vts 2314
//darksun on, HDR on, fixposition off, DAG off
//5634ns
static const struct regval __maybe_unused gc8613ya_hdr_10bit_3840x2160_30fps_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0a38, 0x01},
	{0x0a20, 0x19},
	{0x061b, 0x17},
	{0x061c, 0x44},
	{0x061d, 0x05},
	{0x061e, 0x75},
	{0x061f, 0x04},
	{0x0a21, 0x10},
	{0x0a30, 0x00},
	{0x0a31, 0xf3},
	{0x0a34, 0x40},
	{0x0a35, 0x08},
	{0x0a37, 0x44},
	{0x0314, 0x50},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x02},
	{0x0343, 0x78},
	{0x0259, 0x08},
	{0x025a, 0x96},
	{0x0340, 0x09},
	{0x0341, 0x0a},
	{0x0351, 0x00},
	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0f},
	{0x0349, 0x18},
	{0x034a, 0x08},
	{0x034b, 0x88},
	{0x034f, 0xf0},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x0c},
	{0x009b, 0x0c},
	{0x060c, 0x06},
	{0x060e, 0x20},
	{0x060f, 0x0f},
	{0x070c, 0x06},
	{0x070e, 0x20},
	{0x070f, 0x0f},
	{0x0087, 0x50},
	{0x141b, 0x03},
	{0x023b, 0x18},
	{0x0619, 0x00},
	{0x0901, 0x0e},
	{0x0907, 0xd5},
	{0x0909, 0x06},
	{0x0902, 0x0b},
	{0x0904, 0x08},
	{0x0908, 0x09},
	{0x0903, 0xc5},
	{0x090c, 0x09},
	{0x0905, 0x10},
	{0x0906, 0x00},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x072b, 0x1a},
	{0x072a, 0x5e},
	{0x0601, 0x00},
	{0x073e, 0x40},
	{0x0078, 0x88},
	{0x0618, 0x01},
	{0x1466, 0x12},
	{0x1468, 0x10},
	{0x1467, 0x10},
	{0x1478, 0x10},
	{0x1477, 0x10},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x1469, 0x80},
	{0x146a, 0x20},
	{0x146b, 0x03},
	{0x1479, 0x80},
	{0x147a, 0x20},
	{0x147b, 0x03},
	{0x1480, 0x02},
	{0x1481, 0x80},
	{0x1482, 0x02},
	{0x1483, 0x80},
	{0x1484, 0x08},
	{0x1485, 0xc0},
	{0x1486, 0x08},
	{0x1487, 0xc0},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1434, 0x04},
	{0x1447, 0x75},
	{0x140d, 0x04},
	{0x1461, 0x10},
	{0x146c, 0x10},
	{0x146d, 0x10},
	{0x146e, 0x2e},
	{0x146f, 0x30},
	{0x1474, 0x34},
	{0x1470, 0x10},
	{0x1471, 0x13},
	{0x143a, 0x00},
	{0x024a, 0x66},
	{0x024b, 0x02},
	{0x0245, 0xc7},
	{0x025b, 0x07},
	{0x02bb, 0x77},
	{0x0612, 0x01},
	{0x0613, 0x26},
	{0x0243, 0x66},
	{0x0087, 0x53},
	{0x0053, 0x05},
	{0x0089, 0x02},
	{0x0002, 0xeb},
	{0x005a, 0x0c},
	{0x0040, 0x83},
	{0x0075, 0x58},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x061a, 0x02},
	{0x0122, 0x12},
	{0x0123, 0x50},
	{0x0126, 0x0f},
	{0x0129, 0x10},
	{0x012a, 0x20},
	{0x012b, 0x10},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0136, 0x00},
	{0x0181, 0xf0},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0106, 0x38},
	{0x010d, 0xc0},
	{0x010e, 0x12},
	{0x0113, 0x02},
	{0x0114, 0x03},
	{0x0100, 0x09},
	{0x0222, 0x49},
	{0x0107, 0x89},
	{0x023e, 0x00},
	{0x023f, 0x84},
	{0x0220, 0xcf},
	{0x021b, 0x69},
	{0x0004, 0x0f},
	{0x000e, 0x06}, //0x06:dpc long exp;0x07:dpc short exp
	{0x0219, 0x47},
	{0x0054, 0x98},
	{0x0076, 0x01},
	{0x0052, 0x02},
	{0x021a, 0x10},
	{0x0430, 0x10},
	{0x0431, 0x10},
	{0x0432, 0x10},
	{0x0433, 0x10},
	{0x0434, 0x6d},
	{0x0435, 0x6d},
	{0x0436, 0x6d},
	{0x0437, 0x6d},
	{0x0438, 0x10},
	{0x0439, 0x10},
	{0x043a, 0x10},
	{0x043b, 0x10},
	{0x043c, 0x6d},
	{0x043d, 0x6d},
	{0x043e, 0x6d},
	{0x043f, 0x6d},
	{0x0704, 0x03},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x071d, 0xdc},
	{0x071e, 0x05}, //otp autoload
	{0x031f, 0x01},
	{0x031f, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x04},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0089, 0x02},
	{0x0aa0, 0x00},
	{0x0023, 0x00},
	{0x0022, 0x00},
	{0x0025, 0x00},
	{0x0024, 0x00},
	{0x0028, 0x0f},
	{0x0029, 0x18},
	{0x002a, 0x08},
	{0x002b, 0x88},
	{0x0317, 0x1c},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0xe0},
	{0x0a71, 0x00},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{REG_DELAY, 0x14}, //sleep 20
	{0x0089, 0x02},
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x0020, 0x01},
	{0x0058, 0x00},
	{0x0059, 0x04},
	{REG_NULL, 0x00},
};

/*
 * version 1.6
 * <MODE_20 type="MODE_20_GC8613_3840*2160_30fps_raw12_4lane_DAG_1001_YN002">
 * mclk 24MHz, mipiclk 1188Mbps, wpclk 216MHz, rpclk 198MHz
 * rowtime 14.52us, vts 2296
 * darksun on, HDR off, fixposition off, DAG on
 *
 */
static const struct regval gc8613yn_dag_12bit_3840x2160_30fps_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0a38, 0x01},
	{0x0a20, 0x19},
	{0x061b, 0x17},
	{0x061c, 0x48},
	{0x061d, 0x05},
	{0x061e, 0x5a},
	{0x061f, 0x05},
	{0x0a21, 0x24},
	{0x0a31, 0xc6},
	{0x0a34, 0x40},
	{0x0a35, 0x08},
	{0x0a37, 0x44},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x03},
	{0x0343, 0x10},
	{0x0259, 0x08},
	{0x025a, 0x98},

	//30fps 0x08f8
	{0x0340, 0x08},
	{0x0341, 0xf8},

	//15fps:0x11f0
	//{0x0340, 0x11},
	//{0x0341, 0xf0},

	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0f},
	{0x0349, 0x18},
	{0x034a, 0x08},
	{0x034b, 0x88},
	{0x034f, 0xf0},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x0c},
	{0x009b, 0x0c},
	{0x060c, 0x06},
	{0x060e, 0x20},
	{0x060f, 0x0f},
	{0x070c, 0x06},
	{0x070e, 0x20},
	{0x070f, 0x0f},
	{0x0087, 0x50},
	{0x0907, 0xd5},
	{0x0909, 0x06},
	{0x0902, 0x0b},
	{0x0904, 0x08},
	{0x0908, 0x09},
	{0x0903, 0xc5},
	{0x090c, 0x09},
	{0x0905, 0x10},
	{0x0906, 0x00},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x072b, 0x1c},
	{0x072a, 0x7c},
	{0x073e, 0x40},
	{0x0078, 0x88},
	{0x0268, 0x40},
	{0x0269, 0x44},
	{0x0351, 0x54},
	{0x0618, 0x01},
	{0x1466, 0x45},
	{0x1468, 0x46},
	{0x1467, 0x46},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x1469, 0xf0},
	{0x146a, 0xd0},
	{0x146b, 0x03},
	{0x1480, 0x07},
	{0x1481, 0x80},
	{0x1484, 0x0b},
	{0x1485, 0xc0},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1434, 0x04},
	{0x1447, 0x75},
	{0x1470, 0x10},
	{0x1471, 0x13},
	{0x0122, 0x0b},
	{0x0123, 0x30},
	{0x0124, 0x04},
	{0x0125, 0x30},
	{0x0126, 0x0f},
	{0x0127, 0x15},
	{0x0128, 0xa8},
	{0x0129, 0x0c},
	{0x012a, 0x18},
	{0x012b, 0x18},
	{0x1438, 0x00},
	{0x143a, 0x00},
	{0x024b, 0x02},
	{0x0245, 0xc7},
	{0x025b, 0x07},
	{0x02bb, 0x77},
	{0x0612, 0x01},
	{0x0613, 0x24},
	{0x0243, 0x66},
	{0x0087, 0x53},
	{0x0053, 0x05},
	{0x0089, 0x00},
	{0x0002, 0xeb},
	{0x005a, 0x0c},
	{0x0040, 0x83},
	{0x0075, 0x68},
	{0x0205, 0x0c},
	{0x0202, 0x03},
	{0x0203, 0x27},
	{0x061a, 0x02},
	{0x0213, 0x64},
	{0x0265, 0x01},
	{0x0618, 0x05},
	{0x026e, 0x74},
	{0x0270, 0x02},
	{0x0709, 0x00},
	{0x0719, 0x00},
	{0x0812, 0xdb},
	{0x0822, 0x0f},
	{0x0821, 0x18},
	{0x0002, 0xef},
	{0x0813, 0xfb},
	{0x0070, 0x88},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0136, 0x00},
	{0x0181, 0xf0},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0106, 0x38},
	{0x010d, 0x80},
	{0x010e, 0x16},
	{0x0111, 0x2c},
	{0x0112, 0x02},
	{0x0114, 0x03},
	{0x0100, 0x09},
	{0x79cf, 0x01},
	{0x0219, 0x47},
	{0x0054, 0x98},
	{0x0076, 0x01},
	{0x0052, 0x02},
	{0x021a, 0x10},
	{0x0430, 0x05},
	{0x0431, 0x05},
	{0x0432, 0x05},
	{0x0433, 0x05},
	{0x0434, 0x70},
	{0x0435, 0x70},
	{0x0436, 0x70},
	{0x0437, 0x70},
	{0x0004, 0x0f},
	{0x0704, 0x03},
	{0x071d, 0xdc},
	{0x071e, 0x05},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x071d, 0xdc},
	{0x071e, 0x05},
	{0x1469, 0x80},
	//otp autoload
	{0x031f, 0x01},
	{0x031f, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x04},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0089, 0x02},
	{0x0aa0, 0x00},
	{0x0023, 0x00},
	{0x0022, 0x00},
	{0x0025, 0x00},
	{0x0024, 0x00},
	{0x0028, 0x0f},
	{0x0029, 0x18},
	{0x002a, 0x08},
	{0x002b, 0x88},
	{0x0317, 0x1c},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0xe0},
	{0x0a71, 0x00},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{REG_DELAY, 0x14}, //sleep	20
	{0x0089, 0x00},
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x024b, 0x02},
	{0x0220, 0x80},
	{0x0058, 0x00},
	{0x0059, 0x04},
	{REG_NULL, 0x00},
};

/*
 * version v1.6
 * <MODE_12 type="MODE_12_GC8613_3840*2160_30fps_raw10_4lane_1000_YN002">
 * mclk 24MHz, mipiclk 1004Mbps, wpclk 216MHz, rpclk 200.8MHz
 * rowtime 14.81us, vts 2250
 * darksun on, HDR off, fixposition off, DAG off
 *
 */
static const struct regval __maybe_unused gc8613yn_linear_10bit_3840x2160_30fps_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0a38, 0x01},
	{0x0a20, 0x19},
	{0x061b, 0x17},
	{0x061c, 0x50},
	{0x061d, 0x06},
	{0x061e, 0x87},
	{0x061f, 0x05},
	{0x0a21, 0x10},
	{0x0a31, 0xfb},
	{0x0a34, 0x40},
	{0x0a35, 0x08},
	{0x0a37, 0x46},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x03},
	{0x0343, 0x20},
	{0x0259, 0x08},
	{0x025a, 0x96},

	//30fps 0x08ca
	//{0x0340, 0x08},
	//{0x0341, 0xca},

	//15fps:0x1194
	{0x0340, 0x11},
	{0x0341, 0x94},

	{0x0351, 0x00},
	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0f},
	{0x0349, 0x18},
	{0x034a, 0x08},
	{0x034b, 0x88},
	{0x034f, 0xf0},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x0c},
	{0x009b, 0x0c},
	{0x060c, 0x06},
	{0x060e, 0x20},
	{0x060f, 0x0f},
	{0x070c, 0x06},
	{0x070e, 0x20},
	{0x070f, 0x0f},
	{0x0087, 0x50},
	{0x0907, 0xd5},
	{0x0909, 0x06},
	{0x0902, 0x0b},
	{0x0904, 0x08},
	{0x0908, 0x09},
	{0x0903, 0xc5},
	{0x090c, 0x09},
	{0x0905, 0x10},
	{0x0906, 0x00},
	{0x072a, 0x7c},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x072b, 0x1c},
	{0x073e, 0x40},
	{0x0078, 0x88},
	{0x0618, 0x01},
	{0x1466, 0x12},
	{0x1468, 0x10},
	{0x1467, 0x10},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x1469, 0x80},
	{0x146a, 0xc0},
	{0x146b, 0x03},
	{0x1480, 0x02},
	{0x1481, 0x80},
	{0x1484, 0x08},
	{0x1485, 0xc0},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1434, 0x04},
	{0x1447, 0x75},
	{0x1470, 0x10},
	{0x1471, 0x13},
	{0x1438, 0x00},
	{0x143a, 0x00},
	{0x024b, 0x02},
	{0x0245, 0xc7},
	{0x025b, 0x07},
	{0x02bb, 0x77},
	{0x0612, 0x01},
	{0x0613, 0x26},
	{0x0243, 0x66},
	{0x0087, 0x53},
	{0x0053, 0x05},
	{0x0089, 0x02},
	{0x0002, 0xeb},
	{0x005a, 0x0c},
	{0x0040, 0x83},
	{0x0075, 0x54},
	{0x0205, 0x0c},
	{0x0202, 0x01},
	{0x0203, 0x27},
	{0x061a, 0x02},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0136, 0x00},
	{0x0181, 0xf0},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0106, 0x38},
	{0x010d, 0xc0},
	{0x010e, 0x12},
	{0x0113, 0x02},
	{0x0114, 0x03},
	{0x0100, 0x09},
	{0x0004, 0x0f},
	{0x0219, 0x47},
	{0x0054, 0x98},
	{0x0076, 0x01},
	{0x0052, 0x02},
	{0x021a, 0x10},
	{0x0430, 0x21},
	{0x0431, 0x21},
	{0x0432, 0x21},
	{0x0433, 0x21},
	{0x0434, 0x61},
	{0x0435, 0x61},
	{0x0436, 0x61},
	{0x0437, 0x61},
	{0x0704, 0x03},
	{0x071d, 0xdc},
	{0x071e, 0x05},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	//otp autoload
	{0x031f, 0x01},
	{0x031f, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x04},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0089, 0x02},
	{0x0aa0, 0x00},
	{0x0023, 0x00},
	{0x0022, 0x00},
	{0x0025, 0x00},
	{0x0024, 0x00},
	{0x0028, 0x0f},
	{0x0029, 0x18},
	{0x002a, 0x08},
	{0x002b, 0x88},
	{0x0317, 0x1c},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0xe0},
	{0x0a71, 0x00},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{REG_DELAY, 0x14}, // sleep 20
	{0x0089, 0x02},
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x024b, 0x02},
	{0x0220, 0x80},
	{0x0058, 0x00},
	{0x0059, 0x04},
	{REG_NULL, 0x00},
};

static const struct gc8613_mode supported_modes[] = {
	{
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0100,
		.hts_def = 0x0320,
		.vts_def = 0x08ca,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list[0] = gc8613yn_linear_10bit_3840x2160_30fps_regs,
		.reg_list[1] = gc8613ya_linear_10bit_3840x2160_30fps_regs,
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
			.denominator = 400000,
		},
		.exp_def = 0x0100,
		.hts_def = 0x03b4,
		.vts_def = 0x090a,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list[0] = NULL,
		.reg_list[1] = gc8613ya_linear_10bit_3840x2160_40fps_regs,
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
		.exp_def = 0x0327,
		.hts_def = 0x0310 * 8,
		.vts_def = 0x08f8,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB12_1X12,
		.reg_list[0] = gc8613yn_dag_12bit_3840x2160_30fps_regs,
		.reg_list[1] = gc8613ya_dag_12bit_3840x2160_30fps_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
		.mipi_freq_idx = 1,
		.bpp = 12,
	},
	{
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0100,
		.hts_def = 0x0278,
		.vts_def = 0x090a,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list[0] = gc8613yn_linear_10bit_3840x2160_30fps_regs,
		.reg_list[1] = gc8613ya_hdr_10bit_3840x2160_30fps_regs,
		.hdr_mode = HDR_X2,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,//L->csi wr0
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,//M->csi wr2
		.mipi_freq_idx = 0,
		.bpp = 10,
	},
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SRGGB12_1X12,
};

static const s64 link_freq_menu_items[] = {
	GC8613_LINK_FREQ_LINEAR,
	GC8613_LINK_FREQ_HDR,
};

static const char *const gc8613_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int gc8613_write_reg(struct i2c_client *client, u16 reg,
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

static int gc8613_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		if (regs[i].addr == REG_DELAY) {
			usleep_range(regs[i].val * 1000, regs[i].val * 1000);
			continue;
		}

		ret = gc8613_write_reg(client, regs[i].addr,
				       GC8613_REG_VALUE_08BIT, regs[i].val);
	}

	return ret;
}

/* Read registers up to 4 at a time */
static int gc8613_read_reg(struct i2c_client *client, u16 reg,
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

static int gc8613_get_reso_dist(const struct gc8613_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct gc8613_mode *
gc8613_find_best_fit(struct gc8613 *gc8613, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < gc8613->cfg_num; i++) {
		dist = gc8613_get_reso_dist(&supported_modes[i], framefmt);
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

static int gc8613_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc8613 *gc8613 = to_gc8613(sd);
	const struct gc8613_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&gc8613->mutex);

	mode = gc8613_find_best_fit(gc8613, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gc8613->mutex);
		return -ENOTTY;
#endif
	} else {
		gc8613->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc8613->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(gc8613->vblank, vblank_def,
					 GC8613_VTS_MAX - mode->height,
					 1, vblank_def);

		if (mode->hdr_mode == HDR_X2) {
			gc8613->cur_link_freq = 1;
			gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_HDR;
		} else {
			if (gc8613->cur_mode->bpp == 12) {
				gc8613->cur_link_freq = 1;
				gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_DAG;
			} else {
				gc8613->cur_link_freq = 0;
				gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_LINEAR;
			}
		}
		__v4l2_ctrl_s_ctrl_int64(gc8613->pixel_rate,
					 gc8613->cur_pixel_rate);
		__v4l2_ctrl_s_ctrl(gc8613->link_freq,
				   gc8613->cur_link_freq);
		gc8613->cur_vts = mode->vts_def;
		gc8613->cur_fps = mode->max_fps;
	}
	mutex_unlock(&gc8613->mutex);

	return 0;
}

static int gc8613_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc8613 *gc8613 = to_gc8613(sd);
	const struct gc8613_mode *mode = gc8613->cur_mode;

	mutex_lock(&gc8613->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gc8613->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&gc8613->mutex);

	return 0;
}

static int gc8613_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int gc8613_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct gc8613 *gc8613 = to_gc8613(sd);

	if (fse->index >= gc8613->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int gc8613_enable_test_pattern(struct gc8613 *gc8613, u32 pattern)
{
	u32 val;

	if (pattern)
		val = GC8613_TEST_PATTERN_ENABLE;
	else
		val = GC8613_TEST_PATTERN_DISABLE;

	return gc8613_write_reg(gc8613->client, GC8613_REG_TEST_PATTERN,
				GC8613_REG_VALUE_08BIT, val);
}

static int gc8613_set_gain_reg(struct gc8613 *gc8613, u32 gain)
{
	int i;
	int total;
	int tol_dig_gain;

	if (gain < 64)
		gain = 64;
	if (gc8613->sensor_id) {
		//gc8613ya sensor
		if (gc8613->cur_mode->bpp == 12) {
			// dag
			total = sizeof(gain_level_table_dag_ya) / sizeof(u32) - 1;
			for (i = 0; i < total; i++) {
				if (gain_level_table_dag_ya[i] <= gain &&
				    gain < gain_level_table_dag_ya[i + 1])
					break;
			}
			tol_dig_gain = gain * 64 / gain_level_table_dag_ya[i];
			if (i >= total)
				i = total - 1;
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x2d);
			gc8613_write_reg(gc8613->client, 0x0614,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][0]);
			gc8613_write_reg(gc8613->client, 0x0615,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][1]);
			gc8613_write_reg(gc8613->client, 0x026e,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][5]);
			gc8613_write_reg(gc8613->client, 0x0270,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][6]);
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x28);
			gc8613_write_reg(gc8613->client, 0x0225,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][2]);
			gc8613_write_reg(gc8613->client, 0x1467,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][3]);
			gc8613_write_reg(gc8613->client, 0x1468,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][4]);
			gc8613_write_reg(gc8613->client, 0x00b8,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][7]);
			gc8613_write_reg(gc8613->client, 0x00b9,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][8]);
			gc8613_write_reg(gc8613->client, 0x1447,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_ya[i][9]);
			gc8613_write_reg(gc8613->client, 0x0064,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain >> 6));
			gc8613_write_reg(gc8613->client, 0x0065,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain & 0x3f));
		} else if (gc8613->cur_mode->hdr_mode == HDR_X2) {
			// stagger hdr
			total = sizeof(gain_level_table_hdr_ya) / sizeof(u32) - 1;
			for (i = 0; i < total; i++) {
				if (gain_level_table_hdr_ya[i] <= gain &&
				    gain < gain_level_table_hdr_ya[i + 1])
					break;
			}
			tol_dig_gain = gain * 64 / gain_level_table_hdr_ya[i];
			if (i >= total)
				i = total - 1;
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x2d);
			gc8613_write_reg(gc8613->client, 0x0614,
					 GC8613_REG_VALUE_08BIT, reg_val_table_hdr_ya[i][0]);
			gc8613_write_reg(gc8613->client, 0x0615,
					 GC8613_REG_VALUE_08BIT, reg_val_table_hdr_ya[i][1]);
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x28);
			gc8613_write_reg(gc8613->client, 0x0225,
					 GC8613_REG_VALUE_08BIT, reg_val_table_hdr_ya[i][2]);
			gc8613_write_reg(gc8613->client, 0x1467,
					 GC8613_REG_VALUE_08BIT, reg_val_table_hdr_ya[i][3]);
			gc8613_write_reg(gc8613->client, 0x1468,
					 GC8613_REG_VALUE_08BIT, reg_val_table_hdr_ya[i][4]);
			gc8613_write_reg(gc8613->client, 0x00b8,
					 GC8613_REG_VALUE_08BIT, reg_val_table_hdr_ya[i][5]);
			gc8613_write_reg(gc8613->client, 0x00b9,
					 GC8613_REG_VALUE_08BIT, reg_val_table_hdr_ya[i][6]);
			gc8613_write_reg(gc8613->client, 0x1447,
					 GC8613_REG_VALUE_08BIT, reg_val_table_hdr_ya[i][7]);

			gc8613_write_reg(gc8613->client, 0x0064,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain >> 6));
			gc8613_write_reg(gc8613->client, 0x0065,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain & 0x3f));
			gc8613_write_reg(gc8613->client, 0x007b,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain >> 6));
			gc8613_write_reg(gc8613->client, 0x007c,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain & 0x3f));
		} else {
			// linear
			total = sizeof(gain_level_table_linear_ya) / sizeof(u32) - 1;
			for (i = 0; i < total; i++) {
				if (gain_level_table_linear_ya[i] <= gain &&
				    gain < gain_level_table_linear_ya[i + 1])
					break;
			}
			tol_dig_gain = gain * 64 / gain_level_table_linear_ya[i];
			if (i >= total)
				i = total - 1;
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x2d);
			gc8613_write_reg(gc8613->client, 0x0614,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_ya[i][0]);
			gc8613_write_reg(gc8613->client, 0x0615,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_ya[i][1]);
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x28);
			gc8613_write_reg(gc8613->client, 0x0225,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_ya[i][2]);
			gc8613_write_reg(gc8613->client, 0x1467,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_ya[i][3]);
			gc8613_write_reg(gc8613->client, 0x1468,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_ya[i][4]);
			gc8613_write_reg(gc8613->client, 0x00b8,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_ya[i][5]);
			gc8613_write_reg(gc8613->client, 0x00b9,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_ya[i][6]);
			gc8613_write_reg(gc8613->client, 0x1447,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_ya[i][7]);
			gc8613_write_reg(gc8613->client, 0x0064,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain >> 6));
			gc8613_write_reg(gc8613->client, 0x0065,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain & 0x3f));
		}
	} else {
		//gc8613yn sensor
		if (gc8613->cur_mode->bpp == 12) {
			// dag
			total = sizeof(gain_level_table_dag_yn) / sizeof(u32) - 1;
			for (i = 0; i < total; i++) {
				if (gain_level_table_dag_yn[i] <= gain &&
				    gain < gain_level_table_dag_yn[i + 1])
					break;
			}
			tol_dig_gain = gain * 64 / gain_level_table_dag_yn[i];
			if (i >= total)
				i = total - 1;
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x2d);
			gc8613_write_reg(gc8613->client, 0x0614,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][0]);
			gc8613_write_reg(gc8613->client, 0x0615,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][1]);
			gc8613_write_reg(gc8613->client, 0x026e,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][5]);
			gc8613_write_reg(gc8613->client, 0x0270,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][6]);
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x28);
			gc8613_write_reg(gc8613->client, 0x0225,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][2]);
			gc8613_write_reg(gc8613->client, 0x1467,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][3]);
			gc8613_write_reg(gc8613->client, 0x1468,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][4]);
			gc8613_write_reg(gc8613->client, 0x00b8,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][7]);
			gc8613_write_reg(gc8613->client, 0x00b9,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][8]);
			gc8613_write_reg(gc8613->client, 0x1447,
					 GC8613_REG_VALUE_08BIT, reg_val_table_dag_yn[i][9]);
			gc8613_write_reg(gc8613->client, 0x0064,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain >> 6));
			gc8613_write_reg(gc8613->client, 0x0065,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain & 0x3f));
		} else {
			// linear
			total = sizeof(gain_level_table_linear_yn) / sizeof(u32) - 1;
			for (i = 0; i < total; i++) {
				if (gain_level_table_linear_yn[i] <= gain &&
				    gain < gain_level_table_linear_yn[i + 1])
					break;
			}
			tol_dig_gain = gain * 64 / gain_level_table_linear_yn[i];
			if (i >= total)
				i = total - 1;
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x2d);
			gc8613_write_reg(gc8613->client, 0x0614,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_yn[i][0]);
			gc8613_write_reg(gc8613->client, 0x0615,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_yn[i][1]);
			gc8613_write_reg(gc8613->client, 0x031d,
					 GC8613_REG_VALUE_08BIT, 0x28);
			gc8613_write_reg(gc8613->client, 0x0225,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_yn[i][2]);
			gc8613_write_reg(gc8613->client, 0x1467,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_yn[i][3]);
			gc8613_write_reg(gc8613->client, 0x1468,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_yn[i][4]);
			gc8613_write_reg(gc8613->client, 0x00b8,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_yn[i][5]);
			gc8613_write_reg(gc8613->client, 0x00b9,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_yn[i][6]);
			gc8613_write_reg(gc8613->client, 0x1447,
					 GC8613_REG_VALUE_08BIT, reg_val_table_linear_yn[i][7]);
			gc8613_write_reg(gc8613->client, 0x0064,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain >> 6));
			gc8613_write_reg(gc8613->client, 0x0065,
					 GC8613_REG_VALUE_08BIT, (tol_dig_gain & 0x3f));
		}
	}

	return 0;
}

/* window_heigth register is 0x034a/0x034b, vts register is 0x0340/0x0341
 * short_intt < vb, short_intt + long_intt < vts
 * vb = vts - window_height - 16 - 20
 */
static int gc8613_set_hdrae(struct gc8613 *gc8613,
			    struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;
	u32 intt_long_l, intt_long_h;
	u32 intt_short_l, intt_short_h;
	u32 gain;
	u32 vblank = gc8613->cur_vts - gc8613->cur_mode->height - 16 - 20;

	if (!gc8613->has_init_exp && !gc8613->streaming) {
		gc8613->init_hdrae_exp = *ae;
		gc8613->has_init_exp = true;
		dev_dbg(&gc8613->client->dev, "gc8613 don't stream, record exp for hdr!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&gc8613->client->dev,
		"rev exp req: L_exp: 0x%x, M_exp: 0x%x, S_exp 0x%x,l_gain:0x%x, m_gain: 0x%x, s_gain: 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (gc8613->cur_mode->hdr_mode == HDR_X2)
		l_exp_time = m_exp_time;

	gain = s_a_gain;

	if (l_exp_time <= 1)
		l_exp_time = 1;

	if (s_exp_time < 1)
		s_exp_time = 1;

	if (s_exp_time > vblank) {
		dev_err(&gc8613->client->dev, "the s_exp_time is too large. s_exp_time %d\n", s_exp_time);
		s_exp_time = vblank;
	}

	if (l_exp_time > gc8613->cur_vts - s_exp_time) {
		dev_err(&gc8613->client->dev, "the l_exp_time is too large. l_exp_time %d\n", l_exp_time);
		l_exp_time = gc8613->cur_vts - s_exp_time;
	}

	intt_long_l = l_exp_time & 0xff;
	intt_long_h = (l_exp_time >> 8) & 0x3f;

	intt_short_l = s_exp_time & 0xff;
	intt_short_h = (s_exp_time >> 8) & 0x3f;

	ret |= gc8613_write_reg(gc8613->client, GC8613_REG_EXPOSURE_H,
				GC8613_REG_VALUE_08BIT,
				intt_long_h);
	ret |= gc8613_write_reg(gc8613->client, GC8613_REG_EXPOSURE_L,
				GC8613_REG_VALUE_08BIT,
				intt_long_l);
	ret |= gc8613_write_reg(gc8613->client, GC8613_REG_SEXPOSURE_H,
				GC8613_REG_VALUE_08BIT,
				intt_short_h);
	ret |= gc8613_write_reg(gc8613->client, GC8613_REG_SEXPOSURE_L,
				GC8613_REG_VALUE_08BIT,
				intt_short_l);

	ret |= gc8613_set_gain_reg(gc8613, gain);
	return ret;
}

static int gc8613_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc8613 *gc8613 = to_gc8613(sd);
	const struct gc8613_mode *mode = gc8613->cur_mode;

	if (gc8613->streaming)
		fi->interval = gc8613->cur_fps;
	else
		fi->interval = mode->max_fps;

	return 0;
}

static const struct gc8613_mode *gc8613_find_mode(struct gc8613 *gc8613, int fps)
{
	const struct gc8613_mode *mode = NULL;
	const struct gc8613_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < gc8613->cfg_num; i++) {
		mode = &supported_modes[i];
		if (mode->width == gc8613->cur_mode->width &&
		    mode->height == gc8613->cur_mode->height &&
		    mode->hdr_mode == gc8613->cur_mode->hdr_mode &&
		    mode->bus_fmt == gc8613->cur_mode->bus_fmt) {
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

static int gc8613_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc8613 *gc8613 = to_gc8613(sd);
	const struct gc8613_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	int fps;

	if (gc8613->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = gc8613_find_mode(gc8613, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	gc8613->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(gc8613->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(gc8613->vblank, vblank_def,
				 GC8613_VTS_MAX - mode->height,
				 1, vblank_def);
	if (mode->hdr_mode == HDR_X2) {
		gc8613->cur_link_freq = 1;
		gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_HDR;
	} else {
		if (gc8613->cur_mode->bpp == 12) {
			gc8613->cur_link_freq = 1;
			gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_DAG;
		} else {
			gc8613->cur_link_freq = 0;
			gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_LINEAR;
		}
	}

	__v4l2_ctrl_s_ctrl_int64(gc8613->pixel_rate,
				 gc8613->cur_pixel_rate);
	__v4l2_ctrl_s_ctrl(gc8613->link_freq,
			   gc8613->cur_link_freq);
	gc8613->cur_fps = mode->max_fps;

	return 0;
}

static int gc8613_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = GC8613_LANES;

	return 0;
}

static void gc8613_get_module_inf(struct gc8613 *gc8613,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, GC8613_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, gc8613->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, gc8613->len_name, sizeof(inf->base.lens));
}

static int gc8613_get_channel_info(struct gc8613 *gc8613, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = gc8613->cur_mode->vc[ch_info->index];
	ch_info->width = gc8613->cur_mode->width;
	ch_info->height = gc8613->cur_mode->height;
	ch_info->bus_fmt = gc8613->cur_mode->bus_fmt;
	return 0;
}

static long gc8613_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc8613 *gc8613 = to_gc8613(sd);
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
		gc8613_get_module_inf(gc8613, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = gc8613->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		if (hdr->hdr_mode == gc8613->cur_mode->hdr_mode)
			return 0;
		w = gc8613->cur_mode->width;
		h = gc8613->cur_mode->height;
		dst_fps = DIV_ROUND_CLOSEST(gc8613->cur_mode->max_fps.denominator,
					    gc8613->cur_mode->max_fps.numerator);
		for (i = 0; i < gc8613->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode &&
			    supported_modes[i].bus_fmt == gc8613->cur_mode->bus_fmt) {
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
			dev_err(&gc8613->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			gc8613->cur_mode = &supported_modes[cur_best_fit];
			w = gc8613->cur_mode->hts_def -
			    gc8613->cur_mode->width;
			h = gc8613->cur_mode->vts_def -
			    gc8613->cur_mode->height;
			__v4l2_ctrl_modify_range(gc8613->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(gc8613->vblank, h,
						 GC8613_VTS_MAX -
						 gc8613->cur_mode->height,
						 1, h);
			if (gc8613->cur_mode->hdr_mode == HDR_X2) {
				gc8613->cur_link_freq = 1;
				gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_HDR;
			} else {
				if (gc8613->cur_mode->bpp == 12) {
					gc8613->cur_link_freq = 1;
					gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_DAG;
				} else {
					gc8613->cur_link_freq = 0;
					gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_LINEAR;
				}
			}

			__v4l2_ctrl_s_ctrl_int64(gc8613->pixel_rate,
						 gc8613->cur_pixel_rate);
			__v4l2_ctrl_s_ctrl(gc8613->link_freq,
					   gc8613->cur_link_freq);
			gc8613->cur_vts = gc8613->cur_mode->vts_def;
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		ret = gc8613_set_hdrae(gc8613, arg);
		if (gc8613->cam_sw_inf)
			memcpy(&gc8613->cam_sw_inf->hdr_ae, (struct preisp_hdrae_exp_s *)(arg),
			       sizeof(struct preisp_hdrae_exp_s));
		break;
	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);

		if (stream)
			ret = gc8613_write_reg(gc8613->client, GC8613_REG_CTRL_MODE,
					       GC8613_REG_VALUE_08BIT, GC8613_MODE_STREAMING);
		else
			ret = gc8613_write_reg(gc8613->client, GC8613_REG_CTRL_MODE,
					       GC8613_REG_VALUE_08BIT, GC8613_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = gc8613_get_channel_info(gc8613, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gc8613_compat_ioctl32(struct v4l2_subdev *sd,
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

		ret = gc8613_ioctl(sd, cmd, inf);
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
			ret = gc8613_ioctl(sd, cmd, cfg);
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

		ret = gc8613_ioctl(sd, cmd, hdr);
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
			ret = gc8613_ioctl(sd, cmd, hdr);
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
			ret = gc8613_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = gc8613_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc8613_ioctl(sd, cmd, ch_info);
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

static int __gc8613_start_stream(struct gc8613 *gc8613)
{
	int ret;

	if (!gc8613->is_thunderboot) {
		ret = gc8613_write_array(gc8613->client,
			gc8613->cur_mode->reg_list[gc8613->sensor_id]);
		if (ret)
			return ret;

		dev_info(&gc8613->client->dev, "write reg array done, start stream\n");

		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&gc8613->ctrl_handler);
		if (ret)
			return ret;
		if (gc8613->has_init_exp && gc8613->cur_mode->hdr_mode != NO_HDR) {
			ret = gc8613_ioctl(&gc8613->subdev, PREISP_CMD_SET_HDRAE_EXP,
					   &gc8613->init_hdrae_exp);
			if (ret) {
				dev_err(&gc8613->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	} else {
		dev_info(&gc8613->client->dev, "thunderboot mode, just streaming\n");
	}
	dev_info(&gc8613->client->dev, "__v4l2_ctrl_handler_setup done, ready to start stream\n");

	ret = gc8613_write_reg(gc8613->client, GC8613_REG_CTRL_MODE,
			       GC8613_REG_VALUE_08BIT, GC8613_MODE_STREAMING);
	//if (gc8613->cur_mode->hdr_mode == NO_HDR)
	//	ret |= gc8613_write_array(gc8613->client, gc8613_linear_global_regs);
	dev_info(&gc8613->client->dev, "write stream done, streaming ......, ret: %d\n", ret);

	return ret;
}

static int __gc8613_stop_stream(struct gc8613 *gc8613)
{
	gc8613->has_init_exp = false;
	return gc8613_write_reg(gc8613->client, GC8613_REG_CTRL_MODE,
				GC8613_REG_VALUE_08BIT, GC8613_MODE_SW_STANDBY);
}

static int __gc8613_power_on(struct gc8613 *gc8613);
static int gc8613_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc8613 *gc8613 = to_gc8613(sd);
	struct i2c_client *client = gc8613->client;
	int ret = 0;

	mutex_lock(&gc8613->mutex);
	on = !!on;
	if (on == gc8613->streaming)
		goto unlock_and_return;

	if (on) {
		if (gc8613->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			gc8613->is_thunderboot = false;
			__gc8613_power_on(gc8613);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gc8613_start_stream(gc8613);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gc8613_stop_stream(gc8613);
		pm_runtime_put(&client->dev);
	}

	gc8613->streaming = on;

unlock_and_return:
	mutex_unlock(&gc8613->mutex);

	return ret;
}

static int gc8613_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc8613 *gc8613 = to_gc8613(sd);
	struct i2c_client *client = gc8613->client;
	int ret = 0;

	mutex_lock(&gc8613->mutex);

	/* If the power state is not modified - no work to do. */
	if (gc8613->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!gc8613->is_thunderboot) {
			ret = gc8613_write_array(gc8613->client, gc8613_global_regs);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		gc8613->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		gc8613->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc8613->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 gc8613_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, GC8613_XVCLK_FREQ_24M / 1000 / 1000);
}

static int __gc8613_power_on(struct gc8613 *gc8613)
{
	int ret;
	u32 delay_us;
	struct device *dev = &gc8613->client->dev;

	if (!IS_ERR_OR_NULL(gc8613->pins_default)) {
		ret = pinctrl_select_state(gc8613->pinctrl,
					   gc8613->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(gc8613->xvclk, GC8613_XVCLK_FREQ_24M);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(gc8613->xvclk) != GC8613_XVCLK_FREQ_24M)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(gc8613->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	cam_sw_regulator_bulk_init(gc8613->cam_sw_inf,
				   GC8613_NUM_SUPPLIES, gc8613->supplies);

	if (gc8613->is_thunderboot)
		return 0;

	if (!IS_ERR(gc8613->reset_gpio))
		gpiod_set_value_cansleep(gc8613->reset_gpio, 0);

	if (!IS_ERR(gc8613->pwdn_gpio))
		gpiod_set_value_cansleep(gc8613->pwdn_gpio, 0);

	usleep_range(500, 1000);
	ret = regulator_bulk_enable(GC8613_NUM_SUPPLIES, gc8613->supplies);

	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(gc8613->pwren_gpio))
		gpiod_set_value_cansleep(gc8613->pwren_gpio, 1);

	usleep_range(1000, 1100);
	if (!IS_ERR(gc8613->pwdn_gpio))
		gpiod_set_value_cansleep(gc8613->pwdn_gpio, 1);
	usleep_range(100, 150);
	if (!IS_ERR(gc8613->reset_gpio))
		gpiod_set_value_cansleep(gc8613->reset_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = gc8613_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

err_clk:
	if (!IS_ERR(gc8613->reset_gpio))
		gpiod_direction_output(gc8613->reset_gpio, 0);
disable_clk:
	clk_disable_unprepare(gc8613->xvclk);

	return ret;
}

static void __gc8613_power_off(struct gc8613 *gc8613)
{
	int ret;
	struct device *dev = &gc8613->client->dev;

	clk_disable_unprepare(gc8613->xvclk);
	if (gc8613->is_thunderboot) {
		if (gc8613->is_first_streamoff) {
			gc8613->is_thunderboot = false;
			gc8613->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(gc8613->pwdn_gpio))
		gpiod_set_value_cansleep(gc8613->pwdn_gpio, 0);
	if (!IS_ERR(gc8613->reset_gpio))
		gpiod_set_value_cansleep(gc8613->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(gc8613->pins_sleep)) {
		ret = pinctrl_select_state(gc8613->pinctrl,
					   gc8613->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(GC8613_NUM_SUPPLIES, gc8613->supplies);
	if (!IS_ERR(gc8613->pwren_gpio))
		gpiod_set_value_cansleep(gc8613->pwren_gpio, 0);
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int gc8613_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8613 *gc8613 = to_gc8613(sd);

	cam_sw_prepare_wakeup(gc8613->cam_sw_inf, dev);

	usleep_range(4000, 5000);
	cam_sw_write_array(gc8613->cam_sw_inf);

	if (__v4l2_ctrl_handler_setup(&gc8613->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (gc8613->has_init_exp && gc8613->cur_mode != NO_HDR) {	// hdr mode
		ret = gc8613_ioctl(&gc8613->subdev, PREISP_CMD_SET_HDRAE_EXP,
				   &gc8613->cam_sw_inf->hdr_ae);
		if (ret) {
			dev_err(&gc8613->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}
	return 0;
}

static int gc8613_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8613 *gc8613 = to_gc8613(sd);

	cam_sw_write_array_cb_init(gc8613->cam_sw_inf, client,
				   (void *)gc8613->cur_mode->reg_list[gc8613->sensor_id],
				   (sensor_write_array)gc8613_write_array);
	cam_sw_prepare_sleep(gc8613->cam_sw_inf);

	return 0;
}
#else
#define gc8613_resume NULL
#define gc8613_suspend NULL
#endif

static int gc8613_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8613 *gc8613 = to_gc8613(sd);

	return __gc8613_power_on(gc8613);
}

static int gc8613_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8613 *gc8613 = to_gc8613(sd);

	__gc8613_power_off(gc8613);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc8613_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc8613 *gc8613 = to_gc8613(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gc8613_mode *def_mode = &supported_modes[0];

	mutex_lock(&gc8613->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&gc8613->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int gc8613_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum *fie)
{
	struct gc8613 *gc8613 = to_gc8613(sd);

	if (fie->index >= gc8613->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops gc8613_pm_ops = {
	SET_RUNTIME_PM_OPS(gc8613_runtime_suspend,
	gc8613_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(gc8613_suspend, gc8613_resume)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gc8613_internal_ops = {
	.open = gc8613_open,
};
#endif

static const struct v4l2_subdev_core_ops gc8613_core_ops = {
	.s_power = gc8613_s_power,
	.ioctl = gc8613_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc8613_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gc8613_video_ops = {
	.s_stream = gc8613_s_stream,
	.g_frame_interval = gc8613_g_frame_interval,
	.s_frame_interval = gc8613_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc8613_pad_ops = {
	.enum_mbus_code = gc8613_enum_mbus_code,
	.enum_frame_size = gc8613_enum_frame_sizes,
	.enum_frame_interval = gc8613_enum_frame_interval,
	.get_fmt = gc8613_get_fmt,
	.set_fmt = gc8613_set_fmt,
	.get_mbus_config = gc8613_g_mbus_config,
};

static const struct v4l2_subdev_ops gc8613_subdev_ops = {
	.core	= &gc8613_core_ops,
	.video	= &gc8613_video_ops,
	.pad	= &gc8613_pad_ops,
};

static void gc8613_modify_fps_info(struct gc8613 *gc8613)
{
	const struct gc8613_mode *mode = gc8613->cur_mode;

	gc8613->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				      gc8613->cur_vts;
}

static int gc8613_set_mirror_flip(struct gc8613 *gc8613, u8 otp_val)
{
	int ret = 0;

	//otp autoload normal
	ret |= gc8613_write_reg(gc8613->client, 0x0a67,
				GC8613_REG_VALUE_08BIT, 0x80);
	ret |= gc8613_write_reg(gc8613->client, 0x0a98,
				GC8613_REG_VALUE_08BIT, 0x04);
	ret |= gc8613_write_reg(gc8613->client, 0x05be,
				GC8613_REG_VALUE_08BIT, 0x00);
	ret |= gc8613_write_reg(gc8613->client, 0x05a9,
				GC8613_REG_VALUE_08BIT, 0x01);
	ret |= gc8613_write_reg(gc8613->client, 0x0a70,
				GC8613_REG_VALUE_08BIT, 0x03);
	ret |= gc8613_write_reg(gc8613->client, 0x0a73,
				GC8613_REG_VALUE_08BIT, otp_val);
	ret |= gc8613_write_reg(gc8613->client, 0x0a5a,
				GC8613_REG_VALUE_08BIT, 0x80);

	usleep_range(20 * 1000, 25 * 1000);

	ret |= gc8613_write_reg(gc8613->client, 0x05be,
				GC8613_REG_VALUE_08BIT, 0x01);
	ret |= gc8613_write_reg(gc8613->client, 0x0a70,
				GC8613_REG_VALUE_08BIT, 0x00);
	ret |= gc8613_write_reg(gc8613->client, 0x0080,
				GC8613_REG_VALUE_08BIT, 0x02);
	ret |= gc8613_write_reg(gc8613->client, 0x0a67,
				GC8613_REG_VALUE_08BIT, 0x00);
	return ret;
}

static int gc8613_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc8613 *gc8613 = container_of(ctrl->handler,
					     struct gc8613, ctrl_handler);
	struct i2c_client *client = gc8613->client;
	s64 max;
	int mirror = 0, flip = 0;
	int otp_val = 0, ops_val = 0;
	int ret = 0;

	/*Propagate change of current control to all related controls*/
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/*Update max exposure while meeting expected vblanking*/
		max = gc8613->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(gc8613->exposure,
					 gc8613->exposure->minimum,
					 max,
					 gc8613->exposure->step,
					 gc8613->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (gc8613->cur_mode->hdr_mode != NO_HDR)
			break;
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		/* 4 least significant bits of expsoure are fractional part */
		ret = gc8613_write_reg(gc8613->client, GC8613_REG_EXPOSURE_H,
					GC8613_REG_VALUE_08BIT,
					ctrl->val >> 8);
		ret |= gc8613_write_reg(gc8613->client, GC8613_REG_EXPOSURE_L,
					GC8613_REG_VALUE_08BIT,
					ctrl->val & 0xff);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (gc8613->cur_mode->hdr_mode != NO_HDR)
			break;
		dev_dbg(&client->dev, "set gain 0x%x\n", ctrl->val);
		ret = gc8613_set_gain_reg(gc8613, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		gc8613->cur_vts = ctrl->val + gc8613->cur_mode->height;
		ret = gc8613_write_reg(gc8613->client, GC8613_REG_VTS_H,
				       GC8613_REG_VALUE_08BIT,
				       gc8613->cur_vts >> 8);
		ret |= gc8613_write_reg(gc8613->client, GC8613_REG_VTS_L,
					GC8613_REG_VALUE_08BIT,
					gc8613->cur_vts & 0xff);
		if (gc8613->cur_vts != gc8613->cur_mode->vts_def)
			gc8613_modify_fps_info(gc8613);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = gc8613_enable_test_pattern(gc8613, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		otp_val = 0x60;
		ret = gc8613_read_reg(gc8613->client, GC8613_FLIP_MIRROR_REG,
				      GC8613_REG_VALUE_08BIT, &mirror);
		ret |= gc8613_read_reg(gc8613->client, GC8613_OTP_MIRROR_FLIP_REG,
				       GC8613_REG_VALUE_08BIT, &ops_val);
		if (ctrl->val) {
			mirror |= GC8613_MIRROR_BIT_MASK;
			ops_val |= GC8613_MIRROR_BIT_MASK;	//0x61
		} else {
			mirror &= ~GC8613_MIRROR_BIT_MASK;
			ops_val &= ~GC8613_MIRROR_BIT_MASK;	//0x60
		}

		otp_val |= ops_val;	//60:normal / 61:mirror

		ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIRROR_REG,
					GC8613_REG_VALUE_08BIT, 0x00);
		if (mirror & 0x1) {	//mirror
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIR_MOD_REG,
						GC8613_REG_VALUE_08BIT, 0x05);
		} else {	//normal
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIR_MOD_REG,
						GC8613_REG_VALUE_08BIT, 0x00);
		}

		ret |= gc8613_set_mirror_flip(gc8613, otp_val);
		break;
	case V4L2_CID_VFLIP:
		otp_val = 0x62;
		ret = gc8613_read_reg(gc8613->client, GC8613_FLIP_MIRROR_REG,
				      GC8613_REG_VALUE_08BIT, &flip);
		ret |= gc8613_read_reg(gc8613->client, GC8613_OTP_MIRROR_FLIP_REG,
				       GC8613_REG_VALUE_08BIT, &ops_val);

		if (ctrl->val) {
			flip |= GC8613_FLIP_BIT_MASK;		//2 or 3
			ops_val |= GC8613_FLIP_BIT_MASK;	//0x62 or 0x63
		} else {
			flip &= ~GC8613_FLIP_BIT_MASK;		//0 or 1
			ops_val &= ~GC8613_FLIP_BIT_MASK;	//0x60 0r 0x61
		}

		otp_val |= ops_val;	//62:flip / 63:mirror & flip
		if (flip & 0x3) {	//mirror & flip
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIRROR_REG,
						GC8613_REG_VALUE_08BIT, 0x01);
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIR_MOD_REG,
						GC8613_REG_VALUE_08BIT, 0x07);
		} else if (flip & 0x2) {	//flip
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIRROR_REG,
						GC8613_REG_VALUE_08BIT, 0x01);
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIR_MOD_REG,
						GC8613_REG_VALUE_08BIT, 0x02);
		} else if (flip & 0x1) {	//mirror
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIRROR_REG,
						GC8613_REG_VALUE_08BIT, 0x00);
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIR_MOD_REG,
						GC8613_REG_VALUE_08BIT, 0x05);
		} else {
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIRROR_REG,
						GC8613_REG_VALUE_08BIT, 0x00);
			ret |= gc8613_write_reg(gc8613->client, GC8613_FLIP_MIR_MOD_REG,
						GC8613_REG_VALUE_08BIT, 0x00);
		}

		ret |= gc8613_set_mirror_flip(gc8613, otp_val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gc8613_ctrl_ops = {
	.s_ctrl = gc8613_set_ctrl,
};

static int gc8613_initialize_controls(struct gc8613 *gc8613)
{
	const struct gc8613_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &gc8613->ctrl_handler;
	mode = gc8613->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &gc8613->mutex;

	gc8613->link_freq = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
			    1, 0, link_freq_menu_items);
	if (mode->hdr_mode == HDR_X2) {
		gc8613->cur_link_freq = 1;
		gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_HDR;
	} else {
		if (gc8613->cur_mode->bpp == 12) {
			gc8613->cur_link_freq = 1;
			gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_DAG;
		} else {
			gc8613->cur_link_freq = 0;
			gc8613->cur_pixel_rate = GC8613_PIXEL_RATE_LINEAR;
		}
	}

	__v4l2_ctrl_s_ctrl(gc8613->link_freq,
			   gc8613->cur_link_freq);

	gc8613->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
					       0, gc8613->cur_pixel_rate,
						   1, gc8613->cur_pixel_rate);

	h_blank = mode->hts_def - mode->width;
	gc8613->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (gc8613->hblank)
		gc8613->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	gc8613->cur_vts = mode->vts_def;
	gc8613->vblank = v4l2_ctrl_new_std(handler, &gc8613_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   GC8613_VTS_MAX - mode->height,
					   1, vblank_def);

	exposure_max = mode->vts_def - 8;
	gc8613->exposure = v4l2_ctrl_new_std(handler, &gc8613_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     GC8613_EXPOSURE_MIN,
					     exposure_max,
					     GC8613_EXPOSURE_STEP,
					     mode->exp_def);

	gc8613->anal_gain = v4l2_ctrl_new_std(handler, &gc8613_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN,
					      GC8613_GAIN_MIN,
					      GC8613_GAIN_MAX,
					      GC8613_GAIN_STEP,
					      GC8613_GAIN_DEFAULT);

	gc8613->test_pattern =
		v4l2_ctrl_new_std_menu_items(handler,
					     &gc8613_ctrl_ops,
					     V4L2_CID_TEST_PATTERN,
					     ARRAY_SIZE(gc8613_test_pattern_menu) - 1,
					     0, 0, gc8613_test_pattern_menu);

	gc8613->h_flip = v4l2_ctrl_new_std(handler, &gc8613_ctrl_ops,
					   V4L2_CID_HFLIP, 0, 1, 1, 0);

	gc8613->v_flip = v4l2_ctrl_new_std(handler, &gc8613_ctrl_ops,
					   V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&gc8613->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc8613->subdev.ctrl_handler = handler;
	gc8613->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gc8613_check_sensor_id(struct gc8613 *gc8613,
				  struct i2c_client *client)
{
	struct device *dev = &gc8613->client->dev;
	u16 id = 0;
	u32 reg_H = 0;
	u32 reg_M = 0;
	u32 reg_L = 0;
	int ret;

	if (gc8613->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = gc8613_read_reg(client, GC8613_REG_CHIP_ID_H,
			      GC8613_REG_VALUE_08BIT, &reg_H);
	ret |= gc8613_read_reg(client, GC8613_REG_CHIP_ID_M,
			       GC8613_REG_VALUE_08BIT, &reg_M);
	ret |= gc8613_read_reg(client, GC8613_REG_CHIP_ID_L,
			       GC8613_REG_VALUE_08BIT, &reg_L);

	id = ((reg_H << 8) & 0xff00) | (reg_M & 0xff);
	if (!(reg_H == (CHIP_ID >> 8) || reg_L == (CHIP_ID & 0xff))) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	if (reg_L == 0x23)
		gc8613->sensor_id = true;
	else
		gc8613->sensor_id = false;

	dev_info(dev, "Detected gc8613 (0x%04x), 0x%02x sensor\n", id, reg_L);
	return 0;
}

static int gc8613_configure_regulators(struct gc8613 *gc8613)
{
	unsigned int i;

	for (i = 0; i < GC8613_NUM_SUPPLIES; i++)
		gc8613->supplies[i].supply = gc8613_supply_names[i];

	return devm_regulator_bulk_get(&gc8613->client->dev,
				       GC8613_NUM_SUPPLIES,
				       gc8613->supplies);
}

static int gc8613_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gc8613 *gc8613;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	gc8613 = devm_kzalloc(dev, sizeof(*gc8613), GFP_KERNEL);
	if (!gc8613)
		return -ENOMEM;

	of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &gc8613->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &gc8613->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &gc8613->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &gc8613->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	gc8613->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	gc8613->client = client;
	gc8613->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < gc8613->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			gc8613->cur_mode = &supported_modes[i];
			break;
		}
	}
	if (i == gc8613->cfg_num)
		gc8613->cur_mode = &supported_modes[0];

	gc8613->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(gc8613->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	gc8613->pwren_gpio = devm_gpiod_get(dev, "pwren",
					    gc8613->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(gc8613->pwren_gpio))
		dev_warn(dev, "Failed to get pwren-gpios\n");

	gc8613->reset_gpio = devm_gpiod_get(dev, "reset",
					    gc8613->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(gc8613->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	gc8613->pwdn_gpio = devm_gpiod_get(dev, "pwdn",
					   gc8613->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(gc8613->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	gc8613->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(gc8613->pinctrl)) {
		gc8613->pins_default =
			pinctrl_lookup_state(gc8613->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(gc8613->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		gc8613->pins_sleep =
			pinctrl_lookup_state(gc8613->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(gc8613->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = gc8613_configure_regulators(gc8613);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&gc8613->mutex);

	sd = &gc8613->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc8613_subdev_ops);
	ret = gc8613_initialize_controls(gc8613);
	if (ret)
		goto err_destroy_mutex;

	ret = __gc8613_power_on(gc8613);
	if (ret)
		goto err_free_handler;

	usleep_range(3000, 4000);

	ret = gc8613_check_sensor_id(gc8613, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc8613_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gc8613->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc8613->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	if (!gc8613->cam_sw_inf) {
		gc8613->cam_sw_inf = cam_sw_init();
		cam_sw_clk_init(gc8613->cam_sw_inf, gc8613->xvclk, GC8613_XVCLK_FREQ_24M);
		cam_sw_reset_pin_init(gc8613->cam_sw_inf, gc8613->reset_gpio, 0);
		cam_sw_pwdn_pin_init(gc8613->cam_sw_inf, gc8613->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc8613->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc8613->module_index, facing,
		 GC8613_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (gc8613->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__gc8613_power_off(gc8613);
err_free_handler:
	v4l2_ctrl_handler_free(&gc8613->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc8613->mutex);

	return ret;
}

static void gc8613_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc8613 *gc8613 = to_gc8613(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gc8613->ctrl_handler);
	mutex_destroy(&gc8613->mutex);

	cam_sw_deinit(gc8613->cam_sw_inf);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc8613_power_off(gc8613);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc8613_of_match[] = {
	{ .compatible = "galaxycore,gc8613" },
	{},
};
MODULE_DEVICE_TABLE(of, gc8613_of_match);
#endif

static const struct i2c_device_id gc8613_match_id[] = {
	{ "galaxycore,gc8613", 0 },
	{ },
};

static struct i2c_driver gc8613_i2c_driver = {
	.driver = {
		.name = GC8613_NAME,
		.pm = &gc8613_pm_ops,
		.of_match_table = of_match_ptr(gc8613_of_match),
	},
	.probe		= &gc8613_probe,
	.remove		= &gc8613_remove,
	.id_table	= gc8613_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc8613_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc8613_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("galaxycore gc8613 sensor driver");
MODULE_LICENSE("GPL");
