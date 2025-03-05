/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_PROCFS_H
#define _RKVPSS_PROCFS_H

#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/sem.h>
#include <linux/seq_file.h>
#include <media/v4l2-common.h>

#ifdef CONFIG_PROC_FS
int rkvpss_proc_init(struct rkvpss_device *dev);
void rkvpss_proc_cleanup(struct rkvpss_device *dev);
int rkvpss_offline_proc_init(struct rkvpss_offline_dev *dev);
void rkvpss_offline_proc_cleanup(struct rkvpss_offline_dev *dev);

#else
static inline int rkvpss_proc_init(struct rkvpss_device *dev) { return 0; }
static inline void rkvpss_proc_cleanup(struct rkvpss_device *dev) {}
static inline int rkvpss_offline_proc_init(struct rkvpss_offline_dev *dev) { return 0; }
static inline void rkvpss_offline_proc_cleanup(struct rkvpss_offline_dev *dev) {}
#endif

#endif
