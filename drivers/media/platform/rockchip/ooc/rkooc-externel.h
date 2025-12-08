/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKOOC_EXTERNEL_H
#define _RKOOC_EXTERNEL_H

#include <media/v4l2-dev.h>

#define RKOOC_CMD_CONFIG_SENSOR	1
#define RKOOC_SENSOR_GST412C	10
#define RKOOC_SENSOR_H3812C1SH	11

#define RKOOC_CMD_CONFIG_FULLMODE 1
#define RKOOC_CMD_CONFIG_DATAMODE 2

struct rkooc_config_fullmode {
	u32 fmclk;
	// TODO
};

struct rkooc_config_datamode {
	u32 fmclk;

	u16 xwin_size;
	u16 ywin_size;
	u16 xblank_size;
	u16 yblank_size;

	u16 hs_word_size;
	u32 hs_word;
	u16 fs_word_size;
	u32 fs_word;

	u16 fs_delay_size;

	// data send during blank
	u8 blank_key;

	u16 image_width;
	u16 image_height;

	u16 ooc_width;
	u16 ooc_height;
};
#endif
