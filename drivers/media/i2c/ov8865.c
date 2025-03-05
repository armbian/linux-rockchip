// SPDX-License-Identifier: GPL-2.0
/*
 * ov8865 driver
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define MIPI_FREQ			360000000U
#define OV8865_PIXEL_RATE		(MIPI_FREQ * 2LL * 4LL / 10LL)

#define OV8865_XVCLK_FREQ		24000000

#define CHIP_ID				0x008865
#define OV8865_REG_CHIP_ID		0x300a

#define OV8865_REG_CTRL_MODE		0x0100
#define OV8865_MODE_SW_STANDBY		0x0
#define OV8865_MODE_STREAMING		0x1

#define OV8865_REG_EXPOSURE		0x3500
#define	OV8865_EXPOSURE_MIN		2
#define	OV8865_EXPOSURE_STEP		1
#define OV8865_VTS_MAX			0x7fff

#define OV8865_REG_GAIN_H		0x3508
#define OV8865_REG_GAIN_L		0x3509
#define OV8865_GAIN_H_MASK		0x1f
#define OV8865_GAIN_H_SHIFT		8
#define OV8865_GAIN_L_MASK		0xff
#define OV8865_GAIN_MIN			0x80
#define OV8865_GAIN_MAX			0x1fff
#define OV8865_GAIN_STEP		1
#define OV8865_GAIN_DEFAULT		0x80

#define OV8865_REG_TEST_PATTERN		0x5e00
#define	OV8865_TEST_PATTERN_ENABLE	0x80
#define	OV8865_TEST_PATTERN_DISABLE	0x0

#define OV8865_REG_VTS			0x380e

#define REG_NULL			0xFFFF

#define OV8865_REG_VALUE_08BIT		1
#define OV8865_REG_VALUE_16BIT		2
#define OV8865_REG_VALUE_24BIT		3

#define OV8865_LANES			4
#define OV8865_BITS_PER_SAMPLE		10

#define OV8865_CHIP_REVISION_REG	0x302A

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define OV8865_NAME			"ov8865"
#define OV8865_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SBGGR10_1X10

static const char * const ov8865_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define OV8865_NUM_SUPPLIES ARRAY_SIZE(ov8865_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct ov8865_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	u32 bpp;
	const struct regval *reg_list;
	u32 vc[PAD_MAX];
};

struct ov8865 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[OV8865_NUM_SUPPLIES];

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
	const struct ov8865_mode *cur_mode;

	unsigned int		lane_num;
	unsigned int		cfg_num;

	bool			power_on;

	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	struct rkmodule_inf	module_inf;
};

#define to_ov8865(sd) container_of(sd, struct ov8865, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval ov8865_global_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3638, 0xff},
	{0x0302, 0x1e},
	{0x0303, 0x00},
	{0x0304, 0x03},
	{0x030e, 0x00},
	{0x030f, 0x09},
	{0x0312, 0x01},
	{0x031e, 0x0c},
	{0x3015, 0x01},
	{0x3018, 0x72},
	{0x3020, 0x93},
	{0x3022, 0x01},
	{0x3031, 0x0a},
	{0x3106, 0x01},
	{0x3305, 0xf1},
	{0x3308, 0x00},
	{0x3309, 0x28},
	{0x330a, 0x00},
	{0x330b, 0x20},
	{0x330c, 0x00},
	{0x330d, 0x00},
	{0x330e, 0x00},
	{0x330f, 0x40},
	{0x3307, 0x04},
	{0x3604, 0x04},
	{0x3602, 0x30},
	{0x3605, 0x00},
	{0x3607, 0x20},
	{0x3608, 0x11},
	{0x3609, 0x68},
	{0x360a, 0x40},
	{0x360c, 0xdd},
	{0x360e, 0x0c},
	{0x3610, 0x07},
	{0x3612, 0x86},
	{0x3613, 0x58},
	{0x3614, 0x28},
	{0x3617, 0x40},
	{0x3618, 0x5a},
	{0x3619, 0x9b},
	{0x361c, 0x00},
	{0x361d, 0x60},
	{0x3631, 0x60},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x10},
	{0x3641, 0x55},
	{0x3646, 0x86},
	{0x3647, 0x27},
	{0x364a, 0x1b},
	{0x3500, 0x00},
	{0x3501, 0x4c},
	{0x3502, 0x00},
	{0x3503, 0x00},
	{0x3508, 0x02},
	{0x3509, 0x00},
	{0x3700, 0x24},
	{0x3701, 0x0c},
	{0x3702, 0x28},
	{0x3703, 0x19},
	{0x3704, 0x14},
	{0x3705, 0x00},
	{0x3706, 0x38},
	{0x3707, 0x04},
	{0x3708, 0x24},
	{0x3709, 0x40},
	{0x370a, 0x00},
	{0x370b, 0xb8},
	{0x370c, 0x04},
	{0x3718, 0x12},
	{0x3719, 0x31},
	{0x3712, 0x42},
	{0x3714, 0x12},
	{0x371e, 0x19},
	{0x371f, 0x40},
	{0x3720, 0x05},
	{0x3721, 0x05},
	{0x3724, 0x02},
	{0x3725, 0x02},
	{0x3726, 0x06},
	{0x3728, 0x05},
	{0x3729, 0x02},
	{0x372a, 0x03},
	{0x372b, 0x53},
	{0x372c, 0xa3},
	{0x372d, 0x53},
	{0x372e, 0x06},
	{0x372f, 0x10},
	{0x3730, 0x01},
	{0x3731, 0x06},
	{0x3732, 0x14},
	{0x3733, 0x10},
	{0x3734, 0x40},
	{0x3736, 0x20},
	{0x373a, 0x02},
	{0x373b, 0x0c},
	{0x373c, 0x0a},
	{0x373e, 0x03},
	{0x3755, 0x40},
	{0x3758, 0x00},
	{0x3759, 0x4c},
	{0x375a, 0x06},
	{0x375b, 0x13},
	{0x375c, 0x40},
	{0x375d, 0x02},
	{0x375e, 0x00},
	{0x375f, 0x14},
	{0x3767, 0x1c},
	{0x3768, 0x04},
	{0x3769, 0x20},
	{0x376c, 0xc0},
	{0x376d, 0xc0},
	{0x376a, 0x08},
	{0x3761, 0x00},
	{0x3762, 0x00},
	{0x3763, 0x00},
	{0x3766, 0xff},
	{0x376b, 0x42},
	{0x3772, 0x23},
	{0x3773, 0x02},
	{0x3774, 0x16},
	{0x3775, 0x12},
	{0x3776, 0x08},
	{0x37a0, 0x44},
	{0x37a1, 0x3d},
	{0x37a2, 0x3d},
	{0x37a3, 0x01},
	{0x37a4, 0x00},
	{0x37a5, 0x08},
	{0x37a6, 0x00},
	{0x37a7, 0x44},
	{0x37a8, 0x58},
	{0x37a9, 0x58},
	{0x3760, 0x00},
	{0x376f, 0x01},
	{0x37aa, 0x44},
	{0x37ab, 0x2e},
	{0x37ac, 0x2e},
	{0x37ad, 0x33},
	{0x37ae, 0x0d},
	{0x37af, 0x0d},
	{0x37b0, 0x00},
	{0x37b1, 0x00},
	{0x37b2, 0x00},
	{0x37b3, 0x42},
	{0x37b4, 0x42},
	{0x37b5, 0x33},
	{0x37b6, 0x00},
	{0x37b7, 0x00},
	{0x37b8, 0x00},
	{0x37b9, 0xff},
	{0x3800, 0x00},
	{0x3801, 0x0c},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xd3},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x07},
	{0x380d, 0x83},
	{0x380e, 0x04},
	{0x380f, 0xe0},
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3813, 0x04},
	{0x3814, 0x03},
	{0x3815, 0x01},
	{0x3820, 0x00},
	{0x3821, 0x67},
	{0x382a, 0x03},
	{0x382b, 0x01},
	{0x3830, 0x08},
	{0x3836, 0x02},
	{0x3837, 0x18},
	{0x3841, 0xff},
	{0x3846, 0x88},
	{0x3d85, 0x06},
	{0x3d8c, 0x75},
	{0x3d8d, 0xef},
	{0x3f08, 0x0b},
	{0x4000, 0xf1},
	{0x4001, 0x14},
	{0x4005, 0x10},
	{0x400b, 0x0c},
	{0x400d, 0x10},
	{0x401b, 0x00},
	{0x401d, 0x00},
	{0x4020, 0x01},
	{0x4021, 0x20},
	{0x4022, 0x01},
	{0x4023, 0x9f},
	{0x4024, 0x03},
	{0x4025, 0xe0},
	{0x4026, 0x04},
	{0x4027, 0x5f},
	{0x4028, 0x00},
	{0x4029, 0x02},
	{0x402a, 0x04},
	{0x402b, 0x04},
	{0x402c, 0x02},
	{0x402d, 0x02},
	{0x402e, 0x08},
	{0x402f, 0x02},
	{0x401f, 0x00},
	{0x4034, 0x3f},
	{0x4300, 0xff},
	{0x4301, 0x00},
	{0x4302, 0x0f},
	{0x4500, 0x40},
	{0x4503, 0x10},
	{0x4601, 0x74},
	{0x481f, 0x32},
	{0x4837, 0x16},
	{0x4850, 0x10},
	{0x4851, 0x32},
	{0x4b00, 0x2a},
	{0x4b0d, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0x18},
	{0x4d02, 0xc3},
	{0x4d03, 0xff},
	{0x4d04, 0xff},
	{0x4d05, 0xff},
	{0x5000, 0x96},
	{0x5001, 0x01},
	{0x5002, 0x08},
	{0x5901, 0x00},
	{0x5e00, 0x00},
	{0x5e01, 0x41},
	{0x0100, 0x01},
	{0x5b00, 0x02},
	{0x5b01, 0xd0},
	{0x5b02, 0x03},
	{0x5b03, 0xff},
	{0x5b05, 0x6c},
	{0x5780, 0xfc},
	{0x5781, 0xdf},
	{0x5782, 0x3f},
	{0x5783, 0x08},
	{0x5784, 0x0c},
	{0x5786, 0x20},
	{0x5787, 0x40},
	{0x5788, 0x08},
	{0x5789, 0x08},
	{0x578a, 0x02},
	{0x578b, 0x01},
	{0x578c, 0x01},
	{0x578d, 0x0c},
	{0x578e, 0x02},
	{0x578f, 0x01},
	{0x5790, 0x01},
	{0x5800, 0x1d},
	{0x5801, 0x0e},
	{0x5802, 0x0c},
	{0x5803, 0x0c},
	{0x5804, 0x0f},
	{0x5805, 0x22},
	{0x5806, 0x0a},
	{0x5807, 0x06},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x07},
	{0x580b, 0x0a},
	{0x580c, 0x06},
	{0x580d, 0x02},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x07},
	{0x5812, 0x06},
	{0x5813, 0x02},
	{0x5814, 0x00},
	{0x5815, 0x00},
	{0x5816, 0x03},
	{0x5817, 0x07},
	{0x5818, 0x09},
	{0x5819, 0x06},
	{0x581a, 0x04},
	{0x581b, 0x04},
	{0x581c, 0x06},
	{0x581d, 0x0a},
	{0x581e, 0x19},
	{0x581f, 0x0d},
	{0x5820, 0x0b},
	{0x5821, 0x0b},
	{0x5822, 0x0e},
	{0x5823, 0x22},
	{0x5824, 0x23},
	{0x5825, 0x28},
	{0x5826, 0x29},
	{0x5827, 0x27},
	{0x5828, 0x13},
	{0x5829, 0x26},
	{0x582a, 0x33},
	{0x582b, 0x32},
	{0x582c, 0x33},
	{0x582d, 0x16},
	{0x582e, 0x14},
	{0x582f, 0x30},
	{0x5830, 0x31},
	{0x5831, 0x30},
	{0x5832, 0x15},
	{0x5833, 0x26},
	{0x5834, 0x23},
	{0x5835, 0x21},
	{0x5836, 0x23},
	{0x5837, 0x05},
	{0x5838, 0x36},
	{0x5839, 0x27},
	{0x583a, 0x28},
	{0x583b, 0x26},
	{0x583c, 0x24},
	{0x583d, 0xdf},
	{REG_NULL, 0x00},
};

/*
 * XVCLK=24Mhz, sysclk=144Mhz
 * MIPI 4 lane, 720Mbps/lane
 */
static const struct regval ov8865_3264x2448_30fps_regs[] = {
	{0x0100, 0x00},
	{0x030f, 0x04},
	{0x3018, 0x72},
	{0x3106, 0x01},
	{0x3501, 0x98},
	{0x3502, 0x60},
	{0x3700, 0x48},
	{0x3701, 0x18},
	{0x3702, 0x50},
	{0x3703, 0x32},
	{0x3704, 0x28},
	{0x3706, 0x70},
	{0x3707, 0x08},
	{0x3708, 0x48},
	{0x3709, 0x80},
	{0x370a, 0x01},
	{0x370b, 0x70},
	{0x370c, 0x07},
	{0x3718, 0x14},
	{0x3712, 0x44},
	{0x371e, 0x31},
	{0x371f, 0x7f},
	{0x3720, 0x0a},
	{0x3721, 0x0a},
	{0x3724, 0x04},
	{0x3725, 0x04},
	{0x3726, 0x0c},
	{0x3728, 0x0a},
	{0x3729, 0x03},
	{0x372a, 0x06},
	{0x372b, 0xa6},
	{0x372c, 0xa6},
	{0x372d, 0xa6},
	{0x372e, 0x0c},
	{0x372f, 0x20},
	{0x3730, 0x02},
	{0x3731, 0x0c},
	{0x3732, 0x28},
	{0x3736, 0x30},
	{0x373a, 0x04},
	{0x373b, 0x18},
	{0x373c, 0x14},
	{0x373e, 0x06},
	{0x375a, 0x0c},
	{0x375b, 0x26},
	{0x375d, 0x04},
	{0x375f, 0x28},
	{0x3767, 0x1e},
	{0x3772, 0x46},
	{0x3773, 0x04},
	{0x3774, 0x2c},
	{0x3775, 0x13},
	{0x3776, 0x10},
	{0x37a0, 0x88},
	{0x37a1, 0x7a},
	{0x37a2, 0x7a},
	{0x37a3, 0x02},
	{0x37a5, 0x09},
	{0x37a7, 0x88},
	{0x37a8, 0xb0},
	{0x37a9, 0xb0},
	{0x37aa, 0x88},
	{0x37ab, 0x5c},
	{0x37ac, 0x5c},
	{0x37ad, 0x55},
	{0x37ae, 0x19},
	{0x37af, 0x19},
	{0x37b3, 0x84},
	{0x37b4, 0x84},
	{0x37b5, 0x66},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x07},
	{0x380d, 0x98},
	{0x380e, 0x09},
	{0x380f, 0xa6},
	{0x3813, 0x02},
	{0x3814, 0x01},
	{0x3821, 0x46},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x3830, 0x04},
	{0x3836, 0x01},
	{0x3846, 0x48},
	{0x3f08, 0x16},
	{0x4000, 0xf1},
	{0x4001, 0x04},
	{0x4020, 0x02},
	{0x4021, 0x40},
	{0x4022, 0x03},
	{0x4023, 0x3f},
	{0x4024, 0x07},
	{0x4025, 0xc0},
	{0x4026, 0x08},
	{0x4027, 0xbf},
	{0x402a, 0x04},
	{0x402b, 0x04},
	{0x402c, 0x02},
	{0x402d, 0x02},
	{0x402e, 0x08},
	{0x4500, 0x68},
	{0x4601, 0x10},
	{0x5002, 0x08},
	{0x5901, 0x00},
	{REG_NULL, 0x00},
};

static const struct ov8865_mode supported_modes[] = {
	{
		.width = 3264,
		.height = 2448,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x09a2,
		.hts_def = 0x0798 * 2,
		.vts_def = 0x09a6,
		.reg_list = ov8865_3264x2448_30fps_regs,
		.mipi_freq_idx = 0,
		.bus_fmt = MEDIA_BUS_FMT_SGBRG10_1X10,
		.bpp = 10,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_menu_items[] = {
	MIPI_FREQ
};

static const char * const ov8865_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int ov8865_write_reg(struct i2c_client *client, u16 reg,
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

static int ov8865_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = ov8865_write_reg(client, regs[i].addr,
					OV8865_REG_VALUE_08BIT,
					regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int ov8865_read_reg(struct i2c_client *client, u16 reg,
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

static int ov8865_get_reso_dist(const struct ov8865_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct ov8865_mode *
ov8865_find_best_fit(struct ov8865 *ov8865,
		     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ov8865->cfg_num; i++) {
		dist = ov8865_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int ov8865_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct ov8865 *ov8865 = to_ov8865(sd);
	const struct ov8865_mode *mode;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	u32 lane_num = OV8865_LANES;

	mutex_lock(&ov8865->mutex);

	mode = ov8865_find_best_fit(ov8865, fmt);
	fmt->format.code = OV8865_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&ov8865->mutex);
		return -ENOTTY;
#endif
	} else {
		ov8865->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(ov8865->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov8865->vblank, vblank_def,
					 OV8865_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(ov8865->vblank, vblank_def);
		pixel_rate = (u32)link_freq_menu_items[mode->mipi_freq_idx] /
			     mode->bpp * 2 * lane_num;

		__v4l2_ctrl_s_ctrl_int64(ov8865->pixel_rate,
					 pixel_rate);
		__v4l2_ctrl_s_ctrl(ov8865->link_freq,
				   mode->mipi_freq_idx);
	}
	dev_info(&ov8865->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&ov8865->mutex);

	return 0;
}

static int ov8865_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct ov8865 *ov8865 = to_ov8865(sd);
	const struct ov8865_mode *mode = ov8865->cur_mode;

	mutex_lock(&ov8865->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&ov8865->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = OV8865_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&ov8865->mutex);

	return 0;
}

static int ov8865_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = OV8865_MEDIA_BUS_FMT;

	return 0;
}

static int ov8865_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct ov8865 *ov8865 = to_ov8865(sd);

	if (fse->index >= ov8865->cfg_num)
		return -EINVAL;

	if (fse->code != OV8865_MEDIA_BUS_FMT)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int ov8865_enable_test_pattern(struct ov8865 *ov8865, u32 pattern)
{
	u32 val;

	if (pattern)
		val = (pattern - 1) | OV8865_TEST_PATTERN_ENABLE;
	else
		val = OV8865_TEST_PATTERN_DISABLE;

	return ov8865_write_reg(ov8865->client,
				 OV8865_REG_TEST_PATTERN,
				 OV8865_REG_VALUE_08BIT,
				 val);
}

static int ov8865_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct ov8865 *ov8865 = to_ov8865(sd);
	const struct ov8865_mode *mode = ov8865->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static void ov8865_get_module_inf(struct ov8865 *ov8865,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, OV8865_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, ov8865->module_name, sizeof(inf->base.module));
	strscpy(inf->base.lens, ov8865->len_name, sizeof(inf->base.lens));

}

static long ov8865_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct ov8865 *ov8865 = to_ov8865(sd);
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		ov8865_get_module_inf(ov8865, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = ov8865_write_reg(ov8865->client,
				OV8865_REG_CTRL_MODE,
				OV8865_REG_VALUE_08BIT,
				OV8865_MODE_STREAMING);
		else
			ret = ov8865_write_reg(ov8865->client,
				OV8865_REG_CTRL_MODE,
				OV8865_REG_VALUE_08BIT,
				OV8865_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ov8865_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = ov8865_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = ov8865_ioctl(sd, cmd, &stream);
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

static int __ov8865_start_stream(struct ov8865 *ov8865)
{
	int ret;

	ret = ov8865_write_array(ov8865->client, ov8865_global_regs);
	if (ret)
		return ret;

	ret = ov8865_write_array(ov8865->client, ov8865->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&ov8865->mutex);
	ret = v4l2_ctrl_handler_setup(&ov8865->ctrl_handler);
	mutex_lock(&ov8865->mutex);
	if (ret)
		return ret;

	return ov8865_write_reg(ov8865->client,
				OV8865_REG_CTRL_MODE,
				OV8865_REG_VALUE_08BIT,
				OV8865_MODE_STREAMING);
}

static int __ov8865_stop_stream(struct ov8865 *ov8865)
{
	return ov8865_write_reg(ov8865->client,
				OV8865_REG_CTRL_MODE,
				OV8865_REG_VALUE_08BIT,
				OV8865_MODE_SW_STANDBY);
}

static int ov8865_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ov8865 *ov8865 = to_ov8865(sd);
	struct i2c_client *client = ov8865->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				ov8865->cur_mode->width,
				ov8865->cur_mode->height,
		DIV_ROUND_CLOSEST(ov8865->cur_mode->max_fps.denominator,
				  ov8865->cur_mode->max_fps.numerator));

	mutex_lock(&ov8865->mutex);
	on = !!on;
	if (on == ov8865->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __ov8865_start_stream(ov8865);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__ov8865_stop_stream(ov8865);
		pm_runtime_put(&client->dev);
	}

	ov8865->streaming = on;

unlock_and_return:
	mutex_unlock(&ov8865->mutex);

	return ret;
}

static int ov8865_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov8865 *ov8865 = to_ov8865(sd);
	struct i2c_client *client = ov8865->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d\n", __func__, on);

	mutex_lock(&ov8865->mutex);

	/* If the power state is not modified - no work to do. */
	if (ov8865->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ov8865->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		ov8865->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&ov8865->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 ov8865_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, OV8865_XVCLK_FREQ / 1000 / 1000);
}

static int __ov8865_power_on(struct ov8865 *ov8865)
{
	int ret;
	u32 delay_us;
	struct device *dev = &ov8865->client->dev;

	if (!IS_ERR(ov8865->power_gpio))
		gpiod_set_value_cansleep(ov8865->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(ov8865->pins_default)) {
		ret = pinctrl_select_state(ov8865->pinctrl,
					   ov8865->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}

	ret = clk_set_rate(ov8865->xvclk, OV8865_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(ov8865->xvclk) != OV8865_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(ov8865->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (!IS_ERR(ov8865->reset_gpio))
		gpiod_set_value_cansleep(ov8865->reset_gpio, 0);

	ret = regulator_bulk_enable(OV8865_NUM_SUPPLIES, ov8865->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(ov8865->reset_gpio))
		gpiod_set_value_cansleep(ov8865->reset_gpio, 1);

	usleep_range(1000, 2000);
	if (!IS_ERR(ov8865->pwdn_gpio))
		gpiod_set_value_cansleep(ov8865->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = ov8865_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(ov8865->xvclk);

	return ret;
}

static void __ov8865_power_off(struct ov8865 *ov8865)
{
	int ret;
	struct device *dev = &ov8865->client->dev;

	if (!IS_ERR(ov8865->pwdn_gpio))
		gpiod_set_value_cansleep(ov8865->pwdn_gpio, 0);
	clk_disable_unprepare(ov8865->xvclk);
	if (!IS_ERR(ov8865->reset_gpio))
		gpiod_set_value_cansleep(ov8865->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(ov8865->pins_sleep)) {
		ret = pinctrl_select_state(ov8865->pinctrl,
					   ov8865->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}

	if (!IS_ERR(ov8865->power_gpio))
		gpiod_set_value_cansleep(ov8865->power_gpio, 0);

	regulator_bulk_disable(OV8865_NUM_SUPPLIES, ov8865->supplies);
}

static int __maybe_unused ov8865_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8865 *ov8865 = to_ov8865(sd);

	return __ov8865_power_on(ov8865);
}

static int __maybe_unused ov8865_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8865 *ov8865 = to_ov8865(sd);

	__ov8865_power_off(ov8865);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int ov8865_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov8865 *ov8865 = to_ov8865(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct ov8865_mode *def_mode = &supported_modes[0];

	mutex_lock(&ov8865->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = OV8865_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&ov8865->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int ov8865_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = OV8865_MEDIA_BUS_FMT;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static int ov8865_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = OV8865_LANES;

	return 0;
}

static const struct dev_pm_ops ov8865_pm_ops = {
	SET_RUNTIME_PM_OPS(ov8865_runtime_suspend,
			   ov8865_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops ov8865_internal_ops = {
	.open = ov8865_open,
};
#endif

static const struct v4l2_subdev_core_ops ov8865_core_ops = {
	.s_power = ov8865_s_power,
	.ioctl = ov8865_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ov8865_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops ov8865_video_ops = {
	.s_stream = ov8865_s_stream,
	.g_frame_interval = ov8865_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops ov8865_pad_ops = {
	.enum_mbus_code = ov8865_enum_mbus_code,
	.enum_frame_size = ov8865_enum_frame_sizes,
	.enum_frame_interval = ov8865_enum_frame_interval,
	.get_fmt = ov8865_get_fmt,
	.set_fmt = ov8865_set_fmt,
	.get_mbus_config = ov8865_g_mbus_config,
};

static const struct v4l2_subdev_ops ov8865_subdev_ops = {
	.core	= &ov8865_core_ops,
	.video	= &ov8865_video_ops,
	.pad	= &ov8865_pad_ops,
};

static int ov8865_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov8865 *ov8865 = container_of(ctrl->handler,
					     struct ov8865, ctrl_handler);
	struct i2c_client *client = ov8865->client;
	s64 max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = ov8865->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(ov8865->exposure,
					 ov8865->exposure->minimum, max,
					 ov8865->exposure->step,
					 ov8865->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		dev_dbg(&client->dev, "set exposure value 0x%x\n", ctrl->val);
		ret = ov8865_write_reg(ov8865->client,
					OV8865_REG_EXPOSURE,
					OV8865_REG_VALUE_24BIT,
					ctrl->val << 4);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set analog gain value 0x%x\n", ctrl->val);
		ret = ov8865_write_reg(ov8865->client,
					OV8865_REG_GAIN_H,
					OV8865_REG_VALUE_08BIT,
					(ctrl->val >> OV8865_GAIN_H_SHIFT) &
					OV8865_GAIN_H_MASK);
		ret |= ov8865_write_reg(ov8865->client,
					OV8865_REG_GAIN_L,
					OV8865_REG_VALUE_08BIT,
					ctrl->val & OV8865_GAIN_L_MASK);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vb value 0x%x\n", ctrl->val);
		ret = ov8865_write_reg(ov8865->client,
					OV8865_REG_VTS,
					OV8865_REG_VALUE_16BIT,
					ctrl->val + ov8865->cur_mode->height);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = ov8865_enable_test_pattern(ov8865, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov8865_ctrl_ops = {
	.s_ctrl = ov8865_set_ctrl,
};

static int ov8865_initialize_controls(struct ov8865 *ov8865)
{
	const struct ov8865_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_pixel_rate = 0;
	u32 lane_num = OV8865_LANES;

	handler = &ov8865->ctrl_handler;
	mode = ov8865->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &ov8865->mutex;

	ov8865->link_freq = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, link_freq_menu_items);

	dst_pixel_rate = (u32)link_freq_menu_items[mode->mipi_freq_idx] / mode->bpp * 2 * lane_num;

	ov8865->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
			V4L2_CID_PIXEL_RATE,
			0, OV8865_PIXEL_RATE,
			1, dst_pixel_rate);

	__v4l2_ctrl_s_ctrl(ov8865->link_freq,
			   mode->mipi_freq_idx);

	h_blank = mode->hts_def - mode->width;
	ov8865->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (ov8865->hblank)
		ov8865->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	ov8865->vblank = v4l2_ctrl_new_std(handler, &ov8865_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				OV8865_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 4;
	ov8865->exposure = v4l2_ctrl_new_std(handler, &ov8865_ctrl_ops,
				V4L2_CID_EXPOSURE, OV8865_EXPOSURE_MIN,
				exposure_max, OV8865_EXPOSURE_STEP,
				mode->exp_def);

	ov8865->anal_gain = v4l2_ctrl_new_std(handler, &ov8865_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, OV8865_GAIN_MIN,
				OV8865_GAIN_MAX, OV8865_GAIN_STEP,
				OV8865_GAIN_DEFAULT);

	ov8865->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&ov8865_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(ov8865_test_pattern_menu) - 1,
				0, 0, ov8865_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_err(&ov8865->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	ov8865->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int ov8865_check_sensor_id(struct ov8865 *ov8865,
				   struct i2c_client *client)
{
	struct device *dev = &ov8865->client->dev;
	u32 id = 0;
	int ret;

	ret = ov8865_read_reg(client, OV8865_REG_CHIP_ID,
			       OV8865_REG_VALUE_24BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	ret = ov8865_read_reg(client, OV8865_CHIP_REVISION_REG,
			       OV8865_REG_VALUE_08BIT, &id);
	if (ret) {
		dev_err(dev, "Read chip revision register error\n");
		return -ENODEV;
	}
	dev_info(dev, "Detected OV%06x sensor, REVISION 0x%x\n", CHIP_ID, id);

	return 0;
}

static int ov8865_configure_regulators(struct ov8865 *ov8865)
{
	unsigned int i;

	for (i = 0; i < OV8865_NUM_SUPPLIES; i++)
		ov8865->supplies[i].supply = ov8865_supply_names[i];

	return devm_regulator_bulk_get(&ov8865->client->dev,
				       OV8865_NUM_SUPPLIES,
				       ov8865->supplies);
}

static int ov8865_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct ov8865 *ov8865;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	ov8865 = devm_kzalloc(dev, sizeof(*ov8865), GFP_KERNEL);
	if (!ov8865)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &ov8865->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &ov8865->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &ov8865->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &ov8865->len_name);
	if (ret) {
		dev_err(dev,
			"could not get module information!\n");
		return -EINVAL;
	}

	ov8865->client = client;
	ov8865->cur_mode = &supported_modes[0];
	ov8865->cfg_num = ARRAY_SIZE(supported_modes);
	ov8865->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(ov8865->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	ov8865->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(ov8865->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");

	ov8865->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ov8865->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios, maybe no use\n");

	ov8865->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(ov8865->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios, maybe no use\n");

	ret = ov8865_configure_regulators(ov8865);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ov8865->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(ov8865->pinctrl)) {
		ov8865->pins_default =
			pinctrl_lookup_state(ov8865->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(ov8865->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		ov8865->pins_sleep =
			pinctrl_lookup_state(ov8865->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(ov8865->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&ov8865->mutex);

	sd = &ov8865->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov8865_subdev_ops);
	ret = ov8865_initialize_controls(ov8865);
	if (ret)
		goto err_destroy_mutex;

	ret = __ov8865_power_on(ov8865);
	if (ret)
		goto err_free_handler;

	ret = ov8865_check_sensor_id(ov8865, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &ov8865_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	ov8865->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &ov8865->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(ov8865->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 ov8865->module_index, facing,
		 OV8865_NAME, dev_name(sd->dev));
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
	__ov8865_power_off(ov8865);
err_free_handler:
	v4l2_ctrl_handler_free(&ov8865->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&ov8865->mutex);

	return ret;
}

static void ov8865_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8865 *ov8865 = to_ov8865(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&ov8865->ctrl_handler);
	mutex_destroy(&ov8865->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__ov8865_power_off(ov8865);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ov8865_of_match[] = {
	{ .compatible = "ovti,ov8865" },
	{},
};
MODULE_DEVICE_TABLE(of, ov8865_of_match);
#endif

static const struct i2c_device_id ov8865_match_id[] = {
	{ "ovti,ov8865", 0 },
	{ },
};

static struct i2c_driver ov8865_i2c_driver = {
	.driver = {
		.name = OV8865_NAME,
		.pm = &ov8865_pm_ops,
		.of_match_table = of_match_ptr(ov8865_of_match),
	},
	.probe		= &ov8865_probe,
	.remove		= &ov8865_remove,
	.id_table	= ov8865_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&ov8865_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&ov8865_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("OmniVision ov8865 sensor driver");
MODULE_LICENSE("GPL v2");
