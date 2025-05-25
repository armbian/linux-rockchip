/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKFEC_FEC_OFFLINE_H
#define _RKFEC_FEC_OFFLINE_H

#include <linux/clk.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>
#include <linux/pm_runtime.h>
#include <uapi/linux/rk-fec-config.h>
#include <uapi/linux/rk-video-format.h>


extern int rkfec_debug;

/**
 * enum rkfec_memory - type of memory model used to make the buffers visible
 *	on userspace.
 *
 * @RKFEC_MEMORY_UNKNOWN:  Buffer status is unknown or it is not used yet on
 *			   userspace.
 * @RKFEC_MEMORY_MMAP:	   The buffers are allocated by the Kernel and it is
 *			   memory mapped via mmap() ioctl.
 * @RKFEC_MEMORY_DMABUF:   The buffers are passed to userspace via DMA buffer.
 */
enum rkfec_memory {
	RKFEC_MEMORY_UNKNOWN = 0,
	RKFEC_MEMORY_MMAP = 1,
	RKFEC_MEMORY_DMABUF = 2,
};

/*
 *	V I D E O   I M A G E   F O R M A T
 */
struct rkfec_pix_format {
	u32 width;
	u32 height;
	u32 pixelformat;
	u32 offset;
	u32 bytesperline;
	u32 sizeimage;
};

enum rkfec_state {
	RKFEC_FRAME_END = BIT(1),

	RKFEC_STOP = BIT(16),
	RKFEC_START = BIT(17),
	RKFEC_ERROR = BIT(18),
};

struct rkfec_frame_info {
	u32 fs_seq;
	u32 fe_seq;
	u64 fs_timestamp;
	u64 fe_timestamp;
};
struct rkfec_debug_info {
	u32 interval;
	u32 frameloss;
	u32 frame_timeout_cnt;
};

struct rkfec_offline_dev {
	struct rkfec_hw_dev *hw;
	struct v4l2_device v4l2_dev;
	struct video_device vfd;
	struct mutex ioctl_lock;
	struct completion cmpl;
	struct completion pm_cmpl;
	struct list_head list;
	bool pm_need_wait;
	struct vb2_queue vb2_queue;
	struct proc_dir_entry *procfs;
	unsigned int isr_cnt;
	unsigned int err_cnt;
	unsigned int state;
	unsigned int in_seq;
	unsigned int out_seq;
	struct rkfec_frame_info prev_frame;
	struct rkfec_frame_info curr_frame;
	struct rkfec_debug_info debug;
	struct rkfec_pix_format in_fmt;
	struct rkfec_pix_format out_fmt;
};

/*
 * rkfec_offline_buf
 * @memory:  current memory type used
 */
struct rkfec_offline_buf {
	struct list_head list;
	struct vb2_buffer vb;
	struct file *file;
	struct dma_buf *dbuf;
	void *mem;
	int fd;
	unsigned int memory;
};

int rkfec_register_offline(struct rkfec_hw_dev *hw);
void rkfec_unregister_offline(struct rkfec_hw_dev *hw);
void rkfec_offline_irq(struct rkfec_hw_dev *hw, u32 irq);

#endif
