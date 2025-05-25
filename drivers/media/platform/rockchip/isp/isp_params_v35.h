/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_ISP_PARAM_V35_H
#define _RKISP_ISP_PARAM_V35_H

#include "isp_params.h"

#define ISP35_RAWHISTBIG_ROW_NUM		15
#define ISP35_RAWHISTBIG_COLUMN_NUM		15
#define ISP35_RAWHISTBIG_WEIGHT_REG_SIZE	\
	(ISP35_RAWHISTBIG_ROW_NUM * ISP35_RAWHISTBIG_COLUMN_NUM)

struct rkisp_isp_params_vdev;
struct rkisp_isp_params_val_v35 {
	struct rkisp_dummy_buffer buf_ldch[ISP_UNITE_MAX][ISP3X_MESH_BUF_NUM];
	u32 buf_ldch_idx[ISP_UNITE_MAX];
	struct rkisp_dummy_buffer buf_b3dldc[ISP_UNITE_MAX][ISP3X_MESH_BUF_NUM];
	u32 buf_b3dldc_idx[ISP_UNITE_MAX];
	u32 b3dldc_hsize;
	u32 b3dldch_vsize;
	u32 b3dldcv_vsize;
	struct rkisp_dummy_buffer buf_info[RKISP_INFO2DDR_BUF_MAX];
	u32 buf_info_owner;
	u32 buf_info_cnt;
	int buf_info_idx;

	struct rkisp_dummy_buffer buf_aiawb[RKISP_BUFFER_MAX];
	u32 buf_aiawb_cnt;
	int buf_aiawb_idx;

	struct rkisp_dummy_buffer buf_bay3d_wgt[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_bay3d_iir[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_bay3d_ds[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_gain[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_aipre_gain[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_aiisp[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_vpsl[RKISP_BUFFER_MAX];

	spinlock_t buf_lock;
	struct list_head iir_list;
	struct list_head gain_list;
	struct list_head aipre_gain_list;
	struct list_head vpsl_list;
	struct rkisp_dummy_buffer *pbuf_bay3d_iir;
	struct rkisp_dummy_buffer *pbuf_gain_wr;
	struct rkisp_dummy_buffer *pbuf_gain_rd;
	struct rkisp_dummy_buffer *pbuf_aipre_gain;
	struct rkisp_dummy_buffer *pbuf_vpsl;
	struct rkisp_dummy_buffer *pbuf_aiisp;

	u32 bay3d_iir_rw_fmt;
	u32 bay3d_iir_offs;
	u32 bay3d_iir_stride;
	u32 bay3d_iir_size;
	int bay3d_iir_cnt;
	int bay3d_iir_idx;
	int bay3d_iir_cur_idx;

	u32 bay3d_ds_size;
	int bay3d_ds_cnt;
	int bay3d_ds_idx;
	int bay3d_ds_cur_idx;

	u32 bay3d_wgt_size;
	int bay3d_wgt_cnt;
	int bay3d_wgt_idx;
	int bay3d_wgt_cur_idx;

	int aiisp_cnt;
	int aiisp_cur_idx;

	u32 gain_size;
	int gain_cnt;
	int gain_cur_idx;

	u32 aipre_gain_stride;
	int aipre_gain_cnt;
	int aipre_gain_cur_idx;

	int vpsl_cnt;
	int vpsl_cur_idx;

	u32 vpsl_yraw_offs[VPSL_YRAW_CHN_MAX];
	u32 vpsl_yraw_stride[VPSL_YRAW_CHN_MAX];
	u32 vpsl_sig_offs[VPSL_SIG_CHN_MAX];
	u32 vpsl_sig_stride[VPSL_SIG_CHN_MAX];

	u32 hist_blk_num;
	u32 enh_row;
	u32 enh_col;

	bool yraw_sel;
	bool is_ae0_fe;
	bool is_ae3_fe;
	bool is_af_fe;
	bool is_awb_fe;
	bool is_aiawb_fe;
};

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35)
int rkisp_init_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev);
void rkisp_uninit_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev);
void rkisp_params_vpsl_mi_isr_v35(struct rkisp_isp_params_vdev *params_vdev, u32 mis_val);
#else
static inline int rkisp_init_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev) { return -EINVAL; }
static inline void rkisp_uninit_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev) {}
static inline void rkisp_params_vpsl_mi_isr_v35(struct rkisp_isp_params_vdev *params_vdev, u32 mis_val) {}
#endif
#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35_DBG)
int rkisp_get_params_v35(struct rkisp_isp_params_vdev *params_vdev, void *arg);
#else
static inline int rkisp_get_params_v35(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	pr_err("enable CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35_DBG in kernel config\n");
	return -EINVAL;
}
#endif

#endif /* _RKISP_ISP_PARAM_V35_H */
