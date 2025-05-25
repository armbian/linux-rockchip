/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_STREAM_H
#define _RKVPSS_STREAM_H

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

#include <linux/interrupt.h>

#define STREAM_OUT_REQ_BUFS_MIN 0

struct rkvpss_stream;

/*
 * fourcc: pixel format
 * fmt_type: helper filed for pixel format
 * cplanes: number of colour planes
 * mplanes: number of stored memory planes
 * wr_fmt: defines format for reg
 * bpp: bits per pixel
 */
struct capture_fmt {
	u32 fourcc;
	u8 fmt_type;
	u8 cplanes;
	u8 mplanes;
	u8 swap;
	u32 wr_fmt;
	u32 output_fmt;
	u8 bpp[VIDEO_MAX_PLANES];
};

/* Different config for stream */
struct stream_config {
	const struct capture_fmt *fmts;
	unsigned int fmt_size;
	u32 frame_end_id;
	/* registers */
	struct {
		u32 ctrl;
		u32 update;
		u32 src_size;
		u32 dst_size;
		u32 hy_fac;
		u32 hc_fac;
		u32 vy_fac;
		u32 vc_fac;
		u32 hy_offs;
		u32 hc_offs;
		u32 vy_offs;
		u32 vc_offs;
		u32 hy_size;
		u32 hc_size;
		u32 hy_offs_mi;
		u32 hc_offs_mi;
		u32 in_crop_offs;
		u32 ctrl_shd;
		u32 src_size_shd;
		u32 dst_size_shd;
		u32 hy_fac_shd;
		u32 hc_fac_shd;
		u32 vy_fac_shd;
		u32 vc_fac_shd;
		u32 hy_offs_shd;
		u32 hc_offs_shd;
		u32 vy_offs_shd;
		u32 vc_offs_shd;
		u32 hy_size_shd;
		u32 hc_size_shd;
		u32 hy_offs_mi_shd;
		u32 hc_offs_mi_shd;
		u32 in_crop_offs_shd;
	} scale;
	struct {
		u32 ctrl;
		u32 update;
		u32 h_offs;
		u32 v_offs;
		u32 h_size;
		u32 v_size;
		u32 ctrl_shd;
		u32 h_offs_shd;
		u32 v_offs_shd;
		u32 h_size_shd;
		u32 v_size_shd;
		u32 ch_4_5_offs;
		u32 ch_4_5_size;
		u32 ch_4_5_offs_shd;
		u32 ch_4_5_size_shd;
	} crop;
	struct {
		u32 ctrl;
		u32 stride;
		u32 y_base;
		u32 uv_base;
		u32 y_size;
		u32 uv_size;
		u32 y_offs_cnt;
		u32 uv_offs_cnt;
		u32 y_pic_width;
		u32 y_pic_size;

		u32 ctrl_shd;
		u32 y_shd;
		u32 uv_shd;
	} mi;
};

/* Different reg ops for stream */
struct streams_ops {
	void (*config_mi)(struct rkvpss_stream *stream);
	void (*enable_mi)(struct rkvpss_stream *stream);
	void (*disable_mi)(struct rkvpss_stream *stream);
	void (*update_mi)(struct rkvpss_stream *stream);
	void (*frame_start)(struct rkvpss_stream *stream, u32 irq);
	int (*is_stopped)(struct rkvpss_stream *stream);
	int (*limit_check)(struct rkvpss_stream *stream,
			   struct v4l2_pix_format_mplane *try_fmt);
};

struct frame_debug_info {
	u64 timestamp;
	u32 interval;
	u32 delay;
	u32 id;
	u32 frameloss;
};

struct rkvpss_online_unite_params {
	u32 y_w_fac;
	u32 c_w_fac;
	u32 y_h_fac;
	u32 c_h_fac;
	u32 y_w_phase;
	u32 c_w_phase;
	u32 quad_crop_w;
	u32 scl_in_crop_w_y;
	u32 scl_in_crop_w_c;
	u32 right_scl_need_size_y;
	u32 right_scl_need_size_c;
};

/* struct rkvpss_stream - VPSS stream video device
 * id: stream video identify
 * buf_queue: queued buffer list
 * curr_buf: the buffer used for current frame
 * next_buf: the buffer used for next frame
 * done: wait frame end event queue
 * vbq_lock: lock to protect buf_queue
 * out_cap_fmt: the output of vpss
 * out_fmt: the output of v4l2 pix format
 * last_module: last function module
 * streaming: stream start flag
 * stopping: stream stop flag
 * linked: link enable flag
 * alpha: argb a value
 * avg_scl_down : CH0 and CH2 can use average scale down, 1-16 scale range
 */
struct rkvpss_stream {
	struct rkvpss_device *dev;
	struct rkvpss_vdev_node vnode;

	struct list_head buf_queue;
	struct list_head buf_done_list;
	struct tasklet_struct buf_done_tasklet;

	struct rkvpss_buffer *curr_buf;
	struct rkvpss_buffer *next_buf;
	wait_queue_head_t done;
	spinlock_t vbq_lock;

	struct streams_ops *ops;
	struct stream_config *config;
	struct capture_fmt out_cap_fmt;

	struct v4l2_rect crop;
	struct v4l2_pix_format_mplane out_fmt;
	struct frame_debug_info dbg;
	struct rkvpss_online_unite_params unite_params;

	int id;
	u32 alpha;
	u32 fbc_head_size;
	bool streaming;
	bool stopping;
	bool linked;
	bool flip_en;
	bool is_crop_upd;
	bool is_mf_upd;
	bool is_pause;
	bool is_attach_info;
	bool rockit_on;
	bool avg_scl_down;
};

/* rkvpss stream device */
struct rkvpss_stream_vdev {
	struct rkvpss_stream stream[RKVPSS_OUTPUT_MAX];
	atomic_t refcnt;
	u32 wrap_line;
};

void rkvpss_cmsc_config(struct rkvpss_device *dev, bool sync);
void rkvpss_stream_default_fmt(struct rkvpss_device *dev, u32 id,
			       u32 width, u32 height, u32 pixelformat);
void rkvpss_isr(struct rkvpss_device *dev, u32 mis_val);
void rkvpss_mi_isr(struct rkvpss_device *dev, u32 mis_val);
void rkvpss_unregister_stream_vdevs(struct rkvpss_device *dev);
int rkvpss_register_stream_vdevs(struct rkvpss_device *dev);
int rkvpss_stream_buf_cnt(struct rkvpss_stream *stream);
#endif
