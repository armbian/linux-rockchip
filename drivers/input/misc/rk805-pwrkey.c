// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rockchip RK805 PMIC Power Key driver
 *
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Author: Joseph Chen <chenjh@rock-chips.com>
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

static irqreturn_t pwrkey_fall_irq(int irq, void *_pwr)
{
	struct input_dev *pwr = _pwr;

	input_report_key(pwr, KEY_POWER, 1);
	input_sync(pwr);

	return IRQ_HANDLED;
}

static irqreturn_t pwrkey_rise_irq(int irq, void *_pwr)
{
	struct input_dev *pwr = _pwr;

	input_report_key(pwr, KEY_POWER, 0);
	input_sync(pwr);

	return IRQ_HANDLED;
}

static void rk8xx_get_input_info(const char *device_name,
				 const char **input_name,
				 const char **input_phys)
{
	if (strstr(device_name, "rk806")) {
		*input_name = "rk806 pwrkey";
		*input_phys = "rk806_pwrkey/input0";
	} else if (strstr(device_name, "rk809") ||
		   strstr(device_name, "rk817")) {
		*input_name = "rk809 pwrkey";
		*input_phys = "rk809_pwrkey/input0";
	} else if (strstr(device_name, "rk816")) {
		*input_name = "rk816 pwrkey";
		*input_phys = "rk816_pwrkey/input0";
	} else if (strstr(device_name, "rk818")) {
		*input_name = "rk818 pwrkey";
		*input_phys = "rk818_pwrkey/input0";
	} else {
		*input_name = "rk805 pwrkey";
		*input_phys = "rk805_pwrkey/input0";
	}
}

static int rk805_pwrkey_probe(struct platform_device *pdev)
{
	const char *input_name, *input_phys;
	struct input_dev *pwr;
	int fall_irq, rise_irq;
	struct device_node *np;
	int err;

	np = of_get_child_by_name(pdev->dev.parent->of_node, "pwrkey");
	if (np && !of_device_is_available(np)) {
		dev_info(&pdev->dev, "device is disabled\n");
		return -EINVAL;
	}

	rk8xx_get_input_info(pdev->name, &input_name, &input_phys);
	dev_info(&pdev->dev, "Probing %s -> %s\n", pdev->name, input_name);

	pwr = devm_input_allocate_device(&pdev->dev);
	if (!pwr) {
		dev_err(&pdev->dev, "Can't allocate power button\n");
		return -ENOMEM;
	}

	pwr->name = input_name;
	pwr->phys = input_phys;
	pwr->id.bustype = BUS_HOST;
	input_set_capability(pwr, EV_KEY, KEY_POWER);

	fall_irq = platform_get_irq(pdev, 0);
	if (fall_irq < 0)
		return fall_irq;

	rise_irq = platform_get_irq(pdev, 1);
	if (rise_irq < 0)
		return rise_irq;

	err = devm_request_any_context_irq(&pwr->dev, fall_irq,
					   pwrkey_fall_irq,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					   "rk805_pwrkey_fall", pwr);
	if (err < 0) {
		dev_err(&pdev->dev, "Can't register fall irq: %d\n", err);
		return err;
	}

	err = devm_request_any_context_irq(&pwr->dev, rise_irq,
					   pwrkey_rise_irq,
					   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					   "rk805_pwrkey_rise", pwr);
	if (err < 0) {
		dev_err(&pdev->dev, "Can't register rise irq: %d\n", err);
		return err;
	}

	err = input_register_device(pwr);
	if (err) {
		dev_err(&pdev->dev, "Can't register power button: %d\n", err);
		return err;
	}

	platform_set_drvdata(pdev, pwr);
	device_init_wakeup(&pdev->dev, true);

	return 0;
}

static const struct platform_device_id rk8xx_pwrkey_id_table[] = {
	{ "rk801-pwrkey", 0 },
	{ "rk805-pwrkey", 0 },
	{ "rk806-pwrkey", 0 },
	{ "rk816-pwrkey", 0 },
	{ "rk817-pwrkey", 0 },
	{ "rk818-pwrkey", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, rk8xx_pwrkey_id_table);

static struct platform_driver rk805_pwrkey_driver = {
	.probe	= rk805_pwrkey_probe,
	.id_table = rk8xx_pwrkey_id_table,
	.driver	= {
		.name = "rk805-pwrkey",
	},
};
module_platform_driver(rk805_pwrkey_driver);

MODULE_ALIAS("platform:rk805-pwrkey");
MODULE_AUTHOR("Joseph Chen <chenjh@rock-chips.com>");
MODULE_DESCRIPTION("RK805 PMIC Power Key driver");
MODULE_LICENSE("GPL");
