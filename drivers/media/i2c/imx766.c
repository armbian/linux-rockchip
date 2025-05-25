// SPDX-License-Identifier: GPL-2.0
/*
 * imx766 driver
 *
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 add poweron function.
 * V0.0X01.0X02 fix mclk issue when probe multiple camera.
 * V0.0X01.0X03 add enum_frame_interval function.
 * V0.0X01.0X04 add quick stream on/off
 * V0.0X01.0X05 add function g_mbus_config
 * V0.0X01.0X06 support capture spd data and embedded data
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
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include "otp_eeprom.h"

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x06)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define IMX766_LANES			4
#define IMX766_BITS_PER_SAMPLE		10
#define IMX766_LINK_FREQ_436MHZ		436000000 // 872/2
/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define IMX766_PIXEL_RATE_FULL_SIZE	799200000
#define IMX766_PIXEL_RATE_BINNING	348800000
#define IMX766_XVCLK_FREQ		24000000

#define CHIP_ID				0x0766
#define IMX766_REG_CHIP_ID		0x0016

#define IMX766_REG_CTRL_MODE		0x0100
#define IMX766_MODE_SW_STANDBY		0x0
#define IMX766_MODE_STREAMING		BIT(0)

#define IMX766_REG_EXPOSURE		0x0202
#define	IMX766_EXPOSURE_MIN		10
#define	IMX766_EXPOSURE_STEP	4
#define IMX766_VTS_MAX			(0xffff-0x48)

#define IMX766_REG_GAIN_H		0x0204
#define IMX766_REG_GAIN_L		0x0205
#define IMX766_GAIN_MIN			0
#define IMX766_GAIN_MAX			0x3F00
#define IMX766_GAIN_STEP		1
#define IMX766_GAIN_DEFAULT		0x0

#define IMX766_REG_TEST_PATTERN		0x0600
#define	IMX766_TEST_PATTERN_ENABLE	0x80
#define	IMX766_TEST_PATTERN_DISABLE	0x0

#define IMX766_REG_VTS			0x0340

#define REG_NULL			0xFFFF

#define IMX766_REG_VALUE_08BIT		1
#define IMX766_REG_VALUE_16BIT		2
#define IMX766_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define IMX766_NAME			"imx766"

static const char * const imx766_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define IMX766_NUM_SUPPLIES ARRAY_SIZE(imx766_supply_names)

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

struct imx766_mode {
	u32 bus_fmt;
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list;
	/* Shield Pix Data */
	const struct other_data *spd;
	/* embedded Data */
	const struct other_data *ebd;
	u32 hdr_mode;
	u32 vc[PAD_MAX];
};

struct imx766 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct gpio_desc	*power_gpio;
	struct regulator_bulk_data supplies[IMX766_NUM_SUPPLIES];

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
	bool			streaming;
	bool			power_on;
	const struct imx766_mode *cur_mode;
	u32			cfg_num;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct otp_info *otp;
	u32 spd_id;
	u32 ebd_id;
};

#define to_imx766(sd) container_of(sd, struct imx766, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval imx766_global_regs[] = {
	{0x0100, 0x00},
	{0x0136, 0x18},
	{0x0137, 0x00},
	{0x33F0, 0x09},
	{0x33F1, 0x05},
	{0x0111, 0x02},
	{0x33D3, 0x01},
	{0x3892, 0x01},
	{0x4C14, 0x00},
	{0x4C15, 0x07},
	{0x4C16, 0x00},
	{0x4C17, 0x1B},
	{0x4C1A, 0x00},
	{0x4C1B, 0x03},
	{0x4C1C, 0x00},
	{0x4C1D, 0x00},
	{0x4C1E, 0x00},
	{0x4C1F, 0x02},
	{0x4C20, 0x00},
	{0x4C21, 0x5F},
	{0x4C26, 0x00},
	{0x4C27, 0x43},
	{0x4C28, 0x00},
	{0x4C29, 0x09},
	{0x4C2A, 0x00},
	{0x4C2B, 0x4A},
	{0x4C2C, 0x00},
	{0x4C2D, 0x00},
	{0x4C2E, 0x00},
	{0x4C2F, 0x02},
	{0x4C30, 0x00},
	{0x4C31, 0xC6},
	{0x4C3E, 0x00},
	{0x4C3F, 0x55},
	{0x4C52, 0x00},
	{0x4C53, 0x97},
	{0x4CB4, 0x00},
	{0x4CB5, 0x55},
	{0x4CC8, 0x00},
	{0x4CC9, 0x97},
	{0x4D04, 0x00},
	{0x4D05, 0x4F},
	{0x4D74, 0x00},
	{0x4D75, 0x55},
	{0x4F06, 0x00},
	{0x4F07, 0x5F},
	{0x4F48, 0x00},
	{0x4F49, 0xC6},
	{0x544A, 0xFF},
	{0x544B, 0xFF},
	{0x544E, 0x01},
	{0x544F, 0xBD},
	{0x5452, 0xFF},
	{0x5453, 0xFF},
	{0x5456, 0x00},
	{0x5457, 0xA5},
	{0x545A, 0xFF},
	{0x545B, 0xFF},
	{0x545E, 0x00},
	{0x545F, 0xA5},
	{0x5496, 0x00},
	{0x5497, 0xA2},
	{0x54F6, 0x01},
	{0x54F7, 0x55},
	{0x54F8, 0x01},
	{0x54F9, 0x61},
	{0x5670, 0x00},
	{0x5671, 0x85},
	{0x5672, 0x01},
	{0x5673, 0x77},
	{0x5674, 0x01},
	{0x5675, 0x2F},
	{0x5676, 0x02},
	{0x5677, 0x55},
	{0x5678, 0x00},
	{0x5679, 0x85},
	{0x567A, 0x01},
	{0x567B, 0x77},
	{0x567C, 0x01},
	{0x567D, 0x2F},
	{0x567E, 0x02},
	{0x567F, 0x55},
	{0x5680, 0x00},
	{0x5681, 0x85},
	{0x5682, 0x01},
	{0x5683, 0x77},
	{0x5684, 0x01},
	{0x5685, 0x2F},
	{0x5686, 0x02},
	{0x5687, 0x55},
	{0x5688, 0x00},
	{0x5689, 0x85},
	{0x568A, 0x01},
	{0x568B, 0x77},
	{0x568C, 0x01},
	{0x568D, 0x2F},
	{0x568E, 0x02},
	{0x568F, 0x55},
	{0x5690, 0x01},
	{0x5691, 0x7A},
	{0x5692, 0x02},
	{0x5693, 0x6C},
	{0x5694, 0x01},
	{0x5695, 0x35},
	{0x5696, 0x02},
	{0x5697, 0x5B},
	{0x5698, 0x01},
	{0x5699, 0x7A},
	{0x569A, 0x02},
	{0x569B, 0x6C},
	{0x569C, 0x01},
	{0x569D, 0x35},
	{0x569E, 0x02},
	{0x569F, 0x5B},
	{0x56A0, 0x01},
	{0x56A1, 0x7A},
	{0x56A2, 0x02},
	{0x56A3, 0x6C},
	{0x56A4, 0x01},
	{0x56A5, 0x35},
	{0x56A6, 0x02},
	{0x56A7, 0x5B},
	{0x56A8, 0x01},
	{0x56A9, 0x80},
	{0x56AA, 0x02},
	{0x56AB, 0x72},
	{0x56AC, 0x01},
	{0x56AD, 0x2F},
	{0x56AE, 0x02},
	{0x56AF, 0x55},
	{0x5902, 0x0E},
	{0x5A50, 0x04},
	{0x5A51, 0x04},
	{0x5A69, 0x01},
	{0x5C49, 0x0D},
	{0x5D60, 0x08},
	{0x5D61, 0x08},
	{0x5D62, 0x08},
	{0x5D63, 0x08},
	{0x5D64, 0x08},
	{0x5D67, 0x08},
	{0x5D6C, 0x08},
	{0x5D6E, 0x08},
	{0x5D71, 0x08},
	{0x5D8E, 0x14},
	{0x5D90, 0x03},
	{0x5D91, 0x0A},
	{0x5D92, 0x1F},
	{0x5D93, 0x05},
	{0x5D97, 0x1F},
	{0x5D9A, 0x06},
	{0x5D9C, 0x1F},
	{0x5DA1, 0x1F},
	{0x5DA6, 0x1F},
	{0x5DA8, 0x1F},
	{0x5DAB, 0x1F},
	{0x5DC0, 0x06},
	{0x5DC1, 0x06},
	{0x5DC2, 0x07},
	{0x5DC3, 0x06},
	{0x5DC4, 0x07},
	{0x5DC7, 0x07},
	{0x5DCC, 0x07},
	{0x5DCE, 0x07},
	{0x5DD1, 0x07},
	{0x5E3E, 0x00},
	{0x5E3F, 0x00},
	{0x5E41, 0x00},
	{0x5E48, 0x00},
	{0x5E49, 0x00},
	{0x5E4A, 0x00},
	{0x5E4C, 0x00},
	{0x5E4D, 0x00},
	{0x5E4E, 0x00},
	{0x6026, 0x03},
	{0x6028, 0x03},
	{0x602A, 0x03},
	{0x602C, 0x03},
	{0x602F, 0x03},
	{0x6036, 0x03},
	{0x6038, 0x03},
	{0x603A, 0x03},
	{0x603C, 0x03},
	{0x603F, 0x03},
	{0x6074, 0x19},
	{0x6076, 0x19},
	{0x6078, 0x19},
	{0x607A, 0x19},
	{0x607D, 0x19},
	{0x6084, 0x32},
	{0x6086, 0x32},
	{0x6088, 0x32},
	{0x608A, 0x32},
	{0x608D, 0x32},
	{0x60C2, 0x4A},
	{0x60C4, 0x4A},
	{0x60CB, 0x4A},
	{0x60D2, 0x4A},
	{0x60D4, 0x4A},
	{0x60DB, 0x4A},
	{0x62F9, 0x14},
	{0x6305, 0x13},
	{0x6307, 0x13},
	{0x630A, 0x13},
	{0x630D, 0x0D},
	{0x6317, 0x0D},
	{0x632F, 0x2E},
	{0x6333, 0x2E},
	{0x6339, 0x2E},
	{0x6343, 0x2E},
	{0x6347, 0x2E},
	{0x634D, 0x2E},
	{0x6352, 0x00},
	{0x6353, 0x5F},
	{0x6366, 0x00},
	{0x6367, 0x5F},
	{0x638F, 0x95},
	{0x6393, 0x95},
	{0x6399, 0x95},
	{0x63A3, 0x95},
	{0x63A7, 0x95},
	{0x63AD, 0x95},
	{0x63B2, 0x00},
	{0x63B3, 0xC6},
	{0x63C6, 0x00},
	{0x63C7, 0xC6},
	{0x8BDB, 0x02},
	{0x8BDE, 0x02},
	{0x8BE1, 0x2D},
	{0x8BE4, 0x00},
	{0x8BE5, 0x00},
	{0x8BE6, 0x01},
	{0x9002, 0x14},
	{0x9200, 0xB5},
	{0x9201, 0x9E},
	{0x9202, 0xB5},
	{0x9203, 0x42},
	{0x9204, 0xB5},
	{0x9205, 0x43},
	{0x9206, 0xBD},
	{0x9207, 0x20},
	{0x9208, 0xBD},
	{0x9209, 0x22},
	{0x920A, 0xBD},
	{0x920B, 0x23},
	{0xB5D7, 0x10},
	{0xBD24, 0x00},
	{0xBD25, 0x00},
	{0xBD26, 0x00},
	{0xBD27, 0x00},
	{0xBD28, 0x00},
	{0xBD29, 0x00},
	{0xBD2A, 0x00},
	{0xBD2B, 0x00},
	{0xBD2C, 0x32},
	{0xBD2D, 0x70},
	{0xBD2E, 0x25},
	{0xBD2F, 0x30},
	{0xBD30, 0x3B},
	{0xBD31, 0xE0},
	{0xBD32, 0x69},
	{0xBD33, 0x40},
	{0xBD34, 0x25},
	{0xBD35, 0x90},
	{0xBD36, 0x58},
	{0xBD37, 0x00},
	{0xBD38, 0x00},
	{0xBD39, 0x00},
	{0xBD3A, 0x00},
	{0xBD3B, 0x00},
	{0xBD3C, 0x32},
	{0xBD3D, 0x70},
	{0xBD3E, 0x25},
	{0xBD3F, 0x90},
	{0xBD40, 0x58},
	{0xBD41, 0x00},
	{0x793B, 0x01},
	{0xACC6, 0x00},
	{0xACF5, 0x00},
	{0x793B, 0x00},
	{0x1F04, 0xB3},
	{0x1F05, 0x01},
	{0x1F06, 0x07},
	{0x1F07, 0x66},
	{0x1F08, 0x01},
	{0x4D18, 0x00},
	{0x4D19, 0x9D},
	{0x4D88, 0x00},
	{0x4D89, 0x97},
	{0x5C57, 0x0A},
	{0x5D94, 0x1F},
	{0x5D9E, 0x1F},
	{0x5E50, 0x23},
	{0x5E51, 0x20},
	{0x5E52, 0x07},
	{0x5E53, 0x20},
	{0x5E54, 0x07},
	{0x5E55, 0x27},
	{0x5E56, 0x0B},
	{0x5E57, 0x24},
	{0x5E58, 0x0B},
	{0x5E60, 0x24},
	{0x5E61, 0x24},
	{0x5E62, 0x1B},
	{0x5E63, 0x23},
	{0x5E64, 0x1B},
	{0x5E65, 0x28},
	{0x5E66, 0x22},
	{0x5E67, 0x28},
	{0x5E68, 0x23},
	{0x5E70, 0x25},
	{0x5E71, 0x24},
	{0x5E72, 0x20},
	{0x5E73, 0x24},
	{0x5E74, 0x20},
	{0x5E75, 0x28},
	{0x5E76, 0x27},
	{0x5E77, 0x29},
	{0x5E78, 0x24},
	{0x5E80, 0x25},
	{0x5E81, 0x25},
	{0x5E82, 0x24},
	{0x5E83, 0x25},
	{0x5E84, 0x23},
	{0x5E85, 0x2A},
	{0x5E86, 0x28},
	{0x5E87, 0x2A},
	{0x5E88, 0x28},
	{0x5E90, 0x24},
	{0x5E91, 0x24},
	{0x5E92, 0x28},
	{0x5E93, 0x29},
	{0x5E97, 0x25},
	{0x5E98, 0x25},
	{0x5E99, 0x2A},
	{0x5E9A, 0x2A},
	{0x5E9E, 0x3A},
	{0x5E9F, 0x3F},
	{0x5EA0, 0x17},
	{0x5EA1, 0x3F},
	{0x5EA2, 0x17},
	{0x5EA3, 0x32},
	{0x5EA4, 0x10},
	{0x5EA5, 0x33},
	{0x5EA6, 0x10},
	{0x5EAE, 0x3D},
	{0x5EAF, 0x48},
	{0x5EB0, 0x3B},
	{0x5EB1, 0x45},
	{0x5EB2, 0x37},
	{0x5EB3, 0x3A},
	{0x5EB4, 0x31},
	{0x5EB5, 0x3A},
	{0x5EB6, 0x31},
	{0x5EBE, 0x40},
	{0x5EBF, 0x48},
	{0x5EC0, 0x3F},
	{0x5EC1, 0x45},
	{0x5EC2, 0x3F},
	{0x5EC3, 0x3A},
	{0x5EC4, 0x32},
	{0x5EC5, 0x3A},
	{0x5EC6, 0x33},
	{0x5ECE, 0x4B},
	{0x5ECF, 0x4A},
	{0x5ED0, 0x48},
	{0x5ED1, 0x4C},
	{0x5ED2, 0x45},
	{0x5ED3, 0x3F},
	{0x5ED4, 0x3A},
	{0x5ED5, 0x3F},
	{0x5ED6, 0x3A},
	{0x5EDE, 0x48},
	{0x5EDF, 0x45},
	{0x5EE0, 0x3A},
	{0x5EE1, 0x3A},
	{0x5EE5, 0x4A},
	{0x5EE6, 0x4C},
	{0x5EE7, 0x3F},
	{0x5EE8, 0x3F},
	{0x5EEC, 0x06},
	{0x5EED, 0x06},
	{0x5EEE, 0x02},
	{0x5EEF, 0x06},
	{0x5EF0, 0x01},
	{0x5EF1, 0x09},
	{0x5EF2, 0x05},
	{0x5EF3, 0x06},
	{0x5EF4, 0x04},
	{0x5EFC, 0x07},
	{0x5EFD, 0x09},
	{0x5EFE, 0x05},
	{0x5EFF, 0x08},
	{0x5F00, 0x04},
	{0x5F01, 0x09},
	{0x5F02, 0x05},
	{0x5F03, 0x09},
	{0x5F04, 0x04},
	{0x5F0C, 0x08},
	{0x5F0D, 0x09},
	{0x5F0E, 0x06},
	{0x5F0F, 0x09},
	{0x5F10, 0x06},
	{0x5F11, 0x09},
	{0x5F12, 0x09},
	{0x5F13, 0x09},
	{0x5F14, 0x06},
	{0x5F1C, 0x09},
	{0x5F1D, 0x09},
	{0x5F1E, 0x09},
	{0x5F1F, 0x09},
	{0x5F20, 0x08},
	{0x5F21, 0x09},
	{0x5F22, 0x09},
	{0x5F23, 0x09},
	{0x5F24, 0x09},
	{0x5F2C, 0x09},
	{0x5F2D, 0x09},
	{0x5F2E, 0x09},
	{0x5F2F, 0x09},
	{0x5F33, 0x09},
	{0x5F34, 0x09},
	{0x5F35, 0x09},
	{0x5F36, 0x09},
	{0x5F3A, 0x01},
	{0x5F3D, 0x07},
	{0x5F3F, 0x01},
	{0x5F4B, 0x01},
	{0x5F4D, 0x04},
	{0x5F4F, 0x02},
	{0x5F51, 0x02},
	{0x5F5A, 0x02},
	{0x5F5B, 0x01},
	{0x5F5D, 0x03},
	{0x5F5E, 0x07},
	{0x5F5F, 0x01},
	{0x5F60, 0x01},
	{0x5F61, 0x01},
	{0x5F6A, 0x01},
	{0x5F6C, 0x01},
	{0x5F6D, 0x01},
	{0x5F6E, 0x04},
	{0x5F70, 0x02},
	{0x5F72, 0x02},
	{0x5F7A, 0x01},
	{0x5F7B, 0x03},
	{0x5F7C, 0x01},
	{0x5F7D, 0x01},
	{0x5F82, 0x01},
	{0x60C6, 0x4A},
	{0x60C8, 0x4A},
	{0x60D6, 0x4A},
	{0x60D8, 0x4A},
	{0x62E4, 0x33},
	{0x62E9, 0x33},
	{0x62EE, 0x1C},
	{0x62EF, 0x33},
	{0x62F3, 0x33},
	{0x62F6, 0x1C},
	{0x33F2, 0x01},
	{0x1F04, 0xA3},
	{0x1F05, 0x01},
	{0x406E, 0x00},
	{0x406F, 0x08},
	{0x4D08, 0x00},
	{0x4D09, 0x2C},
	{0x4D0E, 0x00},
	{0x4D0F, 0x64},
	{0x4D18, 0x00},
	{0x4D19, 0xB1},
	{0x4D1E, 0x00},
	{0x4D1F, 0xCB},
	{0x4D3A, 0x00},
	{0x4D3B, 0x91},
	{0x4D40, 0x00},
	{0x4D41, 0x64},
	{0x4D4C, 0x00},
	{0x4D4D, 0xE8},
	{0x4D52, 0x00},
	{0x4D53, 0xCB},
	{0x4D78, 0x00},
	{0x4D79, 0x2C},
	{0x4D7E, 0x00},
	{0x4D7F, 0x64},
	{0x4D88, 0x00},
	{0x4D89, 0xAB},
	{0x4D8E, 0x00},
	{0x4D8F, 0xCB},
	{0x4DA6, 0x00},
	{0x4DA7, 0xE7},
	{0x4DAC, 0x00},
	{0x4DAD, 0xCB},
	{0x5B98, 0x00},
	{0x5C52, 0x05},
	{0x5C57, 0x09},
	{0x5D94, 0x0A},
	{0x5D9E, 0x0A},
	{0x5E50, 0x22},
	{0x5E51, 0x22},
	{0x5E52, 0x07},
	{0x5E53, 0x20},
	{0x5E54, 0x06},
	{0x5E55, 0x23},
	{0x5E56, 0x0A},
	{0x5E57, 0x23},
	{0x5E58, 0x0A},
	{0x5E60, 0x25},
	{0x5E61, 0x29},
	{0x5E62, 0x1C},
	{0x5E63, 0x26},
	{0x5E64, 0x1C},
	{0x5E65, 0x2D},
	{0x5E66, 0x1E},
	{0x5E67, 0x2A},
	{0x5E68, 0x1E},
	{0x5E70, 0x26},
	{0x5E71, 0x26},
	{0x5E72, 0x22},
	{0x5E73, 0x23},
	{0x5E74, 0x20},
	{0x5E75, 0x28},
	{0x5E76, 0x23},
	{0x5E77, 0x28},
	{0x5E78, 0x23},
	{0x5E80, 0x28},
	{0x5E81, 0x28},
	{0x5E82, 0x29},
	{0x5E83, 0x27},
	{0x5E84, 0x26},
	{0x5E85, 0x2A},
	{0x5E86, 0x2D},
	{0x5E87, 0x2A},
	{0x5E88, 0x2A},
	{0x5E90, 0x26},
	{0x5E91, 0x23},
	{0x5E92, 0x28},
	{0x5E93, 0x28},
	{0x5E97, 0x2F},
	{0x5E98, 0x2E},
	{0x5E99, 0x32},
	{0x5E9A, 0x32},
	{0x5E9E, 0x50},
	{0x5E9F, 0x50},
	{0x5EA0, 0x1E},
	{0x5EA1, 0x50},
	{0x5EA2, 0x1D},
	{0x5EA3, 0x3E},
	{0x5EA4, 0x14},
	{0x5EA5, 0x3E},
	{0x5EA6, 0x14},
	{0x5EAE, 0x58},
	{0x5EAF, 0x5E},
	{0x5EB0, 0x4B},
	{0x5EB1, 0x5A},
	{0x5EB2, 0x4B},
	{0x5EB3, 0x4C},
	{0x5EB4, 0x3A},
	{0x5EB5, 0x4C},
	{0x5EB6, 0x38},
	{0x5EBE, 0x56},
	{0x5EBF, 0x57},
	{0x5EC0, 0x50},
	{0x5EC1, 0x55},
	{0x5EC2, 0x50},
	{0x5EC3, 0x46},
	{0x5EC4, 0x3E},
	{0x5EC5, 0x46},
	{0x5EC6, 0x3E},
	{0x5ECE, 0x5A},
	{0x5ECF, 0x5F},
	{0x5ED0, 0x5E},
	{0x5ED1, 0x5A},
	{0x5ED2, 0x5A},
	{0x5ED3, 0x50},
	{0x5ED4, 0x4C},
	{0x5ED5, 0x50},
	{0x5ED6, 0x4C},
	{0x5EDE, 0x57},
	{0x5EDF, 0x55},
	{0x5EE0, 0x46},
	{0x5EE1, 0x46},
	{0x5EE5, 0x73},
	{0x5EE6, 0x6E},
	{0x5EE7, 0x5F},
	{0x5EE8, 0x5A},
	{0x5EEC, 0x0A},
	{0x5EED, 0x0A},
	{0x5EEE, 0x0F},
	{0x5EEF, 0x0A},
	{0x5EF0, 0x0E},
	{0x5EF1, 0x08},
	{0x5EF2, 0x0C},
	{0x5EF3, 0x0C},
	{0x5EF4, 0x0F},
	{0x5EFC, 0x0A},
	{0x5EFD, 0x0A},
	{0x5EFE, 0x14},
	{0x5EFF, 0x0A},
	{0x5F00, 0x14},
	{0x5F01, 0x0A},
	{0x5F02, 0x14},
	{0x5F03, 0x0A},
	{0x5F04, 0x19},
	{0x5F0C, 0x0A},
	{0x5F0D, 0x0A},
	{0x5F0E, 0x0A},
	{0x5F0F, 0x05},
	{0x5F10, 0x0A},
	{0x5F11, 0x06},
	{0x5F12, 0x08},
	{0x5F13, 0x0A},
	{0x5F14, 0x0C},
	{0x5F1C, 0x0A},
	{0x5F1D, 0x0A},
	{0x5F1E, 0x0A},
	{0x5F1F, 0x0A},
	{0x5F20, 0x0A},
	{0x5F21, 0x0A},
	{0x5F22, 0x0A},
	{0x5F23, 0x0A},
	{0x5F24, 0x0A},
	{0x5F2C, 0x0A},
	{0x5F2D, 0x05},
	{0x5F2E, 0x06},
	{0x5F2F, 0x0A},
	{0x5F33, 0x0A},
	{0x5F34, 0x0A},
	{0x5F35, 0x0A},
	{0x5F36, 0x0A},
	{0x5F3A, 0x00},
	{0x5F3D, 0x02},
	{0x5F3F, 0x0A},
	{0x5F4A, 0x0A},
	{0x5F4B, 0x0A},
	{0x5F4D, 0x0F},
	{0x5F4F, 0x00},
	{0x5F51, 0x00},
	{0x5F5A, 0x00},
	{0x5F5B, 0x00},
	{0x5F5D, 0x0A},
	{0x5F5E, 0x02},
	{0x5F5F, 0x0A},
	{0x5F60, 0x0A},
	{0x5F61, 0x00},
	{0x5F6A, 0x00},
	{0x5F6C, 0x0A},
	{0x5F6D, 0x06},
	{0x5F6E, 0x0F},
	{0x5F70, 0x00},
	{0x5F72, 0x00},
	{0x5F7A, 0x00},
	{0x5F7B, 0x0A},
	{0x5F7C, 0x0A},
	{0x5F7D, 0x00},
	{0x5F82, 0x06},
	{0x60C6, 0x36},
	{0x60C8, 0x36},
	{0x60D6, 0x36},
	{0x60D8, 0x36},
	{0x62DF, 0x56},
	{0x62E0, 0x52},
	{0x62E4, 0x38},
	{0x62E5, 0x51},
	{0x62E9, 0x35},
	{0x62EA, 0x54},
	{0x62EE, 0x1D},
	{0x62EF, 0x38},
	{0x62F3, 0x33},
	{0x62F6, 0x26},
	{0x6412, 0x1E},
	{0x6413, 0x1E},
	{0x6414, 0x1E},
	{0x6415, 0x1E},
	{0x6416, 0x1E},
	{0x6417, 0x1E},
	{0x6418, 0x1E},
	{0x641A, 0x1E},
	{0x641B, 0x1E},
	{0x641C, 0x1E},
	{0x641D, 0x1E},
	{0x641E, 0x1E},
	{0x641F, 0x1E},
	{0x6420, 0x1E},
	{0x6421, 0x1E},
	{0x6422, 0x1E},
	{0x6424, 0x1E},
	{0x6425, 0x1E},
	{0x6426, 0x1E},
	{0x6427, 0x1E},
	{0x6428, 0x1E},
	{0x6429, 0x1E},
	{0x642A, 0x1E},
	{0x642B, 0x1E},
	{0x642C, 0x1E},
	{0x642E, 0x1E},
	{0x642F, 0x1E},
	{0x6430, 0x1E},
	{0x6431, 0x1E},
	{0x6432, 0x1E},
	{0x6433, 0x1E},
	{0x6434, 0x1E},
	{0x6435, 0x1E},
	{0x6436, 0x1E},
	{0x6438, 0x1E},
	{0x6439, 0x1E},
	{0x643A, 0x1E},
	{0x643B, 0x1E},
	{0x643D, 0x1E},
	{0x643E, 0x1E},
	{0x643F, 0x1E},
	{0x6441, 0x1E},
	{0x33F2, 0x02},
	{0x1F08, 0x00},
	{0xA307, 0x30},
	{0xA309, 0x30},
	{0xA30B, 0x30},
	{0xA406, 0x03},
	{0xA407, 0x48},
	{0xA408, 0x03},
	{0xA409, 0x48},
	{0xA40A, 0x03},
	{0xA40B, 0x48},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 7fps
 * mipi_datarate per lane 600Mbps
 */
static const struct regval imx766_4096x3072_regs[] = {
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0114, 0x03},
	{0x0342, 0xB7},
	{0x0343, 0x00},
	{0x0340, 0x0C},
	{0x0341, 0x5C},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x1F},
	{0x0349, 0xFF},
	{0x034A, 0x17},
	{0x034B, 0xFF},
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x0902, 0x08},
	{0x3005, 0x02},
	{0x3120, 0x04},
	{0x3121, 0x01},
	{0x3200, 0x41},
	{0x3201, 0x41},
	{0x32D6, 0x00},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x10},
	{0x040D, 0x00},
	{0x040E, 0x0C},
	{0x040F, 0x00},
	{0x034C, 0x10},
	{0x034D, 0x00},
	{0x034E, 0x0C},
	{0x034F, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x01},
	{0x0307, 0x35},
	{0x030B, 0x04},
	{0x030D, 0x03},
	{0x030E, 0x01},
	{0x030F, 0xB4},
	{0x30CB, 0x00},
	{0x30CC, 0x10},
	{0x30CD, 0x00},
	{0x30CE, 0x03},
	{0x30CF, 0x00},
	{0x319C, 0x01},
	{0x3800, 0x01},
	{0x3801, 0x01},
	{0x3802, 0x02},
	{0x3847, 0x03},
	{0x38B0, 0x00},
	{0x38B1, 0x64},
	{0x38B2, 0x00},
	{0x38B3, 0x64},
	{0x38C4, 0x00},
	{0x38C5, 0x64},
	{0x4C3A, 0x02},
	{0x4C3B, 0xD2},
	{0x4C68, 0x04},
	{0x4C69, 0x7E},
	{0x4CF8, 0x16},
	{0x4CF9, 0xE0},
	{0x4DB8, 0x08},
	{0x4DB9, 0x98},
	{0x0202, 0x0C},
	{0x0203, 0x2C},
	{0x0224, 0x01},
	{0x0225, 0xF4},
	{0x313A, 0x01},
	{0x313B, 0xF4},
	{0x3803, 0x00},
	{0x3804, 0x17},
	{0x3805, 0xC0},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0216, 0x00},
	{0x0217, 0x00},
	{0x0218, 0x01},
	{0x0219, 0x00},
	{0x313C, 0x00},
	{0x313D, 0x00},
	{0x313E, 0x01},
	{0x313F, 0x00},
	{0x30B4, 0x01},
	{0x3066, 0x01},
	{0x3067, 0x30},
	{0x3068, 0x01},
	{0x3069, 0x30},
	{0x33D0, 0x00},
	{0x33D1, 0x00},
	{0x33D4, 0x01},
	{0x33DC, 0x0A},
	{0x33DD, 0x0A},
	{0x33DE, 0x0A},
	{0x33DF, 0x0A},
	{0x3070, 0x01},
	{0x3077, 0x01},
	{0x3078, 0x30},
	{0x3079, 0x01},
	{0x307A, 0x30},
	{0x307B, 0x01},
	{0x3080, 0x02},
	{0x3087, 0x02},
	{0x3088, 0x30},
	{0x3089, 0x02},
	{0x308A, 0x30},
	{0x308B, 0x02},
	{0x3901, 0x2B},
	{0x3902, 0x00},
	{0x3903, 0x12},
	{0x3905, 0x2B},
	{0x3906, 0x01},
	{0x3907, 0x12},
	{0x3909, 0x2B},
	{0x390A, 0x02},
	{0x390B, 0x12},
	{0x3911, 0x00},
	{REG_NULL, 0x00},
};

static const struct other_data imx766_full_spd = {
	.width = 4096,
	.height = 768,
	.bus_fmt = MEDIA_BUS_FMT_SPD_2X8,
	.data_type = 0x30,
	.data_bit = 10,
};

static const struct other_data imx766_full_ebd = {
	.width = 320,
	.height = 2,
	.bus_fmt = MEDIA_BUS_FMT_EBD_1X8,
};

static const struct imx766_mode supported_modes[] = {
	{
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.width = 4096,
		.height = 3072,
		.max_fps = {
			.numerator = 10000,
			.denominator = 200000,
		},
		.exp_def = 0x0C2C,
		.hts_def = 0xB700,
		.vts_def = 0x0C5C,
		.reg_list = imx766_4096x3072_regs,
		.spd = &imx766_full_spd,
		.ebd = &imx766_full_ebd,
		.hdr_mode = NO_HDR,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_menu_items[] = {
	IMX766_LINK_FREQ_436MHZ
};

static const char * const imx766_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4"
};

#define WRITE_COUNT 5
/* Write registers up to 4 at a time */
static int imx766_write_reg(struct i2c_client *client, u16 reg,
	int len, u32 val)
{
	u32 buf_i, val_i, i;
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
	i = 0;
	while (i < WRITE_COUNT) {
		if (i2c_master_send(client, buf, len + 2) == len + 2)
			break;
		i++;
	}
	if (i >= WRITE_COUNT)
		return -EIO;

	return 0;
}

static int imx766_write_array(struct i2c_client *client,
	const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = imx766_write_reg(client, regs[i].addr,
			IMX766_REG_VALUE_08BIT,
			regs[i].val);
	return ret;
}

/* Read registers up to 4 at a time */
static int imx766_read_reg(struct i2c_client *client, u16 reg,
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

static int imx766_get_reso_dist(const struct imx766_mode *mode,
	struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct imx766_mode *
	imx766_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = imx766_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int imx766_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx766 *imx766 = to_imx766(sd);
	const struct imx766_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&imx766->mutex);

	mode = imx766_find_best_fit(fmt);
	fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&imx766->mutex);
		return -ENOTTY;
#endif
	} else {
		imx766->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(imx766->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(imx766->vblank, vblank_def,
					 IMX766_VTS_MAX - mode->height,
					 1, vblank_def);
		if (mode->width == 4096 && mode->height == 3072) {
			__v4l2_ctrl_s_ctrl(imx766->link_freq,
				link_freq_menu_items[0]);
			__v4l2_ctrl_s_ctrl_int64(imx766->pixel_rate,
				IMX766_PIXEL_RATE_BINNING);
		} else {
			__v4l2_ctrl_s_ctrl(imx766->link_freq,
				link_freq_menu_items[0]);
			__v4l2_ctrl_s_ctrl_int64(imx766->pixel_rate,
				IMX766_PIXEL_RATE_FULL_SIZE);
		}
	}
	mutex_unlock(&imx766->mutex);

	return 0;
}

static int imx766_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct imx766 *imx766 = to_imx766(sd);
	const struct imx766_mode *mode = imx766->cur_mode;

	mutex_lock(&imx766->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&imx766->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
		fmt->format.field = V4L2_FIELD_NONE;
		/* to csi rawwr3, other rawwr also can use */
		if (fmt->pad == imx766->spd_id && mode->spd) {
			fmt->format.width = mode->spd->width;
			fmt->format.height = mode->spd->height;
			fmt->format.code = mode->spd->bus_fmt;
			//Set the vc channel to be consistent with the valid data
			fmt->reserved[0] = 0;
		} else if (fmt->pad == imx766->ebd_id && mode->ebd) {
			fmt->format.width = mode->ebd->width;
			fmt->format.height = mode->ebd->height;
			fmt->format.code = mode->ebd->bus_fmt;
			//Set the vc channel to be consistent with the valid data
			fmt->reserved[0] = 0;
		}
	}
	mutex_unlock(&imx766->mutex);

	return 0;
}

static int imx766_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;

	return 0;
}

static int imx766_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SRGGB10_1X10)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int imx766_enable_test_pattern(struct imx766 *imx766, u32 pattern)
{
	u32 val;

	if (pattern)
		val = (pattern - 1) | IMX766_TEST_PATTERN_ENABLE;
	else
		val = IMX766_TEST_PATTERN_DISABLE;

	return imx766_write_reg(imx766->client,
			IMX766_REG_TEST_PATTERN,
			IMX766_REG_VALUE_08BIT,
			val);
}

static int imx766_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *fi)
{
	struct imx766 *imx766 = to_imx766(sd);
	const struct imx766_mode *mode = imx766->cur_mode;

	mutex_lock(&imx766->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&imx766->mutex);

	return 0;
}

static void imx766_get_otp(struct otp_info *otp,
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

static void imx766_get_module_inf(struct imx766 *imx766,
				  struct rkmodule_inf *inf)
{
	struct otp_info *otp = imx766->otp;

	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, IMX766_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, imx766->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, imx766->len_name, sizeof(inf->base.lens));
	if (otp)
		imx766_get_otp(otp, inf);
}

static int imx766_get_channel_info(struct imx766 *imx766, struct rkmodule_channel_info *ch_info)
{
	const struct imx766_mode *mode = imx766->cur_mode;

	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;

	if (ch_info->index == imx766->spd_id && mode->spd) {
		ch_info->vc = 1;
		ch_info->width = mode->spd->width;
		ch_info->height = mode->spd->height;
		ch_info->bus_fmt = mode->spd->bus_fmt;
		ch_info->data_type = mode->spd->data_type;
		ch_info->data_bit = mode->spd->data_bit;
	} else {
		ch_info->vc = imx766->cur_mode->vc[ch_info->index];
		ch_info->width = imx766->cur_mode->width;
		ch_info->height = imx766->cur_mode->height;
		ch_info->bus_fmt = imx766->cur_mode->bus_fmt;
	}
	return 0;
}

static long imx766_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx766 *imx766 = to_imx766(sd);
	struct rkmodule_hdr_cfg *hdr_cfg;
	struct rkmodule_channel_info *ch_info;
	long ret = 0;
	u32 stream = 0;
	u32 i, h, w;

	switch (cmd) {
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = imx766_write_reg(imx766->client,
					       IMX766_REG_CTRL_MODE,
					       IMX766_REG_VALUE_08BIT,
					       IMX766_MODE_STREAMING);
		else
			ret = imx766_write_reg(imx766->client,
					       IMX766_REG_CTRL_MODE,
					       IMX766_REG_VALUE_08BIT,
					       IMX766_MODE_SW_STANDBY);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		w = imx766->cur_mode->width;
		h = imx766->cur_mode->height;
		for (i = 0; i < imx766->cfg_num; i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr_cfg->hdr_mode) {
				imx766->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == imx766->cfg_num) {
			dev_err(&imx766->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr_cfg->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = imx766->cur_mode->hts_def - imx766->cur_mode->width;
			h = imx766->cur_mode->vts_def - imx766->cur_mode->height;
			__v4l2_ctrl_modify_range(imx766->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(imx766->vblank, h,
						 IMX766_VTS_MAX - imx766->cur_mode->height,
						 1, h);
			dev_info(&imx766->client->dev,
				"sensor mode: %d\n",
				imx766->cur_mode->hdr_mode);
		}
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr_cfg = (struct rkmodule_hdr_cfg *)arg;
		hdr_cfg->esp.mode = HDR_NORMAL_VC;
		hdr_cfg->hdr_mode = imx766->cur_mode->hdr_mode;
		break;
	case RKMODULE_GET_MODULE_INFO:
		imx766_get_module_inf(imx766, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = imx766_get_channel_info(imx766, ch_info);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long imx766_compat_ioctl32(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_hdr_cfg *hdr;
	struct rkmodule_inf *inf;
	struct rkmodule_channel_info *ch_info;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx766_ioctl(sd, cmd, hdr);
		if (!ret) {
			ret = copy_to_user(up, hdr, sizeof(*hdr));
			if (ret) {
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

		ret = copy_from_user(hdr, up, sizeof(*hdr));
		if (ret) {
			kfree(hdr);
			return -EFAULT;
		}
		ret = imx766_ioctl(sd, cmd, hdr);
		kfree(hdr);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (ret)
			return -EFAULT;
		ret = imx766_ioctl(sd, cmd, &stream);
		break;
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = imx766_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}
		ret = copy_from_user(ch_info, up, sizeof(*ch_info));
		if (!ret) {
			ret = imx766_ioctl(sd, cmd, ch_info);
			if (!ret) {
				ret = copy_to_user(up, ch_info, sizeof(*ch_info));
				if (ret)
					ret = -EFAULT;
			}
		} else {
			ret = -EFAULT;
		}
		kfree(ch_info);
		break;
	default:
		ret = -ENOTTY;
		break;
	}
	return ret;
}
#endif

/*--------------------------------------------------------------------------*/

#define IMX766_QSC_CONFIG_ADDR			0xC800
#define IMX766_RD_QSC_KNOT_VALUE_OFFSET		0x86A9
#define IMX766_QSC_EN				0x32D2
static void imx766_config_qsc(struct imx766 *imx766)
{
	struct otp_info *otp = imx766->otp;
	u8 *qsc_calib;
	u32 i;

	if (otp && otp->qsc_data.flag) {
		qsc_calib = &otp->qsc_data.qsc_calib[0];
		for (i = 0; i < 3072; i++) {
			imx766_write_reg(imx766->client, IMX766_QSC_CONFIG_ADDR + i,
				IMX766_REG_VALUE_08BIT,
				qsc_calib[i]);
			dev_dbg(&imx766->client->dev,
				 "set qscdata: qsc_calib[%d]: 0x%x\n",
				 i, qsc_calib[i]);
		}
		imx766_write_reg(imx766->client, IMX766_RD_QSC_KNOT_VALUE_OFFSET,
			IMX766_REG_VALUE_08BIT,
			0x4E);
		imx766_write_reg(imx766->client, IMX766_QSC_EN,
			IMX766_REG_VALUE_08BIT,
			0x01);
	}
}

static int __imx766_start_stream(struct imx766 *imx766)
{
	int ret;

	ret = imx766_write_array(imx766->client, imx766->cur_mode->reg_list);
	if (ret)
		return ret;

	imx766_config_qsc(imx766);

	/* In case these controls are set before streaming */
	mutex_unlock(&imx766->mutex);
	ret = v4l2_ctrl_handler_setup(&imx766->ctrl_handler);
	mutex_lock(&imx766->mutex);
	if (ret)
		return ret;

	return imx766_write_reg(imx766->client,
		IMX766_REG_CTRL_MODE,
		IMX766_REG_VALUE_08BIT,
		IMX766_MODE_STREAMING);
}

static int __imx766_stop_stream(struct imx766 *imx766)
{
	return imx766_write_reg(imx766->client,
		IMX766_REG_CTRL_MODE,
		IMX766_REG_VALUE_08BIT,
		IMX766_MODE_SW_STANDBY);
}

static int imx766_s_stream(struct v4l2_subdev *sd, int on)
{
	struct imx766 *imx766 = to_imx766(sd);
	struct i2c_client *client = imx766->client;
	int ret = 0;

	mutex_lock(&imx766->mutex);
	on = !!on;
	if (on == imx766->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __imx766_start_stream(imx766);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__imx766_stop_stream(imx766);
		pm_runtime_put(&client->dev);
	}

	imx766->streaming = on;

unlock_and_return:
	mutex_unlock(&imx766->mutex);

	return ret;
}

static int imx766_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx766 *imx766 = to_imx766(sd);
	struct i2c_client *client = imx766->client;
	int ret = 0;

	mutex_lock(&imx766->mutex);

	/* If the power state is not modified - no work to do. */
	if (imx766->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = imx766_write_array(imx766->client, imx766_global_regs);
		if (ret) {
			v4l2_err(sd, "could not set init registers\n");
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		imx766->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		imx766->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&imx766->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 imx766_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, IMX766_XVCLK_FREQ / 1000 / 1000);
}

static int __imx766_power_on(struct imx766 *imx766)
{
	int ret;
	u32 delay_us;
	struct device *dev = &imx766->client->dev;

	if (!IS_ERR(imx766->power_gpio))
		gpiod_set_value_cansleep(imx766->power_gpio, 1);
	usleep_range(10000, 12000);

	if (!IS_ERR_OR_NULL(imx766->pins_default)) {
		ret = pinctrl_select_state(imx766->pinctrl,
			imx766->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(imx766->xvclk, IMX766_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(imx766->xvclk) != IMX766_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(imx766->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	if (!IS_ERR(imx766->reset_gpio))
		gpiod_set_value_cansleep(imx766->reset_gpio, 0);
	usleep_range(10000, 12000);

	ret = regulator_bulk_enable(IMX766_NUM_SUPPLIES, imx766->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(imx766->reset_gpio))
		gpiod_set_value_cansleep(imx766->reset_gpio, 1);

	usleep_range(10000, 12000);
	if (!IS_ERR(imx766->pwdn_gpio))
		gpiod_set_value_cansleep(imx766->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = imx766_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(imx766->xvclk);

	return ret;
}

static void __imx766_power_off(struct imx766 *imx766)
{
	int ret;

	if (!IS_ERR(imx766->power_gpio))
		gpiod_set_value_cansleep(imx766->power_gpio, 0);


	if (!IS_ERR(imx766->pwdn_gpio))
		gpiod_set_value_cansleep(imx766->pwdn_gpio, 0);
	clk_disable_unprepare(imx766->xvclk);
	if (!IS_ERR(imx766->reset_gpio))
		gpiod_set_value_cansleep(imx766->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(imx766->pins_sleep)) {
		ret = pinctrl_select_state(imx766->pinctrl,
			imx766->pins_sleep);
		if (ret < 0)
			dev_dbg(&imx766->client->dev, "could not set pins\n");
	}
	regulator_bulk_disable(IMX766_NUM_SUPPLIES, imx766->supplies);
}

static int imx766_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx766 *imx766 = to_imx766(sd);

	return __imx766_power_on(imx766);
}

static int imx766_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx766 *imx766 = to_imx766(sd);

	__imx766_power_off(imx766);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int imx766_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct imx766 *imx766 = to_imx766(sd);
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct imx766_mode *def_mode = &supported_modes[0];

	mutex_lock(&imx766->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&imx766->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int imx766_enum_frame_interval(struct v4l2_subdev *sd,
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

static int imx766_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = IMX766_LANES;

	return 0;
}

static const struct dev_pm_ops imx766_pm_ops = {
	SET_RUNTIME_PM_OPS(imx766_runtime_suspend,
		imx766_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops imx766_internal_ops = {
	.open = imx766_open,
};
#endif

static const struct v4l2_subdev_core_ops imx766_core_ops = {
	.s_power = imx766_s_power,
	.ioctl = imx766_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = imx766_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops imx766_video_ops = {
	.s_stream = imx766_s_stream,
	.g_frame_interval = imx766_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx766_pad_ops = {
	.enum_mbus_code = imx766_enum_mbus_code,
	.enum_frame_size = imx766_enum_frame_sizes,
	.enum_frame_interval = imx766_enum_frame_interval,
	.get_fmt = imx766_get_fmt,
	.set_fmt = imx766_set_fmt,
	.get_mbus_config = imx766_g_mbus_config,
};

static const struct v4l2_subdev_ops imx766_subdev_ops = {
	.core	= &imx766_core_ops,
	.video	= &imx766_video_ops,
	.pad	= &imx766_pad_ops,
};

static int imx766_set_gain_reg(struct imx766 *imx766, u32 a_gain)
{
	int ret = 0;
	u32 gain_reg = 0;

	gain_reg = (16384 - (16384*1024 / a_gain));

	if (gain_reg > 16128) //960
		gain_reg = 16128;

	ret = imx766_write_reg(imx766->client,
		IMX766_REG_GAIN_H,
		IMX766_REG_VALUE_08BIT,
		((gain_reg & 0x3f00) >> 8));
	ret |= imx766_write_reg(imx766->client,
		IMX766_REG_GAIN_L,
		IMX766_REG_VALUE_08BIT,
		(gain_reg & 0xff));
	return ret;
}

static int imx766_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx766 *imx766 = container_of(ctrl->handler,
					     struct imx766, ctrl_handler);
	struct i2c_client *client = imx766->client;
	s64 max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = imx766->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(imx766->exposure,
			imx766->exposure->minimum, max,
			imx766->exposure->step,
			imx766->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = imx766_write_reg(imx766->client,
			IMX766_REG_EXPOSURE,
			IMX766_REG_VALUE_16BIT,
			ctrl->val);

		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = imx766_set_gain_reg(imx766, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = imx766_write_reg(imx766->client,
			IMX766_REG_VTS,
			IMX766_REG_VALUE_16BIT,
			ctrl->val + imx766->cur_mode->height);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = imx766_enable_test_pattern(imx766, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx766_ctrl_ops = {
	.s_ctrl = imx766_set_ctrl,
};

static int imx766_initialize_controls(struct imx766 *imx766)
{
	const struct imx766_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &imx766->ctrl_handler;
	mode = imx766->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &imx766->mutex;

	imx766->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
		V4L2_CID_LINK_FREQ, 1, 0,
		link_freq_menu_items);

	imx766->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
		V4L2_CID_PIXEL_RATE, 0, IMX766_PIXEL_RATE_BINNING,
		1, IMX766_PIXEL_RATE_BINNING);

	h_blank = mode->hts_def - mode->width;
	imx766->hblank = v4l2_ctrl_new_std(handler, NULL,
		V4L2_CID_HBLANK, h_blank, h_blank, 1, h_blank);
	if (imx766->hblank)
		imx766->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	imx766->vblank = v4l2_ctrl_new_std(handler, &imx766_ctrl_ops,
		V4L2_CID_VBLANK, vblank_def,
		IMX766_VTS_MAX - mode->height,
		1, vblank_def);

	exposure_max = mode->vts_def - 4;
	imx766->exposure = v4l2_ctrl_new_std(handler, &imx766_ctrl_ops,
		V4L2_CID_EXPOSURE, IMX766_EXPOSURE_MIN,
		exposure_max, IMX766_EXPOSURE_STEP,
		mode->exp_def);

	imx766->anal_gain = v4l2_ctrl_new_std(handler, &imx766_ctrl_ops,
		V4L2_CID_ANALOGUE_GAIN, IMX766_GAIN_MIN,
		IMX766_GAIN_MAX, IMX766_GAIN_STEP,
		IMX766_GAIN_DEFAULT);

	imx766->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
		&imx766_ctrl_ops, V4L2_CID_TEST_PATTERN,
		ARRAY_SIZE(imx766_test_pattern_menu) - 1,
		0, 0, imx766_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_err(&imx766->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	imx766->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int imx766_check_sensor_id(struct imx766 *imx766,
				  struct i2c_client *client)
{
	struct device *dev = &imx766->client->dev;
	int ret = 0;
	u32 id = 0;
	int i;

	for (i = 0; i < 5; i++) {
		ret = imx766_read_reg(client, IMX766_REG_CHIP_ID,
				      IMX766_REG_VALUE_16BIT, &id);
		if (id == CHIP_ID)
			break;
		usleep_range(300, 1500);
	}
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	return 0;
}

static int imx766_configure_regulators(struct imx766 *imx766)
{
	unsigned int i;

	for (i = 0; i < IMX766_NUM_SUPPLIES; i++)
		imx766->supplies[i].supply = imx766_supply_names[i];

	return devm_regulator_bulk_get(&imx766->client->dev,
		IMX766_NUM_SUPPLIES,
		imx766->supplies);
}

static int imx766_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct imx766 *imx766;
	struct v4l2_subdev *sd;
	char facing[2];
	struct device_node *eeprom_ctrl_node;
	struct i2c_client *eeprom_ctrl_client;
	struct v4l2_subdev *eeprom_ctrl;
	struct otp_info *otp_ptr;
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	imx766 = devm_kzalloc(dev, sizeof(*imx766), GFP_KERNEL);
	if (!imx766)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
		&imx766->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
		&imx766->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
		&imx766->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
		&imx766->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	imx766->client = client;
	imx766->cfg_num = ARRAY_SIZE(supported_modes);
	imx766->cur_mode = &supported_modes[0];

	imx766->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(imx766->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	imx766->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx766->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	imx766->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(imx766->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	imx766->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(imx766->power_gpio))
		dev_warn(dev, "Failed to get power-gpios\n");


	ret = of_property_read_u32(node,
				   "rockchip,spd-id",
				   &imx766->spd_id);
	if (ret != 0) {
		imx766->spd_id = PAD_MAX;
		dev_err(dev,
			"failed get spd_id, will not to use spd\n");
	}
	ret = of_property_read_u32(node,
				   "rockchip,ebd-id",
				   &imx766->ebd_id);
	if (ret != 0) {
		imx766->ebd_id = PAD_MAX;
		dev_err(dev,
			"failed get ebd_id, will not to use ebd\n");
	}

	ret = imx766_configure_regulators(imx766);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	imx766->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(imx766->pinctrl)) {
		imx766->pins_default =
			pinctrl_lookup_state(imx766->pinctrl,
				OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(imx766->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		imx766->pins_sleep =
			pinctrl_lookup_state(imx766->pinctrl,
				OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(imx766->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&imx766->mutex);

	sd = &imx766->subdev;
	v4l2_i2c_subdev_init(sd, client, &imx766_subdev_ops);
	ret = imx766_initialize_controls(imx766);
	if (ret)
		goto err_destroy_mutex;

	ret = __imx766_power_on(imx766);
	if (ret)
		goto err_free_handler;

	ret = imx766_check_sensor_id(imx766, client);
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
				imx766->otp = otp_ptr;
			} else {
				imx766->otp = NULL;
				devm_kfree(dev, otp_ptr);
			}
		}
	}

continue_probe:

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &imx766_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	imx766->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx766->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(imx766->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 imx766->module_index, facing,
		 IMX766_NAME, dev_name(sd->dev));
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
	__imx766_power_off(imx766);
err_free_handler:
	v4l2_ctrl_handler_free(&imx766->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&imx766->mutex);

	return ret;
}

static void imx766_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx766 *imx766 = to_imx766(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&imx766->ctrl_handler);
	mutex_destroy(&imx766->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__imx766_power_off(imx766);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx766_of_match[] = {
	{ .compatible = "sony,imx766" },
	{},
};
MODULE_DEVICE_TABLE(of, imx766_of_match);
#endif

static const struct i2c_device_id imx766_match_id[] = {
	{ "sony,imx766", 0 },
	{ },
};

static struct i2c_driver imx766_i2c_driver = {
	.driver = {
		.name = IMX766_NAME,
		.pm = &imx766_pm_ops,
		.of_match_table = of_match_ptr(imx766_of_match),
	},
	.probe		= &imx766_probe,
	.remove		= &imx766_remove,
	.id_table	= imx766_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&imx766_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&imx766_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sony imx766 sensor driver");
MODULE_LICENSE("GPL");
