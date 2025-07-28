// SPDX-License-Identifier: GPL-2.0-only
/*
 * Regulator driver for Rockchip RK801
 *
 * Copyright (c) 2024, Rockchip Electronics Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/mfd/rk808.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/syscore_ops.h>
#include <linux/gpio/consumer.h>

/* Field Definitions */
#define RK801_BUCK_VSEL_MASK		0x7f
#define RK801_LDO_VSEL_MASK		0x3f
#define ENABLE_MASK(id)			(BIT(4 + (id)) | BIT(id))
#define ENABLE_VAL(id)			(BIT(4 + (id)) | BIT(id))
#define DISABLE_VAL(id)			(BIT(4 + (id)) | 0)

#define RK801_DESC(_id, _match, _supply, _min, _max, _step, _vreg,	\
	_vmask, _ereg, _emask, _enval, _disval, _etime)			\
	{								\
		.name		= (_match),				\
		.supply_name	= (_supply),				\
		.of_match	= of_match_ptr(_match),			\
		.regulators_node = of_match_ptr("regulators"),		\
		.type		= REGULATOR_VOLTAGE,			\
		.id		= (_id),				\
		.n_voltages	= (((_max) - (_min)) / (_step) + 1),	\
		.owner		= THIS_MODULE,				\
		.min_uV		= (_min) * 1000,			\
		.uV_step	= (_step) * 1000,			\
		.vsel_reg	= (_vreg),				\
		.vsel_mask	= (_vmask),				\
		.enable_reg	= (_ereg),				\
		.enable_mask	= (_emask),				\
		.enable_val     = (_enval),				\
		.disable_val    = (_disval),				\
		.enable_time	= (_etime),				\
		.ops		= &rk801_reg_ops,			\
	}

#define RK801_DESC_SWITCH(_id, _match, _supply, _ereg, _emask, _enval,	\
	_disval, _etime)							\
	{								\
		.name		= (_match),				\
		.supply_name	= (_supply),				\
		.of_match	= of_match_ptr(_match),			\
		.regulators_node = of_match_ptr("regulators"),		\
		.type		= REGULATOR_VOLTAGE,			\
		.id		= (_id),				\
		.enable_reg	= (_ereg),				\
		.enable_mask	= (_emask),				\
		.enable_val     = (_enval),				\
		.disable_val    = (_disval),				\
		.enable_time	= (_etime),				\
		.owner		= THIS_MODULE,				\
		.ops		= &rk801_switch_ops			\
	}

struct suspend_device {
	struct regulator_dev *rdev;
	int enable;
	int uv;
};

struct runtime_device {
	int reg_src;
	int reg_dst;
};

struct rk801_regulator_data {
	struct gpio_desc *pwrctrl_gpio;
	struct suspend_device sdev[RK801_ID_MAX];
	bool req_pwrctrl_dvs;
};

static const struct linear_range rk801_buck1_voltage_ranges[] = {
	REGULATOR_LINEAR_RANGE(500000,   0, 80, 12500),	/* 0.5v - 1.5v */
	REGULATOR_LINEAR_RANGE(1800000, 81, 82, 400000),/* 1.8v - 2.2v */
	REGULATOR_LINEAR_RANGE(3300000, 83, 83, 0),	/* 3.3v */
	REGULATOR_LINEAR_RANGE(5000000, 84, 84, 0),	/* 5.0v */
	REGULATOR_LINEAR_RANGE(5250000, 85, 85, 0),	/* 5.25v */
};

static const struct linear_range rk801_buck2_voltage_ranges[] = {
	REGULATOR_LINEAR_RANGE(800000,  0, 2, 50000),	/* 0.8v - 0.9v */
	REGULATOR_LINEAR_RANGE(1800000, 3, 4, 400000),	/* 1.8v - 2.2v */
	REGULATOR_LINEAR_RANGE(3300000, 5, 5, 0),	/* 3.3v */
	REGULATOR_LINEAR_RANGE(5000000, 6, 6, 0),	/* 5.0v */
	REGULATOR_LINEAR_RANGE(5250000, 7, 7, 0),	/* 5.25v */
};

static const struct linear_range rk801_buck4_voltage_ranges[] = {
	REGULATOR_LINEAR_RANGE(500000,   0, 80, 12500),	/* 0.5v - 1.5v */
	REGULATOR_LINEAR_RANGE(1800000, 81, 82, 400000),/* 1.8v - 2.2v */
	REGULATOR_LINEAR_RANGE(2500000, 83, 83, 0),	/* 2.5v */
	REGULATOR_LINEAR_RANGE(2800000, 84, 84, 0),	/* 2.8v */
	REGULATOR_LINEAR_RANGE(3000000, 85, 85, 0),	/* 3.0v */
	REGULATOR_LINEAR_RANGE(3300000, 86, 86, 0),	/* 3.3v */
};

static unsigned int rk801_regulator_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case 1:
		return REGULATOR_MODE_FAST;
	case 2:
		return REGULATOR_MODE_NORMAL;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static unsigned int rk801_get_mode(struct regulator_dev *rdev)
{
	unsigned int pmw_mode_msk;
	unsigned int val;
	int err;

	pmw_mode_msk = BIT(rdev->desc->id);
	err = regmap_read(rdev->regmap, RK801_POWER_FPWM_EN_REG, &val);
	if (err)
		return err;

	if (val & pmw_mode_msk)
		return REGULATOR_MODE_FAST;
	else
		return REGULATOR_MODE_NORMAL;
}

static int rk801_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	unsigned int offset = rdev->desc->id;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		return regmap_update_bits(rdev->regmap, RK801_POWER_FPWM_EN_REG,
					  BIT(offset), RK801_FPWM_MODE << offset);
	case REGULATOR_MODE_NORMAL:
		return regmap_update_bits(rdev->regmap, RK801_POWER_FPWM_EN_REG,
					  BIT(offset), RK801_AUTO_PWM_MODE << offset);
	default:
		dev_err(&rdev->dev, "do not support this mode\n");
		return -EINVAL;
	}

	return 0;
}

static int rk801_set_suspend_voltage(struct regulator_dev *rdev, int uv)
{
	struct rk801_regulator_data *pdata = rdev_get_drvdata(rdev);
	unsigned int reg;
	int sel;

	if (pdata->req_pwrctrl_dvs) {
		pdata->sdev[rdev->desc->id].rdev = rdev;
		pdata->sdev[rdev->desc->id].uv = uv;
	} else {
		if (rdev->desc->id < RK801_ID_LDO1)
			sel = regulator_map_voltage_linear_range(rdev, uv, uv);
		else
			sel = regulator_map_voltage_linear(rdev, uv, uv);
		if (sel < 0)
			return -EINVAL;

		reg = rdev->desc->vsel_reg + RK801_SLP_REG_OFFSET;

		return regmap_update_bits(rdev->regmap, reg,
					  rdev->desc->vsel_mask, sel);
	}

	return 0;
}

static int rk801_set_suspend_enable(struct regulator_dev *rdev)
{
	struct rk801_regulator_data *pdata = rdev_get_drvdata(rdev);

	if (pdata->req_pwrctrl_dvs) {
		pdata->sdev[rdev->desc->id].rdev = rdev;
		pdata->sdev[rdev->desc->id].enable = 1;
	} else {
		return regmap_update_bits(rdev->regmap, RK801_POWER_SLP_EN_REG,
					  BIT(rdev->desc->id), BIT(rdev->desc->id));
	}

	return 0;
}

static int rk801_set_suspend_disable(struct regulator_dev *rdev)
{
	struct rk801_regulator_data *pdata = rdev_get_drvdata(rdev);

	if (pdata->req_pwrctrl_dvs) {
		pdata->sdev[rdev->desc->id].rdev = rdev;
		pdata->sdev[rdev->desc->id].enable = 0;
	} else {
		return regmap_update_bits(rdev->regmap, RK801_POWER_SLP_EN_REG,
					  BIT(rdev->desc->id), 0);
	}

	return 0;
}

static int rk801_set_enable(struct regulator_dev *rdev)
{
	struct rk801_regulator_data *pdata = rdev_get_drvdata(rdev);
	int ret;

	ret = regulator_enable_regmap(rdev);
	if (ret)
		return ret;

	if (pdata->req_pwrctrl_dvs)
		return regmap_update_bits(rdev->regmap, RK801_POWER_SLP_EN_REG,
					  BIT(rdev->desc->id), BIT(rdev->desc->id));

	return 0;
}

static int rk801_set_disable(struct regulator_dev *rdev)
{
	struct rk801_regulator_data *pdata = rdev_get_drvdata(rdev);
	int ret;

	ret = regulator_disable_regmap(rdev);
	if (ret)
		return ret;

	if (pdata->req_pwrctrl_dvs)
		return regmap_update_bits(rdev->regmap, RK801_POWER_SLP_EN_REG,
					  BIT(rdev->desc->id), 0);

	return 0;
}

static int rk801_set_voltage_time_sel(struct regulator_dev *rdev,
				      unsigned int old_selector,
				      unsigned int new_selector)
{
	return regulator_set_voltage_time_sel(rdev, old_selector, new_selector) + 32;
}

static int rk801_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
	struct rk801_regulator_data *pdata = rdev_get_drvdata(rdev);
	struct gpio_desc *gpio = pdata->pwrctrl_gpio;
	unsigned int reg_normal = rdev->desc->vsel_reg;
	unsigned int reg_sleep = rdev->desc->vsel_reg + RK801_SLP_REG_OFFSET;
	unsigned int reg_target;
	int act_level; /* logic value */
	int ret;

	if (pdata->req_pwrctrl_dvs) {
		/*
		 * act_level=1: active on reg_sleep now -> target: reg_normal
		 * act_level=0: inactive on reg_normal now -> target: reg_sleep
		 */
		act_level = gpiod_get_value(gpio);
		reg_target = act_level ? reg_normal : reg_sleep;

		sel <<= ffs(rdev->desc->vsel_mask) - 1;
		ret = regmap_update_bits(rdev->regmap, reg_target, rdev->desc->vsel_mask, sel);
		if (ret)
			return ret;

		udelay(40); /* hw sync */

		gpiod_set_value(gpio, !act_level);

		if (reg_target == reg_normal)
			ret = regmap_update_bits(rdev->regmap, reg_sleep,
						 rdev->desc->vsel_mask, sel);
		else
			ret = regmap_update_bits(rdev->regmap, reg_normal,
						 rdev->desc->vsel_mask, sel);
	} else {
		ret = regulator_set_voltage_sel_regmap(rdev, sel);
	}

	return ret;
}

static const struct regulator_ops rk801_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear_range,
	.map_voltage		= regulator_map_voltage_linear_range,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= rk801_set_voltage_sel,
	.set_voltage_time_sel	= rk801_set_voltage_time_sel,
	.enable			= rk801_set_enable,
	.disable		= rk801_set_disable,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_mode		= rk801_set_mode,
	.get_mode		= rk801_get_mode,
	.set_suspend_voltage	= rk801_set_suspend_voltage,
	.set_suspend_enable	= rk801_set_suspend_enable,
	.set_suspend_disable	= rk801_set_suspend_disable,
};

static const struct regulator_ops rk801_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= rk801_set_voltage_sel,
	.enable			= rk801_set_enable,
	.disable		= rk801_set_disable,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_suspend_voltage	= rk801_set_suspend_voltage,
	.set_suspend_enable	= rk801_set_suspend_enable,
	.set_suspend_disable	= rk801_set_suspend_disable,
};

static const struct regulator_ops rk801_switch_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_suspend_enable	= rk801_set_suspend_enable,
	.set_suspend_disable	= rk801_set_suspend_disable,
};

static const struct regulator_desc rk801_reg[] = {
	{
		.name = "DCDC_REG1",
		.supply_name = "vcc1",
		.of_match = of_match_ptr("DCDC_REG1"),
		.regulators_node = of_match_ptr("regulators"),
		.id = RK801_ID_DCDC1,
		.ops = &rk801_buck_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 86,
		.linear_ranges = rk801_buck1_voltage_ranges,
		.n_linear_ranges = ARRAY_SIZE(rk801_buck1_voltage_ranges),
		.vsel_reg = RK801_BUCK1_ON_VSEL_REG,
		.vsel_mask = RK801_BUCK_VSEL_MASK,
		.enable_reg = RK801_POWER_EN0_REG,
		.enable_mask = ENABLE_MASK(RK801_ID_DCDC1),
		.enable_val = ENABLE_VAL(RK801_ID_DCDC1),
		.disable_val = DISABLE_VAL(RK801_ID_DCDC1),
		.ramp_delay = 1000,
		.of_map_mode = rk801_regulator_of_map_mode,
		.enable_time = 400,
		.owner = THIS_MODULE,
	}, {
		.name = "DCDC_REG2",
		.supply_name = "vcc2",
		.of_match = of_match_ptr("DCDC_REG2"),
		.regulators_node = of_match_ptr("regulators"),
		.id = RK801_ID_DCDC2,
		.ops = &rk801_buck_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 8,
		.linear_ranges = rk801_buck2_voltage_ranges,
		.n_linear_ranges = ARRAY_SIZE(rk801_buck2_voltage_ranges),
		.vsel_reg = RK801_BUCK2_ON_VSEL_REG,
		.vsel_mask = RK801_BUCK_VSEL_MASK,
		.enable_reg = RK801_POWER_EN0_REG,
		.enable_mask = ENABLE_MASK(RK801_ID_DCDC2),
		.enable_val = ENABLE_VAL(RK801_ID_DCDC2),
		.disable_val = DISABLE_VAL(RK801_ID_DCDC2),
		.ramp_delay = 1000,
		.of_map_mode = rk801_regulator_of_map_mode,
		.enable_time = 400,
		.owner = THIS_MODULE,
	}, {
		.name = "DCDC_REG3",
		.supply_name = "vcc3",
		.of_match = of_match_ptr("DCDC_REG3"),
		.regulators_node = of_match_ptr("regulators"),
		.id = RK801_ID_DCDC3,
		.ops = &rk801_switch_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 1,
		.enable_reg = RK801_POWER_EN0_REG,
		.enable_mask = ENABLE_MASK(RK801_ID_DCDC3),
		.enable_val = ENABLE_VAL(RK801_ID_DCDC3),
		.disable_val = DISABLE_VAL(RK801_ID_DCDC3),
		.of_map_mode = rk801_regulator_of_map_mode,
		.enable_time = 400,
		.owner = THIS_MODULE,
	}, {
		.name = "DCDC_REG4",
		.supply_name = "vcc4",
		.of_match = of_match_ptr("DCDC_REG4"),
		.regulators_node = of_match_ptr("regulators"),
		.id = RK801_ID_DCDC4,
		.ops = &rk801_buck_ops,
		.type = REGULATOR_VOLTAGE,
		.n_voltages = 87,
		.linear_ranges = rk801_buck4_voltage_ranges,
		.n_linear_ranges = ARRAY_SIZE(rk801_buck4_voltage_ranges),
		.vsel_reg = RK801_BUCK4_ON_VSEL_REG,
		.vsel_mask = RK801_BUCK_VSEL_MASK,
		.enable_reg = RK801_POWER_EN0_REG,
		.enable_mask = ENABLE_MASK(RK801_ID_DCDC4),
		.enable_val = ENABLE_VAL(RK801_ID_DCDC4),
		.disable_val = DISABLE_VAL(RK801_ID_DCDC4),
		.ramp_delay = 1000,
		.of_map_mode = rk801_regulator_of_map_mode,
		.enable_time = 400,
		.owner = THIS_MODULE,
	},

	RK801_DESC(RK801_ID_LDO1, "LDO_REG1", "vcc5", 500, 3400, 50,
		RK801_LDO1_ON_VSEL_REG, RK801_LDO_VSEL_MASK, RK801_POWER_EN1_REG,
		ENABLE_MASK(0), ENABLE_VAL(0), DISABLE_VAL(0), 400),
	RK801_DESC(RK801_ID_LDO2, "LDO_REG2", "vcc6", 500, 3400, 50,
		RK801_LDO2_ON_VSEL_REG, RK801_LDO_VSEL_MASK, RK801_POWER_EN1_REG,
		ENABLE_MASK(1), ENABLE_VAL(1), DISABLE_VAL(1), 400),
	RK801_DESC_SWITCH(RK801_ID_SWITCH, "SWITCH_REG", "vcc7", RK801_POWER_EN1_REG,
		ENABLE_MASK(2), ENABLE_VAL(2), DISABLE_VAL(2), 400),
};

static int rk801_regulator_init(struct device *dev)
{
	struct rk808 *rk808 = dev_get_drvdata(dev->parent);
	struct rk801_regulator_data *pdata = dev_get_drvdata(dev);
	struct runtime_device rdev[] = {
		{ RK801_BUCK1_ON_VSEL_REG, RK801_BUCK1_SLP_VSEL_REG },
		{ RK801_BUCK2_ON_VSEL_REG, RK801_BUCK2_SLP_VSEL_REG },
		{ RK801_BUCK4_ON_VSEL_REG, RK801_BUCK4_SLP_VSEL_REG },
		{ RK801_LDO1_ON_VSEL_REG,  RK801_LDO1_SLP_VSEL_REG },
		{ RK801_LDO2_ON_VSEL_REG,  RK801_LDO2_SLP_VSEL_REG },
	};
	uint act_pol, en0, en1;
	int i, ret, val;

	if (!pdata->req_pwrctrl_dvs)
		return 0;

	/* set pmic-pwrctrl active pol and use sleep function */
	act_pol = rk808->pwrctrl.act_low ?
			RK801_SLEEP_ACT_L : RK801_SLEEP_ACT_H;

	ret = regmap_update_bits(rk808->regmap, RK801_SYS_CFG2_REG,
				 RK801_SLEEP_POL_MSK, act_pol);
	if (ret)
		return ret;

	ret = regmap_update_bits(rk808->regmap, RK801_SLEEP_CFG_REG,
				 RK801_SLEEP_FUN_MSK, RK801_SLEEP_FUN);
	if (ret)
		return ret;

	/* disable buck/pldo slp lp */
	ret = regmap_update_bits(rk808->regmap, RK801_SLP_LP_CONFIG_REG,
				 RK801_SLP_LP_MASK, 0);
	if (ret)
		return ret;

	/* copy en/slp_en */
	ret = regmap_read(rk808->regmap, RK801_POWER_EN0_REG, &en0);
	if (ret)
		return ret;

	ret = regmap_read(rk808->regmap, RK801_POWER_EN1_REG, &en1);
	if (ret)
		return ret;

	val = (en0 & 0x0f) | ((en1 & 0x0f) << 4);
	ret = regmap_write(rk808->regmap, RK801_POWER_SLP_EN_REG, val);
	if (ret)
		return ret;

	/* copy on_vsel/slp_vsel */
	for (i = 0; i < ARRAY_SIZE(rdev); i++) {
		ret = regmap_read(rk808->regmap, rdev[i].reg_src, &val);
		if (ret)
			return ret;

		ret = regmap_write(rk808->regmap, rdev[i].reg_dst, val);
		if (ret)
			return ret;
	}

	/* init */
	for (i = 0; i < ARRAY_SIZE(pdata->sdev); i++) {
		pdata->sdev[i].rdev = NULL;
		pdata->sdev[i].uv = -EINVAL;
		pdata->sdev[i].enable = -EINVAL;
	}

	return 0;
}

static int rk801_set_suspend_dev(struct suspend_device sdev)
{
	struct regulator_dev *rdev = sdev.rdev;
	int enable = sdev.enable;
	int uv = sdev.uv;
	int val, sel, reg;
	int ret, mask;

	if (!rdev)
		return 0;

	if (uv != -EINVAL) {
		if (rdev->desc->id < RK801_ID_LDO1)
			sel = regulator_map_voltage_linear_range(rdev, uv, uv);
		else
			sel = regulator_map_voltage_linear(rdev, uv, uv);
		if (sel < 0)
			return -EINVAL;

		reg = rdev->desc->vsel_reg + RK801_SLP_REG_OFFSET;
		ret = regmap_update_bits(rdev->regmap, reg,
					 rdev->desc->vsel_mask, sel);
		if (ret)
			return ret;
	}

	if (enable != -EINVAL) {
		mask = BIT(rdev->desc->id);
		val = enable ? mask : 0;
		ret = regmap_update_bits(rdev->regmap,
					 RK801_POWER_SLP_EN_REG, mask, val);
		if (ret)
			return ret;
	}

	return 0;
}

static int rk801_regulator_suspend_late(struct device *dev)
{
	struct rk808 *rk808 = dev_get_drvdata(dev->parent);
	struct rk801_regulator_data *pdata = dev_get_drvdata(dev);
	struct gpio_desc *gpio = pdata->pwrctrl_gpio;
	int i, ret;

	if (!pdata->req_pwrctrl_dvs)
		return 0;

	/* set soc-pwrctrl inactive (0:inactive) */
	gpiod_set_value(gpio, 0);
	udelay(40);

	/* set buck/ldo/switch suspend */
	for (i = 0; i < ARRAY_SIZE(pdata->sdev); i++) {
		ret = rk801_set_suspend_dev(pdata->sdev[i]);
		if (ret)
			return ret;
	}

	/* enable buck/pldo lp enable */
	return regmap_update_bits(rk808->regmap, RK801_SLP_LP_CONFIG_REG,
				  RK801_SLP_LP_MASK, RK801_SLP_LP_MASK);
}

static int rk801_regulator_resume_early(struct device *dev)
{
	struct rk801_regulator_data *pdata = dev_get_drvdata(dev);

	if (!pdata->req_pwrctrl_dvs)
		return 0;

	return rk801_regulator_init(dev);
}

static int rk801_regulator_probe(struct platform_device *pdev)
{
	struct rk808 *rk808 = dev_get_drvdata(pdev->dev.parent);
	struct i2c_client *client = rk808->i2c;
	struct regulator_config config = {};
	struct regulator_dev *rk801_rdev;
	struct rk801_regulator_data *pdata;
	int i, ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->req_pwrctrl_dvs = rk808->pwrctrl.req_pwrctrl_dvs;
	pdata->pwrctrl_gpio = rk808->pwrctrl.gpio;
	dev_info(&pdev->dev, "req pwrctrl dvs: %d, act: %s\n",
		 pdata->req_pwrctrl_dvs, rk808->pwrctrl.act_low ? "low" : "high");

	platform_set_drvdata(pdev, pdata);
	config.dev = &client->dev;
	config.driver_data = pdata;
	config.regmap = rk808->regmap;

	ret = rk801_regulator_init(&pdev->dev);
	if (ret)
		return ret;

	for (i = 0; i < RK801_NUM_REGULATORS; i++) {
		rk801_rdev = devm_regulator_register(&pdev->dev,
						     &rk801_reg[i], &config);
		if (IS_ERR(rk801_rdev)) {
			dev_err(&client->dev,
				"failed to register %d regulator\n", i);
			return PTR_ERR(rk801_rdev);
		}
	}

	return 0;
}

static const struct dev_pm_ops rk801_pm_ops = {
	.suspend_late = pm_sleep_ptr(rk801_regulator_suspend_late),
	.resume_early = pm_sleep_ptr(rk801_regulator_resume_early),
};

static struct platform_driver rk801_regulator_driver = {
	.probe = rk801_regulator_probe,
	.driver = {
		.name = "rk801-regulator",
		.pm = pm_sleep_ptr(&rk801_pm_ops),
	},
};

#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT
static int __init rk801_regulator_driver_init(void)
{
	return platform_driver_register(&rk801_regulator_driver);
}
subsys_initcall(rk801_regulator_driver_init);

static void __exit rk801_regulator_driver_exit(void)
{
	platform_driver_unregister(&rk801_regulator_driver);
}
module_exit(rk801_regulator_driver_exit);
#else
module_platform_driver(rk801_regulator_driver);
#endif

MODULE_DESCRIPTION("regulator driver for the RK801 PMIC");
MODULE_AUTHOR("Joseph Chen <chenjh@rock-chips.com>");
MODULE_LICENSE("GPL");
