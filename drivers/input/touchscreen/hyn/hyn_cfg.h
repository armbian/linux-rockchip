/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_cfg.h
 *
 * hynitron TouchScreen driver.
 *
 * Copyright (c) 2025  hynitron
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _HYNITRON_CFG_H
#define _HYNITRON_CFG_H

#define I2C_PORT

#ifdef I2C_PORT
#define I2C_USE_DMA	  (1)  //0:soft 1:DMA 2:MTK_DMA
#else
#define SPI_MODE		 (0)
#define SPI_DELAY_CS	 (10)  //us
#define SPI_CLOCK_FREQ   (8000000)
#define I2C_USE_DMA	  (0)
// #define CONFIG_BUS_SPI  //default:0  MTK can try define
#endif

#define HYN_TRANSFER_LIMIT_LEN   (2048) //need >= 8

#define HYN_POWER_ON_UPDATA	 (1) //updata

#ifdef CONFIG_MODULES
#define HYN_GKI_VER		   (1) //GKI version need enable
#else
#define HYN_GKI_VER		   (0) //GKI version need enable
#endif
#define HYN_APK_DEBUG_EN	  (1)

#define HYN_GESTURE_EN		(0) //gesture

#define HYN_PROX_TYEP		 (0) //0:disable 1:default 2:mtk_sensor 3:mtk_alps 4:Spread misc

#define KEY_USED_POS_REPORT   (0)

#define ESD_CHECK_EN		  (0)

#define HYN_WAKE_LOCK_EN	  (0)

#define HYN_MT_PROTOCOL_B_EN  (1)

//selftest cfg
#define HYN_TP0_TEST_LOG_SAVE  (0)

#define HYN_DRIVER_VERSION	  "== Hynitron V2.11 20250217 =="

#endif
