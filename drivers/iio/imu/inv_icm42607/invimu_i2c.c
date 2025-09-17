// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/pm_runtime.h>

#include "imu.h"
#include "invimu_core.h"

static bool invimu_writeable_reg(struct device *dev, unsigned int reg);
static bool invimu_volatile_reg(struct device *dev, unsigned int reg);

const struct regmap_config invimu_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
	.cache_type = REGCACHE_RBTREE,
	.writeable_reg = invimu_writeable_reg,
	.volatile_reg = invimu_volatile_reg,
};

static bool invimu_writeable_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static bool invimu_volatile_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static int invimu_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct regmap *regmap;

	regmap = devm_regmap_init_i2c(client, &invimu_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(&client->dev, "Failed to register i2c regmap: %p\n", regmap);
		return PTR_ERR(regmap);
	}
	return invimu_core_probe(&client->dev, regmap, client->irq, false);
}

static int invimu_suspend(struct device *dev)
{
	dev_info(dev, "inv_imu suspend\n");
	return 0;
}

static int invimu_resume(struct device *dev)
{
	int ret;
	struct imu_ctrb *ctrb = dev_get_drvdata(dev);

	ret = invimu_chip_init(ctrb, false);
	dev_info(dev, "inv_imu resume:%d\n", ret);
	return ret;
}

static const struct dev_pm_ops invimu_pm_ops = {
	.suspend = invimu_suspend,
	.resume  = invimu_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id invimu_of_match[] = {
	{ .compatible = "inv,icm42607"},
	{ },
};
MODULE_DEVICE_TABLE(of, invimu_of_match);
#endif

static struct i2c_driver invimu_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "invimu_i2c",
		.pm = &invimu_pm_ops,
		.of_match_table = of_match_ptr(invimu_of_match),
	},
	.probe = invimu_i2c_probe,
};

static int32_t __init invimu_driver_init(void)
{
	return i2c_add_driver(&invimu_i2c_driver);
}

static void __exit invimu_driver_exit(void)
{
	i2c_del_driver(&invimu_i2c_driver);
}

late_initcall(invimu_driver_init);
module_exit(invimu_driver_exit);

MODULE_DESCRIPTION("INV ICM42607 I2C driver");
MODULE_LICENSE("GPL");
