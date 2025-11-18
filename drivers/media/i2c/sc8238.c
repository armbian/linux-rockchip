// SPDX-License-Identifier: GPL-2.0
/*
 * sc8238 driver
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version
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
#include <media/v4l2-fwnode.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/consumer.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"
#include "cam-tb-setup.h"
#include "cam-sleep-wakeup.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC8238_BITS_PER_SAMPLE		10
#define SC8238_LINK_FREQ_360		360000000
#define SC8238_MAX_LINK_FREQ		SC8238_LINK_FREQ_360

#define PIXEL_RATE_WITH_360M_10BIT_4L	(SC8238_LINK_FREQ_360 * 2 / 10 * 4)

#define SC8238_XVCLK_FREQ		27000000

#define SC8238_REG_CHIP_ID_H	0x3107
#define SC8238_REG_CHIP_ID_L	0x3108
#define CHIP_ID			0x8235

#define SC8238_REG_CTRL_MODE		0x0100
#define SC8238_MODE_SW_STANDBY		0x0
#define SC8238_MODE_STREAMING		BIT(0)

#define SC8238_REG_EXPOSURE_H		0x3e00
#define SC8238_REG_EXPOSURE_M		0x3e01
#define SC8238_REG_EXPOSURE_L		0x3e02
#define SC8238_REG_EXP_LONG_H		0x3e00
#define SC8238_REG_EXP_MID_H		0x3e04
#define SC8238_REG_EXP_MAX_MID_H	0x3e23

#define	SC8238_EXPOSURE_MIN			3
#define	SC8238_EXPOSURE_STEP		1
#define SC8238_VTS_MAX			0x7fff

#define SC8238_REG_COARSE_AGAIN_L	0x3e08
#define SC8238_REG_FINE_AGAIN_L		0x3e09
#define SC8238_REG_COARSE_AGAIN_S	0x3e12
#define SC8238_REG_FINE_AGAIN_S		0x3e13
#define SC8238_REG_COARSE_DGAIN_L	0x3e06
#define SC8238_REG_FINE_DGAIN_L		0x3e07
#define SC8238_REG_COARSE_DGAIN_S	0x3e10
#define SC8238_REG_FINE_DGAIN_S		0x3e11

#define SC8238_GAIN_MIN		0x40
#define SC8238_GAIN_MAX		0x7D04
#define SC8238_GAIN_STEP		1
#define SC8238_GAIN_DEFAULT		0x40
#define SC8238_LGAIN			0
#define SC8238_SGAIN			1

#define SC8238_REG_TEST_PATTERN	0x4501
#define SC8238_TEST_PATTERN_BIT_MASK	BIT(3)

#define SC8238_REG_VTS_H		0x320e
#define SC8238_REG_VTS_L		0x320f

#define SC8238_FLIP_MIRROR_REG		0x3221

#define SC8238_FETCH_EXP_H(VAL)	(((VAL) >> 12) & 0xF)
#define SC8238_FETCH_EXP_M(VAL)	(((VAL) >> 4) & 0xFF)
#define SC8238_FETCH_EXP_L(VAL)	(((VAL) & 0xF) << 4)

#define SC8238_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL | 0x06 : VAL & 0xf9)
#define SC8238_FETCH_FLIP(VAL, ENABLE)		(ENABLE ? VAL | 0x60 : VAL & 0x9f)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define SC8238_REG_VALUE_08BIT		1
#define SC8238_REG_VALUE_16BIT		2
#define SC8238_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define SC8238_NAME			"sc8238"

static const char *const sc8238_supply_names[] = {
	"avdd",			/* Analog power */
	"dovdd",		/* Digital I/O power */
	"dvdd",			/* Digital core power */
};

#define SC8238_NUM_SUPPLIES ARRAY_SIZE(sc8238_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct sc8238_mode {
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

struct sc8238 {
	struct i2c_client *client;
	struct clk *xvclk;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	struct regulator_bulk_data supplies[SC8238_NUM_SUPPLIES];

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_sleep;

	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *anal_gain;
	struct v4l2_ctrl *digi_gain;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *test_pattern;
	struct mutex mutex;
	struct v4l2_fract cur_fps;
	bool streaming;
	bool power_on;
	const struct sc8238_mode *supported_modes;
	const struct sc8238_mode *cur_mode;
	u32 cfg_num;
	u32 module_index;
	const char *module_facing;
	const char *module_name;
	const char *len_name;
	u32 standby_hw;
	u32 cur_vts;
	bool has_init_exp;
	bool is_thunderboot;
	bool is_first_streamoff;
	bool is_standby;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	struct cam_sw_info *cam_sw_inf;
	struct v4l2_fwnode_endpoint bus_cfg;
	enum rkmodule_sync_mode sync_mode;
};

#define to_sc8238(sd) container_of(sd, struct sc8238, subdev)

static const struct regval sc8238_global_4lane_regs[] = {
	{ REG_NULL, 0x00 },
};

/*
 * window size=3840*2160 mipi@4lane
 * mclk=27M mipi_clk=708.75Mbps
 * pixel_line_total=xxxx line_frame_total=2256
 * row_time=29.62us frame_rate=30fps
 */
static const struct regval sc8238_3840_2160_liner_30fps_settings[] = {
	{ 0x0103, 0x01 },
	{ 0x0100, 0x00 },
	{ 0x36e9, 0x80 },
	{ 0x36f9, 0x80 },
	{ 0x3018, 0x72 },
	{ 0x3019, 0x00 },
	{ 0x301f, 0x02 },
	{ 0x3031, 0x0a },
	{ 0x3037, 0x20 },
	{ 0x3038, 0x44 },
	{ 0x3203, 0x08 },
	{ 0x3207, 0x87 },
	{ 0x320c, 0x08 },
	{ 0x320d, 0x34 },
	{ 0x3213, 0x08 },
	{ 0x3241, 0x00 },
	{ 0x3243, 0x03 },
	{ 0x3248, 0x04 },
	{ 0x3271, 0x1c },
	{ 0x3273, 0x1f },
	{ 0x3301, 0x1c },
	{ 0x3306, 0xa8 },
	{ 0x3308, 0x20 },
	{ 0x3309, 0x68 },
	{ 0x330b, 0x48 },
	{ 0x330d, 0x28 },
	{ 0x330e, 0x58 },
	{ 0x3314, 0x94 },
	{ 0x331f, 0x59 },
	{ 0x3332, 0x24 },
	{ 0x334c, 0x10 },
	{ 0x3350, 0x24 },
	{ 0x3358, 0x24 },
	{ 0x335c, 0x24 },
	{ 0x335d, 0x60 },
	{ 0x3364, 0x16 },
	{ 0x3366, 0x92 },
	{ 0x3367, 0x08 },
	{ 0x3368, 0x07 },
	{ 0x3369, 0x00 },
	{ 0x336a, 0x00 },
	{ 0x336b, 0x00 },
	{ 0x336c, 0xc2 },
	{ 0x337f, 0x33 },
	{ 0x3390, 0x08 },
	{ 0x3391, 0x18 },
	{ 0x3392, 0x38 },
	{ 0x3393, 0x1c },
	{ 0x3394, 0x28 },
	{ 0x3395, 0x60 },
	{ 0x3396, 0x08 },
	{ 0x3397, 0x18 },
	{ 0x3398, 0x38 },
	{ 0x3399, 0x1c },
	{ 0x339a, 0x1c },
	{ 0x339b, 0x28 },
	{ 0x339c, 0x60 },
	{ 0x339e, 0x24 },
	{ 0x33aa, 0x24 },
	{ 0x33af, 0x48 },
	{ 0x33e1, 0x08 },
	{ 0x33e2, 0x18 },
	{ 0x33e3, 0x10 },
	{ 0x33e4, 0x0c },
	{ 0x33e5, 0x10 },
	{ 0x33e6, 0x06 },
	{ 0x33e7, 0x02 },
	{ 0x33e8, 0x18 },
	{ 0x33e9, 0x10 },
	{ 0x33ea, 0x0c },
	{ 0x33eb, 0x10 },
	{ 0x33ec, 0x04 },
	{ 0x33ed, 0x02 },
	{ 0x33ee, 0xa0 },
	{ 0x33ef, 0x08 },
	{ 0x33f4, 0x18 },
	{ 0x33f5, 0x10 },
	{ 0x33f6, 0x0c },
	{ 0x33f7, 0x10 },
	{ 0x33f8, 0x06 },
	{ 0x33f9, 0x02 },
	{ 0x33fa, 0x18 },
	{ 0x33fb, 0x10 },
	{ 0x33fc, 0x0c },
	{ 0x33fd, 0x10 },
	{ 0x33fe, 0x04 },
	{ 0x33ff, 0x02 },
	{ 0x360f, 0x01 },
	{ 0x3622, 0xf7 },
	{ 0x3624, 0x45 },
	{ 0x3628, 0x83 },
	{ 0x3630, 0x80 },
	{ 0x3631, 0x80 },
	{ 0x3632, 0xa8 },
	{ 0x3633, 0x53 },
	{ 0x3635, 0x02 },
	{ 0x3637, 0x52 },
	{ 0x3638, 0x0a },
	{ 0x363a, 0x88 },
	{ 0x363b, 0x06 },
	{ 0x363d, 0x01 },
	{ 0x363e, 0x00 },
	{ 0x3641, 0x00 },
	{ 0x3670, 0x4a },
	{ 0x3671, 0xf7 },
	{ 0x3672, 0xf7 },
	{ 0x3673, 0x17 },
	{ 0x3674, 0x80 },
	{ 0x3675, 0x85 },
	{ 0x3676, 0xa5 },
	{ 0x367a, 0x48 },
	{ 0x367b, 0x78 },
	{ 0x367c, 0x48 },
	{ 0x367d, 0x78 },
	{ 0x3690, 0x53 },
	{ 0x3691, 0x63 },
	{ 0x3692, 0x54 },
	{ 0x3699, 0x88 },
	{ 0x369a, 0x9f },
	{ 0x369b, 0x9f },
	{ 0x369c, 0x48 },
	{ 0x369d, 0x78 },
	{ 0x36a2, 0x48 },
	{ 0x36a3, 0x78 },
	{ 0x36bb, 0x48 },
	{ 0x36bc, 0x78 },
	{ 0x36c9, 0x05 },
	{ 0x36ca, 0x05 },
	{ 0x36cb, 0x05 },
	{ 0x36cc, 0x00 },
	{ 0x36cd, 0x10 },
	{ 0x36ce, 0x1a },
	{ 0x36d0, 0x30 },
	{ 0x36d1, 0x48 },
	{ 0x36d2, 0x78 },
	{ 0x36ea, 0x39 },
	{ 0x36eb, 0x06 },
	{ 0x36ec, 0x05 },
	{ 0x36ed, 0x24 },
	{ 0x36fa, 0x39 },
	{ 0x36fb, 0x13 },
	{ 0x36fc, 0x10 },
	{ 0x36fd, 0x14 },
	{ 0x3901, 0x00 },
	{ 0x3902, 0xc5 },
	{ 0x3904, 0x18 },
	{ 0x3905, 0xd8 },
	{ 0x394c, 0x0f },
	{ 0x394d, 0x20 },
	{ 0x394e, 0x08 },
	{ 0x394f, 0x90 },
	{ 0x3980, 0x71 },
	{ 0x3981, 0x70 },
	{ 0x3982, 0x00 },
	{ 0x3983, 0x00 },
	{ 0x3984, 0x20 },
	{ 0x3987, 0x0b },
	{ 0x3990, 0x03 },
	{ 0x3991, 0xfd },
	{ 0x3992, 0x03 },
	{ 0x3993, 0xfc },
	{ 0x3994, 0x00 },
	{ 0x3995, 0x00 },
	{ 0x3996, 0x00 },
	{ 0x3997, 0x05 },
	{ 0x3998, 0x00 },
	{ 0x3999, 0x09 },
	{ 0x399a, 0x00 },
	{ 0x399b, 0x12 },
	{ 0x399c, 0x00 },
	{ 0x399d, 0x12 },
	{ 0x399e, 0x00 },
	{ 0x399f, 0x18 },
	{ 0x39a0, 0x00 },
	{ 0x39a1, 0x14 },
	{ 0x39a2, 0x03 },
	{ 0x39a3, 0xe3 },
	{ 0x39a4, 0x03 },
	{ 0x39a5, 0xf2 },
	{ 0x39a6, 0x03 },
	{ 0x39a7, 0xf6 },
	{ 0x39a8, 0x03 },
	{ 0x39a9, 0xfa },
	{ 0x39aa, 0x03 },
	{ 0x39ab, 0xff },
	{ 0x39ac, 0x00 },
	{ 0x39ad, 0x06 },
	{ 0x39ae, 0x00 },
	{ 0x39af, 0x09 },
	{ 0x39b0, 0x00 },
	{ 0x39b1, 0x12 },
	{ 0x39b2, 0x00 },
	{ 0x39b3, 0x22 },
	{ 0x39b4, 0x0c },
	{ 0x39b5, 0x1c },
	{ 0x39b6, 0x38 },
	{ 0x39b7, 0x5b },
	{ 0x39b8, 0x50 },
	{ 0x39b9, 0x38 },
	{ 0x39ba, 0x20 },
	{ 0x39bb, 0x10 },
	{ 0x39bc, 0x0c },
	{ 0x39bd, 0x16 },
	{ 0x39be, 0x21 },
	{ 0x39bf, 0x36 },
	{ 0x39c0, 0x3b },
	{ 0x39c1, 0x2a },
	{ 0x39c2, 0x16 },
	{ 0x39c3, 0x0c },
	{ 0x39c5, 0x30 },
	{ 0x39c6, 0x07 },
	{ 0x39c7, 0xf8 },
	{ 0x39c9, 0x07 },
	{ 0x39ca, 0xf8 },
	{ 0x39cc, 0x00 },
	{ 0x39cd, 0x1b },
	{ 0x39ce, 0x00 },
	{ 0x39cf, 0x00 },
	{ 0x39d0, 0x1b },
	{ 0x39d1, 0x00 },
	{ 0x39e2, 0x15 },
	{ 0x39e3, 0x87 },
	{ 0x39e4, 0x12 },
	{ 0x39e5, 0xb7 },
	{ 0x39e6, 0x00 },
	{ 0x39e7, 0x8c },
	{ 0x39e8, 0x01 },
	{ 0x39e9, 0x31 },
	{ 0x39ea, 0x01 },
	{ 0x39eb, 0xd7 },
	{ 0x39ec, 0x08 },
	{ 0x39ed, 0x00 },
	{ 0x3e00, 0x01 },
	{ 0x3e01, 0x18 },
	{ 0x3e02, 0xa0 },
	{ 0x3e08, 0x03 },
	{ 0x3e09, 0x40 },
	{ 0x3e0e, 0x09 },
	{ 0x3e14, 0x31 },
	{ 0x3e16, 0x00 },
	{ 0x3e17, 0xac },
	{ 0x3e18, 0x00 },
	{ 0x3e19, 0xac },
	{ 0x3e1b, 0x3a },
	{ 0x3e1e, 0x76 },
	{ 0x3e25, 0x23 },
	{ 0x3e26, 0x40 },
	{ 0x4501, 0xa4 },
	{ 0x4509, 0x10 },
	{ 0x4837, 0x1c },
	{ 0x5799, 0x06 },
	{ 0x57aa, 0x2f },
	{ 0x57ab, 0xff },
	{ 0x36e9, 0x53 },
	{ 0x36f9, 0x57 },
	{ 0x0100, 0x01 },
	{ REG_NULL, 0x00 },
};

static const struct sc8238_mode supported_modes[] = {
	{
	 .width = 3840,
	 .height = 2160,
	 .max_fps = {
		     .numerator = 10000,
		     .denominator = 300000,
		     },
	 .exp_def = 0x8ca - 0xa,
	 .hts_def = 0x41a * 4 * 2,
	 .vts_def = 0x8ca,
	 .bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
	 .global_reg_list = sc8238_global_4lane_regs,
	 .reg_list = sc8238_3840_2160_liner_30fps_settings,
	 .hdr_mode = NO_HDR,
	 .mclk = 27000000,
	 .link_freq_idx = 0,
	 .bpp = 10,
	 .vc[PAD0] = 0,
	 .lanes = 4,
	  },
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,
};

static const s64 link_freq_menu_items[] = {
	SC8238_LINK_FREQ_360,
};

static const char *const sc8238_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4",
};

/* Write registers up to 4 at a time */
static int sc8238_write_reg(struct i2c_client *client, u16 reg,
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

static int sc8238_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = sc8238_write_reg(client, regs[i].addr,
				       SC8238_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int sc8238_read_reg(struct i2c_client *client, u16 reg,
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

static int sc8238_get_gain_reg(struct sc8238 *sc8238, u32 total_gain,
			       u32 *again_coarse_reg, u32 *again_fine_reg,
			       u32 *dgain_coarse_reg, u32 *dgain_fine_reg)
{
	u32 again, dgain;

	if (total_gain > 32004) {
		dev_err(&sc8238->client->dev,
			"total_gain max is 15.875*31.5*64, current total_gain is %d\n",
			total_gain);
		return -EINVAL;
	}

	if (total_gain > 1016) {	/*15.875 */
		again = 1016;
		dgain = total_gain * 128 / 1016;
	} else {
		again = total_gain;
		dgain = 128;
	}

	if (again < 0x80) {	/*1x ~ 2x */
		*again_fine_reg = again & 0x7f;
		*again_coarse_reg = 0x03;
	} else if (again < 0x100) {	/*2x ~ 4x */
		*again_fine_reg = (again >> 1) & 0x7f;
		*again_coarse_reg = 0x07;
	} else if (again < 0x200) {	/*4x ~ 8x */
		*again_fine_reg = (again >> 2) & 0x7f;
		*again_coarse_reg = 0x0f;
	} else {		/*8x ~ 16x */
		*again_fine_reg = (again >> 3) & 0x7f;
		*again_coarse_reg = 0x1f;
	}

	if (dgain < 0x100) {	/*1x ~ 2x */
		*dgain_fine_reg = dgain & 0xff;
		*dgain_coarse_reg = 0x00;
	} else if (dgain < 0x200) {	/*2x ~ 4x */
		*dgain_fine_reg = (dgain >> 1) & 0xff;
		*dgain_coarse_reg = 0x01;
	} else if (dgain < 0x400) {	/*4x ~ 8x */
		*dgain_fine_reg = (dgain >> 2) & 0xff;
		*dgain_coarse_reg = 0x03;
	} else if (dgain < 0x800) {	/*8x ~ 16x */
		*dgain_fine_reg = (dgain >> 3) & 0xff;
		*dgain_coarse_reg = 0x07;
	} else {		/*16x ~ 31.5x */
		*dgain_fine_reg = (dgain >> 4) & 0xff;
		*dgain_coarse_reg = 0x0f;
	}
	dev_dbg(&sc8238->client->dev,
		"total_gain 0x%x again_coarse 0x%x, again_fine 0x%x, dgain_coarse 0x%x, dgain_fine 0x%x\n",
		total_gain, *again_coarse_reg, *again_fine_reg,
		*dgain_coarse_reg, *dgain_fine_reg);
	return 0;
}

static int sc8238_get_reso_dist(const struct sc8238_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	    abs(mode->height - framefmt->height);
}

static const struct sc8238_mode *sc8238_find_best_fit(struct sc8238 *sc8238,
						      struct v4l2_subdev_format
						      *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < sc8238->cfg_num; i++) {
		dist =
		    sc8238_get_reso_dist(&sc8238->supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		} else if (dist == cur_best_fit_dist &&
			   framefmt->code ==
			   sc8238->supported_modes[i].bus_fmt) {
			cur_best_fit = i;
			break;
		}
	}

	return &sc8238->supported_modes[cur_best_fit];
}

static int sc8238_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct sc8238 *sc8238 = to_sc8238(sd);
	const struct sc8238_mode *mode;
	s64 h_blank, vblank_def;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc8238->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&sc8238->mutex);

	mode = sc8238_find_best_fit(sc8238, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) =
		    fmt->format;
#else
		mutex_unlock(&sc8238->mutex);
		return -ENOTTY;
#endif
	} else {
		sc8238->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc8238->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc8238->vblank, vblank_def,
					 SC8238_VTS_MAX - mode->height,
					 1, vblank_def);
		dst_link_freq = mode->link_freq_idx;
		dst_pixel_rate =
		    (u32) link_freq_menu_items[mode->link_freq_idx] /
		    mode->bpp * 2 * lanes;
		__v4l2_ctrl_s_ctrl_int64(sc8238->pixel_rate, dst_pixel_rate);
		__v4l2_ctrl_s_ctrl(sc8238->link_freq, dst_link_freq);
		sc8238->cur_fps = mode->max_fps;
	}

	mutex_unlock(&sc8238->mutex);

	return 0;
}

static int sc8238_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct sc8238 *sc8238 = to_sc8238(sd);
	const struct sc8238_mode *mode = sc8238->cur_mode;

	mutex_lock(&sc8238->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format =
		    *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&sc8238->mutex);
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
	mutex_unlock(&sc8238->mutex);

	return 0;
}

static int sc8238_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int sc8238_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct sc8238 *sc8238 = to_sc8238(sd);

	if (fse->index >= sc8238->cfg_num)
		return -EINVAL;

	if (fse->code != sc8238->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width = sc8238->supported_modes[fse->index].width;
	fse->max_width = sc8238->supported_modes[fse->index].width;
	fse->max_height = sc8238->supported_modes[fse->index].height;
	fse->min_height = sc8238->supported_modes[fse->index].height;

	return 0;
}

static int sc8238_enable_test_pattern(struct sc8238 *sc8238, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc8238_read_reg(sc8238->client, SC8238_REG_TEST_PATTERN,
			      SC8238_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC8238_TEST_PATTERN_BIT_MASK;
	else
		val &= ~SC8238_TEST_PATTERN_BIT_MASK;

	ret |= sc8238_write_reg(sc8238->client, SC8238_REG_TEST_PATTERN,
				SC8238_REG_VALUE_08BIT, val);
	return ret;
}

static int sc8238_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct sc8238 *sc8238 = to_sc8238(sd);
	const struct sc8238_mode *mode = sc8238->cur_mode;

	if (sc8238->streaming)
		fi->interval = sc8238->cur_fps;
	else
		fi->interval = mode->max_fps;
	return 0;
}

static const struct sc8238_mode *sc8238_find_mode(struct sc8238 *sc8238,
						  int fps)
{
	const struct sc8238_mode *mode = NULL;
	const struct sc8238_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < sc8238->cfg_num; i++) {
		mode = &sc8238->supported_modes[i];
		if (mode->width == sc8238->cur_mode->width &&
		    mode->height == sc8238->cur_mode->height &&
		    mode->hdr_mode == sc8238->cur_mode->hdr_mode &&
		    mode->bus_fmt == sc8238->cur_mode->bus_fmt) {
			cur_fps =
			    DIV_ROUND_CLOSEST(mode->max_fps.denominator,
					      mode->max_fps.numerator);
			if (cur_fps == fps) {
				match = mode;
				break;
			}
		}
	}
	return match;
}

static int sc8238_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct sc8238 *sc8238 = to_sc8238(sd);
	const struct sc8238_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	int fps;

	if (sc8238->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = sc8238_find_mode(sc8238, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	sc8238->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(sc8238->hblank, h_blank, h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(sc8238->vblank, vblank_def,
				 SC8238_VTS_MAX - mode->height, 1, vblank_def);
	pixel_rate = (u32) link_freq_menu_items[mode->link_freq_idx] /
	    mode->bpp * 2 * mode->lanes;

	__v4l2_ctrl_s_ctrl_int64(sc8238->pixel_rate, pixel_rate);
	__v4l2_ctrl_s_ctrl(sc8238->link_freq, mode->link_freq_idx);
	sc8238->cur_fps = mode->max_fps;

	return 0;
}

static int sc8238_g_mbus_config(struct v4l2_subdev *sd,
				unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct sc8238 *sc8238 = to_sc8238(sd);

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = sc8238->cur_mode->lanes;

	return 0;
}

static void sc8238_get_module_inf(struct sc8238 *sc8238,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC8238_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc8238->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc8238->len_name, sizeof(inf->base.lens));
}

static long sc8238_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc8238 *sc8238 = to_sc8238(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_hdr_cfg *hdr_cfg;
	long ret = 0;
	u32 i, h, w;
	u32 stream = 0;
	u32 *sync_mode = NULL;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc8238_get_module_inf(sc8238, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = sc8238->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		w = sc8238->cur_mode->width;
		h = sc8238->cur_mode->height;

		dev_info(&sc8238->client->dev,
			 "%s config hdr mode: %d\n",
			 __func__, hdr_cfg->hdr_mode);
		for (i = 0; i < sc8238->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr_cfg->hdr_mode) {
				sc8238->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == sc8238->cfg_num) {
			dev_err(&sc8238->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr_cfg->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = sc8238->cur_mode->hts_def - sc8238->cur_mode->width;
			h = sc8238->cur_mode->vts_def -
			    sc8238->cur_mode->height;
			__v4l2_ctrl_modify_range(sc8238->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(sc8238->vblank, h,
						 SC8238_VTS_MAX -
						 sc8238->cur_mode->height, 1,
						 h);
			sc8238->cur_fps = sc8238->cur_mode->max_fps;
			sc8238->cur_vts = sc8238->cur_mode->vts_def;
			dev_info(&sc8238->client->dev,
				 "sensor mode: %d\n",
				 sc8238->cur_mode->hdr_mode);
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *) arg);

		if (stream)
			ret =
			    sc8238_write_reg(sc8238->client,
					     SC8238_REG_CTRL_MODE,
					     SC8238_REG_VALUE_08BIT,
					     SC8238_MODE_STREAMING);
		else
			ret =
			    sc8238_write_reg(sc8238->client,
					     SC8238_REG_CTRL_MODE,
					     SC8238_REG_VALUE_08BIT,
					     SC8238_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_SYNC_MODE:
		sync_mode = (u32 *) arg;
		*sync_mode = sc8238->sync_mode;
		break;
	case RKMODULE_SET_SYNC_MODE:
		sync_mode = (u32 *) arg;
		sc8238->sync_mode = *sync_mode;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long sc8238_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
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

		ret = sc8238_ioctl(sd, cmd, inf);
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

		ret = sc8238_ioctl(sd, cmd, hdr);
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
			ret = sc8238_ioctl(sd, cmd, hdr);
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
			ret = sc8238_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = sc8238_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_SYNC_MODE:
		ret = sc8238_ioctl(sd, cmd, &sync_mode);
		if (!ret) {
			ret = copy_to_user(up, &sync_mode, sizeof(u32));
			if (ret)
				ret = -EFAULT;
		}
		break;
	case RKMODULE_SET_SYNC_MODE:
		ret = copy_from_user(&sync_mode, up, sizeof(u32));
		if (!ret)
			ret = sc8238_ioctl(sd, cmd, &sync_mode);
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

static int __sc8238_start_stream(struct sc8238 *sc8238)
{
	int ret = 0;

	if (!sc8238->is_thunderboot) {
		ret =
		    sc8238_write_array(sc8238->client,
				       sc8238->cur_mode->reg_list);
		if (ret)
			return ret;
		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&sc8238->ctrl_handler);
		if (ret)
			return ret;
		if (sc8238->has_init_exp
		    && sc8238->cur_mode->hdr_mode != NO_HDR) {
			ret =
			    sc8238_ioctl(&sc8238->subdev,
					 PREISP_CMD_SET_HDRAE_EXP,
					 &sc8238->init_hdrae_exp);
			if (ret) {
				dev_err(&sc8238->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	ret = sc8238_write_reg(sc8238->client, SC8238_REG_CTRL_MODE,
			       SC8238_REG_VALUE_08BIT, SC8238_MODE_STREAMING);
	return ret;
}

static int __sc8238_stop_stream(struct sc8238 *sc8238)
{
	sc8238->has_init_exp = false;
	if (sc8238->is_thunderboot)
		sc8238->is_first_streamoff = true;
	return sc8238_write_reg(sc8238->client, SC8238_REG_CTRL_MODE,
				SC8238_REG_VALUE_08BIT, SC8238_MODE_SW_STANDBY);
}

static int __sc8238_power_on(struct sc8238 *sc8238);
static int sc8238_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc8238 *sc8238 = to_sc8238(sd);
	struct i2c_client *client = sc8238->client;
	int ret = 0;

	mutex_lock(&sc8238->mutex);
	on = !!on;
	if (on == sc8238->streaming)
		goto unlock_and_return;
	if (on) {
		if (sc8238->is_thunderboot
		    && rkisp_tb_get_state() == RKISP_TB_NG) {
			sc8238->is_thunderboot = false;
			__sc8238_power_on(sc8238);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __sc8238_start_stream(sc8238);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc8238_stop_stream(sc8238);
		pm_runtime_put(&client->dev);
	}

	sc8238->streaming = on;
unlock_and_return:
	mutex_unlock(&sc8238->mutex);
	return ret;
}

static int sc8238_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc8238 *sc8238 = to_sc8238(sd);
	struct i2c_client *client = sc8238->client;
	int ret = 0;

	mutex_lock(&sc8238->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc8238->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!sc8238->is_thunderboot) {
			ret = sc8238_write_array(sc8238->client,
						 sc8238->cur_mode->global_reg_list);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		sc8238->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc8238->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc8238->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 sc8238_cal_delay(u32 cycles, struct sc8238 *sc8238)
{
	return DIV_ROUND_UP(cycles, sc8238->cur_mode->mclk / 1000 / 1000);
}

static int __sc8238_power_on(struct sc8238 *sc8238)
{
	int ret;
	struct device *dev = &sc8238->client->dev;

	if (!IS_ERR_OR_NULL(sc8238->pins_default)) {
		ret = pinctrl_select_state(sc8238->pinctrl,
					   sc8238->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc8238->xvclk, sc8238->cur_mode->mclk);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (%dHz)\n",
			 sc8238->cur_mode->mclk);
	if (clk_get_rate(sc8238->xvclk) != sc8238->cur_mode->mclk)
		dev_warn(dev, "xvclk mismatched, modes are based on %dHz\n",
			 sc8238->cur_mode->mclk);
	ret = clk_prepare_enable(sc8238->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	cam_sw_regulator_bulk_init(sc8238->cam_sw_inf, SC8238_NUM_SUPPLIES,
				   sc8238->supplies);

	if (sc8238->is_thunderboot)
		return 0;

	if (!IS_ERR(sc8238->reset_gpio))
		gpiod_set_value_cansleep(sc8238->reset_gpio, 0);

	ret = regulator_bulk_enable(SC8238_NUM_SUPPLIES, sc8238->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc8238->reset_gpio))
		gpiod_set_value_cansleep(sc8238->reset_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR(sc8238->pwdn_gpio))
		gpiod_set_value_cansleep(sc8238->pwdn_gpio, 1);

	if (!IS_ERR(sc8238->reset_gpio))
		gpiod_set_value_cansleep(sc8238->reset_gpio, 0);

	usleep_range(10000, 20000);

	return 0;

disable_clk:
	clk_disable_unprepare(sc8238->xvclk);

	return ret;
}

static void __sc8238_power_off(struct sc8238 *sc8238)
{
	int ret;
	struct device *dev = &sc8238->client->dev;

	clk_disable_unprepare(sc8238->xvclk);
	if (sc8238->is_thunderboot) {
		if (sc8238->is_first_streamoff) {
			sc8238->is_thunderboot = false;
			sc8238->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(sc8238->reset_gpio))
		gpiod_set_value_cansleep(sc8238->reset_gpio, 1);
	if (!IS_ERR(sc8238->pwdn_gpio))
		gpiod_set_value_cansleep(sc8238->pwdn_gpio, 0);
	if (!IS_ERR_OR_NULL(sc8238->pins_sleep)) {
		ret = pinctrl_select_state(sc8238->pinctrl, sc8238->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(SC8238_NUM_SUPPLIES, sc8238->supplies);
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int __maybe_unused sc8238_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc8238 *sc8238 = to_sc8238(sd);

	if (sc8238->standby_hw) {
		dev_info(dev, "resume standby!");
		return 0;
	}

	cam_sw_prepare_wakeup(sc8238->cam_sw_inf, dev);

	usleep_range(4000, 5000);
	cam_sw_write_array(sc8238->cam_sw_inf);

	if (__v4l2_ctrl_handler_setup(&sc8238->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (sc8238->has_init_exp && sc8238->cur_mode != NO_HDR) {
		ret = sc8238_ioctl(&sc8238->subdev, PREISP_CMD_SET_HDRAE_EXP,
				   &sc8238->cam_sw_inf->hdr_ae);
		if (ret) {
			dev_err(&sc8238->client->dev,
				"set exp fail in hdr mode\n");
			return ret;
		}
	}

	return 0;
}

static int __maybe_unused sc8238_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc8238 *sc8238 = to_sc8238(sd);

	if (sc8238->standby_hw) {
		dev_info(dev, "suspend standby!");
		return 0;
	}

	cam_sw_write_array_cb_init(sc8238->cam_sw_inf, client,
				   (void *)sc8238->cur_mode->reg_list,
				   (sensor_write_array) sc8238_write_array);
	cam_sw_prepare_sleep(sc8238->cam_sw_inf);

	return 0;
}
#else
#define sc8238_resume NULL
#define sc8238_suspend NULL
#endif

static int __maybe_unused sc8238_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc8238 *sc8238 = to_sc8238(sd);

	return __sc8238_power_on(sc8238);
}

static int __maybe_unused sc8238_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc8238 *sc8238 = to_sc8238(sd);

	__sc8238_power_off(sc8238);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc8238_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc8238 *sc8238 = to_sc8238(sd);
	struct v4l2_mbus_framefmt *try_fmt =
	    v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct sc8238_mode *def_mode = &sc8238->supported_modes[0];

	mutex_lock(&sc8238->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc8238->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc8238_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum
				      *fie)
{
	struct sc8238 *sc8238 = to_sc8238(sd);

	if (fie->index >= sc8238->cfg_num)
		return -EINVAL;

	fie->code = sc8238->supported_modes[fie->index].bus_fmt;
	fie->width = sc8238->supported_modes[fie->index].width;
	fie->height = sc8238->supported_modes[fie->index].height;
	fie->interval = sc8238->supported_modes[fie->index].max_fps;
	fie->reserved[0] = sc8238->supported_modes[fie->index].hdr_mode;

	return 0;
}

static const struct dev_pm_ops sc8238_pm_ops = {
	SET_RUNTIME_PM_OPS(sc8238_runtime_suspend,
			   sc8238_runtime_resume, NULL)
#ifdef CONFIG_VIDEO_CAM_SLEEP_WAKEUP
	    SET_LATE_SYSTEM_SLEEP_PM_OPS(sc8238_suspend, sc8238_resume)
#endif
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc8238_internal_ops = {
	.open = sc8238_open,
};
#endif

static const struct v4l2_subdev_core_ops sc8238_core_ops = {
	.s_power = sc8238_s_power,
	.ioctl = sc8238_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc8238_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc8238_video_ops = {
	.s_stream = sc8238_s_stream,
	.g_frame_interval = sc8238_g_frame_interval,
	.s_frame_interval = sc8238_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops sc8238_pad_ops = {
	.enum_mbus_code = sc8238_enum_mbus_code,
	.enum_frame_size = sc8238_enum_frame_sizes,
	.enum_frame_interval = sc8238_enum_frame_interval,
	.get_fmt = sc8238_get_fmt,
	.set_fmt = sc8238_set_fmt,
	.get_mbus_config = sc8238_g_mbus_config,
};

static const struct v4l2_subdev_ops sc8238_subdev_ops = {
	.core = &sc8238_core_ops,
	.video = &sc8238_video_ops,
	.pad = &sc8238_pad_ops,
};

static void sc8238_modify_fps_info(struct sc8238 *sc8238)
{
	const struct sc8238_mode *mode = sc8238->cur_mode;

	sc8238->cur_fps.denominator =
	    mode->max_fps.denominator * mode->vts_def / sc8238->cur_vts;
}

static int sc8238_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc8238 *sc8238 = container_of(ctrl->handler,
					     struct sc8238, ctrl_handler);
	struct i2c_client *client = sc8238->client;
	s64 max;
	int ret = 0;
	u32 val = 0;
	u32 again_coarse_reg = 0;
	u32 again_fine_reg = 0;
	u32 dgain_coarse_reg = 0;
	u32 dgain_fine_reg = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc8238->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(sc8238->exposure,
					 sc8238->exposure->minimum, max,
					 sc8238->exposure->step,
					 sc8238->exposure->default_value);
		break;
	}

	if (sc8238->standby_hw && sc8238->is_standby) {
		dev_dbg(&client->dev, "%s: is_standby = true, will return\n",
			__func__);
		return 0;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		if (sc8238->cur_mode->hdr_mode == NO_HDR) {
			val = ctrl->val << 1;
			/* 4 least significant bits of expsoure are fractional part */
			ret = sc8238_write_reg(sc8238->client,
					       SC8238_REG_EXPOSURE_H,
					       SC8238_REG_VALUE_08BIT,
					       SC8238_FETCH_EXP_H(val));
			ret |= sc8238_write_reg(sc8238->client,
						SC8238_REG_EXPOSURE_M,
						SC8238_REG_VALUE_08BIT,
						SC8238_FETCH_EXP_M(val));
			ret |= sc8238_write_reg(sc8238->client,
						SC8238_REG_EXPOSURE_L,
						SC8238_REG_VALUE_08BIT,
						SC8238_FETCH_EXP_L(val));
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = sc8238_get_gain_reg(sc8238, ctrl->val,
					  &again_coarse_reg, &again_fine_reg,
					  &dgain_coarse_reg, &dgain_fine_reg);
		ret |= sc8238_write_reg(sc8238->client,
					SC8238_REG_COARSE_AGAIN_L,
					SC8238_REG_VALUE_08BIT,
					again_coarse_reg);
		ret |= sc8238_write_reg(sc8238->client,
					SC8238_REG_FINE_AGAIN_L,
					SC8238_REG_VALUE_08BIT, again_fine_reg);
		ret |= sc8238_write_reg(sc8238->client,
					SC8238_REG_COARSE_DGAIN_L,
					SC8238_REG_VALUE_08BIT,
					dgain_coarse_reg);
		ret |= sc8238_write_reg(sc8238->client,
					SC8238_REG_FINE_DGAIN_L,
					SC8238_REG_VALUE_08BIT, dgain_fine_reg);
		dev_dbg(&client->dev, "set analog gain 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		ret = sc8238_write_reg(sc8238->client,
				       SC8238_REG_VTS_H,
				       SC8238_REG_VALUE_08BIT,
				       (ctrl->val + sc8238->cur_mode->height)
				       >> 8);
		ret |= sc8238_write_reg(sc8238->client,
					SC8238_REG_VTS_L,
					SC8238_REG_VALUE_08BIT,
					(ctrl->val + sc8238->cur_mode->height)
					& 0xff);
		sc8238->cur_vts = ctrl->val + sc8238->cur_mode->height;
		if (sc8238->cur_vts != sc8238->cur_mode->vts_def)
			sc8238_modify_fps_info(sc8238);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc8238_enable_test_pattern(sc8238, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc8238_read_reg(sc8238->client, SC8238_FLIP_MIRROR_REG,
				      SC8238_REG_VALUE_08BIT, &val);
		ret |= sc8238_write_reg(sc8238->client, SC8238_FLIP_MIRROR_REG,
					SC8238_REG_VALUE_08BIT,
					SC8238_FETCH_MIRROR(val, ctrl->val));
		break;
	case V4L2_CID_VFLIP:
		ret = sc8238_read_reg(sc8238->client, SC8238_FLIP_MIRROR_REG,
				      SC8238_REG_VALUE_08BIT, &val);
		ret |= sc8238_write_reg(sc8238->client, SC8238_FLIP_MIRROR_REG,
					SC8238_REG_VALUE_08BIT,
					SC8238_FETCH_FLIP(val, ctrl->val));
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sc8238_ctrl_ops = {
	.s_ctrl = sc8238_set_ctrl,
};

static int sc8238_initialize_controls(struct sc8238 *sc8238)
{
	const struct sc8238_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0, max_dst_pixel_rate = 0;
	u8 lanes = sc8238->bus_cfg.bus.mipi_csi2.num_data_lanes;

	handler = &sc8238->ctrl_handler;
	mode = sc8238->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc8238->mutex;

	sc8238->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
						   V4L2_CID_LINK_FREQ,
						   ARRAY_SIZE
						   (link_freq_menu_items) - 1,
						   0, link_freq_menu_items);
	if (sc8238->link_freq)
		sc8238->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	dst_link_freq = mode->link_freq_idx;
	max_dst_pixel_rate = SC8238_MAX_LINK_FREQ / mode->bpp * 2 * lanes;
	dst_pixel_rate = (u32) link_freq_menu_items[mode->link_freq_idx] /
	    mode->bpp * 2 * lanes;
	sc8238->pixel_rate =
	    v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE, 0,
			      max_dst_pixel_rate, 1, dst_pixel_rate);

	__v4l2_ctrl_s_ctrl(sc8238->link_freq, dst_link_freq);

	h_blank = mode->hts_def - mode->width;
	sc8238->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (sc8238->hblank)
		sc8238->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	sc8238->vblank = v4l2_ctrl_new_std(handler, &sc8238_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   SC8238_VTS_MAX - mode->height,
					   1, vblank_def);
	exposure_max = mode->vts_def - 4;
	sc8238->exposure = v4l2_ctrl_new_std(handler, &sc8238_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     SC8238_EXPOSURE_MIN, exposure_max,
					     SC8238_EXPOSURE_STEP,
					     mode->exp_def);
	sc8238->anal_gain =
	    v4l2_ctrl_new_std(handler, &sc8238_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			      SC8238_GAIN_MIN, SC8238_GAIN_MAX,
			      SC8238_GAIN_STEP, SC8238_GAIN_DEFAULT);
	sc8238->test_pattern =
	    v4l2_ctrl_new_std_menu_items(handler, &sc8238_ctrl_ops,
					 V4L2_CID_TEST_PATTERN,
					 ARRAY_SIZE(sc8238_test_pattern_menu) -
					 1, 0, 0, sc8238_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &sc8238_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1,
			  0);
	v4l2_ctrl_new_std(handler, &sc8238_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1,
			  0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&sc8238->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc8238->subdev.ctrl_handler = handler;
	sc8238->has_init_exp = false;
	sc8238->cur_fps = mode->max_fps;
	sc8238->is_standby = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc8238_check_sensor_id(struct sc8238 *sc8238,
				  struct i2c_client *client)
{
	struct device *dev = &sc8238->client->dev;
	u32 id = 0;
	int ret = 0;

	ret = sc8238_read_reg(client, SC8238_REG_CHIP_ID_H,
			      SC8238_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected sc8238 id(%06x)\n", CHIP_ID);

	return 0;
}

static int sc8238_configure_regulators(struct sc8238 *sc8238)
{
	unsigned int i;

	for (i = 0; i < SC8238_NUM_SUPPLIES; i++)
		sc8238->supplies[i].supply = sc8238_supply_names[i];

	return devm_regulator_bulk_get(&sc8238->client->dev,
				       SC8238_NUM_SUPPLIES, sc8238->supplies);
}

static int sc8238_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sc8238 *sc8238;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	int i, hdr_mode = 0;
	struct device_node *endpoint;
	const char *sync_mode_name = NULL;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	sc8238 = devm_kzalloc(dev, sizeof(*sc8238), GFP_KERNEL);
	if (!sc8238)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc8238->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc8238->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc8238->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc8238->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	/* Compatible with non-standby mode if this attribute is not configured in dts */
	of_property_read_u32(node, RKMODULE_CAMERA_STANDBY_HW,
			     &sc8238->standby_hw);
	dev_info(dev, "sc8238->standby_hw = %d\n", sc8238->standby_hw);
	ret = of_property_read_string(node, RKMODULE_CAMERA_SYNC_MODE,
				      &sync_mode_name);
	if (ret) {
		sc8238->sync_mode = NO_SYNC_MODE;
		dev_err(dev, "could not get sync mode!\n");
	} else {
		if (strcmp(sync_mode_name, RKMODULE_EXTERNAL_MASTER_MODE) == 0) {
			sc8238->sync_mode = EXTERNAL_MASTER_MODE;
			dev_info(dev, "external master mode\n");
		} else if (strcmp(sync_mode_name, RKMODULE_INTERNAL_MASTER_MODE)
			   == 0) {
			sc8238->sync_mode = INTERNAL_MASTER_MODE;
			dev_info(dev, "internal master mode\n");
		} else if (strcmp(sync_mode_name, RKMODULE_SOFT_SYNC_MODE) == 0) {
			sc8238->sync_mode = SOFT_SYNC_MODE;
			dev_info(dev, "soft sync mode\n");
		}
	}

	sc8238->is_thunderboot =
	    IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);
	dev_err(dev, "========= is_thunderboot %d\n", sc8238->is_thunderboot);

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
					 &sc8238->bus_cfg);
	of_node_put(endpoint);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}

	sc8238->supported_modes = supported_modes;
	sc8238->cfg_num = ARRAY_SIZE(supported_modes);
	dev_info(dev, "detect sc8238 lane: %d\n",
		 sc8238->bus_cfg.bus.mipi_csi2.num_data_lanes);

	sc8238->client = client;
	for (i = 0; i < sc8238->cfg_num; i++) {
		if (hdr_mode == sc8238->supported_modes[i].hdr_mode) {
			sc8238->cur_mode = &sc8238->supported_modes[i];
			break;
		}
	}

	if (i == sc8238->cfg_num)
		sc8238->cur_mode = &sc8238->supported_modes[0];

	dev_dbg(dev, "SC8238 Info hdr_mode %d lanes %d vts 0x%04x fps %d\n",
		sc8238->cur_mode->hdr_mode, sc8238->cur_mode->lanes,
		sc8238->cur_mode->vts_def,
		sc8238->cur_mode->max_fps.denominator /
		sc8238->cur_mode->max_fps.numerator);

	sc8238->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc8238->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	sc8238->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sc8238->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	sc8238->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_HIGH);
	if (IS_ERR(sc8238->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	sc8238->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc8238->pinctrl)) {
		sc8238->pins_default =
		    pinctrl_lookup_state(sc8238->pinctrl,
					 OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc8238->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		sc8238->pins_sleep =
		    pinctrl_lookup_state(sc8238->pinctrl,
					 OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc8238->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = sc8238_configure_regulators(sc8238);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc8238->mutex);

	sd = &sc8238->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc8238_subdev_ops);
	ret = sc8238_initialize_controls(sc8238);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc8238_power_on(sc8238);
	if (ret)
		goto err_free_handler;

	ret = sc8238_check_sensor_id(sc8238, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc8238_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc8238->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc8238->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	if (!sc8238->cam_sw_inf) {
		sc8238->cam_sw_inf = cam_sw_init();
		cam_sw_clk_init(sc8238->cam_sw_inf, sc8238->xvclk,
				sc8238->cur_mode->mclk);
		cam_sw_reset_pin_init(sc8238->cam_sw_inf, sc8238->reset_gpio,
				      0);
		cam_sw_pwdn_pin_init(sc8238->cam_sw_inf, sc8238->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc8238->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc8238->module_index, facing, SC8238_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (sc8238->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__sc8238_power_off(sc8238);
err_free_handler:
	v4l2_ctrl_handler_free(&sc8238->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc8238->mutex);

	return ret;
}

static void sc8238_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc8238 *sc8238 = to_sc8238(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc8238->ctrl_handler);
	mutex_destroy(&sc8238->mutex);

	cam_sw_deinit(sc8238->cam_sw_inf);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc8238_power_off(sc8238);
	pm_runtime_set_suspended(&client->dev);

}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc8238_of_match[] = {
	{.compatible = "smartsens,sc8238" },
	{ },
};

MODULE_DEVICE_TABLE(of, sc8238_of_match);
#endif

static const struct i2c_device_id sc8238_match_id[] = {
	{ "smartsens,sc8238", 0 },
	{ },
};

static struct i2c_driver sc8238_i2c_driver = {
	.driver = {
		   .name = SC8238_NAME,
		   .pm = &sc8238_pm_ops,
		   .of_match_table = of_match_ptr(sc8238_of_match),
		    },
	.probe = sc8238_probe,
	.remove = sc8238_remove,
	.id_table = sc8238_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc8238_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc8238_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens sc8238 sensor driver");
MODULE_LICENSE("GPL");
