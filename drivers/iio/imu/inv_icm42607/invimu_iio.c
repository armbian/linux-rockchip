// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/pm_runtime.h>

#include "icm42607.h"
#include "imu.h"
#include "invimu_iio.h"

#define INVIMU_CHANNEL(_type, _address, _channel2, _scan_index) \
{ \
	.type = _type, \
	.address = _address, \
	.modified = 1, \
	.channel2 = _channel2, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | \
		BIT(IIO_CHAN_INFO_OFFSET) | BIT(IIO_CHAN_INFO_SCALE), \
	.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ), \
	.info_mask_shared_by_all_available = BIT(IIO_CHAN_INFO_SAMP_FREQ), \
	.scan_index = _scan_index, \
	.scan_type = { \
		.sign = 's', \
		.realbits = 16, \
		.storagebits = 16, \
		.endianness = IIO_LE, \
	}, \
}

static const int icm42607_avail_acc_sample_freqs[] = {100};
static const int icm42607_avail_gyro_sample_freqs[] = {100};

int invimu_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct imu_sensor *sensor = iio_priv(indio_dev);
	struct imu_ctrb *ctrb = sensor->ctrb;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ctrb->chipinfo->read_asix_one(ctrb, chan->address, val);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		if (sensor->id == IMU_SENSOR_ID_ACCE) {
			*val = 980665ULL;
			*val2 = 100000ULL * 2048;/* scale = 9.8 / 2048 */
		} else if (sensor->id == IMU_SENSOR_ID_GYRO) {
			*val = 314159ULL;
			*val2 = 1800000ULL * 143;/* scale = pi / (180 * 14.3) */
		} else {
			return -EINVAL;
		}
		return IIO_VAL_FRACTIONAL;
	case IIO_CHAN_INFO_OFFSET:
		if (chan->channel2 == IIO_MOD_X)
			*val = sensor->offset_x;
		else if (chan->channel2 == IIO_MOD_Y)
			*val = sensor->offset_y;
		else if (chan->channel2 == IIO_MOD_Z)
			*val = sensor->offset_z;
		else
			return -EINVAL;

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = sensor->odr;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
	return IIO_VAL_INT;
}
EXPORT_SYMBOL_GPL(invimu_read_raw);

int invimu_read_avail(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, const int **vals, int *type, int *length, long mask)
{
	struct imu_sensor *sensor = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		*type = IIO_VAL_INT;
		switch (sensor->id) {
		case IMU_SENSOR_ID_ACCE:
			*vals = icm42607_avail_acc_sample_freqs;
			*length = ARRAY_SIZE(icm42607_avail_acc_sample_freqs);
			return IIO_AVAIL_LIST;
		case IMU_SENSOR_ID_GYRO:
			*vals = icm42607_avail_gyro_sample_freqs;
			*length = ARRAY_SIZE(icm42607_avail_gyro_sample_freqs);
			return IIO_AVAIL_LIST;
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(invimu_read_avail);

int invimu_write_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int val, int val2, long mask)
{
	struct imu_sensor *sensor = iio_priv(indio_dev);
	struct imu_ctrb *ctrb = sensor->ctrb;

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		sensor->odr = val;
		break;
	case IIO_CHAN_INFO_OFFSET:
		switch (chan->channel2) {
		case IIO_MOD_X:
			sensor->offset_x = val;
			break;
		case IIO_MOD_Y:
			sensor->offset_y = val;
			break;
		case IIO_MOD_Z:
			sensor->offset_z = val;
			break;
		default:
			return -EINVAL;
		}

		if (sensor->id == IMU_SENSOR_ID_ACCE)
			return ctrb->chipinfo->set_accel_offset(ctrb, val, chan->channel2);
		else if (sensor->id == IMU_SENSOR_ID_GYRO)
			return ctrb->chipinfo->set_gyro_offset(ctrb, val, chan->channel2);
		else
			return -EINVAL;
	default:
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(invimu_write_raw);

int invimu_write_raw_get_fmt(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SAMP_FREQ:
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(invimu_write_raw_get_fmt);

static const struct iio_chan_spec invimu_acc_channels[] = {
	INVIMU_CHANNEL(IIO_ACCEL, 0, IIO_MOD_X, 0),
	INVIMU_CHANNEL(IIO_ACCEL, 1, IIO_MOD_Y, 1),
	INVIMU_CHANNEL(IIO_ACCEL, 2, IIO_MOD_Z, 2),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec invimu_gyro_channels[] = {
	INVIMU_CHANNEL(IIO_ANGL_VEL, 3, IIO_MOD_X, 0),
	INVIMU_CHANNEL(IIO_ANGL_VEL, 4, IIO_MOD_Y, 1),
	INVIMU_CHANNEL(IIO_ANGL_VEL, 5, IIO_MOD_Z, 2),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static int invimu_buffer_preenable(struct iio_dev *indio_dev)
{
	int mode, ret = 0;
	struct imu_sensor *sensor = iio_priv(indio_dev);
	struct imu_ctrb *ctrb = sensor->ctrb;

	mutex_lock(&ctrb->power_lock);

	switch (sensor->id) {
	case IMU_SENSOR_ID_ACCE:
		mode = iio_buffer_enabled(ctrb->iio_devs[IMU_SENSOR_ID_GYRO]) ?
			IMU_POWER_ACCE_GYRO : IMU_POWER_ACCE_ONLY;
		break;
	case IMU_SENSOR_ID_GYRO:
		mode = IMU_POWER_ACCE_GYRO;
		break;
	default:
		mode = IMU_POWER_MODE_DOWN;
		break;
	}
	ret = ctrb->chipinfo->mode_set(ctrb, mode);

	if (ret == 0 && (mode == IMU_POWER_ACCE_ONLY || mode == IMU_POWER_ACCE_GYRO))
		schedule_delayed_work(&ctrb->pollingwork, msecs_to_jiffies(IMU_POLLING_TIME_MS));

	mutex_unlock(&ctrb->power_lock);
	return ret;
}

static int invimu_buffer_postdisable(struct iio_dev *indio_dev)
{
	int ret = 0, mode = 0;
	struct imu_sensor *sensor = iio_priv(indio_dev);
	struct imu_ctrb *ctrb = sensor->ctrb;

	mutex_lock(&ctrb->power_lock);

	switch (sensor->id) {
	case IMU_SENSOR_ID_ACCE:
		mode = iio_buffer_enabled(ctrb->iio_devs[IMU_SENSOR_ID_GYRO]) ?
			IMU_POWER_ACCE_GYRO : IMU_POWER_MODE_DOWN;
		break;
	case IMU_SENSOR_ID_GYRO:
		mode = iio_buffer_enabled(ctrb->iio_devs[IMU_SENSOR_ID_ACCE]) ?
			IMU_POWER_ACCE_ONLY : IMU_POWER_MODE_DOWN;
		break;
	default:
		mode = IMU_POWER_MODE_DOWN;
		break;
	}
	ret = ctrb->chipinfo->mode_set(ctrb, mode);
	if (ret == 0 && mode == IMU_POWER_MODE_DOWN)
		cancel_delayed_work(&ctrb->pollingwork);

	mutex_unlock(&ctrb->power_lock);
	return ret;
}

static const struct iio_buffer_setup_ops invimu_buffer_ops = {
	.preenable = invimu_buffer_preenable,
	.postdisable = invimu_buffer_postdisable,
};

struct iio_dev *invimu_alloc_iiodev(struct imu_ctrb *ctrb,
	const struct iio_info *acce_iio_info, const struct iio_info *gyro_iio_info,
	enum imu_sensor_id id, char *name)
{
	struct imu_sensor *sensor;
	struct iio_dev *indio_dev;

	indio_dev = devm_iio_device_alloc(ctrb->dev, sizeof(*sensor));
	if (!indio_dev)
		return NULL;

	indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_SOFTWARE;

	devm_iio_kfifo_buffer_setup(ctrb->dev, indio_dev, &invimu_buffer_ops);

	sensor = iio_priv(indio_dev);
	sensor->id = id;
	sensor->ctrb = ctrb;

	switch (id) {
	case IMU_SENSOR_ID_ACCE:
		sensor->odr = icm42607_avail_acc_sample_freqs[0];
		indio_dev->info = acce_iio_info;
		indio_dev->channels = invimu_acc_channels;
		indio_dev->num_channels = ARRAY_SIZE(invimu_acc_channels);
		scnprintf(sensor->name, sizeof(sensor->name), "%s_accel", name);
		break;
	case IMU_SENSOR_ID_GYRO:
		sensor->odr = icm42607_avail_gyro_sample_freqs[0];
		indio_dev->info = gyro_iio_info;
		indio_dev->channels = invimu_gyro_channels;
		indio_dev->num_channels = ARRAY_SIZE(invimu_gyro_channels);
		scnprintf(sensor->name, sizeof(sensor->name), "%s_gyro", name);
		break;
	default:
		return NULL;
	}

	indio_dev->name = sensor->name;
	return indio_dev;
}
EXPORT_SYMBOL_GPL(invimu_alloc_iiodev);
