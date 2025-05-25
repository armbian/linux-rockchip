/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef ICM42670_H_
#define ICM42670_H_

#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/mutex.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/regmap.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

/* Registers and associated bit definitions */
/* Bank 0 */
#define REG_MCLK_RDY				0x00
#define REG_CHIP_CONFIG_REG			0x01
#define REG_SIGNAL_PATH_RESET			0x02
#define REG_DRIVE_CONFIG_REG1			0x03
#define REG_DRIVE_CONFIG_REG2			0x04
#define REG_DRIVE_CONFIG_REG3			0x05
#define REG_INT_CONFIG_REG			0x06
#define REG_TEMP_DATA0_UI			0x09
#define REG_TEMP_DATA1_UI			0x0a
#define REG_ACCEL_DATA_X0_UI			0x0b
#define REG_ACCEL_DATA_X1_UI			0x0c
#define REG_ACCEL_DATA_Y0_UI			0x0d
#define REG_ACCEL_DATA_Y1_UI			0x0e
#define REG_ACCEL_DATA_Z0_UI			0x0f
#define REG_ACCEL_DATA_Z1_UI			0x10
#define REG_GYRO_DATA_X0_UI			0x11
#define REG_GYRO_DATA_X1_UI			0x12
#define REG_GYRO_DATA_Y0_UI			0x13
#define REG_GYRO_DATA_Y1_UI			0x14
#define REG_GYRO_DATA_Z0_UI			0x15
#define REG_GYRO_DATA_Z1_UI			0x16
#define REG_TMST_FSYNC1				0x17
#define REG_TMST_FSYNC2				0x18
#define REG_APEX_DATA4				0x1d
#define REG_APEX_DATA5				0x1e
#define REG_PWR_MGMT_0				0x1f
#define REG_GYRO_CONFIG0			0x20
#define REG_ACCEL_CONFIG0			0x21
#define REG_TEMP_CONFIG0			0x22
#define REG_GYRO_CONFIG1			0x23
#define REG_ACCEL_CONFIG1			0x24
#define REG_APEX_CONFIG0			0x25
#define REG_APEX_CONFIG1			0x26
#define REG_WOM_CONFIG				0x27
#define REG_FIFO_CONFIG1			0x28
#define REG_FIFO_CONFIG2			0x29
#define REG_FIFO_CONFIG3			0x2a
#define REG_INT_SOURCE0				0x2b
#define REG_INT_SOURCE1				0x2c
#define REG_INT_SOURCE3				0x2d
#define REG_INT_SOURCE4				0x2e
#define REG_FIFO_LOST_PKT0			0x2f
#define REG_FIFO_LOST_PKT1			0x30
#define REG_APEX_DATA0				0x31
#define REG_APEX_DATA1				0x32
#define REG_APEX_DATA2				0x33
#define REG_APEX_DATA3				0x34
#define REG_INTF_CONFIG0			0x35
#define REG_INTF_CONFIG1			0x36
#define REG_INT_STATUS_DRDY			0x39
#define REG_INT_STATUS				0x3a
#define REG_INT_STATUS2				0x3b
#define REG_INT_STATUS3				0x3c
#define REG_FIFO_BYTE_COUNT1		0x3d
#define REG_FIFO_BYTE_COUNT2		0x3e
#define REG_FIFO_DATA_REG			0x3f
#define REG_WHO_AM_I				0x75
#define REG_BLK_SEL_W				0x79
#define REG_MADDR_W				0x7a
#define REG_M_W					0x7b
#define REG_BLK_SEL_R				0x7c
#define REG_MADDR_R				0x7d
#define REG_M_R					0x7e

/* MREG_TOP1 */
#define REG_TMST_CONFIG1_MREG_TOP1		0x00
#define REG_FIFO_CONFIG5_MREG_TOP1		0x01
#define REG_FIFO_CONFIG6_MREG_TOP1		0x02
#define REG_FSYNC_CONFIG_MREG_TOP1		0x03
#define REG_INT_CONFIG0_MREG_TOP1		0x04
#define REG_INT_CONFIG1_MREG_TOP1		0x05
#define REG_ST_CONFIG_MREG_TOP1			0x13
#define REG_SELFTEST_MREG_TOP1			0x14
#define REG_INTF_CONFIG6_MREG_TOP1		0x23
#define REG_INTF_CONFIG10_MREG_TOP1		0x25
#define REG_INTF_CONFIG7_MREG_TOP1		0x28
#define REG_OTP_CONFIG_MREG_TOP1		0x2b
#define REG_INT_SOURCE6_MREG_TOP1		0x2f
#define REG_INT_SOURCE7_MREG_TOP1		0x30
#define REG_INT_SOURCE8_MREG_TOP1		0x31
#define REG_INT_SOURCE9_MREG_TOP1		0x32
#define REG_APEX_CONFIG2_MREG_TOP1		0x44
#define REG_APEX_CONFIG3_MREG_TOP1		0x45
#define REG_APEX_CONFIG4_MREG_TOP1		0x46
#define REG_APEX_CONFIG5_MREG_TOP1		0x47
#define REG_APEX_CONFIG9_MREG_TOP1		0x48
#define REG_APEX_CONFIG10_MREG_TOP1		0x49
#define REG_APEX_CONFIG11_MREG_TOP1		0x4a
#define REG_ACCEL_WOM_X_THR_MREG_TOP1		0x4b
#define REG_ACCEL_WOM_Y_THR_MREG_TOP1		0x4c
#define REG_ACCEL_WOM_Z_THR_MREG_TOP1		0x4d
#define REG_GOS_USER0_MREG_TOP1			0x4e
#define REG_GOS_USER1_MREG_TOP1			0x4f
#define REG_GOS_USER2_MREG_TOP1			0x50
#define REG_GOS_USER3_MREG_TOP1			0x51
#define REG_GOS_USER4_MREG_TOP1			0x52
#define REG_GOS_USER5_MREG_TOP1			0x53
#define REG_GOS_USER6_MREG_TOP1			0x54
#define REG_GOS_USER7_MREG_TOP1			0x55
#define REG_GOS_USER8_MREG_TOP1			0x56
#define REG_ST_STATUS1_MREG_TOP1		0x63
#define REG_ST_STATUS2_MREG_TOP1		0x64
#define REG_FDR_CONFIG_MREG_TOP1		0x66
#define REG_APEX_CONFIG12_MREG_TOP1		0x67

/* MREG_TOP2 */
#define REG_OTP_CTRL7_MREG_OTP_TOP2			0x2806

/* MREG_TOP3 */
#define REG_XA_ST_DATA_MMEM_TOP3			0x5000
#define REG_YA_ST_DATA_MMEM_TOP3			0x5001
#define REG_ZA_ST_DATA_MMEM_TOP3			0x5002
#define REG_XG_ST_DATA_MMEM_TOP3			0x5003
#define REG_YG_ST_DATA_MMEM_TOP3			0x5004
#define REG_ZG_ST_DATA_MMEM_TOP3			0x5005

/* Bank0 REG_GYRO_CONFIG0/REG_ACCEL_CONFIG0 */
#define BIT_SENSOR_ODR_1600HZ		0x05	//(LN mode)
#define BIT_SENSOR_ODR_800HZ		0x06	//(LN mode)
#define BIT_SENSOR_ODR_400HZ		0x07	//(LP or LN mode)
#define BIT_SENSOR_ODR_200HZ		0x08	//(LP or LN mode)
#define BIT_SENSOR_ODR_100HZ		0x09	//(LP or LN mode)
#define BIT_SENSOR_ODR_50HZ			0x0A	//(LP or LN mode)
#define BIT_SENSOR_ODR_25HZ			0x0B	//(LP or LN mode)
#define BIT_SENSOR_ODR_12HZ			0x0C	//(LP or LN mode)
#define BIT_SENSOR_ODR_6HZ			0x0D	// only accel (LP mode)
#define BIT_SENSOR_ODR_3HZ			0x0E	// only accel (LP mode)
#define BIT_SENSOR_ODR_1HZ			0x0F	// only accel (LP mode)

#define BIT_ACCEL_FSR_16G		0x00
#define BIT_ACCEL_FSR_8G		0x01
#define BIT_ACCEL_FSR_4G		0x02
#define BIT_ACCEL_FSR_2G		0x03

#define BIT_GYRO_FSR_2000DPS	0X00
#define BIT_GYRO_FSR_1000DPS	0X01
#define BIT_GYRO_FSR_500DPS		0x02
#define BIT_GYRO_FSR_250DPS		0x03

#define BIT_GYRO_UI_FS_SEL_MASK		GENMASK(6, 5)
#define BIT_GYRO_ODR_MASK			GENMASK(3, 0)
#define BIT_ACCEL_UI_FS_SEL_MASK	GENMASK(6, 5)
#define BIT_ACCEL_ODR_MASK			GENMASK(3, 0)

/*
 * GYRO_CONFIG1
 * Register Name : GYRO_CONFIG1
 */

/*
 * gyro_ui_filt_bw
 * Selects GYRO UI low pass filter bandwidth
 *
 * 000: Low pass filter bypassed
 * 001: 180 Hz
 * 010: 121 Hz
 * 011: 73 Hz
 * 100: 53 Hz
 * 101: 34 Hz
 * 110: 25 Hz
 * 111: 16 Hz
 *
 * This field can be changed on-the-fly even if gyro sensor is on
 */
#define GYRO_CONFIG1_GYRO_UI_FILT_BW_MASK	0x07

/* Bank0 REG_GYRO_CONFIG1 */
#define BIT_GYR_UI_FLT_BW_BYPASS		0x00
#define BIT_GYR_UI_FLT_BW_180HZ			0x01
#define BIT_GYR_UI_FLT_BW_121HZ			0x02
#define BIT_GYR_UI_FLT_BW_73HZ			0x03
#define BIT_GYR_UI_FLT_BW_53HZ			0x04
#define BIT_GYR_UI_FLT_BW_34HZ			0x05
#define BIT_GYR_UI_FLT_BW_25HZ			0x06
#define BIT_GYR_UI_FLT_BW_16HZ			0x07
#define BIT_GYR_UI_AVG_IND_2X			0x00
#define BIT_GYR_UI_AVG_IND_4X			0x10
#define BIT_GYR_UI_AVG_IND_8X			0x20
#define BIT_GYR_UI_AVG_IND_16X			0x30
#define BIT_GYR_UI_AVG_IND_32X			0x40
#define BIT_GYR_UI_AVG_IND_64X			0x50

/*
 * accel_ui_filt_bw
 * Selects ACCEL UI low pass filter bandwidth
 *
 * 000: Low pass filter bypassed
 * 001: 180 Hz
 * 010: 121 Hz
 * 011: 73 Hz
 * 100: 53 Hz
 * 101: 34 Hz
 * 110: 25 Hz
 * 111: 16 Hz
 *
 * This field can be changed on-the-fly even if accel sensor is on
 */
#define ACCEL_CONFIG1_ACCEL_UI_FILT_BW_MASK	0x07

/* Bank0 REG_ACCEL_CONFIG1 */
#define BIT_ACC_FILT_BW_IND_BYPASS		0x00
#define BIT_ACC_FILT_BW_IND_180HZ		0x01
#define BIT_ACC_FILT_BW_IND_121HZ		0x02
#define BIT_ACC_FILT_BW_IND_73HZ		0x03
#define BIT_ACC_FILT_BW_IND_53HZ		0x04
#define BIT_ACC_FILT_BW_IND_34HZ		0x05
#define BIT_ACC_FILT_BW_IND_25HZ		0x06
#define BIT_ACC_FILT_BW_IND_16HZ		0x07
#define BIT_ACC_UI_AVG_IND_2X			0x00
#define BIT_ACC_UI_AVG_IND_4X			0x10
#define BIT_ACC_UI_AVG_IND_8X			0x20
#define BIT_ACC_UI_AVG_IND_16X			0x30
#define BIT_ACC_UI_AVG_IND_32X			0x40
#define BIT_ACC_UI_AVG_IND_64X			0x50

/* Bank0 REG_INT_CONFIG_REG */
#define SHIFT_INT1_MODE				0x02
#define SHIFT_INT1_DRIVE_CIRCUIT		0x01
#define SHIFT_INT1_POLARITY			0x00
#define BIT_ONLY_INT1_ACTIVE_HIGH   \
		((1 << SHIFT_INT1_POLARITY) | \
		(0 << SHIFT_INT1_MODE))
#define BIT_ONLY_INT1_ACTIVE_LOW	\
		((0 << SHIFT_INT1_POLARITY) | \
		(0 << SHIFT_INT1_MODE))
#define BIT_ONLY_INT1_OPEN_DRAIN (0 << SHIFT_INT1_DRIVE_CIRCUIT)
#define BIT_ONLY_INT1_PUSH_PULL  (1 << SHIFT_INT1_DRIVE_CIRCUIT)

/* Bank0 REG_PWR_MGMT_0 */
#define BIT_PWR_MGMTO_ACCEL_LP_CLK_SEL  BIT(7)
#define BIT_PWR_MGMT0_IDLE			BIT(4)
#define BIT_PWR_MGMT0_GYRO(_mode)		\
		FIELD_PREP(GENMASK(3, 2), (_mode))
#define BIT_PWR_MGMT0_ACCEL(_mode)		\
		FIELD_PREP(GENMASK(1, 0), (_mode))

#define BIT_ACCEL_MODE_OFF			0x00
#define BIT_ACCEL_MODE_LPM			0x02
#define BIT_ACCEL_MODE_LNM			0x03
#define BIT_ACCEL_MODE_MASK			0x03

#define BIT_GYRO_MODE_OFF			0x00
#define BIT_GYRO_MODE_STBY			0x04
#define BIT_GYRO_MODE_LPM			0x08
#define BIT_GYRO_MODE_LNM			0x0c
#define BIT_GYRO_MODE_MASK			0x0c

#define BIT_IDLE				0x10
#define BIT_ACCEL_LP_CLK_SEL			0x80

/* Bank0 REG_SIGNAL_PATH_RESET */
#define BIT_FIFO_FLUSH					BIT(2)
#define BIT_SOFT_RESET_CHIP_CONFIG		BIT(4)

/* Bank0 REG_INTF_CONFIG0 */
#define BIT_FIFO_COUNT_REC_FIFO_COUNT_REC	BIT(6)
#define BIT_FIFO_COUNT_REC_FIFO_COUNT_ENDIAN	BIT(5)
#define BIT_FIFO_COUNT_REC_SENSOR_DATA_ENDIAN	BIT(4)
#define BIT_FIFO_COUNT_REC_UI_SIFS_CFG_MASK		GENMASK(1, 0)
#define BIT_FIFO_COUNT_REC_UI_SIFS_CFG_SPI_DIS	\
		FIELD_PREP(BIT_FIFO_COUNT_REC_UI_SIFS_CFG_MASK, 2)
#define BIT_FIFO_COUNT_REC_UI_SIFS_CFG_I2C_DIS	\
		FIELD_PREP(BIT_FIFO_COUNT_REC_UI_SIFS_CFG_MASK, 3)

/* Bank0 REG_INTF_CONFIG1 */
#define BIT_CLK_SEL_RC				0x00
#define BIT_CLK_SEL_PLL				0x01
#define BIT_CLK_SEL_DIS				0x03
#define BIT_I3C_DDR_EN				0x04
#define BIT_I3C_SDR_EN				0x08
#define BIT_GYRO_AFSR_MODE_LFS			0x00
#define BIT_GYRO_AFSR_MODE_HFS			0x20
#define BIT_GYRO_AFSR_MODE_DYN			0x40

/* Bank0 REG_FIFO_CONFIG1 */
#define BIT_FIFO_MODE_NO_BYPASS			0x00
#define BIT_FIFO_MODE_BYPASS			0x01
#define BIT_FIFO_MODE_STREAM			0x00
#define BIT_FIFO_MODE_STOPFULL			0x02

/* Bank0 REG_FIFO_CONFIG2 / REG_FIFO_CONFIG3 */
#define BIT_FIFO_WM5 0x10
#define INT_FIFO_WM5_NUM 16

/* Bank 0 REG_INT_SOURCE0 */
#define BIT_INT_MODE_OFF				0x00
#define BIT_INT_AGC_RDY_INT1_EN			0x01
#define BIT_INT_FIFO_FULL_INT1_EN		0x02
#define BIT_INT_FIFO_THS_INT1_EN		0x04
#define BIT_INT_DRDY_INT_EN			0x08
#define BIT_INT_RESET_DONE_INT1_EN		0x10
#define BIT_INT_PLL_RDY_INT1_EN			0x20
#define BIT_INT_FSYNC_INT1_EN			0x40
#define BIT_INT_ST_DONE_INT1_EN			0x80

/* Bank 0 REG_INT_SOURCE1 */
#define BIT_INT_WOM_X_INT1_EN			0x01
#define BIT_INT_WOM_Y_INT1_EN			0x02
#define BIT_INT_WOM_Z_INT1_EN			0x04
#define BIT_INT_WOM_XYZ_INT1_EN (BIT_INT_WOM_X_INT1_EN | \
		BIT_INT_WOM_Y_INT1_EN | BIT_INT_WOM_Z_INT1_EN)
#define BIT_INT_SMD_INT1_EN			0x08
#define BIT_INT_I3C_PROTCL_ERR_INT1_EN		0x40

/* Bank0 REG_INT_STATUS_DRDY */
#define BIT_INT_STATUS_DRDY			0x01

/* Bank0 REG_INT_STATUS */
#define BIT_INT_STATUS_AGC_RDY			0x01
#define BIT_INT_STATUS_FIFO_FULL		0x02
#define BIT_INT_STATUS_FIFO_THS			0x04
#define BIT_INT_STATUS_RESET_DONE		0x10
#define BIT_INT_STATUS_PLL_RDY			0x20
#define BIT_INT_STATUS_FSYNC			0x40
#define BIT_INT_STATUS_ST_DONE			0x80

/* Bank0 REG_INT_STATUS2 */
#define BIT_INT_STATUS_WOM_Z			0x01
#define BIT_INT_STATUS_WOM_Y			0x02
#define BIT_INT_STATUS_WOM_X			0x04
#define BIT_INT_STATUS_WOM_XYZ (BIT_INT_STATUS_WOM_X | \
		BIT_INT_STATUS_WOM_Y | BIT_INT_STATUS_WOM_Z)
#define BIT_INT_STATUS_SMD			0x08

/* Bank 0 REG_INT_STATUS3 */
#define BIT_INT_STATUS_LOWG_DET			0x02
#define BIT_INT_STATUS_FF_DET			0x04
#define BIT_INT_STATUS_TILT_DET			0x08
#define BIT_INT_STATUS_STEP_CNT_OVFL		0x10
#define BIT_INT_STATUS_STEP_DET			0x20

/* Bank0 REG_WOM_CONFIG */
#define BIT_WOM_EN_OFF				0x00
#define BIT_WOM_EN_ON				0x01
#define BIT_WOM_MODE_INITIAL			0x00
#define BIT_WOM_MODE_PREV			0x02
#define BIT_WOM_INT_MODE_OR			0x00
#define BIT_WOM_INT_MODE_AND			0x04
#define BIT_WOM_INT_DUR_LEGACY			0x00
#define BIT_WOM_INT_DUR_2ND			0x08
#define BIT_WOM_INT_DUR_3RD			0x10
#define BIT_WOM_INT_DUR_4TH			0x18

/* Bank0 REG_APEX_CONFIG0 */
#define BIT_DMP_SRAM_RESET_APEX			0x01
#define BIT_DMP_INIT_EN				0x04
#define BIT_DMP_POWER_SAVE_EN			0x08

/* Bank0 REG_APEX_CONFIG1 */
#define BIT_DMP_ODR_25HZ			0x00
#define BIT_DMP_ODR_50HZ			0x02
#define BIT_DMP_ODR_100HZ			0x03
#define BIT_DMP_PEDO_EN				0x08
#define BIT_DMP_TILT_EN				0x10
#define BIT_DMP_FF_EN				0x20
#define BIT_DMP_SMD_EN				0x40

/* Bank0 REG_WHO_AM_I */
#define BIT_I_AM_ICM42670			0x67

/* REG_OTP_CONFIG_MREG_TOP1 */
#define BIT_OTP_COPY_NORMAL			0x04
#define BIT_OTP_COPY_ST_DATA			0x0C
#define OTP_COPY_MODE_MASK                      0x0C

/* REG_INT_SOURCE6_MREG_TOP1 */
#define BIT_INT_TLT_DET_INT1_EN			0x08
#define BIT_INT_STEP_CNT_OVFL_INT1_EN		0x10
#define BIT_INT_STEP_DET_INT1_EN		0x20
#define BIT_INT_LOWG_INT1_EN			0x40
#define BIT_INT_FF_INT1_EN			0x80

/* REG_TMST_CONFIG1_MREG_TOP1 */
#define BIT_TMST_EN				0x01
#define BIT_TMST_FSYNC_EN			0x02
#define BIT_TMST_DELTA_EN			0x04
#define BIT_TMST_RESOL				0x08
#define BIT_TMST_ON_SREG_EN			0x10
#define BIT_ODR_EN_WITHOUT_SENSOR		0x40

/* REG_FIFO_CONFIG5_MREG_TOP1 */
#define BIT_FIFO_ACCEL_EN			0x01
#define BIT_FIFO_GYRO_EN			0x02
#define BIT_FIFO_TMST_FSYNC_EN			0x04
#define BIT_FIFO_HIRES_EN			0x08
#define BIT_RESUME_PARTIAL_RD			0x10
#define BIT_WM_GT_TH				0x20

/* REG_SELFTEST_MREG_TOP1 */
#define BIT_EN_AX_ST				0x01
#define BIT_EN_AY_ST				0x02
#define BIT_EN_AZ_ST				0x04
#define BIT_EN_GX_ST				0x08
#define BIT_EN_GY_ST				0x10
#define BIT_EN_GZ_ST				0x20
#define BIT_ACCEL_ST_EN				0x40
#define BIT_GYRO_ST_EN				0x80

/* REG_ST_CONFIG_MREG_TOP1 */
#define BIT_PD_ACCEL_CP45_ST_REG		0x80
#define SHIFT_GYRO_ST_LIM			0
#define SHIFT_ACCEL_ST_LIM			3
#define SHIFT_ST_NUM_SAMPLE			6

/* REG_ST_STATUS1_MREG_TOP1 */
#define BIT_DMP_AX_ST_PASS			0x02
#define BIT_DMP_AY_ST_PASS			0x04
#define BIT_DMP_AZ_ST_PASS			0x08
#define BIT_DMP_ACCEL_ST_DONE			0x10
#define BIT_DMP_ACCEL_ST_PASS			0x20

/* REG_ST_STATUS2_MREG_TOP1 */
#define BIT_DMP_GX_ST_PASS			0x02
#define BIT_DMP_GY_ST_PASS			0x04
#define BIT_DMP_GZ_ST_PASS			0x08
#define BIT_DMP_GYRO_ST_DONE			0x10
#define BIT_DMP_GYRO_ST_PASS			0x20
#define BIT_DMP_ST_INCOMPLETE			0x40

/* REG_OTP_CTRL7_MREG_OTP */
#define BIT_OTP_RELOAD				0x08
#define BIT_OTP_PWR_DOWN			0x02

/** FIFO CONTENT DEFINITION */
#define HEADER_SIZE		1
#define ACCEL_DATA_SIZE	6
#define GYRO_DATA_SIZE	 6
#define TEMP_DATA_SIZE	 1
#define TS_FSYNC_SIZE	  2

/* M-reg access wait tile */
#define ICM42670_MCLK_WAIT_US		20
#define ICM42670_BLK_SEL_WAIT_US		10
#define ICM42670_MADDR_WAIT_US		10
#define ICM42670_M_RW_WAIT_US		10


/* Allowed timestamp period jitter in percent */
#define ICM42670_TS_PERIOD_JITTER	4

enum ICM406xx_fio_format {
	FIFO_20_BYTE,
	FIFO_ACCEL_ONLY,
	FIFO_GYRO_ONLY,
	FIFO_16_BYTE,
};

enum inv_icm42670_sensor_mode {
	INV_ICM42670_SENSOR_MODE_OFF,
	INV_ICM42670_SENSOR_MODE_STANDBY,
	INV_ICM42670_SENSOR_MODE_LOW_POWER,
	INV_ICM42670_SENSOR_MODE_LOW_NOISE,
	INV_ICM42670_SENSOR_MODE_NB,
};

#define FIFO_ACCEL_EN		   0x40
#define FIFO_GYRO_EN			0x20
#define FIFO_TS_MASK			0x0C
#define FIFO_FSYNC_BITS		 0x0C
#define HAVANA_MAX_PACKET_SIZE	  20
#define ICM42670_FIFO_COUNT_LIMIT		 60
#define ICM42670_DATA_BUFF_SIZE 960

// ODR
enum icm42670_odr_index {
	ICM42670_ODR_RESERVED0 = 0,
	ICM42670_ODR_RESERVED1,
	ICM42670_ODR_RESERVED2,
	ICM42670_ODR_8KHZ,
	ICM42670_ODR_4KHZ,
	ICM42670_ODR_2KHZ,
	ICM42670_ODR_1KHZ,
	ICM42670_ODR_200HZ,
	ICM42670_ODR_100HZ,
	ICM42670_ODR_50HZ,
	ICM42670_ODR_25HZ,
	ICM42670_NUM_ODRS, /* must be last */
};

struct icm42670_chip_config {
	unsigned int fsr:2;
	unsigned int lpf:3;
	unsigned int accl_fs:2;
	int gyro_odr:10;
	int accel_odr:10;
	unsigned int accl_fifo_enable:1;
	unsigned int gyro_fifo_enable:1;
	unsigned int temp_fifo_enable:1;
	unsigned int time_fifo_enable:1;
	u8 divider;
	u8 user_ctrl;
};

struct icm42670_data {
	struct mutex lock;
	struct regmap *regmap;
	struct iio_trigger  *trig;
	struct device_node	*node;
	struct gpio_desc *int1_gpiod;
	struct regulator *vdd_supply;
	struct regulator *vddio_supply;
	u16  accel_frequency;
	u16  gyro_frequency;
	u16  accel_frequency_buff;
	u16  gyro_frequency_buff;
	u16  accel_lpf_bw;
	u16  gyro_lpf_bw;
	u16  accel_lpf_bw_buff;
	u16  gyro_lpf_bw_buff;
	int irq;
	u8 irq_mask;
	int chip_type; // not used
	unsigned int powerup_count;
	struct icm42670_chip_config chip_config;
	int skip_samples;
	s64 it_timestamp;		// Timestamp of when the data was read
	s64 data_timestamp;		 // Timestamp of when the data was generated
	s64 standard_period;	 // Standard interrupt period in nanoseconds
	s64 interrupt_period;	// Actual interrupt period in nanoseconds
	s64 period_min;		// Minimum interrupt period deviation in nanoseconds
	s64 period_max;		 // Maximum interrupt period deviation in nanoseconds
	int period_divider;
	int interrupt_regval;
	bool enable_fifo;
	u8  data_buff[ICM42670_DATA_BUFF_SIZE];
};

/* scan indexes follow DATA register order */
enum icm42670_scan_axis {
	ICM42670_SCAN_ACCEL_X = 0,
	ICM42670_SCAN_ACCEL_Y,
	ICM42670_SCAN_ACCEL_Z,
	ICM42670_SCAN_GYRO_X,
	ICM42670_SCAN_GYRO_Y,
	ICM42670_SCAN_GYRO_Z,
	ICM42670_SCAN_TEMP,
	ICM42670_SCAN_TIMESTAMP,
};

enum icm42670_sensor_type {
	ICM42670_ACCEL = 0,
	ICM42670_GYRO,
	ICM42670_TEMP,
	ICM42670_TIMESTAMP,
	ICM42670_NUM_SENSORS /* must be last */
};

#define IIO_TRIGGER 1
#define ICM42670_RESET_FLAG 1
#define ICM42670_DEBUG_FLAG 0

// fifo
#define ICM42670_OUTPUT_DATA_SIZE 24 // align 8, last 8 for timestamp
#define ICM42670_OUTPUT_DATA_SIZE_PULS_ONE 25
#define ICM42670_ACCEL_GYRO_SIZE	12
#define ICM42670_FIFO_DATUM			   16
#define ICM42670_BYTES_PER_3AXIS_SENSOR   6
#define ICM42670_FIFO_COUNT_BYTE		  2
#define ICM42670_BYTE_FIFO_TEMP		   1
#define ICM42670_FIFO_SIZE				1024

typedef int (*icm42670_bus_setup)(struct icm42670_data *);

extern const struct regmap_config icm42670_regmap_config;
extern const struct dev_pm_ops icm42670_pm_ops;

irqreturn_t icm42670_read_fifo(int irq, void *p);
int icm42670_reset_fifo(struct iio_dev *indio_dev);
irqreturn_t icm42670_read_data(int irq, void *p);
int icm42670_reset_data(struct iio_dev *indio_dev);
int icm42670_core_probe(struct regmap *regmap, int irq, const char *name,
						int chip_type, icm42670_bus_setup bus_setup);
void icm42670_core_remove(struct device *dev);
int icm42670_probe_trigger(struct iio_dev *indio_dev, int irq_type);
int icm42670_set_enable(struct iio_dev *indio_dev, bool enable);
int icm42670_mreg_single_write(struct icm42670_data *st, int addr, u32 data);
int icm42670_set_mode(struct icm42670_data *data,
				enum icm42670_sensor_type t,
				bool mode);
int icm42670_mreg_read(struct icm42670_data *st, int addr, int len, u32 *data);

#endif
