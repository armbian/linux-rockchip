/*!
 @file nds03.h
 @brief
 @author lull
 @date 2025-06
 @copyright Copyright (c) 2025  Shenzhen Nephotonics  Semiconductor Technology Co., Ltd.
 @license BSD 3-Clause License
          This file is part of the Nephotonics sensor SDK.
          It is licensed under the BSD 3-Clause License.
          A copy of the license can be found in the project root directory, in the file named LICENSE.
 */
#ifndef __NDS03_H
#define __NDS03_H

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>

/* nds03 sdk  */
#include "nds03_comm.h"
#include "nds03_dev.h"
#include "nds03_data.h"
#include "nds03_calib.h"
#include "nds03_def.h"

#define TOF_NDS03_DRV_NAME	"tof_nds03"
#define DRIVER_VERSION		"1.0.4"
#define	TOF_NDS03_MAJOR				255
#define MAX_POS_BITS		32

/*! use debug */
extern int nds03_enable_debug;

#define nds03_dbgmsg(str, ...)		\
	do {				\
		if (nds03_enable_debug > 0)  	\
			printk("%s: " str, __FUNCTION__, ##__VA_ARGS__); 	\
	} while (0)


#define nds03_info(str, ...) \
	pr_info("%s: " str , __FUNCTION__,  ##__VA_ARGS__)

#define nds03_errmsg(str, ...) \
	pr_err("%s: " str, __FUNCTION__, ##__VA_ARGS__)

#define nds03_warnmsg(str, ...) \
	pr_warn("%s: " str,__FUNCTION__, ##__VA_ARGS__)

struct nds03_context  {

	/*!< multiple device id 0 based*/
	int id;

	/*!< misc device name */
	char name[64];

	struct i2c_client * client;

	/*!< nds03 device info */
	NDS03_Dev_t g_nds03_device;

	/*!< main dev mutex/lock */
	struct mutex work_mutex;

	// /*!< work for pseudo irq polling check  */
	struct delayed_work	dwork;
	struct work_struct irq_work;
	// /*!< input device used for sending event */
	struct input_dev *idev;

	/*!<  intr gpio number  */
	int irq;

	bool remove_flag;
	/// /* user control configuration parameter */
	/*!< measure mode irq or poll*/
	atomic_t meas_mode;
	/*!< rescheduled time use in poll mode  */
	atomic_t poll_delay_ms;
	/*!< use ctrl measure state  */
	atomic_t  is_meas;
	/*!< calibtion result of the deivce */
	int calib_result;
	/*!< open input file descriptor count*/
	int fd_open_count;
	struct iio_dev *indio_dev;
	struct nds03_iio_dev *iio;
};

int nds03_common_probe(struct nds03_context * ctx);
int nds03_common_remove(struct nds03_context * ctx);
int nds03_interrupt_config(NDS03_Dev_t *pNxDevice, uint8_t is_open);
int nds03_sensor_init(struct nds03_context *ctx);

#endif
