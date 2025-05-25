/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKAIISP_PROCFS_H
#define _RKAIISP_PROCFS_H

#include <linux/proc_fs.h>

struct rkaiisp_device;

#ifdef CONFIG_PROC_FS
int rkaiisp_proc_init(struct rkaiisp_device *aidev);
void rkaiisp_proc_cleanup(struct rkaiisp_device *aidev);
#else
static inline int rkaiisp_proc_init(struct rkaiisp_device *aidev) { return 0; }
static inline void rkaiisp_proc_cleanup(struct rkaiisp_device *aidev) {}
#endif

#endif
