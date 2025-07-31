// SPDX-License-Identifier: GPL-2.0
/*
 * ALSA SoC Audio Layer - Rockchip SPDIF_RX Controller driver
 *
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co., Ltd
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <sound/asoundef.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>

#include "rockchip_spdifrx.h"

#define QUIRK_ALWAYS_ON		BIT(0)

struct rk_spdifrx_info {
	int sync;
	int debounce_time_ms;
	int liner_pcm;
	int liner_pcm_last;
	unsigned int sample_rate_src;
	unsigned int sample_rate_cal;
	unsigned int sample_rate_src_last;
	unsigned int sample_rate_cal_last;
	unsigned int sample_width;		/* valid width */
	unsigned int sample_width_last;
};

struct rk_spdifrx_dev {
	struct device *dev;
	struct clk *mclk;
	struct clk *hclk;
	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct regmap *regmap;
	struct reset_control *reset;
	struct rk_spdifrx_info info;
	struct snd_soc_dai *dai;
	struct snd_pcm_substream *substream;
	struct timer_list debounce_timer;
	struct timer_list non_liner_timer;
	struct timer_list fifo_timer;
	struct work_struct xrun_work;
	unsigned int mclk_rate;
	int irq;
	bool cdr_count_avg;
	bool need_reset;
};

static const struct spdifrx_of_quirks {
	char *quirk;
	int id;
} of_quirks[] = {
	{
		.quirk = "rockchip,always-on",
		.id = QUIRK_ALWAYS_ON,
	},
};

static int rk_spdifrx_runtime_suspend(struct device *dev)
{
	struct rk_spdifrx_dev *spdifrx = dev_get_drvdata(dev);

	clk_disable_unprepare(spdifrx->mclk);
	clk_disable_unprepare(spdifrx->hclk);

	return 0;
}

static int rk_spdifrx_runtime_resume(struct device *dev)
{
	struct rk_spdifrx_dev *spdifrx = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(spdifrx->mclk);
	if (ret) {
		dev_err(spdifrx->dev, "mclk clock enable failed %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(spdifrx->hclk);
	if (ret) {
		dev_err(spdifrx->dev, "hclk clock enable failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int rk_spdifrx_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
			   SPDIFRX_INTEN_SYNCIE_MASK |
			   SPDIFRX_INTEN_NSYNCIE_MASK |
			   SPDIFRX_INTEN_BTEIE_MASK |
			   SPDIFRX_INTEN_NPSPIE_MASK |
			   SPDIFRX_INTEN_BMDEIE_MASK |
			   SPDIFRX_INTEN_PEIE_MASK |
			   SPDIFRX_INTEN_CSCIE_MASK |
			   SPDIFRX_INTEN_NVLDIE_MASK,
			   SPDIFRX_INTEN_SYNCIE_EN |
			   SPDIFRX_INTEN_NSYNCIE_EN |
			   SPDIFRX_INTEN_BTEIE_EN |
			   SPDIFRX_INTEN_NPSPIE_EN |
			   SPDIFRX_INTEN_BMDEIE_EN |
			   SPDIFRX_INTEN_PEIE_EN |
			   SPDIFRX_INTEN_CSCIE_EN |
			   SPDIFRX_INTEN_NVLDIE_EN);
	regmap_update_bits(spdifrx->regmap, SPDIFRX_DMACR,
			   SPDIFRX_DMACR_RDL_MASK, SPDIFRX_DMACR_RDL(8));
	regmap_update_bits(spdifrx->regmap, SPDIFRX_CDR,
			   SPDIFRX_CDR_AVGSEL_MASK | SPDIFRX_CDR_BYPASS_MASK,
			   SPDIFRX_CDR_AVGSEL_MIN | SPDIFRX_CDR_BYPASS_EN);

	spdifrx->need_reset = false;
	spdifrx->info.sample_rate_cal_last = 0;
	spdifrx->info.sample_rate_src_last = 0;
	spdifrx->info.sample_width_last = 0;
	spdifrx->info.liner_pcm_last = 1;
	spdifrx->substream = substream;

	if (params_rate(params) >= 44100)
		spdifrx->cdr_count_avg = true;
	else
		spdifrx->cdr_count_avg = false;

	return 0;
}

static void rk_spdifrx_reset(struct rk_spdifrx_dev *spdifrx)
{
	reset_control_assert(spdifrx->reset);
	udelay(1);
	reset_control_deassert(spdifrx->reset);
}

static int rk_spdifrx_trigger(struct snd_pcm_substream *substream,
			      int cmd, struct snd_soc_dai *dai)
{
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);
	int ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		regmap_write(spdifrx->regmap, SPDIFRX_CLR, SPDIFRX_CLR_RXSC);
		ret = regmap_update_bits(spdifrx->regmap, SPDIFRX_DMACR,
					 SPDIFRX_DMACR_RDE_MASK,
					 SPDIFRX_DMACR_RDE_ENABLE);

		if (ret != 0)
			return ret;

		ret = regmap_update_bits(spdifrx->regmap, SPDIFRX_CFGR,
					 SPDIFRX_EN_MASK,
					 SPDIFRX_EN);

		mod_timer(&spdifrx->fifo_timer, jiffies + msecs_to_jiffies(1000));
		dev_dbg(spdifrx->dev, "start fifo timer\n");
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ret = regmap_update_bits(spdifrx->regmap, SPDIFRX_DMACR,
					 SPDIFRX_DMACR_RDE_MASK,
					 SPDIFRX_DMACR_RDE_DISABLE);

		if (ret != 0)
			return ret;

		ret = regmap_update_bits(spdifrx->regmap, SPDIFRX_CFGR,
					 SPDIFRX_EN_MASK,
					 SPDIFRX_DIS);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int rk_spdifrx_parse_quirks(struct rk_spdifrx_dev *spdifrx)
{
	int i = 0;
	unsigned int quirks = 0;

	for (i = 0; i < ARRAY_SIZE(of_quirks); i++)
		if (device_property_read_bool(spdifrx->dev, of_quirks[i].quirk))
			quirks |= of_quirks[i].id;

	if (quirks & QUIRK_ALWAYS_ON) {
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_SYNCIE_MASK |
				   SPDIFRX_INTEN_NSYNCIE_MASK |
				   SPDIFRX_INTEN_BTEIE_MASK |
				   SPDIFRX_INTEN_NPSPIE_MASK |
				   SPDIFRX_INTEN_BMDEIE_MASK |
				   SPDIFRX_INTEN_PEIE_MASK |
				   SPDIFRX_INTEN_CSCIE_MASK |
				   SPDIFRX_INTEN_NVLDIE_MASK,
				   SPDIFRX_INTEN_SYNCIE_EN |
				   SPDIFRX_INTEN_NSYNCIE_EN |
				   SPDIFRX_INTEN_BTEIE_EN |
				   SPDIFRX_INTEN_NPSPIE_EN |
				   SPDIFRX_INTEN_BMDEIE_EN |
				   SPDIFRX_INTEN_PEIE_EN |
				   SPDIFRX_INTEN_CSCIE_EN |
				   SPDIFRX_INTEN_NVLDIE_EN);

		pm_runtime_forbid(spdifrx->dev);
	}

	return 0;
}

static int rk_spdifrx_sync_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	ucontrol->value.integer.value[0] = spdifrx->info.sync;
	return 0;
}

static int rk_spdifrx_sample_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	ucontrol->value.integer.value[0] = spdifrx->info.sample_rate_src;
	ucontrol->value.integer.value[1] = spdifrx->info.sample_rate_cal;
	return 0;
}

static int rk_spdifrx_debounce_time_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	ucontrol->value.integer.value[0] = spdifrx->info.debounce_time_ms;
	return 0;
}

static int rk_spdifrx_debounce_time_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	spdifrx->info.debounce_time_ms = ucontrol->value.integer.value[0];
	return 0;
}

static int rk_spdifrx_sample_width_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	ucontrol->value.integer.value[0] = spdifrx->info.sample_width;
	return 0;
}

static int rk_spdifrx_liner_pcm_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	ucontrol->value.integer.value[0] = spdifrx->info.liner_pcm;
	return 0;
}

static int rk_spdifrx_sync_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int rk_spdifrx_sample_rate_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffffffff;

	return 0;
}

static int rk_spdifrx_debounce_time_info(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1000;

	return 0;
}

static int rk_spdifrx_sample_width_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 64;

	return 0;
}

static int rk_spdifrx_liner_pcm_info(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static struct snd_kcontrol_new rk_spdifrx_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "RK SPDIFRX SYNC STATUS",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = rk_spdifrx_sync_info,
		.get = rk_spdifrx_sync_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "RK SPDIFRX SAMPLE RATE",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = rk_spdifrx_sample_rate_info,
		.get = rk_spdifrx_sample_rate_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "RK SPDIFRX DEBOUNCE TIME",
		.info = rk_spdifrx_debounce_time_info,
		.get = rk_spdifrx_debounce_time_get,
		.put = rk_spdifrx_debounce_time_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "RK SPDIFRX SAMPLE WIDTH",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = rk_spdifrx_sample_width_info,
		.get = rk_spdifrx_sample_width_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "RK SPDIFRX LINER PCM",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = rk_spdifrx_liner_pcm_info,
		.get = rk_spdifrx_liner_pcm_get,
	},
};

static int rk_spdifrx_dai_probe(struct snd_soc_dai *dai)
{
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	dai->capture_dma_data = &spdifrx->capture_dma_data;
	spdifrx->dai = dai;
	snd_soc_add_dai_controls(dai, rk_spdifrx_controls,
				 ARRAY_SIZE(rk_spdifrx_controls));

	rk_spdifrx_parse_quirks(spdifrx);
	spdifrx->need_reset = true;

	return 0;
}

static const struct snd_soc_dai_ops rk_spdifrx_dai_ops = {
	.hw_params = rk_spdifrx_hw_params,
	.trigger = rk_spdifrx_trigger,
};

static struct snd_soc_dai_driver rk_spdifrx_dai = {
	.probe = rk_spdifrx_dai_probe,
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			    SNDRV_PCM_FMTBIT_S20_3LE |
			    SNDRV_PCM_FMTBIT_S24_LE |
			    SNDRV_PCM_FMTBIT_S32_LE),
	},
	.ops = &rk_spdifrx_dai_ops,
};

static const struct snd_soc_component_driver rk_spdifrx_component = {
	.name = "rockchip-spdifrx",
	.legacy_dai_naming = 1,
};

static bool rk_spdifrx_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SPDIFRX_CFGR:
	case SPDIFRX_CLR:
	case SPDIFRX_CDR:
	case SPDIFRX_CDRST:
	case SPDIFRX_DMACR:
	case SPDIFRX_FIFOCTRL:
	case SPDIFRX_INTEN:
	case SPDIFRX_INTMASK:
	case SPDIFRX_INTSR:
	case SPDIFRX_INTCLR:
	case SPDIFRX_SMPDR:
	case SPDIFRX_CHNSR1:
	case SPDIFRX_CHNSR2:
	case SPDIFRX_BURSTINFO:
		return true;
	default:
		return false;
	}
}

static bool rk_spdifrx_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SPDIFRX_CFGR:
	case SPDIFRX_CLR:
	case SPDIFRX_CDR:
	case SPDIFRX_CDRST:
	case SPDIFRX_DMACR:
	case SPDIFRX_FIFOCTRL:
	case SPDIFRX_INTEN:
	case SPDIFRX_INTMASK:
	case SPDIFRX_INTSR:
	case SPDIFRX_INTCLR:
	case SPDIFRX_SMPDR:
	case SPDIFRX_CHNSR1:
	case SPDIFRX_CHNSR2:
	case SPDIFRX_BURSTINFO:
		return true;
	default:
		return false;
	}
}

static bool rk_spdifrx_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SPDIFRX_CLR:
	case SPDIFRX_CDR:
	case SPDIFRX_CDRST:
	case SPDIFRX_FIFOCTRL:
	case SPDIFRX_INTSR:
	case SPDIFRX_INTCLR:
	case SPDIFRX_SMPDR:
	case SPDIFRX_CHNSR1:
	case SPDIFRX_CHNSR2:
	case SPDIFRX_BURSTINFO:
		return true;
	default:
		return false;
	}
}

static bool rk_spdifrx_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SPDIFRX_SMPDR:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config rk_spdifrx_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = SPDIFRX_BURSTINFO,
	.writeable_reg = rk_spdifrx_wr_reg,
	.readable_reg = rk_spdifrx_rd_reg,
	.volatile_reg = rk_spdifrx_volatile_reg,
	.precious_reg = rk_spdifrx_precious_reg,
	.cache_type = REGCACHE_FLAT,
};

static unsigned int rk_spdifrx_get_sample_width(unsigned int flag)
{
	unsigned int width = 0;

	switch (flag) {
	case IEC958_AES4_CON_WORDLEN_20_16:
		width = 16;
		break;
	case IEC958_AES4_CON_WORDLEN_22_18:
		width = 18;
		break;
	case IEC958_AES4_CON_WORDLEN_20_16 | IEC958_AES4_CON_MAX_WORDLEN_24:
		width = 20;
		break;
	case IEC958_AES4_CON_WORDLEN_24_20 | IEC958_AES4_CON_MAX_WORDLEN_24:
		width = 24;
		break;
	default:
		return 0;
	}

	return width;
}

static unsigned int rk_spdifrx_get_sample_rate(unsigned int flag)
{
	unsigned int rate = 0;

	switch (flag) {
	case IEC958_AES3_CON_FS_22050:
		rate = 22050;
		break;
	case IEC958_AES3_CON_FS_24000:
		rate = 24000;
		break;
	case IEC958_AES3_CON_FS_32000:
		rate = 32000;
		break;
	case IEC958_AES3_CON_FS_44100:
		rate = 44100;
		break;
	case IEC958_AES3_CON_FS_48000:
		rate = 48000;
		break;
	case IEC958_AES3_CON_FS_88200:
		rate = 88200;
		break;
	case IEC958_AES3_CON_FS_96000:
		rate = 96000;
		break;
	case IEC958_AES3_CON_FS_176400:
		rate = 176400;
		break;
	case IEC958_AES3_CON_FS_192000:
		rate = 192000;
		break;
	case IEC958_AES3_CON_FS_768000:
		rate = 768000;
		break;
	default:
		return 0;
	}

	return rate;
}

static unsigned int rk_spdifrx_convert_sample_rate(unsigned int mclk, unsigned int count)
{
	unsigned int rate;

	rate = mclk / (count * 128);

	if (rate >= (8000 + 16000) / 2 && rate < (16000 + 22050) / 2)
		return 16000;

	if (rate >= (16000 + 22050) / 2 && rate < (22050 + 24000) / 2)
		return 22050;

	if (rate >= (22050 + 24000) / 2 && rate < (24000 + 32000) / 2)
		return 24000;

	if (rate >= (24000 + 32000) / 2 && rate < (32000 + 44100) / 2)
		return 32000;

	if (rate >= (32000 + 44100) / 2 && rate < (44100 + 48000) / 2)
		return 44100;

	if (rate >= (44100 + 48000) / 2 && rate < (48000 + 88200) / 2)
		return 48000;

	if (rate >= (48000 + 88200) / 2 && rate < (88200 + 96000) / 2)
		return 88200;

	if (rate >= (88200 + 96000) / 2 && rate < (96000 + 192000) / 2)
		return 96000;

	if (rate >= (96000 + 176400) / 2 && rate < (176400 + 192000) / 2)
		return 176400;

	if (rate >= (176400 + 192000) / 2 && rate < (192000 + 384000) / 2)
		return 192000;

	if (rate >= (192000 + 384000) / 2 && rate < (384000 + 768000) / 2)
		return 384000;

	if (rate >= (384000 + 768000) / 2)
		return 768000;

	return 0;
}

static int rk_spdifrx_disable_dma(struct rk_spdifrx_dev *spdifrx)
{
	int ret;

	ret = regmap_update_bits(spdifrx->regmap, SPDIFRX_DMACR,
				 SPDIFRX_DMACR_RDE_MASK,
				 SPDIFRX_DMACR_RDE_DISABLE);

	if (ret != 0) {
		dev_err(spdifrx->dev, "Failed to disable rxdma\n");
		return ret;
	}

	dev_dbg(spdifrx->dev, "rxdma disabled\n");

	return ret;
}

static irqreturn_t rk_spdifrx_isr(int irq, void *dev_id)
{
	struct rk_spdifrx_dev *spdifrx = dev_id;
	struct snd_soc_dai *dai = spdifrx->dai;
	struct snd_kcontrol *sample_kctl = snd_soc_card_get_kcontrol(dai->component->card,
								     "RK SPDIFRX SAMPLE RATE");
	struct snd_kcontrol *width_kctl = snd_soc_card_get_kcontrol(dai->component->card,
								    "RK SPDIFRX SAMPLE WIDTH");
	struct snd_kcontrol *liner_pcm_kctl = snd_soc_card_get_kcontrol(dai->component->card,
									"RK SPDIFRX LINER PCM");
	u32 intsr;
	u32 val;
	u32 count;

	if (pm_runtime_resume_and_get(spdifrx->dev) < 0)
		return IRQ_NONE;

	regmap_read(spdifrx->regmap, SPDIFRX_INTSR, &intsr);

	if (intsr & SPDIFRX_INTSR_NVLDISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "No Valid Error\n");
		regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, SPDIFRX_INTCLR_NVLDICLR);
		rk_spdifrx_reset(spdifrx);
		spdifrx->need_reset = true;
		rk_spdifrx_disable_dma(spdifrx);
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_NVLDIE_MASK, SPDIFRX_INTEN_NVLDIE_DIS);
	}

	if (intsr & SPDIFRX_INTSR_CSCISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "CSC Changed\n");
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_BTEIE_MASK, SPDIFRX_INTEN_BTEIE_EN);
		regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, SPDIFRX_INTCLR_CSCICLR);
	}

	if (intsr & SPDIFRX_INTSR_PEISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "Parity Error\n");
		regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, SPDIFRX_INTCLR_PEICLR);
		rk_spdifrx_reset(spdifrx);
		spdifrx->need_reset = true;
		rk_spdifrx_disable_dma(spdifrx);
	}

	if (intsr & SPDIFRX_INTSR_NPSPISR_ACTIVE) {
		spdifrx->info.liner_pcm = 0;
		if (spdifrx->info.liner_pcm != spdifrx->info.liner_pcm_last) {
			snd_ctl_notify(dai->component->card->snd_card,
				       SNDRV_CTL_EVENT_MASK_VALUE, &liner_pcm_kctl->id);
			spdifrx->info.liner_pcm_last = spdifrx->info.liner_pcm;
			dev_dbg(spdifrx->dev, "non liner data\n");
		}
		mod_timer(&spdifrx->non_liner_timer, jiffies + msecs_to_jiffies(100));
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_NVLDIE_MASK, SPDIFRX_INTEN_NVLDIE_DIS);
		regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, SPDIFRX_INTCLR_NPSPICLR);
	}

	if (intsr & SPDIFRX_INTSR_BMDEISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "BMD Error\n");
		regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, SPDIFRX_INTCLR_BMDEICLR);
		rk_spdifrx_reset(spdifrx);
		spdifrx->need_reset = true;
		rk_spdifrx_disable_dma(spdifrx);
	}

	if (intsr & SPDIFRX_INTSR_NSYNCISR_ACTIVE) {
		spdifrx->info.sync = 0;
		spdifrx->need_reset = true;
		mod_timer(&spdifrx->debounce_timer, jiffies +
			  msecs_to_jiffies(spdifrx->info.debounce_time_ms));
		dev_dbg(spdifrx->dev, "NSYNC\n");
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN, SPDIFRX_INTEN_NSYNCIE_MASK,
				   SPDIFRX_INTEN_NSYNCIE_DIS);
		regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, SPDIFRX_INTCLR_NSYNCICLR);
		regmap_write(spdifrx->regmap, SPDIFRX_CLR, SPDIFRX_CLR_RXSC);
	}

	if (intsr & SPDIFRX_INTSR_BTEISR_ACTIVE) {
		regmap_read(spdifrx->regmap, SPDIFRX_CHNSR1, &val);
		spdifrx->info.sample_rate_src =
			rk_spdifrx_get_sample_rate((val & SPDIFRX_CHNSR1_SAMPLE_RATE_MASK) >> 8);

		regmap_read(spdifrx->regmap, SPDIFRX_CDRST, &val);
		if (spdifrx->cdr_count_avg)
			count = ((val & SPDIFRX_CDRST_MINCNT_MASK) + 1 +
				((val & SPDIFRX_CDRST_MAXCNT_MASK) >> 8) + 1) / 4;
		else
			count = (val & SPDIFRX_CDRST_MINCNT_MASK) + 1;

		if (count > 0)
			spdifrx->info.sample_rate_cal =
				rk_spdifrx_convert_sample_rate(spdifrx->mclk_rate, count);
		else
			spdifrx->info.sample_rate_cal = 0;

		regmap_read(spdifrx->regmap, SPDIFRX_CHNSR2, &val);
		spdifrx->info.sample_width =
			rk_spdifrx_get_sample_width(val & SPDIFRX_CHNSR2_SAMPLE_WIDTH_MASK);

		if (spdifrx->info.sample_rate_src != spdifrx->info.sample_rate_src_last ||
		    spdifrx->info.sample_rate_cal != spdifrx->info.sample_rate_cal_last) {
			snd_ctl_notify(dai->component->card->snd_card,
				       SNDRV_CTL_EVENT_MASK_VALUE, &sample_kctl->id);
			spdifrx->info.sample_rate_src_last = spdifrx->info.sample_rate_src;
			spdifrx->info.sample_rate_cal_last = spdifrx->info.sample_rate_cal;
			dev_dbg(spdifrx->dev, "src sample rate: %u Hz\n",
				spdifrx->info.sample_rate_src);
			dev_dbg(spdifrx->dev, "cal sample rate: %u Hz\n",
				spdifrx->info.sample_rate_cal);
		}

		if (spdifrx->info.sample_width != spdifrx->info.sample_width_last) {
			snd_ctl_notify(dai->component->card->snd_card,
				       SNDRV_CTL_EVENT_MASK_VALUE, &width_kctl->id);
			spdifrx->info.sample_width_last = spdifrx->info.sample_width;
			dev_dbg(spdifrx->dev, "sample width: %u bit\n", spdifrx->info.sample_width);
		}

		dev_dbg(spdifrx->dev, "BTEIE\n");

		regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, SPDIFRX_INTCLR_BTECLR);
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_BTEIE_MASK, SPDIFRX_INTEN_BTEIE_DIS);
	}

	if (intsr & SPDIFRX_INTSR_SYNCISR_ACTIVE) {
		spdifrx->info.sync = 1;
		mod_timer(&spdifrx->debounce_timer, jiffies +
			  msecs_to_jiffies(spdifrx->info.debounce_time_ms));
		regmap_read(spdifrx->regmap, SPDIFRX_CDRST, &val);
		dev_dbg(spdifrx->dev, "MINCNT = %lu, MAXCNT = %lu\n",
			val & SPDIFRX_CDRST_MINCNT_MASK, (val & SPDIFRX_CDRST_MAXCNT_MASK) >> 8);
		dev_dbg(spdifrx->dev, "SYNC\n");
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_BTEIE_MASK | SPDIFRX_INTEN_NSYNCIE_MASK,
				   SPDIFRX_INTEN_BTEIE_EN | SPDIFRX_INTEN_NSYNCIE_EN);
		regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, SPDIFRX_INTCLR_SYNCICLR);
	}

	pm_runtime_put(spdifrx->dev);

	return IRQ_HANDLED;
}

/*
 * Check if data has been received into the rxfifo.
 *
 * Within the timeout_us interval, poll the SPDIFRX_FIFOCTRL
 * register via regmap_read_poll_timeout_atomic to detect new
 * data entries in the fifo
 *
 * The timeout_us parameter corresponds to the time required
 * for a 32-bit data entry to be written into the fifo, which
 * is inversely proportional to the sample rate, with a maximum
 * value of 32μs (at 16kHz sample rate).
 *
 * If the actual polling duration exceeds the timeout_us by a
 * predefined threshold, the current detection result is invalidated,
 * and the system proceeds to the next scheduled timer interval.
 */
static void rk_spdifrx_fifo_timer_isr(struct timer_list *timer)
{
	struct rk_spdifrx_dev *spdifrx = from_timer(spdifrx, timer, fifo_timer);
	unsigned int val, timeout_us;
	unsigned int fifo_cnt;
	ktime_t start, end;
	int ret;

	if (spdifrx->info.sync == 0 || spdifrx->need_reset || spdifrx->info.sample_rate_src == 0) {
		dev_dbg(spdifrx->dev, "exit fifo timer\n");
		dev_dbg(spdifrx->dev, "sync: %d, need_reset: %d, sample_rate_src: %u\n",
			spdifrx->info.sync, spdifrx->need_reset, spdifrx->info.sample_rate_src);
		return;
	}

	regmap_read(spdifrx->regmap, SPDIFRX_DMACR, &val);
	if ((val & SPDIFRX_DMACR_RDE_MASK) == 0) {
		dev_dbg(spdifrx->dev, "exit fifo timer: rxdma disabled\n");
		return;
	}

	timeout_us = DIV_ROUND_UP(500000, spdifrx->info.sample_rate_src);

	start = ktime_get();
	regmap_read(spdifrx->regmap, SPDIFRX_FIFOCTRL, &val);
	fifo_cnt = (val & SPDIFRX_FIFOCTRL_RFL_MASK) >> 8;

	if (fifo_cnt < 8) {
		ret = regmap_read_poll_timeout_atomic(spdifrx->regmap, SPDIFRX_FIFOCTRL, val,
						      ((val & SPDIFRX_FIFOCTRL_RFL_MASK) >> 8) !=
						      fifo_cnt, 1, timeout_us);
		end = ktime_get();
		if (ret == -ETIMEDOUT && ktime_us_delta(end, start) < 8 * timeout_us) {
			dev_info(spdifrx->dev, "no data to fifo, reset\n");
			rk_spdifrx_reset(spdifrx);
			spdifrx->need_reset = true;
			rk_spdifrx_disable_dma(spdifrx);
			return;
		}
	}
	mod_timer(&spdifrx->fifo_timer, jiffies + msecs_to_jiffies(100));
}

static void rk_spdifrx_non_liner_timer_isr(struct timer_list *timer)
{
	struct rk_spdifrx_dev *spdifrx = from_timer(spdifrx, timer, non_liner_timer);
	struct snd_soc_dai *dai = spdifrx->dai;
	struct snd_kcontrol *liner_pcm_kctl = snd_soc_card_get_kcontrol(dai->component->card,
									"RK SPDIFRX LINER PCM");

	spdifrx->info.liner_pcm = 1;
	snd_ctl_notify(dai->component->card->snd_card,
		       SNDRV_CTL_EVENT_MASK_VALUE, &liner_pcm_kctl->id);
	spdifrx->info.liner_pcm_last = spdifrx->info.liner_pcm;
	regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
			   SPDIFRX_INTEN_NVLDIE_MASK, SPDIFRX_INTEN_NVLDIE_EN);
	dev_dbg(spdifrx->dev, "liner data\n");
}

static void rk_spdifrx_debounce_timer_isr(struct timer_list *timer)
{
	struct rk_spdifrx_dev *spdifrx = from_timer(spdifrx, timer, debounce_timer);
	struct snd_soc_dai *dai = spdifrx->dai;
	struct snd_kcontrol *sync_kctl = snd_soc_card_get_kcontrol(dai->component->card,
								   "RK SPDIFRX SYNC STATUS");
	struct snd_kcontrol *sample_kctl = snd_soc_card_get_kcontrol(dai->component->card,
								     "RK SPDIFRX SAMPLE RATE");
	u32 val;
	u32 count;

	if (spdifrx->info.sync == 1) {
		if (spdifrx->need_reset) {
			rk_spdifrx_reset(spdifrx);
			spdifrx->need_reset = false;
			schedule_work(&spdifrx->xrun_work);
		} else {
			regmap_read(spdifrx->regmap, SPDIFRX_CDRST, &val);
			if (spdifrx->cdr_count_avg)
				count = ((val & SPDIFRX_CDRST_MINCNT_MASK) + 1 +
					((val & SPDIFRX_CDRST_MAXCNT_MASK) >> 8) + 1) / 4;
			else
				count = (val & SPDIFRX_CDRST_MINCNT_MASK) + 1;

			if (count > 0)
				spdifrx->info.sample_rate_cal =
					rk_spdifrx_convert_sample_rate(spdifrx->mclk_rate, count);
			else
				spdifrx->info.sample_rate_cal = 0;

			snd_ctl_notify(dai->component->card->snd_card,
				       SNDRV_CTL_EVENT_MASK_VALUE, &sample_kctl->id);
			snd_ctl_notify(dai->component->card->snd_card,
				       SNDRV_CTL_EVENT_MASK_VALUE, &sync_kctl->id);
			spdifrx->info.sample_rate_cal_last = spdifrx->info.sample_rate_cal;
			if (spdifrx->info.liner_pcm == 1)
				regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
						   SPDIFRX_INTEN_NVLDIE_MASK,
						   SPDIFRX_INTEN_NVLDIE_EN);
			dev_dbg(spdifrx->dev, "notify sync and sample_rate_cal = %u hz\n",
				spdifrx->info.sample_rate_cal);
		}
	} else if (spdifrx->info.sync == 0) {
		snd_ctl_notify(dai->component->card->snd_card,
			       SNDRV_CTL_EVENT_MASK_VALUE, &sync_kctl->id);
		dev_dbg(spdifrx->dev, "notify usync\n");
	}
}

static void rk_spdifrx_xrun_work(struct work_struct *work)
{
	struct rk_spdifrx_dev *spdifrx = container_of(work, struct rk_spdifrx_dev, xrun_work);
	int ret;
	u32 val;

	ret = regmap_read_poll_timeout(spdifrx->regmap, SPDIFRX_CDR, val,
				       ((val & SPDIFRX_CDR_CS_MASK) >> 9) == 0x3, 300, 3000);
	if (!ret) {
		if (spdifrx->substream) {
			snd_pcm_stop_xrun(spdifrx->substream);
			dev_dbg(spdifrx->dev, "stop xrun\n");
		}
	} else {
		dev_dbg(spdifrx->dev, "reset enter sync failed\n");
	}
}

static int rk_spdifrx_probe(struct platform_device *pdev)
{
	struct rk_spdifrx_dev *spdifrx;
	struct resource *res;
	void __iomem *regs;
	int ret;

	spdifrx = devm_kzalloc(&pdev->dev, sizeof(*spdifrx), GFP_KERNEL);
	if (!spdifrx)
		return -ENOMEM;

	spdifrx->reset = devm_reset_control_get(&pdev->dev, "spdifrx-m");
	if (IS_ERR(spdifrx->reset)) {
		ret = PTR_ERR(spdifrx->reset);
		if (ret != -ENOENT)
			return ret;
	}

	spdifrx->hclk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(spdifrx->hclk))
		return PTR_ERR(spdifrx->hclk);

	spdifrx->mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(spdifrx->mclk))
		return PTR_ERR(spdifrx->mclk);

	spdifrx->mclk_rate = clk_get_rate(spdifrx->mclk);

	spdifrx->irq = platform_get_irq(pdev, 0);
	if (spdifrx->irq < 0)
		return spdifrx->irq;

	spdifrx->info.debounce_time_ms = 100;
	spdifrx->info.liner_pcm = 1;
	spdifrx->info.liner_pcm_last = 1;
	timer_setup(&spdifrx->debounce_timer, rk_spdifrx_debounce_timer_isr, 0);
	timer_setup(&spdifrx->non_liner_timer, rk_spdifrx_non_liner_timer_isr, 0);
	timer_setup(&spdifrx->fifo_timer, rk_spdifrx_fifo_timer_isr, 0);
	INIT_WORK(&spdifrx->xrun_work, rk_spdifrx_xrun_work);

	ret = devm_request_threaded_irq(&pdev->dev, spdifrx->irq, NULL,
					rk_spdifrx_isr,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					dev_name(&pdev->dev), spdifrx);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	spdifrx->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
						&rk_spdifrx_regmap_config);
	if (IS_ERR(spdifrx->regmap))
		return PTR_ERR(spdifrx->regmap);

	spdifrx->capture_dma_data.addr = res->start + SPDIFRX_SMPDR;
	spdifrx->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	spdifrx->capture_dma_data.maxburst = 8;

	spdifrx->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, spdifrx);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = rk_spdifrx_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_runtime;
	}

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev, "Could not register PCM\n");
		goto err_pm_suspend;
	}

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &rk_spdifrx_component,
					      &rk_spdifrx_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "Could not register DAI\n");
		goto err_pm_suspend;
	}

	return 0;

err_pm_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		rk_spdifrx_runtime_suspend(&pdev->dev);
err_pm_runtime:
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int rk_spdifrx_remove(struct platform_device *pdev)
{
	struct rk_spdifrx_dev *spdifrx = dev_get_drvdata(&pdev->dev);

	del_timer_sync(&spdifrx->debounce_timer);
	del_timer_sync(&spdifrx->non_liner_timer);
	del_timer_sync(&spdifrx->fifo_timer);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		rk_spdifrx_runtime_suspend(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rockchip_spdifrx_suspend(struct device *dev)
{
	struct rk_spdifrx_dev *spdifrx = dev_get_drvdata(dev);

	regcache_mark_dirty(spdifrx->regmap);

	return 0;
}

static int rockchip_spdifrx_resume(struct device *dev)
{
	struct rk_spdifrx_dev *spdifrx = dev_get_drvdata(dev);
	int ret;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		return ret;
	ret = regcache_sync(spdifrx->regmap);
	pm_runtime_put(dev);

	return ret;
}
#endif

static const struct dev_pm_ops rk_spdifrx_pm_ops = {
	SET_RUNTIME_PM_OPS(rk_spdifrx_runtime_suspend, rk_spdifrx_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(rockchip_spdifrx_suspend, rockchip_spdifrx_resume)
};

static const struct of_device_id rk_spdifrx_match[] = {
	{ .compatible = "rockchip,rk3308-spdifrx", },
	{},
};
MODULE_DEVICE_TABLE(of, rk_spdifrx_match);

static struct platform_driver rk_spdifrx_driver = {
	.probe = rk_spdifrx_probe,
	.remove = rk_spdifrx_remove,
	.driver = {
		.name = "rockchip-spdifrx",
		.of_match_table = of_match_ptr(rk_spdifrx_match),
		.pm = &rk_spdifrx_pm_ops,
	},
};
module_platform_driver(rk_spdifrx_driver);

MODULE_ALIAS("platform:rockchip-spdifrx");
MODULE_DESCRIPTION("ROCKCHIP SPDIFRX Controller Interface");
MODULE_AUTHOR("Sugar Zhang <sugar.zhang@rock-chips.com>");
MODULE_LICENSE("GPL v2");
