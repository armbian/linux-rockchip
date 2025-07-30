// SPDX-License-Identifier: GPL-2.0
/*
 * os04e10 driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define OS04E10_LANES			2
#define OS04E10_BITS_PER_SAMPLE		10
#define OS04E10_LINK_FREQ_900		450000000

#define PIXEL_RATE_WITH_900M_10BIT	(OS04E10_LINK_FREQ_900 * 2 * \
					 OS04E10_LANES / OS04E10_BITS_PER_SAMPLE)
#define OS04E10_XVCLK_FREQ		24000000

#define CHIP_ID				0x530641
#define OS04E10_REG_CHIP_ID		0x300A

#define OS04E10_REG_CTRL_MODE		0x0100
#define OS04E10_MODE_SW_STANDBY		0x0
#define OS04E10_MODE_STREAMING		BIT(0)

#define OS04E10_REG_EXPOSURE_H		0x3500
#define OS04E10_REG_EXPOSURE_M		0x3501
#define OS04E10_REG_EXPOSURE_L		0x3502
#define OS04E10_REG_EXPOSURE_SHORT_H	0x3510
#define OS04E10_REG_EXPOSURE_SHORT_M	0x3511
#define OS04E10_REG_EXPOSURE_SHORT_L	0x3512
#define	OS04E10_EXPOSURE_MIN		2
#define	OS04E10_EXPOSURE_STEP		1
#define OS04E10_VTS_MAX			0x7fff

#define OS04E10_REG_ANA_GAIN_H		0x3508
#define OS04E10_REG_ANA_GAIN_L		0x3509
#define OS04E10_REG_DIG_GAIN_H		0x350A
#define OS04E10_REG_DIG_GAIN_L		0x350B
#define OS04E10_REG_ANA_GAIN_SHORT_H	0x350C
#define OS04E10_REG_ANA_GAIN_SHORT_L	0x350D
#define OS04E10_REG_DIG_GAIN_SHORT_H	0x350E
#define OS04E10_REG_DIG_GAIN_SHORT_L	0x350F

#define OS04E10_LGAIN			0
#define OS04E10_SGAIN			1
#define OS04E10_GAIN_MIN		0x0080
#define OS04E10_GAIN_MAX		(31743)	//15.5 * 16 * 128
#define OS04E10_GAIN_STEP		1
#define OS04E10_GAIN_DEFAULT		0x80

#define OS04E10_REG_GROUP_HOLD		0x3812
#define OS04E10_GROUP_HOLD_START		0x00
#define OS04E10_GROUP_HOLD_END		0x30

#define OS04E10_REG_TEST_PATTERN	0x5080
#define OS04E10_TEST_PATTERN_BIT_MASK	BIT(3)

#define OS04E10_REG_VTS_H		0x380E
#define OS04E10_REG_VTS_L		0x380F

#define OS04E10_FLIP_MIRROR_REG		0x3820

#define OS04E10_FETCH_EXP_H(VAL)	(((VAL) >> 16) & 0xF)
#define OS04E10_FETCH_EXP_M(VAL)	(((VAL) >> 8) & 0xFF)
#define OS04E10_FETCH_EXP_L(VAL)	((VAL) & 0xFF)

#define OS04E10_FETCH_AGAIN_H(VAL)	(((VAL) >> 8) & 0x3F)
#define OS04E10_FETCH_AGAIN_L(VAL)	((VAL) & 0xFF)

#define OS04E10_FETCH_DGAIN_H(VAL)	(((VAL) >> 8) & 0x3F)
#define OS04E10_FETCH_DGAIN_L(VAL)	((VAL) & 0xFF)

#define OS04E10_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL & 0xf7 : VAL | 0x08)
#define OS04E10_FETCH_FLIP(VAL, ENABLE)		(ENABLE ? VAL | 0x30 : VAL & 0xcf)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define OS04E10_REG_VALUE_08BIT		1
#define OS04E10_REG_VALUE_16BIT		2
#define OS04E10_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE              "rockchip,camera-hdr-mode"
#define OS04E10_NAME			"os04e10"

static const char * const os04e10_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define OS04E10_NUM_SUPPLIES ARRAY_SIZE(os04e10_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct os04e10_mode {
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
};

struct os04e10 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[OS04E10_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	struct v4l2_fract	cur_fps;
	bool			streaming;
	bool			power_on;
	const struct os04e10_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	bool			is_thunderboot;
	bool			is_first_streamoff;
	struct preisp_hdrae_exp_s init_hdrae_exp;
};

#define to_os04e10(sd) container_of(sd, struct os04e10, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval os04e10_global_regs[] = {
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 900Mbps, 2lane
 */
static const struct regval os04e10_linear_10_2048x2048_regs[] = {
	{0x0103, 0x01},
	{0x0301, 0x44},
	{0x0303, 0x02},
	{0x0304, 0x00},
	{0x0305, 0x4b},
	{0x0306, 0x00},
	{0x0325, 0x3b},
	{0x0327, 0x04},
	{0x0328, 0x05},
	{0x3002, 0x21},
	{0x3016, 0x32},
	{0x301e, 0xb4},
	{0x301f, 0xd0},
	{0x3021, 0x03},
	{0x3022, 0x01},
	{0x3107, 0xa1},
	{0x3108, 0x7d},
	{0x3109, 0xfc},
	{0x3500, 0x00},
	{0x3501, 0x08},
	{0x3502, 0x54},
	{0x3503, 0x88},
	{0x3508, 0x01},
	{0x3509, 0x00},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x04},
	{0x350d, 0x00},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x20},
	{0x3600, 0x4c},
	{0x3601, 0x08},
	{0x3610, 0x87},
	{0x3611, 0x24},
	{0x3614, 0x4c},
	{0x3620, 0x0c},
	{0x3621, 0x04},
	{0x3632, 0x80},
	{0x3633, 0x00},
	{0x3660, 0x00},
	{0x3662, 0x10},
	{0x3664, 0x70},
	{0x3665, 0x00},
	{0x3666, 0x00},
	{0x3667, 0x00},
	{0x366a, 0x14},
	{0x3670, 0x0b},
	{0x3671, 0x0b},
	{0x3672, 0x0b},
	{0x3673, 0x0b},
	{0x3674, 0x00},
	{0x3678, 0x2b},
	{0x3679, 0x43},
	{0x3681, 0xff},
	{0x3682, 0x86},
	{0x3683, 0x44},
	{0x3684, 0x24},
	{0x3685, 0x00},
	{0x368a, 0x00},
	{0x368d, 0x2b},
	{0x368e, 0x6b},
	{0x3690, 0x00},
	{0x3691, 0x0b},
	{0x3692, 0x0b},
	{0x3693, 0x0b},
	{0x3694, 0x0b},
	{0x3699, 0x03},
	{0x369d, 0x68},
	{0x369e, 0x34},
	{0x369f, 0x1b},
	{0x36a0, 0x0f},
	{0x36a1, 0x77},
	{0x36a2, 0x00},
	{0x36a3, 0x02},
	{0x36a4, 0x02},
	{0x36b0, 0x30},
	{0x36b1, 0xf0},
	{0x36b2, 0x00},
	{0x36b3, 0x00},
	{0x36b4, 0x00},
	{0x36b5, 0x00},
	{0x36b6, 0x00},
	{0x36b7, 0x00},
	{0x36b8, 0x00},
	{0x36b9, 0x00},
	{0x36ba, 0x00},
	{0x36bb, 0x00},
	{0x36bc, 0x00},
	{0x36bd, 0x00},
	{0x36be, 0x00},
	{0x36bf, 0x00},
	{0x36c0, 0x1f},
	{0x36c1, 0x00},
	{0x36c2, 0x00},
	{0x36c3, 0x00},
	{0x36c4, 0x00},
	{0x36c5, 0x00},
	{0x36c6, 0x00},
	{0x36c7, 0x00},
	{0x36c8, 0x00},
	{0x36c9, 0x00},
	{0x36ca, 0x0e},
	{0x36cb, 0x0e},
	{0x36cc, 0x0e},
	{0x36cd, 0x0e},
	{0x36ce, 0x0c},
	{0x36cf, 0x0c},
	{0x36d0, 0x0c},
	{0x36d1, 0x0c},
	{0x36d2, 0x00},
	{0x36d3, 0x08},
	{0x36d4, 0x10},
	{0x36d5, 0x10},
	{0x36d6, 0x00},
	{0x36d7, 0x08},
	{0x36d8, 0x10},
	{0x36d9, 0x10},
	{0x3704, 0x05},
	{0x3705, 0x00},
	{0x3706, 0x2b},
	{0x3709, 0x49},
	{0x370a, 0x00},
	{0x370b, 0x60},
	{0x370e, 0x0c},
	{0x370f, 0x1c},
	{0x3710, 0x00},
	{0x3713, 0x00},
	{0x3714, 0x24},
	{0x3716, 0x24},
	{0x371a, 0x1e},
	{0x3724, 0x0d},
	{0x3725, 0xb2},
	{0x372b, 0x54},
	{0x3739, 0x10},
	{0x373f, 0xb0},
	{0x3740, 0x2b},
	{0x3741, 0x2b},
	{0x3742, 0x2b},
	{0x3743, 0x2b},
	{0x3744, 0x60},
	{0x3745, 0x60},
	{0x3746, 0x60},
	{0x3747, 0x60},
	{0x3748, 0x00},
	{0x3749, 0x00},
	{0x374a, 0x00},
	{0x374b, 0x00},
	{0x374c, 0x00},
	{0x374d, 0x00},
	{0x374e, 0x00},
	{0x374f, 0x00},
	{0x3756, 0x00},
	{0x3757, 0x0e},
	{0x3760, 0x11},
	{0x3767, 0x08},
	{0x3773, 0x01},
	{0x3774, 0x02},
	{0x3775, 0x12},
	{0x3776, 0x02},
	{0x377b, 0x40},
	{0x377c, 0x00},
	{0x377d, 0x0c},
	{0x3782, 0x02},
	{0x3787, 0x24},
	{0x3795, 0x24},
	{0x3796, 0x01},
	{0x3798, 0x40},
	{0x379c, 0x00},
	{0x379d, 0x00},
	{0x379e, 0x00},
	{0x379f, 0x01},
	{0x37a1, 0x10},
	{0x37a6, 0x00},
	{0x37ac, 0xa0},
	{0x37bb, 0x02},
	{0x37be, 0x0a},
	{0x37bf, 0x0a},
	{0x37c2, 0x04},
	{0x37c4, 0x11},
	{0x37c5, 0x80},
	{0x37c6, 0x14},
	{0x37c7, 0x08},
	{0x37c8, 0x42},
	{0x37cd, 0x17},
	{0x37ce, 0x04},
	{0x37d9, 0x08},
	{0x37dc, 0x01},
	{0x37e0, 0x30},
	{0x37e1, 0x10},
	{0x37e2, 0x14},
	{0x37e4, 0x28},
	{0x37ef, 0x00},
	{0x37f4, 0x00},
	{0x37f5, 0x00},
	{0x37f6, 0x00},
	{0x37f7, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x08},
	{0x3805, 0x0f},
	{0x3806, 0x08},
	{0x3807, 0x0f},
	{0x3808, 0x08},
	{0x3809, 0x00},
	{0x380a, 0x08},
	{0x380b, 0x00},
	{0x380c, 0x06},
	{0x380d, 0x50},
	{0x380e, 0x08},
	{0x380f, 0x74},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x08},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x3818, 0x00},
	{0x3819, 0x00},
	{0x381a, 0x00},
	{0x381b, 0x01},
	{0x3820, 0x80},
	{0x3821, 0x00},
	{0x3822, 0x04},
	{0x3823, 0x08},
	{0x3824, 0x00},
	{0x3825, 0x20},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x3829, 0x03},
	{0x382a, 0x00},
	{0x382b, 0x00},
	{0x3832, 0x08},
	{0x3838, 0x00},
	{0x3839, 0x00},
	{0x383a, 0x00},
	{0x383b, 0x00},
	{0x383d, 0x01},
	{0x383e, 0x00},
	{0x383f, 0x00},
	{0x3843, 0x00},
	{0x3848, 0x08},
	{0x3849, 0x00},
	{0x384a, 0x08},
	{0x384b, 0x00},
	{0x384c, 0x00},
	{0x384d, 0x08},
	{0x384e, 0x00},
	{0x384f, 0x08},
	{0x3880, 0x16},
	{0x3881, 0x00},
	{0x3882, 0x08},
	{0x388a, 0x00},
	{0x389a, 0x00},
	{0x389b, 0x00},
	{0x389c, 0x00},
	{0x38a2, 0x02},
	{0x38a3, 0x02},
	{0x38a4, 0x02},
	{0x38a5, 0x02},
	{0x38a7, 0x04},
	{0x38ae, 0x1e},
	{0x38b8, 0x02},
	{0x38c3, 0x06},
	{0x3c80, 0x3f},
	{0x3c86, 0x01},
	{0x3c87, 0x02},
	{0x3ca0, 0x01},
	{0x3ca2, 0x0c},
	{0x3d8c, 0x71},
	{0x3d8d, 0xe2},
	{0x3f00, 0xcb},
	{0x3f04, 0x04},
	{0x3f07, 0x04},
	{0x3f09, 0x50},
	{0x3f9e, 0x07},
	{0x3f9f, 0x04},
	{0x4000, 0xf3},
	{0x4002, 0x00},
	{0x4003, 0x40},
	{0x4008, 0x00},
	{0x4009, 0x0f},
	{0x400a, 0x01},
	{0x400b, 0x78},
	{0x400f, 0x89},
	{0x4040, 0x00},
	{0x4041, 0x07},
	{0x4090, 0x14},
	{0x40b0, 0x00},
	{0x40b1, 0x00},
	{0x40b2, 0x00},
	{0x40b3, 0x00},
	{0x40b4, 0x00},
	{0x40b5, 0x00},
	{0x40b7, 0x00},
	{0x40b8, 0x00},
	{0x40b9, 0x00},
	{0x40ba, 0x00},
	{0x4300, 0xff},
	{0x4301, 0x00},
	{0x4302, 0x0f},
	{0x4303, 0x01},
	{0x4304, 0x01},
	{0x4305, 0x83},
	{0x4306, 0x21},
	{0x430d, 0x00},
	{0x4501, 0x00},
	{0x4505, 0xc4},
	{0x4506, 0x00},
	{0x4507, 0x60},
	{0x4508, 0x00},
	{0x4800, 0x64},
	{0x4803, 0x00},
	{0x4809, 0x8e},
	{0x480e, 0x00},
	{0x4813, 0x00},
	{0x4814, 0x2a},
	{0x481b, 0x3c},
	{0x481f, 0x26},
	{0x4825, 0x32},
	{0x4829, 0x64},
	{0x4837, 0x11},
	{0x484b, 0x07},
	{0x4883, 0x36},
	{0x4885, 0x03},
	{0x488b, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0x99},
	{0x4d02, 0xbd},
	{0x4d03, 0xac},
	{0x4d04, 0xf2},
	{0x4d05, 0x54},
	{0x4e00, 0x2a},
	{0x4e0d, 0x00},
	{0x5000, 0xbb},
	{0x5001, 0x09},
	{0x5004, 0x00},
	{0x5005, 0x0e},
	{0x5036, 0x00},
	{0x5080, 0x04},
	{0x5082, 0x00},
	{0x5180, 0x70},
	{0x5181, 0x10},
	{0x5182, 0x71},
	{0x5183, 0xdf},
	{0x5184, 0x02},
	{0x5185, 0x6c},
	{0x5189, 0x48},
	{0x5324, 0x09},
	{0x5325, 0x11},
	{0x5326, 0x1f},
	{0x5327, 0x3b},
	{0x5328, 0x49},
	{0x5329, 0x61},
	{0x532a, 0x9c},
	{0x532b, 0xc9},
	{0x5335, 0x04},
	{0x5336, 0x00},
	{0x5337, 0x04},
	{0x5338, 0x00},
	{0x5339, 0x0b},
	{0x533a, 0x00},
	{0x53a4, 0x09},
	{0x53a5, 0x11},
	{0x53a6, 0x1f},
	{0x53a7, 0x3b},
	{0x53a8, 0x49},
	{0x53a9, 0x61},
	{0x53aa, 0x9c},
	{0x53ab, 0xc9},
	{0x53b5, 0x04},
	{0x53b6, 0x00},
	{0x53b7, 0x04},
	{0x53b8, 0x00},
	{0x53b9, 0x0b},
	{0x53ba, 0x00},
	{0x580b, 0x03},
	{0x580d, 0x00},
	{0x580f, 0x00},
	{0x5820, 0x00},
	{0x5821, 0x00},
	{0x5888, 0x01},
	{REG_NULL, 0x00},
};


/*
 * Xclk 24Mhz
 * max_framerate 15fps
 * mipi_datarate per lane 900Mbps, 2lane
 */
static const struct regval os04e10_hdrx2_10_2048x2048_15fps_regs[] = {
	{0x0103, 0x01},
	{0x0301, 0x44},
	{0x0303, 0x02},
	{0x0304, 0x00},
	{0x0305, 0x4b},
	{0x0306, 0x00},
	{0x0325, 0x3b},
	{0x0327, 0x04},
	{0x0328, 0x05},
	{0x3002, 0x21},
	{0x3016, 0x32},
	{0x301b, 0xf0},
	{0x301e, 0xb4},
	{0x301f, 0xf0},
	{0x3021, 0x03},
	{0x3022, 0x01},
	{0x3107, 0xa1},
	{0x3108, 0x7d},
	{0x3109, 0xfc},
	{0x3500, 0x00},
	{0x3501, 0x04},
	{0x3502, 0x2a},
	{0x3503, 0x88},
	{0x3508, 0x01},
	{0x3509, 0x00},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x04},
	{0x350d, 0x00},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x20},
	{0x3600, 0x4c},
	{0x3601, 0x08},
	{0x3610, 0x87},
	{0x3611, 0x24},
	{0x3614, 0x4c},
	{0x3620, 0x0c},
	{0x3621, 0x04},
	{0x3632, 0x80},
	{0x3633, 0x00},
	{0x3660, 0x04},
	{0x3662, 0x10},
	{0x3664, 0x70},
	{0x3665, 0x00},
	{0x3666, 0x00},
	{0x3667, 0x00},
	{0x366a, 0x54},
	{0x3670, 0x0b},
	{0x3671, 0x0b},
	{0x3672, 0x0b},
	{0x3673, 0x0b},
	{0x3674, 0x00},
	{0x3678, 0x2b},
	{0x3679, 0x43},
	{0x3681, 0xff},
	{0x3682, 0x86},
	{0x3683, 0x44},
	{0x3684, 0x24},
	{0x3685, 0x00},
	{0x368a, 0x00},
	{0x368d, 0x2b},
	{0x368e, 0x6b},
	{0x3690, 0x00},
	{0x3691, 0x0b},
	{0x3692, 0x0b},
	{0x3693, 0x0b},
	{0x3694, 0x0b},
	{0x3699, 0x03},
	{0x369d, 0xff},
	{0x369e, 0x86},
	{0x369f, 0x44},
	{0x36a0, 0x24},
	{0x36a1, 0x77},
	{0x36a2, 0xf0},
	{0x36a3, 0x82},
	{0x36a4, 0x82},
	{0x36b0, 0x30},
	{0x36b1, 0xf0},
	{0x36b2, 0x00},
	{0x36b3, 0x00},
	{0x36b4, 0x00},
	{0x36b5, 0x00},
	{0x36b6, 0x00},
	{0x36b7, 0x00},
	{0x36b8, 0x00},
	{0x36b9, 0x00},
	{0x36ba, 0x00},
	{0x36bb, 0x00},
	{0x36bc, 0x1f},
	{0x36bd, 0x00},
	{0x36be, 0x00},
	{0x36bf, 0x00},
	{0x36c0, 0x1f},
	{0x36c1, 0x00},
	{0x36c2, 0x00},
	{0x36c3, 0x00},
	{0x36c4, 0x00},
	{0x36c5, 0x00},
	{0x36c6, 0x00},
	{0x36c7, 0x00},
	{0x36c8, 0x00},
	{0x36c9, 0x00},
	{0x36ca, 0x0c},
	{0x36cb, 0x0c},
	{0x36cc, 0x0c},
	{0x36cd, 0x0c},
	{0x36ce, 0x0c},
	{0x36cf, 0x0c},
	{0x36d0, 0x0c},
	{0x36d1, 0x0c},
	{0x36d2, 0x00},
	{0x36d3, 0x08},
	{0x36d4, 0x10},
	{0x36d5, 0x10},
	{0x36d6, 0x00},
	{0x36d7, 0x08},
	{0x36d8, 0x10},
	{0x36d9, 0x10},
	{0x3704, 0x01},
	{0x3705, 0x00},
	{0x3706, 0x2b},
	{0x3709, 0x46},
	{0x370a, 0x00},
	{0x370b, 0x60},
	{0x370e, 0x0c},
	{0x370f, 0x1c},
	{0x3710, 0x00},
	{0x3713, 0x00},
	{0x3714, 0x24},
	{0x3716, 0x24},
	{0x371a, 0x1e},
	{0x3724, 0x0d},
	{0x3725, 0xb2},
	{0x372b, 0x54},
	{0x3739, 0x10},
	{0x373f, 0xb0},
	{0x3740, 0x2b},
	{0x3741, 0x2b},
	{0x3742, 0x2b},
	{0x3743, 0x2b},
	{0x3744, 0x60},
	{0x3745, 0x60},
	{0x3746, 0x60},
	{0x3747, 0x60},
	{0x3748, 0x00},
	{0x3749, 0x00},
	{0x374a, 0x00},
	{0x374b, 0x00},
	{0x374c, 0x00},
	{0x374d, 0x00},
	{0x374e, 0x00},
	{0x374f, 0x00},
	{0x3756, 0x00},
	{0x3757, 0x00},
	{0x3760, 0x22},
	{0x3767, 0x08},
	{0x3773, 0x01},
	{0x3774, 0x02},
	{0x3775, 0x12},
	{0x3776, 0x02},
	{0x377b, 0x4a},
	{0x377c, 0x00},
	{0x377d, 0x0c},
	{0x3782, 0x01},
	{0x3787, 0x14},
	{0x3795, 0x14},
	{0x3796, 0x01},
	{0x3798, 0x40},
	{0x379c, 0x0c},
	{0x379d, 0x1c},
	{0x379e, 0x0c},
	{0x379f, 0x00},
	{0x37a1, 0x10},
	{0x37a6, 0x00},
	{0x37ac, 0x00},
	{0x37bb, 0x01},
	{0x37be, 0x0a},
	{0x37bf, 0x0a},
	{0x37c2, 0x04},
	{0x37c4, 0x11},
	{0x37c5, 0x80},
	{0x37c6, 0x14},
	{0x37c7, 0x08},
	{0x37c8, 0x42},
	{0x37cd, 0x17},
	{0x37ce, 0x04},
	{0x37d9, 0x08},
	{0x37dc, 0x01},
	{0x37e0, 0x30},
	{0x37e1, 0x10},
	{0x37e2, 0x14},
	{0x37e4, 0x28},
	{0x37ef, 0x00},
	{0x37f4, 0x00},
	{0x37f5, 0x00},
	{0x37f6, 0x00},
	{0x37f7, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x08},
	{0x3805, 0x0f},
	{0x3806, 0x08},
	{0x3807, 0x0f},
	{0x3808, 0x08},
	{0x3809, 0x00},
	{0x380a, 0x08},
	{0x380b, 0x00},
	{0x380c, 0x06},
	{0x380d, 0x34},
	{0x380e, 0x08},
	{0x380f, 0x9b},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x08},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x3818, 0x00},
	{0x3819, 0x00},
	{0x381a, 0x00},
	{0x381b, 0x01},
	{0x3820, 0x88},
	{0x3821, 0x04},
	{0x3822, 0x14},
	{0x3823, 0x08},
	{0x3824, 0x00},
	{0x3825, 0x20},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x3829, 0x03},
	{0x382a, 0x00},
	{0x382b, 0x00},
	{0x3832, 0x08},
	{0x3838, 0x00},
	{0x3839, 0x00},
	{0x383a, 0x00},
	{0x383b, 0x00},
	{0x383d, 0x01},
	{0x383e, 0x00},
	{0x383f, 0x00},
	{0x3843, 0x00},
	{0x3848, 0x08},
	{0x3849, 0x00},
	{0x384a, 0x08},
	{0x384b, 0x00},
	{0x384c, 0x00},
	{0x384d, 0x08},
	{0x384e, 0x00},
	{0x384f, 0x08},
	{0x3880, 0x16},
	{0x3881, 0x16},
	{0x3882, 0x08},
	{0x388a, 0x00},
	{0x389a, 0x00},
	{0x389b, 0x00},
	{0x389c, 0x00},
	{0x38a2, 0x01},
	{0x38a3, 0x01},
	{0x38a4, 0x01},
	{0x38a5, 0x01},
	{0x38a7, 0x04},
	{0x38ae, 0x1e},
	{0x38b8, 0x02},
	{0x38c3, 0x06},
	{0x3c80, 0x3e},
	{0x3c86, 0x01},
	{0x3c87, 0x02},
	{0x3ca0, 0x01},
	{0x3ca2, 0x0c},
	{0x3d8c, 0x71},
	{0x3d8d, 0xe2},
	{0x3f00, 0xcb},
	{0x3f04, 0x04},
	{0x3f07, 0x04},
	{0x3f09, 0x50},
	{0x3f9e, 0x07},
	{0x3f9f, 0x04},
	{0x4000, 0xf3},
	{0x4002, 0x00},
	{0x4003, 0x40},
	{0x4008, 0x00},
	{0x4009, 0x0f},
	{0x400a, 0x01},
	{0x400b, 0x78},
	{0x400f, 0x89},
	{0x4040, 0x00},
	{0x4041, 0x07},
	{0x4090, 0x14},
	{0x40b0, 0x00},
	{0x40b1, 0x00},
	{0x40b2, 0x00},
	{0x40b3, 0x00},
	{0x40b4, 0x00},
	{0x40b5, 0x00},
	{0x40b7, 0x00},
	{0x40b8, 0x00},
	{0x40b9, 0x00},
	{0x40ba, 0x01},
	{0x4300, 0xff},
	{0x4301, 0x00},
	{0x4302, 0x0f},
	{0x4303, 0x01},
	{0x4304, 0x01},
	{0x4305, 0x83},
	{0x4306, 0x21},
	{0x430d, 0x00},
	{0x4501, 0x00},
	{0x4505, 0xc4},
	{0x4506, 0x00},
	{0x4507, 0x43},
	{0x4508, 0x00},
	{0x4600, 0x00},
	{0x4601, 0x40},
	{0x4603, 0x03},
	{0x4800, 0x64},
	{0x4803, 0x00},
	{0x4809, 0x8e},
	{0x480e, 0x04},
	{0x4813, 0xe4},
	{0x4814, 0x2a},
	{0x481b, 0x3c},
	{0x481f, 0x26},
	{0x4825, 0x32},
	{0x4829, 0x64},
	{0x4837, 0x11},
	{0x484b, 0x27},
	{0x4883, 0x36},
	{0x4885, 0x03},
	{0x488b, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0x99},
	{0x4d02, 0xbd},
	{0x4d03, 0xac},
	{0x4d04, 0xf2},
	{0x4d05, 0x54},
	{0x4e00, 0x2a},
	{0x4e0d, 0x00},
	{0x5000, 0xbb},
	{0x5001, 0x09},
	{0x5004, 0x00},
	{0x5005, 0x0e},
	{0x5036, 0x80},
	{0x5080, 0x04},
	{0x5082, 0x00},
	{0x5180, 0x70},
	{0x5181, 0x10},
	{0x5182, 0x71},
	{0x5183, 0xdf},
	{0x5184, 0x02},
	{0x5185, 0x6c},
	{0x5189, 0x48},
	{0x5324, 0x09},
	{0x5325, 0x11},
	{0x5326, 0x1f},
	{0x5327, 0x3b},
	{0x5328, 0x49},
	{0x5329, 0x61},
	{0x532a, 0x9c},
	{0x532b, 0xc9},
	{0x5335, 0x04},
	{0x5336, 0x00},
	{0x5337, 0x04},
	{0x5338, 0x00},
	{0x5339, 0x0b},
	{0x533a, 0x00},
	{0x53a4, 0x09},
	{0x53a5, 0x11},
	{0x53a6, 0x1f},
	{0x53a7, 0x3b},
	{0x53a8, 0x49},
	{0x53a9, 0x61},
	{0x53aa, 0x9c},
	{0x53ab, 0xc9},
	{0x53b5, 0x04},
	{0x53b6, 0x00},
	{0x53b7, 0x04},
	{0x53b8, 0x00},
	{0x53b9, 0x0b},
	{0x53ba, 0x00},
	{0x580b, 0x03},
	{0x580d, 0x00},
	{0x580f, 0x00},
	{0x5820, 0x00},
	{0x5821, 0x00},
	{0x5888, 0x01},
	{REG_NULL, 0x00},
};

static const struct os04e10_mode supported_modes[] = {
	{
		.width = 2048,
		.height = 2048,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0080,
		.hts_def = 0x0650 * 2,
		.vts_def = 0x0874,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = os04e10_linear_10_2048x2048_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
	{
		.width = 2048,
		.height = 2048,
		.max_fps = {
			.numerator = 10000,
			.denominator = 150000,
		},
		.exp_def = 0x0080,
		.hts_def = 0x0634 * 2,
		.vts_def = 0x089B,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = os04e10_hdrx2_10_2048x2048_15fps_regs,
		.hdr_mode = HDR_X2,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,//L->csi wr0
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,//M->csi wr2
	},
};

static const s64 link_freq_menu_items[] = {
	OS04E10_LINK_FREQ_900
};

static const char * const os04e10_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int os04e10_write_reg(struct i2c_client *client, u16 reg,
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

static int os04e10_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = os04e10_write_reg(client, regs[i].addr,
					OS04E10_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int os04e10_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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
static int os04e10_set_gain_reg(struct os04e10 *os04e10, u32 gain, int mode)
{
	struct i2c_client *client = os04e10->client;
	u32 again = 0, dgain = 0;
	int ret = 0;

	if (gain < OS04E10_GAIN_MIN)
		gain = OS04E10_GAIN_MIN;
	else if (gain > OS04E10_GAIN_MAX)
		gain = OS04E10_GAIN_MAX;

	if (gain < 0x7c0) {
		again = gain;
		dgain = 0x400;
	} else {
		again = 0x7c0;
		dgain = gain * 1024 / 0x7c0;
	}

	dev_dbg(&client->dev, "again: 0x%02x, dgain: 0x%02x mode %d\n",
		again, dgain, mode);

	if (mode == OS04E10_LGAIN) {
		ret = os04e10_write_reg(os04e10->client,
					OS04E10_REG_ANA_GAIN_H,
					OS04E10_REG_VALUE_08BIT,
					OS04E10_FETCH_AGAIN_H(again));
		ret |= os04e10_write_reg(os04e10->client,
					OS04E10_REG_ANA_GAIN_L,
					OS04E10_REG_VALUE_08BIT,
					OS04E10_FETCH_AGAIN_L(again));
		ret |= os04e10_write_reg(os04e10->client,
					 OS04E10_REG_DIG_GAIN_H,
					 OS04E10_REG_VALUE_08BIT,
					 OS04E10_FETCH_DGAIN_H(dgain));
		ret |= os04e10_write_reg(os04e10->client,
					 OS04E10_REG_DIG_GAIN_L,
					 OS04E10_REG_VALUE_08BIT,
					 OS04E10_FETCH_DGAIN_L(dgain));
	} else if (mode == OS04E10_SGAIN) {
		ret = os04e10_write_reg(os04e10->client,
					OS04E10_REG_ANA_GAIN_SHORT_H,
					OS04E10_REG_VALUE_08BIT,
					OS04E10_FETCH_AGAIN_H(again));
		ret |= os04e10_write_reg(os04e10->client,
					OS04E10_REG_ANA_GAIN_SHORT_L,
					OS04E10_REG_VALUE_08BIT,
					OS04E10_FETCH_AGAIN_L(again));
		ret |= os04e10_write_reg(os04e10->client,
					 OS04E10_REG_DIG_GAIN_SHORT_H,
					 OS04E10_REG_VALUE_08BIT,
					 OS04E10_FETCH_DGAIN_H(dgain));
		ret |= os04e10_write_reg(os04e10->client,
					 OS04E10_REG_DIG_GAIN_SHORT_L,
					 OS04E10_REG_VALUE_08BIT,
					 OS04E10_FETCH_DGAIN_L(dgain));
	}

	return ret;
}

static int os04e10_set_hdrae(struct os04e10 *os04e10,
			    struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;

	if (!os04e10->has_init_exp && !os04e10->streaming) {
		os04e10->init_hdrae_exp = *ae;
		os04e10->has_init_exp = true;
		dev_dbg(&os04e10->client->dev, "OS04E10 don't stream, record exp for hdr!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&os04e10->client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, l_a_gain, m_exp_time,
		m_a_gain, s_exp_time, s_a_gain);

	if (os04e10->cur_mode->hdr_mode == HDR_X2) {
		//2 stagger
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	// (max_l_time = vts - 8)
	if (l_exp_time > os04e10->cur_vts - 8)
		l_exp_time = os04e10->cur_vts - 8;
	// (max_s_time = vts - l_exp_time - 12)
	if (s_exp_time > os04e10->cur_vts - l_exp_time - 12)
		s_exp_time = os04e10->cur_vts - l_exp_time - 12;
	ret = os04e10_write_reg(os04e10->client,
				OS04E10_REG_EXPOSURE_H,
				OS04E10_REG_VALUE_08BIT,
				OS04E10_FETCH_EXP_H(l_exp_time));
	ret |= os04e10_write_reg(os04e10->client,
				 OS04E10_REG_EXPOSURE_M,
				 OS04E10_REG_VALUE_08BIT,
				 OS04E10_FETCH_EXP_M(l_exp_time));
	ret |= os04e10_write_reg(os04e10->client,
				 OS04E10_REG_EXPOSURE_L,
				 OS04E10_REG_VALUE_08BIT,
				 OS04E10_FETCH_EXP_L(l_exp_time));

	ret |= os04e10_write_reg(os04e10->client,
				OS04E10_REG_EXPOSURE_SHORT_H,
				OS04E10_REG_VALUE_08BIT,
				OS04E10_FETCH_EXP_H(s_exp_time));
	ret |= os04e10_write_reg(os04e10->client,
				 OS04E10_REG_EXPOSURE_SHORT_M,
				 OS04E10_REG_VALUE_08BIT,
				 OS04E10_FETCH_EXP_M(s_exp_time));
	ret |= os04e10_write_reg(os04e10->client,
				 OS04E10_REG_EXPOSURE_SHORT_L,
				 OS04E10_REG_VALUE_08BIT,
				 OS04E10_FETCH_EXP_L(s_exp_time));

	ret |= os04e10_set_gain_reg(os04e10, l_a_gain, OS04E10_LGAIN);
	ret |= os04e10_set_gain_reg(os04e10, s_a_gain, OS04E10_SGAIN);
	return ret;
}

static int os04e10_get_reso_dist(const struct os04e10_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct os04e10_mode *
os04e10_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = os04e10_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int os04e10_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct os04e10 *os04e10 = to_os04e10(sd);
	const struct os04e10_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&os04e10->mutex);

	mode = os04e10_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&os04e10->mutex);
		return -ENOTTY;
#endif
	} else {
		os04e10->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(os04e10->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(os04e10->vblank, vblank_def,
					 OS04E10_VTS_MAX - mode->height,
					 1, vblank_def);
		os04e10->cur_fps = mode->max_fps;
		os04e10->cur_vts = mode->vts_def;
	}

	mutex_unlock(&os04e10->mutex);

	return 0;
}

static int os04e10_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct os04e10 *os04e10 = to_os04e10(sd);
	const struct os04e10_mode *mode = os04e10->cur_mode;

	mutex_lock(&os04e10->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&os04e10->mutex);
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
	mutex_unlock(&os04e10->mutex);

	return 0;
}

static int os04e10_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct os04e10 *os04e10 = to_os04e10(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = os04e10->cur_mode->bus_fmt;

	return 0;
}

static int os04e10_enum_frame_sizes(struct v4l2_subdev *sd,
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

static int os04e10_enable_test_pattern(struct os04e10 *os04e10, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = os04e10_read_reg(os04e10->client, OS04E10_REG_TEST_PATTERN,
			       OS04E10_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= OS04E10_TEST_PATTERN_BIT_MASK;
	else
		val &= ~OS04E10_TEST_PATTERN_BIT_MASK;

	ret |= os04e10_write_reg(os04e10->client, OS04E10_REG_TEST_PATTERN,
				 OS04E10_REG_VALUE_08BIT, val);
	return ret;
}

static int os04e10_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct os04e10 *os04e10 = to_os04e10(sd);
	const struct os04e10_mode *mode = os04e10->cur_mode;

	if (os04e10->streaming)
		fi->interval = os04e10->cur_fps;
	else
		fi->interval = mode->max_fps;

	return 0;
}

static int os04e10_g_mbus_config(struct v4l2_subdev *sd,
				unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = OS04E10_LANES;

	return 0;
}

static void os04e10_get_module_inf(struct os04e10 *os04e10,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, OS04E10_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, os04e10->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, os04e10->len_name, sizeof(inf->base.lens));
}

static long os04e10_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct os04e10 *os04e10 = to_os04e10(sd);
	struct rkmodule_hdr_cfg *hdr;
	u32 i, h, w;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		os04e10_get_module_inf(os04e10, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = os04e10->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = os04e10->cur_mode->width;
		h = os04e10->cur_mode->height;
		for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode) {
				os04e10->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == ARRAY_SIZE(supported_modes)) {
			dev_err(&os04e10->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = os04e10->cur_mode->hts_def - os04e10->cur_mode->width;
			h = os04e10->cur_mode->vts_def - os04e10->cur_mode->height;
			__v4l2_ctrl_modify_range(os04e10->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(os04e10->vblank, h,
						 OS04E10_VTS_MAX - os04e10->cur_mode->height, 1, h);
			os04e10->cur_fps = os04e10->cur_mode->max_fps;
			os04e10->cur_vts = os04e10->cur_mode->vts_def;
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		os04e10_set_hdrae(os04e10, arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = os04e10_write_reg(os04e10->client, OS04E10_REG_CTRL_MODE,
				 OS04E10_REG_VALUE_08BIT, OS04E10_MODE_STREAMING);
		else
			ret = os04e10_write_reg(os04e10->client, OS04E10_REG_CTRL_MODE,
				 OS04E10_REG_VALUE_08BIT, OS04E10_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long os04e10_compat_ioctl32(struct v4l2_subdev *sd,
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

		ret = os04e10_ioctl(sd, cmd, inf);
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

		ret = os04e10_ioctl(sd, cmd, hdr);
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
			ret = os04e10_ioctl(sd, cmd, hdr);
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
			ret = os04e10_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = os04e10_ioctl(sd, cmd, &stream);
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

static int __os04e10_start_stream(struct os04e10 *os04e10)
{
	int ret;

	dev_info(&os04e10->client->dev,
		 "%dx%d@%d, mode %d, vts 0x%x\n",
		 os04e10->cur_mode->width,
		 os04e10->cur_mode->height,
		 os04e10->cur_fps.denominator / os04e10->cur_fps.numerator,
		 os04e10->cur_mode->hdr_mode,
		 os04e10->cur_vts);

	if (!os04e10->is_thunderboot) {
		ret = os04e10_write_array(os04e10->client, os04e10->cur_mode->reg_list);
		if (ret)
			return ret;
		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&os04e10->ctrl_handler);
		if (ret)
			return ret;
		if (os04e10->has_init_exp && os04e10->cur_mode->hdr_mode != NO_HDR) {
			ret = os04e10_ioctl(&os04e10->subdev, PREISP_CMD_SET_HDRAE_EXP,
				&os04e10->init_hdrae_exp);
			if (ret) {
				dev_err(&os04e10->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	return os04e10_write_reg(os04e10->client, OS04E10_REG_CTRL_MODE,
				OS04E10_REG_VALUE_08BIT, OS04E10_MODE_STREAMING);
}

static int __os04e10_stop_stream(struct os04e10 *os04e10)
{
	os04e10->has_init_exp = false;
	if (os04e10->is_thunderboot) {
		os04e10->is_first_streamoff = true;
		pm_runtime_put(&os04e10->client->dev);
	}
	return os04e10_write_reg(os04e10->client, OS04E10_REG_CTRL_MODE,
				 OS04E10_REG_VALUE_08BIT, OS04E10_MODE_SW_STANDBY);
}

static int __os04e10_power_on(struct os04e10 *os04e10);
static int os04e10_s_stream(struct v4l2_subdev *sd, int on)
{
	struct os04e10 *os04e10 = to_os04e10(sd);
	struct i2c_client *client = os04e10->client;
	int ret = 0;

	mutex_lock(&os04e10->mutex);
	on = !!on;
	if (on == os04e10->streaming)
		goto unlock_and_return;
	if (on) {
		if (os04e10->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			os04e10->is_thunderboot = false;
			__os04e10_power_on(os04e10);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __os04e10_start_stream(os04e10);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__os04e10_stop_stream(os04e10);
		pm_runtime_put(&client->dev);
	}

	os04e10->streaming = on;
unlock_and_return:
	mutex_unlock(&os04e10->mutex);
	return ret;
}

static int os04e10_s_power(struct v4l2_subdev *sd, int on)
{
	struct os04e10 *os04e10 = to_os04e10(sd);
	struct i2c_client *client = os04e10->client;
	int ret = 0;

	mutex_lock(&os04e10->mutex);

	/* If the power state is not modified - no work to do. */
	if (os04e10->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!os04e10->is_thunderboot) {
			ret = os04e10_write_array(os04e10->client, os04e10_global_regs);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		os04e10->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		os04e10->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&os04e10->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 os04e10_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, OS04E10_XVCLK_FREQ / 1000 / 1000);
}

static int __os04e10_power_on(struct os04e10 *os04e10)
{
	int ret;
	u32 delay_us;
	struct device *dev = &os04e10->client->dev;

	if (!IS_ERR_OR_NULL(os04e10->pins_default)) {
		ret = pinctrl_select_state(os04e10->pinctrl,
					   os04e10->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(os04e10->xvclk, OS04E10_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(os04e10->xvclk) != OS04E10_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(os04e10->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (os04e10->is_thunderboot)
		return 0;

	if (!IS_ERR(os04e10->reset_gpio))
		gpiod_set_value_cansleep(os04e10->reset_gpio, 0);

	ret = regulator_bulk_enable(OS04E10_NUM_SUPPLIES, os04e10->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(os04e10->reset_gpio))
		gpiod_set_value_cansleep(os04e10->reset_gpio, 1);

	usleep_range(500, 1000);

	if (!IS_ERR(os04e10->pwdn_gpio))
		gpiod_set_value_cansleep(os04e10->pwdn_gpio, 1);

	if (!IS_ERR(os04e10->reset_gpio))
		usleep_range(6000, 8000);
	else
		usleep_range(12000, 16000);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = os04e10_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(os04e10->xvclk);

	return ret;
}

static void __os04e10_power_off(struct os04e10 *os04e10)
{
	int ret;
	struct device *dev = &os04e10->client->dev;

	clk_disable_unprepare(os04e10->xvclk);
	if (os04e10->is_thunderboot) {
		if (os04e10->is_first_streamoff) {
			os04e10->is_thunderboot = false;
			os04e10->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(os04e10->pwdn_gpio))
		gpiod_set_value_cansleep(os04e10->pwdn_gpio, 0);
	clk_disable_unprepare(os04e10->xvclk);
	if (!IS_ERR(os04e10->reset_gpio))
		gpiod_set_value_cansleep(os04e10->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(os04e10->pins_sleep)) {
		ret = pinctrl_select_state(os04e10->pinctrl,
					   os04e10->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(OS04E10_NUM_SUPPLIES, os04e10->supplies);
}

static int os04e10_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct os04e10 *os04e10 = to_os04e10(sd);

	return __os04e10_power_on(os04e10);
}

static int os04e10_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct os04e10 *os04e10 = to_os04e10(sd);

	__os04e10_power_off(os04e10);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int os04e10_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct os04e10 *os04e10 = to_os04e10(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct os04e10_mode *def_mode = &supported_modes[0];

	mutex_lock(&os04e10->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&os04e10->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int os04e10_enum_frame_interval(struct v4l2_subdev *sd,
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

static const struct dev_pm_ops os04e10_pm_ops = {
	SET_RUNTIME_PM_OPS(os04e10_runtime_suspend, os04e10_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops os04e10_internal_ops = {
	.open = os04e10_open,
};
#endif

static const struct v4l2_subdev_core_ops os04e10_core_ops = {
	.s_power = os04e10_s_power,
	.ioctl = os04e10_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = os04e10_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops os04e10_video_ops = {
	.s_stream = os04e10_s_stream,
	.g_frame_interval = os04e10_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops os04e10_pad_ops = {
	.enum_mbus_code = os04e10_enum_mbus_code,
	.enum_frame_size = os04e10_enum_frame_sizes,
	.enum_frame_interval = os04e10_enum_frame_interval,
	.get_fmt = os04e10_get_fmt,
	.set_fmt = os04e10_set_fmt,
	.get_mbus_config = os04e10_g_mbus_config,
};

static const struct v4l2_subdev_ops os04e10_subdev_ops = {
	.core	= &os04e10_core_ops,
	.video	= &os04e10_video_ops,
	.pad	= &os04e10_pad_ops,
};

static void os04e10_modify_fps_info(struct os04e10 *os04e10)
{
	const struct os04e10_mode *mode = os04e10->cur_mode;

	os04e10->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				      os04e10->cur_vts;
}

static int os04e10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct os04e10 *os04e10 = container_of(ctrl->handler,
					       struct os04e10, ctrl_handler);
	struct i2c_client *client = os04e10->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = os04e10->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(os04e10->exposure,
					 os04e10->exposure->minimum, max,
					 os04e10->exposure->step,
					 os04e10->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		if (os04e10->cur_mode->hdr_mode == NO_HDR) {
			val = ctrl->val;
			/* 4 least significant bits of expsoure are fractional part */
			ret = os04e10_write_reg(os04e10->client,
						OS04E10_REG_EXPOSURE_H,
						OS04E10_REG_VALUE_08BIT,
						OS04E10_FETCH_EXP_H(val));
			ret |= os04e10_write_reg(os04e10->client,
						 OS04E10_REG_EXPOSURE_M,
						 OS04E10_REG_VALUE_08BIT,
						 OS04E10_FETCH_EXP_M(val));
			ret |= os04e10_write_reg(os04e10->client,
						 OS04E10_REG_EXPOSURE_L,
						 OS04E10_REG_VALUE_08BIT,
						 OS04E10_FETCH_EXP_L(val));
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev,
			"set gain 0x%x mode %d\n",
			ctrl->val,
			os04e10->cur_mode->hdr_mode);
		if (os04e10->cur_mode->hdr_mode == NO_HDR)
			ret = os04e10_set_gain_reg(os04e10, ctrl->val, OS04E10_LGAIN);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		ret = os04e10_write_reg(os04e10->client,
					OS04E10_REG_VTS_H,
					OS04E10_REG_VALUE_08BIT,
					((ctrl->val + os04e10->cur_mode->height)
					>> 8) & 0x7F);
		ret |= os04e10_write_reg(os04e10->client,
					 OS04E10_REG_VTS_L,
					 OS04E10_REG_VALUE_08BIT,
					 (ctrl->val + os04e10->cur_mode->height)
					 & 0xff);
		os04e10->cur_vts = ctrl->val + os04e10->cur_mode->height;
		os04e10_modify_fps_info(os04e10);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = os04e10_enable_test_pattern(os04e10, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		dev_dbg(&client->dev, "set hflip 0x%x\n", ctrl->val);
		os04e10_read_reg(os04e10->client,
				 OS04E10_FLIP_MIRROR_REG,
				 OS04E10_REG_VALUE_08BIT,
				 &val);
		os04e10_write_reg(os04e10->client, 0x0100, OS04E10_REG_VALUE_08BIT, 0x00);
		os04e10_write_reg(os04e10->client, 0x3716, OS04E10_REG_VALUE_08BIT, 0x24);
		os04e10_write_reg(os04e10->client,
				  OS04E10_FLIP_MIRROR_REG,
				  OS04E10_REG_VALUE_08BIT,
				  OS04E10_FETCH_MIRROR(val, ctrl->val));
		os04e10_write_reg(os04e10->client, 0x0100, OS04E10_REG_VALUE_08BIT, 0x01);
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(&client->dev, "set vflip 0x%x\n", ctrl->val);
		os04e10_read_reg(os04e10->client,
				 OS04E10_FLIP_MIRROR_REG,
				 OS04E10_REG_VALUE_08BIT,
				 &val);
		os04e10_write_reg(os04e10->client, 0x0100, OS04E10_REG_VALUE_08BIT, 0x00);
		os04e10_write_reg(os04e10->client,
				  0x3716,
				  OS04E10_REG_VALUE_08BIT,
				  ctrl->val ? 0x04 : 0x24);
		os04e10_write_reg(os04e10->client,
				  OS04E10_FLIP_MIRROR_REG,
				  OS04E10_REG_VALUE_08BIT,
				  OS04E10_FETCH_FLIP(val, ctrl->val));
		os04e10_write_reg(os04e10->client, 0x0100, OS04E10_REG_VALUE_08BIT, 0x01);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops os04e10_ctrl_ops = {
	.s_ctrl = os04e10_set_ctrl,
};

static int os04e10_initialize_controls(struct os04e10 *os04e10)
{
	const struct os04e10_mode *mode;
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *ctrl;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &os04e10->ctrl_handler;
	mode = os04e10->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &os04e10->mutex;

	ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, link_freq_menu_items);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, PIXEL_RATE_WITH_900M_10BIT, 1, PIXEL_RATE_WITH_900M_10BIT);

	h_blank = mode->hts_def - mode->width;
	os04e10->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					    h_blank, h_blank, 1, h_blank);
	if (os04e10->hblank)
		os04e10->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	os04e10->vblank = v4l2_ctrl_new_std(handler, &os04e10_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_def,
					    OS04E10_VTS_MAX - mode->height,
					    1, vblank_def);
	exposure_max = mode->vts_def - 8;
	os04e10->exposure = v4l2_ctrl_new_std(handler, &os04e10_ctrl_ops,
					      V4L2_CID_EXPOSURE, OS04E10_EXPOSURE_MIN,
					      exposure_max, OS04E10_EXPOSURE_STEP,
					      mode->exp_def);
	os04e10->anal_gain = v4l2_ctrl_new_std(handler, &os04e10_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN, OS04E10_GAIN_MIN,
					       OS04E10_GAIN_MAX, OS04E10_GAIN_STEP,
					       OS04E10_GAIN_DEFAULT);
	os04e10->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							    &os04e10_ctrl_ops,
					V4L2_CID_TEST_PATTERN,
					ARRAY_SIZE(os04e10_test_pattern_menu) - 1,
					0, 0, os04e10_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &os04e10_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &os04e10_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&os04e10->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	os04e10->subdev.ctrl_handler = handler;
	os04e10->has_init_exp = false;
	os04e10->cur_fps = mode->max_fps;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int os04e10_check_sensor_id(struct os04e10 *os04e10,
				   struct i2c_client *client)
{
	struct device *dev = &os04e10->client->dev;
	u32 id = 0;
	int ret;

	if (os04e10->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = os04e10_read_reg(client, OS04E10_REG_CHIP_ID,
			       OS04E10_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected OV%06x sensor\n", CHIP_ID);

	return 0;
}

static int os04e10_configure_regulators(struct os04e10 *os04e10)
{
	unsigned int i;

	for (i = 0; i < OS04E10_NUM_SUPPLIES; i++)
		os04e10->supplies[i].supply = os04e10_supply_names[i];

	return devm_regulator_bulk_get(&os04e10->client->dev,
				       OS04E10_NUM_SUPPLIES,
				       os04e10->supplies);
}

#ifdef CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP
static void find_terminal_resolution(struct os04e10 *os04e10)
{
	int i = 0;
	const struct os04e10_mode *mode = NULL;
	const struct os04e10_mode *fit_mode = NULL;
	u32 cur_fps = 0;
	u32 dst_fps = 0;
	u32 tmp_fps = 0;
	u32 rk_cam_hdr, rk_cam_w, rk_cam_h, rk_cam_fps;

	rk_cam_hdr = get_rk_cam_hdr();
	rk_cam_w = get_rk_cam_w();
	rk_cam_h = get_rk_cam_h();
	rk_cam_fps = get_rk_cam_fps();

	if (rk_cam_w == 0 || rk_cam_h == 0 ||
	    rk_cam_fps == 0)
		goto err_find_res;

	dev_info(&os04e10->client->dev, "find resolution width: %d, height: %d, hdr: %d, fps: %d\n",
		 rk_cam_w, rk_cam_h, rk_cam_hdr, rk_cam_fps);
	dst_fps = rk_cam_fps;
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		mode = &supported_modes[i];
		cur_fps = mode->max_fps.denominator / mode->max_fps.numerator;
		if (mode->width == rk_cam_w && mode->height == rk_cam_h &&
		    mode->hdr_mode == rk_cam_hdr) {
			if (cur_fps == dst_fps) {
				os04e10->cur_mode = mode;
				return;
			}
			if (cur_fps >= dst_fps) {
				if (fit_mode) {
					tmp_fps = fit_mode->max_fps.denominator /
						  fit_mode->max_fps.numerator;
					if (tmp_fps - dst_fps > cur_fps - dst_fps)
						fit_mode = mode;
				} else {
					fit_mode = mode;
				}
			}
		}
	}
	if (fit_mode) {
		os04e10->cur_mode = fit_mode;
		return;
	}
err_find_res:
	dev_err(&os04e10->client->dev, "not match %dx%d@%dfps mode %d\n!",
		rk_cam_w, rk_cam_h, dst_fps, rk_cam_hdr);
	os04e10->cur_mode = &supported_modes[0];
}
#else
static void find_terminal_resolution(struct os04e10 *os04e10)
{
	u32 hdr_mode = 0;
	struct device_node *node = os04e10->client->dev.of_node;
	int i = 0;

	of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			os04e10->cur_mode = &supported_modes[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(supported_modes))
		os04e10->cur_mode = &supported_modes[0];

}
#endif

static int os04e10_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct os04e10 *os04e10;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	os04e10 = devm_kzalloc(dev, sizeof(*os04e10), GFP_KERNEL);
	if (!os04e10)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &os04e10->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &os04e10->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &os04e10->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &os04e10->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	os04e10->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	os04e10->client = client;

	find_terminal_resolution(os04e10);

	os04e10->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(os04e10->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	os04e10->reset_gpio = devm_gpiod_get(dev, "reset",
					     os04e10->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(os04e10->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	os04e10->pwdn_gpio = devm_gpiod_get(dev, "pwdn",
					    os04e10->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(os04e10->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	os04e10->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(os04e10->pinctrl)) {
		os04e10->pins_default =
			pinctrl_lookup_state(os04e10->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(os04e10->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		os04e10->pins_sleep =
			pinctrl_lookup_state(os04e10->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(os04e10->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = os04e10_configure_regulators(os04e10);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&os04e10->mutex);

	sd = &os04e10->subdev;
	v4l2_i2c_subdev_init(sd, client, &os04e10_subdev_ops);
	ret = os04e10_initialize_controls(os04e10);
	if (ret)
		goto err_destroy_mutex;

	ret = __os04e10_power_on(os04e10);
	if (ret)
		goto err_free_handler;

	ret = os04e10_check_sensor_id(os04e10, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &os04e10_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	os04e10->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &os04e10->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(os04e10->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 os04e10->module_index, facing,
		 OS04E10_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (os04e10->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__os04e10_power_off(os04e10);
err_free_handler:
	v4l2_ctrl_handler_free(&os04e10->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&os04e10->mutex);

	return ret;
}

static void os04e10_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct os04e10 *os04e10 = to_os04e10(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&os04e10->ctrl_handler);
	mutex_destroy(&os04e10->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__os04e10_power_off(os04e10);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id os04e10_of_match[] = {
	{ .compatible = "OmniVision,os04e10" },
	{},
};
MODULE_DEVICE_TABLE(of, os04e10_of_match);
#endif

static const struct i2c_device_id os04e10_match_id[] = {
	{ "OmniVision,os04e10", 0 },
	{ },
};

static struct i2c_driver os04e10_i2c_driver = {
	.driver = {
		.name = OS04E10_NAME,
		.pm = &os04e10_pm_ops,
		.of_match_table = of_match_ptr(os04e10_of_match),
	},
	.probe		= &os04e10_probe,
	.remove		= &os04e10_remove,
	.id_table	= os04e10_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&os04e10_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&os04e10_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("OmniVision os04e10 sensor driver");
MODULE_LICENSE("GPL");
