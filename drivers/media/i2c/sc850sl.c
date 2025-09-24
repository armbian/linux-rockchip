// SPDX-License-Identifier: GPL-2.0
/*
 * sc850sl driver
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 first version
 * V0.0X01.0X02 change power_gpio to pwdn_gpio
 * V0.0X01.0X03 support thunder boot
 * V0.0X01.0X04 support sleep/wake up
 * V0.0X01.0X05 unified standby hw mode
 *
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
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/rk-preisp.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"
#include "cam-sleep-wakeup.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x05)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define MIPI_FREQ_540M			540000000
#define MIPI_FREQ_972M			486000000
#define SC850AI_MAX_LINK_FREQ   MIPI_FREQ_540M

#define SC850SL_MAX_PIXEL_RATE	(SC850AI_MAX_LINK_FREQ / 10 * 2 * SC850SL_4LANES)
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

#define SC850SL_XVCLK_FREQ_24M		24000000

/* TODO: Get the real chip id from reg */
#define CHIP_ID				0x9D1E
#define SC850SL_REG_CHIP_ID		0x3107

#define SC850SL_REG_CTRL_MODE		0x0100
#define SC850SL_MODE_SW_STANDBY		0x0
#define SC850SL_MODE_STREAMING		BIT(0)

/*expo*/
#define	SC850SL_EXPOSURE_MIN		2    /*okay*/
#define	SC850SL_EXPOSURE_STEP		1    /*okay*/
#define SC850SL_VTS_MAX			0xffff   /*okay*/

//long exposure
#define SC850SL_REG_EXP_LONG_H		0x3e00    //[3:0]
#define SC850SL_REG_EXP_LONG_M		0x3e01    //[7:0]
#define SC850SL_REG_EXP_LONG_L		0x3e02    //[7:4]

//short exposure  //for hdr
#define SC850SL_REG_EXP_SF_H		0x3e22
#define SC850SL_REG_EXP_SF_M		0x3e04    //[7:0]
#define SC850SL_REG_EXP_SF_L		0x3e05    //[7:4]

#define SC850SL_FETCH_EXP_H(VAL)	(((VAL) >> 12) & 0xF)
#define SC850SL_FETCH_EXP_M(VAL)	(((VAL) >> 4) & 0xFF)
#define SC850SL_FETCH_EXP_L(VAL)	(((VAL) & 0xF) << 4)

/*gain*/
//long frame and normal gain reg
#define SC850SL_REG_DGAIN		0x3e06
#define SC850SL_REG_AGAIN		0x3e08
#define SC850SL_REG_AGAIN_FINE		0x3e09
#define SC850SL_REG_DGAIN_FINE		0x3e07

//short fram gain reg
#define SC850SL_SF_REG_AGAIN		0x3e12
#define SC850SL_SF_REG_AGAIN_FINE	0x3e13
#define SC850SL_SF_REG_DGAIN		0x3e10

#define SC850SL_GAIN_MIN		0x40	//1.000 = 64 * 1/64
#define SC850SL_GAIN_MAX		(8 * 50 * 64)   /*need_view   8*50*64=25600  */
#define SC850SL_GAIN_STEP		1
#define SC850SL_GAIN_DEFAULT		0x40

#define SC850SL_REG_VTS			0x320e

//group hold
#define SC850SL_GROUP_UPDATE_ADDRESS	0x3800
#define SC850SL_GROUP_UPDATE_START_DATA	0x00
#define SC850SL_GROUP_UPDATE_LAUNCH	0x30

#define SC850SL_SOFTWARE_RESET_REG	0x0103
#define SC850SL_REG_TEST_PATTERN	0x4501
#define SC850SL_TEST_PATTERN_ENABLE	0x08

#define SC850SL_FLIP_REG		0x3221
#define SC850SL_FLIP_MASK		0x60
#define SC850SL_MIRROR_MASK		0x06

#define REG_NULL			0xFFFF

#define SC850SL_REG_VALUE_08BIT		1
#define SC850SL_REG_VALUE_16BIT		2
#define SC850SL_REG_VALUE_24BIT		3

#define SC850SL_4LANES			4

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define SC850SL_NAME			"sc850sl"
#define ENABLE_NR			1
#define SC850SL_LGAIN			0
#define SC850SL_SGAIN			1

static const char *const sc850sl_supply_names[] = {
	"dvdd",		// Digital core power
	"dovdd",	// Digital I/O power
	"avdd",		// Analog power
};

#define SC850SL_NUM_SUPPLIES ARRAY_SIZE(sc850sl_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct sc850sl_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	u32 xvclk_freq;
	u32 bpp;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct sc850sl {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[SC850SL_NUM_SUPPLIES];

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_a_gain;
	struct v4l2_ctrl	*digi_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*test_pattern;
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct mutex		mutex;
	struct v4l2_fract	cur_fps;
	bool			streaming;
	bool			power_on;
	const struct sc850sl_mode *cur_mode;
	u32			module_index;
	u32			cfg_num;
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
	struct cam_sw_info	*cam_sw_info;
};

#define to_sc850sl(sd) container_of(sd, struct sc850sl, subdev)

//cleaned_0x20_SC850SL_MIPI_24Minput_1C4D_1080Mbps_10bit_3840x2160_30fps_one_expo.ini
static __maybe_unused const struct regval sc850sl_linear10bit_3840x2160_30fps_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x36ea, 0x09},
	{0x36eb, 0x0c},
	{0x36ec, 0x4a},
	{0x36ed, 0x24},
	{0x36fa, 0x0b},
	{0x36fb, 0x33},
	{0x36fc, 0x10},
	{0x36fd, 0x37},
	{0x36e9, 0x24},
	{0x36f9, 0x53},
	{0x3018, 0x7a},
	{0x3019, 0xf0},
	{0x301a, 0x30},
	{0x301e, 0x3c},
	{0x301f, 0x20},
	{0x302a, 0x00},
	{0x3031, 0x0a},
	{0x3032, 0x20},
	{0x3033, 0x22},
	{0x3037, 0x00},
	{0x303e, 0xb4},
	{0x320c, 0x04},
	{0x320d, 0x4c},
	{0x3226, 0x00},
	{0x3227, 0x03},
	{0x3250, 0x40},
	{0x3253, 0x08},
	{0x327e, 0x00},
	{0x3280, 0x00},
	{0x3281, 0x00},
	{0x3301, 0x3c},
	{0x3304, 0x30},
	{0x3306, 0xe8},
	{0x3308, 0x10},
	{0x3309, 0x70},
	{0x330a, 0x01},
	{0x330b, 0xe0},
	{0x330d, 0x10},
	{0x3314, 0x92},
	{0x331e, 0x29},
	{0x331f, 0x69},
	{0x3333, 0x10},
	{0x3347, 0x05},
	{0x3348, 0xd0},
	{0x3352, 0x01},
	{0x3356, 0x38},
	{0x335d, 0x60},
	{0x3362, 0x70},
	{0x338f, 0x80},
	{0x33af, 0x48},
	{0x33fe, 0x00},
	{0x3400, 0x12},
	{0x3406, 0x04},
	{0x3410, 0x12},
	{0x3416, 0x06},
	{0x3433, 0x01},
	{0x3440, 0x12},
	{0x3446, 0x08},
	{0x3478, 0x01},
	{0x3479, 0x01},
	{0x347a, 0x02},
	{0x347b, 0x01},
	{0x347c, 0x04},
	{0x347d, 0x01},
	{0x3616, 0x0c},
	{0x3620, 0x92},
	{0x3622, 0x74},
	{0x3629, 0x74},
	{0x362a, 0xf0},
	{0x362b, 0x0f},
	{0x362d, 0x00},
	{0x3630, 0x68},
	{0x3633, 0x22},
	{0x3634, 0x22},
	{0x3635, 0x20},
	{0x3637, 0x06},
	{0x3638, 0x26},
	{0x363b, 0x06},
	{0x363c, 0x07},
	{0x363d, 0x05},
	{0x363e, 0x8f},
	{0x3648, 0xe0},
	{0x3649, 0x0a},
	{0x364a, 0x06},
	{0x364c, 0x6a},
	{0x3650, 0x3d},
	{0x3654, 0x40},
	{0x3656, 0x68},
	{0x3657, 0x0f},
	{0x3658, 0x3d},
	{0x365c, 0x40},
	{0x365e, 0x68},
	{0x3901, 0x04},
	{0x3904, 0x20},
	{0x3905, 0x91},
	{0x391e, 0x83},
	{0x3928, 0x04},
	{0x3933, 0x90},
	{0x3934, 0x10},
	{0x3935, 0x70},
	{0x3936, 0x00},
	{0x3937, 0x10},
	{0x3938, 0x14},
	{0x3946, 0x20},
	{0x3961, 0x40},
	{0x3962, 0x40},
	{0x3963, 0xc8},
	{0x3964, 0xc8},
	{0x3965, 0x40},
	{0x3966, 0x40},
	{0x3967, 0x00},
	{0x39cd, 0xc8},
	{0x39ce, 0xc8},
	{0x3e01, 0x82},
	{0x3e02, 0x00},
	{0x3e0e, 0x02},
	{0x3e0f, 0x00},
	{0x3e1c, 0x0f},
	{0x3e23, 0x00},
	{0x3e24, 0x00},
	{0x3e53, 0x00},
	{0x3e54, 0x00},
	{0x3e68, 0x00},
	{0x3e69, 0x80},
	{0x3e73, 0x00},
	{0x3e74, 0x00},
	{0x3e86, 0x03},
	{0x3e87, 0x40},
	{0x3f02, 0x24},
	{0x4424, 0x02},
	{0x4501, 0xc4},
	{0x4509, 0x20},
	{0x4561, 0x12},
	{0x4800, 0x24},
	{0x4837, 0x0f},
	{0x4900, 0x24},
	{0x4937, 0x0f},
	{0x5000, 0x0e},
	{0x500f, 0x35},
	{0x5020, 0x00},
	{0x5787, 0x10},
	{0x5788, 0x06},
	{0x5789, 0x00},
	{0x578a, 0x18},
	{0x578b, 0x0c},
	{0x578c, 0x00},
	{0x5790, 0x10},
	{0x5791, 0x06},
	{0x5792, 0x01},
	{0x5793, 0x18},
	{0x5794, 0x0c},
	{0x5795, 0x01},
	{0x5799, 0x06},
	{0x57a2, 0x60},
	{0x57ab, 0x7f},
	{0x57ac, 0x7f},
	{0x57ad, 0x7f},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x38},
	{0x59e3, 0x30},
	{0x59e4, 0x20},
	{0x59e5, 0x38},
	{0x59e6, 0x30},
	{0x59e7, 0x20},
	{0x59e8, 0x3f},
	{0x59e9, 0x38},
	{0x59ea, 0x30},
	{0x59eb, 0x3f},
	{0x59ec, 0x38},
	{0x59ed, 0x30},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x38},
	{0x59f5, 0x30},
	{0x59f6, 0x20},
	{0x59f7, 0x38},
	{0x59f8, 0x30},
	{0x59f9, 0x20},
	{0x59fa, 0x3f},
	{0x59fb, 0x38},
	{0x59fc, 0x30},
	{0x59fd, 0x3f},
	{0x59fe, 0x38},
	{0x59ff, 0x30},
	/*
	 * [gain < 2x] {0x363c, 0x05},
	 * [gain >=2x] {0x363c, 0x07},
	 */
	{0x363c, 0x07},
	{0x5799, 0x77},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval sc850sl_linear10bit_3840x2160_40fps_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x36ea, 0x09},
	{0x36eb, 0x0c},
	{0x36ec, 0x4b},
	{0x36ed, 0x34},
	{0x36fa, 0xcb},
	{0x36fb, 0x13},
	{0x36fc, 0x00},
	{0x36fd, 0x07},
	{0x36e9, 0x53},
	{0x36f9, 0x53},
	{0x3000, 0x01},
	{0x3018, 0x7a},
	{0x3019, 0xf0},
	{0x301a, 0x30},
	{0x301e, 0x3c},
	{0x301f, 0x0d},
	{0x302a, 0x00},
	{0x3031, 0x0a},
	{0x3032, 0x20},
	{0x3033, 0x22},
	{0x3037, 0x60},
	{0x303e, 0xb4},
	{0x3201, 0x98},
	{0x3203, 0x0c},
	{0x3205, 0xa7},
	{0x3207, 0x83},
	{0x320c, 0x03},
	{0x320d, 0x39},
	{0x3211, 0x08},
	{0x3213, 0x2c},
	{0x3223, 0xc0},
	{0x3226, 0x00},
	{0x3227, 0x03},
	{0x3230, 0x11},
	{0x3231, 0x93},
	{0x3250, 0x40},
	{0x3253, 0x08},
	{0x327e, 0x00},
	{0x3280, 0x00},
	{0x3281, 0x00},
	{0x3301, 0x24},
	{0x3304, 0x30},
	{0x3306, 0x54},
	{0x3308, 0x10},
	{0x3309, 0x60},
	{0x330a, 0x00},
	{0x330b, 0xa0},
	{0x330d, 0x10},
	{0x3314, 0x92},
	{0x331e, 0x29},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3347, 0x05},
	{0x3348, 0xd0},
	{0x3352, 0x01},
	{0x3356, 0x38},
	{0x335d, 0x60},
	{0x3362, 0x70},
	{0x338f, 0x80},
	{0x33af, 0x48},
	{0x33fe, 0x00},
	{0x3400, 0x12},
	{0x3406, 0x04},
	{0x3410, 0x12},
	{0x3416, 0x06},
	{0x3433, 0x01},
	{0x3440, 0x12},
	{0x3446, 0x08},
	{0x3478, 0x01},
	{0x3479, 0x01},
	{0x347a, 0x02},
	{0x347b, 0x01},
	{0x347c, 0x04},
	{0x347d, 0x01},
	{0x3616, 0x0c},
	{0x3620, 0x94},
	{0x3622, 0x74},
	{0x3629, 0x74},
	{0x362a, 0xf0},
	{0x362b, 0x0f},
	{0x362d, 0x00},
	{0x3630, 0x68},
	{0x3633, 0x24},
	{0x3634, 0x22},
	{0x3635, 0x20},
	{0x3637, 0x18},
	{0x3638, 0x26},
	{0x363b, 0x06},
	{0x363c, 0x07},
	{0x363d, 0x05},
	{0x363e, 0x8f},
	{0x3648, 0xe0},
	{0x3649, 0x0a},
	{0x364a, 0x06},
	{0x364c, 0x6a},
	{0x3650, 0x3d},
	{0x3654, 0x70},
	{0x3656, 0x68},
	{0x3657, 0x0f},
	{0x3658, 0x3d},
	{0x365c, 0x40},
	{0x365e, 0x68},
	{0x3901, 0x04},
	{0x3902, 0xf1},
	{0x3904, 0x20},
	{0x3905, 0x91},
	{0x391e, 0x03},
	{0x3928, 0x04},
	{0x3933, 0xa0},
	{0x3934, 0x0a},
	{0x3935, 0x68},
	{0x3936, 0x00},
	{0x3937, 0x20},
	{0x3938, 0x0a},
	{0x3946, 0x20},
	{0x3961, 0x40},
	{0x3962, 0x40},
	{0x3963, 0xc8},
	{0x3964, 0xc8},
	{0x3965, 0x40},
	{0x3966, 0x40},
	{0x3967, 0x00},
	{0x39cd, 0xc8},
	{0x39ce, 0xc8},
	{0x3e01, 0x82},
	{0x3e02, 0x00},
	{0x3e0e, 0x02},
	{0x3e0f, 0x00},
	{0x3e1c, 0x0f},
	{0x3e23, 0x00},
	{0x3e24, 0x00},
	{0x3e53, 0x00},
	{0x3e54, 0x00},
	{0x3e68, 0x00},
	{0x3e69, 0x80},
	{0x3e73, 0x00},
	{0x3e74, 0x00},
	{0x3e86, 0x03},
	{0x3e87, 0x40},
	{0x3f02, 0x24},
	{0x4424, 0x02},
	{0x4501, 0xb4},
	{0x4503, 0x20},
	{0x4509, 0x20},
	{0x4561, 0x12},
	{0x4800, 0x24},
	{0x4837, 0x20},
	{0x4900, 0x24},
	{0x4937, 0x16},
	{0x5000, 0x0e},
	{0x500f, 0x35},
	{0x5020, 0x00},
	{0x5787, 0x10},
	{0x5788, 0x06},
	{0x5789, 0x00},
	{0x578a, 0x18},
	{0x578b, 0x0c},
	{0x578c, 0x00},
	{0x5790, 0x10},
	{0x5791, 0x06},
	{0x5792, 0x01},
	{0x5793, 0x18},
	{0x5794, 0x0c},
	{0x5795, 0x01},
	{0x5799, 0x06},
	{0x57a2, 0x60},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x38},
	{0x59e3, 0x30},
	{0x59e4, 0x20},
	{0x59e5, 0x38},
	{0x59e6, 0x30},
	{0x59e7, 0x20},
	{0x59e8, 0x3f},
	{0x59e9, 0x38},
	{0x59ea, 0x30},
	{0x59eb, 0x3f},
	{0x59ec, 0x38},
	{0x59ed, 0x30},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x38},
	{0x59f5, 0x30},
	{0x59f6, 0x20},
	{0x59f7, 0x38},
	{0x59f8, 0x30},
	{0x59f9, 0x20},
	{0x59fa, 0x3f},
	{0x59fb, 0x38},
	{0x59fc, 0x30},
	{0x59fd, 0x3f},
	{0x59fe, 0x38},
	{0x59ff, 0x30},
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
static const struct sc850sl_mode supported_modes[] = {
#if defined CONFIG_VIDEO_CAM_SLEEP_WAKEUP
	{
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 400000,
		},
		.exp_def = 0x08c0,
		.hts_def = 0x10e0,
		.vts_def = 0x08ca,
		.reg_list = sc850sl_linear10bit_3840x2160_40fps_regs,
		.hdr_mode = NO_HDR,
		.xvclk_freq = 24000000,
		.mipi_freq_idx = 1,
		.bpp = 10,
		.vc[PAD0] = 0,
	},
#endif
	{
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x08c0,
		.hts_def = 0x0226 * 5 - 0x180,
		.vts_def = 0x08ca,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = sc850sl_linear10bit_3840x2160_30fps_regs,
		.hdr_mode = NO_HDR,
		.xvclk_freq = 24000000,
		.mipi_freq_idx = 0,
		.bpp = 10,
		.vc[PAD0] = 0,
	},
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,
};

static const char *const sc850sl_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

static const s64 link_freq_items[] = {
	MIPI_FREQ_540M,
	MIPI_FREQ_972M,
};

/* Write registers up to 4 at a time */
static int sc850sl_write_reg(struct i2c_client *client, u16 reg,
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

static int sc850sl_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		ret = sc850sl_write_reg(client, regs[i].addr,
					SC850SL_REG_VALUE_08BIT, regs[i].val);
	}
	return ret;
}

/* Read registers up to 4 at a time */
static int sc850sl_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int sc850sl_get_reso_dist(const struct sc850sl_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc850sl_mode *
sc850sl_find_best_fit(struct sc850sl *sc850sl, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = sc850sl_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		} else if (dist == cur_best_fit_dist &&
			   framefmt->code == supported_modes[i].bus_fmt) {
			cur_best_fit = i;
			break;
		}
	}
	dev_info(&sc850sl->client->dev, "%s: cur_best_fit(%d)",
		 __func__, cur_best_fit);

	return &supported_modes[cur_best_fit];
}

static void sc850sl_change_mode(struct sc850sl *sc850sl, const struct sc850sl_mode *mode)
{
	sc850sl->cur_mode = mode;
	sc850sl->cur_vts = sc850sl->cur_mode->vts_def;
	dev_info(&sc850sl->client->dev, "set fmt: cur_mode: %dx%d, hdr: %d\n",
		 mode->width, mode->height, mode->hdr_mode);
}

static int sc850sl_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);
	const struct sc850sl_mode *mode;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;

	mutex_lock(&sc850sl->mutex);

	mode = sc850sl_find_best_fit(sc850sl, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&sc850sl->mutex);
		return -ENOTTY;
#endif
	} else {
		sc850sl_change_mode(sc850sl, mode);
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc850sl->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc850sl->vblank, vblank_def,
					 SC850SL_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(sc850sl->link_freq, mode->mipi_freq_idx);
		pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
			     mode->bpp * 2 * SC850SL_4LANES;
		__v4l2_ctrl_s_ctrl_int64(sc850sl->pixel_rate, pixel_rate);
		sc850sl->cur_fps = mode->max_fps;
		sc850sl->cur_vts = mode->vts_def;
	}

	mutex_unlock(&sc850sl->mutex);

	return 0;
}

static int sc850sl_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);
	const struct sc850sl_mode *mode = sc850sl->cur_mode;

	mutex_lock(&sc850sl->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&sc850sl->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&sc850sl->mutex);

	return 0;
}

static int sc850sl_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int sc850sl_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);

	if (fse->index >= sc850sl->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int sc850sl_enable_test_pattern(struct sc850sl *sc850sl, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc850sl_read_reg(sc850sl->client, SC850SL_REG_TEST_PATTERN,
			       SC850SL_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC850SL_TEST_PATTERN_ENABLE;
	else
		val &= ~SC850SL_TEST_PATTERN_ENABLE;
	ret |= sc850sl_write_reg(sc850sl->client, SC850SL_REG_TEST_PATTERN,
				 SC850SL_REG_VALUE_08BIT, val);
	return ret;
}

static int sc850sl_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);
	const struct sc850sl_mode *mode = sc850sl->cur_mode;

	if (sc850sl->streaming)
		fi->interval = sc850sl->cur_fps;
	else
		fi->interval = mode->max_fps;

	return 0;
}

static const struct sc850sl_mode *sc850sl_find_mode(struct sc850sl *sc850sl, int fps)
{
	const struct sc850sl_mode *mode = NULL;
	const struct sc850sl_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < sc850sl->cfg_num; i++) {
		mode = &supported_modes[i];
		if (mode->width == sc850sl->cur_mode->width &&
		    mode->height == sc850sl->cur_mode->height &&
		    mode->hdr_mode == sc850sl->cur_mode->hdr_mode &&
		    mode->bus_fmt == sc850sl->cur_mode->bus_fmt) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator, mode->max_fps.numerator);
			if (cur_fps == fps) {
				match = mode;
				break;
			}
		}
	}
	return match;
}

static int sc850sl_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);
	const struct sc850sl_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	int fps;

	if (sc850sl->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = sc850sl_find_mode(sc850sl, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	sc850sl->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(sc850sl->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(sc850sl->vblank, vblank_def,
				 SC850SL_VTS_MAX - mode->height,
				 1, vblank_def);
	__v4l2_ctrl_s_ctrl(sc850sl->link_freq, mode->mipi_freq_idx);
	pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
		     mode->bpp * 2 * SC850SL_4LANES;
	__v4l2_ctrl_s_ctrl_int64(sc850sl->pixel_rate, pixel_rate);
	sc850sl->cur_fps = mode->max_fps;
	sc850sl->cur_vts = mode->vts_def;

	return 0;
}

static int sc850sl_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				 struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes  = SC850SL_4LANES;

	return 0;
}

static void sc850sl_get_module_inf(struct sc850sl *sc850sl,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC850SL_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc850sl->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc850sl->len_name, sizeof(inf->base.lens));
}

/* mode: 0 = lgain  1 = sgain */
static int sc850sl_set_gain_reg(struct sc850sl *sc850sl, u32 gain, int mode)
{
	u32 ANA_Fine_gainx64 = 1;
	u32 Dcg_gainx1000;
	u8 Coarse_gain = 1, DIG_gain = 1;
	u8 ANA_Fine_gain_reg = 0x40, DIG_Fine_gain_reg = 0;
	u8 Coarse_gain_reg, DIG_gain_reg;
	int ret = 0;
	u64 val = 0;

	gain = gain * 16;
	if (gain <= 1024)
		gain = 1024;
	else if (gain > SC850SL_GAIN_MAX * 16)
		gain = SC850SL_GAIN_MAX * 16;

	if (gain < 2048) { // start again  2 * 1024
		Dcg_gainx1000 = 1000;
		Coarse_gain = 1;
		DIG_gain = 1;
		Coarse_gain_reg = 0x03;
		DIG_gain_reg = 0x0;
		DIG_Fine_gain_reg = 0x80;
	} else if (gain < 3200) {// 3.125 * 1024
		Dcg_gainx1000 = 1000;
		Coarse_gain = 2;
		DIG_gain = 1;
		Coarse_gain_reg = 0x07;
		DIG_gain_reg = 0x0;
		DIG_Fine_gain_reg = 0x80;
	} else if (gain < 6400) {// 6.25 * 1024
		Dcg_gainx1000 = 3125;
		Coarse_gain = 1;
		DIG_gain = 1;
		Coarse_gain_reg = 0x23;
		DIG_gain_reg = 0x0;
		DIG_Fine_gain_reg = 0x80;
	} else if (gain < 12800) {// 12.5 * 1024
		Dcg_gainx1000 = 3125;
		Coarse_gain = 2;
		DIG_gain = 1;
		Coarse_gain_reg = 0x27;
		DIG_gain_reg = 0x0;
		DIG_Fine_gain_reg = 0x80;
	} else if (gain < 25600) {// 25 * 1024
		Dcg_gainx1000 = 3125;
		Coarse_gain = 4;
		DIG_gain = 1;
		Coarse_gain_reg = 0x2f;
		DIG_gain_reg = 0x0;
		DIG_Fine_gain_reg = 0x80;
	} else if (gain < 51200) {// 50 * 1024 // end again
		Dcg_gainx1000 = 3125;
		Coarse_gain = 8;
		DIG_gain = 1;
		Coarse_gain_reg = 0x3f;
		DIG_gain_reg = 0x0;
		DIG_Fine_gain_reg = 0x80;
	} else if (gain < 50800 * 2) {// start dgain
		Dcg_gainx1000 = 3125;
		Coarse_gain = 8;
		DIG_gain = 1;
		ANA_Fine_gainx64 = 127;
		Coarse_gain_reg = 0x3f;
		DIG_gain_reg = 0x0;
		ANA_Fine_gain_reg = 0x7f;
	} else if (gain < 50800 * 4) {
		Dcg_gainx1000 = 3125;
		Coarse_gain = 8;
		DIG_gain = 2;
		ANA_Fine_gainx64 = 127;
		Coarse_gain_reg = 0x3f;
		DIG_gain_reg = 0x1;
		ANA_Fine_gain_reg = 0x7f;
	} else if (gain < 50800 * 8) {
		Dcg_gainx1000 = 3125;
		Coarse_gain = 8;
		DIG_gain = 4;
		ANA_Fine_gainx64 = 127;
		Coarse_gain_reg = 0x3f;
		DIG_gain_reg = 0x3;
		ANA_Fine_gain_reg = 0x7f;
	} else {
		Dcg_gainx1000 = 3125;
		Coarse_gain = 8;
		DIG_gain = 8;
		ANA_Fine_gainx64 = 127;
		Coarse_gain_reg = 0x3f;
		DIG_gain_reg = 0x7;
		ANA_Fine_gain_reg = 0x7f;
	}

	if (gain < 2048) {
		val = div_u64(1000ULL * gain, (Dcg_gainx1000 * Coarse_gain));
		ANA_Fine_gain_reg = div_u64(val, 16);
	} else if (gain == 3200) {
		ANA_Fine_gain_reg = 0x40;
	} else if (gain < 51200) {
		val = div_u64(1000ULL * gain, (Dcg_gainx1000 * Coarse_gain));
		ANA_Fine_gain_reg = div_u64(val, 16);
	} else if (gain < 50800 * 8) {
		val = div_u64(8000ULL * gain, (Dcg_gainx1000 * Coarse_gain * DIG_gain));
		DIG_Fine_gain_reg = div_u64(val, ANA_Fine_gainx64);
	} else {
		DIG_Fine_gain_reg = 0x80;
	}

	if (mode == SC850SL_LGAIN) {
		ret = sc850sl_write_reg(sc850sl->client,
					SC850SL_REG_DGAIN,
					SC850SL_REG_VALUE_08BIT,
					DIG_gain_reg & 0xF);
		ret |= sc850sl_write_reg(sc850sl->client,
					 SC850SL_REG_DGAIN_FINE,
					 SC850SL_REG_VALUE_08BIT,
					 DIG_Fine_gain_reg);
		ret |= sc850sl_write_reg(sc850sl->client,
					 SC850SL_REG_AGAIN,
					 SC850SL_REG_VALUE_08BIT,
					 Coarse_gain_reg);
		ret |= sc850sl_write_reg(sc850sl->client,
					 SC850SL_REG_AGAIN_FINE,
					 SC850SL_REG_VALUE_08BIT,
					 ANA_Fine_gain_reg);
	}
#if ENABLE_NR
	if (gain < 2048)
		ret |= sc850sl_write_reg(sc850sl->client,
					 0x363c,
					 SC850SL_REG_VALUE_08BIT,
					 0x05);
	else
		ret |= sc850sl_write_reg(sc850sl->client,
					 0x363c,
					 SC850SL_REG_VALUE_08BIT,
					 0x07);
#endif
	dev_dbg(&sc850sl->client->dev,
		"recv_gain:%d set again 0x%x, again_fine 0x%x, set dgain 0x%x, dgain_fine 0x%x\n",
		gain / 16, Coarse_gain_reg, ANA_Fine_gain_reg, DIG_gain_reg, DIG_Fine_gain_reg);
	return ret;
}

static int sc850sl_get_channel_info(struct sc850sl *sc850sl, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = sc850sl->cur_mode->vc[ch_info->index];
	ch_info->width = sc850sl->cur_mode->width;
	ch_info->height = sc850sl->cur_mode->height;
	ch_info->bus_fmt = sc850sl->cur_mode->bus_fmt;
	return 0;
}

static long sc850sl_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);
	struct rkmodule_hdr_cfg *hdr_cfg;
	const struct sc850sl_mode *mode;
	struct rkmodule_channel_info *ch_info;
	long ret = 0;
	u64 pixel_rate = 0;
	u32 i, h, w, stream;
	int cur_best_fit = -1;
	int cur_best_fit_dist = -1;
	int cur_dist, cur_fps, dst_fps;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		/*
		 * ret = sc850sl_set_hdrae(sc850sl, arg);
		 */
		if (sc850sl->cam_sw_info)
			memcpy(&sc850sl->cam_sw_info->hdr_ae, (struct preisp_hdrae_exp_s *)(arg),
			       sizeof(struct preisp_hdrae_exp_s));
		break;

	case RKMODULE_SET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		if (hdr_cfg->hdr_mode == sc850sl->cur_mode->hdr_mode)
			return 0;
		w = sc850sl->cur_mode->width;
		h = sc850sl->cur_mode->height;
		dst_fps = DIV_ROUND_CLOSEST(sc850sl->cur_mode->max_fps.denominator,
					    sc850sl->cur_mode->max_fps.numerator);
		for (i = 0; i < sc850sl->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr_cfg->hdr_mode &&
			    supported_modes[i].bus_fmt == sc850sl->cur_mode->bus_fmt) {
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
			dev_err(&sc850sl->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr_cfg->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			sc850sl_change_mode(sc850sl, &supported_modes[cur_best_fit]);
			mode = sc850sl->cur_mode;
			w = mode->hts_def - mode->width;
			h = mode->vts_def - mode->height;
			__v4l2_ctrl_modify_range(sc850sl->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(sc850sl->vblank, h,
						 SC850SL_VTS_MAX - mode->height,
						 1, h);
			__v4l2_ctrl_s_ctrl(sc850sl->link_freq, mode->mipi_freq_idx);
			pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
				     mode->bpp * 2 * SC850SL_4LANES;
			__v4l2_ctrl_s_ctrl_int64(sc850sl->pixel_rate,
						 pixel_rate);
			sc850sl->cur_fps = mode->max_fps;
			sc850sl->cur_vts = mode->vts_def;
			dev_info(&sc850sl->client->dev,
				 "sensor mode: %d\n", mode->hdr_mode);
		}
		break;
	case RKMODULE_GET_MODULE_INFO:
		sc850sl_get_module_inf(sc850sl, (struct rkmodule_inf *)arg);
		break;

	case RKMODULE_GET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		hdr_cfg->esp.mode = HDR_NORMAL_VC;
		hdr_cfg->hdr_mode = sc850sl->cur_mode->hdr_mode;
		break;

	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);
		dev_info(&sc850sl->client->dev, "stream: %d\n", stream);

		if (stream) {
			ret |= sc850sl_write_reg(sc850sl->client, 0x3019,
						 SC850SL_REG_VALUE_08BIT,
						 0xf0);
			ret |= sc850sl_write_reg(sc850sl->client, 0x3018,
						 SC850SL_REG_VALUE_08BIT,
						 0x7a);
			ret |= sc850sl_write_reg(sc850sl->client, 0x302c,
						 SC850SL_REG_VALUE_08BIT,
						 0x00);
			ret |= sc850sl_write_reg(sc850sl->client, SC850SL_REG_CTRL_MODE,
						 SC850SL_REG_VALUE_08BIT,
						 SC850SL_MODE_STREAMING);
		} else {
			ret |= sc850sl_write_reg(sc850sl->client, 0x3018,
						 SC850SL_REG_VALUE_08BIT,
						 0x7f);
			ret |= sc850sl_write_reg(sc850sl->client, 0x3019,
						 SC850SL_REG_VALUE_08BIT,
						 0xff);
			ret |= sc850sl_write_reg(sc850sl->client, SC850SL_REG_CTRL_MODE,
						 SC850SL_REG_VALUE_08BIT,
						 SC850SL_MODE_SW_STANDBY);
			ret |= sc850sl_write_reg(sc850sl->client, 0x302c,
						 SC850SL_REG_VALUE_08BIT,
						 0x0f);
		}
		break;

	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = sc850sl_get_channel_info(sc850sl, ch_info);
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long sc850sl_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rkmodule_channel_info *ch_info;
	long ret;
	u32  stream;
	u32 brl = 0;
	struct rkmodule_csi_dphy_param *dphy_param;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc850sl_ioctl(sd, cmd, inf);
		if (!ret) {
			if (copy_to_user(up, inf, sizeof(*inf))) {
				kfree(inf);
				return -EFAULT;
			}
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(cfg, up, sizeof(*cfg))) {
			kfree(cfg);
			return -EFAULT;
		}
		ret = sc850sl_ioctl(sd, cmd, cfg);
		kfree(cfg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc850sl_ioctl(sd, cmd, hdr);
		if (!ret) {
			if (copy_to_user(up, hdr, sizeof(*hdr))) {
				kfree(hdr);
				return -EFAULT;
			}
		}
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(hdr, up, sizeof(*hdr))) {
			kfree(hdr);
			return -EFAULT;
		}
		ret = sc850sl_ioctl(sd, cmd, hdr);
		kfree(hdr);
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		hdrae = kzalloc(sizeof(*hdrae), GFP_KERNEL);
		if (!hdrae) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(hdrae, up, sizeof(*hdrae))) {
			kfree(hdrae);
			return -EFAULT;
		}
		ret = sc850sl_ioctl(sd, cmd, hdrae);
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		if (copy_from_user(&stream, up, sizeof(u32)))
			return -EFAULT;
		ret = sc850sl_ioctl(sd, cmd, &stream);
		break;
	case RKMODULE_GET_SONY_BRL:
		ret = sc850sl_ioctl(sd, cmd, &brl);
		if (!ret) {
			if (copy_to_user(up, &brl, sizeof(u32)))
				return -EFAULT;
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc850sl_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc850sl_ioctl(sd, cmd, dphy_param);
		if (!ret) {
			ret = copy_to_user(up, dphy_param, sizeof(*dphy_param));
			if (ret)
				ret = -EFAULT;
		}
		kfree(dphy_param);
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif


static int __sc850sl_start_stream(struct sc850sl *sc850sl)
{
	int ret;

	dev_info(&sc850sl->client->dev,
		 "%dx%d@%d, mode %d, vts 0x%x\n",
		 sc850sl->cur_mode->width,
		 sc850sl->cur_mode->height,
		 sc850sl->cur_fps.denominator / sc850sl->cur_fps.numerator,
		 sc850sl->cur_mode->hdr_mode,
		 sc850sl->cur_vts);

	if (!sc850sl->is_thunderboot) {
		ret = sc850sl_write_array(sc850sl->client, sc850sl->cur_mode->reg_list);
		if (ret)
			return ret;

		ret = __v4l2_ctrl_handler_setup(&sc850sl->ctrl_handler);
		if (ret)
			return ret;
		/* In case these controls are set before streaming */
		if (sc850sl->has_init_exp && sc850sl->cur_mode->hdr_mode != NO_HDR) {
			ret = sc850sl_ioctl(&sc850sl->subdev, PREISP_CMD_SET_HDRAE_EXP,
					    &sc850sl->init_hdrae_exp);
			if (ret) {
				dev_err(&sc850sl->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	return sc850sl_write_reg(sc850sl->client, SC850SL_REG_CTRL_MODE,
				 SC850SL_REG_VALUE_08BIT, SC850SL_MODE_STREAMING);
}

static int __sc850sl_stop_stream(struct sc850sl *sc850sl)
{
	sc850sl->has_init_exp = false;
	if (sc850sl->is_thunderboot) {
		sc850sl->is_first_streamoff = true;
	}
	return sc850sl_write_reg(sc850sl->client, SC850SL_REG_CTRL_MODE,
				 SC850SL_REG_VALUE_08BIT, SC850SL_MODE_SW_STANDBY);
}

static int __sc850sl_power_on(struct sc850sl *sc850sl);
static int sc850sl_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);
	struct i2c_client *client = sc850sl->client;
	int ret = 0;

	dev_info(&sc850sl->client->dev, "s_stream: %d. %dx%d, hdr: %d, bpp: %d\n",
		 on, sc850sl->cur_mode->width, sc850sl->cur_mode->height,
		 sc850sl->cur_mode->hdr_mode, sc850sl->cur_mode->bpp);

	mutex_lock(&sc850sl->mutex);
	on = !!on;
	if (on == sc850sl->streaming)
		goto unlock_and_return;

	if (on) {
		if (sc850sl->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			sc850sl->is_thunderboot = false;
			__sc850sl_power_on(sc850sl);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __sc850sl_start_stream(sc850sl);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc850sl_stop_stream(sc850sl);
		pm_runtime_put(&client->dev);
	}

	sc850sl->streaming = on;

unlock_and_return:
	mutex_unlock(&sc850sl->mutex);
	return ret;
}

static int sc850sl_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);
	struct i2c_client *client = sc850sl->client;
	int ret = 0;

	mutex_lock(&sc850sl->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc850sl->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		if (!sc850sl->is_thunderboot) {
			ret |= sc850sl_write_reg(sc850sl->client,
						 SC850SL_SOFTWARE_RESET_REG,
						 SC850SL_REG_VALUE_08BIT,
						 0x01);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
			udelay(100);
		}

		sc850sl->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc850sl->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc850sl->mutex);

	return ret;
}

static int __sc850sl_power_on(struct sc850sl *sc850sl)
{
	int ret;
	struct device *dev = &sc850sl->client->dev;

	if (!IS_ERR_OR_NULL(sc850sl->pins_default)) {
		ret = pinctrl_select_state(sc850sl->pinctrl, sc850sl->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc850sl->xvclk, sc850sl->cur_mode->xvclk_freq);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate 24MHz\n");
	if (clk_get_rate(sc850sl->xvclk) != sc850sl->cur_mode->xvclk_freq)
		dev_warn(dev, "xvclk mismatched\n");
	ret = clk_prepare_enable(sc850sl->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	cam_sw_regulator_bulk_init(sc850sl->cam_sw_info,
				   SC850SL_NUM_SUPPLIES, sc850sl->supplies);

	if (sc850sl->is_thunderboot)
		return 0;

	if (!IS_ERR(sc850sl->pwdn_gpio))
		gpiod_set_value_cansleep(sc850sl->pwdn_gpio, 1);

	usleep_range(4000, 6000);
	if (!IS_ERR(sc850sl->reset_gpio))
		gpiod_set_value_cansleep(sc850sl->reset_gpio, 1);

	usleep_range(4000, 6000);

	ret = regulator_bulk_enable(SC850SL_NUM_SUPPLIES, sc850sl->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc850sl->reset_gpio))
		gpiod_set_value_cansleep(sc850sl->reset_gpio, 0);

	usleep_range(4000, 6000);

	if (!IS_ERR(sc850sl->pwdn_gpio))
		gpiod_set_value_cansleep(sc850sl->pwdn_gpio, 1);
	return 0;
err_clk:
	if (!IS_ERR(sc850sl->reset_gpio))
		gpiod_direction_output(sc850sl->reset_gpio, 0);
disable_clk:
	clk_disable_unprepare(sc850sl->xvclk);

	return ret;
}

static void __sc850sl_power_off(struct sc850sl *sc850sl)
{
	int ret;
	struct device *dev = &sc850sl->client->dev;

	clk_disable_unprepare(sc850sl->xvclk);
	if (sc850sl->is_thunderboot) {
		if (sc850sl->is_first_streamoff) {
			sc850sl->is_thunderboot = false;
			sc850sl->is_first_streamoff = false;
		} else {
			return;
		}
	}
	if (!IS_ERR(sc850sl->reset_gpio))
		gpiod_direction_output(sc850sl->reset_gpio, 1);

	if (!IS_ERR_OR_NULL(sc850sl->pins_sleep)) {
		ret = pinctrl_select_state(sc850sl->pinctrl,
					   sc850sl->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	if (!IS_ERR(sc850sl->pwdn_gpio))
		gpiod_direction_output(sc850sl->pwdn_gpio, 0);
	regulator_bulk_disable(SC850SL_NUM_SUPPLIES, sc850sl->supplies);
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int sc850sl_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc850sl *sc850sl = to_sc850sl(sd);

	if (sc850sl->standby_hw) {
		dev_info(dev, "resume standby!");
		return 0;
	}

	cam_sw_prepare_wakeup(sc850sl->cam_sw_info, dev);

	usleep_range(4000, 5000);
	cam_sw_write_array(sc850sl->cam_sw_info);

	if (__v4l2_ctrl_handler_setup(&sc850sl->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (sc850sl->has_init_exp && sc850sl->cur_mode != NO_HDR) {	// hdr mode
		ret = sc850sl_ioctl(&sc850sl->subdev, PREISP_CMD_SET_HDRAE_EXP,
				    &sc850sl->cam_sw_info->hdr_ae);
		if (ret) {
			dev_err(&sc850sl->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}
	return 0;
}

static int sc850sl_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc850sl *sc850sl = to_sc850sl(sd);

	if (sc850sl->standby_hw) {
		dev_info(dev, "suspend standby!");
		return 0;
	}

	cam_sw_write_array_cb_init(sc850sl->cam_sw_info, client,
				   (void *)sc850sl->cur_mode->reg_list,
				   (sensor_write_array)sc850sl_write_array);
	cam_sw_prepare_sleep(sc850sl->cam_sw_info);

	return 0;
}
#else
#define sc850sl_resume NULL
#define sc850sl_suspend NULL
#endif

static int sc850sl_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc850sl *sc850sl = to_sc850sl(sd);

	return __sc850sl_power_on(sc850sl);
}

static int sc850sl_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc850sl *sc850sl = to_sc850sl(sd);

	__sc850sl_power_off(sc850sl);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc850sl_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct sc850sl_mode *def_mode = &supported_modes[0];

	mutex_lock(&sc850sl->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc850sl->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc850sl_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);

	if (fie->index >= sc850sl->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_3840 3840
#define DST_HEIGHT_2160 2160
#define DST_WIDTH_1920 1920
#define DST_HEIGHT_1080 1080

static int sc850sl_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_selection *sel)
{
	struct sc850sl *sc850sl = to_sc850sl(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		if (sc850sl->cur_mode->width == 3856) {
			sel->r.left = CROP_START(sc850sl->cur_mode->width, DST_WIDTH_3840);
			sel->r.width = DST_WIDTH_3840;
			sel->r.top = CROP_START(sc850sl->cur_mode->height, DST_HEIGHT_2160);
			sel->r.height = DST_HEIGHT_2160;
		} else if (sc850sl->cur_mode->width == 1944) {
			sel->r.left = CROP_START(sc850sl->cur_mode->width, DST_WIDTH_1920);
			sel->r.width = DST_WIDTH_1920;
			sel->r.top = CROP_START(sc850sl->cur_mode->height, DST_HEIGHT_1080);
			sel->r.height = DST_HEIGHT_1080;
		} else {
			sel->r.left = CROP_START(sc850sl->cur_mode->width,
						 sc850sl->cur_mode->width);
			sel->r.width = sc850sl->cur_mode->width;
			sel->r.top = CROP_START(sc850sl->cur_mode->height,
						sc850sl->cur_mode->height);
			sel->r.height = sc850sl->cur_mode->height;
		}
		return 0;
	}
	return -EINVAL;
}

static const struct dev_pm_ops sc850sl_pm_ops = {
	SET_RUNTIME_PM_OPS(sc850sl_runtime_suspend,
	sc850sl_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sc850sl_suspend, sc850sl_resume)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc850sl_internal_ops = {
	.open = sc850sl_open,
};
#endif

static const struct v4l2_subdev_core_ops sc850sl_core_ops = {
	.s_power = sc850sl_s_power,
	.ioctl = sc850sl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc850sl_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc850sl_video_ops = {
	.s_stream = sc850sl_s_stream,
	.g_frame_interval = sc850sl_g_frame_interval,
	.s_frame_interval = sc850sl_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops sc850sl_pad_ops = {
	.enum_mbus_code = sc850sl_enum_mbus_code,
	.enum_frame_size = sc850sl_enum_frame_sizes,
	.enum_frame_interval = sc850sl_enum_frame_interval,
	.get_fmt = sc850sl_get_fmt,
	.set_fmt = sc850sl_set_fmt,
	.get_selection = sc850sl_get_selection,
	.get_mbus_config = sc850sl_g_mbus_config,
};

static const struct v4l2_subdev_ops sc850sl_subdev_ops = {
	.core	= &sc850sl_core_ops,
	.video	= &sc850sl_video_ops,
	.pad	= &sc850sl_pad_ops,
};

static void sc850sl_modify_fps_info(struct sc850sl *sc850sl)
{
	const struct sc850sl_mode *mode = sc850sl->cur_mode;

	sc850sl->cur_fps.denominator = mode->max_fps.denominator * sc850sl->cur_vts /
				       mode->vts_def;
}

static int sc850sl_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc850sl *sc850sl = container_of(ctrl->handler,
					       struct sc850sl, ctrl_handler);
	struct i2c_client *client = sc850sl->client;
	s64 max;
	int ret = 0;
	u32 val;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc850sl->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(sc850sl->exposure,
					 sc850sl->exposure->minimum, max,
					 sc850sl->exposure->step,
					 sc850sl->exposure->default_value);
		break;
	}

	if (sc850sl->standby_hw && sc850sl->is_standby) {
		dev_dbg(&client->dev, "%s: is_standby=true, will return\n", __func__);
		return 0;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (sc850sl->cur_mode->hdr_mode != NO_HDR)
			goto out_ctrl;
		ret = sc850sl_write_reg(sc850sl->client,
					SC850SL_REG_EXP_LONG_H,
					SC850SL_REG_VALUE_08BIT,
					SC850SL_FETCH_EXP_H(ctrl->val));
		ret |= sc850sl_write_reg(sc850sl->client,
					 SC850SL_REG_EXP_LONG_M,
					 SC850SL_REG_VALUE_08BIT,
					 SC850SL_FETCH_EXP_M(ctrl->val));
		ret |= sc850sl_write_reg(sc850sl->client,
					 SC850SL_REG_EXP_LONG_L,
					 SC850SL_REG_VALUE_08BIT,
					 SC850SL_FETCH_EXP_L(ctrl->val));

		dev_dbg(&client->dev, "set exposure 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (sc850sl->cur_mode->hdr_mode != NO_HDR)
			goto out_ctrl;
		ret = sc850sl_set_gain_reg(sc850sl, ctrl->val, SC850SL_LGAIN);

		break;
	case V4L2_CID_VBLANK:
		ret = sc850sl_write_reg(sc850sl->client, SC850SL_REG_VTS,
					SC850SL_REG_VALUE_16BIT,
					ctrl->val + sc850sl->cur_mode->height);
		if (!ret)
			sc850sl->cur_vts = ctrl->val + sc850sl->cur_mode->height;
		sc850sl_modify_fps_info(sc850sl);
		dev_dbg(&client->dev, "set vblank 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc850sl_enable_test_pattern(sc850sl, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc850sl_read_reg(sc850sl->client, SC850SL_FLIP_REG,
				       SC850SL_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= SC850SL_MIRROR_MASK;
		else
			val &= ~SC850SL_MIRROR_MASK;
		ret |= sc850sl_write_reg(sc850sl->client, SC850SL_FLIP_REG,
					 SC850SL_REG_VALUE_08BIT, val);
		break;
	case V4L2_CID_VFLIP:
		ret = sc850sl_read_reg(sc850sl->client, SC850SL_FLIP_REG,
				       SC850SL_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= SC850SL_FLIP_MASK;
		else
			val &= ~SC850SL_FLIP_MASK;
		ret |= sc850sl_write_reg(sc850sl->client, SC850SL_FLIP_REG,
					 SC850SL_REG_VALUE_08BIT, val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

out_ctrl:
	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sc850sl_ctrl_ops = {
	.s_ctrl = sc850sl_set_ctrl,
};

static int sc850sl_initialize_controls(struct sc850sl *sc850sl)
{
	const struct sc850sl_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u64 pixel_rate = 0;
	u32 h_blank;
	int ret;

	handler = &sc850sl->ctrl_handler;
	mode = sc850sl->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc850sl->mutex;

	sc850sl->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			     V4L2_CID_LINK_FREQ, 0, 0, link_freq_items);
	v4l2_ctrl_s_ctrl(sc850sl->link_freq, mode->mipi_freq_idx);

	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 * SC850SL_4LANES;
	sc850sl->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
						V4L2_CID_PIXEL_RATE, 0, SC850SL_MAX_PIXEL_RATE,
						1, pixel_rate);

	h_blank = mode->hts_def - mode->width;
	sc850sl->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					    h_blank, h_blank, 1, h_blank);
	if (sc850sl->hblank)
		sc850sl->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	sc850sl->vblank = v4l2_ctrl_new_std(handler, &sc850sl_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_def,
					    SC850SL_VTS_MAX - mode->height,
					    1, vblank_def);

	exposure_max = mode->vts_def - 4;	/*vts_def  0x08ca=2250*/
	sc850sl->exposure = v4l2_ctrl_new_std(handler, &sc850sl_ctrl_ops,
					      V4L2_CID_EXPOSURE, SC850SL_EXPOSURE_MIN,
					      exposure_max, SC850SL_EXPOSURE_STEP,
					      mode->exp_def);	/*exp_def 0x08c0=2240*/

	sc850sl->anal_a_gain = v4l2_ctrl_new_std(handler, &sc850sl_ctrl_ops,
			       V4L2_CID_ANALOGUE_GAIN, SC850SL_GAIN_MIN,
			       SC850SL_GAIN_MAX, SC850SL_GAIN_STEP,
			       SC850SL_GAIN_DEFAULT);

	sc850sl->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&sc850sl_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(sc850sl_test_pattern_menu) - 1,
				0, 0, sc850sl_test_pattern_menu);

	v4l2_ctrl_new_std(handler, &sc850sl_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &sc850sl_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&sc850sl->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc850sl->subdev.ctrl_handler = handler;
	sc850sl->has_init_exp = false;
	sc850sl->is_standby = false;
	sc850sl->cur_fps = mode->max_fps;
	sc850sl->cur_vts = mode->vts_def;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc850sl_check_sensor_id(struct sc850sl *sc850sl,
				   struct i2c_client *client)
{
	struct device *dev = &sc850sl->client->dev;
	u32 id = 0;
	int ret;

	if (sc850sl->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}
	ret = sc850sl_read_reg(client, SC850SL_REG_CHIP_ID,
			       SC850SL_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected sc850sl id(%06x)\n", CHIP_ID);

	return 0;
}

static int sc850sl_configure_regulators(struct sc850sl *sc850sl)
{
	unsigned int i;

	for (i = 0; i < SC850SL_NUM_SUPPLIES; i++)
		sc850sl->supplies[i].supply = sc850sl_supply_names[i];

	return devm_regulator_bulk_get(&sc850sl->client->dev,
				       SC850SL_NUM_SUPPLIES,
				       sc850sl->supplies);
}

static int sc850sl_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sc850sl *sc850sl;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	sc850sl = devm_kzalloc(dev, sizeof(*sc850sl), GFP_KERNEL);
	if (!sc850sl)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc850sl->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc850sl->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc850sl->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc850sl->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	/* Compatible with non-standby mode if this attribute is not configured in dts*/
	of_property_read_u32(node, RKMODULE_CAMERA_STANDBY_HW,
			     &sc850sl->standby_hw);

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	sc850sl->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);
	sc850sl->client = client;
	sc850sl->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < sc850sl->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			sc850sl->cur_mode = &supported_modes[i];
			break;
		}
	}

	sc850sl->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc850sl->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	sc850sl->reset_gpio = devm_gpiod_get(dev, "reset",
					     sc850sl->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(sc850sl->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	sc850sl->pwdn_gpio = devm_gpiod_get(dev, "pwdn",
					    sc850sl->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(sc850sl->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn_gpio\n");

	sc850sl->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc850sl->pinctrl)) {
		sc850sl->pins_default =
			pinctrl_lookup_state(sc850sl->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc850sl->pins_default))
			dev_info(dev, "could not get default pinstate\n");

		sc850sl->pins_sleep =
			pinctrl_lookup_state(sc850sl->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc850sl->pins_sleep))
			dev_info(dev, "could not get sleep pinstate\n");
	} else {
		dev_info(dev, "no pinctrl\n");
	}

	ret = sc850sl_configure_regulators(sc850sl);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc850sl->mutex);

	sd = &sc850sl->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc850sl_subdev_ops);
	ret = sc850sl_initialize_controls(sc850sl);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc850sl_power_on(sc850sl);
	if (ret)
		goto err_free_handler;

	ret = sc850sl_check_sensor_id(sc850sl, client);
	if (ret)
		goto err_power_off;
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc850sl_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc850sl->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc850sl->pad);
	if (ret < 0)
		goto err_power_off;
#endif
	if (!sc850sl->cam_sw_info) {
		sc850sl->cam_sw_info = cam_sw_init();
		cam_sw_clk_init(sc850sl->cam_sw_info, sc850sl->xvclk, sc850sl->cur_mode->xvclk_freq);
		cam_sw_reset_pin_init(sc850sl->cam_sw_info, sc850sl->reset_gpio, 0);
		cam_sw_pwdn_pin_init(sc850sl->cam_sw_info, sc850sl->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc850sl->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';
	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc850sl->module_index, facing,
		 SC850SL_NAME, dev_name(sd->dev));

	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (sc850sl->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__sc850sl_power_off(sc850sl);
err_free_handler:
	v4l2_ctrl_handler_free(&sc850sl->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc850sl->mutex);

	return ret;
}

static void sc850sl_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc850sl *sc850sl = to_sc850sl(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc850sl->ctrl_handler);
	mutex_destroy(&sc850sl->mutex);

	cam_sw_deinit(sc850sl->cam_sw_info);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc850sl_power_off(sc850sl);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc850sl_of_match[] = {
	{ .compatible = "smartsens,sc850sl" },
	{},
};
MODULE_DEVICE_TABLE(of, sc850sl_of_match);
#endif

static const struct i2c_device_id sc850sl_match_id[] = {
	{ "smartsens,sc850sl", 0 },
	{ },
};

static struct i2c_driver sc850sl_i2c_driver = {
	.driver = {
		.name = SC850SL_NAME,
		.pm = &sc850sl_pm_ops,
		.of_match_table = of_match_ptr(sc850sl_of_match),
	},
	.probe		= &sc850sl_probe,
	.remove		= &sc850sl_remove,
	.id_table	= sc850sl_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc850sl_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc850sl_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens,sc850sl sensor driver");
MODULE_LICENSE("GPL");
