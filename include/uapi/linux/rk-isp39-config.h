/* SPDX-License-Identifier: (GPL-2.0+ WITH Linux-syscall-note) OR MIT
 *
 * Rockchip ISP39
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_ISP39_CONFIG_H
#define _UAPI_RK_ISP39_CONFIG_H

#include <linux/types.h>
#include <linux/v4l2-controls.h>
#include <linux/rk-isp32-config.h>

#define RKISP_CMD_GET_PARAMS_V39 \
	_IOR('V', BASE_VIDIOC_PRIVATE + 115, struct isp39_isp_params_cfg)

#define ISP39_MODULE_DPCC		ISP3X_MODULE_DPCC
#define ISP39_MODULE_BLS		ISP3X_MODULE_BLS
#define ISP39_MODULE_SDG		ISP3X_MODULE_SDG
#define ISP39_MODULE_LSC		ISP3X_MODULE_LSC
#define ISP39_MODULE_AWB_GAIN		ISP3X_MODULE_AWB_GAIN
#define ISP39_MODULE_BDM		ISP3X_MODULE_BDM
#define ISP39_MODULE_CCM		ISP3X_MODULE_CCM
#define ISP39_MODULE_GOC		ISP3X_MODULE_GOC
#define ISP39_MODULE_CPROC		ISP3X_MODULE_CPROC
#define ISP39_MODULE_IE			ISP3X_MODULE_IE
#define ISP39_MODULE_RAWAF		ISP3X_MODULE_RAWAF
#define ISP39_MODULE_RAWAE0		ISP3X_MODULE_RAWAE0
#define ISP39_MODULE_RAWAE1		ISP3X_MODULE_RAWAE1
#define ISP39_MODULE_RAWAE2		ISP3X_MODULE_RAWAE2
#define ISP39_MODULE_RAWAE3		ISP3X_MODULE_RAWAE3
#define ISP39_MODULE_RAWAWB		ISP3X_MODULE_RAWAWB
#define ISP39_MODULE_RAWHIST0		ISP3X_MODULE_RAWHIST0
#define ISP39_MODULE_RAWHIST1		ISP3X_MODULE_RAWHIST1
#define ISP39_MODULE_RAWHIST2		ISP3X_MODULE_RAWHIST2
#define ISP39_MODULE_RAWHIST3		ISP3X_MODULE_RAWHIST3
#define ISP39_MODULE_HDRMGE		ISP3X_MODULE_HDRMGE
#define ISP39_MODULE_RAWNR		ISP3X_MODULE_RAWNR
#define ISP39_MODULE_GIC		ISP3X_MODULE_GIC
#define ISP39_MODULE_DHAZ		ISP3X_MODULE_DHAZ
#define ISP39_MODULE_3DLUT		ISP3X_MODULE_3DLUT
#define ISP39_MODULE_LDCH		ISP3X_MODULE_LDCH
#define ISP39_MODULE_GAIN		ISP3X_MODULE_GAIN
#define ISP39_MODULE_DEBAYER		ISP3X_MODULE_DEBAYER
#define ISP39_MODULE_BAYNR		ISP3X_MODULE_BAYNR
#define ISP39_MODULE_BAY3D		ISP3X_MODULE_BAY3D
#define ISP39_MODULE_YNR		ISP3X_MODULE_YNR
#define ISP39_MODULE_CNR		ISP3X_MODULE_CNR
#define ISP39_MODULE_SHARP		ISP3X_MODULE_SHARP
#define ISP39_MODULE_DRC		ISP3X_MODULE_DRC
#define ISP39_MODULE_CAC		ISP3X_MODULE_CAC
#define ISP39_MODULE_CSM		ISP3X_MODULE_CSM
#define ISP39_MODULE_CGC		ISP3X_MODULE_CGC
#define ISP39_MODULE_LDCV		BIT_ULL(45)
#define ISP39_MODULE_YUVME		BIT_ULL(46)
#define ISP39_MODULE_RGBIR		BIT_ULL(47)

#define ISP39_MODULE_FORCE		ISP3X_MODULE_FORCE

/* Measurement types */
#define ISP39_STAT_RAWAWB		ISP3X_STAT_RAWAWB
#define ISP39_STAT_RAWAF		ISP3X_STAT_RAWAF
#define ISP39_STAT_RAWAE0		ISP3X_STAT_RAWAE0
#define ISP39_STAT_RAWAE1		ISP3X_STAT_RAWAE1
#define ISP39_STAT_RAWAE2		ISP3X_STAT_RAWAE2
#define ISP39_STAT_RAWAE3		ISP3X_STAT_RAWAE3
#define ISP39_STAT_RAWHST0		ISP3X_STAT_RAWHST0
#define ISP39_STAT_RAWHST1		ISP3X_STAT_RAWHST1
#define ISP39_STAT_RAWHST2		ISP3X_STAT_RAWHST2
#define ISP39_STAT_RAWHST3		ISP3X_STAT_RAWHST3
#define ISP39_STAT_BLS			ISP3X_STAT_BLS
#define ISP39_STAT_DHAZ			ISP3X_STAT_DHAZ
#define ISP39_STAT_INFO2DDR		BIT(19)
#define ISP39_STAT_BAY3D		BIT(20)

#define ISP39_MESH_BUF_NUM		ISP3X_MESH_BUF_NUM

#define ISP39_LSC_GRAD_TBL_SIZE		ISP3X_LSC_GRAD_TBL_SIZE
#define ISP39_LSC_SIZE_TBL_SIZE		ISP3X_LSC_SIZE_TBL_SIZE
#define ISP39_LSC_DATA_TBL_SIZE		ISP3X_LSC_DATA_TBL_SIZE

#define ISP39_DEGAMMA_CURVE_SIZE	ISP3X_DEGAMMA_CURVE_SIZE

#define ISP39_GAIN_IDX_NUM		ISP3X_GAIN_IDX_NUM
#define ISP39_GAIN_LUT_NUM		ISP3X_GAIN_LUT_NUM

#define ISP39_RAWAWB_WEIGHT_NUM		ISP3X_RAWAWB_WEIGHT_NUM
#define ISP39_RAWAWB_HSTBIN_NUM		ISP3X_RAWAWB_HSTBIN_NUM
#define ISP39_RAWAWB_SUM_NUM		4
#define ISP39_RAWAWB_EXCL_STAT_NUM	4

#define	ISP39_RAWAEBIG_SUBWIN_NUM	2

#define ISP39_RAWHISTBIG_SUBWIN_NUM	ISP3X_RAWHISTBIG_SUBWIN_NUM
#define ISP39_RAWHISTLITE_SUBWIN_NUM	ISP3X_RAWHISTLITE_SUBWIN_NUM
#define ISP39_HIST_BIN_N_MAX		ISP3X_HIST_BIN_N_MAX

#define ISP39_RAWAF_CURVE_NUM		ISP3X_RAWAF_CURVE_NUM
#define ISP39_RAWAF_HIIR_COE_NUM	ISP3X_RAWAF_HIIR_COE_NUM
#define ISP39_RAWAF_VFIR_COE_NUM	ISP3X_RAWAF_VFIR_COE_NUM
#define ISP39_RAWAF_WIN_NUM		ISP3X_RAWAF_WIN_NUM
#define ISP39_RAWAF_LINE_NUM		ISP3X_RAWAF_LINE_NUM
#define ISP39_RAWAF_GAMMA_NUM		ISP3X_RAWAF_GAMMA_NUM
#define ISP39_RAWAF_SUMDATA_NUM		ISP3X_RAWAF_SUMDATA_NUM
#define ISP39_RAWAF_VIIR_COE_NUM	3
#define ISP39_RAWAF_GAUS_COE_NUM	9

#define ISP39_DPCC_PDAF_POINT_NUM	ISP3X_DPCC_PDAF_POINT_NUM

#define ISP39_HDRMGE_L_CURVE_NUM	ISP3X_HDRMGE_L_CURVE_NUM
#define ISP39_HDRMGE_E_CURVE_NUM	ISP3X_HDRMGE_E_CURVE_NUM

#define ISP39_GIC_SIGMA_Y_NUM		ISP3X_GIC_SIGMA_Y_NUM

#define ISP39_CCM_CURVE_NUM		18
#define ISP39_CCM_HF_FACTOR_NUM		17

#define ISP39_3DLUT_DATA_NUM		ISP3X_3DLUT_DATA_NUM

#define ISP39_LDCH_BIC_NUM		36
#define ISP39_LDCV_BIC_NUM		36

#define ISP39_GAMMA_OUT_MAX_SAMPLES     ISP3X_GAMMA_OUT_MAX_SAMPLES

#define ISP39_DHAZ_SIGMA_IDX_NUM	ISP3X_DHAZ_SIGMA_IDX_NUM
#define ISP39_DHAZ_SIGMA_LUT_NUM	ISP3X_DHAZ_SIGMA_LUT_NUM
#define ISP39_DHAZ_HIST_WR_NUM		ISP3X_DHAZ_HIST_WR_NUM
#define ISP39_DHAZ_ENH_CURVE_NUM	ISP3X_DHAZ_ENH_CURVE_NUM
#define ISP39_DHAZ_ENH_LUMA_NUM		17
#define ISP39_DHAZ_BLEND_WET_NUM	17
#define ISP39_DHAZ_THUMB_ROW_MAX	8
#define ISP39_DHAZ_THUMB_COL_MAX	10
#define ISP39_DHAZ_HIST_IIR_NUM		16
#define ISP39_DHAZ_HIST_IIR_BLK_MAX	(ISP39_DHAZ_THUMB_ROW_MAX * ISP39_DHAZ_THUMB_COL_MAX)

#define ISP39_DRC_Y_NUM			ISP3X_DRC_Y_NUM

#define ISP39_CNR_SIGMA_Y_NUM		ISP3X_CNR_SIGMA_Y_NUM
#define ISP39_CNR_GAUS_COE_NUM		6
#define ISP39_CNR_GAUS_SIGMAR_NUM	8
#define ISP39_CNR_WGT_SIGMA_Y_NUM	13

#define ISP39_YNR_LOW_GAIN_ADJ_NUM	9
#define ISP39_YNR_XY_NUM		ISP3X_YNR_XY_NUM
#define ISP39_YNR_NLM_COE_NUM		6

#define ISP39_BAYNR_XY_NUM		ISP3X_BAYNR_XY_NUM
#define ISP39_BAYNR_GAIN_NUM		16

#define ISP39_BAY3D_XY_NUM		16
#define ISP39_BAY3D_WD_NUM		9
#define ISP39_BAY3D_TNRSIG_NUM		20
#define ISP39_BAY3D_COEFF_NUM		9

#define ISP39_SHARP_X_NUM		ISP3X_SHARP_X_NUM
#define ISP39_SHARP_Y_NUM		ISP3X_SHARP_Y_NUM
#define ISP39_SHARP_GAUS_COEF_NUM	ISP3X_SHARP_GAUS_COEF_NUM
#define ISP39_SHARP_GAIN_ADJ_NUM	14
#define ISP39_SHARP_STRENGTH_NUM	22
#define ISP39_SHARP_TEX_WET_LUT_NUM	17
#define ISP39_SHARP_DETAIL_STRG_NUM 17

#define ISP39_CAC_STRENGTH_NUM		ISP3X_CAC_STRENGTH_NUM

#define ISP39_CSM_COEFF_NUM		ISP3X_CSM_COEFF_NUM

#define ISP39_DEBAYER_LUMA_NUM		7
#define ISP39_DEBAYER_DRCT_OFFSET_NUM	8
#define ISP39_DEBAYER_VSIGMA_NUM	8

#define ISP39_RGBIR_SCALE_NUM		4
#define ISP39_RGBIR_LUMA_POINT_NUM	17
#define ISP39_RGBIR_SCALE_MAP_NUM	17

#define ISP39_YUVME_SIGMA_NUM		16

#define ISP39_MEAN_BLK_X_NUM		15
#define ISP39_MEAN_BLK_Y_NUM		15

#define ISP39_BNR2AEBIG_SEL_EN		0x10
#define ISP39_BNR2AE0_SEL_EN		0x20

struct isp39_gic_cfg {
	/* CONTROL */
	__u8 bypass_en;
	/* DIFF_PARA1 */
	__u16 regmingradthrdark2;
	__u16 regmingradthrdark1;
	__u16 regminbusythre;
	/* DIFF_PARA2 */
	__u16 regdarkthre;
	__u16 regmaxcorvboth;
	__u16 regdarktthrehi;
	/* DIFF_PARA3 */
	__u8 regkgrad2dark;
	__u8 regkgrad1dark;
	__u8 regstrengthglobal_fix;
	__u8 regdarkthrestep;
	__u8 regkgrad2;
	__u8 regkgrad1;
	__u8 reggbthre;
	/* DIFF_PARA4 */
	__u16 regmaxcorv;
	__u16 regmingradthr2;
	__u16 regmingradthr1;
	/* NOISE_PARA1 */
	__u8 gr_ratio;
	__u8 noise_scale;
	__u16 noise_base;
	/* NOISE_PARA2 */
	__u16 diff_clip;
	/* SIGMA */
	__u16 sigma_y[ISP39_GIC_SIGMA_Y_NUM];
} __attribute__ ((packed));

struct isp39_dpcc_cfg {
	/* MODE */
	__u8 stage1_enable;
	__u8 grayscale_mode;
	/* OUTPUT_MODE */
	__u8 border_bypass_mode;
	__u8 sw_rk_out_sel;
	__u8 sw_dpcc_output_sel;
	__u8 stage1_rb_3x3;
	__u8 stage1_g_3x3;
	__u8 stage1_incl_rb_center;
	__u8 stage1_incl_green_center;
	/* SET_USE */
	__u8 stage1_use_fix_set;
	__u8 stage1_use_set_3;
	__u8 stage1_use_set_2;
	__u8 stage1_use_set_1;
	/* METHODS_SET_1 */
	__u8 sw_rk_red_blue1_en;
	__u8 rg_red_blue1_enable;
	__u8 rnd_red_blue1_enable;
	__u8 ro_red_blue1_enable;
	__u8 lc_red_blue1_enable;
	__u8 pg_red_blue1_enable;
	__u8 sw_rk_green1_en;
	__u8 rg_green1_enable;
	__u8 rnd_green1_enable;
	__u8 ro_green1_enable;
	__u8 lc_green1_enable;
	__u8 pg_green1_enable;
	/* METHODS_SET_2 */
	__u8 sw_rk_red_blue2_en;
	__u8 rg_red_blue2_enable;
	__u8 rnd_red_blue2_enable;
	__u8 ro_red_blue2_enable;
	__u8 lc_red_blue2_enable;
	__u8 pg_red_blue2_enable;
	__u8 sw_rk_green2_en;
	__u8 rg_green2_enable;
	__u8 rnd_green2_enable;
	__u8 ro_green2_enable;
	__u8 lc_green2_enable;
	__u8 pg_green2_enable;
	/* METHODS_SET_3 */
	__u8 sw_rk_red_blue3_en;
	__u8 rg_red_blue3_enable;
	__u8 rnd_red_blue3_enable;
	__u8 ro_red_blue3_enable;
	__u8 lc_red_blue3_enable;
	__u8 pg_red_blue3_enable;
	__u8 sw_rk_green3_en;
	__u8 rg_green3_enable;
	__u8 rnd_green3_enable;
	__u8 ro_green3_enable;
	__u8 lc_green3_enable;
	__u8 pg_green3_enable;
	/* LINE_THRESH_1 */
	__u8 sw_mindis1_rb;
	__u8 sw_mindis1_g;
	__u8 line_thr_1_rb;
	__u8 line_thr_1_g;
	/* LINE_MAD_FAC_1 */
	__u8 sw_dis_scale_min1;
	__u8 sw_dis_scale_max1;
	__u8 line_mad_fac_1_rb;
	__u8 line_mad_fac_1_g;
	/* PG_FAC_1 */
	__u8 pg_fac_1_rb;
	__u8 pg_fac_1_g;
	/* RND_THRESH_1 */
	__u8 rnd_thr_1_rb;
	__u8 rnd_thr_1_g;
	/* RG_FAC_1 */
	__u8 rg_fac_1_rb;
	__u8 rg_fac_1_g;
	/* LINE_THRESH_2 */
	__u8 sw_mindis2_rb;
	__u8 sw_mindis2_g;
	__u8 line_thr_2_rb;
	__u8 line_thr_2_g;
	/* LINE_MAD_FAC_2 */
	__u8 sw_dis_scale_min2;
	__u8 sw_dis_scale_max2;
	__u8 line_mad_fac_2_rb;
	__u8 line_mad_fac_2_g;
	/* PG_FAC_2 */
	__u8 pg_fac_2_rb;
	__u8 pg_fac_2_g;
	/* RND_THRESH_2 */
	__u8 rnd_thr_2_rb;
	__u8 rnd_thr_2_g;
	/* RG_FAC_2 */
	__u8 rg_fac_2_rb;
	__u8 rg_fac_2_g;
	/* LINE_THRESH_3 */
	__u8 sw_mindis3_rb;
	__u8 sw_mindis3_g;
	__u8 line_thr_3_rb;
	__u8 line_thr_3_g;
	/* LINE_MAD_FAC_3 */
	__u8 sw_dis_scale_min3;
	__u8 sw_dis_scale_max3;
	__u8 line_mad_fac_3_rb;
	__u8 line_mad_fac_3_g;
	/* PG_FAC_3 */
	__u8 pg_fac_3_rb;
	__u8 pg_fac_3_g;
	/* RND_THRESH_3 */
	__u8 rnd_thr_3_rb;
	__u8 rnd_thr_3_g;
	/* RG_FAC_3 */
	__u8 rg_fac_3_rb;
	__u8 rg_fac_3_g;
	/* RO_LIMITS */
	__u8 ro_lim_3_rb;
	__u8 ro_lim_3_g;
	__u8 ro_lim_2_rb;
	__u8 ro_lim_2_g;
	__u8 ro_lim_1_rb;
	__u8 ro_lim_1_g;
	/* RND_OFFS */
	__u8 rnd_offs_3_rb;
	__u8 rnd_offs_3_g;
	__u8 rnd_offs_2_rb;
	__u8 rnd_offs_2_g;
	__u8 rnd_offs_1_rb;
	__u8 rnd_offs_1_g;
	/* BPT_CTRL */
	__u8 bpt_rb_3x3;
	__u8 bpt_g_3x3;
	__u8 bpt_incl_rb_center;
	__u8 bpt_incl_green_center;
	__u8 bpt_use_fix_set;
	__u8 bpt_use_set_3;
	__u8 bpt_use_set_2;
	__u8 bpt_use_set_1;
	__u8 bpt_cor_en;
	__u8 bpt_det_en;
	/* BPT_NUMBER */
	__u16 bp_number;
	/* BPT_ADDR */
	__u16 bp_table_addr;
	/* BPT_DATA */
	__u16 bpt_v_addr;
	__u16 bpt_h_addr;
	/* BP_CNT */
	__u32 bp_cnt;
	/* PDAF_EN */
	__u8 sw_pdaf_en;
	/* PDAF_POINT_EN */
	__u8 pdaf_point_en[ISP39_DPCC_PDAF_POINT_NUM];
	/* PDAF_OFFSET */
	__u16 pdaf_offsety;
	__u16 pdaf_offsetx;
	/* PDAF_WRAP */
	__u16 pdaf_wrapy;
	__u16 pdaf_wrapx;
	/* PDAF_SCOPE */
	__u16 pdaf_wrapy_num;
	__u16 pdaf_wrapx_num;
	/* PDAF_POINT */
	struct isp2x_dpcc_pdaf_point point[ISP39_DPCC_PDAF_POINT_NUM];
	/* PDAF_FORWARD_MED */
	__u8 pdaf_forward_med;
} __attribute__ ((packed));

struct isp39_yuvme_cfg {
	/* CTRL */
	__u8 bypass;
	__u8 tnr_wgt0_en;
	/* YUVME_PARA0 */
	__u8 global_nr_strg;
	__u8 wgt_fact3;
	__u8 search_range_mode;
	__u8 static_detect_thred;
	/* YUVME_PARA1 */
	__u8 time_relevance_offset;
	__u8 space_relevance_offset;
	__u8 nr_diff_scale;
	__u16 nr_fusion_limit;
	/* YUVME_PARA2 */
	__u8 nr_static_scale;
	__u16 nr_motion_scale;
	__u8 nr_fusion_mode;
	__u16 cur_weight_limit;
	/* YUVME_SIGMA */
	__u16 nr_luma2sigma_val[ISP39_YUVME_SIGMA_NUM];
} __attribute__ ((packed));

struct isp39_rgbir_cfg {
	/* THETA */
	__u16 coe_theta;
	/* DELTA */
	__u16 coe_delta;
	/* SCALE */
	__u16 scale[ISP39_RGBIR_SCALE_NUM];
	/* LUMA_POINT */
	__u16 luma_point[ISP39_RGBIR_LUMA_POINT_NUM];
	/* SCALE_MAP */
	__u16 scale_map[ISP39_RGBIR_SCALE_MAP_NUM];
} __attribute__ ((packed));

struct isp39_ldcv_cfg {
	/* CTRL */
	__u8 thumb_mode;
	__u8 dth_bypass;
	__u8 force_map_en;
	__u8 map13p3_en;

	/* BIC_TABLE */
	__u8 bicubic[ISP39_LDCV_BIC_NUM];

	/* LAST_OFFSET */
	__u16 last_offset;

	__u32 out_vsize;

	__u32 hsize;
	__u32 vsize;
	__s32 buf_fd;
} __attribute__ ((packed));

struct isp39_ldch_cfg {
	__u8 frm_end_dis;
	__u8 sample_avr_en;
	__u8 bic_mode_en;
	__u8 force_map_en;
	__u8 map13p3_en;

	__u8 bicubic[ISP39_LDCH_BIC_NUM];

	__u16 out_hsize;

	__u32 hsize;
	__u32 vsize;
	__s32 buf_fd;
} __attribute__ ((packed));

struct isp39_ccm_cfg {
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
	__u16 alp_y[ISP39_CCM_CURVE_NUM];
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
	__u16 hf_factor[ISP39_CCM_HF_FACTOR_NUM];

} __attribute__ ((packed));

struct isp39_debayer_cfg {
	/* CONTROL */
	__u8 filter_g_en;
	__u8 filter_c_en;
	/* LUMA_DX */
	__u8 luma_dx[ISP39_DEBAYER_LUMA_NUM];
	/* G_INTERP */
	__u8 clip_en;
	__u8 dist_scale;
	__u8 thed0;
	__u8 thed1;
	__u8 select_thed;
	__u8 max_ratio;
	/* G_INTERP_FILTER1 */
	__s8 filter1_coe1;
	__s8 filter1_coe2;
	__s8 filter1_coe3;
	__s8 filter1_coe4;
	/* G_INTERP_FILTER2 */
	__s8 filter2_coe1;
	__s8 filter2_coe2;
	__s8 filter2_coe3;
	__s8 filter2_coe4;
	/* G_INTERP_OFFSET_ALPHA */
	__u16 gain_offset;
	__u8 gradloflt_alpha;
	__u8 wgt_alpha;
	/* G_INTERP_DRCT_OFFSET */
	__u16 drct_offset[ISP39_DEBAYER_DRCT_OFFSET_NUM];
	/* G_FILTER_MODE_OFFSET */
	__u8 gfilter_mode;
	__u16 bf_ratio;
	__u16 offset;
	/* G_FILTER_FILTER */
	__s8 filter_coe0;
	__s8 filter_coe1;
	__s8 filter_coe2;
	/* G_FILTER_VSIGMA */
	__u16 vsigma[ISP39_DEBAYER_VSIGMA_NUM];
	/* C_FILTER_GUIDE_GAUS */
	__s8 guid_gaus_coe0;
	__s8 guid_gaus_coe1;
	__s8 guid_gaus_coe2;
	/* C_FILTER_CE_GAUS */
	__s8 ce_gaus_coe0;
	__s8 ce_gaus_coe1;
	__s8 ce_gaus_coe2;
	/* C_FILTER_ALPHA_GAUS */
	__s8 alpha_gaus_coe0;
	__s8 alpha_gaus_coe1;
	__s8 alpha_gaus_coe2;
	/* C_FILTER_LOG_OFFSET */
	__u16 loghf_offset;
	__u16 loggd_offset;
	__u8 log_en;
	/* C_FILTER_ALPHA */
	__u16 alpha_offset;
	__u32 alpha_scale;
	/* C_FILTER_EDGE */
	__u16 edge_offset;
	__u32 edge_scale;
	/* C_FILTER_IIR_0 */
	__u8 ce_sgm;
	__u8 exp_shift;
	__u16 wgtslope;
	/* C_FILTER_IIR_1 */
	__u8 wet_clip;
	__u8 wet_ghost;
	/* C_FILTER_BF */
	__u16 bf_sgm;
	__u8 bf_clip;
	__u8 bf_curwgt;
} __attribute__ ((packed));

struct isp39_bay3d_cfg {
	/* BAY3D_CTRL */
	__u8 bypass_en;
	__u8 iirsparse_en;
	/* BAY3D_CTRL1 */
	__u8 transf_bypass_en;
	__u8 sigma_curve_double_en;
	__u8 cur_spnr_bypass_en;
	__u8 cur_spnr_sigma_idxfilt_bypass_en;
	__u8 cur_spnr_sigma_curve_double_en;
	__u8 pre_spnr_bypass_en;
	__u8 pre_spnr_sigma_idxfilt_bypass_en;
	__u8 pre_spnr_sigma_curve_double_en;
	__u8 lpf_hi_bypass_en;
	__u8 lo_diff_vfilt_bypass_en;
	__u8 lpf_lo_bypass_en;
	__u8 lo_wgt_hfilt_en;
	__u8 lo_diff_hfilt_en;
	__u8 sig_hfilt_en;
	__u8 spnr_pre_sigma_use_en;
	__u8 lo_detection_mode;
	__u8 md_wgt_out_en;
	__u8 md_bypass_en;
	__u8 iirspnr_out_en;
	__u8 lomdwgt_dbg_en;
	__u8 curdbg_out_en;
	__u8 noisebal_mode;
	/* BAY3D_CTRL2 */
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
	/* BAY3D_CURDGAIN */
	__u16 cur_spnr_sigma_hdr_sht_scale;
	__u16 cur_spnr_sigma_scale;
	/* BAY3D_CURSIG_X */
	__u16 cur_spnr_luma_sigma_x[ISP39_BAY3D_XY_NUM];
	/* BAY3D_CURSIG_Y */
	__u16 cur_spnr_luma_sigma_y[ISP39_BAY3D_XY_NUM];
	/* BAY3D_CURGAIN_OFF */
	__u16 cur_spnr_sigma_rgain_offset;
	__u16 cur_spnr_sigma_bgain_offset;
	/* BAY3D_CURSIG_OFF */
	__u16 cur_spnr_sigma_hdr_sht_offset;
	__u16 cur_spnr_sigma_offset;
	/* BAY3D_CURWTH */
	__u16 cur_spnr_pix_diff_max_limit;
	__u16 cur_spnr_wgt_cal_offset;
	/* BAY3D_CURBFALP */
	__u16 cur_spnr_wgt;
	__u16 pre_spnr_wgt;
	/* BAY3D_CURWDC */
	__u16 cur_spnr_space_rb_wgt[ISP39_BAY3D_WD_NUM];
	/* BAY3D_CURWDY */
	__u16 cur_spnr_space_gg_wgt[ISP39_BAY3D_WD_NUM];
	/* BAY3D_IIRDGAIN */
	__u16 pre_spnr_sigma_hdr_sht_scale;
	__u16 pre_spnr_sigma_scale;
	/* BAY3D_IIRSIG_X */
	__u16 pre_spnr_luma_sigma_x[ISP39_BAY3D_XY_NUM];
	/* BAY3D_IIRSIG_Y */
	__u16 pre_spnr_luma_sigma_y[ISP39_BAY3D_XY_NUM];
	/* BAY3D_IIRGAIN_OFF */
	__u16 pre_spnr_sigma_rgain_offset;
	__u16 pre_spnr_sigma_bgain_offset;
	/* BAY3D_IIRSIG_OFF */
	__u16 pre_spnr_sigma_hdr_sht_offset;
	__u16 pre_spnr_sigma_offset;
	/* BAY3D_IIRWTH */
	__u16 pre_spnr_pix_diff_max_limit;
	__u16 pre_spnr_wgt_cal_offset;
	/* BAY3D_IIRWDC */
	__u16 pre_spnr_space_rb_wgt[ISP39_BAY3D_WD_NUM];
	/* BAY3D_IIRWDY */
	__u16 pre_spnr_space_gg_wgt[ISP39_BAY3D_WD_NUM];
	/* BAY3D_BFCOEF */
	__u16 cur_spnr_wgt_cal_scale;
	__u16 pre_spnr_wgt_cal_scale;

	/* BAY3D_TNRSIG_X */
	__u16 tnr_luma_sigma_x[ISP39_BAY3D_TNRSIG_NUM];
	/* BAY3D_TNRSIG_Y */
	__u16 tnr_luma_sigma_y[ISP39_BAY3D_TNRSIG_NUM];
	/* BAY3D_TNRHIW */
	__u16 tnr_lpf_hi_coeff[ISP39_BAY3D_COEFF_NUM];
	/* BAY3D_TNRLOW*/
	__u16 tnr_lpf_lo_coeff[ISP39_BAY3D_COEFF_NUM];
	/* BAY3D_TNRGF3 */
	__u16 tnr_wgt_filt_coeff0;
	__u16 tnr_wgt_filt_coeff1;
	__u16 tnr_wgt_filt_coeff2;
	/* BAY3D_TNRSIGSCL */
	__u16 tnr_sigma_scale;
	__u16 tnr_sigma_hdr_sht_scale;
	/* BAY3D_TNRVIIR */
	__u8 tnr_sig_vfilt_wgt;
	__u8 tnr_lo_diff_vfilt_wgt;
	__u8 tnr_lo_wgt_vfilt_wgt;
	__u8 tnr_sig_first_line_scale;
	__u8 tnr_lo_diff_first_line_scale;
	/* BAY3D_TNRLFSCL */
	__u16 tnr_lo_wgt_cal_offset;
	__u16 tnr_lo_wgt_cal_scale;
	/* BAY3D_TNRLFSCLTH */
	__u16 tnr_low_wgt_cal_max_limit;
	__u16 tnr_mode0_base_ratio;
	/* BAY3D_TNRDSWGTSCL */
	__u16 tnr_lo_diff_wgt_cal_offset;
	__u16 tnr_lo_diff_wgt_cal_scale;
	/* BAY3D_TNRWLSTSCL */
	__u16 tnr_lo_mge_pre_wgt_offset;
	__u16 tnr_lo_mge_pre_wgt_scale;
	/* BAY3D_TNRWGT0SCL0 */
	__u16 tnr_mode0_lo_wgt_scale;
	__u16 tnr_mode0_lo_wgt_hdr_sht_scale;
	/* BAY3D_TNRWGT1SCL1 */
	__u16 tnr_mode1_lo_wgt_scale;
	__u16 tnr_mode1_lo_wgt_hdr_sht_scale;
	/* BAY3D_TNRWGT1SCL2 */
	__u16 tnr_mode1_wgt_scale;
	__u16 tnr_mode1_wgt_hdr_sht_scale;
	/* BAY3D_TNRWGTOFF */
	__u16 tnr_mode1_lo_wgt_offset;
	__u16 tnr_mode1_lo_wgt_hdr_sht_offset;
	/* BAY3D_TNRWGT1OFF */
	__u16 tnr_auto_sigma_count_wgt_thred;
	__u16 tnr_mode1_wgt_min_limit;
	__u16 tnr_mode1_wgt_offset;
	/* BAY3D_TNRSIGORG */
	__u32 tnr_out_sigma_sq;
	/* BAY3D_TNRWLO_THL */
	__u16 tnr_lo_wgt_clip_min_limit;
	__u16 tnr_lo_wgt_clip_hdr_sht_min_limit;
	/* BAY3D_TNRWLO_THH */
	__u16 tnr_lo_wgt_clip_max_limit;
	__u16 tnr_lo_wgt_clip_hdr_sht_max_limit;
	/* BAY3D_TNRWHI_THL */
	__u16 tnr_hi_wgt_clip_min_limit;
	__u16 tnr_hi_wgt_clip_hdr_sht_min_limit;
	/* BAY3D_TNRWHI_THH */
	__u16 tnr_hi_wgt_clip_max_limit;
	__u16 tnr_hi_wgt_clip_hdr_sht_max_limit;
	/* BAY3D_TNRKEEP */
	__u8 tnr_cur_spnr_hi_wgt_min_limit;
	__u8 tnr_pre_spnr_hi_wgt_min_limit;
	/* BAY3D_PIXMAX */
	__u16 tnr_pix_max;
	__u8 lowgt_ctrl;
	__u16 lowgt_offint;
	/* BAY3D_SIGNUMTH */
	__u32 tnr_auto_sigma_count_th;
	/* BAY3D_TNRMO_STR */
	__u16 tnr_motion_nr_strg;
	__u8 tnr_gain_max;
} __attribute__ ((packed));

struct isp39_ynr_cfg {
	/* GLOBAL_CTRL */
	__u8 lospnr_bypass;
	__u8 hispnr_bypass;
	__u8 exgain_bypass;
	__u16 global_set_gain;
	__u8 gain_merge_alpha;
	__u8 rnr_en;
	/* RNR_MAX_R */
	__u16 rnr_max_radius;
	__u16 local_gain_scale;
	/* RNR_CENTER_COOR */
	__u16 rnr_center_coorh;
	__u16 rnr_center_coorv;
	/* LOWNR_CTRL0 */
	__u16 ds_filt_soft_thred_scale;
	__u8 ds_img_edge_scale;
	__u16 ds_filt_wgt_thred_scale;
	/* LOWNR_CTRL1 */
	__u8 ds_filt_local_gain_alpha;
	__u8 ds_iir_init_wgt_scale;
	__u16 ds_filt_center_wgt;
	/* LOWNR_CTRL2 */
	__u16 ds_filt_inv_strg;
	__u16 lospnr_wgt;
	/* LOWNR_CTRL3 */
	__u16 lospnr_center_wgt;
	__u16 lospnr_strg;
	/* LOWNR_CTRL4 */
	__u16 lospnr_dist_vstrg_scale;
	__u16 lospnr_dist_hstrg_scale;
	/* GAUSS_COEFF */
	__u8 pre_filt_coeff0;
	__u8 pre_filt_coeff1;
	__u8 pre_filt_coeff2;
	/* LOW_GAIN_ADJ */
	__u8 lospnr_gain2strg_val[ISP39_YNR_LOW_GAIN_ADJ_NUM];
	/* SGM_DX */
	__u16 luma2sima_idx[ISP39_YNR_XY_NUM];
	/* LSGM_Y */
	__u16 luma2sima_val[ISP39_YNR_XY_NUM];
	/* RNR_STRENGTH */
	__u8 radius2strg_val[ISP39_YNR_XY_NUM];
	/* NLM_STRONG_EDGE */
	__u8 hispnr_strong_edge;
	/* NLM_SIGMA_GAIN */
	__u16 hispnr_sigma_min_limit;
	__u8 hispnr_local_gain_alpha;
	__u16 hispnr_strg;
	/* NLM_COE */
	__u8 hispnr_filt_coeff[ISP39_YNR_NLM_COE_NUM];
	/* NLM_WEIGHT */
	__u16 hispnr_filt_wgt_offset;
	__u32 hispnr_filt_center_wgt;
	/* NLM_NR_WEIGHT */
	__u16 hispnr_filt_wgt;
	__u16 hispnr_gain_thred;
} __attribute__ ((packed));

struct isp39_cnr_cfg {
	/* CNR_CTRL */
	__u8 exgain_bypass;
	__u8 yuv422_mode;
	__u8 thumb_mode;
	__u8 hiflt_wgt0_mode;
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
	__u8 gaus_flt_coeff[ISP39_CNR_GAUS_COE_NUM];
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
	__u8 sgm_ratio[ISP39_CNR_SIGMA_Y_NUM];
	/* CNR_IIR_GLOBAL_GAIN */
	__u8 loflt_global_sgm_ratio;
	__u8 loflt_global_sgm_ratio_alpha;
	/* CNR_WGT_SIGMA */
	__u8 cur_wgt[ISP39_CNR_WGT_SIGMA_Y_NUM];
	/* GAUS_X_SIGMAR */
	__u16 hiflt_vsigma_idx[ISP39_CNR_GAUS_SIGMAR_NUM];
	/* GAUS_Y_SIGMAR */
	__u16 hiflt_vsigma[ISP39_CNR_GAUS_SIGMAR_NUM];
} __attribute__ ((packed));

struct isp39_sharp_cfg {
	/* SHARP_EN */
	__u8 bypass;
	__u8 center_mode;
	__u8 local_gain_bypass;
	__u8 radius_step_mode;
	__u8 noise_clip_mode;
	__u8 clipldx_sel;
	__u8 baselmg_sel;
	__u8 noise_filt_sel;
	__u8 tex2wgt_en;

	/* SHARP_ALPHA */
	__u8 pre_bifilt_alpha;
	__u8 guide_filt_alpha;
	__u8 detail_bifilt_alpha;
	__u8 global_sharp_strg;

	/* SHARP_LUMA_DX */
	__u8 luma2table_idx[ISP39_SHARP_X_NUM];

	/* SHARP_PBF_SIGMA_INV */
	__u16 pbf_sigma_inv[ISP39_SHARP_Y_NUM];

	/* SHARP_DETAILBF_SIGMA_INV */
	__u16 bf_sigma_inv[ISP39_SHARP_Y_NUM];

	/* SHARP_SIGMA_SHIFT */
	__u8 pbf_sigma_shift;
	__u8 bf_sigma_shift;

	/* SHARP_LOCAL_STRG */
	__u16 luma2strg_val[ISP39_SHARP_Y_NUM];

	/* SHARP_POS_CLIP */
	__u16 luma2posclip_val[ISP39_SHARP_Y_NUM];

	/* SHARP_PBF_COEF */
	__u8 pbf_coef0;
	__u8 pbf_coef1;
	__u8 pbf_coef2;

	/* SHARP_DETAILBF_COEF */
	__u8 bf_coef0;
	__u8 bf_coef1;
	__u8 bf_coef2;

	/* SHARP_IMGLPF_COEF */
	__u8 img_lpf_coeff[ISP39_SHARP_GAUS_COEF_NUM];

	/* SHARP_GAIN */
	__u16 global_gain;
	__u8 gain_merge_alpha;
	__u8 local_gain_scale;

	/* SHARP_GAIN_ADJUST */
	__u16 gain2strg_val[ISP39_SHARP_GAIN_ADJ_NUM];

	/* SHARP_CENTER */
	__u16 center_x;
	__u16 center_y;

	/* SHARP_GAIN_DIS_STRENGTH */
	__u8 distance2strg_val[ISP39_SHARP_STRENGTH_NUM];

	/* NEG_CLIP */
	__u16 luma2neg_clip_val[ISP39_SHARP_Y_NUM];

	/* TEXTURE0 */
	__u16 noise_max_limit;
	__u8 tex_reserve_level;
	/* TEXTURE1 */
	__u16 tex_wet_scale;
	__u8 noise_norm_bit;
	__u8 tex_wgt_mode;
	/* TEXTURE_LUT */
	__u16 tex2wgt_val[ISP39_SHARP_TEX_WET_LUT_NUM];
	/* TEXTURE2 */
	__u32 noise_strg;
	/* DETAIL_STRG_LUT */
	__u16 detail2strg_val[ISP39_SHARP_DETAIL_STRG_NUM];
} __attribute__ ((packed));

struct isp39_dhaz_cfg {
	/* DHAZ_CTRL */
	__u8 dc_en;
	__u8 hist_en;
	__u8 map_mode;
	__u8 mem_mode;
	__u8 mem_force;
	__u8 air_lc_en;
	__u8 enhance_en;
	__u8 soft_wr_en;
	__u8 round_en;
	__u8 color_deviate_en;
	__u8 enh_luma_en;
	/* DHAZ_ADP0 */
	__u8 dc_min_th;
	__u8 dc_max_th;
	__u8 yhist_th;
	__u8 yblk_th;
	/* DHAZ_ADP1 */
	__u8 bright_min;
	__u8 bright_max;
	__u16 wt_max;
	/* DHAZ_ADP2 */
	__u8 air_min;
	__u8 air_max;
	__u8 dark_th;
	__u8 tmax_base;
	/* DHAZ_ADP_TMAX */
	__u16 tmax_off;
	__u16 tmax_max;
	/* DHAZ_ENHANCE */
	__u16 enhance_chroma;
	__u16 enhance_value;
	/* DHAZ_IIR0 */
	__u8 stab_fnum;
	__u8 iir_sigma;
	__u16 iir_wt_sigma;
	/* DHAZ_IIR1 */
	__u8 iir_air_sigma;
	__u16 iir_tmax_sigma;
	__u8 iir_pre_wet;
	/* DHAZ_SOFT_CFG0 */
	__u8 cfg_alpha;
	__u8 cfg_air;
	__u16 cfg_wt;
	/* DHAZ_SOFT_CFG1 */
	__u16 cfg_tmax;
	/* DHAZ_BF_SIGMA */
	__u8 space_sigma_cur;
	__u8 space_sigma_pre;
	__u16 range_sima;
	/* DHAZ_BF_WET */
	__u16 bf_weight;
	__u16 dc_weitcur;
	/* DHAZ_ENH_CURVE0 ~ 5 */
	__u16 enh_curve[ISP39_DHAZ_ENH_CURVE_NUM];
	/* DHAZ_GAUS */
	__u8 gaus_h0;
	__u8 gaus_h1;
	__u8 gaus_h2;
	/* DHAZ_ENH_LUMA0 ~ 5 */
	__u16 enh_luma[ISP39_DHAZ_ENH_LUMA_NUM];
	/* DHAZ_ADP_WR0 */
	__u16 adp_wt_wr;
	__u8 adp_air_wr;
	/* DHAZ_ADP_WR1 */
	__u16 adp_tmax_wr;
	/* DHAZ_GAIN_IDX0 ~ 3 */
	__u8 sigma_idx[ISP39_DHAZ_SIGMA_IDX_NUM];
	/* DHAZ_GAIN_LUT0 ~ 5 */
	__u16 sigma_lut[ISP39_DHAZ_SIGMA_LUT_NUM];
	/* GAIN_FUSE */
	__u16 gain_fuse_alpha;
	/* DHAZ_ADP_HF */
	__u8 hist_k;
	__u8 hist_th_off;
	__u8 hist_min;
	/* DHAZ_BLOCK_SIZE */
	__u16 blk_het;
	__u16 blk_wid;
	/* DHAZ_THUMB_SIZE */
	__u8 thumb_row;
	__u8 thumb_col;
	/* DHAZ_HIST_CFG */
	__u16 cfg_k_alpha;
	__u16 cfg_k;
	/* DHAZ_HIST_GAIN */
	__u16 k_gain;
	/* DHAZ_BLEND_WET0 ~ 5 */
	__u16 blend_wet[ISP39_DHAZ_BLEND_WET_NUM];
	/* DHAZ_HIST_IIR */
	__u16 hist_iir_wr;
	__u16 hist_iir[ISP39_DHAZ_HIST_IIR_BLK_MAX][ISP39_DHAZ_HIST_IIR_NUM];
} __attribute__ ((packed));

struct isp39_drc_cfg {
	/* DRC_CTRL0 */
	__u8 bypass_en;
	__u8 cmps_byp_en;
	__u8 gainx32_en;
	__u8 raw_dly_dis;

	/* DRC_CTRL1 */
	__u16 position;
	__u16 compres_scl;
	__u8 offset_pow2;

	/* DRC_LPRATIO */
	__u16 lpdetail_ratio;
	__u16 hpdetail_ratio;
	__u8 delta_scalein;

	/* DRC_BILAT0 */
	__u8 bilat_wt_off;
	__u16 thumb_thd_neg;
	__u8 thumb_thd_enable;
	__u8 weicur_pix;

	/* DRC_BILAT1 */
	__u8 cmps_offset_bits_int;
	__u8 cmps_fixbit_mode;
	__u16 drc_gas_t;

	/* DRC_BILAT2 */
	__u32 thumb_clip;
	__u8 thumb_scale;

	/* DRC_BILAT3 */
	__u32 range_sgm_inv0;
	__u16 range_sgm_inv1;

	/* DRC_BILAT4 */
	__u8 weig_bilat;
	__u8 weight_8x8thumb;
	__u16 bilat_soft_thd;
	__u8 enable_soft_thd;

	/* DRC_GAIN_Y0 ~ Y8 */
	__u16 gain_y[ISP39_DRC_Y_NUM];
	/* DRC_COMPRES_Y0 ~ Y8 */
	__u16 compres_y[ISP39_DRC_Y_NUM];
	/* DRC_SCALE_Y0 ~ Y8 */
	__u16 scale_y[ISP39_DRC_Y_NUM];

	/* IIRWG_GAIN */
	__u16 min_ogain;
	/* SFTHD_Y0 ~ Y8 */
	__u16 sfthd_y[ISP39_DRC_Y_NUM];
} __attribute__ ((packed));

struct isp39_rawawb_meas_cfg {
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
	__u8 wp_blk_wei_w[ISP39_RAWAWB_WEIGHT_NUM];

	struct isp2x_bls_fixed_val bls2_val;
} __attribute__ ((packed));

struct isp39_rawaf_meas_cfg {
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
	__u8 ae_config_use;
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

struct isp39_isp_other_cfg {
	struct isp32_bls_cfg bls_cfg;
	struct isp39_dpcc_cfg dpcc_cfg;
	struct isp3x_lsc_cfg lsc_cfg;
	struct isp32_awb_gain_cfg awb_gain_cfg;
	struct isp39_gic_cfg gic_cfg;
	struct isp39_debayer_cfg debayer_cfg;
	struct isp39_ccm_cfg ccm_cfg;
	struct isp3x_gammaout_cfg gammaout_cfg;
	struct isp2x_cproc_cfg cproc_cfg;
	struct isp2x_sdg_cfg sdg_cfg;
	struct isp39_drc_cfg drc_cfg;
	struct isp32_hdrmge_cfg hdrmge_cfg;
	struct isp39_dhaz_cfg dhaz_cfg;
	struct isp2x_3dlut_cfg isp3dlut_cfg;
	struct isp39_ldch_cfg ldch_cfg;
	struct isp39_bay3d_cfg bay3d_cfg;
	struct isp39_ynr_cfg ynr_cfg;
	struct isp39_cnr_cfg cnr_cfg;
	struct isp39_sharp_cfg sharp_cfg;
	struct isp32_cac_cfg cac_cfg;
	struct isp3x_gain_cfg gain_cfg;
	struct isp21_csm_cfg csm_cfg;
	struct isp21_cgc_cfg cgc_cfg;
	struct isp39_yuvme_cfg yuvme_cfg;
	struct isp39_ldcv_cfg ldcv_cfg;
	struct isp39_rgbir_cfg rgbir_cfg;
} __attribute__ ((packed));

struct isp39_isp_meas_cfg {
	struct isp39_rawaf_meas_cfg rawaf;
	struct isp39_rawawb_meas_cfg rawawb;
	struct isp2x_rawaebig_meas_cfg rawae0;
	struct isp2x_rawaebig_meas_cfg rawae3;
	struct isp2x_rawhistbig_cfg rawhist0;
	struct isp2x_rawhistbig_cfg rawhist3;
} __attribute__ ((packed));

struct isp39_isp_params_cfg {
	__u64 module_en_update;
	__u64 module_ens;
	__u64 module_cfg_update;

	__u32 frame_id;
	struct isp39_isp_meas_cfg meas;
	struct isp39_isp_other_cfg others;
	struct sensor_exposure_cfg exposure;
} __attribute__ ((packed));

struct isp39_dhaz_stat {
	__u16 adp_wt;
	__u16 adp_air_base;
	__u16 adp_tmax;

	__u16 hist_iir[ISP39_DHAZ_HIST_IIR_BLK_MAX][ISP39_DHAZ_HIST_IIR_NUM];
} __attribute__ ((packed));

struct isp39_bay3d_stat {
	__u32 tnr_auto_sigma_count;
	__u16 tnr_auto_sigma_calc[ISP39_BAY3D_TNRSIG_NUM];
} __attribute__ ((packed));

struct isp39_rawae_mean_lum {
	__u32 g:12;
	__u32 b:10;
	__u32 r:10;
} __attribute__ ((packed));

struct isp39_rawae_mean_line {
	/* line: 15 word + one unuse */
	struct isp39_rawae_mean_lum blk_x[ISP39_MEAN_BLK_X_NUM];
	__u32 reserved;
} __attribute__ ((packed));

struct isp39_rawae_stat {
	struct isp39_rawae_mean_line blk_y[ISP39_MEAN_BLK_Y_NUM];
} __attribute__ ((packed));

struct isp39_rawhist_stat {
	__u32 bin[ISP39_HIST_BIN_N_MAX];
} __attribute__ ((packed));

struct isp39_rawaf_mean_hiir {
	__u32 h1;
	__u32 h2;
} __attribute__ ((packed));

struct isp39_rawaf_mean_hiir_line {
	/* line: 15 * 2 word + two unuse */
	struct isp39_rawaf_mean_hiir hiir_blk_x[ISP39_MEAN_BLK_X_NUM];
	__u32 reserved[2];
} __attribute__ ((packed));

struct isp39_rawaf_mean_viir {
	__u32 v1;
	__u32 v2;
} __attribute__ ((packed));

struct isp39_rawaf_mean_viir_line {
	/* line: 15 * 2 word + two unuse */
	struct isp39_rawaf_mean_viir viir_blk_x[ISP39_MEAN_BLK_X_NUM];
	__u32 reserved[2];
} __attribute__ ((packed));

struct isp39_rawaf_mean_aehgl {
	__u32 average:12;
	__u32 highlight:20;
} __attribute__ ((packed));

struct isp39_rawaf_mean_aehgl_line {
	/* line: 15 word + one unuse */
	struct isp39_rawaf_mean_aehgl aehgl_blk_x[ISP39_MEAN_BLK_X_NUM];
	__u32 reserved;
} __attribute__ ((packed));

struct isp39_rawaf_stat {
	struct isp39_rawaf_mean_hiir_line hiir_blk_y[ISP39_MEAN_BLK_Y_NUM];
	struct isp39_rawaf_mean_viir_line viir_blk_y[ISP39_MEAN_BLK_Y_NUM];
	struct isp39_rawaf_mean_aehgl_line aehgl_blk_y[ISP39_MEAN_BLK_Y_NUM];
	__u32 h1iir_sumb;
	__u32 h2iir_sumb;
	__u32 v1iir_sumb;
	__u32 v2iir_sumb;
	__u32 sumy_winb;
	__u32 highlit_cnt_winb;
	__u32 reserved[2];
} __attribute__ ((packed));

struct isp39_rawawb_mean_ramdata {
	__u64 b:18;
	__u64 g:18;
	__u64 r:18;
	__u64 wp:10;
} __attribute__ ((packed));

struct isp39_rawawb_mean_ramdata_line {
	struct isp39_rawawb_mean_ramdata ramdata_blk_x[ISP39_MEAN_BLK_X_NUM];
	__u32 reserved[2];
} __attribute__ ((packed));

struct isp39_rawawb_mean_sum {
	__u32 rgain_nor;
	__u32 bgain_nor;
	__u32 wp_num_nor;
	__u32 wp_num2;

	__u32 rgain_big;
	__u32 bgain_big;
	__u32 wp_num_big;
	__u32 reserved;
} __attribute__ ((packed));

struct isp39_rawawb_mean_sum_exc {
	__u32 rgain_exc;
	__u32 bgain_exc;
	__u32 wp_num_exc;
	__u32 reserved;
} __attribute__ ((packed));

struct isp39_rawawb_stat {
	struct isp39_rawawb_mean_ramdata_line ramdata_blk_y[ISP39_MEAN_BLK_Y_NUM];
	struct isp39_rawawb_mean_sum sum[ISP39_RAWAWB_SUM_NUM];
	__u16 yhist[ISP39_RAWAWB_HSTBIN_NUM];
	struct isp39_rawawb_mean_sum_exc sum_exc[ISP39_RAWAWB_EXCL_STAT_NUM];
} __attribute__ ((packed));

struct isp39_stat {
	/* mean to ddr */
	struct isp39_rawae_stat rawae3;
	struct isp39_rawhist_stat rawhist3;
	struct isp39_rawae_stat rawae0;
	struct isp39_rawhist_stat rawhist0;
	struct isp39_rawaf_stat rawaf;
	struct isp39_rawawb_stat rawawb;
	/* ahb read reg */
	struct isp39_dhaz_stat dhaz;
	struct isp39_bay3d_stat bay3d;
	struct isp32_info2ddr_stat info2ddr;
	int buf_bay3d_iir_index;
} __attribute__ ((packed));

struct rkisp39_stat_buffer {
	struct isp39_stat stat;
	__u32 meas_type;
	__u32 frame_id;
	__u32 params_id;
} __attribute__ ((packed));
#endif /* _UAPI_RK_ISP39_CONFIG_H */
