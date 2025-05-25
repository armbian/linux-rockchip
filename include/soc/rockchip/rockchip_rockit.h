/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */
#ifndef __SOC_ROCKCHIP_ROCKIT_H
#define __SOC_ROCKCHIP_ROCKIT_H

#include <linux/dma-buf.h>
#include <linux/rk-isp2-config.h>

#define ROCKIT_BUF_NUM_MAX	20
#define ROCKIT_ISP_NUM_MAX	3
#define ROCKIT_STREAM_NUM_MAX	12

#define ROCKIT_VICAP_NUM_MAX	6
#define ROCKIT_VPSS_NUM_MAX 3

enum {
	RKISP_NORMAL_ONLINE,
	RKISP_NORMAL_OFFLINE,
	RKISP_FAST_ONLINE,
	RKISP_FAST_OFFLINE,
};

enum function_cmd {
	ROCKIT_BUF_QUE,
	ROCKIT_MPIBUF_DONE
};

struct rkisp_stream_cfg {
	struct rkisp_rockit_buffer *rkisp_buff[ROCKIT_BUF_NUM_MAX];
	int buff_id[ROCKIT_BUF_NUM_MAX];
	void *node;
	int fps_cnt;
	int dst_fps;
	int cur_fps;
	u64 old_time;
	bool is_discard;
	struct mutex freebuf_lock;
};

struct ISP_VIDEO_FRAMES {
	u32	pMbBlk;
	u32	u32Width;
	u32	u32Height;
	u32	u32VirWidth;
	u32	u32VirHeight;
	u32	enField;
	u32	enPixelFormat;
	u32	enVideoFormat;
	u32	enCompressMode;
	u32	enDynamicRange;
	u32	enColorGamut;
	u32	u32TimeRef;
	u64	u64PTS;

	u64	u64PrivateData;
	u32	u32FrameFlag;     /* FRAME_FLAG_E, can be OR operation. */
	u8	ispEncCnt;

	u32	hdr;
	u32	rolling_shutter_skew;
	/* linear or hdr short frame */
	u32	sensor_exposure_time;
	u32	sensor_analog_gain;
	u32	sensor_digital_gain;
	u32	isp_digital_gain;
	/* hdr mid-frame */
	u32	sensor_exposure_time_m;
	u32	sensor_analog_gain_m;
	u32	sensor_digital_gain_m;
	u32	isp_digital_gain_m;
	/* hdr long frame */
	u32	sensor_exposure_time_l;
	u32	sensor_analog_gain_l;
	u32	sensor_digital_gain_l;
	u32	isp_digital_gain_l;
};

struct rkisp_dev_cfg {
	char *isp_name;
	void *isp_dev;
	struct rkisp_stream_cfg rkisp_stream_cfg[ROCKIT_STREAM_NUM_MAX];
};

struct rockit_cfg {
	bool is_alloc;
	bool is_empty;
	bool is_qbuf;
	bool is_color;
	char *current_name;
	dma_addr_t dma_addr;
	int *buff_id;
	int mpi_id;
	int isp_num;
	u32 nick_id;
	u32 event;
	u32 y_offset;
	u32 u_offset;
	u32 v_offset;
	u32 vir_width;
	void *node;
	void *mpibuf;
	void *vvi_dev[ROCKIT_ISP_NUM_MAX];
	struct dma_buf *buf;
	struct ISP_VIDEO_FRAMES frame;
	struct rkisp_dev_cfg rkisp_dev_cfg[ROCKIT_ISP_NUM_MAX];
	int (*rkisp_rockit_mpibuf_done)(struct rockit_cfg *rockit_isp_cfg);
};

struct rkcif_stream_cfg {
	struct rkcif_rockit_buffer *rkcif_buff[ROCKIT_BUF_NUM_MAX];
	int buff_id[ROCKIT_BUF_NUM_MAX];
	void *node;
	int fps_cnt;
	int dst_fps;
	int cur_fps;
	u64 old_time;
	bool is_discard;
};

struct rkcif_dev_cfg {
	const char *cif_name;
	void *cif_dev;
	struct rkcif_stream_cfg rkcif_stream_cfg[ROCKIT_STREAM_NUM_MAX];
};

struct rockit_rkcif_cfg {
	bool is_alloc;
	bool is_empty;
	bool is_qbuf;
	const char *cur_name;
	int *buff_id;
	int mpi_id;
	u32 nick_id;
	u32 event;
	int cif_num;
	void *node;
	void *mpibuf;
	void *vvi_dev[ROCKIT_VICAP_NUM_MAX];
	struct dma_buf *buf;
	struct ISP_VIDEO_FRAMES frame;
	struct rkcif_dev_cfg rkcif_dev_cfg[ROCKIT_VICAP_NUM_MAX];
	int (*rkcif_rockit_mpibuf_done)(struct rockit_rkcif_cfg *rockit_cif_cfg);
};

struct rkvpss_stream_cfg {
	struct rkvpss_rockit_buffer *rkvpss_buff[ROCKIT_BUF_NUM_MAX];
	int buff_id[ROCKIT_BUF_NUM_MAX];
	void *node;
	int fps_cnt;
	int dst_fps;
	int cur_fps;
	u64 old_time;
	bool is_discard;
	struct mutex freebuf_lock;
};

struct rkvpss_dev_cfg {
	const char *vpss_name;
	void *vpss_dev;
	struct rkvpss_stream_cfg rkvpss_stream_cfg[ROCKIT_STREAM_NUM_MAX];
};

struct rockit_rkvpss_cfg {
	bool is_alloc;
	bool is_empty;
	bool is_qbuf;
	char *current_name;
	int *buff_id;
	int mpi_id;
	u32 nick_id;
	u32 event;
	int vpss_num;
	u32 y_offset;
	u32 uv_offset;
	u32 vir_width;
	void *node;
	void *mpibuf;
	void *vvi_dev[ROCKIT_VPSS_NUM_MAX];
	struct dma_buf *buf;
	struct ISP_VIDEO_FRAMES frame;
	struct rkvpss_dev_cfg rkvpss_dev_cfg[ROCKIT_VPSS_NUM_MAX];
	int (*rkvpss_rockit_mpibuf_done)(struct rockit_rkvpss_cfg *rockit_vpss_cfg);
};

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V32) || \
IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V33) || \
IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35)

void *rkisp_rockit_function_register(void *function, int cmd);
int rkisp_rockit_get_ispdev(char **name);
int rkisp_rockit_get_isp_mode(const char *name);
int rkisp_rockit_buf_queue(struct rockit_cfg *input_rockit_cfg);
int rkisp_rockit_pause_stream(struct rockit_cfg *input_rockit_cfg);
int rkisp_rockit_resume_stream(struct rockit_cfg *input_rockit_cfg);
int rkisp_rockit_config_stream(struct rockit_cfg *input_rockit_cfg,
				int width, int height, int wrap_line);
int rkisp_rockit_get_tb_stream_info(struct rockit_cfg *input_rockit_cfg,
				    struct rkisp_tb_stream_info *info);
int rkisp_rockit_free_tb_stream_buf(struct rockit_cfg *input_rockit_cfg);
int rkisp_rockit_free_stream_buf(struct rockit_cfg *input_rockit_cfg);

void *rkcif_rockit_function_register(void *function, int cmd);
int rkcif_rockit_get_cifdev(char **name);
int rkcif_rockit_buf_queue(struct rockit_rkcif_cfg *input_rockit_cfg);
int rkcif_rockit_config_stream(struct rockit_rkcif_cfg *input_rockit_cfg,
				int width, int height, int v4l2_fmt);
int rkcif_rockit_resume_stream(struct rockit_rkcif_cfg *input_rockit_cfg);
int rkcif_rockit_pause_stream(struct rockit_rkcif_cfg *input_rockit_cfg);

#else

static inline void *rkisp_rockit_function_register(void *function, int cmd) { return NULL; }
static inline int rkisp_rockit_get_ispdev(char **name) { return -EINVAL; }
static inline int rkisp_rockit_get_isp_mode(const char *name) { return -EINVAL; }
static inline int rkisp_rockit_buf_queue(struct rockit_cfg *input_rockit_cfg)
{
	return -EINVAL;
}
static inline int rkisp_rockit_pause_stream(struct rockit_cfg *input_rockit_cfg)
{
	return -EINVAL;
}
static inline int rkisp_rockit_resume_stream(struct rockit_cfg *input_rockit_cfg)
{
	return -EINVAL;
}
static inline int rkisp_rockit_config_stream(struct rockit_cfg *input_rockit_cfg,
					     int width, int height, int wrap_line)
{
	return -EINVAL;
}

static inline int rkisp_rockit_get_tb_stream_info(struct rockit_cfg *input_rockit_cfg,
						  struct rkisp_tb_stream_info *info)
{
	return -EINVAL;
}

static inline int rkisp_rockit_free_tb_stream_buf(struct rockit_cfg *input_rockit_cfg)
{
	return -EINVAL;
}

static inline int rkisp_rockit_free_stream_buf(struct rockit_cfg *input_rockit_cfg)
{
	return -EINVAL;
}

#endif

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_VPSS_V20)
void *rkvpss_rockit_function_register(void *function, int cmd);
int rkvpss_rockit_get_vpssdev(char **name);
int rkvpss_rockit_buf_queue(struct rockit_rkvpss_cfg *input_cfg);
int rkvpss_rockit_pause_stream(struct rockit_rkvpss_cfg *input_cfg);
int rkvpss_rockit_config_stream(struct rockit_rkvpss_cfg *input_cfg,
				int width, int height, int wrap_line);
int rkvpss_rockit_resume_stream(struct rockit_rkvpss_cfg *input_cfg);
int rkvpss_rockit_free_stream_buf(struct rockit_rkvpss_cfg *input_cfg);
#else
static inline void *rkvpss_rockit_function_register(void *function, int cmd)
{
	return NULL;
}
static inline int rkvpss_rockit_get_vpssdev(char **name)
{
	return -EINVAL;
}
static inline int rkvpss_rockit_buf_queue(struct rockit_rkvpss_cfg *input_cfg)
{
	return -EINVAL;
}
static inline int rkvpss_rockit_pause_stream(struct rockit_rkvpss_cfg *input_cfg)
{
	return -EINVAL;
}
static inline int rkvpss_rockit_config_stream(struct rockit_rkvpss_cfg *input_cfg,
					      int width, int height, int wrap_line)
{
	return -EINVAL;
}
static inline int rkvpss_rockit_resume_stream(struct rockit_rkvpss_cfg *input_cfg)
{
	return -EINVAL;
}
static inline int rkvpss_rockit_free_stream_buf(struct rockit_rkvpss_cfg *input_cfg)
{
	return -EINVAL;
}
#endif

#endif
