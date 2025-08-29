// SPDX-License-Identifier: GPL-2.0-only
// ALSA SoC Audio Layer - Rockchip I2S/TDM Controller driver

// Copyright (c) 2018 Rockchip Electronics Co., Ltd.
// Author: Sugar Zhang <sugar.zhang@rock-chips.com>
// Author: Nicolas Frattaroli <frattaroli.nicolas@gmail.com>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/clk/rockchip.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/spinlock.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>

#include "rockchip_i2s_tdm.h"
#include "rockchip_dlp_pcm.h"
#include "rockchip_utils.h"
#include "rockchip_trcm.h"

#define DRV_NAME "rockchip-i2s-tdm"

#if IS_ENABLED(CONFIG_CPU_PX30) || IS_ENABLED(CONFIG_CPU_RK1808) || IS_ENABLED(CONFIG_CPU_RK3308)
#define HAVE_SYNC_RESET
#endif

#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
/*
 * Example: RK3588
 *
 * Use I2S2_2CH as Clk-Gen to serve TDM_MULTI_LANES
 *
 * I2S2_2CH ----> BCLK,I2S_LRCK --------> I2S0_8CH_TX (Slave TRCM-TXONLY)
 *     |
 *     |--------> BCLK,TDM_SYNC --------> TDM Device (Slave)
 *
 * Note:
 *
 * I2S2_2CH_MCLK: BCLK
 * I2S2_2CH_SCLK: I2S_LRCK (GPIO2_B7)
 * I2S2_2CH_LRCK: TDM_SYNC (GPIO2_C0)
 *
 */

#define XFER_EN					0x3
#define XFER_DIS				0x0
#define CKR_V(m, r, t)				((m - 1) << 16 | (r - 1) << 8 | (t - 1) << 0)
#define I2S_XCR_IBM_V(v)			((v) & I2S_TXCR_IBM_MASK)
#define I2S_XCR_IBM_NORMAL			I2S_TXCR_IBM_NORMAL
#define I2S_XCR_IBM_LSJM			I2S_TXCR_IBM_LSJM
#endif

#define CLK_MAX_COUNT				1000
#define NSAMPLES				4
#define DEFAULT_MCLK_FS				256
#define DEFAULT_FS				48000
#define CH_GRP_MAX				4  /* The max channel 8 / 2 */
#define MULTIPLEX_CH_MAX			10
#define CLK_PPM_MIN				(-1000)
#define CLK_PPM_MAX				(1000)
#define CLK_SHIFT_RATE_HZ_MAX			5
#define MAXBURST				16
#define MAXBURST_PER_FIFO			8
#define DEPTH_PER_FIFO				32
#define WAIT_TIME_MS_MAX			10000

#define TRCM_TXRX				0
#define TRCM_TX					1
#define TRCM_RX					2

#define QUIRK_ALWAYS_ON				BIT(0)
#define QUIRK_HDMI_PATH				BIT(1)

struct txrx_config {
	u32 addr;
	u32 reg;
	u32 txonly;
	u32 rxonly;
};

struct rk_i2s_tdm_dev;

struct rk_i2s_soc_data {
	u32 softrst_offset;
	u32 grf_reg_offset;
	u32 grf_shift;
	int config_count;
	const struct txrx_config *configs;
	int (*init)(struct device *dev, u32 addr);
	void (*src_clk_ctrl)(struct rk_i2s_tdm_dev *i2s_tdm, bool en);
};

struct rk_i2s_tdm_dev {
	struct device *dev;
	struct clk *hclk;
	struct clk *mclk_tx;
	struct clk *mclk_rx;
	/* The mclk_tx_src is parent of mclk_tx */
	struct clk *mclk_tx_src;
	/* The mclk_rx_src is parent of mclk_rx */
	struct clk *mclk_rx_src;
	/*
	 * The mclk_root0 and mclk_root1 are root parent and supplies for
	 * the different FS.
	 *
	 * e.g:
	 * mclk_root0 is VPLL0, used for FS=48000Hz
	 * mclk_root1 is VPLL1, used for FS=44100Hz
	 */
	struct clk *mclk_root0;
	struct clk *mclk_root1;
	struct regmap *regmap;
	struct regmap *grf;
	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct snd_dmaengine_dai_dma_data playback_dma_data;
	struct snd_pcm_substream *substreams[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int wait_time[SNDRV_PCM_STREAM_LAST + 1];
	struct snd_soc_component *pcm_comp;
	struct reset_control *tx_reset;
	struct reset_control *rx_reset;
	struct pinctrl *pinctrl;
	struct pinctrl_state *clk_state;
	const struct rk_i2s_soc_data *soc_data;
#ifdef HAVE_SYNC_RESET
	int id;
	void __iomem *cru_base;
#endif
	bool is_master_mode;
	bool io_multiplex;
	bool mclk_calibrate;
	bool tdm_mode;
	bool tdm_fsync_half_frame;
	bool is_dma_active[SNDRV_PCM_STREAM_LAST + 1];
	bool no_pcm;
	unsigned int mclk_rx_freq;
	unsigned int mclk_tx_freq;
	unsigned int mclk_root0_freq;
	unsigned int mclk_root1_freq;
	unsigned int mclk_root0_initial_freq;
	unsigned int mclk_root1_initial_freq;
	unsigned int frame_width;
	unsigned int clk_trcm;
	unsigned int i2s_sdis[CH_GRP_MAX];
	unsigned int i2s_sdos[CH_GRP_MAX];
	unsigned int quirks;
	unsigned int lrck_ratio;
	unsigned int tdm_slots;
	unsigned int resume_deferred_ms;
	int clk_ppm;
	int refcount;
	spinlock_t lock; /* xfer lock */
	bool has_playback;
	bool has_capture;
	struct snd_soc_dai_driver *dai;
	struct gpio_desc *i2s_lrck_gpio;
#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	struct snd_soc_dai *clk_src_dai;
	struct gpio_desc *tdm_fsync_gpio;
	struct gpio_desc *fsxn_tx_gpio;
	struct gpio_desc *fsxn_rx_gpio;
	unsigned int tx_lanes;
	unsigned int rx_lanes;
	void __iomem *clk_src_base;
	bool is_tdm_multi_lanes;
#endif
};

static struct i2s_of_quirks {
	char *quirk;
	int id;
} of_quirks[] = {
	{
		.quirk = "rockchip,always-on",
		.id = QUIRK_ALWAYS_ON,
	},
	{
		.quirk = "rockchip,hdmi-path",
		.id = QUIRK_HDMI_PATH,
	},
};

static int rockchip_trcm_dma_guard_ctrl(struct rk_i2s_tdm_dev *i2s_tdm,
					int stream, bool en)
{
	if (i2s_tdm->no_pcm)
		return 0;

	if (!i2s_tdm->pcm_comp) {
		dev_err(i2s_tdm->dev, "Uninitialized component for TRCM\n");
		return -EINVAL;
	}

	return dmaengine_trcm_dma_guard_ctrl(i2s_tdm->pcm_comp, stream, en);
}

static bool rockchip_i2s_tdm_stream_valid(struct snd_pcm_substream *substream,
					  struct snd_soc_dai *dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_dai_get_drvdata(dai);

	if (!substream)
		return false;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
	    i2s_tdm->has_playback)
		return true;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
	    i2s_tdm->has_capture)
		return true;

	return false;
}

static int to_ch_num(unsigned int val)
{
	switch (val) {
	case I2S_CHN_4:
		return 4;
	case I2S_CHN_6:
		return 6;
	case I2S_CHN_8:
		return 8;
	default:
		return 2;
	}
}

static void i2s_tdm_disable_unprepare_mclk(struct rk_i2s_tdm_dev *i2s_tdm)
{
	clk_disable_unprepare(i2s_tdm->mclk_tx);
	clk_disable_unprepare(i2s_tdm->mclk_rx);
	if (i2s_tdm->mclk_calibrate) {
		clk_disable_unprepare(i2s_tdm->mclk_tx_src);
		clk_disable_unprepare(i2s_tdm->mclk_rx_src);
		clk_disable_unprepare(i2s_tdm->mclk_root0);
		clk_disable_unprepare(i2s_tdm->mclk_root1);
	}
}

/**
 * i2s_tdm_prepare_enable_mclk - prepare to enable all mclks, disable them on
 *				 failure.
 * @i2s_tdm: rk_i2s_tdm_dev struct
 *
 * This function attempts to enable all mclk clocks, but cleans up after
 * itself on failure. Guarantees to balance its calls.
 *
 * Returns success (0) or negative errno.
 */
static int i2s_tdm_prepare_enable_mclk(struct rk_i2s_tdm_dev *i2s_tdm)
{
	int ret = 0;

	ret = clk_prepare_enable(i2s_tdm->mclk_tx);
	if (ret)
		goto err_mclk_tx;
	ret = clk_prepare_enable(i2s_tdm->mclk_rx);
	if (ret)
		goto err_mclk_rx;

	if (i2s_tdm->mclk_calibrate) {
		ret = clk_prepare_enable(i2s_tdm->mclk_tx_src);
		if (ret)
			goto err_mclk_tx_src;
		ret = clk_prepare_enable(i2s_tdm->mclk_rx_src);
		if (ret)
			goto err_mclk_rx_src;
		ret = clk_prepare_enable(i2s_tdm->mclk_root0);
		if (ret)
			goto err_mclk_root0;
		ret = clk_prepare_enable(i2s_tdm->mclk_root1);
		if (ret)
			goto err_mclk_root1;
	}

	return 0;

err_mclk_root1:
	clk_disable_unprepare(i2s_tdm->mclk_root0);
err_mclk_root0:
	clk_disable_unprepare(i2s_tdm->mclk_rx_src);
err_mclk_rx_src:
	clk_disable_unprepare(i2s_tdm->mclk_tx_src);
err_mclk_tx_src:
	clk_disable_unprepare(i2s_tdm->mclk_rx);
err_mclk_rx:
	clk_disable_unprepare(i2s_tdm->mclk_tx);
err_mclk_tx:
	return ret;
}

static inline struct rk_i2s_tdm_dev *to_info(struct snd_soc_dai *dai)
{
	return snd_soc_dai_get_drvdata(dai);
}

static inline bool is_stream_active(struct rk_i2s_tdm_dev *i2s_tdm, int stream)
{
	unsigned int val;

	regmap_read(i2s_tdm->regmap, I2S_XFER, &val);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		return (val & I2S_XFER_TXS_START);
	else
		return (val & I2S_XFER_RXS_START);
}

static inline bool is_dma_active(struct rk_i2s_tdm_dev *i2s_tdm, int stream)
{
	unsigned int val;

	regmap_read(i2s_tdm->regmap, I2S_DMACR, &val);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		return (val & I2S_DMACR_TDE_MASK);
	else
		return (val & I2S_DMACR_RDE_MASK);
}

#ifdef HAVE_SYNC_RESET
static void rockchip_i2s_tdm_src_clk_ctrl(struct rk_i2s_tdm_dev *i2s_tdm, bool en,
					  unsigned int gate_reg, unsigned int gate_val,
					  unsigned int sel_reg)
{
	int val = readl(i2s_tdm->cru_base + sel_reg);

	if (!gate_reg || !sel_reg)
		return;

	if (IS_I2S_CLK_SRC_MCLKIN(val) && en)
		writel(I2S_CLK_SRC_MCLKIN, i2s_tdm->cru_base + sel_reg);

	writel(gate_val, i2s_tdm->cru_base + gate_reg);

	if (IS_I2S_CLK_SRC_MCLKIN(val) && !en)
		writel(I2S_CLK_SRC_PLL, i2s_tdm->cru_base + sel_reg);
}

static void rockchip_i2s_tdm_px30_src_clk_ctrl(struct rk_i2s_tdm_dev *i2s_tdm, bool en)
{
	unsigned int gate_reg = 0, gate_val = 0, sel_reg = 0;

	switch (i2s_tdm->clk_trcm) {
	case TRCM_TX:
		sel_reg  = PX30_CLKSEL_CON28_I2S0_TX;
		gate_reg = PX30_CLKGATE_CON9;
		gate_val = en ? PX30_CLKGATE_CON9_I2S0_TX_PLL_EN :
				PX30_CLKGATE_CON9_I2S0_TX_PLL_DIS;
		break;
	case TRCM_RX:
		sel_reg  = PX30_CLKSEL_CON58_I2S0_RX;
		gate_reg = PX30_CLKGATE_CON17;
		gate_val = en ? PX30_CLKGATE_CON17_I2S0_RX_PLL_EN :
				PX30_CLKGATE_CON17_I2S0_RX_PLL_DIS;
		break;
	}

	rockchip_i2s_tdm_src_clk_ctrl(i2s_tdm, en, gate_reg, gate_val, sel_reg);
}

static void rockchip_i2s_tdm_rk1808_src_clk_ctrl(struct rk_i2s_tdm_dev *i2s_tdm, bool en)
{
	unsigned int gate_reg = 0, gate_val = 0, sel_reg = 0;

	switch (i2s_tdm->clk_trcm) {
	case TRCM_TX:
		sel_reg  = RK1808_CLKSEL_CON32_I2S0_TX;
		gate_reg = RK1808_CLKGATE_CON17;
		gate_val = en ? RK1808_CLKGATE_CON17_I2S0_TX_PLL_EN :
				RK1808_CLKGATE_CON17_I2S0_TX_PLL_DIS;
		break;
	case TRCM_RX:
		sel_reg  = RK1808_CLKSEL_CON34_I2S0_RX;
		gate_reg = RK1808_CLKGATE_CON18;
		gate_val = en ? RK1808_CLKGATE_CON18_I2S0_RX_PLL_EN :
				RK1808_CLKGATE_CON18_I2S0_RX_PLL_DIS;
		break;
	}

	rockchip_i2s_tdm_src_clk_ctrl(i2s_tdm, en, gate_reg, gate_val, sel_reg);
}

static void rockchip_i2s_tdm_rk3308_src_clk_ctrl(struct rk_i2s_tdm_dev *i2s_tdm, bool en)
{
	unsigned int gate_reg = 0, gate_val = 0, sel_reg = 0;

	/* I2S_8CH_2 used for internal and TRCM-none mode default */
	switch (i2s_tdm->id) {
	case 0:
		switch (i2s_tdm->clk_trcm) {
		case TRCM_TX:
			sel_reg  = RK3308_CLKSEL_CON52_I2S0_TX;
			gate_reg = RK3308_CLKGATE_CON10;
			gate_val = en ? RK3308_CLKGATE_CON10_I2S0_TX_PLL_EN :
					RK3308_CLKGATE_CON10_I2S0_TX_PLL_DIS;
			break;
		case TRCM_RX:
			sel_reg  = RK3308_CLKSEL_CON54_I2S0_RX;
			gate_reg = RK3308_CLKGATE_CON11;
			gate_val = en ? RK3308_CLKGATE_CON11_I2S0_RX_PLL_EN :
					RK3308_CLKGATE_CON11_I2S0_RX_PLL_DIS;
			break;
		}
		break;
	case 1:
		switch (i2s_tdm->clk_trcm) {
		case TRCM_TX:
			sel_reg  = RK3308_CLKSEL_CON56_I2S1_TX;
			gate_reg = RK3308_CLKGATE_CON11;
			gate_val = en ? RK3308_CLKGATE_CON11_I2S1_TX_PLL_EN :
					RK3308_CLKGATE_CON11_I2S1_TX_PLL_DIS;
			break;
		case TRCM_RX:
			sel_reg  = RK3308_CLKSEL_CON58_I2S1_RX;
			gate_reg = RK3308_CLKGATE_CON11;
			gate_val = en ? RK3308_CLKGATE_CON11_I2S1_RX_PLL_EN :
					RK3308_CLKGATE_CON11_I2S1_RX_PLL_DIS;
			break;
		}
		break;
	}

	rockchip_i2s_tdm_src_clk_ctrl(i2s_tdm, en, gate_reg, gate_val, sel_reg);
}

static void rockchip_i2s_tdm_reset_assert(struct rk_i2s_tdm_dev *i2s_tdm)
{
	if (!i2s_tdm->cru_base || !i2s_tdm->soc_data || !i2s_tdm->is_master_mode)
		return;

	if (IS_ERR_OR_NULL(i2s_tdm->tx_reset) || IS_ERR_OR_NULL(i2s_tdm->rx_reset))
		return;

	reset_control_assert(i2s_tdm->tx_reset);
	reset_control_assert(i2s_tdm->rx_reset);

	/* delay for reset assert done */
	udelay(10);
}

static void rockchip_i2s_tdm_reset_deassert(struct rk_i2s_tdm_dev *i2s_tdm)
{
	if (!i2s_tdm->cru_base || !i2s_tdm->soc_data || !i2s_tdm->is_master_mode)
		return;

	if (IS_ERR_OR_NULL(i2s_tdm->tx_reset) || IS_ERR_OR_NULL(i2s_tdm->rx_reset))
		return;

	if (i2s_tdm->soc_data && i2s_tdm->soc_data->src_clk_ctrl)
		i2s_tdm->soc_data->src_clk_ctrl(i2s_tdm, 0);

	reset_control_deassert(i2s_tdm->tx_reset);
	reset_control_deassert(i2s_tdm->rx_reset);

	if (i2s_tdm->soc_data && i2s_tdm->soc_data->src_clk_ctrl)
		i2s_tdm->soc_data->src_clk_ctrl(i2s_tdm, 1);

	/* delay for reset deassert done */
	udelay(10);
}
#else
static inline void rockchip_i2s_tdm_reset_assert(struct rk_i2s_tdm_dev *i2s_tdm)
{
}
static inline void rockchip_i2s_tdm_reset_deassert(struct rk_i2s_tdm_dev *i2s_tdm)
{
}
#endif

static void rockchip_i2s_tdm_reset(struct rk_i2s_tdm_dev *i2s_tdm, unsigned int clr)
{
	if ((clr & I2S_CLR_TXC) && !IS_ERR_OR_NULL(i2s_tdm->tx_reset)) {
		reset_control_assert(i2s_tdm->tx_reset);
		/* delay for reset assert done */
		udelay(10);
		reset_control_deassert(i2s_tdm->tx_reset);
		/* delay for reset deassert done */
		udelay(10);
	}

	if ((clr & I2S_CLR_RXC) && !IS_ERR_OR_NULL(i2s_tdm->rx_reset)) {
		reset_control_assert(i2s_tdm->rx_reset);
		/* delay for reset assert done */
		udelay(10);
		reset_control_deassert(i2s_tdm->rx_reset);
		/* delay for reset deassert done */
		udelay(10);
	}
}

static int rockchip_i2s_tdm_clear(struct rk_i2s_tdm_dev *i2s_tdm,
				  unsigned int clr)
{
	unsigned int val = 0;
	int ret = 0;

	regmap_update_bits(i2s_tdm->regmap, I2S_CLR, clr, clr);
	ret = regmap_read_poll_timeout_atomic(i2s_tdm->regmap, I2S_CLR, val,
					      !(val & clr), 10, 100);
	if (ret == 0)
		return 0;

	/*
	 * Workaround for FIFO clear on SLAVE mode:
	 *
	 * A Suggest to do reset hclk domain and then do mclk
	 *   domain, especially for SLAVE mode without CLK in.
	 *   at last, recovery regmap config.
	 *
	 * B Suggest to switch to MASTER, and then do FIFO clr,
	 *   at last, bring back to SLAVE.
	 *
	 * Now we choose plan B here.
	 */
	if (!i2s_tdm->is_master_mode)
		regmap_update_bits(i2s_tdm->regmap, I2S_CKR,
				   I2S_CKR_MSS_MASK, I2S_CKR_MSS_MASTER);
	regmap_update_bits(i2s_tdm->regmap, I2S_CLR, clr, clr);
	ret = regmap_read_poll_timeout_atomic(i2s_tdm->regmap, I2S_CLR, val,
					      !(val & clr), 10, 100);
	if (!i2s_tdm->is_master_mode)
		regmap_update_bits(i2s_tdm->regmap, I2S_CKR,
				   I2S_CKR_MSS_MASK, I2S_CKR_MSS_SLAVE);

	if (ret < 0) {
		dev_warn(i2s_tdm->dev, "failed to clear %u on %s mode\n",
			 clr, i2s_tdm->is_master_mode ? "master" : "slave");
		goto reset;
	}

	return 0;

reset:
	rockchip_i2s_tdm_reset(i2s_tdm, clr);

	return 0;
}

/*
 * HDMI controller ignores the first FRAME_SYNC cycle, Lost one frame is no big deal
 * for LPCM, but it does matter for Bitstream (NLPCM/HBR), So, padding one frame
 * before xfer the real data to fix it.
 */
static void rockchip_i2s_tdm_tx_fifo_padding(struct rk_i2s_tdm_dev *i2s_tdm, bool en)
{
	unsigned int val, w, c, i;

	if (!en)
		return;

	regmap_read(i2s_tdm->regmap, I2S_TXCR, &val);
	w = ((val & I2S_TXCR_VDW_MASK) >> I2S_TXCR_VDW_SHIFT) + 1;
	c = to_ch_num(val & I2S_TXCR_CSR_MASK) * w / 32;

	for (i = 0; i < c; i++)
		regmap_write(i2s_tdm->regmap, I2S_TXDR, 0x0);
}

static void rockchip_i2s_tdm_fifo_xrun_detect(struct rk_i2s_tdm_dev *i2s_tdm,
					      int stream, bool en)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* clear irq status which was asserted before TXUIE enabled */
		regmap_update_bits(i2s_tdm->regmap, I2S_INTCR,
				   I2S_INTCR_TXUIC, I2S_INTCR_TXUIC);
		regmap_update_bits(i2s_tdm->regmap, I2S_INTCR,
				   I2S_INTCR_TXUIE_MASK,
				   I2S_INTCR_TXUIE(en));
	} else {
		/* clear irq status which was asserted before RXOIE enabled */
		regmap_update_bits(i2s_tdm->regmap, I2S_INTCR,
				   I2S_INTCR_RXOIC, I2S_INTCR_RXOIC);
		regmap_update_bits(i2s_tdm->regmap, I2S_INTCR,
				   I2S_INTCR_RXOIE_MASK,
				   I2S_INTCR_RXOIE(en));
	}
}

static void rockchip_i2s_tdm_dma_ctrl(struct rk_i2s_tdm_dev *i2s_tdm,
				      int stream, bool en)
{
	if (!en)
		rockchip_i2s_tdm_fifo_xrun_detect(i2s_tdm, stream, 0);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (i2s_tdm->quirks & QUIRK_HDMI_PATH)
			rockchip_i2s_tdm_tx_fifo_padding(i2s_tdm, en);

		regmap_update_bits(i2s_tdm->regmap, I2S_DMACR,
				   I2S_DMACR_TDE_MASK,
				   I2S_DMACR_TDE(en));
		/*
		 * Explicitly delay 1 usec for dma to fill FIFO,
		 * though there was a implied HW delay that around
		 * half LRCK cycle (e.g. 2.6us@192k) from XFER-start
		 * to FIFO-pop.
		 *
		 * 1 usec is enough to fill at lease 4 entry each FIFO
		 * @192k 8ch 32bit situation.
		 */
		udelay(1);
	} else {
		regmap_update_bits(i2s_tdm->regmap, I2S_DMACR,
				   I2S_DMACR_RDE_MASK,
				   I2S_DMACR_RDE(en));
	}

	if (en)
		rockchip_i2s_tdm_fifo_xrun_detect(i2s_tdm, stream, 1);
}

static inline int rockchip_i2s_tdm_clk_assert_h(const struct gpio_desc *desc)
{
	int cnt = CLK_MAX_COUNT;

	while (gpiod_get_raw_value(desc) && --cnt)
		;

	return cnt;
}

static inline int rockchip_i2s_tdm_clk_assert_l(const struct gpio_desc *desc)
{
	int cnt = CLK_MAX_COUNT;

	while (!gpiod_get_raw_value(desc) && --cnt)
		;

	return cnt;
}

static inline bool rockchip_i2s_tdm_clk_valid(struct rk_i2s_tdm_dev *i2s_tdm,
					      bool has_fsync)
{
	int dc_h = CLK_MAX_COUNT, dc_l = CLK_MAX_COUNT;

	/*
	 * TBD: optimize debounce and get value
	 *
	 * debounce at least one cycle found, otherwise, the clk ref maybe
	 * not on the fly.
	 */

	/* check HIGH-Level */
	dc_h = rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);
	if (!dc_h)
		return false;

	/* check LOW-Level */
	dc_l = rockchip_i2s_tdm_clk_assert_l(i2s_tdm->i2s_lrck_gpio);
	if (!dc_l)
		return false;

#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	if (!has_fsync)
		return true;

	/* check HIGH-Level */
	dc_h = rockchip_i2s_tdm_clk_assert_h(i2s_tdm->tdm_fsync_gpio);
	if (!dc_h)
		return false;

	/* check LOW-Level */
	dc_l = rockchip_i2s_tdm_clk_assert_l(i2s_tdm->tdm_fsync_gpio);
	if (!dc_l)
		return false;
#endif

	return true;
}

static void __maybe_unused rockchip_i2s_tdm_gpio_clk_meas(struct rk_i2s_tdm_dev *i2s_tdm,
							  const struct gpio_desc *desc,
							  const char *name)
{
	int h[NSAMPLES], l[NSAMPLES], i;

	dev_dbg(i2s_tdm->dev, "%s:\n", name);

	if (!rockchip_i2s_tdm_clk_valid(i2s_tdm, 1))
		return;

	for (i = 0; i < NSAMPLES; i++) {
		h[i] = rockchip_i2s_tdm_clk_assert_h(desc);
		l[i] = rockchip_i2s_tdm_clk_assert_l(desc);
	}

	for (i = 0; i < NSAMPLES; i++)
		dev_dbg(i2s_tdm->dev, "H[%d]: %2d, L[%d]: %2d\n",
			i, CLK_MAX_COUNT - h[i], i, CLK_MAX_COUNT - l[i]);
}

#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
static const char * const tx_lanes_text[] = { "Auto", "SDOx1", "SDOx2", "SDOx3", "SDOx4" };
static const char * const rx_lanes_text[] = { "Auto", "SDIx1", "SDIx2", "SDIx3", "SDIx4" };
static const struct soc_enum tx_lanes_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tx_lanes_text), tx_lanes_text);
static const struct soc_enum rx_lanes_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_lanes_text), rx_lanes_text);

static int rockchip_i2s_tdm_tx_lanes_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = i2s_tdm->tx_lanes;

	return 0;
}

static int rockchip_i2s_tdm_tx_lanes_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);
	int num;

	num = ucontrol->value.enumerated.item[0];
	if (num >= ARRAY_SIZE(tx_lanes_text))
		return -EINVAL;

	i2s_tdm->tx_lanes = num;

	return 1;
}

static int rockchip_i2s_tdm_rx_lanes_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = i2s_tdm->rx_lanes;

	return 0;
}

static int rockchip_i2s_tdm_rx_lanes_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);
	int num;

	num = ucontrol->value.enumerated.item[0];
	if (num >= ARRAY_SIZE(rx_lanes_text))
		return -EINVAL;

	i2s_tdm->rx_lanes = num;

	return 1;
}

static int rockchip_i2s_tdm_get_lanes(struct rk_i2s_tdm_dev *i2s_tdm, int stream)
{
	unsigned int lanes = 1;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (i2s_tdm->tx_lanes)
			lanes = i2s_tdm->tx_lanes;
	} else {
		if (i2s_tdm->rx_lanes)
			lanes = i2s_tdm->rx_lanes;
	}

	return lanes;
}

static struct snd_soc_dai *rockchip_i2s_tdm_find_dai(struct device_node *np)
{
	struct snd_soc_dai_link_component dai_component = { 0 };

	dai_component.of_node = np;

	return snd_soc_find_dai_with_mutex(&dai_component);
}

static int rockchip_i2s_tdm_multi_lanes_set_clk(struct snd_pcm_substream *substream,
						struct snd_pcm_hw_params *params,
						struct snd_soc_dai *cpu_dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(cpu_dai);
	struct snd_soc_dai *dai = i2s_tdm->clk_src_dai;
	unsigned int div, mclk_rate;
	unsigned int lanes, ch_per_lane;

	lanes = rockchip_i2s_tdm_get_lanes(i2s_tdm, substream->stream);
	ch_per_lane = params_channels(params) / lanes;
	mclk_rate = ch_per_lane * params_rate(params) * 32;
	div = ch_per_lane / 2;

	/* Do nothing when use external clk src */
	if (dai && dai->driver->ops) {
		if (dai->driver->ops->set_sysclk)
			dai->driver->ops->set_sysclk(dai, substream->stream, mclk_rate, 0);

		writel(XFER_DIS, i2s_tdm->clk_src_base + I2S_XFER);
		writel(CKR_V(64, div, div), i2s_tdm->clk_src_base + I2S_CKR);
		writel(XFER_EN, i2s_tdm->clk_src_base + I2S_XFER);
	}

	i2s_tdm->lrck_ratio = div;
	i2s_tdm->mclk_tx_freq = mclk_rate;
	i2s_tdm->mclk_rx_freq = mclk_rate;

	return 0;
}

static int rockchip_i2s_tdm_fsxn_start(struct rk_i2s_tdm_dev *i2s_tdm, int stream)
{
	struct gpio_desc *fsn;
	unsigned int msk, val;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		fsn = i2s_tdm->fsxn_tx_gpio;
		msk = I2S_XFER_TXS_MASK;
		val = I2S_XFER_TXS_START;
	} else {
		fsn = i2s_tdm->fsxn_rx_gpio;
		msk = I2S_XFER_RXS_MASK;
		val = I2S_XFER_RXS_START;
	}

	if (!fsn)
		return -ENODEV;

	regmap_update_bits(i2s_tdm->regmap, I2S_XFER, msk, val);
	udelay(10);
	gpiod_set_value(fsn, 1);

	return 0;
}

static int rockchip_i2s_tdm_fsxn_stop(struct rk_i2s_tdm_dev *i2s_tdm, int stream)
{
	struct gpio_desc *fsn;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		fsn = i2s_tdm->fsxn_tx_gpio;
	else
		fsn = i2s_tdm->fsxn_rx_gpio;

	if (!fsn)
		return -ENODEV;

	gpiod_set_value(fsn, 0);

	return 0;
}

static int rockchip_i2s_tdm_multi_lanes_start(struct rk_i2s_tdm_dev *i2s_tdm, int stream)
{
	unsigned int tdm_h = 0, tdm_l = 0, i2s_h = 0, i2s_l = 0;
	unsigned int msk, val, reg, fmt;
	unsigned long flags;
	int ret;

	ret = rockchip_i2s_tdm_fsxn_start(i2s_tdm, stream);
	if (ret == 0)
		return 0;

	if (!i2s_tdm->tdm_fsync_gpio || !i2s_tdm->i2s_lrck_gpio)
		return -ENOSYS;

	if (i2s_tdm->lrck_ratio != 4 && i2s_tdm->lrck_ratio != 8)
		return -EINVAL;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		msk = I2S_XFER_TXS_MASK;
		val = I2S_XFER_TXS_START;
		reg = I2S_TXCR;
	} else {
		msk = I2S_XFER_RXS_MASK;
		val = I2S_XFER_RXS_START;
		reg = I2S_RXCR;
	}

	regmap_read(i2s_tdm->regmap, reg, &fmt);
	fmt = I2S_XCR_IBM_V(fmt);

	local_irq_save(flags);

	if (!rockchip_i2s_tdm_clk_valid(i2s_tdm, 1)) {
		local_irq_restore(flags);
		dev_err(i2s_tdm->dev, "Invalid LRCK / FSYNC measured by ref IO\n");
		return -EINVAL;
	}

	switch (fmt) {
	case I2S_XCR_IBM_NORMAL:
		tdm_h = rockchip_i2s_tdm_clk_assert_h(i2s_tdm->tdm_fsync_gpio);
		tdm_l = rockchip_i2s_tdm_clk_assert_l(i2s_tdm->tdm_fsync_gpio);

		if (i2s_tdm->lrck_ratio == 8) {
			rockchip_i2s_tdm_clk_assert_l(i2s_tdm->i2s_lrck_gpio);
			rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);
			rockchip_i2s_tdm_clk_assert_l(i2s_tdm->i2s_lrck_gpio);
			rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);
		}

		i2s_l = rockchip_i2s_tdm_clk_assert_l(i2s_tdm->i2s_lrck_gpio);

		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			i2s_h = rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);
		break;
	case I2S_XCR_IBM_LSJM:
		tdm_l = rockchip_i2s_tdm_clk_assert_l(i2s_tdm->tdm_fsync_gpio);
		tdm_h = rockchip_i2s_tdm_clk_assert_h(i2s_tdm->tdm_fsync_gpio);

		if (i2s_tdm->lrck_ratio == 8) {
			rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);
			rockchip_i2s_tdm_clk_assert_l(i2s_tdm->i2s_lrck_gpio);
			rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);
			rockchip_i2s_tdm_clk_assert_l(i2s_tdm->i2s_lrck_gpio);
		}

		rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);

		i2s_l = rockchip_i2s_tdm_clk_assert_l(i2s_tdm->i2s_lrck_gpio);
		i2s_h = rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);
		break;
	default:
		local_irq_restore(flags);
		return -EINVAL;
	}

	regmap_update_bits(i2s_tdm->regmap, I2S_XFER, msk, val);
	local_irq_restore(flags);

	dev_dbg(i2s_tdm->dev, "STREAM[%d]: TDM-H: %d, TDM-L: %d, I2S-H: %d, I2S-L: %d\n", stream,
		CLK_MAX_COUNT - tdm_h, CLK_MAX_COUNT - tdm_l,
		CLK_MAX_COUNT - i2s_h, CLK_MAX_COUNT - i2s_l);

	return 0;
}

static int rockchip_i2s_tdm_multi_lanes_parse(struct rk_i2s_tdm_dev *i2s_tdm)
{
	struct device_node *clk_src_node = NULL;
	enum gpiod_flags gpiod_flags;
	unsigned int val;
	int ret;

	i2s_tdm->is_tdm_multi_lanes =
		device_property_read_bool(i2s_tdm->dev, "rockchip,tdm-multi-lanes");

	if (!i2s_tdm->is_tdm_multi_lanes)
		return 0;

	i2s_tdm->tx_lanes = 1;
	i2s_tdm->rx_lanes = 1;

	if (!device_property_read_u32(i2s_tdm->dev, "rockchip,tdm-tx-lanes", &val)) {
		if ((val >= 1) && (val <= 4))
			i2s_tdm->tx_lanes = val;
	}

	if (!device_property_read_u32(i2s_tdm->dev, "rockchip,tdm-rx-lanes", &val)) {
		if ((val >= 1) && (val <= 4))
			i2s_tdm->rx_lanes = val;
	}

	i2s_tdm->fsxn_rx_gpio = devm_gpiod_get_optional(i2s_tdm->dev, "fsxn-rx",
							GPIOD_OUT_LOW);
	if (IS_ERR(i2s_tdm->fsxn_rx_gpio)) {
		ret = PTR_ERR(i2s_tdm->fsxn_rx_gpio);
		dev_err(i2s_tdm->dev, "Failed to get fsxn_rx_gpio %d\n", ret);
		return ret;
	}

	i2s_tdm->fsxn_tx_gpio = devm_gpiod_get_optional(i2s_tdm->dev, "fsxn-tx",
							GPIOD_OUT_LOW);
	if (IS_ERR(i2s_tdm->fsxn_tx_gpio)) {
		ret = PTR_ERR(i2s_tdm->fsxn_tx_gpio);
		dev_err(i2s_tdm->dev, "Failed to get fsxn_tx_gpio %d\n", ret);
		return ret;
	}

	if (i2s_tdm->fsxn_rx_gpio || i2s_tdm->fsxn_tx_gpio)
		dev_info(i2s_tdm->dev, "FSXN Mode\n");

	/* It's optional, required when use soc clk src, such as: i2s2_2ch */
	clk_src_node = of_parse_phandle(i2s_tdm->dev->of_node, "rockchip,clk-src", 0);
	gpiod_flags = clk_src_node ? GPIOD_ASIS : GPIOD_IN;
	/*
	 * Two situation for 'tdm-fsync':
	 *
	 * A. when the pin is a generic gpio as the ref signal pin which is drived from
	 *    external. should use flag GPIOD_IN to reclaim as GPIO_IN function.
	 *
	 * B. when the pin is the same pin from the 'clk-src' on the same SoC, we can
	 *    use the 'clk-src' fsync out signal as the 'tdm-fsync' to query status.
	 *    in this case, should use flag GPIOD_ASIS not to reclaim it as GPIO.
	 */
	i2s_tdm->tdm_fsync_gpio = devm_gpiod_get_optional(i2s_tdm->dev, "tdm-fsync",
							  gpiod_flags);
	if (IS_ERR(i2s_tdm->tdm_fsync_gpio)) {
		ret = PTR_ERR(i2s_tdm->tdm_fsync_gpio);
		dev_err(i2s_tdm->dev, "Failed to get tdm_fsync_gpio %d\n", ret);
		return ret;
	}

	if (clk_src_node) {
		i2s_tdm->clk_src_dai = rockchip_i2s_tdm_find_dai(clk_src_node);
		if (!i2s_tdm->clk_src_dai)
			return -EPROBE_DEFER;

		i2s_tdm->clk_src_base = of_iomap(clk_src_node, 0);
		if (!i2s_tdm->clk_src_base)
			return -ENOENT;

		pm_runtime_forbid(i2s_tdm->clk_src_dai->dev);
	}

	dev_info(i2s_tdm->dev, "Used as TDM_MULTI_LANES mode\n");

	return 0;
}
#endif

static int rockchip_i2s_tdm_slave_one_frame_start(struct rk_i2s_tdm_dev *i2s_tdm,
						   int stream)
{
	unsigned int msk, val, h;
	unsigned long flags;
	bool sof;

	sof = i2s_tdm->tdm_mode && !i2s_tdm->is_master_mode &&
	      !i2s_tdm->tdm_fsync_half_frame;

	if (!sof)
		return -ENOSYS;

	if (!i2s_tdm->i2s_lrck_gpio) {
		dev_err(i2s_tdm->dev, "SOF: should assign 'i2s-lrck-gpio' the pin used in DT\n");
		return -EINVAL;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		msk = I2S_XFER_TXS_MASK;
		val = I2S_XFER_TXS_START;
	} else {
		msk = I2S_XFER_RXS_MASK;
		val = I2S_XFER_RXS_START;
	}

	local_irq_save(flags);
	if (!rockchip_i2s_tdm_clk_valid(i2s_tdm, 0)) {
		local_irq_restore(flags);
		dev_err(i2s_tdm->dev, "SOF: invalid LRCK, please check 'i2s-lrck-gpio' in DT\n");
		return -EINVAL;
	}
	h = rockchip_i2s_tdm_clk_assert_h(i2s_tdm->i2s_lrck_gpio);
	regmap_update_bits(i2s_tdm->regmap, I2S_XFER, msk, val);
	local_irq_restore(flags);

	dev_dbg(i2s_tdm->dev, "STREAM[%d]: TDM-H: %d\n", stream, CLK_MAX_COUNT - h);

	return 0;
}

static int rockchip_i2s_tdm_xfer_with_gate(struct rk_i2s_tdm_dev *i2s_tdm)
{
	struct clk *mclk = NULL;

	switch (i2s_tdm->clk_trcm) {
	case TRCM_TX:
		mclk = i2s_tdm->mclk_tx;
		break;
	case TRCM_RX:
		mclk = i2s_tdm->mclk_rx;
		break;
	default:
		dev_err(i2s_tdm->dev, "Must use in TRCM mode.\n");
		return -EINVAL;
	}

	rockchip_utils_clk_gate_endisable(i2s_tdm->dev, mclk, 0);
	udelay(10);
	regmap_update_bits(i2s_tdm->regmap, I2S_XFER,
			   I2S_XFER_TXS_MASK |
			   I2S_XFER_RXS_MASK,
			   I2S_XFER_TXS_START |
			   I2S_XFER_RXS_START);
	udelay(10);
	rockchip_utils_clk_gate_endisable(i2s_tdm->dev, mclk, 1);

	return 0;
}

static int rockchip_i2s_tdm_trcm_xfer(struct rk_i2s_tdm_dev *i2s_tdm)
{
	/* No need to do GATE for HAVE_SYNC_RESET case */
	if (i2s_tdm->soc_data && i2s_tdm->soc_data->src_clk_ctrl)
		return regmap_update_bits(i2s_tdm->regmap, I2S_XFER,
					  I2S_XFER_TXS_MASK |
					  I2S_XFER_RXS_MASK,
					  I2S_XFER_TXS_START |
					  I2S_XFER_RXS_START);

	return rockchip_i2s_tdm_xfer_with_gate(i2s_tdm);
}

static void rockchip_i2s_tdm_xfer_start(struct rk_i2s_tdm_dev *i2s_tdm,
					int stream)
{
#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	if (i2s_tdm->is_tdm_multi_lanes) {
		if (rockchip_i2s_tdm_multi_lanes_start(i2s_tdm, stream) != -ENOSYS)
			return;
	}
#endif
	if (rockchip_i2s_tdm_slave_one_frame_start(i2s_tdm, stream) != -ENOSYS)
		return;

	if (i2s_tdm->clk_trcm) {
		rockchip_i2s_tdm_reset_assert(i2s_tdm);
		rockchip_i2s_tdm_trcm_xfer(i2s_tdm);
		rockchip_i2s_tdm_reset_deassert(i2s_tdm);
	} else if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_update_bits(i2s_tdm->regmap, I2S_XFER,
				   I2S_XFER_TXS_MASK,
				   I2S_XFER_TXS_START);
	} else {
		regmap_update_bits(i2s_tdm->regmap, I2S_XFER,
				   I2S_XFER_RXS_MASK,
				   I2S_XFER_RXS_START);
	}
}

static void rockchip_i2s_tdm_xfer_stop(struct rk_i2s_tdm_dev *i2s_tdm,
				       int stream, bool force)
{
	unsigned int msk, val, clr;

	if (i2s_tdm->quirks & QUIRK_ALWAYS_ON && !force)
		return;

	if (i2s_tdm->clk_trcm) {
		msk = I2S_XFER_TXS_MASK | I2S_XFER_RXS_MASK;
		val = I2S_XFER_TXS_STOP | I2S_XFER_RXS_STOP;
		clr = I2S_CLR_TXC | I2S_CLR_RXC;
	} else if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		msk = I2S_XFER_TXS_MASK;
		val = I2S_XFER_TXS_STOP;
		clr = I2S_CLR_TXC;
	} else {
		msk = I2S_XFER_RXS_MASK;
		val = I2S_XFER_RXS_STOP;
		clr = I2S_CLR_RXC;
	}

	regmap_update_bits(i2s_tdm->regmap, I2S_XFER, msk, val);

	/* delay for LRCK signal integrity */
	udelay(150);

	rockchip_i2s_tdm_clear(i2s_tdm, clr);

#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	rockchip_i2s_tdm_fsxn_stop(i2s_tdm, stream);
#endif

	dev_dbg(i2s_tdm->dev, "%s: stream: %d force: %d\n",
		__func__, stream, force);
}

static void rockchip_i2s_tdm_xfer_trcm_start(struct rk_i2s_tdm_dev *i2s_tdm,
					     int stream)
{
	int bstream = SNDRV_PCM_STREAM_LAST - stream;
	unsigned long flags;
	u32 val, en;

	spin_lock_irqsave(&i2s_tdm->lock, flags);
	if (++i2s_tdm->refcount == 1) {
		regmap_read(i2s_tdm->regmap, I2S_DMACR, &val);
		en = I2S_DMACR_RDE(1) | I2S_DMACR_TDE(1);
		if ((val & en) != en) {
			rockchip_trcm_dma_guard_ctrl(i2s_tdm, bstream, 1);
			rockchip_i2s_tdm_dma_ctrl(i2s_tdm, bstream, 1);
		}
		rockchip_i2s_tdm_xfer_start(i2s_tdm, 0);
	}
	spin_unlock_irqrestore(&i2s_tdm->lock, flags);
}

static void rockchip_i2s_tdm_xfer_trcm_stop(struct rk_i2s_tdm_dev *i2s_tdm,
					    int stream)
{
	unsigned long flags;

	spin_lock_irqsave(&i2s_tdm->lock, flags);
	if (--i2s_tdm->refcount == 0)
		rockchip_i2s_tdm_xfer_stop(i2s_tdm, 0, false);
	rockchip_i2s_tdm_dma_ctrl(i2s_tdm, stream, 1);
	spin_unlock_irqrestore(&i2s_tdm->lock, flags);
}

static void rockchip_i2s_tdm_trcm_pause(struct snd_pcm_substream *substream,
					struct rk_i2s_tdm_dev *i2s_tdm)
{
	int stream = substream->stream;
	int bstream = SNDRV_PCM_STREAM_LAST - stream;

	if (i2s_tdm->pcm_comp)
		rockchip_trcm_dma_guard_ctrl(i2s_tdm, stream, 0);

	/* store the current state, prepare for resume if necessary */
	i2s_tdm->is_dma_active[bstream] = is_dma_active(i2s_tdm, bstream);

	/* disable dma for both tx and rx */
	rockchip_i2s_tdm_dma_ctrl(i2s_tdm, stream, 0);
	rockchip_i2s_tdm_dma_ctrl(i2s_tdm, bstream, 0);
	rockchip_i2s_tdm_xfer_stop(i2s_tdm, bstream, true);
}

static void rockchip_i2s_tdm_trcm_resume(struct snd_pcm_substream *substream,
					 struct rk_i2s_tdm_dev *i2s_tdm)
{
	int stream = substream->stream;
	int bstream = SNDRV_PCM_STREAM_LAST - substream->stream;

	if (i2s_tdm->pcm_comp) {
		rockchip_trcm_dma_guard_ctrl(i2s_tdm, stream, 1);
		rockchip_i2s_tdm_dma_ctrl(i2s_tdm, stream, 1);
	}

	if (i2s_tdm->is_dma_active[bstream])
		rockchip_i2s_tdm_dma_ctrl(i2s_tdm, bstream, 1);

	rockchip_i2s_tdm_xfer_start(i2s_tdm, bstream);
}

static void rockchip_i2s_tdm_start(struct rk_i2s_tdm_dev *i2s_tdm, int stream)
{
	/*
	 * On HDMI-PATH-ALWAYS-ON situation, we almost keep XFER always on,
	 * so, for new data start, suggested to STOP-CLEAR-START to make sure
	 * data aligned.
	 */
	if ((i2s_tdm->quirks & QUIRK_HDMI_PATH) &&
	    (i2s_tdm->quirks & QUIRK_ALWAYS_ON) &&
	    (stream == SNDRV_PCM_STREAM_PLAYBACK)) {
		rockchip_i2s_tdm_xfer_stop(i2s_tdm, stream, true);
	}

	rockchip_i2s_tdm_dma_ctrl(i2s_tdm, stream, 1);

	if (i2s_tdm->clk_trcm)
		rockchip_i2s_tdm_xfer_trcm_start(i2s_tdm, stream);
	else
		rockchip_i2s_tdm_xfer_start(i2s_tdm, stream);
}

static void rockchip_i2s_tdm_stop(struct rk_i2s_tdm_dev *i2s_tdm, int stream)
{
	rockchip_i2s_tdm_dma_ctrl(i2s_tdm, stream, 0);

	if (i2s_tdm->clk_trcm)
		rockchip_i2s_tdm_xfer_trcm_stop(i2s_tdm, stream);
	else
		rockchip_i2s_tdm_xfer_stop(i2s_tdm, stream, false);
}

static int rockchip_i2s_tdm_parse_channels(struct rk_i2s_tdm_dev *i2s_tdm,
					   int stream, int channels)
{
	unsigned int reg_fmt, fmt;
	int ret = 0;

#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	if (i2s_tdm->is_tdm_multi_lanes) {
		unsigned int lanes = rockchip_i2s_tdm_get_lanes(i2s_tdm, stream);

		switch (lanes) {
		case 4:
			ret = I2S_CHN_8;
			break;
		case 3:
			ret = I2S_CHN_6;
			break;
		case 2:
			ret = I2S_CHN_4;
			break;
		case 1:
			ret = I2S_CHN_2;
			break;
		default:
			ret = -EINVAL;
			break;
		}

		return ret;
	}
#endif
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		reg_fmt = I2S_TXCR;
	else
		reg_fmt = I2S_RXCR;

	regmap_read(i2s_tdm->regmap, reg_fmt, &fmt);
	fmt &= I2S_TXCR_TFS_MASK;

	if (fmt == I2S_TXCR_TFS_TDM_I2S && !i2s_tdm->tdm_fsync_half_frame) {
		switch (channels) {
		case 16:
			ret = I2S_CHN_8;
			break;
		case 12:
			ret = I2S_CHN_6;
			break;
		case 8:
			ret = I2S_CHN_4;
			break;
		case 4:
			ret = I2S_CHN_2;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	} else {
		switch (channels) {
		case 8:
			ret = I2S_CHN_8;
			break;
		case 6:
			ret = I2S_CHN_6;
			break;
		case 4:
			ret = I2S_CHN_4;
			break;
		case 2:
			ret = I2S_CHN_2;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

static int rockchip_i2s_tdm_set_fmt(struct snd_soc_dai *cpu_dai,
				    unsigned int fmt)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(cpu_dai);
	unsigned int mask, val, tdm_val, txcr_val, rxcr_val;
	int ret;
	bool is_tdm = i2s_tdm->tdm_mode;

	ret = pm_runtime_resume_and_get(cpu_dai->dev);
	if (ret < 0 && ret != -EACCES)
		return ret;

	mask = I2S_CKR_MSS_MASK;
	switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_BP_FP:
		val = I2S_CKR_MSS_MASTER;
		i2s_tdm->is_master_mode = true;
		break;
	case SND_SOC_DAIFMT_BC_FC:
		val = I2S_CKR_MSS_SLAVE;
		i2s_tdm->is_master_mode = false;
		/*
		 * TRCM require TX/RX enabled at the same time, or need the one
		 * which provide clk enabled at first for master mode.
		 *
		 * It is quite a different for slave mode which does not have
		 * these restrictions, because the BCLK / LRCK are provided by
		 * external master devices.
		 *
		 * So, we just set the right clk path value on TRCM register on
		 * stage probe and then drop the trcm value to make TX / RX work
		 * independently.
		 */
		i2s_tdm->clk_trcm = 0;
		break;
	default:
		ret = -EINVAL;
		goto err_pm_put;
	}

	regmap_update_bits(i2s_tdm->regmap, I2S_CKR, mask, val);

	mask = I2S_CKR_CKP_MASK | I2S_CKR_TLP_MASK | I2S_CKR_RLP_MASK;
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		val = I2S_CKR_CKP_NORMAL |
		      I2S_CKR_TLP_NORMAL |
		      I2S_CKR_RLP_NORMAL;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		val = I2S_CKR_CKP_NORMAL |
		      I2S_CKR_TLP_INVERTED |
		      I2S_CKR_RLP_INVERTED;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		val = I2S_CKR_CKP_INVERTED |
		      I2S_CKR_TLP_NORMAL |
		      I2S_CKR_RLP_NORMAL;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		val = I2S_CKR_CKP_INVERTED |
		      I2S_CKR_TLP_INVERTED |
		      I2S_CKR_RLP_INVERTED;
		break;
	default:
		ret = -EINVAL;
		goto err_pm_put;
	}

	regmap_update_bits(i2s_tdm->regmap, I2S_CKR, mask, val);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_RIGHT_J:
		txcr_val = I2S_TXCR_IBM_RSJM;
		rxcr_val = I2S_RXCR_IBM_RSJM;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		txcr_val = I2S_TXCR_IBM_LSJM;
		rxcr_val = I2S_RXCR_IBM_LSJM;
		break;
	case SND_SOC_DAIFMT_I2S:
		txcr_val = I2S_TXCR_IBM_NORMAL;
		rxcr_val = I2S_RXCR_IBM_NORMAL;
		break;
	case SND_SOC_DAIFMT_DSP_A: /* PCM delay 1 mode */
		txcr_val = I2S_TXCR_TFS_PCM | I2S_TXCR_PBM_MODE(1);
		rxcr_val = I2S_RXCR_TFS_PCM | I2S_RXCR_PBM_MODE(1);
		break;
	case SND_SOC_DAIFMT_DSP_B: /* PCM no delay mode */
		txcr_val = I2S_TXCR_TFS_PCM;
		rxcr_val = I2S_RXCR_TFS_PCM;
		break;
	default:
		ret = -EINVAL;
		goto err_pm_put;
	}

	mask = I2S_TXCR_IBM_MASK | I2S_TXCR_TFS_MASK | I2S_TXCR_PBM_MASK;
	regmap_update_bits(i2s_tdm->regmap, I2S_TXCR, mask, txcr_val);

	mask = I2S_RXCR_IBM_MASK | I2S_RXCR_TFS_MASK | I2S_RXCR_PBM_MASK;
	regmap_update_bits(i2s_tdm->regmap, I2S_RXCR, mask, rxcr_val);

	if (is_tdm) {
		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_RIGHT_J:
			val = I2S_TXCR_TFS_TDM_I2S;
			tdm_val = TDM_SHIFT_CTRL(2);
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			val = I2S_TXCR_TFS_TDM_I2S;
			tdm_val = TDM_SHIFT_CTRL(1);
			break;
		case SND_SOC_DAIFMT_I2S:
			val = I2S_TXCR_TFS_TDM_I2S;
			tdm_val = TDM_SHIFT_CTRL(0);
			break;
		case SND_SOC_DAIFMT_DSP_A:
			val = I2S_TXCR_TFS_TDM_PCM;
			tdm_val = TDM_SHIFT_CTRL(2);
			break;
		case SND_SOC_DAIFMT_DSP_B:
			val = I2S_TXCR_TFS_TDM_PCM;
			tdm_val = TDM_SHIFT_CTRL(4);
			break;
		default:
			ret = -EINVAL;
			goto err_pm_put;
		}

		tdm_val |= TDM_FSYNC_WIDTH_SEL1(1);
		if (i2s_tdm->tdm_fsync_half_frame)
			tdm_val |= TDM_FSYNC_WIDTH_HALF_FRAME;
		else
			tdm_val |= TDM_FSYNC_WIDTH_ONE_FRAME;

		mask = I2S_TXCR_TFS_MASK;
		regmap_update_bits(i2s_tdm->regmap, I2S_TXCR, mask, val);
		regmap_update_bits(i2s_tdm->regmap, I2S_RXCR, mask, val);

		mask = TDM_FSYNC_WIDTH_SEL1_MSK | TDM_FSYNC_WIDTH_SEL0_MSK |
		       TDM_SHIFT_CTRL_MSK;
		regmap_update_bits(i2s_tdm->regmap, I2S_TDM_TXCR,
				   mask, tdm_val);
		regmap_update_bits(i2s_tdm->regmap, I2S_TDM_RXCR,
				   mask, tdm_val);

		if (val == I2S_TXCR_TFS_TDM_I2S && !i2s_tdm->tdm_fsync_half_frame) {
			/* refine frame width for TDM_I2S_ONE_FRAME */
			mask = TDM_FRAME_WIDTH_MSK;
			tdm_val = TDM_FRAME_WIDTH(i2s_tdm->frame_width >> 1);
			regmap_update_bits(i2s_tdm->regmap, I2S_TDM_TXCR,
					   mask, tdm_val);
			regmap_update_bits(i2s_tdm->regmap, I2S_TDM_RXCR,
					   mask, tdm_val);
		}

		mask = I2S_TXCR_CSR_MASK;
		ret = rockchip_i2s_tdm_parse_channels(i2s_tdm, SNDRV_PCM_STREAM_PLAYBACK,
						      i2s_tdm->tdm_slots);
		if (ret < 0) {
			dev_err(i2s_tdm->dev, "Invalid slots: %d\n", i2s_tdm->tdm_slots);
			return ret;
		}

		val = ret;
		ret = 0;
		regmap_update_bits(i2s_tdm->regmap, I2S_TXCR, mask, val);
		regmap_update_bits(i2s_tdm->regmap, I2S_RXCR, mask, val);
	}

	/* Enable the xfer in the last card init stage. */
	if (i2s_tdm->quirks & QUIRK_ALWAYS_ON && !i2s_tdm->clk_trcm)
		rockchip_i2s_tdm_xfer_start(i2s_tdm, SNDRV_PCM_STREAM_PLAYBACK);

err_pm_put:
	pm_runtime_put(cpu_dai->dev);

	return ret;
}

static int rockchip_i2s_tdm_clk_set_rate(struct rk_i2s_tdm_dev *i2s_tdm,
					 struct clk *clk, unsigned long rate,
					 int ppm)
{
	unsigned long rate_target;
	int delta, ret;

	if (ppm == i2s_tdm->clk_ppm)
		return 0;

	ret = rockchip_pll_clk_compensation(clk, ppm);
	if (ret != -ENOSYS)
		goto out;

	delta = (ppm < 0) ? -1 : 1;
	delta *= (int)div64_u64((uint64_t)rate * (uint64_t)abs(ppm) + 500000, 1000000);

	rate_target = rate + delta;

	if (!rate_target)
		return -EINVAL;

	ret = clk_set_rate(clk, rate_target);
	if (ret)
		return ret;
out:
	if (!ret)
		i2s_tdm->clk_ppm = ppm;

	return ret;
}

static int rockchip_i2s_tdm_calibrate_mclk(struct rk_i2s_tdm_dev *i2s_tdm,
					   struct snd_pcm_substream *substream,
					   unsigned int lrck_freq)
{
	struct clk *mclk_root;
	struct clk *mclk_parent;
	unsigned int mclk_root_freq;
	unsigned int mclk_root_initial_freq;
	unsigned int mclk_parent_freq;
	unsigned int mclk_freq, freq, freq_req;
	unsigned int div, delta;
	u64 ppm;
	int ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mclk_parent = i2s_tdm->mclk_tx_src;
		mclk_freq = i2s_tdm->mclk_tx_freq;
	} else {
		mclk_parent = i2s_tdm->mclk_rx_src;
		mclk_freq = i2s_tdm->mclk_rx_freq;
	}

	switch (i2s_tdm->clk_trcm) {
	case TRCM_TX:
		mclk_parent = i2s_tdm->mclk_tx_src;
		mclk_freq = i2s_tdm->mclk_tx_freq;
		break;
	case TRCM_RX:
		mclk_parent = i2s_tdm->mclk_rx_src;
		mclk_freq = i2s_tdm->mclk_rx_freq;
		break;
	}

	switch (lrck_freq) {
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
	case 192000:
		mclk_root = i2s_tdm->mclk_root0;
		mclk_root_freq = i2s_tdm->mclk_root0_freq;
		mclk_root_initial_freq = i2s_tdm->mclk_root0_initial_freq;
		mclk_parent_freq = DEFAULT_MCLK_FS * 192000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
		mclk_root = i2s_tdm->mclk_root1;
		mclk_root_freq = i2s_tdm->mclk_root1_freq;
		mclk_root_initial_freq = i2s_tdm->mclk_root1_initial_freq;
		mclk_parent_freq = DEFAULT_MCLK_FS * 176400;
		break;
	default:
		dev_err(i2s_tdm->dev, "Invalid LRCK frequency: %u Hz\n",
			lrck_freq);
		return -EINVAL;
	}

	ret = clk_set_parent(mclk_parent, mclk_root);
	if (ret)
		return ret;

	ret = rockchip_i2s_tdm_clk_set_rate(i2s_tdm, mclk_root,
					    mclk_root_freq, 0);
	if (ret)
		return ret;

	delta = abs(mclk_root_freq % mclk_parent_freq - mclk_parent_freq);
	ppm = div64_u64((uint64_t)delta * 1000000, (uint64_t)mclk_root_freq);

	if (ppm) {
		div = DIV_ROUND_CLOSEST(mclk_root_initial_freq, mclk_parent_freq);
		if (!div)
			return -EINVAL;

		mclk_root_freq = mclk_parent_freq * round_up(div, 2);

		ret = clk_set_rate(mclk_root, mclk_root_freq);
		if (ret)
			return ret;

		i2s_tdm->mclk_root0_freq = clk_get_rate(i2s_tdm->mclk_root0);
		i2s_tdm->mclk_root1_freq = clk_get_rate(i2s_tdm->mclk_root1);
	}

	freq = clk_get_rate(mclk_parent);
	div = DIV_ROUND_CLOSEST(freq, mclk_freq);
	freq_req = mclk_freq * div;
	if (freq < freq_req - CLK_SHIFT_RATE_HZ_MAX ||
	    freq > freq_req + CLK_SHIFT_RATE_HZ_MAX) {
		dev_dbg(i2s_tdm->dev, "Change mclk parent freq from %d to %d\n",
			freq, mclk_parent_freq);
		ret = clk_set_rate(mclk_parent, mclk_parent_freq);
	}

	return ret;
}

static int rockchip_i2s_tdm_mclk_reparent(struct rk_i2s_tdm_dev *i2s_tdm)
{
	struct clk *parent;
	int ret = 0;

	/* reparent to the same clk on TRCM mode */
	switch (i2s_tdm->clk_trcm) {
	case TRCM_TX:
		parent = clk_get_parent(i2s_tdm->mclk_tx);
		/*
		 * API clk_has_parent is not available yet on GKI, so we
		 * use clk_set_parent directly and ignore the ret value.
		 * if the API has addressed on GKI, should remove it.
		 */
#ifdef CONFIG_NO_GKI
		if (clk_has_parent(i2s_tdm->mclk_rx, parent))
			ret = clk_set_parent(i2s_tdm->mclk_rx, parent);
#else
		clk_set_parent(i2s_tdm->mclk_rx, parent);
#endif
		break;
	case TRCM_RX:
		parent = clk_get_parent(i2s_tdm->mclk_rx);
#ifdef CONFIG_NO_GKI
		if (clk_has_parent(i2s_tdm->mclk_tx, parent))
			ret = clk_set_parent(i2s_tdm->mclk_tx, parent);
#else
		clk_set_parent(i2s_tdm->mclk_tx, parent);
#endif
		break;
	}

	return ret;
}

static int rockchip_i2s_tdm_set_mclk(struct rk_i2s_tdm_dev *i2s_tdm,
				     struct snd_pcm_substream *substream,
				     struct clk **mclk)
{
	unsigned int mclk_freq;
	int ret;

	if (i2s_tdm->clk_trcm) {
		if (i2s_tdm->mclk_tx_freq != i2s_tdm->mclk_rx_freq) {
			dev_err(i2s_tdm->dev,
				"clk_trcm, tx: %d and rx: %d should be the same\n",
				i2s_tdm->mclk_tx_freq,
				i2s_tdm->mclk_rx_freq);
			return -EINVAL;
		}

		ret = clk_set_rate(i2s_tdm->mclk_tx, i2s_tdm->mclk_tx_freq);
		if (ret)
			return ret;

		ret = clk_set_rate(i2s_tdm->mclk_rx, i2s_tdm->mclk_rx_freq);
		if (ret)
			return ret;

		ret = rockchip_i2s_tdm_mclk_reparent(i2s_tdm);
		if (ret)
			return ret;

		/* mclk_rx is also ok. */
		*mclk = i2s_tdm->mclk_tx;
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			*mclk = i2s_tdm->mclk_tx;
			mclk_freq = i2s_tdm->mclk_tx_freq;
		} else {
			*mclk = i2s_tdm->mclk_rx;
			mclk_freq = i2s_tdm->mclk_rx_freq;
		}

		ret = clk_set_rate(*mclk, mclk_freq);
		if (ret)
			return ret;
	}

	return 0;
}

static int rockchip_i2s_ch_to_io(unsigned int ch, bool substream_capture)
{
	if (substream_capture) {
		switch (ch) {
		case I2S_CHN_4:
			return I2S_IO_6CH_OUT_4CH_IN;
		case I2S_CHN_6:
			return I2S_IO_4CH_OUT_6CH_IN;
		case I2S_CHN_8:
			return I2S_IO_2CH_OUT_8CH_IN;
		default:
			return I2S_IO_8CH_OUT_2CH_IN;
		}
	} else {
		switch (ch) {
		case I2S_CHN_4:
			return I2S_IO_4CH_OUT_6CH_IN;
		case I2S_CHN_6:
			return I2S_IO_6CH_OUT_4CH_IN;
		case I2S_CHN_8:
			return I2S_IO_8CH_OUT_2CH_IN;
		default:
			return I2S_IO_2CH_OUT_8CH_IN;
		}
	}
}

static int rockchip_i2s_io_multiplex(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(dai);
	int usable_chs = MULTIPLEX_CH_MAX;
	unsigned int val = 0;

	if (!i2s_tdm->io_multiplex)
		return 0;

	if (IS_ERR(i2s_tdm->grf))
		return 0;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		struct snd_pcm_str *playback_str =
			&substream->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK];

		if (playback_str->substream_opened) {
			regmap_read(i2s_tdm->regmap, I2S_TXCR, &val);
			val &= I2S_TXCR_CSR_MASK;
			usable_chs = MULTIPLEX_CH_MAX - to_ch_num(val);
		}

		regmap_read(i2s_tdm->regmap, I2S_RXCR, &val);
		val &= I2S_RXCR_CSR_MASK;

		if (to_ch_num(val) > usable_chs) {
			dev_err(i2s_tdm->dev,
				"Capture channels (%d) > usable channels (%d)\n",
				to_ch_num(val), usable_chs);
			return -EINVAL;
		}

		rockchip_i2s_ch_to_io(val, true);
	} else {
		struct snd_pcm_str *capture_str =
			&substream->pcm->streams[SNDRV_PCM_STREAM_CAPTURE];

		if (capture_str->substream_opened) {
			regmap_read(i2s_tdm->regmap, I2S_RXCR, &val);
			val &= I2S_RXCR_CSR_MASK;
			usable_chs = MULTIPLEX_CH_MAX - to_ch_num(val);
		}

		regmap_read(i2s_tdm->regmap, I2S_TXCR, &val);
		val &= I2S_TXCR_CSR_MASK;

		if (to_ch_num(val) > usable_chs) {
			dev_err(i2s_tdm->dev,
				"Playback channels (%d) > usable channels (%d)\n",
				to_ch_num(val), usable_chs);
			return -EINVAL;
		}
	}

	val <<= i2s_tdm->soc_data->grf_shift;
	val |= (I2S_IO_DIRECTION_MASK << i2s_tdm->soc_data->grf_shift) << 16;
	regmap_write(i2s_tdm->grf, i2s_tdm->soc_data->grf_reg_offset, val);

	return 0;
}

static bool is_params_dirty(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai,
			    unsigned int div_bclk,
			    unsigned int div_lrck,
			    unsigned int fmt)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(dai);
	unsigned int last_div_bclk, last_div_lrck, last_fmt, val;

	regmap_read(i2s_tdm->regmap, I2S_CLKDIV, &val);
	last_div_bclk = ((val & I2S_CLKDIV_TXM_MASK) >> I2S_CLKDIV_TXM_SHIFT) + 1;
	if (last_div_bclk != div_bclk)
		return true;

	if (i2s_tdm->tdm_mode) {
		regmap_read(i2s_tdm->regmap,
			    substream->stream ? I2S_TDM_RXCR : I2S_TDM_TXCR, &val);
		last_div_lrck = TDM_FRAME_WIDTH_V(val);

		regmap_read(i2s_tdm->regmap,
			    substream->stream ? I2S_RXCR : I2S_TXCR, &val);
		val &= I2S_TXCR_TFS_MASK;
		if (val == I2S_TXCR_TFS_TDM_I2S && !i2s_tdm->tdm_fsync_half_frame)
			last_div_lrck <<= 1;
	} else {
		regmap_read(i2s_tdm->regmap, I2S_CKR, &val);
		last_div_lrck = I2S_CKR_TSD_V(val);
	}
	if (last_div_lrck != div_lrck)
		return true;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_read(i2s_tdm->regmap, I2S_TXCR, &val);
		last_fmt = val & (I2S_TXCR_VDW_MASK | I2S_TXCR_CSR_MASK);
	} else {
		regmap_read(i2s_tdm->regmap, I2S_RXCR, &val);
		last_fmt = val & (I2S_RXCR_VDW_MASK | I2S_RXCR_CSR_MASK);
	}
	if (last_fmt != fmt)
		return true;

	return false;
}

static int rockchip_i2s_tdm_params_trcm(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params,
					struct snd_soc_dai *dai,
					unsigned int div_bclk,
					unsigned int div_lrck,
					unsigned int fmt)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(dai);
	struct snd_soc_component *comp = i2s_tdm->pcm_comp;
	unsigned long flags;

	/* Prepare params changes for trcm dma guard resume */
	if (comp && comp->driver->hw_params)
		comp->driver->hw_params(comp, substream, params);

	spin_lock_irqsave(&i2s_tdm->lock, flags);
	if (i2s_tdm->refcount)
		rockchip_i2s_tdm_trcm_pause(substream, i2s_tdm);

	regmap_update_bits(i2s_tdm->regmap, I2S_CLKDIV,
			   I2S_CLKDIV_TXM_MASK | I2S_CLKDIV_RXM_MASK,
			   I2S_CLKDIV_TXM(div_bclk) | I2S_CLKDIV_RXM(div_bclk));
	regmap_update_bits(i2s_tdm->regmap, I2S_CKR,
			   I2S_CKR_TSD_MASK | I2S_CKR_RSD_MASK,
			   I2S_CKR_TSD(div_lrck) | I2S_CKR_RSD(div_lrck));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_update_bits(i2s_tdm->regmap, I2S_TXCR,
				   I2S_TXCR_VDW_MASK | I2S_TXCR_CSR_MASK,
				   fmt);
	else
		regmap_update_bits(i2s_tdm->regmap, I2S_RXCR,
				   I2S_RXCR_VDW_MASK | I2S_RXCR_CSR_MASK,
				   fmt);

	if (i2s_tdm->refcount)
		rockchip_i2s_tdm_trcm_resume(substream, i2s_tdm);
	spin_unlock_irqrestore(&i2s_tdm->lock, flags);

	return 0;
}

static int rockchip_i2s_tdm_params(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai,
				   unsigned int div_bclk,
				   unsigned int div_lrck,
				   unsigned int fmt)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(dai);
	int stream = substream->stream;

	if (is_stream_active(i2s_tdm, stream))
		rockchip_i2s_tdm_xfer_stop(i2s_tdm, stream, true);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_update_bits(i2s_tdm->regmap, I2S_CLKDIV,
				   I2S_CLKDIV_TXM_MASK,
				   I2S_CLKDIV_TXM(div_bclk));
		regmap_update_bits(i2s_tdm->regmap, I2S_CKR,
				   I2S_CKR_TSD_MASK,
				   I2S_CKR_TSD(div_lrck));
		regmap_update_bits(i2s_tdm->regmap, I2S_TXCR,
				   I2S_TXCR_VDW_MASK | I2S_TXCR_CSR_MASK,
				   fmt);
	} else {
		regmap_update_bits(i2s_tdm->regmap, I2S_CLKDIV,
				   I2S_CLKDIV_RXM_MASK,
				   I2S_CLKDIV_RXM(div_bclk));
		regmap_update_bits(i2s_tdm->regmap, I2S_CKR,
				   I2S_CKR_RSD_MASK,
				   I2S_CKR_RSD(div_lrck));
		regmap_update_bits(i2s_tdm->regmap, I2S_RXCR,
				   I2S_RXCR_VDW_MASK | I2S_RXCR_CSR_MASK,
				   fmt);
	}

	/*
	 * Bring back CLK ASAP after cfg changed to make SINK devices active
	 * on HDMI-PATH-ALWAYS-ON situation, this workaround for some TVs no
	 * sound issue. at the moment, it's 8K@60Hz display situation.
	 */
	if ((i2s_tdm->quirks & QUIRK_HDMI_PATH) &&
	    (i2s_tdm->quirks & QUIRK_ALWAYS_ON) &&
	    (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)) {
		rockchip_i2s_tdm_xfer_start(i2s_tdm, SNDRV_PCM_STREAM_PLAYBACK);
	}

	return 0;
}

static int rockchip_i2s_tdm_params_channels(struct snd_pcm_substream *substream,
					    struct snd_pcm_hw_params *params,
					    struct snd_soc_dai *dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(dai);

	return rockchip_i2s_tdm_parse_channels(i2s_tdm, substream->stream,
					       params_channels(params));
}

static void rockchip_i2s_tdm_get_performance(struct snd_pcm_substream *substream,
					     struct snd_pcm_hw_params *params,
					     struct snd_soc_dai *dai,
					     unsigned int csr)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(dai);
	unsigned int tdl;
	int fifo;

	regmap_read(i2s_tdm->regmap, I2S_DMACR, &tdl);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		fifo = I2S_DMACR_TDL_V(tdl) * I2S_TXCR_CSR_V(csr);
	else
		fifo = I2S_DMACR_RDL_V(tdl) * I2S_RXCR_CSR_V(csr);

	rockchip_utils_get_performance(substream, params, dai, fifo);
}

static int rockchip_i2s_tdm_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params,
				      struct snd_soc_dai *dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(dai);
	struct snd_dmaengine_dai_dma_data *dma_data;
	struct clk *mclk;
	int ret = 0;
	unsigned int val = 0;
	unsigned int mclk_rate, bclk_rate, lrck_rate, div_bclk = 4, div_lrck = 64;

	if (!rockchip_i2s_tdm_stream_valid(substream, dai))
		return 0;

#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	if (i2s_tdm->is_tdm_multi_lanes)
		rockchip_i2s_tdm_multi_lanes_set_clk(substream, params, dai);
#endif
	dma_data = snd_soc_dai_get_dma_data(dai, substream);
	dma_data->maxburst = MAXBURST_PER_FIFO * params_channels(params) / 2;

	if (i2s_tdm->mclk_calibrate)
		rockchip_i2s_tdm_calibrate_mclk(i2s_tdm, substream,
						params_rate(params));

	ret = rockchip_i2s_tdm_set_mclk(i2s_tdm, substream, &mclk);
	if (ret)
		return ret;

	mclk_rate = clk_get_rate(mclk);
	lrck_rate = params_rate(params) * i2s_tdm->lrck_ratio;
	bclk_rate = i2s_tdm->frame_width * lrck_rate;
	if (!bclk_rate) {
		return -EINVAL;
	}
	div_bclk = DIV_ROUND_CLOSEST(mclk_rate, bclk_rate);
	div_lrck = bclk_rate / lrck_rate;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		val |= I2S_TXCR_VDW(8);
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		val |= I2S_TXCR_VDW(16);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val |= I2S_TXCR_VDW(20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val |= I2S_TXCR_VDW(24);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE:
		val |= I2S_TXCR_VDW(32);
		break;
	default:
		return -EINVAL;
	}

	ret = rockchip_i2s_tdm_params_channels(substream, params, dai);
	if (ret < 0)
		return ret;

	rockchip_i2s_tdm_get_performance(substream, params, dai, ret);

	val |= ret;
	if (!is_params_dirty(substream, dai, div_bclk, div_lrck, val))
		return 0;

	if (i2s_tdm->clk_trcm)
		rockchip_i2s_tdm_params_trcm(substream, params, dai, div_bclk, div_lrck, val);
	else
		rockchip_i2s_tdm_params(substream, dai, div_bclk, div_lrck, val);

	return rockchip_i2s_io_multiplex(substream, dai);
}
static int rockchip_i2s_tdm_hw_free(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	if (!rockchip_i2s_tdm_stream_valid(substream, dai))
		return 0;

	rockchip_utils_put_performance(substream, dai);

	return 0;
}

static int rockchip_i2s_tdm_trigger(struct snd_pcm_substream *substream,
				    int cmd, struct snd_soc_dai *dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(dai);

	if (!rockchip_i2s_tdm_stream_valid(substream, dai))
		return 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		rockchip_i2s_tdm_start(i2s_tdm, substream->stream);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		rockchip_i2s_tdm_stop(i2s_tdm, substream->stream);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rockchip_i2s_tdm_set_sysclk(struct snd_soc_dai *cpu_dai, int stream,
				       unsigned int freq, int dir)
{
	struct rk_i2s_tdm_dev *i2s_tdm = to_info(cpu_dai);

	/* Put set mclk rate into rockchip_i2s_tdm_set_mclk() */
	if (i2s_tdm->clk_trcm) {
		i2s_tdm->mclk_tx_freq = freq;
		i2s_tdm->mclk_rx_freq = freq;
	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			i2s_tdm->mclk_tx_freq = freq;
		else
			i2s_tdm->mclk_rx_freq = freq;
	}

	dev_dbg(i2s_tdm->dev, "The target mclk_%s freq is: %d\n",
		stream ? "rx" : "tx", freq);

	return 0;
}

static int rockchip_i2s_tdm_clk_compensation_info(struct snd_kcontrol *kcontrol,
						  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = CLK_PPM_MIN;
	uinfo->value.integer.max = CLK_PPM_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int rockchip_i2s_tdm_clk_compensation_get(struct snd_kcontrol *kcontrol,
						 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = i2s_tdm->clk_ppm;

	return 0;
}

static int rockchip_i2s_tdm_clk_compensation_put(struct snd_kcontrol *kcontrol,
						 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);
	int ret = 0, ppm = 0;
	int changed = 0;
	unsigned long old_rate;

	if (ucontrol->value.integer.value[0] < CLK_PPM_MIN ||
	    ucontrol->value.integer.value[0] > CLK_PPM_MAX)
		return -EINVAL;

	ppm = ucontrol->value.integer.value[0];

	old_rate = clk_get_rate(i2s_tdm->mclk_root0);
	ret = rockchip_i2s_tdm_clk_set_rate(i2s_tdm, i2s_tdm->mclk_root0,
					    i2s_tdm->mclk_root0_freq, ppm);
	if (ret)
		return ret;
	if (old_rate != clk_get_rate(i2s_tdm->mclk_root0))
		changed = 1;

	if (clk_is_match(i2s_tdm->mclk_root0, i2s_tdm->mclk_root1))
		return changed;

	old_rate = clk_get_rate(i2s_tdm->mclk_root1);
	ret = rockchip_i2s_tdm_clk_set_rate(i2s_tdm, i2s_tdm->mclk_root1,
					    i2s_tdm->mclk_root1_freq, ppm);
	if (ret)
		return ret;
	if (old_rate != clk_get_rate(i2s_tdm->mclk_root1))
		changed = 1;

	return changed;
}

static struct snd_kcontrol_new rockchip_i2s_tdm_compensation_control = {
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,
	.name = "PCM Clk Compensation In PPM",
	.info = rockchip_i2s_tdm_clk_compensation_info,
	.get = rockchip_i2s_tdm_clk_compensation_get,
	.put = rockchip_i2s_tdm_clk_compensation_put,
};

/* loopback mode select */
enum {
	LOOPBACK_MODE_DIS = 0,
	LOOPBACK_MODE_1,
	LOOPBACK_MODE_2,
	LOOPBACK_MODE_2_SWAP,
};

static const char *const loopback_text[] = {
	"Disabled",
	"Mode1",
	"Mode2",
	"Mode2 Swap",
};

static SOC_ENUM_SINGLE_EXT_DECL(loopback_mode, loopback_text);

static int rockchip_i2s_tdm_loopback_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);
	unsigned int reg = 0, mode = 0;

	pm_runtime_get_sync(component->dev);
	regmap_read(i2s_tdm->regmap, I2S_XFER, &reg);
	pm_runtime_put(component->dev);

	switch (reg & I2S_XFER_LP_MODE_MASK) {
	case I2S_XFER_LP_MODE_2_SWAP:
		mode = LOOPBACK_MODE_2_SWAP;
		break;
	case I2S_XFER_LP_MODE_2:
		mode = LOOPBACK_MODE_2;
		break;
	case I2S_XFER_LP_MODE_1:
		mode = LOOPBACK_MODE_1;
		break;
	default:
		mode = LOOPBACK_MODE_DIS;
		break;
	}

	ucontrol->value.enumerated.item[0] = mode;

	return 0;
}

static int rockchip_i2s_tdm_loopback_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);
	unsigned int val = 0, mode = ucontrol->value.enumerated.item[0];

	if (mode < LOOPBACK_MODE_DIS ||
	    mode > LOOPBACK_MODE_2_SWAP)
		return -EINVAL;

	switch (mode) {
	case LOOPBACK_MODE_2_SWAP:
		val = I2S_XFER_LP_MODE_2_SWAP;
		break;
	case LOOPBACK_MODE_2:
		val = I2S_XFER_LP_MODE_2;
		break;
	case LOOPBACK_MODE_1:
		val = I2S_XFER_LP_MODE_1;
		break;
	default:
		val = I2S_XFER_LP_MODE_DIS;
		break;
	}

	pm_runtime_get_sync(component->dev);
	regmap_update_bits(i2s_tdm->regmap, I2S_XFER, I2S_XFER_LP_MODE_MASK, val);
	pm_runtime_put(component->dev);

	return 0;
}

static const char * const rpaths_text[] = {
	"From SDI0", "From SDI1", "From SDI2", "From SDI3" };

static const char * const tpaths_text[] = {
	"From PATH0", "From PATH1", "From PATH2", "From PATH3" };

/* TXCR */
static SOC_ENUM_SINGLE_DECL(tpath3_enum, I2S_TXCR, 29, tpaths_text);
static SOC_ENUM_SINGLE_DECL(tpath2_enum, I2S_TXCR, 27, tpaths_text);
static SOC_ENUM_SINGLE_DECL(tpath1_enum, I2S_TXCR, 25, tpaths_text);
static SOC_ENUM_SINGLE_DECL(tpath0_enum, I2S_TXCR, 23, tpaths_text);

/* RXCR */
static SOC_ENUM_SINGLE_DECL(rpath3_enum, I2S_RXCR, 23, rpaths_text);
static SOC_ENUM_SINGLE_DECL(rpath2_enum, I2S_RXCR, 21, rpaths_text);
static SOC_ENUM_SINGLE_DECL(rpath1_enum, I2S_RXCR, 19, rpaths_text);
static SOC_ENUM_SINGLE_DECL(rpath0_enum, I2S_RXCR, 17, rpaths_text);

static int rockchip_i2s_tdm_wait_time_info(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = WAIT_TIME_MS_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int rockchip_i2s_tdm_rd_wait_time_get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = i2s_tdm->wait_time[SNDRV_PCM_STREAM_CAPTURE];

	return 0;
}

static int rockchip_i2s_tdm_rd_wait_time_put(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] > WAIT_TIME_MS_MAX)
		return -EINVAL;

	i2s_tdm->wait_time[SNDRV_PCM_STREAM_CAPTURE] = ucontrol->value.integer.value[0];

	return 1;
}

static int rockchip_i2s_tdm_wr_wait_time_get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = i2s_tdm->wait_time[SNDRV_PCM_STREAM_PLAYBACK];

	return 0;
}

static int rockchip_i2s_tdm_wr_wait_time_put(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] > WAIT_TIME_MS_MAX)
		return -EINVAL;

	i2s_tdm->wait_time[SNDRV_PCM_STREAM_PLAYBACK] = ucontrol->value.integer.value[0];

	return 1;
}

#define SAI_PCM_WAIT_TIME(xname, xhandler_get, xhandler_put)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_PCM, .name = xname,	\
	.info = rockchip_i2s_tdm_wait_time_info,			\
	.get = xhandler_get, .put = xhandler_put }

static const struct snd_kcontrol_new rockchip_i2s_tdm_snd_controls[] = {
	SOC_ENUM("Receive PATH3 Source Select", rpath3_enum),
	SOC_ENUM("Receive PATH2 Source Select", rpath2_enum),
	SOC_ENUM("Receive PATH1 Source Select", rpath1_enum),
	SOC_ENUM("Receive PATH0 Source Select", rpath0_enum),
	SOC_ENUM("Transmit SDO3 Source Select", tpath3_enum),
	SOC_ENUM("Transmit SDO2 Source Select", tpath2_enum),
	SOC_ENUM("Transmit SDO1 Source Select", tpath1_enum),
	SOC_ENUM("Transmit SDO0 Source Select", tpath0_enum),

	SOC_ENUM_EXT("I2STDM Digital Loopback Mode", loopback_mode,
		     rockchip_i2s_tdm_loopback_get,
		     rockchip_i2s_tdm_loopback_put),
#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	SOC_ENUM_EXT("Transmit SDOx Select", tx_lanes_enum,
		     rockchip_i2s_tdm_tx_lanes_get, rockchip_i2s_tdm_tx_lanes_put),
	SOC_ENUM_EXT("Receive SDIx Select", rx_lanes_enum,
		     rockchip_i2s_tdm_rx_lanes_get, rockchip_i2s_tdm_rx_lanes_put),
#endif
	SAI_PCM_WAIT_TIME("PCM Read Wait Time MS",
			  rockchip_i2s_tdm_rd_wait_time_get,
			  rockchip_i2s_tdm_rd_wait_time_put),
	SAI_PCM_WAIT_TIME("PCM Write Wait Time MS",
			  rockchip_i2s_tdm_wr_wait_time_get,
			  rockchip_i2s_tdm_wr_wait_time_put),
};

static int rockchip_i2s_tdm_dai_probe(struct snd_soc_dai *dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_dai_get_drvdata(dai);

	if (i2s_tdm->has_capture)
		dai->capture_dma_data = &i2s_tdm->capture_dma_data;
	if (i2s_tdm->has_playback)
		dai->playback_dma_data = &i2s_tdm->playback_dma_data;

	if (i2s_tdm->mclk_calibrate)
		snd_soc_add_component_controls(dai->component,
					       &rockchip_i2s_tdm_compensation_control,
					       1);

	return 0;
}

static int rockchip_dai_tdm_slot(struct snd_soc_dai *dai,
				 unsigned int tx_mask, unsigned int rx_mask,
				 int slots, int slot_width)
{
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_dai_get_drvdata(dai);
	unsigned int mask, val, wl, fifos;
	int ret;

	i2s_tdm->tdm_mode = true;
	i2s_tdm->tdm_slots = slots;
	i2s_tdm->frame_width = slots * slot_width;

	mask = TDM_SLOT_BIT_WIDTH_MSK | TDM_FRAME_WIDTH_MSK;
	val = TDM_SLOT_BIT_WIDTH(slot_width) |
	      TDM_FRAME_WIDTH(slots * slot_width);

	ret = pm_runtime_resume_and_get(dai->dev);
	if (ret < 0 && ret != -EACCES)
		return ret;

	regmap_update_bits(i2s_tdm->regmap, I2S_TDM_TXCR, mask, val);
	regmap_update_bits(i2s_tdm->regmap, I2S_TDM_RXCR, mask, val);

	mask = I2S_TXCR_VDW_MASK;
	val = I2S_TXCR_VDW(slot_width);

	regmap_update_bits(i2s_tdm->regmap, I2S_TXCR, mask, val);
	regmap_update_bits(i2s_tdm->regmap, I2S_RXCR, mask, val);
	/*
	 * TDM mode use all FIFOs, the max burst is 16 word of DMAC,
	 * so we used the max FIFO to cover DDR dmc windows.
	 *
	 * 4 FIFOs controller:
	 *
	 * TDL:
	 *
	 * 16 word: WL = ((32 * 4) - 16) / 4 = 28
	 *
	 * RDL:
	 *
	 * 16 word: WL = 16 / 4 = 4
	 */
	if (!i2s_tdm->tdm_fsync_half_frame)
		fifos = slots / 4;
	else
		fifos = slots / 2;

	if (!fifos)
		fifos = 1;

	/* RK3568 I2S2/I2S3 TDM has only one FIFO */
	if (strstr(dev_name(dai->dev), "fe420000") || strstr(dev_name(dai->dev), "fe430000"))
		fifos = 1;

	wl = ((DEPTH_PER_FIFO * fifos) - MAXBURST) / fifos;
	regmap_update_bits(i2s_tdm->regmap, I2S_DMACR, I2S_DMACR_TDL_MASK,
			   I2S_DMACR_TDL(wl));
	wl = MAXBURST / fifos;
	regmap_update_bits(i2s_tdm->regmap, I2S_DMACR, I2S_DMACR_RDL_MASK,
			   I2S_DMACR_RDL(wl));

	pm_runtime_put(dai->dev);

	return 0;
}

static int rockchip_i2s_tdm_set_bclk_ratio(struct snd_soc_dai *dai,
					   unsigned int ratio)
{
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_dai_get_drvdata(dai);

	if (ratio < 32 || ratio > 512 || ratio % 2 == 1)
		return -EINVAL;

	i2s_tdm->frame_width = ratio;

	return 0;
}

static int rockchip_i2s_tdm_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_dai_get_drvdata(dai);
	int stream = substream->stream;

	if (!rockchip_i2s_tdm_stream_valid(substream, dai))
		return 0;

	if (i2s_tdm->substreams[stream])
		return -EBUSY;

	if (i2s_tdm->wait_time[stream])
		substream->wait_time = msecs_to_jiffies(i2s_tdm->wait_time[stream]);

	i2s_tdm->substreams[stream] = substream;

	return 0;
}

static void rockchip_i2s_tdm_shutdown(struct snd_pcm_substream *substream,
				      struct snd_soc_dai *dai)
{
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_dai_get_drvdata(dai);

	if (!rockchip_i2s_tdm_stream_valid(substream, dai))
		return;

	i2s_tdm->substreams[substream->stream] = NULL;
}

static int rockchip_i2s_tdm_comp_resume(struct snd_soc_component *component)
{
	struct rk_i2s_tdm_dev *i2s_tdm = snd_soc_component_get_drvdata(component);

	if (i2s_tdm->resume_deferred_ms)
		msleep(i2s_tdm->resume_deferred_ms);

	dev_dbg(component->dev, "%s: resume deferred %d ms\n",
		__func__, i2s_tdm->resume_deferred_ms);

	return 0;
}

static const struct snd_soc_dai_ops rockchip_i2s_tdm_dai_ops = {
	.startup = rockchip_i2s_tdm_startup,
	.shutdown = rockchip_i2s_tdm_shutdown,
	.hw_params = rockchip_i2s_tdm_hw_params,
	.hw_free = rockchip_i2s_tdm_hw_free,
	.set_bclk_ratio	= rockchip_i2s_tdm_set_bclk_ratio,
	.set_sysclk = rockchip_i2s_tdm_set_sysclk,
	.set_fmt = rockchip_i2s_tdm_set_fmt,
	.set_tdm_slot = rockchip_dai_tdm_slot,
	.trigger = rockchip_i2s_tdm_trigger,
};

static const struct snd_soc_component_driver rockchip_i2s_tdm_component = {
	.name = DRV_NAME,
	.legacy_dai_naming = 1,
	.controls = rockchip_i2s_tdm_snd_controls,
	.num_controls = ARRAY_SIZE(rockchip_i2s_tdm_snd_controls),
	.resume = rockchip_i2s_tdm_comp_resume,
};

static bool rockchip_i2s_tdm_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_TXCR:
	case I2S_RXCR:
	case I2S_CKR:
	case I2S_DMACR:
	case I2S_INTCR:
	case I2S_XFER:
	case I2S_CLR:
	case I2S_TXDR:
	case I2S_TDM_TXCR:
	case I2S_TDM_RXCR:
	case I2S_CLKDIV:
		return true;
	default:
		return false;
	}
}

static bool rockchip_i2s_tdm_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_TXCR:
	case I2S_RXCR:
	case I2S_CKR:
	case I2S_DMACR:
	case I2S_INTCR:
	case I2S_XFER:
	case I2S_CLR:
	case I2S_TXDR:
	case I2S_RXDR:
	case I2S_TXFIFOLR:
	case I2S_INTSR:
	case I2S_RXFIFOLR:
	case I2S_TDM_TXCR:
	case I2S_TDM_RXCR:
	case I2S_CLKDIV:
		return true;
	default:
		return false;
	}
}

static bool rockchip_i2s_tdm_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case I2S_TXFIFOLR:
	case I2S_INTCR:
	case I2S_INTSR:
	case I2S_CLR:
	case I2S_TXDR:
	case I2S_RXDR:
	case I2S_RXFIFOLR:
		return true;
	default:
		return false;
	}
}

static bool rockchip_i2s_tdm_precious_reg(struct device *dev, unsigned int reg)
{
	if (reg == I2S_RXDR)
		return true;
	return false;
}

static const struct reg_default rockchip_i2s_tdm_reg_defaults[] = {
	{0x00, 0x7200000f},
	{0x04, 0x01c8000f},
	{0x08, 0x00001f1f},
	{0x10, 0x001f0000},
	{0x14, 0x01f00000},
	{0x30, 0x00003eff},
	{0x34, 0x00003eff},
	{0x38, 0x00000707},
};

static const struct regmap_config rockchip_i2s_tdm_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = I2S_CLKDIV,
	.reg_defaults = rockchip_i2s_tdm_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(rockchip_i2s_tdm_reg_defaults),
	.writeable_reg = rockchip_i2s_tdm_wr_reg,
	.readable_reg = rockchip_i2s_tdm_rd_reg,
	.volatile_reg = rockchip_i2s_tdm_volatile_reg,
	.precious_reg = rockchip_i2s_tdm_precious_reg,
	.cache_type = REGCACHE_FLAT,
};

static int common_soc_init(struct device *dev, u32 addr)
{
	struct rk_i2s_tdm_dev *i2s_tdm = dev_get_drvdata(dev);
	const struct txrx_config *configs = i2s_tdm->soc_data->configs;
	u32 reg = 0, val = 0, trcm = i2s_tdm->clk_trcm;
	int i;

	if (trcm == TRCM_TXRX)
		return 0;

	if (IS_ERR(i2s_tdm->grf))
		return 0;

	for (i = 0; i < i2s_tdm->soc_data->config_count; i++) {
		if (addr != configs[i].addr)
			continue;
		reg = configs[i].reg;
		if (trcm == TRCM_TX)
			val = configs[i].txonly;
		else
			val = configs[i].rxonly;

		if (reg)
			regmap_write(i2s_tdm->grf, reg, val);
	}

	return 0;
}

static const struct txrx_config px30_txrx_config[] = {
	{ 0xff060000, 0x184, PX30_I2S0_CLK_TXONLY, PX30_I2S0_CLK_RXONLY },
};

static const struct txrx_config rk1808_txrx_config[] = {
	{ 0xff7e0000, 0x190, RK1808_I2S0_CLK_TXONLY, RK1808_I2S0_CLK_RXONLY },
};

static const struct txrx_config rk3308_txrx_config[] = {
	{ 0xff300000, 0x308, RK3308_I2S0_CLK_TXONLY, RK3308_I2S0_CLK_RXONLY },
	{ 0xff310000, 0x308, RK3308_I2S1_CLK_TXONLY, RK3308_I2S1_CLK_RXONLY },
};

static const struct txrx_config rk3568_txrx_config[] = {
	{ 0xfe410000, 0x504, RK3568_I2S1_CLK_TXONLY, RK3568_I2S1_CLK_RXONLY },
	{ 0xfe430000, 0x504, RK3568_I2S3_CLK_TXONLY, RK3568_I2S3_CLK_RXONLY },
	{ 0xfe430000, 0x508, RK3568_I2S3_MCLK_TXONLY, RK3568_I2S3_MCLK_RXONLY },
};

static const struct txrx_config rv1126_txrx_config[] = {
	{ 0xff800000, 0x10260, RV1126_I2S0_CLK_TXONLY, RV1126_I2S0_CLK_RXONLY },
};

static const struct rk_i2s_soc_data px30_i2s_soc_data = {
	.softrst_offset = 0x0300,
	.configs = px30_txrx_config,
	.config_count = ARRAY_SIZE(px30_txrx_config),
	.init = common_soc_init,
#ifdef HAVE_SYNC_RESET
	.src_clk_ctrl = rockchip_i2s_tdm_px30_src_clk_ctrl,
#endif
};

static const struct rk_i2s_soc_data rk1808_i2s_soc_data = {
	.softrst_offset = 0x0300,
	.configs = rk1808_txrx_config,
	.config_count = ARRAY_SIZE(rk1808_txrx_config),
	.init = common_soc_init,
#ifdef HAVE_SYNC_RESET
	.src_clk_ctrl = rockchip_i2s_tdm_rk1808_src_clk_ctrl,
#endif
};

static const struct rk_i2s_soc_data rk3308_i2s_soc_data = {
	.softrst_offset = 0x0400,
	.grf_reg_offset = 0x0308,
	.grf_shift = 5,
	.configs = rk3308_txrx_config,
	.config_count = ARRAY_SIZE(rk3308_txrx_config),
	.init = common_soc_init,
#ifdef HAVE_SYNC_RESET
	.src_clk_ctrl = rockchip_i2s_tdm_rk3308_src_clk_ctrl,
#endif
};

static const struct rk_i2s_soc_data rk3568_i2s_soc_data = {
	.softrst_offset = 0x0400,
	.configs = rk3568_txrx_config,
	.config_count = ARRAY_SIZE(rk3568_txrx_config),
	.init = common_soc_init,
};

static const struct rk_i2s_soc_data rv1126_i2s_soc_data = {
	.softrst_offset = 0x0300,
	.configs = rv1126_txrx_config,
	.config_count = ARRAY_SIZE(rv1126_txrx_config),
	.init = common_soc_init,
};

static const struct of_device_id rockchip_i2s_tdm_match[] = {
#ifdef CONFIG_CPU_PX30
	{ .compatible = "rockchip,px30-i2s-tdm", .data = &px30_i2s_soc_data },
#endif
#ifdef CONFIG_CPU_RK1808
	{ .compatible = "rockchip,rk1808-i2s-tdm", .data = &rk1808_i2s_soc_data },
#endif
#ifdef CONFIG_CPU_RK3308
	{ .compatible = "rockchip,rk3308-i2s-tdm", .data = &rk3308_i2s_soc_data },
#endif
#ifdef CONFIG_CPU_RK3568
	{ .compatible = "rockchip,rk3568-i2s-tdm", .data = &rk3568_i2s_soc_data },
#endif
#ifdef CONFIG_CPU_RK3588
	{ .compatible = "rockchip,rk3588-i2s-tdm", },
#endif
#ifdef CONFIG_CPU_RV1106
	{ .compatible = "rockchip,rv1106-i2s-tdm", },
#endif
#ifdef CONFIG_CPU_RV1126
	{ .compatible = "rockchip,rv1126-i2s-tdm", .data = &rv1126_i2s_soc_data },
#endif
	{},
};

static const struct snd_soc_dai_driver i2s_tdm_dai = {
	.probe = rockchip_i2s_tdm_dai_probe,
	.ops = &rockchip_i2s_tdm_dai_ops,
};

static int rockchip_i2s_tdm_init_dai(struct rk_i2s_tdm_dev *i2s_tdm)
{
	struct snd_soc_dai_driver *dai;
	struct property *dma_names;
	const char *dma_name;
	u64 formats = (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE |
		       SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE |
		       SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE);
	struct device_node *node = i2s_tdm->dev->of_node;

	of_property_for_each_string(node, "dma-names", dma_names, dma_name) {
		if (!strcmp(dma_name, "tx"))
			i2s_tdm->has_playback = true;
		if (!strcmp(dma_name, "rx"))
			i2s_tdm->has_capture = true;
	}

	dai = devm_kmemdup(i2s_tdm->dev, &i2s_tdm_dai,
			   sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	if (i2s_tdm->has_playback) {
		dai->playback.stream_name  = "Playback";
		dai->playback.channels_min = 2;
		dai->playback.channels_max = 64;
		dai->playback.rates = SNDRV_PCM_RATE_CONTINUOUS;
		dai->playback.formats = formats;
	}

	if (i2s_tdm->has_capture) {
		dai->capture.stream_name  = "Capture";
		dai->capture.channels_min = 2;
		dai->capture.channels_max = 64;
		dai->capture.rates = SNDRV_PCM_RATE_CONTINUOUS;
		dai->capture.formats = formats;
	}

	if (i2s_tdm->clk_trcm != TRCM_TXRX)
		dai->symmetric_rate = 1;

	i2s_tdm->dai = dai;

	return 0;
}

static int rockchip_i2s_tdm_path_check(struct rk_i2s_tdm_dev *i2s_tdm,
				       int num,
				       bool is_rx_path)
{
	unsigned int *i2s_data;
	int i, j;

	if (is_rx_path)
		i2s_data = i2s_tdm->i2s_sdis;
	else
		i2s_data = i2s_tdm->i2s_sdos;

	for (i = 0; i < num; i++) {
		if (i2s_data[i] > CH_GRP_MAX - 1) {
			dev_err(i2s_tdm->dev,
				"%s path i2s_data[%d]: %d is too high, max is: %d\n",
				is_rx_path ? "RX" : "TX",
				i, i2s_data[i], CH_GRP_MAX);
			return -EINVAL;
		}

		for (j = 0; j < num; j++) {
			if (i == j)
				continue;

			if (i2s_data[i] == i2s_data[j]) {
				dev_err(i2s_tdm->dev,
					"%s path invalid routed i2s_data: [%d]%d == [%d]%d\n",
					is_rx_path ? "RX" : "TX",
					i, i2s_data[i],
					j, i2s_data[j]);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static void rockchip_i2s_tdm_tx_path_config(struct rk_i2s_tdm_dev *i2s_tdm,
					    int num)
{
	int idx;

	for (idx = 0; idx < num; idx++) {
		regmap_update_bits(i2s_tdm->regmap, I2S_TXCR,
				   I2S_TXCR_PATH_MASK(idx),
				   I2S_TXCR_PATH(idx, i2s_tdm->i2s_sdos[idx]));
	}
}

static void rockchip_i2s_tdm_rx_path_config(struct rk_i2s_tdm_dev *i2s_tdm,
					    int num)
{
	int idx;

	for (idx = 0; idx < num; idx++) {
		regmap_update_bits(i2s_tdm->regmap, I2S_RXCR,
				   I2S_RXCR_PATH_MASK(idx),
				   I2S_RXCR_PATH(idx, i2s_tdm->i2s_sdis[idx]));
	}
}

static void rockchip_i2s_tdm_path_config(struct rk_i2s_tdm_dev *i2s_tdm,
					 int num, bool is_rx_path)
{
	if (is_rx_path)
		rockchip_i2s_tdm_rx_path_config(i2s_tdm, num);
	else
		rockchip_i2s_tdm_tx_path_config(i2s_tdm, num);
}

static int rockchip_i2s_tdm_get_calibrate_mclks(struct rk_i2s_tdm_dev *i2s_tdm)
{
	int num_mclks = 0;

	i2s_tdm->mclk_tx_src = devm_clk_get(i2s_tdm->dev, "mclk_tx_src");
	if (!IS_ERR(i2s_tdm->mclk_tx_src))
		num_mclks++;

	i2s_tdm->mclk_rx_src = devm_clk_get(i2s_tdm->dev, "mclk_rx_src");
	if (!IS_ERR(i2s_tdm->mclk_rx_src))
		num_mclks++;

	i2s_tdm->mclk_root0 = devm_clk_get(i2s_tdm->dev, "mclk_root0");
	if (!IS_ERR(i2s_tdm->mclk_root0))
		num_mclks++;

	i2s_tdm->mclk_root1 = devm_clk_get(i2s_tdm->dev, "mclk_root1");
	if (!IS_ERR(i2s_tdm->mclk_root1))
		num_mclks++;

	if (num_mclks < 4 && num_mclks != 0)
		return -ENOENT;

	if (num_mclks == 4)
		i2s_tdm->mclk_calibrate = 1;

	return 0;
}

static int rockchip_i2s_tdm_wait_time_init(struct rk_i2s_tdm_dev *i2s_tdm)
{
	unsigned int wait_time;

	if (!device_property_read_u32(i2s_tdm->dev, "rockchip,i2s-tx-wait-time-ms", &wait_time)) {
		dev_info(i2s_tdm->dev, "Init TX wait-time-ms: %d\n", wait_time);
		i2s_tdm->wait_time[SNDRV_PCM_STREAM_PLAYBACK] = wait_time;
	}

	if (!device_property_read_u32(i2s_tdm->dev, "rockchip,i2s-rx-wait-time-ms", &wait_time)) {
		dev_info(i2s_tdm->dev, "Init RX wait-time-ms: %d\n", wait_time);
		i2s_tdm->wait_time[SNDRV_PCM_STREAM_CAPTURE] = wait_time;
	}
	return 0;
}

static int rockchip_i2s_tdm_path_prepare(struct rk_i2s_tdm_dev *i2s_tdm,
					 struct device_node *np,
					 bool is_rx_path)
{
	char *i2s_tx_path_prop = "rockchip,i2s-tx-route";
	char *i2s_rx_path_prop = "rockchip,i2s-rx-route";
	char *i2s_path_prop;
	unsigned int *i2s_data;
	int num, ret = 0;

	if (is_rx_path) {
		i2s_path_prop = i2s_rx_path_prop;
		i2s_data = i2s_tdm->i2s_sdis;
	} else {
		i2s_path_prop = i2s_tx_path_prop;
		i2s_data = i2s_tdm->i2s_sdos;
	}

	num = of_count_phandle_with_args(np, i2s_path_prop, NULL);
	if (num < 0) {
		if (num != -ENOENT) {
			dev_err(i2s_tdm->dev,
				"Failed to read '%s' num: %d\n",
				i2s_path_prop, num);
			ret = num;
		}
		return ret;
	} else if (num != CH_GRP_MAX) {
		dev_err(i2s_tdm->dev,
			"The num: %d should be: %d\n", num, CH_GRP_MAX);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, i2s_path_prop,
					 i2s_data, num);
	if (ret < 0) {
		dev_err(i2s_tdm->dev,
			"Failed to read '%s': %d\n",
			i2s_path_prop, ret);
		return ret;
	}

	ret = rockchip_i2s_tdm_path_check(i2s_tdm, num, is_rx_path);
	if (ret < 0) {
		dev_err(i2s_tdm->dev,
			"Failed to check i2s data bus: %d\n", ret);
		return ret;
	}

	rockchip_i2s_tdm_path_config(i2s_tdm, num, is_rx_path);

	return 0;
}

static int rockchip_i2s_tdm_tx_path_prepare(struct rk_i2s_tdm_dev *i2s_tdm,
					    struct device_node *np)
{
	return rockchip_i2s_tdm_path_prepare(i2s_tdm, np, 0);
}

static int rockchip_i2s_tdm_rx_path_prepare(struct rk_i2s_tdm_dev *i2s_tdm,
					    struct device_node *np)
{
	return rockchip_i2s_tdm_path_prepare(i2s_tdm, np, 1);
}

static int rockchip_i2s_tdm_get_fifo_count(struct device *dev,
					   struct snd_pcm_substream *substream)
{
	struct rk_i2s_tdm_dev *i2s_tdm = dev_get_drvdata(dev);
	int val = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_read(i2s_tdm->regmap, I2S_TXFIFOLR, &val);
	else
		regmap_read(i2s_tdm->regmap, I2S_RXFIFOLR, &val);

	val = ((val & I2S_FIFOLR_TFL3_MASK) >> I2S_FIFOLR_TFL3_SHIFT) +
	      ((val & I2S_FIFOLR_TFL2_MASK) >> I2S_FIFOLR_TFL2_SHIFT) +
	      ((val & I2S_FIFOLR_TFL1_MASK) >> I2S_FIFOLR_TFL1_SHIFT) +
	      ((val & I2S_FIFOLR_TFL0_MASK) >> I2S_FIFOLR_TFL0_SHIFT);

	return val;
}

static const struct snd_dlp_config dconfig = {
	.get_fifo_count = rockchip_i2s_tdm_get_fifo_count,
};

static irqreturn_t rockchip_i2s_tdm_isr(int irq, void *devid)
{
	struct rk_i2s_tdm_dev *i2s_tdm = (struct rk_i2s_tdm_dev *)devid;
	struct snd_pcm_substream *substream;
	u32 val;

	regmap_read(i2s_tdm->regmap, I2S_INTSR, &val);
	if (val & I2S_INTSR_TXUI_ACT) {
		dev_warn_ratelimited(i2s_tdm->dev, "TX FIFO Underrun\n");
		regmap_update_bits(i2s_tdm->regmap, I2S_INTCR,
				   I2S_INTCR_TXUIC, I2S_INTCR_TXUIC);
		regmap_update_bits(i2s_tdm->regmap, I2S_INTCR,
				   I2S_INTCR_TXUIE_MASK,
				   I2S_INTCR_TXUIE(0));
		substream = i2s_tdm->substreams[SNDRV_PCM_STREAM_PLAYBACK];
		if (substream)
			snd_pcm_stop_xrun(substream);
	}

	if (val & I2S_INTSR_RXOI_ACT) {
		dev_warn_ratelimited(i2s_tdm->dev, "RX FIFO Overrun\n");
		regmap_update_bits(i2s_tdm->regmap, I2S_INTCR,
				   I2S_INTCR_RXOIC, I2S_INTCR_RXOIC);
		regmap_update_bits(i2s_tdm->regmap, I2S_INTCR,
				   I2S_INTCR_RXOIE_MASK,
				   I2S_INTCR_RXOIE(0));
		substream = i2s_tdm->substreams[SNDRV_PCM_STREAM_CAPTURE];
		if (substream)
			snd_pcm_stop_xrun(substream);
	}

	return IRQ_HANDLED;
}

static int rockchip_i2s_tdm_keep_clk_always_on(struct rk_i2s_tdm_dev *i2s_tdm)
{
	unsigned int mclk_rate = DEFAULT_FS * DEFAULT_MCLK_FS;
	unsigned int bclk_rate = i2s_tdm->frame_width * DEFAULT_FS;
	unsigned int div_lrck = i2s_tdm->frame_width;
	unsigned int div_bclk;
	int ret;

	if (mclk_rate < bclk_rate)
		mclk_rate = bclk_rate;

	div_bclk = DIV_ROUND_CLOSEST(mclk_rate, bclk_rate);

	/* assign generic freq */
	clk_set_rate(i2s_tdm->mclk_rx, mclk_rate);
	clk_set_rate(i2s_tdm->mclk_tx, mclk_rate);

	ret = rockchip_i2s_tdm_mclk_reparent(i2s_tdm);
	if (ret)
		return ret;

	regmap_update_bits(i2s_tdm->regmap, I2S_CLKDIV,
			   I2S_CLKDIV_RXM_MASK | I2S_CLKDIV_TXM_MASK,
			   I2S_CLKDIV_RXM(div_bclk) | I2S_CLKDIV_TXM(div_bclk));
	regmap_update_bits(i2s_tdm->regmap, I2S_CKR,
			   I2S_CKR_RSD_MASK | I2S_CKR_TSD_MASK,
			   I2S_CKR_RSD(div_lrck) | I2S_CKR_TSD(div_lrck));

	dev_info(i2s_tdm->dev, "CLK-ALWAYS-ON: mclk: %d, bclk: %d, fsync: %d\n",
		 mclk_rate, bclk_rate, DEFAULT_FS);

	return 0;
}

static int rockchip_i2s_tdm_register_platform(struct device *dev)
{
	struct rk_i2s_tdm_dev *i2s_tdm = dev_get_drvdata(dev);
	struct snd_soc_component *comp;
	int ret = 0;

	if (device_property_read_bool(dev, "rockchip,no-dmaengine")) {
		i2s_tdm->no_pcm = true;
		dev_info(dev, "Used for Multi-DAI\n");
		return 0;
	}

	if (device_property_read_bool(dev, "rockchip,digital-loopback")) {
		ret = devm_snd_dmaengine_dlp_register(dev, &dconfig);
		if (ret)
			dev_err(dev, "Could not register DLP\n");
		return ret;
	}

	if (i2s_tdm->clk_trcm) {
		ret =  devm_snd_dmaengine_trcm_register(dev);
		if (ret) {
			dev_err(dev, "Could not register TRCM PCM\n");
			return ret;
		}

		comp = snd_soc_lookup_component(i2s_tdm->dev,
						SND_DMAENGINE_TRCM_DRV_NAME);
		if (!comp) {
			dev_err(dev, "Could not find TRCM PCM\n");
			ret = -ENODEV;
		}

		i2s_tdm->pcm_comp = comp;

		return ret;
	}

	ret = devm_snd_dmaengine_pcm_register(dev, NULL, 0);
	if (ret)
		dev_err(dev, "Could not register PCM\n");

	return ret;
}

static int __maybe_unused i2s_tdm_runtime_suspend(struct device *dev)
{
	struct rk_i2s_tdm_dev *i2s_tdm = dev_get_drvdata(dev);

	if (i2s_tdm->pcm_comp && i2s_tdm->clk_trcm) {
		rockchip_i2s_tdm_dma_ctrl(i2s_tdm, 0, 0);
		rockchip_i2s_tdm_dma_ctrl(i2s_tdm, 1, 0);
		rockchip_trcm_dma_guard_ctrl(i2s_tdm, 0, 0);
		rockchip_trcm_dma_guard_ctrl(i2s_tdm, 1, 0);
	}

	regcache_cache_only(i2s_tdm->regmap, true);
	i2s_tdm_disable_unprepare_mclk(i2s_tdm);

	clk_disable_unprepare(i2s_tdm->hclk);

	pinctrl_pm_select_idle_state(dev);

	return 0;
}

static int rockchip_i2s_tdm_pinctrl_select_clk_state(struct device *dev)
{
	struct rk_i2s_tdm_dev *i2s_tdm = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(i2s_tdm->pinctrl) || !i2s_tdm->clk_state)
		return 0;

	pinctrl_select_state(i2s_tdm->pinctrl, i2s_tdm->clk_state);

	return 0;
}

static int __maybe_unused i2s_tdm_runtime_resume(struct device *dev)
{
	struct rk_i2s_tdm_dev *i2s_tdm = dev_get_drvdata(dev);
	int ret;

	/*
	 * pinctrl default state is invoked by ASoC framework, so,
	 * we just handle clk state here if DT assigned.
	 */
	if (i2s_tdm->is_master_mode)
		rockchip_i2s_tdm_pinctrl_select_clk_state(dev);

	ret = clk_prepare_enable(i2s_tdm->hclk);
	if (ret)
		goto err_hclk;

	ret = i2s_tdm_prepare_enable_mclk(i2s_tdm);
	if (ret)
		goto err_mclk;

	regcache_cache_only(i2s_tdm->regmap, false);
	regcache_mark_dirty(i2s_tdm->regmap);

	/*
	 * XFER must be placed after all registers sync done,
	 * because a lots of registers depends on the XFER-Disabled.
	 */
	ret = 0;
	ret |= regcache_sync_region(i2s_tdm->regmap, I2S_TXCR, I2S_INTCR);
	ret |= regcache_sync_region(i2s_tdm->regmap, I2S_TXDR, I2S_CLKDIV);
	ret |= regcache_sync_region(i2s_tdm->regmap, I2S_XFER, I2S_XFER);
	if (ret) {
		dev_err(i2s_tdm->dev, "Failed to sync registers\n");
		goto err_regcache;
	}

	/*
	 * should be placed after regcache sync done to back
	 * to the slave mode and then enable clk state.
	 */
	if (!i2s_tdm->is_master_mode)
		rockchip_i2s_tdm_pinctrl_select_clk_state(dev);

	return 0;

err_regcache:
	i2s_tdm_disable_unprepare_mclk(i2s_tdm);
err_mclk:
	clk_disable_unprepare(i2s_tdm->hclk);
err_hclk:
	return ret;
}

static void __maybe_unused rockchip_i2s_tdm_unmap(struct rk_i2s_tdm_dev *i2s_tdm)
{
#ifdef HAVE_SYNC_RESET
	if (i2s_tdm->cru_base)
		iounmap(i2s_tdm->cru_base);
#endif

#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	if (i2s_tdm->clk_src_base)
		iounmap(i2s_tdm->clk_src_base);
#endif
}

static int rockchip_i2s_tdm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	const struct of_device_id *of_id;
	struct rk_i2s_tdm_dev *i2s_tdm;
	struct resource *res;
	void __iomem *regs;
#ifdef HAVE_SYNC_RESET
	bool sync;
#endif
	int ret, val, i, irq;

	i2s_tdm = devm_kzalloc(&pdev->dev, sizeof(*i2s_tdm), GFP_KERNEL);
	if (!i2s_tdm)
		return -ENOMEM;

	i2s_tdm->dev = &pdev->dev;
	i2s_tdm->lrck_ratio = 1;

	if (!device_property_read_u32(i2s_tdm->dev, "rockchip,resume-deferred-ms", &val))
		i2s_tdm->resume_deferred_ms = val;

	/*
	 * Should use flag GPIOD_ASIS not to reclaim LRCK pin as GPIO function,
	 * because we use the same PIN and just read EXT_PORT value which show
	 * the pin status.
	 */
	i2s_tdm->i2s_lrck_gpio = devm_gpiod_get_optional(i2s_tdm->dev, "i2s-lrck",
							 GPIOD_ASIS);
	if (IS_ERR(i2s_tdm->i2s_lrck_gpio)) {
		ret = PTR_ERR(i2s_tdm->i2s_lrck_gpio);
		dev_err(i2s_tdm->dev, "Failed to get i2s_lrck_gpio %d\n", ret);
		return ret;
	}

	of_id = of_match_device(rockchip_i2s_tdm_match, &pdev->dev);
	if (!of_id)
		return -EINVAL;

	spin_lock_init(&i2s_tdm->lock);
	i2s_tdm->soc_data = (struct rk_i2s_soc_data *)of_id->data;

	for (i = 0; i < ARRAY_SIZE(of_quirks); i++)
		if (of_property_read_bool(node, of_quirks[i].quirk))
			i2s_tdm->quirks |= of_quirks[i].id;

	i2s_tdm->frame_width = 64;

	i2s_tdm->clk_trcm = TRCM_TXRX;
	if (!of_property_read_u32(node, "rockchip,clk-trcm", &val)) {
		if (val >= 0 && val <= 2)
			i2s_tdm->clk_trcm = val;
		dev_warn(i2s_tdm->dev, "Deprecated property 'rockchip,clk-trcm', please use 'rockchip,trcm-sync-tx/rx-only' in DT\n");
	}
	if (of_property_read_bool(node, "rockchip,trcm-sync-tx-only"))
		i2s_tdm->clk_trcm = TRCM_TX;
	if (of_property_read_bool(node, "rockchip,trcm-sync-rx-only")) {
		if (i2s_tdm->clk_trcm) {
			dev_err(i2s_tdm->dev, "invalid trcm-sync configuration\n");
			return -EINVAL;
		}
		i2s_tdm->clk_trcm = TRCM_RX;
	}

	rockchip_i2s_tdm_wait_time_init(i2s_tdm);

	ret = rockchip_i2s_tdm_init_dai(i2s_tdm);
	if (ret)
		return ret;

	i2s_tdm->grf = syscon_regmap_lookup_by_phandle(node, "rockchip,grf");

	i2s_tdm->tx_reset = devm_reset_control_get_optional_exclusive(&pdev->dev,
								      "tx-m");
	if (IS_ERR(i2s_tdm->tx_reset)) {
		ret = PTR_ERR(i2s_tdm->tx_reset);
		return dev_err_probe(i2s_tdm->dev, ret,
				     "Error in tx-m reset control\n");
	}

	i2s_tdm->rx_reset = devm_reset_control_get_optional_exclusive(&pdev->dev,
								      "rx-m");
	if (IS_ERR(i2s_tdm->rx_reset)) {
		ret = PTR_ERR(i2s_tdm->rx_reset);
		return dev_err_probe(i2s_tdm->dev, ret,
				     "Error in rx-m reset control\n");
	}

	i2s_tdm->hclk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(i2s_tdm->hclk)) {
		return dev_err_probe(i2s_tdm->dev, PTR_ERR(i2s_tdm->hclk),
				     "Failed to get clock hclk\n");
	}

	i2s_tdm->mclk_tx = devm_clk_get(&pdev->dev, "mclk_tx");
	if (IS_ERR(i2s_tdm->mclk_tx)) {
		return dev_err_probe(i2s_tdm->dev, PTR_ERR(i2s_tdm->mclk_tx),
				     "Failed to get clock mclk_tx\n");
	}

	i2s_tdm->mclk_rx = devm_clk_get(&pdev->dev, "mclk_rx");
	if (IS_ERR(i2s_tdm->mclk_rx)) {
		return dev_err_probe(i2s_tdm->dev, PTR_ERR(i2s_tdm->mclk_rx),
				     "Failed to get clock mclk_rx\n");
	}

	i2s_tdm->tdm_fsync_half_frame =
		of_property_read_bool(node, "rockchip,tdm-fsync-half-frame");

	i2s_tdm->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR_OR_NULL(i2s_tdm->pinctrl)) {
		i2s_tdm->clk_state = pinctrl_lookup_state(i2s_tdm->pinctrl, "clk");
		if (IS_ERR(i2s_tdm->clk_state)) {
			i2s_tdm->clk_state = NULL;
			dev_dbg(i2s_tdm->dev, "Have no clk pinctrl state\n");
		}
	}

	i2s_tdm->io_multiplex =
		of_property_read_bool(node, "rockchip,io-multiplex");

	ret = rockchip_i2s_tdm_get_calibrate_mclks(i2s_tdm);
	if (ret)
		return dev_err_probe(i2s_tdm->dev, ret,
				     "mclk-calibrate clocks missing");

	regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(regs)) {
		return dev_err_probe(i2s_tdm->dev, PTR_ERR(regs),
				     "Failed to get resource IORESOURCE_MEM\n");
	}

	i2s_tdm->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
						&rockchip_i2s_tdm_regmap_config);
	if (IS_ERR(i2s_tdm->regmap)) {
		return dev_err_probe(i2s_tdm->dev, PTR_ERR(i2s_tdm->regmap),
				     "Failed to initialise regmap\n");
	}

	if (i2s_tdm->has_playback) {
		i2s_tdm->playback_dma_data.addr = res->start + I2S_TXDR;
		i2s_tdm->playback_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		i2s_tdm->playback_dma_data.maxburst = MAXBURST_PER_FIFO;
	}

	if (i2s_tdm->has_capture) {
		i2s_tdm->capture_dma_data.addr = res->start + I2S_RXDR;
		i2s_tdm->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		i2s_tdm->capture_dma_data.maxburst = MAXBURST_PER_FIFO;
	}

	irq = platform_get_irq_optional(pdev, 0);
	if (irq > 0) {
		ret = devm_request_irq(&pdev->dev, irq, rockchip_i2s_tdm_isr,
				       IRQF_SHARED, node->name, i2s_tdm);
		if (ret) {
			dev_err(&pdev->dev, "failed to request irq %u\n", irq);
			return ret;
		}
	}

	ret = clk_prepare_enable(i2s_tdm->hclk);
	if (ret) {
		return dev_err_probe(i2s_tdm->dev, ret,
				     "Failed to enable clock hclk\n");
	}

	ret = rockchip_i2s_tdm_tx_path_prepare(i2s_tdm, node);
	if (ret < 0) {
		dev_err(&pdev->dev, "I2S TX path prepare failed: %d\n", ret);
		goto err_disable_hclk;
	}

	ret = rockchip_i2s_tdm_rx_path_prepare(i2s_tdm, node);
	if (ret < 0) {
		dev_err(&pdev->dev, "I2S RX path prepare failed: %d\n", ret);
		goto err_disable_hclk;
	}

	dev_set_drvdata(&pdev->dev, i2s_tdm);

	if (i2s_tdm->mclk_calibrate) {
		i2s_tdm->mclk_root0_initial_freq = clk_get_rate(i2s_tdm->mclk_root0);
		i2s_tdm->mclk_root1_initial_freq = clk_get_rate(i2s_tdm->mclk_root1);
		i2s_tdm->mclk_root0_freq = i2s_tdm->mclk_root0_initial_freq;
		i2s_tdm->mclk_root1_freq = i2s_tdm->mclk_root1_initial_freq;
	}

	regmap_update_bits(i2s_tdm->regmap, I2S_DMACR, I2S_DMACR_TDL_MASK,
			   I2S_DMACR_TDL(16));
	regmap_update_bits(i2s_tdm->regmap, I2S_DMACR, I2S_DMACR_RDL_MASK,
			   I2S_DMACR_RDL(16));
	regmap_update_bits(i2s_tdm->regmap, I2S_CKR, I2S_CKR_TRCM_MASK,
			   i2s_tdm->clk_trcm << I2S_CKR_TRCM_SHIFT);

	if (i2s_tdm->soc_data && i2s_tdm->soc_data->init)
		i2s_tdm->soc_data->init(&pdev->dev, res->start);

	/*
	 * CLK_ALWAYS_ON should be placed after all registers write done,
	 * because this situation will enable XFER bit which will make
	 * some registers(depend on XFER) write failed.
	 */
	if (i2s_tdm->quirks & QUIRK_ALWAYS_ON) {
		ret = rockchip_i2s_tdm_keep_clk_always_on(i2s_tdm);
		if (ret)
			goto err_disable_hclk;
	}

#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	ret = rockchip_i2s_tdm_multi_lanes_parse(i2s_tdm);
	if (ret)
		goto err_unmap;
#endif

#ifdef HAVE_SYNC_RESET
	sync = of_device_is_compatible(node, "rockchip,px30-i2s-tdm") ||
	       of_device_is_compatible(node, "rockchip,rk1808-i2s-tdm") ||
	       of_device_is_compatible(node, "rockchip,rk3308-i2s-tdm");

	if (i2s_tdm->clk_trcm && sync) {
		struct device_node *cru_node;

		cru_node = of_parse_phandle(node, "rockchip,cru", 0);
		i2s_tdm->cru_base = of_iomap(cru_node, 0);
		if (!i2s_tdm->cru_base) {
			ret = -ENOENT;
			goto err_unmap;
		}

		i2s_tdm->id = (res->start >> 16) & GENMASK(3, 0);
	}
#endif

	/*
	 * MUST: after pm_runtime_enable step, any register R/W
	 * should be wrapped with pm_runtime_get_sync/put.
	 *
	 * Another approach is to enable the regcache true to
	 * avoid access HW registers.
	 *
	 * Alternatively, performing the registers R/W before
	 * pm_runtime_enable is also a good option.
	 */
	pm_runtime_enable(&pdev->dev);

	/*
	 * Should be placed after pm_runtime_enable to do
	 * rpm_resume at the moment. otherwise, it will make sense
	 * at the next pm_runtime_get.
	 */
	if (i2s_tdm->quirks & QUIRK_ALWAYS_ON)
		pm_runtime_forbid(i2s_tdm->dev);

	ret = rockchip_i2s_tdm_register_platform(&pdev->dev);
	if (ret)
		goto err_suspend;

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &rockchip_i2s_tdm_component,
					      i2s_tdm->dai, 1);

	if (ret) {
		dev_err(&pdev->dev, "Could not register DAI\n");
		goto err_suspend;
	}

	return 0;

err_suspend:
#ifdef CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES
	if (i2s_tdm->clk_src_base)
		iounmap(i2s_tdm->clk_src_base);
#endif
	if (!pm_runtime_status_suspended(&pdev->dev))
		i2s_tdm_runtime_suspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

#if defined(HAVE_SYNC_RESET) || defined(CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES)
err_unmap:
	rockchip_i2s_tdm_unmap(i2s_tdm);
#endif

err_disable_hclk:
	clk_disable_unprepare(i2s_tdm->hclk);

	return ret;
}

static int rockchip_i2s_tdm_remove(struct platform_device *pdev)
{
#if defined(HAVE_SYNC_RESET) || defined(CONFIG_SND_SOC_ROCKCHIP_I2S_TDM_MULTI_LANES)
	rockchip_i2s_tdm_unmap(dev_get_drvdata(&pdev->dev));
#endif
	if (!pm_runtime_status_suspended(&pdev->dev))
		i2s_tdm_runtime_suspend(&pdev->dev);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static void rockchip_i2s_tdm_platform_shutdown(struct platform_device *pdev)
{
	struct rk_i2s_tdm_dev *i2s_tdm = dev_get_drvdata(&pdev->dev);

	pm_runtime_get_sync(i2s_tdm->dev);
	rockchip_i2s_tdm_stop(i2s_tdm, SNDRV_PCM_STREAM_PLAYBACK);
	rockchip_i2s_tdm_stop(i2s_tdm, SNDRV_PCM_STREAM_CAPTURE);
	pm_runtime_put(i2s_tdm->dev);
}

static const struct dev_pm_ops rockchip_i2s_tdm_pm_ops = {
	SET_RUNTIME_PM_OPS(i2s_tdm_runtime_suspend, i2s_tdm_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
};

static struct platform_driver rockchip_i2s_tdm_driver = {
	.probe = rockchip_i2s_tdm_probe,
	.remove = rockchip_i2s_tdm_remove,
	.shutdown = rockchip_i2s_tdm_platform_shutdown,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(rockchip_i2s_tdm_match),
		.pm = &rockchip_i2s_tdm_pm_ops,
	},
};
module_platform_driver(rockchip_i2s_tdm_driver);

MODULE_DESCRIPTION("ROCKCHIP I2S/TDM ASoC Interface");
MODULE_AUTHOR("Sugar Zhang <sugar.zhang@rock-chips.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, rockchip_i2s_tdm_match);
