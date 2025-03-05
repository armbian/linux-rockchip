// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include "inv_icm42670.h"
#include <linux/math64.h>

/**
 *  icm42670_set_enable() - enable chip functions.
 *  @indio_dev:	Device driver instance.
 *  @enable: enable/disable
 */
int icm42670_set_enable(struct iio_dev *indio_dev, bool enable)
{
	int ret;
	struct icm42670_data *data = iio_priv(indio_dev);
	const struct device *dev = regmap_get_device(data->regmap);

	if (enable) {
		ret = icm42670_set_mode(data, ICM42670_ACCEL, true);
		if (ret)
			goto error_off;

		ret = icm42670_set_mode(data, ICM42670_GYRO, true);
		if (ret)
			goto error_accl_off;

		if (true == data->enable_fifo) {
			ret = icm42670_reset_fifo(indio_dev);
			if (ret)
				goto error_gyro_off;
		} else {
			ret = icm42670_reset_data(indio_dev);
			if (ret)
				goto error_gyro_off;
		}

		/* Theoretical interrupt period */
		data->standard_period = data->period_divider > 0 ?
			div_s64(NSEC_PER_SEC, data->period_divider) :
			NSEC_PER_USEC * data->period_divider * (-1);

		data->interrupt_period = data->standard_period;

		/* Set a floating range of 4% */
		data->period_min = div_s64((data->standard_period * (100 -
					ICM42670_TS_PERIOD_JITTER)), 100);
		data->period_max = div_s64((data->standard_period * (100 +
					ICM42670_TS_PERIOD_JITTER)), 100);
	} else {
		ret = regmap_write(data->regmap, REG_INT_SOURCE0, BIT_INT_MODE_OFF);
		if (ret) {
			dev_err(dev, "int_enable failed %d\n", ret);
			goto error_gyro_off;
		}
		ret = regmap_write(data->regmap, REG_FIFO_CONFIG1, BIT_FIFO_MODE_BYPASS);
		if (ret) {
			dev_err(dev, "set REG_FIFO_CONFIG1 fail: %d\n", ret);
			goto error_gyro_off;
		}

		ret = icm42670_set_mode(data, ICM42670_ACCEL, false);
		if (ret)
			goto error_accl_off;

		ret = icm42670_set_mode(data, ICM42670_GYRO, false);
		if (ret)
			goto error_gyro_off;
	}
	return ret;

error_gyro_off:
	icm42670_set_mode(data, ICM42670_GYRO, false);
error_accl_off:
	icm42670_set_mode(data, ICM42670_ACCEL, false);
error_off:
	return ret;
}

/**
 * inv_mpu_data_rdy_trigger_set_state() - set data ready interrupt state
 * @trig: Trigger instance
 * @state: Desired trigger state
 */
static int inv_mpu_data_rdy_trigger_set_state(struct iio_trigger *trig,
						  bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct icm42670_data *data = iio_priv(indio_dev);
	int result;

	mutex_lock(&data->lock);
	dev_info(regmap_get_device(data->regmap), "in data_rdy_trigger_set_state, %d\n", state);
	result = icm42670_set_enable(indio_dev, state);
	mutex_unlock(&data->lock);

	return result;
}

static const struct iio_trigger_ops inv_mpu_trigger_ops = {
	.set_trigger_state = &inv_mpu_data_rdy_trigger_set_state,
};

static irqreturn_t iio_ext_irq(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct icm42670_data *data = iio_priv(indio_dev);
	ktime_t irq_pts = ktime_get();

	data->data_timestamp = ktime_to_ns(irq_pts);
	iio_trigger_generic_data_rdy_poll(irq, data->trig);

	return IRQ_WAKE_THREAD;
}

int icm42670_probe_trigger(struct iio_dev *indio_dev, int irq_type)
{
	int ret;
	struct icm42670_data *data = iio_priv(indio_dev);

	data->trig = devm_iio_trigger_alloc(&indio_dev->dev,
						"%s-dev%d",
						indio_dev->name,
						iio_device_id(indio_dev));
	if (!data->trig)
		return -ENOMEM;

	if (data->enable_fifo) {
		ret = devm_request_irq(&indio_dev->dev, data->irq,
			&iio_trigger_generic_data_rdy_poll,
			irq_type,
			"inv_mpu",
			data->trig);
	} else {
		ret = devm_request_threaded_irq(&indio_dev->dev, data->irq,
			&iio_ext_irq,
			&icm42670_read_data,
			IRQF_SHARED,
			"inv_mpu",
			indio_dev);
	}
	if (ret)
		return ret;

	data->trig->dev.parent = regmap_get_device(data->regmap);
	data->trig->ops = &inv_mpu_trigger_ops;
	iio_trigger_set_drvdata(data->trig, indio_dev);

	ret = devm_iio_trigger_register(&indio_dev->dev, data->trig);
	if (ret)
		return ret;

	indio_dev->trig = iio_trigger_get(data->trig);

	return 0;
}
