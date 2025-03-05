/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Maxim Dual GMSL Deserializer driver API function declaration
 *
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#ifndef __MAXIM2C_API_H__
#define __MAXIM2C_API_H__

#include <linux/i2c.h>
#include <linux/i2c-mux.h>

#include "maxim2c_compact.h"
#include "maxim2c_i2c.h"
#include "maxim2c_link.h"
#include "maxim2c_video_pipe.h"
#include "maxim2c_mipi_txphy.h"
#include "maxim2c_pattern.h"
#include "maxim2c_drv.h"

/* Maxim Deserializer Test Pattern */
#define MAXIM2C_TEST_PATTERN		0

/* Maxim Deserializer pwdn on/off enable */
#define MAXIM2C_LOCAL_DES_ON_OFF_EN	0

/* maxim2c i2c mux api */
int maxim2c_i2c_mux_enable(maxim2c_t *maxim2c, u8 def_mask);
int maxim2c_i2c_mux_disable(maxim2c_t *maxim2c);
int maxim2c_i2c_mux_init(maxim2c_t *maxim2c);
int maxim2c_i2c_mux_deinit(maxim2c_t *maxim2c);

/* maxim2c link api */
u8 maxim2c_link_get_lock_state(maxim2c_t *maxim2c, u8 link_mask);
int maxim2c_link_oneshot_reset(maxim2c_t *maxim2c, u8 link_mask);
int maxim2c_link_mask_enable(maxim2c_t *maxim2c, u8 link_mask, bool enable);
int maxim2c_link_wait_linklock(maxim2c_t *maxim2c, u8 link_mask);
int maxim2c_link_select_remote_enable(maxim2c_t *maxim2c, u8 link_mask);
int maxim2c_link_select_remote_control(maxim2c_t *maxim2c, u8 link_mask);
int maxim2c_link_hw_init(maxim2c_t *maxim2c);
void maxim2c_link_data_init(maxim2c_t *maxim2c);
int maxim2c_link_parse_dt(maxim2c_t *maxim2c, struct device_node *of_node);

/* maxim2c video pipe api */
int maxim2c_video_pipe_hw_init(maxim2c_t *maxim2c);
int maxim2c_video_pipe_mask_enable(maxim2c_t *maxim2c, u8 video_pipe_mask, bool enable);
int maxim2c_video_pipe_linkid_enable(maxim2c_t *maxim2c, u8 link_id, bool enable);
void maxim2c_video_pipe_data_init(maxim2c_t *maxim2c);
int maxim2c_video_pipe_parse_dt(maxim2c_t *maxim2c, struct device_node *of_node);

/* maxim2c mipi txphy api */
int maxim2c_mipi_txphy_hw_init(maxim2c_t *maxim2c);
void maxim2c_mipi_txphy_data_init(maxim2c_t *maxim2c);
int maxim2c_mipi_txphy_parse_dt(maxim2c_t *maxim2c, struct device_node *of_node);
int maxim2c_mipi_txphy_enable(maxim2c_t *maxim2c, bool enable);
int maxim2c_dphy_dpll_predef_set(maxim2c_t *maxim2c, s64 link_freq_hz);
int maxim2c_mipi_csi_output(maxim2c_t *maxim2c, bool enable);

/* maxim2c remote api */
int maxim2c_remote_devices_power(maxim2c_t *maxim2c, u8 link_mask, int on);
int maxim2c_remote_devices_s_stream(maxim2c_t *maxim2c, u8 link_mask, int enable);

/* maxim2c v4l2 subdev api */
int maxim2c_v4l2_subdev_init(maxim2c_t *maxim2c);
void maxim2c_v4l2_subdev_deinit(maxim2c_t *maxim2c);

/* maxim2c driver api */
int maxim2c_module_hw_init(maxim2c_t *maxim2c);
int maxim2c_hot_plug_detect_work_start(maxim2c_t *maxim2c);

/* maxim2c pattern api */
int maxim2c_pattern_hw_init(maxim2c_t *maxim2c);
int maxim2c_pattern_support_mode_init(maxim2c_t *maxim2c);
int maxim2c_pattern_data_init(maxim2c_t *maxim2c);
int maxim2c_pattern_enable(maxim2c_t *maxim2c, bool enable);

#endif /* __MAXIM2C_API_H__ */
