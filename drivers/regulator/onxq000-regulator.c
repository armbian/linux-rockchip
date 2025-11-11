// SPDX-License-Identifier: GPL-2.0
/*
 * Regulator driver for OMNIVISION ONXQ000 power switch chip
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 *
 * Author: Jing Wu <jing.wu@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#include "onxq000-regulator.h"

static int onxq000_regulator_enable_regmap(struct regulator_dev *rdev);
static int onxq000_regulator_disable_regmap(struct regulator_dev *rdev);
static int onxq000_regulator_is_enabled_regmap(struct regulator_dev *rdev);

static const struct reg_field onxq000_reg_fields[] = {
	[ONXQ_OVINEN]   = REG_FIELD(0x00, 4, 4),
	[ONXQ_THER]     = REG_FIELD(0x00, 5, 5),
	[ONXQ_EXTEN]    = REG_FIELD(0x00, 6, 6),
	[ONXQ_STBY]     = REG_FIELD(0x00, 7, 7),
	[ONXQ_OLM]      = REG_FIELD(0x01, 0, 0),
	[ONXQ_OCM]      = REG_FIELD(0x01, 1, 1),
	[ONXQ_TSM]      = REG_FIELD(0x01, 2, 2),
	[ONXQ_UVINM]    = REG_FIELD(0x01, 3, 3),
	[ONXQ_OVINM]    = REG_FIELD(0x01, 4, 4),
	[ONXQ_ACCM]     = REG_FIELD(0x01, 5, 5),
	[ONXQ_EN1]      = REG_FIELD(0x02, 0, 0),
	[ONXQ_EN2]      = REG_FIELD(0x02, 1, 1),
	[ONXQ_EN3]      = REG_FIELD(0x02, 2, 2),
	[ONXQ_EN4]      = REG_FIELD(0x02, 3, 3),
	[ONXQ_CLR]      = REG_FIELD(0x02, 4, 4),
	[ONXQ_ENC]      = REG_FIELD(0x02, 5, 5),
	[ONXQ_MUX]      = REG_FIELD(0x02, 6, 7),
	[ONXQ_R]        = REG_FIELD(0x03, 0, 3),
	[ONXQ_ID]       = REG_FIELD(0x03, 4, 5),
	[ONXQ_REL]      = REG_FIELD(0x03, 7, 7),
	[ONXQ_UVIN]     = REG_FIELD(0x04, 0, 0),
	[ONXQ_OVIN]     = REG_FIELD(0x04, 1, 1),
	[ONXQ_CLO]      = REG_FIELD(0x04, 2, 2),
	[ONXQ_RVS]      = REG_FIELD(0x04, 3, 3),
	[ONXQ_ACC]      = REG_FIELD(0x04, 6, 6),
	[ONXQ_OTPNG]    = REG_FIELD(0x04, 7, 7),
	[ONXQ_OL1]      = REG_FIELD(0x05, 0, 0),
	[ONXQ_OC1]      = REG_FIELD(0x05, 1, 1),
	[ONXQ_TW1]      = REG_FIELD(0x05, 2, 2),
	[ONXQ_TS1]      = REG_FIELD(0x05, 3, 3),
	[ONXQ_OL2]      = REG_FIELD(0x05, 4, 4),
	[ONXQ_OC2]      = REG_FIELD(0x05, 5, 5),
	[ONXQ_TW2]      = REG_FIELD(0x05, 6, 6),
	[ONXQ_TS2]      = REG_FIELD(0x05, 7, 7),
	[ONXQ_OL3]      = REG_FIELD(0x06, 0, 0),
	[ONXQ_OC3]      = REG_FIELD(0x06, 1, 1),
	[ONXQ_TW3]      = REG_FIELD(0x06, 2, 2),
	[ONXQ_TS3]      = REG_FIELD(0x06, 3, 3),
	[ONXQ_OL4]      = REG_FIELD(0x06, 4, 4),
	[ONXQ_OC4]      = REG_FIELD(0x06, 5, 5),
	[ONXQ_TW4]      = REG_FIELD(0x06, 6, 6),
	[ONXQ_TS4]      = REG_FIELD(0x06, 7, 7),
	[ONXQ_ADC1H]    = REG_FIELD(0x07, 0, 7),
	[ONXQ_ADC1L]    = REG_FIELD(0x08, 6, 7),
	[ONXQ_ADC2H]    = REG_FIELD(0x09, 0, 7),
	[ONXQ_ADC2L]    = REG_FIELD(0x0A, 6, 7),
	[ONXQ_ADC3H]    = REG_FIELD(0x0B, 0, 7),
	[ONXQ_ADC3L]    = REG_FIELD(0x0C, 6, 7),
	[ONXQ_ADC4H]    = REG_FIELD(0x0D, 0, 7),
	[ONXQ_ADC4L]    = REG_FIELD(0x0E, 6, 7),
};

static const struct regulator_ops onxq000_regulator_ops = {
	.enable			= onxq000_regulator_enable_regmap,
	.disable		= onxq000_regulator_disable_regmap,
	.is_enabled		= onxq000_regulator_is_enabled_regmap,
};

static const struct regulator_desc onxq000_descs[ONXQ000_REGULATOR_COUNT] = {
	[0] = {
		.name = "out1",
		.supply_name = "vin",
		.of_match = of_match_ptr("out1"),
		.regulators_node = of_match_ptr("regulators"),
		.id = 0,
		.ops = &onxq000_regulator_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 1,
		.enable_reg = 0x02,
		.enable_mask = BIT(0),
		.enable_val = BIT(0),
		.disable_val = 0x00,
		.owner = THIS_MODULE,
	},
	[1] = {
		.name = "out2",
		.supply_name = "vin",
		.of_match = of_match_ptr("out2"),
		.regulators_node = of_match_ptr("regulators"),
		.id = 1,
		.ops = &onxq000_regulator_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 1,
		.enable_reg = 0x02,
		.enable_mask = BIT(1),
		.enable_val = BIT(1),
		.disable_val = 0x00,
		.owner = THIS_MODULE,
	},
	[2] = {
		.name = "out3",
		.supply_name = "vin",
		.of_match = of_match_ptr("out3"),
		.regulators_node = of_match_ptr("regulators"),
		.id = 2,
		.ops = &onxq000_regulator_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 1,
		.enable_reg = 0x02,
		.enable_mask = BIT(2),
		.enable_val = BIT(2),
		.disable_val = 0x00,
		.owner = THIS_MODULE,
	},
	[3] = {
		.name = "out4",
		.supply_name = "vin",
		.of_match = of_match_ptr("out4"),
		.regulators_node = of_match_ptr("regulators"),
		.id = 3,
		.ops = &onxq000_regulator_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 1,
		.enable_reg = 0x02,
		.enable_mask = BIT(3),
		.enable_val = BIT(3),
		.disable_val = 0x00,
		.owner = THIS_MODULE,
	},
};

static const struct regmap_range onxq000_yes_ranges[] = {
	regmap_reg_range(0x00, 0x0e),
};

static const struct regmap_access_table onxq000_volatile_table = {
	.yes_ranges = onxq000_yes_ranges,
	.n_yes_ranges = ARRAY_SIZE(onxq000_yes_ranges),
};

static const struct regmap_config onxq000_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.volatile_table = &onxq000_volatile_table,
};

static int onxq000_field_read(struct onxq000 *onxq000, enum onxq000_field field_id)
{
	int ret;
	int val;

	ret = regmap_field_read(onxq000->rmap_fields[field_id], &val);
	if (ret < 0)
		return ret;

	return val;
}

static int onxq000_field_write(struct onxq000 *onxq000, enum onxq000_field field_id,
								unsigned int val)
{
	return regmap_field_write(onxq000->rmap_fields[field_id], val);
}

static int onxq000_regulator_enable_regmap(struct regulator_dev *rdev)
{
	int rdev_idx = 0;
	struct onxq000 *onxq000 = rdev_get_drvdata(rdev);

	dev_dbg(onxq000->dev, "enable_regmap called, rdev[%s]\n", rdev->desc->name);

	/* first: get index of this regulator device */
	for (rdev_idx = 0; rdev_idx < ONXQ000_REGULATOR_COUNT; rdev_idx++) {
		if (onxq000->rdevs[rdev_idx] == rdev)
			break;
	}

	if (rdev_idx >= ONXQ000_REGULATOR_COUNT) {
		dev_err(onxq000->dev, "Failed to find regulator device\n");
		return -EINVAL;
	}

	/* second: enable the regulator */
	if (onxq000->mode == ONXQ_MODE_GPIO) {
		/* control by GPIO */
		if (onxq000->ena_gpiods[rdev_idx]) {
			gpiod_set_value(onxq000->ena_gpiods[rdev_idx], 1);
			onxq000->ena_gpio_states[rdev_idx] = 1;
		} else {
			dev_err(onxq000->dev, "gpio not available for regulator %d\n", rdev_idx);
			return -EINVAL;
		}
	} else {
		/* control by register */
		return regmap_field_write(onxq000->rmap_fields[ONXQ_EN1 + rdev_idx], 1);
	}

	return 0;
}

static int onxq000_regulator_disable_regmap(struct regulator_dev *rdev)
{
	int rdev_idx = 0;
	struct onxq000 *onxq000 = rdev_get_drvdata(rdev);

	dev_dbg(onxq000->dev, "disable_regmap called, rdev[%s]\n", rdev->desc->name);

	/* first: get index of this regulator device */
	for (rdev_idx = 0; rdev_idx < ONXQ000_REGULATOR_COUNT; rdev_idx++) {
		if (onxq000->rdevs[rdev_idx] == rdev)
			break;
	}

	if (rdev_idx >= ONXQ000_REGULATOR_COUNT) {
		dev_err(onxq000->dev, "Failed to find regulator device\n");
		return -EINVAL;
	}

	/* second: disable the regulator */
	if (onxq000->mode == ONXQ_MODE_GPIO) {
		/* control by GPIO */
		if (onxq000->ena_gpiods[rdev_idx]) {
			gpiod_set_value(onxq000->ena_gpiods[rdev_idx], 0);
			onxq000->ena_gpio_states[rdev_idx] = 0;
		} else {
			dev_err(onxq000->dev, "gpio not available for regulator %d\n", rdev_idx);
			return -EINVAL;
		}
	} else {
		/* control by register */
		return regmap_field_write(onxq000->rmap_fields[ONXQ_EN1 + rdev_idx], 0);
	}

	return 0;
}

static int onxq000_regulator_is_enabled_regmap(struct regulator_dev *rdev)
{
	int val = 0;
	int ret = 0;
	int rdev_idx = 0;
	struct onxq000 *onxq000 = rdev_get_drvdata(rdev);

	dev_dbg(onxq000->dev, "enabled_regmap called, rdev[%s]\n", rdev->desc->name);

	/* first: get index of this regulator device */
	for (rdev_idx = 0; rdev_idx < ONXQ000_REGULATOR_COUNT; rdev_idx++) {
		if (onxq000->rdevs[rdev_idx] == rdev)
			break;
	}

	if (rdev_idx >= ONXQ000_REGULATOR_COUNT) {
		dev_err(onxq000->dev, "Failed to find regulator device\n");
		return -EINVAL;
	}

	/* second: check if the regulator is enabled */
	if (onxq000->mode == ONXQ_MODE_GPIO) {
		/* control by GPIO */
		if (onxq000->ena_gpiods[rdev_idx]) {
			val = onxq000->ena_gpio_states[rdev_idx];
		} else {
			dev_err(onxq000->dev, "gpio not available for regulator %d\n", rdev_idx);
			val = -EINVAL;
		}
	} else {
		/* control by register */
		ret = regmap_field_read(onxq000->rmap_fields[ONXQ_EN1 + rdev_idx], &val);
		if (ret < 0)
			val = ret;
	}

	return val;
}

static int onxq000_device_init(struct onxq000 *onxq000)
{
	int i = 0;
	int rel_field = 0;
	int id_field = 0;
	int r_field = 0;
	struct regulator_config onxq000_config;
	struct device_node *regulators_node = NULL;

	/* first: init regmap fields for onxq000 */
	for (i = 0; i < ONXQ_FIELD_END; i++) {
		onxq000->rmap_fields[i] = devm_regmap_field_alloc(onxq000->dev, onxq000->regmap,
									onxq000_reg_fields[i]);
		if (IS_ERR(onxq000->rmap_fields[i])) {
			dev_err(onxq000->dev, "Failed to allocate regmap field %d\n", i);
			return PTR_ERR(onxq000->rmap_fields[i]);
		}
	}

	/* second: read id information of onxq000 */
	rel_field = onxq000_field_read(onxq000, ONXQ_REL);
	id_field = onxq000_field_read(onxq000, ONXQ_ID);
	r_field = onxq000_field_read(onxq000, ONXQ_R);
	dev_info(onxq000->dev, "ONXQ000 ID: %d, REL: %d, R: %d\n", id_field, rel_field, r_field);

	/* third: init working mode of onxq000 */
	if (of_property_read_bool(onxq000->dev->of_node, "onxq000,control-by-gpio")) {
		dev_info(onxq000->dev, "ONXQ000 is configured to be controlled by GPIOs\n");
		onxq000->mode = ONXQ_MODE_GPIO;
		onxq000_field_write(onxq000, ONXQ_EXTEN, 1);	/* control by GPIO */
	} else {
		dev_info(onxq000->dev, "ONXQ000 is configured to be controlled by I2C registers\n");
		onxq000->mode = ONXQ_MODE_I2C;
		onxq000_field_write(onxq000, ONXQ_EXTEN, 0);	/* control by I2C */
	}

	/* fourth: register regulators */
	memset(&onxq000_config, 0, sizeof(onxq000_config));
	onxq000_config.dev = onxq000->dev;
	onxq000_config.regmap = onxq000->regmap;
	onxq000_config.driver_data = onxq000;
	onxq000_config.of_node = onxq000->dev->of_node;

	regulators_node = of_get_child_by_name(onxq000->dev->of_node, "regulators");
	if (!regulators_node) {
		dev_err(onxq000->dev, "Failed to get regulators node\n");
		return -EINVAL;
	}

	for (i = 0; i < ONXQ000_REGULATOR_COUNT; i++) {
		onxq000_config.ena_gpiod = devm_gpiod_get_index(onxq000->dev, "ena",
								i, GPIOD_OUT_LOW);
		if (!IS_ERR(onxq000_config.ena_gpiod)) {
			onxq000->ena_gpiods[i] = onxq000_config.ena_gpiod;
			onxq000->ena_gpio_states[i] = 0;	/* initial state is low */
		} else {
			onxq000_config.ena_gpiod = NULL;
			onxq000->ena_gpiods[i] = NULL;
			onxq000->ena_gpio_states[i] = -1;	/* invalid state */
			dev_info(onxq000->dev, "gpio not found for regulator %d, regulator[%s]\n",
						i, onxq000_descs[i].name);
		}

		onxq000->rdevs[i] = devm_regulator_register(onxq000->dev, &onxq000_descs[i],
							&onxq000_config);
		if (IS_ERR(onxq000->rdevs[i])) {
			dev_err(onxq000->dev, "failed to register %s regulator\n",
					onxq000_descs[i].name);
			return PTR_ERR(onxq000->rdevs[i]);
		}
	}

	return 0;
}

static int onxq000_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct onxq000 *onxq000 = NULL;

	/* first: allocate memory for onxq000 device structure */
	onxq000 = devm_kzalloc(&client->dev, sizeof(*onxq000), GFP_KERNEL);
	if (!onxq000)
		return -ENOMEM;

	onxq000->regmap = devm_regmap_init_i2c(client, &onxq000_regmap_config);
	if (IS_ERR(onxq000->regmap)) {
		dev_err(&client->dev, "regmap initialization failed\n");
		return PTR_ERR(onxq000->regmap);
	}

	onxq000->client = client;
	onxq000->dev = &client->dev;
	onxq000->fault_irq = client->irq;

	i2c_set_clientdata(client, onxq000);

	return onxq000_device_init(onxq000);
}

static const struct of_device_id onxq000_of_match[] = {
	{ .compatible = "omnivision,onxq000", },
	{ }
};

static struct i2c_driver onxq000_i2c_driver = {
	.driver = {
		.name = "onxq000-regulator",
		.of_match_table = of_match_ptr(onxq000_of_match),
	},
	.probe    = onxq000_i2c_probe,
};
module_i2c_driver(onxq000_i2c_driver);

MODULE_AUTHOR("Jing Wu <jing.wu@rock-chips.com>");
MODULE_DESCRIPTION("ONXQ000 Voltage Regulator Driver");
MODULE_LICENSE("GPL");
