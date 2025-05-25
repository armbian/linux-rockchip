/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_STATS_V39_H
#define _RKISP_STATS_V39_H

#include <linux/rk-isp1-config.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include "common.h"

#define ISP39_RD_STATS_BUF_SIZE 0x10000

struct rkisp_isp_stats_vdev;

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39)
void rkisp_init_stats_vdev_v39(struct rkisp_isp_stats_vdev *stats_vdev);
void rkisp_uninit_stats_vdev_v39(struct rkisp_isp_stats_vdev *stats_vdev);
#else
static inline void rkisp_init_stats_vdev_v39(struct rkisp_isp_stats_vdev *stats_vdev) {}
static inline void rkisp_uninit_stats_vdev_v39(struct rkisp_isp_stats_vdev *stats_vdev) {}
#endif

#endif /* _RKISP_STATS_V39_H */
