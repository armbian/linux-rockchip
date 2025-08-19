/* SPDX-License-Identifier: (GPL-2.0+ WITH Linux-syscall-note) OR MIT
 *
 * Rockchip ISP351s
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_ISP351S_CONFIG_H
#define _UAPI_RK_ISP351S_CONFIG_H

#include <linux/types.h>
#include <linux/v4l2-controls.h>
#include <linux/rk-isp35-config.h>

#define ISP351S_DEBAYER_LUMA_NUM	7
#define ISP351S_DEBAYER_DRCT_OFFSET_NUM	8
#define ISP351S_DEBAYER_VSIGMA_NUM	8
#define ISP351S_DEBAYER_MED_NUM		8

#define ISP351S_GIC_CURVE_NUM		11
#define ISP351S_GIC_THRED_NUM		12
#define ISP351S_GIC_SIGMA_NUM		12

#define ISP351S_ENH_IIR_DATA_MAX	768
#define ISP351S_HIST_IIR_DATA_MAX	1600

struct isp351s_awb_gain_cfg {
	__u32 awb1_gain_gb;
	__u32 awb1_gain_gr;
	__u32 awb1_gain_b;
	__u32 awb1_gain_r;

	__u16 gain0_gb;
	__u16 gain0_gr;
	__u16 gain0_b;
	__u16 gain0_r;

	__u16 gain1_gb;
	__u16 gain1_gr;
	__u16 gain1_b;
	__u16 gain1_r;

	__u16 gain2_gb;
	__u16 gain2_gr;
	__u16 gain2_b;
	__u16 gain2_r;
} __attribute__ ((packed));

struct isp351s_debayer_cfg {
	/* CONTROL */
	__u8 bypass;
	__u8 g_out_flt_en;
	__u8 cnt_flt_en;
	/* LUMA_DX */
	__u8 luma_dx[ISP351S_DEBAYER_LUMA_NUM];
	/* G_INTERP */
	__u8 g_interp_clip_en;
	__u8 g_interp_global_sharp_strg_en;
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
	__u16 drct_offset[ISP351S_DEBAYER_DRCT_OFFSET_NUM];
	/* G_FILTER_MODE_OFFSET */
	__u8 gflt_mode;
	__u16 gflt_ratio;
	__u16 gflt_offset;
	/* G_FILTER_FILTER */
	__s8 gflt_coe0;
	__s8 gflt_coe1;
	__s8 gflt_coe2;
	/* G_FILTER_VSIGMA */
	__u16 gflt_vsigma[ISP351S_DEBAYER_VSIGMA_NUM];
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
	/* C_FILTER_EDGE_1 */
	__u8 cnr_edge_norm_bit;
	/* C_FILTER_MED_THRED */
	__u16 cnr_med_flt_wgt_min_thred[ISP351S_DEBAYER_MED_NUM];
	/* C_FILTER_MED_0 */
	__u16 cnr_med_flt_wgt_slope;
	__u16 cnr_med_flt_range_scale;
	/* C_FILTER_MED_1 */
	__u8 cnr_med_flt_index_min;
	__u8 cnr_med_flt_index_max;
	/* C_FILTER_MED_GAUS */
	__u8 med_wgt_lpf_coeff0;
	__u8 med_wgt_lpf_coeff1;
	__u8 med_wgt_lpf_coeff2;
} __attribute__ ((packed));

struct isp351s_drc_cfg {
	/* DRC_CTRL0 */
	__u8 bypass_en;
	__u8 cmps_byp_en;
	__u8 gainx32_en;
	__u8 bf_lp_en;
	__u8 gain_shift_bit;
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

struct isp351s_gic_cfg {
	/* CTRL */
	__u8 bypass_en;
	__u8 rb_flt_en;
	/* GR_COEF */
	__u8 loflt_gr_coef0;
	__u8 loflt_gr_coef1;
	__u8 loflt_gr_coef2;
	__u8 loflt_gr_coef3;
	/* GB_COEF */
	__u8 loflt_gb_coef0;
	__u8 loflt_gb_coef1;
	/* GAUS_COEF */
	__u8 gaus_coef0;
	__u8 gaus_coef1;
	__u8 gaus_coef2;
	/* BF_COEF */
	__u8 bf_coef0;
	__u8 bf_coef1;
	__u8 bf_coef2;
	/* WGT_SLOPE */
	__u32 fusion_wgt_slope;
	/* ALPHA */
	__u8 gaus_alpha;
	__u8 bf_out_alpha;
	/* BF_PARA0 */
	__u16 bf_wet_min;
	__u16 bf_wet_scale;
	/* BF_PARA1 */
	__u16 r_noise_strg;
	__u16 b_noise_strg;
	/* BF_PARA2 */
	__u16 rat2wgt_min_thed;
	/* CURVE_IDX */
	__u8 curve_idx[ISP351S_GIC_CURVE_NUM];
	/* FUSION_MIN */
	__u32 fusion_min_thred[ISP351S_GIC_THRED_NUM];
	/* SOFT_THED */
	__u32 luma2soft_thred[ISP351S_GIC_THRED_NUM];
	/* R_SIGMA_INV */
	__u32 r_sigma_inv[ISP351S_GIC_SIGMA_NUM];
	/* B_SIGMA_INV */
	__u32 b_sigma_inv[ISP351S_GIC_SIGMA_NUM];
} __attribute__ ((packed));

struct isp351s_ynr_cfg {
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
	/* HI_SIGMA_LIMIT */
	__u8 hi_spnr_filt_noise_limit_en;
	__u16 hi_spnr_filt_noise_min_limit_scale;
	__u16 hi_spnr_filt_noise_max_limit_scale;
	__u16 hi_spnr_filt_diff_offset_scale;
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
	/* MI_EDGE_ADJUST */
	__u8 mi_spnr_filt_edge_adjust_en;
	__u8 mi_spnr_filt_edge_scale;
	__u16 mi_spnr_filt_edge_offset;
	/* MI_TEXT_ADJUST */
	__u8 mi_spnr_tex_adjust_en;
	__u8 mi_spnr_tex_scale;
	__u16 mi_spnr_tex_min_weight;
	__u16 mi_spnr_tex_thred;
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

struct isp351s_hist_cfg {
	/* CTRL */
	__u8 bypass;
	__u8 mem_mode;
	__u8 filter_en;
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
	/* MAP_FLT */
	__u8 flt_offset;
	__u8 flt_sigma;
	/* GAIN*/
	__u8 gain_min;
	__u16 gain_max;
	/* COEF_MAX */
	__u8 coef_max0;
	__u8 coef_max1;
	__u8 coef_max2;
	__u8 coef_max3;
	/* COEF_MIN */
	__u8 coef_min0;
	__u8 coef_min1;
	__u8 coef_min2;
	__u8 coef_min3;
	/* STAB */
	__u8 stab_frame_cnt0;
	__u8 stab_frame_cnt1;
	/* UV_SCL */
	__u8 saturate_scale;
	/* HIST_IIR */
	__u8 iir_wr;
	__u32 iir_data[ISP351S_HIST_IIR_DATA_MAX];
} __attribute__ ((packed));

struct isp351s_enh_cfg {
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
	__u32 iir_data[ISP351S_ENH_IIR_DATA_MAX];
} __attribute__ ((packed));

struct isp351s_bay3d_cfg {
	/* BAY3D_CTRL */
	__u8 bypass_en;
	__u8 soft_mode;
	__u8 out_use_pre_mode;
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
	/* BAY3D_TRANS2 */
	__u16 transf_mode_offset1;
	__u16 transf_mode_ob;
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
	/* B3DLDC_ADR_STS */
	__u8 b3dldch_en;
	__u8 b3dldch_map13p3_en;
	__u8 b3dldch_force_map_en;
	/* B3DLDC_EXTBOUND1 */
	__u8 btnr_ldcltp_mode;
	__u16 btnr_ldc_wrap_ext_bound_offset;
	/* B3DLDC_FFFF_OFF */
	__u16 b3dldc_last;
	/* lut_ldch:offset data_oft; lut_ldcv:offset data1_oft */
	__s32 lut_buf_fd;
} __attribute__ ((packed));

struct isp351s_ai_cfg {
	/* CTRL */
	__u8 aiisp_raw12_msb;
	__u8 aiisp_gain_mode;
	__u8 aiisp_curve_en;
	__u8 aipre_iir_en;
	__u8 aipre_iir2ddr_en;
	__u8 aipre_gain_en;
	__u8 aipre_gain2ddr_en;
	__u8 aipre_yraw_sel;
	__u8 aipre_nl_ddr_mode;
	__u8 aipre_gain_bypass;
	__u8 aipre_gain_mode;
	__u8 aipre_narmap_inv;
	__u8 aipre_luma2gain_dis;
	__u8 aipre_input_sel;
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
} __attribute__ ((packed));

struct isp351s_aiawb_meas_cfg {
	__u8 bls3_en;
	/* CTRL0 */
	__u8 ds_mode_config_en;
	__u8 expand_ovf_clip_en;
	__u8 rgb2w_mode;
	__u8 rawout_sel;
	__u8 path_sel;
	__u8 in_shift;
	__u8 ds_mode;
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

struct isp351s_rawae_meas_cfg {
	__u8 rawae_sel;
	__u8 bnr2ae_sel;
	__u8 drc2aebig_sel;

	__u8 wnd_num;
	__u8 wnd1_en;
	__u8 debug_en;
	__u8 bnr_be_sel;
	__u8 wb1_sel_mode;

	__u16 win0_h_offset;
	__u16 win0_v_offset;
	__u16 win0_h_size;
	__u16 win0_v_size;
	__u16 win1_h_offset;
	__u16 win1_v_offset;
	__u16 win1_h_size;
	__u16 win1_v_size;
} __attribute__ ((packed));

struct isp351s_isp_other_cfg {
	struct isp35_bls_cfg bls_cfg;
	struct isp39_dpcc_cfg dpcc_cfg;
	struct isp35_hdrmge_cfg hdrmge_cfg;
	struct isp3x_gain_cfg gain_cfg;
	struct isp33_cac_cfg cac_cfg;
	struct isp3x_lsc_cfg lsc_cfg;
	struct isp33_ccm_cfg ccm_cfg;
	struct isp3x_gammaout_cfg gammaout_cfg;
	struct isp35_hsv_cfg hsv_cfg;
	struct isp21_csm_cfg csm_cfg;
	struct isp35_cnr_cfg cnr_cfg;
	struct isp35_sharp_cfg sharp_cfg;
	struct isp2x_cproc_cfg cproc_cfg;
	struct isp39_ldch_cfg ldch_cfg;
	struct isp39_ldcv_cfg ldcv_cfg;
	struct isp21_cgc_cfg cgc_cfg;

	struct isp351s_awb_gain_cfg awb_gain_cfg;
	struct isp351s_bay3d_cfg bay3d_cfg;
	struct isp351s_ai_cfg ai_cfg;
	struct isp351s_debayer_cfg debayer_cfg;
	struct isp351s_drc_cfg drc_cfg;
	struct isp351s_gic_cfg gic_cfg;
	struct isp351s_ynr_cfg ynr_cfg;
	struct isp351s_enh_cfg enh_cfg;
	struct isp351s_hist_cfg hist_cfg;
} __attribute__ ((packed));

struct isp351s_isp_meas_cfg {
	struct isp351s_rawae_meas_cfg rawae0;
	struct isp35_rawhist_meas_cfg rawhist0;
	struct isp351s_rawae_meas_cfg rawae3;
	struct isp35_rawhist_meas_cfg rawhist3;
	struct isp35_rawawb_meas_cfg rawawb;
	struct isp35_rawaf_meas_cfg rawaf;
	struct isp35_awbsync_meas_cfg awbsync;

	struct isp351s_aiawb_meas_cfg aiawb;
} __attribute__ ((packed));

struct isp351s_isp_params_cfg {
	__u64 module_en_update;
	__u64 module_ens;
	__u64 module_cfg_update;

	__u32 frame_id;
	struct isp351s_isp_meas_cfg meas;
	struct isp351s_isp_other_cfg others;
	struct sensor_exposure_cfg exposure;
} __attribute__ ((packed));

struct isp351s_enh_stat {
	__u32 iir_data[ISP351S_ENH_IIR_DATA_MAX];
} __attribute__ ((packed));

struct isp351s_hist_stat {
	__u32 iir_data[ISP351S_HIST_IIR_DATA_MAX];
} __attribute__ ((packed));

struct isp351s_stat {
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
	struct isp351s_enh_stat enh;
	struct isp351s_hist_stat hist;
	struct isp35_awbsync_stat awbsync;
	struct isp32_info2ddr_stat info2ddr;

	int buf_aiawb_index;
	int buf_bay3d_iir_index;
	int buf_bay3d_ds_index;
	int buf_bay3d_wgt_index;
	int buf_gain_index;
	int buf_aipre_index;
} __attribute__ ((packed));

struct rkisp351s_stat_buffer {
	struct isp351s_stat stat;
	__u32 meas_type;
	__u32 frame_id;
	__u32 params_id;
} __attribute__ ((packed));
#endif /* _UAPI_RK_ISP351S_CONFIG_H */
