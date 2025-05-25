/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKFEC_PROCFS_H
#define _RKFEC_PROCFS_H

#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/sem.h>
#include <linux/seq_file.h>
#include <media/v4l2-common.h>

#ifdef CONFIG_PROC_FS
int rkfec_offline_proc_init(struct rkfec_offline_dev *dev);
void rkfec_offline_proc_cleanup(struct rkfec_offline_dev *dev);
#else
static inline int rkfec_offline_proc_init(struct rkfec_offline_dev *dev) { return 0; }
static inline void rkfec_offline_proc_cleanup(struct rkfec_offline_dev *dev) {}
#endif

#endif
