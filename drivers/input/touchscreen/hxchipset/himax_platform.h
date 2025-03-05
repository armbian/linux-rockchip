/* Himax Android Driver Sample Code for QCT platform
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

#ifndef HIMAX_PLATFORM_H
#define HIMAX_PLATFORM_H

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>

#define HIMAX_I2C_PLATFORM
#define HIMAX_I2C_RETRY_TIMES 3
#define BUS_RW_MAX_LEN 256

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
#define D(x...) pr_debug("[HXTP] " x)
#define I(x...) pr_info("[HXTP] " x)
#define W(x...) pr_warn("[HXTP][WARNING] " x)
#define E(x...) pr_err("[HXTP][ERROR] " x)
#define DIF(x...)                                                                                  \
	do {                                                                                       \
		if (debug_flag)                                                                    \
			pr_debug("[HXTP][DEBUG] " x)                                               \
	} while (0)
#else

#define D(x...)
#define I(x...)
#define W(x...)
#define E(x...)
#define DIF(x...)
#endif

#define HIMAX_common_NAME "himax_tp"
#define HIMAX_I2C_ADDR 0x48
#define INPUT_DEV_NAME "himax-touchscreen"

struct himax_i2c_platform_data {
	int abs_x_min;
	int abs_x_max;
	int abs_x_fuzz;
	int abs_y_min;
	int abs_y_max;
	int abs_y_fuzz;
	int abs_pressure_min;
	int abs_pressure_max;
	int abs_pressure_fuzz;
	int abs_width_min;
	int abs_width_max;
	int screenWidth;
	int screenHeight;
	uint8_t fw_version;
	uint8_t tw_id;
	uint8_t powerOff3V3;
	uint8_t cable_config[2];
	uint8_t protocol_type;
	int gpio_irq;
	int fail_det;
	int gpio_reset;
	int gpio_3v3_en;
	int gpio_pon;
	int lcm_rst;
	int (*power)(int on);
	void (*reset)(void);
	struct himax_virtual_key *virtual_key;
	int hx_config_size;
	const char *fw_name;
	const char *criteria_file_name;
};

extern int himax_bus_read(struct himax_ts_data *ts, uint8_t command, uint8_t *data, uint32_t length,
			  uint8_t toRetry);
extern int himax_bus_write(struct himax_ts_data *ts, uint8_t command, uint8_t *data,
			   uint32_t length, uint8_t toRetry);
extern void himax_int_enable(struct himax_ts_data *ts, int enable);
extern int himax_ts_register_interrupt(struct himax_ts_data *ts);
extern int himax_fail_det_register_interrupt(struct himax_ts_data *ts);
int himax_ts_unregister_interrupt(struct himax_ts_data *ts);
extern uint8_t himax_int_gpio_read(int pinnum);
extern int himax_gpio_power_config(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata);
void himax_gpio_power_deconfig(struct himax_i2c_platform_data *pdata);

#if defined(HX_CONFIG_FB)
extern int himax_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#elif defined(HX_CONFIG_DRM)
extern int himax_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#endif
extern int himax_hotplug_notifier(struct notifier_block *self, unsigned long event, void *data);

extern void himax_ts_work(struct himax_ts_data *ts);
extern void himax_fail_det_work(struct himax_ts_data *ts);
extern enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer);
extern int himax_chip_common_init(struct himax_ts_data *ts);
extern void himax_chip_common_deinit(struct himax_ts_data *ts);
extern int himax_sysfs_init(struct himax_ts_data *ts);
extern void himax_sysfs_deinit(struct himax_ts_data *ts);
int himax_int_en_set(struct himax_ts_data *ts);
int tp_diag_himax(void);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83192)
bool hx83192_chip_detect(struct himax_ts_data *ts);
#endif
#endif
