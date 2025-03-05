// SPDX-License-Identifier: GPL-2.0
/*
 * gc05a2 driver
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X01 init first version.
 * V0.0X01.0X01 update sensor driver.
 * 1. adjust power sequence to suit spec.
 * 2. fix bayer pattern to suit setting.
 * V0.0X01.0X02 add mirror & flip support.
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
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN	V4L2_CID_GAIN
#endif

#define GC05A2_REG_VALUE_08BIT	1
#define GC05A2_REG_VALUE_16BIT	2
#define GC05A2_REG_VALUE_24BIT	3

#define GC05A2_LANES			4
#define GC05A2_BITS_PER_SAMPLE	10
#define GC05A2_MIPI_FREQ_451MHZ	451000000LL


/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define GC05A2_PIXEL_RATE		(MIPI_FREQ * 2LL * GC05A2_LANES / GC05A2_BITS_PER_SAMPLE)
#define GC05A2_XVCLK_FREQ		24000000

#define CHIP_ID					0x05a2
#define GC05A2_REG_CHIP_ID_H	0x03f0
#define GC05A2_REG_CHIP_ID_L	0x03f1

#define GC05A2_REG_CTRL_MODE	0x0100
#define GC05A2_MODE_SW_STANDBY	0x00
#define GC05A2_MODE_STREAMING	0x01

#define GC05A2_REG_EXPOSURE_H	0x0202
#define GC05A2_REG_EXPOSURE_L	0x0203
#define GC05A2_FETCH_HIGH_BYTE(VAL) (((VAL) >> 8) & 0xFF)	/* 4 Bits */
#define GC05A2_FETCH_LOW_BYTE(VAL)	((VAL) & 0xFF)	/* 8 Bits */
#define	GC05A2_EXPOSURE_MIN		4
#define	GC05A2_EXPOSURE_STEP	1
#define GC05A2_VTS_MAX			0xfffe

#define GC05A2_REG_GAIN_H		0x0204
#define GC05A2_REG_GAIN_L		0x0205

#define GC05A2_AGAIN_MIN		0x400
#define GC05A2_AGAIN_MAX		0x4000
#define GC05A2_AGAIN_STEP		1
#define GC05A2_AGAIN_DEFAULT	0x800

#define GC05A2_REG_VTS_H		0x0340
#define GC05A2_REG_VTS_L		0x0341

#define GC05A2_FLIP_MIRROR_REG		0x0101
#define MIRROR_BIT_MASK			BIT(0)
#define FLIP_BIT_MASK			BIT(1)

#define REG_NULL				0xFFFF

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define GC05A2_NAME				"gc05a2"
#define GC05A2_MEDIA_BUS_FMT	MEDIA_BUS_FMT_SGRBG10_1X10

static const char * const gc05a2_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define GC05A2_NUM_SUPPLIES ARRAY_SIZE(gc05a2_supply_names)

struct gc05a2_id_name {
	u32 id;
	char name[RKMODULE_NAME_LEN];
};

struct regval {
	u16 addr;
	u8 val;
};

struct gc05a2_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	const struct regval *reg_list;
	const struct regval *global_reg_list;
	u32 mipi_freq_idx;
	u32 vc[PAD_MAX];
};

struct gc05a2 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[GC05A2_NUM_SUPPLIES];
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
	struct v4l2_ctrl	*h_flip;
	struct v4l2_ctrl	*v_flip;
	u8			flip_mirror;
	struct mutex		mutex;
	bool			streaming;
	unsigned int		lane_num;
	unsigned int		cfg_num;
	unsigned int		pixel_rate;
	bool			power_on;
	const struct gc05a2_mode *cur_mode;
	const struct gc05a2_mode *support_modes;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	struct rkmodule_inf	module_inf;
	struct rkmodule_awb_cfg	awb_cfg;
};

#define to_gc05a2(sd) container_of(sd, struct gc05a2, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval gc05a2_global_regs[] = {
	/*system*/
	{0x0315, 0xd4},
	{0x0d06, 0x01},
	{0x0a70, 0x80},
	{0x031a, 0x00},
	{0x0314, 0x00},
	{0x0130, 0x08},
	{0x0132, 0x01},
	{0x0135, 0x01},
	{0x0136, 0x38},   // 0x38
	{0x0137, 0x03},
	{0x0134, 0x5b},
	{0x031c, 0xe0},
	{0x0d82, 0x14},
	{0x0dd1, 0x56},
	/*gate_mode*/
	{0x0af4, 0x01},
	{0x0002, 0x10},
	{0x00c3, 0x34},
	/*pre_setting*/
	{0x0084, 0x21},
	{0x0d05, 0xcc},
	{0x0218, 0x00},
	{0x005e, 0x48},
	{0x0d06, 0x01},
	{0x0007, 0x16},
	{0x0101, 0x00},
	/*analog*/
	{0x0342, 0x07},
	{0x0343, 0x28},
	{0x0220, 0x07},
	{0x0221, 0xd0},
	{0x0202, 0x07},
	{0x0203, 0x32},
	{0x0340, 0x07},
	{0x0341, 0xf0},
	{0x0219, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x04},
	{0x0d14, 0x00},
	{0x0d13, 0x05},
	{0x0d16, 0x05},
	{0x0d15, 0x1d},
	{0x00c0, 0x0a},
	{0x00c1, 0x30},
	{0x034a, 0x07},
	{0x034b, 0xa8},
	{0x0e0a, 0x00},
	{0x0e0b, 0x00},
	{0x0e0e, 0x03},
	{0x0e0f, 0x00},
	{0x0e06, 0x0a},
	{0x0e23, 0x15},
	{0x0e24, 0x15},
	{0x0e2a, 0x10},
	{0x0e2b, 0x10},
	{0x0e17, 0x49},
	{0x0e1b, 0x1c},
	{0x0e3a, 0x36},
	{0x0d11, 0x84},
	{0x0e52, 0x14},
	{0x000b, 0x10},
	{0x0008, 0x08},
	{0x0223, 0x17},
	{0x0d27, 0x39},
	{0x0d22, 0x00},
	{0x03f6, 0x0d},
	{0x0d04, 0x07},
	{0x03f3, 0x72},
	{0x03f4, 0xb8},
	{0x03f5, 0xbc},
	{0x0d02, 0x73},
	/*auto load start*/
	{0x00c4, 0x00},
	{0x00c5, 0x01},
	{0x0af6, 0x00},
	{0x0ba0, 0x17},
	{0x0ba1, 0x00},
	{0x0ba2, 0x00},
	{0x0ba3, 0x00},
	{0x0ba4, 0x03},
	{0x0ba5, 0x00},
	{0x0ba6, 0x00},
	{0x0ba7, 0x00},
	{0x0ba8, 0x40},
	{0x0ba9, 0x00},
	{0x0baa, 0x00},
	{0x0bab, 0x00},
	{0x0bac, 0x40},
	{0x0bad, 0x00},
	{0x0bae, 0x00},
	{0x0baf, 0x00},
	{0x0bb0, 0x02},
	{0x0bb1, 0x00},
	{0x0bb2, 0x00},
	{0x0bb3, 0x00},
	{0x0bb8, 0x02},
	{0x0bb9, 0x00},
	{0x0bba, 0x00},
	{0x0bbb, 0x00},
	{0x0a70, 0x80},
	{0x0a71, 0x00},
	{0x0a72, 0x00},
	{0x0a66, 0x00},
	{0x0a67, 0x80},
	{0x0a4d, 0x4e},
	{0x0a50, 0x00},
	{0x0a4f, 0x0c},
	{0x0a66, 0x00},
	{0x00ca, 0x00},
	{0x00cb, 0x00},
	{0x00cc, 0x00},
	{0x00cd, 0x00},
	{0x0aa1, 0x00},
	{0x0aa2, 0xe0},
	{0x0aa3, 0x00},
	{0x0aa4, 0x40},
	{0x0a90, 0x03},
	{0x0a91, 0x0e},
	{0x0a94, 0x80},
	/*standby*/
	{0x0af6, 0x20},
	{0x0b00, 0x91},
	{0x0b01, 0x17},
	{0x0b02, 0x01},
	{0x0b03, 0x00},
	{0x0b04, 0x01},
	{0x0b05, 0x17},
	{0x0b06, 0x01},
	{0x0b07, 0x00},
	{0x0ae9, 0x01},
	{0x0aea, 0x02},
	{0x0ae8, 0x53},
	{0x0ae8, 0x43},
	/*gain_partition*/
	{0x0af6, 0x30},
	{0x0b00, 0x08},
	{0x0b01, 0x0f},
	{0x0b02, 0x00},
	{0x0b04, 0x1c},
	{0x0b05, 0x24},
	{0x0b06, 0x00},
	{0x0b08, 0x30},
	{0x0b09, 0x40},
	{0x0b0a, 0x00},
	{0x0b0c, 0x0e},
	{0x0b0d, 0x2a},
	{0x0b0e, 0x00},
	{0x0b10, 0x0e},
	{0x0b11, 0x2b},
	{0x0b12, 0x00},
	{0x0b14, 0x0e},
	{0x0b15, 0x23},
	{0x0b16, 0x00},
	{0x0b18, 0x0e},
	{0x0b19, 0x24},
	{0x0b1a, 0x00},
	{0x0b1c, 0x0c},
	{0x0b1d, 0x0c},
	{0x0b1e, 0x00},
	{0x0b20, 0x03},
	{0x0b21, 0x03},
	{0x0b22, 0x00},
	{0x0b24, 0x0e},
	{0x0b25, 0x0e},
	{0x0b26, 0x00},
	{0x0b28, 0x03},
	{0x0b29, 0x03},
	{0x0b2a, 0x00},
	{0x0b2c, 0x12},
	{0x0b2d, 0x12},
	{0x0b2e, 0x00},
	{0x0b30, 0x08},
	{0x0b31, 0x08},
	{0x0b32, 0x00},
	{0x0b34, 0x14},
	{0x0b35, 0x14},
	{0x0b36, 0x00},
	{0x0b38, 0x10},
	{0x0b39, 0x10},
	{0x0b3a, 0x00},
	{0x0b3c, 0x16},
	{0x0b3d, 0x16},
	{0x0b3e, 0x00},
	{0x0b40, 0x10},
	{0x0b41, 0x10},
	{0x0b42, 0x00},
	{0x0b44, 0x19},
	{0x0b45, 0x19},
	{0x0b46, 0x00},
	{0x0b48, 0x16},
	{0x0b49, 0x16},
	{0x0b4a, 0x00},
	{0x0b4c, 0x19},
	{0x0b4d, 0x19},
	{0x0b4e, 0x00},
	{0x0b50, 0x16},
	{0x0b51, 0x16},
	{0x0b52, 0x00},
	{0x0b80, 0x01},
	{0x0b81, 0x00},
	{0x0b82, 0x00},
	{0x0b84, 0x00},
	{0x0b85, 0x00},
	{0x0b86, 0x00},
	{0x0b88, 0x01},
	{0x0b89, 0x6a},
	{0x0b8a, 0x00},
	{0x0b8c, 0x00},
	{0x0b8d, 0x01},
	{0x0b8e, 0x00},
	{0x0b90, 0x01},
	{0x0b91, 0xf6},
	{0x0b92, 0x00},
	{0x0b94, 0x00},
	{0x0b95, 0x02},
	{0x0b96, 0x00},
	{0x0b98, 0x02},
	{0x0b99, 0xc4},
	{0x0b9a, 0x00},
	{0x0b9c, 0x00},
	{0x0b9d, 0x03},
	{0x0b9e, 0x00},
	{0x0ba0, 0x03},
	{0x0ba1, 0xd8},
	{0x0ba2, 0x00},
	{0x0ba4, 0x00},
	{0x0ba5, 0x04},
	{0x0ba6, 0x00},
	{0x0ba8, 0x05},
	{0x0ba9, 0x4d},
	{0x0baa, 0x00},
	{0x0bac, 0x00},
	{0x0bad, 0x05},
	{0x0bae, 0x00},
	{0x0bb0, 0x07},
	{0x0bb1, 0x3e},
	{0x0bb2, 0x00},
	{0x0bb4, 0x00},
	{0x0bb5, 0x06},
	{0x0bb6, 0x00},
	{0x0bb8, 0x0a},
	{0x0bb9, 0x1a},
	{0x0bba, 0x00},
	{0x0bbc, 0x09},
	{0x0bbd, 0x36},
	{0x0bbe, 0x00},
	{0x0bc0, 0x0e},
	{0x0bc1, 0x66},
	{0x0bc2, 0x00},
	{0x0bc4, 0x10},
	{0x0bc5, 0x06},
	{0x0bc6, 0x00},
	{0x02c1, 0xe0},
	{0x0207, 0x04},
	{0x02c2, 0x10},
	{0x02c3, 0x74},
	{0x02c5, 0x09},
	{0x02c1, 0xe0},
	{0x0207, 0x04},
	{0x02c2, 0x10},
	{0x02c5, 0x09},
	{0x02c1, 0xe0},
	{0x0207, 0x04},
	{0x02c2, 0x10},
	{0x02c5, 0x09},
	/*auto load CH_GAIN*/
	{0x0aa1, 0x15},
	{0x0aa2, 0x50},
	{0x0aa3, 0x00},
	{0x0aa4, 0x09},
	{0x0a90, 0x25},
	{0x0a91, 0x0e},
	{0x0a94, 0x80},
	/*ISP*/
	{0x0050, 0x00},
	{0x0089, 0x83},
	{0x005a, 0x40},
	{0x00c3, 0x35},
	{0x00c4, 0x80},
	{0x0080, 0x10},
	{0x0040, 0x12},
	{0x0053, 0x0a},
	{0x0054, 0x44},
	{0x0055, 0x32},
	{0x004a, 0x03},
	{0x0048, 0xf0},
	{0x0049, 0x0f},
	{0x0041, 0x20},
	{0x0043, 0x0a},
	{0x009d, 0x08},
	/*gain*/
	{0x0204, 0x04},
	{0x0205, 0x00},
	{0x02b3, 0x00},
	{0x02b4, 0x00},
	{0x009e, 0x01},
	{0x009f, 0x94},
	/*OUT 2592x1944*/
	{0x0350, 0x01},
	{0x0353, 0x00},
	{0x0354, 0x08},
	{0x034c, 0x0a},
	{0x034d, 0x20},
	{0x021f, 0x14},
	/*auto load REG*/
	{0x0aa1, 0x10},
	{0x0aa2, 0xf8},
	{0x0aa3, 0x00},
	{0x0aa4, 0x1f},
	{0x0a90, 0x11},
	{0x0a91, 0x0e},
	{0x0a94, 0x80},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x0a90, 0x00},
	{0x0a70, 0x00},
	{0x0a67, 0x00},
	{0x0af4, 0x29},
	/*DPHY*/
	{0x0d80, 0x07},
	{0x0dd3, 0x18},
	/*MIPI*/
	{0x0107, 0x05},
	{0x0117, 0x01},
	{0x0d81, 0x00},
	/*CISCTL_Reset*/
	{0x031c, 0x80},
	{0x03fe, 0x30},
	{0x0d17, 0x06},
	{0x03fe, 0x00},
	{0x0d17, 0x00},
	{0x031c, 0x93},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x031c, 0x80},
	{0x03fe, 0x30},
	{0x0d17, 0x06},
	{0x03fe, 0x00},
	{0x0d17, 0x00},
	{0x031c, 0x93},

	{0x0315, 0xd4},
	{0x0d06, 0x01},
	{0x0a70, 0x80},
	{0x031a, 0x00},
	{0x0314, 0x00},
	{0x0130, 0x08},
	{0x0132, 0x01},
	{0x0135, 0x01},
	{0x0136, 0x38},
	{0x0137, 0x03},
	{0x0134, 0x5b},
	{0x031c, 0xe0},
	{0x0d82, 0x14},
	{0x0dd1, 0x56},
	/*gate_mode*/
	{0x0af4, 0x01},
	{0x0002, 0x10},
	{0x00c3, 0x34},
	/*pre_setting*/
	{0x0084, 0x21},
	{0x0d05, 0xcc},
	{0x0218, 0x00},
	{0x005e, 0x48},
	{0x0d06, 0x01},
	{0x0007, 0x16},
	/*analog*/
	{0x0342, 0x07},
	{0x0343, 0x28},
	{0x0220, 0x07},
	{0x0221, 0xd0},
	{0x0202, 0x07},
	{0x0203, 0x32},
	{0x0340, 0x07},
	{0x0341, 0xf0},
	{0x0346, 0x00},
	{0x0347, 0x04},
	{0x0d14, 0x00},
	{0x0d13, 0x05},
	{0x0d16, 0x05},
	{0x0d15, 0x1d},
	{0x00c0, 0x0a},
	{0x00c1, 0x30},
	{0x034a, 0x07},
	{0x034b, 0xa8},
	{0x000b, 0x10},
	{0x0008, 0x08},
	{0x0223, 0x17},
	/*auto load DD*/
	{0x00ca, 0x00},
	{0x00cb, 0x00},
	{0x00cc, 0x00},
	{0x00cd, 0x00},
	/*ISP*/
	{0x00c3, 0x35},
	{0x0053, 0x0a},
	{0x0054, 0x44},
	{0x0055, 0x32},
	/*OUT 2592x1944*/
	{0x0350, 0x01},
	{0x0353, 0x00},
	{0x0354, 0x08},
	{0x034c, 0x0a},
	{0x034d, 0x20},
	{0x021f, 0x14},
	/*MIPI*/
	{0x0d84, 0x0c},
	{0x0d85, 0xa8},
	{0x0d86, 0x06},
	{0x0d87, 0x55},
	{0x0db3, 0x06},
	{0x0db4, 0x08},
	{0x0db5, 0x1e},
	{0x0db6, 0x02},
	{0x0db8, 0x12},
	{0x0db9, 0x0a},
	{0x0d93, 0x06},
	{0x0d94, 0x09},
	{0x0d95, 0x0d},
	{0x0d99, 0x0b},
	{0x0084, 0x01},
	/* CISCTL_Reset*/
	{0x031c, 0x80},
	{0x03fe, 0x30},
	{0x0d17, 0x06},
	{0x03fe, 0x00},
	{0x0d17, 0x00},
	{0x031c, 0x93},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x031c, 0x80},
	{0x03fe, 0x30},
	{0x0d17, 0x06},
	{0x03fe, 0x00},
	{0x0d17, 0x00},
	{0x031c, 0x93},
	/*OUT*/
	{0x0110, 0x01},
	{REG_NULL, 0x00},
};

/*
 * Xclk 24Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 876Mbps
 */
static const struct regval gc05a2_2592x1944_regs[] = {
	/* lane snap */
	{REG_NULL, 0x00},
};

static const struct gc05a2_mode supported_modes_2lane[] = {
	{
		.width = 2592,
		.height = 1944,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x07C0,
		.hts_def = 0x728 * 3,
		.vts_def = 0x7F8,
		.reg_list = gc05a2_2592x1944_regs,
		.global_reg_list = gc05a2_global_regs,
		.mipi_freq_idx = 0,
		.vc[PAD0] = 0,
	},
};

static const s64 link_freq_menu_items[] = {
	GC05A2_MIPI_FREQ_451MHZ,
};

static int gc05a2_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		return 0;

	dev_err(&client->dev,
		"gc05a2 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int gc05a2_write_array(struct i2c_client *client,
	const struct regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = gc05a2_write_reg(client, regs[i].addr, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int gc05a2_read_reg(struct i2c_client *client, u16 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

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
		"gc05a2 read reg:0x%x failed !\n", reg);

	return ret;
}

static int gc05a2_get_reso_dist(const struct gc05a2_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		abs(mode->height - framefmt->height);
}

static const struct gc05a2_mode *
gc05a2_find_best_fit(struct gc05a2 *gc05a2,
		     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < gc05a2->cfg_num; i++) {
		dist = gc05a2_get_reso_dist(&gc05a2->support_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &gc05a2->support_modes[cur_best_fit];
}

static int gc05a2_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_format *fmt)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);
	const struct gc05a2_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&gc05a2->mutex);

	mode = gc05a2_find_best_fit(gc05a2, fmt);
	fmt->format.code = GC05A2_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gc05a2->mutex);
		return -ENOTTY;
#endif
	} else {
		gc05a2->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc05a2->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(gc05a2->vblank, vblank_def,
					 GC05A2_VTS_MAX - mode->height,
					 1, vblank_def);
		__v4l2_ctrl_s_ctrl(gc05a2->link_freq,
				   mode->mipi_freq_idx);
	}
	dev_info(&gc05a2->client->dev, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	mutex_unlock(&gc05a2->mutex);

	return 0;
}

static int gc05a2_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_format *fmt)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);
	const struct gc05a2_mode *mode = gc05a2->cur_mode;

	mutex_lock(&gc05a2->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gc05a2->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = GC05A2_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&gc05a2->mutex);

	return 0;
}

static int gc05a2_enum_mbus_code(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = GC05A2_MEDIA_BUS_FMT;

	return 0;
}

static int gc05a2_enum_frame_sizes(struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);

	if (fse->index >= gc05a2->cfg_num)
		return -EINVAL;

	if (fse->code != GC05A2_MEDIA_BUS_FMT)
		return -EINVAL;

	fse->min_width  = gc05a2->support_modes[fse->index].width;
	fse->max_width  = gc05a2->support_modes[fse->index].width;
	fse->max_height = gc05a2->support_modes[fse->index].height;
	fse->min_height = gc05a2->support_modes[fse->index].height;

	return 0;
}

static int gc05a2_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *fi)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);
	const struct gc05a2_mode *mode = gc05a2->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static void gc05a2_get_module_inf(struct gc05a2 *gc05a2,
				struct rkmodule_inf *inf)
{
	strscpy(inf->base.sensor,
		GC05A2_NAME,
		sizeof(inf->base.sensor));
	strscpy(inf->base.module,
		gc05a2->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens,
		gc05a2->len_name,
		sizeof(inf->base.lens));
}

static void gc05a2_set_module_inf(struct gc05a2 *gc05a2,
				struct rkmodule_awb_cfg *cfg)
{
	mutex_lock(&gc05a2->mutex);
	memcpy(&gc05a2->awb_cfg, cfg, sizeof(*cfg));
	mutex_unlock(&gc05a2->mutex);
}

static int gc05a2_get_channel_info(struct gc05a2 *gc05a2, struct rkmodule_channel_info *ch_info)
{
	if (ch_info->index < PAD0 || ch_info->index >= PAD_MAX)
		return -EINVAL;
	ch_info->vc = gc05a2->cur_mode->vc[ch_info->index];
	ch_info->width = gc05a2->cur_mode->width;
	ch_info->height = gc05a2->cur_mode->height;
	ch_info->bus_fmt = GC05A2_MEDIA_BUS_FMT;
	return 0;
}

static long gc05a2_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gc05a2_get_module_inf(gc05a2, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_AWB_CFG:
		gc05a2_set_module_inf(gc05a2, (struct rkmodule_awb_cfg *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream) {
			ret |= gc05a2_write_reg(gc05a2->client,
						GC05A2_REG_CTRL_MODE,
						GC05A2_MODE_STREAMING);
		} else {
			ret |= gc05a2_write_reg(gc05a2->client,
						GC05A2_REG_CTRL_MODE,
						GC05A2_MODE_SW_STANDBY);
		}
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = (struct rkmodule_channel_info *)arg;
		ret = gc05a2_get_channel_info(gc05a2, ch_info);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gc05a2_compat_ioctl32(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	long ret = 0;
	u32 stream = 0;
	struct rkmodule_channel_info *ch_info;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc05a2_ioctl(sd, cmd, inf);
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
			ret = gc05a2_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = gc05a2_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CHANNEL_INFO:
		ch_info = kzalloc(sizeof(*ch_info), GFP_KERNEL);
		if (!ch_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc05a2_ioctl(sd, cmd, ch_info);
		if (!ret) {
			ret = copy_to_user(up, ch_info, sizeof(*ch_info));
			if (ret)
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

static int __gc05a2_start_stream(struct gc05a2 *gc05a2)
{
	int ret;

	ret = gc05a2_write_array(gc05a2->client, gc05a2->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&gc05a2->mutex);
	ret = v4l2_ctrl_handler_setup(&gc05a2->ctrl_handler);
	mutex_lock(&gc05a2->mutex);
	ret |= gc05a2_write_reg(gc05a2->client,
		GC05A2_REG_CTRL_MODE,
		GC05A2_MODE_STREAMING);
	return ret;
}

static int __gc05a2_stop_stream(struct gc05a2 *gc05a2)
{
	int ret;

	ret = gc05a2_write_reg(gc05a2->client,
		GC05A2_REG_CTRL_MODE,
		GC05A2_MODE_SW_STANDBY);

	return ret;
}

static int gc05a2_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);
	struct i2c_client *client = gc05a2->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				gc05a2->cur_mode->width,
				gc05a2->cur_mode->height,
		DIV_ROUND_CLOSEST(gc05a2->cur_mode->max_fps.denominator,
		gc05a2->cur_mode->max_fps.numerator));

	mutex_lock(&gc05a2->mutex);
	on = !!on;
	if (on == gc05a2->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gc05a2_start_stream(gc05a2);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gc05a2_stop_stream(gc05a2);
		pm_runtime_put(&client->dev);
	}

	gc05a2->streaming = on;

unlock_and_return:
	mutex_unlock(&gc05a2->mutex);

	return ret;
}

static int gc05a2_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);
	struct i2c_client *client = gc05a2->client;
	int ret = 0;

	dev_info(&client->dev, "%s(%d) on(%d)\n", __func__, __LINE__, on);
	mutex_lock(&gc05a2->mutex);

	/* If the power state is not modified - no work to do. */
	if (gc05a2->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gc05a2->flip_mirror = 0;
		ret = gc05a2_write_array(gc05a2->client, gc05a2->cur_mode->global_reg_list);
		if (ret) {
			v4l2_err(sd, "could not set init registers\n");
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gc05a2->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		gc05a2->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc05a2->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 gc05a2_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, GC05A2_XVCLK_FREQ / 1000 / 1000);
}

static int __gc05a2_power_on(struct gc05a2 *gc05a2)
{
	int ret;
	u32 delay_us;
	struct device *dev = &gc05a2->client->dev;

	if (!IS_ERR(gc05a2->power_gpio))
		gpiod_set_value_cansleep(gc05a2->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(gc05a2->pins_default)) {
		ret = pinctrl_select_state(gc05a2->pinctrl,
					   gc05a2->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(gc05a2->xvclk, GC05A2_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(gc05a2->xvclk) != GC05A2_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(gc05a2->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	ret = regulator_bulk_enable(GC05A2_NUM_SUPPLIES, gc05a2->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}
	usleep_range(2000, 2100);

	if (!IS_ERR(gc05a2->reset_gpio))
		gpiod_set_value_cansleep(gc05a2->reset_gpio, 0);

	usleep_range(1000, 1100);
	if (!IS_ERR(gc05a2->reset_gpio))
		gpiod_set_value_cansleep(gc05a2->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(gc05a2->pwdn_gpio))
		gpiod_set_value_cansleep(gc05a2->pwdn_gpio, 1);

	/* 8192 cycles prior to first SCCB transaction */
	delay_us = gc05a2_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(gc05a2->xvclk);

	return ret;
}

static void __gc05a2_power_off(struct gc05a2 *gc05a2)
{
	int ret;

	if (!IS_ERR(gc05a2->pwdn_gpio))
		gpiod_set_value_cansleep(gc05a2->pwdn_gpio, 0);
	clk_disable_unprepare(gc05a2->xvclk);
	if (!IS_ERR(gc05a2->reset_gpio))
		gpiod_set_value_cansleep(gc05a2->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(gc05a2->pins_sleep)) {
		ret = pinctrl_select_state(gc05a2->pinctrl,
					   gc05a2->pins_sleep);
		if (ret < 0)
			dev_dbg(&gc05a2->client->dev, "could not set pins\n");
	}
	if (!IS_ERR(gc05a2->power_gpio))
		gpiod_set_value_cansleep(gc05a2->power_gpio, 0);

	regulator_bulk_disable(GC05A2_NUM_SUPPLIES, gc05a2->supplies);
}

static int gc05a2_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc05a2 *gc05a2 = to_gc05a2(sd);

	return __gc05a2_power_on(gc05a2);
}

static int gc05a2_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc05a2 *gc05a2 = to_gc05a2(sd);

	__gc05a2_power_off(gc05a2);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc05a2_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);
	struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gc05a2_mode *def_mode = &gc05a2->support_modes[0];

	mutex_lock(&gc05a2->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = GC05A2_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&gc05a2->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int gc05a2_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	struct gc05a2 *gc05a2 = to_gc05a2(sd);

	if (fie->index >= gc05a2->cfg_num)
		return -EINVAL;

	fie->code = GC05A2_MEDIA_BUS_FMT;
	fie->width = gc05a2->support_modes[fie->index].width;
	fie->height = gc05a2->support_modes[fie->index].height;
	fie->interval = gc05a2->support_modes[fie->index].max_fps;
	return 0;
}

static int gc05a2_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct gc05a2 *sensor = to_gc05a2(sd);
	struct device *dev = &sensor->client->dev;

	dev_info(dev, "%s(%d) enter!\n", __func__, __LINE__);

	if (2 == sensor->lane_num) {
		config->type = V4L2_MBUS_CSI2_DPHY;
		config->bus.mipi_csi2.num_data_lanes = sensor->lane_num;
	} else {
		dev_err(&sensor->client->dev,
			"unsupported lane_num(%d)\n", sensor->lane_num);
	}
	return 0;
}

static const struct dev_pm_ops gc05a2_pm_ops = {
	SET_RUNTIME_PM_OPS(gc05a2_runtime_suspend,
			gc05a2_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gc05a2_internal_ops = {
	.open = gc05a2_open,
};
#endif

static const struct v4l2_subdev_core_ops gc05a2_core_ops = {
	.s_power = gc05a2_s_power,
	.ioctl = gc05a2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc05a2_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gc05a2_video_ops = {
	.s_stream = gc05a2_s_stream,
	.g_frame_interval = gc05a2_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc05a2_pad_ops = {
	.enum_mbus_code = gc05a2_enum_mbus_code,
	.enum_frame_size = gc05a2_enum_frame_sizes,
	.enum_frame_interval = gc05a2_enum_frame_interval,
	.get_fmt = gc05a2_get_fmt,
	.set_fmt = gc05a2_set_fmt,
	.get_mbus_config = gc05a2_g_mbus_config,
};

static const struct v4l2_subdev_ops gc05a2_subdev_ops = {
	.core	= &gc05a2_core_ops,
	.video	= &gc05a2_video_ops,
	.pad	= &gc05a2_pad_ops,
};

static int gc05a2_set_exposure_reg(struct gc05a2 *gc05a2, u32 exposure)
{
	int ret = 0;
	u32 cal_shutter = 0;

	cal_shutter = exposure >> 1;
	cal_shutter = cal_shutter << 1;

	ret |= gc05a2_write_reg(gc05a2->client,
		GC05A2_REG_EXPOSURE_H,
		GC05A2_FETCH_HIGH_BYTE(cal_shutter));
	ret |= gc05a2_write_reg(gc05a2->client,
		GC05A2_REG_EXPOSURE_L,
		GC05A2_FETCH_LOW_BYTE(cal_shutter));
	return ret;
}

static int gc05a2_set_gain_reg(struct gc05a2 *gc05a2, u32 a_gain)
{
	int ret = 0;
	u32 temp_gain;

	if (a_gain < GC05A2_AGAIN_MIN)
		temp_gain = GC05A2_AGAIN_MIN;
	else if (a_gain > GC05A2_AGAIN_MAX)
		temp_gain = GC05A2_AGAIN_MAX;
	else
		temp_gain = a_gain;

	ret |= gc05a2_write_reg(gc05a2->client,
		GC05A2_REG_GAIN_H,
		GC05A2_FETCH_HIGH_BYTE(temp_gain));
	/* gain effect when 0x0205 is written */
	ret |= gc05a2_write_reg(gc05a2->client,
		GC05A2_REG_GAIN_L,
		GC05A2_FETCH_LOW_BYTE(temp_gain));

	return ret;
}

static int gc05a2_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc05a2 *gc05a2 = container_of(ctrl->handler,
					struct gc05a2, ctrl_handler);
	struct i2c_client *client = gc05a2->client;
	s64 max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = gc05a2->cur_mode->height + ctrl->val - 16;
		__v4l2_ctrl_modify_range(gc05a2->exposure,
					 gc05a2->exposure->minimum, max,
					 gc05a2->exposure->step,
					 gc05a2->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		dev_dbg(&client->dev, "set exposure value 0x%x\n", ctrl->val);
		ret = gc05a2_set_exposure_reg(gc05a2, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set analog gain value 0x%x\n", ctrl->val);
		ret = gc05a2_set_gain_reg(gc05a2, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vb value 0x%x\n", ctrl->val);
		ret = gc05a2_write_reg(gc05a2->client,
					GC05A2_REG_VTS_H,
					(ctrl->val + gc05a2->cur_mode->height)
					>> 8);
		ret |= gc05a2_write_reg(gc05a2->client,
					GC05A2_REG_VTS_L,
					(ctrl->val + gc05a2->cur_mode->height)
					& 0xff);
		break;
	case V4L2_CID_HFLIP:
		dev_dbg(&client->dev, "set mirror value 0x%x\n", ctrl->val);

		if (ctrl->val)
			gc05a2->flip_mirror |= MIRROR_BIT_MASK;
		else
			gc05a2->flip_mirror &= ~MIRROR_BIT_MASK;

		ret |= gc05a2_write_reg(gc05a2->client, GC05A2_FLIP_MIRROR_REG,
					 gc05a2->flip_mirror);
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(&client->dev, "set flip value 0x%x\n", ctrl->val);
		if (ctrl->val)
			gc05a2->flip_mirror |= FLIP_BIT_MASK;
		else
			gc05a2->flip_mirror &= ~FLIP_BIT_MASK;

		ret |= gc05a2_write_reg(gc05a2->client, GC05A2_FLIP_MIRROR_REG,
					 gc05a2->flip_mirror);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gc05a2_ctrl_ops = {
	.s_ctrl = gc05a2_set_ctrl,
};

static int gc05a2_initialize_controls(struct gc05a2 *gc05a2)
{
	const struct gc05a2_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &gc05a2->ctrl_handler;
	mode = gc05a2->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &gc05a2->mutex;

	gc05a2->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ, 2, 0,
				link_freq_menu_items);

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
			0, gc05a2->pixel_rate, 1, gc05a2->pixel_rate);

	__v4l2_ctrl_s_ctrl(gc05a2->link_freq,
			   mode->mipi_freq_idx);

	h_blank = mode->hts_def - mode->width;
	gc05a2->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (gc05a2->hblank)
		gc05a2->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	gc05a2->vblank = v4l2_ctrl_new_std(handler, &gc05a2_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				GC05A2_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 4;
	gc05a2->exposure = v4l2_ctrl_new_std(handler, &gc05a2_ctrl_ops,
				V4L2_CID_EXPOSURE, GC05A2_EXPOSURE_MIN,
				exposure_max, GC05A2_EXPOSURE_STEP,
				mode->exp_def);

	gc05a2->anal_gain = v4l2_ctrl_new_std(handler, &gc05a2_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, GC05A2_AGAIN_MIN,
				GC05A2_AGAIN_MAX, GC05A2_AGAIN_STEP,
				GC05A2_AGAIN_DEFAULT);
	gc05a2->h_flip = v4l2_ctrl_new_std(handler, &gc05a2_ctrl_ops,
					   V4L2_CID_HFLIP, 0, 1, 1, 0);

	gc05a2->v_flip = v4l2_ctrl_new_std(handler, &gc05a2_ctrl_ops,
					   V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (handler->error) {
		ret = handler->error;
		dev_err(&gc05a2->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc05a2->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gc05a2_check_sensor_id(struct gc05a2 *gc05a2,
				struct i2c_client *client)
{
	struct device *dev = &gc05a2->client->dev;
	u32 id = 0;
	u8 reg_H = 0;
	u8 reg_L = 0;
	int ret;

	ret = gc05a2_read_reg(client, GC05A2_REG_CHIP_ID_H, &reg_H);
	ret |= gc05a2_read_reg(client, GC05A2_REG_CHIP_ID_L, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected gc%04x sensor\n", id);
	return ret;
}

static int gc05a2_configure_regulators(struct gc05a2 *gc05a2)
{
	unsigned int i;

	for (i = 0; i < GC05A2_NUM_SUPPLIES; i++)
		gc05a2->supplies[i].supply = gc05a2_supply_names[i];

	return devm_regulator_bulk_get(&gc05a2->client->dev,
		GC05A2_NUM_SUPPLIES,
		gc05a2->supplies);
}

static int gc05a2_parse_of(struct gc05a2 *gc05a2)
{
	struct device *dev = &gc05a2->client->dev;
	struct device_node *endpoint;
	struct fwnode_handle *fwnode;
	int rval;
	unsigned int fps;

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

	gc05a2->lane_num = rval;
	if (2 == gc05a2->lane_num) {
		gc05a2->cur_mode = &supported_modes_2lane[0];
		gc05a2->support_modes = supported_modes_2lane;
		gc05a2->cfg_num = ARRAY_SIZE(supported_modes_2lane);
		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		fps = DIV_ROUND_CLOSEST(gc05a2->cur_mode->max_fps.denominator,
					gc05a2->cur_mode->max_fps.numerator);
		gc05a2->pixel_rate = gc05a2->cur_mode->vts_def *
				     gc05a2->cur_mode->hts_def * fps;

		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
			 gc05a2->lane_num, gc05a2->pixel_rate);
	} else {
		/* TODO*/
		dev_err(dev, "unsupported lane_num(%d)\n", gc05a2->lane_num);
		return -1;
	}

	return 0;
}

static int gc05a2_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gc05a2 *gc05a2;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	gc05a2 = devm_kzalloc(dev, sizeof(*gc05a2), GFP_KERNEL);
	if (!gc05a2)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
		&gc05a2->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
		&gc05a2->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
		&gc05a2->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
		&gc05a2->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}
	gc05a2->client = client;

	gc05a2->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(gc05a2->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	gc05a2->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(gc05a2->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");
	gc05a2->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(gc05a2->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	gc05a2->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(gc05a2->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = gc05a2_configure_regulators(gc05a2);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ret = gc05a2_parse_of(gc05a2);
	if (ret != 0)
		return -EINVAL;

	gc05a2->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(gc05a2->pinctrl)) {
		gc05a2->pins_default =
			pinctrl_lookup_state(gc05a2->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(gc05a2->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		gc05a2->pins_sleep =
			pinctrl_lookup_state(gc05a2->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(gc05a2->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&gc05a2->mutex);

	sd = &gc05a2->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc05a2_subdev_ops);
	ret = gc05a2_initialize_controls(gc05a2);
	if (ret)
		goto err_destroy_mutex;

	ret = __gc05a2_power_on(gc05a2);
	if (ret)
		goto err_free_handler;

	ret = gc05a2_check_sensor_id(gc05a2, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc05a2_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gc05a2->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc05a2->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc05a2->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc05a2->module_index, facing,
		 GC05A2_NAME, dev_name(sd->dev));
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
	__gc05a2_power_off(gc05a2);
err_free_handler:
	v4l2_ctrl_handler_free(&gc05a2->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc05a2->mutex);

	return ret;
}

static void gc05a2_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc05a2 *gc05a2 = to_gc05a2(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gc05a2->ctrl_handler);
	mutex_destroy(&gc05a2->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc05a2_power_off(gc05a2);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc05a2_of_match[] = {
	{ .compatible = "galaxycore,gc05a2" },
	{},
};
MODULE_DEVICE_TABLE(of, gc05a2_of_match);
#endif

static const struct i2c_device_id gc05a2_match_id[] = {
	{ "galaxycore,gc05a2", 0},
	{ },
};

static struct i2c_driver gc05a2_i2c_driver = {
	.driver = {
		.name = GC05A2_NAME,
		.pm = &gc05a2_pm_ops,
		.of_match_table = of_match_ptr(gc05a2_of_match),
	},
	.probe		= &gc05a2_probe,
	.remove		= &gc05a2_remove,
	.id_table	= gc05a2_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc05a2_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc05a2_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("GalaxyCore gc05a2 sensor driver");
MODULE_LICENSE("GPL");
