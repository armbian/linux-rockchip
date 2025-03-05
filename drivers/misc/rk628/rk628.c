// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Wyon Bi <bivvy.bi@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/backlight.h>
#include <linux/pm_runtime.h>
#include <video/videomode.h>
#include <linux/debugfs.h>

#include "rk628.h"
#include "rk628_cru.h"
#include "rk628_combrxphy.h"
#include "rk628_post_process.h"
#include "rk628_hdmirx.h"
#include "rk628_combtxphy.h"
#include "rk628_dsi.h"
#include "rk628_rgb.h"
#include "rk628_lvds.h"
#include "rk628_gvi.h"
#include "rk628_csi.h"
#include "rk628_hdmitx.h"
#include "rk628_efuse.h"
#include "rk628_config.h"
#include "rk628_pwm.h"

static const struct regmap_range rk628_cru_readable_ranges[] = {
	regmap_reg_range(CRU_CPLL_CON0, CRU_CPLL_CON4),
	regmap_reg_range(CRU_GPLL_CON0, CRU_GPLL_CON4),
	regmap_reg_range(CRU_APLL_CON0, CRU_APLL_CON4),
	regmap_reg_range(CRU_MODE_CON00, CRU_MODE_CON00),
	regmap_reg_range(CRU_CLKSEL_CON00, CRU_CLKSEL_CON21),
	regmap_reg_range(CRU_GATE_CON00, CRU_GATE_CON05),
	regmap_reg_range(CRU_SOFTRST_CON00, CRU_SOFTRST_CON04),
};

static const struct regmap_access_table rk628_cru_readable_table = {
	.yes_ranges     = rk628_cru_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_cru_readable_ranges),
};

static const struct regmap_range rk628_combrxphy_readable_ranges[] = {
	regmap_reg_range(COMBRX_REG(0x6600), COMBRX_REG(0x665b)),
	regmap_reg_range(COMBRX_REG(0x66a0), COMBRX_REG(0x66db)),
	regmap_reg_range(COMBRX_REG(0x66f0), COMBRX_REG(0x66ff)),
	regmap_reg_range(COMBRX_REG(0x6700), COMBRX_REG(0x6790)),
};

static const struct regmap_access_table rk628_combrxphy_readable_table = {
	.yes_ranges     = rk628_combrxphy_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_combrxphy_readable_ranges),
};

static const struct regmap_range rk628_hdmirx_readable_ranges[] = {
	regmap_reg_range(HDMI_RX_HDMI_SETUP_CTRL, HDMI_RX_HDMI_SETUP_CTRL),
	regmap_reg_range(HDMI_RX_HDMI_PCB_CTRL, HDMI_RX_HDMI_PCB_CTRL),
	regmap_reg_range(HDMI_RX_HDMI_MODE_RECOVER, HDMI_RX_HDMI_ERROR_PROTECT),
	regmap_reg_range(HDMI_RX_HDMI_SYNC_CTRL, HDMI_RX_HDMI_CKM_RESULT),
	regmap_reg_range(HDMI_RX_HDMI_RESMPL_CTRL, HDMI_RX_HDMI_RESMPL_CTRL),
	regmap_reg_range(HDMI_RX_HDMI_DCM_CTRL, HDMI_RX_HDMI_DCM_CTRL),
	regmap_reg_range(HDMI_RX_HDMI_VM_CFG_CH2, HDMI_RX_HDMI_STS),
	regmap_reg_range(HDMI_RX_HDCP_CTRL, HDMI_RX_HDCP_SETTINGS),
	regmap_reg_range(HDMI_RX_HDCP_KIDX, HDMI_RX_HDCP_KIDX),
	regmap_reg_range(HDMI_RX_HDCP_DBG, HDMI_RX_HDCP_AN0),
	regmap_reg_range(HDMI_RX_HDCP_STS, HDMI_RX_HDCP_STS),
	regmap_reg_range(HDMI_RX_MD_HCTRL1, HDMI_RX_MD_HACT_PX),
	regmap_reg_range(HDMI_RX_MD_VCTRL, HDMI_RX_MD_VSC),
	regmap_reg_range(HDMI_RX_MD_VOL, HDMI_RX_MD_VTL),
	regmap_reg_range(HDMI_RX_MD_IL_POL, HDMI_RX_MD_STS),
	regmap_reg_range(HDMI_RX_AUD_CTRL, HDMI_RX_AUD_CTRL),
	regmap_reg_range(HDMI_RX_AUD_PLL_CTRL, HDMI_RX_AUD_PLL_CTRL),
	regmap_reg_range(HDMI_RX_AUD_CLK_CTRL, HDMI_RX_AUD_CLK_CTRL),
	regmap_reg_range(HDMI_RX_AUD_FIFO_CTRL, HDMI_RX_AUD_FIFO_TH),
	regmap_reg_range(HDMI_RX_AUD_CHEXTR_CTRL, HDMI_RX_AUD_PAO_CTRL),
	regmap_reg_range(HDMI_RX_AUD_FIFO_STS, HDMI_RX_AUD_FIFO_STS),
	regmap_reg_range(HDMI_RX_AUDPLL_GEN_CTS, HDMI_RX_AUDPLL_GEN_N),
	regmap_reg_range(HDMI_RX_I2CM_PHYG3_DATAI, HDMI_RX_I2CM_PHYG3_DATAI),
	regmap_reg_range(HDMI_RX_PDEC_CTRL, HDMI_RX_PDEC_CTRL),
	regmap_reg_range(HDMI_RX_PDEC_AUDIODET_CTRL, HDMI_RX_PDEC_AUDIODET_CTRL),
	regmap_reg_range(HDMI_RX_PDEC_ERR_FILTER, HDMI_RX_PDEC_ASP_CTRL),
	regmap_reg_range(HDMI_RX_PDEC_STS, HDMI_RX_PDEC_STS),
	regmap_reg_range(HDMI_RX_PDEC_GCP_AVMUTE, HDMI_RX_PDEC_GCP_AVMUTE),
	regmap_reg_range(HDMI_RX_PDEC_ACR_CTS, HDMI_RX_PDEC_ACR_N),
	regmap_reg_range(HDMI_RX_PDEC_AIF_CTRL, HDMI_RX_PDEC_AIF_PB0),
	regmap_reg_range(HDMI_RX_PDEC_AVI_PB, HDMI_RX_PDEC_AVI_PB),
	regmap_reg_range(HDMI_RX_HDMI20_CONTROL, HDMI_RX_CHLOCK_CONFIG),
	regmap_reg_range(HDMI_RX_SCDC_REGS0, HDMI_RX_SCDC_REGS2),
	regmap_reg_range(HDMI_RX_SCDC_WRDATA0, HDMI_RX_SCDC_WRDATA0),
	regmap_reg_range(HDMI_RX_PDEC_ISTS, HDMI_RX_PDEC_IEN),
	regmap_reg_range(HDMI_RX_AUD_FIFO_ISTS, HDMI_RX_AUD_FIFO_IEN),
	regmap_reg_range(HDMI_RX_MD_ISTS, HDMI_RX_MD_IEN),
	regmap_reg_range(HDMI_RX_HDMI_ISTS, HDMI_RX_HDMI_IEN),
	regmap_reg_range(HDMI_RX_DMI_DISABLE_IF, HDMI_RX_DMI_DISABLE_IF),
};

static const struct regmap_access_table rk628_hdmirx_readable_table = {
	.yes_ranges     = rk628_hdmirx_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_hdmirx_readable_ranges),
};

static const struct regmap_range rk628_key_readable_ranges[] = {
	regmap_reg_range(EDID_BASE, EDID_BASE + 0x400),
};

static const struct regmap_access_table rk628_key_readable_table = {
	.yes_ranges     = rk628_key_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_key_readable_ranges),
};

static const struct regmap_range rk628_efuse_readable_ranges[] = {
	regmap_reg_range(EFUSE_BASE, EFUSE_BASE + EFUSE_REVISION),
};

static const struct regmap_access_table rk628_efuse_readable_table = {
	.yes_ranges     = rk628_efuse_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_efuse_readable_ranges),
};

static const struct regmap_range rk628_combtxphy_readable_ranges[] = {
	regmap_reg_range(COMBTXPHY_BASE, COMBTXPHY_CON10),
};

static const struct regmap_access_table rk628_combtxphy_readable_table = {
	.yes_ranges     = rk628_combtxphy_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_combtxphy_readable_ranges),
};

static const struct regmap_range rk628_dsi0_readable_ranges[] = {
	regmap_reg_range(DSI0_BASE, DSI0_BASE + DSI_MAX_REGISTER),
};

static const struct regmap_access_table rk628_dsi0_readable_table = {
	.yes_ranges     = rk628_dsi0_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_dsi0_readable_ranges),
};

static const struct regmap_range rk628_dsi1_readable_ranges[] = {
	regmap_reg_range(DSI1_BASE, DSI1_BASE + DSI_MAX_REGISTER),
};

static const struct regmap_access_table rk628_dsi1_readable_table = {
	.yes_ranges     = rk628_dsi1_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_dsi1_readable_ranges),
};

static const struct regmap_range rk628_gvi_readable_ranges[] = {
	regmap_reg_range(GVI_BASE, GVI_COLOR_BAR_VTIMING1),
};

static const struct regmap_access_table rk628_gvi_readable_table = {
	.yes_ranges     = rk628_gvi_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_gvi_readable_ranges),
};

static const struct regmap_range rk628_csi_readable_ranges[] = {
	regmap_reg_range(CSITX_CONFIG_DONE, CSITX_CSITX_VERSION),
	regmap_reg_range(CSITX_SYS_CTRL0_IMD, CSITX_TIMING_HPW_PADDING_NUM),
	regmap_reg_range(CSITX_VOP_PATH_CTRL, CSITX_VOP_PATH_CTRL),
	regmap_reg_range(CSITX_VOP_PATH_PKT_CTRL, CSITX_VOP_PATH_PKT_CTRL),
	regmap_reg_range(CSITX_CSITX_STATUS0, CSITX_LPDT_DATA_IMD),
	regmap_reg_range(CSITX_DPHY_CTRL, CSITX_DPHY_CTRL),
};

static const struct regmap_access_table rk628_csi_readable_table = {
	.yes_ranges     = rk628_csi_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_csi_readable_ranges),
};

static const struct regmap_range rk628_hdmi_volatile_reg_ranges[] = {
	regmap_reg_range(HDMI_SYS_CTRL, HDMI_MAX_REG),
};

static const struct regmap_access_table rk628_hdmi_volatile_regs = {
	.yes_ranges = rk628_hdmi_volatile_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(rk628_hdmi_volatile_reg_ranges),
};

static const struct regmap_range rk628_gpio0_readable_ranges[] = {
	regmap_reg_range(RK628_GPIO0_BASE, RK628_GPIO0_BASE + GPIO_VER_ID),
};

static const struct regmap_access_table rk628_gpio0_readable_table = {
	.yes_ranges     = rk628_gpio0_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_gpio0_readable_ranges),
};

static const struct regmap_range rk628_gpio1_readable_ranges[] = {
	regmap_reg_range(RK628_GPIO1_BASE, RK628_GPIO1_BASE + GPIO_VER_ID),
};

static const struct regmap_access_table rk628_gpio1_readable_table = {
	.yes_ranges     = rk628_gpio1_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_gpio1_readable_ranges),
};

static const struct regmap_range rk628_gpio2_readable_ranges[] = {
	regmap_reg_range(RK628_GPIO2_BASE, RK628_GPIO2_BASE + GPIO_VER_ID),
};

static const struct regmap_access_table rk628_gpio2_readable_table = {
	.yes_ranges     = rk628_gpio2_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_gpio2_readable_ranges),
};

static const struct regmap_range rk628_gpio3_readable_ranges[] = {
	regmap_reg_range(RK628_GPIO3_BASE, RK628_GPIO3_BASE + GPIO_VER_ID),
};

static const struct regmap_access_table rk628_gpio3_readable_table = {
	.yes_ranges     = rk628_gpio3_readable_ranges,
	.n_yes_ranges   = ARRAY_SIZE(rk628_gpio3_readable_ranges),
};

static const struct regmap_config rk628_regmap_config[RK628_DEV_MAX] = {
	[RK628_DEV_GRF] = {
		.name = "grf",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = GRF_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
	},
	[RK628_DEV_CRU] = {
		.name = "cru",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = CRU_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_cru_readable_table,
	},
	[RK628_DEV_COMBRXPHY] = {
		.name = "combrxphy",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = COMBRX_REG(0x6790),
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_combrxphy_readable_table,
	},
	[RK628_DEV_HDMIRX] = {
		.name = "hdmirx",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = HDMI_RX_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_hdmirx_readable_table,
	},
	[RK628_DEV_ADAPTER] = {
		.name = "adapter",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = KEY_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_key_readable_table,
	},
	[RK628_DEV_EFUSE] = {
		.name = "efuse",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = EFUSE_BASE + EFUSE_REVISION,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_efuse_readable_table,
	},
	[RK628_DEV_COMBTXPHY] = {
		.name = "combtxphy",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = COMBTXPHY_CON10,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_combtxphy_readable_table,
	},
	[RK628_DEV_DSI0] = {
		.name = "dsi0",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = DSI0_BASE + DSI_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_dsi0_readable_table,
	},
	[RK628_DEV_DSI1] = {
		.name = "dsi1",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = DSI1_BASE + DSI_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_dsi1_readable_table,
	},
	[RK628_DEV_GVI] = {
		.name = "gvi",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = GVI_COLOR_BAR_VTIMING1,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_gvi_readable_table,
	},
	[RK628_DEV_CSI] = {
		.name = "csi",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = CSI_MAX_REGISTER,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_csi_readable_table,
	},
	[RK628_DEV_HDMITX] = {
		.name = "hdmi",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = HDMI_MAX_REG,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_hdmi_volatile_regs,
	},
	[RK628_DEV_GPIO0] = {
		.name = "gpio0",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = RK628_GPIO0_BASE + GPIO_VER_ID,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_gpio0_readable_table,
	},
	[RK628_DEV_GPIO1] = {
		.name = "gpio1",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = RK628_GPIO1_BASE + GPIO_VER_ID,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_gpio1_readable_table,
	},
	[RK628_DEV_GPIO2] = {
		.name = "gpio2",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = RK628_GPIO2_BASE + GPIO_VER_ID,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_gpio2_readable_table,
	},
	[RK628_DEV_GPIO3] = {
		.name = "gpio3",
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.max_register = RK628_GPIO3_BASE + GPIO_VER_ID,
		.reg_format_endian = REGMAP_ENDIAN_NATIVE,
		.val_format_endian = REGMAP_ENDIAN_NATIVE,
		.rd_table = &rk628_gpio3_readable_table,
	},
};

static void rk628_power_on(struct rk628 *rk628, bool on)
{
	if (!rk628->display_enabled && on) {
		gpiod_direction_output(rk628->enable_gpio, 1);
		usleep_range(10000, 11000);
		gpiod_direction_output(rk628->reset_gpio, 0);
		usleep_range(10000, 11000);
		gpiod_direction_output(rk628->reset_gpio, 1);
		usleep_range(10000, 11000);
		gpiod_direction_output(rk628->reset_gpio, 0);
		usleep_range(10000, 11000);
	}

	if (!on) {
		gpiod_direction_output(rk628->reset_gpio, 1);
		gpiod_direction_output(rk628->enable_gpio, 0);
	}
}

static void rk628_display_disable(struct rk628 *rk628)
{
	if (!rk628->display_enabled)
		return;

	if (rk628_output_is_csi(rk628))
		rk628_csi_disable(rk628);

	if (rk628_output_is_gvi(rk628))
		rk628_gvi_disable(rk628);

	if (rk628_output_is_lvds(rk628))
		rk628_lvds_disable(rk628);

	if (rk628_output_is_dsi(rk628))
		rk628_dsi_disable(rk628);

	if (rk628_output_is_rgb(rk628))
		rk628_rgb_tx_disable(rk628);

	rk628_post_process_disable(rk628);

	if (rk628_input_is_hdmi(rk628))
		rk628_hdmirx_disable(rk628);

	if (rk628_output_is_hdmi(rk628))
		rk628_hdmitx_disable(rk628);

	rk628->display_enabled = false;
}

static void rk628_display_enable(struct rk628 *rk628)
{
	u8 ret = 0;

	if (rk628->display_enabled)
		return;

	if (rk628_input_is_rgb(rk628))
		rk628_rgb_rx_enable(rk628);

	/*
	 * bt1120 needs to configure the timing register, but hdmitx will modify
	 * the timing as needed, so the bt1120 enable process is moved to the
	 * configuration of post_process (function rk628_post_process_enable in
	 * rk628_post_process.c)
	 */

	if (rk628_output_is_rgb(rk628))
		rk628_rgb_tx_enable(rk628);

	if (rk628_output_is_dsi(rk628))
		queue_delayed_work(rk628->dsi_wq, &rk628->dsi_delay_work, msecs_to_jiffies(10));

	if (rk628_input_is_hdmi(rk628)) {
		ret = rk628_hdmirx_enable(rk628);
		if ((ret == HDMIRX_PLUGOUT) || (ret & HDMIRX_NOSIGNAL)) {
			rk628_display_disable(rk628);
			return;
		}
	}

	if (rk628_output_is_bt1120(rk628))
		rk628_bt1120_tx_enable(rk628);

	if (!rk628_output_is_hdmi(rk628)) {
		rk628_post_process_init(rk628);
		rk628_post_process_enable(rk628);
	}

	if (rk628_output_is_lvds(rk628))
		rk628_lvds_enable(rk628);

	if (rk628_output_is_gvi(rk628))
		rk628_gvi_enable(rk628);

	if (rk628_output_is_csi(rk628))
		rk628_csi_enable(rk628);

	if (rk628_output_is_hdmi(rk628))
		rk628_hdmitx_enable(rk628);

	rk628->display_enabled = true;
}

static void rk628_set_hdmirx_irq(struct rk628 *rk628, u32 reg, bool enable)
{
	if (rk628->version != RK628D_VERSION)
		rk628_i2c_write(rk628, reg, RK628F_HDMIRX_IRQ_EN(enable));
	else
		rk628_i2c_write(rk628, reg, RK628D_HDMIRX_IRQ_EN(enable));
}

static void rk628_pwr_consumption_init(struct rk628 *rk628)
{
	if (rk628->display_enabled)
		return;

	/* set pin as int function to allow output interrupt */
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x30002000);

	/*
	 * set unuse pin as GPIO input and
	 * pull down to reduce power consumption
	 */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffff0000);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff0000);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b0000);
	rk628_i2c_write(rk628, GRF_GPIO2C_P_CON, 0x3f0015);
	rk628_i2c_write(rk628, GRF_GPIO3A_P_CON, 0xcc0044);

	if (!rk628_output_is_hdmi(rk628)) {
		u32 mask = SW_OUTPUT_MODE_MASK;
		u32 val = SW_OUTPUT_MODE(OUTPUT_MODE_HDMI);

		if (rk628->version == RK628F_VERSION) {
			mask = SW_HDMITX_EN_MASK;
			val = SW_HDMITX_EN(1);
		}

		/* disable clock/data channel and band gap when hdmitx not work */
		rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, mask, val);
		rk628_i2c_write(rk628, HDMI_PHY_SYS_CTL, 0x17);
		rk628_i2c_write(rk628, HDMI_PHY_CHG_PWR, 0x0);
		rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, mask, 0);
	}
}

#ifdef CONFIG_FB
static int rk628_fb_notifier_callback(struct notifier_block *nb,
				      unsigned long event, void *data)
{
	struct rk628 *rk628 = container_of(nb, struct rk628, fb_nb);
	struct fb_event *ev_data = data;
	int *blank;

	if ((event != FB_EVENT_BLANK) || (!ev_data) || (!ev_data->data))
		return NOTIFY_DONE;

	blank = ev_data->data;
	if (rk628->old_blank == *blank)
		return NOTIFY_DONE;

	rk628->old_blank = *blank;

	switch (*blank) {
	case FB_BLANK_UNBLANK:
		rk628_power_on(rk628, true);
		rk628_pwr_consumption_init(rk628);
		rk628_cru_init(rk628);

		if (rk628_input_is_hdmi(rk628)) {
			rk628_set_hdmirx_irq(rk628, GRF_INTR0_EN, true);

			/*
			 * make hdmi rx register domain polling
			 * access to be normal by setting hdmi in
			 */
			rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
					      SW_INPUT_MODE_MASK,
					      SW_INPUT_MODE(INPUT_MODE_HDMI));

			queue_delayed_work(rk628->monitor_wq,
					   &rk628->delay_work,
					   msecs_to_jiffies(50));

			return NOTIFY_OK;
		}

		rk628_display_enable(rk628);

		return NOTIFY_OK;
	case FB_BLANK_POWERDOWN:
		if (rk628_input_is_hdmi(rk628))
			cancel_delayed_work_sync(&rk628->delay_work);

		rk628_display_disable(rk628);
		rk628_power_on(rk628, false);

		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}
#endif

static int rk628_get_pwm_bl(struct rk628 *rk628)
{
	struct device_node *backlight;

	backlight = of_parse_phandle(rk628->dev->of_node, "panel-backlight", 0);
	if (backlight) {
		rk628->panel->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!rk628->panel->backlight) {
			dev_err(rk628->dev, "%s: failed to find backlight\n", __func__);
			return -EAGAIN;
		}
	} else {
		dev_err(rk628->dev, "%s: failed to find panel-backlight\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void rk628_display_work(struct work_struct *work)
{
	u8 ret = 0;
	struct rk628 *rk628 =
		container_of(work, struct rk628, delay_work.work);
	int delay = msecs_to_jiffies(2000);

	if (rk628_input_is_hdmi(rk628)) {
		ret = rk628_hdmirx_detect(rk628);
		dev_info(rk628->dev, "%s: hdmirx detect status:0x%x\n", __func__, ret);
		if (!(ret & (HDMIRX_CHANGED | HDMIRX_NOLOCK))) {
			if (!rk628->plugin_det_gpio)
				queue_delayed_work(rk628->monitor_wq,
						   &rk628->delay_work, delay);
			else
				rk628_hdmirx_enable_interrupts(rk628, true);
			return;
		}
	}

	if (ret & HDMIRX_PLUGIN) {
		/* if resolution or input format change, disable first */
		rk628_display_disable(rk628);
		if (rk628->pwm_bl_en && !rk628->panel->backlight) {
			int bl_ret = 0;

			bl_ret = rk628_get_pwm_bl(rk628);
			if (!bl_ret) {
				rk628_display_enable(rk628);
			} else if (bl_ret == -EAGAIN) {
				queue_delayed_work(rk628->monitor_wq,
						   &rk628->delay_work, msecs_to_jiffies(50));
				return;
			} else {
				return;
			}
		} else {
			rk628_display_enable(rk628);
		}
	} else if (ret & HDMIRX_PLUGOUT) {
		rk628_display_disable(rk628);
	}

	if (rk628_input_is_hdmi(rk628)) {
		if (!rk628->plugin_det_gpio) {
			if (ret & HDMIRX_NOLOCK)
				delay = msecs_to_jiffies(200);
			queue_delayed_work(rk628->monitor_wq, &rk628->delay_work,
					   delay);
		} else {
			rk628_hdmirx_enable_interrupts(rk628, true);
		}
	}
}

static void rk628_dsi_work(struct work_struct *work)
{
	struct rk628 *rk628 = container_of(work, struct rk628, dsi_delay_work.work);

	rk628_mipi_dsi_pre_enable(rk628);
	rk628_mipi_dsi_enable(rk628);
}

static irqreturn_t rk628_hdmirx_plugin_irq(int irq, void *dev_id)
{
	struct rk628 *rk628 = dev_id;
	u32 val;

	rk628_hdmirx_enable_interrupts(rk628, false);
	/* update hdmirx phy lock status */
	rk628_hdmirx_detect(rk628);
	rk628_i2c_read(rk628, HDMI_RX_MD_ISTS, &val);
	dev_dbg(rk628->dev, "HDMI_RX_MD_ISTS:0x%x\n", val);
	rk628_i2c_write(rk628, HDMI_RX_MD_ICLR, val);
	rk628_set_hdmirx_irq(rk628, GRF_INTR0_CLR_EN, true);

	/* control hpd after 50ms */
	schedule_delayed_work(&rk628->delay_work, HZ / 20);

	return IRQ_HANDLED;
}

static bool rk628_display_route_check(struct rk628 *rk628)
{
	if (!hweight32(rk628->input_mode) || !hweight32(rk628->output_mode))
		return false;

	/*
	 * the RGB/BT1120 RX and RGB/BT1120 TX are the same shared IP
	 * and cannot be used as both input and output simultaneously.
	 */
	if ((rk628_input_is_rgb(rk628) || rk628_input_is_bt1120(rk628)) &&
	    (rk628_output_is_rgb(rk628) || rk628_output_is_bt1120(rk628)))
		return false;

	if (rk628->version == RK628F_VERSION)
		return true;

	/* rk628d only support rgb and hdmi output simultaneously */
	if (hweight32(rk628->output_mode) > 2)
		return false;

	if (hweight32(rk628->output_mode) == 2 &&
	    !(rk628_output_is_rgb(rk628) && rk628_output_is_hdmi(rk628)))
		return false;

	return true;
}

static void rk628_current_display_route(struct rk628 *rk628, char *input_s,
					int input_s_len, char *output_s,
					int output_s_len)
{
	*input_s = '\0';
	*output_s = '\0';

	if (rk628_input_is_rgb(rk628))
		strlcat(input_s, "RGB", input_s_len);
	else if (rk628_input_is_bt1120(rk628))
		strlcat(input_s, "BT1120", input_s_len);
	else if (rk628_input_is_hdmi(rk628))
		strlcat(input_s, "HDMI", input_s_len);
	else
		strlcat(input_s, "unknown", input_s_len);

	if (rk628_output_is_rgb(rk628))
		strlcat(output_s, "RGB ", output_s_len);

	if (rk628_output_is_bt1120(rk628))
		strlcat(output_s, "BT1120 ", output_s_len);

	if (rk628_output_is_gvi(rk628))
		strlcat(output_s, "GVI ", output_s_len);

	if (rk628_output_is_lvds(rk628))
		strncat(output_s, "LVDS ", output_s_len);

	if (rk628_output_is_dsi(rk628))
		strlcat(output_s, "DSI ", output_s_len);

	if (rk628_output_is_csi(rk628))
		strlcat(output_s, "CSI ", output_s_len);

	if (rk628_output_is_hdmi(rk628))
		strlcat(output_s, "HDMI ", output_s_len);

	if (!strlen(output_s))
		strlcat(output_s, "unknown", output_s_len);
}

static void rk628_show_current_display_route(struct rk628 *rk628)
{
	char input_s[10], output_s[30];

	rk628_current_display_route(rk628, input_s, sizeof(input_s),
				    output_s, sizeof(output_s));

	dev_info(rk628->dev, "input_mode: %s, output_mode: %s\n", input_s,
		 output_s);
}

static int rk628_display_route_info_parse(struct rk628 *rk628)
{
	struct device_node *np;
	int ret = 0;

	if (of_property_read_bool(rk628->dev->of_node, "rk628-hdmi-in") ||
	    of_property_read_bool(rk628->dev->of_node, "rk628,hdmi-in")) {
		rk628->input_mode = BIT(INPUT_MODE_HDMI);
	} else if (of_property_read_bool(rk628->dev->of_node, "rk628-rgb-in") ||
		   of_property_read_bool(rk628->dev->of_node, "rk628,rgb-in")) {
		rk628->input_mode = BIT(INPUT_MODE_RGB);
		ret = rk628_rgb_parse(rk628, NULL);
	} else if (of_property_read_bool(rk628->dev->of_node, "rk628-bt1120-in") ||
		   of_property_read_bool(rk628->dev->of_node, "rk628,bt1120-in")) {
		rk628->input_mode = BIT(INPUT_MODE_BT1120);
		ret = rk628_rgb_parse(rk628, NULL);
	} else {
		rk628->input_mode = BIT(INPUT_MODE_RGB);
	}

	if ((np = of_get_child_by_name(rk628->dev->of_node, "rk628-gvi-out")) ||
	    (np = of_get_child_by_name(rk628->dev->of_node, "rk628-gvi"))) {
		rk628->output_mode |= BIT(OUTPUT_MODE_GVI);
		ret = rk628_gvi_parse(rk628, np);
	} else if ((np = of_get_child_by_name(rk628->dev->of_node, "rk628-lvds-out")) ||
		   (np = of_get_child_by_name(rk628->dev->of_node, "rk628-lvds"))) {
		rk628->output_mode |= BIT(OUTPUT_MODE_LVDS);
		ret = rk628_lvds_parse(rk628, np);
	} else if ((np = of_get_child_by_name(rk628->dev->of_node, "rk628-dsi-out")) ||
		   (np = of_get_child_by_name(rk628->dev->of_node, "rk628-dsi"))) {
		rk628->output_mode |= BIT(OUTPUT_MODE_DSI);
		ret = rk628_dsi_parse(rk628, np);
	} else if (of_property_read_bool(rk628->dev->of_node, "rk628-csi-out") ||
		   of_property_read_bool(rk628->dev->of_node, "rk628,csi-out")) {
		rk628->output_mode |= BIT(OUTPUT_MODE_CSI);
	}
	if (np)
		of_node_put(np);

	if (of_property_read_bool(rk628->dev->of_node, "rk628-hdmi-out") ||
	    of_property_read_bool(rk628->dev->of_node, "rk628,hdmi-out"))
		rk628->output_mode |= BIT(OUTPUT_MODE_HDMI);

	if (of_property_read_bool(rk628->dev->of_node, "rk628-rgb-out") ||
	    of_property_read_bool(rk628->dev->of_node, "rk628-rgb")) {
		rk628->output_mode |= BIT(OUTPUT_MODE_RGB);
		ret = rk628_rgb_parse(rk628, NULL);
	} else if (of_property_read_bool(rk628->dev->of_node, "rk628-bt1120-out") ||
		   of_property_read_bool(rk628->dev->of_node, "rk628-bt1120")) {
		rk628->output_mode |= BIT(OUTPUT_MODE_BT1120);
		ret = rk628_rgb_parse(rk628, NULL);
	}

	return ret;
}

static void
rk628_display_mode_from_videomode(const struct rk628_videomode *vm,
				  struct rk628_display_mode *dmode)
{
	dmode->hdisplay = vm->hactive;
	dmode->hsync_start = dmode->hdisplay + vm->hfront_porch;
	dmode->hsync_end = dmode->hsync_start + vm->hsync_len;
	dmode->htotal = dmode->hsync_end + vm->hback_porch;

	dmode->vdisplay = vm->vactive;
	dmode->vsync_start = dmode->vdisplay + vm->vfront_porch;
	dmode->vsync_end = dmode->vsync_start + vm->vsync_len;
	dmode->vtotal = dmode->vsync_end + vm->vback_porch;

	dmode->clock = vm->pixelclock / 1000;
	dmode->flags = vm->flags;
}

static void of_parse_rk628_display_sync_pol(struct rk628 *rk628)
{
	u32 val;

	rk628->src_mode.flags = 0;
	if (!of_property_read_u32(rk628->dev->of_node, "mode-sync-pol", &val)) {
		if (val)
			rk628->src_mode.flags |= DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
		else
			rk628->src_mode.flags |= DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC;
	} else {
		if (of_property_read_u32(rk628->dev->of_node, "mode-hsync-pol", &val) || val)
			rk628->src_mode.flags |= DRM_MODE_FLAG_PHSYNC;
		else
			rk628->src_mode.flags |= DRM_MODE_FLAG_NHSYNC;

		if (of_property_read_u32(rk628->dev->of_node, "mode-vsync-pol", &val) || val)
			rk628->src_mode.flags |= DRM_MODE_FLAG_PVSYNC;
		else
			rk628->src_mode.flags |= DRM_MODE_FLAG_NVSYNC;
	}
}

static void
of_parse_rk628_display_timing(struct device_node *np, struct rk628_videomode *vm)
{
	u32 val;

	of_property_read_u32(np, "clock-frequency", &vm->pixelclock);
	of_property_read_u32(np, "hactive", &vm->hactive);
	of_property_read_u32(np, "hfront-porch", &vm->hfront_porch);
	of_property_read_u32(np, "hback-porch", &vm->hback_porch);
	of_property_read_u32(np, "hsync-len", &vm->hsync_len);

	of_property_read_u32(np, "vactive", &vm->vactive);
	of_property_read_u32(np, "vfront-porch", &vm->vfront_porch);
	of_property_read_u32(np, "vback-porch", &vm->vback_porch);
	of_property_read_u32(np, "vsync-len", &vm->vsync_len);

	vm->flags = 0;
	of_property_read_u32(np, "hsync-active", &val);
	vm->flags |= val ? DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;

	of_property_read_u32(np, "vsync-active", &val);
	vm->flags |= val ? DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;
}

static int rk628_get_video_mode(struct rk628 *rk628)
{
	struct device_node *timings_np, *src_np, *dst_np;
	struct rk628_videomode vm;

	of_parse_rk628_display_sync_pol(rk628);

	timings_np = of_get_child_by_name(rk628->dev->of_node, "display-timings");
	if (!timings_np) {
		dev_info(rk628->dev, "failed to found display timings\n");
		return -EINVAL;
	}

	src_np = of_get_child_by_name(timings_np, "src-timing");
	if (!src_np) {
		dev_info(rk628->dev, "failed to found src timing\n");
		of_node_put(timings_np);
		return -EINVAL;
	}

	of_parse_rk628_display_timing(src_np, &vm);
	rk628_display_mode_from_videomode(&vm, &rk628->src_mode);
	dev_info(rk628->dev, "src mode: %d %d %d %d %d %d %d %d %d 0x%x\n",
		 rk628->src_mode.clock, rk628->src_mode.hdisplay, rk628->src_mode.hsync_start,
		 rk628->src_mode.hsync_end, rk628->src_mode.htotal, rk628->src_mode.vdisplay,
		 rk628->src_mode.vsync_start, rk628->src_mode.vsync_end, rk628->src_mode.vtotal,
		 rk628->src_mode.flags);

	dst_np = of_get_child_by_name(timings_np, "dst-timing");
	if (!dst_np) {
		dev_info(rk628->dev, "failed to found dst timing\n");
		of_node_put(timings_np);
		of_node_put(src_np);
		return -EINVAL;
	}

	of_parse_rk628_display_timing(dst_np, &vm);
	rk628_display_mode_from_videomode(&vm, &rk628->dst_mode);
	dev_info(rk628->dev, "dst mode: %d %d %d %d %d %d %d %d %d 0x%x\n",
		 rk628->dst_mode.clock, rk628->dst_mode.hdisplay, rk628->dst_mode.hsync_start,
		 rk628->dst_mode.hsync_end, rk628->dst_mode.htotal, rk628->dst_mode.vdisplay,
		 rk628->dst_mode.vsync_start, rk628->dst_mode.vsync_end, rk628->dst_mode.vtotal,
		 rk628->dst_mode.flags);

	of_node_put(timings_np);
	of_node_put(src_np);
	of_node_put(dst_np);

	return 0;
}

static int rk628_display_timings_get(struct rk628 *rk628)
{
	int ret;

	ret = rk628_get_video_mode(rk628);

	return ret;

}

static void rk628_pwm_work(struct work_struct *work)
{
	struct rk628 *rk628 = container_of(work, struct rk628, pwm_delay_work.work);
	int ret;

	ret = rk628_get_pwm_bl(rk628);
	if (!ret)
		rk628_display_enable(rk628);
	else if (ret == -EAGAIN)
		queue_delayed_work(rk628->pwm_wq, &rk628->pwm_delay_work,
				   msecs_to_jiffies(50));
}

#define DEBUG_PRINT(args...) \
		do { \
			if (s) \
				seq_printf(s, args); \
			else \
				pr_info(args); \
		} while (0)

static void rk628_show_resolution(struct seq_file *s)
{
	struct rk628 *rk628 = s->private;
	struct rk628_display_mode *src_mode = &rk628->src_mode;
	u32 src_hactive, src_hs_start, src_hs_end, src_htotal;
	u32 src_vactive, src_vs_start, src_vs_end, src_vtotal;
	u32 src_dclk, clk_rx_read;
	u32 fps;
	const char *bus_format_s;

	/* get timing */
	src_dclk = src_mode->clock;
	src_hactive = src_mode->hdisplay;
	src_hs_start = src_mode->hsync_start;
	src_hs_end = src_mode->hsync_end;
	src_htotal = src_mode->htotal;
	src_vactive = src_mode->vdisplay;
	src_vs_start = src_mode->vsync_start;
	src_vs_end = src_mode->vsync_end;
	src_vtotal = src_mode->vtotal;

	/* get fps */
	fps = src_dclk * 1000 / (src_htotal * src_vtotal);

	clk_rx_read = rk628_cru_clk_get_rate(rk628, CGU_CLK_RX_READ) / 1000;

	bus_format_s = rk628_get_input_bus_format_name(rk628);

	/* print */
	DEBUG_PRINT("    Display mode: %dx%dp%d  bus_format: %s\n",
		    src_hactive, src_vactive, fps, bus_format_s);
	DEBUG_PRINT("\tclk[%u] real_clk[%u] flag[%x]\n",
		    clk_rx_read, src_dclk, src_mode->flags);
	DEBUG_PRINT("\tH: %d %d %d %d\n", src_hactive, src_hs_start, src_hs_end,
		    src_htotal);
	DEBUG_PRINT("\tV: %d %d %d %d\n", src_vactive, src_vs_start, src_vs_end,
		    src_vtotal);
}

static void rk628f_show_rgbrx_resolution(struct seq_file *s)
{
	struct rk628 *rk628 = s->private;
	struct rk628_display_mode *src_mode = &rk628->src_mode;
	u32 val;

	u64 src_dclk;
	u32 clk_rx_read;
	u64 imodet_clk;
	u32 rgb_rx_eval_time, rgb_rx_clkrate;

	u16 src_hactive = 0, src_vactive = 0;
	u16 src_htotal, src_vtotal;
	u32 fps;
	const char *bus_format_s;

	/* get clk rgbrx read */
	rk628_i2c_read(rk628, GRF_RGB_RX_DBG_MEAS0, &val);
	rgb_rx_eval_time = (val & RGB_RX_EVAL_TIME_MASK) >> 16;

	rk628_i2c_read(rk628, GRF_RGB_RX_DBG_MEAS2, &val);
	rgb_rx_clkrate = val & RGB_RX_CLKRATE_MASK;

	imodet_clk = rk628_cru_clk_get_rate(rk628, CGU_CLK_IMODET);

	src_dclk = imodet_clk * rgb_rx_clkrate;
	do_div(src_dclk, rgb_rx_eval_time + 1);
	do_div(src_dclk, 1000);

	/* get timing */
	if (rk628_input_is_rgb(rk628)) {
		rk628_i2c_read(rk628, GRF_RGB_RX_DBG_MEAS4, &val);
		src_vactive = (val >> 16) & 0xffff;
		src_hactive = val & 0xffff;
	} else if (rk628_input_is_bt1120(rk628)) {
		rk628_i2c_read(rk628, GRF_SYSTEM_STATUS3, &val);
		src_vactive = val & DECODER_1120_LAST_LINE_NUM_MASK;

		rk628_i2c_read(rk628, GRF_SYSTEM_STATUS4, &val);
		src_hactive = val & DECODER_1120_LAST_PIX_NUM_MASK;
	}

	rk628_i2c_read(rk628, 0x134, &val);
	src_htotal = (val & 0xffff0000) >> 16;
	src_vtotal = val & 0xffff;

	/* get fps */
	fps = src_dclk * 1000 / (src_htotal * src_vtotal);

	clk_rx_read = rk628_cru_clk_get_rate(rk628, CGU_CLK_RX_READ) / 1000;

	bus_format_s = rk628_get_input_bus_format_name(rk628);

	/* print */
	DEBUG_PRINT("    Display mode: %dx%dp%d  bus_format: %s\n",
		    src_hactive, src_vactive, fps, bus_format_s);
	DEBUG_PRINT("\tclk[%u] real_clk[%llu] flag[%x]\n",
		    clk_rx_read, src_dclk, src_mode->flags);
	DEBUG_PRINT("\tH-total: %d\n", src_htotal);
	DEBUG_PRINT("\tV-total: %d\n", src_vtotal);
}

static void rk628_show_input_resolution(struct seq_file *s)
{
	struct rk628 *rk628 = s->private;

	if ((rk628_input_is_rgb(rk628) || rk628_input_is_bt1120(rk628)) &&
		rk628->version == RK628F_VERSION)
		rk628f_show_rgbrx_resolution(s);
	else
		rk628_show_resolution(s);
}

static void rk628_show_output_resolution(struct seq_file *s)
{
	struct rk628 *rk628 = s->private;
	struct rk628_display_mode *dst_mode = &rk628->dst_mode;
	u32 val;
	u64 sclk_vop;
	u32 dsp_htotal, dsp_hs_end, dsp_hact_st, dsp_hact_end;
	u32 dsp_vtotal, dsp_vs_end, dsp_vact_st, dsp_vact_end;
	u32 fps;
	const char *bus_format_s;

	/* get sclk_vop */
	sclk_vop = rk628_cru_clk_get_rate(rk628, CGU_SCLK_VOP);
	do_div(sclk_vop, 1000);

	/* get timing */
	rk628_i2c_read(rk628, GRF_SCALER_CON3, &val);
	dsp_htotal = val & 0xffff;
	dsp_hs_end = (val & 0xff0000) >> 16;

	rk628_i2c_read(rk628, GRF_SCALER_CON4, &val);
	dsp_hact_end = val & 0xffff;
	dsp_hact_st = (val & 0xfff0000) >> 16;

	rk628_i2c_read(rk628, GRF_SCALER_CON5, &val);
	dsp_vtotal = val & 0xfff;
	dsp_vs_end = (val & 0xff0000) >> 16;

	rk628_i2c_read(rk628, GRF_SCALER_CON6, &val);
	dsp_vact_st = (val & 0xfff0000) >> 16;
	dsp_vact_end = val & 0xfff;

	/* get fps */
	fps = sclk_vop * 1000 / (dsp_vtotal * dsp_htotal);

	bus_format_s = rk628_get_output_bus_format_name(rk628);

	/* print */
	DEBUG_PRINT("    Display mode: %dx%dp%d  bus_format: %s\n",
		    dsp_hact_end - dsp_hact_st, dsp_vact_end - dsp_vact_st, fps,
		    bus_format_s);
	DEBUG_PRINT("\tclk[%u] real_clk[%llu] flag[%x]\n",
		    dst_mode->clock, sclk_vop, dst_mode->flags);
	DEBUG_PRINT("\tH: %d %d %d %d\n", dsp_hact_end - dsp_hact_st,
		    dsp_htotal - dsp_hact_st,
		    dsp_htotal - dsp_hact_st + dsp_hs_end, dsp_htotal);
	DEBUG_PRINT("\tV: %d %d %d %d\n", dsp_vact_end - dsp_vact_st,
		    dsp_vtotal - dsp_vact_st,
		    dsp_vtotal - dsp_vact_st + dsp_vs_end, dsp_vtotal);
}

static void rk628_show_csc_info(struct seq_file *s)
{
	struct rk628 *rk628 = s->private;
	u32 val;
	bool r2y, y2r, csc;
	char csc_mode_r2y_s[10];
	char csc_mode_y2r_s[10];
	const char *csc_mode_s;
	u32 mode;
	enum csc_mode {
		BT601_L,
		BT709_L,
		BT601_F,
		BT2020
	};

	rk628_i2c_read(rk628, GRF_CSC_CTRL_CON, &val);
	r2y = ((val & BIT(4)) == BIT(4));
	y2r = ((val & BIT(0)) == BIT(0));
	csc = ((val & BIT(11)) == BIT(11));

	DEBUG_PRINT("csc:\n");

	mode = (val & 0xc0) >> 6;
	switch (mode) {
	case BT601_L:
		strcpy(csc_mode_r2y_s, "BT601_L");
		break;
	case BT601_F:
		strcpy(csc_mode_r2y_s, "BT601_F");
		break;
	case BT709_L:
		strcpy(csc_mode_r2y_s, "BT709_L");
		break;
	case BT2020:
		strcpy(csc_mode_r2y_s, "BT2020");
		break;
	}

	mode = (val & 0xc) >> 2;
	switch (mode) {
	case BT601_L:
		strcpy(csc_mode_y2r_s, "BT601_L");
		break;
	case BT601_F:
		strcpy(csc_mode_y2r_s, "BT601_F");
		break;
	case BT709_L:
		strcpy(csc_mode_y2r_s, "BT709_L");
		break;
	case BT2020:
		strcpy(csc_mode_y2r_s, "BT2020");
		break;
	}

	csc_mode_s = rk628_post_process_get_csc_mode_name(rk628);

	if (r2y)
		DEBUG_PRINT("\tr2y[1], csc mode: %s\n", csc_mode_r2y_s);
	else if (y2r)
		DEBUG_PRINT("\ty2r[1], csc mode: %s\n", csc_mode_y2r_s);
	else if (csc)
		DEBUG_PRINT("\tcsc[1], csc mode: %s\n", csc_mode_s);
	else
		DEBUG_PRINT("\tnot open\n");
}

static int rk628_debugfs_dump(struct seq_file *s, void *data)
{
	struct rk628 *rk628 = s->private;
	char input_s[10], output_s[30];
	u32 val;
	int sw_hsync_pol, sw_vsync_pol;
	u32 dsp_frame_v_start, dsp_frame_h_start;

	rk628_current_display_route(rk628, input_s, sizeof(input_s),
				    output_s, sizeof(output_s));

	/* show input info */
	DEBUG_PRINT("input: %s\n", input_s);
	rk628_show_input_resolution(s);

	/* show output info */
	DEBUG_PRINT("output: %s\n", output_s);
	rk628_show_output_resolution(s);

	/* show csc info */
	rk628_show_csc_info(s);

	/* show other info */
	rk628_i2c_read(rk628, GRF_SYSTEM_CON0, &val);
	sw_hsync_pol = (val & 0x4000000) ? 1 : 0;
	sw_vsync_pol = (val & 0x2000000) ? 1 : 0;

	rk628_i2c_read(rk628, GRF_SCALER_CON2, &val);
	dsp_frame_h_start = val & 0xffff;
	dsp_frame_v_start = (val & 0xffff0000) >> 16;

	DEBUG_PRINT("system:\n");
	DEBUG_PRINT("\tsw_hsync_pol:%d, sw_vsync_pol:%d\n", sw_hsync_pol,
		    sw_vsync_pol);
	DEBUG_PRINT("\tdsp_frame_h_start:%d, dsp_frame_v_start:%d\n",
		    dsp_frame_h_start, dsp_frame_v_start);

	return 0;
}

static int rk628_debugfs_open(struct inode *inode, struct file *file)
{
	struct rk628 *rk628 = inode->i_private;

	return single_open(file, rk628_debugfs_dump, rk628);
}


static const struct file_operations rk628_debugfs_summary_fops = {
	.owner = THIS_MODULE,
	.open = rk628_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,

};

static int rk628_version_info(struct rk628 *rk628)
{
	int ret;
	char *version;

	ret = rk628_i2c_read(rk628, GRF_SOC_VERSION, &rk628->version);
	if (ret) {
		dev_err(rk628->dev, "failed to access register: %d\n", ret);
		return ret;
	}

	switch (rk628->version) {
	case RK628D_VERSION:
		version = "D/E";
		break;
	case RK628F_VERSION:
		version = "F/H";
		break;
	default:
		version = "unknown";
		ret = -EINVAL;
	}

	dev_info(rk628->dev, "the driver version is %s of RK628-%s\n", DRIVER_VERSION, version);

	return ret;
}

static int rk628_reg_show(struct seq_file *s, void *v)
{
	const struct regmap_config *reg;
	struct rk628 *rk628 = s->private;
	unsigned int i, j;
	u32 val;

	seq_printf(s, "rk628_%s:\n", file_dentry(s->file)->d_iname);

	for (i = 0; i < ARRAY_SIZE(rk628_regmap_config); i++) {
		reg = &rk628_regmap_config[i];
		if (!reg->name)
			continue;
		if (!strcmp(reg->name, file_dentry(s->file)->d_iname))
			break;
	}

	if (i == ARRAY_SIZE(rk628_regmap_config))
		return -ENODEV;

	/* grf */
	if (!reg->rd_table) {
		for (i = 0; i <= reg->max_register; i += 4) {
			rk628_i2c_read(rk628, i, &val);
			if (i % 16 == 0)
				seq_printf(s, "\n0x%04x:", i);
			seq_printf(s, " %08x", val);
		}
	} else {
		const struct regmap_range *range_list = reg->rd_table->yes_ranges;
		const struct regmap_range *range;
		int range_list_len = reg->rd_table->n_yes_ranges;

		for (i = 0; i < range_list_len; i++) {
			range = &range_list[i];
			for (j = range->range_min; j <= range->range_max; j += 4) {
				rk628_i2c_read(rk628, j, &val);
				if (j % 16 == 0 || j == range->range_min)
					seq_printf(s, "\n0x%04x:", j);
				seq_printf(s, " %08x", val);
			}
		}

		seq_puts(s, "\n");
	}

	return 0;
}

static ssize_t rk628_reg_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	const struct regmap_config *reg;
	struct rk628 *rk628 = file->f_path.dentry->d_inode->i_private;
	int i;
	u32 addr;
	u32 val;
	char kbuf[25];
	int ret;

	if (count >= sizeof(kbuf))
		return -ENOSPC;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	ret = sscanf(kbuf, "%x%x", &addr, &val);
	if (ret != 2)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(rk628_regmap_config); i++) {
		reg = &rk628_regmap_config[i];
		if (!reg->name)
			continue;
		if (!strcmp(reg->name, file_dentry(file)->d_iname))
			break;
	}

	if (i == ARRAY_SIZE(rk628_regmap_config))
		return -ENODEV;

	rk628_i2c_write(rk628, addr, val);

	return count;
}

static int rk628_reg_open(struct inode *inode, struct file *file)
{
	struct rk628 *rk628 = inode->i_private;

	return single_open(file, rk628_reg_show, rk628);
}

static const struct file_operations rk628_reg_fops = {
	.owner          = THIS_MODULE,
	.open           = rk628_reg_open,
	.read           = seq_read,
	.write          = rk628_reg_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void rk628_debugfs_register_create(struct rk628 *rk628)
{
	const struct regmap_config *reg;
	struct dentry *dir;
	int i;

	dir = debugfs_create_dir("registers", rk628->debug_dir);
	if (IS_ERR(dir))
		return;

	for (i = 0; i < ARRAY_SIZE(rk628_regmap_config); i++) {
		reg = &rk628_regmap_config[i];
		if (!reg->name)
			continue;
		debugfs_create_file(reg->name, 0600, dir, rk628, &rk628_reg_fops);
	}
}

static void rk628_debugfs_create(struct rk628 *rk628)
{
	rk628->debug_dir = debugfs_create_dir(dev_name(rk628->dev), debugfs_lookup("rk628", NULL));
	if (IS_ERR(rk628->debug_dir))
		return;

	/* path example: /sys/kernel/debug/rk628/2-0050/summary */
	debugfs_create_file("summary", 0400, rk628->debug_dir, rk628,
			    &rk628_debugfs_summary_fops);

	rk628_debugfs_register_create(rk628);

	rk628_cru_create_debugfs_file(rk628);
	rk628_rgb_decoder_create_debugfs_file(rk628);
	rk628_post_process_create_debugfs_file(rk628);
	rk628_mipi_dsi_create_debugfs_file(rk628);
	rk628_gvi_create_debugfs_file(rk628);
	rk628_hdmitx_create_debugfs_file(rk628);
	rk628_pwm_create_debugfs_file(rk628);
}

static void rk628_loader_protect(struct rk628 *rk628)
{
	u32 enabled;
	int ret;

	ret = rk628_i2c_read(rk628, GRF_SCALER_CON0, &enabled);
	if (ret || !(enabled & SCL_EN(1)))
		return;

	if (rk628_input_is_rgb(rk628) || rk628_input_is_bt1120(rk628))
		rk628->display_enabled = true;

	if (rk628_input_is_hdmi(rk628)) {
		if (!rk628_hdmirx_boot_state_init(rk628))
			rk628->display_enabled = true;
	}

	if (rk628->display_enabled && rk628->panel && rk628->panel->supply) {
		ret = regulator_enable(rk628->panel->supply);
		if (ret)
			dev_info(rk628->dev, "failed to enable panel power supply\n");

		dev_info(rk628->dev, "%s:%d show uboot logo\n", __func__, __LINE__);
	}
}

static int
rk628_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct rk628 *rk628;
	int i, ret;
	unsigned long irq_flags;
	struct device_node *np;

	rk628 = devm_kzalloc(dev, sizeof(*rk628), GFP_KERNEL);
	if (!rk628)
		return -ENOMEM;

	rk628->dev = dev;
	rk628->client = client;
	i2c_set_clientdata(client, rk628);
	rk628->hdmirx_irq = client->irq;

	np = of_find_node_by_name(rk628->dev->of_node, "rk628-pwm");
	if (of_device_is_available(np)) {
		ret = rk628_pwm_probe(rk628, np);
		if (ret) {
			of_node_put(np);
			dev_err(dev, "failed to probe pwm\n");
			return ret;
		}
	}
	of_node_put(np);

	if (rk628->pwm_bl_en) {
		rk628->pwm_wq = alloc_ordered_workqueue("%s",
			WQ_MEM_RECLAIM | WQ_FREEZABLE, "rk628-pwm-wq");
		if (!rk628->pwm_wq)
			return -ENOMEM;

		INIT_DELAYED_WORK(&rk628->pwm_delay_work, rk628_pwm_work);
	}

	ret = rk628_display_route_info_parse(rk628);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "display route err\n");
		goto destroy_pwm_wq;
	}

	if (!rk628_output_is_csi(rk628)) {
		ret = rk628_display_timings_get(rk628);
		if (ret && !rk628_output_is_hdmi(rk628)) {
			dev_info(dev, "display timings err\n");
			goto destroy_pwm_wq;
		}
	}

	rk628->soc_24M = devm_clk_get(dev, "soc_24M");
	if (rk628->soc_24M == ERR_PTR(-ENOENT))
		rk628->soc_24M = NULL;

	if (IS_ERR(rk628->soc_24M)) {
		ret = PTR_ERR(rk628->soc_24M);
		dev_err(dev, "Unable to get soc_24M: %d\n", ret);
		goto destroy_pwm_wq;
	}

	clk_prepare_enable(rk628->soc_24M);

	rk628->enable_gpio = devm_gpiod_get_optional(dev, "enable",
						     GPIOD_ASIS);
	if (IS_ERR(rk628->enable_gpio)) {
		ret = PTR_ERR(rk628->enable_gpio);
		dev_err(dev, "failed to request enable GPIO: %d\n", ret);
		goto err_clk;
	}

	rk628->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(rk628->reset_gpio)) {
		ret = PTR_ERR(rk628->reset_gpio);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		goto err_clk;
	}

	rk628->plugin_det_gpio = devm_gpiod_get_optional(dev, "plugin-det",
						    GPIOD_IN);
	if (IS_ERR(rk628->plugin_det_gpio)) {
		dev_err(rk628->dev, "failed to get hdmirx det gpio\n");
		ret = PTR_ERR(rk628->plugin_det_gpio);
		goto err_clk;
	}

	for (i = 0; i < RK628_DEV_MAX; i++) {
		const struct regmap_config *config = &rk628_regmap_config[i];

		if (!config->name)
			continue;

		rk628->regmap[i] = devm_regmap_init_i2c(client, config);
		if (IS_ERR(rk628->regmap[i])) {
			ret = PTR_ERR(rk628->regmap[i]);
			dev_err(dev, "failed to allocate register map %d: %d\n",
				i, ret);
			goto err_clk;
		}
	}

	rk628_loader_protect(rk628);
	rk628_power_on(rk628, true);

	ret = rk628_version_info(rk628);
	if (ret)
		goto err_clk;

	rk628_show_current_display_route(rk628);

	if (!rk628_display_route_check(rk628)) {
		dev_err(dev, "display route check err\n");
		ret = -EINVAL;
		goto err_clk;
	}

	rk628_pwr_consumption_init(rk628);

#ifdef CONFIG_FB
	rk628->fb_nb.notifier_call = rk628_fb_notifier_callback;
	fb_register_client(&rk628->fb_nb);
#endif

	rk628->monitor_wq = alloc_ordered_workqueue("%s",
		WQ_MEM_RECLAIM | WQ_FREEZABLE, "rk628-monitor-wq");
	INIT_DELAYED_WORK(&rk628->delay_work, rk628_display_work);

	if (rk628_output_is_dsi(rk628)) {
		rk628->dsi_wq = alloc_ordered_workqueue("%s",
			WQ_MEM_RECLAIM | WQ_FREEZABLE, "rk628-dsi-wq");
		INIT_DELAYED_WORK(&rk628->dsi_delay_work, rk628_dsi_work);
	}

	rk628_pwm_init(rk628);

	rk628_cru_init(rk628);

	if (rk628_output_is_csi(rk628))
		rk628_csi_init(rk628);

	if (rk628_input_is_hdmi(rk628)) {
		if (rk628->plugin_det_gpio) {
			rk628->plugin_irq = gpiod_to_irq(rk628->plugin_det_gpio);
			if (rk628->plugin_irq < 0) {
				dev_err(rk628->dev, "failed to get plugin det irq\n");
				ret = rk628->plugin_irq;
				goto err_clk;
			}

			ret = devm_request_threaded_irq(dev, rk628->plugin_irq, NULL,
							rk628_hdmirx_plugin_irq,
							IRQF_TRIGGER_FALLING |
							IRQF_TRIGGER_RISING | IRQF_ONESHOT,
							"rk628_hdmirx", rk628);
			if (ret) {
				dev_err(rk628->dev, "failed to register plugin det irq (%d)\n",
					ret);
				goto err_clk;
			}

			if (rk628->hdmirx_irq) {
				irq_flags =
					irqd_get_trigger_type(irq_get_irq_data(rk628->hdmirx_irq));
				dev_dbg(rk628->dev, "cfg hdmirx irq, flags: %lu!\n", irq_flags);
				ret = devm_request_threaded_irq(dev, rk628->hdmirx_irq, NULL,
								rk628_hdmirx_plugin_irq,
								irq_flags | IRQF_ONESHOT,
								"rk628", rk628);
				if (ret) {
					dev_err(rk628->dev, "request rk628 irq failed! err:%d\n",
						ret);
					goto err_clk;
				}
				/* hdmirx int en */
				rk628_set_hdmirx_irq(rk628, GRF_INTR0_EN, true);
				queue_delayed_work(rk628->monitor_wq, &rk628->delay_work,
						   msecs_to_jiffies(20));
			}
		} else {
			queue_delayed_work(rk628->monitor_wq, &rk628->delay_work,
					    msecs_to_jiffies(50));
		}
	} else {
		if (rk628->pwm_bl_en)
			queue_delayed_work(rk628->pwm_wq, &rk628->pwm_delay_work,
					   msecs_to_jiffies(50));
		else
			rk628_display_enable(rk628);
	}

	pm_runtime_enable(dev);
	rk628_debugfs_create(rk628);

	return 0;

err_clk:
	clk_disable_unprepare(rk628->soc_24M);

destroy_pwm_wq:
	if (rk628->pwm_wq)
		destroy_workqueue(rk628->pwm_wq);

	return ret;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void rk628_i2c_remove(struct i2c_client *client)
#else
static int rk628_i2c_remove(struct i2c_client *client)
#endif
{
	struct rk628 *rk628 = i2c_get_clientdata(client);
	struct device *dev = &client->dev;

	debugfs_remove_recursive(rk628->debug_dir);

	if (rk628_output_is_dsi(rk628)) {
		cancel_delayed_work_sync(&rk628->dsi_delay_work);
		destroy_workqueue(rk628->dsi_wq);
	}

#ifdef CONFIG_FB
	fb_unregister_client(&rk628->fb_nb);
#endif
	rk628_set_hdmirx_irq(rk628, GRF_INTR0_EN, false);
	cancel_delayed_work_sync(&rk628->delay_work);
	destroy_workqueue(rk628->monitor_wq);
	cancel_delayed_work_sync(&rk628->pwm_delay_work);
	destroy_workqueue(rk628->pwm_wq);
	put_device(&rk628->panel->backlight->dev);
	pm_runtime_disable(dev);
	clk_disable_unprepare(rk628->soc_24M);
#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static void rk628_i2c_shutdown(struct i2c_client *client)
{
	struct rk628 *rk628 = i2c_get_clientdata(client);

	rk628_power_on(rk628, false);
}

#ifndef CONFIG_FB
#ifdef CONFIG_PM_SLEEP
static int rk628_suspend(struct device *dev)
{
	struct rk628 *rk628 = dev_get_drvdata(dev);

	rk628_set_hdmirx_irq(rk628, GRF_INTR0_EN, false);
	rk628_display_disable(rk628);
	rk628_power_on(rk628, false);

	return 0;
}

static int rk628_resume(struct device *dev)
{
	struct rk628 *rk628 = dev_get_drvdata(dev);

	rk628_power_on(rk628, true);
	rk628_pwr_consumption_init(rk628);
	rk628_cru_init(rk628);

	if (rk628_input_is_hdmi(rk628)) {
		rk628_set_hdmirx_irq(rk628, GRF_INTR0_EN, true);
		/*
		 * make hdmi rx register domain polling
		 * access to be normal by setting hdmi in
		 */
		rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
				      SW_INPUT_MODE_MASK,
				      SW_INPUT_MODE(INPUT_MODE_HDMI));

		return 0;
	}

	rk628_display_enable(rk628);

	return 0;
}
#endif
#endif

static const struct dev_pm_ops rk628_pm_ops = {
#ifndef CONFIG_FB
#ifdef CONFIG_PM_SLEEP
	.suspend = rk628_suspend,
	.resume = rk628_resume,
#endif
#endif
};

static const struct of_device_id rk628_of_match[] = {
	{ .compatible = "rockchip,rk628", },
	{}
};
MODULE_DEVICE_TABLE(of, rk628_of_match);

static const struct i2c_device_id rk628_i2c_id[] = {
	{ "rk628", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, rk628_i2c_id);

static struct i2c_driver rk628_i2c_driver = {
	.driver = {
		.name = "rk628",
		.of_match_table = of_match_ptr(rk628_of_match),
		.pm = &rk628_pm_ops,
	},
	.probe = rk628_i2c_probe,
	.remove = rk628_i2c_remove,
	.id_table = rk628_i2c_id,
	.shutdown = rk628_i2c_shutdown,
};


static int __init rk628_i2c_driver_init(void)
{
	debugfs_create_dir("rk628", NULL);
	i2c_add_driver(&rk628_i2c_driver);

	return 0;
}
#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT_RK628
subsys_initcall_sync(rk628_i2c_driver_init);
#else
module_init(rk628_i2c_driver_init);
#endif

static void __exit rk628_i2c_driver_exit(void)
{
	debugfs_remove_recursive(debugfs_lookup("rk628", NULL));
	i2c_del_driver(&rk628_i2c_driver);
}
module_exit(rk628_i2c_driver_exit);

MODULE_AUTHOR("Wyon Bi <bivvy.bi@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip RK628 MFD driver");
MODULE_LICENSE("GPL");
