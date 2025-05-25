// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 *
 * Author: Dingxian Wen <shawn.wen@rock-chips.com>
 */

#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/extcon-provider.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/rk-camera-module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/v4l2-dv-timings.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/workqueue.h>
#include <linux/rk_hdmirx_class.h>
#include <media/v4l2-controls_rockchip.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <video/videomode.h>
#include <linux/rk_hdmirx_config.h>
#if IS_REACHABLE(CONFIG_VIDEO_ROCKCHIP_CIF)
#include "../../platform/rockchip/cif/rkcif-externel.h"
#endif

#include "rk628.h"
#include "rk628_combrxphy.h"
#include "rk628_combtxphy.h"
#include "rk628_csi.h"
#include "rk628_cru.h"
#include "rk628_dsi.h"
#include "rk628_hdmirx.h"
#include "rk628_mipi_dphy.h"
#include "rk628_post_process.h"

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level (0-3)");

#define DRIVER_VERSION			KERNEL_VERSION(0, 0x1, 0x0)
#define RK628_CSI_NAME			"rk628-csi"

enum tx_mode_type {
	CSI_MODE,
	DSI_MODE,
};

struct rk628_plat_data {
	int bus_fmt;
	int tx_mode;
};

struct rk628_csi {
	struct device *dev;
	struct i2c_client *i2c_client;
	struct rk628 *rk628;
	struct media_pad pad;
	struct v4l2_subdev sd;
	struct v4l2_dv_timings src_timings;
	struct v4l2_dv_timings timings;
	struct v4l2_ctrl_handler hdl;
	struct v4l2_ctrl *detect_tx_5v_ctrl;
	struct v4l2_ctrl *audio_sampling_rate_ctrl;
	struct v4l2_ctrl *audio_present_ctrl;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *power_gpio;
	struct gpio_desc *plugin_det_gpio;
	struct clk *soc_24M;
	struct clk *clk_hdmirx_aud;
	struct clk *clk_vop;
	struct clk *clk_rx_read;
	struct delayed_work delayed_work_enable_hotplug;
	struct delayed_work delayed_work_res_change;
	struct timer_list timer;
	struct work_struct work_i2c_poll;
	struct mutex confctl_mutex;
	struct rkmodule_multi_dev_info multi_dev_info;
	const struct rk628_csi_mode *cur_mode;
	const char *module_facing;
	const char *module_name;
	const char *len_name;
	u32 module_index;
	u8 edid_blocks_written;
	u64 lane_mbps;
	u8 csi_lanes_in_use;
	u32 mbus_fmt_code;
	u8 fps;
	u8 edid_version;
	u32 stream_state;
	int hdmirx_irq;
	int plugin_irq;
	int lock_fail_time;
	bool nosignal;
	bool rxphy_pwron;
	bool txphy_pwron;
	bool enable_hdcp;
	bool scaler_en;
	bool hpd_output_inverted;
	bool avi_rcv_rdy;
	bool vid_ints_en;
	bool continues_clk;
	bool cec_enable;
	bool dvi_mode;
	struct rk628_hdmirx_cec *cec;
	struct rk628_hdcp hdcp;
	bool i2s_enable_default;
	HAUDINFO audio_info;
	struct extcon_dev *extcon;
	struct rk628_combtxphy *txphy;
	struct rk628_dsi dsi;
	const struct rk628_plat_data *plat_data;
	struct device *classdev;
	bool is_streaming;
	bool csi_ints_en;
	bool dual_mipi_use;
	enum user_color_range user_color_range;
};

struct rk628_csi_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
};

struct rk628_edid {
	u8 version;
	u8 *data;
};

static const s64 link_freq_menu_items[] = {
	RK628_CSI_LINK_FREQ_LOW,
	RK628_CSI_LINK_FREQ_HIGH,
	RK628_CSI_LINK_FREQ_925M,
};

static const struct v4l2_dv_timings_cap rk628_csi_timings_cap = {
	.type = V4L2_DV_BT_656_1120,
	/* keep this initialization for compatibility with GCC < 4.4.6 */
	.reserved = { 0 },
	V4L2_INIT_BT_TIMINGS(1, 10000, 1, 10000, 0, 600000000,
			V4L2_DV_BT_STD_CEA861 | V4L2_DV_BT_STD_DMT |
			V4L2_DV_BT_STD_GTF | V4L2_DV_BT_STD_CVT,
			V4L2_DV_BT_CAP_PROGRESSIVE | V4L2_DV_BT_CAP_INTERLACED |
			V4L2_DV_BT_CAP_REDUCED_BLANKING |
			V4L2_DV_BT_CAP_CUSTOM)
};

static u8 edid_init_data[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x49, 0x73, 0x8D, 0x62, 0x00, 0x88, 0x88, 0x88,
	0x08, 0x1E, 0x01, 0x03, 0x80, 0x00, 0x00, 0x78,
	0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27,
	0x12, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
	0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
	0x45, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E,
	0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20,
	0x6E, 0x28, 0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00,
	0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
	0x37, 0x34, 0x39, 0x2D, 0x66, 0x48, 0x44, 0x37,
	0x32, 0x30, 0x0A, 0x20, 0x00, 0x00, 0x00, 0xFD,
	0x00, 0x14, 0x78, 0x01, 0xFF, 0x1D, 0x00, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x18,

	0x02, 0x03, 0x1D, 0x71, 0x47, 0x5F, 0x90, 0x22,
	0x04, 0x11, 0x02, 0x01, 0x23, 0x09, 0x07, 0x01,
	0x83, 0x01, 0x00, 0x00, 0x65, 0x03, 0x0C, 0x00,
	0x10, 0x00, 0xE2, 0x00, 0xCB, 0x02, 0x3A, 0x80,
	0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C, 0x45,
	0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x01,
	0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E,
	0x28, 0x55, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00,
	0x1E, 0x02, 0x3A, 0x80, 0xD0, 0x72, 0x38, 0x2D,
	0x40, 0x10, 0x2C, 0x45, 0x80, 0x20, 0xC2, 0x31,
	0x00, 0x00, 0x1E, 0x01, 0x1D, 0x80, 0x18, 0x71,
	0x38, 0x2D, 0x40, 0x58, 0x2C, 0x45, 0x00, 0xC0,
	0x6C, 0x00, 0x00, 0x00, 0x18, 0x01, 0x1D, 0x80,
	0x18, 0x71, 0x1C, 0x16, 0x20, 0x58, 0x2C, 0x25,
	0x00, 0xC0, 0x6C, 0x00, 0x00, 0x00, 0x18, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
};

static u8 rk628f_edid_init_data[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x24, 0xD0, 0x8F, 0x62, 0x01, 0x00, 0x00, 0x00,
	0x2D, 0x21, 0x01, 0x03, 0x80, 0x78, 0x44, 0x78,
	0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
	0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x61, 0x40,
	0x01, 0x01, 0x81, 0x00, 0x95, 0x00, 0xA9, 0xC0,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x08, 0xE8,
	0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
	0x8A, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E,
	0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
	0x58, 0x2C, 0x45, 0x00, 0xB9, 0xA8, 0x42, 0x00,
	0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x49,
	0x46, 0x50, 0x20, 0x44, 0x69, 0x73, 0x70, 0x6C,
	0x61, 0x79, 0x0A, 0x20, 0x00, 0x00, 0x00, 0xFD,
	0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xA8,

	0x02, 0x03, 0x3C, 0xF2, 0x4D, 0x01, 0x03, 0x12,
	0x13, 0x84, 0x22, 0x1F, 0x90, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x23, 0x09, 0x07, 0x07, 0x83, 0x01,
	0x00, 0x00, 0x6D, 0x03, 0x0C, 0x00, 0x10, 0x00,
	0x00, 0x44, 0x20, 0x00, 0x60, 0x03, 0x02, 0x01,
	0x67, 0xD8, 0x5D, 0xC4, 0x01, 0x78, 0x80, 0x00,
	0xE3, 0x05, 0x03, 0x01, 0xE4, 0x0F, 0x00, 0x18,
	0x00, 0xE2, 0x00, 0xCB, 0x02, 0x3A, 0x80, 0x18,
	0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C, 0x45, 0x00,
	0xB9, 0xA8, 0x42, 0x00, 0x00, 0x1E, 0x08, 0xE8,
	0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
	0x8A, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23,
};


static struct rk628_edid edid_data[] = {
	{
		.version = 1,
		.data = edid_init_data,
	},
	{
		.version = 2,
		.data = rk628f_edid_init_data,
	},
};

static const unsigned int rk628_csi_extcon_cable[] = {
	EXTCON_JACK_VIDEO_IN,
	EXTCON_NONE,
};

static const struct mipi_timing rk628d_csi_mipi = {
	0x0b, 0x53, 0x10, 0x5b, 0x0b, 0x43, 0x2c, 0x50, 0x0f
};

static const struct mipi_timing rk628f_csi0_mipi = {
	0x0b, 0x53, 0x10, 0x5b, 0x0b, 0x43, 0x2c, 0x50, 0x0f
};

static const struct mipi_timing rk628f_csi1_mipi = {
//data_lp, data-pre, data-zero, data-trail, clk_lp, clk-pre, clk-zero, clk-trail, clk-post
	0x0b, 0x53, 0x10, 0x5b, 0x0b, 0x43, 0x2c, 0x50, 0x0f
};

static const struct mipi_timing rk628f_dsi0_mipi = {
	0x10, 0x70, 0x1c, 0x7f, 0x10, 0x70, 0x3f, 0x7f, 0x1f
};

static struct rkmodule_csi_dphy_param rk3588_dcphy_param = {
	.vendor = PHY_VENDOR_SAMSUNG,
	.lp_vol_ref = 0,
	.lp_hys_sw = {3, 0, 0, 0},
	.lp_escclk_pol_sel = {1, 0, 0, 0},
	.skew_data_cal_clk = {0, 3, 3, 3},
	.clk_hs_term_sel = 2,
	.data_hs_term_sel = {2, 2, 2, 2},
	.reserved = {0},
};

static const struct rk628_csi_mode supported_modes[] = {
	{
		.width = 4096,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
	}, {
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
	}, {
		.width = 3840,
		.height = 2160,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.hts_def = 4400,
		.vts_def = 2250,
	}, {
		.width = 1920,
		.height = 1080,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 2200,
		.vts_def = 1125,
	}, {
		.width = 1280,
		.height = 720,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 1650,
		.vts_def = 750,
	}, {
		.width = 720,
		.height = 576,
		.max_fps = {
			.numerator = 10000,
			.denominator = 500000,
		},
		.hts_def = 864,
		.vts_def = 625,
	}, {
		.width = 720,
		.height = 480,
		.max_fps = {
			.numerator = 10000,
			.denominator = 600000,
		},
		.hts_def = 858,
		.vts_def = 525,
	},
};

static struct v4l2_dv_timings dst_timing = {
	.type = V4L2_DV_BT_656_1120,
	.bt = {
		.interlaced = V4L2_DV_PROGRESSIVE,
		.width = 1920,
		.height = 1080,
		.hfrontporch = 88,
		.hsync = 44,
		.hbackporch = 148,
		.vfrontporch = 4,
		.vsync = 5,
		.vbackporch = 36,
		.pixelclock = 148500000,
	},
};

static void rk628_post_process_setup(struct v4l2_subdev *sd);
static void rk628_csi_enable_interrupts(struct v4l2_subdev *sd, bool en);
static void rk628_csi_enable_csi_interrupts(struct v4l2_subdev *sd, bool en);
static void rk628_csi_clear_csi_interrupts(struct v4l2_subdev *sd);
static int rk628_csi_s_ctrl_detect_tx_5v(struct v4l2_subdev *sd);
static int rk628_csi_s_dv_timings(struct v4l2_subdev *sd,
				 struct v4l2_dv_timings *timings);
static int rk628_csi_s_edid(struct v4l2_subdev *sd,
				struct v4l2_subdev_edid *edid);
static int mipi_dphy_power_on(struct rk628_csi *csi);
static void mipi_dphy_power_off(struct rk628_csi *csi);
static int rk628_hdmirx_inno_phy_power_on(struct v4l2_subdev *sd);
static int rk628_hdmirx_inno_phy_power_off(struct v4l2_subdev *sd);
static int rk628_hdmirx_phy_setup(struct v4l2_subdev *sd);
static int rk628_csi_format_change(struct v4l2_subdev *sd);
static void enable_stream(struct v4l2_subdev *sd, bool enable);
static void rk628_hdmirx_vid_enable(struct v4l2_subdev *sd, bool en);
static void rk628_csi_set_csi(struct v4l2_subdev *sd);
static void rk628_hdmirx_hpd_ctrl(struct v4l2_subdev *sd, bool en);
static bool rk628_rcv_supported_res(struct v4l2_subdev *sd, u32 width,
				    u32 height);
static void rk628_dsi_set_scs(struct rk628_csi *csi);
static void rk628_dsi_enable(struct v4l2_subdev *sd);
static void rk628_csi_disable_stream(struct v4l2_subdev *sd);

static inline struct rk628_csi *to_csi(struct v4l2_subdev *sd)
{
	return container_of(sd, struct rk628_csi, sd);
}

static bool tx_5v_power_present(struct v4l2_subdev *sd)
{
	bool ret;
	struct rk628_csi *csi = to_csi(sd);

	ret = rk628_hdmirx_tx_5v_power_detect(csi->plugin_det_gpio);
	v4l2_dbg(2, debug, sd, "%s: %d\n", __func__, ret);
	if (csi->rk628->is_suspend)
		ret = false;

	return ret;
}

static inline bool no_signal(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	v4l2_dbg(1, debug, sd, "%s no signal:%d\n", __func__, csi->nosignal);
	return csi->nosignal;
}

static inline bool audio_present(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	return rk628_hdmirx_audio_present(csi->audio_info);
}

static int get_audio_sampling_rate(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	if (no_signal(sd))
		return 0;

	return rk628_hdmirx_audio_fs(csi->audio_info);
}

static void rk628_hdmirx_ctrl_enable(struct v4l2_subdev *sd, int en)
{
	u32 mask;
	struct rk628_csi *csi = to_csi(sd);

	if (en) {
		/* don't enable audio until N CTS updated */
		mask = HDMI_ENABLE_MASK;
		v4l2_dbg(1, debug, sd, "%s: %#x %d\n", __func__, mask, en);
		rk628_i2c_update_bits(csi->rk628, HDMI_RX_DMI_DISABLE_IF,
				   mask, HDMI_ENABLE(1) | AUD_ENABLE(1));
	} else {
		mask = AUD_ENABLE_MASK | HDMI_ENABLE_MASK;
		v4l2_dbg(1, debug, sd, "%s: %#x %d\n", __func__, mask, en);
		rk628_i2c_update_bits(csi->rk628, HDMI_RX_DMI_DISABLE_IF,
				   mask, HDMI_ENABLE(0) | AUD_ENABLE(0));
	}
}

static int rk628_csi_get_detected_timings(struct v4l2_subdev *sd,
				     struct v4l2_dv_timings *timings)
{
	struct rk628_csi *csi = to_csi(sd);
	struct v4l2_bt_timings *bt = &timings->bt;
	int ret;

	ret = rk628_hdmirx_get_timings(csi->rk628, timings);
	if (ret)
		return ret;

	if ((bt->pixelclock > 300000000 && csi->rk628->version >= RK628F_VERSION) ||
		(bt->width > 2048 && csi->plat_data->tx_mode == DSI_MODE)) {
		v4l2_info(sd, "rk628f detect pixclk more than 300M, use dual mipi mode\n");
		csi->rk628->dual_mipi = true;
	} else {
		v4l2_info(sd, "pixclk less than 300M, use single mipi mode\n");
		csi->rk628->dual_mipi = false;
	}

	v4l2_dbg(1, debug, sd, "hfp:%d, hs:%d, hbp:%d, vfp:%d, vs:%d, vbp:%d, interlace:%d\n",
		 bt->hfrontporch, bt->hsync, bt->hbackporch, bt->vfrontporch, bt->vsync,
		 bt->vbackporch, bt->interlaced);

	csi->src_timings = *timings;
	if (csi->scaler_en)
		*timings = csi->timings;

	return ret;

}

static void rk628_csi_hdmirx_reset(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_hdmirx_audio_cancel_work_audio(csi->audio_info, true);
	disable_irq(csi->plugin_irq);
	disable_irq(csi->hdmirx_irq);
	rk628_hdmirx_controller_reset(csi->rk628);
	csi->hdcp.hdcp_start = false;
	enable_irq(csi->plugin_irq);
	enable_irq(csi->hdmirx_irq);
}

static void rk628_hdmirx_plugout(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	enable_stream(sd, false);
	csi->nosignal = true;
	csi->hdcp.hdcp_start = false;
	rk628_csi_enable_interrupts(sd, false);
	cancel_delayed_work(&csi->delayed_work_res_change);
	rk628_hdmirx_audio_cancel_work_audio(csi->audio_info, true);
	rk628_csi_hdmirx_reset(sd);
	rk628_hdmirx_hpd_ctrl(sd, false);
	rk628_hdmirx_inno_phy_power_off(sd);
	rk628_hdmirx_verisyno_phy_power_off(csi->rk628);
	mipi_dphy_power_off(csi);
}

static void rk628_hdmirx_config_all(struct v4l2_subdev *sd)
{
	int ret, delay;
	struct rk628_csi *csi = to_csi(sd);

	ret = rk628_hdmirx_phy_setup(sd);
	if (ret == LOCK_OK && !rk628_hdmirx_scdc_ced_err(csi->rk628)) {
		ret = rk628_csi_format_change(sd);
		if (ret == LOCK_OK) {
			csi->lock_fail_time = 0;
			csi->nosignal = false;
			return;
		}
	}

	if (ret == LOCK_RESET || rk628_hdmirx_scdc_ced_err(csi->rk628)) {
		rk628_csi_hdmirx_reset(sd);
		rk628_hdmirx_hpd_ctrl(sd, true);
		schedule_delayed_work(&csi->delayed_work_enable_hotplug,
				      msecs_to_jiffies(100));
	} else if (ret == LOCK_FAIL) {
		if (rk628_hdmirx_get_arc_enable(csi->audio_info))
			return;

		rk628_hdmirx_plugout(sd);
		csi->lock_fail_time++;
		v4l2_dbg(1, debug, sd, "%s: lock fail time: %d\n",
			 __func__, csi->lock_fail_time);
		delay = 800 + 800 * ((csi->lock_fail_time + 1) % 2);
		schedule_delayed_work(&csi->delayed_work_enable_hotplug,
				      msecs_to_jiffies(delay));
	}
}

static void rk628_csi_delayed_work_enable_hotplug(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rk628_csi *csi = container_of(dwork, struct rk628_csi,
			delayed_work_enable_hotplug);
	struct v4l2_subdev *sd = &csi->sd;
	bool plugin;

	mutex_lock(&csi->confctl_mutex);
	rk628_set_bg_enable(csi->rk628, false);
	csi->avi_rcv_rdy = false;
	plugin = tx_5v_power_present(sd);
	v4l2_ctrl_s_ctrl(csi->detect_tx_5v_ctrl, plugin);
	v4l2_dbg(1, debug, sd, "%s: 5v_det:%d\n", __func__, plugin);
	if (plugin) {
		extcon_set_state_sync(csi->extcon, EXTCON_JACK_VIDEO_IN, true);
		rk628_csi_enable_interrupts(sd, false);
		cancel_delayed_work(&csi->delayed_work_res_change);
		rk628_hdmirx_audio_setup(csi->audio_info);
		rk628_hdmirx_set_hdcp(csi->rk628, &csi->hdcp, csi->hdcp.enable);
		rk628_hdmirx_controller_setup(csi->rk628);
		rk628_hdmirx_hpd_ctrl(sd, true);
		rk628_hdmirx_config_all(sd);
		if (csi->cec && csi->cec->adap)
			rk628_hdmirx_cec_state_reconfiguration(csi->rk628, csi->cec);
		rk628_csi_enable_interrupts(sd, true);
	} else {
		extcon_set_state_sync(csi->extcon, EXTCON_JACK_VIDEO_IN, false);
		rk628_hdmirx_plugout(sd);
	}
	mutex_unlock(&csi->confctl_mutex);
}

static int rk628_check_resulotion_change(struct v4l2_subdev *sd)
{
	u32 val;
	struct rk628_csi *csi = to_csi(sd);
	u32 htotal, vtotal;
	u32 old_htotal, old_vtotal;
	struct v4l2_bt_timings *bt = &csi->src_timings.bt;

	if (csi->rk628->version >= RK628F_VERSION)
		return 1;

	rk628_i2c_read(csi->rk628, HDMI_RX_MD_HT1, &val);
	htotal = (val >> 16) & 0xffff;
	rk628_i2c_read(csi->rk628, HDMI_RX_MD_VTL, &val);
	vtotal = val & 0xffff;

	old_htotal = bt->hfrontporch + bt->hsync + bt->width + bt->hbackporch;
	old_vtotal = bt->vfrontporch + bt->vsync + bt->height + bt->vbackporch;

	v4l2_dbg(1, debug, sd, "new mode: %d x %d\n", htotal, vtotal);
	v4l2_dbg(1, debug, sd, "old mode: %d x %d\n", old_htotal, old_vtotal);

	if (htotal != old_htotal || vtotal != old_vtotal)
		return 1;

	return 0;
}

static void rk628_delayed_work_res_change(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rk628_csi *csi = container_of(dwork, struct rk628_csi,
			delayed_work_res_change);
	struct v4l2_subdev *sd = &csi->sd;
	bool plugin;

	mutex_lock(&csi->confctl_mutex);
	rk628_set_bg_enable(csi->rk628, false);
	enable_stream(sd, false);
	csi->nosignal = true;
	csi->avi_rcv_rdy = false;
	plugin = tx_5v_power_present(sd);
	v4l2_dbg(1, debug, sd, "%s: 5v_det:%d\n", __func__, plugin);
	if (plugin) {
		if (rk628_check_resulotion_change(sd)) {
			v4l2_dbg(1, debug, sd, "res change, recfg ctrler and phy!\n");
			if (csi->rk628->version >= RK628F_VERSION) {
				rk628_csi_enable_interrupts(sd, false);
				rk628_hdmirx_audio_cancel_work_audio(csi->audio_info, true);
				rk628_csi_hdmirx_reset(sd);
				rk628_hdmirx_verisyno_phy_power_off(csi->rk628);
				schedule_delayed_work(&csi->delayed_work_enable_hotplug,
						      msecs_to_jiffies(100));
			} else {
				rk628_hdmirx_audio_cancel_work_audio(csi->audio_info, true);
				rk628_hdmirx_inno_phy_power_off(sd);
				rk628_csi_hdmirx_reset(sd);
				rk628_hdmirx_audio_setup(csi->audio_info);
				rk628_hdmirx_set_hdcp(csi->rk628, &csi->hdcp, csi->hdcp.enable);
				rk628_hdmirx_controller_setup(csi->rk628);
				rk628_hdmirx_hpd_ctrl(sd, true);
				rk628_hdmirx_config_all(sd);
				rk628_csi_enable_interrupts(sd, true);
				rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON0,
						      SW_I2S_DATA_OEN_MASK,
						      SW_I2S_DATA_OEN(0));
			}
		} else {
			rk628_csi_format_change(sd);
			csi->nosignal = false;
			rk628_csi_enable_interrupts(sd, true);
		}
	}
	mutex_unlock(&csi->confctl_mutex);
}

static void rk628_hdmirx_hpd_ctrl(struct v4l2_subdev *sd, bool en)
{
	u8 en_level, set_level;
	struct rk628_csi *csi = to_csi(sd);

	v4l2_dbg(1, debug, sd, "%s: %sable, hpd invert:%d\n", __func__,
			en ? "en" : "dis", csi->hpd_output_inverted);
	en_level = csi->hpd_output_inverted ? 0 : 1;
	set_level = en ? en_level : !en_level;
	rk628_i2c_update_bits(csi->rk628, HDMI_RX_HDMI_SETUP_CTRL,
			HOT_PLUG_DETECT_MASK, HOT_PLUG_DETECT(set_level));

	if (csi->cec_enable && csi->cec)
		rk628_hdmirx_cec_hpd(csi->cec, tx_5v_power_present(sd));
}


static int rk628_csi_s_ctrl_detect_tx_5v(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	return v4l2_ctrl_s_ctrl(csi->detect_tx_5v_ctrl,
			tx_5v_power_present(sd));
}

static int rk628_csi_s_ctrl_audio_sampling_rate(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	return v4l2_ctrl_s_ctrl(csi->audio_sampling_rate_ctrl,
			get_audio_sampling_rate(sd));
}

static int rk628_csi_s_ctrl_audio_present(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	return v4l2_ctrl_s_ctrl(csi->audio_present_ctrl,
			audio_present(sd));
}

static int rk628_csi_update_controls(struct v4l2_subdev *sd)
{
	int ret = 0;

	ret |= rk628_csi_s_ctrl_detect_tx_5v(sd);
	ret |= rk628_csi_s_ctrl_audio_sampling_rate(sd);
	ret |= rk628_csi_s_ctrl_audio_present(sd);

	return ret;
}

static void rk628_csi0_cru_reset(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_control_assert(csi->rk628, RGU_CSI);
	udelay(10);
	rk628_control_deassert(csi->rk628, RGU_CSI);
}

static void rk628_csi1_cru_reset(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_control_assert(csi->rk628, RGU_CSI1);
	udelay(10);
	rk628_control_deassert(csi->rk628, RGU_CSI1);
}

static void rk628_mipi_txdata_reset(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_control_assert(csi->rk628, RGU_TXDATA);
	rk628_control_assert(csi->rk628, RGU_TXBYTEHS);
	usleep_range(1000, 1100);
	rk628_control_deassert(csi->rk628, RGU_TXDATA);
	rk628_control_deassert(csi->rk628, RGU_TXBYTEHS);
}

static void rk628_csi_soft_reset(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_i2c_write(csi->rk628, CSITX_SYS_CTRL0_IMD, 0x1);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_write(csi->rk628, CSITX1_SYS_CTRL0_IMD, 0x1);

	rk628_mipi_txdata_reset(sd);
	rk628_i2c_write(csi->rk628, CSITX_SYS_CTRL0_IMD, 0x0);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_write(csi->rk628, CSITX1_SYS_CTRL0_IMD, 0x0);
}

static void enable_csitx(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);
	u32 mask = SW_OUTPUT_MODE_MASK;
	u32 val = SW_OUTPUT_MODE(OUTPUT_MODE_CSI);

	if (csi->rk628->version == RK628F_VERSION) {
		mask = SW_OUTPUT_COMBTX_MODE_MASK;
		val = SW_OUTPUT_COMBTX_MODE(OUTPUT_MODE_CSI - 1);
		rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON3,
				      GRF_AS_DSIPHY_MASK,
				      GRF_AS_DSIPHY(0));
	}
	rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON0, mask, val);
	//enable dphy1 and split mode
	rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON3, GRF_DPHY_CH1_EN_MASK,
			     csi->rk628->dual_mipi ? GRF_DPHY_CH1_EN(1) : 0);
	rk628_i2c_update_bits(csi->rk628, GRF_POST_PROC_CON, SW_SPLIT_EN,
			     csi->rk628->dual_mipi ? SW_SPLIT_EN : 0);
	rk628_csi_set_csi(sd);

	rk628_csi_soft_reset(sd);
	usleep_range(5000, 5500);
	//disabled csi state ints
	rk628_i2c_write(csi->rk628, CSITX_INTR_EN_IMD, 0x0fff0000);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_write(csi->rk628, CSITX1_INTR_EN_IMD, 0x0fff0000);

	rk628_i2c_update_bits(csi->rk628, CSITX_CSITX_EN,
					DPHY_EN_MASK |
					CSITX_EN_MASK,
					DPHY_EN(1) |
					CSITX_EN(1));
	rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);
	if (csi->rk628->version >= RK628F_VERSION) {
		rk628_i2c_update_bits(csi->rk628, CSITX1_CSITX_EN,
					DPHY_EN_MASK |
					CSITX_EN_MASK,
					DPHY_EN(1) |
					CSITX_EN(1));
		rk628_i2c_write(csi->rk628, CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);
	}

	rk628_i2c_write(csi->rk628, CSITX_ERR_INTR_CLR_IMD, 0xffffffff);
	if (csi->rk628->version <= RK628D_VERSION)
		rk628_i2c_update_bits(csi->rk628, CSITX_SYS_CTRL1,
				BYPASS_SELECT_MASK, BYPASS_SELECT(0));
	rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);

	if (csi->rk628->version >= RK628F_VERSION) {
		rk628_i2c_write(csi->rk628, CSITX1_ERR_INTR_CLR_IMD, 0xffffffff);
		rk628_i2c_write(csi->rk628, CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);
	}
	csi->is_streaming = true;
}

static void rk628_dsi_set_scs(struct rk628_csi *csi)
{
	u8 video_fmt;
	u32 val;
	int avi_rdy;

	avi_rdy = rk628_is_avi_ready(csi->rk628, csi->avi_rcv_rdy);
	rk628_i2c_read(csi->rk628, HDMI_RX_PDEC_AVI_PB, &val);
	video_fmt = (val & VIDEO_FORMAT_MASK) >> 5;
	v4l2_info(&csi->sd, "%s PDEC_AVI_PB:%#x, video format:%d\n",
			__func__, val, video_fmt);
	if (csi->rk628->version == RK628D_VERSION) {
		if (video_fmt) {
			if (csi->dsi.vid_mode == VIDEO_MODE)
				rk628_i2c_write(csi->rk628, GRF_CSC_CTRL_CON,
						SW_Y2R_EN(1) | SW_YUV2VYU_SWP(1));
			else
				rk628_i2c_write(csi->rk628, GRF_CSC_CTRL_CON,
						SW_Y2R_EN(1) | SW_YUV2VYU_SWP(0));
		} else {
			if (csi->dsi.vid_mode == VIDEO_MODE)
				rk628_i2c_write(csi->rk628, GRF_CSC_CTRL_CON,
						SW_Y2R_EN(0) | SW_YUV2VYU_SWP(1));
			else
				rk628_i2c_write(csi->rk628, GRF_CSC_CTRL_CON,
						SW_Y2R_EN(0) | SW_YUV2VYU_SWP(0));
		}
	} else {
		u8 color_range;

		color_range = rk628_hdmirx_get_range(csi->rk628);
		rk628_i2c_write(csi->rk628, GRF_CSC_CTRL_CON, SW_YUV2VYU_SWP(0));
		if (csi->user_color_range == COLOR_RANGE_AUTO)
			rk628_post_process_csc_en(csi->rk628,
					color_range == HDMIRX_LIMIT_RANGE ? false : true, true);
		else if (csi->user_color_range == COLOR_RANGE_LIMIT)
			rk628_post_process_csc_en(csi->rk628, false, true);
		else
			rk628_post_process_csc_en(csi->rk628, true, true);
	}

	/* if avi packet is not stable, reset ctrl*/
	if (!avi_rdy) {
		csi->nosignal = true;
		schedule_delayed_work(&csi->delayed_work_enable_hotplug, HZ / 20);
	}
}

static void rk628_dsi_enable(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_post_process_setup(sd);

	if (csi->txphy_pwron) {
		v4l2_dbg(1, debug, sd,
			"%s: txphy already power on, power off\n", __func__);
		mipi_dphy_power_off(csi);
		csi->txphy_pwron = false;
	}

	csi->dsi.rk628 = csi->rk628;
	csi->dsi.timings = csi->timings;
	csi->dsi.lane_mbps = csi->lane_mbps;
	rk628_mipi_dsi_power_on(&csi->dsi);
	rk628_mipi_txdata_reset(sd);
	csi->txphy_pwron = true;
	v4l2_dbg(2, debug, sd, "%s: txphy power on!\n", __func__);
	usleep_range(1000, 1500);
	rk628_dsi_set_scs(csi);
}

static void rk628_dsi_disable(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_dsi_disable_stream(&csi->dsi);
	csi->txphy_pwron = false;
}

static void enable_dsitx(struct v4l2_subdev *sd)
{
	rk628_dsi_disable(sd);
	rk628_dsi_enable(sd);
}

static void rk628_disable_dsitx(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_hdmirx_vid_enable(sd, false);
	rk628_i2c_write(csi->rk628, GRF_SCALER_CON0, SCL_EN(0));
	rk628_dsi_disable(sd);
}

static void rk628_csi_disable_stream(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_i2c_update_bits(csi->rk628, CSITX_CSITX_EN,
				DPHY_EN_MASK | CSITX_EN_MASK,
				DPHY_EN(0) | CSITX_EN(0));
	rk628_i2c_update_bits(csi->rk628, CSITX_SYS_CTRL3_IMD, CONT_MODE_CLK_CLR_MASK,
			csi->continues_clk ? CONT_MODE_CLK_CLR(1) : CONT_MODE_CLK_CLR(0));
	rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);

	if (csi->rk628->version >= RK628F_VERSION) {
		rk628_i2c_update_bits(csi->rk628, CSITX1_CSITX_EN,
					DPHY_EN_MASK | CSITX_EN_MASK,
					DPHY_EN(0) | CSITX_EN(0));
		rk628_i2c_update_bits(csi->rk628, CSITX1_SYS_CTRL3_IMD, CONT_MODE_CLK_CLR_MASK,
				csi->continues_clk ? CONT_MODE_CLK_CLR(1) : CONT_MODE_CLK_CLR(0));
		rk628_i2c_write(csi->rk628, CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);
	}
	rk628_csi_soft_reset(sd);
	mipi_dphy_power_off(csi);
	csi->txphy_pwron = false;
	csi->is_streaming = false;
}

static void enable_stream(struct v4l2_subdev *sd, bool en)
{
	struct rk628_csi *csi = to_csi(sd);

	v4l2_dbg(1, debug, sd, "%s: %sable\n", __func__, en ? "en" : "dis");
	if (en) {
		if (!tx_5v_power_present(sd) || csi->nosignal) {
			v4l2_err(sd, "%s: hdmi no signal or unplug!\n", __func__);
			return;
		}

		if (rk628_hdmirx_scdc_ced_err(csi->rk628) ||
		    !rk628_hdmirx_is_locked(csi->rk628)) {
			rk628_hdmirx_plugout(sd);
			schedule_delayed_work(&csi->delayed_work_enable_hotplug,
					      msecs_to_jiffies(800));
			return;
		}

		if (csi->plat_data->tx_mode == DSI_MODE)
			enable_dsitx(sd);
		else
			enable_csitx(sd);

		rk628_hdmirx_vid_enable(sd, true);
		if (csi->plat_data->tx_mode == CSI_MODE &&
		    csi->rk628->version >= RK628F_VERSION) {
			msleep(20);
			rk628_mipi_txdata_reset(sd);
			rk628_csi_enable_csi_interrupts(sd, true);
		}

		rk628_i2c_update_bits(csi->rk628, HDMI_RX_PDEC_CTRL,
				      GCPFORCE_CLRAVMUTE_MASK, GCPFORCE_CLRAVMUTE(1));
		rk628_i2c_update_bits(csi->rk628, HDMI_RX_PDEC_CTRL,
				      GCPFORCE_CLRAVMUTE_MASK, GCPFORCE_CLRAVMUTE(0));
	} else {
		if (csi->plat_data->tx_mode == CSI_MODE) {
			if (csi->rk628->version >= RK628F_VERSION) {
				rk628_csi_enable_csi_interrupts(sd, false);
				msleep(20);
			}
			rk628_hdmirx_vid_enable(sd, false);
			rk628_csi_disable_stream(sd);
		} else {
			rk628_disable_dsitx(sd);
		}
	}
}

static void rk628_post_process_setup(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);
	struct v4l2_bt_timings *bt = &csi->src_timings.bt;
	struct v4l2_bt_timings *dst_bt = &csi->timings.bt;
	struct videomode src, dst;
	u64 dst_pclk;

	src.hactive = bt->width;
	src.hfront_porch = bt->hfrontporch;
	src.hsync_len = bt->hsync;
	src.hback_porch = bt->hbackporch;
	src.vactive = bt->height;
	src.vfront_porch = bt->vfrontporch;
	src.vsync_len = bt->vsync;
	src.vback_porch = bt->vbackporch;
	src.pixelclock = bt->pixelclock;
	src.flags = 0;
	if (bt->interlaced == V4L2_DV_INTERLACED)
		src.flags |= DISPLAY_FLAGS_INTERLACED;
	if (!src.pixelclock) {
		enable_stream(sd, false);
		csi->nosignal = true;
		schedule_delayed_work(&csi->delayed_work_enable_hotplug, HZ / 20);
		return;
	}

	dst.hactive = dst_bt->width;
	dst.hfront_porch = dst_bt->hfrontporch;
	dst.hsync_len = dst_bt->hsync;
	dst.hback_porch = dst_bt->hbackporch;
	dst.vactive = dst_bt->height;
	dst.vfront_porch = dst_bt->vfrontporch;
	dst.vsync_len = dst_bt->vsync;
	dst.vback_porch = dst_bt->vbackporch;
	dst.pixelclock = dst_bt->pixelclock;

	rk628_post_process_en(csi->rk628, &src, &dst, &dst_pclk);
	dst_bt->pixelclock = dst_pclk;
}

static void rk628_csi_set_csi(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);
	u8 video_fmt;
	u8 lanes = csi->csi_lanes_in_use;
	u8 lane_num;
	u32 wc_usrdef, val;
	int avi_rdy;

	lane_num = lanes - 1;
	csi->rk628->dphy_lane_en = (1 << (lanes + 1)) - 1;
	wc_usrdef = csi->timings.bt.width * 2;
	if (csi->rk628->dual_mipi)
		wc_usrdef = csi->timings.bt.width;
	v4l2_info(sd, "%s mipi mode, word count user define: %d\n",
			csi->rk628->dual_mipi ? "dual" : "single", wc_usrdef);
	rk628_csi_disable_stream(sd);
	usleep_range(5000, 5500);
	rk628_csi0_cru_reset(sd);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_csi1_cru_reset(sd);
	rk628_post_process_setup(sd);

	if (csi->txphy_pwron) {
		v4l2_dbg(1, debug, sd,
			"%s: txphy already power on, power off\n", __func__);
		mipi_dphy_power_off(csi);
		csi->txphy_pwron = false;
	}

	mipi_dphy_power_on(csi);
	csi->txphy_pwron = true;
	v4l2_dbg(2, debug, sd, "%s: txphy power on!\n", __func__);
	usleep_range(1000, 1500);

	if (csi->rk628->version <= RK628D_VERSION) {
		rk628_i2c_update_bits(csi->rk628, CSITX_CSITX_EN,
			VOP_UV_SWAP_MASK |
			VOP_YUV422_EN_MASK |
			VOP_P2_EN_MASK |
			LANE_NUM_MASK |
			DPHY_EN_MASK |
			CSITX_EN_MASK,
			VOP_UV_SWAP(1) |
			VOP_YUV422_EN(1) |
			VOP_P2_EN(1) |
			LANE_NUM(lane_num) |
			DPHY_EN(0) |
			CSITX_EN(0));
		rk628_i2c_update_bits(csi->rk628, CSITX_SYS_CTRL1,
			BYPASS_SELECT_MASK,
			BYPASS_SELECT(1));
	} else {
		rk628_i2c_update_bits(csi->rk628, CSITX_CSITX_EN,
			VOP_UV_SWAP_MASK |
			VOP_YUV422_EN_MASK |
			VOP_YUV422_MODE_MASK |
			VOP_P2_EN_MASK |
			LANE_NUM_MASK |
			DPHY_EN_MASK |
			CSITX_EN_MASK,
			VOP_UV_SWAP(0) |
			VOP_YUV422_EN(1) |
			VOP_YUV422_MODE(2) |
			VOP_P2_EN(1) |
			LANE_NUM(lane_num) |
			DPHY_EN(0) |
			CSITX_EN(0));
		rk628_i2c_update_bits(csi->rk628, CSITX_SYS_CTRL1,
			BYPASS_SELECT_MASK,
			BYPASS_SELECT(0));
	}

	rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);
	rk628_i2c_write(csi->rk628, CSITX_SYS_CTRL2, VOP_WHOLE_FRM_EN | VSYNC_ENABLE);
	if (csi->continues_clk)
		rk628_i2c_update_bits(csi->rk628, CSITX_SYS_CTRL3_IMD,
			CONT_MODE_CLK_CLR_MASK |
			CONT_MODE_CLK_SET_MASK |
			NON_CONTINUOUS_MODE_MASK,
			CONT_MODE_CLK_CLR(0) |
			CONT_MODE_CLK_SET(1) |
			NON_CONTINUOUS_MODE(0));
	else
		rk628_i2c_update_bits(csi->rk628, CSITX_SYS_CTRL3_IMD,
			CONT_MODE_CLK_CLR_MASK |
			CONT_MODE_CLK_SET_MASK |
			NON_CONTINUOUS_MODE_MASK,
			CONT_MODE_CLK_CLR(0) |
			CONT_MODE_CLK_SET(0) |
			NON_CONTINUOUS_MODE(1));

	rk628_i2c_write(csi->rk628, CSITX_VOP_PATH_CTRL,
			VOP_WC_USERDEFINE(wc_usrdef) |
			VOP_DT_USERDEFINE(YUV422_8BIT) |
			VOP_PIXEL_FORMAT(0) |
			VOP_WC_USERDEFINE_EN(1) |
			VOP_DT_USERDEFINE_EN(1) |
			VOP_PATH_EN(1));
	rk628_i2c_update_bits(csi->rk628, CSITX_DPHY_CTRL,
				CSI_DPHY_EN_MASK,
				CSI_DPHY_EN(csi->rk628->dphy_lane_en));
	rk628_i2c_update_bits(csi->rk628, CSITX_VOP_FILTER_CTRL,
				VOP_FILTER_EN_MASK | VOP_FILTER_MASK,
				VOP_FILTER_EN(1) | VOP_FILTER(3));
	rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);
	v4l2_dbg(1, debug, sd, "%s csi config done\n", __func__);

	if (csi->rk628->version >= RK628F_VERSION) {
		rk628_i2c_update_bits(csi->rk628, CSITX1_CSITX_EN,
				VOP_UV_SWAP_MASK |
				VOP_YUV422_EN_MASK |
				VOP_YUV422_MODE_MASK |
				VOP_P2_EN_MASK |
				LANE_NUM_MASK |
				DPHY_EN_MASK |
				CSITX_EN_MASK,
				VOP_UV_SWAP(0) |
				VOP_YUV422_EN(1) |
				VOP_YUV422_MODE(2) |
				VOP_P2_EN(1) |
				LANE_NUM(lane_num) |
				DPHY_EN(0) |
				CSITX_EN(0));
		rk628_i2c_update_bits(csi->rk628, CSITX1_SYS_CTRL1,
				BYPASS_SELECT_MASK,
				BYPASS_SELECT(0));
		rk628_i2c_write(csi->rk628, CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);
		rk628_i2c_write(csi->rk628, CSITX1_SYS_CTRL2, VOP_WHOLE_FRM_EN | VSYNC_ENABLE);
		if (csi->continues_clk)
			rk628_i2c_update_bits(csi->rk628, CSITX1_SYS_CTRL3_IMD,
				CONT_MODE_CLK_CLR_MASK |
				CONT_MODE_CLK_SET_MASK |
				NON_CONTINUOUS_MODE_MASK,
				CONT_MODE_CLK_CLR(0) |
				CONT_MODE_CLK_SET(1) |
				NON_CONTINUOUS_MODE(0));
		else
			rk628_i2c_update_bits(csi->rk628, CSITX1_SYS_CTRL3_IMD,
				CONT_MODE_CLK_CLR_MASK |
				CONT_MODE_CLK_SET_MASK |
				NON_CONTINUOUS_MODE_MASK,
				CONT_MODE_CLK_CLR(0) |
				CONT_MODE_CLK_SET(0) |
				NON_CONTINUOUS_MODE(1));

		rk628_i2c_write(csi->rk628, CSITX1_VOP_PATH_CTRL,
				VOP_WC_USERDEFINE(wc_usrdef) |
				VOP_DT_USERDEFINE(YUV422_8BIT) |
				VOP_PIXEL_FORMAT(0) |
				VOP_WC_USERDEFINE_EN(1) |
				VOP_DT_USERDEFINE_EN(1) |
				VOP_PATH_EN(1));
		rk628_i2c_update_bits(csi->rk628, CSITX1_DPHY_CTRL,
					CSI_DPHY_EN_MASK,
					CSI_DPHY_EN(csi->rk628->dphy_lane_en));
		rk628_i2c_update_bits(csi->rk628, CSITX1_VOP_FILTER_CTRL,
				VOP_FILTER_EN_MASK | VOP_FILTER_MASK,
				VOP_FILTER_EN(1) | VOP_FILTER(3));
		rk628_i2c_write(csi->rk628, CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);
		v4l2_dbg(1, debug, sd, "%s csi1 config done\n", __func__);
	}

	avi_rdy = rk628_is_avi_ready(csi->rk628, csi->avi_rcv_rdy);

	rk628_i2c_read(csi->rk628, HDMI_RX_PDEC_AVI_PB, &val);
	video_fmt = (val & VIDEO_FORMAT_MASK) >> 5;
	v4l2_dbg(1, debug, &csi->sd, "%s PDEC_AVI_PB:%#x, video format:%d\n",
			__func__, val, video_fmt);
	if (csi->rk628->version == RK628D_VERSION) {
		if (video_fmt) {
			/* yuv data: cfg SW_YUV2VYU_SWP */
			rk628_i2c_write(csi->rk628, GRF_CSC_CTRL_CON,
				SW_YUV2VYU_SWP(1) |
				SW_R2Y_EN(0));
		} else {
			/* rgb data: cfg SW_R2Y_EN */
			rk628_i2c_write(csi->rk628, GRF_CSC_CTRL_CON,
				SW_YUV2VYU_SWP(0) |
				SW_R2Y_EN(1) | SW_R2Y_CSC_MODE(2));
		}
	} else {
		u8 color_range;

		color_range = rk628_hdmirx_get_range(csi->rk628);
		rk628_i2c_write(csi->rk628, GRF_CSC_CTRL_CON, SW_YUV2VYU_SWP(1));
		if (csi->user_color_range == COLOR_RANGE_AUTO)
			rk628_post_process_csc_en(csi->rk628,
					color_range == HDMIRX_LIMIT_RANGE ? false : true, true);
		else if (csi->user_color_range == COLOR_RANGE_LIMIT)
			rk628_post_process_csc_en(csi->rk628, false, true);
		else
			rk628_post_process_csc_en(csi->rk628, true, true);
	}
	/* if avi packet is not stable, reset ctrl*/
	if (!avi_rdy) {
		csi->nosignal = true;
		schedule_delayed_work(&csi->delayed_work_enable_hotplug, HZ / 20);
	}
}

static int rk628_hdmirx_inno_phy_power_on(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);
	int ret, f;

	/* Bit31 is used to distinguish HDMI cable mode and direct connection
	 * mode in the rk628_combrxphy driver.
	 * Bit31: 0 -direct connection mode;
	 *        1 -cable mode;
	 * The cable mode is to know the input clock frequency through cdr_mode
	 * in the rk628_combrxphy driver, and the cable mode supports up to
	 * 297M, so 297M is passed uniformly here.
	 */
	f = 297000 | BIT(31);

	if (csi->rxphy_pwron) {
		v4l2_dbg(1, debug, sd, "rxphy already power on, power off!\n");
		ret = rk628_rxphy_power_off(csi->rk628);
		if (ret)
			v4l2_err(sd, "hdmi rxphy power off failed!\n");
		else
			csi->rxphy_pwron = false;
		usleep_range(100, 110);
	}

	if (csi->rxphy_pwron == false) {
		rk628_hdmirx_ctrl_enable(sd, 0);
		ret = rk628_rxphy_power_on(csi->rk628, f);
		if (ret) {
			csi->rxphy_pwron = false;
			v4l2_err(sd, "hdmi rxphy power on failed\n");
		} else {
			csi->rxphy_pwron = true;
		}
		rk628_hdmirx_ctrl_enable(sd, 1);
		msleep(100);
	}

	return ret;
}

static int rk628_hdmirx_inno_phy_power_off(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	if (csi->rk628->version >= RK628F_VERSION)
		return 0;

	if (csi->rxphy_pwron) {
		v4l2_dbg(1, debug, sd, "rxphy power off!\n");
		rk628_rxphy_power_off(csi->rk628);
		csi->rxphy_pwron = false;
	}
	usleep_range(100, 100);

	return 0;
}

static void rk628_hdmirx_vid_enable(struct v4l2_subdev *sd, bool en)
{
	struct rk628_csi *csi = to_csi(sd);

	v4l2_dbg(1, debug, sd, "%s: %sable\n", __func__, en ? "en" : "dis");
	if (en) {
		if (!csi->i2s_enable_default)
			rk628_hdmirx_audio_i2s_ctrl(csi->audio_info, true);
		rk628_i2c_update_bits(csi->rk628, HDMI_RX_DMI_DISABLE_IF,
				      VID_ENABLE_MASK, VID_ENABLE(1));
	} else {
		if (!csi->i2s_enable_default)
			rk628_hdmirx_audio_i2s_ctrl(csi->audio_info, false);
		rk628_i2c_update_bits(csi->rk628, HDMI_RX_DMI_DISABLE_IF,
				      VID_ENABLE_MASK, VID_ENABLE(0));
	}
}

static bool rk628_rcv_supported_res(struct v4l2_subdev *sd, u32 width,
		u32 height)
{
	u32 i;
	struct rk628_csi *csi = to_csi(sd);

	if (csi->rk628->version >= RK628F_VERSION)
		return true;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		if ((supported_modes[i].width == width) &&
		    (supported_modes[i].height == height)) {
			break;
		}
	}
	if (i == ARRAY_SIZE(supported_modes)) {
		v4l2_err(sd, "%s do not support res wxh: %dx%d\n", __func__,
				width, height);
		return false;
	} else {
		return true;
	}
}

static int rk628_hdmirx_dvi_mode_reset(struct v4l2_subdev *sd)
{
	u32 val, avi_pb;
	struct rk628_csi *csi = to_csi(sd);

	rk628_i2c_read(csi->rk628, HDMI_RX_PDEC_STS, &val);
	if (val & DVI_DET) {
		rk628_i2c_read(csi->rk628, HDMI_RX_PDEC_AVI_PB, &avi_pb);
		if (avi_pb && !csi->dvi_mode) {
			csi->dvi_mode = true;
			v4l2_info(sd, "%s HDMI to DVI hdmirx ctrl reset!\n", __func__);
			return -1;
		}
		csi->dvi_mode = true;
	} else {
		csi->dvi_mode = false;
	}

	return 0;
}

static int rk628_hdmirx_phy_setup(struct v4l2_subdev *sd)
{
	u32 i, cnt, val;
	u32 width, height, frame_width, frame_height, status;
	struct rk628_csi *csi = to_csi(sd);
	int ret = 0;

	for (i = 0; i < RXPHY_CFG_MAX_TIMES; i++) {
		if (csi->rk628->version < RK628F_VERSION)
			ret = rk628_hdmirx_inno_phy_power_on(sd);
		else
			rk628_hdmirx_verisyno_phy_power_on(csi->rk628);
		if (ret < 0) {
			msleep(50);
			continue;
		}
		cnt = 0;

		do {
			msleep(20);
			cnt++;
			rk628_i2c_read(csi->rk628, HDMI_RX_MD_HACT_PX, &val);
			width = val & 0xffff;
			rk628_i2c_read(csi->rk628, HDMI_RX_MD_VAL, &val);
			height = val & 0xffff;
			rk628_i2c_read(csi->rk628, HDMI_RX_MD_HT1, &val);
			frame_width = (val >> 16) & 0xffff;
			rk628_i2c_read(csi->rk628, HDMI_RX_MD_VTL, &val);
			frame_height = val & 0xffff;
			rk628_i2c_read(csi->rk628, HDMI_RX_SCDC_REGS1, &val);
			status = val;
			v4l2_dbg(1, debug, sd, "%s read wxh:%dx%d, total:%dx%d, SCDC_REGS1:%#x, cnt:%d\n",
				 __func__, width, height, frame_width, frame_height, status, cnt);

			rk628_i2c_read(csi->rk628, HDMI_RX_PDEC_STS, &val);
			if (csi->rk628->version < RK628F_VERSION && (val & DVI_DET))
				dev_info(csi->dev, "DVI mode detected\n");

			if ((status & 0xfff) >= 0xf00) {
				msleep(50);
				if (rk628_hdmirx_dvi_mode_reset(sd))
					return LOCK_RESET;
			}

			if (!tx_5v_power_present(sd)) {
				v4l2_info(sd, "HDMI pull out, return!\n");
				return LOCK_FAIL;
			}

			if (cnt >= 15)
				break;
		} while (((status & 0xfff) < 0xf00) ||
				(!rk628_rcv_supported_res(sd, width, height)));

		if (((status & 0xfff) < 0xf00) ||
				(!rk628_rcv_supported_res(sd, width, height))) {
			v4l2_err(sd, "%s hdmi rxphy lock failed, retry:%d\n",
					__func__, i);
			continue;
		} else {
			if (csi->rk628->version >= RK628F_VERSION)
				rk628_hdmirx_phy_prepclk_cfg(csi->rk628);
			break;
		}
	}

	if (i == RXPHY_CFG_MAX_TIMES)
		return LOCK_FAIL;

	return LOCK_OK;
}

static void rk628_csi_initial(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);
	struct v4l2_subdev_edid def_edid;
	u32 mask = SW_OUTPUT_MODE_MASK;
	u32 val = SW_OUTPUT_MODE(OUTPUT_MODE_CSI);

	/* select int io function */
	rk628_i2c_write(csi->rk628, GRF_GPIO3AB_SEL_CON, 0x30002000);
	rk628_i2c_write(csi->rk628, GRF_GPIO1AB_SEL_CON, HIWORD_UPDATE(0xf, 11, 8));
	/* I2S_SCKM0 */
	rk628_i2c_write(csi->rk628, GRF_GPIO0AB_SEL_CON, HIWORD_UPDATE(0x1, 2, 2));
	/* I2SLR_M0 */
	rk628_i2c_write(csi->rk628, GRF_GPIO0AB_SEL_CON, HIWORD_UPDATE(0x1, 3, 3));
	/* I2SM0D0 */
	rk628_i2c_write(csi->rk628, GRF_GPIO0AB_SEL_CON, HIWORD_UPDATE(0x1, 5, 4));
	/* hdmirx int en */
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_write(csi->rk628, GRF_INTR0_EN, 0x02000200);
	else
		rk628_i2c_write(csi->rk628, GRF_INTR0_EN, 0x01000100);

	udelay(10);
	rk628_control_assert(csi->rk628, RGU_HDMIRX);
	rk628_control_assert(csi->rk628, RGU_HDMIRX_PON);
	rk628_control_assert(csi->rk628, RGU_CSI);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_control_assert(csi->rk628, RGU_CSI1);
	udelay(10);
	rk628_control_deassert(csi->rk628, RGU_HDMIRX);
	rk628_control_deassert(csi->rk628, RGU_HDMIRX_PON);
	rk628_control_deassert(csi->rk628, RGU_CSI);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_control_deassert(csi->rk628, RGU_CSI1);
	udelay(10);

	if (csi->rk628->version >= RK628F_VERSION) {
		mask = SW_OUTPUT_COMBTX_MODE_MASK;
		val = SW_OUTPUT_COMBTX_MODE(OUTPUT_MODE_CSI - 1);
	}
	rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON0,
			SW_INPUT_MODE_MASK |
			mask |
			SW_EFUSE_HDCP_EN_MASK |
			SW_HSYNC_POL_MASK |
			SW_VSYNC_POL_MASK |
			SW_I2S_DATA_OEN_MASK,
			SW_INPUT_MODE(INPUT_MODE_HDMI) |
			val |
			SW_EFUSE_HDCP_EN(0) |
			SW_HSYNC_POL(1) |
			SW_VSYNC_POL(1) |
			SW_I2S_DATA_OEN(0));

	def_edid.pad = 0;
	def_edid.start_block = 0;
	def_edid.blocks = 2;
	if (csi->rk628->version >= RK628F_VERSION && csi->dual_mipi_use) {
		def_edid.edid = rk628f_edid_init_data;
		csi->edid_version = 2;
	} else {
		def_edid.edid = edid_init_data;
		csi->edid_version = 1;
	}
	rk628_csi_s_edid(sd, &def_edid);
}

static void rk628_csi_initial_setup(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_csi_initial(sd);
	if (csi->rk628->version >= RK628F_VERSION) {
		csi->rk628->mipi_timing[0] = rk628f_csi0_mipi;
		csi->rk628->mipi_timing[1] = rk628f_csi1_mipi;
	} else {
		csi->rk628->mipi_timing[0] = rk628d_csi_mipi;
	}

	if (csi->plat_data->tx_mode == DSI_MODE) {
		csi->rk628->mipi_timing[0] = rk628f_dsi0_mipi;
		csi->rk628->mipi_timing[1] = rk628f_dsi0_mipi;
	}

	csi->rk628->dphy_lane_en = 0x1f;
	csi->txphy_pwron = true;
	if (tx_5v_power_present(sd))
		schedule_delayed_work(&csi->delayed_work_enable_hotplug, msecs_to_jiffies(4000));
}

static int rk628_csi_format_change(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);
	struct v4l2_dv_timings timings;
	const struct v4l2_event rk628_csi_ev_fmt = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION,
	};
	int ret;

	ret = rk628_csi_get_detected_timings(sd, &timings);
	if (ret) {
		v4l2_dbg(1, debug, sd, "%s: get timing fail\n", __func__);
		return LOCK_FAIL;
	}
	if (!v4l2_match_dv_timings(&csi->timings, &timings, 0, false)) {
		/* automatically set timing rather than set by userspace */
		rk628_csi_s_dv_timings(sd, &timings);
		v4l2_print_dv_timings(sd->name,
				"rk628_csi_format_change: New format: ",
				&timings, false);
	}

	if (sd->devnode)
		v4l2_subdev_notify_event(sd, &rk628_csi_ev_fmt);

	return LOCK_OK;
}

#if IS_REACHABLE(CONFIG_VIDEO_ROCKCHIP_CIF)
static void rk628_get_remote_dev(struct media_entity *sensor_entity,
				  struct video_device **video)
{
	struct media_graph graph;
	struct media_device *mdev = sensor_entity->graph_obj.mdev;
	struct media_entity *entity;
	int ret = 0;

	mutex_lock(&mdev->graph_mutex);
	ret = media_graph_walk_init(&graph, mdev);
	if (ret) {
		mutex_unlock(&mdev->graph_mutex);
		return;
	}

	media_graph_walk_start(&graph, sensor_entity);
	while ((entity = media_graph_walk_next(&graph))) {
		if (strcmp(entity->name, "stream_cif_mipi_id0") == 0)
			break;
	}
	mutex_unlock(&mdev->graph_mutex);
	media_graph_walk_cleanup(&graph);

	if (entity)
		*video = media_entity_to_video_device(entity);
	else
		*video = NULL;
}

static void rk628_csi_reset_rkcif(struct v4l2_subdev *sd)
{
	struct video_device *vdev = NULL;

	rk628_get_remote_dev(&sd->entity, &vdev);
	if (vdev != NULL) {
		rkcif_external_soft_reset(vdev);
		v4l2_dbg(1, debug, sd, "%s, do reset rkcif\n", __func__);
	} else {
		v4l2_dbg(1, debug, sd, "%s, get remote rkcif failed\n", __func__);
	}
}
#endif

static void rk628_csi_enable_csi_interrupts(struct v4l2_subdev *sd, bool en)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_csi_clear_csi_interrupts(sd);
	//disabled csi state ints
	rk628_i2c_write(csi->rk628, CSITX_INTR_EN_IMD, 0x0fff0000);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_write(csi->rk628, CSITX1_INTR_EN_IMD, 0x0fff0000);

	//enable csi error ints
	if (en) {
		if (csi->rk628->version >= RK628F_VERSION) {
			rk628_i2c_update_bits(csi->rk628,
					GRF_INTR0_EN, CSI_INT_EN_MASK | CSI_INT_WRITE_EN_MASK,
					CSI_INT_EN(3) | CSI_INT_WRITE_EN(3));
			rk628_i2c_write(csi->rk628, CSITX_ERR_INTR_EN_IMD, 0x0fff0fff);
			rk628_i2c_write(csi->rk628, CSITX1_ERR_INTR_EN_IMD, 0x0fff0fff);
		} else {
			rk628_i2c_update_bits(csi->rk628,
					GRF_INTR0_EN, CSI_INT_EN_MASK | CSI_INT_WRITE_EN_MASK,
					CSI_INT_EN(1) | CSI_INT_WRITE_EN(1));
			rk628_i2c_write(csi->rk628, CSITX_ERR_INTR_EN_IMD, 0x0fff0fff);
		}
		csi->csi_ints_en = true;
	} else {
		if (csi->rk628->version >= RK628F_VERSION) {
			rk628_i2c_update_bits(csi->rk628,
					GRF_INTR0_EN, CSI_INT_EN_MASK | CSI_INT_WRITE_EN_MASK,
					CSI_INT_EN(0) | CSI_INT_WRITE_EN(3));
			rk628_i2c_write(csi->rk628, CSITX_ERR_INTR_EN_IMD, 0x0fff0000);
			rk628_i2c_write(csi->rk628, CSITX1_ERR_INTR_EN_IMD, 0x0fff0000);
		} else {
			rk628_i2c_update_bits(csi->rk628,
					GRF_INTR0_EN, CSI_INT_EN_MASK | CSI_INT_WRITE_EN_MASK,
					CSI_INT_EN(0) | CSI_INT_WRITE_EN(1));
			rk628_i2c_write(csi->rk628, CSITX_ERR_INTR_EN_IMD, 0x0fff0000);
		}
		csi->csi_ints_en = false;
	}
}

static void rk628_csi_enable_interrupts(struct v4l2_subdev *sd, bool en)
{
	u32 pdec_ien, md_ien;
	u32 pdec_mask = 0, md_mask = 0;
	struct rk628_csi *csi = to_csi(sd);

	pdec_mask = AVI_RCV_ENSET | AVI_CKS_CHG_ICLR;
	md_mask = VACT_LIN_ENSET | HACT_PIX_ENSET | HS_CLK_ENSET |
		  DE_ACTIVITY_ENSET | VS_ACT_ENSET | HS_ACT_ENSET | VS_CLK_ENSET;
	v4l2_dbg(1, debug, sd, "%s: %sable\n", __func__, en ? "en" : "dis");
	/* clr irq */
	rk628_i2c_write(csi->rk628, HDMI_RX_MD_ICLR, md_mask);
	rk628_i2c_write(csi->rk628, HDMI_RX_PDEC_ICLR, pdec_mask);
	if (en) {
		rk628_i2c_write(csi->rk628, HDMI_RX_MD_IEN_SET, md_mask);
		rk628_i2c_write(csi->rk628, HDMI_RX_PDEC_IEN_SET, pdec_mask);
		csi->vid_ints_en = true;
	} else {
		rk628_i2c_write(csi->rk628, HDMI_RX_MD_IEN_CLR, md_mask);
		rk628_i2c_write(csi->rk628, HDMI_RX_PDEC_IEN_CLR, pdec_mask);
		rk628_i2c_write(csi->rk628, HDMI_RX_AUD_FIFO_IEN_CLR, 0x1f);
		csi->vid_ints_en = false;
	}
	usleep_range(5000, 5000);
	rk628_i2c_read(csi->rk628, HDMI_RX_MD_IEN, &md_ien);
	rk628_i2c_read(csi->rk628, HDMI_RX_PDEC_IEN, &pdec_ien);
	v4l2_dbg(1, debug, sd, "%s MD_IEN:%#x, PDEC_IEN:%#x\n", __func__, md_ien, pdec_ien);
}

static void rk628_csi_clear_csi_interrupts(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	//clr int status
	rk628_i2c_write(csi->rk628, CSITX_ERR_INTR_CLR_IMD, 0xffffffff);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_write(csi->rk628, CSITX1_ERR_INTR_CLR_IMD, 0xffffffff);

	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_write(csi->rk628, GRF_INTR0_CLR_EN, 0xc000c0);
	else
		rk628_i2c_write(csi->rk628, GRF_INTR0_CLR_EN, 0x400040);
}

static void rk628_csi_clear_hdmirx_interrupts(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);

	v4l2_dbg(2, debug, sd, "%s: clear hdmirx ints\n", __func__);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_write(csi->rk628, GRF_INTR0_CLR_EN, 0x02000200);
	else
		rk628_i2c_write(csi->rk628, GRF_INTR0_CLR_EN, 0x01000100);
}

static void rk628_csi_error_process(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);
	u32 val, val_csi1 = 0;
	int i;

	if (csi->is_streaming) {
		v4l2_info(sd,
			"%s: csitx is streaming, reset csitx and restart cstitx!\n", __func__);
		rk628_hdmirx_vid_enable(sd, false);
		rk628_csi_enable_csi_interrupts(sd, false);
		rk628_i2c_update_bits(csi->rk628, CSITX_CSITX_EN, CSITX_EN_MASK, CSITX_EN(0));
		if (csi->rk628->version >= RK628F_VERSION)
			rk628_i2c_update_bits(csi->rk628, CSITX1_CSITX_EN,
						CSITX_EN_MASK, CSITX_EN(0));
		rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);
		if (csi->rk628->version >= RK628F_VERSION)
			rk628_i2c_write(csi->rk628, CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);

		usleep_range(5000, 5500);
		rk628_csi_soft_reset(sd);
		usleep_range(5000, 5500);

#if IS_REACHABLE(CONFIG_VIDEO_ROCKCHIP_CIF)
		rk628_csi_reset_rkcif(sd);
#endif

		rk628_i2c_update_bits(csi->rk628, CSITX_CSITX_EN, CSITX_EN_MASK, CSITX_EN(1));
		rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);
		if (csi->rk628->version >= RK628F_VERSION) {
			rk628_i2c_update_bits(csi->rk628, CSITX1_CSITX_EN,
						CSITX_EN_MASK, CSITX_EN(1));
			rk628_i2c_write(csi->rk628, CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);
		}

		for (i = 0; i < 3; i++) {
			rk628_i2c_read(csi->rk628, CSITX_CSITX_EN, &val);
			if (csi->rk628->version >= RK628F_VERSION)
				rk628_i2c_read(csi->rk628, CSITX1_CSITX_EN, &val_csi1);
			v4l2_dbg(1, debug, sd, "%s, csi0_status: 0x%x, csi1_status: 0x%x, i=%d\n",
							__func__, val, val_csi1, i);

			if (csi->rk628->version >= RK628F_VERSION ?
				((val & BIT(0)) && (val_csi1 & BIT(0))) : (val & BIT(0)))
				break;

			rk628_i2c_update_bits(csi->rk628, CSITX_CSITX_EN,
							CSITX_EN_MASK, CSITX_EN(1));
			rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);
			if (csi->rk628->version >= RK628F_VERSION) {
				rk628_i2c_update_bits(csi->rk628, CSITX1_CSITX_EN,
								CSITX_EN_MASK, CSITX_EN(1));
				rk628_i2c_write(csi->rk628,
						CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);
			}
		}
		rk628_hdmirx_vid_enable(sd, true);
		msleep(20);
		rk628_mipi_txdata_reset(sd);
		rk628_csi_enable_csi_interrupts(sd, true);
		v4l2_dbg(1, debug, sd, "%s, do reset successful\n", __func__);
	} else {
		v4l2_info(sd,
			"%s: csitx is not streaming\n", __func__);
	}
}

static int rk628_is_general_isr(struct rk628_csi *csi, u32 md_ints, u32 pdec_ints)
{
	if (rk628_hdmirx_is_signal_change_ists(csi->rk628, md_ints, pdec_ints))
		return 1;
	if ((pdec_ints & AVI_RCV_ISTS) && !csi->avi_rcv_rdy)
		return 1;

	return 0;
}

static int rk628_hdmirx_general_isr(struct v4l2_subdev *sd, u32 status, bool *handled)
{
	struct rk628_csi *csi = to_csi(sd);
	u32 md_ints = 0x0, pdec_ints = 0x0, fifo_ints, hact, vact;
	bool plugin;
	void *audio_info = csi->audio_info;
	u32 int0_status;
	const struct v4l2_event evt_signal_lost = {
		.type = RK_HDMIRX_V4L2_EVENT_SIGNAL_LOST,
	};

	if (handled == NULL) {
		v4l2_err(sd, "handled NULL, err return!\n");
		return -EINVAL;
	}

	if (csi->rk628->version < RK628F_VERSION) {
		if (rk628_audio_ctsnints_enabled(audio_info)) {
			rk628_i2c_read(csi->rk628, HDMI_RX_PDEC_ISTS, &pdec_ints);
			if (pdec_ints & (ACR_N_CHG_ICLR | ACR_CTS_CHG_ICLR)) {
				rk628_csi_isr_ctsn(audio_info, pdec_ints);
				pdec_ints &= ~(ACR_CTS_CHG_ICLR | ACR_CTS_CHG_ICLR);
				*handled = true;
			}
		}
		if (rk628_audio_fifoints_enabled(audio_info)) {
			rk628_i2c_read(csi->rk628, HDMI_RX_AUD_FIFO_ISTS, &fifo_ints);
			if (fifo_ints & 0x18) {
				rk628_csi_isr_fifoints(audio_info, fifo_ints);
				*handled = true;
			}
		}
	}

	if (!csi->vid_ints_en)
		return 0;

	rk628_i2c_read(csi->rk628, GRF_INTR0_STATUS, &int0_status);
	if (!(int0_status & (BIT(8) | BIT(9))))
		return 0;

	rk628_i2c_read(csi->rk628, HDMI_RX_MD_ISTS, &md_ints);
	rk628_i2c_read(csi->rk628, HDMI_RX_PDEC_ISTS, &pdec_ints);

	/* clear interrupts */
	rk628_i2c_write(csi->rk628, HDMI_RX_MD_ICLR, 0xffffffff);
	rk628_i2c_write(csi->rk628, HDMI_RX_PDEC_ICLR, 0xff3fffff);

	if (!rk628_is_general_isr(csi, md_ints, pdec_ints))
		return 0;

	v4l2_dbg(1, debug, sd, "%s: int0 status: 0x%x\n", __func__, int0_status);

	if (csi->rk628->version >= RK628F_VERSION &&
	    rk628_hdmirx_is_signal_change_ists(csi->rk628, md_ints, pdec_ints))
		rk628_set_bg_enable(csi->rk628, true);

	plugin = tx_5v_power_present(sd);
	if (!plugin) {
		rk628_csi_enable_interrupts(sd, false);
		return 0;
	}

	v4l2_dbg(1, debug, sd, "%s: md_ints: %#x, pdec_ints:%#x, plugin: %d\n",
		 __func__, md_ints, pdec_ints, plugin);

	if (rk628_hdmirx_is_signal_change_ists(csi->rk628, md_ints, pdec_ints)) {
		rk628_i2c_read(csi->rk628, HDMI_RX_MD_HACT_PX, &hact);
		rk628_i2c_read(csi->rk628, HDMI_RX_MD_VAL, &vact);
		v4l2_dbg(1, debug, sd, "%s: HACT:%#x, VACT:%#x\n",
			 __func__, hact, vact);

		rk628_csi_enable_interrupts(sd, false);
		if (csi->rk628->version < RK628F_VERSION)
			enable_stream(sd, false);
		csi->nosignal = true;
		v4l2_event_queue(sd->devnode, &evt_signal_lost);
		schedule_delayed_work(&csi->delayed_work_res_change, msecs_to_jiffies(100));

		v4l2_dbg(1, debug, sd, "%s: hact/vact change, md_ints: %#x\n",
			 __func__, (u32)(md_ints & (VACT_LIN_ISTS | HACT_PIX_ISTS)));
		*handled = true;
	}

	if ((pdec_ints & AVI_RCV_ISTS) && plugin && !csi->avi_rcv_rdy) {
		v4l2_dbg(1, debug, sd, "%s: AVI RCV INT!\n", __func__);
		if (csi->plat_data->tx_mode == DSI_MODE)
			enable_stream(sd, false);
		csi->avi_rcv_rdy = true;
		/* After get the AVI_RCV interrupt state, disable interrupt. */
		rk628_i2c_write(csi->rk628, HDMI_RX_PDEC_IEN_CLR, AVI_RCV_ISTS);

		*handled = true;
	}

	if (*handled != true)
		v4l2_dbg(1, debug, sd, "%s: unhandled interrupt!\n", __func__);

	return 0;
}

static int rk628_hdmirx_isr(struct v4l2_subdev *sd, u32 status, bool *handled)
{
	struct rk628_csi *csi = to_csi(sd);

	rk628_hdmirx_general_isr(sd, status, handled);
	if (csi->cec_enable && csi->cec)
		rk628_hdmirx_cec_irq(csi->rk628, csi->cec);

	rk628_csi_clear_hdmirx_interrupts(sd);

	return 0;
}

static int rk628_csi_isr(struct v4l2_subdev *sd, u32 status, bool *handled)
{
	struct rk628_csi *csi = to_csi(sd);
	u32 int0_status;
	u32 csi0_raw_ints = 0x0, csi1_raw_ints = 0x0;

	rk628_i2c_read(csi->rk628, GRF_INTR0_STATUS, &int0_status);
	if (!(int0_status & (BIT(6) | BIT(7))))
		return 0;

	v4l2_dbg(1, debug, sd, "%s: int0 status: 0x%x\n", __func__, int0_status);

	rk628_i2c_read(csi->rk628, CSITX_ERR_INTR_RAW_STATUS_IMD, &csi0_raw_ints);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_i2c_read(csi->rk628, CSITX1_ERR_INTR_RAW_STATUS_IMD, &csi1_raw_ints);

	rk628_csi_clear_csi_interrupts(sd);

	if (csi0_raw_ints || csi1_raw_ints) {
		v4l2_info(sd,
			"%s: csi interrupt: csi0_raw_ints: 0x%x, csi1_raw_ints: 0x%x!\n",
				__func__, csi0_raw_ints, csi1_raw_ints);
		rk628_csi_error_process(sd);
		*handled = true;
	}

	if (*handled != true)
		v4l2_dbg(1, debug, sd, "%s: unhandled interrupt!\n", __func__);

	return 0;
}

static int rk628_isr_process(struct v4l2_subdev *sd, u32 status, bool *handled)
{
	rk628_hdmirx_isr(sd, status, handled);
	rk628_csi_isr(sd, status, handled);

	return 0;
}

static irqreturn_t rk628_csi_irq_handler(int irq, void *dev_id)
{
	struct rk628_csi *csi = dev_id;
	bool handled = true;

	rk628_isr_process(&csi->sd, 0, &handled);

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static void rk628_csi_irq_poll_timer(struct timer_list *t)
{
	struct rk628_csi *csi = from_timer(csi, t, timer);

	schedule_work(&csi->work_i2c_poll);
	mod_timer(&csi->timer, jiffies + msecs_to_jiffies(POLL_INTERVAL_MS));
}

static void rk628_csi_work_i2c_poll(struct work_struct *work)
{
	struct rk628_csi *csi = container_of(work, struct rk628_csi,
			work_i2c_poll);
	struct v4l2_subdev *sd = &csi->sd;

	rk628_csi_format_change(sd);
}

static int rk628_csi_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
				    struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subdev_subscribe(sd, fh, sub);
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(sd, fh, sub);
	case RK_HDMIRX_V4L2_EVENT_SIGNAL_LOST:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case RK_HDMIRX_V4L2_EVENT_AUDIOINFO:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	default:
		return -EINVAL;
	}
}

static int rk628_csi_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct rk628_csi *csi = to_csi(sd);
	static u8 cnt;

	*status = 0;
	*status |= no_signal(sd) ? V4L2_IN_ST_NO_SIGNAL : 0;

	if (no_signal(sd) && tx_5v_power_present(sd)) {
		if (cnt++ >= 6) {
			cnt = 0;
			v4l2_info(sd, "no signal but 5v_det, recfg hdmirx!\n");
			schedule_delayed_work(&csi->delayed_work_enable_hotplug,
					HZ / 20);
		}
	} else {
		cnt = 0;
	}

	v4l2_dbg(1, debug, sd, "%s: status = 0x%x\n", __func__, *status);

	return 0;
}

static int rk628_csi_s_dv_timings(struct v4l2_subdev *sd,
		struct v4l2_dv_timings *timings)
{
	struct rk628_csi *csi = to_csi(sd);

	if (!timings)
		return -EINVAL;

	if (debug)
		v4l2_print_dv_timings(sd->name, "rk628_csi_s_dv_timings: ",
				timings, false);

	if (v4l2_match_dv_timings(&csi->timings, timings, 0, false)) {
		v4l2_dbg(1, debug, sd, "%s: no change\n", __func__);
		return 0;
	}

	if (!v4l2_valid_dv_timings(timings, &rk628_csi_timings_cap, NULL,
				NULL)) {
		v4l2_dbg(1, debug, sd, "%s: timings out of range\n", __func__);
		return -ERANGE;
	}

	csi->timings = *timings;
	enable_stream(sd, false);

	return 0;
}

static int rk628_csi_g_dv_timings(struct v4l2_subdev *sd,
		struct v4l2_dv_timings *timings)
{
	struct rk628_csi *csi = to_csi(sd);

	*timings = csi->timings;

	return 0;
}

static int rk628_csi_enum_dv_timings(struct v4l2_subdev *sd,
		struct v4l2_enum_dv_timings *timings)
{
	if (timings->pad != 0)
		return -EINVAL;

	return v4l2_enum_dv_timings_cap(timings, &rk628_csi_timings_cap, NULL,
			NULL);
}

static int rk628_csi_query_dv_timings(struct v4l2_subdev *sd,
		struct v4l2_dv_timings *timings)
{
	int ret;
	struct rk628_csi *csi = to_csi(sd);
	struct v4l2_dv_timings default_timing =
				V4L2_DV_BT_CEA_640X480P59_94;

	if (!tx_5v_power_present(sd) || csi->nosignal) {
		*timings = default_timing;
		v4l2_info(sd, "%s: not detect 5v, set default timing\n", __func__);
		return 0;
	}
	mutex_lock(&csi->confctl_mutex);
	ret = rk628_csi_get_detected_timings(sd, timings);
	mutex_unlock(&csi->confctl_mutex);
	if (ret)
		return ret;

	if (debug)
		v4l2_print_dv_timings(sd->name, "rk628_csi_query_dv_timings: ",
				timings, false);

	if (!v4l2_valid_dv_timings(timings, &rk628_csi_timings_cap, NULL,
				NULL)) {
		v4l2_dbg(1, debug, sd, "%s: timings out of range\n", __func__);
		return -ERANGE;
	}

	return 0;
}

static int rk628_csi_dv_timings_cap(struct v4l2_subdev *sd,
		struct v4l2_dv_timings_cap *cap)
{
	if (cap->pad != 0)
		return -EINVAL;

	*cap = rk628_csi_timings_cap;

	return 0;
}

static int rk628_csi_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
			     struct v4l2_mbus_config *cfg)
{
	struct rk628_csi *csi = to_csi(sd);

	cfg->type = V4L2_MBUS_CSI2_DPHY;
	cfg->bus.mipi_csi2.num_data_lanes = csi->csi_lanes_in_use;

	return 0;
}

static inline unsigned int fps_calc(const struct v4l2_bt_timings *t)
{
	if (!V4L2_DV_BT_FRAME_HEIGHT(t) || !V4L2_DV_BT_FRAME_WIDTH(t))
		return 0;

	return DIV_ROUND_CLOSEST((unsigned int)t->pixelclock,
			V4L2_DV_BT_FRAME_HEIGHT(t) * V4L2_DV_BT_FRAME_WIDTH(t));
}

static int rk628_csi_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct rk628_csi *csi = to_csi(sd);

	enable_stream(sd, enable);

	v4l2_info(sd, "%s: on: %d, %dx%d@%d\n", __func__, enable,
				csi->timings.bt.width,
				csi->timings.bt.height,
				fps_calc(&csi->timings.bt));

	return 0;
}

static int rk628_csi_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *sd_state,
		struct v4l2_subdev_mbus_code_enum *code)
{
	struct rk628_csi *csi = to_csi(sd);

	switch (code->index) {
	case 0:
		code->code = csi->plat_data->bus_fmt;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int rk628_csi_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct rk628_csi *csi = to_csi(sd);

	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != csi->plat_data->bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

static int rk628_csi_enum_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *sd_state,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	struct rk628_csi *csi = to_csi(sd);

	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = csi->plat_data->bus_fmt;

	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static int rk628_csi_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *sd_state,
		struct v4l2_subdev_format *format)
{
	struct rk628_csi *csi = to_csi(sd);

	if (!tx_5v_power_present(sd) || csi->nosignal) {
		v4l2_info(sd, "%s hdmirx no signal\n", __func__);
		format->format.code = csi->mbus_fmt_code;
		format->format.width = RK628_DEFAULT_WIDTH;
		format->format.height = RK628_DEFAULT_HEIGHT;
		format->format.field = V4L2_FIELD_NONE;
		return 0;
	}
	mutex_lock(&csi->confctl_mutex);
	format->format.code = csi->mbus_fmt_code;
	format->format.width = ALIGN_DOWN(csi->timings.bt.width, 8);
	format->format.height = csi->timings.bt.height;
	format->format.field = csi->timings.bt.interlaced ?
		V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;

	if (csi->plat_data->tx_mode == CSI_MODE) {
		if (csi->timings.bt.pixelclock > 150000000 || csi->csi_lanes_in_use <= 2) {
			v4l2_dbg(1, debug, sd,
				"%s res wxh:%dx%d, link freq:%llu, pixrate:%u\n",
				__func__, csi->timings.bt.width, csi->timings.bt.height,
				link_freq_menu_items[1], RK628_CSI_PIXEL_RATE_HIGH);
			__v4l2_ctrl_s_ctrl(csi->link_freq, 1);
			__v4l2_ctrl_s_ctrl_int64(csi->pixel_rate,
				RK628_CSI_PIXEL_RATE_HIGH);
		} else {
			v4l2_dbg(1, debug, sd,
				"%s res wxh:%dx%d, link freq:%llu, pixrate:%u\n",
				__func__, csi->timings.bt.width, csi->timings.bt.height,
				link_freq_menu_items[0], RK628_CSI_PIXEL_RATE_LOW);
			__v4l2_ctrl_s_ctrl(csi->link_freq, 0);
			__v4l2_ctrl_s_ctrl_int64(csi->pixel_rate,
				RK628_CSI_PIXEL_RATE_LOW);
		}
	} else {
		u32 rate;

		csi->dsi.rk628 = csi->rk628;
		csi->dsi.timings = csi->timings;
		rate = rk628_dsi_get_lane_rate_mbps(&csi->dsi);
		v4l2_dbg(1, debug, sd, "%s mipi bitrate:%u mbps\n", __func__, rate);

		if (rate > 1300) {
			csi->rk628->mipi_timing[0] = rk628f_dsi0_mipi;
			csi->rk628->mipi_timing[1] = rk628f_dsi0_mipi;
			__v4l2_ctrl_s_ctrl(csi->link_freq, 2);
		} else if (rate <= 1300 && rate > 700) {
			csi->rk628->mipi_timing[0] = rk628f_csi0_mipi;
			csi->rk628->mipi_timing[1] = rk628f_csi0_mipi;
			__v4l2_ctrl_s_ctrl(csi->link_freq, 1);
		} else {
			csi->rk628->mipi_timing[0] = rk628f_csi0_mipi;
			csi->rk628->mipi_timing[1] = rk628f_csi0_mipi;
			__v4l2_ctrl_s_ctrl(csi->link_freq, 0);
		}
		__v4l2_ctrl_s_ctrl_int64(csi->pixel_rate, RK628_CSI_PIXEL_RATE_HIGH);
	}

	mutex_unlock(&csi->confctl_mutex);
	v4l2_dbg(1, debug, sd, "%s: fmt code:%d, w:%d, h:%d, field code:%d\n",
			__func__, format->format.code, format->format.width,
			format->format.height, format->format.field);

	return 0;
}

static int rk628_csi_get_reso_dist(const struct rk628_csi_mode *mode,
		struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct rk628_csi_mode *
rk628_csi_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = rk628_csi_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int rk628_csi_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *sd_state,
		struct v4l2_subdev_format *format)
{
	struct rk628_csi *csi = to_csi(sd);
	const struct rk628_csi_mode *mode;

	u32 code = format->format.code; /* is overwritten by get_fmt */
	int ret = rk628_csi_get_fmt(sd, sd_state, format);

	format->format.code = code;

	if (ret)
		return ret;

	switch (code) {
	case MEDIA_BUS_FMT_UYVY8_2X8:
		if (csi->plat_data->bus_fmt == MEDIA_BUS_FMT_UYVY8_2X8)
			break;
		return -EINVAL;
	case MEDIA_BUS_FMT_RGB888_1X24:
		if (csi->plat_data->bus_fmt == MEDIA_BUS_FMT_RGB888_1X24)
			break;
		return -EINVAL;
	default:
		return -EINVAL;
	}

	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
		if (csi->plat_data->bus_fmt == MEDIA_BUS_FMT_UYVY8_2X8)
			return 0;

		*v4l2_subdev_get_try_format(sd, sd_state, format->pad) = format->format;
	}

	csi->mbus_fmt_code = format->format.code;
	mode = rk628_csi_find_best_fit(format);
	csi->cur_mode = mode;

	enable_stream(sd, false);

	return 0;
}

static int rk628_csi_g_edid(struct v4l2_subdev *sd,
		struct v4l2_subdev_edid *edid)
{
	struct rk628_csi *csi = to_csi(sd);
	u32 i, val;

	memset(edid->reserved, 0, sizeof(edid->reserved));

	if (edid->pad != 0)
		return -EINVAL;

	if (edid->start_block == 0 && edid->blocks == 0) {
		edid->blocks = csi->edid_blocks_written;
		return 0;
	}

	if (csi->edid_blocks_written == 0)
		return -ENODATA;

	if (edid->start_block >= csi->edid_blocks_written ||
			edid->blocks == 0)
		return -EINVAL;

	if (edid->start_block + edid->blocks > csi->edid_blocks_written)
		edid->blocks = csi->edid_blocks_written - edid->start_block;

	/* edid access by apb when read, i2c slave addr: 0x0 */
	rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON0,
			SW_ADAPTER_I2CSLADR_MASK |
			SW_EDID_MODE_MASK,
			SW_ADAPTER_I2CSLADR(0) |
			SW_EDID_MODE(1));

	for (i = 0; i < (edid->blocks * EDID_BLOCK_SIZE); i++) {
		rk628_i2c_read(csi->rk628, EDID_BASE + ((edid->start_block *
				EDID_BLOCK_SIZE) + i) * 4, &val);
		edid->edid[i] = val;
	}

	rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON0,
			SW_EDID_MODE_MASK,
			SW_EDID_MODE(0));

	return 0;
}

static int rk628_csi_s_edid(struct v4l2_subdev *sd,
				struct v4l2_subdev_edid *edid)
{
	struct rk628_csi *csi = to_csi(sd);
	u16 edid_len = edid->blocks * EDID_BLOCK_SIZE;
	u32 i, val;

	v4l2_dbg(1, debug, sd, "%s, pad %d, start block %d, blocks %d\n",
		 __func__, edid->pad, edid->start_block, edid->blocks);

	memset(edid->reserved, 0, sizeof(edid->reserved));

	if (edid->pad != 0)
		return -EINVAL;

	if (edid->start_block != 0)
		return -EINVAL;

	if (edid->blocks > EDID_NUM_BLOCKS_MAX) {
		edid->blocks = EDID_NUM_BLOCKS_MAX;
		return -E2BIG;
	}

	rk628_hdmirx_controller_reset(csi->rk628);
	rk628_hdmirx_hpd_ctrl(sd, false);

	if (edid->blocks == 0) {
		csi->edid_blocks_written = 0;
		return 0;
	}

	/* edid access by apb when write, i2c slave addr: 0x0 */
	rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON0,
			SW_ADAPTER_I2CSLADR_MASK |
			SW_EDID_MODE_MASK,
			SW_ADAPTER_I2CSLADR(0) |
			SW_EDID_MODE(1));

	for (i = 0; i < edid_len; i++)
		rk628_i2c_write(csi->rk628, EDID_BASE + i * 4, edid->edid[i]);

	/* read out for debug */
	if (debug >= 3) {
		pr_info("%s: Read EDID: ======\n", __func__);
		for (i = 0; i < edid_len; i++) {
			rk628_i2c_read(csi->rk628, EDID_BASE + i * 4, &val);
			pr_info("0x%02x ", val);
			if ((i + 1) % 8 == 0)
				pr_info("\n");
		}
		pr_info("%s: ======\n", __func__);
	}

	/* edid access by RX's i2c, i2c slave addr: 0x0 */
	rk628_i2c_update_bits(csi->rk628, GRF_SYSTEM_CON0,
			SW_ADAPTER_I2CSLADR_MASK |
			SW_EDID_MODE_MASK,
			SW_ADAPTER_I2CSLADR(0) |
			SW_EDID_MODE(0));
	csi->edid_blocks_written = edid->blocks;
	udelay(100);

	if (tx_5v_power_present(sd) && csi->rk628->version < RK628F_VERSION)
		rk628_hdmirx_hpd_ctrl(sd, true);

	return 0;
}

static void rk628_reset_edid(struct v4l2_subdev *sd, u8 *edid_data, u8 edid_version)
{
	struct rk628_csi *csi = to_csi(sd);
	struct v4l2_subdev_edid def_edid;

	rk628_hdmirx_plugout(&csi->sd);
	def_edid.pad = 0;
	def_edid.start_block = 0;
	def_edid.blocks = 2;
	def_edid.edid = edid_data;
	csi->edid_version = edid_version;
	rk628_csi_s_edid(sd, &def_edid);
	schedule_delayed_work(&csi->delayed_work_enable_hotplug,
			      msecs_to_jiffies(1000));
}

static int rk628_csi_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct rk628_csi *csi = to_csi(sd);
	const struct rk628_csi_mode *mode = csi->cur_mode;

	fi->interval = mode->max_fps;

	return 0;
}

static void rk628_csi_get_module_inf(struct rk628_csi *rk628_csi,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, RK628_CSI_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, rk628_csi->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, rk628_csi->len_name, sizeof(inf->base.lens));
}

static void rk628_csi_reset_streaming(struct v4l2_subdev *sd, int on)
{
	struct rk628_csi *csi = to_csi(sd);

	if (on) {
		if (csi->plat_data->tx_mode == CSI_MODE) {
			rk628_csi_soft_reset(sd);
			usleep_range(5000, 5500);
			if (csi->continues_clk) {
				rk628_i2c_update_bits(csi->rk628, CSITX_SYS_CTRL3_IMD,
					CONT_MODE_CLK_CLR_MASK | CONT_MODE_CLK_SET_MASK |
					NON_CONTINUOUS_MODE_MASK, CONT_MODE_CLK_CLR(0) |
					CONT_MODE_CLK_SET(1) | NON_CONTINUOUS_MODE(0));
				if (csi->rk628->version >= RK628F_VERSION)
					rk628_i2c_update_bits(csi->rk628, CSITX1_SYS_CTRL3_IMD,
						CONT_MODE_CLK_CLR_MASK | CONT_MODE_CLK_SET_MASK |
						NON_CONTINUOUS_MODE_MASK, CONT_MODE_CLK_CLR(0) |
						CONT_MODE_CLK_SET(1) | NON_CONTINUOUS_MODE(0));
			} else {
				rk628_i2c_update_bits(csi->rk628, CSITX_SYS_CTRL3_IMD,
					CONT_MODE_CLK_CLR_MASK | CONT_MODE_CLK_SET_MASK |
					NON_CONTINUOUS_MODE_MASK, CONT_MODE_CLK_CLR(0) |
					CONT_MODE_CLK_SET(0) | NON_CONTINUOUS_MODE(1));
				if (csi->rk628->version >= RK628F_VERSION)
					rk628_i2c_update_bits(csi->rk628, CSITX1_SYS_CTRL3_IMD,
						CONT_MODE_CLK_CLR_MASK | CONT_MODE_CLK_SET_MASK |
						NON_CONTINUOUS_MODE_MASK, CONT_MODE_CLK_CLR(0) |
						CONT_MODE_CLK_SET(0) | NON_CONTINUOUS_MODE(1));
			}

			rk628_i2c_update_bits(csi->rk628, CSITX_CSITX_EN,
					DPHY_EN_MASK | CSITX_EN_MASK, DPHY_EN(1) | CSITX_EN(1));
			rk628_i2c_write(csi->rk628, CSITX_CONFIG_DONE, CONFIG_DONE_IMD);
			if (csi->rk628->version >= RK628F_VERSION) {
				rk628_i2c_update_bits(csi->rk628, CSITX1_CSITX_EN,
					DPHY_EN_MASK | CSITX_EN_MASK, DPHY_EN(1) | CSITX_EN(1));
				rk628_i2c_write(csi->rk628, CSITX1_CONFIG_DONE, CONFIG_DONE_IMD);
			}
		} else {
			enable_dsitx(sd);
		}
		rk628_hdmirx_vid_enable(sd, true);
		if (csi->plat_data->tx_mode == CSI_MODE) {
			msleep(20);
			rk628_mipi_txdata_reset(sd);
			rk628_csi_enable_csi_interrupts(sd, true);
			csi->is_streaming = true;
		}
	} else {
		rk628_hdmirx_vid_enable(sd, false);
		if (csi->plat_data->tx_mode == CSI_MODE) {
			rk628_csi_enable_csi_interrupts(sd, false);
			msleep(20);
			rk628_csi_disable_stream(sd);
		} else {
			rk628_disable_dsitx(sd);
		}
	}
	v4l2_info(sd, "%s: on: %d, %dx%d@%d\n", __func__, on,
				csi->timings.bt.width,
				csi->timings.bt.height,
				fps_calc(&csi->timings.bt));
}

static void rk628_csi_set_color_range(struct v4l2_subdev *sd)
{
	struct rk628_csi *csi = to_csi(sd);
	u8 color_range;

	color_range = rk628_hdmirx_get_range(csi->rk628);
	if (csi->user_color_range == COLOR_RANGE_AUTO)
		rk628_post_process_csc_en(csi->rk628,
				color_range == HDMIRX_LIMIT_RANGE ? false : true, true);
	else if (csi->user_color_range == COLOR_RANGE_LIMIT)
		rk628_post_process_csc_en(csi->rk628, false, true);
	else
		rk628_post_process_csc_en(csi->rk628, true, true);
}

static long rk628_csi_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct rk628_csi *csi = to_csi(sd);
	long ret = 0;
	struct rkmodule_csi_dphy_param *dphy_param;
	struct rkmodule_capture_info  *capture_info;
	u32 stream = 0;
	int edid_version, i;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		rk628_csi_get_module_inf(csi, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDMI_MODE:
		*(int *)arg = RKMODULE_HDMIIN_MODE;
		break;
	case RKMODULE_SET_CSI_DPHY_PARAM:
		dphy_param = (struct rkmodule_csi_dphy_param *)arg;
		if (dphy_param->vendor == PHY_VENDOR_SAMSUNG)
			rk3588_dcphy_param = *dphy_param;
		v4l2_dbg(1, debug, sd,
			"sensor set dphy param\n");
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = (struct rkmodule_csi_dphy_param *)arg;
		*dphy_param = rk3588_dcphy_param;

		v4l2_dbg(1, debug, sd,
			"sensor get dphy param\n");
		break;
	case RKMODULE_GET_CAPTURE_MODE:
		capture_info = (struct rkmodule_capture_info *)arg;
		if (csi->rk628->dual_mipi) {
			v4l2_dbg(1, debug, sd, "set dual mipi mode\n");
			capture_info->mode = RKMODULE_MULTI_DEV_COMBINE_ONE;
			capture_info->multi_dev = csi->multi_dev_info;
		} else {
			capture_info->mode = 0;
			capture_info->multi_dev = csi->multi_dev_info;
		}
		break;
	case RKMODULE_SET_QUICK_STREAM:
		stream = *((u32 *)arg);
		rk628_csi_reset_streaming(sd, !!stream);
		break;
	case RKMODULE_GET_CSI_DSI_INFO:
		if (csi->plat_data->tx_mode == DSI_MODE)
			*(int *)arg = RKMODULE_DSI_INPUT;
		else
			*(int *)arg = RKMODULE_CSI_INPUT;
		break;
	case RK_HDMIRX_CMD_GET_FPS:
		*(int *)arg = fps_calc(&csi->timings.bt);
		break;
	case RK_HDMIRX_CMD_GET_HDCP_ENC_STATUS:
		*(int *)arg = rk628_hdmirx_get_hdcp_enc_status(csi->rk628);
		break;
	case RK_HDMIRX_CMD_GET_INPUT_MODE:
		*(int *)arg = csi->rk628->dvi_mode;
		break;
	case RK_HDMIRX_CMD_GET_SIGNAL_STABLE_STATUS:
		*(int *)arg = !csi->nosignal;
		break;
	case RK_HDMIRX_CMD_GET_SCAN_MODE:
		if (csi->timings.bt.interlaced == V4L2_DV_INTERLACED)
			*(int *)arg = HDMIRX_INTERLACED;
		else
			*(int *)arg = HDMIRX_PROGRESSIVE;
		break;
	case RK_HDMIRX_CMD_GET_EDID_MODE:
		*(int *)arg = HDMIRX_EDID_4K60HZ_YUV444;
		break;
	case RK_HDMIRX_CMD_SET_EDID_MODE:
		break;
	case RK_HDMIRX_CMD_GET_COLOR_RANGE:
		*(int *)arg = csi->rk628->color_range;
		break;
	case RK_HDMIRX_CMD_GET_COLOR_SPACE:
		*(int *)arg = csi->rk628->color_space;
		break;
	case RKMODULE_GET_DSI_MODE:
		*(int *)arg = csi->dsi.vid_mode;
		break;
	case RK_HDMIRX_CMD_SET_COLOR_RANGE:
		csi->user_color_range = *((int *)arg);
		if (csi->user_color_range > COLOR_RANGE_FULL ||
		    csi->user_color_range < COLOR_RANGE_AUTO)
			csi->user_color_range = COLOR_RANGE_AUTO;
		v4l2_info(sd, "user set color range: %d\n", csi->user_color_range);
		rk628_csi_set_color_range(sd);
		break;
	case RK_HDMIRX_CMD_GET_EDID_VERSION:
		*(int *)arg = csi->edid_version;
		break;
	case RK_HDMIRX_CMD_SET_EDID_VERSION:
		edid_version = *((int *)arg);
		if (edid_version <= 0)
			return -EINVAL;
		for (i = 0; i < ARRAY_SIZE(edid_data); i++) {
			if (edid_data[i].version == edid_version) {
				rk628_reset_edid(sd, edid_data[i].data, edid_data[i].version);
				return 0;
			}
		}
		v4l2_info(sd, "the edid version is not supported: %d\n", edid_version);
		return -EINVAL;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static int mipi_dphy_power_on(struct rk628_csi *csi)
{
	unsigned int val;
	u32 bus_width, mask;
	struct v4l2_subdev *sd = &csi->sd;
	int ret;

	if (csi->timings.bt.pixelclock > 150000000 || csi->csi_lanes_in_use <= 2) {
		csi->lane_mbps = MIPI_DATARATE_MBPS_HIGH;
	} else {
		csi->lane_mbps = MIPI_DATARATE_MBPS_LOW;
	}
	if (csi->rk628->dual_mipi)
		csi->lane_mbps = MIPI_DATARATE_MBPS_HIGH;
	bus_width =  csi->lane_mbps << 8;
	bus_width |= COMBTXPHY_MODULEA_EN;
	if (csi->rk628->version >= RK628F_VERSION)
		bus_width |= COMBTXPHY_MODULEB_EN;
	v4l2_dbg(1, debug, sd, "%s mipi bitrate:%llu mbps\n", __func__,
			csi->lane_mbps);
	rk628_mipi_dphy_reset_assert(csi->rk628);
	rk628_mipi_dphy_reset_deassert(csi->rk628);

	rk628_txphy_set_bus_width(csi->rk628, bus_width);
	rk628_txphy_set_mode(csi->rk628, PHY_MODE_VIDEO_MIPI);

	rk628_mipi_dphy_init_hsfreqrange(csi->rk628, csi->lane_mbps, 0);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_mipi_dphy_init_hsfreqrange(csi->rk628, csi->lane_mbps, 1);

	if (csi->rk628->dual_mipi) {
		rk628_mipi_dphy_init_hsmanual(csi->rk628, true, 0);
		rk628_mipi_dphy_init_hsmanual(csi->rk628, true, 1);
	} else if (csi->lane_mbps == MIPI_DATARATE_MBPS_HIGH && !csi->rk628->dual_mipi) {
		rk628_mipi_dphy_init_hsmanual(csi->rk628, true, 0);
		if (csi->rk628->version >= RK628F_VERSION)
			rk628_mipi_dphy_init_hsmanual(csi->rk628, false, 1);
	} else {
		rk628_mipi_dphy_init_hsmanual(csi->rk628, false, 0);
		if (csi->rk628->version >= RK628F_VERSION)
			rk628_mipi_dphy_init_hsmanual(csi->rk628, false, 1);
	}
	rk628_txphy_power_on(csi->rk628);

	usleep_range(1500, 2000);
	ret = regmap_read_poll_timeout(csi->rk628->regmap[RK628_DEV_CSI],
				       CSITX_CSITX_STATUS1,
				       val, val & DPHY_PLL_LOCK,
				       0, 1000);
	if (ret < 0)
		dev_err(csi->rk628->dev, "csi0 phy is not locked\n");
	if (csi->rk628->version >= RK628F_VERSION) {
		ret = regmap_read_poll_timeout(csi->rk628->regmap[RK628_DEV_CSI1],
				       CSITX1_CSITX_STATUS1,
				       val, val & DPHY_PLL_LOCK,
				       0, 1000);
		if (ret < 0)
			dev_err(csi->rk628->dev, "csi1 phy is not locked\n");
	}

	mask = STOPSTATE_CLK | STOPSTATE_LANE0;
	ret = regmap_read_poll_timeout(csi->rk628->regmap[RK628_DEV_CSI],
				       CSITX_CSITX_STATUS1,
				       val, (val & mask) == mask,
				       0, 1000);
	if (ret < 0)
		dev_err(csi->rk628->dev, "csi0 lane module is not in stop state, val: 0x%x\n", val);

	if (csi->rk628->version >= RK628F_VERSION) {
		ret = regmap_read_poll_timeout(csi->rk628->regmap[RK628_DEV_CSI1],
				       CSITX1_CSITX_STATUS1,
				       val, (val & mask) == mask,
				       0, 1000);
		if (ret < 0)
			dev_err(csi->rk628->dev,
				"csi1 lane module is not in stop state, val: 0x%x\n", val);
	}

	return 0;
}

static void mipi_dphy_power_off(struct rk628_csi *csi)
{
	rk628_txphy_power_off(csi->rk628);
}

#ifdef CONFIG_COMPAT
static long rk628_csi_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	long ret;
	int *seq;
	struct rkmodule_csi_dphy_param *dphy_param;
	struct rkmodule_capture_info  *capture_info;
	u32 stream = 0;
	int is_full_range = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = rk628_csi_ioctl(sd, cmd, inf);
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

		ret = rk628_csi_ioctl(sd, cmd, seq);
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
			ret = rk628_csi_ioctl(sd, cmd, dphy_param);
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

		ret = rk628_csi_ioctl(sd, cmd, dphy_param);
		if (!ret) {
			ret = copy_to_user(up, dphy_param, sizeof(*dphy_param));
			if (ret)
				ret = -EFAULT;
		}
		kfree(dphy_param);
		break;
	case RKMODULE_GET_CAPTURE_MODE:
		capture_info = kzalloc(sizeof(*capture_info), GFP_KERNEL);
		if (!capture_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = rk628_csi_ioctl(sd, cmd, capture_info);
		if (!ret) {
			ret = copy_to_user(up, capture_info, sizeof(*capture_info));
			if (ret)
				ret = -EFAULT;
		}
		kfree(capture_info);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = rk628_csi_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	case RKMODULE_GET_CSI_DSI_INFO:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_GET_FPS:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_GET_HDCP_ENC_STATUS:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_GET_INPUT_MODE:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_GET_SIGNAL_STABLE_STATUS:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_GET_SCAN_MODE:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_GET_EDID_MODE:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_SET_EDID_MODE:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_GET_COLOR_RANGE:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_GET_COLOR_SPACE:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RKMODULE_GET_DSI_MODE:
		seq = kzalloc(sizeof(*seq), GFP_KERNEL);
		if (!seq) {
			ret = -ENOMEM;
			return ret;
		}
		ret = rk628_csi_ioctl(sd, cmd, seq);
		if (!ret) {
			ret = copy_to_user(up, seq, sizeof(*seq));
			if (ret)
				ret = -EFAULT;
		}
		kfree(seq);
		break;
	case RK_HDMIRX_CMD_SET_COLOR_RANGE:
		ret = copy_from_user(&is_full_range, up, sizeof(int));
		if (!ret)
			ret = rk628_csi_ioctl(sd, cmd, &is_full_range);
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

static const struct v4l2_subdev_core_ops rk628_csi_core_ops = {
	.interrupt_service_routine = rk628_isr_process,
	.subscribe_event = rk628_csi_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
	.ioctl = rk628_csi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = rk628_csi_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops rk628_csi_video_ops = {
	.g_input_status = rk628_csi_g_input_status,
	.s_dv_timings = rk628_csi_s_dv_timings,
	.g_dv_timings = rk628_csi_g_dv_timings,
	.query_dv_timings = rk628_csi_query_dv_timings,
	.s_stream = rk628_csi_s_stream,
	.g_frame_interval = rk628_csi_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops rk628_csi_pad_ops = {
	.enum_mbus_code = rk628_csi_enum_mbus_code,
	.enum_frame_size = rk628_csi_enum_frame_sizes,
	.enum_frame_interval = rk628_csi_enum_frame_interval,
	.set_fmt = rk628_csi_set_fmt,
	.get_fmt = rk628_csi_get_fmt,
	.get_edid = rk628_csi_g_edid,
	.set_edid = rk628_csi_s_edid,
	.enum_dv_timings = rk628_csi_enum_dv_timings,
	.dv_timings_cap = rk628_csi_dv_timings_cap,
	.get_mbus_config = rk628_csi_g_mbus_config,
};

static const struct v4l2_subdev_ops rk628_csi_ops = {
	.core = &rk628_csi_core_ops,
	.video = &rk628_csi_video_ops,
	.pad = &rk628_csi_pad_ops,
};

static int rk628_csi_get_custom_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct rk628_csi *csi = container_of(ctrl->handler, struct rk628_csi,
			hdl);
	struct v4l2_subdev *sd = &csi->sd;

	if (ctrl->id == RK_V4L2_CID_AUDIO_SAMPLING_RATE) {
		*ctrl->p_new.p_s32 = get_audio_sampling_rate(sd);
	} else if (ctrl->id == RK_V4L2_CID_AUDIO_PRESENT) {
		*ctrl->p_new.p_s32 = tx_5v_power_present(sd) ?
			rk628_hdmirx_audio_present(csi->audio_info) : 0;
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops rk628_csi_custom_ctrl_ops = {
	.g_volatile_ctrl = rk628_csi_get_custom_ctrl,
};

static const struct v4l2_ctrl_config rk628_csi_ctrl_audio_sampling_rate = {
	.ops = &rk628_csi_custom_ctrl_ops,
	.id = RK_V4L2_CID_AUDIO_SAMPLING_RATE,
	.name = "Audio sampling rate",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 768000,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_ctrl_config rk628_csi_ctrl_audio_present = {
	.ops = &rk628_csi_custom_ctrl_ops,
	.id = RK_V4L2_CID_AUDIO_PRESENT,
	.name = "Audio present",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_READ_ONLY,
};

static irqreturn_t plugin_detect_irq(int irq, void *dev_id)
{
	struct rk628_csi *csi = dev_id;
	struct v4l2_subdev *sd = &csi->sd;
	const struct v4l2_event evt_signal_lost = {
		.type = RK_HDMIRX_V4L2_EVENT_SIGNAL_LOST,
	};

	if (csi->plat_data->tx_mode == DSI_MODE)
		rk628_dsi_disable(sd);
	if (csi->plat_data->tx_mode == CSI_MODE) {
		rk628_csi_enable_csi_interrupts(sd, false);
		rk628_csi_disable_stream(sd);
	}
	csi->nosignal = true;
	/* control hpd after 50ms */
	schedule_delayed_work(&csi->delayed_work_enable_hotplug, HZ / 20);
	v4l2_event_queue(sd->devnode, &evt_signal_lost);

	return IRQ_HANDLED;
}

static void rk628_csi_audio_info_cb(struct rk628 *rk628, bool on)
{
	struct i2c_client *client = to_i2c_client(rk628->dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	const struct v4l2_event evt_audio_info = {
		.type = RK_HDMIRX_V4L2_EVENT_AUDIOINFO,
	};
	v4l2_event_queue(sd->devnode, &evt_audio_info);
}

static int rk628_csi_power_on(struct rk628_csi *csi)
{
	clk_prepare_enable(csi->soc_24M);
	if (csi->enable_gpio) {
		gpiod_set_value(csi->enable_gpio, 1);
		usleep_range(10000, 11000);
	}
	gpiod_set_value(csi->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value(csi->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value(csi->reset_gpio, 0);
	usleep_range(10000, 11000);

	if (csi->power_gpio) {
		gpiod_set_value(csi->power_gpio, 1);
		usleep_range(10000, 11000);
	}

	return 0;
}

static int rk628_csi_power_off(struct rk628_csi *csi)
{
	if (csi->enable_gpio) {
		gpiod_set_value(csi->enable_gpio, 0);
		usleep_range(10000, 11000);
	}
	gpiod_set_value(csi->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value(csi->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value(csi->reset_gpio, 1);
	usleep_range(10000, 11000);

	if (csi->power_gpio) {
		gpiod_set_value(csi->power_gpio, 0);
		usleep_range(10000, 11000);
	}
	clk_disable_unprepare(csi->soc_24M);

	return 0;
}

static int rk628_csi_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct rk628_csi *csi = to_csi(sd);

	v4l2_info(sd, "%s: resume!\n", __func__);

	rk628_csi_power_on(csi);
	rk628_cru_initialize(csi->rk628);
	rk628_csi_initial(sd);
	rk628_hdmirx_plugout(sd);
	enable_irq(csi->plugin_irq);
	enable_irq(csi->hdmirx_irq);
	schedule_delayed_work(&csi->delayed_work_enable_hotplug,
			     msecs_to_jiffies(500));
	csi->rk628->is_suspend = false;

	return 0;
}

static int rk628_csi_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct rk628_csi *csi = to_csi(sd);

	v4l2_info(sd, "%s: suspend!\n", __func__);

	csi->rk628->is_suspend = true;
	rk628_hdmirx_plugout(sd);
	disable_irq(csi->plugin_irq);
	disable_irq(csi->hdmirx_irq);
	cancel_delayed_work_sync(&csi->delayed_work_res_change);
	cancel_delayed_work_sync(&csi->delayed_work_enable_hotplug);
	rk628_hdmirx_audio_cancel_work_audio(csi->audio_info, true);
	rk628_csi_power_off(csi);

	return 0;
}

static const struct dev_pm_ops rk628_csi_pm_ops = {
	.suspend = rk628_csi_suspend,
	.resume = rk628_csi_resume,
};

static int rk628_csi_probe_of(struct rk628_csi *csi)
{
	struct device *dev = csi->dev;
	struct v4l2_fwnode_endpoint endpoint = { .bus_type = 0 };
	struct device_node *ep;
	int ret = -EINVAL;
	bool hdcp1x_enable = false, i2s_enable_default = false;
	bool scaler_en = false;
	bool continues_clk = false;

	csi->soc_24M = devm_clk_get(dev, "soc_24M");
	if (csi->soc_24M == ERR_PTR(-ENOENT))
		csi->soc_24M = NULL;
	if (IS_ERR(csi->soc_24M)) {
		ret = PTR_ERR(csi->soc_24M);
		dev_err(dev, "Unable to get soc_24M: %d\n", ret);
	}

	csi->enable_gpio = devm_gpiod_get_optional(dev, "enable",
						     GPIOD_OUT_LOW);
	if (IS_ERR(csi->enable_gpio)) {
		ret = PTR_ERR(csi->enable_gpio);
		dev_err(dev, "failed to request enable GPIO: %d\n", ret);
		return ret;
	}

	csi->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(csi->reset_gpio)) {
		ret = PTR_ERR(csi->reset_gpio);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		return ret;
	}

	csi->power_gpio = devm_gpiod_get_optional(dev, "power", GPIOD_OUT_HIGH);
	if (IS_ERR(csi->power_gpio)) {
		dev_err(dev, "failed to get power gpio\n");
		ret = PTR_ERR(csi->power_gpio);
		return ret;
	}

	csi->plugin_det_gpio = devm_gpiod_get_optional(dev, "plugin-det",
						    GPIOD_IN);
	if (IS_ERR(csi->plugin_det_gpio)) {
		dev_err(dev, "failed to get hdmirx det gpio\n");
		ret = PTR_ERR(csi->plugin_det_gpio);
		return ret;
	}
	csi->rk628->hdmirx_det_gpio = csi->plugin_det_gpio;

	if (of_property_read_bool(dev->of_node, "hdcp-enable"))
		hdcp1x_enable = true;

	if (of_property_read_bool(dev->of_node, "cec-enable"))
		csi->cec_enable = true;

	if (of_property_read_bool(dev->of_node, "i2s-enable-default"))
		i2s_enable_default = true;

	if (csi->plat_data->tx_mode == DSI_MODE) {
		if (of_property_read_bool(dev->of_node, "dsi-video-mode"))
			csi->dsi.vid_mode = VIDEO_MODE;
		else
			csi->dsi.vid_mode = COMMAND_MODE;
	}

	if (of_property_read_bool(dev->of_node, "scaler-en"))
		scaler_en = true;

	if (of_property_read_bool(dev->of_node, "continues-clk"))
		continues_clk = true;

	ep = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!ep) {
		dev_err(dev, "missing endpoint node\n");
		ret = -EINVAL;
		return ret;
	}

	ret = v4l2_fwnode_endpoint_alloc_parse(of_fwnode_handle(ep), &endpoint);
	if (ret) {
		dev_err(dev, "failed to parse endpoint\n");
		goto put_node;
	}

	if (endpoint.bus_type != V4L2_MBUS_CSI2_DPHY ||
	    endpoint.bus.mipi_csi2.num_data_lanes == 0) {
		dev_err(dev, "missing CSI-2 properties in endpoint\n");
		goto free_endpoint;
	}

	csi->csi_lanes_in_use = endpoint.bus.mipi_csi2.num_data_lanes;
	csi->enable_hdcp = hdcp1x_enable;
	csi->hdcp.enable = hdcp1x_enable;
	csi->i2s_enable_default = i2s_enable_default;
	csi->scaler_en = scaler_en;
	if (csi->scaler_en)
		csi->timings = dst_timing;
	csi->continues_clk = continues_clk;

	csi->rxphy_pwron = false;
	csi->txphy_pwron = false;
	csi->nosignal = true;
	csi->stream_state = 0;
	csi->avi_rcv_rdy = false;
	csi->user_color_range = COLOR_RANGE_AUTO;

	ret = 0;

free_endpoint:
	v4l2_fwnode_endpoint_free(&endpoint);
put_node:
	of_node_put(ep);

	return ret;
}

static const struct rk628_plat_data rk628_csi_data = {
	.bus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
	.tx_mode = CSI_MODE,
};

static const struct rk628_plat_data rk628_dsi_data = {
	.bus_fmt = MEDIA_BUS_FMT_RGB888_1X24,
	.tx_mode = DSI_MODE,
};

static const struct i2c_device_id rk628_csi_i2c_id[] = {
	{ "rk628-csi-v4l2", 0 },
	{ "rk628-dsi-v4l2", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, rk628_csi_i2c_id);

static const struct of_device_id rk628_csi_of_match[] = {
	{ .compatible = "rockchip,rk628-csi-v4l2", .data = &rk628_csi_data },
	{ .compatible = "rockchip,rk628-dsi-v4l2", .data = &rk628_dsi_data },
	{}
};
MODULE_DEVICE_TABLE(of, rk628_csi_of_match);

static bool tx_5v_power_present(struct v4l2_subdev *sd);

static ssize_t audio_rate_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct rk628_csi *csi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", rk628_hdmirx_audio_fs(csi->audio_info));
}

static ssize_t audio_present_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct rk628_csi *csi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d",
			tx_5v_power_present(&csi->sd) ?
			rk628_hdmirx_audio_present(csi->audio_info) : 0);
}

static ssize_t arc_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct rk628_csi *csi = dev_get_drvdata(dev);
	struct rk_hdmirx_dev *hdmirx_dev = dev_get_drvdata(dev);

	if (!hdmirx_dev)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			rk628_hdmirx_get_arc_enable(csi->audio_info) ? 1 : 0);
}

static ssize_t arc_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rk628_csi *csi = dev_get_drvdata(dev);
	struct rk_hdmirx_dev *hdmirx_dev = dev_get_drvdata(dev);
	bool enabled;
	int ret;

	if (!hdmirx_dev)
		return -EINVAL;

	ret = kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	rk628_hdmirx_set_arc_enable(csi->audio_info, enabled);

	return count;
}

static ssize_t edid_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct rk628_csi *csi = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", csi->edid_version);
}

static ssize_t edid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rk628_csi *csi = dev_get_drvdata(dev);
	int version, ret, i;

	ret = kstrtoint(buf, 0, &version);
	if (ret)
		return ret;

	if (version <= 0)
		return count;
	for (i = 0; i < ARRAY_SIZE(edid_data); i++) {
		if (edid_data[i].version == version) {
			rk628_reset_edid(&csi->sd, edid_data[i].data, edid_data[i].version);
			break;
		}
	}

	return count;
}

static DEVICE_ATTR_RO(audio_rate);
static DEVICE_ATTR_RO(audio_present);
static DEVICE_ATTR_RW(arc_enable);
static DEVICE_ATTR_RW(edid);

static struct attribute *rk628_attrs[] = {
	&dev_attr_audio_rate.attr,
	&dev_attr_audio_present.attr,
	&dev_attr_arc_enable.attr,
	&dev_attr_edid.attr,
	NULL
};
ATTRIBUTE_GROUPS(rk628);

static int rk628_csi_get_multi_dev_info(struct rk628_csi *csi)
{
	struct device *dev = &csi->i2c_client->dev;
	struct device_node *node = dev->of_node;
	struct device_node *multi_info_np;

	csi->dual_mipi_use = false;
	multi_info_np = of_get_child_by_name(node, "multi-dev-info");
	if (!multi_info_np) {
		dev_info(dev, "failed to get multi dev info\n");
		return -EINVAL;
	}

	of_property_read_u32(multi_info_np, "dev-idx-l",
			&csi->multi_dev_info.dev_idx[0]);
	of_property_read_u32(multi_info_np, "dev-idx-r",
			&csi->multi_dev_info.dev_idx[1]);
	of_property_read_u32(multi_info_np, "combine-idx",
			&csi->multi_dev_info.combine_idx[0]);
	of_property_read_u32(multi_info_np, "pixel-offset",
			&csi->multi_dev_info.pixel_offset);
	of_property_read_u32(multi_info_np, "dev-num",
			&csi->multi_dev_info.dev_num);

	csi->dual_mipi_use = true;
	dev_info(dev,
		"multi dev left: mipi%d, multi dev right: mipi%d, combile mipi%d, dev num: %d\n",
		csi->multi_dev_info.dev_idx[0], csi->multi_dev_info.dev_idx[1],
		csi->multi_dev_info.combine_idx[0], csi->multi_dev_info.dev_num);

	return 0;
}

static int rk628_csi_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct rk628_csi *csi;
	struct v4l2_subdev *sd;
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	char facing[2];
	int err;
	u32 val;
	struct rk628 *rk628;
	unsigned long irq_flags;
	const struct of_device_id *match;
	struct v4l2_dv_timings default_timing =
				V4L2_DV_BT_CEA_640X480P59_94;

	dev_info(dev, "RK628 I2C driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	if (!of_device_is_available(dev->of_node))
		return -ENODEV;

	csi = devm_kzalloc(dev, sizeof(*csi), GFP_KERNEL);
	if (!csi)
		return -ENOMEM;

	csi->dev = dev;
	csi->i2c_client = client;
	rk628 = rk628_i2c_register(client);
	if (!rk628)
		return -ENOMEM;

	match = of_match_node(rk628_csi_of_match, dev->of_node);
	csi->plat_data = match->data;

	rk628->tx_mode = csi->plat_data->tx_mode;
	csi->rk628 = rk628;
	csi->dsi.rk628 = rk628;
	csi->cur_mode = &supported_modes[0];
	csi->timings = default_timing;
	csi->hdmirx_irq = client->irq;
	sd = &csi->sd;
	sd->dev = dev;

	csi->hpd_output_inverted = of_property_read_bool(node,
			"hpd-output-inverted");
	err = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &csi->module_index);
	err |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &csi->module_facing);
	err |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &csi->module_name);
	err |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &csi->len_name);
	if (err) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	err = rk628_csi_probe_of(csi);
	if (err) {
		v4l2_err(sd, "rk628_csi_probe_of failed! err:%d\n", err);
		return err;
	}

	rk628_csi_power_on(csi);
	rk628_cru_initialize(csi->rk628);

	rk628_version_parse(rk628);
	if (rk628->version == RK628_UNKNOWN) {
		v4l2_err(sd, "can't get rk628 version\n");
		err = -ENODEV;
		goto power_off;
	}
	rk628_clk_set_rate(rk628, CGU_CLK_CPLL, CPLL_REF_CLK);

	if (rk628->version >= RK628F_VERSION) {
		err = rk628_csi_get_multi_dev_info(csi);
		if (err)
			v4l2_info(sd, "get multi dev info failed, not use dual mipi mode\n");
	}

	v4l2_i2c_subdev_init(sd, client, &rk628_csi_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;

	/* i2c access, read chip id*/
	err = rk628_i2c_read(csi->rk628, CSITX_CSITX_VERSION, &val);
	if (err) {
		v4l2_err(sd, "i2c access failed! err:%d\n", err);
		err = -ENODEV;
		goto power_off;
	}
	v4l2_dbg(1, debug, sd, "CSITX VERSION: %#x\n", val);

	if (rk628->version >= RK628F_VERSION) {
		err = rk628_i2c_read(csi->rk628, CSITX1_CSITX_VERSION, &val);
		if (err) {
			v4l2_err(sd, "i2c access failed! err:%d\n", err);
			err = -ENODEV;
			goto power_off;
		}
		v4l2_dbg(1, debug, sd, "CSITX1 VERSION: %#x\n", val);
	}

	mutex_init(&csi->confctl_mutex);

	csi->txphy = rk628_txphy_register(rk628);
	if (!csi->txphy) {
		v4l2_err(sd, "register txphy failed\n");
		err = -ENOMEM;
		goto power_off;
	}

	/* control handlers */
	v4l2_ctrl_handler_init(&csi->hdl, 4);
	csi->link_freq = v4l2_ctrl_new_int_menu(&csi->hdl, NULL,
			V4L2_CID_LINK_FREQ,
			ARRAY_SIZE(link_freq_menu_items) - 1,
			0, link_freq_menu_items);
	csi->pixel_rate = v4l2_ctrl_new_std(&csi->hdl, NULL,
			V4L2_CID_PIXEL_RATE, 0, RK628_CSI_PIXEL_RATE_HIGH, 1,
			RK628_CSI_PIXEL_RATE_HIGH);
	csi->detect_tx_5v_ctrl = v4l2_ctrl_new_std(&csi->hdl,
			NULL, V4L2_CID_DV_RX_POWER_PRESENT,
			0, 1, 0, 0);

	/* custom controls */
	csi->audio_sampling_rate_ctrl = v4l2_ctrl_new_custom(&csi->hdl,
			&rk628_csi_ctrl_audio_sampling_rate, NULL);
	if (csi->audio_sampling_rate_ctrl)
		csi->audio_sampling_rate_ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	csi->audio_present_ctrl = v4l2_ctrl_new_custom(&csi->hdl,
			&rk628_csi_ctrl_audio_present, NULL);
	if (csi->audio_present_ctrl)
		csi->audio_present_ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	sd->ctrl_handler = &csi->hdl;
	if (csi->hdl.error) {
		err = csi->hdl.error;
		v4l2_err(sd, "cfg v4l2 ctrls failed! err:%d\n", err);
		goto err_hdl;
	}

	if (rk628_csi_update_controls(sd)) {
		err = -ENODEV;
		v4l2_err(sd, "update v4l2 ctrls failed! err:%d\n", err);
		goto err_hdl;
	}

	csi->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	err = media_entity_pads_init(&sd->entity, 1, &csi->pad);
	if (err < 0) {
		v4l2_err(sd, "media entity init failed! err:%d\n", err);
		goto err_hdl;
	}

	if (csi->plat_data->tx_mode == DSI_MODE)
		csi->mbus_fmt_code = MEDIA_BUS_FMT_RGB888_1X24;
	else
		csi->mbus_fmt_code = MEDIA_BUS_FMT_UYVY8_2X8;

	memset(facing, 0, sizeof(facing));
	if (strcmp(csi->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 csi->module_index, facing,
		 RK628_CSI_NAME, dev_name(sd->dev));
	err = v4l2_async_register_subdev(sd);
	if (err < 0) {
		v4l2_err(sd, "v4l2 register subdev failed! err:%d\n", err);
		goto err_hdl;
	}

	csi->classdev = device_create_with_groups(rk_hdmirx_class(),
						  dev, MKDEV(0, 0),
						  csi,
						  rk628_groups,
						  "rk628");
	if (IS_ERR(csi->classdev))
		goto err_hdl;

	csi->extcon = devm_extcon_dev_allocate(dev, rk628_csi_extcon_cable);
	if (IS_ERR(csi->extcon)) {
		err = PTR_ERR(csi->extcon);
		v4l2_err(sd, "allocate extcon failed\n");
		goto err_hdl;
	}

	err = devm_extcon_dev_register(dev, csi->extcon);
	if (err) {
		v4l2_err(sd, "failed to register extcon: %d\n", err);
		goto err_hdl;
	}

	INIT_DELAYED_WORK(&csi->delayed_work_enable_hotplug,
			rk628_csi_delayed_work_enable_hotplug);
	INIT_DELAYED_WORK(&csi->delayed_work_res_change,
			rk628_delayed_work_res_change);
	csi->audio_info = rk628_hdmirx_audioinfo_alloc(dev,
						       &csi->confctl_mutex,
						       rk628,
						       csi->i2s_enable_default,
						       rk628_csi_audio_info_cb);
	if (!csi->audio_info) {
		err = -ENOMEM;
		v4l2_err(sd, "request audio info fail\n");
		goto err_work_queues;
	}
	rk628_csi_initial_setup(sd);

	if (csi->hdmirx_irq) {
		irq_flags = irqd_get_trigger_type(irq_get_irq_data(csi->hdmirx_irq));
		v4l2_dbg(1, debug, sd, "cfg hdmirx irq, flags: %lu!\n", irq_flags);
		err = devm_request_threaded_irq(dev, csi->hdmirx_irq, NULL,
				rk628_csi_irq_handler, irq_flags |
				IRQF_ONESHOT, "rk628_csi", csi);
		if (err) {
			v4l2_err(sd, "request rk628-csi irq failed! err:%d\n",
					err);
			goto err_work_queues;
		}
	} else {
		v4l2_dbg(1, debug, sd, "no irq, cfg poll!\n");
		INIT_WORK(&csi->work_i2c_poll,
			  rk628_csi_work_i2c_poll);
		timer_setup(&csi->timer, rk628_csi_irq_poll_timer, 0);
		csi->timer.expires = jiffies +
				       msecs_to_jiffies(POLL_INTERVAL_MS);
		add_timer(&csi->timer);
	}

	if (csi->plugin_det_gpio) {
		csi->plugin_irq = gpiod_to_irq(csi->plugin_det_gpio);
		if (csi->plugin_irq < 0) {
			dev_err(csi->dev, "failed to get plugin det irq\n");
			err = csi->plugin_irq;
			goto err_work_queues;
		}

		err = devm_request_threaded_irq(dev, csi->plugin_irq, NULL,
				plugin_detect_irq, IRQF_TRIGGER_FALLING |
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, "rk628_csi", csi);
		if (err) {
			dev_err(csi->dev, "failed to register plugin det irq (%d)\n", err);
			goto err_work_queues;
		}
	}

	err = v4l2_ctrl_handler_setup(sd->ctrl_handler);
	if (err) {
		v4l2_err(sd, "v4l2 ctrl handler setup failed! err:%d\n", err);
		goto err_work_queues;
	}
	csi->rk628->dual_mipi = false;
	rk628_debugfs_create(csi->rk628);
	rk628_hdmirx_debugfs_create(rk628, &csi->hdcp);

	if (csi->cec_enable)
		csi->cec = rk628_hdmirx_cec_register(rk628);

	v4l2_info(sd, "%s found @ 0x%x (%s)\n", client->name,
		  client->addr << 1, client->adapter->name);

	return 0;

err_work_queues:
	if (!csi->hdmirx_irq)
		flush_work(&csi->work_i2c_poll);
	cancel_delayed_work(&csi->delayed_work_enable_hotplug);
	cancel_delayed_work(&csi->delayed_work_res_change);
	rk628_hdmirx_audio_destroy(csi->audio_info);
err_hdl:
	mutex_destroy(&csi->confctl_mutex);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&csi->hdl);
power_off:
	rk628_csi_power_off(csi);

	return err;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void rk628_csi_remove(struct i2c_client *client)
#else
static int rk628_csi_remove(struct i2c_client *client)
#endif
{
	struct rk628_csi *csi = i2c_get_clientdata(client);

	rk628_debugfs_remove(csi->rk628);
	if (!csi->hdmirx_irq) {
		del_timer_sync(&csi->timer);
		flush_work(&csi->work_i2c_poll);
	}

	if (csi->cec_enable && csi->cec)
		rk628_hdmirx_cec_unregister(csi->cec);

	rk628_hdmirx_audio_cancel_work_audio(csi->audio_info, true);
	rk628_hdmirx_audio_cancel_work_rate_change(csi->audio_info, true);
	cancel_delayed_work_sync(&csi->delayed_work_enable_hotplug);
	cancel_delayed_work_sync(&csi->delayed_work_res_change);

	if (csi->rxphy_pwron)
		rk628_rxphy_power_off(csi->rk628);
	if (csi->txphy_pwron)
		mipi_dphy_power_off(csi);

	mutex_destroy(&csi->confctl_mutex);

	rk628_control_assert(csi->rk628, RGU_HDMIRX);
	rk628_control_assert(csi->rk628, RGU_HDMIRX_PON);
	rk628_control_assert(csi->rk628, RGU_DECODER);
	rk628_control_assert(csi->rk628, RGU_CLK_RX);
	rk628_control_assert(csi->rk628, RGU_VOP);
	rk628_control_assert(csi->rk628, RGU_CSI);
	if (csi->rk628->version >= RK628F_VERSION)
		rk628_control_assert(csi->rk628, RGU_CSI1);
	rk628_csi_power_off(csi);
#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static struct i2c_driver rk628_csi_i2c_driver = {
	.driver = {
		.name = "rk628-csi-v4l2",
		.pm = &rk628_csi_pm_ops,
		.of_match_table = of_match_ptr(rk628_csi_of_match),
	},
	.id_table = rk628_csi_i2c_id,
	.probe	= rk628_csi_probe,
	.remove = rk628_csi_remove,
};

module_i2c_driver(rk628_csi_i2c_driver);

MODULE_DESCRIPTION("Rockchip RK628 HDMI to MIPI CSI-2 bridge I2C driver");
MODULE_AUTHOR("Dingxian Wen <shawn.wen@rock-chips.com>");
MODULE_AUTHOR("Shunqing Chen <csq@rock-chips.com>");
MODULE_LICENSE("GPL");
