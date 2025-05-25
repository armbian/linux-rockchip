// SPDX-License-Identifier: GPL-2.0
/*
 * gc16b3c driver
 *
 * Copyright (C) 2024 Ingking Co., Ltd.
 *
 * V0.0X01.0X01 init driver.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_gpio.h>

#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x00)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define GC16B3C_LANES			4
#define GC16B3C_BITS_PER_SAMPLE		10

#define GC16B3C_LINK_FREQ_MHZ		(362400000LL/2)


//mipi speed = GC16B3C_LINK_FREQ_MHZ * 2LL
/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define GC16B3C_PIXEL_RATE		(GC16B3C_LINK_FREQ_MHZ * 2LL * GC16B3C_LANES / GC16B3C_BITS_PER_SAMPLE)
#define GC16B3C_XVCLK_FREQ		24000000

#define CHIP_ID				0x16B3
#define GC16B3C_REG_CHIP_ID_H		0x03f0
#define GC16B3C_REG_CHIP_ID_L		0x03f1

#define GC16B3C_REG_CTRL_MODE		0x0100//MIPI enable
#define GC16B3C_MODE_SW_STANDBY		0x80 //close lane_en && mipi_en
#define GC16B3C_MODE_STREAMING		0x01

#define GC16B3C_REG_EXPOSURE_H		0x0202
#define GC16B3C_REG_EXPOSURE_L		0x0203
#define	GC16B3C_EXPOSURE_MIN		2
#define	GC16B3C_EXPOSURE_STEP		2
#define GC16B3C_VTS_MAX			0xffff

#define GC16B3C_REG_AGAIN_H		0x0204
#define GC16B3C_REG_AGAIN_L		0x0205
#define GC16B3C_REG_DGAIN_H		0x020e
#define GC16B3C_REG_DGAIN_L		0x020f

#define GC16B3C_GAIN_MIN		0X400
#define GC16B3C_GAIN_MAX		0X24000
#define GC16B3C_GAIN_STEP		1
#define GC16B3C_GAIN_DEFAULT		0X400

#define GC16B3C_REG_VTS_H		0x0340
#define GC16B3C_REG_VTS_L		0x0341

#define REG_NULL			0xFFFF

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define GC16B3C_NAME			"gc16b3c"

#define GC16B3_MIRROR_NORMAL		1
#define GC16B3_MIRROR_HV		0

#if GC16B3_MIRROR_NORMAL
#define GC16B3_MIRROR			0x00
#elif GC16B3_MIRROR_HV
#define GC16B3_MIRROR			0x03
#else
#define GC16B3_MIRROR			0x00
#endif

static const char * const gc16b3c_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define GC16B3C_NUM_SUPPLIES ARRAY_SIZE(gc16b3c_supply_names)

struct regval {
	u16 addr;
	u8 val;
};

struct gc16b3c_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	const struct regval *reg_list;
};

struct gc16b3c {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[GC16B3C_NUM_SUPPLIES];

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
	struct v4l2_ctrl	*link_freq;
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	bool			streaming;
	bool			power_on;
	const struct gc16b3c_mode *cur_mode;
	unsigned int lane_num;
	unsigned int cfg_num;
	unsigned int pixel_rate;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			Dgain_ratio;
	struct rkmodule_inf	module_inf;
	struct rkmodule_awb_cfg	awb_cfg;
};

#define to_gc16b3c(sd) container_of(sd, struct gc16b3c, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval gc16b3c_2320_1744_30fps_regs[] = {
	{0x0315, 0xd7},
	{0x03a2, 0x0f},
	{0x0321, 0x10},
	{0x0c0c, 0x33},
	{0x0187, 0x40},
	{0x0188, 0x5f},
	{0x0335, 0x51},
	{0x0336, 0x97},
	{0x0314, 0x11},
	{0x031a, 0x00},
	{0x0337, 0x05},
	{0x0316, 0x08},
	{0x0c0e, 0x40},
	{0x0c0d, 0xac},
	{0x0334, 0x40},
	{0x031c, 0xe0},
	{0x0311, 0xf8},
	{0x0268, 0x03},
	{0x02d1, 0x19},
	{0x05a0, 0x0a},
	{0x05c3, 0x50},
	{0x0217, 0x20},
	{0x0074, 0x0a},
	{0x00a0, 0x04},
	{0x0057, 0x0c},
	{0x0358, 0x05},
	{0x0059, 0x11},
	{0x0084, 0x90},
	{0x0087, 0x51},
	{0x0c08, 0x19},
	{0x02d0, 0x40},
	{0x0101, GC16B3_MIRROR},
	{0x0af0, 0x00},
	{0x0c15, 0x05},
	{0x0c55, 0x05},
	{0x0244, 0x15},
	{0x0245, 0x15},
	{0x0348, 0x12},
	{0x0349, 0x30},
	{0x0342, 0x07},
	{0x0343, 0x4e},
	{0x0219, 0x05},
	{0x0e0a, 0x01},
	{0x0e0b, 0x01},
	{0x0e01, 0x75},
	{0x0e03, 0x44},
	{0x0e04, 0x44},
	{0x0e05, 0x44},
	{0x0e06, 0x44},
	{0x0e36, 0x06},
	{0x0e34, 0xf8},
	{0x0e35, 0x34},
	{0x0e15, 0x5a},
	{0x0e16, 0xaa},
	{0x025c, 0xe0},
	{0x0c05, 0xbf},
	{0x0c09, 0x20},
	{0x0c41, 0x0a},
	{0x0c42, 0x00},
	{0x0c44, 0x00},
	{0x0c45, 0xdf},
	{0x0e42, 0x0f},
	{0x0e44, 0x04},
	{0x0e48, 0x00},
	{0x0e4f, 0x04},
	{0x031c, 0x80},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x031c, 0x9f},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x031c, 0x80},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x031c, 0x9f},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x031c, 0xe0},
	{0x02db, 0x01},
	{0x0b00, 0x0f},
	{0x0b01, 0xa2},
	{0x0b02, 0x03},
	{0x0b03, 0x07},
	{0x0b04, 0x11},
	{0x0b05, 0x14},
	{0x0b06, 0x03},
	{0x0b07, 0x07},
	{0x0b08, 0xac},
	{0x0b09, 0x0d},
	{0x0b0a, 0x0c},
	{0x0b0b, 0x07},
	{0x0b0c, 0x40},
	{0x0b0d, 0x34},
	{0x0b0e, 0x03},
	{0x0b0f, 0x07},
	{0x0b10, 0x80},
	{0x0b11, 0x1c},
	{0x0b12, 0x03},
	{0x0b13, 0x07},
	{0x0b14, 0x10},
	{0x0b15, 0xfe},
	{0x0b16, 0x03},
	{0x0b17, 0x07},
	{0x0b18, 0x00},
	{0x0b19, 0xfe},
	{0x0b1a, 0x03},
	{0x0b1b, 0x07},
	{0x0b1c, 0x9f},
	{0x0b1d, 0x1c},
	{0x0b1e, 0x03},
	{0x0b1f, 0x07},
	{0x0b20, 0x00},
	{0x0b21, 0xfe},
	{0x0b22, 0x03},
	{0x0b23, 0x07},
	{0x0b24, 0x00},
	{0x0b25, 0xfe},
	{0x0b26, 0x03},
	{0x0b27, 0x07},
	{0x0b28, 0x80},
	{0x0b29, 0x1c},
	{0x0b2a, 0x03},
	{0x0b2b, 0x07},
	{0x0b2c, 0x10},
	{0x0b2d, 0xfe},
	{0x0b2e, 0x03},
	{0x0b2f, 0x07},
	{0x0b30, 0x00},
	{0x0b31, 0xfe},
	{0x0b32, 0x03},
	{0x0b33, 0x07},
	{0x0b34, 0x9f},
	{0x0b35, 0x1c},
	{0x0b36, 0x03},
	{0x0b37, 0x07},
	{0x0b38, 0x48},
	{0x0b39, 0x80},
	{0x0b3a, 0x01},
	{0x0b3b, 0x07},
	{0x0b3c, 0x10},
	{0x0b3d, 0x84},
	{0x0b3e, 0x00},
	{0x0b3f, 0x07},
	{0x0b40, 0xb8},
	{0x0b41, 0x11},
	{0x0b42, 0x03},
	{0x0b43, 0x07},
	{0x0b44, 0x99},
	{0x0b45, 0x02},
	{0x0b46, 0x01},
	{0x0b47, 0x07},
	{0x0b48, 0xd9},
	{0x0b49, 0x02},
	{0x0b4a, 0x01},
	{0x0b4b, 0x07},
	{0x0b4c, 0x00},
	{0x0b4d, 0xfe},
	{0x0b4e, 0x03},
	{0x0b4f, 0x07},
	{0x0b50, 0x06},
	{0x0b51, 0x14},
	{0x0b52, 0x03},
	{0x0b53, 0x07},
	{0x0b54, 0x2c},
	{0x0b55, 0x0d},
	{0x0b56, 0x0c},
	{0x0b57, 0x07},
	{0x0b58, 0x00},
	{0x0b59, 0x34},
	{0x0b5a, 0x03},
	{0x0b5b, 0x07},
	{0x0b5c, 0xe0},
	{0x0b5d, 0x1c},
	{0x0b5e, 0x03},
	{0x0b5f, 0x07},
	{0x0b60, 0x90},
	{0x0b61, 0x84},
	{0x0b62, 0x00},
	{0x0b63, 0x07},
	{0x0b64, 0x08},
	{0x0b65, 0x80},
	{0x0b66, 0x01},
	{0x0b67, 0x07},
	{0x0b68, 0x07},
	{0x0b69, 0xa2},
	{0x0b6a, 0x03},
	{0x0b6b, 0x07},
	{0x0aab, 0x01},
	{0x0af0, 0x02},
	{0x0aa8, 0xb0},
	{0x0aa9, 0x92},
	{0x0aaa, 0x1b},
	{0x0264, 0x00},
	{0x0265, 0x04},
	{0x0266, 0x1e},
	{0x0267, 0x10},
	{0x0041, 0x30},
	{0x0043, 0x00},
	{0x0044, 0x01},
	{0x005b, 0x02},
	{0x0047, 0xf0},
	{0x0048, 0x0f},
	{0x004b, 0x0f},
	{0x004c, 0x00},
	{0x024a, 0x02},
	{0x0249, 0x00},
	{0x024f, 0x0e},
	{0x024e, 0x80},
	{0x0c12, 0xe6},
	{0x0c52, 0xe6},
	{0x0c10, 0x20},
	{0x0c11, 0x58},
	{0x0c50, 0x20},
	{0x0c51, 0x58},
	{0x0460, 0x08},
	{0x0462, 0x06},
	{0x0464, 0x04},
	{0x0466, 0x02},
	{0x0468, 0x10},
	{0x046a, 0x0e},
	{0x046c, 0x0e},
	{0x046e, 0x0c},
	{0x0461, 0x03},
	{0x0463, 0x03},
	{0x0465, 0x03},
	{0x0467, 0x03},
	{0x0469, 0x04},
	{0x046b, 0x04},
	{0x046d, 0x04},
	{0x046f, 0x04},
	{0x0470, 0x04},
	{0x0472, 0x08},
	{0x0474, 0x0c},
	{0x0476, 0x10},
	{0x0478, 0x06},
	{0x047a, 0x06},
	{0x047c, 0x08},
	{0x047e, 0x08},
	{0x0471, 0x04},
	{0x0473, 0x04},
	{0x0475, 0x04},
	{0x0477, 0x04},
	{0x0479, 0x03},
	{0x047b, 0x03},
	{0x047d, 0x03},
	{0x047f, 0x03},
	{0x0315, 0xd3},
	{0x03a2, 0x0f},
	{0x0321, 0x10},
	{0x0c0c, 0x33},
	{0x0187, 0x40},
	{0x0188, 0x5f},
	{0x0335, 0x59},
	{0x0336, 0x97},
	{0x0314, 0x11},
	{0x031a, 0x01},
	{0x0337, 0x05},
	{0x0316, 0x08},
	{0x0c0e, 0x41},
	{0x0c0d, 0xac},
	{0x0334, 0x40},
	{0x031c, 0xe0},
	{0x0311, 0xf8},
	{0x0268, 0x03},
	{0x0218, 0x01},
	{0x0241, 0xd4},
	{0x0346, 0x00},
	{0x0347, 0x04},
	{0x034a, 0x0d},
	{0x034b, 0xb0},
	{0x0342, 0x07},
	{0x0343, 0x2c},
	{0x0226, 0x00},
	{0x0227, 0x40},
	{0x0202, 0x06},
	{0x0203, 0x8a},
	{0x0340, 0x07},
	{0x0341, 0x28},
	{0x0e24, 0x02},
	{0x0e25, 0x02},
	{0x0e2c, 0x08},
	{0x0e2d, 0x0c},
	{0x0e37, 0x41},
	{0x0e38, 0x41},
	{0x0e17, 0x36},
	{0x0e18, 0x39},
	{0x0e19, 0x60},
	{0x0e1a, 0x62},
	{0x0e49, 0x3a},
	{0x0e2b, 0x6c},
	{0x0e0c, 0x28},
	{0x0e28, 0x28},
	{0x0210, 0xa3},
	{0x02b5, 0x84},
	{0x02b6, 0x72},
	{0x02b7, 0x0e},
	{0x02b8, 0x05},
	{0x0c07, 0xec},
	{0x0c46, 0xfe},
	{0x0c47, 0x02},
	{0x0e43, 0x00},
	{0x0e45, 0x04},
	{0x031c, 0x80},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x031c, 0x9f},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x031c, 0x80},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x031c, 0x9f},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x031c, 0xe0},
	{0x0360, 0x01},
	{0x0360, 0x00},
	{0x0a67, 0x80},
	{0x0313, 0x00},
	{0x0ace, 0x08},
	{0x0a53, 0x04},
	{0x0a65, 0x05},
	{0x0a68, 0x11},
	{0x0a58, 0x00},
	{0x00a4, 0x00},
	{0x00a5, 0x01},
	{0x00a2, 0x00},
	{0x00a3, 0x00},
	{0x00ab, 0x00},
	{0x00ac, 0x00},
	{0x00a7, 0x0d},
	{0x00a8, 0xb0},
	{0x00a9, 0x12},
	{0x00aa, 0x30},
	{0x0a85, 0x1e},
	{0x0a86, 0xa8},
	{0x0a8a, 0x00},
	{0x0a8b, 0xe0},
	{0x0a8c, 0x1e},
	{0x0a8d, 0x10},
	{0x0a90, 0x08},
	{0x0a91, 0x1c},
	{0x0a92, 0x78},
	{0x0a71, 0xd2},
	{0x0a72, 0x12},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x87},
	{0x0313, 0x80},
	{0x0042, 0x00},
	{0x0056, 0x00},
	{0x0488, 0x06},
	{0x048a, 0x06},
	{0x048c, 0x06},
	{0x048e, 0x06},
	{0x05a0, 0x82},
	{0x05ac, 0x00},
	{0x05ad, 0x01},
	{0x0597, 0x6b},
	{0x059a, 0x00},
	{0x059b, 0x00},
	{0x059c, 0x01},
	{0x05a3, 0x0a},
	{0x05a4, 0x08},
	{0x05ab, 0x0a},
	{0x05ae, 0x00},
	{0x0108, 0x48},
	{0x010b, 0x12},
	{0x01c1, 0x95},
	{0x01c2, 0x00},
	{0x0800, 0x05},
	{0x0801, 0x06},
	{0x0802, 0x0a},
	{0x0803, 0x0d},
	{0x0804, 0x12},
	{0x0805, 0x17},
	{0x0806, 0x22},
	{0x0807, 0x2e},
	{0x0808, 0x5a},
	{0x0809, 0x0e},
	{0x080a, 0x32},
	{0x080b, 0x0e},
	{0x080c, 0x33},
	{0x080d, 0x02},
	{0x080e, 0xb8},
	{0x080f, 0x03},
	{0x0810, 0x1d},
	{0x0811, 0x00},
	{0x0812, 0xc0},
	{0x0813, 0x03},
	{0x0814, 0x1d},
	{0x0815, 0x03},
	{0x0816, 0x1e},
	{0x0817, 0x03},
	{0x0818, 0x1e},
	{0x0819, 0x02},
	{0x081a, 0x08},
	{0x081b, 0x3e},
	{0x081c, 0x02},
	{0x081d, 0x00},
	{0x081e, 0x00},
	{0x081f, 0x01},
	{0x0820, 0x01},
	{0x0821, 0x02},
	{0x0822, 0x06},
	{0x0823, 0x3e},
	{0x0824, 0x02},
	{0x0825, 0x00},
	{0x0826, 0x00},
	{0x0827, 0x01},
	{0x0828, 0x01},
	{0x0829, 0x02},
	{0x082a, 0x02},
	{0x082b, 0x3e},
	{0x082c, 0x02},
	{0x082d, 0x00},
	{0x082e, 0x00},
	{0x082f, 0x01},
	{0x0830, 0x01},
	{0x0831, 0x01},
	{0x0832, 0x1c},
	{0x0833, 0x3e},
	{0x0834, 0x02},
	{0x0835, 0x00},
	{0x0836, 0x00},
	{0x0837, 0x01},
	{0x0838, 0x01},
	{0x0839, 0x01},
	{0x083a, 0x16},
	{0x083b, 0x3e},
	{0x083c, 0x02},
	{0x083d, 0x00},
	{0x083e, 0x00},
	{0x083f, 0x01},
	{0x0840, 0x01},
	{0x0841, 0x01},
	{0x0842, 0x10},
	{0x0843, 0x3e},
	{0x0844, 0x02},
	{0x0845, 0x00},
	{0x0846, 0x00},
	{0x0847, 0x01},
	{0x0848, 0x01},
	{0x0849, 0x01},
	{0x084a, 0x08},
	{0x084b, 0x3e},
	{0x084c, 0x02},
	{0x084d, 0x00},
	{0x084e, 0x00},
	{0x084f, 0x01},
	{0x0850, 0x01},
	{0x0851, 0x00},
	{0x0852, 0x1e},
	{0x0853, 0x3e},
	{0x0854, 0x02},
	{0x0855, 0x00},
	{0x0856, 0x00},
	{0x0857, 0x01},
	{0x0858, 0x01},
	{0x0859, 0x00},
	{0x085a, 0x14},
	{0x085b, 0x3e},
	{0x085c, 0x02},
	{0x085d, 0x02},
	{0x085e, 0x00},
	{0x085f, 0x01},
	{0x0860, 0x01},
	{0x0861, 0x00},
	{0x0862, 0x0c},
	{0x0863, 0x36},
	{0x0864, 0x02},
	{0x0865, 0x02},
	{0x0866, 0x00},
	{0x0867, 0x01},
	{0x0868, 0x01},
	{0x0869, 0x00},
	{0x086a, 0x00},
	{0x086b, 0x01},
	{0x086c, 0x00},
	{0x086d, 0x01},
	{0x086e, 0x00},
	{0x086f, 0x00},
	{0x0870, 0x01},
	{0x0871, 0x01},
	{0x0872, 0x62},
	{0x0873, 0x00},
	{0x0874, 0x02},
	{0x0875, 0x01},
	{0x0876, 0xf8},
	{0x0877, 0x00},
	{0x0878, 0x03},
	{0x0879, 0x02},
	{0x087a, 0xc0},
	{0x087b, 0x00},
	{0x087c, 0x04},
	{0x087d, 0x03},
	{0x087e, 0xeb},
	{0x087f, 0x00},
	{0x0880, 0x05},
	{0x0881, 0x05},
	{0x0882, 0x7a},
	{0x0883, 0x00},
	{0x0884, 0x06},
	{0x0885, 0x07},
	{0x0886, 0xe0},
	{0x0887, 0x10},
	{0x0888, 0x05},
	{0x0889, 0x0b},
	{0x088a, 0x02},
	{0x088b, 0x10},
	{0x088c, 0x06},
	{0x088d, 0x0f},
	{0x088e, 0x92},
	{0x088f, 0x14},
	{0x0890, 0xb6},
	{0x0891, 0x1f},
	{0x0892, 0xab},
	{0x0893, 0x1a},
	{0x0894, 0x66},
	{0x0895, 0x01},
	{0x0896, 0x46},
	{0x0897, 0x02},
	{0x0898, 0x01},
	{0x0899, 0x01},
	{0x089a, 0x01},
	{0x089b, 0x03},
	{0x089c, 0x4c},
	{0x089d, 0x04},
	{0x089e, 0xff},
	{0x089f, 0xff},
	{0x08a0, 0x99},
	{0x08a1, 0x02},
	{0x08a2, 0x02},
	{0x08a3, 0x04},
	{0x08a4, 0x02},
	{0x08a5, 0x0e},
	{0x08a6, 0x02},
	{0x08a7, 0x03},
	{0x08a8, 0x40},
	{0x08a9, 0x04},
	{0x08aa, 0xff},
	{0x08ab, 0xff},
	{0x08ac, 0x00},
	{0x05ac, 0x01},
	{0x0207, 0xc4},
	{0x05a0, 0xc2},
	{0x01c0, 0x01},
	{0x0096, 0x81},
	{0x0097, 0x08},
	{0x0098, 0x87},
	{0x0204, 0x04},
	{0x0205, 0x00},
	{0x0208, 0x01},
	{0x0209, 0x6f},
	{0x0351, 0x00},
	{0x0352, 0x04},
	{0x0353, 0x00},
	{0x0354, 0x04},
	{0x034c, 0x09},
	{0x034d, 0x10},
	{0x034e, 0x06},
	{0x034f, 0xd0},
	{0x0180, 0x48},
	{0x0181, 0xf0},
	{0x0185, 0x01},
	{0x0103, 0x10},
	{0x0106, 0x39},
	{0x0114, 0x03},
	{0x0115, 0x20},
	{0x0121, 0x02},
	{0x0122, 0x03},
	{0x0123, 0x0a},
	{0x0124, 0x00},
	{0x0125, 0x08},
	{0x0126, 0x04},
	{0x0128, 0xf0},
	{0x0129, 0x03},
	{0x012a, 0x02},
	{0x012b, 0x05},
	{0x0a70, 0x11},
	{0x0313, 0x80},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x0a70, 0x00},
	{0x0070, 0x05},
	{0x0089, 0x03},
	{0x009b, 0x40},
	{0x00a4, 0x80},
	{0x00a0, 0x05},
	{0x00a6, 0x07},
	{0x0080, 0xd2},
	{0x00c1, 0x80},
	{0x00c2, 0x11},
	{0x024d, 0x01},
	{0x0084, 0x10},
	{0x0268, 0x00},
	{0x031c, 0x9f},
	{0x0100, 0x01},

	{REG_NULL, 0x00},
};

static const struct gc16b3c_mode supported_modes_4lane[] = {
	{
		.width = 2320,
		.height = 1744,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x500,
		.hts_def = 0x19B0,
		.vts_def = 0x728,
		.mipi_freq_idx = 0,
		.reg_list = gc16b3c_2320_1744_30fps_regs,
	},
};

static const struct gc16b3c_mode *supported_modes;

static const s64 link_freq_menu_items[] = {
	GC16B3C_LINK_FREQ_MHZ,
};

/* Write registers up to 4 at a time */
static int gc16b3c_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	dev_dbg(&client->dev, "write reg(0x%x val:0x%x)!\n", reg, val);
	buf[0] = (reg >> 8) & 0xFF;
	buf[1] = reg & 0xFF;
	buf[2] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		return 0;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		return 0;

	dev_err(&client->dev,
		"gc16b3c write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int gc16b3c_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = gc16b3c_write_reg(client, regs[i].addr, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int gc16b3c_read_reg(struct i2c_client *client, u16 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	buf[0] = (reg >> 8) & 0xFF;
	buf[1] = reg & 0xFF;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret >= 0) {
		*val = buf[0];
		return 0;
	}

	dev_err(&client->dev,
		"gc16b3c read reg:0x%x failed !\n", reg);

	return ret;
}

static int gc16b3c_get_reso_dist(const struct gc16b3c_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		abs(mode->height - framefmt->height);
}

static const struct gc16b3c_mode *
gc16b3c_find_best_fit(struct gc16b3c *gc16b3c,
		      struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < gc16b3c->cfg_num; i++) {
		dist = gc16b3c_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int gc16b3c_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);
	const struct gc16b3c_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&gc16b3c->mutex);

	mode = gc16b3c_find_best_fit(gc16b3c, fmt);
	fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gc16b3c->mutex);
		return -ENOTTY;
#endif
	} else {
		gc16b3c->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc16b3c->hblank, h_blank,
			h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(gc16b3c->vblank, vblank_def,
			GC16B3C_VTS_MAX - mode->height,
			1, vblank_def);
		__v4l2_ctrl_s_ctrl(gc16b3c->link_freq, mode->mipi_freq_idx);
	}

	mutex_unlock(&gc16b3c->mutex);

	return 0;
}

static int gc16b3c_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);
	const struct gc16b3c_mode *mode = gc16b3c->cur_mode;

	mutex_lock(&gc16b3c->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gc16b3c->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&gc16b3c->mutex);

	return 0;
}

static int gc16b3c_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;

	return 0;
}

static int gc16b3c_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);

	if (fse->index >= gc16b3c->cfg_num)
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SRGGB10_1X10)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int gc16b3c_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);
	const struct gc16b3c_mode *mode = gc16b3c->cur_mode;

	mutex_lock(&gc16b3c->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&gc16b3c->mutex);

	return 0;
}

static void gc16b3c_get_module_inf(struct gc16b3c *gc16b3c,
				   struct rkmodule_inf *inf)
{
	strscpy(inf->base.sensor,
		GC16B3C_NAME,
		sizeof(inf->base.sensor));
	strscpy(inf->base.module,
		gc16b3c->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens,
		gc16b3c->len_name,
		sizeof(inf->base.lens));
}

static void gc16b3c_set_module_inf(struct gc16b3c *gc16b3c,
				   struct rkmodule_awb_cfg *cfg)
{
	mutex_lock(&gc16b3c->mutex);
	memcpy(&gc16b3c->awb_cfg, cfg, sizeof(*cfg));
	mutex_unlock(&gc16b3c->mutex);
}

static long gc16b3c_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gc16b3c_get_module_inf(gc16b3c, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_AWB_CFG:
		gc16b3c_set_module_inf(gc16b3c, (struct rkmodule_awb_cfg *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);
		if (stream) {
			ret = gc16b3c_write_reg(gc16b3c->client,
						GC16B3C_REG_CTRL_MODE,
						GC16B3C_MODE_STREAMING);
		} else {
			ret = gc16b3c_write_reg(gc16b3c->client,
						GC16B3C_REG_CTRL_MODE,
						GC16B3C_MODE_SW_STANDBY);
		}
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gc16b3c_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc16b3c_ioctl(sd, cmd, inf);
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
			ret = gc16b3c_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = gc16b3c_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;

		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}
#endif

static int __gc16b3c_start_stream(struct gc16b3c *gc16b3c)
{
	int ret;

	ret = gc16b3c_write_array(gc16b3c->client, gc16b3c->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&gc16b3c->mutex);
	ret = v4l2_ctrl_handler_setup(&gc16b3c->ctrl_handler);
	mutex_lock(&gc16b3c->mutex);
	if (ret)
		return ret;
	ret = gc16b3c_write_reg(gc16b3c->client,
				GC16B3C_REG_CTRL_MODE,
				GC16B3C_MODE_STREAMING);
	return ret;
}

static int __gc16b3c_stop_stream(struct gc16b3c *gc16b3c)
{
	int ret = 0;

	ret = gc16b3c_write_reg(gc16b3c->client,
				GC16B3C_REG_CTRL_MODE,
				GC16B3C_MODE_SW_STANDBY);
	return ret;
}

static int gc16b3c_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);
	struct i2c_client *client = gc16b3c->client;
	int ret = 0;

	mutex_lock(&gc16b3c->mutex);
	on = !!on;
	if (on == gc16b3c->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gc16b3c_start_stream(gc16b3c);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gc16b3c_stop_stream(gc16b3c);
		pm_runtime_put(&client->dev);
	}

	gc16b3c->streaming = on;

unlock_and_return:
	mutex_unlock(&gc16b3c->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 gc16b3c_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, GC16B3C_XVCLK_FREQ / 1000 / 1000);
}

static int __gc16b3c_power_on(struct gc16b3c *gc16b3c)
{
	int ret;
	u32 delay_us;
	struct device *dev = &gc16b3c->client->dev;

	if (!IS_ERR_OR_NULL(gc16b3c->pins_default)) {
		ret = pinctrl_select_state(gc16b3c->pinctrl,
					   gc16b3c->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(gc16b3c->xvclk, GC16B3C_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(gc16b3c->xvclk) != GC16B3C_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(gc16b3c->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}
	if (!IS_ERR(gc16b3c->reset_gpio))
		gpiod_set_value_cansleep(gc16b3c->reset_gpio, 0);

	ret = regulator_bulk_enable(GC16B3C_NUM_SUPPLIES, gc16b3c->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	usleep_range(1000, 1100);
	if (!IS_ERR(gc16b3c->reset_gpio))
		gpiod_set_value_cansleep(gc16b3c->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(gc16b3c->pwdn_gpio))
		gpiod_set_value_cansleep(gc16b3c->pwdn_gpio, 1);

	usleep_range(15000, 16000);
	/* 8192 cycles prior to first SCCB transaction */
	delay_us = gc16b3c_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(gc16b3c->xvclk);

	return ret;
}

static void __gc16b3c_power_off(struct gc16b3c *gc16b3c)
{
	int ret;

	if (!IS_ERR(gc16b3c->pwdn_gpio))
		gpiod_set_value_cansleep(gc16b3c->pwdn_gpio, 0);
	clk_disable_unprepare(gc16b3c->xvclk);
	if (!IS_ERR(gc16b3c->reset_gpio))
		gpiod_set_value_cansleep(gc16b3c->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(gc16b3c->pins_sleep)) {
		ret = pinctrl_select_state(gc16b3c->pinctrl,
			gc16b3c->pins_sleep);
		if (ret < 0)
			dev_dbg(&gc16b3c->client->dev, "could not set pins\n");
	}
	regulator_bulk_disable(GC16B3C_NUM_SUPPLIES, gc16b3c->supplies);
}


static int gc16b3c_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);
	struct i2c_client *client = gc16b3c->client;
	int ret = 0;

	dev_info(&gc16b3c->client->dev, "%s on:%d\n", __func__, on);
	mutex_lock(&gc16b3c->mutex);

	/* If the power state is not modified - no work to do. */
	if (gc16b3c->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		__gc16b3c_power_on(gc16b3c);
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gc16b3c->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		__gc16b3c_power_off(gc16b3c);
		gc16b3c->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc16b3c->mutex);

	return ret;
}

static int gc16b3c_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);

	return __gc16b3c_power_on(gc16b3c);
}

static int gc16b3c_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);

	__gc16b3c_power_off(gc16b3c);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc16b3c_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);
	struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gc16b3c_mode *def_mode = &supported_modes[0];

	mutex_lock(&gc16b3c->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&gc16b3c->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sensor_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				 struct v4l2_mbus_config *config)
{
	struct gc16b3c *sensor = to_gc16b3c(sd);

	if (sensor->lane_num == 4) {
		config->type = V4L2_MBUS_CSI2_DPHY;
		config->bus.mipi_csi2.num_data_lanes = GC16B3C_LANES;
	} else {
		dev_err(&sensor->client->dev,
			"unsupported lane_num(%d)\n", sensor->lane_num);
	}
	return 0;
}

static int gc16b3c_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum *fie)
{
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);

	if (fie->index >= gc16b3c->cfg_num)
		return -EINVAL;

	fie->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static const struct dev_pm_ops gc16b3c_pm_ops = {
	SET_RUNTIME_PM_OPS(gc16b3c_runtime_suspend,
			   gc16b3c_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gc16b3c_internal_ops = {
	.open = gc16b3c_open,
};
#endif

static const struct v4l2_subdev_core_ops gc16b3c_core_ops = {
	.s_power = gc16b3c_s_power,
	.ioctl = gc16b3c_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc16b3c_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gc16b3c_video_ops = {
	.s_stream = gc16b3c_s_stream,
	.g_frame_interval = gc16b3c_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc16b3c_pad_ops = {
	.enum_mbus_code = gc16b3c_enum_mbus_code,
	.enum_frame_size = gc16b3c_enum_frame_sizes,
	.enum_frame_interval = gc16b3c_enum_frame_interval,
	.get_fmt = gc16b3c_get_fmt,
	.set_fmt = gc16b3c_set_fmt,
	.get_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_ops gc16b3c_subdev_ops = {
	.core	= &gc16b3c_core_ops,
	.video	= &gc16b3c_video_ops,
	.pad	= &gc16b3c_pad_ops,
};

static const char * const gc16b3c_test_pattern_menu[] = {
	"Disabled",
	"solid_color",
	"color_bars",
	"ade_to_gray_color_bars",
	"PN9",
	"horizental_gradient",
	"checkerboard",
	"slant",
	"resolution"
};

static int gc16b3c_set_exposure_reg(struct gc16b3c *gc16b3c, u32 exposure)
{
	int ret = 0;
	u32 caltime = 0;

	caltime = exposure / 2;
	caltime = caltime * 2;
	ret = gc16b3c_write_reg(gc16b3c->client,
		GC16B3C_REG_EXPOSURE_H,
		(caltime >> 8) & 0xFF);
	ret |= gc16b3c_write_reg(gc16b3c->client,
		GC16B3C_REG_EXPOSURE_L,
		caltime & 0xFF);
	return ret;
}

static int gc16b3c_set_gain_reg(struct gc16b3c *gc16b3c, u32 t_gain)
{
	int ret = 0;
	u32 a_gain, d_gain;

	dev_dbg(&gc16b3c->client->dev, "%s(%d) t_gain(%d)!\n", __func__, __LINE__, t_gain);
	if (t_gain < GC16B3C_GAIN_MIN)
		t_gain = GC16B3C_GAIN_MIN;
	else if (t_gain > GC16B3C_GAIN_MAX)
		t_gain = GC16B3C_GAIN_MAX;

	if (t_gain <= 16 * 0x400) {
		a_gain = t_gain;
		d_gain = 0x400;
	} else {
		a_gain = 16 * 0x400;
		d_gain = t_gain * 0x400 / a_gain;
	}

	ret = gc16b3c_write_reg(gc16b3c->client,
		GC16B3C_REG_AGAIN_H, a_gain >> 8);
	ret |= gc16b3c_write_reg(gc16b3c->client,
		GC16B3C_REG_AGAIN_L, a_gain & 0xFF);

	ret |= gc16b3c_write_reg(gc16b3c->client,
		GC16B3C_REG_DGAIN_H,
		d_gain >> 8);
	ret |= gc16b3c_write_reg(gc16b3c->client,
		GC16B3C_REG_DGAIN_L,
		d_gain & 0xFF);
	return ret;
}

static int gc16b3c_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc16b3c *gc16b3c = container_of(ctrl->handler,
					     struct gc16b3c, ctrl_handler);
	struct i2c_client *client = gc16b3c->client;
	s64 max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = gc16b3c->cur_mode->height + ctrl->val - 64;
		__v4l2_ctrl_modify_range(gc16b3c->exposure,
			gc16b3c->exposure->minimum, max,
			gc16b3c->exposure->step,
			gc16b3c->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = gc16b3c_set_exposure_reg(gc16b3c, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = gc16b3c_set_gain_reg(gc16b3c, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = gc16b3c_write_reg(gc16b3c->client,
			GC16B3C_REG_VTS_H,
			((ctrl->val + gc16b3c->cur_mode->height) >> 8) & 0xff);
		ret |= gc16b3c_write_reg(gc16b3c->client,
			GC16B3C_REG_VTS_L,
			(ctrl->val + gc16b3c->cur_mode->height) & 0xff);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			__func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gc16b3c_ctrl_ops = {
	.s_ctrl = gc16b3c_set_ctrl,
};

static int gc16b3c_initialize_controls(struct gc16b3c *gc16b3c)
{
	const struct gc16b3c_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &gc16b3c->ctrl_handler;
	mode = gc16b3c->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &gc16b3c->mutex;

	gc16b3c->link_freq = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(link_freq_menu_items) - 1, 0, link_freq_menu_items);
	v4l2_ctrl_s_ctrl(gc16b3c->link_freq, mode->mipi_freq_idx);

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
		0, GC16B3C_PIXEL_RATE, 1, GC16B3C_PIXEL_RATE);
	h_blank = mode->hts_def - mode->width;
	gc16b3c->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
		h_blank, h_blank, 1, h_blank);
	if (gc16b3c->hblank)
		gc16b3c->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	gc16b3c->vblank = v4l2_ctrl_new_std(handler, &gc16b3c_ctrl_ops,
		V4L2_CID_VBLANK, vblank_def,
		GC16B3C_VTS_MAX - mode->height,
		1, vblank_def);

	exposure_max = mode->vts_def - 64;
	gc16b3c->exposure = v4l2_ctrl_new_std(handler, &gc16b3c_ctrl_ops,
		V4L2_CID_EXPOSURE, GC16B3C_EXPOSURE_MIN,
		exposure_max, GC16B3C_EXPOSURE_STEP,
		mode->exp_def);

	gc16b3c->anal_gain = v4l2_ctrl_new_std(handler, &gc16b3c_ctrl_ops,
		V4L2_CID_ANALOGUE_GAIN, GC16B3C_GAIN_MIN,
		GC16B3C_GAIN_MAX, GC16B3C_GAIN_STEP,
		GC16B3C_GAIN_DEFAULT);

	gc16b3c->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&gc16b3c_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(gc16b3c_test_pattern_menu) - 1,
				0, 0, gc16b3c_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_err(&gc16b3c->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc16b3c->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gc16b3c_check_sensor_id(struct gc16b3c *gc16b3c,
	struct i2c_client *client)
{
	struct device *dev = &gc16b3c->client->dev;
	u16 id = 0;
	u8 reg_H = 0;
	u8 reg_L = 0;
	int ret;

	ret = gc16b3c_read_reg(client, GC16B3C_REG_CHIP_ID_H, &reg_H);
	if (ret)
		ret = gc16b3c_read_reg(client, GC16B3C_REG_CHIP_ID_H, &reg_H);
	ret |= gc16b3c_read_reg(client, GC16B3C_REG_CHIP_ID_L, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected gc%04x sensor\n", id);
	return ret;
}

static int gc16b3c_configure_regulators(struct gc16b3c *gc16b3c)
{
	unsigned int i;

	for (i = 0; i < GC16B3C_NUM_SUPPLIES; i++)
		gc16b3c->supplies[i].supply = gc16b3c_supply_names[i];

	return devm_regulator_bulk_get(&gc16b3c->client->dev,
		GC16B3C_NUM_SUPPLIES,
		gc16b3c->supplies);
}

static void free_gpio(struct gc16b3c *sensor)
{
	struct device *dev = &sensor->client->dev;
	unsigned int temp_gpio = -1;

	if (!IS_ERR(sensor->reset_gpio)) {
		temp_gpio = desc_to_gpio(sensor->reset_gpio);
		dev_info(dev, "free gpio(%d)!\n", temp_gpio);
		gpio_free(temp_gpio);
	}

	if (!IS_ERR(sensor->pwdn_gpio)) {
		temp_gpio = desc_to_gpio(sensor->pwdn_gpio);
		dev_info(dev, "free gpio(%d)!\n", temp_gpio);
		gpio_free(temp_gpio);
	}
}

static int gc16b3c_parse_of(struct gc16b3c *gc16b3c)
{
	struct device *dev = &gc16b3c->client->dev;
	struct device_node *endpoint;
	struct fwnode_handle *fwnode;
	int rval;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}
	fwnode = of_fwnode_handle(endpoint);
	rval = fwnode_property_read_u32_array(fwnode, "data-lanes", NULL, 0);
	if (rval <= 0) {
		dev_warn(dev, " Get mipi lane num failed!\n");
		return -1;
	}

	gc16b3c->lane_num = rval;
	if (gc16b3c->lane_num == 4) {
		gc16b3c->cur_mode = &supported_modes_4lane[0];
		supported_modes = supported_modes_4lane;
		gc16b3c->cfg_num = ARRAY_SIZE(supported_modes_4lane);

		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		gc16b3c->pixel_rate = link_freq_menu_items[gc16b3c->cur_mode->mipi_freq_idx] *
				      2U * gc16b3c->lane_num / 10U;
		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
			 gc16b3c->lane_num, gc16b3c->pixel_rate);
	} else {
		dev_err(dev, "unsupported lane_num(%d)\n", gc16b3c->lane_num);
		return -1;
	}
	return 0;
}

static int gc16b3c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gc16b3c *gc16b3c;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	gc16b3c = devm_kzalloc(dev, sizeof(*gc16b3c), GFP_KERNEL);
	if (!gc16b3c)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
		&gc16b3c->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
		&gc16b3c->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
		&gc16b3c->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
		&gc16b3c->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}
	gc16b3c->client = client;

	gc16b3c->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(gc16b3c->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	gc16b3c->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(gc16b3c->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	gc16b3c->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(gc16b3c->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = gc16b3c_configure_regulators(gc16b3c);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ret = gc16b3c_parse_of(gc16b3c);
	if (ret != 0)
		return -EINVAL;

	gc16b3c->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(gc16b3c->pinctrl)) {
		gc16b3c->pins_default =
			pinctrl_lookup_state(gc16b3c->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(gc16b3c->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		gc16b3c->pins_sleep =
			pinctrl_lookup_state(gc16b3c->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(gc16b3c->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&gc16b3c->mutex);

	sd = &gc16b3c->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc16b3c_subdev_ops);
	ret = gc16b3c_initialize_controls(gc16b3c);
	if (ret)
		goto err_destroy_mutex;

	ret = __gc16b3c_power_on(gc16b3c);
	if (ret)
		goto err_free_handler;

	ret = gc16b3c_check_sensor_id(gc16b3c, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc16b3c_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gc16b3c->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc16b3c->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc16b3c->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc16b3c->module_index, facing,
		 GC16B3C_NAME, dev_name(sd->dev));
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
	__gc16b3c_power_off(gc16b3c);
	free_gpio(gc16b3c);
err_free_handler:
	v4l2_ctrl_handler_free(&gc16b3c->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc16b3c->mutex);

	return ret;
}

static void gc16b3c_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc16b3c *gc16b3c = to_gc16b3c(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gc16b3c->ctrl_handler);
	mutex_destroy(&gc16b3c->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc16b3c_power_off(gc16b3c);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc16b3c_of_match[] = {
	{ .compatible = "galaxycore,gc16b3c" },
	{},
};
MODULE_DEVICE_TABLE(of, gc16b3c_of_match);
#endif

static const struct i2c_device_id gc16b3c_match_id[] = {
	{ "galaxycore,gc16b3c", 0 },
	{ },
};

static struct i2c_driver gc16b3c_i2c_driver = {
	.driver = {
		.name = GC16B3C_NAME,
		.pm = &gc16b3c_pm_ops,
		.of_match_table = of_match_ptr(gc16b3c_of_match),
	},
	.probe		= &gc16b3c_probe,
	.remove		= &gc16b3c_remove,
	.id_table	= gc16b3c_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc16b3c_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc16b3c_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("GalaxyCore gc16b3c sensor driver");
MODULE_LICENSE("GPL");
