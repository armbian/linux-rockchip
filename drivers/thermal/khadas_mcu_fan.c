// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Khadas MCU Controlled FAN driver
 *
 * Copyright (C) 2020 BayLibre SAS
 * Author(s): Neil Armstrong <narmstrong@baylibre.com>
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mfd/khadas-mcu.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>

#define MAX_LEVEL 3
#define MAX_SPEED 0x64

struct khadas_mcu_fan_ctx {
	struct khadas_mcu *mcu;
	unsigned int level;

	unsigned int fan_max_level;
	unsigned int fan_register;
	unsigned int *fan_cooling_levels;

	struct thermal_cooling_device *cdev;
};

static int khadas_mcu_fan_set_level(struct khadas_mcu_fan_ctx *ctx,
				    unsigned int level)
{
	int ret;
	unsigned int write_level = level;

	if (level > ctx->fan_max_level)
		return -EINVAL;

	if (ctx->fan_cooling_levels != NULL) {
		write_level = ctx->fan_cooling_levels[level];

		if (write_level > MAX_SPEED)
			return -EINVAL;
	}

	ret = regmap_write(ctx->mcu->regmap, ctx->fan_register,
			   write_level);

	if (ret)
		return ret;

	ctx->level = level;

	return 0;
}

static int khadas_mcu_fan_get_max_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct khadas_mcu_fan_ctx *ctx = cdev->devdata;

	*state = ctx->fan_max_level;

	return 0;
}

static int khadas_mcu_fan_get_cur_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct khadas_mcu_fan_ctx *ctx = cdev->devdata;

	*state = ctx->level;

	return 0;
}

static int
khadas_mcu_fan_set_cur_state(struct thermal_cooling_device *cdev,
			     unsigned long state)
{
	struct khadas_mcu_fan_ctx *ctx = cdev->devdata;

	if (state > ctx->fan_max_level)
		return -EINVAL;

	if (state == ctx->level)
		return 0;

	return khadas_mcu_fan_set_level(ctx, state);
}

static const struct thermal_cooling_device_ops khadas_mcu_fan_cooling_ops = {
	.get_max_state = khadas_mcu_fan_get_max_state,
	.get_cur_state = khadas_mcu_fan_get_cur_state,
	.set_cur_state = khadas_mcu_fan_set_cur_state,
};

// Khadas Edge 2 sets fan level by passing fan speed(0-100). So we need different logic here like pwm-fan cooling-levels.
// This is optional and just necessary for Edge 2.
static int khadas_mcu_fan_get_cooling_data_edge2(struct khadas_mcu_fan_ctx *ctx, struct device *dev) {
	struct device_node *np = ctx->mcu->dev->of_node;
	int num, i, ret;

	if (!of_find_property(np, "cooling-levels", NULL))
		return 0;

	ret = of_property_count_u32_elems(np, "cooling-levels");
	if (ret <= 0) {
		dev_err(dev, "Wrong data!\n");
		return ret ? : -EINVAL;
	}

	num = ret;
	ctx->fan_cooling_levels = devm_kcalloc(dev, num, sizeof(u32),
						   GFP_KERNEL);
	if (!ctx->fan_cooling_levels)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "cooling-levels",
					 ctx->fan_cooling_levels, num);
	if (ret) {
		dev_err(dev, "Property 'cooling-levels' cannot be read!\n");
		return ret;
	}

	for (i = 0; i < num; i++) {
		if (ctx->fan_cooling_levels[i] > MAX_SPEED) {
			dev_err(dev, "PWM fan state[%d]:%d > %d\n", i,
				ctx->fan_cooling_levels[i], MAX_SPEED);
			return -EINVAL;
		}
	}

	ctx->fan_max_level = num - 1;
	ctx->fan_register = KHADAS_MCU_CMD_FAN_STATUS_CTRL_REG_V2;

	return 0;
}

static int khadas_mcu_fan_probe(struct platform_device *pdev)
{
	struct khadas_mcu *mcu = dev_get_drvdata(pdev->dev.parent);
	struct thermal_cooling_device *cdev;
	struct device *dev = &pdev->dev;
	struct khadas_mcu_fan_ctx *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	ctx->mcu = mcu;
	platform_set_drvdata(pdev, ctx);

	ctx->fan_max_level = MAX_LEVEL;
	ctx->fan_register = KHADAS_MCU_CMD_FAN_STATUS_CTRL_REG;

	ret = khadas_mcu_fan_get_cooling_data_edge2(ctx, dev);
	if (ret)
		return ret;

	cdev = devm_thermal_of_cooling_device_register(dev->parent,
			dev->parent->of_node, "khadas-mcu-fan", ctx,
			&khadas_mcu_fan_cooling_ops);
	if (IS_ERR(cdev)) {
		ret = PTR_ERR(cdev);
		dev_err(dev, "Failed to register khadas-mcu-fan as cooling device: %d\n",
			ret);
		return ret;
	}
	ctx->cdev = cdev;

	return 0;
}

static void khadas_mcu_fan_shutdown(struct platform_device *pdev)
{
	struct khadas_mcu_fan_ctx *ctx = platform_get_drvdata(pdev);

	khadas_mcu_fan_set_level(ctx, 0);
}

#ifdef CONFIG_PM_SLEEP
static int khadas_mcu_fan_suspend(struct device *dev)
{
	struct khadas_mcu_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned int level_save = ctx->level;
	int ret;

	ret = khadas_mcu_fan_set_level(ctx, 0);
	if (ret)
		return ret;

	ctx->level = level_save;

	return 0;
}

static int khadas_mcu_fan_resume(struct device *dev)
{
	struct khadas_mcu_fan_ctx *ctx = dev_get_drvdata(dev);

	return khadas_mcu_fan_set_level(ctx, ctx->level);
}
#endif

static SIMPLE_DEV_PM_OPS(khadas_mcu_fan_pm, khadas_mcu_fan_suspend,
			 khadas_mcu_fan_resume);

static const struct platform_device_id khadas_mcu_fan_id_table[] = {
	{ .name = "khadas-mcu-fan-ctrl", },
	{},
};
MODULE_DEVICE_TABLE(platform, khadas_mcu_fan_id_table);

static struct platform_driver khadas_mcu_fan_driver = {
	.probe		= khadas_mcu_fan_probe,
	.shutdown	= khadas_mcu_fan_shutdown,
	.driver	= {
		.name		= "khadas-mcu-fan-ctrl",
		.pm		= &khadas_mcu_fan_pm,
	},
	.id_table	= khadas_mcu_fan_id_table,
};

module_platform_driver(khadas_mcu_fan_driver);

MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_DESCRIPTION("Khadas MCU FAN driver");
MODULE_LICENSE("GPL");
