// SPDX-License-Identifier: GPL-2.0-only
/*
 *  MCU I2C Port Expander I/O
 *
 *  Copyright (C) 2023 Cody Xie <cody.xie@rock-chips.com>
 *
 */

#include <linux/compiler_types.h>
#include <linux/bitfield.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/types.h>
#include "core.h"

#define MCU_REG_CONFIG_BASE 0x00
#define MCU_REG_CONFIG_PORT0  (MCU_REG_CONFIG_BASE + 0x0)
#define MCU_REG_CONFIG_PORT1  (MCU_REG_CONFIG_BASE + 0x1)
#define MCU_REG_CONFIG_PORT31 (MCU_REG_CONFIG_BASE + 0x1f)

#define MCU_REG_DIRECTION_BASE (MCU_REG_CONFIG_PORT31 + 0x1)
#define MCU_REG_DIRECTION_END (MCU_REG_DIRECTION_BASE + 0x20)


static struct mcu_gpio_chip *g_gpio_mcu_chip;

static int mcu_gpio_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	struct mcu_gpio_chip *priv = gpiochip_get_data(gc);
	unsigned int value;
	int ret;

	dev_info(gc->parent, "%s offset(%d)", __func__, offset);

	if ((!priv->regmap) || IS_ERR(priv->regmap)) {
		ret = -ENOMEM;
		dev_err(&priv->pdev->dev, "%s register map not ready: %d\n",
			__func__, ret);
		return ret;
	}

	ret = regmap_read(priv->regmap, MCU_REG_DIRECTION_BASE + offset, &value);
	if (ret < 0) {
		dev_err(gc->parent, "%s offset(%d) read config failed",
			__func__, offset);
		return ret;
	}

	return value;
}

static int mcu_gpio_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	struct mcu_gpio_chip *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(gc->parent, "%s offset(%d)", __func__, offset);

	if ((!priv->regmap) || IS_ERR(priv->regmap)) {
		ret = -ENOMEM;
		dev_err(&priv->pdev->dev, "%s register map not ready: %d\n",
			__func__, ret);
		return ret;
	}

	ret = regmap_write(priv->regmap, MCU_REG_DIRECTION_BASE + offset, GPIO_LINE_DIRECTION_IN);
	if (ret < 0) {
		dev_err(gc->parent, "%s offset(%d) read config failed",
			__func__, offset);
	}

	return ret;
}

static int mcu_gpio_direction_output(struct gpio_chip *gc, unsigned int offset, int val)
{
	struct mcu_gpio_chip *priv = gpiochip_get_data(gc);
	int ret;

	dev_dbg(gc->parent, "%s offset(%d) val(%d)", __func__, offset, val);

	if ((!priv->regmap) || IS_ERR(priv->regmap)) {
		ret = -ENOMEM;
		dev_err(&priv->pdev->dev, "%s register map not ready: %d\n",
			__func__, ret);
		return ret;
	}


	ret = regmap_write(priv->regmap, MCU_REG_DIRECTION_BASE + offset, GPIO_LINE_DIRECTION_OUT);
	if (ret < 0) {
		dev_err(gc->parent,
			"%s offset(%d) val(%d) update config failed", __func__,
			offset, val);
		return ret;
	}

	ret = regmap_write(priv->regmap, MCU_REG_CONFIG_BASE + offset, val);
	if (ret < 0) {
		dev_err(gc->parent,
			"%s offset(%d) val(%d) update output failed", __func__,
			offset, val);
		return ret;
	}

	return ret;
}

static int mcu_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	struct mcu_gpio_chip *priv = gpiochip_get_data(gc);
	unsigned int value;
	int ret;

	dev_info(gc->parent, "%s offset(%d)", __func__, offset);

	if ((!priv->regmap) || IS_ERR(priv->regmap)) {
		ret = -ENOMEM;
		dev_err(&priv->pdev->dev, "%s register map not ready: %d\n",
			__func__, ret);
		return ret;
	}

	ret = regmap_read(priv->regmap, MCU_REG_CONFIG_BASE + offset, &value);
	if (ret < 0) {
		dev_err(gc->parent, "%s offset(%d) check config failed",
			__func__, offset);
		return ret;
	}

	return value;
}

static void mcu_gpio_set(struct gpio_chip *gc, unsigned int offset, int val)
{
	struct mcu_gpio_chip *priv = gpiochip_get_data(gc);
	int ret;

	dev_info(gc->parent, "%s offset(%d) val(%d)", __func__, offset, val);

	if ((!priv->regmap) || IS_ERR(priv->regmap)) {
		dev_err(&priv->pdev->dev, "%s register map not ready\n",
			__func__);
		return;
	}

	ret = regmap_write(priv->regmap, MCU_REG_CONFIG_BASE + offset, val);
	if (ret < 0) {
		dev_err(gc->parent, "%s offset(%d) val(%d) set output failed",
			__func__, offset, val);
	}
}

static bool mcu_is_writeable_reg(struct device *dev, unsigned int reg)
{
	if ((reg <= MCU_REG_DIRECTION_END) && (reg >= MCU_REG_CONFIG_PORT0))
		return true;

	return false;
}

static bool mcu_is_readable_reg(struct device *dev, unsigned int reg)
{
	if ((reg <= MCU_REG_DIRECTION_END) && (reg >= MCU_REG_CONFIG_PORT0))
		return true;

	return false;
}

static bool mcu_is_volatile_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static const struct reg_default mcu_regmap_default[MCU_MAX_REGS] = {
	{ MCU_REG_CONFIG_PORT0, 0xFF },
	{ MCU_REG_CONFIG_PORT1, 0xFF },
	{ }
};

static const struct regmap_config mcu_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x3f,
	.writeable_reg = mcu_is_writeable_reg,
	.readable_reg = mcu_is_readable_reg,
	.volatile_reg = mcu_is_volatile_reg,
	.reg_defaults = mcu_regmap_default,
	.num_reg_defaults = ARRAY_SIZE(mcu_regmap_default),
	.cache_type = REGCACHE_FLAT,
};

static const struct gpio_chip template_chip = {
	.label = "mcu-gpio",
	.owner = THIS_MODULE,
	.get_direction = mcu_gpio_get_direction,
	.direction_input = mcu_gpio_direction_input,
	.direction_output = mcu_gpio_direction_output,
	.get = mcu_gpio_get,
	.set = mcu_gpio_set,
	.base = -1,
	.can_sleep = true,
};

#ifdef CONFIG_PM
static int mcu_suspend(struct device *dev)
{
	struct mcu_gpio_chip *priv = dev_get_drvdata(dev);
	int ret = 0;

	dev_info(dev, "%s: registers backup", __func__);

	if ((!priv->regmap) || IS_ERR(priv->regmap)) {
		ret = -ENOMEM;
		dev_err(dev, "%s register map not ready: %d\n", __func__, ret);
		return ret;
	}

	regcache_mark_dirty(priv->regmap);
	regcache_cache_only(priv->regmap, true);

	return 0;
}

static int mcu_resume(struct device *dev)
{
	struct mcu_gpio_chip *priv = dev_get_drvdata(dev);
	int ret = 0;

	dev_info(dev, "%s: registers recovery", __func__);

	if ((!priv->regmap) || IS_ERR(priv->regmap)) {
		ret = -ENOMEM;
		dev_err(dev, "%s register map not ready: %d\n", __func__, ret);
		return ret;
	}

	regcache_cache_only(priv->regmap, false);
	ret = regcache_sync(priv->regmap);
	if (ret != 0) {
		dev_err(dev, "Failed to restore register map: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops mcu_dev_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(mcu_suspend, mcu_resume)
};
#endif

int gpio_mcu_register(struct spi_device *spi)
{
	struct mcu_gpio_chip *chip = NULL;
	struct vehicle_spi *vehicle_spi;
	int ret;

	if (!g_gpio_mcu_chip) {
		g_gpio_mcu_chip = devm_kzalloc(&spi->dev, sizeof(struct mcu_gpio_chip),
					       GFP_KERNEL);
		if (!g_gpio_mcu_chip)
			return -ENOMEM;
	}

	chip = g_gpio_mcu_chip;
	vehicle_spi = spi_get_drvdata(spi);
	vehicle_spi->gpio_mcu = chip;

	chip->regmap = devm_regmap_init(&spi->dev, &vehicle_regmap_spi,
					&spi->dev, &mcu_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n", ret);
		goto err_exit;
	}

	pr_info("%s successfully\n", __func__);
	return 0;

err_exit:
	return ret;
}
EXPORT_SYMBOL_GPL(gpio_mcu_register);

static int gpio_mcu_probe(struct platform_device *pdev)
{
	struct mcu_gpio_chip *chip;
	struct device_node *np = pdev->dev.of_node;
	int ret;
	int i;

	if (!g_gpio_mcu_chip) {
		g_gpio_mcu_chip = devm_kzalloc(&pdev->dev, sizeof(struct mcu_gpio_chip),
					       GFP_KERNEL);
		if (!g_gpio_mcu_chip)
			return -ENOMEM;
	}

	chip = g_gpio_mcu_chip;
	chip->pdev = pdev;

	chip->name = "gpio-mcu";
	chip->gpio_chip = template_chip;
	chip->gpio_chip.label = "mcu-gpio";
	chip->gpio_chip.parent = &pdev->dev;
	chip->ngpio = (uintptr_t)of_device_get_match_data(&pdev->dev);
	chip->gpio_chip.ngpio = chip->ngpio;

	for (i = 0; i < MCU_MAX_REGS; i++)
		chip->backup_regs[i] = 0xff;

	if (np) {
		chip->reset_gpio_desc = devm_gpiod_get_optional(&pdev->dev, "reset", GPIOD_IN);
		if (IS_ERR_OR_NULL(chip->reset_gpio_desc))
			dev_warn(&pdev->dev, "Failed to request reset-gpio\n");
		else
			chip->reset_gpio_irq = gpiod_to_irq(chip->reset_gpio_desc);
	}

	/* Add gpiochip */
	ret = devm_gpiochip_add_data(&pdev->dev, &chip->gpio_chip, chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to register gpiochip\n");
		goto err_exit;
	}

	platform_set_drvdata(pdev, chip);

	pr_info("%s successfully\n", __func__);

	return 0;

err_exit:
	return ret;
}

static int gpio_mcu_remove(struct platform_device *pdev)
{
	struct mcu_gpio_chip *chip = platform_get_drvdata(pdev);

	pr_info("%s name=%s\n", __func__, chip->name);

	return 0;
}

static const struct of_device_id mcu_gpio_of_match_table[] = {
	{
		.compatible = "rockchip,mcu-gpio",
		.data = (void *)32,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mcu_gpio_of_match_table);

static struct platform_driver gpio_mcu_driver = {
	.driver = {
		.name = "mcu-gpio",
		.of_match_table	= mcu_gpio_of_match_table,
#ifdef CONFIG_PM
		.pm = &mcu_dev_pm_ops,
#endif
	},
	.probe	= gpio_mcu_probe,
	.remove	= gpio_mcu_remove,
};

static int __init gpio_mcu_driver_init(void)
{
	return platform_driver_register(&gpio_mcu_driver);
}

static void __exit gpio_mcu_driver_exit(void)
{
	platform_driver_unregister(&gpio_mcu_driver);
}

/* it should be later than vehicle spi */
#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT
fs_initcall(gpio_mcu_driver_init);
#else
module_init(gpio_mcu_driver_init);
#endif
module_exit(gpio_mcu_driver_exit);

MODULE_AUTHOR("Luo Wei <lw@rock-chips.com>");
MODULE_DESCRIPTION("GPIO expander driver for rockchip mcu");
MODULE_LICENSE("GPL");
