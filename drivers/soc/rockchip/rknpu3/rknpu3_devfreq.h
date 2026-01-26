/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU3 devfreq support header
 */

#ifndef __RKNPU3_DEVFREQ_H__
#define __RKNPU3_DEVFREQ_H__

#include "rknpu3_types.h"

#ifdef CONFIG_PM_DEVFREQ
void rknpu3_devfreq_lock(struct rknpu3_device *rknpu3_dev);
void rknpu3_devfreq_unlock(struct rknpu3_device *rknpu3_dev);
int rknpu3_devfreq_init(struct rknpu3_device *rknpu3_dev);
void rknpu3_devfreq_remove(struct rknpu3_device *rknpu3_dev);
int rknpu3_devfreq_runtime_suspend(struct device *dev);
int rknpu3_devfreq_runtime_resume(struct device *dev);
#else
static inline int rknpu3_devfreq_init(struct rknpu3_device *rknpu3_dev)
{
	return -EOPNOTSUPP;
}

static inline void rknpu3_devfreq_remove(struct rknpu3_device *rknpu3_dev)
{
}

static inline void rknpu3_devfreq_lock(struct rknpu3_device *rknpu3_dev)
{
}

static inline void rknpu3_devfreq_unlock(struct rknpu3_device *rknpu3_dev)
{
}

static inline int rknpu3_devfreq_runtime_suspend(struct device *dev)
{
	return -EOPNOTSUPP;
}

static inline int rknpu3_devfreq_runtime_resume(struct device *dev)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_PM_DEVFREQ */

#endif /* __RKNPU3_DEVFREQ_H__ */
