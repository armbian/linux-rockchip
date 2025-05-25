/* SPDX-License-Identifier: (GPL-2.0+ WITH Linux-syscall-note) OR MIT
 *
 * Rockchip FEC
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_FEC_CONFIG_H
#define _UAPI_RK_FEC_CONFIG_H

#include <linux/types.h>
#include <linux/v4l2-controls.h>

#define RKFEC_API_VERSION		KERNEL_VERSION(0, 1, 0)

#define FEC_BUF_CNT		3

/*************VIDIOC_PRIVATE*************/
#define RKFEC_CMD_IN_OUT \
	_IOW('V', BASE_VIDIOC_PRIVATE + 10, struct rkfec_in_out)

#define RKFEC_CMD_BUF_ADD \
	_IOW('V', BASE_VIDIOC_PRIVATE + 1, int)

#define RKFEC_CMD_BUF_DEL \
	_IOW('V', BASE_VIDIOC_PRIVATE + 2, int)

#define RKFEC_CMD_BUF_ALLOC \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct rkfec_buf)

/* rkfec_buf_info
 * @in_offs: in_buf c addr offset
 * @out_offs: out_buf c addr offset
 */
struct rkfec_buf_cfg {
	int in_pic_fd;
	int out_pic_fd;
	int lut_fd;
	int in_stride;
	int out_stride;
	int in_size;
	int out_size;
	int lut_size;
	int in_offs;
	int out_offs;
} __attribute__ ((packed));

/* rkfec_core_ctrl
 * @ bic_mode: 0:precise 1:spline 2:catrom 3: mitchell
 * @ border_mode: 0:fill with bg_value 1:copy with the nearest pixel
 * @ buf_mode: 0:fill with bg_value 1:copy with the nearest pixel
 * @ pbuf_crs_dis
 * @ density: 0:16x8; 1:32x16; 2:4x4
 */
struct rkfec_core_ctrl {
	int bic_mode;
	int density;
	int border_mode;
	int pbuf_crs_dis;
	int buf_mode;
} __attribute__ ((packed));

struct rkfec_bg_val {
	int bg_y;
	int bg_u;
	int bg_v;
} __attribute__ ((packed));

struct rkfec_in_out {
	int in_width;
	int in_height;
	int out_width;
	int out_height;
	int in_fourcc;
	int out_fourcc;

	struct rkfec_buf_cfg buf_cfg;
	struct rkfec_core_ctrl core_ctrl;
	struct rkfec_bg_val bg_val;
} __attribute__ ((packed));

struct rkfec_buf {
	int size;
	int buf_fd;
} __attribute__ ((packed));

#endif
