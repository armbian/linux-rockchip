// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC Audio Layer - Rockchip Dummy Controller driver
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/module.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>

#define DRV_NAME		"rockchip-dummy-dai"

struct rk_dummy_dai_dev {
	struct device *dev;
};

static struct snd_soc_dai_driver rockchip_dummy_dai_dai = {
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 512,
		.rates = SNDRV_PCM_RATE_8000_384000,
		.formats = SNDRV_PCM_FMTBIT_S8 |
			   SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S32_LE |
			   SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 512,
		.rates = SNDRV_PCM_RATE_8000_384000,
		.formats = SNDRV_PCM_FMTBIT_S8 |
			   SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S32_LE |
			   SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE,
	},
};

static const struct of_device_id rockchip_dummy_dai_match[] __maybe_unused = {
	{ .compatible = "rockchip,dummy-dai", },
	{},
};

static const struct snd_soc_component_driver rockchip_dummy_dai_component = {
	.name = DRV_NAME,
	.legacy_dai_naming = 1,
};

static int rockchip_dummy_dai_probe(struct platform_device *pdev)
{
	struct rk_dummy_dai_dev *dummy_dai;
	int ret;

	dummy_dai = devm_kzalloc(&pdev->dev, sizeof(*dummy_dai), GFP_KERNEL);
	if (!dummy_dai)
		return -ENOMEM;

	dummy_dai->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, dummy_dai);

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &rockchip_dummy_dai_component,
					      &rockchip_dummy_dai_dai, 1);
	return ret;
}

static struct platform_driver rockchip_dummy_dai_driver = {
	.probe = rockchip_dummy_dai_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(rockchip_dummy_dai_match),
	},
};
module_platform_driver(rockchip_dummy_dai_driver);

MODULE_DESCRIPTION("Rockchip Dummy DAI ASoC Interface");
MODULE_AUTHOR("Sugar Zhang <sugar.zhang@rock-chips.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, rockchip_dummy_dai_match);
