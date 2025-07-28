/* SPDX-License-Identifier: (GPL-2.0+ WITH Linux-syscall-note) OR MIT
 *
 * Rockchip ISP35
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_ISP35_CONFIG_H
#define _UAPI_RK_ISP35_CONFIG_H

#include <linux/types.h>
#include <linux/v4l2-controls.h>
#include <linux/rk-isp33-config.h>

#define RKISP_CMD_GET_PARAMS_V35 \
	_IOR('V', BASE_VIDIOC_PRIVATE + 119, struct isp35_isp_params_cfg)

#define ISP35_MODULE_DPCC		ISP3X_MODULE_DPCC
#define ISP35_MODULE_BLS		ISP3X_MODULE_BLS
#define ISP35_MODULE_SDG		ISP3X_MODULE_SDG
#define ISP35_MODULE_LSC		ISP3X_MODULE_LSC
#define ISP35_MODULE_AWB_GAIN		ISP3X_MODULE_AWB_GAIN
#define ISP35_MODULE_BDM		ISP3X_MODULE_BDM
#define ISP35_MODULE_CCM		ISP3X_MODULE_CCM
#define ISP35_MODULE_GOC		ISP3X_MODULE_GOC
#define ISP35_MODULE_CPROC		ISP3X_MODULE_CPROC
#define ISP35_MODULE_IE			ISP3X_MODULE_IE
#define ISP35_MODULE_RAWAF		ISP3X_MODULE_RAWAF
#define ISP35_MODULE_RAWAE0		ISP3X_MODULE_RAWAE0
#define ISP35_MODULE_RAWAE1		ISP3X_MODULE_RAWAE1
#define ISP35_MODULE_RAWAE2		ISP3X_MODULE_RAWAE2
#define ISP35_MODULE_RAWAE3		ISP3X_MODULE_RAWAE3
#define ISP35_MODULE_RAWAWB		ISP3X_MODULE_RAWAWB
#define ISP35_MODULE_RAWHIST0		ISP3X_MODULE_RAWHIST0
#define ISP35_MODULE_RAWHIST1		ISP3X_MODULE_RAWHIST1
#define ISP35_MODULE_RAWHIST2		ISP3X_MODULE_RAWHIST2
#define ISP35_MODULE_RAWHIST3		ISP3X_MODULE_RAWHIST3
#define ISP35_MODULE_HDRMGE		ISP3X_MODULE_HDRMGE
#define ISP35_MODULE_RAWNR		ISP3X_MODULE_RAWNR
#define ISP35_MODULE_GIC		ISP3X_MODULE_GIC
#define ISP35_MODULE_DHAZ		ISP3X_MODULE_DHAZ
#define ISP35_MODULE_3DLUT		ISP3X_MODULE_3DLUT
#define ISP35_MODULE_LDCH		ISP3X_MODULE_LDCH
#define ISP35_MODULE_GAIN		ISP3X_MODULE_GAIN
#define ISP35_MODULE_DEBAYER		ISP3X_MODULE_DEBAYER
#define ISP35_MODULE_BAYNR		ISP3X_MODULE_BAYNR
#define ISP35_MODULE_BAY3D		ISP3X_MODULE_BAY3D
#define ISP35_MODULE_YNR		ISP3X_MODULE_YNR
#define ISP35_MODULE_CNR		ISP3X_MODULE_CNR
#define ISP35_MODULE_SHARP		ISP3X_MODULE_SHARP
#define ISP35_MODULE_DRC		ISP3X_MODULE_DRC
#define ISP35_MODULE_CAC		ISP3X_MODULE_CAC
#define ISP35_MODULE_CSM		ISP3X_MODULE_CSM
#define ISP35_MODULE_CGC		ISP3X_MODULE_CGC
#define ISP35_MODULE_RGBIR		ISP39_MODULE_RGBIR
#define ISP35_MODULE_HSV		ISP33_MODULE_HSV
#define ISP35_MODULE_ENH		ISP33_MODULE_ENH
#define ISP35_MODULE_HIST		ISP33_MODULE_HIST
#define ISP35_MODULE_AI			BIT_ULL(51)
#define ISP35_MODULE_AIAWB		BIT_ULL(52)
#define ISP35_MODULE_AWBSYNC		BIT_ULL(53)

#define ISP35_MODULE_FORCE		ISP3X_MODULE_FORCE

/* Measurement types */
#define ISP35_STAT_RAWAWB		ISP3X_STAT_RAWAWB
#define ISP35_STAT_RAWAF		ISP3X_STAT_RAWAF
#define ISP35_STAT_RAWAE0		ISP3X_STAT_RAWAE0
#define ISP35_STAT_RAWAE3		ISP3X_STAT_RAWAE3
#define ISP35_STAT_RAWHST0		ISP3X_STAT_RAWHST0
#define ISP35_STAT_RAWHST3		ISP3X_STAT_RAWHST3
#define ISP35_STAT_INFO2DDR		ISP33_STAT_INFO2DDR
#define ISP35_STAT_BAY3D		ISP33_STAT_BAY3D
#define ISP35_STAT_ENH			ISP33_STAT_ENH
#define ISP35_STAT_HIST			ISP33_STAT_HIST
#define ISP35_STAT_SHARP		ISP33_STAT_SHARP
#define ISP35_STAT_AIAWB		BIT(24)
#define ISP35_STAT_AWBSYNC		BIT(25)
#define ISP35_STAT_RTT_FST		ISP33_STAT_RTT_FST

#define ISP35_MESH_BUF_NUM		ISP3X_MESH_BUF_NUM

#define ISP35_LSC_GRAD_TBL_SIZE		ISP3X_LSC_GRAD_TBL_SIZE
#define ISP35_LSC_SIZE_TBL_SIZE		ISP3X_LSC_SIZE_TBL_SIZE
#define ISP35_LSC_DATA_TBL_SIZE		ISP3X_LSC_DATA_TBL_SIZE

#define ISP35_DEGAMMA_CURVE_SIZE	ISP3X_DEGAMMA_CURVE_SIZE

#define ISP35_GAIN_IDX_NUM		ISP3X_GAIN_IDX_NUM
#define ISP35_GAIN_LUT_NUM		ISP3X_GAIN_LUT_NUM

#define ISP35_RAWAWB_WEIGHT_NUM		ISP3X_RAWAWB_WEIGHT_NUM
#define ISP35_RAWAWB_HSTBIN_NUM		ISP3X_RAWAWB_HSTBIN_NUM
#define ISP35_RAWAWB_SUM_NUM		4
#define ISP35_RAWAWB_EXCL_STAT_NUM	4

#define	ISP35_RAWAEBIG_SUBWIN_NUM	2

#define ISP35_RAWHIST_WEIGHT_NUM	225

#define ISP35_RAWAF_CURVE_NUM		ISP3X_RAWAF_CURVE_NUM
#define ISP35_RAWAF_HIIR_COE_NUM	ISP3X_RAWAF_HIIR_COE_NUM
#define ISP35_RAWAF_VFIR_COE_NUM	ISP3X_RAWAF_VFIR_COE_NUM
#define ISP35_RAWAF_WIN_NUM		ISP3X_RAWAF_WIN_NUM
#define ISP35_RAWAF_LINE_NUM		ISP3X_RAWAF_LINE_NUM
#define ISP35_RAWAF_GAMMA_NUM		ISP3X_RAWAF_GAMMA_NUM
#define ISP35_RAWAF_SUMDATA_NUM		ISP3X_RAWAF_SUMDATA_NUM
#define ISP35_RAWAF_VIIR_COE_NUM	3
#define ISP35_RAWAF_GAUS_COE_NUM	9

#define ISP35_DPCC_PDAF_POINT_NUM	ISP3X_DPCC_PDAF_POINT_NUM

#define ISP35_HDRMGE_WGT_NUM		17

#define ISP35_GIC_SIGMA_Y_NUM		ISP33_GIC_SIGMA_Y_NUM
#define ISP35_GIC_LUMA_DX_NUM		ISP33_GIC_LUMA_DX_NUM
#define ISP35_GIC_THRED_Y_NUM		ISP33_GIC_THRED_Y_NUM

#define ISP35_CCM_CURVE_NUM		18
#define ISP35_CCM_HF_FACTOR_NUM		17

#define ISP35_HSV_1DLUT_NUM		ISP33_HSV_1DLUT_NUM
#define ISP35_HSV_2DLUT_ROW		ISP33_HSV_2DLUT_ROW
#define ISP35_HSV_2DLUT_COL		ISP33_HSV_2DLUT_COL

#define ISP35_LDCH_BIC_NUM		ISP33_LDCH_BIC_NUM

#define ISP35_GAMMA_OUT_MAX_SAMPLES     ISP3X_GAMMA_OUT_MAX_SAMPLES

#define ISP35_ENH_LUMA_NUM		ISP33_ENH_LUMA_NUM
#define ISP35_ENH_DETAIL_NUM		ISP33_ENH_DETAIL_NUM
#define ISP35_ENH_IIR_ROW_MAX		24
#define ISP35_ENH_IIR_COL_MAX		32

#define ISP35_HIST_ALPHA_NUM		ISP33_HIST_ALPHA_NUM
#define ISP35_HIST_THUMB_ROW_MAX	ISP33_HIST_THUMB_ROW_MAX
#define ISP35_HIST_THUMB_COL_MAX	ISP33_HIST_THUMB_COL_MAX
#define ISP35_HIST_IIR_NUM		ISP33_HIST_IIR_NUM
#define ISP35_HIST_IIR_BLK_MAX		ISP33_HIST_IIR_BLK_MAX

#define ISP35_DRC_Y_NUM			ISP3X_DRC_Y_NUM

#define ISP35_CNR_SIGMA_Y_NUM		ISP3X_CNR_SIGMA_Y_NUM
#define ISP35_CNR_GAUS_COE_NUM		6
#define ISP35_CNR_GAUS_SIGMAR_NUM	8
#define ISP35_CNR_WGT_SIGMA_Y_NUM	13
#define ISP35_CNR_CURVE_NUM		10

#define ISP35_YNR_XY_NUM		ISP3X_YNR_XY_NUM
#define ISP35_YNR_HI_GAUS1_COE_NUM	ISP33_YNR_HI_GAUS1_COE_NUM
#define ISP35_YNR_ADJ_NUM		ISP33_YNR_ADJ_NUM
#define ISP35_YNR_HI_GAUS_COE_NUM	4
#define ISP35_YNR_TEX2WGT_NUM		9

#define ISP35_BAY3D_XY_NUM		16
#define ISP35_BAY3D_TNRSIG_NUM		20
#define ISP35_BAY3D_LPF_COEFF_NUM	9
#define ISP35_BAY3D_FILT_COEFF_NUM	6

#define ISP35_AI_SIGMA_NUM		33
#define ISP35_VPSL_SIGMA_NUM		81

#define ISP35_SHARP_X_NUM		ISP33_SHARP_X_NUM
#define ISP35_SHARP_Y_NUM		ISP33_SHARP_Y_NUM
#define ISP35_SHARP_KERNEL_NUM		ISP33_SHARP_KERNEL_NUM
#define ISP35_SHARP_EDGE_KERNEL_NUM	ISP33_SHARP_EDGE_KERNEL_NUM
#define ISP35_SHARP_EDGE_WGT_NUM	ISP33_SHARP_EDGE_WGT_NUM
#define ISP35_SHARP_LUMA_STRG_NUM	ISP33_SHARP_LUMA_STRG_NUM
#define ISP35_SHARP_CONTRAST_STRG_NUM	ISP33_SHARP_CONTRAST_STRG_NUM
#define ISP35_SHARP_TEX_CLIP_NUM	ISP33_SHARP_TEX_CLIP_NUM
#define ISP35_SHARP_LUM_CLIP_NUM	ISP33_SHARP_LUM_CLIP_NUM
#define ISP35_SHARP_HUE_NUM		ISP33_SHARP_HUE_NUM
#define ISP35_SHARP_DISATANCE_NUM	ISP33_SHARP_DISATANCE_NUM
#define ISP35_SHARP_TEX_NUM		ISP33_SHARP_HITEX_NUM
#define ISP35_SHARP_NOISE_CURVE_NUM	ISP33_SHARP_NOISE_CURVE_NUM
#define ISP35_SHARP_SATURATION_NUM	9
#define ISP35_SHARP_LOCAL_STRG_NUM	4

#define ISP35_CAC_PSF_NUM		ISP33_CAC_PSF_NUM

#define ISP35_CSM_COEFF_NUM		ISP3X_CSM_COEFF_NUM

#define ISP35_DEBAYER_LUMA_NUM		7
#define ISP35_DEBAYER_DRCT_OFFSET_NUM	8
#define ISP35_DEBAYER_VSIGMA_NUM	8

#define ISP35_RGBIR_SCALE_NUM		4
#define ISP35_RGBIR_LUMA_POINT_NUM	17
#define ISP35_RGBIR_SCALE_MAP_NUM	17

#define ISP35_MEAN_BLK_X_NUM		15
#define ISP35_MEAN_BLK_Y_NUM		15

#define ISP35_AIAWB_FLT_COE_NUM		5

#define ISP35_AWBSYNC_WIN_MAX		3

struct isp35_bls_cfg {
	__u8 enable_auto;
	__u8 en_windows;
	__u8 bls1_en;

	__u8 bls_samples;

	struct isp2x_window bls_window1;
	struct isp2x_window bls_window2;
	struct isp2x_bls_fixed_val fixed_val;
	struct isp2x_bls_fixed_val bls1_val;

	__u16 isp_ob_offset;
	__u16 isp_ob_offset1;
	__u16 isp_ob_predgain;
	__u32 isp_ob_max;
} __attribute__ ((packed));

struct isp35_hdrmge_cfg {
	/* CTRL */
	__u8 short_base_en;
	__u8 frame_mode;
	__u8 dbg_mode;
	__u8 channel_detection_en;
	__u8 s_base_mode;
	/* GAIN0 */
	__u16 short_gain;
	__u16 short_inv_gain;
	/* GAIN1 */
	__u16 medium_gain;
	__u16 medium_inv_gain;
	/* GAIN2 */
	__u8 long_gain;
	/* LIGHTZ */
	__u8 ms_diff_scale;
	__u8 ms_diff_offset;
	__u8 lm_diff_scale;
	__u8 lm_diff_offset;
	/* MS_DIFF */
	__u16 ms_abs_diff_scale;
	__u16 ms_abs_diff_thred_min_limit;
	__u16 ms_adb_diff_thred_max_limit;
	/* LM_DIFF */
	__u16 lm_abs_diff_scale;
	__u16 lm_abs_diff_thred_min_limit;
	__u16 lm_abs_diff_thred_max_limit;
	/* DIFF_Y */
	__u16 ms_luma_diff2wgt[ISP35_HDRMGE_WGT_NUM];
	__u16 lm_luma_diff2wgt[ISP35_HDRMGE_WGT_NUM];
	/* OVER_Y */
	__u16 luma2wgt[ISP35_HDRMGE_WGT_NUM];
	__u16 ms_raw_diff2wgt[ISP35_HDRMGE_WGT_NUM];
	__u16 lm_raw_diff2wgt[ISP35_HDRMGE_WGT_NUM];
	/* EACH_GAIN */
	__u16 channel_detn_short_gain;
	__u16 channel_detn_medium_gain;
	/* FORCE_LONG0 */
	__u16 mid_luma_scale;
	/* FORCE_LONG1 */
	__u16 mid_luma_thred_max_limit;
	__u16 mid_luma_thred_min_limit;
} __attribute__ ((packed));

struct isp35_hsv_cfg {
	__u8 hsv_1dlut0_en;
	__u8 hsv_1dlut1_en;
	__u8 hsv_2dlut0_en;
	__u8 hsv_2dlut1_en;
	__u8 hsv_2dlut2_en;
	__u8 hsv_2dlut12_cfg;

	__u8 hsv_1dlut0_idx_mode;
	__u8 hsv_1dlut1_idx_mode;
	__u8 hsv_2dlut0_idx_mode;
	__u8 hsv_2dlut1_idx_mode;
	__u8 hsv_2dlut2_idx_mode;
	__u8 hsv_1dlut0_item_mode;
	__u8 hsv_1dlut1_item_mode;
	__u8 hsv_2dlut0_item_mode;
	__u8 hsv_2dlut1_item_mode;
	__u8 hsv_2dlut2_item_mode;

	__u16 lut0_1d[ISP35_HSV_1DLUT_NUM];
	__u16 lut1_1d[ISP35_HSV_1DLUT_NUM];
	__u16 lut0_2d[ISP35_HSV_2DLUT_ROW][ISP35_HSV_2DLUT_COL];
	__u16 lut1_2d[ISP35_HSV_2DLUT_ROW][ISP35_HSV_2DLUT_COL];
	__u16 lut2_2d[ISP35_HSV_2DLUT_ROW][ISP35_HSV_2DLUT_COL];
} __attribute__ ((packed));

struct isp35_debayer_cfg {
	/* CONTROL */
	__u8 bypass;
	__u8 g_out_flt_en;
	__u8 cnt_flt_en;
	/* LUMA_DX */
	__u8 luma_dx[ISP35_DEBAYER_LUMA_NUM];
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
	__u16 drct_offset[ISP35_DEBAYER_DRCT_OFFSET_NUM];
	/* G_FILTER_MODE_OFFSET */
	__u8 gflt_mode;
	__u16 gflt_ratio;
	__u16 gflt_offset;
	/* G_FILTER_FILTER */
	__s8 gflt_coe0;
	__s8 gflt_coe1;
	__s8 gflt_coe2;
	/* G_FILTER_VSIGMA */
	__u16 gflt_vsigma[ISP35_DEBAYER_VSIGMA_NUM];
	/* C_FILTER_GUIDE_GAUS */
	__u8 cnr_lo_guide_lpf_coe0;
	__u8 cnr_lo_guide_lpf_coe1;
	__u8 cnr_lo_guide_lpf_coe2;
	/* C_FILTER_CE_GAUS */
	__u8 cnr_pre_flt_coe0;
	__u8 cnr_pre_flt_coe1;
	__u8 cnr_pre_flt_coe2;
	/* C_FILTER_ALPHA_GAUS */
	__u8 cnr_alpha_lpf_coe0;
	__u8 cnr_alpha_lpf_coe1;
	__u8 cnr_alpha_lpf_coe2;
	/* C_FILTER_LOG_OFFSET */
	__u16 cnr_log_grad_offset;
	__u16 cnr_log_guide_offset;
	__u8 cnr_trans_en;
	/* C_FILTER_ALPHA */
	__u16 cnr_moire_alpha_offset;
	__u32 cnr_moire_alpha_scale;
	/* C_FILTER_EDGE */
	__u16 cnr_edge_alpha_offset;
	__u32 cnr_edge_alpha_scale;
	/* C_FILTER_IIR_0 */
	__u8 cnr_lo_flt_strg_inv;
	__u8 cnr_lo_flt_strg_shift;
	__u16 cnr_lo_flt_wgt_slope;
	/* C_FILTER_IIR_1 */
	__u8 cnr_lo_flt_wgt_max_limit;
	__u8 cnr_lo_flt_wgt_min_thred;
	/* C_FILTER_BF */
	__u16 cnr_hi_flt_vsigma;
	__u8 cnr_hi_flt_wgt_min_limit;
	__u8 cnr_hi_flt_cur_wgt;
} __attribute__ ((packed));

struct isp35_bay3d_cfg {
	/* BAY3D_CTRL */
	__u8 bypass_en;
	__u8 iir_wr_src;
	__u8 out_use_pre_mode;
	__u8 motion_est_en;
	__u8 iir_rw_fmt;
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
	__u8 pre_spnr_dpc_flt_en;
	__u8 pre_spnr_dpc_nr_bal_mode;
	__u8 pre_spnr_dpc_flt_mode;
	__u8 pre_spnr_dpc_flt_prewgt_en;
	/* BAY3D_CTRL3 */
	__u8 transf_mode;
	__u8 wgt_cal_mode;
	__u8 mge_wgt_ds_mode;
	__u8 kalman_wgt_ds_mode;
	__u8 mge_wgt_hdr_sht_thred;
	__u8 sigma_calc_mge_wgt_hdr_sht_thred;
	/* BAY3D_TRANS0 */
	__u16 transf_mode_offset;
	__u8 transf_mode_scale;
	__u16 itransf_mode_offset;
	/* BAY3D_TRANS1 */
	__u32 transf_data_max_limit;
	/* BAY3D_PREHI_SIGSCL */
	__u16 pre_spnr_sigma_ctrl_scale;
	/* BAY3D_PREHI_SIGOF */
	__u8 pre_spnr_hi_guide_out_wgt;
	/* BAY3D_CURHISPW */
	__u8 cur_spnr_filter_coeff[ISP35_BAY3D_FILT_COEFF_NUM];
	/* BAY3D_IIRSX */
	__u16 pre_spnr_luma2sigma_x[ISP35_BAY3D_XY_NUM];
	/* BAY3D_IIRSY */
	__u16 pre_spnr_luma2sigma_y[ISP35_BAY3D_XY_NUM];
	/* BAY3D_PREHI_SIGSCL */
	__u16 pre_spnr_hi_sigma_scale;
	/* BAY3D_PREHI_WSCL */
	__u8 pre_spnr_hi_wgt_calc_scale;
	/* BAY3D_PREHIWMM */
	__u8 pre_spnr_hi_filter_wgt_min_limit;
	__u8 pre_spnr_hi_wgt_calc_offset;
	/* BAY3D_PREHISIGOF */
	__u8 pre_spnr_hi_filter_out_wgt;
	__u8 pre_spnr_sigma_offset;
	__u8 pre_spnr_sigma_hdr_sht_offset;
	/* BAY3D_PREHISIGSCL */
	__u16 pre_spnr_sigma_scale;
	__u16 pre_spnr_sigma_hdr_sht_scale;
	/* BAY3D_PREHISPW */
	__u8 pre_spnr_hi_filter_coeff[ISP35_BAY3D_FILT_COEFF_NUM];
	/* BAY3D_PRELOSIGCSL */
	__u16 pre_spnr_lo_sigma_scale;
	/* BAY3D_PRELOSIGOF */
	__u8 pre_spnr_lo_wgt_calc_offset;
	__u8 pre_spnr_lo_wgt_calc_scale;
	/* BAY3D_PREHI_NRCT */
	__u16 pre_spnr_hi_noise_ctrl_scale;
	__u8 pre_spnr_hi_noise_ctrl_offset;
	/* BAY3D_TNRSX */
	__u16 tnr_luma2sigma_x[ISP35_BAY3D_TNRSIG_NUM];
	/* BAY3D_TNRSY */
	__u16 tnr_luma2sigma_y[ISP35_BAY3D_TNRSIG_NUM];
	/* BAY3D_HIWD */
	__u16 lpf_hi_coeff[ISP35_BAY3D_LPF_COEFF_NUM];
	/* BAY3D_LOWD */
	__u16 lpf_lo_coeff[ISP35_BAY3D_LPF_COEFF_NUM];
	/* BAY3D_GF */
	__u8 sigma_idx_filt_coeff[ISP35_BAY3D_FILT_COEFF_NUM];
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
	__u16 out_use_hi_noise_bal_nr_strg;
	__u16 out_use_md_noise_bal_nr_strg;
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
	/* BAY3D_LOCOEF0 */
	__u8 lo_wgt_hflt_coeff2;
	__u8 lo_wgt_hflt_coeff1;
	__u8 lo_wgt_hflt_coeff0;
	__u8 sig_hflt_coeff2;
	__u8 sig_hflt_coeff1;
	__u8 sig_hflt_coeff0;
	/* BAY3D_LOCOEF1 */
	__u8 lo_dif_hflt_coeff2;
	__u8 lo_dif_hflt_coeff1;
	__u8 lo_dif_hflt_coeff0;
	/* BAY3D_DPC0 */
	__u8 pre_spnr_dpc_bright_str;
	__u8 pre_spnr_dpc_dark_str;
	__u8 pre_spnr_dpc_str;
	__u8 pre_spnr_dpc_wk_scale;
	__u8 pre_spnr_dpc_wk_offset;
	/* BAY3D_DPC1 */
	__u16 pre_spnr_dpc_nr_bal_str;
	__u16 pre_spnr_dpc_soft_thr_scale;
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
	/* BAY3D_MONROFF */
	__u16 out_use_hi_noise_bal_nr_off;
	__u16 out_use_md_noise_bal_nr_off;

	/* B3DLDC_CTRL */
	__u8 btnr_ldc_en;
	__u8 b3dldcv_map13p3_en;
	__u8 b3dldcv_force_map_en;
	/* B3DLDC_ADR_STS */
	__u8 b3dldch_en;
	__u8 b3dldch_map13p3_en;
	__u8 b3dldch_force_map_en;
	/* B3DLDC_EXTBOUND1 */
	__u8 btnr_ldcltp_mode;
	__u16 btnr_ldc_wrap_ext_bound_offset;
	/* lut_ldch:offset data_oft; lut_ldcv:offset data1_oft */
	__s32 lut_buf_fd;
} __attribute__ ((packed));

struct isp35_ai_cfg {
	/* CTRL */
	__u8 aiisp_raw12_msb;
	__u8 aiisp_gain_mode;
	__u8 aiisp_curve_en;
	__u8 aipre_iir_en;
	__u8 aipre_iir2ddr_en;
	__u8 aipre_gain_en;
	__u8 aipre_gain2ddr_en;
	__u8 aipre_luma2gain_dis;
	__u8 aipre_nl_ddr_mode;
	__u8 aipre_yraw_sel;
	__u8 aipre_gain_bypass;
	__u8 aipre_gain_mode;
	__u8 aipre_narmap_inv;
	/* SIGMA_Y */
	__u16 aiisp_sigma_y[ISP35_AI_SIGMA_NUM];
	/* AIPRE_NL_PRE */
	__u8 aipre_scale;
	__s8 aipre_zp;
	__u16 aipre_black_lvl;
	/* AIPRE_GAIN_PARA */
	__u8 aipre_gain_alpha;
	__u8 aipre_global_gain;
	__u16 aipre_gain_ratio;
	/* AIPRE_SIGMA_CURVE */
	__u16 aipre_sigma_y[ISP35_AI_SIGMA_NUM];
	/* AIPRE_NOISE0 */
	__u8 aipre_noise_mot_offset;
	__s8 aipre_noise_mot_gain;
	__u16 aipre_noise_luma_offset;
	/* AIPRE_NOISE1 */
	__u16 aipre_noise_luma_gain;
	__u16 aipre_noise_luma_clip;
	__u8 aipre_noise_luma_static;
	/* AIPRE_NOISE2 */
	__u8 aipre_nar_manual;
	__u8 aipre_nar_manual_alpha;

	/* VPSL_PYR_CTRL */
	__u8 pyr_yraw_mode;
	__u8 pyr_sigma_en;
	__u8 pyr_yraw_sel;
	__u8 pyr_gain_leftshift;
	__u8 pyr_blacklvl_sig;
	/* VPSL_PYR_SIGMA_LUT */
	__u8 pyr_sigma_y[ISP35_VPSL_SIGMA_NUM];
} __attribute__ ((packed));

struct isp35_ynr_cfg {
	/* GLOBAL_CTRL */
	__u8 hi_spnr_bypass;
	__u8 mi_spnr_bypass;
	__u8 lo_spnr_bypass;
	__u8 rnr_en;
	__u8 tex2lo_strg_en;
	__u8 hi_lp_en;
	__u8 dsfilt_bypass;
	__u8 tex2wgt_en;
	/* GAIN_CTRL */
	__u16 global_set_gain;
	__u8 gain_merge_alpha;
	__u8 local_gain_scale;
	/* GAIN_ADJ */
	__u16 lo_spnr_gain2strg[ISP35_YNR_ADJ_NUM];
	/* RNR_MAX_R */
	__u16 rnr_max_radius;
	/* RNR_CENTER_COOR */
	__u16 rnr_center_h;
	__u16 rnr_center_v;
	/* RNR_STRENGTH */
	__u8 radius2strg[ISP35_YNR_XY_NUM];
	/* SGM_DX */
	__u16 luma2sima_x[ISP35_YNR_XY_NUM];
	/* SGM_Y */
	__u16 luma2sima_y[ISP35_YNR_XY_NUM];
	/* MI_TEX2WGT_SCALE */
	__u8 mi_spnr_tex2wgt_scale[ISP35_YNR_TEX2WGT_NUM];
	/* LO_TEX2WGT_SCALE */
	__u8 lo_spnr_tex2wgt_scale[ISP35_YNR_TEX2WGT_NUM];
	/* HI_SIGMA_GAIN */
	__u16 hi_spnr_sigma_min_limit;
	__u8 hi_spnr_local_gain_alpha;
	__u16 hi_spnr_strg;
	/* HI_GAUS_COE */
	__u8 hi_spnr_filt_coeff[ISP35_YNR_HI_GAUS_COE_NUM];
	/* HI_WEIGHT */
	__u16 hi_spnr_filt_wgt_offset;
	__u16 hi_spnr_filt_center_wgt;
	/* HI_GAUS1_COE */
	__u16 hi_spnr_filt1_coeff[ISP35_YNR_HI_GAUS1_COE_NUM];
	/* HI_TEXT */
	__u16 hi_spnr_filt1_tex_thred;
	__u16 hi_spnr_filt1_tex_scale;
	__u16 hi_spnr_filt1_wgt_alpha;
	/* MI_GAUS_COE */
	__u8 mi_spnr_filt_coeff0;
	__u8 mi_spnr_filt_coeff1;
	__u8 mi_spnr_filt_coeff2;
	__u8 mi_spnr_filt_coeff3;
	__u8 mi_spnr_filt_coeff4;
	/* MI_STRG_DETAIL */
	__u16 mi_spnr_strg;
	__u16 mi_spnr_soft_thred_scale;
	/* MI_WEIGHT */
	__u8 mi_spnr_wgt;
	__u8 mi_ehance_scale_en;
	__u8 mi_ehance_scale;
	__u16 mi_spnr_filt_center_wgt;
	/* DSIIR_COE */
	__u16 dsfilt_diff_offset;
	__u16 dsfilt_center_wgt;
	__u16 dsfilt_strg;
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
	__u8 lo_enhance_scale;
	/* LO_TEXT_THRED */
	__u16 tex2lo_strg_upper_thred;
	__u16 tex2lo_strg_lower_thred;
	/* FUSION_WEIT_ADJ */
	__u8 lo_gain2wgt[ISP35_YNR_ADJ_NUM];
} __attribute__ ((packed));

struct isp35_cnr_cfg {
	/* CNR_CTRL */
	__u8 exgain_bypass;
	__u8 yuv422_mode;
	__u8 thumb_mode;
	__u8 uv_dis;
	__u8 hiflt_wgt0_mode;
	__u8 local_alpha_dis;
	__u8 loflt_coeff;
	__u8 hsv_alpha_en;
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
	__u8 gaus_flt_coeff[ISP35_CNR_GAUS_COE_NUM];
	/* CNR_GAUS_RATIO */
	__u16 gaus_flt_alpha;
	__u8 hiflt_wgt_min_limit;
	__u16 hiflt_alpha;
	/* CNR_BF_PARA1 */
	__u8 hiflt_uv_gain;
	__u16 hiflt_global_vsigma;
	__u8 hiflt_cur_wgt;
	/* CNR_BF_PARA2 */
	__u16 adj_offset;
	__u16 adj_scale;
	/* CNR_SIGMA */
	__u8 sgm_ratio[ISP35_CNR_SIGMA_Y_NUM];
	__u16 bf_merge_max_limit;
	/* CNR_IIR_GLOBAL_GAIN */
	__u8 loflt_global_sgm_ratio;
	__u8 loflt_global_sgm_ratio_alpha;
	__u16 bf_alpha_max_limit;
	/* CNR_WGT_SIGMA */
	__u8 cur_wgt[ISP35_CNR_WGT_SIGMA_Y_NUM];
	/* GAUS_X_SIGMAR */
	__u16 hiflt_vsigma_idx[ISP35_CNR_GAUS_SIGMAR_NUM];
	/* GAUS_Y_SIGMAR */
	__u16 hiflt_vsigma[ISP35_CNR_GAUS_SIGMAR_NUM];
	/* IIR_SIGMAR */
	__u8 lo_flt_vsigma[ISP35_CNR_WGT_SIGMA_Y_NUM];
	/* HSV_CURVE */
	__u8 hsv_adj_alpha_table[ISP35_CNR_CURVE_NUM];
	/* SAT_CURVE */
	__u8 sat_adj_alpha_table[ISP35_CNR_CURVE_NUM];
	/* GAIN_ADJ_CURVE */
	__u8 gain_adj_alpha_table[ISP35_CNR_CURVE_NUM];
} __attribute__ ((packed));

struct isp35_sharp_cfg {
	/* ctrl */
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
	__u8 img_hpf_coeff[ISP35_SHARP_KERNEL_NUM];
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
	/* LUMA_DX */
	__u8 luma_dx[ISP35_SHARP_X_NUM];
	/* PBF_VSIGMA */
	__u16 pre_bifilt_vsigma_inv[ISP35_SHARP_Y_NUM];
	/* PBF_KERNEL */
	__u8 pre_bifilt_coeff0;
	__u8 pre_bifilt_coeff1;
	__u8 pre_bifilt_coeff2;
	/* DETAIL_KERNEL */
	__u8 hi_detail_lpf_coeff[ISP35_SHARP_KERNEL_NUM];
	__u8 mi_detail_lpf_coeff[ISP35_SHARP_KERNEL_NUM];
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
	__u8 edge_bpf_coeff[ISP35_SHARP_EDGE_KERNEL_NUM];
	/* EDGE_WGT_VAL */
	__u16 edge_wgt_val[ISP35_SHARP_EDGE_WGT_NUM];
	/* LUMA_ADJ_STRG */
	__u8 luma2strg[ISP35_SHARP_LUMA_STRG_NUM];
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
	__u16 tex2loss_tex_in_hinr_strg[ISP35_SHARP_LOCAL_STRG_NUM];
	/* DETAIL_SCALE_TAB */
	__u8 contrast2pos_strg[ISP35_SHARP_CONTRAST_STRG_NUM];
	__u8 contrast2neg_strg[ISP35_SHARP_CONTRAST_STRG_NUM];
	__u8 pos_detail_strg;
	__u8 neg_detail_strg;
	/* DETAIL_TEX_CLIP */
	__u16 tex2detail_pos_clip[ISP35_SHARP_TEX_CLIP_NUM];
	__u16 tex2detail_neg_clip[ISP35_SHARP_TEX_CLIP_NUM];
	/* GRAIN_TEX_CLIP */
	__u16 tex2grain_pos_clip[ISP35_SHARP_TEX_CLIP_NUM];
	__u16 tex2grain_neg_clip[ISP35_SHARP_TEX_CLIP_NUM];
	/* DETAIL_LUMA_CLIP */
	__u16 luma2detail_pos_clip[ISP35_SHARP_LUM_CLIP_NUM];
	__u16 luma2detail_neg_clip[ISP35_SHARP_LUM_CLIP_NUM];
	/* GRAIN_STRG */
	__u8 grain_strg;
	/* HUE_ADJ_TAB */
	__u16 hue2strg[ISP35_SHARP_HUE_NUM];
	/* DISATANCE_ADJ */
	__u8 distance2strg[ISP35_SHARP_DISATANCE_NUM];
	/* TEX2DETAIL_STRG */
	__u16 tex2detail_strg[ISP35_SHARP_TEX_NUM];
	/* NOISE_SIGMA */
	__u16 hi_tex_threshold[ISP35_SHARP_TEX_NUM];
	/* TEX2MFDETAIL_STRG */
	__u16 tex2mf_detail_strg[ISP35_SHARP_TEX_NUM];
	/* LOSSTEXINHINR_STRG */
	__u8 loss_tex_in_hinr_strg;
	/* NOISE_CURVE */
	__u16 noise_curve_ext[ISP35_SHARP_NOISE_CURVE_NUM];
	__u8 noise_count_thred_ratio;
	__u8 noise_clip_scale;
	/* NOISE_CLIP */
	__u16 noise_clip_min_limit;
	__u16 noise_clip_max_limit;
	/* EDGEWGTFLT_KERNEL */
	__u8 edge_wgt_flt_coeff0;
	__u8 edge_wgt_flt_coeff1;
	__u8 edge_wgt_flt_coeff2;
	/* EDGE_GLOBAL_CLIP */
	__u16 edge_glb_clip_thred;
	__u16 pos_edge_clip;
	__u16 neg_edge_clip;
	/* MFDETAIL */
	__u8 mf_detail_data_alpha;
	__u8 pos_mf_detail_strg;
	__u8 neg_mf_detail_strg;
	/* MFDETAIL_CLIP */
	__u16 mf_detail_pos_clip;
	__u16 sharp_mf_detail_neg_clip;
	/* SATURATION_STRG */
	__u8 staturation2strg[ISP35_SHARP_SATURATION_NUM];
	__u16 lo_saturation_strg;
} __attribute__ ((packed));

struct isp35_enh_cfg {
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
	__u16 lum2strg[ISP35_ENH_LUMA_NUM];
	/* DETAIL_IDX */
	__u16 detail2strg_idx[ISP35_ENH_DETAIL_NUM];
	/* DETAIL_POWER */
	__u8 detail2strg_power0;
	__u8 detail2strg_power1;
	__u8 detail2strg_power2;
	__u8 detail2strg_power3;
	__u8 detail2strg_power4;
	__u8 detail2strg_power5;
	__u8 detail2strg_power6;
	/* DETAIL_VALUE */
	__u16 detail2strg_val[ISP35_ENH_DETAIL_NUM];
	/* PRE_FRAME */
	__u8 pre_wet_frame_cnt0;
	__u8 pre_wet_frame_cnt1;
	/* IIR */
	__u8 iir_wr;
	__u8 iir[ISP35_ENH_IIR_ROW_MAX][ISP35_ENH_IIR_COL_MAX];
} __attribute__ ((packed));

struct isp35_drc_cfg {
	/* DRC_CTRL0 */
	__u8 bypass_en;
	__u8 cmps_byp_en;
	__u8 gainx32_en;
	/* DRC_CTRL1 */
	__u16 gain_idx_luma_scale;
	__u16 comps_idx_luma_scale;
	__u8 log_transform_offset_bits;
	/* DRC_LPRATIO */
	__u16 lo_detail_ratio;
	__u16 hi_detail_ratio;
	__u8 adj_gain_idx_luma_scale;
	/* DRC_BILAT0 */
	__u8 bifilt_wgt_offset;
	__u16 thumb_thred_neg;
	__u8 thumb_thred_en;
	__u8 bifilt_cur_pixel_wgt;
	/* DRC_BILAT1 */
	__u8 cmps_offset_bits;
	__u8 cmps_mode;
	__u16 filt_luma_soft_thred;
	/* DRC_BILAT2 */
	__u16 thumb_max_limit;
	__u8 thumb_scale;
	/* DRC_BILAT3 */
	__u16 hi_range_inv_sigma;
	__u16 lo_range_inv_sigma;
	/* DRC_BILAT4 */
	__u8 bifilt_wgt;
	__u8 bifilt_hi_wgt;
	__u16 bifilt_soft_thred;
	__u8 bifilt_soft_thred_en;
	/* DRC_GAIN_Y */
	__u16 gain_y[ISP35_DRC_Y_NUM];
	/* DRC_COMPRES_Y */
	__u16 compres_y[ISP35_DRC_Y_NUM];
	/* DRC_SCALE_Y */
	__u16 scale_y[ISP35_DRC_Y_NUM];
	/* IIRWG_GAIN */
	__u16 comps_gain_min_limit;
	/* SFTHD_Y */
	__u16 sfthd_y[ISP35_DRC_Y_NUM];
	/* LUMA_MIX */
	__u8 max_luma_wgt;
	__u8 mid_luma_wgt;
	__u8 min_luma_wgt;
} __attribute__ ((packed));

struct isp35_rawawb_meas_cfg {
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
	__u8 bnr_be_sel;
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
	/* RAWAWB_YUV_RGB2ROTY_0 */
	__u16 rgb2ryuvmat0_y;
	__u16 rgb2ryuvmat1_y;
	/* RAWAWB_YUV_RGB2ROTY_1 */
	__u16 rgb2ryuvmat2_y;
	__u16 rgb2ryuvofs_y;
	/* RAWAWB_YUV_RGB2ROTU_0 */
	__u16 rgb2ryuvmat0_u;
	__u16 rgb2ryuvmat1_u;
	/* RAWAWB_YUV_RGB2ROTU_1 */
	__u16 rgb2ryuvmat2_u;
	__u16 rgb2ryuvofs_u;
	/* RAWAWB_YUV_RGB2ROTV_0 */
	__u16 rgb2ryuvmat0_v;
	__u16 rgb2ryuvmat1_v;
	/* RAWAWB_YUV_RGB2ROTV_1 */
	__u16 rgb2ryuvmat2_v;
	__u16 rgb2ryuvofs_v;
	/* RAWAWB_YUV_X_COOR_Y_0 */
	__u16 coor_x1_ls0_y;
	__u16 vec_x21_ls0_y;
	/* RAWAWB_YUV_X_COOR_U_0 */
	__u16 coor_x1_ls0_u;
	__u16 vec_x21_ls0_u;
	/* RAWAWB_YUV_X_COOR_V_0 */
	__u16 coor_x1_ls0_v;
	__u16 vec_x21_ls0_v;
	/* RAWAWB_YUV_X1X2_DIS_0 */
	__u8 dis_x1x2_ls0;
	__u8 rotu0_ls0;
	__u8 rotu1_ls0;
	/* RAWAWB_YUV_INTERP_CURVE_UCOOR_0 */
	__u8 rotu2_ls0;
	__u8 rotu3_ls0;
	__u8 rotu4_ls0;
	__u8 rotu5_ls0;
	/* RAWAWB_YUV_INTERP_CURVE_TH0_0 */
	__u16 th0_ls0;
	__u16 th1_ls0;
	/* RAWAWB_YUV_INTERP_CURVE_TH1_0 */
	__u16 th2_ls0;
	__u16 th3_ls0;
	/* RAWAWB_YUV_INTERP_CURVE_TH2_0 */
	__u16 th4_ls0;
	__u16 th5_ls0;
	/* RAWAWB_YUV_X_COOR_Y_1 */
	__u16 coor_x1_ls1_y;
	__u16 vec_x21_ls1_y;
	/* RAWAWB_YUV_X_COOR_U_1 */
	__u16 coor_x1_ls1_u;
	__u16 vec_x21_ls1_u;
	/* RAWAWB_YUV_X_COOR_V_1 */
	__u16 coor_x1_ls1_v;
	__u16 vec_x21_ls1_v;
	/* RAWAWB_YUV_X1X2_DIS_1 */
	__u8 dis_x1x2_ls1;
	__u8 rotu0_ls1;
	__u8 rotu1_ls1;
	/* YUV_INTERP_CURVE_UCOOR_1 */
	__u8 rotu2_ls1;
	__u8 rotu3_ls1;
	__u8 rotu4_ls1;
	__u8 rotu5_ls1;
	/* RAWAWB_YUV_INTERP_CURVE_TH0_1 */
	__u16 th0_ls1;
	__u16 th1_ls1;
	/* RAWAWB_YUV_INTERP_CURVE_TH1_1 */
	__u16 th2_ls1;
	__u16 th3_ls1;
	/* RAWAWB_YUV_INTERP_CURVE_TH2_1 */
	__u16 th4_ls1;
	__u16 th5_ls1;
	/* RAWAWB_YUV_X_COOR_Y_2 */
	__u16 coor_x1_ls2_y;
	__u16 vec_x21_ls2_y;
	/* RAWAWB_YUV_X_COOR_U_2 */
	__u16 coor_x1_ls2_u;
	__u16 vec_x21_ls2_u;
	/* RAWAWB_YUV_X_COOR_V_2 */
	__u16 coor_x1_ls2_v;
	__u16 vec_x21_ls2_v;
	/* RAWAWB_YUV_X1X2_DIS_2 */
	__u8 dis_x1x2_ls2;
	__u8 rotu0_ls2;
	__u8 rotu1_ls2;
	/* YUV_INTERP_CURVE_UCOOR_2 */
	__u8 rotu2_ls2;
	__u8 rotu3_ls2;
	__u8 rotu4_ls2;
	__u8 rotu5_ls2;
	/* RAWAWB_YUV_INTERP_CURVE_TH0_2 */
	__u16 th0_ls2;
	__u16 th1_ls2;
	/* RAWAWB_YUV_INTERP_CURVE_TH1_2 */
	__u16 th2_ls2;
	__u16 th3_ls2;
	/* RAWAWB_YUV_INTERP_CURVE_TH2_2 */
	__u16 th4_ls2;
	__u16 th5_ls2;
	/* RAWAWB_YUV_X_COOR_Y_3 */
	__u16 coor_x1_ls3_y;
	__u16 vec_x21_ls3_y;
	/* RAWAWB_YUV_X_COOR_U_3 */
	__u16 coor_x1_ls3_u;
	__u16 vec_x21_ls3_u;
	/* RAWAWB_YUV_X_COOR_V_3 */
	__u16 coor_x1_ls3_v;
	__u16 vec_x21_ls3_v;
	/* RAWAWB_YUV_X1X2_DIS_3 */
	__u8 dis_x1x2_ls3;
	__u8 rotu0_ls3;
	__u8 rotu1_ls3;
	/* RAWAWB_YUV_INTERP_CURVE_UCOOR_3 */
	__u8 rotu2_ls3;
	__u8 rotu3_ls3;
	__u8 rotu4_ls3;
	__u8 rotu5_ls3;
	/* RAWAWB_YUV_INTERP_CURVE_TH0_3 */
	__u16 th0_ls3;
	__u16 th1_ls3;
	/* RAWAWB_YUV_INTERP_CURVE_TH1_3 */
	__u16 th2_ls3;
	__u16 th3_ls3;
	/* RAWAWB_YUV_INTERP_CURVE_TH2_3 */
	__u16 th4_ls3;
	__u16 th5_ls3;
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
	__u8 wp_blk_wei_w[ISP35_RAWAWB_WEIGHT_NUM];

	struct isp2x_bls_fixed_val bls2_val;
} __attribute__ ((packed));

struct isp35_aiawb_meas_cfg {
	__u8 bls3_en;
	/* CTRL0 */
	__u8 ds_mode_config_en;
	__u8 ds_mode;
	__u8 rgb2w_mode;
	__u8 rawout_sel;
	__u8 path_sel;
	__u8 in_shift;
	/* CTRL1 */
	__u8 exp1_check_en;
	__u8 exp_thr;
	__u16 saturation_hthr;
	__u16 saturation_lthr;
	/* WIN_OFFS */
	__u16 h_offs;
	__u16 v_offs;
	/* WIN_SIZE */
	__u16 h_size;
	__u16 v_size;
	/* FLT_COE */
	__s8 flt_coe[ISP35_AIAWB_FLT_COE_NUM];
	/* WBGAIN_INV0 */
	__u16 wbgain_inv_g;
	__u16 wbgain_inv_b;
	/* WBGAIN_INV1 */
	__u16 wbgain_inv_r;
	__u16 expand;
	/* MATRIX_SCALE */
	__u16 ms00;
	__u16 ms01;
	/* MATRIX_ROT0 */
	__u16 mr00;
	__u16 mr01;
	/* MATRIX_ROT1 */
	__u16 mr10;
	__u16 mr11;

	struct isp2x_bls_fixed_val bls3_val;
} __attribute__ ((packed));

struct isp35_awbsync_meas_cfg {
	/* CTRL */
	__u8 sumval_check_en;
	__u8 sumval_mode;
	/* SCL */
	__u16 scl_b;
	__u16 scl_g;
	__u16 scl_r;
	/* SUMVAL_MIN */
	__u16 sumval_minb;
	__u16 sumval_ming;
	__u16 sumval_minr;
	/* SUMVAL_MAX */
	__u16 sumval_maxb;
	__u16 sumval_maxg;
	__u16 sumval_maxr;
	/* WINDOW0_OFFS */
	__u16 win0_h_offs;
	__u16 win0_v_offs;
	/* WINDOW0_RD_COOR */
	__u16 win0_r_coor;
	__u16 win0_d_coor;
	/* WINDOW1_OFFS */
	__u16 win1_h_offs;
	__u16 win1_v_offs;
	/* WINDOW1_RD_COOR */
	__u16 win1_r_coor;
	__u16 win1_d_coor;
	/* WINDOW2_OFFS */
	__u16 win2_h_offs;
	__u16 win2_v_offs;
	/* WINDOW2_RD_COOR */
	__u16 win2_r_coor;
	__u16 win2_d_coor;
} __attribute__ ((packed));

struct isp35_rawaf_meas_cfg {
	__u8 rawaf_sel;
	__u8 num_afm_win;
	__u8 bnr2af_sel;

	/* CTRL */
	__u8 gamma_en;
	__u8 gaus_en;
	__u8 hiir_en;
	__u8 viir_en;
	__u8 ldg_en;
	__u8 h1_fv_mode;
	__u8 h2_fv_mode;
	__u8 v1_fv_mode;
	__u8 v2_fv_mode;
	__u8 ae_mode;
	__u8 y_mode;
	__u8 vldg_sel;
	__u8 v_dnscl_mode;
	__u8 from_ynr;
	__u8 bnr_be_sel;
	__u8 hiir_left_border_mode;
	__u8 avg_ds_en;
	__u8 avg_ds_mode;
	__u8 h1_acc_mode;
	__u8 h2_acc_mode;
	__u8 v1_acc_mode;
	__u8 v2_acc_mode;

	/* WINA_B */
	struct isp2x_window win[ISP39_RAWAF_WIN_NUM];

	/* CTRL1 */
	__s16 bls_offset;
	__u8 bls_en;
	__u8 aehgl_en;
	__u8 hldg_dilate_num;
	__u8 tnrin_shift;

	/* HVIIR_VAR_SHIFT */
	__u8 h1iir_shift_wina;
	__u8 h2iir_shift_wina;
	__u8 v1iir_shift_wina;
	__u8 v2iir_shift_wina;
	__u8 h1iir_shift_winb;
	__u8 h2iir_shift_winb;
	__u8 v1iir_shift_winb;
	__u8 v2iir_shift_winb;

	/* GAUS_COE */
	__s8 gaus_coe[ISP39_RAWAF_GAUS_COE_NUM];

	/* GAMMA_Y */
	__u16 gamma_y[ISP39_RAWAF_GAMMA_NUM];
	/* HIIR_THRESH */
	__u16 h_fv_thresh;
	__u16 v_fv_thresh;
	struct isp3x_rawaf_curve curve_h[ISP39_RAWAF_CURVE_NUM];
	struct isp3x_rawaf_curve curve_v[ISP39_RAWAF_CURVE_NUM];
	__s16 h1iir1_coe[ISP39_RAWAF_HIIR_COE_NUM];
	__s16 h1iir2_coe[ISP39_RAWAF_HIIR_COE_NUM];
	__s16 h2iir1_coe[ISP39_RAWAF_HIIR_COE_NUM];
	__s16 h2iir2_coe[ISP39_RAWAF_HIIR_COE_NUM];
	__s16 v1iir_coe[ISP39_RAWAF_VIIR_COE_NUM];
	__s16 v2iir_coe[ISP39_RAWAF_VIIR_COE_NUM];
	__s16 v1fir_coe[ISP39_RAWAF_VFIR_COE_NUM];
	__s16 v2fir_coe[ISP39_RAWAF_VFIR_COE_NUM];
	/* HIGHLIT_THRESH */
	__u16 highlit_thresh;

	/* CORING_H */
	__u16 h_fv_limit;
	__u16 h_fv_slope;
	/* CORING_V */
	__u16 v_fv_limit;
	__u16 v_fv_slope;
} __attribute__ ((packed));

struct isp35_rawae_meas_cfg {
	__u8 rawae_sel;
	__u8 bnr2ae_sel;

	__u8 wnd_num;
	__u8 wnd1_en;
	__u8 debug_en;
	__u8 bnr_be_sel;

	__u16 win0_h_offset;
	__u16 win0_v_offset;
	__u16 win0_h_size;
	__u16 win0_v_size;
	__u16 win1_h_offset;
	__u16 win1_v_offset;
	__u16 win1_h_size;
	__u16 win1_v_size;
} __attribute__ ((packed));

struct isp35_rawhist_meas_cfg {
	__u8 stepsize;
	__u8 debug_en;
	__u8 mode;
	__u8 data_sel;
	__u8 wnd_num;
	__u16 waterline;

	__u8 rcc;
	__u8 gcc;
	__u8 bcc;
	__u8 off;

	__u16 h_offset;
	__u16 v_offset;
	__u16 h_size;
	__u16 v_size;

	__u8 weight[ISP35_RAWHIST_WEIGHT_NUM];
} __attribute__ ((packed));

struct isp35_isp_other_cfg {
	struct isp39_rgbir_cfg rgbir_cfg;
	struct isp35_bls_cfg bls_cfg;
	struct isp32_awb_gain_cfg awb_gain_cfg;
	struct isp39_dpcc_cfg dpcc_cfg;
	struct isp35_hdrmge_cfg hdrmge_cfg;
	struct isp3x_gain_cfg gain_cfg;
	struct isp35_bay3d_cfg bay3d_cfg;
	struct isp35_ai_cfg ai_cfg;

	struct isp33_cac_cfg cac_cfg;
	struct isp3x_lsc_cfg lsc_cfg;

	struct isp35_debayer_cfg debayer_cfg;
	struct isp35_drc_cfg drc_cfg;
	struct isp33_ccm_cfg ccm_cfg;
	struct isp3x_gammaout_cfg gammaout_cfg;
	struct isp35_hsv_cfg hsv_cfg;
	struct isp21_csm_cfg csm_cfg;
	struct isp33_gic_cfg gic_cfg;
	struct isp35_cnr_cfg cnr_cfg;
	struct isp35_ynr_cfg ynr_cfg;
	struct isp35_sharp_cfg sharp_cfg;
	struct isp35_enh_cfg enh_cfg;
	struct isp33_hist_cfg hist_cfg;
	struct isp32_ldch_cfg ldch_cfg;
	struct isp21_cgc_cfg cgc_cfg;
	struct isp2x_cproc_cfg cproc_cfg;
} __attribute__ ((packed));

struct isp35_isp_meas_cfg {
	struct isp35_rawae_meas_cfg rawae0;
	struct isp35_rawhist_meas_cfg rawhist0;
	struct isp35_rawae_meas_cfg rawae3;
	struct isp35_rawhist_meas_cfg rawhist3;
	struct isp35_rawawb_meas_cfg rawawb;
	struct isp35_rawaf_meas_cfg rawaf;
	struct isp35_aiawb_meas_cfg aiawb;
	struct isp35_awbsync_meas_cfg awbsync;
} __attribute__ ((packed));

struct isp35_isp_params_cfg {
	__u64 module_en_update;
	__u64 module_ens;
	__u64 module_cfg_update;

	__u32 frame_id;
	struct isp35_isp_meas_cfg meas;
	struct isp35_isp_other_cfg others;
	struct sensor_exposure_cfg exposure;
} __attribute__ ((packed));

struct isp35_awbsync_stat {
	__u64 sumr[ISP35_AWBSYNC_WIN_MAX];
	__u64 sumg[ISP35_AWBSYNC_WIN_MAX];
	__u64 sumb[ISP35_AWBSYNC_WIN_MAX];
	__u64 sump[ISP35_AWBSYNC_WIN_MAX];
} __attribute__ ((packed));

struct isp35_enh_stat {
	__u8 iir[ISP35_ENH_IIR_ROW_MAX][ISP35_ENH_IIR_COL_MAX];
} __attribute__ ((packed));

struct isp35_stat {
	/* mean to ddr */
	struct isp33_rawae_stat rawae3;
	struct isp33_rawhist_stat rawhist3;
	struct isp33_rawae_stat rawae0;
	struct isp33_rawhist_stat rawhist0;
	struct isp39_rawaf_stat rawaf;
	struct isp33_rawawb_stat rawawb;
	/* ahb read reg */
	struct isp33_bay3d_stat bay3d;
	struct isp33_sharp_stat sharp;
	struct isp35_enh_stat enh;
	struct isp33_hist_stat hist;
	struct isp35_awbsync_stat awbsync;
	struct isp32_info2ddr_stat info2ddr;

	int buf_aiawb_index;
	int buf_bay3d_iir_index;
	int buf_bay3d_ds_index;
	int buf_bay3d_wgt_index;
	int buf_gain_index;
	int buf_aipre_gain_index;
	int buf_vpsl_index;
} __attribute__ ((packed));

struct rkisp35_stat_buffer {
	struct isp35_stat stat;
	__u32 meas_type;
	__u32 frame_id;
	__u32 params_id;
} __attribute__ ((packed));
#endif /* _UAPI_RK_ISP35_CONFIG_H */
