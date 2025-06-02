// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */


#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>

#include "inv_icm42670.h"

static int icm42670_read_raw(struct iio_dev *indio_dev,
					struct iio_chan_spec const *chan,
					int *val, int *val2, long mask);

static int icm42670_write_raw_get_fmt(struct iio_dev *indio_dev,
					struct iio_chan_spec const *chan,
					long mask);

static int icm42670_set_scale(struct icm42670_data *data,
					enum icm42670_sensor_type t,
					int scale, int uscale);

static int icm42670_write_raw(struct iio_dev *indio_dev,
					struct iio_chan_spec const *chan,
					int val, int val2, long mask);

static int icm42670_set_odr(struct icm42670_data *data,
					enum icm42670_sensor_type t,
					int odr);

static int icm42670_temp_read_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long mask);

#define ICM42670_CHANNEL(_type, _axis, _index) {		\
	.type = _type,						\
	.modified = 1,						\
	.channel2 = _axis,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |  \
		BIT(IIO_CHAN_INFO_SAMP_FREQ),			\
	.scan_index = _index,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.endianness = IIO_BE,				\
	},							\
}

#define ICM42670_TEMP_CHANNEL_8BIT(_type, _index) {			\
	.type = _type,						\
	.modified = 1,						\
	.channel2 = 1,					\
	.info_mask_separate =					\
		BIT(IIO_CHAN_INFO_RAW) |			\
		BIT(IIO_CHAN_INFO_OFFSET) |			\
		BIT(IIO_CHAN_INFO_SCALE),			\
	.scan_index = _index,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 8,					\
		.storagebits = 8,				\
		.endianness = IIO_BE,				\
	},							\
}

#define ICM42670_TEMP_CHANNEL_16BIT(_type, _index) {			\
	.type = _type,						\
	.modified = 1,						\
	.channel2 = 1,					\
	.info_mask_separate =					\
		BIT(IIO_CHAN_INFO_RAW) |			\
		BIT(IIO_CHAN_INFO_OFFSET) |			\
		BIT(IIO_CHAN_INFO_SCALE),			\
	.scan_index = _index,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.endianness = IIO_BE,				\
	},							\
}

static const struct iio_chan_spec icm42670_fifo_channels[] = {
	ICM42670_CHANNEL(IIO_ACCEL, IIO_MOD_X, ICM42670_SCAN_ACCEL_X),
	ICM42670_CHANNEL(IIO_ACCEL, IIO_MOD_Y, ICM42670_SCAN_ACCEL_Y),
	ICM42670_CHANNEL(IIO_ACCEL, IIO_MOD_Z, ICM42670_SCAN_ACCEL_Z),
	ICM42670_CHANNEL(IIO_ANGL_VEL, IIO_MOD_X, ICM42670_SCAN_GYRO_X),
	ICM42670_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Y, ICM42670_SCAN_GYRO_Y),
	ICM42670_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Z, ICM42670_SCAN_GYRO_Z),
	ICM42670_TEMP_CHANNEL_8BIT(IIO_TEMP, ICM42670_SCAN_TEMP),
	IIO_CHAN_SOFT_TIMESTAMP(ICM42670_SCAN_TIMESTAMP),
};

static const struct iio_chan_spec icm42670_channels[] = {
	ICM42670_CHANNEL(IIO_ACCEL, IIO_MOD_X, ICM42670_SCAN_ACCEL_X),
	ICM42670_CHANNEL(IIO_ACCEL, IIO_MOD_Y, ICM42670_SCAN_ACCEL_Y),
	ICM42670_CHANNEL(IIO_ACCEL, IIO_MOD_Z, ICM42670_SCAN_ACCEL_Z),
	ICM42670_CHANNEL(IIO_ANGL_VEL, IIO_MOD_X, ICM42670_SCAN_GYRO_X),
	ICM42670_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Y, ICM42670_SCAN_GYRO_Y),
	ICM42670_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Z, ICM42670_SCAN_GYRO_Z),
	ICM42670_TEMP_CHANNEL_16BIT(IIO_TEMP, ICM42670_SCAN_TEMP),
	IIO_CHAN_SOFT_TIMESTAMP(ICM42670_SCAN_TIMESTAMP),
};

static const unsigned long icm42670_scan_masks[] = {
	/* 3-axis accel + temp */
	BIT(ICM42670_SCAN_ACCEL_X)
		| BIT(ICM42670_SCAN_ACCEL_Y)
		| BIT(ICM42670_SCAN_ACCEL_Z)
		| BIT(ICM42670_SCAN_TEMP),
	/* 3-axis gyro + temp */
	BIT(ICM42670_SCAN_GYRO_X)
		| BIT(ICM42670_SCAN_GYRO_Y)
		| BIT(ICM42670_SCAN_GYRO_Z)
		| BIT(ICM42670_SCAN_TEMP),
	/* 6-axis accel + gyro + temp */
	BIT(ICM42670_SCAN_ACCEL_X)
		| BIT(ICM42670_SCAN_ACCEL_Y)
		| BIT(ICM42670_SCAN_ACCEL_Z)
		| BIT(ICM42670_SCAN_GYRO_X)
		| BIT(ICM42670_SCAN_GYRO_Y)
		| BIT(ICM42670_SCAN_GYRO_Z)
		| BIT(ICM42670_SCAN_TEMP),
	0,
};

struct icm42670_regs {
	u8 data;
	u8 config;
	u8 config_odr_mask;
	u8 config_fsr_mask;
};

static struct icm42670_regs icm42670_regs[] = {
	[ICM42670_ACCEL] = {
		.data	= REG_ACCEL_DATA_X0_UI,
		.config	= REG_ACCEL_CONFIG0,
		.config_odr_mask = BIT_ACCEL_ODR_MASK,
		.config_fsr_mask = BIT_ACCEL_UI_FS_SEL_MASK,
	},
	[ICM42670_GYRO] = {
		.data	= REG_GYRO_DATA_X0_UI,
		.config	= REG_GYRO_CONFIG0,
		.config_odr_mask = BIT_GYRO_ODR_MASK,
		.config_fsr_mask = BIT_GYRO_UI_FS_SEL_MASK,
	},
	[ICM42670_TEMP] = {
		.data = REG_TEMP_DATA0_UI,
		.config = REG_TEMP_CONFIG0,
		.config_odr_mask = 0,
		.config_fsr_mask = 0,
	},
};

struct icm42670_filter_freq {
	u8 reg_val;
	u16 freq;
};

static const struct icm42670_filter_freq accel_filter_freq_table[] = {
	{BIT_ACC_FILT_BW_IND_BYPASS, 0},      /* BYPASS */
	{BIT_ACC_FILT_BW_IND_180HZ, 180},     /* 180Hz */
	{BIT_ACC_FILT_BW_IND_121HZ, 121},     /* 121Hz */
	{BIT_ACC_FILT_BW_IND_73HZ, 73},       /* 73Hz */
	{BIT_ACC_FILT_BW_IND_53HZ, 53},       /* 53Hz */
	{BIT_ACC_FILT_BW_IND_34HZ, 34},       /* 34Hz */
	{BIT_ACC_FILT_BW_IND_25HZ, 25},       /* 25Hz */
	{BIT_ACC_FILT_BW_IND_16HZ, 16},       /* 16Hz */
};

static const struct icm42670_filter_freq gyro_filter_freq_table[] = {
	{BIT_GYR_UI_FLT_BW_BYPASS, 0},      /* BYPASS */
	{BIT_GYR_UI_FLT_BW_180HZ, 180},     /* 180Hz */
	{BIT_GYR_UI_FLT_BW_121HZ, 121},     /* 121Hz */
	{BIT_GYR_UI_FLT_BW_73HZ, 73},       /* 73Hz */
	{BIT_GYR_UI_FLT_BW_53HZ, 53},       /* 53Hz */
	{BIT_GYR_UI_FLT_BW_34HZ, 34},       /* 34Hz */
	{BIT_GYR_UI_FLT_BW_25HZ, 25},       /* 25Hz */
	{BIT_GYR_UI_FLT_BW_16HZ, 16},       /* 16Hz */
};

static IIO_CONST_ATTR(in_accel_sampling_frequency_available,
			"1 3 6 12 25 50 100 200 400 800 1600");
static IIO_CONST_ATTR(in_anglvel_sampling_frequency_available,
			"12 25 50 100 200 400 800 1600");
static IIO_CONST_ATTR(in_accel_scale_available,
			"0.000598 0.001196 0.002393 0.004785");
static IIO_CONST_ATTR(in_anglvel_scale_available,
			"0.000133231 0.000266462 0.000532113 0.001064225");
static IIO_CONST_ATTR(in_accel_lpf_bw_available,
			"0 16 25 34 53 73 121 180");
static IIO_CONST_ATTR(in_anglvel_lpf_bw_available,
			"0 16 25 34 53 73 121 180");

static u8 freq_to_reg_val(u16 freq, bool is_accel)
{
	const struct icm42670_filter_freq *table;
	int size, i;

	if (is_accel) {
		table = accel_filter_freq_table;
		size = ARRAY_SIZE(accel_filter_freq_table);
	} else {
		table = gyro_filter_freq_table;
		size = ARRAY_SIZE(gyro_filter_freq_table);
	}

	for (i = 0; i < size; i++) {
		if (table[i].freq == freq)
			return table[i].reg_val;
	}

	return 0;
}

static u16 reg_val_to_freq(u8 reg_val, bool is_accel)
{
	const struct icm42670_filter_freq *table;
	int size, i;

	if (is_accel) {
		table = accel_filter_freq_table;
		size = ARRAY_SIZE(accel_filter_freq_table);
	} else {
		table = gyro_filter_freq_table;
		size = ARRAY_SIZE(gyro_filter_freq_table);
	}

	for (i = 0; i < size; i++) {
		if (table[i].reg_val == reg_val)
			return table[i].freq;
	}

	return 0;
}

static int icm42670_set_lpf_bw(struct icm42670_data *data,
			      enum icm42670_sensor_type t,
			      u16 freq)
{
	int ret;
	unsigned int reg;
	u8 filter_val;
	u8 reg_addr;
	u32 mask;

	mutex_lock(&data->lock);

	if (t == ICM42670_ACCEL) {
		reg_addr = REG_ACCEL_CONFIG1;
		mask = ACCEL_CONFIG1_ACCEL_UI_FILT_BW_MASK;
		filter_val = freq_to_reg_val(freq, true);
		data->accel_lpf_bw = freq;
	} else {
		reg_addr = REG_GYRO_CONFIG1;
		mask = GYRO_CONFIG1_GYRO_UI_FILT_BW_MASK;
		filter_val = freq_to_reg_val(freq, false);
		data->gyro_lpf_bw = freq;
	}

	ret = regmap_read(data->regmap, reg_addr, &reg);
	if (ret)
		goto unlock;

	reg &= ~mask;
	reg |= filter_val;

	ret = regmap_write(data->regmap, reg_addr, reg);

unlock:
	mutex_unlock(&data->lock);
	return ret;
}

static ssize_t in_accel_lpf_bw_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct icm42670_data *data = iio_priv(indio_dev);
	u16 freq = reg_val_to_freq(data->accel_lpf_bw, true);

	return sprintf(buf, "%u\n", freq);
}

static ssize_t in_accel_lpf_bw_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct icm42670_data *data = iio_priv(indio_dev);
	unsigned long freq;
	int ret;

	ret = kstrtoul(buf, 10, &freq);
	if (ret)
		return ret;

	ret = icm42670_set_lpf_bw(data, ICM42670_ACCEL, freq);
	if (ret)
		return ret;

	return count;
}

static ssize_t in_anglvel_lpf_bw_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct icm42670_data *data = iio_priv(indio_dev);
	u16 freq = reg_val_to_freq(data->gyro_lpf_bw, false);

	return sprintf(buf, "%u\n", freq);
}

static ssize_t in_anglvel_lpf_bw_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct icm42670_data *data = iio_priv(indio_dev);
	unsigned long freq;
	int ret;

	ret = kstrtoul(buf, 10, &freq);
	if (ret)
		return ret;

	ret = icm42670_set_lpf_bw(data, ICM42670_GYRO, freq);
	if (ret)
		return ret;

	return count;
}

static IIO_DEVICE_ATTR(in_accel_lpf_bw, 0644,
		      in_accel_lpf_bw_show, in_accel_lpf_bw_store, 0);
static IIO_DEVICE_ATTR(in_anglvel_lpf_bw, 0644,
		      in_anglvel_lpf_bw_show, in_anglvel_lpf_bw_store, 0);

static struct attribute *icm42670_attrs[] = {
	&iio_const_attr_in_accel_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_accel_scale_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_scale_available.dev_attr.attr,
	&iio_const_attr_in_accel_lpf_bw_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_lpf_bw_available.dev_attr.attr,
	&iio_dev_attr_in_accel_lpf_bw.dev_attr.attr,
	&iio_dev_attr_in_anglvel_lpf_bw.dev_attr.attr,
	NULL,
};

static const struct attribute_group icm42670_attrs_group = {
	.attrs = icm42670_attrs,
};

static const struct iio_info icm42670_info = {
	.read_raw = icm42670_read_raw,
	.write_raw = icm42670_write_raw,
	.write_raw_get_fmt = &icm42670_write_raw_get_fmt,
	.attrs = &icm42670_attrs_group,
};

struct icm42670_odr {
	u8 bits;
	int odr;
	int divider;		//us
};

struct icm42670_odr_item {
	const struct icm42670_odr *tbl;
	int num;
};

static const struct icm42670_odr icm42670_accel_odr[] = {
	{BIT_SENSOR_ODR_1600HZ, 1600, 1600},
	{BIT_SENSOR_ODR_800HZ, 800, -1250},
	{BIT_SENSOR_ODR_400HZ, 400, -2500},
	{BIT_SENSOR_ODR_200HZ, 200, -5000},
	{BIT_SENSOR_ODR_100HZ, 100, -10000},
	{BIT_SENSOR_ODR_50HZ, 50, -20000},
	{BIT_SENSOR_ODR_25HZ, 25, -40000},
	{BIT_SENSOR_ODR_12HZ, 12, -80000},
	{BIT_SENSOR_ODR_6HZ, 6, -160000},
	{BIT_SENSOR_ODR_3HZ, 3, -320000},
	{BIT_SENSOR_ODR_1HZ, 1, -640000},
};

static const struct icm42670_odr icm42670_gyro_odr[] = {
	{BIT_SENSOR_ODR_1600HZ, 1600, 1600},
	{BIT_SENSOR_ODR_800HZ, 800, -1250},
	{BIT_SENSOR_ODR_400HZ, 400, -2500},
	{BIT_SENSOR_ODR_200HZ, 200, -5000},
	{BIT_SENSOR_ODR_100HZ, 100, -10000},
	{BIT_SENSOR_ODR_50HZ, 50, -20000},
	{BIT_SENSOR_ODR_25HZ, 25, -40000},
	{BIT_SENSOR_ODR_12HZ, 12, -80000},
};

static const struct  icm42670_odr_item icm42670_odr_table[] = {
	[ICM42670_ACCEL] = {
		.tbl	= icm42670_accel_odr,
		.num	= ARRAY_SIZE(icm42670_accel_odr),
	},
	[ICM42670_GYRO] = {
		.tbl	= icm42670_gyro_odr,
		.num	= ARRAY_SIZE(icm42670_gyro_odr),
	},
};

struct icm42670_scale {
	u8 bits;
	int scale;
	int uscale;
};

struct icm42670_scale_item {
	const struct icm42670_scale *tbl;
	int num;
	int format;
};

/*
 * this is the accel scale translated from dynamic range plus/minus
 * {2, 4, 8, 16} to m/s^2
 * Formula: Range / Sampling Accuracy * Gravity Acceleration
 * For ±2g full scale, 598 = 4(-2g, -1g, 1g, 2g) / 2^16 * 9.8(1g) * 1000000
 * For ±4g full scale, 1196 = 8 / 2^16 * 9.8 * 1000000
 */
static const struct icm42670_scale icm42670_accel_scale[] = {
	{BIT_ACCEL_FSR_2G, 0, 598},
	{BIT_ACCEL_FSR_4G, 0, 1196},
	{BIT_ACCEL_FSR_8G, 0, 2393},
	{BIT_ACCEL_FSR_16G, 0, 4785},
};

/*
 * this is the gyro scale translated from dynamic range plus/minus
 * {250, 500, 1000, 2000} to rad/s
 * Formula: Range / sampling accuracy * Radians
 * For ±2000DPS full scale, 1064225 = 4000 / 2^16 * (π/180) * 1000000000
 */
static const struct icm42670_scale icm42670_gyro_scale[] = {
	{ BIT_GYRO_FSR_250DPS, 0, 133231},
	{ BIT_GYRO_FSR_500DPS, 0, 266462},
	{ BIT_GYRO_FSR_1000DPS, 0, 532113},
	{ BIT_GYRO_FSR_2000DPS, 0, 1064225},
};

static const struct  icm42670_scale_item icm42670_scale_table[] = {
	[ICM42670_ACCEL] = {
		.tbl	= icm42670_accel_scale,
		.num	= ARRAY_SIZE(icm42670_accel_scale),
		.format = IIO_VAL_INT_PLUS_MICRO,
	},
	[ICM42670_GYRO] = {
		.tbl	= icm42670_gyro_scale,
		.num	= ARRAY_SIZE(icm42670_gyro_scale),
		.format = IIO_VAL_INT_PLUS_NANO,
	},
};

const struct regmap_config icm42670_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
EXPORT_SYMBOL(icm42670_regmap_config);

static enum icm42670_sensor_type icm42670_to_sensor(enum iio_chan_type iio_type)
{
	switch (iio_type) {
	case IIO_ACCEL:
		return ICM42670_ACCEL;
	case IIO_ANGL_VEL:
		return ICM42670_GYRO;
	case IIO_TEMP:
		return ICM42670_TEMP;
	default:
		return -EINVAL;
	}
}

static int icm42670_gyro_on(struct icm42670_data *data)
{
	int ret;
	unsigned int cmd;

	// Enable Gyro
	ret = regmap_read(data->regmap, REG_PWR_MGMT_0, &cmd);
	if (ret < 0)
		return ret;

	cmd |= BIT_PWR_MGMT0_GYRO(INV_ICM42670_SENSOR_MODE_LOW_NOISE);  // Gyro on in LNM
	usleep_range(50, 51);
	ret = regmap_write(data->regmap, REG_PWR_MGMT_0, cmd);
	if (ret < 0)
		return ret;
	usleep_range(200, 201);

	return ret;
}

static int icm42670_gyro_off(struct icm42670_data *data)
{
	int ret;
	unsigned int cmd;

	ret = regmap_read(data->regmap, REG_PWR_MGMT_0, &cmd);
	if (ret < 0)
		return ret;

	cmd &= (~(BIT_PWR_MGMT0_GYRO(INV_ICM42670_SENSOR_MODE_LOW_NOISE)));	 // Gyro off
	ret = regmap_write(data->regmap, REG_PWR_MGMT_0, cmd);
	if (ret < 0)
		return ret;
	usleep_range(200, 201);
	return ret;
}

static int icm42670_accel_on(struct icm42670_data *data)
{
	int ret;
	unsigned int cmd;

	ret = regmap_read(data->regmap, REG_PWR_MGMT_0, &cmd);
	if (ret < 0)
		return ret;
	cmd |= (BIT_PWR_MGMT0_ACCEL(INV_ICM42670_SENSOR_MODE_LOW_NOISE));	  // Accel on in LNM
	ret = regmap_write(data->regmap, REG_PWR_MGMT_0, cmd);
	if (ret < 0)
		return ret;
	usleep_range(200, 201);

	return ret;
}

static int icm42670_accel_off(struct icm42670_data *data)
{
	int ret;
	unsigned int cmd;

	ret = regmap_read(data->regmap, REG_PWR_MGMT_0, &cmd);
	if (ret < 0)
		return ret;
	cmd &= (~(BIT_PWR_MGMT0_ACCEL(INV_ICM42670_SENSOR_MODE_LOW_NOISE)));	  // Accel on in LNM
	usleep_range(50, 51);
	ret = regmap_write(data->regmap, REG_PWR_MGMT_0, cmd);
	if (ret < 0)
		return ret;
	usleep_range(200, 201);
	return ret;
}

int icm42670_set_mode(struct icm42670_data *data,
				enum icm42670_sensor_type t,
				bool mode)
{
	int ret;

	if (t == ICM42670_GYRO) {
		if (mode)
			ret = icm42670_gyro_on(data);
		else
			ret = icm42670_gyro_off(data);
	} else if (t == ICM42670_ACCEL) {
		if (mode)
			ret = icm42670_accel_on(data);
		else
			ret = icm42670_accel_off(data);
	} else {
		return -EINVAL;
	}

	return ret;
}

/**
 * icm42670_mreg_read() - Multiple byte read from MREG area.
 * @st: struct icm42670_data.
 * @addr: MREG register start address including bank in upper byte.
 * @len: length to read in byte.
 * @data: pointer to store read data.
 *
 * Return: 0 when successful.
 */
int icm42670_mreg_read(struct icm42670_data *st, int addr, int len, u32 *data)
{
	int ret;
	u32 reg_pwr_mgmt_0;

	ret = regmap_read(st->regmap, REG_PWR_MGMT_0, &reg_pwr_mgmt_0);
	if (ret)
		return ret;

	//TODO: set idle

	ret = regmap_write(st->regmap, REG_BLK_SEL_R, (addr >> 8) & 0xff);
	usleep_range(ICM42670_BLK_SEL_WAIT_US,
			ICM42670_BLK_SEL_WAIT_US + 1);
	if (ret)
		goto restore_bank;

	ret = regmap_write(st->regmap, REG_MADDR_R, addr & 0xff);
	usleep_range(ICM42670_MADDR_WAIT_US,
			ICM42670_MADDR_WAIT_US + 1);
	if (ret)
		goto restore_bank;

	ret = regmap_read(st->regmap, REG_M_R, data);
	usleep_range(ICM42670_M_RW_WAIT_US,
			ICM42670_M_RW_WAIT_US + 1);
	if (ret)
		goto restore_bank;

restore_bank:
	ret |= regmap_write(st->regmap, REG_BLK_SEL_R, 0);
	usleep_range(ICM42670_BLK_SEL_WAIT_US,
			ICM42670_BLK_SEL_WAIT_US + 1);

	ret |= regmap_write(st->regmap, REG_PWR_MGMT_0, reg_pwr_mgmt_0);

	return ret;
}

/**
 * icm42670_mreg_single_write() - Single byte write to MREG area.
 * @st: struct icm42670_data.
 * @addr: MREG register address including bank in upper byte.
 * @data: data to write.
 *
 * Return: 0 when successful.
 */
int icm42670_mreg_single_write(struct icm42670_data *st, int addr, u32 data)
{
	int ret;
	u32 reg_pwr_mgmt_0;

	ret = regmap_read(st->regmap, REG_PWR_MGMT_0, &reg_pwr_mgmt_0);
	if (ret)
		return ret;

	//TODO: set idle

	ret = regmap_write(st->regmap, REG_BLK_SEL_W, (addr >> 8) & 0xff);
	usleep_range(ICM42670_BLK_SEL_WAIT_US,
			ICM42670_BLK_SEL_WAIT_US + 1);
	if (ret)
		goto restore_bank;

	ret = regmap_write(st->regmap, REG_MADDR_W, addr & 0xff);
	usleep_range(ICM42670_MADDR_WAIT_US,
			ICM42670_MADDR_WAIT_US + 1);
	if (ret)
		goto restore_bank;

	ret = regmap_write(st->regmap, REG_M_W, data);
	usleep_range(ICM42670_M_RW_WAIT_US,
			ICM42670_M_RW_WAIT_US + 1);
	if (ret)
		goto restore_bank;

restore_bank:
	ret |= regmap_write(st->regmap, REG_BLK_SEL_W, 0);
	usleep_range(ICM42670_BLK_SEL_WAIT_US,
			ICM42670_BLK_SEL_WAIT_US + 1);

	ret |= regmap_write(st->regmap, REG_PWR_MGMT_0, reg_pwr_mgmt_0);

	return ret;
}

static int icm42670_get_scale(struct icm42670_data *data,
					enum icm42670_sensor_type t,
					int *scale, int *uscale)
{
	int i, ret, val;

	if (t < 0 || (t >= ARRAY_SIZE(icm42670_scale_table)))
		return -EINVAL;

	ret = regmap_read(data->regmap, icm42670_regs[t].config, &val);
	if (ret < 0)
		return ret;

	val = ((val) & (icm42670_regs[t].config_fsr_mask)) >>
		__builtin_ctzl(icm42670_regs[t].config_fsr_mask);

	for (i = 0; i < icm42670_scale_table[t].num; i++)
		if (icm42670_scale_table[t].tbl[i].bits == val) {

			*scale = icm42670_scale_table[t].tbl[i].scale;
			*uscale = icm42670_scale_table[t].tbl[i].uscale;

			return icm42670_scale_table[t].format;
		}

	return -EINVAL;
}

static int icm42670_get_odr(struct icm42670_data *data,
					enum icm42670_sensor_type t,
					int *odr)
{
	int i, val, ret;

	if (t < 0 || t >= ARRAY_SIZE(icm42670_odr_table))
		return 0;

	ret = regmap_read(data->regmap, icm42670_regs[t].config, &val);
	if (ret < 0)
		return ret;

	val = ((val) & (icm42670_regs[t].config_odr_mask)) >>
		__builtin_ctzl(icm42670_regs[t].config_odr_mask);

	for (i = 0; i < icm42670_odr_table[t].num; i++)
		if (val == icm42670_odr_table[t].tbl[i].bits)
			break;

	if (i >= icm42670_odr_table[t].num)
		return -EINVAL;

	*odr = icm42670_odr_table[t].tbl[i].odr;

	return 0;
}

static int icm42670_get_data(struct icm42670_data *data,
					enum icm42670_sensor_type t,
					int axis, int *val)
{
	u8 reg;
	int ret;
	__be16 sample;

	if (t == ICM42670_TEMP)
		reg = icm42670_regs[t].data;
	else
		reg = icm42670_regs[t].data + (axis - IIO_MOD_X) * sizeof(sample);

	ret = regmap_bulk_read(data->regmap, reg, (u8 *)&sample, 2);
	if (ret)
		return -EINVAL;

	*val = (short)be16_to_cpup(&sample);
	dev_info(regmap_get_device(data->regmap),
			"icm42670get_data: ch: %d, aix: %d, reg: %x, val: %x\n",
			t, axis, reg, *val);

	return ret;
}

static int icm42670_read_raw(struct iio_dev *indio_dev,
				 struct iio_chan_spec const *chan,
				 int *val, int *val2, long mask)
{
	int ret;
	struct icm42670_data *data = iio_priv(indio_dev);

	switch (chan->type) {
	case IIO_ANGL_VEL:
	case IIO_ACCEL:
		break;
	case IIO_TEMP:
		return icm42670_temp_read_raw(indio_dev, chan, val, val2, mask);
	default:
		return -EINVAL;
	}

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(indio_dev);
		if (ret)
			return ret;
		mutex_lock(&data->lock);
		icm42670_get_data(data, icm42670_to_sensor(chan->type), chan->channel2, val);
		mutex_unlock(&data->lock);
		iio_device_release_direct_mode(indio_dev);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		mutex_lock(&data->lock);
		ret = icm42670_get_scale(data, icm42670_to_sensor(chan->type), val, val2);
		mutex_unlock(&data->lock);
		return ret;
	case IIO_CHAN_INFO_SAMP_FREQ:
		mutex_lock(&data->lock);
		ret = icm42670_get_odr(data, icm42670_to_sensor(chan->type), val);
		mutex_unlock(&data->lock);
		return ret < 0 ? ret : IIO_VAL_INT;
	default:
		return -EINVAL;
	}

	return 0;
}

static int icm42670_write_raw(struct iio_dev *indio_dev,
					struct iio_chan_spec const *chan,
					int val, int val2, long mask)
{
	int ret = 0;
	struct icm42670_data *data = iio_priv(indio_dev);

	mutex_lock(&data->lock);
	ret = iio_device_claim_direct_mode(indio_dev);
	if (ret)
		goto error_write_raw_unlock;

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		ret = icm42670_set_scale(data,
					 icm42670_to_sensor(chan->type),
					 val,
					 val2);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		ret = icm42670_set_odr(data,
					icm42670_to_sensor(chan->type),
					val);
		break;
	default:
		ret = -EINVAL;
	}

error_write_raw_unlock:
	iio_device_release_direct_mode(indio_dev);
	mutex_unlock(&data->lock);
	return ret;
}

static int icm42670_write_raw_get_fmt(struct iio_dev *indio_dev,
					struct iio_chan_spec const *chan,
					long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ANGL_VEL:
			return IIO_VAL_INT_PLUS_NANO;
		default:
			return IIO_VAL_INT_PLUS_MICRO;
		}
	default:
		return IIO_VAL_INT_PLUS_MICRO;
	}

	return -EINVAL;
}

static const char *icm42670_match_acpi_device(struct device *dev)
{
	const struct acpi_device_id *id;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!id)
		return NULL;

	return dev_name(dev);
}

static void icm42670_chip_uninit(struct icm42670_data *data)
{
	icm42670_set_mode(data, ICM42670_GYRO, false);
	icm42670_set_mode(data, ICM42670_ACCEL, false);
}

static void icm42670_disable_vdd_reg(void *_data)
{
	struct icm42670_data *st = _data;
	const struct device *dev = regmap_get_device(st->regmap);
	int ret;

	ret = regulator_disable(st->vdd_supply);
	if (ret)
		dev_err(dev, "failed to disable vdd error %d\n", ret);
}

static int icm42670_temp_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	struct icm42670_data *data = iio_priv(indio_dev);
	int ret = 0;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(indio_dev);
		if (ret)
			return ret;
		ret = icm42670_get_data(data, icm42670_to_sensor(chan->type), chan->channel2, val);
		iio_device_release_direct_mode(indio_dev);
		return ret ? ret : IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = data->enable_fifo ? 500000 : 7812500;
		return data->enable_fifo ? IIO_VAL_INT_PLUS_MICRO : IIO_VAL_INT_PLUS_NANO;

	case IIO_CHAN_INFO_OFFSET:
		*val = data->enable_fifo ? 50 : 3200;
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int __icm42670_set_fsr(struct icm42670_data *data,
							 enum icm42670_sensor_type t, int sel)
{
	int ret;
	unsigned int cmd;

	if (t == ICM42670_ACCEL) {
		ret = regmap_read(data->regmap, REG_ACCEL_CONFIG0, &cmd);
		if (ret < 0)
			return ret;

		if (FIELD_GET(BIT_ACCEL_UI_FS_SEL_MASK, cmd) == sel)
			return 0;

		ret = regmap_update_bits(data->regmap, REG_ACCEL_CONFIG0,
							BIT_ACCEL_UI_FS_SEL_MASK,
							FIELD_PREP(BIT_ACCEL_UI_FS_SEL_MASK, sel));
	} else if (t == ICM42670_GYRO) {
		ret = regmap_read(data->regmap, REG_GYRO_CONFIG0, &cmd);
		if (ret < 0)
			return ret;

		if (FIELD_GET(BIT_GYRO_UI_FS_SEL_MASK, cmd) == sel)
			return 0;

		ret = regmap_update_bits(data->regmap, REG_GYRO_CONFIG0,
							BIT_GYRO_UI_FS_SEL_MASK,
							FIELD_PREP(BIT_GYRO_UI_FS_SEL_MASK, sel));
	} else {
		return -EINVAL;
	}

	return ret;
}

static int icm42670_set_scale(struct icm42670_data *data,
					enum icm42670_sensor_type t,
					int scale, int uscale)
{
	int i;

	if (t >= ARRAY_SIZE(icm42670_scale_table))
		return -EINVAL;

	for (i = 0; i < icm42670_scale_table[t].num; i++)
		if (icm42670_scale_table[t].tbl[i].uscale == uscale &&
			icm42670_scale_table[t].tbl[i].scale == scale)
			break;

	if (i == icm42670_scale_table[t].num)
		return -EINVAL;

	dev_info(regmap_get_device(data->regmap), "set scale t: %d, bit: %x\n",
		 t, icm42670_scale_table[t].tbl[i].bits);
	return __icm42670_set_fsr(data, t, icm42670_scale_table[t].tbl[i].bits);
}

static int __icm42670_set_odr(struct icm42670_data *data,
						enum icm42670_sensor_type t,
						int sel, int divider)
{
	int ret;
	unsigned int cmd;

	if (t == ICM42670_ACCEL) {
		ret = regmap_read(data->regmap, REG_ACCEL_CONFIG0, &cmd);
		if (ret < 0)
			return ret;

		if (FIELD_GET(BIT_ACCEL_ODR_MASK, cmd) == sel)
			return 0;

		cmd &= (~BIT_ACCEL_ODR_MASK);
		cmd |= sel;
		ret = regmap_write(data->regmap, REG_ACCEL_CONFIG0, cmd);
		if (!ret) {
			data->chip_config.accel_odr = sel;
			/* Select the largest of multiple sampling periods */
			data->period_divider =
				(data->period_divider && data->period_divider > divider) ?
			data->period_divider : divider;
		}
	} else if (t == ICM42670_GYRO) {
		ret = regmap_read(data->regmap, REG_GYRO_CONFIG0, &cmd);
		if (ret < 0)
			return ret;

		if (FIELD_GET(BIT_GYRO_ODR_MASK, cmd) == sel)
			return 0;

		cmd &= (~BIT_GYRO_ODR_MASK);
		cmd |= sel;
		ret = regmap_write(data->regmap, REG_GYRO_CONFIG0, cmd);
		if (!ret) {
			data->chip_config.gyro_odr = sel;
			/* Select the largest of multiple sampling periods */
			data->period_divider =
				(data->period_divider && data->period_divider > divider) ?
			data->period_divider : divider;
		}
	} else {
		return -EINVAL;
	}

	return ret;
}

static int icm42670_set_odr(struct icm42670_data *data,
					enum icm42670_sensor_type t,
					int odr)
{
	int i;

	if (t == ICM42670_ACCEL)
		data->accel_frequency = odr;

	if (t == ICM42670_GYRO)
		data->gyro_frequency = odr;

	if (t >= ARRAY_SIZE(icm42670_odr_table))
		return -EINVAL;

	for (i = 0; i < icm42670_odr_table[t].num; i++)
		if (icm42670_odr_table[t].tbl[i].odr == odr)
			break;

	if (i >= icm42670_odr_table[t].num)
		return -EINVAL;

	dev_info(regmap_get_device(data->regmap), "set odr t: %d, bit: %x\n", t,
		 icm42670_odr_table[t].tbl[i].bits);

	return __icm42670_set_odr(data, t, icm42670_odr_table[t].tbl[i].bits,
						icm42670_odr_table[t].tbl[i].divider);
}

static int icm42670_chip_init(struct icm42670_data *data, icm42670_bus_setup bus_setup)
{
	int ret;
	unsigned int regval;
	struct device *dev = regmap_get_device(data->regmap);

	// who am i
	ret = regmap_read(data->regmap, REG_WHO_AM_I, &regval);
	if (ret)
		return ret;

	dev_info(dev, "i am: %x, icm42670: %0x\n", regval, BIT_I_AM_ICM42670);

	if (regval != BIT_I_AM_ICM42670)
		return -EINVAL;

	// reset
	ret = regmap_write(data->regmap, REG_SIGNAL_PATH_RESET, BIT_SOFT_RESET_CHIP_CONFIG);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	/* check reset done bit in interrupt status */
	ret = regmap_read(data->regmap, REG_INT_STATUS, &regval);
	if (ret)
		return ret;

	if (!(regval & BIT_INT_STATUS_RESET_DONE)) {
		dev_err(dev, "reset done status bit missing (%0x)\n", regval);
		return -ENODEV;
	}

	/* set chip bus configuration */
	ret = bus_setup(data);
	if (ret) {
		dev_err(dev, "bus_setup failed!\r\n");
		return ret;
	}

	if (true == data->enable_fifo) {
		/* FIFO count is reported in records
		 * (1 record = 16 bytes for gyro+accel+tempsensor data)
		 */
		ret = regmap_update_bits(data->regmap, REG_INTF_CONFIG0,
					BIT_FIFO_COUNT_REC_FIFO_COUNT_REC,
						BIT_FIFO_COUNT_REC_FIFO_COUNT_REC);
		if (ret)
			return ret;

		/* FIFO data in big-endian (default) */
		ret = regmap_update_bits(data->regmap, REG_INTF_CONFIG0,
					BIT_FIFO_COUNT_REC_FIFO_COUNT_ENDIAN,
					BIT_FIFO_COUNT_REC_FIFO_COUNT_ENDIAN);
		if (ret)
			return ret;

		/* sensor data in big-endian (default) */
		ret = regmap_update_bits(data->regmap, REG_INTF_CONFIG0,
					BIT_FIFO_COUNT_REC_SENSOR_DATA_ENDIAN,
					BIT_FIFO_COUNT_REC_SENSOR_DATA_ENDIAN);
		if (ret)
			return ret;
	}

	ret = icm42670_set_mode(data, ICM42670_ACCEL, true);
	if (ret < 0) {
		dev_err(dev, "icm42670_set_mode ICM42670_ACCEL failed!\r\n");
		return ret;
	}

	ret = icm42670_set_mode(data, ICM42670_GYRO, true);
	if (ret < 0) {
		dev_err(dev, "icm42670_set_mode ICM42670_GYRO failed!\r\n");
		return ret;
	}

	// init gyro and accel para
	ret = icm42670_set_odr(data, ICM42670_ACCEL, icm42670_accel_odr[0].odr);
	if (ret < 0) {
		dev_err(dev, "icm42670_set_odr ICM42670_ACCEL failed!\r\n");
		return ret;
	}

	ret = icm42670_set_odr(data, ICM42670_GYRO, icm42670_gyro_odr[0].odr);
	if (ret < 0) {
		dev_err(dev, "icm42670_set_odr ICM42670_GYRO failed!\r\n");
		return ret;
	}

	ret = icm42670_set_scale(data, ICM42670_ACCEL,
					icm42670_accel_scale[0].scale,
					icm42670_accel_scale[0].uscale);
	if (ret < 0) {
		dev_err(dev, "icm42670_set_scale ICM42670_ACCEL failed!\r\n");
		return ret;
	}

	ret = icm42670_set_scale(data, ICM42670_GYRO,
					icm42670_gyro_scale[0].scale,
					icm42670_gyro_scale[0].uscale);
	if (ret < 0) {
		dev_err(dev, "icm42670_set_scale ICM42670_GYRO failed!\r\n");
		return ret;
	}

	ret = icm42670_set_lpf_bw(data, ICM42670_ACCEL, 0);
	if (ret < 0) {
		dev_err(dev, "icm42670 set accel lpf bandwidth failed!\n");
		return ret;
	}

	ret = icm42670_set_lpf_bw(data, ICM42670_GYRO, 0);
	if (ret < 0) {
		dev_err(dev, "icm42670 set gyro lpf bandwidth failed!\n");
		return ret;
	}

	ret = regmap_write(data->regmap, REG_INT_CONFIG_REG, data->irq_mask);
	if (ret < 0)
		return ret;

	dev_info(dev, "icm42670_init success!\r\n");

	return ret;
}

static irqreturn_t icm42670_do_nothing(int irq, void *private)
{
	return IRQ_HANDLED;
}

int icm42670_core_probe(struct regmap *regmap,
						int irq, const char *name,
						int chip_type, icm42670_bus_setup bus_setup)
{
	struct iio_dev *indio_dev;
	struct icm42670_data *data;
	struct device *dev = regmap_get_device(regmap);
	int ret;
	struct irq_data *desc;
	int irq_type;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	memset(data, 0, sizeof(struct icm42670_data));
	mutex_init(&data->lock);
	data->chip_type = chip_type;
	data->powerup_count = 0;
	data->irq = irq;
	data->regmap = regmap;

	/* get the node */
	data->node = of_find_node_by_name(NULL, "icm42670");
	if (data->node == NULL)
		dev_err(dev, "ic2 node not find!\n");

	data->int1_gpiod = devm_gpiod_get(dev, "int1", GPIOD_IN);
	if (IS_ERR(data->int1_gpiod))
		return dev_err_probe(dev, PTR_ERR(data->int1_gpiod), "Could not get int1 gpio\n");

	/* Get the same result through gpiod_to_irq to ensure that
	 * the affinity can be restored after CPU off/on
	 */
	irq = gpiod_to_irq(data->int1_gpiod);
	if (irq < 0)
		return dev_err_probe(dev, irq, "Could not get IRQ from GPIO\n");

	data->enable_fifo = of_property_read_bool(data->node, "enable-fifo");
	if (true != data->enable_fifo)
		dev_info(dev, "Not using FIFO!\n");

	desc = irq_get_irq_data(irq);
	if (!desc) {
		dev_err(dev, "Could not find IRQ %d\n", irq);
		return -EINVAL;
	}

	irq_type = irqd_get_trigger_type(desc);
	if (!irq_type)
		irq_type = IRQF_TRIGGER_RISING;

	if (irq_type == IRQF_TRIGGER_RISING) {
		data->irq_mask = BIT_ONLY_INT1_ACTIVE_HIGH;
	} else if (irq_type == IRQF_TRIGGER_FALLING) {
		data->irq_mask = BIT_ONLY_INT1_ACTIVE_LOW;
	} else {
		dev_err(dev, "Invalid interrupt type 0x%x specified\n",
			irq_type);
		return -EINVAL;
	}

	if (device_property_read_bool(dev, "drive-open-drain"))
		data->irq_mask |= BIT_ONLY_INT1_OPEN_DRAIN;
	else
		data->irq_mask |= BIT_ONLY_INT1_PUSH_PULL;

	data->vdd_supply = devm_regulator_get(dev, "vcc_3v3_s0");
	if (IS_ERR(data->vdd_supply)) {
		dev_err(dev, "Could not find vdd_avdd!\n");
		return PTR_ERR(data->vdd_supply);
	}

	ret = regulator_enable(data->vdd_supply);
	if (ret) {
		dev_err(dev, "regulator_enable failed!\n");
		return ret;
	}

	ret = devm_add_action_or_reset(dev, icm42670_disable_vdd_reg, data);
	if (ret) {
		dev_err(dev, "devm_add_action_or_reset failed!\n");
		return ret;
	}

	ret = icm42670_chip_init(data, bus_setup);
	if (ret < 0) {
		dev_err(dev, "icm42670_chip_init fail\n");
		return ret;
	}

	dev_set_drvdata(dev, indio_dev);
	if (!name && ACPI_HANDLE(dev))
		name = icm42670_match_acpi_device(dev);

	indio_dev->dev.parent = dev;
	indio_dev->channels =
		data->enable_fifo ? icm42670_fifo_channels : icm42670_channels;
	indio_dev->num_channels =
		data->enable_fifo ?
		ARRAY_SIZE(icm42670_fifo_channels) : ARRAY_SIZE(icm42670_channels);
	indio_dev->available_scan_masks = icm42670_scan_masks;
	indio_dev->name = name;
	indio_dev->info = &icm42670_info;
	indio_dev->modes = INDIO_BUFFER_TRIGGERED;

	if (data->enable_fifo) {
		ret = devm_iio_triggered_buffer_setup(dev, indio_dev,
				iio_pollfunc_store_time, icm42670_read_fifo, NULL);
	} else {
		ret = devm_iio_triggered_buffer_setup(dev, indio_dev,
				icm42670_do_nothing, NULL, NULL);
	}
	if (ret < 0)
		goto uninit;

	ret = icm42670_probe_trigger(indio_dev, irq_type);
	if (ret) {
		dev_err(dev, "trigger probe fail %d\n", ret);
		goto uninit;
	}

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret < 0) {
		dev_err(dev, "devm_iio_device_register fail\n");
		goto uninit;
	}

	ret = pm_runtime_set_active(dev);
	if (ret)
		return ret;

	return 0;

uninit:
	icm42670_chip_uninit(data);
	return ret;
}
EXPORT_SYMBOL_GPL(icm42670_core_probe);

/*
 * System resume gets the system back on and restores the sensors state.
 * Manually put runtime power management in system active state.
 */
static int __maybe_unused sleep_icm42670_resume(struct device *dev)
{
	int ret = 0;
	struct icm42670_data *st = iio_priv(dev_get_drvdata(dev));
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	usleep_range(3000, 4000);

	icm42670_set_odr(st, ICM42670_GYRO, st->gyro_frequency_buff);
	icm42670_set_odr(st, ICM42670_ACCEL, st->accel_frequency_buff);
	icm42670_set_lpf_bw(st, ICM42670_ACCEL, st->accel_lpf_bw_buff);
	icm42670_set_lpf_bw(st, ICM42670_GYRO, st->gyro_lpf_bw_buff);

	ret = icm42670_set_enable(indio_dev, 1);

	return ret;
}

/*
 * Suspend saves sensors state and turns everything off.
 * Check first if runtime suspend has not already done the job.
 */
static int __maybe_unused sleep_icm42670_suspend(struct device *dev)
{
	struct icm42670_data *st = iio_priv(dev_get_drvdata(dev));
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int ret = 0;

	st->gyro_frequency_buff = st->gyro_frequency;
	st->accel_frequency_buff = st->accel_frequency;
	st->accel_lpf_bw_buff = st->accel_lpf_bw;
	st->gyro_lpf_bw_buff = st->gyro_lpf_bw;

	ret = icm42670_set_enable(indio_dev, 0);

	return ret;
}

const struct dev_pm_ops icm42670_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sleep_icm42670_suspend, sleep_icm42670_resume)
};
EXPORT_SYMBOL_GPL(icm42670_pm_ops);

void icm42670_core_remove(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct icm42670_data *data = iio_priv(indio_dev);

	icm42670_chip_uninit(data);
}
EXPORT_SYMBOL_GPL(icm42670_core_remove);

MODULE_AUTHOR("Hangyu Li <hangyu.li@rock-chips.com>");
MODULE_DESCRIPTION("ICM42670 CORE driver");
MODULE_LICENSE("GPL");
