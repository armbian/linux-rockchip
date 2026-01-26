/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 */

#ifndef __RKNPU3_CORE_H__
#define __RKNPU3_CORE_H__

#include "rknpu3_types.h"
#include "rknpu3_ioctl.h"

int rknpu3_core_module_init(struct rknpu3_device *rknpu3_dev);
void rknpu3_core_module_deinit(struct rknpu3_device *rknpu3_dev);
int rknpu3_core_submit_task(struct rknpu3_device *rknpu3_dev, uint32_t core_id,
		struct rknpu3_task *task);
int rknpu3_core_get_load(struct rknpu3_device *rknpu3_dev, struct rknpu3_load *load);
int rknpu3_core_get_hw_version(struct rknpu3_device *rknpu3_dev, uint64_t *hw_version);
int rknpu3_core_get_bw_info(struct rknpu3_device *rknpu3_dev,
		struct rknpu3_bw_info *bw_info);
int rknpu3_core_get_cycle_info(struct rknpu3_device *rknpu3_dev,
		struct rknpu3_cycle_info *cycle_info);

/* IRQ handler declarations */
irqreturn_t rknpu3_core0_irq_handler(int irq, void *data);
irqreturn_t rknpu3_core1_irq_handler(int irq, void *data);

#endif /* __RKNPU3_CORE_H__ */
