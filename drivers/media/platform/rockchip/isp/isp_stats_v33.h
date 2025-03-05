/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_STATS_V33_H
#define _RKISP_STATS_V33_H

#include <linux/rk-isp1-config.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include "common.h"

struct rkisp_isp_stats_vdev;

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V33)
void rkisp_stats_first_ddr_config_v33(struct rkisp_isp_stats_vdev *stats_vdev);
void rkisp_stats_next_ddr_config_v33(struct rkisp_isp_stats_vdev *stats_vdev);
void rkisp_init_stats_vdev_v33(struct rkisp_isp_stats_vdev *stats_vdev);
void rkisp_uninit_stats_vdev_v33(struct rkisp_isp_stats_vdev *stats_vdev);
#else
static inline void rkisp_stats_first_ddr_config_v33(struct rkisp_isp_stats_vdev *stats_vdev) {}
static inline void rkisp_stats_next_ddr_config_v33(struct rkisp_isp_stats_vdev *stats_vdev) {}
static inline void rkisp_init_stats_vdev_v33(struct rkisp_isp_stats_vdev *stats_vdev) {}
static inline void rkisp_uninit_stats_vdev_v33(struct rkisp_isp_stats_vdev *stats_vdev) {}
#endif

#endif /* _RKISP_ISP_STATS_V33_H */
