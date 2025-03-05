/*
 * pt_core.h
 * Parade TrueTouch(TM) Standard Product Core Module.
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
 * Contact Parade Technologies at www.parade.com <ttdrivers@paradetech.com>
 */

#ifndef _LINUX_PT_CORE_H
#define _LINUX_PT_CORE_H

#include <linux/stringify.h>

#define PT_I2C_NAME "pt_i2c_adapter"
#define PT_SPI_NAME "pt_spi_adapter"

#define PT_CORE_NAME "pt_core"
#define PT_MT_NAME "pt_mt"
#define PT_BTN_NAME "pt_btn"
#define PT_PROXIMITY_NAME "pt_proximity"

#define PT_DRIVER_NAME TTDL
#define PT_DRIVER_MAJOR 04
#define PT_DRIVER_MINOR 11

#define PT_DRIVER_REVCTRL 977092

#define PT_DRIVER_VERSION		    \
__stringify(PT_DRIVER_NAME)		    \
"." __stringify(PT_DRIVER_MAJOR)	    \
"." __stringify(PT_DRIVER_MINOR)	    \
"." __stringify(PT_DRIVER_REVCTRL)

#define PT_DRIVER_DATE "20201210"

/* abs settings */
#define PT_IGNORE_VALUE             -1

enum pt_core_platform_flags {
	PT_CORE_FLAG_NONE,
	PT_CORE_FLAG_POWEROFF_ON_SLEEP = 0x02,
	PT_CORE_FLAG_RESTORE_PARAMETERS = 0x04,
	PT_CORE_FLAG_DEEP_STANDBY = 0x08,
	PT_CORE_FLAG_SKIP_SYS_SLEEP = 0x10,
	PT_CORE_FLAG_SKIP_RUNTIME = 0x20,
	PT_CORE_FLAG_SKIP_RESUME = 0x40,
};

enum pt_core_platform_easy_wakeup_gesture {
	PT_CORE_EWG_NONE,
	PT_CORE_EWG_TAP_TAP,
	PT_CORE_EWG_TWO_FINGER_SLIDE,
	PT_CORE_EWG_RESERVED,
	PT_CORE_EWG_WAKE_ON_INT_FROM_HOST = 0xFF,
};

enum pt_loader_platform_flags {
	PT_LOADER_FLAG_NONE,
	PT_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
	/* Use CONFIG_VER field in TT_CFG to decide TT_CFG update */
	PT_LOADER_FLAG_CHECK_TTCONFIG_VERSION,
	PT_LOADER_FLAG_CALIBRATE_AFTER_TTCONFIG_UPGRADE,
};

enum CONFIG_DUT_GENERATION {
	CONFIG_DUT_AUTO_DETECT          = 0x00,
	CONFIG_DUT_PIP1_ONLY            = 0x01,
	CONFIG_DUT_PIP2_CAPABLE         = 0x02,
};

enum pt_core_platform_panel_id_flags {
	PT_PANEL_ID_DISABLE         = 0x00,
	PT_PANEL_ID_BY_BL           = 0x01,
	PT_PANEL_ID_BY_SYS_INFO     = 0x02,
	PT_PANEL_ID_BY_MFG_DATA     = 0x04,
};

struct touch_settings {
	const uint8_t   *data;
	uint32_t         size;
	uint8_t         tag;
};

struct pt_touch_firmware {
	const uint8_t *img;
	uint32_t size;
	const uint8_t *ver;
	uint8_t vsize;
	uint8_t panel_id;
};

struct pt_touch_config {
	struct touch_settings *param_regs;
	struct touch_settings *param_size;
	const uint8_t *fw_ver;
	uint8_t fw_vsize;
	uint8_t panel_id;
};

struct pt_loader_platform_data {
	struct pt_touch_firmware *fw;
	struct pt_touch_config *ttconfig;
	struct pt_touch_firmware **fws;
	struct pt_touch_config **ttconfigs;
	u32 flags;
};

typedef int (*pt_platform_read) (struct device *dev, void *buf, int size);

#define PT_TOUCH_SETTINGS_MAX 32

struct pt_core_platform_data {
	int irq_gpio;
	int rst_gpio;
	int ddi_rst_gpio;
	int vddi_gpio;
	int vcc_gpio;
	int avdd_gpio;
	int avee_gpio;
	int level_irq_udelay;
	u16 hid_desc_register;
	u16 vendor_id;
	u16 product_id;

	int (*xres)(struct pt_core_platform_data *pdata,
		struct device *dev);
	int (*init)(struct pt_core_platform_data *pdata,
		int on, struct device *dev);
	int (*power)(struct pt_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq);
	int (*detect)(struct pt_core_platform_data *pdata,
		struct device *dev, pt_platform_read read);
	int (*irq_stat)(struct pt_core_platform_data *pdata,
		struct device *dev);
	int (*setup_power)(struct pt_core_platform_data *pdata,
		int on, struct device *dev);
	int (*setup_irq)(struct pt_core_platform_data *pdata,
		int on, struct device *dev);
	struct touch_settings *sett[PT_TOUCH_SETTINGS_MAX];
	u32 flags;
	u8 easy_wakeup_gesture;
	u8 config_dut_generation;
	u8 watchdog_force_stop;
	u8 panel_id_support;
};

struct touch_framework {
	const int16_t  *abs;
	uint8_t         size;
	uint8_t         enable_vkeys;
} __packed;

enum pt_mt_platform_power_state {
	PT_MT_POWER_OFF = 0x00,
	PT_MT_POWER_ON  = 0x01
};

enum pt_mt_platform_irq_state {
	PT_MT_IRQ_FREE = 0x00,
	PT_MT_IRQ_REG  = 0x01
};

enum pt_mt_platform_flags {
	PT_MT_FLAG_NONE,
	PT_MT_FLAG_HOVER = 0x04,
	PT_MT_FLAG_FLIP = 0x08,
	PT_MT_FLAG_INV_X = 0x10,
	PT_MT_FLAG_INV_Y = 0x20,
	PT_MT_FLAG_VKEYS = 0x40,
	PT_MT_FLAG_NO_TOUCH_ON_LO = 0x80,
};

struct pt_mt_platform_data {
	struct touch_framework *frmwrk;
	unsigned short flags;
	char const *inp_dev_name;
	int vkeys_x;
	int vkeys_y;
};

struct pt_btn_platform_data {
	char const *inp_dev_name;
};

struct pt_proximity_platform_data {
	struct touch_framework *frmwrk;
	char const *inp_dev_name;
};

struct pt_platform_data {
	struct pt_core_platform_data *core_pdata;
	struct pt_mt_platform_data *mt_pdata;
	struct pt_btn_platform_data *btn_pdata;
	struct pt_proximity_platform_data *prox_pdata;
	struct pt_loader_platform_data *loader_pdata;
};

irqreturn_t pt_irq(int irq, void *handle);

#endif /* _LINUX_PT_CORE_H */
