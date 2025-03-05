// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2023 Rockchip Electronics Co., Ltd.

#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include "cam-tb-setup.h"


#define DEFINE_CAM_TB_PARAM(para) \
static u32 para; \
static int __init para##_setup(char *str) \
{ \
	int ret = 0; \
	unsigned long val = 0; \
	ret = kstrtoul(str, 0, &val); \
	if (!ret) \
		para = (u32)val; \
	else \
		pr_err("get "#para" fail\n"); \
	return 0; \
} \
u32 get_##para(void) \
{ \
	return para; \
} \
EXPORT_SYMBOL(get_##para); \
__setup(#para"=", para##_setup)

DEFINE_CAM_TB_PARAM(rk_cam_w);
DEFINE_CAM_TB_PARAM(rk_cam_h);
DEFINE_CAM_TB_PARAM(rk_cam_hdr);
DEFINE_CAM_TB_PARAM(rk_cam_fps);
DEFINE_CAM_TB_PARAM(rk_cam1_max_fps);
DEFINE_CAM_TB_PARAM(rk_cam2_w);
DEFINE_CAM_TB_PARAM(rk_cam2_h);
DEFINE_CAM_TB_PARAM(rk_cam2_hdr);
DEFINE_CAM_TB_PARAM(rk_cam2_fps);
DEFINE_CAM_TB_PARAM(rk_cam2_max_fps);
DEFINE_CAM_TB_PARAM(rk_cam_skip);
DEFINE_CAM_TB_PARAM(rockit_en_mcu);

