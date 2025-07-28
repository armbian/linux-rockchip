// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * vehicle-adc.c  --  I2C register interface access for adc
 *
 * Copyright (c) 2025-2030 Rockchip Electronics Co., Ltd.
 *
 * Author: LuoWei <lw@rock-chips.com>
 */

#include "core.h"

static int vehicle_adc_init_channel(struct vehicle *vehicle, struct iio_channel **channel,
				    const char *name)
{
	int err = 0;
	enum iio_chan_type type;
	struct device *dev = vehicle->vehicle_adc->dev;

	struct iio_channel *tmp_channel = devm_iio_channel_get(dev, name);

	if (IS_ERR(tmp_channel))
		return PTR_ERR(tmp_channel);

	if (!tmp_channel->indio_dev)
		return -ENXIO;

	err = iio_get_channel_type(tmp_channel, &type);
	if (err < 0)
		return err;

	if (type != IIO_VOLTAGE) {
		dev_err(dev, "Incompatible channel type %d\n", type);
		return -EINVAL;
	}

	*channel = tmp_channel;

	return err;
}

static int vehicle_adc_update_data(struct vehicle *vehicle)
{
	int val;
	struct device *dev = vehicle->vehicle_adc->dev;

	vehicle->vehicle_data.turn = TURN_0;
	if (vehicle->vehicle_adc->gear_adc_chn) {
		if (iio_read_channel_raw(vehicle->vehicle_adc->gear_adc_chn, &val) < 0)
			dev_err(dev, "Failed to read gear adc value\n");

		if (val < 200)
			vehicle->vehicle_data.gear = GEAR_0;
		else if (val > 200 && val < 1200)
			vehicle->vehicle_data.gear = GEAR_1;
		else if (val > 1200 && val < 2200)
			vehicle->vehicle_data.gear = GEAR_3;
		else if (val > 2200 && val < 3200)
			vehicle->vehicle_data.gear = GEAR_2;
	}

	if (vehicle->vehicle_adc->turn_left_adc_chn) {
		if (iio_read_channel_raw(vehicle->vehicle_adc->turn_left_adc_chn, &val) < 0)
			dev_err(dev, "Failed to read turn adc value\n");

		if (val < 100)
			vehicle->vehicle_data.turn = TURN_1;
	}

	if (vehicle->vehicle_adc->turn_right_adc_chn) {
		if (iio_read_channel_raw(vehicle->vehicle_adc->turn_right_adc_chn, &val) < 0)
			dev_err(dev, "Failed to read turn adc value\n");

		if (val < 100)
			vehicle->vehicle_data.turn = TURN_2;
	}

	vehicle_set_property(VEHICLE_GEAR, 0, vehicle->vehicle_data.gear, 0);
	vehicle_set_property(VEHICLE_TURN_SIGNAL, 0, vehicle->vehicle_data.turn, 0);

	dev_info(dev, "gear %u turn %u\n", vehicle->vehicle_data.gear,
		 vehicle->vehicle_data.turn);

	/* to do others adc */
	return 0;
}

static void vehicle_adc_delay_work_func(struct work_struct *work)
{
	struct vehicle_adc *vehicle_adc = container_of(work, struct vehicle_adc,
						       vehicle_delay_work.work);
	struct device *dev = vehicle_adc->dev;

	vehicle_adc_update_data(g_vehicle_hw);

	if (vehicle_adc->use_delay_work)
		queue_delayed_work(vehicle_adc->vehicle_wq, &vehicle_adc->vehicle_delay_work,
				   msecs_to_jiffies(1000));

	dev_info(dev, "%s\n", __func__);
}

static irqreturn_t vehicle_adc_irq_handle(int irq, void *_data)
{
	struct vehicle_adc *vehicle_adc = _data;

	queue_delayed_work(vehicle_adc->vehicle_wq, &vehicle_adc->vehicle_delay_work,
			   msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static int vehicle_adc_irq_init(struct vehicle_adc *vehicle_adc)
{
	struct device *dev = vehicle_adc->dev;
	int ret;

	vehicle_adc->irq = platform_get_irq(vehicle_adc->pdev, 0);
	if (vehicle_adc->irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_threaded_irq(dev, vehicle_adc->irq,
					vehicle_adc_irq_handle, NULL,
					IRQF_TRIGGER_HIGH,
					dev_name(dev), vehicle_adc);

	if (ret < 0) {
		dev_err(dev, "error: irq %d\n", vehicle_adc->irq);
		return ret;
	}

	return 0;
}

static int adc_hw_init(struct vehicle *vehicle)
{
	int err = 0;
	struct device *dev = vehicle->dev;

	err = vehicle_adc_init_channel(vehicle,
				       &vehicle->vehicle_adc->gear_adc_chn,
				       "gear");
	if (err)
		dev_err(dev, "failed to get turn adc channel.\n");

	err = vehicle_adc_init_channel(vehicle,
				       &vehicle->vehicle_adc->turn_left_adc_chn,
				       "turn_left");
	if (err)
		dev_err(dev, "failed to get turn_left adc channel.\n");

	err = vehicle_adc_init_channel(vehicle,
				       &vehicle->vehicle_adc->turn_right_adc_chn,
				       "turn_right");
	if (err)
		dev_err(dev, "failed to get turn_right adc channel.\n");

	vehicle->vehicle_adc->vehicle_wq = alloc_ordered_workqueue("%s",
								   WQ_MEM_RECLAIM | WQ_FREEZABLE,
								   "vehicle-adc-wq");
	mutex_init(&vehicle->vehicle_adc->wq_lock);
	INIT_DELAYED_WORK(&vehicle->vehicle_adc->vehicle_delay_work, vehicle_adc_delay_work_func);

	//vehicle->vehicle_adc->use_delay_work =
	//	of_property_read_bool(vehicle->dev->of_node, "use-delay-work");
	vehicle->vehicle_adc->use_delay_work = 1;
	if (vehicle->vehicle_adc->use_delay_work) {
		queue_delayed_work(vehicle->vehicle_adc->vehicle_wq,
				   &vehicle->vehicle_adc->vehicle_delay_work,
				   msecs_to_jiffies(100));
		VEHICLE_DBG("%s: vehicle_adc->use_delay_work=%d\n", __func__,
			    vehicle->vehicle_adc->use_delay_work);
	} else {
		vehicle_adc_irq_init(vehicle->vehicle_adc);
		VEHICLE_DBG("%s: vehicle_adc->use_delay_work=%d\n", __func__,
			    vehicle->vehicle_adc->use_delay_work);
	}

	return 0;
}

static int adc_pm_suspend(struct vehicle *vehicle)
{
	return 0;
}

static int adc_pm_resume(struct vehicle *vehicle)
{
	return 0;
}

struct vehicle_hw_data vehicle_adc_data = {
	.name	= "vehicle-adc",
	.vehicle_hw_type	= VEHICLE_HW_TYPE_ADC,
	.data_update	= vehicle_adc_update_data,
	.hw_init	= adc_hw_init,
	.suspend	= adc_pm_suspend,
	.resume		= adc_pm_resume,
};
EXPORT_SYMBOL_GPL(vehicle_adc_data);

static int vehicle_adc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vehicle_adc *vehicle_adc;

	if (!pdev)
		return -ENOMEM;

	if (g_vehicle_hw == NULL)
		return -ENOMEM;

	vehicle_adc = devm_kzalloc(dev, sizeof(*vehicle_adc), GFP_KERNEL);
	if (vehicle_adc == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, vehicle_adc);
	vehicle_adc->hw_data = &vehicle_adc_data;
	vehicle_adc->dev = dev;
	vehicle_adc->pdev = pdev;

	g_vehicle_hw->vehicle_adc = vehicle_adc;

	adc_hw_init(g_vehicle_hw);

	pr_info("%s successfully\n", __func__);

	return 0;
}

static int vehicle_adc_remove(struct platform_device *pdev)
{
	struct vehicle_adc *vehicle_adc = platform_get_drvdata(pdev);

	destroy_workqueue(vehicle_adc->vehicle_wq);
	return 0;
}

static const struct of_device_id vehicle_adc_id[] = {
	{ .compatible = "rockchip,vehicle-adc", },
	{ .compatible = "rockchip,vehicle-dummy-adc", },
	{},
};

static struct platform_driver vehicle_adc_device_driver = {
	.probe          = vehicle_adc_probe,
	.remove         = vehicle_adc_remove,
	.driver         =  {
		.name   = "vehicle-adc",
		.of_match_table = vehicle_adc_id,
	}
};

static int vehicle_adc_init(void)
{
	int err;

	err = platform_driver_register(&vehicle_adc_device_driver);
	if (err)
		pr_err("Failed to register vehicle driver\n");

	pr_info("%s successfully\n", __func__);
	return err;
}

static void __exit vehicle_adc_exit(void)
{
	platform_driver_unregister(&vehicle_adc_device_driver);
}

postcore_initcall(vehicle_adc_init);
module_exit(vehicle_adc_exit);

MODULE_LICENSE("GPL");
