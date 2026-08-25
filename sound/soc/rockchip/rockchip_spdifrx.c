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
#include <linux/workqueue.h>
#include <sound/asoundef.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>

#include "rockchip_spdifrx.h"

#define QUIRK_ALWAYS_ON		BIT(0)
#define WAIT_TIME_MS_MAX	10000
#define FIFO_WORK_DELAY_MS	100
#define NON_LINER_WORK_DELAY_MS	100

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
	struct delayed_work debounce_work;
	struct delayed_work non_liner_work;
	struct delayed_work fifo_work;
	unsigned int mclk_rate;
	unsigned int version;
	unsigned int wait_time;
	int irq;
	bool need_reset;
	bool fs_monitor;
	bool spurious_xrun;
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

	regcache_cache_only(spdifrx->regmap, true);
	clk_disable_unprepare(spdifrx->mclk);
	clk_disable_unprepare(spdifrx->hclk);

	reset_control_assert(spdifrx->reset);
	udelay(1);
	reset_control_deassert(spdifrx->reset);

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
		clk_disable_unprepare(spdifrx->mclk);
		dev_err(spdifrx->dev, "hclk clock enable failed %d\n", ret);
		return ret;
	}

	regcache_cache_only(spdifrx->regmap, false);
	regcache_mark_dirty(spdifrx->regmap);

	ret = regcache_sync(spdifrx->regmap);
	if (ret) {
		clk_disable_unprepare(spdifrx->mclk);
		clk_disable_unprepare(spdifrx->hclk);
		regcache_cache_only(spdifrx->regmap, true);
	}

	return ret;
}

static int rk_spdifrx_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);

	regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
			   SPDIFRX_INTEN_SYNCIE_MASK | SPDIFRX_INTEN_NSYNCIE_MASK |
			   SPDIFRX_INTEN_BTEIE_MASK | SPDIFRX_INTEN_NPSPIE_MASK |
			   SPDIFRX_INTEN_BMDEIE_MASK | SPDIFRX_INTEN_PEIE_MASK |
			   SPDIFRX_INTEN_CSCIE_MASK | SPDIFRX_INTEN_NVLDIE_MASK |
			   SPDIFRX_INTEN_FSCHGIE_MASK,
			   SPDIFRX_INTEN_SYNCIE_EN | SPDIFRX_INTEN_NSYNCIE_EN |
			   SPDIFRX_INTEN_BTEIE_EN | SPDIFRX_INTEN_NPSPIE_EN |
			   SPDIFRX_INTEN_BMDEIE_EN | SPDIFRX_INTEN_PEIE_EN |
			   SPDIFRX_INTEN_CSCIE_EN | SPDIFRX_INTEN_NVLDIE_EN |
			   SPDIFRX_INTEN_FSCHGIE_EN);
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

	if (spdifrx->wait_time)
		substream->wait_time = spdifrx->wait_time;

	if (spdifrx->version >= SPDIFRX_VER_2312) {
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			regmap_update_bits(spdifrx->regmap, SPDIFRX_CFGR,
					   SPDIFRX_CFGR_DAT_MASK, SPDIFRX_CFGR_DAT_JOIN);
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S32_LE:
			regmap_update_bits(spdifrx->regmap, SPDIFRX_CFGR,
					   SPDIFRX_CFGR_DAT_MASK, SPDIFRX_CFGR_DAT_JOIN_DIS);
			break;
		default:
			break;
		}
	}

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
		if (!spdifrx->fs_monitor) {
			mod_delayed_work(system_unbound_wq, &spdifrx->fifo_work,
					 msecs_to_jiffies(FIFO_WORK_DELAY_MS));
			dev_dbg(spdifrx->dev, "trigger start fifo work\n");
		}
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		cancel_delayed_work(&spdifrx->fifo_work);
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
				   SPDIFRX_INTEN_SYNCIE_MASK | SPDIFRX_INTEN_NSYNCIE_MASK |
				   SPDIFRX_INTEN_BTEIE_MASK | SPDIFRX_INTEN_NPSPIE_MASK |
				   SPDIFRX_INTEN_BMDEIE_MASK | SPDIFRX_INTEN_PEIE_MASK |
				   SPDIFRX_INTEN_CSCIE_MASK | SPDIFRX_INTEN_NVLDIE_MASK|
				   SPDIFRX_INTEN_FSCHGIE_MASK,
				   SPDIFRX_INTEN_SYNCIE_EN | SPDIFRX_INTEN_NSYNCIE_EN |
				   SPDIFRX_INTEN_BTEIE_EN | SPDIFRX_INTEN_NPSPIE_EN |
				   SPDIFRX_INTEN_BMDEIE_EN | SPDIFRX_INTEN_PEIE_EN |
				   SPDIFRX_INTEN_CSCIE_EN | SPDIFRX_INTEN_NVLDIE_EN |
				   SPDIFRX_INTEN_FSCHGIE_EN);

		pm_runtime_forbid(spdifrx->dev);
	}

	return 0;
}

static int rk_spdifrx_sync_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	ucontrol->value.integer.value[0] = spdifrx->info.sync;

	return 0;
}

static int rk_spdifrx_sample_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	ucontrol->value.integer.value[0] = spdifrx->info.sample_rate_src;
	ucontrol->value.integer.value[1] = spdifrx->info.sample_rate_cal;

	return 0;
}

static int rk_spdifrx_debounce_time_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	ucontrol->value.integer.value[0] = spdifrx->info.debounce_time_ms;

	return 0;
}

static int rk_spdifrx_debounce_time_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	spdifrx->info.debounce_time_ms = ucontrol->value.integer.value[0];

	return 0;
}

static int rk_spdifrx_sample_width_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	ucontrol->value.integer.value[0] = spdifrx->info.sample_width;

	return 0;
}

static int rk_spdifrx_liner_pcm_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	ucontrol->value.integer.value[0] = spdifrx->info.liner_pcm;

	return 0;
}

static int rk_spdifrx_wait_time_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	ucontrol->value.integer.value[0] = spdifrx->wait_time;

	return 0;
}

static int rk_spdifrx_wait_time_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	if (ucontrol->value.integer.value[0] > WAIT_TIME_MS_MAX)
		return -EINVAL;

	spdifrx->wait_time = ucontrol->value.integer.value[0];

	return 0;
}

static int rk_spdifrx_usync_threshold_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);
	unsigned int val;
	int ret;

	ret = pm_runtime_resume_and_get(spdifrx->dev);
	if (ret < 0)
		return ret;

	regmap_read(spdifrx->regmap, SPDIFRX_CDRST, &val);
	pm_runtime_put(spdifrx->dev);

	ucontrol->value.integer.value[0] = (val & SPDIFRX_CDRST_NOSTRTHR_MASK) >> 16;

	return 0;
}

static int rk_spdifrx_usync_threshold_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);
	int ret;

	ret = pm_runtime_resume_and_get(spdifrx->dev);
	if (ret < 0)
		return ret;

	regmap_update_bits(spdifrx->regmap, SPDIFRX_CDRST, SPDIFRX_CDRST_NOSTRTHR_MASK,
			   SPDIFRX_CDRST_NOSTRTHR(ucontrol->value.integer.value[0]));
	pm_runtime_put(spdifrx->dev);

	return 0;
}

static int rk_spdifrx_spurious_xrun_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	ucontrol->value.integer.value[0] = spdifrx->spurious_xrun;

	return 0;
}

static int rk_spdifrx_spurious_xrun_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compnt = snd_soc_kcontrol_component(kcontrol);
	struct rk_spdifrx_dev *spdifrx = snd_soc_component_get_drvdata(compnt);

	spdifrx->spurious_xrun = ucontrol->value.integer.value[0];

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

static int rk_spdifrx_wait_time_info(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = WAIT_TIME_MS_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int rk_spdifrx_usync_threshold_info(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff;

	return 0;
}

static int rk_spdifrx_spurious_xrun_info(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static struct snd_kcontrol_new rk_spdifrx_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "RK SPDIFRX SYNC STATUS",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = rk_spdifrx_sync_info,
		.get = rk_spdifrx_sync_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "RK SPDIFRX SAMPLE RATE",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = rk_spdifrx_sample_rate_info,
		.get = rk_spdifrx_sample_rate_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "RK SPDIFRX DEBOUNCE TIME",
		.info = rk_spdifrx_debounce_time_info,
		.get = rk_spdifrx_debounce_time_get,
		.put = rk_spdifrx_debounce_time_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "RK SPDIFRX SAMPLE WIDTH",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = rk_spdifrx_sample_width_info,
		.get = rk_spdifrx_sample_width_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "RK SPDIFRX LINER PCM",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = rk_spdifrx_liner_pcm_info,
		.get = rk_spdifrx_liner_pcm_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "RK SPDIFRX WAIT TIME MS",
		.info = rk_spdifrx_wait_time_info,
		.get = rk_spdifrx_wait_time_get,
		.put = rk_spdifrx_wait_time_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "RK SPDIFRX USYNC THRESHOLD CNT",
		.info = rk_spdifrx_usync_threshold_info,
		.get = rk_spdifrx_usync_threshold_get,
		.put = rk_spdifrx_usync_threshold_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "RK SPDIFRX SPURIOUS XRUN",
		.info = rk_spdifrx_spurious_xrun_info,
		.get = rk_spdifrx_spurious_xrun_get,
		.put = rk_spdifrx_spurious_xrun_put,
	},
};

static struct snd_kcontrol_new rk_spdifrx_v2312_controls[] = {
	SOC_SINGLE("RK SPDIFRX CDR WAIT TIME", SPDIFRX_CDRTIME, 0, 0xffff, 0),
	SOC_SINGLE("RK SPDIFRX CDR COUNT TIME", SPDIFRX_CDRTIME, 16, 0xffff, 0),
};

static struct snd_kcontrol_new rk_spdifrx_v2505_controls[] = {
	SOC_SINGLE("RK SPDIFRX NON-LINER PCM MUTE", SPDIFRX_CFGR, 3, 1, 0),
};

static struct snd_kcontrol_new rk_spdifrx_v2508_controls[] = {
	SOC_SINGLE("RK SPDIFRX USYNC THRESHOLD STEP", SPDIFRX_CFGR, 10, 0xf, 0),
};

static int rk_spdifrx_dai_probe(struct snd_soc_dai *dai)
{
	struct rk_spdifrx_dev *spdifrx = snd_soc_dai_get_drvdata(dai);
	int ret;

	snd_soc_dai_dma_data_set_capture(dai, &spdifrx->capture_dma_data);
	spdifrx->dai = dai;

	if (spdifrx->version >= SPDIFRX_VER_2312)
		snd_soc_add_component_controls(dai->component,
					       rk_spdifrx_v2312_controls,
					       ARRAY_SIZE(rk_spdifrx_v2312_controls));

	if (spdifrx->version >= SPDIFRX_VER_2505)
		snd_soc_add_component_controls(dai->component,
					       rk_spdifrx_v2505_controls,
					       ARRAY_SIZE(rk_spdifrx_v2505_controls));

	if (spdifrx->version >= SPDIFRX_VER_2508)
		snd_soc_add_component_controls(dai->component,
					       rk_spdifrx_v2508_controls,
					       ARRAY_SIZE(rk_spdifrx_v2508_controls));

	ret = pm_runtime_resume_and_get(spdifrx->dev);
	if (ret < 0)
		return ret;

	rk_spdifrx_parse_quirks(spdifrx);
	pm_runtime_put(spdifrx->dev);

	spdifrx->need_reset = true;

	return 0;
}

static const struct snd_soc_dai_ops rk_spdifrx_dai_ops = {
	.probe = rk_spdifrx_dai_probe,
	.hw_params = rk_spdifrx_hw_params,
	.trigger = rk_spdifrx_trigger,
};

static struct snd_soc_dai_driver rk_spdifrx_dai = {
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
	.controls = rk_spdifrx_controls,
	.num_controls = ARRAY_SIZE(rk_spdifrx_controls),
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
	case SPDIFRX_CDRTIME:
	case SPDIFRX_BURSTINFO:
		return true;
	default:
		return false;
	}
}

static bool rk_spdifrx_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SPDIFRX_VERSION:
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
	case SPDIFRX_CDRTIME:
	case SPDIFRX_CNTINFO:
	case SPDIFRX_BURSTINFO:
		return true;
	default:
		return false;
	}
}

static bool rk_spdifrx_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SPDIFRX_VERSION:
	case SPDIFRX_CLR:
	case SPDIFRX_CDR:
	case SPDIFRX_CDRST:
	case SPDIFRX_FIFOCTRL:
	case SPDIFRX_INTSR:
	case SPDIFRX_INTCLR:
	case SPDIFRX_SMPDR:
	case SPDIFRX_CHNSR1:
	case SPDIFRX_CHNSR2:
	case SPDIFRX_CNTINFO:
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

static const struct reg_default rk_spdifrx_reg_defaults[] = {
	{ SPDIFRX_CDRTIME, 0x03ff03ff },
};

static const struct regmap_config rk_spdifrx_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = SPDIFRX_BURSTINFO,
	.reg_defaults = rk_spdifrx_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(rk_spdifrx_reg_defaults),
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

static bool rk_spdifrx_is_locked(struct rk_spdifrx_dev *spdifrx)
{
	u32 val = 0;

	regmap_read(spdifrx->regmap, SPDIFRX_CDR, &val);

	return (val & SPDIFRX_CDR_CS_MASK) == SPDIFRX_CDR_CS_LOCKED;
}

static u32 rk_spdifrx_get_cdr_count(struct rk_spdifrx_dev *spdifrx, u32 *mincnt, u32 *maxcnt)
{
	u32 val, min_cnt, max_cnt, count;

	if (spdifrx->version >= SPDIFRX_VER_2505) {
		regmap_read(spdifrx->regmap, SPDIFRX_CNTINFO, &val);
		min_cnt = (val & SPDIFRX_CNTINFO_MINCNT_MASK) + 1;
		max_cnt = ((val & SPDIFRX_CNTINFO_MAXCNT_MASK) >> 10) + 1;
		count = (min_cnt + max_cnt) / 4;
	} else {
		regmap_read(spdifrx->regmap, SPDIFRX_CDRST, &val);
		min_cnt = (val & SPDIFRX_CDRST_MINCNT_MASK) + 1;
		max_cnt = ((val & SPDIFRX_CDRST_MAXCNT_MASK) >> 8) + 1;
		/*
		 * For 8-bit CDR counters (version < SPDIFRX_VER_2505), maxcnt is
		 * theoretically 3 * mincnt in the adjusted (+1) count domain. Average
		 * the min/max counts only when maxcnt is still greater than 2 * mincnt
		 * and 3 * mincnt fits within the adjusted count range (whose maximum
		 * less than 256), indicating that maxcnt has not overflowed.
		 */
		if ((max_cnt > 2 * min_cnt) && (3 * min_cnt < 256))
			count = (min_cnt + max_cnt) / 4;
		else
			count = min_cnt;
	}

	if (mincnt)
		*mincnt = min_cnt;
	if (maxcnt)
		*maxcnt = max_cnt;

	return count;
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
	u32 count, mincnt, maxcnt;
	bool reset_now = false;
	bool locked;

	if (pm_runtime_resume_and_get(spdifrx->dev) < 0)
		return IRQ_NONE;

	regmap_read(spdifrx->regmap, SPDIFRX_INTSR, &intsr);
	regmap_write(spdifrx->regmap, SPDIFRX_INTCLR, intsr);
	locked = rk_spdifrx_is_locked(spdifrx);

	if (intsr & SPDIFRX_INTSR_FSCHGISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "FS Changed\n");
		spdifrx->need_reset = true;
		if (spdifrx->spurious_xrun)
			rk_spdifrx_disable_dma(spdifrx);
	}

	if (intsr & SPDIFRX_INTSR_NVLDISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "No Valid Error\n");
		if (!spdifrx->fs_monitor) {
			reset_now = true;
			spdifrx->need_reset = true;
			regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
					   SPDIFRX_INTEN_NVLDIE_MASK, SPDIFRX_INTEN_NVLDIE_DIS);
		}
	}

	if (intsr & SPDIFRX_INTSR_CSCISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "CSC Changed\n");
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_BTEIE_MASK, SPDIFRX_INTEN_BTEIE_EN);
	}

	if (intsr & SPDIFRX_INTSR_PEISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "Parity Error\n");
		if (!spdifrx->fs_monitor) {
			reset_now = true;
			spdifrx->need_reset = true;
		}
	}

	if (intsr & SPDIFRX_INTSR_NPSPISR_ACTIVE) {
		spdifrx->info.liner_pcm = 0;
		if (spdifrx->info.liner_pcm != spdifrx->info.liner_pcm_last) {
			snd_ctl_notify(dai->component->card->snd_card,
				       SNDRV_CTL_EVENT_MASK_VALUE, &liner_pcm_kctl->id);
			spdifrx->info.liner_pcm_last = spdifrx->info.liner_pcm;
			dev_dbg(spdifrx->dev, "non liner data\n");
		}
		mod_delayed_work(system_unbound_wq, &spdifrx->non_liner_work,
				 msecs_to_jiffies(NON_LINER_WORK_DELAY_MS));
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_NVLDIE_MASK, SPDIFRX_INTEN_NVLDIE_DIS);
	}

	if (intsr & SPDIFRX_INTSR_BMDEISR_ACTIVE) {
		dev_dbg(spdifrx->dev, "BMD Error\n");
		if (!spdifrx->fs_monitor) {
			reset_now = true;
			spdifrx->need_reset = true;
		}
	}

	if ((intsr & SPDIFRX_INTSR_NSYNCISR_ACTIVE) && !locked) {
		spdifrx->info.sync = 0;
		mod_delayed_work(system_unbound_wq, &spdifrx->debounce_work,
				 msecs_to_jiffies(spdifrx->info.debounce_time_ms));
		dev_dbg(spdifrx->dev, "NSYNC\n");
		if (!spdifrx->fs_monitor) {
			spdifrx->need_reset = true;
			regmap_write(spdifrx->regmap, SPDIFRX_CLR, SPDIFRX_CLR_RXSC);
			regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
					   SPDIFRX_INTEN_NSYNCIE_MASK,
					   SPDIFRX_INTEN_NSYNCIE_DIS);
		}
	}

	if (intsr & SPDIFRX_INTSR_BTEISR_ACTIVE) {
		regmap_read(spdifrx->regmap, SPDIFRX_CHNSR1, &val);
		spdifrx->info.sample_rate_src =
			rk_spdifrx_get_sample_rate((val & SPDIFRX_CHNSR1_SAMPLE_RATE_MASK) >> 8);

		count = rk_spdifrx_get_cdr_count(spdifrx, NULL, NULL);
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

		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_BTEIE_MASK, SPDIFRX_INTEN_BTEIE_DIS);
	}

	if ((intsr & SPDIFRX_INTSR_SYNCISR_ACTIVE) && locked) {
		spdifrx->info.sync = 1;
		mod_delayed_work(system_unbound_wq, &spdifrx->debounce_work,
				 msecs_to_jiffies(spdifrx->info.debounce_time_ms));
		rk_spdifrx_get_cdr_count(spdifrx, &mincnt, &maxcnt);
		if (spdifrx->version < SPDIFRX_VER_2505)
			regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
					   SPDIFRX_INTEN_NSYNCIE_MASK, SPDIFRX_INTEN_NSYNCIE_EN);
		dev_dbg(spdifrx->dev, "SYNC: MINCNT = %u, MAXCNT = %u\n", mincnt, maxcnt);
		regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
				   SPDIFRX_INTEN_BTEIE_MASK, SPDIFRX_INTEN_BTEIE_EN);
	}

	if (reset_now) {
		dev_dbg(spdifrx->dev, "irq reset spdifrx\n");
		rk_spdifrx_reset(spdifrx);
		if (spdifrx->spurious_xrun)
			rk_spdifrx_disable_dma(spdifrx);
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
 * and the system proceeds to the next scheduled work interval.
 */
static void rk_spdifrx_fifo_work(struct work_struct *work)
{
	struct rk_spdifrx_dev *spdifrx = container_of(to_delayed_work(work),
						      struct rk_spdifrx_dev, fifo_work);
	unsigned int val, timeout_us;
	unsigned int fifo_cnt;
	unsigned int sample_rate;
	unsigned int src_rate = spdifrx->info.sample_rate_src;
	unsigned int cal_rate = spdifrx->info.sample_rate_cal;
	ktime_t start, end;
	int ret;

	/*
	 * A sample rate reported in the channel status is more accurate than the
	 * CDR estimate. However, some transmitters leave the channel status unset,
	 * which is parsed as the default rate of 44.1 kHz. Prefer the CDR estimate
	 * for that ambiguous value when available, as an unset value cannot be
	 * distinguished from a genuine 44.1 kHz report. When both indicate
	 * 44.1 kHz, the selected rate is unchanged. Use the reported source rate
	 * otherwise.
	 */
	if (cal_rate && (src_rate == 0 || src_rate == 44100))
		sample_rate = cal_rate;
	else
		sample_rate = src_rate;

	if (spdifrx->info.sync == 0 || spdifrx->need_reset || sample_rate == 0) {
		dev_dbg(spdifrx->dev, "exit fifo work\n");
		dev_dbg(spdifrx->dev, "sync: %d, need_reset: %d, sample_rate: %u\n",
			spdifrx->info.sync, spdifrx->need_reset, sample_rate);
		return;
	}

	if (pm_runtime_resume_and_get(spdifrx->dev) < 0)
		return;

	regmap_read(spdifrx->regmap, SPDIFRX_DMACR, &val);
	if ((val & SPDIFRX_DMACR_RDE_MASK) == 0) {
		dev_dbg(spdifrx->dev, "exit fifo work: rxdma disabled\n");
		goto out;
	}

	timeout_us = DIV_ROUND_UP(500000, sample_rate);
	if (spdifrx->version >= SPDIFRX_VER_2312) {
		regmap_read(spdifrx->regmap, SPDIFRX_CFGR, &val);
		if (val & SPDIFRX_CFGR_DAT_JOIN)
			timeout_us *= 2;
	}

	start = ktime_get();
	regmap_read(spdifrx->regmap, SPDIFRX_FIFOCTRL, &val);
	fifo_cnt = (val & SPDIFRX_FIFOCTRL_RFL_MASK) >> 8;

	if (fifo_cnt < 8) {
		ret = regmap_read_poll_timeout_atomic(spdifrx->regmap, SPDIFRX_FIFOCTRL, val,
						      ((val & SPDIFRX_FIFOCTRL_RFL_MASK) >> 8) !=
						      fifo_cnt, 1, timeout_us);
		end = ktime_get();
		if (ret == -ETIMEDOUT && ktime_us_delta(end, start) < 8 * timeout_us) {
			dev_dbg(spdifrx->dev, "no data to fifo, reset\n");
			rk_spdifrx_reset(spdifrx);
			spdifrx->need_reset = true;
			if (spdifrx->spurious_xrun)
				rk_spdifrx_disable_dma(spdifrx);
			goto out;
		}
	}
	mod_delayed_work(system_unbound_wq, &spdifrx->fifo_work,
			 msecs_to_jiffies(FIFO_WORK_DELAY_MS));
out:
	pm_runtime_put(spdifrx->dev);
}

static void rk_spdifrx_non_liner_work(struct work_struct *work)
{
	struct rk_spdifrx_dev *spdifrx = container_of(to_delayed_work(work),
						      struct rk_spdifrx_dev, non_liner_work);
	struct snd_soc_dai *dai = spdifrx->dai;
	struct snd_kcontrol *liner_pcm_kctl = snd_soc_card_get_kcontrol(dai->component->card,
									"RK SPDIFRX LINER PCM");

	if (pm_runtime_resume_and_get(spdifrx->dev) < 0)
		return;

	spdifrx->info.liner_pcm = 1;
	snd_ctl_notify(dai->component->card->snd_card,
		       SNDRV_CTL_EVENT_MASK_VALUE, &liner_pcm_kctl->id);
	spdifrx->info.liner_pcm_last = spdifrx->info.liner_pcm;
	regmap_update_bits(spdifrx->regmap, SPDIFRX_INTEN,
			   SPDIFRX_INTEN_NVLDIE_MASK, SPDIFRX_INTEN_NVLDIE_EN);
	dev_dbg(spdifrx->dev, "liner data\n");

	pm_runtime_put(spdifrx->dev);
}

static void rk_spdifrx_spurious_xrun(struct rk_spdifrx_dev *spdifrx)
{
	int ret;
	u32 val;

	if (!spdifrx->spurious_xrun)
		return;

	ret = regmap_read_poll_timeout(spdifrx->regmap, SPDIFRX_CDR, val,
				       (val & SPDIFRX_CDR_CS_MASK) == SPDIFRX_CDR_CS_LOCKED,
				       300, 3000);
	if (!ret) {
		if (spdifrx->substream) {
			dev_dbg(spdifrx->dev, "spdifrx spurious stop xrun\n");
			snd_pcm_stop_xrun(spdifrx->substream);
		}
	} else {
		dev_dbg(spdifrx->dev, "reset enter sync failed\n");
	}
}

static void rk_spdifrx_debounce_work(struct work_struct *work)
{
	struct rk_spdifrx_dev *spdifrx = container_of(to_delayed_work(work),
						      struct rk_spdifrx_dev, debounce_work);
	struct snd_soc_dai *dai = spdifrx->dai;
	struct snd_kcontrol *sync_kctl = snd_soc_card_get_kcontrol(dai->component->card,
								   "RK SPDIFRX SYNC STATUS");
	struct snd_kcontrol *sample_kctl = snd_soc_card_get_kcontrol(dai->component->card,
								     "RK SPDIFRX SAMPLE RATE");
	u32 val;
	u32 count;

	if (pm_runtime_resume_and_get(spdifrx->dev) < 0)
		return;

	if (spdifrx->info.sync == 1) {
		if (spdifrx->need_reset && !spdifrx->fs_monitor) {
			dev_dbg(spdifrx->dev, "debounce reset spdifrx\n");
			rk_spdifrx_reset(spdifrx);
			spdifrx->need_reset = false;
			rk_spdifrx_spurious_xrun(spdifrx);
		} else {
			if (spdifrx->need_reset) {
				spdifrx->need_reset = false;
				rk_spdifrx_spurious_xrun(spdifrx);
			}

			count = rk_spdifrx_get_cdr_count(spdifrx, NULL, NULL);
			if (count > 0)
				spdifrx->info.sample_rate_cal =
					rk_spdifrx_convert_sample_rate(spdifrx->mclk_rate, count);
			else
				spdifrx->info.sample_rate_cal = 0;

			if (!spdifrx->fs_monitor) {
				regmap_read(spdifrx->regmap, SPDIFRX_DMACR, &val);
				if (val & SPDIFRX_DMACR_RDE_MASK) {
					mod_delayed_work(system_unbound_wq, &spdifrx->fifo_work,
							 msecs_to_jiffies(FIFO_WORK_DELAY_MS));
					dev_dbg(spdifrx->dev, "debounce start fifo work\n");
				}
			}

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

	pm_runtime_put(spdifrx->dev);
}

static int rk_spdifrx_wait_time_init(struct rk_spdifrx_dev *spdifrx)
{
	unsigned int wait_time;

	if (!device_property_read_u32(spdifrx->dev, "rockchip,wait-time-ms", &wait_time)) {
		if (wait_time > WAIT_TIME_MS_MAX) {
			dev_err(spdifrx->dev, "Failed to set init wait-time-ms: exceed max\n");
			return -EINVAL;
		}
		dev_dbg(spdifrx->dev, "Init wait-time-ms: %u\n", wait_time);
		spdifrx->wait_time = wait_time;
	}

	return 0;
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

	spdifrx->spurious_xrun = device_property_read_bool(&pdev->dev, "rockchip,spurious-xrun");

	spdifrx->info.debounce_time_ms = 100;
	spdifrx->info.liner_pcm = 1;
	spdifrx->info.liner_pcm_last = 1;

	INIT_DELAYED_WORK(&spdifrx->debounce_work, rk_spdifrx_debounce_work);
	INIT_DELAYED_WORK(&spdifrx->non_liner_work, rk_spdifrx_non_liner_work);
	INIT_DELAYED_WORK(&spdifrx->fifo_work, rk_spdifrx_fifo_work);

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

	ret = clk_prepare_enable(spdifrx->hclk);
	if (ret)
		return ret;

	regmap_read(spdifrx->regmap, SPDIFRX_VERSION, &spdifrx->version);
	if (spdifrx->version >= SPDIFRX_VER_2505) {
		regmap_update_bits(spdifrx->regmap, SPDIFRX_CFGR,
				   SPDIFRX_CFGR_MONITOR_THR_MASK |
				   SPDIFRX_CFGR_MONITOR_EN_MASK,
				   SPDIFRX_CFGR_MONITOR_THR(0xa) |
				   SPDIFRX_CFGR_MONITOR_EN);
		regmap_update_bits(spdifrx->regmap, SPDIFRX_CDRST,
				   SPDIFRX_CDRST_MONITOR_CNT_MASK,
				   SPDIFRX_CDRST_MONITOR_CNT(0x3ff));
		spdifrx->fs_monitor = true;
	}

	regmap_update_bits(spdifrx->regmap, SPDIFRX_CDRST, SPDIFRX_CDRST_NOSTRTHR_MASK,
			   SPDIFRX_CDRST_NOSTRTHR(0x3ff));

	clk_disable_unprepare(spdifrx->hclk);

	rk_spdifrx_wait_time_init(spdifrx);

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

static void rk_spdifrx_remove(struct platform_device *pdev)
{
	struct rk_spdifrx_dev *spdifrx = dev_get_drvdata(&pdev->dev);

	cancel_delayed_work_sync(&spdifrx->debounce_work);
	cancel_delayed_work_sync(&spdifrx->non_liner_work);
	cancel_delayed_work_sync(&spdifrx->fifo_work);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		rk_spdifrx_runtime_suspend(&pdev->dev);
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
