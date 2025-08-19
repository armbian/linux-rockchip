/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_STATS_V351S_H
#define _RKISP_STATS_V351S_H

struct rkisp_isp_stats_vdev;

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35_1)
void rkisp_init_stats_vdev_v351s(struct rkisp_isp_stats_vdev *stats_vdev);
void rkisp_uninit_stats_vdev_v351s(struct rkisp_isp_stats_vdev *stats_vdev);
#else
static inline void rkisp_init_stats_vdev_v351s(struct rkisp_isp_stats_vdev *stats_vdev) {}
static inline void rkisp_uninit_stats_vdev_v351s(struct rkisp_isp_stats_vdev *stats_vdev) {}
#endif

#endif /* _RKISP_STATS_V351S_H */
