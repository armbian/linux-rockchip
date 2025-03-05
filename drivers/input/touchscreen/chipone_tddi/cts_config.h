// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CTS_CONFIG_H
#define CTS_CONFIG_H

/** Driver version */
#define CFG_CTS_DRIVER_MAJOR_VERSION        3
#define CFG_CTS_DRIVER_MINOR_VERSION        6
#define CFG_CTS_DRIVER_PATCH_VERSION        3
#define CFG_CTS_DRIVER_VERSION              "v3.6.3"

/* Support Mediatek TPD arch */
/* #define CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */

/* Support Spreadtrum platform */
#define CFG_CTS_PLATFORM_SPRD_SUPPORTED

/* Support cascade application */
//#define CFG_CTS_CASCADE_SUPPORTED

/* Support both finger and stylus protocol, 11 */
// #define CFG_CTS_FINGER_STYLUS_SUPPORTED

/* #define CFG_CTS_STYLUS_BTN_SUPPORTED */

#define CONFIG_CTS_I2C_HOST

/* For Goole Security */
#define CFG_CTS_FOR_GKI

/** Whether reset pin is used */
#define CFG_CTS_HAS_RESET_PIN

#ifndef CONFIG_CTS_I2C_HOST
#ifndef CFG_CTS_HAS_RESET_PIN
#define CFG_CTS_HAS_RESET_PIN
#endif
#define CFG_CTS_SPI_SPEED_KHZ               9600
#endif /* CONFIG_CTS_I2C_HOST */

/* #define CFG_CTS_FORCE_UP */

/* #define CFG_CTS_HEARTBEAT_MECHANISM */

/* #define CFG_CTS_PALM_DETECT */
#ifdef CFG_CTS_PALM_DETECT
#define CFG_CTS_PALM_EVENT                  240
#define CFG_CTS_PALM_ID                     0x80
#endif

#define CONFIG_GENERIC_HARDIRQS

/** Whether force download firmware to chip */
/* #define CFG_CTS_FIRMWARE_FORCE_UPDATE */

/** Use build in firmware or firmware file in fs*/
#define CFG_CTS_DRIVER_BUILTIN_FIRMWARE
#define CFG_CTS_FIRMWARE_IN_FS
#ifdef CFG_CTS_FIRMWARE_IN_FS
/* #define CFG_CTS_FW_UPDATE_SYS */
#define CFG_CTS_FIRMWARE_FILENAME           "chipone_firmware.bin"
#endif /* CFG_CTS_FIRMWARE_IN_FS */

#define CFG_CTS_FIRMWARE_SIZE               (120 * 1024)
#define CFG_CTS_FACTORY_LIMIT_FILENAME      "chipone_limit.bin"

/* IC type support */
#define CFG_CTS_CHIP_NAME                   "CHIPONE-TDDI"

#ifdef CONFIG_PROC_FS
/* Proc FS for backward compatibility for APK tool com.ICN85xx */
#define CONFIG_CTS_LEGACY_TOOL
#endif /* CONFIG_PROC_FS */

#ifdef CONFIG_SYSFS
/* Sys FS for gesture report, debug feature etc. */
#define CONFIG_CTS_SYSFS
#endif /* CONFIG_SYSFS */

#define CFG_CTS_MAX_TOUCH_NUM               (10)
#define CFG_CTS_MAX_STYLUS_NUM              (4)

/* Virtual key support */
/* #define CONFIG_CTS_VIRTUALKEY */
#ifdef CONFIG_CTS_VIRTUALKEY
#define CFG_CTS_MAX_VKEY_NUM                (4)
#define CFG_CTS_NUM_VKEY                    (3)
#define CFG_CTS_VKEY_KEYCODES               {KEY_BACK, KEY_HOME, KEY_MENU}
#endif /* CONFIG_CTS_VIRTUALKEY */

/* Gesture wakeup */
#define CFG_CTS_GESTURE
#ifdef CFG_CTS_GESTURE
#define GESTURE_UP                          0x11
#define GESTURE_C                           0x12
#define GESTURE_O                           0x13
#define GESTURE_M                           0x14
#define GESTURE_W                           0x15
#define GESTURE_E                           0x16
#define GESTURE_S                           0x17
#define GESTURE_Z                           0x1d
#define GESTURE_V                           0x1e
#define GESTURE_D_TAP                       0x50
#define GESTURE_DOWN                        0x22
#define GESTURE_LEFT                        0x23
#define GESTURE_RIGHT                       0x24
#define GESTURE_TAP                         0x7f

#define CFG_CTS_NUM_GESTURE                 (14u)
#define CFG_CTS_GESTURE_REPORT_KEY
#define CFG_CTS_GESTURE_KEYMAP          \
{                                       \
    { GESTURE_C, KEY_C,},               \
    { GESTURE_W, KEY_W,},               \
    { GESTURE_V, KEY_V,},               \
    { GESTURE_D_TAP, KEY_F1,},          \
    { GESTURE_Z, KEY_Z,},               \
    { GESTURE_M, KEY_M,},               \
    { GESTURE_O, KEY_O,},               \
    { GESTURE_E, KEY_E,},               \
    { GESTURE_S, KEY_S,},               \
    { GESTURE_UP, KEY_UP,},             \
    { GESTURE_DOWN, KEY_DOWN,},         \
    { GESTURE_LEFT, KEY_LEFT,},         \
    { GESTURE_RIGHT, KEY_RIGHT,},       \
    { GESTURE_TAP, KEY_F2,},            \
}
#define CFG_CTS_GESTURE_REPORT_TRACE         0
#endif /* CFG_CTS_GESTURE */

#define CONFIG_CTS_GLOVE
//#define CONFIG_CTS_EARJACK_DETECT
//#define CONFIG_CTS_CHARGER_DETECT

/* #define CONFIG_CTS_TP_PROXIMITY */

/* ESD protection */
//#define CONFIG_CTS_ESD_PROTECTION
#ifdef CONFIG_CTS_ESD_PROTECTION
#define CFG_CTS_ESD_PROTECTION_CHECK_PERIOD   (2 * HZ)
#define CFG_CTS_ESD_FAILED_CONFIRM_CNT        3
#endif /* CONFIG_CTS_ESD_PROTECTION */

/* Use slot protocol (protocol B), comment it if use protocol A. */
#define CONFIG_CTS_SLOTPROTOCOL

/* #define CTS_CONFIG_MKDIR_FOR_CTS_TEST */

#ifdef CONFIG_CTS_LEGACY_TOOL
#define CFG_CTS_TOOL_PROC_FILENAME            "cts_tool"
#endif /* CONFIG_CTS_LEGACY_TOOL */

#define CFG_CTS_UPDATE_CRCCHECK


#ifdef CONFIG_OF
#define CONFIG_CTS_OF
#endif

#ifdef CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED
/****************************************************************************
 * MTK Platform configurations
 ****************************************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
#define TPD_SUPPORT_I2C_DMA
#define CFG_CTS_MAX_I2C_XFER_SIZE           (250)
#define CFG_CTS_MAX_I2C_FIFO_XFER_SIZE      (8)
#else
#define CFG_CTS_MAX_I2C_XFER_SIZE           (128)
#endif /* CONFIG_MTK_I2C_EXTENSION */

#define CFG_CTS_MAX_I2C_READ_SIZE           (8192u)
#define CFG_CTS_MAX_SPI_XFER_SIZE           (8192u)
#define CFG_CTS_INT_DATA_MAX_SIZE           (8192u)

#define CTS_FW_LOG_REDIRECT_SIGN            0x60
#define CTS_FW_LOG_BUF_LEN                  128

//#define CFG_MTK_LEGEND_PLATFORM

#define CFG_CTS_DEVICE_NAME         TPD_DEVICE
#define CFG_CTS_DRIVER_NAME         "chipone-tddi"

#ifdef CONFIG_CTS_OF
#define CFG_CTS_OF_DEVICE_ID_NAME   "mediatek,cap_touch"
#endif /* CONFIG_CTS_OF */

#else /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */
/****************************************************************************
 * QCOM Platform configurations
 ****************************************************************************/
#ifndef CFG_CTS_PLATFORM_SPRD_SUPPORTED
#define CONFIG_CTS_PM_FB_NOTIFIER
#endif
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
#ifdef CONFIG_DRM
#define CFG_CTS_DRM_NOTIFIER
#endif /*CONFIG_DRM */
#else /*CONFIG_CTS_PM_FB_NOTIFIER */
#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_PM_SUSPEND)
    /* #define CONFIG_CTS_PM_GENERIC */
#endif /* CONFIG_PM_SLEEP */

#ifndef CONFIG_CTS_PM_GENERIC
#define CONFIG_CTS_PM_LEGACY
#endif /*CONFIG_CTS_PM_GENERIC */
#endif /*CONFIG_CTS_PM_FB_NOTIFIER */

#define CFG_CTS_MAX_I2C_XFER_SIZE           (48u)
#define CFG_CTS_MAX_I2C_READ_SIZE           (8192u)
#define CFG_CTS_MAX_SPI_XFER_SIZE           (8192u)
#define CFG_CTS_INT_DATA_MAX_SIZE           (8192u)

#define CTS_FW_LOG_REDIRECT_SIGN            0x60
#define CTS_FW_LOG_BUF_LEN                  128

#define CFG_CTS_DEVICE_NAME                 "chipone-tddi"
#define CFG_CTS_DRIVER_NAME                 "chipone-tddi"

#ifdef CONFIG_CTS_OF
#define CFG_CTS_OF_DEVICE_ID_NAME           "chipone-tddi"
#define CFG_CTS_OF_INT_GPIO_NAME            "chipone,irq-gpio"
#define CFG_CTS_OF_RST_GPIO_NAME            "chipone,rst-gpio"

#ifdef CFG_CTS_MANUAL_CS
#define CFG_CTS_OF_CS_GPIO_NAME             "chipone,cs-gpio"
#endif

#define CFG_CTS_OF_X_RESOLUTION_NAME        "chipone,x-res"
#define CFG_CTS_OF_Y_RESOLUTION_NAME        "chipone,y-res"

#ifdef CFG_CTS_FW_UPDATE_SYS
#define CFG_CTS_OF_PANEL_SUPPLIER           "chipone,panel-supplier"
#endif
#endif /* CONFIG_CTS_OF */

#endif /* CFG_CTS_PLATFORM_MTK_TPD_SUPPORTED */

#endif /* CTS_CONFIG_H */
