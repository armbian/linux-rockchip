// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Rockchip Electronics Co., Ltd.
 *
 * Author: Algea Cao <algea.cao@rock-chips.com>
 */

#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>

#include <drm/drm_of.h>
#include <drm/drm_crtc_helper.h>
#include <drm/display/drm_dsc.h>
#include <drm/drm_edid.h>
#include <drm/display/drm_hdcp_helper.h>
#include <drm/bridge/dw_hdmi.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_vblank.h>

#include <video/display_timing.h>
#include <video/videomode.h>
#include <video/of_display_timing.h>

#include <uapi/linux/videodev2.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_vop.h"
#include "../../../phy/rockchip/phy-hdmi.h"

#define HIWORD_UPDATE(val, mask)	(val | (mask) << 16)

#define RK3538_VO_GRF_HDMI_MISC		0x68
#define RK3538_COLOR_FORMAT_MASK	(0xf << 4)
#define RK3538_COLOR_DEPTH_MASK		(0xf)
#define RK3538_8BPC			0x0
#define RK3538_10BPC			0x6
#define RK3538_VO_GRF_HDMI_SWITCH	0x6c
#define RK3538_PMU_GRF_SOC_CON2		0x8
#define RK3538_HDMI_CEC_DET_SEL		BIT(15)
#define RK3538_HDMI_HPD_INT_CON		0x400
#define RK3538_HDMITX_HPD_INT_MSK	BIT(2)
#define RK3538_HDMITX_HPD_INT_CLR	BIT(1)
#define RK3538_HDMI_HPD_CON		0x404
#define RK3538_HDMI_HPD_ST		0x408

#define RK3572_VO0_GRF_SOC_CON0		0x0000
#define RK3572_VO0_GRF_SOC_CON8		0x0020
#define RK3572_VO0_GRF_SOC_CON12	0x0030
#define RK3572_VO0_GRF_SOC_CON13	0x0034

#define RK3572_SYS_GRF_CON1		0x4
#define RK3572_HDMITX_HPD_INT_MSK	BIT(15)
#define RK3572_HDMITX_HPD_INT_CLR	BIT(14)
#define RK3572_SYS_GRF_CON2		0x8
#define RK3572_SET_DLY_EN_MASK		(0x3f << 8)
#define RK3572_SET_DLY_EN		BIT(8)
#define RK3572_SET_LNUM_MS_MASK		0xff
#define RK3572_HDMITX_HPD_STATUS	0x140
#define RK3572_HDMITX_OHPD_INT		BIT(5)
#define RK3572_HDMITX_LEVEL_INT		BIT(4)
#define RK3572_HDMITX_INTR_CHANGE_CNT	0xe

#define RK3576_IOC_MISC_CON0		0xa400
#define RK3576_HDMITX_HPD_INT_MSK	BIT(2)
#define RK3576_HDMITX_HPD_INT_CLR	BIT(1)
#define RK3576_IOC_MISC_CON1		0Xa404
#define RK3576_SET_DLY_EN_MASK		(0x3f << 8)
#define RK3576_SET_DLY_EN		BIT(8)
#define RK3576_SET_LNUM_MS_MASK		0xff
#define RK3576_IOC_HDMITX_HPD_STATUS	0xa440
#define RK3576_HDMITX_LOW_MORETHAN100MS	BIT(7)
#define RK3576_HDMITX_HPD_PORT_LEVEL	BIT(6)
#define RK3576_HDMITX_IHPD_PORT		BIT(5)
#define RK3576_HDMITX_OHPD_INT		BIT(4)
#define RK3576_HDMITX_LEVEL_INT		BIT(3)
#define RK3576_HDMITX_INTR_CHANGE_CNT	0x7

#define RK3576_VO0_GRF_SOC_CON1		0x0004
#define RK3576_HDMITX_FRL_MOD		BIT(0)
#define RK3576_HDMI_EMP_MEM_LEN_BYPASS	BIT(13)
#define RK3576_HDMI_EMP_MEM_LEN_EN	BIT(14)
#define RK3576_HDMI_HDCP14_MEM_EN	BIT(15)

#define RK3576_VO0_GRF_SOC_CON8		0x0020
#define RK3576_COLOR_FORMAT_MASK	(0xf << 4)
#define RK3576_COLOR_DEPTH_MASK		(0xf << 8)
#define RK3576_RGB                     (0 << 4)
#define RK3576_YUV422                  (0x1 << 4)
#define RK3576_YUV444                  (0x2 << 4)
#define RK3576_YUV420                  (0x3 << 4)
#define RK3576_8BPC                    (0x0 << 8)
#define RK3576_10BPC                   (0x6 << 8)
#define RK3576_CECIN_MASK		BIT(3)

#define RK3576_VO0_GRF_SOC_CON12	0x0030
#define RK3576_GRF_OSDA_DLYN		(0xf << 12)
#define RK3576_GRF_OSDA_DIV		(0x7f << 1)
#define RK3576_GRF_OSDA_DLY_EN		BIT(0)

#define RK3576_VO0_GRF_SOC_CON14	0x0038
#define RK3576_I2S_SEL_MASK		BIT(0)
#define RK3576_SPDIF_SEL_MASK		BIT(1)
#define HDCP0_P1_GPIO_IN		BIT(2)
#define RK3576_SCLIN_MASK		BIT(4)
#define RK3576_SDAIN_MASK		BIT(5)
#define RK3576_HDMITX_GRANT_SEL		BIT(6)

#define RK3588_GRF_SOC_CON2		0x0308
#define RK3588_HDMI1_HPD_INT_MSK	BIT(15)
#define RK3588_HDMI1_HPD_INT_CLR	BIT(14)
#define RK3588_HDMI0_HPD_INT_MSK	BIT(13)
#define RK3588_HDMI0_HPD_INT_CLR	BIT(12)
#define RK3588_GRF_SOC_CON7		0x031c
#define RK3588_SET_HPD_PATH_MASK	(0x3 << 12)
#define RK3588_GRF_SOC_CON12		0x0330
#define RK3588_SET_DLY_EN_MASK		(0x3f << 8)
#define RK3588_SET_DLY_EN		BIT(8)
#define RK3588_SET_LNUM_MS_MASK		0xff
#define RK3588_GRF_SOC_CON13		0x0334
#define RK3588_GRF_SOC_STATUS1		0x0384
#define RK3588_HDMI0_LOW_MORETHAN100MS	BIT(20)
#define RK3588_HDMI0_HPD_PORT_LEVEL	BIT(19)
#define RK3588_HDMI0_IHPD_PORT		BIT(18)
#define RK3588_HDMI0_OHPD_INT		BIT(17)
#define RK3588_HDMI0_LEVEL_INT		BIT(16)
#define RK3588_HDMI0_INTR_CHANGE_CNT	(0x7 << 13)
#define RK3588_HDMI1_LOW_MORETHAN100MS	BIT(28)
#define RK3588_HDMI1_HPD_PORT_LEVEL	BIT(27)
#define RK3588_HDMI1_IHPD_PORT		BIT(26)
#define RK3588_HDMI1_OHPD_INT		BIT(25)
#define RK3588_HDMI1_LEVEL_INT		BIT(24)
#define RK3588_HDMI1_INTR_CHANGE_CNT	(0x7 << 21)

#define RK3588_GRF_VO1_CON1		0x0004
#define HDCP1_P1_GPIO_IN		BIT(9)
#define RK3588_GRF_VO1_CON3		0x000c
#define RK3588_COLOR_FORMAT_MASK	0xf
#define RK3588_RGB			0
#define RK3588_YUV422			0x1
#define RK3588_YUV444			0x2
#define RK3588_YUV420			0x3
#define RK3588_COMPRESSED_DATA		0xb
#define RK3588_COLOR_DEPTH_MASK		(0xf << 4)
#define RK3588_8BPC			0
#define RK3588_10BPC			(0x6 << 4)
#define RK3588_CECIN_MASK		BIT(8)
#define RK3588_SCLIN_MASK		BIT(9)
#define RK3588_SDAIN_MASK		BIT(10)
#define RK3588_MODE_MASK		BIT(11)
#define RK3588_COMPRESS_MODE_MASK	BIT(12)
#define RK3588_I2S_SEL_MASK		BIT(13)
#define RK3588_SPDIF_SEL_MASK		BIT(14)
#define RK3588_GRF_VO1_CON4		0x0010
#define RK3588_HDMI21_MASK		BIT(0)
#define RK3588_GRF_VO1_CON9		0x0024
#define RK3588_HDMI0_GRANT_SEL		BIT(10)
#define RK3588_HDMI0_GRANT_SW		BIT(11)
#define RK3588_HDMI1_GRANT_SEL		BIT(12)
#define RK3588_HDMI1_GRANT_SW		BIT(13)
#define RK3588_GRF_VO1_CON4		0x0010
#define RK3588_HDMI_HDCP14_MEM_EN	BIT(15)
#define RK3588_GRF_VO1_CON6		0x0018
#define RK3588_GRF_VO1_CON7		0x001c

#define COLOR_DEPTH_10BIT		BIT(31)
#define HDMI_FRL_MODE			BIT(30)
#define HDMI_EARC_MODE			BIT(29)
#define DATA_RATE_MASK			0xFFFFFFF

#define HDMI14_MAX_RATE                 340000
#define HDMI20_MAX_RATE			600000
#define HDMI_8K60_RATE			2376000

#define HDMI_MAX_VRR_REFRESH_RATE	120
#define HDMI_MIN_VRR_REFRESH_RATE	24

/* Pixel rates in KPixels/sec */
#define HDMI_DSC_PEAK_PIXEL_RATE		2720000
/*
 * Rates at which the source and sink are required to process pixels in each
 * slice, can be two levels: either at least 340000KHz or at least 40000KHz.
 */
#define HDMI_DSC_MAX_ENC_THROUGHPUT_0		340000
#define HDMI_DSC_MAX_ENC_THROUGHPUT_1		400000

/* Spec limits the slice width to 2720 pixels */
#define MAX_HDMI_SLICE_WIDTH			2720

#define HDMI_COLORSPACE_CAPS_V1		(DRM_MODE_COLORIMETRY_DEFAULT | \
					DRM_MODE_COLORIMETRY_SMPTE_170M_YCC | \
					DRM_MODE_COLORIMETRY_BT709_YCC | \
					DRM_MODE_COLORIMETRY_BT2020_RGB)

#define HDMI_COLORSPACE_CAPS_V2		(HDMI_COLORSPACE_CAPS_V1 | \
					DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65)

struct rockchip_dw_hdmi_qp;

struct rockchip_hdmi_chip_ops {
	void (*set_link_mode)(struct rockchip_dw_hdmi_qp *hdmi);
	void (*set_color_format)(struct rockchip_dw_hdmi_qp *hdmi, u64 bus_format, u32 depth);
	void (*get_grf_color_fmt)(struct rockchip_dw_hdmi_qp *hdmi, u32 *fmt, u32 *depth);
	void (*io_path_init)(struct rockchip_dw_hdmi_qp *hdmi);
	irqreturn_t (*hdmi_hardirq)(int irq, void *dev_id);
	irqreturn_t (*hdmi_thread)(int irq, void *dev_id);
	void (*set_hdcp14_mem)(struct rockchip_dw_hdmi_qp *hdmi, bool enable);
	void (*set_hdcp2_enable)(struct rockchip_dw_hdmi_qp *hdmi, bool enable);
	void (*set_emp_bypass_enable)(struct rockchip_dw_hdmi_qp *hdmi, bool enable);
};

/**
 * struct rockchip_hdmi_chip_data - split the grf setting of kind of chips
 * @ddc_en_reg: grf register offset of hdmi ddc enable
 * @ops: hdmi grf config functions for different chips
 */
struct rockchip_hdmi_chip_data {
	int	ddc_en_reg;
	bool	split_mode;
	const struct rockchip_hdmi_chip_ops *ops;
};

enum hdmi_vrr_state {
	VRR_IS_DISABLED = 0,
	VRR_GOTO_ENABLE,
	VRR_GOTO_DISABLE,
	VRR_RATE_CHANGED,
	VRR_IS_STABLE,
	VRR_WAIT_REFRESH_CHANGE,
};

enum {
	GAMING_VRR_SUPPORTED = 0,
	QMS_VRR_SUPPORTED,
	FVA_SUPPORTED,
};

struct hdmi_vrr_capacity {
	u32 vrr_mode;
	u16 qms_vrr_rate_max;
	u16 qms_vrr_rate_min;
	u16 gaming_vrr_rate_min;
	u16 gaming_vrr_rate_max;
};

struct rockchip_dw_hdmi_qp {
	struct device *dev;
	struct regmap *regmap;
	struct regmap *vo0_regmap;
	struct regmap *vo1_regmap;
	struct drm_encoder encoder;
	struct drm_device *drm_dev;
	const struct rockchip_hdmi_chip_data *chip_data;
	struct dw_hdmi_plat_data *plat_data;
	struct clk *phyref_clk;
	struct clk *hdmitx_ref;
	struct clk *link_clk;
	struct dw_hdmi_qp *hdmi_qp;

	struct phy *phy;

	u32 max_tmdsclk;
	bool skip_check_420_mode;
	bool hpd_wake_en;
	u8 force_output;
	u8 id;
	bool hpd_stat;
	enum dw_hdmi_qp_version dw_hdmi_qp_version;
	bool force_disable_dsc;
	bool cec_wakeup_supported;
	bool dynamic_hdr_en;

	unsigned long bus_format;
	unsigned long output_bus_format;
	unsigned long enc_out_encoding;
	unsigned long prev_bus_format;
	int color_changed;
	int hpd_irq;

	struct drm_property *color_depth_property;
	struct drm_property *hdmi_output_property;
	struct drm_property *colordepth_capacity;
	struct drm_property *outputmode_capacity;
	struct drm_property *quant_range;
	struct drm_property *hdr_panel_metadata_property;
	struct drm_property *output_hdmi_dvi;
	struct drm_property *output_type_capacity;
	struct drm_property *allm_capacity;
	struct drm_property *allm_enable;
	struct drm_property *hdcp_state_property;
	struct drm_property *mode_color_capacity;
	struct drm_property *hdr_panel_dovi_vsdb;
	struct drm_property *vsif_data;
	struct drm_property *hdr10_plus_vsdb;
	struct drm_property *gaming_vrr_enable;
	struct drm_property *next_tfr;
	struct drm_property *fva_factor_m1;
	struct drm_property *hdmi_vrr_cap;
	struct drm_property *hdmi_colorspace_caps;
	struct drm_property *hdrvivid_vsdb;
	struct drm_property *dynamic_hdr_enable;

	struct drm_property_blob *mode_color_caps_ptr;
	struct drm_property_blob *hdr_panel_blob_ptr;
	struct drm_property_blob *hdr_panel_dovi_vsdb_ptr;
	struct drm_property_blob *vsif_data_ptr;
	struct drm_property_blob *hdr10_plus_vsdb_ptr;
	struct drm_property_blob *hdmi_vrr_cap_ptr;
	struct drm_property_blob *hdrvivid_vsdb_ptr;
	struct drm_property_blob *mode_infos_blob_ptr;

	unsigned int colordepth;
	unsigned int colorimetry;
	unsigned int hdmi_quant_range;
	unsigned int phy_bus_width;
	unsigned int enable_allm;
	unsigned int enable_gaming_vrr;
	u8 next_tfr_val;
	u8 fva_factor_m1_val;
	enum hdmi_vrr_state vrr_state;
	enum hdmi_vrr_state old_vrr_state;
	enum TARGET_FRAME_RATE brr_tfr;
	struct hdmi_vrr_capacity vrr_cap;
	enum rk_if_color_format hdmi_output;
	struct rockchip_drm_sub_dev sub_dev;

	u64 force_frl_rate;
	u32 edid_colorimetry;
	u8 hdcp_status;
	u8 dovi_vsdb[DOVI_VSDB_LEN];
	struct hdr10_plus_vsdb hdr10_plus_data;
	struct dw_hdmi_link_config link_cfg;
	struct gpio_desc *enable_gpio;

	struct delayed_work hpd_work;
	struct work_struct qms_vrr_work;
	struct workqueue_struct *workqueue;
	struct rockchip_drm_mode_color_caps *mode_color_caps;
	bool timing_force_output;
	struct drm_display_mode force_mode;
	struct rockchip_drm_hdmi21_data hdmi21_data;
	u32 force_bus_format;
	u32 sda_falling_delay_ns;

	u8 hdrvivid_vsvdb[HDRVIVID_VSVDB_LEN];
};

#define to_rockchip_hdmi(x)	container_of(x, struct rockchip_dw_hdmi_qp, x)

enum ROW_INDEX_BPP {
	ROW_INDEX_6BPP = 0,
	ROW_INDEX_8BPP,
	ROW_INDEX_10BPP,
	ROW_INDEX_12BPP,
	ROW_INDEX_23BPP,
	MAX_ROW_INDEX
};

enum COLUMN_INDEX_BPC {
	COLUMN_INDEX_8BPC = 0,
	COLUMN_INDEX_10BPC,
	COLUMN_INDEX_12BPC,
	COLUMN_INDEX_14BPC,
	COLUMN_INDEX_16BPC,
	MAX_COLUMN_INDEX
};

#define PPS_BPP_LEN 4
#define PPS_BPC_LEN 2

struct pps_data {
	u32 pic_width;
	u32 pic_height;
	u32 slice_width;
	u32 slice_height;
	bool convert_rgb;
	u8 bpc;
	u8 bpp;
	u8 raw_pps[128];
};

/*
 * Selected Rate Control Related Parameter Recommended Values
 * from DSC_v1.11 spec & C Model release: DSC_model_20161212
 */
static struct pps_data pps_datas[] = {
	{
		/* 7680x4320/960X96 rgb 8bpc 12bpp */
		7680, 4320, 960, 96, 1, 8, 192,
		{
			0x12, 0x00, 0x00, 0x8d, 0x30, 0xc0, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x05, 0xa0,
			0x01, 0x55, 0x03, 0x90, 0x00, 0x0a, 0x05, 0xc9,
			0x00, 0xa0, 0x00, 0x0f, 0x01, 0x44, 0x01, 0xaa,
			0x08, 0x00, 0x10, 0xf4, 0x03, 0x0c, 0x20, 0x00,
			0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x00, 0x82, 0x00, 0xc0, 0x09, 0x00,
			0x09, 0x7e, 0x19, 0xbc, 0x19, 0xba, 0x19, 0xf8,
			0x1a, 0x38, 0x1a, 0x38, 0x1a, 0x76, 0x2a, 0x76,
			0x2a, 0x76, 0x2a, 0x74, 0x3a, 0xb4, 0x52, 0xf4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 8bpc 11bpp */
		7680, 4320, 960, 96, 1, 8, 176,
		{
			0x12, 0x00, 0x00, 0x8d, 0x30, 0xb0, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x05, 0x28,
			0x01, 0x74, 0x03, 0x40, 0x00, 0x0f, 0x06, 0xe0,
			0x00, 0x2d, 0x00, 0x0f, 0x01, 0x44, 0x01, 0x33,
			0x0f, 0x00, 0x10, 0xf4, 0x03, 0x0c, 0x20, 0x00,
			0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x00, 0x82, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
			0x1a, 0x38, 0x1a, 0x38, 0x1a, 0x76, 0x2a, 0x76,
			0x2a, 0x76, 0x2a, 0xb4, 0x3a, 0xb4, 0x52, 0xf4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 8bpc 10bpp */
		7680, 4320, 960, 96, 1, 8, 160,
		{
			0x12, 0x00, 0x00, 0x8d, 0x30, 0xa0, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x04, 0xb0,
			0x01, 0x9a, 0x02, 0xe0, 0x00, 0x19, 0x09, 0xb0,
			0x00, 0x12, 0x00, 0x0f, 0x01, 0x44, 0x00, 0xbb,
			0x16, 0x00, 0x10, 0xec, 0x03, 0x0c, 0x20, 0x00,
			0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x00, 0xc2, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
			0x1a, 0x38, 0x1a, 0x78, 0x1a, 0x76, 0x2a, 0xb6,
			0x2a, 0xb6, 0x2a, 0xf4, 0x3a, 0xf4, 0x5b, 0x34,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 8bpc 9bpp */
		7680, 4320, 960, 96, 1, 8, 144,
		{
			0x12, 0x00, 0x00, 0x8d, 0x30, 0x90, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x04, 0x38,
			0x01, 0xc7, 0x03, 0x16, 0x00, 0x1c, 0x08, 0xc7,
			0x00, 0x10, 0x00, 0x0f, 0x01, 0x44, 0x00, 0xaa,
			0x17, 0x00, 0x10, 0xf1, 0x03, 0x0c, 0x20, 0x00,
			0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x00, 0xc2, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
			0x1a, 0x38, 0x1a, 0x78, 0x1a, 0x76, 0x2a, 0xb6,
			0x2a, 0xb6, 0x2a, 0xf4, 0x3a, 0xf4, 0x63, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 8bpc 8bpp */
		7680, 4320, 960, 96, 1, 8, 128,
		{
			0x12, 0x00, 0x00, 0x8d, 0x30, 0x80, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x03, 0xc0,
			0x02, 0x00, 0x03, 0x58, 0x00, 0x20, 0x0a, 0x63,
			0x00, 0x0d, 0x00, 0x0f, 0x01, 0x44, 0x00, 0x99,
			0x18, 0x00, 0x10, 0xf0, 0x03, 0x0c, 0x20, 0x00,
			0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
			0x1a, 0x38, 0x1a, 0x78, 0x22, 0xb6, 0x2a, 0xb6,
			0x2a, 0xf6, 0x2a, 0xf4, 0x43, 0x34, 0x63, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 10bpc 12bpp */
		7680, 4320, 960, 96, 1, 10, 192,
		{
			0x12, 0x00, 0x00, 0xad, 0x30, 0xc0, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x05, 0xa0,
			0x01, 0x55, 0x03, 0x90, 0x00, 0x0a, 0x05, 0xc9,
			0x00, 0xa0, 0x00, 0x0f, 0x01, 0x44, 0x01, 0xaa,
			0x08, 0x00, 0x10, 0xf4, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0f, 0x0f, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x01, 0x02, 0x11, 0x80, 0x22, 0x00,
			0x22, 0x7e, 0x32, 0xbc, 0x32, 0xba, 0x3a, 0xf8,
			0x3b, 0x38, 0x3b, 0x38, 0x3b, 0x76, 0x4b, 0x76,
			0x4b, 0x76, 0x4b, 0x74, 0x5b, 0xb4, 0x73, 0xf4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 10bpc 11bpp */
		7680, 4320, 960, 96, 1, 10, 176,
		{
			0x12, 0x00, 0x00, 0xad, 0x30, 0xb0, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x05, 0x28,
			0x01, 0x74, 0x03, 0x40, 0x00, 0x0f, 0x06, 0xe0,
			0x00, 0x2d, 0x00, 0x0f, 0x01, 0x44, 0x01, 0x33,
			0x0f, 0x00, 0x10, 0xf4, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0f, 0x0f, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x01, 0x42, 0x19, 0xc0, 0x2a, 0x40,
			0x2a, 0xbe, 0x3a, 0xfc, 0x3a, 0xfa, 0x3a, 0xf8,
			0x3b, 0x38, 0x3b, 0x38, 0x3b, 0x76, 0x4b, 0x76,
			0x4b, 0x76, 0x4b, 0xb4, 0x5b, 0xb4, 0x73, 0xf4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 10bpc 10bpp */
		7680, 4320, 960, 96, 1, 10, 160,
		{
			0x12, 0x00, 0x00, 0xad, 0x30, 0xa0, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x04, 0xb0,
			0x01, 0x9a, 0x02, 0xe0, 0x00, 0x19, 0x09, 0xb0,
			0x00, 0x12, 0x00, 0x0f, 0x01, 0x44, 0x00, 0xbb,
			0x16, 0x00, 0x10, 0xec, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0f, 0x0f, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x01, 0xc2, 0x22, 0x00, 0x2a, 0x40,
			0x2a, 0xbe, 0x3a, 0xfc, 0x3a, 0xfa, 0x3a, 0xf8,
			0x3b, 0x38, 0x3b, 0x78, 0x3b, 0x76, 0x4b, 0xb6,
			0x4b, 0xb6, 0x4b, 0xf4, 0x63, 0xf4, 0x7c, 0x34,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 10bpc 9bpp */
		7680, 4320, 960, 96, 1, 10, 144,
		{
			0x12, 0x00, 0x00, 0xad, 0x30, 0x90, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x04, 0x38,
			0x01, 0xc7, 0x03, 0x16, 0x00, 0x1c, 0x08, 0xc7,
			0x00, 0x10, 0x00, 0x0f, 0x01, 0x44, 0x00, 0xaa,
			0x17, 0x00, 0x10, 0xf1, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0f, 0x0f, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x01, 0xc2, 0x22, 0x00, 0x2a, 0x40,
			0x2a, 0xbe, 0x3a, 0xfc, 0x3a, 0xfa, 0x3a, 0xf8,
			0x3b, 0x38, 0x3b, 0x78, 0x3b, 0x76, 0x4b, 0xb6,
			0x4b, 0xb6, 0x4b, 0xf4, 0x63, 0xf4, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
	{
		/* 7680x4320/960X96 rgb 10bpc 8bpp */
		7680, 4320, 960, 96, 1, 10, 128,
		{
			0x12, 0x00, 0x00, 0xad, 0x30, 0x80, 0x10, 0xe0,
			0x1e, 0x00, 0x00, 0x60, 0x03, 0xc0, 0x03, 0xc0,
			0x02, 0x00, 0x03, 0x58, 0x00, 0x20, 0x0a, 0x63,
			0x00, 0x0d, 0x00, 0x0f, 0x01, 0x44, 0x00, 0x99,
			0x18, 0x00, 0x10, 0xf0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0f, 0x0f, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			0x7d, 0x7e, 0x02, 0x02, 0x22, 0x00, 0x2a, 0x40,
			0x2a, 0xbe, 0x3a, 0xfc, 0x3a, 0xfa, 0x3a, 0xf8,
			0x3b, 0x38, 0x3b, 0x78, 0x43, 0xb6, 0x4b, 0xb6,
			0x4b, 0xf6, 0x4b, 0xf4, 0x64, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		},
	},
};

static int rockchip_hdmi_match_by_id(struct device *dev, const void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_get_drvdata(dev);
	const unsigned int *id = data;

	return hdmi->id == *id;
}

static struct rockchip_dw_hdmi_qp *
rockchip_hdmi_find_by_id(struct device_driver *drv, unsigned int id)
{
	struct device *dev;

	dev = driver_find_device(drv, NULL, &id, rockchip_hdmi_match_by_id);
	if (!dev)
		return NULL;

	return dev_get_drvdata(dev);
}

static bool rockchip_hdmi_check_dsc_rate_supported(struct rockchip_dw_hdmi_qp *hdmi,
						   u64 tmdsclk, u8 bpp)
{
	u64 data_rate, dsc_rate;
	u64 frl_rate, dsc_frl_rate;

	frl_rate = (u64)hdmi->hdmi21_data.max_lanes *
		hdmi->hdmi21_data.max_frl_rate_per_lane * 1000000000;
	dsc_frl_rate = (u64)hdmi->hdmi21_data.dsc_cap.max_lanes *
		hdmi->hdmi21_data.dsc_cap.max_frl_rate_per_lane * 1000000000;
	data_rate = (u64)tmdsclk * bpp;
	data_rate = DIV_ROUND_UP_ULL(data_rate * 18, 16);
	/* compression ratio needs to be greater than 0.375. */
	dsc_rate = DIV_ROUND_UP_ULL(data_rate * 9, 24);

	if ((data_rate > frl_rate) && (dsc_rate < dsc_frl_rate))
		return true;

	return false;
}

static bool rockchip_hdmi_if_dsc_enable(struct rockchip_dw_hdmi_qp *hdmi, unsigned int tmdsclk)
{
	u8 bpp = rockchip_drm_bus_fmt_color_depth(hdmi->bus_format) * 3;

	/* rk3588 dsc can't support yuv420/422 dsc */
	if (rockchip_drm_bus_fmt_is_yuv420(hdmi->bus_format) ||
	    rockchip_drm_bus_fmt_is_yuv422(hdmi->bus_format))
		return false;

	return rockchip_hdmi_check_dsc_rate_supported(hdmi, tmdsclk, bpp);
}

static void hdmi_select_link_config(struct rockchip_dw_hdmi_qp *hdmi,
				    struct drm_crtc_state *crtc_state, unsigned int tmdsclk)
{
	struct drm_display_mode mode = {};
	int max_lanes, max_rate_per_lane;
	int max_dsc_lanes, max_dsc_rate_per_lane;
	unsigned long max_frl_rate;

	drm_mode_copy(&mode, &crtc_state->mode);
	if (hdmi->plat_data->split_mode || hdmi->plat_data->dual_connector_split)
		drm_mode_convert_to_origin_mode(&mode);

	max_lanes = hdmi->hdmi21_data.max_lanes;
	max_rate_per_lane = hdmi->hdmi21_data.max_frl_rate_per_lane;
	max_frl_rate = max_lanes * max_rate_per_lane * 1000000;

	hdmi->link_cfg.dsc_mode = false;
	hdmi->link_cfg.frl_lanes = max_lanes;
	hdmi->link_cfg.rate_per_lane = max_rate_per_lane;
	hdmi->link_cfg.allm_supported = hdmi->hdmi21_data.allm_supported;

	if (!max_frl_rate || (tmdsclk < HDMI20_MAX_RATE && mode.clock < HDMI20_MAX_RATE) ||
	    hdmi->plat_data->dw_hdmi_qp_version == DW_HDMI_QP_V2) {
		dev_dbg(hdmi->dev, "use tmds mode\n");
		hdmi->link_cfg.frl_mode = false;
		return;
	}

	hdmi->link_cfg.frl_mode = true;

	if (!hdmi->hdmi21_data.dsc_cap.v_1p2)
		return;

	max_dsc_lanes = hdmi->hdmi21_data.dsc_cap.max_lanes;
	max_dsc_rate_per_lane =
		hdmi->hdmi21_data.dsc_cap.max_frl_rate_per_lane;

	if (rockchip_hdmi_if_dsc_enable(hdmi,  tmdsclk * 1000)) {
		hdmi->link_cfg.dsc_mode = true;
		hdmi->link_cfg.frl_lanes = max_dsc_lanes;
		hdmi->link_cfg.rate_per_lane = max_dsc_rate_per_lane;
	} else {
		hdmi->link_cfg.dsc_mode = false;
		hdmi->link_cfg.frl_lanes = max_lanes;
		hdmi->link_cfg.rate_per_lane = max_rate_per_lane;
	}
}

/////////////////////////////////////////////////////////////////////////////////////

static int hdmi_dsc_get_slice_height(int vactive)
{
	int slice_height;

	/*
	 * Slice Height determination : HDMI2.1 Section 7.7.5.2
	 * Select smallest slice height >=96, that results in a valid PPS and
	 * requires minimum padding lines required for final slice.
	 *
	 * Assumption : Vactive is even.
	 */
	for (slice_height = 96; slice_height <= vactive; slice_height += 2)
		if (vactive % slice_height == 0)
			return slice_height;

	return 0;
}

static int hdmi_dsc_get_num_slices(struct rockchip_dw_hdmi_qp *hdmi,
				   struct drm_crtc_state *crtc_state,
				   int src_max_slices, int src_max_slice_width,
				   int hdmi_max_slices, int hdmi_throughput)
{
	int kslice_adjust;
	int adjusted_clk_khz;
	int min_slices;
	int target_slices;
	int max_throughput; /* max clock freq. in khz per slice */
	int max_slice_width;
	int slice_width;
	int pixel_clock = crtc_state->mode.clock;

	if (!hdmi_throughput)
		return 0;

	/*
	 * Slice Width determination : HDMI2.1 Section 7.7.5.1
	 * kslice_adjust factor for 4:2:0, and 4:2:2 formats is 0.5, where as
	 * for 4:4:4 is 1.0. Multiplying these factors by 10 and later
	 * dividing adjusted clock value by 10.
	 */
	if (rockchip_drm_bus_fmt_is_yuv444(hdmi->output_bus_format) ||
	    rockchip_drm_bus_fmt_is_rgb(hdmi->output_bus_format))
		kslice_adjust = 10;
	else
		kslice_adjust = 5;

	/*
	 * As per spec, the rate at which the source and the sink process
	 * the pixels per slice are at two levels: at least 340Mhz or 400Mhz.
	 * This depends upon the pixel clock rate and output formats
	 * (kslice adjust).
	 * If pixel clock * kslice adjust >= 2720MHz slices can be processed
	 * at max 340MHz, otherwise they can be processed at max 400MHz.
	 */

	adjusted_clk_khz = DIV_ROUND_UP(kslice_adjust * pixel_clock, 10);

	if (adjusted_clk_khz <= HDMI_DSC_PEAK_PIXEL_RATE)
		max_throughput = HDMI_DSC_MAX_ENC_THROUGHPUT_0;
	else
		max_throughput = HDMI_DSC_MAX_ENC_THROUGHPUT_1;

	/*
	 * Taking into account the sink's capability for maximum
	 * clock per slice (in MHz) as read from HF-VSDB.
	 */
	max_throughput = min(max_throughput, hdmi_throughput * 1000);

	min_slices = DIV_ROUND_UP(adjusted_clk_khz, max_throughput);
	max_slice_width = min(MAX_HDMI_SLICE_WIDTH, src_max_slice_width);

	/*
	 * Keep on increasing the num of slices/line, starting from min_slices
	 * per line till we get such a number, for which the slice_width is
	 * just less than max_slice_width. The slices/line selected should be
	 * less than or equal to the max horizontal slices that the combination
	 * of PCON encoder and HDMI decoder can support.
	 */
	do {
		if (min_slices <= 1 && src_max_slices >= 1 && hdmi_max_slices >= 1)
			target_slices = 1;
		else if (min_slices <= 2 && src_max_slices >= 2 && hdmi_max_slices >= 2)
			target_slices = 2;
		else if (min_slices <= 4 && src_max_slices >= 4 && hdmi_max_slices >= 4)
			target_slices = 4;
		else if (min_slices <= 8 && src_max_slices >= 8 && hdmi_max_slices >= 8)
			target_slices = 8;
		else if (min_slices <= 12 && src_max_slices >= 12 && hdmi_max_slices >= 12)
			target_slices = 12;
		else if (min_slices <= 16 && src_max_slices >= 16 && hdmi_max_slices >= 16)
			target_slices = 16;
		else
			return 0;

		slice_width = DIV_ROUND_UP(crtc_state->mode.hdisplay, target_slices);
		if (slice_width > max_slice_width)
			min_slices = target_slices + 1;
	} while (slice_width > max_slice_width);

	return target_slices;
}

static int hdmi_dsc_slices(struct rockchip_dw_hdmi_qp *hdmi,
			   struct drm_crtc_state *crtc_state)
{
	int hdmi_throughput = hdmi->hdmi21_data.dsc_cap.clk_per_slice;
	int hdmi_max_slices = hdmi->hdmi21_data.dsc_cap.max_slices;
	int rk_max_slices = 8;
	int rk_max_slice_width = 2048;

	return hdmi_dsc_get_num_slices(hdmi, crtc_state, rk_max_slices,
				       rk_max_slice_width,
				       hdmi_max_slices, hdmi_throughput);
}

static int hdmi_dsc_get_bpp(struct rockchip_dw_hdmi_qp *hdmi, int src_fractional_bpp,
			    int slice_width, int num_slices, bool hdmi_all_bpp,
			    int hdmi_max_chunk_bytes, u64 pixel_clk)
{
	int max_dsc_bpp, min_dsc_bpp;
	int target_bytes;
	bool bpp_found = false;
	int bpp_decrement_x16;
	int bpp_target;
	int bpp_target_x16;
	u64 frl_rate, dsc_rate, original_rate;
	u8 original_bpp = rockchip_drm_bus_fmt_color_depth(hdmi->output_bus_format) * 3;

	if (!original_bpp) {
		dev_err(hdmi->dev, "can't get original_bpp\n");
		return 0;
	}
	/*
	 * Get min bpp and max bpp as per Table 7.23, in HDMI2.1 spec
	 * Start with the max bpp and keep on decrementing with
	 * fractional bpp, if supported by PCON DSC encoder
	 *
	 * for each bpp we check if no of bytes can be supported by HDMI sink
	 */

	min_dsc_bpp = 8;
	max_dsc_bpp = 12;

	/*
	 * Taking into account if all dsc_all_bpp supported by HDMI2.1 sink
	 * Section 7.7.34 : Source shall not enable compressed Video
	 * Transport with bpp_target settings above 12 bpp unless
	 * DSC_all_bpp is set to 1.
	 */
	if (!hdmi_all_bpp)
		max_dsc_bpp = min(max_dsc_bpp, 12);

	/*
	 * The Sink has a limit of compressed data in bytes for a scanline,
	 * as described in max_chunk_bytes field in HFVSDB block of edid.
	 * The no. of bytes depend on the target bits per pixel that the
	 * source configures. So we start with the max_bpp and calculate
	 * the target_chunk_bytes. We keep on decrementing the target_bpp,
	 * till we get the target_chunk_bytes just less than what the sink's
	 * max_chunk_bytes, or else till we reach the min_dsc_bpp.
	 *
	 * The decrement is according to the fractional support from PCON DSC
	 * encoder. For fractional BPP we use bpp_target as a multiple of 16.
	 *
	 * bpp_target_x16 = bpp_target * 16
	 * So we need to decrement by {1, 2, 4, 8, 16} for fractional bpps
	 * {1/16, 1/8, 1/4, 1/2, 1} respectively.
	 */
	frl_rate = (u64)hdmi->link_cfg.frl_lanes * hdmi->link_cfg.rate_per_lane * 1000000;
	bpp_target = max_dsc_bpp;

	/* hdmi frl mode is 16b18b encoded */
	original_rate = DIV_ROUND_UP_ULL(pixel_clk * original_bpp * 18, 16);

	/* src does not support fractional bpp implies decrement by 16 for bppx16 */
	if (!src_fractional_bpp)
		src_fractional_bpp = 1;
	bpp_decrement_x16 = DIV_ROUND_UP(16, src_fractional_bpp);
	bpp_target_x16 = bpp_target * 16;

	while (bpp_target_x16 >= (min_dsc_bpp * 16)) {
		int bpp;

		bpp = DIV_ROUND_UP(bpp_target_x16, 16);
		target_bytes = DIV_ROUND_UP((num_slices * slice_width * bpp), 8);
		dsc_rate = DIV_ROUND_UP_ULL(original_rate * bpp_target_x16, original_bpp * 16);

		if (target_bytes <= hdmi_max_chunk_bytes && dsc_rate <= frl_rate) {
			bpp_found = true;
			break;
		}
		bpp_target_x16 -= bpp_decrement_x16;
	}
	if (bpp_found)
		return bpp_target_x16;

	return 0;
}

static int dw_hdmi_dsc_bpp(struct rockchip_dw_hdmi_qp *hdmi, int num_slices, int slice_width,
			   u64 pixel_clk)
{
	bool hdmi_all_bpp = hdmi->hdmi21_data.dsc_cap.all_bpp;
	int fractional_bpp = 0;
	int hdmi_max_chunk_bytes = hdmi->hdmi21_data.dsc_cap.total_chunk_kbytes * 1024;

	return hdmi_dsc_get_bpp(hdmi, fractional_bpp, slice_width,
				num_slices, hdmi_all_bpp,
				hdmi_max_chunk_bytes, pixel_clk);
}

static int dw_hdmi_qp_set_link_cfg(struct rockchip_dw_hdmi_qp *hdmi,
				   u16 pic_width, u16 pic_height,
				   u16 slice_width, u16 slice_height,
				   u16 bits_per_pixel, u8 bits_per_component)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pps_datas); i++)
		if (pic_width == pps_datas[i].pic_width &&
		    pic_height == pps_datas[i].pic_height &&
		    slice_width == pps_datas[i].slice_width &&
		    slice_height == pps_datas[i].slice_height &&
		    bits_per_component == pps_datas[i].bpc &&
		    bits_per_pixel == pps_datas[i].bpp)
			break;

	if (i == ARRAY_SIZE(pps_datas)) {
		dev_err(hdmi->dev, "can't find pps cfg!\n");
		return -EINVAL;
	}

	memcpy(hdmi->link_cfg.pps_payload, pps_datas[i].raw_pps, 128);

	/* if yuv dsc format */
	if (rockchip_drm_bus_fmt_is_rgb(hdmi->output_bus_format))
		hdmi->link_cfg.pps_payload[4] |= BIT(4);
	else
		hdmi->link_cfg.pps_payload[4] &= ~BIT(4);

	hdmi->link_cfg.hcactive = DIV_ROUND_UP(slice_width * (bits_per_pixel / 16), 8) *
		(pic_width / slice_width);

	return 0;
}

static void dw_hdmi_qp_dsc_configure(struct rockchip_dw_hdmi_qp *hdmi,
				     struct rockchip_crtc_state *s,
				     struct drm_crtc_state *crtc_state)
{
	int ret;
	int slice_height;
	int slice_width;
	int bits_per_pixel;
	int slice_count;
	bool hdmi_is_dsc_1_2;
	unsigned int depth = rockchip_drm_bus_fmt_color_depth(hdmi->output_bus_format);

	if (!crtc_state)
		return;

	hdmi_is_dsc_1_2 = hdmi->hdmi21_data.dsc_cap.v_1p2;

	if (!hdmi_is_dsc_1_2)
		return;

	if (rockchip_drm_bus_fmt_is_yuv422(hdmi->output_bus_format) ||
	    rockchip_drm_bus_fmt_is_yuv420(hdmi->output_bus_format)) {
		dev_err(hdmi->dev, "dsc can't support yuv422/420\n");
		return;
	}

	slice_height = hdmi_dsc_get_slice_height(crtc_state->mode.vdisplay);
	if (!slice_height)
		return;

	slice_count = hdmi_dsc_slices(hdmi, crtc_state);
	if (!slice_count)
		return;

	slice_width = DIV_ROUND_UP(crtc_state->mode.hdisplay, slice_count);

	bits_per_pixel = dw_hdmi_dsc_bpp(hdmi, slice_count, slice_width, crtc_state->mode.clock);
	if (!bits_per_pixel)
		return;

	ret = dw_hdmi_qp_set_link_cfg(hdmi, crtc_state->mode.hdisplay,
				      crtc_state->mode.vdisplay, slice_width,
				      slice_height, bits_per_pixel, depth);

	if (ret) {
		dev_err(hdmi->dev, "set vdsc cfg failed\n");
		return;
	}
	dev_info(hdmi->dev, "dsc_enable\n");
	s->dsc_enable = 1;
	s->dsc_sink_cap.version_major = 1;
	s->dsc_sink_cap.version_minor = 2;
	s->dsc_sink_cap.slice_width = slice_width;
	s->dsc_sink_cap.slice_height = slice_height;
	s->dsc_sink_cap.target_bits_per_pixel_x16 = bits_per_pixel;
	s->dsc_sink_cap.block_pred = 1;
	s->dsc_sink_cap.native_420 = 0;

	memcpy(&s->pps, hdmi->link_cfg.pps_payload, 128);
}
/////////////////////////////////////////////////////////////////////////////////////////

static void repo_hpd_event(struct work_struct *p_work)
{
	struct rockchip_dw_hdmi_qp *hdmi =
		container_of(p_work, struct rockchip_dw_hdmi_qp, hpd_work.work);
	bool change;

	change = drm_helper_hpd_irq_event(hdmi->drm_dev);
	if (change) {
		dev_dbg(hdmi->dev, "hpd stat changed:%d\n", hdmi->hpd_stat);
		dw_hdmi_qp_handle_hpd(hdmi->hdmi_qp, hdmi->hpd_stat);
		dw_hdmi_qp_cec_set_hpd(hdmi->hdmi_qp, hdmi->hpd_stat, change);
	}
}

static irqreturn_t rk3538_hdmi_hardirq(int irq, void *dev_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_id;
	u32 intr_stat, val;

	regmap_read(hdmi->regmap, RK3538_HDMI_HPD_ST, &intr_stat);

	if (intr_stat & RK3576_HDMITX_OHPD_INT) {
		dev_dbg(hdmi->dev, "hpd irq %#x\n", intr_stat);

		val = HIWORD_UPDATE(RK3538_HDMITX_HPD_INT_MSK,
				    RK3538_HDMITX_HPD_INT_MSK);

		regmap_write(hdmi->regmap, RK3538_HDMI_HPD_INT_CON, val);
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

static irqreturn_t rk3572_hdmi_hardirq(int irq, void *dev_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_id;
	u32 intr_stat, val;

	regmap_read(hdmi->regmap, RK3572_HDMITX_HPD_STATUS, &intr_stat);

	if (intr_stat & RK3572_HDMITX_OHPD_INT) {
		dev_dbg(hdmi->dev, "hpd irq %#x\n", intr_stat);

		val = HIWORD_UPDATE(RK3572_HDMITX_HPD_INT_MSK,
				    RK3572_HDMITX_HPD_INT_MSK);

		regmap_write(hdmi->regmap, RK3572_SYS_GRF_CON1, val);
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

static irqreturn_t rk3576_hdmi_hardirq(int irq, void *dev_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_id;
	u32 intr_stat, val;

	regmap_read(hdmi->regmap, RK3576_IOC_HDMITX_HPD_STATUS, &intr_stat);

	if (intr_stat & RK3576_HDMITX_OHPD_INT) {
		dev_dbg(hdmi->dev, "hpd irq %#x\n", intr_stat);

		val = HIWORD_UPDATE(RK3576_HDMITX_HPD_INT_MSK,
				    RK3576_HDMITX_HPD_INT_MSK);

		regmap_write(hdmi->regmap, RK3576_IOC_MISC_CON0, val);
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

static irqreturn_t rk3588_hdmi_hardirq(int irq, void *dev_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_id;
	u32 intr_stat, val;

	regmap_read(hdmi->regmap, RK3588_GRF_SOC_STATUS1, &intr_stat);

	if (!hdmi->id)
		intr_stat = intr_stat & RK3588_HDMI0_OHPD_INT;
	else
		intr_stat = intr_stat & RK3588_HDMI1_OHPD_INT;

	if (intr_stat) {
		dev_dbg(hdmi->dev, "hpd irq %#x\n", intr_stat);

		if (!hdmi->id)
			val = HIWORD_UPDATE(RK3588_HDMI0_HPD_INT_MSK,
					    RK3588_HDMI0_HPD_INT_MSK);
		else
			val = HIWORD_UPDATE(RK3588_HDMI1_HPD_INT_MSK,
					    RK3588_HDMI1_HPD_INT_MSK);
		regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

static irqreturn_t rk3538_hdmi_thread(int irq, void *dev_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_id;
	u32 intr_stat, val;
	int msecs;
	bool stat;

	regmap_read(hdmi->regmap, RK3538_HDMI_HPD_ST, &intr_stat);

	val = HIWORD_UPDATE(RK3538_HDMITX_HPD_INT_CLR,
			    RK3538_HDMITX_HPD_INT_CLR);
	regmap_write(hdmi->regmap, RK3538_HDMI_HPD_INT_CON, val);

	val = HIWORD_UPDATE(0, RK3538_HDMITX_HPD_INT_CLR);
	regmap_write(hdmi->regmap, RK3538_HDMI_HPD_INT_CON, val);

	if (intr_stat & RK3576_HDMITX_LEVEL_INT)
		stat = true;
	else
		stat = false;

	if (stat) {
		hdmi->hpd_stat = true;
		msecs = 150;
	} else {
		hdmi->hpd_stat = false;
		msecs = 20;
	}
	mod_delayed_work(hdmi->workqueue, &hdmi->hpd_work, msecs_to_jiffies(msecs));

	val = HIWORD_UPDATE(RK3538_HDMITX_HPD_INT_CLR,
			    RK3538_HDMITX_HPD_INT_CLR) |
	      HIWORD_UPDATE(0, RK3538_HDMITX_HPD_INT_MSK);

	regmap_write(hdmi->regmap, RK3538_HDMI_HPD_INT_CON, val);

	return IRQ_HANDLED;
}

static irqreturn_t rk3572_hdmi_thread(int irq, void *dev_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_id;
	u32 intr_stat, val;
	int msecs;
	bool stat;

	regmap_read(hdmi->regmap, RK3572_HDMITX_HPD_STATUS, &intr_stat);

	val = HIWORD_UPDATE(RK3572_HDMITX_HPD_INT_CLR,
			    RK3572_HDMITX_HPD_INT_CLR);
	regmap_write(hdmi->regmap, RK3572_SYS_GRF_CON1, val);

	val = HIWORD_UPDATE(0, RK3572_HDMITX_HPD_INT_CLR);
	regmap_write(hdmi->regmap, RK3572_SYS_GRF_CON1, val);

	if (intr_stat & RK3572_HDMITX_LEVEL_INT)
		stat = true;
	else
		stat = false;

	if (stat) {
		hdmi->hpd_stat = true;
		msecs = 150;
	} else {
		hdmi->hpd_stat = false;
		msecs = 20;
	}
	mod_delayed_work(hdmi->workqueue, &hdmi->hpd_work, msecs_to_jiffies(msecs));

	val = HIWORD_UPDATE(RK3572_HDMITX_HPD_INT_CLR,
			    RK3572_HDMITX_HPD_INT_CLR) |
	      HIWORD_UPDATE(0, RK3572_HDMITX_HPD_INT_MSK);

	regmap_write(hdmi->regmap, RK3572_SYS_GRF_CON1, val);

	return IRQ_HANDLED;
}

static irqreturn_t rk3576_hdmi_thread(int irq, void *dev_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_id;
	u32 intr_stat, val;
	int msecs;
	bool stat;

	regmap_read(hdmi->regmap, RK3576_IOC_HDMITX_HPD_STATUS, &intr_stat);

	val = HIWORD_UPDATE(RK3576_HDMITX_HPD_INT_CLR,
			    RK3576_HDMITX_HPD_INT_CLR);
	if (intr_stat & RK3576_HDMITX_LEVEL_INT)
		stat = true;
	else
		stat = false;

	regmap_write(hdmi->regmap, RK3576_IOC_MISC_CON0, val);

	if (stat) {
		hdmi->hpd_stat = true;
		msecs = 150;
	} else {
		hdmi->hpd_stat = false;
		msecs = 20;
	}
	mod_delayed_work(hdmi->workqueue, &hdmi->hpd_work, msecs_to_jiffies(msecs));

	val = HIWORD_UPDATE(RK3576_HDMITX_HPD_INT_CLR,
			    RK3576_HDMITX_HPD_INT_CLR) |
	      HIWORD_UPDATE(0, RK3576_HDMITX_HPD_INT_MSK);

	regmap_write(hdmi->regmap, RK3576_IOC_MISC_CON0, val);

	return IRQ_HANDLED;
}

static irqreturn_t rk3588_hdmi_thread(int irq, void *dev_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_id;
	u32 intr_stat, val;
	int msecs;
	bool stat;

	regmap_read(hdmi->regmap, RK3588_GRF_SOC_STATUS1, &intr_stat);

	if (!hdmi->id) {
		val = HIWORD_UPDATE(RK3588_HDMI0_HPD_INT_CLR, RK3588_HDMI0_HPD_INT_CLR);
		if (intr_stat & RK3588_HDMI0_LEVEL_INT)
			stat = true;
		else
			stat = false;
	} else {
		val = HIWORD_UPDATE(RK3588_HDMI1_HPD_INT_CLR, RK3588_HDMI1_HPD_INT_CLR);
		if (intr_stat & RK3588_HDMI1_LEVEL_INT)
			stat = true;
		else
			stat = false;
	}

	regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);

	if (stat) {
		hdmi->hpd_stat = true;
		msecs = 150;
	} else {
		hdmi->hpd_stat = false;
		msecs = 20;
	}
	mod_delayed_work(hdmi->workqueue, &hdmi->hpd_work, msecs_to_jiffies(msecs));

	if (!hdmi->id) {
		val = HIWORD_UPDATE(RK3588_HDMI0_HPD_INT_CLR, RK3588_HDMI0_HPD_INT_CLR) |
		      HIWORD_UPDATE(0, RK3588_HDMI0_HPD_INT_MSK);
	} else {
		val = HIWORD_UPDATE(RK3588_HDMI1_HPD_INT_CLR, RK3588_HDMI1_HPD_INT_CLR) |
		      HIWORD_UPDATE(0, RK3588_HDMI1_HPD_INT_MSK);
	}

	regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);

	return IRQ_HANDLED;
}

static int rockchip_hdmi_set_qms_next_tfr(struct rockchip_dw_hdmi_qp *hdmi, u64 val)
{
	if (val && hdmi->enable_gaming_vrr) {
		DRM_WARN("vrr-gaming is enabled, can't set next_tfr\n");
		return 0;
	}

	if (!hdmi->next_tfr_val && val) {
		hdmi->vrr_state = VRR_GOTO_ENABLE;
	} else if (hdmi->next_tfr_val && !val) {
		if (hdmi->vrr_state != VRR_IS_STABLE) {
			DRM_WARN("qms-vrr is switching, can't disable qms-vrr\n");
			return 0;
		}
		hdmi->vrr_state = VRR_GOTO_DISABLE;
	} else if (hdmi->next_tfr_val && val && hdmi->next_tfr_val != val) {
		if (hdmi->vrr_state != VRR_IS_STABLE) {
			DRM_WARN("qms-vrr is switching, can't set new next_tfr\n");
			return 0;
		}
		hdmi->vrr_state = VRR_RATE_CHANGED;
	} else {
		/* new next_tfr value is the same as the old one */
		return 0;
	}

	hdmi->next_tfr_val = (u8)val;
	return 0;
}

static int rockchip_hdmi_get_next_tfr_val(struct drm_display_mode *mode)
{
	u64 val = 0;
	int i;

	val = (u64)mode->clock * 100000;
	val = DIV_ROUND_CLOSEST_ULL(val, (mode->htotal * mode->vtotal));

	for (i = 0; i < TFR_MAX; i++) {
		if (rockchip_hdmi_vrr_tfr_match_to_vrefresh(i) == val)
			return i;
	}

	return -EINVAL;
}

static u32 rockchip_hdmi_get_next_tfr_refresh_val(u8 next_tfr)
{
	int i;

	for (i = 1; i < TFR_MAX; i++) {
		if (i == next_tfr)
			break;
	}

	if (i == TFR_MAX)
		return -EINVAL;

	return rockchip_hdmi_vrr_tfr_match_to_vrefresh(i);
}

static int rockchip_hdmi_wait_vsync(struct rockchip_dw_hdmi_qp *hdmi, struct drm_crtc *crtc,
				    struct drm_vblank_crtc *vblank, u8 vsync_num, u32 timeout)
{
	int ret, i;
	u64 last;

	ret = drm_crtc_vblank_get(crtc);
	if (ret) {
		DRM_DEV_ERROR(hdmi->dev, "failed to get vblank\n");
		return ret;
	}

	for (i = 0; i < vsync_num; i++) {
		last = drm_crtc_vblank_count(crtc);
		ret = wait_event_timeout(vblank->queue, last != drm_crtc_vblank_count(crtc),
					 msecs_to_jiffies(timeout));
		if (!ret) {
			DRM_DEV_ERROR(hdmi->dev, "vblank wait timed out\n");
			drm_crtc_vblank_put(crtc);

			return ret;
		}
	}

	drm_crtc_vblank_put(crtc);

	return 0;
}

static int rockchip_hdmi_qms_vrr_enable(struct rockchip_dw_hdmi_qp *hdmi, struct drm_crtc *crtc,
					struct drm_vblank_crtc *vblank)
{
	u32 timeout = DIV_ROUND_CLOSEST_ULL(1000, drm_mode_vrefresh(&crtc->state->adjusted_mode));
	int ret;
	enum TARGET_FRAME_RATE brr_tfr;

	/* init brr */
	brr_tfr = rockchip_hdmi_get_next_tfr_val(&crtc->state->adjusted_mode);
	hdmi->brr_tfr = brr_tfr;

	dw_hdmi_qp_set_qms(hdmi->hdmi_qp, brr_tfr, 1);
	ret = rockchip_hdmi_wait_vsync(hdmi, crtc, vblank, 2, timeout);
	if (ret)
		return ret;

	dw_hdmi_qp_set_qms(hdmi->hdmi_qp, brr_tfr, 0);
	ret = rockchip_hdmi_wait_vsync(hdmi, crtc, vblank, 1, timeout);
	if (ret)
		return ret;

	hdmi->vrr_state = VRR_WAIT_REFRESH_CHANGE;

	return 0;
}

static void rockchip_hdmi_qms_vrr_set_refresh(struct rockchip_dw_hdmi_qp *hdmi,
					      struct rockchip_crtc_state *vcstate)
{
	if (!vcstate) {
		DRM_DEV_ERROR(hdmi->dev, "failed to get vcstate\n");
		return;
	}

	dw_hdmi_qp_set_qms(hdmi->hdmi_qp, hdmi->next_tfr_val, 0);
	vcstate->hdmi_vrr.refresh_rate_ready_to_change = true;
	vcstate->hdmi_vrr.next_tfr_val = hdmi->next_tfr_val;

	hdmi->vrr_state = VRR_IS_STABLE;
}

static int rockchip_hdmi_qms_vrr_rate_change(struct rockchip_dw_hdmi_qp *hdmi,
					     struct drm_crtc *crtc, struct drm_vblank_crtc *vblank)
{
	u8 current_tfr = dw_hdmi_qp_get_next_tfr(hdmi->hdmi_qp);
	u32 next_tfr_refresh_val = rockchip_hdmi_get_next_tfr_refresh_val(current_tfr);
	u32 timeout = DIV_ROUND_CLOSEST_ULL(100000, next_tfr_refresh_val);
	int ret;

	ret = rockchip_hdmi_wait_vsync(hdmi, crtc, vblank, 1, timeout);
	if (ret)
		return ret;

	dw_hdmi_qp_set_qms(hdmi->hdmi_qp, current_tfr, 0);

	ret = rockchip_hdmi_wait_vsync(hdmi, crtc, vblank, 1, timeout);
	if (ret)
		return ret;

	dw_hdmi_qp_set_qms(hdmi->hdmi_qp, hdmi->next_tfr_val, 0);
	hdmi->vrr_state = VRR_WAIT_REFRESH_CHANGE;

	return 0;
}

static int rockchip_hdmi_qms_vrr_disable(struct rockchip_dw_hdmi_qp *hdmi, struct drm_crtc *crtc,
					 struct drm_vblank_crtc *vblank)
{
	u8 current_tfr = dw_hdmi_qp_get_next_tfr(hdmi->hdmi_qp);
	u32 next_tfr_refresh_val = rockchip_hdmi_get_next_tfr_refresh_val(current_tfr);
	u32 timeout = DIV_ROUND_CLOSEST_ULL(100000, next_tfr_refresh_val);
	int ret;

	dw_hdmi_qp_set_qms(hdmi->hdmi_qp, hdmi->brr_tfr, 0);
	hdmi->next_tfr_val = hdmi->brr_tfr;

	hdmi->vrr_state = VRR_WAIT_REFRESH_CHANGE;

	ret = rockchip_hdmi_wait_vsync(hdmi, crtc, vblank, 3, timeout);
	if (ret)
		return ret;

	dw_hdmi_qp_set_qms(hdmi->hdmi_qp, hdmi->brr_tfr, 1);

	ret = rockchip_hdmi_wait_vsync(hdmi, crtc, vblank, 1, timeout);
	if (ret)
		return ret;

	dw_hdmi_qp_set_qms(hdmi->hdmi_qp, 0, 0);

	hdmi->vrr_state = VRR_IS_DISABLED;
	hdmi->next_tfr_val = 0;

	return 0;
}

static void dw_hdmi_wait_vblank(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi;
	struct drm_encoder *encoder;
	struct drm_crtc *crtc;
	struct drm_vblank_crtc *vblank;
	int pipe = 0;
	u32 timeout;

	if (!data)
		return;

	hdmi = (struct rockchip_dw_hdmi_qp *)data;
	encoder = &hdmi->encoder;

	if (!encoder || !encoder->crtc)
		return;

	crtc = encoder->crtc;
	pipe = drm_crtc_index(crtc);
	vblank = &crtc->dev->vblank[pipe];
	timeout = DIV_ROUND_CLOSEST_ULL(1200, drm_mode_vrefresh(&crtc->state->adjusted_mode));

	rockchip_hdmi_wait_vsync(hdmi, crtc, vblank, 1, timeout);
}

static void dw_hdmi_qms_vrr_work(struct work_struct *p_work)
{
	struct rockchip_dw_hdmi_qp *hdmi =
		container_of(p_work, struct rockchip_dw_hdmi_qp, qms_vrr_work);
	struct drm_encoder *encoder = &hdmi->encoder;
	struct drm_crtc *crtc;
	struct drm_vblank_crtc *vblank;
	int pipe = 0;

	if (!encoder || !encoder->crtc) {
		DRM_DEV_ERROR(hdmi->dev, "can't get crtc\n");
		return;
	}

	crtc = encoder->crtc;
	pipe = drm_crtc_index(crtc);
	vblank = &crtc->dev->vblank[pipe];

	if (hdmi->vrr_state == VRR_GOTO_ENABLE)
		rockchip_hdmi_qms_vrr_enable(hdmi, crtc, vblank);
	else if (hdmi->vrr_state == VRR_GOTO_DISABLE)
		rockchip_hdmi_qms_vrr_disable(hdmi, crtc, vblank);
	else
		rockchip_hdmi_qms_vrr_rate_change(hdmi, crtc, vblank);
}

static void init_hdmi_work(struct rockchip_dw_hdmi_qp *hdmi)
{
	hdmi->workqueue = create_workqueue("hdmi_queue");
	INIT_DELAYED_WORK(&hdmi->hpd_work, repo_hpd_event);
	INIT_WORK(&hdmi->qms_vrr_work, dw_hdmi_qms_vrr_work);
}

static int rockchip_hdmi_parse_dt(struct rockchip_dw_hdmi_qp *hdmi)
{
	int ret = 0;
	struct device_node *np = hdmi->dev->of_node;
	struct display_timing timing;
	struct videomode vm;
	struct clk *hdmi_clk;

	hdmi->regmap = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(hdmi->regmap)) {
		DRM_DEV_ERROR(hdmi->dev, "Unable to get rockchip,grf\n");
		return PTR_ERR(hdmi->regmap);
	}

	hdmi->vo0_regmap = syscon_regmap_lookup_by_phandle_optional(np, "rockchip,vo0_grf");
	if (!hdmi->vo0_regmap)
		dev_dbg(hdmi->dev, "vo0 grf is null\n");
	hdmi->vo1_regmap = syscon_regmap_lookup_by_phandle_optional(np, "rockchip,vo1_grf");
	if (!hdmi->vo1_regmap)
		dev_dbg(hdmi->dev, "vo1 grf is null\n");

	hdmi_clk = devm_clk_get_optional_enabled(hdmi->dev, "aud");
	if (IS_ERR(hdmi_clk)) {
		dev_err_probe(hdmi->dev, PTR_ERR(hdmi_clk), "failed to get aud_clk clock\n");
		return PTR_ERR(hdmi_clk);
	}

	hdmi_clk = devm_clk_get_optional_enabled(hdmi->dev, "hpd");
	if (IS_ERR(hdmi_clk)) {
		dev_err_probe(hdmi->dev, PTR_ERR(hdmi_clk), "failed to get hpd_clk clock\n");
		return PTR_ERR(hdmi_clk);
	}

	hdmi_clk = devm_clk_get_optional_enabled(hdmi->dev, "hclk_vo1");
	if (IS_ERR(hdmi_clk)) {
		dev_err_probe(hdmi->dev, PTR_ERR(hdmi_clk), "failed to get hclk_vo1 clock\n");
		return PTR_ERR(hdmi_clk);
	}

	hdmi_clk = devm_clk_get_optional_enabled(hdmi->dev, "earc");
	if (IS_ERR(hdmi_clk)) {
		dev_err_probe(hdmi->dev, PTR_ERR(hdmi_clk), "failed to get earc clock\n");
		return PTR_ERR(hdmi_clk);
	}

	hdmi_clk = devm_clk_get_optional_enabled(hdmi->dev, "pclk");
	if (IS_ERR(hdmi_clk)) {
		dev_err_probe(hdmi->dev, PTR_ERR(hdmi_clk), "failed to get pclk clock\n");
		return PTR_ERR(hdmi_clk);
	}

	if (hdmi->cec_wakeup_supported) {
		hdmi_clk = devm_clk_get_enabled(hdmi->dev, "cec");
		if (IS_ERR(hdmi_clk)) {
			dev_err_probe(hdmi->dev, PTR_ERR(hdmi_clk), "failed to get cec clock\n");
			return PTR_ERR(hdmi_clk);
		}

		hdmi_clk = devm_clk_get_enabled(hdmi->dev, "cec_wakeup");
		if (IS_ERR(hdmi_clk)) {
			dev_err_probe(hdmi->dev, PTR_ERR(hdmi_clk),
				"failed to get cec_wakeup clock\n");
			return PTR_ERR(hdmi_clk);
		}
	}

	hdmi->hdmitx_ref = devm_clk_get_optional_enabled(hdmi->dev, "hdmitx_ref");
	if (IS_ERR(hdmi->hdmitx_ref)) {
		dev_err_probe(hdmi->dev, PTR_ERR(hdmi->hdmitx_ref),
			      "failed to get hdmitx_ref clock\n");
		return PTR_ERR(hdmi->hdmitx_ref);
	}

	hdmi->link_clk = devm_clk_get_optional(hdmi->dev, "link_clk");
	if (IS_ERR(hdmi->link_clk)) {
		dev_err_probe(hdmi->dev, PTR_ERR(hdmi->link_clk),
			      "failed to get link_clk clock\n");
		return PTR_ERR(hdmi->link_clk);
	}

	hdmi->enable_gpio = devm_gpiod_get_optional(hdmi->dev, "enable", GPIOD_ASIS);
	if (IS_ERR(hdmi->enable_gpio)) {
		ret = PTR_ERR(hdmi->enable_gpio);
		dev_err(hdmi->dev, "failed to request enable GPIO: %d\n", ret);
		return ret;
	}

	hdmi->skip_check_420_mode = of_property_read_bool(np, "skip-check-420-mode");

	hdmi->force_disable_dsc = of_property_read_bool(np, "force-disable-dsc");

	if (of_property_read_bool(np, "force-output")) {
		hdmi->timing_force_output = true;

		ret = of_get_display_timing(np, "force_timing", &timing);
		if (ret < 0) {
			dev_err(hdmi->dev, "can't get force timing\n");
			hdmi->timing_force_output = false;
			return ret;
		}

		videomode_from_timing(&timing, &vm);
		drm_display_mode_from_videomode(&vm, &hdmi->force_mode);
		hdmi->force_mode.type |= DRM_MODE_TYPE_PREFERRED;

		if (of_property_read_u32(np, "force-bus-format", &hdmi->force_bus_format))
			hdmi->force_bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	}

	if (of_property_read_u32(np, "rockchip,sda-falling-delay-ns", &hdmi->sda_falling_delay_ns))
		hdmi->sda_falling_delay_ns = 0;

	return ret;
}

static bool is_hdmi2_mode(const struct drm_display_mode *mode)
{
	if (mode->clock > 340000 && mode->clock <= 600000)
		return true;

	return false;
}

static enum drm_mode_status
dw_hdmi_rockchip_mode_valid(struct dw_hdmi *dw_hdmi, void *data,
			    const struct drm_display_info *info,
			    const struct drm_display_mode *mode)
{
	struct drm_connector *connector = container_of(info, struct drm_connector, display_info);
	struct drm_encoder *encoder = connector->encoder;
	struct drm_crtc *crtc;
	struct rockchip_dw_hdmi_qp *hdmi;
	struct rockchip_crtc_state *s;

	if (!encoder) {
		const struct drm_connector_helper_funcs *funcs;

		funcs = connector->helper_private;
		if (funcs->atomic_best_encoder)
			encoder = funcs->atomic_best_encoder(connector, connector->state->state);
		else
			encoder = funcs->best_encoder(connector);
	}

	if (!encoder || !encoder->possible_crtcs)
		return MODE_BAD;

	hdmi = to_rockchip_hdmi(encoder);

	if (!hdmi->skip_check_420_mode) {
		u32 max_tmds_clock = connector->display_info.max_tmds_clock;

		/* some sinks edid max_tmds_clocks are 0, we think it only support hdmi1.4 */
		if (!connector->display_info.max_tmds_clock)
			max_tmds_clock = 340000;

		/* edid isn't support yuv420 and max_tmds_clock is less than mode pixel clk */
		if (mode->clock < 600000 && max_tmds_clock < mode->clock &&
		    (!drm_mode_is_420(&connector->display_info, mode) ||
		     !connector->ycbcr_420_allowed))
			return MODE_BAD;

		/* edid isn't support yuv420 and hdmitx only support hdmi1.4 clk */
		if (hdmi->max_tmdsclk <= 340000 && is_hdmi2_mode(mode) &&
		    !drm_mode_is_420(&connector->display_info, mode))
			return MODE_BAD;

		/*
		 * hdmi cts hf1-31 required filtering yuv420 mode that frequency
		 * exceeds the max_tmds_clock of edid.
		 */
		if (drm_mode_is_420(&connector->display_info, mode) &&
		    max_tmds_clock < (mode->clock / 2) && is_hdmi2_mode(mode))
			return MODE_BAD;
	};

	if (encoder->crtc) {
		s = to_rockchip_crtc_state(encoder->crtc->state);
		s->output_type = DRM_MODE_CONNECTOR_HDMIA;
	} else {
		drm_for_each_crtc(crtc, connector->dev) {
			if (!drm_encoder_crtc_ok(encoder, crtc))
				continue;

			s = to_rockchip_crtc_state(crtc->state);
			s->output_type = DRM_MODE_CONNECTOR_HDMIA;
		}
	}

	if ((!hdmi->enable_gpio || hdmi->plat_data->dw_hdmi_qp_version == DW_HDMI_QP_V2) &&
	    mode->clock > 600000)
		return MODE_BAD;

	return MODE_OK;
}

static void dw_hdmi_rockchip_encoder_atomic_disable(struct drm_encoder *encoder,
						    struct drm_atomic_state *state)
{
	struct rockchip_dw_hdmi_qp *hdmi = to_rockchip_hdmi(encoder);
	struct drm_crtc *old_crtc, *new_crtc;
	struct rockchip_crtc_state *s;

	old_crtc = drm_atomic_get_old_crtc_for_encoder(state, encoder);
	new_crtc = drm_atomic_get_new_crtc_for_encoder(state, encoder);

	if (old_crtc && old_crtc != new_crtc) {
		s = to_rockchip_crtc_state(old_crtc->state);

		if (hdmi->plat_data->split_mode) {
			s->output_if &= ~(VOP_OUTPUT_IF_HDMI0 | VOP_OUTPUT_IF_HDMI1);
		} else {
			if (!hdmi->id)
				s->output_if &= ~VOP_OUTPUT_IF_HDMI0;
			else
				s->output_if &= ~VOP_OUTPUT_IF_HDMI1;
		}
		s->output_if_left_panel &= ~(hdmi->id ? VOP_OUTPUT_IF_HDMI1 : VOP_OUTPUT_IF_HDMI0);
	}

	hdmi->next_tfr_val = 0;
	hdmi->vrr_state = VRR_IS_DISABLED;

	/*
	 * when plug out hdmi it will be switch cvbs and then phy bus width
	 * must be set as 8
	 */
	if (hdmi->phy)
		phy_set_bus_width(hdmi->phy, 8);
}

static void dw_hdmi_rockchip_encoder_enable(struct drm_encoder *encoder)
{
	struct rockchip_dw_hdmi_qp *hdmi = to_rockchip_hdmi(encoder);
	struct drm_crtc *crtc = encoder->crtc;

	if (!crtc || !crtc->state) {
		dev_info(hdmi->dev, "%s old crtc state is null\n", __func__);
		return;
	}

	if (hdmi->phy)
		phy_set_bus_width(hdmi->phy, hdmi->phy_bus_width);

	if (hdmi->link_cfg.frl_mode)
		gpiod_direction_output(hdmi->enable_gpio, 0);
	else
		gpiod_direction_output(hdmi->enable_gpio, 1);
}

static int _dw_hdmi_rockchip_encoder_loader_protect(struct rockchip_dw_hdmi_qp *hdmi, bool on)
{
	int ret;

	if (on) {
		ret = clk_prepare_enable(hdmi->link_clk);
		if (ret < 0) {
			DRM_DEV_ERROR(hdmi->dev, "failed to enable link_clk %d\n", ret);
			return ret;
		}
	} else {
		clk_disable_unprepare(hdmi->link_clk);
	}

	return 0;
}

static int dw_hdmi_rockchip_encoder_loader_protect(struct rockchip_drm_sub_dev *sub_dev, bool on)
{
	struct rockchip_dw_hdmi_qp *hdmi =
		container_of(sub_dev, struct rockchip_dw_hdmi_qp, sub_dev);
	struct rockchip_dw_hdmi_qp *secondary;

	_dw_hdmi_rockchip_encoder_loader_protect(hdmi, on);
	if (hdmi->plat_data->right) {
		secondary = rockchip_hdmi_find_by_id(hdmi->dev->driver, !hdmi->id);
		_dw_hdmi_rockchip_encoder_loader_protect(secondary, on);
	}

	return 0;
}

static void rk3572_set_link_mode(struct rockchip_dw_hdmi_qp *hdmi)
{
	int val;

	if (!hdmi->vo0_regmap)
		return;

	if (!hdmi->link_cfg.frl_mode) {
		val = HIWORD_UPDATE(0, RK3576_HDMITX_FRL_MOD);
		regmap_write(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON0, val);

		return;
	}

	val = HIWORD_UPDATE(RK3576_HDMITX_FRL_MOD, RK3576_HDMITX_FRL_MOD);
	regmap_write(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON0, val);
}

static void rk3576_set_link_mode(struct rockchip_dw_hdmi_qp *hdmi)
{
	int val;

	if (!hdmi->vo0_regmap)
		return;

	if (!hdmi->link_cfg.frl_mode) {
		val = HIWORD_UPDATE(0, RK3576_HDMITX_FRL_MOD);
		regmap_write(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON1, val);

		return;
	}

	val = HIWORD_UPDATE(RK3576_HDMITX_FRL_MOD, RK3576_HDMITX_FRL_MOD);
	regmap_write(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON1, val);
}

static void rk3588_set_link_mode(struct rockchip_dw_hdmi_qp *hdmi)
{
	int val;
	bool is_hdmi0;

	if (!hdmi->vo1_regmap)
		return;

	if (!hdmi->id)
		is_hdmi0 = true;
	else
		is_hdmi0 = false;

	if (!hdmi->link_cfg.frl_mode) {
		val = HIWORD_UPDATE(0, RK3588_HDMI21_MASK);
		if (is_hdmi0)
			regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON4, val);
		else
			regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON7, val);

		val = HIWORD_UPDATE(0, RK3588_COMPRESS_MODE_MASK | RK3588_COLOR_FORMAT_MASK);
		if (is_hdmi0)
			regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON3, val);
		else
			regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON6, val);

		return;
	}

	val = HIWORD_UPDATE(RK3588_HDMI21_MASK, RK3588_HDMI21_MASK);
	if (is_hdmi0)
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON4, val);
	else
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON7, val);

	if (hdmi->link_cfg.dsc_mode) {
		val = HIWORD_UPDATE(RK3588_COMPRESS_MODE_MASK | RK3588_COMPRESSED_DATA,
				    RK3588_COMPRESS_MODE_MASK | RK3588_COLOR_FORMAT_MASK);
		if (is_hdmi0)
			regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON3, val);
		else
			regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON6, val);
	} else {
		val = HIWORD_UPDATE(0, RK3588_COMPRESS_MODE_MASK | RK3588_COLOR_FORMAT_MASK);
		if (is_hdmi0)
			regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON3, val);
		else
			regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON6, val);
	}
}

static void rk3538_set_color_format(struct rockchip_dw_hdmi_qp *hdmi, u64 bus_format, u32 depth)
{
	u32 val = 0;

	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB101010_1X30:
		val = HIWORD_UPDATE(0, RK3538_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
		val = HIWORD_UPDATE(RK3576_YUV420, RK3538_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_YUV10_1X30:
		val = HIWORD_UPDATE(RK3576_YUV444, RK3538_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_YUYV10_1X20:
	case MEDIA_BUS_FMT_YUYV8_1X16:
		val = HIWORD_UPDATE(RK3576_YUV422, RK3538_COLOR_FORMAT_MASK);
		break;
	default:
		dev_err(hdmi->dev, "can't set correct color format\n");
		return;
	}

	if (depth == 8 || bus_format == MEDIA_BUS_FMT_YUYV10_1X20)
		val |= HIWORD_UPDATE(RK3538_8BPC, RK3538_COLOR_DEPTH_MASK);
	else
		val |= HIWORD_UPDATE(RK3538_10BPC, RK3538_COLOR_DEPTH_MASK);

	regmap_write(hdmi->vo0_regmap, RK3538_VO_GRF_HDMI_MISC, val);
}

static void rk3572_set_color_format(struct rockchip_dw_hdmi_qp *hdmi, u64 bus_format,
				    u32 depth)
{
	u32 val = 0;

	if (!hdmi->vo0_regmap)
		return;

	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB101010_1X30:
		val = HIWORD_UPDATE(0, RK3576_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
		val = HIWORD_UPDATE(RK3576_YUV420, RK3576_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_YUV10_1X30:
		val = HIWORD_UPDATE(RK3576_YUV444, RK3576_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_YUYV10_1X20:
	case MEDIA_BUS_FMT_YUYV8_1X16:
		val = HIWORD_UPDATE(RK3576_YUV422, RK3576_COLOR_FORMAT_MASK);
		break;
	default:
		dev_err(hdmi->dev, "can't set correct color format\n");
		return;
	}

	if (depth == 8 || bus_format == MEDIA_BUS_FMT_YUYV10_1X20)
		val |= HIWORD_UPDATE(RK3576_8BPC, RK3576_COLOR_DEPTH_MASK);
	else
		val |= HIWORD_UPDATE(RK3576_10BPC, RK3576_COLOR_DEPTH_MASK);

	regmap_write(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON13, val);
}

static void rk3576_set_color_format(struct rockchip_dw_hdmi_qp *hdmi, u64 bus_format,
				    u32 depth)
{
	u32 val = 0;

	if (!hdmi->vo0_regmap)
		return;

	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB101010_1X30:
		val = HIWORD_UPDATE(0, RK3576_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
		val = HIWORD_UPDATE(RK3576_YUV420, RK3576_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_YUV10_1X30:
		val = HIWORD_UPDATE(RK3576_YUV444, RK3576_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_YUYV10_1X20:
	case MEDIA_BUS_FMT_YUYV8_1X16:
		val = HIWORD_UPDATE(RK3576_YUV422, RK3576_COLOR_FORMAT_MASK);
		break;
	default:
		dev_err(hdmi->dev, "can't set correct color format\n");
		return;
	}

	if (depth == 8 || bus_format == MEDIA_BUS_FMT_YUYV10_1X20)
		val |= HIWORD_UPDATE(RK3576_8BPC, RK3576_COLOR_DEPTH_MASK);
	else
		val |= HIWORD_UPDATE(RK3576_10BPC, RK3576_COLOR_DEPTH_MASK);

	regmap_write(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON8, val);
}

static void rk3588_set_color_format(struct rockchip_dw_hdmi_qp *hdmi, u64 bus_format, u32 depth)
{
	u32 val = 0;

	if (!hdmi->vo1_regmap)
		return;

	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_RGB101010_1X30:
		val = HIWORD_UPDATE(0, RK3588_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
		val = HIWORD_UPDATE(RK3588_YUV420, RK3588_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_YUV8_1X24:
	case MEDIA_BUS_FMT_YUV10_1X30:
		val = HIWORD_UPDATE(RK3588_YUV444, RK3588_COLOR_FORMAT_MASK);
		break;
	case MEDIA_BUS_FMT_YUYV10_1X20:
	case MEDIA_BUS_FMT_YUYV8_1X16:
		val = HIWORD_UPDATE(RK3588_YUV422, RK3588_COLOR_FORMAT_MASK);
		break;
	default:
		dev_err(hdmi->dev, "can't set correct color format\n");
		return;
	}

	if (hdmi->link_cfg.dsc_mode)
		val = HIWORD_UPDATE(RK3588_COMPRESSED_DATA, RK3588_COLOR_FORMAT_MASK);

	if (depth == 8 || bus_format == MEDIA_BUS_FMT_YUYV10_1X20)
		val |= HIWORD_UPDATE(RK3588_8BPC, RK3588_COLOR_DEPTH_MASK);
	else
		val |= HIWORD_UPDATE(RK3588_10BPC, RK3588_COLOR_DEPTH_MASK);

	if (!hdmi->id)
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON3, val);
	else
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON6, val);
}

static void rockchip_set_hdcp_status(void *data, u8 status)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	hdmi->hdcp_status = status;
}

static void rk3538_set_hdcp2_enable(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (!hdmi->vo0_regmap)
		return;

	if (enable)
		val = HIWORD_UPDATE(HDCP0_P1_GPIO_IN, HDCP0_P1_GPIO_IN);
	else
		val = HIWORD_UPDATE(0, HDCP0_P1_GPIO_IN);

	regmap_write(hdmi->vo0_regmap, RK3538_VO_GRF_HDMI_SWITCH, val);
}

static void rk3572_set_hdcp2_enable(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (!hdmi->vo0_regmap)
		return;

	if (enable)
		val = HIWORD_UPDATE(HDCP0_P1_GPIO_IN, HDCP0_P1_GPIO_IN);
	else
		val = HIWORD_UPDATE(0, HDCP0_P1_GPIO_IN);

	regmap_write(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON12, val);
}

static void rk3576_set_hdcp2_enable(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (!hdmi->vo0_regmap)
		return;

	if (enable)
		val = HIWORD_UPDATE(HDCP0_P1_GPIO_IN, HDCP0_P1_GPIO_IN);
	else
		val = HIWORD_UPDATE(0, HDCP0_P1_GPIO_IN);

	regmap_write(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON14, val);
}

static void rk3572_set_emp_bypass_enable(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (enable)
		val = HIWORD_UPDATE(RK3576_HDMI_EMP_MEM_LEN_EN | RK3576_HDMI_EMP_MEM_LEN_BYPASS,
				    RK3576_HDMI_EMP_MEM_LEN_EN | RK3576_HDMI_EMP_MEM_LEN_BYPASS);
	else
		val = HIWORD_UPDATE(0, RK3576_HDMI_EMP_MEM_LEN_EN |
				    RK3576_HDMI_EMP_MEM_LEN_BYPASS);

	regmap_write(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON0, val);
}

static void rk3576_set_emp_bypass_enable(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (enable)
		val = HIWORD_UPDATE(RK3576_HDMI_EMP_MEM_LEN_EN | RK3576_HDMI_EMP_MEM_LEN_BYPASS,
				    RK3576_HDMI_EMP_MEM_LEN_EN | RK3576_HDMI_EMP_MEM_LEN_BYPASS);
	else
		val = HIWORD_UPDATE(0, RK3576_HDMI_EMP_MEM_LEN_EN |
				    RK3576_HDMI_EMP_MEM_LEN_BYPASS);

	regmap_write(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON1, val);
}

static void rk3588_set_hdcp2_enable(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (!hdmi->vo1_regmap)
		return;

	if (enable)
		val = HIWORD_UPDATE(HDCP1_P1_GPIO_IN, HDCP1_P1_GPIO_IN);
	else
		val = HIWORD_UPDATE(0, HDCP1_P1_GPIO_IN);

	regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON1, val);
}

static void rockchip_set_hdcp2_enable(void *data, bool enable)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	hdmi->chip_data->ops->set_hdcp2_enable(hdmi, enable);
}

static void rockchip_set_grf_cfg(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	int color_depth;

	if (hdmi->chip_data->ops->set_link_mode)
		hdmi->chip_data->ops->set_link_mode(hdmi);

	color_depth = rockchip_drm_bus_fmt_color_depth(hdmi->bus_format);

	hdmi->chip_data->ops->set_color_format(hdmi, hdmi->bus_format, color_depth);
}

static void rk3538_get_grf_color_fmt(struct rockchip_dw_hdmi_qp *hdmi, u32 *fmt, u32 *depth)
{
	if (!hdmi->vo0_regmap)
		return;

	regmap_read(hdmi->vo0_regmap, RK3538_VO_GRF_HDMI_MISC, fmt);

	*depth = *fmt & RK3538_COLOR_DEPTH_MASK;
	*fmt = (*fmt & RK3538_COLOR_FORMAT_MASK) >> 4;
}

static void rk3572_get_grf_color_fmt(struct rockchip_dw_hdmi_qp *hdmi, u32 *fmt, u32 *depth)
{
	if (!hdmi->vo0_regmap)
		return;

	regmap_read(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON13, fmt);

	*depth = (*fmt & RK3576_COLOR_DEPTH_MASK) >> 8;
	*fmt = (*fmt & RK3576_COLOR_FORMAT_MASK) >> 4;
}

static void rk3576_get_grf_color_fmt(struct rockchip_dw_hdmi_qp *hdmi, u32 *fmt, u32 *depth)
{
	if (!hdmi->vo0_regmap)
		return;

	regmap_read(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON8, fmt);

	*depth = (*fmt & RK3576_COLOR_DEPTH_MASK) >> 8;
	*fmt = (*fmt & RK3576_COLOR_FORMAT_MASK) >> 4;
}

static void rk3588_get_grf_color_fmt(struct rockchip_dw_hdmi_qp *hdmi, u32 *fmt, u32 *depth)
{
	if (!hdmi->vo1_regmap)
		return;

	if (!hdmi->id)
		regmap_read(hdmi->vo1_regmap, RK3588_GRF_VO1_CON3, fmt);
	else
		regmap_read(hdmi->vo1_regmap, RK3588_GRF_VO1_CON6, fmt);

	*depth = (*fmt & RK3588_COLOR_DEPTH_MASK) >> 4;
	*fmt = *fmt & RK3588_COLOR_FORMAT_MASK;
}

static unsigned long
rockchip_hdmi_colorspace_to_color_encoding(u32 colorimetry, u32 edid_colorimetry, u8 vic)
{
	if (colorimetry && !(BIT(colorimetry) & edid_colorimetry)) {
		DRM_ERROR("colorimetry %d is not supported in edid\n", colorimetry);
		return DRM_COLOR_YCBCR_BT601;
	}

	switch (colorimetry) {
	case DRM_MODE_COLORIMETRY_BT2020_RGB:
	case DRM_MODE_COLORIMETRY_BT2020_YCC:
		return DRM_COLOR_YCBCR_BT2020;
	case DRM_MODE_COLORIMETRY_SMPTE_170M_YCC:
		return DRM_COLOR_YCBCR_BT601;
	case DRM_MODE_COLORIMETRY_BT709_YCC:
		return DRM_COLOR_YCBCR_BT709;
	case DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65:
		return DRM_COLOR_DCI_P3;
	/*
	 * according to cea spec, sd resolution is set to output
	 * in BT601 format by default. hd and higher resolutions
	 * output bt709 by default
	 */
	case DRM_MODE_COLORIMETRY_DEFAULT:
		if ((vic == 6) || (vic == 7) || (vic == 21) || (vic == 22) ||
		    (vic == 2) || (vic == 3) || (vic == 17) || (vic == 18)) {
			return DRM_COLOR_YCBCR_BT601;
		}
		return DRM_COLOR_YCBCR_BT709;
	default:
		DRM_ERROR("colorimetry %d is out of range\n", colorimetry);
		break;
	}

	return DRM_COLOR_YCBCR_BT709;
}

static u32
rockchip_hdmi_color_encoding_to_colorspace(unsigned long color_encoding)
{
	switch (color_encoding) {
	case DRM_COLOR_YCBCR_BT2020:
		return DRM_MODE_COLORIMETRY_BT2020_RGB;
	case DRM_COLOR_YCBCR_BT601:
		return DRM_MODE_COLORIMETRY_SMPTE_170M_YCC;
	case DRM_COLOR_YCBCR_BT709:
		return DRM_MODE_COLORIMETRY_BT709_YCC;
	case DRM_COLOR_DCI_P3:
		return DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65;
	default:
		DRM_ERROR("color_encoding %lx is out of range\n", color_encoding);
		break;
	}

	return DRM_MODE_COLORIMETRY_BT709_YCC;
}

static void
dw_hdmi_rockchip_select_output(struct drm_connector_state *conn_state,
			       struct drm_crtc_state *crtc_state,
			       struct rockchip_dw_hdmi_qp *hdmi,
			       unsigned int *color_format,
			       unsigned int *output_mode,
			       unsigned long *bus_format,
			       unsigned int *bus_width,
			       unsigned long *enc_out_encoding,
			       unsigned int *eotf)
{
	struct drm_display_info *info = &conn_state->connector->display_info;
	struct drm_display_mode mode = {};
	struct hdr_output_metadata *hdr_metadata;
	u32 vic;
	unsigned long tmdsclock, pixclock;
	unsigned int color_depth;
	bool support_dc = false;
	bool sink_is_hdmi = true;
	bool yuv422_out = false;
	bool dsc_rate_supported;
	bool hdr_no_bt2020 = false;
	u32 max_tmds_clock = info->max_tmds_clock;
	int output_eotf;

	drm_mode_copy(&mode, &crtc_state->mode);
	if (hdmi->plat_data->split_mode)
		drm_mode_convert_to_origin_mode(&mode);
	pixclock = mode.crtc_clock;

	vic = drm_match_cea_mode(&mode);

	sink_is_hdmi = dw_hdmi_qp_get_output_whether_hdmi(hdmi->hdmi_qp);

	*color_format = RK_IF_FORMAT_RGB;

	switch (hdmi->hdmi_output) {
	case RK_IF_FORMAT_YCBCR_HQ:
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*color_format = RK_IF_FORMAT_YCBCR444;
		else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = RK_IF_FORMAT_YCBCR422;
		else if (conn_state->connector->ycbcr_420_allowed &&
			 drm_mode_is_420(info, &mode) && (pixclock > HDMI14_MAX_RATE))
			*color_format = RK_IF_FORMAT_YCBCR420;
		break;
	case RK_IF_FORMAT_YCBCR_LQ:
		if (conn_state->connector->ycbcr_420_allowed &&
		    drm_mode_is_420(info, &mode) && pixclock > HDMI14_MAX_RATE)
			*color_format = RK_IF_FORMAT_YCBCR420;
		else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = RK_IF_FORMAT_YCBCR422;
		else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*color_format = RK_IF_FORMAT_YCBCR444;
		break;
	case RK_IF_FORMAT_YCBCR420:
		if (conn_state->connector->ycbcr_420_allowed &&
		    drm_mode_is_420(info, &mode) && pixclock > HDMI14_MAX_RATE)
			*color_format = RK_IF_FORMAT_YCBCR420;
		break;
	case RK_IF_FORMAT_YCBCR422:
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = RK_IF_FORMAT_YCBCR422;
		break;
	case RK_IF_FORMAT_YCBCR444:
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*color_format = RK_IF_FORMAT_YCBCR444;
		break;
	case RK_IF_FORMAT_RGB:
	default:
		break;
	}

	if (*color_format == RK_IF_FORMAT_RGB &&
	    info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_30)
		support_dc = true;
	if (*color_format == RK_IF_FORMAT_YCBCR444 &&
	    info->edid_hdmi_ycbcr444_dc_modes & DRM_EDID_HDMI_DC_30)
		support_dc = true;
	if (*color_format == RK_IF_FORMAT_YCBCR422)
		support_dc = true;
	if (*color_format == RK_IF_FORMAT_YCBCR420 &&
	    info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_30)
		support_dc = true;

	if (hdmi->colordepth > 8 && support_dc)
		color_depth = 10;
	else
		color_depth = 8;

	*eotf = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
	if (conn_state->hdr_output_metadata) {
		hdr_metadata = (struct hdr_output_metadata *)conn_state->hdr_output_metadata->data;
		output_eotf = hdr_metadata->hdmi_metadata_type1.eotf;
		if (output_eotf > HDMI_EOTF_TRADITIONAL_GAMMA_SDR &&
		    output_eotf <= HDMI_EOTF_BT_2100_HLG)
			*eotf = output_eotf;
	}

	*enc_out_encoding = conn_state->colorspace;

	hdmi->colorimetry =
		rockchip_hdmi_colorspace_to_color_encoding(conn_state->colorspace,
							   hdmi->edid_colorimetry, vic);

	*enc_out_encoding = rockchip_hdmi_color_encoding_to_colorspace(hdmi->colorimetry);
	if ((conn_state->connector->hdr_sink_metadata.hdmi_type1.eotf & BIT(*eotf) &&
	     *eotf > HDMI_EOTF_TRADITIONAL_GAMMA_SDR) &&
	    (hdmi->colorimetry != DRM_COLOR_YCBCR_BT2020))
		hdr_no_bt2020 = true;

	/* bt2020 sdr/hdr output */
	if ((hdmi->colorimetry == DRM_COLOR_YCBCR_BT2020) || hdr_no_bt2020)
		yuv422_out = true;

	if ((yuv422_out || hdmi->hdmi_output == RK_IF_FORMAT_YCBCR_HQ) && color_depth == 10 &&
	    (rockchip_drm_bus_fmt_color_depth(hdmi->prev_bus_format) == 8 ||
	     rockchip_drm_bus_fmt_to_color_format(hdmi->prev_bus_format) ==
	     RK_IF_FORMAT_YCBCR422)) {
		/* We prefer use YCbCr422 to send hdr 10bit */
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = RK_IF_FORMAT_YCBCR422;
	}

	if (mode.flags & DRM_MODE_FLAG_DBLCLK)
		pixclock *= 2;
	if ((mode.flags & DRM_MODE_FLAG_3D_MASK) ==
		DRM_MODE_FLAG_3D_FRAME_PACKING)
		pixclock *= 2;

	tmdsclock = rockchip_drm_hdmi_get_tmdsclock(hdmi->output_bus_format, mode.clock * 1000);
	dsc_rate_supported =
		rockchip_hdmi_check_dsc_rate_supported(hdmi, tmdsclock, color_depth * 3);

	if (drm_mode_is_420_only(info, &mode) ||
	    (mode.clock > 1188000 &&
	     (*color_format == RK_IF_FORMAT_YCBCR422 || hdmi->force_disable_dsc ||
	      !dsc_rate_supported)))
		*color_format = RK_IF_FORMAT_YCBCR420;

	if (!sink_is_hdmi) {
		*color_format = RK_IF_FORMAT_RGB;
		color_depth = 8;
	}

	if (*color_format == RK_IF_FORMAT_YCBCR422 || color_depth == 8)
		tmdsclock = pixclock;
	else
		tmdsclock = pixclock * (color_depth) / 8;

	if (*color_format == RK_IF_FORMAT_YCBCR420)
		tmdsclock /= 2;

	/* XXX: max_tmds_clock of some sink is 0, we think it is 340MHz. */
	if (!max_tmds_clock)
		max_tmds_clock = 340000;

	max_tmds_clock = min(max_tmds_clock, hdmi->max_tmdsclk);

	if (hdmi->link_cfg.rate_per_lane && tmdsclock > 600000)
		max_tmds_clock = hdmi->link_cfg.frl_lanes * hdmi->link_cfg.rate_per_lane * 1000000;

	if (tmdsclock > max_tmds_clock) {
		if (max_tmds_clock >= 594000) {
			color_depth = 8;
		} else if (max_tmds_clock > 340000) {
			if (drm_mode_is_420(info, &mode) || tmdsclock >= 594000)
				*color_format = RK_IF_FORMAT_YCBCR420;
		} else {
			color_depth = 8;
			if (drm_mode_is_420(info, &mode) || tmdsclock >= 594000)
				*color_format = RK_IF_FORMAT_YCBCR420;
		}
	}

	if (hdmi->timing_force_output)
		rockchip_drm_parse_bus_format(hdmi->force_bus_format, color_format, &color_depth);

	if (*color_format == RK_IF_FORMAT_YCBCR420) {
		*output_mode = ROCKCHIP_OUT_MODE_YUV420;
		if (color_depth > 8)
			*bus_format = MEDIA_BUS_FMT_UYYVYY10_0_5X30;
		else
			*bus_format = MEDIA_BUS_FMT_UYYVYY8_0_5X24;
		*bus_width = color_depth / 2;
	} else {
		*output_mode = ROCKCHIP_OUT_MODE_AAAA;
		if (color_depth > 8) {
			if (*color_format != RK_IF_FORMAT_RGB)
				*bus_format = MEDIA_BUS_FMT_YUV10_1X30;
			else
				*bus_format = MEDIA_BUS_FMT_RGB101010_1X30;
		} else {
			if (*color_format != RK_IF_FORMAT_RGB)
				*bus_format = MEDIA_BUS_FMT_YUV8_1X24;
			else
				*bus_format = MEDIA_BUS_FMT_RGB888_1X24;
		}
		if (*color_format == RK_IF_FORMAT_YCBCR422)
			*bus_width = 8;
		else
			*bus_width = color_depth;
	}

	hdmi->bus_format = *bus_format;

	if (*color_format == RK_IF_FORMAT_YCBCR422) {
		if (color_depth == 12)
			hdmi->output_bus_format = MEDIA_BUS_FMT_YUYV12_1X24;
		else if (color_depth == 10)
			hdmi->output_bus_format = MEDIA_BUS_FMT_YUYV10_1X20;
		else
			hdmi->output_bus_format = MEDIA_BUS_FMT_YUYV8_1X16;

		*bus_format = hdmi->output_bus_format;
		hdmi->bus_format = *bus_format;
		*output_mode = ROCKCHIP_OUT_MODE_YUV422;
	} else {
		hdmi->output_bus_format = *bus_format;
	}
}

static bool
dw_hdmi_rockchip_check_color(struct drm_connector_state *conn_state,
			     struct rockchip_dw_hdmi_qp *hdmi)
{
	struct drm_crtc_state *crtc_state = conn_state->crtc->state;
	unsigned int colorformat;
	unsigned long bus_format;
	unsigned long output_bus_format = hdmi->output_bus_format;
	unsigned long enc_out_encoding = hdmi->enc_out_encoding;
	unsigned int eotf, bus_width;
	unsigned int output_mode;

	dw_hdmi_rockchip_select_output(conn_state, crtc_state, hdmi,
				       &colorformat,
				       &output_mode, &bus_format, &bus_width,
				       &hdmi->enc_out_encoding, &eotf);

	if (output_bus_format != hdmi->output_bus_format ||
	    enc_out_encoding != hdmi->enc_out_encoding)
		return true;
	else
		return false;
}

static int
dw_hdmi_get_lane_cfg_by_frl_rate(struct rockchip_dw_hdmi_qp *hdmi, u64 rate,
				 u8 *rate_per_lane, u8 *lanes)
{
	switch (rate) {
	case 48000000000:
		*rate_per_lane = 12;
		*lanes = 4;
		break;
	case 40000000000:
		*rate_per_lane = 10;
		*lanes = 4;
		break;
	case 32000000000:
		*rate_per_lane = 8;
		*lanes = 4;
		break;
	case 24000000000:
		*rate_per_lane = 6;
		*lanes = 4;
		break;
	case 18000000000:
		*rate_per_lane = 6;
		*lanes = 3;
		break;
	case 9000000000:
		*rate_per_lane = 3;
		*lanes = 3;
		break;
	default:
		dev_err(hdmi->dev, "%s frl rate is out of range, set to 40G\n", __func__);
		*rate_per_lane = 10;
		*lanes = 4;
		return -EINVAL;
	}

	return 0;
}

static void rockchip_hdmi_qms_vrr_state(struct rockchip_dw_hdmi_qp *hdmi,
					struct rockchip_crtc_state *vcstate)
{
	switch (hdmi->vrr_state) {
	case VRR_GOTO_ENABLE:
	case VRR_RATE_CHANGED:
		vcstate->hdmi_vrr.m_const = 0;
		vcstate->vrr_type = ROCKCHIP_VRR_VFP_MODE;
		queue_work(hdmi->workqueue, &hdmi->qms_vrr_work);
		break;
	case VRR_GOTO_DISABLE:
		vcstate->hdmi_vrr.m_const = 0;
		vcstate->vrr_type = 0;
		vcstate->hdmi_vrr.next_tfr_val = hdmi->brr_tfr;
		vcstate->hdmi_vrr.refresh_rate_ready_to_change = true;
		queue_work(hdmi->workqueue, &hdmi->qms_vrr_work);
		break;
	case VRR_WAIT_REFRESH_CHANGE:
		vcstate->hdmi_vrr.m_const = 0;
		rockchip_hdmi_qms_vrr_set_refresh(hdmi, vcstate);
		break;
	case VRR_IS_STABLE:
		vcstate->hdmi_vrr.m_const = 1;
		dw_hdmi_qp_set_qms(hdmi->hdmi_qp, hdmi->next_tfr_val, 1);
		break;
	default:
		break;
	}
}

static int dw_hdmi_rockchip_encoder_atomic_check(struct drm_encoder *encoder,
						 struct drm_crtc_state *crtc_state,
						 struct drm_connector_state *conn_state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct rockchip_dw_hdmi_qp *hdmi = to_rockchip_hdmi(encoder);
	unsigned int colorformat, bus_width, tmdsclk;
	struct drm_display_mode mode = {};
	unsigned int output_mode;
	unsigned long bus_format;
	int color_depth;
	u8 fva_factor = hdmi->fva_factor_m1_val + 1;
	bool secondary = false;
	enum phy_hdmi_mode phy_hdmi_mode;
	struct phy_configure_opts_hdmi phy_cfg = {0};

	/*
	 * There are two hdmi but only one encoder in split mode,
	 * so we need to check twice.
	 */
secondary:
	drm_mode_copy(&mode, &crtc_state->adjusted_mode);
	if (hdmi->plat_data->split_mode || hdmi->plat_data->dual_connector_split)
		drm_mode_convert_to_origin_mode(&mode);

	dw_hdmi_rockchip_select_output(conn_state, crtc_state, hdmi,
				       &colorformat,
				       &output_mode, &bus_format, &bus_width,
				       &hdmi->enc_out_encoding, &s->eotf);

	s->bus_format = bus_format;
	hdmi->phy_bus_width = bus_width;

	if (hdmi->vrr_cap.vrr_mode) {
		s->max_refresh_rate = HDMI_MAX_VRR_REFRESH_RATE;
		s->min_refresh_rate = HDMI_MIN_VRR_REFRESH_RATE;
	} else {
		s->max_refresh_rate = 0;
		s->min_refresh_rate = 0;
	}

	if (hdmi->enable_gaming_vrr) {
		s->vrr_type = ROCKCHIP_VRR_VFP_MODE;
		s->max_refresh_rate = hdmi->vrr_cap.gaming_vrr_rate_max;
		s->min_refresh_rate = hdmi->vrr_cap.gaming_vrr_rate_min;
		s->hdmi_vrr.refresh_rate_ready_to_change = true;
	}

	s->hdmi_vrr.fva_factor_m1_val = hdmi->fva_factor_m1_val;

	if (hdmi->vrr_state != hdmi->old_vrr_state) {
		hdmi->old_vrr_state = hdmi->vrr_state;
		rockchip_hdmi_qms_vrr_state(hdmi, s);
	}

	color_depth = rockchip_drm_bus_fmt_color_depth(bus_format);
	tmdsclk = rockchip_drm_hdmi_get_tmdsclock(hdmi->output_bus_format,
						  mode.crtc_clock * fva_factor);
	if (rockchip_drm_bus_fmt_is_yuv420(hdmi->output_bus_format))
		tmdsclk /= 2;
	if (mode.flags & DRM_MODE_FLAG_DBLCLK)
		tmdsclk *= 2;
	hdmi_select_link_config(hdmi, crtc_state, tmdsclk);

	if (hdmi->link_cfg.frl_mode) {
		/* if current frl training failed, set lower frl rate */
		if (hdmi->force_frl_rate) {
			u8 rate_per_lane, frl_lanes;

			dw_hdmi_get_lane_cfg_by_frl_rate(hdmi, hdmi->force_frl_rate,
							 &rate_per_lane, &frl_lanes);

			if (hdmi->link_cfg.rate_per_lane > rate_per_lane)
				hdmi->link_cfg.rate_per_lane = rate_per_lane;

			if (hdmi->link_cfg.frl_lanes > frl_lanes)
				hdmi->link_cfg.frl_lanes = frl_lanes;
		/* 40G is preferred */
		} else if (hdmi->link_cfg.rate_per_lane >= 12 ||
			   !hdmi->link_cfg.rate_per_lane) {
			hdmi->link_cfg.frl_lanes = 4;
			hdmi->link_cfg.rate_per_lane = 10;
		}

		phy_cfg.bpc = color_depth;
		phy_cfg.frl.lanes = hdmi->link_cfg.frl_lanes;
		phy_cfg.frl.rate_per_lane = hdmi->link_cfg.rate_per_lane;
		phy_hdmi_mode = PHY_HDMI_MODE_FRL;
	} else {
		bus_width = rockchip_drm_hdmi_get_tmdsclock(hdmi->output_bus_format,
							    mode.crtc_clock * fva_factor);
		if (rockchip_drm_bus_fmt_is_yuv420(hdmi->output_bus_format))
			bus_width /= 2;

		phy_cfg.tmds_char_rate = bus_width * 1000;
		phy_hdmi_mode = PHY_HDMI_MODE_TMDS;

		if (color_depth == 10 &&
		    !rockchip_drm_bus_fmt_is_yuv422(hdmi->output_bus_format))
			phy_cfg.bpc = 10;
		else
			phy_cfg.bpc = 8;
	}

	if (hdmi->phy) {
		phy_set_mode_ext(hdmi->phy, PHY_MODE_HDMI, phy_hdmi_mode);
		phy_configure(hdmi->phy, (void *)&phy_cfg);
		phy_set_bus_width(hdmi->phy, hdmi->phy_bus_width);
	}

	s->output_type = DRM_MODE_CONNECTOR_HDMIA;
	s->tv_state = &conn_state->tv;

	if (hdmi->plat_data->split_mode) {
		s->output_flags |= ROCKCHIP_OUTPUT_DUAL_CHANNEL_LEFT_RIGHT_MODE;
		if (hdmi->plat_data->right && hdmi->id)
			s->output_flags |= ROCKCHIP_OUTPUT_DATA_SWAP;
		s->output_if |= VOP_OUTPUT_IF_HDMI0 | VOP_OUTPUT_IF_HDMI1;
		s->output_if_left_panel |= hdmi->id ? VOP_OUTPUT_IF_HDMI1 : VOP_OUTPUT_IF_HDMI0;
	} else if (hdmi->plat_data->dual_connector_split) {
		s->output_if |= hdmi->id ? VOP_OUTPUT_IF_HDMI1 : VOP_OUTPUT_IF_HDMI0;
		s->output_flags |= ROCKCHIP_OUTPUT_DUAL_CONNECTOR_SPLIT_MODE;
		if (hdmi->plat_data->left_display)
			s->output_if_left_panel |= hdmi->id ?
				VOP_OUTPUT_IF_HDMI1 : VOP_OUTPUT_IF_HDMI0;
	} else {
		s->output_if |= hdmi->id ? VOP_OUTPUT_IF_HDMI1 : VOP_OUTPUT_IF_HDMI0;
	}

	s->output_mode = output_mode;
	hdmi->bus_format = s->bus_format;

	s->dsc_enable = 0;
	if (hdmi->link_cfg.dsc_mode)
		dw_hdmi_qp_dsc_configure(hdmi, s, crtc_state);

	s->color_encoding = hdmi->colorimetry;

	if (colorformat == RK_IF_FORMAT_RGB)
		s->color_range = hdmi->hdmi_quant_range == HDMI_QUANTIZATION_RANGE_LIMITED ?
					DRM_COLOR_YCBCR_LIMITED_RANGE : DRM_COLOR_YCBCR_FULL_RANGE;
	else
		s->color_range = hdmi->hdmi_quant_range == HDMI_QUANTIZATION_RANGE_FULL ?
					DRM_COLOR_YCBCR_FULL_RANGE : DRM_COLOR_YCBCR_LIMITED_RANGE;

	if (hdmi->plat_data->split_mode && !secondary) {
		hdmi = rockchip_hdmi_find_by_id(hdmi->dev->driver, !hdmi->id);
		secondary = true;
		goto secondary;
	}

	return 0;
}

static unsigned long dw_hdmi_rockchip_get_input_bus_format(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return hdmi->bus_format;
}

static unsigned long dw_hdmi_rockchip_get_output_bus_format(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return hdmi->output_bus_format;
}

static unsigned long dw_hdmi_rockchip_get_enc_in_encoding(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return hdmi->enc_out_encoding;
}

static unsigned long dw_hdmi_rockchip_get_enc_out_encoding(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return hdmi->enc_out_encoding;
}

static unsigned long dw_hdmi_rockchip_get_quant_range(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return hdmi->hdmi_quant_range;
}

static struct drm_property *dw_hdmi_rockchip_get_hdr_property(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return hdmi->hdr_panel_metadata_property;
}

static struct drm_property_blob *dw_hdmi_rockchip_get_hdr_blob(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return hdmi->hdr_panel_blob_ptr;
}

static void dw_hdmi_rockchip_update_color_format(struct drm_connector_state *conn_state,
						 void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	dw_hdmi_rockchip_check_color(conn_state, hdmi);
}

static bool dw_hdmi_rockchip_get_color_changed(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	bool ret = false;

	if (hdmi->color_changed)
		ret = true;
	hdmi->color_changed = 0;

	return ret;
}

static int
dw_hdmi_rockchip_get_yuv422_format(struct drm_connector *connector,
				   const struct edid *edid, int ext_block_num)
{
	if (!connector || !edid)
		return -EINVAL;

	return rockchip_drm_get_yuv422_format(connector, edid, ext_block_num);
}

static void dw_hdmi_rockchip_get_vrr_range(struct rockchip_dw_hdmi_qp *hdmi)
{
	u8 vrr_min, vrr_max;
	bool qms, qms_tfr_min, qms_tfr_max;

	if (hdmi->hdmi21_data.vrr_cap.qms)
		hdmi->vrr_cap.vrr_mode = BIT(QMS_VRR_SUPPORTED);
	if (hdmi->hdmi21_data.vrr_cap.vrr_min || hdmi->hdmi21_data.vrr_cap.vrr_max)
		hdmi->vrr_cap.vrr_mode |= BIT(GAMING_VRR_SUPPORTED);
	if (hdmi->hdmi21_data.vrr_cap.fva)
		hdmi->vrr_cap.vrr_mode |= BIT(FVA_SUPPORTED);

	vrr_min = hdmi->hdmi21_data.vrr_cap.vrr_min;
	vrr_max = hdmi->hdmi21_data.vrr_cap.vrr_max;
	qms = hdmi->hdmi21_data.vrr_cap.qms;
	qms_tfr_min = hdmi->hdmi21_data.vrr_cap.qms_tfr_min;
	qms_tfr_max = hdmi->hdmi21_data.vrr_cap.qms_tfr_max;

	if (qms) {
		if (!vrr_min && !vrr_max) {
			if (!qms_tfr_min && !qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = 60;
				hdmi->vrr_cap.qms_vrr_rate_min = 48;
			} else if (!qms_tfr_min && qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = 120;
				hdmi->vrr_cap.qms_vrr_rate_min = 48;
			} else if (qms_tfr_min && !qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = 60;
				hdmi->vrr_cap.qms_vrr_rate_min = 24;
			} else {
				hdmi->vrr_cap.qms_vrr_rate_max = 120;
				hdmi->vrr_cap.qms_vrr_rate_min = 60;
			}
		} else if (vrr_min && !vrr_max) {
			if (!qms_tfr_min && !qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = 60;
				hdmi->vrr_cap.qms_vrr_rate_min = vrr_min;
			} else if (!qms_tfr_min && qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = 120;
				hdmi->vrr_cap.qms_vrr_rate_min = vrr_min;
			} else if (qms_tfr_min && !qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = 60;
				hdmi->vrr_cap.qms_vrr_rate_min = vrr_min;
			} else {
				hdmi->vrr_cap.qms_vrr_rate_max = 120;
				hdmi->vrr_cap.qms_vrr_rate_min = 60;
			}
		} else {
			if (!qms_tfr_min && !qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = 60;
				hdmi->vrr_cap.qms_vrr_rate_min = vrr_min;
			} else if (!qms_tfr_min && qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = vrr_max;
				hdmi->vrr_cap.qms_vrr_rate_min = vrr_min;
			} else if (qms_tfr_min && !qms_tfr_max) {
				hdmi->vrr_cap.qms_vrr_rate_max = 60;
				hdmi->vrr_cap.qms_vrr_rate_min = 24;
			} else {
				hdmi->vrr_cap.qms_vrr_rate_max = vrr_max;
				hdmi->vrr_cap.qms_vrr_rate_min = 24;
			}
		}
	} else {
		hdmi->vrr_cap.qms_vrr_rate_max = 0;
		hdmi->vrr_cap.qms_vrr_rate_min = 0;
	}

	if (!vrr_max && vrr_min) {
		hdmi->vrr_cap.gaming_vrr_rate_max = 120;
		hdmi->vrr_cap.gaming_vrr_rate_min = vrr_min;
	} else if (vrr_max && vrr_min) {
		hdmi->vrr_cap.gaming_vrr_rate_max = vrr_max;
		hdmi->vrr_cap.gaming_vrr_rate_min = vrr_min;
	} else {
		hdmi->vrr_cap.gaming_vrr_rate_max = 0;
		hdmi->vrr_cap.gaming_vrr_rate_min = 0;
	}
}

static int
dw_hdmi_rockchip_get_edid_hdmi21_info(void *data, const struct edid *edid,
				      struct drm_connector *connector, int ext_block_num)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct drm_property *property = hdmi->hdmi_vrr_cap;
	struct drm_property_blob *blob = hdmi->hdmi_vrr_cap_ptr;
	size_t size = sizeof(struct hdmi_vrr_capacity);
	int ret;

	if (!edid)
		return -EINVAL;

	memset(&hdmi->hdmi21_data, 0, sizeof(hdmi->hdmi21_data));
	memset(&hdmi->vrr_cap, 0, size);

	ret = rockchip_drm_parse_cea_ext(&hdmi->hdmi21_data, edid, ext_block_num);
	if (ret)
		return ret;

	dw_hdmi_rockchip_get_vrr_range(hdmi);

	ret = drm_property_replace_global_blob(connector->dev, &blob, size, &hdmi->vrr_cap,
					       &connector->base, property);

	return ret;
}

static int
dw_hdmi_rockchip_get_hdr10_plus_vsdb(void *data, const struct edid *edid,
				     struct drm_connector *connector, int ext_block_num)
{
	int ret;
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct drm_property *property = hdmi->hdr10_plus_vsdb;
	u8 hdr10_plus;

	if (!edid || !connector)
		return -ENOMEM;

	hdr10_plus = rockchip_drm_parse_hdr10_plus_vsdb(edid, ext_block_num);

	hdmi->hdr10_plus_data.application_version = hdr10_plus & 0x3;
	hdmi->hdr10_plus_data.full_frame_peak_luminance_index = hdr10_plus & 0xc;
	hdmi->hdr10_plus_data.peak_luminance_index = hdr10_plus & 0xf0;

	ret = drm_property_replace_global_blob(connector->dev, &hdmi->hdr10_plus_vsdb_ptr,
					       3, &hdmi->hdr10_plus_data, &connector->base,
					       property);

	return ret;
}

static int
dw_hdmi_rockchip_get_dovi_data(void *data, const struct edid *edid,
			       struct drm_connector *connector, int ext_block_num)
{
	int ret;
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u8 *sink_data = hdmi->dovi_vsdb;
	struct drm_property *property = hdmi->hdr_panel_dovi_vsdb;

	if (!edid || !connector)
		return -ENOMEM;

	rockchip_drm_parse_dovi(sink_data, edid, ext_block_num);

	ret = drm_property_replace_global_blob(connector->dev, &hdmi->hdr_panel_dovi_vsdb_ptr,
					       DOVI_VSDB_LEN, sink_data, &connector->base,
					       property);

	return ret;
}

static int dw_hdmi_rockchip_get_colorimetry(void *data, const struct edid *edid, int ext_block_num)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return rockchip_drm_parse_colorimetry_data_block(&hdmi->edid_colorimetry, edid,
							 ext_block_num);
}

static void dw_hdmi_rockchip_get_vsif_data(void *data, u32 *buf)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct dovi_vsif_data *vsif;
	u8 vsif_db[32] = {0};
	int i;

	if (!buf)
		return;

	if (!hdmi->vsif_data_ptr)
		return;

	vsif = (struct dovi_vsif_data *)hdmi->vsif_data_ptr->data;

	for (i = 0; i < 3; i++)
		vsif_db[i] = vsif->header[i];
	for (i = 0; i < 28; i++)
		vsif_db[i + 4] = vsif->pb[i];

	memcpy(buf, vsif_db, DOVI_VSIF_LEN * 4);
}

static struct dw_hdmi_link_config *dw_hdmi_rockchip_get_link_cfg(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return &hdmi->link_cfg;
}

static int dw_hdmi_rockchip_get_vp_id(struct drm_crtc_state *crtc_state)
{
	struct rockchip_crtc_state *s;

	s = to_rockchip_crtc_state(crtc_state);

	return s->vp_id;
}

static int dw_hdmi_dclk_set(void *data, bool enable, int vp_id)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	char clk_name[16];
	struct clk *dclk;
	int ret;

	snprintf(clk_name, sizeof(clk_name), "dclk_vp%d", vp_id);

	dclk = devm_clk_get_optional(hdmi->dev, clk_name);
	if (IS_ERR(dclk)) {
		DRM_DEV_ERROR(hdmi->dev, "failed to get %s\n", clk_name);
		return PTR_ERR(dclk);
	} else if (!dclk) {
		DRM_DEV_ERROR(hdmi->dev, "failed to get %s\n", clk_name);
		return -ENOENT;
	}

	if (enable) {
		ret = clk_prepare_enable(dclk);
		if (ret < 0)
			DRM_DEV_ERROR(hdmi->dev, "failed to enable dclk for video port%d - %d\n",
				      vp_id, ret);
	} else {
		clk_disable_unprepare(dclk);
	}

	return 0;
}

static int dw_hdmi_link_clk_set(void *data, u32 rate, bool enable)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u64 phy_clk;
	int ret;

	if (enable) {
		ret = clk_prepare_enable(hdmi->link_clk);
		if (ret < 0) {
			DRM_DEV_ERROR(hdmi->dev, "failed to enable link_clk %d\n", ret);
			return ret;
		}

		/*
		 * In frl mode, the unit for frl rate requirements
		 * is GHz, but the output rate of the HDMI PHY PLL
		 * needs to be set to GHz / 10.
		 */
		if (hdmi->link_cfg.frl_mode)
			phy_clk = (u64)hdmi->link_cfg.rate_per_lane * hdmi->link_cfg.frl_lanes
				* 100000000ULL;
		else
			phy_clk = rate;

		/*
		 * When HDMI outputs YUV420, the dclk frequency output from the hdptx-phy
		 * PLL to the VOP is only half of the pixel clock. Therefore, the link clock
		 * frequency configured for the HDMI controller should also be set to half of
		 * the pixel clock.
		 */
		phy_clk = clk_round_rate(hdmi->link_clk, phy_clk);
		clk_set_rate(hdmi->link_clk, phy_clk);
	} else {
		clk_disable_unprepare(hdmi->link_clk);
	}
	return 0;
}

static void dw_hdmi_rockchip_set_prev_bus_format(void *data, unsigned long bus_format)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	hdmi->prev_bus_format = bus_format;
}

static void rk3572_set_hdcp14_mem(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (!hdmi->vo0_regmap)
		return;

	val = HIWORD_UPDATE(enable << 15, RK3576_HDMI_HDCP14_MEM_EN);
	regmap_write(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON0, val);
}


static void rk3576_set_hdcp14_mem(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (!hdmi->vo0_regmap)
		return;

	val = HIWORD_UPDATE(enable << 15, RK3576_HDMI_HDCP14_MEM_EN);
	regmap_write(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON1, val);
}

static void rk3588_set_hdcp14_mem(struct rockchip_dw_hdmi_qp *hdmi, bool enable)
{
	u32 val;

	if (!hdmi->vo1_regmap)
		return;

	val = HIWORD_UPDATE(enable << 15, RK3588_HDMI_HDCP14_MEM_EN);
	if (!hdmi->id)
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON4, val);
	else
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON7, val);
}

static void dw_hdmi_rockchip_set_hdcp14_mem(void *data, bool enable)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	hdmi->chip_data->ops->set_hdcp14_mem(hdmi, enable);
}

static struct drm_display_mode *dw_hdmi_rockchip_get_force_timing(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	if (!hdmi->timing_force_output)
		return NULL;

	return &hdmi->force_mode;
}

static u32 dw_hdmi_rockchip_get_refclk_rate(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return clk_get_rate(hdmi->hdmitx_ref);
}

static void dw_hdmi_rockchip_get_frl_mode(u8 rate, u8 *lanes, u8 *rate_per_lane)
{
	switch (rate) {
	case 48:
		*lanes = 4;
		*rate_per_lane = 12;
		break;
	case 40:
		*lanes = 4;
		*rate_per_lane = 10;
		break;
	case 32:
		*lanes = 4;
		*rate_per_lane = 8;
		break;
	case 24:
		*lanes = 4;
		*rate_per_lane = 6;
		break;
	case 18:
		*lanes = 3;
		*rate_per_lane = 6;
		break;
	case 9:
		*lanes = 3;
		*rate_per_lane = 3;
		break;
	case 0:
		*lanes = 0;
		*rate_per_lane = 0;
		break;
	default:
		DRM_ERROR("Unknown frl rate :%d GHz\n", rate);
		break;
	}
}

static void dw_hdmi_rockchip_force_frl_rate(void *data, u8 rate_ghz)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct phy_configure_opts_hdmi phy_cfg = {0};

	if (!hdmi->link_cfg.frl_mode)
		return;

	phy_cfg.bpc = 8;
	phy_cfg.frl.lanes = hdmi->link_cfg.frl_lanes;
	phy_cfg.frl.rate_per_lane = hdmi->link_cfg.rate_per_lane;
	dw_hdmi_rockchip_get_frl_mode(rate_ghz, &phy_cfg.frl.lanes,
				      &phy_cfg.frl.rate_per_lane);

	hdmi->force_frl_rate = (u64)rate_ghz * 1000000000;
	phy_configure(hdmi->phy, (void *)&phy_cfg);
}

static u8 mode_color_caps_init(struct drm_connector *connector, struct drm_display_mode *mode,
			       struct drm_display_info *info)
{
	u8 color_caps_mask = BIT(RGB_8BIT);

	if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_30)
		color_caps_mask |= BIT(RGB_10BIT);

	if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444) {
		color_caps_mask |= BIT(YUV444_8BIT);
		if (info->edid_hdmi_ycbcr444_dc_modes & DRM_EDID_HDMI_DC_30)
			color_caps_mask |= BIT(YUV444_10BIT);
	}

	/* For hdmi, yuv422 8bit and 10bit are the same format */
	if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422) {
		color_caps_mask |= BIT(YUV422_8BIT);
		color_caps_mask |= BIT(YUV422_10BIT);
	}

	if (connector->ycbcr_420_allowed && drm_mode_is_420(info, mode)) {
		color_caps_mask |= BIT(YUV420_8BIT);
		if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_30)
			color_caps_mask |= BIT(YUV420_10BIT);
	}

	return color_caps_mask;
}

static void dw_hdmi_rockchip_get_mode_color_caps(struct drm_connector *connector,
						 struct drm_display_info *info,
						 void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct drm_display_mode *mode;
	u8 color_caps_mask;
	u32 max_tmds_clock = info->max_tmds_clock;
	u32 size = 0;
	struct rockchip_drm_mode_color_caps *caps;
	struct drm_property *property = hdmi->mode_color_capacity;

	if (list_empty(&connector->modes))
		return;

	list_for_each_entry(mode, &connector->modes, head)
		size++;

	kfree(hdmi->mode_color_caps);

	size = sizeof(struct rockchip_drm_mode_color_caps) * size;

	hdmi->mode_color_caps = kmalloc(size, GFP_KERNEL);
	if (!hdmi->mode_color_caps)
		return;
	caps = hdmi->mode_color_caps;

	max_tmds_clock = min(max_tmds_clock, hdmi->max_tmdsclk);

	list_for_each_entry(mode, &connector->modes, head) {
		color_caps_mask = mode_color_caps_init(connector, mode, info);

		drm_mode_convert_to_umode(&caps->umode, mode);
		/* hdmi 2.1 frl mode */
		if (mode->clock > 600000) {
			if (mode->clock > 1188000)
				color_caps_mask = BIT(YUV420_8BIT) | BIT(YUV420_10BIT);

			caps->color_caps = color_caps_mask;
			caps++;
			continue;
		}

		/* RGB/YUV444 10BIT is out of range */
		if ((mode->clock * 10 / 8) > max_tmds_clock && mode->clock <= max_tmds_clock) {
			color_caps_mask &= ~(BIT(RGB_10BIT) | BIT(YUV444_10BIT));
		/* only support YUV420 */
		} else if (mode->clock > max_tmds_clock && (mode->clock / 2) <= max_tmds_clock) {
			color_caps_mask &= ~(BIT(RGB_10BIT) | BIT(RGB_8BIT) | BIT(YUV444_10BIT) |
					     BIT(YUV444_8BIT) | BIT(YUV422_10BIT) |
					     BIT(YUV422_8BIT));

			/* YUV420 10BIT is out of range */
			if ((mode->clock / 2) * 10 / 8 > max_tmds_clock)
				color_caps_mask &= ~BIT(YUV420_10BIT);
		}

		caps->color_caps = color_caps_mask;
		caps++;
	}

	drm_property_replace_global_blob(connector->dev, &hdmi->mode_color_caps_ptr, size,
					 hdmi->mode_color_caps, &connector->base, property);
}

static bool dw_hdmi_rockchip_get_qms_vrr_support(u8 vic)
{
	u8 qms_vrr_vic[] = {16, 63, 4, 47, 97, 102};
	int i;

	for (i = 0; i < ARRAY_SIZE(qms_vrr_vic); i++) {
		if (vic == qms_vrr_vic[i])
			return true;
	}

	return false;
}

static int dw_hdmi_mode_vrefresh(const struct drm_display_mode *mode)
{
	unsigned int num = 1, den = 1;

	if (mode->htotal == 0 || mode->vtotal == 0)
		return 0;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		num *= 2;
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		den *= 2;
	if (mode->vscan > 1)
		den *= mode->vscan;

	if (check_mul_overflow(mode->clock, num, &num))
		return 0;

	if (check_mul_overflow(mode->htotal * mode->vtotal, den, &den))
		return 0;

	return DIV_ROUND_CLOSEST_ULL(mul_u32_u32(num, 100000), den);
}

static void dw_hdmi_rockchip_get_mode_info(struct drm_connector *connector,
					   struct drm_display_info *info,
					   void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct drm_display_mode *mode;
	struct rockchip_drm_private *private = connector->dev->dev_private;
	struct rockchip_drm_modes_info *modes_info;
	struct rockchip_drm_mode_info *mode_info;
	u32 size, mode_count = 0;
	u32 qms_vrr_rate[] = {23970, 24000, 25000, 29970, 30000, 47950, 48000, 50000, 59940,
			      60000, 100000, 119880, 120000};
	int refresh_rate;
	int i = 0, j;

	if (list_empty(&connector->modes)) {
		drm_property_replace_global_blob(connector->dev, &hdmi->mode_infos_blob_ptr, 0,
						 0, &connector->base, private->mode_info_prop);
		return;
	}

	if (!(hdmi->vrr_cap.vrr_mode & (BIT(QMS_VRR_SUPPORTED) | BIT(GAMING_VRR_SUPPORTED))))
		return;

	list_for_each_entry(mode, &connector->modes, head)
		mode_count++;

	size = struct_size(modes_info, mode_info, mode_count);
	modes_info = kzalloc(size, GFP_KERNEL);
	if (!modes_info)
		return;

	modes_info->version = ROCKCHIP_MODE_INFO_V1;
	modes_info->mode_count = mode_count;
	list_for_each_entry(mode, &connector->modes, head) {
		mode_info = &modes_info->mode_info[i];
		drm_mode_convert_to_umode(&mode_info->umode, mode);
		i++;

		refresh_rate = dw_hdmi_mode_vrefresh(mode);
		if (hdmi->vrr_cap.vrr_mode & BIT(QMS_VRR_SUPPORTED)) {
			if (dw_hdmi_rockchip_get_qms_vrr_support(drm_match_cea_mode(mode))) {
				mode_info->mrr_support = 1;
				for (j = 0; j < ARRAY_SIZE(qms_vrr_rate); j++) {
					if (refresh_rate * 10 < qms_vrr_rate[j])
						break;
					mode_info->mrr_table[j] = qms_vrr_rate[j];
				}
				mode_info->mrr_count = j;
			}
		}
		if (hdmi->vrr_cap.vrr_mode & BIT(GAMING_VRR_SUPPORTED)) {
			if (refresh_rate >= hdmi->vrr_cap.gaming_vrr_rate_min * 100 &&
			    refresh_rate <= hdmi->vrr_cap.gaming_vrr_rate_max * 100) {
				mode_info->vrr_support = 1;
				mode_info->vrr_min_fps = hdmi->vrr_cap.gaming_vrr_rate_min * 1000;
				mode_info->vrr_max_fps =
					min(hdmi->vrr_cap.gaming_vrr_rate_max * 1000,
					    refresh_rate * 10);
				mode_info->vrr_fps_step = 1000;
			}
		}
	}

	drm_property_replace_global_blob(connector->dev, &hdmi->mode_infos_blob_ptr, size,
					 modes_info, &connector->base,
					 private->mode_info_prop);
	kfree(modes_info);
}

static void dw_hdmi_rockchip_crtc_post_enable(void *data, struct drm_crtc *crtc)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	int output_if;

	if (!crtc)
		return;

	switch (hdmi->id) {
	case 0:
		output_if = VOP_OUTPUT_IF_HDMI0;
		break;
	case 1:
		output_if = VOP_OUTPUT_IF_HDMI1;
		break;
	default:
		dev_err(hdmi->dev, "invalid id:%d\n", hdmi->id);
		return;
	}

	rockchip_drm_crtc_output_post_enable(crtc, output_if);
}

static void dw_hdmi_rockchip_crtc_pre_disable(void *data, struct drm_crtc *crtc)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	int output_if;

	if (!crtc)
		return;

	switch (hdmi->id) {
	case 0:
		output_if = VOP_OUTPUT_IF_HDMI0;
		break;
	case 1:
		output_if = VOP_OUTPUT_IF_HDMI1;
		break;
	default:
		dev_err(hdmi->dev, "invalid id:%d\n", hdmi->id);
		return;
	}

	rockchip_drm_crtc_output_pre_disable(crtc, output_if);
}

static void dw_hdmi_rockchip_set_cec_wakeup(void *data, bool enable)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u32 val;

	if (enable) {
		val = HIWORD_UPDATE(RK3538_HDMI_CEC_DET_SEL, RK3538_HDMI_CEC_DET_SEL);
		regmap_write(hdmi->regmap, RK3538_PMU_GRF_SOC_CON2, val);
	} else {
		val = HIWORD_UPDATE(0, RK3538_HDMI_CEC_DET_SEL);
		regmap_write(hdmi->regmap, RK3538_PMU_GRF_SOC_CON2, val);
	}
}

static int drm_property_replace_hdmi_blob(struct drm_device *dev,
					  struct drm_property_blob **replace,
					  size_t length,
					  const void *data,
					  struct drm_mode_object *obj_holds_id,
					  struct drm_property *prop_holds_id)
{
	struct drm_property_blob *new_blob = NULL;
	struct drm_property_blob *old_blob = NULL;
	int ret;

	WARN_ON(replace == NULL);

	old_blob = *replace;

	if (length && data) {
		new_blob = drm_property_create_blob(dev, length, data);
		if (IS_ERR(new_blob))
			return PTR_ERR(new_blob);
	}

	if (obj_holds_id) {
		ret = drm_object_property_set_value(obj_holds_id,
						    prop_holds_id,
						    new_blob ?
						    new_blob->base.id : 0);
		if (ret != 0)
			goto err_created;
	}

	drm_property_blob_put(old_blob);
	*replace = new_blob;

	return 0;

err_created:
	drm_property_blob_put(new_blob);
	return ret;
}

static int
dw_hdmi_rockchip_get_hdrvivid_vsdb(void *data, const struct edid *edid,
				   struct drm_connector *connector, int ext_block_num)
{
	int ret;
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u8 *sink_data = hdmi->hdrvivid_vsvdb;
	struct drm_property *property = hdmi->hdrvivid_vsdb;
	struct drm_property_blob *blob = hdmi->hdrvivid_vsdb_ptr;

	if (!edid || !connector)
		return -ENOMEM;

	rockchip_drm_parse_hdrvivid(sink_data, edid, ext_block_num);

	ret = drm_property_replace_hdmi_blob(connector->dev, &blob, 28, sink_data,
					     &connector->base, property);

	return ret;
};

static bool dw_hdmi_rockchip_get_emp_status(void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	return hdmi->dynamic_hdr_en;
};

static void dw_hdmi_rockchip_set_emp_bypass(void *data, bool enable)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	if (hdmi->chip_data->ops->set_emp_bypass_enable)
		hdmi->chip_data->ops->set_emp_bypass_enable(hdmi, enable);
};

static const struct drm_prop_enum_list color_depth_enum_list[] = {
	{ 0, "Automatic" }, /* Prefer highest color depth */
	{ 8, "24bit" },
	{ 10, "30bit" },
};

static const struct drm_prop_enum_list drm_hdmi_output_enum_list[] = {
	{ RK_IF_FORMAT_RGB, "rgb" },
	{ RK_IF_FORMAT_YCBCR444, "ycbcr444" },
	{ RK_IF_FORMAT_YCBCR422, "ycbcr422" },
	{ RK_IF_FORMAT_YCBCR420, "ycbcr420" },
	{ RK_IF_FORMAT_YCBCR_HQ, "ycbcr_high_subsampling" },
	{ RK_IF_FORMAT_YCBCR_LQ, "ycbcr_low_subsampling" },
	{ RK_IF_FORMAT_MAX, "invalid_output" },
};

static const struct drm_prop_enum_list quant_range_enum_list[] = {
	{ HDMI_QUANTIZATION_RANGE_DEFAULT, "default" },
	{ HDMI_QUANTIZATION_RANGE_LIMITED, "limit" },
	{ HDMI_QUANTIZATION_RANGE_FULL, "full" },
};

static const struct drm_prop_enum_list output_hdmi_dvi_enum_list[] = {
	{ 0, "auto" },
	{ 1, "force_hdmi" },
	{ 2, "force_dvi" },
};

static const struct drm_prop_enum_list output_type_cap_list[] = {
	{ 0, "DVI" },
	{ 1, "HDMI" },
};

static const struct drm_prop_enum_list allm_enable_list[] = {
	{ 0, "disable" },
	{ 1, "enable" },
};

static const struct drm_prop_enum_list dynamic_hdr_enable_list[] = {
	{ 0, "disable" },
	{ 1, "enable" },
};

static const struct drm_prop_enum_list gaming_vrr_enable_list[] = {
	{ 0, "disable" },
	{ 1, "enable" },
};

static const struct drm_prop_enum_list next_tfr_list[] = {
	{ 0, "disable" },
	{ 1, "23.97Hz" },
	{ 2, "24Hz" },
	{ 3, "25Hz" },
	{ 4, "29.97Hz" },
	{ 5, "30Hz" },
	{ 6, "47.95Hz" },
	{ 7, "48Hz" },
	{ 8, "50Hz" },
	{ 9, "59.94Hz" },
	{ 10, "60Hz" },
	{ 11, "100Hz" },
	{ 12, "119.88Hz" },
	{ 13, "120Hz" },
};

static void dw_hdmi_rockchip_attach_properties(struct drm_connector *connector, unsigned int color,
					       int version, void *data, bool allm_en)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct drm_property *prop;
	struct rockchip_drm_private *private = connector->dev->dev_private;
	int ret;

	rockchip_drm_parse_bus_format(color, &hdmi->hdmi_output, &hdmi->colordepth);

	hdmi->bus_format = color;
	hdmi->prev_bus_format = color;

	if (hdmi->hdmi_output == RK_IF_FORMAT_YCBCR422) {
		if (hdmi->colordepth == 12)
			hdmi->output_bus_format = MEDIA_BUS_FMT_YUYV12_1X24;
		else if (hdmi->colordepth == 10)
			hdmi->output_bus_format = MEDIA_BUS_FMT_YUYV10_1X20;
		else
			hdmi->output_bus_format = MEDIA_BUS_FMT_YUYV8_1X16;
	} else {
		hdmi->output_bus_format = hdmi->bus_format;
	}

	if (!hdmi->color_depth_property) {
		prop = drm_property_create_enum(connector->dev, 0, RK_IF_PROP_COLOR_DEPTH,
						color_depth_enum_list,
						ARRAY_SIZE(color_depth_enum_list));
		if (prop) {
			hdmi->color_depth_property = prop;
			drm_object_attach_property(&connector->base, prop, 0);
		}
	}

	prop = drm_property_create_enum(connector->dev, 0, RK_IF_PROP_COLOR_FORMAT,
					drm_hdmi_output_enum_list,
					ARRAY_SIZE(drm_hdmi_output_enum_list));
	if (prop) {
		hdmi->hdmi_output_property = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_range(connector->dev, 0, RK_IF_PROP_COLOR_DEPTH_CAPS, 0, 0xff);
	if (prop) {
		hdmi->colordepth_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_range(connector->dev, 0, RK_IF_PROP_COLOR_FORMAT_CAPS, 0, 0xf);
	if (prop) {
		hdmi->outputmode_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create(connector->dev, DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE,
				   "HDR_PANEL_METADATA", 0);
	if (prop) {
		hdmi->hdr_panel_metadata_property = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create(connector->dev,
				   DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE,
				   "HDR_PANEL_DOVI_VSDB", 0);
	if (prop) {
		hdmi->hdr_panel_dovi_vsdb = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_bool(connector->dev, 0, "allm_capacity");
	if (prop) {
		hdmi->allm_capacity = prop;
		drm_object_attach_property(&connector->base, prop,
					   hdmi->hdmi21_data.allm_supported);
	}

	prop = drm_property_create_enum(connector->dev, 0, "allm_enable", allm_enable_list,
					ARRAY_SIZE(allm_enable_list));
	if (prop) {
		hdmi->allm_enable = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}
	hdmi->enable_allm = allm_en;

	prop = drm_property_create(connector->dev, DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE,
				"HDR10_PLUS_VSDB", 0);
	if (prop) {
		hdmi->hdr10_plus_vsdb = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_enum(connector->dev, 0,
					"gaming_vrr_enable",
					gaming_vrr_enable_list,
					ARRAY_SIZE(gaming_vrr_enable_list));
	if (prop) {
		hdmi->gaming_vrr_enable = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_enum(connector->dev, 0,
					"next_tfr",
					next_tfr_list,
					ARRAY_SIZE(next_tfr_list));
	if (prop) {
		hdmi->next_tfr = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}
	prop = drm_property_create_range(connector->dev, 0, "fva_factor_m1", 0, 15);
	if (prop) {
		hdmi->fva_factor_m1 = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create(connector->dev,
				   DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "hdmi_vrr_cap", 0);
	if (prop) {
		hdmi->hdmi_vrr_cap = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create(connector->dev,
				   DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "HDR_VIVID_VSDB", 0);
	if (prop) {
		hdmi->hdrvivid_vsdb = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_enum(connector->dev, 0,
					"dynamic_hdr_enable",
					dynamic_hdr_enable_list,
					ARRAY_SIZE(dynamic_hdr_enable_list));
	if (prop) {
		hdmi->dynamic_hdr_enable = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_enum(connector->dev, 0,
					"output_hdmi_dvi",
					output_hdmi_dvi_enum_list,
					ARRAY_SIZE(output_hdmi_dvi_enum_list));
	if (prop) {
		hdmi->output_hdmi_dvi = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create(connector->dev, DRM_MODE_PROP_BLOB, "VSIF_DATA", 0);
	if (prop) {
		hdmi->vsif_data = prop;
		drm_object_attach_property(&connector->base, hdmi->vsif_data, 0);
	}

	prop = drm_property_create_enum(connector->dev, 0, "output_type_capacity",
					output_type_cap_list,
					ARRAY_SIZE(output_type_cap_list));
	if (prop) {
		hdmi->output_type_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_enum(connector->dev, 0, "quant_range", quant_range_enum_list,
					ARRAY_SIZE(quant_range_enum_list));
	if (prop) {
		hdmi->quant_range = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = connector->dev->mode_config.hdr_output_metadata_property;
	drm_object_attach_property(&connector->base, prop, 0);

	if (!drm_mode_create_hdmi_colorspace_property(connector, 0))
		drm_object_attach_property(&connector->base, connector->colorspace_property, 0);
	drm_object_attach_property(&connector->base, private->connector_id_prop, hdmi->id);

	ret = drm_connector_attach_content_protection_property(connector, true);
	if (ret) {
		dev_err(hdmi->dev, "failed to attach content protection: %d\n", ret);
		return;
	}

	prop = drm_property_create_range(connector->dev, 0, RK_IF_PROP_ENCRYPTED,
					 RK_IF_HDCP_ENCRYPTED_NONE, RK_IF_HDCP_ENCRYPTED_LEVEL2);
	if (!prop) {
		dev_err(hdmi->dev, "create hdcp encrypted prop for hdmi%d failed\n", hdmi->id);
		return;
	}
	hdmi->hdcp_state_property = prop;
	drm_object_attach_property(&connector->base, prop, RK_IF_HDCP_ENCRYPTED_NONE);

	prop = drm_property_create(connector->dev, DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE,
				   "MODE_COLOR_CAPACITY", 0);
	if (prop) {
		hdmi->mode_color_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_range(connector->dev, 0, "colorspace_caps", 0, 0x1fff);
	if (prop) {
		hdmi->hdmi_colorspace_caps = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}
	drm_object_attach_property(&connector->base, private->mode_info_prop, 0);
}

static void dw_hdmi_rockchip_destroy_properties(struct drm_connector *connector, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	if (hdmi->color_depth_property) {
		drm_property_destroy(connector->dev, hdmi->color_depth_property);
		hdmi->color_depth_property = NULL;
	}

	if (hdmi->hdmi_output_property) {
		drm_property_destroy(connector->dev, hdmi->hdmi_output_property);
		hdmi->hdmi_output_property = NULL;
	}

	if (hdmi->colordepth_capacity) {
		drm_property_destroy(connector->dev, hdmi->colordepth_capacity);
		hdmi->colordepth_capacity = NULL;
	}

	if (hdmi->outputmode_capacity) {
		drm_property_destroy(connector->dev, hdmi->outputmode_capacity);
		hdmi->outputmode_capacity = NULL;
	}

	if (hdmi->quant_range) {
		drm_property_destroy(connector->dev, hdmi->quant_range);
		hdmi->quant_range = NULL;
	}

	if (hdmi->hdr_panel_metadata_property) {
		drm_property_destroy(connector->dev, hdmi->hdr_panel_metadata_property);
		hdmi->hdr_panel_metadata_property = NULL;
	}

	if (hdmi->hdr_panel_dovi_vsdb) {
		drm_property_destroy(connector->dev, hdmi->hdr_panel_dovi_vsdb);
		hdmi->hdr_panel_dovi_vsdb = NULL;
	}

	if (hdmi->output_hdmi_dvi) {
		drm_property_destroy(connector->dev, hdmi->output_hdmi_dvi);
		hdmi->output_hdmi_dvi = NULL;
	}

	if (hdmi->output_type_capacity) {
		drm_property_destroy(connector->dev, hdmi->output_type_capacity);
		hdmi->output_type_capacity = NULL;
	}

	if (hdmi->allm_capacity) {
		drm_property_destroy(connector->dev, hdmi->allm_capacity);
		hdmi->allm_capacity = NULL;
	}

	if (hdmi->allm_enable) {
		drm_property_destroy(connector->dev, hdmi->allm_enable);
		hdmi->allm_enable = NULL;
	}

	if (hdmi->mode_color_capacity) {
		kfree(hdmi->mode_color_caps);
		hdmi->mode_color_caps = NULL;
		drm_property_destroy(connector->dev, hdmi->mode_color_capacity);
		hdmi->mode_color_capacity = NULL;
	}

	if (hdmi->hdr_panel_dovi_vsdb) {
		drm_property_destroy(connector->dev, hdmi->hdr_panel_dovi_vsdb);
		hdmi->hdr_panel_dovi_vsdb = NULL;
	}

	if (hdmi->vsif_data) {
		drm_property_destroy(connector->dev, hdmi->vsif_data);
		hdmi->vsif_data = NULL;
	}

	if (hdmi->hdr10_plus_vsdb) {
		drm_property_destroy(connector->dev, hdmi->hdr10_plus_vsdb);
		hdmi->hdr10_plus_vsdb = NULL;
	}

	if (hdmi->gaming_vrr_enable) {
		drm_property_destroy(connector->dev, hdmi->gaming_vrr_enable);
		hdmi->gaming_vrr_enable = NULL;
	}

	if (hdmi->next_tfr) {
		drm_property_destroy(connector->dev, hdmi->next_tfr);
		hdmi->next_tfr = NULL;
	}

	if (hdmi->fva_factor_m1) {
		drm_property_destroy(connector->dev, hdmi->fva_factor_m1);
		hdmi->fva_factor_m1 = NULL;
	}

	if (hdmi->hdmi_vrr_cap) {
		drm_property_destroy(connector->dev, hdmi->hdmi_vrr_cap);
		hdmi->hdmi_vrr_cap = NULL;
	}

	if (hdmi->hdmi_colorspace_caps) {
		drm_property_destroy(connector->dev, hdmi->hdmi_colorspace_caps);
		hdmi->hdmi_colorspace_caps = NULL;
	}

	if (hdmi->hdrvivid_vsdb) {
		drm_property_destroy(connector->dev, hdmi->hdrvivid_vsdb);
		hdmi->hdrvivid_vsdb = NULL;
	}

	if (hdmi->dynamic_hdr_enable) {
		drm_property_destroy(connector->dev, hdmi->dynamic_hdr_enable);
		hdmi->dynamic_hdr_enable = NULL;
	}
}

static int
dw_hdmi_rockchip_set_property(struct drm_connector *connector, struct drm_connector_state *state,
			      struct drm_property *property, u64 val, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct drm_mode_config *config = &connector->dev->mode_config;
	int ret;
	u64 allm_enable;
	bool replaced = false;

	if (property == hdmi->color_depth_property) {
		hdmi->colordepth = val;
		return 0;
	} else if (property == hdmi->hdmi_output_property) {
		hdmi->hdmi_output = val;
		return 0;
	} else if (property == hdmi->quant_range) {
		u64 quant_range = hdmi->hdmi_quant_range;

		hdmi->hdmi_quant_range = val;
		if (quant_range != hdmi->hdmi_quant_range)
			dw_hdmi_qp_set_quant_range(hdmi->hdmi_qp, connector);
		return 0;
	} else if (property == config->hdr_output_metadata_property) {
		return 0;
	} else if (property == hdmi->output_hdmi_dvi) {
		hdmi->force_output = val;
		dw_hdmi_qp_set_output_type(hdmi->hdmi_qp, val);
		return 0;
	} else if (property == hdmi->colordepth_capacity) {
		return 0;
	} else if (property == hdmi->outputmode_capacity) {
		return 0;
	} else if (property == hdmi->output_type_capacity) {
		return 0;
	} else if (property == hdmi->allm_capacity) {
		return 0;
	} else if (property == hdmi->allm_enable) {
		allm_enable = hdmi->enable_allm;
		hdmi->enable_allm = val;
		if (allm_enable != hdmi->enable_allm)
			dw_hdmi_qp_set_allm_enable(hdmi->hdmi_qp, connector, hdmi->enable_allm);
		return 0;
	} else if (property == hdmi->hdcp_state_property) {
		return 0;
	} else if (property == hdmi->mode_color_capacity) {
		return 0;
	} else if (property == hdmi->hdr_panel_dovi_vsdb) {
		return 0;
	} else if (property == hdmi->vsif_data) {
		ret = rockchip_drm_atomic_replace_property_blob_from_id(connector->dev,
									&hdmi->vsif_data_ptr,
									val, -1, -1, &replaced);
		return ret;
	} else if (property == hdmi->hdr10_plus_vsdb) {
		return 0;
	} else if (property == hdmi->hdrvivid_vsdb) {
		return 0;
	} else if (property == hdmi->dynamic_hdr_enable) {
		hdmi->dynamic_hdr_en = val;
		return 0;
	} else if (property == hdmi->gaming_vrr_enable) {
		if (val && hdmi->next_tfr_val) {
			DRM_WARN("vrr-qms is enabled, can't set gaming vrr\n");
			return 0;
		}

		hdmi->enable_gaming_vrr = val;
		dw_hdmi_qp_set_gaming_vrr_enable(hdmi->hdmi_qp, hdmi->enable_gaming_vrr);
		return 0;
	} else if (property == hdmi->next_tfr) {
		return rockchip_hdmi_set_qms_next_tfr(hdmi, val);
	} else if (property == hdmi->fva_factor_m1) {
		hdmi->fva_factor_m1_val = (u8)val;
		dw_hdmi_qp_set_fva_factor_m1(hdmi->hdmi_qp, hdmi->fva_factor_m1_val);
		return 0;
	} else if (property == hdmi->hdmi_vrr_cap) {
		return 0;
	} else if (property == hdmi->hdmi_colorspace_caps) {
		return 0;
	}

	DRM_ERROR("Unknown property [PROP:%d:%s]\n", property->base.id, property->name);

	return -EINVAL;
}

static int dw_hdmi_rockchip_get_property(struct drm_connector *connector,
					 const struct drm_connector_state *state,
					 struct drm_property *property, u64 *val, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	struct drm_display_info *info = &connector->display_info;
	struct drm_mode_config *config = &connector->dev->mode_config;

	if (property == hdmi->color_depth_property) {
		*val = hdmi->colordepth;
		return 0;
	} else if (property == hdmi->hdmi_output_property) {
		*val = hdmi->hdmi_output;
		return 0;
	} else if (property == hdmi->colordepth_capacity) {
		*val = BIT(RK_IF_DEPTH_8);
		if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_30)
			*val |= BIT(RK_IF_DEPTH_10);
		if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_36)
			*val |= BIT(RK_IF_DEPTH_12);
		if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_48)
			*val |= BIT(RK_IF_DEPTH_16);
		if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_30)
			*val |= BIT(RK_IF_DEPTH_420_10);
		if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_36)
			*val |= BIT(RK_IF_DEPTH_420_12);
		if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_48)
			*val |= BIT(RK_IF_DEPTH_420_16);
		return 0;
	} else if (property == hdmi->outputmode_capacity) {
		*val = BIT(RK_IF_FORMAT_RGB);
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*val |= BIT(RK_IF_FORMAT_YCBCR444);
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*val |= BIT(RK_IF_FORMAT_YCBCR422);
		if (connector->ycbcr_420_allowed &&
		    info->color_formats & DRM_COLOR_FORMAT_YCBCR420)
			*val |= BIT(RK_IF_FORMAT_YCBCR420);
		return 0;
	} else if (property == hdmi->quant_range) {
		*val = hdmi->hdmi_quant_range;
		return 0;
	} else if (property == config->hdr_output_metadata_property) {
		*val = state->hdr_output_metadata ? state->hdr_output_metadata->base.id : 0;
		return 0;
	} else if (property == hdmi->output_hdmi_dvi) {
		*val = hdmi->force_output;
		return 0;
	} else if (property == hdmi->output_type_capacity) {
		*val = dw_hdmi_qp_get_output_type_cap(hdmi->hdmi_qp);
		return 0;
	} else if (property == hdmi->allm_capacity) {
		*val = hdmi->hdmi21_data.allm_supported;
		return 0;
	} else if (property == hdmi->allm_enable) {
		*val = hdmi->enable_allm;
		return 0;
	} else if (property == hdmi->hdcp_state_property) {
		if (hdmi->hdcp_status & BIT(1))
			*val = RK_IF_HDCP_ENCRYPTED_LEVEL2;
		else if (hdmi->hdcp_status & BIT(0))
			*val = RK_IF_HDCP_ENCRYPTED_LEVEL1;
		else
			*val = RK_IF_HDCP_ENCRYPTED_NONE;
		return 0;
	} else if (property == hdmi->mode_color_capacity) {
		*val = hdmi->mode_color_caps_ptr ? hdmi->mode_color_caps_ptr->base.id : 0;
		return 0;
	} else if (property == hdmi->hdr_panel_dovi_vsdb) {
		*val = (hdmi->hdr_panel_dovi_vsdb_ptr) ? hdmi->hdr_panel_dovi_vsdb_ptr->base.id : 0;
		return 0;
	} else if (property == hdmi->vsif_data) {
		*val = (hdmi->vsif_data_ptr) ? hdmi->vsif_data_ptr->base.id : 0;
		return 0;
	} else if (property == hdmi->hdr10_plus_vsdb) {
		*val = (hdmi->hdr10_plus_vsdb_ptr) ? hdmi->hdr10_plus_vsdb_ptr->base.id : 0;
		return 0;
	} else if (property == hdmi->gaming_vrr_enable) {
		*val = hdmi->enable_gaming_vrr;
		return 0;
	} else if (property == hdmi->next_tfr) {
		*val = hdmi->next_tfr_val;
		return 0;
	} else if (property == hdmi->fva_factor_m1) {
		*val = hdmi->fva_factor_m1_val;
		return 0;
	} else if (property == hdmi->hdmi_vrr_cap) {
		*val = hdmi->hdmi_vrr_cap_ptr ?
			hdmi->hdmi_vrr_cap_ptr->base.id : 0;
		return 0;
	} else if (property == hdmi->hdmi_colorspace_caps) {
		*val = hdmi->edid_colorimetry;
		return 0;
	} else if (property == hdmi->hdrvivid_vsdb) {
		*val = (hdmi->hdrvivid_vsdb_ptr) ? hdmi->hdrvivid_vsdb_ptr->base.id : 0;
		return 0;
	} else if (property == hdmi->dynamic_hdr_enable) {
		*val = hdmi->dynamic_hdr_en;
		return 0;
	}

	DRM_ERROR("Unknown property [PROP:%d:%s]\n", property->base.id, property->name);

	return -EINVAL;
}

static const struct dw_hdmi_property_ops dw_hdmi_rockchip_property_ops = {
	.attach_properties	= dw_hdmi_rockchip_attach_properties,
	.destroy_properties	= dw_hdmi_rockchip_destroy_properties,
	.set_property		= dw_hdmi_rockchip_set_property,
	.get_property		= dw_hdmi_rockchip_get_property,
};

static void dw_hdmi_rockchip_encoder_mode_set(struct drm_encoder *encoder,
					      struct drm_display_mode *mode,
					      struct drm_display_mode *adj)
{
	struct rockchip_dw_hdmi_qp *hdmi = to_rockchip_hdmi(encoder);
	struct drm_crtc *crtc;
	struct rockchip_crtc_state *s;

	if (!encoder->crtc)
		return;
	crtc = encoder->crtc;

	if (!crtc->state)
		return;
	s = to_rockchip_crtc_state(crtc->state);

	if (!s)
		return;

	s->dsc_enable = 0;
	if (hdmi->link_cfg.dsc_mode)
		dw_hdmi_qp_dsc_configure(hdmi, s, crtc->state);

	if (hdmi->phy)
		phy_set_bus_width(hdmi->phy, hdmi->phy_bus_width);
}

static const struct drm_encoder_helper_funcs dw_hdmi_rockchip_encoder_helper_funcs = {
	.enable     = dw_hdmi_rockchip_encoder_enable,
	.atomic_disable = dw_hdmi_rockchip_encoder_atomic_disable,
	.atomic_check = dw_hdmi_rockchip_encoder_atomic_check,
	.mode_set = dw_hdmi_rockchip_encoder_mode_set,
};

/*
 * Register child devices like audio/cec/hdcp at late_register stage
 * to avoid register these devie at probe/bind(which may cause
 * infinite loop of .probe() if a component is always defer)
 *
 * As these devices are not that critical, so we don't check
 * the register results here, just give warning in it's register
 * function if it failed, let the drm bringup.
 */
static int dw_hdmi_encoder_late_register(struct drm_encoder *encoder)
{
	struct rockchip_dw_hdmi_qp *hdmi = to_rockchip_hdmi(encoder);

	dw_hdmi_qp_register_audio(hdmi->hdmi_qp);
	dw_hdmi_qp_register_cec(hdmi->hdmi_qp);
	dw_hdmi_qp_register_hdcp(hdmi->hdmi_qp);

	return 0;
}

static const struct drm_encoder_funcs dw_hdmi_rockchip_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
	.late_register = dw_hdmi_encoder_late_register,
};

static void dw_hdmi_qp_rockchip_phy_disable(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	while (hdmi->phy->power_count > 0)
		phy_power_off(hdmi->phy);
}

static int dw_hdmi_qp_rockchip_genphy_init(struct dw_hdmi_qp *dw_hdmi, void *data,
					   struct drm_display_mode *mode)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	dw_hdmi_qp_rockchip_phy_disable(dw_hdmi, data);

	return phy_power_on(hdmi->phy);
}

static
void dw_hdmi_qp_rockchip_sda_delay_cal(void *data, u8 *sda_dlyn, u8 *sda_div)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u8 i;
	u32 val;

	for (i = 0; i <= 127; i++) {
		val = DIV_ROUND_UP(hdmi->sda_falling_delay_ns, (i + 1) * 40);

		if (val <= 15)
			break;
	}

	if (i > 127) {
		dev_err(hdmi->dev, "delay %d ns,can't calculate correct sda falling delay cfg\n",
			hdmi->sda_falling_delay_ns);
		return;
	}

	*sda_div = i;
	*sda_dlyn = val;
}

static void rk3538_io_path_init(struct rockchip_dw_hdmi_qp *hdmi)
{
	u32 val;

	if (!hdmi->vo0_regmap || !hdmi->regmap)
		return;

	val = HIWORD_UPDATE(RK3576_SCLIN_MASK, RK3576_SCLIN_MASK) |
	      HIWORD_UPDATE(RK3576_SDAIN_MASK, RK3576_SDAIN_MASK) |
	      HIWORD_UPDATE(RK3576_HDMITX_GRANT_SEL, RK3576_HDMITX_GRANT_SEL) |
	      HIWORD_UPDATE(RK3576_I2S_SEL_MASK, RK3576_I2S_SEL_MASK);
	regmap_write(hdmi->vo0_regmap, RK3538_VO_GRF_HDMI_SWITCH, val);

	val = HIWORD_UPDATE(0, RK3576_HDMITX_HPD_INT_MSK);
	regmap_write(hdmi->regmap, RK3538_HDMI_HPD_INT_CON, val);
}

static void rk3572_io_path_init(struct rockchip_dw_hdmi_qp *hdmi)
{
	u32 val;
	u8 sda_dlyn = 0, sda_div = 0;

	if (!hdmi->vo0_regmap || !hdmi->regmap)
		return;

	val = HIWORD_UPDATE(RK3576_SCLIN_MASK, RK3576_SCLIN_MASK) |
	      HIWORD_UPDATE(RK3576_SDAIN_MASK, RK3576_SDAIN_MASK) |
	      HIWORD_UPDATE(RK3576_HDMITX_GRANT_SEL, RK3576_HDMITX_GRANT_SEL) |
	      HIWORD_UPDATE(RK3576_I2S_SEL_MASK, RK3576_I2S_SEL_MASK);
	regmap_write(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON12, val);

	val = HIWORD_UPDATE(0, RK3576_HDMITX_HPD_INT_MSK);
	regmap_write(hdmi->regmap, RK3572_SYS_GRF_CON1, val);

	if (hdmi->sda_falling_delay_ns) {
		dw_hdmi_qp_rockchip_sda_delay_cal((void *)hdmi, &sda_dlyn, &sda_div);
		if (sda_dlyn) {
			val = HIWORD_UPDATE(sda_dlyn << 12, RK3576_GRF_OSDA_DLYN) |
			      HIWORD_UPDATE(sda_div << 1, RK3576_GRF_OSDA_DIV) |
			      HIWORD_UPDATE(1, RK3576_GRF_OSDA_DLY_EN);

			regmap_write(hdmi->vo0_regmap, RK3572_VO0_GRF_SOC_CON8, val);
		}
	}
}

static void rk3576_io_path_init(struct rockchip_dw_hdmi_qp *hdmi)
{
	u32 val;
	u8 sda_dlyn = 0, sda_div = 0;

	if (!hdmi->vo0_regmap)
		return;

	val = HIWORD_UPDATE(RK3576_SCLIN_MASK, RK3576_SCLIN_MASK) |
	      HIWORD_UPDATE(RK3576_SDAIN_MASK, RK3576_SDAIN_MASK) |
	      HIWORD_UPDATE(RK3576_HDMITX_GRANT_SEL, RK3576_HDMITX_GRANT_SEL) |
	      HIWORD_UPDATE(RK3576_I2S_SEL_MASK, RK3576_I2S_SEL_MASK);
	regmap_write(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON14, val);

	val = HIWORD_UPDATE(0, RK3576_HDMITX_HPD_INT_MSK);
	regmap_write(hdmi->regmap, RK3576_IOC_MISC_CON0, val);

	if (hdmi->sda_falling_delay_ns) {
		dw_hdmi_qp_rockchip_sda_delay_cal((void *)hdmi, &sda_dlyn, &sda_div);
		if (sda_dlyn) {
			val = HIWORD_UPDATE(sda_dlyn << 12, RK3576_GRF_OSDA_DLYN) |
			      HIWORD_UPDATE(sda_div << 1, RK3576_GRF_OSDA_DIV) |
			      HIWORD_UPDATE(1, RK3576_GRF_OSDA_DLY_EN);

			regmap_write(hdmi->vo0_regmap, RK3576_VO0_GRF_SOC_CON12, val);
		}
	}
}

static enum drm_connector_status
dw_hdmi_rk3538_read_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u32 val;
	int ret;

	regmap_read(hdmi->regmap, RK3538_HDMI_HPD_ST, &val);

	if (val & RK3576_HDMITX_LEVEL_INT) {
		hdmi->hpd_stat = true;
		ret = connector_status_connected;
	} else {
		hdmi->hpd_stat = false;
		ret = connector_status_disconnected;
	}

	return ret;
}

static enum drm_connector_status
dw_hdmi_rk3572_read_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u32 val;
	int ret;

	regmap_read(hdmi->regmap, RK3572_HDMITX_HPD_STATUS, &val);

	if (val & RK3572_HDMITX_LEVEL_INT) {
		hdmi->hpd_stat = true;
		ret = connector_status_connected;
	} else {
		hdmi->hpd_stat = false;
		ret = connector_status_disconnected;
	}

	return ret;
}

static enum drm_connector_status dw_hdmi_rk3576_read_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u32 val;
	int ret;

	regmap_read(hdmi->regmap, RK3576_IOC_HDMITX_HPD_STATUS, &val);

	if (val & RK3576_HDMITX_LEVEL_INT) {
		hdmi->hpd_stat = true;
		ret = connector_status_connected;
	} else {
		hdmi->hpd_stat = false;
		ret = connector_status_disconnected;
	}

	return ret;
}

static void dw_hdmi_rk3538_setup_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u32 val;

	val = HIWORD_UPDATE(RK3538_HDMITX_HPD_INT_CLR, RK3538_HDMITX_HPD_INT_CLR) |
	      HIWORD_UPDATE(0, RK3538_HDMITX_HPD_INT_MSK);

	regmap_write(hdmi->regmap, RK3538_HDMI_HPD_INT_CON, val);

	val = HIWORD_UPDATE(RK3572_SET_DLY_EN, RK3572_SET_DLY_EN_MASK) |
	      HIWORD_UPDATE(2, RK3572_SET_LNUM_MS_MASK);

	regmap_write(hdmi->regmap, RK3538_HDMI_HPD_CON, val);
}

static void dw_hdmi_rk3572_setup_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u32 val;

	val = HIWORD_UPDATE(RK3572_HDMITX_HPD_INT_CLR, RK3572_HDMITX_HPD_INT_CLR) |
	      HIWORD_UPDATE(0, RK3572_HDMITX_HPD_INT_MSK);

	regmap_write(hdmi->regmap, RK3572_SYS_GRF_CON1, val);

	val = HIWORD_UPDATE(RK3572_SET_DLY_EN, RK3572_SET_DLY_EN_MASK) |
	      HIWORD_UPDATE(2, RK3572_SET_LNUM_MS_MASK);

	regmap_write(hdmi->regmap, RK3572_SYS_GRF_CON2, val);
}

static void dw_hdmi_rk3576_setup_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u32 val;

	val = HIWORD_UPDATE(RK3576_HDMITX_HPD_INT_CLR, RK3576_HDMITX_HPD_INT_CLR) |
	      HIWORD_UPDATE(0, RK3576_HDMITX_HPD_INT_MSK);

	regmap_write(hdmi->regmap, RK3576_IOC_MISC_CON0, val);

	val = HIWORD_UPDATE(RK3576_SET_DLY_EN, RK3576_SET_DLY_EN_MASK) |
	      HIWORD_UPDATE(2, RK3576_SET_LNUM_MS_MASK);

	regmap_write(hdmi->regmap, RK3576_IOC_MISC_CON1, val);
}

static void rk3588_io_path_init(struct rockchip_dw_hdmi_qp *hdmi)
{
	u32 val;

	if (!hdmi->vo1_regmap)
		return;

	if (!hdmi->id) {
		val = HIWORD_UPDATE(RK3588_SCLIN_MASK, RK3588_SCLIN_MASK) |
		      HIWORD_UPDATE(RK3588_SDAIN_MASK, RK3588_SDAIN_MASK) |
		      HIWORD_UPDATE(RK3588_MODE_MASK, RK3588_MODE_MASK) |
		      HIWORD_UPDATE(RK3588_I2S_SEL_MASK, RK3588_I2S_SEL_MASK);
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON3, val);
		val = HIWORD_UPDATE(RK3588_SET_HPD_PATH_MASK, RK3588_SET_HPD_PATH_MASK);
		regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON7, val);
		val = HIWORD_UPDATE(RK3588_HDMI0_GRANT_SEL, RK3588_HDMI0_GRANT_SEL);
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON9, val);
		val = HIWORD_UPDATE(RK3588_HDMI0_HPD_INT_MSK, RK3588_HDMI0_HPD_INT_MSK);
		regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);
	} else {
		val = HIWORD_UPDATE(RK3588_SCLIN_MASK, RK3588_SCLIN_MASK) |
		      HIWORD_UPDATE(RK3588_SDAIN_MASK, RK3588_SDAIN_MASK) |
		      HIWORD_UPDATE(RK3588_MODE_MASK, RK3588_MODE_MASK) |
		      HIWORD_UPDATE(RK3588_I2S_SEL_MASK, RK3588_I2S_SEL_MASK);
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON6, val);
		val = HIWORD_UPDATE(RK3588_SET_HPD_PATH_MASK, RK3588_SET_HPD_PATH_MASK);
		regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON7, val);
		val = HIWORD_UPDATE(RK3588_HDMI1_GRANT_SEL, RK3588_HDMI1_GRANT_SEL);
		regmap_write(hdmi->vo1_regmap, RK3588_GRF_VO1_CON9, val);
		val = HIWORD_UPDATE(RK3588_HDMI1_HPD_INT_MSK, RK3588_HDMI1_HPD_INT_MSK);
		regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);
	}
}

static enum drm_connector_status dw_hdmi_rk3588_read_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	u32 val;
	int ret;
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	regmap_read(hdmi->regmap, RK3588_GRF_SOC_STATUS1, &val);

	if (!hdmi->id) {
		if (val & RK3588_HDMI0_LEVEL_INT) {
			hdmi->hpd_stat = true;
			ret = connector_status_connected;
		} else {
			hdmi->hpd_stat = false;
			ret = connector_status_disconnected;
		}
	} else {
		if (val & RK3588_HDMI1_LEVEL_INT) {
			hdmi->hpd_stat = true;
			ret = connector_status_connected;
		} else {
			hdmi->hpd_stat = false;
			ret = connector_status_disconnected;
		}
	}

	return ret;
}

static void dw_hdmi_rk3588_setup_hpd(struct dw_hdmi_qp *dw_hdmi, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;
	u32 val;

	if (!hdmi->id) {
		val = HIWORD_UPDATE(RK3588_SET_DLY_EN, RK3588_SET_DLY_EN_MASK) |
		      HIWORD_UPDATE(2, RK3588_SET_LNUM_MS_MASK);

		regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON12, val);

		val = HIWORD_UPDATE(RK3588_HDMI0_HPD_INT_CLR, RK3588_HDMI0_HPD_INT_CLR) |
		      HIWORD_UPDATE(0, RK3588_HDMI0_HPD_INT_MSK);
	} else {
		val = HIWORD_UPDATE(RK3588_SET_DLY_EN, RK3588_SET_DLY_EN_MASK) |
		      HIWORD_UPDATE(2, RK3588_SET_LNUM_MS_MASK);

		regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON13, val);

		val = HIWORD_UPDATE(RK3588_HDMI1_HPD_INT_CLR, RK3588_HDMI1_HPD_INT_CLR) |
		      HIWORD_UPDATE(0, RK3588_HDMI1_HPD_INT_MSK);
	}

	regmap_write(hdmi->regmap, RK3588_GRF_SOC_CON2, val);
}

static void dw_hdmi_rk3588_phy_set_mode(struct dw_hdmi_qp *dw_hdmi, void *data,
					u32 mode_mask, bool enable)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	if (!hdmi->phy)
		return;

	/* set phy frl mode */
	if (mode_mask & HDMI_MODE_FRL_MASK) {
		if (enable)
			phy_set_mode_ext(hdmi->phy, PHY_MODE_HDMI, PHY_HDMI_MODE_FRL);
		else
			phy_set_mode_ext(hdmi->phy, PHY_MODE_HDMI, PHY_HDMI_MODE_TMDS);
	}
}

static void dw_hdmi_qp_rockchip_phy_set_ffe(struct dw_hdmi_qp *dw_hdmi, void *data, u8 ffe)
{
	struct rockchip_dw_hdmi_qp *hdmi = (struct rockchip_dw_hdmi_qp *)data;

	if (!hdmi->phy)
		return;

	phy_set_mode_ext(hdmi->phy, 0, ffe);
}

static const struct dw_hdmi_qp_phy_ops rk3538_hdmi_phy_ops = {
	.init		= dw_hdmi_qp_rockchip_genphy_init,
	.disable	= dw_hdmi_qp_rockchip_phy_disable,
	.read_hpd	= dw_hdmi_rk3538_read_hpd,
	.setup_hpd	= dw_hdmi_rk3538_setup_hpd,
};

static const struct rockchip_hdmi_chip_ops rk3538_hdmi_chip_ops = {
	.set_color_format = rk3538_set_color_format,
	.get_grf_color_fmt = rk3538_get_grf_color_fmt,
	.io_path_init = rk3538_io_path_init,
	.hdmi_hardirq = rk3538_hdmi_hardirq,
	.hdmi_thread = rk3538_hdmi_thread,
	.set_hdcp2_enable = rk3538_set_hdcp2_enable,
};

struct rockchip_hdmi_chip_data rk3538_hdmi_chip_data = {
	.ddc_en_reg = RK3538_VO_GRF_HDMI_SWITCH,
	.ops = &rk3538_hdmi_chip_ops,
};

static const struct dw_hdmi_plat_data rk3538_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.phy_data = &rk3538_hdmi_chip_data,
	.qp_phy_ops = &rk3538_hdmi_phy_ops,
	.phy_name = "inno_dw_hdmi_phy2",
	.phy_force_vendor = true,
	.ycbcr_420_allowed = true,
	.dw_hdmi_qp_version = DW_HDMI_QP_V2,
	.use_drm_infoframe = true,
	.cec_wakeup_supported = true,
	.pr_supported = true,
};

static const struct dw_hdmi_qp_phy_ops rk3576_hdmi_phy_ops = {
	.init		= dw_hdmi_qp_rockchip_genphy_init,
	.disable	= dw_hdmi_qp_rockchip_phy_disable,
	.read_hpd	= dw_hdmi_rk3576_read_hpd,
	.setup_hpd	= dw_hdmi_rk3576_setup_hpd,
	.set_mode       = dw_hdmi_rk3588_phy_set_mode,
	.set_ffe        = dw_hdmi_qp_rockchip_phy_set_ffe,
};

static const struct rockchip_hdmi_chip_ops rk3576_hdmi_chip_ops = {
	.set_link_mode = rk3576_set_link_mode,
	.set_color_format = rk3576_set_color_format,
	.get_grf_color_fmt = rk3576_get_grf_color_fmt,
	.io_path_init = rk3576_io_path_init,
	.hdmi_hardirq = rk3576_hdmi_hardirq,
	.hdmi_thread = rk3576_hdmi_thread,
	.set_hdcp14_mem = rk3576_set_hdcp14_mem,
	.set_hdcp2_enable = rk3576_set_hdcp2_enable,
	.set_emp_bypass_enable = rk3576_set_emp_bypass_enable,
};

struct rockchip_hdmi_chip_data rk3576_hdmi_chip_data = {
	.ddc_en_reg = RK3576_VO0_GRF_SOC_CON14,
	.ops = &rk3576_hdmi_chip_ops,
};

static const struct dw_hdmi_plat_data rk3576_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.phy_data = &rk3576_hdmi_chip_data,
	.qp_phy_ops = &rk3576_hdmi_phy_ops,
	.phy_name = "samsung_hdptx_phy",
	.phy_force_vendor = true,
	.ycbcr_420_allowed = true,
	.dw_hdmi_qp_version = DW_HDMI_QP_V1,
	.use_drm_infoframe = true,
};

static const struct dw_hdmi_qp_phy_ops rk3572_hdmi_phy_ops = {
	.init		= dw_hdmi_qp_rockchip_genphy_init,
	.disable	= dw_hdmi_qp_rockchip_phy_disable,
	.read_hpd	= dw_hdmi_rk3572_read_hpd,
	.setup_hpd	= dw_hdmi_rk3572_setup_hpd,
	.set_mode       = dw_hdmi_rk3588_phy_set_mode,
	.set_ffe        = dw_hdmi_qp_rockchip_phy_set_ffe,
};

static const struct rockchip_hdmi_chip_ops rk3572_hdmi_chip_ops = {
	.set_link_mode = rk3572_set_link_mode,
	.set_color_format = rk3572_set_color_format,
	.get_grf_color_fmt = rk3572_get_grf_color_fmt,
	.io_path_init = rk3572_io_path_init,
	.hdmi_hardirq = rk3572_hdmi_hardirq,
	.hdmi_thread = rk3572_hdmi_thread,
	.set_hdcp14_mem = rk3572_set_hdcp14_mem,
	.set_hdcp2_enable = rk3572_set_hdcp2_enable,
	.set_emp_bypass_enable = rk3572_set_emp_bypass_enable,
};

struct rockchip_hdmi_chip_data rk3572_hdmi_chip_data = {
	.ddc_en_reg = RK3572_VO0_GRF_SOC_CON12,
	.ops = &rk3572_hdmi_chip_ops,
};

static const struct dw_hdmi_plat_data rk3572_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.phy_data = &rk3572_hdmi_chip_data,
	.qp_phy_ops = &rk3572_hdmi_phy_ops,
	.phy_name = "samsung_hdptx_phy",
	.phy_force_vendor = true,
	.ycbcr_420_allowed = true,
	.dw_hdmi_qp_version = DW_HDMI_QP_V1,
	.use_drm_infoframe = true,
	.pr_supported = true,
};

static const struct dw_hdmi_qp_phy_ops rk3588_hdmi_phy_ops = {
	.init		= dw_hdmi_qp_rockchip_genphy_init,
	.disable	= dw_hdmi_qp_rockchip_phy_disable,
	.read_hpd	= dw_hdmi_rk3588_read_hpd,
	.setup_hpd	= dw_hdmi_rk3588_setup_hpd,
	.set_mode       = dw_hdmi_rk3588_phy_set_mode,
	.set_ffe        = dw_hdmi_qp_rockchip_phy_set_ffe,
};

static const struct rockchip_hdmi_chip_ops rk3588_hdmi_chip_ops = {
	.set_link_mode = rk3588_set_link_mode,
	.set_color_format = rk3588_set_color_format,
	.get_grf_color_fmt = rk3588_get_grf_color_fmt,
	.io_path_init = rk3588_io_path_init,
	.hdmi_hardirq = rk3588_hdmi_hardirq,
	.hdmi_thread = rk3588_hdmi_thread,
	.set_hdcp14_mem = rk3588_set_hdcp14_mem,
	.set_hdcp2_enable = rk3588_set_hdcp2_enable,
};

struct rockchip_hdmi_chip_data rk3588_hdmi_chip_data = {
	.ddc_en_reg = RK3588_GRF_VO1_CON3,
	.split_mode = true,
	.ops = &rk3588_hdmi_chip_ops,
};

static const struct dw_hdmi_plat_data rk3588_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.phy_data = &rk3588_hdmi_chip_data,
	.qp_phy_ops = &rk3588_hdmi_phy_ops,
	.phy_name = "samsung_hdptx_phy",
	.phy_force_vendor = true,
	.ycbcr_420_allowed = true,
	.dw_hdmi_qp_version = DW_HDMI_QP_V1,
	.use_drm_infoframe = true,
};

static const struct of_device_id dw_hdmi_qp_rockchip_dt_ids[] = {
	{ .compatible = "rockchip,rk3538-dw-hdmi",
	  .data = &rk3538_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3572-dw-hdmi",
	  .data = &rk3572_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3576-dw-hdmi",
	  .data = &rk3576_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3588-dw-hdmi",
	  .data = &rk3588_hdmi_drv_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, dw_hdmi_qp_rockchip_dt_ids);

static int dw_hdmi_qp_rockchip_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct rockchip_dw_hdmi_qp *hdmi;
	struct dw_hdmi_plat_data *plat_data;
	struct rockchip_dw_hdmi_qp *secondary;
	int ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	hdmi = platform_get_drvdata(pdev);
	if (!hdmi)
		return -ENOMEM;

	plat_data = hdmi->plat_data;
	hdmi->drm_dev = drm;

	plat_data->phy_data = hdmi;
	plat_data->get_input_bus_format = dw_hdmi_rockchip_get_input_bus_format;
	plat_data->get_output_bus_format = dw_hdmi_rockchip_get_output_bus_format;
	plat_data->get_enc_in_encoding = dw_hdmi_rockchip_get_enc_in_encoding;
	plat_data->get_enc_out_encoding = dw_hdmi_rockchip_get_enc_out_encoding;
	plat_data->get_quant_range = dw_hdmi_rockchip_get_quant_range;
	plat_data->get_hdr_property = dw_hdmi_rockchip_get_hdr_property;
	plat_data->get_hdr_blob = dw_hdmi_rockchip_get_hdr_blob;
	plat_data->get_color_changed = dw_hdmi_rockchip_get_color_changed;
	plat_data->get_yuv422_format = dw_hdmi_rockchip_get_yuv422_format;
	plat_data->get_edid_hdmi21_info = dw_hdmi_rockchip_get_edid_hdmi21_info;
	plat_data->get_dovi_data = dw_hdmi_rockchip_get_dovi_data;
	plat_data->get_colorimetry = dw_hdmi_rockchip_get_colorimetry;
	plat_data->get_dovi_vsif = dw_hdmi_rockchip_get_vsif_data;
	plat_data->get_hdr10_plus_vsdb = dw_hdmi_rockchip_get_hdr10_plus_vsdb;
	plat_data->sda_delay_cal = dw_hdmi_qp_rockchip_sda_delay_cal;
	plat_data->get_link_cfg = dw_hdmi_rockchip_get_link_cfg;
	plat_data->set_hdcp2_enable = rockchip_set_hdcp2_enable;
	plat_data->set_hdcp_status = rockchip_set_hdcp_status;
	plat_data->set_grf_cfg = rockchip_set_grf_cfg;
	plat_data->convert_to_split_mode = drm_mode_convert_to_split_mode;
	plat_data->convert_to_origin_mode = drm_mode_convert_to_origin_mode;
	plat_data->dclk_set = dw_hdmi_dclk_set;
	plat_data->link_clk_set = dw_hdmi_link_clk_set;
	plat_data->get_vp_id = dw_hdmi_rockchip_get_vp_id;
	plat_data->update_color_format = dw_hdmi_rockchip_update_color_format;
	plat_data->set_prev_bus_format = dw_hdmi_rockchip_set_prev_bus_format;
	plat_data->set_hdcp14_mem = dw_hdmi_rockchip_set_hdcp14_mem;
	plat_data->get_force_timing = dw_hdmi_rockchip_get_force_timing;
	plat_data->get_refclk_rate = dw_hdmi_rockchip_get_refclk_rate;
	plat_data->force_frl_rate = dw_hdmi_rockchip_force_frl_rate;
	plat_data->get_mode_color_caps = dw_hdmi_rockchip_get_mode_color_caps;
	plat_data->get_mode_info = dw_hdmi_rockchip_get_mode_info;
	plat_data->crtc_pre_disable = dw_hdmi_rockchip_crtc_pre_disable;
	plat_data->crtc_post_enable = dw_hdmi_rockchip_crtc_post_enable;
	plat_data->set_cec_wakeup = dw_hdmi_rockchip_set_cec_wakeup;
	plat_data->get_hdrvivid_vsdb = dw_hdmi_rockchip_get_hdrvivid_vsdb;
	plat_data->wait_vblank = dw_hdmi_wait_vblank;
	plat_data->get_emp_status = dw_hdmi_rockchip_get_emp_status;
	plat_data->set_emp_bypass = dw_hdmi_rockchip_set_emp_bypass;
	plat_data->property_ops = &dw_hdmi_rockchip_property_ops;

	secondary = rockchip_hdmi_find_by_id(dev->driver, !hdmi->id);
	/* If don't enable hdmi0 and hdmi1, we don't enable split mode */
	if (hdmi->chip_data->split_mode && secondary) {

		/*
		 * hdmi can only attach bridge and init encoder/connector in the
		 * last bind hdmi in split mode, or hdmi->hdmi_qp will not be initialized
		 * and plat_data->left/right will be null pointer. we must check if split
		 * mode is on and determine the sequence of hdmi bind.
		 */
		if (device_property_read_bool(dev, "split-mode") ||
		    device_property_read_bool(dev, "rockchip,split-mode") ||
		    device_property_read_bool(secondary->dev, "split-mode") ||
		    device_property_read_bool(secondary->dev, "rockchip,split-mode")) {
			plat_data->split_mode = true;
			secondary->plat_data->split_mode = true;
			if (!secondary->plat_data->first_screen)
				plat_data->first_screen = true;
		}
	}

	if (device_property_read_bool(dev, "rockchip,dual-connector-split")) {
		plat_data->dual_connector_split = true;

		if (device_property_read_bool(dev, "rockchip,left-display"))
			plat_data->left_display = true;
	}

	if (!plat_data->first_screen) {
		encoder = &hdmi->encoder;
		encoder->possible_crtcs = rockchip_drm_of_find_possible_crtcs(drm, dev->of_node);
		/*
		 * If we failed to find the CRTC(s) which this encoder is
		 * supposed to be connected to, it's because the CRTC has
		 * not been registered yet.  Defer probing, and hope that
		 * the required CRTC is added later.
		 */
		if (encoder->possible_crtcs == 0)
			return -EPROBE_DEFER;

		drm_encoder_helper_add(encoder, &dw_hdmi_rockchip_encoder_helper_funcs);
		drm_encoder_init(drm, encoder, &dw_hdmi_rockchip_encoder_funcs,
				 DRM_MODE_ENCODER_TMDS, NULL);
	}

	if (!plat_data->max_tmdsclk)
		hdmi->max_tmdsclk = 594000;
	else
		hdmi->max_tmdsclk = plat_data->max_tmdsclk;

	hdmi->dw_hdmi_qp_version = plat_data->dw_hdmi_qp_version;
	hdmi->cec_wakeup_supported = plat_data->cec_wakeup_supported;

	ret = rockchip_hdmi_parse_dt(hdmi);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			DRM_DEV_ERROR(hdmi->dev, "Unable to parse OF data\n");
		return ret;
	}

	hdmi->chip_data->ops->io_path_init(hdmi);
	init_hdmi_work(hdmi);

	hdmi->hpd_irq = platform_get_irq(pdev, 4);
	if (hdmi->hpd_irq < 0) {
		ret = hdmi->hpd_irq;
		return ret;
	}

	ret = devm_request_threaded_irq(hdmi->dev, hdmi->hpd_irq,
					hdmi->chip_data->ops->hdmi_hardirq,
					hdmi->chip_data->ops->hdmi_thread,
					IRQF_SHARED, "dw-hdmi-qp-hpd", hdmi);
	if (ret)
		return ret;

	hdmi->phy = devm_phy_optional_get(dev, "hdmi");
	if (IS_ERR(hdmi->phy)) {
		hdmi->phy = devm_phy_optional_get(dev, "hdmi_phy");
		if (IS_ERR(hdmi->phy)) {
			ret = PTR_ERR(hdmi->phy);
			if (ret != -EPROBE_DEFER)
				DRM_DEV_ERROR(hdmi->dev, "failed to get phy\n");
			return ret;
		}
	}

	hdmi->hdmi_qp = dw_hdmi_qp_bind(pdev, &hdmi->encoder, plat_data);
	if (IS_ERR(hdmi->hdmi_qp)) {
		ret = PTR_ERR(hdmi->hdmi_qp);
		drm_encoder_cleanup(&hdmi->encoder);
		goto err_conn;
	}

	if (plat_data->bridge) {
		struct drm_connector *connector = NULL;
		struct list_head *connector_list =
			&plat_data->bridge->dev->mode_config.connector_list;

		list_for_each_entry(connector, connector_list, head)
			if (drm_connector_has_possible_encoder(connector, &hdmi->encoder))
				break;

		hdmi->sub_dev.connector = connector;
		hdmi->sub_dev.of_node = dev->of_node;
		rockchip_drm_register_sub_dev(&hdmi->sub_dev);
	} else if (plat_data->connector) {
		hdmi->sub_dev.connector = plat_data->connector;
		hdmi->sub_dev.loader_protect = dw_hdmi_rockchip_encoder_loader_protect;
		if (secondary && (device_property_read_bool(secondary->dev, "split-mode") ||
				  device_property_read_bool(secondary->dev, "rockchip,split-mode")))
			hdmi->sub_dev.of_node = secondary->dev->of_node;
		else
			hdmi->sub_dev.of_node = hdmi->dev->of_node;

		rockchip_drm_register_sub_dev(&hdmi->sub_dev);
	}

	if (plat_data->split_mode && secondary) {
		if (device_property_read_bool(dev, "split-mode") ||
		    device_property_read_bool(dev, "rockchip,split-mode")) {
			plat_data->right = secondary->hdmi_qp;
			secondary->plat_data->left = hdmi->hdmi_qp;
		} else {
			plat_data->left = secondary->hdmi_qp;
			secondary->plat_data->right = hdmi->hdmi_qp;
		}
	}

	return 0;

err_conn:
	if (plat_data->connector) {
		hdmi->sub_dev.connector = plat_data->connector;
		hdmi->sub_dev.of_node = dev->of_node;
		rockchip_drm_register_sub_dev(&hdmi->sub_dev);
	}

	return ret;
}

static void dw_hdmi_qp_rockchip_unbind(struct device *dev, struct device *master, void *data)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_get_drvdata(dev);

	cancel_delayed_work(&hdmi->hpd_work);
	cancel_work_sync(&hdmi->qms_vrr_work);
	flush_workqueue(hdmi->workqueue);
	destroy_workqueue(hdmi->workqueue);

	if (hdmi->sub_dev.connector)
		rockchip_drm_unregister_sub_dev(&hdmi->sub_dev);

	dw_hdmi_qp_unbind(hdmi->hdmi_qp);

	hdmi->drm_dev = NULL;
}

static const struct component_ops dw_hdmi_rockchip_ops = {
	.bind	= dw_hdmi_qp_rockchip_bind,
	.unbind	= dw_hdmi_qp_rockchip_unbind,
};

static int dw_hdmi_qp_rockchip_probe(struct platform_device *pdev)
{
	struct rockchip_dw_hdmi_qp *hdmi;
	const struct of_device_id *match;
	struct dw_hdmi_plat_data *plat_data;
	int id;

	hdmi = devm_kzalloc(&pdev->dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	id = of_alias_get_id(pdev->dev.of_node, "hdmi");
	if (id < 0)
		id = 0;

	hdmi->id = id;
	hdmi->dev = &pdev->dev;

	match = of_match_node(dw_hdmi_qp_rockchip_dt_ids, pdev->dev.of_node);
	plat_data = devm_kmemdup(&pdev->dev, match->data, sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	plat_data->id = hdmi->id;
	hdmi->plat_data = plat_data;
	hdmi->chip_data = plat_data->phy_data;

	platform_set_drvdata(pdev, hdmi);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	return component_add(&pdev->dev, &dw_hdmi_rockchip_ops);
}

static void dw_hdmi_qp_rockchip_shutdown(struct platform_device *pdev)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_get_drvdata(&pdev->dev);

	if (!hdmi || !hdmi->drm_dev)
		return;

	if (hdmi->hpd_irq)
		disable_irq(hdmi->hpd_irq);
	cancel_delayed_work(&hdmi->hpd_work);
	cancel_work_sync(&hdmi->qms_vrr_work);
	flush_workqueue(hdmi->workqueue);
	dw_hdmi_qp_suspend(hdmi->dev, hdmi->hdmi_qp);

	pm_runtime_put_sync(&pdev->dev);
}

static void dw_hdmi_qp_rockchip_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dw_hdmi_rockchip_ops);
	pm_runtime_disable(&pdev->dev);
}

static int __maybe_unused dw_hdmi_qp_rockchip_suspend(struct device *dev)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_get_drvdata(dev);

	if (hdmi->hpd_irq)
		disable_irq(hdmi->hpd_irq);
	dw_hdmi_qp_suspend(dev, hdmi->hdmi_qp);

	pm_runtime_put_sync(dev);

	return 0;
}

static int __maybe_unused dw_hdmi_qp_rockchip_resume(struct device *dev)
{
	struct rockchip_dw_hdmi_qp *hdmi = dev_get_drvdata(dev);

	hdmi->chip_data->ops->io_path_init(hdmi);
	dw_hdmi_qp_resume(dev, hdmi->hdmi_qp);
	if (hdmi->hpd_irq)
		enable_irq(hdmi->hpd_irq);
	drm_helper_hpd_irq_event(hdmi->drm_dev);

	pm_runtime_get_sync(dev);

	return 0;
}

static const struct dev_pm_ops dw_hdmi_qp_rockchip_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(dw_hdmi_qp_rockchip_suspend, dw_hdmi_qp_rockchip_resume)
};

struct platform_driver dw_hdmi_qp_rockchip_pltfm_driver = {
	.probe  = dw_hdmi_qp_rockchip_probe,
	.remove = dw_hdmi_qp_rockchip_remove,
	.shutdown = dw_hdmi_qp_rockchip_shutdown,
	.driver = {
		.name = "dw_hdmi_qp-rockchip",
		.pm = &dw_hdmi_qp_rockchip_pm,
		.of_match_table = dw_hdmi_qp_rockchip_dt_ids,
	},
};
