/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 */

#ifndef __RKNPU3_DRV_H__
#define __RKNPU3_DRV_H__

#include <linux/types.h>
#include <linux/dma-mapping.h>

#include "rknpu3_types.h"

#define DRIVER_NAME "RKNPU3"
#define DRIVER_DESC "Rockchip RKNPU3 driver"
#define DRIVER_DATE "20260303"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0
#define DRIVER_PATCHLEVEL 1
#define RKNPU3_DEVICE_NAME "rknpu3"

/* Log macros */
#define LOG_TAG "RKNPU3"
#define LOG_DEV_INFO(dev, fmt, args...) dev_info(dev, fmt, ##args)
#define LOG_DEV_WARN(dev, fmt, args...) dev_warn(dev, fmt, ##args)
#define LOG_DEV_DEBUG(dev, fmt, args...) dev_dbg(dev, fmt, ##args)
#define LOG_DEV_ERROR(dev, fmt, args...) dev_err(dev, fmt, ##args)

/* Forward declaration */
struct rknpu3_device;

/* Power management functions */
int rknpu3_power_get(struct rknpu3_device *rknpu3_dev);
int rknpu3_power_put(struct rknpu3_device *rknpu3_dev);
int rknpu3_power_put_delay(struct rknpu3_device *rknpu3_dev);

#endif /* __RKNPU3_DRV_H__ */
