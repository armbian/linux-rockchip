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
#include <linux/pm_runtime.h>
#include "icm42607.h"

static struct imu_reg_value_map icm_acce_only_regs_cfg_map[] = {
	{REG_PWR_MGMT_0, 0x03},    /* Accelerometer only mode */
	{REG_ACCEL_CONFIG0, 0x9},/* 100Hz, 16g range */
};

static struct imu_reg_value_map icm_acce_gyro_regs_cfg_map[] = {
	{REG_PWR_MGMT_0, 0x0f},    /* Both accelerometer and gyroscope on */
	{REG_ACCEL_CONFIG0, 0x9},/* 100Hz, 16g range */
	{REG_GYRO_CONFIG0, 0x9}, /* 100Hz, 2000dps range */
};

static struct imu_reg_value_map icm_powerdown_regs_cfg_map[] = {
	{REG_PWR_MGMT_0, 0x00},
};

static int icm42607_regs_cfg_write(struct imu_ctrb *ctrb,
		struct imu_reg_value_map *maparrays, int regcnt)
{
	int i, ret;

	if (ctrb == NULL)
		return -1;

	if (regcnt <= 0) {
		dev_err(ctrb->dev, "reg map len err\n");
		return -1;
	}
	for (i = 0; i < regcnt; i++) {
		ret = regmap_write(ctrb->regmap, maparrays[i].reg, maparrays[i].value);
		if (ret) {
			dev_err(ctrb->dev, "regmap write err\n");
			return -1;
		}
	}
	return 0;
}

static int icm42607_read_id(void *ctrbp)
{
	int ret;
	unsigned int val = 0;
	struct imu_ctrb *ctrb = (struct imu_ctrb *)ctrbp;

	if (ctrb == NULL)
		return -1;

	ret = regmap_read(ctrb->regmap, REG_WHO_AM_I, &val);
	if (ret) {
		dev_err(ctrb->dev, "regmap_read err\n");
		return 0;
	}
	dev_info(ctrb->dev, "ID = 0x%02x\n", val);
	return (int)val;
}

static int icm42607_mode_set(void *ctrbp, int mode)
{
	int ret = 0;
	struct imu_ctrb *ctrb = (struct imu_ctrb *)ctrbp;

	if (ctrb == NULL)
		return -1;

	switch (mode) {
	case IMU_POWER_MODE_INIT:
		break;
	case IMU_POWER_MODE_DOWN:
		ret = icm42607_regs_cfg_write(ctrb, icm_powerdown_regs_cfg_map,
			sizeof(icm_powerdown_regs_cfg_map) / sizeof(struct imu_reg_value_map));
		break;
	case IMU_POWER_ACCE_ONLY:
		ret = icm42607_regs_cfg_write(ctrb, icm_acce_only_regs_cfg_map,
			sizeof(icm_acce_only_regs_cfg_map) / sizeof(struct imu_reg_value_map));
		break;
	case IMU_POWER_ACCE_GYRO:
		ret = icm42607_regs_cfg_write(ctrb, icm_acce_gyro_regs_cfg_map,
			sizeof(icm_acce_gyro_regs_cfg_map) / sizeof(struct imu_reg_value_map));
		break;
	default:
		ret = -1;
	}
	dev_info(ctrb->dev, "set power:%d success\n", mode);
	return ret;
}

static int icm42607_mreg_write(struct imu_ctrb *ctrb, u16 addr, u8 data)
{
	int ret;
	u8 bank = addr >> 8;
	u8 reg  = addr & 0xFF;

	ret  = regmap_write(ctrb->regmap, REG_BLK_SEL_W, bank);
	usleep_range(10, 11);
	ret |= regmap_write(ctrb->regmap, REG_MADDR_W, reg);
	usleep_range(10, 11);
	ret |= regmap_write(ctrb->regmap, REG_M_W, data);
	usleep_range(10, 11);
	ret |= regmap_write(ctrb->regmap, REG_BLK_SEL_W, 0);

	return ret;
}

static int icm42607_mreg_read(struct imu_ctrb *ctrb, u16 addr, u8 *data)
{
	int ret;
	u8 bank = addr >> 8;
	u8 reg  = addr & 0xFF;
	unsigned int tmp;

	ret  = regmap_write(ctrb->regmap, REG_BLK_SEL_R, bank);
	usleep_range(10, 11);
	ret |= regmap_write(ctrb->regmap, REG_MADDR_R, reg);
	usleep_range(10, 11);
	ret |= regmap_read(ctrb->regmap, REG_M_R, &tmp);
	usleep_range(10, 11);
	ret |= regmap_write(ctrb->regmap, REG_BLK_SEL_R, 0);

	if (!ret)
		*data = (u8)tmp;

	return ret;
}

static int icm42607_otp_reload(struct imu_ctrb *ctrb)
{
	int ret;
	u8 rb;

	ret = regmap_write(ctrb->regmap, REG_PWR_MGMT_0, BIT_IDLE);
	if (ret)
		return ret;
	usleep_range(20, 21);

	/* OTP_COPY_MODE = 2'b01 */
	ret = icm42607_mreg_read(ctrb, REG_OTP_CONFIG_MREG_TOP1, &rb);
	if (ret)
		return ret;
	rb &= ~OTP_COPY_MODE_MASK;
	rb |= BIT_OTP_COPY_NORMAL;
	ret = icm42607_mreg_write(ctrb, REG_OTP_CONFIG_MREG_TOP1, rb);
	if (ret)
		return ret;

	/* OTP_PWR_DOWN = 0 */
	ret = icm42607_mreg_read(ctrb, REG_OTP_CTRL7_MREG_OTP, &rb);
	if (ret)
		return ret;
	rb &= ~BIT_OTP_PWR_DOWN;
	ret = icm42607_mreg_write(ctrb, REG_OTP_CTRL7_MREG_OTP, rb);
	if (ret)
		return ret;
	usleep_range(300, 400);

	/* OTP_RELOAD = 1 */
	ret = icm42607_mreg_read(ctrb, REG_OTP_CTRL7_MREG_OTP, &rb);
	if (ret)
		return ret;
	rb |= BIT_OTP_RELOAD;
	ret = icm42607_mreg_write(ctrb, REG_OTP_CTRL7_MREG_OTP, rb);
	if (ret)
		return ret;
	usleep_range(280, 380);

	return 0;
}

static int icm42607_set_default_register(struct imu_ctrb *ctrb)
{
	int s = 0;

	s |= regmap_write(ctrb->regmap, REG_GYRO_CONFIG0, 0x69);
	s |= regmap_write(ctrb->regmap, REG_ACCEL_CONFIG0, 0x69);
	s |= regmap_write(ctrb->regmap, REG_APEX_CONFIG0, 0x08);
	s |= regmap_write(ctrb->regmap, REG_APEX_CONFIG1, 0x02);
	s |= regmap_write(ctrb->regmap, REG_WOM_CONFIG, 0x00);
	s |= regmap_write(ctrb->regmap, REG_FIFO_CONFIG1, 0x01);
	s |= regmap_write(ctrb->regmap, REG_FIFO_CONFIG2, 0x00);
	s |= regmap_write(ctrb->regmap, REG_FIFO_CONFIG3, 0x00);

	s |= icm42607_mreg_write(ctrb, REG_FIFO_CONFIG5_MREG_TOP1, 0x20);
	s |= icm42607_mreg_write(ctrb, REG_ST_CONFIG_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_INT_SOURCE7_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_INT_SOURCE8_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_INT_SOURCE9_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_INT_SOURCE10_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_APEX_CONFIG2_MREG_TOP1, 0xA2);
	s |= icm42607_mreg_write(ctrb, REG_APEX_CONFIG3_MREG_TOP1, 0x85);
	s |= icm42607_mreg_write(ctrb, REG_APEX_CONFIG4_MREG_TOP1, 0x51);
	s |= icm42607_mreg_write(ctrb, REG_APEX_CONFIG5_MREG_TOP1, 0x80);
	s |= icm42607_mreg_write(ctrb, REG_APEX_CONFIG9_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_APEX_CONFIG10_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_APEX_CONFIG11_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_ACCEL_WOM_X_THR_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_ACCEL_WOM_Y_THR_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_ACCEL_WOM_Z_THR_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER0_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER1_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER2_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER3_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER4_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER5_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER6_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER7_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_GOS_USER8_MREG_TOP1, 0x00);
	s |= icm42607_mreg_write(ctrb, REG_APEX_CONFIG12_MREG_TOP1, 0x00);

	return s ? -EIO : 0;
}

static int icm42607_chip_init(void *ctrbp)
{
	struct imu_ctrb *ctrb = (struct imu_ctrb *)ctrbp;
	int ret;
	unsigned int val;

	if (!ctrb)
		return -EINVAL;

	ret = icm42607_otp_reload(ctrb);
	if (ret) {
		dev_err(ctrb->dev, "OTP reload fail(%d)\n", ret);
		return ret;
	}

	ret = icm42607_set_default_register(ctrb);
	if (ret) {
		dev_err(ctrb->dev, "set default reg fail(%d)\n", ret);
		return ret;
	}

	val = BIT_SENSOR_DATA_ENDIAN | BIT_FIFO_COUNT_ENDIAN;
	ret = regmap_write(ctrb->regmap, REG_INTF_CONFIG0, val);
	if (ret)
		return ret;

	val = BIT_CLK_SEL_PLL | BIT_I3C_SDR_EN | BIT_I3C_DDR_EN;
	ret = regmap_write(ctrb->regmap, REG_INTF_CONFIG1, val);
	if (ret)
		return ret;

	return icm42607_mode_set(ctrb, IMU_POWER_MODE_DOWN);
}

static int icm42607_read_acce_gyro_raw(void *ctrbp, void *rawdatap, uint8_t reg)
{
	int ret = 0;
	uint8_t rawarrays[6] = { 0 };
	struct imu_ctrb *ctrb = (struct imu_ctrb *)ctrbp;
	struct imu_3axis_data *rawdata = (struct imu_3axis_data *)rawdatap;

	if (ctrb == NULL || rawdata == NULL)
		return -1;

	ret = regmap_bulk_read(ctrb->regmap, reg, rawarrays, sizeof(rawarrays));
	if (!ret) {
		rawdata->raw[0] = ((uint16_t)rawarrays[0]) << 8 | rawarrays[1];
		rawdata->raw[1] = ((uint16_t)rawarrays[2]) << 8 | rawarrays[3];
		rawdata->raw[2] = ((uint16_t)rawarrays[4]) << 8 | rawarrays[5];
	} else {
		dev_err(ctrb->dev, "regmap_bulk_read err:%d\n", ret);
	}
	return ret;
}

static int icm42607_read_acce_raw(void *ctrbp, void *rawdatap)
{
	return icm42607_read_acce_gyro_raw(ctrbp, rawdatap, REG_ACCEL_DATA_X0_UI);
}

static int icm42607_read_gyro_raw(void *ctrbp, void *rawdatap)
{
	return icm42607_read_acce_gyro_raw(ctrbp, rawdatap, REG_GYRO_DATA_X0_UI);
}

static int icm42607_read_asix_one(void *ctrbp, int addr, int *datap)
{
	int ret;
	struct imu_3axis_data accedata = {0}, gyrodata = {0};

	ret = icm42607_read_acce_raw(ctrbp, &accedata);
	ret = ret || icm42607_read_gyro_raw(ctrbp, &gyrodata);
	switch (addr) {
	case 0:
		(*datap) = accedata.raw[0];
		break;
	case 1:
		(*datap) = accedata.raw[1];
		break;
	case 2:
		(*datap) = accedata.raw[2];
		break;
	case 3:
		(*datap) = gyrodata.raw[0];
		break;
	case 4:
		(*datap) = gyrodata.raw[1];
		break;
	case 5:
		(*datap) = gyrodata.raw[2];
		break;
	}
	return ret;
}

static int icm42607_set_accel_offset(void *ctrbp, int offset, int axis)
{
	struct imu_ctrb *ctrb = (struct imu_ctrb *)ctrbp;
	int ret = 0;
	u8 reg_l, reg_h;
	u16 val;

	offset = clamp(offset, -2048, 2047);
	val = (u16)(offset & 0x0FFF);

	switch (axis) {
	case 0: /* X */
		reg_l = REG_GOS_USER5_MREG_TOP1;
		reg_h = REG_GOS_USER4_MREG_TOP1;
		break;
	case 1: /* Y */
		reg_l = REG_GOS_USER6_MREG_TOP1;
		reg_h = REG_GOS_USER7_MREG_TOP1;
		break;
	case 2: /* Z */
		reg_l = REG_GOS_USER8_MREG_TOP1;
		reg_h = REG_GOS_USER7_MREG_TOP1;
		break;
	default:
		return -EINVAL;
	}

	ret |= icm42607_mreg_write(ctrb, reg_l, val & 0xFF);
	ret |= icm42607_mreg_write(ctrb, reg_h, (val >> 8) & 0x0F);

	return ret ? -EIO : 0;
}

static int icm42607_set_gyro_offset(void *ctrbp, int offset, int axis)
{
	struct imu_ctrb *ctrb = (struct imu_ctrb *)ctrbp;
	int ret = 0;
	u8 reg_l, reg_h;
	u16 val;

	offset = clamp(offset, -2048, 2047);
	val = (u16)(offset & 0x0FFF);

	switch (axis) {
	case 0: /* X */
		reg_l = REG_GOS_USER0_MREG_TOP1;
		reg_h = REG_GOS_USER1_MREG_TOP1;
		break;
	case 1: /* Y */
		reg_l = REG_GOS_USER2_MREG_TOP1;
		reg_h = REG_GOS_USER1_MREG_TOP1;
		break;
	case 2: /* Z */
		reg_l = REG_GOS_USER3_MREG_TOP1;
		reg_h = REG_GOS_USER4_MREG_TOP1;
		break;
	default:
		return -EINVAL;
	}

	ret |= icm42607_mreg_write(ctrb, reg_l, val & 0xFF);
	ret |= icm42607_mreg_write(ctrb, reg_h, (val >> 8) & 0x0F);

	return ret ? -EIO : 0;
}

static const struct imu_info icm42607_info = {
	.name = "icm42607",
	.id = ICM42607_CHIP_ID,
	.read_id = icm42607_read_id,
	.chip_init = icm42607_chip_init,
	.mode_set = icm42607_mode_set,
	.read_acce_raw = icm42607_read_acce_raw,
	.read_gyro_raw = icm42607_read_gyro_raw,
	.read_asix_one = icm42607_read_asix_one,
	.set_accel_offset = icm42607_set_accel_offset,
	.set_gyro_offset = icm42607_set_gyro_offset,
};

struct imu_info *icm42607_chip_probe(struct imu_ctrb *ctrb)
{
	int chip_id = -1;

	chip_id = icm42607_read_id(ctrb);
	if (chip_id == icm42607_info.id) {
		dev_info(ctrb->dev, "probe sensor: %s, id = 0x%02X\n",
			icm42607_info.name, icm42607_info.id);
		return (struct imu_info *)&icm42607_info;
	}
	return NULL;
}
