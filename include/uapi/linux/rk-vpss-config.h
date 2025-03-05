/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT)
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_VPSS_CONFIG_H
#define _UAPI_RK_VPSS_CONFIG_H

#include <linux/types.h>
#include <linux/v4l2-controls.h>

#define VPSS_API_VERSION		KERNEL_VERSION(0, 1, 0)

/* |-------------------------------------------------------------------------------------------|
 * |     mirror_cmsc_en                                                                        |
 * |     |1------------------------>|                                                          |
 * |ISP->|                          |->|crop1->scl->ddr channelX|       isp->vpss online mode  |
 * |     |0---->|                |->|                                       media v4l2 driver  |
 * |------------|->mirror->cmsc->|-------------------------------------------------------------|
 * |     |1---->|                |->|                                                          |
 * |DDR->|                          |->|crop0->scl->aspt->ddr channelY| ddr->vpss offline mode |
 * |     |0------------------------>|                                       independent driver |
 * |     mirror_cmsc_en                                                                        |
 * |-------------------------------------------------------------------------------------------|
 * mirror/cover mux to ISP or DDR
 * channelX or channelY = 0,1,2,3 but X != Y
 * ioctl RKVPSS_CMD_MODULE_SEL to select function using
 */

#define RKVPSS_OUT_V1_MAX 4

/******vpss(online mode) v4l2 ioctl***************************/
/* set before VIDIOC_S_FMT if dynamically changing output resolution */
#define RKVPSS_CMD_SET_STREAM_MAX_SIZE \
	_IOW('V', BASE_VIDIOC_PRIVATE + 0, struct rkvpss_stream_size)
/* for dynamically changing output resolution:
 * SET_STREAM_SUSPEND->VIDIOC_S_FMT/VIDIOC_S_SELECTION->SET_STREAM_RESUME
 */
#define RKVPSS_CMD_SET_STREAM_SUSPEND \
	_IO('V', BASE_VIDIOC_PRIVATE + 1)
#define RKVPSS_CMD_SET_STREAM_RESUME \
	_IO('V', BASE_VIDIOC_PRIVATE + 2)

#define RKVPSS_CMD_GET_MIRROR_FLIP \
	_IOR('V', BASE_VIDIOC_PRIVATE + 3, struct rkvpss_mirror_flip)
#define RKVPSS_CMD_SET_MIRROR_FLIP \
	_IOW('V', BASE_VIDIOC_PRIVATE + 4, struct rkvpss_mirror_flip)

#define RKVPSS_CMD_GET_CMSC \
	_IOR('V', BASE_VIDIOC_PRIVATE + 5, struct rkvpss_cmsc_cfg)
#define RKVPSS_CMD_SET_CMSC \
	_IOW('V', BASE_VIDIOC_PRIVATE + 6, struct rkvpss_cmsc_cfg)

/******vpss(offline mode) independent video ioctl****************/
#define RKVPSS_CMD_MODULE_SEL \
	_IOW('V', BASE_VIDIOC_PRIVATE + 50, struct rkvpss_module_sel)

#define RKVPSS_CMD_FRAME_HANDLE \
	_IOW('V', BASE_VIDIOC_PRIVATE + 51, struct rkvpss_frame_cfg)

/* request vpss to alloc or external dma buf add to vpss */
#define RKVPSS_CMD_BUF_ADD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 52, struct rkvpss_buf_info)
#define RKVPSS_CMD_BUF_DEL \
	_IOW('V', BASE_VIDIOC_PRIVATE + 53, struct rkvpss_buf_info)

#define RKVPSS_CMD_MODULE_GET \
	_IOR('V', BASE_VIDIOC_PRIVATE + 54, struct rkvpss_module_sel)

#define RKVPSS_CMD_CHECKPARAMS \
	_IOW('V', BASE_VIDIOC_PRIVATE + 55, struct rkvpss_frame_cfg)

#define RKVPSS_CMD_STREAM_ATTACH_INFO \
	_IOW('V', BASE_VIDIOC_PRIVATE + 56, int)
/********************************************************************/

/* struct rkvpss_mirror_flip
 * mirror: global for all output stream
 * flip: independent for all output stream
 */
struct rkvpss_mirror_flip {
	unsigned char mirror;
	unsigned char flip;
} __attribute__ ((packed));

/* struct rkvpss_stream_size
 * set max resolution before VIDIOC_S_FMT for init buffer
 */
struct rkvpss_stream_size {
	unsigned int max_width;
	unsigned int max_height;
} __attribute__ ((packed));

#define RKVPSS_CMSC_WIN_MAX 8
#define RKVPSS_CMSC_POINT_MAX 4
#define RKVPSS_CMSC_COVER_MODE 0
#define RKVPSS_CMSC_MOSAIC_MODE 1

struct rkvpss_cmsc_point {
	unsigned int x;
	unsigned int y;
} __attribute__ ((packed));

/* struct rkvpss_cmsc_win
 * Priacy Mask Window configture, support windows
 *
 * win_index: window index 0~8. windows overlap, priority win8 > win0.
 * mode: RKVPSS_CMSC_MOSAIC_MODE:mosaic mode, RKVPSS_CMSC_COVER_MODE:cover mode
 * cover_color_y: cover mode Y value[0, 255].
 * cover_color_u: cover mode U value[0, 255].
 * cover_color_v: cover mode V value[0, 255].
 * cover_color_a: cover mode alpha value[0, 15], 0 is transparent.
 * point: four coordinates of any quadrilateral, the top left of the input image is the origin.
 *        point0 must be the vertex, point0~ponit3 clockwise, and four coordinates should different.
 */
struct rkvpss_cmsc_win {
	unsigned short win_en;

	/* following share for all channel when same win index */
	unsigned short mode;
	unsigned char cover_color_y;
	unsigned char cover_color_u;
	unsigned char cover_color_v;
	unsigned char cover_color_a;
	struct rkvpss_cmsc_point point[RKVPSS_CMSC_POINT_MAX];
} __attribute__ ((packed));

/* struct rkvpss_cmsc_cfg
 * cover and mosaic configure
 * win: priacy mask window
 * mosaic_block: Mosaic block size, 0:8x8 1:16x16 2:32x32 3:64x64, share for all windows
 * width_ro: vpss full resolution.
 * height_ro: vpss full resolution.
 */
struct rkvpss_cmsc_cfg {
	struct rkvpss_cmsc_win win[RKVPSS_CMSC_WIN_MAX];
	unsigned int mosaic_block;
	unsigned int width_ro;
	unsigned int height_ro;
} __attribute__ ((packed));

/* struct rkisp_aspt_cfg                                           _____background____
 * aspective ratio for image background color filling             |offs __image___  c |
 * width: width of background. 2 align                            |    |scl_width | o |
 * height: height of background. 2 align                          |    |scl_height| l |
 * h_offs: horizontal offset of image in the background. 2 align  |    |__________| o |
 * v_offs: vertical offset of image in the background. 2 align    |  color          r |
 * color_y: background y color. 0~255                             |___________________|
 * color_u: background u color. 0~255
 * color_v: background v color. 0~255
 * enable: function enable
 */
struct rkvpss_aspt_cfg {
	unsigned int width;
	unsigned int height;

	unsigned int h_offs;
	unsigned int v_offs;

	unsigned char color_y;
	unsigned char color_u;
	unsigned char color_v;

	unsigned char enable;
} __attribute__ ((packed));

/********************************************************************/

enum {
	RKVPSS_OUTPUT_CH0 = 0,
	RKVPSS_OUTPUT_CH1,
	RKVPSS_OUTPUT_CH2,
	RKVPSS_OUTPUT_CH3,
	RKVPSS_OUTPUT_MAX,
};

/* struct rkvpss_module_sel
 * selection module for vpss offline mode, default select to online mode.
 * mirror_cmsc_en 1:miiror_cmsc sel to offline mode, 0:sel to online mode
 * ch_en 1:channel sel to offline mode, 0:sel to online mode
 */
struct rkvpss_module_sel {
	unsigned char mirror_cmsc_en;
	unsigned char ch_en[RKVPSS_OUTPUT_MAX];
} __attribute__ ((packed));

/* struct rkvpss_input_cfg
 * input configuration of image
 *
 * width: width of input image, range: 32~4672
 * height: height of input image, range: 32~3504
 * stride: virtual width of input image, 16 align. auto calculate according to width and format if 0.
 * format: V4L2_PIX_FMT_NV12/V4L2_PIX_FMT_NV16/V4L2_PIX_FMT_RGB565/V4L2_PIX_FMT_RGB24/V4L2_PIX_FMT_XBGR32/
 *         V4L2_PIX_FMT_NV61/V4L2_PIX_FMT_NV21/V4L2_PIX_FMT_RGB565X/V4L2_PIX_FMT_BGR24/V4L2_PIX_FMT_XRGB32/
 *         V4L2_PIX_FMT_RGBX32/V4L2_PIX_FMT_BGRX32
 *         V4L2_PIX_FMT_FBC0/V4L2_PIX_FMT_FBC2/V4L2_PIX_FMT_FBC4 for rkfbcd
 *         V4L2_PIX_FMT_TILE420/V4L2_PIX_FMT_TILE422 for tile
 * buf_fd: dmabuf fd of input image buf
 * rotate: 0:rotate0 1:rotate90 2:rotate180; 3:rotate270, note:only tile input support rotate
 */
struct rkvpss_input_cfg {
	int width;
	int height;
	int stride;
	int format;
	int buf_fd;
	int rotate;
} __attribute__ ((packed));

/* struct rkvpss_output_cfg                                     __________________
 * output channel configuration of image                       |offs __________   |
 *                                                             |    |  ______  |  |
 * enable: channel enable                                      |    | |      | |  |
 * crop_h_offs: horizontal offset of crop, 2 align             |    | |      | |  |
 * crop_v_offs: vertical offset of crop, 2align                |    | |scl___| |  |
 * crop_width: crop output width, 2align                       |    |crop______|  |
 * crop_height: crop output height, 2align                     |input_____________|
 * scl_width: scale width. CH0 1~8 scale range. CH1/CH2/CH3 1~32 scale range. CH2/CH3 max 1080p with scale.
 * scl_height: scale height. CH0 1~6 scale range. CH1/CH2/CH3 1~32 scale range. CH2/CH3 max 1080p with scale.
 * stride: virtual width of output image, 16 align. auto calculate according to width and format if 0.
 * format: V4L2_PIX_FMT_NV12/V4L2_PIX_FMT_NV16/V4L2_PIX_FMT_GREY/V4L2_PIX_FMT_UYVY/
 *         V4L2_PIX_FMT_VYUY/V4L2_PIX_FMT_NV21/V4L2_PIX_FMT_NV61 for all channel.
 *         NOTE:V,LSB is for all channel
 *         V4L2_PIX_FMT_RGB565/V4L2_PIX_FMT_RGB24/V4L2_PIX_FMT_XBGR32/V4L2_PIX_FMT_RGB565X/V4L2_PIX_FMT_BGR24/
 *         V4L2_PIX_FMT_XRGB32 only for RKVPSS_OUTPUT_CH1.
 *         V4L2_PIX_FMT_TILE420/V4L2_PIX_FMT_TILE422 for tile, ch0 or ch1 support tile
 * flip: flip enable
 * buf_fd: dmabuf fd of output image buf
 * cmsc: cover and mosaic configure
 * aspt: aspective ratio for image background color filling
 */
struct rkvpss_output_cfg {
	int enable;

	int crop_h_offs;
	int crop_v_offs;
	int crop_width;
	int crop_height;

	int scl_width;
	int scl_height;
	int stride;
	int format;
	int flip;
	int buf_fd;

	struct rkvpss_cmsc_cfg cmsc;
	struct rkvpss_aspt_cfg aspt;
} __attribute__ ((packed));

#define RKVPSS_DEV_ID_MAX 128

/* struct rkvpss_frame_cfg
 * frame handle configure
 *
 * dev_id: device id, range 0~127.
 * sequence: frame sequence
 * mirror: mirror enable
 * input: input configuration of image
 * output: output channel configuration of image
 */
struct rkvpss_frame_cfg {
	int dev_id;
	int sequence;

	int mirror;
	struct rkvpss_input_cfg input;
	struct rkvpss_output_cfg output[RKVPSS_OUTPUT_MAX];
} __attribute__ ((packed));

#define RKVPSS_BUF_MAX 32

/* struct rkvpss_buf_info
 * request vpss to alloc or external dma buf add to vpss.
 * dev_id: device id, range 0~127.
 * buf_alloc: request vpss alloc buf or no. 0: no alloc using external buf
 * buf_cnt: buffer count.
 * buf_size: buffer size.
 * buf_fd: dma buffer fd. return if buf_alloc=1, other user set for driver.
 */
struct rkvpss_buf_info {
	int dev_id;
	int buf_alloc;
	int buf_cnt;
	int buf_size[RKVPSS_BUF_MAX];
	int buf_fd[RKVPSS_BUF_MAX];
} __attribute__ ((packed));

struct rkvpss_frame_info {
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
	__u32 isp_reg[6144];
} __attribute__ ((packed));

#endif
