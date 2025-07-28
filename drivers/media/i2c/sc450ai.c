// SPDX-License-Identifier: GPL-2.0
/*
 * sc450ai driver
 *
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 first version
 * V0.0X01.0X02 Increase vblank in 2688x1520@30fps linear 4lane configuration
 * V0.0X01.0X03 Add sc450ai 2lane hdr/linear configuration and 4 lane linear configuration
 * V0.0X01.0X04 Add sc450ai thunder boot support
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x04)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define SC450AI_BITS_PER_SAMPLE		10
#define SC450AI_LINK_FREQ_180		180000000
#define SC450AI_LINK_FREQ_360		360000000
#define SC450AI_LINK_FREQ_540		540000000
#define SC450AI_MAX_LINK_FREQ		SC450AI_LINK_FREQ_540

/* 2 lane */
#define PIXEL_RATE_WITH_360M_10BIT_2L	(SC450AI_LINK_FREQ_360 * 2 * \
					2 / SC450AI_BITS_PER_SAMPLE)
/* 4 lane */
#define PIXEL_RATE_WITH_180M_10BIT_4L	(SC450AI_LINK_FREQ_180 * 2 / \
					SC450AI_BITS_PER_SAMPLE * 4)
#define PIXEL_RATE_WITH_360M_10BIT_4L	(SC450AI_LINK_FREQ_360 * 2 / \
					SC450AI_BITS_PER_SAMPLE * 4)

#define SC450AI_XVCLK_FREQ		27000000

#define CHIP_ID				0xbd2f
#define SC450AI_REG_CHIP_ID		0x3107

#define SC450AI_REG_MIPI_CTRL		0x3019
#define SC450AI_MIPI_CTRL_ON		0x00
#define SC450AI_MIPI_CTRL_OFF		0xff
#define SC450AI_REG_CTRL_MODE		0x0100
#define SC450AI_MODE_SW_STANDBY		0x0
#define SC450AI_MODE_STREAMING		BIT(0)

#define SC450AI_REG_EXPOSURE_H		0x3e00
#define SC450AI_REG_EXPOSURE_M		0x3e01
#define SC450AI_REG_EXPOSURE_L		0x3e02
#define SC450AI_REG_SEXPOSURE_H		0x3e22
#define SC450AI_REG_SEXPOSURE_M		0x3e04
#define SC450AI_REG_SEXPOSURE_L		0x3e05
#define	SC450AI_EXPOSURE_MIN		1
#define	SC450AI_EXPOSURE_STEP		1
#define SC450AI_VTS_MAX			0x7fff

#define SC450AI_REG_DIG_GAIN		0x3e06
#define SC450AI_REG_DIG_FINE_GAIN	0x3e07
#define SC450AI_REG_ANA_GAIN		0x3e08
#define SC450AI_REG_ANA_FINE_GAIN	0x3e09
#define SC450AI_REG_SDIG_GAIN		0x3e10
#define SC450AI_REG_SDIG_FINE_GAIN	0x3e11
#define SC450AI_REG_SANA_GAIN		0x3e12
#define SC450AI_REG_SANA_FINE_GAIN	0x3e13
#define SC450AI_GAIN_MIN		0x40	//0x0080
#define SC450AI_GAIN_MAX		61975 //60.523*16*64	(99614)	//48.64*16*128
#define SC450AI_GAIN_STEP		1
#define SC450AI_GAIN_DEFAULT		0x40 //0x80 // Note that the benchmark is 0x40
#define SC450AI_LGAIN			0
#define SC450AI_SGAIN			1

#define SC450AI_REG_GROUP_HOLD		0x3800//0x3812
#define SC450AI_GROUP_HOLD_START	0x00
#define SC450AI_GROUP_HOLD_END		0x30 // Not used

#define SC450AI_REG_TEST_PATTERN	0x4501
#define SC450AI_TEST_PATTERN_BIT_MASK	BIT(3)

#define SC450AI_REG_VTS_H		0x320e
#define SC450AI_REG_VTS_L		0x320f

#define SC450AI_FLIP_MIRROR_REG		0x3221

#define SC450AI_FETCH_EXP_H(VAL)	(((VAL) >> 12) & 0xF)
#define SC450AI_FETCH_EXP_M(VAL)	(((VAL) >> 4) & 0xFF)
#define SC450AI_FETCH_EXP_L(VAL)	(((VAL) & 0xF) << 4)

#define SC450AI_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL | 0x06 : VAL & 0xf9)
#define SC450AI_FETCH_FLIP(VAL, ENABLE)		(ENABLE ? VAL | 0x60 : VAL & 0x9f)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define SC450AI_REG_VALUE_08BIT		1
#define SC450AI_REG_VALUE_16BIT		2
#define SC450AI_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"
#define SC450AI_NAME			"sc450ai"

static const char * const sc450ai_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define SC450AI_NUM_SUPPLIES ARRAY_SIZE(sc450ai_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct sc450ai_mode {
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

struct sc450ai {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[SC450AI_NUM_SUPPLIES];

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
	const struct sc450ai_mode *supported_modes;
	const struct sc450ai_mode *cur_mode;
	u32			cfg_num;
	u32			module_index;
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
	struct cam_sw_info *cam_sw_inf;
	struct v4l2_fwnode_endpoint bus_cfg;
	enum rkmodule_sync_mode	sync_mode;
};

#define to_sc450ai(sd) container_of(sd, struct sc450ai, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval sc450ai_global_regs_2lane[] = {
	{REG_NULL, 0x00},
};
/*
 * Xclk 27Mhz
 * max_framerate 120fps
 * mipi_datarate per lane 720Mbps, 2lane
 * binning to 1344x760
 */
static const struct regval sc450ai_linear_10_1344x760_120fps_2lane_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x3a},
	{0x3019, 0x0c},
	{0x301c, 0x78},
	{0x301f, 0x75},
	{0x302e, 0x00},
	{0x3208, 0x05},
	{0x3209, 0x40},
	{0x320a, 0x02},
	{0x320b, 0xf8},
	{0x320c, 0x03},
	{0x320d, 0xa8},
	{0x320e, 0x03},
	{0x320f, 0x0c},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3214, 0x11},
	{0x3215, 0x31},
	{0x3220, 0x01},
	{0x3223, 0xc0},
	{0x3253, 0x10},
	{0x325f, 0x44},
	{0x3274, 0x09},
	{0x3280, 0x01},
	{0x3301, 0x08},
	{0x3306, 0x24},
	{0x3309, 0x60},
	{0x330b, 0x64},
	{0x330d, 0x30},
	{0x3315, 0x00},
	{0x331f, 0x59},
	{0x335d, 0x60},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x0a},
	{0x3394, 0x10},
	{0x3395, 0x18},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0f},
	{0x339a, 0x12},
	{0x339b, 0x14},
	{0x339c, 0x18},
	{0x33af, 0x18},
	{0x360f, 0x13},
	{0x3621, 0xec},
	{0x3627, 0xa0},
	{0x3630, 0x90},
	{0x3633, 0x56},
	{0x3637, 0x1d},
	{0x3638, 0x0a},
	{0x363c, 0x0f},
	{0x363d, 0x0f},
	{0x363e, 0x08},
	{0x3670, 0x4a},
	{0x3671, 0xe0},
	{0x3672, 0xe0},
	{0x3673, 0xe0},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x8c},
	{0x367a, 0x48},
	{0x367b, 0x58},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x3699, 0x03},
	{0x369a, 0x0f},
	{0x369b, 0x1f},
	{0x369c, 0x40},
	{0x369d, 0x48},
	{0x36a2, 0x48},
	{0x36a3, 0x78},
	{0x36b0, 0x54},
	{0x36b1, 0x75},
	{0x36b2, 0x35},
	{0x36b3, 0x48},
	{0x36b4, 0x78},
	{0x36b7, 0xa0},
	{0x36b8, 0xa0},
	{0x36b9, 0x20},
	{0x36bd, 0x40},
	{0x36be, 0x48},
	{0x36d0, 0x20},
	{0x36e0, 0x08},
	{0x36e1, 0x08},
	{0x36e2, 0x12},
	{0x36e3, 0x48},
	{0x36e4, 0x78},
	{0x36fa, 0x0d},
	{0x36fb, 0xa4},
	{0x36fc, 0x00},
	{0x36fd, 0x24},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391e, 0x01},
	{0x391f, 0x11},
	{0x3933, 0x82},
	{0x3934, 0x0b},
	{0x3935, 0x02},
	{0x3936, 0x5e},
	{0x3937, 0x76},
	{0x3938, 0x78},
	{0x3939, 0x00},
	{0x393a, 0x28},
	{0x393b, 0x00},
	{0x393c, 0x1d},
	{0x3e00, 0x00},
	{0x3e01, 0x61},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e08, 0x03},
	{0x3e1b, 0x2a},
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4837, 0x16},
	{0x5000, 0x4e},
	{0x5001, 0x44},
	{0x5780, 0x76},
	{0x5784, 0x08},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x0a},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x0a},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd0},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x5900, 0x01},
	{0x5901, 0x04},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x3f},
	{0x59e3, 0x38},
	{0x59e4, 0x30},
	{0x59e5, 0x3f},
	{0x59e6, 0x38},
	{0x59e7, 0x30},
	{0x59e8, 0x3f},
	{0x59e9, 0x3c},
	{0x59ea, 0x38},
	{0x59eb, 0x3f},
	{0x59ec, 0x3c},
	{0x59ed, 0x38},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x3f},
	{0x59f5, 0x38},
	{0x59f6, 0x30},
	{0x59f7, 0x3f},
	{0x59f8, 0x38},
	{0x59f9, 0x30},
	{0x59fa, 0x3f},
	{0x59fb, 0x3c},
	{0x59fc, 0x38},
	{0x59fd, 0x3f},
	{0x59fe, 0x3c},
	{0x59ff, 0x38},
	{0x36e9, 0x44},
	{0x36f9, 0x20},
	{REG_NULL, 0x00},
};


/*
 * Xclk 27Mhz
 * max_framerate 60fps
 * mipi_datarate per lane 720Mbps, 2lane
 */
static const struct regval sc450ai_linear_10_2688x1520_30fps_2lane_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x3a},
	{0x3019, 0x0c},
	{0x301c, 0x78},
	{0x301f, 0x3c},
	{0x302d, 0xa0},
	{0x302e, 0x00},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x02},	//hts
	{0x320d, 0xee},
	{0x320e, 0x06},	//vts
	// {0x320f, 0x18},
	{0x320f, 0x38},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3220, 0x00},
	{0x3223, 0xc0},
	{0x3253, 0x10},
	{0x325f, 0x44},
	{0x3274, 0x09},
	{0x3280, 0x01},
	{0x3301, 0x07},
	{0x3306, 0x20},
	{0x3308, 0x08},
	{0x330b, 0x58},
	{0x330e, 0x18},
	{0x3315, 0x00},
	{0x335d, 0x60},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x07},
	{0x3394, 0x10},
	{0x3395, 0x18},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x10},
	{0x339a, 0x13},
	{0x339b, 0x15},
	{0x339c, 0x18},
	{0x33af, 0x18},
	{0x3400, 0x16},
	{0x360f, 0x13},
	{0x3621, 0xec},
	{0x3622, 0x00},
	{0x3625, 0x0b},
	{0x3627, 0x20},
	{0x3630, 0x90},
	{0x3633, 0x56},
	{0x3637, 0x1d},
	{0x3638, 0x12},
	{0x363c, 0x0f},
	{0x363d, 0x0f},
	{0x363e, 0x08},
	{0x3670, 0x4a},
	{0x3671, 0xe0},
	{0x3672, 0xe0},
	{0x3673, 0xe0},
	{0x3674, 0xc0},
	{0x3675, 0x87},
	{0x3676, 0x8c},
	{0x367a, 0x48},
	{0x367b, 0x58},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x22},
	{0x3691, 0x33},
	{0x3692, 0x44},
	{0x3699, 0x03},
	{0x369a, 0x0f},
	{0x369b, 0x1f},
	{0x369c, 0x40},
	{0x369d, 0x78},
	{0x36a2, 0x48},
	{0x36a3, 0x78},
	{0x36b0, 0x53},
	{0x36b1, 0x74},
	{0x36b2, 0x34},
	{0x36b3, 0x40},
	{0x36b4, 0x78},
	{0x36b7, 0xa0},
	{0x36b8, 0xa0},
	{0x36b9, 0x20},
	{0x36bd, 0x40},
	{0x36be, 0x48},
	{0x36d0, 0x20},
	{0x36e0, 0x08},
	{0x36e1, 0x08},
	{0x36e2, 0x12},
	{0x36e3, 0x48},
	{0x36e4, 0x78},
	{0x36ec, 0x43},
	{0x36fc, 0x00},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391e, 0xf1},
	{0x391f, 0x11},
	{0x3921, 0x10},
	{0x3933, 0x82},
	{0x3934, 0x30},
	{0x3935, 0x02},
	{0x3936, 0xc7},
	{0x3937, 0x76},
	{0x3938, 0x76},
	{0x3939, 0x00},
	{0x393a, 0x28},
	{0x393b, 0x00},
	{0x393c, 0x23},
	{0x3e01, 0xc2},
	{0x3e02, 0x60},
	{0x3e03, 0x0b},
	{0x3e08, 0x03},
	{0x3e1b, 0x2a},
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4837, 0x16},
	{0x5000, 0x0e},
	{0x5001, 0x44},
	{0x5780, 0x76},
	{0x5784, 0x08},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x0a},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x0a},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd0},
	{0x57aa, 0x28},
	{0x57ab, 0x00},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x3f},
	{0x59e3, 0x38},
	{0x59e4, 0x30},
	{0x59e5, 0x3f},
	{0x59e6, 0x38},
	{0x59e7, 0x30},
	{0x59e8, 0x3f},
	{0x59e9, 0x3c},
	{0x59ea, 0x38},
	{0x59eb, 0x3f},
	{0x59ec, 0x3c},
	{0x59ed, 0x38},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x3f},
	{0x59f5, 0x38},
	{0x59f6, 0x30},
	{0x59f7, 0x3f},
	{0x59f8, 0x38},
	{0x59f9, 0x30},
	{0x59fa, 0x3f},
	{0x59fb, 0x3c},
	{0x59fc, 0x38},
	{0x59fd, 0x3f},
	{0x59fe, 0x3c},
	{0x59ff, 0x38},
	{0x36e9, 0x44},
	{0x36f9, 0x20},
	{REG_NULL, 0x00},
};

static const struct regval sc450ai_hdr2_10_2688x1520_25fps_2lane_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x3a},
	{0x3019, 0x0c},
	{0x301c, 0x78},
	{0x301f, 0x3d},
	{0x302d, 0xa0},
	{0x302e, 0x00},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x03},
	{0x320d, 0x9e},
	{0x320e, 0x0c},
	{0x320f, 0x26},
	{0x3213, 0x14},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3220, 0x00},
	{0x3223, 0xc0},
	{0x3250, 0xff},
	{0x3253, 0x10},
	{0x325f, 0x44},
	{0x3274, 0x09},
	{0x3280, 0x01},
	{0x3281, 0x01},
	{0x3301, 0x08},
	{0x3306, 0x24},
	{0x3309, 0x60},
	{0x330b, 0x64},
	{0x330d, 0x30},
	{0x3314, 0x94},
	{0x3315, 0x00},
	{0x331f, 0x59},
	{0x335d, 0x60},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x0a},
	{0x3394, 0x10},
	{0x3395, 0x18},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0f},
	{0x339a, 0x12},
	{0x339b, 0x14},
	{0x339c, 0x18},
	{0x33af, 0x18},
	{0x3400, 0x16},
	{0x3410, 0x04},
	{0x360f, 0x13},
	{0x3621, 0xec},
	{0x3627, 0xa0},
	{0x3630, 0x90},
	{0x3633, 0x56},
	{0x3637, 0x1d},
	{0x3638, 0x0a},
	{0x363c, 0x0f},
	{0x363d, 0x0f},
	{0x363e, 0x08},
	{0x3670, 0x4a},
	{0x3671, 0xe0},
	{0x3672, 0xe0},
	{0x3673, 0xe0},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x8c},
	{0x367a, 0x48},
	{0x367b, 0x58},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x3699, 0x03},
	{0x369a, 0x0f},
	{0x369b, 0x1f},
	{0x369c, 0x40},
	{0x369d, 0x48},
	{0x36a2, 0x48},
	{0x36a3, 0x78},
	{0x36b0, 0x54},
	{0x36b1, 0x55},
	{0x36b2, 0x55},
	{0x36b3, 0x48},
	{0x36b4, 0x78},
	{0x36b7, 0xa0},
	{0x36b8, 0xa0},
	{0x36b9, 0x20},
	{0x36bd, 0x40},
	{0x36be, 0x48},
	{0x36d0, 0x20},
	{0x36e0, 0x08},
	{0x36e1, 0x08},
	{0x36e2, 0x12},
	{0x36e3, 0x48},
	{0x36e4, 0x78},
	{0x36ea, 0x0c},
	{0x36eb, 0x05},
	{0x36ec, 0x43},
	{0x36ed, 0x24},
	{0x36fa, 0x0a},
	{0x36fb, 0xa4},
	{0x36fc, 0x00},
	{0x36fd, 0x14},
	{0x3900, 0x07},
	{0x3902, 0xf0},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391e, 0x01},
	{0x391f, 0x11},
	{0x3921, 0x10},
	{0x3933, 0x82},
	{0x3934, 0x0b},
	{0x3935, 0x02},
	{0x3936, 0x5e},
	{0x3937, 0x76},
	{0x3938, 0x78},
	{0x3939, 0x00},
	{0x393a, 0x28},
	{0x393b, 0x00},
	{0x393c, 0x1d},
	{0x3e00, 0x01},
	{0x3e01, 0x67},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e04, 0x16},
	{0x3e05, 0x70},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e10, 0x00},
	{0x3e11, 0x80},
	{0x3e12, 0x03},
	{0x3e13, 0x40},
	{0x3e1b, 0x2a},
	{0x3e22, 0x00},
	{0x3e23, 0x00},
	{0x3e24, 0xba},
	{0x440e, 0x02},
	{0x4503, 0x60},
	{0x4509, 0x20},
	{0x4837, 0x16},
	{0x4853, 0xf8},
	{0x5000, 0x0e},
	{0x5001, 0x44},
	{0x5011, 0x80},
	{0x5780, 0x76},
	{0x5784, 0x08},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x0a},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x0a},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd0},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x3f},
	{0x59e3, 0x38},
	{0x59e4, 0x30},
	{0x59e5, 0x3f},
	{0x59e6, 0x38},
	{0x59e7, 0x30},
	{0x59e8, 0x3f},
	{0x59e9, 0x3c},
	{0x59ea, 0x38},
	{0x59eb, 0x3f},
	{0x59ec, 0x3c},
	{0x59ed, 0x38},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x3f},
	{0x59f5, 0x38},
	{0x59f6, 0x30},
	{0x59f7, 0x3f},
	{0x59f8, 0x38},
	{0x59f9, 0x30},
	{0x59fa, 0x3f},
	{0x59fb, 0x3c},
	{0x59fc, 0x38},
	{0x59fd, 0x3f},
	{0x59fe, 0x3c},
	{0x59ff, 0x38},
	{0x36e9, 0x44},
	{0x36f9, 0x44},
	{REG_NULL, 0x00},
};

static const struct regval sc450ai_global_4lane_regs[] = {
	{REG_NULL, 0x00},
};

/*
 * Xclk 27Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 720Mbps, 4lane
 */
static const struct regval sc450ai_linear_10_2688x1520_30fps_4lane_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301c, 0x78},
	{0x301f, 0x02},
	{0x302d, 0xa0},
	{0x302e, 0x00},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x04},
	{0x320d, 0x60},
	//{0x320c, 0x07},
	//{0x320d, 0x50},
	{0x320e, 0x0c},
	{0x320f, 0x30},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3220, 0x00},
	{0x3223, 0xc0},
	{0x3253, 0x10},
	{0x325f, 0x44},
	{0x3274, 0x09},
	{0x3280, 0x01},
	{0x3301, 0x08},
	{0x3306, 0x24},
	{0x3309, 0x60},
	{0x330b, 0x64},
	{0x330d, 0x30},
	{0x3315, 0x00},
	{0x331f, 0x59},
	{0x335d, 0x60},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x0a},
	{0x3394, 0x10},
	{0x3395, 0x18},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0f},
	{0x339a, 0x12},
	{0x339b, 0x14},
	{0x339c, 0x18},
	{0x33af, 0x18},
	{0x3400, 0x16},
	{0x360f, 0x13},
	{0x3621, 0xec},
	{0x3627, 0xa0},
	{0x3630, 0x90},
	{0x3633, 0x56},
	{0x3637, 0x1d},
	{0x3638, 0x0a},
	{0x363c, 0x0f},
	{0x363d, 0x0f},
	{0x363e, 0x08},
	{0x3670, 0x4a},
	{0x3671, 0xe0},
	{0x3672, 0xe0},
	{0x3673, 0xe0},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x8c},
	{0x367a, 0x48},
	{0x367b, 0x58},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x3699, 0x03},
	{0x369a, 0x0f},
	{0x369b, 0x1f},
	{0x369c, 0x40},
	{0x369d, 0x48},
	{0x36a2, 0x48},
	{0x36a3, 0x78},
	{0x36b0, 0x54},
	{0x36b1, 0x75},
	{0x36b2, 0x35},
	{0x36b3, 0x48},
	{0x36b4, 0x78},
	{0x36b7, 0xa0},
	{0x36b8, 0xa0},
	{0x36b9, 0x20},
	{0x36bd, 0x40},
	{0x36be, 0x48},
	{0x36d0, 0x20},
	{0x36e0, 0x08},
	{0x36e1, 0x08},
	{0x36e2, 0x12},
	{0x36e3, 0x48},
	{0x36e4, 0x78},
	{0x36fa, 0x0d},
	{0x36fb, 0xa4},
	{0x36fc, 0x00},
	{0x36fd, 0x24},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391e, 0x01},
	{0x391f, 0x11},
	{0x3921, 0x10},
	{0x3933, 0x82},
	{0x3934, 0x0b},
	{0x3935, 0x02},
	{0x3936, 0x5e},
	{0x3937, 0x76},
	{0x3938, 0x78},
	{0x3939, 0x00},
	{0x393a, 0x28},
	{0x393b, 0x00},
	{0x393c, 0x1d},
	{0x3e01, 0xc2},
	{0x3e02, 0x60},
	{0x3e03, 0x0b},
	{0x3e08, 0x03},
	{0x3e1b, 0x2a},
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4837, 0x16},
	{0x5000, 0x0e},
	{0x5001, 0x44},
	{0x5780, 0x76},
	{0x5784, 0x08},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x0a},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x0a},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd0},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x3f},
	{0x59e3, 0x38},
	{0x59e4, 0x30},
	{0x59e5, 0x3f},
	{0x59e6, 0x38},
	{0x59e7, 0x30},
	{0x59e8, 0x3f},
	{0x59e9, 0x3c},
	{0x59ea, 0x38},
	{0x59eb, 0x3f},
	{0x59ec, 0x3c},
	{0x59ed, 0x38},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x3f},
	{0x59f5, 0x38},
	{0x59f6, 0x30},
	{0x59f7, 0x3f},
	{0x59f8, 0x38},
	{0x59f9, 0x30},
	{0x59fa, 0x3f},
	{0x59fb, 0x3c},
	{0x59fc, 0x38},
	{0x59fd, 0x3f},
	{0x59fe, 0x3c},
	{0x59ff, 0x38},
	{0x36e9, 0x44},
	{0x36f9, 0x20},
	{REG_NULL, 0x00},
};

static const struct regval sc450ai_hdr2_10_2688x1520_30fps_4lane_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301c, 0x78},
	{0x301f, 0x03},
	{0x302d, 0xa0},
	{0x302e, 0x00},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x03},
	{0x320d, 0xa8},
	//{0x320c, 0x07},
	//{0x320d, 0x50},
	{0x320e, 0x0c},
	{0x320f, 0x30},
	{0x3213, 0x14},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3220, 0x00},
	{0x3223, 0xc0},
	{0x3250, 0xff},
	{0x3253, 0x10},
	{0x325f, 0x44},
	{0x3274, 0x09},
	{0x3280, 0x01},
	{0x3281, 0x01},
	{0x3301, 0x08},
	{0x3306, 0x24},
	{0x3309, 0x60},
	{0x330b, 0x64},
	{0x330d, 0x30},
	{0x3314, 0x94},
	{0x3315, 0x00},
	{0x331f, 0x59},
	{0x335d, 0x60},
	{0x3364, 0x56},
	{0x338f, 0x80},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x0a},
	{0x3394, 0x10},
	{0x3395, 0x18},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0f},
	{0x339a, 0x12},
	{0x339b, 0x14},
	{0x339c, 0x18},
	{0x33af, 0x18},
	{0x3400, 0x16},
	{0x3410, 0x04},
	{0x360f, 0x13},
	{0x3621, 0xec},
	{0x3627, 0xa0},
	{0x3630, 0x90},
	{0x3633, 0x56},
	{0x3637, 0x1d},
	{0x3638, 0x0a},
	{0x363c, 0x0f},
	{0x363d, 0x0f},
	{0x363e, 0x08},
	{0x3670, 0x4a},
	{0x3671, 0xe0},
	{0x3672, 0xe0},
	{0x3673, 0xe0},
	{0x3674, 0xb0},
	{0x3675, 0x88},
	{0x3676, 0x8c},
	{0x367a, 0x48},
	{0x367b, 0x58},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x43},
	{0x3692, 0x44},
	{0x3699, 0x03},
	{0x369a, 0x0f},
	{0x369b, 0x1f},
	{0x369c, 0x40},
	{0x369d, 0x48},
	{0x36a2, 0x48},
	{0x36a3, 0x78},
	{0x36b0, 0x54},
	{0x36b1, 0x55},
	{0x36b2, 0x55},
	{0x36b3, 0x48},
	{0x36b4, 0x78},
	{0x36b7, 0xa0},
	{0x36b8, 0xa0},
	{0x36b9, 0x20},
	{0x36bd, 0x40},
	{0x36be, 0x48},
	{0x36d0, 0x20},
	{0x36e0, 0x08},
	{0x36e1, 0x08},
	{0x36e2, 0x12},
	{0x36e3, 0x48},
	{0x36e4, 0x78},
	{0x36fa, 0x0d},
	{0x36fb, 0xa4},
	{0x36fc, 0x00},
	{0x36fd, 0x24},
	{0x3900, 0x07},
	{0x3902, 0xf0},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391e, 0x01},
	{0x391f, 0x11},
	{0x3921, 0x10},
	{0x3933, 0x82},
	{0x3934, 0x0b},
	{0x3935, 0x02},
	{0x3936, 0x5e},
	{0x3937, 0x76},
	{0x3938, 0x78},
	{0x3939, 0x00},
	{0x393a, 0x28},
	{0x393b, 0x00},
	{0x393c, 0x1d},
	{0x3e00, 0x01},
	{0x3e01, 0x6b},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e04, 0x16},
	{0x3e05, 0xb0},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e10, 0x00},
	{0x3e11, 0x80},
	{0x3e12, 0x03},
	{0x3e13, 0x40},
	{0x3e1b, 0x2a},
	{0x3e22, 0x00},
	{0x3e23, 0x00},
	{0x3e24, 0xba},
	{0x440e, 0x02},
	{0x4503, 0x60},
	{0x4509, 0x20},
	{0x4837, 0x16},
	{0x4853, 0xf8},
	{0x5000, 0x0e},
	{0x5001, 0x44},
	{0x5011, 0x80},
	{0x5780, 0x76},
	{0x5784, 0x08},
	{0x5785, 0x04},
	{0x5787, 0x0a},
	{0x5788, 0x0a},
	{0x5789, 0x0a},
	{0x578a, 0x0a},
	{0x578b, 0x0a},
	{0x578c, 0x0a},
	{0x578d, 0x40},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x04},
	{0x5793, 0x08},
	{0x5794, 0x04},
	{0x5795, 0x04},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57a1, 0x04},
	{0x57a8, 0xd0},
	{0x57aa, 0x2a},
	{0x57ab, 0x7f},
	{0x57ac, 0x00},
	{0x57ad, 0x00},
	{0x59e0, 0xfe},
	{0x59e1, 0x40},
	{0x59e2, 0x3f},
	{0x59e3, 0x38},
	{0x59e4, 0x30},
	{0x59e5, 0x3f},
	{0x59e6, 0x38},
	{0x59e7, 0x30},
	{0x59e8, 0x3f},
	{0x59e9, 0x3c},
	{0x59ea, 0x38},
	{0x59eb, 0x3f},
	{0x59ec, 0x3c},
	{0x59ed, 0x38},
	{0x59ee, 0xfe},
	{0x59ef, 0x40},
	{0x59f4, 0x3f},
	{0x59f5, 0x38},
	{0x59f6, 0x30},
	{0x59f7, 0x3f},
	{0x59f8, 0x38},
	{0x59f9, 0x30},
	{0x59fa, 0x3f},
	{0x59fb, 0x3c},
	{0x59fc, 0x38},
	{0x59fd, 0x3f},
	{0x59fe, 0x3c},
	{0x59ff, 0x38},
	{0x36e9, 0x44},
	{0x36f9, 0x20},
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval sc450ai_interal_sync_master_start_regs[] = {
	{0x300a, 0x24}, //bit[2]=1 fsync_oen
	{0x3032, 0xa0},////bit[7]=1 vsync_tc_en
	{REG_NULL, 0x00},
};

static __maybe_unused const struct regval sc450ai_interal_sync_slaver_start_regs[] = {
	{0x300a, 0x22},
	{0x3222, 0x01}, //Bit[0]: Slave mode en
	{0x3224, 0x92}, //fsync trigger
	{0x3230, 0x00},
	{0x3231, 0x04},
	{0x322e, 0x00},
	{0x322f, 0x02},
	{REG_NULL, 0x00},
};

static const struct sc450ai_mode supported_modes_2lane[] = {
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0080,//mark
		.hts_def = 0x2ee * 4,
		.vts_def = 0x0638,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc450ai_global_regs_2lane,
		.reg_list = sc450ai_linear_10_2688x1520_30fps_2lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 1,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 2,
	},
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.exp_def = 0x0080,//mark
		.hts_def = 0x39e * 4,
		.vts_def = 0x0c26,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc450ai_global_regs_2lane,
		.reg_list = sc450ai_hdr2_10_2688x1520_25fps_2lane_regs,
		.hdr_mode = HDR_X2,
		.mclk = 27000000,
		.link_freq_idx = 2,
		.bpp = 10,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,//L->CSI_WR0
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,//M->CSI_WR2
		.lanes = 2,
	},
	{
		.width = 1344,
		.height = 760,
		.max_fps = {
			.numerator = 10000,
			.denominator = 1200000,
		},
		.exp_def = 0x0080,
		.hts_def = 0x03a8,
		.vts_def = 0x030c,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc450ai_global_regs_2lane,
		.reg_list = sc450ai_linear_10_1344x760_120fps_2lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 1,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 2,
	},
};

static const struct sc450ai_mode supported_modes_4lane[] = {
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0080,//mark
		.hts_def = 0x2ee * 4,
		.vts_def = 0x0c30,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc450ai_global_4lane_regs,
		.reg_list = sc450ai_linear_10_2688x1520_30fps_4lane_regs,
		.hdr_mode = NO_HDR,
		.mclk = 27000000,
		.link_freq_idx = 0,
		.bpp = 10,
		.vc[PAD0] = 0,
		.lanes = 4,
	},
	{
		.width = 2688,
		.height = 1520,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0080,//mark
		.hts_def = 0x3a8 * 4,
		.vts_def = 0x0c30,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.global_reg_list = sc450ai_global_4lane_regs,
		.reg_list = sc450ai_hdr2_10_2688x1520_30fps_4lane_regs,
		.hdr_mode = HDR_X2,
		.mclk = 27000000,
		.link_freq_idx = 1,
		.bpp = 10,
		.vc[PAD0] = 1,
		.vc[PAD1] = 0,//L->CSI_WR0
		.vc[PAD2] = 1,
		.vc[PAD3] = 1,//M->CSI_WR2
		.lanes = 4,
	},
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SBGGR10_1X10,
};

static const s64 link_freq_menu_items[] = {
	SC450AI_LINK_FREQ_180,
	SC450AI_LINK_FREQ_360,
	SC450AI_LINK_FREQ_540,
};

static const char * const sc450ai_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4",
};

/* Write registers up to 4 at a time */
static int sc450ai_write_reg(struct i2c_client *client, u16 reg,
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

static int sc450ai_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = sc450ai_write_reg(client, regs[i].addr,
					SC450AI_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int sc450ai_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int sc450ai_set_gain_reg(struct sc450ai *sc450ai, u32 gain, int mode)
{
	struct i2c_client *client = sc450ai->client;
	u32 coarse_again = 0, coarse_dgain = 0, fine_again = 0, fine_dgain = 0;
	int ret = 0, gain_factor;

	if (gain < 64)
		gain = 64;
	else if (gain > SC450AI_GAIN_MAX)
		gain = SC450AI_GAIN_MAX;

	gain_factor = gain * 1000 / 64;
	if (gain_factor < 2000) {
		coarse_again = 0x03;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor * 64 / 1000;
	} else if (gain_factor < 3813) {//mark
		coarse_again = 0x07;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor * 64 / 2000;
	} else if (gain_factor < 7625) {
		coarse_again = 0x23;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor * 64 / 3813;
	} else if (gain_factor < 15250) {
		coarse_again = 0x27;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor * 64 / 7625;
	} else if (gain_factor < 30500) {
		coarse_again = 0x2f;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor * 64 / 15250;
	} else if (gain_factor <= 60523) {
		coarse_again = 0x3f;
		coarse_dgain = 0x00;
		fine_dgain = 0x80;
		fine_again = gain_factor * 64 / 30500;
	} else if (gain_factor < 60523 * 2) {
		//open dgain begin  max digital gain 4X
		coarse_again = 0x3f;
		coarse_dgain = 0x00;
		fine_again = 0x7f;
		fine_dgain = gain_factor * 128 / 60523;
	} else if (gain_factor < 60523 * 4) {
		coarse_again = 0x3f;
		coarse_dgain = 0x01;
		fine_again = 0x7f;
		fine_dgain = gain_factor * 128 / 60523 / 2;
	} else if (gain_factor < 60523 * 8) {
		coarse_again = 0x3f;
		coarse_dgain = 0x03;
		fine_again = 0x7f;
		fine_dgain = gain_factor * 128 / 60523 / 4;
	} else if (gain_factor < 60523 * 16) {
		coarse_again = 0x3f;
		coarse_dgain = 0x07;
		fine_again = 0x7f;
		fine_dgain = gain_factor * 128 / 60523 / 8;
	}
	dev_dbg(&client->dev, "c_again: 0x%x, c_dgain: 0x%x, f_again: 0x%x, f_dgain: 0x%0x\n",
		    coarse_again, coarse_dgain, fine_again, fine_dgain);

	if (mode == SC450AI_LGAIN) {
		ret = sc450ai_write_reg(sc450ai->client,
					SC450AI_REG_DIG_GAIN,
					SC450AI_REG_VALUE_08BIT,
					coarse_dgain);
		ret |= sc450ai_write_reg(sc450ai->client,
					SC450AI_REG_DIG_FINE_GAIN,
					SC450AI_REG_VALUE_08BIT,
					fine_dgain);
		ret |= sc450ai_write_reg(sc450ai->client,
					SC450AI_REG_ANA_GAIN,
					SC450AI_REG_VALUE_08BIT,
					coarse_again);
		ret |= sc450ai_write_reg(sc450ai->client,
					SC450AI_REG_ANA_FINE_GAIN,
					SC450AI_REG_VALUE_08BIT,
					fine_again);
	} else {
		ret |= sc450ai_write_reg(sc450ai->client,
					SC450AI_REG_SDIG_GAIN,
					SC450AI_REG_VALUE_08BIT,
					coarse_dgain);
		ret |= sc450ai_write_reg(sc450ai->client,
					 SC450AI_REG_SDIG_FINE_GAIN,
					 SC450AI_REG_VALUE_08BIT,
					 fine_dgain);
		ret |= sc450ai_write_reg(sc450ai->client,
					 SC450AI_REG_SANA_GAIN,
					 SC450AI_REG_VALUE_08BIT,
					 coarse_again);
		ret |= sc450ai_write_reg(sc450ai->client,
					 SC450AI_REG_SANA_FINE_GAIN,
					 SC450AI_REG_VALUE_08BIT,
					 fine_again);
	}

	return ret;
}

static int sc450ai_set_hdrae(struct sc450ai *sc450ai,
			     struct preisp_hdrae_exp_s *ae)
{
	int ret = 0;
	u32 l_exp_time, m_exp_time, s_exp_time;
	u32 l_a_gain, m_a_gain, s_a_gain;

	if (!sc450ai->has_init_exp && !sc450ai->streaming) {
		sc450ai->init_hdrae_exp = *ae;
		sc450ai->has_init_exp = true;
		dev_dbg(&sc450ai->client->dev, "sc450ai don't stream, record exp for hdr!\n");
		return ret;
	}
	l_exp_time = ae->long_exp_reg;
	m_exp_time = ae->middle_exp_reg;
	s_exp_time = ae->short_exp_reg;
	l_a_gain = ae->long_gain_reg;
	m_a_gain = ae->middle_gain_reg;
	s_a_gain = ae->short_gain_reg;

	dev_dbg(&sc450ai->client->dev,
		"rev exp req: L_exp: 0x%x, 0x%x, M_exp: 0x%x, 0x%x S_exp: 0x%x, 0x%x\n",
		l_exp_time, m_exp_time, s_exp_time,
		l_a_gain, m_a_gain, s_a_gain);

	if (sc450ai->cur_mode->hdr_mode == HDR_X2) {
		//2 stagger
		l_a_gain = m_a_gain;
		l_exp_time = m_exp_time;
	}

	//set exposure
	l_exp_time = l_exp_time * 2;
	s_exp_time = s_exp_time * 2;
	if (l_exp_time > 2 * (sc450ai->cur_vts - 0xba) - 13)
		l_exp_time = 2 * (sc450ai->cur_vts - 0xba) - 13;
	if (s_exp_time > 2 * 0xba - 11)
		s_exp_time = 2 * 0xba - 11;

	ret = sc450ai_write_reg(sc450ai->client,
				SC450AI_REG_EXPOSURE_H,
				SC450AI_REG_VALUE_08BIT,
				SC450AI_FETCH_EXP_H(l_exp_time));
	ret |= sc450ai_write_reg(sc450ai->client,
				 SC450AI_REG_EXPOSURE_M,
				 SC450AI_REG_VALUE_08BIT,
				 SC450AI_FETCH_EXP_M(l_exp_time));
	ret |= sc450ai_write_reg(sc450ai->client,
				 SC450AI_REG_EXPOSURE_L,
				 SC450AI_REG_VALUE_08BIT,
				 SC450AI_FETCH_EXP_L(l_exp_time));
	ret |= sc450ai_write_reg(sc450ai->client,
				 SC450AI_REG_SEXPOSURE_M,
				 SC450AI_REG_VALUE_08BIT,
				 SC450AI_FETCH_EXP_M(s_exp_time));
	ret |= sc450ai_write_reg(sc450ai->client,
				 SC450AI_REG_SEXPOSURE_L,
				 SC450AI_REG_VALUE_08BIT,
				 SC450AI_FETCH_EXP_L(s_exp_time));

	ret |= sc450ai_set_gain_reg(sc450ai, l_a_gain, SC450AI_LGAIN);
	ret |= sc450ai_set_gain_reg(sc450ai, s_a_gain, SC450AI_SGAIN);
	return ret;
}

static int sc450ai_get_reso_dist(const struct sc450ai_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct sc450ai_mode *
sc450ai_find_best_fit(struct sc450ai *sc450ai, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < sc450ai->cfg_num; i++) {
		dist = sc450ai_get_reso_dist(&sc450ai->supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		} else if (dist == cur_best_fit_dist &&
			   framefmt->code == sc450ai->supported_modes[i].bus_fmt) {
			cur_best_fit = i;
			break;
		}
	}

	return &sc450ai->supported_modes[cur_best_fit];
}

static int sc450ai_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);
	const struct sc450ai_mode *mode;
	s64 h_blank, vblank_def;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc450ai->bus_cfg.bus.mipi_csi2.num_data_lanes;

	mutex_lock(&sc450ai->mutex);

	mode = sc450ai_find_best_fit(sc450ai, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_state_get_format(sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&sc450ai->mutex);
		return -ENOTTY;
#endif
	} else {
		sc450ai->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc450ai->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc450ai->vblank, vblank_def,
					 SC450AI_VTS_MAX - mode->height,
					 1, vblank_def);
		dst_link_freq = mode->link_freq_idx;
		dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
						 mode->bpp * 2 * lanes;
		__v4l2_ctrl_s_ctrl_int64(sc450ai->pixel_rate,
					 dst_pixel_rate);
		__v4l2_ctrl_s_ctrl(sc450ai->link_freq,
				   dst_link_freq);
		sc450ai->cur_fps = mode->max_fps;
	}

	mutex_unlock(&sc450ai->mutex);

	return 0;
}

static int sc450ai_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);
	const struct sc450ai_mode *mode = sc450ai->cur_mode;

	mutex_lock(&sc450ai->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_state_get_format(sd_state, fmt->pad);
#else
		mutex_unlock(&sc450ai->mutex);
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
	mutex_unlock(&sc450ai->mutex);

	return 0;
}

static int sc450ai_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int sc450ai_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);

	if (fse->index >= sc450ai->cfg_num)
		return -EINVAL;

	if (fse->code != sc450ai->supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width  = sc450ai->supported_modes[fse->index].width;
	fse->max_width  = sc450ai->supported_modes[fse->index].width;
	fse->max_height = sc450ai->supported_modes[fse->index].height;
	fse->min_height = sc450ai->supported_modes[fse->index].height;

	return 0;
}

static int sc450ai_enable_test_pattern(struct sc450ai *sc450ai, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc450ai_read_reg(sc450ai->client, SC450AI_REG_TEST_PATTERN,
			       SC450AI_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC450AI_TEST_PATTERN_BIT_MASK;
	else
		val &= ~SC450AI_TEST_PATTERN_BIT_MASK;

	ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_TEST_PATTERN,
				 SC450AI_REG_VALUE_08BIT, val);
	return ret;
}

static int sc450ai_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);
	const struct sc450ai_mode *mode = sc450ai->cur_mode;

	if (sc450ai->streaming)
		fi->interval = sc450ai->cur_fps;
	else
		fi->interval = mode->max_fps;
	return 0;
}

static const struct sc450ai_mode *sc450ai_find_mode(struct sc450ai *sc450ai, int fps)
{
	const struct sc450ai_mode *mode = NULL;
	const struct sc450ai_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < sc450ai->cfg_num; i++) {
		mode = &sc450ai->supported_modes[i];
		if (mode->width == sc450ai->cur_mode->width &&
		    mode->height == sc450ai->cur_mode->height &&
		    mode->hdr_mode == sc450ai->cur_mode->hdr_mode &&
		    mode->bus_fmt == sc450ai->cur_mode->bus_fmt) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator, mode->max_fps.numerator);
			if (cur_fps == fps) {
				match = mode;
				break;
			}
		}
	}
	return match;
}

static int sc450ai_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);
	const struct sc450ai_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	int fps;

	if (sc450ai->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = sc450ai_find_mode(sc450ai, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	sc450ai->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(sc450ai->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(sc450ai->vblank, vblank_def,
				 SC450AI_VTS_MAX - mode->height,
				 1, vblank_def);
	pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
		     mode->bpp * 2 * mode->lanes;

	__v4l2_ctrl_s_ctrl_int64(sc450ai->pixel_rate,
				 pixel_rate);
	__v4l2_ctrl_s_ctrl(sc450ai->link_freq,
			   mode->link_freq_idx);
	sc450ai->cur_fps = mode->max_fps;

	return 0;
}

static int sc450ai_g_mbus_config(struct v4l2_subdev *sd,
				unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = sc450ai->cur_mode->lanes;

	return 0;
}

static void sc450ai_get_module_inf(struct sc450ai *sc450ai,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC450AI_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc450ai->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc450ai->len_name, sizeof(inf->base.lens));
}

static int sc450ai_set_setting(struct sc450ai *sc450ai, struct rk_sensor_setting *setting)
{
	int i = 0;
	int cur_fps = 0;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	const struct sc450ai_mode *mode = NULL;
	const struct sc450ai_mode *match = NULL;
	u8 lane = sc450ai->bus_cfg.bus.mipi_csi2.num_data_lanes;

	dev_info(&sc450ai->client->dev,
		"sensor setting: %d x %d, fps:%d fmt:%d, mode:%d\n",
		setting->width, setting->height,
		setting->fps, setting->fmt, setting->mode);

	for (i = 0; i < sc450ai->cfg_num; i++) {
		mode = &sc450ai->supported_modes[i];
		if (mode->width == setting->width &&
		    mode->height == setting->height &&
		    mode->hdr_mode == setting->mode &&
		    mode->bus_fmt == setting->fmt) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator, mode->max_fps.numerator);
			if (cur_fps == setting->fps) {
				match = mode;
				break;
			}
		}
	}

	if (match) {
		dev_info(&sc450ai->client->dev, "-----%s: match the support mode, mode idx:%d-----\n",
			__func__, i);
		sc450ai->cur_mode = mode;

		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc450ai->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc450ai->vblank, vblank_def,
					 SC450AI_VTS_MAX - mode->height,
					 1, vblank_def);


		__v4l2_ctrl_s_ctrl(sc450ai->link_freq, mode->link_freq_idx);
		pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
			     mode->bpp * 2 * lane;
		__v4l2_ctrl_s_ctrl_int64(sc450ai->pixel_rate, pixel_rate);
		dev_info(&sc450ai->client->dev, "freq_idx:%d pixel_rate:%lld\n",
			mode->link_freq_idx, pixel_rate);

		sc450ai->cur_vts = mode->vts_def;
		sc450ai->cur_fps = mode->max_fps;

		dev_info(&sc450ai->client->dev, "hts_def:%d cur_vts:%d cur_fps:%d\n",
			mode->hts_def, mode->vts_def,
			sc450ai->cur_fps.denominator / sc450ai->cur_fps.numerator);
	} else {
		dev_err(&sc450ai->client->dev, "couldn't match the support modes\n");
		return -EINVAL;
	}

	return 0;
}

static long sc450ai_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rk_sensor_setting *setting;
	u32 i, h, w;
	long ret = 0;
	u32 stream = 0;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;
	u8 lanes = sc450ai->bus_cfg.bus.mipi_csi2.num_data_lanes;
	const struct sc450ai_mode *mode;
	int cur_best_fit = -1;
	int cur_best_fit_dist = -1;
	int cur_dist, cur_fps, dst_fps;
	u32 *sync_mode = NULL;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc450ai_get_module_inf(sc450ai, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = sc450ai->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		if (hdr->hdr_mode == sc450ai->cur_mode->hdr_mode)
			return 0;
		w = sc450ai->cur_mode->width;
		h = sc450ai->cur_mode->height;
		dst_fps = DIV_ROUND_CLOSEST(sc450ai->cur_mode->max_fps.denominator,
			sc450ai->cur_mode->max_fps.numerator);
		for (i = 0; i < sc450ai->cfg_num; i++) {
			if (w == sc450ai->supported_modes[i].width &&
			    h == sc450ai->supported_modes[i].height &&
			    sc450ai->supported_modes[i].hdr_mode == hdr->hdr_mode &&
			    sc450ai->supported_modes[i].bus_fmt == sc450ai->cur_mode->bus_fmt) {
				cur_fps = DIV_ROUND_CLOSEST(sc450ai->supported_modes[i].max_fps.denominator,
					sc450ai->supported_modes[i].max_fps.numerator);
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
			dev_err(&sc450ai->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			sc450ai->cur_mode = &sc450ai->supported_modes[cur_best_fit];
			mode = sc450ai->cur_mode;
			w = mode->hts_def - mode->width;
			h = mode->vts_def - mode->height;
			__v4l2_ctrl_modify_range(sc450ai->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(sc450ai->vblank, h,
						 SC450AI_VTS_MAX - sc450ai->cur_mode->height, 1, h);
			sc450ai->cur_fps = sc450ai->cur_mode->max_fps;

			dst_link_freq = mode->link_freq_idx;
			dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
						mode->bpp * 2 * lanes;
			__v4l2_ctrl_s_ctrl_int64(sc450ai->pixel_rate,
						 dst_pixel_rate);
			__v4l2_ctrl_s_ctrl(sc450ai->link_freq,
					   dst_link_freq);
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		sc450ai_set_hdrae(sc450ai, arg);
		if (sc450ai->cam_sw_inf)
			memcpy(&sc450ai->cam_sw_inf->hdr_ae, (struct preisp_hdrae_exp_s *)(arg),
				sizeof(struct preisp_hdrae_exp_s));
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);


		if (sc450ai->standby_hw) {	/* hardware standby */
			if (stream) {
				u32 val;

				/* pwdn gpio pull up */
				if (!IS_ERR(sc450ai->pwdn_gpio))
					gpiod_set_value_cansleep(sc450ai->pwdn_gpio, 1);
				// Make sure __v4l2_ctrl_handler_setup can be called correctly
				sc450ai->is_standby = false;
				/* mipi clk on */
				ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_MIPI_CTRL,
							 SC450AI_REG_VALUE_08BIT,
							 SC450AI_MIPI_CTRL_ON);
				/* adjust timing */
				ret |= sc450ai_read_reg(sc450ai->client, 0x36e9,
							SC450AI_REG_VALUE_08BIT, &val);
				val &= 0x7f;
				ret |= sc450ai_write_reg(sc450ai->client, 0x36e9,
							 SC450AI_REG_VALUE_08BIT,
							 val);
				ret |= sc450ai_read_reg(sc450ai->client, 0x36f9,
							SC450AI_REG_VALUE_08BIT, &val);
				val &= 0x7f;
				ret |= sc450ai_write_reg(sc450ai->client, 0x36f9,
							 SC450AI_REG_VALUE_08BIT,
							 val);

				#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
				if (__v4l2_ctrl_handler_setup(&sc450ai->ctrl_handler))
					dev_err(&sc450ai->client->dev, "__v4l2_ctrl_handler_setup fail!");
				if (sc450ai->cur_mode->hdr_mode != NO_HDR) {	// hdr mode
					if (sc450ai->cam_sw_inf) {
						ret = sc450ai_ioctl(&sc450ai->subdev,
								    PREISP_CMD_SET_HDRAE_EXP,
								    &sc450ai->cam_sw_inf->hdr_ae);
						if (ret) {
							dev_err(&sc450ai->client->dev,
								"init exp fail in hdr mode\n");
							return ret;
						}
					}
				}
				#endif

				/* stream on */
				ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_CTRL_MODE,
							 SC450AI_REG_VALUE_08BIT,
							 SC450AI_MODE_STREAMING);
				dev_info(&sc450ai->client->dev,
					"quickstream, streaming on: exit hw standby mode\n");
			} else {
				u32 val;

				/* adjust timing */
				ret |= sc450ai_read_reg(sc450ai->client, 0x36e9,
							SC450AI_REG_VALUE_08BIT, &val);
				val |= 0x80;
				ret |= sc450ai_write_reg(sc450ai->client, 0x36e9,
							 SC450AI_REG_VALUE_08BIT,
							 val);
				ret |= sc450ai_read_reg(sc450ai->client, 0x36f9,
							SC450AI_REG_VALUE_08BIT, &val);
				val |= 0x80;
				ret |= sc450ai_write_reg(sc450ai->client, 0x36f9,
							 SC450AI_REG_VALUE_08BIT,
							 val);
				/* stream off */
				ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_CTRL_MODE,
							 SC450AI_REG_VALUE_08BIT,
							 SC450AI_MODE_SW_STANDBY);
				/* mipi clk off */
				ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_MIPI_CTRL,
							 SC450AI_REG_VALUE_08BIT,
							 SC450AI_MIPI_CTRL_OFF);
				/* pwnd gpio pull down */
				if (!IS_ERR(sc450ai->pwdn_gpio))
					gpiod_set_value_cansleep(sc450ai->pwdn_gpio, 0);
				dev_info(&sc450ai->client->dev,
					"quickstream, streaming off: enter hw standby mode\n");
				sc450ai->is_standby = true;
			}
		} else {	/* software standby */
			if (stream) {
				ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_MIPI_CTRL,
							 SC450AI_REG_VALUE_08BIT,
							 SC450AI_MIPI_CTRL_ON);
				ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_CTRL_MODE,
							 SC450AI_REG_VALUE_08BIT,
							 SC450AI_MODE_STREAMING);
				dev_info(&sc450ai->client->dev,
					"quickstream, streaming on: exit soft standby mode\n");
			} else {
				ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_CTRL_MODE,
							SC450AI_REG_VALUE_08BIT,
							SC450AI_MODE_SW_STANDBY);
				ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_MIPI_CTRL,
							 SC450AI_REG_VALUE_08BIT,
							 SC450AI_MIPI_CTRL_OFF);
				dev_info(&sc450ai->client->dev,
					"quickstream, streaming off: enter soft standby mode\n");
			}
		}
		break;
	case RKCIS_CMD_SELECT_SETTING:
		setting = (struct rk_sensor_setting *)arg;
		ret = sc450ai_set_setting(sc450ai, setting);
		break;
	case RKMODULE_GET_SYNC_MODE:
		sync_mode = (u32 *)arg;
		*sync_mode = sc450ai->sync_mode;
		break;
	case RKMODULE_SET_SYNC_MODE:
		sync_mode = (u32 *)arg;
		sc450ai->sync_mode = *sync_mode;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long sc450ai_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rk_sensor_setting *setting;
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

		ret = sc450ai_ioctl(sd, cmd, inf);
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

		ret = sc450ai_ioctl(sd, cmd, hdr);
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
			ret = sc450ai_ioctl(sd, cmd, hdr);
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
			ret = sc450ai_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = sc450ai_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKCIS_CMD_SELECT_SETTING:
		setting = kzalloc(sizeof(*setting), GFP_KERNEL);
		if (!setting) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(setting, up, sizeof(*setting));
		if (!ret)
			ret = sc450ai_ioctl(sd, cmd, setting);
		else
			ret = -EFAULT;
		kfree(setting);
		break;
	case RKMODULE_GET_SYNC_MODE:
		ret = sc450ai_ioctl(sd, cmd, &sync_mode);
		if (!ret) {
			ret = copy_to_user(up, &sync_mode, sizeof(u32));
			if (ret)
				ret = -EFAULT;
		}
		break;
	case RKMODULE_SET_SYNC_MODE:
		ret = copy_from_user(&sync_mode, up, sizeof(u32));
		if (!ret)
			ret = sc450ai_ioctl(sd, cmd, &sync_mode);
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

static int __sc450ai_start_stream(struct sc450ai *sc450ai)
{
	int ret = 0;

	if (!sc450ai->is_thunderboot) {
		ret = sc450ai_write_array(sc450ai->client, sc450ai->cur_mode->reg_list);
		if (ret)
			return ret;
		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&sc450ai->ctrl_handler);
		if (ret)
			return ret;
		if (sc450ai->has_init_exp && sc450ai->cur_mode->hdr_mode != NO_HDR) {
			ret = sc450ai_ioctl(&sc450ai->subdev, PREISP_CMD_SET_HDRAE_EXP,
				&sc450ai->init_hdrae_exp);
			if (ret) {
				dev_err(&sc450ai->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	if (sc450ai->sync_mode == INTERNAL_MASTER_MODE)
		ret |= sc450ai_write_array(sc450ai->client,
			sc450ai_interal_sync_master_start_regs);
	else if (sc450ai->sync_mode == EXTERNAL_MASTER_MODE)
		ret |= sc450ai_write_array(sc450ai->client,
			sc450ai_interal_sync_slaver_start_regs);
	else if (sc450ai->sync_mode == NO_SYNC_MODE)
		ret |= sc450ai_write_reg(sc450ai->client, SC450AI_REG_CTRL_MODE,
			SC450AI_REG_VALUE_08BIT, SC450AI_MODE_STREAMING);
	return ret;
}

static int __sc450ai_stop_stream(struct sc450ai *sc450ai)
{
	sc450ai->has_init_exp = false;
	if (sc450ai->is_thunderboot)
		sc450ai->is_first_streamoff = true;
	return sc450ai_write_reg(sc450ai->client, SC450AI_REG_CTRL_MODE,
				 SC450AI_REG_VALUE_08BIT, SC450AI_MODE_SW_STANDBY);
}

static int __sc450ai_power_on(struct sc450ai *sc450ai);
static int sc450ai_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);
	struct i2c_client *client = sc450ai->client;
	int ret = 0;

	mutex_lock(&sc450ai->mutex);
	on = !!on;
	if (on == sc450ai->streaming)
		goto unlock_and_return;
	if (on) {
		if (sc450ai->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			sc450ai->is_thunderboot = false;
			__sc450ai_power_on(sc450ai);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __sc450ai_start_stream(sc450ai);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc450ai_stop_stream(sc450ai);
		pm_runtime_put(&client->dev);
	}

	sc450ai->streaming = on;
unlock_and_return:
	mutex_unlock(&sc450ai->mutex);
	return ret;
}

static int sc450ai_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);
	struct i2c_client *client = sc450ai->client;
	int ret = 0;

	mutex_lock(&sc450ai->mutex);

	/* If the power state is not modified - no work to do. */
	if (sc450ai->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!sc450ai->is_thunderboot) {
			ret = sc450ai_write_array(sc450ai->client,
				sc450ai->cur_mode->global_reg_list);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		sc450ai->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc450ai->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc450ai->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 sc450ai_cal_delay(u32 cycles, struct sc450ai *sc450ai)
{
	return DIV_ROUND_UP(cycles, sc450ai->cur_mode->mclk / 1000 / 1000);
}

static int __sc450ai_power_on(struct sc450ai *sc450ai)
{
	int ret;
	u32 delay_us;
	struct device *dev = &sc450ai->client->dev;

	if (!IS_ERR_OR_NULL(sc450ai->pins_default)) {
		ret = pinctrl_select_state(sc450ai->pinctrl,
					   sc450ai->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc450ai->xvclk, sc450ai->cur_mode->mclk);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (%dHz)\n", sc450ai->cur_mode->mclk);
	if (clk_get_rate(sc450ai->xvclk) != sc450ai->cur_mode->mclk)
		dev_warn(dev, "xvclk mismatched, modes are based on %dHz\n",
			 sc450ai->cur_mode->mclk);
	ret = clk_prepare_enable(sc450ai->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	cam_sw_regulator_bulk_init(sc450ai->cam_sw_inf, SC450AI_NUM_SUPPLIES, sc450ai->supplies);

	if (sc450ai->is_thunderboot)
		return 0;

	if (!IS_ERR(sc450ai->reset_gpio))
		gpiod_set_value_cansleep(sc450ai->reset_gpio, 0);

	ret = regulator_bulk_enable(SC450AI_NUM_SUPPLIES, sc450ai->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc450ai->reset_gpio))
		gpiod_set_value_cansleep(sc450ai->reset_gpio, 1);

	usleep_range(500, 1000);

	if (!IS_ERR(sc450ai->pwdn_gpio))
		gpiod_set_value_cansleep(sc450ai->pwdn_gpio, 1);

	if (!IS_ERR(sc450ai->reset_gpio))
		usleep_range(6000, 8000);
	else
		usleep_range(12000, 16000);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = sc450ai_cal_delay(8192, sc450ai);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(sc450ai->xvclk);

	return ret;
}

static void __sc450ai_power_off(struct sc450ai *sc450ai)
{
	int ret;
	struct device *dev = &sc450ai->client->dev;

	clk_disable_unprepare(sc450ai->xvclk);
	if (sc450ai->is_thunderboot) {
		if (sc450ai->is_first_streamoff) {
			sc450ai->is_thunderboot = false;
			sc450ai->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(sc450ai->pwdn_gpio))
		gpiod_set_value_cansleep(sc450ai->pwdn_gpio, 0);
	if (!IS_ERR(sc450ai->reset_gpio))
		gpiod_set_value_cansleep(sc450ai->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(sc450ai->pins_sleep)) {
		ret = pinctrl_select_state(sc450ai->pinctrl,
					   sc450ai->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(SC450AI_NUM_SUPPLIES, sc450ai->supplies);
}

#if IS_REACHABLE(CONFIG_VIDEO_CAM_SLEEP_WAKEUP)
static int __maybe_unused sc450ai_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc450ai *sc450ai = to_sc450ai(sd);

	if (sc450ai->standby_hw) {
		dev_info(dev, "resume standby!");
		return 0;
	}

	cam_sw_prepare_wakeup(sc450ai->cam_sw_inf, dev);

	usleep_range(4000, 5000);
	cam_sw_write_array(sc450ai->cam_sw_inf);

	if (__v4l2_ctrl_handler_setup(&sc450ai->ctrl_handler))
		dev_err(dev, "__v4l2_ctrl_handler_setup fail!");

	if (sc450ai->has_init_exp && sc450ai->cur_mode != NO_HDR) {	// hdr mode
		ret = sc450ai_ioctl(&sc450ai->subdev, PREISP_CMD_SET_HDRAE_EXP,
				&sc450ai->cam_sw_inf->hdr_ae);
		if (ret) {
			dev_err(&sc450ai->client->dev, "set exp fail in hdr mode\n");
			return ret;
		}
	}

	return 0;
}

static int __maybe_unused sc450ai_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc450ai *sc450ai = to_sc450ai(sd);

	if (sc450ai->standby_hw) {
		dev_info(dev, "suspend standby!");
		return 0;
	}

	cam_sw_write_array_cb_init(sc450ai->cam_sw_inf, client,
				   (void *)sc450ai->cur_mode->reg_list,
				   (sensor_write_array)sc450ai_write_array);
	cam_sw_prepare_sleep(sc450ai->cam_sw_inf);

	return 0;
}
#else
#define sc450ai_resume NULL
#define sc450ai_suspend NULL
#endif

static int __maybe_unused sc450ai_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc450ai *sc450ai = to_sc450ai(sd);

	return __sc450ai_power_on(sc450ai);
}

static int __maybe_unused sc450ai_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc450ai *sc450ai = to_sc450ai(sd);

	__sc450ai_power_off(sc450ai);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int sc450ai_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_state_get_format(fh->state, 0);
	const struct sc450ai_mode *def_mode = &sc450ai->supported_modes[0];

	mutex_lock(&sc450ai->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc450ai->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sc450ai_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct sc450ai *sc450ai = to_sc450ai(sd);

	if (fie->index >= sc450ai->cfg_num)
		return -EINVAL;

	fie->code = sc450ai->supported_modes[fie->index].bus_fmt;
	fie->width = sc450ai->supported_modes[fie->index].width;
	fie->height = sc450ai->supported_modes[fie->index].height;
	fie->interval = sc450ai->supported_modes[fie->index].max_fps;
	fie->reserved[0] = sc450ai->supported_modes[fie->index].hdr_mode;

	return 0;
}

static const struct dev_pm_ops sc450ai_pm_ops = {
	SET_RUNTIME_PM_OPS(sc450ai_runtime_suspend,
			   sc450ai_runtime_resume, NULL)
#ifdef CONFIG_VIDEO_CAM_SLEEP_WAKEUP
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sc450ai_suspend, sc450ai_resume)
#endif
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc450ai_internal_ops = {
	.open = sc450ai_open,
};
#endif

static const struct v4l2_subdev_core_ops sc450ai_core_ops = {
	.s_power = sc450ai_s_power,
	.ioctl = sc450ai_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc450ai_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sc450ai_video_ops = {
	.s_stream = sc450ai_s_stream,
};

static const struct v4l2_subdev_pad_ops sc450ai_pad_ops = {
	.enum_mbus_code = sc450ai_enum_mbus_code,
	.enum_frame_size = sc450ai_enum_frame_sizes,
	.enum_frame_interval = sc450ai_enum_frame_interval,
	.get_fmt = sc450ai_get_fmt,
	.set_fmt = sc450ai_set_fmt,
	.get_mbus_config = sc450ai_g_mbus_config,
	.get_frame_interval = sc450ai_g_frame_interval,
	.set_frame_interval = sc450ai_s_frame_interval,
};

static const struct v4l2_subdev_ops sc450ai_subdev_ops = {
	.core	= &sc450ai_core_ops,
	.video	= &sc450ai_video_ops,
	.pad	= &sc450ai_pad_ops,
};

static void sc450ai_modify_fps_info(struct sc450ai *sc450ai)
{
	const struct sc450ai_mode *mode = sc450ai->cur_mode;

	sc450ai->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				      sc450ai->cur_vts;
}

static int sc450ai_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc450ai *sc450ai = container_of(ctrl->handler,
					       struct sc450ai, ctrl_handler);
	struct i2c_client *client = sc450ai->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc450ai->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(sc450ai->exposure,
					 sc450ai->exposure->minimum, max,
					 sc450ai->exposure->step,
					 sc450ai->exposure->default_value);
		break;
	}

	if (sc450ai->standby_hw && sc450ai->is_standby) {
		dev_dbg(&client->dev, "%s: is_standby = true, will return\n", __func__);
		return 0;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		if (sc450ai->cur_mode->hdr_mode == NO_HDR) {
			val = ctrl->val << 1;
			/* 4 least significant bits of expsoure are fractional part */
			ret = sc450ai_write_reg(sc450ai->client,
						SC450AI_REG_EXPOSURE_H,
						SC450AI_REG_VALUE_08BIT,
						SC450AI_FETCH_EXP_H(val));
			ret |= sc450ai_write_reg(sc450ai->client,
						 SC450AI_REG_EXPOSURE_M,
						 SC450AI_REG_VALUE_08BIT,
						 SC450AI_FETCH_EXP_M(val));
			ret |= sc450ai_write_reg(sc450ai->client,
						 SC450AI_REG_EXPOSURE_L,
						 SC450AI_REG_VALUE_08BIT,
						 SC450AI_FETCH_EXP_L(val));
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set gain 0x%x\n", ctrl->val);
		if (sc450ai->cur_mode->hdr_mode == NO_HDR)
			ret = sc450ai_set_gain_reg(sc450ai, ctrl->val, SC450AI_LGAIN);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		ret = sc450ai_write_reg(sc450ai->client,
					SC450AI_REG_VTS_H,
					SC450AI_REG_VALUE_08BIT,
					(ctrl->val + sc450ai->cur_mode->height)
					>> 8);
		ret |= sc450ai_write_reg(sc450ai->client,
					 SC450AI_REG_VTS_L,
					 SC450AI_REG_VALUE_08BIT,
					 (ctrl->val + sc450ai->cur_mode->height)
					 & 0xff);
		sc450ai->cur_vts = ctrl->val + sc450ai->cur_mode->height;
		if (sc450ai->cur_vts != sc450ai->cur_mode->vts_def)
			sc450ai_modify_fps_info(sc450ai);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc450ai_enable_test_pattern(sc450ai, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc450ai_read_reg(sc450ai->client, SC450AI_FLIP_MIRROR_REG,
				       SC450AI_REG_VALUE_08BIT, &val);
		ret |= sc450ai_write_reg(sc450ai->client, SC450AI_FLIP_MIRROR_REG,
					 SC450AI_REG_VALUE_08BIT,
					 SC450AI_FETCH_MIRROR(val, ctrl->val));
		break;
	case V4L2_CID_VFLIP:
		ret = sc450ai_read_reg(sc450ai->client, SC450AI_FLIP_MIRROR_REG,
				       SC450AI_REG_VALUE_08BIT, &val);
		ret |= sc450ai_write_reg(sc450ai->client, SC450AI_FLIP_MIRROR_REG,
					 SC450AI_REG_VALUE_08BIT,
					 SC450AI_FETCH_FLIP(val, ctrl->val));
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sc450ai_ctrl_ops = {
	.s_ctrl = sc450ai_set_ctrl,
};

static int sc450ai_initialize_controls(struct sc450ai *sc450ai)
{
	const struct sc450ai_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0, max_dst_pixel_rate = 0;
	u8 lanes = sc450ai->bus_cfg.bus.mipi_csi2.num_data_lanes;

	handler = &sc450ai->ctrl_handler;
	mode = sc450ai->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc450ai->mutex;

	sc450ai->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			V4L2_CID_LINK_FREQ,
			ARRAY_SIZE(link_freq_menu_items) - 1, 0, link_freq_menu_items);
	if (sc450ai->link_freq)
		sc450ai->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	dst_link_freq = mode->link_freq_idx;
	max_dst_pixel_rate = SC450AI_MAX_LINK_FREQ / mode->bpp * 2 * lanes;
	dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
					 mode->bpp * 2 * lanes;
	sc450ai->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, max_dst_pixel_rate, 1, dst_pixel_rate);

	__v4l2_ctrl_s_ctrl(sc450ai->link_freq, dst_link_freq);


	h_blank = mode->hts_def - mode->width;
	sc450ai->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					    h_blank, h_blank, 1, h_blank);
	if (sc450ai->hblank)
		sc450ai->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	sc450ai->vblank = v4l2_ctrl_new_std(handler, &sc450ai_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_def,
					    SC450AI_VTS_MAX - mode->height,
					    1, vblank_def);
	exposure_max = mode->vts_def - 4;
	sc450ai->exposure = v4l2_ctrl_new_std(handler, &sc450ai_ctrl_ops,
					      V4L2_CID_EXPOSURE, SC450AI_EXPOSURE_MIN,
					      exposure_max, SC450AI_EXPOSURE_STEP,
					      mode->exp_def);
	sc450ai->anal_gain = v4l2_ctrl_new_std(handler, &sc450ai_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN, SC450AI_GAIN_MIN,
					       SC450AI_GAIN_MAX, SC450AI_GAIN_STEP,
					       SC450AI_GAIN_DEFAULT);
	sc450ai->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							    &sc450ai_ctrl_ops,
					V4L2_CID_TEST_PATTERN,
					ARRAY_SIZE(sc450ai_test_pattern_menu) - 1,
					0, 0, sc450ai_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &sc450ai_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &sc450ai_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&sc450ai->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc450ai->subdev.ctrl_handler = handler;
	sc450ai->has_init_exp = false;
	sc450ai->cur_fps = mode->max_fps;
	sc450ai->is_standby = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int sc450ai_check_sensor_id(struct sc450ai *sc450ai,
				   struct i2c_client *client)
{
	struct device *dev = &sc450ai->client->dev;
	u32 id = 0;
	int ret;

	if (sc450ai->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = sc450ai_read_reg(client, SC450AI_REG_CHIP_ID,
			       SC450AI_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected SC450AI (0x%04x) sensor\n", CHIP_ID);

	return 0;
}

static int sc450ai_configure_regulators(struct sc450ai *sc450ai)
{
	unsigned int i;

	for (i = 0; i < SC450AI_NUM_SUPPLIES; i++)
		sc450ai->supplies[i].supply = sc450ai_supply_names[i];

	return devm_regulator_bulk_get(&sc450ai->client->dev,
				       SC450AI_NUM_SUPPLIES,
				       sc450ai->supplies);
}

static int sc450ai_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sc450ai *sc450ai;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	int i, hdr_mode = 0;
	struct device_node *endpoint;
	const char *sync_mode_name = NULL;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	sc450ai = devm_kzalloc(dev, sizeof(*sc450ai), GFP_KERNEL);
	if (!sc450ai)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc450ai->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc450ai->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc450ai->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc450ai->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	/* Compatible with non-standby mode if this attribute is not configured in dts*/
	of_property_read_u32(node, RKMODULE_CAMERA_STANDBY_HW,
			     &sc450ai->standby_hw);
	dev_info(dev, "sc450ai->standby_hw = %d\n", sc450ai->standby_hw);
	ret = of_property_read_string(node, RKMODULE_CAMERA_SYNC_MODE,
				      &sync_mode_name);
	if (ret) {
		sc450ai->sync_mode = NO_SYNC_MODE;
		dev_err(dev, "could not get sync mode!\n");
	} else {
		if (strcmp(sync_mode_name, RKMODULE_EXTERNAL_MASTER_MODE) == 0) {
			sc450ai->sync_mode = EXTERNAL_MASTER_MODE;
			dev_info(dev, "external master mode\n");
		} else if (strcmp(sync_mode_name, RKMODULE_INTERNAL_MASTER_MODE) == 0) {
			sc450ai->sync_mode = INTERNAL_MASTER_MODE;
			dev_info(dev, "internal master mode\n");
		} else if (strcmp(sync_mode_name, RKMODULE_SOFT_SYNC_MODE) == 0) {
			sc450ai->sync_mode = SOFT_SYNC_MODE;
			dev_info(dev, "soft sync mode\n");
		}
	}

	sc450ai->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);
	dev_err(dev, "========= is_thunderboot %d\n", sc450ai->is_thunderboot);

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
		&sc450ai->bus_cfg);
	of_node_put(endpoint);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}

	if (sc450ai->bus_cfg.bus.mipi_csi2.num_data_lanes == 4) {
		sc450ai->supported_modes = supported_modes_4lane;
		sc450ai->cfg_num = ARRAY_SIZE(supported_modes_4lane);
		dev_info(dev, "detect sc450ai lane: %d\n",
			 sc450ai->bus_cfg.bus.mipi_csi2.num_data_lanes);
	} else {
		sc450ai->supported_modes = supported_modes_2lane;
		sc450ai->cfg_num = ARRAY_SIZE(supported_modes_2lane);
		dev_info(dev, "detect sc450ai lane: %d\n",
			 sc450ai->bus_cfg.bus.mipi_csi2.num_data_lanes);
	}

	sc450ai->client = client;
	for (i = 0; i < sc450ai->cfg_num; i++) {
		if (hdr_mode == sc450ai->supported_modes[i].hdr_mode) {
			sc450ai->cur_mode = &sc450ai->supported_modes[i];
			break;
		}
	}

	if (i == sc450ai->cfg_num)
		sc450ai->cur_mode = &sc450ai->supported_modes[0];

	dev_dbg(dev, "SC450AI Info hdr_mode %d lanes %d vts 0x%04x fps %d\n",
		sc450ai->cur_mode->hdr_mode, sc450ai->cur_mode->lanes, sc450ai->cur_mode->vts_def,
		sc450ai->cur_mode->max_fps.denominator / sc450ai->cur_mode->max_fps.numerator);

	sc450ai->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc450ai->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	if (!sc450ai->is_thunderboot)
		sc450ai->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	else
		sc450ai->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(sc450ai->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	if (!sc450ai->is_thunderboot)
		sc450ai->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	else
		sc450ai->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_ASIS);
	if (IS_ERR(sc450ai->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	sc450ai->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc450ai->pinctrl)) {
		sc450ai->pins_default =
			pinctrl_lookup_state(sc450ai->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc450ai->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		sc450ai->pins_sleep =
			pinctrl_lookup_state(sc450ai->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc450ai->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = sc450ai_configure_regulators(sc450ai);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc450ai->mutex);

	sd = &sc450ai->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc450ai_subdev_ops);
	ret = sc450ai_initialize_controls(sc450ai);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc450ai_power_on(sc450ai);
	if (ret)
		goto err_free_handler;

	ret = sc450ai_check_sensor_id(sc450ai, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc450ai_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc450ai->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc450ai->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	if (!sc450ai->cam_sw_inf) {
		sc450ai->cam_sw_inf = cam_sw_init();
		cam_sw_clk_init(sc450ai->cam_sw_inf, sc450ai->xvclk,
			sc450ai->cur_mode->mclk);
		cam_sw_reset_pin_init(sc450ai->cam_sw_inf, sc450ai->reset_gpio, 0);
		cam_sw_pwdn_pin_init(sc450ai->cam_sw_inf, sc450ai->pwdn_gpio, 1);
	}

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc450ai->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc450ai->module_index, facing,
		 SC450AI_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (sc450ai->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__sc450ai_power_off(sc450ai);
err_free_handler:
	v4l2_ctrl_handler_free(&sc450ai->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc450ai->mutex);

	return ret;
}

static void sc450ai_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc450ai *sc450ai = to_sc450ai(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc450ai->ctrl_handler);
	mutex_destroy(&sc450ai->mutex);

	cam_sw_deinit(sc450ai->cam_sw_inf);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc450ai_power_off(sc450ai);
	pm_runtime_set_suspended(&client->dev);

}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc450ai_of_match[] = {
	{ .compatible = "smartsens,sc450ai" },
	{},
};
MODULE_DEVICE_TABLE(of, sc450ai_of_match);
#endif

static const struct i2c_device_id sc450ai_match_id[] = {
	{ "smartsens,sc450ai", 0 },
	{ },
};

static struct i2c_driver sc450ai_i2c_driver = {
	.driver = {
		.name = SC450AI_NAME,
		.pm = &sc450ai_pm_ops,
		.of_match_table = of_match_ptr(sc450ai_of_match),
	},
	.probe		= &sc450ai_probe,
	.remove		= &sc450ai_remove,
	.id_table	= sc450ai_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc450ai_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc450ai_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens sc450ai sensor driver");
MODULE_LICENSE("GPL");
