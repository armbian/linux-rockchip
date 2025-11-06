// SPDX-License-Identifier: GPL-2.0
/*
 * imx900 driver
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 * Sach.lin@rock-chips.com
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
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/rk-preisp.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

/* link frequency */
#define MIPI_FREQ_2376M			(2376000000/2)

#define IMX900_4LANES			4
#define IMX900_BITS_PER_PIXEL	10

/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define IMX900_MAX_PIXEL_RATE		(MIPI_FREQ_2376M / IMX900_BITS_PER_PIXEL * 2 * IMX900_4LANES)

#define IMX900_XVCLK_FREQ_37M		37125000

/* TODO: Get the real chip id from reg */
#define CHIP_ID				0x3B
#define IMX900_REG_CHIP_ID		0x3014

#define IMX900_REG_CTRL_MODE		0x3000
#define IMX900_MODE_SW_STANDBY		BIT(0)
#define IMX900_MODE_STREAMING		0x0

#define IMX900_REG_CTRL_XMSTA		0x3010
#define IMX900_MODE_XMSTA_STOP		BIT(0)
#define IMX900_MODE_XMSTA_START		0x0

#define IMX900_GAIN_REG_H		0x3515
#define IMX900_GAIN_REG_L		0x3514

#define IMX900_EXPO_REG_H		0x3242
#define IMX900_EXPO_REG_M		0x3241
#define IMX900_EXPO_REG_L		0x3240

#define IMX900_VTS_MAX			0x7fff
#define IMX900_VTS_DEFAULT		0x0006CF
#define IMX900_WAIT_TIME_GMTWT		0x40
#define IMX900_WAIT_TIME_GMRWT2		0x15
#define	IMX900_SHUTTER_TIME_MIN		(IMX900_VTS_DEFAULT - 1)
#define	IMX900_SHUTTER_TIME_MAX		(IMX900_WAIT_TIME_GMTWT + IMX900_WAIT_TIME_GMRWT2)
#define	IMX900_EXPOSURE_MIN			(IMX900_VTS_DEFAULT - IMX900_SHUTTER_TIME_MIN)
#define	IMX900_EXPOSURE_MAX			(IMX900_VTS_DEFAULT - IMX900_SHUTTER_TIME_MAX)
#define	IMX900_EXPOSURE_DEFAULT		(IMX900_VTS_DEFAULT - IMX900_SHUTTER_TIME_MIN)
#define	IMX900_EXPOSURE_STEP		1

#define IMX900_HTS_DEFAULT		0x016C

#define IMX900_GAIN_MIN			0x00
#define IMX900_GAIN_MAX			0x01E0
#define IMX900_GAIN_STEP		1
#define IMX900_GAIN_DEFAULT		0x0000

#define IMX900_FETCH_GAIN_H(VAL)	(((VAL) >> 8) & 0x01)
#define IMX900_FETCH_GAIN_L(VAL)	((VAL) & 0xFF)

#define IMX900_FETCH_EXP_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX900_FETCH_EXP_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX900_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define IMX900_FETCH_VTS_H(VAL)		(((VAL) >> 16) & 0x0F)
#define IMX900_FETCH_VTS_M(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX900_FETCH_VTS_L(VAL)		((VAL) & 0xFF)

#define IMX900_FETCH_HTS_H(VAL)		(((VAL) >> 8) & 0x0F)
#define IMX900_FETCH_HTS_L(VAL)		((VAL) & 0xFF)

#define IMX900_VTS_REG_H		0x30D6
#define IMX900_VTS_REG_M		0x30D5
#define IMX900_VTS_REG_L		0x30D4

#define IMX900_MIRROR_BIT_MASK		BIT(1)
#define IMX900_FLIP_BIT_MASK		BIT(0)
#define IMX900_FLIP_REG			0x3204

#define REG_NULL			0xFFFF

#define IMX900_REG_VALUE_08BIT		1

#define IMX900_GROUP_HOLD_REG		0x30F8
#define IMX900_GROUP_HOLD_START		0x01
#define IMX900_GROUP_HOLD_END		0x00

#define IMX900_SENSOR_INFO_H	0x3817
#define IMX900_SENSOR_INFO_L	0x3816

/* Basic Readout Lines. Number of necessary readout lines in sensor */
#define BRL_ALL_PIXEL_SCAN_MODE     2228u

#define OF_CAMERA_HDR_MODE		        "rockchip,camera-hdr-mode"
#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define IMX900_NAME			"imx900"

static const char *const imx900_supply_names[] = {
	"dvdd",			/* Digital core power */
	"dovdd",		/* Digital I/O power */
	"avdd",			/* Analog power */
};

#define IMX900_NUM_SUPPLIES ARRAY_SIZE(imx900_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct imx900_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	u32 bpp;
	const struct regval *global_reg_list;
	const struct regval *reg_list;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct imx900 {
	struct i2c_client *client;
	struct clk *xvclk;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *power_gpio;
	struct regulator_bulk_data supplies[IMX900_NUM_SUPPLIES];

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_sleep;

	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *anal_a_gain;
	struct v4l2_ctrl *digi_gain;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	struct mutex mutex;
	bool streaming;
	bool power_on;
	bool is_thunderboot;
	bool is_thunderboot_ng;
	bool is_first_streamoff;
	const struct imx900_mode *cur_mode;
	u32 module_index;
	u32 cfg_num;
	const char *module_facing;
	const char *module_name;
	const char *len_name;
	u32 cur_vts;
	bool has_init_exp;
	struct preisp_hdrae_exp_s init_hdrae_exp;
};

#define to_imx900(sd) container_of(sd, struct imx900, subdev)

static __maybe_unused const struct regval imx900_globa_regs[] = {
	{ REG_NULL, 0x00 },
};

/*
 * Xclk 37.125MHz
 */
static __maybe_unused const struct regval
	imx900_color_linear_10bit_2064x1552_2376M_117fps_regs[] = {
	{ 0x3014, 0x1E },
	{ 0x3015, 0x92 },
	{ 0x3016, 0xE0 },
	{ 0x3017, 0x01 },
	{ 0x3018, 0xB6 },
	{ 0x3019, 0x00 },
	{ 0x301C, 0xB6 },
	{ 0x301D, 0x00 },
	{ 0x303A, 0x05 },
	{ 0x303C, 0x00 },
	{ 0x30D4, 0xCF },	/* VMAX 0x6CF */
	{ 0x30D5, 0x06 },
	{ 0x30D6, 0x00 },
	{ 0x30D8, 0x6C },	/* HMAX 0x16C  */
	{ 0x30D9, 0x01 },
	{ 0x30E2, 0x1C },
	{ 0x30E3, 0x40 },
	{ 0x30E5, 0x02 },
	{ 0x30E6, 0x01 },

	{ 0x3200, 0x01 },	/* bit0 Keep 01b */
	{ 0x323C, 0x07 },
	{ 0x3240, 0x55 },
	{ 0x3241, 0x00 },
	{ 0x3242, 0x00 },
	{ 0x32B6, 0x3A },

	{ 0x3312, 0x39 },

	{ 0x3400, 0x00 },
	{ 0x3430, 0x00 },
	{ 0x34D4, 0x78 },
	{ 0x34D5, 0x27 },
	{ 0x34D8, 0xA9 },
	{ 0x34D9, 0x5A },
	{ 0x34F9, 0x12 },

	{ 0x3502, 0x08 },
	{ 0x3514, 0x00 },	/* GAIN=0x00 (analog gain=0dB) */
	{ 0x3515, 0x00 },
	{ 0x3528, 0x00 },
	{ 0x352A, 0x00 },
	{ 0x352C, 0x00 },
	{ 0x352E, 0x00 },
	{ 0x3542, 0x03 },
	{ 0x3549, 0x2A },
	{ 0x354A, 0x20 },
	{ 0x354B, 0x0C },
	{ 0x359C, 0x19 },
	{ 0x359E, 0x3F },
	{ 0x35B4, 0x3C },
	{ 0x35B5, 0x00 },
	{ 0x35EA, 0xF0 },
	{ 0x35F4, 0x03 },
	{ 0x35F8, 0x01 },

	{ 0x3600, 0x00 },
	{ 0x3614, 0x00 },
	{ 0x362A, 0xEC },
	{ 0x362B, 0x1F },
	{ 0x362E, 0xF8 },
	{ 0x362F, 0x1F },
	{ 0x3630, 0x5C },
	{ 0x3648, 0xC6 },
	{ 0x364A, 0xEC },
	{ 0x364B, 0x1F },
	{ 0x364C, 0xDE },
	{ 0x364E, 0xF8 },
	{ 0x364F, 0x1F },
	{ 0x3652, 0xEC },
	{ 0x3653, 0x1F },
	{ 0x3656, 0xF8 },
	{ 0x3657, 0x1F },
	{ 0x3658, 0x5C },
	{ 0x3670, 0xC6 },
	{ 0x3672, 0xEC },
	{ 0x3673, 0x1F },
	{ 0x3674, 0xDE },
	{ 0x3676, 0xF8 },
	{ 0x3677, 0x1F },
	{ 0x367A, 0xEC },
	{ 0x367B, 0x1F },
	{ 0x367E, 0xF8 },
	{ 0x367F, 0x1F },
	{ 0x3698, 0xC6 },
	{ 0x369A, 0xEC },
	{ 0x369B, 0x1F },
	{ 0x369C, 0xDE },
	{ 0x369E, 0xF8 },
	{ 0x369F, 0x1F },
	{ 0x36A8, 0x1C },
	{ 0x36A9, 0x31 },
	{ 0x36B0, 0x28 },
	{ 0x36B1, 0x00 },
	{ 0x36B2, 0xF8 },
	{ 0x36B3, 0x1F },
	{ 0x36BC, 0x28 },
	{ 0x36BD, 0x00 },
	{ 0x36BE, 0xF8 },
	{ 0x36BF, 0x1F },
	{ 0x36D4, 0xEF },
	{ 0x36D5, 0x01 },
	{ 0x36D6, 0x94 },
	{ 0x36D7, 0x03 },
	{ 0x36D8, 0xEF },
	{ 0x36D9, 0x01 },
	{ 0x36DA, 0x94 },
	{ 0x36DB, 0x03 },
	{ 0x36DC, 0x9B },
	{ 0x36DD, 0x09 },
	{ 0x36DE, 0x57 },
	{ 0x36DF, 0x11 },
	{ 0x36E0, 0xEB },
	{ 0x36E1, 0x17 },
	{ 0x36E2, 0x15 },
	{ 0x36E3, 0x27 },

	{ 0x37AC, 0x0E },
	{ 0x37AE, 0x14 },

	{ 0x38E8, 0x82 },

	{ 0x3C98, 0x80 },
	{ 0x3C99, 0x09 },
	{ 0x3CA3, 0x01 },

	{ 0x4100, 0x02 },
	{ 0x4101, 0x07 },
	{ 0x4110, 0x02 },
	{ 0x4111, 0x08 },
	{ 0x4112, 0x0C },
	{ 0x4116, 0xD8 },

	{ 0x5032, 0xFF },
	{ 0x5038, 0x00 },
	{ 0x5039, 0x00 },
	{ 0x503A, 0xF6 },
	{ 0x505C, 0x96 },
	{ 0x505D, 0x02 },
	{ 0x505E, 0x96 },
	{ 0x505F, 0x02 },
	{ 0x5078, 0x09 },
	{ 0x507B, 0x11 },
	{ 0x507C, 0xFF },

	{ 0x531C, 0x48 },
	{ 0x531E, 0x52 },
	{ 0x5320, 0x48 },
	{ 0x5322, 0x52 },
	{ 0x5324, 0x48 },
	{ 0x5326, 0x52 },
	{ 0x5328, 0x48 },
	{ 0x532A, 0x52 },
	{ 0x532C, 0x48 },
	{ 0x532E, 0x52 },
	{ 0x5330, 0x48 },
	{ 0x5332, 0x52 },
	{ 0x5334, 0x48 },
	{ 0x5336, 0x52 },
	{ 0x5338, 0x48 },
	{ 0x533A, 0x52 },

	{ 0x54D0, 0x40 },
	{ 0x54D1, 0x01 },
	{ 0x54D2, 0x81 },
	{ 0x54D3, 0x01 },
	{ 0x54D4, 0x15 },
	{ 0x54D5, 0x01 },
	{ 0x54D6, 0x00 },

	{ 0x5545, 0xA7 },
	{ 0x5546, 0x14 },
	{ 0x5547, 0x14 },
	{ 0x5548, 0x14 },
	{ 0x5550, 0x0A },
	{ 0x5551, 0x0A },
	{ 0x5552, 0x0A },
	{ 0x5553, 0x6A },
	{ 0x5572, 0x5F },
	{ 0x5589, 0x0E },

	{ 0x5613, 0xAF },
	{ 0x5704, 0x0E },
	{ 0x5705, 0x14 },

	{ 0x5832, 0x54 },
	{ 0x5836, 0x54 },
	{ 0x583A, 0x54 },
	{ 0x583E, 0x54 },
	{ 0x5842, 0x54 },
	{ 0x5846, 0x54 },
	{ 0x584A, 0x54 },
	{ 0x584E, 0x54 },
	{ 0x5852, 0x54 },
	{ 0x5856, 0x54 },
	{ 0x585A, 0x54 },
	{ 0x585E, 0x54 },
	{ 0x5862, 0x54 },
	{ 0x5866, 0x54 },
	{ 0x586A, 0x54 },
	{ 0x586E, 0x54 },
	{ 0x5872, 0x54 },
	{ 0x5876, 0x54 },
	{ 0x587A, 0x54 },
	{ 0x587E, 0x54 },
	{ 0x5882, 0x54 },
	{ 0x5886, 0x54 },
	{ 0x588A, 0x54 },
	{ 0x588E, 0x54 },

	{ 0x5902, 0xB0 },
	{ 0x5903, 0x04 },
	{ 0x590A, 0xB0 },
	{ 0x590B, 0x04 },
	{ 0x590C, 0xB0 },
	{ 0x590D, 0x09 },
	{ 0x590E, 0xC4 },
	{ 0x590F, 0x09 },
	{ 0x5934, 0x96 },
	{ 0x5935, 0x02 },
	{ 0x5936, 0x96 },
	{ 0x5937, 0x02 },
	{ 0x5939, 0x08 },
	{ 0x59AC, 0x00 },
	{ 0x59AE, 0x56 },
	{ 0x59AF, 0x01 },
	{ 0x59C1, 0x00 },
	{ 0x59D4, 0x00 },

	{ 0x5B4D, 0x24 },
	{ 0x5B81, 0x36 },
	{ 0x5BB5, 0x09 },
	{ 0x5BB8, 0x5C },
	{ 0x5BBA, 0x3A },
	{ 0x5BBC, 0xC5 },
	{ 0x5BBD, 0x00 },
	{ 0x5BBE, 0x0B },
	{ 0x5BBF, 0x02 },
	{ 0x5BC0, 0x74 },
	{ 0x5BC1, 0x02 },
	{ 0x5BC2, 0x90 },
	{ 0x5BC3, 0x01 },
	{ 0x5BC9, 0x11 },
	{ 0x5BCC, 0x00 },
	{ 0x5BD8, 0x00 },
	{ 0x5BD9, 0x00 },
	{ 0x5BDC, 0x1D },
	{ 0x5BDD, 0x00 },
	{ 0x5BE0, 0x1E },
	{ 0x5BE1, 0x00 },
	{ 0x5BE4, 0x3B },
	{ 0x5BE5, 0x00 },
	{ 0x5BE8, 0x3C },
	{ 0x5BE9, 0x00 },
	{ 0x5BEC, 0x59 },
	{ 0x5BED, 0x00 },
	{ 0x5BF0, 0x5A },
	{ 0x5BF1, 0x00 },
	{ 0x5BF4, 0x77 },
	{ 0x5BF5, 0x00 },

	{ 0x5C00, 0x00 },

	{ 0x5E04, 0x13 },
	{ 0x5E05, 0x05 },
	{ 0x5E06, 0x02 },
	{ 0x5E07, 0x00 },
	{ 0x5E14, 0x14 },
	{ 0x5E15, 0x05 },
	{ 0x5E16, 0x01 },
	{ 0x5E17, 0x00 },
	{ 0x5E34, 0x08 },
	{ 0x5E35, 0x05 },
	{ 0x5E36, 0x02 },
	{ 0x5E37, 0x00 },
	{ 0x5E44, 0x09 },
	{ 0x5E45, 0x05 },
	{ 0x5E46, 0x01 },
	{ 0x5E47, 0x00 },
	{ 0x5E98, 0x7C },
	{ 0x5E99, 0x09 },
	{ 0x5EB8, 0x7E },
	{ 0x5EB9, 0x09 },
	{ 0x5EC8, 0x18 },
	{ 0x5EC9, 0x09 },
	{ 0x5ECA, 0xE8 },
	{ 0x5ECB, 0x03 },
	{ 0x5ED8, 0x1A },
	{ 0x5ED9, 0x09 },
	{ 0x5EDA, 0xE6 },
	{ 0x5EDB, 0x03 },

	{ 0x5F08, 0x18 },
	{ 0x5F09, 0x09 },
	{ 0x5F0A, 0xE8 },
	{ 0x5F0B, 0x03 },
	{ 0x5F18, 0x1A },
	{ 0x5F19, 0x09 },
	{ 0x5F1A, 0xE6 },
	{ 0x5F1B, 0x03 },
	{ 0x5F38, 0x18 },
	{ 0x5F39, 0x09 },
	{ 0x5F3A, 0xE8 },
	{ 0x5F3B, 0x03 },
	{ 0x5F48, 0x1A },
	{ 0x5F49, 0x09 },
	{ 0x5F4A, 0xE6 },
	{ 0x5F4B, 0x03 },
	{ 0x5F68, 0x18 },
	{ 0x5F69, 0x09 },
	{ 0x5F6A, 0xE8 },
	{ 0x5F6B, 0x03 },
	{ 0x5F78, 0x1A },
	{ 0x5F79, 0x09 },
	{ 0x5F7A, 0xE6 },
	{ 0x5F7B, 0x03 },

	{ 0x60B4, 0x1E },
	{ 0x60C0, 0x1F },

	{ 0x6178, 0x7C },
	{ 0x6179, 0x09 },
	{ 0x6198, 0x7E },
	{ 0x6199, 0x09 },
	{ 0x6278, 0x18 },
	{ 0x6279, 0x09 },
	{ 0x627A, 0xE8 },
	{ 0x627B, 0x03 },
	{ 0x6288, 0x1A },
	{ 0x6289, 0x09 },
	{ 0x628A, 0xE6 },
	{ 0x628B, 0x03 },
	{ 0x62A8, 0x18 },
	{ 0x62A9, 0x09 },
	{ 0x62AA, 0xE8 },
	{ 0x62AB, 0x03 },
	{ 0x62B8, 0x1A },
	{ 0x62B9, 0x09 },
	{ 0x62BA, 0xE6 },
	{ 0x62BB, 0x03 },
	{ 0x62D8, 0x18 },
	{ 0x62D9, 0x09 },
	{ 0x62DA, 0xE8 },
	{ 0x62DB, 0x03 },
	{ 0x62E8, 0x1A },
	{ 0x62E9, 0x09 },
	{ 0x62EA, 0xE6 },
	{ 0x62EB, 0x03 },

	{ 0x6318, 0x18 },
	{ 0x6319, 0x09 },
	{ 0x631A, 0xE8 },
	{ 0x631B, 0x03 },
	{ 0x6328, 0x1A },
	{ 0x6329, 0x09 },
	{ 0x632A, 0xE6 },
	{ 0x632B, 0x03 },
	{ 0x6398, 0x1E },
	{ 0x63A4, 0x1F },

	{ 0x6501, 0x01 },
	{ 0x6505, 0x00 },
	{ 0x6508, 0x00 },
	{ 0x650C, 0x01 },
	{ 0x6510, 0x00 },
	{ 0x6514, 0x01 },
	{ 0x6519, 0x01 },
	{ 0x651D, 0x00 },
	{ 0x6528, 0x00 },
	{ 0x652C, 0x01 },
	{ 0x6531, 0x01 },
	{ 0x6535, 0x00 },
	{ 0x6538, 0x00 },
	{ 0x653C, 0x01 },
	{ 0x6541, 0x01 },
	{ 0x6545, 0x00 },
	{ 0x6549, 0x01 },
	{ 0x654D, 0x00 },
	{ 0x6558, 0x00 },
	{ 0x655C, 0x01 },
	{ 0x6560, 0x00 },
	{ 0x6564, 0x01 },
	{ 0x6571, 0x01 },
	{ 0x6575, 0x00 },
	{ 0x6579, 0x01 },
	{ 0x657D, 0x00 },
	{ 0x6588, 0x00 },
	{ 0x658C, 0x01 },
	{ 0x6590, 0x00 },
	{ 0x6594, 0x01 },
	{ 0x6598, 0x00 },
	{ 0x659C, 0x01 },
	{ 0x65A0, 0x00 },
	{ 0x65A4, 0x01 },
	{ 0x65B0, 0x00 },
	{ 0x65B4, 0x01 },
	{ 0x65B9, 0x00 },
	{ 0x65BD, 0x00 },
	{ 0x65C1, 0x00 },
	{ 0x65C9, 0x00 },
	{ 0x65CC, 0x00 },
	{ 0x65D0, 0x00 },
	{ 0x65D4, 0x00 },
	{ 0x65DC, 0x00 },
	{ REG_NULL, 0x00 },
};

/*
 * The width and height must be configured to be
 * the same as the current output resolution of the sensor.
 * The input width of the isp needs to be 16 aligned.
 * The input height of the isp needs to be 8 aligned.
 * If the width or height does not meet the alignment rules,
 * you can configure the cropping parameters with the following function to
 * crop out the appropriate resolution.
 * struct v4l2_subdev_pad_ops {
 *	.get_selection
 * }
 */
static const struct imx900_mode supported_modes[] = {
	/*
	 * frame rate = 1 / (Vtt * 1H) = 1 / (VMAX * 1H)
	 * VMAX >= (PIX_VWIDTH / 2) + 46 = height + 46
	 */
	{
	 .bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
	 .width = 2064,
	 .height = 1552,
	 .max_fps = {
		     .numerator = 10000,
		     .denominator = 1170000,
		     },
	 .exp_def = IMX900_EXPOSURE_DEFAULT,
	 .hts_def = IMX900_HTS_DEFAULT * IMX900_4LANES * 2,
	 .vts_def = IMX900_VTS_DEFAULT,
	 .global_reg_list = imx900_globa_regs,
	 .reg_list = imx900_color_linear_10bit_2064x1552_2376M_117fps_regs,
	 .hdr_mode = NO_HDR,
	 .mipi_freq_idx = 0,
	 .bpp = IMX900_BITS_PER_PIXEL,
	 .vc[PAD0] = 0,
	  },
};

static const s64 link_freq_items[] = {
	MIPI_FREQ_2376M,
};

/* Write registers up to 4 at a time */
static int imx900_write_reg(struct i2c_client *client, u16 reg,
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

static int imx900_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		ret = imx900_write_reg(client, regs[i].addr,
				       IMX900_REG_VALUE_08BIT, regs[i].val);
	}
	return ret;
}

/* Read registers up to 4 at a time */
static int imx900_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int imx900_get_reso_dist(const struct imx900_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	    abs(mode->height - framefmt->height);
}

static const struct imx900_mode *imx900_find_best_fit(struct imx900 *imx900,
						      struct v4l2_subdev_format
						      *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < imx900->cfg_num; i++) {
		dist = imx900_get_reso_dist(&supported_modes[i], framefmt);
		if ((cur_best_fit_dist == -1 || dist < cur_best_fit_dist) &&
		    supported_modes[i].bus_fmt == framefmt->code) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}
	dev_info(&imx900->client->dev, "%s: cur_best_fit(%d)",
		 __func__, cur_best_fit);

	return &supported_modes[cur_best_fit];
}

static int __imx900_power_on(struct imx900 *imx900);

static void imx900_change_mode(struct imx900 *imx900,
			       const struct imx900_mode *mode)
{
	if (imx900->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
		imx900->is_thunderboot = false;
		imx900->is_thunderboot_ng = true;
		__imx900_power_on(imx900);
	}
	imx900->cur_mode = mode;
	imx900->cur_vts = imx900->cur_mode->vts_def;
}

static int imx900_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx900 *imx900 = to_imx900(sd);
	const struct imx900_mode *mode;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;

	mutex_lock(&imx900->mutex);

	mode = imx900_find_best_fit(imx900, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	dev_info(&imx900->client->dev,
		 "set format bus_fmt=%x,width=%d,height=%d\n", fmt->format.code,
		 fmt->format.width, fmt->format.height);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx900->mutex);
		return -ENOTTY;
#endif
	} else {
		imx900_change_mode(imx900, mode);
		dev_info(&imx900->client->dev,
			 "set mode: cur_mode: %dx%d, hdr: %d\n", mode->width,
			 mode->height, mode->hdr_mode);
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx900->hblank, h_blank,
					 h_blank, 1, h_blank);
		dev_info(&imx900->client->dev,
			 "%s: set h_blank range min=%lld,max=%lld,default=%lld\n",
			 __func__, h_blank, h_blank, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(imx900->vblank, vblank_def,
					 IMX900_VTS_MAX - mode->height,
					 1, vblank_def);
		dev_info(&imx900->client->dev,
			 "%s: set v_blank range min=%lld,max=%d,default=%lld)\n",
			 __func__, vblank_def, (IMX900_VTS_MAX - mode->height),
			 vblank_def);
		__v4l2_ctrl_s_ctrl(imx900->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(imx900->link_freq, mode->mipi_freq_idx);
		pixel_rate =
		    (u32) link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 *
		    IMX900_4LANES;
		__v4l2_ctrl_s_ctrl_int64(imx900->pixel_rate, pixel_rate);
		dev_info(&imx900->client->dev,
			 "%s: set link_freq(%lld),mipi_freq_idx(%d)\n",
			 __func__, link_freq_items[mode->mipi_freq_idx],
			 mode->mipi_freq_idx);
	}

	mutex_unlock(&imx900->mutex);

	return 0;
}

static int imx900_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx900 *imx900 = to_imx900(sd);
	const struct imx900_mode *mode = imx900->cur_mode;

	mutex_lock(&imx900->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&imx900->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&imx900->mutex);

	return 0;
}

static int imx900_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx900 *imx900 = to_imx900(sd);

	if (code->index >= imx900->cfg_num)
		return -EINVAL;

	code->code = supported_modes[code->index].bus_fmt;

	return 0;
}

static int imx900_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx900 *imx900 = to_imx900(sd);

	if (fse->index >= imx900->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[fse->index].bus_fmt)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int imx900_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx900 *imx900 = to_imx900(sd);
	const struct imx900_mode *mode = imx900->cur_mode;

	fi->interval = mode->max_fps;

	dev_info(&imx900->client->dev, "%s: max_fps=%d / %d\n",
		 __func__, fi->interval.numerator, fi->interval.denominator);

	return 0;
}

static int imx900_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = IMX900_4LANES;

	return 0;
}

static void imx900_get_module_inf(struct imx900 *imx900,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, IMX900_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, imx900->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, imx900->len_name, sizeof(inf->base.lens));
}

static int imx900_get_channel_info(struct imx900 *imx900,
				   struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = imx900->cur_mode->vc[ch_info->index];
	ch_info->width = imx900->cur_mode->width;
	ch_info->height = imx900->cur_mode->height;
	ch_info->bus_fmt = imx900->cur_mode->bus_fmt;
	return 0;
}

static long imx900_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx900 *imx900 = to_imx900(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	u32 stream;
	long ret = 0;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		dev_info(&imx900->client->dev, "set hdr ae expoosure\n");
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx900_get_module_inf(imx900, (struct rkmodule_inf *)arg);
		dev_info(&imx900->client->dev, "get module info\n");
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = imx900->cur_mode->hdr_mode;
		dev_info(&imx900->client->dev, "get hdr config\n");
		break;
	case RKMODULE_SET_HDR_CFG:
		dev_info(&imx900->client->dev, "set hdr config\n");
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *) arg);

		if (stream)
			ret =
			    imx900_write_reg(imx900->client,
					     IMX900_REG_CTRL_MODE,
					     IMX900_REG_VALUE_08BIT,
					     IMX900_MODE_STREAMING);
		else
			ret =
			    imx900_write_reg(imx900->client,
					     IMX900_REG_CTRL_MODE,
					     IMX900_REG_VALUE_08BIT,
					     IMX900_MODE_SW_STANDBY);
		dev_info(&imx900->client->dev, "set quick sstream=%d\n",
			 stream);
		break;
	case RKMODULE_GET_SONY_BRL:
		*((u32 *) arg) = BRL_ALL_PIXEL_SCAN_MODE;
		dev_info(&imx900->client->dev,
			 "get sony BRL=%d\n", *((u32 *) arg));
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = imx900_get_channel_info(imx900, ch_info);
		dev_info(&imx900->client->dev, "get channel info\n");
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		ret = -EINVAL;
		dev_info(&imx900->client->dev, "get csi dphy parame\n");
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx900_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	struct rkmodule_channel_info *ch_info;
	long ret;
	u32 stream;
	u32 brl = 0;
	struct rkmodule_csi_dphy_param *dphy_param;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx900_ioctl(sd, cmd, inf);
		if (!ret) {
			if (copy_to_user(up, inf, sizeof(*inf))) {
				kfree(inf);
				return -EFAULT;
			}
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		if (copy_from_user(cfg, up, sizeof(*cfg))) {
			kfree(cfg);
			return -EFAULT;
		}
		ret = imx900_ioctl(sd, cmd, cfg);
		kfree(cfg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx900_ioctl(sd, cmd, hdr);
		if (!ret) {
			if (copy_to_user(up, hdr, sizeof(*hdr))) {
				kfree(hdr);
				return -EFAULT;
			}
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
		ret = imx900_ioctl(sd, cmd, hdr);
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
		ret = imx900_ioctl(sd, cmd, hdrae);
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		if (copy_from_user(&stream, up, sizeof(u32)))
			return -EFAULT;
		ret = imx900_ioctl(sd, cmd, &stream);
		break;
	case RKMODULE_GET_SONY_BRL:
		ret = imx900_ioctl(sd, cmd, &brl);
		if (!ret) {
			if (copy_to_user(up, &brl, sizeof(u32)))
				return -EFAULT;
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx900_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx900_ioctl(sd, cmd, dphy_param);
		if (!ret) {
			ret = copy_to_user(up, dphy_param, sizeof(*dphy_param));
			if (ret)
				ret = -EFAULT;
		}
		kfree(dphy_param);
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

// sw_test_002
static int imx900_read_reg_verify(struct i2c_client *client,
				  const struct regval *regs)
{
	u32 i;
	int ret = 0;
	u32 val;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++) {
		ret = imx900_read_reg(client, regs[i].addr,
				      IMX900_REG_VALUE_08BIT, &val);
		if (ret != 0) {
			dev_info(&client->dev,
				 "%s: fail register read addr=%#x reg=%#02x %#02x\n",
				 __func__, regs[i].addr, regs[i].val, val);
			return ret;
		}
		if (val != regs[i].val) {
			dev_info(&client->dev,
				 "%s: read data unmatched addr=%#x reg=%#02x %#02x\n",
				 __func__, regs[i].addr, regs[i].val, val);
			return ret;
		}
		dev_info(&client->dev, "%s: {0x%04hX,0x%02hX},\n",
			 __func__, regs[i].addr, val);
	}
	return ret;
}

// sw_test_020
static int imx900_read_sensor_information(struct i2c_client *client)
{
	int ret;
	u32 val = 0xff;

	ret =
	    imx900_read_reg(client, IMX900_SENSOR_INFO_L,
			    IMX900_REG_VALUE_08BIT, &val);
	dev_info(&client->dev, "%s: sensor information reg(0x%04hX)=0x%02hX\n",
		 __func__, IMX900_SENSOR_INFO_L, val);
	ret |=
	    imx900_read_reg(client, IMX900_SENSOR_INFO_H,
			    IMX900_REG_VALUE_08BIT, &val);
	dev_info(&client->dev, "%s: sensor information reg(0x%04hX)=0x%02hX\n",
		 __func__, IMX900_SENSOR_INFO_H, val);

	return ret;
}

static int __imx900_start_stream(struct imx900 *imx900)
{
	int ret;
	u32 val = 0xff;

	if (!imx900->is_thunderboot) {
		// sw_test_001
		ret = imx900_read_reg(imx900->client, IMX900_REG_CTRL_MODE,
				      IMX900_REG_VALUE_08BIT, &val);
		dev_info(&imx900->client->dev, "%s: Read STANDBY reg=%x\n",
			 __func__, val);
		ret |=
		    imx900_write_array(imx900->client,
				       imx900->cur_mode->global_reg_list);
		if (ret)
			return ret;
		ret =
		    imx900_write_array(imx900->client,
				       imx900->cur_mode->reg_list);
		// sw_test_001
		dev_info(&imx900->client->dev, "%s: is_thunderboot=%d\n",
			 __func__, imx900->is_thunderboot);
		if (ret)
			return ret;
	}
	// sw_test_002
	imx900_read_reg_verify(imx900->client, imx900->cur_mode->reg_list);

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx900->ctrl_handler);
	if (ret)
		return ret;
	if (imx900->has_init_exp && imx900->cur_mode->hdr_mode != NO_HDR) {
		ret = imx900_ioctl(&imx900->subdev, PREISP_CMD_SET_HDRAE_EXP,
				   &imx900->init_hdrae_exp);
		if (ret) {
			dev_err(&imx900->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}

	ret |= imx900_write_reg(imx900->client, IMX900_REG_CTRL_XMSTA,
				IMX900_REG_VALUE_08BIT,
				IMX900_MODE_XMSTA_START);

	usleep_range(11 * 1000, 12 * 1000);	/* wait 11ms */

	ret |= imx900_write_reg(imx900->client, IMX900_REG_CTRL_MODE,
				IMX900_REG_VALUE_08BIT, 0);

	// sw_test_020
	ret |= imx900_read_sensor_information(imx900->client);

	return ret;
}

static int __imx900_stop_stream(struct imx900 *imx900)
{
	int ret = 0;

	imx900->has_init_exp = false;
	if (imx900->is_thunderboot)
		imx900->is_first_streamoff = true;

	ret |= imx900_write_reg(imx900->client, IMX900_REG_CTRL_MODE,
				IMX900_REG_VALUE_08BIT, 1);

	usleep_range(10 * 1000, 11 * 1000);	/* wait 10ms */

	ret |= imx900_write_reg(imx900->client, IMX900_REG_CTRL_XMSTA,
				IMX900_REG_VALUE_08BIT, IMX900_MODE_XMSTA_STOP);

	return ret;
}

static int imx900_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx900 *imx900 = to_imx900(sd);
	struct i2c_client *client = imx900->client;
	int ret = 0;

	dev_info(&imx900->client->dev,
		 "s_stream: %d. %dx%d, hdr: %d, bpp: %d\n", on,
		 imx900->cur_mode->width, imx900->cur_mode->height,
		 imx900->cur_mode->hdr_mode, imx900->cur_mode->bpp);

	mutex_lock(&imx900->mutex);
	on = !!on;
	if (on == imx900->streaming)
		goto unlock_and_return;

	if (on) {
		if (imx900->is_thunderboot
		    && rkisp_tb_get_state() == RKISP_TB_NG) {
			imx900->is_thunderboot = false;
			__imx900_power_on(imx900);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx900_start_stream(imx900);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx900_stop_stream(imx900);
		pm_runtime_put(&client->dev);
	}

	imx900->streaming = on;

unlock_and_return:
	mutex_unlock(&imx900->mutex);

	return ret;
}

static int imx900_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx900 *imx900 = to_imx900(sd);
	struct i2c_client *client = imx900->client;
	int ret = 0;

	mutex_lock(&imx900->mutex);

	if (imx900->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		imx900->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx900->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx900->mutex);

	return ret;
}

int __imx900_power_on(struct imx900 *imx900)
{
	int ret;
	struct device *dev = &imx900->client->dev;

	if (imx900->is_thunderboot)
		return 0;

	if (!IS_ERR_OR_NULL(imx900->pins_default)) {
		ret = pinctrl_select_state(imx900->pinctrl,
					   imx900->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}

	ret = regulator_bulk_enable(IMX900_NUM_SUPPLIES, imx900->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto err_pinctrl;
	}
	if (!IS_ERR(imx900->power_gpio))
		gpiod_direction_output(imx900->power_gpio, 1);
	/* At least 500ns between power raising and XCLR */
	/* fix power on timing if insmod this ko */

	usleep_range(10 * 1000, 20 * 1000);
	if (!IS_ERR(imx900->reset_gpio))
		gpiod_direction_output(imx900->reset_gpio, 1);

	/* At least 1us between XCLR and clk */
	/* fix power on timing if insmod this ko */
	usleep_range(10 * 1000, 20 * 1000);
	ret = clk_set_rate(imx900->xvclk, IMX900_XVCLK_FREQ_37M);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate\n");
	if (clk_get_rate(imx900->xvclk) != IMX900_XVCLK_FREQ_37M)
		dev_warn(dev, "xvclk mismatched\n");
	ret = clk_prepare_enable(imx900->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		goto err_clk;
	}

	/* At least 20us between XCLR and I2C communication */
	usleep_range(20 * 1000, 30 * 1000);

	return 0;

err_clk:
	if (!IS_ERR(imx900->reset_gpio))
		gpiod_direction_output(imx900->reset_gpio, 0);
	regulator_bulk_disable(IMX900_NUM_SUPPLIES, imx900->supplies);

err_pinctrl:
	if (!IS_ERR_OR_NULL(imx900->pins_sleep))
		pinctrl_select_state(imx900->pinctrl, imx900->pins_sleep);

	return ret;
}

static void __imx900_power_off(struct imx900 *imx900)
{
	int ret;
	struct device *dev = &imx900->client->dev;

	if (imx900->is_thunderboot) {
		if (imx900->is_first_streamoff) {
			imx900->is_thunderboot = false;
			imx900->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(imx900->reset_gpio))
		gpiod_direction_output(imx900->reset_gpio, 1);
	clk_disable_unprepare(imx900->xvclk);
	if (!IS_ERR_OR_NULL(imx900->pins_sleep)) {
		ret = pinctrl_select_state(imx900->pinctrl, imx900->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	if (!IS_ERR(imx900->power_gpio))
		gpiod_direction_output(imx900->power_gpio, 0);
	regulator_bulk_disable(IMX900_NUM_SUPPLIES, imx900->supplies);
}

static int __maybe_unused imx900_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);

	return __imx900_power_on(imx900);
}

static int __maybe_unused imx900_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);

	__imx900_power_off(imx900);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx900_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx900 *imx900 = to_imx900(sd);
	struct v4l2_mbus_framefmt *try_fmt =
	    v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct imx900_mode *def_mode = &supported_modes[0];

	mutex_lock(&imx900->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx900->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int imx900_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum
				      *fie)
{
	struct imx900 *imx900 = to_imx900(sd);

	if (fie->index >= imx900->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;

	dev_info(&imx900->client->dev,
		 "%s: width=%d,height=%d,max_fps=%d / %d, hdr_mode=%d\n",
		 __func__, fie->width, fie->height, fie->interval.numerator,
		 fie->interval.denominator, fie->reserved[0]);

	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_2048	2048
#define DST_HEIGHT_1536 1536

/*
 * The resolution of the driver configuration needs to be exactly
 * the same as the current output resolution of the sensor,
 * the input width of the isp needs to be 16 aligned,
 * the input height of the isp needs to be 8 aligned.
 * Can be cropped to standard resolution by this function,
 * otherwise it will crop out strange resolution according
 * to the alignment rules.
 */
static int imx900_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct imx900 *imx900 = to_imx900(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left =
		    CROP_START(imx900->cur_mode->width, DST_WIDTH_2048);
		sel->r.width = DST_WIDTH_2048;
		sel->r.top =
		    CROP_START(imx900->cur_mode->height, DST_HEIGHT_1536);
		sel->r.height = DST_HEIGHT_1536;
		dev_info(&imx900->client->dev,
			 "%s: width=%d,height=%d,left=%d,top=%d", __func__,
			 sel->r.width, sel->r.height, sel->r.left, sel->r.top);
		return 0;
	}
	return -EINVAL;
}

static const struct dev_pm_ops imx900_pm_ops = {
	SET_RUNTIME_PM_OPS(imx900_runtime_suspend,
			   imx900_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx900_internal_ops = {
	.open = imx900_open,
};
#endif

static const struct v4l2_subdev_core_ops imx900_core_ops = {
	.s_power = imx900_s_power,
	.ioctl = imx900_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx900_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx900_video_ops = {
	.s_stream = imx900_s_stream,
	.g_frame_interval = imx900_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx900_pad_ops = {
	.enum_mbus_code = imx900_enum_mbus_code,
	.enum_frame_size = imx900_enum_frame_sizes,
	.enum_frame_interval = imx900_enum_frame_interval,
	.get_fmt = imx900_get_fmt,
	.set_fmt = imx900_set_fmt,
	.get_selection = imx900_get_selection,
	.get_mbus_config = imx900_g_mbus_config,
};

static const struct v4l2_subdev_ops imx900_subdev_ops = {
	.core = &imx900_core_ops,
	.video = &imx900_video_ops,
	.pad = &imx900_pad_ops,
};

static int imx900_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx900 *imx900 = container_of(ctrl->handler,
					     struct imx900, ctrl_handler);
	struct i2c_client *client = imx900->client;
	s64 exposure_max = 0;
	u32 vts = 0, val;
	int ret = 0;
	u32 shs = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		if (imx900->cur_mode->hdr_mode == NO_HDR) {
			/* Update max exposure while meeting expected vblanking */
			exposure_max = IMX900_EXPOSURE_MAX;
			__v4l2_ctrl_modify_range(imx900->exposure,
						 imx900->exposure->minimum,
						 exposure_max,
						 imx900->exposure->step,
						 imx900->exposure->default_value);
		}
		dev_info(&client->dev,
			 "update exposure setting min=%lld,max=%lld,step=%llu,default=%lld\n",
			 imx900->exposure->minimum, exposure_max,
			 imx900->exposure->step,
			 imx900->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (imx900->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		shs = imx900->cur_vts - ctrl->val;

		ret = imx900_write_reg(imx900->client, IMX900_GROUP_HOLD_REG,
				       IMX900_REG_VALUE_08BIT,
				       IMX900_GROUP_HOLD_START);

		ret |= imx900_write_reg(imx900->client, IMX900_EXPO_REG_L,
					IMX900_REG_VALUE_08BIT,
					IMX900_FETCH_EXP_L(shs));
		ret |= imx900_write_reg(imx900->client, IMX900_EXPO_REG_M,
					IMX900_REG_VALUE_08BIT,
					IMX900_FETCH_EXP_M(shs));
		ret |= imx900_write_reg(imx900->client, IMX900_EXPO_REG_H,
					IMX900_REG_VALUE_08BIT,
					IMX900_FETCH_EXP_H(shs));

		ret |= imx900_write_reg(imx900->client, IMX900_GROUP_HOLD_REG,
					IMX900_REG_VALUE_08BIT,
					IMX900_GROUP_HOLD_END);

		dev_info(&client->dev,
			 "set exposure(0x%x) %d = cur_vts(%d) - val(%d)\n",
			 IMX900_EXPO_REG_L, shs, imx900->cur_vts, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		if (imx900->cur_mode->hdr_mode != NO_HDR)
			goto ctrl_end;
		ret = imx900_write_reg(imx900->client, IMX900_GROUP_HOLD_REG,
				       IMX900_REG_VALUE_08BIT,
				       IMX900_GROUP_HOLD_START);

		ret |= imx900_write_reg(imx900->client, IMX900_GAIN_REG_H,
					IMX900_REG_VALUE_08BIT,
					IMX900_FETCH_GAIN_H(ctrl->val));
		ret |= imx900_write_reg(imx900->client, IMX900_GAIN_REG_L,
					IMX900_REG_VALUE_08BIT,
					IMX900_FETCH_GAIN_L(ctrl->val));

		ret |= imx900_write_reg(imx900->client, IMX900_GROUP_HOLD_REG,
					IMX900_REG_VALUE_08BIT,
					IMX900_GROUP_HOLD_END);

		dev_info(&client->dev, "set analog gain(0x%x) 0x%x\n",
			 IMX900_GAIN_REG_L, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		vts = ctrl->val + imx900->cur_mode->height;
		imx900->cur_vts = vts;

		ret = imx900_write_reg(imx900->client, IMX900_GROUP_HOLD_REG,
				       IMX900_REG_VALUE_08BIT,
				       IMX900_GROUP_HOLD_START);

		ret |= imx900_write_reg(imx900->client, IMX900_VTS_REG_L,
					IMX900_REG_VALUE_08BIT,
					IMX900_FETCH_VTS_L(vts));
		ret |= imx900_write_reg(imx900->client, IMX900_VTS_REG_M,
					IMX900_REG_VALUE_08BIT,
					IMX900_FETCH_VTS_M(vts));
		ret |= imx900_write_reg(imx900->client, IMX900_VTS_REG_H,
					IMX900_REG_VALUE_08BIT,
					IMX900_FETCH_VTS_H(vts));

		ret |= imx900_write_reg(imx900->client, IMX900_GROUP_HOLD_REG,
					IMX900_REG_VALUE_08BIT,
					IMX900_GROUP_HOLD_END);

		dev_info(&client->dev, "set vblank(0x%x) 0x%x vts %d\n",
			 IMX900_VTS_REG_L, ctrl->val, vts);
		break;
	case V4L2_CID_HFLIP:
		ret = imx900_read_reg(imx900->client, IMX900_FLIP_REG,
				      IMX900_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX900_MIRROR_BIT_MASK;
		else
			val &= ~IMX900_MIRROR_BIT_MASK;
		ret = imx900_write_reg(imx900->client, IMX900_FLIP_REG,
				       IMX900_REG_VALUE_08BIT, val);
		dev_info(&client->dev, "set HFLIP(0x%x)=0x%x\n",
			 IMX900_FLIP_REG, val);
		break;
	case V4L2_CID_VFLIP:
		ret = imx900_read_reg(imx900->client, IMX900_FLIP_REG,
				      IMX900_REG_VALUE_08BIT, &val);
		if (ret)
			break;
		if (ctrl->val)
			val |= IMX900_FLIP_BIT_MASK;
		else
			val &= ~IMX900_FLIP_BIT_MASK;
		ret = imx900_write_reg(imx900->client, IMX900_FLIP_REG,
				       IMX900_REG_VALUE_08BIT, val);
		dev_info(&client->dev, "set VFLIP(0x%x)=0x%x\n",
			 IMX900_FLIP_REG, val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

ctrl_end:
	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx900_ctrl_ops = {
	.s_ctrl = imx900_set_ctrl,
};

static int imx900_initialize_controls(struct imx900 *imx900)
{
	const struct imx900_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 vblank_def;
	u64 pixel_rate;
	u32 h_blank;
	int ret;

	handler = &imx900->ctrl_handler;
	mode = imx900->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &imx900->mutex;

	imx900->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
						   V4L2_CID_LINK_FREQ,
						   ARRAY_SIZE(link_freq_items) -
						   1, 0, link_freq_items);
	v4l2_ctrl_s_ctrl(imx900->link_freq, mode->mipi_freq_idx);

	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	pixel_rate =
	    (u32) link_freq_items[mode->mipi_freq_idx] / mode->bpp * 2 *
	    IMX900_4LANES;
	imx900->pixel_rate =
	    v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE, 0,
			      IMX900_MAX_PIXEL_RATE, 1, pixel_rate);

	dev_info(&imx900->client->dev,
		 "%s: set pixel rate(%d),link_freq(%lld),pixel_rate(%lld)\n",
		 __func__, mode->mipi_freq_idx,
		 link_freq_items[mode->mipi_freq_idx], pixel_rate);

	h_blank = mode->hts_def - mode->width;
	imx900->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (imx900->hblank)
		imx900->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	dev_info(&imx900->client->dev,
		 "%s: set h_blank range min=%d,max=%d,default=%d\n", __func__,
		 h_blank, h_blank, h_blank);

	vblank_def = mode->vts_def - mode->height;
	imx900->vblank = v4l2_ctrl_new_std(handler, &imx900_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   IMX900_VTS_MAX - mode->height,
					   1, vblank_def);
	imx900->cur_vts = mode->vts_def;

	dev_info(&imx900->client->dev,
		 "%s: set v_blank range min=%lld,max=%d,default=%lld\n",
		 __func__, vblank_def, (IMX900_VTS_MAX - mode->height),
		 vblank_def);

	imx900->exposure = v4l2_ctrl_new_std(handler, &imx900_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX900_EXPOSURE_MIN,
					     IMX900_EXPOSURE_MAX,
					     IMX900_EXPOSURE_STEP,
					     mode->exp_def);

	dev_info(&imx900->client->dev,
		 "%s: set exposure range min=%d,max=%d,default=%d\n", __func__,
		 IMX900_EXPOSURE_MIN, IMX900_EXPOSURE_MAX, mode->exp_def);

	imx900->anal_a_gain = v4l2_ctrl_new_std(handler, &imx900_ctrl_ops,
						V4L2_CID_ANALOGUE_GAIN,
						IMX900_GAIN_MIN,
						IMX900_GAIN_MAX,
						IMX900_GAIN_STEP,
						IMX900_GAIN_DEFAULT);

	dev_info(&imx900->client->dev,
		 "%s: set gain range min=%d,max=%d,default=%d\n", __func__,
		 IMX900_GAIN_MIN, IMX900_GAIN_MAX, IMX900_GAIN_DEFAULT);

	v4l2_ctrl_new_std(handler, &imx900_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1,
			  0);
	v4l2_ctrl_new_std(handler, &imx900_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1,
			  0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx900->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	imx900->subdev.ctrl_handler = handler;
	imx900->has_init_exp = false;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int imx900_check_sensor_id(struct imx900 *imx900,
				  struct i2c_client *client)
{
	struct device *dev = &imx900->client->dev;
	u32 id = 0;
	int ret;

	if (imx900->is_thunderboot) {
		dev_info(dev,
			 "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = imx900_read_reg(client, IMX900_REG_CHIP_ID,
			      IMX900_REG_VALUE_08BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected imx900 id %06x\n", CHIP_ID);

	return 0;
}

static int imx900_configure_regulators(struct imx900 *imx900)
{
	unsigned int i;

	for (i = 0; i < IMX900_NUM_SUPPLIES; i++)
		imx900->supplies[i].supply = imx900_supply_names[i];

	return devm_regulator_bulk_get(&imx900->client->dev,
				       IMX900_NUM_SUPPLIES, imx900->supplies);
}

static int imx900_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx900 *imx900;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	u32 i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x for color",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	imx900 = devm_kzalloc(dev, sizeof(*imx900), GFP_KERNEL);
	if (!imx900)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx900->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx900->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx900->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx900->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}
	imx900->client = client;
	imx900->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < imx900->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			imx900->cur_mode = &supported_modes[i];
			break;
		}
	}

	imx900->is_thunderboot =
	    IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	imx900->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx900->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx900->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx900->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	imx900->power_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(imx900->power_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	imx900->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(imx900->pinctrl)) {
		imx900->pins_default =
		    pinctrl_lookup_state(imx900->pinctrl,
					 OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(imx900->pins_default))
			dev_info(dev, "could not get default pinstate\n");

		imx900->pins_sleep =
		    pinctrl_lookup_state(imx900->pinctrl,
					 OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(imx900->pins_sleep))
			dev_info(dev, "could not get sleep pinstate\n");
	} else {
		dev_info(dev, "no pinctrl\n");
	}

	ret = imx900_configure_regulators(imx900);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&imx900->mutex);

	sd = &imx900->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx900_subdev_ops);
	ret = imx900_initialize_controls(imx900);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx900_power_on(imx900);
	if (ret)
		goto err_free_handler;

	ret = imx900_check_sensor_id(imx900, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx900_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx900->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx900->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(imx900->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx900->module_index, facing, IMX900_NAME, dev_name(sd->dev));
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
	__imx900_power_off(imx900);
err_free_handler:
	v4l2_ctrl_handler_free(&imx900->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx900->mutex);

	return ret;
}

static void imx900_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx900 *imx900 = to_imx900(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx900->ctrl_handler);
	mutex_destroy(&imx900->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx900_power_off(imx900);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx900_of_match[] = {
	{.compatible = "sony,imx900" },
	{ },
};

MODULE_DEVICE_TABLE(of, imx900_of_match);
#endif

static const struct i2c_device_id imx900_match_id[] = {
	{ "sony,imx900", 0 },
	{ },
};

static struct i2c_driver imx900_i2c_driver = {
	.driver = {
		   .name = IMX900_NAME,
		   .pm = &imx900_pm_ops,
		   .of_match_table = of_match_ptr(imx900_of_match),
		    },
	.probe = imx900_probe,
	.remove = imx900_remove,
	.id_table = imx900_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx900_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx900_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx900 sensor driver");
MODULE_LICENSE("GPL");
