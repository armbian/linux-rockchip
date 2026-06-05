// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2014, Fuzhou Rockchip Electronics Co., Ltd
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

#define HIWORD_UPDATE(val, mask)	(val | (mask) << 16)

#define RK3228_GRF_SOC_CON2		0x0408
#define RK3228_HDMI_SDAIN_MSK		BIT(14)
#define RK3228_HDMI_SCLIN_MSK		BIT(13)
#define RK3228_GRF_SOC_CON6		0x0418
#define RK3228_HDMI_HPD_VSEL		BIT(6)
#define RK3228_HDMI_SDA_VSEL		BIT(5)
#define RK3228_HDMI_SCL_VSEL		BIT(4)

#define RK3288_GRF_SOC_CON6		0x025C
#define RK3288_HDMI_LCDC_SEL		BIT(4)
#define RK3288_GRF_SOC_CON16		0x03a8
#define RK3288_HDMI_LCDC0_YUV420	BIT(2)
#define RK3288_HDMI_LCDC1_YUV420	BIT(3)

#define RK3328_GRF_SOC_CON2		0x0408
#define RK3328_HDMI_SDAIN_MSK		BIT(11)
#define RK3328_HDMI_SCLIN_MSK		BIT(10)
#define RK3328_HDMI_HPD_IOE		BIT(2)
#define RK3328_GRF_SOC_CON3		0x040c
/* need to be unset if hdmi or i2c should control voltage */
#define RK3328_HDMI_SDA5V_GRF		BIT(15)
#define RK3328_HDMI_SCL5V_GRF		BIT(14)
#define RK3328_HDMI_HPD5V_GRF		BIT(13)
#define RK3328_HDMI_CEC5V_GRF		BIT(12)
#define RK3328_GRF_SOC_CON4		0x0410
#define RK3328_HDMI_HPD_SARADC		BIT(13)
#define RK3328_HDMI_CEC_5V		BIT(11)
#define RK3328_HDMI_SDA_5V		BIT(10)
#define RK3328_HDMI_SCL_5V		BIT(9)
#define RK3328_HDMI_HPD_5V		BIT(8)

#define RK3399_GRF_SOC_CON20		0x6250
#define RK3399_HDMI_LCDC_SEL		BIT(6)

#define RK3528_VO_GRF_HDMI_MASK		0x60014
#define RK3528_HDMI_SNKDET_SEL		BIT(6)
#define RK3528_HDMI_SNKDET		BIT(5)
#define RK3528_HDMI_CECIN_MSK		BIT(2)
#define RK3528_HDMI_SDAIN_MSK		BIT(1)
#define RK3528_HDMI_SCLIN_MSK		BIT(0)

#define RK3528PMU_GRF_SOC_CON6		0x70018
#define RK3528_HDMI_SDA5V_GRF		BIT(6)
#define RK3528_HDMI_SCL5V_GRF		BIT(5)
#define RK3528_HDMI_CEC5V_GRF		BIT(4)
#define RK3528_HDMI_HPD5V_GRF		BIT(3)

#define RK3528_GPIO_SWPORT_DR_L		0x0000
#define RK3528_GPIO0_A2_DR		BIT(2)

#define RK3568_GRF_VO_CON1		0x0364
#define RK3568_HDMI_SDAIN_MSK		BIT(15)
#define RK3568_HDMI_SCLIN_MSK		BIT(14)

#define HDMI14_MAX_RATE                 340000
#define HDMI20_MAX_RATE			600000

struct rockchip_hdmi;

struct rockchip_hdmi_chip_ops {
	void (*set_link_mode)(struct rockchip_hdmi *hdmi);
	void (*set_color_format)(struct rockchip_hdmi *hdmi, u64 bus_format, u32 depth);
	void (*get_grf_color_fmt)(struct rockchip_hdmi *hdmi, u32 *fmt, u32 *depth);
	void (*io_path_init)(struct rockchip_hdmi *hdmi);
	irqreturn_t (*hdmi_hardirq)(int irq, void *dev_id);
	irqreturn_t (*hdmi_thread)(int irq, void *dev_id);
	void (*set_hdcp14_mem)(struct rockchip_hdmi *hdmi, bool enable);
	void (*set_hdcp2_enable)(struct rockchip_hdmi *hdmi, bool enable);
	void (*set_emp_bypass_enable)(struct rockchip_hdmi *hdmi, bool enable);
};

/**
 * struct rockchip_hdmi_chip_data - splite the grf setting of kind of chips
 * @lcdsel_grf_reg: grf register offset of lcdc select
 * @ddc_en_reg: grf register offset of hdmi ddc enable
 * @lcdsel_big: reg value of selecting vop big for HDMI
 * @lcdsel_lit: reg value of selecting vop little for HDMI
 * @ops: hdmi grf config functions for different chips
 */
struct rockchip_hdmi_chip_data {
	int	lcdsel_grf_reg;
	int	ddc_en_reg;
	u32	lcdsel_big;
	u32	lcdsel_lit;
	const struct rockchip_hdmi_chip_ops *ops;
};

struct rockchip_hdmi {
	struct device *dev;
	struct regmap *regmap;
	void __iomem *gpio_base;
	struct drm_encoder encoder;
	struct drm_device *drm_dev;
	const struct rockchip_hdmi_chip_data *chip_data;
	struct dw_hdmi_plat_data *plat_data;
	struct clk *phyref_clk;
	struct clk *grf_clk;
	struct clk *hclk_vio;
	struct clk *hclk_vop;
	struct dw_hdmi *hdmi;
	struct regulator *avdd_0v9;
	struct regulator *avdd_1v8;

	struct phy *phy;

	u32 max_tmdsclk;
	bool unsupported_yuv_input;
	bool unsupported_deep_color;
	bool skip_check_420_mode;
	bool hpd_wake_en;
	u8 force_output;
	bool hpd_stat;

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
	struct drm_property *vsif_data;
	struct drm_property *hdmi_colorspace_caps;

	struct drm_property_blob *mode_color_caps_ptr;
	struct drm_property_blob *hdr_panel_blob_ptr;
	struct drm_property_blob *vsif_data_ptr;

	unsigned int colordepth;
	unsigned int colorimetry;
	unsigned int hdmi_quant_range;
	unsigned int phy_bus_width;
	enum rk_if_color_format hdmi_output;
	struct rockchip_drm_sub_dev sub_dev;

	u32 edid_colorimetry;
	u8 hdcp_status;
	u8 dovi_vsdb[DOVI_VSDB_LEN];
	struct gpio_desc *enable_gpio;

	struct delayed_work hpd_work;
	struct workqueue_struct *workqueue;
	struct gpio_desc *hpd_gpiod;
	struct pinctrl *p;
	struct pinctrl_state *idle_state;
	struct pinctrl_state *default_state;
	struct rockchip_drm_mode_color_caps *mode_color_caps;
	bool timing_force_output;
	struct drm_display_mode force_mode;
	struct rockchip_drm_hdmi21_data hdmi21_data;
	u32 force_bus_format;
	u32 sda_falling_delay_ns;
};

#define to_rockchip_hdmi(x)	container_of(x, struct rockchip_hdmi, x)

/*
 * There are some rates that would be ranged for better clock jitter at
 * Chrome OS tree, like 25.175Mhz would range to 25.170732Mhz. But due
 * to the clock is aglined to KHz in struct drm_display_mode, this would
 * bring some inaccurate error if we still run the compute_n math, so
 * let's just code an const table for it until we can actually get the
 * right clock rate.
 */
static const struct dw_hdmi_audio_tmds_n rockchip_werid_tmds_n_table[] = {
	/* 25176471 for 25.175 MHz = 428000000 / 17. */
	{ .tmds = 25177000, .n_32k = 4352, .n_44k1 = 14994, .n_48k = 6528, },
	/* 57290323 for 57.284 MHz */
	{ .tmds = 57291000, .n_32k = 3968, .n_44k1 = 4557, .n_48k = 5952, },
	/* 74437500 for 74.44 MHz = 297750000 / 4 */
	{ .tmds = 74438000, .n_32k = 8192, .n_44k1 = 18816, .n_48k = 4096, },
	/* 118666667 for 118.68 MHz */
	{ .tmds = 118667000, .n_32k = 4224, .n_44k1 = 5292, .n_48k = 6336, },
	/* 121714286 for 121.75 MHz */
	{ .tmds = 121715000, .n_32k = 4480, .n_44k1 = 6174, .n_48k = 6272, },
	/* 136800000 for 136.75 MHz */
	{ .tmds = 136800000, .n_32k = 4096, .n_44k1 = 5684, .n_48k = 6144, },
	/* End of table */
	{ .tmds = 0,         .n_32k = 0,    .n_44k1 = 0,    .n_48k = 0, },
};

static const struct dw_hdmi_mpll_config rockchip_mpll_cfg[] = {
	{
		30666000, {
			{ 0x00b3, 0x0000 },
			{ 0x2153, 0x0000 },
			{ 0x40f3, 0x0000 },
		},
	},  {
		36800000, {
			{ 0x00b3, 0x0000 },
			{ 0x2153, 0x0000 },
			{ 0x40a2, 0x0001 },
		},
	},  {
		46000000, {
			{ 0x00b3, 0x0000 },
			{ 0x2142, 0x0001 },
			{ 0x40a2, 0x0001 },
		},
	},  {
		61333000, {
			{ 0x0072, 0x0001 },
			{ 0x2142, 0x0001 },
			{ 0x40a2, 0x0001 },
		},
	},  {
		73600000, {
			{ 0x0072, 0x0001 },
			{ 0x2142, 0x0001 },
			{ 0x4061, 0x0002 },
		},
	},  {
		92000000, {
			{ 0x0072, 0x0001 },
			{ 0x2145, 0x0002 },
			{ 0x4061, 0x0002 },
		},
	},  {
		122666000, {
			{ 0x0051, 0x0002 },
			{ 0x2145, 0x0002 },
			{ 0x4061, 0x0002 },
		},
	},  {
		147200000, {
			{ 0x0051, 0x0002 },
			{ 0x2145, 0x0002 },
			{ 0x4064, 0x0003 },
		},
	},  {
		184000000, {
			{ 0x0051, 0x0002 },
			{ 0x214c, 0x0003 },
			{ 0x4064, 0x0003 },
		},
	},  {
		226666000, {
			{ 0x0040, 0x0003 },
			{ 0x214c, 0x0003 },
			{ 0x4064, 0x0003 },
		},
	},  {
		272000000, {
			{ 0x0040, 0x0003 },
			{ 0x214c, 0x0003 },
			{ 0x5a64, 0x0003 },
		},
	},  {
		340000000, {
			{ 0x0040, 0x0003 },
			{ 0x3b4c, 0x0003 },
			{ 0x5a64, 0x0003 },
		},
	},  {
		600000000, {
			{ 0x1a40, 0x0003 },
			{ 0x3b4c, 0x0003 },
			{ 0x5a64, 0x0003 },
		},
	},  {
		~0UL, {
			{ 0x0000, 0x0000 },
			{ 0x0000, 0x0000 },
			{ 0x0000, 0x0000 },
		},
	}
};

static const struct dw_hdmi_mpll_config rockchip_mpll_cfg_420[] = {
	{
		30666000, {
			{ 0x00b7, 0x0000 },
			{ 0x2157, 0x0000 },
			{ 0x40f7, 0x0000 },
		},
	},  {
		92000000, {
			{ 0x00b7, 0x0000 },
			{ 0x2143, 0x0001 },
			{ 0x40a3, 0x0001 },
		},
	},  {
		184000000, {
			{ 0x0073, 0x0001 },
			{ 0x2146, 0x0002 },
			{ 0x4062, 0x0002 },
		},
	},  {
		340000000, {
			{ 0x0052, 0x0003 },
			{ 0x214d, 0x0003 },
			{ 0x4065, 0x0003 },
		},
	},  {
		600000000, {
			{ 0x0041, 0x0003 },
			{ 0x3b4d, 0x0003 },
			{ 0x5a65, 0x0003 },
		},
	},  {
		~0UL, {
			{ 0x0000, 0x0000 },
			{ 0x0000, 0x0000 },
			{ 0x0000, 0x0000 },
		},
	}
};

static const struct dw_hdmi_mpll_config rockchip_rk3288w_mpll_cfg_420[] = {
	{
		30666000, {
			{ 0x00b7, 0x0000 },
			{ 0x2157, 0x0000 },
			{ 0x40f7, 0x0000 },
		},
	},  {
		92000000, {
			{ 0x00b7, 0x0000 },
			{ 0x2143, 0x0001 },
			{ 0x40a3, 0x0001 },
		},
	},  {
		184000000, {
			{ 0x0073, 0x0001 },
			{ 0x2146, 0x0002 },
			{ 0x4062, 0x0002 },
		},
	},  {
		340000000, {
			{ 0x0052, 0x0003 },
			{ 0x214d, 0x0003 },
			{ 0x4065, 0x0003 },
		},
	},  {
		600000000, {
			{ 0x0040, 0x0003 },
			{ 0x3b4c, 0x0003 },
			{ 0x5a65, 0x0003 },
		},
	},  {
		~0UL, {
			{ 0x0000, 0x0000 },
			{ 0x0000, 0x0000 },
			{ 0x0000, 0x0000 },
		},
	}
};

static const struct dw_hdmi_curr_ctrl rockchip_cur_ctr[] = {
	/*      pixelclk    bpp8    bpp10   bpp12 */
	{
		600000000, { 0x0000, 0x0000, 0x0000 },
	},  {
		~0UL,      { 0x0000, 0x0000, 0x0000},
	}
};

static struct dw_hdmi_phy_config rockchip_phy_config[] = {
	/*pixelclk   symbol   term   vlev*/
	{ 74250000,  0x8009, 0x0004, 0x0272},
	{ 165000000, 0x802b, 0x0004, 0x0209},
	{ 297000000, 0x8039, 0x0005, 0x028d},
	{ 594000000, 0x8039, 0x0000, 0x019d},
	{ ~0UL,	     0x0000, 0x0000, 0x0000},
	{ ~0UL,      0x0000, 0x0000, 0x0000},
};

/////////////////////////////////////////////////////////////////////////////////////////

static int rockchip_hdmi_update_phy_table(struct rockchip_hdmi *hdmi,
					  u32 *config,
					  int phy_table_size)
{
	int i;

	if (phy_table_size > ARRAY_SIZE(rockchip_phy_config)) {
		dev_err(hdmi->dev, "phy table array number is out of range\n");
		return -E2BIG;
	}

	for (i = 0; i < phy_table_size; i++) {
		if (config[i * 4] != 0)
			rockchip_phy_config[i].mpixelclock = (u64)config[i * 4];
		else
			rockchip_phy_config[i].mpixelclock = ~0UL;
		rockchip_phy_config[i].sym_ctr = (u16)config[i * 4 + 1];
		rockchip_phy_config[i].term = (u16)config[i * 4 + 2];
		rockchip_phy_config[i].vlev_ctr = (u16)config[i * 4 + 3];
	}

	return 0;
}

static irqreturn_t rockchip_hdmi_hpd_irq_handler(int irq, void *arg)
{
	u32 val;
	struct rockchip_hdmi *hdmi = arg;

	val = gpiod_get_value(hdmi->hpd_gpiod);
	if (val) {
		val = HIWORD_UPDATE(RK3528_HDMI_SNKDET, RK3528_HDMI_SNKDET);
		if (hdmi->hdmi && hdmi->hpd_wake_en && hdmi->hpd_gpiod)
			dw_hdmi_set_hpd_wake(hdmi->hdmi);
	} else {
		val = HIWORD_UPDATE(0, RK3528_HDMI_SNKDET);
	}
	regmap_write(hdmi->regmap, RK3528_VO_GRF_HDMI_MASK, val);

	return IRQ_HANDLED;
}

static void dw_hdmi_rk3528_gpio_hpd_init(struct rockchip_hdmi *hdmi)
{
	u32 val;

	if (hdmi->hpd_gpiod) {
		/* gpio0_a2's input enable is controlled by gpio output data bit */
		val = HIWORD_UPDATE(RK3528_GPIO0_A2_DR, RK3528_GPIO0_A2_DR);
		writel(val, hdmi->gpio_base + RK3528_GPIO_SWPORT_DR_L);

		val = HIWORD_UPDATE(RK3528_HDMI_SNKDET_SEL | RK3528_HDMI_SDAIN_MSK |
				    RK3528_HDMI_SCLIN_MSK,
				    RK3528_HDMI_SNKDET_SEL | RK3528_HDMI_SDAIN_MSK |
				    RK3528_HDMI_SCLIN_MSK);
	} else {
		val = HIWORD_UPDATE(RK3528_HDMI_SDAIN_MSK | RK3528_HDMI_SCLIN_MSK,
				    RK3528_HDMI_SDAIN_MSK | RK3528_HDMI_SCLIN_MSK);
	}

	regmap_write(hdmi->regmap, RK3528_VO_GRF_HDMI_MASK, val);

	val = gpiod_get_value(hdmi->hpd_gpiod);
	if (val) {
		val = HIWORD_UPDATE(RK3528_HDMI_SNKDET, RK3528_HDMI_SNKDET);
		if (hdmi->hdmi && hdmi->hpd_wake_en && hdmi->hpd_gpiod)
			dw_hdmi_set_hpd_wake(hdmi->hdmi);
	} else {
		val = HIWORD_UPDATE(0, RK3528_HDMI_SNKDET);
	}
	regmap_write(hdmi->regmap, RK3528_VO_GRF_HDMI_MASK, val);
}

static int rockchip_hdmi_parse_dt(struct rockchip_hdmi *hdmi)
{
	int ret, val, phy_table_size;
	u32 *phy_config;
	struct device_node *np = hdmi->dev->of_node;
	struct display_timing timing;
	struct videomode vm;
	struct clk *hdmi_clk;

	hdmi->regmap = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(hdmi->regmap)) {
		DRM_DEV_ERROR(hdmi->dev, "Unable to get rockchip,grf\n");
		return PTR_ERR(hdmi->regmap);
	}

	hdmi->phyref_clk = devm_clk_get(hdmi->dev, "vpll");
	if (PTR_ERR(hdmi->phyref_clk) == -ENOENT)
		hdmi->phyref_clk = devm_clk_get(hdmi->dev, "ref");

	if (PTR_ERR(hdmi->phyref_clk) == -ENOENT) {
		hdmi->phyref_clk = NULL;
	} else if (PTR_ERR(hdmi->phyref_clk) == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	} else if (IS_ERR(hdmi->phyref_clk)) {
		DRM_DEV_ERROR(hdmi->dev, "failed to get grf clock\n");
		return PTR_ERR(hdmi->phyref_clk);
	}

	hdmi->grf_clk = devm_clk_get(hdmi->dev, "grf");
	if (PTR_ERR(hdmi->grf_clk) == -ENOENT) {
		hdmi->grf_clk = NULL;
	} else if (PTR_ERR(hdmi->grf_clk) == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	} else if (IS_ERR(hdmi->grf_clk)) {
		DRM_DEV_ERROR(hdmi->dev, "failed to get grf clock\n");
		return PTR_ERR(hdmi->grf_clk);
	}

	hdmi->hclk_vio = devm_clk_get(hdmi->dev, "hclk_vio");
	if (PTR_ERR(hdmi->hclk_vio) == -ENOENT) {
		hdmi->hclk_vio = NULL;
	} else if (PTR_ERR(hdmi->hclk_vio) == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	} else if (IS_ERR(hdmi->hclk_vio)) {
		dev_err(hdmi->dev, "failed to get hclk_vio clock\n");
		return PTR_ERR(hdmi->hclk_vio);
	}

	hdmi->hclk_vop = devm_clk_get(hdmi->dev, "hclk");
	if (PTR_ERR(hdmi->hclk_vop) == -ENOENT) {
		hdmi->hclk_vop = NULL;
	} else if (PTR_ERR(hdmi->hclk_vop) == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	} else if (IS_ERR(hdmi->hclk_vop)) {
		dev_err(hdmi->dev, "failed to get hclk_vop clock\n");
		return PTR_ERR(hdmi->hclk_vop);
	}

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

	hdmi_clk = devm_clk_get_optional_enabled(hdmi->dev, "pclk");
	if (IS_ERR(hdmi_clk)) {
		dev_err_probe(hdmi->dev, PTR_ERR(hdmi_clk), "failed to get pclk clock\n");
		return PTR_ERR(hdmi_clk);
	}

	hdmi->enable_gpio = devm_gpiod_get_optional(hdmi->dev, "enable",
						    GPIOD_ASIS);
	if (IS_ERR(hdmi->enable_gpio)) {
		ret = PTR_ERR(hdmi->enable_gpio);
		dev_err(hdmi->dev, "failed to request enable GPIO: %d\n", ret);
		return ret;
	}

	hdmi->avdd_0v9 = devm_regulator_get_optional(hdmi->dev, "avdd-0v9");
	if (IS_ERR(hdmi->avdd_0v9)) {
		if (PTR_ERR(hdmi->avdd_0v9) != -ENODEV)
			return dev_err_probe(hdmi->dev, PTR_ERR(hdmi->avdd_0v9),
					     "failed to get regulator: avdd-0v9\n");
		hdmi->avdd_0v9 = NULL;
	}

	hdmi->avdd_1v8 = devm_regulator_get_optional(hdmi->dev, "avdd-1v8");
	if (IS_ERR(hdmi->avdd_1v8)) {
		if (PTR_ERR(hdmi->avdd_1v8) != -ENODEV)
			return dev_err_probe(hdmi->dev, PTR_ERR(hdmi->avdd_1v8),
					     "failed to get regulator: avdd-1v8\n");
		hdmi->avdd_1v8 = NULL;
	}

	hdmi->skip_check_420_mode =
		of_property_read_bool(np, "skip-check-420-mode");

	if (of_get_property(np, "rockchip,phy-table", &val)) {
		phy_config = kmalloc(val, GFP_KERNEL);
		if (!phy_config) {
			/* use default table when kmalloc failed. */
			dev_err(hdmi->dev, "kmalloc phy table failed\n");

			return -ENOMEM;
		}
		phy_table_size = val / 16;
		of_property_read_u32_array(np, "rockchip,phy-table",
					   phy_config, val / sizeof(u32));
		ret = rockchip_hdmi_update_phy_table(hdmi, phy_config,
						     phy_table_size);
		if (ret) {
			kfree(phy_config);
			return ret;
		}
		kfree(phy_config);
	} else {
		dev_dbg(hdmi->dev, "use default hdmi phy table\n");
	}

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

	hdmi->hpd_gpiod = devm_gpiod_get_optional(hdmi->dev, "hpd", GPIOD_IN);

	if (IS_ERR(hdmi->hpd_gpiod)) {
		dev_err(hdmi->dev, "error getting HDP GPIO: %ld\n",
			PTR_ERR(hdmi->hpd_gpiod));
		return PTR_ERR(hdmi->hpd_gpiod);
	}

	if (hdmi->hpd_gpiod) {
		struct resource *res;
		struct platform_device *pdev = to_platform_device(hdmi->dev);

		/* gpio interrupt reflects hpd status */
		hdmi->hpd_irq = gpiod_to_irq(hdmi->hpd_gpiod);
		if (hdmi->hpd_irq < 0)
			return -EINVAL;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!res) {
			DRM_DEV_ERROR(hdmi->dev, "failed to get gpio regs\n");
			return -EINVAL;
		}

		hdmi->gpio_base = devm_ioremap(hdmi->dev, res->start, resource_size(res));
		if (IS_ERR(hdmi->gpio_base)) {
			DRM_DEV_ERROR(hdmi->dev, "Unable to get gpio ioregmap\n");
			return PTR_ERR(hdmi->gpio_base);
		}

		dw_hdmi_rk3528_gpio_hpd_init(hdmi);
		ret = devm_request_threaded_irq(hdmi->dev, hdmi->hpd_irq, NULL,
						rockchip_hdmi_hpd_irq_handler,
						IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						"hdmi-hpd", hdmi);
		if (ret) {
			dev_err(hdmi->dev, "failed to request hpd IRQ: %d\n", ret);
			return ret;
		}

		hdmi->hpd_wake_en = device_property_read_bool(hdmi->dev, "hpd-wake-up");
		if (hdmi->hpd_wake_en)
			enable_irq_wake(hdmi->hpd_irq);
	}

	hdmi->p = devm_pinctrl_get(hdmi->dev);
	if (IS_ERR(hdmi->p)) {
		dev_err(hdmi->dev, "could not get pinctrl\n");
		return PTR_ERR(hdmi->p);
	}

	hdmi->idle_state = pinctrl_lookup_state(hdmi->p, "idle");
	if (IS_ERR(hdmi->idle_state)) {
		dev_dbg(hdmi->dev, "idle state is not defined\n");
		return 0;
	}

	hdmi->default_state = pinctrl_lookup_state(hdmi->p, "default");
	if (IS_ERR(hdmi->default_state)) {
		dev_err(hdmi->dev, "could not find default state\n");
		return PTR_ERR(hdmi->default_state);
	}

	return 0;
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
	enum drm_mode_status status = MODE_OK;
	struct drm_device *dev = connector->dev;
	struct rockchip_drm_private *priv = dev->dev_private;
	struct drm_crtc *crtc;
	struct rockchip_hdmi *hdmi;
	struct rockchip_crtc_state *s;

	if (!encoder) {
		const struct drm_connector_helper_funcs *funcs;

		funcs = connector->helper_private;
		if (funcs->atomic_best_encoder)
			encoder = funcs->atomic_best_encoder(connector,
							     connector->state->state);
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
		    max_tmds_clock < (mode->clock / 2) &&
		    is_hdmi2_mode(mode))
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

	/*
	 * Pixel clocks we support are always < 2GHz and so fit in an
	 * int.  We should make sure source rate does too so we don't get
	 * overflow when we multiply by 1000.
	 */
	if (mode->clock > INT_MAX / 1000)
		return MODE_BAD;

	if (hdmi->phy)
		phy_set_bus_width(hdmi->phy, 8);

	/*
	 * ensure all drm display mode can work, if someone want support more
	 * resolutions, please limit the possible_crtc, only connect to
	 * needed crtc.
	 */
	drm_for_each_crtc(crtc, connector->dev) {
		int pipe = drm_crtc_index(crtc);
		const struct rockchip_crtc_funcs *funcs =
						priv->crtc_funcs[pipe];

		if (!(encoder->possible_crtcs & drm_crtc_mask(crtc)))
			continue;
		if (!funcs || !funcs->mode_valid)
			continue;

		status = funcs->mode_valid(crtc, mode,
					   DRM_MODE_CONNECTOR_HDMIA);
		if (status != MODE_OK)
			return status;
	}

	return status;
}

static void dw_hdmi_rockchip_encoder_atomic_disable(struct drm_encoder *encoder,
						    struct drm_atomic_state *state)
{
	struct rockchip_hdmi *hdmi = to_rockchip_hdmi(encoder);
	struct drm_crtc *old_crtc, *new_crtc;
	struct rockchip_crtc_state *s;

	old_crtc = drm_atomic_get_old_crtc_for_encoder(state, encoder);
	new_crtc = drm_atomic_get_new_crtc_for_encoder(state, encoder);

	if (old_crtc && old_crtc != new_crtc) {
		s = to_rockchip_crtc_state(old_crtc->state);

		s->output_if &= ~VOP_OUTPUT_IF_HDMI0;
	}

	/*
	 * when plug out hdmi it will be switch cvbs and then phy bus width
	 * must be set as 8
	 */
	if (hdmi->phy)
		phy_set_bus_width(hdmi->phy, 8);
}

static void dw_hdmi_rockchip_encoder_enable(struct drm_encoder *encoder)
{
	struct rockchip_hdmi *hdmi = to_rockchip_hdmi(encoder);
	struct drm_crtc *crtc = encoder->crtc;
	u32 val;
	int mux;
	int ret;

	if (!crtc || !crtc->state) {
		dev_info(hdmi->dev, "%s old crtc state is null\n", __func__);
		return;
	}

	if (hdmi->phy)
		phy_set_bus_width(hdmi->phy, hdmi->phy_bus_width);

	clk_set_rate(hdmi->phyref_clk,
		     crtc->state->adjusted_mode.crtc_clock * 1000);

	if (hdmi->chip_data->lcdsel_grf_reg < 0)
		return;

	mux = drm_of_encoder_active_endpoint_id(hdmi->dev->of_node, encoder);
	if (mux)
		val = hdmi->chip_data->lcdsel_lit;
	else
		val = hdmi->chip_data->lcdsel_big;

	ret = clk_prepare_enable(hdmi->grf_clk);
	if (ret < 0) {
		DRM_DEV_ERROR(hdmi->dev, "failed to enable grfclk %d\n", ret);
		return;
	}

	ret = regmap_write(hdmi->regmap, hdmi->chip_data->lcdsel_grf_reg, val);
	if (ret != 0)
		DRM_DEV_ERROR(hdmi->dev, "Could not write to GRF: %d\n", ret);

	if (hdmi->chip_data->lcdsel_grf_reg == RK3288_GRF_SOC_CON6) {
		struct rockchip_crtc_state *s =
				to_rockchip_crtc_state(crtc->state);
		u32 mode_mask = mux ? RK3288_HDMI_LCDC1_YUV420 :
					RK3288_HDMI_LCDC0_YUV420;

		if (s->output_mode == ROCKCHIP_OUT_MODE_YUV420)
			val = HIWORD_UPDATE(mode_mask, mode_mask);
		else
			val = HIWORD_UPDATE(0, mode_mask);

		regmap_write(hdmi->regmap, RK3288_GRF_SOC_CON16, val);
	}

	clk_disable_unprepare(hdmi->grf_clk);
	DRM_DEV_DEBUG(hdmi->dev, "vop %s output to hdmi\n",
		      ret ? "LIT" : "BIG");
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
			       struct rockchip_hdmi *hdmi,
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
	bool hdr_no_bt2020 = false;
	u32 max_tmds_clock = info->max_tmds_clock;
	int output_eotf;

	drm_mode_copy(&mode, &crtc_state->mode);
	pixclock = mode.crtc_clock;

	vic = drm_match_cea_mode(&mode);

	sink_is_hdmi = dw_hdmi_get_output_whether_hdmi(hdmi->hdmi);

	*color_format = RK_IF_FORMAT_RGB;

	switch (hdmi->hdmi_output) {
	case RK_IF_FORMAT_YCBCR_HQ:
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*color_format = RK_IF_FORMAT_YCBCR444;
		else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = RK_IF_FORMAT_YCBCR422;
		else if (conn_state->connector->ycbcr_420_allowed &&
			 drm_mode_is_420(info, &mode) &&
			 (pixclock > HDMI14_MAX_RATE))
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
		hdr_metadata = (struct hdr_output_metadata *)
			conn_state->hdr_output_metadata->data;
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

	if (drm_mode_is_420_only(info, &mode))
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
			if (*color_format != RK_IF_FORMAT_RGB &&
			    !hdmi->unsupported_yuv_input)
				*bus_format = MEDIA_BUS_FMT_YUV10_1X30;
			else
				*bus_format = MEDIA_BUS_FMT_RGB101010_1X30;
		} else {
			if (*color_format != RK_IF_FORMAT_RGB &&
			    !hdmi->unsupported_yuv_input)
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
			hdmi->output_bus_format = MEDIA_BUS_FMT_UYVY12_1X24;
		else if (color_depth == 10)
			hdmi->output_bus_format = MEDIA_BUS_FMT_UYVY10_1X20;
		else
			hdmi->output_bus_format = MEDIA_BUS_FMT_UYVY8_1X16;
	} else {
		hdmi->output_bus_format = *bus_format;
	}
}

static bool
dw_hdmi_rockchip_check_color(struct drm_connector_state *conn_state,
			     struct rockchip_hdmi *hdmi)
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
dw_hdmi_rockchip_encoder_atomic_check(struct drm_encoder *encoder,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct rockchip_hdmi *hdmi = to_rockchip_hdmi(encoder);
	unsigned int colorformat, bus_width;
	struct drm_display_mode mode = {};
	unsigned int output_mode;
	unsigned long bus_format;

	drm_mode_copy(&mode, &crtc_state->adjusted_mode);

	dw_hdmi_rockchip_select_output(conn_state, crtc_state, hdmi,
				       &colorformat,
				       &output_mode, &bus_format, &bus_width,
				       &hdmi->enc_out_encoding, &s->eotf);

	s->bus_format = bus_format;

	hdmi->phy_bus_width = bus_width;

	if (hdmi->phy)
		phy_set_bus_width(hdmi->phy, bus_width);

	s->output_type = DRM_MODE_CONNECTOR_HDMIA;
	s->tv_state = &conn_state->tv;

	s->output_if |= VOP_OUTPUT_IF_HDMI0;

	s->output_mode = output_mode;
	hdmi->bus_format = s->bus_format;

	s->dsc_enable = 0;

	s->color_encoding = hdmi->colorimetry;

	if (colorformat == RK_IF_FORMAT_RGB)
		s->color_range = hdmi->hdmi_quant_range == HDMI_QUANTIZATION_RANGE_LIMITED ?
					DRM_COLOR_YCBCR_LIMITED_RANGE : DRM_COLOR_YCBCR_FULL_RANGE;
	else
		s->color_range = hdmi->hdmi_quant_range == HDMI_QUANTIZATION_RANGE_FULL ?
					DRM_COLOR_YCBCR_FULL_RANGE : DRM_COLOR_YCBCR_LIMITED_RANGE;

	return 0;
}


static unsigned long
dw_hdmi_rockchip_get_input_bus_format(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	return hdmi->bus_format;
}

static unsigned long
dw_hdmi_rockchip_get_output_bus_format(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	return hdmi->output_bus_format;
}

static unsigned long
dw_hdmi_rockchip_get_enc_in_encoding(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	return hdmi->enc_out_encoding;
}

static unsigned long
dw_hdmi_rockchip_get_enc_out_encoding(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	return hdmi->enc_out_encoding;
}

static unsigned long
dw_hdmi_rockchip_get_quant_range(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	return hdmi->hdmi_quant_range;
}

static struct drm_property *
dw_hdmi_rockchip_get_hdr_property(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	return hdmi->hdr_panel_metadata_property;
}

static struct drm_property_blob *
dw_hdmi_rockchip_get_hdr_blob(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	return hdmi->hdr_panel_blob_ptr;
}

static void dw_hdmi_rockchip_update_color_format(struct drm_connector_state *conn_state,
						 void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	dw_hdmi_rockchip_check_color(conn_state, hdmi);
}

static bool
dw_hdmi_rockchip_get_color_changed(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;
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


static int dw_hdmi_rockchip_get_colorimetry(void *data, const struct edid *edid, int ext_block_num)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	return rockchip_drm_parse_colorimetry_data_block(&hdmi->edid_colorimetry, edid,
							 ext_block_num);
}

static void dw_hdmi_rockchip_get_vsif_data(void *data, u32 *buf)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;
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

static int dw_hdmi_dclk_set(void *data, bool enable, int vp_id)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;
	char clk_name[16];
	struct clk *dclk;
	int ret;

	snprintf(clk_name, sizeof(clk_name), "dclk_vp%d", vp_id);

	dclk = devm_clk_get_optional(hdmi->dev, clk_name);
	if (IS_ERR(dclk)) {
		DRM_DEV_ERROR(hdmi->dev, "failed to get %s\n", clk_name);
		return PTR_ERR(dclk);
	} else if (!dclk) {
		return 0;
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

static void dw_hdmi_rockchip_set_prev_bus_format(void *data, unsigned long bus_format)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	hdmi->prev_bus_format = bus_format;
}

static void dw_hdmi_rockchip_set_ddc_io(void *data, bool enable)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	if (!hdmi->p || !hdmi->idle_state || !hdmi->default_state)
		return;

	if (!enable) {
		if (pinctrl_select_state(hdmi->p, hdmi->idle_state))
			dev_err(hdmi->dev, "could not select idle state\n");
	} else {
		if (pinctrl_select_state(hdmi->p, hdmi->default_state))
			dev_err(hdmi->dev, "could not select default state\n");
	}
}

static struct drm_display_mode *dw_hdmi_rockchip_get_force_timing(void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	if (!hdmi->timing_force_output)
		return NULL;

	return &hdmi->force_mode;
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
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;
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

static void dw_hdmi_rockchip_crtc_post_enable(void *data, struct drm_crtc *crtc)
{
	if (!crtc)
		return;

	rockchip_drm_crtc_output_post_enable(crtc, VOP_OUTPUT_IF_HDMI0);
}

static void dw_hdmi_rockchip_crtc_pre_disable(void *data, struct drm_crtc *crtc)
{
	if (!crtc)
		return;

	rockchip_drm_crtc_output_pre_disable(crtc, VOP_OUTPUT_IF_HDMI0);
}

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

static void
dw_hdmi_rockchip_attach_properties(struct drm_connector *connector, unsigned int color,
				   unsigned int colorimetry, int version, void *data, bool allm_en)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;
	struct drm_property *prop;
	struct rockchip_drm_private *private = connector->dev->dev_private;
	int ret;

	rockchip_drm_parse_bus_format(color, &hdmi->hdmi_output, &hdmi->colordepth);

	hdmi->bus_format = color;
	hdmi->prev_bus_format = color;
	hdmi->enc_out_encoding = colorimetry;

	if (hdmi->hdmi_output == RK_IF_FORMAT_YCBCR422) {
		if (hdmi->colordepth == 12)
			hdmi->output_bus_format = MEDIA_BUS_FMT_UYVY12_1X24;
		else if (hdmi->colordepth == 10)
			hdmi->output_bus_format = MEDIA_BUS_FMT_UYVY10_1X20;
		else
			hdmi->output_bus_format = MEDIA_BUS_FMT_UYVY8_1X16;
	} else {
		hdmi->output_bus_format = hdmi->bus_format;
	}

	/* RK3368 does not support deep color mode */
	if (!hdmi->color_depth_property && !hdmi->unsupported_deep_color) {
		prop = drm_property_create_enum(connector->dev, 0,
						RK_IF_PROP_COLOR_DEPTH,
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

	prop = drm_property_create_range(connector->dev, 0,
					 RK_IF_PROP_COLOR_DEPTH_CAPS,
					 0, 0xff);
	if (prop) {
		hdmi->colordepth_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_range(connector->dev, 0,
					 RK_IF_PROP_COLOR_FORMAT_CAPS,
					 0, 0xf);
	if (prop) {
		hdmi->outputmode_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create(connector->dev,
				   DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "HDR_PANEL_METADATA", 0);
	if (prop) {
		hdmi->hdr_panel_metadata_property = prop;
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

	prop = drm_property_create_enum(connector->dev, 0,
					"output_type_capacity",
					output_type_cap_list,
					ARRAY_SIZE(output_type_cap_list));
	if (prop) {
		hdmi->output_type_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_enum(connector->dev, 0,
					"quant_range",
					quant_range_enum_list,
					ARRAY_SIZE(quant_range_enum_list));
	if (prop) {
		hdmi->quant_range = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	if (!drm_mode_create_hdmi_colorspace_property(connector))
		drm_object_attach_property(&connector->base,
					   connector->colorspace_property, 0);
	drm_object_attach_property(&connector->base, private->connector_id_prop, 0);

	ret = drm_connector_attach_content_protection_property(connector, true);
	if (ret) {
		dev_err(hdmi->dev, "failed to attach content protection: %d\n", ret);
		return;
	}

	prop = drm_property_create_range(connector->dev, 0, RK_IF_PROP_ENCRYPTED,
					 RK_IF_HDCP_ENCRYPTED_NONE, RK_IF_HDCP_ENCRYPTED_LEVEL2);
	if (!prop) {
		dev_err(hdmi->dev, "create hdcp encrypted prop for hdmi failed\n");
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

static void
dw_hdmi_rockchip_destroy_properties(struct drm_connector *connector,
				    void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	if (hdmi->color_depth_property) {
		drm_property_destroy(connector->dev,
				     hdmi->color_depth_property);
		hdmi->color_depth_property = NULL;
	}

	if (hdmi->hdmi_output_property) {
		drm_property_destroy(connector->dev,
				     hdmi->hdmi_output_property);
		hdmi->hdmi_output_property = NULL;
	}

	if (hdmi->colordepth_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->colordepth_capacity);
		hdmi->colordepth_capacity = NULL;
	}

	if (hdmi->outputmode_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->outputmode_capacity);
		hdmi->outputmode_capacity = NULL;
	}

	if (hdmi->quant_range) {
		drm_property_destroy(connector->dev,
				     hdmi->quant_range);
		hdmi->quant_range = NULL;
	}

	if (hdmi->hdr_panel_metadata_property) {
		drm_property_destroy(connector->dev,
				     hdmi->hdr_panel_metadata_property);
		hdmi->hdr_panel_metadata_property = NULL;
	}

	if (hdmi->output_hdmi_dvi) {
		drm_property_destroy(connector->dev,
				     hdmi->output_hdmi_dvi);
		hdmi->output_hdmi_dvi = NULL;
	}

	if (hdmi->output_type_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->output_type_capacity);
		hdmi->output_type_capacity = NULL;
	}

	if (hdmi->allm_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->allm_capacity);
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

	if (hdmi->vsif_data) {
		drm_property_destroy(connector->dev, hdmi->vsif_data);
		hdmi->vsif_data = NULL;
	}

	if (hdmi->hdmi_colorspace_caps) {
		drm_property_destroy(connector->dev, hdmi->hdmi_colorspace_caps);
		hdmi->hdmi_colorspace_caps = NULL;
	}
}

static int
dw_hdmi_rockchip_set_property(struct drm_connector *connector,
			      struct drm_connector_state *state,
			      struct drm_property *property,
			      u64 val,
			      void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;
	struct drm_mode_config *config = &connector->dev->mode_config;

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
			dw_hdmi_set_quant_range(hdmi->hdmi);
		return 0;
	} else if (property == config->hdr_output_metadata_property) {
		return 0;
	} else if (property == hdmi->output_hdmi_dvi) {
		if (hdmi->force_output != val)
			hdmi->color_changed++;
		hdmi->force_output = val;
		dw_hdmi_set_output_type(hdmi->hdmi, val);
		return 0;
	} else if (property == hdmi->colordepth_capacity) {
		return 0;
	} else if (property == hdmi->outputmode_capacity) {
		return 0;
	} else if (property == hdmi->output_type_capacity) {
		return 0;
	} else if (property == hdmi->allm_capacity) {
		return 0;
	} else if (property == hdmi->hdcp_state_property) {
		return 0;
	} else if (property == hdmi->mode_color_capacity) {
		return 0;
	} else if (property == hdmi->vsif_data) {
		int ret;
		bool replaced = false;

		ret = rockchip_drm_atomic_replace_property_blob_from_id(connector->dev,
									&hdmi->vsif_data_ptr,
									val, -1, -1, &replaced);
		return ret;
	} else if (property == hdmi->hdmi_colorspace_caps) {
		return 0;
	}

	DRM_ERROR("Unknown property [PROP:%d:%s]\n",
		  property->base.id, property->name);

	return -EINVAL;
}

static int
dw_hdmi_rockchip_get_property(struct drm_connector *connector,
			      const struct drm_connector_state *state,
			      struct drm_property *property,
			      u64 *val,
			      void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;
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
		/* RK3368 only support 8bit */
		if (hdmi->unsupported_deep_color)
			return 0;
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
		*val = state->hdr_output_metadata ?
			state->hdr_output_metadata->base.id : 0;
		return 0;
	} else if (property == hdmi->output_hdmi_dvi) {
		*val = hdmi->force_output;
		return 0;
	} else if (property == hdmi->output_type_capacity) {
		*val = dw_hdmi_get_output_type_cap(hdmi->hdmi);
		return 0;
	} else if (property == hdmi->allm_capacity) {
		*val = hdmi->hdmi21_data.allm_supported;
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
	} else if (property == hdmi->vsif_data) {
		*val = (hdmi->vsif_data_ptr) ? hdmi->vsif_data_ptr->base.id : 0;
		return 0;
	} else if (property == hdmi->hdmi_colorspace_caps) {
		*val = hdmi->edid_colorimetry;
		return 0;
	}

	DRM_ERROR("Unknown property [PROP:%d:%s]\n",
		  property->base.id, property->name);

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
	struct rockchip_hdmi *hdmi = to_rockchip_hdmi(encoder);
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

	clk_set_rate(hdmi->phyref_clk, adj->crtc_clock * 1000);
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
	struct rockchip_hdmi *hdmi = container_of(encoder, struct rockchip_hdmi, encoder);

	dw_hdmi_register_debugfs(hdmi->hdmi);
	return 0;
}

static const struct drm_encoder_funcs dw_hdmi_rockchip_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
	.late_register = dw_hdmi_encoder_late_register,
};

static void
dw_hdmi_rockchip_genphy_disable(struct dw_hdmi *dw_hdmi, void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	while (hdmi->phy->power_count > 0)
		phy_power_off(hdmi->phy);
}

static int
dw_hdmi_rockchip_genphy_init(struct dw_hdmi *dw_hdmi, void *data,
			     const struct drm_display_info *display,
			     const struct drm_display_mode *mode)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	dw_hdmi_rockchip_genphy_disable(dw_hdmi, data);
	dw_hdmi_set_high_tmds_clock_ratio(dw_hdmi, display);
	return phy_power_on(hdmi->phy);
}

static void dw_hdmi_rk3228_setup_hpd(struct dw_hdmi *dw_hdmi, void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	dw_hdmi_phy_setup_hpd(dw_hdmi, data);

	regmap_write(hdmi->regmap,
		RK3228_GRF_SOC_CON6,
		HIWORD_UPDATE(RK3228_HDMI_HPD_VSEL | RK3228_HDMI_SDA_VSEL |
			      RK3228_HDMI_SCL_VSEL,
			      RK3228_HDMI_HPD_VSEL | RK3228_HDMI_SDA_VSEL |
			      RK3228_HDMI_SCL_VSEL));

	regmap_write(hdmi->regmap,
		RK3228_GRF_SOC_CON2,
		HIWORD_UPDATE(RK3228_HDMI_SDAIN_MSK | RK3228_HDMI_SCLIN_MSK,
			      RK3228_HDMI_SDAIN_MSK | RK3228_HDMI_SCLIN_MSK));
}

static enum drm_connector_status
dw_hdmi_rk3328_read_hpd(struct dw_hdmi *dw_hdmi, void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;
	enum drm_connector_status status;

	status = dw_hdmi_phy_read_hpd(dw_hdmi, data);

	if (status == connector_status_connected)
		regmap_write(hdmi->regmap,
			RK3328_GRF_SOC_CON4,
			HIWORD_UPDATE(RK3328_HDMI_SDA_5V | RK3328_HDMI_SCL_5V,
				      RK3328_HDMI_SDA_5V | RK3328_HDMI_SCL_5V));
	else
		regmap_write(hdmi->regmap,
			RK3328_GRF_SOC_CON4,
			HIWORD_UPDATE(0, RK3328_HDMI_SDA_5V |
					 RK3328_HDMI_SCL_5V));
	return status;
}

static void dw_hdmi_rk3328_setup_hpd(struct dw_hdmi *dw_hdmi, void *data)
{
	struct rockchip_hdmi *hdmi = (struct rockchip_hdmi *)data;

	dw_hdmi_phy_setup_hpd(dw_hdmi, data);

	/* Enable and map pins to 3V grf-controlled io-voltage */
	regmap_write(hdmi->regmap,
		RK3328_GRF_SOC_CON4,
		HIWORD_UPDATE(0, RK3328_HDMI_HPD_SARADC | RK3328_HDMI_CEC_5V |
				 RK3328_HDMI_SDA_5V | RK3328_HDMI_SCL_5V |
				 RK3328_HDMI_HPD_5V));
	regmap_write(hdmi->regmap,
		RK3328_GRF_SOC_CON3,
		HIWORD_UPDATE(0, RK3328_HDMI_SDA5V_GRF | RK3328_HDMI_SCL5V_GRF |
				 RK3328_HDMI_HPD5V_GRF |
				 RK3328_HDMI_CEC5V_GRF));
	regmap_write(hdmi->regmap,
		RK3328_GRF_SOC_CON2,
		HIWORD_UPDATE(RK3328_HDMI_SDAIN_MSK | RK3328_HDMI_SCLIN_MSK,
			      RK3328_HDMI_SDAIN_MSK | RK3328_HDMI_SCLIN_MSK |
			      RK3328_HDMI_HPD_IOE));

	dw_hdmi_rk3328_read_hpd(dw_hdmi, data);
}

static const struct dw_hdmi_phy_ops rk3228_hdmi_phy_ops = {
	.init		= dw_hdmi_rockchip_genphy_init,
	.disable	= dw_hdmi_rockchip_genphy_disable,
	.read_hpd	= dw_hdmi_phy_read_hpd,
	.update_hpd	= dw_hdmi_phy_update_hpd,
	.setup_hpd	= dw_hdmi_rk3228_setup_hpd,
};

static struct rockchip_hdmi_chip_data rk3228_chip_data = {
	.lcdsel_grf_reg = -1,
};

static const struct dw_hdmi_plat_data rk3228_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.mpll_cfg = rockchip_mpll_cfg,
	.cur_ctr = rockchip_cur_ctr,
	.phy_config = rockchip_phy_config,
	.phy_data = &rk3228_chip_data,
	.phy_ops = &rk3228_hdmi_phy_ops,
	.phy_name = "inno_dw_hdmi_phy2",
	.phy_force_vendor = true,
	.max_tmdsclk = 371250,
	.ycbcr_420_allowed = true,
};

static struct rockchip_hdmi_chip_data rk3288_chip_data = {
	.lcdsel_grf_reg = RK3288_GRF_SOC_CON6,
	.lcdsel_big = HIWORD_UPDATE(0, RK3288_HDMI_LCDC_SEL),
	.lcdsel_lit = HIWORD_UPDATE(RK3288_HDMI_LCDC_SEL, RK3288_HDMI_LCDC_SEL),
};

static const struct dw_hdmi_plat_data rk3288_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.mpll_cfg   = rockchip_mpll_cfg,
	.mpll_cfg_420 = rockchip_rk3288w_mpll_cfg_420,
	.cur_ctr    = rockchip_cur_ctr,
	.phy_config = rockchip_phy_config,
	.phy_data = &rk3288_chip_data,
	.tmds_n_table = rockchip_werid_tmds_n_table,
	.unsupported_yuv_input = true,
	.ycbcr_420_allowed = true,
};

static const struct dw_hdmi_phy_ops rk3328_hdmi_phy_ops = {
	.init		= dw_hdmi_rockchip_genphy_init,
	.disable	= dw_hdmi_rockchip_genphy_disable,
	.read_hpd	= dw_hdmi_rk3328_read_hpd,
	.update_hpd	= dw_hdmi_phy_update_hpd,
	.setup_hpd	= dw_hdmi_rk3328_setup_hpd,
};

static enum drm_connector_status
dw_hdmi_rk3528_read_hpd(struct dw_hdmi *dw_hdmi, void *data)
{
	return dw_hdmi_phy_read_hpd(dw_hdmi, data);
}

static const struct dw_hdmi_phy_ops rk3528_hdmi_phy_ops = {
	.init		= dw_hdmi_rockchip_genphy_init,
	.disable	= dw_hdmi_rockchip_genphy_disable,
	.read_hpd	= dw_hdmi_rk3528_read_hpd,
	.update_hpd	= dw_hdmi_phy_update_hpd,
	.setup_hpd	= dw_hdmi_phy_setup_hpd,
};

static struct rockchip_hdmi_chip_data rk3328_chip_data = {
	.lcdsel_grf_reg = -1,
};

static const struct dw_hdmi_plat_data rk3328_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.mpll_cfg = rockchip_mpll_cfg,
	.cur_ctr = rockchip_cur_ctr,
	.phy_config = rockchip_phy_config,
	.phy_data = &rk3328_chip_data,
	.phy_ops = &rk3328_hdmi_phy_ops,
	.phy_name = "inno_dw_hdmi_phy2",
	.phy_force_vendor = true,
	.use_drm_infoframe = true,
	.max_tmdsclk = 371250,
	.ycbcr_420_allowed = true,
};

static struct rockchip_hdmi_chip_data rk3368_chip_data = {
	.lcdsel_grf_reg = -1,
};

static const struct dw_hdmi_plat_data rk3368_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.mpll_cfg   = rockchip_mpll_cfg,
	.mpll_cfg_420 = rockchip_mpll_cfg_420,
	.cur_ctr    = rockchip_cur_ctr,
	.phy_config = rockchip_phy_config,
	.phy_data = &rk3368_chip_data,
	.unsupported_deep_color = true,
	.max_tmdsclk = 340000,
	.ycbcr_420_allowed = true,
};

static struct rockchip_hdmi_chip_data rk3399_chip_data = {
	.lcdsel_grf_reg = RK3399_GRF_SOC_CON20,
	.lcdsel_big = HIWORD_UPDATE(0, RK3399_HDMI_LCDC_SEL),
	.lcdsel_lit = HIWORD_UPDATE(RK3399_HDMI_LCDC_SEL, RK3399_HDMI_LCDC_SEL),
};

static const struct dw_hdmi_plat_data rk3399_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.mpll_cfg   = rockchip_mpll_cfg,
	.mpll_cfg_420 = rockchip_mpll_cfg_420,
	.cur_ctr    = rockchip_cur_ctr,
	.phy_config = rockchip_phy_config,
	.phy_data = &rk3399_chip_data,
	.use_drm_infoframe = true,
	.ycbcr_420_allowed = true,
};

static struct rockchip_hdmi_chip_data rk3528_chip_data = {
	.lcdsel_grf_reg = -1,
};

static const struct dw_hdmi_plat_data rk3528_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.mpll_cfg = rockchip_mpll_cfg,
	.cur_ctr = rockchip_cur_ctr,
	.phy_config = rockchip_phy_config,
	.phy_data = &rk3528_chip_data,
	.phy_ops = &rk3528_hdmi_phy_ops,
	.phy_name = "inno_dw_hdmi_phy2",
	.phy_force_vendor = true,
	.use_drm_infoframe = true,
	.ycbcr_420_allowed = true,
};

static struct rockchip_hdmi_chip_data rk3568_chip_data = {
	.lcdsel_grf_reg = -1,
	.ddc_en_reg = RK3568_GRF_VO_CON1,
};

static const struct dw_hdmi_plat_data rk3568_hdmi_drv_data = {
	.mode_valid = dw_hdmi_rockchip_mode_valid,
	.mpll_cfg   = rockchip_mpll_cfg,
	.mpll_cfg_420 = rockchip_mpll_cfg_420,
	.cur_ctr    = rockchip_cur_ctr,
	.phy_config = rockchip_phy_config,
	.phy_data = &rk3568_chip_data,
	.ycbcr_420_allowed = true,
	.use_drm_infoframe = true,
};

static const struct of_device_id dw_hdmi_rockchip_dt_ids[] = {
	{ .compatible = "rockchip,rk3228-dw-hdmi",
	  .data = &rk3228_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3288-dw-hdmi",
	  .data = &rk3288_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3328-dw-hdmi",
	  .data = &rk3328_hdmi_drv_data
	},
	{
	 .compatible = "rockchip,rk3368-dw-hdmi",
	 .data = &rk3368_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3399-dw-hdmi",
	  .data = &rk3399_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3528-dw-hdmi",
	  .data = &rk3528_hdmi_drv_data
	},
	{ .compatible = "rockchip,rk3568-dw-hdmi",
	  .data = &rk3568_hdmi_drv_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, dw_hdmi_rockchip_dt_ids);

static int dw_hdmi_rockchip_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct rockchip_hdmi *hdmi;
	struct dw_hdmi_plat_data *plat_data;
	int ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	hdmi = platform_get_drvdata(pdev);
	if (!hdmi)
		return -ENOMEM;

	plat_data = hdmi->plat_data;
	hdmi->drm_dev = drm;

	plat_data->phy_data = hdmi;
	plat_data->get_input_bus_format =
		dw_hdmi_rockchip_get_input_bus_format;
	plat_data->get_output_bus_format =
		dw_hdmi_rockchip_get_output_bus_format;
	plat_data->get_enc_in_encoding =
		dw_hdmi_rockchip_get_enc_in_encoding;
	plat_data->get_enc_out_encoding =
		dw_hdmi_rockchip_get_enc_out_encoding;
	plat_data->get_quant_range =
		dw_hdmi_rockchip_get_quant_range;
	plat_data->get_hdr_property =
		dw_hdmi_rockchip_get_hdr_property;
	plat_data->get_hdr_blob =
		dw_hdmi_rockchip_get_hdr_blob;
	plat_data->get_color_changed =
		dw_hdmi_rockchip_get_color_changed;
	plat_data->get_yuv422_format =
		dw_hdmi_rockchip_get_yuv422_format;
	plat_data->get_colorimetry =
		dw_hdmi_rockchip_get_colorimetry;
	plat_data->get_dovi_vsif =
		dw_hdmi_rockchip_get_vsif_data;
	plat_data->dclk_set = dw_hdmi_dclk_set;
	plat_data->update_color_format =
		dw_hdmi_rockchip_update_color_format;
	plat_data->set_prev_bus_format =
		dw_hdmi_rockchip_set_prev_bus_format;
	plat_data->set_ddc_io =
		dw_hdmi_rockchip_set_ddc_io;
	plat_data->get_force_timing =
		dw_hdmi_rockchip_get_force_timing;
	plat_data->get_mode_color_caps =
		dw_hdmi_rockchip_get_mode_color_caps;
	plat_data->crtc_pre_disable =
		dw_hdmi_rockchip_crtc_pre_disable;
	plat_data->crtc_post_enable =
		dw_hdmi_rockchip_crtc_post_enable;
	plat_data->property_ops = &dw_hdmi_rockchip_property_ops;

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
			 DRM_MODE_ENCODER_TMDS,
			 NULL);

	if (!plat_data->max_tmdsclk)
		hdmi->max_tmdsclk = 594000;
	else
		hdmi->max_tmdsclk = plat_data->max_tmdsclk;

	hdmi->unsupported_yuv_input = plat_data->unsupported_yuv_input;
	hdmi->unsupported_deep_color = plat_data->unsupported_deep_color;

	ret = rockchip_hdmi_parse_dt(hdmi);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			DRM_DEV_ERROR(hdmi->dev, "Unable to parse OF data\n");
		return ret;
	}

	if (hdmi->avdd_0v9) {
		ret = regulator_enable(hdmi->avdd_0v9);
		if (ret) {
			DRM_DEV_ERROR(hdmi->dev, "failed to enable avdd0v9: %d\n", ret);
			return ret;
		}
	}

	if (hdmi->avdd_1v8) {
		ret = regulator_enable(hdmi->avdd_1v8);
		if (ret) {
			DRM_DEV_ERROR(hdmi->dev, "failed to enable avdd1v8: %d\n", ret);
			return ret;
		}
	}

	if (hdmi->chip_data->ddc_en_reg == RK3568_GRF_VO_CON1) {
		regmap_write(hdmi->regmap, RK3568_GRF_VO_CON1,
			     HIWORD_UPDATE(RK3568_HDMI_SDAIN_MSK |
					   RK3568_HDMI_SCLIN_MSK,
					   RK3568_HDMI_SDAIN_MSK |
					   RK3568_HDMI_SCLIN_MSK));
	}

	ret = clk_prepare_enable(hdmi->phyref_clk);
	if (ret) {
		DRM_DEV_ERROR(hdmi->dev, "Failed to enable HDMI reference clock: %d\n",
			      ret);
		return ret;
	}

	if (hdmi->chip_data == &rk3568_chip_data) {
		regmap_write(hdmi->regmap, RK3568_GRF_VO_CON1,
			     HIWORD_UPDATE(RK3568_HDMI_SDAIN_MSK |
					   RK3568_HDMI_SCLIN_MSK,
					   RK3568_HDMI_SDAIN_MSK |
					   RK3568_HDMI_SCLIN_MSK));
	}

	ret = clk_prepare_enable(hdmi->hclk_vio);
	if (ret) {
		dev_err(hdmi->dev, "Failed to enable HDMI hclk_vio: %d\n",
			ret);
		goto err_phyref_clk;
	}

	ret = clk_prepare_enable(hdmi->hclk_vop);
	if (ret) {
		dev_err(hdmi->dev, "Failed to enable HDMI hclk_vop: %d\n",
			ret);
		goto err_hclk_vio;
	}

	hdmi->phy = devm_phy_optional_get(dev, "hdmi");
	if (IS_ERR(hdmi->phy)) {
		hdmi->phy = devm_phy_optional_get(dev, "hdmi_phy");
		if (IS_ERR(hdmi->phy)) {
			ret = PTR_ERR(hdmi->phy);
			if (ret != -EPROBE_DEFER)
				DRM_DEV_ERROR(hdmi->dev, "failed to get phy\n");
			goto err_hclk_vop;
		}
	}

	hdmi->hdmi = dw_hdmi_bind(pdev, &hdmi->encoder, plat_data);

	/*
	 * If dw_hdmi_bind() fails we'll never call dw_hdmi_unbind(),
	 * which would have called the encoder cleanup.  Do it manually.
	 */
	if (IS_ERR(hdmi->hdmi)) {
		ret = PTR_ERR(hdmi->hdmi);
		drm_encoder_cleanup(&hdmi->encoder);
		goto err_hclk_vop;
	}

	if (plat_data->connector) {
		hdmi->sub_dev.connector = plat_data->connector;
		hdmi->sub_dev.of_node = dev->of_node;
		rockchip_drm_register_sub_dev(&hdmi->sub_dev);
	}

	return 0;

err_hclk_vop:
	clk_disable_unprepare(hdmi->hclk_vop);
err_hclk_vio:
	clk_disable_unprepare(hdmi->hclk_vio);
err_phyref_clk:
	clk_disable_unprepare(hdmi->phyref_clk);

	return ret;
}

static void dw_hdmi_rockchip_unbind(struct device *dev, struct device *master,
				    void *data)
{
	struct rockchip_hdmi *hdmi = dev_get_drvdata(dev);

	if (hdmi->sub_dev.connector)
		rockchip_drm_unregister_sub_dev(&hdmi->sub_dev);

	dw_hdmi_unbind(hdmi->hdmi);
	clk_disable_unprepare(hdmi->phyref_clk);
	clk_disable_unprepare(hdmi->hclk_vio);
	clk_disable_unprepare(hdmi->hclk_vop);

	if (hdmi->avdd_1v8)
		regulator_disable(hdmi->avdd_1v8);
	if (hdmi->avdd_0v9)
		regulator_disable(hdmi->avdd_0v9);

	hdmi->drm_dev = NULL;
}

static const struct component_ops dw_hdmi_rockchip_ops = {
	.bind	= dw_hdmi_rockchip_bind,
	.unbind	= dw_hdmi_rockchip_unbind,
};

static int dw_hdmi_rockchip_probe(struct platform_device *pdev)
{
	struct rockchip_hdmi *hdmi;
	const struct of_device_id *match;
	struct dw_hdmi_plat_data *plat_data;

	hdmi = devm_kzalloc(&pdev->dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	hdmi->dev = &pdev->dev;

	match = of_match_node(dw_hdmi_rockchip_dt_ids, pdev->dev.of_node);
	plat_data = devm_kmemdup(&pdev->dev, match->data,
				 sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	hdmi->plat_data = plat_data;
	hdmi->chip_data = plat_data->phy_data;

	platform_set_drvdata(pdev, hdmi);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	return component_add(&pdev->dev, &dw_hdmi_rockchip_ops);
}

static void dw_hdmi_rockchip_shutdown(struct platform_device *pdev)
{
	struct rockchip_hdmi *hdmi = dev_get_drvdata(&pdev->dev);

	if (!hdmi || !hdmi->drm_dev)
		return;

	if (hdmi->hpd_gpiod) {
		disable_irq(hdmi->hpd_irq);
		if (hdmi->hpd_wake_en)
			disable_irq_wake(hdmi->hpd_irq);
	}
	dw_hdmi_suspend(hdmi->hdmi);

	pm_runtime_put_sync(&pdev->dev);
}

static int dw_hdmi_rockchip_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dw_hdmi_rockchip_ops);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static int __maybe_unused dw_hdmi_rockchip_suspend(struct device *dev)
{
	struct rockchip_hdmi *hdmi = dev_get_drvdata(dev);

	if (hdmi->hpd_gpiod)
		disable_irq(hdmi->hpd_irq);
	dw_hdmi_suspend(hdmi->hdmi);

	pm_runtime_put_sync(dev);

	return 0;
}

static int __maybe_unused dw_hdmi_rockchip_resume(struct device *dev)
{
	struct rockchip_hdmi *hdmi = dev_get_drvdata(dev);

	if (hdmi->hpd_gpiod) {
		dw_hdmi_rk3528_gpio_hpd_init(hdmi);
		enable_irq(hdmi->hpd_irq);
	}
	dw_hdmi_resume(hdmi->hdmi);

	pm_runtime_get_sync(dev);

	return 0;
}

static const struct dev_pm_ops dw_hdmi_rockchip_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(dw_hdmi_rockchip_suspend,
				dw_hdmi_rockchip_resume)
};

struct platform_driver dw_hdmi_rockchip_pltfm_driver = {
	.probe  = dw_hdmi_rockchip_probe,
	.remove = dw_hdmi_rockchip_remove,
	.shutdown = dw_hdmi_rockchip_shutdown,
	.driver = {
		.name = "dwhdmi-rockchip",
		.pm = &dw_hdmi_rockchip_pm,
		.of_match_table = dw_hdmi_rockchip_dt_ids,
	},
};
