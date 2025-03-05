/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT)
 *
 * Copyright (C) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RKCIF_CONFIG_H
#define _UAPI_RKCIF_CONFIG_H

#include <linux/types.h>
#include <linux/v4l2-controls.h>

#ifndef VIDEO_MAX_FRAME
#define VIDEO_MAX_FRAME			64
#endif

#define RKCIF_MAX_CSI_NUM		4

#define RKCIF_API_VERSION		KERNEL_VERSION(0, 2, 0)

#define V4L2_EVENT_RESET_DEV		0X1001
#define V4L2_EVENT_EXPOSURE		0X1002

#define RKCIF_CMD_GET_CSI_MEMORY_MODE \
	_IOR('V', BASE_VIDIOC_PRIVATE + 0, int)

#define RKCIF_CMD_SET_CSI_MEMORY_MODE \
	_IOW('V', BASE_VIDIOC_PRIVATE + 1, int)

#define RKCIF_CMD_GET_SCALE_BLC \
	_IOR('V', BASE_VIDIOC_PRIVATE + 2, struct bayer_blc)

#define RKCIF_CMD_SET_SCALE_BLC \
	_IOW('V', BASE_VIDIOC_PRIVATE + 3, struct bayer_blc)

#define RKCIF_CMD_SET_FPS \
	_IOW('V', BASE_VIDIOC_PRIVATE + 4, struct rkcif_fps)

#define RKCIF_CMD_SET_RESET \
	_IOW('V', BASE_VIDIOC_PRIVATE + 6, int)

#define RKCIF_CMD_SET_CSI_IDX \
	_IOW('V', BASE_VIDIOC_PRIVATE + 7, struct rkcif_csi_info)

#define RKCIF_CMD_SET_QUICK_STREAM \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct rkcif_quick_stream_param)

#define RKCIF_CMD_GET_SCL_MODE \
	_IOR('V', BASE_VIDIOC_PRIVATE + 9, unsigned int)

#define RKCIF_CMD_SET_SCL_MODE \
	_IOW('V', BASE_VIDIOC_PRIVATE + 10, unsigned int)

#define RKCIF_CMD_GET_EXTRACTION_PATTERN \
	_IOR('V', BASE_VIDIOC_PRIVATE + 11, unsigned int)

#define RKCIF_CMD_SET_EXTRACTION_PATTERN \
	_IOW('V', BASE_VIDIOC_PRIVATE + 12, unsigned int)

#define RKCIF_CMD_SET_PPI_DATA_DEBUG \
	_IOW('V', BASE_VIDIOC_PRIVATE + 13, unsigned int)

#define RKCIF_CMD_START_CAPTURE_ONE_FRAME_AOV \
	_IOW('V', BASE_VIDIOC_PRIVATE + 14, int)

#define RKCIF_CMD_SET_EXPOSURE \
	_IOW('V', BASE_VIDIOC_PRIVATE + 15, struct rkcif_exp)

#define RKCIF_CMD_GET_EFFECT_EXPOSURE \
	_IOR('V', BASE_VIDIOC_PRIVATE + 16, struct rkcif_effect_exp)

#define RKCIF_CMD_ALLOC_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct rkcif_buffer_info)

#define RKCIF_CMD_FREE_BUF \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 18, struct rkcif_buffer_info)

#define RKCIF_CMD_GET_CONNECT_ID \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 19, int)

#define RKCIF_CMD_SET_SENSOR_FLIP_START \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 20, int)

#define RKCIF_CMD_SET_SENSOR_FLIP_END \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 21, int)

#define RKCIF_CMD_SUPPORT_GET_EXP \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 22, int)

/* cif memory mode
 * 0: raw12/raw10/raw8 8bit memory compact
 * 1: raw12/raw10 16bit memory one pixel
 *    low align for rv1126/rv1109/rk356x
 *    |15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
 *    | -| -| -| -|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
 * 2: raw12/raw10 16bit memory one pixel
 *    high align for rv1126/rv1109/rk356x
 *    |15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
 *    |11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0| -| -| -| -|
 *
 * note: rv1109/rv1126/rk356x dvp only support uncompact mode,
 *       and can be set low align or high align
 */

enum cif_csi_lvds_memory {
	CSI_LVDS_MEM_COMPACT = 0,
	CSI_LVDS_MEM_WORD_LOW_ALIGN = 1,
	CSI_LVDS_MEM_WORD_HIGH_ALIGN = 2,
};

/* black level for scale image
 * The sequence of pattern00~03 is the same as the output of sensor bayer
 */

struct bayer_blc {
	__u8 pattern00;
	__u8 pattern01;
	__u8 pattern02;
	__u8 pattern03;
};

struct rkcif_fps {
	int ch_num;
	int fps;
};

struct rkcif_csi_info {
	int csi_num;
	int csi_idx[RKCIF_MAX_CSI_NUM];
	int dphy_vendor[RKCIF_MAX_CSI_NUM];
};

struct rkcif_quick_stream_param {
	int on;
	__u32 frame_num;
	int resume_mode;
};

enum rkcif_scl_mode {
	RKCIF_SCL_MODE_SCALE,
	RKCIF_SCL_MODE_BINNING,
	RKCIF_SCL_MODE_EXTRACTION,
};

enum rkcif_extraction_pattern {
	RKCIF_EXTRACTION_PATTERN_UP_LEFT,
	RKCIF_EXTRACTION_PATTERN_UP_RIGHT,
	RKCIF_EXTRACTION_PATTERN_BOTTOM_LEFT,
	RKCIF_EXTRACTION_PATTERN_BOTTOM_RIGHT,
};

struct rkcif_exp_delay {
	__u32 time_delay;
	__u32 gain_delay;
};

struct rkcif_exp {
	__u32 time;
	__u32 gain;
};

struct rkcif_effect_exp {
	__u32 sequence;
	__u32 time;
	__u32 gain;
};

struct rkcif_buffer_info {
	__u32 buf_num;
	int dma_fd[VIDEO_MAX_FRAME];
};

#endif
