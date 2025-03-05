// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim Max96715 GMSL1 Serializer driver
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/compat.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>

#include "maxim_remote.h"

#define DRIVER_VERSION			KERNEL_VERSION(3, 0x02, 0x00)

#define MAX96715_NAME			"maxim-max96715"

#define MAX96715_I2C_ADDR_DEF		0x40

#define MAX96715_CHIP_ID		0x45
#define MAX96715_REG_CHIP_ID		0x1E

/* Config and Video mode switch */
#define MAX96715_MODE_SWITCH		1

enum {
	LINK_MODE_VIDEO = 0,
	LINK_MODE_CONFIG,
};

/* register address: 8bit */
#define MAX96715_I2C_REG_ADDR_08BITS	1

/* register value: 8bit */
#define MAX96715_I2C_REG_VALUE_08BITS	1

/* Write registers up to 4 at a time */
static int max96715_i2c_write(struct i2c_client *client,
		u16 reg_addr, u16 reg_len, u32 val_len, u32 reg_val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	dev_info(&client->dev, "i2c addr(0x%02x) write: 0x%04x (%d) = 0x%08x (%d)\n",
			client->addr, reg_addr, reg_len, reg_val, val_len);

	if (val_len > 4)
		return -EINVAL;

	if (reg_len == 2) {
		buf[0] = reg_addr >> 8;
		buf[1] = reg_addr & 0xff;

		buf_i = 2;
	} else {
		buf[0] = reg_addr & 0xff;

		buf_i = 1;
	}

	val_be = cpu_to_be32(reg_val);
	val_p = (u8 *)&val_be;
	val_i = 4 - val_len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	if (i2c_master_send(client, buf, (val_len + reg_len)) != (val_len + reg_len)) {
		dev_err(&client->dev,
			"%s: writing register 0x%04x from 0x%02x failed\n",
			__func__, reg_addr, client->addr);
		return -EIO;
	}

	return 0;
}

/* Read registers up to 4 at a time */
static int max96715_i2c_read(struct i2c_client *client,
		u16 reg_addr, u16 reg_len, u32 val_len, u32 *reg_val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg_addr);
	u8 *reg_be_p;
	int ret;

	if (val_len > 4 || !val_len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	reg_be_p = (u8 *)&reg_addr_be;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = reg_len;
	msgs[0].buf = &reg_be_p[2 - reg_len];

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = val_len;
	msgs[1].buf = &data_be_p[4 - val_len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev,
			"%s: reading register 0x%04x from 0x%02x failed\n",
			__func__, reg_addr, client->addr);
		return -EIO;
	}

	*reg_val = be32_to_cpu(data_be);

#if 0
	dev_info(&client->dev, "i2c addr(0x%02x) read: 0x%04x (%d) = 0x%08x (%d)\n",
		client->addr, reg_addr, reg_len, *reg_val, val_len);
#endif

	return 0;
}

/* Update registers up to 4 at a time */
static int max96715_i2c_update(struct i2c_client *client,
		u16 reg_addr, u16 reg_len, u32 val_len, u32 val_mask, u32 reg_val)
{
	u32 value;
	int ret;

	ret = max96715_i2c_read(client, reg_addr, reg_len, val_len, &value);
	if (ret)
		return ret;

	value &= ~val_mask;
	value |= (reg_val & val_mask);
	ret = max96715_i2c_write(client, reg_addr, reg_len, val_len, value);

	return ret;
}

static int max96715_write_reg(struct i2c_client *client,
				u16 reg_addr, u8 reg_val)
{
	int ret = 0;

	ret = max96715_i2c_write(client,
			reg_addr, MAX96715_I2C_REG_ADDR_08BITS,
			MAX96715_I2C_REG_VALUE_08BITS, reg_val);

	return ret;
}

static int max96715_read_reg(struct i2c_client *client,
				u16 reg_addr, u8 *reg_val)
{
	int ret = 0;
	u32 value = 0;
	u8 *value_be_p = (u8 *)&value;

	ret = max96715_i2c_read(client,
			reg_addr, MAX96715_I2C_REG_ADDR_08BITS,
			MAX96715_I2C_REG_VALUE_08BITS, &value);

	*reg_val = *value_be_p;

	return ret;
}

static int __maybe_unused max96715_update_reg(struct i2c_client *client,
				u16 reg_addr, u8 val_mask, u8 reg_val)
{
	u8 value;
	int ret;

	ret = max96715_read_reg(client, reg_addr, &value);
	if (ret)
		return ret;

	value &= ~val_mask;
	value |= (reg_val & val_mask);
	ret = max96715_write_reg(client, reg_addr, value);

	return ret;
}

static int max96715_i2c_write_array(struct i2c_client *client,
			const struct maxim_remote_regval *regs)
{
	u32 i = 0;
	int ret = 0;

	for (i = 0; (ret == 0) && (regs[i].reg_addr != MAXIM_REMOTE_REG_NULL); i++) {
		if (regs[i].val_mask != 0)
			ret = max96715_i2c_update(client,
					regs[i].reg_addr, regs[i].reg_len,
					regs[i].val_len, regs[i].val_mask, regs[i].reg_val);
		else
			ret = max96715_i2c_write(client,
					regs[i].reg_addr, regs[i].reg_len,
					regs[i].val_len, regs[i].reg_val);

		if (regs[i].delay != 0)
			usleep_range(regs[i].delay * 1000, regs[i].delay * 1000 + 100);
	}

	return ret;
}

static int max96715_run_ser_init_seq(struct i2c_client *client,
			struct maxim_remote_init_seq *init_seq)
{
	int ret = 0;

	if ((init_seq == NULL) || (init_seq->reg_init_seq == NULL))
		return 0;

	ret = max96715_i2c_write_array(client,
			init_seq->reg_init_seq);

	return ret;
}

static int max96715_load_ser_init_seq(maxim_remote_ser_t *max96715)
{
	struct device *dev = &max96715->client->dev;
	struct device_node *node = NULL;
	int ret = 0;

	node = of_get_child_by_name(dev->of_node, "ser-init-sequence");
	if (!IS_ERR_OR_NULL(node)) {
		dev_info(dev, "load ser-init-sequence\n");

		ret = maxim_remote_load_init_seq(dev, node,
					&max96715->ser_init_seq);

		of_node_put(node);
		return ret;
	}

	return 0;
}

static int __maybe_unused max96715_link_mode_select(maxim_remote_ser_t *max96715, u32 mode)
{
	struct i2c_client *client = max96715->client;
	struct device *dev = &client->dev;
	u8 reg_mask = 0, reg_value = 0;
	u32 delay_ms = 0;
	int ret = 0;

	dev_dbg(dev, "%s: mode = %d\n", __func__, mode);

	reg_mask = BIT(7) | BIT(6);
	if (mode == LINK_MODE_CONFIG) {
		reg_value = BIT(6);
		delay_ms = 5;
	} else {
		reg_value = BIT(7);
		delay_ms = 50;
	}
	ret |= max96715_update_reg(client, 0x04, reg_mask, reg_value);

	msleep(delay_ms);

	return ret;
}

static int max96715_i2c_addr_remap(maxim_remote_ser_t *max96715)
{
	struct i2c_client *client = max96715->client;
	struct device *dev = &client->dev;
	u16 i2c_8bit_addr = 0;
	int ret = 0;

	// Serializer i2c address map
	if (max96715->ser_i2c_addr_map != max96715->ser_i2c_addr_def) {
		dev_info(dev, "Serializer i2c address remap\n");

		maxim_remote_ser_i2c_addr_select(max96715, MAXIM_REMOTE_I2C_SER_DEF);

		i2c_8bit_addr = (max96715->ser_i2c_addr_map << 1);
		ret = max96715_write_reg(client, 0x00, i2c_8bit_addr);
		if (ret) {
			dev_err(dev, "ser i2c address map setting error!\n");
			return ret;
		}

		maxim_remote_ser_i2c_addr_select(max96715, MAXIM_REMOTE_I2C_SER_MAP);
	}

	// Camera i2c address map
	if (max96715->cam_i2c_addr_map != max96715->cam_i2c_addr_def) {
		dev_info(dev, "Camera i2c address remap\n");

		i2c_8bit_addr = (max96715->cam_i2c_addr_map << 1);
		ret = max96715_write_reg(client, 0x09, i2c_8bit_addr);
		if (ret) {
			dev_err(dev, "cam i2c address source setting error!\n");
			return ret;
		}

		i2c_8bit_addr = (max96715->cam_i2c_addr_def << 1);
		ret = max96715_write_reg(client, 0x0A, i2c_8bit_addr);
		if (ret) {
			dev_err(dev, "cam i2c address destination setting error!\n");
			return ret;
		}
	}

	return 0;
}

static int max96715_i2c_addr_def(maxim_remote_ser_t *max96715)
{
	struct i2c_client *client = max96715->client;
	struct device *dev = &client->dev;
	u16 i2c_8bit_addr = 0;
	int ret = 0;

	if (max96715->ser_i2c_addr_map) {
		dev_info(dev, "Serializer i2c address def\n");

		maxim_remote_ser_i2c_addr_select(max96715, MAXIM_REMOTE_I2C_SER_MAP);

		i2c_8bit_addr = (max96715->ser_i2c_addr_def << 1);
		ret = max96715_write_reg(client, 0x00, i2c_8bit_addr);
		if (ret) {
			dev_err(dev, "ser i2c address def setting error!\n");
			return ret;
		}

		maxim_remote_ser_i2c_addr_select(max96715, MAXIM_REMOTE_I2C_SER_DEF);
	}

	return 0;
}

static int max96715_check_chipid(maxim_remote_ser_t *max96715)
{
	struct i2c_client *client = max96715->client;
	struct device *dev = &client->dev;
	u8 chip_id;
	int ret = 0;

	// max96715
	ret = max96715_read_reg(client, MAX96715_REG_CHIP_ID, &chip_id);
	if (ret != 0) {
		dev_info(dev, "Retry check chipid using map address\n");
		maxim_remote_ser_i2c_addr_select(max96715, MAXIM_REMOTE_I2C_SER_MAP);
		ret = max96715_read_reg(client, MAX96715_REG_CHIP_ID, &chip_id);
		if (ret != 0) {
			dev_err(dev, "MAX96715 detect error, ret(%d)\n", ret);
			maxim_remote_ser_i2c_addr_select(max96715, MAXIM_REMOTE_I2C_SER_DEF);

			return -ENODEV;
		}

		max96715_i2c_addr_def(max96715);
	}

	if (chip_id == max96715->chip_id) {
		if (chip_id == MAX96715_CHIP_ID) {
			dev_info(dev, "MAX96715 is Detected\n");
			return 0;
		}
	}

	dev_err(dev, "Unexpected MAX96715 chipid = %02x\n", chip_id);
	return -ENODEV;
}

/*
 * max96715_pclk_detect() - Detect valid pixel clock from image sensor
 *
 * Wait up to 500ms for a valid pixel clock.
 *
 * Returns 0 for success, < 0 for pixel clock not properly detected
 */
static int max96715_pclk_detect(maxim_remote_ser_t *max96715)
{
	struct i2c_client *client = max96715->client;
	struct device *dev = &client->dev;
	u8 pclk_det = 0;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < 500; i++) {
		ret = max96715_read_reg(client, 0x15, &pclk_det);

		if ((ret == 0) && (pclk_det & BIT(0))) {
			dev_info(dev, "Detect valid PCLK (%d)\n", i);
			return 0;
		}

		usleep_range(1000, 1100);
	}

	dev_err(dev, "Unable to detect valid PCLK, timeout\n");

	return -EIO;
}

static int max96715_soft_power_down(maxim_remote_ser_t *max96715)
{
	struct i2c_client *client = max96715->client;
	struct device *dev = &client->dev;
	int ret = 0;

	ret = max96715_write_reg(client, 0x13, BIT(7));
	if (ret) {
		dev_err(dev, "soft power down setting error!\n");
		return ret;
	}

	return 0;
}

static int max96715_module_init(maxim_remote_ser_t *max96715)
{
	struct i2c_client *client = max96715->client;
	int ret = 0;

	ret = maxim_remote_ser_i2c_addr_select(max96715, MAXIM_REMOTE_I2C_SER_DEF);
	if (ret)
		return ret;

	ret = max96715_check_chipid(max96715);
	if (ret)
		return ret;

	ret = max96715_i2c_addr_remap(max96715);
	if (ret)
		return ret;

#if MAX96715_MODE_SWITCH
	ret = max96715_link_mode_select(max96715, LINK_MODE_CONFIG);
	if (ret)
		return ret;
#endif

	ret = max96715_run_ser_init_seq(client, &max96715->ser_init_seq);
	if (ret) {
		dev_err(&client->dev, "run ser init sequence error\n");
		return ret;
	}

#if MAX96715_MODE_SWITCH
	ret = max96715_link_mode_select(max96715, LINK_MODE_VIDEO);
	if (ret)
		return ret;
#endif

	max96715->ser_state = MAXIM_REMOTE_SER_INIT;

	return 0;
}

static int max96715_module_deinit(maxim_remote_ser_t *max96715)
{
	int ret = 0;

#if 0
	ret |= max96715_i2c_addr_def(max96715);
#endif
	ret |= max96715_soft_power_down(max96715);

	max96715->ser_state = MAXIM_REMOTE_SER_DEINIT;

	return ret;
}

static struct maxim_remote_ser_ops max96715_ser_ops = {
	.ser_module_init = max96715_module_init,
	.ser_module_deinit = max96715_module_deinit,
	.ser_pclk_detect = max96715_pclk_detect,
};

static int max96715_parse_dt(maxim_remote_ser_t *max96715)
{
	struct device *dev = &max96715->client->dev;
	struct device_node *of_node = dev->of_node;
	u32 value = 0;
	int ret = 0;

	dev_info(dev, "=== max96715 parse dt ===\n");

	ret = of_property_read_u32(of_node, "ser-i2c-addr-def", &value);
	if (ret == 0) {
		dev_info(dev, "ser-i2c-addr-def property: 0x%x", value);
		max96715->ser_i2c_addr_def = value;
	} else {
		max96715->ser_i2c_addr_def = MAX96715_I2C_ADDR_DEF;
	}

	return 0;
}

static int max96715_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	maxim_remote_ser_t *max96715 = NULL;
	u32 chip_id;

	dev_info(dev, "driver version: %02x.%02x.%02x", DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	chip_id = (uintptr_t)of_device_get_match_data(dev);
	if (chip_id == MAX96715_CHIP_ID) {
		dev_info(dev, "driver for max96715\n");
	} else {
		dev_err(dev, "driver unknown chip\n");
		return -EINVAL;
	}

	max96715 = devm_kzalloc(dev, sizeof(*max96715), GFP_KERNEL);
	if (!max96715) {
		dev_err(dev, "max96715 probe no memory error\n");
		return -ENOMEM;
	}

	max96715->client = client;
	max96715->chip_id = chip_id;
	max96715->ser_i2c_addr_map = client->addr;
	max96715->ser_ops = &max96715_ser_ops;
	max96715->ser_state = MAXIM_REMOTE_SER_DEINIT;

	i2c_set_clientdata(client, max96715);

	mutex_init(&max96715->mutex);

	max96715_parse_dt(max96715);

	max96715_load_ser_init_seq(max96715);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static int max96715_remove(struct i2c_client *client)
#else
static void max96715_remove(struct i2c_client *client)
#endif
{
	maxim_remote_ser_t *max96715 = i2c_get_clientdata(client);

	mutex_destroy(&max96715->mutex);

	max96715->ser_state = MAXIM_REMOTE_SER_DEINIT;

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id max96715_of_match[] = {
	{
		.compatible = "maxim,ser,max96715",
		.data = (const void *)MAX96715_CHIP_ID
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, max96715_of_match);

static struct i2c_driver max96715_i2c_driver = {
	.driver = {
		.name = MAX96715_NAME,
		.of_match_table = of_match_ptr(max96715_of_match),
	},
	.probe		= &max96715_probe,
	.remove		= &max96715_remove,
};

module_i2c_driver(max96715_i2c_driver);

MODULE_AUTHOR("Cai Wenzhong <cwz@rock-chips.com>");
MODULE_DESCRIPTION("Maxim MAX96715 Serializer Driver");
MODULE_LICENSE("GPL");
