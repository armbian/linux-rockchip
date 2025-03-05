/*
 * pt_i2c.c
 * Parade TrueTouch(TM) Standard Product I2C Module.
 * For use with Parade touchscreen controllers.
 * Supported parts include:
 * TMA5XX
 * TMA448
 * TMA445A
 * TT21XXX
 * TT31XXX
 * TT4XXXX
 * TT7XXX
 * TC3XXX
 *
 * Copyright (C) 2015-2020 Parade Technologies
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Parade Technologies at www.paradetech.com <ttdrivers@paradetech.com>
 */

#include "pt_regs.h"

#include <linux/i2c.h>
#include <linux/version.h>

#define PT_I2C_DATA_SIZE  (2 * 256)


/*******************************************************************************
 * FUNCTION: pt_i2c_read_default
 *
 * SUMMARY: Read a certain number of bytes from the I2C bus
 *
 * PARAMETERS:
 *      *dev  - pointer to Device structure
 *      *buf  - pointer to buffer where the data read will be stored
 *       size - size to be read
 ******************************************************************************/
static int pt_i2c_read_default(struct device *dev, void *buf, int size)
{
	struct i2c_client *client = to_i2c_client(dev);
	int rc;
	int read_size = size;

	if (!buf || !size || size > PT_I2C_DATA_SIZE)
		return -EINVAL;

	rc = i2c_master_recv(client, buf, read_size);

	return (rc < 0) ? rc : rc != read_size ? -EIO : 0;
}

/*******************************************************************************
 * FUNCTION: pt_i2c_read_default_nosize
 *
 * SUMMARY: Read from the I2C bus in two transactions first reading the HID
 *	packet size (2 bytes) followed by reading the rest of the packet based
 *	on the size initially read.
 *	NOTE: The empty buffer 'size' was redefined in PIP version 1.7.
 *
 * PARAMETERS:
 *      *dev  - pointer to Device structure
 *      *buf  - pointer to buffer where the data read will be stored
 *       max  - max size that can be read
 ******************************************************************************/
static int pt_i2c_read_default_nosize(struct device *dev, u8 *buf, u32 max)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;
	u32 size;

	if (!buf)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = (client->flags & I2C_M_TEN) | I2C_M_RD;
	msgs[0].len = 2;
	msgs[0].buf = buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);
	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;

	size = get_unaligned_le16(&buf[0]);
	if (!size || size == 2 || size >= PT_PIP_1P7_EMPTY_BUF)
		/*
		 * Before PIP 1.7, empty buffer is 0x0002;
		 * From PIP 1.7, empty buffer is 0xFFXX
		 */
		return 0;

	if (size > max)
		return -EINVAL;

	rc = i2c_master_recv(client, buf, size);
	return (rc < 0) ? rc : rc != (int)size ? -EIO : 0;
}

/*******************************************************************************
 * FUNCTION: pt_i2c_write_read_specific
 *
 * SUMMARY: Write the contents of write_buf to the I2C device and then read
 *	the response using pt_i2c_read_default_nosize()
 *
 * PARAMETERS:
 *      *dev       - pointer to Device structure
 *       write_len - length of data buffer write_buf
 *      *write_buf - pointer to buffer to write
 *      *read_buf  - pointer to buffer to read response into
 ******************************************************************************/
static int pt_i2c_write_read_specific(struct device *dev, u16 write_len,
		u8 *write_buf, u8 *read_buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;

	/* Ensure no packet larger than what the PIP spec allows */
	if (write_len > PT_MAX_PIP2_MSG_SIZE)
		return -EINVAL;

	if (!write_buf || !write_len) {
		if (!write_buf)
			pt_debug(dev, DL_ERROR,
				"%s write_buf is NULL", __func__);
		if (!write_len)
			pt_debug(dev, DL_ERROR,
				"%s write_len is NULL", __func__);
		return -EINVAL;
	}

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].len = write_len;
	msgs[0].buf = write_buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);

	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;

	rc = 0;

	if (read_buf) {
		rc = pt_i2c_read_default_nosize(dev, read_buf,
				PT_I2C_DATA_SIZE);
	}

	return rc;
}

static struct pt_bus_ops pt_i2c_bus_ops = {
	.bustype = BUS_I2C,
	.read_default = pt_i2c_read_default,
	.read_default_nosize = pt_i2c_read_default_nosize,
	.write_read_specific = pt_i2c_write_read_specific,
};

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
static const struct of_device_id pt_i2c_of_match[] = {
	{ .compatible = "parade,pt_i2c_adapter", },
	{ }
};
MODULE_DEVICE_TABLE(of, pt_i2c_of_match);
#endif


/*******************************************************************************
 * FUNCTION: pt_i2c_probe
 *
 * SUMMARY: Probe functon for the I2C module
 *
 * PARAMETERS:
 *      *client - pointer to i2c client structure
 *      *i2c_id - pointer to i2c device structure
 ******************************************************************************/
static int pt_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	int rc;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pt_debug(dev, DL_ERROR, "I2C functionality not Supported\n");
		return -EIO;
	}

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(pt_i2c_of_match), dev);
	if (match) {
		rc = pt_devtree_create_and_get_pdata(dev);
		if (rc < 0)
			return rc;
	}
#endif


	rc = pt_probe(&pt_i2c_bus_ops, &client->dev, client->irq,
			  PT_I2C_DATA_SIZE);

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	if (rc && match)
		pt_devtree_clean_pdata(dev);
#endif

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_i2c_remove
 *
 * SUMMARY: Remove functon for the I2C module
 *
 * PARAMETERS:
 *      *client - pointer to i2c client structure
 ******************************************************************************/
static void pt_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	struct device *dev = &client->dev;
	struct pt_core_data *cd = i2c_get_clientdata(client);

	pt_release(cd);

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(pt_i2c_of_match), dev);
	if (match)
		pt_devtree_clean_pdata(dev);
#endif
}

static const struct i2c_device_id pt_i2c_id[] = {
	{ PT_I2C_NAME, 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pt_i2c_id);

static struct i2c_driver pt_i2c_driver = {
	.driver = {
		.name = PT_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &pt_pm_ops,
#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
		.of_match_table = pt_i2c_of_match,
#endif
	},
	.probe = pt_i2c_probe,
	.remove = pt_i2c_remove,
	.id_table = pt_i2c_id,
};

#if (KERNEL_VERSION(3, 3, 0) <= LINUX_VERSION_CODE)
module_i2c_driver(pt_i2c_driver);
#else
/*******************************************************************************
 * FUNCTION: pt_i2c_init
 *
 * SUMMARY: Initialize function to register i2c module to kernel.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 ******************************************************************************/
static int __init pt_i2c_init(void)
{
	int rc = i2c_add_driver(&pt_i2c_driver);

	pr_info("%s: Parade TTDL I2C Driver (Build %s) rc=%d\n",
			__func__, PT_DRIVER_VERSION, rc);
	return rc;
}
module_init(pt_i2c_init);

/*******************************************************************************
 * FUNCTION: pt_i2c_exit
 *
 * SUMMARY: Exit function to unregister i2c module from kernel.
 *
 ******************************************************************************/
static void __exit pt_i2c_exit(void)
{
	i2c_del_driver(&pt_i2c_driver);
}
module_exit(pt_i2c_exit);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Parade TrueTouch(R) Standard Product I2C driver");
MODULE_AUTHOR("Parade Technologies <ttdrivers@paradetech.com>");
