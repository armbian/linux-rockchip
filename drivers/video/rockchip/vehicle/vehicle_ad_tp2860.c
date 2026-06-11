// SPDX-License-Identifier: GPL-2.0
/*
 * vehicle sensor tp2860
 *
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 * Authors:
 *      wpzz <randy.wang@rock-chips.com>
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include "vehicle_cfg.h"
#include "vehicle_main.h"
#include "vehicle_ad.h"
#include "vehicle_ad_tp2860.h"
#include <linux/err.h>

enum {
	TP2860_CVSTD_720P_60 = 0,
	TP2860_CVSTD_720P_50,
	TP2860_CVSTD_1080P_30,
	TP2860_CVSTD_1080P_25,
	TP2860_CVSTD_720P_30,
	TP2860_CVSTD_720P_25,
	TP2860_CVSTD_SD,
	TP2860_CVSTD_OTHER,
	TP2860_CVSTD_PAL,
	TP2860_CVSTD_NTSC,
};

enum {
	CVSTD_720P60 = 0,
	CVSTD_720P50,
	CVSTD_1080P30,
	CVSTD_1080P25,
	CVSTD_720P30,
	CVSTD_720P25,
	CVSTD_SVGAP30,
	CVSTD_SD,
	CVSTD_NTSC,
	CVSTD_PAL
};

enum {
	FORCE_PAL_WIDTH = 960,
	FORCE_PAL_HEIGHT = 576,
	FORCE_NTSC_WIDTH = 960,
	FORCE_NTSC_HEIGHT = 480,
	FORCE_SVGA_WIDTH = 800,
	FORCE_SVGA_HEIGHT = 600,
	FORCE_720P_WIDTH = 1280,
	FORCE_720P_HEIGHT = 720,
	FORCE_1080P_WIDTH = 1920,
	FORCE_1080P_HEIGHT = 1080,
	FORCE_CIF_OUTPUT_FORMAT = CIF_OUTPUT_FORMAT_420,
};

enum support_mipi_lane {
	MIPI_2LANE,
	MIPI_1LANE,
};

#define FLAG_LOSS				(0x1 << 7)
#define FLAG_V_LOCKED			(0x1 << 6)
#define FLAG_H_LOCKED			(0x1 << 5)
#define FLAG_CARRIER_PLL_LOCKED	(0x1 << 4)
#define FLAG_VIDEO_DETECTED		(0x1 << 3)
#define FLAG_EQ_SD_DETECTED		(0x1 << 2)
#define FLAG_PROGRESSIVE		(0x1 << 1)
#define FLAG_NO_CARRIER			(0x1 << 0)
#define FLAG_LOCKED		(FLAG_V_LOCKED | FLAG_H_LOCKED | FLAG_CARRIER_PLL_LOCKED)

enum {
	VIDEO_UNPLUG,
	VIDEO_IN,
	VIDEO_LOCKED,
	VIDEO_UNLOCK
};

#define TP2860_LINK_FREQ_74M			(74250000UL)
#define TP2860_LINK_FREQ_148M			(148500000UL)
#define TP2860_LINK_FREQ_297M			(297000000UL)

static struct vehicle_ad_dev *tp2860_g_addev;
static int cvstd_mode = CVSTD_PAL;
//static int cvstd_old = CVSTD_720P25;
static int cvstd_old = CVSTD_NTSC;

//static int cvstd_sd = CVSTD_NTSC;
static int cvstd_state = VIDEO_UNPLUG;
static int cvstd_old_state = VIDEO_UNLOCK;
static int video_mode;
static int video_old;

static bool g_tp2860_streaming;

#define SENSOR_REGISTER_LEN	1	/* sensor register address bytes*/
#define SENSOR_VALUE_LEN	1	/* sensor register value bytes*/
#define PAGE_REG			0x40
#define STD_HDA 1
#define CVBS_960H			(1) //1->960H 0->720H

const char * const tp2860_supply_names[] = {
	"avdd-supply",	/* Digital I/O power */
	"dovdd-supply",	/* Analog power */
	"dvdd-supply",	/* Digital core power */
};
#define NUM_SUPPLIES ARRAY_SIZE(tp2860_supply_names)
struct regulator_bulk_data tp2860_supplies[NUM_SUPPLIES];

struct rk_sensor_reg {
	unsigned int reg;
	unsigned int val;
};

#define SENSOR_CHANNEL_REG		0x41

#define SEQCMD_END  0xFF000000
#define SensorEnd   {SEQCMD_END, 0x00}

#define SENSOR_DG(format, ...) VEHICLE_DBG(VEHICLE_DBG_SENSOR, format, ## __VA_ARGS__)

#define SENSOR_ID(_msb, _lsb)		((_msb) << 8 | (_lsb))

/* NTSC Preview resolution setting*/
static struct rk_sensor_reg sensor_preview_data_ntsc_30hz[] = {
	{0x40, 0x00},
	{0x02, 0x47},
	{0x07, 0x40},
	{0x0b, 0x40},
	{0x0C, 0x13},
	{0x0D, 0x50},
	{0x15, 0x13},
	{0x16, 0x60},
	{0x17, 0x80},
	{0x18, 0x12},
	{0x19, 0xF0},
	{0x1A, 0x07},
	{0x1C, 0x09},
	{0x1D, 0x38},
	{0x20, 0x40},
	{0x21, 0x84},
	{0x22, 0x36},
	{0x23, 0x3C},
	{0x25, 0xfe},
	{0x26, 0x0c},
	{0x2B, 0x70},
	{0x2C, 0x2A},
	{0x2D, 0x68},
	{0x2E, 0x57},
	{0x30, 0x62},
	{0x31, 0xBB},
	{0x32, 0x96},
	{0x33, 0xC0},
	{0x35, 0x65},
	{0x38, 0x00},
	{0x39, 0x04},
	{0x4E, 0x00},
	{0x4F, 0x01},
	{0xFB, 0x10},
	{0xFC, 0x0E},
	{0x40, 0x08}, // mipi,pll page
	{0x28, 0x02}, // stream off
	{0x02, 0x79},
	{0x03, 0x71},
	{0x04, 0x71},
	{0x20, 0x00},
	{0x23, 0x9e},
	{0x12, 0x54},
	{0x13, 0xef},
	{0x14, 0x62},
	{0x15, 0x07},
	{0x21, 0x12},
	{0x22, 0x11},
	{0x2a, 0x08},
	{0x2b, 0x01},
	{0x2c, 0x0e},
	{0x2e, 0x02},
	{0x10, 0xa0},
	{0x10, 0x20},
	{0x40, 0x00},

	SensorEnd
};

/* Pal Preview resolution setting*/
static struct rk_sensor_reg sensor_preview_data_pal_25hz[] = {
	{0x40, 0x00},
	{0x02, 0x47},
	{0x0c, 0x13},
	{0x0d, 0x51},
	{0x15, 0x13},
	{0x16, 0x77},
	{0x17, 0x80},
	{0x18, 0x17},
	{0x19, 0x20},
	{0x1a, 0x17},
	{0x1c, 0x09},
	{0x1d, 0x48},
	{0x20, 0x48},
	{0x21, 0x84},
	{0x22, 0x37},
	{0x23, 0x3f},
	{0x2b, 0x70},
	{0x2c, 0x2a},
	{0x2d, 0x64},
	{0x2e, 0x56},
	{0x30, 0x7a},
	{0x31, 0x4a},
	{0x32, 0x4d},
	{0x33, 0xf0},
	{0x35, 0x65},
	{0x38, 0x00},
	{0x39, 0x04},

	/* mipi setting */
	{0x40, 0x08}, //select MIPI page
	{0x28, 0x02}, // stream off
	{0x02, 0x7d},
	{0x03, 0x75},
	{0x04, 0x75},
	{0x13, 0xef},
	{0x20, 0x00},
	{0x23, 0x9e},
	/* 2lane */
	{0x21, 0x12},
	{0x14, 0x62},
	{0x15, 0x07},
	{0x2a, 0x02},
	{0x2b, 0x00},
	{0x2c, 0x03},
	{0x2e, 0x02},
	{0x10, 0xa0},
	{0x10, 0x20},
	{0x40, 0x00},

	SensorEnd
};

/* 720p@25FPS Preview resolution setting*/
static struct rk_sensor_reg sensor_preview_data_720p_25hz[] = {
	{0x40, 0x00},
	{0x02, 0x46},
	{0x07, 0xc0},
	{0x0b, 0xc0},
	{0x0C, 0x13},
	{0x0D, 0x71},
	{0x15, 0x13},
	{0x16, 0x15},
	{0x17, 0x00},
	{0x18, 0x1b},
	{0x19, 0xd0},
	{0x1A, 0x25},
	{0x1C, 0x07},
	{0x1D, 0xbc},
	{0x20, 0x40},
	{0x21, 0x46},
	{0x22, 0x36},
	{0x23, 0x3C},
	{0x2B, 0x60},
	{0x2C, 0x3A},
	{0x2D, 0x5a},
	{0x2E, 0x40},
	{0x30, 0x9e},
	{0x31, 0x20},
	{0x32, 0x10},
	{0x33, 0x90},
	{0x35, 0x25},
	{0x38, 0x00},
	{0x39, 0x18},
	{0x40, 0x08},
	{0x28, 0x02}, // stream off
	{0x02, 0x79},
	{0x03, 0x71},
	{0x04, 0x71},
	{0x10, 0x00},
	{0x11, 0x47},
	{0x12, 0x40},
	{0x20, 0x00},
	{0x23, 0x9e},
	{0x12, 0x54},
	{0x13, 0xef},
	{0x14, 0x41},
	{0x15, 0x12},
	{0x21, 0x12},
	{0x2a, 0x08},
	{0x2b, 0x01},
	{0x2c, 0x0e},
	{0x2e, 0x02},
	{0x42, 0x08},
	{0x4f, 0x00},
	{0x54, 0x04},
	{0xfb, 0x00},
	{0xfc, 0x00},
	{0x10, 0xa0},
	{0x10, 0x20},
	{0x40, 0x00},
	//{0x41, 0x01},
	{0x40, 0x00},
	SensorEnd
};

/* 720p@30FPS Preview resolution setting*/
static struct rk_sensor_reg sensor_preview_data_720p_30hz[] = {
	{0x40, 0x00},
	{0x02, 0x46},
	{0x07, 0xc0},
	{0x0b, 0xc0},
	{0x0C, 0x13},
	{0x0D, 0x70},
	{0x15, 0x13},
	{0x16, 0x15},
	{0x17, 0x00},
	{0x18, 0x1b},
	{0x19, 0xd0},
	{0x1A, 0x25},
	{0x1C, 0x06}, // 1280x720
	{0x1D, 0xbc},
	{0x20, 0x40},
	{0x21, 0x46},
	{0x22, 0x36},
	{0x23, 0x3C},
	{0x2B, 0x60},
	{0x2C, 0x3A},
	{0x2D, 0x5a},
	{0x2E, 0x40},
	{0x30, 0x9d},
	{0x31, 0xca},
	{0x32, 0x01},
	{0x33, 0xd0},
	{0x35, 0x25},
	{0x38, 0x00},
	{0x39, 0x18},
	{0x40, 0x08}, // mipi,pll page
	{0x28, 0x02}, // stream off
	{0x02, 0x79},
	{0x03, 0x71},
	{0x04, 0x71},
	{0x20, 0x00},
	{0x23, 0x9e},
	{0x12, 0x54},
	{0x13, 0xef},
	{0x14, 0x41},
	{0x15, 0x12},
	{0x2a, 0x08},
	{0x2b, 0x01},
	{0x2c, 0x0e},
	{0x2e, 0x02},
	{0x10, 0xa0},
	{0x10, 0x20},
	{0x40, 0x00},

	SensorEnd
};

/* 1080p@25fps Preview resolution setting*/
static struct rk_sensor_reg sensor_preview_data_1080p_25hz[] = {
	{0x40, 0x00},
	{0x42, 0x00},
	{0x4e, 0x00},
	{0x54, 0x00},
	//{0x41, 0x01},
	{0x02, 0x40},
	{0x07, 0xc0},
	{0x0b, 0xc0},
	{0x0c, 0x03},
	{0x0d, 0x50},
	{0x15, 0x03},
	{0x16, 0xd2},
	{0x17, 0x80},
	{0x18, 0x29},
	{0x19, 0x38},
	{0x1a, 0x47},
	{0x1c, 0x0a},
	{0x1d, 0x50},
	{0x20, 0x30},
	{0x21, 0x84},
	{0x22, 0x36},
	{0x23, 0x3c},
	{0x2b, 0x60},
	{0x2c, 0x0a},
	{0x2d, 0x30},
	{0x2e, 0x70},
	{0x30, 0x48},
	{0x31, 0xbb},
	{0x32, 0x2e},
	{0x33, 0x90},
	{0x35, 0x05},
	{0x38, 0x00},
	{0x39, 0x1C},
	{0x02, 0x44},
	{0x0d, 0x73},
	{0x15, 0x01},
	{0x16, 0xf0},
	{0x20, 0x3c},
	{0x21, 0x46},
	{0x25, 0xfe},
	{0x26, 0x0d},
	{0x2c, 0x3a},
	{0x2d, 0x54},
	{0x2e, 0x40},
	{0x30, 0xa5},
	{0x31, 0x86},
	{0x32, 0xfb},
	{0x33, 0x60},
	{0x40, 0x08},
	{0x28, 0x02}, // stream off
	{0x02, 0x79},
	{0x03, 0x71},
	{0x04, 0x71},
	{0x13, 0xef},
	{0x20, 0x00},
	{0x21, 0x12}, //2lane
	{0x23, 0x9e},

	{0x14, 0x41},
	{0x15, 0x02},
	{0x2a, 0x08},
	{0x2b, 0x04},
	{0x2c, 0x0c},
	{0x2e, 0x02},

	{0x10, 0xa0},
	{0x10, 0x20},
	{0x40, 0x00},

	SensorEnd
};

/* 1080p@30FPS Preview resolution setting*/
static struct rk_sensor_reg sensor_preview_data_1080p_30hz[] = {
	//TP9951 AHD 1080p30
	{0x40, 0x00},
	{0x02, 0x44},
	{0x07, 0xc0},
	{0x0b, 0xc0},
	{0x0C, 0x03},
	{0x0D, 0x72},
	{0x15, 0x01},
	{0x16, 0xf0},
	{0x17, 0x80},
	{0x18, 0x2a},
	{0x19, 0x38},
	{0x1A, 0x47},
	{0x1C, 0x08},   // 1920x1080,30fps
	{0x1D, 0x98},
	{0x20, 0x38},
	{0x21, 0x46},
	{0x22, 0x36},
	{0x23, 0x3C},
	{0x2B, 0x60},
	{0x2C, 0x3A},
	{0x2D, 0x54},
	{0x2E, 0x40},
	{0x30, 0xa5},
	{0x31, 0x95},
	{0x32, 0xe0},
	{0x33, 0x60},
	{0x35, 0x05},
	{0x38, 0x00},
	{0x39, 0x1c},
	{0x40, 0x08}, // mipi,pll page
	{0x28, 0x02}, //stream off
	{0x02, 0x79},
	{0x03, 0x71},
	{0x04, 0x71},
	{0x20, 0x00},
	{0x21, 0x12}, //2lane
	{0x23, 0x9e},
	{0x12, 0x54},
	{0x13, 0xef},
	{0x14, 0x41},
	{0x15, 0x02},
	{0x2a, 0x08},
	{0x2b, 0x04},
	{0x2c, 0x0c},
	{0x2e, 0x02},
	{0x10, 0xa0},
	{0x10, 0x20},
	{0x40, 0x00},

	SensorEnd
};

/* common setting */
static struct rk_sensor_reg sensor_common_reg[] = {
	{0x40, 0x00}, // select decoder page
	{0x06, 0x12}, // default value
	{0x42, 0x00}, // common setting for all format
	{0x4e, 0x00}, // common setting for MIPI output
	{0x54, 0x00}, // common setting for MIPI output
	SensorEnd
};

/* color params */
static struct rk_sensor_reg sensor_color_reg[] = {
	{0x40, 0x00}, // select decoder page
	{0x10, 0x7c}, // brightness
	{0x11, 0x4c}, // contrast
	{0x12, 0x2E}, // saturation
	{0x13, 0x00}, // hue
	{0x14, 0x00}, // sharpness
	SensorEnd
};

static void tp2860_reinit_parameter(struct vehicle_ad_dev *ad, unsigned char cvstd)
{
	int i = 0;

	switch (cvstd) {
	case CVSTD_PAL:
		ad->cfg.width = FORCE_PAL_WIDTH;
		ad->cfg.height = FORCE_PAL_HEIGHT;
		ad->cfg.start_x = 0;
		ad->cfg.start_y = 0;
		ad->cfg.input_format = CIF_INPUT_FORMAT_PAL;
		ad->cfg.output_format = FORCE_CIF_OUTPUT_FORMAT;
		ad->cfg.field_order = 1;
		ad->cfg.yuv_order = 0;/*00 - UYVY*/
		ad->cfg.href = 0;
		ad->cfg.vsync = 0;
		ad->cfg.frame_rate = 25;//25	 30
		ad->cfg.mipi_freq = TP2860_LINK_FREQ_74M;
		break;
	case CVSTD_NTSC:
		ad->cfg.width = FORCE_NTSC_WIDTH;
		ad->cfg.height = FORCE_NTSC_HEIGHT;
		ad->cfg.start_x = 0;
		ad->cfg.start_y = 0;
		ad->cfg.input_format = CIF_INPUT_FORMAT_NTSC;
		ad->cfg.output_format = FORCE_CIF_OUTPUT_FORMAT;
		ad->cfg.field_order = 1;
		ad->cfg.yuv_order = 0;/*00 - UYVY*/
		ad->cfg.href = 0;
		ad->cfg.vsync = 0;
		ad->cfg.frame_rate = 30;//25	 30
		ad->cfg.mipi_freq = TP2860_LINK_FREQ_74M;
		break;
	case CVSTD_720P25:
		ad->cfg.width = 1280;
		ad->cfg.height = 720;
		ad->cfg.start_x = 0;
		ad->cfg.start_y = 0;
		ad->cfg.input_format = CIF_INPUT_FORMAT_YUV;
		ad->cfg.output_format = FORCE_CIF_OUTPUT_FORMAT;
		ad->cfg.field_order = 0;
		ad->cfg.yuv_order = 0;/*00 - UYVY*/
		ad->cfg.href = 0;
		ad->cfg.vsync = 0;
		ad->cfg.frame_rate = 25;
		ad->cfg.mipi_freq = TP2860_LINK_FREQ_148M;
		break;

	case CVSTD_1080P25:
		ad->cfg.width = 1920;
		ad->cfg.height = 1080;
		ad->cfg.start_x = 0;
		ad->cfg.start_y = 0;
		ad->cfg.input_format = CIF_INPUT_FORMAT_YUV;
		ad->cfg.output_format = FORCE_CIF_OUTPUT_FORMAT;
		ad->cfg.field_order = 0;
		ad->cfg.yuv_order = 0;/*00 - UYVY*/
		ad->cfg.href = 0;
		ad->cfg.vsync = 0;
		ad->cfg.frame_rate = 25;
		ad->cfg.mipi_freq = TP2860_LINK_FREQ_297M;
		break;

	default:
		ad->cfg.width = 1920;
		ad->cfg.height = 1080;
		ad->cfg.start_x = 0;
		ad->cfg.start_y = 0;
		ad->cfg.input_format = CIF_INPUT_FORMAT_YUV;
		ad->cfg.output_format = FORCE_CIF_OUTPUT_FORMAT;
		ad->cfg.field_order = 0;
		ad->cfg.yuv_order = 0;/*00 - UYVY*/
		ad->cfg.href = 0;
		ad->cfg.vsync = 0;
		ad->cfg.frame_rate = 25;
		ad->cfg.mipi_freq = TP2860_LINK_FREQ_297M;
		break;
	}
	ad->cfg.type = V4L2_MBUS_CSI2_DPHY;
	ad->cfg.mbus_flags = V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CONTINUOUS_CLOCK |
			 V4L2_MBUS_CSI2_CHANNELS;
	ad->cfg.mbus_code = MEDIA_BUS_FMT_UYVY8_2X8;

	switch (ad->cfg.mbus_flags & V4L2_MBUS_CSI2_LANES) {
	case V4L2_MBUS_CSI2_1_LANE:
		ad->cfg.lanes = 1;
		break;
	case V4L2_MBUS_CSI2_2_LANE:
		ad->cfg.lanes = 2;
		break;
	case V4L2_MBUS_CSI2_3_LANE:
		ad->cfg.lanes = 3;
		break;
	case V4L2_MBUS_CSI2_4_LANE:
		ad->cfg.lanes = 4;
		break;
	default:
		ad->cfg.lanes = 1;
		break;
	}

	/* fix crop info from dts config */
	for (i = 0; i < 4; i++) {
		if ((ad->defrects[i].width == ad->cfg.width) &&
		    (ad->defrects[i].height == ad->cfg.height)) {
			ad->cfg.start_x = ad->defrects[i].crop_x;
			ad->cfg.start_y = ad->defrects[i].crop_y;
			ad->cfg.width = ad->defrects[i].crop_width;
			ad->cfg.height = ad->defrects[i].crop_height;
		}
	}
}

static void tp2860_reg_init(struct vehicle_ad_dev *ad, unsigned char cvstd)
{
	struct rk_sensor_reg *sensor;
	int i;
	u8 tmp = 0;

	//return;
	/* common register */
	sensor = sensor_common_reg;
	i = 0;
	while ((sensor[i].reg != SEQCMD_END) && (sensor[i].reg != 0xFC000000)) {
		vehicle_sensor_write(ad, sensor[i].reg, sensor[i].val);
		i++;
	}

	switch (cvstd) {
	case CVSTD_NTSC:
		SENSOR_DG("%s, init CVSTD_NTSC mode", __func__);
		sensor = sensor_preview_data_ntsc_30hz;
		break;
	case CVSTD_PAL:
		SENSOR_DG("%s, init CVSTD_PAL mode", __func__);
		sensor = sensor_preview_data_pal_25hz;
		break;
	case CVSTD_720P25:
		SENSOR_DG("%s, init CVSTD_720P25 mode)", __func__);
		sensor = sensor_preview_data_720p_25hz;
		break;
	case CVSTD_720P30:
		SENSOR_DG("%s, init CVSTD_720P30 mode)", __func__);
		sensor = sensor_preview_data_720p_30hz;
		break;
	case CVSTD_1080P25:
		SENSOR_DG("%s, init CVSTD_1080P25 mode", __func__);
		sensor = sensor_preview_data_1080p_25hz;
		break;
	case CVSTD_1080P30:
		SENSOR_DG("%s, init CVSTD_1080P30 mode", __func__);
		sensor = sensor_preview_data_1080p_30hz;
		break;
	default:
		SENSOR_DG("%s, init CVSTD_1080P25 mode", __func__);
		sensor = sensor_preview_data_1080p_25hz;
		break;
	}


	i = 0;
	while ((sensor[i].reg != SEQCMD_END) && (sensor[i].reg != 0xFC000000)) {
		vehicle_sensor_write(ad, sensor[i].reg, sensor[i].val);
		i++;
	}

	/* color register */
	sensor = sensor_color_reg;
	i = 0;
	while ((sensor[i].reg != SEQCMD_END) && (sensor[i].reg != 0xFC000000)) {
		vehicle_sensor_write(ad, sensor[i].reg, sensor[i].val);
		i++;
	}
	vehicle_sensor_write(ad, PAGE_REG, 0x00); //back to decoder page
	vehicle_sensor_read(ad, 0x06, &tmp); //PLL reset
	vehicle_sensor_write(ad, 0x06, 0x80 | tmp);

	vehicle_sensor_write(ad, PAGE_REG, 0x08); //back to mipi page
	vehicle_sensor_read(ad, 0x14, &tmp); //PLL reset
	vehicle_sensor_write(ad, 0x14, 0x80 | tmp);
	vehicle_sensor_write(ad, 0x14, tmp);

	vehicle_sensor_write(ad, PAGE_REG, 0x00); //back to decoder page
	//set color bar
	if (!ad->last_detect_status)
		vehicle_sensor_write(ad, 0x2a, 0x3c);
	else
		vehicle_sensor_write(ad, 0x2a, 0x30);
}

void tp2860_channel_set(struct vehicle_ad_dev *ad, int channel)
{
	//detect interesting channel
	ad->ad_chl = channel;
	SENSOR_DG("%s, channel set(%d)", __func__, ad->ad_chl);
	//vehicle_sensor_write(ad, 0x40, 0x00);
	//vehicle_sensor_write(ad, 0x41, channel);
}

int tp2860_ad_get_cfg(struct vehicle_cfg **cfg)
{

	if (!tp2860_g_addev)
		return -ENODEV;

	switch (cvstd_state) {
	case VIDEO_UNPLUG:
		tp2860_g_addev->cfg.ad_ready = false;
		break;
	case VIDEO_LOCKED:
		tp2860_g_addev->cfg.ad_ready = true;
		break;
	case VIDEO_IN:
		tp2860_g_addev->cfg.ad_ready = false;
		break;
	}

	tp2860_g_addev->cfg.ad_ready = true;
	tp2860_g_addev->cfg.drop_frames = tp2860_g_addev->drop_frames;

	*cfg = &tp2860_g_addev->cfg;

	return 0;
}

void tp2860_ad_check_cif_error(struct vehicle_ad_dev *ad, int last_line)
{
	SENSOR_DG("%s, last_line %d\n", __func__, last_line);

	if (last_line < 1)
		return;

	ad->cif_error_last_line = last_line;
	if (cvstd_mode == CVSTD_PAL) {
		if (last_line == FORCE_NTSC_HEIGHT) {
			if (ad->state_check_work.state_check_wq)
				queue_delayed_work(
					ad->state_check_work.state_check_wq,
					&ad->state_check_work.work,
					msecs_to_jiffies(0));
		}
	} else if (cvstd_mode == CVSTD_NTSC) {
		if (last_line == FORCE_PAL_HEIGHT) {
			if (ad->state_check_work.state_check_wq)
				queue_delayed_work(
					ad->state_check_work.state_check_wq,
					&ad->state_check_work.work,
					msecs_to_jiffies(0));
		}
	} else if (cvstd_mode == CVSTD_1080P25) {
		if (last_line == FORCE_1080P_HEIGHT) {
			if (ad->state_check_work.state_check_wq)
				queue_delayed_work(
					ad->state_check_work.state_check_wq,
					&ad->state_check_work.work,
					msecs_to_jiffies(0));
		}
	} else if (cvstd_mode == CVSTD_720P25) {
		if (last_line == FORCE_720P_HEIGHT) {
			if (ad->state_check_work.state_check_wq)
				queue_delayed_work(
					ad->state_check_work.state_check_wq,
					&ad->state_check_work.work,
					msecs_to_jiffies(0));
		}
	}
}

static __maybe_unused int auto_detect_lockstatus(struct vehicle_ad_dev *ad)
{
	unsigned char status;
	int state = VIDEO_UNPLUG;
	int check_count = 10;

check_continue:
	vehicle_sensor_write(ad, 0x40, 0x00);
	vehicle_sensor_read(ad, 0x01, &status);

	SENSOR_DG(">>>>>>>>>>>>>>>>>>>>>> check_continue = %0x\n  check_count = %d",
		  status, check_count);

	if (status & FLAG_LOSS) {
		state = VIDEO_UNPLUG;
	} else if (FLAG_LOCKED == (status & FLAG_LOCKED)) {
		/* video locked */
		state = VIDEO_LOCKED;
	} else {
		/* video in but unlocked */
		state = VIDEO_IN;
	}

	if (check_count && state != VIDEO_LOCKED) {
		check_count--;
		usleep_range(100000, 120000);
		goto check_continue;
	}

	return 0;
}

int tp2860_check_id(struct vehicle_ad_dev *ad)
{
	int ret = 0;
	u8 chip_id_h = 0, chip_id_l = 0;

	vehicle_sensor_write(ad, 0x40, 0x0);
	ret = vehicle_sensor_read(ad, 0xFE, &chip_id_h);
	ret |= vehicle_sensor_read(ad, 0xFF, &chip_id_l);
	if (ret)
		return ret;

	if (chip_id_h != 0x28 || chip_id_l != 0x60) {
		VEHICLE_DGERR("%s: expected 0x2860, detected: 0x%0x%0x !",
			ad->ad_name, chip_id_h, chip_id_l);
		ret = -EINVAL;
	} else {
		VEHICLE_INFO("%s Found TP2860 sensor: id(0x%x%x) !\n",
						__func__, chip_id_h, chip_id_l);
	}

	return ret;
}

static int tp2860_check_cvstd(struct vehicle_ad_dev *ad, bool activate_check)
{

	u8 videoloss = 0, cvbs = 0;
	int ret = 0;
	unsigned char cvstd = 0;

	//return 0;
	ret = vehicle_sensor_write(ad, 0x40, 0x00);
	ret |= vehicle_sensor_read(ad, 0x01, &videoloss);

	video_mode = videoloss & 0x80;

	ret |= vehicle_sensor_write(ad, 0x40, 0x00);
	ret |= vehicle_sensor_read(ad, 0x03, &cvstd);
	cvstd = cvstd & 0x7; // CVSTD[2-0]
	if (ret)
		return ret;

	if (cvstd == TP2860_CVSTD_1080P_25) {
		cvstd_mode = CVSTD_1080P25;
		SENSOR_DG("%s(%d): 1080P25\n", __func__, __LINE__);
	} else if (cvstd == TP2860_CVSTD_720P_25) {
		cvstd_mode = CVSTD_720P25;
		SENSOR_DG("%s(%d): 720P25", __func__, __LINE__);
	} else if (cvstd == TP2860_CVSTD_SD) {
		vehicle_sensor_read(ad, 0x01, &cvbs);
		if (cvbs & 0x04) {
			SENSOR_DG("%s(%d): 960H PAL\n", __func__, __LINE__);
			cvstd_mode = CVSTD_PAL;
		} else {
			SENSOR_DG("%s(%d): 960H NTSC\n", __func__, __LINE__);
			cvstd_mode = CVSTD_NTSC;
		}
	} else if (cvstd == 0x7) {
		cvstd_mode = cvstd_old;
		SENSOR_DG("%s(%d): no ahd plugin!\n", __func__, __LINE__);
	} else {
		cvstd_mode = cvstd_old;
		SENSOR_DG("%s(%d): not support ahd mode!\n", __func__, __LINE__);
	}

	return 0;
}

int tp2860_stream(struct vehicle_ad_dev *ad, int enable)
{
	SENSOR_DG("%s on(%d)\n", __func__, enable);

	g_tp2860_streaming = (enable != 0);
	if (g_tp2860_streaming) {
		auto_detect_lockstatus(ad);
		vehicle_sensor_write(ad, 0x40, 0x08);
		vehicle_sensor_write(ad, 0x22, 0x10);
		vehicle_sensor_write(ad, 0x28, 0x00);
		vehicle_sensor_write(ad, 0x40, 0x00);
		if (ad->state_check_work.state_check_wq)
			queue_delayed_work(ad->state_check_work.state_check_wq,
				&ad->state_check_work.work, msecs_to_jiffies(200));
	} else {
		if (ad->state_check_work.state_check_wq)
			cancel_delayed_work_sync(&ad->state_check_work.work);
		vehicle_sensor_write(ad, 0x40, 0x08);
		vehicle_sensor_write(ad, 0x28, 0x02);
		vehicle_sensor_write(ad, 0x40, 0x00);
	}
	SENSOR_DG("%s on(%d) end!\n", __func__, enable);

	return 0;
}

static void tp2860_disable_regulators(struct vehicle_ad_dev *ad,
				    struct regulator_bulk_data *consumers)
{
	int j;
	int num_consumers = NUM_SUPPLIES;

	for (j = 0; j < num_consumers; j++) {
		if (consumers[j].consumer != NULL)
			regulator_disable(consumers[j].consumer);
	}
}

static int tp2860_enable_regulators(struct vehicle_ad_dev *ad,
				    struct regulator_bulk_data *consumers)
{
	int i, j;
	int ret = 0;
	struct device *dev = ad->dev;
	int num_consumers = NUM_SUPPLIES;

	for (i = 0; i < num_consumers; i++) {
		if (consumers[i].consumer != NULL) {
			ret = regulator_enable(consumers[i].consumer);
			if (ret < 0) {
				dev_err(dev, "Failed to enable regulator: %s\n",
					consumers[i].supply);
				goto err;
			}
		}
	}
	return 0;
err:
	for (j = 0; j < i; j++) {
		if (consumers[j].consumer != NULL)
			regulator_disable(consumers[j].consumer);
	}

	return ret;
}

static void tp2860_power_on(struct vehicle_ad_dev *ad)
{
	int ret = 0;

	if (!IS_ERR(ad->power_gpio))
		gpiod_direction_output(ad->power_gpio, 1);

	if (!IS_ERR(ad->supplies)) {
		VEHICLE_INFO("enable regulators\n");
		ret = tp2860_enable_regulators(ad, ad->supplies);
		if (ret < 0)
			VEHICLE_DGERR("Failed to enable regulators\n");
		usleep_range(2000, 5000);
	}

	if (!IS_ERR(ad->powerdown_gpio))
		gpiod_direction_output(ad->powerdown_gpio, 1);

	if (!IS_ERR(ad->reset_gpio)) {
		gpiod_direction_output(ad->reset_gpio, 0);
		usleep_range(1500, 2000);
		gpiod_direction_output(ad->reset_gpio, 1);
	}
}

static void tp2860_power_deinit(struct vehicle_ad_dev *ad)
{
	if (!IS_ERR(ad->powerdown_gpio))
		gpiod_direction_output(ad->powerdown_gpio, 0);
	if (!IS_ERR(ad->power_gpio))
		gpiod_direction_output(ad->power_gpio, 0);
	if (!IS_ERR(ad->reset_gpio))
		gpiod_direction_output(ad->reset_gpio, 0);
	if (!IS_ERR(ad->supplies))
		tp2860_disable_regulators(ad, ad->supplies);
}

static __maybe_unused int tp2860_auto_detect_hotplug(struct vehicle_ad_dev *ad)
{
	u8 detect_status = 0;

	vehicle_sensor_write(ad, 0x40, 0x00);
	vehicle_sensor_read(ad, 0x01, &detect_status);

	ad->detect_status = (detect_status & 0x80) ? 0 : 1;
	SENSOR_DG("input_status:0x%x, last_detect_status:0x%x\n",
		    ad->detect_status, ad->last_detect_status);

	return 0;
}

static void tp2860_check_state_work(struct work_struct *work)
{
	struct vehicle_ad_dev *ad;

	ad = tp2860_g_addev;

	tp2860_auto_detect_hotplug(ad);

	if (ad->cif_error_last_line > 0) {
		tp2860_check_cvstd(ad, true);
		ad->cif_error_last_line = 0;
	} else {
		tp2860_check_cvstd(ad, false);
	}

	if (ad->detect_status != ad->last_detect_status ||
		cvstd_old != cvstd_mode) {
		VEHICLE_INFO("%s:ad sensor std mode change, cvstd_old(%d), cvstd_mode(%d)\n",
				 __func__, cvstd_old, cvstd_mode);
		cvstd_old = cvstd_mode;
		cvstd_old_state = cvstd_state;
		video_old = video_mode;
		ad->last_detect_status = ad->detect_status;
		tp2860_reinit_parameter(ad, cvstd_mode);
		tp2860_reg_init(ad, cvstd_mode);
		vehicle_ad_stat_change_notify();
	}
	if (g_tp2860_streaming) {
		queue_delayed_work(ad->state_check_work.state_check_wq,
				&ad->state_check_work.work, msecs_to_jiffies(200));
	}
}

int tp2860_configure_regulators(struct vehicle_ad_dev *ad, struct device_node *cp)
{
	unsigned int i;
	int err;

	for (i = 0; i < NUM_SUPPLIES; i++) {
		tp2860_supplies[i].supply = NULL;
		tp2860_supplies[i].consumer = NULL;
	}

	ad->supplies = tp2860_supplies;

	for (i = 0; i < NUM_SUPPLIES; i++) {
		err = of_property_read_string(cp, tp2860_supply_names[i], &ad->supplies[i].supply);
		if (err < 0) {
			VEHICLE_DGERR("Get %s failed!\n", tp2860_supply_names[i]);
			continue;
		}
		if (ad->supplies[i].supply != NULL) {
			ad->supplies[i].consumer = regulator_get(NULL, ad->supplies[i].supply);
			if (IS_ERR(ad->supplies[i].consumer)) {
				VEHICLE_DGERR("Failed to get:%s regulator!\n",
					       ad->supplies[i].supply);
				ad->supplies[i].consumer = NULL;
			} else {
				VEHICLE_INFO("Get supply:%s success!\n", ad->supplies[i].supply);
			}
		}
	}

	return 0;
}

static int tp2860_release_regulators(struct vehicle_ad_dev *ad)
{
	unsigned int i;

	for (i = 0; i < NUM_SUPPLIES; i++)
		regulator_put(ad->supplies[i].consumer);

	return 0;
}

int tp2860_ad_deinit(void)
{
	struct vehicle_ad_dev *ad;

	ad = tp2860_g_addev;

	if (!ad)
		return -ENODEV;

	if (ad->state_check_work.state_check_wq) {
		cancel_delayed_work_sync(&ad->state_check_work.work);
		flush_delayed_work(&ad->state_check_work.work);
		flush_workqueue(ad->state_check_work.state_check_wq);
		destroy_workqueue(ad->state_check_work.state_check_wq);
	}
	if (ad->irq)
		free_irq(ad->irq, ad);
	tp2860_power_deinit(ad);

	tp2860_release_regulators(ad);
	return 0;
}

static __maybe_unused int get_ad_mode_from_fix_format(int fix_format)
{
	int mode = -1;

	switch (fix_format) {
	case AD_FIX_FORMAT_PAL:
	case AD_FIX_FORMAT_NTSC:
	case AD_FIX_FORMAT_720P_50FPS:
	case AD_FIX_FORMAT_720P_30FPS:
	case AD_FIX_FORMAT_720P_25FPS:
		mode = CVSTD_720P25;
		break;
	case AD_FIX_FORMAT_1080P_30FPS:
	case AD_FIX_FORMAT_1080P_25FPS:

	default:
		mode = CVSTD_720P25;
		break;
	}

	return mode;
}

int tp2860_ad_init(struct vehicle_ad_dev *ad)
{
	int val;
	int i = 0;

	tp2860_g_addev = ad;

	/*  1. i2c init */
	while (ad->adapter == NULL) {
		ad->adapter = i2c_get_adapter(ad->i2c_chl);
		VEHICLE_INFO("wait 10ms to get i2c adapter!\n");
		usleep_range(10000, 12000);
	}
	if (ad->adapter == NULL)
		return -ENODEV;

	if (!i2c_check_functionality(ad->adapter, I2C_FUNC_I2C))
		return -EIO;

	tp2860_power_on(ad);

	while (++i < 5) {
		usleep_range(10000, 12000);
		val = vehicle_generic_sensor_read(ad, 0xf0);
		if (val != 0xff)
			break;
		VEHICLE_DGERR("tp2860_init i2c_reg_read fail\n");
	}

	tp2860_reg_init(ad, cvstd_mode);

	tp2860_reinit_parameter(ad, cvstd_mode);
	ad->detect_status = true;
	ad->last_detect_status = false;

	INIT_DELAYED_WORK(&ad->state_check_work.work, tp2860_check_state_work);
	ad->state_check_work.state_check_wq =
		create_singlethread_workqueue("vehicle-ad-tp2860");

	/* tp2860_check_cvstd(ad, true); */
	if (!ad->state_check_work.state_check_wq) {
		VEHICLE_DGERR("create tp2860 state check workqueue failed!\n");
	} else {
		VEHICLE_INFO("create tp2860 state check workqueue success!\n");
		queue_delayed_work(ad->state_check_work.state_check_wq,
				   &ad->state_check_work.work, msecs_to_jiffies(100));
	}

	return 0;
}
