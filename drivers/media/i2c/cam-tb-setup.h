/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Rockchip Electronics Co., Ltd. */

#ifndef CAM_TB_SETUP_H
#define CAM_TB_SETUP_H
#include <linux/types.h>

#ifdef CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_SETUP
#define EXTERN_CAM_TB_PARAM(para) u32 get_##para(void)
#else
#define EXTERN_CAM_TB_PARAM(para) \
static inline u32 get_##para(void) \
{ \
	return 0; \
}
#endif

EXTERN_CAM_TB_PARAM(rk_cam_w);
EXTERN_CAM_TB_PARAM(rk_cam_h);
EXTERN_CAM_TB_PARAM(rk_cam_hdr);
EXTERN_CAM_TB_PARAM(rk_cam_fps);
EXTERN_CAM_TB_PARAM(rk_cam1_max_fps);
EXTERN_CAM_TB_PARAM(rk_cam2_w);
EXTERN_CAM_TB_PARAM(rk_cam2_h);
EXTERN_CAM_TB_PARAM(rk_cam2_hdr);
EXTERN_CAM_TB_PARAM(rk_cam2_fps);
EXTERN_CAM_TB_PARAM(rk_cam2_max_fps);
EXTERN_CAM_TB_PARAM(rk_cam_skip);
EXTERN_CAM_TB_PARAM(rockit_en_mcu);
#endif
