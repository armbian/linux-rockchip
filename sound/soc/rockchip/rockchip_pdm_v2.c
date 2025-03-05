// SPDX-License-Identifier: GPL-2.0-only
/*
 * Rockchip PDM ALSA SoC Digital Audio Interface(DAI)  driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/rational.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "rockchip_pdm_v2.h"
#include "rockchip_utils.h"

#define PDM_V2_DMA_BURST_SIZE		(8) /* size * width: 8*4 = 32 bytes */
#define PDM_V2_PATH_MAX			(4)
#define PDM_V2_DEFAULT_RATE		(48000)
#define PDM_V2_START_DELAY_MS_DEFAULT	(20)
#define PDM_V2_START_DELAY_MS_MIN	(0)
#define PDM_V2_START_DELAY_MS_MAX	(1000)
#define PDM_V2_REF_CLK_MAX		61440000

#define QUIRK_ALWAYS_ON			BIT(0)

#define RK3506_PDM			0x2311
#define RK3576_PDM			0x2302

struct rk_pdm_v2_clkref {
	unsigned int sr;
	unsigned int clk;
	unsigned int clk_out;
};

static const struct rk_pdm_v2_clkref clkref[] = {
	{ 8000, 40960000, 2048000 },
	{ 11025, 56448000, 2822400 },
	{ 12000, 61440000, 3072000 },
	{ 8000, 24000000, 2400000 },
	{ 12000, 24000000, 2400000 },
};

static const struct pdm_of_quirks {
	char *quirk;
	int id;
} of_quirks[] = {
	{
		.quirk = "rockchip,always-on",
		.id = QUIRK_ALWAYS_ON,
	},
};

struct rk_pdm_v2_dev {
	struct device *dev;
	struct clk *clk;
	struct clk *hclk;
	struct clk *clk_out;
	struct regmap *regmap;
	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct reset_control *reset;
	struct pinctrl *pinctrl;
	struct pinctrl_state *clk_state;
	unsigned int start_delay_ms;
	unsigned int clk_ref_frq;
	unsigned int quirks;
	unsigned int version;
};

static int get_pdm_v2_clkref(struct rk_pdm_v2_dev *pdm, unsigned int sr)
{
	int i, clk_ref = 0;

	if (!sr)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(clkref); i++) {
		if (sr % clkref[i].sr)
			continue;

		if (clkref[i].clk_out % sr)
			continue;

		if (pdm->clk_ref_frq != 0) {
			if (pdm->clk_ref_frq != clkref[i].clk)
				continue;
		}

		clk_ref = 1;
		break;
	}

	if (clk_ref)
		return i;
	else
		return -EINVAL;
}

static inline struct rk_pdm_v2_dev *to_info(struct snd_soc_dai *dai)
{
	return snd_soc_dai_get_drvdata(dai);
}

static void rockchip_pdm_v2_rxctrl(struct rk_pdm_v2_dev *pdm, int on)
{
	if (on) {
		regmap_update_bits(pdm->regmap, PDM_V2_SYSCONFIG,
				   PDM_V2_NUM_MSK,
				   PDM_V2_NUM_START);
	} else {
		regmap_update_bits(pdm->regmap, PDM_V2_FIFO_CTRL,
				   PDM_V2_DMA_RD_MSK, PDM_V2_DMA_RD_DIS);
		regmap_update_bits(pdm->regmap, PDM_V2_SYSCONFIG,
				   PDM_V2_RX_MSK | PDM_V2_RX_CLR_MSK | PDM_V2_NUM_MSK,
				   PDM_V2_RX_STOP | PDM_V2_RX_CLR_WR | PDM_V2_NUM_STOP);
	}
}

static int rockchip_pdm_v2_set_samplerate(struct rk_pdm_v2_dev *pdm, unsigned int samplerate)
{
	unsigned int upsamplerate, mclk, ratio, scale = 0;
	int index, ret = 0;

	index = get_pdm_v2_clkref(pdm, samplerate);
	if (index < 0)
		return -EINVAL;

	mclk = clkref[index].clk;
	upsamplerate = clkref[index].clk_out;
	ret = clk_set_rate(pdm->clk, mclk);
	if (ret)
		return ret;
	/* Move the pdm clk source to cru */
	ret = clk_set_rate(pdm->clk_out, upsamplerate);
	if (ret)
		return ret;

	ratio = upsamplerate / samplerate / 2;
	switch (ratio) {
	case 8:
		scale = 27;
		break;
	case 12:
		scale = 33;
		break;
	case 16:
		scale = 36;
		break;
	case 24:
		scale = 42;
		break;
	case 25:
		scale = 42;
		break;
	case 32:
		scale = 45;
		break;
	case 48:
		scale = 51;
		break;
	case 50:
		scale = 51;
		break;
	case 64:
		scale = 54;
		break;
	case 75:
		scale = 57;
		break;
	case 96:
		scale = 60;
		break;
	case 100:
		scale = 60;
		break;
	case 125:
		scale = 63;
		break;
	case 150:
		scale = 66;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret)
		return ret;

	dev_dbg(pdm->dev, "The upsamplerate is %d, ratio is %d, scale is %d\n",
		upsamplerate, ratio, scale);
	regmap_update_bits(pdm->regmap, PDM_V2_FILTER_CTRL,
			   PDM_V2_CIC_SCALE_MSK | PDM_V2_CIC_RATIO_MSK,
			   PDM_V2_CIC_SCALE(scale) | PDM_V2_CIC_RATIO(ratio));

	return 0;
}

static int rockchip_pdm_v2_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params,
				     struct snd_soc_dai *dai)
{
	struct rk_pdm_v2_dev *pdm = to_info(dai);
	unsigned int val = 0;

	regmap_update_bits(pdm->regmap, PDM_V2_CTRL,
			   PDM_V2_SJM_SEL_MSK, PDM_V2_SJM_SEL_L);
	if (pdm->version == RK3506_PDM) {
		regmap_update_bits(pdm->regmap, PDM_V2_FILTER_CTRL,
				   PDM_V2_HPF_V2_R_MSK | PDM_V2_HPF_V2_L_MSK |
				   PDM_V2_HPF_V2_FREQ_MSK,
				   PDM_V2_HPF_V2_R_EN | PDM_V2_HPF_V2_L_EN |
				   PDM_V2_HPF_V2_FREQ_60);
	} else if (pdm->version == RK3576_PDM) {
		regmap_update_bits(pdm->regmap, PDM_V2_FILTER_CTRL,
				   PDM_V2_HPF_R_MSK | PDM_V2_HPF_L_MSK | PDM_V2_HPF_FREQ_MSK,
				   PDM_V2_HPF_R_EN | PDM_V2_HPF_L_EN | PDM_V2_HPF_FREQ_60);
	}

	rockchip_pdm_v2_set_samplerate(pdm, params_rate(params));
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		val |= PDM_V2_VDW(16);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val |= PDM_V2_VDW(24);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		val |= PDM_V2_VDW(24);
		break;
	default:
		return -EINVAL;
	}

	switch (params_channels(params)) {
	case 8:
		val |= PDM_V2_PATH3_EN;
		fallthrough;
	case 6:
		val |= PDM_V2_PATH2_EN;
		fallthrough;
	case 4:
		val |= PDM_V2_PATH1_EN;
		fallthrough;
	case 2:
		val |= PDM_V2_PATH0_EN;
		break;
	default:
		dev_err(pdm->dev, "invalid channel: %d\n",
			params_channels(params));
		return -EINVAL;
	}

	regmap_update_bits(pdm->regmap, PDM_V2_CTRL,
			   PDM_V2_PATH_MSK | PDM_V2_VDW_MSK,
			   val);
	/* all channels share the single FIFO */
	regmap_update_bits(pdm->regmap, PDM_V2_FIFO_CTRL, PDM_V2_RDL_MSK,
			   PDM_V2_DMA_RDL(8 * params_channels(params)));

	return 0;
}

static int rockchip_pdm_v2_set_fmt(struct snd_soc_dai *cpu_dai,
				   unsigned int fmt)
{
	struct rk_pdm_v2_dev *pdm = to_info(cpu_dai);
	unsigned int mask = 0, val = 0;

	mask = PDM_V2_CKP_MSK;
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		val = PDM_V2_CKP_NORMAL;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		val = PDM_V2_CKP_INVERTED;
		break;
	default:
		return -EINVAL;
	}

	pm_runtime_get_sync(cpu_dai->dev);
	regmap_update_bits(pdm->regmap, PDM_V2_CTRL, mask, val);
	pm_runtime_put(cpu_dai->dev);

	return 0;
}

static int rockchip_pdm_v2_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *dai)
{
	struct rk_pdm_v2_dev *pdm = to_info(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		rockchip_pdm_v2_rxctrl(pdm, 1);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		rockchip_pdm_v2_rxctrl(pdm, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int rockchip_pdm_v2_prepare(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct rk_pdm_v2_dev *pdm = to_info(dai);

	regmap_update_bits(pdm->regmap, PDM_V2_FIFO_CTRL,
			   PDM_V2_DMA_RD_MSK, PDM_V2_DMA_RD_EN);
	regmap_update_bits(pdm->regmap, PDM_V2_SYSCONFIG,
			   PDM_V2_RX_MSK,
			   PDM_V2_RX_START);
	usleep_range((pdm->start_delay_ms) * 1000, (pdm->start_delay_ms + 1) * 1000);

	return 0;
}

static const struct snd_kcontrol_new rk3506_controls[];
static const struct snd_kcontrol_new rk3576_controls[];

static int rockchip_pdm_v2_dai_probe(struct snd_soc_dai *dai)
{
	struct rk_pdm_v2_dev *pdm = to_info(dai);

	dai->capture_dma_data = &pdm->capture_dma_data;

	if (pdm->version == RK3506_PDM)
		snd_soc_add_component_controls(dai->component, rk3506_controls, 1);
	else if (pdm->version == RK3576_PDM)
		snd_soc_add_component_controls(dai->component, rk3576_controls, 1);

	return 0;
}

static const struct snd_soc_dai_ops rockchip_pdm_v2_dai_ops = {
	.set_fmt = rockchip_pdm_v2_set_fmt,
	.trigger = rockchip_pdm_v2_trigger,
	.prepare = rockchip_pdm_v2_prepare,
	.hw_params = rockchip_pdm_v2_hw_params,
};

#define ROCKCHIP_PDM_V2_RATES SNDRV_PCM_RATE_8000_192000
#define ROCKCHIP_PDM_V2_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
				 SNDRV_PCM_FMTBIT_S24_LE | \
				 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver rockchip_pdm_v2_dai = {
	.probe = rockchip_pdm_v2_dai_probe,
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 8,
		.rates = ROCKCHIP_PDM_V2_RATES,
		.formats = ROCKCHIP_PDM_V2_FORMATS,
	},
	.ops = &rockchip_pdm_v2_dai_ops,
	.symmetric_rate = 1,
};

static int rockchip_pdm_v2_start_delay_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_pdm_v2_dev *pdm = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = pdm->start_delay_ms;

	return 0;
}

static int rockchip_pdm_v2_start_delay_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_pdm_v2_dev *pdm = snd_soc_component_get_drvdata(component);

	if ((ucontrol->value.integer.value[0] < PDM_V2_START_DELAY_MS_MIN) ||
	    (ucontrol->value.integer.value[0] > PDM_V2_START_DELAY_MS_MAX))
		return -EINVAL;

	pdm->start_delay_ms = ucontrol->value.integer.value[0];

	return 1;
}

static int rockchip_pdm_v2_clk_ref_frq_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_pdm_v2_dev *pdm = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = pdm->clk_ref_frq;

	return 0;
}

static int rockchip_pdm_v2_clk_ref_frq_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rk_pdm_v2_dev *pdm = snd_soc_component_get_drvdata(component);

	pdm->clk_ref_frq = ucontrol->value.integer.value[0];

	return 1;
}

static const char * const rpaths_text[] = {
	"From SDI0", "From SDI1", "From SDI2", "From SDI3" };

static SOC_ENUM_SINGLE_DECL(rpath3_enum, PDM_V2_CTRL, 19, rpaths_text);
static SOC_ENUM_SINGLE_DECL(rpath2_enum, PDM_V2_CTRL, 17, rpaths_text);
static SOC_ENUM_SINGLE_DECL(rpath1_enum, PDM_V2_CTRL, 15, rpaths_text);
static SOC_ENUM_SINGLE_DECL(rpath0_enum, PDM_V2_CTRL, 13, rpaths_text);

static const char * const hpf_cutoff_text[] = {
	"3.79Hz", "60Hz", "243Hz", "493Hz",
};

static const char * const hpf_v2_cutoff_text[] = {
	"0.234Hz", "0.468Hz", "0.937Hz", "1.875Hz", "3.75Hz",
	"7.5Hz", "15Hz", "30Hz", "60Hz", "122Hz", "251Hz",
	"528Hz", "1183Hz", "3152Hz",
};

static SOC_ENUM_SINGLE_DECL(hpf_cutoff_enum, PDM_V2_FILTER_CTRL,
			    19, hpf_cutoff_text);
static SOC_ENUM_SINGLE_DECL(hpf_v2_cutoff_enum, PDM_V2_FILTER_CTRL,
			    21, hpf_v2_cutoff_text);
static const DECLARE_TLV_DB_SCALE(pdm_v2_digtal_gain_tlv, -6563, 75, 0);

static const struct snd_kcontrol_new rockchip_pdm_v2_controls[] = {
	SOC_ENUM("Receive PATH3 Source Select", rpath3_enum),
	SOC_ENUM("Receive PATH2 Source Select", rpath2_enum),
	SOC_ENUM("Receive PATH1 Source Select", rpath1_enum),
	SOC_ENUM("Receive PATH0 Source Select", rpath0_enum),

	SOC_SINGLE_EXT("Start Delay Ms", 0, 0, PDM_V2_START_DELAY_MS_MAX, 0,
			rockchip_pdm_v2_start_delay_get,
			rockchip_pdm_v2_start_delay_put),

	SOC_SINGLE_EXT("Reference Clock Frequency", 0, 0, PDM_V2_REF_CLK_MAX, 0,
			rockchip_pdm_v2_clk_ref_frq_get,
			rockchip_pdm_v2_clk_ref_frq_put),
};

static const struct snd_kcontrol_new rk3506_controls[] = {
	SOC_SINGLE_RANGE_TLV("Gain Volume",
			     PDM_V2_GAIN_CTRL,
			     PDM_V2_GAIN_CTRL_SHIFT,
			     PDM_V2_GAIN_CTRL_MIN,
			     PDM_V2_GAIN_CTRL_MAX,
			     0, pdm_v2_digtal_gain_tlv),
	SOC_ENUM("HPF Cutoff", hpf_v2_cutoff_enum),
	SOC_SINGLE("HPFL Switch", PDM_V2_FILTER_CTRL, 20, 1, 0),
	SOC_SINGLE("HPFR Switch", PDM_V2_FILTER_CTRL, 19, 1, 0),
};

static const struct snd_kcontrol_new rk3576_controls[] = {
	SOC_SINGLE_RANGE_TLV("Gain Volume",
			     PDM_V2_FILTER_CTRL,
			     PDM_V2_GAIN_SHIFT,
			     PDM_V2_GAIN_MIN,
			     PDM_V2_GAIN_MAX,
			     0, pdm_v2_digtal_gain_tlv),
	SOC_ENUM("HPF Cutoff", hpf_cutoff_enum),
	SOC_SINGLE("HPFL Switch", PDM_V2_FILTER_CTRL, 22, 1, 0),
	SOC_SINGLE("HPFR Switch", PDM_V2_FILTER_CTRL, 21, 1, 0),
};

static const struct snd_soc_component_driver rockchip_pdm_v2_component = {
	.name = "rockchip-pdm-v2",
	.controls = rockchip_pdm_v2_controls,
	.num_controls = ARRAY_SIZE(rockchip_pdm_v2_controls),
	.legacy_dai_naming = 1,
};

static int rockchip_pdm_v2_pinctrl_select_clk_state(struct device *dev)
{
	struct rk_pdm_v2_dev *pdm = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(pdm->pinctrl) || !pdm->clk_state)
		return 0;
	/*
	 * A necessary delay to make sure the correct
	 * frac div has been applied when resume from
	 * power down.
	 */
	udelay(10);

	/*
	 * Must disable the clk to avoid clk glitch
	 * when pinctrl switch from gpio to pdm clk.
	 */

	rockchip_utils_clk_gate_endisable(pdm->dev, pdm->clk_out, 0);
	udelay(10);
	pinctrl_select_state(pdm->pinctrl, pdm->clk_state);
	udelay(10);
	rockchip_utils_clk_gate_endisable(pdm->dev, pdm->clk_out, 1);

	return 0;
}

static int rockchip_pdm_v2_runtime_suspend(struct device *dev)
{
	struct rk_pdm_v2_dev *pdm = dev_get_drvdata(dev);

	regcache_cache_only(pdm->regmap, true);
	clk_disable_unprepare(pdm->clk);
	clk_disable_unprepare(pdm->hclk);
	clk_disable_unprepare(pdm->clk_out);

	pinctrl_pm_select_idle_state(dev);

	return 0;
}

static int rockchip_pdm_v2_runtime_resume(struct device *dev)
{
	struct rk_pdm_v2_dev *pdm = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(pdm->clk_out);
	if (ret)
		goto err_clk_out;

	ret = clk_prepare_enable(pdm->clk);
	if (ret)
		goto err_clk;

	ret = clk_prepare_enable(pdm->hclk);
	if (ret)
		goto err_hclk;

	regcache_cache_only(pdm->regmap, false);
	regcache_mark_dirty(pdm->regmap);
	ret = regcache_sync(pdm->regmap);
	if (ret)
		goto err_regmap;

	rockchip_pdm_v2_rxctrl(pdm, 0);

	rockchip_pdm_v2_pinctrl_select_clk_state(dev);

	return 0;

err_regmap:
	clk_disable_unprepare(pdm->hclk);
err_hclk:
	clk_disable_unprepare(pdm->clk);
err_clk:
	clk_disable_unprepare(pdm->clk_out);
err_clk_out:
	return ret;
}

static bool rockchip_pdm_v2_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PDM_V2_SYSCONFIG:
	case PDM_V2_CTRL:
	case PDM_V2_FILTER_CTRL:
	case PDM_V2_FIFO_CTRL:
	case PDM_V2_RXFIFO_DATA:
	case PDM_V2_DATA_VALID:
	case PDM_V2_GAIN_CTRL:
		return true;
	default:
		return false;
	}
}

static bool rockchip_pdm_v2_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PDM_V2_SYSCONFIG:
	case PDM_V2_CTRL:
	case PDM_V2_FILTER_CTRL:
	case PDM_V2_FIFO_CTRL:
	case PDM_V2_DATA_VALID:
	case PDM_V2_RXFIFO_DATA:
	case PDM_V2_VERSION:
	case PDM_V2_GAIN_CTRL:
		return true;
	default:
		return false;
	}
}

static bool rockchip_pdm_v2_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PDM_V2_FIFO_CTRL:
	case PDM_V2_RXFIFO_DATA:
	case PDM_V2_VERSION:
		return true;
	default:
		return false;
	}
}

static bool rockchip_pdm_v2_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case PDM_V2_RXFIFO_DATA:
		return true;
	default:
		return false;
	}
}

static const struct reg_default rockchip_pdm_v2_reg_defaults[] = {
	{ PDM_V2_SYSCONFIG, 0x00000002 },
	{ PDM_V2_CTRL, 0x001C8797 },
	{ PDM_V2_FIFO_CTRL, 0x0003E000 },
};

static const struct regmap_config rockchip_pdm_v2_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = PDM_V2_GAIN_CTRL,
	.reg_defaults = rockchip_pdm_v2_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(rockchip_pdm_v2_reg_defaults),
	.writeable_reg = rockchip_pdm_v2_wr_reg,
	.readable_reg = rockchip_pdm_v2_rd_reg,
	.volatile_reg = rockchip_pdm_v2_volatile_reg,
	.precious_reg = rockchip_pdm_v2_precious_reg,
	.cache_type = REGCACHE_FLAT,
};

static const struct of_device_id rockchip_pdm_v2_match[] __maybe_unused = {
	{ .compatible = "rockchip,rk3506-pdm", },
	{ .compatible = "rockchip,rk3576-pdm", },
	{},
};
MODULE_DEVICE_TABLE(of, rockchip_pdm_v2_match);

static int rockchip_pdm_v2_path_parse(struct rk_pdm_v2_dev *pdm, struct device_node *node)
{
	unsigned int path[PDM_V2_PATH_MAX];
	int cnt = 0, ret = 0, i = 0, val = 0, msk = 0;

	cnt = of_count_phandle_with_args(node, "rockchip,path-map",
					 NULL);
	if (cnt != PDM_V2_PATH_MAX)
		return cnt;

	ret = of_property_read_u32_array(node, "rockchip,path-map",
					 path, cnt);
	if (ret)
		return ret;

	for (i = 0; i < cnt; i++) {
		if (path[i] >= PDM_V2_PATH_MAX)
			return -EINVAL;
		msk |= PDM_V2_RX_PATH_SEL_MASK(i);
		val |= PDM_V2_RX_PATH_SEL(i, path[i]);
	}

	regmap_update_bits(pdm->regmap, PDM_V2_CTRL, msk, val);

	return 0;
}

static int rockchip_pdm_v2_keep_clk_always_on(struct rk_pdm_v2_dev *pdm)
{
	unsigned int upsamplerate, mclk;
	int index, ret = 0;

	index = get_pdm_v2_clkref(pdm, PDM_V2_DEFAULT_RATE);
	if (index < 0)
		return -EINVAL;

	mclk = clkref[index].clk;
	upsamplerate = clkref[index].clk_out;

	ret = clk_prepare_enable(pdm->clk_out);
	if (ret)
		goto err_prepare_clk_out;

	ret = clk_prepare_enable(pdm->clk);
	if (ret)
		goto err_clk_out;

	pm_runtime_forbid(pdm->dev);

	dev_info(pdm->dev, "CLK-ALWAYS-ON: mclk: %d, upsamplerate: %d, samplerate: %d\n",
		 mclk, upsamplerate, PDM_V2_DEFAULT_RATE);

	return 0;

err_clk_out:
	clk_disable_unprepare(pdm->clk_out);
err_prepare_clk_out:
	return ret;
}

static int rockchip_pdm_v2_parse_quirks(struct rk_pdm_v2_dev *pdm)
{
	int ret = 0, i = 0;

	for (i = 0; i < ARRAY_SIZE(of_quirks); i++)
		if (device_property_read_bool(pdm->dev, of_quirks[i].quirk))
			pdm->quirks |= of_quirks[i].id;

	if (pdm->quirks & QUIRK_ALWAYS_ON)
		ret = rockchip_pdm_v2_keep_clk_always_on(pdm);

	return ret;
}

static void rockchip_pdm_v2_quirks_close_clk(struct rk_pdm_v2_dev *pdm)
{
	if (pdm->quirks & QUIRK_ALWAYS_ON) {
		clk_disable_unprepare(pdm->clk_out);
		clk_disable_unprepare(pdm->clk);
	}
}

static int rockchip_pdm_v2_register_platform(struct device *dev)
{
	int ret = 0;

	if (device_property_read_bool(dev, "rockchip,no-dmaengine")) {
		dev_info(dev, "Used for Multi-DAI\n");
		return 0;
	}

	ret = devm_snd_dmaengine_pcm_register(dev, NULL, 0);
	if (ret)
		dev_err(dev, "Could not register PCM\n");

	return ret;
}

static int rockchip_pdm_v2_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct rk_pdm_v2_dev *pdm;
	struct resource *res;
	void __iomem *regs;
	int ret;

	pdm = devm_kzalloc(&pdev->dev, sizeof(*pdm), GFP_KERNEL);
	if (!pdm)
		return -ENOMEM;

	regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	pdm->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &rockchip_pdm_v2_regmap_config);
	if (IS_ERR(pdm->regmap))
		return PTR_ERR(pdm->regmap);

	pdm->capture_dma_data.addr = res->start + PDM_V2_RXFIFO_DATA;
	pdm->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	pdm->capture_dma_data.maxburst = PDM_V2_DMA_BURST_SIZE;

	pdm->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, pdm);

	pdm->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR_OR_NULL(pdm->pinctrl)) {
		pdm->clk_state = pinctrl_lookup_state(pdm->pinctrl, "clk");
		if (IS_ERR(pdm->clk_state)) {
			pdm->clk_state = NULL;
			dev_warn(pdm->dev, "Have no clk pinctrl state\n");
		}
	}

	pdm->start_delay_ms = PDM_V2_START_DELAY_MS_DEFAULT;

	pdm->clk = devm_clk_get(&pdev->dev, "pdm_clk");
	if (IS_ERR(pdm->clk))
		return PTR_ERR(pdm->clk);

	pdm->hclk = devm_clk_get(&pdev->dev, "pdm_hclk");
	if (IS_ERR(pdm->hclk))
		return PTR_ERR(pdm->hclk);

	pdm->clk_out = devm_clk_get(&pdev->dev, "pdm_clk_out");
	if (IS_ERR(pdm->clk_out))
		return PTR_ERR(pdm->clk_out);

	ret = clk_prepare_enable(pdm->hclk);
	if (ret)
		return ret;

	rockchip_pdm_v2_set_samplerate(pdm, PDM_V2_DEFAULT_RATE);
	rockchip_pdm_v2_rxctrl(pdm, 0);
	regmap_read(pdm->regmap, PDM_V2_VERSION, &pdm->version);
	/*
	 * The pdm version rule:
	 * Low 16bit is soc number.
	 * High 16bit is PDM release time.
	 * The Only soc number is changed with every chips. So use the
	 * release time here.
	 */
	pdm->version = (pdm->version >> 16) & 0xffff;

	if (pdm->version == RK3506_PDM) {
		regmap_update_bits(pdm->regmap, PDM_V2_GAIN_CTRL, PDM_V2_GAIN_CTRL_MSK,
				   PDM_V2_GAIN_CTRL_0DB);
	} else if (pdm->version == RK3576_PDM) {
		regmap_update_bits(pdm->regmap, PDM_V2_FILTER_CTRL, PDM_V2_GAIN_MSK,
				   PDM_V2_GAIN_0DB);
	}

	ret = rockchip_pdm_v2_path_parse(pdm, node);
	if (ret != 0 && ret != -ENOENT)
		goto err_hclk;

	ret = rockchip_pdm_v2_parse_quirks(pdm);
	if (ret)
		goto err_hclk;
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
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = rockchip_pdm_v2_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	/* API snd_soc_register_component will try to rebind card per
	 * each component register. And the ASoC allow no-pcm card instance.
	 * devm_snd_soc_register_component
	 * snd_soc_try_rebind_card
	 *     snd_soc_bind_card
	 *         snd_soc_add_pcm_runtime
	 * devm_snd_dmaengine_pcm_register
	 * So, we should register PCM before DAI component.
	 */
	ret = rockchip_pdm_v2_register_platform(&pdev->dev);
	if (ret)
		goto err_suspend;

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &rockchip_pdm_v2_component,
					      &rockchip_pdm_v2_dai, 1);

	if (ret) {
		dev_err(&pdev->dev, "could not register dai: %d\n", ret);
		goto err_suspend;
	}

	clk_disable_unprepare(pdm->hclk);

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		rockchip_pdm_v2_runtime_suspend(&pdev->dev);
err_pm_disable:
	rockchip_pdm_v2_quirks_close_clk(pdm);
	pm_runtime_disable(&pdev->dev);
err_hclk:
	clk_disable_unprepare(pdm->hclk);

	return ret;
}

static int rockchip_pdm_v2_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		rockchip_pdm_v2_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops rockchip_pdm_v2_pm_ops = {
	SET_RUNTIME_PM_OPS(rockchip_pdm_v2_runtime_suspend,
			   rockchip_pdm_v2_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
};

static struct platform_driver rockchip_pdm_v2_driver = {
	.probe  = rockchip_pdm_v2_probe,
	.remove = rockchip_pdm_v2_remove,
	.driver = {
		.name = "rockchip-pdm-v2",
		.of_match_table = of_match_ptr(rockchip_pdm_v2_match),
		.pm = &rockchip_pdm_v2_pm_ops,
	},
};

module_platform_driver(rockchip_pdm_v2_driver);

MODULE_AUTHOR("Jason Zhu<jason.zhu@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip PDM V2 Controller Driver");
MODULE_LICENSE("GPL");
