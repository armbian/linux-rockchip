/* Himax Android Driver Sample Code for common functions
 *
 * Copyright (C) 2021 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef HIMAX_COMMON_H
#define HIMAX_COMMON_H

#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/extcon.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/async.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/firmware.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/pm_wakeup.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#if defined(CONFIG_OF)
#include <linux/of_gpio.h>
#endif

#include "himax_platform.h"

#define HIMAX_DRIVER_VER "Sample_code_V61_A08.5.2"
#define FLASH_DUMP_FILE "/sdcard/HX_Flash_Dump.bin"

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
#define HX_TP_PROC_2T2R
/*if enable, selftest works in driver*/
/*#define HX_TP_SELF_TEST_DRIVER*/
#endif

/*===========Himax Option function=============*/
#define HX_BOOT_UPGRADE
/*#define HX_PROTOCOL_A*/
#define HX_PROTOCOL_B_3PA
#define HX_SYSTEM_RESET 1

/*for Himax auto-motive chipset */
#define HX_RST_PIN_FUNC
#define HX_TP_INSPECT_MODE
#define HX_FIX_TOUCH_INFO
/*#define HX_ID_EN*/
/*#define FCA_PROTOCOL_EN */
/*=============================================*/

/* Enable it if driver go into suspend/resume twice */
/*#undef HX_CONFIG_FB*/
/* Enable it if driver go into suspend/resume twice */
/*#undef HX_CONFIG_DRM*/

#if defined(HX_CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(HX_CONFIG_DRM)
#include <linux/msm_drm_notify.h>
#endif

#if defined(__HIMAX_MOD__)
#define HX_USE_KSYM
#if !defined(HX_USE_KSYM) || !defined(__KERNEL_KALLSYMS_ALL_ENABLED__)
#error Modulized driver must enable HX_USE_KSYM and CONFIG_KALLSYM_ALL
#endif
#endif

/* WP GPIO setting, decided by which pin direct to OS side, WP need pin */
/* high either GPIO0 or GPIO4 */
/* #define WP_GPIO0 */
#define WP_GPIO4

#if defined(HX_BOOT_UPGRADE)
/* FW Auto upgrade case, you need to setup the fix_touch_info of module */
#define BOOT_UPGRADE_FWNAME "Himax_firmware.bin"
#endif

#if defined(HX_CONTAINER_SPEED_UP)
/* Resume queue delay work time after LCM RST (unit:ms) */
#define DELAY_TIME 40
#endif

#define HX_MAX_WRITE_SZ (64 * 1024 + 4)
#define HX_KEY_MAX_COUNT 4
#define DEFAULT_RETRY_CNT 3

#define HX_83191A_SERIES_PWON "HX83191A"
#define HX_83192A_SERIES_PWON "HX83192A"
#define HX_83193A_SERIES_PWON "HX83193A"

#define HX_TP_BIN_CHECKSUM_SW 1
#define HX_TP_BIN_CHECKSUM_HW 2
#define HX_TP_BIN_CHECKSUM_CRC 3

#define SHIFTBITS 5
#define FW_SIZE_64k 65536
#define FW_SIZE_128k 131072

#define NO_ERR 0
#define READY_TO_SERVE 1
#define WORK_OUT 2

#define I2C_FAIL -1
#define HX_INIT_FAIL -1
#define MEM_ALLOC_FAIL -2
#define CHECKSUM_FAIL -3
#define GESTURE_DETECT_FAIL -4
#define INPUT_REGISTER_FAIL -5
#define FW_NOT_READY -6
#define LENGTH_FAIL -7
#define OPEN_FILE_FAIL -8
#define PROBE_FAIL -9
#define ERR_WORK_OUT -10
#define ERR_STS_WRONG -11
#define ERR_TEST_FAIL -12

#define HW_CRC_FAIL 1
#define HX_FINGER_ON 1
#define HX_FINGER_LEAVE 2

#if defined(__EMBEDDED_FW__)
extern const uint8_t _binary___Himax_firmware_bin_start[];
extern const uint8_t _binary___Himax_firmware_bin_end[];
extern struct firmware g_embedded_fw;
#endif

enum HX_TS_PATH {
	HX_REPORT_COORD = 1,
	HX_REPORT_COORD_RAWDATA,
};

enum HX_TS_STATUS {
	HX_TS_GET_DATA_FAIL = -4,
	HX_CHKSUM_FAIL,
	HX_PATH_FAIL,
	HX_TS_NORMAL_END = 0,
	HX_READY_SERVE,
	HX_REPORT_DATA,
	HX_EXCP_WARNING,
	HX_RST_OK,
};

enum cell_type { CHIP_IS_ON_CELL, CHIP_IS_IN_CELL };

#if defined(HX_FIX_TOUCH_INFO)
enum fix_touch_info {
	FIX_HX_RX_NUM = 60,
	FIX_HX_TX_NUM = 32,
	FIX_HX_BT_NUM = 0,
	FIX_HX_MAX_PT = 10,

	FIX_HX_XY_REVERSE = false,
	FIX_HX_INT_IS_EDGE = true,

#if defined(HX_TP_PROC_2T2R)
	FIX_HX_RX_NUM_2 = 0,
	FIX_HX_TX_NUM_2 = 0,
#endif
};
#endif

enum input_protocol_type {
	PROTOCOL_TYPE_A = 0x00,
	PROTOCOL_TYPE_B = 0x01,
};

/*void himax_HW_reset(uint8_t loadconfig,uint8_t int_off);*/
int himax_chip_common_suspend(struct himax_ts_data *ts);
int himax_chip_common_resume(struct himax_ts_data *ts);

struct himax_core_fp;
extern struct device *g_device;

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
int himax_debug_init(struct himax_ts_data *ts);
int himax_debug_remove(struct himax_ts_data *ts);
#endif

int himax_parse_dt(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata);
int himax_report_data(struct himax_ts_data *ts, int ts_path, int ts_status);
int himax_report_data_init(struct himax_ts_data *ts);
int himax_dev_set(struct himax_ts_data *ts);
int himax_input_register_device(struct input_dev *input_dev);
int himax_input_register(struct himax_ts_data *ts);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
extern void himax_mcu_in_cmd_struct_free(struct himax_ts_data *ts);
#endif
#endif
