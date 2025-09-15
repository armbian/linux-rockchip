// SPDX-License-Identifier: GPL-2.0
/*
 * sc235hai driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 first implement.
 * V0.0X01.0X02 add soft sync mode.
 * V0.0X01.0X03 add support wake-up/sleep(aov) mode.
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
#include <linux/rk-camera-module.h>
#include <linux/rk-preisp.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"
#include "cam-sleep-wakeup.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x03)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC235HAI_LANES			2
#define SC235HAI_BITS_PER_SAMPLE	10
#define SC235HAI_LINK_FREQ_371		371250000// 742.5Mbps

#define PIXEL_RATE_WITH_371M_10BIT	(SC235HAI_LINK_FREQ_371 * 2 * \
					SC235HAI_LANES / SC235HAI_BITS_PER_SAMPLE)

#define SC235HAI_XVCLK_FREQ		27000000

#define CHIP_ID				0xcb6a
#define SC235HAI_REG_CHIP_ID		0x3107

#define SC235HAI_REG_CTRL_MODE		0x0100
#define SC235HAI_MODE_SW_STANDBY	0x0
#define SC235HAI_MODE_STREAMING		BIT(0)

#define SC235HAI_REG_MIPI_CTRL		0x3019
#define SC235HAI_MIPI_CTRL_ON		0x0c
#define SC235HAI_MIPI_CTRL_OFF		0x0f

#define SC235HAI_REG_EXPOSURE_H		0x3e00
#define SC235HAI_REG_EXPOSURE_M		0x3e01
#define SC235HAI_REG_EXPOSURE_L		0x3e02
#define SC235HAI_REG_SEXPOSURE_H	0x3e22
#define SC235HAI_REG_SEXPOSURE_M	0x3e04
#define SC235HAI_REG_SEXPOSURE_L	0x3e05
#define	SC235HAI_EXPOSURE_MIN		1
#define	SC235HAI_EXPOSURE_STEP		1
#define SC235HAI_VTS_MAX		0x7fff

#define SC235HAI_REG_DIG_GAIN		0x3e06
#define SC235HAI_REG_DIG_FINE_GAIN	0x3e07
#define SC235HAI_REG_ANA_GAIN		0x3e08
#define SC235HAI_REG_ANA_FINE_GAIN	0x3e09

#define SC235HAI_REG_SDIG_GAIN		0x3e10
#define SC235HAI_REG_SDIG_FINE_GAIN	0x3e11
#define SC235HAI_REG_SANA_GAIN		0x3e12
#define SC235HAI_REG_SANA_FINE_GAIN	0x3e13
#define SC235HAI_REG_MAX_SEXPOSURE_H	0x3e23
#define SC235HAI_REG_MAX_SEXPOSURE_L	0x3e24

#define SC235HAI_GAIN_MIN		0x20
#define SC235HAI_GAIN_MAX		(117 * 16 * 32)       // 116.55*15.875*32
#define SC235HAI_GAIN_STEP		1
#define SC235HAI_GAIN_DEFAULT		0x40
#define SC235HAI_LGAIN			0
#define SC235HAI_SGAIN			1

#define SC235HAI_REG_GROUP_HOLD		0x3812
#define SC235HAI_GROUP_HOLD_START	0x00
#define SC235HAI_GROUP_HOLD_END		0x30

#define SC235HAI_REG_HIGH_TEMP_H	0x3974
#define SC235HAI_REG_HIGH_TEMP_L	0x3975

#define SC235HAI_REG_TEST_PATTERN	0x4501
#define SC235HAI_TEST_PATTERN_BIT_MASK	BIT(3)

#define SC235HAI_REG_VTS_H		0x320e
#define SC235HAI_REG_VTS_L		0x320f

#define SC235HAI_FLIP_MIRROR_REG	0x3221

#define SC235HAI_FETCH_EXP_H(VAL)	(((VAL) >> 12) & 0xF)
#define SC235HAI_FETCH_EXP_M(VAL)	(((VAL) >> 4) & 0xFF)
#define SC235HAI_FETCH_EXP_L(VAL)	(((VAL) & 0xF) << 4)

#define SC235HAI_FETCH_AGAIN_H(VAL)	(((VAL) >> 8) & 0x03)
#define SC235HAI_FETCH_AGAIN_L(VAL)	((VAL) & 0xFF)

#define SC235HAI_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL | 0x06 : VAL & 0xf9)
#define SC235HAI_FETCH_FLIP(VAL, ENABLE)	(ENABLE ? VAL | 0x60 : VAL & 0x9f)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define SC235HAI_REG_VALUE_08BIT	1
#define SC235HAI_REG_VALUE_16BIT	2
#define SC235HAI_REG_VALUE_24BIT	3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define SC235HAI_NAME			"sc235hai"

static const char *const sc235hai_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define SC235HAI_NUM_SUPPLIES ARRAY_SIZE(sc235hai_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct sc235hai_mode {
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
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct sc235hai {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[SC235HAI_NUM_SUPPLIES];

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
	const struct sc235hai_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	enum rkmodule_sync_mode	sync_mode;
	u32			cur_vts;
	bool			has_init_exp;
	bool			is_thunderboot;
	bool			is_first_streamoff;
	u32			standby_hw;
	bool			is_standby;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	struct cam_sw_info	*cam_sw_info;
};

#define to_sc235hai(sd) container_of(sd, struct sc235hai, subdev)

/*
 * Xclk 27Mhz
 */
static const struct regval sc235hai_global_regs[] = {
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval sc235hai_linear_10_640x480_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x2d},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0x3c},
	{0x3204, 0x07},
	{0x3205, 0x87},
	{0x3206, 0x04},
	{0x3207, 0x03},
	{0x3208, 0x02},
	{0x3209, 0x80},
	{0x320a, 0x01},
	{0x320b, 0xe0},
	{0x320e, 0x02},
	{0x320f, 0x32},
	{0x3210, 0x00},
	{0x3211, 0xa2},
	{0x3212, 0x00},
	{0x3213, 0x02},
	{0x3215, 0x31},
	{0x3220, 0x01},
	{0x3301, 0x09},
	{0x3304, 0x50},
	{0x3306, 0x48},
	{0x3308, 0x18},
	{0x3309, 0x68},
	{0x330a, 0x00},
	{0x330b, 0xc0},
	{0x331e, 0x41},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x0b},
	{0x3392, 0x0f},
	{0x3393, 0x0c},
	{0x3394, 0x0d},
	{0x3395, 0x60},
	{0x3396, 0x48},
	{0x3397, 0x49},
	{0x3398, 0x4f},
	{0x3399, 0x0a},
	{0x339a, 0x0f},
	{0x339b, 0x14},
	{0x339c, 0x60},
	{0x33a2, 0x04},
	{0x33af, 0x40},
	{0x33b1, 0x80},
	{0x33b3, 0x40},
	{0x33b9, 0x0a},
	{0x33f9, 0x70},
	{0x33fb, 0x90},
	{0x33fc, 0x4b},
	{0x33fd, 0x5f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x4f},
	{0x34a8, 0x30},
	{0x34a9, 0x20},
	{0x34aa, 0x00},
	{0x34ab, 0xe0},
	{0x34ac, 0x01},
	{0x34ad, 0x00},
	{0x34f8, 0x5f},
	{0x34f9, 0x10},
	{0x3630, 0xc0},
	{0x3633, 0x44},
	{0x3637, 0x29},
	{0x363b, 0x20},
	{0x3670, 0x09},
	{0x3674, 0xb0},
	{0x3675, 0x80},
	{0x3676, 0x88},
	{0x367c, 0x40},
	{0x367d, 0x49},
	{0x3690, 0x44},
	{0x3691, 0x44},
	{0x3692, 0x54},
	{0x369c, 0x49},
	{0x369d, 0x4f},
	{0x36ae, 0x4b},
	{0x36af, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x9b},
	{0x36b2, 0xb7},
	{0x36d0, 0x01},
	{0x36ea, 0x0b},
	{0x36eb, 0x04},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x370f, 0x01},
	{0x3722, 0x17},
	{0x3728, 0x90},
	{0x37b0, 0x17},
	{0x37b1, 0x17},
	{0x37b2, 0x97},
	{0x37b3, 0x4b},
	{0x37b4, 0x4f},
	{0x37fa, 0x0b},
	{0x37fb, 0x24},
	{0x37fc, 0x10},
	{0x37fd, 0x22},
	{0x3901, 0x02},
	{0x3902, 0xc5},
	{0x3904, 0x04},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x3909, 0x00},
	{0x390a, 0x00},
	{0x391f, 0x04},
	{0x3933, 0x84},
	{0x3934, 0x02},
	{0x3940, 0x62},
	{0x3941, 0x00},
	{0x3942, 0x04},
	{0x3943, 0x03},
	{0x3e00, 0x00},
	{0x3e01, 0x45},
	{0x3e02, 0xb0},
	{0x440e, 0x02},
	{0x450d, 0x11},
	{0x4819, 0x05},
	{0x481b, 0x03},
	{0x481d, 0x0a},
	{0x481f, 0x02},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x02},
	{0x4827, 0x03},
	{0x4829, 0x04},
	{0x5000, 0x46},
	{0x5010, 0x01},
	{0x5787, 0x08},
	{0x5788, 0x03},
	{0x5789, 0x00},
	{0x578a, 0x10},
	{0x578b, 0x08},
	{0x578c, 0x00},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x08},
	{0x5795, 0x00},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5900, 0xf1},
	{0x5901, 0x04},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x3f},
	{0x5ae3, 0x38},
	{0x5ae4, 0x28},
	{0x5ae5, 0x3f},
	{0x5ae6, 0x38},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x3c},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x3c},
	{0x5aed, 0x2c},
	{0x5af4, 0x3f},
	{0x5af5, 0x38},
	{0x5af6, 0x28},
	{0x5af7, 0x3f},
	{0x5af8, 0x38},
	{0x5af9, 0x28},
	{0x5afa, 0x3f},
	{0x5afb, 0x3c},
	{0x5afc, 0x2c},
	{0x5afd, 0x3f},
	{0x5afe, 0x3c},
	{0x5aff, 0x2c},
	{0x36e9, 0x20},
	{0x37f9, 0x24},
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 60fps
 * mipi_datarate per lane 371.25Mbps, 2lane
 */
static const struct regval sc235hai_linear_10_1920x1080_60fps_regs[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x02},
	{0x3058, 0x21},
	{0x3059, 0x53},
	{0x305a, 0x40},
	{0x3250, 0x00},
	{0x3301, 0x0a},
	{0x3302, 0x20},
	{0x3304, 0x90},
	{0x3305, 0x00},
	{0x3306, 0x78},
	{0x3309, 0xd0},
	{0x330b, 0xe8},
	{0x330d, 0x08},
	{0x331c, 0x04},
	{0x331e, 0x81},
	{0x331f, 0xc1},
	{0x3323, 0x06},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x3364, 0x5e},
	{0x336c, 0x8c},
	{0x337f, 0x13},
	{0x338f, 0x80},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0xb8},
	{0x3393, 0x0e},
	{0x3394, 0x14},
	{0x3395, 0x10},
	{0x3396, 0x88},
	{0x3397, 0x98},
	{0x3398, 0xf8},
	{0x3399, 0x0a},
	{0x339a, 0x0e},
	{0x339b, 0x10},
	{0x339c, 0x14},
	{0x33ae, 0x80},
	{0x33af, 0xc0},
	{0x33b2, 0x50},
	{0x33b3, 0x08},
	{0x33f8, 0x00},
	{0x33f9, 0x78},
	{0x33fa, 0x00},
	{0x33fb, 0x78},
	{0x33fc, 0x48},
	{0x33fd, 0x78},
	{0x349f, 0x03},
	{0x34a6, 0x40},
	{0x34a7, 0x58},
	{0x34a8, 0x08},
	{0x34a9, 0x0c},
	{0x34f8, 0x78},
	{0x34f9, 0x18},
	{0x3619, 0x20},
	{0x361a, 0x90},
	{0x3633, 0x44},
	{0x3637, 0x5c},
	{0x363c, 0xc0},
	{0x363d, 0x02},
	{0x3660, 0x80},
	{0x3661, 0x81},
	{0x3662, 0x8f},
	{0x3663, 0x81},
	{0x3664, 0x81},
	{0x3665, 0x82},
	{0x3666, 0x8f},
	{0x3667, 0x08},
	{0x3668, 0x80},
	{0x3669, 0x88},
	{0x366a, 0x98},
	{0x366b, 0xb8},
	{0x366c, 0xf8},
	{0x3670, 0xc2},
	{0x3671, 0xc2},
	{0x3672, 0x98},
	{0x3680, 0x43},
	{0x3681, 0x54},
	{0x3682, 0x54},
	{0x36c0, 0x80},
	{0x36c1, 0x88},
	{0x36c8, 0x88},
	{0x36c9, 0xb8},
	{0x3718, 0x04},
	{0x3722, 0x8b},
	{0x3724, 0xd1},
	{0x3741, 0x08},
	{0x3770, 0x17},
	{0x3771, 0x9b},
	{0x3772, 0x9b},
	{0x37c0, 0x88},
	{0x37c1, 0xb8},
	{0x3902, 0xc0},
	{0x3903, 0x40},
	{0x3909, 0x00},
	{0x391f, 0x41},
	{0x3926, 0xe0},
	{0x3933, 0x80},
	{0x3934, 0x02},
	{0x3937, 0x6f},
	{0x3e00, 0x00},
	{0x3e01, 0x8b},
	{0x3e02, 0xf0},
	{0x3e08, 0x00},
	{0x4509, 0x20},
	{0x450d, 0x07},
	{0x5780, 0x76},
	{0x5784, 0x10},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x08},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x08},
	{0x578d, 0x40},
	{0x5792, 0x04},
	{0x5795, 0x04},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x36e9, 0x24},
	{0x37f9, 0x24},
	//{0x0100, 0x01},
	{REG_NULL, 0x00},
};


static const struct sc235hai_mode supported_modes[] = {
	{
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.exp_def = 0x0460,
		.hts_def = 0x44C * 2,
		.vts_def = 0x0465,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = sc235hai_linear_10_1920x1080_60fps_regs,
		.hdr_mode = NO_HDR,
		.bpp = 10,
		.mipi_freq_idx = 0,
		.vc[PAD0] = 0,
	},
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,
};

static const s64 link_freq_menu_items[] = {
	SC235HAI_LINK_FREQ_371
};

static const char *const sc235hai_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int sc235hai_write_reg(struct i2c_client *client, u16 reg,
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

static int sc235hai_write_array(struct i2c_client *client,
				const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = sc235hai_write_reg(client, regs[i].addr,
					 SC235HAI_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int sc235hai_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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
static int sc235hai_set_gain_reg(struct sc235hai *sc235hai, u32 total_gain, int mode)
{
	u8 Coarse_gain = 1, DIG_gain = 1;
	u32 Dcg_gainx100 = 1, ANA_Fine_gainx64 = 1;
	u8 Coarse_gain_reg = 0, DIG_gain_reg = 0;
	u8 ANA_Fine_gain_reg = 0x20, DIG_Fine_gain_reg = 0x80;
	int ret = 0;

	total_gain = total_gain * 32;
	if (total_gain < SC235HAI_GAIN_MIN * 32)
		total_gain = SC235HAI_GAIN_MIN;
	else if (total_gain > SC235HAI_GAIN_MAX * 32)
		total_gain = SC235HAI_GAIN_MAX * 32;

	if (total_gain < 2 * 1024) {		/* Start again  1.0x ~ 2.0x */
		Dcg_gainx100 = 100;
		Coarse_gain = 1; DIG_gain = 1;
		Coarse_gain_reg = 0x00;
		DIG_gain_reg = 0x0; DIG_Fine_gain_reg = 0x80;
	} else if (total_gain < 3788) {		/* 2.0x ~ 3.7x  1024 * 3.7 = 3788*/
		Dcg_gainx100 = 100;
		Coarse_gain = 2; DIG_gain = 1;
		Coarse_gain_reg = 0x01;
		DIG_gain_reg = 0x0; DIG_Fine_gain_reg = 0x80;
	} else if (total_gain < 7577) {		/* 3.7x ~ 7.4x  1024 * 7.4 = 7577 */
		Dcg_gainx100 = 370;
		Coarse_gain = 1; DIG_gain = 1;
		Coarse_gain_reg = 0x80;
		DIG_gain_reg = 0x0; DIG_Fine_gain_reg = 0x80;
	} else if (total_gain < 15115) {	/* 7.4x ~ 14.8x  1024 * 14.8 = 15115*/
		Dcg_gainx100 = 370;
		Coarse_gain = 2; DIG_gain = 1;
		Coarse_gain_reg = 0x81;
		DIG_gain_reg = 0x0; DIG_Fine_gain_reg = 0x80;
	} else if (total_gain < 30310) {	/* 14.8x ~ 29.6x  1024 * 29.6 = 30310*/
		Dcg_gainx100 = 370;
		Coarse_gain = 4; DIG_gain = 1;
		Coarse_gain_reg = 0x83;
		DIG_gain_reg = 0x0; DIG_Fine_gain_reg = 0x80;
	} else if (total_gain < 60620) {	/* 29.6x ~ 59.2x  1024 * 59.2 = 60620*/
		Dcg_gainx100 = 370;
		Coarse_gain = 8; DIG_gain = 1;
		Coarse_gain_reg = 0x87;
		DIG_gain_reg = 0x0; DIG_Fine_gain_reg = 0x80;
	} else if (total_gain <= 119347) {
		/* End again 59.2x ~ 116.55x  1024 * 116.55 = 119347*/
		Dcg_gainx100 = 370;
		Coarse_gain = 16; DIG_gain = 1;
		Coarse_gain_reg = 0x8f;
		DIG_gain_reg = 0x0; DIG_Fine_gain_reg = 0x80;
	} else if (total_gain <= 119347 * 2) {	/* Start dgain 1.0x ~ 2.0x */
		Dcg_gainx100 = 370;
		Coarse_gain = 16; DIG_gain = 1;
		Coarse_gain_reg = 0x8f; ANA_Fine_gain_reg = 0x3f;
		DIG_gain_reg = 0x0; DIG_Fine_gain_reg = 0x80;
		ANA_Fine_gainx64 = 127;
	} else if (total_gain <= 119347 * 4) {	/* 2.0x ~ 4.0x */
		Dcg_gainx100 = 370;
		Coarse_gain = 16; DIG_gain = 2;
		Coarse_gain_reg = 0x8f; ANA_Fine_gain_reg = 0x3f;
		DIG_gain_reg = 0x1; DIG_Fine_gain_reg = 0x80;
		ANA_Fine_gainx64 = 127;
	} else if (total_gain <= 119347 * 8) {	/* 4.0x ~ 8.0x */
		Dcg_gainx100 = 370;
		Coarse_gain = 16; DIG_gain = 4;
		Coarse_gain_reg = 0x8f; ANA_Fine_gain_reg = 0x3f;
		DIG_gain_reg = 0x3; DIG_Fine_gain_reg = 0x80;
		ANA_Fine_gainx64 = 127;
	} else if (total_gain <= 1894633) {	/* End dgain 8.0x ~ 15.875x 119347*15.875=1894633*/
		Dcg_gainx100 = 370;
		Coarse_gain = 16; DIG_gain = 8;
		Coarse_gain_reg = 0x8f; ANA_Fine_gain_reg = 0x3f;
		DIG_gain_reg = 0x7; DIG_Fine_gain_reg = 0x80;
		ANA_Fine_gainx64 = 127;
	}

	if (total_gain < 3776)
		ANA_Fine_gain_reg = abs(100 * total_gain / (Dcg_gainx100 * Coarse_gain) / 32);  // *NOPAD*
	else if (total_gain == 3776)	/* 3.688x */
		ANA_Fine_gain_reg = 0x3B;
	else if (total_gain < 119347)	/* again */
		ANA_Fine_gain_reg = abs(100 * total_gain / (Dcg_gainx100 * Coarse_gain) / 32);  // *NOPAD*
	else				/* dgain */
		DIG_Fine_gain_reg = abs(800 * total_gain / (Dcg_gainx100 * Coarse_gain *  // *NOPAD*
					DIG_gain) / ANA_Fine_gainx64);

	if (mode == SC235HAI_LGAIN) {
		ret = sc235hai_write_reg(sc235hai->client,
					 SC235HAI_REG_DIG_GAIN,
					 SC235HAI_REG_VALUE_08BIT,
					 DIG_gain_reg & 0xF);
		ret |= sc235hai_write_reg(sc235hai->client,
					  SC235HAI_REG_DIG_FINE_GAIN,
					  SC235HAI_REG_VALUE_08BIT,
					  DIG_Fine_gain_reg);
		ret |= sc235hai_write_reg(sc235hai->client,
					  SC235HAI_REG_ANA_GAIN,
					  SC235HAI_REG_VALUE_08BIT,
					  Coarse_gain_reg);
		ret |= sc235hai_write_reg(sc235hai->client,
					  SC235HAI_REG_ANA_FINE_GAIN,
					  SC235HAI_REG_VALUE_08BIT,
					  ANA_Fine_gain_reg);
	} else {
		ret = sc235hai_write_reg(sc235hai->client,
					 SC235HAI_REG_SDIG_GAIN,
					 SC235HAI_REG_VALUE_08BIT,
					 DIG_gain_reg & 0xF);
		ret |= sc235hai_write_reg(sc235hai->client,
					  SC235HAI_REG_SDIG_FINE_GAIN,
					  SC235HAI_REG_VALUE_08BIT,
					  DIG_Fine_gain_reg);
		ret |= sc235hai_write_reg(sc235hai->client,
					  SC235HAI_REG_SANA_GAIN,
					  SC235HAI_REG_VALUE_08BIT,
					  Coarse_gain_reg);
		ret |= sc235hai_write_reg(sc235hai->client,
					  SC235HAI_REG_SANA_FINE_GAIN,
					  SC235HAI_REG_VALUE_08BIT,
					  ANA_Fine_gain_reg);
	}

	return ret;
}

static int sc235hai_set_hdrae(struct sc235hai *sc235hai,
			      struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;

	if (!sc235hai->has_init_exp && !sc235hai->streaming) {
		sc235hai->init_hdrae_exp = *ae;
		sc235hai->has_init_exp = true;
		dev_dbg(&sc235hai->client->dev, "sc235hai don't stream, record exp for hdr!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&sc235hai->client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (sc235hai->cur_mode->hdr_mode == HDR_X2) {
		//2 stagger
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	//set exposure
	l_exp_time = l_exp_time * 2;
	s_exp_time = s_exp_time * 2;
	if (l_exp_time > 4362)                  //(2250 - 64 - 5) * 2
		l_exp_time = 4362;
	if (s_exp_time > 404)                //(64 - 5) * 2
		s_exp_time = 404;

	ret = sc235hai_write_reg(sc235hai->client,
				 SC235HAI_REG_EXPOSURE_H,
				 SC235HAI_REG_VALUE_08BIT,
				 SC235HAI_FETCH_EXP_H(l_exp_time));
	ret |= sc235hai_write_reg(sc235hai->client,
				  SC235HAI_REG_EXPOSURE_M,
				  SC235HAI_REG_VALUE_08BIT,
				  SC235HAI_FETCH_EXP_M(l_exp_time));
	ret |= sc235hai_write_reg(sc235hai->client,
				  SC235HAI_REG_EXPOSURE_L,
				  SC235HAI_REG_VALUE_08BIT,
				  SC235HAI_FETCH_EXP_L(l_exp_time));
	ret |= sc235hai_write_reg(sc235hai->client,
				  SC235HAI_REG_SEXPOSURE_M,
				  SC235HAI_REG_VALUE_08BIT,
				  SC235HAI_FETCH_EXP_M(s_exp_time));
	ret |= sc235hai_write_reg(sc235hai->client,
				  SC235HAI_REG_SEXPOSURE_L,
				  SC235HAI_REG_VALUE_08BIT,
				  SC235HAI_FETCH_EXP_L(s_exp_time));

	ret |= sc235hai_set_gain_reg(sc235hai, l_a_gain, SC235HAI_LGAIN);
	ret |= sc235hai_set_gain_reg(sc235hai, s_a_gain, SC235HAI_SGAIN);
	return ret;

	return ret;
}

static int sc235hai_get_reso_dist(const struct sc235hai_mode *mode,
				  struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc235hai_mode *
sc235hai_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = sc235hai_get_reso_dist(&supported_modes[i], framefmt);
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

static int sc235hai_set_rates(struct sc235hai *sc235hai)
{
	const struct sc235hai_mode *mode = sc235hai->cur_mode;
	s64 h_blank, vblank_def;
	int ret = 0;
	u64 pixel_rate = 0;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(sc235hai->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(sc235hai->vblank, vblank_def,
				 SC235HAI_VTS_MAX - mode->height,
				 1, vblank_def);
	pixel_rate = (u32)link_freq_menu_items[mode->mipi_freq_idx] /
		     mode->bpp * 2 * SC235HAI_LANES;
	__v4l2_ctrl_s_ctrl_int64(sc235hai->pixel_rate,
				 pixel_rate);
	__v4l2_ctrl_s_ctrl(sc235hai->link_freq,
			   mode->mipi_freq_idx);

	return ret;
}

static int sc235hai_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct sc235hai *sc235hai = to_sc235hai(sd);
	const struct sc235hai_mode *mode;

	mutex_lock(&sc235hai->mutex);

	mode = sc235hai_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&sc235hai->mutex);
		return -ENOTTY;
#endif
	} else {
		sc235hai->cur_mode = mode;
		sc235hai_set_rates(sc235hai);
		sc235hai->cur_fps = mode->max_fps;
	}

	mutex_unlock(&sc235hai->mutex);

	return 0;
}

static int sc235hai_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct sc235hai *sc235hai = to_sc235hai(sd);
	const struct sc235hai_mode *mode = sc235hai->cur_mode;

	mutex_lock(&sc235hai->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&sc235hai->mutex);
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
	mutex_unlock(&sc235hai->mutex);

	return 0;
}

static int sc235hai_enum_mbus_code(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int sc235hai_enum_frame_sizes(struct v4l2_subdev *sd,
				     struct v4l2_subdev_state *sd_state,
				     struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int sc235hai_enable_test_pattern(struct sc235hai *sc235hai, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc235hai_read_reg(sc235hai->client, SC235HAI_REG_TEST_PATTERN,
				SC235HAI_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC235HAI_TEST_PATTERN_BIT_MASK;
	else
		val &= ~SC235HAI_TEST_PATTERN_BIT_MASK;

	ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_TEST_PATTERN,
				  SC235HAI_REG_VALUE_08BIT, val);
	return ret;
}

static int sc235hai_g_frame_interval(struct v4l2_subdev *sd,
				     struct v4l2_subdev_frame_interval *fi)
{
	struct sc235hai *sc235hai = to_sc235hai(sd);
	const struct sc235hai_mode *mode = sc235hai->cur_mode;

	if (sc235hai->streaming)
		fi->interval = sc235hai->cur_fps;
	else
		fi->interval = mode->max_fps;

	return 0;
}

static const struct sc235hai_mode *sc235hai_find_mode(struct sc235hai *sc235hai, int fps)
{
	const struct sc235hai_mode *mode = NULL;
	const struct sc235hai_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		mode = &supported_modes[i];
		if (mode->width == sc235hai->cur_mode->width &&
		    mode->height == sc235hai->cur_mode->height &&
		    mode->hdr_mode == sc235hai->cur_mode->hdr_mode &&
		    mode->bus_fmt == sc235hai->cur_mode->bus_fmt) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator, mode->max_fps.numerator);
			if (cur_fps == fps) {
				match = mode;
				break;
			}
		}
	}
	return match;
}

static int sc235hai_s_frame_interval(struct v4l2_subdev *sd,
				     struct v4l2_subdev_frame_interval *fi)
{
	struct sc235hai *sc235hai = to_sc235hai(sd);
	const struct sc235hai_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	int fps;

	if (sc235hai->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = sc235hai_find_mode(sc235hai, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	sc235hai->cur_mode = mode;

	sc235hai_set_rates(sc235hai);

	return 0;
}

static int sc235hai_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				  struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = SC235HAI_LANES;

	return 0;
}

static void sc235hai_get_module_inf(struct sc235hai *sc235hai,
				    struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC235HAI_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc235hai->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc235hai->len_name, sizeof(inf->base.lens));
}

static int sc235hai_get_channel_info(struct sc235hai *sc235hai,
				     struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = sc235hai->cur_mode->vc[ch_info->index];
	ch_info->width = sc235hai->cur_mode->width;
	ch_info->height = sc235hai->cur_mode->height;
	ch_info->bus_fmt = sc235hai->cur_mode->bus_fmt;
	return 0;
}

static long sc235hai_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc235hai *sc235hai = to_sc235hai(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	u32 i, w, h;
	long ret = 0;
	u32 stream = 0;
	u32 *sync_mode = NULL;
	int cur_best_fit = -1;
	int cur_best_fit_dist = -1;
	int cur_dist, cur_fps, dst_fps;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc235hai_get_module_inf(sc235hai, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = sc235hai->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		if (hdr->hdr_mode == sc235hai->cur_mode->hdr_mode)
			return 0;
		w = sc235hai->cur_mode->width;
		h = sc235hai->cur_mode->height;
		dst_fps = DIV_ROUND_CLOSEST(sc235hai->cur_mode->max_fps.denominator,
					    sc235hai->cur_mode->max_fps.numerator);
		for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode) {
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
			dev_err(&sc235hai->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			sc235hai->cur_mode = &supported_modes[cur_best_fit];
			sc235hai_set_rates(sc235hai);
			sc235hai->cur_fps = sc235hai->cur_mode->max_fps;
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		sc235hai_set_hdrae(sc235hai, arg);
		if (sc235hai->cam_sw_info)
			memcpy(&sc235hai->cam_sw_info->hdr_ae, (struct preisp_hdrae_exp_s *)(arg),
			       sizeof(struct preisp_hdrae_exp_s));
		break;
	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);
		if (sc235hai->standby_hw) { // hardware standby
			if (stream) {
				if (!IS_ERR(sc235hai->pwdn_gpio))
					gpiod_set_value_cansleep(sc235hai->pwdn_gpio, 1);
				// Make sure __v4l2_ctrl_handler_setup can be called correctly
				sc235hai->is_standby = false;

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
				if (__v4l2_ctrl_handler_setup(&sc235hai->ctrl_handler))
					dev_err(&sc235hai->client->dev, "__v4l2_ctrl_handler_setup fail!");
				if (sc235hai->cur_mode->hdr_mode != NO_HDR) {
					if (sc235hai->cam_sw_info) {
						ret = sc235hai_ioctl(&sc235hai->subdev,
								     PREISP_CMD_SET_HDRAE_EXP,
								     &sc235hai->cam_sw_info->hdr_ae);
						if (ret) {
							dev_err(&sc235hai->client->dev,
								"init exp fail in hdr mode\n");
							return ret;
						}
					}
				}
#endif

				// according sensor FAE: to save power to set 0x302c,0x363c,0x36e9,0x37f9
				ret = sc235hai_write_reg(sc235hai->client, 0x302c,
							 SC235HAI_REG_VALUE_08BIT, 0x00);
				ret |= sc235hai_write_reg(sc235hai->client, 0x363c,
							  SC235HAI_REG_VALUE_08BIT, 0x8e);
				ret |= sc235hai_write_reg(sc235hai->client, 0x36e9,
							  SC235HAI_REG_VALUE_08BIT, 0x24);
				ret |= sc235hai_write_reg(sc235hai->client, 0x37f9,
							  SC235HAI_REG_VALUE_08BIT, 0x24);
				ret |= sc235hai_write_reg(sc235hai->client, 0x3018,
							  SC235HAI_REG_VALUE_08BIT, 0x3A);

				ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_MIPI_CTRL,
							  SC235HAI_REG_VALUE_08BIT,
							  SC235HAI_MIPI_CTRL_ON);
				ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_CTRL_MODE,
							  SC235HAI_REG_VALUE_08BIT,
							  SC235HAI_MODE_STREAMING);

				dev_info(&sc235hai->client->dev, "quickstream, streaming on: exit hw standby mode\n");
			} else {
				ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_CTRL_MODE,
							  SC235HAI_REG_VALUE_08BIT,
							  SC235HAI_MODE_SW_STANDBY);
				ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_MIPI_CTRL,
							  SC235HAI_REG_VALUE_08BIT,
							  SC235HAI_MIPI_CTRL_OFF);
				ret |= sc235hai_write_reg(sc235hai->client, 0x363c,
							  SC235HAI_REG_VALUE_08BIT, 0xae);
				ret |= sc235hai_write_reg(sc235hai->client, 0x36e9,
							  SC235HAI_REG_VALUE_08BIT, 0xa4);
				ret |= sc235hai_write_reg(sc235hai->client, 0x37f9,
							  SC235HAI_REG_VALUE_08BIT, 0xa4);
				ret |= sc235hai_write_reg(sc235hai->client, 0x3018,
							  SC235HAI_REG_VALUE_08BIT, 0x3F);

				if (!IS_ERR(sc235hai->pwdn_gpio))
					gpiod_set_value_cansleep(sc235hai->pwdn_gpio, 0);

				dev_info(&sc235hai->client->dev, "quickstream, streaming off: enter hw standby mode\n");
				sc235hai->is_standby = true;
			}
		} else { // software standby
			if (stream) {
				// according sensor FAE: to save power to set 0x302c,0x363c,0x36e9,0x37f9
				ret = sc235hai_write_reg(sc235hai->client, 0x302c,
							 SC235HAI_REG_VALUE_08BIT, 0x00);
				ret |= sc235hai_write_reg(sc235hai->client, 0x363c,
							  SC235HAI_REG_VALUE_08BIT, 0x8e);
				ret |= sc235hai_write_reg(sc235hai->client, 0x36e9,
							  SC235HAI_REG_VALUE_08BIT, 0x24);
				ret |= sc235hai_write_reg(sc235hai->client, 0x37f9,
							  SC235HAI_REG_VALUE_08BIT, 0x24);
				ret |= sc235hai_write_reg(sc235hai->client, 0x3018,
							  SC235HAI_REG_VALUE_08BIT, 0x3A);

				ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_MIPI_CTRL,
							  SC235HAI_REG_VALUE_08BIT,
							  SC235HAI_MIPI_CTRL_ON);
				ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_CTRL_MODE,
							  SC235HAI_REG_VALUE_08BIT,
							  SC235HAI_MODE_STREAMING);
				dev_info(&sc235hai->client->dev, "quickstream, streaming on: exit soft standby mode\n");
			} else {
				// according sensor FAE: to save power to set 0x302c,0x363c,0x36e9,0x37f9
				ret = sc235hai_write_reg(sc235hai->client, 0x302c,
							 SC235HAI_REG_VALUE_08BIT, 0x01);

				ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_CTRL_MODE,
							  SC235HAI_REG_VALUE_08BIT,
							  SC235HAI_MODE_SW_STANDBY);
				ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_REG_MIPI_CTRL,
							  SC235HAI_REG_VALUE_08BIT,
							  SC235HAI_MIPI_CTRL_OFF);

				ret |= sc235hai_write_reg(sc235hai->client, 0x363c,
							  SC235HAI_REG_VALUE_08BIT, 0xae);
				ret |= sc235hai_write_reg(sc235hai->client, 0x36e9,
							  SC235HAI_REG_VALUE_08BIT, 0xa4);
				ret |= sc235hai_write_reg(sc235hai->client, 0x37f9,
							  SC235HAI_REG_VALUE_08BIT, 0xa4);
				ret |= sc235hai_write_reg(sc235hai->client, 0x3018,
							  SC235HAI_REG_VALUE_08BIT, 0x3F);
				dev_info(&sc235hai->client->dev, "quickstream, streaming off: enter soft standby mode\n");
			}
		}
		break;
	case RKMODULE_GET_SYNC_MODE:
		sync_mode = (u32 *)arg;
		*sync_mode = sc235hai->sync_mode;
		break;
	case RKMODULE_SET_SYNC_MODE:
		sync_mode = (u32 *)arg;
		sc235hai->sync_mode = *sync_mode;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = sc235hai_get_channel_info(sc235hai, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long sc235hai_compat_ioctl32(struct v4l2_subdev *sd,
				    unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	struct preisp_hdrae_exp_s *hdrae;
	long ret;
	u32 stream = 0;
	u32 *sync_mode = NULL;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc235hai_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				return -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc235hai_ioctl(sd, cmd, hdr);
		if (!ret) {
			ret = copy_to_user(up, hdr, sizeof(*hdr));
			if (ret)
				return -EFAULT;
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

		ret = sc235hai_ioctl(sd, cmd, hdr);
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

		ret = sc235hai_ioctl(sd, cmd, hdrae);
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		if (copy_from_user(&stream, up, sizeof(u32)))
			return -EFAULT;

		ret = sc235hai_ioctl(sd, cmd, &stream);
		break;
	case RKMODULE_GET_SYNC_MODE:
		ret = sc235hai_ioctl(sd, cmd, &sync_mode);
		if (!ret) {
			ret = copy_to_user(up, &sync_mode, sizeof(u32));
			if (ret)
				ret = -EFAULT;
		}
		break;
	case RKMODULE_SET_SYNC_MODE:
		ret = copy_from_user(&sync_mode, up, sizeof(u32));
		if (!ret)
			ret = sc235hai_ioctl(sd, cmd, &sync_mode);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc235hai_ioctl(sd, cmd, ch_info);
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

static int __sc235hai_start_stream(struct sc235hai *sc235hai)
{
	int ret;

	dev_info(&sc235hai->client->dev,
		 "%dx%d@%d, mode %d, vts 0x%x\n",
		 sc235hai->cur_mode->width,
		 sc235hai->cur_mode->height,
		 sc235hai->cur_fps.denominator / sc235hai->cur_fps.numerator,
		 sc235hai->cur_mode->hdr_mode,
		 sc235hai->cur_vts);

	if (!sc235hai->is_thunderboot) {
		ret = sc235hai_write_array(sc235hai->client, sc235hai->cur_mode->reg_list);
		if (ret)
			return ret;
		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&sc235hai->ctrl_handler);
		if (ret)
			return ret;
		if (sc235hai->has_init_exp && sc235hai->cur_mode->hdr_mode != NO_HDR) {
			ret = sc235hai_ioctl(&sc235hai->subdev, PREISP_CMD_SET_HDRAE_EXP,
					     &sc235hai->init_hdrae_exp);
			if (ret) {
				dev_err(&sc235hai->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	return sc235hai_write_reg(sc235hai->client, SC235HAI_REG_CTRL_MODE,
				  SC235HAI_REG_VALUE_08BIT, SC235HAI_MODE_STREAMING);
}

static int __sc235hai_stop_stream(struct sc235hai *sc235hai)
{
	sc235hai->has_init_exp = false;
	if (sc235hai->is_thunderboot) {
		sc235hai->is_first_streamoff = true;
		pm_runtime_put(&sc235hai->client->dev);
	}
	return sc235hai_write_reg(sc235hai->client, SC235HAI_REG_CTRL_MODE,
				  SC235HAI_REG_VALUE_08BIT, SC235HAI_MODE_SW_STANDBY);
}

static int __sc235hai_power_on(struct sc235hai *sc235hai);
static int sc235hai_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc235hai *sc235hai = to_sc235hai(sd);
	struct i2c_client *client = sc235hai->client;
	int ret = 0;

	mutex_lock(&sc235hai->mutex);
	on = !!on;
	if (on == sc235hai->streaming)
		goto unlock_and_return;

	if (on) {
		if (sc235hai->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			sc235hai->is_thunderboot = false;
			__sc235hai_power_on(sc235hai);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __sc235hai_start_stream(sc235hai);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc235hai_stop_stream(sc235hai);
		pm_runtime_put(&client->dev);
	}

	sc235hai->streaming = on;

unlock_and_return:
	mutex_unlock(&sc235hai->mutex);

	return ret;
}

static int sc235hai_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc235hai *sc235hai = to_sc235hai(sd);
	struct i2c_client *client = sc235hai->client;
	int ret = 0;

	mutex_lock(&sc235hai->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc235hai->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		if (!sc235hai->is_thunderboot) {
			ret = sc235hai_write_array(sc235hai->client, sc235hai_global_regs);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		sc235hai->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc235hai->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc235hai->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 sc235hai_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, SC235HAI_XVCLK_FREQ / 1000 / 1000);
}

static int __sc235hai_power_on(struct sc235hai *sc235hai)
{
	int ret;
	u32 delay_us;
	struct device *dev = &sc235hai->client->dev;

	if (!IS_ERR_OR_NULL(sc235hai->pins_default)) {
		ret = pinctrl_select_state(sc235hai->pinctrl,
					   sc235hai->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc235hai->xvclk, SC235HAI_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(sc235hai->xvclk) != SC235HAI_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(sc235hai->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	cam_sw_regulator_bulk_init(sc235hai->cam_sw_info,
				   SC235HAI_NUM_SUPPLIES, sc235hai->supplies);

	if (sc235hai->is_thunderboot)
		return 0;

	if (!IS_ERR(sc235hai->reset_gpio))
		gpiod_set_value_cansleep(sc235hai->reset_gpio, 0);

	ret = regulator_bulk_enable(SC235HAI_NUM_SUPPLIES, sc235hai->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc235hai->reset_gpio))
		gpiod_set_value_cansleep(sc235hai->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(sc235hai->pwdn_gpio))
		gpiod_set_value_cansleep(sc235hai->pwdn_gpio, 1);

	if (!IS_ERR(sc235hai->reset_gpio))
		usleep_range(6000, 8000);
	else
		usleep_range(12000, 16000);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = sc235hai_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(sc235hai->xvclk);

	return ret;
}

static void __sc235hai_power_off(struct sc235hai *sc235hai)
{
	int ret;
	struct device *dev = &sc235hai->client->dev;

	clk_disable_unprepare(sc235hai->xvclk);
	if (sc235hai->is_thunderboot) {
		if (sc235hai->is_first_streamoff) {
			sc235hai->is_thunderboot = false;
			sc235hai->is_first_streamoff = false;
		} else {
			return;
		}
	}
	if (!IS_ERR(sc235hai->pwdn_gpio))
		gpiod_set_value_cansleep(sc235hai->pwdn_gpio, 0);
	if (!IS_ERR(sc235hai->reset_gpio))
		gpiod_set_value_cansleep(sc235hai->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(sc235hai->pins_sleep)) {
		ret = pinctrl_select_state(sc235hai->pinctrl,
					   sc235hai->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(SC235HAI_NUM_SUPPLIES, sc235hai->supplies);
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int sc235hai_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc235hai *sc235hai = to_sc235hai(sd);

	if (sc235hai->standby_hw) {
		dev_info(dev, "resume standby!");
		return 0;
	}
	cam_sw_prepare_wakeup(sc235hai->cam_sw_info, dev);

	usleep_range(4000, 5000);
	cam_sw_write_array(sc235hai->cam_sw_info);

	if (__v4l2_ctrl_handler_setup(&sc235hai->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (sc235hai->has_init_exp && sc235hai->cur_mode != NO_HDR) {	// hdr mode
		ret = sc235hai_ioctl(&sc235hai->subdev, PREISP_CMD_SET_HDRAE_EXP,
				     &sc235hai->cam_sw_info->hdr_ae);
		if (ret) {
			dev_err(&sc235hai->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}
	return 0;
}

static int sc235hai_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc235hai *sc235hai = to_sc235hai(sd);

	if (sc235hai->standby_hw) {
		dev_info(dev, "suspend standby!");
		return 0;
	}

	cam_sw_write_array_cb_init(sc235hai->cam_sw_info, client,
				   (void *)sc235hai->cur_mode->reg_list,
				   (sensor_write_array)sc235hai_write_array);
	cam_sw_prepare_sleep(sc235hai->cam_sw_info);

	return 0;
}
#else
#define sc235hai_resume NULL
#define sc235hai_suspend NULL
#endif

static int sc235hai_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc235hai *sc235hai = to_sc235hai(sd);

	return __sc235hai_power_on(sc235hai);
}

static int sc235hai_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc235hai *sc235hai = to_sc235hai(sd);

	__sc235hai_power_off(sc235hai);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc235hai_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc235hai *sc235hai = to_sc235hai(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct sc235hai_mode *def_mode = &supported_modes[0];

	mutex_lock(&sc235hai->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc235hai->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc235hai_enum_frame_interval(struct v4l2_subdev *sd,
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

static const struct dev_pm_ops sc235hai_pm_ops = {
	SET_RUNTIME_PM_OPS(sc235hai_runtime_suspend, sc235hai_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sc235hai_suspend, sc235hai_resume)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc235hai_internal_ops = {
	.open = sc235hai_open,
};
#endif

static const struct v4l2_subdev_core_ops sc235hai_core_ops = {
	.s_power = sc235hai_s_power,
	.ioctl = sc235hai_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc235hai_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc235hai_video_ops = {
	.s_stream = sc235hai_s_stream,
	.g_frame_interval = sc235hai_g_frame_interval,
	.s_frame_interval = sc235hai_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops sc235hai_pad_ops = {
	.enum_mbus_code = sc235hai_enum_mbus_code,
	.enum_frame_size = sc235hai_enum_frame_sizes,
	.enum_frame_interval = sc235hai_enum_frame_interval,
	.get_fmt = sc235hai_get_fmt,
	.set_fmt = sc235hai_set_fmt,
	.get_mbus_config = sc235hai_g_mbus_config,
};

static const struct v4l2_subdev_ops sc235hai_subdev_ops = {
	.core	= &sc235hai_core_ops,
	.video	= &sc235hai_video_ops,
	.pad	= &sc235hai_pad_ops,
};

static void sc235hai_modify_fps_info(struct sc235hai *sc235hai)
{
	const struct sc235hai_mode *mode = sc235hai->cur_mode;

	sc235hai->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def / // *NOPAD*
					sc235hai->cur_vts;
}

static int sc235hai_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc235hai *sc235hai = container_of(ctrl->handler,
				    struct sc235hai, ctrl_handler);
	struct i2c_client *client = sc235hai->client;
	s64 max;
	int ret = 0;
	u32 val = 0;
	s32 temp = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc235hai->cur_mode->height + ctrl->val - 5;
		__v4l2_ctrl_modify_range(sc235hai->exposure,
					 sc235hai->exposure->minimum, max,
					 sc235hai->exposure->step,
					 sc235hai->exposure->default_value);
		break;
	}

	if (sc235hai->standby_hw && sc235hai->is_standby) {
		dev_dbg(&client->dev, "%s: is_standby = true, will return\n", __func__);
		return 0;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure value 0x%x\n", ctrl->val);
		if (sc235hai->cur_mode->hdr_mode == NO_HDR) {
			temp = ctrl->val * 2;
			/* 4 least significant bits of expsoure are fractional part */
			ret = sc235hai_write_reg(sc235hai->client,
						 SC235HAI_REG_EXPOSURE_H,
						 SC235HAI_REG_VALUE_08BIT,
						 SC235HAI_FETCH_EXP_H(temp));
			ret |= sc235hai_write_reg(sc235hai->client,
						  SC235HAI_REG_EXPOSURE_M,
						  SC235HAI_REG_VALUE_08BIT,
						  SC235HAI_FETCH_EXP_M(temp));
			ret |= sc235hai_write_reg(sc235hai->client,
						  SC235HAI_REG_EXPOSURE_L,
						  SC235HAI_REG_VALUE_08BIT,
						  SC235HAI_FETCH_EXP_L(temp));
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (sc235hai->cur_mode->hdr_mode == NO_HDR)
			ret = sc235hai_set_gain_reg(sc235hai, ctrl->val, SC235HAI_LGAIN);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set blank value 0x%x\n", ctrl->val);
		ret = sc235hai_write_reg(sc235hai->client,
					 SC235HAI_REG_VTS_H,
					 SC235HAI_REG_VALUE_08BIT,
					 (ctrl->val + sc235hai->cur_mode->height)
					 >> 8);
		ret |= sc235hai_write_reg(sc235hai->client,
					  SC235HAI_REG_VTS_L,
					  SC235HAI_REG_VALUE_08BIT,
					  (ctrl->val + sc235hai->cur_mode->height)
					  & 0xff);
		if (!ret)
			sc235hai->cur_vts = ctrl->val + sc235hai->cur_mode->height;
		sc235hai_modify_fps_info(sc235hai);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc235hai_enable_test_pattern(sc235hai, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc235hai_read_reg(sc235hai->client, SC235HAI_FLIP_MIRROR_REG,
					SC235HAI_REG_VALUE_08BIT, &val);
		ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_FLIP_MIRROR_REG,
					  SC235HAI_REG_VALUE_08BIT,
					  SC235HAI_FETCH_MIRROR(val, ctrl->val));
		break;
	case V4L2_CID_VFLIP:
		ret = sc235hai_read_reg(sc235hai->client, SC235HAI_FLIP_MIRROR_REG,
					SC235HAI_REG_VALUE_08BIT, &val);
		ret |= sc235hai_write_reg(sc235hai->client, SC235HAI_FLIP_MIRROR_REG,
					  SC235HAI_REG_VALUE_08BIT,
					  SC235HAI_FETCH_FLIP(val, ctrl->val));
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sc235hai_ctrl_ops = {
	.s_ctrl = sc235hai_set_ctrl,
};

static int sc235hai_initialize_controls(struct sc235hai *sc235hai)
{
	const struct sc235hai_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u64 dst_pixel_rate = 0;
	u32 h_blank;
	int ret;

	handler = &sc235hai->ctrl_handler;
	mode = sc235hai->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc235hai->mutex;

	sc235hai->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			      V4L2_CID_LINK_FREQ,
			      ARRAY_SIZE(link_freq_menu_items) - 1, 0,
			      link_freq_menu_items);
	__v4l2_ctrl_s_ctrl(sc235hai->link_freq, mode->mipi_freq_idx);

	if (mode->mipi_freq_idx == 0)
		dst_pixel_rate = PIXEL_RATE_WITH_371M_10BIT;
	else if (mode->mipi_freq_idx == 1)
		dst_pixel_rate = PIXEL_RATE_WITH_371M_10BIT;

	sc235hai->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
			       V4L2_CID_PIXEL_RATE, 0,
			       PIXEL_RATE_WITH_371M_10BIT,
			       1, dst_pixel_rate);

	h_blank = mode->hts_def - mode->width;
	sc235hai->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					     h_blank, h_blank, 1, h_blank);
	if (sc235hai->hblank)
		sc235hai->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	sc235hai->vblank = v4l2_ctrl_new_std(handler, &sc235hai_ctrl_ops,
					     V4L2_CID_VBLANK, vblank_def,
					     SC235HAI_VTS_MAX - mode->height,
					     1, vblank_def);
	exposure_max = mode->vts_def - 5;
	// exposure_max = 2 * mode->vts_def - 8;
	sc235hai->exposure = v4l2_ctrl_new_std(handler, &sc235hai_ctrl_ops,
					       V4L2_CID_EXPOSURE, SC235HAI_EXPOSURE_MIN,
					       exposure_max, SC235HAI_EXPOSURE_STEP,
					       mode->exp_def);
	sc235hai->anal_gain = v4l2_ctrl_new_std(handler, &sc235hai_ctrl_ops,
						V4L2_CID_ANALOGUE_GAIN, SC235HAI_GAIN_MIN,
						SC235HAI_GAIN_MAX, SC235HAI_GAIN_STEP,
						SC235HAI_GAIN_DEFAULT);
	sc235hai->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				 &sc235hai_ctrl_ops,
				 V4L2_CID_TEST_PATTERN,
				 ARRAY_SIZE(sc235hai_test_pattern_menu) - 1,
				 0, 0, sc235hai_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &sc235hai_ctrl_ops,
			  V4L2_CID_HFLIP, 0, 1, 1, 0);

	v4l2_ctrl_new_std(handler, &sc235hai_ctrl_ops,
			  V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&sc235hai->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc235hai->subdev.ctrl_handler = handler;
	sc235hai->has_init_exp = false;
	sc235hai->is_standby = false;
	sc235hai->cur_fps = mode->max_fps;
	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc235hai_check_sensor_id(struct sc235hai *sc235hai,
				    struct i2c_client *client)
{
	struct device *dev = &sc235hai->client->dev;
	u32 id = 0;
	int ret;

	if (sc235hai->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}
	ret = sc235hai_read_reg(client, SC235HAI_REG_CHIP_ID,
				SC235HAI_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected SC235HAI (%04x) sensor\n", CHIP_ID);

	return 0;
}

static int sc235hai_configure_regulators(struct sc235hai *sc235hai)
{
	unsigned int i;

	for (i = 0; i < SC235HAI_NUM_SUPPLIES; i++)
		sc235hai->supplies[i].supply = sc235hai_supply_names[i];

	return devm_regulator_bulk_get(&sc235hai->client->dev,
				       SC235HAI_NUM_SUPPLIES,
				       sc235hai->supplies);
}

static int sc235hai_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sc235hai *sc235hai;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;
	const char *sync_mode_name = NULL;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	sc235hai = devm_kzalloc(dev, sizeof(*sc235hai), GFP_KERNEL);
	if (!sc235hai)
		return -ENOMEM;

	of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc235hai->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc235hai->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc235hai->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc235hai->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}
	/* Compatible with non-standby mode if this attribute is not configured in dts*/
	of_property_read_u32(node, RKMODULE_CAMERA_STANDBY_HW,
			     &sc235hai->standby_hw);
	dev_info(dev, "sc235hai->standby_hw = %d\n", sc235hai->standby_hw);

	ret = of_property_read_string(node, RKMODULE_CAMERA_SYNC_MODE,
				      &sync_mode_name);
	if (ret) {
		sc235hai->sync_mode = NO_SYNC_MODE;
		dev_err(dev, "could not get sync mode!\n");
	} else {
		if (strcmp(sync_mode_name, RKMODULE_EXTERNAL_MASTER_MODE) == 0) {
			sc235hai->sync_mode = EXTERNAL_MASTER_MODE;
			dev_info(dev, "external master mode\n");
		} else if (strcmp(sync_mode_name, RKMODULE_INTERNAL_MASTER_MODE) == 0) {
			sc235hai->sync_mode = INTERNAL_MASTER_MODE;
			dev_info(dev, "internal master mode\n");
		} else if (strcmp(sync_mode_name, RKMODULE_SLAVE_MODE) == 0) {
			sc235hai->sync_mode = SLAVE_MODE;
			dev_info(dev, "slave mode\n");
		} else if (strcmp(sync_mode_name, RKMODULE_SOFT_SYNC_MODE) == 0) {
			sc235hai->sync_mode = SOFT_SYNC_MODE;
			dev_info(dev, "sync_mode = [SOFT_SYNC_MODE]\n");
		} else {
			dev_info(dev, "sync_mode = [NO_SYNC_MODE]\n");
		}
	}

	sc235hai->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);
	sc235hai->client = client;
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			sc235hai->cur_mode = &supported_modes[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(supported_modes))
		sc235hai->cur_mode = &supported_modes[0];

	sc235hai->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc235hai->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	sc235hai->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(sc235hai->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	sc235hai->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_ASIS);
	if (IS_ERR(sc235hai->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	sc235hai->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc235hai->pinctrl)) {
		sc235hai->pins_default =
			pinctrl_lookup_state(sc235hai->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc235hai->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		sc235hai->pins_sleep =
			pinctrl_lookup_state(sc235hai->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc235hai->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = sc235hai_configure_regulators(sc235hai);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc235hai->mutex);

	sd = &sc235hai->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc235hai_subdev_ops);
	ret = sc235hai_initialize_controls(sc235hai);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc235hai_power_on(sc235hai);
	if (ret)
		goto err_free_handler;

	ret = sc235hai_check_sensor_id(sc235hai, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc235hai_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc235hai->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc235hai->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	if (!sc235hai->cam_sw_info) {
		sc235hai->cam_sw_info = cam_sw_init();
		cam_sw_clk_init(sc235hai->cam_sw_info, sc235hai->xvclk, SC235HAI_XVCLK_FREQ);
		cam_sw_reset_pin_init(sc235hai->cam_sw_info, sc235hai->reset_gpio, 0);
		cam_sw_pwdn_pin_init(sc235hai->cam_sw_info, sc235hai->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc235hai->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc235hai->module_index, facing,
		 SC235HAI_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (sc235hai->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__sc235hai_power_off(sc235hai);
err_free_handler:
	v4l2_ctrl_handler_free(&sc235hai->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc235hai->mutex);

	return ret;
}

static void sc235hai_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc235hai *sc235hai = to_sc235hai(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc235hai->ctrl_handler);
	mutex_destroy(&sc235hai->mutex);

	cam_sw_deinit(sc235hai->cam_sw_info);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc235hai_power_off(sc235hai);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc235hai_of_match[] = {
	{ .compatible = "smartsens,sc235hai" },
	{},
};
MODULE_DEVICE_TABLE(of, sc235hai_of_match);
#endif

static const struct i2c_device_id sc235hai_match_id[] = {
	{ "smartsens,sc235hai", 0 },
	{ },
};

static struct i2c_driver sc235hai_i2c_driver = {
	.driver = {
		.name = SC235HAI_NAME,
		.pm = &sc235hai_pm_ops,
		.of_match_table = of_match_ptr(sc235hai_of_match),
	},
	.probe		= sc235hai_probe,
	.remove		= sc235hai_remove,
	.id_table	= sc235hai_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc235hai_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc235hai_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens sc235hai sensor driver");
MODULE_LICENSE("GPL");
