// SPDX-License-Identifier: GPL-2.0
/*
 * ps5458 driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 first version
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

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define PS5458_LANES			2
#define PS5458_BITS_PER_SAMPLE		10
#define PS5458_LINK_FREQ_384		384000000

#define PIXEL_RATE_WITH_384M_10BIT	(PS5458_LINK_FREQ_384  * 2 * PS5458_LANES \
					 /PS5458_BITS_PER_SAMPLE)
#define PS5458_XVCLK_FREQ		24000000

#define CHIP_ID				0x5416
#define PS5458_REG_CHIP_ID		0x0000

#define PS5458_REG_CTRL_MODE		0x0008
#define PS5458_MODE_SW_STANDBY		0x81
#define PS5458_MODE_STREAMING		0x83

#define PS5458_REG_EXPOSURE_L		0x014E //[7:0]
#define PS5458_REG_EXPOSURE_H		0x014F //[15:8]
#define	PS5458_EXPOSURE_MIN		3
#define	PS5458_EXPOSURE_MAX		0x063e
#define	PS5458_EXPOSURE_STEP		1
#define PS5458_VTS_MAX			0x7fff

#define PS5458_REG_GAIN_IDX_L		0x0150 //[7:0]
#define PS5458_REG_GAIN_IDX_H		0x0151 //[9:8] -> bit[0:1]
#define PS5458_GAIN_MIN			0x0026
#define PS5458_GAIN_MAX			(576)
#define PS5458_GAIN_STEP		1
#define PS5458_GAIN_DEFAULT		0x0026

#define PS5458_REG_EXPOSURE_UPDATE	0x0156 //[1:0] -> bit[0:1]
#define PS5458_EXPOSURE_UPDATE		0x03

#define PS5458_REG_GROUP_HOLD		0x3812
#define PS5458_GROUP_HOLD_START		0x00
#define PS5458_GROUP_HOLD_END		0x30

#define PS5458_REG_TEST_PATTERN		0x040a
#define PS5458_TEST_PATTERN_BIT_MASK	BIT(3)

#define PS5458_REG_VTS_H		0x011f
#define PS5458_REG_VTS_L		0x011e

#define PS5458_FLIP_MIRROR_REG		0x01CE
#define PS5458_GLOBE_UPDATE_REG		0x00EB

#define PS5458_FETCH_EXP_H(VAL)		(((VAL) >> 8) & 0xFF)
#define PS5458_FETCH_EXP_L(VAL)		((VAL) & 0xFF)

#define PS5458_FETCH_MIRROR(VAL, ENABLE)	(ENABLE ? VAL | 0x04 : VAL & 0xfb)
#define PS5458_FETCH_FLIP(VAL, ENABLE)		(ENABLE ? VAL | 0x08 : VAL & 0xf7)

#define REG_DELAY			0xFFFE
#define REG_NULL			0xFFFF

#define PS5458_REG_VALUE_08BIT		1
#define PS5458_REG_VALUE_16BIT		2
#define PS5458_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define PS5458_NAME			"ps5458"

static const char * const ps5458_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define PS5458_NUM_SUPPLIES ARRAY_SIZE(ps5458_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct ps5458_mode {
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
};

struct ps5458 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct regulator_bulk_data supplies[PS5458_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	struct v4l2_fract	cur_fps;
	bool			streaming;
	bool			power_on;
	const struct ps5458_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	bool			is_thunderboot;
	bool			is_first_streamoff;
	struct preisp_hdrae_exp_s init_hdrae_exp;
};

#define to_ps5458(sd) container_of(sd, struct ps5458, subdev)

/*
 * Xclk 27Mhz
 */
static const struct regval ps5458_global_regs[] = {
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 768Mbps, 2lane
 */
static const struct regval ps5458_linear_10_2560x1440_30fps_regs[] = {
	{0x00FF, 0xA5},
	{0x00A9, 0x00},//R_FAE_Auto_EnH=0 due to customer no used
	{0x00A8, 0x02},//R_FAE_Skip=1 due to customer no used
	{0x238F, 0x01},
	{0x07D8, 0x17},
	{0x07D4, 0x22},
	{0x2228, 0x01},
	{0x0709, 0x6E},
	{0x079A, 0xF5},
	{0x079C, 0x13},
	{0x079D, 0x08},
	{0x0220, 0x2F},
	{0x0222, 0x12},
	{0x0224, 0x1D},
	{0x0226, 0x24},
	{0x0228, 0x2F},
	{0x022A, 0x10},
	{0x022C, 0x1B},
	{0x0230, 0x6F},
	{0x0234, 0x23},
	{0x02CE, 0x40},
	{0x02CF, 0x40},
	{0x02EF, 0x1C},
	{0x02F1, 0x30},
	{0x0736, 0x0A},
	{0x0737, 0x0B},
	{0x073C, 0x0A},
	{0x073D, 0x0B},
	{0x0752, 0x1E},
	{0x075C, 0x1E},
	{0x0762, 0x9B},
	{0x0770, 0xA5},
	{0x0778, 0xA0},
	{0x077E, 0xB1},
	{0x077F, 0x00},
	{0x0784, 0xA7},
	{0x07B5, 0xB9},
	{0x07B6, 0x00},
	{0x07BD, 0xAF},
	{0x0805, 0xEB},
	{0x0806, 0x00},
	{0x080B, 0x31},
	{0x081B, 0x30},
	{0x081C, 0x1F},
	{0x082F, 0x30},
	{0x0830, 0x1F},
	{0x086F, 0xC3},
	{0x0870, 0x00},
	{0x0871, 0xBE},
	{0x0872, 0x00},
	{0x0873, 0x9B},
	{0x0875, 0xAF},
	{0x0876, 0x00},
	{0x0877, 0xB9},
	{0x0879, 0xB4},
	{0x087B, 0xA5},
	{0x0107, 0x1E},
	{0x000C, 0x02},
	{0x2256, 0x70},
	{0x0723, 0x78},
	{0x0724, 0x00},
	{0x0725, 0xF0},
	{0x0726, 0x00},
	{0x0734, 0x04},
	{0x0735, 0x05},
	{0x073A, 0x04},
	{0x073B, 0x05},
	{0x0750, 0x6E},
	{0x075A, 0x6E},
	{0x0760, 0xA0},
	{0x076E, 0xAF},
	{0x0776, 0xAA},
	{0x077C, 0xC3},
	{0x077D, 0x00},
	{0x0782, 0x5F},
	{0x07B3, 0x22},
	{0x07BB, 0xBE},
	{0x07BC, 0x00},
	{0x0803, 0x8B},
	{0x0809, 0x53},
	{0x0861, 0x2C},
	{0x0863, 0x27},
	{0x0865, 0xA0},
	{0x0867, 0xBE},
	{0x0869, 0xC8},
	{0x086A, 0x00},
	{0x086B, 0xC3},
	{0x086C, 0x00},
	{0x086D, 0x5A},
	{0x0819, 0x33},
	{0x081A, 0x01},
	{0x082D, 0x31},
	{0x082E, 0x01},
	{0x0552, 0x01},
	{0x056E, 0xFE},
	{0x0732, 0x09},
	{0x0733, 0x0A},
	{0x0738, 0x09},
	{0x0739, 0x0A},
	{0x074E, 0x6E},
	{0x0758, 0x6E},
	{0x075E, 0xF4},
	{0x076C, 0x30},
	{0x076D, 0x02},
	{0x0774, 0x26},
	{0x0775, 0x02},
	{0x077A, 0x49},
	{0x077B, 0x02},
	{0x0780, 0xAF},
	{0x07B1, 0xB2},
	{0x07B9, 0x18},
	{0x0801, 0x7F},
	{0x0802, 0x03},
	{0x0807, 0x47},
	{0x0811, 0x7A},
	{0x0812, 0x03},
	{0x0817, 0xAA},
	{0x0818, 0x02},
	{0x0825, 0x7A},
	{0x0826, 0x03},
	{0x082B, 0xA8},
	{0x082C, 0x02},
	{0x0853, 0xBC},
	{0x0855, 0xB7},
	{0x0857, 0xF4},
	{0x0859, 0x44},
	{0x085A, 0x02},
	{0x085B, 0x22},
	{0x085D, 0x1D},
	{0x085F, 0xAA},
	{0x007b, 0xC0}, //VSYNC IO
	{0x007c, 0xC0}, //HSYNC IO
	{0x007d, 0xC0}, //PXCLK IO
	{0x007e, 0xC0}, //PXDX IO
	{0x00af, 0x01}, //HSYNC/VSYNC polarity
	{0x0509, 0x28},
	{0x0510, 0x02},//4->2 for MIPI 2Lane
	{0x0511, 0x00},//non-continues mode
	{0x0515, 0x08},
	{0x0518, 0x06},//R_HsEoT_prd=6 to satisfy MIPI timing spec.
	{0x055B, 0x10}, //R_MIPI_FrameReset_by_Vsync_En
	{0x05B0, 0x05},
	{0x06B6, 0x00},
	{0x0620, 0x00},
	{0x0622, 0x08},
	{0x0617, 0x98},
	{0x0618, 0x08},
	{0x0619, 0xFC},
	{0x061A, 0x08},
	{0x062E, 0x98},
	{0x062F, 0x08},
	{0x0630, 0xFC},
	{0x0631, 0x08},
	{0x01BF, 0x02},
	{0x01C4, 0x03},
	{0x0266, 0x11},
	{0x02D8, 0x08},
	{0x02E7, 0x83},
	{0x02DA, 0x73},
	{0x0600, 0xB3},
	{0x0601, 0x06},
	{0x234a, 0x01},
	{0x234b, 0x40},
	{0x234c, 0x27},
	{0x2343, 0x02},
	{0x2340, 0x01},
	{0x07D9, 0x20},
	{0x07DA, 0x01},
	{0x0731, 0x00},
	{0x0706, 0x15},
	{0x080D, 0x08},
	{0x0821, 0xFF},
	{0x0822, 0x03},
	{0x0835, 0xFF},
	{0x0836, 0x03},
	{0x2247, 0xB6},
	{0x070A, 0x92},
	{0x0708, 0x34},
	{0x0543, 0x02},
	{0x050F, 0x01},
	{0x01E6, 0x00},
	{0x01C6, 0xA4},
	{0x01C7, 0x02},
	{0x01C8, 0x00},
	{0x01C9, 0x06},
	{0x01CF, 0x00},//R_TG_WOI_HSize = 2560
	{0x01D0, 0x0a},//
	{0x01d1, 0xA0},//R_TG_WOI_VSize = 1440
	{0x01d2, 0x05},
	{0x01D3, 0x44}, //d3 = R_TG_WOI_HStart[7:0]=68 for 2560x1440
	{0x01D5, 0x30}, //d5 = R_TG_WOI_VStart[7:0]=48 for 2560x1440
	{0x01D7, 0x00},
	{0x01D8, 0x00},
	{0x0064, 0x01},
	{0x0123, 0x01},
	{0x01e7, 0x07},
	{0x01e8, 0x00},
	{0x2224, 0x01},
	{0x2229, 0x02},
	{0x2226, 0x00},
	{0x222A, 0x01},
	{0x010B, 0x64},
	{0x011E, 0x40},
	{0x011F, 0x06},
	{0x00EB, 0x01},
	{0x2340, 0x00},
	{0x234D, 0x04},
	{0x234E, 0x80},
	{0x234F, 0x26},
	{0x2343, 0x03},
	{0x2340, 0x01},
	{0x07D9, 0x27},
	{0x07DA, 0x19},
	{0x0731, 0x00},
	{0x0706, 0x09},
	{0x080D, 0xB0},
	{0x0821, 0xAC},
	{0x0822, 0x00},
	{0x0835, 0xA8},
	{0x0836, 0x00},
	{0x2247, 0xD0},
	{0x070A, 0xB1},
	{0x0708, 0x30},
	{0x01E6, 0x10},
	{0x01C6, 0x52},
	{0x01C7, 0x01},
	{0x01C8, 0x70},
	{0x01C9, 0x01},
	{0x01CF, 0x88},//R_TG_WOI_HSize=648
	{0x01D0, 0x02},//R_TG_WOI_HSize=648
	{0x01D1, 0x70},//R_TG_WOI_VSize=368
	{0x01D2, 0x01},//R_TG_WOI_VSize=368
	{0x01D3, 0x08}, //d3 = R_TG_WOI_HStart[7:0]=8 for 648x368
	{0x01D5, 0x00}, //d5 = R_TG_WOI_VStart[7:0]=0 for 648x368
	{0x01D7, 0x03},
	{0x01D8, 0x00},
	{0x0064, 0x03},
	{0x0123, 0x02},
	{0x01e7, 0x0C},
	{0x01e8, 0x01},
	{0x2224, 0x04},
	{0x2229, 0x00},
	{0x2226, 0x01},
	{0x222A, 0x01},
	{0x0494, 0x00},
	{0x010B, 0x60},
	{0x011E, 0xF7},//R_LPF=8695 for Pre-roll 5fps (Pxclk=24MHz)
	{0x011F, 0x21},//R_LPF=8695 for Pre-roll 5fps (Pxclk=24MHz)
	{0x00eb, 0x01},
	{0x2340, 0x00},
	{0x2352, 0x01},
	{0x2353, 0x01},
	{0x2354, 0x01},
	{0x2342, 0x01},
	{0x0506, 0x04},//06 <={1'b0,1'b0,1'b0,1'b0,1'b0, R_Data_Format[2:0]}
	{0x00eb, 0x01},//updateflag
	{0x002E, 0x00},
	{0x002F, 0x00},//R_ExpGain_AutoCalc_Stream_En=0, temp setting
	{0x0149, 0xF5},
	{0x014A, 0x21},
	{0x014B, 0x26},
	{0x014C, 0x00},
	{0x014E, 0x3E},//R_ExpLine_Stream=1598 (R_LPF - 2)
	{0x014F, 0x06},//R_ExpLine_Stream=1598 (R_LPF - 2)
	{0x0150, 0x40},//R_GainIndex_Stream=64 (AG=2X)
	{0x0151, 0x00},//R_GainIndex_Stream=64 (AG=2X)
	{0x0156, 0x03},//AE_UpdateFlag
	{0x01ce, 0x00},//R_TG_HFlip=0, R_TG_VFlip=0
	{0x005F, 0x01},//Rs_Np_Sel=0->1 for Stream 10bit 30fps
	{0x00eb, 0x01},
	{0x05B7, 0xB0},//B03A: Version (Dummy register)
	{0x05B8, 0x3A},//B03A: Version (Dummy register)
	{0x2343, 0x02},
	{0x2341, 0x00},
	{0x2341, 0x01},
	//{0x0008, 0x83},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 15fps
 * mipi_datarate per lane 768Mbps, 2lane
 */
static const struct regval ps5458_linear_10_2560x1440_15fps_regs[] = {
	{0x00FF, 0xA5},
	{0x00A9, 0x00},//R_FAE_Auto_EnH=0 due to customer no used
	{0x00A8, 0x02},//R_FAE_Skip=1 due to customer no used
	{0x238F, 0x01},
	{0x07D8, 0x17},
	{0x07D4, 0x22},
	{0x2228, 0x01},
	{0x0709, 0x6E},
	{0x079A, 0xF5},
	{0x079C, 0x13},
	{0x079D, 0x08},
	{0x0220, 0x2F},
	{0x0222, 0x12},
	{0x0224, 0x1D},
	{0x0226, 0x24},
	{0x0228, 0x2F},
	{0x022A, 0x10},
	{0x022C, 0x1B},
	{0x0230, 0x6F},
	{0x0234, 0x23},
	{0x02CE, 0x40},
	{0x02CF, 0x40},
	{0x02EF, 0x1C},
	{0x02F1, 0x30},
	{0x0736, 0x0A},
	{0x0737, 0x0B},
	{0x073C, 0x0A},
	{0x073D, 0x0B},
	{0x0752, 0x1E},
	{0x075C, 0x1E},
	{0x0762, 0x9B},
	{0x0770, 0xA5},
	{0x0778, 0xA0},
	{0x077E, 0xB1},
	{0x077F, 0x00},
	{0x0784, 0xA7},
	{0x07B5, 0xB9},
	{0x07B6, 0x00},
	{0x07BD, 0xAF},
	{0x0805, 0xEB},
	{0x0806, 0x00},
	{0x080B, 0x31},
	{0x081B, 0x30},
	{0x081C, 0x1F},
	{0x082F, 0x30},
	{0x0830, 0x1F},
	{0x086F, 0xC3},
	{0x0870, 0x00},
	{0x0871, 0xBE},
	{0x0872, 0x00},
	{0x0873, 0x9B},
	{0x0875, 0xAF},
	{0x0876, 0x00},
	{0x0877, 0xB9},
	{0x0879, 0xB4},
	{0x087B, 0xA5},
	{0x0107, 0x1E},
	{0x000C, 0x02},
	{0x2256, 0x70},
	{0x0723, 0x78},
	{0x0724, 0x00},
	{0x0725, 0xF0},
	{0x0726, 0x00},
	{0x0734, 0x04},
	{0x0735, 0x05},
	{0x073A, 0x04},
	{0x073B, 0x05},
	{0x0750, 0x6E},
	{0x075A, 0x6E},
	{0x0760, 0xA0},
	{0x076E, 0xAF},
	{0x0776, 0xAA},
	{0x077C, 0xC3},
	{0x077D, 0x00},
	{0x0782, 0x5F},
	{0x07B3, 0x22},
	{0x07BB, 0xBE},
	{0x07BC, 0x00},
	{0x0803, 0x8B},
	{0x0809, 0x53},
	{0x0861, 0x2C},
	{0x0863, 0x27},
	{0x0865, 0xA0},
	{0x0867, 0xBE},
	{0x0869, 0xC8},
	{0x086A, 0x00},
	{0x086B, 0xC3},
	{0x086C, 0x00},
	{0x086D, 0x5A},
	{0x0819, 0x33},
	{0x081A, 0x01},
	{0x082D, 0x31},
	{0x082E, 0x01},
	{0x0552, 0x01},
	{0x056E, 0xFE},
	{0x0732, 0x09},
	{0x0733, 0x0A},
	{0x0738, 0x09},
	{0x0739, 0x0A},
	{0x074E, 0x6E},
	{0x0758, 0x6E},
	{0x075E, 0xF4},
	{0x076C, 0x30},
	{0x076D, 0x02},
	{0x0774, 0x26},
	{0x0775, 0x02},
	{0x077A, 0x49},
	{0x077B, 0x02},
	{0x0780, 0xAF},
	{0x07B1, 0xB2},
	{0x07B9, 0x18},
	{0x0801, 0x7F},
	{0x0802, 0x03},
	{0x0807, 0x47},
	{0x0811, 0x7A},
	{0x0812, 0x03},
	{0x0817, 0xAA},
	{0x0818, 0x02},
	{0x0825, 0x7A},
	{0x0826, 0x03},
	{0x082B, 0xA8},
	{0x082C, 0x02},
	{0x0853, 0xBC},
	{0x0855, 0xB7},
	{0x0857, 0xF4},
	{0x0859, 0x44},
	{0x085A, 0x02},
	{0x085B, 0x22},
	{0x085D, 0x1D},
	{0x085F, 0xAA},
	{0x007b, 0xC0}, //VSYNC IO
	{0x007c, 0xC0}, //HSYNC IO
	{0x007d, 0xC0}, //PXCLK IO
	{0x007e, 0xC0}, //PXDX IO
	{0x00af, 0x01}, //HSYNC/VSYNC polarity
	{0x0509, 0x12},//40->18 for MIPI data rate=384MHz
	{0x0510, 0x02},//4->2 for MIPI 2Lane
	{0x0511, 0x00},//non-continues mode
	{0x0515, 0x04},//8->4 for MIPI data rate=384MHz
	{0x0517, 0x03},//3 for MIPI data rate=384MHz
	{0x0518, 0x02},//6->2 for MIPI data rate=384MHz
	{0x0540, 0x25},//T_PLL_PREDIVIDER_S=17->37 for MIPI data rate=384MHz
	{0x05EB, 0x80},//MIPI_PLL_Update=1
	{0x055B, 0x10}, //R_MIPI_FrameReset_by_Vsync_En
	{0x05B0, 0x05},
	{0x06B6, 0x00},
	{0x0620, 0x00},
	{0x0622, 0x08},
	{0x0617, 0x98},
	{0x0618, 0x08},
	{0x0619, 0xFC},
	{0x061A, 0x08},
	{0x062E, 0x98},
	{0x062F, 0x08},
	{0x0630, 0xFC},
	{0x0631, 0x08},
	{0x01BF, 0x02},
	{0x01C4, 0x03},
	{0x0266, 0x11},
	{0x02D8, 0x08},
	{0x02E7, 0x83},
	{0x02DA, 0x73},
	{0x0600, 0xB3},
	{0x0601, 0x06},
	{0x234a, 0x01},
	{0x234b, 0x40},
	{0x234c, 0x27},
	{0x2343, 0x02},
	{0x2340, 0x01},
	{0x07D9, 0x20},
	{0x07DA, 0x01},
	{0x0731, 0x00},
	{0x0706, 0x15},
	{0x080D, 0x08},
	{0x0821, 0xFF},
	{0x0822, 0x03},
	{0x0835, 0xFF},
	{0x0836, 0x03},
	{0x2247, 0xB6},
	{0x070A, 0x92},
	{0x0708, 0x34},
	{0x0543, 0x02},
	{0x050F, 0x01},
	{0x01E6, 0x00},
	{0x01C6, 0xA4},
	{0x01C7, 0x02},
	{0x01C8, 0x00},
	{0x01C9, 0x06},
	{0x01CF, 0x00},//R_TG_WOI_HSize = 2560
	{0x01D0, 0x0a},//
	{0x01d1, 0xA0},//R_TG_WOI_VSize = 1440
	{0x01d2, 0x05},
	{0x01D3, 0x44}, //d3 = R_TG_WOI_HStart[7:0]=68 for 2560x1440
	{0x01D5, 0x30}, //d5 = R_TG_WOI_VStart[7:0]=48 for 2560x1440
	{0x01D7, 0x00},
	{0x01D8, 0x00},
	{0x0064, 0x01},
	{0x0123, 0x01},
	{0x01e7, 0x07},
	{0x01e8, 0x00},
	{0x2224, 0x01},
	{0x2229, 0x02},
	{0x2226, 0x00},
	{0x222A, 0x01},
	{0x010B, 0x64},
	{0x011E, 0x40},
	{0x011F, 0x06},
	{0x00EB, 0x01},
	{0x2340, 0x00},
	{0x234D, 0x04},
	{0x234E, 0x80},
	{0x234F, 0x26},
	{0x2343, 0x03},
	{0x2340, 0x01},
	{0x07D9, 0x27},
	{0x07DA, 0x19},
	{0x0731, 0x00},
	{0x0706, 0x09},
	{0x080D, 0xB0},
	{0x0821, 0xAC},
	{0x0822, 0x00},
	{0x0835, 0xA8},
	{0x0836, 0x00},
	{0x2247, 0xD0},
	{0x070A, 0xB1},
	{0x0708, 0x30},
	{0x01E6, 0x10},
	{0x01C6, 0x52},
	{0x01C7, 0x01},
	{0x01C8, 0x70},
	{0x01C9, 0x01},
	{0x01CF, 0x88},//R_TG_WOI_HSize=648
	{0x01D0, 0x02},//R_TG_WOI_HSize=648
	{0x01D1, 0x70},//R_TG_WOI_VSize=368
	{0x01D2, 0x01},//R_TG_WOI_VSize=368
	{0x01D3, 0x08}, //d3 = R_TG_WOI_HStart[7:0]=8 for 648x368
	{0x01D5, 0x00}, //d5 = R_TG_WOI_VStart[7:0]=0 for 648x368
	{0x01D7, 0x03},
	{0x01D8, 0x00},
	{0x0064, 0x03},
	{0x0123, 0x02},
	{0x01e7, 0x0C},
	{0x01e8, 0x01},
	{0x2224, 0x04},
	{0x2229, 0x00},
	{0x2226, 0x01},
	{0x222A, 0x01},
	{0x0494, 0x00},
	{0x010B, 0x60},
	{0x011E, 0xF7},//R_LPF=8695 for Pre-roll 5fps (Pxclk=24MHz)
	{0x011F, 0x21},//R_LPF=8695 for Pre-roll 5fps (Pxclk=24MHz)
	{0x00eb, 0x01},
	{0x2340, 0x00},
	{0x2352, 0x01},
	{0x2353, 0x01},
	{0x2354, 0x01},
	{0x2342, 0x01},
	{0x0506, 0x04},//06 <={1'b0,1'b0,1'b0,1'b0,1'b0, R_Data_Format[2:0]}
	{0x00eb, 0x01},//updateflag
	{0x002E, 0x00},
	{0x002F, 0x00},//R_ExpGain_AutoCalc_Stream_En=0, temp setting
	{0x0149, 0xF5},
	{0x014A, 0x21},
	{0x014B, 0x26},
	{0x014C, 0x00},
	{0x014E, 0x3E},//R_ExpLine_Stream=1598 (R_LPF - 2)
	{0x014F, 0x06},//R_ExpLine_Stream=1598 (R_LPF - 2)
	{0x0150, 0x40},//R_GainIndex_Stream=64 (AG=2X)
	{0x0151, 0x00},//R_GainIndex_Stream=64 (AG=2X)
	{0x0156, 0x03},//AE_UpdateFlag
	{0x01ce, 0x00},//R_TG_HFlip=0, R_TG_VFlip=0
	{0x005F, 0x02},//Rs_Np_Sel=0->2 for Stream 10bit 15fps
	{0x00eb, 0x01},
	{0x05B7, 0xB0},//B03A: Version (Dummy register)
	{0x05B8, 0x3A},//B03A: Version (Dummy register)
	{0x2343, 0x02},
	{0x2341, 0x00},
	{0x2341, 0x01},
	// {0x0008, 0x83},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 840Mbps, 2lane
 */
static const struct regval ps5458_linear_10_2560x1472_30fps_regs[] = {
	{0x00FF, 0xA5},
	{0x00A9, 0x00},
	{0x00A8, 0x02},
	{0x238F, 0x01},
	{0x07D8, 0x17},
	{0x07D4, 0x22},
	{0x2228, 0x01},
	{0x0709, 0x6E},
	{0x079A, 0xF5},
	{0x079C, 0x13},
	{0x079D, 0x08},
	{0x0220, 0x2F},
	{0x0222, 0x12},
	{0x0224, 0x1D},
	{0x0226, 0x24},
	{0x0228, 0x2F},
	{0x022A, 0x10},
	{0x022C, 0x1B},
	{0x0230, 0x6F},
	{0x0234, 0x23},
	{0x02CE, 0x40},
	{0x02CF, 0x40},
	{0x02EF, 0x1C},
	{0x02F1, 0x30},
	{0x0736, 0x0A},
	{0x0737, 0x0B},
	{0x073C, 0x0A},
	{0x073D, 0x0B},
	{0x0752, 0x1E},
	{0x075C, 0x1E},
	{0x0762, 0x9B},
	{0x0770, 0xA5},
	{0x0778, 0xA0},
	{0x077E, 0xB1},
	{0x077F, 0x00},
	{0x0784, 0xA7},
	{0x07B5, 0xB9},
	{0x07B6, 0x00},
	{0x07BD, 0xAF},
	{0x0805, 0xEB},
	{0x0806, 0x00},
	{0x080B, 0x31},
	{0x081B, 0x30},
	{0x081C, 0x1F},
	{0x082F, 0x30},
	{0x0830, 0x1F},
	{0x086F, 0xC3},
	{0x0870, 0x00},
	{0x0871, 0xBE},
	{0x0872, 0x00},
	{0x0873, 0x9B},
	{0x0875, 0xAF},
	{0x0876, 0x00},
	{0x0877, 0xB9},
	{0x0879, 0xB4},
	{0x087B, 0xA5},
	{0x0107, 0x1E},
	{0x000C, 0x02},
	{0x2256, 0x70},
	{0x0723, 0x78},
	{0x0724, 0x00},
	{0x0725, 0xF0},
	{0x0726, 0x00},
	{0x0734, 0x04},
	{0x0735, 0x05},
	{0x073A, 0x04},
	{0x073B, 0x05},
	{0x0750, 0x6E},
	{0x075A, 0x6E},
	{0x0760, 0xA0},
	{0x076E, 0xAF},
	{0x0776, 0xAA},
	{0x077C, 0xC3},
	{0x077D, 0x00},
	{0x0782, 0x5F},
	{0x07B3, 0x22},
	{0x07BB, 0xBE},
	{0x07BC, 0x00},
	{0x0803, 0x8B},
	{0x0809, 0x53},
	{0x0861, 0x2C},
	{0x0863, 0x27},
	{0x0865, 0xA0},
	{0x0867, 0xBE},
	{0x0869, 0xC8},
	{0x086A, 0x00},
	{0x086B, 0xC3},
	{0x086C, 0x00},
	{0x086D, 0x5A},
	{0x0819, 0x33},
	{0x081A, 0x01},
	{0x082D, 0x31},
	{0x082E, 0x01},
	{0x0552, 0x01},
	{0x056E, 0xFE},
	{0x0732, 0x09},
	{0x0733, 0x0A},
	{0x0738, 0x09},
	{0x0739, 0x0A},
	{0x074E, 0x6E},
	{0x0758, 0x6E},
	{0x075E, 0xF4},
	{0x076C, 0x30},
	{0x076D, 0x02},
	{0x0774, 0x26},
	{0x0775, 0x02},
	{0x077A, 0x49},
	{0x077B, 0x02},
	{0x0780, 0xAF},
	{0x07B1, 0xB2},
	{0x07B9, 0x18},
	{0x0801, 0x7F},
	{0x0802, 0x03},
	{0x0807, 0x47},
	{0x0811, 0x7A},
	{0x0812, 0x03},
	{0x0817, 0xAA},
	{0x0818, 0x02},
	{0x0825, 0x7A},
	{0x0826, 0x03},
	{0x082B, 0xA8},
	{0x082C, 0x02},
	{0x0853, 0xBC},
	{0x0855, 0xB7},
	{0x0857, 0xF4},
	{0x0859, 0x44},
	{0x085A, 0x02},
	{0x085B, 0x22},
	{0x085D, 0x1D},
	{0x085F, 0xAA},
	{0x007b, 0xC0},
	{0x007c, 0xC0},
	{0x007d, 0xC0},
	{0x007e, 0xC0},
	{0x00af, 0x01},
	{0x0509, 0x28},
	{0x050A, 0x16},
	{0x0510, 0x02},
	{0x0511, 0x00},
	{0x0515, 0x08},
	{0x0517, 0x06},
	{0x0518, 0x06},
	{0x055B, 0x11},
	{0x05B0, 0x05},
	{0x0541, 0x20},
	{0x05EB, 0x80},
	{0x06B6, 0x00},
	{0x0620, 0x00},
	{0x0622, 0x08},
	{0x0617, 0x98},
	{0x0618, 0x08},
	{0x0619, 0xFC},
	{0x061A, 0x08},
	{0x062E, 0x98},
	{0x062F, 0x08},
	{0x0630, 0xFC},
	{0x0631, 0x08},
	{0x01BF, 0x02},
	{0x01C4, 0x03},
	{0x0266, 0x11},
	{0x02D8, 0x08},
	{0x02E7, 0x83},
	{0x02DA, 0x73},
	{0x0600, 0xB3},
	{0x0601, 0x06},
	{0x07D9, 0x20},
	{0x07DA, 0x15},
	{0x0731, 0x02},
	{0x0706, 0x15},
	{0x080D, 0x08},
	{0x0821, 0xFF},
	{0x0822, 0x03},
	{0x0835, 0xFF},
	{0x0836, 0x03},
	{0x2247, 0xB6},
	{0x070A, 0x92},
	{0x0708, 0x30},
	{0x0543, 0x02},
	{0x050F, 0x01},
	{0x01E6, 0x00},
	{0x01C6, 0xA4},
	{0x01C7, 0x02},
	{0x01C8, 0x00},
	{0x01C9, 0x06},
	{0x01CF, 0x00},
	{0x01D0, 0x0A},
	{0x01D1, 0xc0},
	{0x01d2, 0x05},
	{0x01D3, 0x32},
	{0x01D5, 0x20},
	{0x01D7, 0x00},
	{0x01D8, 0x00},
	{0x0064, 0x01},
	{0x0123, 0x00},
	{0x01e7, 0x07},
	{0x01e8, 0x00},
	{0x2224, 0x01},
	{0x2229, 0x02},
	{0x2226, 0x00},
	{0x222A, 0x01},
	{0x010B, 0x64},
	{0x005F, 0x00},
	{0x0506, 0x04},
	{0x011E, 0x40},
	{0x011F, 0x06},
	{0x00EB, 0x01},
	{0x014E, 0x03},
	{0x014F, 0x00},
	{0x0150, 0x40},
	{0x0151, 0x00},
	{0x0156, 0x03},
	{0x01ce, 0x08},
	{0x00eb, 0x01},
	{0x014E, 0x50},
	{0x014F, 0x03},
	{0x0150, 0x99},
	{0x0151, 0x00},
	{0x0156, 0x03},
	{0x00eb, 0x01},
	{0x05B7, 0xD0},
	{0x05B8, 0x2A},
	{REG_NULL, 0x00},
};

static const struct ps5458_mode supported_modes[] = {
	{
		.width = 2560,
		.height = 1472,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0020,
		.hts_def = 0x05dc * 2,
		.vts_def = 0x0640,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = ps5458_linear_10_2560x1472_30fps_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
	{
		.width = 2560,
		.height = 1440,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0020,
		.hts_def = 0x05dc * 2,
		.vts_def = 0x0640,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = ps5458_linear_10_2560x1440_30fps_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
	{
		.width = 2560,
		.height = 1440,
		.max_fps = {
			.numerator = 10000,
			.denominator = 150000,
		},
		.exp_def = 0x0020,
		.hts_def = 0x05dc * 2,
		.vts_def = 0x0640,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = ps5458_linear_10_2560x1440_15fps_regs,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_menu_items[] = {
	PS5458_LINK_FREQ_384
};

static const char * const ps5458_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

/* Write registers up to 4 at a time */
static int ps5458_write_reg(struct i2c_client *client, u16 reg,
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

static int ps5458_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = ps5458_write_reg(client, regs[i].addr,
					PS5458_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int ps5458_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
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

static int ps5458_get_reso_dist(const struct ps5458_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct ps5458_mode *
ps5458_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = ps5458_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int ps5458_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_subdev_format *fmt)
{
	struct ps5458 *ps5458 = to_ps5458(sd);
	const struct ps5458_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&ps5458->mutex);

	mode = ps5458_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&ps5458->mutex);
		return -ENOTTY;
#endif
	} else {
		ps5458->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(ps5458->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ps5458->vblank, vblank_def,
					 PS5458_VTS_MAX - mode->height,
					 1, vblank_def);
		ps5458->cur_fps = mode->max_fps;
	}

	mutex_unlock(&ps5458->mutex);

	return 0;
}

static int ps5458_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state,
			   struct v4l2_subdev_format *fmt)
{
	struct ps5458 *ps5458 = to_ps5458(sd);
	const struct ps5458_mode *mode = ps5458->cur_mode;

	mutex_lock(&ps5458->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, state, fmt->pad);
#else
		mutex_unlock(&ps5458->mutex);
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
	mutex_unlock(&ps5458->mutex);

	return 0;
}

static int ps5458_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct ps5458 *ps5458 = to_ps5458(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = ps5458->cur_mode->bus_fmt;

	return 0;
}

static int ps5458_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *state,
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

static int ps5458_enable_test_pattern(struct ps5458 *ps5458, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = ps5458_read_reg(ps5458->client, PS5458_REG_TEST_PATTERN,
			       PS5458_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= PS5458_TEST_PATTERN_BIT_MASK;
	else
		val &= ~PS5458_TEST_PATTERN_BIT_MASK;

	ret |= ps5458_write_reg(ps5458->client, PS5458_REG_TEST_PATTERN,
				 PS5458_REG_VALUE_08BIT, val);
	return ret;
}

static int ps5458_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct ps5458 *ps5458 = to_ps5458(sd);
	const struct ps5458_mode *mode = ps5458->cur_mode;

	if (ps5458->streaming)
		fi->interval = ps5458->cur_fps;
	else
		fi->interval = mode->max_fps;

	return 0;
}

static int ps5458_g_mbus_config(struct v4l2_subdev *sd,
				unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->bus.mipi_csi2.num_data_lanes = PS5458_LANES;
	config->type = V4L2_MBUS_CSI2_DPHY;

	return 0;
}

static void ps5458_get_module_inf(struct ps5458 *ps5458,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, PS5458_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, ps5458->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, ps5458->len_name, sizeof(inf->base.lens));
}

static long ps5458_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct ps5458 *ps5458 = to_ps5458(sd);
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_hdr_cfg *hdr;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		ps5458_get_module_inf(ps5458, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = ps5458->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		break;
	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);

		if (stream)
			ret = ps5458_write_reg(ps5458->client, PS5458_REG_CTRL_MODE,
				 PS5458_REG_VALUE_08BIT, PS5458_MODE_STREAMING);
		else
			ret = ps5458_write_reg(ps5458->client, PS5458_REG_CTRL_MODE,
				 PS5458_REG_VALUE_08BIT, PS5458_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ps5458_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
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

		ret = ps5458_ioctl(sd, cmd, inf);
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

		ret = ps5458_ioctl(sd, cmd, hdr);
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
			ret = ps5458_ioctl(sd, cmd, hdr);
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
			ret = ps5458_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = ps5458_ioctl(sd, cmd, &stream);
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

static int __ps5458_start_stream(struct ps5458 *ps5458)
{
	int ret;

	if (!ps5458->is_thunderboot) {
		ret = ps5458_write_array(ps5458->client, ps5458->cur_mode->reg_list);
		if (ret)
			return ret;
		/* In case these controls are set before streaming */
		ret = __v4l2_ctrl_handler_setup(&ps5458->ctrl_handler);
		if (ret)
			return ret;
		if (ps5458->has_init_exp && ps5458->cur_mode->hdr_mode != NO_HDR) {
			ret = ps5458_ioctl(&ps5458->subdev, PREISP_CMD_SET_HDRAE_EXP,
				&ps5458->init_hdrae_exp);
			if (ret) {
				dev_err(&ps5458->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}

	ret = ps5458_write_reg(ps5458->client, PS5458_REG_CTRL_MODE,
				PS5458_REG_VALUE_08BIT, PS5458_MODE_STREAMING);
	return ret;
}

static int __ps5458_stop_stream(struct ps5458 *ps5458)
{
	ps5458->has_init_exp = false;
	if (ps5458->is_thunderboot) {
		ps5458->is_first_streamoff = true;
		pm_runtime_put(&ps5458->client->dev);
	}
	return ps5458_write_reg(ps5458->client, PS5458_REG_CTRL_MODE,
				 PS5458_REG_VALUE_08BIT, PS5458_MODE_SW_STANDBY);
}

static int __ps5458_power_on(struct ps5458 *ps5458);
static int ps5458_s_stream(struct v4l2_subdev *sd, int on)
{
	struct ps5458 *ps5458 = to_ps5458(sd);
	struct i2c_client *client = ps5458->client;
	int ret = 0;

	mutex_lock(&ps5458->mutex);
	on = !!on;
	if (on == ps5458->streaming)
		goto unlock_and_return;
	if (on) {
		if (ps5458->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			ps5458->is_thunderboot = false;
			__ps5458_power_on(ps5458);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __ps5458_start_stream(ps5458);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__ps5458_stop_stream(ps5458);
		pm_runtime_put(&client->dev);
	}

	ps5458->streaming = on;
unlock_and_return:
	mutex_unlock(&ps5458->mutex);
	return ret;
}

static int ps5458_s_power(struct v4l2_subdev *sd, int on)
{
	struct ps5458 *ps5458 = to_ps5458(sd);
	struct i2c_client *client = ps5458->client;
	int ret = 0;

	mutex_lock(&ps5458->mutex);

	/* If the power state is not modified - no work to do. */
	if (ps5458->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!ps5458->is_thunderboot) {
			ret = ps5458_write_array(ps5458->client, ps5458_global_regs);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		ps5458->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		ps5458->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&ps5458->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 ps5458_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, PS5458_XVCLK_FREQ / 1000 / 1000);
}

static int __ps5458_power_on(struct ps5458 *ps5458)
{
	int ret;
	struct device *dev = &ps5458->client->dev;

	if (!IS_ERR_OR_NULL(ps5458->pins_default)) {
		ret = pinctrl_select_state(ps5458->pinctrl,
					   ps5458->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(ps5458->xvclk, PS5458_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(ps5458->xvclk) != PS5458_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(ps5458->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (ps5458->is_thunderboot)
		return 0;

	if (!IS_ERR(ps5458->reset_gpio))
		gpiod_set_value_cansleep(ps5458->reset_gpio, 0);

	ret = regulator_bulk_enable(PS5458_NUM_SUPPLIES, ps5458->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	usleep_range(1000, 2000);

	if (!IS_ERR(ps5458->reset_gpio))
		gpiod_set_value_cansleep(ps5458->reset_gpio, 1);

	if (ps5458->client->addr == 0x4c)
		usleep_range(3000, 4000);
	else
		usleep_range(8000, 9000);

	return 0;

disable_clk:
	clk_disable_unprepare(ps5458->xvclk);

	return ret;
}

static void __ps5458_power_off(struct ps5458 *ps5458)
{
	int ret;
	struct device *dev = &ps5458->client->dev;

	clk_disable_unprepare(ps5458->xvclk);
	if (ps5458->is_thunderboot) {
		if (ps5458->is_first_streamoff) {
			ps5458->is_thunderboot = false;
			ps5458->is_first_streamoff = false;
		} else {
			return;
		}
	}

	clk_disable_unprepare(ps5458->xvclk);
	if (!IS_ERR(ps5458->reset_gpio))
		gpiod_set_value_cansleep(ps5458->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(ps5458->pins_sleep)) {
		ret = pinctrl_select_state(ps5458->pinctrl,
					   ps5458->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(PS5458_NUM_SUPPLIES, ps5458->supplies);
}

#define DST_WIDTH 2560
#define DST_HEIGHT 1472

/*
 * The resolution of the driver configuration needs to be exactly
 * the same as the current output resolution of the sensor,
 * the input width of the isp needs to be 16 aligned,
 * the input height of the isp needs to be 8 aligned.
 * Can be cropped to standard resolution by this function,
 * otherwise it will crop out strange resolution according
 * to the alignment rules.
 */
static int ps5458_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_selection *sel)
{
	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = 0;
		sel->r.width = DST_WIDTH;
		sel->r.top = 0;
		sel->r.height = DST_HEIGHT;
		return 0;
	}
	return -EINVAL;
}

static int ps5458_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ps5458 *ps5458 = to_ps5458(sd);

	return __ps5458_power_on(ps5458);
}

static int ps5458_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ps5458 *ps5458 = to_ps5458(sd);

	__ps5458_power_off(ps5458);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int ps5458_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ps5458 *ps5458 = to_ps5458(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct ps5458_mode *def_mode = &supported_modes[0];

	mutex_lock(&ps5458->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&ps5458->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int ps5458_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *state,
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

static const struct dev_pm_ops ps5458_pm_ops = {
	SET_RUNTIME_PM_OPS(ps5458_runtime_suspend, ps5458_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops ps5458_internal_ops = {
	.open = ps5458_open,
};
#endif

static const struct v4l2_subdev_core_ops ps5458_core_ops = {
	.s_power = ps5458_s_power,
	.ioctl = ps5458_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ps5458_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops ps5458_video_ops = {
	.s_stream = ps5458_s_stream,
	.g_frame_interval = ps5458_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops ps5458_pad_ops = {
	.enum_mbus_code = ps5458_enum_mbus_code,
	.enum_frame_size = ps5458_enum_frame_sizes,
	.enum_frame_interval = ps5458_enum_frame_interval,
	.get_fmt = ps5458_get_fmt,
	.set_fmt = ps5458_set_fmt,
	.get_mbus_config = ps5458_g_mbus_config,
	.get_selection = ps5458_get_selection,
};

static const struct v4l2_subdev_ops ps5458_subdev_ops = {
	.core	= &ps5458_core_ops,
	.video	= &ps5458_video_ops,
	.pad	= &ps5458_pad_ops,
};

static void ps5458_modify_fps_info(struct ps5458 *ps5458)
{
	const struct ps5458_mode *mode = ps5458->cur_mode;

	ps5458->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				      ps5458->cur_vts;
}

static int ps5458_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ps5458 *ps5458 = container_of(ctrl->handler,
					       struct ps5458, ctrl_handler);
	struct i2c_client *client = ps5458->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = ps5458->cur_mode->height + ctrl->val - 2;
		__v4l2_ctrl_modify_range(ps5458->exposure,
					 ps5458->exposure->minimum, max,
					 ps5458->exposure->step,
					 ps5458->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		if (ps5458->cur_mode->hdr_mode == NO_HDR) {
			val = ctrl->val;
			/* 4 least significant bits of expsoure are fractional part */
			ret = ps5458_write_reg(ps5458->client,
						PS5458_REG_EXPOSURE_H,
						PS5458_REG_VALUE_08BIT,
						PS5458_FETCH_EXP_H(val));
			ret |= ps5458_write_reg(ps5458->client,
						 PS5458_REG_EXPOSURE_L,
						 PS5458_REG_VALUE_08BIT,
						 PS5458_FETCH_EXP_L(val));
			ret |= ps5458_write_reg(ps5458->client,
						PS5458_REG_EXPOSURE_UPDATE,
						PS5458_REG_VALUE_08BIT,
						PS5458_EXPOSURE_UPDATE);
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set gain 0x%x\n", ctrl->val);
		ret = ps5458_write_reg(ps5458->client,
					PS5458_REG_GAIN_IDX_L,
					PS5458_REG_VALUE_08BIT,
					ctrl->val & 0xff);
		ret |= ps5458_write_reg(ps5458->client,
					PS5458_REG_GAIN_IDX_H,
					PS5458_REG_VALUE_08BIT,
					(ctrl->val >> 8) & 0x03);
		ret |= ps5458_write_reg(ps5458->client,
					PS5458_REG_EXPOSURE_UPDATE,
					PS5458_REG_VALUE_08BIT,
					PS5458_EXPOSURE_UPDATE);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		ret = ps5458_write_reg(ps5458->client,
					PS5458_REG_VTS_H,
					PS5458_REG_VALUE_08BIT,
					(ctrl->val + ps5458->cur_mode->height)
					>> 8);
		ret |= ps5458_write_reg(ps5458->client,
					 PS5458_REG_VTS_L,
					 PS5458_REG_VALUE_08BIT,
					 (ctrl->val + ps5458->cur_mode->height)
					 & 0xff);
		ret |= ps5458_write_reg(ps5458->client, PS5458_GLOBE_UPDATE_REG,
					 PS5458_REG_VALUE_08BIT,
					 0x01);
		ps5458->cur_vts = ctrl->val + ps5458->cur_mode->height;
		ps5458_modify_fps_info(ps5458);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = ps5458_enable_test_pattern(ps5458, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = ps5458_read_reg(ps5458->client, PS5458_FLIP_MIRROR_REG,
				       PS5458_REG_VALUE_08BIT, &val);
		ret |= ps5458_write_reg(ps5458->client, PS5458_FLIP_MIRROR_REG,
					 PS5458_REG_VALUE_08BIT,
					 PS5458_FETCH_MIRROR(val, ctrl->val));
		ret |= ps5458_write_reg(ps5458->client, PS5458_GLOBE_UPDATE_REG,
					 PS5458_REG_VALUE_08BIT,
					 0x01);
		break;
	case V4L2_CID_VFLIP:
		ret = ps5458_read_reg(ps5458->client, PS5458_FLIP_MIRROR_REG,
				       PS5458_REG_VALUE_08BIT, &val);
		ret |= ps5458_write_reg(ps5458->client, PS5458_FLIP_MIRROR_REG,
					 PS5458_REG_VALUE_08BIT,
					 PS5458_FETCH_FLIP(val, ctrl->val));
		ret |= ps5458_write_reg(ps5458->client, PS5458_GLOBE_UPDATE_REG,
					 PS5458_REG_VALUE_08BIT,
					 0x01);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ps5458_ctrl_ops = {
	.s_ctrl = ps5458_set_ctrl,
};

static int ps5458_initialize_controls(struct ps5458 *ps5458)
{
	const struct ps5458_mode *mode;
	struct v4l2_ctrl_handler *handler;
	struct v4l2_ctrl *ctrl;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &ps5458->ctrl_handler;
	mode = ps5458->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &ps5458->mutex;

	ctrl = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
				      0, 0, link_freq_menu_items);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			  0, PIXEL_RATE_WITH_384M_10BIT, 1, PIXEL_RATE_WITH_384M_10BIT);

	h_blank = mode->hts_def - mode->width;
	ps5458->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					    h_blank, h_blank, 1, h_blank);
	if (ps5458->hblank)
		ps5458->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	ps5458->vblank = v4l2_ctrl_new_std(handler, &ps5458_ctrl_ops,
					    V4L2_CID_VBLANK, vblank_def,
					    PS5458_VTS_MAX - mode->height,
					    1, vblank_def);
	exposure_max = mode->vts_def - 2;
	ps5458->exposure = v4l2_ctrl_new_std(handler, &ps5458_ctrl_ops,
					      V4L2_CID_EXPOSURE, PS5458_EXPOSURE_MIN,
					      exposure_max, PS5458_EXPOSURE_STEP,
					      mode->exp_def);
	ps5458->anal_gain = v4l2_ctrl_new_std(handler, &ps5458_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN, PS5458_GAIN_MIN,
					       PS5458_GAIN_MAX, PS5458_GAIN_STEP,
					       PS5458_GAIN_DEFAULT);
	ps5458->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
							    &ps5458_ctrl_ops,
					V4L2_CID_TEST_PATTERN,
					ARRAY_SIZE(ps5458_test_pattern_menu) - 1,
					0, 0, ps5458_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &ps5458_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &ps5458_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&ps5458->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	ps5458->subdev.ctrl_handler = handler;
	ps5458->has_init_exp = false;
	ps5458->cur_fps = mode->max_fps;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int ps5458_check_sensor_id(struct ps5458 *ps5458,
				   struct i2c_client *client)
{
	struct device *dev = &ps5458->client->dev;
	u32 id = 0;
	int ret;

	if (ps5458->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = ps5458_read_reg(client, PS5458_REG_CHIP_ID,
			       PS5458_REG_VALUE_16BIT, &id);
	if (__cpu_to_be16(id) != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%04x), ret(%d)\n", __cpu_to_be16(id), ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected PS%04x sensor\n", CHIP_ID);

	return 0;
}

static int ps5458_configure_regulators(struct ps5458 *ps5458)
{
	unsigned int i;

	for (i = 0; i < PS5458_NUM_SUPPLIES; i++)
		ps5458->supplies[i].supply = ps5458_supply_names[i];

	return devm_regulator_bulk_get(&ps5458->client->dev,
				       PS5458_NUM_SUPPLIES,
				       ps5458->supplies);
}

static int ps5458_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct ps5458 *ps5458;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	int i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	ps5458 = devm_kzalloc(dev, sizeof(*ps5458), GFP_KERNEL);
	if (!ps5458)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &ps5458->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &ps5458->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &ps5458->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &ps5458->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	ps5458->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	ps5458->client = client;
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			ps5458->cur_mode = &supported_modes[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(supported_modes))
		ps5458->cur_mode = &supported_modes[0];

	ps5458->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(ps5458->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	ps5458->reset_gpio = devm_gpiod_get(dev, "reset",
					    ps5458->is_thunderboot ? GPIOD_ASIS : GPIOD_OUT_LOW);
	if (IS_ERR(ps5458->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	ps5458->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(ps5458->pinctrl)) {
		ps5458->pins_default =
			pinctrl_lookup_state(ps5458->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(ps5458->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		ps5458->pins_sleep =
			pinctrl_lookup_state(ps5458->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(ps5458->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = ps5458_configure_regulators(ps5458);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&ps5458->mutex);

	sd = &ps5458->subdev;
	v4l2_i2c_subdev_init(sd, client, &ps5458_subdev_ops);
	ret = ps5458_initialize_controls(ps5458);
	if (ret)
		goto err_destroy_mutex;

	ret = __ps5458_power_on(ps5458);
	if (ret)
		goto err_free_handler;

	ret = ps5458_check_sensor_id(ps5458, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &ps5458_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	ps5458->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &ps5458->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(ps5458->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 ps5458->module_index, facing,
		 PS5458_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	if (ps5458->is_thunderboot)
		pm_runtime_get_sync(dev);
	else
		pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__ps5458_power_off(ps5458);
err_free_handler:
	v4l2_ctrl_handler_free(&ps5458->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&ps5458->mutex);

	return ret;
}

static void ps5458_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ps5458 *ps5458 = to_ps5458(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&ps5458->ctrl_handler);
	mutex_destroy(&ps5458->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__ps5458_power_off(ps5458);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ps5458_of_match[] = {
	{ .compatible = "prime,ps5458" },
	{},
};
MODULE_DEVICE_TABLE(of, ps5458_of_match);
#endif

static const struct i2c_device_id ps5458_match_id[] = {
	{ "prime,ps5458", 0 },
	{ },
};

static struct i2c_driver ps5458_i2c_driver = {
	.driver = {
		.name = PS5458_NAME,
		.pm = &ps5458_pm_ops,
		.of_match_table = of_match_ptr(ps5458_of_match),
	},
	.probe		= &ps5458_probe,
	.remove		= &ps5458_remove,
	.id_table	= ps5458_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&ps5458_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&ps5458_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP) && !defined(CONFIG_INITCALL_ASYNC)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("prime ps5458 sensor driver");
MODULE_LICENSE("GPL");
