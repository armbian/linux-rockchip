// SPDX-License-Identifier: GPL-2.0
/*
 * sc501cs driver
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version.
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
#include "otp_eeprom.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC501CS_LANES			2

#define SC501CS_LINK_FREQ_450M		450000000 // 900Mbps

#define SC501CS_PIXEL_RATE_450M_10BIT	(SC501CS_LINK_FREQ_450M * 2 * SC501CS_LANES / 10)

#define SC501CS_XVCLK_FREQ		24000000

#define SC501CS_CHIP_ID			0xee45
#define SC501CS_REG_CHIP_ID		0x3107

#define SC501CS_REG_CTRL_MODE		0x0100
#define SC501CS_MODE_SW_STANDBY		0x0
#define SC501CS_MODE_STREAMING		0x1

#define SC501CS_REG_EXPOSURE_H		0x3e00
#define SC501CS_REG_EXPOSURE_M		0x3e01
#define SC501CS_REG_EXPOSURE_L		0x3e02

#define	SC501CS_EXPOSURE_MIN		3
#define	SC501CS_EXPOSURE_STEP		1

#define SC501CS_REG_DIG_GAIN		0x3e06
#define SC501CS_REG_DIG_FINE_GAIN	0x3e07
#define SC501CS_REG_ANA_GAIN		0x3e09

#define SC501CS_GAIN_MIN		0x80   //1x
#define SC501CS_GAIN_MAX		0xFC0 //31.5x
#define SC501CS_GAIN_STEP		1
#define SC501CS_GAIN_DEFAULT		0x100

#define SC501CS_REG_VTS_H		0x320e
#define SC501CS_REG_VTS_L		0x320f
#define SC501CS_VTS_MAX			0x7fff

#define SC501CS_FLIP_MIRROR_REG		0x3221
#define SC501CS_FLIP_MASK		0x60
#define SC501CS_MIRROR_MASK		0x6

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define SC501CS_REG_VALUE_08BIT		1
#define SC501CS_REG_VALUE_16BIT		2
#define SC501CS_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define RK_OTP

#define SC501CS_NAME			"sc501cs"

#define SC501CS_FETCH_EXP_H(VAL)	(((VAL) >> 12) & 0xFF)
#define SC501CS_FETCH_EXP_M(VAL)	(((VAL) >> 4) & 0xFF)
#define SC501CS_FETCH_EXP_L(VAL)	(((VAL) & 0xF) << 4)

static const char * const sc501cs_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define sc501cs_NUM_SUPPLIES ARRAY_SIZE(sc501cs_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct sc501cs_mode {
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

struct sc501cs {
	struct i2c_client	*client;
	struct clk			*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[sc501cs_NUM_SUPPLIES];

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
	struct v4l2_fract	cur_fps;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	const struct sc501cs_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	bool			has_init_exp;
	u32			cur_vts;
	bool			is_thunderboot;
	bool			is_first_streamoff;
#ifdef RK_OTP
	struct otp_info		*otp;
#endif
};

#define to_sc501cs(sd) container_of(sd, struct sc501cs, subdev)

/*
 * Xclk 24Mhz Pclk=90
 * max_framerate 30fps
 * linelength	 1500
 * framelength   2000
 * linetime		 16.667us
 * vbanking		 0.933ms
 * mipi_datarate per lane 900Mbps, 2lane
 */
static const struct regval sc501cs_linear_10_2592x1944_regs[] = {
	{0x0103, 0x01},
	{REG_DELAY, 0x0A}, //wait_ms(10)
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x36e9, 0x23},
	{0x37f9, 0x23},
	{0x301f, 0x01},
	{0x3253, 0x10},
	{0x3301, 0x12},
	{0x3306, 0x38},
	{0x330b, 0xa8},
	{0x3333, 0x10},
	{0x3364, 0x56},
	{0x3390, 0x0b},
	{0x3391, 0x0f},
	{0x3392, 0x1f},
	{0x3393, 0x20},
	{0x3394, 0x40},
	{0x3395, 0x58},
	{0x33b3, 0x40},
	{0x349f, 0x1e},
	{0x34a6, 0x09},
	{0x34a7, 0x0f},
	{0x34a8, 0x38},
	{0x34a9, 0x28},
	{0x34f8, 0x1f},
	{0x34f9, 0x28},
	{0x3630, 0xa0},
	{0x3633, 0x43},
	{0x3637, 0x45},
	{0x363c, 0xc1},
	{0x3670, 0x4a},
	{0x3674, 0xc0},
	{0x3675, 0xa8},
	{0x3676, 0xac},
	{0x367c, 0x08},
	{0x367d, 0x0b},
	{0x3690, 0x53},
	{0x3691, 0x53},
	{0x3692, 0x63},
	{0x3698, 0x85},
	{0x3699, 0x8c},
	{0x369a, 0x9b},
	{0x369b, 0xb8},
	{0x369c, 0x0f},
	{0x369d, 0x1f},
	{0x36a2, 0x09},
	{0x36a3, 0x0b},
	{0x36a4, 0x0f},
	{0x36b0, 0x4c},
	{0x36b1, 0xd8},
	{0x36b2, 0x01},
	{0x3722, 0x03},
	{0x3724, 0xa1},
	{0x3903, 0xa0},
	{0x3905, 0x4c},
	{0x391d, 0x04},
	{0x3926, 0x21},
	{0x393f, 0x80},
	{0x3940, 0x80},
	{0x3941, 0x00},
	{0x3942, 0x7f},
	{0x3943, 0x7f},
	{0x3e00, 0x00},
	{0x3e01, 0xf9},
	{0x3e02, 0x60},
	{0x4402, 0x02},
	{0x4403, 0x0a},
	{0x4404, 0x1c},
	{0x4405, 0x24},
	{0x440c, 0x2e},
	{0x440d, 0x2e},
	{0x440e, 0x22},
	{0x440f, 0x39},
	{0x4424, 0x01},
	{0x4509, 0x30},
	{0x450d, 0x18},
	{0x5000, 0x0e},
	{0x5780, 0x76},
	{0x5787, 0x08},
	{0x5788, 0x07},
	{0x5789, 0x02},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57d9, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x0c},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x5aee, 0xfe},
	{0x5aef, 0x40},
	{0x5af0, 0x00},
	{0x5af4, 0x38},
	{0x5af5, 0x30},
	{0x5af6, 0x28},
	{0x5af7, 0x38},
	{0x5af8, 0x30},
	{0x5af9, 0x28},
	{0x5afa, 0x3f},
	{0x5afb, 0x34},
	{0x5afc, 0x2c},
	{0x5afd, 0x3f},
	{0x5afe, 0x34},
	{0x5aff, 0x2c},
	//{0x0100, 0x01},
	{REG_NULL, 0x00},
};

static const struct sc501cs_mode supported_modes[] = {
	{
		.width = 2592,
		.height = 1944,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x700,
		.hts_def = 0x5DC * 2,
		.vts_def = 0x07D0,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = sc501cs_linear_10_2592x1944_regs,
		.mipi_freq_idx = 0,
		.bpp = 10,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_menu_items[] = {
	SC501CS_LINK_FREQ_450M
};

/* Write registers up to 4 at a time */
static int sc501cs_write_reg(struct i2c_client *client, u16 reg,
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

	/* Add delay after each I2C write for sensor processing */
	usleep_range(100, 200);

	return 0;
}

static int sc501cs_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		if (regs[i].addr == REG_DELAY) {
			usleep_range(regs[i].val * 1000, regs[i].val * 1000 + 500);
			dev_info(&client->dev, "write reg array, sleep %dms\n", regs[i].val);
		} else {
			ret = sc501cs_write_reg(client, regs[i].addr,
						SC501CS_REG_VALUE_08BIT, regs[i].val);
		}

	return ret;
}

/* Read registers up to 4 at a time */
static int sc501cs_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int sc501cs_get_reso_dist(const struct sc501cs_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc501cs_mode *
sc501cs_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = sc501cs_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int sc501cs_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc501cs *sc501cs = to_sc501cs(sd);
	const struct sc501cs_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&sc501cs->mutex);

	mode = sc501cs_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&sc501cs->mutex);
		return -ENOTTY;
#endif
	} else {
		sc501cs->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc501cs->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc501cs->vblank, vblank_def,
					 SC501CS_VTS_MAX - mode->height,
					 1, vblank_def);
		sc501cs->cur_fps = mode->max_fps;
	}

	mutex_unlock(&sc501cs->mutex);

	return 0;
}

static int sc501cs_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc501cs *sc501cs = to_sc501cs(sd);
	const struct sc501cs_mode *mode = sc501cs->cur_mode;

	mutex_lock(&sc501cs->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&sc501cs->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		/* format info: width/height/data type/virctual channel */
		fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&sc501cs->mutex);

	return 0;
}

static int sc501cs_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct sc501cs *sc501cs = to_sc501cs(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = sc501cs->cur_mode->bus_fmt;

	return 0;
}

static int sc501cs_enum_frame_sizes(struct v4l2_subdev *sd,
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

static int sc501cs_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc501cs *sc501cs = to_sc501cs(sd);
	const struct sc501cs_mode *mode = sc501cs->cur_mode;

	if (sc501cs->streaming)
		fi->interval = sc501cs->cur_fps;
	else
		fi->interval = mode->max_fps;

	return 0;
}

static int sc501cs_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				 struct v4l2_mbus_config *config)
{

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = SC501CS_LANES;

	return 0;
}

#ifdef RK_OTP
static void sc501cs_get_otp(struct otp_info *otp,
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

static void sc501cs_get_module_inf(struct sc501cs *sc501cs,
				   struct rkmodule_inf *inf)
{
#ifdef RK_OTP
	struct otp_info *otp = sc501cs->otp;
#endif
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC501CS_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc501cs->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc501cs->len_name, sizeof(inf->base.lens));
#ifdef RK_OTP
	if (otp)
		sc501cs_get_otp(otp, inf);
#endif
}

static void sc501cs_get_gain_reg(u32 val, u32 *again_reg, u32 *dgain_reg,
				 u32 *dgain_fine_reg)
{
	u32 total_gain;

	total_gain = val;
	if (total_gain < SC501CS_GAIN_MIN)
		total_gain = SC501CS_GAIN_MIN;
	else if (total_gain > SC501CS_GAIN_MAX)
		total_gain = SC501CS_GAIN_MAX;

	if (total_gain < 0x100) { /* 1 - 2x gain */
		*again_reg = 0x00;
		*dgain_reg = 0x00;
		*dgain_fine_reg = total_gain;
	} else if (total_gain < 0x200) { /* 2x - 4x gain */
		*again_reg = 0x08;
		*dgain_reg = 0x00;
		*dgain_fine_reg = total_gain / 2;
	} else if (total_gain < 0x400) { /* 4x - 8x gain */
		*again_reg = 0x09;
		*dgain_reg = 0x00;
		*dgain_fine_reg = total_gain / 4;
	} else if (total_gain < 0x800) { /* 8x - 16x gain */
		*again_reg = 0x0B;
		*dgain_reg = 0x00;
		*dgain_fine_reg = total_gain / 8;
	} else if (total_gain <= SC501CS_GAIN_MAX) { /* 16x - 31.5x gain */
		*again_reg = 0x0F;
		*dgain_reg = 0x00;
		*dgain_fine_reg = total_gain / 16;
	}

}

static long sc501cs_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc501cs *sc501cs = to_sc501cs(sd);

	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc501cs_get_module_inf(sc501cs, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);
		if (stream)
			ret = sc501cs_write_reg(sc501cs->client, SC501CS_REG_CTRL_MODE,
						SC501CS_REG_VALUE_08BIT, SC501CS_MODE_STREAMING);
		else
			ret = sc501cs_write_reg(sc501cs->client, SC501CS_REG_CTRL_MODE,
						SC501CS_REG_VALUE_08BIT, SC501CS_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long sc501cs_compat_ioctl32(struct v4l2_subdev *sd,
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

		ret = sc501cs_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		if (copy_from_user(&stream, up, sizeof(u32)))
			return -EFAULT;

		ret = sc501cs_ioctl(sd, cmd, &stream);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __sc501cs_start_stream(struct sc501cs *sc501cs)
{
	int ret;
	struct device *dev = &sc501cs->client->dev;

	ret = sc501cs_write_array(sc501cs->client, sc501cs->cur_mode->reg_list);
	if (ret) {
		dev_err(dev, "Failed to write mode regs, ret=%d\n", ret);
		return ret;
	}

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&sc501cs->ctrl_handler);
	if (ret) {
		dev_err(dev, "Failed to setup ctrl handler, ret=%d\n", ret);
		return ret;
	}

	ret = sc501cs_write_reg(sc501cs->client, SC501CS_REG_CTRL_MODE,
				 SC501CS_REG_VALUE_08BIT, SC501CS_MODE_STREAMING);
	if (ret)
		dev_err(dev, "Failed to write streaming reg, ret=%d\n", ret);

	return ret;
}

static int __sc501cs_stop_stream(struct sc501cs *sc501cs)
{
	sc501cs->has_init_exp = false;
	return sc501cs_write_reg(sc501cs->client, SC501CS_REG_CTRL_MODE,
				 SC501CS_REG_VALUE_08BIT, SC501CS_MODE_SW_STANDBY);
}

static int __sc501cs_power_on(struct sc501cs *sc501cs);
static int sc501cs_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc501cs *sc501cs = to_sc501cs(sd);
	struct i2c_client *client = sc501cs->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				sc501cs->cur_mode->width,
				sc501cs->cur_mode->height,
		DIV_ROUND_CLOSEST(sc501cs->cur_mode->max_fps.denominator,
				  sc501cs->cur_mode->max_fps.numerator));

	mutex_lock(&sc501cs->mutex);
	on = !!on;
	if (on == sc501cs->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __sc501cs_start_stream(sc501cs);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc501cs_stop_stream(sc501cs);
		pm_runtime_put(&client->dev);
	}

	sc501cs->streaming = on;

unlock_and_return:
	mutex_unlock(&sc501cs->mutex);

	return ret;
}

static int sc501cs_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc501cs *sc501cs = to_sc501cs(sd);
	struct i2c_client *client = sc501cs->client;
	int ret = 0;

	dev_info(&client->dev, "%s on(%d)\n", __func__, on);
	mutex_lock(&sc501cs->mutex);
	/* If the power state is not modified - no work to do. */
	if (sc501cs->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		sc501cs->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc501cs->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc501cs->mutex);

	return ret;
}

static int __sc501cs_power_on(struct sc501cs *sc501cs)
{
	int ret;
	struct device *dev = &sc501cs->client->dev;

	if (!IS_ERR_OR_NULL(sc501cs->pins_default)) {
		ret = pinctrl_select_state(sc501cs->pinctrl,
					   sc501cs->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc501cs->xvclk, SC501CS_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(sc501cs->xvclk) != SC501CS_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(sc501cs->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (!IS_ERR(sc501cs->reset_gpio))
		gpiod_set_value_cansleep(sc501cs->reset_gpio, 0);

	ret = regulator_bulk_enable(sc501cs_NUM_SUPPLIES, sc501cs->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc501cs->reset_gpio))
		gpiod_set_value_cansleep(sc501cs->reset_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR(sc501cs->reset_gpio))
		gpiod_set_value_cansleep(sc501cs->reset_gpio, 0);

	usleep_range(1000, 2000);

	if (!IS_ERR(sc501cs->reset_gpio))
		gpiod_set_value_cansleep(sc501cs->reset_gpio, 1);

	if (!IS_ERR(sc501cs->pwdn_gpio))
		gpiod_set_value_cansleep(sc501cs->pwdn_gpio, 1);

	usleep_range(10000, 12000);

	return 0;

disable_clk:
	clk_disable_unprepare(sc501cs->xvclk);

	return ret;
}

static void __sc501cs_power_off(struct sc501cs *sc501cs)
{
	int ret;
	struct device *dev = &sc501cs->client->dev;

	clk_disable_unprepare(sc501cs->xvclk);

	if (!IS_ERR(sc501cs->pwdn_gpio))
		gpiod_set_value_cansleep(sc501cs->pwdn_gpio, 0);
	if (!IS_ERR(sc501cs->reset_gpio))
		gpiod_set_value_cansleep(sc501cs->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(sc501cs->pins_sleep)) {
		ret = pinctrl_select_state(sc501cs->pinctrl,
					   sc501cs->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(sc501cs_NUM_SUPPLIES, sc501cs->supplies);
}

static int sc501cs_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc501cs *sc501cs = to_sc501cs(sd);

	return __sc501cs_power_on(sc501cs);
}

static int sc501cs_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc501cs *sc501cs = to_sc501cs(sd);

	__sc501cs_power_off(sc501cs);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc501cs_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc501cs *sc501cs = to_sc501cs(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct sc501cs_mode *def_mode = &supported_modes[0];

	mutex_lock(&sc501cs->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc501cs->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc501cs_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = NO_HDR;
	return 0;
}

static const struct dev_pm_ops sc501cs_pm_ops = {
	SET_RUNTIME_PM_OPS(sc501cs_runtime_suspend,
	sc501cs_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc501cs_internal_ops = {
	.open = sc501cs_open,
};
#endif

static const struct v4l2_subdev_core_ops sc501cs_core_ops = {
	.s_power = sc501cs_s_power,
	.ioctl = sc501cs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc501cs_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc501cs_video_ops = {
	.s_stream = sc501cs_s_stream,
	.g_frame_interval = sc501cs_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops sc501cs_pad_ops = {
	.enum_mbus_code = sc501cs_enum_mbus_code,
	.enum_frame_size = sc501cs_enum_frame_sizes,
	.enum_frame_interval = sc501cs_enum_frame_interval,
	.get_fmt = sc501cs_get_fmt,
	.set_fmt = sc501cs_set_fmt,
	.get_mbus_config = sc501cs_g_mbus_config,
};

static const struct v4l2_subdev_ops sc501cs_subdev_ops = {
	.core	= &sc501cs_core_ops,
	.video	= &sc501cs_video_ops,
	.pad	= &sc501cs_pad_ops,
};

static void sc501cs_modify_fps_info(struct sc501cs *sc501cs)
{
	const struct sc501cs_mode *mode = sc501cs->cur_mode;

	sc501cs->cur_fps.denominator = mode->max_fps.denominator * sc501cs->cur_vts /
				       mode->vts_def;
}
static int sc501cs_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc501cs *sc501cs = container_of(ctrl->handler,
					       struct sc501cs, ctrl_handler);
	struct i2c_client *client = sc501cs->client;
	s64 max;
	u32 again = 0, dgain = 0, dgain_fine = 0;
	int ret = 0;
	u32 val = 0, vts = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc501cs->cur_mode->height + ctrl->val - 10;
		__v4l2_ctrl_modify_range(sc501cs->exposure,
					 sc501cs->exposure->minimum, max,
					 sc501cs->exposure->step,
					 sc501cs->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		val = ctrl->val << 1;
		ret |= sc501cs_write_reg(sc501cs->client,
					SC501CS_REG_EXPOSURE_H,
					SC501CS_REG_VALUE_08BIT,
					SC501CS_FETCH_EXP_H(val));
		ret |= sc501cs_write_reg(sc501cs->client,
					 SC501CS_REG_EXPOSURE_M,
					 SC501CS_REG_VALUE_08BIT,
					 SC501CS_FETCH_EXP_M(val));
		ret |= sc501cs_write_reg(sc501cs->client,
					 SC501CS_REG_EXPOSURE_L,
					 SC501CS_REG_VALUE_08BIT,
					 SC501CS_FETCH_EXP_L(val));
		dev_dbg(&client->dev, "set exposure 0x%x\n", val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		sc501cs_get_gain_reg(ctrl->val, &again, &dgain, &dgain_fine);
		ret |= sc501cs_write_reg(sc501cs->client,
					SC501CS_REG_DIG_GAIN,
					SC501CS_REG_VALUE_08BIT,
					dgain);
		ret |= sc501cs_write_reg(sc501cs->client,
					 SC501CS_REG_DIG_FINE_GAIN,
					 SC501CS_REG_VALUE_08BIT,
					 dgain_fine);
		ret |= sc501cs_write_reg(sc501cs->client,
					 SC501CS_REG_ANA_GAIN,
					 SC501CS_REG_VALUE_08BIT,
					 again);
		dev_dbg(&sc501cs->client->dev,
			"total_gain:%d again 0x%x, dgain 0x%x, dgain_fine 0x%x\n",
			ctrl->val, again, dgain, dgain_fine);
		break;
	case V4L2_CID_VBLANK:
		vts = ctrl->val + sc501cs->cur_mode->height;
		ret = sc501cs_write_reg(sc501cs->client,
					SC501CS_REG_VTS_H,
					SC501CS_REG_VALUE_08BIT,
					(vts >> 8) & 0x7f);
		ret |= sc501cs_write_reg(sc501cs->client,
					 SC501CS_REG_VTS_L,
					 SC501CS_REG_VALUE_08BIT,
					 vts & 0xff);
		sc501cs->cur_vts = vts;
		sc501cs_modify_fps_info(sc501cs);
		dev_info(&client->dev, "set vblank 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc501cs_read_reg(sc501cs->client, SC501CS_FLIP_MIRROR_REG,
				       SC501CS_REG_VALUE_08BIT, &val);
		if (ctrl->val)
			val |= SC501CS_MIRROR_MASK;
		else
			val &= ~SC501CS_MIRROR_MASK;
		ret |= sc501cs_write_reg(sc501cs->client, SC501CS_FLIP_MIRROR_REG,
					 SC501CS_REG_VALUE_08BIT, val);
		dev_info(&client->dev, "set hflip 0x%x\n", ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		ret = sc501cs_read_reg(sc501cs->client,
				       SC501CS_FLIP_MIRROR_REG,
				       SC501CS_REG_VALUE_08BIT, &val);
		if (ctrl->val)
			val |= SC501CS_FLIP_MASK;
		else
			val &= ~SC501CS_FLIP_MASK;

		ret |= sc501cs_write_reg(sc501cs->client,
					 SC501CS_FLIP_MIRROR_REG,
					 SC501CS_REG_VALUE_08BIT,
					 val);
		dev_info(&client->dev, "set vflip 0x%x\n", ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sc501cs_ctrl_ops = {
	.s_ctrl = sc501cs_set_ctrl,
};

static int sc501cs_initialize_controls(struct sc501cs *sc501cs)
{
	const struct sc501cs_mode *mode;
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *ctrl;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &sc501cs->ctrl_handler;
	mode = sc501cs->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &sc501cs->mutex;
	ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, link_freq_menu_items);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, SC501CS_PIXEL_RATE_450M_10BIT, 1, SC501CS_PIXEL_RATE_450M_10BIT);
	h_blank = mode->hts_def - mode->width;

	sc501cs->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					    h_blank, h_blank, 1, h_blank);
	if (sc501cs->hblank)
		sc501cs->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	sc501cs->cur_vts = mode->vts_def;

	sc501cs->vblank = v4l2_ctrl_new_std(handler, &sc501cs_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_def,
					    SC501CS_VTS_MAX - mode->height,
					    1, vblank_def);
	exposure_max = mode->vts_def - 10;
	sc501cs->exposure = v4l2_ctrl_new_std(handler, &sc501cs_ctrl_ops,
					      V4L2_CID_EXPOSURE, SC501CS_EXPOSURE_MIN,
					      exposure_max, SC501CS_EXPOSURE_STEP,
					      mode->exp_def);

	sc501cs->anal_gain = v4l2_ctrl_new_std(handler, &sc501cs_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN, SC501CS_GAIN_MIN,
					       SC501CS_GAIN_MAX, SC501CS_GAIN_STEP,
					       SC501CS_GAIN_DEFAULT);

	v4l2_ctrl_new_std(handler, &sc501cs_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);

	v4l2_ctrl_new_std(handler, &sc501cs_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&sc501cs->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}
	sc501cs->subdev.ctrl_handler = handler;
	sc501cs->has_init_exp = false;
	sc501cs->cur_fps = mode->max_fps;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);
	return ret;
}

static int sc501cs_check_sensor_id(struct sc501cs *sc501cs,
				   struct i2c_client *client)
{
	struct device *dev = &sc501cs->client->dev;
	u32 id = 0;
	int ret;

	ret = sc501cs_read_reg(client, SC501CS_REG_CHIP_ID,
			       SC501CS_REG_VALUE_16BIT, &id);
	if (ret) {
		dev_err(dev, "Failed to read sensor id, ret=%d\n", ret);
		return -ENODEV;
	}
	if (id != SC501CS_CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), expected(%06x)\n", id, SC501CS_CHIP_ID);
		return -ENODEV;
	}

	dev_info(dev, "Detected SC%06x sensor\n", SC501CS_CHIP_ID);

	return 0;
}

static int sc501cs_configure_regulators(struct sc501cs *sc501cs)
{
	unsigned int i;

	for (i = 0; i < sc501cs_NUM_SUPPLIES; i++)
		sc501cs->supplies[i].supply = sc501cs_supply_names[i];

	return devm_regulator_bulk_get(&sc501cs->client->dev,
				       sc501cs_NUM_SUPPLIES,
				       sc501cs->supplies);
}

static int sc501cs_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sc501cs *sc501cs;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
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

	sc501cs = devm_kzalloc(dev, sizeof(*sc501cs), GFP_KERNEL);
	if (!sc501cs)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc501cs->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc501cs->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc501cs->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc501cs->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	sc501cs->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	sc501cs->client = client;
	sc501cs->cur_mode = &supported_modes[0];

	sc501cs->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc501cs->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	sc501cs->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sc501cs->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	sc501cs->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(sc501cs->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	sc501cs->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc501cs->pinctrl)) {
		sc501cs->pins_default =
			pinctrl_lookup_state(sc501cs->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc501cs->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		sc501cs->pins_sleep =
			pinctrl_lookup_state(sc501cs->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc501cs->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = sc501cs_configure_regulators(sc501cs);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc501cs->mutex);

	sd = &sc501cs->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc501cs_subdev_ops);
	ret = sc501cs_initialize_controls(sc501cs);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc501cs_power_on(sc501cs);
	if (ret)
		goto err_free_handler;

	ret = sc501cs_check_sensor_id(sc501cs, client);
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
				sc501cs->otp = otp_ptr;
			} else {
				sc501cs->otp = NULL;
				devm_kfree(dev, otp_ptr);
				dev_warn(dev, "can not get otp info, skip!\n");
			}
		}
		put_device(&eeprom_ctrl_client->dev);
	}
continue_probe:
#endif

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc501cs_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc501cs->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc501cs->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc501cs->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc501cs->module_index, facing,
		 SC501CS_NAME, dev_name(sd->dev));
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
	__sc501cs_power_off(sc501cs);
err_free_handler:
	v4l2_ctrl_handler_free(&sc501cs->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc501cs->mutex);

	return ret;
}

static void sc501cs_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc501cs *sc501cs = to_sc501cs(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc501cs->ctrl_handler);
	mutex_destroy(&sc501cs->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc501cs_power_off(sc501cs);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc501cs_of_match[] = {
	{ .compatible = "smartsens,sc501cs" },
	{},
};
MODULE_DEVICE_TABLE(of, sc501cs_of_match);
#endif

static const struct i2c_device_id sc501cs_match_id[] = {
	{ "smartsens,sc501cs", 0 },
	{ },
};

static struct i2c_driver sc501cs_i2c_driver = {
	.driver = {
		.name = SC501CS_NAME,
		.pm = &sc501cs_pm_ops,
		.of_match_table = of_match_ptr(sc501cs_of_match),
	},
	.probe		= sc501cs_probe,
	.remove		= sc501cs_remove,
	.id_table	= sc501cs_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc501cs_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc501cs_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP) && !defined(CONFIG_INITCALL_ASYNC)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens sc501cs sensor driver");
MODULE_LICENSE("GPL");
