// SPDX-License-Identifier: GPL-2.0
/*
 * vehicle sensor tp2855
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 * Authors:
 *
 *      Jianwei Fan <jianwei.fan@rock-chips.com>
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
#include "vehicle_ad_tp2855.h"

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

enum {
	VIDEO_UNPLUG,
	VIDEO_IN,
	VIDEO_LOCKED,
	VIDEO_UNLOCK
};

#define TP2855_LINK_FREQ_297M			(297000000UL >> 1)
#define TP2855_LINK_FREQ_594M			(594000000UL >> 1)
#define TP2855_XVCLK_FREQ			27000000
#define TP2855_TEST_PATTERN			0

static struct vehicle_ad_dev *tp2855_g_addev;
static int cvstd_mode = CVSTD_1080P25;
static int cvstd_old = CVSTD_720P25;
static int cvstd_state = VIDEO_UNPLUG;
// static int cvstd_old_state = VIDEO_UNLOCK;

static bool g_tp2855_streaming;

enum tp2855_support_reso {
	TP2855_CVSTD_720P_60 = 0,
	TP2855_CVSTD_720P_50,
	TP2855_CVSTD_1080P_30,
	TP2855_CVSTD_1080P_25,
	TP2855_CVSTD_720P_30,
	TP2855_CVSTD_720P_25,
	TP2855_CVSTD_SD,
	TP2855_CVSTD_OTHER,
};

struct regval {
	u8 addr;
	u8 val;
};

static __maybe_unused const struct regval common_setting_297M_720p_25fps_regs[] = {
	{0x02, 0x42},
	{0x07, 0xc0},
	{0x0b, 0xc0},
	{0x0c, 0x13},
	{0x0d, 0x50},
	{0x15, 0x13},
	{0x16, 0x15},
	{0x17, 0x00},
	{0x18, 0x19},
	{0x19, 0xd0},
	{0x1a, 0x25},
	{0x1c, 0x07},
	{0x1d, 0xbc},
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
	{0x35, 0x25},
	{0x38, 0x00},
	{0x39, 0x18},

	{0x02, 0x46},
	{0x0d, 0x71},
	{0x18, 0x1b},
	{0x20, 0x40},
	{0x21, 0x46},
	{0x25, 0xfe},
	{0x26, 0x01},
	{0x2c, 0x3a},
	{0x2d, 0x5a},
	{0x2e, 0x40},
	{0x30, 0x9e},
	{0x31, 0x20},
	{0x32, 0x10},
	{0x33, 0x90},
	// {0x23, 0x02}, //vi test ok
	// {0x23, 0x00},
};

static __maybe_unused const struct regval mipi_setting_297M_4ch_4lane_regs[] = {
	{0x40, 0x08},
	{0x01, 0xf0},
	{0x02, 0x01},
	{0x08, 0x0f},
	{0x20, 0x44},
	{0x34, 0xe4},
	{0x14, 0x44},
	{0x15, 0x0d},
	{0x25, 0x04},
	{0x26, 0x03},
	{0x27, 0x09},
	{0x29, 0x02},
	{0x33, 0x0f},
	{0x33, 0x00},
	{0x14, 0xc4},
	{0x14, 0x44},
};

static __maybe_unused const struct regval mipi_setting_594M_4ch_4lane_regs[] = {
	{0x40, 0x08},
	{0x01, 0xf0},
	{0x02, 0x01},
	{0x08, 0x0f},
	{0x20, 0x44},
	{0x34, 0xe4},
	{0x15, 0x0C},
	{0x25, 0x08},
	{0x26, 0x06},
	{0x27, 0x11},
	{0x29, 0x0a},
	{0x33, 0x0f},
	{0x33, 0x00},
	{0x14, 0x33},
	{0x14, 0xb3},
	{0x14, 0x33},
	// {0x23, 0x02}, //vi test ok
	// {0x23, 0x00},
};

static __maybe_unused const struct regval common_setting_594M_1080p_25fps_regs[] = {
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
};

static void tp2855_reinit_parameter(struct vehicle_ad_dev *ad, unsigned char cvstd)
{
	int i = 0;

	switch (cvstd) {
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
		ad->cfg.mipi_freq = TP2855_LINK_FREQ_594M;
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
		ad->cfg.mipi_freq = TP2855_LINK_FREQ_297M;
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
		ad->cfg.mipi_freq = TP2855_LINK_FREQ_594M;
		break;
	}
	ad->cfg.type = V4L2_MBUS_CSI2_DPHY;
	ad->cfg.mbus_flags = V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CONTINUOUS_CLOCK |
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

/* sensor register write */
static int tp2855_write_reg(struct vehicle_ad_dev *ad, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	buf[0] = reg & 0xFF;
	buf[1] = val;

	msg.addr = ad->i2c_add;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(ad->adapter, &msg, 1);
	if (ret >= 0) {
		usleep_range(300, 400);
		return 0;
	}

	VEHICLE_DGERR("tp2855 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

static int tp2855_write_array(struct vehicle_ad_dev *ad,
			       const struct regval *regs, int size)
{
	int i, ret = 0;

	i = 0;
	while (i < size) {
		ret = tp2855_write_reg(ad, regs[i].addr, regs[i].val);
		if (ret) {
			VEHICLE_DGERR("%s failed !\n", __func__);
			break;
		}
		i++;
	}

	return ret;
}

/* sensor register read */
static int tp2855_read_reg(struct vehicle_ad_dev *ad, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	buf[0] = reg & 0xFF;

	msg[0].addr = ad->i2c_add;
	msg[0].flags = 0;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = ad->i2c_add;
	msg[1].flags = 0 | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(ad->adapter, msg, 2);
	if (ret >= 0) {
		*val = buf[0];
		return 0;
	}

	VEHICLE_DGERR("tp2855 read reg(0x%x) failed !\n", reg);

	return ret;
}

void tp2855_channel_set(struct vehicle_ad_dev *ad, int channel)
{
	ad->ad_chl = channel;
	VEHICLE_DG("%s, channel set(%d)", __func__, ad->ad_chl);
}

int tp2855_ad_get_cfg(struct vehicle_cfg **cfg)
{
	if (!tp2855_g_addev)
		return -1;

	switch (cvstd_state) {
	case VIDEO_UNPLUG:
		tp2855_g_addev->cfg.ad_ready = false;
		break;
	case VIDEO_LOCKED:
		tp2855_g_addev->cfg.ad_ready = true;
		break;
	case VIDEO_IN:
		tp2855_g_addev->cfg.ad_ready = false;
		break;
	}

	tp2855_g_addev->cfg.ad_ready = true;

	*cfg = &tp2855_g_addev->cfg;

	return 0;
}

void tp2855_ad_check_cif_error(struct vehicle_ad_dev *ad, int last_line)
{
	VEHICLE_INFO("%s, last_line %d\n", __func__, last_line);

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

static int tp2855_set_channel_reso(struct vehicle_ad_dev *ad, int ch,
						enum tp2855_support_reso reso)
{
	int val = reso;
	u8 tmp = 0;
	const unsigned char SYS_MODE[5] = { 0x01, 0x02, 0x04, 0x08, 0x0f };

	tp2855_write_reg(ad, 0x40, ch);

	switch (val) {
	case TP2855_CVSTD_1080P_25:
		VEHICLE_INFO("set channel %d 1080P_25\n", ch);
		tp2855_read_reg(ad, 0xf5, &tmp);
		tmp &= ~SYS_MODE[ch];
		tp2855_write_reg(ad, 0xf5, tmp);
		tp2855_write_array(ad, common_setting_594M_1080p_25fps_regs,
				ARRAY_SIZE(common_setting_594M_1080p_25fps_regs));
		break;
	case TP2855_CVSTD_720P_25:
		VEHICLE_INFO("set channel %d 720P_25\n", ch);
		tp2855_read_reg(ad, 0xf5, &tmp);
		tmp |= SYS_MODE[ch];
		tp2855_write_reg(ad, 0xf5, tmp);
		tp2855_write_array(ad, common_setting_297M_720p_25fps_regs,
				ARRAY_SIZE(common_setting_297M_720p_25fps_regs));
		break;
	default:
		VEHICLE_INFO(
			"set channel %d is not supported, default 1080P_25\n", ch);
		tp2855_read_reg(ad, 0xf5, &tmp);
		tmp &= ~SYS_MODE[ch];
		tp2855_write_reg(ad, 0xf5, tmp);
		tp2855_write_array(ad, common_setting_594M_1080p_25fps_regs,
				ARRAY_SIZE(common_setting_594M_1080p_25fps_regs));
		break;
	}

	//set color bar
	if (!ad->last_detect_status)
		tp2855_write_reg(ad, 0x2a, 0x3c);
	else
		tp2855_write_reg(ad, 0x2a, 0x30);

	return 0;
}

static int tp2855_get_channel_reso(struct vehicle_ad_dev *ad, int ch)
{
	u8 detect_fmt = 0xff;
	u8 reso = 0xff;

	tp2855_write_reg(ad, 0x40, ch);
	tp2855_read_reg(ad, 0x03, &detect_fmt);
	reso = detect_fmt & 0x7;

	switch (reso) {
	case TP2855_CVSTD_1080P_30:
		VEHICLE_DG("detect channel %d 1080P_30\n", ch);
		cvstd_mode = CVSTD_1080P30;
		break;
	case TP2855_CVSTD_1080P_25:
		VEHICLE_DG("detect channel %d 1080P_25\n", ch);
		cvstd_mode = CVSTD_1080P25;
		break;
	case TP2855_CVSTD_720P_30:
		VEHICLE_DG("detect channel %d 720P_30\n", ch);
		cvstd_mode = CVSTD_720P30;
		break;
	case TP2855_CVSTD_720P_25:
		VEHICLE_DG("detect channel %d 720P_25\n", ch);
		cvstd_mode = CVSTD_720P25;
		break;
	case TP2855_CVSTD_SD:
		VEHICLE_DG("detect channel %d SD\n", ch);
		cvstd_mode = CVSTD_SD;
		break;
	default:
		VEHICLE_DG(
			"detect channel %d is not supported, default 1080P_25\n", ch);
		reso = TP2855_CVSTD_1080P_25;
		cvstd_mode = CVSTD_1080P25;
	}

	return reso;
}

static __maybe_unused int auto_detect_channel_fmt(struct vehicle_ad_dev *ad)
{
	int ch = 0;
	enum tp2855_support_reso reso = 0xff;

	for (ch = 0; ch < 4; ch++) {
		reso = tp2855_get_channel_reso(ad, ch);
		tp2855_set_channel_reso(ad, ch, reso);
	}

	return 0;
}

int tp2855_check_id(struct vehicle_ad_dev *ad)
{
	int ret = 0;
	u8 chip_id_h = 0, chip_id_l = 0;

	tp2855_write_reg(ad, 0x40, 0x0);
	ret = tp2855_read_reg(ad, 0xFE, &chip_id_h);
	ret |= tp2855_read_reg(ad, 0xFF, &chip_id_l);
	if (ret)
		return ret;

	if (chip_id_h != 0x28 || chip_id_l != 0x55) {
		VEHICLE_DGERR("%s: expected 0x2855, detected: 0x%0x%0x !",
			ad->ad_name, chip_id_h, chip_id_l);
		ret = -EINVAL;
	} else {
		VEHICLE_INFO("%s Found TP2855 sensor: id(0x%x%x) !\n",
						__func__, chip_id_h, chip_id_l);
	}

	return ret;
}

static int tp2855_get_channel_input_status(struct vehicle_ad_dev *ad, u8 ch)
{
	u8 val = 0;

	tp2855_write_reg(ad, 0x40, ch);
	tp2855_read_reg(ad, 0x01, &val);
	VEHICLE_DG("input_status ch %d : %x\n", ch, val);

	return (val & 0x80) ? 0 : 1;
}

static int tp2855_get_all_input_status(struct vehicle_ad_dev *ad, u8 *detect_status)
{
	u8 val = 0, i;

	for (i = 0; i < 4; i++) {
		tp2855_write_reg(ad, 0x40, i);
		tp2855_read_reg(ad, 0x01, &val);
		detect_status[i] = tp2855_get_channel_input_status(ad, i);
	}

	return 0;
}

static int tp2855_set_decoder_mode(struct vehicle_ad_dev *ad, int ch, int status)
{
	u8 val = 0;

	tp2855_write_reg(ad, 0x40, ch);
	tp2855_read_reg(ad, 0x26, &val);
	if (status)
		val |= 0x1;
	else
		val &= ~0x1;
	tp2855_write_reg(ad, 0x26, val);

	return 0;
}

static int tp2855_detect(struct vehicle_ad_dev *ad)
{
	int i;
	u8 detect_status[4];

	tp2855_get_all_input_status(ad, detect_status);
	for (i = 0; i < 4; i++)
		tp2855_set_decoder_mode(ad, i, detect_status[i]);

	return 0;
}

static int __tp2855_start_stream(struct vehicle_ad_dev *ad)
{
	int ret = 0;
	int array_size = 0;

	auto_detect_channel_fmt(ad);
	switch (cvstd_mode) {
	case CVSTD_1080P25:
		array_size = ARRAY_SIZE(mipi_setting_594M_4ch_4lane_regs);
		ret = tp2855_write_array(ad, mipi_setting_594M_4ch_4lane_regs, array_size);
		break;
	case CVSTD_720P25:
		array_size = ARRAY_SIZE(mipi_setting_297M_4ch_4lane_regs);
		ret = tp2855_write_array(ad, mipi_setting_297M_4ch_4lane_regs, array_size);
		break;
	}
	if (ret) {
		VEHICLE_INFO(" tp2855 start stream: wrote mipi reg failed");
		return ret;
	}
	tp2855_detect(ad);

	msleep(50);

	return 0;
}

static int __tp2855_stop_stream(struct vehicle_ad_dev *ad)
{
	return 0;
}

static int tp2855_check_cvstd(struct vehicle_ad_dev *ad, bool activate_check)
{
	tp2855_get_channel_reso(ad, ad->ad_chl);

	return 0;
}

int tp2855_stream(struct vehicle_ad_dev *ad, int enable)
{
	VEHICLE_INFO("%s on(%d)\n", __func__, enable);

	g_tp2855_streaming = (enable != 0);
	if (g_tp2855_streaming) {
		__tp2855_start_stream(ad);
		if (ad->state_check_work.state_check_wq)
			queue_delayed_work(ad->state_check_work.state_check_wq,
				&ad->state_check_work.work, msecs_to_jiffies(200));
	} else {
		__tp2855_stop_stream(ad);
		if (ad->state_check_work.state_check_wq)
			cancel_delayed_work_sync(&ad->state_check_work.work);
		VEHICLE_DG("%s(%d): cancel_queue_delayed_work!\n", __func__, __LINE__);
	}

	return 0;
}

static void tp2855_power_on(struct vehicle_ad_dev *ad)
{
	if (!IS_ERR(ad->power_gpio))
		gpiod_direction_output(ad->power_gpio, 1);

	if (!IS_ERR(ad->powerdown_gpio))
		gpiod_direction_output(ad->powerdown_gpio, 1);

	if (!IS_ERR(ad->reset_gpio)) {
		gpiod_direction_output(ad->reset_gpio, 0);
		usleep_range(1500, 2000);
		gpiod_direction_output(ad->reset_gpio, 1);
	}
	mdelay(100);
}

static void tp2855_power_off(struct vehicle_ad_dev *ad)
{
	if (!IS_ERR(ad->powerdown_gpio))
		gpiod_direction_output(ad->powerdown_gpio, 0);
	if (!IS_ERR(ad->power_gpio))
		gpiod_direction_output(ad->power_gpio, 0);
	if (!IS_ERR(ad->reset_gpio))
		gpiod_direction_output(ad->reset_gpio, 0);
}

static __maybe_unused int tp2855_auto_detect_hotplug(struct vehicle_ad_dev *ad)
{
	u8 detect_status = 0;

	tp2855_write_reg(ad, 0x40, 0x04);
	tp2855_read_reg(ad, 0x01, &detect_status);

	ad->detect_status = (detect_status & 0x80) ? 0 : 1;
	VEHICLE_DG("input_status %x\n", ad->detect_status);

	return 0;
}

static void tp2855_check_state_work(struct work_struct *work)
{
	struct vehicle_ad_dev *ad;

	ad = tp2855_g_addev;
	tp2855_auto_detect_hotplug(ad);
	tp2855_check_cvstd(ad, true);
	if (ad->detect_status != ad->last_detect_status || cvstd_old != cvstd_mode) {
		ad->last_detect_status = ad->detect_status;
		cvstd_old = cvstd_mode;
		vehicle_ad_stat_change_notify();
	}

	if (g_tp2855_streaming)
		queue_delayed_work(ad->state_check_work.state_check_wq,
				   &ad->state_check_work.work, msecs_to_jiffies(100));
}

int tp2855_ad_deinit(void)
{
	struct vehicle_ad_dev *ad;

	ad = tp2855_g_addev;

	if (!ad)
		return -1;

	if (ad->state_check_work.state_check_wq) {
		cancel_delayed_work_sync(&ad->state_check_work.work);
		flush_delayed_work(&ad->state_check_work.work);
		flush_workqueue(ad->state_check_work.state_check_wq);
		destroy_workqueue(ad->state_check_work.state_check_wq);
	}

	tp2855_power_off(ad);

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

int tp2855_ad_init(struct vehicle_ad_dev *ad)
{
	int val;
	int i = 0;

	tp2855_g_addev = ad;

	/*  1. i2c init */
	while (ad->adapter == NULL) {
		ad->adapter = i2c_get_adapter(ad->i2c_chl);
		usleep_range(10000, 12000);
	}
	if (ad->adapter == NULL)
		return -ENODEV;

	if (!i2c_check_functionality(ad->adapter, I2C_FUNC_I2C))
		return -EIO;

	/*  2. ad power on sequence */
	tp2855_power_on(ad);

	while (++i < 5) {
		usleep_range(1000, 1200);
		val = vehicle_generic_sensor_read(ad, 0xf0);
		if (val != 0xff)
			break;
		VEHICLE_INFO("tp2855_init i2c_reg_read fail\n");
	}

	tp2855_reinit_parameter(ad, cvstd_mode);
	ad->last_detect_status = false;

	/*  create workqueue to detect signal change */
	INIT_DELAYED_WORK(&ad->state_check_work.work, tp2855_check_state_work);
	ad->state_check_work.state_check_wq =
		create_singlethread_workqueue("vehicle-ad-tp2855");

	queue_delayed_work(ad->state_check_work.state_check_wq,
			   &ad->state_check_work.work, msecs_to_jiffies(100));

	return 0;
}
