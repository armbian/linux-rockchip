/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_STATS_V35_H
#define _RKISP_STATS_V35_H

#define ISP35_RD_STATS_BUF_SIZE 0x10000

struct rkisp_isp_stats_vdev;

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35)
void rkisp_init_stats_vdev_v35(struct rkisp_isp_stats_vdev *stats_vdev);
void rkisp_uninit_stats_vdev_v35(struct rkisp_isp_stats_vdev *stats_vdev);
#else
static inline void rkisp_init_stats_vdev_v35(struct rkisp_isp_stats_vdev *stats_vdev) {}
static inline void rkisp_uninit_stats_vdev_v35(struct rkisp_isp_stats_vdev *stats_vdev) {}
#endif

#endif /* _RKISP_STATS_V35_H */
