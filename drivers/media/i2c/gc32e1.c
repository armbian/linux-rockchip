// SPDX-License-Identifier: GPL-2.0
/*
 * gc32e1 driver
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

#define GC32E1_LANES			4
#define GC32E1_BITS_PER_SAMPLE		10

#define GC32E1_LINK_FREQ_MHZ_6K		(1339200000LL/2)
#define GC32E1_LINK_FREQ_MHZ_3K		(763200000LL/2)


//mipi speed = GC32E1_LINK_FREQ_MHZ * 2LL
/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define GC32E1_PIXEL_RATE		(GC32E1_LINK_FREQ_MHZ_6K * 2LL * GC32E1_LANES / GC32E1_BITS_PER_SAMPLE)
#define GC32E1_XVCLK_FREQ		24000000

#define CHIP_ID				0x32E1
#define GC32E1_REG_CHIP_ID_H		0x03f0
#define GC32E1_REG_CHIP_ID_L		0x03f1

#define GC32E1_REG_CTRL_MODE		0x0102//MIPI enable
#define GC32E1_MODE_SW_STANDBY		0x00 //close lane_en && mipi_en
#define GC32E1_MODE_STREAMING		0x99

#define GC32E1_REG_EXPOSURE_H		0x0202
#define GC32E1_REG_EXPOSURE_L		0x0203
#define	GC32E1_EXPOSURE_MIN		4
#define	GC32E1_EXPOSURE_STEP		1
#define GC32E1_VTS_MAX			0x1fff

#define GC32E1_REG_AGAIN_H		0x0204
#define GC32E1_REG_AGAIN_L		0x0205

#define GC32E1_GAIN_MIN			1024
#define GC32E1_GAIN_MAX			(1024 * 16)
#define GC32E1_GAIN_STEP		1
#define GC32E1_GAIN_DEFAULT		1024

#define GC32E1_REG_VTS_H		0x0340
#define GC32E1_REG_VTS_L		0x0341

#define REG_NULL			0xFFFF

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define GC32E1_NAME			"gc32e1"

static const char * const gc32e1_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define GC32E1_NUM_SUPPLIES ARRAY_SIZE(gc32e1_supply_names)

#define IMAGE_NORMAL_MIRROR
#define DD_PARAM_QTY_5035		200
#define INFO_ROM_START_5035		0x08
#define INFO_WIDTH_5035			0x08
#define WB_ROM_START_5035		0x88
#define WB_WIDTH_5035			0x05
#define GOLDEN_ROM_START_5035		0xe0
#define GOLDEN_WIDTH_5035		0x05
#define WINDOW_WIDTH			0x0a30
#define WINDOW_HEIGHT			0x079c

/* SENSOR MIRROR FLIP INFO */
#define GC32E1_MIRROR_FLIP_ENABLE	0
#if GC32E1_MIRROR_FLIP_ENABLE
#define GC32E1_MIRROR			0x83
#define GC32E1_RSTDUMMY1		0x03
#define GC32E1_RSTDUMMY2		0xfc
#else
#define GC32E1_MIRROR			0x80
#define GC32E1_RSTDUMMY1		0x02
#define GC32E1_RSTDUMMY2		0x7c
#endif

struct gc32e1_otp_info {
	u32 flag; //bit[7]: info bit[6]:wb bit[3]:dd
	u32 module_id;
	u32 lens_id;
	u16 vcm_id;
	u16 vcm_driver_id;
	u32 year;
	u32 month;
	u32 day;
	u32 rg_ratio;
	u32 bg_ratio;
	u32 golden_rg;
	u32 golden_bg;
	u16 dd_param_x[DD_PARAM_QTY_5035];
	u16 dd_param_y[DD_PARAM_QTY_5035];
	u16 dd_param_type[DD_PARAM_QTY_5035];
	u16 dd_cnt;
};

struct gc32e1_id_name {
	u32 id;
	char name[RKMODULE_NAME_LEN];
};

struct regval {
	u16 addr;
	u8 val;
};

struct gc32e1_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
	const struct regval *reg_list;
};

struct gc32e1 {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[GC32E1_NUM_SUPPLIES];

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
	const struct gc32e1_mode *cur_mode;
	unsigned int		lane_num;
	unsigned int		cfg_num;
	unsigned int		pixel_rate;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			Dgain_ratio;
	struct gc32e1_otp_info *otp;
	struct rkmodule_inf	module_inf;
	struct rkmodule_awb_cfg	awb_cfg;
};

#define to_gc32e1(sd) container_of(sd, struct gc32e1, subdev)

/*
 * Xclk 24Mhz
 */
static const struct regval gc32e1_global_3264_2448_30fps_regs[] = {
	{0x031c, 0x60},
	{0x0315, 0xd3},
	{0x0c10, 0x1b},
	{0x01a7, 0x02},
	{0x01aa, 0x05},
	{0x01a8, 0x02},
	{0x01a9, 0x35},
	{0x0c0d, 0xb4},
	{0x0c0c, 0x48},
	{0x0185, 0xc0},
	{0x0314, 0x11},
	{0x031a, 0x00},
	{0x01a1, 0x10},
	{0x0c0e, 0x21},
	{0x01e3, 0x03},

	{0x0057, 0x03},
	{0x00a6, 0x06},
	{0x00d3, 0x30},
	{0x0311, 0xb0},
	{0x05a0, 0x0a},

	{0x0059, 0x11},
	{0x0084, 0x30},
	{0x0087, 0x51},
	{0x0101, 0x00},
	{0x01e2, 0x24},
	{0x01ea, 0x24},

	{0x0202, 0x09},
	{0x0203, 0x78},

	{0x0210, 0xa3},
	{0x0218, 0x03},
	{0x0219, 0x05},

	{0x0226, 0x14},
	{0x0227, 0x80},
	{0x0340, 0x0a},
	{0x0341, 0x60},
	{0x0342, 0x08},
	{0x0343, 0x5a},

	{0x0346, 0x00},
	{0x0347, 0x20},
	{0x034a, 0x13},
	{0x034b, 0x40},
	{0x0c08, 0x19},
	{0x0c25, 0x14},
	{0x0c55, 0x14},

	{0x013e, 0x34},
	{0x025b, 0x00},
	{0x025c, 0x40},
	{0x02c0, 0x55},
	{0x02c1, 0x71},
	{0x02c2, 0x07},
	{0x02c3, 0x1c},
	{0x0c05, 0xff},
	{0x0c07, 0x14},
	{0x0c41, 0x0a},
	{0x0c42, 0x04},
	{0x0c44, 0x00},
	{0x0c45, 0xdf},
	{0x0c46, 0xfd},
	{0x0c47, 0x7f},
	{0x0c48, 0x58},
	{0x0c4a, 0x18},
	{0x0e01, 0x42},
	{0x0e15, 0x58},
	{0x0e28, 0x5a},
	{0x0e33, 0x00},
	{0x0e34, 0x04},
	{0x0e47, 0x02},
	{0x0e61, 0x20},
	{0x0e62, 0x20},
	{0x0e65, 0x01},
	{0x0e66, 0x20},
	{0x0e67, 0x2f},
	{0x0e68, 0x2f},
	{0x0e6a, 0x54},
	{0x0e6b, 0x06},
	{0x0e6c, 0x01},
	{0x0e6d, 0x01},
	{0x0e6e, 0x42},
	{0x0e6f, 0x44},
	{0x0e70, 0x2a},
	{0x0e71, 0x2c},
	{0x0e74, 0x45},

	{0x03a2, 0x00},
	{0x0316, 0x01},
	{0x0a67, 0x80},
	{0x0313, 0x00},
	{0x0a53, 0x04},
	{0x0a65, 0x17},
	{0x0a68, 0x33},
	{0x0a58, 0x00},
	{0x0a4f, 0x00},
	{0x0a66, 0x00},
	{0x0a7f, 0x07},
	{0x0a84, 0x0c},

	{0x00a4, 0x00},
	{0x00a5, 0x01},
	{0x00a2, 0x00},
	{0x00a3, 0x00},
	{0x00ab, 0x00},
	{0x00ac, 0x00},
	{0x00a7, 0x09},
	{0x00a8, 0xa0},
	{0x00a9, 0x0c},
	{0x00aa, 0xd0},
	{0x0aaa, 0x01},
	{0x0aab, 0x60},
	{0x0aac, 0x29},
	{0x0aad, 0xe0},
	{0x0ab0, 0x0f},
	{0x0ab1, 0x26},
	{0x0ab2, 0xf8},
	{0x0a91, 0xf2},
	{0x0a92, 0x12},
	{0x0a93, 0x64},
	{0x0a95, 0x41},
	{0x0a90, 0x17},
	{0x0313, 0x80},

	{0x02db, 0x01},

	{0x0b00, 0xd3},
	{0x0b01, 0x15},
	{0x0b02, 0x03},
	{0x0b03, 0x00},

	{0x0b04, 0xb4},
	{0x0b05, 0x0d},
	{0x0b06, 0x0c},
	{0x0b07, 0x01},

	{0x0b08, 0x02},
	{0x0b09, 0xa7},
	{0x0b0a, 0x01},
	{0x0b0b, 0x00},

	{0x0b0c, 0x11},
	{0x0b0d, 0x14},
	{0x0b0e, 0x03},
	{0x0b0f, 0x00},

	{0x0b10, 0x1b},
	{0x0b11, 0x10},
	{0x0b12, 0x0c},
	{0x0b13, 0x01},

	{0x0b14, 0x46},
	{0x0b15, 0x80},
	{0x0b16, 0x01},
	{0x0b17, 0x00},

	{0x0b18, 0xf0},
	{0x0b19, 0x81},
	{0x0b1a, 0x01},
	{0x0b1b, 0x00},

	{0x0b1c, 0x55},
	{0x0b1d, 0x84},
	{0x0b1e, 0x01},
	{0x0b1f, 0x00},

	{0x0b20, 0x03},
	{0x0b21, 0xe3},
	{0x0b22, 0x01},
	{0x0b23, 0x00},

	{0x0b24, 0x40},
	{0x0b25, 0x64},
	{0x0b26, 0x02},
	{0x0b27, 0x00},

	{0x0b28, 0x12},
	{0x0b29, 0x1c},
	{0x0b2a, 0x03},
	{0x0b2b, 0x00},

	{0x0b2c, 0x80},
	{0x0b2d, 0x1c},
	{0x0b2e, 0x03},
	{0x0b2f, 0x00},

	{0x0b30, 0x10},
	{0x0b31, 0xfe},
	{0x0b32, 0x03},
	{0x0b33, 0x00},

	{0x0b34, 0x00},
	{0x0b35, 0xfe},
	{0x0b36, 0x03},
	{0x0b37, 0x00},

	{0x0b38, 0x9f},
	{0x0b39, 0x1c},
	{0x0b3a, 0x03},
	{0x0b3b, 0x00},

	{0x0b3c, 0x00},
	{0x0b3d, 0xfe},
	{0x0b3e, 0x03},
	{0x0b3f, 0x00},

	{0x0b40, 0x00},
	{0x0b41, 0xfe},
	{0x0b42, 0x03},
	{0x0b43, 0x00},

	{0x0b44, 0x00},
	{0x0b45, 0xfe},
	{0x0b46, 0x03},
	{0x0b47, 0x00},

	{0x0b48, 0x80},
	{0x0b49, 0x1c},
	{0x0b4a, 0x03},
	{0x0b4b, 0x00},

	{0x0b4c, 0x10},
	{0x0b4d, 0xfe},
	{0x0b4e, 0x03},
	{0x0b4f, 0x00},

	{0x0b50, 0x00},
	{0x0b51, 0xfe},
	{0x0b52, 0x03},
	{0x0b53, 0x00},

	{0x0b54, 0x9f},
	{0x0b55, 0x1c},
	{0x0b56, 0x03},
	{0x0b57, 0x00},

	{0x0b58, 0x99},
	{0x0b59, 0x02},
	{0x0b5a, 0x01},
	{0x0b5b, 0x00},

	{0x0b5c, 0x00},
	{0x0b5d, 0x64},
	{0x0b5e, 0x02},
	{0x0b5f, 0x00},

	{0x0b60, 0x00},
	{0x0b61, 0x02},
	{0x0b62, 0x01},
	{0x0b63, 0x00},

	{0x0b64, 0x06},
	{0x0b65, 0x80},
	{0x0b66, 0x01},
	{0x0b67, 0x00},

	{0x0b68, 0x00},
	{0x0b69, 0x81},
	{0x0b6a, 0x01},
	{0x0b6b, 0x00},

	{0x0b6c, 0x54},
	{0x0b6d, 0x84},
	{0x0b6e, 0x01},
	{0x0b6f, 0x00},

	{0x0b70, 0x60},
	{0x0b71, 0x1c},
	{0x0b72, 0x03},
	{0x0b73, 0x00},

	{0x0b74, 0x02},
	{0x0b75, 0xe3},
	{0x0b76, 0x01},
	{0x0b77, 0x00},

	{0x0b78, 0x13},
	{0x0b79, 0x10},
	{0x0b7a, 0x0c},
	{0x0b7b, 0x01},

	{0x0b7c, 0x01},
	{0x0b7d, 0x14},
	{0x0b7e, 0x03},
	{0x0b7f, 0x00},

	{0x0b80, 0x00},
	{0x0b81, 0xa7},
	{0x0b82, 0x01},
	{0x0b83, 0x00},

	{0x0b84, 0x34},
	{0x0b85, 0x0d},
	{0x0b86, 0x0c},
	{0x0b87, 0x01},

	{0x0b88, 0x53},
	{0x0b89, 0x15},
	{0x0b8a, 0x03},
	{0x0b8b, 0x01},

	{0x0aeb, 0x09},
	{0x0ae9, 0x17},
	{0x0aea, 0x23},
	{0x0ae8, 0xb0},

	{0x05a0, 0x82},
	{0x05ac, 0x00},
	{0x05ad, 0x01},

	{0x0597, 0x45},
	{0x05ab, 0x0a},
	{0x05a3, 0x06},
	{0x05a4, 0x08},
	{0x05ae, 0x00},

	{0x0800, 0x0a},
	{0x0801, 0x14},
	{0x0802, 0x22},
	{0x0803, 0x30},
	{0x0804, 0x42},

	{0x0805, 0x0e},
	{0x0806, 0x66},
	{0x0807, 0x0e},
	{0x0808, 0x65},
	{0x0809, 0x02},
	{0x080a, 0xc3},
	{0x080b, 0x02},
	{0x080c, 0xc7},
	{0x080d, 0x02},
	{0x080e, 0xcb},
	{0x080f, 0x0e},
	{0x0810, 0x6c},
	{0x0811, 0x0e},
	{0x0812, 0x6d},
	{0x0813, 0x00},
	{0x0814, 0xc0},

	{0x0815, 0x16},
	{0x0816, 0x01},
	{0x0817, 0x1c},
	{0x0818, 0x1c},
	{0x0819, 0x1c},
	{0x081a, 0x08},
	{0x081b, 0x08},
	{0x081c, 0x00},

	{0x081d, 0x08},
	{0x081e, 0x01},
	{0x081f, 0x3c},
	{0x0820, 0x3c},
	{0x0821, 0x3c},
	{0x0822, 0x08},
	{0x0823, 0x08},
	{0x0824, 0x00},

	{0x0825, 0x12},
	{0x0826, 0x01},
	{0x0827, 0x04},
	{0x0828, 0x04},
	{0x0829, 0x04},
	{0x082a, 0x12},
	{0x082b, 0x12},
	{0x082c, 0x00},

	{0x082d, 0x0e},
	{0x082e, 0x01},
	{0x082f, 0x04},
	{0x0830, 0x04},
	{0x0831, 0x04},
	{0x0832, 0x12},
	{0x0833, 0x12},
	{0x0834, 0x00},

	{0x0835, 0x0b},
	{0x0836, 0x01},
	{0x0837, 0x04},
	{0x0838, 0x04},
	{0x0839, 0x04},
	{0x083a, 0x01},
	{0x083b, 0x01},
	{0x083c, 0x02},

	{0x083d, 0x06},
	{0x083e, 0x01},
	{0x083f, 0x04},
	{0x0840, 0x04},
	{0x0841, 0x04},
	{0x0842, 0x01},
	{0x0843, 0x01},
	{0x0844, 0x02},

	{0x0845, 0x01},
	{0x0846, 0x00},
	{0x0847, 0x00},
	{0x0848, 0x00},

	{0x0849, 0x01},
	{0x084a, 0x68},
	{0x084b, 0x00},
	{0x084c, 0x01},

	{0x084d, 0x01},
	{0x084e, 0xf8},
	{0x084f, 0x00},
	{0x0850, 0x02},

	{0x0851, 0x02},
	{0x0852, 0xcc},
	{0x0853, 0x00},
	{0x0854, 0x03},

	{0x0855, 0x03},
	{0x0856, 0xe8},
	{0x0857, 0x00},
	{0x0858, 0x04},

	{0x0859, 0x05},
	{0x085a, 0x98},
	{0x085b, 0x00},
	{0x085c, 0x05},

	{0x085d, 0x07},
	{0x085e, 0xc8},
	{0x085f, 0x00},
	{0x0860, 0x06},

	{0x0861, 0x0b},
	{0x0862, 0x42},
	{0x0863, 0x10},
	{0x0864, 0x05},

	{0x0865, 0x0f},
	{0x0866, 0xa8},
	{0x0867, 0x10},
	{0x0868, 0x06},

	{0x0869, 0x1e},
	{0x086a, 0xd4},
	{0x086b, 0x18},
	{0x086c, 0x06},

	{0x05a0, 0xc2},
	{0x05ac, 0x01},
	{0x05ae, 0x00},
	{0x0207, 0x04},

	{0x0070, 0x05},
	{0x0080, 0xd0},
	{0x0089, 0x83},
	{0x009a, 0x00},
	{0x00a0, 0x03},

	{0x0c20, 0x10},
	{0x0c21, 0xc8},
	{0x0c22, 0xc8},
	{0x0c50, 0x10},
	{0x0c51, 0xc8},
	{0x0c52, 0xc8},

	{0x0040, 0x22},
	{0x0041, 0x20},
	{0x0042, 0x20},
	{0x0043, 0x0f},
	{0x0044, 0x00},
	{0x0046, 0x0c},
	{0x0049, 0x06},
	{0x004a, 0x19},
	{0x004d, 0x00},
	{0x004e, 0x03},
	{0x0051, 0x26},
	{0x005a, 0x0c},
	{0x005b, 0x03},
	{0x021a, 0x00},
	{0x0450, 0x02},
	{0x0452, 0x02},
	{0x0454, 0x02},
	{0x0456, 0x02},

	{0x0204, 0x04},
	{0x0205, 0x00},
	{0x0208, 0x01},
	{0x0209, 0x74},

	{0x0096, 0x81},
	{0x0097, 0x01},
	{0x0098, 0x87},

	{0x00c0, 0x00},
	{0x00c1, 0x80},
	{0x00c2, 0x11},
	{0x00c3, 0x00},

	{0x0480, 0x04},
	{0x0482, 0x06},
	{0x0484, 0x10},
	{0x0486, 0x10},
	{0x0488, 0x10},
	{0x048a, 0x0c},
	{0x048c, 0x10},
	{0x048e, 0x10},

	{0x0481, 0x03},
	{0x0483, 0x04},
	{0x0485, 0x05},
	{0x0487, 0x05},
	{0x0489, 0x05},
	{0x048b, 0x06},
	{0x048d, 0x06},
	{0x048f, 0x06},

	{0x0490, 0x04},
	{0x0492, 0x10},
	{0x0494, 0x18},
	{0x0496, 0x28},
	{0x0498, 0x2c},
	{0x049a, 0x30},
	{0x049c, 0x40},
	{0x049e, 0x40},

	{0x0491, 0x04},
	{0x0493, 0x05},
	{0x0495, 0x05},
	{0x0497, 0x05},
	{0x0499, 0x05},
	{0x049b, 0x05},
	{0x049d, 0x05},
	{0x049f, 0x05},

	{0x0351, 0x00},
	{0x0352, 0x08},
	{0x0353, 0x00},
	{0x0354, 0x08},
	{0x034c, 0x0c},
	{0x034d, 0xc0},
	{0x034e, 0x09},
	{0x034f, 0x90},

	{0x0180, 0x46},
	{0x0181, 0xf0},
	{0x0182, 0x55},
	{0x0183, 0x55},
	{0x0184, 0x55},
	{0x0186, 0x5f},
	{0x0187, 0x00},
	{0x0188, 0x00},
	{0x0189, 0x00},

	{0x0107, 0x00},
	{0x010b, 0x12},
	{0x0115, 0x00},
	{0x0121, 0x12},
	{0x0122, 0x07},
	{0x0123, 0x1f},
	{0x0124, 0x02},
	{0x0125, 0x16},
	{0x0126, 0x08},
	{0x0127, 0x10},
	{0x0129, 0x07},
	{0x012a, 0x1f},
	{0x012b, 0x08},
	{0x0084, 0x10},

	{0x0a93, 0x60},
	{0x0a90, 0x11},
	{0x0313, 0x80},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0a90, 0x00},
	{0x00a4, 0x80},
	{0x0316, 0x00},
	{0x0a67, 0x00},

	{0x031c, 0x12},
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

	{0x0102, 0x99},
	{0x0100, 0x01},

	{REG_NULL, 0x00},
};

static const struct regval gc32e1_global_6258_4896_15fps_regs[] = {
	{0x031c, 0x60},
	{0x0315, 0xd7},
	{0x0c10, 0x1b},
	{0x01a7, 0x02},
	{0x01aa, 0x05},
	{0x01a8, 0x02},
	{0x01a9, 0x5d},
	{0x0c0d, 0xb4},
	{0x0c0c, 0x48},
	{0x0185, 0xc0},
	{0x0314, 0x11},
	{0x031a, 0x00},
	{0x01a1, 0x10},
	{0x0c0e, 0x20},
	{0x01e3, 0x03},

	{0x0057, 0x03},
	{0x00a6, 0x06},
	{0x00d3, 0x30},
	{0x0311, 0xb0},
	{0x05a0, 0x0a},

	{0x0059, 0x11},
	{0x0084, 0x30},
	{0x0087, 0x51},
	{0x0101, 0x00},
	{0x01e2, 0x24},
	{0x01ea, 0x24},

	{0x0202, 0x08},
	{0x0203, 0xee},

	{0x0210, 0x03},
	{0x0218, 0x00},
	{0x0219, 0x05},

	{0x0226, 0x13},
	{0x0227, 0xb8},
	{0x0340, 0x13},
	{0x0341, 0xd8},
	{0x0342, 0x08},
	{0x0343, 0xdc},

	{0x0346, 0x00},
	{0x0347, 0x20},
	{0x034a, 0x13},
	{0x034b, 0x40},
	{0x0c08, 0x19},
	{0x0c25, 0x14},
	{0x0c55, 0x14},

	{0x013e, 0x34},
	{0x025b, 0x00},
	{0x025c, 0x40},
	{0x02c0, 0x55},
	{0x02c1, 0x71},
	{0x02c2, 0x03},
	{0x02c3, 0x38},
	{0x0c05, 0xff},
	{0x0c07, 0x14},
	{0x0c41, 0x0a},
	{0x0c42, 0x04},
	{0x0c44, 0x00},
	{0x0c45, 0xdf},
	{0x0c46, 0xfd},
	{0x0c47, 0x7c},
	{0x0c48, 0x58},
	{0x0c4a, 0x18},
	{0x0e01, 0x43},
	{0x0e15, 0x58},
	{0x0e28, 0xac},
	{0x0e33, 0x00},
	{0x0e34, 0x04},
	{0x0e47, 0x02},
	{0x0e61, 0x2c},
	{0x0e62, 0x2c},
	{0x0e65, 0x01},
	{0x0e66, 0x1c},
	{0x0e67, 0x60},
	{0x0e68, 0x60},
	{0x0e6a, 0xb4},
	{0x0e6b, 0x05},
	{0x0e6c, 0x01},
	{0x0e6d, 0x01},
	{0x0e6e, 0x7b},
	{0x0e6f, 0x7c},
	{0x0e70, 0x90},
	{0x0e71, 0x91},
	{0x0e74, 0x7d},

	{0x03a2, 0x00},
	{0x0316, 0x01},
	{0x0a67, 0x80},
	{0x0313, 0x00},
	{0x0a53, 0x04},
	{0x0a65, 0x17},
	{0x0a68, 0x33},
	{0x0a58, 0x00},
	{0x0a4f, 0x00},
	{0x0a66, 0x00},
	{0x0a7f, 0x07},
	{0x0a84, 0x0c},

	{0x00a4, 0x00},
	{0x00a5, 0x01},
	{0x00a2, 0x00},
	{0x00a3, 0x00},
	{0x00ab, 0x00},
	{0x00ac, 0x00},
	{0x00a7, 0x13},
	{0x00a8, 0x40},
	{0x00a9, 0x19},
	{0x00aa, 0xa0},
	{0x0aaa, 0x2a},
	{0x0aab, 0x78},
	{0x0aac, 0x29},
	{0x0aad, 0xe0},
	{0x0ab0, 0x0f},
	{0x0ab1, 0x26},
	{0x0ab2, 0xf8},
	{0x0a91, 0xf2},
	{0x0a92, 0x12},
	{0x0a93, 0x64},
	{0x0a95, 0x41},
	{0x0a90, 0x17},
	{0x0313, 0x80},

	{0x02db, 0x01},

	{0x0b00, 0xd7},
	{0x0b01, 0x15},
	{0x0b02, 0x03},
	{0x0b03, 0x00},

	{0x0b04, 0xb4},
	{0x0b05, 0x0d},
	{0x0b06, 0x0c},
	{0x0b07, 0x01},

	{0x0b08, 0x02},
	{0x0b09, 0xa7},
	{0x0b0a, 0x01},
	{0x0b0b, 0x00},

	{0x0b0c, 0x11},
	{0x0b0d, 0x14},
	{0x0b0e, 0x03},
	{0x0b0f, 0x00},

	{0x0b10, 0x1b},
	{0x0b11, 0x10},
	{0x0b12, 0x0c},
	{0x0b13, 0x01},

	{0x0b14, 0x46},
	{0x0b15, 0x80},
	{0x0b16, 0x01},
	{0x0b17, 0x00},

	{0x0b18, 0xf0},
	{0x0b19, 0x81},
	{0x0b1a, 0x01},
	{0x0b1b, 0x00},

	{0x0b1c, 0x55},
	{0x0b1d, 0x84},
	{0x0b1e, 0x01},
	{0x0b1f, 0x00},


	{0x0b20, 0x03},
	{0x0b21, 0xe3},
	{0x0b22, 0x01},
	{0x0b23, 0x00},

	{0x0b24, 0x40},
	{0x0b25, 0x64},
	{0x0b26, 0x02},
	{0x0b27, 0x00},

	{0x0b28, 0x12},
	{0x0b29, 0x1c},
	{0x0b2a, 0x03},
	{0x0b2b, 0x00},

	{0x0b2c, 0x80},
	{0x0b2d, 0x1c},
	{0x0b2e, 0x03},
	{0x0b2f, 0x00},

	{0x0b30, 0x10},
	{0x0b31, 0xfe},
	{0x0b32, 0x03},
	{0x0b33, 0x00},

	{0x0b34, 0x00},
	{0x0b35, 0xfe},
	{0x0b36, 0x03},
	{0x0b37, 0x00},

	{0x0b38, 0x9f},
	{0x0b39, 0x1c},
	{0x0b3a, 0x03},
	{0x0b3b, 0x00},

	{0x0b3c, 0x00},
	{0x0b3d, 0xfe},
	{0x0b3e, 0x03},
	{0x0b3f, 0x00},

	{0x0b40, 0x00},
	{0x0b41, 0xfe},
	{0x0b42, 0x03},
	{0x0b43, 0x00},

	{0x0b44, 0x00},
	{0x0b45, 0xfe},
	{0x0b46, 0x03},
	{0x0b47, 0x00},

	{0x0b48, 0x80},
	{0x0b49, 0x1c},
	{0x0b4a, 0x03},
	{0x0b4b, 0x00},

	{0x0b4c, 0x10},
	{0x0b4d, 0xfe},
	{0x0b4e, 0x03},
	{0x0b4f, 0x00},

	{0x0b50, 0x00},
	{0x0b51, 0xfe},
	{0x0b52, 0x03},
	{0x0b53, 0x00},

	{0x0b54, 0x9f},
	{0x0b55, 0x1c},
	{0x0b56, 0x03},
	{0x0b57, 0x00},

	{0x0b58, 0x99},
	{0x0b59, 0x02},
	{0x0b5a, 0x01},
	{0x0b5b, 0x00},

	{0x0b5c, 0x00},
	{0x0b5d, 0x64},
	{0x0b5e, 0x02},
	{0x0b5f, 0x00},

	{0x0b60, 0x00},
	{0x0b61, 0x02},
	{0x0b62, 0x01},
	{0x0b63, 0x00},

	{0x0b64, 0x06},
	{0x0b65, 0x80},
	{0x0b66, 0x01},
	{0x0b67, 0x00},

	{0x0b68, 0x00},
	{0x0b69, 0x81},
	{0x0b6a, 0x01},
	{0x0b6b, 0x00},

	{0x0b6c, 0x54},
	{0x0b6d, 0x84},
	{0x0b6e, 0x01},
	{0x0b6f, 0x00},

	{0x0b70, 0x60},
	{0x0b71, 0x1c},
	{0x0b72, 0x03},
	{0x0b73, 0x00},

	{0x0b74, 0x02},
	{0x0b75, 0xe3},
	{0x0b76, 0x01},
	{0x0b77, 0x00},

	{0x0b78, 0x13},
	{0x0b79, 0x10},
	{0x0b7a, 0x0c},
	{0x0b7b, 0x01},

	{0x0b7c, 0x01},
	{0x0b7d, 0x14},
	{0x0b7e, 0x03},
	{0x0b7f, 0x00},

	{0x0b80, 0x00},
	{0x0b81, 0xa7},
	{0x0b82, 0x01},
	{0x0b83, 0x00},

	{0x0b84, 0x34},
	{0x0b85, 0x0d},
	{0x0b86, 0x0c},
	{0x0b87, 0x01},

	{0x0b88, 0x53},
	{0x0b89, 0x15},
	{0x0b8a, 0x03},
	{0x0b8b, 0x01},

	{0x0aeb, 0x09},
	{0x0ae9, 0x17},
	{0x0aea, 0x23},
	{0x0ae8, 0xb0},

	{0x05a0, 0x82},
	{0x05ac, 0x00},
	{0x05ad, 0x01},

	{0x0597, 0x3d},
	{0x05ab, 0x09},
	{0x05a3, 0x06},
	{0x05a4, 0x07},
	{0x05ae, 0x00},

	{0x0800, 0x06},
	{0x0801, 0x0c},
	{0x0802, 0x18},
	{0x0803, 0x24},
	{0x0804, 0x30},

	{0x0805, 0x0e},
	{0x0806, 0x66},
	{0x0807, 0x0e},
	{0x0808, 0x65},
	{0x0809, 0x02},
	{0x080a, 0xc3},
	{0x080b, 0x02},
	{0x080c, 0xc7},
	{0x080d, 0x02},
	{0x080e, 0xcb},
	{0x080f, 0x0e},
	{0x0810, 0x6c},
	{0x0811, 0x0e},
	{0x0812, 0x6d},

	{0x0813, 0x20},
	{0x0814, 0x01},
	{0x0815, 0x18},
	{0x0816, 0x18},
	{0x0817, 0x18},
	{0x0818, 0x04},
	{0x0819, 0x04},

	{0x081a, 0x10},
	{0x081b, 0x02},
	{0x081c, 0x00},
	{0x081d, 0x00},
	{0x081e, 0x00},
	{0x081f, 0x04},
	{0x0820, 0x04},

	{0x0821, 0x04},
	{0x0822, 0x02},
	{0x0823, 0x00},
	{0x0824, 0x00},
	{0x0825, 0x00},
	{0x0826, 0x04},
	{0x0827, 0x04},

	{0x0828, 0x1b},
	{0x0829, 0x01},
	{0x082a, 0x00},
	{0x082b, 0x00},
	{0x082c, 0x00},
	{0x082d, 0x04},
	{0x082e, 0x04},

	{0x082f, 0x15},
	{0x0830, 0x01},
	{0x0831, 0x00},
	{0x0832, 0x00},
	{0x0833, 0x00},
	{0x0834, 0x01},
	{0x0835, 0x01},

	{0x0836, 0x0d},
	{0x0837, 0x01},
	{0x0838, 0x00},
	{0x0839, 0x00},
	{0x083a, 0x00},
	{0x083b, 0x01},
	{0x083c, 0x01},

	{0x083d, 0x01},
	{0x083e, 0x00},
	{0x083f, 0x00},
	{0x0840, 0x01},

	{0x0841, 0x01},
	{0x0842, 0x67},
	{0x0843, 0x00},
	{0x0844, 0x02},

	{0x0845, 0x02},
	{0x0846, 0x00},
	{0x0847, 0x00},
	{0x0848, 0x03},

	{0x0849, 0x02},
	{0x084a, 0xca},
	{0x084b, 0x00},
	{0x084c, 0x04},

	{0x084d, 0x03},
	{0x084e, 0xf6},
	{0x084f, 0x00},
	{0x0850, 0x05},

	{0x0851, 0x05},
	{0x0852, 0x84},
	{0x0853, 0x00},
	{0x0854, 0x06},

	{0x0855, 0x07},
	{0x0856, 0xca},
	{0x0857, 0x09},
	{0x0858, 0x36},

	{0x0859, 0x0B},
	{0x085a, 0x20},
	{0x085b, 0x10},
	{0x085c, 0x06},

	{0x085d, 0x0f},
	{0x085e, 0x90},
	{0x085f, 0x14},
	{0x0860, 0xa6},

	{0x05a0, 0xc2},
	{0x05ac, 0x01},
	{0x05ae, 0x00},
	{0x0207, 0x04},

	{0x0070, 0x05},
	{0x0080, 0x10},
	{0x0089, 0x83},
	{0x009a, 0x00},
	{0x00a0, 0x01},

	{0x0040, 0x22},
	{0x0041, 0x20},
	{0x0042, 0x20},
	{0x0043, 0x0f},
	{0x0044, 0x00},
	{0x0046, 0x0c},
	{0x0049, 0x06},
	{0x004a, 0x19},
	{0x004d, 0x00},
	{0x004e, 0x03},
	{0x0051, 0x26},
	{0x005a, 0x0c},
	{0x005b, 0x03},
	{0x021a, 0x00},
	{0x0450, 0x02},
	{0x0452, 0x02},
	{0x0454, 0x02},
	{0x0456, 0x02},

	{0x0204, 0x04},
	{0x0205, 0x00},
	{0x0208, 0x01},
	{0x0209, 0x95},

	{0x0096, 0x81},
	{0x0097, 0x01},
	{0x0098, 0xc7},

	{0x0351, 0x00},
	{0x0352, 0x10},
	{0x0353, 0x00},
	{0x0354, 0x10},
	{0x034c, 0x19},
	{0x034d, 0x80},
	{0x034e, 0x13},
	{0x034f, 0x20},

	{0x0180, 0x46},
	{0x0181, 0xf0},
	{0x0182, 0x55},
	{0x0183, 0x55},
	{0x0184, 0x55},
	{0x0186, 0x9f},
	{0x0187, 0x00},
	{0x0188, 0x00},
	{0x0189, 0x00},

	{0x0107, 0x00},
	{0x010b, 0x12},
	{0x0115, 0x00},
	{0x0121, 0x12},
	{0x0122, 0x0d},
	{0x0123, 0x4b},
	{0x0124, 0x02},
	{0x0125, 0x16},
	{0x0126, 0x0f},
	{0x0127, 0x10},
	{0x0129, 0x0d},
	{0x012a, 0x1f},
	{0x012b, 0x10},
	{0x0084, 0x10},

	{0x0a93, 0x60},
	{0x0a90, 0x11},
	{0x0313, 0x80},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0aff, 0x00},
	{0x0a90, 0x00},
	{0x00a4, 0x80},
	{0x0316, 0x00},
	{0x0a67, 0x00},

	{0x031c, 0x12},
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

	{0x0102, 0x99},
	{0x0100, 0x01},

	{REG_NULL, 0x00},
};

static const struct gc32e1_mode supported_modes_4lane[] = {
	{
		.width = 3264,
		.height = 2448,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0900,
		.hts_def = 0x19B0,
		.vts_def = 0x09A0,
		.mipi_freq_idx = 0,
		.reg_list = gc32e1_global_3264_2448_30fps_regs,
	},
	{
		.width = 6528,
		.height = 4896,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0900,
		.hts_def = 0x19B0,
		.vts_def = 0x13D8,
		.mipi_freq_idx = 1,
		.reg_list = gc32e1_global_6258_4896_15fps_regs,
	},
};

static const struct gc32e1_mode *supported_modes;

static const s64 link_freq_menu_items[] = {
	GC32E1_LINK_FREQ_MHZ_3K,
	GC32E1_LINK_FREQ_MHZ_6K
};

/* Write registers up to 4 at a time */
static int gc32e1_write_reg(struct i2c_client *client, u16 reg, u8 val)
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
		"gc32e1 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int gc32e1_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = gc32e1_write_reg(client, regs[i].addr, regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int gc32e1_read_reg(struct i2c_client *client, u16 reg, u8 *val)
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
		"gc32e1 read reg:0x%x failed !\n", reg);

	return ret;
}

static int gc32e1_get_reso_dist(const struct gc32e1_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
		abs(mode->height - framefmt->height);
}

static const struct gc32e1_mode *
gc32e1_find_best_fit(struct gc32e1 *gc32e1,
		     struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < gc32e1->cfg_num; i++) {
		dist = gc32e1_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int gc32e1_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);
	const struct gc32e1_mode *mode;
	s64 h_blank, vblank_def;

	mutex_lock(&gc32e1->mutex);

	mode = gc32e1_find_best_fit(gc32e1, fmt);
	fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&gc32e1->mutex);
		return -ENOTTY;
#endif
	} else {
		gc32e1->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(gc32e1->hblank, h_blank,
			h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(gc32e1->vblank, vblank_def,
			GC32E1_VTS_MAX - mode->height,
			1, vblank_def);
		__v4l2_ctrl_s_ctrl(gc32e1->link_freq, mode->mipi_freq_idx);
	}

	mutex_unlock(&gc32e1->mutex);

	return 0;
}

static int gc32e1_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);
	const struct gc32e1_mode *mode = gc32e1->cur_mode;

	mutex_lock(&gc32e1->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&gc32e1->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&gc32e1->mutex);

	return 0;
}

static int gc32e1_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;

	return 0;
}

static int gc32e1_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);

	if (fse->index >= gc32e1->cfg_num)
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SRGGB10_1X10)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int gc32e1_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);
	const struct gc32e1_mode *mode = gc32e1->cur_mode;

	mutex_lock(&gc32e1->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&gc32e1->mutex);

	return 0;
}

static void gc32e1_get_module_inf(struct gc32e1 *gc32e1,
				  struct rkmodule_inf *inf)
{
	strscpy(inf->base.sensor,
		GC32E1_NAME,
		sizeof(inf->base.sensor));
	strscpy(inf->base.module,
		gc32e1->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens,
		gc32e1->len_name,
		sizeof(inf->base.lens));
}

static void gc32e1_set_module_inf(struct gc32e1 *gc32e1,
				  struct rkmodule_awb_cfg *cfg)
{
	mutex_lock(&gc32e1->mutex);
	memcpy(&gc32e1->awb_cfg, cfg, sizeof(*cfg));
	mutex_unlock(&gc32e1->mutex);
}

static long gc32e1_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gc32e1_get_module_inf(gc32e1, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_AWB_CFG:
		gc32e1_set_module_inf(gc32e1, (struct rkmodule_awb_cfg *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);
		if (stream) {
			ret = gc32e1_write_reg(gc32e1->client,
					       GC32E1_REG_CTRL_MODE,
					       GC32E1_MODE_STREAMING);
		} else {
			ret = gc32e1_write_reg(gc32e1->client,
					       GC32E1_REG_CTRL_MODE,
					       GC32E1_MODE_SW_STANDBY);
		}
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gc32e1_compat_ioctl32(struct v4l2_subdev *sd,
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

		ret = gc32e1_ioctl(sd, cmd, inf);
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
			ret = gc32e1_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = gc32e1_ioctl(sd, cmd, &stream);
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

static int __gc32e1_start_stream(struct gc32e1 *gc32e1)
{
	int ret;

	ret = gc32e1_write_array(gc32e1->client, gc32e1->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&gc32e1->mutex);
	ret = v4l2_ctrl_handler_setup(&gc32e1->ctrl_handler);
	mutex_lock(&gc32e1->mutex);
	if (ret)
		return ret;
	ret = gc32e1_write_reg(gc32e1->client,
			       GC32E1_REG_CTRL_MODE,
			       GC32E1_MODE_STREAMING);
	return ret;
}

static int __gc32e1_stop_stream(struct gc32e1 *gc32e1)
{
	int ret = 0;

	ret = gc32e1_write_reg(gc32e1->client,
			       GC32E1_REG_CTRL_MODE,
			       GC32E1_MODE_SW_STANDBY);
	return ret;
}

static int gc32e1_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);
	struct i2c_client *client = gc32e1->client;
	int ret = 0;

	mutex_lock(&gc32e1->mutex);
	on = !!on;
	if (on == gc32e1->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gc32e1_start_stream(gc32e1);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gc32e1_stop_stream(gc32e1);
		pm_runtime_put(&client->dev);
	}

	gc32e1->streaming = on;

unlock_and_return:
	mutex_unlock(&gc32e1->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 gc32e1_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, GC32E1_XVCLK_FREQ / 1000 / 1000);
}

static int __gc32e1_power_on(struct gc32e1 *gc32e1)
{
	int ret;
	u32 delay_us;
	struct device *dev = &gc32e1->client->dev;

	if (!IS_ERR_OR_NULL(gc32e1->pins_default)) {
		ret = pinctrl_select_state(gc32e1->pinctrl,
					   gc32e1->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(gc32e1->xvclk, GC32E1_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(gc32e1->xvclk) != GC32E1_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(gc32e1->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}
	if (!IS_ERR(gc32e1->reset_gpio))
		gpiod_set_value_cansleep(gc32e1->reset_gpio, 0);

	ret = regulator_bulk_enable(GC32E1_NUM_SUPPLIES, gc32e1->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	usleep_range(1000, 1100);
	if (!IS_ERR(gc32e1->reset_gpio))
		gpiod_set_value_cansleep(gc32e1->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(gc32e1->pwdn_gpio))
		gpiod_set_value_cansleep(gc32e1->pwdn_gpio, 1);

	usleep_range(15000, 16000);
	/* 8192 cycles prior to first SCCB transaction */
	delay_us = gc32e1_cal_delay(8192);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(gc32e1->xvclk);

	return ret;
}

static void __gc32e1_power_off(struct gc32e1 *gc32e1)
{
	int ret;

	if (!IS_ERR(gc32e1->pwdn_gpio))
		gpiod_set_value_cansleep(gc32e1->pwdn_gpio, 0);
	clk_disable_unprepare(gc32e1->xvclk);
	if (!IS_ERR(gc32e1->reset_gpio))
		gpiod_set_value_cansleep(gc32e1->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(gc32e1->pins_sleep)) {
		ret = pinctrl_select_state(gc32e1->pinctrl,
			gc32e1->pins_sleep);
		if (ret < 0)
			dev_dbg(&gc32e1->client->dev, "could not set pins\n");
	}
	regulator_bulk_disable(GC32E1_NUM_SUPPLIES, gc32e1->supplies);
}


static int gc32e1_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);
	struct i2c_client *client = gc32e1->client;
	int ret = 0;

	dev_info(&gc32e1->client->dev, "%s on:%d\n", __func__, on);
	mutex_lock(&gc32e1->mutex);

	/* If the power state is not modified - no work to do. */
	if (gc32e1->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		__gc32e1_power_on(gc32e1);
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gc32e1->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		__gc32e1_power_off(gc32e1);
		gc32e1->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc32e1->mutex);

	return ret;
}

static int gc32e1_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc32e1 *gc32e1 = to_gc32e1(sd);

	return __gc32e1_power_on(gc32e1);
}

static int gc32e1_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc32e1 *gc32e1 = to_gc32e1(sd);

	__gc32e1_power_off(gc32e1);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc32e1_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);
	struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct gc32e1_mode *def_mode = &supported_modes[0];

	mutex_lock(&gc32e1->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&gc32e1->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int sensor_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct gc32e1 *sensor = to_gc32e1(sd);

	if (sensor->lane_num == 4) {
		config->type = V4L2_MBUS_CSI2_DPHY;
		config->bus.mipi_csi2.num_data_lanes = GC32E1_LANES;
	} else {
		dev_err(&sensor->client->dev,
			"unsupported lane_num(%d)\n", sensor->lane_num);
	}
	return 0;
}

static int gc32e1_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum *fie)
{
	struct gc32e1 *gc32e1 = to_gc32e1(sd);

	if (fie->index >= gc32e1->cfg_num)
		return -EINVAL;

	fie->code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static const struct dev_pm_ops gc32e1_pm_ops = {
	SET_RUNTIME_PM_OPS(gc32e1_runtime_suspend,
			   gc32e1_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops gc32e1_internal_ops = {
	.open = gc32e1_open,
};
#endif

static const struct v4l2_subdev_core_ops gc32e1_core_ops = {
	.s_power = gc32e1_s_power,
	.ioctl = gc32e1_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc32e1_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops gc32e1_video_ops = {
	.s_stream = gc32e1_s_stream,
	.g_frame_interval = gc32e1_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops gc32e1_pad_ops = {
	.enum_mbus_code = gc32e1_enum_mbus_code,
	.enum_frame_size = gc32e1_enum_frame_sizes,
	.enum_frame_interval = gc32e1_enum_frame_interval,
	.get_fmt = gc32e1_get_fmt,
	.set_fmt = gc32e1_set_fmt,
	.get_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_ops gc32e1_subdev_ops = {
	.core	= &gc32e1_core_ops,
	.video	= &gc32e1_video_ops,
	.pad	= &gc32e1_pad_ops,
};

static const char * const gc32e1_test_pattern_menu[] = {
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

static int gc32e1_set_exposure_reg(struct gc32e1 *gc32e1, u32 exposure)
{
	int ret = 0;
	u32 caltime = 0;

	caltime = exposure / 2;
	caltime = caltime * 2;
	ret = gc32e1_write_reg(gc32e1->client,
			       GC32E1_REG_EXPOSURE_H,
			       (caltime >> 8) & 0xFF);
	ret |= gc32e1_write_reg(gc32e1->client,
				GC32E1_REG_EXPOSURE_L,
				caltime & 0xFF);
	return ret;
}

static int gc32e1_set_gain_reg(struct gc32e1 *gc32e1, u32 a_gain)
{
	int ret = 0;

	dev_dbg(&gc32e1->client->dev, "%s(%d) a_gain(%d)!\n",
		__func__, __LINE__, a_gain);
	if (a_gain < GC32E1_GAIN_MIN)
		a_gain = GC32E1_GAIN_MIN;
	else if (a_gain > GC32E1_GAIN_MAX)
		a_gain = GC32E1_GAIN_MAX;

	ret = gc32e1_write_reg(gc32e1->client,
		GC32E1_REG_AGAIN_H, a_gain >> 8);
	ret |= gc32e1_write_reg(gc32e1->client,
		GC32E1_REG_AGAIN_L, a_gain & 0xFF);

	return ret;
}

static int gc32e1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc32e1 *gc32e1 = container_of(ctrl->handler,
					     struct gc32e1, ctrl_handler);
	struct i2c_client *client = gc32e1->client;
	s64 max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = gc32e1->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(gc32e1->exposure,
			gc32e1->exposure->minimum, max,
			gc32e1->exposure->step,
			gc32e1->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = gc32e1_set_exposure_reg(gc32e1, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = gc32e1_set_gain_reg(gc32e1, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = gc32e1_write_reg(gc32e1->client,
			GC32E1_REG_VTS_H,
			((ctrl->val + gc32e1->cur_mode->height) >> 8) & 0xff);
		ret |= gc32e1_write_reg(gc32e1->client,
			GC32E1_REG_VTS_L,
			(ctrl->val + gc32e1->cur_mode->height) & 0xff);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			__func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops gc32e1_ctrl_ops = {
	.s_ctrl = gc32e1_set_ctrl,
};

static int gc32e1_initialize_controls(struct gc32e1 *gc32e1)
{
	const struct gc32e1_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;

	handler = &gc32e1->ctrl_handler;
	mode = gc32e1->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &gc32e1->mutex;

	gc32e1->link_freq = v4l2_ctrl_new_int_menu(handler, NULL, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(link_freq_menu_items) - 1, 0, link_freq_menu_items);
	v4l2_ctrl_s_ctrl(gc32e1->link_freq, mode->mipi_freq_idx);

	v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
		0, GC32E1_PIXEL_RATE, 1, GC32E1_PIXEL_RATE);

	h_blank = mode->hts_def - mode->width;
	gc32e1->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
		h_blank, h_blank, 1, h_blank);
	if (gc32e1->hblank)
		gc32e1->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	gc32e1->vblank = v4l2_ctrl_new_std(handler, &gc32e1_ctrl_ops,
		V4L2_CID_VBLANK, vblank_def,
		GC32E1_VTS_MAX - mode->height,
		1, vblank_def);

	exposure_max = mode->vts_def - 4;
	gc32e1->exposure = v4l2_ctrl_new_std(handler, &gc32e1_ctrl_ops,
		V4L2_CID_EXPOSURE, GC32E1_EXPOSURE_MIN,
		exposure_max, GC32E1_EXPOSURE_STEP,
		mode->exp_def);

	gc32e1->anal_gain = v4l2_ctrl_new_std(handler, &gc32e1_ctrl_ops,
		V4L2_CID_ANALOGUE_GAIN, GC32E1_GAIN_MIN,
		GC32E1_GAIN_MAX, GC32E1_GAIN_STEP,
		GC32E1_GAIN_DEFAULT);

	gc32e1->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&gc32e1_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(gc32e1_test_pattern_menu) - 1,
				0, 0, gc32e1_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_err(&gc32e1->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	gc32e1->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int gc32e1_check_sensor_id(struct gc32e1 *gc32e1,
	struct i2c_client *client)
{
	struct device *dev = &gc32e1->client->dev;
	u16 id = 0;
	u8 reg_H = 0;
	u8 reg_L = 0;
	int ret;

	ret = gc32e1_read_reg(client, GC32E1_REG_CHIP_ID_H, &reg_H);
	if (ret)
		ret = gc32e1_read_reg(client, GC32E1_REG_CHIP_ID_H, &reg_H);
	ret |= gc32e1_read_reg(client, GC32E1_REG_CHIP_ID_L, &reg_L);
	id = ((reg_H << 8) & 0xff00) | (reg_L & 0xff);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}
	dev_info(dev, "detected gc%04x sensor\n", id);
	return ret;
}

static int gc32e1_configure_regulators(struct gc32e1 *gc32e1)
{
	unsigned int i;

	for (i = 0; i < GC32E1_NUM_SUPPLIES; i++)
		gc32e1->supplies[i].supply = gc32e1_supply_names[i];

	return devm_regulator_bulk_get(&gc32e1->client->dev,
		GC32E1_NUM_SUPPLIES,
		gc32e1->supplies);
}

static void free_gpio(struct gc32e1 *sensor)
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

static int gc32e1_parse_of(struct gc32e1 *gc32e1)
{
	struct device *dev = &gc32e1->client->dev;
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

	gc32e1->lane_num = rval;
	if (gc32e1->lane_num == 4) {
		gc32e1->cur_mode = &supported_modes_4lane[0];
		supported_modes = supported_modes_4lane;
		gc32e1->cfg_num = ARRAY_SIZE(supported_modes_4lane);

		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		gc32e1->pixel_rate = link_freq_menu_items[gc32e1->cur_mode->mipi_freq_idx] *
				     2U * gc32e1->lane_num / 10U;
		dev_info(dev, "lane_num(%d)  pixel_rate(%u)\n",
			 gc32e1->lane_num, gc32e1->pixel_rate);
	} else {
		dev_err(dev, "unsupported lane_num(%d)\n", gc32e1->lane_num);
		return -1;
	}
	return 0;
}

static int gc32e1_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct gc32e1 *gc32e1;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	gc32e1 = devm_kzalloc(dev, sizeof(*gc32e1), GFP_KERNEL);
	if (!gc32e1)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
		&gc32e1->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
		&gc32e1->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
		&gc32e1->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
		&gc32e1->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}
	gc32e1->client = client;

	gc32e1->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(gc32e1->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	gc32e1->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(gc32e1->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	gc32e1->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(gc32e1->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = gc32e1_configure_regulators(gc32e1);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	ret = gc32e1_parse_of(gc32e1);
	if (ret != 0)
		return -EINVAL;

	gc32e1->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(gc32e1->pinctrl)) {
		gc32e1->pins_default =
			pinctrl_lookup_state(gc32e1->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(gc32e1->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		gc32e1->pins_sleep =
			pinctrl_lookup_state(gc32e1->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(gc32e1->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&gc32e1->mutex);

	sd = &gc32e1->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc32e1_subdev_ops);
	ret = gc32e1_initialize_controls(gc32e1);
	if (ret)
		goto err_destroy_mutex;

	ret = __gc32e1_power_on(gc32e1);
	if (ret)
		goto err_free_handler;

	ret = gc32e1_check_sensor_id(gc32e1, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc32e1_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	gc32e1->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc32e1->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc32e1->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc32e1->module_index, facing,
		 GC32E1_NAME, dev_name(sd->dev));
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
	__gc32e1_power_off(gc32e1);
	free_gpio(gc32e1);
err_free_handler:
	v4l2_ctrl_handler_free(&gc32e1->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&gc32e1->mutex);

	return ret;
}

static void gc32e1_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc32e1 *gc32e1 = to_gc32e1(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&gc32e1->ctrl_handler);
	mutex_destroy(&gc32e1->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc32e1_power_off(gc32e1);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc32e1_of_match[] = {
	{ .compatible = "galaxycore,gc32e1" },
	{},
};
MODULE_DEVICE_TABLE(of, gc32e1_of_match);
#endif

static const struct i2c_device_id gc32e1_match_id[] = {
	{ "galaxycore,gc32e1", 0 },
	{ },
};

static struct i2c_driver gc32e1_i2c_driver = {
	.driver = {
		.name = GC32E1_NAME,
		.pm = &gc32e1_pm_ops,
		.of_match_table = of_match_ptr(gc32e1_of_match),
	},
	.probe		= &gc32e1_probe,
	.remove		= &gc32e1_remove,
	.id_table	= gc32e1_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc32e1_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc32e1_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("GalaxyCore gc32e1 sensor driver");
MODULE_LICENSE("GPL");
