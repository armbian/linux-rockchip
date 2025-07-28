// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * vehicle-gpio.c  --  I2C register interface access for gpio
 *
 * Copyright (c) 2025-2030 Rockchip Electronics Co., Ltd.
 *
 * Author: LuoWei <lw@rock-chips.com>
 */

#include "core.h"

static int vehicle_gpio_init_io(struct vehicle_gpio *vehicle_gpio)
{
	int err = 0;
	struct device *dev = vehicle_gpio->dev;

	vehicle_gpio->gear_gpio_reverse = devm_gpiod_get_optional(dev, "reverse", GPIOD_IN);
	if (IS_ERR(vehicle_gpio->gear_gpio_reverse)) {
		dev_err(dev, "failed to get gpio reverse\n");
		err = PTR_ERR(vehicle_gpio->gear_gpio_reverse);
	}

	vehicle_gpio->gear_gpio_park = devm_gpiod_get_optional(dev, "park", GPIOD_IN);
	if (IS_ERR(vehicle_gpio->gear_gpio_park)) {
		dev_err(dev, "failed to get gpio park\n");
		err = PTR_ERR(vehicle_gpio->gear_gpio_park);
	}

	return err;
}

static int vehicle_gpio_update_data(struct vehicle *vehicle)
{
	int gear = 0;
	int park_value = 0;
	int reverse_value = 0;
	struct device *dev = vehicle->vehicle_gpio->dev;

	if (vehicle->vehicle_gpio->gear_gpio_park)
		park_value = !!gpiod_get_value(vehicle->vehicle_gpio->gear_gpio_park);

	if (vehicle->vehicle_gpio->gear_gpio_reverse)
		reverse_value = !!gpiod_get_value(vehicle->vehicle_gpio->gear_gpio_reverse);

	dev_info(dev, "vehicle gpio %d %d\n", park_value, reverse_value);

	if (park_value && reverse_value)
		vehicle->vehicle_data.gear = GEAR_2;
	else if (!park_value && reverse_value)
		vehicle->vehicle_data.gear = GEAR_1;
	else if (park_value && !reverse_value)
		vehicle->vehicle_data.gear = GEAR_0;
	else
		vehicle->vehicle_data.gear = GEAR_3;

	vehicle_set_property(VEHICLE_GEAR, 0, vehicle->vehicle_data.gear, 0);
	vehicle_set_property(VEHICLE_TURN_SIGNAL, 0, vehicle->vehicle_data.turn, 0);

	dev_info(dev, "gear %u turn %u\n", vehicle->vehicle_data.gear,
		 vehicle->vehicle_data.turn);

	/* to do others gpio*/

	return gear;
}

static void vehicle_gpio_delay_work_func(struct work_struct *work)
{
	struct vehicle_gpio *vehicle_gpio = container_of(work, struct vehicle_gpio,
							 vehicle_delay_work.work);
	struct device *dev = vehicle_gpio->dev;

	vehicle_gpio_update_data(g_vehicle_hw);

	if (vehicle_gpio->use_delay_work)
		queue_delayed_work(vehicle_gpio->vehicle_wq, &vehicle_gpio->vehicle_delay_work,
				   msecs_to_jiffies(1000));

	dev_info(dev, "%s\n", __func__);
}

static irqreturn_t vehicle_gpio_irq_handle(int irq, void *_data)
{
	struct vehicle_gpio *vehicle_gpio = _data;

	queue_delayed_work(vehicle_gpio->vehicle_wq, &vehicle_gpio->vehicle_delay_work,
			   msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static int vehicle_gpio_irq_init(struct vehicle_gpio *vehicle_gpio)
{
	struct device *dev = vehicle_gpio->dev;
	int ret;

	vehicle_gpio->irq = platform_get_irq(vehicle_gpio->pdev, 0);
	if (vehicle_gpio->irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_threaded_irq(dev, vehicle_gpio->irq,
					vehicle_gpio_irq_handle, NULL,
					IRQF_TRIGGER_HIGH,
					dev_name(dev), vehicle_gpio);

	if (ret < 0) {
		dev_err(dev, "error: irq %d\n", vehicle_gpio->irq);
		return ret;
	}

	return 0;
}

static int gpio_hw_init(struct vehicle *vehicle)
{
	vehicle_gpio_init_io(vehicle->vehicle_gpio);
	vehicle->vehicle_gpio->vehicle_wq =
		alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_FREEZABLE,
					"vehicle-gpio-wq");
	mutex_init(&vehicle->vehicle_gpio->wq_lock);
	INIT_DELAYED_WORK(&vehicle->vehicle_gpio->vehicle_delay_work, vehicle_gpio_delay_work_func);

	vehicle->vehicle_gpio->use_delay_work =
		of_property_read_bool(vehicle->vehicle_gpio->dev->of_node, "use-delay-work");
	if (vehicle->vehicle_gpio->use_delay_work) {
		queue_delayed_work(vehicle->vehicle_gpio->vehicle_wq,
				   &vehicle->vehicle_gpio->vehicle_delay_work,
				   msecs_to_jiffies(100));
		VEHICLE_DBG("%s: vehicle_gpio->use_delay_work=%d\n", __func__,
			    vehicle->vehicle_gpio->use_delay_work);
	} else {
		vehicle_gpio_irq_init(vehicle->vehicle_gpio);
		VEHICLE_DBG("%s: vehicle_gpio->use_delay_work=%d\n", __func__,
			    vehicle->vehicle_gpio->use_delay_work);
	}

	return 0;
}

static int gpio_pm_suspend(struct vehicle *vehicle)
{
	return 0;
}

static int gpio_pm_resume(struct vehicle *vehicle)
{
	return 0;
}

struct vehicle_hw_data vehicle_gpio_data = {
	.name	= "vehicle-gpio",
	.vehicle_hw_type	= VEHICLE_HW_TYPE_GPIO,
	.data_update	= vehicle_gpio_update_data,
	.hw_init	= gpio_hw_init,
	.suspend	= gpio_pm_suspend,
	.resume		= gpio_pm_resume,
};
EXPORT_SYMBOL_GPL(vehicle_gpio_data);

static int vehicle_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vehicle_gpio *vehicle_gpio;

	if (!pdev)
		return -ENOMEM;

	if (g_vehicle_hw == NULL)
		return -ENOMEM;

	vehicle_gpio = devm_kzalloc(dev, sizeof(*vehicle_gpio), GFP_KERNEL);
	if (vehicle_gpio == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, vehicle_gpio);
	vehicle_gpio->hw_data = &vehicle_gpio_data;
	vehicle_gpio->dev = dev;
	vehicle_gpio->pdev = pdev;

	g_vehicle_hw->vehicle_gpio = vehicle_gpio;

	gpio_hw_init(g_vehicle_hw);

	return 0;
}

static int vehicle_gpio_remove(struct platform_device *pdev)
{
	struct vehicle_gpio *vehicle_gpio = platform_get_drvdata(pdev);

	destroy_workqueue(vehicle_gpio->vehicle_wq);
	return 0;
}

static const struct of_device_id vehicle_gpio_id[] = {
	{ .compatible = "rockchip,vehicle-gpio", },
	{ .compatible = "rockchip,vehicle-dummy-gpio", },
	{},
};

static struct platform_driver vehicle_gpio_device_driver = {
	.probe          = vehicle_gpio_probe,
	.remove         = vehicle_gpio_remove,
	.driver         =  {
		.name   = "vehicle-gpio",
		.of_match_table = vehicle_gpio_id,
	}
};

static int vehicle_gpio_init(void)
{
	int err;

	err = platform_driver_register(&vehicle_gpio_device_driver);
	if (err)
		pr_err("Failed to register vehicle driver\n");

	return err;
}

static void __exit vehicle_gpio_exit(void)
{
	platform_driver_unregister(&vehicle_gpio_device_driver);
}

postcore_initcall(vehicle_gpio_init);
module_exit(vehicle_gpio_exit);

MODULE_LICENSE("GPL");
