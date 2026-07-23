// SPDX-License-Identifier: GPL-2.0
/*
 * rk2116.c  --  RK2116 ALSA SoC Audio Core driver
 *
 */

// #define DEBUG 1
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include "rk2116.h"

#define RK2116_RATES (SNDRV_PCM_RATE_192000 | \
		SNDRV_PCM_RATE_96000 | \
		SNDRV_PCM_RATE_88200 | \
		SNDRV_PCM_RATE_8000_48000)
#define RK2116_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_S18_3LE | \
		SNDRV_PCM_FMTBIT_S20_3LE | \
		SNDRV_PCM_FMTBIT_S24_LE | \
		SNDRV_PCM_FMTBIT_S32_LE)

struct rk2116_priv {
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *spk_ctl_gpio;
};

const struct regmap_config rk2116_regmap_config = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.max_raw_read = 128,
	.max_raw_write = 128,
};
EXPORT_SYMBOL_GPL(rk2116_regmap_config);

static int rk2116_component_probe(struct snd_soc_component *component)
{
	u32 val[1] = {0x00000000};
	struct rk2116_priv *rk2116 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s() disable hw through mode\n", __func__);
	regmap_noinc_write(rk2116->regmap, RK2116_REG_OTMD, val, sizeof(val));
	return 0;
}

static void rk2116_component_remove(struct snd_soc_component *component)
{
	u32 val[1] = {0x00000000};
	struct rk2116_priv *rk2116 = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s() enable hw through mode\n", __func__);
	regmap_noinc_write(rk2116->regmap, RK2116_REG_ITMD, val, sizeof(val));
}

static const struct snd_soc_component_driver rk2116_component_driver = {
	.probe		= rk2116_component_probe,
	.remove		= rk2116_component_remove,
	.endianness	= 1,
};

static struct snd_soc_dai_driver rk2116_dai = {
	.name = "rk2116-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 24,
		.rates = RK2116_RATES,
		.formats = RK2116_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 16,
		.rates = RK2116_RATES,
		.formats = RK2116_FORMATS,
	},
	.symmetric_rate = 1,
};

static int rk2116_bootup(struct rk2116_priv *rk2116)
{
	int ret;
	u32 val1[1] = {0x00000000}; /* placeholder */
	u32 val2[2] = {0x4950534F, 0x00004253}; /* boot up command(MSB) */

	/* boot up rk2116 */
	ret = regmap_noinc_write(rk2116->regmap, RK2116_REG_BOOT, val2, sizeof(val2));
	if (ret) {
		dev_err(rk2116->dev, "%s() boot up rk2116 failed, ret: %d\n", __func__, ret);
		return ret;
	}

	rk2116->spk_ctl_gpio = devm_gpiod_get_optional(rk2116->dev,
				"spk-ctl", GPIOD_OUT_LOW);
	if (IS_ERR(rk2116->spk_ctl_gpio))
		return PTR_ERR(rk2116->spk_ctl_gpio);

	/* enable TPA3110 */
	gpiod_set_value_cansleep(rk2116->spk_ctl_gpio, 1);

	/* disable hw through mode */
	ret = regmap_noinc_write(rk2116->regmap, RK2116_REG_OTMD, val1, sizeof(val1));
	if (ret) {
		dev_err(rk2116->dev,
			"%s() disable hw through mode failed, ret: %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

int rk2116_probe(struct device *dev, struct regmap *regmap)
{
	int ret;
	struct rk2116_priv *rk2116;

	dev_dbg(dev, "%s()\n", __func__);

	rk2116 = devm_kzalloc(dev, sizeof(*rk2116), GFP_KERNEL);
	if (!rk2116)
		return -ENOMEM;

	rk2116->regmap = regmap;
	rk2116->dev = dev;

	ret = rk2116_bootup(rk2116);
	if (ret) {
		dev_err(dev, "rk2116 boot failed, %d\n", ret);
		return ret;
	}

	dev_set_drvdata(dev, rk2116);
	ret = devm_snd_soc_register_component(dev,
			&rk2116_component_driver, &rk2116_dai, 1);

	return ret;
}
EXPORT_SYMBOL_GPL(rk2116_probe);

void rk2116_remove(struct device *dev)
{
	struct rk2116_priv *rk2116 = dev_get_drvdata(dev);

	dev_dbg(dev, "%s()\n", __func__);

	if (rk2116) {
		/* disable TPA3110 */
		gpiod_set_value_cansleep(rk2116->spk_ctl_gpio, 0);
	}
}
EXPORT_SYMBOL_GPL(rk2116_remove);

MODULE_DESCRIPTION("ASoC RK2116 driver");
MODULE_AUTHOR("Looper Zheng <looper.zheng@rockchip.com>");
MODULE_LICENSE("GPL");
