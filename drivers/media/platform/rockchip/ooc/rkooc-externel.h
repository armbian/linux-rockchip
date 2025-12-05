/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKOOC_EXTERNEL_H
#define _RKOOC_EXTERNEL_H

#include <media/v4l2-dev.h>

#define RKOOC_CMD_CONFIG_SENSOR	1
#define RKOOC_SENSOR_GST412C	10
#define RKOOC_SENSOR_H3812C1SH	11

struct rkooc_config_win {
	u16 image_width;
	u16 image_height;
	u16 ooc_width;
	u16 ooc_height;
};

#endif
