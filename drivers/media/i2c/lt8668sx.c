// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 * lt8668sx HDMI to MIPI CSI-2 bridge driver.
 *
 * Author: Jianwei Fan <jianwei.fan@rock-chips.com>
 *
 * V0.0X01.0X00 first version.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/hdmi.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/rk-camera-module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/v4l2-dv-timings.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/workqueue.h>
#include <linux/compat.h>
#include <media/v4l2-controls_rockchip.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x00)

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level (0-3)");

#define I2C_MAX_XFER_SIZE	128
#define POLL_INTERVAL_MS	1000

#define LT8668SX_LINK_FREQ_1250M	1250000000
#define LT8668SX_LINK_FREQ_1000M	1000000000
#define LT8668SX_LINK_FREQ_900M		900000000
#define LT8668SX_LINK_FREQ_700M		700000000
#define LT8668SX_LINK_FREQ_650M		650000000
#define LT8668SX_LINK_FREQ_600M		600000000
#define LT8668SX_LINK_FREQ_490M		490000000
#define LT8668SX_LINK_FREQ_400M		400000000
#define LT8668SX_LINK_FREQ_300M		300000000
#define LT8668SX_LINK_FREQ_250M		250000000
#define LT8668SX_LINK_FREQ_200M		200000000
#define LT8668SX_LINK_FREQ_100M		100000000

#define LT8668SX_PIXEL_RATE		800000000

#define LT8668SX_CHIPID		0x0119
#define CHIPID_REGH		0xe101
#define CHIPID_REGL		0xe100
#define I2C_EN_REG		0xe0ee
#define I2C_ENABLE		0x1
#define I2C_DISABLE		0x0

#define HACT_H			0xe090
#define HACT_L			0xe091

#define VACT_H			0xe092
#define VACT_L			0xe093

#define PCLK_H			0xe095
#define PCLK_M			0xe096
#define PCLK_L			0xe097

#define FRAMERATE		0xe094
#define AUDIO_FS_VALUE		0xe098
#define INTERLACED		0xe099

#define LT8668SX_NAME			"LT8668SX"

static const s64 link_freq_dphy_menu_items[] = {
	LT8668SX_LINK_FREQ_1250M,
	LT8668SX_LINK_FREQ_650M,
	LT8668SX_LINK_FREQ_490M,
	LT8668SX_LINK_FREQ_300M,
	LT8668SX_LINK_FREQ_200M,
	LT8668SX_LINK_FREQ_100M,
};

static const s64 link_freq_cphy_menu_items[] = {
	LT8668SX_LINK_FREQ_900M,
	LT8668SX_LINK_FREQ_700M,
	LT8668SX_LINK_FREQ_400M,
	LT8668SX_LINK_FREQ_200M,
	LT8668SX_LINK_FREQ_100M,
};

static const s64 link_freq_cphy_rgb_menu_items[] = {
	LT8668SX_LINK_FREQ_1250M,
	LT8668SX_LINK_FREQ_1000M,
	LT8668SX_LINK_FREQ_600M,
	LT8668SX_LINK_FREQ_300M,
	LT8668SX_LINK_FREQ_200M,
};

struct lt8668sx {
	struct v4l2_mbus_config_mipi_csi2 bus;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler hdl;
	struct i2c_client *i2c_client;
	struct mutex confctl_mutex;
	struct v4l2_ctrl *detect_tx_5v_ctrl;
	struct v4l2_ctrl *audio_sampling_rate_ctrl;
	struct v4l2_ctrl *audio_present_ctrl;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct delayed_work delayed_work_hotplug;
	struct delayed_work delayed_work_res_change;
	struct v4l2_dv_timings timings;
	struct clk *xvclk;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *plugin_det_gpio;
	struct gpio_desc *power_gpio;
	struct work_struct work_i2c_poll;
	struct timer_list timer;
	const char *module_facing;
	const char *module_name;
	const char *len_name;
	const struct lt8668sx_mode *cur_mode;
	const struct lt8668sx_mode *support_modes;
	u32 cfg_num;
	struct v4l2_fwnode_endpoint bus_cfg;
	bool nosignal;
	bool enable_hdcp;
	bool is_audio_present;
	bool power_on;
	int plugin_irq;
	u32 mbus_fmt_code;
	u32 module_index;
	u32 audio_sampling_rate;
	u32 cur_framerate;
	u32 last_framerate;
};

static const struct v4l2_dv_timings_cap lt8668sx_timings_cap = {
	.type = V4L2_DV_BT_656_1120,
	.reserved = { 0 },
	V4L2_INIT_BT_TIMINGS(1, 10000, 1, 10000, 0, 800000000,
			V4L2_DV_BT_STD_CEA861 | V4L2_DV_BT_STD_DMT |
			V4L2_DV_BT_STD_GTF | V4L2_DV_BT_STD_CVT,
			V4L2_DV_BT_CAP_PROGRESSIVE | V4L2_DV_BT_CAP_INTERLACED |
			V4L2_DV_BT_CAP_REDUCED_BLANKING |
			V4L2_DV_BT_CAP_CUSTOM)
};

struct lt8668sx_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 mipi_freq_idx;
};

static struct rkmodule_csi_dphy_param rk3588_dcphy_param = {
	.vendor = PHY_VENDOR_SAMSUNG,
	.lp_vol_ref = 3,
	.lp_hys_sw = {3, 0, 0, 0},
	.lp_escclk_pol_sel = {1, 0, 0, 0},
	.skew_data_cal_clk = {0, 3, 3, 3},
	.clk_hs_term_sel = 2,
	.data_hs_term_sel = {2, 2, 2, 2},
	.reserved = {0},
};

static const struct lt8668sx_mode supported_modes_dphy[] = {
	{
		.width = 4096,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
		.mipi_freq_idx = 0,
	}, {
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
		.mipi_freq_idx = 0,
	}, {
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
		.mipi_freq_idx = 1,
	}, {
		.width = 2560,
		.height = 1440,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 2720,
		.vts_def = 1481,
		.mipi_freq_idx = 2,
	}, {
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 2200,
		.vts_def = 1125,
		.mipi_freq_idx = 3,
	}, {
		.width = 1600,
		.height = 1200,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 2160,
		.vts_def = 1250,
		.mipi_freq_idx = 3,
	}, {
		.width = 1280,
		.height = 960,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 1712,
		.vts_def = 994,
		.mipi_freq_idx = 4,
	}, {
		.width = 1280,
		.height = 720,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 1650,
		.vts_def = 750,
		.mipi_freq_idx = 4,
	}, {
		.width = 800,
		.height = 600,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 1056,
		.vts_def = 628,
		.mipi_freq_idx = 5,
	}, {
		.width = 720,
		.height = 576,
		.max_fps = {
			.numerator = 10000,
			.denominator = 500000,
		},
		.hts_def = 864,
		.vts_def = 625,
		.mipi_freq_idx = 5,
	}, {
		.width = 720,
		.height = 480,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 858,
		.vts_def = 525,
		.mipi_freq_idx = 5,
	},
};

static const struct lt8668sx_mode supported_modes_cphy[] = {
	{
		.width = 5120,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 5500,
		.vts_def = 2250,
		.mipi_freq_idx = 0,
	}, {
		.width = 4096,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
		.mipi_freq_idx = 1,
	}, {
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
		.mipi_freq_idx = 1,
	}, {
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
		.mipi_freq_idx = 2,
	}, {
		.width = 2560,
		.height = 1440,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 2720,
		.vts_def = 1481,
		.mipi_freq_idx = 2,
	}, {
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 2200,
		.vts_def = 1125,
		.mipi_freq_idx = 3,
	}, {
		.width = 1280,
		.height = 720,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 1650,
		.vts_def = 750,
		.mipi_freq_idx = 4,
	}, {
		.width = 1024,
		.height = 768,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 1344,
		.vts_def = 806,
		.mipi_freq_idx = 4,
	}, {
		.width = 800,
		.height = 600,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 1056,
		.vts_def = 628,
		.mipi_freq_idx = 4,
	}, {
		.width = 720,
		.height = 576,
		.max_fps = {
			.numerator = 10000,
			.denominator = 500000,
		},
		.hts_def = 864,
		.vts_def = 625,
		.mipi_freq_idx = 4,
	}, {
		.width = 720,
		.height = 480,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 858,
		.vts_def = 525,
		.mipi_freq_idx = 4,
	},
};

static void lt8668sx_format_change(struct v4l2_subdev *sd);
static int lt8668sx_s_ctrl_detect_tx_5v(struct v4l2_subdev *sd);
static int lt8668sx_s_dv_timings(struct v4l2_subdev *sd,
				struct v4l2_dv_timings *timings);

static inline struct lt8668sx *to_lt8668sx(struct v4l2_subdev *sd)
{
	return container_of(sd, struct lt8668sx, sd);
}

static void i2c_rd(struct v4l2_subdev *sd, u16 reg, u8 *values, u32 n)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	struct i2c_client *client = lt8668sx->i2c_client;
	int err;
	u8 buf[2] = { 0xFF, reg >> 8};
	u8 reg_addr = reg & 0xFF;
	struct i2c_msg msgs[3];

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = 0;
	msgs[1].len = 1;
	msgs[1].buf = &reg_addr;

	msgs[2].addr = client->addr;
	msgs[2].flags = I2C_M_RD;
	msgs[2].len = n;
	msgs[2].buf = values;

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != ARRAY_SIZE(msgs)) {
		v4l2_err(sd, "%s: reading register 0x%x from 0x%x failed\n",
				__func__, reg, client->addr);
	}
}

static void i2c_wr(struct v4l2_subdev *sd, u16 reg, u8 *values, u32 n)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	struct i2c_client *client = lt8668sx->i2c_client;
	int err, i;
	struct i2c_msg msgs[2];
	u8 data[I2C_MAX_XFER_SIZE];
	u8 buf[2] = { 0xFF, reg >> 8};

	if ((1 + n) > I2C_MAX_XFER_SIZE) {
		n = I2C_MAX_XFER_SIZE - 1;
		v4l2_warn(sd, "i2c wr reg=%04x: len=%d is too big!\n",
			  reg, 1 + n);
	}

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = 0;
	msgs[1].len = 1 + n;
	msgs[1].buf = data;

	data[0] = reg & 0xff;
	for (i = 0; i < n; i++)
		data[1 + i] = values[i];

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err < 0) {
		v4l2_err(sd, "%s: writing register 0x%x from 0x%x failed\n",
				__func__, reg, client->addr);
		return;
	}
}

static u8 i2c_rd8(struct v4l2_subdev *sd, u16 reg)
{
	u32 val;

	i2c_rd(sd, reg, (u8 __force *)&val, 1);
	return val;
}

static __maybe_unused void i2c_wr8(struct v4l2_subdev *sd, u16 reg, u8 val)
{
	i2c_wr(sd, reg, &val, 1);
}

static void lt8668sx_i2c_enable(struct v4l2_subdev *sd)
{
	i2c_wr8(sd, I2C_EN_REG, I2C_ENABLE);
}

static void lt8668sx_i2c_disable(struct v4l2_subdev *sd)
{
	i2c_wr8(sd, I2C_EN_REG, I2C_DISABLE);
}

static inline bool tx_5v_power_present(struct v4l2_subdev *sd)
{
	bool ret;
	int val, i, cnt;
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	/* if not use plugin det gpio */
	if (!lt8668sx->plugin_det_gpio)
		return true;

	cnt = 0;
	for (i = 0; i < 5; i++) {
		val = gpiod_get_value(lt8668sx->plugin_det_gpio);
		if (val > 0)
			cnt++;
		usleep_range(500, 600);
	}

	ret = (cnt >= 3) ? true : false;
	v4l2_dbg(1, debug, sd, "%s: %d\n", __func__, ret);

	return ret;
}

static inline bool no_signal(struct v4l2_subdev *sd)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	v4l2_dbg(1, debug, sd, "%s no signal:%d\n", __func__,
			lt8668sx->nosignal);

	return lt8668sx->nosignal;
}

static inline bool audio_present(struct v4l2_subdev *sd)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	return lt8668sx->is_audio_present;
}

static int get_audio_sampling_rate(struct v4l2_subdev *sd)
{
	static const int code_to_rate[] = {
		44100, 0, 48000, 32000, 22050, 384000, 24000, 352800,
		88200, 768000, 96000, 705600, 176400, 0, 192000, 0
	};

	if (no_signal(sd))
		return 0;

	return code_to_rate[2];
}

static inline unsigned int fps_calc(const struct v4l2_bt_timings *t)
{
	if (!V4L2_DV_BT_FRAME_HEIGHT(t) || !V4L2_DV_BT_FRAME_WIDTH(t))
		return 0;

	return DIV_ROUND_CLOSEST((unsigned int)t->pixelclock,
			V4L2_DV_BT_FRAME_HEIGHT(t) * V4L2_DV_BT_FRAME_WIDTH(t));
}

static bool lt8668sx_rcv_supported_res(struct v4l2_subdev *sd, u32 width,
		u32 height)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	u32 i;

	for (i = 0; i < lt8668sx->cfg_num; i++) {
		if ((lt8668sx->support_modes[i].width == width) &&
		    (lt8668sx->support_modes[i].height == height)) {
			break;
		}
	}

	if (i == lt8668sx->cfg_num) {
		v4l2_err(sd, "%s do not support res wxh: %dx%d\n", __func__,
				width, height);
		return false;
	} else {
		return true;
	}
}

static int lt8668sx_get_detected_timings(struct v4l2_subdev *sd,
				     struct v4l2_dv_timings *timings)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	struct v4l2_bt_timings *bt = &timings->bt;
	u32 hact, vact;
	u32 pixel_clock, fps, halt_pix_clk;
	u8 clk_h, clk_m, clk_l;
	u8 val_h, val_l;
	u8 interlaced, audio_rate;

	memset(timings, 0, sizeof(struct v4l2_dv_timings));

	clk_h = i2c_rd8(sd, PCLK_H);
	clk_m = i2c_rd8(sd, PCLK_M);
	clk_l = i2c_rd8(sd, PCLK_L);
	halt_pix_clk = ((clk_h << 16) | (clk_m << 8) | clk_l);
	pixel_clock = halt_pix_clk * 1000;

	val_h = i2c_rd8(sd, HACT_H);
	val_l = i2c_rd8(sd, HACT_L);
	hact = ((val_h << 8) | val_l);

	val_h = i2c_rd8(sd, VACT_H);
	val_l = i2c_rd8(sd, VACT_L);
	vact = (val_h << 8) | val_l;

	fps = i2c_rd8(sd, FRAMERATE);
	interlaced = i2c_rd8(sd, INTERLACED);
	audio_rate = i2c_rd8(sd, AUDIO_FS_VALUE);

	lt8668sx->nosignal = false;
	lt8668sx->is_audio_present = true;
	timings->type = V4L2_DV_BT_656_1120;
	bt->interlaced = interlaced;
	bt->width = hact;
	bt->height = vact;
	bt->pixelclock = pixel_clock;
	lt8668sx->cur_framerate = fps;
	lt8668sx->audio_sampling_rate = audio_rate;

	v4l2_info(sd, "act:%dx%d, pixclk:%d, fps:%d, inerlaced:%d, audio_rate:%dk\n",
		hact, vact, pixel_clock, fps, interlaced, audio_rate);

	if (!lt8668sx_rcv_supported_res(sd, hact, vact)) {
		lt8668sx->nosignal = true;
		v4l2_err(sd, "%s: rcv err res, return no signal!\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void lt8668sx_delayed_work_hotplug(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct lt8668sx *lt8668sx = container_of(dwork,
			struct lt8668sx, delayed_work_hotplug);
	struct v4l2_subdev *sd = &lt8668sx->sd;

	lt8668sx_s_ctrl_detect_tx_5v(sd);
}

static void lt8668sx_delayed_work_res_change(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct lt8668sx *lt8668sx = container_of(dwork,
			struct lt8668sx, delayed_work_res_change);
	struct v4l2_subdev *sd = &lt8668sx->sd;

	lt8668sx_format_change(sd);
}

static int lt8668sx_s_ctrl_detect_tx_5v(struct v4l2_subdev *sd)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	return v4l2_ctrl_s_ctrl(lt8668sx->detect_tx_5v_ctrl,
			tx_5v_power_present(sd));
}

static int lt8668sx_s_ctrl_audio_sampling_rate(struct v4l2_subdev *sd)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	return v4l2_ctrl_s_ctrl(lt8668sx->audio_sampling_rate_ctrl,
			get_audio_sampling_rate(sd));
}

static int lt8668sx_s_ctrl_audio_present(struct v4l2_subdev *sd)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	return v4l2_ctrl_s_ctrl(lt8668sx->audio_present_ctrl,
			audio_present(sd));
}

static int lt8668sx_update_controls(struct v4l2_subdev *sd)
{
	int ret = 0;

	ret |= lt8668sx_s_ctrl_detect_tx_5v(sd);
	ret |= lt8668sx_s_ctrl_audio_sampling_rate(sd);
	ret |= lt8668sx_s_ctrl_audio_present(sd);

	return ret;
}

static bool lt8668sx_match_timings(struct lt8668sx *lt8668sx, const struct v4l2_dv_timings *t1,
					const struct v4l2_dv_timings *t2)
{
	if (t1->type != t2->type || t1->type != V4L2_DV_BT_656_1120)
		return false;
	if (t1->bt.width == t2->bt.width &&
		t1->bt.height == t2->bt.height &&
		t1->bt.interlaced == t2->bt.interlaced &&
		lt8668sx->cur_framerate == lt8668sx->last_framerate)
		return true;

	return false;
}

static int lt8668sx_get_reso_dist(struct lt8668sx *lt8668sx, const struct lt8668sx_mode *mode,
				struct v4l2_dv_timings *timings)
{
	struct v4l2_bt_timings *bt = &timings->bt;
	u32 cur_fps, dist_fps;

	cur_fps = lt8668sx->cur_framerate;
	dist_fps = DIV_ROUND_CLOSEST(mode->max_fps.denominator, mode->max_fps.numerator);

	return abs(mode->width - bt->width) +
		abs(mode->height - bt->height) + abs(dist_fps - cur_fps);
}

static const struct lt8668sx_mode *
lt8668sx_find_best_fit(struct v4l2_subdev *sd)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < lt8668sx->cfg_num; i++) {
		dist = lt8668sx_get_reso_dist(lt8668sx,
				&lt8668sx->support_modes[i], &lt8668sx->timings);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}
	v4l2_info(sd, "find current mode: support_mode[%d], %dx%d@%dfps\n",
		cur_best_fit, lt8668sx->support_modes[cur_best_fit].width,
		lt8668sx->support_modes[cur_best_fit].height,
		DIV_ROUND_CLOSEST(lt8668sx->support_modes[cur_best_fit].max_fps.denominator,
		lt8668sx->support_modes[cur_best_fit].max_fps.numerator));

	return &lt8668sx->support_modes[cur_best_fit];
}

static void lt8668sx_print_dv_timings(struct v4l2_subdev *sd, const char *prefix)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	const struct v4l2_bt_timings *bt = &lt8668sx->timings.bt;
	const struct lt8668sx_mode *mode;
	u32 htot, vtot;
	u32 fps;

	mode = lt8668sx_find_best_fit(sd);
	lt8668sx->cur_mode = mode;
	htot = lt8668sx->cur_mode->hts_def;
	vtot = lt8668sx->cur_mode->vts_def;
	if (bt->interlaced)
		vtot /= 2;

	fps = lt8668sx->cur_framerate;

	if (prefix == NULL)
		prefix = "";

	v4l2_info(sd, "%s: %s%ux%u%s%u (%ux%u)\n", sd->name, prefix,
		bt->width, bt->height, bt->interlaced ? "i" : "p",
		fps, htot, vtot);
}

static void lt8668sx_format_change(struct v4l2_subdev *sd)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	struct v4l2_dv_timings timings;
	const struct v4l2_event lt8668sx_ev_fmt = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION,
	};
	struct v4l2_dv_timings default_timing =
				V4L2_DV_BT_CEA_640X480P59_94;

	if (lt8668sx_get_detected_timings(sd, &timings)) {
		timings = default_timing;
		v4l2_dbg(1, debug, sd, "%s: No signal\n", __func__);
	}

	if (!lt8668sx_match_timings(lt8668sx, &lt8668sx->timings, &timings)) {
		/* automatically set timing rather than set by user */
		lt8668sx_s_dv_timings(sd, &timings);
		lt8668sx_print_dv_timings(sd,
				"Format_change: New format: ");
		if (sd->devnode && !lt8668sx->i2c_client->irq)
			v4l2_subdev_notify_event(sd, &lt8668sx_ev_fmt);
	}
	if (sd->devnode && lt8668sx->i2c_client->irq)
		v4l2_subdev_notify_event(sd, &lt8668sx_ev_fmt);
}

static int lt8668sx_isr(struct v4l2_subdev *sd, u32 status, bool *handled)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	schedule_delayed_work(&lt8668sx->delayed_work_res_change, HZ / 20);
	*handled = true;

	return 0;
}

static irqreturn_t lt8668sx_res_change_irq_handler(int irq, void *dev_id)
{
	struct lt8668sx *lt8668sx = dev_id;
	bool handled;

	lt8668sx_isr(&lt8668sx->sd, 0, &handled);

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static irqreturn_t plugin_detect_irq_handler(int irq, void *dev_id)
{
	struct lt8668sx *lt8668sx = dev_id;

	schedule_delayed_work(&lt8668sx->delayed_work_hotplug, 100);

	return IRQ_HANDLED;
}

static void lt8668sx_irq_poll_timer(struct timer_list *t)
{
	struct lt8668sx *lt8668sx = from_timer(lt8668sx, t, timer);

	schedule_work(&lt8668sx->work_i2c_poll);
	mod_timer(&lt8668sx->timer, jiffies + msecs_to_jiffies(POLL_INTERVAL_MS));
}

static void lt8668sx_work_i2c_poll(struct work_struct *work)
{
	struct lt8668sx *lt8668sx = container_of(work,
			struct lt8668sx, work_i2c_poll);
	struct v4l2_subdev *sd = &lt8668sx->sd;

	lt8668sx_format_change(sd);
}

static int lt8668sx_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				    struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subdev_subscribe(sd, fh, sub);
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(sd, fh, sub);
	default:
		return -EINVAL;
	}
}

static int lt8668sx_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	*status = 0;
	*status |= no_signal(sd) ? V4L2_IN_ST_NO_SIGNAL : 0;

	v4l2_dbg(1, debug, sd, "%s: status = 0x%x\n", __func__, *status);

	return 0;
}

static int lt8668sx_s_dv_timings(struct v4l2_subdev *sd,
				 struct v4l2_dv_timings *timings)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	if (!timings)
		return -EINVAL;

	if (debug)
		v4l2_print_dv_timings(sd->name, "s_dv_timings: ",
				timings, false);

	if (lt8668sx_match_timings(lt8668sx, &lt8668sx->timings, timings)) {
		v4l2_dbg(1, debug, sd, "%s: no change\n", __func__);
		return 0;
	}

	lt8668sx->timings = *timings;
	lt8668sx->last_framerate = lt8668sx->cur_framerate;

	return 0;
}

static int lt8668sx_g_dv_timings(struct v4l2_subdev *sd,
				struct v4l2_dv_timings *timings)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	*timings = lt8668sx->timings;

	return 0;
}

static int lt8668sx_enum_dv_timings(struct v4l2_subdev *sd,
				struct v4l2_enum_dv_timings *timings)
{
	if (timings->pad != 0)
		return -EINVAL;

	return v4l2_enum_dv_timings_cap(timings,
			&lt8668sx_timings_cap, NULL, NULL);
}

static int lt8668sx_query_dv_timings(struct v4l2_subdev *sd,
				struct v4l2_dv_timings *timings)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	*timings = lt8668sx->timings;
	if (debug)
		v4l2_print_dv_timings(sd->name,
				"query_dv_timings: ", timings, false);

	if (!v4l2_valid_dv_timings(timings, &lt8668sx_timings_cap, NULL,
				NULL)) {
		v4l2_dbg(1, debug, sd, "%s: timings out of range\n",
				__func__);

		return -ERANGE;
	}

	return 0;
}

static int lt8668sx_dv_timings_cap(struct v4l2_subdev *sd,
				struct v4l2_dv_timings_cap *cap)
{
	if (cap->pad != 0)
		return -EINVAL;

	*cap = lt8668sx_timings_cap;

	return 0;
}

static int lt8668sx_g_mbus_config(struct v4l2_subdev *sd,
			unsigned int pad, struct v4l2_mbus_config *cfg)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	cfg->type = lt8668sx->bus_cfg.bus_type;
	cfg->bus.mipi_csi2 = lt8668sx->bus_cfg.bus.mipi_csi2;

	return 0;
}

static int lt8668sx_s_stream(struct v4l2_subdev *sd, int on)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	const struct lt8668sx_mode *mode;

	mode = lt8668sx_find_best_fit(sd);
	lt8668sx->cur_mode = mode;

	v4l2_info(sd, "%s: on: %d, %dx%d@%d\n",
				__func__, on, lt8668sx->cur_mode->width,
				lt8668sx->cur_mode->height,
		DIV_ROUND_CLOSEST(lt8668sx->cur_mode->max_fps.denominator,
				  lt8668sx->cur_mode->max_fps.numerator));

	msleep(100);

	return 0;
}

static int lt8668sx_enum_mbus_code(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_mbus_code_enum *code)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	switch (code->index) {
	case 0:
		code->code = lt8668sx->mbus_fmt_code;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int lt8668sx_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	if (fse->index >= lt8668sx->cfg_num)
		return -EINVAL;

	if (fse->code != lt8668sx->mbus_fmt_code)
		return -EINVAL;

	fse->min_width  = lt8668sx->support_modes[fse->index].width;
	fse->max_width  = lt8668sx->support_modes[fse->index].width;
	fse->max_height = lt8668sx->support_modes[fse->index].height;
	fse->min_height = lt8668sx->support_modes[fse->index].height;

	return 0;
}

static int lt8668sx_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *format)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	const struct lt8668sx_mode *mode;

	mutex_lock(&lt8668sx->confctl_mutex);
	format->format.code = lt8668sx->mbus_fmt_code;
	format->format.width = lt8668sx->timings.bt.width;
	format->format.height = lt8668sx->timings.bt.height;
	format->format.field =
		lt8668sx->timings.bt.interlaced ?
		V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
	format->format.colorspace = V4L2_COLORSPACE_SRGB;
	mutex_unlock(&lt8668sx->confctl_mutex);

	mode = lt8668sx_find_best_fit(sd);
	lt8668sx->cur_mode = mode;

	__v4l2_ctrl_s_ctrl_int64(lt8668sx->pixel_rate,
				LT8668SX_PIXEL_RATE);
	__v4l2_ctrl_s_ctrl(lt8668sx->link_freq,
				mode->mipi_freq_idx);

	v4l2_dbg(1, debug, sd, "%s: mode->mipi_freq_idx(%d)",
		 __func__, mode->mipi_freq_idx);

	v4l2_dbg(1, debug, sd, "%s: fmt code:%d, w:%d, h:%d, field code:%d\n",
			__func__, format->format.code, format->format.width,
			format->format.height, format->format.field);

	return 0;
}

static int lt8668sx_enum_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_interval_enum *fie)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	if (fie->index >= lt8668sx->cfg_num)
		return -EINVAL;

	fie->code = lt8668sx->mbus_fmt_code;

	fie->width = lt8668sx->support_modes[fie->index].width;
	fie->height = lt8668sx->support_modes[fie->index].height;
	fie->interval = lt8668sx->support_modes[fie->index].max_fps;

	return 0;
}

static int lt8668sx_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *format)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	const struct lt8668sx_mode *mode;

	/* is overwritten by get_fmt */
	u32 code = format->format.code;
	int ret = lt8668sx_get_fmt(sd, sd_state, format);

	format->format.code = code;

	if (ret)
		return ret;

	switch (code) {
	case MEDIA_BUS_FMT_UYVY8_2X8:
		break;
	case MEDIA_BUS_FMT_BGR888_1X24:
		break;
	default:
		return -EINVAL;
	}

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	lt8668sx->mbus_fmt_code = format->format.code;
	mode = lt8668sx_find_best_fit(sd);
	lt8668sx->cur_mode = mode;

	return 0;
}

static int lt8668sx_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *fi)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	const struct lt8668sx_mode *mode = lt8668sx->cur_mode;

	mutex_lock(&lt8668sx->confctl_mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&lt8668sx->confctl_mutex);

	return 0;
}

static void lt8668sx_get_module_inf(struct lt8668sx *lt8668sx,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, LT8668SX_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, lt8668sx->module_name, sizeof(inf->base.module));
	strscpy(inf->base.lens, lt8668sx->len_name, sizeof(inf->base.lens));
}

static long lt8668sx_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	long ret = 0;
	struct rkmodule_csi_dphy_param *dphy_param;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		lt8668sx_get_module_inf(lt8668sx, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDMI_MODE:
		*(int *)arg = RKMODULE_HDMIIN_MODE;
		break;
	case RKMODULE_SET_CSI_DPHY_PARAM:
		dphy_param = (struct rkmodule_csi_dphy_param *)arg;
		if (dphy_param->vendor == PHY_VENDOR_SAMSUNG)
			rk3588_dcphy_param = *dphy_param;
		dev_dbg(&lt8668sx->i2c_client->dev,
			"sensor set dphy param\n");
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = (struct rkmodule_csi_dphy_param *)arg;
		*dphy_param = rk3588_dcphy_param;
		dev_dbg(&lt8668sx->i2c_client->dev,
			"sensor get dphy param\n");
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static int lt8668sx_s_power(struct v4l2_subdev *sd, int on)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	int ret = 0;

	mutex_lock(&lt8668sx->confctl_mutex);

	if (lt8668sx->power_on == !!on)
		goto unlock_and_return;

	if (on)
		lt8668sx->power_on = true;
	else
		lt8668sx->power_on = false;

unlock_and_return:
	mutex_unlock(&lt8668sx->confctl_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long lt8668sx_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	long ret;
	int *seq;
	struct rkmodule_csi_dphy_param *dphy_param;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = lt8668sx_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_HDMI_MODE:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}

		ret = lt8668sx_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RKMODULE_SET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(dphy_param, up, sizeof(*dphy_param));
		if (!ret)
			ret = lt8668sx_ioctl(sd, cmd, dphy_param);
		else
			ret = -EFAULT;
		kfree(dphy_param);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = lt8668sx_ioctl(sd, cmd, dphy_param);
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

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int lt8668sx_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);
	struct v4l2_mbus_framefmt *try_fmt =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
	const struct lt8668sx_mode *def_mode = &lt8668sx->support_modes[0];

	mutex_lock(&lt8668sx->confctl_mutex);
	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = lt8668sx->mbus_fmt_code;
	try_fmt->field = V4L2_FIELD_NONE;
	mutex_unlock(&lt8668sx->confctl_mutex);

	return 0;
}
#endif

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops lt8668sx_internal_ops = {
	.open = lt8668sx_open,
};
#endif

static const struct v4l2_subdev_core_ops lt8668sx_core_ops = {
	.s_power = lt8668sx_s_power,
	.interrupt_service_routine = lt8668sx_isr,
	.subscribe_event = lt8668sx_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
	.ioctl = lt8668sx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = lt8668sx_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops lt8668sx_video_ops = {
	.g_input_status = lt8668sx_g_input_status,
	.s_dv_timings = lt8668sx_s_dv_timings,
	.g_dv_timings = lt8668sx_g_dv_timings,
	.query_dv_timings = lt8668sx_query_dv_timings,
	.s_stream = lt8668sx_s_stream,
	.g_frame_interval = lt8668sx_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops lt8668sx_pad_ops = {
	.enum_mbus_code = lt8668sx_enum_mbus_code,
	.enum_frame_size = lt8668sx_enum_frame_sizes,
	.enum_frame_interval = lt8668sx_enum_frame_interval,
	.set_fmt = lt8668sx_set_fmt,
	.get_fmt = lt8668sx_get_fmt,
	.enum_dv_timings = lt8668sx_enum_dv_timings,
	.dv_timings_cap = lt8668sx_dv_timings_cap,
	.get_mbus_config = lt8668sx_g_mbus_config,
};

static const struct v4l2_subdev_ops lt8668sx_ops = {
	.core = &lt8668sx_core_ops,
	.video = &lt8668sx_video_ops,
	.pad = &lt8668sx_pad_ops,
};

static const struct v4l2_ctrl_config lt8668sx_ctrl_audio_sampling_rate = {
	.id = RK_V4L2_CID_AUDIO_SAMPLING_RATE,
	.name = "Audio sampling rate",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 768000,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_ctrl_config lt8668sx_ctrl_audio_present = {
	.id = RK_V4L2_CID_AUDIO_PRESENT,
	.name = "Audio present",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
};

static int lt8668sx_init_v4l2_ctrls(struct lt8668sx *lt8668sx)
{
	const struct lt8668sx_mode *mode;
	struct v4l2_subdev *sd;
	int ret;

	mode = lt8668sx->cur_mode;
	sd = &lt8668sx->sd;
	ret = v4l2_ctrl_handler_init(&lt8668sx->hdl, 5);
	if (ret)
		return ret;
	if (lt8668sx->bus_cfg.bus_type == V4L2_MBUS_CSI2_DPHY &&
			lt8668sx->mbus_fmt_code == MEDIA_BUS_FMT_UYVY8_2X8)
		lt8668sx->link_freq = v4l2_ctrl_new_int_menu(&lt8668sx->hdl, NULL,
			V4L2_CID_LINK_FREQ,
			ARRAY_SIZE(link_freq_dphy_menu_items) - 1, 0,
			link_freq_dphy_menu_items);
	else if (lt8668sx->bus_cfg.bus_type != V4L2_MBUS_CSI2_DPHY &&
			lt8668sx->mbus_fmt_code == MEDIA_BUS_FMT_UYVY8_2X8)
		lt8668sx->link_freq = v4l2_ctrl_new_int_menu(&lt8668sx->hdl, NULL,
			V4L2_CID_LINK_FREQ,
			ARRAY_SIZE(link_freq_cphy_menu_items) - 1, 0,
			link_freq_cphy_menu_items);
	else
		lt8668sx->link_freq = v4l2_ctrl_new_int_menu(&lt8668sx->hdl, NULL,
			V4L2_CID_LINK_FREQ,
			ARRAY_SIZE(link_freq_cphy_rgb_menu_items) - 1, 0,
			link_freq_cphy_rgb_menu_items);

	lt8668sx->pixel_rate = v4l2_ctrl_new_std(&lt8668sx->hdl, NULL,
			V4L2_CID_PIXEL_RATE,
			0, LT8668SX_PIXEL_RATE, 1, LT8668SX_PIXEL_RATE);

	lt8668sx->detect_tx_5v_ctrl = v4l2_ctrl_new_std(&lt8668sx->hdl,
			NULL, V4L2_CID_DV_RX_POWER_PRESENT,
			0, 1, 0, 0);

	lt8668sx->audio_sampling_rate_ctrl =
		v4l2_ctrl_new_custom(&lt8668sx->hdl,
				&lt8668sx_ctrl_audio_sampling_rate, NULL);
	lt8668sx->audio_present_ctrl = v4l2_ctrl_new_custom(&lt8668sx->hdl,
			&lt8668sx_ctrl_audio_present, NULL);

	sd->ctrl_handler = &lt8668sx->hdl;
	if (lt8668sx->hdl.error) {
		ret = lt8668sx->hdl.error;
		v4l2_err(sd, "cfg v4l2 ctrls failed! ret:%d\n", ret);
		return ret;
	}

	__v4l2_ctrl_s_ctrl(lt8668sx->link_freq, mode->mipi_freq_idx);
	__v4l2_ctrl_s_ctrl_int64(lt8668sx->pixel_rate, LT8668SX_PIXEL_RATE);

	if (lt8668sx_update_controls(sd)) {
		ret = -ENODEV;
		v4l2_err(sd, "update v4l2 ctrls failed! ret:%d\n", ret);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_OF
static int lt8668sx_probe_of(struct lt8668sx *lt8668sx)
{
	struct device *dev = &lt8668sx->i2c_client->dev;
	struct device_node *node = dev->of_node;
	struct device_node *ep;
	int ret;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
			&lt8668sx->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
			&lt8668sx->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
			&lt8668sx->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
			&lt8668sx->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	if (of_property_read_bool(dev->of_node, "output-rgb"))
		lt8668sx->mbus_fmt_code = MEDIA_BUS_FMT_BGR888_1X24;

	lt8668sx->power_gpio = devm_gpiod_get_optional(dev, "power",
			GPIOD_OUT_LOW);
	if (IS_ERR(lt8668sx->power_gpio)) {
		dev_err(dev, "failed to get power gpio\n");
		ret = PTR_ERR(lt8668sx->power_gpio);
		return ret;
	}

	lt8668sx->reset_gpio = devm_gpiod_get_optional(dev, "reset",
			GPIOD_OUT_HIGH);
	if (IS_ERR(lt8668sx->reset_gpio)) {
		dev_err(dev, "failed to get reset gpio\n");
		ret = PTR_ERR(lt8668sx->reset_gpio);
		return ret;
	}

	lt8668sx->plugin_det_gpio = devm_gpiod_get_optional(dev, "plugin-det",
			GPIOD_IN);
	if (IS_ERR(lt8668sx->plugin_det_gpio)) {
		dev_err(dev, "failed to get plugin det gpio\n");
		ret = PTR_ERR(lt8668sx->plugin_det_gpio);
		return ret;
	}

	ep = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!ep) {
		dev_err(dev, "missing endpoint node\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(ep),
					&lt8668sx->bus_cfg);
	if (ret) {
		dev_err(dev, "failed to parse endpoint\n");
		goto put_node;
	}

	if (lt8668sx->bus_cfg.bus_type == V4L2_MBUS_CSI2_DPHY) {
		lt8668sx->support_modes = supported_modes_dphy;
		lt8668sx->cfg_num = ARRAY_SIZE(supported_modes_dphy);
	} else {
		lt8668sx->support_modes = supported_modes_cphy;
		lt8668sx->cfg_num = ARRAY_SIZE(supported_modes_cphy);
	}

	lt8668sx->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(lt8668sx->xvclk)) {
		dev_err(dev, "failed to get xvclk\n");
		ret = -EINVAL;
		goto put_node;
	}

	ret = clk_prepare_enable(lt8668sx->xvclk);
	if (ret) {
		dev_err(dev, "Failed! to enable xvclk\n");
		goto put_node;
	}

	lt8668sx->enable_hdcp = false;

	ret = 0;

put_node:
	of_node_put(ep);
	return ret;
}
#else
static inline int lt8668sx_probe_of(struct lt8668sx *state)
{
	return -ENODEV;
}
#endif

static int __lt8668sx_power_on(struct lt8668sx *lt8668sx)
{
	struct device *dev = &lt8668sx->i2c_client->dev;

	dev_info(dev, "lt8668sx power on\n");
	gpiod_set_value(lt8668sx->reset_gpio, 1);
	usleep_range(20000, 25000);
	gpiod_set_value(lt8668sx->power_gpio, 1);
	//delay 20ms before reset
	usleep_range(25000, 30000);
	gpiod_set_value(lt8668sx->reset_gpio, 0);
	usleep_range(25000, 30000);

	return 0;
}

static void __lt8668sx_power_off(struct lt8668sx *lt8668sx)
{
	struct device *dev = &lt8668sx->i2c_client->dev;

	dev_info(dev, "lt8668sx power off\n");

	if (!IS_ERR(lt8668sx->reset_gpio))
		gpiod_set_value(lt8668sx->reset_gpio, 1);

	if (!IS_ERR(lt8668sx->power_gpio))
		gpiod_set_value(lt8668sx->power_gpio, 0);
}

static int lt8668sx_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	return __lt8668sx_power_on(lt8668sx);
}

static int lt8668sx_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	__lt8668sx_power_off(lt8668sx);

	return 0;
}

static const struct dev_pm_ops lt8668sx_pm_ops = {
	.suspend = lt8668sx_suspend,
	.resume = lt8668sx_resume,
};

static int lt8668sx_check_chip_id(struct lt8668sx *lt8668sx)
{
	struct device *dev = &lt8668sx->i2c_client->dev;
	struct v4l2_subdev *sd = &lt8668sx->sd;
	u8 id_h, id_l;
	u32 chipid;
	int ret = 0;

	lt8668sx_i2c_enable(sd);
	id_l  = i2c_rd8(sd, CHIPID_REGL);
	id_h  = i2c_rd8(sd, CHIPID_REGH);
	lt8668sx_i2c_disable(sd);

	chipid = (id_h << 8) | id_l;
	if (chipid != LT8668SX_CHIPID) {
		dev_err(dev, "chipid err, read:%#x, expect:%#x\n",
				chipid, LT8668SX_CHIPID);
		return -EINVAL;
	}
	dev_info(dev, "check chipid ok, id:%#x", chipid);

	return ret;
}

static int lt8668sx_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct v4l2_dv_timings default_timing =
				V4L2_DV_BT_CEA_640X480P59_94;
	struct lt8668sx *lt8668sx;
	struct v4l2_subdev *sd;
	struct device *dev = &client->dev;
	char facing[2];
	int err;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	lt8668sx = devm_kzalloc(dev, sizeof(struct lt8668sx), GFP_KERNEL);
	if (!lt8668sx)
		return -ENOMEM;

	sd = &lt8668sx->sd;
	lt8668sx->i2c_client = client;
	lt8668sx->mbus_fmt_code = MEDIA_BUS_FMT_UYVY8_2X8;

	err = lt8668sx_probe_of(lt8668sx);
	if (err) {
		v4l2_err(sd, "lt8668sx_parse_of failed! err:%d\n", err);
		return err;
	}

	lt8668sx->timings = default_timing;
	lt8668sx->cur_mode = &lt8668sx->support_modes[0];

	__lt8668sx_power_on(lt8668sx);
	err = lt8668sx_check_chip_id(lt8668sx);
	if (err < 0)
		return err;

	INIT_DELAYED_WORK(&lt8668sx->delayed_work_hotplug,
			lt8668sx_delayed_work_hotplug);
	INIT_DELAYED_WORK(&lt8668sx->delayed_work_res_change,
			lt8668sx_delayed_work_res_change);

	if (lt8668sx->i2c_client->irq) {
		v4l2_dbg(1, debug, sd, "cfg lt8668sx irq!\n");
		err = devm_request_threaded_irq(dev,
				lt8668sx->i2c_client->irq,
				NULL, lt8668sx_res_change_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"lt8668sx", lt8668sx);
		if (err) {
			v4l2_err(sd, "request irq failed! err:%d\n", err);
			goto err_work_queues;
		}
	} else {
		v4l2_info(sd, "no irq, cfg poll!\n");
		INIT_WORK(&lt8668sx->work_i2c_poll, lt8668sx_work_i2c_poll);
		timer_setup(&lt8668sx->timer, lt8668sx_irq_poll_timer, 0);
		lt8668sx->timer.expires = jiffies +
				       msecs_to_jiffies(POLL_INTERVAL_MS);
		add_timer(&lt8668sx->timer);
	}

	lt8668sx->plugin_irq = gpiod_to_irq(lt8668sx->plugin_det_gpio);
	if (lt8668sx->plugin_irq < 0)
		dev_err(dev, "failed to get plugin det irq, maybe no use\n");

	err = devm_request_threaded_irq(dev, lt8668sx->plugin_irq, NULL,
			plugin_detect_irq_handler, IRQF_TRIGGER_FALLING |
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, "lt8668sx",
			lt8668sx);
	if (err)
		dev_err(dev, "failed to register plugin det irq (%d), maybe no use\n", err);

	mutex_init(&lt8668sx->confctl_mutex);
	err = lt8668sx_init_v4l2_ctrls(lt8668sx);
	if (err)
		goto err_free_hdl;

	client->flags |= I2C_CLIENT_SCCB;
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	v4l2_i2c_subdev_init(sd, client, &lt8668sx_ops);
	sd->internal_ops = &lt8668sx_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
#endif

#if defined(CONFIG_MEDIA_CONTROLLER)
	lt8668sx->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	err = media_entity_pads_init(&sd->entity, 1, &lt8668sx->pad);
	if (err < 0) {
		v4l2_err(sd, "media entity init failed! err:%d\n", err);
		goto err_free_hdl;
	}
#endif
	memset(facing, 0, sizeof(facing));
	if (strcmp(lt8668sx->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 lt8668sx->module_index, facing,
		 LT8668SX_NAME, dev_name(sd->dev));
	err = v4l2_async_register_subdev_sensor(sd);
	if (err < 0) {
		v4l2_err(sd, "v4l2 register subdev failed! err:%d\n", err);
		goto err_clean_entity;
	}

	err = v4l2_ctrl_handler_setup(sd->ctrl_handler);
	if (err) {
		v4l2_err(sd, "v4l2 ctrl handler setup failed! err:%d\n", err);
		goto err_clean_entity;
	}

	schedule_delayed_work(&lt8668sx->delayed_work_res_change, 100);

	v4l2_info(sd, "%s found @ 0x%x (%s)\n", client->name,
			client->addr << 1, client->adapter->name);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_free_hdl:
	v4l2_ctrl_handler_free(&lt8668sx->hdl);
	mutex_destroy(&lt8668sx->confctl_mutex);
err_work_queues:
	if (!lt8668sx->i2c_client->irq)
		flush_work(&lt8668sx->work_i2c_poll);
	cancel_delayed_work(&lt8668sx->delayed_work_hotplug);
	cancel_delayed_work(&lt8668sx->delayed_work_res_change);

	return err;
}

static void lt8668sx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lt8668sx *lt8668sx = to_lt8668sx(sd);

	if (!lt8668sx->i2c_client->irq) {
		del_timer_sync(&lt8668sx->timer);
		flush_work(&lt8668sx->work_i2c_poll);
	}
	cancel_delayed_work_sync(&lt8668sx->delayed_work_hotplug);
	cancel_delayed_work_sync(&lt8668sx->delayed_work_res_change);
	v4l2_async_unregister_subdev(sd);
	v4l2_device_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&lt8668sx->hdl);
	mutex_destroy(&lt8668sx->confctl_mutex);
	clk_disable_unprepare(lt8668sx->xvclk);
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id lt8668sx_of_match[] = {
	{ .compatible = "lontium,lt8668sx" },
	{},
};
MODULE_DEVICE_TABLE(of, lt8668sx_of_match);
#endif

static struct i2c_driver lt8668sx_driver = {
	.driver = {
		.name = LT8668SX_NAME,
		.pm = &lt8668sx_pm_ops,
		.of_match_table = of_match_ptr(lt8668sx_of_match),
	},
	.probe = lt8668sx_probe,
	.remove = lt8668sx_remove,
};

static int __init lt8668sx_driver_init(void)
{
	return i2c_add_driver(&lt8668sx_driver);
}

static void __exit lt8668sx_driver_exit(void)
{
	i2c_del_driver(&lt8668sx_driver);
}

device_initcall_sync(lt8668sx_driver_init);
module_exit(lt8668sx_driver_exit);

MODULE_DESCRIPTION("Lontium lt8668sx HDMI to CSI-2 bridge driver");
MODULE_AUTHOR("Jianwei Fan <jianwei.fan@rock-chips.com>");
MODULE_LICENSE("GPL");
