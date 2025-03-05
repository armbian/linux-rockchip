/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip Flexbus CIF Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef _FLEXBUS_CIF_DEV_H
#define _FLEXBUS_CIF_DEV_H

#include <linux/mutex.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-mc.h>
#include <linux/workqueue.h>
#include <linux/rk-camera-module.h>
#include <linux/interrupt.h>
#include <linux/mfd/rockchip-flexbus.h>
#include "regs.h"

#define CIF_DRIVER_NAME		"flexbus"
#define CIF_VIDEODEVICE_NAME	"stream"

#define FLEXBUS_CIF_SINGLE_STREAM	1
#define FLEXBUS_CIF_STREAM_CIF	0
#define CIF_CIF_VDEV_NAME CIF_VIDEODEVICE_NAME		"_cif"

#define FLEXBUS_CIF_PLANE_Y		0
#define FLEXBUS_CIF_PLANE_CBCR	1

/*
 * RK1808 support 5 channel inputs simultaneously:
 * cif + 4 mipi virtual channels;
 * RV1126/RK356X support 4 channels of BT.656/BT.1120/MIPI
 */
#define FLEXBUS_CIF_MULTI_STREAMS_NUM	5
#define FLEXBUS_CIF_MAX_STREAM_CIF	4

#define FLEXBUS_CIF_MAX_SENSOR	2
#define FLEXBUS_CIF_MAX_PIPELINE	4

#define FLEXBUS_CIF_DEFAULT_WIDTH	640
#define FLEXBUS_CIF_DEFAULT_HEIGHT	480

#define FLEXBUS_CIF_MAX_INTERVAL_NS	5000000

/*
 * for distinguishing cropping from senosr or usr
 */
#define CROP_SRC_SENSOR_MASK		(0x1 << 0)
#define CROP_SRC_USR_MASK		(0x1 << 1)

enum flexbus_cif_yuvaddr_state {
	FLEXBUS_CIF_YUV_ADDR_STATE_UPDATE = 0x0,
	FLEXBUS_CIF_YUV_ADDR_STATE_INIT = 0x1
};

enum flexbus_cif_state {
	FLEXBUS_CIF_STATE_DISABLED,
	FLEXBUS_CIF_STATE_READY,
	FLEXBUS_CIF_STATE_STREAMING,
	FLEXBUS_CIF_STATE_RESET_IN_STREAMING,
};

enum flexbus_cif_clk_edge {
	FLEXBUS_CIF_CLK_RISING = 0x0,
	FLEXBUS_CIF_CLK_FALLING,
};

/*
 * for distinguishing cropping from senosr or usr
 */
enum flexbus_cif_crop_src {
	CROP_SRC_ACT	= 0x0,
	CROP_SRC_SENSOR,
	CROP_SRC_USR,
	CROP_SRC_MAX
};

enum flexbus_cif_chip_id {
	RK_FLEXBUS_CIF_RK3576,
	RK_FLEXBUS_CIF_RK3506,
};

struct flexbus_cif_match_data {
	int chip_id;
};

/*
 * struct flexbus_cif_pipeline - An CIF hardware pipeline
 *
 * Capture device call other devices via pipeline
 *
 * @num_subdevs: number of linked subdevs
 * @power_cnt: pipeline power count
 * @stream_cnt: stream power count
 */
struct flexbus_cif_pipeline {
	struct media_pipeline pipe;
	int num_subdevs;
	atomic_t power_cnt;
	atomic_t stream_cnt;
	struct v4l2_subdev *subdevs[FLEXBUS_CIF_MAX_PIPELINE];
	int (*open)(struct flexbus_cif_pipeline *p,
		    struct media_entity *me, bool prepare);
	int (*close)(struct flexbus_cif_pipeline *p);
	int (*set_stream)(struct flexbus_cif_pipeline *p, bool on);
};

struct flexbus_cif_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head queue;
	union {
		u32 buff_addr[VIDEO_MAX_PLANES];
		void *vaddr[VIDEO_MAX_PLANES];
	};
	struct dma_buf *dbuf;
	u64 fe_timestamp;
};

struct flexbus_cif_dummy_buffer {
	struct vb2_buffer vb;
	struct vb2_queue vb2_queue;
	struct list_head list;
	struct dma_buf *dbuf;
	dma_addr_t dma_addr;
	struct page **pages;
	void *mem_priv;
	void *vaddr;
	u32 size;
	int dma_fd;
	bool is_need_vaddr;
	bool is_need_dbuf;
	bool is_need_dmafd;
	bool is_free;
};


extern int flexbus_cif_debug;

/*
 * struct flexbus_cif_sensor_info - Sensor infomations
 * @sd: v4l2 subdev of sensor
 * @mbus: media bus configuration
 * @fi: v4l2 subdev frame interval
 * @lanes: lane num of sensor
 * @raw_rect: raw output rectangle of sensor, not crop or selection
 * @selection: selection info of sensor
 */
struct flexbus_cif_sensor_info {
	struct v4l2_subdev *sd;
	struct v4l2_mbus_config mbus;
	struct v4l2_subdev_frame_interval fi;
	int lanes;
	struct v4l2_rect raw_rect;
	struct v4l2_subdev_selection selection;
	int dsi_input_en;
};

enum cif_fmt_type {
	CIF_FMT_TYPE_YUV = 0,
	CIF_FMT_TYPE_RAW,
};

/*
 * struct cif_output_fmt - The output format
 *
 * @bpp: bits per pixel for each cplanes
 * @fourcc: pixel format in fourcc
 * @fmt_val: the fmt val corresponding to CIF_FOR register
 * @csi_fmt_val: the fmt val corresponding to CIF_CSI_ID_CTRL
 * @cplanes: number of colour planes
 * @mplanes: number of planes for format
 * @raw_bpp: bits per pixel for raw format
 * @fmt_type: image format, raw or yuv
 */
struct cif_output_fmt {
	u8 bpp[VIDEO_MAX_PLANES];
	u32 fourcc;
	u32 fmt_val;
	u32 cif_yuv_order;
	u8 cplanes;
	u8 mplanes;
	u8 raw_bpp;
};

/*
 * struct cif_input_fmt - The input mbus format from sensor
 *
 * @mbus_code: mbus format
 * @cif_fmt_val: the fmt val corresponding to CIF_FOR register
 * @csi_fmt_val: the fmt val corresponding to CIF_CSI_ID_CTRL
 * @fmt_type: image format, raw or yuv
 * @field: the field type of the input from sensor
 */
struct cif_input_fmt {
	u32 mbus_code;
	u32 cif_yuv_order;
	enum v4l2_field field;
};

struct flexbus_cif_vdev_node {
	struct vb2_queue buf_queue;
	/* vfd lock */
	struct mutex vlock;
	struct video_device vdev;
	struct media_pad pad;
};

/*
 * the mark that csi frame0/1 interrupts enabled
 * in CIF_MIPI_INTEN
 */
enum cif_frame_ready {
	CIF_CSI_FRAME0_READY = 0x1,
	CIF_CSI_FRAME1_READY
};

/* struct flexbus_cif_hdr - hdr configured
 * @op_mode: hdr optional mode
 */
struct flexbus_cif_hdr {
	u8 mode;
};

/* struct flexbus_cif_fps_stats - take notes on timestamp of buf
 * @frm0_timestamp: timesstamp of buf in frm0
 * @frm1_timestamp: timesstamp of buf in frm1
 */
struct flexbus_cif_fps_stats {
	u64 frm0_timestamp;
	u64 frm1_timestamp;
};

/* struct flexbus_cif_fps_stats - take notes on timestamp of buf
 * @fs_timestamp: timesstamp of frame start
 * @fe_timestamp: timesstamp of frame end
 * @wk_timestamp: timesstamp of buf send to user in wake up mode
 * @readout_time: one frame of readout time
 * @early_time: early time of buf send to user
 * @total_time: totaltime of readout time in hdr
 */
struct flexbus_cif_readout_stats {
	u64 fs_timestamp;
	u64 fe_timestamp;
	u64 wk_timestamp;
	u64 readout_time;
	u64 early_time;
	u64 total_time;
};

/*
 * struct flexbus_cif_stream - Stream states TODO
 *
 * @vbq_lock: lock to protect buf_queue
 * @buf_queue: queued buffer list
 * @dummy_buf: dummy space to store dropped data
 * @crop_enable: crop status when stream off
 * @crop_dyn_en: crop status when streaming
 * flexbus_cif use shadowsock registers, so it need two buffer at a time
 * @curr_buf: the buffer used for current frame
 * @next_buf: the buffer used for next frame
 * @fps_lock: to protect parameters about calculating fps
 */
struct flexbus_cif_stream {
	unsigned id:3;
	struct flexbus_cif_device		*cif_dev;
	struct flexbus_cif_vdev_node		vnode;
	enum flexbus_cif_state		state;
	wait_queue_head_t		wq_stopped;
	unsigned int			frame_idx;
	u32				frame_phase;
	u32				frame_phase_cache;
	unsigned int			crop_mask;
	/* lock between irq and buf_queue */
	struct list_head		buf_head;
	struct flexbus_cif_buffer		*curr_buf;
	struct flexbus_cif_buffer		*next_buf;

	spinlock_t vbq_lock; /* vfd lock */
	spinlock_t fps_lock;
	/* TODO: pad for cif and mipi separately? */
	struct media_pad		pad;

	const struct cif_output_fmt	*cif_fmt_out;
	const struct cif_input_fmt	*cif_fmt_in;
	struct v4l2_pix_format_mplane	pixm;
	struct v4l2_rect		crop[CROP_SRC_MAX];
	struct flexbus_cif_fps_stats		fps_stats;
	struct flexbus_cif_readout_stats	readout;
	unsigned int			capture_mode;
	int				dma_en;
	int				to_en_dma;
	int				to_stop_dma;
	int				buf_owner;
	int				buf_replace_cnt;
	struct list_head		rx_buf_head_vicap;
	unsigned int			cur_stream_mode;
	int				lack_buf_cnt;
	unsigned int			buf_wake_up_cnt;
	struct tasklet_struct		vb_done_tasklet;
	struct list_head		vb_done_list;
	int				last_rx_buf_idx;
	int				last_frame_idx;
	int				new_fource_idx;
	atomic_t			buf_cnt;
	bool				stopping;
	bool				crop_enable;
	bool				crop_dyn_en;
	bool				is_compact;
	bool				is_buf_active;
	bool				is_high_align;
	bool				to_en_scale;
	bool				is_finish_stop_dma;
	bool				is_in_vblank;
	bool				is_change_toisp;
	bool				is_stop_capture;
};

struct flexbus_cif_cif_sof_subdev {
	struct flexbus_cif_device *cif_dev;
	struct v4l2_subdev sd;
	atomic_t			frm_sync_seq;
};

static inline struct flexbus_cif_buffer *to_flexbus_cif_buffer(struct vb2_v4l2_buffer *vb)
{
	return container_of(vb, struct flexbus_cif_buffer, vb);
}

static inline
struct flexbus_cif_vdev_node *vdev_to_node(struct video_device *vdev)
{
	return container_of(vdev, struct flexbus_cif_vdev_node, vdev);
}

static inline
struct flexbus_cif_stream *to_flexbus_cif_stream(struct flexbus_cif_vdev_node *vnode)
{
	return container_of(vnode, struct flexbus_cif_stream, vnode);
}

static inline struct flexbus_cif_vdev_node *queue_to_node(struct vb2_queue *q)
{
	return container_of(q, struct flexbus_cif_vdev_node, buf_queue);
}

static inline struct vb2_queue *to_vb2_queue(struct file *file)
{
	struct flexbus_cif_vdev_node *vnode = video_drvdata(file);

	return &vnode->buf_queue;
}

enum flexbus_cif_err_state {
	FLEXBUS_CIF_ERR_ID0_NOT_BUF = 0x1,
	FLEXBUS_CIF_ERR_ID1_NOT_BUF = 0x2,
	FLEXBUS_CIF_ERR_ID2_NOT_BUF = 0x4,
	FLEXBUS_CIF_ERR_ID3_NOT_BUF = 0x8,
	FLEXBUS_CIF_ERR_ID0_TRIG_SIMULT = 0x10,
	FLEXBUS_CIF_ERR_ID1_TRIG_SIMULT = 0x20,
	FLEXBUS_CIF_ERR_ID2_TRIG_SIMULT = 0x40,
	FLEXBUS_CIF_ERR_ID3_TRIG_SIMULT = 0x80,
	FLEXBUS_CIF_ERR_SIZE = 0x100,
	FLEXBUS_CIF_ERR_OVERFLOW = 0x200,
	FLEXBUS_CIF_ERR_BANDWIDTH_LACK = 0x400,
	FLEXBUS_CIF_ERR_BUS = 0X800,
	FLEXBUS_CIF_ERR_ID0_MULTI_FS = 0x1000,
	FLEXBUS_CIF_ERR_ID1_MULTI_FS = 0x2000,
	FLEXBUS_CIF_ERR_ID2_MULTI_FS = 0x4000,
	FLEXBUS_CIF_ERR_ID3_MULTI_FS = 0x8000,
	FLEXBUS_CIF_ERR_PIXEL = 0x10000,
	FLEXBUS_CIF_ERR_LINE = 0x20000,
};

struct flexbus_cif_err_state_work {
	struct work_struct	work;
	u64 last_timestamp;
	u32 err_state;
	u32 intstat;
	u32 lastline;
	u32 lastpixel;
	u32 fifo_dnum;
};

struct flexbus_cif_irq_stats {
	u64 cif_bus_err_cnt;
	u64 cif_overflow_cnt;
	u64 cif_line_err_cnt;
	u64 cif_pix_err_cnt;
	u64 cif_size_err_cnt;
	u64 cif_bwidth_lack_cnt;
	u64 frm_end_cnt[4];
	u64 not_active_buf_cnt[4];
	u64 trig_simult_cnt[4];
	u64 all_err_cnt;
};

/*
 * struct flexbus_cif_device - ISP platform device
 * @base_addr: base register address
 * @active_sensor: sensor in-use, set when streaming on
 * @stream: capture video device
 */
struct flexbus_cif_device {
	struct list_head		list;
	struct device			*dev;
	struct rockchip_flexbus		*fb_dev;
	struct v4l2_device		v4l2_dev;
	struct media_device		media_dev;
	struct v4l2_async_notifier	notifier;
	int				chip_id;

	struct flexbus_cif_sensor_info	sensors[FLEXBUS_CIF_MAX_SENSOR];
	u32				num_sensors;
	struct flexbus_cif_sensor_info	*active_sensor;
	struct flexbus_cif_sensor_info	terminal_sensor;

	struct flexbus_cif_stream		stream[FLEXBUS_CIF_MULTI_STREAMS_NUM];
	struct flexbus_cif_pipeline		pipe;

	atomic_t			stream_cnt;
	atomic_t			power_cnt;
	struct mutex			stream_lock; /* lock between streams */
	struct flexbus_cif_cif_sof_subdev	cif_sof_subdev;
	struct flexbus_cif_dummy_buffer dummy_buf;
	struct flexbus_cif_irq_stats irq_stats;

	u32				err_state;
	struct flexbus_cif_err_state_work err_state_work;
	struct proc_dir_entry		*proc_dir;
	int				id_use_cnt;
	bool				is_use_dummybuf;
	bool				is_dma_sg_ops;
};

//extern struct platform_driver flexbus_cif_plat_drv;

void flexbus_cif_write_register(struct flexbus_cif_device *dev,
			  u32 offset, u32 val);
void flexbus_cif_write_register_or(struct flexbus_cif_device *dev,
			     u32 offset, u32 val);
void flexbus_cif_write_register_and(struct flexbus_cif_device *dev,
			      u32 offset, u32 val);
unsigned int flexbus_cif_read_register(struct flexbus_cif_device *dev,
				 u32 offset);
void flexbus_cif_write_grf_register(struct flexbus_cif_device *dev,
			  u32 offset, u32 val);

void flexbus_cif_unregister_stream_vdevs(struct flexbus_cif_device *dev,
				   int stream_num);
int flexbus_cif_register_stream_vdevs(struct flexbus_cif_device *dev,
				int stream_num,
				bool is_multi_input);
void flexbus_cif_stream_init(struct flexbus_cif_device *dev, u32 id);
void flexbus_cif_set_default_fmt(struct flexbus_cif_device *cif_dev);
void flexbus_cif_irq_pingpong(struct flexbus_cif_device *cif_dev, u32 isr);
int flexbus_cif_register_cif_sof_subdev(struct flexbus_cif_device *dev);
void flexbus_cif_unregister_cif_sof_subdev(struct flexbus_cif_device *dev);

int flexbus_cif_clr_unready_dev(void);

void flexbus_cif_err_print_work(struct work_struct *work);

#endif
