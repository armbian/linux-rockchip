/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __INVIMU_IIO_H
#define __INVIMU_IIO_H

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>

int invimu_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask);

int invimu_read_avail(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, const int **vals, int *type, int *length, long mask);

int invimu_write_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int val, int val2, long mask);

int invimu_write_raw_get_fmt(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, long mask);

struct iio_dev *invimu_alloc_iiodev(struct imu_ctrb *ctrb,
	const struct iio_info *acce_iio_info, const struct iio_info *gyro_iio_info,
	enum imu_sensor_id id, char *name);

#endif
