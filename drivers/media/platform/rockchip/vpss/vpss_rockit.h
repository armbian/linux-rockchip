/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_ROCKIT_H
#define _RKVPSS_ROCKIT_H

#include <linux/of.h>
#include <linux/of_platform.h>
#include <soc/rockchip/rockchip_rockit.h>

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_VPSS_V20)
extern struct rockit_vpss_ops rockit_vpss_ops;

void rkvpss_rockit_dev_init(struct rkvpss_device *dev);
void rkvpss_rockit_dev_deinit(void);
#endif

#endif

