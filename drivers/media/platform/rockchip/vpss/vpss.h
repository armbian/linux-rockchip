/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_VPSS_H
#define _RKVPSS_VPSS_H

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/v4l2-event.h>
#include <linux/version.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <media/v4l2-common.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>
#include <uapi/linux/rk-video-format.h>

#include <linux/rk-vpss-config.h>


#define GRP_ID_VPSS		BIT(0)

enum rkvpss_pad {
	RKVPSS_PAD_SINK,
	RKVPSS_PAD_SOURCE,
	RKVPSS_PAD_MAX
};

enum rkvpss_state {
	VPSS_STOP = 0,
	VPSS_FS = BIT(0),
	VPSS_FE = BIT(1),
	VPSS_FRAME_END = BIT(2),
	VPSS_FRAME_SCL0 = BIT(3),
	VPSS_FRAME_SCL1 = BIT(4),
	VPSS_FRAME_SCL2 = BIT(5),
	VPSS_FRAME_SCL3 = BIT(6),
	VPSS_FRAME_SCL4 = BIT(7),
	VPSS_FRAME_SCL5 = BIT(8),

	VPSS_START = BIT(9),
	VPSS_RX_START = BIT(10),
};

struct vpsssd_fmt {
	u32 mbus_code;
	u32 fourcc;
	u32 width;
	u32 height;
	u8 wr_fmt;
};

struct rkvpss_subdev {
	struct rkvpss_device *dev;
	struct v4l2_subdev sd;
	struct media_pad pads[RKVPSS_PAD_MAX];
	struct v4l2_mbus_framefmt in_fmt;
	struct vpsssd_fmt out_fmt;
	u32 frame_seq;
	/* timestamp in ns */
	u64 frame_timestamp;
	enum rkvpss_state state;
};

int rkvpss_register_subdev(struct rkvpss_device *dev,
			   struct v4l2_device *v4l2_dev);
void rkvpss_unregister_subdev(struct rkvpss_device *dev);
void rkvpss_check_idle(struct rkvpss_device *dev, u32 irq);
#endif
