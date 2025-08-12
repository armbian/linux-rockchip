// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Radxa Limited
 *
 * LTE EM05 modem driver
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/circ_buf.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <dt-bindings/gpio/gpio.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/lte.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#define LOG(x...)   pr_info("[lte_em05_modem]: " x)

static struct lte_em05_data *gpdata;
static struct class *modem_class;

static int modem_power = 1;
static int modem_reset = 0;
static int modem_airplane_mode = 0;

static int lte_em05_modem_power_on_off(int on_off)
{
	struct lte_em05_data *pdata = gpdata;

	if (pdata) {
		if (on_off) {
			LOG("%s: power on lte em05 modem.\n", __func__);
			if (pdata->power_gpio) {
				gpiod_direction_output(pdata->power_gpio, 1);
			}
		} else {
			LOG("%s: power off lte em05 modem.\n", __func__);
			if (pdata->power_gpio) {
				gpiod_direction_output(pdata->power_gpio, 0);
			}
		}
	}

	return 0;
}

static ssize_t modem_power_store(struct class *cls,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	int new_state, ret;

	ret = kstrtoint(buf, 10, &new_state);
	if (ret) {
		LOG("%s: kstrtoint error return %d\n", __func__, ret);
		return ret;
	}

	if (new_state == modem_power)
		return count;

	if (new_state == 1) {
		LOG("%s, power on modem.\n", __func__);
		lte_em05_modem_power_on_off(1);
	} else if (new_state == 0) {
		LOG("%s, power off modem.\n", __func__);
		lte_em05_modem_power_on_off(0);
	} else {
		LOG("%s, invalid parameter.\n", __func__);
		new_state = modem_power;
	}

	modem_power = new_state;

	return count;
}

static ssize_t modem_power_show(struct class *cls,
				 struct class_attribute *attr,
				 char *buf)
{
	return sprintf(buf, "%d\n", modem_power);
}

static CLASS_ATTR_RW(modem_power);

static int lte_em05_modem_reset(int on)
{
	struct lte_em05_data *pdata = gpdata;

	if (pdata) {
		if (on) {
			LOG("%s: reset lte em05 modem.\n", __func__);
			if (pdata->reset_gpio) {
				gpiod_direction_output(pdata->reset_gpio, 1);
				msleep(10);
				gpiod_direction_output(pdata->reset_gpio, 0);
				msleep(1000);
				gpiod_direction_output(pdata->reset_gpio, 1);
			}
		}
	}

	return 0;
}

static ssize_t modem_reset_store(struct class *cls,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	int new_state, ret;

	ret = kstrtoint(buf, 10, &new_state);
	if (ret) {
		LOG("%s: kstrtoint error return %d\n", __func__, ret);
		return ret;
	}

	if (new_state == modem_reset)
		return count;

	if (new_state == 1) {
		LOG("%s, reset modem.\n", __func__);
		lte_em05_modem_reset(1);
	} else {
		LOG("%s, invalid parameter.\n", __func__);
	}

	modem_reset = 0;

	return count;
}

static ssize_t modem_reset_show(struct class *cls,
				 struct class_attribute *attr,
				 char *buf)
{
	return sprintf(buf, "%d\n", modem_reset);
}

static CLASS_ATTR_RW(modem_reset);

static int lte_em05_modem_airplane_mode(int enter)
{
	struct lte_em05_data *pdata = gpdata;

	if (pdata) {
		if (enter) {
			LOG("%s: enter airplane mode.\n", __func__);
			if (pdata->airplane_gpio) {
				gpiod_direction_output(pdata->airplane_gpio, 0);
			}
		} else {
			LOG("%s: exit airplane mode.\n", __func__);
			if (pdata->airplane_gpio) {
				gpiod_direction_output(pdata->airplane_gpio, 1);
			}
		}
	}

	return 0;
}

static ssize_t modem_airplane_mode_store(struct class *cls,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	int new_state, ret;

	ret = kstrtoint(buf, 10, &new_state);
	if (ret) {
		LOG("%s: kstrtoint error return %d\n", __func__, ret);
		return ret;
	}

	if (new_state == modem_airplane_mode)
		return count;

	if (new_state == 1) {
		LOG("%s, enter airplane mode.\n", __func__);
		lte_em05_modem_airplane_mode(1);
	} else if (new_state == 0) {
		LOG("%s, exit airplane mode.\n", __func__);
		lte_em05_modem_airplane_mode(0);
	} else {
		LOG("%s, invalid parameter.\n", __func__);
		new_state = modem_airplane_mode;
	}

	modem_airplane_mode = new_state;

	return count;
}

static ssize_t modem_airplane_mode_show(struct class *cls,
				 struct class_attribute *attr,
				 char *buf)
{
	return sprintf(buf, "%d\n", modem_airplane_mode);
}

static CLASS_ATTR_RW(modem_airplane_mode);

static int lte_em05_modem_platdata_parse_dt(struct device *dev,
				   struct lte_em05_data *data)
{
	struct device_node *node = dev->of_node;
	int ret;

	if (!node)
		return -ENODEV;

	memset(data, 0, sizeof(*data));

	data->reset_gpio = devm_gpiod_get_optional(dev, "em05,reset",
						   GPIOD_OUT_LOW);
	if (IS_ERR(data->reset_gpio)) {
		ret = PTR_ERR(data->reset_gpio);
		dev_err(dev, "failed to request em05,reset GPIO: %d\n", ret);
		return ret;
	}

	data->airplane_gpio = devm_gpiod_get_optional(dev, "em05,airplane",
						   GPIOD_OUT_HIGH);
	if (IS_ERR(data->airplane_gpio)) {
		ret = PTR_ERR(data->airplane_gpio);
		dev_err(dev, "failed to request em05,airplane GPIO: %d\n", ret);
		return ret;
	}

	data->power_gpio = devm_gpiod_get_optional(dev, "em05,power",
						   GPIOD_OUT_HIGH);
	if (IS_ERR(data->power_gpio)) {
		ret = PTR_ERR(data->power_gpio);
		dev_err(dev, "failed to request em05,power GPIO: %d\n", ret);
		return ret;
	}

	return 0;
}

static int lte_em05_probe(struct platform_device *pdev)
{
	struct lte_em05_data *pdata;
	int ret = -1;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = lte_em05_modem_platdata_parse_dt(&pdev->dev, pdata);
	if (ret < 0) {
		LOG("%s: No lte em05 modem platform data specified\n", __func__);
		goto err;
	}

	gpdata = pdata;
	pdata->dev = &pdev->dev;

	if (pdata->airplane_gpio)
		gpiod_direction_output(pdata->airplane_gpio, 1);
	if (pdata->reset_gpio)
		gpiod_direction_output(pdata->reset_gpio, 1);
	if (pdata->power_gpio)
		gpiod_direction_output(pdata->power_gpio, 1);

	return 0;

err:
	gpdata = NULL;
	return ret;
}

static int lte_em05_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int lte_em05_resume(struct platform_device *pdev)
{
	return 0;
}

static int lte_em05_remove(struct platform_device *pdev)
{
	struct lte_em05_data *pdata = gpdata;

	if (pdata->airplane_gpio)
		gpiod_direction_output(pdata->airplane_gpio, 0);
	if (pdata->reset_gpio)
		gpiod_direction_output(pdata->reset_gpio, 0);
	if (pdata->power_gpio)
		gpiod_direction_output(pdata->power_gpio, 0);

	gpdata = NULL;

	return 0;
}

static const struct of_device_id modem_platdata_of_match[] = {
	{ .compatible = "lte-em05-modem-platdata" },
	{ }
};
MODULE_DEVICE_TABLE(of, modem_platdata_of_match);

static struct platform_driver lte_em05_driver = {
	.probe		= lte_em05_probe,
	.remove		= lte_em05_remove,
	.suspend	= lte_em05_suspend,
	.resume		= lte_em05_resume,
	.driver	= {
		.name	= "lte-em05-modem-platdata",
		.of_match_table = of_match_ptr(modem_platdata_of_match),
	},
};

static int __init lte_em05_init(void)
{
	int ret;

	modem_class = class_create(THIS_MODULE, "lte_em05_modem");
	ret =  class_create_file(modem_class, &class_attr_modem_power);
	if (ret)
		LOG("Fail to create class modem_power.\n");

	ret =  class_create_file(modem_class, &class_attr_modem_reset);
	if (ret)
		LOG("Fail to create class modem_reset.\n");

	ret =  class_create_file(modem_class, &class_attr_modem_airplane_mode);
	if (ret)
		LOG("Fail to create class modem_airplane_mode.\n");

	return platform_driver_register(&lte_em05_driver);
}

static void __exit lte_em05_exit(void)
{
	platform_driver_unregister(&lte_em05_driver);
}

late_initcall(lte_em05_init);
module_exit(lte_em05_exit);

MODULE_AUTHOR("Stephen <stephen@vamrs.com>");
MODULE_DESCRIPTION("LTE EM05 modem driver");
MODULE_LICENSE("GPL");
