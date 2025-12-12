/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKOOC_DEV_H_
#define _RKOOC_DEV_H_

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-ioctl.h>
#include <media/media-device.h>
#include "rkooc-externel.h"

/* buffer for one video frame */
struct rkooc_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head	list;
	dma_addr_t dma_addr;
	void *vaddr;
	u32 size;
};

struct rkooc_dummy_buffer {
	struct vb2_v4l2_buffer vb;
	struct vb2_queue vb2_queue;
	u32 size;
	void *mem_priv;
	void *vaddr;
	dma_addr_t dma_addr;
};


struct rkooc_dev {
	struct v4l2_device v4l2_dev;

	void __iomem *ooc_base;
	struct clk *pmclk;
	struct clk *hclk;
	struct clk *aclk;
	int irq;
	const struct vb2_mem_ops *mem_ops;

#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_device mdev;
	struct media_pad vid_cap_pad;
	struct media_pad vid_out_pad;
	struct media_pad vid_rx_pad;
	struct media_pad subdev_pad;
#endif
	struct video_device vid_cap_dev;
	struct video_device vid_out_dev;
	struct video_device irfpa_rx_dev;
	struct v4l2_subdev ooc_sd;

	/* capabilities */
	u32 vid_cap_caps;
	u32 vid_out_caps;

	/* controls */
	struct v4l2_ctrl_handler ctrl_hdl;
	struct v4l2_ctrl *brightness;

	struct vb2_queue irfpa_tx_queue;
	struct list_head vid_cap_active;

	struct vb2_queue vb_vid_out_q;
	struct list_head vid_out_active;

	struct vb2_queue irfpa_rx_queue;
	struct list_head irfpa_rx_buffers;

	spinlock_t slock;
	spinlock_t irfpa_lock;
	struct mutex mutex;

	u32 vid_cap_seq_count;
	u32 vid_out_seq_count;
	u32 irfpa_rx_seq;
	bool have_dummy;
	struct rkooc_dummy_buffer dummy;
	struct rkooc_dummy_buffer reglist;
	struct rkooc_buffer *cur_buf;

	u8 sensor;
	struct rkooc_config_win win;
	u16 reg_bits;

	struct v4l2_rect irfpatx_crop;
	struct v4l2_rect irfpatx_compose;
	u16 irfpatx_width;
	u16 irfpatx_height;
	u32 irfpatx_fourcc;
	u32 irfpatx_sizeimage;

	u32 ooctx_num;
	bool full_mode;
};

static inline u32 rkooc_read_reg(struct rkooc_dev *dev, u32 offset)
{
	return readl(dev->ooc_base + offset);
}

static inline void rkooc_write_reg(struct rkooc_dev *dev, u32 offset, u32 value)
{
	writel(value, dev->ooc_base + offset);
}

// form hw.c
void rkooc_hw_init(struct rkooc_dev *dev);
void rkooc_hw_deinit(struct rkooc_dev *dev);
void rkooc_hw_pmclk_enable(struct rkooc_dev *dev);
void rkooc_hw_pmclk_disable(struct rkooc_dev *dev);
void rkooc_hw_enable_irq(struct rkooc_dev *dev);
void rkooc_hw_disable_irq(struct rkooc_dev *dev);

void rkooc_hw_start(struct rkooc_dev *dev);
void rkooc_hw_stop(struct rkooc_dev *dev);
void rkooc_hw_update_win_addr(struct rkooc_dev *dev, u32 addr);

// from capture.c
int rkooc_create_cap_dev(struct rkooc_dev *dev);
void rkooc_remove_cap_dev(struct rkooc_dev *dev);
// from occ.c
int rkooc_create_ooc_dev(struct rkooc_dev *dev);
void rkooc_remove_ooc_dev(struct rkooc_dev *dev);

//from subdev.c
int rkooc_register_ooc_subdev(struct rkooc_dev *dev);
void rkooc_unregister_ooc_subdev(struct rkooc_dev *dev);

int rkooc_create_rx_dev(struct rkooc_dev *dev);
void rkooc_remove_rx_dev(struct rkooc_dev *dev);
#endif

