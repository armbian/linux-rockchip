// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Elaine Zhang <zhangqing@rock-chips.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <dt-bindings/clock/rockchip,rk3538-cru.h>
#include "clk.h"

#define RK3538_GRF_SOC_STATUS0		0x1a0

enum rk3538_plls {
	cpll, gpll, dpll,
};

/*
 *	## PLL attention.
 *
 * [FRAC PLL]: GPLL, CPLL, DPLL
 *   - frac mode: refdiv can be 1 or 2 only
 *   - int mode:  refdiv has no special limit
 *   - VCO range: [950, 3800] MHZ
 *
 *	## CRU access attention.
 *
 */
static struct rockchip_pll_rate_table rk3538_pll_rates[] = {
	/* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
	RK3036_PLL_RATE(1896000000, 1, 79, 1, 1, 1, 0),
	RK3036_PLL_RATE(1800000000, 1, 75, 1, 1, 1, 0),
	RK3036_PLL_RATE(1704000000, 1, 71, 1, 1, 1, 0),
	RK3036_PLL_RATE(1608000000, 1, 67, 1, 1, 1, 0),
	RK3036_PLL_RATE(1512000000, 1, 63, 1, 1, 1, 0),
	RK3036_PLL_RATE(1416000000, 1, 59, 1, 1, 1, 0),
	RK3036_PLL_RATE(1296000000, 1, 54, 1, 1, 1, 0),
	RK3036_PLL_RATE(1200000000, 1, 50, 1, 1, 1, 0),
	RK3036_PLL_RATE(1188000000, 1, 99, 2, 1, 1, 0),
	RK3036_PLL_RATE(1092000000, 2, 91, 1, 1, 1, 0),
	RK3036_PLL_RATE(1008000000, 1, 42, 1, 1, 1, 0),
	RK3036_PLL_RATE(1000000000, 1, 125, 3, 1, 1, 0),
	RK3036_PLL_RATE(996000000, 2, 83, 1, 1, 1, 0),
	RK3036_PLL_RATE(960000000, 1, 40, 1, 1, 1, 0),
	RK3036_PLL_RATE(912000000, 1, 76, 2, 1, 1, 0),
	RK3036_PLL_RATE(816000000, 1, 68, 2, 1, 1, 0),
	RK3036_PLL_RATE(600000000, 1, 50, 2, 1, 1, 0),
	RK3036_PLL_RATE(594000000, 2, 99, 2, 1, 1, 0),
	RK3036_PLL_RATE(408000000, 1, 68, 2, 2, 1, 0),
	RK3036_PLL_RATE(312000000, 1, 78, 6, 1, 1, 0),
	RK3036_PLL_RATE(216000000, 1, 72, 4, 2, 1, 0),
	RK3036_PLL_RATE(96000000, 1, 24, 3, 2, 1, 0),
	{ /* sentinel */ },
};

#define RK3538_DIV_ACLK_CORE_MASK	0xf
#define RK3538_DIV_ACLK_CORE_SHIFT	10
#define RK3538_DIV_PCLK_CORE_MASK	0x1f
#define RK3538_DIV_PCLK_CORE_SHIFT	3
#define RK3538_DIV_PERIP_CORE_MASK	0x7
#define RK3538_DIV_PERIP_CORE_SHIFT	12
#define RK3538_DIV_SCLK_CORE_MASK	0xf
#define RK3538_DIV_SCLK_CORE_SHIFT	8

#define RK3538_CLKSEL30(_pclk_core, _perip_core, _sclk_core)				\
{											\
	.reg = RK3538_CLKSEL_CON(39),							\
	.val = HIWORD_UPDATE(_pclk_core - 1, RK3538_DIV_PCLK_CORE_MASK,			\
			     RK3538_DIV_PCLK_CORE_SHIFT) |				\
	       HIWORD_UPDATE(_perip_core - 1, RK3538_DIV_PERIP_CORE_MASK,		\
			     RK3538_DIV_PERIP_CORE_SHIFT) |				\
	       HIWORD_UPDATE(_sclk_core - 1, RK3538_DIV_SCLK_CORE_MASK,			\
			     RK3538_DIV_SCLK_CORE_SHIFT),				\
}

#define RK3538_CLKSEL31(_aclk_core)							\
{											\
	.reg = RK3538_CLKSEL_CON(31),							\
	.val = HIWORD_UPDATE(_aclk_core - 1, RK3538_DIV_ACLK_CORE_MASK,			\
			     RK3538_DIV_ACLK_CORE_SHIFT),				\
}

/* SIGN-OFF: _aclk_core: 450M, _pclk_core: 113M, _perip_core: 450M, _sclk_core: 225M */
#define RK3538_CPUCLK_RATE(_prate, _aclk_core, _pclk_core, _perip_core, _sclk_core)	\
{											\
	.prate = _prate,								\
	.divs = {									\
		RK3538_CLKSEL30(_pclk_core, _perip_core, _sclk_core),			\
		RK3538_CLKSEL31(_aclk_core),						\
	},										\
}

static struct rockchip_cpuclk_rate_table rk3538_cpuclk_rates[] __initdata = {
	/* APLL(CPU) rate <= 1900M, due to APLL VCO limit */
	RK3538_CPUCLK_RATE(1896000000, 4, 18, 4, 9),
	RK3538_CPUCLK_RATE(1800000000, 4, 17, 4, 8),
	RK3538_CPUCLK_RATE(1704000000, 4, 17, 4, 8),
	RK3538_CPUCLK_RATE(1608000000, 4, 16, 4, 8),
	RK3538_CPUCLK_RATE(1512000000, 4, 15, 4, 8),
	RK3538_CPUCLK_RATE(1416000000, 4, 14, 4, 8),
	RK3538_CPUCLK_RATE(1296000000, 3, 12, 3, 6),
	RK3538_CPUCLK_RATE(1200000000, 3, 11, 3, 6),
	RK3538_CPUCLK_RATE(1188000000, 3, 11, 3, 6),
	RK3538_CPUCLK_RATE(1092000000, 3, 10, 3, 5),
	RK3538_CPUCLK_RATE(1008000000, 3, 10, 3, 5),
	RK3538_CPUCLK_RATE(1000000000, 3, 10, 3, 5),
	RK3538_CPUCLK_RATE(996000000, 3, 10, 3, 5),
	RK3538_CPUCLK_RATE(960000000, 3, 10, 3, 5),
	RK3538_CPUCLK_RATE(912000000, 2, 9, 2, 5),
	RK3538_CPUCLK_RATE(816000000, 2, 8, 2, 4),
	RK3538_CPUCLK_RATE(600000000, 2, 6, 2, 3),
	RK3538_CPUCLK_RATE(594000000, 2, 6, 2, 3),
	RK3538_CPUCLK_RATE(408000000, 1, 4, 1, 2),
	RK3538_CPUCLK_RATE(312000000, 1, 3, 1, 2),
	RK3538_CPUCLK_RATE(216000000, 1, 2, 1, 1),
	RK3538_CPUCLK_RATE(96000000, 1, 1, 1, 1),
};

/*   Clock Parent Definition    */
PNAME(mux_pll_p)			= { "xin24m" };
PNAME(mux_24m_32k_p)			= { "xin24m", "clk_32k" };
PNAME(mux_gpll_cpll_p)                  = { "gpll", "cpll" };
PNAME(mux_cpll_24m_p)			= { "cpll", "xin24m" };
PNAME(mux_gpll_cpll_24m_p)		= { "gpll", "cpll", "xin24m" };
PNAME(mux_24m_gpll_cpll_p)		= { "xin24m", "gpll", "cpll" };
PNAME(sclk_uart_src_p)			= { "xin24m", "clk_cm_frac_0", "clk_cm_frac_1", "clk_uart_frac_0", "clk_uart_frac_1" };
PNAME(mclk_sai0_src_p)			= { "xin24m", "clk_cm_frac_0", "clk_cm_frac_1", "clk_audio_frac_0", "clk_audio_frac_1", "mclk_sai0_from_io" };
PNAME(mclk_sai1_src_p)			= { "xin24m", "clk_cm_frac_0", "clk_cm_frac_1", "clk_audio_frac_0", "clk_audio_frac_1", "mclk_sai1_from_io" };
PNAME(mclk_sai2_src_p)			= { "xin24m", "clk_cm_frac_0", "clk_cm_frac_1", "clk_audio_frac_0", "clk_audio_frac_1", "mclk_sai2_from_io" };
PNAME(mclk_sai3_src_p)			= { "xin24m", "clk_cm_frac_0", "clk_cm_frac_1", "clk_audio_frac_0", "clk_audio_frac_1" };
PNAME(mclk_spdif_tx_src_p)		= { "xin24m", "clk_cm_frac_0", "clk_cm_frac_1", "clk_audio_frac_0", "clk_audio_frac_1", "mclk_sai0_from_io", "mclk_sai1_from_io", "mclk_sai2_from_io" };
PNAME(mclk_pdm_src_p)			= { "xin24m", "clk_cm_frac_0", "clk_cm_frac_1", "clk_audio_frac_0", "clk_audio_frac_1", "mclk_sai0_from_io", "mclk_sai1_from_io", "mclk_sai2_from_io" };
PNAME(clkout_pdm_src_p)			= { "xin24m", "clk_cm_frac_0", "clk_cm_frac_1", "clk_audio_frac_0", "clk_audio_frac_1", "mclk_sai0_from_io", "mclk_sai1_from_io", "mclk_sai2_from_io" };
PNAME(mux_ref_pulse_p)			= { "usb_sof_clk", "mac_ptp_pps", "gmac_ptp_pps" };
PNAME(mux_200m_24m_p)			= { "clk_gpll_div6", "xin24m" };
PNAME(mux_200m_100m_50m_24m_p)		= { "clk_gpll_div6", "clk_cpll_div10", "clk_cpll_div20", "xin24m" };
PNAME(mux_150m_100m_p)			= { "clk_gpll_div8", "clk_cpll_div10" };
PNAME(mux_300m_200m_100m_24m_p)		= { "clk_gpll_div4", "clk_gpll_div6", "clk_cpll_div10", "xin24m" };
PNAME(mux_300m_200m_150m_100m_p)	= { "clk_gpll_div4", "clk_gpll_div6", "clk_gpll_div8", "clk_cpll_div10" };
PNAME(mux_300m_250m_200m_p)		= { "clk_gpll_div4", "clk_cpll_div4", "clk_gpll_div6" };
PNAME(mux_100m_24m_p)			= { "clk_cpll_div10", "xin24m"};
PNAME(dclk_vp0_p)			= { "dclk_vp0_src", "clk_hdmiphy_pixel_io", "innophyo_prepclk" };
PNAME(clk_irefclk_hdmitx_p)		= { "aclk_vo_root", "clk_hdmitx_earc_high_for_irefclk" };
PNAME(clk_mac_ptp_ref_p)		= { "clk_mac_ptp_ref_src", "clk_mac_ptp_ref_from_io" };
PNAME(clk_in_50m_macphy_p)		= { "clk_50m_vo", "xin24m" };
PNAME(clk_in_macphy_p)			= { "xin24m", "clk_in_50m_macphy" };
PNAME(clk1x_pll_root_p)			= { "dpll", "clk1x_ddrphy_pll_src" };
PNAME(clk_saradc_p)			= { "clk_saradc_src", "clk_saradc_rcosc_io" };
PNAME(clk_gmac_ptp_ref_p)		= { "clk_gmac_ptp_ref_src", "clk_gmac_ptp_ref_from_io" };
PNAME(mclk_sai1_p)			= { "mclk_sai1_src", "mclk_from_earcrx_to_sai1" };
PNAME(clk_refout_p)			= { "xin24m", "xin12m", "clk_refout_pll" };
PNAME(busclk_pmu_pre_p)			= { "pmu_100m_clk", "clk_rcosc" };
PNAME(clk_xin_rc_div_p)			= { "xin24m", "clk_rcosc" };
PNAME(sclk_uart0_p)			= { "sclk_uart0_src", "xin24m", "clk_rcosc" };
PNAME(clk_i2c0_p)			= { "xin24m", "clk_rcosc", "pmu_100m_clk" };
PNAME(clk_pwm0_p)			= { "xin24m", "clk_rcosc", "pmu_100m_clk"};
PNAME(clk_32k_p)			= { "clk_xin_rc_div", "clk_32k_io" };
PNAME(mux_armclk_p)			= { "clk_core_pll_src", "clk_core_pvtpll" };
PNAME(clk_rkvdec_src_p)			= { "clk_rkvdec_pll_src", "clk_rkvdec_pvtpll" };
PNAME(clk_hevc_ca_rkvdec_src_p)		= { "clk_rkvdec_hevc_ca_src", "clk_rkvdec_pvtpll" };

/* Pass 0 to PLL() '_lshift' as a placeholder for rk3066 pll type. We are rk3328 pll type */
static struct rockchip_pll_clock rk3538_pll_clks[] __initdata = {
	[cpll] = PLL(pll_rk3328, PLL_CPLL, "cpll", mux_pll_p,
		     CLK_IS_CRITICAL, RK3538_PLL_CON(0),
		     RK3538_PMUCRU_MODE_CON00, 0, 0, 0, rk3538_pll_rates),

	[gpll] = PLL(pll_rk3328, PLL_GPLL, "gpll", mux_pll_p,
		     CLK_IS_CRITICAL, RK3538_PLL_CON(16),
		     RK3538_PMUCRU_MODE_CON00, 2, 0, 0, rk3538_pll_rates),

	[dpll] = PLL(pll_rk3328, PLL_DPLL, "dpll", mux_pll_p,
		     CLK_IGNORE_UNUSED, RK3538_SUBDDR_PLL_CON(8),
		     RK3538_PMUCRU_MODE_CON00, 2, 0, 0, rk3538_pll_rates),
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)

/*
 * CRU Clock-Architecture
 */
static struct rockchip_clk_branch rk3538_clk_branches[] __initdata = {
	/* top */
	FACTOR_GATE(0, "xin12m", "xin24m", 0, 1, 2,
			RK3538_PMUCRU_CLKGATE_CON(2), 11, GFLAGS),

	FACTOR(0, "clk_spll_div9", "spll", 0, 1, 9),
	FACTOR(0, "clk_spll_div6", "spll", 0, 1, 6),
	FACTOR(0, "clk_spll_div3", "spll", 0, 1, 3),

	COMPOSITE(CLK_CPLL_DIV20, "clk_cpll_div20", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(0), 5, 1, MFLAGS, 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 0, GFLAGS),
	COMPOSITE(CLK_CPLL_DIV10, "clk_cpll_div10", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(0), 11, 1, MFLAGS, 6, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 1, GFLAGS),
	COMPOSITE_NOMUX(CLK_GPLL_DIV8, "clk_gpll_div8", "gpll", 0,
			RK3538_CLKSEL_CON(1), 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 2, GFLAGS),
	COMPOSITE_NOMUX(CLK_GPLL_DIV6, "clk_gpll_div6", "gpll", 0,
			RK3538_CLKSEL_CON(1), 5, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 3, GFLAGS),
	COMPOSITE_NOMUX(CLK_CPLL_DIV4, "clk_cpll_div4", "cpll", 0,
			RK3538_CLKSEL_CON(1), 10, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 4, GFLAGS),
	COMPOSITE_NOMUX(CLK_GPLL_DIV4, "clk_gpll_div4", "gpll", 0,
			RK3538_CLKSEL_CON(2), 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 5, GFLAGS),
	COMPOSITE_NOMUX(CLK_GPLL_DIV3, "clk_gpll_div3", "gpll", 0,
			RK3538_CLKSEL_CON(2), 5, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 6, GFLAGS),

	MUX(CLK_CM_FRAC_0_SRC, "clk_cm_frac_0_src", mux_24m_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(0), 12, 2, MFLAGS),
	COMPOSITE_FRAC(CLK_CM_FRAC_0, "clk_cm_frac_0", "clk_cm_frac_0_src", 0,
			RK3538_CLKSEL_CON(13), CLK_FRAC_DIVIDER_NO_LIMIT,
			RK3538_CLKGATE_CON(0), 7, GFLAGS),
	MUX(CLK_CM_FRAC_1_SRC, "clk_cm_frac_1_src", mux_24m_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(0), 14, 2, MFLAGS),
	COMPOSITE_FRAC(CLK_CM_FRAC_1, "clk_cm_frac_1", "clk_cm_frac_1_src", 0,
			RK3538_CLKSEL_CON(14), CLK_FRAC_DIVIDER_NO_LIMIT,
			RK3538_CLKGATE_CON(0), 8, GFLAGS),
	MUX(CLK_UART_FRAC_0_SRC, "clk_uart_frac_0_src", mux_24m_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(2), 10, 2, MFLAGS),
	COMPOSITE_FRAC(CLK_UART_FRAC_0, "clk_uart_frac_0", "clk_uart_frac_0_src", 0,
			RK3538_CLKSEL_CON(15), CLK_FRAC_DIVIDER_NO_LIMIT,
			RK3538_CLKGATE_CON(0), 9, GFLAGS),
	MUX(CLK_UART_FRAC_1_SRC, "clk_uart_frac_1_src", mux_24m_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(2), 12, 2, MFLAGS),
	COMPOSITE_FRAC(CLK_UART_FRAC_1, "clk_uart_frac_1", "clk_uart_frac_1_src", 0,
			RK3538_CLKSEL_CON(16), CLK_FRAC_DIVIDER_NO_LIMIT,
			RK3538_CLKGATE_CON(0), 10, GFLAGS),
	MUX(CLK_AUDIO_FRAC_0_SRC, "clk_audio_frac_0_src", mux_24m_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(3), 0, 2, MFLAGS),
	COMPOSITE_FRAC(CLK_AUDIO_FRAC_0, "clk_audio_frac_0", "clk_audio_frac_0_src", 0,
			RK3538_CLKSEL_CON(17), CLK_FRAC_DIVIDER_NO_LIMIT,
			RK3538_CLKGATE_CON(0), 11, GFLAGS),
	MUX(CLK_AUDIO_FRAC_1_SRC, "clk_audio_frac_1_src", mux_24m_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(3), 2, 2, MFLAGS),
	COMPOSITE_FRAC(CLK_AUDIO_FRAC_1, "clk_audio_frac_1", "clk_audio_frac_1_src", 0,
			RK3538_CLKSEL_CON(18), CLK_FRAC_DIVIDER_NO_LIMIT,
			RK3538_CLKGATE_CON(0), 12, GFLAGS),
	COMPOSITE(SCLK_UART0_SRC, "sclk_uart0_src", sclk_uart_src_p, 0,
			RK3538_CLKSEL_CON(3), 9, 3, MFLAGS, 4, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 13, GFLAGS),
	COMPOSITE_DIV_OFFSET(SCLK_UART1, "sclk_uart1", sclk_uart_src_p, 0,
			RK3538_CLKSEL_CON(3), 12, 3, MFLAGS,
			RK3538_CLKSEL_CON(4), 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 14, GFLAGS),
	COMPOSITE(SCLK_UART2, "sclk_uart2", sclk_uart_src_p, 0,
			RK3538_CLKSEL_CON(4), 13, 3, MFLAGS, 8, 5, DFLAGS,
			RK3538_CLKGATE_CON(0), 15, GFLAGS),
	COMPOSITE(SCLK_UART3, "sclk_uart3", sclk_uart_src_p, 0,
			RK3538_CLKSEL_CON(5), 5, 3, MFLAGS, 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(1), 0, GFLAGS),
	COMPOSITE(SCLK_UART4, "sclk_uart4", sclk_uart_src_p, 0,
			RK3538_CLKSEL_CON(5), 13, 3, MFLAGS, 8, 5, DFLAGS,
			RK3538_CLKGATE_CON(1), 1, GFLAGS),
	COMPOSITE(SCLK_UART5, "sclk_uart5", sclk_uart_src_p, 0,
			RK3538_CLKSEL_CON(6), 5, 3, MFLAGS, 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(1), 2, GFLAGS),
	COMPOSITE_DIV_OFFSET(MCLK_SAI0, "mclk_sai0", mclk_sai0_src_p, 0,
			RK3538_CLKSEL_CON(6), 8, 3, MFLAGS,
			RK3538_CLKSEL_CON(7), 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(1), 3, GFLAGS),
	COMPOSITE_DIV_OFFSET(MCLK_SAI1_SRC, "mclk_sai1_src", mclk_sai1_src_p, 0,
			RK3538_CLKSEL_CON(6), 11, 3, MFLAGS,
			RK3538_CLKSEL_CON(8), 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(1), 4, GFLAGS),
	COMPOSITE_DIV_OFFSET(MCLK_SAI2, "mclk_sai2", mclk_sai2_src_p, 0,
			RK3538_CLKSEL_CON(7), 8, 3, MFLAGS,
			RK3538_CLKSEL_CON(9), 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(1), 5, GFLAGS),
	COMPOSITE_DIV_OFFSET(MCLK_SAI3, "mclk_sai3", mclk_sai3_src_p, 0,
			RK3538_CLKSEL_CON(7), 11, 3, MFLAGS,
			RK3538_CLKSEL_CON(10), 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(1), 6, GFLAGS),
	COMPOSITE_DIV_OFFSET(MCLK_SPDIF_TX, "mclk_spdif_tx", mclk_spdif_tx_src_p, 0,
			RK3538_CLKSEL_CON(8), 8, 3, MFLAGS,
			RK3538_CLKSEL_CON(11), 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(1), 7, GFLAGS),
	COMPOSITE_DIV_OFFSET(MCLK_PDM, "mclk_pdm", mclk_pdm_src_p, 0,
			RK3538_CLKSEL_CON(8), 11, 3, MFLAGS,
			RK3538_CLKSEL_CON(12), 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(1), 8, GFLAGS),
	COMPOSITE_DIV_OFFSET(CLKOUT_PDM, "clkout_pdm", clkout_pdm_src_p, 0,
			RK3538_CLKSEL_CON(9), 8, 3, MFLAGS,
			RK3538_CLKSEL_CON(12), 5, 8, DFLAGS,
			RK3538_CLKGATE_CON(1), 9, GFLAGS),

	COMPOSITE(CLK_CORE_PLL_SRC, "clk_core_pll_src", mux_gpll_cpll_p, CLK_IS_CRITICAL,
			RK3538_CLKSEL_CON(19), 3, 1, MFLAGS, 0, 3, DFLAGS,
			RK3538_CLKGATE_CON(2), 0, GFLAGS),
	COMPOSITE(CLK_GPU_PLL_SRC, "clk_gpu_pll_src", mux_gpll_cpll_p, CLK_IS_CRITICAL,
			RK3538_CLKSEL_CON(19), 7, 1, MFLAGS, 4, 3, DFLAGS,
			RK3538_CLKGATE_CON(2), 1, GFLAGS),
	COMPOSITE(CLK_RKVDEC_PLL_SRC, "clk_rkvdec_pll_src", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(19), 13, 1, MFLAGS, 8, 5, DFLAGS,
			RK3538_CLKGATE_CON(2), 3, GFLAGS),
	COMPOSITE(CLK_RKVDEC_HEVC_CA_SRC, "clk_rkvdec_hevc_ca_src", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(20), 5, 1, MFLAGS, 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(2), 4, GFLAGS),
	COMPOSITE(DCLK_VP0_SRC, "dclk_vp0_src", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(21), 8, 1, MFLAGS, 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(2), 5, GFLAGS),
	COMPOSITE(ACLK_VO_ROOT_SRC, "aclk_vo_root", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(20), 11, 1, MFLAGS, 6, 5, DFLAGS,
			RK3538_CLKGATE_CON(2), 6, GFLAGS),
	COMPOSITE_NOMUX(CLK_HDMITX_EARC_HIGH_SRC, "clk_hdmitx_earc_high_src", "gpll", 0,
			RK3538_CLKSEL_CON(20), 12, 3, DFLAGS,
			RK3538_CLKGATE_CON(2), 7, GFLAGS),
	COMPOSITE(MCLK_SPDIF_ARC_RX_SRC, "mclk_spdif_arc_rx_src", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(21), 14, 1, MFLAGS, 9, 5, DFLAGS,
			RK3538_CLKGATE_CON(2), 8, GFLAGS),
	GATE(CLK_50M_VO, "clk_50m_vo", "clk_cpll_div20", 0,
			RK3538_CLKGATE_CON(2), 9, GFLAGS),
	COMPOSITE(CLK_MAC_PTP_REF_SRC, "clk_mac_ptp_ref_src", mux_cpll_24m_p, 0,
			RK3538_CLKSEL_CON(22), 5, 1, MFLAGS, 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(2), 10, GFLAGS),
	COMPOSITE(CLK_CORE_RGA, "clk_core_rga", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(22), 11, 1, MFLAGS, 6, 5, DFLAGS,
			RK3538_CLKGATE_CON(2), 11, GFLAGS),
	COMPOSITE(CLK_CORE_VDPP, "clk_core_vdpp", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(23), 5, 1, MFLAGS, 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(2), 12, GFLAGS),
	GATE(CLK1X_DDRPHY_PLL_SRC, "clk1x_ddrphy_pll_src", "clk_gpll_div3", 0,
			RK3538_CLKGATE_CON(2), 13, GFLAGS),
	COMPOSITE(CCLK_SDMMC1, "cclk_sdmmc1", mux_gpll_cpll_24m_p, 0,
			RK3538_CLKSEL_CON(23), 14, 2, MFLAGS, 6, 8, DFLAGS,
			RK3538_CLKGATE_CON(2), 14, GFLAGS),
	COMPOSITE(CCLK_SDIO, "cclk_sdio", mux_gpll_cpll_24m_p, 0,
			RK3538_CLKSEL_CON(24), 8, 2, MFLAGS, 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(2), 15, GFLAGS),
	COMPOSITE(CLK_SARADC_SRC, "clk_saradc_src", mux_200m_24m_p, 0,
			RK3538_CLKSEL_CON(24), 14, 1, MFLAGS, 10, 4, DFLAGS,
			RK3538_CLKGATE_CON(3), 0, GFLAGS),
	COMPOSITE(CLK_GMAC_PTP_REF_SRC, "clk_gmac_ptp_ref_src", mux_cpll_24m_p, 0,
			RK3538_CLKSEL_CON(25), 5, 1, MFLAGS, 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(3), 1, GFLAGS),
	GATE(CLK_GMAC_50M, "clk_gmac_50m", "clk_cpll_div20", 0,
			RK3538_CLKGATE_CON(3), 2, GFLAGS),
	COMPOSITE_NOMUX(CLK_GMAC_125M, "clk_gmac_125m", "cpll", 0,
			RK3538_CLKSEL_CON(25), 6, 5, DFLAGS,
			RK3538_CLKGATE_CON(3), 3, GFLAGS),
	COMPOSITE(CCLK_SDMMC0, "cclk_sdmmc0", mux_gpll_cpll_24m_p, 0,
			RK3538_CLKSEL_CON(26), 8, 2, MFLAGS, 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(3), 4, GFLAGS),
	COMPOSITE(CCLK_EMMC, "cclk_emmc", mux_gpll_cpll_24m_p, 0,
			RK3538_CLKSEL_CON(27), 8, 2, MFLAGS, 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(3), 5, GFLAGS),
	COMPOSITE_NODIV(BCLK_EMMC, "bclk_emmc", mux_200m_100m_50m_24m_p, 0,
			RK3538_CLKSEL_CON(27), 10, 2, MFLAGS,
			RK3538_CLKGATE_CON(3), 6, GFLAGS),
	COMPOSITE(SCLK_2X_FSPI, "sclk_2x_fspi_src", mux_gpll_cpll_24m_p, 0,
			RK3538_CLKSEL_CON(28), 8, 2, MFLAGS, 0, 8, DFLAGS,
			RK3538_CLKGATE_CON(3), 7, GFLAGS),
	GATE(PCLK_TOP_ROOT, "pclk_top_root", "clk_cpll_div10", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(3), 8, GFLAGS),
	COMPOSITE_NOMUX(MCLK_SAI0_OUT2IO, "mclk_sai0_out2io", "mclk_sai0", CLK_SET_RATE_PARENT,
			RK3538_CLKSEL_CON(28), 10, 4, DFLAGS,
			RK3538_CLKGATE_CON(3), 9, GFLAGS),
	COMPOSITE_NOMUX(MCLK_SAI1_OUT2IO, "mclk_sai1_out2io", "mclk_sai1_src", CLK_SET_RATE_PARENT,
			RK3538_CLKSEL_CON(29), 0, 4, DFLAGS,
			RK3538_CLKGATE_CON(3), 10, GFLAGS),
	COMPOSITE_NOMUX(MCLK_SAI2_OUT2IO, "mclk_sai2_out2io", "mclk_sai2", CLK_SET_RATE_PARENT,
			RK3538_CLKSEL_CON(29), 4, 4, DFLAGS,
			RK3538_CLKGATE_CON(3), 11, GFLAGS),
	COMPOSITE_NOMUX(MCLK_SAI3_OUT2IO, "mclk_sai3_out2io", "mclk_sai3", CLK_SET_RATE_PARENT,
			RK3538_CLKSEL_CON(29), 8, 4, DFLAGS,
			RK3538_CLKGATE_CON(3), 12, GFLAGS),

	/* pd_gpu */
	GATE(PCLK_GPU_ROOT, "pclk_gpu_root", "clk_cpll_div10", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 0, GFLAGS),
	GATE(CLK_GPU_CORE, "clk_gpu_core", "clk_gpu_pll_src", 0,
			RK3538_GPUCRU_CLKGATE_CON(0), 7, GFLAGS),

	/* pd_rkvdec */
	GATE(HCLK_RKVDEC_ROOT, "hclk_rkvdec_root", "clk_gpll_div8", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 1, GFLAGS),
	MUX(CLK_RKVDEC_SRC, "clk_rkvdec_src", clk_rkvdec_src_p, CLK_SET_RATE_PARENT,
			RK3538_RKVDECCRU_CLKSEL_CON(0), 1, 1, MFLAGS),
	MUX(CLK_HEVC_CA_RKVDEC_SRC, "clk_hevc_ca_rkvdec_src", clk_hevc_ca_rkvdec_src_p, CLK_SET_RATE_PARENT,
			RK3538_RKVDECCRU_CLKSEL_CON(0), 2, 1, MFLAGS),
	GATE(CLK_RKVDEC_ROOT, "clk_rkvdec_root", "clk_rkvdec_src", 0,
			RK3538_RKVDECCRU_CLKGATE_CON(0), 2, GFLAGS),
	GATE(ACLK_RKVDEC, "aclk_rkvdec", "clk_rkvdec_root", 0,
			RK3538_RKVDECCRU_CLKGATE_CON(0), 6, GFLAGS),
	GATE(HCLK_RKVDEC, "hclk_rkvdec", "hclk_rkvdec_root", 0,
			RK3538_RKVDECCRU_CLKGATE_CON(0), 7, GFLAGS),
	GATE(CLK_HEVC_CA_RKVDEC, "clk_hevc_ca_rkvdec", "clk_hevc_ca_rkvdec_src", 0,
			RK3538_RKVDECCRU_CLKGATE_CON(0), 8, GFLAGS),

	/* pd_vo */
	COMPOSITE_NODIV(PCLK_VO_ROOT, "pclk_vo_root", mux_150m_100m_p, CLK_IS_CRITICAL,
			RK3538_CLKSEL_CON(1), 15, 1, MFLAGS,
			RK3538_CLKGATE_CON(6), 2, GFLAGS),
	GATE(ACLK_VOL_ROOT, "aclk_vol_root", "clk_gpll_div6", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 3, GFLAGS),
	GATE(ACLK_VOP, "aclk_vop", "aclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 3, GFLAGS),
	GATE(HCLK_VOP, "hclk_vop", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 4, GFLAGS),
	COMPOSITE_NODIV(DCLK_VP0, "dclk_vp0", dclk_vp0_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
			RK3538_VOCRU_CLKSEL_CON(0), 14, 2, MFLAGS,
			RK3538_VOCRU_CLKGATE_CON(0), 5, GFLAGS),
	GATE(ACLK_HDCP, "aclk_hdcp", "aclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 6, GFLAGS),
	GATE(HCLK_HDCP, "hclk_hdcp", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 7, GFLAGS),
	GATE(PCLK_HDCP, "pclk_hdcp", "pclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 8, GFLAGS),
	GATE(PCLK_ACODEC, "pclk_acodec", "pclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 9, GFLAGS),
	COMPOSITE_NOMUX(MCLK_ACODEC, "mclk_acodec", "mclk_sai2", CLK_SET_RATE_PARENT,
			RK3538_VOCRU_CLKSEL_CON(0), 1, 8, DFLAGS,
			RK3538_VOCRU_CLKGATE_CON(0), 10, GFLAGS),
	GATE(PCLK_HDMITXPHY, "pclk_hdmitxphy", "pclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 11, GFLAGS),
	GATE(PCLK_HDMITX, "pclk_hdmitx", "pclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 12, GFLAGS),
	COMPOSITE_NODIV(CLK_IREFCLK_HDMITX, "clk_irefclk_hdmitx", clk_irefclk_hdmitx_p, 0,
			RK3538_VOCRU_CLKSEL_CON(1), 14, 1, MFLAGS,
			RK3538_VOCRU_CLKGATE_CON(0), 13, GFLAGS),
	GATE(CLK_HDMITX_EARC, "clk_hdmitx_earc", "clk_cpll_div10", 0,
			RK3538_CLKGATE_CON(6), 4, GFLAGS),
	GATE(CLK_EARCRXPHY_BIST, "clk_earcrxphy_bist", "xin24m", 0,
			RK3538_VOCRU_CLKGATE_CON(2), 10, GFLAGS),
	COMPOSITE(CLK_EARCRXPHY_TERM, "clk_earcrxphy_term", mux_cpll_24m_p, 0,
			RK3538_CLKSEL_CON(34), 0, 1, MFLAGS, 1, 6, DFLAGS,
			RK3538_CLKGATE_CON(7), 7, GFLAGS),
	DIV(CLK_HDMITX_EARC_HIGH_FOR_IREFCLK, "clk_hdmitx_earc_high_for_irefclk", "clk_hdmitx_earc_high_src", 0,
			RK3538_VOCRU_CLKSEL_CON(1), 12, 2, DFLAGS),
	GATE(HCLK_SAI3_HDMI_TX, "hclk_sai3_hdmi_tx", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 15, GFLAGS),
	GATE(HCLK_SAI2, "hclk_sai2", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 0, GFLAGS),
	GATE(HCLK_SPDIF_ARC_RX, "hclk_spdif_arc_rx", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 1, GFLAGS),
	GATE(HCLK_CVBS, "hclk_cvbs", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 2, GFLAGS),
	GATE(PCLK_VDACPHY, "pclk_vdacphy", "pclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 5, GFLAGS),
	GATE(ACLK_MAC, "aclk_mac", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 6, GFLAGS),
	GATE(PCLK_MAC, "pclk_mac", "pclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 7, GFLAGS),
	MUX(CLK_MAC_PTP_REF, "clk_mac_ptp_ref", clk_mac_ptp_ref_p, 0,
			RK3538_VOCRU_CLKSEL_CON(0), 9, 1, MFLAGS),
	GATE(PCLK_MACPHY, "pclk_macphy", "pclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 8, GFLAGS),
	COMPOSITE_NODIV(CLK_IN_50M_MACPHY, "clk_in_50m_macphy", clk_in_50m_macphy_p, 0,
			RK3538_VOCRU_CLKSEL_CON(0), 10, 1, MFLAGS,
			RK3538_VOCRU_CLKGATE_CON(1), 9, GFLAGS),
	GATE(CLK_RMII_MACPHY_IO, "clk_rmii_macphy_io", "clk_50m_vo", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 10, GFLAGS),
	GATE(CLK_RMII_MACPHY_CRU, "clk_rmii_macphy_cru", "clk_50m_vo", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 11, GFLAGS),
	GATE(ACLK_USB2OTG, "aclk_usb2otg", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 12, GFLAGS),
	GATE(CLK_REF_USB2OTG, "clk_ref_usb2otg", "xin24m", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 13, GFLAGS),
	GATE(CLK_SUSPEND_USB2OTG, "clk_suspend_usb2otg", "xin24m", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 14, GFLAGS),
	GATE(HCLK_USB2HOST, "hclk_usb2host", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 15, GFLAGS),
	GATE(HCLK_ARB_USB2HOST, "hclk_arb_usb2host", "aclk_vol_root", 0,
			RK3538_VOCRU_CLKGATE_CON(2), 0, GFLAGS),
	GATE(PCLK_USB2PHY, "pclk_usb2phy", "pclk_vo_root", 0,
			RK3538_VOCRU_CLKGATE_CON(2), 1, GFLAGS),
	MUX(CLK_IN_MACPHY, "clk_in_macphy", clk_in_macphy_p, 0,
			RK3538_VOCRU_CLKSEL_CON(1), 10, 1, MFLAGS),

	FACTOR_GATE(DCLK_CVBS, "dclk_cvbs", "dclk_vp0", 0, 1, 4,
	            RK3538_VOCRU_CLKGATE_CON(1), 3, GFLAGS),
	GATE(DCLK_4X_CVBS, "dclk_4x_cvbs", "dclk_vp0", 0,
			RK3538_VOCRU_CLKGATE_CON(1), 4, GFLAGS),

	/* pd_subddr */
	MUX(CLK1X_PLL_ROOT, "clk1x_pll_root", clk1x_pll_root_p, CLK_IS_CRITICAL,
			RK3538_SUBDDRCRU_CLKSEL_CON(0), 0, 1, MFLAGS),
	GATE(ACLK_DMA2DDR, "aclk_dma2ddr", "clk1x_pll_root", CLK_IS_CRITICAL,
			RK3538_SUBDDRCRU_CLKGATE_CON(0), 9, GFLAGS),
	GATE(PCLK_DDR_ROOT, "pclk_ddr_root", "clk_cpll_div10", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 5, GFLAGS),
	GATE(PCLK_DMA2DDR, "pclk_dma2ddr", "pclk_ddr_root", CLK_IS_CRITICAL,
			RK3538_DDRCRU_CLKGATE_CON(0), 7, GFLAGS),

	/* pd_vpu */
	GATE(HCLK_VPU_ROOT, "hclk_vpu_root", "clk_gpll_div8", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 6, GFLAGS),
	COMPOSITE(ACLK_VPU_ROOT, "aclk_vpu_root", mux_gpll_cpll_p, 0,
			RK3538_CLKSEL_CON(33), 5, 1, MFLAGS, 0, 5, DFLAGS,
			RK3538_CLKGATE_CON(6), 7, GFLAGS),
	GATE(ACLK_VPU, "aclk_vpu", "aclk_vpu_root", 0,
			RK3538_VPUCRU_CLKGATE_CON(0), 3, GFLAGS),
	GATE(HCLK_VPU, "hclk_vpu", "hclk_vpu_root", 0,
			RK3538_VPUCRU_CLKGATE_CON(0), 4, GFLAGS),
	GATE(ACLK_RKJPEG, "aclk_rkjpeg", "aclk_vpu_root", 0,
			RK3538_VPUCRU_CLKGATE_CON(0), 5, GFLAGS),
	GATE(HCLK_RKJPEG, "hclk_rkjpeg", "hclk_vpu_root", 0,
			RK3538_VPUCRU_CLKGATE_CON(0), 6, GFLAGS),
	GATE(ACLK_RGA, "aclk_rga", "aclk_vpu_root", 0,
			RK3538_VPUCRU_CLKGATE_CON(0), 7, GFLAGS),
	GATE(HCLK_RGA, "hclk_rga", "hclk_vpu_root", 0,
			RK3538_VPUCRU_CLKGATE_CON(0), 8, GFLAGS),
	GATE(ACLK_VDPP, "aclk_vdpp", "aclk_vpu_root", 0,
			RK3538_VPUCRU_CLKGATE_CON(0), 9, GFLAGS),
	GATE(HCLK_VDPP, "hclk_vdpp", "hclk_vpu_root", 0,
			RK3538_VPUCRU_CLKGATE_CON(0), 10, GFLAGS),

	/* pd_phpl */
	GATE(HCLK_PHPL_ROOT, "hclk_phpl_root", "clk_gpll_div6", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 8, GFLAGS),
	GATE(PCLK_PHPL_ROOT, "pclk_phpl_root", "clk_cpll_div10", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 9, GFLAGS),
	GATE(HCLK_SDMMC1, "hclk_sdmmc1", "hclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 2, GFLAGS),
	GATE(HCLK_SDIO, "hclk_sdio", "hclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 3, GFLAGS),
	GATE(PCLK_SARADC, "pclk_saradc", "pclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 4, GFLAGS),
	MUX(CLK_SARADC, "clk_saradc", clk_saradc_p, 0,
			RK3538_PHPLCRU_CLKSEL_CON(1), 0, 1, MFLAGS),
	GATE(PCLK_GMAC, "pclk_gmac", "pclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 5, GFLAGS),
	GATE(ACLK_GMAC, "aclk_gmac", "hclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 6, GFLAGS),
	MUX(CLK_GMAC_PTP_REF, "clk_gmac_ptp_ref", clk_gmac_ptp_ref_p, 0,
			RK3538_PHPLCRU_CLKSEL_CON(1), 1, 1, MFLAGS),
	GATE(CLK_GMAC_50M_IOBUF, "clk_gmac_50m_iobuf", "clk_gmac_50m", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 7, GFLAGS),
	GATE(PCLK_TSADC, "pclk_tsadc", "pclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 8, GFLAGS),
	GATE(CLK_TSADC, "clk_tsadc", "xin24m", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 9, GFLAGS),
	GATE(CLK_TSADC_PHYCTRL, "clk_tsadc_phyctrl", "xin24m", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 10, GFLAGS),
	GATE(PCLK_GPIO3, "pclk_gpio3", "pclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 11, GFLAGS),
	GATE(DBCLK_GPIO3, "dbclk_gpio3", "xin24m", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 12, GFLAGS),
	GATE(PCLK_GPIO4, "pclk_gpio4", "pclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 13, GFLAGS),
	GATE(DBCLK_GPIO4, "dbclk_gpio4", "xin24m", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 14, GFLAGS),
	GATE(PCLK_GPIO5, "pclk_gpio5", "pclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(1), 15, GFLAGS),
	GATE(DBCLK_GPIO5, "dbclk_gpio5", "xin24m", 0,
			RK3538_PHPLCRU_CLKGATE_CON(2), 0, GFLAGS),
	GATE(PCLK_IOC_VCCIO3, "pclk_ioc_vccio3", "pclk_phpl_root", CLK_IS_CRITICAL,
			RK3538_PHPLCRU_CLKGATE_CON(2), 1, GFLAGS),
	GATE(PCLK_IOC_VCCIO4, "pclk_ioc_vccio4", "pclk_phpl_root", CLK_IS_CRITICAL,
			RK3538_PHPLCRU_CLKGATE_CON(2), 2, GFLAGS),
	GATE(PCLK_IOC_VCCIO5, "pclk_ioc_vccio5", "pclk_phpl_root", CLK_IS_CRITICAL,
			RK3538_PHPLCRU_CLKGATE_CON(2), 3, GFLAGS),
	GATE(PCLK_SPI1, "pclk_spi1", "pclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(2), 9, GFLAGS),
	COMPOSITE_NODIV(CLK_SPI1, "clk_spi1", mux_300m_200m_100m_24m_p, 0,
			RK3538_CLKSEL_CON(33), 13, 2, MFLAGS,
			RK3538_CLKGATE_CON(7), 3, GFLAGS),
	GATE(HCLK_SAI0, "hclk_sai0", "hclk_phpl_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(2), 10, GFLAGS),

	/* pd_phpr */
	GATE(HCLK_PHPR_ROOT, "hclk_phpr_root", "clk_gpll_div6", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 10, GFLAGS),
	GATE(PCLK_PHPR_ROOT, "pclk_phpr_root", "clk_cpll_div10", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 11, GFLAGS),
	GATE(HCLK_NANDC, "hclk_nandc", "hclk_phpr_root", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 2, GFLAGS),
	COMPOSITE_NODIV(NCLK_NANDC, "nclk_nandc", mux_300m_200m_150m_100m_p, 0,
			RK3538_CLKSEL_CON(33), 6, 2, MFLAGS,
			RK3538_CLKGATE_CON(6), 12, GFLAGS),
	GATE(HCLK_SDMMC0, "hclk_sdmmc0", "hclk_phpr_root", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 3, GFLAGS),
	GATE(HCLK_EMMC, "hclk_emmc", "hclk_phpr_root", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 4, GFLAGS),
	GATE(ACLK_EMMC, "aclk_emmc", "hclk_phpr_root", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 5, GFLAGS),
	GATE(HCLK_FSPI, "hclk_fspi", "hclk_phpr_root", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 6, GFLAGS),
	GATE(PCLK_GPIO1, "pclk_gpio1", "pclk_phpr_root", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 7, GFLAGS),
	GATE(DBCLK_GPIO1, "dbclk_gpio1", "xin24m", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 8, GFLAGS),
	GATE(PCLK_GPIO2, "pclk_gpio2", "pclk_phpr_root", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 9, GFLAGS),
	GATE(DBCLK_GPIO2, "dbclk_gpio2", "xin24m", 0,
			RK3538_PHPRCRU_CLKGATE_CON(0), 10, GFLAGS),
	GATE(PCLK_IOC_VCCIO1, "pclk_ioc_vccio1", "pclk_phpr_root", CLK_IS_CRITICAL,
			RK3538_PHPRCRU_CLKGATE_CON(0), 11, GFLAGS),
	GATE(PCLK_IOC_VCCIO2, "pclk_ioc_vccio2", "pclk_phpr_root", CLK_IS_CRITICAL,
			RK3538_PHPRCRU_CLKGATE_CON(0), 12, GFLAGS),

	/* pd_bus */
	GATE(PCLK_BUS_ROOT, "pclk_bus_root", "clk_cpll_div10", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 13, GFLAGS),
	GATE(HCLK_BUS_ROOT, "hclk_bus_root", "clk_gpll_div8", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(6), 14, GFLAGS),
	COMPOSITE_NODIV(ACLK_BUS_ROOT, "aclk_bus_root", mux_300m_250m_200m_p, CLK_IS_CRITICAL,
			RK3538_CLKSEL_CON(33), 8, 2, MFLAGS,
			RK3538_CLKGATE_CON(6), 15, GFLAGS),
	COMPOSITE_NODIV(CLK_I2C_BUS_ROOT, "clk_i2c_bus_root", mux_200m_24m_p, 0,
			RK3538_CLKSEL_CON(33), 10, 1, MFLAGS,
			RK3538_CLKGATE_CON(7), 0, GFLAGS),
	COMPOSITE_NODIV(CLK_TIMER_ROOT, "clk_timer_root", mux_100m_24m_p, 0,
			RK3538_CLKSEL_CON(33), 11, 1, MFLAGS,
			RK3538_CLKGATE_CON(7), 1, GFLAGS),
	GATE(PCLK_UART1, "pclk_uart1", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 3, GFLAGS),
	GATE(PCLK_UART2, "pclk_uart2", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 4, GFLAGS),
	GATE(PCLK_UART3, "pclk_uart3", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 5, GFLAGS),
	GATE(PCLK_UART4, "pclk_uart4", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 6, GFLAGS),
	GATE(PCLK_UART5, "pclk_uart5", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 7, GFLAGS),
	GATE(PCLK_I2C1, "pclk_i2c1", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 8, GFLAGS),
	GATE(CLK_I2C1, "clk_i2c1", "clk_i2c_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 9, GFLAGS),
	GATE(PCLK_I2C2, "pclk_i2c2", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 10, GFLAGS),
	GATE(CLK_I2C2, "clk_i2c2", "clk_i2c_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 11, GFLAGS),
	GATE(PCLK_I2C3, "pclk_i2c3", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 12, GFLAGS),
	GATE(CLK_I2C3, "clk_i2c3", "clk_i2c_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 13, GFLAGS),
	GATE(PCLK_I2C4, "pclk_i2c4", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 14, GFLAGS),
	GATE(CLK_I2C4, "clk_i2c4", "clk_i2c_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(0), 15, GFLAGS),
	GATE(PCLK_I2C5, "pclk_i2c5", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 0, GFLAGS),
	GATE(CLK_I2C5, "clk_i2c5", "clk_i2c_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 1, GFLAGS),
	GATE(PCLK_RKDMA, "pclk_rkdma", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 2, GFLAGS),
	GATE(ACLK_RKDMA, "aclk_rkdma", "aclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 3, GFLAGS),
	GATE(PCLK_PWM1, "pclk_pwm1", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 4, GFLAGS),
	COMPOSITE_NODIV(CLK_PWM1, "clk_pwm1", mux_100m_24m_p, 0,
			RK3538_CLKSEL_CON(33), 12, 1, MFLAGS,
			RK3538_CLKGATE_CON(7), 2, GFLAGS),
	GATE(CLK_OSC_PWM1, "clk_osc_pwm1", "xin24m", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 5, GFLAGS),
	GATE(CLK_RC_PWM1, "clk_rc_pwm1", "xin24m", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 6, GFLAGS),
	GATE(HCLK_RKSP, "hclk_rksp", "hclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(3), 3, GFLAGS),
	GATE(ACLK_RKSP, "aclk_rksp", "aclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(3), 4, GFLAGS),
	GATE(HCLK_SAI1, "hclk_sai1", "hclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 8, GFLAGS),
	MUX(MCLK_SAI1, "mclk_sai1", mclk_sai1_p, CLK_SET_RATE_PARENT,
			RK3538_BUSCRU_CLKSEL_CON(0), 12, 1, MFLAGS),
	GATE(HCLK_SPDIF_TX, "hclk_spdif_tx", "hclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 9, GFLAGS),
	GATE(HCLK_PDM, "hclk_pdm", "hclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 10, GFLAGS),
	GATE(ACLK_GIC400, "aclk_gic400", "aclk_bus_root", CLK_IS_CRITICAL,
			RK3538_BUSCRU_CLKGATE_CON(1), 11, GFLAGS),
	GATE(PCLK_INTMUX, "pclk_intmux", "pclk_bus_root", CLK_IS_CRITICAL,
			RK3538_BUSCRU_CLKGATE_CON(1), 12, GFLAGS),
	GATE(PCLK_DCF, "pclk_dcf", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 13, GFLAGS),
	GATE(ACLK_DCF, "aclk_dcf", "aclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 14, GFLAGS),
	GATE(ACLK_SPINLOCK, "aclk_spinlock", "aclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(1), 15, GFLAGS),
	GATE(PCLK_SPI0, "pclk_spi0", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 1, GFLAGS),
	COMPOSITE_NODIV(CLK_SPI0, "clk_spi0", mux_300m_200m_100m_24m_p, 0,
			RK3538_CLKSEL_CON(32), 10, 2, MFLAGS,
			RK3538_CLKGATE_CON(7), 4, GFLAGS),
	GATE(PCLK_WDT, "pclk_wdt", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 2, GFLAGS),
	GATE(TCLK_WDT, "tclk_wdt", "xin24m", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 3, GFLAGS),
	GATE(PCLK_TIMER, "pclk_timer", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 4, GFLAGS),
	GATE(CLK_TIMER0, "clk_timer0", "clk_timer_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 5, GFLAGS),
	GATE(CLK_TIMER1, "clk_timer1", "clk_timer_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 6, GFLAGS),
	GATE(CLK_TIMER2, "clk_timer2", "clk_timer_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 7, GFLAGS),
	GATE(CLK_TIMER3, "clk_timer3", "clk_timer_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 8, GFLAGS),
	GATE(CLK_TIMER4, "clk_timer4", "clk_timer_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 9, GFLAGS),
	GATE(CLK_TIMER5, "clk_timer5", "clk_timer_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 10, GFLAGS),
	GATE(PCLK_GPIO6, "pclk_gpio6", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 11, GFLAGS),
	GATE(DBCLK_GPIO6, "dbclk_gpio6", "xin24m", 0,
			RK3538_BUSCRU_CLKGATE_CON(2), 12, GFLAGS),
	GATE(PCLK_IOC_VCCIO6, "pclk_ioc_vccio6", "pclk_bus_root", CLK_IS_CRITICAL,
			RK3538_BUSCRU_CLKGATE_CON(2), 13, GFLAGS),
	GATE(PCLK_LBISTC, "pclk_lbistc", "pclk_bus_root", 0,
			RK3538_BUSCRU_CLKGATE_CON(3), 5, GFLAGS),
	GATE(PCLK_SECURE_NS_ROOT, "pclk_secure_ns_root", "clk_cpll_div10", 0,
			RK3538_CLKGATE_CON(7), 5, GFLAGS),
	GATE(HCLK_SECURE_NS_ROOT, "hclk_secure_ns_root", "clk_gpll_div8", CLK_IS_CRITICAL,
			RK3538_CLKGATE_CON(7), 6, GFLAGS),
	GATE(HCLK_NS_RKCE, "hclk_ns_rkce", "hclk_secure_ns_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 2, GFLAGS),
	GATE(PCLK_NS_OTPC, "pclk_ns_otpc", "pclk_secure_ns_root", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 3, GFLAGS),
	GATE(CLK_NS_SBPI_OTPC, "clk_ns_sbpi_otpc", "xin24m", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 4, GFLAGS),
	COMPOSITE_NOMUX(CLK_NS_USER_OTPC, "clk_ns_user_otpc", "xin24m", 0,
			RK3538_PHPLCRU_CLKSEL_CON(0), 0, 3, DFLAGS,
			RK3538_PHPLCRU_CLKGATE_CON(0), 5, GFLAGS),
	GATE(ACLK_NSRKCE, "aclk_nsrkce", "clk_spll_div3", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 7, GFLAGS),
	GATE(CLK_PKA_NSRKCE, "clk_pka_nsrkce", "clk_spll_div3", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 8, GFLAGS),
	GATE(HCLK_RKRNG_NS, "hclk_rkrng_ns", "clk_spll_div6", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 9, GFLAGS),
	GATE(CLK_DRNG_NS, "clk_drng_ns", "clk_spll_div6", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 10, GFLAGS),
	GATE(PCLK_NS_OTPC_MASK, "pclk_ns_otpc_mask", "clk_spll_div9", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 11, GFLAGS),
	GATE(CLK_NS_OTPC_ARB, "clk_ns_otpc_arb", "xin24m", 0,
			RK3538_PHPLCRU_CLKGATE_CON(0), 12, GFLAGS),

	/* pd_pmu */
	MUX(0, "clk_xin_rc_src", clk_xin_rc_div_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(1), 8, 1, MFLAGS),
	COMPOSITE_FRAC(CLK_XIN_RC_DIV, "clk_xin_rc_div", "clk_xin_rc_src", CLK_IS_CRITICAL,
			RK3538_PMUCRU_CLKSEL_CON(4), 0,
			RK3538_PMUCRU_CLKGATE_CON(0), 2, GFLAGS),
	MUX(CLK_DEEPSLOW, "clk_32k", clk_32k_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(1), 9, 1, MFLAGS),
	COMPOSITE(CLK_REFOUT_PLL, "clk_refout_pll", mux_gpll_cpll_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(3), 13, 1, MFLAGS, 7, 6, DFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(2), 10, GFLAGS),
	COMPOSITE_NODIV(CLK_REFOUT, "clk_refout", clk_refout_p, CLK_SET_RATE_PARENT,
			RK3538_PMUCRU_CLKSEL_CON(3), 14, 2, MFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(2), 12, GFLAGS),
	COMPOSITE_NOMUX(ETH0_CLK_25M_OUT, "eth0_clk_25m_out", "cpll", 0,
			RK3538_PMUCRU_CLKSEL_CON(0), 1, 6, DFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(0), 2, GFLAGS),
	COMPOSITE(PMU_100M_CLK, "pmu_100m_clk", mux_gpll_cpll_p, CLK_IS_CRITICAL,
			RK3538_PMUCRU_CLKSEL_CON(1), 4, 1, MFLAGS, 0, 4, DFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(0), 4, GFLAGS),
	COMPOSITE_NOGATE(BUSCLK_PMU_PRE, "busclk_pmu_pre", busclk_pmu_pre_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(1), 7, 1, MFLAGS, 5, 2, DFLAGS),
	GATE(BUSCLK_PMU_ROOT, "busclk_pmu_root", "busclk_pmu_pre", CLK_IS_CRITICAL,
			RK3538_PMUCRU_CLKGATE_CON(0), 5, GFLAGS),
	GATE(PCLK_PMU, "pclk_pmu", "busclk_pmu_root", CLK_IS_CRITICAL,
			RK3538_PMUCRU_CLKGATE_CON(0), 8, GFLAGS),
	GATE(CLK_PMU_32K, "clk_pmu_32k", "clk_32k", CLK_IS_CRITICAL,
			RK3538_PMUCRU_CLKGATE_CON(2), 7, GFLAGS),
	GATE(PCLK_UART0, "pclk_uart0", "busclk_pmu_root", 0,
			RK3538_PMUCRU_CLKGATE_CON(0), 9, GFLAGS),
	COMPOSITE_NODIV(SCLK_UART0, "sclk_uart0", sclk_uart0_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(1), 10, 2, MFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(0), 10, GFLAGS),
	GATE(PCLK_I2C0, "pclk_i2c0", "busclk_pmu_root", 0,
			RK3538_PMUCRU_CLKGATE_CON(0), 11, GFLAGS),
	COMPOSITE(CLK_I2C0, "clk_i2c0", clk_i2c0_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(1), 14, 2, MFLAGS, 12, 2, DFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(0), 12, GFLAGS),
	GATE(PCLK_PMU_GPIO0, "pclk_pmu_gpio0", "busclk_pmu_root", 0,
			RK3538_PMUCRU_CLKGATE_CON(0), 13, GFLAGS),
	COMPOSITE_NODIV(DBCLK_PMU_GPIO0, "dbclk_pmu_gpio0", mux_24m_32k_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(0), 14, 1, MFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(0), 14, GFLAGS),
	GATE(PCLK_CEC, "pclk_cec", "busclk_pmu_root", 0,
			RK3538_PMUCRU_CLKGATE_CON(0), 15, GFLAGS),
	GATE(CLK_32K_CEC, "clk_32k_cec", "clk_32k", 0,
			RK3538_PMUCRU_CLKGATE_CON(1), 0, GFLAGS),
	GATE(PCLK_PWM0, "pclk_pwm0", "busclk_pmu_root", 0,
			RK3538_PMUCRU_CLKGATE_CON(1), 4, GFLAGS),
	COMPOSITE(CLK_PWM0, "clk_pwm0", clk_pwm0_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(2), 2, 2, MFLAGS, 0, 2, DFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(1), 5, GFLAGS),
	GATE(CLK_OSC_PWM0, "clk_osc_pwm0", "xin24m", 0,
			RK3538_PMUCRU_CLKGATE_CON(1), 6, GFLAGS),
	GATE(CLK_RC_PWM0, "clk_rc_pwm0", "clk_32k", 0,
			RK3538_PMUCRU_CLKGATE_CON(1), 7, GFLAGS),
	GATE(PCLK_IOC_PMUIO0, "pclk_ioc_pmuio0", "busclk_pmu_root", CLK_IS_CRITICAL,
			RK3538_PMUCRU_CLKGATE_CON(1), 10, GFLAGS),
	GATE(PCLK_IOC_PMUIO1, "pclk_ioc_pmuio1", "busclk_pmu_root", CLK_IS_CRITICAL,
			RK3538_PMUCRU_CLKGATE_CON(1), 11, GFLAGS),
	GATE(PCLK_WDT_MCU, "pclk_wdt_mcu", "busclk_pmu_root", 0,
			RK3538_PMUCRU_CLKGATE_CON(1), 15, GFLAGS),
	COMPOSITE_NODIV(TCLK_WDT_MCU, "tclk_wdt_mcu", mux_24m_32k_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(2), 4, 1, MFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(2), 0, GFLAGS),
	GATE(CLK_AON_N100, "clk_aon_n100", "busclk_pmu_root", 0,
			RK3538_PMUCRU_CLKGATE_CON(2), 1, GFLAGS),
	COMPOSITE(OSC_CLK_N100, "osc_clk_n100", mux_24m_32k_p, 0,
			RK3538_PMUCRU_CLKSEL_CON(2), 5, 1, MFLAGS, 6, 6, DFLAGS,
			RK3538_PMUCRU_CLKGATE_CON(2), 2, GFLAGS),
	GATE(PCLK_MAILBOX, "pclk_mailbox", "busclk_pmu_root", 0,
			RK3538_PMUCRU_CLKGATE_CON(2), 4, GFLAGS),
	GATE(HCLK_PMU_SRAM, "hclk_pmu_sram", "busclk_pmu_root", CLK_IS_CRITICAL,
			RK3538_PMUCRU_CLKSEL_CON(2), 9, GFLAGS),

	/* dummy */
	MUX(REF_PULSE_CLK_AUDIO_FRAC_0, "ref_pulse_clk_audio_frac_0", mux_ref_pulse_p, 0,
			RK3538_CLKSEL_CON(10), 8, 2, MFLAGS),
	MUX(REF_PULSE_CLK_AUDIO_FRAC_1, "ref_pulse_clk_audio_frac_1", mux_ref_pulse_p, 0,
			RK3538_CLKSEL_CON(10), 10, 2, MFLAGS),
	MUX(REF_PULSE_ROOT, "ref_pulse_root", mux_ref_pulse_p, 0,
			RK3538_CLKSEL_CON(10), 12, 2, MFLAGS),
	GATE(CLK_LINK2PHY_HDMITX, "clk_link2phy_hdmitx", "clk_ihdmitx_link2phy", 0,
			RK3538_VOCRU_CLKGATE_CON(0), 14, GFLAGS),
};

static struct rockchip_clk_branch rk3538_armclk01 __initdata =
	MUX(ARMCLK01, "armclk01", mux_armclk_p, CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
			RK3538_CLKSEL_CON(31), 2, 1, MFLAGS);

static struct rockchip_clk_branch rk3538_armclk23 __initdata =
	MUX(ARMCLK23, "armclk23", mux_armclk_p, CLK_IS_CRITICAL | CLK_SET_RATE_PARENT,
			RK3538_CLKSEL_CON(31), 3, 1, MFLAGS);

static void __iomem *rk3538_cru_base;

static void rk3538_dump_cru(void)
{
	if (rk3538_cru_base) {
		pr_warn("CRU:\n");
		print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET,
			       32, 4, rk3538_cru_base,
			       0x8b8, false);
		pr_warn("BUS CRU:\n");
		print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET,
			       32, 4, rk3538_cru_base + RK3538_BUS_CRU_BASE,
			       0x80c, false);
		pr_warn("PMU CRU:\n");
		print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET,
			       32, 4, rk3538_cru_base + RK3538_PMU_CRU_BASE,
			       0x808, false);
	}
}

static void __init rk3538_clk_init(struct device_node *np)
{
	struct rockchip_clk_provider *ctx;
	void __iomem *reg_base;

	reg_base = of_iomap(np, 0);
	if (!reg_base) {
		pr_err("%s: could not map cru region\n", __func__);
		return;
	}

	rk3538_cru_base = reg_base;

	ctx = rockchip_clk_init(np, reg_base, CLK_NR_CLKS);
	if (IS_ERR(ctx)) {
		pr_err("%s: rockchip clk init failed\n", __func__);
		iounmap(reg_base);
		return;
	}

	rockchip_clk_register_plls(ctx, rk3538_pll_clks,
				   ARRAY_SIZE(rk3538_pll_clks),
				   RK3538_GRF_SOC_STATUS0);

	rockchip_clk_register_branches(ctx, rk3538_clk_branches,
				       ARRAY_SIZE(rk3538_clk_branches));

	rockchip_clk_register_armclk_v2(ctx, &rk3538_armclk01,
					rk3538_cpuclk_rates,
					ARRAY_SIZE(rk3538_cpuclk_rates));

	rockchip_clk_register_armclk_v2(ctx, &rk3538_armclk23,
					rk3538_cpuclk_rates,
					ARRAY_SIZE(rk3538_cpuclk_rates));

	rk3538_rst_init(np, reg_base);
	rockchip_register_restart_notifier(ctx, RK3538_GLB_SRST_FST, NULL);

	rockchip_clk_of_add_provider(np, ctx);

	if (!rk_dump_cru)
		rk_dump_cru = rk3538_dump_cru;
}

CLK_OF_DECLARE(rk3538_cru, "rockchip,rk3538-cru", rk3538_clk_init);

#ifdef MODULE
struct clk_rk3538_inits {
	void (*inits)(struct device_node *np);
};

static const struct clk_rk3538_inits clk_rk3538_init = {
	.inits = rk3538_clk_init,
};

static const struct of_device_id clk_rk3538_match_table[] = {
	{
		.compatible = "rockchip,rk3538-cru",
		.data = &clk_rk3538_init,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, clk_rk3538_match_table);

static int __init clk_rk3538_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	const struct clk_rk3538_inits *init_data;

	match = of_match_device(clk_rk3538_match_table, &pdev->dev);
	if (!match || !match->data)
		return -EINVAL;

	init_data = match->data;
	if (init_data->inits)
		init_data->inits(np);

	return 0;
}

static struct platform_driver clk_rk3538_driver = {
	.driver		= {
		.name	= "clk-rk3538",
		.of_match_table = clk_rk3538_match_table,
	},
};
builtin_platform_driver_probe(clk_rk3538_driver, clk_rk3538_probe);

MODULE_DESCRIPTION("RockchipRK3538 Clock Driver");
MODULE_LICENSE("GPL");
#endif /* MODULE */
