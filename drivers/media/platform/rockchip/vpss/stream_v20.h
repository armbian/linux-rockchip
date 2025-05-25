/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */


#ifndef _RKVPSS_STREAM_V20_H
#define _RKVPSS_STREAM_V20_H

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

struct rockit_vpss_ops {
	int (*rkvpss_stream_start)(struct rkvpss_stream *stream);
	void (*rkvpss_stream_stop)(struct rkvpss_stream *stream);
	int (*rkvpss_set_fmt)(struct rkvpss_stream *stream,
			      struct v4l2_pix_format_mplane *pixm,
			      bool try);
};

int rkvpss_rockit_buf_done(struct rkvpss_stream *stream, int cmd, struct rkvpss_buffer *curr_buf);
int rkvpss_rockit_buf_free(struct rkvpss_stream *stream);
void rkvpss_rockit_buf_state_clear(struct rkvpss_stream *stream);
void rkvpss_rockit_frame_start(struct rkvpss_device *dev);


#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_VPSS_V20)
int rkvpss_register_stream_vdevs_v20(struct rkvpss_device *dev);
void rkvpss_unregister_stream_vdevs_v20(struct rkvpss_device *dev);
void rkvpss_stream_default_fmt_v20(struct rkvpss_device *dev, u32 id,
			       u32 width, u32 height, u32 pixelformat);
void rkvpss_isr_v20(struct rkvpss_device *dev, u32 mis_val);
void rkvpss_mi_isr_v20(struct rkvpss_device *dev, u32 mis_val);
void rkvpss_cmsc_config_v20(struct rkvpss_device *dev, bool sync);
int rkvpss_stream_buf_cnt_v20(struct rkvpss_stream *stream);

#else
static inline int rkvpss_register_stream_vdevs_v20(struct rkvpss_device *dev) {return -EINVAL; }
static inline void rkvpss_unregister_stream_vdevs_v20(struct rkvpss_device *dev) {}
static inline void rkvpss_stream_default_fmt_v20(struct rkvpss_device *dev, u32 id, u32 width, u32 height, u32 pixelformat) {}
static inline void rkvpss_isr_v20(struct rkvpss_device *dev, u32 mis_val) {}
static inline void rkvpss_mi_isr_v20(struct rkvpss_device *dev, u32 mis_val) {}
static inline void rkvpss_cmsc_config_v20(struct rkvpss_device *dev, bool sync) {}
static inline int rkvpss_stream_buf_cnt_v20(struct rkvpss_stream *stream) {return 0; }

#endif

#endif

