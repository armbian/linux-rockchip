/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_OFFLINE_V20_H
#define _RKVPSS_OFFLINE_V20_H

struct rkvpss_hw_dev;

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_VPSS_V20)
int rkvpss_register_offline_v20(struct rkvpss_hw_dev *hw);
void rkvpss_unregister_offline_v20(struct rkvpss_hw_dev *hw);
void rkvpss_offline_irq_v20(struct rkvpss_hw_dev *hw, u32 irq);

/* Internal functions used by rockit mode */
struct rkvpss_offline_dev;
int rkvpss_ofl_add_file_id(struct rkvpss_offline_dev *ofl, void *idr_entity);
void rkvpss_ofl_buf_del_by_file(struct rkvpss_offline_dev *ofl, int file_id);
long rkvpss_ofl_action(struct rkvpss_offline_dev *ofl, int file_id, unsigned int cmd, void *arg);
#else
static inline int rkvpss_register_offline_v20(struct rkvpss_hw_dev *hw) {return -EINVAL; }
static inline void rkvpss_unregister_offline_v20(struct rkvpss_hw_dev *hw) {}
static inline void rkvpss_offline_irq_v20(struct rkvpss_hw_dev *hw, u32 irq) {}
#endif

#endif
