// SPDX-License-Identifier: GPL-2.0
/*
 * ov8856 camera driver
 * Copyright (C) 2021 Rockchip Electronics Co., Ltd.
 *
 * v0.1.0x00 : first driver version.
 * V0.0X01.0X01 fix bus format.
 * V0.0X01.0X02 support mirror & flip ctrl.
 *
 */

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
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>

#include <media/v4l2-async.h>
#include <media/media-entity.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>
#include "otp_eeprom.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x02)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif
/*MIPI_FREQ * 2 * lane / bit*/
#define OV8856_PIXEL_RATE		(360000000LL * 2LL * 4LL / 10LL)

#define MIPI_FREQ			360000000U
#define OV8856_XVCLK_FREQ		24000000

#define CHIP_ID				0x00885a
#define OV8856_REG_CHIP_ID		0x300a

#define OV8856_REG_CTRL_MODE		0x0100
#define OV8856_MODE_SW_STANDBY		0x0
#define OV8856_MODE_STREAMING		0x1

#define OV8856_REG_EXPOSURE		0x3500
#define	OV8856_EXPOSURE_MIN		6
#define	OV8856_EXPOSURE_STEP		1
#define OV8856_VTS_MAX			0x7fff

#define OV8856_REG_GAIN_H		0x3508
#define OV8856_REG_GAIN_L		0x3509
#define OV8856_GAIN_H_MASK		0x1f
#define OV8856_GAIN_H_SHIFT		8
#define OV8856_GAIN_L_MASK		0xff
#define OV8856_GAIN_MIN			0x80
#define OV8856_GAIN_MAX			0x7c0
#define OV8856_GAIN_STEP		1
#define OV8856_GAIN_DEFAULT		0x80

#define OV8856_REG_TEST_PATTERN		0x5e00
#define	OV8856_TEST_PATTERN_ENABLE	0x80
#define	OV8856_TEST_PATTERN_DISABLE	0x0

#define OV8856_REG_VTS			0x380e

#define OV8856_MIRROR_REG		0x3820
#define OV8856_FLIP_REG			0x3821
#define MIRROR_BIT_MASK			0x06
#define FLIP_BIT_MASK			0x06

#define OV8856_ISPCTRL1_REG		0x5001
#define OV8856_ISPCTRL2E_REG		0x502e

#define ISPCTRL1_BIT_MASK		BIT(2)
#define ISPCTRL2E_BIT_MASK		(BIT(1)|BIT(0))

#define REG_NULL			0xFFFF

#define OV8856_REG_VALUE_08BIT		1
#define OV8856_REG_VALUE_16BIT		2
#define OV8856_REG_VALUE_24BIT		3

#define OV8856_LANES			4
#define OV8856_BITS_PER_SAMPLE		10

#define OV8856_CHIP_REVISION_REG	0x302A

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define OV8856_NAME			"ov8856"
#define OV8856_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SBGGR10_1X10
#define MIRROR   1
static const struct regval *ov8856_global_regs;

static const char * const ov8856_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define OV8856_NUM_SUPPLIES ARRAY_SIZE(ov8856_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct ov8856_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list;
};

struct ov8856 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[OV8856_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*h_flip;
	struct v4l2_ctrl	*v_flip;
	struct mutex		mutex;
	bool			streaming;
	const struct ov8856_mode *cur_mode;
	unsigned int		lane_num;
	unsigned int		cfg_num;
	unsigned int		pixel_rate;
	bool			power_on;
	struct otp_info		*otp;

	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	struct rkmodule_inf	module_inf;
	struct rkmodule_awb_cfg	awb_cfg;
	struct rkmodule_lsc_cfg	lsc_cfg;
};

#define to_ov8856(sd) container_of(sd, struct ov8856, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval ov8856_global_regs_2lane[] = {
	{0x0103, 0x01},
	{0x0302, 0x3c},
	{0x0303, 0x01},
	{0x031e, 0x0c},
	{0x3000, 0x00},
	{0x300e, 0x00},
	{0x3010, 0x00},
	{0x3015, 0x84},
	{0x3018, 0x32},
	{0x3033, 0x24},
	{0x3500, 0x00},
	{0x3501, 0x4c},
	{0x3502, 0xe0},
	{0x3503, 0x08},
	{0x3505, 0x83},
	{0x3508, 0x01},
	{0x3509, 0x80},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x02},
	{0x3512, 0x00},
	{0x3600, 0x72},
	{0x3601, 0x40},
	{0x3602, 0x30},
	{0x3610, 0xc5},
	{0x3611, 0x58},
	{0x3612, 0x5c},
	{0x3613, 0x5a},
	{0x3614, 0x60},
	{0x3628, 0xff},
	{0x3629, 0xff},
	{0x362a, 0xff},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x10},
	{0x3663, 0x08},
	{0x3669, 0x34},
	{0x366e, 0x08},
	{0x3706, 0x86},
	{0x370b, 0x7e},
	{0x3714, 0x27},
	{0x3730, 0x12},
	{0x3733, 0x10},
	{0x3764, 0x00},
	{0x3765, 0x00},
	{0x3769, 0x62},
	{0x376a, 0x2a},
	{0x3780, 0x00},
	{0x3781, 0x24},
	{0x3782, 0x00},
	{0x3783, 0x23},
	{0x3798, 0x2f},
	{0x37a1, 0x60},
	{0x37a8, 0x6a},
	{0x37ab, 0x3f},
	{0x37c2, 0x14},
	{0x37c3, 0xf1},
	{0x37c9, 0x80},
	{0x37cb, 0x03},
	{0x37cc, 0x0a},
	{0x37cd, 0x16},
	{0x37ce, 0x1f},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x0e},
	{0x380d, 0xa0},
	{0x380e, 0x04},
	{0x380f, 0xde},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x03},
	{0x3815, 0x01},
	{0x3816, 0x00},
	{0x3817, 0x00},
	{0x3818, 0x00},
	{0x3819, 0x00},
	{0x382a, 0x03},
	{0x382b, 0x01},
	{0x3830, 0x06},
	{0x3836, 0x02},
	{0x3862, 0x04},
	{0x3863, 0x08},
	{0x3cc0, 0x33},
	{0x3d85, 0x17},
	{0x3d8c, 0x73},
	{0x3d8d, 0xde},
	{0x4001, 0xe0},
	{0x4003, 0x40},
	{0x4008, 0x00},
	{0x4009, 0x05},
	{0x400f, 0x80},
	{0x4010, 0xf0},
	{0x4011, 0xff},
	{0x4012, 0x02},
	{0x4013, 0x01},
	{0x4014, 0x01},
	{0x4015, 0x01},
	{0x4042, 0x00},
	{0x4043, 0x80},
	{0x4044, 0x00},
	{0x4045, 0x80},
	{0x4046, 0x00},
	{0x4047, 0x80},
	{0x4048, 0x00},
	{0x4049, 0x80},
	{0x4041, 0x03},
	{0x404c, 0x20},
	{0x404d, 0x00},
	{0x404e, 0x20},
	{0x4203, 0x80},
	{0x4307, 0x30},
	{0x4317, 0x00},
	{0x4503, 0x08},
	{0x4601, 0x80},
	{0x4816, 0x53},
	{0x481b, 0x58},
	{0x481f, 0x27},
	{0x4837, 0x16},
	{0x5000, 0x77},
	{0x5030, 0x41},
	{0x5795, 0x00},
	{0x5796, 0x10},
	{0x5797, 0x10},
	{0x5798, 0x73},
	{0x5799, 0x73},
	{0x579a, 0x00},
	{0x579b, 0x28},
	{0x579c, 0x00},
	{0x579d, 0x16},
	{0x579e, 0x06},
	{0x579f, 0x20},
	{0x57a0, 0x04},
	{0x57a1, 0xa0},
	{0x5780, 0x14},
	{0x5781, 0x0f},
	{0x5782, 0x44},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x01},
	{0x5786, 0x00},
	{0x5787, 0x04},
	{0x5788, 0x02},
	{0x5789, 0x0f},
	{0x578a, 0xfd},
	{0x578b, 0xf5},
	{0x578c, 0xf5},
	{0x578d, 0x03},
	{0x578e, 0x08},
	{0x578f, 0x0c},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x52},
	{0x5794, 0xa3},
	{0x5a08, 0x02},
	{0x5b00, 0x02},
	{0x5b01, 0x10},
	{0x5b02, 0x03},
	{0x5b03, 0xcf},
	{0x5b05, 0x6c},
	{0x5e00, 0x00},
	{0x3820, 0x90},
	{0x3821, 0x67},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x04},
	{0x376b, 0x30},
	//{0x0100, 0x01},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 720Mbps
 */
static const struct regval ov8856_1632x1224_30fps_regs_2lane[] = {
	{0x0100, 0x00},
	{0x0302, 0x3c},
	{0x0303, 0x01},
	{0x3501, 0x4c},
	{0x3502, 0xe0},
	{0x366e, 0x08},
	{0x3714, 0x27},
	{0x37c2, 0x14},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x0e},
	{0x380d, 0xa0},
	{0x380e, 0x04},
	{0x380f, 0xde},
	{0x3810, 0x00},
	{0x3811, 0x02},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x03},
	{0x382a, 0x03},
	{0x4009, 0x05},
	{0x4837, 0x16},
	{0x4601, 0x80},
	{0x5795, 0x00},
	{0x5796, 0x10},
	{0x5797, 0x10},
	{0x5798, 0x73},
	{0x5799, 0x73},
	{0x579a, 0x00},
	{0x579b, 0x28},
	{0x579c, 0x00},
	{0x579d, 0x16},
	{0x579f, 0x20},
	{0x57a0, 0x04},
	{0x57a1, 0xa0},
	{0x3820, 0x90},
	{0x3821, 0x67},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x04},
	{0x376b, 0x30},
	//{0x0100, 0x01},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 720Mbps
 */
static const struct regval ov8856_3264x2448_30fps_regs_2lane[] = {
	{0x0100, 0x00},
	{0x0302, 0x35},
	{0x0303, 0x00},
	{0x3501, 0x9a},
	{0x3502, 0x20},
	{0x366e, 0x10},
	{0x3714, 0x23},
	{0x37c2, 0x04},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x07},
	{0x380d, 0x8c},
	{0x380e, 0x09},
	{0x380f, 0xb2},
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x01},
	{0x382a, 0x01},
	{0x4009, 0x0b},
	{0x4837, 0x0c},
	{0x4601, 0x80},
	{0x5795, 0x02},
	{0x5796, 0x20},
	{0x5797, 0x20},
	{0x5798, 0xd5},
	{0x5799, 0xd5},
	{0x579a, 0x00},
	{0x579b, 0x50},
	{0x579c, 0x00},
	{0x579d, 0x2c},
	{0x579f, 0x40},
	{0x57a0, 0x09},
	{0x57a1, 0x40},
	{0x3820, 0x80},
	{0x3821, 0x46},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x04},
	{0x376b, 0x30},
	{0x0100, 0x01},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 */
static const struct regval ov8856_global_regs_4lane[] = {
	{0x0103, 0x01},
	{0x0302, 0x3c},
	{0x0303, 0x01},
	{0x031e, 0x0c},
	{0x3000, 0x00},
	{0x300e, 0x00},
	{0x3010, 0x00},
	{0x3015, 0x84},
	{0x3018, 0x72},
	{0x3033, 0x24},
	{0x3500, 0x00},
	{0x3501, 0x4c},
	{0x3502, 0xe0},
	{0x3503, 0x08},
	{0x3505, 0x83},
	{0x3508, 0x01},
	{0x3509, 0x80},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x02},
	{0x3512, 0x00},
	{0x3600, 0x72},
	{0x3601, 0x40},
	{0x3602, 0x30},
	{0x3610, 0xc5},
	{0x3611, 0x58},
	{0x3612, 0x5c},
	{0x3613, 0x5a},
	{0x3614, 0x60},
	{0x3628, 0xff},
	{0x3629, 0xff},
	{0x362a, 0xff},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x10},
	{0x3663, 0x08},
	{0x3669, 0x34},
	{0x366e, 0x08},
	{0x3706, 0x86},
	{0x370b, 0x7e},
	{0x3714, 0x27},
	{0x3730, 0x12},
	{0x3733, 0x10},
	{0x3764, 0x00},
	{0x3765, 0x00},
	{0x3769, 0x62},
	{0x376a, 0x2a},
	{0x3780, 0x00},
	{0x3781, 0x24},
	{0x3782, 0x00},
	{0x3783, 0x23},
	{0x3798, 0x2f},
	{0x37a1, 0x60},
	{0x37a8, 0x6a},
	{0x37ab, 0x3f},
	{0x37c2, 0x14},
	{0x37c3, 0xf1},
	{0x37c9, 0x80},
	{0x37cb, 0x03},
	{0x37cc, 0x0a},
	{0x37cd, 0x16},
	{0x37ce, 0x1f},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x07},
	{0x380d, 0x8c},
	{0x380e, 0x09},
	{0x380f, 0xb2},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x03},
	{0x3815, 0x01},
	{0x3816, 0x00},
	{0x3817, 0x00},
	{0x3818, 0x00},
	{0x3819, 0x00},
	{0x382a, 0x03},
	{0x382b, 0x01},
	{0x3830, 0x06},
	{0x3836, 0x02},
	{0x3862, 0x04},
	{0x3863, 0x08},
	{0x3cc0, 0x33},
	{0x3d85, 0x17},
	{0x3d8c, 0x73},
	{0x3d8d, 0xde},
	{0x4001, 0xe0},
	{0x4003, 0x40},
	{0x4008, 0x00},
	{0x4009, 0x05},
	{0x400f, 0x80},
	{0x4010, 0xf0},
	{0x4011, 0xff},
	{0x4012, 0x02},
	{0x4013, 0x01},
	{0x4014, 0x01},
	{0x4015, 0x01},
	{0x4042, 0x00},
	{0x4043, 0x80},
	{0x4044, 0x00},
	{0x4045, 0x80},
	{0x4046, 0x00},
	{0x4047, 0x80},
	{0x4048, 0x00},
	{0x4049, 0x80},
	{0x4041, 0x03},
	{0x404c, 0x20},
	{0x404d, 0x00},
	{0x404e, 0x20},
	{0x4203, 0x80},
	{0x4307, 0x30},
	{0x4317, 0x00},
	{0x4503, 0x08},
	{0x4601, 0x80},
	{0x4816, 0x53},
	{0x481b, 0x58},
	{0x481f, 0x27},
	{0x4837, 0x16},
	{0x5000, 0x77},
	{0x5030, 0x41},
	{0x5795, 0x00},
	{0x5796, 0x10},
	{0x5797, 0x10},
	{0x5798, 0x73},
	{0x5799, 0x73},
	{0x579a, 0x00},
	{0x579b, 0x28},
	{0x579c, 0x00},
	{0x579d, 0x16},
	{0x579e, 0x06},
	{0x579f, 0x20},
	{0x57a0, 0x04},
	{0x57a1, 0xa0},
	{0x5780, 0x14},
	{0x5781, 0x0f},
	{0x5782, 0x44},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x01},
	{0x5786, 0x00},
	{0x5787, 0x04},
	{0x5788, 0x02},
	{0x5789, 0x0f},
	{0x578a, 0xfd},
	{0x578b, 0xf5},
	{0x578c, 0xf5},
	{0x578d, 0x03},
	{0x578e, 0x08},
	{0x578f, 0x0c},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x52},
	{0x5794, 0xa3},
	{0x5a08, 0x02},
	{0x5b00, 0x02},
	{0x5b01, 0x10},
	{0x5b02, 0x03},
	{0x5b03, 0xcf},
	{0x5b05, 0x6c},
	{0x5e00, 0x00},
	{0x3820, 0x90},
	{0x3821, 0x67},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x04},
	{0x376b, 0x30},
	//{0x0100, 0x01},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 720Mbps
 */
static const struct regval ov8856_3264x2448_30fps_regs_4lane[] = {
	{0x0100, 0x00},
	{0x3501, 0x9a},
	{0x3502, 0x20},
	{0x366e, 0x10},
	{0x3714, 0x23},
	{0x37c2, 0x04},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x07},
	{0x380d, 0x8c},
	{0x380e, 0x09},
	{0x380f, 0xb2},
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x01},
	{0x382a, 0x01},
	{0x4009, 0x0b},
	{0x4601, 0x80},
	{0x5795, 0x02},
	{0x5796, 0x20},
	{0x5797, 0x20},
	{0x5798, 0xd5},
	{0x5799, 0xd5},
	{0x579a, 0x00},
	{0x579b, 0x50},
	{0x579c, 0x00},
	{0x579d, 0x2c},
	{0x579e, 0x0c},
	{0x579f, 0x40},
	{0x57a0, 0x09},
	{0x57a1, 0x40},
	{0x3820, 0x80},
	{0x3821, 0x46},
	{0x502e, 0x03},
	{0x5001, 0x02},
	{0x5004, 0x04},
	{0x376b, 0x30},
	//{0x0100, 0x01},
	{REG_NULL, 0x00},
};


static const struct ov8856_mode supported_modes_2lane[] = {
	{
		.width = 3264,
		.height = 2448,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x09ac,
		.hts_def = 0x078c * 2,
		.vts_def = 0x09b2,
		.reg_list = ov8856_3264x2448_30fps_regs_2lane,
	},

	{
		.width = 1632,
		.height = 1224,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x04d0,
		.hts_def = 0x0ea0,
		.vts_def = 0x04de,
		.reg_list = ov8856_1632x1224_30fps_regs_2lane,
	},

};

static const struct ov8856_mode supported_modes_4lane[] = {
	{
		.width = 3264,
		.height = 2448,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x09ac,
		.hts_def = 0x078c * 2,
		.vts_def = 0x0A00,
		.reg_list = ov8856_3264x2448_30fps_regs_4lane,
	},
};

static const struct ov8856_mode *supported_modes;

static const s64 link_freq_menu_items[] = {
	MIPI_FREQ
};

static const char * const ov8856_test_pattern_menu[] = {
	"Disabled",
	"Standard Color Bar",
	"Top-Bottom Darker Color Bar",
	"Right-Left Darker Color Bar",
	"Bottom-Top Darker Color Bar"
};

/* Write registers up to 4 at a time */
static int ov8856_write_reg(struct i2c_client *client, u16 reg,
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

	if (i2c_master_send(client, buf, len + 2) != len + 2) {
		dev_err(&client->dev, "write reg(0x%x val:0x%x)! failed !\n", reg, val);
		return -EIO;
	}

	return 0;
}

static int ov8856_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = ov8856_write_reg(client, regs[i].addr,
					OV8856_REG_VALUE_08BIT,
					regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int ov8856_read_reg(struct i2c_client *client, u16 reg,
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
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "read reg:0x%x failed !\n", reg);
		return -EIO;
	}

	*val = be32_to_cpu(data_be);
	dev_dbg(&client->dev, "read reg:0x%x value(0x%x) !\n", reg, *val);

	return 0;
}

static int ov8856_get_reso_dist(const struct ov8856_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct ov8856_mode *
ov8856_find_best_fit(struct ov8856 *ov8856,
		     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ov8856->cfg_num; i++) {
		dist = ov8856_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int ov8856_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	const struct ov8856_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&ov8856->mutex);

	mode = ov8856_find_best_fit(ov8856, fmt);
	fmt->format.code = OV8856_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&ov8856->mutex);
		return -ENOTTY;
#endif
	} else {
		ov8856->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(ov8856->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov8856->vblank, vblank_def,
					 OV8856_VTS_MAX - mode->height,
					 1, vblank_def);
	}

	mutex_unlock(&ov8856->mutex);

	return 0;
}

static int ov8856_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	const struct ov8856_mode *mode = ov8856->cur_mode;

	mutex_lock(&ov8856->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&ov8856->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = OV8856_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&ov8856->mutex);

	return 0;
}

static int ov8856_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = OV8856_MEDIA_BUS_FMT;

	return 0;
}

static int ov8856_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct ov8856 *ov8856 = to_ov8856(sd);

	if (fse->index >= ov8856->cfg_num)
		return -EINVAL;

	if (fse->code != OV8856_MEDIA_BUS_FMT)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int ov8856_enable_test_pattern(struct ov8856 *ov8856, u32 pattern)
{
	u32 val;

	if (pattern)
		val = (pattern - 1) | OV8856_TEST_PATTERN_ENABLE;
	else
		val = OV8856_TEST_PATTERN_DISABLE;

	return ov8856_write_reg(ov8856->client,
				 OV8856_REG_TEST_PATTERN,
				 OV8856_REG_VALUE_08BIT,
				 val);
}

static int ov8856_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	const struct ov8856_mode *mode = ov8856->cur_mode;
//	printk("%s  yyk\n", __func__);
	mutex_lock(&ov8856->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&ov8856->mutex);

	return 0;
}

static void ov8856_get_otp(struct otp_info *otp,
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
		inf->pdaf.pd_offset = otp->pdaf_data.pd_offset;
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

static void ov8856_get_module_inf(struct ov8856 *ov8856,
				  struct rkmodule_inf *inf)
{
	struct otp_info *otp = ov8856->otp;

	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, OV8856_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, ov8856->module_name, sizeof(inf->base.module));
	strscpy(inf->base.lens, ov8856->len_name, sizeof(inf->base.lens));
	if (otp)
		ov8856_get_otp(otp, inf);

}

static void ov8856_set_awb_cfg(struct ov8856 *ov8856,
			       struct rkmodule_awb_cfg *cfg)
{
	mutex_lock(&ov8856->mutex);
	memcpy(&ov8856->awb_cfg, cfg, sizeof(*cfg));
	mutex_unlock(&ov8856->mutex);
}

static void ov8856_set_lsc_cfg(struct ov8856 *ov8856,
			       struct rkmodule_lsc_cfg *cfg)
{
	mutex_lock(&ov8856->mutex);
	memcpy(&ov8856->lsc_cfg, cfg, sizeof(*cfg));
	mutex_unlock(&ov8856->mutex);
}

static long ov8856_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		ov8856_get_module_inf(ov8856, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_AWB_CFG:
		ov8856_set_awb_cfg(ov8856, (struct rkmodule_awb_cfg *)arg);
		break;
	case RKMODULE_LSC_CFG:
		ov8856_set_lsc_cfg(ov8856, (struct rkmodule_lsc_cfg *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = ov8856_write_reg(ov8856->client,
				OV8856_REG_CTRL_MODE,
				OV8856_REG_VALUE_08BIT,
				OV8856_MODE_STREAMING);
		else
			ret = ov8856_write_reg(ov8856->client,
				OV8856_REG_CTRL_MODE,
				OV8856_REG_VALUE_08BIT,
				OV8856_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ov8856_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *awb_cfg;
	struct rkmodule_lsc_cfg *lsc_cfg;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = ov8856_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		awb_cfg = kzalloc(sizeof(*awb_cfg), GFP_KERNEL);
		if (!awb_cfg) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(awb_cfg, up, sizeof(*awb_cfg));
		if (!ret)
			ret = ov8856_ioctl(sd, cmd, awb_cfg);
		else
			ret = -EFAULT;
		kfree(awb_cfg);
		break;
	case RKMODULE_LSC_CFG:
		lsc_cfg = kzalloc(sizeof(*lsc_cfg), GFP_KERNEL);
		if (!lsc_cfg) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(lsc_cfg, up, sizeof(*lsc_cfg));
		if (!ret)
			ret = ov8856_ioctl(sd, cmd, lsc_cfg);
		else
			ret = -EFAULT;
		kfree(lsc_cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = ov8856_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}
#endif

static int __ov8856_start_stream(struct ov8856 *ov8856)
{
	int ret;

	ret = ov8856_write_array(ov8856->client, ov8856->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&ov8856->mutex);
	ret = v4l2_ctrl_handler_setup(&ov8856->ctrl_handler);
	mutex_lock(&ov8856->mutex);
	if (ret)
		return ret;

	return ov8856_write_reg(ov8856->client,
				OV8856_REG_CTRL_MODE,
				OV8856_REG_VALUE_08BIT,
				OV8856_MODE_STREAMING);
}

static int __ov8856_stop_stream(struct ov8856 *ov8856)
{
	return ov8856_write_reg(ov8856->client,
				OV8856_REG_CTRL_MODE,
				OV8856_REG_VALUE_08BIT,
				OV8856_MODE_SW_STANDBY);
}

static int ov8856_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	struct i2c_client *client = ov8856->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				ov8856->cur_mode->width,
				ov8856->cur_mode->height,
		DIV_ROUND_CLOSEST(ov8856->cur_mode->max_fps.denominator,
				  ov8856->cur_mode->max_fps.numerator));

	mutex_lock(&ov8856->mutex);
	on = !!on;
	if (on == ov8856->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __ov8856_start_stream(ov8856);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}

	} else {
		__ov8856_stop_stream(ov8856);
		pm_runtime_put(&client->dev);
	}

	ov8856->streaming = on;

unlock_and_return:
	mutex_unlock(&ov8856->mutex);

	return ret;
}

static int ov8856_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	struct i2c_client *client = ov8856->client;
	int ret = 0;

	dev_info(&client->dev, "%s on(%d)\n", __func__, on);

	mutex_lock(&ov8856->mutex);

	/* If the power state is not modified - no work to do. */
	if (ov8856->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = ov8856_write_array(ov8856->client, ov8856_global_regs);
		if (ret) {
			v4l2_err(sd, "could not set init registers\n");
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ov8856->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		ov8856->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&ov8856->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 ov8856_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, OV8856_XVCLK_FREQ / 1000 / 1000);
}

static int __ov8856_power_on(struct ov8856 *ov8856)
{
	int ret;
	u32 delay_us;
	struct device *dev = &ov8856->client->dev;

	if (!IS_ERR(ov8856->power_gpio))
		gpiod_set_value_cansleep(ov8856->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(ov8856->pins_default)) {
		ret = pinctrl_select_state(ov8856->pinctrl,
					   ov8856->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}

	ret = clk_set_rate(ov8856->xvclk, OV8856_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(ov8856->xvclk) != OV8856_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(ov8856->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (!IS_ERR(ov8856->reset_gpio))
		gpiod_set_value_cansleep(ov8856->reset_gpio, 0);

	ret = regulator_bulk_enable(OV8856_NUM_SUPPLIES, ov8856->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(ov8856->reset_gpio))
		gpiod_set_value_cansleep(ov8856->reset_gpio, 1);

	usleep_range(1000, 2000);
	if (!IS_ERR(ov8856->pwdn_gpio))
		gpiod_set_value_cansleep(ov8856->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = ov8856_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(ov8856->xvclk);

	return ret;
}

static void __ov8856_power_off(struct ov8856 *ov8856)
{
	int ret;
	struct device *dev = &ov8856->client->dev;

	if (!IS_ERR(ov8856->pwdn_gpio))
		gpiod_set_value_cansleep(ov8856->pwdn_gpio, 0);
	clk_disable_unprepare(ov8856->xvclk);
	if (!IS_ERR(ov8856->reset_gpio))
		gpiod_set_value_cansleep(ov8856->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(ov8856->pins_sleep)) {
		ret = pinctrl_select_state(ov8856->pinctrl,
					   ov8856->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}

	if (!IS_ERR(ov8856->power_gpio))
		gpiod_set_value_cansleep(ov8856->power_gpio, 0);
	regulator_bulk_disable(OV8856_NUM_SUPPLIES, ov8856->supplies);

}

static int __maybe_unused ov8856_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8856 *ov8856 = to_ov8856(sd);

	return __ov8856_power_on(ov8856);
}

static int __maybe_unused ov8856_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8856 *ov8856 = to_ov8856(sd);

	__ov8856_power_off(ov8856);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int ov8856_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov8856 *ov8856 = to_ov8856(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct ov8856_mode *def_mode = &supported_modes[0];

	mutex_lock(&ov8856->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = OV8856_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&ov8856->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int ov8856_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum *fie)
{
	struct ov8856 *ov8856 = to_ov8856(sd);

	if (fie->index >= ov8856->cfg_num)
		return -EINVAL;

	if (fie->code != OV8856_MEDIA_BUS_FMT)
		return -EINVAL;

	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static int ov8856_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct ov8856  *sensor = to_ov8856(sd);
	struct device *dev = &sensor->client->dev;

	dev_info(dev, "%s(%d) enter!\n", __func__, __LINE__);

	if (2 == sensor->lane_num || 4 == sensor->lane_num) {
		config->type = V4L2_MBUS_CSI2_DPHY;
		config->bus.mipi_csi2.num_data_lanes = sensor->lane_num;
	} else {
		dev_err(&sensor->client->dev,
			"unsupported lane_num(%d)\n", sensor->lane_num);
	}
	return 0;
}

static int ov8856_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct ov8856 *ov8856 = to_ov8856(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = 0;
		sel->r.width = ov8856->cur_mode->width;
		sel->r.top = 0;
		sel->r.height = ov8856->cur_mode->height;
		return 0;
	}

	return -EINVAL;
}

static const struct dev_pm_ops ov8856_pm_ops = {
	SET_RUNTIME_PM_OPS(ov8856_runtime_suspend,
			   ov8856_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops ov8856_internal_ops = {
	.open = ov8856_open,
};
#endif

static const struct v4l2_subdev_core_ops ov8856_core_ops = {
	.s_power = ov8856_s_power,
	.ioctl = ov8856_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ov8856_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops ov8856_video_ops = {
	.s_stream = ov8856_s_stream,
	.g_frame_interval = ov8856_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops ov8856_pad_ops = {
	.enum_mbus_code = ov8856_enum_mbus_code,
	.enum_frame_size = ov8856_enum_frame_sizes,
	.enum_frame_interval = ov8856_enum_frame_interval,
	.get_fmt = ov8856_get_fmt,
	.set_fmt = ov8856_set_fmt,
	.get_selection = ov8856_get_selection,
	.get_mbus_config = ov8856_g_mbus_config,
};

static const struct v4l2_subdev_ops ov8856_subdev_ops = {
	.core	= &ov8856_core_ops,
	.video	= &ov8856_video_ops,
	.pad	= &ov8856_pad_ops,
};
static int ov8856_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov8856 *ov8856 = container_of(ctrl->handler,
					     struct ov8856, ctrl_handler);
	struct i2c_client *client = ov8856->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = ov8856->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(ov8856->exposure,
					 ov8856->exposure->minimum, max,
					 ov8856->exposure->step,
					 ov8856->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		dev_dbg(&client->dev, "set exposure value 0x%x\n", ctrl->val);
		ret = ov8856_write_reg(ov8856->client,
					OV8856_REG_EXPOSURE,
					OV8856_REG_VALUE_24BIT,
					ctrl->val << 4);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set analog gain value 0x%x\n", ctrl->val);
		ret = ov8856_write_reg(ov8856->client,
					OV8856_REG_GAIN_H,
					OV8856_REG_VALUE_08BIT,
					(ctrl->val >> OV8856_GAIN_H_SHIFT) &
					OV8856_GAIN_H_MASK);
		ret |= ov8856_write_reg(ov8856->client,
					OV8856_REG_GAIN_L,
					OV8856_REG_VALUE_08BIT,
					ctrl->val & OV8856_GAIN_L_MASK);
		break;
	case V4L2_CID_VBLANK:
		dev_info(&client->dev, "set vb value 0x%x\n", ctrl->val);
		ret = ov8856_write_reg(ov8856->client,
					OV8856_REG_VTS,
					OV8856_REG_VALUE_16BIT,
					ctrl->val + ov8856->cur_mode->height);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = ov8856_enable_test_pattern(ov8856, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		dev_info(&client->dev, "set hflip value 0x%x\n", ctrl->val);

		ret = ov8856_read_reg(ov8856->client, OV8856_MIRROR_REG,
				       OV8856_REG_VALUE_08BIT,
				       &val);
		if (ctrl->val)
			val |= MIRROR_BIT_MASK;
		else
			val &= ~MIRROR_BIT_MASK;


		ret |= ov8856_write_reg(ov8856->client, OV8856_MIRROR_REG,
					 OV8856_REG_VALUE_08BIT,
					 val);
		ret |= ov8856_read_reg(ov8856->client, OV8856_ISPCTRL2E_REG,
				       OV8856_REG_VALUE_08BIT,
				       &val);
		if (ctrl->val)
			val &= ~(ISPCTRL2E_BIT_MASK);
		else
			val |= ISPCTRL2E_BIT_MASK;

		ret |= ov8856_write_reg(ov8856->client, OV8856_ISPCTRL2E_REG,
					 OV8856_REG_VALUE_08BIT,
					 val);

		ret |= ov8856_read_reg(ov8856->client, OV8856_ISPCTRL1_REG,
				       OV8856_REG_VALUE_08BIT,
				       &val);
		if (ctrl->val)
			val |= ISPCTRL1_BIT_MASK;
		else
			val &= ~ISPCTRL1_BIT_MASK;

		ret |= ov8856_write_reg(ov8856->client, OV8856_ISPCTRL1_REG,
					 OV8856_REG_VALUE_08BIT,
					 val);

		break;
	case V4L2_CID_VFLIP:
		dev_info(&client->dev, "set vflip value 0x%x\n", ctrl->val);
		ret = ov8856_read_reg(ov8856->client, OV8856_FLIP_REG,
				       OV8856_REG_VALUE_08BIT,
				       &val);
		if (ctrl->val)
			val &= ~(MIRROR_BIT_MASK);
		else
			val |= MIRROR_BIT_MASK;

		ret |= ov8856_write_reg(ov8856->client, OV8856_FLIP_REG,
					 OV8856_REG_VALUE_08BIT,
					 val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov8856_ctrl_ops = {
	.s_ctrl = ov8856_set_ctrl,
};

static int ov8856_initialize_controls(struct ov8856 *ov8856)
{
	const struct ov8856_mode *mode;
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *ctrl;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &ov8856->ctrl_handler;
	mode = ov8856->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &ov8856->mutex;

	ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, link_freq_menu_items);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, ov8856->pixel_rate, 1, ov8856->pixel_rate);

	h_blank = mode->hts_def - mode->width;
	ov8856->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (ov8856->hblank)
		ov8856->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	ov8856->vblank = v4l2_ctrl_new_std(handler, &ov8856_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				OV8856_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 6;
	ov8856->exposure = v4l2_ctrl_new_std(handler, &ov8856_ctrl_ops,
				V4L2_CID_EXPOSURE, OV8856_EXPOSURE_MIN,
				exposure_max, OV8856_EXPOSURE_STEP,
				mode->exp_def);

	ov8856->anal_gain = v4l2_ctrl_new_std(handler, &ov8856_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, OV8856_GAIN_MIN,
				OV8856_GAIN_MAX, OV8856_GAIN_STEP,
				OV8856_GAIN_DEFAULT);

	ov8856->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&ov8856_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(ov8856_test_pattern_menu) - 1,
				0, 0, ov8856_test_pattern_menu);
	ov8856->h_flip = v4l2_ctrl_new_std(handler, &ov8856_ctrl_ops,
					    V4L2_CID_HFLIP, 0, 1, 1, 0);

	ov8856->v_flip = v4l2_ctrl_new_std(handler, &ov8856_ctrl_ops,
					    V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&ov8856->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	ov8856->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int ov8856_check_sensor_id(struct ov8856 *ov8856,
				   struct i2c_client *client)
{
	struct device *dev = &ov8856->client->dev;
	u32 id = 0;
	int ret;

	ret = ov8856_read_reg(client, OV8856_REG_CHIP_ID,
			       OV8856_REG_VALUE_24BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	ret = ov8856_read_reg(client, OV8856_CHIP_REVISION_REG,
			       OV8856_REG_VALUE_08BIT, &id);
	if (ret) {
		dev_err(dev, "Read chip revision register error\n");
		return -ENODEV;
	}
	dev_info(dev, "Detected OV%06x sensor, REVISION 0x%x\n", CHIP_ID, id);

	if (ov8856->lane_num == 4) {
		ov8856_global_regs = ov8856_global_regs_4lane;
		ov8856->cur_mode = &supported_modes_4lane[0];
		supported_modes = supported_modes_4lane;
		ov8856->cfg_num = ARRAY_SIZE(supported_modes_4lane);
	} else {
		ov8856_global_regs = ov8856_global_regs_2lane;
		ov8856->cur_mode = &supported_modes_2lane[0];
		supported_modes = supported_modes_2lane;
		ov8856->cfg_num = ARRAY_SIZE(supported_modes_2lane);
	}

	return 0;
}

static int ov8856_configure_regulators(struct ov8856 *ov8856)
{
	unsigned int i;

	for (i = 0; i < OV8856_NUM_SUPPLIES; i++)
		ov8856->supplies[i].supply = ov8856_supply_names[i];

	return devm_regulator_bulk_get(&ov8856->client->dev,
				       OV8856_NUM_SUPPLIES,
				       ov8856->supplies);
}

static int ov8856_parse_of(struct ov8856 *ov8856)
{
	struct device *dev = &ov8856->client->dev;
	struct device_node *endpoint;
	struct fwnode_handle *fwnode;
	int rval;

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

	ov8856->lane_num = rval;
	if (ov8856->lane_num == 4) {
		ov8856->cur_mode = &supported_modes_4lane[0];
		supported_modes = supported_modes_4lane;
		ov8856->cfg_num = ARRAY_SIZE(supported_modes_4lane);

		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		ov8856->pixel_rate = MIPI_FREQ * 2U * ov8856->lane_num / 10U;
		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
				 ov8856->lane_num, ov8856->pixel_rate);
	} else {
		ov8856->cur_mode = &supported_modes_2lane[0];
		supported_modes = supported_modes_2lane;
		ov8856->cfg_num = ARRAY_SIZE(supported_modes_2lane);

		/*pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		ov8856->pixel_rate = MIPI_FREQ * 2U * (ov8856->lane_num) / 10U;
		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
				 ov8856->lane_num, ov8856->pixel_rate);
	}
	return 0;
}

static int ov8856_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct ov8856 *ov8856;
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

	ov8856 = devm_kzalloc(dev, sizeof(*ov8856), GFP_KERNEL);
	if (!ov8856)
		return -ENOMEM;

	ov8856->client = client;
	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &ov8856->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &ov8856->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &ov8856->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &ov8856->len_name);
	if (ret) {
		dev_err(dev,
			"could not get module information!\n");
		return -EINVAL;
	}

	ov8856->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(ov8856->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	ov8856->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(ov8856->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");

	ov8856->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ov8856->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios, maybe no use\n");

	ov8856->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(ov8856->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios, maybe no use\n");

	ret = ov8856_configure_regulators(ov8856);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ret = ov8856_parse_of(ov8856);
	if (ret != 0)
		return -EINVAL;

	ov8856->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(ov8856->pinctrl)) {
		ov8856->pins_default =
			pinctrl_lookup_state(ov8856->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(ov8856->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		ov8856->pins_sleep =
			pinctrl_lookup_state(ov8856->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(ov8856->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&ov8856->mutex);

	sd = &ov8856->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov8856_subdev_ops);
	ret = ov8856_initialize_controls(ov8856);
	if (ret)
		goto err_destroy_mutex;

	ret = __ov8856_power_on(ov8856);
	if (ret)
		goto err_free_handler;

	ret = ov8856_check_sensor_id(ov8856, client);
	if (ret)
		goto err_power_off;

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
				ov8856->otp = otp_ptr;
			} else {
				ov8856->otp = NULL;
				devm_kfree(dev, otp_ptr);
				dev_warn(dev, "can not get otp info, skip!\n");
			}
		}
	}

continue_probe:
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &ov8856_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	ov8856->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &ov8856->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(ov8856->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 ov8856->module_index, facing,
		 OV8856_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	dev_info(dev, "%s success!", __func__);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__ov8856_power_off(ov8856);
err_free_handler:
	v4l2_ctrl_handler_free(&ov8856->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&ov8856->mutex);

	return ret;
}

static void ov8856_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8856 *ov8856 = to_ov8856(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&ov8856->ctrl_handler);
	kfree(ov8856->otp);
	mutex_destroy(&ov8856->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__ov8856_power_off(ov8856);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ov8856_of_match[] = {
	{ .compatible = "ovti,ov8856" },
	{},
};
MODULE_DEVICE_TABLE(of, ov8856_of_match);
#endif

static const struct i2c_device_id ov8856_match_id[] = {
	{ "ovti,ov8856", 0 },
	{ },
};

static struct i2c_driver ov8856_i2c_driver = {
	.driver = {
		.name = OV8856_NAME,
		.pm = &ov8856_pm_ops,
		.of_match_table = of_match_ptr(ov8856_of_match),
	},
	.probe		= &ov8856_probe,
	.remove		= &ov8856_remove,
	.id_table	= ov8856_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&ov8856_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&ov8856_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("OmniVision ov8856 sensor driver");
MODULE_LICENSE("GPL v2");
