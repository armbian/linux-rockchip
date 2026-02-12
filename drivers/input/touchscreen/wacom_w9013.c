/*
 * Wacom Penabled Driver for I2C
 *
 * Copyright (c) 2011 - 2013 Tatsunosuke Tobita, Wacom.
 * <tobita.tatsunosuke@wacom.co.jp>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version of 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/unaligned.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include "tp_suspend.h"

static int screen_max_x = 20280;
static int screen_max_y = 13942;

#define WACOM_CMD_QUERY0	0x04
#define WACOM_CMD_QUERY1	0x00
#define WACOM_CMD_QUERY2	0x33
#define WACOM_CMD_QUERY3	0x02
#define WACOM_CMD_THROW0	0x05
#define WACOM_CMD_THROW1	0x00
#define WACOM_QUERY_SIZE	19

struct wacom_features {
	int x_max;
	int y_max;
	int pressure_max;
	char fw_version;
};

/*HID specific register*/
#define HID_DESC_REGISTER       1
#define COMM_REG                0x04
#define DATA_REG                0x05
#define PEN_TYPE_CODE			0x06

typedef struct hid_descriptor {
	u16 wHIDDescLength;
	u16 bcdVersion;
	u16 wReportDescLength;
	u16 wReportDescRegister;
	u16 wInputRegister;
	u16 wMaxInputLength;
	u16 wOutputRegister;
	u16 wMaxOutputLength;
	u16 wCommandRegister;
	u16 wDataRegister;
	u16 wVendorID;
	u16 wProductID;
	u16 wVersion;
	u16 RESERVED_HIGH;
	u16 RESERVED_LOW;
} HID_DESC;

struct wacom_i2c {
	struct wacom_features *features;
	struct i2c_client *client;
	struct input_dev *input;
	u8 data[WACOM_QUERY_SIZE];
	bool prox;
	int tool;
	struct tp_device tp;
	struct regulator *supply;
	int irq_gpio;
	int pen_detect_gpio;
	int reset_gpio;
	int exchange_x_y_flag;
	int revert_x_flag;
	int revert_y_flag;
	/* changed tower: for first force report x and y. */
	bool first_report_flag;
	/* changed end. */
};

static int get_hid_desc(struct i2c_client *client,
			      struct hid_descriptor *hid_desc)
{
	int ret = -1;
	char cmd[] = {HID_DESC_REGISTER, 0x00};
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = cmd,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(HID_DESC),
			.buf = (char *)hid_desc,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		dev_err(&client->dev, "i2c transfer error! ret = %d\n", ret);
		return ret;
	}
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "i2c transfer failed! transfer length = %d\n", ret);
		return -EIO;
	}

	dev_info(&client->dev, "******************************\n");
	dev_info(&client->dev, "wacom firmware vesrsion:0x%x\n", hid_desc->wVersion);
	dev_info(&client->dev, "******************************\n");

	ret = 0;

	return ret;
}


static int wacom_query_device(struct i2c_client *client,
			      struct wacom_i2c *wacom)
{
	int ret;
	u8 cmd1[] = { WACOM_CMD_QUERY0, WACOM_CMD_QUERY1,
			WACOM_CMD_QUERY2, WACOM_CMD_QUERY3 };
	u8 cmd2[] = { WACOM_CMD_THROW0, WACOM_CMD_THROW1 };
	u8 data[WACOM_QUERY_SIZE];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd1),
			.buf = cmd1,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(cmd2),
			.buf = cmd2,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(data),
			.buf = data,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	wacom->features->x_max = get_unaligned_le16(&data[3]);
	wacom->features->y_max = get_unaligned_le16(&data[5]);
	wacom->features->pressure_max = get_unaligned_le16(&data[11]);
	wacom->features->fw_version = get_unaligned_le16(&data[13]);
	dev_info(&client->dev, "Wacom source screen x_max:%d, y_max:%d, pressure:%d, fw:%d\n",
		 wacom->features->x_max, wacom->features->y_max,
		 wacom->features->pressure_max, wacom->features->fw_version);

	if (1 == wacom->exchange_x_y_flag)
		swap(wacom->features->x_max, wacom->features->y_max);
	screen_max_x = wacom->features->x_max;
	screen_max_y = wacom->features->y_max;
	dev_info(&client->dev, "Wacom desc screen x_max:%d, y_max:%d\n",
		 wacom->features->x_max, wacom->features->y_max);

	return 0;
}

static irqreturn_t wacom_i2c_irq(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	struct input_dev *input = wac_i2c->input;
	//struct wacom_features *features = wac_i2c->features;
	u8 *data = wac_i2c->data;
	unsigned int x, y, pressure;
	short tilt_x, tilt_y;
	unsigned char type, tsw, f1, f2, ers;
	int error;

	if (device_can_wakeup(&wac_i2c->client->dev))
		pm_stay_awake(&wac_i2c->client->dev);
	error = i2c_master_recv(wac_i2c->client,
				wac_i2c->data, sizeof(wac_i2c->data));
	if (error < 0)
		goto out;

	type = data[2];  // RFL series ReportID = 26; CP-9x3 series ReportID = 2
	tsw = data[3] & 0x01;
	ers = data[3] & 0x04;
	f1 = data[3] & 0x02;
	f2 = data[3] & 0x10;
	x = le16_to_cpup((__le16 *)&data[4]);
	y = le16_to_cpup((__le16 *)&data[6]);
	pressure = le16_to_cpup((__le16 *)&data[8]);
	tilt_x = le16_to_cpup((__le16 *)&data[11]);
	tilt_y = le16_to_cpup((__le16 *)&data[13]);

	if (!wac_i2c->prox)
		wac_i2c->tool = (data[3] & 0x0c) ?
			BTN_TOOL_RUBBER : BTN_TOOL_PEN;

	wac_i2c->prox = data[3] & 0x20;

	if (1 == wac_i2c->exchange_x_y_flag)
		swap(x, y);
	if (1 == wac_i2c->revert_x_flag)
		x = screen_max_x - x;
	if (1 == wac_i2c->revert_y_flag)
		y = screen_max_y - y;

	/* changed tower: for first force report x and y. */
	if ((tsw || ers) && !wac_i2c->first_report_flag) {
		input->absinfo[ABS_X].value -= 1;
		input->absinfo[ABS_Y].value -= 1;
		wac_i2c->first_report_flag = true;
	}
	if (!tsw && !ers)
		wac_i2c->first_report_flag = false;
	/* changed end. */

	input_report_key(input, BTN_TOUCH, tsw || ers);
	input_report_key(input, wac_i2c->tool, wac_i2c->prox);
	input_report_key(input, BTN_STYLUS, f1);
	input_report_key(input, BTN_STYLUS2, f2);
	input_event(input, EV_MSC, PEN_TYPE_CODE, type == 26 ? 2 : 1);
	input_report_abs(input, ABS_X, x);
	input_report_abs(input, ABS_Y, y);
	input_report_abs(input, ABS_TILT_X, tilt_x / 100);
	input_report_abs(input, ABS_TILT_Y, tilt_y / 100);
	input_report_abs(input, ABS_PRESSURE, pressure);
	input_sync(input);

out:
	if (device_can_wakeup(&wac_i2c->client->dev))
		pm_relax(&wac_i2c->client->dev);

	return IRQ_HANDLED;
}

static int wacom_i2c_open(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
	struct i2c_client *client = wac_i2c->client;

	enable_irq(client->irq);

	return 0;
}

static void wacom_i2c_close(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
	struct i2c_client *client = wac_i2c->client;

	disable_irq(client->irq);
}

/* changed tower: for update hid info when fw update. */
static ssize_t wacom_fw_update_notice(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wacom_i2c *wpen = i2c_get_clientdata(client);
	HID_DESC hid_desc = {0};
	ssize_t ret = 0;

	dev_info(dev, "update firmwared...\n");
	if (get_hid_desc(client, &hid_desc)) {
		dev_err(dev, "update hid descriptor failed!\n");
	} else {
		// update input version property. /proc/bus/input/devices
		wpen->input->id.version = le16_to_cpu(hid_desc.wVersion);
		dev_info(dev, "new firmwared version: %x\n", wpen->input->id.version);
	}

	ret = snprintf(buf, 10, "%x\n", wpen->input->id.version);

	return ret;
}

static struct device_attribute attributes[] = {
	__ATTR(fw_update, S_IRUSR, wacom_fw_update_notice, NULL),
};

static int add_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++) {
		if (device_create_file(dev, attributes + i))
			goto undo;
	}
	return 0;
undo:
	for (i--; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static void rm_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
}
/* changed end. */

static int __maybe_unused wacom_i2c_suspend(struct tp_device *tp_d)
{
	struct wacom_i2c *wac_i2c = container_of(tp_d, struct wacom_i2c, tp);

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);
	disable_irq(wac_i2c->client->irq);
	gpio_direction_output(wac_i2c->irq_gpio, 0);
	gpio_direction_output(wac_i2c->pen_detect_gpio, 0);
	gpio_direction_output(wac_i2c->reset_gpio, 0);
	if (wac_i2c->supply)
		regulator_disable(wac_i2c->supply);
	return 0;
}

static int __maybe_unused wacom_i2c_resume(struct tp_device *tp_d)
{
	struct wacom_i2c *wac_i2c = container_of(tp_d, struct wacom_i2c, tp);
	int ret;

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);
	if (wac_i2c->supply) {
		ret = regulator_enable(wac_i2c->supply);
		if (ret < 0)
			dev_err(&wac_i2c->client->dev, "failed to enable wacom power supply\n");
	}
	gpio_direction_input(wac_i2c->irq_gpio);
	gpio_direction_input(wac_i2c->pen_detect_gpio);
	gpio_direction_output(wac_i2c->reset_gpio, 0);
	msleep(50);
	gpio_direction_output(wac_i2c->reset_gpio, 1);
	msleep(50);
	enable_irq(wac_i2c->client->irq);

	return 0;
}

static int wacom_i2c_probe(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c;
	struct input_dev *input;
	struct wacom_features features = { 0 };
	HID_DESC hid_desc = { 0 };
	struct device_node *wac_np;
	int error;
	struct regulator *power_supply;

	wac_np = client->dev.of_node;
	if (!wac_np) {
		dev_err(&client->dev, "get device node error\n");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}

	wac_i2c = devm_kzalloc(&client->dev, sizeof(*wac_i2c), GFP_KERNEL);
	if (!wac_i2c)
		return -ENOMEM;

	/* 1.parase dt */
	of_property_read_u32(wac_np, "revert_x", &wac_i2c->revert_x_flag);
	of_property_read_u32(wac_np, "revert_y", &wac_i2c->revert_y_flag);
	of_property_read_u32(wac_np, "xy_exchange", &wac_i2c->exchange_x_y_flag);

	power_supply = devm_regulator_get(&client->dev, "pwr");
	if (power_supply) {
		dev_info(&client->dev, "wacom power supply = %dmv\n", regulator_get_voltage(power_supply));
		error = regulator_enable(power_supply);
		if (error < 0)
			dev_err(&client->dev, "failed to enable wacom power supply\n");
		wac_i2c->supply = power_supply;
	}

	wac_i2c->reset_gpio = of_get_named_gpio(wac_np, "gpio_rst", 0);
	if (!gpio_is_valid(wac_i2c->reset_gpio)) {
		dev_err(&client->dev, "no gpio_rst pin available\n");
		return -ENODEV;
	}

	error = devm_gpio_request_one(&client->dev, wac_i2c->reset_gpio, GPIOF_OUT_INIT_LOW, "gpio-rst");
	if (error < 0) {
		dev_err(&client->dev, "request rst gpio error: %d\n", error);
		return error;
	}
	gpio_direction_output(wac_i2c->reset_gpio, 0);
	msleep(50);
	gpio_direction_output(wac_i2c->reset_gpio, 1);
	msleep(50);

	wac_i2c->pen_detect_gpio = of_get_named_gpio(wac_np, "gpio_detect", 0);
	if (!gpio_is_valid(wac_i2c->pen_detect_gpio)) {
		dev_err(&client->dev, "no pen_detect_gpio pin available\n");
		return -ENODEV;
	}
	error = devm_gpio_request_one(&client->dev, wac_i2c->pen_detect_gpio, GPIOF_IN, "gpio_detect");
	if (error < 0) {
		dev_err(&client->dev, "request detect gpio error: %d\n", error);
		return error;
	}

	wac_i2c->irq_gpio = of_get_named_gpio(wac_np, "gpio_intr", 0);
	if (!gpio_is_valid(wac_i2c->irq_gpio)) {
		dev_err(&client->dev, "no gpio_intr pin available\n");
		return -ENODEV;
	}

	error = devm_gpio_request_one(&client->dev, wac_i2c->irq_gpio, GPIOF_IN, "gpio_intr");
	if (error < 0) {
		dev_err(&client->dev, "request int gpio error: %d\n", error);
		return error;
	}

	client->irq = gpio_to_irq(wac_i2c->irq_gpio);
	if (client->irq < 0) {
		dev_err(&client->dev, "Unable to get irq number for GPIO %d, error %d\n", wac_i2c->irq_gpio, client->irq);
		return -ENODEV;
	}

	wac_i2c->features = &features;
	wac_i2c->client = client;

	/* 2.get device info */
	error = wacom_query_device(client, wac_i2c);
	if (error) {
		dev_err(&client->dev, "query device error: %d\n", error);
		return error;
	}

	error = get_hid_desc(client, &hid_desc);
	if (error) {
		dev_err(&client->dev, "get hid desc error: %d\n", error);
		return error;
	}

	/* 3.register input device */
	input = devm_input_allocate_device(&client->dev);
	if (!input) {
		dev_err(&client->dev, "allocate input device failed!\n");
		return -ENOMEM;
	}
	input->name = "Wacom-pen";
	input->id.bustype = BUS_I2C;
	input->id.vendor = 0x56a;
	input->id.product = 0x56a;
	input->id.version = hid_desc.wVersion;
	input->dev.parent = &client->dev;
	input->open = wacom_i2c_open;
	input->close = wacom_i2c_close;
	input->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) | BIT_MASK(EV_MSC);

	__set_bit(BTN_TOOL_PEN, input->keybit);
	__set_bit(BTN_TOOL_RUBBER, input->keybit);
	__set_bit(BTN_STYLUS, input->keybit);
	__set_bit(BTN_STYLUS2, input->keybit);
	__set_bit(BTN_TOUCH, input->keybit);
	__set_bit(INPUT_PROP_DIRECT, input->propbit);

	input_set_abs_params(input, ABS_X, 0, features.x_max, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, features.y_max, 0, 0);
	input_set_abs_params(input, ABS_PRESSURE, 0, features.pressure_max, 0, 0);
	input_set_abs_params(input, ABS_TILT_X, -90, 90, 0, 0);
	input_set_abs_params(input, ABS_TILT_Y, -90, 90, 0, 0);
	input_set_capability(input, EV_MSC, PEN_TYPE_CODE);

	input_set_drvdata(input, wac_i2c);
	wac_i2c->input = input;
	error = input_register_device(wac_i2c->input);
	if (error) {
		dev_err(&client->dev, "Failed to register input device, error: %d\n", error);
		return error;
	}
	/* 4.init irq */
	error = devm_request_threaded_irq(&client->dev, client->irq, NULL, wacom_i2c_irq,
					  IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_NO_AUTOEN,
					  "wacom", wac_i2c);
	if (error) {
		dev_err(&client->dev,
			"Failed to enable IRQ, error: %d\n", error);
		return error;
	}

	device_init_wakeup(&client->dev, 1);
	enable_irq_wake(client->irq);

	/* 5.add ebc fb notify */
	wac_i2c->tp.tp_resume = wacom_i2c_resume;
	wac_i2c->tp.tp_suspend = wacom_i2c_suspend;
	tp_register_fb(&wac_i2c->tp);

	i2c_set_clientdata(client, wac_i2c);

	error = add_sysfs_interfaces(&client->dev);
	if (error < 0) {
		dev_err(&client->dev, "wacom init sysfs fail!\n");
		tp_unregister_fb(&wac_i2c->tp);
		return error;
	}

	dev_info(&client->dev, "wacom probe ok.\n");

	return 0;
}

static void wacom_i2c_remove(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	rm_sysfs_interfaces(&client->dev);
	tp_unregister_fb(&wac_i2c->tp);
}

static const struct i2c_device_id wacom_i2c_id[] = {
	{ "wacom", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, wacom_i2c_id);

static const struct of_device_id wacom_dt_ids[] = {
	{
		.compatible = "wacom,w9013",
		.data = (void *) &wacom_i2c_id[0],
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, wacom_dt_ids);

static struct i2c_driver wacom_i2c_driver = {
	.driver	= {
		.name	= "wacom",
		.owner	= THIS_MODULE,
		.of_match_table = wacom_dt_ids,
	},

	.probe		= wacom_i2c_probe,
	.remove		= wacom_i2c_remove,
	.id_table	= wacom_i2c_id,
};

static int __init wacom_init(void)
{
	return i2c_add_driver(&wacom_i2c_driver);
}

static void __exit wacom_exit(void)
{
	i2c_del_driver(&wacom_i2c_driver);
}

/*
 * Module entry points
 */
subsys_initcall(wacom_init);
//late_initcall(wacom_init);
module_exit(wacom_exit);

//module_i2c_driver(wacom_i2c_driver);

MODULE_AUTHOR("Tatsunosuke Tobita <tobita.tatsunosuke@wacom.co.jp>");
MODULE_DESCRIPTION("WACOM EMR I2C Driver");
MODULE_LICENSE("GPL");
