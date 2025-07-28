/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_COMMON_H
#define _RKVPSS_COMMON_H

#include <linux/clk.h>
#include <linux/kfifo.h>
#include <linux/mutex.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "../isp/isp_vpss.h"
#include <linux/rk-camera-module.h>

#define RKVPSS_PLANE_Y		0
#define RKVPSS_PLANE_UV		1

#define RKVPSS_DEFAULT_WIDTH	1920
#define RKVPSS_DEFAULT_HEIGHT	1080
#define RKVPSS_MAX_WIDTH	4672
#define RKVPSS_MAX_HEIGHT	3504
#define RKVPSS_MAX_WIDTH_V20	4096
#define RKVPSS_MAX_HEIGHT_V20	3072
#define RKVPSS_MIN_WIDTH	32
#define RKVPSS_MIN_HEIGHT	32
#define RKVPSS_UNITE_MAX_WIDTH        8192
#define RKVPSS_UNITE_MAX_HEIGHT       6144
#define RKVPSS_VIDEO_NAME_LEN   16

#define RKVPSS_REG_CACHE_SYNC	0xeeeeeeee
#define RKVPSS_REG_CACHE	0xffffffff
#define RKVPSS_SW_REG_SIZE	0x37c0
#define RKVPSS_SW_REG_SIZE_MAX	(RKVPSS_SW_REG_SIZE * 2)

struct rkvpss_device;

enum rkvpss_ver {
	VPSS_V10 = 0x00,
	VPSS_V20 = 0x20,
};

enum {
	ROCKIT_DVBM_END,
	ROCKIT_DVBM_START,
};

enum {
	DVBM_DEINIT = 0,
	DVBM_ONLINE = 1,
	DVBM_OFFLINE = 2,
};

enum rkvpss_fmt_pix_type {
	FMT_YUV,
	FMT_RGB,
	FMT_TILE,
	FMT_FBC,
};

enum rkvpss_rotate {
	ROTATE_0 = 0,
	ROTATE_90,
	ROTATE_180,
	ROTATE_270,
};

/* One structure per video node */
struct rkvpss_vdev_node {
	struct vb2_queue buf_queue;
	struct video_device vdev;
	struct media_pad pad;
};

struct rkvpss_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head queue;
	u32 dma[VIDEO_MAX_PLANES];
	void *vaddr[VIDEO_MAX_PLANES];
};

static inline struct rkvpss_vdev_node *vdev_to_node(struct video_device *vdev)
{
	return container_of(vdev, struct rkvpss_vdev_node, vdev);
}

static inline struct rkvpss_vdev_node *queue_to_node(struct vb2_queue *q)
{
	return container_of(q, struct rkvpss_vdev_node, buf_queue);
}

static inline struct rkvpss_buffer *to_rkvpss_buffer(struct vb2_v4l2_buffer *vb)
{
	return container_of(vb, struct rkvpss_buffer, vb);
}

static inline struct vb2_queue *to_vb2_queue(struct file *file)
{
	struct rkvpss_vdev_node *vnode = video_drvdata(file);

	return &vnode->buf_queue;
}

static inline int vpss_outchn_max(int version)
{
	if (version == VPSS_V10)
		return 4;
	else if (version == VPSS_V20)
		return 6;

	return 0;
}

extern int rkvpss_debug;
extern int rkvpss_buf_dbg;
extern struct platform_driver rkvpss_plat_drv;
extern int rkvpss_cfginfo_num;

void rkvpss_idx_write(struct rkvpss_device *dev, u32 reg, u32 val, int idx);
void rkvpss_unite_write(struct rkvpss_device *dev, u32 reg, u32 val);
void rkvpss_idx_set_bits(struct rkvpss_device *dev, u32 reg, u32 mask, u32 val, int idx);
void rkvpss_unite_set_bits(struct rkvpss_device *dev, u32 reg, u32 mask, u32 val);
u32 rkvpss_idx_read(struct rkvpss_device *dev, u32 reg, int idx);
void rkvpss_idx_clear_bits(struct rkvpss_device *dev, u32 reg, u32 mask, int idx);
void rkvpss_unite_clear_bits(struct rkvpss_device *dev, u32 reg, u32 mask);
void rkvpss_update_regs(struct rkvpss_device *dev, u32 start, u32 end);

int rkvpss_attach_hw(struct rkvpss_device *vpss);
void rkvpss_detach_hw(struct rkvpss_device *vpss);
void rkvpss_set_clk_rate(struct clk *clk, unsigned long rate);
#endif
