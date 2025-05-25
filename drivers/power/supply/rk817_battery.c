// SPDX-License-Identifier: GPL-2.0+
/*
 * rk817 battery  driver
 *
 * Copyright (C) 2018 Rockchip Electronics Co., Ltd.
 *
 */

#define pr_fmt(fmt) "rk817-bat: " fmt

#include <linux/delay.h>
#include <linux/extcon.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/mfd/rk808.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/rk_usbbc.h>
#include <linux/regmap.h>
#include <linux/rk_keys.h>
#include <linux/rtc.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

static int dbg_enable;

module_param_named(dbg_level, dbg_enable, int, 0644);

#define DBG(args...) \
	do { \
		if (dbg_enable) { \
			pr_info(args); \
		} \
	} while (0)

#define BAT_INFO(fmt, args...) pr_info(fmt, ##args)

#define DRIVER_VERSION	"1.10"
#define SFT_SET_KB	1

#define DIV(x)	((x) ? (x) : 1)
#define ENABLE	0x01
#define DISABLE	0x00
#define MAX_INTERPOLATE		1000
/* Maximum Resolution Precision of Percentage: 0.001% */
#define MAX_PERCENTAGE		(100 * 1000)
#define MAX_INT			0x7FFF
#define OCV_SAMP_MIN_MSK	0x0c
#define OCV_SAMP_8MIN		(0x00 << 2)
#define MINUTE(x)	\
	((x) * 60)

#define ADC_TO_CURRENT(adc_value, samp_res)	\
	(adc_value * 1720 / 1000 / samp_res)
#define CURRENT_TO_ADC(current, samp_res)	\
	(current * 1000 * samp_res / 1720)

#define ADC_TO_CAPACITY(adc_value, samp_res)	\
	(adc_value / 1000 * 1720 / 3600 / samp_res)
#define CAPACITY_TO_ADC(capacity, samp_res)	\
	(capacity * samp_res * 3600 / 1720 * 1000)

#define ADC_TO_CAPACITY_UAH(adc_value, samp_res)	\
	(adc_value / 3600 * 1720 / samp_res)
#define ADC_TO_CAPACITY_MAH(adc_value, samp_res)	\
	(adc_value / 1000 * 1720 / 3600 / samp_res)

/* Adjust full capacity to a reduced value */
#define UPDATE_REDUCE_FCC(fcc)		((fcc) * 995 / 1000)
/* Raise the maximum capacity value */
#define UPDATE_RAISE_FCC(fcc)		((fcc) * 1005 / 1000)
/* Effective full capacity */
#define EFFECTIVE_FULL_MIN_CAP(fcc)	((fcc) * 800 / 1000)
#define EFFECTIVE_FULL_MAX_CAP(fcc)	((fcc) * 1200 / 1000)
/* Battery Percentage (or State of Charge (SOC) Percentage) */
#define BATTERY_PERCENTAGE(n)		(n * 1000)

#define ADC_CALIB_THRESHOLD		4
#define ADC_CALIB_LMT_MIN		3
#define ADC_CALIB_CNT			5

/* default param */
#define DEFAULT_BAT_RES			135
#define DEFAULT_SLP_ENTER_CUR		300
#define DEFAULT_SLP_EXIT_CUR		300
#define DEFAULT_SLP_FILTER_CUR		100
#define DEFAULT_PWROFF_VOL_THRESD	3400
#define DEFAULT_MONITOR_SEC		5
#define DEFAULT_SAMPLE_RES		20

/* sleep */
#define SLP_CURR_MAX			40
#define SLP_CURR_MIN			6
#define LOW_PWR_SLP_CURR_MAX		20
#define LOW_PWR_SLP_CURR_MIN		1
#define DISCHRG_TIME_STEP1		MINUTE(10)
#define DISCHRG_TIME_STEP2		MINUTE(60)
#define SLP_DSOC_VOL_THRESD		3600
#define REBOOT_PERIOD_SEC		180
#define REBOOT_MAX_CNT			80

#define TIMER_MS_COUNTS			1000
/* fcc */
#define MIN_FCC				500
#define CAP_INVALID			0x80

/* virtual params */
#define VIRTUAL_CURRENT			1000
#define VIRTUAL_VOLTAGE			3888
#define VIRTUAL_SOC			66
#define VIRTUAL_PRESET			1
#define VIRTUAL_TEMPERATURE		188
#define VIRTUAL_STATUS			POWER_SUPPLY_STATUS_CHARGING

#define FINISH_CHRG_CUR1		1000
#define FINISH_CHRG_CUR2		1500
#define FINISH_MAX_SOC_DELAY		20
/* Discharge Current Threshold */
#define FINISH_CURR_THRESD		(-30)
/* OCV Table Percentage Accuracy: 5.000% */
#define OCV_TABLE_STEP			5000

enum ts_fun {
	TS_FUN_SOURCE_CURRENT,
	TS_FUN_VOLTAGE_INPUT,
};

enum tscur_sel {
	FLOW_OUT_10uA,
	FLOW_OUT_20uA,
	FLOW_OUT_30uA,
	FLOW_OUT_40uA,
};

enum charge_current {
	CHRG_CUR_1000MA,
	CHRG_CUR_1500MA,
	CHRG_CUR_2000MA,
	CHRG_CUR_2500MA,
	CHRG_CUR_2750MA,
	CHRG_CUR_3000MA,
	CHRG_CUR_3500MA,
	CHRG_CUR_500MA,
};

enum charge_voltage {
	CHRG_VOL_4100MV,
	CHRG_VOL_4150MV,
	CHRG_VOL_4200MV,
	CHRG_VOL_4250MV,
	CHRG_VOL_4300MV,
	CHRG_VOL_4350MV,
	CHRG_VOL_4400MV,
	CHRG_VOL_4450MV,
};

enum work_mode {
	MODE_ZERO = 0,
	MODE_FINISH,
	MODE_SMOOTH_CHRG,
	MODE_SMOOTH_DISCHRG,
	MODE_SMOOTH,
};

enum charge_status {
	CHRG_OFF,
	DEAD_CHRG,
	TRICKLE_CHRG,
	CC_OR_CV_CHRG,
	CHARGE_FINISH,
	USB_OVER_VOL,
	BAT_TMP_ERR,
	BAT_TIM_ERR,
};

enum bat_mode {
	MODE_BATTARY = 0,
	MODE_VIRTUAL,
};

enum rk817_sample_time {
	S_8_MIN,
	S_16_MIN,
	S_32_MIN,
	S_48_MIN,
};

enum rk817_output_mode {
	AVERAGE_MODE,
	INSTANT_MODE,
};

enum rk817_battery_fields {
	ADC_SLP_RATE, BAT_CUR_ADC_EN, BAT_VOL_ADC_EN,
	USB_VOL_ADC_EN, TS_ADC_EN, SYS_VOL_ADC_EN, GG_EN, /*ADC_CONFIG0*/
	CUR_ADC_DITH_SEL, CUR_ADC_DIH_EN, CUR_ADC_CHOP_EN,
	CUR_ADC_CHOP_SEL, CUR_ADC_CHOP_VREF_EN, /*CUR_ADC_CFG0*/
	CUR_ADC_VCOM_SEL, CUR_ADC_VCOM_BUF_INC, CUR_ADC_VREF_BUF_INC,
	CUR_ADC_BIAS_DEC, CUR_ADC_IBIAS_SEL,/*CUR_ADC_CFG1*/
	VOL_ADC_EXT_VREF_EN, VOL_ADC_DITH_SEL, VOL_ADC_DITH_EN,
	VOL_ADC_CHOP_EN, VOL_ADC_CHOP_SEL, VOL_ADC_CHOP_VREF_EN,
	VOL_ADC_VCOM_SEL, VOL_ADC_VCOM_BUF_INC, VOL_ADC_VREF_BUF_INC,
	VOL_ADC_IBIAS_SEL, /*VOL_ADC_CFG1*/
	RLX_CUR_FILTER, TS_FUN, VOL_ADC_TSCUR_SEL,
	VOL_CALIB_UPD, CUR_CALIB_UPD, /*ADC_CONFIG1*/
	CUR_OUT_MOD, VOL_OUT_MOD, FRAME_SMP_INTERV,
	ADC_OFF_CAL_INTERV, RLX_SPT, /*GG_CON*/
	OCV_UPD, RELAX_STS, RELAX_VOL2_UPD, RELAX_VOL1_UPD, BAT_CON,
	QMAX_UPD_SOFT, TERM_UPD, OCV_STS, /*GG_STS*/
	RELAX_THRE_H, RELAX_THRE_L, /*RELAX_THRE*/
	RELAX_VOL1_H, RELAX_VOL1_L,
	RELAX_VOL2_H, RELAX_VOL2_L,
	RELAX_CUR1_H, RELAX_CUR1_L,
	RELAX_CUR2_H, RELAX_CUR2_L,
	OCV_THRE_VOL,
	OCV_VOL_H, OCV_VOL_L,
	OCV_VOL0_H, OCV_VOL0_L,
	OCV_CUR_H, OCV_CUR_L,
	OCV_CUR0_H, OCV_CUR0_L,
	PWRON_VOL_H, PWRON_VOL_L,
	PWRON_CUR_H, PWRON_CUR_L,
	OFF_CNT,
	Q_INIT_H3, Q_INIT_H2, Q_INIT_L1, Q_INIT_L0,
	Q_PRESS_H3, Q_PRESS_H2, Q_PRESS_L1, Q_PRESS_L0,
	BAT_VOL_H, BAT_VOL_L,
	BAT_CUR_H, BAT_CUR_L,
	BAT_TS_H, BAT_TS_L,
	USB_VOL_H, USB_VOL_L,
	SYS_VOL_H, SYS_VOL_L,
	Q_MAX_H3, Q_MAX_H2, Q_MAX_L1, Q_MAX_L0,
	Q_TERM_H3, Q_TERM_H2, Q_TERM_L1, Q_TERM_L0,
	Q_OCV_H3, Q_OCV_H2, Q_OCV_L1, Q_OCV_L0,
	OCV_CNT,
	SLEEP_CON_SAMP_CUR_H, SLEEP_CON_SAMP_CUR_L,
	CAL_OFFSET_H, CAL_OFFSET_L,
	VCALIB0_H, VCALIB0_L,
	VCALIB1_H, VCALIB1_L,
	IOFFSET_H, IOFFSET_L,
	BAT_R0, SOC_REG0, SOC_REG1, SOC_REG2,
	REMAIN_CAP_REG2, REMAIN_CAP_REG1, REMAIN_CAP_REG0,
	NEW_FCC_REG2, NEW_FCC_REG1, NEW_FCC_REG0,
	RESET_MODE,
	FG_INIT, HALT_CNT_REG, CALC_REST_REGL, UPDATE_LEVE_REG,
	VOL_ADC_B3, VOL_ADC_B2, VOL_ADC_B1, VOL_ADC_B0,
	VOL_ADC_K3, VOL_ADC_K2, VOL_ADC_K1, VOL_ADC_K0,
	BAT_EXS, CHG_STS, BAT_OVP_STS, CHRG_IN_CLAMP,
	CHIP_NAME_H, CHIP_NAME_L, CHRG_CUR_SEL, CHRG_VOL_SEL,
	PLUG_IN_STS, BAT_LTS_TS, USB_SYS_EN,
	F_MAX_FIELDS
};

static const struct reg_field rk817_battery_reg_fields[] = {
	[ADC_SLP_RATE] = REG_FIELD(0x50, 0, 0),
	[BAT_CUR_ADC_EN] = REG_FIELD(0x50, 2, 2),
	[BAT_VOL_ADC_EN] = REG_FIELD(0x50, 3, 3),
	[USB_VOL_ADC_EN] = REG_FIELD(0x50, 4, 4),
	[TS_ADC_EN] = REG_FIELD(0x50, 5, 5),
	[SYS_VOL_ADC_EN] = REG_FIELD(0x50, 6, 6),
	[GG_EN] = REG_FIELD(0x50, 7, 7),/*ADC_CONFIG0*/

	[CUR_ADC_DITH_SEL] = REG_FIELD(0x51, 1, 3),
	[CUR_ADC_DIH_EN] = REG_FIELD(0x51, 4, 4),
	[CUR_ADC_CHOP_EN] = REG_FIELD(0x51, 5, 5),
	[CUR_ADC_CHOP_SEL] = REG_FIELD(0x51, 6, 6),
	[CUR_ADC_CHOP_VREF_EN] = REG_FIELD(0x51, 7, 7), /*CUR_ADC_COFG0*/

	[CUR_ADC_VCOM_SEL] = REG_FIELD(0x52, 0, 1),
	[CUR_ADC_VCOM_BUF_INC] = REG_FIELD(0x52, 2, 2),
	[CUR_ADC_VREF_BUF_INC] = REG_FIELD(0x52, 3, 3),
	[CUR_ADC_BIAS_DEC] = REG_FIELD(0x52, 4, 4),
	[CUR_ADC_IBIAS_SEL] = REG_FIELD(0x52, 5, 6), /*CUR_ADC_COFG1*/

	[VOL_ADC_EXT_VREF_EN] = REG_FIELD(0x53, 0, 0),
	[VOL_ADC_DITH_SEL]  = REG_FIELD(0x53, 1, 3),
	[VOL_ADC_DITH_EN] = REG_FIELD(0x53, 4, 4),
	[VOL_ADC_CHOP_EN] = REG_FIELD(0x53, 5, 5),
	[VOL_ADC_CHOP_SEL] = REG_FIELD(0x53, 6, 6),
	[VOL_ADC_CHOP_VREF_EN] = REG_FIELD(0x53, 7, 7),/*VOL_ADC_COFG0*/

	[VOL_ADC_VCOM_SEL] = REG_FIELD(0x54, 0, 1),
	[VOL_ADC_VCOM_BUF_INC] = REG_FIELD(0x54, 2, 2),
	[VOL_ADC_VREF_BUF_INC] = REG_FIELD(0x54, 3, 3),
	[VOL_ADC_IBIAS_SEL] = REG_FIELD(0x54, 5, 6), /*VOL_ADC_COFG1*/

	[RLX_CUR_FILTER] = REG_FIELD(0x55, 0, 1),
	[TS_FUN] = REG_FIELD(0x55, 3, 3),
	[VOL_ADC_TSCUR_SEL] = REG_FIELD(0x55, 4, 5),
	[VOL_CALIB_UPD] = REG_FIELD(0x55, 6, 6),
	[CUR_CALIB_UPD] = REG_FIELD(0x55, 7, 7), /*ADC_CONFIG1*/

	[CUR_OUT_MOD] = REG_FIELD(0x56, 0, 0),
	[VOL_OUT_MOD] = REG_FIELD(0x56, 1, 1),
	[FRAME_SMP_INTERV] = REG_FIELD(0x56, 2, 3),
	[ADC_OFF_CAL_INTERV] = REG_FIELD(0x56, 4, 5),
	[RLX_SPT] = REG_FIELD(0x56, 6, 7), /*GG_CON*/

	[OCV_UPD] = REG_FIELD(0x57, 0, 0),
	[RELAX_STS] = REG_FIELD(0x57, 1, 1),
	[RELAX_VOL2_UPD] = REG_FIELD(0x57, 2, 2),
	[RELAX_VOL1_UPD] = REG_FIELD(0x57, 3, 3),
	[BAT_CON] = REG_FIELD(0x57, 4, 4),
	[QMAX_UPD_SOFT] = REG_FIELD(0x57, 5, 5),
	[TERM_UPD] = REG_FIELD(0x57, 6, 6),
	[OCV_STS] = REG_FIELD(0x57, 7, 7), /*GG_STS*/

	[RELAX_THRE_H] = REG_FIELD(0x58, 0, 7),
	[RELAX_THRE_L] = REG_FIELD(0x59, 0, 7),

	[RELAX_VOL1_H] = REG_FIELD(0x5A, 0, 7),
	[RELAX_VOL1_L] = REG_FIELD(0x5B, 0, 7),
	[RELAX_VOL2_H] = REG_FIELD(0x5C, 0, 7),
	[RELAX_VOL2_L] = REG_FIELD(0x5D, 0, 7),

	[RELAX_CUR1_H] = REG_FIELD(0x5E, 0, 7),
	[RELAX_CUR1_L] = REG_FIELD(0x5F, 0, 7),
	[RELAX_CUR2_H] = REG_FIELD(0x60, 0, 7),
	[RELAX_CUR2_L] = REG_FIELD(0x61, 0, 7),

	[OCV_THRE_VOL] = REG_FIELD(0x62, 0, 7),

	[OCV_VOL_H] = REG_FIELD(0x63, 0, 7),
	[OCV_VOL_L] = REG_FIELD(0x64, 0, 7),
	[OCV_VOL0_H] = REG_FIELD(0x65, 0, 7),
	[OCV_VOL0_L] = REG_FIELD(0x66, 0, 7),
	[OCV_CUR_H] = REG_FIELD(0x67, 0, 7),
	[OCV_CUR_L] = REG_FIELD(0x68, 0, 7),
	[OCV_CUR0_H] = REG_FIELD(0x69, 0, 7),
	[OCV_CUR0_L] = REG_FIELD(0x6A, 0, 7),
	[PWRON_VOL_H] = REG_FIELD(0x6B, 0, 7),
	[PWRON_VOL_L] = REG_FIELD(0x6C, 0, 7),
	[PWRON_CUR_H] = REG_FIELD(0x6D, 0, 7),
	[PWRON_CUR_L] = REG_FIELD(0x6E, 0, 7),
	[OFF_CNT] = REG_FIELD(0x6F, 0, 7),
	[Q_INIT_H3] = REG_FIELD(0x70, 0, 7),
	[Q_INIT_H2] = REG_FIELD(0x71, 0, 7),
	[Q_INIT_L1] = REG_FIELD(0x72, 0, 7),
	[Q_INIT_L0] = REG_FIELD(0x73, 0, 7),

	[Q_PRESS_H3] = REG_FIELD(0x74, 0, 7),
	[Q_PRESS_H2] = REG_FIELD(0x75, 0, 7),
	[Q_PRESS_L1] = REG_FIELD(0x76, 0, 7),
	[Q_PRESS_L0] = REG_FIELD(0x77, 0, 7),

	[BAT_VOL_H] = REG_FIELD(0x78, 0, 7),
	[BAT_VOL_L] = REG_FIELD(0x79, 0, 7),

	[BAT_CUR_H] = REG_FIELD(0x7A, 0, 7),
	[BAT_CUR_L] = REG_FIELD(0x7B, 0, 7),

	[BAT_TS_H] = REG_FIELD(0x7C, 0, 7),
	[BAT_TS_L] = REG_FIELD(0x7D, 0, 7),
	[USB_VOL_H] = REG_FIELD(0x7E, 0, 7),
	[USB_VOL_L] = REG_FIELD(0x7F, 0, 7),

	[SYS_VOL_H] = REG_FIELD(0x80, 0, 7),
	[SYS_VOL_L] = REG_FIELD(0x81, 0, 7),
	[Q_MAX_H3] = REG_FIELD(0x82, 0, 7),
	[Q_MAX_H2] = REG_FIELD(0x83, 0, 7),
	[Q_MAX_L1] = REG_FIELD(0x84, 0, 7),
	[Q_MAX_L0] = REG_FIELD(0x85, 0, 7),

	[Q_TERM_H3] = REG_FIELD(0x86, 0, 7),
	[Q_TERM_H2] = REG_FIELD(0x87, 0, 7),
	[Q_TERM_L1] = REG_FIELD(0x88, 0, 7),
	[Q_TERM_L0] = REG_FIELD(0x89, 0, 7),
	[Q_OCV_H3] = REG_FIELD(0x8A, 0, 7),
	[Q_OCV_H2] = REG_FIELD(0x8B, 0, 7),

	[Q_OCV_L1] = REG_FIELD(0x8C, 0, 7),
	[Q_OCV_L0] = REG_FIELD(0x8D, 0, 7),
	[OCV_CNT] = REG_FIELD(0x8E, 0, 7),
	[SLEEP_CON_SAMP_CUR_H] = REG_FIELD(0x8F, 0, 7),
	[SLEEP_CON_SAMP_CUR_L] = REG_FIELD(0x90, 0, 7),
	[CAL_OFFSET_H] = REG_FIELD(0x91, 0, 7),
	[CAL_OFFSET_L] = REG_FIELD(0x92, 0, 7),
	[VCALIB0_H] = REG_FIELD(0x93, 0, 7),
	[VCALIB0_L] = REG_FIELD(0x94, 0, 7),
	[VCALIB1_H] = REG_FIELD(0x95, 0, 7),
	[VCALIB1_L] = REG_FIELD(0x96, 0, 7),
	[IOFFSET_H] = REG_FIELD(0x97, 0, 7),
	[IOFFSET_L] = REG_FIELD(0x98, 0, 7),

	[BAT_R0] = REG_FIELD(0x99, 0, 7),
	[SOC_REG0] = REG_FIELD(0x9A, 0, 7),
	[SOC_REG1] = REG_FIELD(0x9B, 0, 7),
	[SOC_REG2] = REG_FIELD(0x9C, 0, 7),

	[REMAIN_CAP_REG0] = REG_FIELD(0x9D, 0, 7),
	[REMAIN_CAP_REG1] = REG_FIELD(0x9E, 0, 7),
	[REMAIN_CAP_REG2] = REG_FIELD(0x9F, 0, 7),
	[NEW_FCC_REG0] = REG_FIELD(0xA0, 0, 7),
	[NEW_FCC_REG1] = REG_FIELD(0xA1, 0, 7),
	[NEW_FCC_REG2] = REG_FIELD(0xA2, 0, 7),
	[RESET_MODE] = REG_FIELD(0xA3, 0, 3),
	[FG_INIT] = REG_FIELD(0xA5, 7, 7),

	[HALT_CNT_REG] = REG_FIELD(0xA6, 0, 7),
	[CALC_REST_REGL] = REG_FIELD(0xA7, 0, 7),
	[UPDATE_LEVE_REG] = REG_FIELD(0xA8, 0, 7),

	[VOL_ADC_B3] = REG_FIELD(0xA9, 0, 7),
	[VOL_ADC_B2] = REG_FIELD(0xAA, 0, 7),
	[VOL_ADC_B1] = REG_FIELD(0xAB, 0, 7),
	[VOL_ADC_B0] = REG_FIELD(0xAC, 0, 7),

	[VOL_ADC_K3] = REG_FIELD(0xAD, 0, 7),
	[VOL_ADC_K2] = REG_FIELD(0xAE, 0, 7),
	[VOL_ADC_K1] = REG_FIELD(0xAF, 0, 7),
	[VOL_ADC_K0] = REG_FIELD(0xB0, 0, 7),
	[CHRG_CUR_SEL] = REG_FIELD(0xE4, 0, 2),
	[CHRG_VOL_SEL] = REG_FIELD(0xE4, 4, 6),
	[USB_SYS_EN] = REG_FIELD(0xE6, 6, 6),
	[BAT_LTS_TS] = REG_FIELD(0xE9, 0, 7),
	[BAT_EXS] = REG_FIELD(0xEB, 7, 7),
	[CHG_STS] = REG_FIELD(0xEB, 4, 6),
	[BAT_OVP_STS] = REG_FIELD(0xEB, 3, 3),
	[CHRG_IN_CLAMP] = REG_FIELD(0xEB, 2, 2),
	[CHIP_NAME_H] = REG_FIELD(0xED, 0, 7),
	[CHIP_NAME_L] = REG_FIELD(0xEE, 0, 7),
	[PLUG_IN_STS] = REG_FIELD(0xF0, 6, 6),
};

struct temp_chrg_table {
	int temp_down;
	int temp_up;
	int chrg_current;
	int chrg_voltage;
	int chrg_current_index;
	int chrg_voltage_index;
};

struct battery_platform_data {
	u32 *ocv_table;
	u32 ocv_size;
	struct temp_chrg_table *tc_table;
	u32 tc_count;
	u32 *ntc_table;
	u32 ntc_size;
	int ntc_degree_from;
	u32 ntc_factor;
	u32 pwroff_vol;
	u32 monitor_sec;
	u32 bat_res;
	u32 design_capacity;
	u32 design_qmax;
	u32 sleep_enter_current;
	u32 sleep_exit_current;
	u32 sleep_filter_current;

	u32 bat_mode;
	u32 sample_res;
	u32 bat_res_up;
	u32 bat_res_down;
	u32 design_max_voltage;
	int fake_full_soc;
	int charge_stay_awake;
};

struct rk817_battery_device {
	struct platform_device		*pdev;
	struct device			*dev;
	struct i2c_client		*client;
	struct rk808			*rk817;
	struct power_supply		*bat;
	struct power_supply		*chg_psy;
	struct regmap_field		*rmap_fields[F_MAX_FIELDS];
	struct battery_platform_data	*pdata;
	struct workqueue_struct		*bat_monitor_wq;
	struct delayed_work		bat_delay_work;
	struct delayed_work		calib_delay_work;
	struct timer_list		caltimer;

	int				sample_res;
	int				bat_res;
	bool				is_first_power_on;
	int				chrg_status;
	bool				is_initialized;
	int				current_avg;
	int				current_relax;
	int				current_sleep;
	int				voltage_usb;
	int				voltage_sys;
	int				voltage_avg;
	int				voltage_ocv;
	int				voltage_relax;
	int				voltage_k;/* VCALIB0 VCALIB1 */
	int				voltage_b;
	u32				remain_cap;
	int				design_cap;
	int				nac;
	int				fcc;
	int				qmax;
	int				dsoc;
	int				rsoc;
	int				delta_rsoc;
	int				fake_offline;
	int				fake_full_soc;
	int				age_ocv_soc;
	bool				age_allow_update;
	int				age_level;
	int				age_ocv_cap;
	int				pwron_voltage;
	int				age_voltage;
	int				age_adjust_cap;
	int				expected_voltage;
	unsigned long			age_keep_sec;
	int				powerpatch_res;
	unsigned long			finish_base;
	time64_t			rtc_base;
	int				sm_remain_cap;
	int				delta_cap_remainder;
	int				sm_linek;
	int				smooth_soc;
	unsigned long			sleep_dischrg_sec;
	unsigned long			sleep_sum_sec;
	bool				sleep_chrg_online;
	u8				sleep_chrg_status;
	bool				s2r; /*suspend to resume*/
	u32				work_mode;
	bool				active_awake;
	int				temperature;
	u32				monitor_ms;
	u32				pwroff_min;
	u8				halt_cnt;
	bool				is_halt;
	bool				is_max_soc_offset;
	bool				is_sw_reset;
	bool				is_ocv_calib;
	bool				is_first_on;
	bool				is_force_calib;
	int				ocv_pre_dsoc;
	int				ocv_new_dsoc;
	int				charge_index;
	int				force_pre_dsoc;
	int				force_new_dsoc;

	int				dbg_dcount[10];
	int				dbg_rcount[10];
	int				dbg_pwr_dsoc;
	int				dbg_pwr_rsoc;
	int				dbg_pwr_vol;
	int				dbg_meet_soc;
	int				dbg_calc_dsoc;
	int				dbg_calc_rsoc;
	int				is_charging;
	u8				plugin_trigger;
	u8				plugout_trigger;
	int				chip_id;
	int				is_register_chg_psy;
};

static u64 get_boot_sec(void)
{
	struct timespec64 ts;

	ktime_get_boottime_ts64(&ts);

	return ts.tv_sec;
}

static unsigned long base2sec(unsigned long x)
{
	if (x)
		return (get_boot_sec() > x) ? (get_boot_sec() - x) : 0;
	else
		return 0;
}

static unsigned long base2min(unsigned long x)
{
	return base2sec(x) / 60;
}

static u32 interpolate(int value, u32 *table, int size)
{
	u8 i;
	u16 d;

	if (size < 2)
		return 0;

	for (i = 0; i < size; i++) {
		if (value < table[i])
			break;
	}

	if ((i > 0) && (i < size)) {
		d = (value - table[i - 1]) * (MAX_INTERPOLATE / (size - 1));
		d /= table[i] - table[i - 1];
		d = d + (i - 1) * (MAX_INTERPOLATE / (size - 1));
	} else {
		d = i * ((MAX_INTERPOLATE + size / 2) / size);
	}

	if (d > 1000)
		d = 1000;

	return d;
}

/* (a * b) / c */
static int32_t ab_div_c(u32 a, u32 b, u32 c)
{
	bool sign;
	u32 ans = MAX_INT;
	int tmp;

	sign = ((((a ^ b) ^ c) & 0x80000000) != 0);
	if (c != 0) {
		if (sign)
			c = -c;
		tmp = (a * b + (c >> 1)) / c;
		if (tmp < MAX_INT)
			ans = tmp;
	}

	if (sign)
		ans = -ans;

	return ans;
}

static int rk817_bat_field_read(struct rk817_battery_device *battery,
				enum rk817_battery_fields field_id)
{
	int val;
	int ret;

	ret = regmap_field_read(battery->rmap_fields[field_id], &val);
	if (ret < 0)
		return ret;

	return val;
}

static int rk817_bat_field_write(struct rk817_battery_device *battery,
				 enum rk817_battery_fields field_id,
				 unsigned int val)
{
	return regmap_field_write(battery->rmap_fields[field_id], val);
}

/*cal_offset: current offset value*/
static int rk817_bat_get_coffset(struct rk817_battery_device *battery)
{
	int  coffset_value = 0;

	coffset_value |= rk817_bat_field_read(battery, CAL_OFFSET_H) << 8;
	coffset_value |= rk817_bat_field_read(battery, CAL_OFFSET_L);

	return coffset_value;
}

static void rk817_bat_set_coffset(struct rk817_battery_device *battery, int val)
{
	u8  buf = 0;

	buf = (val >> 8) & 0xff;
	rk817_bat_field_write(battery, CAL_OFFSET_H, buf);
	buf = (val >> 0) & 0xff;
	rk817_bat_field_write(battery, CAL_OFFSET_L, buf);
}

/* current offset value calculated */
static int rk817_bat_get_ioffset(struct rk817_battery_device *battery)
{
	int  ioffset_value = 0;

	ioffset_value |= rk817_bat_field_read(battery, IOFFSET_H) << 8;
	ioffset_value |= rk817_bat_field_read(battery, IOFFSET_L);

	return ioffset_value;
}

static void rk817_bat_current_calibration(struct rk817_battery_device *battery)
{
	int pwron_value, ioffset, cal_offset;

	pwron_value = rk817_bat_field_read(battery, PWRON_CUR_H) << 8;
	pwron_value |= rk817_bat_field_read(battery, PWRON_CUR_L);

	ioffset = rk817_bat_get_ioffset(battery);

	DBG("Caloffset: 0x%x\n", rk817_bat_get_coffset(battery));
	DBG("IOFFSET: 0x%x\n", ioffset);
	if (0)
		cal_offset = pwron_value + ioffset;
	else
		cal_offset = ioffset;

	rk817_bat_set_coffset(battery, cal_offset);
	DBG("Caloffset: 0x%x\n", rk817_bat_get_coffset(battery));
	DBG("pwron_cur: 0x%x\n", pwron_value);
}

static int rk817_bat_get_vaclib0(struct rk817_battery_device *battery)
{
	int vcalib_value = 0;

	vcalib_value |= rk817_bat_field_read(battery, VCALIB0_H) << 8;
	vcalib_value |= rk817_bat_field_read(battery, VCALIB0_L);

	return vcalib_value;
}

static int rk817_bat_get_vaclib1(struct rk817_battery_device *battery)
{
	int vcalib_value = 0;

	vcalib_value |= rk817_bat_field_read(battery, VCALIB1_H) << 8;
	vcalib_value |= rk817_bat_field_read(battery, VCALIB1_L);

	return vcalib_value;
}

static void rk817_bat_init_voltage_kb(struct rk817_battery_device *battery)
{
	int vcalib0, vcalib1;

	vcalib0 = rk817_bat_get_vaclib0(battery);
	vcalib1 =  rk817_bat_get_vaclib1(battery);
	if (battery->chip_id == RK809_ID) {
		battery->voltage_k = (1050 - 600) * 1000 / DIV(vcalib1 - vcalib0);
		battery->voltage_b = 1050 - (battery->voltage_k * vcalib1) / 1000;
	} else {
		battery->voltage_k = (4025 - 2300) * 1000 / DIV(vcalib1 - vcalib0);
		battery->voltage_b = 4025 - (battery->voltage_k * vcalib1) / 1000;
	}
}

static void rk817_bat_save_age_level(struct rk817_battery_device *battery, u8 level)
{
	rk817_bat_field_write(battery, UPDATE_LEVE_REG, level);
}

static u8 rk817_bat_get_age_level(struct  rk817_battery_device *battery)
{
	return rk817_bat_field_read(battery, UPDATE_LEVE_REG);
}

static void rk817_bat_restart_relax(struct rk817_battery_device *battery)
{
	rk817_bat_field_write(battery, RELAX_VOL1_UPD, 0x00);
	rk817_bat_field_write(battery, RELAX_VOL2_UPD, 0x00);
}

static bool is_rk817_bat_relax_mode(struct rk817_battery_device *battery)
{
	u8 relax_sts, relax_vol1_upd, relax_vol2_upd;

	relax_sts = rk817_bat_field_read(battery, RELAX_STS);
	relax_vol1_upd = rk817_bat_field_read(battery, RELAX_VOL1_UPD);
	relax_vol2_upd = rk817_bat_field_read(battery, RELAX_VOL2_UPD);

	DBG("RELAX_STS: %d\n", relax_sts);
	DBG("RELAX_VOL1_UPD: %d\n", relax_vol1_upd);
	DBG("RELAX_VOL2_UPD: %d\n", relax_vol2_upd);
	if (relax_sts && relax_vol1_upd && relax_vol2_upd)
		return true;
	else
		return false;
}

static u16 rk817_bat_get_relax_vol1(struct rk817_battery_device *battery)
{
	u16 vol, val = 0;

	val = rk817_bat_field_read(battery, RELAX_VOL1_H) << 8;
	val |= rk817_bat_field_read(battery, RELAX_VOL1_L);
	vol = battery->voltage_k * val / 1000 + battery->voltage_b;

	return vol;
}

static u16 rk817_bat_get_relax_vol2(struct rk817_battery_device *battery)
{
	u16 vol, val = 0;

	val = rk817_bat_field_read(battery, RELAX_VOL2_H) << 8;
	val |= rk817_bat_field_read(battery, RELAX_VOL2_L);
	vol = battery->voltage_k * val / 1000 + battery->voltage_b;

	return vol;
}

static u16 rk817_bat_get_relax_voltage(struct rk817_battery_device *battery)
{
	u16 relax_vol1, relax_vol2;

	if (!is_rk817_bat_relax_mode(battery))
		return 0;

	relax_vol1 = rk817_bat_get_relax_vol1(battery);
	relax_vol2 = rk817_bat_get_relax_vol2(battery);

	return relax_vol1 > relax_vol2 ? relax_vol1 : relax_vol2;
}

static void rk817_bat_set_relax_sample(struct rk817_battery_device *battery)
{
	u8 buf;
	int enter_thres, filter_thres;
	struct battery_platform_data *pdata = battery->pdata;

	enter_thres = CURRENT_TO_ADC(pdata->sleep_enter_current,
				     battery->sample_res);
	filter_thres = CURRENT_TO_ADC(pdata->sleep_filter_current,
				      battery->sample_res);

	/* set relax enter and exit threshold */
	buf = (enter_thres >> 8) & 0xff;
	rk817_bat_field_write(battery, RELAX_THRE_H, buf);
	buf = enter_thres & 0xff;
	rk817_bat_field_write(battery, RELAX_THRE_L, buf);
	/* set sample current threshold */
	buf = (filter_thres >> 8) & 0xff;
	rk817_bat_field_write(battery, SLEEP_CON_SAMP_CUR_H, buf);
	buf = filter_thres & 0xff;
	rk817_bat_field_write(battery, SLEEP_CON_SAMP_CUR_L, buf);

	/* reset relax update state */
	rk817_bat_restart_relax(battery);
	DBG("<%s>. sleep_enter_current = %d, sleep_exit_current = %d\n",
	    __func__, pdata->sleep_enter_current, pdata->sleep_exit_current);
}

static int rk817_bat_get_ocv_voltage(struct rk817_battery_device *battery)
{
	int vol, val = 0, vol_temp;

	val = rk817_bat_field_read(battery, OCV_VOL_H) << 8;
	val |= rk817_bat_field_read(battery, OCV_VOL_L);
	vol = battery->voltage_k * val / 1000 + battery->voltage_b;

	if (battery->chip_id == RK809_ID) {
		vol_temp = vol * battery->pdata->bat_res_up /
			   battery->pdata->bat_res_down + vol;
		vol = vol_temp;
	}

	return vol;
}

static int rk817_bat_get_ocv0_voltage0(struct rk817_battery_device *battery)
{
	int vol, val = 0, vol_temp;

	val = rk817_bat_field_read(battery, OCV_VOL0_H) << 8;
	val |= rk817_bat_field_read(battery, OCV_VOL0_L);
	vol = battery->voltage_k * val / 1000 + battery->voltage_b;
	if (battery->chip_id == RK809_ID) {
		vol_temp = vol * battery->pdata->bat_res_up /
			   battery->pdata->bat_res_down + vol;
		vol = vol_temp;
	}

	return vol;
}

/* power on battery voltage */
static int rk817_bat_get_pwron_voltage(struct rk817_battery_device *battery)
{
	int vol, val = 0, vol_temp;

	val = rk817_bat_field_read(battery, PWRON_VOL_H) << 8;
	val |= rk817_bat_field_read(battery, PWRON_VOL_L);
	vol = battery->voltage_k * val / 1000 + battery->voltage_b;
	if (battery->chip_id == RK809_ID) {
		vol_temp = vol * battery->pdata->bat_res_up /
			   battery->pdata->bat_res_down + vol;
		vol = vol_temp;
	}

	return vol;
}

static int rk817_bat_get_battery_voltage(struct rk817_battery_device *battery)
{
	int vol, val = 0, vol_temp;

	val = rk817_bat_field_read(battery, BAT_VOL_H) << 8;
	val |= rk817_bat_field_read(battery, BAT_VOL_L) << 0;

	vol = battery->voltage_k * val / 1000 + battery->voltage_b;

	if (battery->chip_id == RK809_ID) {
		vol_temp = vol * battery->pdata->bat_res_up /
			   battery->pdata->bat_res_down + vol;
		vol = vol_temp;
	}

	return vol;
}

static int rk817_bat_get_USB_voltage(struct rk817_battery_device *battery)
{
	int vol, val = 0, vol_temp;

	rk817_bat_field_write(battery, USB_VOL_ADC_EN, 0x01);

	val = rk817_bat_field_read(battery, USB_VOL_H) << 8;
	val |= rk817_bat_field_read(battery, USB_VOL_L) << 0;

	vol = (battery->voltage_k * val / 1000 + battery->voltage_b) * 60 / 46;

	if (battery->chip_id == RK809_ID) {
		vol_temp = vol * battery->pdata->bat_res_up /
			   battery->pdata->bat_res_down + vol;
		vol = vol_temp;
	}

	return vol;
}

static int rk817_bat_get_sys_voltage(struct rk817_battery_device *battery)
{
	int vol, val = 0, vol_temp;

	val = rk817_bat_field_read(battery, SYS_VOL_H) << 8;
	val |= rk817_bat_field_read(battery, SYS_VOL_L) << 0;

	vol = (battery->voltage_k * val / 1000 + battery->voltage_b) * 60 / 46;

	if (battery->chip_id == RK809_ID) {
		vol_temp = vol * battery->pdata->bat_res_up /
			   battery->pdata->bat_res_down + vol;
		vol = vol_temp;
	}

	return vol;
}

static int rk817_bat_get_avg_current(struct rk817_battery_device *battery)
{
	int cur, val = 0;

	val = rk817_bat_field_read(battery, BAT_CUR_H) << 8;
	val |= rk817_bat_field_read(battery, BAT_CUR_L);

	if (val & 0x8000)
		val -= 0x10000;

	cur = ADC_TO_CURRENT(val, battery->sample_res);

	return cur;
}

static int rk817_bat_get_relax_cur1(struct rk817_battery_device *battery)
{
	int cur, val = 0;

	val = rk817_bat_field_read(battery, RELAX_CUR1_H) << 8;
	val |= rk817_bat_field_read(battery, RELAX_CUR1_L);

	if (val & 0x8000)
		val -= 0x10000;

	cur = ADC_TO_CURRENT(val, battery->sample_res);

	return cur;
}

static int rk817_bat_get_relax_cur2(struct rk817_battery_device *battery)
{
	int cur, val = 0;

	val = rk817_bat_field_read(battery, RELAX_CUR2_L);
	val |= rk817_bat_field_read(battery, RELAX_CUR2_H) << 8;

	if (val & 0x8000)
		val -= 0x10000;

	cur = ADC_TO_CURRENT(val, battery->sample_res);

	return cur;
}

static int rk817_bat_get_relax_current(struct rk817_battery_device *battery)
{
	int relax_cur1, relax_cur2;

	if (!is_rk817_bat_relax_mode(battery))
		return 0;

	relax_cur1 = rk817_bat_get_relax_cur1(battery);
	relax_cur2 = rk817_bat_get_relax_cur2(battery);

	return (relax_cur1 < relax_cur2) ? relax_cur1 : relax_cur2;
}

static int rk817_bat_get_ocv_current(struct rk817_battery_device *battery)
{
	int cur, val = 0;

	val = rk817_bat_field_read(battery, OCV_CUR_H) << 8;
	val |= rk817_bat_field_read(battery, OCV_CUR_L);

	if (val & 0x8000)
		val -= 0x10000;

	cur = ADC_TO_CURRENT(val, battery->sample_res);

	return cur;
}

static int rk817_bat_get_ocv_current0(struct rk817_battery_device *battery)
{
	int cur, val = 0;

	val = rk817_bat_field_read(battery, OCV_CUR0_H) << 8;
	val |= rk817_bat_field_read(battery, OCV_CUR0_L);

	if (val & 0x8000)
		val -= 0x10000;

	cur = ADC_TO_CURRENT(val, battery->sample_res);

	return cur;
}

static int rk817_bat_get_pwron_current(struct rk817_battery_device *battery)
{
	int cur, val = 0;

	val = rk817_bat_field_read(battery, PWRON_CUR_H) << 8;
	val |= rk817_bat_field_read(battery, PWRON_CUR_L);

	if (val & 0x8000)
		val -= 0x10000;
	cur = ADC_TO_CURRENT(val, battery->sample_res);

	return cur;
}

static bool rk817_bat_remain_cap_is_valid(struct rk817_battery_device *battery)
{
	return !(rk817_bat_field_read(battery, Q_PRESS_H3) & CAP_INVALID);
}

static u32 rk817_bat_get_capacity_uah(struct rk817_battery_device *battery)
{
	u32 val = 0, capacity = 0;

	if (rk817_bat_remain_cap_is_valid(battery)) {
		val = rk817_bat_field_read(battery, Q_PRESS_H3) << 24;
		val |= rk817_bat_field_read(battery, Q_PRESS_H2) << 16;
		val |= rk817_bat_field_read(battery, Q_PRESS_L1) << 8;
		val |= rk817_bat_field_read(battery, Q_PRESS_L0) << 0;

		capacity = ADC_TO_CAPACITY_UAH(val, battery->sample_res);
	}

	return  capacity;
}

static u32 rk817_bat_get_capacity_mah(struct rk817_battery_device *battery)
{
	u32 val, capacity = 0;

	if (rk817_bat_remain_cap_is_valid(battery)) {
		val = rk817_bat_field_read(battery, Q_PRESS_H3) << 24;
		val |= rk817_bat_field_read(battery, Q_PRESS_H2) << 16;
		val |= rk817_bat_field_read(battery, Q_PRESS_L1) << 8;
		val |= rk817_bat_field_read(battery, Q_PRESS_L0) << 0;

		capacity = ADC_TO_CAPACITY(val, battery->sample_res);
	}
	DBG("Q_PRESS_H3 = 0x%x\n", rk817_bat_field_read(battery, Q_PRESS_H3));
	DBG("Q_PRESS_H2 = 0x%x\n", rk817_bat_field_read(battery, Q_PRESS_H2));
	DBG("Q_PRESS_H1 = 0x%x\n", rk817_bat_field_read(battery, Q_PRESS_L1));
	DBG("Q_PRESS_H0 = 0x%x\n", rk817_bat_field_read(battery, Q_PRESS_L0));

	DBG("capacity = %d\n", capacity);
	return  capacity;
}

static void  fuel_gauge_q_init_info(struct rk817_battery_device *battery)
{
	DBG("Q_INIT_H3 = 0x%x\n", rk817_bat_field_read(battery, Q_INIT_H3));
	DBG("Q_INIT_H2 = 0x%x\n", rk817_bat_field_read(battery, Q_INIT_H2));
	DBG("Q_INIT_L1 = 0x%x\n", rk817_bat_field_read(battery, Q_INIT_L1));
	DBG("Q_INIT_L0 = 0x%x\n", rk817_bat_field_read(battery, Q_INIT_L0));
}

static void rk817_bat_init_coulomb_cap(struct rk817_battery_device *battery,
				       u32 capacity)
{
	u8 buf;
	u32 cap;
	int val;

	fuel_gauge_q_init_info(battery);
	cap = CAPACITY_TO_ADC(capacity, battery->sample_res);
	DBG("new cap: 0x%x\n", cap);
	buf = (cap >> 24) & 0xff;
	rk817_bat_field_write(battery, Q_INIT_H3, buf);
	buf = (cap >> 16) & 0xff;
	rk817_bat_field_write(battery, Q_INIT_H2, buf);
	buf = (cap >> 8) & 0xff;
	rk817_bat_field_write(battery, Q_INIT_L1, buf);
	buf = (cap >> 0) & 0xff;

	val = rk817_bat_field_read(battery, Q_INIT_L0);
	if (val == buf)
		rk817_bat_field_write(battery, Q_INIT_L0, buf + 1);
	else
		rk817_bat_field_write(battery, Q_INIT_L0, buf);

	battery->rsoc = capacity * 1000 * 100 / DIV(battery->fcc);
	battery->remain_cap = capacity * 1000;
	battery->sm_remain_cap = battery->remain_cap;
	DBG("new remaincap: %d\n", battery->remain_cap);
	fuel_gauge_q_init_info(battery);
}

static void rk817_bat_save_cap(struct rk817_battery_device *battery,
			       int capacity)
{
	u8 buf;
	static u32 old_cap;

	if (capacity >= battery->qmax)
		capacity = battery->qmax;
	if (capacity <= 0)
		capacity = 0;
	if (old_cap == capacity)
		return;

	old_cap = capacity;
	buf = (capacity >> 16) & 0xff;
	rk817_bat_field_write(battery, REMAIN_CAP_REG2, buf);
	buf = (capacity >> 8) & 0xff;
	rk817_bat_field_write(battery, REMAIN_CAP_REG1, buf);
	buf = (capacity >> 0) & 0xff;
	rk817_bat_field_write(battery, REMAIN_CAP_REG0, buf);
}

static void rk817_bat_update_qmax(struct rk817_battery_device *battery,
				  u32 capacity)
{
	u8 buf;
	u32 cap_adc;

	cap_adc = CAPACITY_TO_ADC(capacity, battery->sample_res);
	buf = (cap_adc >> 24) & 0xff;
	rk817_bat_field_write(battery, Q_MAX_H3, buf);
	buf = (cap_adc >> 16) & 0xff;
	rk817_bat_field_write(battery, Q_MAX_H2, buf);
	buf = (cap_adc >> 8) & 0xff;
	rk817_bat_field_write(battery, Q_MAX_L1, buf);
	buf = (cap_adc >> 0) & 0xff;
	rk817_bat_field_write(battery, Q_MAX_L0, buf);
	battery->qmax = capacity;
}

static int rk817_bat_get_qmax(struct rk817_battery_device *battery)
{
	u32 capacity;
	int val = 0;

	val = rk817_bat_field_read(battery, Q_MAX_H3) << 24;
	val |= rk817_bat_field_read(battery, Q_MAX_H2) << 16;
	val |= rk817_bat_field_read(battery, Q_MAX_L1) << 8;
	val |= rk817_bat_field_read(battery, Q_MAX_L0) << 0;
	capacity = ADC_TO_CAPACITY(val, battery->sample_res);
	battery->qmax = capacity;
	return capacity;
}

static void rk817_bat_save_fcc(struct rk817_battery_device *battery, int  fcc)
{
	u8 buf;

	buf = (fcc >> 16) & 0xff;
	rk817_bat_field_write(battery, NEW_FCC_REG2, buf);
	buf = (fcc >> 8) & 0xff;
	rk817_bat_field_write(battery, NEW_FCC_REG1, buf);
	buf = (fcc >> 0) & 0xff;
	rk817_bat_field_write(battery, NEW_FCC_REG0, buf);
}

static int rk817_bat_get_fcc(struct rk817_battery_device *battery)
{
	u32 fcc = 0;

	fcc |= rk817_bat_field_read(battery, NEW_FCC_REG2) << 16;
	fcc |= rk817_bat_field_read(battery, NEW_FCC_REG1) << 8;
	fcc |= rk817_bat_field_read(battery, NEW_FCC_REG0) << 0;

	if (fcc < MIN_FCC) {
		DBG("invalid fcc(%d), use design cap", fcc);
		fcc = battery->pdata->design_capacity;
		rk817_bat_save_fcc(battery, fcc);
	} else if (fcc > battery->pdata->design_qmax) {
		DBG("invalid fcc(%d), use qmax", fcc);
		fcc = battery->pdata->design_qmax;
		rk817_bat_save_fcc(battery, fcc);
	}

	return fcc;
}

static int rk817_bat_get_rsoc(struct rk817_battery_device *battery)
{
	int remain_cap;

	remain_cap = rk817_bat_get_capacity_uah(battery);

	return remain_cap * 100 / DIV(battery->fcc);
}

static int rk817_bat_get_off_count(struct rk817_battery_device *battery)
{
	return rk817_bat_field_read(battery, OFF_CNT);
}

static int rk817_bat_get_ocv_count(struct rk817_battery_device *battery)
{
	return rk817_bat_field_read(battery, OCV_CNT);
}

static int rk817_bat_vol2soc(struct rk817_battery_device *battery,
			     int voltage)
{
	u32 *ocv_table, temp;
	int ocv_size, ocv_soc;

	ocv_table = battery->pdata->ocv_table;
	ocv_size = battery->pdata->ocv_size;
	temp = interpolate(voltage, ocv_table, ocv_size);
	ocv_soc = ab_div_c(temp, MAX_PERCENTAGE, MAX_INTERPOLATE);

	return ocv_soc;
}

static int rk817_bat_soc2vol(struct rk817_battery_device *battery, int rsoc)
{
	int ocv_soc;
	u32 *ocv_table;

	ocv_table = battery->pdata->ocv_table;

	ocv_soc = ocv_table[rsoc / OCV_TABLE_STEP];
	ocv_soc += (((ocv_table[rsoc / OCV_TABLE_STEP + 1] - ocv_table[rsoc / OCV_TABLE_STEP]) *
		(rsoc % OCV_TABLE_STEP)) + OCV_TABLE_STEP / 2) / OCV_TABLE_STEP;

	return ocv_soc;
}

static int rk817_bat_vol2cap(struct rk817_battery_device *battery,
			     int voltage)
{
	u32 *ocv_table, temp;
	int ocv_size, capacity;

	ocv_table = battery->pdata->ocv_table;
	ocv_size = battery->pdata->ocv_size;
	temp = interpolate(voltage, ocv_table, ocv_size);
	capacity = ab_div_c(temp, battery->fcc, MAX_INTERPOLATE);

	return capacity;
}

static void rk817_bat_save_dsoc(struct rk817_battery_device *battery,
				int save_soc)
{
	static int last_soc = -1;

	if (last_soc != save_soc) {
		rk817_bat_field_write(battery, SOC_REG0,
				      save_soc & 0xff);
		rk817_bat_field_write(battery, SOC_REG1,
				      (save_soc >> 8) & 0xff);
		rk817_bat_field_write(battery, SOC_REG2,
				      (save_soc >> 16) & 0xff);

		last_soc = save_soc;
	}
}

static int rk817_bat_get_prev_dsoc(struct rk817_battery_device *battery)
{
	int soc_save;

	soc_save = rk817_bat_field_read(battery, SOC_REG0);
	soc_save |= (rk817_bat_field_read(battery, SOC_REG1) << 8);
	soc_save |= (rk817_bat_field_read(battery, SOC_REG2) << 16);

	return soc_save;
}

static bool is_rk817_bat_first_pwron(struct rk817_battery_device *battery)
{
	if (rk817_bat_field_read(battery, BAT_CON)) {
		rk817_bat_field_write(battery, BAT_CON, 0x00);
		return true;
	}

	return false;
}

static int rk817_bat_get_charge_status(struct rk817_battery_device *battery)
{
	int status;

	if (battery->chip_id == RK809_ID) {
		if ((battery->voltage_avg > battery->pdata->design_max_voltage) &&
		    (battery->current_avg > 0) &&
		    ((battery->current_avg < 500) || (battery->rsoc / 1000 == 100)))
			return CHARGE_FINISH;

		if (battery->plugin_trigger)
			return CC_OR_CV_CHRG;
		else
			return CHRG_OFF;
	}
	status = rk817_bat_field_read(battery, CHG_STS);

	if (status == CC_OR_CV_CHRG) {
		if (battery->rsoc == MAX_PERCENTAGE) {
			DBG("charge to finish\n");
			status = CHARGE_FINISH;
		}
	}

	switch (status) {
	case CHRG_OFF:
		DBG("charge off...\n");
		break;
	case DEAD_CHRG:
		DBG("dead charge...\n");
		break;
	case TRICKLE_CHRG:
		DBG("trickle charge...\n");
		break;
	case CC_OR_CV_CHRG:
		DBG("CC or CV charge...\n");
		break;
	case CHARGE_FINISH:
		DBG("charge finish...\n");
		break;
	case USB_OVER_VOL:
		DBG("USB over voltage...\n");
		break;
	case BAT_TMP_ERR:
		DBG("battery temperature error...\n");
		break;
	case BAT_TIM_ERR:
		DBG("battery timer error..\n");
		break;
	default:
		break;
	}

	return status;
}

static void rk817_bat_update_fcc(struct rk817_battery_device *battery)
{
	static int update_status;
	int temp_fcc = 0;

	if (update_status)
		return;

	/* The conditions to update FCC are:
	 * during discharging state,
	 * with temperature above 18°C and
	 * displayed battery level below 1%
	 */
	if ((battery->chrg_status != CHRG_OFF) || (battery->dsoc > 1000) ||
		(battery->temperature < VIRTUAL_TEMPERATURE))
		return;

	/*
	 * Update the FCC to 99.5% of its original value when the aforementioned
	 * basic conditions are met ‌and‌ the loaded voltage falls below
	 * the DTS-configured shutdown voltage.
	 */
	if ((battery->voltage_avg <= battery->pdata->pwroff_vol) &&
	    (battery->rsoc > BATTERY_PERCENTAGE(1))) {
		temp_fcc = UPDATE_REDUCE_FCC(battery->fcc);
		if (temp_fcc > EFFECTIVE_FULL_MIN_CAP(battery->pdata->design_capacity)) {
			DBG("REDUCE: update fcc: design: %d, old: %d, new: %d\n",
			    battery->pdata->design_capacity, battery->fcc, temp_fcc);
			battery->qmax = temp_fcc;
			battery->fcc = temp_fcc;
			rk817_bat_update_qmax(battery, battery->qmax);
			rk817_bat_save_fcc(battery, battery->fcc);
			update_status = 1;
		}
	}

	/* Update the FCC to 100.5% of its original value when the following conditions are met:
	 *
	 * The aforementioned basic conditions are satisfied
	 * (e.g., discharging state, temperature >18°C).
	 * The loaded voltage exceeds the voltage corresponding to 5% SOC in the OCV table.
	 * The RSOC (Relative State of Charge) is below 5%.
	 */
	if ((battery->voltage_avg >= battery->pdata->ocv_table[1]) &&
	    (battery->rsoc < BATTERY_PERCENTAGE(5))) {
		temp_fcc = UPDATE_RAISE_FCC(battery->fcc);
		if (temp_fcc < EFFECTIVE_FULL_MAX_CAP(battery->pdata->design_capacity)) {
			DBG("RAISE fcc: design: %d, old: %d, new: %d\n",
			    battery->pdata->design_capacity, battery->fcc, temp_fcc);
			battery->qmax = temp_fcc;
			battery->fcc = temp_fcc;
			rk817_bat_update_qmax(battery, battery->qmax);
			rk817_bat_save_fcc(battery, battery->fcc);
			update_status = 1;
		}
	}
}

static void rk817_bat_enable_usb2vsys(struct rk817_battery_device *battery)
{
	DBG("enable usb2vsys!!!\n");
	rk817_bat_field_write(battery, USB_SYS_EN, 1);
}

static void rk817_bat_disable_usb2vsys(struct rk817_battery_device *battery)
{
	DBG("disable usb2vsys!!!\n");
	rk817_bat_field_write(battery, USB_SYS_EN, 0);
}

static void rk817_bat_enable_charge(struct rk817_battery_device *battery)
{
	DBG("enable charge by BAT_LTS_TS: 0xFA\n");
	rk817_bat_field_write(battery, BAT_LTS_TS, 0xFA);
}

static void rk817_bat_disable_charge(struct rk817_battery_device *battery)
{
	DBG("disable charge by BAT_LTS_TS: 0x05\n");
	rk817_bat_field_write(battery, BAT_LTS_TS, 0x05);
}

static void rk817_bat_init_ts_detect(struct rk817_battery_device *battery)
{
	if (!battery->pdata->ntc_size)
		return;

	/* the adc of ts1 controlled bit: enable */
	rk817_bat_field_write(battery, TS_ADC_EN, ENABLE);
	/* source current to TS pin */
	rk817_bat_field_write(battery, TS_FUN, TS_FUN_SOURCE_CURRENT);
	/* ts pin flow out current in active state */
	rk817_bat_field_write(battery, VOL_ADC_TSCUR_SEL, FLOW_OUT_20uA);

	battery->pdata->ntc_factor = (FLOW_OUT_20uA + 1) * 10;
	rk817_bat_enable_charge(battery);
}

static void rk817_bat_temperature_chrg(struct rk817_battery_device *battery, int temp)
{
	int i, up_temp, down_temp;
	int now_temp = temp;

	for (i = 0; i < battery->pdata->tc_count; i++) {
		up_temp = battery->pdata->tc_table[i].temp_up;
		down_temp = battery->pdata->tc_table[i].temp_down;

		if (now_temp >= down_temp && now_temp <= up_temp) {
			/* Temp range or charger are not update, return */
			if (battery->charge_index == i)
				return;

			if ((battery->pdata->tc_table[i].chrg_current != 0) &&
			    (battery->pdata->tc_table[i].chrg_current_index != 0xff)) {
				rk817_bat_field_write(battery,
						      CHRG_CUR_SEL,
						      battery->pdata->tc_table[i].chrg_current_index);
				DBG("T change: charger current: %d, index: %d\n",
				    battery->pdata->tc_table[i].chrg_current,
				    battery->pdata->tc_table[i].chrg_current_index);
			} else
				rk817_bat_disable_charge(battery);

			if ((battery->pdata->tc_table[i].chrg_voltage != 0) &&
				(battery->pdata->tc_table[i].chrg_voltage_index != 0xff)) {
				rk817_bat_disable_usb2vsys(battery);
				rk817_bat_field_write(battery,
						      CHRG_VOL_SEL,
						      battery->pdata->tc_table[i].chrg_voltage_index);
				rk817_bat_enable_usb2vsys(battery);
				DBG("T change: charger voltage: %d, index: %d\n",
				    battery->pdata->tc_table[i].chrg_voltage,
				    battery->pdata->tc_table[i].chrg_voltage_index);
			} else
				rk817_bat_enable_charge(battery);

			battery->charge_index = i;
		}
	}
}

static int rk817_bat_get_bat_ts(struct rk817_battery_device *battery)
{
	int temp_value = 0;

	temp_value = rk817_bat_field_read(battery, BAT_TS_H) << 8;
	temp_value |= rk817_bat_field_read(battery, BAT_TS_L);

	return temp_value;
}

static int rk817_bat_get_nts_res(struct rk817_battery_device *battery)
{
	int temp_value, res;
	int adc_to_vol;

	temp_value = rk817_bat_get_bat_ts(battery);
	adc_to_vol = temp_value * 1200 / 65536;

	res = adc_to_vol * 1000 / battery->pdata->ntc_factor;

	DBG("NTC: ADC: value: 0x%x, adc2vol:%d, res: %d\n",
	    temp_value, adc_to_vol, res);

	return res;
}

static void rk817_bat_update_temperature(struct rk817_battery_device *battery)
{
	u32 ntc_size, *ntc_table;
	int i, res;

	ntc_table = battery->pdata->ntc_table;
	ntc_size = battery->pdata->ntc_size;

	if (ntc_size) {
		res = rk817_bat_get_nts_res(battery);
		if (res == 0)
			return;

		if (res < ntc_table[ntc_size - 1]) {
			battery->temperature = (ntc_size + battery->pdata->ntc_degree_from) * 10;
			DBG("bat ntc upper max degree: R=%d\n", res);
		} else if (res > ntc_table[0]) {
			battery->temperature = battery->pdata->ntc_degree_from * 10;
			DBG("bat ntc lower min degree: R=%d\n", res);
		} else {
			for (i = 0; i < ntc_size; i++) {
				if (res >= ntc_table[i])
					break;
			}

			if (i <= 0)
				battery->temperature =
					(battery->pdata->ntc_degree_from) * 10;
			else
				battery->temperature =
					(i + battery->pdata->ntc_degree_from) * 10;
		}
		DBG("Temperature: %d\n", battery->temperature);
		rk817_bat_temperature_chrg(battery, battery->temperature / 10);
	}
}

/*
 * cccv and finish switch all the time will cause dsoc freeze,
 * if so, do finish chrg, 100ma is less than min finish_ma.
 */
static bool rk817_bat_fake_finish_mode(struct rk817_battery_device *battery)
{
	int status;

	status = rk817_bat_get_charge_status(battery);
	if ((battery->rsoc / 1000 == 100) &&
	    (status == CC_OR_CV_CHRG) &&
	    (abs(battery->current_avg) <= 100))
		return true;
	if ((status == CC_OR_CV_CHRG) &&
	    (battery->rsoc > battery->fake_full_soc) && (battery->current_avg > 0))
		return true;

	return false;
}

static bool is_rk817_bat_ocv_valid(struct rk817_battery_device *battery)
{
	return (!battery->is_initialized && battery->pwroff_min >= 30);
}

static void rk817_bat_gas_gaugle_enable(struct rk817_battery_device *battery)
{
	rk817_bat_field_write(battery, GG_EN, ENABLE);
}

static void rk817_bat_gg_con_init(struct rk817_battery_device *battery)
{
	rk817_bat_field_write(battery, RLX_SPT, S_8_MIN);
	rk817_bat_field_write(battery, ADC_OFF_CAL_INTERV, S_8_MIN);
	rk817_bat_field_write(battery, VOL_OUT_MOD, AVERAGE_MODE);
	rk817_bat_field_write(battery, CUR_OUT_MOD, AVERAGE_MODE);
}

static void rk817_bat_adc_init(struct rk817_battery_device *battery)
{
	rk817_bat_field_write(battery, SYS_VOL_ADC_EN, ENABLE);
	rk817_bat_field_write(battery, TS_ADC_EN, ENABLE);
	rk817_bat_field_write(battery, USB_VOL_ADC_EN, ENABLE);
	rk817_bat_field_write(battery, BAT_VOL_ADC_EN, ENABLE);
	rk817_bat_field_write(battery, BAT_CUR_ADC_EN, ENABLE);
}

static void rk817_bat_init_info(struct rk817_battery_device *battery)
{
	battery->design_cap = battery->pdata->design_capacity;
	battery->qmax = battery->pdata->design_qmax;
	battery->bat_res = battery->pdata->bat_res;
	battery->monitor_ms = battery->pdata->monitor_sec * TIMER_MS_COUNTS;
	battery->sample_res = battery->pdata->sample_res;
	battery->fake_full_soc = battery->pdata->fake_full_soc * 1000;
	DBG("battery->qmax :%d\n", battery->qmax);
}

static int rk817_bat_get_prev_cap(struct rk817_battery_device *battery)
{
	int val = 0;

	val = rk817_bat_field_read(battery, REMAIN_CAP_REG2) << 16;
	val |= rk817_bat_field_read(battery, REMAIN_CAP_REG1) << 8;
	val |= rk817_bat_field_read(battery, REMAIN_CAP_REG0) << 0;

	return val;
}

static u8 rk817_bat_get_halt_cnt(struct rk817_battery_device *battery)
{
	return rk817_bat_field_read(battery, HALT_CNT_REG);
}

static void rk817_bat_inc_halt_cnt(struct rk817_battery_device *battery)
{
	u8 cnt;

	cnt =  rk817_bat_field_read(battery, HALT_CNT_REG);
	rk817_bat_field_write(battery, HALT_CNT_REG, ++cnt);
}

static bool is_rk817_bat_last_halt(struct rk817_battery_device *battery)
{
	int pre_cap = rk817_bat_get_prev_cap(battery);
	int now_cap = rk817_bat_get_capacity_mah(battery);

	/* over 10%: system halt last time */
	if (abs(now_cap - pre_cap) > (battery->fcc / 10)) {
		rk817_bat_inc_halt_cnt(battery);
		return true;
	} else {
		return false;
	}
}

static u8 is_rk817_bat_initialized(struct rk817_battery_device *battery)
{
	u8 val = rk817_bat_field_read(battery, FG_INIT);

	if (val) {
		rk817_bat_field_write(battery, FG_INIT, 0x00);
		return true;
	} else {
		return false;
	}
}

static void rk817_bat_calc_sm_linek(struct rk817_battery_device *battery)
{
	long expected_voltage, expected_res2voltage;
	int expected_rsoc;
	int current_avg;
	int soc2vol;
	int status;
	int linek;

	current_avg = rk817_bat_get_avg_current(battery);
	soc2vol = rk817_bat_soc2vol(battery, battery->rsoc);
	expected_voltage = battery->pdata->pwroff_vol +
		soc2vol * (soc2vol - battery->voltage_avg) / battery->pdata->pwroff_vol;

	expected_res2voltage = battery->pdata->pwroff_vol +
		(soc2vol * abs(current_avg) * battery->bat_res) / battery->pdata->pwroff_vol / 1000;

	DBG("expected_voltage: %ld, expected_res2voltage: %ld\n",
	    expected_voltage, expected_res2voltage);

	expected_voltage =
		(expected_voltage > expected_res2voltage) ? expected_voltage : expected_res2voltage;
	DBG("expected_voltage: %ld\n", expected_voltage);

	expected_rsoc = rk817_bat_vol2soc(battery, expected_voltage);
	battery->delta_rsoc = expected_rsoc;

	DBG("expected_voltage: %ld, RSOC: %d expected_rsoc: %d delta_rsoc: %d\n",
	    expected_voltage, battery->rsoc, expected_rsoc, battery->delta_rsoc);

	status = rk817_bat_get_charge_status(battery);
	if ((status == CHRG_OFF) || ((status == CC_OR_CV_CHRG) && (current_avg < 0)) ||
	    ((status == CHARGE_FINISH) && (current_avg < FINISH_CURR_THRESD))) {
		/* When the discharge current is less than 30A and the charging IC reports a
		 * full charge status, the system will determine that the current operation is
		 *in discharge mode.
		 */
		linek = -(MAX_PERCENTAGE - battery->rsoc + battery->dsoc) * 1000;
		linek /= (MAX_PERCENTAGE - battery->delta_rsoc);
	} else {
		linek = MAX_PERCENTAGE * 1000 / (MAX_PERCENTAGE - battery->rsoc + battery->dsoc);
	}
	DBG("expected_voltage %ld expected_rsoc: %d\n", expected_voltage, expected_rsoc);
	DBG("ocv_voltage %d sd_ocv_voltage: %ld, linek: %d\n", soc2vol, expected_voltage, linek);

	battery->expected_voltage = expected_voltage;
	battery->sm_linek = linek;
	battery->dbg_calc_dsoc = battery->dsoc;
	battery->dbg_calc_rsoc = battery->rsoc;
}

static void rk817_bat_smooth_algo_prepare(struct rk817_battery_device *battery)
{
	battery->smooth_soc = battery->dsoc;
	battery->sm_remain_cap = battery->remain_cap;
	DBG("<%s>. dsoc=%d, dsoc:smooth_soc=%d\n",
	    __func__, battery->dsoc, battery->smooth_soc);
	rk817_bat_calc_sm_linek(battery);
}

static void rk817_bat_finish_algo_prepare(struct rk817_battery_device *battery)
{
	battery->finish_base = get_boot_sec();

	if (!battery->finish_base)
		battery->finish_base = 1;
}

static void rk817_bat_init_dsoc_algorithm(struct rk817_battery_device *battery)
{
	if (battery->dsoc >= MAX_PERCENTAGE)
		battery->dsoc = MAX_PERCENTAGE;
	else if (battery->dsoc <= 0)
		battery->dsoc = 0;
	/* init current mode */
	battery->voltage_avg = rk817_bat_get_battery_voltage(battery);
	battery->current_avg = rk817_bat_get_avg_current(battery);

	if (rk817_bat_get_charge_status(battery) == CHARGE_FINISH) {
		rk817_bat_finish_algo_prepare(battery);
		battery->work_mode = MODE_FINISH;
	} else {
		rk817_bat_smooth_algo_prepare(battery);
		battery->work_mode = MODE_SMOOTH;
	}
	DBG("%s, sm_remain_cap = %d, smooth_soc = %d\n",
	    __func__, battery->sm_remain_cap, battery->smooth_soc);
}

static void rk817_bat_first_pwron(struct rk817_battery_device *battery)
{
	battery->rsoc =
		rk817_bat_vol2soc(battery,
				  battery->pwron_voltage);/* uAH */
	battery->dsoc = battery->rsoc;
	battery->fcc = battery->pdata->design_capacity;
	if (battery->fcc < MIN_FCC)
		battery->fcc = MIN_FCC;

	battery->nac = rk817_bat_vol2cap(battery, battery->pwron_voltage);
	battery->fcc = battery->pdata->design_capacity;

	rk817_bat_update_qmax(battery, battery->qmax);
	rk817_bat_save_fcc(battery, battery->fcc);
	DBG("%s, rsoc = %d, dsoc = %d, fcc = %d, nac = %d\n",
	    __func__, battery->rsoc, battery->dsoc, battery->fcc, battery->nac);
}

static void rk817_bat_not_first_pwron(struct rk817_battery_device *battery)
{
	int now_cap, pre_soc, pre_cap;

	battery->fcc = rk817_bat_get_fcc(battery);
	pre_soc = rk817_bat_get_prev_dsoc(battery);
	pre_cap = rk817_bat_get_prev_cap(battery);
	now_cap = rk817_bat_get_capacity_mah(battery);
	battery->remain_cap = pre_cap * 1000;
	battery->is_halt = is_rk817_bat_last_halt(battery);
	battery->halt_cnt = rk817_bat_get_halt_cnt(battery);
	battery->is_initialized = is_rk817_bat_initialized(battery);
	battery->is_ocv_calib = is_rk817_bat_ocv_valid(battery);

	if (battery->is_halt) {
		BAT_INFO("system halt last time... cap: pre=%d, now=%d\n",
			 pre_cap, now_cap);
		if (now_cap < 0)
			now_cap = 0;
		rk817_bat_init_coulomb_cap(battery, now_cap);
		pre_cap = now_cap;
		pre_soc = battery->rsoc;
	} else if (battery->is_initialized) {
		/* uboot initialized */
		BAT_INFO("initialized yet..\n");
	}

	battery->dsoc = pre_soc;
	battery->nac = pre_cap;
	if (battery->nac < 0)
		battery->nac = 0;

	DBG("dsoc=%d cap=%d v=%d ov=%d rv=%d min=%d psoc=%d pcap=%d\n",
	    battery->dsoc, battery->nac, rk817_bat_get_battery_voltage(battery),
	    rk817_bat_get_ocv_voltage(battery),
	    rk817_bat_get_relax_voltage(battery),
	    battery->pwroff_min, rk817_bat_get_prev_dsoc(battery),
	    rk817_bat_get_prev_cap(battery));
}

static void rk817_bat_rsoc_init(struct rk817_battery_device *battery)
{
	battery->is_first_power_on = is_rk817_bat_first_pwron(battery);
	battery->pwroff_min = rk817_bat_get_off_count(battery);
	battery->pwron_voltage = rk817_bat_get_pwron_voltage(battery);

	DBG("%s, is_first_power_on = %d, pwroff_min = %d, pwron_voltage = %d\n",
	    __func__, battery->is_first_power_on,
	    battery->pwroff_min, battery->pwron_voltage);

	if (battery->is_first_power_on)
		rk817_bat_first_pwron(battery);
	else
		rk817_bat_not_first_pwron(battery);

	 rk817_bat_save_dsoc(battery, battery->dsoc);
}

static void rk817_bat_caltimer_isr(struct timer_list *t)
{
	struct rk817_battery_device *battery =
		from_timer(battery, t, caltimer);

	mod_timer(&battery->caltimer, jiffies + MINUTE(8) * HZ);
	queue_delayed_work(battery->bat_monitor_wq,
			   &battery->calib_delay_work,
			   msecs_to_jiffies(10));
}

static void rk817_bat_internal_calib(struct work_struct *work)
{
	struct rk817_battery_device *battery = container_of(work,
			struct rk817_battery_device, calib_delay_work.work);

	return;

	rk817_bat_current_calibration(battery);
	/* calib voltage kb */
	rk817_bat_init_voltage_kb(battery);

	DBG("caltimer:coffset=0x%x\n", rk817_bat_get_coffset(battery));
}

static void rk817_bat_init_caltimer(struct rk817_battery_device *battery)
{
	timer_setup(&battery->caltimer,
		    rk817_bat_caltimer_isr,
		    0);
	battery->caltimer.expires = jiffies + MINUTE(8) * HZ;
	add_timer(&battery->caltimer);
	INIT_DELAYED_WORK(&battery->calib_delay_work, rk817_bat_internal_calib);
}

static void rk817_bat_init_fg(struct rk817_battery_device *battery)
{
	rk817_bat_adc_init(battery);
	rk817_bat_gas_gaugle_enable(battery);
	rk817_bat_gg_con_init(battery);
	rk817_bat_init_voltage_kb(battery);
	rk817_bat_set_relax_sample(battery);
	rk817_bat_init_caltimer(battery);
	rk817_bat_rsoc_init(battery);
	rk817_bat_init_coulomb_cap(battery, battery->nac);
	rk817_bat_init_ts_detect(battery);
	DBG("rsoc%d, fcc = %d\n", battery->rsoc, battery->fcc);
	rk817_bat_init_dsoc_algorithm(battery);
	battery->qmax = rk817_bat_get_qmax(battery);
	battery->voltage_avg = rk817_bat_get_battery_voltage(battery);
	battery->voltage_sys = rk817_bat_get_sys_voltage(battery);

	battery->voltage_ocv = rk817_bat_get_ocv_voltage(battery);
	battery->voltage_relax = rk817_bat_get_relax_voltage(battery);
	battery->current_avg = rk817_bat_get_avg_current(battery);
	battery->dbg_pwr_dsoc = battery->dsoc;
	battery->dbg_pwr_rsoc = battery->rsoc;
	battery->dbg_pwr_vol = battery->voltage_avg;
	battery->temperature = VIRTUAL_TEMPERATURE;

	DBG("probe init: battery->dsoc = %d, rsoc = %d, remain_cap = %d\n, bat_vol = %d\n, sys_vol = %d, qmax = %d\n",
	    battery->dsoc, battery->rsoc, battery->remain_cap,
	    battery->voltage_avg, battery->voltage_sys, battery->qmax);
}

static u8 rk817_bat_decode_chrg_voltage(u32 chrg_voltage)
{
	if (chrg_voltage == 0)
		return 0xff;

	if (chrg_voltage < 4150)
		return CHRG_VOL_4100MV;
	else if (chrg_voltage < 4200)
		return CHRG_VOL_4100MV;
	else if (chrg_voltage < 4250)
		return CHRG_VOL_4200MV;
	else if (chrg_voltage < 4300)
		return CHRG_VOL_4250MV;
	else if (chrg_voltage < 4350)
		return CHRG_VOL_4300MV;
	else if (chrg_voltage < 4400)
		return CHRG_VOL_4350MV;
	else if (chrg_voltage < 4450)
		return CHRG_VOL_4400MV;
	else
		return CHRG_VOL_4450MV;
}

static u8 rk817_bat_decode_chrg_current(struct rk817_battery_device *battery,
					u32 chrg_current)
{
	int val;

	if (chrg_current == 0)
		return 0xff;

	val = chrg_current * battery->pdata->sample_res / 10;
	if (val < 1000)
		return CHRG_CUR_500MA;
	else if (val < 1500)
		return CHRG_CUR_1000MA;
	else if (val < 2000)
		return CHRG_CUR_1500MA;
	else if (val < 2500)
		return CHRG_CUR_2000MA;
	else if (val < 2750)
		return CHRG_CUR_2500MA;
	else if (val < 3000)
		return CHRG_CUR_2750MA;
	else if (val < 3500)
		return CHRG_CUR_3000MA;
	else
		return CHRG_CUR_3500MA;
}

static int parse_temperature_chrg_table(struct rk817_battery_device *battery,
					struct device_node *np)
{
	int size, count;
	int i, chrg_current, chrg_voltage;
	const __be32 *list;

	if (!of_find_property(np, "temperature_chrg_table", &size))
		return 0;

	list = of_get_property(np, "temperature_chrg_table", &size);
	size /= sizeof(u32);
	if (!size || (size % 3)) {
		dev_err(battery->dev,
			"invalid temperature_chrg_table: size=%d\n", size);
		return -EINVAL;
	}

	count = size / 4;
	battery->pdata->tc_count = count;
	battery->pdata->tc_table = devm_kzalloc(battery->dev,
						count * sizeof(*battery->pdata->tc_table),
						GFP_KERNEL);
	if (!battery->pdata->tc_table)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		/* temperature */
		battery->pdata->tc_table[i].temp_down = be32_to_cpu(*list++);
		battery->pdata->tc_table[i].temp_up = be32_to_cpu(*list++);
		chrg_current = be32_to_cpu(*list++);
		chrg_voltage = be32_to_cpu(*list++);

		/*
		 * because charge current lowest level is 500mA:
		 * higher than or equal 1000ma, select charge current;
		 * lower than 500ma, must select input current.
		 */
		if (chrg_current >= 500) {
			battery->pdata->tc_table[i].chrg_current = chrg_current;
			battery->pdata->tc_table[i].chrg_current_index = rk817_bat_decode_chrg_current(battery, chrg_current);
		} else {
			battery->pdata->tc_table[i].chrg_current = 0;
		}

		if (chrg_voltage >= 4100) {
			battery->pdata->tc_table[i].chrg_voltage = chrg_voltage;
			battery->pdata->tc_table[i].chrg_voltage_index = rk817_bat_decode_chrg_voltage(chrg_voltage);
		} else {
			battery->pdata->tc_table[i].chrg_voltage = 0;
		}
		DBG("temp%d: [%d, %d], chrg_current=%d, current_index: %d, chrg_voltage: %d, voltage_index: %d\n",
		    i, battery->pdata->tc_table[i].temp_down,
		    battery->pdata->tc_table[i].temp_up,
		    battery->pdata->tc_table[i].chrg_current,
		    battery->pdata->tc_table[i].chrg_current_index,
		    battery->pdata->tc_table[i].chrg_voltage,
		    battery->pdata->tc_table[i].chrg_voltage_index);
	}

	return 0;
}

static int rk817_bat_parse_dt(struct rk817_battery_device *battery)
{
	u32 out_value;
	int length, ret;
	size_t size;
	struct battery_platform_data *pdata;
	struct device *dev = battery->dev;
	struct device_node *np = battery->dev->of_node;

	pdata = devm_kzalloc(battery->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	battery->pdata = pdata;
	/* init default param */
	pdata->bat_res = DEFAULT_BAT_RES;
	pdata->monitor_sec = DEFAULT_MONITOR_SEC;
	pdata->pwroff_vol = DEFAULT_PWROFF_VOL_THRESD;
	pdata->sleep_exit_current = DEFAULT_SLP_EXIT_CUR;
	pdata->sleep_enter_current = DEFAULT_SLP_ENTER_CUR;

	pdata->sleep_filter_current = DEFAULT_SLP_FILTER_CUR;
	pdata->bat_mode = MODE_BATTARY;
	pdata->fake_full_soc = 100;
	pdata->sample_res = DEFAULT_SAMPLE_RES;
	pdata->charge_stay_awake = 0;

	/* parse necessary param */
	if (!of_find_property(np, "ocv_table", &length)) {
		dev_err(dev, "ocv_table not found!\n");
		return -EINVAL;
	}

	pdata->ocv_size = length / sizeof(u32);
	if (pdata->ocv_size < 2) {
		dev_err(dev, "invalid ocv table\n");
		return -EINVAL;
	}

	size = sizeof(*pdata->ocv_table) * pdata->ocv_size;
	pdata->ocv_table = devm_kzalloc(battery->dev, size, GFP_KERNEL);
	if (!pdata->ocv_table)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "ocv_table", pdata->ocv_table,
					 pdata->ocv_size);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32(np, "design_capacity", &out_value);
	if (ret < 0) {
		dev_err(dev, "design_capacity not found!\n");
		return ret;
	}
	pdata->design_capacity = out_value;

	ret = of_property_read_u32(np, "design_qmax", &out_value);
	if (ret < 0) {
		dev_err(dev, "design_qmax not found!\n");
		return ret;
	}
	pdata->design_qmax = out_value;

	/* parse unnecessary param */
	ret = of_property_read_u32(np, "sample_res", &pdata->sample_res);
	if (ret < 0)
		dev_err(dev, "sample_res missing!\n");

	ret = of_property_read_u32(np, "monitor_sec", &pdata->monitor_sec);
	if (ret < 0)
		dev_err(dev, "monitor_sec missing!\n");

	ret = of_property_read_u32(np, "virtual_power", &pdata->bat_mode);
	if (ret < 0)
		dev_err(dev, "virtual_power missing!\n");

	ret = of_property_read_u32(np, "bat_res", &pdata->bat_res);
	if (ret < 0)
		dev_err(dev, "bat_res missing!\n");

	ret = of_property_read_u32(np, "sleep_enter_current",
				   &pdata->sleep_enter_current);
	if (ret < 0)
		dev_err(dev, "sleep_enter_current missing!\n");

	ret = of_property_read_u32(np, "sleep_exit_current",
				   &pdata->sleep_exit_current);
	if (ret < 0)
		dev_err(dev, "sleep_exit_current missing!\n");

	ret = of_property_read_u32(np, "sleep_filter_current",
				   &pdata->sleep_filter_current);
	if (ret < 0)
		dev_err(dev, "sleep_filter_current missing!\n");

	ret = of_property_read_u32(np, "power_off_thresd", &pdata->pwroff_vol);
	if (ret < 0)
		dev_err(dev, "power_off_thresd missing!\n");

	ret = of_property_read_u32(np, "charge_stay_awake", &pdata->charge_stay_awake);
	if (ret < 0)
		dev_info(dev, "charge_stay_awake missing!\n");

	ret = of_property_read_u32(np, "fake_full_soc", &pdata->fake_full_soc);
	if (ret < 0)
		dev_info(dev, "fake_full_soc missing!\n");
	else {
		if ((pdata->fake_full_soc > 100) || (pdata->fake_full_soc < 0))
			pdata->fake_full_soc = 100;
	}

	if (battery->chip_id == RK809_ID) {
		ret = of_property_read_u32(np, "bat_res_up",
					   &pdata->bat_res_up);
		if (ret < 0)
			dev_err(dev, "battery res_up missing\n");

		ret = of_property_read_u32(np, "bat_res_down",
					   &pdata->bat_res_down);
		if (ret < 0)
			dev_err(dev, "battery res_down missing!\n");

		ret = of_property_read_u32(np, "design_max_voltage",
					   &pdata->design_max_voltage);
		if (ret < 0) {
			dev_err(dev, "battery design_max_voltage missing!\n");
			pdata->design_max_voltage = pdata->ocv_table[pdata->ocv_size - 1];
		}
		ret = of_property_read_u32(np, "register_chg_psy",
					   &battery->is_register_chg_psy);
		if (ret < 0 || !battery->is_register_chg_psy)
			dev_err(dev, "not have to register chg psy!\n");
	}

	if (!of_find_property(np, "ntc_table", &length)) {
		pdata->ntc_size = 0;
		battery->temperature = VIRTUAL_TEMPERATURE;
	} else {
		/* get ntc degree base value */
		ret = of_property_read_u32_index(np, "ntc_degree_from", 1,
				&pdata->ntc_degree_from);
		if (ret) {
			dev_err(dev, "invalid ntc_degree_from\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_index(np, "ntc_degree_from", 0,
						 &out_value);
		if (ret) {
			dev_err(dev, "invalid ntc_degree_from\n");
			return -EINVAL;
		}

		if (out_value)
			pdata->ntc_degree_from = -pdata->ntc_degree_from;

		pdata->ntc_size = length / sizeof(u32);
	}

	if (pdata->ntc_size) {
		parse_temperature_chrg_table(battery, np);
		size = sizeof(*pdata->ntc_table) * pdata->ntc_size;
		pdata->ntc_table = devm_kzalloc(battery->dev, size, GFP_KERNEL);
		if (!pdata->ntc_table)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, "ntc_table",
				pdata->ntc_table,
				pdata->ntc_size);
		if (ret < 0)
			return ret;
	}

	DBG("the battery dts info dump:\n"
	    "bat_res:%d\n"
	    "res_sample:%d\n"
	    "design_capacity:%d\n"
	    "design_qmax :%d\n"
	    "sleep_enter_current:%d\n"
	    "sleep_exit_current:%d\n"
	    "sleep_filter_current:%d\n"
	    "monitor_sec:%d\n"
	    "virtual_power:%d\n"
	    "pwroff_vol:%d\n",
	    pdata->bat_res,
	    pdata->sample_res,
	    pdata->design_capacity,
	    pdata->design_qmax,
	    pdata->sleep_enter_current,
	    pdata->sleep_exit_current,
	    pdata->sleep_filter_current,
	    pdata->monitor_sec,
	    pdata->bat_mode,
	    pdata->pwroff_vol);

	return 0;
}

static enum power_supply_property rk817_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

static int rk817_get_capacity_leve(struct rk817_battery_device *battery)
{
	int dsoc;

	if (battery->pdata->bat_mode == MODE_VIRTUAL)
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;

	dsoc = (battery->dsoc + 500) / 1000;
	if (dsoc < 1)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else if (dsoc <= 20)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (dsoc <= 70)
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (dsoc <= 90)
		return POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
}

static int rk817_battery_time_to_full(struct rk817_battery_device *battery)
{
	int time_sec;
	int cap_temp;

	if (battery->pdata->bat_mode == MODE_VIRTUAL) {
		time_sec = 3600;
	} else if (battery->voltage_avg > 0) {
		cap_temp = battery->design_cap - (battery->remain_cap / 1000);
		if (cap_temp < 0)
			cap_temp = 0;
		time_sec = (3600 * cap_temp) / battery->voltage_avg;
	} else {
		time_sec = 3600 * 24; /* One day */
	}

	return time_sec;
}

static int rk817_battery_get_property(struct power_supply *psy,
				      enum power_supply_property psp,
				      union power_supply_propval *val)
{
	struct rk817_battery_device *battery = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = battery->current_avg * 1000;/*uA*/
		if (battery->pdata->bat_mode == MODE_VIRTUAL)
			val->intval = VIRTUAL_CURRENT * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery->voltage_avg * 1000;/*uV*/
		if (battery->pdata->bat_mode == MODE_VIRTUAL)
			val->intval = VIRTUAL_VOLTAGE * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = (battery->dsoc + 400) / 1000;
		if (battery->pdata->bat_mode == MODE_VIRTUAL)
			val->intval = VIRTUAL_SOC;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = rk817_get_capacity_leve(battery);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->temperature;
		if (battery->pdata->bat_mode == MODE_VIRTUAL)
			val->intval = VIRTUAL_TEMPERATURE;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (battery->pdata->bat_mode == MODE_VIRTUAL)
			val->intval = VIRTUAL_STATUS;
		else if (battery->dsoc == MAX_PERCENTAGE)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else {
			if ((battery->chip_id != RK809_ID) &&
			    power_supply_is_system_supplied())
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else if (battery->chip_id == RK809_ID &&
				 battery->plugin_trigger)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = battery->pdata->design_capacity * 1000;/* uAh */
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = rk817_battery_time_to_full(battery);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 4500 * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 5000 * 1000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct power_supply_desc rk817_bat_desc = {
	.name		= "battery",
	.type		= POWER_SUPPLY_TYPE_BATTERY,
	.properties	= rk817_bat_props,
	.num_properties	= ARRAY_SIZE(rk817_bat_props),
	.get_property	= rk817_battery_get_property,
};

static int rk817_bat_init_power_supply(struct rk817_battery_device *battery)
{
	struct power_supply_config psy_cfg = { .drv_data = battery, };

	battery->bat = devm_power_supply_register(battery->dev,
						  &rk817_bat_desc,
						  &psy_cfg);
	if (IS_ERR(battery->bat)) {
		dev_err(battery->dev, "register bat power supply fail\n");
		return PTR_ERR(battery->bat);
	}

	return 0;
}

static enum power_supply_property rk809_chg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
};

static int rk809_chg_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct rk817_battery_device *battery = power_supply_get_drvdata(psy);
	int online = 0;
	int ret = 0;

	if (battery->plugin_trigger)
		online = 1;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = online;
		dev_dbg(battery->dev, "report online: %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (online)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		dev_dbg(battery->dev, "report prop: %d\n", val->intval);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct power_supply_desc rk809_chg_desc = {
	.name		= "charger",
	.type		= POWER_SUPPLY_TYPE_USB,
	.properties	= rk809_chg_props,
	.num_properties	= ARRAY_SIZE(rk809_chg_props),
	.get_property	= rk809_chg_get_property,
};

static int rk809_chg_init_power_supply(struct rk817_battery_device *battery)
{
	struct power_supply_config psy_cfg = { .drv_data = battery, };

	battery->chg_psy =
		devm_power_supply_register(battery->dev, &rk809_chg_desc,
					   &psy_cfg);
	if (IS_ERR(battery->chg_psy)) {
		dev_err(battery->dev, "register chg psy power supply fail\n");
		return PTR_ERR(battery->chg_psy);
	}

	return 0;
}

static void rk817_bat_power_supply_changed(struct rk817_battery_device *battery)
{
	static int old_soc = -1;
	static int status = CHRG_OFF;

	if (battery->dsoc > MAX_PERCENTAGE)
		battery->dsoc = MAX_PERCENTAGE;
	else if (battery->dsoc < 0)
		battery->dsoc = 0;

	if ((battery->dsoc == old_soc) && (battery->chrg_status == status))
		return;

	/* Battery status change, report information */
	status = battery->chrg_status;
	old_soc = battery->dsoc;
	power_supply_changed(battery->bat);
	DBG("changed: dsoc=%d, rsoc=%d, v=%d, ov=%d c=%d, cap=%d, f=%d\n",
	    battery->dsoc, battery->rsoc, battery->voltage_avg,
	    battery->voltage_ocv, battery->current_avg,
	    battery->remain_cap, battery->fcc);
}

static void rk817_battery_debug_info(struct rk817_battery_device *battery)
{
	rk817_bat_get_battery_voltage(battery);
	rk817_bat_get_sys_voltage(battery);
	rk817_bat_get_USB_voltage(battery);
	rk817_bat_get_pwron_voltage(battery);
	rk817_bat_get_ocv_voltage(battery);
	rk817_bat_get_ocv0_voltage0(battery);

	rk817_bat_current_calibration(battery);
	rk817_bat_get_avg_current(battery);
	rk817_bat_get_relax_cur1(battery);
	rk817_bat_get_relax_cur2(battery);
	rk817_bat_get_relax_current(battery);
	rk817_bat_get_ocv_current(battery);
	rk817_bat_get_ocv_current0(battery);
	rk817_bat_get_pwron_current(battery);
	rk817_bat_get_ocv_count(battery);
	rk817_bat_save_dsoc(battery, battery->dsoc);
	DBG("capactiy = %d\n", rk817_bat_get_capacity_mah(battery));
}

static void rk817_bat_update_fg_info(struct rk817_battery_device *battery)
{
	battery->voltage_avg = rk817_bat_get_battery_voltage(battery);
	battery->voltage_sys = rk817_bat_get_sys_voltage(battery);
	battery->current_avg = rk817_bat_get_avg_current(battery);
	battery->voltage_relax = rk817_bat_get_relax_voltage(battery);
	battery->rsoc = rk817_bat_get_rsoc(battery);
	battery->remain_cap = rk817_bat_get_capacity_uah(battery);
	battery->voltage_usb = rk817_bat_get_USB_voltage(battery);
	battery->chrg_status = rk817_bat_get_charge_status(battery);
	DBG("valtage usb: %d\n", battery->voltage_usb);
	DBG("UPDATE: voltage_avg = %d\n"
	    "voltage_sys = %d\n"
	    "curren_avg = %d\n"
	    "rsoc = %d\n"
	    "chrg_status = %d\n"
	    "PWRON_CUR = %d\n"
	    "remain_cap = %d\n",
	    battery->voltage_avg,
	    battery->voltage_sys,
	    battery->current_avg,
	    battery->rsoc,
	    battery->chrg_status,
	    rk817_bat_get_pwron_current(battery),
	    battery->remain_cap);

	/* smooth charge */
	if (battery->remain_cap / 1000 > battery->fcc) {
		battery->sm_remain_cap = battery->fcc * 1000;
		DBG("<%s>. cap: remain=%d, sm_remain=%d\n",
		    __func__, battery->remain_cap, battery->sm_remain_cap);
		DBG("fcc: %d\n", battery->fcc);
		rk817_bat_init_coulomb_cap(battery, battery->fcc);
		rk817_bat_get_capacity_mah(battery);
	}

	/* (battery->chrg_status != CHARGE_FINISH) */
	if (battery->chrg_status == CC_OR_CV_CHRG)
		battery->finish_base = get_boot_sec();
}

static void rk817_bat_save_data(struct rk817_battery_device *battery)
{
	rk817_bat_save_dsoc(battery, battery->dsoc);
	rk817_bat_save_cap(battery, battery->remain_cap / 1000);
}

/* high load: current < 0 with charger in.
 * System will not shutdown while dsoc=0% with charging state(ac_in),
 * which will cause over discharge, so oppose status before report states.
 */
static void rk817_bat_lowpwr_check(struct rk817_battery_device *battery)
{
	static u64 time;
	int pwr_off_thresd = battery->pdata->pwroff_vol;

	if (battery->current_avg < 0 && battery->voltage_avg < pwr_off_thresd) {
		if (!time)
			time = get_boot_sec();

		if ((base2sec(time) > MINUTE(1)) ||
		    (battery->voltage_avg <= pwr_off_thresd - 50)) {
			battery->fake_offline = 1;
			if (battery->voltage_avg <= pwr_off_thresd - 50)
				battery->dsoc -= 1000;
			DBG("low power, soc=%d, current=%d\n",
			    battery->dsoc, battery->current_avg);
		}
	} else {
		time = 0;
		battery->fake_offline = 0;
	}

	DBG("<%s>. t=%lu, dsoc=%d, current=%d, fake_offline=%d\n",
	    __func__, base2sec(time), battery->dsoc,
	    battery->current_avg, battery->fake_offline);
}

static void rk817_bat_update_soc(struct rk817_battery_device *battery, int delta_soc)
{
	int tmp_soc = 0;

	battery->smooth_soc += delta_soc;

	/* check new dsoc */
	if (battery->smooth_soc < 0)
		battery->smooth_soc = 0;

	tmp_soc = battery->smooth_soc / 1000;

	/* Mitigate step transitions in displayed SOC (State of Charge) during charging */
	if (tmp_soc != battery->dsoc / 1000) {
		/* During charging, the battery state of charge (SOC) will decrease if the
		 * charger's input power is lower than the system power consumption
		 */
		if (delta_soc < 0) {
			if (battery->smooth_soc > battery->dsoc)
				return;
			if (battery->smooth_soc + 1000 > battery->dsoc)
				battery->dsoc = battery->smooth_soc;
			else
				battery->dsoc -= 1000;
		} else {
			if (battery->smooth_soc < battery->dsoc)
				return;
			if (battery->smooth_soc < battery->dsoc + 1000)
				battery->dsoc = battery->smooth_soc;
			else
				battery->dsoc += 1000;
		}
	} else {
		/* Control of SOC fluctuation magnitude during normal operation */
		battery->dsoc = battery->smooth_soc;
	}

	if (battery->dsoc <= 0)
		battery->dsoc = 0;
}

static void rk817_bat_smooth_algorithm(struct rk817_battery_device *battery)
{
	int delta_cap = 0, old_cap = 0;
	long cap_change;
	long ydsoc = 0;

	/* charge and discharge switch */
	if ((battery->sm_linek * battery->current_avg <= 0)) {
		DBG("<%s>. linek mode, retinit sm linek..\n", __func__);
		rk817_bat_calc_sm_linek(battery);
	}

	battery->remain_cap = rk817_bat_get_capacity_uah(battery);
	old_cap = battery->sm_remain_cap;

	DBG("smooth: smooth_soc = %d, dsoc = %d, battery->sm_linek = %d\n",
	    battery->smooth_soc, battery->dsoc, battery->sm_linek);

	/* discharge status: sm_remain_cap > remain_cap, delta_cap > 0
	 * from charge to discharge:
	 * remain_cap may be above sm_remain_cap, delta_cap <= 0
	 */
	delta_cap = battery->remain_cap - battery->sm_remain_cap;

	DBG("smooth: sm_remain_cap: %d, remain_cap: %d, delta_cap: %d\n",
	    battery->sm_remain_cap, battery->remain_cap, delta_cap);
	if (delta_cap == 0) {
		DBG("<%s>. delta_cap = 0\n", __func__);
		return;
	}

	/* discharge: sm_linek < 0, if delate_cap <0, ydsoc > 0
	 * captosoc: delta_cap * MAX_PERCENTAGE / (DIV(battery->fcc) * 1000)
	 * delta_cap * 100 / DIV(battery->fcc)
	 * captosoc Remainder: delta_cap * MAX_PERCENTAGE % (DIV(battery->fcc) * 1000)
	 * delta_cap * 100 % DIV(battery->fcc)
	 * delta_cap += battery->delta_cap_remainder;
	 * ydsoc = battery->sm_linek * abs(delta_cap) / (10 * DIV(battery->fcc));
	 */
	cap_change = battery->sm_linek * abs(delta_cap) + battery->delta_cap_remainder;
	ydsoc = cap_change / (10 * DIV(battery->fcc));

	DBG("smooth: ydsoc = %ld, delta_cap_remainder: %d fcc = %d\n",
		ydsoc, battery->delta_cap_remainder, battery->fcc);
	if (ydsoc == 0) {
		DBG("<%s>. ydsoc = 0\n", __func__);
		return;
	}

	battery->delta_cap_remainder = cap_change % (10 * DIV(battery->fcc));

	DBG("<%s>. k=%d, ydsoc=%ld; cap:old=%d, new:%d; delta_cap=%d\n",
	    __func__, battery->sm_linek, ydsoc, old_cap,
	    battery->sm_remain_cap, delta_cap);

	rk817_bat_update_soc(battery, ydsoc);

	battery->sm_remain_cap = battery->remain_cap;

	rk817_bat_calc_sm_linek(battery);

	DBG("smooth: smooth_soc = %d, dsoc = %d\n",
	    battery->smooth_soc, battery->dsoc);
	DBG("smooth: delta_cap = %d, dsoc = %d\n",
	    delta_cap, battery->dsoc);
}

static void rk817_bat_init_capacity(struct rk817_battery_device *battery,
				    u32 cap)
{
	int delta_cap;

	delta_cap = cap - battery->remain_cap;
	if (!delta_cap)
		return;

	battery->age_adjust_cap += delta_cap;
	rk817_bat_init_coulomb_cap(battery, cap);
	rk817_bat_smooth_algo_prepare(battery);
}

static void rk817_bat_finish_algorithm(struct rk817_battery_device *battery)
{
	unsigned long finish_sec, soc_sec;
	int plus_soc, finish_current, rest = 0;

	/* rsoc */
	if ((battery->remain_cap != battery->fcc) &&
	    (rk817_bat_get_charge_status(battery) == CHARGE_FINISH)) {
		battery->age_adjust_cap +=
			(battery->fcc * 1000 - battery->remain_cap);
		rk817_bat_init_coulomb_cap(battery, battery->fcc);
		rk817_bat_get_capacity_mah(battery);
	}

	/* dsoc */
	if (battery->dsoc < MAX_PERCENTAGE) {
		if (!battery->finish_base)
			battery->finish_base = get_boot_sec();

		finish_current = (battery->rsoc - battery->dsoc) / 1000 > FINISH_MAX_SOC_DELAY ?
					FINISH_CHRG_CUR2 : FINISH_CHRG_CUR1;
		finish_sec = base2sec(battery->finish_base);

		soc_sec = battery->fcc * 3600 / 100 / DIV(finish_current);
		if (soc_sec == 0)
			soc_sec = 1;
		plus_soc = finish_sec / DIV(soc_sec);

		if (finish_sec > soc_sec) {
			rest = finish_sec % soc_sec;
			battery->dsoc += plus_soc * 1000;
			battery->finish_base = get_boot_sec();
			if (battery->finish_base > rest)
				battery->finish_base = get_boot_sec() - rest;
		}

		DBG("CHARGE_FINISH:dsoc<100,dsoc=%d,\n"
		    "soc_time=%lu, sec_finish=%lu, plus_soc=%d, rest=%d\n",
		    battery->dsoc, soc_sec, finish_sec, plus_soc, rest);
		DBG("battery->age_adjust_cap = %d\n", battery->age_adjust_cap);
	}
	if (battery->dsoc > MAX_PERCENTAGE)
		battery->dsoc = MAX_PERCENTAGE;
}

static void rk817_bat_update_age_fcc(struct rk817_battery_device *battery)
{
	int fcc;
	int remain_cap;
	int age_keep_min;


	fcc = battery->fcc * 1000;
	remain_cap = fcc - battery->age_ocv_cap - battery->age_adjust_cap;
	age_keep_min = base2min(battery->age_keep_sec);

	DBG("%s: lock_fcc=%d, age_ocv_cap=%d, age_adjust_cap=%d, remain_cap=%d, age_allow_update=%d, age_keep_min=%d\n",
	    __func__, fcc, battery->age_ocv_cap, battery->age_adjust_cap, remain_cap,
	    battery->age_allow_update, age_keep_min);

	if ((battery->chrg_status == CHARGE_FINISH) && (battery->age_allow_update) &&
	    (age_keep_min < battery->fcc * 60 / 2000)) {
		battery->age_allow_update = false;
		fcc = remain_cap * 100 * 1000 / DIV(100 * 1000 - battery->age_ocv_soc);
		BAT_INFO("calc_cap=%d, age: soc=%d, cap=%d, level=%d, fcc:%d->%d?\n",
			 remain_cap, battery->age_ocv_soc,
			 battery->age_ocv_cap, battery->age_level, battery->fcc, fcc);

		if ((fcc < EFFECTIVE_FULL_MAX_CAP(battery->pdata->design_capacity)) &&
			(fcc > EFFECTIVE_FULL_MIN_CAP(battery->pdata->design_capacity))) {
			BAT_INFO("fcc:%d->%d!\n", battery->fcc, fcc);
			battery->fcc = fcc / 1000;
			rk817_bat_init_capacity(battery, battery->fcc);
			rk817_bat_save_fcc(battery, battery->fcc);
		}
	}
}

static void rk817_bat_wait_finish_sig(struct rk817_battery_device *battery)
{
	int chrg_finish_vol = battery->pdata->design_max_voltage;

	if (battery->chrg_status == CHARGE_FINISH)
		return;

	if ((battery->chrg_status == CHARGE_FINISH) &&
	    (battery->temperature >= VIRTUAL_TEMPERATURE) &&
	    (battery->voltage_avg > chrg_finish_vol - 150) && battery->age_allow_update) {
		rk817_bat_update_age_fcc(battery);/* save new fcc*/
		battery->age_allow_update = false;
	}
}

static void rk817_bat_display_smooth(struct rk817_battery_device *battery)
{
	if (battery->s2r && !battery->sleep_chrg_online) {
		DBG("s2r: discharge, reset algorithm...\n");
		battery->s2r = false;
		rk817_bat_smooth_algo_prepare(battery);
		return;
	}

	if (battery->work_mode == MODE_FINISH) {
		DBG("step1: charge finish...\n");
		rk817_bat_finish_algorithm(battery);
		if ((rk817_bat_get_charge_status(battery) != CHARGE_FINISH) &&
		    !rk817_bat_fake_finish_mode(battery)) {
			DBG("step1: change to smooth mode...\n");
			rk817_bat_smooth_algo_prepare(battery);
			battery->work_mode = MODE_SMOOTH;
		}
	} else {
		DBG("step3: smooth algorithm...\n");
		rk817_bat_smooth_algorithm(battery);
		if ((rk817_bat_get_charge_status(battery) == CHARGE_FINISH) ||
		    rk817_bat_fake_finish_mode(battery)) {
			DBG("step3: change to finish mode...\n");
			rk817_bat_finish_algo_prepare(battery);
			battery->work_mode = MODE_FINISH;
		}
	}
}

static void rk817_bat_stay_awake(struct rk817_battery_device *battery)
{
	bool status = false;

	if (!battery->pdata->charge_stay_awake)
		return;

	status = (battery->current_avg > 0) ||
		(battery->sleep_chrg_status == CC_OR_CV_CHRG) ||
		((battery->sleep_chrg_status == CHARGE_FINISH) &&
		(battery->dsoc / 1000 < 100));

	if (status && !battery->active_awake) {
		battery->active_awake = true;
		pm_stay_awake(battery->dev);
	} else {
		if (battery->active_awake && !status) {
			battery->active_awake = false;
			pm_relax(battery->dev);
		}
	}
}

static void rk817_bat_print_time(struct rk817_battery_device *battery)
{
	int cout_dsoc = battery->dsoc / 1000;
	int cout_rsoc = battery->rsoc / 1000;
	int time_avg = 0, time_count = 0;
	int j;

	if (cout_dsoc >= 0 && cout_dsoc < 100)
		battery->dbg_dcount[cout_dsoc / 10]++;

	if (cout_rsoc >= 0 && cout_rsoc < 100)
		battery->dbg_rcount[cout_rsoc / 10]++;

	if ((cout_dsoc < 1) || (cout_dsoc >= 99)) {
		for (j = 0; j < 10; j++)
			time_count += battery->dbg_dcount[j];
		time_avg = time_count / 10;

		for (j = 0; j < 10; j++)
			DBG("DSOC[%d]: %d(minute) %d(s), %d(s)",
			    j, battery->dbg_dcount[j] * battery->pdata->monitor_sec / 60,
			    (battery->dbg_dcount[j] * battery->pdata->monitor_sec) % 60,
			    (battery->dbg_dcount[j] - time_avg) * battery->pdata->monitor_sec);

		for (j = 0; j < 10; j++)
			DBG("RSOC[%d]: %d(minute) %d(s)",
			    j, battery->dbg_rcount[j] * battery->pdata->monitor_sec / 60,
			    battery->dbg_rcount[j] * battery->pdata->monitor_sec % 60);

		DBG("time:%d(minute): avg: %d(minute), %d(s)",
		    time_count * battery->pdata->monitor_sec / 60,
		    time_avg * battery->pdata->monitor_sec / 60,
		    (time_avg * battery->pdata->monitor_sec) % 60);
	}
}

static void rk817_bat_output_info(struct rk817_battery_device *battery)
{
	int index;

	DBG("info start:\n");
	DBG("info: voltage_k %d\n", battery->voltage_k);
	DBG("info: voltage_b %d\n", battery->voltage_b);
	DBG("info: voltage %d\n", battery->voltage_avg);
	DBG("info: voltage_sys %d\n", battery->voltage_sys);
	DBG("info: FCC %d\n", battery->fcc);
	DBG("info: fake_full_soc: %d\n", battery->fake_full_soc);
	DBG("info: awke: %d, count: %d\n",
	    battery->pdata->charge_stay_awake,
	    battery->active_awake);

	DBG("DEBUG: dsoc/1000: %d, dsoc: %d, rsoc: %d, sm_soc: %d, delta_rsoc: %d, vol: %d, exp_vol %d, current: %d, sm_link: %d, remain_cap: %d, sm_cap: %d\n",
	    battery->dsoc / 1000, battery->dsoc, battery->rsoc,
	    battery->smooth_soc, battery->delta_rsoc,
	    battery->voltage_avg, battery->expected_voltage, battery->current_avg,
	    battery->sm_linek, battery->remain_cap, battery->sm_remain_cap);
	rk817_bat_print_time(battery);
	if (battery->pdata->ntc_size) {
		index = battery->charge_index;
		DBG("Temperature: %d charger current: %dmA, index: %d, charger voltage: %dmV, index: %d\n",
		    battery->temperature,
		    battery->pdata->tc_table[index].chrg_current,
		    battery->pdata->tc_table[index].chrg_current_index,
		    battery->pdata->tc_table[index].chrg_voltage,
		    battery->pdata->tc_table[index].chrg_voltage_index);
	}
	DBG("info END.\n");
}

static void rk817_battery_work(struct work_struct *work)
{
	struct rk817_battery_device *battery =
		container_of(work,
			     struct rk817_battery_device,
			     bat_delay_work.work);

	rk817_bat_update_fg_info(battery);
	rk817_bat_wait_finish_sig(battery);
	rk817_bat_lowpwr_check(battery);
	rk817_bat_display_smooth(battery);
	rk817_bat_update_fcc(battery);
	rk817_bat_power_supply_changed(battery);
	rk817_bat_save_data(battery);
	rk817_bat_stay_awake(battery);
	rk817_bat_update_temperature(battery);
	rk817_bat_output_info(battery);

	if (rk817_bat_field_read(battery, CUR_CALIB_UPD)) {
		rk817_bat_current_calibration(battery);
		rk817_bat_init_voltage_kb(battery);
		rk817_bat_field_write(battery, CUR_CALIB_UPD, 0x01);
	}

	queue_delayed_work(battery->bat_monitor_wq, &battery->bat_delay_work,
			   msecs_to_jiffies(battery->monitor_ms));
}

static irqreturn_t rk809_plug_in_isr(int irq, void *cg)
{
	struct rk817_battery_device *battery;

	battery = (struct rk817_battery_device *)cg;
	battery->plugin_trigger = 1;
	battery->plugout_trigger = 0;
	power_supply_changed(battery->bat);
	if (battery->is_register_chg_psy)
		power_supply_changed(battery->chg_psy);

	return IRQ_HANDLED;
}

static irqreturn_t rk809_plug_out_isr(int irq, void *cg)
{
	struct rk817_battery_device *battery;

	battery = (struct rk817_battery_device *)cg;
	battery->plugin_trigger = 0;
	battery->plugout_trigger = 1;
	power_supply_changed(battery->bat);
	if (battery->is_register_chg_psy)
		power_supply_changed(battery->chg_psy);

	return IRQ_HANDLED;
}

static int rk809_charge_init_irqs(struct rk817_battery_device *battery)
{
	struct rk808 *rk817 = battery->rk817;
	struct platform_device *pdev = battery->pdev;
	int ret, plug_in_irq, plug_out_irq;

	battery->plugin_trigger = 0;
	battery->plugout_trigger = 0;

	plug_in_irq = regmap_irq_get_virq(rk817->irq_data, RK817_IRQ_PLUG_IN);
	if (plug_in_irq < 0) {
		dev_err(battery->dev, "plug_in_irq request failed!\n");
		return plug_in_irq;
	}

	plug_out_irq = regmap_irq_get_virq(rk817->irq_data, RK817_IRQ_PLUG_OUT);
	if (plug_out_irq < 0) {
		dev_err(battery->dev, "plug_out_irq request failed!\n");
		return plug_out_irq;
	}

	ret = devm_request_threaded_irq(battery->dev, plug_in_irq, NULL,
					rk809_plug_in_isr,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"rk817_plug_in", battery);
	if (ret) {
		dev_err(&pdev->dev, "plug_in_irq request failed!\n");
		return ret;
	}

	ret = devm_request_threaded_irq(battery->dev, plug_out_irq, NULL,
					rk809_plug_out_isr,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"rk817_plug_out", battery);
	if (ret) {
		dev_err(&pdev->dev, "plug_out_irq request failed!\n");
		return ret;
	}

	if (rk817_bat_field_read(battery, PLUG_IN_STS)) {
		battery->plugin_trigger = 1;
		battery->plugout_trigger = 0;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id rk817_bat_of_match[] = {
	{ .compatible = "rk817,battery", },
	{ },
};
MODULE_DEVICE_TABLE(of, rk817_bat_of_match);
#else
static const struct of_device_id rk817_bat_of_match[] = {
	{ },
};
#endif

static int rk817_battery_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
			of_match_device(rk817_bat_of_match, &pdev->dev);
	struct rk808 *rk817 = dev_get_drvdata(pdev->dev.parent);
	struct i2c_client *client = rk817->i2c;
	struct rk817_battery_device *battery;
	int i, ret;

	if (!of_id) {
		dev_err(&pdev->dev, "Failed to find matching dt id\n");
		return -ENODEV;
	}

	battery = devm_kzalloc(&client->dev, sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -EINVAL;

	battery->rk817 = rk817;
	battery->client = client;
	battery->dev = &pdev->dev;
	platform_set_drvdata(pdev, battery);
	battery->chip_id = rk817->variant;

	if (IS_ERR(rk817->regmap)) {
		dev_err(battery->dev, "Failed to initialize regmap\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(rk817_battery_reg_fields); i++) {
		const struct reg_field *reg_fields = rk817_battery_reg_fields;

		battery->rmap_fields[i] =
			devm_regmap_field_alloc(battery->dev,
						rk817->regmap,
						reg_fields[i]);
		if (IS_ERR(battery->rmap_fields[i])) {
			dev_err(battery->dev, "cannot allocate regmap field\n");
			return PTR_ERR(battery->rmap_fields[i]);
		}
	}

	ret = rk817_bat_parse_dt(battery);
	if (ret < 0) {
		dev_err(battery->dev, "battery parse dt failed!\n");
		return ret;
	}

	rk817_bat_init_info(battery);
	rk817_bat_init_fg(battery);

	rk817_battery_debug_info(battery);
	rk817_bat_update_fg_info(battery);

	rk817_bat_output_info(battery);
	battery->bat_monitor_wq = alloc_ordered_workqueue("%s",
			WQ_MEM_RECLAIM | WQ_FREEZABLE, "rk817-bat-monitor-wq");
	INIT_DELAYED_WORK(&battery->bat_delay_work, rk817_battery_work);
	queue_delayed_work(battery->bat_monitor_wq, &battery->bat_delay_work,
			   msecs_to_jiffies(TIMER_MS_COUNTS * 5));

	ret = rk817_bat_init_power_supply(battery);
	if (ret) {
		dev_err(battery->dev, "rk817 power supply register failed!\n");
		return ret;
	}
	if (battery->is_register_chg_psy) {
		ret = rk809_chg_init_power_supply(battery);
		if (ret) {
			dev_err(battery->dev, "rk809 chg psy init failed!\n");
			return ret;
		}
	}

	if (battery->chip_id == RK809_ID)
		rk809_charge_init_irqs(battery);

	device_init_wakeup(battery->dev, true);

	DBG("name: 0x%x", rk817_bat_field_read(battery, CHIP_NAME_H));
	DBG("%x\n", rk817_bat_field_read(battery, CHIP_NAME_L));
	BAT_INFO("driver version %s\n", DRIVER_VERSION);

	return 0;
}

static void rk817_battery_shutdown(struct platform_device *dev)
{
}

static time64_t rk817_get_rtc_sec(void)
{
	int err;
	struct rtc_time tm;
	struct rtc_device *rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

	err = rtc_read_time(rtc, &tm);
	if (err) {
		dev_err(rtc->dev.parent, "read hardware clk failed\n");
		return 0;
	}

	err = rtc_valid_tm(&tm);
	if (err) {
		dev_err(rtc->dev.parent, "invalid date time\n");
		return 0;
	}

	return rtc_tm_to_time64(&tm);
}

static int rk817_bat_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rk817_battery_device *battery = dev_get_drvdata(&pdev->dev);

	cancel_delayed_work_sync(&battery->bat_delay_work);
	rk817_bat_update_fg_info(battery);

	battery->s2r = false;
	battery->sleep_chrg_status = rk817_bat_get_charge_status(battery);
	/* battery->current_avg = rk817_bat_get_avg_current(battery); */
	battery->current_sleep = battery->current_avg;

	if (battery->current_avg > 0 ||
	    (battery->sleep_chrg_status == CC_OR_CV_CHRG) ||
	    (battery->sleep_chrg_status == CHARGE_FINISH))
		battery->sleep_chrg_online = 1;
	else
		battery->sleep_chrg_online = 0;

	battery->remain_cap = rk817_bat_get_capacity_uah(battery);
	battery->rsoc = rk817_bat_get_rsoc(battery);

	battery->rtc_base = rk817_get_rtc_sec();
	rk817_bat_save_data(battery);

	if (is_rk817_bat_relax_mode(battery))
		rk817_bat_restart_relax(battery);

	if (battery->sleep_chrg_status != CHARGE_FINISH)
		battery->finish_base = get_boot_sec();

	rk817_bat_calc_sm_linek(battery);
	DBG("suspend get_boot_sec: %lld\n", get_boot_sec());

	DBG("suspend: dl=%d rl=%d c=%d v=%d cap=%d at=%ld ch=%d\n",
	    battery->dsoc, battery->rsoc, battery->current_avg,
	    rk817_bat_get_battery_voltage(battery),
	    rk817_bat_get_capacity_uah(battery),
	    battery->sleep_dischrg_sec, battery->sleep_chrg_online);

	DBG("battery->sleep_chrg_status=%d\n", battery->sleep_chrg_status);

	return 0;
}

static int rk817_bat_rtc_sleep_sec(struct rk817_battery_device *battery)
{
	int interval_sec;

	interval_sec = rk817_get_rtc_sec() - battery->rtc_base;

	return (interval_sec > 0) ? interval_sec : 0;
}

static void rk817_bat_relife_age_flag(struct rk817_battery_device *battery)
{
	int age_level, ocv_soc, ocv_cap;

	if (battery->voltage_relax <= 0)
		return;

	ocv_soc = rk817_bat_vol2soc(battery, battery->voltage_relax) / 1000;
	ocv_cap = rk817_bat_vol2cap(battery, battery->voltage_relax) / 1000;
	DBG("Resume: <%s>. ocv_soc=%d, min=%lu, vol=%d\n", __func__,
	    ocv_soc, battery->sleep_dischrg_sec / 60, battery->voltage_relax);

	/* sleep enough time and ocv_soc enough low */
	if (!battery->age_allow_update && ocv_soc <= 10) {
		battery->age_voltage = battery->voltage_relax;
		battery->age_ocv_cap = ocv_cap;
		battery->age_ocv_soc = ocv_soc;
		battery->age_adjust_cap = 0;

		if (ocv_soc <= 1)
			battery->age_level = 100;
		else if (ocv_soc < 5)
			battery->age_level = 90;
		else
			battery->age_level = 80;

		age_level = rk817_bat_get_age_level(battery);
		if (age_level > battery->age_level) {
			battery->age_allow_update = false;
			age_level -= 5;
			if (age_level <= 80)
				age_level = 80;
			rk817_bat_save_age_level(battery, age_level);
		} else {
			battery->age_allow_update = true;
			battery->age_keep_sec = get_boot_sec();
		}

		BAT_INFO("resume: age_vol:%d, age_ocv_cap:%d, age_ocv_soc:%d, age_allow_update:%d, age_level:%d\n",
			 battery->age_voltage, battery->age_ocv_cap,
			 ocv_soc, battery->age_allow_update, battery->age_level);
	}
}

static void rk817_bat_relax_vol_calib(struct rk817_battery_device *battery)
{
	int soc, cap, vol;

	vol = battery->voltage_relax;
	soc = rk817_bat_vol2soc(battery, vol) / 1000;
	cap = rk817_bat_vol2cap(battery, vol);
	rk817_bat_init_capacity(battery, cap);
	BAT_INFO("sleep relax voltage calib: rsoc=%d, cap=%d\n", soc, cap);
}

static void rk817_bat_resume_profile_smoothing(struct rk817_battery_device *battery)
{
	int delta_cap = 0, old_cap = 0;
	unsigned long charge_soc;
	int interval_sec = 0;
	long cap_change;
	long ydsoc = 0;

	battery->remain_cap = rk817_bat_get_capacity_uah(battery);
	old_cap = battery->sm_remain_cap;

	DBG("smooth: smooth_soc = %d, dsoc = %d, battery->sm_linek = %d\n",
	    battery->smooth_soc, battery->dsoc, battery->sm_linek);

	/* discharge status: sm_remain_cap > remain_cap, delta_cap > 0 */
	/* from charge to discharge:
	 * remain_cap may be above sm_remain_cap, delta_cap <= 0
	 */
	delta_cap = battery->remain_cap - battery->sm_remain_cap;

	DBG("smooth: sm_remain_cap: %d, remain_cap: %d, delta_cap: %d\n",
	    battery->sm_remain_cap, battery->remain_cap, delta_cap);

	/* discharge: sm_linek < 0, if delate_cap <0, ydsoc > 0 */
	/* captosoc: delta_cap * MAX_PERCENTAGE / (DIV(battery->fcc) * 1000)
	 * delta_cap * 100 / DIV(battery->fcc)
	 *
	 * captosoc Remainder: delta_cap * MAX_PERCENTAGE % (DIV(battery->fcc) * 1000)
	 * delta_cap * 100 % DIV(battery->fcc)
	 */
	/* delta_cap += battery->delta_cap_remainder;
	 * ydsoc = battery->sm_linek * abs(delta_cap) / (10 * DIV(battery->fcc));
	 */

	/* ydsoc = battery->sm_linek * (abs(delta_cap) * 100 + battery->delta_cap_remainder) /
	 * DIV(battery->fcc) / 1000;
	 */
	cap_change = battery->sm_linek * abs(delta_cap) + battery->delta_cap_remainder;
	ydsoc = cap_change / (10 * DIV(battery->fcc));

	DBG("smooth: ydsoc = %ld, fcc = %d\n", ydsoc, battery->fcc);

	DBG("<%s>. k=%d, ydsoc=%ld; cap:old=%d, new:%d; delta_cap=%d\n",
	    __func__, battery->sm_linek, ydsoc, old_cap,
	    battery->sm_remain_cap, delta_cap);

	/* finish:
	 * 1, suspend online: battery->sleep_chrg_online = 1
	 */
	if (battery->sleep_chrg_online && ((battery->rsoc >= battery->fake_full_soc) ||
	    (rk817_bat_get_charge_status(battery) == CHARGE_FINISH))) {
		if (battery->current_sleep < FINISH_CHRG_CUR1)
			battery->current_sleep = FINISH_CHRG_CUR1;
		interval_sec = rk817_bat_rtc_sleep_sec(battery);
		charge_soc =
			interval_sec * battery->current_sleep * MAX_PERCENTAGE / 3600 / DIV(battery->fcc);

		if (ydsoc < charge_soc) {
			battery->dsoc += charge_soc;
			battery->smooth_soc = battery->dsoc;
			battery->delta_cap_remainder = 0;
			battery->sm_remain_cap = battery->remain_cap;
		}
	} else {
		/* discharge mode, but ydsoc > 0, from charge status to dischrage
		 */
		if (ydsoc != 0) {
			battery->smooth_soc += ydsoc;
			battery->dsoc += ydsoc;
			battery->delta_cap_remainder = cap_change % (10 * DIV(battery->fcc));
			battery->sm_remain_cap = battery->remain_cap;
		}
	}

	if (rk817_bat_field_read(battery, CHG_STS) == CHARGE_FINISH) {
		battery->rsoc = MAX_PERCENTAGE;
		rk817_bat_init_coulomb_cap(battery, battery->fcc);
	}

	/* check new dsoc */
	if (battery->smooth_soc < 0)
		battery->smooth_soc = 0;
	if (battery->dsoc < 0)
		battery->dsoc = 0;
	if (battery->smooth_soc > MAX_PERCENTAGE)
		battery->smooth_soc = MAX_PERCENTAGE;
	if (battery->dsoc > MAX_PERCENTAGE)
		battery->dsoc = MAX_PERCENTAGE;

	rk817_bat_output_info(battery);

	DBG("Resume: voltage_relax: %d\n", battery->voltage_relax);
	if (is_rk817_bat_relax_mode(battery)) {
		if (battery->voltage_relax >= battery->voltage_avg) {
			rk817_bat_relax_vol_calib(battery);
			rk817_bat_restart_relax(battery);
			rk817_bat_relife_age_flag(battery);
		}
		DBG("Resume:relax:\n");
		rk817_bat_output_info(battery);
	}
	rk817_bat_calc_sm_linek(battery);
}

static int rk817_bat_pm_resume(struct device *dev)
{
	struct rk817_battery_device *battery = dev_get_drvdata(dev);
	int interval_sec = 0;

	rk817_bat_update_fg_info(battery);

	battery->s2r = true;
	interval_sec = rk817_bat_rtc_sleep_sec(battery);
	battery->sleep_sum_sec += interval_sec;

	rk817_bat_resume_profile_smoothing(battery);
	rk817_bat_save_data(battery);
	DBG("RESUME:");
	rk817_bat_output_info(battery);

	queue_delayed_work(battery->bat_monitor_wq, &battery->bat_delay_work,
			   msecs_to_jiffies(1000));
	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(rk817_bat_pm_ops,
				rk817_bat_pm_suspend,
				rk817_bat_pm_resume);

static struct platform_driver rk817_battery_driver = {
	.probe = rk817_battery_probe,
	.shutdown = rk817_battery_shutdown,
	.driver = {
		.name = "rk817-battery",
		.pm = pm_sleep_ptr(&rk817_bat_pm_ops),
		.of_match_table = of_match_ptr(rk817_bat_of_match),
	},
};

static int __init rk817_battery_init(void)
{
	return platform_driver_register(&rk817_battery_driver);
}
fs_initcall_sync(rk817_battery_init);

static void __exit rk817_battery_exit(void)
{
	platform_driver_unregister(&rk817_battery_driver);
}
module_exit(rk817_battery_exit);

MODULE_DESCRIPTION("RK817 Battery driver");
MODULE_LICENSE("GPL");
