// SPDX-License-Identifier: GPL-2.0
/*
 * imx351 driver
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 * V0.0X01.0X01 add imx351 driver.
 * V0.0X01.0X02 add imx351 support mirror and flip.
 * V0.0X01.0X03 add quick stream on/off
 * V0.0X01.0X04 support rk otp spec.
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
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/mfd/syscon.h>
#include <linux/rk-preisp.h>
#include "otp_eeprom.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x03)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define IMX351_LINK_FREQ_660		660000000UL	// 1320Mbps per lane
#define IMX351_LINK_FREQ_399		399000000UL	// 798Mbps per lane

#define IMX351_LANES			4

#define PIXEL_RATE_WITH_846M_10BIT	(IMX351_LINK_FREQ_660 * 2 / 10 * 4)

#define IMX351_XVCLK_FREQ		24000000

#define CHIP_ID				0x0351
#define IMX351_REG_CHIP_ID_H		0x0016
#define IMX351_REG_CHIP_ID_L		0x0017

#define IMX351_REG_CTRL_MODE		0x0100
#define IMX351_MODE_SW_STANDBY		0x0
#define IMX351_MODE_STREAMING		0x1

#define IMX351_REG_EXPOSURE_H		0x0202
#define IMX351_REG_EXPOSURE_L		0x0203
#define IMX351_EXPOSURE_MIN		8
#define IMX351_EXPOSURE_STEP		1
#define IMX351_VTS_MAX			0xffff

#define IMX351_REG_GAIN_H		0x0204
#define IMX351_REG_GAIN_L		0x0205
#define IMX351_GAIN_MIN			0x10
#define IMX351_GAIN_MAX			0x100
#define IMX351_GAIN_STEP		1
#define IMX351_GAIN_DEFAULT		0x20

#define IMX351_REG_DGAIN		0x3ff9
#define IMX351_DGAIN_MODE		1
#define IMX351_REG_DGAINGR_H		0x020e
#define IMX351_REG_DGAINGR_L		0x020f
#define IMX351_REG_DGAINR_H		0x0210
#define IMX351_REG_DGAINR_L		0x0211
#define IMX351_REG_DGAINB_H		0x0212
#define IMX351_REG_DGAINB_L		0x0213
#define IMX351_REG_DGAINGB_H		0x0214
#define IMX351_REG_DGAINGB_L		0x0215
#define IMX351_REG_GAIN_GLOBAL_H	0x0218
#define IMX351_REG_GAIN_GLOBAL_L	0x0219

#define IMX351_REG_TEST_PATTERN		0x0601
#define IMX351_TEST_PATTERN_ENABLE	0x1
#define IMX351_TEST_PATTERN_DISABLE	0x0

#define IMX351_REG_VTS_H		0x0340
#define IMX351_REG_VTS_L		0x0341

#define IMX351_FLIP_MIRROR_REG		0x0101
#define IMX351_MIRROR_BIT_MASK		BIT(0)
#define IMX351_FLIP_BIT_MASK		BIT(1)

#define IMX351_FETCH_EXP_H(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX351_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define IMX351_FETCH_AGAIN_H(VAL)	(((VAL) >> 8) & 0x03)
#define IMX351_FETCH_AGAIN_L(VAL)	((VAL) & 0xFF)

#define IMX351_FETCH_DGAIN_H(VAL)	(((VAL) >> 8) & 0x0F)
#define IMX351_FETCH_DGAIN_L(VAL)	((VAL) & 0xFF)

//#define IMX351_FETCH_RHS1_H(VAL)	(((VAL) >> 16) & 0x0F)
//#define IMX351_FETCH_RHS1_M(VAL)	(((VAL) >> 8) & 0xFF)
//#define IMX351_FETCH_RHS1_L(VAL)	((VAL) & 0xFF)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define IMX351_REG_VALUE_08BIT		1
#define IMX351_REG_VALUE_16BIT		2
#define IMX351_REG_VALUE_24BIT		3

#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define RK_OTP

#define IMX351_NAME			"imx351"

static const char * const imx351_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define IMX351_NUM_SUPPLIES ARRAY_SIZE(imx351_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct other_data {
	u32 width;
	u32 height;
	u32 bus_fmt;
	u32 data_type;
	u32 data_bit;
};

struct imx351_mode {
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
	u32 mipi_freq_idx;
	const struct other_data *spd;
	u32 vc[PAD_MAX];
};

struct imx351 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[IMX351_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*h_flip;
	struct v4l2_ctrl	*v_flip;
	struct v4l2_ctrl	*test_pattern;
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	const struct imx351_mode *cur_mode;
	u32			cfg_num;
	s64			cur_pixel_rate;
	u32			cur_link_freq;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	u8			flip;
	u32			spd_id;
#ifdef RK_OTP
	struct otp_info		*otp;
#endif

};

#define to_imx351(sd) container_of(sd, struct imx351, subdev)

/*
 * IMX351 All-pixel scan CSI-2_4lane 24Mhz
 * AD:10bit Output:10bit 1692Mbps/lane Master Mode 30fps
 *
 */
static const struct regval imx351_linear_10bit_global_regs[] = {
	//{0x0103, 0x01},
	/* External Clock Setting */
	{0x0136, 0x18},
	{0x0137, 0x00},
	/* Register version */
	{0x3C7D, 0x28},
	{0x3C7E, 0x01},
	{0x3C7F, 0x03},
	/* Global setting */
	{0x3140, 0x02},
	{0x4430, 0x05},
	{0x4431, 0xDC},
	{0x5222, 0x02},
	{0x562B, 0x0A},
	{0x562D, 0x0C},
	{0x56B7, 0x74},
	{0x6200, 0x95},
	{0x6201, 0xB9},
	{0x6202, 0x58},
	{0x6220, 0x05},
	{0x6229, 0x70},
	{0x622A, 0xC3},
	{0x622C, 0x54},
	{0x622F, 0xA8},
	{0x6231, 0xD3},
	{0x6234, 0x6A},
	{0x6235, 0x8C},
	{0x6236, 0x37},
	{0x6237, 0x45},
	{0x623A, 0x96},
	{0x6240, 0x59},
	{0x6241, 0x83},
	{0x6243, 0x54},
	{0x6246, 0xA8},
	{0x6248, 0xD3},
	{0x624B, 0x65},
	{0x624C, 0x98},
	{0x624D, 0x37},
	{0x6265, 0x45},
	{0x626E, 0x72},
	{0x626F, 0xC3},
	{0x6271, 0x58},
	{0x6279, 0xA7},
	{0x627B, 0x37},
	{0x6282, 0x82},
	{0x6283, 0x80},
	{0x6286, 0x07},
	{0x6287, 0xC0},
	{0x6288, 0x08},
	{0x628A, 0x18},
	{0x628B, 0x80},
	{0x628C, 0x20},
	{0x628E, 0x32},
	{0x6290, 0x40},
	{0x6292, 0x0A},
	{0x6296, 0x50},
	{0x629A, 0xF8},
	{0x629B, 0x01},
	{0x629D, 0x03},
	{0x629F, 0x04},
	{0x62B1, 0x06},
	{0x62B5, 0x3C},
	{0x62B9, 0xC8},
	{0x62BC, 0x02},
	{0x62BD, 0x70},
	{0x62D0, 0x06},
	{0x62D4, 0x38},
	{0x62D8, 0xB8},
	{0x62DB, 0x02},
	{0x62DC, 0x40},
	{0x62DD, 0x03},
	{0x637A, 0x31},
	{0x637B, 0xD4},
	{0x6388, 0x22},
	{0x6389, 0x82},
	{0x638A, 0xC8},
	{0x9006, 0x01},
	{0x935D, 0x01},
	{0x9389, 0x05},
	{0x938B, 0x05},
	{0x9391, 0x05},
	{0x9393, 0x05},
	{0x9395, 0x82},
	{0x9397, 0x78},
	{0x9399, 0x05},
	{0x939B, 0x05},
	{0xBC40, 0x03},
	{0xE000, 0x01},
	{0xE0A5, 0x0B},
	{0xE0A6, 0x0B},
	{REG_NULL, 0x00},
};

/* INCK 24  pclk=676.8Mhz
 * frame_length_lines  3736
 * line_length_pck 6032
 * Data rate[Mbps/lane] 1692
 * FPS 30
 * Linetime 8192ns
 * Vbanking 2.139ms
 */

static const struct regval imx351_linear_10bit_4656x3496_30fps_nopd_regs[] = {
	{0x0100, 0x00},
	/* MIPI output setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0114, 0x03},

	/* Line Length PCK Setting */
	{0x0342, 0x17},
	{0x0343, 0x90},

	/* Frame Length Lines Setting */
	{0x0340, 0x0D},
	{0x0341, 0xFC},

	/* ROI Setting */
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x12},
	{0x0349, 0x2F},
	{0x034A, 0x0D},
	{0x034B, 0xA7},

	/* Mode Setting */
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0222, 0x01},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x0902, 0x0A},
	{0x3243, 0x00},
	{0x3F4C, 0x01},
	{0x3F4D, 0x01},
	{0x4254, 0x7F},
	{0x9385, 0x5B},
	{0x9387, 0x54},
	{0x938D, 0x77},
	{0x938F, 0x66},

	/* Digital Crop & Scaling */
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x12},
	{0x040D, 0x30},
	{0x040E, 0x0D},
	{0x040F, 0xA8},
	{0xBC41, 0x01},

	/* Output Size Setting */
	{0x034C, 0x12},
	{0x034D, 0x30},
	{0x034E, 0x0D},
	{0x034F, 0xA8},

	/* Clock Setting */
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x01},
	{0x0307, 0x0E},
	{0x030B, 0x01},
	{0x030D, 0x04},
	{0x030E, 0x00},
	{0x030F, 0xDC},
	{0x0310, 0x01},
	{0x0820, 0x14},
	{0x0821, 0xA0},
	{0x0822, 0x00},
	{0x0823, 0x00},

	/* PDAF Setting */
	{0x3E20, 0x01},
	{0x3E37, 0x01},
	{0x3E3B, 0x00},

	/* EIS Setting */
	{0x3614, 0x00},
	{0x3616, 0x0E},
	{0x3617, 0x66},

	/* Other Setting */
	{0x0106, 0x00},
	{0x0B00, 0x00},
	{0x3230, 0x00},
	{0x3F14, 0x01},
	{0x3F17, 0x00},
	{0x3F3C, 0x01},

	/* Integration Setting */
	{0x0202, 0x0D},
	{0x0203, 0xE8},
	{0x0224, 0x01},
	{0x0225, 0xF4},

	/* Gain Setting */
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x0216, 0x00},
	{0x0217, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0218, 0x01},
	{0x0219, 0x00},
	{REG_NULL, 0x00},
};

static const struct imx351_mode supported_modes[] = {
	{
		.width = 4656,
		.height = 3496,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0E84,
		.hts_def = 0x1790,
		.vts_def = 0x0E98,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.global_reg_list = imx351_linear_10bit_global_regs,
		.reg_list = imx351_linear_10bit_4656x3496_30fps_nopd_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 1,
		.spd = NULL,
		.vc[PAD0] = 0,
	},
};

/*
 * pixel_rate = link_freq * data-rate * nr_of_lanes / bits_per_sample
 * data rate => double data rate; number of lanes => 2; bits per pixel => 10
 */
static u64 link_freq_to_pixel_rate(u64 f)
{
	f *= 2 * IMX351_LANES;
	do_div(f, 10);

	return f;
}

static const s64 link_freq_items[] = {
	IMX351_LINK_FREQ_399,
	IMX351_LINK_FREQ_660,
};

static const char * const imx351_test_pattern_menu[] = {
	"Disabled",
	"Solid color",
	"100% color bars",
	"Fade to grey color bars",
	"PN9"
};

/* Write registers up to 4 at a time */
static int imx351_write_reg(struct i2c_client *client, u16 reg,
			    int len, u32 val)
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

static int imx351_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		if (unlikely(regs[i].addr == REG_DELAY))
			usleep_range(regs[i].val, regs[i].val * 2);
		else
			ret = imx351_write_reg(client, regs[i].addr,
					       IMX351_REG_VALUE_08BIT,
					       regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int imx351_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
			   u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret, i;

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

	for (i = 0; i < 3; i++) {
		ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
		if (ret == ARRAY_SIZE(msgs))
			break;
	}
	if (ret != ARRAY_SIZE(msgs) && i == 3)
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

static int imx351_get_reso_dist(const struct imx351_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		   abs(mode->height - framefmt->height);
}

static const struct imx351_mode *
imx351_find_best_fit(struct imx351 *imx351, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < imx351->cfg_num; i++) {
		dist = imx351_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int imx351_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx351 *imx351 = to_imx351(sd);
	const struct imx351_mode *mode;
	s64 h_blank, vblank_def;
	s64 pixel_rate = 0;

	mutex_lock(&imx351->mutex);

	mode = imx351_find_best_fit(imx351, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx351->mutex);
		return -ENOTTY;
#endif
	} else {
		imx351->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx351->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(imx351->vblank, vblank_def,
					 IMX351_VTS_MAX - mode->height,
					 1, vblank_def);

		__v4l2_ctrl_s_ctrl(imx351->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(imx351->link_freq, mode->mipi_freq_idx);
		pixel_rate = link_freq_to_pixel_rate(link_freq_items[mode->mipi_freq_idx]);
		__v4l2_ctrl_s_ctrl_int64(imx351->pixel_rate,
					 pixel_rate);
	}

	dev_info(&imx351->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&imx351->mutex);

	return 0;
}

static int imx351_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx351 *imx351 = to_imx351(sd);
	const struct imx351_mode *mode = imx351->cur_mode;

	mutex_lock(&imx351->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&imx351->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		if (imx351->flip & IMX351_MIRROR_BIT_MASK) {
			fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;
			if (imx351->flip & IMX351_FLIP_BIT_MASK)
				fmt->format.code = MEDIA_BUS_FMT_SBGGR10_1X10;
		} else if (imx351->flip & IMX351_FLIP_BIT_MASK) {
			fmt->format.code = MEDIA_BUS_FMT_SGBRG10_1X10;
		} else {
			fmt->format.code = mode->bus_fmt;
		}
		fmt->format.field = V4L2_FIELD_NONE;
		/* format info: width/height/data type/virctual channel */
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&imx351->mutex);

	return 0;
}

static int imx351_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx351 *imx351 = to_imx351(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = imx351->cur_mode->bus_fmt;

	return 0;
}

static int imx351_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx351 *imx351 = to_imx351(sd);

	if (fse->index >= imx351->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int imx351_enable_test_pattern(struct imx351 *imx351, u32 pattern)
{
	u32 val;

	if (pattern)
		val = (pattern - 1) | IMX351_TEST_PATTERN_ENABLE;
	else
		val = IMX351_TEST_PATTERN_DISABLE;

	return imx351_write_reg(imx351->client,
				IMX351_REG_TEST_PATTERN,
				IMX351_REG_VALUE_08BIT,
				val);
}

static int imx351_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx351 *imx351 = to_imx351(sd);
	const struct imx351_mode *mode = imx351->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH 4656
#define DST_HEIGHT 3496

/*
 * The resolution of the driver configuration needs to be exactly
 * the same as the current output resolution of the sensor,
 * the input width of the isp needs to be 16 aligned,
 * the input height of the isp needs to be 8 aligned.
 * Can be cropped to standard resolution by this function,
 * otherwise it will crop out strange resolution according
 * to the alignment rules.
 */
static int imx351_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct imx351 *imx351 = to_imx351(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		if (imx351->cur_mode->width == 4656) {
			sel->r.left = CROP_START(imx351->cur_mode->width, DST_WIDTH);
			sel->r.width = DST_WIDTH;
			sel->r.top = CROP_START(imx351->cur_mode->height, DST_HEIGHT);
			sel->r.height = DST_HEIGHT;
		} else {
			sel->r.left = 0;
			sel->r.width = imx351->cur_mode->width;
			sel->r.top = 0;
			sel->r.height = imx351->cur_mode->height;
		}
		return 0;
	}

	return -EINVAL;
}

static int imx351_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = IMX351_LANES;

	return 0;
}

#ifdef RK_OTP
static void imx351_get_otp(struct otp_info *otp,
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

static void imx351_get_module_inf(struct imx351 *imx351,
				  struct rkmodule_inf *inf)
{
#ifdef RK_OTP
	struct otp_info *otp = imx351->otp;
#endif
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, IMX351_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, imx351->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, imx351->len_name, sizeof(inf->base.lens));
#ifdef RK_OTP
	if (otp)
		imx351_get_otp(otp, inf);
#endif
}

static int imx351_get_channel_info(struct imx351 *imx351, struct rkmodule_channel_info *ch_info)
{
	const struct imx351_mode *mode = imx351->cur_mode;

	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;

	if (ch_info->index == imx351->spd_id && mode->spd) {
		ch_info->vc = 0;
		ch_info->width = mode->spd->width;
		ch_info->height = mode->spd->height;
		ch_info->bus_fmt = mode->spd->bus_fmt;
		ch_info->data_type = mode->spd->data_type;
		ch_info->data_bit = mode->spd->data_bit;
	} else {
		ch_info->vc = imx351->cur_mode->vc[ch_info->index];
		ch_info->width = imx351->cur_mode->width;
		ch_info->height = imx351->cur_mode->height;
		ch_info->bus_fmt = imx351->cur_mode->bus_fmt;
	}
	return 0;
}

static long imx351_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx351 *imx351 = to_imx351(sd);
	struct rkmodule_channel_info *ch_info;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx351_get_module_inf(imx351, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx351_write_reg(imx351->client, IMX351_REG_CTRL_MODE,
				IMX351_REG_VALUE_08BIT, IMX351_MODE_STREAMING);
		else
			ret = imx351_write_reg(imx351->client, IMX351_REG_CTRL_MODE,
				IMX351_REG_VALUE_08BIT, IMX351_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = imx351_get_channel_info(imx351, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx351_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
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

		ret = imx351_ioctl(sd, cmd, inf);
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
			ret = imx351_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(ch_info, up, sizeof(*ch_info));
		if (ret) {
			ret = -EFAULT;
			kfree(ch_info);
			return ret;
		}

		ret = imx351_ioctl(sd, cmd, ch_info);
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

static int imx351_set_flip(struct imx351 *imx351)
{
	int ret = 0;
	u32 val = 0;

	ret = imx351_read_reg(imx351->client, IMX351_FLIP_MIRROR_REG,
			      IMX351_REG_VALUE_08BIT, &val);
	if (imx351->flip & IMX351_MIRROR_BIT_MASK)
		val |= IMX351_MIRROR_BIT_MASK;
	else
		val &= ~IMX351_MIRROR_BIT_MASK;
	if (imx351->flip & IMX351_FLIP_BIT_MASK)
		val |= IMX351_FLIP_BIT_MASK;
	else
		val &= ~IMX351_FLIP_BIT_MASK;
	ret |= imx351_write_reg(imx351->client, IMX351_FLIP_MIRROR_REG,
				IMX351_REG_VALUE_08BIT, val);

	return ret;
}

static int __imx351_start_stream(struct imx351 *imx351)
{
	int ret;

	ret = imx351_write_array(imx351->client, imx351->cur_mode->global_reg_list);
	if (ret)
		return ret;
	ret |= imx351_write_array(imx351->client, imx351->cur_mode->reg_list);
	if (ret)
		return ret;
	imx351->cur_vts = imx351->cur_mode->vts_def;
	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx351->ctrl_handler);
	if (ret)
		return ret;

	ret = imx351_set_flip(imx351);
	if (ret)
		return ret;

	return imx351_write_reg(imx351->client, IMX351_REG_CTRL_MODE,
				IMX351_REG_VALUE_08BIT, IMX351_MODE_STREAMING);
}

static int __imx351_stop_stream(struct imx351 *imx351)
{
	return imx351_write_reg(imx351->client, IMX351_REG_CTRL_MODE,
				IMX351_REG_VALUE_08BIT, IMX351_MODE_SW_STANDBY);
}

static int imx351_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx351 *imx351 = to_imx351(sd);
	struct i2c_client *client = imx351->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				imx351->cur_mode->width,
				imx351->cur_mode->height,
		DIV_ROUND_CLOSEST(imx351->cur_mode->max_fps.denominator,
				  imx351->cur_mode->max_fps.numerator));

	mutex_lock(&imx351->mutex);
	on = !!on;
	if (on == imx351->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx351_start_stream(imx351);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx351_stop_stream(imx351);
		pm_runtime_put(&client->dev);
	}

	imx351->streaming = on;

unlock_and_return:
	mutex_unlock(&imx351->mutex);

	return ret;
}

static int imx351_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx351 *imx351 = to_imx351(sd);
	struct i2c_client *client = imx351->client;
	int ret = 0;

	dev_info(&client->dev, "%s(%d) on(%d)\n", __func__, __LINE__, on);
	mutex_lock(&imx351->mutex);

	/* If the power state is not modified - no work to do. */
	if (imx351->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		imx351->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx351->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx351->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 imx351_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, IMX351_XVCLK_FREQ / 1000 / 1000);
}

static int __imx351_power_on(struct imx351 *imx351)
{
	int ret;
	u32 delay_us;
	struct device *dev = &imx351->client->dev;

	ret = clk_set_rate(imx351->xvclk, IMX351_XVCLK_FREQ);
	if (ret < 0) {
		dev_err(dev, "Failed to set xvclk rate (24MHz)\n");
		return ret;
	}
	if (clk_get_rate(imx351->xvclk) != IMX351_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(imx351->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (!IS_ERR(imx351->reset_gpio))
		gpiod_set_value_cansleep(imx351->reset_gpio, 0);

	ret = regulator_bulk_enable(IMX351_NUM_SUPPLIES, imx351->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(imx351->reset_gpio))
		gpiod_set_value_cansleep(imx351->reset_gpio, 1);

	/* need wait 8ms to set register */
	usleep_range(8000, 10000);

	if (!IS_ERR(imx351->pwdn_gpio))
		gpiod_set_value_cansleep(imx351->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = imx351_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(imx351->xvclk);

	return ret;
}

static void __imx351_power_off(struct imx351 *imx351)
{
	if (!IS_ERR(imx351->pwdn_gpio))
		gpiod_set_value_cansleep(imx351->pwdn_gpio, 0);
	clk_disable_unprepare(imx351->xvclk);
	if (!IS_ERR(imx351->reset_gpio))
		gpiod_set_value_cansleep(imx351->reset_gpio, 0);
	regulator_bulk_disable(IMX351_NUM_SUPPLIES, imx351->supplies);
}

static int imx351_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx351 *imx351 = to_imx351(sd);

	return __imx351_power_on(imx351);
}

static int imx351_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx351 *imx351 = to_imx351(sd);

	__imx351_power_off(imx351);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx351_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx351 *imx351 = to_imx351(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct imx351_mode *def_mode = &supported_modes[0];

	mutex_lock(&imx351->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx351->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int imx351_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx351 *imx351 = to_imx351(sd);

	if (fie->index >= imx351->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops imx351_pm_ops = {
	SET_RUNTIME_PM_OPS(imx351_runtime_suspend,
			   imx351_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx351_internal_ops = {
	.open = imx351_open,
};
#endif

static const struct v4l2_subdev_core_ops imx351_core_ops = {
	.s_power = imx351_s_power,
	.ioctl = imx351_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx351_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx351_video_ops = {
	.s_stream = imx351_s_stream,
	.g_frame_interval = imx351_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx351_pad_ops = {
	.enum_mbus_code = imx351_enum_mbus_code,
	.enum_frame_size = imx351_enum_frame_sizes,
	.enum_frame_interval = imx351_enum_frame_interval,
	.get_fmt = imx351_get_fmt,
	.set_fmt = imx351_set_fmt,
	.get_selection = imx351_get_selection,
	.get_mbus_config = imx351_g_mbus_config,
};

static const struct v4l2_subdev_ops imx351_subdev_ops = {
	.core	= &imx351_core_ops,
	.video	= &imx351_video_ops,
	.pad	= &imx351_pad_ops,
};

static int imx351_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx351 *imx351 = container_of(ctrl->handler,
					     struct imx351, ctrl_handler);
	struct i2c_client *client = imx351->client;
	s64 max;
	int ret = 0;
	u32 again = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = imx351->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(imx351->exposure,
					 imx351->exposure->minimum, max,
					 imx351->exposure->step,
					 imx351->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = imx351_write_reg(imx351->client,
				       IMX351_REG_EXPOSURE_H,
				       IMX351_REG_VALUE_08BIT,
				       IMX351_FETCH_EXP_H(ctrl->val));
		ret |= imx351_write_reg(imx351->client,
					IMX351_REG_EXPOSURE_L,
					IMX351_REG_VALUE_08BIT,
					IMX351_FETCH_EXP_L(ctrl->val));
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		/* gain_reg = 1024 - 1024 / gain_ana
		 * manual multiple 16 to add accuracy:
		 * then formula change to:
		 * gain_reg = 1024 - 1024 * 16 / (gain_ana * 16)
		 * max gain 16x
		 */
		if (ctrl->val > 0x100)
			ctrl->val = 0x100;
		if (ctrl->val < 0x10)
			ctrl->val = 0x10;

		again = 1024 - 1024 * 16 / ctrl->val;
		ret = imx351_write_reg(imx351->client, IMX351_REG_GAIN_H,
				       IMX351_REG_VALUE_08BIT,
				       IMX351_FETCH_AGAIN_H(again));
		ret |= imx351_write_reg(imx351->client, IMX351_REG_GAIN_L,
					IMX351_REG_VALUE_08BIT,
					IMX351_FETCH_AGAIN_L(again));

		dev_dbg(&client->dev, "set analog gain 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = imx351_write_reg(imx351->client,
				       IMX351_REG_VTS_H,
				       IMX351_REG_VALUE_08BIT,
				       (ctrl->val + imx351->cur_mode->height)
				       >> 8);
		ret |= imx351_write_reg(imx351->client,
					IMX351_REG_VTS_L,
					IMX351_REG_VALUE_08BIT,
					(ctrl->val + imx351->cur_mode->height)
					& 0xff);
		imx351->cur_vts = ctrl->val + imx351->cur_mode->height;
		dev_info(&client->dev, "set vblank 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->val)
			imx351->flip |= IMX351_MIRROR_BIT_MASK;
		else
			imx351->flip &= ~IMX351_MIRROR_BIT_MASK;
		dev_info(&client->dev, "set hflip 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		if (ctrl->val)
			imx351->flip |= IMX351_FLIP_BIT_MASK;
		else
			imx351->flip &= ~IMX351_FLIP_BIT_MASK;
		dev_info(&client->dev, "set vflip 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		dev_dbg(&client->dev, "set testpattern 0x%x\n", ctrl->val);
		ret = imx351_enable_test_pattern(imx351, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx351_ctrl_ops = {
	.s_ctrl = imx351_set_ctrl,
};

static int imx351_initialize_controls(struct imx351 *imx351)
{
	const struct imx351_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &imx351->ctrl_handler;
	mode = imx351->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &imx351->mutex;

	imx351->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);

	imx351->cur_link_freq = 0;
	imx351->cur_pixel_rate = link_freq_to_pixel_rate(link_freq_items[mode->mipi_freq_idx]);

	imx351->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
					       V4L2_CID_PIXEL_RATE,
					       0, PIXEL_RATE_WITH_846M_10BIT,
					       1, imx351->cur_pixel_rate);
	v4l2_ctrl_s_ctrl(imx351->link_freq,
			   imx351->cur_link_freq);

	h_blank = mode->hts_def - mode->width;
	imx351->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (imx351->hblank)
		imx351->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx351->vblank = v4l2_ctrl_new_std(handler, &imx351_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   IMX351_VTS_MAX - mode->height,
					   1, vblank_def);
	imx351->cur_vts = mode->vts_def;
	exposure_max = mode->vts_def - 4;
	imx351->exposure = v4l2_ctrl_new_std(handler, &imx351_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX351_EXPOSURE_MIN,
					     exposure_max,
					     IMX351_EXPOSURE_STEP,
					     mode->exp_def);
	imx351->anal_gain = v4l2_ctrl_new_std(handler, &imx351_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN,
					      IMX351_GAIN_MIN,
					      IMX351_GAIN_MAX,
					      IMX351_GAIN_STEP,
					      IMX351_GAIN_DEFAULT);
	imx351->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							    &imx351_ctrl_ops,
				V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(imx351_test_pattern_menu) - 1,
				0, 0, imx351_test_pattern_menu);

	imx351->h_flip = v4l2_ctrl_new_std(handler, &imx351_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);

	imx351->v_flip = v4l2_ctrl_new_std(handler, &imx351_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	imx351->flip = 0;

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx351->client->dev,
			"Failed to init controls(  %d  )\n", ret);
		goto err_free_handler;
	}

	imx351->subdev.ctrl_handler = handler;
	imx351->has_init_exp = false;
	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int imx351_check_sensor_id(struct imx351 *imx351,
				  struct i2c_client *client)
{
	struct device *dev = &imx351->client->dev;
	u16 id = 0;
	u32 reg_H = 0;
	u32 reg_L = 0;
	int ret;

	ret = imx351_read_reg(client, IMX351_REG_CHIP_ID_H,
			      IMX351_REG_VALUE_08BIT, &reg_H);
	ret |= imx351_read_reg(client, IMX351_REG_CHIP_ID_L,
			       IMX351_REG_VALUE_08BIT, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (!(reg_H == (CHIP_ID >> 8) && reg_L == (CHIP_ID & 0xff))) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected imx351 %04x sensor\n", id);
	return 0;
}

static int imx351_configure_regulators(struct imx351 *imx351)
{
	unsigned int i;

	for (i = 0; i < IMX351_NUM_SUPPLIES; i++)
		imx351->supplies[i].supply = imx351_supply_names[i];

	return devm_regulator_bulk_get(&imx351->client->dev,
				       IMX351_NUM_SUPPLIES,
				       imx351->supplies);
}

static int imx351_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx351 *imx351;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;
#ifdef RK_OTP
	struct device_node *eeprom_ctrl_node;
	struct i2c_client *eeprom_ctrl_client = NULL;
	struct v4l2_subdev *eeprom_ctrl;
	struct otp_info *otp_ptr;
#endif

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	imx351 = devm_kzalloc(dev, sizeof(*imx351), GFP_KERNEL);
	if (!imx351)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx351->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx351->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx351->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx351->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	imx351->client = client;
	imx351->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < imx351->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			imx351->cur_mode = &supported_modes[i];
			break;
		}
	}

	if (i == imx351->cfg_num)
		imx351->cur_mode = &supported_modes[0];

	imx351->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx351->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx351->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx351->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	imx351->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(imx351->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = of_property_read_u32(node, "rockchip,spd-id", &imx351->spd_id);
	if (ret != 0) {
		imx351->spd_id = PAD_MAX;
		dev_err(dev,
			"failed get spd_id, will not to use spd\n");
	}

	ret = imx351_configure_regulators(imx351);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&imx351->mutex);

	sd = &imx351->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx351_subdev_ops);

	ret = imx351_initialize_controls(imx351);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx351_power_on(imx351);
	if (ret)
		goto err_free_handler;

	ret = imx351_check_sensor_id(imx351, client);
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
			if (!otp_ptr) {
				put_device(&eeprom_ctrl_client->dev);
				ret = -ENOMEM;
				goto err_power_off;
			}
			ret = v4l2_subdev_call(eeprom_ctrl,
				core, ioctl, 0, otp_ptr);
			if (!ret) {
				imx351->otp = otp_ptr;
			} else {
				imx351->otp = NULL;
				devm_kfree(dev, otp_ptr);
				dev_warn(dev, "can not get otp info, skip!\n");
			}
		}
		put_device(&eeprom_ctrl_client->dev);
	}
continue_probe:
#endif

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx351_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx351->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx351->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(imx351->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx351->module_index, facing,
		 IMX351_NAME, dev_name(sd->dev));
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
	__imx351_power_off(imx351);
err_free_handler:
	v4l2_ctrl_handler_free(&imx351->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx351->mutex);

	return ret;
}

static void imx351_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx351 *imx351 = to_imx351(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx351->ctrl_handler);
	mutex_destroy(&imx351->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx351_power_off(imx351);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx351_of_match[] = {
	{ .compatible = "sony,imx351" },
	{},
};
MODULE_DEVICE_TABLE(of, imx351_of_match);
#endif

static const struct i2c_device_id imx351_match_id[] = {
	{ "imx351", 0 },
	{ },
};

static struct i2c_driver imx351_i2c_driver = {
	.driver = {
		.name = IMX351_NAME,
		.pm = &imx351_pm_ops,
		.of_match_table = of_match_ptr(imx351_of_match),
	},
	.probe		= imx351_probe,
	.remove		= imx351_remove,
	.id_table	= imx351_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx351_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx351_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx351 sensor driver");
MODULE_LICENSE("GPL");
