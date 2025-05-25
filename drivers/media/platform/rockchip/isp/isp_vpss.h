/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_VPSS_H
#define _RKISP_VPSS_H

#define RKISP_VPSS_CMD_SOF \
	_IOW('V', BASE_VIDIOC_PRIVATE + 0, struct rkisp_vpss_sof)

#define RKISP_VPSS_CMD_EOF \
	_IOW('V', BASE_VIDIOC_PRIVATE + 1, int)

#define RKISP_VPSS_GET_UNITE_MODE \
	_IOR('V', BASE_VIDIOC_PRIVATE + 2, unsigned int)

#define RKISP_VPSS_RESET_NOTIFY_VPSS \
	_IO('V', BASE_VIDIOC_PRIVATE + 3)

#define RKISP_VPSS_CMD_FRAME_INFO \
	_IOW('V', BASE_VIDIOC_PRIVATE + 4, struct rkisp_vpss_frame_info)

#define RKISP_VPSS_GET_ISP_WORKING \
	_IOR('V', BASE_VIDIOC_PRIVATE + 5, int)

struct rkisp_vpss_sof {
	u32 irq;
	u32 seq;
	u64 timestamp;
	u32 unite_index;
	u32 grey;
};

struct rkisp_vpss_frame_info {
	__u64 timestamp;
	__u32 seq;
	__u32 hdr;
	__u32 rolling_shutter_skew;
	/* linear or hdr short frame */
	__u32 sensor_exposure_time;
	__u32 sensor_analog_gain;
	__u32 sensor_digital_gain;
	__u32 isp_digital_gain;
	/* hdr mid-frame */
	__u32 sensor_exposure_time_m;
	__u32 sensor_analog_gain_m;
	__u32 sensor_digital_gain_m;
	__u32 isp_digital_gain_m;
	/* hdr long frame */
	__u32 sensor_exposure_time_l;
	__u32 sensor_analog_gain_l;
	__u32 sensor_digital_gain_l;
	__u32 isp_digital_gain_l;
} __packed;
#endif
