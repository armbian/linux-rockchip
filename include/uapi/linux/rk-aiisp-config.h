/* SPDX-License-Identifier: (GPL-2.0+ WITH Linux-syscall-note) OR MIT
 *
 * Rockchip AIISP
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_AIISP_CONFIG_H
#define _UAPI_RK_AIISP_CONFIG_H

#include <linux/rk-isp2-config.h>

#define RKAIISP_PYRAMID_LAYER_NUM		4
#define RKAIISP_MAX_RUNCNT			8
#define RKAIISP_MAX_ISPBUF			8
#define RKAIISP_MODEL_UPDATE			0x01
#define RKAIISP_OTHER_UPDATE			0x02

#define RKAIISP_CMD_SET_PARAM_INFO		\
	_IOW('V', BASE_VIDIOC_PRIVATE + 0, struct rkaiisp_param_info)

#define RKAIISP_CMD_INIT_BUFPOOL		\
	_IOW('V', BASE_VIDIOC_PRIVATE + 1, struct rkaiisp_ispbuf_info)

#define RKAIISP_CMD_FREE_BUFPOOL		\
	_IO('V', BASE_VIDIOC_PRIVATE + 2)

#define RKAIISP_CMD_QUEUE_BUF			\
	_IOW('V', BASE_VIDIOC_PRIVATE + 3, union rkaiisp_queue_buf)

#define RKAIISP_CMD_INIT_AIRMS_BUFPOOL		\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct rkaiisp_rmsbuf_info)

/**********************EVENT_PRIVATE***************************/
#define RKAIISP_V4L2_EVENT_AIISP_DONE		(V4L2_EVENT_PRIVATE_START + 1)

enum rkaiisp_chn_src {
	ISP_IIR,
	VPSL_YRAW_CHN0,
	VPSL_YRAW_CHN1,
	VPSL_YRAW_CHN2,
	VPSL_YRAW_CHN3,
	VPSL_YRAW_CHN4,
	VPSL_YRAW_CHN5,
	VPSL_SIG_CHN0,
	VPSL_SIG_CHN1,
	VPSL_SIG_CHN2,
	VPSL_SIG_CHN3,
	VPSL_SIG_CHN4,
	ISP_AIPRE_NARMAP,
	AIISP_LAST_OUT,
	VICAP_BAYER_RAW,
	ALLZERO_SIGMA,
	ALLZERO_NARMAP
};

enum rkaiisp_exealgo {
	AIBNR,
	AIRMS,
	AIYNR
};

enum rkaiisp_model_mode {
	SINGLE_MODE,
	COMBO_MODE,
	SINGLEX2_MODE,
	REMOSAIC_MODE
};

enum rkaiisp_exemode {
	BOTHEVENT_TO_AIQ,
	ISPEVENT_IN_KERNEL,
	BOTHEVENT_IN_KERNEL
};

struct rkaiisp_airms_st {
	int sequence;
	int inbuf_idx;
	int outbuf_idx;
} __attribute__ ((packed));

union rkaiisp_queue_buf {
	struct rkisp_aiisp_st aibnr_st;
	struct rkaiisp_airms_st airms_st;
} __attribute__ ((packed));

struct rkaiisp_param_info {
	enum rkaiisp_exealgo exealgo;
	enum rkaiisp_exemode exemode;
	__u32 para_size;
	__u32 max_runcnt;
} __attribute__ ((packed));

struct rkaiisp_ispbuf_info {
	struct rkisp_bnr_buf_info bnr_buf;
	__u32 iir_width;
	__u32 iir_height;
	__u32 raw_width[6];
	__u32 raw_height[6];
	__u32 sig_width[5];
	__u32 sig_height[5];
	__u32 narmap_width;
	__u32 narmap_height;
} __attribute__ ((packed));

struct rkaiisp_rmsbuf_info {
	__u32 image_width;
	__u32 image_height;
	__u32 sigma_width;
	__u32 sigma_height;
	__u32 narmap_width;
	__u32 narmap_height;
	__u32 inbuf_num;
	__u32 outbuf_num;
	int inbuf_fd[6];
	int outbuf_fd[6];
} __attribute__ ((packed));

struct rkaiisp_other_cfg {
	__u16 sw_neg_noiselimit;
	__u16 sw_pos_noiselimit;

	__u16 sw_prev_blacklvl;
	__u16 sw_post_blacklvl;

	__u16 sw_in_comp_y[33];
	__u16 sw_out_decomp_y[33];
} __attribute__ ((packed));

struct rkaiisp_model_cfg {
	enum rkaiisp_chn_src mi_chn_src[7];
	__u32 sw_aiisp_mode;
	__u32 sw_aiisp_level_num;
	__u32 sw_aiisp_l1_level_num;
	__u32 sw_aiisp_op_mode;
	__u32 sw_aiisp_drop_en;
	__u32 sw_aiisp_lv_active[16];
	__u32 sw_aiisp_lv_mode[16];
	__u32 sw_mi_chn_en[7];
	__u32 sw_mi_chn_mode[7];
	__u32 sw_mi_chn_num[7];
	__u32 sw_mi_chn_data_mode[7];
	__u32 sw_mi_chn1_sel;
	__u32 sw_mi_chn3_sel;
	__u32 sw_out_d2s_en;
	__u32 sw_out_mode;
	__u32 sw_lastlvlm1_clip8bit;
} __attribute__ ((packed));

struct rkaiisp_kwt_cfg {
	__u32 kwt_offet[RKAIISP_MAX_RUNCNT];
	__u32 kwt_size[RKAIISP_MAX_RUNCNT];
	__u32 kwt_pad_size[RKAIISP_MAX_RUNCNT];
} __attribute__ ((packed));

struct rkaiisp_params {
	__u32 frame_id;
	__u32 module_update;
	__u32 model_runcnt;
	enum rkaiisp_model_mode model_mode;

	struct rkaiisp_other_cfg other_cfg;
	struct rkaiisp_model_cfg model_cfg[RKAIISP_MAX_RUNCNT];
	struct rkaiisp_kwt_cfg kwt_cfg;
	__u8 reserved[36];
} __attribute__ ((packed));

struct rkaiisp_model_info {
	__u32 checksum;
	__u32 model_runcnt;
	float model_qr;
	enum rkaiisp_model_mode model_mode;

	struct rkaiisp_model_cfg model_cfg[RKAIISP_MAX_RUNCNT];
	struct rkaiisp_kwt_cfg kwt_cfg;
	__u8 reserved[48];
} __attribute__ ((packed));

#endif /* _UAPI_RK_AIISP_CONFIG_H */
