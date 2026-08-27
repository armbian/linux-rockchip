// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include <linux/platform_data/spi-rockchip.h>

#include "spidev-rkspi-obj.h"

#define SPI_OBJ_DEFAULT_TIMEOUT_US	100000

struct spidev_rkmst_data {
	struct device	*dev;
	struct spi_device *spi;
	char *ctrlbuf;
	char *rxbuf;
	char *txbuf;
	struct mutex lock;	/* protects ctrlbuf/rxbuf/txbuf */
	struct gpio_desc *ready;
	int ready_irqnum;
	bool ready_status;
	bool verbose;
	struct miscdevice misc_dev;
};

static u32 bit_per_word = 8;

static inline void spidev_mst_slave_ready_status(struct spidev_rkmst_data *spidev, bool status)
{
	spidev->ready_status = status;
}

static irqreturn_t spidev_mst_slave_ready_interrupt(int irq, void *arg)
{
	struct spidev_rkmst_data *spidev = (struct spidev_rkmst_data *)arg;

	spidev_mst_slave_ready_status(spidev, true);

	return IRQ_HANDLED;
}

static bool spidev_mst_check_slave_ready(struct spidev_rkmst_data *spidev)
{
	return spidev->ready_status;
}

static int spidev_mst_wait_for_slave_ready(struct spidev_rkmst_data *spidev,
					   u32 timeout_us)
{
	bool ready;
	int ret;

	ret = read_poll_timeout(spidev_mst_check_slave_ready, ready,
				ready, 100, timeout_us + 100, false, spidev);
	if (ret) {
		dev_err(&spidev->spi->dev, "timeout and reset slave\n");

		return -ETIMEDOUT;
	}

	return true;
}

static int spidev_mst_write(struct spidev_rkmst_data *spidev, const void *txbuf, size_t n)
{
	struct spi_device *spi = spidev->spi;
	struct spi_transfer t = {
			.tx_buf = txbuf,
			.len = n,
			.bits_per_word = bit_per_word,
		};
	struct spi_message m;
	int ret;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ret = spidev_mst_wait_for_slave_ready(spidev, SPI_OBJ_DEFAULT_TIMEOUT_US);
	if (ret < 0)
		return ret;

	spidev_mst_slave_ready_status(spidev, false);
	ret = spi_sync(spi, &m);

	return ret;
}

static int spidev_mst_write_bypass(struct spidev_rkmst_data *spidev, const void *txbuf, size_t n)
{
	struct spi_device *spi = spidev->spi;
	struct spi_transfer t = {
			.tx_buf = txbuf,
			.len = n,
			.bits_per_word = bit_per_word,
		};
	struct spi_message m;
	int ret;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ret = spi_sync(spi, &m);

	return ret;
}

static int spidev_mst_read(struct spidev_rkmst_data *spidev, void *rxbuf, size_t n)
{
	struct spi_device *spi = spidev->spi;
	struct spi_transfer t = {
			.rx_buf = rxbuf,
			.len = n,
			.bits_per_word = bit_per_word,
		};
	struct spi_message m;
	int ret;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ret = spidev_mst_wait_for_slave_ready(spidev, SPI_OBJ_DEFAULT_TIMEOUT_US);
	if (ret < 0)
		return ret;

	spidev_mst_slave_ready_status(spidev, false);
	ret = spi_sync(spi, &m);

	return ret;
}

static int spidev_slv_write_and_read(struct spidev_rkmst_data *spidev,
				     const void *tx_buf, void *rx_buf,
				     size_t len)
{
	struct spi_device *spi = spidev->spi;
	struct spi_transfer t = {
			.tx_buf = tx_buf,
			.rx_buf = rx_buf,
			.len = len,
			.bits_per_word = bit_per_word,
		};
	struct spi_message m;
	int ret;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ret = spidev_mst_wait_for_slave_ready(spidev, SPI_OBJ_DEFAULT_TIMEOUT_US);
	if (ret < 0)
		return ret;

	spidev_mst_slave_ready_status(spidev, false);
	ret = spi_sync(spi, &m);

	return ret;
}

static void spidev_rkmst_reset_slave(struct spidev_rkmst_data *spidev)
{
	struct spi_obj_ctrl *ctrl = (struct spi_obj_ctrl *)spidev->ctrlbuf;

	ctrl->cmd = SPI_OBJ_CTRL_CMD_INIT;

	spidev_mst_write_bypass(spidev, ctrl, SPI_OBJ_MAX_XFER_SIZE);
	msleep(100);
	spidev_mst_write_bypass(spidev, ctrl, SPI_OBJ_MAX_XFER_SIZE);
}


static int spidev_rkmst_ctrl(struct spidev_rkmst_data *spidev, u32 cmd, u16 addr, u32 data)
{
	struct spi_obj_ctrl *ctrl = (struct spi_obj_ctrl *)spidev->ctrlbuf;
	struct spi_device *spi = spidev->spi;
	int ret;

	if (spidev->verbose)
		dev_err(&spi->dev, "ctrl cmd=%x addr=0x%x data=0x%x\n", cmd, addr, data);

	/* ctrl_xfer */
	ctrl->cmd = cmd;
	ctrl->addr = addr;
	ctrl->data = data;
	ret = spidev_mst_write(spidev, ctrl, SPI_OBJ_CTRL_MSG_SIZE);
	if (ret) {
		dev_err(&spi->dev, "ctrl cmd=%x addr=0x%x data=0x%x, ret=%d\n", cmd, addr, data, ret);
		return -EIO;
	}

	return 0;
}

static int spidev_rkmst_xfer(struct spidev_rkmst_data *spidev, void *tx,
			     void *rx, u16 addr, u32 len)
{
	struct spi_device *spi = spidev->spi;
	int ret;
	u32 cmd;

	if (tx && rx)
		cmd = SPI_OBJ_CTRL_CMD_DUPLEX;
	else if (rx)
		cmd = SPI_OBJ_CTRL_CMD_READ;
	else if (tx)
		cmd = SPI_OBJ_CTRL_CMD_WRITE;
	else
		return -EINVAL;

	/* ctrl_xfer */
	ret = spidev_rkmst_ctrl(spidev, cmd, addr, len);
	if (ret) {
		spidev_rkmst_reset_slave(spidev);
		return -EIO;
	}

	if (spidev->verbose)
		dev_err(&spi->dev, "xfer len=0x%x\n", len);
	/* data_xfer */
	switch (cmd) {
	case SPI_OBJ_CTRL_CMD_READ:
		ret = spidev_mst_read(spidev, rx, len);
		if (ret)
			goto err_out;
		break;
	case SPI_OBJ_CTRL_CMD_WRITE:
		ret = spidev_mst_write(spidev, tx, len);
		if (ret)
			goto err_out;
		break;
	case SPI_OBJ_CTRL_CMD_DUPLEX:
		ret = spidev_slv_write_and_read(spidev, tx, rx, len);
		if (ret)
			goto err_out;
		break;
	default:
		dev_err(&spi->dev, "%s unknown\n", __func__);
	}

	return 0;
err_out:
	dev_err(&spi->dev, "xfer cmd=%x addr=0x%x len=0x%x, ret=%d\n",
		cmd, addr, len, ret);
	/*
	 * Slave stays in the "waiting for data" state, so the next ctrl would
	 * be consumed as data: reset it to recover the protocol.
	 */
	spidev_rkmst_reset_slave(spidev);

	return ret;
}

static int spidev_rkmst_parse_args(char **argv, int argc, int nargs, int *args)
{
	int i;

	if (argc < nargs)
		return -EINVAL;

	for (i = 0; i < nargs; i++) {
		if (kstrtoint(argv[i], 0, &args[i]))
			return -EINVAL;
	}

	return 0;
}

static int spidev_rkmst_check_params(struct spidev_rkmst_data *spidev,
				     int addr, int len, int max_len, int loops)
{
	struct spi_device *spi = spidev->spi;

	if (!spidev_obj_param_valid(addr, len, max_len)) {
		dev_err(&spi->dev, "param invalid, addr=%d len=%d\n", addr, len);
		return -EINVAL;
	}

	if (loops <= 0) {
		dev_err(&spi->dev, "param invalid, loops=%d\n", loops);
		return -EINVAL;
	}

	return 0;
}

static ssize_t spidev_rkmst_cmd_exec(struct spidev_rkmst_data *spidev,
				     const char *cmd, char **argv, int argc,
				     size_t n)
{
	struct spi_device *spi = spidev->spi;
	int args[4];
	int i;

	if (!strcmp(cmd, "verbose")) {
		int val;

		if (spidev_rkmst_parse_args(argv, argc, 1, &val))
			return -EINVAL;

		spidev->verbose = (val == 1);
	} else if (!strcmp(cmd, "init")) {
		int ret;

		ret = spidev_rkmst_ctrl(spidev, SPI_OBJ_CTRL_CMD_INIT, 0x55AA, 0x1234567);
		if (ret)
			return ret;
	} else if (!strcmp(cmd, "read")) {
		int addr, len, ret;

		if (spidev_rkmst_parse_args(argv, argc, 2, args))
			return -EINVAL;

		addr = args[0];
		len = args[1];

		ret = spidev_rkmst_check_params(spidev, addr, len, 0, 1);
		if (ret)
			return ret;

		ret = spidev_rkmst_xfer(spidev, NULL, spidev->rxbuf, addr, len);
		if (ret)
			return ret;

		print_hex_dump(KERN_ERR, "m-r: ",
			       DUMP_PREFIX_OFFSET,
			       16,
			       1,
			       spidev->rxbuf,
			       len,
			       1);
	} else if (!strcmp(cmd, "write")) {
		int addr, len, ret;

		if (spidev_rkmst_parse_args(argv, argc, 2, args))
			return -EINVAL;

		addr = args[0];
		len = args[1];

		ret = spidev_rkmst_check_params(spidev, addr, len,
						SPI_OBJ_MAX_XFER_SIZE, 1);
		if (ret)
			return ret;

		for (i = 0; i < len; i++)
			spidev->txbuf[i] = i & 0xFF;
		((u32 *)spidev->txbuf)[0] = addr;

		ret = spidev_rkmst_xfer(spidev, spidev->txbuf, NULL, addr, len);
		if (ret)
			return ret;
	} else if (!strcmp(cmd, "duplex")) {
		int addr, len, loops, compare, ret;
		char *tempbuf __free(kfree) = NULL;

		if (spidev_rkmst_parse_args(argv, argc, 4, args))
			return -EINVAL;

		addr = args[0];
		len = args[1];
		loops = args[2];
		compare = args[3];

		ret = spidev_rkmst_check_params(spidev, addr, len,
						SPI_OBJ_MAX_XFER_SIZE, loops);
		if (ret)
			return ret;

		tempbuf = kzalloc(len, GFP_KERNEL);
		if (!tempbuf)
			return -ENOMEM;

		get_random_bytes(spidev->txbuf, len);
		ret = spidev_rkmst_xfer(spidev, spidev->txbuf, spidev->rxbuf,
					addr, len);
		if (ret)
			return ret;

		memcpy(tempbuf, spidev->txbuf, len);
		for (i = 0; i < loops; i++) {
			get_random_bytes(spidev->txbuf, len);
			ret = spidev_rkmst_xfer(spidev, spidev->txbuf,
						spidev->rxbuf, addr, len);
			if (ret)
				return ret;

			if (compare && memcmp(spidev->rxbuf, tempbuf, len)) {
				dev_err(&spi->dev, "duplex autotest failed, loops=%d\n", i);
				print_hex_dump(KERN_ERR, "m-d-r: ",
					       DUMP_PREFIX_OFFSET,
					       16,
					       1,
					       spidev->rxbuf,
					       len,
					       1);
				print_hex_dump(KERN_ERR, "m-d-c: ",
					       DUMP_PREFIX_OFFSET,
					       16,
					       1,
					       tempbuf,
					       len,
					       1);
				ret = -EIO;
				return ret;
			}

			memcpy(tempbuf, spidev->txbuf, len);
		}
		dev_info(&spi->dev, "duplex test pass, loops=%d\n", loops);

		return n;
	} else if (!strcmp(cmd, "autotest")) {
		int addr = 0, len, loops, ret;
		unsigned long long bytes = 0;
		unsigned long us = 0;
		ktime_t start_time;
		ktime_t end_time;
		ktime_t cost_time;
		char *tempbuf __free(kfree) = NULL;

		if (spidev_rkmst_parse_args(argv, argc, 2, args))
			return -EINVAL;

		len = args[0];
		loops = args[1];

		ret = spidev_rkmst_check_params(spidev, addr, len,
						SPI_OBJ_MAX_XFER_SIZE, loops);
		if (ret)
			return ret;

		tempbuf = kzalloc(len, GFP_KERNEL);
		if (!tempbuf)
			return -ENOMEM;

		get_random_bytes(tempbuf, len);
		ret = spidev_rkmst_xfer(spidev, tempbuf, NULL, addr, len);
		if (ret)
			return ret;
		start_time = ktime_get();
		for (i = 0; i < loops; i++) {
			get_random_bytes(spidev->txbuf, len);
			ret = spidev_rkmst_xfer(spidev, spidev->txbuf, spidev->rxbuf,
						addr, len);
			if (ret)
				return ret;

			if (memcmp(spidev->rxbuf, tempbuf, len)) {
				dev_err(&spi->dev, "duplex autotest failed, loops=%d\n", i);
				print_hex_dump(KERN_ERR, "m-d-t: ",
					       DUMP_PREFIX_OFFSET,
					       16,
					       1,
					       spidev->txbuf,
					       len,
					       1);
				print_hex_dump(KERN_ERR, "m-d-r: ",
					       DUMP_PREFIX_OFFSET,
					       16,
					       1,
					       spidev->rxbuf,
					       len,
					       1);
				print_hex_dump(KERN_ERR, "m-d-c: ",
					       DUMP_PREFIX_OFFSET,
					       16,
					       1,
					       tempbuf,
					       len,
					       1);
				return -EIO;
			}
			memcpy(tempbuf, spidev->txbuf, len);
		}
		end_time = ktime_get();
		cost_time = ktime_sub(end_time, start_time);
		us = ktime_to_us(cost_time);
		if (!us)
			us = 1;

		bytes = (u64)len * (u64)loops * 1000;
		do_div(bytes, us);
		if (i >= loops)
			dev_err(&spi->dev, "duplex test pass, cost=%ldus, speed=%lldKB/S, %ldus/loops\n",
				us, bytes, us / loops);

		start_time = ktime_get();
		for (i = 0; i < loops; i++) {
			get_random_bytes(spidev->txbuf, len);
			ret = spidev_rkmst_xfer(spidev, spidev->txbuf, NULL, addr, len);
			if (ret)
				return ret;
			ret = spidev_rkmst_xfer(spidev, NULL, spidev->rxbuf, addr, len);
			if (ret)
				return ret;
			if (memcmp(spidev->rxbuf, spidev->txbuf, len)) {
				dev_err(&spi->dev, "read/write autotest failed, loops=%d\n", i);
				print_hex_dump(KERN_ERR, "m-d-t: ",
					       DUMP_PREFIX_OFFSET,
					       16,
					       1,
					       spidev->txbuf,
					       len,
					       1);
				print_hex_dump(KERN_ERR, "m-d-r: ",
					       DUMP_PREFIX_OFFSET,
					       16,
					       1,
					       spidev->rxbuf,
					       len,
					       1);
				return -EIO;
			}
		}
		end_time = ktime_get();
		cost_time = ktime_sub(end_time, start_time);
		us = ktime_to_us(cost_time);
		if (!us)
			us = 1;

		bytes = (u64)len * (u64)loops * 2 * 1000; /* multi 2 for both write and read */
		do_div(bytes, us);
		if (i >= loops)
			dev_err(&spi->dev, "read/write test pass, cost=%ldus, speed=%lldKB/S, %ldus/loops\n",
				us, bytes, us / loops);

		return n;
	} else {
		dev_err(&spi->dev, "unknown command\n");
	}

	return n;
}

static ssize_t spidev_rkmst_misc_write(struct file *filp, const char __user *buf,
				       size_t n, loff_t *offset)
{
	struct spidev_rkmst_data *spidev;
	char tmp[64];
	char *argv[16];
	char *cmd, *data;
	int argc = 0;
	ssize_t ret;

	if (!n || n >= 64)
		return -EINVAL;

	spidev = filp->private_data;

	if (!spidev)
		return -ESHUTDOWN;

	memset(tmp, 0, sizeof(tmp));
	if (copy_from_user(tmp, buf, n))
		return -EFAULT;
	cmd = tmp;
	data = tmp;

	while (data < (tmp + n)) {
		data = strstr(data, " ");
		if (!data)
			break;
		*data = 0;
		argv[argc] = ++data;
		argc++;
		if (argc >= 16)
			break;
	}

	tmp[n - 1] = 0;

	/* cmd handling shares ctrlbuf/rxbuf/txbuf, serialize concurrent writers */
	if (mutex_lock_interruptible(&spidev->lock))
		return -ERESTARTSYS;

	ret = spidev_rkmst_cmd_exec(spidev, cmd, argv, argc, n);

	mutex_unlock(&spidev->lock);

	return ret;
}

static int spidev_rkmst_misc_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct spidev_rkmst_data *spidev;

	spidev = container_of(miscdev, struct spidev_rkmst_data, misc_dev);
	filp->private_data = spidev;

	return 0;
}

static const struct file_operations spidev_rkmst_misc_fops = {
	.write = spidev_rkmst_misc_write,
	.open = spidev_rkmst_misc_open,
};

static int spidev_rkmst_probe(struct spi_device *spi)
{
	struct spidev_rkmst_data *spidev = NULL;
	int ret;

	if (!spi)
		return -ENOMEM;

	spidev = devm_kzalloc(&spi->dev, sizeof(struct spidev_rkmst_data), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;

	spidev->ctrlbuf = devm_kzalloc(&spi->dev, SPI_OBJ_MAX_XFER_SIZE, GFP_KERNEL);
	if (!spidev->ctrlbuf)
		return -ENOMEM;

	spidev->rxbuf = devm_kzalloc(&spi->dev, SPI_OBJ_APP_RAM_SIZE, GFP_KERNEL | GFP_DMA);
	if (!spidev->rxbuf)
		return -ENOMEM;

	spidev->txbuf = devm_kzalloc(&spi->dev, SPI_OBJ_MAX_XFER_SIZE, GFP_KERNEL);
	if (!spidev->txbuf)
		return -ENOMEM;

	spidev->spi = spi;
	spidev->dev = &spi->dev;
	mutex_init(&spidev->lock);

	spidev_mst_slave_ready_status(spidev, false);
	spidev->ready = devm_gpiod_get_optional(&spi->dev, "ready", GPIOD_IN);
	if (IS_ERR(spidev->ready))
		return dev_err_probe(&spi->dev, PTR_ERR(spidev->ready),
				     "invalid ready-gpios property in node\n");

	spidev->ready_irqnum = gpiod_to_irq(spidev->ready);
	ret = devm_request_irq(&spi->dev, spidev->ready_irqnum, spidev_mst_slave_ready_interrupt,
		IRQF_TRIGGER_FALLING, "spidev-mst-ready-in", spidev);
	if (ret < 0) {
		dev_err(&spi->dev, "request ready irq failed\n");
		return ret;
	}
	dev_set_drvdata(&spi->dev, spidev);

	dev_err(&spi->dev, "mode=%d, max_speed_hz=%d\n", spi->mode, spi->max_speed_hz);

	spidev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	spidev->misc_dev.name = "spidev_rkmst_misc";
	spidev->misc_dev.fops = &spidev_rkmst_misc_fops;
	spidev->misc_dev.parent = &spi->dev;
	ret = misc_register(&spidev->misc_dev);
	if (ret) {
		dev_err(&spi->dev, "fail to register misc device\n");
		return ret;
	}

	spidev_rkmst_reset_slave(spidev);

	return 0;
}

static void spidev_rkmst_remove(struct spi_device *spi)
{
	struct spidev_rkmst_data *spidev = dev_get_drvdata(&spi->dev);

	misc_deregister(&spidev->misc_dev);
}

#ifdef CONFIG_OF
static const struct of_device_id spidev_rkmst_dt_match[] = {
	{ .compatible = "rockchip,spi-obj-master", },
	{},
};
MODULE_DEVICE_TABLE(of, spidev_rkmst_dt_match);

#endif /* CONFIG_OF */

static const struct spi_device_id spidev_rkmst_spi_ids[] = {
	{ .name = "spi-obj-master" },
	{},
};
MODULE_DEVICE_TABLE(spi, spidev_rkmst_spi_ids);

static struct spi_driver spidev_rkmst_driver = {
	.driver = {
		.name	= "spidev_rkmst",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(spidev_rkmst_dt_match),
	},
	.probe		= spidev_rkmst_probe,
	.remove		= spidev_rkmst_remove,
	.id_table	= spidev_rkmst_spi_ids,
};
module_spi_driver(spidev_rkmst_driver);

MODULE_AUTHOR("Jon Lin <jon.lin@rock-chips.com>");
MODULE_DESCRIPTION("ROCKCHIP SPI Object Master Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spidev_rkmst");
