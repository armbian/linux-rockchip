/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_OFFLINE_DVBM_H
#define _RKVPSS_OFFLINE_DVBM_H

#include <linux/of.h>
#include <linux/of_platform.h>

#include <soc/rockchip/rockchip_dvbm.h>


#if IS_ENABLED(CONFIG_ROCKCHIP_DVBM)
int rkvpss_ofl_dvbm_get(struct rkvpss_offline_dev *ofl);
int rkvpss_ofl_dvbm_init(struct rkvpss_offline_dev *ofl, struct dma_buf *dbuf,
	u32 dma_addr, u32 wrap_line, int width, int height, int id);
void rkvpss_ofl_dvbm_deinit(struct rkvpss_offline_dev *ofl, int id);
int rkvpss_ofl_dvbm_event(u32 event, u32 seq);
#else
static inline int rkvpss_ofl_dvbm_get(struct rkvpss_offline_dev *ofl) {return -EINVAL; }
static inline int rkvpss_ofl_dvbm_init(struct rkvpss_offline_dev *ofl, struct dma_buf *dbuf,
					u32 dma_addr, u32 wrap_line, int width, int height,
					int id) {return -EINVAL; }
static inline void rkvpss_ofl_dvbm_deinit(struct rkvpss_offline_dev *ofl, int id) {}
static inline int rkvpss_ofl_dvbm_event(u32 event, u32 seq) {return -EINVAL; }
#endif

#endif

