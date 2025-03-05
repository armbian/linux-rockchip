// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include "linux/stddef.h"
#include <linux/acpi.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include "inv_icm42670.h"

static int icm42670_i2c_bus_setup(struct icm42670_data *st)
{
	/* set slew rates for I2C and SPI */
	// TODO

	/* disable SPI bus */
	return regmap_update_bits(st->regmap, REG_INTF_CONFIG0,
				  BIT_FIFO_COUNT_REC_UI_SIFS_CFG_MASK,
				   BIT_FIFO_COUNT_REC_UI_SIFS_CFG_SPI_DIS);
}

static int icm42670_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct regmap *regmap;
	const char *name = NULL;
	int chip_type = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2c function error\n");
		return -EOPNOTSUPP;
	}

	if (id) {
		chip_type = id->driver_data;
		name = id->name;
	}

	regmap = devm_regmap_init_i2c(client, &icm42670_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(&client->dev, "Failed to register i2c regmap %d\n",
			(int)PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	dev_info(&client->dev, "chip_type = %d, name = %s\n", chip_type, name);

	return icm42670_core_probe(regmap, client->irq, name, chip_type, icm42670_i2c_bus_setup);
}

static void icm42670_i2c_remove(struct i2c_client *client)
{
	icm42670_core_remove(&client->dev);
}

static const struct i2c_device_id icm42670_i2c_id[] = {
	{"icm42670", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, icm42670_i2c_id);

static const struct acpi_device_id icm42670_acpi_match[] = {
	{"ICM42670", 0},
	{ }
};
MODULE_DEVICE_TABLE(acpi, icm42670_acpi_match);

#ifdef CONFIG_OF
static const struct of_device_id icm42670_of_match[] = {
	{ .compatible = "invensense,icm42670" },
	{ },
};
MODULE_DEVICE_TABLE(of, icm42670_of_match);
#endif

static struct i2c_driver icm42670_i2c_driver = {
	.driver = {
		.name = "invensense,icm42670",
		.acpi_match_table = ACPI_PTR(icm42670_acpi_match),
		.of_match_table = of_match_ptr(icm42670_of_match),
		.pm = &icm42670_pm_ops,
	},
	.probe = icm42670_i2c_probe,
	.remove = icm42670_i2c_remove,
	.id_table = icm42670_i2c_id,
};
module_i2c_driver(icm42670_i2c_driver);

MODULE_AUTHOR("Hangyu Li <hangyu.li@rock-chips.com>");
MODULE_DESCRIPTION("ICM42670 I2C driver");
MODULE_LICENSE("GPL");
