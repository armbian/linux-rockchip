/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_PDAF_H
#define _RKISP_PDAF_H

#include "common.h"

struct rkisp_pdaf_vdev;

struct rkisp_pdaf_vdev {
	struct rkisp_device *dev;
	struct rkisp_vdev_node vnode;

	spinlock_t vbq_lock;
	struct mutex api_lock;
	struct list_head buf_queue;
	struct list_head buf_done_list;
	struct tasklet_struct buf_done_tasklet;
	struct v4l2_pix_format_mplane fmt;
	struct rkisp_buffer *curr_buf;
	struct rkisp_buffer *next_buf;
	wait_queue_head_t done;
	bool streaming;
	bool stopping;
};

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39) || IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35)
void rkisp_pdaf_update_buf(struct rkisp_device *dev);
void rkisp_pdaf_isr(struct rkisp_device *dev);
int rkisp_register_pdaf_vdev(struct rkisp_device *dev);
void rkisp_unregister_pdaf_vdev(struct rkisp_device *dev);
#else
static inline void rkisp_pdaf_update_buf(struct rkisp_device *dev) {}
static inline void rkisp_pdaf_isr(struct rkisp_device *dev) {}
static inline int rkisp_register_pdaf_vdev(struct rkisp_device *dev) { return 0; }
static inline void rkisp_unregister_pdaf_vdev(struct rkisp_device *dev) {}
#endif

#endif
