// SPDX-License-Identifier: GPL-2.0
/*
 * s5k3l8xx camera driver
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 *
 * V0.0X01.0X00 first version.
 * V0.0X01.0X01 update sensor driver.
 * 1. adjust power sequence to suit spec.
 * 2. add read/write reg error debug log.
 *
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
#include <linux/compat.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#define S5K3L8XX_MAJOR_I2C_ADDR		0x10
#define S5K3L8XX_MINOR_I2C_ADDR		0x2D

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define S5K3L8XX_LINK_FREQ_562MHZ	562000000U
/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
#define S5K3L8XX_PIXEL_RATE		(S5K3L8XX_LINK_FREQ_562MHZ * 2LL * 4LL / 10LL)
#define S5K3L8XX_XVCLK_FREQ		24000000

#define CHIP_ID				0x30c8
#define S5K3L8XX_REG_CHIP_ID		0x0000

#define S5K3L8XX_REG_CTRL_MODE		0x0100
#define S5K3L8XX_MODE_SW_STANDBY	0x0
#define S5K3L8XX_MODE_STREAMING		BIT(0)
#define S5K3L8XX_REG_STREAM_ON		0x3C1E

#define S5K3L8XX_REG_EXPOSURE		0x0202
#define	S5K3L8XX_EXPOSURE_MIN		4
#define	S5K3L8XX_EXPOSURE_STEP		1
#define S5K3L8XX_VTS_MAX		0xfff7

#define S5K3L8XX_REG_ANALOG_GAIN	0x0204
#define S5K3L8XX_GAIN_MIN		0x40
#define S5K3L8XX_GAIN_MAX		0x200
#define S5K3L8XX_GAIN_STEP		1
#define S5K3L8XX_GAIN_DEFAULT		0x100

#define S5K3L8XX_REG_TEST_PATTERN	0x0601
#define	S5K3L8XX_TEST_PATTERN_ENABLE	0x80
#define	S5K3L8XX_TEST_PATTERN_DISABLE	0x0

#define S5K3L8XX_REG_VTS		0x0340

#define REG_NULL			0xFFFF

#define S5K3L8XX_REG_VALUE_08BIT	1
#define S5K3L8XX_REG_VALUE_16BIT	2
#define S5K3L8XX_REG_VALUE_24BIT	3

#define S5K3L8XX_LANES			4
#define S5K3L8XX_BITS_PER_SAMPLE	10

#define S5K3L8XX_CHIP_REVISION_REG	0x0004

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"

#define S5K3L8XX_NAME			"s5k3l8xx"

// #define S5K3L8XX_MIRROR
// #define S5K3L8XX_FLIP
#define S5K3L8XX_FLIP_MIRROR
#ifdef S5K3L8XX_MIRROR
#define S5K3L8XX_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SRGGB10_1X10
#elif defined S5K3L8XX_FLIP
#define S5K3L8XX_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SBGGR10_1X10
#elif defined S5K3L8XX_FLIP_MIRROR
#define S5K3L8XX_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SGBRG10_1X10
#else
#define S5K3L8XX_MEDIA_BUS_FMT		MEDIA_BUS_FMT_SGRBG10_1X10
#endif

static const char * const s5k3l8xx_supply_names[] = {
	"avdd",		/* Analog power */
	"dovdd",	/* Digital I/O power */
	"dvdd",		/* Digital core power */
};

#define S5K3L8XX_NUM_SUPPLIES ARRAY_SIZE(s5k3l8xx_supply_names)

struct regval {
	u16 addr;
	u16 val;
};

struct s5k3l8xx_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 link_freq_idx;
	u32 bpp;
	const struct regval *reg_list;
};

struct s5k3l8xx {
	struct i2c_client	*client;
	struct clk		*xvclk;
	struct gpio_desc	*power_gpio;
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[S5K3L8XX_NUM_SUPPLIES];

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
	bool			streaming;
	bool			power_on;
	const struct s5k3l8xx_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
};

#define to_s5k3l8xx(sd) container_of(sd, struct s5k3l8xx, subdev)

static const struct regval s5k3l8xx_4208x3120_30fps_regs[] = {
#ifdef S5K3L8XX_MIRROR
	{0x0100, 0x0001},
#elif defined S5K3L8XX_FLIP
	{0x0100, 0x0002},
#elif defined S5K3L8XX_FLIP_MIRROR
	{0x0100, 0x0003},
#else
	{0x0100, 0x0000},
#endif
	{0x6028, 0x4000},
	{0x6214, 0xFFFF},
	{0x6216, 0xFFFF},
	{0x6218, 0x0000},
	{0x621A, 0x0000},

	{0x6028, 0x2000},
	{0x602A, 0x2450},
	{0x6F12, 0x0448},
	{0x6F12, 0x0349},
	{0x6F12, 0x0160},
	{0x6F12, 0xC26A},
	{0x6F12, 0x511A},
	{0x6F12, 0x8180},
	{0x6F12, 0x00F0},
	{0x6F12, 0x48B8},
	{0x6F12, 0x2000},
	{0x6F12, 0x2588},
	{0x6F12, 0x2000},
	{0x6F12, 0x16C0},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x10B5},
	{0x6F12, 0x00F0},
	{0x6F12, 0x5DF8},
	{0x6F12, 0x2748},
	{0x6F12, 0x4078},
	{0x6F12, 0x0028},
	{0x6F12, 0x0AD0},
	{0x6F12, 0x00F0},
	{0x6F12, 0x5CF8},
	{0x6F12, 0x2549},
	{0x6F12, 0xB1F8},
	{0x6F12, 0x1403},
	{0x6F12, 0x4200},
	{0x6F12, 0x2448},
	{0x6F12, 0x4282},
	{0x6F12, 0x91F8},
	{0x6F12, 0x9610},
	{0x6F12, 0x4187},
	{0x6F12, 0x10BD},
	{0x6F12, 0x70B5},
	{0x6F12, 0x0446},
	{0x6F12, 0x2148},
	{0x6F12, 0x0022},
	{0x6F12, 0x4068},
	{0x6F12, 0x86B2},
	{0x6F12, 0x050C},
	{0x6F12, 0x3146},
	{0x6F12, 0x2846},
	{0x6F12, 0x00F0},
	{0x6F12, 0x4CF8},
	{0x6F12, 0x2046},
	{0x6F12, 0x00F0},
	{0x6F12, 0x4EF8},
	{0x6F12, 0x14F8},
	{0x6F12, 0x680F},
	{0x6F12, 0x6178},
	{0x6F12, 0x40EA},
	{0x6F12, 0x4100},
	{0x6F12, 0x1749},
	{0x6F12, 0xC886},
	{0x6F12, 0x1848},
	{0x6F12, 0x2278},
	{0x6F12, 0x007C},
	{0x6F12, 0x4240},
	{0x6F12, 0x1348},
	{0x6F12, 0xA230},
	{0x6F12, 0x8378},
	{0x6F12, 0x43EA},
	{0x6F12, 0xC202},
	{0x6F12, 0x0378},
	{0x6F12, 0x4078},
	{0x6F12, 0x9B00},
	{0x6F12, 0x43EA},
	{0x6F12, 0x4000},
	{0x6F12, 0x0243},
	{0x6F12, 0xD0B2},
	{0x6F12, 0x0882},
	{0x6F12, 0x3146},
	{0x6F12, 0x2846},
	{0x6F12, 0xBDE8},
	{0x6F12, 0x7040},
	{0x6F12, 0x0122},
	{0x6F12, 0x00F0},
	{0x6F12, 0x2AB8},
	{0x6F12, 0x10B5},
	{0x6F12, 0x0022},
	{0x6F12, 0xAFF2},
	{0x6F12, 0x8701},
	{0x6F12, 0x0B48},
	{0x6F12, 0x00F0},
	{0x6F12, 0x2DF8},
	{0x6F12, 0x084C},
	{0x6F12, 0x0022},
	{0x6F12, 0xAFF2},
	{0x6F12, 0x6D01},
	{0x6F12, 0x2060},
	{0x6F12, 0x0848},
	{0x6F12, 0x00F0},
	{0x6F12, 0x25F8},
	{0x6F12, 0x6060},
	{0x6F12, 0x10BD},
	{0x6F12, 0x0000},
	{0x6F12, 0x2000},
	{0x6F12, 0x0550},
	{0x6F12, 0x2000},
	{0x6F12, 0x0C60},
	{0x6F12, 0x4000},
	{0x6F12, 0xD000},
	{0x6F12, 0x2000},
	{0x6F12, 0x2580},
	{0x6F12, 0x2000},
	{0x6F12, 0x16F0},
	{0x6F12, 0x0000},
	{0x6F12, 0x2221},
	{0x6F12, 0x0000},
	{0x6F12, 0x2249},
	{0x6F12, 0x42F2},
	{0x6F12, 0x351C},
	{0x6F12, 0xC0F2},
	{0x6F12, 0x000C},
	{0x6F12, 0x6047},
	{0x6F12, 0x42F2},
	{0x6F12, 0xE11C},
	{0x6F12, 0xC0F2},
	{0x6F12, 0x000C},
	{0x6F12, 0x6047},
	{0x6F12, 0x40F2},
	{0x6F12, 0x077C},
	{0x6F12, 0xC0F2},
	{0x6F12, 0x000C},
	{0x6F12, 0x6047},
	{0x6F12, 0x42F2},
	{0x6F12, 0x492C},
	{0x6F12, 0xC0F2},
	{0x6F12, 0x000C},
	{0x6F12, 0x6047},
	{0x6F12, 0x4BF2},
	{0x6F12, 0x453C},
	{0x6F12, 0xC0F2},
	{0x6F12, 0x000C},
	{0x6F12, 0x6047},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x0000},
	{0x6F12, 0x30C8},
	{0x6F12, 0x0157},
	{0x6F12, 0x0000},
	{0x6F12, 0x0003},

	{0x6028, 0x2000},
	{0x602A, 0x1082},
	{0x6F12, 0x8010},
	{0x6028, 0x4000},
	{0x31CE, 0x0001},
	{0x0200, 0x00C6},
	{0x3734, 0x0010},
	{0x3736, 0x0001},
	{0x3738, 0x0001},
	{0x37CC, 0x0001},
	{0x3744, 0x0100},
	{0x3762, 0x0105},
	{0x3764, 0x0105},
	{0x376A, 0x00F0},
	{0x344A, 0x000F},
	{0x344C, 0x003D},
	{0xF460, 0x0030},
	{0xF414, 0x24C2},
	{0xF416, 0x0183},
	{0xF468, 0x4005},
	{0x3424, 0x0A07},
	{0x3426, 0x0F07},
	{0x3428, 0x0F07},
	{0x341E, 0x0804},
	{0x3420, 0x0C0C},
	{0x3422, 0x2D2D},
	{0xF462, 0x003A},
	{0x3450, 0x0010},
	{0x3452, 0x0010},
	{0xF446, 0x0020},
	{0xF44E, 0x000C},
	{0x31FA, 0x0007},
	{0x31FC, 0x0161},
	{0x31FE, 0x0009},
	{0x3200, 0x000C},
	{0x3202, 0x007F},
	{0x3204, 0x00A2},
	{0x3206, 0x007D},
	{0x3208, 0x00A4},
	{0x3334, 0x00A7},
	{0x3336, 0x00A5},
	{0x3338, 0x0033},
	{0x333A, 0x0006},
	{0x333C, 0x009F},
	{0x333E, 0x008C},
	{0x3340, 0x002D},
	{0x3342, 0x000A},
	{0x3344, 0x002F},
	{0x3346, 0x0008},
	{0x3348, 0x009F},
	{0x334A, 0x008C},
	{0x334C, 0x002D},
	{0x334E, 0x000A},
	{0x3350, 0x000A},
	{0x320A, 0x007B},
	{0x320C, 0x0161},
	{0x320E, 0x007F},
	{0x3210, 0x015F},
	{0x3212, 0x007B},
	{0x3214, 0x00B0},
	{0x3216, 0x0009},
	{0x3218, 0x0038},
	{0x321A, 0x0009},
	{0x321C, 0x0031},
	{0x321E, 0x0009},
	{0x3220, 0x0038},
	{0x3222, 0x0009},
	{0x3224, 0x007B},
	{0x3226, 0x0001},
	{0x3228, 0x0010},
	{0x322A, 0x00A2},
	{0x322C, 0x00B1},
	{0x322E, 0x0002},
	{0x3230, 0x015D},
	{0x3232, 0x0001},
	{0x3234, 0x015D},
	{0x3236, 0x0001},
	{0x3238, 0x000B},
	{0x323A, 0x0016},
	{0x323C, 0x000D},
	{0x323E, 0x001C},
	{0x3240, 0x000D},
	{0x3242, 0x0054},
	{0x3244, 0x007B},
	{0x3246, 0x00CC},
	{0x3248, 0x015D},
	{0x324A, 0x007E},
	{0x324C, 0x0095},
	{0x324E, 0x0085},
	{0x3250, 0x009D},
	{0x3252, 0x008D},
	{0x3254, 0x009D},
	{0x3256, 0x007E},
	{0x3258, 0x0080},
	{0x325A, 0x0001},
	{0x325C, 0x0005},
	{0x325E, 0x0085},
	{0x3260, 0x009D},
	{0x3262, 0x0001},
	{0x3264, 0x0005},
	{0x3266, 0x007E},
	{0x3268, 0x0080},
	{0x326A, 0x0053},
	{0x326C, 0x007D},
	{0x326E, 0x00CB},
	{0x3270, 0x015E},
	{0x3272, 0x0001},
	{0x3274, 0x0005},
	{0x3276, 0x0009},
	{0x3278, 0x000C},
	{0x327A, 0x007E},
	{0x327C, 0x0098},
	{0x327E, 0x0009},
	{0x3280, 0x000C},
	{0x3282, 0x007E},
	{0x3284, 0x0080},
	{0x3286, 0x0044},
	{0x3288, 0x0163},
	{0x328A, 0x0045},
	{0x328C, 0x0047},
	{0x328E, 0x007D},
	{0x3290, 0x0080},
	{0x3292, 0x015F},
	{0x3294, 0x0162},
	{0x3296, 0x007D},
	{0x3298, 0x0000},
	{0x329A, 0x0000},
	{0x329C, 0x0000},
	{0x329E, 0x0000},
	{0x32A0, 0x0008},
	{0x32A2, 0x0010},
	{0x32A4, 0x0018},
	{0x32A6, 0x0020},
	{0x32A8, 0x0000},
	{0x32AA, 0x0008},
	{0x32AC, 0x0010},
	{0x32AE, 0x0018},
	{0x32B0, 0x0020},
	{0x32B2, 0x0020},
	{0x32B4, 0x0020},
	{0x32B6, 0x0020},
	{0x32B8, 0x0000},
	{0x32BA, 0x0000},
	{0x32BC, 0x0000},
	{0x32BE, 0x0000},
	{0x32C0, 0x0000},
	{0x32C2, 0x0000},
	{0x32C4, 0x0000},
	{0x32C6, 0x0000},
	{0x32C8, 0x0000},
	{0x32CA, 0x0000},
	{0x32CC, 0x0000},
	{0x32CE, 0x0000},
	{0x32D0, 0x0000},
	{0x32D2, 0x0000},
	{0x32D4, 0x0000},
	{0x32D6, 0x0000},
	{0x32D8, 0x0000},
	{0x32DA, 0x0000},
	{0x32DC, 0x0000},
	{0x32DE, 0x0000},
	{0x32E0, 0x0000},
	{0x32E2, 0x0000},
	{0x32E4, 0x0000},
	{0x32E6, 0x0000},
	{0x32E8, 0x0000},
	{0x32EA, 0x0000},
	{0x32EC, 0x0000},
	{0x32EE, 0x0000},
	{0x32F0, 0x0000},
	{0x32F2, 0x0000},
	{0x32F4, 0x000A},
	{0x32F6, 0x0002},
	{0x32F8, 0x0008},
	{0x32FA, 0x0010},
	{0x32FC, 0x0020},
	{0x32FE, 0x0028},
	{0x3300, 0x0038},
	{0x3302, 0x0040},
	{0x3304, 0x0050},
	{0x3306, 0x0058},
	{0x3308, 0x0068},
	{0x330A, 0x0070},
	{0x330C, 0x0080},
	{0x330E, 0x0088},
	{0x3310, 0x0098},
	{0x3312, 0x00A0},
	{0x3314, 0x00B0},
	{0x3316, 0x00B8},
	{0x3318, 0x00C8},
	{0x331A, 0x00D0},
	{0x331C, 0x00E0},
	{0x331E, 0x00E8},
	{0x3320, 0x0017},
	{0x3322, 0x002F},
	{0x3324, 0x0047},
	{0x3326, 0x005F},
	{0x3328, 0x0077},
	{0x332A, 0x008F},
	{0x332C, 0x00A7},
	{0x332E, 0x00BF},
	{0x3330, 0x00D7},
	{0x3332, 0x00EF},
	{0x3352, 0x00A5},
	{0x3354, 0x00AF},
	{0x3356, 0x0187},
	{0x3358, 0x0000},
	{0x335A, 0x009E},
	{0x335C, 0x016B},
	{0x335E, 0x0015},
	{0x3360, 0x00A5},
	{0x3362, 0x00AF},
	{0x3364, 0x01FB},
	{0x3366, 0x0000},
	{0x3368, 0x009E},
	{0x336A, 0x016B},
	{0x336C, 0x0015},
	{0x336E, 0x00A5},
	{0x3370, 0x00A6},
	{0x3372, 0x0187},
	{0x3374, 0x0000},
	{0x3376, 0x009E},
	{0x3378, 0x016B},
	{0x337A, 0x0015},
	{0x337C, 0x00A5},
	{0x337E, 0x00A6},
	{0x3380, 0x01FB},
	{0x3382, 0x0000},
	{0x3384, 0x009E},
	{0x3386, 0x016B},
	{0x3388, 0x0015},
	{0x319A, 0x0005},
	{0x1006, 0x0005},
	{0x3416, 0x0001},
	{0x308C, 0x0008},
	{0x307C, 0x0240},
	{0x375E, 0x0050},
	{0x31CE, 0x0101},
	{0x374E, 0x0007},
	{0x3460, 0x0001},
	{0x3052, 0x0002},
	{0x3058, 0x0100},
	{0x6028, 0x2000},
	{0x602A, 0x108A},
	{0x6F12, 0x0359},
	{0x6F12, 0x0100},
	{0x6028, 0x4000},
	{0x1124, 0x4100},
	{0x1126, 0x0000},
	{0x112C, 0x4100},
	{0x112E, 0x0000},
	{0x3442, 0x0100},
	{0x3098, 0x0364}, //

	/* 4208*3120 */
	{0x6028, 0x2000},
	{0x602A, 0x0F74},
	{0x6F12, 0x0040},
	{0x6F12, 0x0040},
	{0x6028, 0x4000},
	{0x0344, 0x0008},
	{0x0346, 0x0008},
	{0x0348, 0x1077},
	{0x034A, 0x0C37},
	{0x034C, 0x1070},
	{0x034E, 0x0C30},
	{0x0900, 0x0011},
	{0x0380, 0x0001},
	{0x0382, 0x0001},
	{0x0384, 0x0001},
	{0x0386, 0x0001},
	{0x0400, 0x0000},
	{0x0404, 0x0010},
	{0x0114, 0x0300},
	{0x0110, 0x0002},
	{0x0136, 0x1800},
	{0x0304, 0x0006},
	{0x0306, 0x00B1},
	{0x0302, 0x0001},
	{0x0300, 0x0005},
	{0x030C, 0x0006},
	{0x030E, 0x0119},
	{0x030A, 0x0001},
	{0x0308, 0x0008},
	{0x0342, 0x16B0},
	{0x0340, 0x0CA2},
	{0x0202, 0x0200},
	{0x0200, 0x00C6},
	{0x0B04, 0x0101},
	{0x0B08, 0x0000},
	{0x0B00, 0x0007},
	{0x316A, 0x00A0},
	{REG_NULL, 0x0000},
};

static const struct s5k3l8xx_mode supported_modes[] = {
	{
		.width = 4208,
		.height = 3120,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0c00,
		.hts_def = 0x1320,
		.vts_def = 0x0ca8,
		.bpp = 10,
		.reg_list = s5k3l8xx_4208x3120_30fps_regs,
		.link_freq_idx = 0,
	},
};

static const s64 link_freq_items[] = {
	S5K3L8XX_LINK_FREQ_562MHZ,
};

static const char * const s5k3l8xx_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3"
};

/* Write registers up to 4 at a time */
static int s5k3l8xx_write_reg(struct i2c_client *client, u16 reg,
			     u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	dev_dbg(&client->dev, "write reg(0x%x val:0x%x)!\n", reg, val);

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

	if (i2c_master_send(client, buf, len + 2) != len + 2) {
		dev_err(&client->dev,
			"write reg(0x%x val:0x%x) failed !\n", reg, val);
		return -EIO;
	}

	return 0;
}

static int s5k3l8xx_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = s5k3l8xx_write_reg(client, regs[i].addr,
					S5K3L8XX_REG_VALUE_16BIT,
					regs[i].val);

	return ret;
}

/* Read registers up to 4 at a time */
static int s5k3l8xx_read_reg(struct i2c_client *client, u16 reg,
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
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev,
			"read reg:0x%x failed !\n", reg);
		return -EIO;
	}

	*val = be32_to_cpu(data_be);

	return 0;
}

static int s5k3l8xx_get_reso_dist(const struct s5k3l8xx_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct s5k3l8xx_mode *
s5k3l8xx_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = s5k3l8xx_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int s5k3l8xx_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);
	const struct s5k3l8xx_mode *mode;
	s64 h_blank, vblank_def;
	u64 pixel_rate = 0;
	u32 lane_num = S5K3L8XX_LANES;

	mutex_lock(&s5k3l8xx->mutex);

	mode = s5k3l8xx_find_best_fit(fmt);
	fmt->format.code = S5K3L8XX_MEDIA_BUS_FMT;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
#else
		mutex_unlock(&s5k3l8xx->mutex);
		return -ENOTTY;
#endif
	} else {
		s5k3l8xx->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(s5k3l8xx->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(s5k3l8xx->vblank, vblank_def,
					 S5K3L8XX_VTS_MAX - mode->height,
					 1, vblank_def);
		pixel_rate = (u32)link_freq_items[mode->link_freq_idx] / mode->bpp * 2 * lane_num;

		__v4l2_ctrl_s_ctrl_int64(s5k3l8xx->pixel_rate,
					 pixel_rate);
		__v4l2_ctrl_s_ctrl(s5k3l8xx->link_freq,
				   mode->link_freq_idx);
	}

	mutex_unlock(&s5k3l8xx->mutex);

	return 0;
}

static int s5k3l8xx_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *fmt)
{
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);
	const struct s5k3l8xx_mode *mode = s5k3l8xx->cur_mode;

	mutex_lock(&s5k3l8xx->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
#else
		mutex_unlock(&s5k3l8xx->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = S5K3L8XX_MEDIA_BUS_FMT;
		fmt->format.field = V4L2_FIELD_NONE;
	}
	mutex_unlock(&s5k3l8xx->mutex);

	return 0;
}

static int s5k3l8xx_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = S5K3L8XX_MEDIA_BUS_FMT;

	return 0;
}

static int s5k3l8xx_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != S5K3L8XX_MEDIA_BUS_FMT)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int s5k3l8xx_enable_test_pattern(struct s5k3l8xx *s5k3l8xx, u32 pattern)
{
	u32 val;

	if (pattern)
		val = (pattern - 1) | S5K3L8XX_TEST_PATTERN_ENABLE;
	else
		val = S5K3L8XX_TEST_PATTERN_DISABLE;

	return s5k3l8xx_write_reg(s5k3l8xx->client,
				 S5K3L8XX_REG_TEST_PATTERN,
				 S5K3L8XX_REG_VALUE_08BIT,
				 val);
}

static int s5k3l8xx_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);
	const struct s5k3l8xx_mode *mode = s5k3l8xx->cur_mode;

	mutex_lock(&s5k3l8xx->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&s5k3l8xx->mutex);

	return 0;
}

static void s5k3l8xx_get_module_inf(struct s5k3l8xx *s5k3l8xx,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, S5K3L8XX_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, s5k3l8xx->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, s5k3l8xx->len_name, sizeof(inf->base.lens));
}

static long s5k3l8xx_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		s5k3l8xx_get_module_inf(s5k3l8xx, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = s5k3l8xx_write_reg(s5k3l8xx->client,
				 S5K3L8XX_REG_CTRL_MODE,
				 S5K3L8XX_REG_VALUE_08BIT,
				 S5K3L8XX_MODE_STREAMING);
		else
			ret = s5k3l8xx_write_reg(s5k3l8xx->client,
				 S5K3L8XX_REG_CTRL_MODE,
				 S5K3L8XX_REG_VALUE_08BIT,
				 S5K3L8XX_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long s5k3l8xx_compat_ioctl32(struct v4l2_subdev *sd,
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

		ret = s5k3l8xx_ioctl(sd, cmd, inf);
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
			ret = s5k3l8xx_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = s5k3l8xx_ioctl(sd, cmd, &stream);
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

static int __s5k3l8xx_start_stream(struct s5k3l8xx *s5k3l8xx)
{
	int ret;

	ret = s5k3l8xx_write_array(s5k3l8xx->client, s5k3l8xx->cur_mode->reg_list);
	if (ret)
		return ret;

	/* In case these controls are set before streaming */
	mutex_unlock(&s5k3l8xx->mutex);
	ret = v4l2_ctrl_handler_setup(&s5k3l8xx->ctrl_handler);
	mutex_lock(&s5k3l8xx->mutex);
	if (ret)
		return ret;

	s5k3l8xx_write_reg(s5k3l8xx->client,
				 S5K3L8XX_REG_STREAM_ON,
				 S5K3L8XX_REG_VALUE_08BIT,
				 S5K3L8XX_MODE_STREAMING);
	s5k3l8xx_write_reg(s5k3l8xx->client,
				 S5K3L8XX_REG_CTRL_MODE,
				 S5K3L8XX_REG_VALUE_08BIT,
				 S5K3L8XX_MODE_STREAMING);
	s5k3l8xx_write_reg(s5k3l8xx->client,
				 S5K3L8XX_REG_STREAM_ON,
				 S5K3L8XX_REG_VALUE_08BIT,
				 S5K3L8XX_MODE_SW_STANDBY);
	return 0;
}

static int __s5k3l8xx_stop_stream(struct s5k3l8xx *s5k3l8xx)
{
	return s5k3l8xx_write_reg(s5k3l8xx->client,
				 S5K3L8XX_REG_CTRL_MODE,
				 S5K3L8XX_REG_VALUE_08BIT,
				 S5K3L8XX_MODE_SW_STANDBY);
}

static int s5k3l8xx_s_stream(struct v4l2_subdev *sd, int on)
{
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);
	struct i2c_client *client = s5k3l8xx->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				s5k3l8xx->cur_mode->width,
				s5k3l8xx->cur_mode->height,
		DIV_ROUND_CLOSEST(s5k3l8xx->cur_mode->max_fps.denominator,
				  s5k3l8xx->cur_mode->max_fps.numerator));

	mutex_lock(&s5k3l8xx->mutex);
	on = !!on;
	if (on == s5k3l8xx->streaming)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __s5k3l8xx_start_stream(s5k3l8xx);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__s5k3l8xx_stop_stream(s5k3l8xx);
		pm_runtime_put(&client->dev);
	}

	s5k3l8xx->streaming = on;

unlock_and_return:
	mutex_unlock(&s5k3l8xx->mutex);

	return ret;
}

static int s5k3l8xx_s_power(struct v4l2_subdev *sd, int on)
{
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);
	struct i2c_client *client = s5k3l8xx->client;
	int ret = 0;

	mutex_lock(&s5k3l8xx->mutex);

	/* If the power state is not modified - no work to do. */
	if (s5k3l8xx->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		s5k3l8xx->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		s5k3l8xx->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&s5k3l8xx->mutex);

	return ret;
}

/* Calculate the delay in us by clock rate and clock cycles */
static inline u32 s5k3l8xx_cal_delay(u32 cycles)
{
	return DIV_ROUND_UP(cycles, S5K3L8XX_XVCLK_FREQ / 1000 / 1000);
}

static int __s5k3l8xx_power_on(struct s5k3l8xx *s5k3l8xx)
{
	int ret;
	u32 delay_us;
	struct device *dev = &s5k3l8xx->client->dev;

	if (!IS_ERR(s5k3l8xx->power_gpio))
		gpiod_set_value_cansleep(s5k3l8xx->power_gpio, 1);

	usleep_range(1000, 2000);

	if (!IS_ERR_OR_NULL(s5k3l8xx->pins_default)) {
		ret = pinctrl_select_state(s5k3l8xx->pinctrl,
					   s5k3l8xx->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(s5k3l8xx->xvclk, S5K3L8XX_XVCLK_FREQ);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (24MHz)\n");
	if (clk_get_rate(s5k3l8xx->xvclk) != S5K3L8XX_XVCLK_FREQ)
		dev_warn(dev, "xvclk mismatched, modes are based on 24MHz\n");
	ret = clk_prepare_enable(s5k3l8xx->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	ret = regulator_bulk_enable(S5K3L8XX_NUM_SUPPLIES, s5k3l8xx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	/* need reset after all vdd power on */
	usleep_range(2000, 2100);

	if (!IS_ERR(s5k3l8xx->reset_gpio))
		gpiod_set_value_cansleep(s5k3l8xx->reset_gpio, 0);

	usleep_range(1000, 1100);

	if (!IS_ERR(s5k3l8xx->reset_gpio))
		gpiod_set_value_cansleep(s5k3l8xx->reset_gpio, 1);

	usleep_range(500, 1000);
	if (!IS_ERR(s5k3l8xx->pwdn_gpio))
		gpiod_set_value_cansleep(s5k3l8xx->pwdn_gpio, 1);

	/* 23000 cycles prior to first SCCB transaction */
	delay_us = s5k3l8xx_cal_delay(23000);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(s5k3l8xx->xvclk);

	return ret;
}

static void __s5k3l8xx_power_off(struct s5k3l8xx *s5k3l8xx)
{
	int ret;
	struct device *dev = &s5k3l8xx->client->dev;

	if (!IS_ERR(s5k3l8xx->pwdn_gpio))
		gpiod_set_value_cansleep(s5k3l8xx->pwdn_gpio, 0);
	clk_disable_unprepare(s5k3l8xx->xvclk);
	if (!IS_ERR(s5k3l8xx->reset_gpio))
		gpiod_set_value_cansleep(s5k3l8xx->reset_gpio, 0);

	if (!IS_ERR_OR_NULL(s5k3l8xx->pins_sleep)) {
		ret = pinctrl_select_state(s5k3l8xx->pinctrl,
					   s5k3l8xx->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	if (!IS_ERR(s5k3l8xx->power_gpio))
		gpiod_set_value_cansleep(s5k3l8xx->power_gpio, 0);

	regulator_bulk_disable(S5K3L8XX_NUM_SUPPLIES, s5k3l8xx->supplies);
}

static int __maybe_unused s5k3l8xx_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);

	return __s5k3l8xx_power_on(s5k3l8xx);
}

static int __maybe_unused s5k3l8xx_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);

	__s5k3l8xx_power_off(s5k3l8xx);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int s5k3l8xx_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct s5k3l8xx_mode *def_mode = &supported_modes[0];

	mutex_lock(&s5k3l8xx->mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = S5K3L8XX_MEDIA_BUS_FMT;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&s5k3l8xx->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int s5k3l8xx_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_state *sd_state,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = S5K3L8XX_MEDIA_BUS_FMT;

	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;

	return 0;
}

static int s5k3l8xx_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->bus.mipi_csi2.num_data_lanes = S5K3L8XX_LANES;

	return 0;
}

#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_2096 2096
#define DST_HEIGHT_1560 1560

static int s5k3l8xx_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		if (s5k3l8xx->cur_mode->width == 2104) {
			sel->r.left = CROP_START(s5k3l8xx->cur_mode->width, DST_WIDTH_2096);
			sel->r.width = DST_WIDTH_2096;
			sel->r.top = CROP_START(s5k3l8xx->cur_mode->height, DST_HEIGHT_1560);
			sel->r.height = DST_HEIGHT_1560;
		} else {
			sel->r.left = CROP_START(s5k3l8xx->cur_mode->width,
							s5k3l8xx->cur_mode->width);
			sel->r.width = s5k3l8xx->cur_mode->width;
			sel->r.top = CROP_START(s5k3l8xx->cur_mode->height,
							s5k3l8xx->cur_mode->height);
			sel->r.height = s5k3l8xx->cur_mode->height;
		}
		return 0;
	}

	return -EINVAL;
}

static const struct dev_pm_ops s5k3l8xx_pm_ops = {
	SET_RUNTIME_PM_OPS(s5k3l8xx_runtime_suspend,
			   s5k3l8xx_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops s5k3l8xx_internal_ops = {
	.open = s5k3l8xx_open,
};
#endif

static const struct v4l2_subdev_core_ops s5k3l8xx_core_ops = {
	.s_power = s5k3l8xx_s_power,
	.ioctl = s5k3l8xx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = s5k3l8xx_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops s5k3l8xx_video_ops = {
	.s_stream = s5k3l8xx_s_stream,
	.g_frame_interval = s5k3l8xx_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops s5k3l8xx_pad_ops = {
	.enum_mbus_code = s5k3l8xx_enum_mbus_code,
	.enum_frame_size = s5k3l8xx_enum_frame_sizes,
	.enum_frame_interval = s5k3l8xx_enum_frame_interval,
	.get_fmt = s5k3l8xx_get_fmt,
	.set_fmt = s5k3l8xx_set_fmt,
	.get_selection = s5k3l8xx_get_selection,
	.get_mbus_config = s5k3l8xx_g_mbus_config,
};

static const struct v4l2_subdev_ops s5k3l8xx_subdev_ops = {
	.core	= &s5k3l8xx_core_ops,
	.video	= &s5k3l8xx_video_ops,
	.pad	= &s5k3l8xx_pad_ops,
};

static int s5k3l8xx_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5k3l8xx *s5k3l8xx = container_of(ctrl->handler,
					     struct s5k3l8xx, ctrl_handler);
	struct i2c_client *client = s5k3l8xx->client;
	s64 max;
	int ret = 0;

	/* Propagate change of current control to all related controls */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = s5k3l8xx->cur_mode->height + ctrl->val - 4;
		__v4l2_ctrl_modify_range(s5k3l8xx->exposure,
					 s5k3l8xx->exposure->minimum, max,
					 s5k3l8xx->exposure->step,
					 s5k3l8xx->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		/* 4 least significant bits of expsoure are fractional part */
		ret = s5k3l8xx_write_reg(s5k3l8xx->client,
					S5K3L8XX_REG_EXPOSURE,
					S5K3L8XX_REG_VALUE_16BIT,
					ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = s5k3l8xx_write_reg(s5k3l8xx->client,
					S5K3L8XX_REG_ANALOG_GAIN,
					S5K3L8XX_REG_VALUE_16BIT,
					ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		ret = s5k3l8xx_write_reg(s5k3l8xx->client,
					S5K3L8XX_REG_VTS,
					S5K3L8XX_REG_VALUE_16BIT,
					ctrl->val + s5k3l8xx->cur_mode->height);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = s5k3l8xx_enable_test_pattern(s5k3l8xx, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops s5k3l8xx_ctrl_ops = {
	.s_ctrl = s5k3l8xx_set_ctrl,
};

static int s5k3l8xx_initialize_controls(struct s5k3l8xx *s5k3l8xx)
{
	const struct s5k3l8xx_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_pixel_rate = 0;
	u32 lane_num = S5K3L8XX_LANES;

	handler = &s5k3l8xx->ctrl_handler;
	mode = s5k3l8xx->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;
	handler->lock = &s5k3l8xx->mutex;

	s5k3l8xx->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			V4L2_CID_LINK_FREQ,
			1, 0, link_freq_items);

	dst_pixel_rate = (u32)link_freq_items[mode->link_freq_idx] / mode->bpp * 2 * lane_num;

	s5k3l8xx->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
			V4L2_CID_PIXEL_RATE,
			0, S5K3L8XX_PIXEL_RATE,
			1, dst_pixel_rate);

	__v4l2_ctrl_s_ctrl(s5k3l8xx->link_freq,
			   mode->link_freq_idx);

	h_blank = mode->hts_def - mode->width;
	s5k3l8xx->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				h_blank, h_blank, 1, h_blank);
	if (s5k3l8xx->hblank)
		s5k3l8xx->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = mode->vts_def - mode->height;
	s5k3l8xx->vblank = v4l2_ctrl_new_std(handler, &s5k3l8xx_ctrl_ops,
				V4L2_CID_VBLANK, vblank_def,
				S5K3L8XX_VTS_MAX - mode->height,
				1, vblank_def);

	exposure_max = mode->vts_def - 4;
	s5k3l8xx->exposure = v4l2_ctrl_new_std(handler, &s5k3l8xx_ctrl_ops,
				V4L2_CID_EXPOSURE, S5K3L8XX_EXPOSURE_MIN,
				exposure_max, S5K3L8XX_EXPOSURE_STEP,
				mode->exp_def);

	s5k3l8xx->anal_gain = v4l2_ctrl_new_std(handler, &s5k3l8xx_ctrl_ops,
				V4L2_CID_ANALOGUE_GAIN, S5K3L8XX_GAIN_MIN,
				S5K3L8XX_GAIN_MAX, S5K3L8XX_GAIN_STEP,
				S5K3L8XX_GAIN_DEFAULT);

	s5k3l8xx->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&s5k3l8xx_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(s5k3l8xx_test_pattern_menu) - 1,
				0, 0, s5k3l8xx_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_err(&s5k3l8xx->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	s5k3l8xx->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int s5k3l8xx_check_sensor_id(struct s5k3l8xx *s5k3l8xx,
				   struct i2c_client *client)
{
	struct device *dev = &s5k3l8xx->client->dev;
	u32 id = 0;
	int ret;

	client->addr = S5K3L8XX_MAJOR_I2C_ADDR;

	ret = s5k3l8xx_read_reg(client, S5K3L8XX_REG_CHIP_ID,
			       S5K3L8XX_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%04x), ret(%d)\n", id, ret);
		client->addr = S5K3L8XX_MINOR_I2C_ADDR;
		ret = s5k3l8xx_read_reg(client, S5K3L8XX_REG_CHIP_ID,
				       S5K3L8XX_REG_VALUE_16BIT, &id);
		if (id != CHIP_ID) {
			dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
			return -ENODEV;
		}
	}

	ret = s5k3l8xx_read_reg(client, S5K3L8XX_CHIP_REVISION_REG,
			       S5K3L8XX_REG_VALUE_08BIT, &id);
	if (ret) {
		dev_err(dev, "Read chip revision register error\n");
		return ret;
	}

	dev_info(dev, "Detected Samsung %04x sensor, REVISION 0x%x\n", CHIP_ID, id);

	return 0;
}

static int s5k3l8xx_configure_regulators(struct s5k3l8xx *s5k3l8xx)
{
	unsigned int i;

	for (i = 0; i < S5K3L8XX_NUM_SUPPLIES; i++)
		s5k3l8xx->supplies[i].supply = s5k3l8xx_supply_names[i];

	return devm_regulator_bulk_get(&s5k3l8xx->client->dev,
				       S5K3L8XX_NUM_SUPPLIES,
				       s5k3l8xx->supplies);
}

static int s5k3l8xx_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct s5k3l8xx *s5k3l8xx;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	s5k3l8xx = devm_kzalloc(dev, sizeof(*s5k3l8xx), GFP_KERNEL);
	if (!s5k3l8xx)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &s5k3l8xx->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &s5k3l8xx->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &s5k3l8xx->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &s5k3l8xx->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	s5k3l8xx->client = client;
	s5k3l8xx->cur_mode = &supported_modes[0];

	s5k3l8xx->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(s5k3l8xx->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	s5k3l8xx->power_gpio = devm_gpiod_get(dev, "power", GPIOD_OUT_LOW);
	if (IS_ERR(s5k3l8xx->power_gpio))
		dev_warn(dev, "Failed to get power-gpios, maybe no use\n");

	s5k3l8xx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(s5k3l8xx->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	s5k3l8xx->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(s5k3l8xx->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	ret = s5k3l8xx_configure_regulators(s5k3l8xx);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	s5k3l8xx->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(s5k3l8xx->pinctrl)) {
		s5k3l8xx->pins_default =
			pinctrl_lookup_state(s5k3l8xx->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(s5k3l8xx->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		s5k3l8xx->pins_sleep =
			pinctrl_lookup_state(s5k3l8xx->pinctrl,
					     OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(s5k3l8xx->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	}

	mutex_init(&s5k3l8xx->mutex);

	sd = &s5k3l8xx->subdev;
	v4l2_i2c_subdev_init(sd, client, &s5k3l8xx_subdev_ops);
	ret = s5k3l8xx_initialize_controls(s5k3l8xx);
	if (ret)
		goto err_destroy_mutex;

	ret = __s5k3l8xx_power_on(s5k3l8xx);
	if (ret)
		goto err_free_handler;

	ret = s5k3l8xx_check_sensor_id(s5k3l8xx, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &s5k3l8xx_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	s5k3l8xx->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &s5k3l8xx->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(s5k3l8xx->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 s5k3l8xx->module_index, facing,
		 S5K3L8XX_NAME, dev_name(sd->dev));
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
	__s5k3l8xx_power_off(s5k3l8xx);
err_free_handler:
	v4l2_ctrl_handler_free(&s5k3l8xx->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&s5k3l8xx->mutex);

	return ret;
}

static void s5k3l8xx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k3l8xx *s5k3l8xx = to_s5k3l8xx(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&s5k3l8xx->ctrl_handler);
	mutex_destroy(&s5k3l8xx->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__s5k3l8xx_power_off(s5k3l8xx);
	pm_runtime_set_suspended(&client->dev);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id s5k3l8xx_of_match[] = {
	{ .compatible = "samsung,s5k3l8xx" },
	{},
};
MODULE_DEVICE_TABLE(of, s5k3l8xx_of_match);
#endif

static const struct i2c_device_id s5k3l8xx_match_id[] = {
	{ "samsung,s5k3l8xx", 0 },
	{},
};

static struct i2c_driver s5k3l8xx_i2c_driver = {
	.driver = {
		.name = S5K3L8XX_NAME,
		.pm = &s5k3l8xx_pm_ops,
		.of_match_table = of_match_ptr(s5k3l8xx_of_match),
	},
	.probe		= &s5k3l8xx_probe,
	.remove		= &s5k3l8xx_remove,
	.id_table	= s5k3l8xx_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&s5k3l8xx_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&s5k3l8xx_i2c_driver);
}

device_initcall_sync(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Samsung s5k3l8xx sensor driver");
MODULE_LICENSE("GPL");
