// SPDX-License-Identifier: GPL-2.0
/*
 * imx386 driver
 *
 * Copyright (C) 2017 Rockchip Electronics Co., Ltd.
 * V0.0X01.0X01 add imx386 driver.
 * V0.0X01.0X02 add imx386 support mirror and flip.
 * V0.0X01.0X03 add quick stream on/off
 */

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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x03)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define IMX386_LINK_FREQ_800		800000000// 1600Mbps
#define IMX386_LINK_FREQ_600		600000000// 1200Mbps
#define IMX386_LINK_FREQ_400		400000000// 800Mbps

#define IMX386_LANES			4

#define PIXEL_RATE_WITH_800M_10BIT	(IMX386_LINK_FREQ_800 * 2 / 10 * 4)

#define IMX386_XVCLK_FREQ		24000000

#define CHIP_ID				0x0386
#define IMX386_REG_CHIP_ID_H		0x0016
#define IMX386_REG_CHIP_ID_L		0x0017

#define IMX386_REG_CTRL_MODE		0x0100
#define IMX386_MODE_SW_STANDBY		0x0
#define IMX386_MODE_STREAMING		0x1

#define IMX386_REG_EXPOSURE_H		0x0202
#define IMX386_REG_EXPOSURE_L		0x0203
#define IMX386_EXPOSURE_MIN		2
#define IMX386_EXPOSURE_STEP		1
#define IMX386_VTS_MAX			0x7fff

#define IMX386_REG_GAIN_H		0x0204
#define IMX386_REG_GAIN_L		0x0205
#define IMX386_GAIN_MIN			0x100
#define IMX386_GAIN_MAX			0xFFFF
#define IMX386_GAIN_STEP		1
#define IMX386_GAIN_DEFAULT		0x0100

#define IMX386_REG_DGAIN		0x300b
#define IMX386_DGAIN_MODE		1
#define IMX386_REG_DGAINGR_H		0x020e
#define IMX386_REG_DGAINGR_L		0x020f
#define IMX386_REG_DGAINR_H		0x0210
#define IMX386_REG_DGAINR_L		0x0211
#define IMX386_REG_DGAINB_H		0x0212
#define IMX386_REG_DGAINB_L		0x0213
#define IMX386_REG_DGAINGB_H		0x0214
#define IMX386_REG_DGAINGB_L		0x0215
#define IMX386_REG_GAIN_GLOBAL_H	0x020e
#define IMX386_REG_GAIN_GLOBAL_L	0x020f

#define IMX386_REG_TEST_PATTERN		0x0601
#define IMX386_TEST_PATTERN_ENABLE	0x1
#define IMX386_TEST_PATTERN_DISABLE	0x0

#define IMX386_REG_VTS_H		0x0340
#define IMX386_REG_VTS_L		0x0341

#define IMX386_FLIP_MIRROR_REG		0x0101
#define IMX386_MIRROR_BIT_MASK		BIT(0)
#define IMX386_FLIP_BIT_MASK		BIT(1)

#define IMX386_GROUP_HOLD_REG		0x0104
#define IMX386_GROUP_HOLD_START		BIT(0)
#define IMX386_GROUP_HOLD_END		(0)

#define IMX386_FETCH_EXP_H(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX386_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define IMX386_FETCH_AGAIN_H(VAL)	(((VAL) >> 8) & 0x03)
#define IMX386_FETCH_AGAIN_L(VAL)	((VAL) & 0xFF)

#define IMX386_FETCH_DGAIN_H(VAL)	(((VAL) >> 8) & 0x0F)
#define IMX386_FETCH_DGAIN_L(VAL)	((VAL) & 0xFF)

#define IMX386_FETCH_RHS1_H(VAL)	(((VAL) >> 16) & 0x0F)
#define IMX386_FETCH_RHS1_M(VAL)	(((VAL) >> 8) & 0xFF)
#define IMX386_FETCH_RHS1_L(VAL)	((VAL) & 0xFF)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define IMX386_REG_VALUE_08BIT		1
#define IMX386_REG_VALUE_16BIT		2
#define IMX386_REG_VALUE_24BIT		3

#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

#define IMX386_NAME			"imx386"

static const char * const imx386_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define IMX386_NUM_SUPPLIES ARRAY_SIZE(imx386_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct imx386_mode {
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
	u32 link_freq_idx;
	u32 bpp;
};

struct imx386 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[IMX386_NUM_SUPPLIES];

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
	const struct imx386_mode *cur_mode;
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
	struct v4l2_fract	cur_fps;
};

#define to_imx386(sd) container_of(sd, struct imx386, subdev)

/*
 *IMX386LQR All-pixel scan CSI-2_4lane 24Mhz
 *AD:10bit Output:10bit 1600Mbps Master Mode 60fps
 *Tool ver : Ver4.0
 */
static const struct regval imx386_linear_10_4032x2256_regs[] = {
	{0x0136, 0x18},//global setting
	{0x0137, 0x00},
	{0x3A7D, 0x00},
	{0x3A7E, 0x02},
	{0x3A7F, 0x05},
	{0x3100, 0x00},
	{0x3101, 0x40},
	{0x3102, 0x00},
	{0x3103, 0x10},
	{0x3104, 0x01},
	{0x3105, 0xE8},
	{0x3106, 0x01},
	{0x3107, 0xF0},
	{0x3150, 0x04},
	{0x3151, 0x03},
	{0x3152, 0x02},
	{0x3153, 0x01},
	{0x5A86, 0x00},
	{0x5A87, 0x82},
	{0x5D1A, 0x00},
	{0x5D95, 0x02},
	{0x5E1B, 0x00},
	{0x5F5A, 0x00},
	{0x5F5B, 0x04},
	{0x682C, 0x31},
	{0x6831, 0x31},
	{0x6835, 0x0E},
	{0x6836, 0x31},
	{0x6838, 0x30},
	{0x683A, 0x06},
	{0x683B, 0x33},
	{0x683D, 0x30},
	{0x6842, 0x31},
	{0x6844, 0x31},
	{0x6847, 0x31},
	{0x6849, 0x31},
	{0x684D, 0x0E},
	{0x684E, 0x32},
	{0x6850, 0x31},
	{0x6852, 0x06},
	{0x6853, 0x33},
	{0x6855, 0x31},
	{0x685A, 0x32},
	{0x685C, 0x33},
	{0x685F, 0x31},
	{0x6861, 0x33},
	{0x6865, 0x0D},
	{0x6866, 0x33},
	{0x6868, 0x31},
	{0x686B, 0x34},
	{0x686D, 0x31},
	{0x6872, 0x32},
	{0x6877, 0x33},
	{0x7FF0, 0x01},
	{0x7FF4, 0x08},
	{0x7FF5, 0x3C},
	{0x7FFA, 0x01},
	{0x7FFD, 0x00},
	{0x831E, 0x00},
	{0x831F, 0x00},
	{0x9301, 0xBD},
	{0x9B94, 0x03},
	{0x9B95, 0x00},
	{0x9B96, 0x08},
	{0x9B97, 0x00},
	{0x9B98, 0x0A},
	{0x9B99, 0x00},
	{0x9BA7, 0x18},
	{0x9BA8, 0x18},
	{0x9D04, 0x08},
	{0x9D50, 0x8C},
	{0x9D51, 0x64},
	{0x9D52, 0x50},
	{0x9E31, 0x04},
	{0x9E32, 0x04},
	{0x9E33, 0x04},
	{0x9E34, 0x04},
	{0xA200, 0x00},
	{0xA201, 0x0A},
	{0xA202, 0x00},
	{0xA203, 0x0A},
	{0xA204, 0x00},
	{0xA205, 0x0A},
	{0xA206, 0x01},
	{0xA207, 0xC0},
	{0xA208, 0x00},
	{0xA209, 0xC0},
	{0xA20C, 0x00},
	{0xA20D, 0x0A},
	{0xA20E, 0x00},
	{0xA20F, 0x0A},
	{0xA210, 0x00},
	{0xA211, 0x0A},
	{0xA212, 0x01},
	{0xA213, 0xC0},
	{0xA214, 0x00},
	{0xA215, 0xC0},
	{0xA300, 0x00},
	{0xA301, 0x0A},
	{0xA302, 0x00},
	{0xA303, 0x0A},
	{0xA304, 0x00},
	{0xA305, 0x0A},
	{0xA306, 0x01},
	{0xA307, 0xC0},
	{0xA308, 0x00},
	{0xA309, 0xC0},
	{0xA30C, 0x00},
	{0xA30D, 0x0A},
	{0xA30E, 0x00},
	{0xA30F, 0x0A},
	{0xA310, 0x00},
	{0xA311, 0x0A},
	{0xA312, 0x01},
	{0xA313, 0xC0},
	{0xA314, 0x00},
	{0xA315, 0xC0},
	{0xBC19, 0x01},
	{0xBC1C, 0x0A},
	{0x3035, 0x01},//image quality
	{0x3051, 0x00},
	{0x7F47, 0x00},
	{0x7F78, 0x00},
	{0x7F89, 0x00},
	{0x7F93, 0x00},
	{0x7FB4, 0x00},
	{0x7FCC, 0x01},
	{0x9D02, 0x00},
	{0x9D44, 0x8C},
	{0x9D62, 0x8C},
	{0x9D63, 0x50},
	{0x9D64, 0x1B},
	{0x9E0D, 0x00},
	{0x9E0E, 0x00},
	{0x9E15, 0x0A},
	{0x9F02, 0x00},
	{0x9F03, 0x23},
	{0x9F4E, 0x00},
	{0x9F4F, 0x42},
	{0x9F54, 0x00},
	{0x9F55, 0x5A},
	{0x9F6E, 0x00},
	{0x9F6F, 0x10},
	{0x9F72, 0x00},
	{0x9F73, 0xC8},
	{0x9F74, 0x00},
	{0x9F75, 0x32},
	{0x9FD3, 0x00},
	{0x9FD4, 0x00},
	{0x9FD5, 0x00},
	{0x9FD6, 0x3C},
	{0x9FD7, 0x3C},
	{0x9FD8, 0x3C},
	{0x9FD9, 0x00},
	{0x9FDA, 0x00},
	{0x9FDB, 0x00},
	{0x9FDC, 0xFF},
	{0x9FDD, 0xFF},
	{0x9FDE, 0xFF},
	{0xA002, 0x00},
	{0xA003, 0x14},
	{0xA04E, 0x00},
	{0xA04F, 0x2D},
	{0xA054, 0x00},
	{0xA055, 0x40},
	{0xA06E, 0x00},
	{0xA06F, 0x10},
	{0xA072, 0x00},
	{0xA073, 0xC8},
	{0xA074, 0x00},
	{0xA075, 0x32},
	{0xA0CA, 0x04},
	{0xA0CB, 0x04},
	{0xA0CC, 0x04},
	{0xA0D3, 0x0A},
	{0xA0D4, 0x0A},
	{0xA0D5, 0x0A},
	{0xA0D6, 0x00},
	{0xA0D7, 0x00},
	{0xA0D8, 0x00},
	{0xA0D9, 0x18},
	{0xA0DA, 0x18},
	{0xA0DB, 0x18},
	{0xA0DC, 0x00},
	{0xA0DD, 0x00},
	{0xA0DE, 0x00},
	{0xBCB2, 0x01},
	{0x0112, 0x0A},//4k2k@60fps
	{0x0113, 0x0A},
	{0x0301, 0x04},
	{0x0303, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x64},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030D, 0x0C},
	{0x030E, 0x03},
	{0x030F, 0x20},
	{0x0310, 0x01},
	{0x0342, 0x10},//10c8 60fps
	{0x0343, 0xc8},
	{0x0340, 0x09},
	{0x0341, 0x66},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x01},
	{0x0347, 0x7C},
	{0x0348, 0x0F},
	{0x0349, 0xBF},
	{0x034A, 0x0A},
	{0x034B, 0x4B},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x300D, 0x00},
	{0x302E, 0x01},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x040C, 0x0F},
	{0x040D, 0xC0},
	{0x040E, 0x08},
	{0x040F, 0xD0},
	{0x034C, 0x0F},
	{0x034D, 0xC0},
	{0x034E, 0x08},
	{0x034F, 0xD0},
	{0x0114, 0x03},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x0902, 0x00},
	{0x3030, 0x00},
	{0x3031, 0x01},
	{0x3032, 0x00},
	{0x3047, 0x01},
	{0x3049, 0x01},
	{0x30E6, 0x00},
	{0x30E7, 0x00},
	{0x4E25, 0x80},
	{0x663A, 0x02},
	{0x9311, 0x00},
	{0xA0CD, 0x19},
	{0xA0CE, 0x19},
	{0xA0CF, 0x19},
	{0x0202, 0x09},
	{0x0203, 0x0C},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{REG_NULL, 0x00},
};

static const struct regval imx386_linear_10_1920x1080_regs[] = {
	{0x0136, 0x18},//global setting
	{0x0137, 0x00},
	{0x3A7D, 0x00},
	{0x3A7E, 0x02},
	{0x3A7F, 0x05},
	{0x3100, 0x00},
	{0x3101, 0x40},
	{0x3102, 0x00},
	{0x3103, 0x10},
	{0x3104, 0x01},
	{0x3105, 0xE8},
	{0x3106, 0x01},
	{0x3107, 0xF0},
	{0x3150, 0x04},
	{0x3151, 0x03},
	{0x3152, 0x02},
	{0x3153, 0x01},
	{0x5A86, 0x00},
	{0x5A87, 0x82},
	{0x5D1A, 0x00},
	{0x5D95, 0x02},
	{0x5E1B, 0x00},
	{0x5F5A, 0x00},
	{0x5F5B, 0x04},
	{0x682C, 0x31},
	{0x6831, 0x31},
	{0x6835, 0x0E},
	{0x6836, 0x31},
	{0x6838, 0x30},
	{0x683A, 0x06},
	{0x683B, 0x33},
	{0x683D, 0x30},
	{0x6842, 0x31},
	{0x6844, 0x31},
	{0x6847, 0x31},
	{0x6849, 0x31},
	{0x684D, 0x0E},
	{0x684E, 0x32},
	{0x6850, 0x31},
	{0x6852, 0x06},
	{0x6853, 0x33},
	{0x6855, 0x31},
	{0x685A, 0x32},
	{0x685C, 0x33},
	{0x685F, 0x31},
	{0x6861, 0x33},
	{0x6865, 0x0D},
	{0x6866, 0x33},
	{0x6868, 0x31},
	{0x686B, 0x34},
	{0x686D, 0x31},
	{0x6872, 0x32},
	{0x6877, 0x33},
	{0x7FF0, 0x01},
	{0x7FF4, 0x08},
	{0x7FF5, 0x3C},
	{0x7FFA, 0x01},
	{0x7FFD, 0x00},
	{0x831E, 0x00},
	{0x831F, 0x00},
	{0x9301, 0xBD},
	{0x9B94, 0x03},
	{0x9B95, 0x00},
	{0x9B96, 0x08},
	{0x9B97, 0x00},
	{0x9B98, 0x0A},
	{0x9B99, 0x00},
	{0x9BA7, 0x18},
	{0x9BA8, 0x18},
	{0x9D04, 0x08},
	{0x9D50, 0x8C},
	{0x9D51, 0x64},
	{0x9D52, 0x50},
	{0x9E31, 0x04},
	{0x9E32, 0x04},
	{0x9E33, 0x04},
	{0x9E34, 0x04},
	{0xA200, 0x00},
	{0xA201, 0x0A},
	{0xA202, 0x00},
	{0xA203, 0x0A},
	{0xA204, 0x00},
	{0xA205, 0x0A},
	{0xA206, 0x01},
	{0xA207, 0xC0},
	{0xA208, 0x00},
	{0xA209, 0xC0},
	{0xA20C, 0x00},
	{0xA20D, 0x0A},
	{0xA20E, 0x00},
	{0xA20F, 0x0A},
	{0xA210, 0x00},
	{0xA211, 0x0A},
	{0xA212, 0x01},
	{0xA213, 0xC0},
	{0xA214, 0x00},
	{0xA215, 0xC0},
	{0xA300, 0x00},
	{0xA301, 0x0A},
	{0xA302, 0x00},
	{0xA303, 0x0A},
	{0xA304, 0x00},
	{0xA305, 0x0A},
	{0xA306, 0x01},
	{0xA307, 0xC0},
	{0xA308, 0x00},
	{0xA309, 0xC0},
	{0xA30C, 0x00},
	{0xA30D, 0x0A},
	{0xA30E, 0x00},
	{0xA30F, 0x0A},
	{0xA310, 0x00},
	{0xA311, 0x0A},
	{0xA312, 0x01},
	{0xA313, 0xC0},
	{0xA314, 0x00},
	{0xA315, 0xC0},
	{0xBC19, 0x01},
	{0xBC1C, 0x0A},
	{0x3035, 0x01},//image quality
	{0x3051, 0x00},
	{0x7F47, 0x00},
	{0x7F78, 0x00},
	{0x7F89, 0x00},
	{0x7F93, 0x00},
	{0x7FB4, 0x00},
	{0x7FCC, 0x01},
	{0x9D02, 0x00},
	{0x9D44, 0x8C},
	{0x9D62, 0x8C},
	{0x9D63, 0x50},
	{0x9D64, 0x1B},
	{0x9E0D, 0x00},
	{0x9E0E, 0x00},
	{0x9E15, 0x0A},
	{0x9F02, 0x00},
	{0x9F03, 0x23},
	{0x9F4E, 0x00},
	{0x9F4F, 0x42},
	{0x9F54, 0x00},
	{0x9F55, 0x5A},
	{0x9F6E, 0x00},
	{0x9F6F, 0x10},
	{0x9F72, 0x00},
	{0x9F73, 0xC8},
	{0x9F74, 0x00},
	{0x9F75, 0x32},
	{0x9FD3, 0x00},
	{0x9FD4, 0x00},
	{0x9FD5, 0x00},
	{0x9FD6, 0x3C},
	{0x9FD7, 0x3C},
	{0x9FD8, 0x3C},
	{0x9FD9, 0x00},
	{0x9FDA, 0x00},
	{0x9FDB, 0x00},
	{0x9FDC, 0xFF},
	{0x9FDD, 0xFF},
	{0x9FDE, 0xFF},
	{0xA002, 0x00},
	{0xA003, 0x14},
	{0xA04E, 0x00},
	{0xA04F, 0x2D},
	{0xA054, 0x00},
	{0xA055, 0x40},
	{0xA06E, 0x00},
	{0xA06F, 0x10},
	{0xA072, 0x00},
	{0xA073, 0xC8},
	{0xA074, 0x00},
	{0xA075, 0x32},
	{0xA0CA, 0x04},
	{0xA0CB, 0x04},
	{0xA0CC, 0x04},
	{0xA0D3, 0x0A},
	{0xA0D4, 0x0A},
	{0xA0D5, 0x0A},
	{0xA0D6, 0x00},
	{0xA0D7, 0x00},
	{0xA0D8, 0x00},
	{0xA0D9, 0x18},
	{0xA0DA, 0x18},
	{0xA0DB, 0x18},
	{0xA0DC, 0x00},
	{0xA0DD, 0x00},
	{0xA0DE, 0x00},
	{0xBCB2, 0x01},
	{0x0112, 0x0A},//1080p@120fps
	{0x0113, 0x0A},
	{0x0301, 0x06},
	{0x0303, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x50},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030D, 0x0C},
	{0x030E, 0x01},
	{0x030F, 0x94},
	{0x0310, 0x01},
	{0x0342, 0x08},
	{0x0343, 0xD0},
	{0x0340, 0x04},
	{0x0341, 0x9E},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x01},
	{0x0347, 0x7C},
	{0x0348, 0x0F},
	{0x0349, 0xBF},
	{0x034A, 0x0A},
	{0x034B, 0x4B},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x300D, 0x00},
	{0x302E, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x040C, 0x07},
	{0x040D, 0x80},
	{0x040E, 0x04},
	{0x040F, 0x38},
	{0x034C, 0x07},
	{0x034D, 0x80},
	{0x034E, 0x04},
	{0x034F, 0x38},
	{0x0114, 0x03},
	{0x0408, 0x00},
	{0x0409, 0x30},
	{0x040A, 0x00},
	{0x040B, 0x18},
	{0x0902, 0x02},
	{0x3030, 0x00},
	{0x3031, 0x01},
	{0x3032, 0x00},
	{0x3047, 0x01},
	{0x3049, 0x00},
	{0x30E6, 0x00},
	{0x30E7, 0x00},
	{0x4E25, 0x80},
	{0x663A, 0x01},
	{0x9311, 0x3F},
	{0xA0CD, 0x0A},
	{0xA0CE, 0x0A},
	{0xA0CF, 0x0A},
	{0x0202, 0x04},
	{0x0203, 0x94},
	{0x0202, 0x04},
	{0x0203, 0x94},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{REG_NULL, 0x00},
};

static const struct regval imx386_linear_10_4032x3016_30fps_regs[] = {
	{0x0136, 0x18},//global setting
	{0x0137, 0x00},
	{0x3A7D, 0x00},
	{0x3A7E, 0x02},
	{0x3A7F, 0x05},
	{0x3100, 0x00},
	{0x3101, 0x40},
	{0x3102, 0x00},
	{0x3103, 0x10},
	{0x3104, 0x01},
	{0x3105, 0xE8},
	{0x3106, 0x01},
	{0x3107, 0xF0},
	{0x3150, 0x04},
	{0x3151, 0x03},
	{0x3152, 0x02},
	{0x3153, 0x01},
	{0x5A86, 0x00},
	{0x5A87, 0x82},
	{0x5D1A, 0x00},
	{0x5D95, 0x02},
	{0x5E1B, 0x00},
	{0x5F5A, 0x00},
	{0x5F5B, 0x04},
	{0x682C, 0x31},
	{0x6831, 0x31},
	{0x6835, 0x0E},
	{0x6836, 0x31},
	{0x6838, 0x30},
	{0x683A, 0x06},
	{0x683B, 0x33},
	{0x683D, 0x30},
	{0x6842, 0x31},
	{0x6844, 0x31},
	{0x6847, 0x31},
	{0x6849, 0x31},
	{0x684D, 0x0E},
	{0x684E, 0x32},
	{0x6850, 0x31},
	{0x6852, 0x06},
	{0x6853, 0x33},
	{0x6855, 0x31},
	{0x685A, 0x32},
	{0x685C, 0x33},
	{0x685F, 0x31},
	{0x6861, 0x33},
	{0x6865, 0x0D},
	{0x6866, 0x33},
	{0x6868, 0x31},
	{0x686B, 0x34},
	{0x686D, 0x31},
	{0x6872, 0x32},
	{0x6877, 0x33},
	{0x7FF0, 0x01},
	{0x7FF4, 0x08},
	{0x7FF5, 0x3C},
	{0x7FFA, 0x01},
	{0x7FFD, 0x00},
	{0x831E, 0x00},
	{0x831F, 0x00},
	{0x9301, 0xBD},
	{0x9B94, 0x03},
	{0x9B95, 0x00},
	{0x9B96, 0x08},
	{0x9B97, 0x00},
	{0x9B98, 0x0A},
	{0x9B99, 0x00},
	{0x9BA7, 0x18},
	{0x9BA8, 0x18},
	{0x9D04, 0x08},
	{0x9D50, 0x8C},
	{0x9D51, 0x64},
	{0x9D52, 0x50},
	{0x9E31, 0x04},
	{0x9E32, 0x04},
	{0x9E33, 0x04},
	{0x9E34, 0x04},
	{0xA200, 0x00},
	{0xA201, 0x0A},
	{0xA202, 0x00},
	{0xA203, 0x0A},
	{0xA204, 0x00},
	{0xA205, 0x0A},
	{0xA206, 0x01},
	{0xA207, 0xC0},
	{0xA208, 0x00},
	{0xA209, 0xC0},
	{0xA20C, 0x00},
	{0xA20D, 0x0A},
	{0xA20E, 0x00},
	{0xA20F, 0x0A},
	{0xA210, 0x00},
	{0xA211, 0x0A},
	{0xA212, 0x01},
	{0xA213, 0xC0},
	{0xA214, 0x00},
	{0xA215, 0xC0},
	{0xA300, 0x00},
	{0xA301, 0x0A},
	{0xA302, 0x00},
	{0xA303, 0x0A},
	{0xA304, 0x00},
	{0xA305, 0x0A},
	{0xA306, 0x01},
	{0xA307, 0xC0},
	{0xA308, 0x00},
	{0xA309, 0xC0},
	{0xA30C, 0x00},
	{0xA30D, 0x0A},
	{0xA30E, 0x00},
	{0xA30F, 0x0A},
	{0xA310, 0x00},
	{0xA311, 0x0A},
	{0xA312, 0x01},
	{0xA313, 0xC0},
	{0xA314, 0x00},
	{0xA315, 0xC0},
	{0xBC19, 0x01},
	{0xBC1C, 0x0A},
	{0x3035, 0x01},//image quality
	{0x3051, 0x00},
	{0x7F47, 0x00},
	{0x7F78, 0x00},
	{0x7F89, 0x00},
	{0x7F93, 0x00},
	{0x7FB4, 0x00},
	{0x7FCC, 0x01},
	{0x9D02, 0x00},
	{0x9D44, 0x8C},
	{0x9D62, 0x8C},
	{0x9D63, 0x50},
	{0x9D64, 0x1B},
	{0x9E0D, 0x00},
	{0x9E0E, 0x00},
	{0x9E15, 0x0A},
	{0x9F02, 0x00},
	{0x9F03, 0x23},
	{0x9F4E, 0x00},
	{0x9F4F, 0x42},
	{0x9F54, 0x00},
	{0x9F55, 0x5A},
	{0x9F6E, 0x00},
	{0x9F6F, 0x10},
	{0x9F72, 0x00},
	{0x9F73, 0xC8},
	{0x9F74, 0x00},
	{0x9F75, 0x32},
	{0x9FD3, 0x00},
	{0x9FD4, 0x00},
	{0x9FD5, 0x00},
	{0x9FD6, 0x3C},
	{0x9FD7, 0x3C},
	{0x9FD8, 0x3C},
	{0x9FD9, 0x00},
	{0x9FDA, 0x00},
	{0x9FDB, 0x00},
	{0x9FDC, 0xFF},
	{0x9FDD, 0xFF},
	{0x9FDE, 0xFF},
	{0xA002, 0x00},
	{0xA003, 0x14},
	{0xA04E, 0x00},
	{0xA04F, 0x2D},
	{0xA054, 0x00},
	{0xA055, 0x40},
	{0xA06E, 0x00},
	{0xA06F, 0x10},
	{0xA072, 0x00},
	{0xA073, 0xC8},
	{0xA074, 0x00},
	{0xA075, 0x32},
	{0xA0CA, 0x04},
	{0xA0CB, 0x04},
	{0xA0CC, 0x04},
	{0xA0D3, 0x0A},
	{0xA0D4, 0x0A},
	{0xA0D5, 0x0A},
	{0xA0D6, 0x00},
	{0xA0D7, 0x00},
	{0xA0D8, 0x00},
	{0xA0D9, 0x18},
	{0xA0DA, 0x18},
	{0xA0DB, 0x18},
	{0xA0DC, 0x00},
	{0xA0DD, 0x00},
	{0xA0DE, 0x00},
	{0xBCB2, 0x01},
	{0x0112, 0x0A},//4k3k@30fps
	{0x0113, 0x0A},
	{0x0301, 0x04},
	{0x0303, 0x02},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x64},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030D, 0x0C},
	{0x030E, 0x03},
	{0x030F, 0x20},
	{0x0310, 0x01},
	{0x0342, 0x10}, //hts
	{0x0343, 0xC8},
	{0x0340, 0x12}, //vts
	{0x0341, 0x2b},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0F},
	{0x0349, 0xBF},
	{0x034A, 0x0B},
	{0x034B, 0xC7},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x00},
	{0x0901, 0x11},
	{0x300D, 0x00},
	{0x302E, 0x00},
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x040C, 0x0F},
	{0x040D, 0xC0},
	{0x040E, 0x0B},
	{0x040F, 0xC8},
	{0x034C, 0x0F},
	{0x034D, 0xC0},
	{0x034E, 0x0B}, //Y_OUT_SIZE
	{0x034F, 0xC8},
	{0x0114, 0x03},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x0902, 0x00},
	{0x3030, 0x00},
	{0x3031, 0x01},
	{0x3032, 0x00},
	{0x3047, 0x01},
	{0x3049, 0x01},
	{0x30E6, 0x02},
	{0x30E7, 0x59},
	{0x4E25, 0x80},
	{0x663A, 0x02},
	{0x9311, 0x00},
	{0xA0CD, 0x19},
	{0xA0CE, 0x19},
	{0xA0CF, 0x19},
	{0x0202, 0x0C},
	{0x0203, 0x14}, //Integration Time
	{0x0204, 0x00},
	{REG_NULL, 0x00},
};

static const struct imx386_mode supported_modes[] = {
	{
		.width = 4032,
		.height = 2256,
		.max_fps = {
			.numerator = 10000,
			.denominator = 580000,
		},
		.exp_def = 0x0600,
		.hts_def = 0x10c8,
		.vts_def = 0x0966,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = imx386_linear_10_4032x2256_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
		.link_freq_idx = 1,
		.bpp = 10,
	},
	{
		.width = 4032,
		.height = 2256,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0600,
		.hts_def = 0x10c8,
		.vts_def = 0x122b,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = imx386_linear_10_4032x2256_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
		.link_freq_idx = 1,
		.bpp = 10,
	},
	{
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 1200000,
		},
		.exp_def = 0x300,
		.hts_def = 0x8d0,
		.vts_def = 0x49e,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = imx386_linear_10_1920x1080_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
		.link_freq_idx = 0,
		.bpp = 10,
	},
	{
		.width = 4032,
		.height = 3016,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0600,
		.hts_def = 0x10c8,
		.vts_def = 0x122b,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.reg_list = imx386_linear_10_4032x3016_30fps_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
		.link_freq_idx = 1,
		.bpp = 10,
	},
};

static const s64 link_freq_menu_items[] = {
	IMX386_LINK_FREQ_400,
	IMX386_LINK_FREQ_800,
};

static const u32 bus_code[] = {
	MEDIA_BUS_FMT_SRGGB10_1X10,
};

static const char * const imx386_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int imx386_write_reg(struct i2c_client *client, u16 reg,
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

static int imx386_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		if (unlikely(regs[i].addr == REG_DELAY))
			usleep_range(regs[i].val, regs[i].val * 2);
		else
			ret = imx386_write_reg(client, regs[i].addr,
					       IMX386_REG_VALUE_08BIT,
					       regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int imx386_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int imx386_get_reso_dist(const struct imx386_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		   abs(mode->height - framefmt->height);
}

static const struct imx386_mode *
imx386_find_best_fit(struct imx386 *imx386, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < imx386->cfg_num; i++) {
		dist = imx386_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int imx386_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx386 *imx386 = to_imx386(sd);
	const struct imx386_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&imx386->mutex);

	mode = imx386_find_best_fit(imx386, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx386->mutex);
		return -ENOTTY;
#endif
	} else {
		imx386->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx386->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(imx386->vblank, vblank_def,
					 IMX386_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(imx386->vblank, vblank_def);

		imx386->cur_link_freq = mode->link_freq_idx;
		imx386->cur_pixel_rate = link_freq_menu_items[mode->link_freq_idx] *
			2 / mode->bpp * IMX386_LANES;
		__v4l2_ctrl_s_ctrl_int64(imx386->pixel_rate,
					 imx386->cur_pixel_rate);
		__v4l2_ctrl_s_ctrl(imx386->link_freq,
				   imx386->cur_link_freq);
		imx386->cur_fps = mode->max_fps;
	}

	mutex_unlock(&imx386->mutex);

	return 0;
}

static int imx386_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx386 *imx386 = to_imx386(sd);
	const struct imx386_mode *mode = imx386->cur_mode;

	mutex_lock(&imx386->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&imx386->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		if (imx386->flip & IMX386_MIRROR_BIT_MASK) {
			fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;
			if (imx386->flip & IMX386_FLIP_BIT_MASK)
				fmt->format.code = MEDIA_BUS_FMT_SBGGR10_1X10;
		} else if (imx386->flip & IMX386_FLIP_BIT_MASK) {
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
	mutex_unlock(&imx386->mutex);

	return 0;
}

static int imx386_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(bus_code))
		return -EINVAL;
	code->code = bus_code[code->index];

	return 0;
}

static int imx386_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx386 *imx386 = to_imx386(sd);

	if (fse->index >= imx386->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int imx386_enable_test_pattern(struct imx386 *imx386, u32 pattern)
{
	u32 val;

	if (pattern)
		val = (pattern - 1) | IMX386_TEST_PATTERN_ENABLE;
	else
		val = IMX386_TEST_PATTERN_DISABLE;

	return imx386_write_reg(imx386->client,
				IMX386_REG_TEST_PATTERN,
				IMX386_REG_VALUE_08BIT,
				val);
}

static int imx386_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx386 *imx386 = to_imx386(sd);
	const struct imx386_mode *mode = imx386->cur_mode;

	if (imx386->streaming)
		fi->interval = imx386->cur_fps;
	else
		fi->interval = mode->max_fps;

	return 0;
}

static const struct imx386_mode *imx386_find_mode(struct imx386 *imx386, int fps)
{
	const struct imx386_mode *mode = NULL;
	const struct imx386_mode *match = NULL;
	int cur_fps = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		mode = &supported_modes[i];
		if (mode->width == imx386->cur_mode->width &&
		    mode->height == imx386->cur_mode->height &&
		    mode->bus_fmt == imx386->cur_mode->bus_fmt &&
		    mode->hdr_mode == imx386->cur_mode->hdr_mode) {
			cur_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator, mode->max_fps.numerator);
			if (cur_fps == fps) {
				match = mode;
				break;
			}
		}
	}
	return match;
}

static int imx386_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx386 *imx386 = to_imx386(sd);
	const struct imx386_mode *mode = NULL;
	struct v4l2_fract *fract = &fi->interval;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	int fps;

	if (imx386->streaming)
		return -EBUSY;

	if (fi->pad != 0)
		return -EINVAL;

	if (fract->numerator == 0) {
		v4l2_err(sd, "error param, check interval param\n");
		return -EINVAL;
	}
	fps = DIV_ROUND_CLOSEST(fract->denominator, fract->numerator);
	mode = imx386_find_mode(imx386, fps);
	if (mode == NULL) {
		v4l2_err(sd, "couldn't match fi\n");
		return -EINVAL;
	}

	imx386->cur_mode = mode;

	h_blank = mode->hts_def - mode->width;
	__v4l2_ctrl_modify_range(imx386->hblank, h_blank,
				 h_blank, 1, h_blank);
	vblank_def = mode->vts_def - mode->height;
	__v4l2_ctrl_modify_range(imx386->vblank, vblank_def,
				 IMX386_VTS_MAX - mode->height,
				 1, vblank_def);
	__v4l2_ctrl_s_ctrl(imx386->vblank, vblank_def);
	pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] / mode->bpp * 2 * IMX386_LANES;

	__v4l2_ctrl_s_ctrl_int64(imx386->pixel_rate,
				 pixel_rate);
	__v4l2_ctrl_s_ctrl(imx386->link_freq,
			   mode->link_freq_idx);
	imx386->cur_fps = mode->max_fps;

	return 0;
}

static int imx386_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = IMX386_LANES;

	return 0;
}

static void imx386_get_module_inf(struct imx386 *imx386,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, IMX386_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, imx386->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, imx386->len_name, sizeof(inf->base.lens));
}

static long imx386_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx386 *imx386 = to_imx386(sd);
	struct rkmodule_hdr_cfg *hdr;
	long ret = 0;
	u32 i, h, w;
	u32 stream = 0;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx386_get_module_inf(imx386, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = imx386->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = imx386->cur_mode->width;
		h = imx386->cur_mode->height;
		for (i = 0; i < imx386->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].bus_fmt == imx386->cur_mode->bus_fmt &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode) {
				imx386->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == imx386->cfg_num) {
			dev_err(&imx386->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = imx386->cur_mode->hts_def -
			    imx386->cur_mode->width;
			h = imx386->cur_mode->vts_def -
			    imx386->cur_mode->height;
			__v4l2_ctrl_modify_range(imx386->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(imx386->vblank, h,
						 IMX386_VTS_MAX -
						 imx386->cur_mode->height,
						 1, h);
			__v4l2_ctrl_s_ctrl(imx386->vblank, h);

			imx386->cur_link_freq = imx386->cur_mode->link_freq_idx;
			imx386->cur_pixel_rate = link_freq_menu_items[imx386->cur_mode->link_freq_idx] *
				2 / imx386->cur_mode->bpp * IMX386_LANES;

			__v4l2_ctrl_s_ctrl_int64(imx386->pixel_rate,
						 imx386->cur_pixel_rate);
			__v4l2_ctrl_s_ctrl(imx386->link_freq,
					   imx386->cur_link_freq);
			imx386->cur_fps = imx386->cur_mode->max_fps;
		}
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx386_write_reg(imx386->client, IMX386_REG_CTRL_MODE,
				IMX386_REG_VALUE_08BIT, IMX386_MODE_STREAMING);
		else
			ret = imx386_write_reg(imx386->client, IMX386_REG_CTRL_MODE,
				IMX386_REG_VALUE_08BIT, IMX386_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx386_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
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

		ret = imx386_ioctl(sd, cmd, inf);
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
			ret = imx386_ioctl(sd, cmd, cfg);
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

		ret = imx386_ioctl(sd, cmd, hdr);
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
			ret = imx386_ioctl(sd, cmd, hdr);
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
			ret = imx386_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = imx386_ioctl(sd, cmd, &stream);
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

static int imx386_set_flip(struct imx386 *imx386)
{
	int ret = 0;
	u32 val = 0;

	ret = imx386_read_reg(imx386->client, IMX386_FLIP_MIRROR_REG,
			      IMX386_REG_VALUE_08BIT, &val);
	if (imx386->flip & IMX386_MIRROR_BIT_MASK)
		val |= IMX386_MIRROR_BIT_MASK;
	else
		val &= ~IMX386_MIRROR_BIT_MASK;
	if (imx386->flip & IMX386_FLIP_BIT_MASK)
		val |= IMX386_FLIP_BIT_MASK;
	else
		val &= ~IMX386_FLIP_BIT_MASK;
	ret |= imx386_write_reg(imx386->client, IMX386_FLIP_MIRROR_REG,
				IMX386_REG_VALUE_08BIT, val);

	return ret;
}

static int __imx386_start_stream(struct imx386 *imx386)
{
	int ret;

	ret = imx386_write_array(imx386->client, imx386->cur_mode->reg_list);
	if (ret)
		return ret;
	imx386->cur_vts = imx386->cur_mode->vts_def;
	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx386->ctrl_handler);
	if (ret)
		return ret;
	if (imx386->has_init_exp && imx386->cur_mode->hdr_mode != NO_HDR) {
		ret = imx386_ioctl(&imx386->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&imx386->init_hdrae_exp);
		if (ret) {
			dev_err(&imx386->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}

	imx386_set_flip(imx386);

	return imx386_write_reg(imx386->client, IMX386_REG_CTRL_MODE,
				IMX386_REG_VALUE_08BIT, IMX386_MODE_STREAMING);
}

static int __imx386_stop_stream(struct imx386 *imx386)
{
	return imx386_write_reg(imx386->client, IMX386_REG_CTRL_MODE,
				IMX386_REG_VALUE_08BIT, IMX386_MODE_SW_STANDBY);
}

static int imx386_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx386 *imx386 = to_imx386(sd);
	struct i2c_client *client = imx386->client;
	int ret = 0;

	mutex_lock(&imx386->mutex);
	on = !!on;
	if (on == imx386->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx386_start_stream(imx386);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx386_stop_stream(imx386);
		pm_runtime_put(&client->dev);
	}

	imx386->streaming = on;

unlock_and_return:
	mutex_unlock(&imx386->mutex);

	return ret;
}

static int imx386_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx386 *imx386 = to_imx386(sd);
	struct i2c_client *client = imx386->client;
	int ret = 0;

	mutex_lock(&imx386->mutex);

	/* If the power state is not modified - no work to do. */
	if (imx386->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		imx386->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx386->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx386->mutex);

	return ret;
}

static int __imx386_power_on(struct imx386 *imx386)
{
	int ret;
	struct device *dev = &imx386->client->dev;

	ret = clk_set_rate(imx386->xvclk, IMX386_XVCLK_FREQ);
	if (ret < 0) {
		dev_err(dev, "Failed to set xvclk rate (24MHz)\n");
		return ret;
	}
	if (clk_get_rate(imx386->xvclk) != IMX386_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 37.125MHz\n");
	ret = clk_prepare_enable(imx386->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (!IS_ERR(imx386->reset_gpio))
		gpiod_set_value_cansleep(imx386->reset_gpio, 0);

	ret = regulator_bulk_enable(IMX386_NUM_SUPPLIES, imx386->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(imx386->reset_gpio))
		gpiod_set_value_cansleep(imx386->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(imx386->pwdn_gpio))
		gpiod_set_value_cansleep(imx386->pwdn_gpio, 1);

	usleep_range(15000, 16000);

	return 0;

disable_clk:
	clk_disable_unprepare(imx386->xvclk);

	return ret;
}

static void __imx386_power_off(struct imx386 *imx386)
{
	if (!IS_ERR(imx386->pwdn_gpio))
		gpiod_set_value_cansleep(imx386->pwdn_gpio, 0);
	clk_disable_unprepare(imx386->xvclk);
	if (!IS_ERR(imx386->reset_gpio))
		gpiod_set_value_cansleep(imx386->reset_gpio, 0);
	regulator_bulk_disable(IMX386_NUM_SUPPLIES, imx386->supplies);
}

static int imx386_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx386 *imx386 = to_imx386(sd);

	return __imx386_power_on(imx386);
}

static int imx386_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx386 *imx386 = to_imx386(sd);

	__imx386_power_off(imx386);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx386_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx386 *imx386 = to_imx386(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct imx386_mode *def_mode = &supported_modes[0];

	mutex_lock(&imx386->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx386->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int imx386_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx386 *imx386 = to_imx386(sd);

	if (fie->index >= imx386->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

static const struct dev_pm_ops imx386_pm_ops = {
	SET_RUNTIME_PM_OPS(imx386_runtime_suspend,
			   imx386_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx386_internal_ops = {
	.open = imx386_open,
};
#endif

static const struct v4l2_subdev_core_ops imx386_core_ops = {
	.s_power = imx386_s_power,
	.ioctl = imx386_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx386_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx386_video_ops = {
	.s_stream = imx386_s_stream,
	.g_frame_interval = imx386_g_frame_interval,
	.s_frame_interval = imx386_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx386_pad_ops = {
	.enum_mbus_code = imx386_enum_mbus_code,
	.enum_frame_size = imx386_enum_frame_sizes,
	.enum_frame_interval = imx386_enum_frame_interval,
	.get_fmt = imx386_get_fmt,
	.set_fmt = imx386_set_fmt,
	.get_mbus_config = imx386_g_mbus_config,
};

static const struct v4l2_subdev_ops imx386_subdev_ops = {
	.core	= &imx386_core_ops,
	.video	= &imx386_video_ops,
	.pad	= &imx386_pad_ops,
};

static void imx386_modify_fps_info(struct imx386 *imx386)
{
	const struct imx386_mode *mode = imx386->cur_mode;

	imx386->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				      imx386->cur_vts;
}

static int imx386_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx386 *imx386 = container_of(ctrl->handler,
					     struct imx386, ctrl_handler);
	struct i2c_client *client = imx386->client;
	s64 max;
	int ret = 0;
	u32 again = 0;
	u32 dgain = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = imx386->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(imx386->exposure,
					 imx386->exposure->minimum, max,
					 imx386->exposure->step,
					 imx386->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		dev_dbg(&client->dev, "set exp %d\n", ctrl->val);
		ret = imx386_write_reg(imx386->client,
				       IMX386_REG_EXPOSURE_H,
				       IMX386_REG_VALUE_08BIT,
				       IMX386_FETCH_EXP_H(ctrl->val));
		ret |= imx386_write_reg(imx386->client,
					IMX386_REG_EXPOSURE_L,
					IMX386_REG_VALUE_08BIT,
					IMX386_FETCH_EXP_L(ctrl->val));
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (ctrl->val <= 16 * 256) {
			again = 512 - (512 * 256 / ctrl->val);
			dgain = 256;
		} else {
			again = 480;
			dgain = ctrl->val / 16;
		}
		dev_dbg(&client->dev, "set total gain %d, again %d, dgain %d\n",
			ctrl->val, again, dgain);
		ret = imx386_write_reg(imx386->client, IMX386_GROUP_HOLD_REG,
				       IMX386_REG_VALUE_08BIT,
				       IMX386_GROUP_HOLD_START);
		ret |= imx386_write_reg(imx386->client, IMX386_REG_GAIN_H,
					IMX386_REG_VALUE_08BIT,
					IMX386_FETCH_AGAIN_H(again));
		ret |= imx386_write_reg(imx386->client, IMX386_REG_GAIN_L,
					IMX386_REG_VALUE_08BIT,
					IMX386_FETCH_AGAIN_L(again));
		ret |= imx386_write_reg(imx386->client, IMX386_REG_DGAIN,
					IMX386_REG_VALUE_08BIT,
					IMX386_DGAIN_MODE);
		if (IMX386_DGAIN_MODE && dgain > 0) {
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_DGAINGR_H,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_H(dgain));
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_DGAINGR_L,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_L(dgain));
		} else if (dgain > 0) {
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_DGAINR_H,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_H(dgain));
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_DGAINR_L,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_L(dgain));
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_DGAINB_H,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_H(dgain));
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_DGAINB_L,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_L(dgain));
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_DGAINGB_H,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_H(dgain));
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_DGAINGB_L,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_L(dgain));
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_GAIN_GLOBAL_H,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_H(dgain));
			ret |= imx386_write_reg(imx386->client,
						IMX386_REG_GAIN_GLOBAL_L,
						IMX386_REG_VALUE_08BIT,
						IMX386_FETCH_DGAIN_L(dgain));
		}
		ret |= imx386_write_reg(imx386->client, IMX386_GROUP_HOLD_REG,
					IMX386_REG_VALUE_08BIT,
					IMX386_GROUP_HOLD_END);
		break;
	case V4L2_CID_VBLANK:
		ret = imx386_write_reg(imx386->client,
				       IMX386_REG_VTS_H,
				       IMX386_REG_VALUE_08BIT,
				       (ctrl->val + imx386->cur_mode->height)
				       >> 8);
		ret |= imx386_write_reg(imx386->client,
					IMX386_REG_VTS_L,
					IMX386_REG_VALUE_08BIT,
					(ctrl->val + imx386->cur_mode->height)
					& 0xff);
		imx386->cur_vts = ctrl->val + imx386->cur_mode->height;
		imx386_modify_fps_info(imx386);
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->val)
			imx386->flip |= IMX386_MIRROR_BIT_MASK;
		else
			imx386->flip &= ~IMX386_MIRROR_BIT_MASK;
		break;
	case V4L2_CID_VFLIP:
		if (ctrl->val)
			imx386->flip |= IMX386_FLIP_BIT_MASK;
		else
			imx386->flip &= ~IMX386_FLIP_BIT_MASK;
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = imx386_enable_test_pattern(imx386, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx386_ctrl_ops = {
	.s_ctrl = imx386_set_ctrl,
};

static int imx386_initialize_controls(struct imx386 *imx386)
{
	const struct imx386_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &imx386->ctrl_handler;
	mode = imx386->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &imx386->mutex;

	imx386->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
						   V4L2_CID_LINK_FREQ,
						   ARRAY_SIZE(link_freq_menu_items) - 1, 0, link_freq_menu_items);

	imx386->cur_link_freq = mode->link_freq_idx;
	imx386->cur_pixel_rate = link_freq_menu_items[mode->link_freq_idx] *
		2 / mode->bpp * IMX386_LANES;

	imx386->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
					       V4L2_CID_PIXEL_RATE,
					       0, PIXEL_RATE_WITH_800M_10BIT,
					       1, imx386->cur_pixel_rate);
	v4l2_ctrl_s_ctrl(imx386->link_freq,
			   imx386->cur_link_freq);

	h_blank = mode->hts_def - mode->width;
	imx386->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (imx386->hblank)
		imx386->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx386->vblank = v4l2_ctrl_new_std(handler, &imx386_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   IMX386_VTS_MAX - mode->height,
					   1, vblank_def);
	imx386->cur_vts = mode->vts_def;
	exposure_max = mode->vts_def - 4;
	imx386->exposure = v4l2_ctrl_new_std(handler, &imx386_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX386_EXPOSURE_MIN,
					     exposure_max,
					     IMX386_EXPOSURE_STEP,
					     mode->exp_def);
	imx386->anal_gain = v4l2_ctrl_new_std(handler, &imx386_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN,
					      IMX386_GAIN_MIN,
					      IMX386_GAIN_MAX,
					      IMX386_GAIN_STEP,
					      IMX386_GAIN_DEFAULT);
	imx386->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							    &imx386_ctrl_ops,
				V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(imx386_test_pattern_menu) - 1,
				0, 0, imx386_test_pattern_menu);

	imx386->h_flip = v4l2_ctrl_new_std(handler, &imx386_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);

	imx386->v_flip = v4l2_ctrl_new_std(handler, &imx386_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	imx386->flip = 0;

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx386->client->dev,
			"Failed to init controls(  %d  )\n", ret);
		goto err_free_handler;
	}

	imx386->subdev.ctrl_handler = handler;
	imx386->has_init_exp = false;
	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int imx386_check_sensor_id(struct imx386 *imx386,
				  struct i2c_client *client)
{
	struct device *dev = &imx386->client->dev;
	u16 id = 0;
	u32 reg_H = 0;
	u32 reg_L = 0;
	int ret;

	ret = imx386_read_reg(client, IMX386_REG_CHIP_ID_H,
			      IMX386_REG_VALUE_08BIT, &reg_H);
	ret |= imx386_read_reg(client, IMX386_REG_CHIP_ID_L,
			       IMX386_REG_VALUE_08BIT, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (!(reg_H == (CHIP_ID >> 8) || reg_L == (CHIP_ID & 0xff))) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected imx386 %04x sensor\n", id);
	return 0;
}

static int imx386_configure_regulators(struct imx386 *imx386)
{
	unsigned int i;

	for (i = 0; i < IMX386_NUM_SUPPLIES; i++)
		imx386->supplies[i].supply = imx386_supply_names[i];

	return devm_regulator_bulk_get(&imx386->client->dev,
				       IMX386_NUM_SUPPLIES,
				       imx386->supplies);
}

static int imx386_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx386 *imx386;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	imx386 = devm_kzalloc(dev, sizeof(*imx386), GFP_KERNEL);
	if (!imx386)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx386->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx386->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx386->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx386->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	imx386->client = client;
	imx386->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < imx386->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			imx386->cur_mode = &supported_modes[i];
			break;
		}
	}

	if (i == imx386->cfg_num)
		imx386->cur_mode = &supported_modes[0];

	imx386->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx386->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx386->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx386->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	imx386->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(imx386->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = imx386_configure_regulators(imx386);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&imx386->mutex);

	sd = &imx386->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx386_subdev_ops);

	ret = imx386_initialize_controls(imx386);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx386_power_on(imx386);
	if (ret)
		goto err_free_handler;

	ret = imx386_check_sensor_id(imx386, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx386_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx386->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx386->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(imx386->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx386->module_index, facing,
		 IMX386_NAME, dev_name(sd->dev));
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
	__imx386_power_off(imx386);
err_free_handler:
	v4l2_ctrl_handler_free(&imx386->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx386->mutex);

	return ret;
}

static void imx386_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx386 *imx386 = to_imx386(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx386->ctrl_handler);
	mutex_destroy(&imx386->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx386_power_off(imx386);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx386_of_match[] = {
	{ .compatible = "sony,imx386" },
	{},
};
MODULE_DEVICE_TABLE(of, imx386_of_match);
#endif

static const struct i2c_device_id imx386_match_id[] = {
	{ "sony,imx386", 0 },
	{ },
};

static struct i2c_driver imx386_i2c_driver = {
	.driver = {
		.name = IMX386_NAME,
		.pm = &imx386_pm_ops,
		.of_match_table = of_match_ptr(imx386_of_match),
	},
	.probe		= &imx386_probe,
	.remove		= &imx386_remove,
	.id_table	= imx386_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx386_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx386_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx386 sensor driver");
MODULE_LICENSE("GPL");
