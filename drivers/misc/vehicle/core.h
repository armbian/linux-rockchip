/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * core.h -- core define for mfd display arch
 *
 * Copyright (c) 2023-2028 Rockchip Electronics Co., Ltd.
 *
 * Author: luowei <lw@rock-chips.com>
 *
 */

#ifndef __MFD_VEHICLE_CORE_H__
#define __MFD_VEHICLE_CORE_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/regulator/consumer.h>
#include <linux/backlight.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/random.h>

#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>
#include <linux/extcon-provider.h>
#include <linux/bitfield.h>
#include <linux/version.h>

#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/hrtimer.h>
#include <linux/platform_data/spi-rockchip.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/completion.h>



#include "vehicle_core.h"

/*
 * if enable all the debug information,
 * there will be much log.
 *
 * so suggest set CONFIG_LOG_BUF_SHIFT to 18
 */
#define VEHICLE_DEBUG


#ifdef VEHICLE_DEBUG
#define VEHICLE_DBG(x...) pr_info(x)
#else
#define VEHICLE_DBG(x...) no_printk(x)
#endif

#define MCU_MAX_REGS 64
struct mcu_gpio_chip {
	const char *name;
	struct platform_device *pdev;
	struct gpio_chip gpio_chip;
	struct regmap *regmap;
	struct regulator *regulator;
	unsigned int ngpio;
	u8 backup_regs[MCU_MAX_REGS];

	struct gpio_desc *reset_gpio_desc;
	int reset_gpio_irq;
};

struct vehicle;
enum vehicle_hw_type {
	VEHICLE_HW_TYPE_INVALID = 0,

	VEHICLE_HW_TYPE_ADC,
	VEHICLE_HW_TYPE_GPIO,
	VEHICLE_HW_TYPE_I2C,
	VEHICLE_HW_TYPE_SPI,
	VEHICLE_HW_TYPE_UART,
	VEHICLE_HW_TYPE_CHIP_MCU,

	VEHICLE_HW_TYPE_END,
};

/* VEHICLE is in state parking */
#define GEAR_0 1
/* VEHICLE is in state reverse */
#define GEAR_1 2
/* VEHICLE is in state neutral */
#define GEAR_3 3
/* VEHICLE is in state driver */
#define GEAR_2 4
/* no turn signal */
#define TURN_0 0
/* left turn signal */
#define TURN_1 1
/* right turn signal */
#define TURN_2 2

#define POWER_REQ_STATE_ON 0
#define POWER_REQ_STATE_SHUTDOWN_PREPARE 1
#define POWER_REQ_STATE_CANCEL_SHUTDOWN 2
#define POWER_REQ_STATE_FINISHED 3

#define POWER_REQ_PARAM_SHUTDOWN_IMMEDIATELY 1
#define POWER_REQ_PARAM_CAN_SLEEP 2
#define POWER_REQ_PARAM_SHUTDOWN_ONLY 3
#define POWER_REQ_PARAM_SLEEP_IMMEDIATELY 4
#define POWER_REQ_PARAM_HIBERNATE_IMMEDIATELY 5
#define POWER_REQ_PARAM_CAN_HIBERNATE 6

/* temperature set from hardware on Android OREO and PIE uses below indexes */
#define AC_TEMP_LEFT_INDEX 1
#define AC_TEMP_RIGHT_INDEX 4

/* temperature set from APP on Android PIE uses below indexes */
#define PIE_AC_TEMP_LEFT_INDEX 49
#define PIE_AC_TEMP_RIGHT_INDEX 68


struct vehicle_hw_data {
	const char *name;
	enum vehicle_hw_type vehicle_hw_type;

	int (*hw_init)(struct vehicle *vehicle);
	int (*data_update)(struct vehicle *vehicle);
	int (*suspend)(struct vehicle *vehicle);
	int (*resume)(struct vehicle *vehicle);
};

struct vehicle_event_data {
	u32 gear;
	u32 turn;
	u32 temp_right;
	u32 temp_left;
	u32 fan_direction;
	u32 fan_speed;
	u32 defrost_left;
	u32 defrost_right;
	u32 ac_on;
	u32 auto_on;
	u32 hvac_on;
	u32 recirc_on;
	u32 power_req_state;
	u32 power_req_param;
	u32 seat_temp_left;
	u32 seat_temp_right;
};

struct vehicle_gpio {
	struct mutex wq_lock;
	int use_delay_work;
	struct delayed_work irq_work;
	struct workqueue_struct *vehicle_wq;
	struct delayed_work vehicle_delay_work;

	enum vehicle_hw_type type;
	struct device *dev;
	struct platform_device *pdev;
	struct vehicle *parent;
	struct regmap *regmap;
	int irq;

	struct gpio_desc  *gear_gpio_park;
	struct gpio_desc  *gear_gpio_reverse;
	struct gpio_desc  *gear_gpio_neutral;
	struct gpio_desc  *gear_gpio_drive;
	struct gpio_desc  *gear_gpio_manual;
	struct gpio_desc  *turn_gpio;
	struct gpio_desc  *temp_right_gpio;
	struct gpio_desc  *temp_left_gpio;
	struct gpio_desc  *fan_direction_gpio;
	struct gpio_desc  *fan_speed_gpio;
	struct gpio_desc  *defrost_left_gpio;
	struct gpio_desc  *defrost_right_gpio;
	struct gpio_desc  *ac_on_gpio;
	struct gpio_desc  *auto_on_gpio;
	struct gpio_desc  *hvac_on_gpio;
	struct gpio_desc  *recirc_on_gpio;
	struct gpio_desc  *power_req_state_gpio;
	struct gpio_desc  *power_req_param_gpio;
	struct gpio_desc  *seat_temp_left_gpio;
	struct gpio_desc  *seat_temp_right_gpio;

	struct vehicle_hw_data *hw_data;
};

struct vehicle_chip_mcu {
	enum vehicle_hw_type type;
	struct device *dev;
	struct vehicle *parent;
	struct regmap *regmap;

};

struct vehicle_spi {
	struct mutex wq_lock;
	int use_delay_work;
	struct delayed_work irq_work;
	struct workqueue_struct *vehicle_wq;
	struct delayed_work vehicle_delay_work;

	enum vehicle_hw_type hw_type;
	struct device *dev;
	struct platform_device *pdev;
	struct spi_device *spi;
	int irq;
	struct vehicle *parent;
	struct regmap *regmap;

	struct mcu_gpio_chip *gpio_mcu;

	char *rx_buf;
	int rx_len;
	char *tx_buf;
	int tx_len;

	struct vehicle_event_data spi_data;
	struct vehicle_hw_data *hw_data;
};

struct vehicle_uart {
	struct mutex wq_lock;
	int use_delay_work;
	struct delayed_work irq_work;
	struct workqueue_struct *vehicle_wq;
	struct delayed_work vehicle_delay_work;

	enum vehicle_hw_type hw_type;
	struct device *dev;
	struct platform_device *pdev;
	struct vehicle *parent;
	struct regmap *regmap;

	struct vehicle_event_data uart_data;
	struct vehicle_hw_data *hw_data;
};

struct vehicle_dummy {
	struct mutex wq_lock;
	int use_delay_work;
	struct delayed_work irq_work;
	struct workqueue_struct *vehicle_wq;
	struct delayed_work vehicle_delay_work;

	enum vehicle_hw_type hw_type;
	struct device *dev;
	struct platform_device *pdev;
	struct vehicle *parent;
	struct regmap *regmap;

	struct vehicle_event_data dummy_hw_data;
};

struct vehicle_adc {
	struct mutex wq_lock;
	int use_delay_work;
	struct delayed_work irq_work;
	struct workqueue_struct *vehicle_wq;
	struct delayed_work vehicle_delay_work;

	enum vehicle_hw_type type;
	struct device *dev;
	struct platform_device *pdev;
	struct vehicle *parent;
	struct regmap *regmap;
	int irq;

	struct iio_channel *gear_adc_chn;
	struct iio_channel *turn_left_adc_chn;
	struct iio_channel *turn_right_adc_chn;
	struct iio_channel *temp_right_adc_chn;
	struct iio_channel *temp_left_adc_chn;
	struct iio_channel *fan_direction_adc_chn;
	struct iio_channel *fan_speed_adc_chn;
	struct iio_channel *defrost_left_adc_chn;
	struct iio_channel *defrost_right_adc_chn;
	struct iio_channel *ac_on_adc_chn;
	struct iio_channel *auto_on_adc_chn;
	struct iio_channel *hvac_on_adc_chn;
	struct iio_channel *recirc_on_adc_chn;
	struct iio_channel *power_req_state_adc_chn;
	struct iio_channel *power_req_param_adc_chn;
	struct iio_channel *seat_temp_left_adc_chn;
	struct iio_channel *seat_temp_right_adc_chn;

	struct vehicle_hw_data *hw_data;
};

struct vehicle_i2c {
	struct mutex wq_lock;
	int use_delay_work;
	struct delayed_work irq_work;
	struct workqueue_struct *vehicle_wq;
	struct delayed_work vehicle_delay_work;

	enum vehicle_hw_type hw_type;
	struct device *dev;
	struct platform_device *pdev;
	struct vehicle *parent;
	struct regmap *regmap;

	struct i2c_client *client;
	struct vehicle_event_data i2c_data;
	struct vehicle_hw_data *hw_data;
};

struct vehicle {
	enum vehicle_hw_type hw_type;
	struct device *dev;
	struct platform_device *pdev;

	struct vehicle_event_data vehicle_data;

	struct vehicle_adc *vehicle_adc;
	struct vehicle_gpio *vehicle_gpio;
	struct vehicle_i2c *vehicle_i2c;
	struct vehicle_spi *vehicle_spi;
	struct vehicle_uart *vehicle_uart;
	struct vehicle_dummy *vehicle_dummy;
	struct vehicle_chip_mcu *vehicle_chip_mcu;
};

extern struct vehicle_hw_data vehicle_adc_data;
extern struct vehicle_hw_data vehicle_gpio_data;
extern struct vehicle_hw_data vehicle_i2c_data;
extern struct vehicle_hw_data vehicle_spi_data;
extern struct vehicle_hw_data vehicle_uart_data;
extern struct vehicle_hw_data vehicle_chip_mcu_data;

extern struct vehicle *g_vehicle_hw;
extern const struct regmap_bus vehicle_regmap_spi;
extern void vehicle_set_property(u16 prop, u8 index, u32 value, u32 param);
extern int gpio_mcu_register(struct spi_device *spi);
extern int vehicle_spi_write_slt(struct vehicle *vehicle, const void *txbuf, size_t n);
extern int vehicle_spi_read_slt(struct vehicle *vehicle, void *rxbuf, size_t n);

#endif
