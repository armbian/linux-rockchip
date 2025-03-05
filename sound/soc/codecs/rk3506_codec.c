// SPDX-License-Identifier: GPL-2.0+
/*
 * rk3506_codec.c - Rockchip RK3506 SoC Codec Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "rk3506_codec.h"

#define CODEC_DRV_NAME			"rk3506-acodec"

#define MCLK_REFERENCE_8000		32768000
#define MCLK_REFERENCE_11025		45158400
#define MCLK_REFERENCE_12000		49152000
#define MCLK_I2S_REFERENCE_DIV		4
#define I2S_MCLK_FS			64

struct rk3506_codec_priv {
	const struct device *plat_dev;
	struct reset_control *reset;
	struct regmap *regmap;
	struct clk *pclk;
	struct clk *mclk;
	struct snd_soc_component *component;
};

static unsigned int samplerate_to_bit(unsigned int samplerate)
{
	switch (samplerate) {
	case 8000:
	case 11025:
	case 12000:
		return 0;
	case 16000:
	case 22050:
	case 24000:
		return 1;
	case 32000:
	case 44100:
	case 48000:
		return 2;
	case 64000:
	case 88200:
	case 96000:
		return 3;
	case 128000:
	case 176400:
	case 192000:
		return 4;
	default:
		return 2;
	}
}

static void rk3506_codec_power_on(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AUDIO_ADC_LDO,
				      ADC_IP_MSK, ADC_IP_EN);
	snd_soc_component_update_bits(component, AUDIO_ADC_PGA0,
				      PGA_PWD_MSK, PGA_PWD_EN);
	snd_soc_component_update_bits(component, AUDIO_ADC_ADC0,
				      ADC_PWD_MSK | ADC_DEM_CTRL, ADC_PWD_EN | ADC_DEM_DWA);
	udelay(10);
}

static void rk3506_codec_power_off(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AUDIO_ADC_PGA0,
				      PGA_PWD_MSK, PGA_PWD_DIS);
	snd_soc_component_update_bits(component, AUDIO_ADC_ADC0,
				      ADC_PWD_MSK, ADC_PWD_DIS);
	snd_soc_component_update_bits(component, AUDIO_ADC_LDO,
				      ADC_IP_MSK, ADC_IP_DIS);
}

static void rk3506_codec_adc_enable(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AUDIO_ADC_DIGEN_CLKE,
				      ADC_MSK | ADC_CKE_MSK,
				      ADC_EN | ADC_CKE_EN);
	udelay(10);
}

static void rk3506_codec_adc_disable(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AUDIO_ADC_DIGEN_CLKE,
				      ADC_MSK | ADC_CKE_MSK,
				      ADC_DIS | ADC_CKE_DIS);
}

static void rk3506_codec_tx_start(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AUDIO_ADC_I2S_TXCR2_TXCMD,
				      TXC_MSK | TXS_MSK,
				      TXC_DIS | TXS_START);
	snd_soc_component_update_bits(component, AUDIO_ADC_DIGEN_CLKE,
				      I2STX_MSK | I2STX_CKE_MSK,
				      I2STX_EN | I2STX_CKE_EN);
}

static void rk3506_codec_tx_stop(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AUDIO_ADC_I2S_TXCR2_TXCMD,
				      TXC_MSK | TXS_MSK,
				      TXC_EN | TXS_STOP);
	snd_soc_component_update_bits(component, AUDIO_ADC_DIGEN_CLKE,
				      I2STX_MSK | I2STX_CKE_MSK,
				      I2STX_DIS | I2STX_CKE_DIS);
}

static void rk3506_codec_capture_on(struct snd_soc_component *component)
{
	rk3506_codec_adc_enable(component);
	rk3506_codec_tx_start(component);
	rk3506_codec_power_on(component);
}

static void rk3506_codec_capture_off(struct snd_soc_component *component)
{
	rk3506_codec_tx_stop(component);
	rk3506_codec_adc_disable(component);
	rk3506_codec_power_off(component);
}

static int rk3506_codec_reset(struct snd_soc_component *component)
{
	struct rk3506_codec_priv *rk3506 = snd_soc_component_get_drvdata(component);

	clk_prepare_enable(rk3506->pclk);
	/* Auto clear reset */
	snd_soc_component_update_bits(component, AUDIO_ADC_DIGEN_CLKE,
				      SRST_MSK, SRST_EN);
	udelay(10);

	/* Set parameters */
	snd_soc_component_update_bits(component, AUDIO_ADC_LDO,
				      LDO_VSEL_MSK, LDO_VSEL_1_65V);
	snd_soc_component_update_bits(component, AUDIO_ADC_PGA1,
				      PGA_CHOP_SEL_MSK,
				      PGA_CHOP_SEL_200K);
	snd_soc_component_update_bits(component, AUDIO_ADC_HK0,
				      HK_VAG_BUF_MSK | HK_ADC_BUF_MSK,
				      HK_VAG_BUF_ON | HK_ADC_BUF_ON);

	snd_soc_component_update_bits(component, AUDIO_ADC_ADC2,
				      ADC_CHOP_MSK | ADC_CAPTRIM_MSK,
				      ADC_CHOP_OFF | ADC_CAPTRIM_100_PCT);
	snd_soc_component_update_bits(component, AUDIO_ADC_AGC0,
				      ADC_BYPS_MSK | ADC_NG_MODE_MSK,
				      ADC_BYPS_EN | ADC_NG_MODE_EN);

	snd_soc_component_update_bits(component, AUDIO_ADC_HK1,
				      HK_VREF_1P2V_SEL_MSK,
				      HK_VREF_1P2V_SEL_N10M);
	snd_soc_component_update_bits(component, AUDIO_ADC_PGA2,
				      PGA_BUF_IB_SEL_MSK | PGA_BUF_CHOP_SEL_MSK,
				      PGA_BUF_IB_SEL_167_PCT | PGA_BUF_CHOP_SEL_400K);
	snd_soc_component_update_bits(component, AUDIO_ADC_PGA0,
				      PGA_INPUT_DEC_MSK | PGA_GAIN_MASK,
				      PGA_INPUT_DEC_N4_34DB | PGA_GAIN_3DB);

	clk_disable_unprepare(rk3506->pclk);

	return 0;
}

static int rk3506_set_dai_fmt(struct snd_soc_dai *codec_dai,
			      unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	int val = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		val = I2S_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		val = I2S_MASTER;
		break;
	default:
		return -EINVAL;
	}

	/* I2S mode, MSB */
	snd_soc_component_update_bits(component, AUDIO_ADC_I2S_CKM,
				      I2S_MST_MSK | SCK_MSK,
				      val | SCK_EN);

	return 0;
}

static int rk3506_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct rk3506_codec_priv *rk3506 = snd_soc_component_get_drvdata(component);
	unsigned int width, rate;
	int ratio;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return 0;

	if ((params_rate(params) % 12000) == 0) {
		clk_set_rate(rk3506->mclk, MCLK_REFERENCE_12000);
		ratio = MCLK_REFERENCE_12000 / MCLK_I2S_REFERENCE_DIV /
			(I2S_MCLK_FS * params_rate(params));
		snd_soc_component_update_bits(component, AUDIO_ADC_I2S_CKM,
					      SCK_DIV_MSK, SCK_DIV(ratio));
	} else if ((params_rate(params) % 11025) == 0) {
		clk_set_rate(rk3506->mclk, MCLK_REFERENCE_11025);
		ratio = MCLK_REFERENCE_11025 / MCLK_I2S_REFERENCE_DIV /
			(I2S_MCLK_FS * params_rate(params));
		snd_soc_component_update_bits(component, AUDIO_ADC_I2S_CKM,
					      SCK_DIV_MSK, SCK_DIV(ratio));
	} else if ((params_rate(params) % 8000) == 0) {
		clk_set_rate(rk3506->mclk, MCLK_REFERENCE_8000);
		ratio = MCLK_REFERENCE_8000 / MCLK_I2S_REFERENCE_DIV /
			(I2S_MCLK_FS * params_rate(params));
		snd_soc_component_update_bits(component, AUDIO_ADC_I2S_CKM,
					      SCK_DIV_MSK, SCK_DIV(ratio));
	}

	udelay(10);
	switch (params_rate(params)) {
	case 8000:
	case 11025:
	case 12000:
	case 64000:
	case 88200:
	case 96000:
	case 128000:
	case 176400:
	case 192000:
		snd_soc_component_update_bits(component, AUDIO_ADC_FILTER,
					      AUDIO_ADC_FILTER_MSK,
					      AUDIO_ADC_FILTER_MODE1);
		break;
	case 16000:
	case 24000:
	case 22050:
		snd_soc_component_update_bits(component, AUDIO_ADC_FILTER,
					      AUDIO_ADC_FILTER_MSK,
					      AUDIO_ADC_FILTER_MODE3);
		break;
	case 32000:
	case 44100:
	case 48000:
		snd_soc_component_update_bits(component, AUDIO_ADC_FILTER,
					      AUDIO_ADC_FILTER_MSK,
					      AUDIO_ADC_FILTER_MODE2);
		break;
	default:
		snd_soc_component_update_bits(component, AUDIO_ADC_FILTER,
					      AUDIO_ADC_FILTER_MSK,
					      AUDIO_ADC_FILTER_MODE2);
	}

	width = min(params_width(params), 24);
	rate = samplerate_to_bit(params_rate(params));
	snd_soc_component_update_bits(component, AUDIO_ADC_I2S_TSD,
				      VDW_TX_MSK, VDW_TX(width));
	snd_soc_component_update_bits(component, AUDIO_ADC_DIGEN_CLKE,
				      ADCSRT_MSK, ADCSRT(rate));
	rk3506_codec_capture_on(component);

	return 0;
}

static void rk3506_pcm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct rk3506_codec_priv *rk3506 = snd_soc_component_get_drvdata(component);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return;

	rk3506_codec_capture_off(component);
	regcache_cache_only(rk3506->regmap, false);
	regcache_sync(rk3506->regmap);
}

static const struct snd_soc_dai_ops rk3506_dai_ops = {
	.hw_params = rk3506_hw_params,
	.set_fmt = rk3506_set_dai_fmt,
	.shutdown = rk3506_pcm_shutdown,
	.no_capture_mute = 1,
};

static struct snd_soc_dai_driver rk3506_dai[] = {
	{
		.name = "rk3506-hifi",
		.id = ACODEC_HIFI,
		.capture = {
			.stream_name = "HiFi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_S24_LE |
				    SNDRV_PCM_FMTBIT_S32_LE),
		},
		.ops = &rk3506_dai_ops,
	},
};

static int rk3506_codec_probe(struct snd_soc_component *component)
{
	struct rk3506_codec_priv *rk3506 = snd_soc_component_get_drvdata(component);

	rk3506->component = component;
	rk3506_codec_reset(component);
	regcache_cache_only(rk3506->regmap, false);
	regcache_sync(rk3506->regmap);

	return 0;
}

static void rk3506_codec_remove(struct snd_soc_component *component)
{
	struct rk3506_codec_priv *rk3506 = snd_soc_component_get_drvdata(component);

	regcache_cache_only(rk3506->regmap, false);
	regcache_sync(rk3506->regmap);
}

static int rk3506_codec_suspend(struct snd_soc_component *component)
{
	struct rk3506_codec_priv *rk3506 = snd_soc_component_get_drvdata(component);

	regcache_cache_only(rk3506->regmap, true);
	clk_disable_unprepare(rk3506->mclk);
	clk_disable_unprepare(rk3506->pclk);

	return 0;
}

static int rk3506_codec_resume(struct snd_soc_component *component)
{
	struct rk3506_codec_priv *rk3506 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ret = clk_prepare_enable(rk3506->pclk);
	if (ret < 0) {
		dev_err(rk3506->plat_dev,
			"Failed to enable acodec pclk: %d\n", ret);
		goto pclk_error;
	}

	ret = clk_prepare_enable(rk3506->mclk);
	if (ret < 0) {
		dev_err(rk3506->plat_dev,
			"Failed to enable acodec mclk: %d\n", ret);
		goto mclk_error;
	}

	regcache_cache_only(rk3506->regmap, false);
	ret = regcache_sync(rk3506->regmap);
	if (ret)
		goto reg_error;

	return 0;
reg_error:
	clk_disable_unprepare(rk3506->mclk);
mclk_error:
	clk_disable_unprepare(rk3506->pclk);
pclk_error:
	return ret;
}

static const DECLARE_TLV_DB_SCALE(adc_dig_gain_tlv, -9500, 75, 0);
static const DECLARE_TLV_DB_SCALE(adc_pga_gain_tlv, 0, 300, 0);

static const char * const adc_hpf_cutoff_text[] = {
	"3.79Hz", "60Hz", "243Hz", "493Hz",
};

static SOC_ENUM_SINGLE_DECL(adc_hpf_cutoff_enum, AUDIO_ADC_FILTER,
			    6, adc_hpf_cutoff_text);

static const struct snd_kcontrol_new rk3506_codec_dapm_controls[] = {
	SOC_ENUM("HPF Cutoff", adc_hpf_cutoff_enum),
	SOC_SINGLE("HPF Switch", AUDIO_ADC_FILTER, 4, 1, 0),
	SOC_SINGLE_RANGE_TLV("Digital Gain Volume",
			     AUDIO_ADC_VOLL,
			     ADCLV_SHIFT,
			     ADCLV_MIN,
			     ADCLV_MAX,
			     0, adc_dig_gain_tlv),
	SOC_SINGLE_RANGE_TLV("PGA Gain Volume",
			     AUDIO_ADC_PGA0,
			     PGA_GAIN_SHIFT,
			     PGA_GAIN_MIN,
			     PGA_GAIN_MAX,
			     0, adc_pga_gain_tlv),
	SOC_SINGLE("ADC Switch", AUDIO_ADC_AGC0, 0, 1, 0),
};

static const struct snd_soc_component_driver soc_codec_dev_rk3506 = {
	.probe = rk3506_codec_probe,
	.remove = rk3506_codec_remove,
	.suspend = rk3506_codec_suspend,
	.resume = rk3506_codec_resume,
	.controls = rk3506_codec_dapm_controls,
	.num_controls = ARRAY_SIZE(rk3506_codec_dapm_controls),
};

/* Set the default value or reset value */
static const struct reg_default rk3506_codec_reg_defaults[] = {
	{ AUDIO_ADC_BG_LPF0, 0x3 },
};

static bool rk3506_codec_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AUDIO_ADC_BG_LPF0 ... AUDIO_ADC_AGC8:
		return true;
	case AUDIO_ADC_FILTER ... AUDIO_ADC_I2S_TXCR2_TXCMD:
		return true;
	default:
		return false;
	}

	return true;
}

static bool rk3506_codec_readable_reg(struct device *dev, unsigned int reg)
{
	return reg >= AUDIO_ADC_BG_LPF0;
}

static bool rk3506_codec_volatile_reg(struct device *dev, unsigned int reg)
{
	return reg >= AUDIO_ADC_BG_LPF0;
}

static const struct regmap_config rk3506_codec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = ACODEC_REG_MAX,
	.writeable_reg = rk3506_codec_writeable_reg,
	.readable_reg = rk3506_codec_readable_reg,
	.volatile_reg = rk3506_codec_volatile_reg,
	.reg_defaults = rk3506_codec_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(rk3506_codec_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct of_device_id rk3506_codec_of_match[] = {
	{ .compatible = "rockchip,rk3506-codec", },
	{},
};

MODULE_DEVICE_TABLE(of, rk3506_codec_of_match);

static int rk3506_platform_probe(struct platform_device *pdev)
{
	struct rk3506_codec_priv *rk3506;
	struct resource *res;
	void __iomem *base;
	int ret;

	rk3506 = devm_kzalloc(&pdev->dev, sizeof(*rk3506), GFP_KERNEL);
	if (!rk3506)
		return -ENOMEM;

	rk3506->plat_dev = &pdev->dev;
	rk3506->reset = devm_reset_control_get_optional_exclusive(&pdev->dev, "acodec");
	if (IS_ERR(rk3506->reset))
		return PTR_ERR(rk3506->reset);

	rk3506->pclk = devm_clk_get(&pdev->dev, "pclk");
	if (IS_ERR(rk3506->pclk)) {
		dev_err(&pdev->dev, "Can't get acodec pclk\n");
		return -EINVAL;
	}

	rk3506->mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(rk3506->mclk)) {
		dev_err(&pdev->dev, "Can't get acodec mclk\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(rk3506->pclk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable acodec pclk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(rk3506->mclk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable acodec mclk: %d\n", ret);
		goto failed_1;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		ret = PTR_ERR(base);
		dev_err(&pdev->dev, "Failed to ioremap resource\n");
		goto failed;
	}

	rk3506->regmap = devm_regmap_init_mmio(&pdev->dev, base,
					       &rk3506_codec_regmap_config);
	if (IS_ERR(rk3506->regmap)) {
		ret = PTR_ERR(rk3506->regmap);
		dev_err(&pdev->dev, "Failed to regmap mmio\n");
		goto failed;
	}

	platform_set_drvdata(pdev, rk3506);
	ret = devm_snd_soc_register_component(&pdev->dev, &soc_codec_dev_rk3506,
					      rk3506_dai, ARRAY_SIZE(rk3506_dai));
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register codec: %d\n", ret);
		goto failed;
	}

	return ret;

failed:
	clk_disable_unprepare(rk3506->mclk);
failed_1:
	clk_disable_unprepare(rk3506->pclk);

	return ret;
}

static int rk3506_platform_remove(struct platform_device *pdev)
{
	struct rk3506_codec_priv *rk3506 =
		(struct rk3506_codec_priv *)platform_get_drvdata(pdev);

	clk_disable_unprepare(rk3506->mclk);
	clk_disable_unprepare(rk3506->pclk);

	return 0;
}

static struct platform_driver rk3506_codec_driver = {
	.driver = {
		   .name = CODEC_DRV_NAME,
		   .of_match_table = of_match_ptr(rk3506_codec_of_match),
	},
	.probe = rk3506_platform_probe,
	.remove = rk3506_platform_remove,
};
module_platform_driver(rk3506_codec_driver);

MODULE_DESCRIPTION("ASoC RK3506 Codec Driver");
MODULE_AUTHOR("Jason Zhu <jason.zhu@rock-chips.com>");
MODULE_LICENSE("GPL");
