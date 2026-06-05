// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/of.h>

#include "imu.h"
#include "invimu_core.h"

static bool invimu_spi_writeable_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static bool invimu_spi_volatile_reg(struct device *dev, unsigned int reg)
{
	return true;
}

const struct regmap_config invimu_spi_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
	.cache_type = REGCACHE_RBTREE,
	.writeable_reg = invimu_spi_writeable_reg,
	.volatile_reg = invimu_spi_volatile_reg,
	.read_flag_mask = 0x80,
	.write_flag_mask = 0x00,
};

static int invimu_spi_probe(struct spi_device *spi)
{
	struct regmap *regmap;
	int ret;

	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "SPI setup failed: %d\n", ret);
		return ret;
	}

	regmap = devm_regmap_init_spi(spi, &invimu_spi_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(&spi->dev, "Failed to register spi regmap: %ld\n", PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	return invimu_core_probe(&spi->dev, regmap, spi->irq, IMU_BUS_SPI);
}

static void invimu_spi_remove(struct spi_device *spi)
{
	struct imu_ctrb *ctrb = dev_get_drvdata(&spi->dev);

	if (ctrb)
		cancel_delayed_work_sync(&ctrb->pollingwork);
}

static int invimu_spi_suspend(struct device *dev)
{
	dev_info(dev, "inv_imu spi suspend\n");
	return 0;
}

static int invimu_spi_resume(struct device *dev)
{
	int ret;
	struct imu_ctrb *ctrb = dev_get_drvdata(dev);

	ret = invimu_chip_init(ctrb);
	dev_info(dev, "inv_imu spi resume:%d\n", ret);
	return ret;
}

static const struct dev_pm_ops invimu_spi_pm_ops = {
	.suspend = invimu_spi_suspend,
	.resume  = invimu_spi_resume,
};

static const struct spi_device_id invimu_spi_id[] = {
	{"icm42607", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, invimu_spi_id);

#ifdef CONFIG_OF
static const struct of_device_id invimu_spi_of_match[] = {
	{ .compatible = "inv,icm42607" },
	{ },
};
MODULE_DEVICE_TABLE(of, invimu_spi_of_match);
#endif

static struct spi_driver invimu_spi_driver = {
	.driver = {
		.name = "invimu_spi",
		.pm = &invimu_spi_pm_ops,
		.of_match_table = of_match_ptr(invimu_spi_of_match),
	},
	.probe = invimu_spi_probe,
	.remove = invimu_spi_remove,
	.id_table = invimu_spi_id,
};
module_spi_driver(invimu_spi_driver);

MODULE_DESCRIPTION("INV ICM42607 SPI driver");
MODULE_LICENSE("GPL");
