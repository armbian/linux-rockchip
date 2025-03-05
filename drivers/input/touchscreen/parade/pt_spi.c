/*
 * pt_spi.c
 * Parade TrueTouch(TM) Standard Product SPI Module.
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

#include <linux/spi/spi.h>
#include <linux/version.h>

/* TC3315 - DUT Address (0x24 & 0x07) << 1 = 0x08 for write and 0x09 for read */
#define PT_SPI_WR_OP		0x08 /* r/~w */
#define PT_SPI_RD_OP		0x09
#define PT_SPI_BITS_PER_WORD	8
#define PT_SPI_SYNC_ACK         0x62

#define PT_SPI_CMD_BYTES	0
#define PT_SPI_DATA_SIZE	(2 * 256)
#define PT_SPI_DATA_BUF_SIZE	(PT_SPI_CMD_BYTES + PT_SPI_DATA_SIZE)

#define PT_SPI_OP_SIZE     (1)
#define PT_SPI_DUMMY_READ  (1)
#define PT_SPI_BUFFER_SIZE  \
	(PT_MAX_PIP2_MSG_SIZE + PT_SPI_OP_SIZE + PT_SPI_DUMMY_READ)

static u8 *tmp_rbuf;
static u8 *tmp_wbuf;
DEFINE_MUTEX(pt_spi_bus_lock);


/*******************************************************************************
 * FUNCTION: pt_spi_xfer
 *
 * SUMMARY: Read or write date for SPI device.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *      *dev - pointer to device structure
 *        op - flag to write or read data
 *      *buf - pointer to data buffer
 *    length - data length
 ******************************************************************************/
static int pt_spi_xfer(struct device *dev, u8 op, u8 *buf, int length)
{
	struct spi_device *spi = to_spi_device(dev);
	struct spi_message msg;
	struct spi_transfer xfer;
	int rc;

	memset(&xfer, 0, sizeof(xfer));
	spi_message_init(&msg);

	switch (op) {
	case PT_SPI_RD_OP:
		/* Clear tmp_wbuf with additional OP Code and dummy byte */
		memset(tmp_wbuf, 0, length + 2);
		tmp_wbuf[0] = op;
		/* Total read/write = Read length + Op code + dummy byte */
		xfer.tx_buf = tmp_wbuf;
		xfer.rx_buf = tmp_rbuf;
		xfer.len = length + 2;
		break;
	case PT_SPI_WR_OP:
		memcpy(&tmp_wbuf[1], buf, length);
		tmp_wbuf[0] = op;
		/* Write length + size of Op code */
		xfer.tx_buf = tmp_wbuf;
		xfer.len = length + 1;
		break;
	default:
		rc = -EIO;
		goto exit;
	}

	spi_message_add_tail(&xfer, &msg);
	rc = spi_sync(spi, &msg);

	/* On reads copy only the data content back into the passed in buf */
	if (op == PT_SPI_RD_OP)
		memcpy(buf, &tmp_rbuf[2], length);
exit:
	if (rc < 0)
		pt_debug(dev, DL_ERROR, "%s: spi_sync() error %d\n",
			__func__, rc);

#if 0 /* TODO TC3315 - need to verify the ACK byte */
	if (tmp_rbuf[0] != PT_SPI_SYNC_ACK) {
		pt_debug(dev, DL_ERROR, "%s: r_header = 0x%02X\n", __func__,
			r_header[0]);
		return -EIO;
	}
#endif

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_spi_read_default
 *
 * SUMMARY: Read a certain number of bytes from the SPI bus
 *	NOTE: For TC3315 every response includes a "dummy" prefix byte that
 *		needs to be stipped off before returning buf.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *      *dev  - pointer to Device structure
 *      *buf  - pointer to buffer where the data read will be stored
 *       size - size to be read
 ******************************************************************************/
static int pt_spi_read_default(struct device *dev, void *buf, int size)
{
	int rc = 0;

	if (!buf || !size || size > PT_MAX_PIP2_MSG_SIZE)
		return -EINVAL;

	mutex_lock(&pt_spi_bus_lock);
	rc = pt_spi_xfer(dev, PT_SPI_RD_OP, buf, size);
	mutex_unlock(&pt_spi_bus_lock);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_spi_read_default_nosize
 *
 * SUMMARY: Read from the SPI bus in two transactions first reading the HID
 *	packet size (2 bytes) followed by reading the rest of the packet based
 *	on the size initially read.
 *	NOTE: The empty buffer 'size' was redefined in PIP version 1.7.
 *	NOTE: For TC3315 every response includes a "dummy" prefix byte that
 *		needs to be stipped off before returning buf.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *      *dev  - pointer to Device structure
 *      *buf  - pointer to buffer where the data read will be stored
 *       max  - max size that can be read
 ******************************************************************************/
static int pt_spi_read_default_nosize(struct device *dev, u8 *buf, u32 max)
{
	u32 size;
	int rc = 0;

	if (!buf)
		return 0;

	mutex_lock(&pt_spi_bus_lock);

	/* Separate transaction to retrieve only the length to read */
	rc = pt_spi_xfer(dev, PT_SPI_RD_OP, buf, 2);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: SPI transfer error rc = %d\n",
			 __func__, rc);
		goto exit;
	}

	size = get_unaligned_le16(&buf[0]);
	if (!size || size == 2 || size >= PT_PIP_1P7_EMPTY_BUF)
		goto exit;

	if (size > max || size > PT_MAX_PIP2_MSG_SIZE) {
		pt_debug(dev, DL_ERROR, "%s: Invalid size %d !\n", __func__,
			 size);
		rc = -EINVAL;
		goto exit;
	}

	rc = pt_spi_xfer(dev, PT_SPI_RD_OP, buf, size);
	if (rc)
		pt_debug(dev, DL_ERROR, "%s: SPI transfer error rc = %d\n",
		 __func__, rc);

exit:
	mutex_unlock(&pt_spi_bus_lock);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_spi_write_read_specific
 *
 * SUMMARY: Write the contents of write_buf to the SPI device and then read
 *	the response using pt_spi_read_default_nosize()
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *      *dev       - pointer to Device structure
 *       write_len - length of data buffer write_buf
 *      *write_buf - pointer to buffer to write
 *      *read_buf  - pointer to buffer to read response into
 ******************************************************************************/
static int pt_spi_write_read_specific(struct device *dev, u16 write_len,
		u8 *write_buf, u8 *read_buf)
{
	int rc = 0;

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

	mutex_lock(&pt_spi_bus_lock);
	rc = pt_spi_xfer(dev, PT_SPI_WR_OP, write_buf, write_len);
	if (rc < 0)
		goto error;
	mutex_unlock(&pt_spi_bus_lock);

	if (read_buf)
		rc = pt_spi_read_default_nosize(dev, read_buf,
				PT_SPI_DATA_SIZE);
	return rc;

error:
	mutex_unlock(&pt_spi_bus_lock);
	return rc;
}

static struct pt_bus_ops pt_spi_bus_ops = {
	.bustype = BUS_SPI,
	.read_default = pt_spi_read_default,
	.read_default_nosize = pt_spi_read_default_nosize,
	.write_read_specific = pt_spi_write_read_specific,
};

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
static const struct of_device_id pt_spi_of_match[] = {
	{ .compatible = "parade,pt_spi_adapter", },
	{ }
};
MODULE_DEVICE_TABLE(of, pt_spi_of_match);
#endif

/*******************************************************************************
 * FUNCTION: pt_spi_probe
 *
 * SUMMARY: Probe functon for the SPI module
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *      *spi - pointer to spi device structure
 ******************************************************************************/
static int pt_spi_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	int rc;

	/* Set up SPI*/
	spi->bits_per_word = PT_SPI_BITS_PER_WORD;
	spi->mode = SPI_MODE_0;
	rc = spi_setup(spi);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: SPI setup error %d\n",
			__func__, rc);
		return rc;
	}

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(pt_spi_of_match), dev);
	if (match) {
		rc = pt_devtree_create_and_get_pdata(dev);
		if (rc < 0)
			return rc;
	}
#endif

	/* Add 2 to the length for the 'OP Code' & 'Dummy' prefix bytes */
	tmp_wbuf = kzalloc(PT_SPI_BUFFER_SIZE, GFP_KERNEL);
	tmp_rbuf = kzalloc(PT_SPI_BUFFER_SIZE, GFP_KERNEL);
	if (!tmp_wbuf || !tmp_rbuf)
		return -ENOMEM;


	rc = pt_probe(&pt_spi_bus_ops, &spi->dev, spi->irq,
			  PT_SPI_DATA_BUF_SIZE);

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	if (rc && match)
		pt_devtree_clean_pdata(dev);
#endif


	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_spi_remove
 *
 * SUMMARY: Remove functon for the SPI module
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *      *spi - pointer to spi device structure
 ******************************************************************************/
static int pt_spi_remove(struct spi_device *spi)
{
#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	const struct of_device_id *match;
#endif
	struct device *dev = &spi->dev;
	struct pt_core_data *cd = dev_get_drvdata(dev);

	kfree(tmp_rbuf);
	kfree(tmp_wbuf);


	pt_release(cd);

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
	match = of_match_device(of_match_ptr(pt_spi_of_match), dev);
	if (match)
		pt_devtree_clean_pdata(dev);
#endif

	return 0;
}

static const struct spi_device_id pt_spi_id[] = {
	{ PT_SPI_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, pt_spi_id);

static struct spi_driver pt_spi_driver = {
	.driver = {
		.name = PT_SPI_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.pm = &pt_pm_ops,
#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
		.of_match_table = pt_spi_of_match,
#endif
	},
	.probe = pt_spi_probe,
	.remove = (pt_spi_remove),
	.id_table = pt_spi_id,
};

#if (KERNEL_VERSION(3, 3, 0) <= LINUX_VERSION_CODE)
module_spi_driver(pt_spi_driver);
#else
/*******************************************************************************
 * FUNCTION: pt_spi_init
 *
 * SUMMARY: Initialize function to register spi module to kernel.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 ******************************************************************************/
static int __init pt_spi_init(void)
{
	int err = spi_register_driver(&pt_spi_driver);

	pr_info("%s: Parade TTDL SPI Driver (Build %s) rc=%d\n",
		 __func__, PT_DRIVER_VERSION, err);
	return err;
}
module_init(pt_spi_init);

/*******************************************************************************
 * FUNCTION: pt_spi_exit
 *
 * SUMMARY: Exit function to unregister spi module from kernel.
 *
 ******************************************************************************/
static void __exit pt_spi_exit(void)
{
	spi_unregister_driver(&pt_spi_driver);
}
module_exit(pt_spi_exit);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Parade TrueTouch(R) Standard Product SPI Driver");
MODULE_AUTHOR("Parade Technologies <ttdrivers@paradetech.com>");
