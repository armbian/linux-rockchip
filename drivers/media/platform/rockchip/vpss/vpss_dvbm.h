/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_DVBM_H
#define _RKVPSS_DVBM_H

#include <linux/of.h>
#include <linux/of_platform.h>

#include <soc/rockchip/rockchip_dvbm.h>


#if IS_ENABLED(CONFIG_ROCKCHIP_DVBM)
int rkvpss_dvbm_get(struct rkvpss_device *vpss_dev);
int rkvpss_dvbm_init(struct rkvpss_stream *stream);
void rkvpss_dvbm_deinit(struct rkvpss_device *vpss_dev);
int rkvpss_dvbm_event(struct rkvpss_device *vpss_dev, u32 event);
#else
static inline int rkvpss_dvbm_get(struct rkvpss_device *vpss_dev) {return -EINVAL; }
static inline int rkvpss_dvbm_init(struct rkvpss_stream *stream) {return -EINVAL; }
static inline void rkvpss_dvbm_deinit(struct rkvpss_device *vpss_dev) {}
static inline int rkvpss_dvbm_event(struct rkvpss_device *vpss_dev, u32 event) {return -EINVAL; }
#endif

#endif

