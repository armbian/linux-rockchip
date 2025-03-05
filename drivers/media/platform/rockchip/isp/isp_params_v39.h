/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_PARAM_V39_H
#define _RKISP_PARAM_V39_H

#include "common.h"
#include "isp_params.h"

#define ISP39_3DLUT_BUF_NUM			2
#define ISP39_3DLUT_BUF_SIZE			(9 * 9 * 9 * 4)

#define ISP39_LSC_LUT_BUF_NUM			2
#define ISP39_LSC_LUT_TBL_SIZE			(9 * 17 * 4)
#define ISP39_LSC_LUT_BUF_SIZE			(ISP39_LSC_LUT_TBL_SIZE * 4)

#define ISP39_RAWHISTBIG_ROW_NUM		15
#define ISP39_RAWHISTBIG_COLUMN_NUM		15
#define ISP39_RAWHISTBIG_WEIGHT_REG_SIZE	\
	(ISP39_RAWHISTBIG_ROW_NUM * ISP39_RAWHISTBIG_COLUMN_NUM)

struct rkisp_isp_params_vdev;
struct rkisp_isp_params_val_v39 {
	struct rkisp_dummy_buffer buf_3dlut[ISP_UNITE_MAX][ISP39_3DLUT_BUF_NUM];
	u32 buf_3dlut_idx[ISP_UNITE_MAX];

	struct rkisp_dummy_buffer buf_ldch[ISP_UNITE_MAX][ISP39_MESH_BUF_NUM];
	u32 buf_ldch_idx[ISP_UNITE_MAX];
	u32 ldch_out_hsize;

	struct rkisp_dummy_buffer buf_ldcv[ISP_UNITE_MAX][ISP39_MESH_BUF_NUM];
	u32 buf_ldcv_idx[ISP_UNITE_MAX];
	u32 ldcv_out_vsize;

	struct rkisp_dummy_buffer buf_cac[ISP_UNITE_MAX][ISP39_MESH_BUF_NUM];
	u32 buf_cac_idx[ISP_UNITE_MAX];

	struct rkisp_dummy_buffer buf_lsclut[ISP39_LSC_LUT_BUF_NUM];
	u32 buf_lsclut_idx;

	struct rkisp_dummy_buffer buf_info[RKISP_INFO2DDR_BUF_MAX];
	u32 buf_info_owner;
	u32 buf_info_cnt;
	int buf_info_idx;

	u32 gain_size;
	int gain_cnt;
	int gain_cur_idx;
	int aiisp_cnt;
	int aiisp_cur_idx;
	u32 bay3d_iir_size;
	int bay3d_iir_cnt;
	int bay3d_iir_idx;
	int bay3d_iir_cur_idx;
	u32 bay3d_cur_size;
	struct rkisp_dummy_buffer buf_gain[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_aiisp[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_bay3d_iir[RKISP_BUFFER_MAX];
	struct rkisp_dummy_buffer buf_bay3d_cur;

	spinlock_t buf_lock;
	struct list_head iir_list;
	struct list_head gain_list;
	struct rkisp_dummy_buffer *pbuf_bay3d_iir;
	struct rkisp_dummy_buffer *pbuf_gain_wr;
	struct rkisp_dummy_buffer *pbuf_gain_rd;
	struct rkisp_dummy_buffer *pbuf_aiisp;

	struct rkisp_dummy_buffer buf_frm;

	u32 dhaz_blk_num;

	bool is_bigmode;
};

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39)
int rkisp_init_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev);
void rkisp_uninit_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev);
#else
static inline int rkisp_init_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	return -EINVAL;
}
static inline void rkisp_uninit_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev) {}
#endif
#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39_DBG)
int rkisp_get_params_v39(struct rkisp_isp_params_vdev *params_vdev, void *arg);
#else
static inline int rkisp_get_params_v39(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	pr_err("enable CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39_DBG in kernel config\n");
	return -EINVAL;
}
#endif

#endif /* _RKISP_PARAM_V39_H */
