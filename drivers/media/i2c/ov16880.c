// SPDX-License-Identifier: GPL-2.0
/*
 * ov16880 camera driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version.
 * V0.0X01.0X01 support rk otp spec.
 *
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
#include <linux/compat.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include "otp_eeprom.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)
#define OV16880_MAJOR_I2C_ADDR		0x36
#define OV16880_MINOR_I2C_ADDR		0x10

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define OV16880_LINK_FREQ_720MHZ	720000000U

/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define OV16880_PIXEL_RATE		(OV16880_LINK_FREQ_720MHZ * 2LL * 4LL / 10LL)
#define OV16880_XVCLK_FREQ		24000000

#define CHIP_ID				0x016880
#define OV16880_REG_CHIP_ID		0x300a

#define OV16880_REG_CTRL_MODE		0x0100
#define OV16880_MODE_SW_STANDBY		0x0
#define OV16880_MODE_STREAMING		BIT(0)

#define OV16880_REG_EXPOSURE_H		0x3500
#define OV16880_REG_EXPOSURE_M		0x3501
#define OV16880_REG_EXPOSURE_L		0x3502
#define	OV16880_EXPOSURE_MIN		8
#define	OV16880_EXPOSURE_STEP		1
#define OV16880_VTS_MAX			0x7fff

#define OV16880_AGAIN_REG_H		0x350A
#define OV16880_AGAIN_REG_L		0x350B
#define OV16880_SF_AGAIN_REG_H		0x350E
#define OV16880_SF_AGAIN_REG_L		0x350F
#define OV16880_GAIN_H_MASK		0x1f
#define OV16880_GAIN_H_SHIFT		8


#define OV16880_GAIN_MIN		0x10
#define OV16880_GAIN_MAX		0xf8
#define OV16880_GAIN_STEP		1
#define OV16880_GAIN_DEFAULT		0x20

#define OV16880_SOFTWARE_RESET_REG	0x0103
#define OV16880_REG_ISP_X_WIN		0x3810

#define OV16880_GROUP_UPDATE_ADDRESS	0x3208
#define OV16880_GROUP_UPDATE_START_DATA	0x00
#define OV16880_GROUP_UPDATE_END_DATA	0x10
#define OV16880_GROUP_UPDATE_LAUNCH	0xA0

#define OV16880_REG_TEST_PATTERN	0x5081
#define	OV16880_TEST_PATTERN_ENABLE	0x01
#define	OV16880_TEST_PATTERN_DISABLE	0x0

#define OV16880_REG_VTS_H		0x380e
#define OV16880_REG_VTS_L		0x380f

#define OV16880_FLIP_REG		0x3820
#define OV16880_MIRROR_REG		0x3821
#define MIRROR_BIT_MASK			BIT(2)
#define FLIP_BIT_MASK			BIT(2)

#define OV16880_BLC_CTRL_REG		0x4000

#define OV16880_FETCH_EXP_H(VAL)	(((VAL) >> 16) & 0xF)
#define OV16880_FETCH_EXP_M(VAL)	(((VAL) >> 8) & 0xFF)
#define OV16880_FETCH_EXP_L(VAL)	((VAL) & 0xFF)

#define OV16880_FETCH_AGAIN_H(VAL)	(((VAL) >> 8) & 0x7)
#define OV16880_FETCH_AGAIN_L(VAL)	((VAL) & 0xFF)

#define OV16880_FETCH_VTS_H(VAL)	(((VAL) >> 8) & 0x7F)
#define OV16880_FETCH_VTS_L(VAL)	((VAL) & 0xFF)

#define REG_NULL			0xFFFF

#define OV16880_REG_VALUE_08BIT		1
#define OV16880_REG_VALUE_16BIT		2
#define OV16880_REG_VALUE_24BIT		3

#define OV16880_LANES			4
#define OV16880_BITS_PER_SAMPLE		10

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define RK_OTP

#define OV16880_NAME			"ov16880"
#define OV16880_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SBGGR10_1X10

static const char * const ov16880_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define OV16880_NUM_SUPPLIES ARRAY_SIZE(ov16880_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct ov16880_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 link_freq_idx;
	u32 bpp;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct ov16880 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[OV16880_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*h_flip;
	struct v4l2_ctrl	*v_flip;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	const struct ov16880_mode *cur_mode;
	u32			cfg_num;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
#ifdef RK_OTP
	struct otp_info		*otp;
#endif
};

#define to_ov16880(sd) container_of(sd, struct ov16880, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval ov16880_global_regs[] = {
	// init_setting
	{0x301e, 0x00},
	{0x0103, 0x01},
	{0x0300, 0x00},
	{0x0303, 0x00},
	{0x0304, 0x07},
	{0x030e, 0x02},
	{0x0312, 0x03},
	{0x0313, 0x03},
	{0x031e, 0x09},
	{0x3002, 0x00},
	{0x3009, 0x06},
	{0x3010, 0x00},
	{0x3011, 0x04},
	{0x3012, 0x41},
	{0x3013, 0xcc},
	{0x3017, 0xf0},
	{0x3018, 0xda},
	{0x3019, 0xf1},
	{0x301a, 0xf2},
	{0x301b, 0x96},
	{0x301d, 0x02},
	{0x3022, 0x0e},
	{0x3023, 0xb0},
	{0x3028, 0xc3},
	{0x3031, 0x68},
	{0x3036, 0x6c},
	{0x3400, 0x08},
	{0x3507, 0x02},
	{0x3508, 0x00},
	{0x3509, 0x12},
	{0x350a, 0x00},
	{0x350b, 0x40},
	{0x3549, 0x12},
	{0x354e, 0x00},
	{0x354f, 0x10},
	{0x3600, 0x12},
	{0x3603, 0xc0},
	{0x3604, 0x2c},
	{0x3606, 0x55},
	{0x3607, 0x05},
	{0x360a, 0x6d},
	{0x360d, 0x05},
	{0x360e, 0x07},
	{0x360f, 0x44},
	{0x3622, 0x79},
	{0x3623, 0x57},
	{0x3624, 0x50},
	{0x362b, 0x77},
	{0x3641, 0x01},
	{0x3660, 0xc0},
	{0x3661, 0x04},
	{0x3662, 0x00},
	{0x3663, 0x20},
	{0x3664, 0x08},
	{0x3667, 0x00},
	{0x366a, 0x54},
	{0x366c, 0x10},
	{0x3708, 0x42},
	{0x3709, 0x1c},
	{0x3718, 0x8c},
	{0x3719, 0x00},
	{0x371a, 0x04},
	{0x3725, 0xf0},
	{0x3728, 0x22},
	{0x372a, 0x20},
	{0x372b, 0x40},
	{0x3730, 0x00},
	{0x3731, 0x00},
	{0x3732, 0x00},
	{0x3733, 0x00},
	{0x3748, 0x00},
	{0x3767, 0x30},
	{0x3772, 0x00},
	{0x3773, 0x00},
	{0x3774, 0x40},
	{0x3775, 0x81},
	{0x3776, 0x31},
	{0x3777, 0x06},
	{0x3778, 0xa0},
	{0x377f, 0x31},
	{0x378d, 0x39},
	{0x3790, 0xcc},
	{0x3791, 0xcc},
	{0x379c, 0x02},
	{0x379d, 0x00},
	{0x379e, 0x00},
	{0x379f, 0x1e},
	{0x37a0, 0x22},
	{0x37a4, 0x00},
	{0x37b0, 0x48},
	{0x37b3, 0x62},
	{0x37b6, 0x22},
	{0x37b9, 0x23},
	{0x37c3, 0x07},
	{0x37c6, 0x35},
	{0x37c8, 0x00},
	{0x37c9, 0x00},
	{0x37cc, 0x93},
	{0x3810, 0x00},
	{0x3812, 0x00},
	{0x3837, 0x02},
	{0x3d85, 0x17},
	{0x3d8c, 0x79},
	{0x3d8d, 0x7f},
	{0x3f00, 0x50},
	{0x3f85, 0x00},
	{0x3f86, 0x00},
	{0x3f87, 0x05},
	{0x3f9e, 0x00},
	{0x3f9f, 0x00},
	{0x4000, 0x17},
	{0x4001, 0x60},
	{0x4003, 0x40},
	{0x4008, 0x00},
	{0x400f, 0x09},
	{0x4010, 0xf0},
	{0x4011, 0xf0},
	{0x4013, 0x02},
	{0x4018, 0x04},
	{0x4019, 0x04},
	{0x401a, 0x48},
	{0x4020, 0x08},
	{0x4022, 0x08},
	{0x4024, 0x08},
	{0x4026, 0x08},
	{0x4028, 0x08},
	{0x402a, 0x08},
	{0x402c, 0x08},
	{0x402e, 0x08},
	{0x4030, 0x08},
	{0x4032, 0x08},
	{0x4034, 0x08},
	{0x4036, 0x08},
	{0x4038, 0x08},
	{0x403a, 0x08},
	{0x403c, 0x08},
	{0x403e, 0x08},
	{0x4040, 0x00},
	{0x4041, 0x00},
	{0x4042, 0x00},
	{0x4043, 0x00},
	{0x4044, 0x00},
	{0x4045, 0x00},
	{0x4046, 0x00},
	{0x4047, 0x00},
	{0x40b0, 0x00},
	{0x40b1, 0x00},
	{0x40b2, 0x00},
	{0x40b3, 0x00},
	{0x40b4, 0x00},
	{0x40b5, 0x00},
	{0x40b6, 0x00},
	{0x40b7, 0x00},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x4202, 0x00},
	{0x4203, 0x00},
	{0x4066, 0x02},
	{0x4509, 0x07},
	{0x4605, 0x00},
	{0x4641, 0x1e},
	{0x4645, 0x00},
	{0x4800, 0x04},
	{0x4809, 0x2b},
	{0x4812, 0x2b},
	{0x4813, 0x90},
	{0x4833, 0x18},
	{0x484b, 0x01},
	{0x4850, 0x7c},
	{0x4852, 0x06},
	{0x4856, 0x58},
	{0x4857, 0xaa},
	{0x4862, 0x0a},
	{0x4867, 0x02},
	{0x4869, 0x18},
	{0x486a, 0x6a},
	{0x486e, 0x03},
	{0x486f, 0x55},
	{0x4875, 0xf0},
	{0x4b05, 0x93},
	{0x4b06, 0x00},
	{0x4c01, 0x5f},
	{0x4d00, 0x04},
	{0x4d01, 0x22},
	{0x4d02, 0xb7},
	{0x4d03, 0xca},
	{0x4d04, 0x30},
	{0x4d05, 0x1d},
	{0x4f00, 0x1c},
	{0x4f03, 0x50},
	{0x4f04, 0x01},
	{0x4f05, 0x7c},
	{0x4f08, 0x00},
	{0x4f09, 0x60},
	{0x4f0a, 0x00},
	{0x4f0b, 0x30},
	{0x4f14, 0xf0},
	{0x4f15, 0xf0},
	{0x4f16, 0xf0},
	{0x4f17, 0xf0},
	{0x4f18, 0xf0},
	{0x4f19, 0x00},
	{0x4f1a, 0x00},
	{0x4f20, 0x07},
	{0x4f21, 0xd0},
	{0x5000, 0x9b},
	{0x5001, 0x4e},
	{0x5002, 0x0c},
	{0x501d, 0x00},
	{0x5020, 0x03},
	{0x5022, 0x91},
	{0x5023, 0x00},
	{0x5030, 0x00},
	{0x5056, 0x00},
	{0x5063, 0x00},
	{0x5200, 0x02},
	{0x5202, 0x00},
	{0x5204, 0x00},
	{0x5209, 0x81},
	{0x520e, 0x31},
	{0x5280, 0x00},
	{0x5400, 0x01},
	{0x5401, 0x00},
	{0x5504, 0x00},
	{0x5505, 0x00},
	{0x5506, 0x00},
	{0x5507, 0x00},
	{0x550c, 0x00},
	{0x550d, 0x00},
	{0x550e, 0x00},
	{0x550f, 0x00},
	{0x5514, 0x00},
	{0x5515, 0x00},
	{0x5516, 0x00},
	{0x5517, 0x00},
	{0x551c, 0x00},
	{0x551d, 0x00},
	{0x551e, 0x00},
	{0x551f, 0x00},
	{0x5524, 0x00},
	{0x5525, 0x00},
	{0x5526, 0x00},
	{0x5527, 0x00},
	{0x552c, 0x00},
	{0x552d, 0x00},
	{0x552e, 0x00},
	{0x552f, 0x00},
	{0x5534, 0x00},
	{0x5535, 0x00},
	{0x5536, 0x00},
	{0x5537, 0x00},
	{0x553c, 0x00},
	{0x553d, 0x00},
	{0x553e, 0x00},
	{0x553f, 0x00},
	{0x5544, 0x00},
	{0x5545, 0x00},
	{0x5546, 0x00},
	{0x5547, 0x00},
	{0x554c, 0x00},
	{0x554d, 0x00},
	{0x554e, 0x00},
	{0x554f, 0x00},
	{0x5554, 0x00},
	{0x5555, 0x00},
	{0x5556, 0x00},
	{0x5557, 0x00},
	{0x555c, 0x00},
	{0x555d, 0x00},
	{0x555e, 0x00},
	{0x555f, 0x00},
	{0x5564, 0x00},
	{0x5565, 0x00},
	{0x5566, 0x00},
	{0x5567, 0x00},
	{0x556c, 0x00},
	{0x556d, 0x00},
	{0x556e, 0x00},
	{0x556f, 0x00},
	{0x5574, 0x00},
	{0x5575, 0x00},
	{0x5576, 0x00},
	{0x5577, 0x00},
	{0x557c, 0x00},
	{0x557d, 0x00},
	{0x557e, 0x00},
	{0x557f, 0x00},
	{0x5584, 0x00},
	{0x5585, 0x00},
	{0x5586, 0x00},
	{0x5587, 0x00},
	{0x558c, 0x00},
	{0x558d, 0x00},
	{0x558e, 0x00},
	{0x558f, 0x00},
	{0x5594, 0x00},
	{0x5595, 0x00},
	{0x5596, 0x00},
	{0x5597, 0x00},
	{0x559c, 0x00},
	{0x559d, 0x00},
	{0x559e, 0x00},
	{0x559f, 0x00},
	{0x55a4, 0x00},
	{0x55a5, 0x00},
	{0x55a6, 0x00},
	{0x55a7, 0x00},
	{0x55ac, 0x00},
	{0x55ad, 0x00},
	{0x55ae, 0x00},
	{0x55af, 0x00},
	{0x55b4, 0x00},
	{0x55b5, 0x00},
	{0x55b6, 0x00},
	{0x55b7, 0x00},
	{0x55bc, 0x00},
	{0x55bd, 0x00},
	{0x55be, 0x00},
	{0x55bf, 0x00},
	{0x55c4, 0x00},
	{0x55c5, 0x00},
	{0x55c6, 0x00},
	{0x55c7, 0x00},
	{0x55cc, 0x00},
	{0x55cd, 0x00},
	{0x55ce, 0x00},
	{0x55cf, 0x00},
	{0x55d4, 0x00},
	{0x55d5, 0x00},
	{0x55d6, 0x00},
	{0x55d7, 0x00},
	{0x55dc, 0x00},
	{0x55dd, 0x00},
	{0x55de, 0x00},
	{0x55df, 0x00},
	{0x55e4, 0x00},
	{0x55e5, 0x00},
	{0x55e6, 0x00},
	{0x55e7, 0x00},
	{0x55ec, 0x00},
	{0x55ed, 0x00},
	{0x55ee, 0x00},
	{0x55ef, 0x00},
	{0x55f4, 0x00},
	{0x55f5, 0x00},
	{0x55f6, 0x00},
	{0x55f7, 0x00},
	{0x55fc, 0x00},
	{0x55fd, 0x00},
	{0x55fe, 0x00},
	{0x55ff, 0x00},
	{0x5600, 0x30},
	{0x560f, 0xfc},
	{0x5610, 0xf0},
	{0x5611, 0x10},
	{0x562f, 0xfc},
	{0x5630, 0xf0},
	{0x5631, 0x10},
	{0x564f, 0xfc},
	{0x5650, 0xf0},
	{0x5651, 0x10},
	{0x5670, 0xf0},
	{0x5671, 0x10},
	{0x5694, 0xc0},
	{0x5695, 0x00},
	{0x5696, 0x00},
	{0x5697, 0x08},
	{0x5698, 0x00},
	{0x5699, 0x70},
	{0x569a, 0x11},
	{0x569b, 0xf0},
	{0x569c, 0x00},
	{0x569d, 0x68},
	{0x569e, 0x0d},
	{0x569f, 0x68},
	{0x56a1, 0xff},
	{0x5bd9, 0x01},
	{0x5d04, 0x01},
	{0x5d05, 0x00},
	{0x5d06, 0x01},
	{0x5d07, 0x00},
	{0x5d12, 0x38},
	{0x5d13, 0x38},
	{0x5d14, 0x38},
	{0x5d15, 0x38},
	{0x5d16, 0x38},
	{0x5d17, 0x38},
	{0x5d18, 0x38},
	{0x5d19, 0x38},
	{0x5d1a, 0x38},
	{0x5d1b, 0x38},
	{0x5d1c, 0x00},
	{0x5d1d, 0x00},
	{0x5d1e, 0x00},
	{0x5d1f, 0x00},
	{0x5d20, 0x16},
	{0x5d21, 0x20},
	{0x5d22, 0x10},
	{0x5d23, 0xa0},
	{0x5d28, 0xc0},
	{0x5d29, 0x80},
	{0x5d2a, 0x00},
	{0x5d2b, 0x70},
	{0x5d2c, 0x11},
	{0x5d2d, 0xf0},
	{0x5d2e, 0x00},
	{0x5d2f, 0x68},
	{0x5d30, 0x0d},
	{0x5d31, 0x68},
	{0x5d32, 0x00},
	{0x5d38, 0x70},
	{0x5c80, 0x06},
	{0x5c81, 0x90},
	{0x5c82, 0x09},
	{0x5c83, 0x5f},
	{0x5c85, 0x6c},
	{0x5601, 0x04},
	{0x5602, 0x02},
	{0x5603, 0x01},
	{0x5604, 0x04},
	{0x5605, 0x02},
	{0x5606, 0x01},
	{0x5b80, 0x00},
	{0x5b81, 0x04},
	{0x5b82, 0x00},
	{0x5b83, 0x08},
	{0x5b84, 0x00},
	{0x5b85, 0x0c},
	{0x5b86, 0x00},
	{0x5b87, 0x16},
	{0x5b88, 0x00},
	{0x5b89, 0x28},
	{0x5b8a, 0x00},
	{0x5b8b, 0x40},
	{0x5b8c, 0x1a},
	{0x5b8d, 0x1a},
	{0x5b8e, 0x1a},
	{0x5b8f, 0x1a},
	{0x5b90, 0x1a},
	{0x5b91, 0x1a},
	{0x5b92, 0x1a},
	{0x5b93, 0x1a},
	{0x5b94, 0x1a},
	{0x5b95, 0x1a},
	{0x5b96, 0x1a},
	{0x5b97, 0x1a},
	{0x2800, 0x60},
	{0x2801, 0x4c},
	{0x2802, 0x45},
	{0x2803, 0x41},
	{0x2804, 0x3b},
	{0x2805, 0x3a},
	{0x2806, 0x39},
	{0x2807, 0x37},
	{0x2808, 0x36},
	{0x2809, 0x38},
	{0x280a, 0x3b},
	{0x280b, 0x3d},
	{0x280c, 0x43},
	{0x280d, 0x4a},
	{0x280e, 0x52},
	{0x280f, 0x61},
	{0x2810, 0x47},
	{0x2811, 0x37},
	{0x2812, 0x33},
	{0x2813, 0x2e},
	{0x2814, 0x2b},
	{0x2815, 0x27},
	{0x2816, 0x26},
	{0x2817, 0x26},
	{0x2818, 0x26},
	{0x2819, 0x27},
	{0x281a, 0x29},
	{0x281b, 0x2c},
	{0x281c, 0x30},
	{0x281d, 0x35},
	{0x281e, 0x3b},
	{0x281f, 0x4c},
	{0x2820, 0x3a},
	{0x2821, 0x2d},
	{0x2822, 0x29},
	{0x2823, 0x24},
	{0x2824, 0x20},
	{0x2825, 0x1d},
	{0x2826, 0x1b},
	{0x2827, 0x1a},
	{0x2828, 0x1b},
	{0x2829, 0x1c},
	{0x282a, 0x1e},
	{0x282b, 0x21},
	{0x282c, 0x26},
	{0x282d, 0x2b},
	{0x282e, 0x30},
	{0x282f, 0x3f},
	{0x2830, 0x2f},
	{0x2831, 0x24},
	{0x2832, 0x20},
	{0x2833, 0x1a},
	{0x2834, 0x16},
	{0x2835, 0x14},
	{0x2836, 0x12},
	{0x2837, 0x11},
	{0x2838, 0x11},
	{0x2839, 0x13},
	{0x283a, 0x15},
	{0x283b, 0x18},
	{0x283c, 0x1c},
	{0x283d, 0x22},
	{0x283e, 0x27},
	{0x283f, 0x34},
	{0x2840, 0x28},
	{0x2841, 0x1d},
	{0x2842, 0x18},
	{0x2843, 0x13},
	{0x2844, 0x0f},
	{0x2845, 0x0d},
	{0x2846, 0x0b},
	{0x2847, 0x0a},
	{0x2848, 0x0a},
	{0x2849, 0x0b},
	{0x284a, 0x0e},
	{0x284b, 0x11},
	{0x284c, 0x15},
	{0x284d, 0x1a},
	{0x284e, 0x20},
	{0x284f, 0x2c},
	{0x2850, 0x21},
	{0x2851, 0x17},
	{0x2852, 0x12},
	{0x2853, 0x0e},
	{0x2854, 0x0b},
	{0x2855, 0x08},
	{0x2856, 0x06},
	{0x2857, 0x05},
	{0x2858, 0x05},
	{0x2859, 0x07},
	{0x285a, 0x09},
	{0x285b, 0x0c},
	{0x285c, 0x10},
	{0x285d, 0x15},
	{0x285e, 0x1a},
	{0x285f, 0x26},
	{0x2860, 0x1d},
	{0x2861, 0x14},
	{0x2862, 0x10},
	{0x2863, 0x0b},
	{0x2864, 0x07},
	{0x2865, 0x05},
	{0x2866, 0x03},
	{0x2867, 0x02},
	{0x2868, 0x02},
	{0x2869, 0x03},
	{0x286a, 0x05},
	{0x286b, 0x08},
	{0x286c, 0x0c},
	{0x286d, 0x12},
	{0x286e, 0x17},
	{0x286f, 0x22},
	{0x2870, 0x1c},
	{0x2871, 0x13},
	{0x2872, 0x0e},
	{0x2873, 0x09},
	{0x2874, 0x06},
	{0x2875, 0x03},
	{0x2876, 0x01},
	{0x2877, 0x01},
	{0x2878, 0x01},
	{0x2879, 0x02},
	{0x287a, 0x04},
	{0x287b, 0x06},
	{0x287c, 0x0a},
	{0x287d, 0x10},
	{0x287e, 0x15},
	{0x287f, 0x21},
	{0x2880, 0x1c},
	{0x2881, 0x13},
	{0x2882, 0x0e},
	{0x2883, 0x09},
	{0x2884, 0x06},
	{0x2885, 0x03},
	{0x2886, 0x01},
	{0x2887, 0x01},
	{0x2888, 0x01},
	{0x2889, 0x02},
	{0x288a, 0x03},
	{0x288b, 0x06},
	{0x288c, 0x0a},
	{0x288d, 0x0f},
	{0x288e, 0x15},
	{0x288f, 0x20},
	{0x2890, 0x1e},
	{0x2891, 0x15},
	{0x2892, 0x10},
	{0x2893, 0x0b},
	{0x2894, 0x08},
	{0x2895, 0x05},
	{0x2896, 0x03},
	{0x2897, 0x02},
	{0x2898, 0x03},
	{0x2899, 0x04},
	{0x289a, 0x06},
	{0x289b, 0x08},
	{0x289c, 0x0c},
	{0x289d, 0x11},
	{0x289e, 0x16},
	{0x289f, 0x21},
	{0x28a0, 0x22},
	{0x28a1, 0x18},
	{0x28a2, 0x13},
	{0x28a3, 0x0f},
	{0x28a4, 0x0b},
	{0x28a5, 0x09},
	{0x28a6, 0x07},
	{0x28a7, 0x06},
	{0x28a8, 0x06},
	{0x28a9, 0x07},
	{0x28aa, 0x09},
	{0x28ab, 0x0c},
	{0x28ac, 0x10},
	{0x28ad, 0x15},
	{0x28ae, 0x1b},
	{0x28af, 0x26},
	{0x28b0, 0x29},
	{0x28b1, 0x1e},
	{0x28b2, 0x19},
	{0x28b3, 0x14},
	{0x28b4, 0x11},
	{0x28b5, 0x0e},
	{0x28b6, 0x0c},
	{0x28b7, 0x0b},
	{0x28b8, 0x0b},
	{0x28b9, 0x0c},
	{0x28ba, 0x0e},
	{0x28bb, 0x11},
	{0x28bc, 0x16},
	{0x28bd, 0x1b},
	{0x28be, 0x20},
	{0x28bf, 0x2d},
	{0x28c0, 0x30},
	{0x28c1, 0x26},
	{0x28c2, 0x21},
	{0x28c3, 0x1b},
	{0x28c4, 0x18},
	{0x28c5, 0x15},
	{0x28c6, 0x13},
	{0x28c7, 0x12},
	{0x28c8, 0x12},
	{0x28c9, 0x13},
	{0x28ca, 0x16},
	{0x28cb, 0x18},
	{0x28cc, 0x1d},
	{0x28cd, 0x23},
	{0x28ce, 0x28},
	{0x28cf, 0x33},
	{0x28d0, 0x3c},
	{0x28d1, 0x2f},
	{0x28d2, 0x2a},
	{0x28d3, 0x26},
	{0x28d4, 0x22},
	{0x28d5, 0x1e},
	{0x28d6, 0x1c},
	{0x28d7, 0x1c},
	{0x28d8, 0x1c},
	{0x28d9, 0x1d},
	{0x28da, 0x1f},
	{0x28db, 0x22},
	{0x28dc, 0x27},
	{0x28dd, 0x2c},
	{0x28de, 0x31},
	{0x28df, 0x40},
	{0x28e0, 0x4a},
	{0x28e1, 0x3a},
	{0x28e2, 0x34},
	{0x28e3, 0x2f},
	{0x28e4, 0x2c},
	{0x28e5, 0x28},
	{0x28e6, 0x27},
	{0x28e7, 0x26},
	{0x28e8, 0x27},
	{0x28e9, 0x27},
	{0x28ea, 0x29},
	{0x28eb, 0x2d},
	{0x28ec, 0x31},
	{0x28ed, 0x37},
	{0x28ee, 0x3d},
	{0x28ef, 0x4e},
	{0x28f0, 0x5c},
	{0x28f1, 0x4d},
	{0x28f2, 0x46},
	{0x28f3, 0x3d},
	{0x28f4, 0x3a},
	{0x28f5, 0x36},
	{0x28f6, 0x35},
	{0x28f7, 0x33},
	{0x28f8, 0x34},
	{0x28f9, 0x35},
	{0x28fa, 0x37},
	{0x28fb, 0x3a},
	{0x28fc, 0x40},
	{0x28fd, 0x48},
	{0x28fe, 0x50},
	{0x28ff, 0x64},
	{0x2900, 0x74},
	{0x2901, 0x7d},
	{0x2902, 0x7c},
	{0x2903, 0x7b},
	{0x2904, 0x7c},
	{0x2905, 0x7b},
	{0x2906, 0x7b},
	{0x2907, 0x79},
	{0x2908, 0x7c},
	{0x2909, 0x7a},
	{0x290a, 0x79},
	{0x290b, 0x7b},
	{0x290c, 0x7a},
	{0x290d, 0x7b},
	{0x290e, 0x79},
	{0x290f, 0x7d},
	{0x2910, 0x7f},
	{0x2911, 0x7d},
	{0x2912, 0x7d},
	{0x2913, 0x7d},
	{0x2914, 0x7c},
	{0x2915, 0x7c},
	{0x2916, 0x7c},
	{0x2917, 0x7c},
	{0x2918, 0x7b},
	{0x2919, 0x7c},
	{0x291a, 0x7c},
	{0x291b, 0x7d},
	{0x291c, 0x7d},
	{0x291d, 0x7c},
	{0x291e, 0x7d},
	{0x291f, 0x7c},
	{0x2920, 0x7e},
	{0x2921, 0x7d},
	{0x2922, 0x7c},
	{0x2923, 0x7d},
	{0x2924, 0x7d},
	{0x2925, 0x7d},
	{0x2926, 0x7d},
	{0x2927, 0x7c},
	{0x2928, 0x7d},
	{0x2929, 0x7d},
	{0x292a, 0x7d},
	{0x292b, 0x7d},
	{0x292c, 0x7d},
	{0x292d, 0x7d},
	{0x292e, 0x7d},
	{0x292f, 0x7f},
	{0x2930, 0x80},
	{0x2931, 0x7d},
	{0x2932, 0x7c},
	{0x2933, 0x7d},
	{0x2934, 0x7d},
	{0x2935, 0x7e},
	{0x2936, 0x7e},
	{0x2937, 0x7e},
	{0x2938, 0x7e},
	{0x2939, 0x7e},
	{0x293a, 0x7e},
	{0x293b, 0x7e},
	{0x293c, 0x7d},
	{0x293d, 0x7d},
	{0x293e, 0x7d},
	{0x293f, 0x80},
	{0x2940, 0x80},
	{0x2941, 0x7d},
	{0x2942, 0x7d},
	{0x2943, 0x7d},
	{0x2944, 0x7f},
	{0x2945, 0x7f},
	{0x2946, 0x80},
	{0x2947, 0x80},
	{0x2948, 0x80},
	{0x2949, 0x80},
	{0x294a, 0x7f},
	{0x294b, 0x7f},
	{0x294c, 0x7e},
	{0x294d, 0x7e},
	{0x294e, 0x7e},
	{0x294f, 0x80},
	{0x2950, 0x7e},
	{0x2951, 0x7d},
	{0x2952, 0x7d},
	{0x2953, 0x7e},
	{0x2954, 0x7f},
	{0x2955, 0x80},
	{0x2956, 0x81},
	{0x2957, 0x81},
	{0x2958, 0x81},
	{0x2959, 0x81},
	{0x295a, 0x81},
	{0x295b, 0x80},
	{0x295c, 0x7f},
	{0x295d, 0x7e},
	{0x295e, 0x7e},
	{0x295f, 0x81},
	{0x2960, 0x80},
	{0x2961, 0x7c},
	{0x2962, 0x7c},
	{0x2963, 0x7e},
	{0x2964, 0x7f},
	{0x2965, 0x80},
	{0x2966, 0x81},
	{0x2967, 0x81},
	{0x2968, 0x81},
	{0x2969, 0x82},
	{0x296a, 0x81},
	{0x296b, 0x81},
	{0x296c, 0x80},
	{0x296d, 0x7e},
	{0x296e, 0x7e},
	{0x296f, 0x80},
	{0x2970, 0x7e},
	{0x2971, 0x7c},
	{0x2972, 0x7c},
	{0x2973, 0x7e},
	{0x2974, 0x7f},
	{0x2975, 0x80},
	{0x2976, 0x81},
	{0x2977, 0x80},
	{0x2978, 0x81},
	{0x2979, 0x81},
	{0x297a, 0x81},
	{0x297b, 0x81},
	{0x297c, 0x80},
	{0x297d, 0x7e},
	{0x297e, 0x7e},
	{0x297f, 0x81},
	{0x2980, 0x7f},
	{0x2981, 0x7b},
	{0x2982, 0x7c},
	{0x2983, 0x7d},
	{0x2984, 0x7f},
	{0x2985, 0x80},
	{0x2986, 0x81},
	{0x2987, 0x81},
	{0x2988, 0x81},
	{0x2989, 0x81},
	{0x298a, 0x81},
	{0x298b, 0x81},
	{0x298c, 0x80},
	{0x298d, 0x7e},
	{0x298e, 0x7e},
	{0x298f, 0x81},
	{0x2990, 0x7e},
	{0x2991, 0x7c},
	{0x2992, 0x7c},
	{0x2993, 0x7d},
	{0x2994, 0x7f},
	{0x2995, 0x7f},
	{0x2996, 0x80},
	{0x2997, 0x81},
	{0x2998, 0x81},
	{0x2999, 0x81},
	{0x299a, 0x81},
	{0x299b, 0x80},
	{0x299c, 0x80},
	{0x299d, 0x7e},
	{0x299e, 0x7f},
	{0x299f, 0x81},
	{0x29a0, 0x81},
	{0x29a1, 0x7b},
	{0x29a2, 0x7c},
	{0x29a3, 0x7d},
	{0x29a4, 0x7e},
	{0x29a5, 0x7e},
	{0x29a6, 0x7f},
	{0x29a7, 0x80},
	{0x29a8, 0x80},
	{0x29a9, 0x80},
	{0x29aa, 0x80},
	{0x29ab, 0x80},
	{0x29ac, 0x7f},
	{0x29ad, 0x7e},
	{0x29ae, 0x7e},
	{0x29af, 0x81},
	{0x29b0, 0x7e},
	{0x29b1, 0x7c},
	{0x29b2, 0x7b},
	{0x29b3, 0x7c},
	{0x29b4, 0x7d},
	{0x29b5, 0x7d},
	{0x29b6, 0x7d},
	{0x29b7, 0x7e},
	{0x29b8, 0x7e},
	{0x29b9, 0x7f},
	{0x29ba, 0x7e},
	{0x29bb, 0x7e},
	{0x29bc, 0x7e},
	{0x29bd, 0x7d},
	{0x29be, 0x7e},
	{0x29bf, 0x81},
	{0x29c0, 0x81},
	{0x29c1, 0x7c},
	{0x29c2, 0x7c},
	{0x29c3, 0x7c},
	{0x29c4, 0x7c},
	{0x29c5, 0x7c},
	{0x29c6, 0x7c},
	{0x29c7, 0x7c},
	{0x29c8, 0x7d},
	{0x29c9, 0x7d},
	{0x29ca, 0x7d},
	{0x29cb, 0x7d},
	{0x29cc, 0x7d},
	{0x29cd, 0x7d},
	{0x29ce, 0x7e},
	{0x29cf, 0x82},
	{0x29d0, 0x80},
	{0x29d1, 0x7d},
	{0x29d2, 0x7c},
	{0x29d3, 0x7c},
	{0x29d4, 0x7c},
	{0x29d5, 0x7c},
	{0x29d6, 0x7c},
	{0x29d7, 0x7c},
	{0x29d8, 0x7c},
	{0x29d9, 0x7d},
	{0x29da, 0x7d},
	{0x29db, 0x7e},
	{0x29dc, 0x7e},
	{0x29dd, 0x7e},
	{0x29de, 0x7f},
	{0x29df, 0x81},
	{0x29e0, 0x82},
	{0x29e1, 0x7d},
	{0x29e2, 0x7b},
	{0x29e3, 0x7c},
	{0x29e4, 0x7b},
	{0x29e5, 0x7b},
	{0x29e6, 0x7b},
	{0x29e7, 0x7a},
	{0x29e8, 0x7a},
	{0x29e9, 0x7b},
	{0x29ea, 0x7c},
	{0x29eb, 0x7c},
	{0x29ec, 0x7d},
	{0x29ed, 0x7d},
	{0x29ee, 0x7f},
	{0x29ef, 0x82},
	{0x29f0, 0x7e},
	{0x29f1, 0x86},
	{0x29f2, 0x86},
	{0x29f3, 0x84},
	{0x29f4, 0x85},
	{0x29f5, 0x85},
	{0x29f6, 0x83},
	{0x29f7, 0x84},
	{0x29f8, 0x84},
	{0x29f9, 0x85},
	{0x29fa, 0x85},
	{0x29fb, 0x87},
	{0x29fc, 0x87},
	{0x29fd, 0x88},
	{0x29fe, 0x88},
	{0x29ff, 0x80},
	{0x2a00, 0x7f},
	{0x2a01, 0x85},
	{0x2a02, 0x83},
	{0x2a03, 0x82},
	{0x2a04, 0x83},
	{0x2a05, 0x81},
	{0x2a06, 0x82},
	{0x2a07, 0x81},
	{0x2a08, 0x82},
	{0x2a09, 0x81},
	{0x2a0a, 0x83},
	{0x2a0b, 0x82},
	{0x2a0c, 0x83},
	{0x2a0d, 0x83},
	{0x2a0e, 0x83},
	{0x2a0f, 0x82},
	{0x2a10, 0x82},
	{0x2a11, 0x86},
	{0x2a12, 0x85},
	{0x2a13, 0x85},
	{0x2a14, 0x84},
	{0x2a15, 0x84},
	{0x2a16, 0x83},
	{0x2a17, 0x83},
	{0x2a18, 0x82},
	{0x2a19, 0x83},
	{0x2a1a, 0x83},
	{0x2a1b, 0x84},
	{0x2a1c, 0x84},
	{0x2a1d, 0x84},
	{0x2a1e, 0x86},
	{0x2a1f, 0x83},
	{0x2a20, 0x82},
	{0x2a21, 0x84},
	{0x2a22, 0x83},
	{0x2a23, 0x82},
	{0x2a24, 0x82},
	{0x2a25, 0x81},
	{0x2a26, 0x81},
	{0x2a27, 0x81},
	{0x2a28, 0x81},
	{0x2a29, 0x81},
	{0x2a2a, 0x81},
	{0x2a2b, 0x82},
	{0x2a2c, 0x82},
	{0x2a2d, 0x83},
	{0x2a2e, 0x84},
	{0x2a2f, 0x81},
	{0x2a30, 0x81},
	{0x2a31, 0x83},
	{0x2a32, 0x81},
	{0x2a33, 0x81},
	{0x2a34, 0x80},
	{0x2a35, 0x80},
	{0x2a36, 0x80},
	{0x2a37, 0x80},
	{0x2a38, 0x7f},
	{0x2a39, 0x80},
	{0x2a3a, 0x80},
	{0x2a3b, 0x80},
	{0x2a3c, 0x80},
	{0x2a3d, 0x81},
	{0x2a3e, 0x83},
	{0x2a3f, 0x80},
	{0x2a40, 0x80},
	{0x2a41, 0x81},
	{0x2a42, 0x81},
	{0x2a43, 0x7f},
	{0x2a44, 0x80},
	{0x2a45, 0x7f},
	{0x2a46, 0x80},
	{0x2a47, 0x7f},
	{0x2a48, 0x80},
	{0x2a49, 0x7f},
	{0x2a4a, 0x80},
	{0x2a4b, 0x7f},
	{0x2a4c, 0x80},
	{0x2a4d, 0x80},
	{0x2a4e, 0x82},
	{0x2a4f, 0x7f},
	{0x2a50, 0x7f},
	{0x2a51, 0x82},
	{0x2a52, 0x80},
	{0x2a53, 0x80},
	{0x2a54, 0x7f},
	{0x2a55, 0x80},
	{0x2a56, 0x80},
	{0x2a57, 0x80},
	{0x2a58, 0x80},
	{0x2a59, 0x80},
	{0x2a5a, 0x80},
	{0x2a5b, 0x7f},
	{0x2a5c, 0x7f},
	{0x2a5d, 0x80},
	{0x2a5e, 0x81},
	{0x2a5f, 0x7f},
	{0x2a60, 0x7f},
	{0x2a61, 0x81},
	{0x2a62, 0x80},
	{0x2a63, 0x7f},
	{0x2a64, 0x80},
	{0x2a65, 0x80},
	{0x2a66, 0x80},
	{0x2a67, 0x81},
	{0x2a68, 0x80},
	{0x2a69, 0x81},
	{0x2a6a, 0x80},
	{0x2a6b, 0x80},
	{0x2a6c, 0x80},
	{0x2a6d, 0x80},
	{0x2a6e, 0x81},
	{0x2a6f, 0x7e},
	{0x2a70, 0x7e},
	{0x2a71, 0x80},
	{0x2a72, 0x7f},
	{0x2a73, 0x80},
	{0x2a74, 0x80},
	{0x2a75, 0x81},
	{0x2a76, 0x81},
	{0x2a77, 0x80},
	{0x2a78, 0x80},
	{0x2a79, 0x81},
	{0x2a7a, 0x80},
	{0x2a7b, 0x7f},
	{0x2a7c, 0x7f},
	{0x2a7d, 0x7f},
	{0x2a7e, 0x81},
	{0x2a7f, 0x7e},
	{0x2a80, 0x7e},
	{0x2a81, 0x81},
	{0x2a82, 0x7f},
	{0x2a83, 0x80},
	{0x2a84, 0x80},
	{0x2a85, 0x81},
	{0x2a86, 0x81},
	{0x2a87, 0x81},
	{0x2a88, 0x80},
	{0x2a89, 0x81},
	{0x2a8a, 0x80},
	{0x2a8b, 0x80},
	{0x2a8c, 0x7f},
	{0x2a8d, 0x7f},
	{0x2a8e, 0x80},
	{0x2a8f, 0x7e},
	{0x2a90, 0x7f},
	{0x2a91, 0x81},
	{0x2a92, 0x80},
	{0x2a93, 0x80},
	{0x2a94, 0x80},
	{0x2a95, 0x80},
	{0x2a96, 0x81},
	{0x2a97, 0x81},
	{0x2a98, 0x81},
	{0x2a99, 0x81},
	{0x2a9a, 0x81},
	{0x2a9b, 0x80},
	{0x2a9c, 0x80},
	{0x2a9d, 0x80},
	{0x2a9e, 0x81},
	{0x2a9f, 0x7e},
	{0x2aa0, 0x7f},
	{0x2aa1, 0x81},
	{0x2aa2, 0x80},
	{0x2aa3, 0x80},
	{0x2aa4, 0x80},
	{0x2aa5, 0x80},
	{0x2aa6, 0x80},
	{0x2aa7, 0x80},
	{0x2aa8, 0x80},
	{0x2aa9, 0x80},
	{0x2aaa, 0x80},
	{0x2aab, 0x80},
	{0x2aac, 0x7f},
	{0x2aad, 0x80},
	{0x2aae, 0x81},
	{0x2aaf, 0x7e},
	{0x2ab0, 0x80},
	{0x2ab1, 0x82},
	{0x2ab2, 0x81},
	{0x2ab3, 0x80},
	{0x2ab4, 0x80},
	{0x2ab5, 0x80},
	{0x2ab6, 0x7f},
	{0x2ab7, 0x7f},
	{0x2ab8, 0x7f},
	{0x2ab9, 0x7f},
	{0x2aba, 0x7f},
	{0x2abb, 0x7f},
	{0x2abc, 0x80},
	{0x2abd, 0x80},
	{0x2abe, 0x81},
	{0x2abf, 0x7f},
	{0x2ac0, 0x81},
	{0x2ac1, 0x83},
	{0x2ac2, 0x82},
	{0x2ac3, 0x81},
	{0x2ac4, 0x80},
	{0x2ac5, 0x80},
	{0x2ac6, 0x80},
	{0x2ac7, 0x80},
	{0x2ac8, 0x7f},
	{0x2ac9, 0x80},
	{0x2aca, 0x80},
	{0x2acb, 0x80},
	{0x2acc, 0x80},
	{0x2acd, 0x81},
	{0x2ace, 0x82},
	{0x2acf, 0x80},
	{0x2ad0, 0x83},
	{0x2ad1, 0x85},
	{0x2ad2, 0x83},
	{0x2ad3, 0x82},
	{0x2ad4, 0x82},
	{0x2ad5, 0x81},
	{0x2ad6, 0x81},
	{0x2ad7, 0x80},
	{0x2ad8, 0x80},
	{0x2ad9, 0x81},
	{0x2ada, 0x81},
	{0x2adb, 0x81},
	{0x2adc, 0x82},
	{0x2add, 0x82},
	{0x2ade, 0x83},
	{0x2adf, 0x81},
	{0x2ae0, 0x83},
	{0x2ae1, 0x87},
	{0x2ae2, 0x85},
	{0x2ae3, 0x85},
	{0x2ae4, 0x84},
	{0x2ae5, 0x83},
	{0x2ae6, 0x83},
	{0x2ae7, 0x83},
	{0x2ae8, 0x82},
	{0x2ae9, 0x83},
	{0x2aea, 0x83},
	{0x2aeb, 0x84},
	{0x2aec, 0x84},
	{0x2aed, 0x84},
	{0x2aee, 0x85},
	{0x2aef, 0x82},
	{0x2af0, 0x84},
	{0x2af1, 0x86},
	{0x2af2, 0x86},
	{0x2af3, 0x86},
	{0x2af4, 0x84},
	{0x2af5, 0x87},
	{0x2af6, 0x84},
	{0x2af7, 0x84},
	{0x2af8, 0x84},
	{0x2af9, 0x84},
	{0x2afa, 0x85},
	{0x2afb, 0x84},
	{0x2afc, 0x85},
	{0x2afd, 0x85},
	{0x2afe, 0x86},
	{0x2aff, 0x81},
	{0x2b05, 0x03},
	{0x2b06, 0x0c},
	{0x2b07, 0x02},
	{0x2b08, 0x0b},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * resolution = 4672*3504
 * bayer pattern = BGGR"
 */
static const struct regval ov16880_4672x3504_30fps_regs[] = {
	{0x0302, 0x3c},
	{0x030f, 0x03},
	{0x3501, 0x0e},
	{0x3502, 0xea},
	{0x3726, 0x00},
	{0x3727, 0x22},
	{0x3729, 0x00},
	{0x372f, 0x92},
	{0x3760, 0x92},
	{0x37a1, 0x02},
	{0x37a5, 0x96},
	{0x37a2, 0x04},
	{0x37a3, 0x23},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x08},
	{0x3804, 0x12},
	{0x3805, 0x5f},
	{0x3806, 0x0d},
	{0x3807, 0xc7},
	{0x3808, 0x12},
	{0x3809, 0x40},
	{0x380a, 0x0d},
	{0x380b, 0xb0},
	{0x380c, 0x13},
	{0x380d, 0xa0},
	{0x380e, 0x0e},
	{0x380f, 0xf0},
	{0x3811, 0x10},
	{0x3813, 0x08},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x80},
	{0x3821, 0x04},
	{0x3836, 0x14},
	{0x3841, 0x04},
	{0x4006, 0x00},
	{0x4007, 0x00},
	{0x4009, 0x0f},
	{0x4050, 0x04},
	{0x4051, 0x0f},
	{0x4501, 0x38},
	{0x4837, 0x0b},
	{0x5201, 0x71},
	{0x5203, 0x10},
	{0x5205, 0x90},
	{0x5500, 0x00},
	{0x5501, 0x00},
	{0x5502, 0x00},
	{0x5503, 0x00},
	{0x5508, 0x20},
	{0x5509, 0x00},
	{0x550a, 0x20},
	{0x550b, 0x00},
	{0x5510, 0x00},
	{0x5511, 0x00},
	{0x5512, 0x00},
	{0x5513, 0x00},
	{0x5518, 0x20},
	{0x5519, 0x00},
	{0x551a, 0x20},
	{0x551b, 0x00},
	{0x5520, 0x00},
	{0x5521, 0x00},
	{0x5522, 0x00},
	{0x5523, 0x00},
	{0x5528, 0x00},
	{0x5529, 0x20},
	{0x552a, 0x00},
	{0x552b, 0x20},
	{0x5530, 0x00},
	{0x5531, 0x00},
	{0x5532, 0x00},
	{0x5533, 0x00},
	{0x5538, 0x00},
	{0x5539, 0x20},
	{0x553a, 0x00},
	{0x553b, 0x20},
	{0x5540, 0x00},
	{0x5541, 0x00},
	{0x5542, 0x00},
	{0x5543, 0x00},
	{0x5548, 0x20},
	{0x5549, 0x00},
	{0x554a, 0x20},
	{0x554b, 0x00},
	{0x5550, 0x00},
	{0x5551, 0x00},
	{0x5552, 0x00},
	{0x5553, 0x00},
	{0x5558, 0x20},
	{0x5559, 0x00},
	{0x555a, 0x20},
	{0x555b, 0x00},
	{0x5560, 0x00},
	{0x5561, 0x00},
	{0x5562, 0x00},
	{0x5563, 0x00},
	{0x5568, 0x00},
	{0x5569, 0x20},
	{0x556a, 0x00},
	{0x556b, 0x20},
	{0x5570, 0x00},
	{0x5571, 0x00},
	{0x5572, 0x00},
	{0x5573, 0x00},
	{0x5578, 0x00},
	{0x5579, 0x20},
	{0x557a, 0x00},
	{0x557b, 0x20},
	{0x5580, 0x00},
	{0x5581, 0x00},
	{0x5582, 0x00},
	{0x5583, 0x00},
	{0x5588, 0x20},
	{0x5589, 0x00},
	{0x558a, 0x20},
	{0x558b, 0x00},
	{0x5590, 0x00},
	{0x5591, 0x00},
	{0x5592, 0x00},
	{0x5593, 0x00},
	{0x5598, 0x20},
	{0x5599, 0x00},
	{0x559a, 0x20},
	{0x559b, 0x00},
	{0x55a0, 0x00},
	{0x55a1, 0x00},
	{0x55a2, 0x00},
	{0x55a3, 0x00},
	{0x55a8, 0x00},
	{0x55a9, 0x20},
	{0x55aa, 0x00},
	{0x55ab, 0x20},
	{0x55b0, 0x00},
	{0x55b1, 0x00},
	{0x55b2, 0x00},
	{0x55b3, 0x00},
	{0x55b8, 0x00},
	{0x55b9, 0x20},
	{0x55ba, 0x00},
	{0x55bb, 0x20},
	{0x55c0, 0x00},
	{0x55c1, 0x00},
	{0x55c2, 0x00},
	{0x55c3, 0x00},
	{0x55c8, 0x20},
	{0x55c9, 0x00},
	{0x55ca, 0x20},
	{0x55cb, 0x00},
	{0x55d0, 0x00},
	{0x55d1, 0x00},
	{0x55d2, 0x00},
	{0x55d3, 0x00},
	{0x55d8, 0x20},
	{0x55d9, 0x00},
	{0x55da, 0x20},
	{0x55db, 0x00},
	{0x55e0, 0x00},
	{0x55e1, 0x00},
	{0x55e2, 0x00},
	{0x55e3, 0x00},
	{0x55e8, 0x00},
	{0x55e9, 0x20},
	{0x55ea, 0x00},
	{0x55eb, 0x20},
	{0x55f0, 0x00},
	{0x55f1, 0x00},
	{0x55f2, 0x00},
	{0x55f3, 0x00},
	{0x55f8, 0x00},
	{0x55f9, 0x20},
	{0x55fa, 0x00},
	{0x55fb, 0x20},
	{0x5690, 0x00},
	{0x5691, 0x00},
	{0x5692, 0x00},
	{0x5693, 0x08},
	{0x5d24, 0x00},
	{0x5d25, 0x00},
	{0x5d26, 0x00},
	{0x5d27, 0x08},
	{0x5d3a, 0x58},
#ifdef IMAGE_NORMAL_MIRROR
	{0x3820, 0x80},
	{0x3821, 0x00},
	{0x4000, 0x17},
#endif
#ifdef IMAGE_H_MIRROR
	{0x3820, 0x80},
	{0x3821, 0x04},
	{0x4000, 0x17},
#endif
#ifdef IMAGE_V_MIRROR
	{0x3820, 0xc4},
	{0x3821, 0x00},
	{0x4000, 0x57},
#endif
#ifdef IMAGE_HV_MIRROR
	{0x3820, 0xc4},
	{0x3821, 0x04},
	{0x4000, 0x57},
#endif
	{0x5000, 0x9b},
	{0x5002, 0x0c},
	{0x5b85, 0x0c},
	{0x5b87, 0x16},
	{0x5b89, 0x28},
	{0x5b8b, 0x40},
	{0x5b8f, 0x1a},
	{0x5b90, 0x1a},
	{0x5b91, 0x1a},
	{0x5b95, 0x1a},
	{0x5b96, 0x1a},
	{0x5b97, 0x1a},
	{0x5001, 0x4e},
	{0x3018, 0xda},
	{0x366c, 0x10},
	{0x3641, 0x01},
	{0x5d29, 0x80},
	{0x030a, 0x00},
	{0x0311, 0x00},
	{0x3606, 0x55},
	{0x3607, 0x05},
	{0x360b, 0x48},
	{0x360c, 0x18},
	{0x3720, 0x88},
	//{0x0100, 0x01},
	{REG_NULL, 0x00},
};

static const struct ov16880_mode supported_modes[] = {
	{
		.width = 4672,
		.height = 3504,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0ee8,
		.hts_def = 0x13a0,
		.vts_def = 0x0ef0,
		.bpp = 10,
		.reg_list = ov16880_4672x3504_30fps_regs,
		.link_freq_idx = 0,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_items[] = {
	OV16880_LINK_FREQ_720MHZ,
};

static const char * const ov16880_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int ov16880_write_reg(struct i2c_client *client, u16 reg,
			     u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	dev_dbg(&client->dev, "write reg(0x%x val:0x%x)!\n", reg, val);

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

static int ov16880_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = ov16880_write_reg(client, regs[i].addr,
					OV16880_REG_VALUE_08BIT,
					regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int ov16880_read_reg(struct i2c_client *client, u16 reg,
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

static int ov16880_get_reso_dist(const struct ov16880_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct ov16880_mode *
ov16880_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = ov16880_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int ov16880_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct ov16880 *ov16880 = to_ov16880(sd);
	const struct ov16880_mode *mode;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	u32 lane_num = OV16880_LANES;

	mutex_lock(&ov16880->mutex);

	mode = ov16880_find_best_fit(fmt);
	fmt->format.code = OV16880_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&ov16880->mutex);
		return -ENOTTY;
#endif
	} else {
		ov16880->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(ov16880->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov16880->vblank, vblank_def,
					 OV16880_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(ov16880->vblank, vblank_def);
		pixel_rate = (u32)link_freq_items[mode->link_freq_idx] / mode->bpp * 2 * lane_num;

		__v4l2_ctrl_s_ctrl_int64(ov16880->pixel_rate,
					 pixel_rate);
		__v4l2_ctrl_s_ctrl(ov16880->link_freq,
				   mode->link_freq_idx);
	}
	dev_info(&ov16880->client->dev, "%s: mode->link_freq_idx(%d)",
		 __func__, mode->link_freq_idx);

	mutex_unlock(&ov16880->mutex);

	return 0;
}

static int ov16880_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct ov16880 *ov16880 = to_ov16880(sd);
	const struct ov16880_mode *mode = ov16880->cur_mode;

	mutex_lock(&ov16880->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&ov16880->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = OV16880_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&ov16880->mutex);

	return 0;
}

static int ov16880_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = OV16880_MEDIA_BUS_FMT;

	return 0;
}

static int ov16880_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct ov16880 *ov16880 = to_ov16880(sd);

	if (fse->index >= ov16880->cfg_num)
		return -EINVAL;

	if (fse->code != OV16880_MEDIA_BUS_FMT)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int ov16880_enable_test_pattern(struct ov16880 *ov16880, u32 pattern)
{
	u32 val;

	if (pattern)
		val = ((pattern - 1) << 4) | OV16880_TEST_PATTERN_ENABLE;
	else
		val = OV16880_TEST_PATTERN_DISABLE;

	return ov16880_write_reg(ov16880->client,
				 OV16880_REG_TEST_PATTERN,
				 OV16880_REG_VALUE_08BIT,
				 val);
}

static int ov16880_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct ov16880 *ov16880 = to_ov16880(sd);
	const struct ov16880_mode *mode = ov16880->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

#ifdef RK_OTP
static void ov16880_get_otp(struct otp_info *otp,
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

static void ov16880_get_module_inf(struct ov16880 *ov16880,
				   struct rkmodule_inf *inf)
{
#ifdef RK_OTP
	struct otp_info *otp = ov16880->otp;
#endif

	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, OV16880_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, ov16880->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, ov16880->len_name, sizeof(inf->base.lens));
#ifdef RK_OTP
	if (otp)
		ov16880_get_otp(otp, inf);
#endif
}

static long ov16880_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct ov16880 *ov16880 = to_ov16880(sd);
	struct rkmodule_hdr_cfg *hdr_cfg;
	long ret = 0;
	u32 i, h, w;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_SET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		w = ov16880->cur_mode->width;
		h = ov16880->cur_mode->height;
		for (i = 0; i < ov16880->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			h == supported_modes[i].height &&
			supported_modes[i].hdr_mode == hdr_cfg->hdr_mode) {
				ov16880->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == ov16880->cfg_num) {
			dev_err(&ov16880->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr_cfg->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = ov16880->cur_mode->hts_def - ov16880->cur_mode->width;
			h = ov16880->cur_mode->vts_def - ov16880->cur_mode->height;
			mutex_lock(&ov16880->mutex);
			__v4l2_ctrl_modify_range(ov16880->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(ov16880->vblank, h,
						 OV16880_VTS_MAX - ov16880->cur_mode->height,
						 1, h);
			mutex_unlock(&ov16880->mutex);
			dev_info(&ov16880->client->dev,
				"sensor mode: %d\n",
				ov16880->cur_mode->hdr_mode);
		}
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		hdr_cfg->esp.mode = HDR_NORMAL_VC;
		hdr_cfg->hdr_mode = ov16880->cur_mode->hdr_mode;
		break;
	case RKMODULE_GET_MODULE_INFO:
		ov16880_get_module_inf(ov16880, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = ov16880_write_reg(ov16880->client,
				 OV16880_REG_CTRL_MODE,
				 OV16880_REG_VALUE_08BIT,
				 OV16880_MODE_STREAMING);
		else
			ret = ov16880_write_reg(ov16880->client,
				 OV16880_REG_CTRL_MODE,
				 OV16880_REG_VALUE_08BIT,
				 OV16880_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ov16880_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = ov16880_ioctl(sd, cmd, inf);
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
			ret = ov16880_ioctl(sd, cmd, cfg);
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

		ret = ov16880_ioctl(sd, cmd, hdr);
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
		ret = ov16880_ioctl(sd, cmd, hdr);
		kfree(hdr);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = ov16880_ioctl(sd, cmd, &stream);
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

static int __ov16880_start_stream(struct ov16880 *ov16880)
{
	int ret;

	ret = ov16880_write_array(ov16880->client, ov16880_global_regs);
	if (ret)
		return ret;

	ret = ov16880_write_array(ov16880->client, ov16880->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&ov16880->mutex);
	ret = v4l2_ctrl_handler_setup(&ov16880->ctrl_handler);
	mutex_lock(&ov16880->mutex);
	if (ret)
		return ret;

	return ov16880_write_reg(ov16880->client,
				 OV16880_REG_CTRL_MODE,
				 OV16880_REG_VALUE_08BIT,
				 OV16880_MODE_STREAMING);
}

static int __ov16880_stop_stream(struct ov16880 *ov16880)
{
	return ov16880_write_reg(ov16880->client,
				 OV16880_REG_CTRL_MODE,
				 OV16880_REG_VALUE_08BIT,
				 OV16880_MODE_SW_STANDBY);
}

static int ov16880_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ov16880 *ov16880 = to_ov16880(sd);
	struct i2c_client *client = ov16880->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				ov16880->cur_mode->width,
				ov16880->cur_mode->height,
		DIV_ROUND_CLOSEST(ov16880->cur_mode->max_fps.denominator,
				  ov16880->cur_mode->max_fps.numerator));

	mutex_lock(&ov16880->mutex);
	on = !!on;
	if (on == ov16880->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __ov16880_start_stream(ov16880);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__ov16880_stop_stream(ov16880);
		pm_runtime_put(&client->dev);
	}

	ov16880->streaming = on;

unlock_and_return:
	mutex_unlock(&ov16880->mutex);

	return ret;
}

static int ov16880_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov16880 *ov16880 = to_ov16880(sd);
	struct i2c_client *client = ov16880->client;
	int ret = 0;

	mutex_lock(&ov16880->mutex);

	/* If the power state is not modified - no work to do. */
	if (ov16880->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ov16880->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		ov16880->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&ov16880->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 ov16880_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, OV16880_XVCLK_FREQ / 1000 / 1000);
}

static int __ov16880_power_on(struct ov16880 *ov16880)
{
	int ret;
	u32 delay_us;
	struct device *dev = &ov16880->client->dev;

	if (!IS_ERR(ov16880->power_gpio))
		gpiod_set_value_cansleep(ov16880->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(ov16880->pins_default)) {
		ret = pinctrl_select_state(ov16880->pinctrl,
					   ov16880->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(ov16880->xvclk, OV16880_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(ov16880->xvclk) != OV16880_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(ov16880->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}
	if (!IS_ERR(ov16880->reset_gpio))
		gpiod_set_value_cansleep(ov16880->reset_gpio, 0);

	ret = regulator_bulk_enable(OV16880_NUM_SUPPLIES, ov16880->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(ov16880->reset_gpio))
		gpiod_set_value_cansleep(ov16880->reset_gpio, 1);

	usleep_range(5000, 6000);
	if (!IS_ERR(ov16880->pwdn_gpio))
		gpiod_set_value_cansleep(ov16880->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = ov16880_cal_delay(8192);
	usleep_range(delay_us * 2, delay_us * 3);

	return 0;

disable_clk:
	clk_disable_unprepare(ov16880->xvclk);

	return ret;
}

static void __ov16880_power_off(struct ov16880 *ov16880)
{
	int ret;
	struct device *dev = &ov16880->client->dev;

	if (!IS_ERR(ov16880->pwdn_gpio))
		gpiod_set_value_cansleep(ov16880->pwdn_gpio, 0);
	clk_disable_unprepare(ov16880->xvclk);
	if (!IS_ERR(ov16880->reset_gpio))
		gpiod_set_value_cansleep(ov16880->reset_gpio, 0);

	if (!IS_ERR_OR_NULL(ov16880->pins_sleep)) {
		ret = pinctrl_select_state(ov16880->pinctrl,
					   ov16880->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	if (!IS_ERR(ov16880->power_gpio))
		gpiod_set_value_cansleep(ov16880->power_gpio, 0);

	regulator_bulk_disable(OV16880_NUM_SUPPLIES, ov16880->supplies);
}

static int ov16880_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov16880 *ov16880 = to_ov16880(sd);

	return __ov16880_power_on(ov16880);
}

static int ov16880_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov16880 *ov16880 = to_ov16880(sd);

	__ov16880_power_off(ov16880);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int ov16880_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov16880 *ov16880 = to_ov16880(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct ov16880_mode *def_mode = &supported_modes[0];

	mutex_lock(&ov16880->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = OV16880_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&ov16880->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int ov16880_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = OV16880_MEDIA_BUS_FMT;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;

	return 0;
}

static int ov16880_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes  = OV16880_LANES;

	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_2320 2320
#define DST_HEIGHT_1744 1744
/*
 * The resolution of the driver configuration needs to be exactly
 * the same as the current output resolution of the sensor,
 * the input width of the isp needs to be 16 aligned,
 * the input height of the isp needs to be 8 aligned.
 * Can be cropped to standard resolution by this function,
 * otherwise it will crop out strange resolution according
 * to the alignment rules.
 */
static int ov16880_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct ov16880 *ov16880 = to_ov16880(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		if (ov16880->cur_mode->width == 2328) {
			sel->r.left = CROP_START(ov16880->cur_mode->width, DST_WIDTH_2320);
			sel->r.width = DST_WIDTH_2320;
			sel->r.top = CROP_START(ov16880->cur_mode->height, DST_HEIGHT_1744);
			sel->r.height = DST_HEIGHT_1744;
		} else {
			sel->r.left = 0;
			sel->r.width = ov16880->cur_mode->width;
			sel->r.top = 0;
			sel->r.height = ov16880->cur_mode->height;
		}
		return 0;
	}

	return -EINVAL;
}

static const struct dev_pm_ops ov16880_pm_ops = {
	SET_RUNTIME_PM_OPS(ov16880_runtime_suspend,
			   ov16880_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops ov16880_internal_ops = {
	.open = ov16880_open,
};
#endif

static const struct v4l2_subdev_core_ops ov16880_core_ops = {
	.s_power = ov16880_s_power,
	.ioctl = ov16880_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ov16880_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops ov16880_video_ops = {
	.s_stream = ov16880_s_stream,
	.g_frame_interval = ov16880_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops ov16880_pad_ops = {
	.enum_mbus_code = ov16880_enum_mbus_code,
	.enum_frame_size = ov16880_enum_frame_sizes,
	.enum_frame_interval = ov16880_enum_frame_interval,
	.get_fmt = ov16880_get_fmt,
	.set_fmt = ov16880_set_fmt,
	.get_selection = ov16880_get_selection,
	.get_mbus_config = ov16880_g_mbus_config,
};

static const struct v4l2_subdev_ops ov16880_subdev_ops = {
	.core	= &ov16880_core_ops,
	.video	= &ov16880_video_ops,
	.pad	= &ov16880_pad_ops,
};

static int ov16880_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov16880 *ov16880 = container_of(ctrl->handler,
					     struct ov16880, ctrl_handler);
	struct i2c_client *client = ov16880->client;
	s64 max;
	int ret = 0;
	u32 val = 0, blc_ctrl = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = ov16880->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(ov16880->exposure,
					 ov16880->exposure->minimum, max,
					 ov16880->exposure->step,
					 ov16880->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = ov16880_write_reg(ov16880->client,
					OV16880_REG_EXPOSURE_H,
					OV16880_REG_VALUE_24BIT,
					ctrl->val & 0xfffff);
		dev_dbg(&client->dev, "set exposure 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = ov16880_write_reg(ov16880->client,
					OV16880_AGAIN_REG_H,
					OV16880_REG_VALUE_16BIT,
					ctrl->val & 0x7ff);
		dev_dbg(&client->dev, "set analog gain value 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = ov16880_write_reg(ov16880->client,
					OV16880_REG_VTS_H,
					OV16880_REG_VALUE_16BIT,
					ctrl->val + ov16880->cur_mode->height);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = ov16880_enable_test_pattern(ov16880, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = ov16880_read_reg(ov16880->client, OV16880_MIRROR_REG,
				       OV16880_REG_VALUE_08BIT,
				       &val);
		if (ctrl->val)
			val |= MIRROR_BIT_MASK;
		else
			val &= ~MIRROR_BIT_MASK;

		ret |= ov16880_write_reg(ov16880->client, OV16880_MIRROR_REG,
					 OV16880_REG_VALUE_08BIT,
					 val);
		break;
	case V4L2_CID_VFLIP:
		ret = ov16880_read_reg(ov16880->client, OV16880_FLIP_REG,
				       OV16880_REG_VALUE_08BIT,
				       &val);
		ret |= ov16880_read_reg(ov16880->client, OV16880_BLC_CTRL_REG,
				       OV16880_REG_VALUE_08BIT,
				       &blc_ctrl);
		if (ctrl->val) {
			val |= (FLIP_BIT_MASK | BIT(6));
			blc_ctrl |= BIT(6);
		} else {
			val &= ~(FLIP_BIT_MASK | BIT(6));
			blc_ctrl &= ~BIT(6);
		}

		ret |= ov16880_write_reg(ov16880->client,
					 OV16880_GROUP_UPDATE_ADDRESS,
					 OV16880_REG_VALUE_08BIT,
					 OV16880_GROUP_UPDATE_START_DATA);

		ret |= ov16880_write_reg(ov16880->client, OV16880_FLIP_REG,
					 OV16880_REG_VALUE_08BIT,
					 val);
		ret |= ov16880_write_reg(ov16880->client, OV16880_BLC_CTRL_REG,
					 OV16880_REG_VALUE_08BIT,
					 blc_ctrl);
		ret |= ov16880_write_reg(ov16880->client,
					 OV16880_GROUP_UPDATE_ADDRESS,
					 OV16880_REG_VALUE_08BIT,
					 OV16880_GROUP_UPDATE_END_DATA);
		ret |= ov16880_write_reg(ov16880->client,
					 OV16880_GROUP_UPDATE_ADDRESS,
					 OV16880_REG_VALUE_08BIT,
					 OV16880_GROUP_UPDATE_LAUNCH);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov16880_ctrl_ops = {
	.s_ctrl = ov16880_set_ctrl,
};

static int ov16880_initialize_controls(struct ov16880 *ov16880)
{
	const struct ov16880_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_pixel_rate = 0;
	u32 lane_num = OV16880_LANES;

	handler = &ov16880->ctrl_handler;
	mode = ov16880->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &ov16880->mutex;

	ov16880->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			V4L2_CID_LINK_FREQ,
			0, 0, link_freq_items);

	dst_pixel_rate = (u32)link_freq_items[mode->link_freq_idx] / mode->bpp * 2 * lane_num;

	ov16880->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
			V4L2_CID_PIXEL_RATE,
			0, OV16880_PIXEL_RATE,
			1, dst_pixel_rate);

	__v4l2_ctrl_s_ctrl(ov16880->link_freq,
			   mode->link_freq_idx);

	h_blank = mode->hts_def - mode->width;
	ov16880->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (ov16880->hblank)
		ov16880->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	ov16880->vblank = v4l2_ctrl_new_std(handler, &ov16880_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				OV16880_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 8;
	ov16880->exposure = v4l2_ctrl_new_std(handler, &ov16880_ctrl_ops,
				V4L2_CID_EXPOSURE, OV16880_EXPOSURE_MIN,
				exposure_max, OV16880_EXPOSURE_STEP,
				mode->exp_def);

	ov16880->anal_gain = v4l2_ctrl_new_std(handler, &ov16880_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, OV16880_GAIN_MIN,
				OV16880_GAIN_MAX, OV16880_GAIN_STEP,
				OV16880_GAIN_DEFAULT);

	ov16880->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&ov16880_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(ov16880_test_pattern_menu) - 1,
				0, 0, ov16880_test_pattern_menu);

	ov16880->h_flip = v4l2_ctrl_new_std(handler, &ov16880_ctrl_ops,
					    V4L2_CID_HFLIP, 0, 1, 1, 0);

	ov16880->v_flip = v4l2_ctrl_new_std(handler, &ov16880_ctrl_ops,
					    V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&ov16880->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	ov16880->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int ov16880_check_sensor_id(struct ov16880 *ov16880,
				   struct i2c_client *client)
{
	struct device *dev = &ov16880->client->dev;
	u32 id = 0;
	int ret;

	client->addr = OV16880_MAJOR_I2C_ADDR;

	ret = ov16880_read_reg(client, OV16880_REG_CHIP_ID,
			       OV16880_REG_VALUE_24BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		client->addr = OV16880_MINOR_I2C_ADDR;
		ret = ov16880_read_reg(client, OV16880_REG_CHIP_ID,
				       OV16880_REG_VALUE_24BIT, &id);
		if (id != CHIP_ID) {
			dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
			return -ENODEV;
		}
	}

	dev_info(dev, "Detected OV%06x sensor\n", CHIP_ID);

	return 0;
}

static int ov16880_configure_regulators(struct ov16880 *ov16880)
{
	unsigned int i;

	for (i = 0; i < OV16880_NUM_SUPPLIES; i++)
		ov16880->supplies[i].supply = ov16880_supply_names[i];

	return devm_regulator_bulk_get(&ov16880->client->dev,
				       OV16880_NUM_SUPPLIES,
				       ov16880->supplies);
}

static int ov16880_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct ov16880 *ov16880;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;
#ifdef RK_OTP
	struct device_node *eeprom_ctrl_node;
	struct i2c_client *eeprom_ctrl_client;
	struct v4l2_subdev *eeprom_ctrl;
	struct otp_info *otp_ptr;
#endif

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	ov16880 = devm_kzalloc(dev, sizeof(*ov16880), GFP_KERNEL);
	if (!ov16880)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &ov16880->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &ov16880->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &ov16880->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &ov16880->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE,
			&hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}
	ov16880->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < ov16880->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			ov16880->cur_mode = &supported_modes[i];
			break;
		}
	}

	ov16880->client = client;

	ov16880->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(ov16880->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	ov16880->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(ov16880->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");

	ov16880->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ov16880->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	ov16880->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(ov16880->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = ov16880_configure_regulators(ov16880);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ov16880->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(ov16880->pinctrl)) {
		ov16880->pins_default =
			pinctrl_lookup_state(ov16880->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(ov16880->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		ov16880->pins_sleep =
			pinctrl_lookup_state(ov16880->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(ov16880->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&ov16880->mutex);

	sd = &ov16880->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov16880_subdev_ops);
	ret = ov16880_initialize_controls(ov16880);
	if (ret)
		goto err_destroy_mutex;

	ret = __ov16880_power_on(ov16880);
	if (ret)
		goto err_free_handler;

	ret = ov16880_check_sensor_id(ov16880, client);
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
				ov16880->otp = otp_ptr;
			} else {
				ov16880->otp = NULL;
				devm_kfree(dev, otp_ptr);
				dev_warn(dev, "can not get otp info, skip!\n");
			}
		}
	}
continue_probe:
#endif

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &ov16880_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	ov16880->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &ov16880->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(ov16880->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 ov16880->module_index, facing,
		 OV16880_NAME, dev_name(sd->dev));
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
	__ov16880_power_off(ov16880);
err_free_handler:
	v4l2_ctrl_handler_free(&ov16880->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&ov16880->mutex);

	return ret;
}

static void ov16880_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov16880 *ov16880 = to_ov16880(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&ov16880->ctrl_handler);
	mutex_destroy(&ov16880->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__ov16880_power_off(ov16880);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ov16880_of_match[] = {
	{ .compatible = "ovti,ov16880" },
	{},
};
MODULE_DEVICE_TABLE(of, ov16880_of_match);
#endif

static const struct i2c_device_id ov16880_match_id[] = {
	{ "ovti,ov16880", 0 },
	{},
};

static struct i2c_driver ov16880_i2c_driver = {
	.driver = {
		.name = OV16880_NAME,
		.pm = &ov16880_pm_ops,
		.of_match_table = of_match_ptr(ov16880_of_match),
	},
	.probe		= &ov16880_probe,
	.remove		= &ov16880_remove,
	.id_table	= ov16880_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&ov16880_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&ov16880_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("OmniVision ov16880 sensor driver");
MODULE_LICENSE("GPL");
