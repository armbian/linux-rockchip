/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKAIISP_DEV_H
#define _RKAIISP_DEV_H

#include <linux/clk.h>
#include <linux/media.h>
#include <linux/mutex.h>
#include <linux/rk-video-format.h>
#include <linux/rk-aiisp-config.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/videobuf2-dma-sg.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-event.h>
#include "hw.h"

#define DRIVER_NAME "rkaiisp"

#define RKAIISP_SUBDEV_NAME DRIVER_NAME	"-subdev"
#define RKAIISP_V4L2_EVENT_ELEMS	4

#define RKAIISP_MAX_CHANNEL		7
#define RKAIISP_TMP_BUF_CNT		2
#define RKAIISP_DEFAULT_MAXRUNCNT	8
#define RKAIISP_DEFAULT_PARASIZE	(16 * 1024)
#define RKAIISP_SW_REG_SIZE		0x3000
#define RKAIISP_SW_MAX_SIZE		(RKAIISP_SW_REG_SIZE * 2)
#define RKAIISP_AIRMS_BUF_MAXCNT	8
#define RKAIISP_MIN(a, b)		((a) < (b) ? (a) : (b))

enum rkaiisp_irqhdl_ret {
	NOT_WREND		= (0 << 0),
	CONTINUE_RUN		= (1 << 0),
	RUN_COMPLETE		= (2 << 0)
};

enum rkaiisp_hwstate {
	HW_STOP,
	HW_RUNNING
};

struct rkaiisp_vdev_node {
	struct vb2_queue buf_queue;
	struct video_device vdev;
	struct media_pad pad;
};

struct rkaiisp_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head queue;
	u32 buff_addr[VIDEO_MAX_PLANES];
	void *vaddr[VIDEO_MAX_PLANES];
};

struct rkaiisp_dummy_buffer {
	struct vb2_buffer vb;
	struct vb2_queue vb2_queue;
	s32 dma_fd;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *dba;
	struct sg_table	*sgt;
	dma_addr_t dma_addr;
	void *vaddr;
	u32 size;
	void *mem_priv;
	bool is_need_vaddr;
	bool is_need_dbuf;
	bool is_need_dmafd;
};

struct rkaiisp_buffer_size {
	u32 height;
	u32 width;
	u32 channel;
	u32 stride;
};

struct rkaiiisp_subdev {
	struct v4l2_subdev sd;
	bool is_subs_evt;
};

struct rkaiisp_device {
	char name[128];
	void *sw_base_addr;
	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct proc_dir_entry *procfs;
	struct device *dev;

	const struct vb2_mem_ops *mem_ops;
	spinlock_t config_lock;
	struct mutex apilock;
	wait_queue_head_t sync_onoff;
	atomic_t opencnt;

	struct rkaiisp_hw_dev *hw_dev;
	bool is_hw_link;
	int dev_id;

	struct rkaiiisp_subdev subdev;

	struct rkaiisp_ispbuf_info ispbuf;
	struct rkaiisp_dummy_buffer iirbuf[RKISP_BUFFER_MAX];
	struct rkaiisp_dummy_buffer aiprebuf[RKISP_BUFFER_MAX];
	struct rkaiisp_dummy_buffer vpslbuf[RKISP_BUFFER_MAX];
	struct rkaiisp_dummy_buffer aiispbuf[RKISP_BUFFER_MAX];
	struct rkaiisp_dummy_buffer temp_buf[RKAIISP_TMP_BUF_CNT];
	u32 outbuf_idx;

	struct rkaiisp_rmsbuf_info rmsbuf;
	struct rkaiisp_dummy_buffer rms_inbuf[RKAIISP_AIRMS_BUF_MAXCNT];
	struct rkaiisp_dummy_buffer rms_outbuf[RKAIISP_AIRMS_BUF_MAXCNT];
	struct rkaiisp_dummy_buffer sigma_buf;
	struct rkaiisp_dummy_buffer narmap_buf;

	struct kfifo idxbuf_kfifo;
	union rkaiisp_queue_buf curr_idxbuf;

	struct rkaiisp_vdev_node vnode;
	struct list_head params;
	struct rkaiisp_buffer *cur_params;
	struct v4l2_format vdev_fmt;

	struct rkaiisp_buffer_size outbuf_size[RKAIISP_MAX_RUNCNT];
	struct rkaiisp_buffer_size chn_size[RKAIISP_MAX_CHANNEL];
	enum rkaiisp_exealgo exealgo;
	enum rkaiisp_exemode exemode;
	enum rkaiisp_model_mode model_mode;
	enum rkaiisp_hwstate hwstate;
	u32 para_size;
	u32 max_runcnt;
	u32 model_runcnt;
	u32 run_idx;
	u32 frame_id;

	u64 pre_frm_st;
	u64 frm_st;
	u64 frm_ed;
	u32 frm_interval;
	u32 frm_oversdtim_cnt;
	u32 isr_buserr_cnt;
	u32 isr_wrend_cnt;

	bool streamon;
	bool showreg;
	bool init_buf;
};

extern int rkaiisp_debug;
extern int rkaiisp_showreg;
extern int rkaiisp_stdfps;

static inline struct rkaiisp_buffer *to_rkaiisp_buffer(struct vb2_v4l2_buffer *vb)
{
	return container_of(vb, struct rkaiisp_buffer, vb);
}

static inline void rkaiisp_write(struct rkaiisp_device *aidev, u32 reg, u32 val, bool is_direct)
{
	u32 *mem = aidev->sw_base_addr + reg;
	u32 *flag = aidev->sw_base_addr + reg + RKAIISP_SW_REG_SIZE;

	*mem = val;
	*flag = SW_REG_CACHE;
	if (aidev->hw_dev->is_single || is_direct) {
		*flag = SW_REG_CACHE_SYNC;
		writel(val, aidev->hw_dev->base_addr + reg);
	}
}

static inline u32 rkaiisp_read(struct rkaiisp_device *aidev, u32 reg, bool is_direct)
{
	u32 val;

	if (aidev->hw_dev->is_single || is_direct)
		val = readl(aidev->hw_dev->base_addr + reg);
	else
		val = *(u32 *)(aidev->sw_base_addr + reg);

	return val;
}

extern struct platform_driver rkaiisp_plat_drv;
int rkaiisp_queue_ispbuf(struct rkaiisp_device *aidev, union rkaiisp_queue_buf *idxbuf);
void rkaiisp_update_list_reg(struct rkaiisp_device *aidev);
void rkaiisp_trigger(struct rkaiisp_device *aidev);
int rkaiisp_get_idxbuf_len(struct rkaiisp_device *aidev);
enum rkaiisp_irqhdl_ret rkaiisp_irq_hdl(struct rkaiisp_device *aidev, u32 mi_mis);
int rkaiisp_register_vdev(struct rkaiisp_device *aidev, struct v4l2_device *v4l2_dev);
void rkaiisp_unregister_vdev(struct rkaiisp_device *aidev);

#endif
