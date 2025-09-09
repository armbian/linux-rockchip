// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Synopsys DesignWare Cores Mobile Storage Host Controller
 *
 * Copyright (C) 2018 Synaptics Incorporated
 *
 * Author: Jisheng Zhang <jszhang@kernel.org>
 */

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/sizes.h>

#include "sdhci-pltfm.h"
#include "mmc_hsq.h"
#include "cqhci.h"
#include "sdhci-cqhci.h"

#define SDHCI_DWCMSHC_ARG2_STUFF	GENMASK(31, 16)

/* DWCMSHC specific Mode Select value */
#define DWCMSHC_CTRL_HS400		0x7

/* DWC IP vendor area 1 pointer */
#define DWCMSHC_P_VENDOR_AREA1		0xe8
#define DWCMSHC_AREA1_MASK		GENMASK(11, 0)
/* Offset inside the  vendor area 1 */
#define DWCMSHC_HOST_CTRL3		0x8
#define DWCMSHC_EMMC_CONTROL		0x2c
#define DWCMSHC_CARD_IS_EMMC		BIT(0)
#define DWCMSHC_ENHANCED_STROBE		BIT(8)
#define DWCMSHC_EMMC_ATCTRL		0x40

/* DWC IP vendor area 2 pointer */
#define DWCMSHC_P_VENDOR_AREA2		0xea

/* Rockchip specific Registers */
#define DWCMSHC_EMMC_DLL_CTRL		0x800
#define DWCMSHC_EMMC_DLL_RXCLK		0x804
#define DWCMSHC_EMMC_DLL_TXCLK		0x808
#define DWCMSHC_EMMC_DLL_STRBIN		0x80c
#define DECMSHC_EMMC_DLL_CMDOUT		0x810
#define DECMSHC_EMMC_MISC_CON		0x81C
#define MISC_INTCLK_EN			BIT(1)
#define DWCMSHC_EMMC_DLL_STATUS0	0x840
#define DWCMSHC_EMMC_DLL_STATUS1	0x844
#define DWCMSHC_EMMC_DLL_START		BIT(0)
#define DWCMSHC_EMMC_DLL_LOCKED		BIT(8)
#define DWCMSHC_EMMC_DLL_TIMEOUT	BIT(9)
#define DWCMSHC_EMMC_DLL_RXCLK_SRCSEL	29
#define DWCMSHC_EMMC_DLL_START_POINT	16
#define DWCMSHC_EMMC_DLL_INC		8
#define DWCMSHC_EMMC_DLL_BYPASS		BIT(24)
#define DWCMSHC_EMMC_DLL_DLYENA		BIT(27)
#define DLL_TAP_VALUE_SEL		BIT(25)
#define DLL_TAP_VALUE_OFFSET		8
#define DLL_TXCLK_TAPNUM_DEFAULT	0x10
#define DLL_TXCLK_TAPNUM_90_DEGREES	0xA
#define DLL_TXCLK_TAPNUM_FROM_SW	BIT(24)
#define DLL_STRBIN_TAPNUM_DEFAULT	0x8
#define DLL_STRBIN_TAPNUM_FROM_SW	BIT(24)
#define DLL_STRBIN_DELAY_NUM_SEL	BIT(26)
#define DLL_STRBIN_DELAY_NUM_OFFSET	16
#define DLL_STRBIN_DELAY_NUM_DEFAULT	0x16
#define DLL_RXCLK_NO_INVERTER		1
#define DLL_RXCLK_INVERTER		0
#define DLL_CMDOUT_TAPNUM_90_DEGREES	0x8
#define DLL_RXCLK_TAPNUM_FROM_SW	BIT(24)
#define DLL_RXCLK_ORI_GATE		BIT(31)
#define DLL_RXCLK_MAX_TAP		32
#define DLL_CMDOUT_TAPNUM_FROM_SW	BIT(24)
#define DLL_CMDOUT_SRC_CLK_NEG		BIT(28)
#define DLL_CMDOUT_EN_SRC_CLK_NEG	BIT(29)
#define DLL_CMDOUT_BOTH_CLK_EDGE	BIT(30)

#define DLL_LOCK_WO_TMOUT(x) \
	((((x) & DWCMSHC_EMMC_DLL_LOCKED) == DWCMSHC_EMMC_DLL_LOCKED) && \
	(((x) & DWCMSHC_EMMC_DLL_TIMEOUT) == 0))
#define RK35xx_MAX_CLKS 3

#define BOUNDARY_OK(addr, len) \
	((addr | (SZ_128M - 1)) == ((addr + len - 1) | (SZ_128M - 1)))

#define DWCMSHC_SDHCI_CQE_TRNS_MODE	(SDHCI_TRNS_MULTI | \
					 SDHCI_TRNS_BLK_CNT_EN | \
					 SDHCI_TRNS_DMA)

enum dwcmshc_rk_type {
	DWCMSHC_RK3568,
	DWCMSHC_RK3588,
};

struct dwcmshc_driver_data {
	const struct sdhci_pltfm_data *pdata;
	u32 flags;
#define RK_PLATFROM		BIT(0)
#define RK_DLL_CMD_OUT		BIT(1)
#define RK_RXCLK_NO_INVERTER	BIT(2)
#define RK_TAP_VALUE_SEL	BIT(3)

	u8 hs200_tx_tap;
	u8 hs400_tx_tap;
	u8 hs400_cmd_tap;
	u8 ddr50_strbin_delay_num;
	u8 hs400_strbin_tap;
};

struct rk35xx_priv {
	/* Rockchip specified optional clocks */
	struct clk_bulk_data rockchip_clks[RK35xx_MAX_CLKS];
	struct reset_control *reset;
	enum dwcmshc_rk_type devtype;
	u8 txclk_tapnum;
	u32 cclk_rate;
	unsigned int actual_clk;
	const struct dwcmshc_driver_data *drv_data;
	u32 acpi_en;
};

struct dwcmshc_priv {
	struct clk	*bus_clk;
	int vendor_specific_area1; /* P_VENDOR_SPECIFIC_AREA1 reg */
	int vendor_specific_area2; /* P_VENDOR_SPECIFIC_AREA2 reg */

	void *priv; /* pointer to SoC private stuff */
};

/*
 * If DMA addr spans 128MB boundary, we split the DMA transfer into two
 * so that each DMA transfer doesn't exceed the boundary.
 */
static void dwcmshc_adma_write_desc(struct sdhci_host *host, void **desc,
				    dma_addr_t addr, int len, unsigned int cmd)
{
	int tmplen, offset;

	if (likely(!len || BOUNDARY_OK(addr, len))) {
		sdhci_adma_write_desc(host, desc, addr, len, cmd);
		return;
	}

	offset = addr & (SZ_128M - 1);
	tmplen = SZ_128M - offset;
	sdhci_adma_write_desc(host, desc, addr, tmplen, cmd);

	addr += tmplen;
	len -= tmplen;
	sdhci_adma_write_desc(host, desc, addr, len, cmd);
}

static unsigned int dwcmshc_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	if (pltfm_host->clk)
		return sdhci_pltfm_clk_get_max_clock(host);
	else
		return pltfm_host->clock;
}

static void dwcmshc_check_auto_cmd23(struct mmc_host *mmc,
				     struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);

	/*
	 * No matter V4 is enabled or not, ARGUMENT2 register is 32-bit
	 * block count register which doesn't support stuff bits of
	 * CMD23 argument on dwcmsch host controller.
	 */
	if (mrq->sbc && (mrq->sbc->arg & SDHCI_DWCMSHC_ARG2_STUFF))
		host->flags &= ~SDHCI_AUTO_CMD23;
	else
		host->flags |= SDHCI_AUTO_CMD23;
}

static void dwcmshc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	dwcmshc_check_auto_cmd23(mmc, mrq);

	sdhci_request(mmc, mrq);
}

static void dwcmshc_set_uhs_signaling(struct sdhci_host *host,
				      unsigned int timing)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u16 ctrl, ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	if ((timing == MMC_TIMING_MMC_HS200) ||
	    (timing == MMC_TIMING_UHS_SDR104))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
	else if (timing == MMC_TIMING_UHS_SDR12)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
	else if ((timing == MMC_TIMING_UHS_SDR25) ||
		 (timing == MMC_TIMING_MMC_HS))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
	else if (timing == MMC_TIMING_UHS_SDR50)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
	else if ((timing == MMC_TIMING_UHS_DDR50) ||
		 (timing == MMC_TIMING_MMC_DDR52))
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
	else if (timing == MMC_TIMING_MMC_HS400) {
		/* set CARD_IS_EMMC bit to enable Data Strobe for HS400 */
		ctrl = sdhci_readw(host, priv->vendor_specific_area1 + DWCMSHC_EMMC_CONTROL);
		ctrl |= DWCMSHC_CARD_IS_EMMC;
		sdhci_writew(host, ctrl, priv->vendor_specific_area1 + DWCMSHC_EMMC_CONTROL);

		ctrl_2 |= DWCMSHC_CTRL_HS400;
	}

	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);
}

static void dwcmshc_hs400_enhanced_strobe(struct mmc_host *mmc,
					  struct mmc_ios *ios)
{
	u32 vendor;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int reg = priv->vendor_specific_area1 + DWCMSHC_EMMC_CONTROL;

	vendor = sdhci_readl(host, reg);
	if (ios->enhanced_strobe)
		vendor |= DWCMSHC_ENHANCED_STROBE;
	else
		vendor &= ~DWCMSHC_ENHANCED_STROBE;

	sdhci_writel(host, vendor, reg);
}

static int dwcmshc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	int err = sdhci_execute_tuning(mmc, opcode);
	struct sdhci_host *host = mmc_priv(mmc);

	if (err)
		return err;

	/*
	 * Tuning can leave the IP in an active state (Buffer Read Enable bit
	 * set) which prevents the entry to low power states (i.e. S0i3). Data
	 * reset will clear it.
	 */
	sdhci_reset(host, SDHCI_RESET_DATA);

	return 0;
}

static u32 dwcmshc_cqe_irq_handler(struct sdhci_host *host, u32 intmask)
{
	int cmd_error = 0;
	int data_error = 0;

	if (!sdhci_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	return 0;
}

static void dwcmshc_sdhci_cqe_enable(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);
	u8 ctrl;

	sdhci_writew(host, DWCMSHC_SDHCI_CQE_TRNS_MODE, SDHCI_TRANSFER_MODE);

	sdhci_cqe_enable(mmc);

	/*
	 * The "DesignWare Cores Mobile Storage Host Controller
	 * DWC_mshc / DWC_mshc_lite Databook" says:
	 * when Host Version 4 Enable" is 1 in Host Control 2 register,
	 * SDHCI_CTRL_ADMA32 bit means ADMA2 is selected.
	 * Selection of 32-bit/64-bit System Addressing:
	 * either 32-bit or 64-bit system addressing is selected by
	 * 64-bit Addressing bit in Host Control 2 register.
	 *
	 * On the other hand the "DesignWare Cores Mobile Storage Host
	 * Controller DWC_mshc / DWC_mshc_lite User Guide" says, that we have to
	 * set DMA_SEL to ADMA2 _only_ mode in the Host Control 2 register.
	 */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	ctrl &= ~SDHCI_CTRL_DMA_MASK;
	ctrl |= SDHCI_CTRL_ADMA32;
	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);
}

static void rk35xx_sdhci_cqe_enable(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	u32 reg;

	/* Set Send Status Command Idle Timer to 10.66us (256 * 1 / 24) */
	reg = sdhci_readl(host, dwc_priv->vendor_specific_area2 + CQHCI_SSC1);
	reg = (reg & ~CQHCI_SSC1_CIT_MASK) | 0x0100;
	sdhci_writel(host, reg, dwc_priv->vendor_specific_area2 + CQHCI_SSC1);

	reg = sdhci_readl(host, dwc_priv->vendor_specific_area2 + CQHCI_CFG);
	reg |= CQHCI_ENABLE;
	sdhci_writel(host, reg, dwc_priv->vendor_specific_area2 + CQHCI_CFG);

	reg = sdhci_readl(host, SDHCI_PRESENT_STATE);
	while (reg & SDHCI_DATA_AVAILABLE) {
		sdhci_readl(host, SDHCI_BUFFER);
		reg = sdhci_readl(host, SDHCI_PRESENT_STATE);
	}

	sdhci_writew(host, DWCMSHC_SDHCI_CQE_TRNS_MODE, SDHCI_TRANSFER_MODE);

	sdhci_cqe_enable(mmc);

	sdhci_writew(host, DWCMSHC_SDHCI_CQE_TRNS_MODE, SDHCI_TRANSFER_MODE);
}

static void rk35xx_sdhci_cqe_disabled(struct mmc_host *mmc, bool recovery)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	unsigned long flags;
	u32 ctrl;

	mmc->cqe_ops->cqe_wait_for_idle(mmc);
	spin_lock_irqsave(&host->lock, flags);

	/*
	 * During CQE command transfers, command complete bit gets latched.
	 * So s/w should clear command complete interrupt status when CQE is
	 * either halted or disabled. Otherwise unexpected SDCHI legacy
	 * interrupt gets triggered when CQE is halted/disabled.
	 */
	ctrl = sdhci_readl(host, SDHCI_INT_ENABLE);
	ctrl |= SDHCI_INT_RESPONSE;
	sdhci_writel(host,  ctrl, SDHCI_INT_ENABLE);
	sdhci_writel(host, SDHCI_INT_RESPONSE, SDHCI_INT_STATUS);

	spin_unlock_irqrestore(&host->lock, flags);

	sdhci_cqe_disable(mmc, recovery);

	ctrl = sdhci_readl(host, dwc_priv->vendor_specific_area2 + CQHCI_CFG);
	ctrl &= ~CQHCI_ENABLE;
	sdhci_writel(host, ctrl, dwc_priv->vendor_specific_area2 + CQHCI_CFG);
}

static void dwcmshc_set_tran_desc(struct cqhci_host *cq_host, u8 **desc,
				  dma_addr_t addr, int len, bool end, bool dma64)
{
	int tmplen, offset;

	if (likely(!len || BOUNDARY_OK(addr, len))) {
		cqhci_set_tran_desc(*desc, addr, len, end, dma64);
		return;
	}

	offset = addr & (SZ_128M - 1);
	tmplen = SZ_128M - offset;
	cqhci_set_tran_desc(*desc, addr, tmplen, false, dma64);

	addr += tmplen;
	len -= tmplen;
	*desc += cq_host->trans_desc_len;
	cqhci_set_tran_desc(*desc, addr, len, end, dma64);
}

static void dwcmshc_cqhci_dumpregs(struct mmc_host *mmc)
{
	sdhci_dumpregs(mmc_priv(mmc));
}

static void dwcmshc_rk3568_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *priv = dwc_priv->priv;
	const struct dwcmshc_driver_data *drv_data = priv->drv_data;
	u8 txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT;
	u32 extra, reg, dll_lock_value;
	int err;

	host->mmc->actual_clock = 0;

	if (clock == 0) {
		/* Disable interface clock at initial state. */
		sdhci_set_clock(host, clock);
		return;
	}

	/* Rockchip platform only support 375KHz for identify mode */
	if (clock <= 400000)
		clock = 375000;

	if (priv->acpi_en) {
		union acpi_object params[1];
		struct acpi_object_list param_objects;

		params[0].type = ACPI_TYPE_INTEGER;
		params[0].integer.value  = clock;
		param_objects.count = 1;
		param_objects.pointer = params;
		acpi_evaluate_object(ACPI_HANDLE(mmc_dev(host->mmc)), "SCLK", &param_objects, NULL);
	} else {
		err = clk_set_rate(pltfm_host->clk, clock);
		if (err)
			dev_err(mmc_dev(host->mmc), "fail to set clock %d", clock);
	}

	sdhci_set_clock(host, clock);

	/* Disable cmd conflict check and disable internal clock gate */
	reg = dwc_priv->vendor_specific_area1 + DWCMSHC_HOST_CTRL3;
	extra = sdhci_readl(host, reg);
	extra &= ~BIT(0);
	extra |= BIT(4);
	sdhci_writel(host, extra, reg);

	/* Disable output clock while config DLL */
	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	if (clock <= 52000000) {
		/* Disable DLL */
		sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_CTRL);
		/*
		 * Config DLL BYPASS and Reset both of sample and drive clock.
		 * The bypass bit and start bit need to set if DLL is not locked.
		 */
		sdhci_writel(host, DWCMSHC_EMMC_DLL_BYPASS | DWCMSHC_EMMC_DLL_START, DWCMSHC_EMMC_DLL_CTRL);
		sdhci_writel(host, DLL_RXCLK_ORI_GATE, DWCMSHC_EMMC_DLL_RXCLK);
		sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_TXCLK);
		sdhci_writel(host, 0, DECMSHC_EMMC_DLL_CMDOUT);
		/*
		 * Before switching to hs400es mode, the driver will enable
		 * enhanced strobe first. PHY needs to configure the parameters
		 * of enhanced strobe first.
		 */
		extra = DWCMSHC_EMMC_DLL_DLYENA |
			DLL_STRBIN_DELAY_NUM_SEL |
			drv_data->ddr50_strbin_delay_num << DLL_STRBIN_DELAY_NUM_OFFSET;
		sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_STRBIN);
		goto exit;
	}

	/* Reset DLL */
	sdhci_writel(host, BIT(1), DWCMSHC_EMMC_DLL_CTRL);
	udelay(1);
	sdhci_writel(host, 0x0, DWCMSHC_EMMC_DLL_CTRL);

	/* Init DLL settings, clean start bit before resetting */
	sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_CTRL);
	/* Init DLL settings */
	extra = 0x5 << DWCMSHC_EMMC_DLL_START_POINT |
		0x2 << DWCMSHC_EMMC_DLL_INC |
		DWCMSHC_EMMC_DLL_START;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_CTRL);
	err = readl_poll_timeout(host->ioaddr + DWCMSHC_EMMC_DLL_STATUS0,
				 extra, DLL_LOCK_WO_TMOUT(extra), 1,
				 500 * USEC_PER_MSEC);
	if (err) {
		dev_err(mmc_dev(host->mmc), "DLL lock timeout!\n");
		goto exit;
	}

	dll_lock_value = ((sdhci_readl(host, DWCMSHC_EMMC_DLL_STATUS0) & 0xFF) * 2) & 0xFF;

	extra = 0x1 << 16 | /* tune clock stop en */
		0x3 << 17 | /* pre-change delay */
		0x3 << 19;  /* post-change delay */
	sdhci_writel(host, extra, dwc_priv->vendor_specific_area1 + DWCMSHC_EMMC_ATCTRL);

	extra = DWCMSHC_EMMC_DLL_DLYENA | DLL_RXCLK_ORI_GATE;
	if (drv_data->flags & RK_RXCLK_NO_INVERTER)
		extra |= DLL_RXCLK_NO_INVERTER << DWCMSHC_EMMC_DLL_RXCLK_SRCSEL;
	if (drv_data->flags & RK_TAP_VALUE_SEL)
		extra |= DLL_TAP_VALUE_SEL | dll_lock_value << DLL_TAP_VALUE_OFFSET;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_RXCLK);

	txclk_tapnum = drv_data->hs200_tx_tap;
	if (host->mmc->ios.timing == MMC_TIMING_MMC_HS400) {
		txclk_tapnum = drv_data->hs400_tx_tap;

		if (drv_data->flags & RK_DLL_CMD_OUT) {
			extra = DLL_CMDOUT_SRC_CLK_NEG |
				DLL_CMDOUT_BOTH_CLK_EDGE |
				DWCMSHC_EMMC_DLL_DLYENA |
				drv_data->hs400_cmd_tap |
				DLL_CMDOUT_TAPNUM_FROM_SW;
			if (drv_data->flags & RK_TAP_VALUE_SEL)
				extra |= DLL_TAP_VALUE_SEL | dll_lock_value << DLL_TAP_VALUE_OFFSET;
			sdhci_writel(host, extra, DECMSHC_EMMC_DLL_CMDOUT);
		}
	}
	extra = DWCMSHC_EMMC_DLL_DLYENA |
		DLL_TXCLK_TAPNUM_FROM_SW |
		DLL_RXCLK_NO_INVERTER << DWCMSHC_EMMC_DLL_RXCLK_SRCSEL |
		txclk_tapnum;
	if (drv_data->flags & RK_TAP_VALUE_SEL)
		extra |= DLL_TAP_VALUE_SEL | dll_lock_value << DLL_TAP_VALUE_OFFSET;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_TXCLK);

	extra = DWCMSHC_EMMC_DLL_DLYENA |
		drv_data->hs400_strbin_tap |
		DLL_STRBIN_TAPNUM_FROM_SW;
	if (drv_data->flags & RK_TAP_VALUE_SEL)
		extra |= DLL_TAP_VALUE_SEL | dll_lock_value << DLL_TAP_VALUE_OFFSET;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_STRBIN);

exit:
	/* enable output clock */
	sdhci_enable_clk(host, 0);
}

static void rk35xx_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *priv = dwc_priv->priv;
	u32 extra = sdhci_readl(host, DECMSHC_EMMC_MISC_CON);

	if ((host->mmc->caps2 & MMC_CAP2_CQE) && (mask & SDHCI_RESET_ALL))
		cqhci_deactivate(host->mmc);

	if (mask & SDHCI_RESET_ALL && priv->reset) {
		reset_control_assert(priv->reset);
		udelay(1);
		reset_control_deassert(priv->reset);
	}

	sdhci_reset(host, mask);

	/* Enable INTERNAL CLOCK */
	sdhci_writel(host, MISC_INTCLK_EN | extra, DECMSHC_EMMC_MISC_CON);
}

static void sdhci_dwcmshc_request_done(struct sdhci_host *host, struct mmc_request *mrq)
{
	if (!(host->mmc->caps2 & MMC_CAP2_CQE)) {
		if (mmc_hsq_finalize_request(host->mmc, mrq))
			return;
	}

	mmc_request_done(host->mmc, mrq);
}

static const struct sdhci_ops sdhci_dwcmshc_ops = {
	.set_clock		= sdhci_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= dwcmshc_get_max_clock,
	.reset			= sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,
	.irq			= dwcmshc_cqe_irq_handler,
};

static const struct sdhci_ops sdhci_dwcmshc_rk35xx_ops = {
	.set_clock		= dwcmshc_rk3568_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= sdhci_pltfm_clk_get_max_clock,
	.reset			= rk35xx_sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,
	.irq			= dwcmshc_cqe_irq_handler,
	.request_done		= sdhci_dwcmshc_request_done,
};

static const struct sdhci_pltfm_data sdhci_dwcmshc_pdata = {
	.ops = &sdhci_dwcmshc_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

#ifdef CONFIG_ACPI
static const struct sdhci_pltfm_data sdhci_dwcmshc_bf3_pdata = {
	.ops = &sdhci_dwcmshc_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_ACMD23_BROKEN,
};
#endif

static const struct sdhci_pltfm_data sdhci_dwcmshc_rk35xx_pdata = {
	.ops = &sdhci_dwcmshc_rk35xx_ops,
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN,
};

static const struct cqhci_host_ops dwcmshc_cqhci_ops = {
	.enable		= dwcmshc_sdhci_cqe_enable,
	.disable	= sdhci_cqe_disable,
	.dumpregs	= dwcmshc_cqhci_dumpregs,
	.set_tran_desc	= dwcmshc_set_tran_desc,
};

static const struct cqhci_host_ops rk35xx_cqhci_ops = {
	.enable		= rk35xx_sdhci_cqe_enable,
	.disable	= rk35xx_sdhci_cqe_disabled,
	.dumpregs	= dwcmshc_cqhci_dumpregs,
	.set_tran_desc	= dwcmshc_set_tran_desc,
};

static void dwcmshc_cqhci_init(struct sdhci_host *host, struct platform_device *pdev)
{
	struct cqhci_host *cq_host;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *rk_priv = priv->priv;
	bool dma64 = false;
	u16 clk;
	int err;

	host->mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;
	cq_host = devm_kzalloc(&pdev->dev, sizeof(*cq_host), GFP_KERNEL);
	if (!cq_host) {
		dev_err(mmc_dev(host->mmc), "Unable to setup CQE: not enough memory\n");
		goto dsbl_cqe_caps;
	}

	/*
	 * For dwcmshc host controller we have to enable internal clock
	 * before access to some registers from Vendor Specific Area 2.
	 */
	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	if (!(clk & SDHCI_CLOCK_INT_EN)) {
		dev_err(mmc_dev(host->mmc), "Unable to setup CQE: internal clock enable error\n");
		goto free_cq_host;
	}

	cq_host->mmio = host->ioaddr + priv->vendor_specific_area2;
	if (rk_priv)
		cq_host->ops = &rk35xx_cqhci_ops;
	else
		cq_host->ops = &dwcmshc_cqhci_ops;

	/* Enable using of 128-bit task descriptors */
	dma64 = host->flags & SDHCI_USE_64_BIT_DMA;
	if (dma64) {
		dev_dbg(mmc_dev(host->mmc), "128-bit task descriptors\n");
		cq_host->caps |= CQHCI_TASK_DESC_SZ_128;
	}
	err = cqhci_init(cq_host, host->mmc, dma64);
	if (err) {
		dev_err(mmc_dev(host->mmc), "Unable to setup CQE: error %d\n", err);
		goto int_clock_disable;
	}

	dev_dbg(mmc_dev(host->mmc), "CQE init done\n");

	return;

int_clock_disable:
	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

free_cq_host:
	devm_kfree(&pdev->dev, cq_host);

dsbl_cqe_caps:
	host->mmc->caps2 &= ~(MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD);
}

static int dwcmshc_rk35xx_init(struct sdhci_host *host, struct dwcmshc_priv *dwc_priv)
{
	int err;
	struct rk35xx_priv *priv = dwc_priv->priv;

	priv->reset = devm_reset_control_array_get_optional_exclusive(mmc_dev(host->mmc));
	if (IS_ERR(priv->reset)) {
		err = PTR_ERR(priv->reset);
		dev_err(mmc_dev(host->mmc), "failed to get reset control %d\n", err);
		return err;
	}

	priv->rockchip_clks[0].id = "axi";
	priv->rockchip_clks[1].id = "block";
	priv->rockchip_clks[2].id = "timer";
	err = devm_clk_bulk_get_optional(mmc_dev(host->mmc), RK35xx_MAX_CLKS,
					 priv->rockchip_clks);
	if (err) {
		dev_err(mmc_dev(host->mmc), "failed to get clocks %d\n", err);
		return err;
	}

	err = clk_bulk_prepare_enable(RK35xx_MAX_CLKS, priv->rockchip_clks);
	if (err) {
		dev_err(mmc_dev(host->mmc), "failed to enable clocks %d\n", err);
		return err;
	}

	if (of_property_read_u8(mmc_dev(host->mmc)->of_node, "rockchip,txclk-tapnum",
				&priv->txclk_tapnum))
		priv->txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT;

	/* Disable cmd conflict check */
	sdhci_writel(host, 0x0, dwc_priv->vendor_specific_area1 + DWCMSHC_HOST_CTRL3);
	/* Reset previous settings */
	sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_TXCLK);
	sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_STRBIN);

	return 0;
}

static void dwcmshc_rk35xx_postinit(struct sdhci_host *host, struct dwcmshc_priv *dwc_priv)
{
	/*
	 * Don't support highspeed bus mode with low clk speed as we
	 * cannot use DLL for this condition.
	 */
	if (host->mmc->f_max <= 52000000) {
		dev_info(mmc_dev(host->mmc), "Disabling HS200/HS400, frequency too low (%d)\n",
			 host->mmc->f_max);
		host->mmc->caps2 &= ~(MMC_CAP2_HS200 | MMC_CAP2_HS400);
		host->mmc->caps &= ~(MMC_CAP_3_3V_DDR | MMC_CAP_1_8V_DDR);
	}
}

static const struct dwcmshc_driver_data dwcmshc_drvdata = {
	.pdata = &sdhci_dwcmshc_pdata,
	.flags = 0,
};

static const struct dwcmshc_driver_data rk3568_drvdata = {
	.pdata = &sdhci_dwcmshc_rk35xx_pdata,
	.flags = RK_PLATFROM | RK_RXCLK_NO_INVERTER | RK_TAP_VALUE_SEL,
	.hs200_tx_tap = 16,
	.hs400_tx_tap = 8,
	.hs400_cmd_tap = 8,
	.hs400_strbin_tap = 4,
	.ddr50_strbin_delay_num = 16,
};

static const struct dwcmshc_driver_data rk3588_drvdata = {
	.pdata = &sdhci_dwcmshc_rk35xx_pdata,
	.flags = RK_PLATFROM | RK_DLL_CMD_OUT | RK_TAP_VALUE_SEL,
	.hs200_tx_tap = 16,
	.hs400_tx_tap = 9,
	.hs400_cmd_tap = 8,
	.hs400_strbin_tap = 4,
	.ddr50_strbin_delay_num = 16,
};

static const struct dwcmshc_driver_data rk3528_drvdata = {
	.pdata = &sdhci_dwcmshc_rk35xx_pdata,
	.flags = RK_PLATFROM | RK_DLL_CMD_OUT | RK_TAP_VALUE_SEL,
	.hs200_tx_tap = 12,
	.hs400_tx_tap = 6,
	.hs400_cmd_tap = 6,
	.hs400_strbin_tap = 3,
	.ddr50_strbin_delay_num = 10,
};

static const struct dwcmshc_driver_data rk3562_drvdata = {
	.pdata = &sdhci_dwcmshc_rk35xx_pdata,
	.flags = RK_PLATFROM | RK_DLL_CMD_OUT | RK_TAP_VALUE_SEL,
	.hs200_tx_tap = 12,
	.hs400_tx_tap = 6,
	.hs400_cmd_tap = 6,
	.hs400_strbin_tap = 3,
	.ddr50_strbin_delay_num = 10,
};

static const struct dwcmshc_driver_data rk3576_drvdata = {
	.pdata = &sdhci_dwcmshc_rk35xx_pdata,
	.flags = RK_PLATFROM | RK_DLL_CMD_OUT | RK_TAP_VALUE_SEL,
	.hs200_tx_tap = 16,
	.hs400_tx_tap = 7,
	.hs400_cmd_tap = 7,
	.hs400_strbin_tap = 5,
	.ddr50_strbin_delay_num = 10,
};

static const struct of_device_id sdhci_dwcmshc_dt_ids[] = {
	{
		.compatible = "rockchip,rk3588-dwcmshc",
		.data = &rk3588_drvdata,
	},
	{
		.compatible = "rockchip,rk3576-dwcmshc",
		.data = &rk3576_drvdata,
	},
	{
		.compatible = "rockchip,rk3568-dwcmshc",
		.data = &rk3568_drvdata,
	},
	{
		.compatible = "rockchip,rk3528-dwcmshc",
		.data = &rk3528_drvdata,
	},
	{
		.compatible = "rockchip,rk3562-dwcmshc",
		.data = &rk3562_drvdata,
	},
	{
		.compatible = "snps,dwcmshc-sdhci",
		.data = &dwcmshc_drvdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, sdhci_dwcmshc_dt_ids);

#ifdef CONFIG_ACPI
static const struct acpi_device_id sdhci_dwcmshc_acpi_ids[] = {
	{
		.id = "MLNXBF30",
		.driver_data = (kernel_ulong_t)&sdhci_dwcmshc_bf3_pdata,
	},
	{}
};
#endif

static int dwcmshc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_host *host;
	struct dwcmshc_priv *priv;
	struct rk35xx_priv *rk_priv = NULL;
	const struct sdhci_pltfm_data *pltfm_data;
	const struct dwcmshc_driver_data *drv_data;
	struct mmc_hsq *hsq;
	int err;
	u32 extra, caps;

	drv_data = device_get_match_data(&pdev->dev);
	if (!drv_data) {
		dev_err(&pdev->dev, "Error: No device match data found\n");
		return -ENODEV;
	}
	pltfm_data = drv_data->pdata;

	host = sdhci_pltfm_init(pdev, pltfm_data,
				sizeof(struct dwcmshc_priv));
	if (IS_ERR(host))
		return PTR_ERR(host);

	/*
	 * extra adma table cnt for cross 128M boundary handling.
	 */
	extra = DIV_ROUND_UP_ULL(dma_get_required_mask(dev), SZ_128M);
	if (extra > SDHCI_MAX_SEGS)
		extra = SDHCI_MAX_SEGS;
	host->adma_table_cnt += extra;

	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);

	if (dev->of_node) {
		pltfm_host->clk = devm_clk_get(dev, "core");
		if (IS_ERR(pltfm_host->clk)) {
			err = PTR_ERR(pltfm_host->clk);
			dev_err(dev, "failed to get core clk: %d\n", err);
			goto free_pltfm;
		}
		err = clk_prepare_enable(pltfm_host->clk);
		if (err)
			goto free_pltfm;

		priv->bus_clk = devm_clk_get(dev, "bus");
		if (!IS_ERR(priv->bus_clk))
			clk_prepare_enable(priv->bus_clk);
	}

	err = mmc_of_parse(host->mmc);
	if (err)
		goto err_clk;

	sdhci_get_of_property(pdev);

	priv->vendor_specific_area1 =
		sdhci_readl(host, DWCMSHC_P_VENDOR_AREA1) & DWCMSHC_AREA1_MASK;

	host->mmc_host_ops.request = dwcmshc_request;
	host->mmc_host_ops.hs400_enhanced_strobe = dwcmshc_hs400_enhanced_strobe;
	host->mmc_host_ops.execute_tuning = dwcmshc_execute_tuning;

	if (drv_data->flags & RK_PLATFROM) {
		rk_priv = devm_kzalloc(&pdev->dev, sizeof(struct rk35xx_priv), GFP_KERNEL);
		if (!rk_priv) {
			err = -ENOMEM;
			goto err_clk;
		}

		rk_priv->drv_data = drv_data;
		rk_priv->acpi_en = has_acpi_companion(&pdev->dev);

		if (of_device_is_compatible(pdev->dev.of_node, "rockchip,rk3588-dwcmshc"))
			rk_priv->devtype = DWCMSHC_RK3588;
		else
			rk_priv->devtype = DWCMSHC_RK3568;

		priv->priv = rk_priv;

		err = dwcmshc_rk35xx_init(host, priv);
		if (err)
			goto err_clk;
	}

	caps = sdhci_readl(host, SDHCI_CAPABILITIES);
	if (caps & SDHCI_CAN_64BIT_V4)
		sdhci_enable_v4_mode(host);

	host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;

	err = sdhci_setup_host(host);
	if (err)
		goto err_clk;

	/* Setup Command Queue Engine if enabled */
	if (device_property_read_bool(&pdev->dev, "supports-cqe")) {
		priv->vendor_specific_area2 =
			sdhci_readw(host, DWCMSHC_P_VENDOR_AREA2);

		dwcmshc_cqhci_init(host, pdev);
	} else {
		hsq = devm_kzalloc(&pdev->dev, sizeof(*hsq), GFP_KERNEL);
		if (!hsq) {
			err = -ENOMEM;
			goto err_setup_host;
		}

		err = mmc_hsq_init(hsq, host->mmc);
		if (err)
			goto err_setup_host;
	}

	if (rk_priv)
		dwcmshc_rk35xx_postinit(host, priv);

	err = __sdhci_add_host(host);
	if (err)
		goto err_setup_host;

	if (rk_priv && !rk_priv->acpi_en) {
		if (dev->pm_domain) {
			struct generic_pm_domain *genpd;

			genpd = pd_to_genpd(dev->pm_domain);
			genpd->flags |= GENPD_FLAG_RPM_ALWAYS_ON;
		}
		pm_runtime_get_noresume(&pdev->dev);
		pm_runtime_set_active(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_runtime_put_autosuspend(&pdev->dev);
	}

	return 0;

err_setup_host:
	sdhci_cleanup_host(host);
err_clk:
	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(priv->bus_clk);
	if (rk_priv)
		clk_bulk_disable_unprepare(RK35xx_MAX_CLKS,
					   rk_priv->rockchip_clks);
free_pltfm:
	sdhci_pltfm_free(pdev);
	return err;
}

static int dwcmshc_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *rk_priv = priv->priv;

	if (rk_priv && !rk_priv->acpi_en) {
		pm_runtime_get_sync(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
		pm_runtime_put_noidle(&pdev->dev);
		pm_runtime_dont_use_autosuspend(&pdev->dev);
	}

	sdhci_remove_host(host, 0);

	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(priv->bus_clk);
	if (rk_priv)
		clk_bulk_disable_unprepare(RK35xx_MAX_CLKS,
					   rk_priv->rockchip_clks);
	sdhci_pltfm_free(pdev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwcmshc_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *rk_priv = priv->priv;
	int ret;

	if (host->mmc->caps2 & MMC_CAP2_CQE) {
		ret = cqhci_suspend(host->mmc);
		if (ret)
			return ret;
	} else {
		mmc_hsq_suspend(host->mmc);
	}

	ret = sdhci_suspend_host(host);
	if (ret)
		return ret;

	clk_disable_unprepare(pltfm_host->clk);
	if (!IS_ERR(priv->bus_clk))
		clk_disable_unprepare(priv->bus_clk);

	if (rk_priv)
		clk_bulk_disable_unprepare(RK35xx_MAX_CLKS,
					   rk_priv->rockchip_clks);

	return ret;
}

static int dwcmshc_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk35xx_priv *rk_priv = priv->priv;
	int ret;

	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret)
		return ret;

	if (!IS_ERR(priv->bus_clk)) {
		ret = clk_prepare_enable(priv->bus_clk);
		if (ret)
			goto disable_clk;
	}

	if (rk_priv) {
		ret = clk_bulk_prepare_enable(RK35xx_MAX_CLKS,
					      rk_priv->rockchip_clks);
		if (ret)
			goto disable_bus_clk;
	}

	ret = sdhci_resume_host(host);
	if (ret)
		goto disable_rockchip_clks;

	if (host->mmc->caps2 & MMC_CAP2_CQE) {
		ret = cqhci_resume(host->mmc);
		if (ret)
			goto disable_rockchip_clks;
	} else {
		return mmc_hsq_resume(host->mmc);
	}

	return 0;

disable_rockchip_clks:
	if (rk_priv)
		clk_bulk_disable_unprepare(RK35xx_MAX_CLKS,
					   rk_priv->rockchip_clks);
disable_bus_clk:
	if (!IS_ERR(priv->bus_clk))
		clk_disable_unprepare(priv->bus_clk);
disable_clk:
	clk_disable_unprepare(pltfm_host->clk);
	return ret;
}
#endif

#ifdef CONFIG_PM

static int dwcmshc_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	u16 data;

	data = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	data &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, data, SDHCI_CLOCK_CONTROL);

	return 0;
}

static int dwcmshc_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	u16 data;

	data = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	data |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, data, SDHCI_CLOCK_CONTROL);

	return 0;
}
#endif

static const struct dev_pm_ops dwcmshc_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwcmshc_suspend, dwcmshc_resume)
	SET_RUNTIME_PM_OPS(dwcmshc_runtime_suspend, dwcmshc_runtime_resume, NULL)
};

static struct platform_driver sdhci_dwcmshc_driver = {
	.driver	= {
		.name	= "sdhci-dwcmshc",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = sdhci_dwcmshc_dt_ids,
		.acpi_match_table = ACPI_PTR(sdhci_dwcmshc_acpi_ids),
		.pm = &dwcmshc_pmops,
	},
	.probe	= dwcmshc_probe,
	.remove	= dwcmshc_remove,
};
module_platform_driver(sdhci_dwcmshc_driver);

MODULE_DESCRIPTION("SDHCI platform driver for Synopsys DWC MSHC");
MODULE_AUTHOR("Jisheng Zhang <jszhang@kernel.org>");
MODULE_LICENSE("GPL v2");
