// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:
 *      Algea Cao <algea.cao@rock-chips.com>
 */
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/extcon-provider.h>
#include <linux/extcon.h>
#include <linux/hdmi.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_encoder_slave.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <drm/bridge/dw_hdmi.h>
#include <drm/display/drm_dsc.h>
#include <drm/display/drm_hdcp_helper.h>
#include <drm/display/drm_hdmi_helper.h>
#include <drm/display/drm_scdc_helper.h>

#include <uapi/linux/media-bus-format.h>
#include <uapi/linux/videodev2.h>

#include "dw-hdmi-qp-audio.h"
#include "dw-hdmi-qp.h"
#include "dw-hdmi-qp-cec.h"
#include "dw-hdmi-qp-hdcp.h"

#include <media/cec-notifier.h>

#define DDC_CI_ADDR		0x37
#define DDC_SEGMENT_ADDR	0x30

#define HDMI_EDID_LEN		512
#define VENDOR_INFO_LEN		10
#define HDMI_EDID_BLOCK_LEN	128

/* DW-HDMI Controller >= 0x200a are at least compliant with SCDC version 1 */
#define SCDC_MIN_SOURCE_VERSION	0x1

#define HDMI14_MAX_TMDSCLK	340000000
#define HDMI20_MAX_TMDSCLK_KHZ	600000

#define HDMI_VH0		0x20
#define HDMI_HDCP_ADDR		0x3a
#define HDMI_BCAPS		0x40
#define HDMI_HDCP14_SUPPORT	BIT(7)
#define HDMI_HDCP2_VERSION	0x50
#define HDMI_HDCP2_SUPPORT	BIT(2)

#define SINK_CAP_HDCP14		BIT(0)
#define SINK_CAP_HDCP2		BIT(1)

#define HDMI_HDCP2_AUTH		BIT(1)
#define HDMI_HDCP14_AUTH	BIT(0)

#define HDMI_CTRL_CLK_EN	0x15

#define DW_HDMI_QP_RGB		0
#define DW_HDMI_QP_YUV422	0x1
#define DW_HDMI_QP_YUV444	0x2
#define DW_HDMI_QP_YUV420	0x3
#define DW_HDMI_QP_DSC		0xb

#define IPI_FORMAT_MASK		0xf
#define IPI_COLOR_DEPTH_MASK	0xf0

static const unsigned int dw_hdmi_cable[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

/*
 * Recommended N and Expected CTS Values in FRL Mode in chapter 9.2.2
 * of HDMI Specification 2.1.
 */
static const struct dw_hdmi_audio_frl_n common_frl_n_table[] = {
	{ .r_bit = 3,  .n_32k = 4224, .n_44k1 = 5292, .n_48k = 5760, },
	{ .r_bit = 6,  .n_32k = 4032, .n_44k1 = 5292, .n_48k = 6048, },
	{ .r_bit = 8,  .n_32k = 4032, .n_44k1 = 3969, .n_48k = 6048, },
	{ .r_bit = 10, .n_32k = 3456, .n_44k1 = 3969, .n_48k = 5184, },
	{ .r_bit = 12, .n_32k = 3072, .n_44k1 = 3969, .n_48k = 4752, },
};

/*
 * Unless otherwise noted, entries in this table are 100% optimization.
 * Values can be obtained from hdmi_compute_n() but that function is
 * slow so we pre-compute values we expect to see.
 *
 * All 32k and 48k values are expected to be the same (due to the way
 * the math works) for any rate that's an exact kHz.
 */
static const struct dw_hdmi_audio_tmds_n common_tmds_n_table[] = {
	{ .tmds = 25175000, .n_32k = 4096, .n_44k1 = 12854, .n_48k = 6144, },
	{ .tmds = 25200000, .n_32k = 4096, .n_44k1 = 5656, .n_48k = 6144, },
	{ .tmds = 27000000, .n_32k = 4096, .n_44k1 = 5488, .n_48k = 6144, },
	{ .tmds = 28320000, .n_32k = 4096, .n_44k1 = 5586, .n_48k = 6144, },
	{ .tmds = 30240000, .n_32k = 4096, .n_44k1 = 5642, .n_48k = 6144, },
	{ .tmds = 31500000, .n_32k = 4096, .n_44k1 = 5600, .n_48k = 6144, },
	{ .tmds = 32000000, .n_32k = 4096, .n_44k1 = 5733, .n_48k = 6144, },
	{ .tmds = 33750000, .n_32k = 4096, .n_44k1 = 6272, .n_48k = 6144, },
	{ .tmds = 36000000, .n_32k = 4096, .n_44k1 = 5684, .n_48k = 6144, },
	{ .tmds = 40000000, .n_32k = 4096, .n_44k1 = 5733, .n_48k = 6144, },
	{ .tmds = 49500000, .n_32k = 4096, .n_44k1 = 5488, .n_48k = 6144, },
	{ .tmds = 50000000, .n_32k = 4096, .n_44k1 = 5292, .n_48k = 6144, },
	{ .tmds = 54000000, .n_32k = 4096, .n_44k1 = 5684, .n_48k = 6144, },
	{ .tmds = 65000000, .n_32k = 4096, .n_44k1 = 7056, .n_48k = 6144, },
	{ .tmds = 68250000, .n_32k = 4096, .n_44k1 = 5376, .n_48k = 6144, },
	{ .tmds = 71000000, .n_32k = 4096, .n_44k1 = 7056, .n_48k = 6144, },
	{ .tmds = 72000000, .n_32k = 4096, .n_44k1 = 5635, .n_48k = 6144, },
	{ .tmds = 73250000, .n_32k = 4096, .n_44k1 = 14112, .n_48k = 6144, },
	{ .tmds = 74250000, .n_32k = 4096, .n_44k1 = 6272, .n_48k = 6144, },
	{ .tmds = 75000000, .n_32k = 4096, .n_44k1 = 5880, .n_48k = 6144, },
	{ .tmds = 78750000, .n_32k = 4096, .n_44k1 = 5600, .n_48k = 6144, },
	{ .tmds = 78800000, .n_32k = 4096, .n_44k1 = 5292, .n_48k = 6144, },
	{ .tmds = 79500000, .n_32k = 4096, .n_44k1 = 4704, .n_48k = 6144, },
	{ .tmds = 83500000, .n_32k = 4096, .n_44k1 = 7056, .n_48k = 6144, },
	{ .tmds = 85500000, .n_32k = 4096, .n_44k1 = 5488, .n_48k = 6144, },
	{ .tmds = 88750000, .n_32k = 4096, .n_44k1 = 14112, .n_48k = 6144, },
	{ .tmds = 97750000, .n_32k = 4096, .n_44k1 = 14112, .n_48k = 6144, },
	{ .tmds = 101000000, .n_32k = 4096, .n_44k1 = 7056, .n_48k = 6144, },
	{ .tmds = 106500000, .n_32k = 4096, .n_44k1 = 4704, .n_48k = 6144, },
	{ .tmds = 108000000, .n_32k = 4096, .n_44k1 = 5684, .n_48k = 6144, },
	{ .tmds = 115500000, .n_32k = 4096, .n_44k1 = 5712, .n_48k = 6144, },
	{ .tmds = 119000000, .n_32k = 4096, .n_44k1 = 5544, .n_48k = 6144, },
	{ .tmds = 135000000, .n_32k = 4096, .n_44k1 = 5488, .n_48k = 6144, },
	{ .tmds = 146250000, .n_32k = 4096, .n_44k1 = 6272, .n_48k = 6144, },
	{ .tmds = 148500000, .n_32k = 4096, .n_44k1 = 5488, .n_48k = 6144, },
	{ .tmds = 154000000, .n_32k = 4096, .n_44k1 = 5544, .n_48k = 6144, },
	{ .tmds = 162000000, .n_32k = 4096, .n_44k1 = 5684, .n_48k = 6144, },

	/* For 297 MHz+ HDMI spec have some other rule for setting N */
	{ .tmds = 297000000, .n_32k = 3073, .n_44k1 = 4704, .n_48k = 5120, },
	{ .tmds = 594000000, .n_32k = 3073, .n_44k1 = 9408, .n_48k = 10240, },

	/* End of table */
	{ .tmds = 0,         .n_32k = 0,    .n_44k1 = 0,    .n_48k = 0, },
};

static const struct drm_display_mode dw_hdmi_default_modes[] = {
	/* 16 - 1920x1080@60Hz 16:9 */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 2 - 720x480@60Hz 4:3 */
	{ DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		   798, 858, 0, 480, 489, 495, 525, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
	/* 4 - 1280x720@60Hz 16:9 */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 31 - 1920x1080@50Hz 16:9 */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
		   2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 19 - 1280x720@50Hz 16:9 */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1720,
		   1760, 1980, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, },
	/* 17 - 720x576@50Hz 4:3 */
	{ DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		   796, 864, 0, 576, 581, 586, 625, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
	/* 2 - 720x480@60Hz 4:3 */
	{ DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		   798, 858, 0, 480, 489, 495, 525, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
};

enum quirk_case {
	VSI_SEND,
	DELAY,
};

static const struct hdmi_quirk {
	u8 vendor_info[VENDOR_INFO_LEN];
	u8 quirk_case;
	u32 delay;
} hdmi_quirk_list[] = {
	/* XMD-004A-00000001-46-2019 */
	{ { 0x61, 0xA4, 0x4A, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2E, 0x1D, },
	  BIT(VSI_SEND), 0, },
};

enum frl_mask {
	FRL_3GBPS_3LANE = 1,
	FRL_6GBPS_3LANE,
	FRL_6GBPS_4LANE,
	FRL_8GBPS_4LANE,
	FRL_10GBPS_4LANE,
	FRL_12GBPS_4LANE,
};

struct hdmi_vmode_qp {
	bool mdataenablepolarity;

	unsigned int previous_pixelclock;
	unsigned long mpixelclock;
	unsigned int mpixelrepetitioninput;
	unsigned int mpixelrepetitionoutput;
	unsigned long previous_tmdsclock;
	unsigned int mtmdsclock;
};

struct hdmi_qp_data_info {
	unsigned int enc_in_bus_format;
	unsigned int enc_out_bus_format;
	unsigned int enc_in_encoding;
	unsigned int enc_out_encoding;
	unsigned int quant_range;
	unsigned int pix_repet_factor;
	struct hdmi_vmode_qp video_mode;
	bool update;
};

struct dw_hdmi_qp_i2c {
	struct i2c_adapter	adap;

	struct mutex		lock;	/* used to serialize data transfers */
	struct completion	cmp;
	u32			stat;

	u8			slave_reg;
	bool			is_regaddr;
	bool			is_segment;

	unsigned int		scl_high_ns;
	unsigned int		scl_low_ns;
};

struct dw_hdmi_phy_data {
	enum dw_hdmi_phy_type type;
	const char *name;
	unsigned int gen;
	bool has_svsret;
	int (*configure)(struct dw_hdmi_qp *hdmi,
			 const struct dw_hdmi_plat_data *pdata,
			 unsigned long mpixelclock);
};

struct dw_hdmi_qp {
	struct drm_connector connector;
	struct drm_bridge bridge;
	struct drm_bridge *next_bridge;
	struct drm_panel *panel;
	struct platform_device *hdcp_dev;
	struct platform_device *audio;
	struct platform_device *cec;
	struct device *dev;
	struct dw_hdmi_qp_i2c *i2c;

	struct hdmi_qp_data_info hdmi_data;
	struct dw_hdmi_qp_cec_data cec_data;
	const struct dw_hdmi_plat_data *plat_data;
	struct dw_qp_hdcp *hdcp;
	int vic;
	int main_irq;
	int avp_irq;
	int earc_irq;

	u8 edid[HDMI_EDID_LEN];
	u8 vendor_info[VENDOR_INFO_LEN];
	u32 dovi_vsif[DOVI_VSIF_LEN];

	struct {
		const struct dw_hdmi_qp_phy_ops *ops;
		const char *name;
		void *data;
		bool enabled;
	} phy;

	struct drm_display_mode previous_mode;

	struct i2c_adapter *ddc;
	void __iomem *regs;
	void __iomem *hdcp14_mem;
	bool sink_is_hdmi;
	bool sink_has_audio;
	bool dclk_en;
	bool frl_switch;		/* when frl mode switch color and freq is equal set true */
	bool cec_enable;
	bool allm_enable;
	bool support_hdmi;
	bool skip_connector;
	bool force_kernel_output;	/* force kernel hdmi output specific resolution */
	int force_output;		/* force hdmi/dvi output mode */
	int vp_id;
	int old_vp_id;

	struct mutex mutex;		/* for state below and previous_mode */
	struct drm_connector *curr_conn;/* current connector (only valid when !disabled) */
	enum drm_connector_force force;	/* mutex-protected force state */
	bool disabled;			/* DRM has disabled our bridge */
	bool bridge_is_on;		/* indicates the bridge is on */
	bool rxsense;			/* rxsense state */
	u8 phy_mask;			/* desired phy int mask settings */
	u8 mc_clkdis;			/* clock disable register */
	u8 hdcp_status;
	u32 max_ffe_lv;

	bool update;
	bool hdr2sdr;
	bool flt_no_timeout;

	u32 scdc_intr;
	u32 flt_intr;
	u32 earc_intr;

	u32 refclk_rate;

	struct mutex audio_mutex;
	unsigned int sample_rate;
	unsigned int audio_cts;
	unsigned int audio_n;
	bool audio_enable;
	void (*enable_audio)(struct dw_hdmi_qp *hdmi);
	void (*disable_audio)(struct dw_hdmi_qp *hdmi);

	struct dentry *debugfs_dir;
	bool scramble_low_rates;

	struct extcon_dev *extcon;

	struct regmap *regm;

	bool initialized;		/* hdmi is enabled before bind */
	bool logo_plug_out;             /* hdmi is plug out when kernel logo */
	struct completion flt_cmp;
	struct completion earc_cmp;

	struct cec_notifier *cec_notifier;
	struct cec_adapter *cec_adap;
	struct mutex cec_notifier_mutex;

	hdmi_codec_plugged_cb plugged_cb;
	struct device *codec_dev;
	enum drm_connector_status last_connector_result;

	struct work_struct flt_work;
	struct workqueue_struct *workqueue;
};

static inline void hdmi_writel(struct dw_hdmi_qp *hdmi, u32 val, int offset)
{
	regmap_write(hdmi->regm, offset, val);
}

static inline u32 hdmi_readl(struct dw_hdmi_qp *hdmi, int offset)
{
	unsigned int val = 0;

	regmap_read(hdmi->regm, offset, &val);

	return val;
}

static void handle_plugged_change(struct dw_hdmi_qp *hdmi, bool plugged)
{
	if (hdmi->plugged_cb && hdmi->codec_dev)
		hdmi->plugged_cb(hdmi->codec_dev, plugged);
}

int dw_hdmi_qp_set_plugged_cb(struct dw_hdmi_qp *hdmi, hdmi_codec_plugged_cb fn,
			      struct device *codec_dev)
{
	bool plugged;

	mutex_lock(&hdmi->mutex);
	hdmi->plugged_cb = fn;
	hdmi->codec_dev = codec_dev;
	plugged = hdmi->last_connector_result == connector_status_connected;
	handle_plugged_change(hdmi, plugged);
	mutex_unlock(&hdmi->mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_plugged_cb);

static void hdmi_modb(struct dw_hdmi_qp *hdmi, u32 data, u32 mask, u32 reg)
{
	regmap_update_bits(hdmi->regm, reg, mask, data);
}

static void hdmi_set_cts_n(struct dw_hdmi_qp *hdmi, unsigned int cts,
			   unsigned int n)
{
	/* Set N */
	hdmi_modb(hdmi, n, AUDPKT_ACR_N_VALUE, AUDPKT_ACR_CONTROL0);

	/* Set CTS */
	if (cts)
		hdmi_modb(hdmi, AUDPKT_ACR_CTS_OVR_EN, AUDPKT_ACR_CTS_OVR_EN_MSK,
			  AUDPKT_ACR_CONTROL1);
	else
		hdmi_modb(hdmi, 0, AUDPKT_ACR_CTS_OVR_EN_MSK,
			  AUDPKT_ACR_CONTROL1);

	hdmi_modb(hdmi, AUDPKT_ACR_CTS_OVR_VAL(cts), AUDPKT_ACR_CTS_OVR_VAL_MSK,
		  AUDPKT_ACR_CONTROL1);
}

static int hdmi_match_frl_n_table(struct dw_hdmi_qp *hdmi,
				  unsigned long r_bit,
				  unsigned long freq)
{
	const struct dw_hdmi_audio_frl_n *frl_n = NULL;
	int i = 0, n = 0;

	for (i = 0; ARRAY_SIZE(common_frl_n_table); i++) {
		if (r_bit == common_frl_n_table[i].r_bit) {
			frl_n = &common_frl_n_table[i];
			break;
		}
	}

	if (!frl_n)
		goto err;

	switch (freq) {
	case 32000:
	case 64000:
	case 128000:
		n = (freq / 32000) * frl_n->n_32k;
		break;
	case 44100:
	case 88200:
	case 176400:
		n = (freq / 44100) * frl_n->n_44k1;
		break;
	case 48000:
	case 96000:
	case 192000:
		n = (freq / 48000) * frl_n->n_48k;
		break;
	default:
		goto err;
	}

	return n;
err:
	dev_err(hdmi->dev, "FRL; unexpected Rbit: %lu Gbps\n", r_bit);

	return 0;
}

static int hdmi_match_tmds_n_table(struct dw_hdmi_qp *hdmi,
				   unsigned long pixel_clk,
				   unsigned long freq)
{
	const struct dw_hdmi_plat_data *plat_data = hdmi->plat_data;
	const struct dw_hdmi_audio_tmds_n *tmds_n = NULL;
	int i;

	if (plat_data->tmds_n_table) {
		for (i = 0; plat_data->tmds_n_table[i].tmds != 0; i++) {
			if (pixel_clk == plat_data->tmds_n_table[i].tmds) {
				tmds_n = &plat_data->tmds_n_table[i];
				break;
			}
		}
	}

	if (tmds_n == NULL) {
		for (i = 0; common_tmds_n_table[i].tmds != 0; i++) {
			if (pixel_clk == common_tmds_n_table[i].tmds) {
				tmds_n = &common_tmds_n_table[i];
				break;
			}
		}
	}

	if (tmds_n == NULL)
		return -ENOENT;

	switch (freq) {
	case 32000:
		return tmds_n->n_32k;
	case 44100:
	case 88200:
	case 176400:
		return (freq / 44100) * tmds_n->n_44k1;
	case 48000:
	case 96000:
	case 192000:
		return (freq / 48000) * tmds_n->n_48k;
	default:
		return -ENOENT;
	}
}

static u64 hdmi_audio_math_diff(unsigned int freq, unsigned int n,
				unsigned int pixel_clk)
{
	u64 final, diff;
	u64 cts;

	final = (u64)pixel_clk * n;

	cts = final;
	do_div(cts, 128 * freq);

	diff = final - (u64)cts * (128 * freq);

	return diff;
}

static unsigned int hdmi_compute_n(struct dw_hdmi_qp *hdmi,
				   unsigned long pixel_clk,
				   unsigned long freq)
{
	unsigned int min_n = DIV_ROUND_UP((128 * freq), 1500);
	unsigned int max_n = (128 * freq) / 300;
	unsigned int ideal_n = (128 * freq) / 1000;
	unsigned int best_n_distance = ideal_n;
	unsigned int best_n = 0;
	u64 best_diff = U64_MAX;
	int n;

	/* If the ideal N could satisfy the audio math, then just take it */
	if (hdmi_audio_math_diff(freq, ideal_n, pixel_clk) == 0)
		return ideal_n;

	for (n = min_n; n <= max_n; n++) {
		u64 diff = hdmi_audio_math_diff(freq, n, pixel_clk);

		if (diff < best_diff || (diff == best_diff &&
		    abs(n - ideal_n) < best_n_distance)) {
			best_n = n;
			best_diff = diff;
			best_n_distance = abs(best_n - ideal_n);
		}

		/*
		 * The best N already satisfy the audio math, and also be
		 * the closest value to ideal N, so just cut the loop.
		 */
		if ((best_diff == 0) && (abs(n - ideal_n) > best_n_distance))
			break;
	}

	return best_n;
}

static unsigned int hdmi_find_n(struct dw_hdmi_qp *hdmi, unsigned long pixel_clk,
				unsigned long sample_rate)
{
	struct dw_hdmi_link_config *link_cfg = NULL;
	void *data = hdmi->plat_data->phy_data;
	int n;

	if (hdmi->plat_data->get_link_cfg) {
		link_cfg = hdmi->plat_data->get_link_cfg(data);
		if (link_cfg && link_cfg->frl_mode)
			return hdmi_match_frl_n_table(hdmi, link_cfg->rate_per_lane, sample_rate);
	}

	n = hdmi_match_tmds_n_table(hdmi, pixel_clk, sample_rate);
	if (n > 0)
		return n;

	dev_warn(hdmi->dev, "Rate %lu missing; compute N dynamically\n",
		 pixel_clk);

	return hdmi_compute_n(hdmi, pixel_clk, sample_rate);
}

void dw_hdmi_qp_set_audio_interface(struct dw_hdmi_qp *hdmi,
				    struct hdmi_codec_daifmt *fmt,
				    struct hdmi_codec_params *hparms)
{
	u32 conf0 = 0;

	mutex_lock(&hdmi->audio_mutex);
	if (!hdmi->dclk_en) {
		mutex_unlock(&hdmi->audio_mutex);
		return;
	}

	/* Reset the audio data path of the AVP */
	hdmi_writel(hdmi, AVP_DATAPATH_PACKET_AUDIO_SWINIT_P, GLOBAL_SWRESET_REQUEST);

	/* Disable AUDS, ACR, AUDI */
	hdmi_modb(hdmi, 0,
		  PKTSCHED_ACR_TX_EN | PKTSCHED_AUDS_TX_EN | PKTSCHED_AUDI_TX_EN,
		  PKTSCHED_PKT_EN);

	/* Clear the audio FIFO */
	hdmi_writel(hdmi, AUDIO_FIFO_CLR_P, AUDIO_INTERFACE_CONTROL0);

	/* Select I2S interface as the audio source */
	hdmi_modb(hdmi, AUD_IF_I2S, AUD_IF_SEL_MSK, AUDIO_INTERFACE_CONFIG0);

	/* Enable the active i2s lanes */
	switch (hparms->channels) {
	case 7 ... 8:
		conf0 |= I2S_LINES_EN(3);
		fallthrough;
	case 5 ... 6:
		conf0 |= I2S_LINES_EN(2);
		fallthrough;
	case 3 ... 4:
		conf0 |= I2S_LINES_EN(1);
		fallthrough;
	default:
		conf0 |= I2S_LINES_EN(0);
		break;
	}

	hdmi_modb(hdmi, conf0, I2S_LINES_EN_MSK, AUDIO_INTERFACE_CONFIG0);

	/*
	 * Enable bpcuv generated internally for L-PCM, or received
	 * from stream for NLPCM/HBR.
	 */
	switch (fmt->bit_fmt) {
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE:
		conf0 = (hparms->channels == 8) ? AUD_HBR : AUD_ASP;
		conf0 |= I2S_BPCUV_RCV_EN;
		break;
	default:
		conf0 = AUD_ASP | I2S_BPCUV_RCV_DIS;
		break;
	}

	hdmi_modb(hdmi, conf0, I2S_BPCUV_RCV_MSK | AUD_FORMAT_MSK,
		  AUDIO_INTERFACE_CONFIG0);

	/* Enable audio FIFO auto clear when overflow */
	hdmi_modb(hdmi, AUD_FIFO_INIT_ON_OVF_EN, AUD_FIFO_INIT_ON_OVF_MSK,
		  AUDIO_INTERFACE_CONFIG0);

	mutex_unlock(&hdmi->audio_mutex);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_audio_interface);

/*
 * When transmitting IEC60958 linear PCM audio, these registers allow to
 * configure the channel status information of all the channel status
 * bits in the IEC60958 frame. For the moment this configuration is only
 * used when the I2S audio interface, General Purpose Audio (GPA),
 * or AHB audio DMA (AHBAUDDMA) interface is active
 * (for S/PDIF interface this information comes from the stream).
 */
void dw_hdmi_qp_set_channel_status(struct dw_hdmi_qp *hdmi,
				   u8 *channel_status, bool ref2stream)
{
	mutex_lock(&hdmi->audio_mutex);
	if (!hdmi->dclk_en) {
		mutex_unlock(&hdmi->audio_mutex);
		return;
	}

	/*
	 * AUDPKT_CHSTATUS_OVR0: { RSV, RSV, CS1, CS0 }
	 * AUDPKT_CHSTATUS_OVR1: { CS6, CS5, CS4, CS3 }
	 *
	 *      |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
	 * CS0: |   Mode    |        d        |  c  |  b  |  a  |
	 * CS1: |               Category Code                   |
	 * CS2: |    Channel Number     |     Source Number     |
	 * CS3: |    Clock Accuracy     |     Sample Freq       |
	 * CS4: |    Ori Sample Freq    |     Word Length       |
	 * CS5: |                                   |   CGMS-A  |
	 * CS6~CS23: Reserved
	 *
	 * a: use of channel status block
	 * b: linear PCM identification: 0 for lpcm, 1 for nlpcm
	 * c: copyright information
	 * d: additional format information
	 */

	if (ref2stream)
		channel_status[0] |= IEC958_AES0_NONAUDIO;

	if ((hdmi_readl(hdmi, AUDIO_INTERFACE_CONFIG0) & GENMASK(25, 24)) == AUD_HBR) {
		/* fixup cs for HBR */
		channel_status[3] = (channel_status[3] & 0xf0) | IEC958_AES3_CON_FS_768000;
		channel_status[4] = (channel_status[4] & 0x0f) | IEC958_AES4_CON_ORIGFS_NOTID;
	}

	hdmi_writel(hdmi, channel_status[0] | (channel_status[1] << 8),
		    AUDPKT_CHSTATUS_OVR0);

	regmap_bulk_write(hdmi->regm, AUDPKT_CHSTATUS_OVR1, &channel_status[3], 1);

	if (ref2stream)
		hdmi_modb(hdmi, 0,
			  AUDPKT_PBIT_FORCE_EN_MASK | AUDPKT_CHSTATUS_OVR_EN_MASK,
			  AUDPKT_CONTROL0);
	else
		hdmi_modb(hdmi, AUDPKT_PBIT_FORCE_EN | AUDPKT_CHSTATUS_OVR_EN,
			  AUDPKT_PBIT_FORCE_EN_MASK | AUDPKT_CHSTATUS_OVR_EN_MASK,
			  AUDPKT_CONTROL0);

	mutex_unlock(&hdmi->audio_mutex);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_channel_status);

static void hdmi_set_clk_regenerator(struct dw_hdmi_qp *hdmi,
	unsigned long pixel_clk, unsigned int sample_rate)
{
	unsigned int n = 0, cts = 0;

	n = hdmi_find_n(hdmi, pixel_clk, sample_rate);

	hdmi->audio_n = n;
	hdmi->audio_cts = cts;
	hdmi_set_cts_n(hdmi, cts, hdmi->audio_enable ? n : 0);
}

static void hdmi_init_clk_regenerator(struct dw_hdmi_qp *hdmi)
{
	mutex_lock(&hdmi->audio_mutex);
	if (hdmi->dclk_en)
		hdmi_set_clk_regenerator(hdmi, 74250000, hdmi->sample_rate);
	mutex_unlock(&hdmi->audio_mutex);
}

static void hdmi_clk_regenerator_update_pixel_clock(struct dw_hdmi_qp *hdmi)
{
	mutex_lock(&hdmi->audio_mutex);
	if (hdmi->dclk_en)
		hdmi_set_clk_regenerator(hdmi, hdmi->hdmi_data.video_mode.mtmdsclock,
					 hdmi->sample_rate);
	mutex_unlock(&hdmi->audio_mutex);
}

void dw_hdmi_qp_set_sample_rate(struct dw_hdmi_qp *hdmi, unsigned int rate)
{
	mutex_lock(&hdmi->audio_mutex);
	if (hdmi->dclk_en) {
		hdmi->sample_rate = rate;
		hdmi_set_clk_regenerator(hdmi, hdmi->hdmi_data.video_mode.mtmdsclock,
					 hdmi->sample_rate);
	}
	mutex_unlock(&hdmi->audio_mutex);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_sample_rate);

void dw_hdmi_qp_set_channel_count(struct dw_hdmi_qp *hdmi, unsigned int cnt)
{
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_channel_count);

void dw_hdmi_qp_set_channel_allocation(struct dw_hdmi_qp *hdmi, unsigned int ca)
{
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_channel_allocation);

static int dw_hdmi_qp_init_audio_infoframe(struct dw_hdmi_qp *hdmi)
{
	struct hdmi_audio_infoframe frame;
	u8 infoframe_buf[HDMI_INFOFRAME_SIZE(AUDIO)];
	int ret = 0;

	hdmi_audio_infoframe_init(&frame);

	frame.coding_type = HDMI_AUDIO_CODING_TYPE_STREAM;
	frame.sample_frequency = HDMI_AUDIO_SAMPLE_FREQUENCY_STREAM;
	frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_STREAM;
	frame.channels = 2;

	ret = hdmi_audio_infoframe_pack(&frame, infoframe_buf,
					sizeof(infoframe_buf));
	if (ret < 0) {
		dev_err(hdmi->dev, "%s: Failed to pack audio infoframe: %d\n",
			__func__, ret);
		return ret;
	}

	regmap_bulk_write(hdmi->regm, PKT_AUDI_CONTENTS1, &infoframe_buf[3], 2);
	hdmi_modb(hdmi,
		  PKTSCHED_ACR_TX_EN | PKTSCHED_AUDI_TX_EN,
		  PKTSCHED_ACR_TX_EN | PKTSCHED_AUDI_TX_EN,
		  PKTSCHED_PKT_EN);

	return 0;
}

void dw_hdmi_qp_set_audio_infoframe(struct dw_hdmi_qp *hdmi,
				    struct hdmi_codec_params *hparms)
{
	u8 infoframe_buf[HDMI_INFOFRAME_SIZE(AUDIO)];
	int ret = 0;

	ret = hdmi_audio_infoframe_pack(&hparms->cea, infoframe_buf,
					sizeof(infoframe_buf));
	if (!ret) {
		dev_err(hdmi->dev, "%s: Failed to pack audio infoframe: %d\n",
			__func__, ret);
		return;
	}

	mutex_lock(&hdmi->audio_mutex);
	if (!hdmi->dclk_en) {
		mutex_unlock(&hdmi->audio_mutex);
		return;
	}

	/*
	 * AUDI_CONTENTS0: { RSV, HB2, HB1, RSV }
	 * AUDI_CONTENTS1: { PB3, PB2, PB1, PB0 }
	 * AUDI_CONTENTS2: { PB7, PB6, PB5, PB4 }
	 *
	 * PB0: CheckSum
	 * PB1: | CT3    | CT2  | CT1  | CT0  | F13  | CC2 | CC1 | CC0 |
	 * PB2: | F27    | F26  | F25  | SF2  | SF1  | SF0 | SS1 | SS0 |
	 * PB3: | F37    | F36  | F35  | F34  | F33  | F32 | F31 | F30 |
	 * PB4: | CA7    | CA6  | CA5  | CA4  | CA3  | CA2 | CA1 | CA0 |
	 * PB5: | DM_INH | LSV3 | LSV2 | LSV1 | LSV0 | F52 | F51 | F50 |
	 * PB6~PB10: Reserved
	 *
	 * AUDI_CONTENTS0 default value defined by HDMI specification,
	 * and shall only be changed for debug purposes.
	 * So, we only configure payload byte from PB0~PB7(2 word total).
	 */
	regmap_bulk_write(hdmi->regm, PKT_AUDI_CONTENTS1, &infoframe_buf[3], 2);

	/* Enable ACR, AUDI */
	hdmi_modb(hdmi, PKTSCHED_ACR_TX_EN | PKTSCHED_AUDI_TX_EN,
		  PKTSCHED_ACR_TX_EN | PKTSCHED_AUDI_TX_EN,
		  PKTSCHED_PKT_EN);

	/* Enable AUDS */
	hdmi_modb(hdmi, PKTSCHED_AUDS_TX_EN, PKTSCHED_AUDS_TX_EN, PKTSCHED_PKT_EN);
	mutex_unlock(&hdmi->audio_mutex);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_audio_infoframe);

static void hdmi_enable_audio_clk(struct dw_hdmi_qp *hdmi, bool enable)
{
	if (enable)
		hdmi_modb(hdmi, 0,
			  AVP_DATAPATH_PACKET_AUDIO_SWDISABLE, GLOBAL_SWDISABLE);
	else
		hdmi_modb(hdmi, AVP_DATAPATH_PACKET_AUDIO_SWDISABLE,
			  AVP_DATAPATH_PACKET_AUDIO_SWDISABLE, GLOBAL_SWDISABLE);
}

static void dw_hdmi_i2s_audio_enable(struct dw_hdmi_qp *hdmi)
{
	hdmi_set_cts_n(hdmi, hdmi->audio_cts, hdmi->audio_n);
	hdmi_enable_audio_clk(hdmi, true);
}

static void dw_hdmi_i2s_audio_disable(struct dw_hdmi_qp *hdmi)
{
	/*
	 * Keep ACR, AUDI, AUDS packet always on to make SINK device
	 * active for better compatibility and user experience.
	 *
	 * This also fix POP sound on some SINK devices which wakeup
	 * from suspend to active.
	 */
	hdmi_modb(hdmi, I2S_BPCUV_RCV_DIS, I2S_BPCUV_RCV_MSK,
		  AUDIO_INTERFACE_CONFIG0);
	hdmi_modb(hdmi, AUDPKT_PBIT_FORCE_EN | AUDPKT_CHSTATUS_OVR_EN,
		  AUDPKT_PBIT_FORCE_EN_MASK | AUDPKT_CHSTATUS_OVR_EN_MASK,
		  AUDPKT_CONTROL0);
}

void dw_hdmi_qp_audio_enable(struct dw_hdmi_qp *hdmi)
{
	mutex_lock(&hdmi->audio_mutex);
	if (hdmi->dclk_en) {
		hdmi->audio_enable = true;
		if (hdmi->enable_audio)
			hdmi->enable_audio(hdmi);
	}
	mutex_unlock(&hdmi->audio_mutex);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_audio_enable);

void dw_hdmi_qp_audio_disable(struct dw_hdmi_qp *hdmi)
{
	mutex_lock(&hdmi->audio_mutex);
	if (hdmi->dclk_en) {
		hdmi->audio_enable = false;
		if (hdmi->disable_audio)
			hdmi->disable_audio(hdmi);
	}
	mutex_unlock(&hdmi->audio_mutex);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_audio_disable);

static bool hdmi_bus_fmt_is_rgb(unsigned int bus_format)
{
	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB101010_1X30:
	case MEDIA_BUS_FMT_RGB121212_1X36:
	case MEDIA_BUS_FMT_RGB161616_1X48:
		return true;

	default:
		return false;
	}
}

static bool hdmi_bus_fmt_is_yuv444(unsigned int bus_format)
{
	switch (bus_format) {
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_YUV10_1X30:
	case MEDIA_BUS_FMT_YUV12_1X36:
	case MEDIA_BUS_FMT_YUV16_1X48:
		return true;

	default:
		return false;
	}
}

static bool hdmi_bus_fmt_is_yuv422(unsigned int bus_format)
{
	switch (bus_format) {
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_UYVY10_1X20:
	case MEDIA_BUS_FMT_UYVY12_1X24:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YUYV10_1X20:
	case MEDIA_BUS_FMT_YUYV12_1X24:
		return true;

	default:
		return false;
	}
}

static bool hdmi_bus_fmt_is_yuv420(unsigned int bus_format)
{
	switch (bus_format) {
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
	case MEDIA_BUS_FMT_UYYVYY12_0_5X36:
	case MEDIA_BUS_FMT_UYYVYY16_0_5X48:
		return true;

	default:
		return false;
	}
}

static int hdmi_bus_fmt_color_depth(unsigned int bus_format)
{
	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
		return 8;

	case MEDIA_BUS_FMT_RGB101010_1X30:
	case MEDIA_BUS_FMT_YUV10_1X30:
	case MEDIA_BUS_FMT_UYVY10_1X20:
	case MEDIA_BUS_FMT_YUYV10_1X20:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
		return 10;

	case MEDIA_BUS_FMT_RGB121212_1X36:
	case MEDIA_BUS_FMT_YUV12_1X36:
	case MEDIA_BUS_FMT_UYVY12_1X24:
	case MEDIA_BUS_FMT_YUYV12_1X24:
	case MEDIA_BUS_FMT_UYYVYY12_0_5X36:
		return 12;

	case MEDIA_BUS_FMT_RGB161616_1X48:
	case MEDIA_BUS_FMT_YUV16_1X48:
	case MEDIA_BUS_FMT_UYYVYY16_0_5X48:
		return 16;

	default:
		return 0;
	}
}

static const struct hdmi_quirk *get_hdmi_quirk(u8 *vendor_id)
{
	int i;

	if (!vendor_id)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(hdmi_quirk_list); i++) {
		if (!memcmp(vendor_id, hdmi_quirk_list[i].vendor_info, VENDOR_INFO_LEN))
			return &hdmi_quirk_list[i];
	}

	return NULL;
}

static void dw_hdmi_i2c_init(struct dw_hdmi_qp *hdmi)
{
	u64 scl_high_cnt, scl_low_cnt, val;

	scl_high_cnt = hdmi->i2c->scl_high_ns;
	scl_low_cnt = hdmi->i2c->scl_low_ns;

	scl_high_cnt = scl_high_cnt * hdmi->refclk_rate;
	scl_high_cnt = DIV_ROUND_CLOSEST_ULL(scl_high_cnt, 1000000000);

	scl_low_cnt = scl_low_cnt * hdmi->refclk_rate;
	scl_low_cnt = DIV_ROUND_CLOSEST_ULL(scl_low_cnt, 1000000000);

	val = (scl_high_cnt & 0xffff) << 16 | (scl_low_cnt & 0xffff);

	/* Software reset */
	hdmi_writel(hdmi, 0x01, I2CM_CONTROL0);

	hdmi_writel(hdmi, val, I2CM_SM_SCL_CONFIG0);
	hdmi_modb(hdmi, 0, I2CM_FM_EN, I2CM_INTERFACE_CONTROL0);

	/* Clear DONE and ERROR interrupts */
	hdmi_writel(hdmi, I2CM_OP_DONE_CLEAR | I2CM_NACK_RCVD_CLEAR,
		    MAINUNIT_1_INT_CLEAR);
}

static int dw_hdmi_i2c_read(struct dw_hdmi_qp *hdmi,
			    unsigned char *buf, unsigned int length)
{
	struct dw_hdmi_qp_i2c *i2c = hdmi->i2c;
	int stat, retry;
	bool read_edid = false;

	if (!i2c->is_regaddr) {
		dev_dbg(hdmi->dev, "set read register address to 0\n");
		i2c->slave_reg = 0x00;
		i2c->is_regaddr = true;
	}

	/* edid reads are in 128 bytes. scdc reads are in 1 byte */
	if (length == HDMI_EDID_BLOCK_LEN)
		read_edid = true;

	while (length > 0) {
		retry = 100;
		hdmi_modb(hdmi, i2c->slave_reg << 12, I2CM_ADDR,
			  I2CM_INTERFACE_CONTROL0);

		if (read_edid) {
			hdmi_modb(hdmi, I2CM_16BYTES, I2CM_NBYTES_MASK,
				  I2CM_INTERFACE_CONTROL0);
			i2c->slave_reg += 16;
			length -= 16;
		} else {
			hdmi_modb(hdmi, I2CM_1BYTES, I2CM_NBYTES_MASK,
				  I2CM_INTERFACE_CONTROL0);
			i2c->slave_reg++;
			length--;
		}

		while (retry > 0) {
			if (hdmi->phy.ops->read_hpd(hdmi, hdmi->phy.data) !=
			    connector_status_connected) {
				dev_dbg(hdmi->dev, "hdmi disconnect, stop ddc read\n");
				return -EPERM;
			}

			reinit_completion(&i2c->cmp);

			if (i2c->is_segment)
				hdmi_modb(hdmi, I2CM_EXT_READ, I2CM_WR_MASK,
					  I2CM_INTERFACE_CONTROL0);
			else
				hdmi_modb(hdmi, I2CM_FM_READ, I2CM_WR_MASK,
					  I2CM_INTERFACE_CONTROL0);

			stat = wait_for_completion_timeout(&i2c->cmp, HZ / 10);
			if (!stat) {
				dev_err(hdmi->dev, "i2c read time out!\n");
				hdmi_writel(hdmi, 0x01, I2CM_CONTROL0);
				hdmi_modb(hdmi, 0, I2CM_WR_MASK, I2CM_INTERFACE_CONTROL0);
				retry -= 10;
				continue;
			}

			/* Check for error condition on the bus */
			if (i2c->stat & I2CM_NACK_RCVD_IRQ) {
				dev_err(hdmi->dev, "i2c read err!\n");
				hdmi_writel(hdmi, 0x01, I2CM_CONTROL0);
				hdmi_modb(hdmi, 0, I2CM_WR_MASK, I2CM_INTERFACE_CONTROL0);
				retry--;
				usleep_range(10000, 11000);
				continue;
			}
			/* read success */
			break;
		}

		if (retry <= 0) {
			dev_err(hdmi->dev, "ddc read failed offset:0x%x\n", i2c->slave_reg);
			return -EIO;
		}

		if (read_edid) {
			u8 reg_offset, val_offset, i;
			u32 val;

			for (i = 0; i < 16; i++) {
				reg_offset = i / 4;
				val_offset = (i % 4) * 8;
				val = hdmi_readl(hdmi, I2CM_INTERFACE_RDDATA_0_3 + 4 * reg_offset);
				*buf++ = (val & (0xff << val_offset)) >> val_offset;
				dev_dbg(hdmi->dev, "i2c read done! i2c->stat:%02x 0x%02x\n",
					i2c->stat, (val & (0xff << val_offset)) >> val_offset);
			}
		} else {
			*buf++ = hdmi_readl(hdmi, I2CM_INTERFACE_RDDATA_0_3) & 0xff;
			dev_dbg(hdmi->dev, "i2c read done! i2c->stat:%02x 0x%02x\n",
				i2c->stat, hdmi_readl(hdmi, I2CM_INTERFACE_RDDATA_0_3));
		}

		hdmi_modb(hdmi, 0, I2CM_WR_MASK, I2CM_INTERFACE_CONTROL0);
	}
	i2c->is_segment = false;

	return 0;
}

static int dw_hdmi_i2c_write(struct dw_hdmi_qp *hdmi,
			     unsigned char *buf, unsigned int length)
{
	struct dw_hdmi_qp_i2c *i2c = hdmi->i2c;
	int stat, retry;

	if (!i2c->is_regaddr) {
		/* Use the first write byte as register address */
		i2c->slave_reg = buf[0];
		length--;
		buf++;
		i2c->is_regaddr = true;
	}

	while (length--) {
		retry = 100;

		while (retry > 0) {
			if (hdmi->phy.ops->read_hpd(hdmi, hdmi->phy.data) !=
			    connector_status_connected) {
				dev_dbg(hdmi->dev, "hdmi disconnect, stop ddc write\n");
				return -EPERM;
			}

			reinit_completion(&i2c->cmp);

			hdmi_writel(hdmi, *buf, I2CM_INTERFACE_WRDATA_0_3);
			hdmi_modb(hdmi, i2c->slave_reg++ << 12, I2CM_ADDR,
				I2CM_INTERFACE_CONTROL0);
			hdmi_modb(hdmi, I2CM_FM_WRITE, I2CM_WR_MASK,
				I2CM_INTERFACE_CONTROL0);

			stat = wait_for_completion_timeout(&i2c->cmp, HZ / 10);
			if (!stat) {
				dev_err(hdmi->dev, "i2c write time out!\n");
				hdmi_writel(hdmi, 0x01, I2CM_CONTROL0);
				hdmi_modb(hdmi, 0, I2CM_WR_MASK, I2CM_INTERFACE_CONTROL0);
				retry -= 10;
				continue;
			}

			/* Check for error condition on the bus */
			if (i2c->stat & I2CM_NACK_RCVD_IRQ) {
				dev_err(hdmi->dev, "i2c write nack!\n");
				hdmi_writel(hdmi, 0x01, I2CM_CONTROL0);
				hdmi_modb(hdmi, 0, I2CM_WR_MASK, I2CM_INTERFACE_CONTROL0);
				retry--;
				usleep_range(10000, 11000);
				continue;
			}
			/* write success */
			buf++;
			break;
		}
		hdmi_modb(hdmi, 0, I2CM_WR_MASK, I2CM_INTERFACE_CONTROL0);

		if (retry <= 0) {
			dev_err(hdmi->dev, "ddc write failed\n");
			return -EIO;
		}
	}
	dev_dbg(hdmi->dev, "i2c write done!\n");
	return 0;
}

static int dw_hdmi_i2c_xfer(struct i2c_adapter *adap,
			    struct i2c_msg *msgs, int num)
{
	struct dw_hdmi_qp *hdmi = i2c_get_adapdata(adap);
	struct dw_hdmi_qp_i2c *i2c = hdmi->i2c;
	u8 addr = msgs[0].addr;
	int i, ret = 0;

	if (addr == DDC_CI_ADDR)
		/*
		 * The internal I2C controller does not support the multi-byte
		 * read and write operations needed for DDC/CI.
		 * TOFIX: Blacklist the DDC/CI address until we filter out
		 * unsupported I2C operations.
		 */
		return -EOPNOTSUPP;

	dev_dbg(hdmi->dev, "i2c xfer: num: %d, addr: %#x\n", num, addr);

	for (i = 0; i < num; i++) {
		if (msgs[i].len == 0) {
			dev_err(hdmi->dev,
				"unsupported transfer %d/%d, no data\n",
				i + 1, num);
			return -EOPNOTSUPP;
		}
	}

	mutex_lock(&i2c->lock);

	/* Unmute DONE and ERROR interrupts */
	hdmi_modb(hdmi, I2CM_NACK_RCVD_MASK_N | I2CM_OP_DONE_MASK_N,
		  I2CM_NACK_RCVD_MASK_N | I2CM_OP_DONE_MASK_N,
		  MAINUNIT_1_INT_MASK_N);

	/* Set slave device address taken from the first I2C message */
	if (addr == DDC_SEGMENT_ADDR && msgs[0].len == 1)
		addr = DDC_ADDR;

	hdmi_modb(hdmi, addr << 5, I2CM_SLVADDR, I2CM_INTERFACE_CONTROL0);

	/* Set slave device register address on transfer */
	i2c->is_regaddr = false;

	/* Set segment pointer for I2C extended read mode operation */
	i2c->is_segment = false;

	for (i = 0; i < num; i++) {
		dev_dbg(hdmi->dev, "xfer: num: %d/%d, len: %d, flags: %#x\n",
			i + 1, num, msgs[i].len, msgs[i].flags);

		if (msgs[i].addr == DDC_SEGMENT_ADDR && msgs[i].len == 1) {
			i2c->is_segment = true;
			hdmi_modb(hdmi, DDC_SEGMENT_ADDR, I2CM_SEG_ADDR,
				  I2CM_INTERFACE_CONTROL1);
			hdmi_modb(hdmi, *msgs[i].buf << 7, I2CM_SEG_PTR,
				  I2CM_INTERFACE_CONTROL1);
		} else {
			if (msgs[i].flags & I2C_M_RD)
				ret = dw_hdmi_i2c_read(hdmi, msgs[i].buf,
						       msgs[i].len);
			else
				ret = dw_hdmi_i2c_write(hdmi, msgs[i].buf,
							msgs[i].len);
		}
		if (ret < 0)
			break;
	}

	if (!ret)
		ret = num;

	/* Mute DONE and ERROR interrupts */
	hdmi_modb(hdmi, 0, I2CM_OP_DONE_MASK_N | I2CM_NACK_RCVD_MASK_N,
		  MAINUNIT_1_INT_MASK_N);

	mutex_unlock(&i2c->lock);

	return ret;
}

static u32 dw_hdmi_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm dw_hdmi_algorithm = {
	.master_xfer	= dw_hdmi_i2c_xfer,
	.functionality	= dw_hdmi_i2c_func,
};

static struct i2c_adapter *dw_hdmi_i2c_adapter(struct dw_hdmi_qp *hdmi)
{
	struct i2c_adapter *adap;
	struct dw_hdmi_qp_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(hdmi->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return ERR_PTR(-ENOMEM);

	mutex_init(&i2c->lock);
	init_completion(&i2c->cmp);

	adap = &i2c->adap;
	adap->class = I2C_CLASS_DDC;
	adap->owner = THIS_MODULE;
	adap->dev.parent = hdmi->dev;
	adap->algo = &dw_hdmi_algorithm;
	strscpy(adap->name, "ddc", sizeof(adap->name));
	i2c_set_adapdata(adap, hdmi);

	ret = i2c_add_adapter(adap);
	if (ret) {
		dev_warn(hdmi->dev, "cannot add %s I2C adapter\n", adap->name);
		devm_kfree(hdmi->dev, i2c);
		return ERR_PTR(ret);
	}

	hdmi->i2c = i2c;

	dev_info(hdmi->dev, "registered %s I2C bus driver\n", adap->name);

	return adap;
}

#define HDMI_PHY_EARC_MASK	BIT(29)

int dw_hdmi_qp_set_earc(struct dw_hdmi_qp *hdmi)
{
	u32 stat, ret;

	/* set hdmi phy earc mode */
	hdmi->phy.ops->set_mode(hdmi, hdmi->phy.data, HDMI_PHY_EARC_MASK,
				true);

	ret = hdmi->phy.ops->init(hdmi, hdmi->phy.data,
				  &hdmi->previous_mode);
	if (ret)
		return ret;

	hdmi->disabled = false;

	reinit_completion(&hdmi->earc_cmp);

	hdmi_modb(hdmi, EARCRX_CMDC_DISCOVERY_TIMEOUT_IRQ |
		  EARCRX_CMDC_DISCOVERY_DONE_IRQ,
		  EARCRX_CMDC_DISCOVERY_TIMEOUT_IRQ |
		  EARCRX_CMDC_DISCOVERY_DONE_IRQ, EARCRX_0_INT_MASK_N);

	/* start discovery */
	hdmi_modb(hdmi, EARCRX_CMDC_DISCOVERY_EN, EARCRX_CMDC_DISCOVERY_EN,
		  EARCRX_CMDC_CONTROL);

	/*
	 * The eARC TX device drives a logic-high-voltage-level
	 * pulse on the physical HPD connector pin, after
	 * at least 100 ms of low voltage level to start the
	 * eARC Discovery process.
	 */
	hdmi_modb(hdmi, EARCRX_CONNECTOR_HPD, EARCRX_CONNECTOR_HPD,
		  EARCRX_CMDC_CONTROL);

	stat = wait_for_completion_timeout(&hdmi->earc_cmp, HZ / 10);
	if (!stat)
		return -EAGAIN;

	if (hdmi->earc_intr & EARCRX_CMDC_DISCOVERY_TIMEOUT_IRQ) {
		dev_err(hdmi->dev, "discovery timeout\n");
		return -ETIMEDOUT;
	} else if (hdmi->earc_intr & EARCRX_CMDC_DISCOVERY_DONE_IRQ) {
		dev_info(hdmi->dev, "discovery done\n");
	} else {
		dev_err(hdmi->dev, "discovery failed\n");
		return -EINVAL;
	}

	hdmi_writel(hdmi, 1, EARCRX_DMAC_PHY_CONTROL);
	hdmi_modb(hdmi, EARCRX_CMDC_SWINIT_P, EARCRX_CMDC_SWINIT_P,
		  EARCRX_CMDC_CONFIG0);

	hdmi_writel(hdmi, 0xf3, EARCRX_DMAC_CONFIG);
	hdmi_writel(hdmi, 0x63, EARCRX_DMAC_CONTROL0);
	hdmi_writel(hdmi, 0xff, EARCRX_DMAC_CONTROL1);

	hdmi_modb(hdmi, EARCRX_XACTREAD_STOP_CFG | EARCRX_XACTREAD_RETRY_CFG |
		  EARCRX_CMDC_DSCVR_EARCVALID0_TO_DISC1 | EARCRX_CMDC_XACT_RESTART_EN,
		  EARCRX_XACTREAD_STOP_CFG | EARCRX_XACTREAD_RETRY_CFG |
		  EARCRX_CMDC_DSCVR_EARCVALID0_TO_DISC1 | EARCRX_CMDC_XACT_RESTART_EN,
		  EARCRX_CMDC_CONFIG0);

	hdmi_writel(hdmi, 0, EARCRX_DMAC_CHSTATUS_STREAMER0);
	hdmi_writel(hdmi, 0x1b0e, EARCRX_DMAC_CHSTATUS_STREAMER1);
	hdmi_writel(hdmi, 0, EARCRX_DMAC_CHSTATUS_STREAMER2);
	hdmi_writel(hdmi, 0, EARCRX_DMAC_CHSTATUS_STREAMER3);
	hdmi_writel(hdmi, 0xf2000000, EARCRX_DMAC_CHSTATUS_STREAMER4);
	hdmi_writel(hdmi, 0, EARCRX_DMAC_CHSTATUS_STREAMER5);
	hdmi_writel(hdmi, 0, EARCRX_DMAC_CHSTATUS_STREAMER6);
	hdmi_writel(hdmi, 0, EARCRX_DMAC_CHSTATUS_STREAMER7);
	hdmi_writel(hdmi, 0, EARCRX_DMAC_CHSTATUS_STREAMER8);

	return 0;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_earc);

/* -----------------------------------------------------------------------------
 * HDMI TX Setup
 */

static void hdmi_infoframe_set_checksum(u8 *ptr, int size)
{
	u8 csum = 0;
	int i;

	ptr[3] = 0;
	/* compute checksum */
	for (i = 0; i < size; i++)
		csum += ptr[i];

	ptr[3] = 256 - csum;
}

static bool is_hdmi2_sink(const struct drm_connector *connector)
{
	if (!connector)
		return true;

	return connector->display_info.hdmi.scdc.supported ||
		connector->display_info.color_formats & DRM_COLOR_FORMAT_YCBCR420;
}

static
bool hdmi_quirk_vsi(const struct drm_connector *connector, u8 *vendor_info)
{
	const struct hdmi_quirk *quirk = get_hdmi_quirk(vendor_info);

	if (!quirk)
		return false;

	if (!(quirk->quirk_case & BIT(VSI_SEND)))
		return false;

	return true;
}

static void hdmi_config_AVI(struct dw_hdmi_qp *hdmi,
			    const struct drm_connector *connector,
			    const struct drm_display_mode *mode)
{
	struct hdmi_avi_infoframe frame;
	u32 val, i, j;
	u8 buff[17];
	enum hdmi_quantization_range rgb_quant_range =
		hdmi->hdmi_data.quant_range;

	/* Initialise info frame from DRM mode */
	drm_hdmi_avi_infoframe_from_display_mode(&frame, connector, mode);

	/*
	 * Ignore monitor selectable quantization, use quantization set
	 * by the user
	 */
	drm_hdmi_avi_infoframe_quant_range(&frame, connector, mode, rgb_quant_range);
	if (hdmi_bus_fmt_is_yuv444(hdmi->hdmi_data.enc_out_bus_format))
		frame.colorspace = HDMI_COLORSPACE_YUV444;
	else if (hdmi_bus_fmt_is_yuv422(hdmi->hdmi_data.enc_out_bus_format))
		frame.colorspace = HDMI_COLORSPACE_YUV422;
	else if (hdmi_bus_fmt_is_yuv420(hdmi->hdmi_data.enc_out_bus_format))
		frame.colorspace = HDMI_COLORSPACE_YUV420;
	else
		frame.colorspace = HDMI_COLORSPACE_RGB;

	/* Set up colorimetry and quant range */
	if (!hdmi_bus_fmt_is_rgb(hdmi->hdmi_data.enc_out_bus_format)) {
		switch (hdmi->hdmi_data.enc_out_encoding) {
		case V4L2_YCBCR_ENC_601:
			if (hdmi->hdmi_data.enc_in_encoding == V4L2_YCBCR_ENC_XV601)
				frame.colorimetry = HDMI_COLORIMETRY_EXTENDED;
			else
				frame.colorimetry = HDMI_COLORIMETRY_ITU_601;
			frame.extended_colorimetry =
					HDMI_EXTENDED_COLORIMETRY_XV_YCC_601;
			break;
		case V4L2_YCBCR_ENC_709:
			if (hdmi->hdmi_data.enc_in_encoding == V4L2_YCBCR_ENC_XV709)
				frame.colorimetry = HDMI_COLORIMETRY_EXTENDED;
			else
				frame.colorimetry = HDMI_COLORIMETRY_ITU_709;
			frame.extended_colorimetry =
					HDMI_EXTENDED_COLORIMETRY_XV_YCC_709;
			break;
		case V4L2_YCBCR_ENC_BT2020:
			if (hdmi->hdmi_data.enc_in_encoding == V4L2_YCBCR_ENC_BT2020)
				frame.colorimetry = HDMI_COLORIMETRY_EXTENDED;
			else
				frame.colorimetry = HDMI_COLORIMETRY_ITU_709;
			frame.extended_colorimetry =
					HDMI_EXTENDED_COLORIMETRY_BT2020;
			break;
		default: /* Carries no data */
			frame.colorimetry = HDMI_COLORIMETRY_ITU_601;
			frame.extended_colorimetry =
					HDMI_EXTENDED_COLORIMETRY_XV_YCC_601;
			break;
		}

		frame.ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
	} else {
		if (hdmi->hdmi_data.enc_out_encoding == V4L2_YCBCR_ENC_BT2020) {
			frame.colorimetry = HDMI_COLORIMETRY_EXTENDED;
			frame.extended_colorimetry =
				HDMI_EXTENDED_COLORIMETRY_BT2020;
		} else {
			frame.colorimetry = HDMI_COLORIMETRY_NONE;
			frame.extended_colorimetry =
				HDMI_EXTENDED_COLORIMETRY_XV_YCC_601;
		}

		if (is_hdmi2_sink(connector) &&
		    frame.quantization_range == HDMI_QUANTIZATION_RANGE_FULL)
			frame.ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_FULL;
		else
			frame.ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
	}

	frame.scan_mode = HDMI_SCAN_MODE_NONE;

	hdmi_avi_infoframe_pack_only(&frame, buff, 17);

	/* mode which vic >= 128 must use avi version 3 */
	if (hdmi->vic >= 128) {
		frame.version = 3;
		buff[1] = frame.version;
		buff[4] &= 0x1f;
		buff[4] |= ((frame.colorspace & 0x7) << 5);
		buff[7] = hdmi->vic;
		hdmi_infoframe_set_checksum(buff, 17);
	} else if (is_hdmi2_sink(connector) && hdmi_quirk_vsi(connector, hdmi->vendor_info)) {
		buff[7] = hdmi->vic;
		hdmi_infoframe_set_checksum(buff, 17);
	}

	/*
	 * The Designware IP uses a different byte format from standard
	 * AVI info frames, though generally the bits are in the correct
	 * bytes.
	 */

	val = (frame.version << 8) | (frame.length << 16);
	hdmi_writel(hdmi, val, PKT_AVI_CONTENTS0);

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			if (i * 4 + j >= 14)
				break;
			if (!j)
				val = buff[i * 4 + j + 3];
			val |= buff[i * 4 + j + 3] << (8 * j);
		}

		hdmi_writel(hdmi, val, PKT_AVI_CONTENTS1 + i * 4);
	}

	hdmi_modb(hdmi, 0, PKTSCHED_AVI_FIELDRATE, PKTSCHED_PKT_CONFIG1);

	hdmi_modb(hdmi, PKTSCHED_AVI_TX_EN, PKTSCHED_AVI_TX_EN, PKTSCHED_PKT_EN);
}

#define VSI_PKT_TYPE		0x81
#define VSI_PKT_VERSION		1
#define HDMI_FORUM_OUI		0xc45dd8
#define ALLM_MODE		BIT(1)
#define HDMI_FORUM_LEN		9

static void hdmi_config_vendor_specific_infoframe(struct dw_hdmi_qp *hdmi,
						  const struct drm_connector *connector,
						  const struct drm_display_mode *mode)
{
	struct hdmi_vendor_infoframe frame;
	u8 buffer[10];
	u32 val;
	ssize_t err;
	int i, reg;
	struct dw_hdmi_link_config *link_cfg = NULL;
	void *data = hdmi->plat_data->phy_data;

	hdmi_modb(hdmi, 0, PKTSCHED_VSI_TX_EN, PKTSCHED_PKT_EN);

	if (hdmi->dovi_vsif[0]) {
		for (i = 0; i < 8; i++)
			hdmi_writel(hdmi, hdmi->dovi_vsif[i], PKT_VSI_CONTENTS0 + i * 4);

		goto out;
	}

	if (hdmi->plat_data->get_link_cfg)
		link_cfg = hdmi->plat_data->get_link_cfg(data);

	for (i = 0; i <= 7; i++)
		hdmi_writel(hdmi, 0, PKT_VSI_CONTENTS0 + i * 4);

	if (hdmi->allm_enable && link_cfg->allm_supported) {
		buffer[0] = VSI_PKT_TYPE;
		buffer[1] = VSI_PKT_VERSION;
		buffer[2] = 5;
		buffer[4] = HDMI_FORUM_OUI & 0xff;
		buffer[5] = (HDMI_FORUM_OUI >> 8) & 0xff;
		buffer[6] = (HDMI_FORUM_OUI >> 16) & 0xff;
		buffer[7] = VSI_PKT_VERSION;
		buffer[8] = ALLM_MODE;

		hdmi_infoframe_set_checksum(buffer, HDMI_FORUM_LEN);

		err = 9;
	} else {
		err = drm_hdmi_vendor_infoframe_from_display_mode(&frame, connector,
								  mode);
		if (err < 0)
			/*
			 * Going into that statement does not means vendor infoframe
			 * fails. It just informed us that vendor infoframe is not
			 * needed for the selected mode. Only 4k or stereoscopic 3D
			 * mode requires vendor infoframe. So just simply return.
			 */
			return;

		if ((is_hdmi2_sink(connector) && hdmi_quirk_vsi(connector, hdmi->vendor_info)) ||
		    !frame.vic) {
			hdmi_modb(hdmi, 0, PKTSCHED_VSI_TX_EN, PKTSCHED_PKT_EN);
			return;
		}

		err = hdmi_vendor_infoframe_pack(&frame, buffer, sizeof(buffer));
		if (err < 0) {
			dev_err(hdmi->dev, "Failed to pack vendor infoframe: %zd\n",
				err);
			return;
		}
	}

	/* vsi header */
	val = (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
	hdmi_writel(hdmi, val, PKT_VSI_CONTENTS0);

	reg = PKT_VSI_CONTENTS1;
	for (i = 3; i < err; i++) {
		if (i % 4 == 3)
			val = buffer[i];
		if (i % 4 == 0)
			val |= buffer[i] << 8;
		if (i % 4 == 1)
			val |= buffer[i] << 16;
		if (i % 4 == 2)
			val |= buffer[i] << 24;

		if ((i % 4 == 2) || (i == (err - 1))) {
			hdmi_writel(hdmi, val, reg);
			reg += 4;
		}
	}

	hdmi_writel(hdmi, 0, PKT_VSI_CONTENTS7);

out:
	hdmi_modb(hdmi, 0, PKTSCHED_VSI_FIELDRATE, PKTSCHED_PKT_CONFIG1);
	hdmi_modb(hdmi, PKTSCHED_VSI_TX_EN, PKTSCHED_VSI_TX_EN,
		  PKTSCHED_PKT_EN);
}

static void hdmi_config_CVTEM(struct dw_hdmi_qp *hdmi)
{
	u8 ds_type = 0;
	u8 sync = 1;
	u8 vfr = 1;
	u8 afr = 0;
	u8 new = 1;
	u8 end = 0;
	u8 data_set_length = 136;
	u8 hb1[6] = { 0x80, 0, 0, 0, 0, 0x40 };
	u8 *pps_body;
	u32 val, i, reg;
	struct drm_display_mode *mode = &hdmi->previous_mode;
	int hsync, hfront, hback;
	struct dw_hdmi_link_config *link_cfg;
	void *data = hdmi->plat_data->phy_data;

	hdmi_modb(hdmi, 0, PKTSCHED_EMP_CVTEM_TX_EN, PKTSCHED_PKT_EN);

	if (hdmi->plat_data->get_link_cfg) {
		link_cfg = hdmi->plat_data->get_link_cfg(data);
	} else {
		dev_err(hdmi->dev, "can't get frl link cfg\n");
		return;
	}

	if (!link_cfg->dsc_mode) {
		dev_info(hdmi->dev, "don't use dsc mode\n");
		return;
	}

	pps_body = link_cfg->pps_payload;

	hsync = mode->hsync_end - mode->hsync_start;
	hback = mode->htotal - mode->hsync_end;
	hfront = mode->hsync_start - mode->hdisplay;

	for (i = 0; i < 6; i++) {
		val = i << 16 | hb1[i] << 8;
		hdmi_writel(hdmi, val, PKT0_EMP_CVTEM_CONTENTS0 + i * 0x20);
	}

	val = new << 7 | end << 6 | ds_type << 4 | afr << 3 |
	      vfr << 2 | sync << 1;
	hdmi_writel(hdmi, val, PKT0_EMP_CVTEM_CONTENTS1);

	val = data_set_length << 16 | pps_body[0] << 24;
	hdmi_writel(hdmi, val, PKT0_EMP_CVTEM_CONTENTS2);

	reg = PKT0_EMP_CVTEM_CONTENTS3;
	for (i = 1; i < 125; i++) {
		if (reg == PKT1_EMP_CVTEM_CONTENTS0 ||
		    reg == PKT2_EMP_CVTEM_CONTENTS0 ||
		    reg == PKT3_EMP_CVTEM_CONTENTS0 ||
		    reg == PKT4_EMP_CVTEM_CONTENTS0 ||
		    reg == PKT5_EMP_CVTEM_CONTENTS0) {
			reg += 4;
			i--;
			continue;
		}
		if (i % 4 == 1)
			val = pps_body[i];
		if (i % 4 == 2)
			val |= pps_body[i] << 8;
		if (i % 4 == 3)
			val |= pps_body[i] << 16;
		if (!(i % 4)) {
			val |= pps_body[i] << 24;
			hdmi_writel(hdmi, val, reg);
			reg += 4;
		}
	}

	val = (hfront & 0xff) << 24 | pps_body[127] << 16 |
	      pps_body[126] << 8 | pps_body[125];
	hdmi_writel(hdmi, val, PKT4_EMP_CVTEM_CONTENTS6);

	val = (hback & 0xff) << 24 | ((hsync >> 8) & 0xff) << 16 |
	      (hsync & 0xff) << 8 | ((hfront >> 8) & 0xff);
	hdmi_writel(hdmi, val, PKT4_EMP_CVTEM_CONTENTS7);

	val = link_cfg->hcactive << 8 | ((hback >> 8) & 0xff);
	hdmi_writel(hdmi, val, PKT5_EMP_CVTEM_CONTENTS1);

	for (i = PKT5_EMP_CVTEM_CONTENTS2; i <= PKT5_EMP_CVTEM_CONTENTS7; i += 4)
		hdmi_writel(hdmi, 0, i);

	hdmi_modb(hdmi, PKTSCHED_EMP_CVTEM_TX_EN, PKTSCHED_EMP_CVTEM_TX_EN,
		  PKTSCHED_PKT_EN);
}

static void hdmi_config_drm_infoframe(struct dw_hdmi_qp *hdmi,
				      const struct drm_connector *connector)
{
	const struct drm_connector_state *conn_state = connector->state;
	struct hdr_output_metadata *hdr_metadata;
	struct hdmi_drm_infoframe frame;
	u8 buffer[30];
	ssize_t err;
	int i;
	u32 val;

	if (!hdmi->plat_data->use_drm_infoframe)
		return;

	hdmi_modb(hdmi, 0, PKTSCHED_DRMI_TX_EN, PKTSCHED_PKT_EN);

	if (!hdmi->connector.hdr_sink_metadata.hdmi_type1.eotf) {
		DRM_DEBUG("No need to set HDR metadata in infoframe\n");
		return;
	}

	if (!conn_state->hdr_output_metadata) {
		DRM_DEBUG("source metadata not set yet\n");
		return;
	}

	hdr_metadata = (struct hdr_output_metadata *)
		conn_state->hdr_output_metadata->data;

	if (!(hdmi->connector.hdr_sink_metadata.hdmi_type1.eotf &
	      BIT(hdr_metadata->hdmi_metadata_type1.eotf))) {
		DRM_ERROR("Not support EOTF %d\n",
			  hdr_metadata->hdmi_metadata_type1.eotf);
		return;
	}

	err = drm_hdmi_infoframe_set_hdr_metadata(&frame, conn_state);
	if (err < 0)
		return;

	err = hdmi_drm_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to pack drm infoframe: %zd\n", err);
		return;
	}

	val = (frame.version << 8) | (frame.length << 16);
	hdmi_writel(hdmi, val, PKT_DRMI_CONTENTS0);

	for (i = 0; i <= frame.length; i++) {
		if (i % 4 == 0)
			val = buffer[3 + i];
		val |= buffer[3 + i] << ((i % 4) * 8);

		if (i % 4 == 3 || (i == (frame.length)))
			hdmi_writel(hdmi, val, PKT_DRMI_CONTENTS1 + ((i / 4) * 4));
	}

	hdmi_modb(hdmi, 0, PKTSCHED_DRMI_FIELDRATE, PKTSCHED_PKT_CONFIG1);

	/*
	 * avi and hdr infoframe cannot be sent at the same time
	 * for compatibility with Huawei TV
	 */
	mdelay(50);
	hdmi_modb(hdmi, PKTSCHED_DRMI_TX_EN, PKTSCHED_DRMI_TX_EN, PKTSCHED_PKT_EN);

	DRM_DEBUG("%s eotf %d end\n", __func__,
		  hdr_metadata->hdmi_metadata_type1.eotf);
}

/* Filter out invalid setups to avoid configuring SCDC and scrambling */
static bool dw_hdmi_support_scdc(struct dw_hdmi_qp *hdmi,
				 const struct drm_display_info *display)
{
	/* Disable if no DDC bus */
	if (!hdmi->ddc)
		return false;

	if (hdmi->force_kernel_output)
		return true;

	/* Disable if SCDC is not supported, or if an HF-VSDB block is absent */
	if (!display->hdmi.scdc.supported ||
	    !display->hdmi.scdc.scrambling.supported)
		return false;

	/*
	 * Disable if display only support low TMDS rates and scrambling
	 * for low rates is not supported either
	 */
	if (!display->hdmi.scdc.scrambling.low_rates &&
	    display->max_tmds_clock <= 340000)
		return false;

	return true;
}

static int hdmi_set_frl_mask(int frl_rate)
{
	switch (frl_rate) {
	case 48:
		return FRL_12GBPS_4LANE;
	case 40:
		return FRL_10GBPS_4LANE;
	case 32:
		return FRL_8GBPS_4LANE;
	case 24:
		return FRL_6GBPS_4LANE;
	case 18:
		return FRL_6GBPS_3LANE;
	case 9:
		return FRL_3GBPS_3LANE;
	}

	return 0;
}

static int hdmi_set_frl_actual(int frl_level)
{
	switch (frl_level) {
	case FRL_12GBPS_4LANE:
		return 48;
	case FRL_10GBPS_4LANE:
		return 40;
	case FRL_8GBPS_4LANE:
		return 32;
	case FRL_6GBPS_4LANE:
		return 24;
	case FRL_6GBPS_3LANE:
		return 18;
	case FRL_3GBPS_3LANE:
		return 9;
	}

	return 0;
}

enum flt_state {
	LTS1 = 0,	/* Read edid */
	LTS2,		/* Prepare for frl */
	LTS3,		/* Training in progress */
	LTS4,		/* Update frl_rate */
	LTSP,		/* Training passed */
	LTSL,		/* Exit frl mode */
};

static bool dw_hdmi_qp_is_disabled(struct dw_hdmi_qp *hdmi)
{
	if (hdmi->phy.ops->read_hpd(hdmi, hdmi->phy.data) == connector_status_disconnected)
		return true;

	mutex_lock(&hdmi->mutex);
	if (hdmi->disabled) {
		mutex_unlock(&hdmi->mutex);
		return true;
	}
	mutex_unlock(&hdmi->mutex);

	return false;
}

/* check sink version and if flt no timeout mode */
static int dw_hdmi_qp_flt_lts1(struct dw_hdmi_qp *hdmi)
{
	u8 val = 0;

	mutex_lock(&hdmi->audio_mutex);
	if (hdmi->dclk_en) {
		hdmi_modb(hdmi, AVP_DATAPATH_VIDEO_SWDISABLE,
			  AVP_DATAPATH_VIDEO_SWDISABLE, GLOBAL_SWDISABLE);

		/* reset avp data path */
		hdmi_writel(hdmi, BIT(6), GLOBAL_SWRESET_REQUEST);
	} else {
		dev_err(hdmi->dev, "hdmi dclk is disabled, lts1 failed\n");
		mutex_unlock(&hdmi->audio_mutex);
		return LTSL;
	}
	mutex_unlock(&hdmi->audio_mutex);

	drm_scdc_readb(hdmi->ddc, SCDC_SINK_VERSION, &val);
	if (!val) {
		dev_err(hdmi->dev, "scdc sink version is zero, lts1 failed\n");
		return LTSL;
	}
	drm_scdc_writeb(hdmi->ddc, SCDC_SOURCE_VERSION, 1);
	drm_scdc_readb(hdmi->ddc, SCDC_SOURCE_TEST_CONFIG, &val);
	if (val & BIT(5))
		hdmi->flt_no_timeout = true;
	else
		hdmi->flt_no_timeout = false;

	return LTS2;
}

/* check if sink is ready to training and set source output frl rate/max ffe level */
static int dw_hdmi_qp_flt_lts2(struct dw_hdmi_qp *hdmi, u8 rate)
{
	int i;
	u8 val = 0;
	u8 flt_rate = hdmi_set_frl_mask(rate);

	/* FLT_READY & FFE_LEVELS read */
	for (i = 0; i < 20; i++) {
		drm_scdc_readb(hdmi->ddc, SCDC_STATUS_FLAGS_0, &val);
		if (val & BIT(6))
			break;
		msleep(20);
	}

	if (i == 20) {
		dev_err(hdmi->dev, "sink flt isn't ready,SCDC_STATUS_FLAGS_0:0x%x\n", val);
		return LTSL;
	}

	/* max ffe level 3 */
	val = hdmi->max_ffe_lv << 4 | flt_rate;
	drm_scdc_writeb(hdmi->ddc, SCDC_CONFIG_1, val);
	drm_scdc_writeb(hdmi->ddc, SCDC_CONFIG_0, 0);

	return LTS3;
}

static void dw_hdmi_qp_set_ltp(struct dw_hdmi_qp *hdmi, u32 value, bool flt_no_timeout)
{
	/* support hfr1-10, send old ltp when all lane is 3 */
	if (!flt_no_timeout && value == 0x3333f)
		value = hdmi_readl(hdmi, FLT_CONFIG1);

	hdmi_writel(hdmi, value, FLT_CONFIG1);
}

/*
 * conducts link training for the specified frl rate
 * send sink request ltp or change ffe level
 */
static int dw_hdmi_qp_flt_lts3(struct dw_hdmi_qp *hdmi, u8 rate)
{
	u8 val;
	int i = 0, ret = 0;
	u8 src_test_cfg = 0;
	u32 value;
	u8 ffe_lv = 0;

	/* we set max 2s timeout */
	i = 4000;
	while (i > 0 || hdmi->flt_no_timeout) {
		if (dw_hdmi_qp_is_disabled(hdmi)) {
			dev_info(hdmi->dev, "hdmi dclk is disabled, stop flt\n");
			break;
		}

		i--;
		/* source should poll update flag every 2ms or less */
		usleep_range(400, 500);

		drm_scdc_readb(hdmi->ddc, SCDC_UPDATE_0, &val);

		/* SOURCE_TEST_UPDATE */
		if (val & BIT(3)) {
			/* quit test mode */
			drm_scdc_readb(hdmi->ddc, SCDC_SOURCE_TEST_CONFIG, &src_test_cfg);
			if (hdmi->flt_no_timeout && !(src_test_cfg & BIT(5))) {
				DRM_DEV_DEBUG_DRIVER(hdmi->dev, "flt get out of test mode\n");
				hdmi->flt_no_timeout = false;
			} else if (!hdmi->flt_no_timeout && (src_test_cfg & BIT(5))) {
				DRM_DEV_DEBUG_DRIVER(hdmi->dev, "flt go into test mode\n");
				hdmi->flt_no_timeout = true;
			}
		}

		if (!(val & SCDC_CONFIG_0)) {
			/* clear SOURCE_TEST_UPDATE flag */
			if (val & BIT(3))
				drm_scdc_writeb(hdmi->ddc, SCDC_UPDATE_0, val);
			continue;
		}

		/* flt_update */
		if (val & BIT(5)) {
			u8 reg_val, ln0, ln1, ln2, ln3;

			drm_scdc_readb(hdmi->ddc, SCDC_STATUS_FLAGS_1, &reg_val);
			ln0 = reg_val & 0xf;
			ln1 = (reg_val >> 4) & 0xf;

			drm_scdc_readb(hdmi->ddc, SCDC_STATUS_FLAGS_2, &reg_val);
			ln2 = reg_val & 0xf;
			ln3 = (reg_val >> 4) & 0xf;

			DRM_DEV_DEBUG_DRIVER(hdmi->dev, "ln0:0x%x,ln1:0x%x,ln2:0x%x,ln3:0x%x\n",
					     ln0, ln1, ln2, ln3);

			if (!ln0 && !ln1 && !ln2 && !ln3) {
				dev_info(hdmi->dev, "Training finish, go to ltsp\n");
				mutex_lock(&hdmi->audio_mutex);
				if (hdmi->dclk_en) {
					hdmi_writel(hdmi, 0, FLT_CONFIG1);
					ret = LTSP;
				} else {
					dev_err(hdmi->dev, "hdmi dclk is disabled, goto ltsp failed\n");
					ret = LTSL;
				}
				mutex_unlock(&hdmi->audio_mutex);
			} else if ((ln0 == 0xf) | (ln1 == 0xf) | (ln2 == 0xf) | (ln3 == 0xf)) {
				dev_err(hdmi->dev, "goto lts4\n");
				ret = LTS4;
			} else if ((ln0 == 0xe) | (ln1 == 0xe) | (ln2 == 0xe) | (ln3 == 0xe)) {
				dev_info(hdmi->dev, "goto ffe\n");
				if (ffe_lv < 3) {
					hdmi->phy.ops->set_ffe(hdmi, hdmi->phy.data, ++ffe_lv);
				} else {
					dev_err(hdmi->dev, "ffe level out of range\n");
					ret = LTSL;
				}
			} else {
				mutex_lock(&hdmi->audio_mutex);
				if (hdmi->dclk_en) {
					value = (ln3 << 16) | (ln2 << 12) | (ln1 << 8) |
						(ln0 << 4) | 0xf;

					dw_hdmi_qp_set_ltp(hdmi, value, hdmi->flt_no_timeout);
				} else {
					dev_err(hdmi->dev, "hdmi dclk is disabled, set ltp failed\n");
					ret = LTSL;
				}
				mutex_unlock(&hdmi->audio_mutex);
			}

			/* only clear flt_update */
			drm_scdc_writeb(hdmi->ddc, SCDC_UPDATE_0, val);
		}

		if (ret)
			break;
	}

	if (!ret) {
		ret = LTSL;
		dev_err(hdmi->dev, "lts3 time out, goto ltsl\n");
	}

	return ret;
}

/* sink request frl rate change, start training for a new rate. */
static int dw_hdmi_qp_flt_lts4(struct dw_hdmi_qp *hdmi, u8 *rate)
{
	u8 actual_rate;
	void *data = hdmi->plat_data->phy_data;
	u8 flt_rate = hdmi_set_frl_mask(*rate);

	/* we don't use frl rate below 24G */
	if (flt_rate == FRL_8GBPS_4LANE) {
		dev_err(hdmi->dev, "goto ltsl\n");
		return LTSL;
	}

	/* disable phy */
	hdmi->phy.ops->disable(hdmi, hdmi->phy.data);
	if (hdmi->plat_data->link_clk_set)
		hdmi->plat_data->link_clk_set(data, false);

	/* set lower frl rate */
	flt_rate--;
	actual_rate = hdmi_set_frl_actual(flt_rate);
	if (hdmi->plat_data->force_frl_rate)
		hdmi->plat_data->force_frl_rate(data, actual_rate);

	if (hdmi->plat_data->link_clk_set)
		hdmi->plat_data->link_clk_set(data, true);

	/* enable phy */
	hdmi->phy.ops->init(hdmi, hdmi->phy.data, &hdmi->previous_mode);

	*rate = actual_rate;
	/* set new rate */
	drm_scdc_writeb(hdmi->ddc, SCDC_CONFIG_1, (hdmi->max_ffe_lv << 4 | flt_rate));
	drm_scdc_writeb(hdmi->ddc, SCDC_UPDATE_0, BIT(5));

	dev_info(hdmi->dev, "from lts4 go to lts3\n");
	return LTS3;
}

/* training is passed, start poll sink check if sink want to change rate or exit frl mode */
static int dw_hdmi_qp_flt_ltsp(struct dw_hdmi_qp *hdmi)
{
	u8 val = 0;
	int i = 4000;

	/* wait frl start */
	while (i--) {
		if (dw_hdmi_qp_is_disabled(hdmi)) {
			dev_info(hdmi->dev, "hdmi dclk is disabled, quit ltsp\n");
			return LTSL;
		}

		/* source should poll update flag every 2ms or less */
		usleep_range(400, 500);

		drm_scdc_readb(hdmi->ddc, SCDC_UPDATE_0, &val);

		if (!(val & SCDC_CONFIG_0))
			continue;

		mutex_lock(&hdmi->audio_mutex);
		if (hdmi->dclk_en) {
			/* flt_start */
			if (val & BIT(4)) {
				hdmi_modb(hdmi, 0, AVP_DATAPATH_VIDEO_SWDISABLE, GLOBAL_SWDISABLE);
				/* clear flt_start */
				drm_scdc_writeb(hdmi->ddc, SCDC_UPDATE_0, BIT(4));
				hdmi_writel(hdmi, 2, PKTSCHED_PKT_CONTROL0);
				hdmi_modb(hdmi, PKTSCHED_GCP_TX_EN, PKTSCHED_GCP_TX_EN,
					  PKTSCHED_PKT_EN);
				dev_info(hdmi->dev, "flt success\n");
				mutex_unlock(&hdmi->audio_mutex);
				break;
			} else if (val & BIT(5)) {
				hdmi_modb(hdmi, AVP_DATAPATH_VIDEO_SWDISABLE,
					  AVP_DATAPATH_VIDEO_SWDISABLE, GLOBAL_SWDISABLE);
				drm_scdc_writeb(hdmi->ddc, SCDC_UPDATE_0, BIT(5));
				mutex_unlock(&hdmi->audio_mutex);
				return LTS3;
			}
		} else {
			mutex_unlock(&hdmi->audio_mutex);
			dev_err(hdmi->dev, "hdmi dclk is disabled, wait frl start failed\n");
			return LTSL;
		}
		mutex_unlock(&hdmi->audio_mutex);
	}

	if (i < 0) {
		dev_err(hdmi->dev, "wait flt_start or flt_update time out, SCDC_UPDATE_0:0x%x\n",
			val);
		return LTSL;
	}

	/*
	 * For compatibility with tv changhong d8k no signal issue,
	 * not hdmi protocol requirements.
	 */
	for (i = 0; i < 200; i++) {
		hdmi_modb(hdmi, PKTSCHED_NULL_TX_EN, PKTSCHED_NULL_TX_EN, PKTSCHED_PKT_EN);
		usleep_range(50, 60);
		hdmi_modb(hdmi, 0, PKTSCHED_NULL_TX_EN, PKTSCHED_PKT_EN);
		usleep_range(50, 60);
	}

	i = 5;
	/* flt success poll flt_update */
	while (1) {
		if (dw_hdmi_qp_is_disabled(hdmi)) {
			dev_info(hdmi->dev, "hdmi dclk is disabled, stop poll flt_update\n");
			return LTSL;
		}

		if (!i) {
			i = 5;
			drm_scdc_readb(hdmi->ddc, SCDC_UPDATE_0, &val);

			mutex_lock(&hdmi->audio_mutex);
			if (hdmi->dclk_en) {
				if (val & BIT(5)) {
					hdmi_writel(hdmi, 1, PKTSCHED_PKT_CONTROL0);
					hdmi_modb(hdmi, PKTSCHED_GCP_TX_EN, PKTSCHED_GCP_TX_EN,
						  PKTSCHED_PKT_EN);
					msleep(50);
					hdmi_modb(hdmi, AVP_DATAPATH_VIDEO_SWDISABLE,
						AVP_DATAPATH_VIDEO_SWDISABLE, GLOBAL_SWDISABLE);
					drm_scdc_writeb(hdmi->ddc, SCDC_UPDATE_0, BIT(5));
					mutex_unlock(&hdmi->audio_mutex);
					return LTS2;
				}
			} else {
				mutex_unlock(&hdmi->audio_mutex);
				dev_info(hdmi->dev,
					 "hdmi is disconnected, stop poll flt update flag\n");
				return LTSL;
			}
			mutex_unlock(&hdmi->audio_mutex);
		}
		/* after flt success source should poll update_flag at least once per 250ms */
		msleep(20);
		i--;
	}

	return LTSL;
}

/* exit frl mode, maybe it was a training failure or hdmi was disabled */
static int dw_hdmi_qp_flt_ltsl(struct dw_hdmi_qp *hdmi)
{
	if (hdmi->frl_switch)
		return -EINVAL;

	drm_scdc_writeb(hdmi->ddc, SCDC_CONFIG_1, 0);
	drm_scdc_writeb(hdmi->ddc, SCDC_UPDATE_0, BIT(5));

	return -EINVAL;
}

#define HDMI_MODE_FRL_MASK     BIT(30)

static int hdmi_set_op_mode(struct dw_hdmi_qp *hdmi,
			    struct dw_hdmi_link_config *link_cfg,
			    const struct drm_connector *connector)
{
	int ret = 0;

	if (hdmi->frl_switch)
		return 0;

	if (!link_cfg->frl_mode) {
		dev_info(hdmi->dev, "dw hdmi qp use tmds mode\n");
		hdmi_modb(hdmi, 0, OPMODE_FRL, LINK_CONFIG0);
		hdmi_modb(hdmi, 0, OPMODE_FRL_4LANES, LINK_CONFIG0);
		if (!hdmi->update) {
			ret = hdmi->phy.ops->init(hdmi, hdmi->phy.data, &hdmi->previous_mode);
			if (!ret)
				hdmi->disabled = false;
		}

		return ret;
	}

	if (hdmi->update)
		return 0;

	if (link_cfg->frl_lanes == 4)
		hdmi_modb(hdmi, OPMODE_FRL_4LANES, OPMODE_FRL_4LANES,
			  LINK_CONFIG0);
	else
		hdmi_modb(hdmi, 0, OPMODE_FRL_4LANES, LINK_CONFIG0);

	hdmi_modb(hdmi, 1, OPMODE_FRL, LINK_CONFIG0);

	ret = hdmi->phy.ops->init(hdmi, hdmi->phy.data, &hdmi->previous_mode);
	if (!ret) {
		hdmi->disabled = false;
		/* wait phy output stable then start flt */
		msleep(50);
	}

	return ret;
}

static unsigned long
hdmi_get_tmdsclock(struct dw_hdmi_qp *hdmi, unsigned long mpixelclock)
{
	unsigned long tmdsclock = mpixelclock;
	unsigned int depth =
		hdmi_bus_fmt_color_depth(hdmi->hdmi_data.enc_out_bus_format);

	if (!hdmi_bus_fmt_is_yuv422(hdmi->hdmi_data.enc_out_bus_format)) {
		switch (depth) {
		case 16:
			tmdsclock = mpixelclock * 2;
			break;
		case 12:
			tmdsclock = mpixelclock * 3 / 2;
			break;
		case 10:
			tmdsclock = mpixelclock * 5 / 4;
			break;
		default:
			break;
		}
	}

	return tmdsclock;
}

static ssize_t hdcp_ddc_read(struct i2c_adapter *adapter, u8 address,
			     u8 offset, void *buffer)
{
	int ret;
	struct i2c_msg msgs[2] = {
		{
			.addr = address,
			.flags = 0,
			.len = 1,
			.buf = &offset,
		}, {
			.addr = address,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = buffer,
		}
	};

	ret = i2c_transfer(adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;
	if (ret != ARRAY_SIZE(msgs))
		return -EPROTO;

	return 0;
}

static bool is_sink_hdcp2_supported(struct dw_hdmi_qp *hdmi)
{
	u8 bcaps;
	int ret;

	ret = hdcp_ddc_read(hdmi->ddc, HDMI_HDCP_ADDR, HDMI_HDCP2_VERSION, &bcaps);
	if (ret < 0)
		dev_err(hdmi->dev, "get hdcp2.x capable failed:%d\n", ret);

	if (bcaps & HDMI_HDCP2_SUPPORT)
		return true;

	return false;
}

static void dw_hdmi_qp_hdcp_enable(struct dw_hdmi_qp *hdmi,
				   const struct drm_connector_state *conn_state)
{
	void *data = hdmi->plat_data->phy_data;

	hdmi_writel(hdmi, 0, HDCP2LOGIC_ESM_GPIO_IN);

	if (conn_state->content_protection != DRM_MODE_CONTENT_PROTECTION_DESIRED)
		return;

	/* sink support hdcp2.x */
	if (is_sink_hdcp2_supported(hdmi)) {
		if (hdmi->plat_data->set_hdcp2_enable)
			hdmi->plat_data->set_hdcp2_enable(data, true);

		hdmi_modb(hdmi, 0, HDCP2_BYPASS, HDCP2LOGIC_CONFIG0);
		hdmi_writel(hdmi, HDCP2_ESM_P0_GPIO_OUT_2_CHG_IRQ, AVP_3_INT_CLEAR);
		hdmi_modb(hdmi, HDCP2_ESM_P0_GPIO_OUT_2_CHG_IRQ,
			  HDCP2_ESM_P0_GPIO_OUT_2_CHG_IRQ, AVP_3_INT_MASK_N);
		hdmi_writel(hdmi, 0x35, HDCP2LOGIC_ESM_GPIO_IN);
	} else {
		if (hdmi->hdcp && hdmi->hdcp->hdcp_start)
			hdmi->hdcp->hdcp_start(hdmi->hdcp);
	}
}

static void dw_hdmi_qp_flt_work(struct work_struct *p_work)
{
	struct dw_hdmi_qp *hdmi = container_of(p_work, struct dw_hdmi_qp, flt_work);
	void *data = hdmi->plat_data->phy_data;
	struct dw_hdmi_link_config *link_cfg = hdmi->plat_data->get_link_cfg(data);
	u8 frl_rate;
	int state = LTS1;

	if (hdmi->frl_switch)
		return;

	frl_rate = link_cfg->frl_lanes * link_cfg->rate_per_lane;

	while (1) {
		switch (state) {
		case LTS1:
			state = dw_hdmi_qp_flt_lts1(hdmi);
			break;
		case LTS2:
			state = dw_hdmi_qp_flt_lts2(hdmi, frl_rate);
			break;
		case LTS3:
			state = dw_hdmi_qp_flt_lts3(hdmi, frl_rate);
			break;
		case LTS4:
			state = dw_hdmi_qp_flt_lts4(hdmi, &frl_rate);
			break;
		case LTSP:
			state = dw_hdmi_qp_flt_ltsp(hdmi);
			break;
		case LTSL:
			state = dw_hdmi_qp_flt_ltsl(hdmi);
			break;
		default:
			dev_err(hdmi->dev, "flt failed\n");
		}

		if (state <= 0)
			break;
	}
}

static int dw_hdmi_qp_setup(struct dw_hdmi_qp *hdmi,
			    const struct drm_connector *connector,
			    struct drm_display_mode *mode)
{
	void *data = hdmi->plat_data->phy_data;
	struct hdmi_vmode_qp *vmode = &hdmi->hdmi_data.video_mode;
	struct dw_hdmi_link_config *link_cfg;
	u8 bytes = 0;

	hdmi->vic = drm_match_cea_mode(mode);
	if (!hdmi->vic)
		dev_dbg(hdmi->dev, "Non-CEA mode used in HDMI\n");
	else
		dev_dbg(hdmi->dev, "CEA mode used vic=%d\n", hdmi->vic);

	if (hdmi->plat_data->get_enc_out_encoding)
		hdmi->hdmi_data.enc_out_encoding =
			hdmi->plat_data->get_enc_out_encoding(data);
	else if ((hdmi->vic == 6) || (hdmi->vic == 7) ||
		 (hdmi->vic == 21) || (hdmi->vic == 22) ||
		 (hdmi->vic == 2) || (hdmi->vic == 3) ||
		 (hdmi->vic == 17) || (hdmi->vic == 18))
		hdmi->hdmi_data.enc_out_encoding = V4L2_YCBCR_ENC_601;
	else
		hdmi->hdmi_data.enc_out_encoding = V4L2_YCBCR_ENC_709;

	if (mode->flags & DRM_MODE_FLAG_DBLCLK) {
		hdmi->hdmi_data.video_mode.mpixelrepetitionoutput = 1;
		hdmi->hdmi_data.video_mode.mpixelrepetitioninput = 1;
	} else {
		hdmi->hdmi_data.video_mode.mpixelrepetitionoutput = 0;
		hdmi->hdmi_data.video_mode.mpixelrepetitioninput = 0;
	}

	/*  Get input format from plat data or fallback to RGB888 */
	if (hdmi->plat_data->get_input_bus_format)
		hdmi->hdmi_data.enc_in_bus_format =
			hdmi->plat_data->get_input_bus_format(data);
	else if (hdmi->plat_data->input_bus_format)
		hdmi->hdmi_data.enc_in_bus_format =
			hdmi->plat_data->input_bus_format;
	else
		hdmi->hdmi_data.enc_in_bus_format = MEDIA_BUS_FMT_RGB888_1X24;

	/* Default to RGB888 output format */
	if (hdmi->plat_data->get_output_bus_format)
		hdmi->hdmi_data.enc_out_bus_format =
			hdmi->plat_data->get_output_bus_format(data);
	else
		hdmi->hdmi_data.enc_out_bus_format = MEDIA_BUS_FMT_RGB888_1X24;

	if (hdmi->plat_data->set_prev_bus_format)
		hdmi->plat_data->set_prev_bus_format(data, hdmi->hdmi_data.enc_out_bus_format);

	/* Get input encoding from plat data or fallback to none */
	if (hdmi->plat_data->get_enc_in_encoding)
		hdmi->hdmi_data.enc_in_encoding =
			hdmi->plat_data->get_enc_in_encoding(data);
	else if (hdmi->plat_data->input_bus_encoding)
		hdmi->hdmi_data.enc_in_encoding =
			hdmi->plat_data->input_bus_encoding;
	else
		hdmi->hdmi_data.enc_in_encoding = V4L2_YCBCR_ENC_DEFAULT;

	if (hdmi->plat_data->get_quant_range)
		hdmi->hdmi_data.quant_range =
			hdmi->plat_data->get_quant_range(data);
	else
		hdmi->hdmi_data.quant_range = HDMI_QUANTIZATION_RANGE_DEFAULT;

	if (hdmi->plat_data->get_link_cfg)
		link_cfg = hdmi->plat_data->get_link_cfg(data);
	else
		return -EINVAL;

	hdmi->phy.ops->set_mode(hdmi, hdmi->phy.data, HDMI_MODE_FRL_MASK,
				link_cfg->frl_mode);

	if (!hdmi->update && !hdmi->frl_switch && hdmi->plat_data->link_clk_set)
		hdmi->plat_data->link_clk_set(data, true);

	/*
	 * According to the dw-hdmi specification 6.4.2
	 * vp_pr_cd[3:0]:
	 * 0000b: No pixel repetition (pixel sent only once)
	 * 0001b: Pixel sent two times (pixel repeated once)
	 */
	hdmi->hdmi_data.pix_repet_factor =
		(mode->flags & DRM_MODE_FLAG_DBLCLK) ? 1 : 0;
	hdmi->hdmi_data.video_mode.mdataenablepolarity = true;

	vmode->previous_pixelclock = vmode->mpixelclock;
	vmode->mpixelclock = mode->crtc_clock * 1000;
	if ((mode->flags & DRM_MODE_FLAG_3D_MASK) == DRM_MODE_FLAG_3D_FRAME_PACKING)
		vmode->mpixelclock *= 2;
	dev_dbg(hdmi->dev, "final pixclk = %ld\n", vmode->mpixelclock);
	vmode->previous_tmdsclock = vmode->mtmdsclock;
	vmode->mtmdsclock = hdmi_get_tmdsclock(hdmi, vmode->mpixelclock);
	if (hdmi_bus_fmt_is_yuv420(hdmi->hdmi_data.enc_out_bus_format))
		vmode->mtmdsclock /= 2;
	dev_info(hdmi->dev, "final tmdsclk = %d\n", vmode->mtmdsclock);

	if (hdmi->plat_data->set_grf_cfg)
		hdmi->plat_data->set_grf_cfg(data);

	if (hdmi->sink_has_audio) {
		dev_dbg(hdmi->dev, "sink has audio support\n");

		/* HDMI Initialization Step E - Configure audio */
		hdmi_clk_regenerator_update_pixel_clock(hdmi);
		hdmi_enable_audio_clk(hdmi, hdmi->audio_enable);
	}

	/* not for DVI mode */
	if (hdmi->sink_is_hdmi) {
		int ret;

		dev_dbg(hdmi->dev, "%s HDMI mode\n", __func__);
		hdmi_modb(hdmi, 0, OPMODE_DVI, LINK_CONFIG0);
		hdmi_modb(hdmi, HDCP2_BYPASS, HDCP2_BYPASS, HDCP2LOGIC_CONFIG0);
		hdmi_modb(hdmi, KEEPOUT_REKEY_ALWAYS, KEEPOUT_REKEY_CFG, FRAME_COMPOSER_CONFIG9);

		if (!link_cfg->frl_mode && dw_hdmi_support_scdc(hdmi, &connector->display_info) &&
		    !hdmi->update) {
			if (vmode->mtmdsclock > HDMI14_MAX_TMDSCLK) {
				drm_scdc_readb(hdmi->ddc, SCDC_SINK_VERSION, &bytes);
				drm_scdc_writeb(hdmi->ddc, SCDC_SOURCE_VERSION,
						min_t(u8, bytes, SCDC_MIN_SOURCE_VERSION));
				drm_scdc_set_high_tmds_clock_ratio(hdmi->ddc, 1);
				drm_scdc_set_scrambling(hdmi->ddc, 1);
				hdmi_writel(hdmi, 1, SCRAMB_CONFIG0);
				/* Wait for resuming transmission of TMDS clock and data */
				msleep(100);
			} else {
				drm_scdc_set_high_tmds_clock_ratio(hdmi->ddc, 0);
				drm_scdc_set_scrambling(hdmi->ddc, 0);
				hdmi_writel(hdmi, 0, SCRAMB_CONFIG0);
			}
		}
		/* HDMI Initialization Step F - Configure AVI InfoFrame */
		hdmi_config_AVI(hdmi, connector, mode);
		hdmi_config_vendor_specific_infoframe(hdmi, connector, mode);
		hdmi_config_CVTEM(hdmi);
		hdmi_config_drm_infoframe(hdmi, connector);
		ret = hdmi_set_op_mode(hdmi, link_cfg, connector);
		if (ret) {
			dev_err(hdmi->dev, "%s hdmi set operation mode failed\n", __func__);
			hdmi->frl_switch = false;
			return ret;
		}
	} else {
		hdmi_modb(hdmi, HDCP2_BYPASS, HDCP2_BYPASS, HDCP2LOGIC_CONFIG0);
		hdmi_modb(hdmi, OPMODE_DVI, OPMODE_DVI, LINK_CONFIG0);
		hdmi_writel(hdmi, 2, PKTSCHED_PKT_CONTROL0);
		hdmi_modb(hdmi, PKTSCHED_GCP_TX_EN, PKTSCHED_GCP_TX_EN, PKTSCHED_PKT_EN);
		hdmi->phy.ops->init(hdmi, hdmi->phy.data, &hdmi->previous_mode);
		dev_info(hdmi->dev, "%s DVI mode\n", __func__);
	}

	hdmi->frl_switch = false;
	return 0;
}

static enum drm_connector_status
dw_hdmi_connector_detect(struct drm_connector *connector, bool force)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);
	struct dw_hdmi_qp *secondary = NULL;
	enum drm_connector_status result, result_secondary;

	mutex_lock(&hdmi->mutex);
	hdmi->force = DRM_FORCE_UNSPECIFIED;
	mutex_unlock(&hdmi->mutex);

	if (hdmi->panel || hdmi->force_kernel_output) {
		result = connector_status_connected;
		goto out;
	}

	if (hdmi->next_bridge && hdmi->next_bridge->ops & DRM_BRIDGE_OP_DETECT) {
		result = drm_bridge_detect(hdmi->next_bridge);
		goto out;
	}

	if (hdmi->plat_data->left)
		secondary = hdmi->plat_data->left;
	else if (hdmi->plat_data->right)
		secondary = hdmi->plat_data->right;

	result = hdmi->phy.ops->read_hpd(hdmi, hdmi->phy.data);

	if (secondary) {
		result_secondary = secondary->phy.ops->read_hpd(secondary, secondary->phy.data);
		if (result == connector_status_connected &&
		    result_secondary == connector_status_connected)
			result = connector_status_connected;
		else
			result = connector_status_disconnected;
	}

out:
	if (result == connector_status_connected) {
		extcon_set_state_sync(hdmi->extcon, EXTCON_DISP_HDMI, true);
		handle_plugged_change(hdmi, true);
	} else {
		if (!hdmi->next_bridge)
			drm_connector_update_edid_property(&hdmi->connector, NULL);
		extcon_set_state_sync(hdmi->extcon, EXTCON_DISP_HDMI, false);
		handle_plugged_change(hdmi, false);
	}

	return result;
}

static int
dw_hdmi_update_hdr_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct dw_hdmi_qp *hdmi = container_of(connector, struct dw_hdmi_qp,
					       connector);
	void *data = hdmi->plat_data->phy_data;
	const struct hdr_static_metadata *metadata =
		&connector->hdr_sink_metadata.hdmi_type1;
	size_t size = sizeof(*metadata);
	struct drm_property *property;
	struct drm_property_blob *blob;
	int ret;

	if (hdmi->plat_data->get_hdr_property)
		property = hdmi->plat_data->get_hdr_property(data);
	else
		return -EINVAL;

	if (hdmi->plat_data->get_hdr_blob)
		blob = hdmi->plat_data->get_hdr_blob(data);
	else
		return -EINVAL;

	ret = drm_property_replace_global_blob(dev, &blob, size, metadata,
					       &connector->base, property);
	return ret;
}

static bool dw_hdmi_qp_check_output_type_changed(struct dw_hdmi_qp *hdmi)
{
	bool sink_hdmi;

	sink_hdmi = hdmi->sink_is_hdmi;

	if (hdmi->force_output == 1)
		hdmi->sink_is_hdmi = true;
	else if (hdmi->force_output == 2)
		hdmi->sink_is_hdmi = false;
	else
		hdmi->sink_is_hdmi = hdmi->support_hdmi;

	if (sink_hdmi != hdmi->sink_is_hdmi)
		return true;

	return false;
}

static int dw_hdmi_connector_get_modes(struct drm_connector *connector)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);
	struct hdr_static_metadata *metedata =
		&connector->hdr_sink_metadata.hdmi_type1;
	const struct edid *edid = NULL;
	const struct drm_edid *drm_edid;
	struct drm_display_mode *mode;
	struct drm_display_info *info = &connector->display_info;
	void *data = hdmi->plat_data->phy_data;
	struct drm_property_blob *edid_blob_ptr = connector->edid_blob_ptr;
	int i, ret = 0;

	if (hdmi->force_kernel_output) {
		mode = hdmi->plat_data->get_force_timing(data);
		hdmi->support_hdmi = true;
		hdmi->sink_is_hdmi = true;
		hdmi->sink_has_audio = true;
		mode = drm_mode_duplicate(connector->dev, mode);
		drm_mode_debug_printmodeline(mode);
		drm_mode_probed_add(connector, mode);
		info->edid_hdmi_rgb444_dc_modes = 0;
		info->edid_hdmi_ycbcr444_dc_modes = 0;
		info->hdmi.y420_dc_modes = 0;
		info->color_formats = 0;

		return 1;
	}

	if (hdmi->plat_data->right && hdmi->plat_data->right->next_bridge) {
		struct drm_bridge *bridge = hdmi->plat_data->right->next_bridge;

		if (bridge->ops & DRM_BRIDGE_OP_MODES) {
			if (!drm_bridge_get_modes(bridge, connector))
				return 0;
		}
	}

	if (hdmi->panel) {
		hdmi->support_hdmi = true;
		hdmi->sink_is_hdmi = true;
		hdmi->sink_has_audio = true;
		info->hdmi.scdc.supported = true;
		info->hdmi.scdc.scrambling.supported = true;
		info->max_tmds_clock = HDMI20_MAX_TMDSCLK_KHZ;
		return drm_panel_get_modes(hdmi->panel, connector);
	}

	if (hdmi->next_bridge && hdmi->next_bridge->ops & DRM_BRIDGE_OP_MODES)
		return drm_bridge_get_modes(hdmi->next_bridge, connector);

	if (!hdmi->ddc)
		return 0;

	memset(metedata, 0, sizeof(*metedata));
	memset(hdmi->vendor_info, 0, VENDOR_INFO_LEN);

	if (edid_blob_ptr && edid_blob_ptr->length)
		drm_edid = drm_edid_alloc(edid_blob_ptr->data, edid_blob_ptr->length);
	else
		drm_edid = drm_edid_read_ddc(connector, hdmi->ddc);

	if (drm_edid)
		edid = drm_edid_raw(drm_edid);

	if (edid) {
		u8 *raw_edid = (u8 *)edid;

		dev_dbg(hdmi->dev, "got edid: width[%d] x height[%d]\n",
			edid->width_cm, edid->height_cm);

		hdmi->support_hdmi = drm_detect_hdmi_monitor(edid);
		hdmi->sink_has_audio = drm_detect_monitor_audio(edid);
		if (hdmi->cec_notifier)
			cec_notifier_set_phys_addr_from_edid(hdmi->cec_notifier, edid);
		if (hdmi->plat_data->get_edid_hdmi21_info)
			hdmi->plat_data->get_edid_hdmi21_info(data, edid);
		memcpy(hdmi->vendor_info, &raw_edid[8], VENDOR_INFO_LEN);
		ret = drm_edid_connector_update(connector, drm_edid);
		if (hdmi->plat_data->get_dovi_data)
			hdmi->plat_data->get_dovi_data(data, edid, connector);
		if (hdmi->plat_data->get_colorimetry)
			hdmi->plat_data->get_colorimetry(data, edid);
		if (hdmi->plat_data->get_yuv422_format)
			hdmi->plat_data->get_yuv422_format(connector, edid);
		if (hdmi->plat_data->get_hdr10_plus_vsdb)
			hdmi->plat_data->get_hdr10_plus_vsdb(data, edid, connector);
		dw_hdmi_update_hdr_property(connector);
		if (ret > 0 && hdmi->plat_data->split_mode) {
			struct dw_hdmi_qp *secondary = NULL;
			void *secondary_data;

			if (hdmi->plat_data->left)
				secondary = hdmi->plat_data->left;
			else if (hdmi->plat_data->right)
				secondary = hdmi->plat_data->right;

			if (!secondary) {
				kfree(edid);
				kfree(drm_edid);
				return -ENOMEM;
			}
			secondary_data = secondary->plat_data->phy_data;

			secondary->sink_is_hdmi = drm_detect_hdmi_monitor(edid);
			secondary->sink_has_audio = drm_detect_monitor_audio(edid);
			if (secondary->cec_notifier)
				cec_notifier_set_phys_addr_from_edid(secondary->cec_notifier,
								     edid);
			if (secondary->plat_data->get_edid_hdmi21_info)
				secondary->plat_data->get_edid_hdmi21_info(secondary_data, edid);
		}
		kfree(edid);
		kfree(drm_edid);
	} else {
		hdmi->support_hdmi = true;
		hdmi->sink_is_hdmi = true;
		hdmi->sink_has_audio = true;

		if (hdmi->plat_data->split_mode) {
			if (hdmi->plat_data->left) {
				hdmi->plat_data->left->sink_is_hdmi = true;
				hdmi->plat_data->left->sink_has_audio = true;
			} else if (hdmi->plat_data->right) {
				hdmi->plat_data->right->sink_is_hdmi = true;
				hdmi->plat_data->right->sink_has_audio = true;
			}
		}

		for (i = 0; i < ARRAY_SIZE(dw_hdmi_default_modes); i++) {
			const struct drm_display_mode *ptr =
				&dw_hdmi_default_modes[i];

			mode = drm_mode_duplicate(connector->dev, ptr);
			if (mode) {
				if (!i)
					mode->type = DRM_MODE_TYPE_PREFERRED;
				drm_mode_probed_add(connector, mode);
				ret++;
			}
		}

		info->edid_hdmi_rgb444_dc_modes = 0;
		info->edid_hdmi_ycbcr444_dc_modes = 0;
		info->hdmi.y420_dc_modes = 0;
		info->color_formats = 0;
		info->hdmi.scdc.supported = true;
		info->hdmi.scdc.scrambling.supported = true;
		info->max_tmds_clock = HDMI20_MAX_TMDSCLK_KHZ;

		dev_info(hdmi->dev, "failed to get edid\n");
	}
	if (ret > 0 && (hdmi->plat_data->split_mode || hdmi->plat_data->dual_connector_split)) {
		struct drm_display_mode *mode;

		list_for_each_entry(mode, &connector->probed_modes, head)
			hdmi->plat_data->convert_to_split_mode(mode);
	}

	return ret;
}

void dw_hdmi_qp_set_allm_enable(struct dw_hdmi_qp *hdmi, bool enable)
{
	struct dw_hdmi_link_config *link_cfg = NULL;
	void *data;

	if (!hdmi || !hdmi->curr_conn)
		return;

	data = hdmi->plat_data->phy_data;

	if (hdmi->plat_data->get_link_cfg)
		link_cfg = hdmi->plat_data->get_link_cfg(data);

	if (!link_cfg)
		return;

	if (enable == hdmi->allm_enable)
		return;

	hdmi->allm_enable = enable;

	if (enable && !link_cfg->allm_supported) {
		hdmi->allm_enable = false;
		dev_err(hdmi->dev, "sink don't support allm, allm won't be enabled\n");
		return;
	}

	hdmi_config_vendor_specific_infoframe(hdmi, hdmi->curr_conn, &hdmi->previous_mode);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_allm_enable);

/*
 * If userspace don't disable hdmi output when hdmi plug out,
 * turn off hdmi signal output. Recovering hdmi status that
 * before plug out via turn on hdmi signal output and do scdc
 * communication when hdmi plug in.
 */
void dw_hdmi_qp_handle_hpd(struct dw_hdmi_qp *hdmi, bool enable)
{
	bool is_hdmi14 = false;

	mutex_lock(&hdmi->mutex);
	/* hdmi2.1 don't support keep vop output in current version */
	if (hdmi->hdmi_data.video_mode.mtmdsclock > 600000000)
		goto out;

	mutex_lock(&hdmi->audio_mutex);
	if (!hdmi->dclk_en)
		goto err_dclk;

	if (!enable && !hdmi->disabled) {
		hdmi_writel(hdmi, 1, PKTSCHED_PKT_CONTROL0);
		hdmi_modb(hdmi, PKTSCHED_GCP_TX_EN, PKTSCHED_GCP_TX_EN, PKTSCHED_PKT_EN);
		msleep(50);
		hdmi->phy.ops->disable(hdmi, hdmi->phy.data);
		hdmi->disabled = true;
		goto err_dclk;
	}

	if (hdmi->hdmi_data.video_mode.mtmdsclock <= 340000000)
		is_hdmi14 = true;

	if (enable && hdmi->disabled) {
		if (!is_hdmi14) {
			drm_scdc_set_high_tmds_clock_ratio(hdmi->ddc, 1);
			drm_scdc_set_scrambling(hdmi->ddc, 1);
			hdmi_writel(hdmi, 1, SCRAMB_CONFIG0);
			/* Wait for resuming transmission of TMDS clock and data */
			msleep(100);
		} else {
			drm_scdc_set_high_tmds_clock_ratio(hdmi->ddc, 0);
			drm_scdc_set_scrambling(hdmi->ddc, 0);
			hdmi_writel(hdmi, 0, SCRAMB_CONFIG0);
		}

		hdmi->phy.ops->init(hdmi, hdmi->phy.data, &hdmi->previous_mode);
		hdmi->disabled = false;
		msleep(50);
		hdmi_writel(hdmi, 2, PKTSCHED_PKT_CONTROL0);
		hdmi_modb(hdmi, PKTSCHED_GCP_TX_EN, PKTSCHED_GCP_TX_EN, PKTSCHED_PKT_EN);
	}

err_dclk:
	mutex_unlock(&hdmi->audio_mutex);
out:
	mutex_unlock(&hdmi->mutex);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_handle_hpd);

static int
dw_hdmi_atomic_connector_set_property(struct drm_connector *connector,
				      struct drm_connector_state *state,
				      struct drm_property *property,
				      uint64_t val)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);
	const struct dw_hdmi_property_ops *ops = hdmi->plat_data->property_ops;

	if (ops && ops->set_property)
		return ops->set_property(connector, state, property,
					 val, hdmi->plat_data->phy_data);
	else
		return -EINVAL;
}

static int
dw_hdmi_atomic_connector_get_property(struct drm_connector *connector,
				      const struct drm_connector_state *state,
				      struct drm_property *property,
				      uint64_t *val)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);
	const struct dw_hdmi_property_ops *ops = hdmi->plat_data->property_ops;

	if (ops && ops->get_property)
		return ops->get_property(connector, state, property,
					 val, hdmi->plat_data->phy_data);
	else
		return -EINVAL;
}

static int
dw_hdmi_connector_set_property(struct drm_connector *connector,
			       struct drm_property *property, uint64_t val)
{
	return dw_hdmi_atomic_connector_set_property(connector, NULL,
						     property, val);
}

static u64 dw_hdmi_qp_get_avi_color_fmt(struct dw_hdmi_qp *hdmi, u32 depth)
{
	u32 fmt;
	u64 bus_format;

	fmt = (hdmi_readl(hdmi, PKT_AVI_CONTENTS1) >> 13) & 0x3;

	switch (fmt) {
	case DW_HDMI_QP_YUV444:
		if (!depth)
			bus_format = MEDIA_BUS_FMT_YUV8_1X24;
		else
			bus_format = MEDIA_BUS_FMT_YUV10_1X30;
		break;
	case DW_HDMI_QP_YUV422:
		bus_format = MEDIA_BUS_FMT_YUYV10_1X20;
		break;
	case DW_HDMI_QP_YUV420:
		if (!depth)
			bus_format = MEDIA_BUS_FMT_UYYVYY8_0_5X24;
		else
			bus_format = MEDIA_BUS_FMT_UYYVYY10_0_5X30;
		break;
	case DW_HDMI_QP_RGB:
		if (!depth)
			bus_format = MEDIA_BUS_FMT_RGB888_1X24;
		else
			bus_format = MEDIA_BUS_FMT_RGB101010_1X30;
		break;
	default:
		dev_err(hdmi->dev, "can't get correct color format\n");
		bus_format = MEDIA_BUS_FMT_YUV8_1X24;
		break;
	}

	return bus_format;
}

static u64 dw_hdmi_qp_get_color_fmt(struct dw_hdmi_qp *hdmi)
{
	u32 fmt, depth;
	u64 bus_format;

	fmt = hdmi_readl(hdmi, VIDEO_INTERFACE_STATUS0);
	depth = fmt & IPI_COLOR_DEPTH_MASK;
	fmt = fmt & IPI_FORMAT_MASK;

	switch (fmt) {
	case DW_HDMI_QP_YUV444:
		if (!depth)
			bus_format = MEDIA_BUS_FMT_YUV8_1X24;
		else
			bus_format = MEDIA_BUS_FMT_YUV10_1X30;
		break;
	case DW_HDMI_QP_YUV422:
		bus_format = MEDIA_BUS_FMT_YUYV10_1X20;
		break;
	case DW_HDMI_QP_YUV420:
		if (!depth)
			bus_format = MEDIA_BUS_FMT_UYYVYY8_0_5X24;
		else
			bus_format = MEDIA_BUS_FMT_UYYVYY10_0_5X30;
		break;
	case DW_HDMI_QP_RGB:
		if (!depth)
			bus_format = MEDIA_BUS_FMT_RGB888_1X24;
		else
			bus_format = MEDIA_BUS_FMT_RGB101010_1X30;
		break;
	/*
	 * RK3588/RK3576 hdmi color format setting and dsc mode setting are
	 * the same mask in the same register. If dsc is enabled for hdmi in
	 * uboot, what color is hdmi output in uboot that cannot be obtained
	 * from this register in kernel.
	 * Since hdmi in dsc mode always sends avi infoframe, color format
	 * of hdmi output in uboot can be read from the avi register.
	 */
	case DW_HDMI_QP_DSC:
		bus_format = dw_hdmi_qp_get_avi_color_fmt(hdmi, depth);
		break;
	default:
		dev_err(hdmi->dev, "can't get correct color format\n");
		bus_format = MEDIA_BUS_FMT_YUV8_1X24;
		break;
	}

	return bus_format;
}

static void dw_hdmi_attach_properties(struct dw_hdmi_qp *hdmi)
{
	u32 val;
	u64 color = MEDIA_BUS_FMT_YUV8_1X24;
	const struct dw_hdmi_property_ops *ops =
				hdmi->plat_data->property_ops;
	enum drm_connector_status connect_status =
		hdmi->phy.ops->read_hpd(hdmi, hdmi->phy.data);

	if ((connect_status == connector_status_connected) &&
	    hdmi->initialized) {
		color = dw_hdmi_qp_get_color_fmt(hdmi);

		val = (hdmi_readl(hdmi, PKT_VSI_CONTENTS1) >> 8) & 0xffffff;
		if (val == HDMI_FORUM_OUI)
			hdmi->allm_enable = true;
		else
			hdmi->allm_enable = false;
	}

	/*
	 * Because all hdmi registers are configured the same value
	 * between yuv422 8/10 bit. We set a useless bit in uboot to mark
	 * yuv422 10bit.
	 */
	if (color == MEDIA_BUS_FMT_YUYV10_1X20 &&
	    !(hdmi_readl(hdmi, VIDEO_INTERFACE_CONFIG0) & BIT(20)))
		color = MEDIA_BUS_FMT_YUYV8_1X16;

	if (ops && ops->attach_properties)
		return ops->attach_properties(&hdmi->connector, color, 0,
					      hdmi->plat_data->phy_data, hdmi->allm_enable);
}

static void dw_hdmi_destroy_properties(struct dw_hdmi_qp *hdmi)
{
	const struct dw_hdmi_property_ops *ops =
				hdmi->plat_data->property_ops;

	if (ops && ops->destroy_properties)
		return ops->destroy_properties(&hdmi->connector,
					       hdmi->plat_data->phy_data);
}

static struct drm_encoder *
dw_hdmi_connector_best_encoder(struct drm_connector *connector)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);

	return hdmi->bridge.encoder;
}

static bool dw_hdmi_color_changed(struct drm_connector *connector,
				  struct drm_atomic_state *state)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);
	void *data = hdmi->plat_data->phy_data;
	struct drm_connector_state *old_state =
		drm_atomic_get_old_connector_state(state, connector);
	struct drm_connector_state *new_state =
		drm_atomic_get_new_connector_state(state, connector);
	bool ret = false;

	if (hdmi->plat_data->get_color_changed)
		ret = hdmi->plat_data->get_color_changed(data);

	if (new_state->colorspace != old_state->colorspace)
		ret = true;

	return ret;
}

static bool hdr_metadata_equal(struct dw_hdmi_qp *hdmi, const struct drm_connector_state *old_state,
			       const struct drm_connector_state *new_state)
{
	struct drm_property_blob *old_blob = old_state->hdr_output_metadata;
	struct drm_property_blob *new_blob = new_state->hdr_output_metadata;
	int i, ret;
	u8 *data;

	hdmi->hdr2sdr = false;

	if (!old_blob && !new_blob)
		return true;

	if (!old_blob) {
		data = (u8 *)new_blob->data;

		for (i = 0; i < new_blob->length; i++)
			if (data[i])
				return false;

		return true;
	}

	if (!new_blob) {
		data = (u8 *)old_blob->data;

		for (i = 0; i < old_blob->length; i++)
			if (data[i])
				return false;

		return true;
	}

	if (old_blob->length != new_blob->length)
		return false;

	ret = !memcmp(old_blob->data, new_blob->data, old_blob->length);

	if (!ret && new_blob) {
		data = (u8 *)new_blob->data;

		for (i = 0; i < new_blob->length; i++)
			if (data[i])
				break;

		if (i == new_blob->length)
			hdmi->hdr2sdr = true;
	}

	return ret;
}

static bool check_hdr_color_change(struct drm_connector_state *old_state,
				   struct drm_connector_state *new_state,
				   struct dw_hdmi_qp *hdmi)
{
	void *data = hdmi->plat_data->phy_data;

	if (!hdr_metadata_equal(hdmi, old_state, new_state)) {
		hdmi->plat_data->check_hdr_color_change(new_state, data);
		return true;
	}

	return false;
}

static void dw_hdmi_qp_hdcp_disable(struct dw_hdmi_qp *hdmi,
				    const struct drm_connector_state *conn_state)
{
	void *data = hdmi->plat_data->phy_data;

	hdmi_writel(hdmi, 0, AVP_1_INT_MASK_N);
	hdmi_writel(hdmi, 0, AVP_3_INT_MASK_N);
	if (hdmi->hdcp && hdmi->hdcp->hdcp_stop)
		hdmi->hdcp->hdcp_stop(hdmi->hdcp);

	hdmi_writel(hdmi, 0x34, HDCP2LOGIC_ESM_GPIO_IN);
	hdmi_modb(hdmi, HDCP2_BYPASS, HDCP2_BYPASS, HDCP2LOGIC_CONFIG0);

	if (conn_state->content_protection != DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
		drm_hdcp_update_content_protection(hdmi->curr_conn,
						   DRM_MODE_CONTENT_PROTECTION_DESIRED);

	hdmi->hdcp_status = 0;
	if (hdmi->plat_data->set_hdcp_status)
		hdmi->plat_data->set_hdcp_status(data, hdmi->hdcp_status);
}

static void set_dw_hdmi_hdcp_enable(struct dw_hdmi_qp *hdmi,
				    struct drm_connector *conn,
				    struct drm_atomic_state *state)
{
	struct drm_connector_state *old_state, *new_state;
	u64 old_cp, new_cp;

	old_state = drm_atomic_get_old_connector_state(state, conn);
	new_state = drm_atomic_get_new_connector_state(state, conn);
	old_cp = old_state->content_protection;
	new_cp = new_state->content_protection;

	DRM_DEV_DEBUG_DRIVER(hdmi->dev, "old cp:%llu new cp:%llu\n", old_cp, new_cp);
	if (old_cp != new_cp) {
		if (new_cp == DRM_MODE_CONTENT_PROTECTION_DESIRED &&
		    old_cp == DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
			dw_hdmi_qp_hdcp_enable(hdmi, new_state);
		else if (new_cp == DRM_MODE_CONTENT_PROTECTION_UNDESIRED &&
			 old_cp != DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
			dw_hdmi_qp_hdcp_disable(hdmi, new_state);
	}
}

static bool dovi_vsif_equal(struct dw_hdmi_qp *hdmi)
{
	void *data = hdmi->plat_data->phy_data;
	int ret;
	u32 vsif[DOVI_VSIF_LEN] = {0};

	if (!hdmi->plat_data->get_dovi_vsif)
		return true;

	hdmi->plat_data->get_dovi_vsif(data, vsif);

	ret = memcmp(hdmi->dovi_vsif, vsif, DOVI_VSIF_LEN * 4);

	if (ret) {
		memcpy(hdmi->dovi_vsif, vsif, DOVI_VSIF_LEN * 4);
		return false;
	} else {
		return true;
	}
}

static int dw_hdmi_connector_atomic_check(struct drm_connector *connector,
					  struct drm_atomic_state *state)
{
	struct drm_connector_state *old_state =
		drm_atomic_get_old_connector_state(state, connector);
	struct drm_connector_state *new_state =
		drm_atomic_get_new_connector_state(state, connector);
	struct drm_crtc *crtc = new_state->crtc;
	struct drm_crtc *old_crtc = old_state->crtc;
	struct drm_crtc_state *crtc_state, *old_crtc_state;
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);
	struct drm_display_mode mode;
	void *data = hdmi->plat_data->phy_data;
	struct hdmi_vmode_qp *vmode = &hdmi->hdmi_data.video_mode;

	if (old_crtc) {
		old_crtc_state = drm_atomic_get_crtc_state(state, old_crtc);
		if (IS_ERR(old_crtc_state))
			return PTR_ERR(old_crtc_state);

		if (hdmi->plat_data->get_vp_id)
			hdmi->old_vp_id = hdmi->plat_data->get_vp_id(old_crtc_state);
	}

	if (!crtc)
		return 0;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	if (hdmi->plat_data->get_vp_id)
		hdmi->vp_id = hdmi->plat_data->get_vp_id(crtc_state);

	drm_mode_copy(&mode, &crtc_state->mode);
	/*
	 * If HDMI is enabled in uboot, it's need to record
	 * drm_display_mode and set phy status to enabled.
	 */
	if (!vmode->mpixelclock) {
		struct dw_hdmi_qp *secondary = NULL;
		u8 val;

		if (hdmi->plat_data->left)
			secondary = hdmi->plat_data->left;
		else if (hdmi->plat_data->right)
			secondary = hdmi->plat_data->right;
		hdmi->curr_conn = connector;
		if (secondary)
			secondary->curr_conn = connector;
		if (hdmi->plat_data->get_enc_in_encoding)
			hdmi->hdmi_data.enc_in_encoding =
				hdmi->plat_data->get_enc_in_encoding(data);
		if (hdmi->plat_data->get_enc_out_encoding)
			hdmi->hdmi_data.enc_out_encoding =
				hdmi->plat_data->get_enc_out_encoding(data);
		if (hdmi->plat_data->get_input_bus_format)
			hdmi->hdmi_data.enc_in_bus_format =
				hdmi->plat_data->get_input_bus_format(data);
		if (hdmi->plat_data->get_output_bus_format)
			hdmi->hdmi_data.enc_out_bus_format =
				hdmi->plat_data->get_output_bus_format(data);

		if (hdmi->plat_data->split_mode || hdmi->plat_data->dual_connector_split)
			hdmi->plat_data->convert_to_origin_mode(&mode);

		drm_mode_copy(&hdmi->previous_mode, &mode);
		vmode->mpixelclock = mode.crtc_clock * 1000;
		vmode->previous_pixelclock = mode.clock;
		vmode->previous_tmdsclock = mode.clock;
		vmode->mtmdsclock = hdmi_get_tmdsclock(hdmi,
						       vmode->mpixelclock);
		if (hdmi_bus_fmt_is_yuv420(hdmi->hdmi_data.enc_out_bus_format))
			vmode->mtmdsclock /= 2;

		/*
		 * If uboot logo enabled, atomic_enable won't be called,
		 * but atomic_disable will be called when hdmi plug out.
		 * That will cause dclk enable count is incorrect. So
		 * we should check ipi/link/video clk to determine whether
		 * uboot logo is enabled.
		 */
		if (hdmi->initialized && !hdmi->dclk_en) {
			mutex_lock(&hdmi->audio_mutex);
			if (hdmi->plat_data->dclk_set)
				hdmi->plat_data->dclk_set(data, true, hdmi->vp_id);
			hdmi->dclk_en = true;
			mutex_unlock(&hdmi->audio_mutex);
			hdmi->curr_conn = connector;
			extcon_set_state_sync(hdmi->extcon, EXTCON_DISP_HDMI, true);
		}

		val = 0;
		if (dw_hdmi_support_scdc(hdmi, &connector->display_info))
			drm_scdc_readb(hdmi->ddc, SCDC_TMDS_CONFIG, &val);
		/* if plug out before hdmi bind, reset hdmi */
		if (vmode->mtmdsclock >= 340000000 && vmode->mpixelclock <= 600000000 &&
		    !(val & SCDC_TMDS_BIT_CLOCK_RATIO_BY_40) && !hdmi->force_kernel_output)
			hdmi->logo_plug_out = true;
	}

	if (check_hdr_color_change(old_state, new_state, hdmi) || hdmi->logo_plug_out ||
	    dw_hdmi_color_changed(connector, state) ||
	    dw_hdmi_qp_check_output_type_changed(hdmi)) {
		u32 mtmdsclk;

		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state))
			return PTR_ERR(crtc_state);

		if (hdmi->plat_data->update_color_format)
			hdmi->plat_data->update_color_format(new_state, data);
		if (hdmi->plat_data->get_enc_in_encoding)
			hdmi->hdmi_data.enc_in_encoding =
				hdmi->plat_data->get_enc_in_encoding(data);
		if (hdmi->plat_data->get_enc_out_encoding)
			hdmi->hdmi_data.enc_out_encoding =
				hdmi->plat_data->get_enc_out_encoding(data);
		if (hdmi->plat_data->get_input_bus_format)
			hdmi->hdmi_data.enc_in_bus_format =
				hdmi->plat_data->get_input_bus_format(data);
		if (hdmi->plat_data->get_output_bus_format)
			hdmi->hdmi_data.enc_out_bus_format =
				hdmi->plat_data->get_output_bus_format(data);

		mtmdsclk = hdmi_get_tmdsclock(hdmi, mode.clock);
		if (hdmi_bus_fmt_is_yuv420(hdmi->hdmi_data.enc_out_bus_format))
			mtmdsclk /= 2;

		if (hdmi->hdmi_data.video_mode.mpixelclock == (mode.clock * 1000) &&
		    hdmi->hdmi_data.video_mode.mtmdsclock == (mtmdsclk * 1000) &&
		    mode.clock <= 600000 && !hdmi->disabled && !hdmi->logo_plug_out) {
			hdmi->update = true;
			hdmi_writel(hdmi, 1, PKTSCHED_PKT_CONTROL0);
			hdmi_modb(hdmi, PKTSCHED_GCP_TX_EN, PKTSCHED_GCP_TX_EN, PKTSCHED_PKT_EN);
			mdelay(50);
		} else if (!hdmi->disabled) {
			if (hdmi->previous_mode.clock > 600000 && mode.clock > 600000)
				hdmi->frl_switch = true;
			hdmi->update = false;
			crtc_state->mode_changed = true;
			hdmi->logo_plug_out = false;
		}
	}

	return 0;
}

static void dw_hdmi_connector_atomic_commit(struct drm_connector *connector,
					    struct drm_atomic_state *state)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);

	if (hdmi->update) {
		dw_hdmi_qp_setup(hdmi, hdmi->curr_conn, &hdmi->previous_mode);
		msleep(50);
		hdmi_writel(hdmi, 2, PKTSCHED_PKT_CONTROL0);
		hdmi->update = false;
	}

	if (!hdmi->disabled) {
		set_dw_hdmi_hdcp_enable(hdmi, connector, state);
		if (!dovi_vsif_equal(hdmi))
			hdmi_config_vendor_specific_infoframe(hdmi, hdmi->curr_conn,
							      &hdmi->previous_mode);
	}
}

void dw_hdmi_qp_set_quant_range(struct dw_hdmi_qp *hdmi)
{
	void *data = hdmi->plat_data->phy_data;

	if (hdmi->disabled)
		return;

	if (hdmi->plat_data->get_quant_range)
		hdmi->hdmi_data.quant_range =
			hdmi->plat_data->get_quant_range(data);

	hdmi_config_AVI(hdmi, hdmi->curr_conn, &hdmi->previous_mode);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_quant_range);

void dw_hdmi_qp_set_output_type(struct dw_hdmi_qp *hdmi, u64 val)
{
	hdmi->force_output = val;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_output_type);

bool dw_hdmi_qp_get_output_whether_hdmi(struct dw_hdmi_qp *hdmi)
{
	return hdmi->sink_is_hdmi;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_get_output_whether_hdmi);

int dw_hdmi_qp_get_output_type_cap(struct dw_hdmi_qp *hdmi)
{
	return hdmi->support_hdmi;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_get_output_type_cap);

static void dw_hdmi_connector_force(struct drm_connector *connector)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);

	mutex_lock(&hdmi->mutex);

	if (hdmi->force != connector->force) {
		if (!hdmi->disabled && connector->force == DRM_FORCE_OFF)
			extcon_set_state_sync(hdmi->extcon, EXTCON_DISP_HDMI,
					      false);
		else if (hdmi->disabled && connector->force == DRM_FORCE_ON)
			extcon_set_state_sync(hdmi->extcon, EXTCON_DISP_HDMI,
					      true);
	}

	hdmi->force = connector->force;
	mutex_unlock(&hdmi->mutex);
}

static int drm_hdmi_probe_single_connector_modes(struct drm_connector *connector,
						 uint32_t maxX, uint32_t maxY)
{
	struct dw_hdmi_qp *hdmi =
		container_of(connector, struct dw_hdmi_qp, connector);
	struct drm_display_info *info = &connector->display_info;
	void *data = hdmi->plat_data->phy_data;
	int ret;

	ret = drm_helper_probe_single_connector_modes(connector, maxX, maxY);

	if (hdmi->plat_data->get_mode_color_caps)
		hdmi->plat_data->get_mode_color_caps(connector, info, data);

	return ret;
}

static const struct drm_connector_funcs dw_hdmi_connector_funcs = {
	.fill_modes = drm_hdmi_probe_single_connector_modes,
	.detect = dw_hdmi_connector_detect,
	.destroy = drm_connector_cleanup,
	.force = dw_hdmi_connector_force,
	.reset = drm_atomic_helper_connector_reset,
	.set_property = dw_hdmi_connector_set_property,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.atomic_set_property = dw_hdmi_atomic_connector_set_property,
	.atomic_get_property = dw_hdmi_atomic_connector_get_property,
};

static const struct drm_connector_helper_funcs dw_hdmi_connector_helper_funcs = {
	.get_modes = dw_hdmi_connector_get_modes,
	.best_encoder = dw_hdmi_connector_best_encoder,
	.atomic_check = dw_hdmi_connector_atomic_check,
	.atomic_commit = dw_hdmi_connector_atomic_commit,
};

static int dw_hdmi_qp_bridge_attach(struct drm_bridge *bridge,
				    enum drm_bridge_attach_flags flags)
{
	struct dw_hdmi_qp *hdmi = bridge->driver_private;
	struct drm_encoder *encoder = bridge->encoder;
	struct drm_connector *connector = &hdmi->connector;
	bool skip_connector = false;

	if (hdmi->next_bridge) {
		struct drm_bridge *next_bridge = hdmi->next_bridge;
		int ret;

		ret = drm_bridge_attach(bridge->encoder, next_bridge, bridge,
					next_bridge->ops & DRM_BRIDGE_OP_MODES ?
					DRM_BRIDGE_ATTACH_NO_CONNECTOR : 0);
		if (ret) {
			DRM_ERROR("failed to attach next bridge: %d\n", ret);
			return ret;
		}

		skip_connector = !(next_bridge->ops & DRM_BRIDGE_OP_MODES);
	}

	hdmi->skip_connector = skip_connector;
	if (flags & DRM_BRIDGE_ATTACH_NO_CONNECTOR || skip_connector)
		return 0;

	connector->interlace_allowed = 1;
	connector->polled = DRM_CONNECTOR_POLL_HPD;
	if (hdmi->next_bridge && hdmi->next_bridge->ops & DRM_BRIDGE_OP_DETECT)
		connector->polled = DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT;
	drm_connector_helper_add(connector, &dw_hdmi_connector_helper_funcs);

	drm_connector_init(bridge->dev, connector, &dw_hdmi_connector_funcs,
			   DRM_MODE_CONNECTOR_HDMIA);

	drm_connector_attach_encoder(connector, encoder);
	dw_hdmi_attach_properties(hdmi);

	return 0;
}

static void dw_hdmi_qp_bridge_detach(struct drm_bridge *bridge)
{
	struct dw_hdmi_qp *hdmi = bridge->driver_private;

	if (hdmi->cec_notifier) {
		mutex_lock(&hdmi->cec_notifier_mutex);
		cec_notifier_conn_unregister(hdmi->cec_notifier);
		hdmi->cec_notifier = NULL;
		mutex_unlock(&hdmi->cec_notifier_mutex);
	}
}

static enum drm_mode_status
dw_hdmi_qp_bridge_mode_valid(struct drm_bridge *bridge,
			     const struct drm_display_info *info,
			     const struct drm_display_mode *mode)
{
	struct dw_hdmi_qp *hdmi = bridge->driver_private;
	const struct dw_hdmi_plat_data *pdata = hdmi->plat_data;

	if (hdmi->force_kernel_output)
		return MODE_OK;

	if (mode->clock <= 25000)
		return MODE_CLOCK_RANGE;

	if (!hdmi->sink_is_hdmi && mode->clock > 340000)
		return MODE_BAD;

	if (pdata->mode_valid)
		return pdata->mode_valid(NULL, pdata->priv_data, info,
					 mode);

	return MODE_OK;
}

static void dw_hdmi_qp_bridge_mode_set(struct drm_bridge *bridge,
				       const struct drm_display_mode *orig_mode,
				       const struct drm_display_mode *mode)
{
	struct dw_hdmi_qp *hdmi = bridge->driver_private;

	mutex_lock(&hdmi->mutex);

	if (!drm_mode_equal(orig_mode, mode))
		hdmi->frl_switch = false;
	/* Store the display mode for plugin/DKMS poweron events */
	drm_mode_copy(&hdmi->previous_mode, mode);
	if (hdmi->plat_data->split_mode || hdmi->plat_data->dual_connector_split)
		hdmi->plat_data->convert_to_origin_mode(&hdmi->previous_mode);

	mutex_unlock(&hdmi->mutex);
}

static void dw_hdmi_qp_bridge_atomic_disable(struct drm_bridge *bridge,
					     struct drm_bridge_state *old_state)
{
	struct dw_hdmi_qp *hdmi = bridge->driver_private;
	void *data = hdmi->plat_data->phy_data;
	const struct drm_connector_state *conn_state = hdmi->curr_conn->state;
	struct dw_hdmi_link_config *link_cfg = NULL;

	if (hdmi->panel)
		drm_panel_disable(hdmi->panel);

	if (hdmi->plat_data->get_link_cfg)
		link_cfg = hdmi->plat_data->get_link_cfg(data);

	if (!link_cfg) {
		dev_err(hdmi->dev, "link_cfg is null\n");
		return;
	}

	if (link_cfg->dsc_mode)
		hdmi->frl_switch = false;

	/* set avmute */
	hdmi_writel(hdmi, 1, PKTSCHED_PKT_CONTROL0);
	mdelay(50);

	dw_hdmi_qp_hdcp_disable(hdmi, conn_state);

	if (hdmi->plat_data->crtc_pre_disable)
		hdmi->plat_data->crtc_pre_disable(data, bridge->encoder->crtc);
	mutex_lock(&hdmi->mutex);

	if (hdmi->dclk_en) {
		mutex_lock(&hdmi->audio_mutex);
		if (hdmi->plat_data->dclk_set)
			hdmi->plat_data->dclk_set(data, false, hdmi->old_vp_id);
		hdmi->dclk_en = false;
		mutex_unlock(&hdmi->audio_mutex);
	};

	if (hdmi->phy.ops->disable && !hdmi->frl_switch) {
		hdmi_writel(hdmi, 0, FLT_CONFIG0);
		hdmi_writel(hdmi, 0, SCRAMB_CONFIG0);

		if (hdmi->plat_data->force_frl_rate)
			hdmi->plat_data->force_frl_rate(data, 0);

		hdmi->phy.ops->disable(hdmi, hdmi->phy.data);
		hdmi->disabled = true;
		if (hdmi->plat_data->link_clk_set)
			hdmi->plat_data->link_clk_set(data, false);
	}

	hdmi->curr_conn = NULL;
	hdmi->update = false;
	mutex_unlock(&hdmi->mutex);

	cancel_work_sync(&hdmi->flt_work);
	flush_workqueue(hdmi->workqueue);
	dw_hdmi_qp_flt_ltsl(hdmi);

	if (hdmi->panel)
		drm_panel_unprepare(hdmi->panel);
}

static void dw_hdmi_qp_bridge_atomic_enable(struct drm_bridge *bridge,
					    struct drm_bridge_state *old_state)
{
	struct dw_hdmi_qp *hdmi = bridge->driver_private;
	struct drm_atomic_state *state = old_state->base.state;
	struct drm_connector *connector;
	void *data = hdmi->plat_data->phy_data;
	struct dw_hdmi_link_config *link_cfg = NULL;

	if (hdmi->plat_data->get_link_cfg)
		link_cfg = hdmi->plat_data->get_link_cfg(data);

	if (hdmi->panel)
		drm_panel_prepare(hdmi->panel);

	connector = drm_atomic_get_new_connector_for_encoder(state,
							     bridge->encoder);

	mutex_lock(&hdmi->mutex);
	hdmi->curr_conn = connector;

	dw_hdmi_qp_setup(hdmi, hdmi->curr_conn, &hdmi->previous_mode);

	if ((link_cfg && !link_cfg->frl_mode) || hdmi->frl_switch) {
		hdmi_writel(hdmi, 2, PKTSCHED_PKT_CONTROL0);
		hdmi_modb(hdmi, PKTSCHED_GCP_TX_EN, PKTSCHED_GCP_TX_EN, PKTSCHED_PKT_EN);
	}
	mutex_unlock(&hdmi->mutex);

	if (!hdmi->dclk_en) {
		mutex_lock(&hdmi->audio_mutex);
		if (hdmi->plat_data->dclk_set)
			hdmi->plat_data->dclk_set(data, true, hdmi->vp_id);
		hdmi->dclk_en = true;
		mutex_unlock(&hdmi->audio_mutex);
	}

	if (link_cfg && link_cfg->frl_mode)
		queue_work(hdmi->workqueue, &hdmi->flt_work);

	if (hdmi->plat_data->crtc_post_enable)
		hdmi->plat_data->crtc_post_enable(data, bridge->encoder->crtc);

	dw_hdmi_qp_init_audio_infoframe(hdmi);
	dw_hdmi_qp_audio_enable(hdmi);
	hdmi_clk_regenerator_update_pixel_clock(hdmi);

	if (hdmi->panel)
		drm_panel_enable(hdmi->panel);

	dw_hdmi_qp_hdcp_enable(hdmi, hdmi->curr_conn->state);
}

static const struct drm_bridge_funcs dw_hdmi_bridge_funcs = {
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.attach = dw_hdmi_qp_bridge_attach,
	.detach = dw_hdmi_qp_bridge_detach,
	.mode_set = dw_hdmi_qp_bridge_mode_set,
	.mode_valid = dw_hdmi_qp_bridge_mode_valid,
	.atomic_enable = dw_hdmi_qp_bridge_atomic_enable,
	.atomic_disable = dw_hdmi_qp_bridge_atomic_disable,
};

void dw_hdmi_qp_set_cec_adap(struct dw_hdmi_qp *hdmi, struct cec_adapter *adap)
{
	hdmi->cec_adap = adap;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_set_cec_adap);

static irqreturn_t dw_hdmi_qp_main_hardirq(int irq, void *dev_id)
{
	struct dw_hdmi_qp *hdmi = dev_id;
	struct dw_hdmi_qp_i2c *i2c = hdmi->i2c;
	u32 stat;

	stat = hdmi_readl(hdmi, MAINUNIT_1_INT_STATUS);

	i2c->stat = stat & (I2CM_OP_DONE_IRQ | I2CM_READ_REQUEST_IRQ |
			    I2CM_NACK_RCVD_IRQ);
	hdmi->scdc_intr = stat & (SCDC_UPD_FLAGS_RD_IRQ |
				  SCDC_UPD_FLAGS_CHG_IRQ |
				  SCDC_UPD_FLAGS_CLR_IRQ |
				  SCDC_RR_REPLY_STOP_IRQ |
				  SCDC_NACK_RCVD_IRQ);
	hdmi->flt_intr = stat & (FLT_EXIT_TO_LTSP_IRQ |
				 FLT_EXIT_TO_LTS4_IRQ |
				 FLT_EXIT_TO_LTSL_IRQ);

	dev_dbg(hdmi->dev, "i2c main unit irq:%#x\n", stat);
	if (i2c->stat) {
		hdmi_writel(hdmi, i2c->stat, MAINUNIT_1_INT_CLEAR);
		complete(&i2c->cmp);
	}

	if (hdmi->flt_intr) {
		dev_dbg(hdmi->dev, "i2c flt irq:%#x\n", hdmi->flt_intr);
		hdmi_writel(hdmi, hdmi->flt_intr, MAINUNIT_1_INT_CLEAR);
		complete(&hdmi->flt_cmp);
	}

	if (hdmi->scdc_intr) {
		u8 val;

		dev_dbg(hdmi->dev, "i2c scdc irq:%#x\n", hdmi->scdc_intr);
		hdmi_writel(hdmi, hdmi->scdc_intr, MAINUNIT_1_INT_CLEAR);
		val = hdmi_readl(hdmi, SCDC_STATUS0);

		/* frl start */
		if (val & BIT(4)) {
			hdmi_modb(hdmi, 0, SCDC_UPD_FLAGS_POLL_EN |
				  SCDC_UPD_FLAGS_AUTO_CLR, SCDC_CONFIG0);
			hdmi_modb(hdmi, 0, SCDC_UPD_FLAGS_RD_IRQ,
				  MAINUNIT_1_INT_MASK_N);
			dev_info(hdmi->dev, "frl start\n");
		}

	}

	if (stat)
		return IRQ_HANDLED;

	return IRQ_NONE;
}

static irqreturn_t dw_hdmi_qp_avp_hardirq(int irq, void *dev_id)
{
	struct dw_hdmi_qp *hdmi = dev_id;
	u32 stat1, stat3;

	if (!hdmi->dclk_en)
		return IRQ_HANDLED;

	stat1 = hdmi_readl(hdmi, AVP_1_INT_STATUS);
	stat3 = hdmi_readl(hdmi, AVP_3_INT_STATUS);

	if (!stat1 && !stat3)
		return IRQ_NONE;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t dw_hdmi_qp_avp_irq(int irq, void *dev_id)
{
	struct dw_hdmi_qp *hdmi = dev_id;
	struct drm_connector_state *conn_state;
	void *data = hdmi->plat_data->phy_data;
	u32 stat1, stat3, val;

	stat1 = hdmi_readl(hdmi, AVP_1_INT_STATUS);
	stat3 = hdmi_readl(hdmi, AVP_3_INT_STATUS);

	hdmi_writel(hdmi, stat1, AVP_1_INT_CLEAR);
	hdmi_writel(hdmi, stat3, AVP_3_INT_CLEAR);

	if (!hdmi->curr_conn || !hdmi->curr_conn->state)
		return IRQ_HANDLED;

	conn_state = hdmi->curr_conn->state;
	val = conn_state->content_protection;

	dev_dbg(hdmi->dev, "AVP_1_INT_STATUS:%x AVP_3_INT_STATUS:%x\n", stat1, stat3);
	if (hdmi->hdcp && hdmi->hdcp->hdcp_isr) {
		u32 hdcp_status = hdmi_readl(hdmi, HDCP14_STATUS0);

		if (stat1 & HDCP14_AUTH_CHG_MASK_N) {
			/* hdcp14 auth success */
			if (hdcp_status & BIT(2)) {
				hdmi->hdcp_status |= HDMI_HDCP14_AUTH;
				if (conn_state->content_protection !=
				    DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
					val = DRM_MODE_CONTENT_PROTECTION_ENABLED;
			} else if (!(hdcp_status & BIT(2))) {
				hdmi->hdcp_status &= ~HDMI_HDCP14_AUTH;
				if (conn_state->content_protection !=
				    DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
					val = DRM_MODE_CONTENT_PROTECTION_DESIRED;
			}
			conn_state->content_protection = val;
		}
		hdmi->hdcp->hdcp_isr(hdmi->hdcp, stat1, hdcp_status);
	}

	if (stat3 & HDCP2_ESM_P0_GPIO_OUT_2_CHG_IRQ) {
		stat3 = hdmi_readl(hdmi, HDCP2LOGIC_ESM_GPIO_OUT);
		if (stat3 & HDCP2_AUTHENTICATION_SUCCESS) {
			hdmi->hdcp_status |= HDMI_HDCP2_AUTH;
			if (conn_state->content_protection !=
			    DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
				val = DRM_MODE_CONTENT_PROTECTION_ENABLED;
			hdmi_modb(hdmi, 0, HDCP2_BYPASS, HDCP2LOGIC_CONFIG0);
			dev_info(hdmi->dev, "HDCP2 authentication succeed\n");
		} else if (stat3 & (HDCP2_AUTHENTICATION_SUCCESS | HDCP2_AUTHENTICATION_FAILED |
				    HDCP2_AUTHENTICATION_LINK_ERR |
				    HDCP2_AUTHENTICATION_NOT_CAPABLE)) {
			hdmi->hdcp_status &= ~HDMI_HDCP2_AUTH;
			if (conn_state->content_protection !=
			    DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
				val = DRM_MODE_CONTENT_PROTECTION_DESIRED;
			hdmi_modb(hdmi, HDCP2_BYPASS, HDCP2_BYPASS, HDCP2LOGIC_CONFIG0);
			dev_err(hdmi->dev, "HDCP2 authentication failed\n");
		}
		conn_state->content_protection = val;
	}

	if (hdmi->plat_data->set_hdcp_status)
		hdmi->plat_data->set_hdcp_status(data, hdmi->hdcp_status);

	return IRQ_HANDLED;
}

static irqreturn_t dw_hdmi_qp_earc_hardirq(int irq, void *dev_id)
{
	struct dw_hdmi_qp *hdmi = dev_id;
	u32 stat;

	stat = hdmi_readl(hdmi, EARCRX_0_INT_STATUS);
	if (stat) {
		dev_dbg(hdmi->dev, "earc irq %#x\n", stat);
		stat &= ~stat;
		hdmi_writel(hdmi, stat, EARCRX_0_INT_MASK_N);
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

static irqreturn_t dw_hdmi_qp_earc_irq(int irq, void *dev_id)
{
	struct dw_hdmi_qp *hdmi = dev_id;
	u32 stat;

	stat = hdmi_readl(hdmi, EARCRX_0_INT_STATUS);

	if (!stat)
		return IRQ_NONE;

	hdmi_writel(hdmi, stat, EARCRX_0_INT_CLEAR);

	hdmi->earc_intr = stat;
	complete(&hdmi->earc_cmp);

	return IRQ_HANDLED;
}

static int dw_hdmi_detect_phy(struct dw_hdmi_qp *hdmi)
{
	u8 phy_type;

	phy_type = hdmi->plat_data->phy_force_vendor ?
				DW_HDMI_PHY_VENDOR_PHY : 0;

	if (phy_type == DW_HDMI_PHY_VENDOR_PHY) {
		/* Vendor PHYs require support from the glue layer. */
		if (!hdmi->plat_data->qp_phy_ops || !hdmi->plat_data->phy_name) {
			dev_err(hdmi->dev,
				"Vendor HDMI PHY not supported by glue layer\n");
			return -ENODEV;
		}

		hdmi->phy.ops = hdmi->plat_data->qp_phy_ops;
		hdmi->phy.data = hdmi->plat_data->phy_data;
		hdmi->phy.name = hdmi->plat_data->phy_name;
	}

	return 0;
}

void dw_hdmi_qp_cec_set_hpd(struct dw_hdmi_qp *hdmi, bool plug_in, bool change)
{
	enum drm_connector_status status = plug_in ?
		connector_status_connected : connector_status_disconnected;

	if (!hdmi->cec_notifier)
		return;

	if (!plug_in)
		cec_notifier_set_phys_addr(hdmi->cec_notifier,
					   CEC_PHYS_ADDR_INVALID);

	if (hdmi->bridge.dev) {
#if IS_REACHABLE(CONFIG_DRM_DW_HDMI_CEC)
		if (change && hdmi->cec_adap && hdmi->cec_adap->devnode.registered)
			cec_queue_pin_hpd_event(hdmi->cec_adap, plug_in, ktime_get());
#endif
		drm_bridge_hpd_notify(&hdmi->bridge, status);
	}
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_cec_set_hpd);

static void dw_hdmi_qp_cec_enable(struct dw_hdmi_qp *hdmi)
{
	mutex_lock(&hdmi->mutex);
	hdmi_modb(hdmi, 0, CEC_SWDISABLE, GLOBAL_SWDISABLE);
	mutex_unlock(&hdmi->mutex);
}

static void dw_hdmi_qp_cec_disable(struct dw_hdmi_qp *hdmi)
{
	mutex_lock(&hdmi->mutex);
	hdmi_modb(hdmi, CEC_SWDISABLE, CEC_SWDISABLE, GLOBAL_SWDISABLE);
	mutex_unlock(&hdmi->mutex);
}

static const struct dw_hdmi_qp_cec_ops dw_hdmi_qp_cec_ops = {
	.enable = dw_hdmi_qp_cec_enable,
	.disable = dw_hdmi_qp_cec_disable,
	.write = hdmi_writel,
	.read = hdmi_readl,
};

static const struct regmap_config hdmi_regmap_config = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= EARCRX_1_INT_FORCE,
};

struct dw_hdmi_qp_reg_table {
	int reg_base;
	int reg_end;
};

static const struct dw_hdmi_qp_reg_table hdmi_reg_table[] = {
	{0x0, 0xc},
	{0x14, 0x1c},
	{0x44, 0x48},
	{0x50, 0x58},
	{0x80, 0x84},
	{0xa0, 0xc4},
	{0xe0, 0xe8},
	{0xf0, 0x118},
	{0x140, 0x140},
	{0x150, 0x150},
	{0x160, 0x168},
	{0x180, 0x180},
	{0x800, 0x800},
	{0x808, 0x808},
	{0x814, 0x814},
	{0x81c, 0x824},
	{0x834, 0x834},
	{0x840, 0x864},
	{0x86c, 0x86c},
	{0x880, 0x89c},
	{0x8e0, 0x8e8},
	{0x900, 0x900},
	{0x908, 0x90c},
	{0x920, 0x938},
	{0x920, 0x938},
	{0x960, 0x960},
	{0x968, 0x968},
	{0xa20, 0xa20},
	{0xa30, 0xa30},
	{0xa40, 0xa40},
	{0xa54, 0xa54},
	{0xa80, 0xaac},
	{0xab4, 0xab8},
	{0xb00, 0xcbc},
	{0xce0, 0xce0},
	{0xd00, 0xddc},
	{0xe20, 0xe24},
	{0xe40, 0xe44},
	{0xe4c, 0xe4c},
	{0xe60, 0xe80},
	{0xea0, 0xf24},
	{0x1004, 0x100c},
	{0x1020, 0x1030},
	{0x1040, 0x1050},
	{0x1060, 0x1068},
	{0x1800, 0x1820},
	{0x182c, 0x182c},
	{0x1840, 0x1940},
	{0x1960, 0x1a60},
	{0x1b00, 0x1b00},
	{0x1c00, 0x1c00},
	{0x3000, 0x3000},
	{0x3010, 0x3014},
	{0x3020, 0x3024},
	{0x3800, 0x3800},
	{0x3810, 0x3814},
	{0x3820, 0x3824},
	{0x3830, 0x3834},
	{0x3840, 0x3844},
	{0x3850, 0x3854},
	{0x3860, 0x3864},
	{0x3870, 0x3874},
	{0x4000, 0x4004},
	{0x4800, 0x4800},
	{0x4810, 0x4814},
};

static int dw_hdmi_ctrl_show(struct seq_file *s, void *v)
{
	struct dw_hdmi_qp *hdmi = s->private;
	u32 i = 0, j = 0, val = 0;

	if (hdmi->disabled) {
		dev_err(hdmi->dev, "hdmi is disabled\n");
		return -EACCES;
	}

	seq_puts(s, "\n---------------------------------------------------");

	for (i = 0; i < ARRAY_SIZE(hdmi_reg_table); i++) {
		for (j = hdmi_reg_table[i].reg_base;
		     j <= hdmi_reg_table[i].reg_end; j += 4) {
			val = hdmi_readl(hdmi, j);

			if ((j - hdmi_reg_table[i].reg_base) % 16 == 0)
				seq_printf(s, "\n>>>hdmi_ctl %04x:", j);
			seq_printf(s, " %08x", val);
		}
	}
	seq_puts(s, "\n---------------------------------------------------\n");

	return 0;
}

static int dw_hdmi_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, dw_hdmi_ctrl_show, inode->i_private);
}

static ssize_t
dw_hdmi_ctrl_write(struct file *file, const char __user *buf,
		   size_t count, loff_t *ppos)
{
	struct dw_hdmi_qp *hdmi =
		((struct seq_file *)file->private_data)->private;
	u32 reg, val;
	char kbuf[25];

	if (hdmi->disabled) {
		dev_err(hdmi->dev, "hdmi is disabled\n");
		return -EACCES;
	}

	if (count > 24) {
		dev_err(hdmi->dev, "out of buf range\n");
		return count;
	}

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count - 1] = '\0';

	if (sscanf(kbuf, "%x %x", &reg, &val) == -1)
		return -EFAULT;
	if (reg > EARCRX_1_INT_FORCE) {
		dev_err(hdmi->dev, "it is no a hdmi register\n");
		return count;
	}
	dev_info(hdmi->dev, "/**********hdmi register config******/");
	dev_info(hdmi->dev, "\n reg=%x val=%x\n", reg, val);
	hdmi_writel(hdmi, val, reg);
	return count;
}

static const struct file_operations dw_hdmi_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = dw_hdmi_ctrl_open,
	.read = seq_read,
	.write = dw_hdmi_ctrl_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dw_hdmi_status_show(struct seq_file *s, void *v)
{
	struct dw_hdmi_qp *hdmi = s->private;
	u32 val;

	seq_puts(s, "PHY: ");
	if (hdmi->disabled) {
		seq_puts(s, "disabled\n");
		return 0;
	}
	seq_puts(s, "enabled\t\t\tMode: ");
	if (hdmi->sink_is_hdmi)
		seq_puts(s, "HDMI\n");
	else
		seq_puts(s, "DVI\n");

	if (hdmi->hdmi_data.video_mode.mpixelclock > 600000000) {
		seq_printf(s, "FRL Mode Pixel Clk: %luHz\n",
			   hdmi->hdmi_data.video_mode.mpixelclock);
	} else {
		if (hdmi->hdmi_data.video_mode.mtmdsclock > 340000000)
			val = hdmi->hdmi_data.video_mode.mtmdsclock / 4;
		else
			val = hdmi->hdmi_data.video_mode.mtmdsclock;
		seq_printf(s, "TMDS Mode Pixel Clk: %luHz\t\tTMDS Clk: %uHz\n",
			   hdmi->hdmi_data.video_mode.mpixelclock, val);
	}
	seq_printf(s, "ALLM: %d\n", hdmi->allm_enable);
	seq_puts(s, "Color Format: ");
	if (hdmi_bus_fmt_is_rgb(hdmi->hdmi_data.enc_out_bus_format))
		seq_puts(s, "RGB");
	else if (hdmi_bus_fmt_is_yuv444(hdmi->hdmi_data.enc_out_bus_format))
		seq_puts(s, "YUV444");
	else if (hdmi_bus_fmt_is_yuv422(hdmi->hdmi_data.enc_out_bus_format))
		seq_puts(s, "YUV422");
	else if (hdmi_bus_fmt_is_yuv420(hdmi->hdmi_data.enc_out_bus_format))
		seq_puts(s, "YUV420");
	else
		seq_puts(s, "UNKNOWN");
	val =  hdmi_bus_fmt_color_depth(hdmi->hdmi_data.enc_out_bus_format);
	seq_printf(s, "\t\tColor Depth: %d bit\n", val);
	seq_puts(s, "Colorimetry: ");
	switch (hdmi->hdmi_data.enc_out_encoding) {
	case V4L2_YCBCR_ENC_601:
		seq_puts(s, "ITU.BT601");
		break;
	case V4L2_YCBCR_ENC_709:
		seq_puts(s, "ITU.BT709");
		break;
	case V4L2_YCBCR_ENC_BT2020:
		seq_puts(s, "ITU.BT2020");
		break;
	default: /* Carries no data */
		seq_puts(s, "ITU.BT601");
		break;
	}

	seq_puts(s, "\t\tEOTF: ");

	val = hdmi_readl(hdmi, PKTSCHED_PKT_EN);
	if (!(val & PKTSCHED_DRMI_TX_EN)) {
		seq_puts(s, "Off\n");
		return 0;
	}

	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS1);
	val = (val >> 8) & 0x7;
	switch (val) {
	case HDMI_EOTF_TRADITIONAL_GAMMA_SDR:
		seq_puts(s, "SDR");
		break;
	case HDMI_EOTF_TRADITIONAL_GAMMA_HDR:
		seq_puts(s, "HDR");
		break;
	case HDMI_EOTF_SMPTE_ST2084:
		seq_puts(s, "ST2084");
		break;
	case HDMI_EOTF_BT_2100_HLG:
		seq_puts(s, "HLG");
		break;
	default:
		seq_puts(s, "Not Defined\n");
		return 0;
	}

	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS1);
	val = (val >> 16) & 0xffff;
	seq_printf(s, "\nx0: %d", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS2);
	val = val & 0xffff;
	seq_printf(s, "\t\t\t\ty0: %d\n", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS2);
	val = (val >> 16) & 0xffff;
	seq_printf(s, "x1: %d", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS3);
	val = val & 0xffff;
	seq_printf(s, "\t\t\t\ty1: %d\n", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS3);
	val = (val >> 16) & 0xffff;
	seq_printf(s, "x2: %d", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS4);
	val = val & 0xffff;
	seq_printf(s, "\t\t\t\ty2: %d\n", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS4);
	val = (val >> 16) & 0xffff;
	seq_printf(s, "white x: %d", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS5);
	val = val & 0xffff;
	seq_printf(s, "\t\t\twhite y: %d\n", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS5);
	val = (val >> 16) & 0xffff;
	seq_printf(s, "max lum: %d", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS6);
	val = val & 0xffff;
	seq_printf(s, "\t\t\tmin lum: %d\n", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS6);
	val = (val >> 16) & 0xffff;
	seq_printf(s, "max cll: %d", val);
	val = hdmi_readl(hdmi, PKT_DRMI_CONTENTS7);
	val = val & 0xffff;
	seq_printf(s, "\t\t\tmax fall: %d\n", val);
	return 0;
}

static int dw_hdmi_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, dw_hdmi_status_show, inode->i_private);
}

static const struct file_operations dw_hdmi_status_fops = {
	.owner = THIS_MODULE,
	.open = dw_hdmi_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void dw_hdmi_register_debugfs(struct device *dev, struct dw_hdmi_qp *hdmi)
{
	u8 buf[11];

	snprintf(buf, sizeof(buf), "dw-hdmi%d", hdmi->plat_data->id);
	hdmi->debugfs_dir = debugfs_create_dir(buf, NULL);
	if (IS_ERR(hdmi->debugfs_dir)) {
		dev_err(dev, "failed to create debugfs dir!\n");
		return;
	}

	debugfs_create_file("status", 0400, hdmi->debugfs_dir,
			    hdmi, &dw_hdmi_status_fops);
	debugfs_create_file("ctrl", 0600, hdmi->debugfs_dir,
			    hdmi, &dw_hdmi_ctrl_fops);
}

static void dw_hdmi_qp_hdcp14_get_mem(struct dw_hdmi_qp *hdmi, u8 *data, u32 len)
{
	u32 ksv_len, i, val;
	void *hdmi_data = hdmi->plat_data->phy_data;

	if (hdmi->plat_data->set_hdcp14_mem)
		hdmi->plat_data->set_hdcp14_mem(hdmi_data, true);

	ksv_len = len - BSTATUS_LEN - M0_LEN - SHAMAX;
	for (i = 0; i < len; i++) {
		/* read ksv list */
		if (i < ksv_len)
			val = readl(hdmi->hdcp14_mem + HDMI_HDCP14_MEM_KSV0 + i * 4);
		/*
		 * read bstatus, if device count is 0, bstatus save in external
		 * memory is error, we need to read bstatus via ddc
		 */
		else if (i < len - SHAMAX - M0_LEN)
			hdcp_ddc_read(hdmi->ddc, HDMI_HDCP_ADDR, 0x41 + i - ksv_len,
				      &val);
		/* read M0 */
		else if (i < len - SHAMAX)
			val = readl(hdmi->hdcp14_mem + HDMI_HDCP14_MEM_M0_1 +
				    (i - ksv_len - BSTATUS_LEN) * 4);
		else
			/* VH0 save in external memory is error, we need to read VH0 via ddc */
			hdcp_ddc_read(hdmi->ddc, HDMI_HDCP_ADDR, HDMI_VH0 + i - (len - SHAMAX),
				      &val);

		data[i] = val;
	}

	if (hdmi->plat_data->set_hdcp14_mem)
		hdmi->plat_data->set_hdcp14_mem(hdmi_data, false);
}

static void dw_hdmi_qp_unregister_platform_device(void *pdev)
{
	platform_device_unregister(pdev);
}

int dw_hdmi_qp_register_hdcp(struct dw_hdmi_qp *hdmi)
{
	int ret = 0;

	struct dw_qp_hdcp hdmi_hdcp = {
		.hdmi = hdmi,
		.write = hdmi_writel,
		.read = hdmi_readl,
		.regs = hdmi->regs,
		.get_mem = dw_hdmi_qp_hdcp14_get_mem,
	};

	struct platform_device_info hdcp_device_info = {
		.parent = hdmi->dev,
		.id = PLATFORM_DEVID_AUTO,
		.res = NULL,
		.num_res = 0,
		.name = DW_HDCP_QP_DRIVER_NAME,
		.data = &hdmi_hdcp,
		.size_data = sizeof(hdmi_hdcp),
		.dma_mask = DMA_BIT_MASK(32),
	};

	if (hdmi->hdcp14_mem) {
		hdmi->hdcp_dev = platform_device_register_full(&hdcp_device_info);
		if (IS_ERR(hdmi->hdcp_dev)) {
			dev_err(hdmi->dev, "Failed to register hdcp device!\n");
			ret = PTR_ERR(hdmi->hdcp_dev);
		} else {
			hdmi->hdcp = hdmi->hdcp_dev->dev.platform_data;
			ret = devm_add_action_or_reset(hdmi->dev,
						       dw_hdmi_qp_unregister_platform_device,
						       hdmi->hdcp_dev);
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_register_hdcp);

int dw_hdmi_qp_register_audio(struct dw_hdmi_qp *hdmi)
{
	int ret = 0;

	struct dw_hdmi_qp_i2s_audio_data audio = {
		.hdmi	= hdmi,
		.eld	= hdmi->connector.eld,
		.write	= hdmi_writel,
		.read	= hdmi_readl,
		.mod	= hdmi_modb,
	};

	struct platform_device_info pdevinfo = {
		.parent = hdmi->dev,
		.id = PLATFORM_DEVID_AUTO,
		.name = "dw-hdmi-qp-i2s-audio",
		.data = &audio,
		.size_data = sizeof(audio),
		.dma_mask = DMA_BIT_MASK(32),
	};

	hdmi->enable_audio = dw_hdmi_i2s_audio_enable;
	hdmi->disable_audio = dw_hdmi_i2s_audio_disable;
	hdmi->audio = platform_device_register_full(&pdevinfo);
	if (IS_ERR(hdmi->audio)) {
		dev_err(hdmi->dev, "cannot register %s device\n", pdevinfo.name);
		ret = PTR_ERR(hdmi->audio);
	} else {
		ret = devm_add_action_or_reset(hdmi->dev, dw_hdmi_qp_unregister_platform_device,
					       hdmi->audio);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_register_audio);

int dw_hdmi_qp_register_cec(struct dw_hdmi_qp *hdmi)
{
	struct cec_connector_info conn_info;
	struct cec_notifier *notifier;
	int ret = 0;

	struct platform_device_info pdevinfo = {
		.parent = hdmi->dev,
		.name = "dw-hdmi-qp-cec",
		.id = PLATFORM_DEVID_AUTO,
		.data = &hdmi->cec_data,
		.size_data = sizeof(hdmi->cec_data),
		.dma_mask = 0,
	};

	/*
	 * We need hdmi cec, and hdmi connecter is registered.
	 */

	if (!hdmi->cec_enable || !hdmi->connector.dev)
		return ret;

	hdmi->cec = platform_device_register_full(&pdevinfo);
	if (IS_ERR(hdmi->cec)) {
		dev_err(hdmi->dev, "Cannot register %s device\n", pdevinfo.name);
		ret = PTR_ERR(hdmi->cec);
	} else {
		cec_fill_conn_info_from_drm(&conn_info, &hdmi->connector);
		notifier = cec_notifier_conn_register(hdmi->dev, NULL, &conn_info);
		if (notifier) {
			mutex_lock(&hdmi->cec_notifier_mutex);
			hdmi->cec_notifier = notifier;
			mutex_unlock(&hdmi->cec_notifier_mutex);
		} else {
			dev_warn(hdmi->dev, "Register cec notifier failed\n");
		}
		devm_add_action_or_reset(hdmi->dev, dw_hdmi_qp_unregister_platform_device,
					 hdmi->cec);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_register_cec);

static struct dw_hdmi_qp *dw_hdmi_qp_probe(struct platform_device *pdev,
					   const struct dw_hdmi_plat_data *plat_data)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *ddc_node;
	struct dw_hdmi_qp *hdmi;
	struct resource *iores = NULL;
	struct drm_panel *panel = NULL;
	struct drm_bridge *bridge = NULL;
	int irq;
	int ret;

	ret = drm_of_find_panel_or_bridge(np, 1, -1, &panel, &bridge);
	if (ret < 0 && ret != -ENODEV)
		return ERR_PTR(ret);

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return ERR_PTR(-ENOMEM);

	hdmi->panel = panel;
	hdmi->next_bridge = bridge;
	hdmi->connector.stereo_allowed = 1;
	hdmi->plat_data = plat_data;
	hdmi->dev = dev;
	hdmi->sample_rate = 48000;
	hdmi->disabled = true;

	mutex_init(&hdmi->mutex);
	mutex_init(&hdmi->audio_mutex);
	mutex_init(&hdmi->cec_notifier_mutex);

	ddc_node = of_parse_phandle(np, "ddc-i2c-bus", 0);
	if (ddc_node) {
		hdmi->ddc = of_get_i2c_adapter_by_node(ddc_node);
		of_node_put(ddc_node);
		if (!hdmi->ddc) {
			dev_dbg(hdmi->dev, "failed to read ddc node\n");
			return ERR_PTR(-EPROBE_DEFER);
		}

	} else {
		dev_dbg(hdmi->dev, "no ddc property found\n");
	}

	if (!plat_data->regm) {
		const struct regmap_config *reg_config;

		reg_config = &hdmi_regmap_config;

		iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		hdmi->regs = devm_ioremap_resource(dev, iores);
		if (IS_ERR(hdmi->regs))
			return ERR_CAST(hdmi->regs);

		hdmi->regm = devm_regmap_init_mmio(dev, hdmi->regs, reg_config);
		if (IS_ERR(hdmi->regm)) {
			dev_err(dev, "Failed to configure regmap\n");
			return ERR_CAST(hdmi->regm);
		}
	} else {
		hdmi->regm = plat_data->regm;
	}

	ret = dw_hdmi_detect_phy(hdmi);
	if (ret < 0)
		return ERR_PTR(ret);

	if (hdmi->plat_data->get_force_timing(hdmi->plat_data->phy_data))
		hdmi->force_kernel_output = true;

	hdmi->refclk_rate = hdmi->plat_data->get_refclk_rate(hdmi->plat_data->phy_data);

	hdmi_writel(hdmi, 0, MAINUNIT_0_INT_MASK_N);
	hdmi_writel(hdmi, 0, MAINUNIT_1_INT_MASK_N);
	hdmi_writel(hdmi, 0, AVP_1_INT_MASK_N);
	hdmi_writel(hdmi, 0, AVP_3_INT_MASK_N);
	hdmi_writel(hdmi, hdmi->refclk_rate, TIMER_BASE_CONFIG0);
	hdmi->logo_plug_out = false;
	if (hdmi->phy.ops->read_hpd(hdmi, hdmi->phy.data) == connector_status_connected &&
	    hdmi_readl(hdmi, I2CM_INTERFACE_CONTROL0) &&
	    (hdmi_readl(hdmi, CMU_STATUS) & HDMI_CTRL_CLK_EN) == HDMI_CTRL_CLK_EN) {
		hdmi->initialized = true;
		hdmi->disabled = false;
	}

	hdmi->sink_is_hdmi = true;

	/* frl training max ffe level */
	if (of_property_read_u32(np, "max-ffe-lv", &hdmi->max_ffe_lv)) {
		hdmi->max_ffe_lv = 0;
	} else if (hdmi->max_ffe_lv > 3) {
		dev_err(hdmi->dev, "dts max ffe level is %d, out of allowed range of 0 to 3\n",
			hdmi->max_ffe_lv);
		return ERR_PTR(-EINVAL);
	}

	/* If DDC bus is not specified, try to register HDMI I2C bus */
	if (!hdmi->ddc) {
		hdmi->ddc = dw_hdmi_i2c_adapter(hdmi);
		if (IS_ERR(hdmi->ddc)) {
			dev_err(hdmi->dev, "register HDMI I2C bus failed\n");
			return ERR_PTR(-ENOMEM);
		}

		/*
		 * Read high and low time from device tree. If not available use
		 * the default timing scl clock rate is about 100KHz.
		 */
		if (of_property_read_u32(np, "ddc-i2c-scl-high-time-ns",
					 &hdmi->i2c->scl_high_ns))
			hdmi->i2c->scl_high_ns = 5000;
		if (of_property_read_u32(np, "ddc-i2c-scl-low-time-ns",
					 &hdmi->i2c->scl_low_ns))
			hdmi->i2c->scl_low_ns = 5000;

		/* limit scl freq from 10KHz to 100KHz */
		if (hdmi->i2c->scl_high_ns < 5000 || hdmi->i2c->scl_high_ns > 50000) {
			dev_err(hdmi->dev, "scl_high_ns is out of range, set to 5000\n");
			hdmi->i2c->scl_high_ns = 5000;
		}

		if (hdmi->i2c->scl_low_ns < 5000 || hdmi->i2c->scl_low_ns > 50000) {
			dev_err(hdmi->dev, "scl_low_ns is out of range, set to 5000\n");
			hdmi->i2c->scl_low_ns = 5000;
		}

		/* Reset HDMI DDC I2C master controller and mute I2CM interrupts */
		dw_hdmi_i2c_init(hdmi);
	}

	init_completion(&hdmi->flt_cmp);
	init_completion(&hdmi->earc_cmp);

	if (of_property_read_bool(np, "scramble-low-rates"))
		hdmi->scramble_low_rates = true;

	hdmi_init_clk_regenerator(hdmi);

	hdmi->bridge.driver_private = hdmi;
	hdmi->bridge.funcs = &dw_hdmi_bridge_funcs;
#ifdef CONFIG_OF
	hdmi->bridge.of_node = pdev->dev.of_node;
#endif

	if (hdmi->phy.ops->setup_hpd)
		hdmi->phy.ops->setup_hpd(hdmi, hdmi->phy.data);

	hdmi->connector.ycbcr_420_allowed = hdmi->plat_data->ycbcr_420_allowed;

	hdmi->extcon = devm_extcon_dev_allocate(hdmi->dev, dw_hdmi_cable);
	if (IS_ERR(hdmi->extcon)) {
		dev_err(hdmi->dev, "allocate extcon failed\n");
		ret = PTR_ERR(hdmi->extcon);
		goto err_ddc;
	}

	ret = devm_extcon_dev_register(hdmi->dev, hdmi->extcon);
	if (ret) {
		dev_err(hdmi->dev, "failed to register extcon: %d\n", ret);
		goto err_ddc;
	}

	ret = extcon_set_property_capability(hdmi->extcon, EXTCON_DISP_HDMI,
					     EXTCON_PROP_DISP_HPD);
	if (ret) {
		dev_err(hdmi->dev,
			"failed to set USB property capability: %d\n", ret);
		goto err_ddc;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto err_ddc;
	}

	hdmi->avp_irq = irq;
	ret = devm_request_threaded_irq(dev, hdmi->avp_irq,
					dw_hdmi_qp_avp_hardirq,
					dw_hdmi_qp_avp_irq, IRQF_ONESHOT,
					dev_name(dev), hdmi);
	if (ret)
		goto err_ddc;

	if (of_property_read_bool(np, "cec-enable")) {
		hdmi->cec_enable = true;
		irq = platform_get_irq(pdev, 1);
		if (irq < 0) {
			ret = irq;
			goto err_ddc;
		}

		hdmi->cec_data.irq  = irq;
		hdmi->cec_data.hdmi = hdmi;
		hdmi->cec_data.ops = &dw_hdmi_qp_cec_ops;
	};

	irq = platform_get_irq(pdev, 2);
	if (irq < 0) {
		ret = irq;
		goto err_ddc;
	}

	hdmi->earc_irq = irq;
	ret = devm_request_threaded_irq(dev, hdmi->earc_irq,
					dw_hdmi_qp_earc_hardirq,
					dw_hdmi_qp_earc_irq, IRQF_SHARED,
					dev_name(dev), hdmi);
	if (ret)
		goto err_ddc;

	irq = platform_get_irq(pdev, 3);
	if (irq < 0) {
		ret = irq;
		goto err_ddc;
	}

	hdmi->main_irq = irq;
	ret = devm_request_threaded_irq(dev, hdmi->main_irq,
					dw_hdmi_qp_main_hardirq, NULL,
					IRQF_SHARED, dev_name(dev), hdmi);
	if (ret)
		goto err_ddc;

	dw_hdmi_register_debugfs(dev, hdmi);

	if (hdmi_readl(hdmi, CONFIG_REG) & CONFIG_HDCP14) {
		iores = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		hdmi->hdcp14_mem = devm_ioremap_resource(dev, iores);

		if (IS_ERR(hdmi->hdcp14_mem)) {
			ret = PTR_ERR(hdmi->hdcp14_mem);
			hdmi->hdcp14_mem = NULL;
			goto err_ddc;
		}
	}

	hdmi->workqueue = create_workqueue("dw_hdmi_flt_queue");
	INIT_WORK(&hdmi->flt_work, dw_hdmi_qp_flt_work);

	return hdmi;

err_ddc:
	if (hdmi->i2c)
		i2c_del_adapter(&hdmi->i2c->adap);
	else
		i2c_put_adapter(hdmi->ddc);

	if (!hdmi->plat_data->first_screen) {
		dw_hdmi_destroy_properties(hdmi);
		hdmi->connector.funcs->destroy(&hdmi->connector);
	}

	if (hdmi->bridge.encoder && !hdmi->plat_data->first_screen)
		hdmi->bridge.encoder->funcs->destroy(hdmi->bridge.encoder);

	return ERR_PTR(ret);
}

static void dw_hdmi_qp_remove(struct dw_hdmi_qp *hdmi)
{
	cancel_work_sync(&hdmi->flt_work);
	flush_workqueue(hdmi->workqueue);
	destroy_workqueue(hdmi->workqueue);

	if (hdmi->avp_irq)
		disable_irq(hdmi->avp_irq);

	if (hdmi->main_irq)
		disable_irq(hdmi->main_irq);

	if (hdmi->earc_irq)
		disable_irq(hdmi->earc_irq);

	debugfs_remove_recursive(hdmi->debugfs_dir);

	if (!hdmi->plat_data->first_screen) {
		dw_hdmi_destroy_properties(hdmi);
		hdmi->connector.funcs->destroy(&hdmi->connector);
	}

	if (hdmi->bridge.encoder && !hdmi->plat_data->first_screen)
		hdmi->bridge.encoder->funcs->destroy(hdmi->bridge.encoder);
	if (!IS_ERR(hdmi->cec))
		platform_device_unregister(hdmi->cec);
	if (!IS_ERR(hdmi->hdcp_dev))
		platform_device_unregister(hdmi->hdcp_dev);
	if (hdmi->i2c)
		i2c_del_adapter(&hdmi->i2c->adap);
	else
		i2c_put_adapter(hdmi->ddc);
}

/* -----------------------------------------------------------------------------
 * Bind/unbind API, used from platforms based on the component framework.
 */
struct dw_hdmi_qp *dw_hdmi_qp_bind(struct platform_device *pdev,
				   struct drm_encoder *encoder,
				   struct dw_hdmi_plat_data *plat_data)
{
	struct dw_hdmi_qp *hdmi;
	int ret;

	hdmi = dw_hdmi_qp_probe(pdev, plat_data);
	if (IS_ERR(hdmi))
		return hdmi;

	if (!plat_data->first_screen) {
		ret = drm_bridge_attach(encoder, &hdmi->bridge, NULL, 0);
		if (ret) {
			dw_hdmi_qp_remove(hdmi);
			dev_err(hdmi->dev, "Failed to initialize bridge with drm\n");
			return ERR_PTR(ret);
		}

		plat_data->connector = &hdmi->connector;
		if (hdmi->skip_connector && hdmi->next_bridge)
			plat_data->bridge = hdmi->next_bridge;
		else
			plat_data->bridge = NULL;
	}

	if (plat_data->split_mode && !hdmi->plat_data->first_screen) {
		struct dw_hdmi_qp *secondary = NULL;

		if (hdmi->plat_data->left)
			secondary = hdmi->plat_data->left;
		else if (hdmi->plat_data->right)
			secondary = hdmi->plat_data->right;

		if (!secondary)
			return ERR_PTR(-ENOMEM);
		ret = drm_bridge_attach(encoder, &secondary->bridge, &hdmi->bridge,
					DRM_BRIDGE_ATTACH_NO_CONNECTOR);
		if (ret)
			return ERR_PTR(ret);
	}

	return hdmi;
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_bind);

void dw_hdmi_qp_unbind(struct dw_hdmi_qp *hdmi)
{
	dw_hdmi_qp_remove(hdmi);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_unbind);

void dw_hdmi_qp_suspend(struct device *dev, struct dw_hdmi_qp *hdmi)
{
	if (!hdmi) {
		dev_warn(dev, "Hdmi has not been initialized\n");
		return;
	}

	mutex_lock(&hdmi->mutex);

	/*
	 * When system shutdown, hdmi should be disabled.
	 * When system suspend, dw_hdmi_qp_bridge_disable will disable hdmi first.
	 * To prevent duplicate operation, we should determine whether hdmi
	 * has been disabled.
	 */
	if (!hdmi->disabled)
		hdmi->disabled = true;
	mutex_unlock(&hdmi->mutex);

	if (hdmi->avp_irq)
		disable_irq(hdmi->avp_irq);

	if (hdmi->main_irq)
		disable_irq(hdmi->main_irq);

	if (hdmi->earc_irq)
		disable_irq(hdmi->earc_irq);

	pinctrl_pm_select_sleep_state(dev);
	if (!hdmi->next_bridge)
		drm_connector_update_edid_property(&hdmi->connector, NULL);
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_suspend);

void dw_hdmi_qp_resume(struct device *dev, struct dw_hdmi_qp *hdmi)
{
	enum drm_connector_status result;

	if (!hdmi) {
		dev_warn(dev, "Hdmi has not been initialized\n");
		return;
	}

	hdmi_writel(hdmi, 0, MAINUNIT_0_INT_MASK_N);
	hdmi_writel(hdmi, 0, MAINUNIT_1_INT_MASK_N);
	hdmi_writel(hdmi, hdmi->refclk_rate, TIMER_BASE_CONFIG0);

	pinctrl_pm_select_default_state(dev);

	if (hdmi->cec_adap)
		hdmi->cec_adap->ops->adap_enable(hdmi->cec_adap, true);

	mutex_lock(&hdmi->mutex);
	if (hdmi->i2c)
		dw_hdmi_i2c_init(hdmi);
	if (hdmi->avp_irq)
		enable_irq(hdmi->avp_irq);

	if (hdmi->main_irq)
		enable_irq(hdmi->main_irq);

	if (hdmi->earc_irq)
		enable_irq(hdmi->earc_irq);

	mutex_unlock(&hdmi->mutex);

	if (hdmi->phy.ops->setup_hpd)
		hdmi->phy.ops->setup_hpd(hdmi, hdmi->phy.data);

	result = hdmi->phy.ops->read_hpd(hdmi, hdmi->phy.data);
	if (result == connector_status_connected) {
		mutex_lock(&hdmi->connector.dev->mode_config.mutex);
		drm_helper_probe_single_connector_modes(&hdmi->connector, 9000, 9000);
		mutex_unlock(&hdmi->connector.dev->mode_config.mutex);
	}
}
EXPORT_SYMBOL_GPL(dw_hdmi_qp_resume);

MODULE_AUTHOR("Algea Cao <algea.cao@rock-chips.com>");
MODULE_DESCRIPTION("DW HDMI QP transmitter driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:dw-hdmi-qp");
