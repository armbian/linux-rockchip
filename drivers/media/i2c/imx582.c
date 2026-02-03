// SPDX-License-Identifier: GPL-2.0
/*
 * imx582 driver
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 * V0.0X01.0X00 init version.
 * V0.0X02.0X00 Add PDAF Type1/Type2 Support
 */

// #define DEBUG
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x02, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define IMX582_LINK_FREQ_1098		1098000000	// 2196Mbps per lane
#define IMX582_LINK_FREQ_432		432000000	// 864Mbps per lane
#define IMX582_LINK_FREQ_1000		1000000000	// 2000Mbps per lane

#define IMX582_LANES			4

#define PIXEL_RATE_WITH_2196M_10BIT	(IMX582_LINK_FREQ_1098 / 10 * 2 * 4)
#define PIXEL_RATE_WITH_864M_10BIT	(IMX582_LINK_FREQ_432 / 10 * 2 * 4)
#define PIXEL_RATE_WITH_2000M_10BIT	(IMX582_LINK_FREQ_1000 / 10 * 2 * 4)

#define IMX582_XVCLK_FREQ		24000000

#define CHIP_ID				0x0582
#define IMX582_REG_CHIP_ID_H		0x0016
#define IMX582_REG_CHIP_ID_L		0x0017

#define IMX582_REG_CTRL_MODE		0x0100
#define IMX582_MODE_SW_STANDBY		0x0
#define IMX582_MODE_STREAMING		0x1

#define IMX582_REG_EXPOSURE_H		0x0202
#define IMX582_REG_EXPOSURE_L		0x0203
#define IMX582_EXPOSURE_MIN		2
#define IMX582_EXPOSURE_STEP		1
#define IMX582_VTS_MAX			0x7fff

#define IMX582_REG_GAIN_H		0x0204
#define IMX582_REG_GAIN_L		0x0205
#define IMX582_GAIN_MIN			0x10
#define IMX582_GAIN_MAX			0x400
#define IMX582_GAIN_STEP		1
#define IMX582_GAIN_DEFAULT		0x80

#define IMX582_REG_DGAIN		0x3130
#define IMX582_DGAIN_MODE		BIT(0)
#define IMX582_REG_DGAINGR_H		0x020e
#define IMX582_REG_DGAINGR_L		0x020f
#define IMX582_REG_DGAINR_H		0x0210
#define IMX582_REG_DGAINR_L		0x0211
#define IMX582_REG_DGAINB_H		0x0212
#define IMX582_REG_DGAINB_L		0x0213
#define IMX582_REG_DGAINGB_H		0x0214
#define IMX582_REG_DGAINGB_L		0x0215
#define IMX582_REG_GAIN_GLOBAL_H	0x3ffc
#define IMX582_REG_GAIN_GLOBAL_L	0x3ffd

#define IMX582_REG_TEST_PATTERN		0x0601
#define IMX582_TEST_PATTERN_ENABLE	0x1
#define IMX582_TEST_PATTERN_DISABLE	0x0

#define IMX582_REG_VTS_H		0x0340
#define IMX582_REG_VTS_L		0x0341

/* PDAF Window ROI Control Registers */
/* Based on IMX582 Software Reference Manual for PDAF v1.0.0 */

/* AREA_MODE: Type of AF detection area */
/* 0: Fixed area (16x12) */
/* 1: Fixed area (8x6)  */
/* 2: Flexible/Free area */
/* 3: Reserved */
#define IMX582_REG_AREA_MODE            0x38A3
#define IMX582_AREA_MODE_MASK           0x03
#define IMX582_AREA_MODE_FIXED_16_12    0x00
#define IMX582_AREA_MODE_FIXED_8_6      0x01
#define IMX582_AREA_MODE_FLEXIBLE       0x02

/* PDAF_CTRL1: Output setting from MIPI */
/* 0: Phase Difference Data is not output */
/* 1: Phase Difference Data is output */
#define IMX582_REG_PDAF_CTRL1           0x3E37
#define IMX582_PDAF_OUTPUT_DISABLE      0x00
#define IMX582_PDAF_OUTPUT_ENABLE       0x01

/* Fixed Window Mode Registers (16x12 or 8x6) */
/* Top left X coordinates of the first division */
#define IMX582_REG_PD_AREA_X_OFFSET_H   0x38A4
#define IMX582_REG_PD_AREA_X_OFFSET_L   0x38A5
#define IMX582_PD_AREA_X_OFFSET_MASK_H  0x1F

/* Top left Y coordinates of the first division */
#define IMX582_REG_PD_AREA_Y_OFFSET_H   0x38A6
#define IMX582_REG_PD_AREA_Y_OFFSET_L   0x38A7
#define IMX582_PD_AREA_Y_OFFSET_MASK_H  0x1F

/* Horizontal direction pixels of one division */
#define IMX582_REG_PD_AREA_WIDTH_H      0x38A8
#define IMX582_REG_PD_AREA_WIDTH_L      0x38A9
#define IMX582_PD_AREA_WIDTH_MASK_H     0x1F

/* Vertical direction pixels of one division */
#define IMX582_REG_PD_AREA_HEIGHT_H     0x38AA
#define IMX582_REG_PD_AREA_HEIGHT_L     0x38AB
#define IMX582_PD_AREA_HEIGHT_MASK_H    0x1F

/* Flexible Window Mode Enable Registers (up to 8 windows) */
#define IMX582_REG_AREA_EN_BASE         0x38AC
#define IMX582_REG_AREA_EN_0            0x38AC
#define IMX582_REG_AREA_EN_1            0x38AD
#define IMX582_REG_AREA_EN_2            0x38AE
#define IMX582_REG_AREA_EN_3            0x38AF
#define IMX582_REG_AREA_EN_4            0x38B0
#define IMX582_REG_AREA_EN_5            0x38B1
#define IMX582_REG_AREA_EN_6            0x38B2
#define IMX582_REG_AREA_EN_7            0x38B3
#define IMX582_AREA_EN_MASK             0x01
#define IMX582_AREA_EN_DISABLE          0x00
#define IMX582_AREA_EN_ENABLE           0x01

/* Flexible Window Registers Base Address */
#define IMX582_REG_PD_AREA_FLEX_BASE    0x38B4
#define IMX582_REG_PD_AREA_FLEX_STRIDE  0x08

/* Flexible Window 0 Registers */
#define IMX582_REG_PD_AREA_XSTA_0_H     0x38B4
#define IMX582_REG_PD_AREA_XSTA_0_L     0x38B5
#define IMX582_REG_PD_AREA_YSTA_0_H     0x38B6
#define IMX582_REG_PD_AREA_YSTA_0_L     0x38B7
#define IMX582_REG_PD_AREA_XEND_0_H     0x38B8
#define IMX582_REG_PD_AREA_XEND_0_L     0x38B9
#define IMX582_REG_PD_AREA_YEND_0_H     0x38BA
#define IMX582_REG_PD_AREA_YEND_0_L     0x38BB

/* Flexible Window 1 Registers */
#define IMX582_REG_PD_AREA_XSTA_1_H     0x38BC
#define IMX582_REG_PD_AREA_XSTA_1_L     0x38BD
#define IMX582_REG_PD_AREA_YSTA_1_H     0x38BE
#define IMX582_REG_PD_AREA_YSTA_1_L     0x38BF
#define IMX582_REG_PD_AREA_XEND_1_H     0x38C0
#define IMX582_REG_PD_AREA_XEND_1_L     0x38C1
#define IMX582_REG_PD_AREA_YEND_1_H     0x38C2
#define IMX582_REG_PD_AREA_YEND_1_L     0x38C3

/* Flexible Window 2 Registers */
#define IMX582_REG_PD_AREA_XSTA_2_H     0x38C4
#define IMX582_REG_PD_AREA_XSTA_2_L     0x38C5
#define IMX582_REG_PD_AREA_YSTA_2_H     0x38C6
#define IMX582_REG_PD_AREA_YSTA_2_L     0x38C7
#define IMX582_REG_PD_AREA_XEND_2_H     0x38C8
#define IMX582_REG_PD_AREA_XEND_2_L     0x38C9
#define IMX582_REG_PD_AREA_YEND_2_H     0x38CA
#define IMX582_REG_PD_AREA_YEND_2_L     0x38CB

/* Flexible Window 3 Registers */
#define IMX582_REG_PD_AREA_XSTA_3_H     0x38CC
#define IMX582_REG_PD_AREA_XSTA_3_L     0x38CD
#define IMX582_REG_PD_AREA_YSTA_3_H     0x38CE
#define IMX582_REG_PD_AREA_YSTA_3_L     0x38CF
#define IMX582_REG_PD_AREA_XEND_3_H     0x38D0
#define IMX582_REG_PD_AREA_XEND_3_L     0x38D1
#define IMX582_REG_PD_AREA_YEND_3_H     0x38D2
#define IMX582_REG_PD_AREA_YEND_3_L     0x38D3

/* Flexible Window 4 Registers */
#define IMX582_REG_PD_AREA_XSTA_4_H     0x38D4
#define IMX582_REG_PD_AREA_XSTA_4_L     0x38D5
#define IMX582_REG_PD_AREA_YSTA_4_H     0x38D6
#define IMX582_REG_PD_AREA_YSTA_4_L     0x38D7
#define IMX582_REG_PD_AREA_XEND_4_H     0x38D8
#define IMX582_REG_PD_AREA_XEND_4_L     0x38D9
#define IMX582_REG_PD_AREA_YEND_4_H     0x38DA
#define IMX582_REG_PD_AREA_YEND_4_L     0x38DB

/* Flexible Window 5 Registers */
#define IMX582_REG_PD_AREA_XSTA_5_H     0x38DC
#define IMX582_REG_PD_AREA_XSTA_5_L     0x38DD
#define IMX582_REG_PD_AREA_YSTA_5_H     0x38DE
#define IMX582_REG_PD_AREA_YSTA_5_L     0x38DF
#define IMX582_REG_PD_AREA_XEND_5_H     0x38E0
#define IMX582_REG_PD_AREA_XEND_5_L     0x38E1
#define IMX582_REG_PD_AREA_YEND_5_H     0x38E2
#define IMX582_REG_PD_AREA_YEND_5_L     0x38E3

/* Flexible Window 6 Registers */
#define IMX582_REG_PD_AREA_XSTA_6_H     0x38E4
#define IMX582_REG_PD_AREA_XSTA_6_L     0x38E5
#define IMX582_REG_PD_AREA_YSTA_6_H     0x38E6
#define IMX582_REG_PD_AREA_YSTA_6_L     0x38E7
#define IMX582_REG_PD_AREA_XEND_6_H     0x38E8
#define IMX582_REG_PD_AREA_XEND_6_L     0x38E9
#define IMX582_REG_PD_AREA_YEND_6_H     0x38EA
#define IMX582_REG_PD_AREA_YEND_6_L     0x38EB

/* Flexible Window 7 Registers */
#define IMX582_REG_PD_AREA_XSTA_7_H     0x38EC
#define IMX582_REG_PD_AREA_XSTA_7_L     0x38ED
#define IMX582_REG_PD_AREA_YSTA_7_H     0x38EE
#define IMX582_REG_PD_AREA_YSTA_7_L     0x38EF
#define IMX582_REG_PD_AREA_XEND_7_H     0x38F0
#define IMX582_REG_PD_AREA_XEND_7_L     0x38F1
#define IMX582_REG_PD_AREA_YEND_7_H     0x38F2
#define IMX582_REG_PD_AREA_YEND_7_L     0x38F3

/* Coordinate mask for high byte (12-bit coordinate: 5-bit high + 8-bit low) */
#define IMX582_PD_COORD_MASK_H          0x1F

/* Maximum number of flexible windows */
#define IMX582_MAX_FLEXIBLE_WINDOWS     8

/* Phase Difference Data Hold Control for I2C read */
#define IMX582_REG_PD_TABLE_HOLD        0xE24F
#define IMX582_PD_TABLE_HOLD_MASK       0x01
#define IMX582_PD_TABLE_HOLD_OFF        0x00
#define IMX582_PD_TABLE_HOLD_ON         0x01

/* Phase Difference Data Readout Base Address via I2C */
#define IMX582_REG_PD_DATA_BASE         0x4408

#define IMX582_FLIP_MIRROR_REG		0x0101
#define IMX582_MIRROR_BIT_MASK		BIT(0)
#define IMX582_FLIP_BIT_MASK		BIT(1)

#define IMX582_FETCH_EXP_H(VAL)		(((VAL) >> 8) & 0xFF)
#define IMX582_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define IMX582_FETCH_AGAIN_H(VAL)	(((VAL) >> 8) & 0x03)
#define IMX582_FETCH_AGAIN_L(VAL)	((VAL) & 0xFF)

#define IMX582_FETCH_DGAIN_H(VAL)	(((VAL) >> 8) & 0x0F)
#define IMX582_FETCH_DGAIN_L(VAL)	((VAL) & 0xFF)

#define IMX582_FETCH_RHS1_H(VAL)	(((VAL) >> 16) & 0x0F)
#define IMX582_FETCH_RHS1_M(VAL)	(((VAL) >> 8) & 0xFF)
#define IMX582_FETCH_RHS1_L(VAL)	((VAL) & 0xFF)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define IMX582_REG_VALUE_08BIT		1
#define IMX582_REG_VALUE_16BIT		2
#define IMX582_REG_VALUE_24BIT		3

#define OF_CAMERA_HDR_MODE		"rockchip,camera-hdr-mode"

#define IMX582_NAME			"imx582"

static const char * const imx582_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define IMX582_NUM_SUPPLIES ARRAY_SIZE(imx582_supply_names)

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

struct imx582_mode {
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
	/* Shield Pix Data */
	const struct other_data *spd;
	/* embedded Data */
	const struct other_data *ebd;
	u32 vc[PAD_MAX];
};

struct imx582 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[IMX582_NUM_SUPPLIES];

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
	const struct imx582_mode *cur_mode;
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
	u32			ebd_id;
};

#define to_imx582(sd) container_of(sd, struct imx582, subdev)

/*
 *IMX582LQR All-pixel scan CSI-2_4lane 24Mhz
 *AD:10bit Output:10bit 2196Mbps Master Mode 30fps
 *
 */
static const struct regval imx582_linear_10bit_global_regs[] = {
	/*External clock setting*/
	{0x0136, 0x18},
	{0x0137, 0x00},
	/*Register version*/
	{0x3C7E, 0x04},
	{0x3C7F, 0x08},
	/*Global Setting*/
	{0x3C00, 0x10},
	{0x3C01, 0x10},
	{0x3C02, 0x10},
	{0x3C03, 0x10},
	{0x3C04, 0x10},
	{0x3C05, 0x01},
	{0x3C06, 0x00},
	{0x3C07, 0x00},
	{0x3C08, 0x03},
	{0x3C09, 0xFF},
	{0x3C0A, 0x01},
	{0x3C0B, 0x00},
	{0x3C0C, 0x00},
	{0x3C0D, 0x03},
	{0x3C0E, 0xFF},
	{0x3C0F, 0x20},
	{0x6E1D, 0x00},
	{0x6E25, 0x00},
	{0x6E38, 0x03},
	{0x6E3B, 0x01},
	{0x9004, 0x2C},
	{0x9200, 0xF4},
	{0x9201, 0xA7},
	{0x9202, 0xF4},
	{0x9203, 0xAA},
	{0x9204, 0xF4},
	{0x9205, 0xAD},
	{0x9206, 0xF4},
	{0x9207, 0xB0},
	{0x9208, 0xF4},
	{0x9209, 0xB3},
	{0x920A, 0xB7},
	{0x920B, 0x34},
	{0x920C, 0xB7},
	{0x920D, 0x36},
	{0x920E, 0xB7},
	{0x920F, 0x37},
	{0x9210, 0xB7},
	{0x9211, 0x38},
	{0x9212, 0xB7},
	{0x9213, 0x39},
	{0x9214, 0xB7},
	{0x9215, 0x3A},
	{0x9216, 0xB7},
	{0x9217, 0x3C},
	{0x9218, 0xB7},
	{0x9219, 0x3D},
	{0x921A, 0xB7},
	{0x921B, 0x3E},
	{0x921C, 0xB7},
	{0x921D, 0x3F},
	{0x921E, 0x85},
	{0x921F, 0x77},
	{0x9226, 0x42},
	{0x9227, 0x52},
	{0x9228, 0x60},
	{0x9229, 0xB9},
	{0x922A, 0x60},
	{0x922B, 0xBF},
	{0x922C, 0x60},
	{0x922D, 0xC5},
	{0x922E, 0x60},
	{0x922F, 0xCB},
	{0x9230, 0x60},
	{0x9231, 0xD1},
	{0x9232, 0x60},
	{0x9233, 0xD7},
	{0x9234, 0x60},
	{0x9235, 0xDD},
	{0x9236, 0x60},
	{0x9237, 0xE3},
	{0x9238, 0x60},
	{0x9239, 0xE9},
	{0x923A, 0x60},
	{0x923B, 0xEF},
	{0x923C, 0x60},
	{0x923D, 0xF5},
	{0x923E, 0x60},
	{0x923F, 0xF9},
	{0x9240, 0x60},
	{0x9241, 0xFD},
	{0x9242, 0x61},
	{0x9243, 0x01},
	{0x9244, 0x61},
	{0x9245, 0x05},
	{0x924A, 0x61},
	{0x924B, 0x6B},
	{0x924C, 0x61},
	{0x924D, 0x7F},
	{0x924E, 0x61},
	{0x924F, 0x92},
	{0x9250, 0x61},
	{0x9251, 0x9C},
	{0x9252, 0x61},
	{0x9253, 0xAB},
	{0x9254, 0x61},
	{0x9255, 0xC4},
	{0x9256, 0x61},
	{0x9257, 0xCE},
	{0x9810, 0x14},
	{0x9814, 0x14},
	{0xC449, 0x04},
	{0xC44A, 0x01},
	{0xE286, 0x31},
	{0xE2A6, 0x32},
	{0xE2C6, 0x33},
	/* Image Quality adjustment setting */
	{0x88D6, 0x60},
	{0x9852, 0x00},
	{0xAE09, 0xFF},
	{0xAE0A, 0xFF},
	{0xAE12, 0x58},
	{0xAE13, 0x58},
	{0xAE15, 0x10},
	{0xAE16, 0x10},
	{0xB071, 0x00},
	{REG_NULL, 0x00},
};

static const struct regval imx582_linear_10bit_4000x3000_30fps_nopd_regs[] = {
	/* MIPI output setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0114, 0x03},

	/* Line Length PCK Setting */
	{0x0342, 0x1E},  // 7872
	{0x0343, 0xC0},

	/* Frame Length Lines Setting */
	{0x0340, 0x0B},  // 3062
	{0x0341, 0xF6},

	/* ROI Setting */
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x1F},
	{0x0349, 0x3F},
	{0x034A, 0x17},
	{0x034B, 0x6F},

	/* Mode Setting */
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x0902, 0x08},
	{0x3246, 0x81},
	{0x3247, 0x81},

	/* Digital Crop & Scaling */
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x0F},
	{0x040D, 0xA0},
	{0x040E, 0x0B},
	{0x040F, 0xB8},

	/* Output Size Setting */
	{0x034C, 0x0F},
	{0x034D, 0xA0},
	{0x034E, 0x0B},
	{0x034F, 0xB8},

	/* Clock Setting */
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x01},
	{0x0307, 0x2D},
	{0x030B, 0x01},
	{0x030D, 0x04},
	{0x030E, 0x01},
	{0x030F, 0x6E},
	{0x0310, 0x01},

	/* Other Setting */
	{0x3620, 0x00},
	{0x3621, 0x00},
	{0x380C, 0x80},
	{0x3C13, 0x00},
	{0x3C14, 0x28},
	{0x3C15, 0x28},
	{0x3C16, 0x32},
	{0x3C17, 0x46},
	{0x3C18, 0x67},
	{0x3C19, 0x8F},
	{0x3C1A, 0x8F},
	{0x3C1B, 0x99},
	{0x3C1C, 0xAD},
	{0x3C1D, 0xCE},
	{0x3C1E, 0x8F},
	{0x3C1F, 0x8F},
	{0x3C20, 0x99},
	{0x3C21, 0xAD},
	{0x3C22, 0xCE},
	{0x3C25, 0x22},
	{0x3C26, 0x23},
	{0x3C27, 0xE6},
	{0x3C28, 0xE6},
	{0x3C29, 0x08},
	{0x3C2A, 0x0F},
	{0x3C2B, 0x14},
	{0x3F0C, 0x01},
	{0x3F14, 0x00},
	{0x3F80, 0x06},
	{0x3F81, 0xB7},
	{0x3F82, 0x00},
	{0x3F83, 0x00},
	{0x3F8C, 0x00},
	{0x3F8D, 0xD0},
	{0x3FF4, 0x01},
	{0x3FF5, 0x40},
	{0x3FFC, 0x02},
	{0x3FFD, 0x15},

	/* Integration Setting */
	{0x0202, 0x0B},
	{0x0203, 0xC6},
	{0x0224, 0x01},
	{0x0225, 0xF4},
	{0x3FE0, 0x01},
	{0x3FE1, 0xF4},

	/* Gain Setting */
	{0x0204, 0x00},
	{0x0205, 0x70},
	{0x0216, 0x00},
	{0x0217, 0x70},
	{0x0218, 0x01},
	{0x0219, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x3FE2, 0x00},
	{0x3FE3, 0x70},
	{0x3FE4, 0x01},
	{0x3FE5, 0x00},

	/* PDAF TYPE Setting */
	{0x3E20, 0x01},
	/* PDAF TYPE1 Setting */
	{0x3E37, 0x00},

	{REG_NULL, 0x00},
};

static const struct regval imx582_linear_10bit_3840x2160_30fps_pd_regs[] = {
	/* MIPI output setting */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0114, 0x03},

	/* Line Length PCK Setting */
	{0x0342, 0x1E},  // 7872
	{0x0343, 0xC0},

	/* Frame Length Lines Setting */
	{0x0340, 0x0E},  // 3658
	{0x0341, 0x4A},

	/* ROI Setting */
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x03},
	{0x0347, 0x48},
	{0x0348, 0x1F},
	{0x0349, 0x3F},
	{0x034A, 0x14},
	{0x034B, 0x27},

	/* Mode Setting */
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x0902, 0x08},
	{0x3246, 0x81},
	{0x3247, 0x81},

	/* Digital Crop & Scaling */
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x50},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x0F},
	{0x040D, 0x00},
	{0x040E, 0x08},
	{0x040F, 0x70},

	/* Output Size Setting */
	{0x034C, 0x0F},
	{0x034D, 0x00},
	{0x034E, 0x08},
	{0x034F, 0x70},

	/* Clock Setting */
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x01},
	{0x0307, 0x68},
	{0x030B, 0x01},
	{0x030D, 0x02},
	{0x030E, 0x01},
	{0x030F, 0x68},
	{0x0310, 0x00},

	/* Other Setting */
	{0x3620, 0x00},
	{0x3621, 0x00},
	{0x380C, 0x80},
	{0x3C13, 0x00},
	{0x3C14, 0x28},
	{0x3C15, 0x28},
	{0x3C16, 0x32},
	{0x3C17, 0x46},
	{0x3C18, 0x67},
	{0x3C19, 0x8F},
	{0x3C1A, 0x8F},
	{0x3C1B, 0x99},
	{0x3C1C, 0xAD},
	{0x3C1D, 0xCE},
	{0x3C1E, 0x8F},
	{0x3C1F, 0x8F},
	{0x3C20, 0x99},
	{0x3C21, 0xAD},
	{0x3C22, 0xCE},
	{0x3C25, 0x22},
	{0x3C26, 0x23},
	{0x3C27, 0xE6},
	{0x3C28, 0xE6},
	{0x3C29, 0x08},
	{0x3C2A, 0x0F},
	{0x3C2B, 0x14},
	{0x3F0C, 0x01},
	{0x3F14, 0x00},
	{0x3F80, 0x06},
	{0x3F81, 0xB7},
	{0x3F82, 0x00},
	{0x3F83, 0x00},
	{0x3F8C, 0x07},
	{0x3F8D, 0xD0},
	{0x3FF4, 0x01},
	{0x3FF5, 0x40},
	{0x3FFC, 0x02},
	{0x3FFD, 0x15},

	/* Integration Setting */
	{0x0202, 0x0E},
	{0x0203, 0x1A},
	{0x0224, 0x01},
	{0x0225, 0xF4},
	{0x3FE0, 0x01},
	{0x3FE1, 0xF4},

	/* Gain Setting */
	{0x0204, 0x00},
	{0x0205, 0x70},
	{0x0216, 0x00},
	{0x0217, 0x70},
	{0x0218, 0x01},
	{0x0219, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x3FE2, 0x00},
	{0x3FE3, 0x70},
	{0x3FE4, 0x01},
	{0x3FE5, 0x00},

	/* MIPI Setting */
	{0xE000, 0X00},

	/* PDAF TYPE Setting */
	{0x3E20, 0x02},
	/* PDAF TYPE2 Setting */
	{0x3E3B, 0x01},
	{0x4034, 0x01},
	{0x4035, 0xE0},
	{REG_NULL, 0x00},
};


/* Add 3840x2160 PDAF 30fps register configuration */
static const struct regval imx582_linear_10bit_3840x2160_30fps_pd_type1_regs[] = {
	/* MIPI output setting */
	{0x0112, 0x0A},  /* CSI_DT_FMT_H */
	{0x0113, 0x0A},  /* CSI_DT_FMT_L */
	{0x0114, 0x03},  /* CSI_LANE_MODE */

	/* Line Length PCK Setting */
	{0x0342, 0x1E},
	{0x0343, 0xC0},

	/* Frame Length Lines Setting */
	{0x0340, 0x0E},
	{0x0341, 0x4A},

	/* ROI setting */
	{0x0344, 0x00},  /* X_ADD_STA[12:8] */
	{0x0345, 0x00},  /* X_ADD_STA[7:0] */
	{0x0346, 0x00},  /* Y_ADD_STA[12:8] */
	{0x0347, 0x00},  /* Y_ADD_STA[7:0] */
	{0x0348, 0x1F},  /* X_ADD_END[12:8] */
	{0x0349, 0x3F},  /* X_ADD_END[7:0] */
	{0x034A, 0x17},  /* Y_ADD_END[12:8] */
	{0x034B, 0x6F},  /* Y_ADD_END[7:0] */

	/* Mode setting */
	{0x0900, 0x01},  /* BINNING_MODE */
	{0x0901, 0x22},  /* [7:4] BINNING_TYPE_H, [3:0] BINNING_TYPE_V */
	{0x0902, 0x08},  /* BINNING_WEIGHTING */
	{0x3246, 0x81},  /* BINNING_PRIORITY_H */
	{0x3247, 0x81},  /* BINNING_PRIORITY_V */

	/* Digital Crop & Scaling */
	{0x0401, 0x00},  /* SCALE_MODE */
	{0x0404, 0x00},  /* SCALE_M[8] */
	{0x0405, 0x10},  /* SCALE_M[7:0] */
	{0x0408, 0x00},  /* DIG_CROP_X_OFFSET[12:8] */
	{0x0409, 0x50},  /* DIG_CROP_X_OFFSET[7:0] */
	{0x040A, 0x01},  /* DIG_CROP_Y_OFFSET[12:8] */
	{0x040B, 0xA4},  /* DIG_CROP_Y_OFFSET[7:0] */
	{0x040C, 0x0F},  /* DIG_CROP_IMAGE_WIDTH[12:8] */
	{0x040D, 0x00},  /* DIG_CROP_IMAGE_WIDTH[7:0] */
	{0x040E, 0x08},  /* DIG_CROP_IMAGE_HEIGHT[12:8] */
	{0x040F, 0x70},  /* DIG_CROP_IMAGE_HEIGHT[7:0] */

	/* Output size setting */
	{0x034C, 0x0F},  /* X_OUT_SIZE[12:8] */
	{0x034D, 0x00},  /* X_OUT_SIZE[7:0] */
	{0x034E, 0x08},  /* Y_OUT_SIZE[12:8] */
	{0x034F, 0x70},  /* Y_OUT_SIZE[7:0] */

	/* Clock setting */
	{0x0301, 0x05},  /* IVT_PXCK_DIV */
	{0x0303, 0x02},  /* IVT_SYCK_DIV */
	{0x0305, 0x04},  /* IVT_PREPLLCK_DIV */
	{0x0306, 0x01},  /* IVT_PLL_MPY[10:8] */
	{0x0307, 0x68},  /* IVT_PLL_MPY[7:0] */
	{0x030B, 0x01},  /* IOP_SYCK_DIV */
	{0x030D, 0x04},  /* IOP_PREPLLCK_DIV */
	{0x030E, 0x01},  /* IOP_PLL_MPY[10:8] */
	{0x030F, 0x68},  /* IOP_PLL_MPY[7:0] */
	{0x0310, 0x01},  /* PLL_MULT_DRIV */

	/* Other setting */
	{0x3620, 0x00},
	{0x3621, 0x00},
	{0x380C, 0x80},
	{0x3C13, 0x00},
	{0x3C14, 0x28},
	{0x3C15, 0x28},
	{0x3C16, 0x32},
	{0x3C17, 0x46},
	{0x3C18, 0x67},
	{0x3C19, 0x8F},
	{0x3C1A, 0x8F},
	{0x3C1B, 0x99},
	{0x3C1C, 0xAD},
	{0x3C1D, 0xCE},
	{0x3C1E, 0x8F},
	{0x3C1F, 0x8F},
	{0x3C20, 0x99},
	{0x3C21, 0xAD},
	{0x3C22, 0xCE},
	{0x3C25, 0x22},
	{0x3C26, 0x23},
	{0x3C27, 0xE6},
	{0x3C28, 0xE6},
	{0x3C29, 0x08},
	{0x3C2A, 0x0F},
	{0x3C2B, 0x14},
	{0x3F0C, 0x01},
	{0x3F14, 0x00},
	{0x3F80, 0x06},
	{0x3F81, 0xB7},
	{0x3F82, 0x00},
	{0x3F83, 0x00},
	{0x3F8C, 0x07},
	{0x3F8D, 0xD0},
	{0x3FF4, 0x01},
	{0x3FF5, 0x40},
	{0x3FFC, 0x02},
	{0x3FFD, 0x15},

	/* Integration setting */
	{0x0202, 0x0E},  /* COARSE_INTEG_TIME[15:8] */
	{0x0203, 0x1A},  /* COARSE_INTEG_TIME[7:0] */
	{0x0224, 0x01},  /* ST_COARSE_INTEG_TIME[15:8] */
	{0x0225, 0xF4},  /* ST_COARSE_INTEG_TIME[7:0] */
	{0x3FE0, 0x01},
	{0x3FE1, 0xF4},

	/* Gain setting */
	{0x0204, 0x00},  /* ANA_GAIN_GLOBAL[9:8] */
	{0x0205, 0x70},  /* ANA_GAIN_GLOBAL[7:0] */
	{0x0216, 0x00},  /* ST_ANA_GAIN_GLOBAL[9:8] */
	{0x0217, 0x70},  /* ST_ANA_GAIN_GLOBAL[7:0] */
	{0x0218, 0x01},  /* ST_DIG_GAIN_GLOBAL[15:8] */
	{0x0219, 0x00},  /* ST_DIG_GAIN_GLOBAL[7:0] */
	{0x020E, 0x01},  /* DIG_GAIN_GLOBAL[15:8] */
	{0x020F, 0x00},  /* DIG_GAIN_GLOBAL[7:0] */
	{0x0210, 0x01},  /* DIG_GAIN_R[15:8] */
	{0x0211, 0x00},  /* DIG_GAIN_R[7:0] */
	{0x0212, 0x01},  /* DIG_GAIN_B[15:8] */
	{0x0213, 0x00},  /* DIG_GAIN_B[7:0] */
	{0x0214, 0x01},  /* DIG_GAIN_GB[15:8] */
	{0x0215, 0x00},  /* DIG_GAIN_GB[7:0] */
	{0x3FE2, 0x00},
	{0x3FE3, 0x70},
	{0x3FE4, 0x01},
	{0x3FE5, 0x00},

	/* PDAF setting */
	{0x3E20, 0x01},  /* PDAF TYPE setting */
	{0x3E37, 0x01},  /* PDAF TYPE1 setting */
	{0x3E3B, 0x00},  /* PDAF TYPE2 setting */
	{0x4034, 0x01},
	{0x4035, 0xF0},
	{REG_NULL, 0x00},
};

static const struct other_data imx582_binning2x2_spd = {
	.width = 960,
	.height = 1072,
	.bus_fmt = MEDIA_BUS_FMT_SPD_2X8,
	.data_type = 0x34,
	.data_bit = 10,
};

static const struct other_data imx582_binning2x2_spd_type1 = {
	.width = 3840,
	.height = 1,
	.bus_fmt = MEDIA_BUS_FMT_SPD_2X8,
	.data_type = 0x36,
	.data_bit = 10,
};

static const struct other_data imx582_binning2x2_ebd = {
	.width = 368,
	.height = 2,
	.bus_fmt = MEDIA_BUS_FMT_EBD_1X8,
};

static const struct imx582_mode supported_modes[] = {
	{
		.width = 4000,
		.height = 3000,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0080,
		.hts_def = 0x1EC0,
		.vts_def = 0x0BF6,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.global_reg_list = imx582_linear_10bit_global_regs,
		.reg_list = imx582_linear_10bit_4000x3000_30fps_nopd_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 0,
		.vc[PAD0] = 0,
	},
	{
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0100,
		.hts_def = 0x1EC0,
		.vts_def = 0x0E4A,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.global_reg_list = imx582_linear_10bit_global_regs,
		.reg_list = imx582_linear_10bit_3840x2160_30fps_pd_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 2,
		.spd = &imx582_binning2x2_spd,
		.ebd = &imx582_binning2x2_ebd,
		.vc[PAD0] = 0,
	},
	{
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0100,
		.hts_def = 0x1EC0,
		.vts_def = 0x0E4A,
		.bus_fmt = MEDIA_BUS_FMT_SRGGB10_1X10,
		.global_reg_list = imx582_linear_10bit_global_regs,
		.reg_list = imx582_linear_10bit_3840x2160_30fps_pd_type1_regs,
		.hdr_mode = NO_HDR,
		.mipi_freq_idx = 2,
		.spd = &imx582_binning2x2_spd_type1,
		.ebd = &imx582_binning2x2_ebd,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_items[] = {
	IMX582_LINK_FREQ_1098,
	IMX582_LINK_FREQ_432,
	IMX582_LINK_FREQ_1000,
};

static const char * const imx582_test_pattern_menu[] = {
	"Disabled",
	"Solid color",
	"100% color bars",
	"Fade to grey color bars",
	"PN9"
};

/* Write registers up to 4 at a time */
static int imx582_write_reg(struct i2c_client *client, u16 reg,
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

static int imx582_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		if (unlikely(regs[i].addr == REG_DELAY))
			usleep_range(regs[i].val, regs[i].val * 2);
		else
			ret = imx582_write_reg(client, regs[i].addr,
					       IMX582_REG_VALUE_08BIT,
					       regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int imx582_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int imx582_get_reso_dist(const struct imx582_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		   abs(mode->height - framefmt->height);
}

static const struct imx582_mode *
imx582_find_best_fit(struct imx582 *imx582, struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < imx582->cfg_num; i++) {
		dist = imx582_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int imx582_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx582 *imx582 = to_imx582(sd);
	const struct imx582_mode *mode;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;

	mutex_lock(&imx582->mutex);

	mode = imx582_find_best_fit(imx582, fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx582->mutex);
		return -ENOTTY;
#endif
	} else {
		imx582->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx582->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(imx582->vblank, vblank_def,
					 IMX582_VTS_MAX - mode->height,
					 1, vblank_def);

		__v4l2_ctrl_s_ctrl(imx582->vblank, vblank_def);
		__v4l2_ctrl_s_ctrl(imx582->link_freq, mode->mipi_freq_idx);
		pixel_rate = (u32)link_freq_items[mode->mipi_freq_idx] / 10 * 2 * IMX582_LANES;
		__v4l2_ctrl_s_ctrl_int64(imx582->pixel_rate,
					 pixel_rate);
	}

	dev_info(&imx582->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&imx582->mutex);

	return 0;
}

static int imx582_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx582 *imx582 = to_imx582(sd);
	const struct imx582_mode *mode = imx582->cur_mode;

	mutex_lock(&imx582->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&imx582->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		if (imx582->flip & IMX582_MIRROR_BIT_MASK) {
			fmt->format.code = MEDIA_BUS_FMT_SGRBG10_1X10;
			if (imx582->flip & IMX582_FLIP_BIT_MASK)
				fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
		} else if (imx582->flip & IMX582_FLIP_BIT_MASK) {
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

		/* to csi rawwr3, other rawwr also can use */
		if (fmt->pad == imx582->spd_id && mode->spd) {
			fmt->format.width = mode->spd->width;
			fmt->format.height = mode->spd->height;
			fmt->format.code = mode->spd->bus_fmt;
			//Set the vc channel to be consistent with the valid data
			fmt->reserved[0] = 0;
		} else if (fmt->pad == imx582->ebd_id && mode->ebd) {
			fmt->format.width = mode->ebd->width;
			fmt->format.height = mode->ebd->height;
			fmt->format.code = mode->ebd->bus_fmt;
			//Set the vc channel to be consistent with the valid data
			fmt->reserved[0] = 0;
		}
	}
	mutex_unlock(&imx582->mutex);

	return 0;
}

static int imx582_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx582 *imx582 = to_imx582(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = imx582->cur_mode->bus_fmt;

	return 0;
}

static int imx582_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx582 *imx582 = to_imx582(sd);

	if (fse->index >= imx582->cfg_num)
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int imx582_enable_test_pattern(struct imx582 *imx582, u32 pattern)
{
	u32 val;

	if (pattern)
		val = (pattern - 1) | IMX582_TEST_PATTERN_ENABLE;
	else
		val = IMX582_TEST_PATTERN_DISABLE;

	return imx582_write_reg(imx582->client,
				IMX582_REG_TEST_PATTERN,
				IMX582_REG_VALUE_08BIT,
				val);
}

static int imx582_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx582 *imx582 = to_imx582(sd);
	const struct imx582_mode *mode = imx582->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static int imx582_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = IMX582_LANES;

	return 0;
}

static void imx582_get_otp(struct otp_info *otp,
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

static void imx582_get_module_inf(struct imx582 *imx582,
				  struct rkmodule_inf *inf)
{
	struct otp_info *otp = imx582->otp;

	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, IMX582_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, imx582->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, imx582->len_name, sizeof(inf->base.lens));
	if (otp)
		imx582_get_otp(otp, inf);

}

static int imx582_get_channel_info(struct imx582 *imx582, struct rkmodule_channel_info *ch_info)
{
	const struct imx582_mode *mode = imx582->cur_mode;

	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;

	if (ch_info->index == imx582->spd_id && mode->spd) {
		ch_info->vc = 0;
		ch_info->width = mode->spd->width;
		ch_info->height = mode->spd->height;
		ch_info->bus_fmt = mode->spd->bus_fmt;
		ch_info->data_type = mode->spd->data_type;
		ch_info->data_bit = mode->spd->data_bit;
	} else {
		ch_info->vc = imx582->cur_mode->vc[ch_info->index];
		ch_info->width = imx582->cur_mode->width;
		ch_info->height = imx582->cur_mode->height;
		ch_info->bus_fmt = imx582->cur_mode->bus_fmt;
	}
	return 0;
}

static long imx582_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx582 *imx582 = to_imx582(sd);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_channel_info *ch_info;
	long ret = 0;
	u32 i, h, w;
	u32 stream = 0;
	struct rkmodule_pdaf_win_cfg_t *pdaf_cfg;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx582_get_module_inf(imx582, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = imx582->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = imx582->cur_mode->width;
		h = imx582->cur_mode->height;
		for (i = 0; i < imx582->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode) {
				imx582->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == imx582->cfg_num) {
			dev_err(&imx582->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = imx582->cur_mode->hts_def -
			    imx582->cur_mode->width;
			h = imx582->cur_mode->vts_def -
			    imx582->cur_mode->height;
			__v4l2_ctrl_modify_range(imx582->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(imx582->vblank, h,
						 IMX582_VTS_MAX -
						 imx582->cur_mode->height,
						 1, h);

			imx582->cur_link_freq = 0;
			imx582->cur_pixel_rate = PIXEL_RATE_WITH_2196M_10BIT;

			__v4l2_ctrl_s_ctrl_int64(imx582->pixel_rate,
						 imx582->cur_pixel_rate);
			__v4l2_ctrl_s_ctrl(imx582->link_freq,
					   imx582->cur_link_freq);
		}
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx582_write_reg(imx582->client, IMX582_REG_CTRL_MODE,
				IMX582_REG_VALUE_08BIT, IMX582_MODE_STREAMING);
		else
			ret = imx582_write_reg(imx582->client, IMX582_REG_CTRL_MODE,
				IMX582_REG_VALUE_08BIT, IMX582_MODE_SW_STANDBY);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = imx582_get_channel_info(imx582, ch_info);
		break;
	case RKMODULE_SET_PDAF_WIN_CFG:
	{
		pdaf_cfg = (struct rkmodule_pdaf_win_cfg_t *)arg;
		struct i2c_client *client = imx582->client;
		u32 j;

		dev_info(&client->dev, "%s: win_mode = %d\n",
			__func__, pdaf_cfg->win_mode);

		if (!imx582->streaming) {
			dev_err(&client->dev, "sensor not streaming, cannot set pdaf win\n");
			return -EINVAL;
		}

		switch (pdaf_cfg->win_mode) {
		case FIXED_GRID_WIN_16_12:
			/* Set fixed grid 16x12 mode */
			ret = imx582_write_reg(client, IMX582_REG_AREA_MODE,
				IMX582_REG_VALUE_08BIT, IMX582_AREA_MODE_FIXED_16_12);
			if (ret) {
				dev_err(&client->dev, "Failed to set fixed grid 16x12 mode\n");
				break;
			}

			/* Set regional parameters */
			ret = imx582_write_reg(client, IMX582_REG_PD_AREA_X_OFFSET_H,
				IMX582_REG_VALUE_08BIT,
				(pdaf_cfg->fixed_win.area_x_offset >> 8) & IMX582_PD_COORD_MASK_H);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_X_OFFSET_L,
				IMX582_REG_VALUE_08BIT,
				pdaf_cfg->fixed_win.area_x_offset & 0xFF);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_Y_OFFSET_H,
				IMX582_REG_VALUE_08BIT,
				(pdaf_cfg->fixed_win.area_y_offset >> 8) & IMX582_PD_COORD_MASK_H);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_Y_OFFSET_L,
				IMX582_REG_VALUE_08BIT,
				pdaf_cfg->fixed_win.area_y_offset & 0xFF);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_WIDTH_H,
				IMX582_REG_VALUE_08BIT,
				(pdaf_cfg->fixed_win.area_width >> 8) & IMX582_PD_COORD_MASK_H);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_WIDTH_L,
				IMX582_REG_VALUE_08BIT,
				pdaf_cfg->fixed_win.area_width & 0xFF);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_HEIGHT_H,
				IMX582_REG_VALUE_08BIT,
				(pdaf_cfg->fixed_win.area_height >> 8) & IMX582_PD_COORD_MASK_H);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_HEIGHT_L,
				IMX582_REG_VALUE_08BIT,
				pdaf_cfg->fixed_win.area_height & 0xFF);
			if (ret) {
				dev_err(&client->dev, "Failed to set fixed grid params\n");
			} else {
				dev_info(&client->dev, "Set fixed grid 16x12: x_off=%d, y_off=%d, w=%d, h=%d\n",
					pdaf_cfg->fixed_win.area_x_offset,
					pdaf_cfg->fixed_win.area_y_offset,
					pdaf_cfg->fixed_win.area_width,
					pdaf_cfg->fixed_win.area_height);
			}
			break;

		case FIXED_GRID_WIN_8_6:
			/* Set fixed grid 8x6 mode */
			ret = imx582_write_reg(client, IMX582_REG_AREA_MODE,
				IMX582_REG_VALUE_08BIT, IMX582_AREA_MODE_FIXED_8_6);
			if (ret) {
				dev_err(&client->dev, "Failed to set fixed grid 8x6 mode\n");
				break;
			}

			/* Set regional parameters (same registers as 16x12) */
			ret = imx582_write_reg(client, IMX582_REG_PD_AREA_X_OFFSET_H,
				IMX582_REG_VALUE_08BIT,
				(pdaf_cfg->fixed_win.area_x_offset >> 8) & IMX582_PD_COORD_MASK_H);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_X_OFFSET_L,
				IMX582_REG_VALUE_08BIT,
				pdaf_cfg->fixed_win.area_x_offset & 0xFF);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_Y_OFFSET_H,
				IMX582_REG_VALUE_08BIT,
				(pdaf_cfg->fixed_win.area_y_offset >> 8) & IMX582_PD_COORD_MASK_H);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_Y_OFFSET_L,
				IMX582_REG_VALUE_08BIT,
				pdaf_cfg->fixed_win.area_y_offset & 0xFF);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_WIDTH_H,
				IMX582_REG_VALUE_08BIT,
				(pdaf_cfg->fixed_win.area_width >> 8) & IMX582_PD_COORD_MASK_H);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_WIDTH_L,
				IMX582_REG_VALUE_08BIT,
				pdaf_cfg->fixed_win.area_width & 0xFF);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_HEIGHT_H,
				IMX582_REG_VALUE_08BIT,
				(pdaf_cfg->fixed_win.area_height >> 8) & IMX582_PD_COORD_MASK_H);
			ret |= imx582_write_reg(client, IMX582_REG_PD_AREA_HEIGHT_L,
				IMX582_REG_VALUE_08BIT,
				pdaf_cfg->fixed_win.area_height & 0xFF);

			if (ret) {
				dev_err(&client->dev, "Failed to set fixed grid 8x6 params\n");
			} else {
				dev_info(&client->dev, "Set fixed grid 8x6: x_off=%d, y_off=%d, w=%d, h=%d\n",
					pdaf_cfg->fixed_win.area_x_offset,
					pdaf_cfg->fixed_win.area_y_offset,
					pdaf_cfg->fixed_win.area_width,
					pdaf_cfg->fixed_win.area_height);
			}
			break;

		case FLOAT_WINDOW:
			/* Set flexible window mode */
			ret = imx582_write_reg(client, IMX582_REG_AREA_MODE,
				IMX582_REG_VALUE_08BIT, IMX582_AREA_MODE_FLEXIBLE);
			if (ret) {
				dev_err(&client->dev, "Failed to set flexible window mode\n");
				break;
			}

			/* Disable all windows first */
			for (j = 0; j < IMX582_MAX_FLEXIBLE_WINDOWS; j++) {
				ret |= imx582_write_reg(client, IMX582_REG_AREA_EN_0 + j,
					IMX582_REG_VALUE_08BIT, IMX582_AREA_EN_DISABLE);
			}

			/* Configure each enabled window */
			for (j = 0; j < pdaf_cfg->float_win.win_num && j < MAX_FLOAT_WINDOW; j++) {
				u16 base_addr = IMX582_REG_PD_AREA_FLEX_BASE + j *
						IMX582_REG_PD_AREA_FLEX_STRIDE;

				/* Set window start X coordinate */
				ret |= imx582_write_reg(client, base_addr,
					IMX582_REG_VALUE_08BIT,
					(pdaf_cfg->float_win.win[j].x_sta >> 8) & IMX582_PD_COORD_MASK_H);
				ret |= imx582_write_reg(client, base_addr + 1,
					IMX582_REG_VALUE_08BIT,
					pdaf_cfg->float_win.win[j].x_sta & 0xFF);

				/* Set window start Y coordinate */
				ret |= imx582_write_reg(client, base_addr + 2,
					IMX582_REG_VALUE_08BIT,
					(pdaf_cfg->float_win.win[j].y_sta >> 8) & IMX582_PD_COORD_MASK_H);
				ret |= imx582_write_reg(client, base_addr + 3,
					IMX582_REG_VALUE_08BIT,
					pdaf_cfg->float_win.win[j].y_sta & 0xFF);

				/* Set window end X coordinate */
				ret |= imx582_write_reg(client, base_addr + 4,
					IMX582_REG_VALUE_08BIT,
					(pdaf_cfg->float_win.win[j].x_end >> 8) & IMX582_PD_COORD_MASK_H);
				ret |= imx582_write_reg(client, base_addr + 5,
					IMX582_REG_VALUE_08BIT,
					pdaf_cfg->float_win.win[j].x_end & 0xFF);

				/* Set window end Y coordinate */
				ret |= imx582_write_reg(client, base_addr + 6,
					IMX582_REG_VALUE_08BIT,
					(pdaf_cfg->float_win.win[j].y_end >> 8) & IMX582_PD_COORD_MASK_H);
				ret |= imx582_write_reg(client, base_addr + 7,
					IMX582_REG_VALUE_08BIT,
					pdaf_cfg->float_win.win[j].y_end & 0xFF);

				/* Enable this window */
				ret |= imx582_write_reg(client, IMX582_REG_AREA_EN_0 + j,
					IMX582_REG_VALUE_08BIT, IMX582_AREA_EN_ENABLE);

				dev_info(&client->dev, "Set flexible window[%d]: x_sta=%d, y_sta=%d, x_end=%d, y_end=%d\n",
					j, pdaf_cfg->float_win.win[j].x_sta,
					pdaf_cfg->float_win.win[j].y_sta,
					pdaf_cfg->float_win.win[j].x_end,
					pdaf_cfg->float_win.win[j].y_end);
			}

			if (ret) {
				dev_err(&client->dev, "Failed to set flexible window params\n");
			} else {
				dev_info(&client->dev, "Set flexible window mode with %d windows\n",
					pdaf_cfg->float_win.win_num);
			}
			break;

		default:
			dev_err(&client->dev, "Invalid PDAF window mode: %d\n",
				pdaf_cfg->win_mode);
			ret = -EINVAL;
			break;
		}
		break;
	}

	case RKMODULE_GET_PDAF_WIN_CFG:
	{
		pdaf_cfg = (struct rkmodule_pdaf_win_cfg_t *)arg;
		struct i2c_client *client = imx582->client;
		u32 val = 0;
		u32 j;

		/* Read current window mode */
		ret = imx582_read_reg(client, IMX582_REG_AREA_MODE,
			IMX582_REG_VALUE_08BIT, &val);
		if (ret) {
			dev_err(&client->dev, "Failed to read PDAF window mode\n");
			break;
		}

		pdaf_cfg->win_mode = val & IMX582_AREA_MODE_MASK;
		switch (pdaf_cfg->win_mode) {
		case FIXED_GRID_WIN_16_12:
		case FIXED_GRID_WIN_8_6:
			/* Read fixed window parameters */
			ret = imx582_read_reg(client, IMX582_REG_PD_AREA_X_OFFSET_H,
				IMX582_REG_VALUE_08BIT, &val);
			pdaf_cfg->fixed_win.area_x_offset = (val & IMX582_PD_COORD_MASK_H) << 8;
			ret |= imx582_read_reg(client, IMX582_REG_PD_AREA_X_OFFSET_L,
				IMX582_REG_VALUE_08BIT, &val);
			pdaf_cfg->fixed_win.area_x_offset |= val;

			ret |= imx582_read_reg(client, IMX582_REG_PD_AREA_Y_OFFSET_H,
				IMX582_REG_VALUE_08BIT, &val);
			pdaf_cfg->fixed_win.area_y_offset = (val & IMX582_PD_COORD_MASK_H) << 8;
			ret |= imx582_read_reg(client, IMX582_REG_PD_AREA_Y_OFFSET_L,
				IMX582_REG_VALUE_08BIT, &val);
			pdaf_cfg->fixed_win.area_y_offset |= val;

			ret |= imx582_read_reg(client, IMX582_REG_PD_AREA_WIDTH_H,
				IMX582_REG_VALUE_08BIT, &val);
			pdaf_cfg->fixed_win.area_width = (val & IMX582_PD_COORD_MASK_H) << 8;
			ret |= imx582_read_reg(client, IMX582_REG_PD_AREA_WIDTH_L,
				IMX582_REG_VALUE_08BIT, &val);
			pdaf_cfg->fixed_win.area_width |= val;

			ret |= imx582_read_reg(client, IMX582_REG_PD_AREA_HEIGHT_H,
				IMX582_REG_VALUE_08BIT, &val);
			pdaf_cfg->fixed_win.area_height = (val & IMX582_PD_COORD_MASK_H) << 8;
			ret |= imx582_read_reg(client, IMX582_REG_PD_AREA_HEIGHT_L,
				IMX582_REG_VALUE_08BIT, &val);
			pdaf_cfg->fixed_win.area_height |= val;

			dev_info(&client->dev, "Get fixed grid: mode=%d, x_off=%d, y_off=%d, w=%d, h=%d\n",
				pdaf_cfg->win_mode,
				pdaf_cfg->fixed_win.area_x_offset,
				pdaf_cfg->fixed_win.area_y_offset,
				pdaf_cfg->fixed_win.area_width,
				pdaf_cfg->fixed_win.area_height);
			break;
		case IMX582_AREA_MODE_FLEXIBLE:
			/* Read flexible window parameters */
			pdaf_cfg->float_win.win_num = 0;
			for (j = 0; j < IMX582_MAX_FLEXIBLE_WINDOWS; j++) {
				/* Check if window is enabled */
				ret |= imx582_read_reg(client, IMX582_REG_AREA_EN_0 + j,
					IMX582_REG_VALUE_08BIT, &val);
				if (!(val & IMX582_AREA_EN_MASK))
					continue;

				u16 base_addr = IMX582_REG_PD_AREA_FLEX_BASE + j *
						IMX582_REG_PD_AREA_FLEX_STRIDE;

				/* Read window start X coordinate */
				ret |= imx582_read_reg(client, base_addr,
					IMX582_REG_VALUE_08BIT, &val);
				pdaf_cfg->float_win.win[j].x_sta = (val & IMX582_PD_COORD_MASK_H) << 8;
				ret |= imx582_read_reg(client, base_addr + 1,
					IMX582_REG_VALUE_08BIT, &val);
				pdaf_cfg->float_win.win[j].x_sta |= val;

				/* Read window start Y coordinate */
				ret |= imx582_read_reg(client, base_addr + 2,
					IMX582_REG_VALUE_08BIT, &val);
				pdaf_cfg->float_win.win[j].y_sta = (val & IMX582_PD_COORD_MASK_H) << 8;
				ret |= imx582_read_reg(client, base_addr + 3,
					IMX582_REG_VALUE_08BIT, &val);
				pdaf_cfg->float_win.win[j].y_sta |= val;

				/* Read window end X coordinate */
				ret |= imx582_read_reg(client, base_addr + 4,
					IMX582_REG_VALUE_08BIT, &val);
				pdaf_cfg->float_win.win[j].x_end = (val & IMX582_PD_COORD_MASK_H) << 8;
				ret |= imx582_read_reg(client, base_addr + 5,
					IMX582_REG_VALUE_08BIT, &val);
				pdaf_cfg->float_win.win[j].x_end |= val;

				/* Read window end Y coordinate */
				ret |= imx582_read_reg(client, base_addr + 6,
					IMX582_REG_VALUE_08BIT, &val);
				pdaf_cfg->float_win.win[j].y_end = (val & IMX582_PD_COORD_MASK_H) << 8;
				ret |= imx582_read_reg(client, base_addr + 7,
					IMX582_REG_VALUE_08BIT, &val);
				pdaf_cfg->float_win.win[j].y_end |= val;

				pdaf_cfg->float_win.win_num++;

				dev_info(&client->dev, "Get flexible window[%d]: x_sta=%d, y_sta=%d, x_end=%d, y_end=%d\n",
					j, pdaf_cfg->float_win.win[j].x_sta,
					pdaf_cfg->float_win.win[j].y_sta,
					pdaf_cfg->float_win.win[j].x_end,
					pdaf_cfg->float_win.win[j].y_end);
			}

			dev_info(&client->dev, "Get flexible window mode with %d windows\n",
				pdaf_cfg->float_win.win_num);
			break;
		}
		break;
	}

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx582_compat_ioctl32(struct v4l2_subdev *sd,
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
	struct rkmodule_pdaf_win_cfg_t *pdaf_cfg;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx582_ioctl(sd, cmd, inf);
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
			ret = imx582_ioctl(sd, cmd, cfg);
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

		ret = imx582_ioctl(sd, cmd, hdr);
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
			ret = imx582_ioctl(sd, cmd, hdr);
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
			ret = imx582_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = imx582_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}
		ret = imx582_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	case RKMODULE_GET_PDAF_WIN_CFG:
		pdaf_cfg = kzalloc(sizeof(*pdaf_cfg), GFP_KERNEL);
		if (!pdaf_cfg) {
			ret = -ENOMEM;
			return ret;
		}
		ret = imx582_ioctl(sd, cmd, pdaf_cfg);
		if (!ret) {
			ret = copy_to_user(up, pdaf_cfg, sizeof(*pdaf_cfg));
			if (ret)
				ret = -EFAULT;
		}
		kfree(pdaf_cfg);
		break;
	case RKMODULE_SET_PDAF_WIN_CFG:
		pdaf_cfg = kzalloc(sizeof(*pdaf_cfg), GFP_KERNEL);
		if (!pdaf_cfg) {
			ret = -ENOMEM;
			return ret;
		}
		ret = copy_from_user(pdaf_cfg, up, sizeof(*pdaf_cfg));
		if (!ret)
			ret = imx582_ioctl(sd, cmd, pdaf_cfg);
		else
			ret = -EFAULT;
		kfree(pdaf_cfg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int __imx582_start_stream(struct imx582 *imx582)
{
	int ret;

	ret = imx582_write_array(imx582->client, imx582->cur_mode->global_reg_list);
	if (ret)
		return ret;

	ret = imx582_write_array(imx582->client, imx582->cur_mode->reg_list);
	if (ret)
		return ret;

	imx582->cur_vts = imx582->cur_mode->vts_def;
	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&imx582->ctrl_handler);
	if (ret)
		return ret;
	if (imx582->has_init_exp && imx582->cur_mode->hdr_mode != NO_HDR) {
		ret = imx582_ioctl(&imx582->subdev, PREISP_CMD_SET_HDRAE_EXP,
			&imx582->init_hdrae_exp);
		if (ret) {
			dev_err(&imx582->client->dev,
				"init exp fail in hdr mode\n");
			return ret;
		}
	}

	return imx582_write_reg(imx582->client, IMX582_REG_CTRL_MODE,
				IMX582_REG_VALUE_08BIT, IMX582_MODE_STREAMING);
}

static int __imx582_stop_stream(struct imx582 *imx582)
{
	return imx582_write_reg(imx582->client, IMX582_REG_CTRL_MODE,
				IMX582_REG_VALUE_08BIT, IMX582_MODE_SW_STANDBY);
}

static int imx582_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx582 *imx582 = to_imx582(sd);
	struct i2c_client *client = imx582->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				imx582->cur_mode->width,
				imx582->cur_mode->height,
		DIV_ROUND_CLOSEST(imx582->cur_mode->max_fps.denominator,
				  imx582->cur_mode->max_fps.numerator));

	mutex_lock(&imx582->mutex);
	on = !!on;
	if (on == imx582->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx582_start_stream(imx582);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx582_stop_stream(imx582);
		pm_runtime_put(&client->dev);
	}

	imx582->streaming = on;

unlock_and_return:
	mutex_unlock(&imx582->mutex);

	return ret;
}

static int imx582_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx582 *imx582 = to_imx582(sd);
	struct i2c_client *client = imx582->client;
	int ret = 0;

	mutex_lock(&imx582->mutex);

	/* If the power state is not modified - no work to do. */
	if (imx582->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		imx582->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx582->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx582->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 imx582_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, IMX582_XVCLK_FREQ / 1000 / 1000);
}

static int __imx582_power_on(struct imx582 *imx582)
{
	int ret;
	u32 delay_us;
	struct device *dev = &imx582->client->dev;

	ret = clk_set_rate(imx582->xvclk, IMX582_XVCLK_FREQ);
	if (ret < 0) {
		dev_err(dev, "Failed to set xvclk rate (24MHz)\n");
		return ret;
	}
	if (clk_get_rate(imx582->xvclk) != IMX582_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 37.125MHz\n");
	ret = clk_prepare_enable(imx582->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (!IS_ERR(imx582->reset_gpio))
		gpiod_set_value_cansleep(imx582->reset_gpio, 0);

	ret = regulator_bulk_enable(IMX582_NUM_SUPPLIES, imx582->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(imx582->reset_gpio))
		gpiod_set_value_cansleep(imx582->reset_gpio, 1);

	/* need wait 8ms to set register */
	usleep_range(8000, 10000);

	if (!IS_ERR(imx582->pwdn_gpio))
		gpiod_set_value_cansleep(imx582->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = imx582_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(imx582->xvclk);

	return ret;
}

static void __imx582_power_off(struct imx582 *imx582)
{

	if (!IS_ERR(imx582->pwdn_gpio))
		gpiod_set_value_cansleep(imx582->pwdn_gpio, 0);
	clk_disable_unprepare(imx582->xvclk);
	if (!IS_ERR(imx582->reset_gpio))
		gpiod_set_value_cansleep(imx582->reset_gpio, 0);
	regulator_bulk_disable(IMX582_NUM_SUPPLIES, imx582->supplies);
}

static int imx582_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx582 *imx582 = to_imx582(sd);

	return __imx582_power_on(imx582);
}

static int imx582_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx582 *imx582 = to_imx582(sd);

	__imx582_power_off(imx582);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx582_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx582 *imx582 = to_imx582(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct imx582_mode *def_mode = &supported_modes[0];

	mutex_lock(&imx582->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx582->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int imx582_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_interval_enum *fie)
{
	struct imx582 *imx582 = to_imx582(sd);

	if (fie->index >= imx582->cfg_num)
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_3840 3840
#define DST_HEIGHT_2160 2160

static int imx582_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_selection *sel)
{
	struct imx582 *imx582 = to_imx582(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = CROP_START(imx582->cur_mode->width, DST_WIDTH_3840);
		sel->r.width = DST_WIDTH_3840;
		sel->r.top = CROP_START(imx582->cur_mode->height, DST_HEIGHT_2160);
		sel->r.height = DST_HEIGHT_2160;
		return 0;
	}
	return -EINVAL;
}

static const struct dev_pm_ops imx582_pm_ops = {
	SET_RUNTIME_PM_OPS(imx582_runtime_suspend,
			   imx582_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx582_internal_ops = {
	.open = imx582_open,
};
#endif

static const struct v4l2_subdev_core_ops imx582_core_ops = {
	.s_power = imx582_s_power,
	.ioctl = imx582_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx582_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx582_video_ops = {
	.s_stream = imx582_s_stream,
	.g_frame_interval = imx582_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx582_pad_ops = {
	.enum_mbus_code = imx582_enum_mbus_code,
	.enum_frame_size = imx582_enum_frame_sizes,
	.enum_frame_interval = imx582_enum_frame_interval,
	.get_fmt = imx582_get_fmt,
	.set_fmt = imx582_set_fmt,
	.get_selection = imx582_get_selection,
	.get_mbus_config = imx582_g_mbus_config,
};

static const struct v4l2_subdev_ops imx582_subdev_ops = {
	.core	= &imx582_core_ops,
	.video	= &imx582_video_ops,
	.pad	= &imx582_pad_ops,
};

static int imx582_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx582 *imx582 = container_of(ctrl->handler,
					     struct imx582, ctrl_handler);
	struct i2c_client *client = imx582->client;
	s64 max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = imx582->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(imx582->exposure,
					 imx582->exposure->minimum, max,
					 imx582->exposure->step,
					 imx582->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = imx582_write_reg(imx582->client,
				       IMX582_REG_EXPOSURE_H,
				       IMX582_REG_VALUE_08BIT,
				       IMX582_FETCH_EXP_H(ctrl->val));
		ret |= imx582_write_reg(imx582->client,
					IMX582_REG_EXPOSURE_L,
					IMX582_REG_VALUE_08BIT,
					IMX582_FETCH_EXP_L(ctrl->val));
		dev_dbg(&client->dev, "set exposure 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = imx582_write_reg(imx582->client, IMX582_REG_GAIN_H,
				       IMX582_REG_VALUE_08BIT,
				       IMX582_FETCH_AGAIN_H(ctrl->val));
		ret |= imx582_write_reg(imx582->client, IMX582_REG_GAIN_L,
					IMX582_REG_VALUE_08BIT,
					IMX582_FETCH_AGAIN_L(ctrl->val));

		dev_dbg(&client->dev, "set analog gain 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = imx582_write_reg(imx582->client,
				       IMX582_REG_VTS_H,
				       IMX582_REG_VALUE_08BIT,
				       (ctrl->val + imx582->cur_mode->height)
				       >> 8);
		ret |= imx582_write_reg(imx582->client,
					IMX582_REG_VTS_L,
					IMX582_REG_VALUE_08BIT,
					(ctrl->val + imx582->cur_mode->height)
					& 0xff);
		imx582->cur_vts = ctrl->val + imx582->cur_mode->height;

		dev_dbg(&client->dev, "set vblank 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->val)
			imx582->flip |= IMX582_MIRROR_BIT_MASK;
		else
			imx582->flip &= ~IMX582_MIRROR_BIT_MASK;
		dev_dbg(&client->dev, "set hflip 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		if (ctrl->val)
			imx582->flip |= IMX582_FLIP_BIT_MASK;
		else
			imx582->flip &= ~IMX582_FLIP_BIT_MASK;
		dev_dbg(&client->dev, "set vflip 0x%x\n",
			ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		dev_dbg(&client->dev, "set testpattern 0x%x\n",
			ctrl->val);
		ret = imx582_enable_test_pattern(imx582, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx582_ctrl_ops = {
	.s_ctrl = imx582_set_ctrl,
};

static int imx582_initialize_controls(struct imx582 *imx582)
{
	const struct imx582_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &imx582->ctrl_handler;
	mode = imx582->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &imx582->mutex;

	imx582->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);

	imx582->cur_link_freq = imx582->cur_mode->mipi_freq_idx;
	imx582->cur_pixel_rate = link_freq_items[imx582->cur_link_freq] /
				 10 * 2 * 4;

	imx582->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
					       V4L2_CID_PIXEL_RATE,
					       0, PIXEL_RATE_WITH_2196M_10BIT,
					       1, imx582->cur_pixel_rate);
	v4l2_ctrl_s_ctrl(imx582->link_freq,
			   imx582->cur_link_freq);

	h_blank = mode->hts_def - mode->width;
	imx582->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	if (imx582->hblank)
		imx582->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx582->vblank = v4l2_ctrl_new_std(handler, &imx582_ctrl_ops,
					   V4L2_CID_VBLANK, vblank_def,
					   IMX582_VTS_MAX - mode->height,
					   1, vblank_def);
	imx582->cur_vts = mode->vts_def;
	exposure_max = mode->vts_def - 4;
	imx582->exposure = v4l2_ctrl_new_std(handler, &imx582_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX582_EXPOSURE_MIN,
					     exposure_max,
					     IMX582_EXPOSURE_STEP,
					     mode->exp_def);
	imx582->anal_gain = v4l2_ctrl_new_std(handler, &imx582_ctrl_ops,
					      V4L2_CID_ANALOGUE_GAIN,
					      IMX582_GAIN_MIN,
					      IMX582_GAIN_MAX,
					      IMX582_GAIN_STEP,
					      IMX582_GAIN_DEFAULT);
	imx582->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							    &imx582_ctrl_ops,
				V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(imx582_test_pattern_menu) - 1,
				0, 0, imx582_test_pattern_menu);

	imx582->h_flip = v4l2_ctrl_new_std(handler, &imx582_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);

	imx582->v_flip = v4l2_ctrl_new_std(handler, &imx582_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	imx582->flip = 0;

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx582->client->dev,
			"Failed to init controls(  %d  )\n", ret);
		goto err_free_handler;
	}

	imx582->subdev.ctrl_handler = handler;
	imx582->has_init_exp = false;
	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int imx582_check_sensor_id(struct imx582 *imx582,
				  struct i2c_client *client)
{
	struct device *dev = &imx582->client->dev;
	u16 id = 0;
	u32 reg_H = 0;
	u32 reg_L = 0;
	int ret;

	ret = imx582_read_reg(client, IMX582_REG_CHIP_ID_H,
			      IMX582_REG_VALUE_08BIT, &reg_H);
	ret |= imx582_read_reg(client, IMX582_REG_CHIP_ID_L,
			       IMX582_REG_VALUE_08BIT, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (!(reg_H == (CHIP_ID >> 8) || reg_L == (CHIP_ID & 0xff))) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected imx582 %04x sensor\n", id);
	return 0;
}

static int imx582_configure_regulators(struct imx582 *imx582)
{
	unsigned int i;

	for (i = 0; i < IMX582_NUM_SUPPLIES; i++)
		imx582->supplies[i].supply = imx582_supply_names[i];

	return devm_regulator_bulk_get(&imx582->client->dev,
				       IMX582_NUM_SUPPLIES,
				       imx582->supplies);
}

static int imx582_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx582 *imx582;
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

	imx582 = devm_kzalloc(dev, sizeof(*imx582), GFP_KERNEL);
	if (!imx582)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &imx582->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &imx582->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &imx582->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &imx582->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node,
				   "rockchip,spd-id",
				   &imx582->spd_id);
	if (ret != 0) {
		imx582->spd_id = PAD_MAX;
		dev_err(dev,
			"failed get spd_id, will not to use spd\n");
	} else if (imx582->spd_id >= PAD_MAX) {
		dev_err(dev, "invalid spd_id %d, must be < %d\n",
			imx582->spd_id, PAD_MAX);
		imx582->spd_id = PAD_MAX;
	}
	ret = of_property_read_u32(node,
				   "rockchip,ebd-id",
				   &imx582->ebd_id);
	if (ret != 0) {
		imx582->ebd_id = PAD_MAX;
		dev_err(dev,
			"failed get ebd_id, will not to use ebd\n");
	} else if (imx582->ebd_id >= PAD_MAX) {
		dev_err(dev, "invalid ebd_id %d, must be < %d\n",
			imx582->ebd_id, PAD_MAX);
		imx582->ebd_id = PAD_MAX;
	}

	ret = of_property_read_u32(node, OF_CAMERA_HDR_MODE, &hdr_mode);
	if (ret) {
		hdr_mode = NO_HDR;
		dev_warn(dev, " Get hdr mode failed! no hdr default\n");
	}

	imx582->client = client;
	imx582->cfg_num = ARRAY_SIZE(supported_modes);
	for (i = 0; i < imx582->cfg_num; i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			imx582->cur_mode = &supported_modes[i];
			break;
		}
	}

	if (i == imx582->cfg_num)
		imx582->cur_mode = &supported_modes[0];

	imx582->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx582->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx582->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx582->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	imx582->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(imx582->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = imx582_configure_regulators(imx582);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&imx582->mutex);

	sd = &imx582->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx582_subdev_ops);

	ret = imx582_initialize_controls(imx582);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx582_power_on(imx582);
	if (ret)
		goto err_free_handler;

	ret = imx582_check_sensor_id(imx582, client);
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
				imx582->otp = otp_ptr;
			} else {
				imx582->otp = NULL;
				devm_kfree(dev, otp_ptr);
			}
		}
	}
continue_probe:

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx582_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx582->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx582->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(imx582->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx582->module_index, facing,
		 IMX582_NAME, dev_name(sd->dev));
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
	__imx582_power_off(imx582);
err_free_handler:
	v4l2_ctrl_handler_free(&imx582->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx582->mutex);

	return ret;
}

static void imx582_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx582 *imx582 = to_imx582(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx582->ctrl_handler);
	mutex_destroy(&imx582->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx582_power_off(imx582);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx582_of_match[] = {
	{ .compatible = "sony,imx582" },
	{},
};
MODULE_DEVICE_TABLE(of, imx582_of_match);
#endif

static const struct i2c_device_id imx582_match_id[] = {
	{ "sony,imx582", 0 },
	{ },
};

static struct i2c_driver imx582_i2c_driver = {
	.driver = {
		.name = IMX582_NAME,
		.pm = &imx582_pm_ops,
		.of_match_table = of_match_ptr(imx582_of_match),
	},
	.probe		= imx582_probe,
	.remove		= imx582_remove,
	.id_table	= imx582_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx582_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx582_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx582 sensor driver");
MODULE_LICENSE("GPL");
