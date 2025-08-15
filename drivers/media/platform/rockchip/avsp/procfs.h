/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKAVSP_PROCFS_H
#define _RKAVSP_PROCFS_H

#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/sem.h>
#include <linux/seq_file.h>

#ifdef CONFIG_PROC_FS
int rkavsp_proc_init(struct rkavsp_dev *dev);
void rkavsp_proc_cleanup(struct rkavsp_dev *dev);
#else
static inline int rkavsp_proc_init(struct rkavsp_dev *dev) { return 0; }
static inline void rkavsp_proc_cleanup(struct rkavsp_dev *dev) {}
#endif

#endif
