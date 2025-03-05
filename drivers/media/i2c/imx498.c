// SPDX-License-Identifier: GPL-2.0
/*
 * imx498 driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 * V0.0X01.0X00 init version.
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define IMX498_LINK_FREQ_822		822000000	// 1644Mbps per lane
#define IMX498_LINK_FREQ_252            252000000	// 297Mbps per lane

#define IMX498_LANES			4

#define PIXEL_RATE_WITH_822M_10BIT	(IMX498_LINK_FREQ_822 * 2 / 10 * 4)

#define IMX498_XVCLK_FREQ		24000000

#define CHIP_ID				0x0498
#define IMX498_REG_CHIP_ID_H		0x0016
#define IMX498_REG_CHIP_ID_L		0x0017

#define IMX498_REG_CTRL_MODE		0x0100
#define IMX498_MODE_SW_STANDBY		0x0
#define IMX498_MODE_STREAMING		0x1

#define IMX498_REG_EXPOSURE_H		0x0202
#define IMX498_REG_EXPOSURE_L		0x0203
#define IMX498_EXPOSURE_MIN		2
#define IMX498_EXPOSURE_STEP		1
#define IMX498_VTS_MAX			0x7fff

#define IMX498_REG_GAIN_H		0x0204
#define IMX498_REG_GAIN_L		0x0205
#define IMX498_GAIN_MIN			0x10
#define IMX498_GAIN_MAX			0x200
#define IMX498_GAIN_STEP		1
#define IMX498_GAIN_DEFAULT		0x40

#define IMX498_REG_DGAIN		0x3130
#define IMX498_DGAIN_MODE		BIT(0)
#define IMX498_REG_DGAINGR_H		0x020e
#define IMX498_REG_DGAINGR_L		0x020f
#define IMX498_REG_DGAINR_H		0x0210
#define IMX498_REG_DGAINR_L		0x0211
#define IMX498_REG_DGAINB_H		0x0212
#define IMX498_REG_DGAINB_L		0x0213
#define IMX498_REG_DGAINGB_H		0x0214
#define IMX498_REG_DGAINGB_L		0x0215
#define IMX498_REG_GAIN_GLOBAL_H	0x3ffc
#define IMX498_REG_GAIN_GLOBAL_L	0x3ffd

//#define IMX498_REG_TEST_PATTERN_H	0x0600
#define IMX498_REG_TEST_PATTERN	0x0601
#define IMX498_TEST_PATTERN_ENABLE	0x1
#define IMX498_TEST_PATTERN_DISABLE	0x0

#define IMX498_REG_VTS_H		0x0340
#define IMX498_REG_VTS_L		0x0341

#define IMX498_FLIP_MIRROR_REG		0x0101
#define IMX498_MIRROR_BIT_MASK		BIT(0)
#define IMX498_FLIP_BIT_MASK		BIT(1)

#define IMX498_FETCH_EXP_H(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX498_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define IMX498_FETCH_AGAIN_H(VAL)		(((VAL) >> 8) & 0x03)
#define IMX498_FETCH_AGAIN_L(VAL)		((VAL) & 0xFF)

#define IMX498_FETCH_DGAIN_H(VAL)		(((VAL) >> 8) & 0x0F)
#define IMX498_FETCH_DGAIN_L(VAL)		((VAL) & 0xFF)

#define IMX498_FETCH_RHS1_H(VAL)	(((VAL) >> 16) & 0x0F)
#define IMX498_FETCH_RHS1_M(VAL)	(((VAL) >> 8) & 0xFF)
#define IMX498_FETCH_RHS1_L(VAL)	((VAL) & 0xFF)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define IMX498_REG_VALUE_08BIT		1
#define IMX498_REG_VALUE_16BIT		2
#define IMX498_REG_VALUE_24BIT		3

#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

#define IMX498_NAME			"imx498"

static const char * const imx498_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define IMX498_NUM_SUPPLIES ARRAY_SIZE(imx498_supply_names)

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

struct imx498_mode {
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

struct imx498 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[IMX498_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*h_flip;
	struct v4l2_ctrl	*v_flip;
	struct v4l2_ctrl	*test_pattern;
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	const struct imx498_mode *cur_mode;
	u32			cfg_num;
	u32			cur_pixel_rate;
	u32			cur_link_freq;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	struct preisp_hdrae_exp_s init_hdrae_exp;
	u8			flip;
	struct otp_info		*otp;
	u32			spd_id;
};

#define to_imx498(sd) container_of(sd, struct imx498, subdev)

/*
 * IMX498 All-pixel scan CSI-2_4lane 24Mhz
 * AD:10bit Output:10bit 1644Mbps/lane Master Mode 30fps
 *
 */
static const struct regval imx498_linear_10bit_global_regs[] = {
	/* External Clock Setting */
	{0x0136, 0x18},
	{0x0137, 0x00},
	{0x3C7E, 0x02},
	{0x3C7F, 0x01},
	{0x3F7F, 0x01},
	{0x4D44, 0x00},
	{0x4D45, 0x27},
	{0x531C, 0x01},
	{0x531D, 0x02},
	{0x531E, 0x04},
	{0x5928, 0x00},
	{0x5929, 0x28},
	{0x592A, 0x00},
	{0x592B, 0x7E},
	{0x592C, 0x00},
	{0x592D, 0x3A},
	{0x592E, 0x00},
	{0x592F, 0x90},
	{0x5930, 0x00},
	{0x5931, 0x3F},
	{0x5932, 0x00},
	{0x5933, 0x95},
	{0x5938, 0x00},
	{0x5939, 0x20},
	{0x593A, 0x00},
	{0x593B, 0x76},
	{0x5B38, 0x00},
	{0x5B79, 0x02},
	{0x5B7A, 0x07},
	{0x5B88, 0x05},
	{0x5B8D, 0x05},
	{0x5C2E, 0x00},
	{0x5C54, 0x00},
	{0x6F6D, 0x01},
	{0x79A0, 0x01},
	{0x79A8, 0x00},
	{0x79A9, 0x46},
	{0x79AA, 0x01},
	{0x79AD, 0x00},
	{0x8169, 0x01},
	{0x8359, 0x01},
	{0x9004, 0x02},
	{0x9200, 0x6A},
	{0x9201, 0x22},
	{0x9202, 0x6A},
	{0x9203, 0x23},
	{0x9302, 0x23},
	{0x9312, 0x37},
	{0x9316, 0x37},
	{0xB046, 0x01},
	{0xB048, 0x01},
	{REG_NULL, 0x00},
};

/*
 * frame_length_lines  3566
 * line_length_pck 5120
 * Data rate[Mbps/lane] 1644
 * FPS 30
 */

static const struct regval imx498_linear_10bit_4656x3496_30fps_nopd_regs[] = {
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0114, 0x03},
	{0x0342, 0x14},
	{0x0343, 0x00},
	{0x0340, 0x0D},
	{0x0341, 0xEE},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x12},
	{0x0349, 0x2F},
	{0x034A, 0x0D},
	{0x034B, 0xA7},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x0902, 0x0A},
	{0x3F4C, 0x01},
	{0x3F4D, 0x01},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x12},
	{0x040D, 0x30},
	{0x040E, 0x0D},
	{0x040F, 0xA8},
	{0x034C, 0x12},
	{0x034D, 0x30},
	{0x034E, 0x0D},
	{0x034F, 0xA8},
	{0x0301, 0x06},
	{0x0303, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x89},
	{0x030B, 0x01},
	{0x030D, 0x02},
	{0x030E, 0x01},
	{0x030F, 0x22},
	{0x0310, 0x00},
	{0x0820, 0x19},
	{0x0821, 0xB0},
	{0x0822, 0x00},
	{0x0823, 0x00},
	{0x3E20, 0x02},
	{0x3E3B, 0x00},
	{0x4434, 0x00},
	{0x4435, 0x00},
	{0x8271, 0x00},
	{0x0106, 0x00},
	{0x0B00, 0x00},
	{0x3230, 0x00},
	{0x3C00, 0x00},
	{0x3C01, 0x38},
	{0x3F78, 0x01},
	{0x3F79, 0x20},
	{0x0202, 0x0D},
	{0x0203, 0xDC},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0100, 0x00},
	{REG_NULL, 0x00},
};

static const struct regval imx498_linear_10bit_1920x1080_50fps_nopd_regs[] = {

	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0114, 0x03},
	{0x0342, 0x14},
	{0x0343, 0x00},
	{0x0340, 0x07},
	{0x0341, 0x1E},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x02},
	{0x0347, 0x9c},
	{0x0348, 0x12},
	{0x0349, 0x2F},
	{0x034A, 0x0B},
	{0x034B, 0x0B},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x0902, 0x0A},
	{0x3F4C, 0x01},
	{0x3F4D, 0x03},
	{0x0408, 0x00},
	{0x0409, 0xCC},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x07},
	{0x040D, 0x80},
	{0x040E, 0x04},
	{0x040F, 0x38},
	{0x034C, 0x07},
	{0x034D, 0x80},
	{0x034E, 0x04},
	{0x034F, 0x38},
	{0x0301, 0x06},
	{0x0303, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x75},
	{0x030B, 0x01},
	{0x030D, 0x02},
	{0x030E, 0x00},
	{0x030F, 0x2A},
	{0x0310, 0x01},
	{0x0820, 0x09},
	{0x0821, 0xC0},
	{0x0822, 0x00},
	{0x0823, 0x00},
	{0x3E20, 0x02},
	{0x3E3B, 0x00},
	{0x4434, 0x00},
	{0x4435, 0x00},
	{0x8271, 0x00},
	{0x0106, 0x00},
	{0x0B00, 0x00},
	{0x3230, 0x00},
	{0x3C00, 0x00},
	{0x3C01, 0x8C},
	{0x3F78, 0x01},
	{0x3F79, 0x60},
	{0x0202, 0x07},
	{0x0203, 0x0C},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0100, 0x01},
	{REG_NULL, 0x00},
};

static const struct imx498_mode supported_modes[] = {
	{
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 500000,
		},
		.exp_def = 0x0398,
		.hts_def = 0x0898,
		.vts_def = 0x071e,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.global_reg_list = imx498_linear_10bit_global_regs,
		.reg_list = imx498_linear_10bit_1920x1080_50fps_nopd_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 0,
		.vc[PAD0] = 0,
	},
	{
		.width = 4656,
		.height = 3496,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0D00,
		.hts_def = 0x1400,
		.vts_def = 0x0DEE,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.global_reg_list = imx498_linear_10bit_global_regs,
		.reg_list = imx498_linear_10bit_4656x3496_30fps_nopd_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 1,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_items[] = {
	IMX498_LINK_FREQ_252,
	IMX498_LINK_FREQ_822,
};

static const char * const imx498_test_pattern_menu[] = {
	"Disabled",
	"Solid color",
	"100% color bars",
	"Fade to grey color bars",
	"PN9"
};

/* Write registers up to 4 at a time */
static int imx498_write_reg(struct i2c_client *client, u16 reg,
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

static int imx498_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		if (unlikely(regs[i].addr == REG_DELAY))
			usleep_range(regs[i].val, regs[i].val * 2);
		else
			ret = imx498_write_reg(client, regs[i].addr,
					       IMX498_REG_VALUE_08BIT,
					       regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int imx498_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int imx498_get_reso_dist(const struct imx498_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		   abs(mode->height - framefmt->height);
}

static const struct imx498_mode *
imx498_find_best_fit(struct imx498 *imx498, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < imx498->cfg_num; i++) {
		dist = imx498_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int imx498_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx498 *imx498 = to_imx498(sd);
	const struct imx498_mode *mode;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;

	mutex_lock(&imx498->mutex);

	mode = imx498_find_best_fit(imx498, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx498->mutex);
		return -ENOTTY;
#endif
	} else {
		imx498->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx498->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(imx498->vblank, vblank_def,
					 IMX498_VTS_MAX - mode->height,
					 1, vblank_def);

		__v4l2_ctrl_s_ctrl(imx498->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(imx498->link_freq, mode->mipi_freq_idx);
		pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] /
				10 * 2 * IMX498_LANES;
		__v4l2_ctrl_s_ctrl_int64(imx498->pixel_rate,
					 pixel_rate);
	}

	dev_info(&imx498->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&imx498->mutex);

	return 0;
}

static int imx498_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx498 *imx498 = to_imx498(sd);
	const struct imx498_mode *mode = imx498->cur_mode;

	mutex_lock(&imx498->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&imx498->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		if (imx498->flip & IMX498_MIRROR_BIT_MASK) {
			fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;
			if (imx498->flip & IMX498_FLIP_BIT_MASK)
				fmt->format.code = MEDIA_BUS_FMT_SBGGR10_1X10;
		} else if (imx498->flip & IMX498_FLIP_BIT_MASK) {
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
	mutex_unlock(&imx498->mutex);

	return 0;
}

static int imx498_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx498 *imx498 = to_imx498(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = imx498->cur_mode->bus_fmt;

	return 0;
}

static int imx498_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx498 *imx498 = to_imx498(sd);

	if (fse->index >= imx498->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int imx498_enable_test_pattern(struct imx498 *imx498, u32 pattern)
{
	u32 val;

	if (pattern)
		val = (pattern - 1) | IMX498_TEST_PATTERN_ENABLE;
	else
		val = IMX498_TEST_PATTERN_DISABLE;

	return imx498_write_reg(imx498->client,
				IMX498_REG_TEST_PATTERN,
				IMX498_REG_VALUE_08BIT,
				val);
}

static int imx498_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx498 *imx498 = to_imx498(sd);
	const struct imx498_mode *mode = imx498->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH 4640
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
static int imx498_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct imx498 *imx498 = to_imx498(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		if (imx498->cur_mode->width == 4656) {
			sel->r.left = CROP_START(imx498->cur_mode->width, DST_WIDTH);
			sel->r.width = DST_WIDTH;
			sel->r.top = CROP_START(imx498->cur_mode->height, DST_HEIGHT);
			sel->r.height = DST_HEIGHT;
		} else {
			sel->r.left = 0;
			sel->r.width = imx498->cur_mode->width;
			sel->r.top = 0;
			sel->r.height = imx498->cur_mode->height;
		}
		return 0;
	}

	return -EINVAL;
}

static int imx498_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = IMX498_LANES;

	return 0;
}

static void imx498_get_otp(struct otp_info *otp,
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

static void imx498_get_module_inf(struct imx498 *imx498,
				  struct rkmodule_inf *inf)
{
	struct otp_info *otp = imx498->otp;

	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, IMX498_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, imx498->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, imx498->len_name, sizeof(inf->base.lens));
	if (otp)
		imx498_get_otp(otp, inf);

}

static int imx498_get_channel_info(struct imx498 *imx498, struct rkmodule_channel_info *ch_info)
{
	const struct imx498_mode *mode = imx498->cur_mode;

	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;

	if (ch_info->index == imx498->spd_id && mode->spd) {
		ch_info->vc = 0;
		ch_info->width = mode->spd->width;
		ch_info->height = mode->spd->height;
		ch_info->bus_fmt = mode->spd->bus_fmt;
		ch_info->data_type = mode->spd->data_type;
		ch_info->data_bit = mode->spd->data_bit;
	} else {
		ch_info->vc = imx498->cur_mode->vc[ch_info->index];
		ch_info->width = imx498->cur_mode->width;
		ch_info->height = imx498->cur_mode->height;
		ch_info->bus_fmt = imx498->cur_mode->bus_fmt;
	}
	return 0;
}

static long imx498_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx498 *imx498 = to_imx498(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	long ret = 0;
	u32 i, h, w;
	u32 stream = 0;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx498_get_module_inf(imx498, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = imx498->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = imx498->cur_mode->width;
		h = imx498->cur_mode->height;
		for (i = 0; i < imx498->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode) {
				imx498->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == imx498->cfg_num) {
			dev_err(&imx498->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = imx498->cur_mode->hts_def -
			    imx498->cur_mode->width;
			h = imx498->cur_mode->vts_def -
			    imx498->cur_mode->height;
			__v4l2_ctrl_modify_range(imx498->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(imx498->vblank, h,
						 IMX498_VTS_MAX -
						 imx498->cur_mode->height,
						 1, h);
			imx498->cur_link_freq = 0;
			imx498->cur_pixel_rate =
				(u32)link_freq_items[imx498->cur_mode->mipi_freq_idx] /
				10 * 2 * IMX498_LANES;
			__v4l2_ctrl_s_ctrl_int64(imx498->pixel_rate,
						 imx498->cur_pixel_rate);
			__v4l2_ctrl_s_ctrl(imx498->link_freq,
					   imx498->cur_link_freq);
		}
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx498_write_reg(imx498->client, IMX498_REG_CTRL_MODE,
				IMX498_REG_VALUE_08BIT, IMX498_MODE_STREAMING);
		else
			ret = imx498_write_reg(imx498->client, IMX498_REG_CTRL_MODE,
				IMX498_REG_VALUE_08BIT, IMX498_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = imx498_get_channel_info(imx498, ch_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx498_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rkmodule_channel_info *ch_info;
	long ret;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx498_ioctl(sd, cmd, inf);
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
			ret = imx498_ioctl(sd, cmd, cfg);
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

		ret = imx498_ioctl(sd, cmd, hdr);
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
			ret = imx498_ioctl(sd, cmd, hdr);
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
			ret = imx498_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = imx498_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}
		ret = imx498_ioctl(sd, cmd, ch_info);
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

static int imx498_set_flip(struct imx498 *imx498)
{
	int ret = 0;
	u32 val = 0;

	ret = imx498_read_reg(imx498->client, IMX498_FLIP_MIRROR_REG,
			      IMX498_REG_VALUE_08BIT, &val);
	if (imx498->flip & IMX498_MIRROR_BIT_MASK)
		val |= IMX498_MIRROR_BIT_MASK;
	else
		val &= ~IMX498_MIRROR_BIT_MASK;
	if (imx498->flip & IMX498_FLIP_BIT_MASK)
		val |= IMX498_FLIP_BIT_MASK;
	else
		val &= ~IMX498_FLIP_BIT_MASK;
	ret |= imx498_write_reg(imx498->client, IMX498_FLIP_MIRROR_REG,
				IMX498_REG_VALUE_08BIT, val);

	return ret;
}

static int __imx498_start_stream(struct imx498 *imx498)
{
	int ret;

	ret = imx498_write_array(imx498->client, imx498->cur_mode->global_reg_list);
	if (ret)
		return ret;

	ret = imx498_write_array(imx498->client, imx498->cur_mode->reg_list);
	if (ret)
		return ret;
	imx498->cur_vts = imx498->cur_mode->vts_def;
	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx498->ctrl_handler);
	if (ret)
		return ret;
	if (imx498->has_init_exp && imx498->cur_mode->hdr_mode != NO_HDR) {
		ret = imx498_ioctl(&imx498->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&imx498->init_hdrae_exp);
		if (ret) {
			dev_err(&imx498->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}

	imx498_set_flip(imx498);

	return imx498_write_reg(imx498->client, IMX498_REG_CTRL_MODE,
				IMX498_REG_VALUE_08BIT, IMX498_MODE_STREAMING);
}

static int __imx498_stop_stream(struct imx498 *imx498)
{
	return imx498_write_reg(imx498->client, IMX498_REG_CTRL_MODE,
				IMX498_REG_VALUE_08BIT, IMX498_MODE_SW_STANDBY);
}

static int imx498_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx498 *imx498 = to_imx498(sd);
	struct i2c_client *client = imx498->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				imx498->cur_mode->width,
				imx498->cur_mode->height,
		DIV_ROUND_CLOSEST(imx498->cur_mode->max_fps.denominator,
				  imx498->cur_mode->max_fps.numerator));

	mutex_lock(&imx498->mutex);
	on = !!on;
	if (on == imx498->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx498_start_stream(imx498);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx498_stop_stream(imx498);
		pm_runtime_put(&client->dev);
	}

	imx498->streaming = on;

unlock_and_return:
	mutex_unlock(&imx498->mutex);

	return ret;
}

static int imx498_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx498 *imx498 = to_imx498(sd);
	struct i2c_client *client = imx498->client;
	int ret = 0;

	mutex_lock(&imx498->mutex);

	/* If the power state is not modified - no work to do. */
	if (imx498->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		imx498->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx498->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx498->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 imx498_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, IMX498_XVCLK_FREQ / 1000 / 1000);
}

static int __imx498_power_on(struct imx498 *imx498)
{
	int ret;
	u32 delay_us;
	struct device *dev = &imx498->client->dev;

	ret = clk_set_rate(imx498->xvclk, IMX498_XVCLK_FREQ);
	if (ret < 0) {
		dev_err(dev, "Failed to set xvclk rate (24MHz)\n");
		return ret;
	}
	if (clk_get_rate(imx498->xvclk) != IMX498_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 37.125MHz\n");
	ret = clk_prepare_enable(imx498->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (!IS_ERR(imx498->reset_gpio))
		gpiod_set_value_cansleep(imx498->reset_gpio, 0);

	ret = regulator_bulk_enable(IMX498_NUM_SUPPLIES, imx498->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(imx498->reset_gpio))
		gpiod_set_value_cansleep(imx498->reset_gpio, 1);

	/* need wait 8ms to set register */
	usleep_range(8000, 10000);

	if (!IS_ERR(imx498->pwdn_gpio))
		gpiod_set_value_cansleep(imx498->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = imx498_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(imx498->xvclk);

	return ret;
}

static void __imx498_power_off(struct imx498 *imx498)
{
	if (!IS_ERR(imx498->pwdn_gpio))
		gpiod_set_value_cansleep(imx498->pwdn_gpio, 0);
	clk_disable_unprepare(imx498->xvclk);
	if (!IS_ERR(imx498->reset_gpio))
		gpiod_set_value_cansleep(imx498->reset_gpio, 0);
	regulator_bulk_disable(IMX498_NUM_SUPPLIES, imx498->supplies);
}

static int imx498_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx498 *imx498 = to_imx498(sd);

	return __imx498_power_on(imx498);
}

static int imx498_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx498 *imx498 = to_imx498(sd);

	__imx498_power_off(imx498);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx498_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx498 *imx498 = to_imx498(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct imx498_mode *def_mode = &supported_modes[0];

	mutex_lock(&imx498->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx498->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int imx498_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx498 *imx498 = to_imx498(sd);

	if (fie->index >= imx498->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops imx498_pm_ops = {
	SET_RUNTIME_PM_OPS(imx498_runtime_suspend,
			   imx498_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx498_internal_ops = {
	.open = imx498_open,
};
#endif

static const struct v4l2_subdev_core_ops imx498_core_ops = {
	.s_power = imx498_s_power,
	.ioctl = imx498_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx498_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx498_video_ops = {
	.s_stream = imx498_s_stream,
	.g_frame_interval = imx498_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx498_pad_ops = {
	.enum_mbus_code = imx498_enum_mbus_code,
	.enum_frame_size = imx498_enum_frame_sizes,
	.enum_frame_interval = imx498_enum_frame_interval,
	.get_fmt = imx498_get_fmt,
	.set_fmt = imx498_set_fmt,
	.get_selection = imx498_get_selection,
	.get_mbus_config = imx498_g_mbus_config,
};

static const struct v4l2_subdev_ops imx498_subdev_ops = {
	.core	= &imx498_core_ops,
	.video	= &imx498_video_ops,
	.pad	= &imx498_pad_ops,
};

static int imx498_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx498 *imx498 = container_of(ctrl->handler,
					     struct imx498, ctrl_handler);
	struct i2c_client *client = imx498->client;
	s64 max;
	int ret = 0;
	u32 again = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = imx498->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(imx498->exposure,
					 imx498->exposure->minimum, max,
					 imx498->exposure->step,
					 imx498->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = imx498_write_reg(imx498->client,
				       IMX498_REG_EXPOSURE_H,
				       IMX498_REG_VALUE_08BIT,
				       IMX498_FETCH_EXP_H(ctrl->val));
		ret |= imx498_write_reg(imx498->client,
					IMX498_REG_EXPOSURE_L,
					IMX498_REG_VALUE_08BIT,
					IMX498_FETCH_EXP_L(ctrl->val));
		dev_dbg(&client->dev, "set exposure 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		/* gain_reg = 1024 - 1024 / gain_ana
		 * manual multiple 16 to add accuracy:
		 * then formula change to:
		 * gain_reg = 1024 - 1024 * 16 / (gain_ana * 16)
		 */
		if (ctrl->val > 0x200)
			ctrl->val = 0x200;
		if (ctrl->val < 0x10)
			ctrl->val = 0x10;

		again = 1024 - 1024 * 16 / ctrl->val;
		ret = imx498_write_reg(imx498->client, IMX498_REG_GAIN_H,
				       IMX498_REG_VALUE_08BIT,
				       IMX498_FETCH_AGAIN_H(again));
		ret |= imx498_write_reg(imx498->client, IMX498_REG_GAIN_L,
					IMX498_REG_VALUE_08BIT,
					IMX498_FETCH_AGAIN_L(again));

		dev_dbg(&client->dev, "set analog gain 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = imx498_write_reg(imx498->client,
				       IMX498_REG_VTS_H,
				       IMX498_REG_VALUE_08BIT,
				       (ctrl->val + imx498->cur_mode->height)
				       >> 8);
		ret |= imx498_write_reg(imx498->client,
					IMX498_REG_VTS_L,
					IMX498_REG_VALUE_08BIT,
					(ctrl->val + imx498->cur_mode->height)
					& 0xff);
		imx498->cur_vts = ctrl->val + imx498->cur_mode->height;

		dev_dbg(&client->dev, "set vblank 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->val)
			imx498->flip |= IMX498_MIRROR_BIT_MASK;
		else
			imx498->flip &= ~IMX498_MIRROR_BIT_MASK;
		dev_dbg(&client->dev, "set hflip 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		if (ctrl->val)
			imx498->flip |= IMX498_FLIP_BIT_MASK;
		else
			imx498->flip &= ~IMX498_FLIP_BIT_MASK;
		dev_dbg(&client->dev, "set vflip 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		dev_dbg(&client->dev, "set testpattern 0x%x\n",
			ctrl->val);
		ret = imx498_enable_test_pattern(imx498, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx498_ctrl_ops = {
	.s_ctrl = imx498_set_ctrl,
};

static int imx498_initialize_controls(struct imx498 *imx498)
{
	const struct imx498_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &imx498->ctrl_handler;
	mode = imx498->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &imx498->mutex;

	imx498->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);

	imx498->cur_link_freq = 0;
	imx498->cur_pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / 10 * 2 * IMX498_LANES;

	imx498->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
					       V4L2_CID_PIXEL_RATE,
					       0, imx498->cur_pixel_rate,
					       1, imx498->cur_pixel_rate);
	v4l2_ctrl_s_ctrl(imx498->link_freq,
			   imx498->cur_link_freq);

	h_blank = mode->hts_def - mode->width;
	imx498->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (imx498->hblank)
		imx498->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx498->vblank = v4l2_ctrl_new_std(handler, &imx498_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   IMX498_VTS_MAX - mode->height,
					   1, vblank_def);
	imx498->cur_vts = mode->vts_def;
	exposure_max = mode->vts_def - 4;
	imx498->exposure = v4l2_ctrl_new_std(handler, &imx498_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX498_EXPOSURE_MIN,
					     exposure_max,
					     IMX498_EXPOSURE_STEP,
					     mode->exp_def);
	imx498->anal_gain = v4l2_ctrl_new_std(handler, &imx498_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN,
					      IMX498_GAIN_MIN,
					      IMX498_GAIN_MAX,
					      IMX498_GAIN_STEP,
					      IMX498_GAIN_DEFAULT);
	imx498->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							    &imx498_ctrl_ops,
				V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(imx498_test_pattern_menu) - 1,
				0, 0, imx498_test_pattern_menu);

	imx498->h_flip = v4l2_ctrl_new_std(handler, &imx498_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);

	imx498->v_flip = v4l2_ctrl_new_std(handler, &imx498_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	imx498->flip = 0;

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx498->client->dev,
			"Failed to init controls(  %d  )\n", ret);
		goto err_free_handler;
	}

	imx498->subdev.ctrl_handler = handler;
	imx498->has_init_exp = false;
	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int imx498_check_sensor_id(struct imx498 *imx498,
				  struct i2c_client *client)
{
	struct device *dev = &imx498->client->dev;
	u16 id = 0;
	u32 reg_H = 0;
	u32 reg_L = 0;
	int ret;

	ret = imx498_read_reg(client, IMX498_REG_CHIP_ID_H,
			      IMX498_REG_VALUE_08BIT, &reg_H);
	ret |= imx498_read_reg(client, IMX498_REG_CHIP_ID_L,
			       IMX498_REG_VALUE_08BIT, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (!(reg_H == (CHIP_ID >> 8) || reg_L == (CHIP_ID & 0xff))) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected imx498 %04x sensor\n", id);
	return 0;
}

static int imx498_configure_regulators(struct imx498 *imx498)
{
	unsigned int i;

	for (i = 0; i < IMX498_NUM_SUPPLIES; i++)
		imx498->supplies[i].supply = imx498_supply_names[i];

	return devm_regulator_bulk_get(&imx498->client->dev,
				       IMX498_NUM_SUPPLIES,
				       imx498->supplies);
}

static int imx498_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx498 *imx498;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;
	struct device_node *eeprom_ctrl_node;
	struct i2c_client *eeprom_ctrl_client;
	struct v4l2_subdev *eeprom_ctrl;
	struct otp_info *otp_ptr;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	imx498 = devm_kzalloc(dev, sizeof(*imx498), GFP_KERNEL);
	if (!imx498)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx498->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx498->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx498->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx498->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	imx498->client = client;
	imx498->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < imx498->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			imx498->cur_mode = &supported_modes[i];
			break;
		}
	}

	if (i == imx498->cfg_num)
		imx498->cur_mode = &supported_modes[0];

	imx498->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx498->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx498->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx498->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	imx498->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(imx498->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = of_property_read_u32(node,
				   "rockchip,spd-id",
				   &imx498->spd_id);
	if (ret != 0) {
		imx498->spd_id = PAD_MAX;
		dev_err(dev,
			"failed get spd_id, will not to use spd\n");
	}

	ret = imx498_configure_regulators(imx498);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&imx498->mutex);

	sd = &imx498->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx498_subdev_ops);

	ret = imx498_initialize_controls(imx498);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx498_power_on(imx498);
	if (ret)
		goto err_free_handler;

	ret = imx498_check_sensor_id(imx498, client);
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
				imx498->otp = otp_ptr;
			} else {
				imx498->otp = NULL;
				devm_kfree(dev, otp_ptr);
			}
		}
	}
continue_probe:

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx498_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx498->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx498->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(imx498->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx498->module_index, facing,
		 IMX498_NAME, dev_name(sd->dev));
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
	__imx498_power_off(imx498);
err_free_handler:
	v4l2_ctrl_handler_free(&imx498->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx498->mutex);

	return ret;
}

static void imx498_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx498 *imx498 = to_imx498(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx498->ctrl_handler);
	mutex_destroy(&imx498->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx498_power_off(imx498);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx498_of_match[] = {
	{ .compatible = "sony,imx498" },
	{},
};
MODULE_DEVICE_TABLE(of, imx498_of_match);
#endif

static const struct i2c_device_id imx498_match_id[] = {
	{ "sony,imx498", 0 },
	{ },
};

static struct i2c_driver imx498_i2c_driver = {
	.driver = {
		.name = IMX498_NAME,
		.pm = &imx498_pm_ops,
		.of_match_table = of_match_ptr(imx498_of_match),
	},
	.probe		= &imx498_probe,
	.remove		= &imx498_remove,
	.id_table	= imx498_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx498_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx498_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx498 sensor driver");
MODULE_LICENSE("GPL");
