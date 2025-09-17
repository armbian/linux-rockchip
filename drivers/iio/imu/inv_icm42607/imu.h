/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __IMU_H__
#define __IMU_H__

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/of_platform.h>

enum imu_power_mode {
	IMU_POWER_MODE_INIT = 0,
	IMU_POWER_MODE_DOWN = 1,
	IMU_POWER_ACCE_ONLY = 2,
	IMU_POWER_ACCE_GYRO = 3,
};

enum imu_sensor_id {
	IMU_SENSOR_ID_ACCE = 0,
	IMU_SENSOR_ID_GYRO,
	IMU_SENSOR_ID_MAX
};

enum imu_position_id {
	IMU_P_D_0   = 0,
	IMU_P_D_90  = 1,
	IMU_P_D_180 = 2,
	IMU_P_D_270 = 3,
	IMU_D_MAX
};

struct imu_3axis_data {
	int16_t raw[3];
	s64 ts;
};

struct imu_info {
	char name[30];
	int id;
	int (*read_id)(void *ctrbp);
	int (*mode_set)(void *ctrbp, int mode);
	int (*chip_init)(void *ctrbp);
	int (*read_acce_raw)(void *ctrbp, void *rawdata);
	int (*read_gyro_raw)(void *ctrbp, void *rawdata);
	int (*read_asix_one)(void *ctrbp, int addr, int *datap);
	int (*set_accel_offset)(void *ctrbp, int offset, int axis);
	int (*set_gyro_offset)(void *ctrbp, int offset, int axis);
};

struct imu_ctrb {
	struct device *dev;
	struct regmap *regmap;
	struct iio_dev *iio_devs[IMU_SENSOR_ID_MAX];
	struct mutex power_lock;
	struct delayed_work pollingwork;
	struct imu_info *chipinfo;
	int irq;
	int mode;
	int debugon;
	int position;
	bool irq_enable;
};

struct imu_sensor {
	char name[32];
	enum imu_sensor_id id;
	struct imu_ctrb *ctrb;
	unsigned int odr;
	int calibrated;
	int32_t offset_x;
	int32_t offset_y;
	int32_t offset_z;
	uint32_t readcnt;
	struct imu_3axis_data rawdata;
};

struct imu_reg_value_map {
	uint8_t reg;
	uint8_t value;
};

#define IMU_POLLING_TIME_MS (10)
#endif
