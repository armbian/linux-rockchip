// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * vehicle-spi.c  --  I2C register interface access for spi
 *
 * Copyright (c) 2025-2030 Rockchip Electronics Co., Ltd.
 *
 * Author: LuoWei <lw@rock-chips.com>
 */

#include "core.h"
#ifdef CONFIG_VEHICLE_SPI_PROTOCOL
#include "vehicle_spi_protocol.h"
#endif

static struct completion spi_complete;
#define SPI_TIMEOUT_MS 20

static void spi_complete_callback(void *arg)
{
	complete(&spi_complete);
}

int vehicle_spi_write_slt(struct vehicle *vehicle, const void *txbuf, size_t n)
{
	int ret = -1;
	struct spi_device *spi = NULL;
	struct spi_transfer     t = {
			.tx_buf         = txbuf,
			.len            = n,
			.bits_per_word = 8,
		};
	struct spi_message      m;

	mutex_lock(&vehicle->vehicle_spi->wq_lock);
	spi = vehicle->vehicle_spi->spi;
	reinit_completion(&spi_complete);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	m.complete = spi_complete_callback;
	ret = spi_async(spi, &m);

	if (ret) {
		dev_err(&spi->dev, "SPI write async error: %d\n", ret);
		goto unlock;
	}

	if (!wait_for_completion_timeout(&spi_complete, msecs_to_jiffies(SPI_TIMEOUT_MS))) {
		dev_err(&spi->dev, "SPI write operation timed out\n");
		goto unlock;
	}
unlock:
	mutex_unlock(&vehicle->vehicle_spi->wq_lock);
	return ret;
}

int vehicle_spi_read_slt(struct vehicle *vehicle, void *rxbuf, size_t n)
{
	int ret = -1;
	struct spi_device *spi = NULL;
	struct spi_transfer     t = {
			.rx_buf         = rxbuf,
			.len            = n,
			.bits_per_word = 8,
		};
	struct spi_message      m;

	mutex_lock(&vehicle->vehicle_spi->wq_lock);
	spi = vehicle->vehicle_spi->spi;
	reinit_completion(&spi_complete);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	m.complete = spi_complete_callback;
	ret = spi_async(spi, &m);
	if (ret) {
		dev_err(&spi->dev, "SPI read async error: %d\n", ret);
		goto unlock;
	}

	if (!wait_for_completion_timeout(&spi_complete, msecs_to_jiffies(SPI_TIMEOUT_MS))) {
		dev_err(&spi->dev, "SPI read operation timed out\n");
		goto unlock;
	}
unlock:
	mutex_unlock(&vehicle->vehicle_spi->wq_lock);
	return ret;
}

static int vehicle_spi_update_data(struct vehicle *vehicle)
{
	int i = 0;
	unsigned int times = 1, size = 12;
	unsigned long us = 0, bytes = 0;
	unsigned char *rxbuf = NULL;
	ktime_t start_time;
	ktime_t end_time;
	ktime_t cost_time;
	struct device *dev = vehicle->vehicle_spi->dev;

	rxbuf = kzalloc(size, GFP_KERNEL);

	if (!rxbuf)
		return -ENOMEM;

	start_time = ktime_get();
	for (i = 0; i < times; i++)
#ifndef CONFIG_VEHICLE_SPI_PROTOCOL
		vehicle_spi_read_slt(vehicle, rxbuf, size);
#else
		vehicle_analyze_read_data(vehicle, rxbuf, size);
#endif
	end_time = ktime_get();
	cost_time = ktime_sub(end_time, start_time);
	us = ktime_to_us(cost_time);

	bytes = size * times * 1;
	bytes = bytes * 1000 / us;
	pr_info("spi read %d*%d cost %ldus speed:%ldKB/S\n", size, times, us, bytes);
	print_hex_dump(KERN_ERR, "SPI RX: ",
		       DUMP_PREFIX_OFFSET,
		       16,
		       1,
		       rxbuf,
		       size,
		       1);

	kfree(rxbuf);
	vehicle_set_property(VEHICLE_GEAR, 0, vehicle->vehicle_data.gear, 0);
	vehicle_set_property(VEHICLE_TURN_SIGNAL, 0, vehicle->vehicle_data.turn, 0);

	dev_info(dev, "gear %u turn %u\n", vehicle->vehicle_data.gear, vehicle->vehicle_data.turn);

	/* to do others spi */
	return 0;
}


static void vehicle_spi_delay_work_func(struct work_struct *work)
{
	struct vehicle_spi *vehicle_spi = container_of(work, struct vehicle_spi,
						       vehicle_delay_work.work);
	struct device *dev = vehicle_spi->dev;

	vehicle_spi_update_data(g_vehicle_hw);

	if (vehicle_spi->use_delay_work)
		queue_delayed_work(vehicle_spi->vehicle_wq, &vehicle_spi->vehicle_delay_work,
				   msecs_to_jiffies(1000));

	dev_info(dev, "%s end\n", __func__);
}

static irqreturn_t vehicle_spi_irq_handle(int irq, void *_data)
{
	struct vehicle_spi *vehicle_spi = _data;

	queue_delayed_work(vehicle_spi->vehicle_wq, &vehicle_spi->vehicle_delay_work,
			   msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

#if defined(CONFIG_VEHICLE_GPIO_MCU_EXPANDER) && defined(CONFIG_VEHICLE_SPI_PROTOCOL)
static int vehicle_spi_write_data(void *context, unsigned int reg,
				   unsigned int val)
{
	unsigned char value = 0;

	value = val & 0x0f;
	return vehicle_analyze_write_data(g_vehicle_hw, (unsigned char)reg, &value, sizeof(value));

}

static int vehicle_spi_read_data(void *context, unsigned int reg,
				  unsigned int *val)
{
	return vehicle_analyze_read_reg(g_vehicle_hw, reg, val);
}

static int vehicle_analyze_update_bits(void *context, unsigned int reg,
					 unsigned int mask, unsigned int val)
{
	return 0;
}

const struct regmap_bus vehicle_regmap_spi = {
	.reg_write = vehicle_spi_write_data,
	.reg_read = vehicle_spi_read_data,
	.reg_update_bits = vehicle_analyze_update_bits,
};
#endif

static int vehicle_spi_irq_init(struct vehicle_spi *vehicle_spi)
{
	struct device *dev = vehicle_spi->dev;
	struct gpio_desc *irq_gpio_desc;
	int ret;

	irq_gpio_desc = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
	if (IS_ERR_OR_NULL(irq_gpio_desc))
		dev_warn(dev, "Failed to request irq-gpio\n");
	else {
		vehicle_spi->irq = gpiod_to_irq(irq_gpio_desc);
		ret = devm_request_threaded_irq(dev, vehicle_spi->irq,
				vehicle_spi_irq_handle, NULL,
				IRQF_TRIGGER_FALLING,
				dev_name(dev), vehicle_spi);

		if (ret < 0) {
			dev_err(dev, "error: irq %d\n", vehicle_spi->irq);
			gpio_free(vehicle_spi->irq);
			return ret;
		}
	}

	return 0;
}

static int spi_hw_init(struct vehicle *vehicle)
{
	vehicle->vehicle_spi->vehicle_wq = alloc_ordered_workqueue("%s",
							 WQ_MEM_RECLAIM | WQ_FREEZABLE,
							 "vehicle-spi-wq");
	mutex_init(&vehicle->vehicle_spi->wq_lock);
	INIT_DELAYED_WORK(&vehicle->vehicle_spi->vehicle_delay_work,
			  vehicle_spi_delay_work_func);

	vehicle->vehicle_spi->use_delay_work =
		of_property_read_bool(vehicle->vehicle_spi->dev->of_node, "use-delay-work");

	if (vehicle->vehicle_spi->use_delay_work) {
		queue_delayed_work(vehicle->vehicle_spi->vehicle_wq,
				   &vehicle->vehicle_spi->vehicle_delay_work,
				   msecs_to_jiffies(100));
		VEHICLE_DBG("%s: vehicle_spi->use_delay_work=%d\n", __func__,
			    vehicle->vehicle_spi->use_delay_work);
	} else {
		vehicle_spi_irq_init(vehicle->vehicle_spi);
		VEHICLE_DBG("%s: vehicle_spi->use_delay_work=%d\n", __func__,
			    vehicle->vehicle_spi->use_delay_work);
	}

	return 0;
}

static int spi_pm_suspend(struct vehicle *vehicle)
{
	return 0;
}

static int spi_pm_resume(struct vehicle *vehicle)
{
	return 0;
}

struct vehicle_hw_data vehicle_spi_data = {
	.name	= "vehicle-spi",
	.vehicle_hw_type	= VEHICLE_HW_TYPE_SPI,
	.data_update	= vehicle_spi_update_data,
	.hw_init	= spi_hw_init,
	.suspend	= spi_pm_suspend,
	.resume		= spi_pm_resume,
};
EXPORT_SYMBOL_GPL(vehicle_spi_data);

static ssize_t spi_test_write(struct file *file,
			const char __user *buf, size_t n, loff_t *offset)
{
	int argc = 0, i;
	char tmp[64];
	char *argv[16];
	char *cmd, *data;
	unsigned int id = 0, times = 0, size = 0, cmd_spi = 0;
	unsigned long us = 0, bytes = 0;
	char *txbuf = NULL;
	ktime_t start_time;
	ktime_t end_time;
	ktime_t cost_time;

	if (n >= sizeof(tmp)) {
		pr_info("%s error size > 64\n", __func__);
		return -EINVAL;
	}

	memset(tmp, 0, sizeof(tmp));
	if (copy_from_user(tmp, buf, n))
		return -EFAULT;
	cmd = tmp;
	data = tmp;

	memset(argv, 0, sizeof(argv));

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

	if (!strcmp(cmd, "write")) {
		if (kstrtoint(argv[0], 10, &id) < 0)
			return -EFAULT;

		if (kstrtoint(argv[1], 10, &times) < 0)
			return -EFAULT;

		if (kstrtoint(argv[2], 10, &size) < 0)
			return -EFAULT;

		if (kstrtoint(argv[3], 16, &cmd_spi) < 0)
			return -EFAULT;

		txbuf = kzalloc(size, GFP_KERNEL);
		if (!txbuf)
			return n;

		for (i = 0; i < size; i++) {
			if (kstrtoint(argv[4+i], 16, (int *)(txbuf+i)) < 0)
				return -EFAULT;
		}

		start_time = ktime_get();
		for (i = 0; i < times; i++)
#ifndef CONFIG_VEHICLE_SPI_PROTOCOL
			vehicle_spi_write_slt(g_vehicle_hw, txbuf, size);
#else
			vehicle_analyze_write_data(g_vehicle_hw, (unsigned char)cmd_spi,
							 txbuf, size);
#endif
		end_time = ktime_get();
		cost_time = ktime_sub(end_time, start_time);
		us = ktime_to_us(cost_time);

		bytes = size * times * 1;
		bytes = bytes * 1000 / us;
		pr_info("spi write %d*%d cost %ldus speed:%ldKB/S\n", size, times, us, bytes);
		kfree(txbuf);
	}
	return n;
}
static const struct file_operations spi_test_fops = {
	.write = spi_test_write,
};

static struct miscdevice spi_test_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "spi_misc_test",
	.fops = &spi_test_fops,
};

static int vehicle_spi_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct vehicle_spi *vehicle_spi;
	int ret = 0;
	int id = 0;

	if (!spi)
		return -ENOMEM;

	if (g_vehicle_hw == NULL)
		return -ENOMEM;

	vehicle_spi = devm_kzalloc(dev, sizeof(*vehicle_spi), GFP_KERNEL);
	if (vehicle_spi == NULL)
		return -ENOMEM;

	spi_set_drvdata(spi, vehicle_spi);

	vehicle_spi->hw_data = &vehicle_spi_data;
	spi->bits_per_word = 8;
	vehicle_spi->dev = dev;
	vehicle_spi->spi = spi;
	g_vehicle_hw->vehicle_spi = vehicle_spi;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(dev, "ERR: fail to setup spi\n");
		return -1;
	}

	if (device_property_read_u32(&spi->dev, "id", &id)) {
		dev_warn(&spi->dev, "fail to get id, default set 0\n");
		id = 0;
	}

	init_completion(&spi_complete);
	spi_hw_init(g_vehicle_hw);

#if defined(CONFIG_VEHICLE_GPIO_MCU_EXPANDER) && defined(CONFIG_VEHICLE_SPI_PROTOCOL)
	ret = vehicle_analyze_read_reg(g_vehicle_hw, VERSION, &id);
	if (ret == 0 && id == VERSION_ID)
		gpio_mcu_register(spi);
#endif

	return 0;
}

static void vehicle_spi_remove(struct spi_device *spi)
{
	struct vehicle_spi *vehicle_spi = spi_get_drvdata(spi);

	destroy_workqueue(vehicle_spi->vehicle_wq);
}

#ifdef CONFIG_OF
static const struct of_device_id vehicle_spi_id[] = {
	{ .compatible = "rockchip,vehicle-spi", },
	{},
};
#endif

static struct spi_driver vehicle_spi_device_driver = {
	.probe          = vehicle_spi_probe,
	.remove         = vehicle_spi_remove,
	.driver         =  {
		.name   = "vehicle-spi",
		.of_match_table = vehicle_spi_id,
	}
};

static int vehicle_spi_init(void)
{
	int err;

	misc_register(&spi_test_misc);
	err = spi_register_driver(&vehicle_spi_device_driver);
	if (err) {
		pr_err("Failed to register vehicle spi driver\n");
		misc_deregister(&spi_test_misc);
	}

	return err;
}
module_init(vehicle_spi_init);

static void __exit vehicle_spi_exit(void)
{
	misc_deregister(&spi_test_misc);
	spi_unregister_driver(&vehicle_spi_device_driver);
}

module_exit(vehicle_spi_exit);

MODULE_LICENSE("GPL");
