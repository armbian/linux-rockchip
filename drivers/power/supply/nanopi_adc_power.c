// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Simple ADC power supply driver
 *
 * Copyright (c) 2024 FriendlyElec Computer Tech. Co., Ltd.
 * (http://www.friendlyelec.com)
 *
 * based on ingenic-battery.c
 */

#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

struct nanopi_adc_power_priv {
	struct device *dev;
	struct iio_channel *iio_v;
	struct power_supply_desc desc;
	struct power_supply *psy;
	struct power_supply_battery_info *info;
	const struct nanopi_adc_platform_data *pdata;
	int online;
};

struct nanopi_adc_platform_data {
	int (*to_voltage)(int raw);
};

static int nanopi_adc_power_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	struct nanopi_adc_power_priv *priv = power_supply_get_drvdata(psy);
	struct power_supply_battery_info *info = priv->info;
	int raw = 0;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		ret = iio_read_channel_raw(priv->iio_v, &raw);
		val->intval = priv->pdata->to_voltage(raw);
		if (val->intval < info->voltage_min_design_uv)
			val->intval = POWER_SUPPLY_HEALTH_DEAD;
		else if (val->intval > info->voltage_max_design_uv)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		return ret < 0 ? ret : 0;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = priv->online;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = iio_read_channel_raw(priv->iio_v, &raw);
		val->intval = priv->pdata->to_voltage(raw);
		return ret < 0 ? ret : 0;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = info->voltage_min_design_uv;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = info->voltage_max_design_uv;
		return 0;
	default:
		return -EINVAL;
	}
}

static enum power_supply_property nanopi_adc_power_properties[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
};

static int nanopi_adc_to_voltage_v1(int raw)
{
	int mv = 5000;

	if (raw > 160 && raw < 1024) {
		mv = raw * 196 - 2130;
		mv /= 10;
	}

	return (mv * 1000);
}

static int nanopi_adc_to_voltage_v2(int raw)
{
	int mv = raw * 2475 / 512;

	return (mv * 1000);
}

static const struct nanopi_adc_platform_data nanopi_adc_pdata_v1 = {
	.to_voltage = nanopi_adc_to_voltage_v1,
};

static const struct nanopi_adc_platform_data nanopi_adc_pdata_v2 = {
	.to_voltage = nanopi_adc_to_voltage_v2,
};

static const struct of_device_id nanopi_adc_power_of_match[] = {
	{ .compatible = "nanopi-adc-power-v1", .data = &nanopi_adc_pdata_v1 },
	{ .compatible = "nanopi-adc-power-v2", .data = &nanopi_adc_pdata_v2 },
	{},
};
MODULE_DEVICE_TABLE(of, nanopi_adc_power_of_match);

static int nanopi_adc_power_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct nanopi_adc_power_priv *priv;
	struct power_supply_config psy_cfg = {};
	struct power_supply_desc *desc;
	int ret;

	match = of_match_device(nanopi_adc_power_of_match, dev);
	if (!match || !match->data)
		return -ENODEV;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->online = 1;
	priv->pdata = match->data;

	priv->iio_v = devm_iio_channel_get(dev, "voltage");
	if (IS_ERR(priv->iio_v))
		return PTR_ERR(priv->iio_v);

	desc = &priv->desc;
	desc->name = dev_name(dev);
	desc->type = POWER_SUPPLY_TYPE_MAINS;
	desc->properties = nanopi_adc_power_properties;
	desc->num_properties = ARRAY_SIZE(nanopi_adc_power_properties);
	desc->get_property = nanopi_adc_power_get_property;

	psy_cfg.drv_data = priv;
	psy_cfg.of_node = dev->of_node;

	priv->psy = devm_power_supply_register(dev, desc, &psy_cfg);
	if (IS_ERR(priv->psy))
		return dev_err_probe(dev, PTR_ERR(priv->psy),
				     "Unable to register supply\n");

	ret = power_supply_get_battery_info(priv->psy, &priv->info);
	if (ret) {
		dev_err(dev, "Unable to get battery info: %d\n", ret);
		return ret;
	}
	if (priv->info->voltage_min_design_uv < 0) {
		dev_err(dev, "Unable to get voltage min design\n");
		return priv->info->voltage_min_design_uv;
	}
	if (priv->info->voltage_max_design_uv < 0) {
		dev_err(dev, "Unable to get voltage max design\n");
		return priv->info->voltage_max_design_uv;
	}

	return 0;
}

static struct platform_driver nanopi_adc_power_driver = {
	.driver = {
		.name = "nanopi-adc-power",
		.of_match_table = of_match_ptr(nanopi_adc_power_of_match),
	},
	.probe = nanopi_adc_power_probe,
};
module_platform_driver(nanopi_adc_power_driver);

MODULE_ALIAS("platform:nanopi-adc-power");
MODULE_AUTHOR("support@friendlyarm.com");
MODULE_DESCRIPTION("Simple ADC power supply driver for NanoPi 5/6 series");
MODULE_LICENSE("GPL");