/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:
 *      Zhang Yubing <yubing.zhang@rock-chips.com>
 */

#ifndef _ROCKCHIP_POST_CSC_H
#define _ROCKCHIP_POST_CSC_H

#include <drm/drm_crtc.h>
#include "rockchip_drm_drv.h"
#include "rockchip_drm_vop.h"

int rockchip_calc_post_csc(struct post_csc *csc_cfg, struct post_csc_coef *csc_simple_coef,
			   struct post_csc_convert_mode *convert_mode);

#endif
