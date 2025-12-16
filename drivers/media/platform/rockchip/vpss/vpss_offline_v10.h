/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_OFFLINE_V10_H
#define _RKVPSS_OFFLINE_V10_H

#include "vpss_offline.h"


#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_VPSS_V10)
int rkvpss_register_offline_v10(struct rkvpss_hw_dev *hw);
void rkvpss_unregister_offline_v10(struct rkvpss_hw_dev *hw);
void rkvpss_offline_irq_v10(struct rkvpss_hw_dev *hw, u32 irq);
#else
static inline int rkvpss_register_offline_v10(struct rkvpss_hw_dev *hw) {return -EINVAL; }
static inline void rkvpss_unregister_offline_v10(struct rkvpss_hw_dev *hw) {}
static inline void rkvpss_offline_irq_v10(struct rkvpss_hw_dev *hw, u32 irq) {}
#endif

#endif

