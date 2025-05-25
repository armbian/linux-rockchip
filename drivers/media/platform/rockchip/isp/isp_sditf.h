/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_SDITF_H
#define _RKISP_SDITF_H

/* struct rkisp_sditf_device
 * isp subdev interface link other media device
 */
struct rkisp_sditf_device {
	struct device *dev;
	struct rkisp_device *isp;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_async_notifier notifier;
	struct v4l2_subdev *remote_sd;

	bool is_on;
};

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39) || IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35)
extern struct platform_driver rkisp_sditf_drv;
void rkisp_sditf_sof(struct rkisp_device *dev, u32 irq);
void rkisp_sditf_reset_notify_vpss(struct rkisp_device *dev);
#else
static inline void rkisp_sditf_sof(struct rkisp_device *dev, u32 irq) {}
static inline void rkisp_sditf_reset_notify_vpss(struct rkisp_device *dev) {}
#endif

#endif
