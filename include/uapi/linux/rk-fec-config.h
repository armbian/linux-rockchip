/* SPDX-License-Identifier: (GPL-2.0+ WITH Linux-syscall-note) OR MIT
 *
 * Rockchip FEC
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_FEC_CONFIG_H
#define _UAPI_RK_FEC_CONFIG_H

#include <linux/types.h>
#include <linux/version.h>
#include <linux/v4l2-controls.h>

#define RKFEC_API_VERSION		KERNEL_VERSION(0, 3, 0)

#define FEC_BUF_CNT		3
/* Number of bytes per point in the LUT */
#define BYTES_PER_LUT_POINT	6

/*************VIDIOC_PRIVATE*************/
#define RKFEC_CMD_IN_OUT \
	_IOW('V', BASE_VIDIOC_PRIVATE + 10, struct rkfec_in_out)

#define RKFEC_CMD_BUF_ADD \
	_IOW('V', BASE_VIDIOC_PRIVATE + 1, int)

#define RKFEC_CMD_BUF_DEL \
	_IOW('V', BASE_VIDIOC_PRIVATE + 2, int)

#define RKFEC_CMD_BUF_ALLOC \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct rkfec_buf)

#define RKFEC_CMD_PLANE_CFG \
	_IOW('V', BASE_VIDIOC_PRIVATE + 11, struct rkfec_plane_cfg)

#define RKFEC_CMD_QUERY_VERSION \
	_IOR('V', BASE_VIDIOC_PRIVATE + 12, struct rkfec_version)

#define RKFEC_MAX_PLANES	2

/*
 * rkfec_plane - per-plane byte layout for multi-block buffer processing
 *
 * Mirrors V4L2: bytesperline maps to VIR_STRIDE register,
 * offset (data_offset) maps to BASE register (dma_base + offset).
 * Both fields are in bytes; driver writes them directly to hardware.
 *
 * The mplane path is enabled when any field in planes[] is non-zero.
 * Plane offsets may be zero for valid layouts.
 *
 *   NV12 / TILE420  ([0]=Y/luma, [1]=UV/chroma):
 *     planes[i].bytesperline = stride in bytes (NV12: width, TILE420: width*6)
 *     x/y are the block origin in pixels. x and y should be 2-aligned.
 *
 *     NV12:
 *       planes[0].offset = y * planes[0].bytesperline + x
 *       planes[1].offset = full_H * planes[0].bytesperline
 *                          + (y/2) * planes[1].bytesperline + x
 *
 *     TILE420:
 *       planes[0].offset = y * planes[0].bytesperline + x * 6
 *       planes[1].offset = full_H * planes[0].bytesperline
 *                          + (y/2) * planes[1].bytesperline + x * 6
 *
 *   FBC0  ([0]=header/C, [1]=data/Y):
 *     planes[0].bytesperline = (width+63)/64 * 16    (header stride)
 *     planes[1].bytesperline = (width+63)/64 * 384   (data stride)
 *     x/y are the block origin in pixels. x should be 64-aligned,
 *     and y should be 4-aligned. x_grp = x / 64, y_grp = y / 4.
 *
 *     planes[0].offset = y_grp * planes[0].bytesperline + x_grp * 16
 *     planes[1].offset = (full_H/4) * planes[0].bytesperline
 *                        + y_grp * planes[1].bytesperline + x_grp * 384
 *
 * @bytesperline: stride in bytes for this plane
 * @offset:       byte offset from the DMA base of in_pic_fd / out_pic_fd
 */
struct rkfec_plane {
	__u32 bytesperline;
	__u32 offset;
} __attribute__((packed));

/*
 * rkfec_plane_cfg - per-plane byte layout configured by RKFEC_CMD_PLANE_CFG
 *
 * Set any field in planes[] non-zero to enable the mplane path for subsequent
 * RKFEC_CMD_IN_OUT calls. plane offsets may be zero for valid layouts.
 *
 * NV12/TILE420: in_planes[0]/out_planes[0] = Y luma
 *               in_planes[1]/out_planes[1] = UV chroma
 * FBC0:         in_planes[0]/out_planes[0] = header (C channel)
 *               in_planes[1]/out_planes[1] = data   (Y channel)
 */
struct rkfec_plane_cfg {
	struct rkfec_plane in_planes[RKFEC_MAX_PLANES];
	struct rkfec_plane out_planes[RKFEC_MAX_PLANES];
} __attribute__((packed));

/*
 * rkfec_buf_cfg - buffer descriptor
 *
 * in_stride/out_stride/in_offs/out_offs are in pixels. Per-plane byte layout
 * is configured separately through RKFEC_CMD_PLANE_CFG to keep
 * RKFEC_CMD_IN_OUT ABI compatible with legacy userspace.
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

struct rkfec_version {
	__u32 api_version;
	__u32 in_out_size;
} __attribute__((packed));

struct rkfec_buf {
	int size;
	int buf_fd;
} __attribute__ ((packed));

#endif
