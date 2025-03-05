/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Maxim Quad GMSL Deserializer driver API function declaration
 *
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#ifndef __MAXIM4C_API_H__
#define __MAXIM4C_API_H__

#include <linux/i2c.h>
#include <linux/i2c-mux.h>

#include "maxim4c_compact.h"
#include "maxim4c_i2c.h"
#include "maxim4c_link.h"
#include "maxim4c_video_pipe.h"
#include "maxim4c_mipi_txphy.h"
#include "maxim4c_pattern.h"
#include "maxim4c_drv.h"

/* Maxim Deserializer Test Pattern */
#define MAXIM4C_TEST_PATTERN		0

/* Maxim Deserializer pwdn on/off enable */
#define MAXIM4C_LOCAL_DES_ON_OFF_EN	0

/* maxim4c i2c mux api */
int maxim4c_i2c_mux_enable(maxim4c_t *maxim4c, u8 def_mask);
int maxim4c_i2c_mux_disable(maxim4c_t *maxim4c);
int maxim4c_i2c_mux_init(maxim4c_t *maxim4c);
int maxim4c_i2c_mux_deinit(maxim4c_t *maxim4c);

/* maxim4c link api */
u8 maxim4c_link_get_lock_state(maxim4c_t *maxim4c, u8 link_mask);
int maxim4c_link_oneshot_reset(maxim4c_t *maxim4c, u8 link_mask);
int maxim4c_link_mask_enable(maxim4c_t *maxim4c, u8 link_mask, bool enable);
int maxim4c_link_wait_linklock(maxim4c_t *maxim4c, u8 link_mask);
int maxim4c_link_select_remote_enable(maxim4c_t *maxim4c, u8 link_mask);
int maxim4c_link_select_remote_control(maxim4c_t *maxim4c, u8 link_mask);
int maxim4c_link_hw_init(maxim4c_t *maxim4c);
void maxim4c_link_data_init(maxim4c_t *maxim4c);
int maxim4c_link_parse_dt(maxim4c_t *maxim4c, struct device_node *of_node);

/* maxim4c video pipe api */
int maxim4c_video_pipe_hw_init(maxim4c_t *maxim4c);
int maxim4c_video_pipe_mask_enable(maxim4c_t *maxim4c, u8 video_pipe_mask, bool enable);
int maxim4c_video_pipe_linkid_enable(maxim4c_t *maxim4c, u8 link_id, bool enable);
void maxim4c_video_pipe_data_init(maxim4c_t *maxim4c);
int maxim4c_video_pipe_parse_dt(maxim4c_t *maxim4c, struct device_node *of_node);

/* maxim4c mipi txphy api */
int maxim4c_mipi_txphy_hw_init(maxim4c_t *maxim4c);
void maxim4c_mipi_txphy_data_init(maxim4c_t *maxim4c);
int maxim4c_mipi_txphy_parse_dt(maxim4c_t *maxim4c, struct device_node *of_node);
int maxim4c_mipi_txphy_enable(maxim4c_t *maxim4c, bool enable);
int maxim4c_dphy_dpll_predef_set(maxim4c_t *maxim4c, s64 link_freq_hz);
int maxim4c_mipi_csi_output(maxim4c_t *maxim4c, bool enable);

/* maxim4c remote api */
int maxim4c_remote_devices_power(maxim4c_t *maxim4c, u8 link_mask, int on);
int maxim4c_remote_devices_s_stream(maxim4c_t *maxim4c, u8 link_mask, int enable);

/* maxim4c v4l2 subdev api */
int maxim4c_v4l2_subdev_init(maxim4c_t *maxim4c);
void maxim4c_v4l2_subdev_deinit(maxim4c_t *maxim4c);

/* maxim4c driver api */
int maxim4c_module_hw_init(maxim4c_t *maxim4c);
int maxim4c_hot_plug_detect_work_start(maxim4c_t *maxim4c);

/* maxim4c pattern api */
int maxim4c_pattern_hw_init(maxim4c_t *maxim4c);
int maxim4c_pattern_support_mode_init(maxim4c_t *maxim4c);
int maxim4c_pattern_data_init(maxim4c_t *maxim4c);
int maxim4c_pattern_enable(maxim4c_t *maxim4c, bool enable);

int maxim4c_dbgfs_init(maxim4c_t *maxim4c);
void maxim4c_dbgfs_deinit(maxim4c_t *maxim4c);

#endif /* __MAXIM4C_API_H__ */
