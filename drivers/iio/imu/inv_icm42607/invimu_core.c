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
#include "invimu_iio.h"
#include "invimu_core.h"
#include "imu.h"

static ssize_t invimu_debug_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int val = -1;
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct imu_sensor *sensor = iio_priv(indio_dev);
	struct imu_ctrb *ctrb = sensor->ctrb;

	if (sscanf(buf, "%d\n", &val) != 1) {
		dev_err(ctrb->dev, "debugon para err\n");
		return -1;
	}
	ctrb->debugon = val;
	dev_info(ctrb->dev, "debugon set %d\n", val);
	return size;
}

static IIO_DEVICE_ATTR(in_imu_debug, 0200, NULL, invimu_debug_store, 0);

static struct attribute *invimu_accel_attributes[] = {
	&iio_dev_attr_in_imu_debug.dev_attr.attr,
	NULL,
};

static const struct attribute_group invimu_accel_attribute_group = {
	.attrs = invimu_accel_attributes,
};

static const struct iio_info invimu_acc_info = {
	.attrs = &invimu_accel_attribute_group,
	.read_raw = invimu_read_raw,
	.read_avail = invimu_read_avail,
	.write_raw = invimu_write_raw,
	.write_raw_get_fmt = invimu_write_raw_get_fmt,
};

static struct attribute *invimu_anglvel_attributes[] = {
	NULL,
};

static const struct attribute_group invimu_anglvel_attribute_group = {
	.attrs = invimu_anglvel_attributes,
};

static const struct iio_info invimu_gryo_info = {
	.attrs = &invimu_anglvel_attribute_group,
	.read_raw = invimu_read_raw,
	.read_avail = invimu_read_avail,
	.write_raw = invimu_write_raw,
	.write_raw_get_fmt = invimu_write_raw_get_fmt,
};

static void invimu_axis_transposition(struct imu_3axis_data *rawdata, int position)
{
	struct imu_3axis_data tempdata;

	memcpy(&tempdata, rawdata, sizeof(struct imu_3axis_data));
	if (position == IMU_P_D_90) {
		rawdata->raw[0] = tempdata.raw[1];
		rawdata->raw[1] = 0 - tempdata.raw[0];
		rawdata->raw[2] = tempdata.raw[2];
	} else if (position == IMU_P_D_270) {
		rawdata->raw[0] = tempdata.raw[1];
		rawdata->raw[1] = tempdata.raw[0];
		rawdata->raw[2] = tempdata.raw[2];
	} else if (position == IMU_P_D_180) {
		rawdata->raw[0] = tempdata.raw[0];
		rawdata->raw[1] = -tempdata.raw[1];
		rawdata->raw[2] = tempdata.raw[2];
	}
}

static void invimu_axis_print(struct imu_ctrb *ctrb, struct imu_sensor *sensor)
{
	int print_on = 0;

	if (ctrb == NULL || sensor == NULL)
		return;

	print_on = (sensor->readcnt != 0) && ((sensor->readcnt % 6000) == 0);
	print_on = (print_on) || (ctrb->debugon);
	if (print_on) {
		if (sensor->id == IMU_SENSOR_ID_ACCE) {
			dev_info(ctrb->dev, "acce read cnt=%d, raw=%d,%d,%d\n", sensor->readcnt,
				sensor->rawdata.raw[0],
				sensor->rawdata.raw[1],
				sensor->rawdata.raw[2]);
			dev_info(ctrb->dev, "acce calib offset=%d,%d,%d\n",
				sensor->offset_x, sensor->offset_y, sensor->offset_z);
		}
		if (sensor->id == IMU_SENSOR_ID_GYRO) {
			dev_info(ctrb->dev, "gyro read cnt=%d, raw=%d,%d,%d\n", sensor->readcnt,
				sensor->rawdata.raw[0],
				sensor->rawdata.raw[1],
				sensor->rawdata.raw[2]);
			dev_info(ctrb->dev, "gyro calib offset=%d,%d,%d\n",
				sensor->offset_x, sensor->offset_y, sensor->offset_z);
		}
	}
}

static int invimu_data_report(struct imu_ctrb *ctrb)
{
	int i, ret = 0;
	struct iio_dev *indio_dev;
	struct imu_sensor *sensor;
	struct imu_3axis_data rawdata;

	if (ctrb == NULL)
		return -1;

	for (i = 0; i < IMU_SENSOR_ID_MAX; i++) {
		indio_dev = ctrb->iio_devs[i];
		sensor = iio_priv(indio_dev);

		if (iio_buffer_enabled(indio_dev)) {
			memset(&rawdata, 0, sizeof(struct imu_3axis_data));
			sensor->readcnt++;
			switch (sensor->id) {
			case IMU_SENSOR_ID_ACCE:
				ret = ctrb->chipinfo->read_acce_raw(ctrb, &rawdata);
				break;
			case IMU_SENSOR_ID_GYRO:
				ret = ctrb->chipinfo->read_gyro_raw(ctrb, &rawdata);
				break;
			default:
				continue;
			}
			invimu_axis_transposition(&rawdata, ctrb->position);
			memcpy(&sensor->rawdata, &rawdata, sizeof(struct imu_3axis_data));
			iio_push_to_buffers_with_timestamp(indio_dev,
						&rawdata, ktime_get_boottime_ns());
			invimu_axis_print(ctrb, sensor);
			//invimu_calibrator_process(ctrb, sensor);
		}
	}
	return ret;
}

static void invimu_work_handler(struct work_struct *work)
{
	struct imu_ctrb *ctrb;

	ctrb = (struct imu_ctrb *)container_of(work, struct imu_ctrb, pollingwork.work);
	invimu_data_report(ctrb);
	schedule_delayed_work(&ctrb->pollingwork, msecs_to_jiffies(IMU_POLLING_TIME_MS));
}

int invimu_chip_init(struct imu_ctrb *ctrb, bool use_spi)
{
	if (ctrb == NULL)
		return -1;

	return ctrb->chipinfo->chip_init(ctrb);
}
EXPORT_SYMBOL_GPL(invimu_chip_init);

static int invimu_parse_dt_parameters(struct device *dev, struct imu_ctrb *ctrb)
{
	int position = 0;
	struct device_node *np = dev->of_node;

	ctrb->position = 0;
	if (np != NULL) {
		if (of_property_read_s32(np, "position", &position) == 0) {
			if (position < IMU_P_D_0 || position >= IMU_D_MAX)
				goto HWCIMU_PARSE_DT_ERR;
			else
				ctrb->position = position;
		} else {
			goto HWCIMU_PARSE_DT_ERR;
		}
	} else {
		goto HWCIMU_PARSE_DT_ERR;
	}
	dev_info(ctrb->dev, "imu position sets %d\n", ctrb->position);
	return 0;

HWCIMU_PARSE_DT_ERR:
	dev_err(ctrb->dev, "imu position sets default %d\n", ctrb->position);
	return -ENODEV;
}

int invimu_core_probe(struct device *dev, struct regmap *regmap, int irq, bool use_spi)
{
	int i, ret = 0;
	struct imu_ctrb *ctrb;
	struct imu_info *info = NULL;

	ctrb = devm_kzalloc(dev, sizeof(*ctrb), GFP_KERNEL);
	if (!ctrb)
		return -ENOMEM;

	dev_set_drvdata(dev, (void *)ctrb);
	mutex_init(&ctrb->power_lock);
	ctrb->dev = dev;
	ctrb->regmap = regmap;
	ctrb->irq = irq;
	ctrb->debugon = 0;
	dev_info(ctrb->dev, "probe start\n");

	info = icm42607_chip_probe(ctrb);
	if (info == NULL) {
		dev_err(ctrb->dev, "no chip probed!\n");
		return -ENODEV;
	}
	ctrb->chipinfo = info;

	ret = invimu_chip_init(ctrb, use_spi);
	if (ret) {
		dev_err(ctrb->dev, "chip err\n");
		return ret;
	}

	for (i = 0; i < IMU_SENSOR_ID_MAX; i++) {
		ctrb->iio_devs[i] = invimu_alloc_iiodev(ctrb,
			&invimu_acc_info, &invimu_gryo_info, i, ctrb->chipinfo->name);
		if (!ctrb->iio_devs[i]) {
			dev_err(ctrb->dev, "iio alloc err\n");
			return -ENOMEM;
		}
		ret = devm_iio_device_register(ctrb->dev, ctrb->iio_devs[i]);
		if (ret) {
			dev_err(ctrb->dev, "iio register err\n");
			return ret;
		}
	}

	invimu_parse_dt_parameters(dev, ctrb);
	INIT_DELAYED_WORK(&ctrb->pollingwork, invimu_work_handler);

	dev_info(ctrb->dev, "probe end\n");
	return 0;
}
EXPORT_SYMBOL_GPL(invimu_core_probe);

MODULE_DESCRIPTION("inv imu driver");
MODULE_LICENSE("GPL");
