/* SPDX-License-Identifier: (GPL-2.0+ WITH Linux-syscall-note) OR MIT
 *
 * Rockchip ISP33
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_ISP33_CONFIG_H
#define _UAPI_RK_ISP33_CONFIG_H

#include <linux/types.h>
#include <linux/v4l2-controls.h>
#include <linux/rk-isp39-config.h>

#define RKISP_CMD_GET_TB_HEAD_V33 \
	_IOR('V', BASE_VIDIOC_PRIVATE + 19, struct rkisp33_thunderboot_resmem_head)

#define RKISP_CMD_SET_TB_HEAD_V33 \
	_IOW('V', BASE_VIDIOC_PRIVATE + 20, struct rkisp33_thunderboot_resmem_head)

#define RKISP_CMD_GET_PARAMS_V33 \
	_IOR('V', BASE_VIDIOC_PRIVATE + 116, struct isp33_isp_params_cfg)

#define ISP33_MODULE_DPCC		ISP3X_MODULE_DPCC
#define ISP33_MODULE_BLS		ISP3X_MODULE_BLS
#define ISP33_MODULE_LSC		ISP3X_MODULE_LSC
#define ISP33_MODULE_AWB_GAIN		ISP3X_MODULE_AWB_GAIN
#define ISP33_MODULE_CCM		ISP3X_MODULE_CCM
#define ISP33_MODULE_GOC		ISP3X_MODULE_GOC
#define ISP33_MODULE_CPROC		ISP3X_MODULE_CPROC
#define ISP33_MODULE_IE			ISP3X_MODULE_IE
#define ISP33_MODULE_RAWAE0		ISP3X_MODULE_RAWAE0
#define ISP33_MODULE_RAWAE3		ISP3X_MODULE_RAWAE3
#define ISP33_MODULE_RAWAWB		ISP3X_MODULE_RAWAWB
#define ISP33_MODULE_RAWHIST0		ISP3X_MODULE_RAWHIST0
#define ISP33_MODULE_RAWHIST3		ISP3X_MODULE_RAWHIST3
#define ISP33_MODULE_HDRMGE		ISP3X_MODULE_HDRMGE
#define ISP33_MODULE_GIC		ISP3X_MODULE_GIC
#define ISP33_MODULE_LDCH		ISP3X_MODULE_LDCH
#define ISP33_MODULE_GAIN		ISP3X_MODULE_GAIN
#define ISP33_MODULE_DEBAYER		ISP3X_MODULE_DEBAYER
#define ISP33_MODULE_BAY3D		ISP3X_MODULE_BAY3D
#define ISP33_MODULE_YNR		ISP3X_MODULE_YNR
#define ISP33_MODULE_CNR		ISP3X_MODULE_CNR
#define ISP33_MODULE_SHARP		ISP3X_MODULE_SHARP
#define ISP33_MODULE_DRC		ISP3X_MODULE_DRC
#define ISP33_MODULE_CAC		ISP3X_MODULE_CAC
#define ISP33_MODULE_CSM		ISP3X_MODULE_CSM
#define ISP33_MODULE_CGC		ISP3X_MODULE_CGC
#define ISP33_MODULE_HSV		BIT_ULL(48)
#define ISP33_MODULE_ENH		BIT_ULL(49)
#define ISP33_MODULE_HIST		BIT_ULL(50)

#define ISP33_MODULE_FORCE		ISP3X_MODULE_FORCE

/* Measurement types */
#define ISP33_STAT_RAWAWB		ISP3X_STAT_RAWAWB
#define ISP33_STAT_RAWAE0		ISP3X_STAT_RAWAE0
#define ISP33_STAT_RAWAE3		ISP3X_STAT_RAWAE3
#define ISP33_STAT_RAWHST0		ISP3X_STAT_RAWHST0
#define ISP33_STAT_RAWHST3		ISP3X_STAT_RAWHST3
#define ISP33_STAT_INFO2DDR		ISP32_STAT_INFO2DDR
#define ISP33_STAT_BAY3D		BIT(20)
#define ISP33_STAT_ENH			BIT(21)
#define ISP33_STAT_HIST			BIT(22)
#define ISP33_STAT_SHARP		BIT(23)
#define ISP33_STAT_RTT_FST		ISP32_STAT_RTT_FST

#define ISP33_MESH_BUF_NUM		ISP3X_MESH_BUF_NUM

#define ISP33_LSC_GRAD_TBL_SIZE		ISP3X_LSC_GRAD_TBL_SIZE
#define ISP33_LSC_SIZE_TBL_SIZE		ISP3X_LSC_SIZE_TBL_SIZE
#define ISP33_LSC_DATA_TBL_SIZE		ISP3X_LSC_DATA_TBL_SIZE

#define ISP33_DEGAMMA_CURVE_SIZE	ISP3X_DEGAMMA_CURVE_SIZE

#define ISP33_GAIN_IDX_NUM		ISP3X_GAIN_IDX_NUM
#define ISP33_GAIN_LUT_NUM		ISP3X_GAIN_LUT_NUM

#define ISP33_RAWAWB_WEIGHT_NUM		ISP3X_RAWAWB_WEIGHT_NUM
#define ISP33_RAWAWB_HSTBIN_NUM		ISP3X_RAWAWB_HSTBIN_NUM
#define ISP33_RAWAWB_SUM_NUM		4
#define ISP33_RAWAWB_EXCL_STAT_NUM	4

#define	ISP33_RAWAEBIG_SUBWIN_NUM	2

#define ISP33_RAWHISTBIG_SUBWIN_NUM	ISP3X_RAWHISTBIG_SUBWIN_NUM
#define ISP33_RAWHISTLITE_SUBWIN_NUM	ISP3X_RAWHISTLITE_SUBWIN_NUM
#define ISP33_HIST_BIN_N_MAX		ISP3X_HIST_BIN_N_MAX

#define ISP33_DPCC_PDAF_POINT_NUM	ISP3X_DPCC_PDAF_POINT_NUM

#define ISP33_HDRMGE_L_CURVE_NUM	ISP3X_HDRMGE_L_CURVE_NUM
#define ISP33_HDRMGE_E_CURVE_NUM	ISP3X_HDRMGE_E_CURVE_NUM

#define ISP33_GIC_SIGMA_Y_NUM		17
#define ISP33_GIC_LUMA_DX_NUM		7
#define ISP33_GIC_THRED_Y_NUM		8

#define ISP33_CCM_CURVE_NUM		18
#define ISP33_CCM_HF_FACTOR_NUM		17

#define ISP33_HSV_1DLUT_NUM		65
#define ISP33_HSV_2DLUT_ROW		17
#define ISP33_HSV_2DLUT_COL		17

#define ISP33_LDCH_BIC_NUM		ISP32_LDCH_BIC_NUM

#define ISP33_GAMMA_OUT_MAX_SAMPLES     ISP3X_GAMMA_OUT_MAX_SAMPLES

#define ISP33_ENH_LUMA_NUM		17
#define ISP33_ENH_DETAIL_NUM		8
#define ISP33_ENH_IIR_ROW_MAX		18
#define ISP33_ENH_IIR_COL_MAX		24

#define ISP33_HIST_ALPHA_NUM		17
#define ISP33_HIST_THUMB_ROW_MAX	8
#define ISP33_HIST_THUMB_COL_MAX	10
#define ISP33_HIST_IIR_NUM		16
#define ISP33_HIST_IIR_BLK_MAX		(ISP33_HIST_THUMB_ROW_MAX * ISP33_HIST_THUMB_COL_MAX)

#define ISP33_DRC_Y_NUM			ISP3X_DRC_Y_NUM

#define ISP33_CNR_SIGMA_Y_NUM		ISP3X_CNR_SIGMA_Y_NUM
#define ISP33_CNR_GAUS_COE_NUM		6
#define ISP33_CNR_GAUS_SIGMAR_NUM	8
#define ISP33_CNR_WGT_SIGMA_Y_NUM	13

#define ISP33_YNR_XY_NUM		ISP3X_YNR_XY_NUM
#define ISP33_YNR_HI_GAUS_COE_NUM	5
#define ISP33_YNR_HI_GAUS1_COE_NUM	6
#define ISP33_YNR_ADJ_NUM	9

#define ISP33_BAY3D_XY_NUM		16
#define ISP33_BAY3D_TNRSIG_NUM		20
#define ISP33_BAY3D_LPF_COEFF_NUM	9
#define ISP33_BAY3D_FILT_COEFF_NUM	6

#define ISP33_SHARP_X_NUM		7
#define ISP33_SHARP_Y_NUM		8
#define ISP33_SHARP_KERNEL_NUM		6
#define ISP33_SHARP_EDGE_KERNEL_NUM	10
#define ISP33_SHARP_EDGE_WGT_NUM	17
#define ISP33_SHARP_LUMA_STRG_NUM	8
#define ISP33_SHARP_LOCAL_STRG_NUM	4
#define ISP33_SHARP_CONTRAST_STRG_NUM	9
#define ISP33_SHARP_TEX_CLIP_NUM	9
#define ISP33_SHARP_LUM_CLIP_NUM	8
#define ISP33_SHARP_HUE_NUM		9
#define ISP33_SHARP_DISATANCE_NUM	11
#define ISP33_SHARP_HITEX_NUM		9
#define ISP33_SHARP_NOISE_CURVE_NUM	17

#define ISP33_CAC_PSF_NUM		11

#define ISP33_CSM_COEFF_NUM		ISP3X_CSM_COEFF_NUM

#define ISP33_DEBAYER_LUMA_NUM		7
#define ISP33_DEBAYER_DRCT_OFFSET_NUM	8
#define ISP33_DEBAYER_VSIGMA_NUM	8

#define ISP33_MEAN_BLK_X_NUM		15
#define ISP33_MEAN_BLK_Y_NUM		15

#define ISP33_BNR2AEBIG_SEL_EN		0x10
#define ISP33_BNR2AE0_SEL_EN		0x20

struct isp33_hsv_cfg {
	__u8 hsv_1dlut0_en;
	__u8 hsv_1dlut1_en;
	__u8 hsv_2dlut_en;
	__u8 hsv_1dlut0_idx_mode;
	__u8 hsv_1dlut1_idx_mode;
	__u8 hsv_2dlut_idx_mode;
	__u8 hsv_1dlut0_item_mode;
	__u8 hsv_1dlut1_item_mode;
	__u8 hsv_2dlut_item_mode;

	__u16 lut0_1d[ISP33_HSV_1DLUT_NUM];
	__u16 lut1_1d[ISP33_HSV_1DLUT_NUM];
	__u16 lut_2d[ISP33_HSV_2DLUT_ROW][ISP33_HSV_2DLUT_COL];
} __attribute__ ((packed));

struct isp33_gic_cfg {
	/* CTRL */
	__u8 bypass_en;
	__u8 pro_mode;
	__u8 manualnoisecurve_en;
	__u8 manualnoisethred_en;
	__u8 gain_bypass_en;
	/* MEDFLT_PARA */
	__u8 medflt_minthred;
	__u8 medflt_maxthred;
	__u8 medflt_ratio;
	/* MEDFLTUV_PARA */
	__u8 medfltuv_minthred;
	__u8 medfltuv_maxthred;
	__u8 medfltuv_ratio;
	/* NOISE_SCALE */
	__u16 noisecurve_scale;
	/* BILAT_PARA1 */
	__u16 bffltwgt_offset;
	__u8 bffltwgt_scale;
	/* BILAT_PARA2 */
	__u8 bfflt_ratio;
	/* DISWGT_COEFF */
	__u8 bfflt_coeff0;
	__u8 bfflt_coeff1;
	__u8 bfflt_coeff2;
	/* SIGMA_Y */
	__u16 bfflt_vsigma_y[ISP33_GIC_SIGMA_Y_NUM];
	/* LUMA_DX */
	__u8 luma_dx[ISP33_GIC_LUMA_DX_NUM];
	/* THRED_Y */
	__u16 thred_y[ISP33_GIC_THRED_Y_NUM];
	/* MIN_THRED_Y */
	__u16 minthred_y[ISP33_GIC_THRED_Y_NUM];
	/* THRED_SCALE */
	__u16 autonoisethred_scale;
	/* LOFLTGR_COEFF */
	__u8 lofltgr_coeff0;
	__u8 lofltgr_coeff1;
	__u8 lofltgr_coeff2;
	__u8 lofltgr_coeff3;
	/* LOFLTGB_COEFF */
	__u8 lofltgb_coeff0;
	__u8 lofltgb_coeff1;
	/* SUM_LOFLT_INV */
	__u16 sumlofltcoeff_inv;
	/* LOFLTTHRED_COEFF */
	__u8 lofltthred_coeff0;
	__u8 lofltthred_coeff1;
	/* GAIN */
	__u8 globalgain_alpha;
	__u8 globalgain_scale;
	__u16 global_gain;
	/* GAIN_SLOPE */
	__u16 gain_offset;
	__u16 gain_scale;
	/* GAIN_THRED */
	__u16 gainadjflt_minthred;
	__u16 gainadjflt_maxthred;
} __attribute__ ((packed));

struct isp33_cac_cfg {
	/* CTRL */
	__u8 bypass_en;
	__u8 edge_detect_en;
	__u8 neg_clip0_en;
	__u8 wgt_color_en;
	/* PSF_PARA */
	__u8 psf_table_fix_bit;
	/* HIGH_DIRECT */
	__u16 hi_drct_ratio;
	/* OVER_EXPO0 */
	__u32 over_expo_thred;
	/* OVER_EXP01 */
	__u32 over_expo_adj;
	/* FLAT */
	__u8 flat_thred;
	__u16 flat_offset;
	/* GAUSS_COEFF */
	__u8 chroma_lo_flt_coeff0;
	__u8 chroma_lo_flt_coeff1;
	__u8 color_lo_flt_coeff0;
	__u8 color_lo_flt_coeff1;
	/* RATIO */
	__u16 search_range_ratio;
	__u16 residual_chroma_ratio;
	/* WGT_COLOR_B */
	__u32 wgt_color_b_min_thred;
	/* WGT_COLOR_R */
	__u32 wgt_color_r_min_thred;
	/* WGT_COLOR_SLOPE_B */
	__u32 wgt_color_b_slope;
	/* WGT_COLOR_SLOPE_R */
	__u32 wgt_color_r_slope;
	/* WGT_COLOR_LUMA0 */
	__u32 wgt_color_min_luma;
	/* WGT_COLOR_LUMA1 */
	__u32 wgt_color_luma_slope;
	/* WGT_OVER_EXPO0 */
	__u32 wgt_over_expo_min_thred;
	/* WGT_OVER_EXPO1 */
	__u32 wgt_over_expo_slope;
	/* WGT_CONTRAST0 */
	__u32 wgt_contrast_min_thred;
	/* WGT_CONTRAST1 */
	__u32 wgt_contrast_slope;
	/* WGT_CONTRAST2 */
	__u32 wgt_contrast_offset;
	/* WGT_DARK_AREA0 */
	__u32 wgt_dark_thed;
	/* WGT_DARK_AREA1 */
	__u32 wgt_dark_slope;
	/* PSF_B */
	__u8 psf_b_ker[ISP33_CAC_PSF_NUM];
	/* PSF_R */
	__u8 psf_r_ker[ISP33_CAC_PSF_NUM];
} __attribute__ ((packed));

struct isp33_ccm_cfg {
	/* CTRL */
	__u8 highy_adjust_dis;
	__u8 enh_adj_en;
	__u8 asym_adj_en;
	__u8 sat_decay_en;
	/* COEFF0_R */
	__s16 coeff0_r;
	__s16 coeff1_r;
	/* COEFF1_R */
	__s16 coeff2_r;
	__s16 offset_r;
	/* COEFF0_G */
	__s16 coeff0_g;
	__s16 coeff1_g;
	/* COEFF1_G */
	__s16 coeff2_g;
	__s16 offset_g;
	/* COEFF0_B */
	__s16 coeff0_b;
	__s16 coeff1_b;
	/* COEFF1_B */
	__s16 coeff2_b;
	__s16 offset_b;
	/* COEFF0_Y */
	__u16 coeff0_y;
	__u16 coeff1_y;
	/* COEFF1_Y */
	__u16 coeff2_y;
	/* ALP_Y */
	__u16 alp_y[ISP33_CCM_CURVE_NUM];
	/* BOUND_BIT */
	__u8 bound_bit;
	__u8 right_bit;
	/* ENHANCE0 */
	__u16 color_coef0_r2y;
	__u16 color_coef1_g2y;
	/* ENHANCE1 */
	__u16 color_coef2_b2y;
	__u16 color_enh_rat_max;
	/* HF_THD */
	__u8 hf_low;
	__u8 hf_up;
	__u16 hf_scale;
	/* HF_FACTOR */
	__u16 hf_factor[ISP33_CCM_HF_FACTOR_NUM];
} __attribute__ ((packed));

struct isp33_debayer_cfg {
	/* CONTROL */
	__u8 bypass;
	__u8 g_out_flt_en;
	/* LUMA_DX */
	__u8 luma_dx[ISP33_DEBAYER_LUMA_NUM];
	/* G_INTERP */
	__u8 g_interp_clip_en;
	__u8 hi_texture_thred;
	__u8 hi_drct_thred;
	__u8 lo_drct_thred;
	__u8 drct_method_thred;
	__u8 g_interp_sharp_strg_max_limit;
	/* G_INTERP_FILTER1 */
	__s8 lo_drct_flt_coeff1;
	__s8 lo_drct_flt_coeff2;
	__s8 lo_drct_flt_coeff3;
	__s8 lo_drct_flt_coeff4;
	/* G_INTERP_FILTER2 */
	__s8 hi_drct_flt_coeff1;
	__s8 hi_drct_flt_coeff2;
	__s8 hi_drct_flt_coeff3;
	__s8 hi_drct_flt_coeff4;
	/* G_INTERP_OFFSET_ALPHA */
	__u16 g_interp_sharp_strg_offset;
	__u8 grad_lo_flt_alpha;
	/* G_INTERP_DRCT_OFFSET */
	__u16 drct_offset[ISP33_DEBAYER_DRCT_OFFSET_NUM];
	/* G_FILTER_MODE_OFFSET */
	__u8 gflt_mode;
	__u16 gflt_ratio;
	__u16 gflt_offset;
	/* G_FILTER_FILTER */
	__s8 gflt_coe0;
	__s8 gflt_coe1;
	__s8 gflt_coe2;
	/* G_FILTER_VSIGMA */
	__u16 gflt_vsigma[ISP33_DEBAYER_VSIGMA_NUM];
} __attribute__ ((packed));

struct isp33_bay3d_cfg {
	/* BAY3D_CTRL0 */
	__u8 bypass_en;
	__u8 iirsparse_en;
	__u8 out_use_pre_mode;
	__u8 motion_est_en;
	/* BAY3D_CTRL1 */
	__u8 transf_bypass_en;
	__u8 tnrsigma_curve_double_en;
	__u8 md_large_lo_use_mode;
	__u8 md_large_lo_min_filter_bypass_en;
	__u8 md_large_lo_gauss_filter_bypass_en;
	__u8 md_large_lo_md_wgt_bypass_en;
	__u8 pre_pix_out_mode;
	__u8 motion_detect_bypass_en;
	__u8 lpf_hi_bypass_en;
	__u8 lo_diff_vfilt_bypass_en;
	__u8 lpf_lo_bypass_en;
	__u8 lo_wgt_hfilt_en;
	__u8 lo_diff_hfilt_en;
	__u8 sig_hfilt_en;
	__u8 lo_detection_bypass_en;
	__u8 lo_mge_wgt_mode;
	__u8 pre_spnr_out_en;
	__u8 md_only_lo_en;
	__u8 cur_spnr_out_en;
	__u8 md_wgt_out_en;
	/* BAY3D_CTRL2 */
	__u8 cur_spnr_filter_bypass_en;
	__u8 pre_spnr_hi_filter_gic_en;
	__u8 pre_spnr_hi_filter_gic_enhance_en;
	__u8 spnr_presigma_use_en;
	__u8 pre_spnr_lo_filter_bypass_en;
	__u8 pre_spnr_hi_filter_bypass_en;
	__u8 pre_spnr_sigma_curve_double_en;
	__u8 pre_spnr_hi_guide_filter_bypass_en;
	__u8 pre_spnr_sigma_idx_filt_bypass_en;
	__u8 pre_spnr_sigma_idx_filt_mode;
	__u8 pre_spnr_hi_noise_ctrl_en;
	__u8 pre_spnr_hi_filter_wgt_mode;
	__u8 pre_spnr_lo_filter_wgt_mode;
	__u8 pre_spnr_hi_filter_rb_wgt_mode;
	__u8 pre_spnr_lo_filter_rb_wgt_mode;
	__u8 pre_hi_gic_lp_en;
	__u8 pre_hi_bf_lp_en;
	__u8 pre_lo_avg_lp_en;
	/* BAY3D_CTRL3 */
	__u8 transf_mode;
	__u8 wgt_cal_mode;
	__u8 ww_mode;
	__u8 wgt_last_mode;
	__u8 mge_wgt_hdr_sht_thred;
	__u8 sigma_calc_mge_wgt_hdr_sht_thred;
	/* BAY3D_TRANS0 */
	__u8 transf_mode_scale;
	__u16 transf_mode_offset;
	__u16 itransf_mode_offset;
	/* BAY3D_TRANS1 */
	__u32 transf_data_max_limit;
	/* BAY3D_CURHI_SIGSCL */
	__u16 pre_sig_ctrl_scl;
	/* BAY3D_CURHI_SIGOF */
	__u8 pre_hi_guide_out_wgt;
	/* BAY3D_CURHISPW */
	__u8 cur_spnr_filter_coeff[ISP33_BAY3D_FILT_COEFF_NUM];
	/* BAY3D_IIRSX */
	__u16 pre_spnr_luma2sigma_x[ISP33_BAY3D_XY_NUM];
	/* BAY3D_IIRSY */
	__u16 pre_spnr_luma2sigma_y[ISP33_BAY3D_XY_NUM];
	/* BAY3D_PREHI_SIGSCL */
	__u16 pre_spnr_hi_sigma_scale;
	/* BAY3D_PREHI_WSCL */
	__u8 pre_spnr_hi_wgt_calc_scale;
	/* BAY3D_PREHIWMM */
	__u8 pre_spnr_hi_filter_wgt_min_limit;
	/* BAY3D_PREHISIGOF */
	__u8 pre_spnr_hi_filter_out_wgt;
	__u8 pre_spnr_sigma_offset;
	__u8 pre_spnr_sigma_hdr_sht_offset;
	/* BAY3D_PREHISIGSCL */
	__u16 pre_spnr_sigma_scale;
	__u16 pre_spnr_sigma_hdr_sht_scale;
	/* BAY3D_PREHISPW */
	__u8 pre_spnr_hi_filter_coeff[ISP33_BAY3D_FILT_COEFF_NUM];
	/* BAY3D_PRELOSIGCSL */
	__u16 pre_spnr_lo_sigma_scale;
	/* BAY3D_PRELOSIGOF */
	__u8 pre_spnr_lo_wgt_calc_scale;
	/* BAY3D_PREHI_NRCT */
	__u16 pre_spnr_hi_noise_ctrl_scale;
	__u8 pre_spnr_hi_noise_ctrl_offset;
	/* BAY3D_TNRSX */
	__u16 tnr_luma2sigma_x[ISP33_BAY3D_TNRSIG_NUM];
	/* BAY3D_TNRSY */
	__u16 tnr_luma2sigma_y[ISP33_BAY3D_TNRSIG_NUM];
	/* BAY3D_HIWD */
	__u16 lpf_hi_coeff[ISP33_BAY3D_LPF_COEFF_NUM];
	/* BAY3D_LOWD */
	__u16 lpf_lo_coeff[ISP33_BAY3D_LPF_COEFF_NUM];
	/* BAY3D_GF */
	__u8 sigma_idx_filt_coeff[ISP33_BAY3D_FILT_COEFF_NUM];
	__u16 lo_wgt_cal_first_line_sigma_scale;
	/* BAY3D_VIIR */
	__u8 lo_diff_vfilt_wgt;
	__u8 lo_wgt_vfilt_wgt;
	__u8 sig_first_line_scale;
	__u8 lo_diff_first_line_scale;
	/* BAY3D_LFSCL */
	__u16 lo_wgt_cal_offset;
	__u16 lo_wgt_cal_scale;
	/* BAY3D_LFSCLTH */
	__u16 lo_wgt_cal_max_limit;
	__u16 mode0_base_ratio;
	/* BAY3D_DSWGTSCL */
	__u16 lo_diff_wgt_cal_offset;
	__u16 lo_diff_wgt_cal_scale;
	/* BAY3D_WGTLASTSCL */
	__u16 lo_mge_pre_wgt_offset;
	__u16 lo_mge_pre_wgt_scale;
	/* BAY3D_WGTSCL0 */
	__u16 mode0_lo_wgt_scale;
	__u16 mode0_lo_wgt_hdr_sht_scale;
	/* BAY3D_WGTSCL1 */
	__u16 mode1_lo_wgt_scale;
	__u16 mode1_lo_wgt_hdr_sht_scale;
	/* BAY3D_WGTSCL2 */
	__u16 mode1_wgt_scale;
	__u16 mode1_wgt_hdr_sht_scale;
	/* BAY3D_WGTOFF */
	__u16 mode1_lo_wgt_offset;
	__u16 mode1_lo_wgt_hdr_sht_offset;
	/* BAY3D_WGT1OFF */
	__u16 auto_sigma_count_wgt_thred;
	__u16 mode1_wgt_min_limit;
	__u16 mode1_wgt_offset;
	/* BAY3D_SIGORG */
	__u32 tnr_out_sigma_sq;
	/* BAY3D_WGTLO_L */
	__u16 lo_wgt_clip_min_limit;
	__u16 lo_wgt_clip_hdr_sht_min_limit;
	/* BAY3D_WGTLO_H */
	__u16 lo_wgt_clip_max_limit;
	__u16 lo_wgt_clip_hdr_sht_max_limit;
	/* BAY3D_STH_SCL */
	__u16 lo_pre_gg_soft_thresh_scale;
	__u16 lo_pre_rb_soft_thresh_scale;
	/* BAY3D_STH_LIMIT */
	__u16 lo_pre_soft_thresh_max_limit;
	__u16 lo_pre_soft_thresh_min_limit;
	/* BAY3D_HIKEEP */
	__u8 cur_spnr_hi_wgt_min_limit;
	__u8 pre_spnr_hi_wgt_min_limit;
	__u16 motion_est_lo_wgt_thred;
	/* BAY3D_PIXMAX */
	__u16 pix_max_limit;
	/* BAY3D_SIGNUMTH */
	__u32 sigma_num_th;
	/* BAY3D_MONR */
	__u32 out_use_hi_noise_bal_nr_strg;
	__u8 gain_out_max_limit;
	/* BAY3D_SIGSCL */
	__u16 sigma_scale;
	__u16 sigma_hdr_sht_scale;
	/* BAY3D_DSOFF */
	__u16 lo_wgt_vfilt_offset;
	__u16 lo_diff_vfilt_offset;
	__u8 lo_wgt_cal_first_line_vfilt_wgt;
	/* BAY3D_DSSCL */
	__u8 lo_wgt_vfilt_scale;
	__u8 lo_diff_vfilt_scale_bit;
	__u8 lo_diff_vfilt_scale;
	__u8 lo_diff_first_line_vfilt_wgt;
	/* BAY3D_ME0 */
	__u16 motion_est_up_mvx_cost_offset;
	__u16 motion_est_up_mvx_cost_scale;
	__u8 motion_est_sad_vert_wgt0;
	/* BAY3D_ME1 */
	__u16 motion_est_up_left_mvx_cost_offset;
	__u16 motion_est_up_left_mvx_cost_scale;
	__u8 motion_est_sad_vert_wgt1;
	/* BAY3D_ME2 */
	__u16 motion_est_up_right_mvx_cost_offset;
	__u16 motion_est_up_right_mvx_cost_scale;
	__u8 motion_est_sad_vert_wgt2;
	/* BAY3D_WGTMAX */
	__u16 lo_wgt_clip_motion_max_limit;
	/* BAY3D_WGT1MAX */
	__u16 mode1_wgt_max_limit;
	/* BAY3D_WGTM0 */
	__u16 mode0_wgt_out_max_limit;
	__u16 mode0_wgt_out_offset;
	/* BAY3D_PRELOWGT */
	__u8 pre_spnr_lo_val_wgt_out_wgt;
	__u8 pre_spnr_lo_filter_out_wgt;
	__u8 pre_spnr_lo_filter_wgt_min;
	/* BAY3D_MIDBIG0 */
	__u8 md_large_lo_md_wgt_offset;
	__u16 md_large_lo_md_wgt_scale;
	/* BAY3D_MIDBIG1 */
	__u16 md_large_lo_wgt_cut_offset;
	__u16 md_large_lo_wgt_add_offset;
	/* BAY3D_MIDBIG2 */
	__u16 md_large_lo_wgt_scale;
} __attribute__ ((packed));

struct isp33_ynr_cfg {
	/* GLOBAL_CTRL */
	__u8 hi_spnr_bypass;
	__u8 mi_spnr_bypass;
	__u8 lo_spnr_bypass;
	__u8 rnr_en;
	__u8 tex2lo_strg_en;
	__u8 hi_lp_en;
	/* GAIN_CTRL */
	__u16 global_set_gain;
	__u8 gain_merge_alpha;
	__u8 local_gain_scale;
	/* GAIN_ADJ */
	__u16 lo_spnr_gain2strg[ISP33_YNR_ADJ_NUM];
	/* RNR_MAX_R */
	__u16 rnr_max_radius;
	/* RNR_CENTER_COOR */
	__u16 rnr_center_h;
	__u16 rnr_center_v;
	/* RNR_STRENGTH */
	__u8 radius2strg[ISP33_YNR_XY_NUM];
	/* SGM_DX */
	__u16 luma2sima_x[ISP33_YNR_XY_NUM];
	/* SGM_Y */
	__u16 luma2sima_y[ISP33_YNR_XY_NUM];
	/* HI_SIGMA_GAIN */
	__u16 hi_spnr_sigma_min_limit;
	__u16 hi_spnr_strg;
	__u8 hi_spnr_local_gain_alpha;
	/* HI_GAUS_COE */
	__u8 hi_spnr_filt_coeff[ISP33_YNR_HI_GAUS_COE_NUM];
	/* HI_WEIGHT */
	__u16 hi_spnr_filt_wgt_offset;
	__u16 hi_spnr_filt_center_wgt;
	/* HI_GAUS1_COE */
	__u16 hi_spnr_filt1_coeff[ISP33_YNR_HI_GAUS1_COE_NUM];
	/* HI_TEXT */
	__u16 hi_spnr_filt1_tex_thred;
	__u16 hi_spnr_filt1_tex_scale;
	__u16 hi_spnr_filt1_wgt_alpha;
	/* MI_GAUS_COE */
	__u16 mi_spnr_filt_coeff0;
	__u16 mi_spnr_filt_coeff1;
	__u16 mi_spnr_filt_coeff2;
	/* MI_STRG_DETAIL */
	__u16 mi_spnr_strg;
	__u16 mi_spnr_soft_thred_scale;
	/* MI_WEIGHT */
	__u8 mi_spnr_wgt;
	__u8 mi_ehance_scale_en;
	__u8 mi_ehance_scale;
	__u16 mi_spnr_filt_center_wgt;
	/* LO_STRG_DETAIL */
	__u16 lo_spnr_strg;
	__u16 lo_spnr_soft_thred_scale;
	/* LO_LIMIT_SCALE */
	__u16 lo_spnr_thumb_thred_scale;
	__u16 tex2lo_strg_mantissa;
	__u8 tex2lo_strg_exponent;
	/* LO_WEIGHT */
	__u8 lo_spnr_wgt;
	__u16 lo_spnr_filt_center_wgt;
	/* LO_TEXT_THRED */
	__u16 tex2lo_strg_upper_thred;
	__u16 tex2lo_strg_lower_thred;
	/* FUSION_WEIT_ADJ */
	__u8 lo_gain2wgt[ISP33_YNR_ADJ_NUM];
} __attribute__ ((packed));

struct isp33_cnr_cfg {
	/* CNR_CTRL */
	__u8 exgain_bypass;
	__u8 yuv422_mode;
	__u8 thumb_mode;
	__u8 hiflt_wgt0_mode;
	__u8 local_alpha_dis;
	__u8 loflt_coeff;
	/* CNR_EXGAIN */
	__u16 global_gain;
	__u8 global_gain_alpha;
	__u8 local_gain_scale;
	/* CNR_THUMB1 */
	__u16 lobfflt_vsigma_uv;
	__u16 lobfflt_vsigma_y;
	/* CNR_THUMB_BF_RATIO */
	__u16 lobfflt_alpha;
	/* CNR_LBF_WEITD */
	__u8 thumb_bf_coeff0;
	__u8 thumb_bf_coeff1;
	__u8 thumb_bf_coeff2;
	__u8 thumb_bf_coeff3;
	/* CNR_IIR_PARA1 */
	__u8 loflt_uv_gain;
	__u8 loflt_vsigma;
	__u8 exp_x_shift_bit;
	__u16 loflt_wgt_slope;
	/* CNR_IIR_PARA2 */
	__u8 loflt_wgt_min_thred;
	__u8 loflt_wgt_max_limit;
	/* CNR_GAUS_COE */
	__u8 gaus_flt_coeff[ISP33_CNR_GAUS_COE_NUM];
	/* CNR_GAUS_RATIO */
	__u8 hiflt_wgt_min_limit;
	__u16 gaus_flt_alpha;
	__u16 hiflt_alpha;
	/* CNR_BF_PARA1 */
	__u8 hiflt_uv_gain;
	__u8 hiflt_cur_wgt;
	__u16 hiflt_global_vsigma;
	/* CNR_BF_PARA2 */
	__u16 adj_offset;
	__u16 adj_scale;
	/* CNR_SIGMA */
	__u8 sgm_ratio[ISP33_CNR_SIGMA_Y_NUM];
	__u16 bf_merge_max_limit;
	/* CNR_IIR_GLOBAL_GAIN */
	__u8 loflt_global_sgm_ratio;
	__u8 loflt_global_sgm_ratio_alpha;
	__u16 bf_alpha_max_limit;
	/* CNR_WGT_SIGMA */
	__u8 cur_wgt[ISP33_CNR_WGT_SIGMA_Y_NUM];
	/* GAUS_X_SIGMAR */
	__u16 hiflt_vsigma_idx[ISP33_CNR_GAUS_SIGMAR_NUM];
	/* GAUS_Y_SIGMAR */
	__u16 hiflt_vsigma[ISP33_CNR_GAUS_SIGMAR_NUM];
} __attribute__ ((packed));

struct isp33_sharp_cfg {
	/* SHARP_EN */
	__u8 bypass;
	__u8 local_gain_bypass;
	__u8 tex_est_mode;
	__u8 max_min_flt_mode;
	__u8 detail_fusion_wgt_mode;
	__u8 noise_calc_mode;
	__u8 radius_step_mode;
	__u8 noise_curve_mode;
	__u8 gain_wgt_mode;
	__u8 detail_lp_en;
	__u8 debug_mode;
	/* TEXTURE0 */
	__u16 fst_noise_scale;
	__u16 fst_sigma_scale;
	/* TEXTURE1 */
	__u16 fst_sigma_offset;
	__u16 fst_wgt_scale;
	/* TEXTURE2 */
	__u8 tex_wgt_mode;
	__u8 noise_est_alpha;
	/* TEXTURE3 */
	__u16 sec_noise_scale;
	__u16 sec_sigma_scale;
	/* TEXTURE4 */
	__u16 sec_sigma_offset;
	__u16 sec_wgt_scale;
	/* HPF_KERNEL */
	__u8 img_hpf_coeff[ISP33_SHARP_KERNEL_NUM];
	/* TEXFLT_KERNEL */
	__u8 texWgt_flt_coeff0;
	__u8 texWgt_flt_coeff1;
	__u8 texWgt_flt_coeff2;
	/* DETAIL0 */
	__u8 detail_in_alpha;
	__u8 pre_bifilt_alpha;
	__u8 fusion_wgt_min_limit;
	__u8 fusion_wgt_max_limit;
	__u16 pre_bifilt_slope_fix;
	/* DETAIL1 */
	__u32 detail_fusion_slope_fix;
	/* LUMA_DX */
	__u8 luma_dx[ISP33_SHARP_X_NUM];
	/* PBF_VSIGMA */
	__u16 pre_bifilt_vsigma_inv[ISP33_SHARP_Y_NUM];
	/* PBF_KERNEL */
	__u8 pre_bifilt_coeff0;
	__u8 pre_bifilt_coeff1;
	__u8 pre_bifilt_coeff2;
	/* DETAIL_KERNEL */
	__u8 hi_detail_lpf_coeff[ISP33_SHARP_KERNEL_NUM];
	__u8 mi_detail_lpf_coeff[ISP33_SHARP_KERNEL_NUM];
	/* GAIN */
	__u16 global_gain;
	__u8 gain_merge_alpha;
	__u8 local_gain_scale;
	/* GAIN_ADJ0 */
	__u8 edge_gain_max_limit;
	__u8 edge_gain_min_limit;
	__u8 detail_gain_max_limit;
	__u8 detail_gain_min_limit;
	/* GAIN_ADJ1 */
	__u8 hitex_gain_max_limit;
	__u8 hitex_gain_min_limit;
	/* GAIN_ADJ2 */
	__u8 edge_gain_slope;
	__u8 detail_gain_slope;
	__u8 hitex_gain_slope;
	/* GAIN_ADJ3 */
	__u16 edge_gain_offset;
	__u16 detail_gain_offset;
	__u16 hitex_gain_offset;
	/* GAIN_ADJ4 */
	__u16 edge_gain_sigma;
	__u16 detail_gain_sigma;
	/* EDGE0 */
	__u16 pos_edge_wgt_scale;
	__u16 neg_edge_wgt_scale;
	/* EDGE1 */
	__u8 pos_edge_strg;
	__u8 neg_edge_strg;
	__u8 overshoot_alpha;
	__u8 undershoot_alpha;
	/* EDGE_KERNEL */
	__u8 edge_lpf_coeff[ISP33_SHARP_EDGE_KERNEL_NUM];
	/* EDGE_WGT_VAL */
	__u16 edge_wgt_val[ISP33_SHARP_EDGE_WGT_NUM];
	/* LUMA_ADJ_STRG */
	__u8 luma2strg[ISP33_SHARP_LUMA_STRG_NUM];
	/* CENTER */
	__u16 center_x;
	__u16 center_y;
	/* OUT_LIMIT */
	__u16 flat_max_limit;
	__u16 edge_min_limit;
	/* TEX_X_INV_FIX */
	__u32 tex_x_inv_fix0;
	__u32 tex_x_inv_fix1;
	__u32 tex_x_inv_fix2;
	/* LOCAL_STRG */
	__u16 tex2detail_strg[ISP33_SHARP_LOCAL_STRG_NUM];
	__u16 tex2loss_tex_in_hinr_strg[ISP33_SHARP_LOCAL_STRG_NUM];
	/* DETAIL_SCALE_TAB */
	__u8 contrast2pos_strg[ISP33_SHARP_CONTRAST_STRG_NUM];
	__u8 contrast2neg_strg[ISP33_SHARP_CONTRAST_STRG_NUM];
	__u8 pos_detail_strg;
	__u8 neg_detail_strg;
	/* DETAIL_TEX_CLIP */
	__u16 tex2detail_pos_clip[ISP33_SHARP_TEX_CLIP_NUM];
	__u16 tex2detail_neg_clip[ISP33_SHARP_TEX_CLIP_NUM];
	/* GRAIN_TEX_CLIP */
	__u16 tex2grain_pos_clip[ISP33_SHARP_TEX_CLIP_NUM];
	__u16 tex2grain_neg_clip[ISP33_SHARP_TEX_CLIP_NUM];
	/* DETAIL_LUMA_CLIP */
	__u16 luma2detail_pos_clip[ISP33_SHARP_LUM_CLIP_NUM];
	__u16 luma2detail_neg_clip[ISP33_SHARP_LUM_CLIP_NUM];
	/* GRAIN_STRG */
	__u8 grain_strg;
	/* HUE_ADJ_TAB */
	__u16 hue2strg[ISP33_SHARP_HUE_NUM];
	/* DISATANCE_ADJ */
	__u8 distance2strg[ISP33_SHARP_DISATANCE_NUM];
	/* NOISE_SIGMA */
	__u16 hi_tex_threshold[ISP33_SHARP_HITEX_NUM];
	/* LOSSTEXINHINR_STRG */
	__u8 loss_tex_in_hinr_strg;
	/* NOISE_CURVE */
	__u16 noise_curve_ext[ISP33_SHARP_NOISE_CURVE_NUM];
	__u8 noise_count_thred_ratio;
	__u8 noise_clip_scale;
	/* NOISE_CLIP */
	__u16 noise_clip_min_limit;
	__u16 noise_clip_max_limit;
} __attribute__ ((packed));

struct isp33_enh_cfg {
	/* CTRL */
	__u8 bypass;
	__u8 blf3_bypass;
	/* IIR_FLT */
	__u16 iir_inv_sigma;
	__u8 iir_soft_thed;
	__u8 iir_cur_wgt;
	/* BILAT_FLT3X3 */
	__u16 blf3_inv_sigma;
	__u16 blf3_cur_wgt;
	__u8 blf3_thumb_cur_wgt;
	/* BILAT_FLT5X5 */
	__u8 blf5_cur_wgt;
	__u16 blf5_inv_sigma;
	/* GLOBAL_STRG */
	__u16 global_strg;
	/* LUMA_LUT */
	__u16 lum2strg[ISP33_ENH_LUMA_NUM];
	/* DETAIL_IDX */
	__u16 detail2strg_idx[ISP33_ENH_DETAIL_NUM];
	/* DETAIL_POWER */
	__u8 detail2strg_power0;
	__u8 detail2strg_power1;
	__u8 detail2strg_power2;
	__u8 detail2strg_power3;
	__u8 detail2strg_power4;
	__u8 detail2strg_power5;
	__u8 detail2strg_power6;
	/* DETAIL_VALUE */
	__u16 detail2strg_val[ISP33_ENH_DETAIL_NUM];
	/* PRE_FRAME */
	__u8 pre_wet_frame_cnt0;
	__u8 pre_wet_frame_cnt1;
	/* IIR */
	__u8 iir_wr;
	__u8 iir[ISP33_ENH_IIR_ROW_MAX][ISP33_ENH_IIR_COL_MAX];
} __attribute__ ((packed));

struct isp33_hist_cfg {
	/* CTRL */
	__u8 bypass;
	__u8 mem_mode;
	/* HF_STAT */
	__u8 count_scale;
	__u8 count_offset;
	__u16 count_min_limit;
	/* BLOCK_SIZE */
	__u16 blk_het;
	__u16 blk_wid;
	/* THUMB_SIZE */
	__u8 thumb_row;
	__u8 thumb_col;
	/* MAP0 */
	__u16 merge_alpha;
	__u16 user_set;
	/* MAP1 */
	__u16 map_count_scale;
	__u8 gain_ref_wgt;
	/* IIR */
	__u8 flt_cur_wgt;
	__u16 flt_inv_sigma;
	/* POS_ALPHA */
	__u8 pos_alpha[ISP33_HIST_ALPHA_NUM];
	/* NEG_ALPHA */
	__u8 neg_alpha[ISP33_HIST_ALPHA_NUM];
	/* STAB */
	__u8 stab_frame_cnt0;
	__u8 stab_frame_cnt1;
	/* UV_SCL */
	__u8 saturate_scale;
	/* HIST_IIR */
	__u16 iir_wr;
	__u16 iir[ISP33_HIST_IIR_BLK_MAX][ISP33_HIST_IIR_NUM];
} __attribute__ ((packed));

struct isp33_drc_cfg {
	/* CTRL0 */
	__u8 bypass_en;
	__u8 cmps_byp_en;
	__u8 gainx32_en;
	__u8 bf_lp_en;
	__u8 raw_dly_dis;
	/* CTRL1 */
	__u16 position;
	__u16 compres_scl;
	__u8 offset_pow2;
	/* LPRATIO */
	__u16 lpdetail_ratio;
	__u16 hpdetail_ratio;
	__u8 delta_scalein;
	/* BILAT0 */
	__u8 bilat_wt_off;
	__u16 thumb_thd_neg;
	__u8 thumb_thd_enable;
	__u8 weicur_pix;
	/* BILAT1 */
	__u8 cmps_offset_bits_int;
	__u8 cmps_fixbit_mode;
	__u16 drc_gas_t;
	/* BILAT2 */
	__u16 thumb_clip;
	__u8 thumb_scale;
	/* BILAT3 */
	__u16 range_sgm_inv0;
	__u16 range_sgm_inv1;
	/* BILAT4 */
	__u8 weig_bilat;
	__u8 weight_8x8thumb;
	__u8 enable_soft_thd;
	__u16 bilat_soft_thd;
	/* GAIN_Y */
	__u16 gain_y[ISP33_DRC_Y_NUM];
	/* COMPRES_Y */
	__u16 compres_y[ISP33_DRC_Y_NUM];
	/* SCALE_Y */
	__u16 scale_y[ISP33_DRC_Y_NUM];
	/* IIRWG_GAIN */
	__u16 min_ogain;
	/* SFTHD_Y */
	__u16 sfthd_y[ISP33_DRC_Y_NUM];
} __attribute__ ((packed));

struct isp33_rawawb_meas_cfg {
	__u8 bls2_en;

	__u8 rawawb_sel;
	__u8 bnr2awb_sel;
	__u8 drc2awb_sel;
	/* RAWAWB_CTRL */
	__u8 uv_en0;
	__u8 xy_en0;
	__u8 yuv3d_en0;
	__u8 yuv3d_ls_idx0;
	__u8 yuv3d_ls_idx1;
	__u8 yuv3d_ls_idx2;
	__u8 yuv3d_ls_idx3;
	__u8 in_rshift_to_12bit_en;
	__u8 in_overexposure_check_en;
	__u8 wind_size;
	__u8 rawlsc_bypass_en;
	__u8 light_num;
	__u8 uv_en1;
	__u8 xy_en1;
	__u8 yuv3d_en1;
	__u8 low12bit_val;
	/* RAWAWB_BLK_CTRL */
	__u8 blk_measure_enable;
	__u8 blk_measure_mode;
	__u8 blk_measure_xytype;
	__u8 blk_rtdw_measure_en;
	__u8 blk_measure_illu_idx;
	__u8 ds16x8_mode_en;
	__u8 blk_with_luma_wei_en;
	__u8 ovexp_2ddr_dis;
	__u16 in_overexposure_threshold;
	/* RAWAWB_WIN_OFFS */
	__u16 h_offs;
	__u16 v_offs;
	/* RAWAWB_WIN_SIZE */
	__u16 h_size;
	__u16 v_size;
	/* RAWAWB_LIMIT_RG_MAX*/
	__u16 r_max;
	__u16 g_max;
	/* RAWAWB_LIMIT_BY_MAX */
	__u16 b_max;
	__u16 y_max;
	/* RAWAWB_LIMIT_RG_MIN */
	__u16 r_min;
	__u16 g_min;
	/* RAWAWB_LIMIT_BY_MIN */
	__u16 b_min;
	__u16 y_min;
	/* RAWAWB_WEIGHT_CURVE_CTRL */
	__u8 wp_luma_wei_en0;
	__u8 wp_luma_wei_en1;
	__u8 wp_blk_wei_en0;
	__u8 wp_blk_wei_en1;
	__u8 wp_hist_xytype;
	/* RAWAWB_YWEIGHT_CURVE_XCOOR03 */
	__u8 wp_luma_weicurve_y0;
	__u8 wp_luma_weicurve_y1;
	__u8 wp_luma_weicurve_y2;
	__u8 wp_luma_weicurve_y3;
	/* RAWAWB_YWEIGHT_CURVE_XCOOR47 */
	__u8 wp_luma_weicurve_y4;
	__u8 wp_luma_weicurve_y5;
	__u8 wp_luma_weicurve_y6;
	__u8 wp_luma_weicurve_y7;
	/* RAWAWB_YWEIGHT_CURVE_XCOOR8 */
	__u8 wp_luma_weicurve_y8;
	/* RAWAWB_YWEIGHT_CURVE_YCOOR03 */
	__u8 wp_luma_weicurve_w0;
	__u8 wp_luma_weicurve_w1;
	__u8 wp_luma_weicurve_w2;
	__u8 wp_luma_weicurve_w3;
	/* RAWAWB_YWEIGHT_CURVE_YCOOR47 */
	__u8 wp_luma_weicurve_w4;
	__u8 wp_luma_weicurve_w5;
	__u8 wp_luma_weicurve_w6;
	__u8 wp_luma_weicurve_w7;
	/* RAWAWB_YWEIGHT_CURVE_YCOOR8 */
	__u8 wp_luma_weicurve_w8;
	__u16 pre_wbgain_inv_r;
	/* RAWAWB_PRE_WBGAIN_INV */
	__u16 pre_wbgain_inv_g;
	__u16 pre_wbgain_inv_b;
	/* RAWAWB_UV_DETC_VERTEX0_0 */
	__u16 vertex0_u_0;
	__u16 vertex0_v_0;
	/* RAWAWB_UV_DETC_VERTEX1_0 */
	__u16 vertex1_u_0;
	__u16 vertex1_v_0;
	/* RAWAWB_UV_DETC_VERTEX2_0 */
	__u16 vertex2_u_0;
	__u16 vertex2_v_0;
	/* RAWAWB_UV_DETC_VERTEX3_0 */
	__u16 vertex3_u_0;
	__u16 vertex3_v_0;
	/* RAWAWB_UV_DETC_ISLOPE01_0 */
	__u32 islope01_0;
	/* RAWAWB_UV_DETC_ISLOPE12_0 */
	__u32 islope12_0;
	/* RAWAWB_UV_DETC_ISLOPE23_0 */
	__u32 islope23_0;
	/* RAWAWB_UV_DETC_ISLOPE30_0 */
	__u32 islope30_0;
	/* RAWAWB_UV_DETC_VERTEX0_1 */
	__u16 vertex0_u_1;
	__u16 vertex0_v_1;
	/* RAWAWB_UV_DETC_VERTEX1_1 */
	__u16 vertex1_u_1;
	__u16 vertex1_v_1;
	/* RAWAWB_UV_DETC_VERTEX2_1 */
	__u16 vertex2_u_1;
	__u16 vertex2_v_1;
	/* RAWAWB_UV_DETC_VERTEX3_1 */
	__u16 vertex3_u_1;
	__u16 vertex3_v_1;
	/* RAWAWB_UV_DETC_ISLOPE01_1 */
	__u32 islope01_1;
	/* RAWAWB_UV_DETC_ISLOPE12_1 */
	__u32 islope12_1;
	/* RAWAWB_UV_DETC_ISLOPE23_1 */
	__u32 islope23_1;
	/* RAWAWB_UV_DETC_ISLOPE30_1 */
	__u32 islope30_1;
	/* RAWAWB_UV_DETC_VERTEX0_2 */
	__u16 vertex0_u_2;
	__u16 vertex0_v_2;
	/* RAWAWB_UV_DETC_VERTEX1_2 */
	__u16 vertex1_u_2;
	__u16 vertex1_v_2;
	/* RAWAWB_UV_DETC_VERTEX2_2 */
	__u16 vertex2_u_2;
	__u16 vertex2_v_2;
	/* RAWAWB_UV_DETC_VERTEX3_2 */
	__u16 vertex3_u_2;
	__u16 vertex3_v_2;
	/* RAWAWB_UV_DETC_ISLOPE01_2 */
	__u32 islope01_2;
	/* RAWAWB_UV_DETC_ISLOPE12_2 */
	__u32 islope12_2;
	/* RAWAWB_UV_DETC_ISLOPE23_2 */
	__u32 islope23_2;
	/* RAWAWB_UV_DETC_ISLOPE30_2 */
	__u32 islope30_2;
	/* RAWAWB_UV_DETC_VERTEX0_3 */
	__u16 vertex0_u_3;
	__u16 vertex0_v_3;
	/* RAWAWB_UV_DETC_VERTEX1_3 */
	__u16 vertex1_u_3;
	__u16 vertex1_v_3;
	/* RAWAWB_UV_DETC_VERTEX2_3 */
	__u16 vertex2_u_3;
	__u16 vertex2_v_3;
	/* RAWAWB_UV_DETC_VERTEX3_3 */
	__u16 vertex3_u_3;
	__u16 vertex3_v_3;
	/* RAWAWB_UV_DETC_ISLOPE01_3 */
	__u32 islope01_3;
	/* RAWAWB_UV_DETC_ISLOPE12_3 */
	__u32 islope12_3;
	/* RAWAWB_UV_DETC_ISLOPE23_3 */
	__u32 islope23_3;
	/* RAWAWB_UV_DETC_ISLOPE30_3 */
	__u32 islope30_3;
	/* CCM_COEFF0_R */
	__u16 ccm_coeff0_r;
	__u16 ccm_coeff1_r;
	/* CCM_COEFF1_R */
	__u16 ccm_coeff2_r;
	/* CCM_COEFF0_G */
	__u16 ccm_coeff0_g;
	__u16 ccm_coeff1_g;
	/* CCM_COEFF1_G */
	__u16 ccm_coeff2_g;
	/* CCM_COEFF0_B */
	__u16 ccm_coeff0_b;
	__u16 ccm_coeff1_b;
	/* CCM_COEFF1_B */
	__u16 ccm_coeff2_b;
	/* RAWAWB_RGB2XY_WT01 */
	__u16 wt0;
	__u16 wt1;
	/* RAWAWB_RGB2XY_WT2 */
	__u16 wt2;
	/* RAWAWB_RGB2XY0_MAT */
	__u16 mat0_x;
	__u16 mat0_y;
	/* RAWAWB_RGB2XY_MAT1_XY */
	__u16 mat1_x;
	__u16 mat1_y;
	/* RAWAWB_RGB2XY_MAT2_XY */
	__u16 mat2_x;
	__u16 mat2_y;
	/* RAWAWB_XY_DETC_NOR_X_0 */
	__u16 nor_x0_0;
	__u16 nor_x1_0;
	/* RAWAWB_XY_DETC_NOR_Y_0 */
	__u16 nor_y0_0;
	__u16 nor_y1_0;
	/* RAWAWB_XY_DETC_BIG_X_0 */
	__u16 big_x0_0;
	__u16 big_x1_0;
	/* RAWAWB_XY_DETC_BIG_Y_0 */
	__u16 big_y0_0;
	__u16 big_y1_0;
	/* RAWAWB_XY_DETC_NOR_X_1 */
	__u16 nor_x0_1;
	__u16 nor_x1_1;
	/* RAWAWB_XY_DETC_NOR_Y_1 */
	__u16 nor_y0_1;
	__u16 nor_y1_1;
	/* RAWAWB_XY_DETC_BIG_X_1 */
	__u16 big_x0_1;
	__u16 big_x1_1;
	/* RAWAWB_XY_DETC_BIG_Y_1 */
	__u16 big_y0_1;
	__u16 big_y1_1;
	/* RAWAWB_XY_DETC_NOR_X_2 */
	__u16 nor_x0_2;
	__u16 nor_x1_2;
	/* RAWAWB_XY_DETC_NOR_Y_2 */
	__u16 nor_y0_2;
	__u16 nor_y1_2;
	/* RAWAWB_XY_DETC_BIG_X_2 */
	__u16 big_x0_2;
	__u16 big_x1_2;
	/* RAWAWB_XY_DETC_BIG_Y_2 */
	__u16 big_y0_2;
	__u16 big_y1_2;
	/* RAWAWB_XY_DETC_NOR_X_3 */
	__u16 nor_x0_3;
	__u16 nor_x1_3;
	/* RAWAWB_XY_DETC_NOR_Y_3 */
	__u16 nor_y0_3;
	__u16 nor_y1_3;
	/* RAWAWB_XY_DETC_BIG_X_3 */
	__u16 big_x0_3;
	__u16 big_x1_3;
	/* RAWAWB_XY_DETC_BIG_Y_3 */
	__u16 big_y0_3;
	__u16 big_y1_3;
	/* RAWAWB_MULTIWINDOW_EXC_CTRL */
	__u8 exc_wp_region0_excen;
	__u8 exc_wp_region0_measen;
	__u8 exc_wp_region0_domain;
	__u8 exc_wp_region1_excen;
	__u8 exc_wp_region1_measen;
	__u8 exc_wp_region1_domain;
	__u8 exc_wp_region2_excen;
	__u8 exc_wp_region2_measen;
	__u8 exc_wp_region2_domain;
	__u8 exc_wp_region3_excen;
	__u8 exc_wp_region3_measen;
	__u8 exc_wp_region3_domain;
	__u8 exc_wp_region4_excen;
	__u8 exc_wp_region4_domain;
	__u8 exc_wp_region5_excen;
	__u8 exc_wp_region5_domain;
	__u8 exc_wp_region6_excen;
	__u8 exc_wp_region6_domain;
	__u8 multiwindow_en;
	/* RAWAWB_MULTIWINDOW0_OFFS */
	__u16 multiwindow0_h_offs;
	__u16 multiwindow0_v_offs;
	/* RAWAWB_MULTIWINDOW0_SIZE */
	__u16 multiwindow0_h_size;
	__u16 multiwindow0_v_size;
	/* RAWAWB_MULTIWINDOW1_OFFS */
	__u16 multiwindow1_h_offs;
	__u16 multiwindow1_v_offs;
	/* RAWAWB_MULTIWINDOW1_OFFS */
	__u16 multiwindow1_h_size;
	__u16 multiwindow1_v_size;
	/* RAWAWB_MULTIWINDOW2_OFFS */
	__u16 multiwindow2_h_offs;
	__u16 multiwindow2_v_offs;
	/* RAWAWB_MULTIWINDOW2_SIZE */
	__u16 multiwindow2_h_size;
	__u16 multiwindow2_v_size;
	/* RAWAWB_MULTIWINDOW3_OFFS */
	__u16 multiwindow3_h_offs;
	__u16 multiwindow3_v_offs;
	/* RAWAWB_MULTIWINDOW3_SIZE */
	__u16 multiwindow3_h_size;
	__u16 multiwindow3_v_size;
	/* RAWAWB_EXC_WP_REGION0_XU */
	__u16 exc_wp_region0_xu0;
	__u16 exc_wp_region0_xu1;
	/* RAWAWB_EXC_WP_REGION0_YV */
	__u16 exc_wp_region0_yv0;
	__u16 exc_wp_region0_yv1;
	/* RAWAWB_EXC_WP_REGION1_XU */
	__u16 exc_wp_region1_xu0;
	__u16 exc_wp_region1_xu1;
	/* RAWAWB_EXC_WP_REGION1_YV */
	__u16 exc_wp_region1_yv0;
	__u16 exc_wp_region1_yv1;
	/* RAWAWB_EXC_WP_REGION2_XU */
	__u16 exc_wp_region2_xu0;
	__u16 exc_wp_region2_xu1;
	/* RAWAWB_EXC_WP_REGION2_YV */
	__u16 exc_wp_region2_yv0;
	__u16 exc_wp_region2_yv1;
	/* RAWAWB_EXC_WP_REGION3_XU */
	__u16 exc_wp_region3_xu0;
	__u16 exc_wp_region3_xu1;
	/* RAWAWB_EXC_WP_REGION3_YV */
	__u16 exc_wp_region3_yv0;
	__u16 exc_wp_region3_yv1;
	/* RAWAWB_EXC_WP_REGION4_XU */
	__u16 exc_wp_region4_xu0;
	__u16 exc_wp_region4_xu1;
	/* RAWAWB_EXC_WP_REGION4_YV */
	__u16 exc_wp_region4_yv0;
	__u16 exc_wp_region4_yv1;
	/* RAWAWB_EXC_WP_REGION5_XU */
	__u16 exc_wp_region5_xu0;
	__u16 exc_wp_region5_xu1;
	/* RAWAWB_EXC_WP_REGION5_YV */
	__u16 exc_wp_region5_yv0;
	__u16 exc_wp_region5_yv1;
	/* RAWAWB_EXC_WP_REGION6_XU */
	__u16 exc_wp_region6_xu0;
	__u16 exc_wp_region6_xu1;
	/* RAWAWB_EXC_WP_REGION6_YV */
	__u16 exc_wp_region6_yv0;
	__u16 exc_wp_region6_yv1;
	/* RAWAWB_EXC_WP_WEIGHT0_3 */
	__u8 exc_wp_region0_weight;
	__u8 exc_wp_region1_weight;
	__u8 exc_wp_region2_weight;
	__u8 exc_wp_region3_weight;
	/* RAWAWB_EXC_WP_WEIGHT4_6 */
	__u8 exc_wp_region4_weight;
	__u8 exc_wp_region5_weight;
	__u8 exc_wp_region6_weight;
	/* RAWAWB_WRAM_DATA */
	__u8 wp_blk_wei_w[ISP33_RAWAWB_WEIGHT_NUM];

	struct isp2x_bls_fixed_val bls2_val;
} __attribute__ ((packed));

struct isp33_isp_other_cfg {
	struct isp32_bls_cfg bls_cfg;
	struct isp39_dpcc_cfg dpcc_cfg;
	struct isp3x_lsc_cfg lsc_cfg;
	struct isp32_awb_gain_cfg awb_gain_cfg;
	struct isp33_gic_cfg gic_cfg;
	struct isp33_debayer_cfg debayer_cfg;
	struct isp33_ccm_cfg ccm_cfg;
	struct isp3x_gammaout_cfg gammaout_cfg;
	struct isp2x_cproc_cfg cproc_cfg;
	struct isp33_drc_cfg drc_cfg;
	struct isp32_hdrmge_cfg hdrmge_cfg;
	struct isp33_enh_cfg enh_cfg;
	struct isp33_hist_cfg hist_cfg;
	struct isp33_hsv_cfg hsv_cfg;
	struct isp32_ldch_cfg ldch_cfg;
	struct isp33_bay3d_cfg bay3d_cfg;
	struct isp33_ynr_cfg ynr_cfg;
	struct isp33_cnr_cfg cnr_cfg;
	struct isp33_sharp_cfg sharp_cfg;
	struct isp33_cac_cfg cac_cfg;
	struct isp3x_gain_cfg gain_cfg;
	struct isp21_csm_cfg csm_cfg;
	struct isp21_cgc_cfg cgc_cfg;
} __attribute__ ((packed));

struct isp33_isp_meas_cfg {
	struct isp33_rawawb_meas_cfg rawawb;
	struct isp2x_rawaebig_meas_cfg rawae0;
	struct isp2x_rawaebig_meas_cfg rawae3;
	struct isp2x_rawhistbig_cfg rawhist0;
	struct isp2x_rawhistbig_cfg rawhist3;
} __attribute__ ((packed));

struct isp33_isp_params_cfg {
	__u64 module_en_update;
	__u64 module_ens;
	__u64 module_cfg_update;

	__u32 frame_id;
	struct isp33_isp_meas_cfg meas;
	struct isp33_isp_other_cfg others;
} __attribute__ ((packed));

struct rkisp33_thunderboot_resmem_head {
	struct rkisp_thunderboot_resmem_head head;
	struct isp33_isp_params_cfg cfg;
} __attribute__ ((packed));

struct isp33_sharp_stat {
	__u16 noise_curve[ISP33_SHARP_NOISE_CURVE_NUM];
} __attribute__ ((packed));

struct isp33_bay3d_stat {
	__u32 sigma_num;
	__u16 sigma_y[ISP33_BAY3D_TNRSIG_NUM];
} __attribute__ ((packed));

struct isp33_enh_stat {
	__u8 iir[ISP33_ENH_IIR_ROW_MAX][ISP33_ENH_IIR_COL_MAX];
} __attribute__ ((packed));

struct isp33_hist_stat {
	__u16 iir[ISP33_HIST_IIR_BLK_MAX][ISP33_HIST_IIR_NUM];
} __attribute__ ((packed));

struct isp33_rawae_mean_lum {
	__u32 g:12;
	__u32 b:10;
	__u32 r:10;
} __attribute__ ((packed));

struct isp33_rawae_mean_line {
	/* line: 15 word + one unuse */
	struct isp33_rawae_mean_lum blk_x[ISP33_MEAN_BLK_X_NUM];
	__u32 reserved;
} __attribute__ ((packed));

struct isp33_rawae_stat {
	struct isp33_rawae_mean_line blk_y[ISP33_MEAN_BLK_Y_NUM];
	__u32 wnd1_sumg;
	__u32 wnd1_sumb;
	__u32 wnd1_sumr;
	__u32 reserved0;
	__u32 wnd2_sumg;
	__u32 wnd2_sumb;
	__u32 wnd2_sumr;
	__u32 reserved1;
} __attribute__ ((packed));

struct isp33_rawhist_stat {
	__u32 bin[ISP33_HIST_BIN_N_MAX];
} __attribute__ ((packed));

struct isp33_rawawb_mean_ramdata {
	__u64 b:18;
	__u64 g:18;
	__u64 r:18;
	__u64 wp:10;
} __attribute__ ((packed));

struct isp33_rawawb_mean_ramdata_line {
	struct isp33_rawawb_mean_ramdata ramdata_blk_x[ISP33_MEAN_BLK_X_NUM];
	__u32 reserved[2];
} __attribute__ ((packed));

struct isp33_rawawb_mean_sum {
	__u32 rgain_nor;
	__u32 bgain_nor;
	__u32 wp_num_nor;
	__u32 wp_num2;

	__u32 rgain_big;
	__u32 bgain_big;
	__u32 wp_num_big;
	__u32 reserved;
} __attribute__ ((packed));

struct isp33_rawawb_mean_sum_exc {
	__u32 rgain_exc;
	__u32 bgain_exc;
	__u32 wp_num_exc;
	__u32 reserved;
} __attribute__ ((packed));

struct isp33_rawawb_stat {
	struct isp33_rawawb_mean_ramdata_line ramdata_blk_y[ISP33_MEAN_BLK_Y_NUM];
	struct isp33_rawawb_mean_sum sum[ISP33_RAWAWB_SUM_NUM];
	__u16 yhist[ISP33_RAWAWB_HSTBIN_NUM];
	struct isp33_rawawb_mean_sum_exc sum_exc[ISP33_RAWAWB_EXCL_STAT_NUM];
} __attribute__ ((packed));

struct isp33_stat {
	/* mean to ddr */
	struct isp33_rawae_stat rawae3;
	struct isp33_rawhist_stat rawhist3;
	struct isp33_rawae_stat rawae0;
	struct isp33_rawhist_stat rawhist0;
	struct isp33_rawawb_stat rawawb;
	/* ahb read reg */
	struct isp33_bay3d_stat bay3d;
	struct isp33_sharp_stat sharp;
	struct isp33_enh_stat enh;
	struct isp33_hist_stat hist;
	struct isp32_info2ddr_stat info2ddr;
} __attribute__ ((packed));

struct rkisp33_stat_buffer {
	struct isp33_stat stat;
	__u32 meas_type;
	__u32 frame_id;
	__u32 params_id;
} __attribute__ ((packed));
#endif /* _UAPI_RK_ISP33_CONFIG_H */
