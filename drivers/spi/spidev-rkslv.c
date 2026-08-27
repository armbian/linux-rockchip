// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include <soc/rockchip/rockchip-system-status.h>

#include "spidev-rkspi-obj.h"

#define SPI_OBJ_CTRL_DYQ_PERF_THRESHOLD  0x20

struct spidev_rkslv_data {
	struct device	*dev;
	struct spi_device *spi;
	char *ctrlbuf;
	char *appmem;
	char *tempbuf;
	bool verbose;
	struct task_struct *tsk;
	bool tsk_run;
	struct miscdevice misc_dev;

	/*
	 * If the DRAM frequency conversion jitters during the transmission process,
	 * it will cause the DMA to be unable to transport SPI FIFO data in a timely
	 * manner, resulting in FIFO overflow/underflow.
	 *
	 * However, since the command packet length is smaller than FIFO, this
	 * problem does not exist. So set performance status dynamically for data packet.
	 */
	bool dyq_perf;
};

static u32 bit_per_word = 8;

static bool spidev_rkslv_ctrl_valid(u16 cmd, u16 addr, u32 len)
{
	/* only READ/WRITE/DUPLEX carry addr/len, let INIT and unknown pass */
	if (cmd != SPI_OBJ_CTRL_CMD_READ && cmd != SPI_OBJ_CTRL_CMD_WRITE &&
	    cmd != SPI_OBJ_CTRL_CMD_DUPLEX)
		return true;

	if (!len || len > SPI_OBJ_APP_RAM_SIZE ||
	    addr > SPI_OBJ_APP_RAM_SIZE - len)
		return false;

	/*
	 * mirror master side: WRITE/DUPLEX data phase is capped at
	 * SPI_OBJ_MAX_XFER_SIZE, READ may reach SPI_OBJ_APP_RAM_SIZE
	 */
	if (cmd != SPI_OBJ_CTRL_CMD_READ && len > SPI_OBJ_MAX_XFER_SIZE)
		return false;

	return true;
}

static int spidev_slv_write(struct spidev_rkslv_data *spidev, const void *txbuf, size_t len)
{
	struct spi_device *spi = spidev->spi;
	struct spi_transfer t = {
			.tx_buf = txbuf,
			.len = len,
			.bits_per_word = bit_per_word,
		};
	struct spi_message m;
	int ret;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	if (spidev->dyq_perf && len > SPI_OBJ_CTRL_DYQ_PERF_THRESHOLD)
		rockchip_set_system_status(SYS_STATUS_PERFORMANCE);
	ret = spi_sync(spi, &m);
	if (spidev->dyq_perf && len > SPI_OBJ_CTRL_DYQ_PERF_THRESHOLD)
		rockchip_clear_system_status(SYS_STATUS_PERFORMANCE);

	return ret;
}

static int spidev_slv_read(struct spidev_rkslv_data *spidev, void *rxbuf, size_t len)
{
	struct spi_device *spi = spidev->spi;
	struct spi_transfer t = {
			.rx_buf = rxbuf,
			.len = len,
			.bits_per_word = bit_per_word,
		};
	struct spi_message m;
	int ret;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	if (spidev->dyq_perf && len > SPI_OBJ_CTRL_DYQ_PERF_THRESHOLD)
		rockchip_set_system_status(SYS_STATUS_PERFORMANCE);
	ret = spi_sync(spi, &m);
	if (spidev->dyq_perf && len > SPI_OBJ_CTRL_DYQ_PERF_THRESHOLD)
		rockchip_clear_system_status(SYS_STATUS_PERFORMANCE);

	return ret;
}

static int spidev_slv_write_and_read(struct spidev_rkslv_data *spidev, const void *tx_buf,
				     void *rx_buf, size_t len)
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

	if (spidev->dyq_perf && len > SPI_OBJ_CTRL_DYQ_PERF_THRESHOLD)
		rockchip_set_system_status(SYS_STATUS_PERFORMANCE);
	ret = spi_sync(spi, &m);
	if (spidev->dyq_perf && len > SPI_OBJ_CTRL_DYQ_PERF_THRESHOLD)
		rockchip_clear_system_status(SYS_STATUS_PERFORMANCE);

	return ret;
}

static ssize_t spidev_rkslv_misc_write(struct file *filp, const char __user *buf,
				       size_t n, loff_t *offset)
{
	struct spidev_rkslv_data *spidev;
	struct spi_device *spi;
	int argc = 0;
	char tmp[64];
	char *argv[16];
	char *cmd, *data;

	if (!n || n >= 64)
		return -EINVAL;

	spidev = filp->private_data;

	if (!spidev)
		return -ESHUTDOWN;

	spi = spidev->spi;
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

	if (!strcmp(cmd, "verbose")) {
		int val;

		if (argc < 1)
			return -EINVAL;

		if (kstrtoint(argv[0], 0, &val))
			return -EINVAL;

		if (val == 1)
			spidev->verbose = true;
		else
			spidev->verbose = false;
	} else if (!strcmp(cmd, "appmem")) {
		int addr, len;

		if (argc < 2)
			return -EINVAL;

		if (kstrtoint(argv[0], 0, &addr))
			return -EINVAL;
		if (kstrtoint(argv[1], 0, &len))
			return -EINVAL;

		if (!spidev_obj_param_valid(addr, len, 0)) {
			dev_err(&spi->dev, "param invalid, addr=%d len=%d\n", addr, len);
			return -EINVAL;
		}

		print_hex_dump(KERN_ERR, "APPMEM: ",
			       DUMP_PREFIX_OFFSET,
			       16,
			       1,
			       spidev->appmem + addr,
			       len,
			       1);
	} else {
		dev_err(&spi->dev, "unknown command\n");
	}

	return n;
}

static int spidev_rkslv_misc_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct spidev_rkslv_data *spidev;

	spidev = container_of(miscdev, struct spidev_rkslv_data, misc_dev);
	filp->private_data = spidev;

	return 0;
}

static const struct file_operations spidev_rkslv_misc_fops = {
	.write = spidev_rkslv_misc_write,
	.open = spidev_rkslv_misc_open,
};

static int spidev_rkslv_xfer(struct spidev_rkslv_data *spidev)
{
	char *ctrlbuf = spidev->ctrlbuf, *appmem = spidev->appmem, *tempbuf = spidev->tempbuf;
	struct spi_obj_ctrl *ctrl;
	struct spi_device *spi = spidev->spi;
	u32 len, left;
	int ret;

	memset(spidev->ctrlbuf, 0, SPI_OBJ_CTRL_MSG_SIZE);
	ret = spidev_slv_read(spidev, spidev->ctrlbuf, SPI_OBJ_CTRL_MSG_SIZE);
	if (ret) {
		dev_err(&spi->dev, "%s ctrl\n", __func__);
		return -EIO;
	}

	ctrl = (struct spi_obj_ctrl *)ctrlbuf;
	if (spidev->verbose)
		dev_err(&spi->dev, "ctrl cmd=%x addr=0x%x data=0x%x\n",
			ctrl->cmd, ctrl->addr, ctrl->data);

	if (!spidev_rkslv_ctrl_valid(ctrl->cmd, ctrl->addr, ctrl->data)) {
		dev_err_ratelimited(&spi->dev, "invalid ctrl cmd=%x addr=0x%x data=0x%x\n",
				    ctrl->cmd, ctrl->addr, ctrl->data);
		/*
		 * Master clocks out the data phase right after the ctrl phase:
		 * drain it, otherwise the next ctrl read would consume the
		 * data bytes and desynchronize the protocol until the master
		 * timeout reset.  Only WRITE/DUPLEX carry a master data phase
		 * and the master always clocks out at most
		 * SPI_OBJ_MAX_XFER_SIZE bytes there (READ has no master data
		 * phase at all), so cap the drain at SPI_OBJ_MAX_XFER_SIZE.
		 * This is best-effort only: the length is the untrusted
		 * ctrl->data and can mismatch what the master actually clocks
		 * out (e.g. on protocol desync), so bail out on shutdown
		 * instead of blocking the kthread forever.
		 */
		left = ctrl->data;
		while (left && spidev->tsk_run) {
			u32 chunk = min_t(u32, left, SPI_OBJ_MAX_XFER_SIZE);

			if (spidev_slv_read(spidev, tempbuf, chunk))
				break;
			left -= chunk;
		}

		return -EINVAL;
	}

	switch (ctrl->cmd) {
	case SPI_OBJ_CTRL_CMD_INIT:
		return 0;
	case SPI_OBJ_CTRL_CMD_READ:
		len = ctrl->data;
		ret = spidev_slv_write(spidev, appmem + ctrl->addr, len);
		if (ret) {
			dev_err(&spi->dev, "%s cmd=%x addr=0x%x data=0x%x\n",
				__func__, ctrl->cmd, ctrl->addr, ctrl->data);
			return -EIO;
		}
		break;
	case SPI_OBJ_CTRL_CMD_WRITE:
		len = ctrl->data;
		ret = spidev_slv_read(spidev, appmem + ctrl->addr, len);
		if (ret) {
			dev_err(&spi->dev, "%s cmd=%x addr=0x%x data=0x%x\n",
				__func__, ctrl->cmd, ctrl->addr, ctrl->data);
			return -EIO;
		}
		if (spidev->verbose) {
			print_hex_dump(KERN_ERR, "s-r: ",
					DUMP_PREFIX_OFFSET,
					16,
					1,
					appmem + ctrl->addr,
					len,
					1);
		}
		break;
	case SPI_OBJ_CTRL_CMD_DUPLEX:
		len = ctrl->data;
		ret = spidev_slv_write_and_read(spidev, appmem + ctrl->addr, tempbuf, len);
		if (ret) {
			dev_err(&spi->dev, "%s cmd=%x addr=0x%x data=0x%x\n",
				__func__, ctrl->cmd, ctrl->addr, ctrl->data);
			return -EIO;
		}
		if (spidev->verbose) {
			print_hex_dump(KERN_ERR, "s-d-t: ",
				DUMP_PREFIX_OFFSET,
				16,
				1,
				appmem + ctrl->addr,
				len,
				1);
			print_hex_dump(KERN_ERR, "s-d-r: ",
					DUMP_PREFIX_OFFSET,
					16,
					1,
					tempbuf,
					len,
					1);
		}
		memcpy(appmem + ctrl->addr, tempbuf, len);
		break;
	default:
		if (spidev->verbose)
			dev_err(&spi->dev, "%s unknown\n", __func__);
		return 0;
	}

	if (spidev->verbose)
		dev_err(&spi->dev, "xfer len=0x%x\n", ctrl->data);

	return 0;
}

static int spidev_rkslv_ctrl_receiver_thread(void *p)
{
	struct spidev_rkslv_data *spidev = (struct spidev_rkslv_data *)p;

	while (spidev->tsk_run)
		spidev_rkslv_xfer(spidev);

	return 0;
}

static int spidev_rkslv_probe(struct spi_device *spi)
{
	struct spidev_rkslv_data *spidev = NULL;
	int ret;

	if (!spi)
		return -ENOMEM;

	spidev = devm_kzalloc(&spi->dev, sizeof(struct spidev_rkslv_data), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;

	spidev->ctrlbuf = devm_kzalloc(&spi->dev, SPI_OBJ_MAX_XFER_SIZE, GFP_KERNEL);
	if (!spidev->ctrlbuf)
		return -ENOMEM;

	spidev->appmem = devm_kzalloc(&spi->dev, SPI_OBJ_APP_RAM_SIZE, GFP_KERNEL | GFP_DMA);
	if (!spidev->appmem)
		return -ENOMEM;

	spidev->tempbuf = devm_kzalloc(&spi->dev, SPI_OBJ_MAX_XFER_SIZE, GFP_KERNEL);
	if (!spidev->tempbuf)
		return -ENOMEM;

	spidev->dyq_perf = true;
	spidev->spi = spi;
	spidev->dev = &spi->dev;
	dev_set_drvdata(&spi->dev, spidev);

	dev_err(&spi->dev, "mode=%d, max_speed_hz=%d\n", spi->mode, spi->max_speed_hz);

	spidev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	spidev->misc_dev.name = "spidev_rkslv_misc";
	spidev->misc_dev.fops = &spidev_rkslv_misc_fops;
	spidev->misc_dev.parent = &spi->dev;
	ret = misc_register(&spidev->misc_dev);
	if (ret) {
		dev_err(&spi->dev, "fail to register misc device\n");
		return ret;
	}

	spidev->tsk_run = true;
	spidev->tsk = kthread_run(spidev_rkslv_ctrl_receiver_thread, spidev, "spidev-rkslv");
	if (IS_ERR(spidev->tsk)) {
		dev_err(&spi->dev, "start spidev-rkslv thread failed\n");
		return PTR_ERR(spidev->tsk);
	}

	return 0;
}

static void spidev_rkslv_remove(struct spi_device *spi)
{
	struct spidev_rkslv_data *spidev = dev_get_drvdata(&spi->dev);

	spidev->tsk_run = false;
	spi_target_abort(spi);
	kthread_stop(spidev->tsk);
	misc_deregister(&spidev->misc_dev);
}

#ifdef CONFIG_OF
static const struct of_device_id spidev_rkslv_dt_match[] = {
	{ .compatible = "rockchip,spi-obj-slave", },
	{},
};
MODULE_DEVICE_TABLE(of, spidev_rkslv_dt_match);

#endif /* CONFIG_OF */

static const struct spi_device_id spidev_rkslv_spi_ids[] = {
	{ .name = "spi-obj-slave" },
	{},
};
MODULE_DEVICE_TABLE(spi, spidev_rkslv_spi_ids);

static struct spi_driver spidev_rkslv_driver = {
	.driver = {
		.name	= "spidev_rkslv",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(spidev_rkslv_dt_match),
	},
	.probe		= spidev_rkslv_probe,
	.remove		= spidev_rkslv_remove,
	.id_table	= spidev_rkslv_spi_ids,
};
module_spi_driver(spidev_rkslv_driver);

MODULE_AUTHOR("Jon Lin <jon.lin@rock-chips.com>");
MODULE_DESCRIPTION("ROCKCHIP SPI Object Slave Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spidev_rkslv");
