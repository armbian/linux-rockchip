/*
 * pt_regs.h
 * Parade TrueTouch(TM) Standard Product Registers.
 * For use with Parade touchscreen controllers.
 * Supported parts include:
 * TMA5XX
 * TMA448
 * TMA445A
 * TT21XXX
 * TT31XXX
 * TT4XXXX
 * TT7XXX
 * TC3XXX
 *
 * Copyright (C) 2015-2020 Parade Technologies
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Parade Technologies at www.paradetech.com <ttdrivers@paradetech.com>
 *
 */

#ifndef _PT_REGS_H
#define _PT_REGS_H

#define PT_PANEL_ID_DEFAULT     0

#define PT_MAX_PATH_SIZE 128
#define PT_PIP2_BIN_FILE_PATH "/data/ttdl/pt_fw.bin"
#define PT_SUPPRESS_AUTO_BL 0
#define PT_ALLOW_AUTO_BL    1

#define PT_PIP2_MAX_FILE_SIZE           0x18000
#define PT_PIP2_FILE_SECTOR_SIZE        0x1000

#include <linux/device.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#include <asm/unaligned.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include "pt_core.h"
#include "../tp_suspend.h"

#define STATUS_SUCCESS   0
#define STATUS_FAIL     -1

#define PT_FW_FILE_PREFIX	"tt_fw"
#define PT_FW_FILE_SUFFIX	".bin"
#define PT_FW_FILE_NAME		"tt_fw.bin"
#define PT_FW_RAM_FILE_NAME	"tt_fw_ram.bin"
/* Enable special TTDL features */
#ifndef TTHE_TUNER_SUPPORT
#define TTHE_TUNER_SUPPORT
#endif

#ifndef TTDL_DIAGNOSTICS
#define TTDL_DIAGNOSTICS
#endif

#ifndef EASYWAKE_TSG6
#define EASYWAKE_TSG6
#endif

#ifdef TTHE_TUNER_SUPPORT
#define PT_TTHE_TUNER_FILE_NAME "tthe_tuner"
#endif
#define PT_MAX_PRBUF_SIZE       PIPE_BUF
#define PT_PR_TRUNCATED         " truncated..."

#define PT_DEFAULT_CORE_ID      "pt_core0"
#define PT_MAX_NUM_CORE_DEVS    5
#define PT_IRQ_ASSERTED_VALUE	0

#ifdef PT_ENABLE_MAX_ELEN
#define PT_MAX_ELEN 100
#endif

/*
 * The largest PIP message is the PIP2 FILE_WRITE which has:
 *     2 byte register
 *     4 byte header
 *   256 byte payload
 *     2 byte CRC
 */
#define PT_MAX_PIP2_MSG_SIZE      264
#define PT_MAX_PIP1_MSG_SIZE      255

/*
 * The minimun size of PIP2 packet includes:
 *     2 byte length
 *     1 byte sequence
 *     1 byte command ID
 *     2 byte CRC
 */
#define PT_MIN_PIP2_PACKET_SIZE   6

static const u8 pt_data_block_security_key[] = {
	0xA5, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0x5A
};

/* Enum for debug reporting levels */
enum PT_DEBUG_LEVEL {
	DL_QUIET	= 0,
	DL_ERROR	= 1,
	DL_WARN		= 2,
	DL_INFO		= 3,
	DL_DEBUG	= 4,
	DL_MAX
};
#define PT_INITIAL_DEBUG_LEVEL DL_WARN

/* Startup DUT enum status bitmask */
enum PT_STARTUP_STATUS {
	STARTUP_STATUS_START              = 0,
	STARTUP_STATUS_BL_RESET_SENTINEL  = 0x001,
	STARTUP_STATUS_FW_RESET_SENTINEL  = 0x002,
	STARTUP_STATUS_GET_DESC           = 0x004,
	STARTUP_STATUS_FW_OUT_OF_BOOT     = 0x008,
	STARTUP_STATUS_GET_RPT_DESC       = 0x010,
	STARTUP_STATUS_GET_SYS_INFO       = 0x020,
	STARTUP_STATUS_GET_CFG_CRC        = 0x040,
	STARTUP_STATUS_RESTORE_PARM       = 0x080,
	STARTUP_STATUS_COMPLETE           = 0x100,
	STARTUP_STATUS_FULL               = 0x1FF
};

#define PT_INITIAL_SHOW_TIME_STAMP 0

/*
 * Print out all debug prints that are less then or equal to set level.
 */
#define pt_debug(dev, dlevel, format, arg...)	 \
	do { \
		struct pt_core_data *cd_tmp = dev_get_drvdata(dev);\
		if (cd_tmp->debug_level >= dlevel) {\
			if (dlevel == DL_ERROR)\
				dev_err(dev, "[%d] "format, dlevel, ##arg);\
			else\
				dev_info(dev, "[%d] "format, dlevel, ##arg);\
		} \
	} while (0)

enum PT_PIP_REPORT_ID {
	PT_PIP_INVALID_REPORT_ID           = 0x00,
	PT_PIP_TOUCH_REPORT_ID             = 0x01,
	PT_PIP_TOUCH_REPORT_WIN8_ID        = 0x02,
	PT_PIP_CAPSENSE_BTN_REPORT_ID      = 0x03,
	PT_PIP_WAKEUP_REPORT_ID            = 0x04,
	PT_PIP_NOISE_METRIC_REPORT_ID      = 0x05,
	PT_PIP_PUSH_BUTTON_REPORT_ID       = 0x06,
	PT_PIP_SELFCAP_INPUT_REPORT_ID     = 0x0D,
	PT_PIP_TRACKING_HEATMAP_REPORT_ID  = 0x0E,
	PT_PIP_SENSOR_DATA_REPORT_ID       = 0x0F,
	PT_PIP_NON_HID_RESPONSE_ID         = 0x1F,
	PT_PIP_NON_HID_COMMAND_ID          = 0x2F,
	PT_PIP_BL_RESPONSE_REPORT_ID       = 0x30,
	PT_PIP_BL_COMMAND_REPORT_ID        = 0x40
};

enum PT_HID_REPORT_ID {
	PT_HID_FINGER_REPORT_ID            = 0x01,
	PT_HID_PEN_REPORT_ID               = 0x02
};


/* HID IDs and commands */
#define HID_VENDOR_ID                         0x04B4
#define HID_APP_PRODUCT_ID                    0xC101
#define HID_VERSION                           0x0100
#define HID_APP_REPORT_ID                       0xF7
#define HID_BL_REPORT_ID                        0xFF
#define HID_RESPONSE_REPORT_ID                  0xF0
#define HID_POWER_ON                             0x0
#define HID_POWER_SLEEP                          0x1
#define HID_POWER_STANDBY                        0x2

/* PIP1 offsets and masks */
#define PIP1_RESP_REPORT_ID_OFFSET                 2
#define PIP1_RESP_COMMAND_ID_OFFSET                4
#define PIP1_RESP_COMMAND_ID_MASK               0x7F
#define PIP1_CMD_COMMAND_ID_OFFSET                 6
#define PIP1_CMD_COMMAND_ID_MASK                0x7F

#define PIP1_SYSINFO_TTDATA_OFFSET                 5
#define PIP1_SYSINFO_SENSING_OFFSET               33
#define PIP1_SYSINFO_BTN_OFFSET                   48
#define PIP1_SYSINFO_BTN_MASK                   0xFF
#define PIP1_SYSINFO_MAX_BTN                       8

/*  Timeouts in ms */
#define PT_PTSBC_INIT_WATCHDOG_TIMEOUT         20000
#define PT_REQUEST_EXCLUSIVE_TIMEOUT            8000
#define PT_WATCHDOG_TIMEOUT                     2000
#define PT_FW_EXIT_BOOT_MODE_TIMEOUT            1000
#define PT_BL_WAIT_FOR_SENTINEL                  500
#define PT_REQUEST_ENUM_TIMEOUT                 4000
#define PT_GET_HID_DESCRIPTOR_TIMEOUT            500
#define PT_HID_CMD_DEFAULT_TIMEOUT               500
#define PT_PIP_CMD_DEFAULT_TIMEOUT              2000
#define PT_PIP1_CMD_DEFAULT_TIMEOUT             1000
#define PT_PIP1_START_BOOTLOADER_TIMEOUT        2000
#define PT_PIP1_CMD_GET_SYSINFO_TIMEOUT          500
#define PT_PIP1_CMD_CALIBRATE_IDAC_TIMEOUT      5000
#define PT_PIP1_CMD_CALIBRATE_EXT_TIMEOUT       5000
#define PT_PIP1_CMD_WRITE_CONF_BLOCK_TIMEOUT     400
#define PT_PIP1_CMD_RUN_SELF_TEST_TIMEOUT      10000
#define PT_PIP1_CMD_INITIATE_BL_TIMEOUT        20000
#define PT_PIP1_CMD_PROGRAM_AND_VERIFY_TIMEOUT   400
#define PT_PIP2_CMD_FILE_ERASE_TIMEOUT          3000

/* Max counts */
#define PT_WATCHDOG_RETRY_COUNT                   30
#define PT_BUS_READ_INPUT_RETRY_COUNT              2

#define PT_FLUSH_BUS_BASED_ON_LEN                  0
#define PT_FLUSH_BUS_FULL_256_READ                 1

/* maximum number of concurrent tracks */
#define TOUCH_REPORT_SIZE                         10
#define TOUCH_INPUT_HEADER_SIZE                    7
#define TOUCH_COUNT_BYTE_OFFSET                    5
#define BTN_REPORT_SIZE                            9
#define BTN_INPUT_HEADER_SIZE                      5
#define SENSOR_REPORT_SIZE                       150
#define SENSOR_HEADER_SIZE                         4

/* helpers */
#define GET_NUM_TOUCHES(x)          ((x) & 0x1F)
#define IS_LARGE_AREA(x)            ((x) & 0x20)
#define IS_BAD_PKT(x)               ((x) & 0x20)
#define IS_TMO(t)                   ((t) == 0)
#define HI_BYTE(x)                  (u8)(((x) >> 8) & 0xFF)
#define LOW_BYTE(x)                 (u8)((x) & 0xFF)
#define SET_CMD_LOW(byte, bits)	\
	((byte) = (((byte) & 0xF0) | ((bits) & 0x0F)))
#define SET_CMD_HIGH(byte, bits)\
	((byte) = (((byte) & 0x0F) | ((bits) & 0xF0)))

#define GET_MASK(length) \
	((1 << length) - 1)
#define GET_FIELD(name, length, shift) \
	((name >> shift) & GET_MASK(length))

#define _base(x) ((x >= '0' && x <= '9') ? '0' : \
	(x >= 'a' && x <= 'f') ? 'a' - 10 : \
	(x >= 'A' && x <= 'F') ? 'A' - 10 : \
	'\255')
#define HEXOF(x) (x - _base(x))

#define IS_EASY_WAKE_CONFIGURED(x) \
		((x) != 0 && (x) != 0xFF)

#define IS_PIP_VER_GE(p, maj, min) \
		((p)->ttdata.pip_ver_major > (maj) ? \
			1 : \
			(((p)->ttdata.pip_ver_major == (maj) ? \
				((p)->ttdata.pip_ver_minor >= (min) ? \
				1 : 0) : \
				0)))
#define IS_PIP_VER_EQ(p, maj, min) \
		((p)->ttdata.pip_ver_major == (maj) ? \
			((p)->ttdata.pip_ver_minor == (min) ? \
			1 : \
				0 : \
				0))
#define PT_PANEL_ID_BITMASK  0x0000000C
#define PT_PANEL_ID_SHIFT    2

#define TTDL_PTVIRTDUT_SUPPORT  1

/* DUT Debug commands (dut_debug sysfs) */
#define PT_DUT_DBG_HID_RESET                  50
#define PT_DUT_DBG_HID_GET_REPORT             51 /* depricated */
#define PT_DUT_DBG_HID_SET_REPORT             52 /* depricated */
#define PT_DUT_DBG_HID_SET_POWER_ON           53
#define PT_DUT_DBG_HID_SET_POWER_SLEEP        54
#define PT_DUT_DBG_HID_SET_POWER_STANDBY      55
#define PT_DUT_DBG_PIP_SOFT_RESET             97
#define PT_DUT_DBG_RESET                      98
#define PT_DUT_DBG_PIP_NULL                  100
#define PT_DUT_DBG_PIP_ENTER_BL              101
#define PT_DUT_DBG_HID_SYSINFO               102
#define PT_DUT_DBG_PIP_SUSPEND_SCAN          103
#define PT_DUT_DBG_PIP_RESUME_SCAN           104
#define	PT_DUT_DBG_HID_DESC                  109

/* Driver Debug commands (drv_debug sysfs) */
#define PT_DRV_DBG_SUSPEND                     4
#define PT_DRV_DBG_RESUME                      5
#define PT_DRV_DBG_STOP_WD                   105
#define PT_DRV_DBG_START_WD                  106
#define PT_DRV_DBG_TTHE_TUNER_EXIT           107
#define	PT_DRV_DBG_TTHE_BUF_CLEAN            108
#define	PT_DRV_DBG_CLEAR_PARM_LIST           110
#define PT_DRV_DBG_FORCE_BUS_READ            111
#define PT_DRV_DBG_CLEAR_CAL_DATA            112

/*
 * Commands that require additional parameters
 * will be in the 200 range. Commands that do not
 * require additional parameters remain below 200.
 */
#define PT_DRV_DBG_REPORT_LEVEL              200
#define PT_DRV_DBG_WATCHDOG_INTERVAL         201
#define PT_DRV_DBG_SHOW_TIMESTAMP            202
#define PT_DRV_DBG_SET_GENERATION            210

#ifdef TTDL_DIAGNOSTICS
#define PT_DRV_DBG_FLUSH_BUS                 204 /* deprecated */
#define PT_DRV_DBG_SETUP_PWR                 205
#define PT_DRV_DBG_GET_PUT_SYNC              206
#define PT_DRV_DBG_SET_PIP2_LAUNCH_APP       207 /* deprecated */
#define PT_DRV_DBG_SET_TT_DATA               208
#define PT_DRV_DBG_SET_RUN_FW_PIN            209 /* deprecated */
#define PT_DRV_DBG_SET_BRIDGE_MODE           211
#define PT_DRV_DBG_SET_I2C_ADDRESS           212
#define PT_DRV_DBG_SET_FLASHLESS_DUT         213
#define PT_DRV_DBG_SET_FORCE_SEQ             214
#define PT_DRV_DBG_BL_WITH_NO_INT            215
#define PT_DRV_DBG_CAL_CACHE_IN_HOST         216
#define PT_DRV_DBG_MULTI_CHIP                217
#define PT_DRV_DBG_SET_PANEL_ID_TYPE         218
#define PT_DRV_DBG_PIP_TIMEOUT               219
#define PT_DRV_DBG_TTHE_HID_USB_FORMAT       220
#ifdef TTDL_PTVIRTDUT_SUPPORT
#define PT_DRV_DBG_SET_HW_DETECT             298
#define PT_DRV_DBG_VIRTUAL_I2C_DUT           299
#endif /* TTDL_PTVIRTDUT_SUPPORT */

/* TTDL Built In Self Test selection bit masks */
#define PT_TTDL_BIST_BUS_TEST           0x01
#define PT_TTDL_BIST_IRQ_TEST           0x02
#define PT_TTDL_BIST_TP_XRES_TEST       0x04
#define PT_TTDL_BIST_SLAVE_BUS_TEST     0x08
#define PT_TTDL_BIST_SLAVE_IRQ_TEST     0x10
#define PT_TTDL_BIST_SLAVE_XRES_TEST    0x20

#define SLAVE_DETECT_MASK               0x01

#define VIRT_MAX_IRQ_RELEASE_TIME_US    500000
#endif /* TTDL DIAGNOSTICS */

/* Recognized usages */
/* undef them first for possible redefinition in Linux */
#undef HID_DI_PRESSURE
#undef HID_DI_TIP
#undef HID_DI_CONTACTID
#undef HID_DI_CONTACTCOUNT
#undef HID_DI_SCANTIME
#define HID_DI_PRESSURE                   0x000d0030
#define HID_DI_TIP                        0x000d0042
#define HID_DI_CONTACTID                  0x000d0051
#define HID_DI_CONTACTCOUNT               0x000d0054
#define HID_DI_SCANTIME                   0x000d0056

/* Parade vendor specific usages */
#define HID_PT_UNDEFINED                  0xff010000
#define HID_PT_BOOTLOADER                 0xff010001
#define HID_PT_TOUCHAPPLICATION           0xff010002
#define HID_PT_BUTTONS                    0xff010020
#define HID_PT_GENERICITEM                0xff010030
#define HID_PT_LARGEOBJECT                0xff010040
#define HID_PT_NOISEEFFECTS               0xff010041
#define HID_PT_REPORTCOUNTER              0xff010042
#define HID_PT_TOUCHTYPE                  0xff010060
#define HID_PT_EVENTID                    0xff010061
#define HID_PT_MAJORAXISLENGTH            0xff010062
#define HID_PT_MINORAXISLENGTH            0xff010063
#define HID_PT_ORIENTATION                0xff010064
#define HID_PT_BUTTONSIGNAL               0xff010065
#define HID_PT_MAJOR_CONTACT_AXIS_LENGTH  0xff010066
#define HID_PT_MINOR_CONTACT_AXIS_LENGTH  0xff010067
#define HID_PT_TCH_COL_USAGE_PG           0x000D0022
#define HID_PT_BTN_COL_USAGE_PG           0xFF010020

#define PANEL_ID_NOT_ENABLED	0xFF

#ifdef EASYWAKE_TSG6
#define GESTURE_DOUBLE_TAP         (1)
#define GESTURE_TWO_FINGERS_SLIDE  (2)
#define GESTURE_TOUCH_DETECTED     (3)
#define GESTURE_PUSH_BUTTON        (4)
#define GESTURE_SINGLE_SLIDE_DE_TX (5)
#define GESTURE_SINGLE_SLIDE_IN_TX (6)
#define GESTURE_SINGLE_SLIDE_DE_RX (7)
#define GESTURE_SINGLE_SLIDE_IN_RX (8)
#endif

/* FW RAM parameters */
#define PT_RAM_ID_TOUCHMODE_ENABLED	0x02
#define PT_RAM_ID_PROXIMITY_ENABLE	0x20
#define PT_RAM_ID_TOUCHMODE_ENABLED_SIZE	1
#define PT_RAM_ID_PROXIMITY_ENABLE_SIZE	1

/* abs signal capabilities offsets in the frameworks array */
enum pt_sig_caps {
	PT_SIGNAL_OST,
	PT_MIN_OST,
	PT_MAX_OST,
	PT_FUZZ_OST,
	PT_FLAT_OST,
	PT_NUM_ABS_SET	/* number of signal capability fields */
};

/* helpers */
#define NUM_SIGNALS(frmwrk)		((frmwrk)->size / PT_NUM_ABS_SET)
#define PARAM(frmwrk, sig_ost, cap_ost) \
		((frmwrk)->abs[((sig_ost) * PT_NUM_ABS_SET) + (cap_ost)])

#define PARAM_SIGNAL(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, PT_SIGNAL_OST)
#define PARAM_MIN(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, PT_MIN_OST)
#define PARAM_MAX(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, PT_MAX_OST)
#define PARAM_FUZZ(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, PT_FUZZ_OST)
#define PARAM_FLAT(frmwrk, sig_ost)	PARAM(frmwrk, sig_ost, PT_FLAT_OST)

/* abs axis signal offsets in the framworks array  */
enum pt_sig_ost {
	PT_ABS_X_OST,
	PT_ABS_Y_OST,
	PT_ABS_P_OST,
	PT_ABS_W_OST,
	PT_ABS_ID_OST,
	PT_ABS_MAJ_OST,
	PT_ABS_MIN_OST,
	PT_ABS_OR_OST,
	PT_ABS_TOOL_OST,
	PT_ABS_D_OST,
	PT_NUM_ABS_OST	/* number of abs signals */
};

enum hid_command {
	HID_CMD_RESERVED       = 0x0,
	HID_CMD_RESET          = 0x1,
	HID_CMD_GET_REPORT     = 0x2,
	HID_CMD_SET_REPORT     = 0x3,
	HID_CMD_GET_IDLE       = 0x4,
	HID_CMD_SET_IDLE       = 0x5,
	HID_CMD_GET_PROTOCOL   = 0x6,
	HID_CMD_SET_PROTOCOL   = 0x7,
	HID_CMD_SET_POWER      = 0x8,
	HID_CMD_VENDOR         = 0xE,
};

enum PIP1_cmd_type {
	PIP1_CMD_TYPE_FW,
	PIP1_CMD_TYPE_BL,
};

/* PIP BL cmd IDs and input for dut_debug sysfs */
enum pip1_bl_cmd_id {
	PIP1_BL_CMD_ID_VERIFY_APP_INTEGRITY     = 0x31, /* 49 */
	PIP1_BL_CMD_ID_GET_INFO                 = 0x38, /* 56 */
	PIP1_BL_CMD_ID_PROGRAM_AND_VERIFY       = 0x39, /* 57 */
	PIP1_BL_CMD_ID_LAUNCH_APP               = 0x3B, /* 59 */
	PIP1_BL_CMD_ID_GET_PANEL_ID             = 0x3E, /* 62 */
	PIP1_BL_CMD_ID_INITIATE_BL              = 0x48, /* 72 */
	PIP1_BL_CMD_ID_LAST,
};
#define PIP1_BL_SOP	0x1
#define PIP1_BL_EOP	0x17

/* PIP1 Command/Response IDs */
enum PIP1_CMD_ID {
	PIP1_CMD_ID_NULL                        = 0x00,
	PIP1_CMD_ID_START_BOOTLOADER            = 0x01,
	PIP1_CMD_ID_GET_SYSINFO                 = 0x02,
	PIP1_CMD_ID_SUSPEND_SCANNING            = 0x03,
	PIP1_CMD_ID_RESUME_SCANNING             = 0x04,
	PIP1_CMD_ID_GET_PARAM                   = 0x05,
	PIP1_CMD_ID_SET_PARAM                   = 0x06,
	PIP1_CMD_ID_GET_NOISE_METRICS           = 0x07,
	PIP1_CMD_ID_RESERVED                    = 0x08,
	PIP1_CMD_ID_ENTER_EASYWAKE_STATE        = 0x09,
	PIP1_CMD_ID_VERIFY_CONFIG_BLOCK_CRC     = 0x20,
	PIP1_CMD_ID_GET_CONFIG_ROW_SIZE         = 0x21,
	PIP1_CMD_ID_READ_DATA_BLOCK             = 0x22,
	PIP1_CMD_ID_WRITE_DATA_BLOCK            = 0x23,
	PIP1_CMD_ID_GET_DATA_STRUCTURE          = 0x24,
	PIP1_CMD_ID_LOAD_SELF_TEST_PARAM        = 0x25,
	PIP1_CMD_ID_RUN_SELF_TEST               = 0x26,
	PIP1_CMD_ID_GET_SELF_TEST_RESULT        = 0x27,
	PIP1_CMD_ID_CALIBRATE_IDACS             = 0x28,
	PIP1_CMD_ID_INITIALIZE_BASELINES        = 0x29,
	PIP1_CMD_ID_EXEC_PANEL_SCAN             = 0x2A,
	PIP1_CMD_ID_RETRIEVE_PANEL_SCAN         = 0x2B,
	PIP1_CMD_ID_START_SENSOR_DATA_MODE      = 0x2C,
	PIP1_CMD_ID_STOP_SENSOR_DATA_MODE       = 0x2D,
	PIP1_CMD_ID_START_TRACKING_HEATMAP_MODE = 0x2E,
	PIP1_CMD_ID_START_SELF_CAP_RPT_MODE     = 0x2F,
	PIP1_CMD_ID_CALIBRATE_DEVICE_EXTENDED   = 0x30,
	PIP1_CMD_ID_INT_PIN_OVERRIDE            = 0x40,
	PIP1_CMD_ID_STORE_PANEL_SCAN            = 0x60,
	PIP1_CMD_ID_PROCESS_PANEL_SCAN          = 0x61,
	PIP1_CMD_ID_DISCARD_INPUT_REPORT,
	PIP1_CMD_ID_LAST,
	PIP1_CMD_ID_USER_CMD,
};

/* PIP2 Command/Response data and structures */
enum PIP2_CMD_ID {
	PIP2_CMD_ID_PING                = 0x00,
	PIP2_CMD_ID_STATUS              = 0x01,
	PIP2_CMD_ID_CTRL                = 0x02,
	PIP2_CMD_ID_CONFIG              = 0x03,
	PIP2_CMD_ID_RESERVE             = 0x04,
	PIP2_CMD_ID_CLEAR               = 0x05,
	PIP2_CMD_ID_RESET               = 0x06,
	PIP2_CMD_ID_VERSION             = 0x07,
	PIP2_CMD_ID_FILE_OPEN           = 0x10,
	PIP2_CMD_ID_FILE_CLOSE          = 0x11,
	PIP2_CMD_ID_FILE_READ           = 0x12,
	PIP2_CMD_ID_FILE_WRITE          = 0x13,
	PIP2_CMD_ID_FILE_IOCTL          = 0x14,
	PIP2_CMD_ID_FLASH_INFO          = 0x15,
	PIP2_CMD_ID_EXECUTE             = 0x16,
	PIP2_CMD_ID_GET_LAST_ERRNO      = 0x17,
	PIP2_CMD_ID_EXIT_HOST_MODE      = 0x18,
	PIP2_CMD_ID_READ_GPIO           = 0x19,
	PIP2_CMD_EXECUTE_SCAN           = 0x21,
	PIP2_CMD_SET_PARAMETER          = 0x40,
	PIP2_CMD_GET_PARAMETER          = 0x41,
	PIP2_CMD_SET_DDI_REG            = 0x42,
	PIP2_CMD_GET_DDI_REG            = 0x43,
	PIP2_CMD_ID_END                 = 0x7F
};

enum PIP2_STATUS_EXEC_RUNNING {
	PIP2_STATUS_BOOT_EXEC           = 0x00,
	PIP2_STATUS_APP_EXEC            = 0x01,
};

/* FW_SYS_MODE_UNDEFINED must be 1 greater than FW_SYS_MODE_MAX */
enum PIP2_FW_SYSTEM_MODE {
	FW_SYS_MODE_BOOT                = 0x00,
	FW_SYS_MODE_SCANNING            = 0x01,
	FW_SYS_MODE_DEEP_SLEEP          = 0x02,
	FW_SYS_MODE_TEST                = 0x03,
	FW_SYS_MODE_DEEP_STANDBY        = 0x04,
	FW_SYS_MODE_MAX                 = FW_SYS_MODE_DEEP_STANDBY,
	FW_SYS_MODE_UNDEFINED           = FW_SYS_MODE_MAX + 1,
};

/* PIP2 Command/Response data and structures */
enum PIP2_FILE_ID {
	PIP2_RAM_FILE			= 0x00,
	PIP2_FW_FILE			= 0x01,
	PIP2_CONFIG_FILE		= 0x02,
	PIP2_FILE_3			= 0x03,
	PIP2_FILE_4			= 0x04,
	PIP2_FILE_5			= 0x05,
	PIP2_FILE_6			= 0x06,
	PIP2_FILE_7			= 0x07,
	PIP2_FILE_MAX			= PIP2_FILE_7,
};

/* Optimize packet sizes per Allwinner H3 bus drivers */
#define PIP2_FILE_WRITE_LEN_PER_PACKET		245
#define PIP2_BL_I2C_FILE_WRITE_LEN_PER_PACKET	245
#define PIP2_BL_SPI_FILE_WRITE_LEN_PER_PACKET	256

enum DUT_GENERATION {
	DUT_UNKNOWN                     = 0x00,
	DUT_PIP1_ONLY			= 0x01,
	DUT_PIP2_CAPABLE		= 0x02,
};

enum PIP2_RSP_ERR {
	PIP2_RSP_ERR_NONE               = 0x00,
	PIP2_RSP_ERR_BUSY               = 0x01,
	PIP2_RSP_ERR_INIT_FAILURE       = 0x02,
	PIP2_RSP_ERR_ALREADY_OPEN       = 0x03,
	PIP2_RSP_ERR_NOT_OPEN           = 0x04,
	PIP2_RSP_ERR_IO_FAILURE         = 0x05,
	PIP2_RSP_ERR_UNKNOWN_IOCTL      = 0x06,
	PIP2_RSP_ERR_BAD_ADDRESS        = 0x07,
	PIP2_RSP_ERR_BAD_FILE           = 0x08,
	PIP2_RSP_ERR_END_OF_FILE        = 0x09,
	PIP2_RSP_ERR_TOO_MANY_FILES     = 0x0A,
	PIP2_RSP_ERR_TIMEOUT            = 0x0B,
	PIP2_RSP_ERR_ABORTED            = 0x0C,
	PIP2_RSP_ERR_CRC                = 0x0D,
	PIP2_RSP_ERR_UNKNOWN_REC_TYPE   = 0x0E,
	PIP2_RSP_ERR_BAD_FRAME          = 0x0F,
	PIP2_RSP_ERR_NO_PERMISSION      = 0x10,
	PIP2_RSP_ERR_UNKNOWN_COMMAND    = 0x11,
	PIP2_RSP_ERR_INVALID_PARAM      = 0x12,
	PIP2_RSP_ERR_IO_ALREADY_ACTIVE  = 0x13,
	PIP2_RSP_ERR_SHUTDOWN           = 0x14,
	PIP2_RSP_ERR_INVALID_IMAGE      = 0x15,
	PIP2_RSP_ERR_UNKNOWN_REGISTER   = 0x16,
	PIP2_RSP_ERR_BAD_LENGTH         = 0x17,
	PIP2_RSP_ERR_TRIM_FAILURE       = 0x18,
};

/*
 * Extra bytes for PIP2 = 4 + 2:
 * 4 byte header - (len_lsb, len_msb, report ID, Tag, Sequence)
 * 2 byte footer - (crc_lsb, crc_msb)
 */
#define PIP2_CMD_COMMAND_ID_OFFSET           5
#define PIP2_CMD_COMMAND_ID_MASK          0x7F
#define PIP2_RESP_COMMAND_ID_OFFSET          3
#define PIP2_RESP_SEQUENCE_OFFSET            2
#define PIP2_RESP_SEQUENCE_MASK           0x0F
#define PIP2_RESP_REPORT_ID_OFFSET           3
#define PIP2_RESP_STATUS_OFFSET              4
#define PIP2_RESP_BODY_OFFSET                5
#define PIP2_CRC_SIZE                        2
#define PIP2_LEN_FIELD_SIZE                  2
#define PIP2_VERSION_CHIP_REV_OFFSET        14
#define PIP2_EXTRA_BYTES_NUM (PIP2_RESP_STATUS_OFFSET + PIP2_CRC_SIZE)

/* File IOCTL commands */
#define PIP2_FILE_IOCTL_CODE_ERASE_FILE		0
#define PIP2_FILE_IOCTL_CODE_SEEK_POINTER	1
#define PIP2_FILE_IOCTL_CODE_AES_CONTROL	2
#define PIP2_FILE_IOCTL_CODE_FILE_STATS		3
#define PIP2_FILE_IOCTL_CODE_FILE_CRC		4

struct pip2_cmd_structure {
	u8 reg[2];
	u16 len;
	u8 id;
	u8 seq;
	u8 *data;
	u8 crc[2]; /* MSB:crc[0], LSB:crc[1] */
};

struct pip2_cmd_response_structure {
	u8 id;
	u16 response_len;
	u32 response_time_min;
	u32 response_time_max;
};

enum pip1_bl_status {
	ERROR_SUCCESS,
	ERROR_KEY,
	ERROR_VERIFICATION,
	ERROR_LENGTH,
	ERROR_DATA,
	ERROR_COMMAND,
	ERROR_CRC                = 8,
	ERROR_FLASH_ARRAY,
	ERROR_FLASH_ROW,
	ERROR_FLASH_PROTECTION,
	ERROR_UKNOWN             = 15,
	ERROR_INVALID,
};

enum pt_mode {
	PT_MODE_UNKNOWN      = 0,
	PT_MODE_BOOTLOADER   = 1,
	PT_MODE_OPERATIONAL  = 2,
	PT_MODE_IGNORE       = 255,
};

enum PT_ENTER_BL_RESULT {
	PT_ENTER_BL_PASS                = 0,
	PT_ENTER_BL_ERROR               = 1,
	PT_ENTER_BL_RESET_FAIL          = 2,
	PT_ENTER_BL_HID_START_BL_FAIL   = 3,
	PT_ENTER_BL_CONFIRM_FAIL        = 4,
	PT_ENTER_BL_GET_FLASH_INFO_FAIL = 5,
};

enum TTDL_EXTENDED_ERROR_CODES {
	EX_ERR_FREAD            = 400,
	EX_ERR_FWRITE           = 401,
	EX_ERR_FOPEN            = 402,
	EX_ERR_FCLOSE           = 403,
	EX_ERR_FLEN             = 404,
};

enum pt_cmd_status {
	PT_CMD_STATUS_SUCCESS        = 0,
	PT_CMD_STATUS_FAILURE        = 1,
};

enum {
	PT_IC_GRPNUM_RESERVED,
	PT_IC_GRPNUM_CMD_REGS,
	PT_IC_GRPNUM_TCH_REP,
	PT_IC_GRPNUM_DATA_REC,
	PT_IC_GRPNUM_TEST_REC,
	PT_IC_GRPNUM_PCFG_REC,
	PT_IC_GRPNUM_TCH_PARM_VAL,
	PT_IC_GRPNUM_TCH_PARM_SIZE,
	PT_IC_GRPNUM_RESERVED1,
	PT_IC_GRPNUM_RESERVED2,
	PT_IC_GRPNUM_OPCFG_REC,
	PT_IC_GRPNUM_DDATA_REC,
	PT_IC_GRPNUM_MDATA_REC,
	PT_IC_GRPNUM_TEST_REGS,
	PT_IC_GRPNUM_BTN_KEYS,
	PT_IC_GRPNUM_TTHE_REGS,
	PT_IC_GRPNUM_SENSING_CONF,
	PT_IC_GRPNUM_NUM,
};

enum pt_event_id {
	PT_EV_NO_EVENT,
	PT_EV_TOUCHDOWN,
	PT_EV_MOVE,		/* significant displacement (> act dist) */
	PT_EV_LIFTOFF,		/* record reports last position */
};

enum pt_object_id {
	PT_OBJ_STANDARD_FINGER,
	PT_OBJ_PROXIMITY,
	PT_OBJ_STYLUS,
	PT_OBJ_GLOVE,
};

enum pt_self_test_result {
	PT_ST_RESULT_PASS                = 0,
	PT_ST_RESULT_FAIL                = 1,
	PT_ST_RESULT_ABORTED             = 2,
	PT_ST_RESULT_PARAM_ERR           = 3,
	PT_ST_RESULT_CFG_ERR             = 4,
	PT_ST_RESULT_CAL_ERR             = 5,
	PT_ST_RESULT_DDI_STATE_ERR       = 6,
	PT_ST_RESULT_HOST_MUST_INTERPRET = 0xFF,
};
#define PT_ST_PRINT_RESULTS    true
#define PT_ST_NOPRINT          false
#define PT_ST_GET_RESULTS      true
#define PT_ST_DONT_GET_RESULTS false

/*
 * Maximum number of parameters for the fw_self_test sysfs (255 - 12 + 2)
 * 255 - Max PIP message size
 *  12 - Header size for PIP message 0x25 (Load Self Test Parameters)
 *   2 - Additional parameters for fw_self_test for test_id and format
 */
#define PT_FW_SELF_TEST_MAX_PARM 245

enum pt_self_test_id {
	PT_ST_ID_NULL                        = 0,
	PT_ST_ID_BIST                        = 1,
	PT_ST_ID_SHORTS                      = 2,
	PT_ST_ID_OPENS                       = 3,
	PT_ST_ID_AUTOSHORTS                  = 4,
	PT_ST_ID_CM_PANEL                    = 5,
	PT_ST_ID_CP_PANEL                    = 6,
	PT_ST_ID_CM_BUTTON                   = 7,
	PT_ST_ID_CP_BUTTON                   = 8,
	PT_ST_ID_FORCE                       = 9,
	PT_ST_ID_OPENS_HIZ                   = 10,
	PT_ST_ID_OPENS_GND                   = 11,
	PT_ST_ID_CP_LFT                      = 12,
	PT_ST_ID_SC_NOISE                    = 13,
	PT_ST_ID_LFT_NOISE                   = 14,
	PT_ST_ID_CP_CHIP_ROUTE_PARASITIC_CAP = 15,
	PT_ST_ID_NORMALIZED_RAW_CNT_PANEL    = 16,
	PT_ST_ID_NORMALIZED_RAW_CNT_LFT      = 17,
	PT_ST_ID_INVALID                     = 255
};

enum pt_scan_state {
	PT_SCAN_STATE_UNKNOWN    = 0,
	PT_SCAN_STATE_ACTIVE     = 1,
	PT_SCAN_STATE_INACTIVE   = 2,
};

#define PT_CAL_DATA_MAX_SIZE    2048
#define PT_CAL_DATA_ROW_SIZE     128
#define PT_WAFER_LOT_SIZE          5
#define PT_UID_SIZE               12

enum pt_cal_data_actions {
	PT_CAL_DATA_SAVE         = 0,
	PT_CAL_DATA_RESTORE      = 1,
	PT_CAL_DATA_CLEAR        = 2,
	PT_CAL_DATA_INFO         = 3
};

enum pt_feature_enable_state {
	PT_FEATURE_DISABLE     = 0,
	PT_FEATURE_ENABLE      = 1
};

#define PT_NUM_MFGID               8
/* System Information interface definitions */
struct pt_ttdata_dev {
	u8 pip_ver_major;
	u8 pip_ver_minor;
	__le16 fw_pid;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	__le32 revctrl;
	__le16 fw_ver_conf;
	u8 bl_ver_major;
	u8 bl_ver_minor;
	__le16 jtag_si_id_l;
	__le16 jtag_si_id_h;
	u8 mfg_id[PT_NUM_MFGID];
	__le16 post_code;
} __packed;

/* Struct to cast over PIP2 VERSION response */
struct pt_pip2_version_full {
	u8 status_code;
	u8 pip2_version_lsb;
	u8 pip2_version_msb;
	u8 fw_version_lsb;
	u8 fw_version_msb;
	u8 bl_version_lsb;
	u8 bl_version_msb;
	__le16 chip_rev;
	__le16 chip_id;
	u8 uid[PT_UID_SIZE];
} __packed;

struct pt_pip2_version {
	u8 status_code;
	u8 pip2_version_lsb;
	u8 pip2_version_msb;
	u8 bl_version_lsb;
	u8 bl_version_msb;
	u8 fw_version_lsb;
	u8 fw_version_msb;
	__le16 chip_id;
	__le16 chip_rev;
} __packed;

struct pt_sensing_conf_data_dev {
	u8 electrodes_x;
	u8 electrodes_y;
	__le16 len_x;
	__le16 len_y;
	__le16 res_x;
	__le16 res_y;
	__le16 max_z;
	u8 origin_x;
	u8 origin_y;
	u8 panel_id;
	u8 btn;
	u8 scan_mode;
	u8 max_num_of_tch_per_refresh_cycle;
} __packed;

struct pt_ttdata {
	u8 pip_ver_major;
	u8 pip_ver_minor;
	u8 bl_ver_major;
	u8 bl_ver_minor;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u16 fw_pid;
	u16 fw_ver_conf;
	u16 post_code;
	u32 revctrl;
	u16 jtag_id_l;
	u16 jtag_id_h;
	u8 mfg_id[PT_NUM_MFGID];
	u16 chip_rev;
	u16 chip_id;
	u8 uid[PT_UID_SIZE];
};

struct pt_sensing_conf_data {
	u16 res_x;
	u16 res_y;
	u16 max_z;
	u16 len_x;
	u16 len_y;
	u8 electrodes_x;
	u8 electrodes_y;
	u8 origin_x;
	u8 origin_y;
	u8 panel_id;
	u8 btn;
	u8 scan_mode;
	u8 max_tch;
	u8 rx_num;
	u8 tx_num;
};

enum pt_tch_abs {	/* for ordering within the extracted touch data array */
	PT_TCH_X,	/* X */
	PT_TCH_Y,	/* Y */
	PT_TCH_P,	/* P (Z) */
	PT_TCH_T,	/* TOUCH ID */
	PT_TCH_E,	/* EVENT ID */
	PT_TCH_O,	/* OBJECT ID */
	PT_TCH_TIP,	/* OBJECT ID */
	PT_TCH_MAJ,	/* TOUCH_MAJOR */
	PT_TCH_MIN,	/* TOUCH_MINOR */
	PT_TCH_OR,	/* ORIENTATION */
	PT_TCH_NUM_ABS,
};

enum pt_tch_hdr {
	PT_TCH_TIME,	/* SCAN TIME */
	PT_TCH_NUM,	/* NUMBER OF RECORDS */
	PT_TCH_LO,	/* LARGE OBJECT */
	PT_TCH_NOISE,	/* NOISE EFFECT */
	PT_TCH_COUNTER,	/* REPORT_COUNTER */
	PT_TCH_NUM_HDR,
};

static const char * const pt_tch_abs_string[] = {
	[PT_TCH_X]	= "X",
	[PT_TCH_Y]	= "Y",
	[PT_TCH_P]	= "P",
	[PT_TCH_T]	= "T",
	[PT_TCH_E]	= "E",
	[PT_TCH_O]	= "O",
	[PT_TCH_TIP]	= "TIP",
	[PT_TCH_MAJ]	= "MAJ",
	[PT_TCH_MIN]	= "MIN",
	[PT_TCH_OR]	= "OR",
	[PT_TCH_NUM_ABS] = "INVALID",
};

static const char * const pt_tch_hdr_string[] = {
	[PT_TCH_TIME]	= "SCAN TIME",
	[PT_TCH_NUM]	= "NUMBER OF RECORDS",
	[PT_TCH_LO]	= "LARGE OBJECT",
	[PT_TCH_NOISE]	= "NOISE EFFECT",
	[PT_TCH_COUNTER] = "REPORT_COUNTER",
	[PT_TCH_NUM_HDR] = "INVALID",
};

static const int pt_tch_abs_field_map[] = {
	[PT_TCH_X]	= 0x00010030 /* HID_GD_X */,
	[PT_TCH_Y]	= 0x00010031 /* HID_GD_Y */,
	[PT_TCH_P]	= HID_DI_PRESSURE,
	[PT_TCH_T]	= HID_DI_CONTACTID,
	[PT_TCH_E]	= HID_PT_EVENTID,
	[PT_TCH_O]	= HID_PT_TOUCHTYPE,
	[PT_TCH_TIP]	= HID_DI_TIP,
	[PT_TCH_MAJ]	= HID_PT_MAJORAXISLENGTH,
	[PT_TCH_MIN]	= HID_PT_MINORAXISLENGTH,
	[PT_TCH_OR]	= HID_PT_ORIENTATION,
	[PT_TCH_NUM_ABS] = 0,
};

static const int pt_tch_hdr_field_map[] = {
	[PT_TCH_TIME]	= HID_DI_SCANTIME,
	[PT_TCH_NUM]	= HID_DI_CONTACTCOUNT,
	[PT_TCH_LO]	= HID_PT_LARGEOBJECT,
	[PT_TCH_NOISE]	= HID_PT_NOISEEFFECTS,
	[PT_TCH_COUNTER] = HID_PT_REPORTCOUNTER,
	[PT_TCH_NUM_HDR] = 0,
};

#define PT_NUM_EXT_TCH_FIELDS   3

struct pt_tch_abs_params {
	size_t ofs;	/* abs byte offset */
	size_t size;	/* size in bits */
	size_t min;	/* min value */
	size_t max;	/* max value */
	size_t bofs;	/* bit offset */
	u8 report;
};

struct pt_touch {
	int hdr[PT_TCH_NUM_HDR];
	int abs[PT_TCH_NUM_ABS];
};

/* button to keycode support */
#define PT_BITS_PER_BTN		1
#define PT_NUM_BTN_EVENT_ID	((1 << PT_BITS_PER_BTN) - 1)

enum pt_btn_state {
	PT_BTN_RELEASED = 0,
	PT_BTN_PRESSED = 1,
	PT_BTN_NUM_STATE
};

struct pt_btn {
	bool enabled;
	int state;	/* PT_BTN_PRESSED, PT_BTN_RELEASED */
	int key_code;
};

enum pt_ic_ebid {
	PT_TCH_PARM_EBID  = 0x00,
	PT_MDATA_EBID     = 0x01,
	PT_DDATA_EBID     = 0x02,
	PT_CAL_EBID       = 0xF0,
};

/* ttconfig block */
#define PT_TTCONFIG_VERSION_OFFSET	8
#define PT_TTCONFIG_VERSION_SIZE	2
#define PT_TTCONFIG_VERSION_ROW		0

struct pt_ttconfig {
	u16 version;
	u16 crc;
};

struct pt_report_desc_data {
	u16 tch_report_id;
	u16 tch_record_size;
	u16 tch_header_size;
	u16 btn_report_id;
};

struct pt_sysinfo {
	bool ready;
	struct pt_ttdata ttdata;
	struct pt_sensing_conf_data sensing_conf_data;
	struct pt_report_desc_data desc;
	int num_btns;
	struct pt_btn *btn;
	struct pt_ttconfig ttconfig;
	struct pt_tch_abs_params tch_hdr[PT_TCH_NUM_HDR];
	struct pt_tch_abs_params tch_abs[PT_TCH_NUM_ABS];
	u8 *xy_mode;
	u8 *xy_data;
};

struct pt_bl_info {
	bool ready;
	u16 chip_id;
};

enum pt_atten_type {
	PT_ATTEN_IRQ,
	PT_ATTEN_STARTUP,
	PT_ATTEN_EXCLUSIVE,
	PT_ATTEN_WAKE,
	PT_ATTEN_LOADER,
	PT_ATTEN_SUSPEND,
	PT_ATTEN_RESUME,
	PT_ATTEN_CANCEL_LOADER,
	PT_ATTEN_NUM_ATTEN,
};

enum pt_sleep_state {
	SS_SLEEP_OFF,
	SS_SLEEP_ON,
	SS_SLEEPING,
	SS_WAKING,
};

enum pt_fb_state {
	FB_ON,
	FB_OFF,
};

enum pt_startup_state {
	STARTUP_NONE,
	STARTUP_QUEUED,
	STARTUP_RUNNING,
	STARTUP_ILLEGAL,
};

struct pt_hid_desc {
	__le16 hid_desc_len;
	u8 packet_id;
	u8 reserved_byte;
	__le16 bcd_version;
	__le16 report_desc_len;
	__le16 report_desc_register;
	__le16 input_register;
	__le16 max_input_len;
	__le16 output_register;
	__le16 max_output_len;
	__le16 command_register;
	__le16 data_register;
	__le16 vendor_id;
	__le16 product_id;
	__le16 version_id;
	u8 reserved[4];
} __packed;

struct pt_hid_core {
	u16 hid_vendor_id;
	u16 hid_product_id;
	__le16 hid_desc_register;
	u16 hid_report_desc_len;
	u16 hid_max_input_len;
	u16 hid_max_output_len;
};

#define PT_HID_MAX_REPORTS		8
#define PT_HID_MAX_FIELDS		128
#define PT_HID_MAX_COLLECTIONS		3
#define PT_HID_MAX_NESTED_COLLECTIONS	PT_HID_MAX_COLLECTIONS

/* Max input is for ASCII representation of hex characters */
#define PT_MAX_INPUT            (PT_MAX_PIP2_MSG_SIZE * 2)
#define PT_PIP_1P7_EMPTY_BUF    0xFF00

enum pt_module_id {
	PT_MODULE_MT,
	PT_MODULE_BTN,
	PT_MODULE_PROX,
	PT_MODULE_LAST,
};

struct pt_mt_data;
struct pt_mt_function {
	int (*mt_release)(struct device *dev);
	int (*mt_probe)(struct device *dev, struct pt_mt_data *md);
	void (*report_slot_liftoff)(struct pt_mt_data *md, int max_slots);
	void (*input_sync)(struct input_dev *input);
	void (*input_report)(struct input_dev *input, int sig, int t, int type);
	void (*final_sync)(struct input_dev *input, int max_slots,
			int mt_sync_count, unsigned long *ids);
	int (*input_register_device)(struct input_dev *input, int max_slots);
};

struct pt_mt_data {
	struct device *dev;
	struct pt_mt_platform_data *pdata;
	struct pt_sysinfo *si;
	struct input_dev *input;
	struct pt_mt_function mt_function;
	struct mutex mt_lock;
	bool is_suspended;
	bool input_device_registered;
	bool input_device_allocated;
	char phys[NAME_MAX];
	int num_prv_rec;
	int or_min;
	int or_max;
	int t_min;
	int t_max;
};

struct pt_btn_data {
	struct device *dev;
	struct pt_btn_platform_data *pdata;
	struct pt_sysinfo *si;
	struct input_dev *input;
	struct mutex btn_lock;
	bool is_suspended;
	bool input_device_registered;
	bool input_device_allocated;
	char phys[NAME_MAX];
};

struct pt_proximity_data {
	struct device *dev;
	struct pt_proximity_platform_data *pdata;
	struct pt_sysinfo *si;
	struct input_dev *input;
	struct mutex prox_lock;
	struct mutex sysfs_lock;
	int enable_count;
	bool input_device_registered;
	bool input_device_allocated;
	char phys[NAME_MAX];
};

enum pt_calibrate_idacs_sensing_mode {
	PT_CI_SM_MUTCAP_FINE,
	PT_CI_SM_MUTCAP_BUTTON,
	PT_CI_SM_SELFCAP,
};

enum pt_initialize_baselines_sensing_mode {
	PT_IB_SM_MUTCAP = 1,
	PT_IB_SM_BUTTON = 2,
	PT_IB_SM_SELFCAP = 4,
	PT_IB_SM_BALANCED = 8,
};

/* parameters for extended calibrate command(0x30)*/
struct pt_cal_ext_data {
	u8 mode;
	u8 data0;
	u8 data1;
	u8 data2;
} __packed;
#define PT_CAL_EXT_MODE_UNDEFINED  0xFF

#define PT_BIN_FILE_MIN_HDR_LENGTH 14
#define PT_BIN_FILE_MAX_HDR_LENGTH 18
struct pt_bin_file_hdr {
	u8 length;
	u16 ttpid;
	u8  fw_major;
	u8  fw_minor;
	u32 fw_rev_ctrl;
	u32 fw_crc;
	u16 si_rev;
	u16 si_id;
	u16 config_ver;
	u32 hex_file_size;
};

struct pt_core_nonhid_cmd {
	int (*start_bl)(struct device *dev, int protect);
	int (*suspend_scanning)(struct device *dev, int protect);
	int (*resume_scanning)(struct device *dev, int protect);
	int (*get_param)(struct device *dev, int protect, u8 param_id,
			u32 *value);
	int (*set_param)(struct device *dev, int protect, u8 param_id,
			u32 value, u8 size);
	int (*verify_cfg_block_crc)(struct device *dev, int protect,
			u8 ebid, u8 *status, u16 *calculated_crc,
			u16 *stored_crc);
	int (*get_config_row_size)(struct device *dev, int protect,
			u16 *row_size);
	int (*get_data_structure)(struct device *dev, int protect,
			u16 read_offset, u16 read_length, u8 data_id,
			u8 *status, u8 *data_format, u16 *actual_read_len,
			u8 *data);
	int (*run_selftest)(struct device *dev, int protect, u8 test_id,
		u8 write_idacs_to_flash, u8 *status, u8 *summary_result,
		u8 *results_available);
	int (*get_selftest_result)(struct device *dev, int protect,
		u16 read_offset, u16 read_length, u8 test_id, u8 *status,
		u16 *actual_read_len, u8 *data);
	int (*load_self_test_param)(struct device *dev, int protect,
		u8 self_test_id, u16 load_offset, u16 load_length,
		u8 *parameters, u8 *status, u8 *ret_test_id, u16 *act_load_len);
	int (*calibrate_idacs)(struct device *dev, int protect, u8 mode,
			u8 *status);
	int (*calibrate_ext)(struct device *dev,
		int protect, struct pt_cal_ext_data *cal_data, u8 *status);
	int (*initialize_baselines)(struct device *dev, int protect,
			u8 test_id, u8 *status);
	int (*exec_panel_scan)(struct device *dev, int protect, u8 scan_type);
	int (*retrieve_panel_scan)(struct device *dev, int protect,
			u16 read_offset, u16 read_count, u8 data_id,
			u8 *response, u8 *config, u16 *actual_read_len,
			u8 *read_buf);
	int (*read_data_block)(struct device *dev, u16 row_number,
			u16 length, u8 ebid, u16 *actual_read_len,
			u8 *read_buf, u16 read_buf_size, u16 *crc);
	int (*write_data_block)(struct device *dev, u16 row_number,
			u16 write_length, u8 ebid, u8 *write_buf,
			u8 *security_key, u16 *actual_write_len);
	int (*user_cmd)(struct device *dev, int protect, u16 read_len,
			u8 *read_buf, u16 write_len, u8 *write_buf,
			u16 *actual_read_len);
	int (*get_bl_info)(struct device *dev, int protect, u8 *return_data);
	int (*initiate_bl)(struct device *dev, int protect, u16 key_size,
			u8 *key_buf, u16 row_size, u8 *metadata_row_buf);
	int (*launch_app)(struct device *dev, int protect);
	int (*prog_and_verify)(struct device *dev, int protect, u16 data_len,
			u8 *data_buf);
	int (*verify_app_integrity)(struct device *dev, int protect,
			u8 *result);
	int (*get_panel_id)(struct device *dev, int protect, u8 *panel_id);
	int (*pip2_send_cmd)(struct device *dev, int protect,
			u8 id, u8 *data, u16 report_body_len, u8 *read_buf,
			u16 *actual_read_len);
	int (*pip2_send_cmd_no_int)(struct device *dev, int protect,
			u8 id, u8 *data, u16 report_body_len, u8 *read_buf,
			u16 *actual_read_len);
	int (*get_bl_pip2_version)(struct device *dev);
	int (*pip2_file_open)(struct device *dev, u8 file_no);
	int (*pip2_file_close)(struct device *dev, u8 file_no);
	int (*pip2_file_erase)(struct device *dev, u8 file_no, int *status);
	int (*read_us_file)(struct device *dev, u8 *file_path, u8 *buf,
			int *size);
	int (*pip2_file_read)(struct device *dev, u8 file_no,
			u16 num_bytes, u8 *read_buf);
	int (*pip2_file_seek_offset)(struct device *dev, u8 file_no,
			u32 read_offset, u32 write_offset);
	int (*pip2_file_get_stats)(struct device *dev, u8 file_no,
			u32 *address, u32 *file_size);
	int (*pip2_file_crc)(struct device *dev, u8 file_no,
			u32 offset, u32 length, u8 *read_buf);
	int (*manage_cal_data)(struct device *dev, u8 action, u16 *size,
			unsigned short *crc);
	unsigned short (*calc_crc)(unsigned char *q, int len);
};

typedef int (*pt_atten_func) (struct device *);

struct pt_core_commands {
	int (*subscribe_attention)(struct device *dev,
			enum pt_atten_type type, char *id,
			pt_atten_func func, int flags);
	int (*unsubscribe_attention)(struct device *dev,
			enum pt_atten_type type, char *id,
			pt_atten_func func, int flags);
	int (*request_exclusive)(struct device *dev, int timeout_ms);
	int (*release_exclusive)(struct device *dev);
	int (*request_reset)(struct device *dev, int protect);
	int (*request_pip2_launch_app)(struct device *dev, int protect);
	int (*request_enum)(struct device *dev, bool wait);
	struct pt_sysinfo * (*request_sysinfo)(struct device *dev);
	struct pt_loader_platform_data
		*(*request_loader_pdata)(struct device *dev);
	int (*request_stop_wd)(struct device *dev);
	int (*request_start_wd)(struct device *dev);
	int (*request_get_mode)(struct device *dev, int protect, u8 *mode);
	int (*request_pip2_get_mode_sysmode)(struct device *dev, int protect,
		u8 *mode, u8 *sys_mode);
	int (*request_active_pip_prot)(struct device *dev, int protect,
		u8 *pip_version_major, u8 *pip_version_minor);
	int (*request_enable_scan_type)(struct device *dev, u8 scan_type);
	int (*request_disable_scan_type)(struct device *dev, u8 scan_type);
	int (*request_pip2_enter_bl)(struct device *dev, u8 *start_mode,
		int *result);
	int (*request_pip2_bin_hdr)(struct device *dev,
		struct pt_bin_file_hdr *hdr);
	int (*request_dut_generation)(struct device *dev);
	int (*request_hw_version)(struct device *dev, char *hw_version);
	int (*parse_sysfs_input)(struct device *dev,
		const char *buf, size_t buf_size,
		u32 *out_buf, size_t out_buf_size);
#ifdef TTHE_TUNER_SUPPORT
	int (*request_tthe_print)(struct device *dev, u8 *buf, int buf_len,
			const u8 *data_name);
#endif
#ifdef TTDL_DIAGNOSTICS
	void (*request_toggle_err_gpio)(struct device *dev, u8 type);
#endif
	struct pt_core_nonhid_cmd *nonhid_cmd;
	int (*request_get_fw_mode)(struct device *dev, int protect,
		u8 *sys_mode, u8 *mode);
};

enum core_command_protected_status {
	PT_CORE_CMD_UNPROTECTED = 0,
	PT_CORE_CMD_PROTECTED = 1
};

enum pt_err_gpio_type {
	PT_ERR_GPIO_NONE             = 0,
	PT_ERR_GPIO_I2C_TRANS        = 1,
	PT_ERR_GPIO_IRQ_STUCK        = 2,
	PT_ERR_GPIO_EXCLUSIVE_ACCESS = 3,
	PT_ERR_GPIO_EMPTY_PACKET     = 4,
	PT_ERR_GPIO_BL_RETRY_PACKET  = 5,
	PT_ERR_GPIO_MAX_TYPE         = PT_ERR_GPIO_BL_RETRY_PACKET,
};

struct pt_features {
	uint8_t easywake;
	uint8_t noise_metric;
	uint8_t tracking_heatmap;
	uint8_t sensor_data;
};

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_PM_RUNTIME)
#if (KERNEL_VERSION(3, 3, 0) > LINUX_VERSION_CODE)
#define NEED_SUSPEND_NOTIFIER
#endif /* CONFIG_PM_SLEEP && CONFIG_PM_RUNTIME */
#endif /* LINUX_VERSION_CODE */

struct pt_module {
	struct list_head node;
	char *name;
	int (*probe)(struct device *dev, void **data);
	void (*release)(struct device *dev, void *data);
};

struct pt_bus_ops {
	u16 bustype;
	int (*read_default)(struct device *dev, void *buf, int size);
	int (*read_default_nosize)(struct device *dev, u8 *buf, u32 max);
	int (*write_read_specific)(struct device *dev, u16 write_len,
			u8 *write_buf, u8 *read_buf);
};

struct pt_core_data {
	struct list_head node;
	struct list_head module_list; /* List of probed modules */
	char core_id[20];
	struct device *dev;
	struct list_head atten_list[PT_ATTEN_NUM_ATTEN];
	struct list_head param_list;
	struct mutex module_list_lock;
	struct mutex system_lock;
	struct mutex sysfs_lock;
	struct mutex ttdl_restart_lock;
	struct mutex firmware_class_lock;
	enum pt_mode mode;
	spinlock_t spinlock;
	struct pt_mt_data md;
	struct pt_btn_data bd;
	struct pt_proximity_data pd;
	int phys_num;
	int pip_cmd_timeout;
	int pip_cmd_timeout_default;
	void *pt_dynamic_data[PT_MODULE_LAST];
	struct pt_platform_data *pdata;
	struct pt_core_platform_data *cpdata;
	const struct pt_bus_ops *bus_ops;
	wait_queue_head_t wait_q;
	enum pt_sleep_state sleep_state;
	enum pt_startup_state startup_state;
	int irq;
	bool irq_enabled;
	bool irq_wake;
	bool irq_disabled;
	bool hw_detected;
	u8 easy_wakeup_gesture;
#ifdef EASYWAKE_TSG6
	u8 gesture_id;
	u8 gesture_data_length;
	u8 gesture_data[80];
#endif
	bool wait_until_wake;
	u8 pid_for_loader;
	char hw_version[13];
#ifdef NEED_SUSPEND_NOTIFIER
	/*
	 * This notifier is used to receive suspend prepare events
	 * When device is PM runtime suspended, pm_generic_suspend()
	 * does not call our PM suspend callback for kernels with
	 * version less than 3.3.0.
	 */
	struct notifier_block pm_notifier;
#endif
	struct pt_sysinfo sysinfo;
	struct pt_bl_info bl_info;
	void *exclusive_dev;
	int exclusive_waits;
	struct timer_list watchdog_timer;
	struct work_struct watchdog_work;
	struct work_struct enum_work;
	struct work_struct ttdl_restart_work;
	u16 startup_retry_count;
	struct pt_hid_core hid_core;
	int hid_cmd_state;
	int hid_reset_cmd_state; /* reset can happen any time */
	struct pt_hid_desc hid_desc;
	struct pt_features features;
#define PT_PREALLOCATED_CMD_BUFFER 32
	u8 cmd_buf[PT_PREALLOCATED_CMD_BUFFER];
	u8 input_buf[PT_MAX_INPUT];
	u8 response_buf[PT_MAX_INPUT];
	u8 cmd_rsp_buf[PT_MAX_INPUT];
	u16 cmd_rsp_buf_len;
	int raw_cmd_status;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es;
#elif defined(CONFIG_FB)
	struct notifier_block fb_notifier;
	enum pt_fb_state fb_state;
#endif
#ifdef TTHE_TUNER_SUPPORT
	struct dentry *tthe_debugfs;
	u8 *tthe_buf;
	u32 tthe_buf_len;
	u32 tthe_buf_size;
	struct mutex tthe_lock;
	u8 tthe_exit;
#endif
	u8 debug_level;
	u8 watchdog_enabled;
	bool watchdog_force_stop;
	u32 watchdog_interval;
	u8 show_timestamp;
	u32 startup_status;
	u8 pip2_cmd_tag_seq;
	u8 pip2_prot_active;
	u8 pip2_send_user_cmd;
	u8 get_param_id;
	bool bl_pip_ver_ready;
	bool app_pip_ver_ready;
	u8 core_probe_complete;
	u8 active_dut_generation;
	bool set_dut_generation;
	u8 fw_system_mode;
	u8 flashless_dut;
	u8 bl_with_no_int;
	u8 cal_cache_in_host;
	u8 multi_chip;
	u8 tthe_hid_usb_format;
	u8 flashless_auto_bl;
	u8 pip2_us_file_path[PT_MAX_PATH_SIZE];
	bool fw_updating;
	bool fw_sys_mode_in_standby_state;
#ifdef TTDL_PTVIRTDUT_SUPPORT
	u8 route_bus_virt_dut;
#endif /* TTDL_PTVIRTDUT_SUPPORT */
	u8 panel_id_support;
#ifdef TTDL_DIAGNOSTICS
	u8 t_refresh_active;
	u8 flush_bus_type;
	u8 ttdl_bist_select;
	u8 force_pip2_seq;
	u16 ping_test_size;
	u16 pip2_crc_error_count;
	u16 t_refresh_count;
	u16 t_refresh_total;
	u16 wd_xres_count;
	u32 watchdog_count;
	u32 watchdog_irq_stuck_count;
	u32 watchdog_failed_access_count;
	u32 bus_transmit_error_count;
	u32 irq_count;
	u32 bl_retry_packet_count;
	u32 file_erase_timeout_count;
	unsigned long t_refresh_time;
	u16 err_gpio;
	u16 err_gpio_type;
	bool show_tt_data;
	bool bridge_mode;
	bool hw_detect_enabled;
#endif
	struct tp_device tp;
};

struct gd_sensor {
	int32_t cm_min;
	int32_t cm_max;
	int32_t cm_ave;
	int32_t cm_min_exclude_edge;
	int32_t cm_max_exclude_edge;
	int32_t cm_ave_exclude_edge;
	int32_t gradient_val;
};

#ifdef TTHE_TUNER_SUPPORT
#define PT_CMD_RET_PANEL_IN_DATA_OFFSET	0
#define PT_CMD_RET_PANEL_ELMNT_SZ_MASK	0x07
#define PT_CMD_RET_PANEL_HDR		0x0A
#define PT_CMD_RET_PANEL_ELMNT_SZ_MAX	0x2

enum scan_data_type_list {
	PT_MUT_RAW,
	PT_MUT_BASE,
	PT_MUT_DIFF,
	PT_SELF_RAW,
	PT_SELF_BASE,
	PT_SELF_DIFF,
	PT_BAL_RAW,
	PT_BAL_BASE,
	PT_BAL_DIFF,
};
#endif

static inline int pt_adap_read_default(struct pt_core_data *cd,
		void *buf, int size)
{
	return cd->bus_ops->read_default(cd->dev, buf, size);
}

static inline int pt_adap_read_default_nosize(struct pt_core_data *cd,
		void *buf, int max)
{
	return cd->bus_ops->read_default_nosize(cd->dev, buf, max);
}

static inline int pt_adap_write_read_specific(struct pt_core_data *cd,
		u16 write_len, u8 *write_buf, u8 *read_buf)
{
	return cd->bus_ops->write_read_specific(cd->dev, write_len, write_buf,
			read_buf);
}

static inline void *pt_get_dynamic_data(struct device *dev, int id)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);

	return cd->pt_dynamic_data[id];
}

int request_exclusive_pt(struct pt_core_data *cd, void *ownptr,
		int timeout_ms);
int release_exclusive(struct pt_core_data *cd, void *ownptr);
int _pt_request_pip_get_param(struct device *dev,
		int protect, u8 param_id, u32 *value);
int _pt_request_pip_set_param(struct device *dev,
		int protect, u8 param_id, u32 value, u8 size);

static inline int pt_request_exclusive(struct device *dev, int timeout_ms)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);

	return request_exclusive_pt(cd, dev, timeout_ms);
}

static inline int pt_release_exclusive(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);

	return release_exclusive(cd, dev);
}

static inline int pt_request_nonhid_get_param(struct device *dev,
		int protect, u8 param_id, u32 *value)
{
	return _pt_request_pip_get_param(dev, protect, param_id,
			value);
}

static inline int pt_request_nonhid_set_param(struct device *dev,
		int protect, u8 param_id, u32 value, u8 size)
{
	return _pt_request_pip_set_param(dev, protect, param_id,
			value, size);
}

void pt_pr_buf(struct device *dev, u8 debug_level, u8 *buf,
	u16 buf_len, const char *data_name);

#ifdef CONFIG_TOUCHSCREEN_PARADE_DEVICETREE_SUPPORT
int pt_devtree_create_and_get_pdata(struct device *adap_dev);
int pt_devtree_clean_pdata(struct device *adap_dev);
#else
static inline int pt_devtree_create_and_get_pdata(struct device *adap_dev)
{
	return 0;
}

static inline int pt_devtree_clean_pdata(struct device *adap_dev)
{
	return 0;
}
#endif

int pt_probe(const struct pt_bus_ops *ops, struct device *dev,
		u16 irq, size_t xfer_buf_size);
int pt_release(struct pt_core_data *cd);

struct pt_core_commands *pt_get_commands(void);
struct pt_core_data *pt_get_core_data(char *id);


int pt_mt_release(struct device *dev);
int pt_mt_probe(struct device *dev);

#ifdef CONFIG_TOUCHSCREEN_PARADE_BUTTON
int pt_btn_probe(struct device *dev);
int pt_btn_release(struct device *dev);
#else
static inline int pt_btn_probe(struct device *dev) { return 0; }
static inline int pt_btn_release(struct device *dev) { return 0; }
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_PROXIMITY
int pt_proximity_probe(struct device *dev);
int pt_proximity_release(struct device *dev);
#else
static inline int pt_proximity_probe(struct device *dev) { return 0; }
static inline int pt_proximity_release(struct device *dev) { return 0; }
#endif

static inline unsigned int pt_get_time_stamp(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
	//struct timespec ts;

	//getnstimeofday(&ts);
	//return (ts.tv_sec*1000 + ts.tv_nsec/1000000);
	return 0;
#else
	struct timeval tv;

	do_gettimeofday(&tv);
	return (tv.tv_sec*1000 + tv.tv_usec/1000);
#endif
}

void pt_init_function_ptrs(struct pt_mt_data *md);
int _pt_subscribe_attention(struct device *dev,
	enum pt_atten_type type, char *id, int (*func)(struct device *),
	int mode);
int _pt_unsubscribe_attention(struct device *dev,
	enum pt_atten_type type, char *id, int (*func)(struct device *),
	int mode);
struct pt_sysinfo *_pt_request_sysinfo(struct device *dev);

extern const struct dev_pm_ops pt_pm_ops;

int pt_register_module(struct pt_module *module);
void pt_unregister_module(struct pt_module *module);

void *pt_get_module_data(struct device *dev,
		struct pt_module *module);

#endif /* _PT_REGS_H */
