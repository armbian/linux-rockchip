// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/math64.h>
#include <asm/unaligned.h>

#include "inv_icm42670.h"

/**
 *  icm42670_update_period() - Update chip internal period estimation
 *
 *  @st:		driver state
 *  @timestamp:		the interrupt timestamp
 *  @nb:		number of data set in the fifo
 *
 *  This function uses interrupt timestamps to estimate the chip period and
 *  to choose the data timestamp to come.
 */
static void icm42670_update_period(struct icm42670_data *st,
				   s64 timestamp, size_t nb)
{
	s64 interval;

	if (st->it_timestamp != 0)
		st->interrupt_period = div_s64((timestamp - st->it_timestamp), nb);

	interval = (nb - 1) * st->interrupt_period;
	st->data_timestamp = timestamp - interval;
	/* save it timestamp */
	st->it_timestamp = timestamp;
}

/**
 *  icm42670_get_timestamp() - Return the current data timestamp
 *
 *  @st:		driver state
 *  @return:		current data timestamp
 *
 *  This function returns the current data timestamp and prepares for next one.
 */
static s64 icm42670_get_timestamp(struct icm42670_data *st)
{
	s64 ts;

	/* return current data timestamp and increment */
	ts = st->data_timestamp;
	st->data_timestamp += st->interrupt_period;

	return ts;
}

int icm42670_reset_fifo(struct iio_dev *indio_dev)
{
	int ret;
	struct icm42670_data  *st = iio_priv(indio_dev);
	const struct device *dev = regmap_get_device(st->regmap);

	/* reset it timestamp validation */
	st->it_timestamp = 0;

	/* Before setting the FIFO water level, make sure the interrupt source is disabled. */
	ret = regmap_write(st->regmap, REG_INT_SOURCE0, BIT_INT_MODE_OFF);
	if (ret) {
		dev_err(dev, "int_enable failed %d\n",
			ret);
		goto reset_fifo_fail;
	}

	/* disable the sensor output to FIFO */
	ret = regmap_write(st->regmap, REG_FIFO_CONFIG1, BIT_FIFO_MODE_BYPASS);
	if (ret) {
		dev_err(dev, "Failed to write to REG_FIFO_CONFIG1 register!\n");
		goto reset_fifo_fail;
	}

	/* reset FIFO*/
	ret = regmap_update_bits(st->regmap, REG_SIGNAL_PATH_RESET,
				 BIT_FIFO_FLUSH, BIT_FIFO_FLUSH);
	if (ret) {
		dev_err(dev, "Failed to write to REG_SIGNAL_PATH_RESET register!\n");
		goto reset_fifo_fail;
	}
	ndelay(1500); //wait for 1.5us

	/* enable sensor output to FIFO */
	ret = regmap_write(st->regmap, REG_FIFO_CONFIG1,
			 BIT_FIFO_MODE_NO_BYPASS | BIT_FIFO_MODE_STOPFULL);
	if (ret) {
		dev_err(dev, "Failed to write to REG_FIFO_CONFIG1 register!\n");
		goto reset_fifo_fail;
	}

	ret = icm42670_mreg_single_write(st, REG_TMST_CONFIG1_MREG_TOP1,
						 BIT_TMST_EN | BIT_TMST_FSYNC_EN);
	if (ret) {
		dev_err(dev, "Failed to write to REG_TMST_CONFIG1_MREG_TOP1 register!\n");
		goto reset_fifo_fail;
	}

	/*
	 * FIFO_COUNT_FORMAT setting. FIFO_WM_EN must be zero before writing this register.
	 * Interrupt only fires once. This register should be set to nonzero value,
	 * before choosing this interrupt source.
	 * This field should be changed when FIFO is empty to avoid spurious interrupts
	 */
	ret = regmap_write(st->regmap, REG_FIFO_CONFIG2, BIT_FIFO_WM5);
	if (ret) {
		dev_err(dev, "Failed to write to REG_FIFO_CONFIG2 register!\n");
		goto reset_fifo_fail;
	}

	ret = icm42670_mreg_single_write(st, REG_FIFO_CONFIG5_MREG_TOP1,
			BIT_WM_GT_TH | BIT_FIFO_ACCEL_EN |
			BIT_FIFO_GYRO_EN | BIT_FIFO_TMST_FSYNC_EN);
	if (ret) {
		dev_err(dev, "Failed to write to REG_FIFO_CONFIG5_MREG_TOP1 register!\n");
		goto reset_fifo_fail;
	}

	/* Set FIFO interrupt source to INT1*/
	ret = regmap_write(st->regmap, REG_INT_SOURCE0,
			  BIT_INT_RESET_DONE_INT1_EN | BIT_INT_FIFO_THS_INT1_EN);
	if (ret) {
		dev_err(dev, "Failed to write to REG_INT_SOURCE0 register!\n");
		goto reset_fifo_fail;
	}

	st->interrupt_period = st->standard_period;

	return 0;

reset_fifo_fail:
	dev_err(regmap_get_device(st->regmap), "%s :reset fifo failed %d\n", __func__, ret);
	ret = regmap_write(st->regmap, REG_INT_SOURCE0,
			BIT_INT_RESET_DONE_INT1_EN | BIT_INT_FIFO_THS_INT1_EN);

	return ret;
}

/**
 * icm42670_read_fifo() - Transfer data from hardware FIFO to KFIFO.
 */
irqreturn_t icm42670_read_fifo(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct icm42670_data *st = iio_priv(indio_dev);
	const struct device *dev = regmap_get_device(st->regmap);
	size_t bytes_per_datum;
	int result, int_status, int_drdy;
	u8 data[ICM42670_OUTPUT_DATA_SIZE_PULS_ONE], i;
	u16 fifo_count;
	u32 data_len;
	s64 timestamp = 0;

	mutex_lock(&st->lock);

	/* ack interrupt and check status */
	result = regmap_read(st->regmap, REG_INT_STATUS, &int_status);
	if (result) {
		dev_err(dev, "failed to ack interrupt\n");
		goto flush_fifo;
	}

	result = regmap_read(st->regmap, REG_INT_STATUS_DRDY, &int_drdy);
	if (result) {
		dev_err(dev, "failed to ack interrupt\n");
		goto flush_fifo;
	}

	if (!(int_status & BIT_INT_STATUS_FIFO_THS)) {
		dev_warn(dev, "spurious interrupt with status 0x%x\n", int_status);
		if (!(int_drdy & (BIT_INT_STATUS_DRDY)))
			goto flush_fifo;
	}

	if ((int_status & BIT_INT_STATUS_FIFO_FULL)) {
		dev_warn(dev, "the fifo is full\n");
		goto flush_fifo;
	}

	bytes_per_datum = ICM42670_FIFO_DATUM;
	result = regmap_bulk_read(st->regmap, REG_FIFO_BYTE_COUNT1,
				  data, ICM42670_FIFO_COUNT_BYTE);
	if (result) {
		dev_err(dev, "read fifo count fail: %d\n", result);
		goto end_session;
	}

	fifo_count = get_unaligned_be16(&data[0]);
	if (fifo_count > ICM42670_FIFO_COUNT_LIMIT) {
		dev_warn(dev, "fifo overflow reset, cnt: %u\n", fifo_count);
		goto flush_fifo;
	}

	icm42670_update_period(st, pf->timestamp, fifo_count);

	data_len = bytes_per_datum * fifo_count;

	result = regmap_bulk_read(st->regmap, REG_FIFO_DATA_REG,
				  st->data_buff, data_len);
	if (result) {
		dev_err(dev,
			"regmap_bulk_read failed\n");
		goto flush_fifo;
	}

	for (i = 0; i < fifo_count; ++i) {
		/* skip first samples if needed */
		if (st->skip_samples) {
			st->skip_samples--;
			continue;
		}
		memcpy(data, st->data_buff+i*16, bytes_per_datum);
		timestamp = icm42670_get_timestamp(st);
		iio_push_to_buffers_with_timestamp(indio_dev, &(data[1]), timestamp);
	}

end_session:
	mutex_unlock(&st->lock);
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;

flush_fifo:
	/* Flush HW and SW FIFOs. */
	dev_info(dev, "flush info\n");
	icm42670_reset_fifo(indio_dev);
	mutex_unlock(&st->lock);
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

int icm42670_reset_data(struct iio_dev *indio_dev)
{
	int ret;
	struct icm42670_data  *st = iio_priv(indio_dev);
	const struct device *dev = regmap_get_device(st->regmap);

	/* reset it timestamp validation */
	st->it_timestamp = 0;

	ret = regmap_write(st->regmap, REG_INT_SOURCE0, BIT_INT_MODE_OFF);
	if (ret) {
		dev_err(dev, "int_enable failed %d\n",
			ret);
		goto reset_fail;
	}

	/* disable the sensor output to FIFO */
	ret = regmap_write(st->regmap, REG_FIFO_CONFIG1, BIT_FIFO_MODE_BYPASS);
	if (ret) {
		dev_err(dev, "Failed to write to REG_FIFO_CONFIG1 register!\n");
		goto reset_fail;
	}

	ret = regmap_write(st->regmap, REG_INT_SOURCE0,
			BIT_INT_DRDY_INT_EN);
	if (ret) {
		dev_err(dev, "Failed to write to REG_INT_SOURCE0 register!\n");
		goto reset_fail;
	}

	msleep(50);

	st->interrupt_period = st->standard_period;

	return 0;

reset_fail:
	dev_err(regmap_get_device(st->regmap), "%s :reset fifo failed %d\n", __func__, ret);

	return ret;
}

irqreturn_t icm42670_read_data(int irq, void *p)
{
	struct iio_dev *indio_dev = p;
	struct icm42670_data *st = iio_priv(indio_dev);
	const struct device *dev = regmap_get_device(st->regmap);
	int ret, int_drdy;
	u8 data[ICM42670_OUTPUT_DATA_SIZE_PULS_ONE];

	mutex_lock(&st->lock);

	ret = regmap_read(st->regmap, REG_INT_STATUS_DRDY, &int_drdy);
	if (ret) {
		dev_err(dev, "failed to ack interrupt\n");
		goto read_fail;
	}

	if (!(int_drdy & (BIT_INT_STATUS_DRDY)))
		goto read_fail;

	ret = regmap_bulk_read(st->regmap, REG_ACCEL_DATA_X0_UI,
		(u8 *)data, ICM42670_ACCEL_GYRO_SIZE);
	if (ret) {
		dev_err(dev, "regmap_bulk_read accel+gyro failed\n");
		goto read_fail;
	}

	ret = regmap_bulk_read(st->regmap, REG_TEMP_DATA0_UI,
		(u8 *)&data[ICM42670_ACCEL_GYRO_SIZE], 2);
	if (ret) {
		dev_err(dev, "regmap_bulk_read temperature failed\n");
		goto read_fail;
	}

	iio_push_to_buffers_with_timestamp(indio_dev, &(data[0]), st->data_timestamp);

	mutex_unlock(&st->lock);
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;

read_fail:
	dev_info(dev, "read data fail\n");
	mutex_unlock(&st->lock);
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}
