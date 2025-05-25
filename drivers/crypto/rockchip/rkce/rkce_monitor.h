/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_MONITOR_H__
#define __RKCE_MONITOR_H__

#include <linux/types.h>

#include "rkce_core.h"

int rkce_monitor_add(void *td, request_cb_func callback);

void rkce_monitor_del(void *td);

int rkce_monitor_init(void);

void rkce_monitor_deinit(void);

#endif
