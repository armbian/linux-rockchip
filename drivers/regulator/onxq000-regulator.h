/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */
#ifndef __ONXQ000_REGULATOR_H__
#define __ONXQ000_REGULATOR_H__

#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>

#define ONXQ000_REGULATOR_COUNT 4   /* Number of regulators in onxq000 */

enum onxq_mode {
	ONXQ_MODE_I2C = 0,              /* output voltage is controlled by register */
	ONXQ_MODE_GPIO,                 /* output voltage is controlled by GPIO */
};

enum onxq000_field {
	ONXQ_OVINEN = 0,
	ONXQ_THER,
	ONXQ_EXTEN,
	ONXQ_STBY,
	ONXQ_OLM,
	ONXQ_OCM,
	ONXQ_TSM,
	ONXQ_UVINM,
	ONXQ_OVINM,
	ONXQ_ACCM,
	ONXQ_EN1,
	ONXQ_EN2,
	ONXQ_EN3,
	ONXQ_EN4,
	ONXQ_CLR,
	ONXQ_ENC,
	ONXQ_MUX,
	ONXQ_R,
	ONXQ_ID,
	ONXQ_REL,
	ONXQ_UVIN,
	ONXQ_OVIN,
	ONXQ_CLO,
	ONXQ_RVS,
	ONXQ_ACC,
	ONXQ_OTPNG,
	ONXQ_OL1,
	ONXQ_OC1,
	ONXQ_TW1,
	ONXQ_TS1,
	ONXQ_OL2,
	ONXQ_OC2,
	ONXQ_TW2,
	ONXQ_TS2,
	ONXQ_OL3,
	ONXQ_OC3,
	ONXQ_TW3,
	ONXQ_TS3,
	ONXQ_OL4,
	ONXQ_OC4,
	ONXQ_TW4,
	ONXQ_TS4,
	ONXQ_ADC1H,
	ONXQ_ADC1L,
	ONXQ_ADC2H,
	ONXQ_ADC2L,
	ONXQ_ADC3H,
	ONXQ_ADC3L,
	ONXQ_ADC4H,
	ONXQ_ADC4L,
	ONXQ_FIELD_END,
};

struct onxq000 {
	enum onxq_mode mode;

	int fault_irq;

	struct device *dev;
	struct regmap *regmap;
	struct i2c_client *client;

	struct regmap_field *rmap_fields[ONXQ_FIELD_END];
	struct regulator_dev *rdevs[ONXQ000_REGULATOR_COUNT];

	struct gpio_desc *ena_gpiods[ONXQ000_REGULATOR_COUNT];
	int ena_gpio_states[ONXQ000_REGULATOR_COUNT];
};

#endif /* __ONXQ000_REGULATOR_H__ */
