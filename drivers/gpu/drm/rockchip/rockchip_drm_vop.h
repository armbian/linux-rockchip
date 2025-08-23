/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:Mark Yao <mark.yao@rock-chips.com>
 */

#ifndef _ROCKCHIP_DRM_VOP_H
#define _ROCKCHIP_DRM_VOP_H

#include <drm/drm_plane.h>
#include <drm/drm_modes.h>

#include "rockchip_drm_drv.h"

/*
 * major: IP major version, used for IP structure
 * minor: big feature change under same structure
 * build: RTL current SVN number
 */
#define VOP_VERSION(major, minor)	((major) << 8 | (minor))
#define VOP_MAJOR(version)		((version) >> 8)
#define VOP_MINOR(version)		((version) & 0xff)

#define VOP_VERSION_RK3066		VOP_VERSION(2, 1)
#define VOP_VERSION_RK3036		VOP_VERSION(2, 2)
#define VOP_VERSION_RK3126		VOP_VERSION(2, 4)
#define VOP_VERSION_PX30_LITE		VOP_VERSION(2, 5)
#define VOP_VERSION_PX30_BIG		VOP_VERSION(2, 6)
#define VOP_VERSION_RK3308		VOP_VERSION(2, 7)
#define VOP_VERSION_RV1126		VOP_VERSION(2, 0xb)
#define VOP_VERSION_RV1106		VOP_VERSION(2, 0xc)
#define VOP_VERSION_RK3576_LITE		VOP_VERSION(2, 0xd)
#define VOP_VERSION_RK3506		VOP_VERSION(2, 0xe)
#define VOP_VERSION_RV1126B		VOP_VERSION(2, 0xf)
#define VOP_VERSION_RK3288		VOP_VERSION(3, 0)
#define VOP_VERSION_RK3288W		VOP_VERSION(3, 1)
#define VOP_VERSION_RK3368		VOP_VERSION(3, 2)
#define VOP_VERSION_RK3366		VOP_VERSION(3, 4)
#define VOP_VERSION_RK3399_BIG		VOP_VERSION(3, 5)
#define VOP_VERSION_RK3399_LITE		VOP_VERSION(3, 6)
#define VOP_VERSION_RK3228		VOP_VERSION(3, 7)
#define VOP_VERSION_RK3328		VOP_VERSION(3, 8)

#define VOP2_VERSION(major, minor, build)	((major) << 24 | (minor) << 16 | (build))
#define VOP2_MAJOR(version)		(((version) >> 24) & 0xff)
#define VOP2_MINOR(version)		(((version) >> 16) & 0xff)
#define VOP2_BUILD(version)		((version) & 0xffff)

/* The new SOC VOP version is bigger than the old */
#define VOP_VERSION_RK3568	VOP2_VERSION(0x40, 0x15, 0x8023)
#define VOP_VERSION_RK3588	VOP2_VERSION(0x40, 0x17, 0x6786)
#define VOP_VERSION_RK3528	VOP2_VERSION(0x50, 0x17, 0x1263)
#define VOP_VERSION_RK3562	VOP2_VERSION(0x50, 0x17, 0x4350)
#define VOP_VERSION_RK3576	VOP2_VERSION(0x50, 0x19, 0x9765)

/* register one connector */
#define ROCKCHIP_OUTPUT_DUAL_CHANNEL_LEFT_RIGHT_MODE	BIT(0)
/* register one connector */
#define ROCKCHIP_OUTPUT_DUAL_CHANNEL_ODD_EVEN_MODE	BIT(1)
#define ROCKCHIP_OUTPUT_DATA_SWAP			BIT(2)
/* MIPI DSI DataStream(cmd) mode on rk3588 */
#define ROCKCHIP_OUTPUT_MIPI_DS_MODE			BIT(3)
/* register two connector */
#define ROCKCHIP_OUTPUT_DUAL_CONNECTOR_SPLIT_MODE	BIT(4)

#define AFBDC_FMT_RGB565	0x0
#define AFBDC_FMT_U8U8U8U8	0x5
#define AFBDC_FMT_U8U8U8	0x4

#define VOP_FEATURE_OUTPUT_RGB10	BIT(0)
#define VOP_FEATURE_INTERNAL_RGB	BIT(1)
#define VOP_FEATURE_ALPHA_SCALE		BIT(2)
#define VOP_FEATURE_HDR10		BIT(3)
#define VOP_FEATURE_DOVI		BIT(4)
/* a feature to splice two windows and two vps to support resolution > 4096 */
#define VOP_FEATURE_SPLICE		BIT(5)
#define VOP_FEATURE_OVERSCAN		BIT(6)
#define VOP_FEATURE_VIVID_HDR		BIT(7)
#define VOP_FEATURE_POST_ACM		BIT(8)
#define VOP_FEATURE_POST_CSC		BIT(9)
#define VOP_FEATURE_POST_FRC_V2		BIT(10)
#define VOP_FEATURE_POST_SHARP		BIT(11)
#define VOP_FEATURE_HW_CURSOR		BIT(12)

#define VOP_FEATURE_OUTPUT_10BIT	VOP_FEATURE_OUTPUT_RGB10


#define WIN_FEATURE_HDR2SDR		BIT(0)
#define WIN_FEATURE_SDR2HDR		BIT(1)
#define WIN_FEATURE_PRE_OVERLAY		BIT(2)
#define WIN_FEATURE_AFBDC		BIT(3)
#define WIN_FEATURE_CLUSTER_MAIN	BIT(4)
#define WIN_FEATURE_CLUSTER_SUB		BIT(5)
/* Left win in splice mode */
#define WIN_FEATURE_SPLICE_LEFT		BIT(6)
/* a mirror win can only get fb address
 * from source win:
 * Cluster1---->Cluster0
 * Esmart1 ---->Esmart0
 * Smart1  ---->Smart0
 * This is a feather on rk3566
 */
#define WIN_FEATURE_MIRROR		BIT(6)
#define WIN_FEATURE_MULTI_AREA		BIT(7)
#define WIN_FEATURE_Y2R_13BIT_DEPTH	BIT(8)
#define WIN_FEATURE_DCI			BIT(9)
#define WIN_FEATURE_HW_CURSOR		BIT(10)


#define VOP2_SOC_VARIANT		4

#define ROCKCHIP_DSC_PPS_SIZE_BYTE	88

enum bcsh_out_mode {
	BCSH_OUT_MODE_BLACK,
	BCSH_OUT_MODE_BLUE,
	BCSH_OUT_MODE_COLOR_BAR,
	BCSH_OUT_MODE_NORMAL_VIDEO,
};

enum cabc_stage_mode {
	LAST_FRAME_PWM_VAL	= 0x0,
	CUR_FRAME_PWM_VAL	= 0x1,
	STAGE_BY_STAGE		= 0x2
};

enum cabc_stage_up_mode {
	MUL_MODE,
	ADD_MODE,
};

/*
 *  the delay number of a window in different mode.
 */
enum vop2_win_dly_mode {
	VOP2_DLY_MODE_DEFAULT,   /**< default mode */
	VOP2_DLY_MODE_HISO_S,    /** HDR in SDR out mode, as a SDR window */
	VOP2_DLY_MODE_HIHO_H,    /** HDR in HDR out mode, as a HDR window */
	VOP2_DLY_MODE_DOVI_IN_CORE1,	/*  dovi video input, as dovi core1 */
	VOP2_DLY_MODE_DOVI_IN_CORE2,	/*  dovi video input, as dovi core2 */
	VOP2_DLY_MODE_NONDOVI_IN_CORE1,	/*  ndovi video input, as dovi core1 */
	VOP2_DLY_MODE_NONDOVI_IN_CORE2,	/*  ndovi video input, as dovi core2 */
	VOP2_DLY_MODE_MAX,
};

enum vop3_esmart_lb_mode {
	VOP3_ESMART_8K_MODE,
	VOP3_ESMART_4K_4K_MODE,
	VOP3_ESMART_4K_2K_2K_MODE,
	VOP3_ESMART_2K_2K_2K_2K_MODE,
	VOP3_ESMART_4K_4K_4K_MODE,
	VOP3_ESMART_4K_4K_2K_2K_MODE,
};

/**
 * enum rockchip_drm_crc_source: CRC source
 * @ROCKCHIP_DRM_CRC_SOURCE_AUTO: no source set
 * @ROCKCHIP_DRM_CRC_SOURCE_PLANE: CRC in layer
 * @ROCKCHIP_DRM_CRC_SOURCE_CRTC: CRC in vp
 * @ROCKCHIP_DRM_CRC_SOURCE_ENCODER: CRC in encoder
 * @ROCKCHIP_DRM_CRC_SOURCE_INVALID: Invalid source
 */
enum rockchip_drm_crc_source {
	ROCKCHIP_DRM_CRC_SOURCE_AUTO = 0,
	ROCKCHIP_DRM_CRC_SOURCE_PLANE,
	ROCKCHIP_DRM_CRC_SOURCE_CRTC,
	ROCKCHIP_DRM_CRC_SOURCE_ENCODER,
	ROCKCHIP_DRM_CRC_SOURCE_INVALID = -1
};

/*
 * vop2 dsc id
 */
#define ROCKCHIP_VOP2_DSC_8K	0
#define ROCKCHIP_VOP2_DSC_4K	1

/*
 * vop2 internal power domain id,
 * should be all none zero, 0 will be
 * treat as invalid;
 */
#define VOP2_PD_CLUSTER0	BIT(0)
#define VOP2_PD_CLUSTER1	BIT(1)
#define VOP2_PD_CLUSTER2	BIT(2)
#define VOP2_PD_CLUSTER3	BIT(3)
#define VOP2_PD_DSC_8K		BIT(5)
#define VOP2_PD_DSC_4K		BIT(6)
#define VOP2_PD_ESMART		BIT(7)
#define VOP2_PD_CLUSTER		BIT(8)

/*
 * vop2 submem power gate,
 * should be all none zero, 0 will be
 * treat as invalid;
 */
#define VOP2_MEM_PG_VP0		BIT(0)
#define VOP2_MEM_PG_VP1		BIT(1)
#define VOP2_MEM_PG_VP2		BIT(2)
#define VOP2_MEM_PG_VP3		BIT(3)
#define VOP2_MEM_PG_DB0		BIT(4)
#define VOP2_MEM_PG_DB1		BIT(5)
#define VOP2_MEM_PG_DB2		BIT(6)
#define VOP2_MEM_PG_WB		BIT(7)

#define DSP_BG_SWAP		0x1
#define DSP_RB_SWAP		0x2
#define DSP_RG_SWAP		0x4
#define DSP_DELTA_SWAP		0x8

#define V4L2_COLORSPACE_BT709F	0xfe
#define V4L2_COLORSPACE_BT2020F	0xff

enum vop_csc_format {
	CSC_BT601L,
	CSC_BT709L,
	CSC_BT601F,
	CSC_BT2020L,
	CSC_BT709L_13BIT,
	CSC_BT709F_13BIT,
	CSC_BT2020L_13BIT,
	CSC_BT2020F_13BIT,
};

enum vop_csc_mode {
	CSC_RGB,
	CSC_YUV,
};

enum vop_csc_bit_depth {
	CSC_10BIT_DEPTH,
	CSC_13BIT_DEPTH,
};

enum vop_data_format {
	VOP_FMT_ARGB8888 = 0,
	VOP_FMT_RGB888,
	VOP_FMT_RGB565 = 2,
	VOP_FMT_YUYV = 2,
	VOP_FMT_YUV420SP = 4,
	VOP_FMT_YUV422SP,
	VOP_FMT_YUV444SP,
};

enum vop_dsc_interface_mode {
	VOP_DSC_IF_DISABLE = 0,
	VOP_DSC_IF_HDMI = 1,
	VOP_DSC_IF_MIPI_DS_MODE = 2,
	VOP_DSC_IF_MIPI_VIDEO_MODE = 3,
};

struct vop_reg_data {
	uint32_t offset;
	uint32_t value;
};

struct vop_reg {
	uint32_t mask;
	uint32_t offset:19;
	uint32_t shift:5;
	uint32_t begin_minor:4;
	uint32_t end_minor:4;
	uint32_t major:3;
	uint32_t write_mask:1;
};

struct vop_afbc {
	struct vop_reg enable;
	struct vop_reg win_sel;
	struct vop_reg format;
	struct vop_reg rb_swap;
	struct vop_reg uv_swap;
	struct vop_reg auto_gating_en;
	struct vop_reg rotate;
	struct vop_reg block_split_en;
	struct vop_reg pic_vir_width;
	struct vop_reg tile_num;
	struct vop_reg pic_offset;
	struct vop_reg pic_size;
	struct vop_reg dsp_offset;
	struct vop_reg transform_offset;
	struct vop_reg hdr_ptr;
	struct vop_reg half_block_en;
	struct vop_reg pld_offset_en;
	struct vop_reg pld_ptr_offset;
	struct vop_reg pld_range_en;
	struct vop_reg pld_ptr_range;
	struct vop_reg xmirror;
	struct vop_reg ymirror;
	struct vop_reg rotate_270;
	struct vop_reg rotate_90;
	struct vop_reg rstn;
};

struct vop_csc {
	struct vop_reg y2r_en;
	struct vop_reg r2r_en;
	struct vop_reg r2y_en;
	struct vop_reg csc_mode;

	uint32_t y2r_offset;
	uint32_t r2r_offset;
	uint32_t r2y_offset;
};

struct vop_rect {
	int width;
	int height;
};

struct vop_ctrl {
	struct vop_reg version;
	struct vop_reg standby;
	struct vop_reg dma_stop;
	struct vop_reg axi_outstanding_max_num;
	struct vop_reg axi_max_outstanding_en;
	struct vop_reg htotal_pw;
	struct vop_reg hact_st_end;
	struct vop_reg vtotal_pw;
	struct vop_reg vact_st_end;
	struct vop_reg vact_st_end_f1;
	struct vop_reg vs_st_end_f1;
	struct vop_reg hpost_st_end;
	struct vop_reg vpost_st_end;
	struct vop_reg vpost_st_end_f1;
	struct vop_reg post_scl_factor;
	struct vop_reg post_scl_ctrl;
	struct vop_reg dsp_interlace;
	struct vop_reg dsp_interlace_pol;
	struct vop_reg global_regdone_en;
	struct vop_reg auto_gate_en;
	struct vop_reg post_lb_mode;
	struct vop_reg dsp_layer_sel;
	struct vop_reg overlay_mode;
	struct vop_reg core_dclk_div;
	struct vop_reg dclk_ddr;
	struct vop_reg p2i_en;
	struct vop_reg hdmi_dclk_out_en;
	struct vop_reg rgb_en;
	struct vop_reg lvds_en;
	struct vop_reg edp_en;
	struct vop_reg hdmi_en;
	struct vop_reg mipi_en;
	struct vop_reg data01_swap;
	struct vop_reg mipi_dual_channel_en;
	struct vop_reg dp_en;
	struct vop_reg dclk_pol;
	struct vop_reg pin_pol;
	struct vop_reg rgb_dclk_pol;
	struct vop_reg rgb_pin_pol;
	struct vop_reg lvds_dclk_pol;
	struct vop_reg lvds_pin_pol;
	struct vop_reg hdmi_dclk_pol;
	struct vop_reg hdmi_pin_pol;
	struct vop_reg edp_dclk_pol;
	struct vop_reg edp_pin_pol;
	struct vop_reg mipi_dclk_pol;
	struct vop_reg mipi_pin_pol;
	struct vop_reg dp_dclk_pol;
	struct vop_reg dp_pin_pol;
	struct vop_reg dither_down_sel;
	struct vop_reg dither_down_mode;
	struct vop_reg dither_down_en;
	struct vop_reg pre_dither_down_en;
	struct vop_reg dither_up_en;

	struct vop_reg sw_dac_sel;
	struct vop_reg tve_sw_mode;
	struct vop_reg tve_dclk_pol;
	struct vop_reg tve_dclk_en;
	struct vop_reg sw_genlock;
	struct vop_reg sw_uv_offset_en;
	struct vop_reg dsp_out_yuv;
	struct vop_reg dsp_data_swap;
	struct vop_reg dsp_bg_swap;
	struct vop_reg dsp_rb_swap;
	struct vop_reg dsp_rg_swap;
	struct vop_reg dsp_delta_swap;
	struct vop_reg dsp_dummy_swap;
	struct vop_reg yuv_clip;
	struct vop_reg dsp_ccir656_avg;
	struct vop_reg dsp_black;
	struct vop_reg dsp_blank;
	struct vop_reg dsp_outzero;
	struct vop_reg update_gamma_lut;
	struct vop_reg lut_buffer_index;
	struct vop_reg dsp_lut_en;

	struct vop_reg out_mode;

	struct vop_reg xmirror;
	struct vop_reg ymirror;
	struct vop_reg dsp_background;

	/* AFBDC */
	struct vop_reg afbdc_en;
	struct vop_reg afbdc_sel;
	struct vop_reg afbdc_format;
	struct vop_reg afbdc_hreg_block_split;
	struct vop_reg afbdc_pic_size;
	struct vop_reg afbdc_hdr_ptr;
	struct vop_reg afbdc_rstn;
	struct vop_reg afbdc_pic_vir_width;
	struct vop_reg afbdc_pic_offset;
	struct vop_reg afbdc_axi_ctrl;

	/* BCSH */
	struct vop_reg bcsh_brightness;
	struct vop_reg bcsh_contrast;
	struct vop_reg bcsh_sat_con;
	struct vop_reg bcsh_sin_hue;
	struct vop_reg bcsh_cos_hue;
	struct vop_reg bcsh_r2y_csc_mode;
	struct vop_reg bcsh_r2y_en;
	struct vop_reg bcsh_y2r_csc_mode;
	struct vop_reg bcsh_y2r_en;
	struct vop_reg bcsh_color_bar;
	struct vop_reg bcsh_out_mode;
	struct vop_reg bcsh_en;

	/* HDR */
	struct vop_reg level2_overlay_en;
	struct vop_reg alpha_hard_calc;
	struct vop_reg hdr2sdr_en;
	struct vop_reg hdr2sdr_en_win0_csc;
	struct vop_reg hdr2sdr_src_min;
	struct vop_reg hdr2sdr_src_max;
	struct vop_reg hdr2sdr_normfaceetf;
	struct vop_reg hdr2sdr_dst_min;
	struct vop_reg hdr2sdr_dst_max;
	struct vop_reg hdr2sdr_normfacgamma;

	struct vop_reg bt1886eotf_pre_conv_en;
	struct vop_reg rgb2rgb_pre_conv_en;
	struct vop_reg rgb2rgb_pre_conv_mode;
	struct vop_reg st2084oetf_pre_conv_en;
	struct vop_reg bt1886eotf_post_conv_en;
	struct vop_reg rgb2rgb_post_conv_en;
	struct vop_reg rgb2rgb_post_conv_mode;
	struct vop_reg st2084oetf_post_conv_en;
	struct vop_reg win_csc_mode_sel;

	/* MCU OUTPUT */
	struct vop_reg mcu_pix_total;
	struct vop_reg mcu_cs_pst;
	struct vop_reg mcu_cs_pend;
	struct vop_reg mcu_rw_pst;
	struct vop_reg mcu_rw_pend;
	struct vop_reg mcu_clk_sel;
	struct vop_reg mcu_hold_mode;
	struct vop_reg mcu_frame_st;
	struct vop_reg mcu_rs;
	struct vop_reg mcu_bypass;
	struct vop_reg mcu_type;
	struct vop_reg mcu_rw_bypass_port;
	struct vop_reg mcu_force_rdn;
	struct vop_reg mcu_data_map_mode;

	/* bt1120 */
	struct vop_reg bt1120_uv_swap;
	struct vop_reg bt1120_yc_swap;
	struct vop_reg bt1120_en;
	struct vop_reg bt1120_data_map_mode;

	/* bt656 */
	struct vop_reg bt656_en;
	struct vop_reg bt656_data_map_mode;

	struct vop_reg reg_done_frm;
	struct vop_reg cfg_done;

	struct vop_reg edpi_wms_fs;
	struct vop_reg edpi_ctrl_mode;
	struct vop_reg edpi_te_en;

	/* ebc vop */
	struct vop_reg enable;
	struct vop_reg inf_out_en;
	struct vop_reg out_dresetn;

	/* color bar */
	struct vop_reg color_bar_en;
	struct vop_reg color_bar_mode;

	/* clk cnt */
	struct vop_reg calc_clk_en;
	struct vop_reg calc_dclk_cnt;
};

struct vop_intr {
	const int *intrs;
	uint32_t nintrs;

	struct vop_reg line_flag_num[2];
	struct vop_reg enable;
	struct vop_reg clear;
	struct vop_reg status;
};

struct vop_urgency {
	u8 urgen_thl;
	u8 urgen_thh;
};

struct vop_scl_extension {
	struct vop_reg cbcr_vsd_mode;
	struct vop_reg cbcr_vsu_mode;
	struct vop_reg cbcr_hsd_mode;
	struct vop_reg cbcr_ver_scl_mode;
	struct vop_reg cbcr_hor_scl_mode;
	struct vop_reg yrgb_vsd_mode;
	struct vop_reg yrgb_vsu_mode;
	struct vop_reg yrgb_hsd_mode;
	struct vop_reg yrgb_ver_scl_mode;
	struct vop_reg yrgb_hor_scl_mode;
	struct vop_reg line_load_mode;
	struct vop_reg cbcr_axi_gather_num;
	struct vop_reg yrgb_axi_gather_num;
	struct vop_reg vsd_cbcr_gt2;
	struct vop_reg vsd_cbcr_gt4;
	struct vop_reg vsd_yrgb_gt2;
	struct vop_reg vsd_yrgb_gt4;
	struct vop_reg bic_coe_sel;
	struct vop_reg cbcr_axi_gather_en;
	struct vop_reg yrgb_axi_gather_en;
	struct vop_reg lb_mode;
};

struct vop_scl_regs {
	const struct vop_scl_extension *ext;

	struct vop_reg scale_yrgb_x;
	struct vop_reg scale_yrgb_y;
	struct vop_reg scale_cbcr_x;
	struct vop_reg scale_cbcr_y;
};

struct vop_csc_table {
	const uint32_t *y2r_bt601;
	const uint32_t *y2r_bt601_12_235;
	const uint32_t *y2r_bt601_10bit;
	const uint32_t *y2r_bt601_10bit_12_235;
	const uint32_t *r2y_bt601;
	const uint32_t *r2y_bt601_12_235;
	const uint32_t *r2y_bt601_10bit;
	const uint32_t *r2y_bt601_10bit_12_235;

	const uint32_t *y2r_bt709;
	const uint32_t *y2r_bt709_10bit;
	const uint32_t *r2y_bt709;
	const uint32_t *r2y_bt709_10bit;

	const uint32_t *y2r_bt2020;
	const uint32_t *r2y_bt2020;

	const uint32_t *r2r_bt709_to_bt2020;
	const uint32_t *r2r_bt2020_to_bt709;
};

struct vop_hdr_table {
	const uint32_t hdr2sdr_eetf_oetf_y0_offset;
	const uint32_t hdr2sdr_eetf_oetf_y1_offset;
	const uint32_t *hdr2sdr_eetf_yn;
	const uint32_t *hdr2sdr_bt1886oetf_yn;
	const uint32_t hdr2sdr_sat_y0_offset;
	const uint32_t hdr2sdr_sat_y1_offset;
	const uint32_t *hdr2sdr_sat_yn;

	const uint32_t hdr2sdr_src_range_min;
	const uint32_t hdr2sdr_src_range_max;
	const uint32_t hdr2sdr_normfaceetf;
	const uint32_t hdr2sdr_dst_range_min;
	const uint32_t hdr2sdr_dst_range_max;
	const uint32_t hdr2sdr_normfacgamma;

	const uint32_t sdr2hdr_eotf_oetf_y0_offset;
	const uint32_t sdr2hdr_eotf_oetf_y1_offset;
	const uint32_t *sdr2hdr_bt1886eotf_yn_for_hlg_hdr;
	const uint32_t *sdr2hdr_bt1886eotf_yn_for_bt2020;
	const uint32_t *sdr2hdr_bt1886eotf_yn_for_hdr;
	const uint32_t *sdr2hdr_st2084oetf_yn_for_hlg_hdr;
	const uint32_t *sdr2hdr_st2084oetf_yn_for_bt2020;
	const uint32_t *sdr2hdr_st2084oetf_yn_for_hdr;
	const uint32_t sdr2hdr_oetf_dx_dxpow1_offset;
	const uint32_t *sdr2hdr_st2084oetf_dxn_pow2;
	const uint32_t *sdr2hdr_st2084oetf_dxn;
	const uint32_t sdr2hdr_oetf_xn1_offset;
	const uint32_t *sdr2hdr_st2084oetf_xn;
};

#define RK_HDRVIVID_TONE_SCA_TAB_LENGTH		257
#define RK_HDRVIVID_GAMMA_CURVE_LENGTH		81
#define RK_HDRVIVID_GAMMA_MDFVALUE_LENGTH	9
#define RK_SDR2HDR_INVGAMMA_CURVE_LENGTH	69
#define RK_SDR2HDR_INVGAMMA_S_IDX_LENGTH	6
#define RK_SDR2HDR_INVGAMMA_C_IDX_LENGTH	6
#define RK_SDR2HDR_SMGAIN_LENGTH		64
#define RK_HDRVIVID_TONE_SCA_AXI_TAB_LENGTH	264

struct hdrvivid_regs {
	uint32_t sdr2hdr_ctrl;
	uint32_t sdr2hdr_coe0;
	uint32_t sdr2hdr_coe1;
	uint32_t sdr2hdr_csc_coe00_01;
	uint32_t sdr2hdr_csc_coe02_10;
	uint32_t sdr2hdr_csc_coe11_12;
	uint32_t sdr2hdr_csc_coe20_21;
	uint32_t sdr2hdr_csc_coe22;
	uint32_t hdrvivid_ctrl;
	uint32_t hdr_pq_gamma;
	uint32_t hlg_rfix_scalefac;
	uint32_t hlg_maxluma;
	uint32_t hlg_r_tm_lin2non;
	uint32_t hdr_csc_coe00_01;
	uint32_t hdr_csc_coe02_10;
	uint32_t hdr_csc_coe11_12;
	uint32_t hdr_csc_coe20_21;
	uint32_t hdr_csc_coe22;
	uint32_t hdr_tone_sca[RK_HDRVIVID_TONE_SCA_TAB_LENGTH];
	uint32_t hdrgamma_curve[RK_HDRVIVID_GAMMA_CURVE_LENGTH];
	uint32_t hdrgamma_mdfvalue[RK_HDRVIVID_GAMMA_MDFVALUE_LENGTH];
	uint32_t sdrinvgamma_curve[RK_SDR2HDR_INVGAMMA_CURVE_LENGTH];
	uint32_t sdrinvgamma_startidx[RK_SDR2HDR_INVGAMMA_S_IDX_LENGTH];
	uint32_t sdrinvgamma_changeidx[RK_SDR2HDR_INVGAMMA_C_IDX_LENGTH];
	uint32_t sdr_smgain[RK_SDR2HDR_SMGAIN_LENGTH];
	uint32_t hdr_mode;
	uint32_t tone_sca_axi_tab[RK_HDRVIVID_TONE_SCA_AXI_TAB_LENGTH];
};

#define RK_HDR_TYPE_MASK 0xff
#define RK_HDR_PLAT_MASK (0xff << 8)

/* byte unit */
#define VOP2_DOVI_CORE1_LUT_SIZE		5120
#define VOP2_DOVI_TONE_SCA_AXI_TAB_SIZE		(2560 * 4)

/* word unit */
#define DOVI_LUT_SIZE				1280
#define DOVI_CORE1_SIZE				242
#define DOVI_CORE2_SIZE				43
#define DOVI_CORE3_SIZE				256

enum vop_dovi_input_type {
	COMMON_LAYER = 0,
	DOVI_BASE_LAYER = 1,
	DOVI_ENHANCE_LAYER = 2,
};

struct dovi_regs {
	uint32_t version;
	uint32_t valid;
	uint32_t input_mode;
	uint32_t output_mode;
	uint32_t core1_lut[DOVI_LUT_SIZE];
	uint32_t core2_lut[DOVI_LUT_SIZE];
	uint32_t core1[DOVI_CORE1_SIZE];
	uint32_t core2[DOVI_CORE2_SIZE];
	uint32_t core3[DOVI_CORE3_SIZE];
};

struct hdr_extend {
	uint32_t hdr_type;
	uint32_t length;
	union {
		struct hdrvivid_regs hdrvivid_data;
		struct dovi_regs dovi_data;
	};
};

enum _vop_hdrvivid_mode {
	PQHDR2HDR_WITH_DYNAMIC = 0,
	PQHDR2SDR_WITH_DYNAMIC,
	HLG2PQHDR_WITH_DYNAMIC,
	HLG2SDR_WITH_DYNAMIC,
	HLG2PQHDR_WITHOUT_DYNAMIC,
	HLG2SDR_WITHOUT_DYNAMIC,
	HDR_BYPASS,
	HDR102SDR,
	SDR2HDR10,
	SDR2HLG,
	SDR2HDR10_USERSPACE = 100,
	SDR2HLG_USERSPACE = 101,
};

enum vop_hdr_format {
	HDR_NONE = 0,
	HDR_HDR10 = 1,
	HDR_HLGSTATIC = 2,
	RESERVED3 = 3,		/* reserved for more future static hdr format */
	RESERVED4 = 4,		/* reserved for more future static hdr format */
	HDR_HDRVIVID = 5,
	RESERVED6 = 6,		/* reserved for hdr vivid */
	RESERVED7 = 7,		/* reserved for hdr vivid */
	HDR_HDR10PLUS = 8,
	RESERVED9 = 9,		/* reserved for hdr hdr10+ */
	RESERVED10 = 10,	/* reserved for hdr hdr10+ */
	HDR_DOVI = 11,
	RESERVED12 = 12,	/* reserved for other dynamic hdr format */
	RESERVED13 = 13,	/* reserved for other dynamic hdr format */
	HDR_FORMAT_MAX,
};

struct post_csc_convert_mode {
	enum drm_color_encoding intput_color_encoding;
	enum drm_color_encoding output_color_encoding;
	bool is_input_yuv;
	bool is_output_yuv;
	bool is_input_full_range;
	bool is_output_full_range;
};

struct post_csc_coef {
	s32 csc_coef00;
	s32 csc_coef01;
	s32 csc_coef02;
	s32 csc_coef10;
	s32 csc_coef11;
	s32 csc_coef12;
	s32 csc_coef20;
	s32 csc_coef21;
	s32 csc_coef22;

	s32 csc_dc0;
	s32 csc_dc1;
	s32 csc_dc2;

	u32 range_type;
};

enum {
	VOP_CSC_Y2R_BT601,
	VOP_CSC_Y2R_BT709,
	VOP_CSC_Y2R_BT2020,
	VOP_CSC_R2Y_BT601,
	VOP_CSC_R2Y_BT709,
	VOP_CSC_R2Y_BT2020,
	VOP_CSC_R2R_BT2020_TO_BT709,
	VOP_CSC_R2R_BT709_TO_2020,
};

enum _vop_overlay_mode {
	VOP_RGB_DOMAIN,
	VOP_YUV_DOMAIN
};

enum _vop_sdr2hdr_func {
	SDR2HDR_FOR_BT2020,
	SDR2HDR_FOR_HDR,
	SDR2HDR_FOR_HLG_HDR,
};

enum _vop_rgb2rgb_conv_mode {
	BT709_TO_BT2020,
	BT2020_TO_BT709,
};

enum _MCU_IOCTL {
	MCU_WRCMD = 0,
	MCU_WRDATA,
	MCU_RDDATA,
	MCU_SETBYPASS,
};

struct vop_win_phy {
	const struct vop_scl_regs *scl;
	const uint32_t *data_formats;
	uint32_t nformats;

	struct vop_reg enable;
	struct vop_reg gate;
	struct vop_reg format;
	struct vop_reg interlace_read;
	struct vop_reg fmt_10;
	struct vop_reg fmt_yuyv;
	struct vop_reg csc_mode;
	struct vop_reg xmirror;
	struct vop_reg ymirror;
	struct vop_reg rb_swap;
	struct vop_reg uv_swap;
	struct vop_reg act_info;
	struct vop_reg dsp_info;
	struct vop_reg dsp_st;
	struct vop_reg yrgb_mst;
	struct vop_reg uv_mst;
	struct vop_reg yrgb_vir;
	struct vop_reg uv_vir;

	struct vop_reg dst_alpha_ctl;
	struct vop_reg src_alpha_ctl;
	struct vop_reg alpha_pre_mul;
	struct vop_reg alpha_mode;
	struct vop_reg alpha_en;
	struct vop_reg channel;
	struct vop_reg global_alpha_val;
	struct vop_reg color_key;
	struct vop_reg color_key_en;
};

struct vop_win_data {
	uint32_t base;
	enum drm_plane_type type;
	const struct vop_win_phy *phy;
	const struct vop_win_phy **area;
	const uint64_t *format_modifiers;
	const struct vop_csc *csc;
	unsigned int area_size;
	u64 feature;
};

struct vop2_cluster_regs {
	struct vop_reg enable;
	struct vop_reg afbc_enable;
	struct vop_reg lb_mode;
	struct vop_reg scl_lb_mode;
	struct vop_reg frm_reset_en;
	struct vop_reg dma_stride_4k_disable;

	struct vop_reg blk_size_h;
	struct vop_reg blk_size_v;
	struct vop_reg blk_offset_h;
	struct vop_reg blk_offset_v;
	struct vop_reg blk_size_fix;
	struct vop_reg pix_region_start_h;
	struct vop_reg pix_region_start_v;
	struct vop_reg sat_adj_zero;
	struct vop_reg sat_adj_thr;
	struct vop_reg sat_adj_k;
	struct vop_reg sat_w;
	struct vop_reg dci_en;
	struct vop_reg uv_adjust_en;
	struct vop_reg csc_range;
	struct vop_reg dci_dma_rid;
	struct vop_reg dci_dma_rlen;
	struct vop_reg dci_dma_mst;
	struct vop_reg debug_point_h;
	struct vop_reg debug_point_v;
	struct vop_reg debug_mode;
	struct vop_reg debug_en;

	struct vop_reg src_color_ctrl;
	struct vop_reg dst_color_ctrl;
	struct vop_reg src_alpha_ctrl;
	struct vop_reg dst_alpha_ctrl;
};

struct vop2_scl_regs {
	struct vop_reg scale_yrgb_x;
	struct vop_reg scale_yrgb_y;
	struct vop_reg scale_cbcr_x;
	struct vop_reg scale_cbcr_y;
	struct vop_reg yrgb_hor_scl_mode;
	struct vop_reg yrgb_hscl_filter_mode;
	struct vop_reg yrgb_ver_scl_mode;
	struct vop_reg yrgb_vscl_filter_mode;
	struct vop_reg cbcr_ver_scl_mode;
	struct vop_reg cbcr_hscl_filter_mode;
	struct vop_reg cbcr_hor_scl_mode;
	struct vop_reg cbcr_vscl_filter_mode;
	struct vop_reg zme_dering_en;
	struct vop_reg zme_dering_para;
	struct vop_reg vsd_cbcr_gt2;
	struct vop_reg vsd_cbcr_gt4;
	struct vop_reg vsd_yrgb_gt2;
	struct vop_reg vsd_yrgb_gt4;
	struct vop_reg bic_coe_sel;
	struct vop_reg xavg_en; /* supported from vop3 */
	struct vop_reg xgt_en;
	struct vop_reg xgt_mode;
	struct vop_reg vsd_avg2;
	struct vop_reg vsd_avg4;
};

struct vop2_win_regs {
	const struct vop2_scl_regs *scl;
	const struct vop2_cluster_regs *cluster;
	const struct vop_afbc *afbc;

	struct vop_reg gate;
	struct vop_reg enable;
	struct vop_reg format;
	struct vop_reg format_argb1555;
	struct vop_reg tile_mode;
	struct vop_reg csc_mode;
	struct vop_reg csc_13bit_en;
	struct vop_reg xmirror;
	struct vop_reg ymirror;
	struct vop_reg rb_swap;
	struct vop_reg uv_swap;
	struct vop_reg rg_swap;
	struct vop_reg act_info;
	struct vop_reg dsp_info;
	struct vop_reg dsp_st;
	struct vop_reg yrgb_mst;
	struct vop_reg uv_mst;
	struct vop_reg yrgb_vir;
	struct vop_reg uv_vir;
	struct vop_reg yuv_clip;
	struct vop_reg lb_mode;
	struct vop_reg y2r_en;
	struct vop_reg csc_y2r_path_sel;
	struct vop_reg r2y_en;
	struct vop_reg channel;
	struct vop_reg dst_alpha_ctl;
	struct vop_reg src_alpha_ctl;
	struct vop_reg alpha_mode;
	struct vop_reg alpha_en;
	struct vop_reg global_alpha_val;
	struct vop_reg color_key;
	struct vop_reg color_key_en;
	struct vop_reg background;
	struct vop_reg dither_up;
	struct vop_reg axi_id;
	struct vop_reg axi_yrgb_id;
	struct vop_reg axi_uv_id;
	struct vop_reg scale_engine_num;
	struct vop_reg alpha_map_en;
	struct vop_reg alpha_map_val;
};

struct vop2_video_port_regs {
	struct vop_reg cfg_done;
	struct vop_reg sys_cfg_done;
	struct vop_reg overlay_mode;
	struct vop_reg dsp_background;
	struct vop_reg port_mux;
	struct vop_reg out_mode;
	struct vop_reg standby;
	struct vop_reg dsp_interlace;
	struct vop_reg dsp_filed_pol;
	struct vop_reg dsp_data_swap;
	struct vop_reg dsp_x_mir_en;
	struct vop_reg post_dsp_out_r2y;
	struct vop_reg pre_scan_htiming;
	struct vop_reg dovi_pre_scan_en;
	struct vop_reg pre_scan_htiming1;
	struct vop_reg pre_scan_htiming2;
	struct vop_reg pre_scan_htiming3;
	struct vop_reg htotal_pw;
	struct vop_reg hact_st_end;
	struct vop_reg dsp_vtotal;
	struct vop_reg sw_dsp_vtotal_imd;
	struct vop_reg dsp_vs_end;
	struct vop_reg vact_st_end;
	struct vop_reg vact_st_end_f1;
	struct vop_reg vs_st_end_f1;
	struct vop_reg hpost_st_end;
	struct vop_reg vpost_st_end;
	struct vop_reg vpost_st_end_f1;
	struct vop_reg post_scl_factor;
	struct vop_reg post_scl_ctrl;
	struct vop_reg dither_down_sel;
	struct vop_reg dither_down_mode;
	struct vop_reg dither_down_en;
	struct vop_reg pre_dither_down_en;
	struct vop_reg dither_frc_0;
	struct vop_reg dither_frc_1;
	struct vop_reg dither_frc_2;
	struct vop_reg dither_up_en;
	struct vop_reg bg_dly;
	struct vop_reg dp_line_end_mode;
	struct vop_reg dp_bg_bottom_disable;

	struct vop_reg p2i_en;
	struct vop_reg dual_channel_en;
	struct vop_reg dual_channel_swap;
	struct vop_reg dsp_lut_en;

	struct vop_reg core_dclk_div;		/* dclk core */
	struct vop_reg dclk_div2;		/* dclk out */

	struct vop_reg dclk_div2_phase_lock;	/* used to adjust phase when yuv420 output */

	struct vop_reg hdr10_en;
	struct vop_reg hdr_lut_update_en;
	struct vop_reg hdr_lut_mode;
	struct vop_reg hdr_lut_mst;
	struct vop_reg hdr_lut_fetch_done;
	struct vop_reg hdr_vivid_en;
	struct vop_reg hdr_vivid_bypass_en;
	struct vop_reg hdr_vivid_path_mode;
	struct vop_reg hdr_vivid_dstgamut;
	struct vop_reg sdr2hdr_en;
	struct vop_reg sdr2hdr_dstmode;
	struct vop_reg sdr2hdr_eotf_en;
	struct vop_reg sdr2hdr_r2r_en;
	struct vop_reg sdr2hdr_r2r_mode;
	struct vop_reg sdr2hdr_oetf_en;
	struct vop_reg sdr2hdr_bypass_en;
	struct vop_reg sdr2hdr_auto_gating_en;
	struct vop_reg sdr2hdr_path_en;
	struct vop_reg hdr2sdr_en;
	struct vop_reg hdr2sdr_bypass_en;
	struct vop_reg hdr2sdr_auto_gating_en;
	struct vop_reg hdr2sdr_src_min;
	struct vop_reg hdr2sdr_src_max;
	struct vop_reg hdr2sdr_normfaceetf;
	struct vop_reg hdr2sdr_dst_min;
	struct vop_reg hdr2sdr_dst_max;
	struct vop_reg hdr2sdr_normfacgamma;
	uint32_t hdr2sdr_eetf_oetf_y0_offset;
	uint32_t hdr2sdr_sat_y0_offset;
	uint32_t sdr2hdr_eotf_oetf_y0_offset;
	uint32_t sdr2hdr_oetf_dx_pow1_offset;
	uint32_t sdr2hdr_oetf_xn1_offset;
	struct vop_reg hdr_src_color_ctrl;
	struct vop_reg hdr_dst_color_ctrl;
	struct vop_reg hdr_src_alpha_ctrl;
	struct vop_reg hdr_dst_alpha_ctrl;
	struct vop_reg port_extra_en;
	struct vop_reg bg_mix_ctrl;
	struct vop_reg layer_sel;

	/* BCSH */
	struct vop_reg bcsh_brightness;
	struct vop_reg bcsh_contrast;
	struct vop_reg bcsh_sat_con;
	struct vop_reg bcsh_sin_hue;
	struct vop_reg bcsh_cos_hue;
	struct vop_reg bcsh_r2y_csc_mode;
	struct vop_reg bcsh_r2y_en;
	struct vop_reg bcsh_y2r_csc_mode;
	struct vop_reg bcsh_y2r_en;
	struct vop_reg bcsh_out_mode;
	struct vop_reg bcsh_en;

	/* 3d lut */
	struct vop_reg cubic_lut_en;
	struct vop_reg cubic_lut_update_en;
	struct vop_reg cubic_lut_mst;

	/* cru */
	struct vop_reg dclk_core_div;
	struct vop_reg dclk_out_div;
	struct vop_reg dclk_src_sel;

	struct vop_reg splice_en;

	struct vop_reg edpi_wms_hold_en;
	struct vop_reg edpi_te_en;
	struct vop_reg edpi_wms_fs;
	struct vop_reg gamma_update_en;
	struct vop_reg lut_dma_rid;

	/* MCU output */
	struct vop_reg mcu_pix_total;
	struct vop_reg mcu_cs_pst;
	struct vop_reg mcu_cs_pend;
	struct vop_reg mcu_rw_pst;
	struct vop_reg mcu_rw_pend;
	struct vop_reg mcu_clk_sel;
	struct vop_reg mcu_hold_mode;
	struct vop_reg mcu_frame_st;
	struct vop_reg mcu_rs;
	struct vop_reg mcu_bypass;
	struct vop_reg mcu_type;
	struct vop_reg mcu_rw_bypass_port;

	/* for DCF */
	struct vop_reg line_flag_or_en;
	struct vop_reg dsp_hold_or_en;
	struct vop_reg almost_full_or_en;

	/* CSC */
	struct vop_reg acm_bypass_en;
	struct vop_reg csc_en;
	struct vop_reg acm_r2y_en;
	struct vop_reg csc_mode;
	struct vop_reg acm_r2y_mode;
	struct vop_reg csc_coe00;
	struct vop_reg csc_coe01;
	struct vop_reg csc_coe02;
	struct vop_reg csc_coe10;
	struct vop_reg csc_coe11;
	struct vop_reg csc_coe12;
	struct vop_reg csc_coe20;
	struct vop_reg csc_coe21;
	struct vop_reg csc_coe22;
	struct vop_reg csc_offset0;
	struct vop_reg csc_offset1;
	struct vop_reg csc_offset2;

	/* axi urgency */
	struct vop_reg axi0_port_urgency_en;
	struct vop_reg axi1_port_urgency_en;
	struct vop_reg post_urgency_en;
	struct vop_reg post_urgency_thl;
	struct vop_reg post_urgency_thh;

	/* color bar */
	struct vop_reg color_bar_en;
	struct vop_reg color_bar_mode;

	/* crc */
	struct vop_reg crc_en;
	struct vop_reg crc_val;
	struct vop_reg crc_check_en;
	struct vop_reg crc_check_val;

	/* clk calc*/
	struct vop_reg calc_clk_en;
	struct vop_reg calc_dclk_cnt;

	/* dsp vcnt */
	struct vop_reg dsp_vcnt;
};

struct vop2_power_domain_regs {
	struct vop_reg pd;
	struct vop_reg status;
	struct vop_reg bisr_en_status;
	struct vop_reg otp_bisr_en_status;
	struct vop_reg pmu_status;
};

struct vop2_dovi_regs {
	/* common */
	struct vop_reg enable;
	struct vop_reg interrupt_enable;
	struct vop_reg interrupt_raw;
	struct vop_reg metadata_program_st;
	struct vop_reg metadata_program_end;
	struct vop_reg metadata_copy_finish;

	/* core1 */
	struct vop_reg bypass_composer;
	struct vop_reg bypass_csc;
	struct vop_reg bypass_cvm;
	struct vop_reg operating_mode;
	struct vop_reg pixel_rate;

	/* core2 */
	struct vop_reg yuv2rgb_en;
	struct vop_reg yuv422to444_en;
	struct vop_reg yuv_swap;
	struct vop_reg yuv422_en;
	struct vop_reg dly_en;

	/* core1 and core2 */
	struct vop_reg lut_mst;
	struct vop_reg lut_update;

	/* core3 */
	struct vop_reg output_mode;
};

struct vop2_dsc_regs {
	/* DSC SYS CTRL */
	struct vop_reg dsc_port_sel;
	struct vop_reg dsc_man_mode;
	struct vop_reg dsc_interface_mode;
	struct vop_reg dsc_pixel_num;
	struct vop_reg dsc_pxl_clk_div;
	struct vop_reg dsc_cds_clk_div;
	struct vop_reg dsc_txp_clk_div;
	struct vop_reg dsc_init_dly_mode;
	struct vop_reg dsc_scan_en;
	struct vop_reg dsc_halt_en;
	struct vop_reg rst_deassert;
	struct vop_reg dsc_flush;
	struct vop_reg dsc_cfg_done;
	struct vop_reg dsc_init_dly_num;
	struct vop_reg scan_timing_para_imd_en;
	struct vop_reg dsc_htotal_pw;
	struct vop_reg dsc_hact_st_end;
	struct vop_reg dsc_vtotal;
	struct vop_reg dsc_vs_end;
	struct vop_reg dsc_vact_st_end;
	struct vop_reg dsc_error_status;

	/* DSC encoder */
	struct vop_reg dsc_pps0_3;
	struct vop_reg dsc_en;
	struct vop_reg dsc_rbit;
	struct vop_reg dsc_rbyt;
	struct vop_reg dsc_flal;
	struct vop_reg dsc_mer;
	struct vop_reg dsc_epb;
	struct vop_reg dsc_epl;
	struct vop_reg dsc_nslc;
	struct vop_reg dsc_sbo;
	struct vop_reg dsc_ifep;
	struct vop_reg dsc_pps_upd;
	struct vop_reg dsc_status;
	struct vop_reg dsc_ecw;
};

struct vop2_wb_regs {
	struct vop_reg enable;
	struct vop_reg format;
	struct vop_reg dither_en;
	struct vop_reg r2y_en;
	struct vop_reg yrgb_mst;
	struct vop_reg uv_mst;
	struct vop_reg vp_id;
	struct vop_reg fifo_throd;
	struct vop_reg scale_x_factor;
	struct vop_reg scale_x_en;
	struct vop_reg scale_y_en;
	struct vop_reg axi_yrgb_id;
	struct vop_reg axi_uv_id;
	struct vop_reg vir_stride;
	struct vop_reg vir_stride_en;
	struct vop_reg act_width;
	struct vop_reg post_empty_stop_en;
	struct vop_reg one_frame_mode;
	struct vop_reg auto_gating;
};

struct vop2_power_domain_data {
	uint16_t id;
	uint16_t parent_id;
	/*
	 * @module_id_mask: module id of which module this power domain is belongs to.
	 * PD_CLUSTER0,1,2,3 only belongs to CLUSTER0/1/2/3, PD_Esmart0 shared by Esmart1/2/3
	 */
	uint32_t module_id_mask;

	const struct vop2_power_domain_regs *regs;
};

/*
 * connector interface(RGB/HDMI/eDP/DP/MIPI) data
 */
struct vop2_connector_if_data {
	u32 id;
	const char *clk_src_name;
	const char *clk_parent_name;
	const char *pixclk_name;
	const char *dclk_name;
	u32 post_proc_div_shift;
	u32 if_div_shift;
	u32 if_div_yuv420_shift;
	u32 bus_div_shift;
	u32 pixel_clk_div_shift;
};

struct vop2_win_data {
	const char *name;
	uint8_t phys_id;
	uint8_t splice_win_id;
	uint16_t pd_id;
	uint8_t axi_id;
	uint8_t axi_yrgb_id;
	uint8_t axi_uv_id;
	uint8_t possible_vp_mask;
	uint8_t dci_rid_id;
	uint8_t reg_done_bit;

	uint32_t base;
	enum drm_plane_type type;

	uint32_t nformats;
	const uint32_t *formats;
	const uint64_t *format_modifiers;
	const unsigned int supported_rotations;

	const struct vop2_win_regs *regs;
	const struct vop2_win_regs **area;
	unsigned int area_size;
	struct vop_rect max_input;
	struct vop_rect max_output;

	/*
	 * vertical/horizontal scale up/down filter mode
	 */
	const u8 hsu_filter_mode;
	const u8 hsd_filter_mode;
	const u8 vsu_filter_mode;
	const u8 vsd_filter_mode;
	const u8 hsd_pre_filter_mode;
	const u8 vsd_pre_filter_mode;
	/**
	 * @layer_sel_id: defined by register OVERLAY_LAYER_SEL of VOP2
	 */
	const uint8_t layer_sel_id[ROCKCHIP_MAX_CRTC];
	uint64_t feature;

	unsigned int max_upscale_factor;
	unsigned int max_downscale_factor;
	const uint8_t dly[VOP2_DLY_MODE_MAX];
};

struct vop2_dovi_core_data {
	const uint8_t id;
	const uint32_t ctrl_offset;
	const uint32_t srange_offset;
	const uint32_t srange_offset_from_core;
	const struct vop2_dovi_regs *regs;
};

struct vop2_dovi_data {
	const uint8_t nr_dovi_cores;
	const uint8_t dovi_max_delay[2];
	const uint32_t enhance_layer_phy_id;
	const struct vop2_dovi_core_data *dovi_core_data;
};

struct dsc_error_info {
	u32 dsc_error_val;
	char dsc_error_info[50];
};

struct vop2_dsc_data {
	uint8_t id;
	uint16_t pd_id;
	uint8_t max_slice_num;
	uint8_t max_linebuf_depth;	/* used to generate the bitstream */
	uint8_t min_bits_per_pixel;	/* bit num after encoder compress */
	const char *dsc_txp_clk_src_name;
	const char *dsc_txp_clk_name;
	const char *dsc_pxl_clk_name;
	const char *dsc_cds_clk_name;
	const struct vop2_dsc_regs *regs;
};

struct vop2_wb_data {
	uint32_t nformats;
	const uint32_t *formats;
	struct vop_rect max_output;
	const struct vop2_wb_regs *regs;
	uint32_t fifo_depth;
};

struct vop3_ovl_mix_regs {
	struct vop_reg src_color_ctrl;
	struct vop_reg dst_color_ctrl;
	struct vop_reg src_alpha_ctrl;
	struct vop_reg dst_alpha_ctrl;
};

struct vop3_ovl_regs {
	const struct vop3_ovl_mix_regs *layer_mix_regs;
	const struct vop3_ovl_mix_regs *hdr_mix_regs;
	const struct vop3_ovl_mix_regs *extra_mix_regs;
	const struct vop3_ovl_mix_regs *cursor_mix_regs;
};

struct vop2_video_port_data {
	char id;
	uint8_t splice_vp_id;
	uint16_t lut_dma_rid;
	uint32_t dclk_switch_id;
	uint32_t feature;
	uint64_t soc_id[VOP2_SOC_VARIANT];
	uint16_t gamma_lut_len;
	uint16_t cubic_lut_len;
	unsigned long dclk_max;
	struct vop_rect max_output;
	const u8 pre_scan_max_dly[4];
	const u8 hdrvivid_dly[10];
	const u8 sdr2hdr_dly;
	const u8 layer_mix_dly;
	const u8 hdr_mix_dly;
	const u8 win_dly;
	const u8 pixel_rate;
	const u8 cursor_dly;
	const struct vop_intr *intr;
	const struct vop_urgency *urgency;
	const struct vop_hdr_table *hdr_table;
	const struct vop2_video_port_regs *regs;
	const struct vop3_ovl_regs *ovl_regs;
};

struct vop2_layer_regs {
	struct vop_reg layer_sel;
};

/**
 * struct vop2_layer_data - The logic graphic layer in vop2
 *
 * The zorder:
 *   LAYERn
 *   LAYERn-1
 *     .
 *     .
 *     .
 *   LAYER5
 *   LAYER4
 *   LAYER3
 *   LAYER2
 *   LAYER1
 *   LAYER0
 *
 * Each layer can select a unused window as input than feed to
 * mixer for overlay.
 *
 * The pipeline in vop2:
 *
 * win-->layer-->mixer-->vp--->connector(RGB/LVDS/HDMI/MIPI)
 *
 */
struct vop2_layer_data {
	char id;
	const struct vop2_layer_regs *regs;
};

struct vop_grf_ctrl {
	struct vop_reg grf_dclk_inv;
	struct vop_reg grf_bt1120_clk_inv;
	struct vop_reg grf_bt656_clk_inv;
	struct vop_reg grf_edp0_en;
	struct vop_reg grf_edp1_en;
	struct vop_reg grf_hdmi0_en;
	struct vop_reg grf_hdmi1_en;
	struct vop_reg grf_hdmi0_dsc_en;
	struct vop_reg grf_hdmi1_dsc_en;
	struct vop_reg grf_hdmi0_pin_pol;
	struct vop_reg grf_hdmi1_pin_pol;
	struct vop_reg grf_vopl_sel;
	/*
	 * For rk3576, vopl supports eDP/HDMI/MIPI by the 1to4
	 * module, which can transfer 1 pixle/cycle data from
	 * vopl to 4 pixle/cycle data for HDMI/MIPI controllers.
	 */
	struct vop_reg grf_edp_ch_sel;
	struct vop_reg grf_hdmi_ch_sel;
	struct vop_reg grf_mipi_ch_sel;
	struct vop_reg grf_hdmi_pin_pol;
	struct vop_reg grf_hdmi_1to4_en;
	struct vop_reg grf_mipi_mode;
	struct vop_reg grf_mipi_pin_pol;
	struct vop_reg grf_mipi_1to4_en;
};

struct vop_wb_regs {
	struct vop_reg cfg_done;
	struct vop_reg enable;
	struct vop_reg format;
	struct vop_reg dither_en;
	struct vop_reg r2y_en;
	struct vop_reg yrgb_mst;
	struct vop_reg uv_mst;
	struct vop_reg fifo_throd;
	struct vop_reg scale_x_factor;
	struct vop_reg scale_x_en;
	struct vop_reg scale_y_en;
	struct vop_reg axi_yrgb_id;
	struct vop_reg axi_uv_id;
	struct vop_reg vir_stride;
	struct vop_reg vir_stride_en;
	struct vop_reg act_width;
	struct vop_reg post_empty_stop_en;
	struct vop_reg one_frame_mode;
	struct vop_reg xgt2_en;
};

struct vop_wb_data {
	uint32_t nformats;
	const uint32_t *formats;
	struct vop_rect max_output;
	const struct vop_wb_regs *regs;
	uint32_t fifo_depth;
	uint16_t axi_yrgb_id;
	uint16_t axi_uv_id;
};

struct vop_data {
	const struct vop_reg_data *init_table;
	unsigned int table_size;
	const struct vop_ctrl *ctrl;
	const struct vop_intr *intr;
	const struct vop_intr *wb_intr;
	const struct vop_wb_data *wb;
	const struct vop_win_data *win;
	const struct vop_csc_table *csc_table;
	const struct vop_hdr_table *hdr_table;
	const struct vop_grf_ctrl *grf;
	const struct vop_grf_ctrl *vo0_grf;
	const struct vop_mcu_bypass_cfg *mcu_bypass_cfg;
	unsigned int win_size;
	uint32_t version;
	struct vop_rect max_input;
	struct vop_rect max_output;
	u64 feature;
	u64 soc_id;
	u8 vop_id;
};

struct vop2_ctrl {
	struct vop_reg cfg_done_en;
	struct vop_reg wb_cfg_done;
	struct vop_reg win_cfg_done;
	struct vop_reg auto_gating_en;
	struct vop_reg aclk_pre_auto_gating_en;
	struct vop_reg dma_finish_mode;
	struct vop_reg axi_dma_finish_and_en;
	struct vop_reg wb_dma_finish_and_en;
	struct vop_reg ovl_cfg_done_port;
	struct vop_reg ovl_port_mux_cfg_done_imd;
	struct vop_reg ovl_port_mux_cfg;
	struct vop_reg if_ctrl_cfg_done_imd;
	struct vop_reg version;
	struct vop_reg standby;
	struct vop_reg dma_stop;
	struct vop_reg rkmmu_v2_en;
	struct vop_reg rkmmu_v2_sel_axi;
	struct vop_reg dsp_vs_t_sel;
	struct vop_reg lut_dma_en;
	struct vop_reg lut_use_axi1;
	struct vop_reg axi_outstanding_max_num;
	struct vop_reg axi_max_outstanding_en;
	struct vop_reg hdmi_dclk_out_en;
	struct vop_reg hdmi0_r2y_en;
	struct vop_reg hdmi0_r2y_mode;
	struct vop_reg rgb_en;
	struct vop_reg hdmi0_en;
	struct vop_reg hdmi1_en;
	struct vop_reg dp0_en;
	struct vop_reg dp1_en;
	struct vop_reg dp2_en;
	struct vop_reg edp0_en;
	struct vop_reg edp1_en;
	struct vop_reg mipi0_en;
	struct vop_reg mipi1_en;
	struct vop_reg lvds0_en;
	struct vop_reg lvds1_en;
	struct vop_reg bt656_en;
	struct vop_reg bt1120_en;
	struct vop_reg bt656_dclk_pol;
	struct vop_reg bt1120_dclk_pol;
	struct vop_reg dclk_pol;
	struct vop_reg pin_pol;
	struct vop_reg rgb_dclk_pol;
	struct vop_reg rgb_pin_pol;
	struct vop_reg lvds_dclk_pol;
	struct vop_reg lvds_pin_pol;
	struct vop_reg hdmi_dclk_pol;
	struct vop_reg hdmi_pin_pol;
	struct vop_reg edp_dclk_pol;
	struct vop_reg edp_pin_pol;
	struct vop_reg mipi_dclk_pol;
	struct vop_reg mipi_pin_pol;
	struct vop_reg dp0_dclk_pol;
	struct vop_reg dp0_pin_pol;
	struct vop_reg dp1_dclk_pol;
	struct vop_reg dp1_pin_pol;
	struct vop_reg dp2_dclk_pol;
	struct vop_reg dp2_pin_pol;

	/* This will be reference by win_phy_id */
	struct vop_reg win_vp_id[16];
	struct vop_reg win_dly[16];

	/* connector mux */
	struct vop_reg rgb_mux;
	struct vop_reg hdmi0_mux;
	struct vop_reg hdmi1_mux;
	struct vop_reg dp0_mux;
	struct vop_reg dp1_mux;
	struct vop_reg dp2_mux;
	struct vop_reg edp0_mux;
	struct vop_reg edp1_mux;
	struct vop_reg mipi0_mux;
	struct vop_reg mipi1_mux;
	struct vop_reg lvds0_mux;
	struct vop_reg lvds1_mux;

	struct vop_reg lvds_dual_en;
	struct vop_reg lvds_dual_mode;
	struct vop_reg lvds_dual_channel_swap;

	struct vop_reg dp_dual_en;
	struct vop_reg edp_dual_en;
	struct vop_reg hdmi_dual_en;
	struct vop_reg mipi_dual_en;
	struct vop_reg rgb_dual_en;

	struct vop_reg hdmi0_dclk_div;
	struct vop_reg hdmi0_pixclk_div;/* crtc last pipeline clk to connector */
	struct vop_reg edp0_dclk_div;
	struct vop_reg edp0_pixclk_div;

	struct vop_reg hdmi1_dclk_div;
	struct vop_reg hdmi1_pixclk_div;
	struct vop_reg edp1_dclk_div;
	struct vop_reg edp1_pixclk_div;

	struct vop_reg mipi0_pixclk_div;
	struct vop_reg mipi1_pixclk_div;
	struct vop_reg mipi0_ds_mode;
	struct vop_reg mipi1_ds_mode;

	struct vop_reg hdmi0_dclk_sel;/* sel from dclk_core or dclk_out */
	struct vop_reg edp0_dclk_sel;
	struct vop_reg mipi0_dclk_sel;
	struct vop_reg rgb_dclk_sel;
	struct vop_reg dp0_dclk_sel;
	struct vop_reg dp1_dclk_sel;
	struct vop_reg dp2_dclk_sel;

	struct vop_reg dp0_pixclk_div;/* crtc last pipeline clk to connector */
	struct vop_reg dp1_pixclk_div;
	struct vop_reg dp2_pixclk_div;

	struct vop_reg src_color_ctrl;
	struct vop_reg dst_color_ctrl;
	struct vop_reg src_alpha_ctrl;
	struct vop_reg dst_alpha_ctrl;

	struct vop_reg bt1120_yc_swap;
	struct vop_reg bt656_yc_swap;
	struct vop_reg gamma_port_sel;
	struct vop_reg pd_off_imd;

	struct vop_reg mipi0_regdone_imd_en;
	struct vop_reg mipi0_data1_sel;
	struct vop_reg mipi0_dclk_out_en;
	struct vop_reg hdmi0_regdone_imd_en;
	struct vop_reg hdmi0_data1_sel;
	struct vop_reg hdmi0_dclk_out_en;
	struct vop_reg edp0_regdone_imd_en;
	struct vop_reg edp0_data1_sel;
	struct vop_reg edp0_dclk_out_en;
	struct vop_reg dp0_regdone_imd_en;
	struct vop_reg dp0_data1_sel;
	struct vop_reg dp0_dclk_out_en;
	struct vop_reg dp1_regdone_imd_en;
	struct vop_reg dp1_data1_sel;
	struct vop_reg dp1_dclk_out_en;
	struct vop_reg dp2_regdone_imd_en;
	struct vop_reg dp2_data1_sel;
	struct vop_reg dp2_dclk_out_en;
	struct vop_reg rgb_regdone_imd_en;
	struct vop_reg rgb_data1_sel;
	struct vop_reg rgb_dclk_out_en;

	struct vop_reg otp_en;
	struct vop_reg esmart_lb_mode;
	struct vop_reg vp_intr_merge_en;
	struct vop_reg reg_done_frm;
	struct vop_reg cfg_done;

	struct vop_reg dovi_core1_en;
	struct vop_reg dovi_core2_en;
	struct vop_reg dovi_core3_en;
};

struct vop_dump_regs {
	uint32_t offset;
	const char *name;
	struct vop_reg state;
	bool enable_state;
	uint32_t size;
};

struct vop2_vp_plane_mask {
	u8 primary_plane_id;
	u8 attached_layers_nr;
	u8 attached_layers[ROCKCHIP_MAX_LAYER];
};

struct vop2_esmart_lb_map {
	u8 lb_mode;
	u8 lb_map_value;
};

/**
 * VOP2 data structe
 *
 * @version: VOP IP version
 * @win_size: hardware win number
 */
struct vop2_data {
	uint32_t version;
	uint32_t feature;
	uint8_t nr_dscs;
	uint8_t nr_dsc_ecw;
	uint8_t nr_dsc_buffer_flow;
	uint8_t nr_vps;
	uint8_t nr_mixers;
	uint8_t nr_layers;
	uint8_t nr_axi_intr;
	uint8_t nr_gammas;
	uint8_t nr_conns;
	uint8_t nr_pds;
	uint8_t nr_mem_pgs;
	uint8_t esmart_lb_mode;
	bool delayed_pd;
	uint8_t esmart_lb_mode_num;
	uint8_t crc_sources_num;
	const struct vop2_esmart_lb_map *esmart_lb_mode_map;
	const struct vop_intr *axi_intr;
	const struct vop2_ctrl *ctrl;
	const struct vop2_dovi_data *dovi;
	const struct vop2_dsc_data *dsc;
	const struct dsc_error_info *dsc_error_ecw;
	const struct dsc_error_info *dsc_error_buffer_flow;
	const struct vop2_win_data *win;
	const struct vop2_video_port_data *vp;
	const struct vop2_connector_if_data *conn;
	const struct vop2_wb_data *wb;
	const struct vop2_layer_data *layer;
	const struct vop2_power_domain_data *pd;
	const struct vop2_power_domain_data *mem_pg;
	const struct vop_csc_table *csc_table;
	const struct vop_hdr_table *hdr_table;
	const struct vop_grf_ctrl *sys_grf;
	const struct vop_grf_ctrl *grf;
	const struct vop_grf_ctrl *vo0_grf;
	const struct vop_grf_ctrl *vo1_grf;
	const struct vop_grf_ctrl *ioc_grf;
	const struct vop_dump_regs *dump_regs;
	const char * const *crc_sources;
	uint32_t dump_regs_size;
	struct vop_rect max_input;
	struct vop_rect max_output;
	const struct vop2_vp_plane_mask *plane_mask;
	uint32_t plane_mask_base;

	unsigned int win_size;
};

#define CVBS_PAL_VDISPLAY		288

/* interrupt define */
#define DSP_HOLD_VALID_INTR		BIT(0)
#define FS_INTR				BIT(1)
#define LINE_FLAG_INTR			BIT(2)
#define BUS_ERROR_INTR			BIT(3)
#define FS_NEW_INTR			BIT(4)
#define ADDR_SAME_INTR			BIT(5)
#define LINE_FLAG1_INTR			BIT(6)
#define WIN0_EMPTY_INTR			BIT(7)
#define WIN1_EMPTY_INTR			BIT(8)
#define WIN2_EMPTY_INTR			BIT(9)
#define WIN3_EMPTY_INTR			BIT(10)
#define HWC_EMPTY_INTR			BIT(11)
#define POST_BUF_EMPTY_INTR		BIT(12)
#define PWM_GEN_INTR			BIT(13)
#define DMA_FINISH_INTR			BIT(14)
#define FS_FIELD_INTR			BIT(15)
#define FE_INTR				BIT(16)
#define WB_UV_FIFO_FULL_INTR		BIT(17)
#define WB_YRGB_FIFO_FULL_INTR		BIT(18)
#define WB_COMPLETE_INTR		BIT(19)
#define MMU_EN_INTR			BIT(20)
#define DOLBY_CORE1_INTR		BIT(21)
#define DOLBY_CORE2_INTR		BIT(22)
#define DOLBY_CORE3_INTR		BIT(23)

#define INTR_MASK			(DSP_HOLD_VALID_INTR | FS_INTR | \
					 LINE_FLAG_INTR | BUS_ERROR_INTR | \
					 FS_NEW_INTR | LINE_FLAG1_INTR | \
					 WIN0_EMPTY_INTR | WIN1_EMPTY_INTR | \
					 WIN2_EMPTY_INTR | WIN3_EMPTY_INTR | \
					 HWC_EMPTY_INTR | \
					 POST_BUF_EMPTY_INTR | \
					 DMA_FINISH_INTR | FS_FIELD_INTR | \
					 FE_INTR | WB_COMPLETE_INTR | MMU_EN_INTR | \
					 DOLBY_CORE1_INTR | DOLBY_CORE2_INTR | DOLBY_CORE3_INTR)
#define DSP_HOLD_VALID_INTR_EN(x)	((x) << 4)
#define FS_INTR_EN(x)			((x) << 5)
#define LINE_FLAG_INTR_EN(x)		((x) << 6)
#define BUS_ERROR_INTR_EN(x)		((x) << 7)
#define DSP_HOLD_VALID_INTR_MASK	(1 << 4)
#define FS_INTR_MASK			(1 << 5)
#define LINE_FLAG_INTR_MASK		(1 << 6)
#define BUS_ERROR_INTR_MASK		(1 << 7)

#define INTR_CLR_SHIFT			8
#define DSP_HOLD_VALID_INTR_CLR		(1 << (INTR_CLR_SHIFT + 0))
#define FS_INTR_CLR			(1 << (INTR_CLR_SHIFT + 1))
#define LINE_FLAG_INTR_CLR		(1 << (INTR_CLR_SHIFT + 2))
#define BUS_ERROR_INTR_CLR		(1 << (INTR_CLR_SHIFT + 3))

/* RV1126 VOP Lite WB intr define */
#define VOPL_WB_YRGB_FIFO_FULL_INTR	BIT(0)
#define VOPL_WB_UV_FIFO_FULL_INTR	BIT(1)
#define VOPL_WB_COMPLETE_INTR		BIT(4)

#define DSP_LINE_NUM(x)			(((x) & 0x1fff) << 12)
#define DSP_LINE_NUM_MASK		(0x1fff << 12)

/* src alpha ctrl define */
#define SRC_FADING_VALUE(x)		(((x) & 0xff) << 24)
#define SRC_GLOBAL_ALPHA(x)		(((x) & 0xff) << 16)
#define SRC_FACTOR_M0(x)		(((x) & 0x7) << 6)
#define SRC_ALPHA_CAL_M0(x)		(((x) & 0x1) << 5)
#define SRC_BLEND_M0(x)			(((x) & 0x3) << 3)
#define SRC_ALPHA_M0(x)			(((x) & 0x1) << 2)
#define SRC_COLOR_M0(x)			(((x) & 0x1) << 1)
#define SRC_ALPHA_EN(x)			(((x) & 0x1) << 0)
/* dst alpha ctrl define */
#define DST_FACTOR_M0(x)		(((x) & 0x7) << 6)

/*
 * display output interface supported by rockchip lcdc
 */
#define ROCKCHIP_OUT_MODE_P888		0
#define ROCKCHIP_OUT_MODE_BT1120	0
#define ROCKCHIP_OUT_MODE_P666		1
#define ROCKCHIP_OUT_MODE_P565		2
#define RK3588_EDP_OUTPUT_MODE_YUV422	3
#define ROCKCHIP_OUT_MODE_BT656		5
#define ROCKCHIP_OUT_MODE_S888		8
#define ROCKCHIP_OUT_MODE_S666		9
#define ROCKCHIP_OUT_MODE_YUV422	9
#define ROCKCHIP_OUT_MODE_S565		10
#define ROCKCHIP_OUT_MODE_S888_DUMMY	12
#define RK3588_DP_OUT_MODE_YUV422	12
#define RK3576_EDP_OUT_MODE_YUV422	12
#define RK3588_DP_OUT_MODE_YUV420	13
#define RK3576_HDMI_OUT_MODE_YUV422	13
#define ROCKCHIP_OUT_MODE_YUV420	14
/* for use special outface */
#define ROCKCHIP_OUT_MODE_AAAA		15

#define ROCKCHIP_OUT_MODE_TYPE(x)	((x) >> 16)
#define ROCKCHIP_OUT_MODE(x)		((x) & 0xffff)

enum alpha_mode {
	ALPHA_STRAIGHT,
	ALPHA_INVERSE,
};

enum global_blend_mode {
	ALPHA_GLOBAL,
	ALPHA_PER_PIX,
	ALPHA_PER_PIX_GLOBAL,
};

enum alpha_cal_mode {
	ALPHA_SATURATION,
	ALPHA_NO_SATURATION,
};

enum color_mode {
	ALPHA_SRC_PRE_MUL,
	ALPHA_SRC_NO_PRE_MUL,
};

enum factor_mode {
	ALPHA_ZERO,
	ALPHA_ONE,
	ALPHA_SRC,
	ALPHA_SRC_INVERSE,
	ALPHA_SRC_GLOBAL,
	ALPHA_DST_GLOBAL,
};

enum src_factor_mode {
	SRC_FAC_ALPHA_ZERO,
	SRC_FAC_ALPHA_ONE,
	SRC_FAC_ALPHA_DST,
	SRC_FAC_ALPHA_DST_INVERSE,
	SRC_FAC_ALPHA_SRC,
	SRC_FAC_ALPHA_SRC_GLOBAL,
};

enum dst_factor_mode {
	DST_FAC_ALPHA_ZERO,
	DST_FAC_ALPHA_ONE,
	DST_FAC_ALPHA_SRC,
	DST_FAC_ALPHA_SRC_INVERSE,
	DST_FAC_ALPHA_DST,
	DST_FAC_ALPHA_DST_GLOBAL,
};

enum scale_mode {
	SCALE_NONE = 0x0,
	SCALE_UP   = 0x1,
	SCALE_DOWN = 0x2
};

enum lb_mode {
	LB_YUV_3840X5 = 0x0,
	LB_YUV_2560X8 = 0x1,
	LB_RGB_3840X2 = 0x2,
	LB_RGB_2560X4 = 0x3,
	LB_RGB_1920X5 = 0x4,
	LB_RGB_1280X8 = 0x5
};

enum sacle_up_mode {
	SCALE_UP_BIL = 0x0,
	SCALE_UP_BIC = 0x1
};

enum scale_down_mode {
	SCALE_DOWN_BIL = 0x0,
	SCALE_DOWN_AVG = 0x1
};

enum vop2_scale_up_mode {
	VOP2_SCALE_UP_NRST_NBOR,
	VOP2_SCALE_UP_BIL,
	VOP2_SCALE_UP_BIC,
	VOP2_SCALE_UP_ZME,
};

enum vop2_scale_down_mode {
	VOP2_SCALE_DOWN_NRST_NBOR,
	VOP2_SCALE_DOWN_BIL,
	VOP2_SCALE_DOWN_AVG,
	VOP2_SCALE_DOWN_ZME,
};

enum vop3_pre_scale_down_mode {
	VOP3_PRE_SCALE_UNSPPORT,
	VOP3_PRE_SCALE_DOWN_GT,
	VOP3_PRE_SCALE_DOWN_AVG,
};

enum dither_down_mode {
	RGB888_TO_RGB565 = 0x0,
	RGB888_TO_RGB666 = 0x1
};

enum dither_down_mode_sel {
	DITHER_DOWN_ALLEGRO = 0x0,
	DITHER_DOWN_FRC = 0x1
};

enum vop_pol {
	HSYNC_POSITIVE = 0,
	VSYNC_POSITIVE = 1,
	DEN_NEGATIVE   = 2,
	DCLK_INVERT    = 3
};

#define FRAC_16_16(mult, div)    (((mult) << 16) / (div))
#define SCL_FT_DEFAULT_FIXPOINT_SHIFT	12
#define SCL_MAX_VSKIPLINES		4
#define MIN_SCL_FT_AFTER_VSKIP		1

static inline uint16_t scl_cal_scale(int src, int dst, int shift)
{
	return ((src * 2 - 3) << (shift - 1)) / (dst - 1);
}

static inline uint16_t scl_cal_scale2(int src, int dst)
{
	return ((src - 1) << 12) / (dst - 1);
}

#define GET_SCL_FT_BILI_DN(src, dst)	scl_cal_scale(src, dst, 12)
#define GET_SCL_FT_BILI_UP(src, dst)	scl_cal_scale(src, dst, 16)
#define GET_SCL_FT_BIC(src, dst)	scl_cal_scale(src, dst, 16)

static inline uint16_t scl_get_bili_dn_vskip(int src_h, int dst_h,
					     int vskiplines)
{
	int act_height;

	act_height = (src_h + vskiplines - 1) / vskiplines;

	if (act_height == dst_h)
		return GET_SCL_FT_BILI_DN(src_h, dst_h) / vskiplines;

	return GET_SCL_FT_BILI_DN(act_height, dst_h);
}

static inline enum scale_mode scl_get_scl_mode(int src, int dst)
{
	if (src < dst)
		return SCALE_UP;
	else if (src > dst)
		return SCALE_DOWN;

	return SCALE_NONE;
}

static inline int scl_get_vskiplines(uint32_t srch, uint32_t dsth)
{
	uint32_t vskiplines;

	for (vskiplines = SCL_MAX_VSKIPLINES; vskiplines > 1; vskiplines /= 2)
		if (srch >= vskiplines * dsth * MIN_SCL_FT_AFTER_VSKIP)
			break;

	return vskiplines;
}

static inline int scl_vop_cal_lb_mode(int width, bool is_yuv)
{
	int lb_mode;

	if (is_yuv) {
		if (width > 1280)
			lb_mode = LB_YUV_3840X5;
		else
			lb_mode = LB_YUV_2560X8;
	} else {
		if (width > 2560)
			lb_mode = LB_RGB_3840X2;
		else if (width > 1920)
			lb_mode = LB_RGB_2560X4;
		else
			lb_mode = LB_RGB_1920X5;
	}

	return lb_mode;
}

static inline int us_to_vertical_line(struct drm_display_mode *mode, int us)
{
	return us * mode->clock / mode->htotal / 1000;
}

static inline int interpolate(int x1, int y1, int x2, int y2, int x)
{
	return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

extern const struct component_ops vop_component_ops;
extern const struct component_ops vop2_component_ops;
#endif /* _ROCKCHIP_DRM_VOP_H */
