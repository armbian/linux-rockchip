/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_ISP_PARAM_V33_H
#define _RKISP_ISP_PARAM_V33_H

#include "common.h"
#include "isp_params.h"

#define ISP33_RAWHISTBIG_ROW_NUM		15
#define ISP33_RAWHISTBIG_COLUMN_NUM		15
#define ISP33_RAWHISTBIG_WEIGHT_REG_SIZE	\
	(ISP33_RAWHISTBIG_ROW_NUM * ISP33_RAWHISTBIG_COLUMN_NUM)

struct rkisp_isp_params_vdev;
struct rkisp_isp_params_ops_v33 {
	void (*dpcc_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp39_dpcc_cfg *arg, u32 id);
	void (*dpcc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*bls_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp32_bls_cfg *arg, u32 id);
	void (*bls_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*lsc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp3x_lsc_cfg *arg, u32 id);
	void (*lsc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*awbgain_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp32_awb_gain_cfg *arg, u32 id);
	void (*awbgain_enable)(struct rkisp_isp_params_vdev *params_vdev,
			       bool en, u32 id);
	void (*debayer_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp33_debayer_cfg *arg, u32 id);
	void (*debayer_enable)(struct rkisp_isp_params_vdev *params_vdev,
			       bool en, u32 id);
	void (*ccm_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp33_ccm_cfg *arg, u32 id);
	void (*ccm_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*goc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp3x_gammaout_cfg *arg, u32 id);
	void (*goc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*cproc_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp2x_cproc_cfg *arg, u32 id);
	void (*cproc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*ie_enable)(struct rkisp_isp_params_vdev *params_vdev,
			  bool en, u32 id);
	void (*rawae0_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp2x_rawaebig_meas_cfg *arg, u32 id);
	void (*rawae0_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*rawae3_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp2x_rawaebig_meas_cfg *arg, u32 id);
	void (*rawae3_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*rawawb_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp33_rawawb_meas_cfg *arg, u32 id);
	void (*rawawb_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*rawhst0_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp2x_rawhistbig_cfg *arg, u32 id);
	void (*rawhst0_enable)(struct rkisp_isp_params_vdev *params_vdev,
			       bool en, u32 id);
	void (*rawhst3_config)(struct rkisp_isp_params_vdev *params_vdev,
			       const struct isp2x_rawhistbig_cfg *arg, u32 id);
	void (*rawhst3_enable)(struct rkisp_isp_params_vdev *params_vdev,
			       bool en, u32 id);
	void (*hdrdrc_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp33_drc_cfg *arg,
			      enum rkisp_params_type type, u32 id);
	void (*hdrdrc_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*hdrmge_config)(struct rkisp_isp_params_vdev *params_vdev,
			      const struct isp32_hdrmge_cfg *arg,
			      enum rkisp_params_type type, u32 id);
	void (*hdrmge_enable)(struct rkisp_isp_params_vdev *params_vdev,
			      bool en, u32 id);
	void (*gic_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp33_gic_cfg *arg, u32 id);
	void (*gic_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*enh_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp33_enh_cfg *arg, u32 id);
	void (*enh_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*hist_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp33_hist_cfg *arg, u32 id);
	void (*hist_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*hsv_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp33_hsv_cfg *arg, u32 id);
	void (*hsv_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*ldch_config)(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp32_ldch_cfg *arg, u32 id);
	void (*ldch_enable)(struct rkisp_isp_params_vdev *params_vdev,
			    bool en, u32 id);
	void (*ynr_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp33_ynr_cfg *arg, u32 id);
	void (*ynr_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*cnr_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp33_cnr_cfg *arg, u32 id);
	void (*cnr_enable)(struct rkisp_isp_params_vdev *params_vdev,
			   bool en, u32 id);
	void (*sharp_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp33_sharp_cfg *arg, u32 id);
	void (*sharp_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*bay3d_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp33_bay3d_cfg *arg, u32 id);
	void (*bay3d_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*gain_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp3x_gain_cfg *arg, u32 id);
	void (*gain_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*cac_config)(struct rkisp_isp_params_vdev *params_vdev,
			     const struct isp33_cac_cfg *arg, u32 id);
	void (*cac_enable)(struct rkisp_isp_params_vdev *params_vdev,
			     bool en, u32 id);
	void (*csm_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp21_csm_cfg *arg, u32 id);
	void (*cgc_config)(struct rkisp_isp_params_vdev *params_vdev,
			   const struct isp21_cgc_cfg *arg, u32 id);
};

struct rkisp_isp_params_val_v33 {
	struct rkisp_dummy_buffer buf_ldch[ISP_UNITE_MAX][ISP3X_MESH_BUF_NUM];
	u32 buf_ldch_idx[ISP_UNITE_MAX];

	struct rkisp_dummy_buffer buf_info[RKISP_INFO2DDR_BUF_MAX];
	u32 buf_info_owner;
	u32 buf_info_cnt;
	int buf_info_idx;

	struct rkisp_dummy_buffer buf_3dnr_wgt;
	struct rkisp_dummy_buffer buf_3dnr_iir;
	struct rkisp_dummy_buffer buf_3dnr_ds;
	struct rkisp_dummy_buffer buf_gain;
	u32 bay3d_ds_size;
	u32 bay3d_iir_size;
	u32 bay3d_wgt_size;
	u32 gain_size;

	u32 hist_blk_num;
	u32 enh_row;
	u32 enh_col;
};

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V33)
int rkisp_init_params_vdev_v33(struct rkisp_isp_params_vdev *params_vdev);
void rkisp_uninit_params_vdev_v33(struct rkisp_isp_params_vdev *params_vdev);
#else
static inline int rkisp_init_params_vdev_v33(struct rkisp_isp_params_vdev *params_vdev) { return -EINVAL; }
static inline void rkisp_uninit_params_vdev_v33(struct rkisp_isp_params_vdev *params_vdev) {}
#endif
#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V33_DBG)
int rkisp_get_params_v33(struct rkisp_isp_params_vdev *params_vdev, void *arg);
#else
static inline int rkisp_get_params_v33(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	pr_err("enable CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V33_DBG in kernel config\n");
	return -EINVAL;
}
#endif

#endif /* _RKISP_ISP_PARAM_V33_H */
