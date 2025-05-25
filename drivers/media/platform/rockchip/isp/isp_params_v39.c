// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include <media/v4l2-common.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>	/* for ISP params */
#include "dev.h"
#include "regs.h"
#include "isp_params_v39.h"

#define ISP39_MODULE_EN				BIT(0)
#define ISP39_SELF_FORCE_UPD			BIT(31)
#define ISP39_REG_WR_MASK			BIT(31) //disable write protect

#define ISP39_AUTO_BIGMODE_WIDTH		2688
#define ISP39_NOBIG_OVERFLOW_SIZE		(2688 * 1536)

#define ISP39_VIR2_MAX_SIZE			(4416 * 2900)
#define ISP39_VIR2_NOBIG_OVERFLOW_SIZE		(ISP39_VIR2_MAX_SIZE / 4)
#define ISP39_VIR4_MAX_SIZE			(3840 * 1664)
#define ISP39_VIR4_NOBIG_OVERFLOW_SIZE		(ISP39_VIR4_MAX_SIZE / 4)

#define ISP39_FRM_BUF_SIZE			0x1d000

static inline void
isp3_param_write_direct(struct rkisp_isp_params_vdev *params_vdev,
			u32 value, u32 addr)
{
	rkisp_write(params_vdev->dev, addr, value, true);
}

static inline void
isp3_param_write(struct rkisp_isp_params_vdev *params_vdev,
		 u32 value, u32 addr, u32 id)
{
	rkisp_idx_write(params_vdev->dev, addr, value, id, false);
}

static inline u32
isp3_param_read_direct(struct rkisp_isp_params_vdev *params_vdev, u32 addr)
{
	return rkisp_read(params_vdev->dev, addr, true);
}

static inline u32
isp3_param_read(struct rkisp_isp_params_vdev *params_vdev, u32 addr, u32 id)
{
	return rkisp_idx_read(params_vdev->dev, addr, id, false);
}

static inline u32
isp3_param_read_cache(struct rkisp_isp_params_vdev *params_vdev, u32 addr, u32 id)
{
	return rkisp_idx_read_reg_cache(params_vdev->dev, addr, id);
}

static inline void
isp3_param_set_bits(struct rkisp_isp_params_vdev *params_vdev,
		    u32 reg, u32 bit_mask, u32 id)
{
	rkisp_idx_set_bits(params_vdev->dev, reg, 0, bit_mask, id, false);
}

static inline void
isp3_param_clear_bits(struct rkisp_isp_params_vdev *params_vdev,
		      u32 reg, u32 bit_mask, u32 id)
{
	rkisp_idx_clear_bits(params_vdev->dev, reg, bit_mask, id, false);
}

static void
isp_dpcc_config(struct rkisp_isp_params_vdev *params_vdev,
		const struct isp39_dpcc_cfg *arg, u32 id)
{
	u32 value;
	int i;

	value = isp3_param_read(params_vdev, ISP3X_DPCC0_MODE, id);
	value &= ISP_DPCC_EN;

	value |= !!arg->stage1_enable << 2 |
		 !!arg->grayscale_mode << 1;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_MODE, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_MODE, id);

	value = !!arg->border_bypass_mode << 8 |
		(arg->sw_rk_out_sel & 0x03) << 5 |
		!!arg->sw_dpcc_output_sel << 4 |
		!!arg->stage1_rb_3x3 << 3 |
		!!arg->stage1_g_3x3 << 2 |
		!!arg->stage1_incl_rb_center << 1 |
		!!arg->stage1_incl_green_center;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_OUTPUT_MODE, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_OUTPUT_MODE, id);

	value = !!arg->stage1_use_fix_set << 3 |
		!!arg->stage1_use_set_3 << 2 |
		!!arg->stage1_use_set_2 << 1 |
		!!arg->stage1_use_set_1;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_SET_USE, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_SET_USE, id);

	value = !!arg->sw_rk_red_blue1_en << 13 |
		!!arg->rg_red_blue1_enable << 12 |
		!!arg->rnd_red_blue1_enable << 11 |
		!!arg->ro_red_blue1_enable << 10 |
		!!arg->lc_red_blue1_enable << 9 |
		!!arg->pg_red_blue1_enable << 8 |
		!!arg->sw_rk_green1_en << 5 |
		!!arg->rg_green1_enable << 4 |
		!!arg->rnd_green1_enable << 3 |
		!!arg->ro_green1_enable << 2 |
		!!arg->lc_green1_enable << 1 |
		!!arg->pg_green1_enable;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_METHODS_SET_1, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_METHODS_SET_1, id);

	value = !!arg->sw_rk_red_blue2_en << 13 |
		!!arg->rg_red_blue2_enable << 12 |
		!!arg->rnd_red_blue2_enable << 11 |
		!!arg->ro_red_blue2_enable << 10 |
		!!arg->lc_red_blue2_enable << 9 |
		!!arg->pg_red_blue2_enable << 8 |
		!!arg->sw_rk_green2_en << 5 |
		!!arg->rg_green2_enable << 4 |
		!!arg->rnd_green2_enable << 3 |
		!!arg->ro_green2_enable << 2 |
		!!arg->lc_green2_enable << 1 |
		!!arg->pg_green2_enable;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_METHODS_SET_2, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_METHODS_SET_2, id);

	value = !!arg->sw_rk_red_blue3_en << 13 |
		!!arg->rg_red_blue3_enable << 12 |
		!!arg->rnd_red_blue3_enable << 11 |
		!!arg->ro_red_blue3_enable << 10 |
		!!arg->lc_red_blue3_enable << 9 |
		!!arg->pg_red_blue3_enable << 8 |
		!!arg->sw_rk_green3_en << 5 |
		!!arg->rg_green3_enable << 4 |
		!!arg->rnd_green3_enable << 3 |
		!!arg->ro_green3_enable << 2 |
		!!arg->lc_green3_enable << 1 |
		!!arg->pg_green3_enable;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_METHODS_SET_3, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_METHODS_SET_3, id);

	value = ISP_PACK_4BYTE(arg->line_thr_1_g, arg->line_thr_1_rb,
			       arg->sw_mindis1_g, arg->sw_mindis1_rb);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_LINE_THRESH_1, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_LINE_THRESH_1, id);

	value = ISP_PACK_4BYTE(arg->line_mad_fac_1_g, arg->line_mad_fac_1_rb,
			       arg->sw_dis_scale_max1, arg->sw_dis_scale_min1);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_LINE_MAD_FAC_1, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_LINE_MAD_FAC_1, id);

	value = ISP_PACK_4BYTE(arg->pg_fac_1_g, arg->pg_fac_1_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_PG_FAC_1, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_PG_FAC_1, id);

	value = ISP_PACK_4BYTE(arg->rnd_thr_1_g, arg->rnd_thr_1_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_RND_THRESH_1, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_RND_THRESH_1, id);

	value = ISP_PACK_4BYTE(arg->rg_fac_1_g, arg->rg_fac_1_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_RG_FAC_1, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_RG_FAC_1, id);

	value = ISP_PACK_4BYTE(arg->line_thr_2_g, arg->line_thr_2_rb,
			       arg->sw_mindis2_g, arg->sw_mindis2_rb);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_LINE_THRESH_2, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_LINE_THRESH_2, id);

	value = ISP_PACK_4BYTE(arg->line_mad_fac_2_g, arg->line_mad_fac_2_rb,
			       arg->sw_dis_scale_max2, arg->sw_dis_scale_min2);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_LINE_MAD_FAC_2, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_LINE_MAD_FAC_2, id);

	value = ISP_PACK_4BYTE(arg->pg_fac_2_g, arg->pg_fac_2_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_PG_FAC_2, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_PG_FAC_2, id);

	value = ISP_PACK_4BYTE(arg->rnd_thr_2_g, arg->rnd_thr_2_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_RND_THRESH_2, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_RND_THRESH_2, id);

	value = ISP_PACK_4BYTE(arg->rg_fac_2_g, arg->rg_fac_2_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_RG_FAC_2, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_RG_FAC_2, id);

	value = ISP_PACK_4BYTE(arg->line_thr_3_g, arg->line_thr_3_rb,
			       arg->sw_mindis3_g, arg->sw_mindis3_rb);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_LINE_THRESH_3, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_LINE_THRESH_3, id);

	value = ISP_PACK_4BYTE(arg->line_mad_fac_3_g, arg->line_mad_fac_3_rb,
			       arg->sw_dis_scale_max3, arg->sw_dis_scale_min3);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_LINE_MAD_FAC_3, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_LINE_MAD_FAC_3, id);

	value = ISP_PACK_4BYTE(arg->pg_fac_3_g, arg->pg_fac_3_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_PG_FAC_3, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_PG_FAC_3, id);

	value = ISP_PACK_4BYTE(arg->rnd_thr_3_g, arg->rnd_thr_3_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_RND_THRESH_3, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_RND_THRESH_3, id);

	value = ISP_PACK_4BYTE(arg->rg_fac_3_g, arg->rg_fac_3_rb, 0, 0);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_RG_FAC_3, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_RG_FAC_3, id);

	value = (arg->ro_lim_3_rb & 0x03) << 10 |
		(arg->ro_lim_3_g & 0x03) << 8 |
		(arg->ro_lim_2_rb & 0x03) << 6 |
		(arg->ro_lim_2_g & 0x03) << 4 |
		(arg->ro_lim_1_rb & 0x03) << 2 |
		(arg->ro_lim_1_g & 0x03);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_RO_LIMITS, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_RO_LIMITS, id);

	value = (arg->rnd_offs_3_rb & 0x03) << 10 |
		(arg->rnd_offs_3_g & 0x03) << 8 |
		(arg->rnd_offs_2_rb & 0x03) << 6 |
		(arg->rnd_offs_2_g & 0x03) << 4 |
		(arg->rnd_offs_1_rb & 0x03) << 2 |
		(arg->rnd_offs_1_g & 0x03);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_RND_OFFS, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_RND_OFFS, id);

	value = !!arg->bpt_rb_3x3 << 11 |
		!!arg->bpt_g_3x3 << 10 |
		!!arg->bpt_incl_rb_center << 9 |
		!!arg->bpt_incl_green_center << 8 |
		!!arg->bpt_use_fix_set << 7 |
		!!arg->bpt_use_set_3 << 6 |
		!!arg->bpt_use_set_2 << 5 |
		!!arg->bpt_use_set_1 << 4 |
		!!arg->bpt_cor_en << 1 |
		!!arg->bpt_det_en;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_BPT_CTRL, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_BPT_CTRL, id);

	isp3_param_write(params_vdev, arg->bp_number, ISP3X_DPCC0_BPT_NUMBER, id);
	isp3_param_write(params_vdev, arg->bp_number, ISP3X_DPCC1_BPT_NUMBER, id);
	isp3_param_write(params_vdev, arg->bp_table_addr, ISP3X_DPCC0_BPT_ADDR, id);
	isp3_param_write(params_vdev, arg->bp_table_addr, ISP3X_DPCC1_BPT_ADDR, id);

	value = ISP_PACK_2SHORT(arg->bpt_h_addr, arg->bpt_v_addr);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_BPT_DATA, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_BPT_DATA, id);

	isp3_param_write(params_vdev, arg->bp_cnt, ISP3X_DPCC0_BP_CNT, id);
	isp3_param_write(params_vdev, arg->bp_cnt, ISP3X_DPCC1_BP_CNT, id);

	isp3_param_write(params_vdev, arg->sw_pdaf_en, ISP3X_DPCC0_PDAF_EN, id);
	isp3_param_write(params_vdev, arg->sw_pdaf_en, ISP3X_DPCC1_PDAF_EN, id);

	value = 0;
	for (i = 0; i < ISP39_DPCC_PDAF_POINT_NUM; i++)
		value |= !!arg->pdaf_point_en[i] << i;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_PDAF_POINT_EN, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_PDAF_POINT_EN, id);

	value = ISP_PACK_2SHORT(arg->pdaf_offsetx, arg->pdaf_offsety);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_PDAF_OFFSET, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_PDAF_OFFSET, id);

	value = ISP_PACK_2SHORT(arg->pdaf_wrapx, arg->pdaf_wrapy);
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_PDAF_WRAP, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_PDAF_WRAP, id);

	value = ISP_PACK_2SHORT(arg->pdaf_wrapx_num, arg->pdaf_wrapy_num);
	isp3_param_write(params_vdev, value, ISP_DPCC0_PDAF_SCOPE, id);
	isp3_param_write(params_vdev, value, ISP_DPCC1_PDAF_SCOPE, id);

	for (i = 0; i < ISP39_DPCC_PDAF_POINT_NUM / 2; i++) {
		value = ISP_PACK_4BYTE(arg->point[2 * i].x, arg->point[2 * i].y,
				       arg->point[2 * i + 1].x, arg->point[2 * i + 1].y);
		isp3_param_write(params_vdev, value, ISP3X_DPCC0_PDAF_POINT_0 + 4 * i, id);
		isp3_param_write(params_vdev, value, ISP3X_DPCC1_PDAF_POINT_0 + 4 * i, id);
	}

	isp3_param_write(params_vdev, arg->pdaf_forward_med, ISP3X_DPCC0_PDAF_FORWARD_MED, id);
	isp3_param_write(params_vdev, arg->pdaf_forward_med, ISP3X_DPCC1_PDAF_FORWARD_MED, id);
}

static void
isp_dpcc_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 value;

	value = isp3_param_read(params_vdev, ISP3X_DPCC0_MODE, id);
	value &= ~ISP_DPCC_EN;

	if (en)
		value |= ISP_DPCC_EN;
	isp3_param_write(params_vdev, value, ISP3X_DPCC0_MODE, id);
	isp3_param_write(params_vdev, value, ISP3X_DPCC1_MODE, id);
}

static void
isp_bls_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp32_bls_cfg *arg,
	       enum rkisp_params_type type, u32 id)
{
	const struct isp2x_bls_fixed_val *pval;
	u32 new_control, value;

	if (!params_vdev->dev->is_aiisp_en ||
	    type == RKISP_PARAMS_LAT || type == RKISP_PARAMS_ALL) {
		pval = &arg->bls1_val;
		switch (params_vdev->raw_type) {
		case RAW_BGGR:
			isp3_param_write(params_vdev, pval->r, ISP3X_BLS1_D_FIXED, id);
			isp3_param_write(params_vdev, pval->gr, ISP3X_BLS1_C_FIXED, id);
			isp3_param_write(params_vdev, pval->gb, ISP3X_BLS1_B_FIXED, id);
			isp3_param_write(params_vdev, pval->b, ISP3X_BLS1_A_FIXED, id);
			break;
		case RAW_GBRG:
			isp3_param_write(params_vdev, pval->r, ISP3X_BLS1_C_FIXED, id);
			isp3_param_write(params_vdev, pval->gr, ISP3X_BLS1_D_FIXED, id);
			isp3_param_write(params_vdev, pval->gb, ISP3X_BLS1_A_FIXED, id);
			isp3_param_write(params_vdev, pval->b, ISP3X_BLS1_B_FIXED, id);
			break;
		case RAW_GRBG:
			isp3_param_write(params_vdev, pval->r, ISP3X_BLS1_B_FIXED, id);
			isp3_param_write(params_vdev, pval->gr, ISP3X_BLS1_A_FIXED, id);
			isp3_param_write(params_vdev, pval->gb, ISP3X_BLS1_D_FIXED, id);
			isp3_param_write(params_vdev, pval->b, ISP3X_BLS1_C_FIXED, id);
			break;
		case RAW_RGGB:
		default:
			isp3_param_write(params_vdev, pval->r, ISP3X_BLS1_A_FIXED, id);
			isp3_param_write(params_vdev, pval->gr, ISP3X_BLS1_B_FIXED, id);
			isp3_param_write(params_vdev, pval->gb, ISP3X_BLS1_C_FIXED, id);
			isp3_param_write(params_vdev, pval->b, ISP3X_BLS1_D_FIXED, id);
			break;
		}
		if (type == RKISP_PARAMS_LAT)
			return;
	}

	new_control = isp3_param_read(params_vdev, ISP3X_BLS_CTRL, id);
	new_control &= (ISP_BLS_ENA | ISP32_BLS_BLS2_EN);
	if (arg->bls1_en)
		new_control |= ISP_BLS_BLS1_EN;

	/* fixed subtraction values */
	pval = &arg->fixed_val;
	if (!arg->enable_auto) {
		switch (params_vdev->raw_type) {
		case RAW_BGGR:
			isp3_param_write(params_vdev, pval->r, ISP3X_BLS_D_FIXED, id);
			isp3_param_write(params_vdev, pval->gr, ISP3X_BLS_C_FIXED, id);
			isp3_param_write(params_vdev, pval->gb, ISP3X_BLS_B_FIXED, id);
			isp3_param_write(params_vdev, pval->b, ISP3X_BLS_A_FIXED, id);
			break;
		case RAW_GBRG:
			isp3_param_write(params_vdev, pval->r, ISP3X_BLS_C_FIXED, id);
			isp3_param_write(params_vdev, pval->gr, ISP3X_BLS_D_FIXED, id);
			isp3_param_write(params_vdev, pval->gb, ISP3X_BLS_A_FIXED, id);
			isp3_param_write(params_vdev, pval->b, ISP3X_BLS_B_FIXED, id);
			break;
		case RAW_GRBG:
			isp3_param_write(params_vdev, pval->r, ISP3X_BLS_B_FIXED, id);
			isp3_param_write(params_vdev, pval->gr, ISP3X_BLS_A_FIXED, id);
			isp3_param_write(params_vdev, pval->gb, ISP3X_BLS_D_FIXED, id);
			isp3_param_write(params_vdev, pval->b, ISP3X_BLS_C_FIXED, id);
			break;
		case RAW_RGGB:
		default:
			isp3_param_write(params_vdev, pval->r, ISP3X_BLS_A_FIXED, id);
			isp3_param_write(params_vdev, pval->gr, ISP3X_BLS_B_FIXED, id);
			isp3_param_write(params_vdev, pval->gb, ISP3X_BLS_C_FIXED, id);
			isp3_param_write(params_vdev, pval->b, ISP3X_BLS_D_FIXED, id);
			break;
		}
	} else {
		if (arg->en_windows & BIT(1)) {
			isp3_param_write(params_vdev, arg->bls_window2.h_offs,
					 ISP3X_BLS_H2_START, id);
			value = arg->bls_window2.h_offs + arg->bls_window2.h_size;
			isp3_param_write(params_vdev, value, ISP3X_BLS_H2_STOP, id);
			isp3_param_write(params_vdev, arg->bls_window2.v_offs,
					 ISP3X_BLS_V2_START, id);
			value = arg->bls_window2.v_offs + arg->bls_window2.v_size;
			isp3_param_write(params_vdev, value, ISP3X_BLS_V2_STOP, id);
			new_control |= ISP_BLS_WINDOW_2;
		}

		if (arg->en_windows & BIT(0)) {
			isp3_param_write(params_vdev, arg->bls_window1.h_offs,
					 ISP3X_BLS_H1_START, id);
			value = arg->bls_window1.h_offs + arg->bls_window1.h_size;
			isp3_param_write(params_vdev, value, ISP3X_BLS_H1_STOP, id);
			isp3_param_write(params_vdev, arg->bls_window1.v_offs,
					 ISP3X_BLS_V1_START, id);
			value = arg->bls_window1.v_offs + arg->bls_window1.v_size;
			isp3_param_write(params_vdev, value, ISP3X_BLS_V1_STOP, id);
			new_control |= ISP_BLS_WINDOW_1;
		}

		isp3_param_write(params_vdev, arg->bls_samples, ISP3X_BLS_SAMPLES, id);

		new_control |= ISP_BLS_MODE_MEASURED;
	}
	isp3_param_write(params_vdev, new_control, ISP3X_BLS_CTRL, id);

	isp3_param_write(params_vdev, arg->isp_ob_offset, ISP32_BLS_ISP_OB_OFFSET, id);
	isp3_param_write(params_vdev, arg->isp_ob_predgain, ISP32_BLS_ISP_OB_PREDGAIN, id);
	isp3_param_write(params_vdev, arg->isp_ob_max, ISP32_BLS_ISP_OB_MAX, id);
}

static void
isp_bls_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 new_control;

	new_control = isp3_param_read(params_vdev, ISP3X_BLS_CTRL, id);
	if (en)
		new_control |= ISP_BLS_ENA;
	else
		new_control &= ~ISP_BLS_ENA;
	isp3_param_write(params_vdev, new_control, ISP3X_BLS_CTRL, id);
}

static void
isp_sdg_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp2x_sdg_cfg *arg, u32 id)
{
	int i;

	isp3_param_write(params_vdev, arg->xa_pnts.gamma_dx0, ISP3X_ISP_GAMMA_DX_LO, id);
	isp3_param_write(params_vdev, arg->xa_pnts.gamma_dx1, ISP3X_ISP_GAMMA_DX_HI, id);

	for (i = 0; i < ISP39_DEGAMMA_CURVE_SIZE; i++) {
		isp3_param_write(params_vdev, arg->curve_r.gamma_y[i],
				 ISP3X_ISP_GAMMA_R_Y_0 + i * 4, id);
		isp3_param_write(params_vdev, arg->curve_g.gamma_y[i],
				 ISP3X_ISP_GAMMA_G_Y_0 + i * 4, id);
		isp3_param_write(params_vdev, arg->curve_b.gamma_y[i],
				 ISP3X_ISP_GAMMA_B_Y_0 + i * 4, id);
	}
}

static void
isp_sdg_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val;

	val = isp3_param_read_cache(params_vdev, ISP3X_ISP_CTRL0, id);
	if (en)
		isp3_param_write(params_vdev, val | CIF_ISP_CTRL_ISP_GAMMA_IN_ENA,
				 ISP3X_ISP_CTRL0, id);
	else
		isp3_param_write(params_vdev, val & ~CIF_ISP_CTRL_ISP_GAMMA_IN_ENA,
				 ISP3X_ISP_CTRL0, id);
}

static void
isp_lsc_matrix_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
			const struct isp3x_lsc_cfg *pconfig,
			bool is_check, u32 id)
{
	u32 data = isp3_param_read(params_vdev, ISP3X_LSC_CTRL, id);
	int i, j;

	if (is_check && (data & ISP3X_LSC_LUT_EN || !(data & ISP_LSC_EN)))
		return;

	isp3_param_write_direct(params_vdev, 0, ISP3X_LSC_R_TABLE_ADDR);
	isp3_param_write_direct(params_vdev, 0, ISP3X_LSC_GR_TABLE_ADDR);
	isp3_param_write_direct(params_vdev, 0, ISP3X_LSC_GB_TABLE_ADDR);
	isp3_param_write_direct(params_vdev, 0, ISP3X_LSC_B_TABLE_ADDR);

	/* program data tables (table size is 9 * 17 = 153) */
	for (i = 0; i < CIF_ISP_LSC_SECTORS_MAX * CIF_ISP_LSC_SECTORS_MAX;
	     i += CIF_ISP_LSC_SECTORS_MAX) {
		/*
		 * 17 sectors with 2 values in one DWORD = 9
		 * DWORDs (2nd value of last DWORD unused)
		 */
		for (j = 0; j < CIF_ISP_LSC_SECTORS_MAX - 1; j += 2) {
			data = ISP_ISP_LSC_TABLE_DATA(pconfig->r_data_tbl[i + j],
						      pconfig->r_data_tbl[i + j + 1]);
			isp3_param_write_direct(params_vdev, data, ISP3X_LSC_R_TABLE_DATA);

			data = ISP_ISP_LSC_TABLE_DATA(pconfig->gr_data_tbl[i + j],
						      pconfig->gr_data_tbl[i + j + 1]);
			isp3_param_write_direct(params_vdev, data, ISP3X_LSC_GR_TABLE_DATA);

			data = ISP_ISP_LSC_TABLE_DATA(pconfig->gb_data_tbl[i + j],
						      pconfig->gb_data_tbl[i + j + 1]);
			isp3_param_write_direct(params_vdev, data, ISP3X_LSC_GB_TABLE_DATA);

			data = ISP_ISP_LSC_TABLE_DATA(pconfig->b_data_tbl[i + j],
						      pconfig->b_data_tbl[i + j + 1]);
			isp3_param_write_direct(params_vdev, data, ISP3X_LSC_B_TABLE_DATA);
		}

		data = ISP_ISP_LSC_TABLE_DATA(pconfig->r_data_tbl[i + j], 0);
		isp3_param_write_direct(params_vdev, data, ISP3X_LSC_R_TABLE_DATA);

		data = ISP_ISP_LSC_TABLE_DATA(pconfig->gr_data_tbl[i + j], 0);
		isp3_param_write_direct(params_vdev, data, ISP3X_LSC_GR_TABLE_DATA);

		data = ISP_ISP_LSC_TABLE_DATA(pconfig->gb_data_tbl[i + j], 0);
		isp3_param_write_direct(params_vdev, data, ISP3X_LSC_GB_TABLE_DATA);

		data = ISP_ISP_LSC_TABLE_DATA(pconfig->b_data_tbl[i + j], 0);
		isp3_param_write_direct(params_vdev, data, ISP3X_LSC_B_TABLE_DATA);
	}
}

static void
isp_lsc_matrix_cfg_ddr(struct rkisp_isp_params_vdev *params_vdev,
		       const struct isp3x_lsc_cfg *pconfig)
{
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 data, buf_idx, *vaddr[4], index[4];
	void *buf_vaddr;
	int i, j;

	memset(&index[0], 0, sizeof(index));
	buf_idx = (priv_val->buf_lsclut_idx++) % ISP39_LSC_LUT_BUF_NUM;
	buf_vaddr = priv_val->buf_lsclut[buf_idx].vaddr;

	vaddr[0] = buf_vaddr;
	vaddr[1] = buf_vaddr + ISP39_LSC_LUT_TBL_SIZE;
	vaddr[2] = buf_vaddr + ISP39_LSC_LUT_TBL_SIZE * 2;
	vaddr[3] = buf_vaddr + ISP39_LSC_LUT_TBL_SIZE * 3;

	/* program data tables (table size is 9 * 17 = 153) */
	for (i = 0; i < CIF_ISP_LSC_SECTORS_MAX * CIF_ISP_LSC_SECTORS_MAX;
	     i += CIF_ISP_LSC_SECTORS_MAX) {
		/*
		 * 17 sectors with 2 values in one DWORD = 9
		 * DWORDs (2nd value of last DWORD unused)
		 */
		for (j = 0; j < CIF_ISP_LSC_SECTORS_MAX - 1; j += 2) {
			data = ISP_ISP_LSC_TABLE_DATA(pconfig->r_data_tbl[i + j],
						      pconfig->r_data_tbl[i + j + 1]);
			vaddr[0][index[0]++] = data;

			data = ISP_ISP_LSC_TABLE_DATA(pconfig->gr_data_tbl[i + j],
						      pconfig->gr_data_tbl[i + j + 1]);
			vaddr[1][index[1]++] = data;

			data = ISP_ISP_LSC_TABLE_DATA(pconfig->b_data_tbl[i + j],
						      pconfig->b_data_tbl[i + j + 1]);
			vaddr[2][index[2]++] = data;

			data = ISP_ISP_LSC_TABLE_DATA(pconfig->gb_data_tbl[i + j],
						      pconfig->gb_data_tbl[i + j + 1]);
			vaddr[3][index[3]++] = data;
		}

		data = ISP_ISP_LSC_TABLE_DATA(pconfig->r_data_tbl[i + j], 0);
		vaddr[0][index[0]++] = data;

		data = ISP_ISP_LSC_TABLE_DATA(pconfig->gr_data_tbl[i + j], 0);
		vaddr[1][index[1]++] = data;

		data = ISP_ISP_LSC_TABLE_DATA(pconfig->b_data_tbl[i + j], 0);
		vaddr[2][index[2]++] = data;

		data = ISP_ISP_LSC_TABLE_DATA(pconfig->gb_data_tbl[i + j], 0);
		vaddr[3][index[3]++] = data;
	}
	rkisp_prepare_buffer(params_vdev->dev, &priv_val->buf_lsclut[buf_idx]);
	data = priv_val->buf_lsclut[buf_idx].dma_addr;
	isp3_param_write(params_vdev, data, ISP3X_MI_LUT_LSC_RD_BASE, 0);
	isp3_param_write(params_vdev, ISP39_LSC_LUT_TBL_SIZE, ISP3X_MI_LUT_LSC_RD_WSIZE, 0);
}

static void
isp_lsc_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp3x_lsc_cfg *arg, u32 id)
{
	struct isp39_isp_params_cfg *params_rec = params_vdev->isp39_params + id;
	struct rkisp_device *dev = params_vdev->dev;
	u32 data, lsc_ctrl;
	int i;

	lsc_ctrl = isp3_param_read(params_vdev, ISP3X_LSC_CTRL, id);
	/* one lsc sram table */
	if (!IS_HDR_RDBK(dev->rd_mode)) {
		lsc_ctrl |= ISP3X_LSC_LUT_EN;
		isp_lsc_matrix_cfg_ddr(params_vdev, arg);
	} else {
		if (dev->hw_dev->is_single)
			isp_lsc_matrix_cfg_sram(params_vdev, arg, false, id);
	}
	params_rec->others.lsc_cfg = *arg;
	for (i = 0; i < ISP39_LSC_SIZE_TBL_SIZE / 4; i++) {
		/* program x size tables */
		data = CIF_ISP_LSC_SECT_SIZE(arg->x_size_tbl[i * 2], arg->x_size_tbl[i * 2 + 1]);
		isp3_param_write(params_vdev, data, ISP3X_LSC_XSIZE_01 + i * 4, id);
		data = CIF_ISP_LSC_SECT_SIZE(arg->x_size_tbl[i * 2 + 8], arg->x_size_tbl[i * 2 + 9]);
		isp3_param_write(params_vdev, data, ISP3X_LSC_XSIZE_89 + i * 4, id);

		/* program x grad tables */
		data = CIF_ISP_LSC_SECT_SIZE(arg->x_grad_tbl[i * 2], arg->x_grad_tbl[i * 2 + 1]);
		isp3_param_write(params_vdev, data, ISP3X_LSC_XGRAD_01 + i * 4, id);
		data = CIF_ISP_LSC_SECT_SIZE(arg->x_grad_tbl[i * 2 + 8], arg->x_grad_tbl[i * 2 + 9]);
		isp3_param_write(params_vdev, data, ISP3X_LSC_XGRAD_89 + i * 4, id);

		/* program y size tables */
		data = CIF_ISP_LSC_SECT_SIZE(arg->y_size_tbl[i * 2], arg->y_size_tbl[i * 2 + 1]);
		isp3_param_write(params_vdev, data, ISP3X_LSC_YSIZE_01 + i * 4, id);
		data = CIF_ISP_LSC_SECT_SIZE(arg->y_size_tbl[i * 2 + 8], arg->y_size_tbl[i * 2 + 9]);
		isp3_param_write(params_vdev, data, ISP3X_LSC_YSIZE_89 + i * 4, id);

		/* program y grad tables */
		data = CIF_ISP_LSC_SECT_SIZE(arg->y_grad_tbl[i * 2], arg->y_grad_tbl[i * 2 + 1]);
		isp3_param_write(params_vdev, data, ISP3X_LSC_YGRAD_01 + i * 4, id);
		data = CIF_ISP_LSC_SECT_SIZE(arg->y_grad_tbl[i * 2 + 8], arg->y_grad_tbl[i * 2 + 9]);
		isp3_param_write(params_vdev, data, ISP3X_LSC_YGRAD_89 + i * 4, id);
	}

	if (arg->sector_16x16)
		lsc_ctrl |= ISP3X_LSC_SECTOR_16X16;
	else
		lsc_ctrl &= ~ISP3X_LSC_SECTOR_16X16;
	isp3_param_write(params_vdev, lsc_ctrl, ISP3X_LSC_CTRL, id);
}

static void
isp_lsc_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_LSC_CTRL, id);

	if (en == !!(val & ISP_LSC_EN))
		return;

	if (en) {
		val = ISP_LSC_EN | ISP39_SELF_FORCE_UPD;
		if (!IS_HDR_RDBK(params_vdev->dev->rd_mode))
			val |= ISP3X_LSC_LUT_EN;
		isp3_param_set_bits(params_vdev, ISP3X_LSC_CTRL, val, id);
	} else {
		isp3_param_clear_bits(params_vdev, ISP3X_LSC_CTRL, ISP_LSC_EN, id);
	}
}

static void
isp_debayer_config(struct rkisp_isp_params_vdev *params_vdev,
		   const struct isp39_debayer_cfg *arg, u32 id)
{
	u32 i, value;

	value = isp3_param_read(params_vdev, ISP3X_DEBAYER_CONTROL, id);
	value &= ISP_DEBAYER_EN;

	value |= !!arg->filter_g_en << 4 | !!arg->filter_c_en << 8;
	isp3_param_write(params_vdev, value, ISP3X_DEBAYER_CONTROL, id);

	value = 0;
	for (i = 0; i < ISP39_DEBAYER_LUMA_NUM; i++)
		value |= ((arg->luma_dx[i] & 0xf) << (i * 4));
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_LUMA_DX, id);

	value = (arg->max_ratio & 0x3F) << 24 | arg->select_thed << 16 |
		(arg->thed1 & 0x0F) << 12 | (arg->thed0 & 0x0F) << 8 |
		(arg->dist_scale & 0x0F) << 4 | !!arg->clip_en;
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP, id);

	value = (arg->filter1_coe4 & 0x1F) << 24 | (arg->filter1_coe3 & 0x1F) << 16 |
		(arg->filter1_coe2 & 0x1F) << 8 | (arg->filter1_coe1 & 0x1F);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP_FILTER1, id);

	value = (arg->filter2_coe4 & 0x1F) << 24 | (arg->filter2_coe3 & 0x1F) << 16 |
		(arg->filter2_coe2 & 0x1F) << 8 | (arg->filter2_coe1 & 0x1F);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP_FILTER2, id);

	value = (arg->wgt_alpha & 0x7f) << 24 | (arg->gradloflt_alpha & 0x7f) << 16 |
		(arg->gain_offset & 0xfff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP_OFFSET_ALPHA, id);

	for (i = 0; i < ISP39_DEBAYER_DRCT_OFFSET_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->drct_offset[i * 2], arg->drct_offset[i * 2 + 1]);
		isp3_param_write(params_vdev, value,
				 ISP39_DEBAYER_G_INTERP_DRCT_OFFSET0 + i * 4, id);
	}

	value = (arg->offset & 0x7ff) << 16 | (arg->bf_ratio & 0xfff) << 4 | !!arg->gfilter_mode;
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_FILTER_MODE_OFFSET, id);

	value = ISP_PACK_4BYTE(arg->filter_coe0, arg->filter_coe1, arg->filter_coe2, 0);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_FILTER_FILTER, id);

	for (i = 0; i < ISP39_DEBAYER_VSIGMA_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->vsigma[i * 2], arg->vsigma[i * 2 + 1]);
		isp3_param_write(params_vdev, value,
				 ISP39_DEBAYER_G_FILTER_VSIGMA0 + i * 4, id);
	}

	value = ISP_PACK_4BYTE(arg->guid_gaus_coe0, arg->guid_gaus_coe1, arg->guid_gaus_coe2, 0);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_GUIDE_GAUS, id);

	value = ISP_PACK_4BYTE(arg->ce_gaus_coe0, arg->ce_gaus_coe1, arg->ce_gaus_coe2, 0);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_CE_GAUS, id);

	value = ISP_PACK_4BYTE(arg->alpha_gaus_coe0, arg->alpha_gaus_coe1, arg->alpha_gaus_coe2, 0);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_ALPHA_GAUS, id);

	value = !!arg->log_en << 31 | (arg->loggd_offset & 0xfff) << 16 | (arg->loghf_offset & 0x1fff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_LOG_OFFSET, id);

	value = (arg->alpha_scale & 0xfffff) << 12 | (arg->alpha_offset & 0xfff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_ALPHA, id);

	value = (arg->edge_scale & 0xfffff) << 12 | (arg->edge_offset & 0xfff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_EDGE, id);

	value = (arg->wgtslope & 0xfff) << 16 | (arg->exp_shift & 0x3f) << 8 | (arg->ce_sgm & 0xff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_IIR_0, id);

	value = (arg->wet_ghost & 0x3f) << 8 | (arg->wet_clip & 0x7f);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_IIR_1, id);

	value = (arg->bf_curwgt & 0x7f) << 24 | (arg->bf_clip & 0x7f) << 16 | (arg->bf_sgm & 0xffff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_BF, id);
}

static void
isp_debayer_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	if (en)
		isp3_param_set_bits(params_vdev,
				    ISP3X_DEBAYER_CONTROL, ISP39_MODULE_EN, id);
	else
		isp3_param_clear_bits(params_vdev,
				      ISP3X_DEBAYER_CONTROL, ISP39_MODULE_EN, id);
}

static void
isp_awbgain_config(struct rkisp_isp_params_vdev *params_vdev,
		   const struct isp32_awb_gain_cfg *arg,
		   enum rkisp_params_type type, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;

	if (!arg->gain0_red || !arg->gain0_blue ||
	    !arg->gain1_red || !arg->gain1_blue ||
	    !arg->gain2_red || !arg->gain2_blue ||
	    !arg->gain0_green_r || !arg->gain0_green_b ||
	    !arg->gain1_green_r || !arg->gain1_green_b ||
	    !arg->gain2_green_r || !arg->gain2_green_b) {
		dev_err(dev->dev, "awb gain is zero!\n");
		return;
	}

	if (!dev->is_aiisp_en || type == RKISP_PARAMS_ALL || type == RKISP_PARAMS_LAT) {
		isp3_param_write(params_vdev,
				 ISP_PACK_2SHORT(arg->awb1_gain_gb, arg->awb1_gain_gr),
				 ISP32_ISP_AWB1_GAIN_G, id);
		isp3_param_write(params_vdev,
				 ISP_PACK_2SHORT(arg->awb1_gain_b, arg->awb1_gain_r),
				 ISP32_ISP_AWB1_GAIN_RB, id);
		if (type == RKISP_PARAMS_LAT)
			return;
	}

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->gain0_green_b, arg->gain0_green_r),
			 ISP3X_ISP_AWB_GAIN0_G, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->gain0_blue, arg->gain0_red),
			 ISP3X_ISP_AWB_GAIN0_RB, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->gain1_green_b, arg->gain1_green_r),
			 ISP3X_ISP_AWB_GAIN1_G, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->gain1_blue, arg->gain1_red),
			 ISP3X_ISP_AWB_GAIN1_RB, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->gain2_green_b, arg->gain2_green_r),
			 ISP3X_ISP_AWB_GAIN2_G, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->gain2_blue, arg->gain2_red),
			 ISP3X_ISP_AWB_GAIN2_RB, id);
}

static void
isp_awbgain_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val;

	val = isp3_param_read_cache(params_vdev, ISP3X_ISP_CTRL0, id);
	if (en)
		isp3_param_write(params_vdev, val | CIF_ISP_CTRL_ISP_AWB_ENA,
				 ISP3X_ISP_CTRL0, id);
	else
		isp3_param_write(params_vdev, val & ~CIF_ISP_CTRL_ISP_AWB_ENA,
				 ISP3X_ISP_CTRL0, id);
}

static void
isp_ccm_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp39_ccm_cfg *arg, u32 id)
{
	u32 value;
	u32 i;

	value = isp3_param_read(params_vdev, ISP3X_CCM_CTRL, id);
	value &= ISP_CCM_EN;

	value |= !!arg->sat_decay_en << 4 | !!arg->asym_adj_en << 3 |
		 !!arg->enh_adj_en << 2 | !!arg->highy_adjust_dis << 1;
	isp3_param_write(params_vdev, value, ISP3X_CCM_CTRL, id);

	value = ISP_PACK_2SHORT(arg->coeff0_r, arg->coeff1_r);
	isp3_param_write(params_vdev, value, ISP3X_CCM_COEFF0_R, id);

	value = ISP_PACK_2SHORT(arg->coeff2_r, arg->offset_r);
	isp3_param_write(params_vdev, value, ISP3X_CCM_COEFF1_R, id);

	value = ISP_PACK_2SHORT(arg->coeff0_g, arg->coeff1_g);
	isp3_param_write(params_vdev, value, ISP3X_CCM_COEFF0_G, id);

	value = ISP_PACK_2SHORT(arg->coeff2_g, arg->offset_g);
	isp3_param_write(params_vdev, value, ISP3X_CCM_COEFF1_G, id);

	value = ISP_PACK_2SHORT(arg->coeff0_b, arg->coeff1_b);
	isp3_param_write(params_vdev, value, ISP3X_CCM_COEFF0_B, id);

	value = ISP_PACK_2SHORT(arg->coeff2_b, arg->offset_b);
	isp3_param_write(params_vdev, value, ISP3X_CCM_COEFF1_B, id);

	value = ISP_PACK_2SHORT(arg->coeff0_y, arg->coeff1_y);
	isp3_param_write(params_vdev, value, ISP3X_CCM_COEFF0_Y, id);

	value = ISP_PACK_2SHORT(arg->coeff2_y, 0);
	isp3_param_write(params_vdev, value, ISP3X_CCM_COEFF1_Y, id);

	for (i = 0; i < ISP39_CCM_CURVE_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->alp_y[2 * i], arg->alp_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_CCM_ALP_Y0 + 4 * i, id);
	}

	value = (arg->right_bit & 0xf) << 4 | (arg->bound_bit & 0xf);
	isp3_param_write(params_vdev, value, ISP3X_CCM_BOUND_BIT, id);

	value = (arg->color_coef1_g2y & 0x7ff) << 16 |
		(arg->color_coef0_r2y & 0x7ff);
	isp3_param_write(params_vdev, value, ISP32_CCM_ENHANCE0, id);

	value = (arg->color_enh_rat_max & 0x3fff) << 16 |
		(arg->color_coef2_b2y & 0x7ff);
	isp3_param_write(params_vdev, value, ISP32_CCM_ENHANCE1, id);

	value = (arg->hf_scale & 0x3fff) << 16 | (arg->hf_up & 0xff) << 8 | (arg->hf_low & 0xff);
	isp3_param_write(params_vdev, value, ISP39_CCM_HF_THD, id);

	for (i = 0; i < ISP39_CCM_HF_FACTOR_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->hf_factor[i * 2], arg->hf_factor[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_HF_FACTOR0 + i * 4, id);
	}
	value = ISP_PACK_2SHORT(arg->hf_factor[i * 2], 0);
	isp3_param_write(params_vdev, value, ISP39_HF_FACTOR0 + i * 4, id);
}

static void
isp_ccm_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	if (en)
		isp3_param_set_bits(params_vdev, ISP3X_CCM_CTRL, ISP_CCM_EN, id);
	else
		isp3_param_clear_bits(params_vdev, ISP3X_CCM_CTRL, ISP_CCM_EN, id);
}

static void
isp_goc_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp3x_gammaout_cfg *arg, u32 id)
{
	int i;
	u32 value;

	value = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_CTRL, id);
	value &= ISP3X_GAMMA_OUT_EN;
	value |= !!arg->equ_segm << 1 | !!arg->finalx4_dense_en << 2;
	isp3_param_write(params_vdev, value, ISP3X_GAMMA_OUT_CTRL, id);

	isp3_param_write(params_vdev, arg->offset, ISP3X_GAMMA_OUT_OFFSET, id);
	for (i = 0; i < ISP39_GAMMA_OUT_MAX_SAMPLES / 2; i++) {
		value = ISP_PACK_2SHORT(arg->gamma_y[2 * i],
					arg->gamma_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_GAMMA_OUT_Y0 + i * 4, id);
	}
	isp3_param_write(params_vdev, arg->gamma_y[2 * i], ISP3X_GAMMA_OUT_Y0 + i * 4, id);
}

static void
isp_goc_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	if (en)
		isp3_param_set_bits(params_vdev,
				    ISP3X_GAMMA_OUT_CTRL, ISP3X_GAMMA_OUT_EN, id);
	else
		isp3_param_clear_bits(params_vdev,
				      ISP3X_GAMMA_OUT_CTRL, ISP3X_GAMMA_OUT_EN, id);
}

static void
isp_cproc_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp2x_cproc_cfg *arg, u32 id)
{
	u32 quantization = params_vdev->quantization;

	isp3_param_write(params_vdev, arg->contrast, ISP3X_CPROC_CONTRAST, id);
	isp3_param_write(params_vdev, arg->hue, ISP3X_CPROC_HUE, id);
	isp3_param_write(params_vdev, arg->sat, ISP3X_CPROC_SATURATION, id);
	isp3_param_write(params_vdev, arg->brightness, ISP3X_CPROC_BRIGHTNESS, id);

	if (quantization != V4L2_QUANTIZATION_FULL_RANGE) {
		isp3_param_clear_bits(params_vdev, ISP3X_CPROC_CTRL,
				      CIF_C_PROC_YOUT_FULL |
				      CIF_C_PROC_YIN_FULL |
				      CIF_C_PROC_COUT_FULL, id);
	} else {
		isp3_param_set_bits(params_vdev, ISP3X_CPROC_CTRL,
				    CIF_C_PROC_YOUT_FULL |
				    CIF_C_PROC_YIN_FULL |
				    CIF_C_PROC_COUT_FULL, id);
	}
}

static void
isp_cproc_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	if (en)
		isp3_param_set_bits(params_vdev, ISP3X_CPROC_CTRL,
				    CIF_C_PROC_CTR_ENABLE, id);
	else
		isp3_param_clear_bits(params_vdev, ISP3X_CPROC_CTRL,
				      CIF_C_PROC_CTR_ENABLE, id);
}

static void
isp_ie_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = 0;

	if (en)
		val =  CIF_IMG_EFF_CTRL_ENABLE;
	isp3_param_write(params_vdev, val, ISP3X_IMG_EFF_CTRL, id);
}

static void
isp_rawaf_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp39_rawaf_meas_cfg *arg, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	u32 width = out_crop->width, height = out_crop->height;
	u32 i, var, ctrl;
	u16 h_size, v_size;
	u16 h_offs, v_offs;
	u8 gaus_en, viir_en;
	size_t num_of_win = min_t(size_t, ARRAY_SIZE(arg->win), arg->num_afm_win);

	if (dev->unite_div > ISP_UNITE_DIV1)
		width = width / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;
	if (dev->unite_div == ISP_UNITE_DIV4)
		height = height / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;

	for (i = 0; i < num_of_win; i++) {
		h_size = arg->win[i].h_size;
		v_size = arg->win[i].v_size;
		h_offs = arg->win[i].h_offs < 2 ? 2 : arg->win[i].h_offs;
		v_offs = arg->win[i].v_offs < 1 ? 1 : arg->win[i].v_offs;

		if (!v_size || v_size + v_offs - 2 > height)
			v_size = height - v_offs - 2;
		if (!h_size || h_size + h_offs - 2 > width)
			h_size = width - h_offs - 2;

		if (i == 0) {
			h_size = h_size / 15 * 15;
			v_size = v_size / 15 * 15;
		}

		/*
		 * (horizontal left row), value must be greater or equal 2
		 * (vertical top line), value must be greater or equal 1
		 */
		isp3_param_write(params_vdev,
				 ISP_PACK_2SHORT(v_offs, h_offs),
				 ISP3X_RAWAF_OFFSET_WINA + i * 8, id);

		/*
		 * value must be smaller than [width of picture -2]
		 * value must be lower than (number of lines -2)
		 */
		isp3_param_write(params_vdev,
				 ISP_PACK_2SHORT(v_size, h_size),
				 ISP3X_RAWAF_SIZE_WINA + i * 8, id);
	}

	var = (arg->tnrin_shift & 0xf) << 20 |
	      (arg->hldg_dilate_num & 0x7) << 16 |
	      !!arg->aehgl_en << 13 | !!arg->bls_en << 12 |
	      (arg->bls_offset & 0x1FF);
	isp3_param_write(params_vdev, var, ISP32L_RAWAF_CTRL1, id);

	for (i = 0; i < ISP39_RAWAF_GAMMA_NUM / 2; i++) {
		var = ISP_PACK_2SHORT(arg->gamma_y[2 * i], arg->gamma_y[2 * i + 1]);
		isp3_param_write(params_vdev, var, ISP3X_RAWAF_GAMMA_Y0 + i * 4, id);
	}
	var = ISP_PACK_2SHORT(arg->gamma_y[16], 0);
	isp3_param_write(params_vdev, var, ISP3X_RAWAF_GAMMA_Y8, id);

	var = (arg->v2iir_shift_winb & 0xf) << 28 | (arg->v1iir_shift_winb & 0xf) << 24 |
	      (arg->h2iir_shift_winb & 0xf) << 20 | (arg->h1iir_shift_winb & 0xf) << 16 |
	      (arg->v2iir_shift_wina & 0x7) << 12 | (arg->v1iir_shift_wina & 0x7) << 8 |
	      (arg->h2iir_shift_wina & 0x7) << 4 | (arg->h1iir_shift_wina & 0x7);
	isp3_param_write(params_vdev, var, ISP39_RAWAF_HVIIR_VAR_SHIFT, id);

	var = ISP_PACK_2SHORT(arg->h_fv_thresh, arg->v_fv_thresh);
	isp3_param_write(params_vdev, var, ISP3X_RAWAF_HIIR_THRESH, id);

	for (i = 0; i < ISP39_RAWAF_VFIR_COE_NUM; i++) {
		var = ISP_PACK_2SHORT(arg->v1fir_coe[i], arg->v2fir_coe[i]);
		isp3_param_write(params_vdev, var, ISP32_RAWAF_V_FIR_COE0 + i * 4, id);
	}

	for (i = 0; i < ISP39_RAWAF_GAUS_COE_NUM / 4; i++) {
		var = ISP_PACK_4BYTE(arg->gaus_coe[i * 4], arg->gaus_coe[i * 4 + 1],
				     arg->gaus_coe[i * 4 + 2], arg->gaus_coe[i * 4 + 3]);
		isp3_param_write(params_vdev, var, ISP32_RAWAF_GAUS_COE03 + i * 4, id);
	}
	var = ISP_PACK_4BYTE(arg->gaus_coe[ISP32_RAWAF_GAUS_COE_NUM - 1], 0, 0, 0);
	isp3_param_write(params_vdev, var, ISP32_RAWAF_GAUS_COE8, id);

	isp3_param_write(params_vdev, arg->highlit_thresh, ISP3X_RAWAF_HIGHLIT_THRESH, id);

	var = ISP_PACK_2SHORT(arg->h_fv_limit, arg->h_fv_slope);
	isp3_param_write(params_vdev, var, ISP32L_RAWAF_CORING_H, id);
	var = ISP_PACK_2SHORT(arg->v_fv_limit, arg->v_fv_slope);
	isp3_param_write(params_vdev, var, ISP32L_RAWAF_CORING_V, id);

	if (!arg->hiir_en || !arg->viir_en || !arg->aehgl_en)
		dev_err(params_vdev->dev->dev,
			"af hiir:%d viir:%d aehgl:%d no enable together\n",
			arg->hiir_en, arg->viir_en, arg->aehgl_en);
	viir_en = arg->viir_en;
	gaus_en = arg->gaus_en;

	ctrl = isp3_param_read(params_vdev, ISP3X_RAWAF_CTRL, id);
	ctrl &= ISP3X_RAWAF_EN;
	if (arg->hiir_en) {
		ctrl |= ISP3X_RAWAF_HIIR_EN;
		for (i = 0; i < ISP39_RAWAF_HIIR_COE_NUM / 2; i++) {
			var = ISP_PACK_2SHORT(arg->h1iir1_coe[i * 2], arg->h1iir1_coe[i * 2 + 1]);
			isp3_param_write(params_vdev, var, ISP3X_RAWAF_H1_IIR1_COE01 + i * 4, id);
			var = ISP_PACK_2SHORT(arg->h1iir2_coe[i * 2], arg->h1iir2_coe[i * 2 + 1]);
			isp3_param_write(params_vdev, var, ISP3X_RAWAF_H1_IIR2_COE01 + i * 4, id);

			var = ISP_PACK_2SHORT(arg->h2iir1_coe[i * 2], arg->h2iir1_coe[i * 2 + 1]);
			isp3_param_write(params_vdev, var, ISP3X_RAWAF_H2_IIR1_COE01 + i * 4, id);
			var = ISP_PACK_2SHORT(arg->h2iir2_coe[i * 2], arg->h2iir2_coe[i * 2 + 1]);
			isp3_param_write(params_vdev, var, ISP3X_RAWAF_H2_IIR2_COE01 + i * 4, id);
		}
	}
	if (viir_en) {
		ctrl |= ISP3X_RAWAF_VIIR_EN;
		for (i = 0; i < ISP39_RAWAF_VIIR_COE_NUM; i++) {
			var = ISP_PACK_2SHORT(arg->v1iir_coe[i], arg->v2iir_coe[i]);
			isp3_param_write(params_vdev, var, ISP3X_RAWAF_V_IIR_COE0 + i * 4, id);
		}
	}
	if (arg->ldg_en) {
		ctrl |= ISP3X_RAWAF_LDG_EN;
		for (i = 0; i < ISP39_RAWAF_CURVE_NUM; i++) {
			isp3_param_write(params_vdev,
					 arg->curve_h[i].ldg_lumth |
					 arg->curve_h[i].ldg_gain << 8 |
					 arg->curve_h[i].ldg_gslp << 16,
					 ISP3X_RAWAF_H_CURVEL + i * 16, id);
			isp3_param_write(params_vdev,
					 arg->curve_v[i].ldg_lumth |
					 arg->curve_v[i].ldg_gain << 8 |
					 arg->curve_v[i].ldg_gslp << 16,
					 ISP3X_RAWAF_V_CURVEL + i * 16, id);
		}
	}

	ctrl |= !!gaus_en << 2 | !!arg->gamma_en << 1 |
		!!arg->v1_fv_mode << 10 | !!arg->h1_fv_mode << 8 |
		!!arg->v2_fv_mode << 11 | !!arg->h2_fv_mode << 9 |
		!!arg->y_mode << 13 | !!arg->ae_mode << 12 |
		!!arg->vldg_sel << 14 | (arg->v_dnscl_mode & 0x3) << 16 |
		!!arg->ae_config_use << 20 | !!arg->from_ynr << 19 |
		!!arg->hiir_left_border_mode << 21 | !!arg->avg_ds_en << 22 |
		!!arg->avg_ds_mode << 23 | !!arg->h1_acc_mode << 24 |
		!!arg->h2_acc_mode << 25 | !!arg->v1_acc_mode << 26 |
		!!arg->v2_acc_mode << 27;
	isp3_param_write(params_vdev, ctrl, ISP3X_RAWAF_CTRL, id);

	ctrl = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, id);
	ctrl &= ~(ISP3X_RAWAF_SEL(3) | ISP32L_BNR2AF_SEL);
	ctrl |= ISP3X_RAWAF_SEL(arg->rawaf_sel) | !!arg->bnr2af_sel << 28;
	isp3_param_write(params_vdev, ctrl, ISP3X_VI_ISP_PATH, id);
}

static void
isp_rawaf_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 afm_ctrl = isp3_param_read(params_vdev, ISP3X_RAWAF_CTRL, id);

	afm_ctrl &= ~ISP39_REG_WR_MASK;
	if (en)
		afm_ctrl |= ISP3X_RAWAF_EN;
	else
		afm_ctrl &= ~ISP3X_RAWAF_EN;

	isp3_param_write(params_vdev, afm_ctrl, ISP3X_RAWAF_CTRL, id);
}

static void
isp_rawaebig_config(struct rkisp_isp_params_vdev *params_vdev,
		    const struct isp2x_rawaebig_meas_cfg *arg,
		    u32 addr, u32 id)
{
	struct rkisp_device *ispdev = params_vdev->dev;
	struct v4l2_rect *out_crop = &ispdev->isp_sdev.out_crop;
	u32 width = out_crop->width, height = out_crop->height;
	u32 i, value, h_size, v_size, h_offs, v_offs;
	u32 block_hsize, block_vsize, wnd_num_idx = 0;
	const u32 ae_wnd_num[] = {
		1, 5, 15, 15
	};

	/* avoid to override the old enable value */
	value = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_CTRL, id);
	value &= ISP3X_RAWAE_BIG_EN;

	wnd_num_idx = arg->wnd_num;
	if (wnd_num_idx >= ARRAY_SIZE(ae_wnd_num)) {
		wnd_num_idx = ARRAY_SIZE(ae_wnd_num) - 1;
		dev_err(params_vdev->dev->dev,
			"%s invalid wnd_num:%d, set to %d\n",
			__func__, arg->wnd_num, wnd_num_idx);
	}
	value |= ISP3X_RAWAE_BIG_WND0_NUM(wnd_num_idx);

	if (arg->subwin_en[0])
		value |= ISP3X_RAWAE_BIG_WND1_EN;
	if (arg->subwin_en[1])
		value |= ISP3X_RAWAE_BIG_WND2_EN;
	if (arg->subwin_en[2])
		value |= ISP3X_RAWAE_BIG_WND3_EN;
	if (arg->subwin_en[3])
		value |= ISP3X_RAWAE_BIG_WND4_EN;
	isp3_param_write(params_vdev, value, addr + ISP3X_RAWAE_BIG_CTRL, id);

	h_offs = arg->win.h_offs & ~0x1;
	v_offs = arg->win.v_offs & ~0x1;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(h_offs, v_offs),
			 addr + ISP3X_RAWAE_BIG_OFFSET, id);

	if (ispdev->unite_div > ISP_UNITE_DIV1)
		width = width / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;
	if (ispdev->unite_div == ISP_UNITE_DIV4)
		height = height / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;

	h_size = arg->win.h_size;
	v_size = arg->win.v_size;
	if (!h_size || h_size + h_offs + 1 > width)
		h_size = width - h_offs - 1;
	if (!v_size || v_size + v_offs + 2 > height)
		v_size = height - v_offs - 2;
	block_hsize = (h_size / ae_wnd_num[wnd_num_idx]) & ~0x1;
	block_vsize = (v_size / ae_wnd_num[wnd_num_idx]) & ~0x1;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(block_hsize, block_vsize),
			 addr + ISP3X_RAWAE_BIG_BLK_SIZE, id);

	for (i = 0; i < ISP39_RAWAEBIG_SUBWIN_NUM; i++) {
		h_offs = arg->subwin[i].h_offs & ~0x1;
		v_offs = arg->subwin[i].v_offs & ~0x1;
		isp3_param_write(params_vdev, ISP_PACK_2SHORT(h_offs, v_offs),
				 addr + ISP3X_RAWAE_BIG_WND1_OFFSET + 8 * i, id);

		v_size = arg->subwin[i].v_size;
		h_size = arg->subwin[i].h_size;
		if (!h_size || h_size + h_offs > width)
			h_size = width - h_offs;
		if (!v_size || v_size + v_offs > height)
			v_size = height - v_offs;
		h_size = (h_size + h_offs) & ~0x1;
		v_size = (v_size + v_offs) & ~0x1;
		isp3_param_write(params_vdev, ISP_PACK_2SHORT(h_size, v_size),
				 addr + ISP3X_RAWAE_BIG_WND1_SIZE + 8 * i, id);
	}

	value = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, id);
	if (addr == ISP3X_RAWAE_BIG1_BASE) {
		value &= ~(ISP3X_RAWAE3_SEL(3) | BIT(29));
		value |= ISP3X_RAWAE3_SEL(arg->rawae_sel & 0xf);
		if (arg->rawae_sel & ISP39_BNR2AEBIG_SEL_EN)
			value |= BIT(29);
		isp3_param_write(params_vdev, value, ISP3X_VI_ISP_PATH, id);
	} else {
		value &= ~(ISP3X_RAWAE012_SEL(3) | BIT(30));
		value |= ISP3X_RAWAE012_SEL(arg->rawae_sel & 0xf);
		if (arg->rawae_sel & ISP39_BNR2AE0_SEL_EN)
			value |= BIT(30);
		isp3_param_write(params_vdev, value, ISP3X_VI_ISP_PATH, id);
	}
}

static void
isp_rawaebig_enable(struct rkisp_isp_params_vdev *params_vdev,
		    bool en, u32 addr, u32 id)
{
	u32 exp_ctrl;

	exp_ctrl = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_CTRL, id);
	exp_ctrl &= ~ISP39_REG_WR_MASK;
	if (en)
		exp_ctrl |= ISP39_MODULE_EN;
	else
		exp_ctrl &= ~ISP39_MODULE_EN;

	isp3_param_write(params_vdev, exp_ctrl, addr + ISP3X_RAWAE_BIG_CTRL, id);
}

static void
isp_rawae0_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp2x_rawaebig_meas_cfg *arg, u32 id)
{
	isp_rawaebig_config(params_vdev, arg, ISP3X_RAWAE_LITE_BASE, id);
}

static void
isp_rawae0_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	isp_rawaebig_enable(params_vdev, en, ISP3X_RAWAE_LITE_BASE, id);
}

static void
isp_rawae3_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp2x_rawaebig_meas_cfg *arg, u32 id)
{
	isp_rawaebig_config(params_vdev, arg, ISP3X_RAWAE_BIG1_BASE, id);
}

static void
isp_rawae3_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	isp_rawaebig_enable(params_vdev, en, ISP3X_RAWAE_BIG1_BASE, id);
}

static void
isp_rawawb_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
		    const struct isp39_rawawb_meas_cfg *arg, bool is_check, u32 id)
{
	u32 i, val = isp3_param_read(params_vdev, ISP3X_RAWAWB_CTRL, id);

	if (is_check && !(val & ISP39_MODULE_EN))
		return;

	for (i = 0; i < ISP39_RAWAWB_WEIGHT_NUM / 5; i++) {
		isp3_param_write(params_vdev,
				 (arg->wp_blk_wei_w[5 * i] & 0x3f) |
				 (arg->wp_blk_wei_w[5 * i + 1] & 0x3f) << 6 |
				 (arg->wp_blk_wei_w[5 * i + 2] & 0x3f) << 12 |
				 (arg->wp_blk_wei_w[5 * i + 3] & 0x3f) << 18 |
				 (arg->wp_blk_wei_w[5 * i + 4] & 0x3f) << 24,
				 ISP3X_RAWAWB_WRAM_DATA_BASE, id);
	}
}

static void
isp_rawawb_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp39_rawawb_meas_cfg *arg, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	struct isp39_isp_params_cfg *params_rec = params_vdev->isp39_params + id;
	struct isp39_rawawb_meas_cfg *arg_rec = &params_rec->meas.rawawb;
	const struct isp2x_bls_fixed_val *pval = &arg->bls2_val;
	u32 width = out_crop->width, height = out_crop->height;
	u32 value, val, mask, h_size, v_size, h_offs, v_offs;

	/* bug no base on bayer pattern */
	isp3_param_write(params_vdev, pval->r, ISP32_BLS2_A_FIXED, id);
	isp3_param_write(params_vdev, pval->gr, ISP32_BLS2_B_FIXED, id);
	isp3_param_write(params_vdev, pval->gb, ISP32_BLS2_C_FIXED, id);
	isp3_param_write(params_vdev, pval->b, ISP32_BLS2_D_FIXED, id);

	value = isp3_param_read(params_vdev, ISP3X_BLS_CTRL, id);
	value &= ~ISP32_BLS_BLS2_EN;
	if (arg->bls2_en)
		value |= ISP32_BLS_BLS2_EN;
	isp3_param_write(params_vdev, value, ISP3X_BLS_CTRL, id);

	value = arg->in_overexposure_threshold << 16 |
		!!arg->blk_with_luma_wei_en << 8 |
		!!arg->ds16x8_mode_en << 7 |
		(arg->blk_measure_illu_idx & 0x7) << 4 |
		!!arg->blk_rtdw_measure_en << 3 |
		!!arg->blk_measure_xytype << 2 |
		!!arg->blk_measure_mode << 1 |
		!!arg->blk_measure_enable;
	isp3_param_write(params_vdev, value, ISP3X_RAWAWB_BLK_CTRL, id);

	h_offs = arg->h_offs & ~0x1;
	v_offs = arg->v_offs & ~0x1;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(h_offs, v_offs),
			 ISP3X_RAWAWB_WIN_OFFS, id);

	if (dev->unite_div > ISP_UNITE_DIV1)
		width = width / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;
	if (dev->unite_div == ISP_UNITE_DIV4)
		height = height / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;

	h_size = arg->h_size;
	v_size = arg->v_size;
	if (!h_size || h_size + h_offs > width)
		h_size = width - h_offs;
	if (!v_size || v_size + v_offs > height)
		v_size = height - v_offs;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(h_size, v_size),
			 ISP3X_RAWAWB_WIN_SIZE, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->r_max, arg->g_max),
			 ISP3X_RAWAWB_LIMIT_RG_MAX, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->b_max, arg->y_max),
			 ISP3X_RAWAWB_LIMIT_BY_MAX, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->r_min, arg->g_min),
			 ISP3X_RAWAWB_LIMIT_RG_MIN, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->b_min, arg->y_min),
			 ISP3X_RAWAWB_LIMIT_BY_MIN, id);

	value = !!arg->wp_hist_xytype << 4 |
		!!arg->wp_blk_wei_en1 << 3 |
		!!arg->wp_blk_wei_en0 << 2 |
		!!arg->wp_luma_wei_en1 << 1 |
		!!arg->wp_luma_wei_en0;
	isp3_param_write(params_vdev, value, ISP3X_RAWAWB_WEIGHT_CURVE_CTRL, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->wp_luma_weicurve_y0,
					arg->wp_luma_weicurve_y1,
					arg->wp_luma_weicurve_y2,
					arg->wp_luma_weicurve_y3),
			 ISP3X_RAWAWB_YWEIGHT_CURVE_XCOOR03, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->wp_luma_weicurve_y4,
					arg->wp_luma_weicurve_y5,
					arg->wp_luma_weicurve_y6,
					arg->wp_luma_weicurve_y7),
			 ISP3X_RAWAWB_YWEIGHT_CURVE_XCOOR47, id);

	isp3_param_write(params_vdev,
			 arg->wp_luma_weicurve_y8,
			 ISP3X_RAWAWB_YWEIGHT_CURVE_XCOOR8, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->wp_luma_weicurve_w0,
					arg->wp_luma_weicurve_w1,
					arg->wp_luma_weicurve_w2,
					arg->wp_luma_weicurve_w3),
			 ISP3X_RAWAWB_YWEIGHT_CURVE_YCOOR03, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->wp_luma_weicurve_w4,
					arg->wp_luma_weicurve_w5,
					arg->wp_luma_weicurve_w6,
					arg->wp_luma_weicurve_w7),
			 ISP3X_RAWAWB_YWEIGHT_CURVE_YCOOR47, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->wp_luma_weicurve_w8,
					 arg->pre_wbgain_inv_r),
			 ISP3X_RAWAWB_YWEIGHT_CURVE_YCOOR8, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->pre_wbgain_inv_g,
					 arg->pre_wbgain_inv_b),
			 ISP3X_RAWAWB_PRE_WBGAIN_INV, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex0_u_0, arg->vertex0_v_0),
			 ISP3X_RAWAWB_UV_DETC_VERTEX0_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex1_u_0, arg->vertex1_v_0),
			 ISP3X_RAWAWB_UV_DETC_VERTEX1_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex2_u_0, arg->vertex2_v_0),
			 ISP3X_RAWAWB_UV_DETC_VERTEX2_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex3_u_0, arg->vertex3_v_0),
			 ISP3X_RAWAWB_UV_DETC_VERTEX3_0, id);

	isp3_param_write(params_vdev, arg->islope01_0,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE01_0, id);

	isp3_param_write(params_vdev, arg->islope12_0,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE12_0, id);

	isp3_param_write(params_vdev, arg->islope23_0,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE23_0, id);

	isp3_param_write(params_vdev, arg->islope30_0,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE30_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex0_u_1,
					 arg->vertex0_v_1),
			 ISP3X_RAWAWB_UV_DETC_VERTEX0_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex1_u_1,
					 arg->vertex1_v_1),
			 ISP3X_RAWAWB_UV_DETC_VERTEX1_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex2_u_1,
					 arg->vertex2_v_1),
			 ISP3X_RAWAWB_UV_DETC_VERTEX2_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex3_u_1,
					 arg->vertex3_v_1),
			 ISP3X_RAWAWB_UV_DETC_VERTEX3_1, id);

	isp3_param_write(params_vdev, arg->islope01_1,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE01_1, id);

	isp3_param_write(params_vdev, arg->islope12_1,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE12_1, id);

	isp3_param_write(params_vdev, arg->islope23_1,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE23_1, id);

	isp3_param_write(params_vdev, arg->islope30_1,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE30_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex0_u_2,
					 arg->vertex0_v_2),
			 ISP3X_RAWAWB_UV_DETC_VERTEX0_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex1_u_2,
					 arg->vertex1_v_2),
			 ISP3X_RAWAWB_UV_DETC_VERTEX1_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex2_u_2,
					 arg->vertex2_v_2),
			 ISP3X_RAWAWB_UV_DETC_VERTEX2_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex3_u_2,
					 arg->vertex3_v_2),
			 ISP3X_RAWAWB_UV_DETC_VERTEX3_2, id);

	isp3_param_write(params_vdev, arg->islope01_2,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE01_2, id);

	isp3_param_write(params_vdev, arg->islope12_2,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE12_2, id);

	isp3_param_write(params_vdev, arg->islope23_2,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE23_2, id);

	isp3_param_write(params_vdev, arg->islope30_2,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE30_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex0_u_3,
					 arg->vertex0_v_3),
			 ISP3X_RAWAWB_UV_DETC_VERTEX0_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex1_u_3,
					 arg->vertex1_v_3),
			 ISP3X_RAWAWB_UV_DETC_VERTEX1_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex2_u_3,
					 arg->vertex2_v_3),
			 ISP3X_RAWAWB_UV_DETC_VERTEX2_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->vertex3_u_3,
					 arg->vertex3_v_3),
			 ISP3X_RAWAWB_UV_DETC_VERTEX3_3, id);

	isp3_param_write(params_vdev, arg->islope01_3,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE01_3, id);

	isp3_param_write(params_vdev, arg->islope12_3,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE12_3, id);

	isp3_param_write(params_vdev, arg->islope23_3,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE23_3, id);

	isp3_param_write(params_vdev, arg->islope30_3,
			 ISP3X_RAWAWB_UV_DETC_ISLOPE30_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->rgb2ryuvmat0_y,
					 arg->rgb2ryuvmat1_y),
			 ISP3X_RAWAWB_YUV_RGB2ROTY_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->rgb2ryuvmat2_y,
					 arg->rgb2ryuvofs_y),
			 ISP3X_RAWAWB_YUV_RGB2ROTY_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->rgb2ryuvmat0_u,
					 arg->rgb2ryuvmat1_u),
			 ISP3X_RAWAWB_YUV_RGB2ROTU_0, id);


	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->rgb2ryuvmat2_u,
					 arg->rgb2ryuvofs_u),
			 ISP3X_RAWAWB_YUV_RGB2ROTU_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->rgb2ryuvmat0_v,
					 arg->rgb2ryuvmat1_v),
			 ISP3X_RAWAWB_YUV_RGB2ROTV_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->rgb2ryuvmat2_v,
					 arg->rgb2ryuvofs_v),
			 ISP3X_RAWAWB_YUV_RGB2ROTV_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls0_y,
					 arg->vec_x21_ls0_y),
			 ISP3X_RAWAWB_YUV_X_COOR_Y_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls0_u,
					 arg->vec_x21_ls0_u),
			 ISP3X_RAWAWB_YUV_X_COOR_U_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls0_v,
					 arg->vec_x21_ls0_v),
			 ISP3X_RAWAWB_YUV_X_COOR_V_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->dis_x1x2_ls0, 0,
					arg->rotu0_ls0, arg->rotu1_ls0),
			 ISP3X_RAWAWB_YUV_X1X2_DIS_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->rotu2_ls0, arg->rotu3_ls0,
					arg->rotu4_ls0, arg->rotu5_ls0),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_UCOOR_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th0_ls0, arg->th1_ls0),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH0_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th2_ls0, arg->th3_ls0),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH1_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th4_ls0, arg->th5_ls0),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH2_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls1_y, arg->vec_x21_ls1_y),
			 ISP3X_RAWAWB_YUV_X_COOR_Y_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls1_u, arg->vec_x21_ls1_u),
			 ISP3X_RAWAWB_YUV_X_COOR_U_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls1_v, arg->vec_x21_ls1_v),
			 ISP3X_RAWAWB_YUV_X_COOR_V_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->dis_x1x2_ls1, 0, arg->rotu0_ls1, arg->rotu1_ls1),
			 ISP3X_RAWAWB_YUV_X1X2_DIS_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->rotu2_ls1, arg->rotu3_ls1,
					arg->rotu4_ls1, arg->rotu5_ls1),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_UCOOR_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th0_ls1, arg->th1_ls1),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH0_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th2_ls1, arg->th3_ls1),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH1_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th4_ls1, arg->th5_ls1),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH2_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls2_y, arg->vec_x21_ls2_y),
			 ISP3X_RAWAWB_YUV_X_COOR_Y_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls2_u, arg->vec_x21_ls2_u),
			 ISP3X_RAWAWB_YUV_X_COOR_U_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls2_v, arg->vec_x21_ls2_v),
			 ISP3X_RAWAWB_YUV_X_COOR_V_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->dis_x1x2_ls2, 0, arg->rotu0_ls2, arg->rotu1_ls2),
			 ISP3X_RAWAWB_YUV_X1X2_DIS_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->rotu2_ls2, arg->rotu3_ls2,
					arg->rotu4_ls2, arg->rotu5_ls2),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_UCOOR_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th0_ls2, arg->th1_ls2),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH0_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th2_ls2, arg->th3_ls2),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH1_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th4_ls2, arg->th5_ls2),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH2_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls3_y, arg->vec_x21_ls3_y),
			 ISP3X_RAWAWB_YUV_X_COOR_Y_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls3_u, arg->vec_x21_ls3_u),
			 ISP3X_RAWAWB_YUV_X_COOR_U_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->coor_x1_ls3_v, arg->vec_x21_ls3_v),
			 ISP3X_RAWAWB_YUV_X_COOR_V_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->dis_x1x2_ls3, 0,
					arg->rotu0_ls3, arg->rotu1_ls3),
			 ISP3X_RAWAWB_YUV_X1X2_DIS_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->rotu2_ls3, arg->rotu3_ls3,
					arg->rotu4_ls3, arg->rotu5_ls3),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_UCOOR_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th0_ls3, arg->th1_ls3),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH0_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th2_ls3, arg->th3_ls3),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH1_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->th4_ls3, arg->th5_ls3),
			 ISP3X_RAWAWB_YUV_INTERP_CURVE_TH2_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->wt0, arg->wt1),
			 ISP3X_RAWAWB_RGB2XY_WT01, id);

	isp3_param_write(params_vdev, arg->wt2,
			 ISP3X_RAWAWB_RGB2XY_WT2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->mat0_x, arg->mat0_y),
			 ISP3X_RAWAWB_RGB2XY_MAT0_XY, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->mat1_x, arg->mat1_y),
			 ISP3X_RAWAWB_RGB2XY_MAT1_XY, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->mat2_x, arg->mat2_y),
			 ISP3X_RAWAWB_RGB2XY_MAT2_XY, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->nor_x0_0, arg->nor_x1_0),
			 ISP3X_RAWAWB_XY_DETC_NOR_X_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->nor_y0_0, arg->nor_y1_0),
			 ISP3X_RAWAWB_XY_DETC_NOR_Y_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->big_x0_0, arg->big_x1_0),
			 ISP3X_RAWAWB_XY_DETC_BIG_X_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->big_y0_0, arg->big_y1_0),
			 ISP3X_RAWAWB_XY_DETC_BIG_Y_0, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->nor_x0_1, arg->nor_x1_1),
			 ISP3X_RAWAWB_XY_DETC_NOR_X_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->nor_y0_1, arg->nor_y1_1),
			 ISP3X_RAWAWB_XY_DETC_NOR_Y_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->big_x0_1, arg->big_x1_1),
			 ISP3X_RAWAWB_XY_DETC_BIG_X_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->big_y0_1, arg->big_y1_1),
			 ISP3X_RAWAWB_XY_DETC_BIG_Y_1, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->nor_x0_2, arg->nor_x1_2),
			 ISP3X_RAWAWB_XY_DETC_NOR_X_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->nor_y0_2, arg->nor_y1_2),
			 ISP3X_RAWAWB_XY_DETC_NOR_Y_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->big_x0_2, arg->big_x1_2),
			 ISP3X_RAWAWB_XY_DETC_BIG_X_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->big_y0_2, arg->big_y1_2),
			 ISP3X_RAWAWB_XY_DETC_BIG_Y_2, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->nor_x0_3, arg->nor_x1_3),
			 ISP3X_RAWAWB_XY_DETC_NOR_X_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->nor_y0_3, arg->nor_y1_3),
			 ISP3X_RAWAWB_XY_DETC_NOR_Y_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->big_x0_3, arg->big_x1_3),
			 ISP3X_RAWAWB_XY_DETC_BIG_X_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->big_y0_3, arg->big_y1_3),
			 ISP3X_RAWAWB_XY_DETC_BIG_Y_3, id);

	value = (arg->exc_wp_region0_excen & 0x3) |
		!!arg->exc_wp_region0_measen << 2 |
		!!arg->exc_wp_region0_domain << 3 |
		(arg->exc_wp_region1_excen & 0x3) << 4 |
		!!arg->exc_wp_region1_measen << 6 |
		!!arg->exc_wp_region1_domain << 7 |
		(arg->exc_wp_region2_excen & 0x3) << 8 |
		!!arg->exc_wp_region2_measen << 10 |
		!!arg->exc_wp_region2_domain << 11 |
		(arg->exc_wp_region3_excen & 0x3) << 12 |
		!!arg->exc_wp_region3_measen << 14 |
		!!arg->exc_wp_region3_domain << 15 |
		(arg->exc_wp_region4_excen & 0x3) << 16 |
		!!arg->exc_wp_region4_domain << 19 |
		(arg->exc_wp_region5_excen & 0x3) << 20 |
		!!arg->exc_wp_region5_domain << 23 |
		(arg->exc_wp_region6_excen & 0x3) << 24 |
		!!arg->exc_wp_region6_domain << 27 |
		!!arg->multiwindow_en << 31;
	isp3_param_write(params_vdev, value, ISP3X_RAWAWB_MULTIWINDOW_EXC_CTRL, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->multiwindow0_h_offs,
					 arg->multiwindow0_v_offs),
			 ISP3X_RAWAWB_MULTIWINDOW0_OFFS, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->multiwindow0_h_size,
					 arg->multiwindow0_v_size),
			 ISP3X_RAWAWB_MULTIWINDOW0_SIZE, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->multiwindow1_h_offs,
					 arg->multiwindow1_v_offs),
			 ISP3X_RAWAWB_MULTIWINDOW1_OFFS, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->multiwindow1_h_size,
					 arg->multiwindow1_v_size),
			 ISP3X_RAWAWB_MULTIWINDOW1_SIZE, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->multiwindow2_h_offs,
					 arg->multiwindow2_v_offs),
			 ISP3X_RAWAWB_MULTIWINDOW2_OFFS, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->multiwindow2_h_size,
					 arg->multiwindow2_v_size),
			 ISP3X_RAWAWB_MULTIWINDOW2_SIZE, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->multiwindow3_h_offs,
					 arg->multiwindow3_v_offs),
			 ISP3X_RAWAWB_MULTIWINDOW3_OFFS, id);
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->multiwindow3_h_size,
					 arg->multiwindow3_v_size),
			 ISP3X_RAWAWB_MULTIWINDOW3_SIZE, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region0_xu0,
					 arg->exc_wp_region0_xu1),
			 ISP3X_RAWAWB_EXC_WP_REGION0_XU, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region0_yv0,
					 arg->exc_wp_region0_yv1),
			 ISP3X_RAWAWB_EXC_WP_REGION0_YV, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region1_xu0,
					 arg->exc_wp_region1_xu1),
			 ISP3X_RAWAWB_EXC_WP_REGION1_XU, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region1_yv0,
					 arg->exc_wp_region1_yv1),
			 ISP3X_RAWAWB_EXC_WP_REGION1_YV, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region2_xu0,
					 arg->exc_wp_region2_xu1),
			 ISP3X_RAWAWB_EXC_WP_REGION2_XU, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region2_yv0,
					 arg->exc_wp_region2_yv1),
			 ISP3X_RAWAWB_EXC_WP_REGION2_YV, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region3_xu0,
					 arg->exc_wp_region3_xu1),
			 ISP3X_RAWAWB_EXC_WP_REGION3_XU, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region3_yv0,
					 arg->exc_wp_region3_yv1),
			 ISP3X_RAWAWB_EXC_WP_REGION3_YV, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region4_xu0,
					 arg->exc_wp_region4_xu1),
			 ISP3X_RAWAWB_EXC_WP_REGION4_XU, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region4_yv0,
					 arg->exc_wp_region4_yv1),
			 ISP3X_RAWAWB_EXC_WP_REGION4_YV, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region5_xu0,
					 arg->exc_wp_region5_xu1),
			 ISP3X_RAWAWB_EXC_WP_REGION5_XU, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region5_yv0,
					 arg->exc_wp_region5_yv1),
			 ISP3X_RAWAWB_EXC_WP_REGION5_YV, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region6_xu0,
					 arg->exc_wp_region6_xu1),
			 ISP3X_RAWAWB_EXC_WP_REGION6_XU, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(arg->exc_wp_region6_yv0,
					 arg->exc_wp_region6_yv1),
			 ISP3X_RAWAWB_EXC_WP_REGION6_YV, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->exc_wp_region0_weight,
					arg->exc_wp_region1_weight,
					arg->exc_wp_region2_weight,
					arg->exc_wp_region3_weight),
			 ISP32_RAWAWB_EXC_WP_WEIGHT0_3, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->exc_wp_region4_weight,
					arg->exc_wp_region5_weight,
					arg->exc_wp_region6_weight, 0),
			 ISP32_RAWAWB_EXC_WP_WEIGHT4_6, id);

	if (params_vdev->dev->hw_dev->is_single)
		isp_rawawb_cfg_sram(params_vdev, arg, false, id);
	memcpy(arg_rec->wp_blk_wei_w, arg->wp_blk_wei_w, ISP39_RAWAWB_WEIGHT_NUM);

	/* avoid to override the old enable value */
	value = isp3_param_read_cache(params_vdev, ISP3X_RAWAWB_CTRL, id);
	value &= (ISP39_MODULE_EN |
		  ISP32_RAWAWB_2DDR_PATH_EN |
		  ISP32_RAWAWB_2DDR_PATH_DS);
	value |= !!arg->low12bit_val << 28 |
		 !!arg->yuv3d_en1 << 26 |
		 !!arg->xy_en1 << 25 |
		 !!arg->uv_en1 << 24 |
		 (arg->light_num & 0x7) << 20 |
		 !!arg->rawlsc_bypass_en << 19 |
		 !!arg->wind_size << 18 |
		 !!arg->in_overexposure_check_en << 17 |
		 !!arg->in_rshift_to_12bit_en << 16 |
		 (arg->yuv3d_ls_idx3 & 0x7) << 13 |
		 (arg->yuv3d_ls_idx2 & 0x7) << 10 |
		 (arg->yuv3d_ls_idx1 & 0x7) << 7 |
		 (arg->yuv3d_ls_idx0 & 0x7) << 4 |
		 !!arg->yuv3d_en0 << 3 |
		 !!arg->xy_en0 << 2 |
		 !!arg->uv_en0 << 1;
	isp3_param_write(params_vdev, value, ISP3X_RAWAWB_CTRL, id);

	mask = ISP32_DRC2AWB_SEL | ISP32_BNR2AWB_SEL | ISP3X_RAWAWB_SEL(3);
	val = ISP3X_RAWAWB_SEL(arg->rawawb_sel) |
	      (arg->bnr2awb_sel & 0x1) << 26 | (arg->drc2awb_sel & 0x1) << 27;
	value = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, id);
	if ((value & mask) != val) {
		value &= ~mask;
		value |= val;
		isp3_param_write(params_vdev, value, ISP3X_VI_ISP_PATH, id);
	}
}

static void
isp_rawawb_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 awb_ctrl;

	awb_ctrl = isp3_param_read_cache(params_vdev, ISP3X_RAWAWB_CTRL, id);
	awb_ctrl &= ~ISP39_REG_WR_MASK;
	if (en)
		awb_ctrl |= ISP39_MODULE_EN;
	else
		awb_ctrl &= ~ISP39_MODULE_EN;

	isp3_param_write(params_vdev, awb_ctrl, ISP3X_RAWAWB_CTRL, id);
}

static void
isp_rawhstbig_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
		       const struct isp2x_rawhistbig_cfg *arg,
		       u32 addr, bool is_check, u32 id)
{
	u32 i, j, wnd_num_idx, value;
	u8 weight15x15[ISP39_RAWHISTBIG_WEIGHT_REG_SIZE];
	const u32 hist_wnd_num[] = {5, 5, 15, 15};

	value = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, id);
	if (is_check && !(value & ISP3X_RAWHIST_EN))
		return;

	wnd_num_idx = arg->wnd_num;
	if (wnd_num_idx >= ARRAY_SIZE(hist_wnd_num)) {
		wnd_num_idx = ARRAY_SIZE(hist_wnd_num) - 1;
		dev_err(params_vdev->dev->dev,
			"%s invalid wnd_num:%d, set to %d\n",
			__func__, arg->wnd_num, wnd_num_idx);
	}
	memset(weight15x15, 0, sizeof(weight15x15));
	for (i = 0; i < hist_wnd_num[wnd_num_idx]; i++) {
		for (j = 0; j < hist_wnd_num[wnd_num_idx]; j++) {
			weight15x15[i * ISP39_RAWHISTBIG_ROW_NUM + j] =
				arg->weight[i * hist_wnd_num[wnd_num_idx] + j];
		}
	}

	for (i = 0; i < (ISP39_RAWHISTBIG_WEIGHT_REG_SIZE / 5); i++) {
		value = (weight15x15[5 * i + 0] & 0x3f) |
			(weight15x15[5 * i + 1] & 0x3f) << 6 |
			(weight15x15[5 * i + 2] & 0x3f) << 12 |
			(weight15x15[5 * i + 3] & 0x3f) << 18 |
			(weight15x15[5 * i + 4] & 0x3f) << 24;
		isp3_param_write_direct(params_vdev, value,
					addr + ISP3X_RAWHIST_BIG_WEIGHT_BASE);
	}
}

static void
isp_rawhstbig_config(struct rkisp_isp_params_vdev *params_vdev,
		     const struct isp2x_rawhistbig_cfg *arg, u32 addr, u32 id)
{
	struct isp39_isp_params_cfg *params_rec = params_vdev->isp39_params + id;
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	u32 width = out_crop->width, height = out_crop->height;
	struct isp2x_rawhistbig_cfg *arg_rec;
	u32 hist_ctrl, block_hsize, block_vsize, wnd_num_idx;
	u32 h_size, v_size, h_offs, v_offs;
	const u32 hist_wnd_num[] = {5, 5, 15, 15};

	wnd_num_idx = arg->wnd_num;
	if (wnd_num_idx >= ARRAY_SIZE(hist_wnd_num)) {
		wnd_num_idx = ARRAY_SIZE(hist_wnd_num) - 1;
		dev_err(params_vdev->dev->dev,
			"%s invalid wnd_num:%d, set to %d\n",
			__func__, arg->wnd_num, wnd_num_idx);
	}
	/* avoid to override the old enable value */
	hist_ctrl = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, id);
	hist_ctrl &= ISP3X_RAWHIST_EN;
	hist_ctrl = hist_ctrl |
		    ISP3X_RAWHIST_MODE(arg->mode) |
		    ISP3X_RAWHIST_WND_NUM(arg->wnd_num) |
		    ISP3X_RAWHIST_STEPSIZE(arg->stepsize) |
		    ISP3X_RAWHIST_DATASEL(arg->data_sel) |
		    ISP3X_RAWHIST_WATERLINE(arg->waterline);
	isp3_param_write(params_vdev, hist_ctrl, addr + ISP3X_RAWHIST_BIG_CTRL, id);

	h_offs = arg->win.h_offs & ~0x1;
	v_offs = arg->win.v_offs & ~0x1;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(h_offs, v_offs),
			 addr + ISP3X_RAWHIST_BIG_OFFS, id);

	if (dev->unite_div > ISP_UNITE_DIV1)
		width = width / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;
	if (dev->unite_div == ISP_UNITE_DIV4)
		height = height / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;

	h_size = arg->win.h_size;
	v_size = arg->win.v_size;
	if (!h_size || h_size + h_offs + 1 > width)
		h_size = width - h_offs - 1;
	if (!v_size || v_size + v_offs + 1 > height)
		v_size = height - v_offs - 1;
	block_hsize = (h_size / hist_wnd_num[wnd_num_idx]) & ~0x1;
	block_vsize = (v_size / hist_wnd_num[wnd_num_idx]) & ~0x1;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(block_hsize, block_vsize),
			 addr + ISP3X_RAWHIST_BIG_SIZE, id);

	isp3_param_write(params_vdev,
			 ISP_PACK_4BYTE(arg->rcc, arg->gcc, arg->bcc, arg->off),
			 addr + ISP3X_RAWHIST_BIG_RAW2Y_CC, id);

	if (dev->hw_dev->is_single) {
		isp_rawhstbig_cfg_sram(params_vdev, arg, addr, false, id);
	}
	arg_rec = (addr == ISP3X_RAWHIST_LITE_BASE) ?
		  &params_rec->meas.rawhist0 : &params_rec->meas.rawhist3;
	*arg_rec = *arg;
}

static void
isp_rawhstbig_enable(struct rkisp_isp_params_vdev *params_vdev,
		     bool en, u32 addr, u32 id)
{
	u32 hist_ctrl;

	hist_ctrl = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, id);
	hist_ctrl &= ~(ISP3X_RAWHIST_EN | ISP39_REG_WR_MASK);
	if (en)
		hist_ctrl |= ISP3X_RAWHIST_EN;
	isp3_param_write(params_vdev, hist_ctrl, addr + ISP3X_RAWHIST_BIG_CTRL, id);
}

static void
isp_rawhst0_config(struct rkisp_isp_params_vdev *params_vdev,
		   const struct isp2x_rawhistbig_cfg *arg, u32 id)
{
	isp_rawhstbig_config(params_vdev, arg, ISP3X_RAWHIST_LITE_BASE, id);
}

static void
isp_rawhst0_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	isp_rawhstbig_enable(params_vdev, en, ISP3X_RAWHIST_LITE_BASE, id);
}

static void
isp_rawhst3_config(struct rkisp_isp_params_vdev *params_vdev,
		   const struct isp2x_rawhistbig_cfg *arg, u32 id)
{
	isp_rawhstbig_config(params_vdev, arg, ISP3X_RAWHIST_BIG1_BASE, id);
}

static void
isp_rawhst3_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	isp_rawhstbig_enable(params_vdev, en, ISP3X_RAWHIST_BIG1_BASE, id);
}

static void
isp_hdrmge_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp32_hdrmge_cfg *arg,
		  enum rkisp_params_type type, u32 id)
{
	u32 value;
	int i;

	if (type == RKISP_PARAMS_SHD || type == RKISP_PARAMS_ALL) {
		value = ISP_PACK_2SHORT(arg->gain0, arg->gain0_inv);
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_GAIN0, id);

		value = ISP_PACK_2SHORT(arg->gain1, arg->gain1_inv);
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_GAIN1, id);

		value = arg->gain2;
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_GAIN2, id);

		value = isp3_param_read_cache(params_vdev, ISP3X_HDRMGE_CTRL, id);
		if (arg->s_base)
			value |= BIT(1);
		else
			value &= ~BIT(1);
		if (arg->each_raw_en)
			value |= BIT(6);
		else
			value &= ~BIT(6);
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_CTRL, id);
	}

	if (type == RKISP_PARAMS_IMD || type == RKISP_PARAMS_ALL) {
		value = ISP_PACK_4BYTE(arg->ms_dif_0p8, arg->ms_diff_0p15,
				       arg->lm_dif_0p9, arg->lm_dif_0p15);
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_LIGHTZ, id);
		value = (arg->ms_scl & 0x7ff) |
			(arg->ms_thd0 & 0x3ff) << 12 |
			(arg->ms_thd1 & 0x3ff) << 22;
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_MS_DIFF, id);
		value = (arg->lm_scl & 0x7ff) |
			(arg->lm_thd0 & 0x3ff) << 12 |
			(arg->lm_thd1 & 0x3ff) << 22;
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_LM_DIFF, id);

		for (i = 0; i < ISP39_HDRMGE_L_CURVE_NUM; i++) {
			value = ISP_PACK_2SHORT(arg->curve.curve_0[i], arg->curve.curve_1[i]);
			isp3_param_write(params_vdev, value, ISP3X_HDRMGE_DIFF_Y0 + 4 * i, id);
		}

		for (i = 0; i < ISP39_HDRMGE_E_CURVE_NUM; i++) {
			value = (arg->l_raw1[i] & 0x3ff) << 20 |
				(arg->l_raw0[i] & 0x3ff) << 10 |
				(arg->e_y[i] & 0x3ff);
			isp3_param_write(params_vdev, value, ISP3X_HDRMGE_OVER_Y0 + 4 * i, id);
		}

		value = ISP_PACK_2SHORT(arg->each_raw_gain0, arg->each_raw_gain1);
		isp3_param_write(params_vdev, value, ISP32_HDRMGE_EACH_GAIN, id);
	}
}

static void
isp_hdrmge_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
}

static void
isp_hdrdrc_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp39_drc_cfg *arg,
		  enum rkisp_params_type type, u32 id)
{
	u32 i, value, ctrl;

	ctrl = isp3_param_read(params_vdev, ISP3X_DRC_CTRL0, id);
	ctrl &= ISP39_MODULE_EN;
	ctrl |= !!arg->gainx32_en << 3 |
		!!arg->cmps_byp_en << 2 | !!arg->bypass_en << 1;
	isp3_param_write(params_vdev, ctrl, ISP3X_DRC_CTRL0, id);
	if (ctrl & BIT(29))
		dev_warn(params_vdev->dev->dev, "drc raw_dly_dis=1\n");
	value = isp3_param_read_cache(params_vdev, ISP3X_HDRMGE_CTRL, id);
	if (ctrl & BIT(2) && (value & ISP39_MODULE_EN))
		dev_warn(params_vdev->dev->dev, "drc cmps_byp_en=1 but hdr\n");

	if (type == RKISP_PARAMS_IMD)
		return;

	value = (arg->offset_pow2 & 0x0F) << 28 |
		(arg->compres_scl & 0x1FFF) << 14 |
		(arg->position & 0x03FFF);
	isp3_param_write(params_vdev, value, ISP3X_DRC_CTRL1, id);

	value = arg->delta_scalein << 24 |
		(arg->hpdetail_ratio & 0xFFF) << 12 |
		(arg->lpdetail_ratio & 0xFFF);
	isp3_param_write(params_vdev, value, ISP3X_DRC_LPRATIO, id);

	value = arg->weicur_pix << 24 | !!arg->thumb_thd_enable << 23 |
		(arg->thumb_thd_neg & 0x1ff) << 8 | arg->bilat_wt_off;
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT0, id);

	value = (arg->drc_gas_t & 0x3ff) << 16 | !!arg->cmps_fixbit_mode << 4 |
		(arg->cmps_offset_bits_int & 0xf);
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT1, id);

	value = arg->thumb_scale << 16 | (arg->thumb_clip & 0xfff);
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT2, id);

	value = (arg->range_sgm_inv1 & 0x3ff) << 16 | (arg->range_sgm_inv0 & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT3, id);

	value = !!arg->enable_soft_thd << 31 | (arg->bilat_soft_thd & 0x7ff) << 16 |
		arg->weight_8x8thumb << 8 | (arg->weig_bilat & 0x1f);
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT4, id);

	for (i = 0; i < ISP39_DRC_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->gain_y[2 * i],
					arg->gain_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_DRC_GAIN_Y0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->gain_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP3X_DRC_GAIN_Y0 + 4 * i, id);

	for (i = 0; i < ISP39_DRC_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->compres_y[2 * i],
					arg->compres_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_DRC_COMPRES_Y0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->compres_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP3X_DRC_COMPRES_Y0 + 4 * i, id);

	for (i = 0; i < ISP39_DRC_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->scale_y[2 * i],
					arg->scale_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_DRC_SCALE_Y0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->scale_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP3X_DRC_SCALE_Y0 + 4 * i, id);

	value = ISP_PACK_2SHORT(arg->min_ogain, 0);
	isp3_param_write(params_vdev, value, ISP3X_DRC_IIRWG_GAIN, id);

	for (i = 0; i < ISP39_DRC_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->sfthd_y[2 * i], arg->sfthd_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP39_DRC_SFTHD_Y0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->sfthd_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP39_DRC_SFTHD_Y0 + 4 * i, id);
}

static void
isp_hdrdrc_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 value;
	bool real_en;

	value = isp3_param_read(params_vdev, ISP3X_DRC_CTRL0, id);
	real_en = !!(value & ISP39_MODULE_EN);
	if ((en && real_en) || (!en && !real_en))
		return;

	if (en) {
		value |= ISP39_MODULE_EN;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP3X_ADRC_FST_FRAME, id);
	} else {
		value &= ~(ISP39_MODULE_EN | ISP39_SELF_FORCE_UPD);
	}
	isp3_param_write(params_vdev, value, ISP3X_DRC_CTRL0, id);
}

static void
isp_gic_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp39_gic_cfg *arg, u32 id)
{
	u32 value;
	s32 i;

	value = isp3_param_read(params_vdev, ISP3X_GIC_CONTROL, id);
	value &= ISP39_MODULE_EN;
	value |= arg->bypass_en << 1;
	isp3_param_write(params_vdev, value, ISP3X_GIC_CONTROL, id);

	value = (arg->regmingradthrdark2 & 0x03FF) << 20 |
		(arg->regmingradthrdark1 & 0x03FF) << 10 |
		(arg->regminbusythre & 0x03FF);
	isp3_param_write(params_vdev, value, ISP3X_GIC_DIFF_PARA1, id);

	value = (arg->regdarkthre & 0x07FF) << 21 |
		(arg->regmaxcorvboth & 0x03FF) << 11 |
		(arg->regdarktthrehi & 0x07FF);
	isp3_param_write(params_vdev, value, ISP3X_GIC_DIFF_PARA2, id);

	value = (arg->regkgrad2dark & 0x0F) << 28 |
		(arg->regkgrad1dark & 0x0F) << 24 |
		(arg->regstrengthglobal_fix & 0xFF) << 16 |
		(arg->regdarkthrestep & 0x0F) << 12 |
		(arg->regkgrad2 & 0x0F) << 8 |
		(arg->regkgrad1 & 0x0F) << 4 |
		(arg->reggbthre & 0x0F);
	isp3_param_write(params_vdev, value, ISP3X_GIC_DIFF_PARA3, id);

	value = (arg->regmaxcorv & 0x03FF) << 20 |
		(arg->regmingradthr2 & 0x03FF) << 10 |
		(arg->regmingradthr1 & 0x03FF);
	isp3_param_write(params_vdev, value, ISP3X_GIC_DIFF_PARA4, id);

	value = (arg->gr_ratio & 0x03) << 28 |
		(arg->noise_scale & 0x7F) << 12 |
		(arg->noise_base & 0xFFF);
	isp3_param_write(params_vdev, value, ISP3X_GIC_NOISE_PARA1, id);

	value = arg->diff_clip & 0x7fff;
	isp3_param_write(params_vdev, value, ISP3X_GIC_NOISE_PARA2, id);

	for (i = 0; i < ISP39_GIC_SIGMA_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->sigma_y[2 * i], arg->sigma_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_GIC_SIGMA_VALUE0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->sigma_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP3X_GIC_SIGMA_VALUE0 + 4 * i, id);
}

static void
isp_gic_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 value = 0;

	value = isp3_param_read(params_vdev, ISP3X_GIC_CONTROL, id);
	if (en)
		value |= ISP39_MODULE_EN;
	else
		value &= ~ISP39_MODULE_EN;
	isp3_param_write(params_vdev, value, ISP3X_GIC_CONTROL, id);
}

static void
isp_dhaz_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp39_dhaz_cfg *arg, bool is_check, u32 id)
{
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 i, j, val, ctrl = isp3_param_read(params_vdev, ISP3X_DHAZ_CTRL, id);

	if (is_check && !(ctrl & ISP3X_DHAZ_ENMUX))
		return;

	if (arg->hist_iir_wr) {
		for (i = 0; i < priv_val->dhaz_blk_num; i++) {
			val = ISP39_DHAZ_IIR_WR_ID(i) | ISP39_DHAZ_IIR_WR_CLEAR;
			isp3_param_write_direct(params_vdev, val, ISP39_DHAZ_HIST_RW);
			for (j = 0; j < ISP39_DHAZ_HIST_IIR_NUM / 2; j++) {
				val = ISP_PACK_2SHORT(arg->hist_iir[i][2 * j], arg->hist_iir[i][2 * j + 1]);
				isp3_param_write_direct(params_vdev, val, ISP39_DHAZ_HIST_IIR0 + 4 * j);
			}
		}
	}
}

static void
isp_dhaz_config(struct rkisp_isp_params_vdev *params_vdev,
		const struct isp39_dhaz_cfg *arg, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	struct isp39_isp_params_cfg *params_rec = params_vdev->isp39_params + id;
	struct isp39_dhaz_cfg *arg_rec = &params_rec->others.dhaz_cfg;
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 w = out_crop->width, h = out_crop->height;
	u32 i, value, ctrl, thumb_row, thumb_col, blk_het, blk_wid;

	if (dev->unite_div > ISP_UNITE_DIV1)
		w = w / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;
	if (dev->unite_div == ISP_UNITE_DIV4)
		h = h / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL;

	ctrl = isp3_param_read(params_vdev, ISP3X_DHAZ_CTRL, id);
	ctrl &= ISP3X_DHAZ_ENMUX;

	ctrl |= !!arg->dc_en << 4 | !!arg->hist_en << 8 |
		!!arg->map_mode << 9 | !!arg->mem_mode << 10 |
		!!arg->mem_force << 11 | !!arg->air_lc_en << 16 |
		!!arg->enhance_en << 20 | !!arg->soft_wr_en << 25 |
		!!arg->round_en << 26 | !!arg->color_deviate_en << 27 |
		!!arg->enh_luma_en << 28;
	isp3_param_write(params_vdev, ctrl, ISP3X_DHAZ_CTRL, id);

	value = ISP_PACK_4BYTE(arg->dc_min_th, arg->dc_max_th,
			       arg->yhist_th, arg->yblk_th);
	isp3_param_write(params_vdev, value, ISP3X_DHAZ_ADP0, id);

	value = (arg->wt_max & 0x1ff) << 16 | arg->bright_max << 8 |
		arg->bright_min;
	isp3_param_write(params_vdev, value, ISP3X_DHAZ_ADP1, id);

	value = ISP_PACK_4BYTE(arg->air_min, arg->air_max,
			       arg->dark_th, arg->tmax_base);
	isp3_param_write(params_vdev, value, ISP3X_DHAZ_ADP2, id);

	value = ISP_PACK_2SHORT(arg->tmax_off, arg->tmax_max);
	isp3_param_write(params_vdev, value, ISP3X_DHAZ_ADP_TMAX, id);

	value = ISP_PACK_2SHORT(arg->enhance_chroma, arg->enhance_value);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_ENHANCE, id);

	value = (arg->iir_wt_sigma & 0x07FF) << 16 | arg->iir_sigma << 8 |
		(arg->stab_fnum & 0x1F);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_IIR0, id);

	value = (arg->iir_pre_wet & 0x0F) << 24 |
		(arg->iir_tmax_sigma & 0x7FF) << 8 |
		arg->iir_air_sigma;
	isp3_param_write(params_vdev, value, ISP39_DHAZ_IIR1, id);

	value = (arg->cfg_wt & 0x01FF) << 16 | arg->cfg_air << 8 |
		arg->cfg_alpha;
	isp3_param_write(params_vdev, value, ISP39_DHAZ_SOFT_CFG0, id);

	value = arg->cfg_tmax & 0x3ff;
	isp3_param_write(params_vdev, value, ISP39_DHAZ_SOFT_CFG1, id);

	value = (arg->range_sima & 0x01FF) << 16 | arg->space_sigma_pre << 8 |
		arg->space_sigma_cur;
	isp3_param_write(params_vdev, value, ISP39_DHAZ_BF_SIGMA, id);

	value = ISP_PACK_2SHORT(arg->bf_weight, arg->dc_weitcur);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_BF_WET, id);

	for (i = 0; i < ISP39_DHAZ_ENH_CURVE_NUM / 3; i++) {
		value = (arg->enh_curve[3 * i + 2] & 0x3ff) << 20 |
			(arg->enh_curve[3 * i + 1] & 0x3ff) << 10 |
			(arg->enh_curve[3 * i] & 0x3ff);
		isp3_param_write(params_vdev, value, ISP39_DHAZ_ENH_CURVE0 + 4 * i, id);
	}
	value = (arg->enh_curve[3 * i + 1] & 0x3ff) << 10 | (arg->enh_curve[3 * i] & 0x3ff);
	isp3_param_write(params_vdev, value, ISP3X_DHAZ_ENH_CURVE0 + 4 * i, id);

	value = (arg->gaus_h2 & 0x3f) << 16 | (arg->gaus_h1 & 0x3f) << 8 | (arg->gaus_h0 & 0x3f);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_GAUS, id);

	for (i = 0; i < ISP39_DHAZ_ENH_LUMA_NUM / 3; i++) {
		value = (arg->enh_luma[3 * i + 2] & 0x3ff) << 20 |
			(arg->enh_luma[3 * i + 1] & 0x3ff) << 10 |
			(arg->enh_luma[3 * i] & 0x3ff);
		isp3_param_write(params_vdev, value, ISP39_DHAZ_ENH_LUMA0 + i * 4, id);
	}
	value = (arg->enh_luma[3 * i + 1] & 0x3ff) << 10 | (arg->enh_luma[3 * i] & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_ENH_LUMA0 + 4 * i, id);

	value = arg->adp_air_wr << 16 | (arg->adp_wt_wr & 0x1ff);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_ADP_WR0, id);
	value = arg->adp_tmax_wr & 0x1fff;
	isp3_param_write(params_vdev, value, ISP39_DHAZ_ADP_WR1, id);

	for (i = 0; i < ISP39_DHAZ_SIGMA_IDX_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->sigma_idx[i * 4], arg->sigma_idx[i * 4 + 1],
				       arg->sigma_idx[i * 4 + 2], arg->sigma_idx[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP39_DHAZ_GAIN_IDX0 + i * 4, id);
	}
	value = ISP_PACK_4BYTE(arg->sigma_idx[i * 4], arg->sigma_idx[i * 4 + 1],
			       arg->sigma_idx[i * 4 + 2], 0);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_GAIN_IDX0 + i * 4, id);

	for (i = 0; i < ISP39_DHAZ_SIGMA_LUT_NUM / 3; i++) {
		value = (arg->sigma_lut[3 * i + 2] & 0x3ff) << 20 |
			(arg->sigma_lut[3 * i + 1] & 0x3ff) << 10 |
			(arg->sigma_lut[3 * i] & 0x3ff);
		isp3_param_write(params_vdev, value, ISP39_DHAZ_GAIN_LUT0 + i * 4, id);
	}
	value = (arg->sigma_lut[3 * i + 1] & 0x3ff) << 10 | (arg->sigma_lut[3 * i] & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_GAIN_LUT0 + i * 4, id);

	value = arg->gain_fuse_alpha & 0x1ff;
	isp3_param_write(params_vdev, value, ISP39_DHAZ_GAIN_FUSE, id);

	value = (arg->hist_min & 0x1ff) << 16 | arg->hist_th_off << 8 | (arg->hist_k & 0x1f);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_ADP_HF, id);

	value = (arg->cfg_k & 0x1ff) << 16 | (arg->cfg_k_alpha & 0x1ff);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_HIST_CFG, id);

	value = arg->k_gain & 0x7ff;
	isp3_param_write(params_vdev, value, ISP39_DHAZ_HIST_GAIN, id);

	for (i = 0; i < ISP39_DHAZ_BLEND_WET_NUM / 3; i++) {
		value = (arg->blend_wet[3 * i + 2] & 0x1ff) << 18 |
			(arg->blend_wet[3 * i + 1] & 0x1ff) << 9 |
			(arg->blend_wet[3 * i] & 0x1ff);
		isp3_param_write(params_vdev, value, ISP39_DHAZ_BLEND_WET0 + 4 * i, id);
	}
	value = (arg->blend_wet[3 * i + 1] & 0x1ff) << 9 | (arg->blend_wet[3 * i] & 0x1ff);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_BLEND_WET0 + 4 * i, id);

	thumb_row = arg->thumb_row > ISP39_DHAZ_THUMB_ROW_MAX ?
				ISP39_DHAZ_THUMB_ROW_MAX : arg->thumb_row & ~1;
	thumb_col = arg->thumb_col > ISP39_DHAZ_THUMB_COL_MAX ?
				ISP39_DHAZ_THUMB_COL_MAX : arg->thumb_col & ~1;
	if (dev->hw_dev->dev_link_num > 1 && thumb_row > 4 &&
	    !dev->hw_dev->is_frm_buf && thumb_col > 4) {
		thumb_row = 4;
		thumb_col = 4;
	}
	blk_het = ALIGN(h / thumb_row, 2);
	blk_wid = ALIGN(w / thumb_col, 2);
	priv_val->dhaz_blk_num = thumb_row * thumb_col;
	value = ISP_PACK_2SHORT(blk_het, blk_wid);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_BLOCK_SIZE, id);

	value = ISP_PACK_4BYTE(thumb_row, thumb_col, 0, 0);
	isp3_param_write(params_vdev, value, ISP39_DHAZ_THUMB_SIZE, id);

	if (dev->hw_dev->is_single && arg->hist_iir_wr)
		isp_dhaz_cfg_sram(params_vdev, arg, false, id);
	else if (arg->hist_iir_wr)
		memcpy(arg_rec, arg, sizeof(struct isp39_dhaz_cfg));
}

static void
isp_dhaz_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 value;
	bool real_en;

	value = isp3_param_read(params_vdev, ISP3X_DHAZ_CTRL, id);
	real_en = !!(value & ISP3X_DHAZ_ENMUX);
	if ((en && real_en) || (!en && !real_en))
		return;

	if (en) {
		value |= ISP39_SELF_FORCE_UPD | ISP3X_DHAZ_ENMUX;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1,
				    ISP3X_DHAZ_FST_FRAME, id);
	} else {
		value &= ~ISP3X_DHAZ_ENMUX;
	}
	isp3_param_write(params_vdev, value, ISP3X_DHAZ_CTRL, id);
}

static void
isp_3dlut_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp2x_3dlut_cfg *arg, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	u32 value, buf_idx, i;
	u32 *data;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	buf_idx = (priv_val->buf_3dlut_idx[id]++) % ISP39_3DLUT_BUF_NUM;

	if (!priv_val->buf_3dlut[id][buf_idx].vaddr) {
		dev_err(dev->dev, "no find 3dlut buf\n");
		return;
	}
	data = (u32 *)priv_val->buf_3dlut[id][buf_idx].vaddr;
	for (i = 0; i < arg->actual_size; i++)
		data[i] = (arg->lut_b[i] & 0x3FF) |
			  (arg->lut_g[i] & 0xFFF) << 10 |
			  (arg->lut_r[i] & 0x3FF) << 22;
	rkisp_prepare_buffer(params_vdev->dev, &priv_val->buf_3dlut[id][buf_idx]);
	value = priv_val->buf_3dlut[id][buf_idx].dma_addr;
	isp3_param_write(params_vdev, value, ISP3X_MI_LUT_3D_RD_BASE, id);
	isp3_param_write(params_vdev, arg->actual_size, ISP3X_MI_LUT_3D_RD_WSIZE, id);

	value = isp3_param_read(params_vdev, ISP3X_3DLUT_CTRL, id);
	value &= ISP3X_3DLUT_EN;

	if (value)
		isp3_param_set_bits(params_vdev, ISP3X_3DLUT_UPDATE, 0x01, id);

	isp3_param_write(params_vdev, value, ISP3X_3DLUT_CTRL, id);
}

static void
isp_3dlut_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 value;
	bool en_state;
	struct rkisp_isp_params_val_v39 *priv_val;

	value = isp3_param_read(params_vdev, ISP3X_3DLUT_CTRL, id);
	en_state = (value & ISP3X_3DLUT_EN) ? true : false;

	if (en == en_state)
		return;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	if (en && priv_val->buf_3dlut[id][0].vaddr) {
		isp3_param_set_bits(params_vdev, ISP3X_3DLUT_CTRL, 0x01, id);
		isp3_param_set_bits(params_vdev, ISP3X_3DLUT_UPDATE, 0x01, id);
	} else {
		isp3_param_clear_bits(params_vdev, ISP3X_3DLUT_CTRL, 0x01, id);
		isp3_param_clear_bits(params_vdev, ISP3X_3DLUT_UPDATE, 0x01, id);
	}
}

static void
isp_ldch_config(struct rkisp_isp_params_vdev *params_vdev,
		const struct isp39_ldch_cfg *arg, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	struct isp2x_mesh_head *head;
	int buf_idx, i;
	u32 value;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	priv_val->ldch_out_hsize = arg->out_hsize;

	value = isp3_param_read(params_vdev, ISP3X_LDCH_STS, id);
	value &= ISP39_MODULE_EN;
	value |= !!arg->map13p3_en << 7 |
		 !!arg->force_map_en << 6 |
		 !!arg->bic_mode_en << 4 |
		 !!arg->sample_avr_en << 3 |
		 !!arg->frm_end_dis << 1;
	isp3_param_write(params_vdev, value, ISP3X_LDCH_STS, id);
	if (arg->bic_mode_en) {
		for (i = 0; i < ISP39_LDCH_BIC_NUM / 4; i++) {
			value = ISP_PACK_4BYTE(arg->bicubic[i * 4], arg->bicubic[i * 4 + 1],
					       arg->bicubic[i * 4 + 2], arg->bicubic[i * 4 + 3]);
			isp3_param_write(params_vdev, value, ISP32_LDCH_BIC_TABLE0 + i * 4, id);
		}
	}

	for (i = 0; i < ISP39_MESH_BUF_NUM; i++) {
		if (!priv_val->buf_ldch[id][i].mem_priv)
			continue;
		if (arg->buf_fd == priv_val->buf_ldch[id][i].dma_fd)
			break;
	}
	if (i == ISP39_MESH_BUF_NUM) {
		dev_err(dev->dev, "cannot find ldch buf fd(%d)\n", arg->buf_fd);
		return;
	}

	if (!priv_val->buf_ldch[id][i].vaddr) {
		dev_err(dev->dev, "no ldch buffer allocated\n");
		return;
	}

	buf_idx = priv_val->buf_ldch_idx[id];
	head = (struct isp2x_mesh_head *)priv_val->buf_ldch[id][buf_idx].vaddr;
	head->stat = MESH_BUF_INIT;

	buf_idx = i;
	head = (struct isp2x_mesh_head *)priv_val->buf_ldch[id][buf_idx].vaddr;
	head->stat = MESH_BUF_CHIPINUSE;
	priv_val->buf_ldch_idx[id] = buf_idx;
	rkisp_prepare_buffer(dev, &priv_val->buf_ldch[id][buf_idx]);
	value = priv_val->buf_ldch[id][buf_idx].dma_addr + head->data_oft;
	isp3_param_write(params_vdev, value, ISP3X_MI_LUT_LDCH_RD_BASE, id);
	isp3_param_write(params_vdev, arg->hsize, ISP3X_MI_LUT_LDCH_RD_H_WSIZE, id);
	isp3_param_write(params_vdev, arg->vsize, ISP3X_MI_LUT_LDCH_RD_V_SIZE, id);
}

static void
isp_ldch_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	u32 buf_idx;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	if (en) {
		buf_idx = priv_val->buf_ldch_idx[id];
		if (!priv_val->buf_ldch[id][buf_idx].vaddr) {
			dev_err(dev->dev, "no ldch buffer allocated\n");
			isp3_param_clear_bits(params_vdev, ISP3X_LDCH_STS, 0x01, id);
			return;
		}
		isp3_param_set_bits(params_vdev, ISP3X_LDCH_STS, 0x01, id);
	} else {
		isp3_param_clear_bits(params_vdev, ISP3X_LDCH_STS, 0x01, id);
	}
}

static void
isp_ynr_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp39_ynr_cfg *arg, u32 id)
{
	u32 i, value;

	value = isp3_param_read(params_vdev, ISP3X_YNR_GLOBAL_CTRL, id);
	value &= ISP39_MODULE_EN;

	value |= !!arg->rnr_en << 26 |
		 (arg->gain_merge_alpha & 0xF) << 20 |
		 (arg->global_set_gain & 0x3FF) << 8 |
		 !!arg->exgain_bypass << 3 |
		 !!arg->hispnr_bypass << 2 |
		 !!arg->lospnr_bypass << 1;
	isp3_param_write(params_vdev, value, ISP3X_YNR_GLOBAL_CTRL, id);

	value = ISP_PACK_2SHORT(arg->rnr_max_radius, arg->local_gain_scale);
	isp3_param_write(params_vdev, value, ISP3X_YNR_RNR_MAX_R, id);

	value = ISP_PACK_2SHORT(arg->rnr_center_coorh, arg->rnr_center_coorv);
	isp3_param_write(params_vdev, value, ISP3X_YNR_RNR_CENTER_COOR, id);

	value = (arg->ds_filt_wgt_thred_scale & 0x1ff) << 16 |
		(arg->ds_img_edge_scale & 0x1f) << 10 |
		(arg->ds_filt_soft_thred_scale & 0x1ff);
	isp3_param_write(params_vdev, value, ISP3X_YNR_LOWNR_CTRL0, id);

	value = (arg->ds_filt_center_wgt & 0x7ff) << 16 |
		(arg->ds_iir_init_wgt_scale & 0x3f) << 8 |
		(arg->ds_filt_local_gain_alpha & 0x1f);
	isp3_param_write(params_vdev, value, ISP3X_YNR_LOWNR_CTRL1, id);

	value = ISP_PACK_2SHORT(arg->ds_filt_inv_strg, arg->lospnr_wgt);
	isp3_param_write(params_vdev, value, ISP3X_YNR_LOWNR_CTRL2, id);

	value = ISP_PACK_2SHORT(arg->lospnr_center_wgt, arg->lospnr_strg);
	isp3_param_write(params_vdev, value, ISP3X_YNR_LOWNR_CTRL3, id);

	value = ISP_PACK_2SHORT(arg->lospnr_dist_vstrg_scale, arg->lospnr_dist_hstrg_scale);
	isp3_param_write(params_vdev, value, ISP3X_YNR_LOWNR_CTRL4, id);

	value = arg->pre_filt_coeff2 << 16 |
		arg->pre_filt_coeff1 << 8 |
		arg->pre_filt_coeff0;
	isp3_param_write(params_vdev, value, ISP39_YNR_GAUSS_COEFF, id);

	for (i = 0; i < ISP39_YNR_LOW_GAIN_ADJ_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->lospnr_gain2strg_val[i * 4],
				       arg->lospnr_gain2strg_val[i * 4 + 1],
				       arg->lospnr_gain2strg_val[i * 4 + 2],
				       arg->lospnr_gain2strg_val[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP39_YNR_LOW_GAIN_ADJ_0_3 + i * 4, id);
	}
	value = arg->lospnr_gain2strg_val[i * 4];
	isp3_param_write(params_vdev, value, ISP39_YNR_LOW_GAIN_ADJ_0_3 + i * 4, id);

	for (i = 0; i < ISP39_YNR_XY_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->luma2sima_idx[2 * i],
					arg->luma2sima_idx[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP39_YNR_SGM_DX_0_1 + 4 * i, id);
	}
	value = arg->luma2sima_idx[2 * i];
	isp3_param_write(params_vdev, value, ISP39_YNR_SGM_DX_0_1 + 4 * i, id);

	for (i = 0; i < ISP39_YNR_XY_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->luma2sima_val[2 * i],
					arg->luma2sima_val[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP39_YNR_LSGM_Y_0_1 + 4 * i, id);
	}
	value = arg->luma2sima_val[2 * i];
	isp3_param_write(params_vdev, value, ISP39_YNR_LSGM_Y_0_1 + 4 * i, id);

	for (i = 0; i < ISP39_YNR_XY_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->radius2strg_val[4 * i],
				       arg->radius2strg_val[4 * i + 1],
				       arg->radius2strg_val[4 * i + 2],
				       arg->radius2strg_val[4 * i + 3]);
		isp3_param_write(params_vdev, value, ISP39_YNR_RNR_STRENGTH03 + 4 * i, id);
	}
	value = arg->radius2strg_val[4 * i];
	isp3_param_write(params_vdev, value, ISP39_YNR_RNR_STRENGTH03 + 4 * i, id);

	value = arg->hispnr_strong_edge;
	isp3_param_write(params_vdev, value, ISP39_YNR_NLM_STRONG_EDGE, id);

	value = (arg->hispnr_strg & 0x3ff) << 16 |
		(arg->hispnr_local_gain_alpha & 0x1f) << 11 |
		(arg->hispnr_sigma_min_limit & 0x7ff);
	isp3_param_write(params_vdev, value, ISP39_YNR_NLM_SIGMA_GAIN, id);

	value = (arg->hispnr_filt_coeff[5] & 0xf) << 20 | (arg->hispnr_filt_coeff[4] & 0xf) << 16 |
		(arg->hispnr_filt_coeff[3] & 0xf) << 12 | (arg->hispnr_filt_coeff[2] & 0xf) << 8 |
		(arg->hispnr_filt_coeff[1] & 0xf) << 4 | (arg->hispnr_filt_coeff[0] & 0xf);
	isp3_param_write(params_vdev, value, ISP39_YNR_NLM_COE, id);

	value = (arg->hispnr_filt_center_wgt & 0x3ffff) << 10 | (arg->hispnr_filt_wgt_offset & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_YNR_NLM_WEIGHT, id);

	value = (arg->hispnr_gain_thred & 0x3ff) << 16 | (arg->hispnr_filt_wgt & 0x7ff);
	isp3_param_write(params_vdev, value, ISP39_YNR_NLM_NR_WEIGHT, id);
}

static void
isp_ynr_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 ynr_ctrl;
	bool real_en;

	ynr_ctrl = isp3_param_read_cache(params_vdev, ISP3X_YNR_GLOBAL_CTRL, id);
	real_en = !!(ynr_ctrl & ISP39_MODULE_EN);
	if ((en && real_en) || (!en && !real_en))
		return;

	if (en) {
		ynr_ctrl |= ISP39_MODULE_EN;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP3X_YNR_FST_FRAME, id);
	} else {
		ynr_ctrl &= ~ISP39_MODULE_EN;
	}

	isp3_param_write(params_vdev, ynr_ctrl, ISP3X_YNR_GLOBAL_CTRL, id);
}

static void
isp_cnr_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp39_cnr_cfg *arg, u32 id)
{
	u32 i, value, ctrl, gain_ctrl;

	gain_ctrl = isp3_param_read(params_vdev, ISP3X_GAIN_CTRL, id);
	ctrl = isp3_param_read(params_vdev, ISP3X_CNR_CTRL, id);
	ctrl &= ISP39_MODULE_EN;

	ctrl |= (arg->loflt_coeff & 0x3f) << 12 |
		!!arg->hiflt_wgt0_mode << 8 |
		(arg->thumb_mode & 0x3) << 4 |
		!!arg->yuv422_mode << 2 |
		!!arg->exgain_bypass << 1;
	value = (arg->global_gain & 0x3ff) |
		(arg->global_gain_alpha & 0xf) << 12 |
		arg->local_gain_scale << 16;
	/* gain disable, using global gain for cnr */
	if (ctrl & ISP39_MODULE_EN && !(gain_ctrl & ISP39_MODULE_EN)) {
		ctrl |= BIT(1);
		value &= ~ISP3X_CNR_GLOBAL_GAIN_ALPHA_MAX;
		value |= BIT(15);
	}
	isp3_param_write(params_vdev, ctrl, ISP3X_CNR_CTRL, id);
	isp3_param_write(params_vdev, value, ISP3X_CNR_EXGAIN, id);

	value = ISP_PACK_2SHORT(arg->lobfflt_vsigma_uv, arg->lobfflt_vsigma_y);
	isp3_param_write(params_vdev, value, ISP32_CNR_THUMB1, id);

	value = arg->lobfflt_alpha & 0x7ff;
	isp3_param_write(params_vdev, value, ISP32_CNR_THUMB_BF_RATIO, id);

	value = ISP_PACK_4BYTE(arg->thumb_bf_coeff0, arg->thumb_bf_coeff1,
			       arg->thumb_bf_coeff2, arg->thumb_bf_coeff3);
	isp3_param_write(params_vdev, value, ISP32_CNR_LBF_WEITD, id);

	value = (arg->loflt_wgt_slope & 0x3ff) << 20 |
		(arg->exp_x_shift_bit & 0x3f) << 12 |
		arg->loflt_vsigma << 4 | (arg->loflt_uv_gain & 0xf);
	isp3_param_write(params_vdev, value, ISP32_CNR_IIR_PARA1, id);

	value = (arg->loflt_wgt_max_limit & 0x7f) << 8 |
		(arg->loflt_wgt_min_thred & 0x3f);
	isp3_param_write(params_vdev, value, ISP32_CNR_IIR_PARA2, id);

	value = ISP_PACK_4BYTE(arg->gaus_flt_coeff[0], arg->gaus_flt_coeff[1],
			       arg->gaus_flt_coeff[2], arg->gaus_flt_coeff[3]);
	isp3_param_write(params_vdev, value, ISP32_CNR_GAUS_COE1, id);

	value = ISP_PACK_4BYTE(arg->gaus_flt_coeff[4], arg->gaus_flt_coeff[5], 0, 0);
	isp3_param_write(params_vdev, value, ISP32_CNR_GAUS_COE2, id);

	value = (arg->hiflt_alpha & 0x7ff) << 20 |
		arg->hiflt_wgt_min_limit << 12 |
		(arg->gaus_flt_alpha & 0x7ff);
	isp3_param_write(params_vdev, value, ISP32_CNR_GAUS_RATIO, id);

	value = arg->hiflt_cur_wgt << 24 |
		(arg->hiflt_global_vsigma & 0x3fff) << 8 |
		(arg->hiflt_uv_gain & 0x7f);
	isp3_param_write(params_vdev, value, ISP32_CNR_BF_PARA1, id);

	value = (arg->adj_scale & 0x7fff) << 16 | (arg->adj_offset & 0x1ff);
	isp3_param_write(params_vdev, value, ISP32_CNR_BF_PARA2, id);

	for (i = 0; i < ISP39_CNR_SIGMA_Y_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->sgm_ratio[i * 4], arg->sgm_ratio[i * 4 + 1],
				       arg->sgm_ratio[i * 4 + 2], arg->sgm_ratio[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP32_CNR_SIGMA0 + i * 4, id);
	}
	value = arg->sgm_ratio[i * 4];
	isp3_param_write(params_vdev, value, ISP32_CNR_SIGMA0 + i * 4, id);

	value = (arg->loflt_global_sgm_ratio_alpha & 0xf) << 8 |
		arg->loflt_global_sgm_ratio;
	isp3_param_write(params_vdev, value, ISP32_CNR_IIR_GLOBAL_GAIN, id);

	for (i = 0; i < ISP39_CNR_WGT_SIGMA_Y_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->cur_wgt[i * 4], arg->cur_wgt[i * 4 + 1],
				       arg->cur_wgt[i * 4 + 2], arg->cur_wgt[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP39_CNR_WGT_SIGMA0 + i * 4, id);
	}
	value = arg->cur_wgt[i * 4];
	isp3_param_write(params_vdev, value, ISP39_CNR_WGT_SIGMA0 + i * 4, id);

	for (i = 0; i < ISP39_CNR_GAUS_SIGMAR_NUM / 3; i++) {
		value = (arg->hiflt_vsigma_idx[i * 3 + 2] & 0x3ff) << 20 |
			(arg->hiflt_vsigma_idx[i * 3 + 1] & 0x3ff) << 10 |
			(arg->hiflt_vsigma_idx[i * 3] & 0x3ff);
		isp3_param_write(params_vdev, value, ISP39_CNR_GAUS_X_SIGMAR0 + i * 4, id);
	}
	value = (arg->hiflt_vsigma_idx[i * 3 + 1] & 0x3ff) << 20 |
		(arg->hiflt_vsigma_idx[i * 3] & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_CNR_GAUS_X_SIGMAR0 + i * 4, id);

	for (i = 0; i < ISP39_CNR_GAUS_SIGMAR_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->hiflt_vsigma[i * 2], arg->hiflt_vsigma[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_CNR_GAUS_Y_SIGMAR0 + i * 4, id);
	}
}

static void
isp_cnr_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 cnr_ctrl;
	bool real_en;

	cnr_ctrl = isp3_param_read_cache(params_vdev, ISP3X_CNR_CTRL, id);
	real_en = !!(cnr_ctrl & ISP39_MODULE_EN);
	if ((en && real_en) || (!en && !real_en))
		return;

	if (en) {
		cnr_ctrl |= ISP39_MODULE_EN;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1,
				    ISP3X_CNR_FST_FRAME, id);
	} else {
		cnr_ctrl &= ~ISP39_MODULE_EN;
	}

	isp3_param_write(params_vdev, cnr_ctrl, ISP3X_CNR_CTRL, id);
}

static void
isp_sharp_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp39_sharp_cfg *arg, u32 id)
{
	u32 i, value;

	value = isp3_param_read(params_vdev, ISP3X_SHARP_EN, id);
	value &= ISP39_MODULE_EN;

	value |= !!arg->bypass << 1 |
		 !!arg->center_mode << 2 |
		 !!arg->local_gain_bypass << 3 |
		 !!arg->radius_step_mode << 4 |
		 !!arg->noise_clip_mode << 5 |
		 !!arg->clipldx_sel << 6 |
		 !!arg->baselmg_sel << 7 |
		 !!arg->noise_filt_sel << 8 |
		 !!arg->tex2wgt_en << 9;
	isp3_param_write(params_vdev, value, ISP3X_SHARP_EN, id);

	value = ISP_PACK_4BYTE(arg->pre_bifilt_alpha, arg->guide_filt_alpha,
			       arg->detail_bifilt_alpha, arg->global_sharp_strg);
	isp3_param_write(params_vdev, value, ISP39_SHARP_ALPHA, id);

	value = (arg->luma2table_idx[6] & 0x0F) << 24 |
		(arg->luma2table_idx[5] & 0x0F) << 20 |
		(arg->luma2table_idx[4] & 0x0F) << 16 |
		(arg->luma2table_idx[3] & 0x0F) << 12 |
		(arg->luma2table_idx[2] & 0x0F) << 8 |
		(arg->luma2table_idx[1] & 0x0F) << 4 |
		(arg->luma2table_idx[0] & 0x0F);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_LUMA_DX, id);

	value = (arg->pbf_sigma_inv[2] & 0x3FF) << 20 |
		(arg->pbf_sigma_inv[1] & 0x3FF) << 10 |
		(arg->pbf_sigma_inv[0] & 0x3FF);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_PBF_SIGMA_INV_0, id);

	value = (arg->pbf_sigma_inv[5] & 0x3FF) << 20 |
		(arg->pbf_sigma_inv[4] & 0x3FF) << 10 |
		(arg->pbf_sigma_inv[3] & 0x3FF);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_PBF_SIGMA_INV_1, id);

	value = (arg->pbf_sigma_inv[7] & 0x3FF) << 10 |
		(arg->pbf_sigma_inv[6] & 0x3FF);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_PBF_SIGMA_INV_2, id);

	value = (arg->bf_sigma_inv[2] & 0x3FF) << 20 |
		(arg->bf_sigma_inv[1] & 0x3FF) << 10 |
		(arg->bf_sigma_inv[0] & 0x3FF);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_BF_SIGMA_INV_0, id);

	value = (arg->bf_sigma_inv[5] & 0x3FF) << 20 |
		(arg->bf_sigma_inv[4] & 0x3FF) << 10 |
		(arg->bf_sigma_inv[3] & 0x3FF);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_BF_SIGMA_INV_1, id);

	value = (arg->bf_sigma_inv[7] & 0x3FF) << 10 |
		(arg->bf_sigma_inv[6] & 0x3FF);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_BF_SIGMA_INV_2, id);

	value = (arg->bf_sigma_shift & 0x0F) << 4 |
		(arg->pbf_sigma_shift & 0x0F);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_SIGMA_SHIFT, id);

	for (i = 0; i < ISP39_SHARP_Y_NUM / 3; i++) {
		value = (arg->luma2strg_val[i * 3] & 0x3ff) |
			(arg->luma2strg_val[i * 3 + 1] & 0x3ff) << 10 |
			(arg->luma2strg_val[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_SHARP_LOCAL_STRG_0 + i * 4, id);
	}
	value = (arg->luma2strg_val[i * 3] & 0x3ff) |
		(arg->luma2strg_val[i * 3 + 1] & 0x3ff) << 10;
	isp3_param_write(params_vdev, value, ISP39_SHARP_LOCAL_STRG_2, id);

	for (i = 0; i < ISP39_SHARP_Y_NUM / 3; i++) {
		value = (arg->luma2posclip_val[i * 3] & 0x3ff) |
			(arg->luma2posclip_val[i * 3 + 1] & 0x3ff) << 10 |
			(arg->luma2posclip_val[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_SHARP_POS_CLIP_0 + i * 4, id);
	}
	value = (arg->luma2posclip_val[i * 3] & 0x3ff) |
		(arg->luma2posclip_val[i * 3 + 1] & 0x3ff) << 10;
	isp3_param_write(params_vdev, value, ISP39_SHARP_POS_CLIP_2, id);

	value = ISP_PACK_4BYTE(arg->pbf_coef0, arg->pbf_coef1, arg->pbf_coef2, 0);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_PBF_COEF, id);

	value = ISP_PACK_4BYTE(arg->bf_coef0, arg->bf_coef1, arg->bf_coef2, 0);
	isp3_param_write(params_vdev, value, ISP39_SHARP_DETAILBF_COEF, id);

	value = ISP_PACK_4BYTE(arg->img_lpf_coeff[0], arg->img_lpf_coeff[1], arg->img_lpf_coeff[2], 0);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_IMGLPF_COEF_0, id);

	value = ISP_PACK_4BYTE(arg->img_lpf_coeff[3], arg->img_lpf_coeff[4], arg->img_lpf_coeff[5], 0);
	isp3_param_write(params_vdev, value, ISP3X_SHARP_IMGLPF_COEF_1, id);

	value = arg->local_gain_scale << 24 | (arg->gain_merge_alpha & 0xf) << 16 |
		(arg->global_gain & 0x3ff);
	isp3_param_write(params_vdev, value, ISP32_SHARP_GAIN, id);

	for (i = 0; i < ISP39_SHARP_GAIN_ADJ_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->gain2strg_val[i * 2], arg->gain2strg_val[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP32_SHARP_GAIN_ADJUST0 + i * 4, id);
	}

	value = ISP_PACK_2SHORT(arg->center_x, arg->center_y);
	isp3_param_write(params_vdev, value, ISP32_SHARP_CENTER, id);

	for (i = 0; i < ISP39_SHARP_STRENGTH_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->distance2strg_val[i * 4], arg->distance2strg_val[i * 4 + 1],
				       arg->distance2strg_val[i * 4 + 2], arg->distance2strg_val[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP32_SHARP_GAIN_DIS_STRENGTH0 + i * 4, id);
	}
	value = ISP_PACK_4BYTE(arg->distance2strg_val[i * 4], arg->distance2strg_val[i * 4 + 1], 0, 0);
	isp3_param_write(params_vdev, value, ISP32_SHARP_GAIN_DIS_STRENGTH0 + i * 4, id);

	for (i = 0; i < ISP39_SHARP_Y_NUM / 3; i++) {
		value = (arg->luma2neg_clip_val[i * 3 + 2] & 0x3ff) << 20 |
			(arg->luma2neg_clip_val[i * 3 + 1] & 0x3ff) << 10 |
			(arg->luma2neg_clip_val[i * 3] & 0x3ff);
		isp3_param_write(params_vdev, value, ISP39_SHARP_CLIP_NEG_0 + i * 4, id);
	}
	value = (arg->luma2neg_clip_val[i * 3 + 1] & 0x3ff) << 10 | (arg->luma2neg_clip_val[i * 3] & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_SHARP_CLIP_NEG_0 + i * 4, id);

	value = (arg->tex_reserve_level & 0xf) << 12 | (arg->noise_max_limit & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_SHARP_TEXTURE0, id);

	value = (arg->tex_wgt_mode & 0x3) << 20 |
		(arg->noise_norm_bit & 0xf) << 16 |
		(arg->tex_wet_scale & 0x7fff);
	isp3_param_write(params_vdev, value, ISP39_SHARP_TEXTURE1, id);

	for (i = 0; i < ISP39_SHARP_TEX_WET_LUT_NUM / 3; i++) {
		value = (arg->tex2wgt_val[i * 3 + 2] & 0x3ff) << 20 |
			(arg->tex2wgt_val[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tex2wgt_val[i * 3] & 0x3ff);
		isp3_param_write(params_vdev, value, ISP39_SHARP_TEXTURE_LUT0 + i * 4, id);
	}
	value = (arg->tex2wgt_val[i * 3 + 1] & 0x3ff) << 10 | (arg->tex2wgt_val[i * 3] & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_SHARP_TEXTURE_LUT0 + i * 4, id);

	value = arg->noise_strg;
	isp3_param_write(params_vdev, value, ISP39_SHARP_TEXTURE2, id);

	for (i = 0; i < ISP39_SHARP_DETAIL_STRG_NUM / 2; i++) {
		value = (arg->detail2strg_val[i * 2] & 0x7ff) |
			(arg->detail2strg_val[i * 2 + 1] & 0x7ff) << 16;
		isp3_param_write(params_vdev, value, ISP39_SHARP_DETAIL_STRG_LUT0 + i * 4, id);
	}
	value = arg->detail2strg_val[i * 2] & 0x7ff;
	isp3_param_write(params_vdev, value, ISP39_SHARP_DETAIL_STRG_LUT8, id);
}

static void
isp_sharp_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 value;

	value = isp3_param_read_cache(params_vdev, ISP3X_SHARP_EN, id);
	if ((en && (value & ISP39_MODULE_EN)) || (!en && !(value & ISP39_MODULE_EN)))
		return;

	value &= ~ISP39_MODULE_EN;
	if (en) {
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP32_SHP_FST_FRAME, id);
		value |= ISP39_MODULE_EN;
	}
	isp3_param_write(params_vdev, value, ISP3X_SHARP_EN, id);
}

static void
isp_bay3d_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp39_bay3d_cfg *arg, u32 id)
{
	u32 i, value, ctrl;

	ctrl = isp3_param_read(params_vdev, ISP3X_BAY3D_CTRL, id);
	if (ctrl & BIT(1) && !arg->bypass_en)
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP3X_RAW3D_FST_FRAME, id);
	ctrl &= ISP39_MODULE_EN;

	ctrl |= !!arg->bypass_en << 1 | !!arg->iirsparse_en << 2;
	isp3_param_write(params_vdev, ctrl, ISP3X_BAY3D_CTRL, id);

	value = isp3_param_read_cache(params_vdev, ISP3X_HDRMGE_CTRL, id);
	if (arg->transf_bypass_en && (value & ISP39_MODULE_EN))
		dev_err(params_vdev->dev->dev, "bay3d transf_bypass_en=1 but hdr\n");

	value = !!arg->noisebal_mode << 23 |
		!!arg->curdbg_out_en << 22 |
		!!arg->lomdwgt_dbg_en << 21 |
		!!arg->iirspnr_out_en << 20 |
		!!arg->md_bypass_en << 19 |
		!!arg->md_wgt_out_en << 18 |
		(arg->lo_detection_mode & 0x3) << 16 |
		!!arg->spnr_pre_sigma_use_en << 15 |
		!!arg->sig_hfilt_en << 13 |
		!!arg->lo_diff_hfilt_en << 12 |
		!!arg->lo_wgt_hfilt_en << 11 |
		!!arg->lpf_lo_bypass_en << 10 |
		!!arg->lo_diff_vfilt_bypass_en << 9 |
		!!arg->lpf_hi_bypass_en << 8 |
		!!arg->pre_spnr_sigma_curve_double_en << 7 |
		!!arg->pre_spnr_sigma_idxfilt_bypass_en << 6 |
		!!arg->pre_spnr_bypass_en << 5 |
		!!arg->cur_spnr_sigma_curve_double_en << 4 |
		!!arg->cur_spnr_sigma_idxfilt_bypass_en << 3 |
		!!arg->cur_spnr_bypass_en << 2 |
		!!arg->sigma_curve_double_en << 1 |
		!!arg->transf_bypass_en;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_CTRL1, id);

	value = (arg->sigma_calc_mge_wgt_hdr_sht_thred & 0x3f) << 24 |
		(arg->mge_wgt_hdr_sht_thred & 0x3f) << 16 |
		!!arg->kalman_wgt_ds_mode << 3 |
		!!arg->mge_wgt_ds_mode << 2 |
		!!arg->wgt_cal_mode << 1 |
		!!arg->transf_mode;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_CTRL2, id);

	value = (arg->transf_mode_offset & 0x1fff) |
		!!arg->transf_mode_scale << 15 |
		arg->itransf_mode_offset << 16;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TRANS0, id);

	value = arg->transf_data_max_limit & 0xfffff;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TRANS1, id);

	value = ISP_PACK_2SHORT(arg->cur_spnr_sigma_hdr_sht_scale,
				arg->cur_spnr_sigma_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_CURDGAIN, id);

	for (i = 0; i < ISP39_BAY3D_XY_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->cur_spnr_luma_sigma_x[i * 2],
					arg->cur_spnr_luma_sigma_x[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_BAY3D_CURSIG_X0 + i * 4, id);

		value = ISP_PACK_2SHORT(arg->cur_spnr_luma_sigma_y[i * 2],
					arg->cur_spnr_luma_sigma_y[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_BAY3D_CURSIG_Y0 + i * 4, id);
	}

	value = ISP_PACK_2SHORT(arg->cur_spnr_sigma_rgain_offset,
				arg->cur_spnr_sigma_bgain_offset);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_CURGAIN_OFF, id);

	value = ISP_PACK_2SHORT(arg->cur_spnr_sigma_hdr_sht_offset,
				arg->cur_spnr_sigma_offset);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_CURSIG_OFF, id);

	value = ISP_PACK_2SHORT(arg->cur_spnr_pix_diff_max_limit,
				arg->cur_spnr_wgt_cal_offset);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_CURWTH, id);

	value = ISP_PACK_2SHORT(arg->cur_spnr_wgt, arg->pre_spnr_wgt);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_CURBFALP, id);

	for (i = 0; i < ISP39_BAY3D_WD_NUM / 3; i++) {
		value = (arg->cur_spnr_space_rb_wgt[i * 3] & 0x3ff) |
			(arg->cur_spnr_space_rb_wgt[i * 3 + 1] & 0x3ff) << 10 |
			(arg->cur_spnr_space_rb_wgt[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_BAY3D_CURWDC0 + i * 4, id);

		value = (arg->cur_spnr_space_gg_wgt[i * 3] & 0x3ff) |
			(arg->cur_spnr_space_gg_wgt[i * 3 + 1] & 0x3ff) << 10 |
			(arg->cur_spnr_space_gg_wgt[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_BAY3D_CURWDY0 + i * 4, id);
	}

	value = ISP_PACK_2SHORT(arg->pre_spnr_sigma_hdr_sht_scale, arg->pre_spnr_sigma_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_IIRDGAIN, id);

	for (i = 0; i < ISP39_BAY3D_XY_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->pre_spnr_luma_sigma_x[i * 2],
					arg->pre_spnr_luma_sigma_x[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_BAY3D_IIRSIG_X0 + i * 4, id);

		value = ISP_PACK_2SHORT(arg->cur_spnr_luma_sigma_y[i * 2],
					arg->cur_spnr_luma_sigma_y[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_BAY3D_IIRSIG_Y0 + i * 4, id);
	}

	value = ISP_PACK_2SHORT(arg->pre_spnr_sigma_rgain_offset,
				arg->pre_spnr_sigma_bgain_offset);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_IIRGAIN_OFF, id);

	value = ISP_PACK_2SHORT(arg->pre_spnr_sigma_hdr_sht_offset,
				arg->pre_spnr_sigma_offset);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_IIRSIG_OFF, id);

	value = ISP_PACK_2SHORT(arg->pre_spnr_pix_diff_max_limit,
				arg->pre_spnr_wgt_cal_offset);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_IIRWTH, id);

	for (i = 0; i < ISP39_BAY3D_WD_NUM / 3; i++) {
		value = (arg->pre_spnr_space_rb_wgt[i * 3] & 0x3ff) |
			(arg->pre_spnr_space_rb_wgt[i * 3 + 1] & 0x3ff) << 10 |
			(arg->pre_spnr_space_rb_wgt[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_BAY3D_IIRWDC0 + i * 4, id);

		value = (arg->pre_spnr_space_gg_wgt[i * 3] & 0x3ff) |
			(arg->pre_spnr_space_gg_wgt[i * 3 + 1] & 0x3ff) << 10 |
			(arg->pre_spnr_space_gg_wgt[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_BAY3D_IIRWDY0 + i * 4, id);
	}

	value = ISP_PACK_2SHORT(arg->cur_spnr_wgt_cal_scale,
				arg->pre_spnr_wgt_cal_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_BFCOEF, id);

	for (i = 0; i < ISP39_BAY3D_TNRSIG_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->tnr_luma_sigma_x[i * 2],
					arg->tnr_luma_sigma_x[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRSIG_X0 + i * 4, id);

		value = ISP_PACK_2SHORT(arg->tnr_luma_sigma_y[i * 2],
					arg->tnr_luma_sigma_y[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRSIG_Y0 + i * 4, id);
	}

	for (i = 0; i < ISP39_BAY3D_COEFF_NUM / 3; i++) {
		value = (arg->tnr_lpf_hi_coeff[i * 3] & 0x3ff) |
			(arg->tnr_lpf_hi_coeff[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tnr_lpf_hi_coeff[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRHIW0 + i * 4, id);

		value = (arg->tnr_lpf_lo_coeff[i * 3] & 0x3ff) |
			(arg->tnr_lpf_lo_coeff[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tnr_lpf_lo_coeff[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRLOW0 + i * 4, id);
	}

	value = (arg->tnr_wgt_filt_coeff0 & 0x3ff) |
		(arg->tnr_wgt_filt_coeff1 & 0x3ff) << 10 |
		(arg->tnr_wgt_filt_coeff2 & 0x3ff) << 20;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRGF3, id);

	value = ISP_PACK_2SHORT(arg->tnr_sigma_scale, arg->tnr_sigma_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRSIGSCL, id);

	value = (arg->tnr_sig_vfilt_wgt & 0xf) |
		(arg->tnr_lo_diff_vfilt_wgt & 0xf) << 4 |
		(arg->tnr_lo_wgt_vfilt_wgt & 0xf) << 8 |
		(arg->tnr_sig_first_line_scale & 0x1f) << 16 |
		(arg->tnr_lo_diff_first_line_scale & 0x1f) << 24;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRVIIR, id);

	value = ISP_PACK_2SHORT(arg->tnr_lo_wgt_cal_offset, arg->tnr_lo_wgt_cal_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRLFSCL, id);

	value = ISP_PACK_2SHORT(arg->tnr_low_wgt_cal_max_limit, arg->tnr_mode0_base_ratio);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRLFSCLTH, id);

	value = ISP_PACK_2SHORT(arg->tnr_lo_diff_wgt_cal_offset, arg->tnr_lo_diff_wgt_cal_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRDSWGTSCL, id);

	value = ISP_PACK_2SHORT(arg->tnr_lo_mge_pre_wgt_offset, arg->tnr_lo_mge_pre_wgt_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWLSTSCL, id);

	value = ISP_PACK_2SHORT(arg->tnr_mode0_lo_wgt_scale, arg->tnr_mode0_lo_wgt_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWGT0SCL0, id);

	value = ISP_PACK_2SHORT(arg->tnr_mode1_lo_wgt_scale, arg->tnr_mode1_lo_wgt_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWGT1SCL1, id);

	value = ISP_PACK_2SHORT(arg->tnr_mode1_wgt_scale, arg->tnr_mode1_wgt_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWGT1SCL2, id);

	value = ISP_PACK_2SHORT(arg->tnr_mode1_lo_wgt_offset, arg->tnr_mode1_lo_wgt_hdr_sht_offset);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWGTOFF, id);

	value = (arg->tnr_auto_sigma_count_wgt_thred & 0x3ff) |
		(arg->tnr_mode1_wgt_min_limit & 0x3ff) << 10 |
		(arg->tnr_mode1_wgt_offset & 0xfff) << 20;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWGT1OFF, id);

	value = arg->tnr_out_sigma_sq;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRSIGORG, id);

	value = ISP_PACK_2SHORT(arg->tnr_lo_wgt_clip_min_limit, arg->tnr_lo_wgt_clip_hdr_sht_min_limit);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWLO_THL, id);

	value = ISP_PACK_2SHORT(arg->tnr_lo_wgt_clip_max_limit, arg->tnr_lo_wgt_clip_hdr_sht_max_limit);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWLO_THH, id);

	value = ISP_PACK_2SHORT(arg->tnr_hi_wgt_clip_min_limit, arg->tnr_hi_wgt_clip_hdr_sht_min_limit);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWHI_THL, id);

	value = ISP_PACK_2SHORT(arg->tnr_hi_wgt_clip_max_limit, arg->tnr_hi_wgt_clip_hdr_sht_max_limit);
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRWHI_THH, id);

	value = arg->tnr_cur_spnr_hi_wgt_min_limit | arg->tnr_pre_spnr_hi_wgt_min_limit << 16;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRKEEP, id);

	value = (arg->tnr_pix_max & 0xfff) | (arg->lowgt_ctrl & 0x3) << 16 |
		(arg->lowgt_offint & 0x3ff) << 18;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_PIXMAX, id);

	value = arg->tnr_auto_sigma_count_th;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_SIGNUMTH, id);

	value = (arg->tnr_motion_nr_strg & 0x7ff) | arg->tnr_gain_max << 16;
	isp3_param_write(params_vdev, value, ISP39_BAY3D_TNRMO_STR, id);

	if (params_vdev->dev->hw_dev->is_single && ctrl & ISP39_MODULE_EN)
		isp3_param_write(params_vdev, ctrl | ISP39_SELF_FORCE_UPD, ISP3X_BAY3D_CTRL, id);
}

static void
isp_bay3d_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_device *ispdev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	u32 value, bay3d_ctrl;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	bay3d_ctrl = isp3_param_read_cache(params_vdev, ISP3X_BAY3D_CTRL, id);
	if ((en && (bay3d_ctrl & ISP39_MODULE_EN)) ||
	    (!en && !(bay3d_ctrl & ISP39_MODULE_EN)))
		return;

	if (en) {
		if (!priv_val->buf_bay3d_iir[0].mem_priv) {
			dev_err(ispdev->dev, "no bay3d buffer available\n");
			return;
		}

		priv_val->bay3d_iir_idx = 0;
		priv_val->bay3d_iir_cur_idx = 0;
		value = priv_val->bay3d_iir_size;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_IIR_WR_SIZE, id);
		value = priv_val->buf_bay3d_iir[0].dma_addr + value * id;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_IIR_WR_BASE, id);
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_IIR_RD_BASE, id);
		if (priv_val->buf_aiisp[0].mem_priv) {
			priv_val->aiisp_cur_idx = 0;
			value = priv_val->buf_aiisp[0].dma_addr + value * id;
			isp3_param_write(params_vdev, value, ISP39_AIISP_RD_BASE, id);
		}
		if (priv_val->buf_gain[0].mem_priv) {
			priv_val->gain_cur_idx = 0;
			value = priv_val->gain_size;
			isp3_param_write(params_vdev, value, ISP3X_MI_GAIN_WR_SIZE, id);
			isp3_param_write(params_vdev, value, ISP32_MI_RAW0_RD_SIZE, id);
			value = priv_val->buf_gain[0].dma_addr + value * id;
			isp3_param_write(params_vdev, value, ISP3X_MI_GAIN_WR_BASE, id);
			isp3_param_write(params_vdev, value, ISP3X_MI_RAW0_RD_BASE, id);
		}

		bay3d_ctrl |= ISP39_MODULE_EN;
		isp3_param_write(params_vdev, bay3d_ctrl, ISP3X_BAY3D_CTRL, id);

		value = ISP3X_BAY3D_IIR_WR_AUTO_UPD | ISP3X_BAY3D_IIRSELF_UPD |
			ISP3X_BAY3D_RDSELF_UPD;
		if (priv_val->buf_gain[0].mem_priv)
			value |= ISP3X_GAIN_WR_AUTO_UPD | ISP3X_GAINSELF_UPD;
		isp3_param_set_bits(params_vdev, MI_WR_CTRL2, value, id);

		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP3X_RAW3D_FST_FRAME, id);
	} else {
		bay3d_ctrl &= ~ISP39_MODULE_EN;
		isp3_param_write(params_vdev, bay3d_ctrl, ISP3X_BAY3D_CTRL, id);
	}
}

static void
isp_gain_config(struct rkisp_isp_params_vdev *params_vdev,
		const struct isp3x_gain_cfg *arg, u32 id)
{
	u32 val = arg->g0 & 0x3ffff;

	isp3_param_write(params_vdev, val, ISP3X_GAIN_G0, id);
	val = ISP_PACK_2SHORT(arg->g1, arg->g2);
	isp3_param_write(params_vdev, val, ISP3X_GAIN_G1_G2, id);
}

static void
isp_gain_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_GAIN_CTRL, id);

	if (en)
		val |= ISP39_MODULE_EN;
	else
		val &= ~ISP39_MODULE_EN;
	isp3_param_write(params_vdev, val, ISP3X_GAIN_CTRL, id);
}

static void
isp_cac_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp32_cac_cfg *arg, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	struct isp2x_mesh_head *head;
	u32 i, val, ctrl;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;

	ctrl = isp3_param_read(params_vdev, ISP3X_CAC_CTRL, id);
	ctrl &= ISP3X_CAC_EN;
	ctrl |= !!arg->bypass_en << 1 | !!arg->center_en << 3 |
		(arg->clip_g_mode & 0x3) << 5 | !!arg->edge_detect_en << 7 |
		!!arg->neg_clip0_en << 9;

	val = (arg->psf_sft_bit & 0xff) | (arg->cfg_num & 0x7ff) << 8;
	isp3_param_write(params_vdev, val, ISP3X_CAC_PSF_PARA, id);

	val = ISP_PACK_2SHORT(arg->center_width, arg->center_height);
	isp3_param_write(params_vdev, val, ISP3X_CAC_STRENGTH_CENTER, id);

	for (i = 0; i < ISP39_CAC_STRENGTH_NUM / 2; i++) {
		val = ISP_PACK_2SHORT(arg->strength[2 * i], arg->strength[2 * i + 1]);
		isp3_param_write(params_vdev, val, ISP3X_CAC_STRENGTH0 + i * 4, id);
	}

	val = (arg->flat_thed_r & 0x1f) << 8 | (arg->flat_thed_b & 0x1f);
	isp3_param_write(params_vdev, val, ISP32_CAC_FLAT_THED, id);

	val = ISP_PACK_2SHORT(arg->offset_b, arg->offset_r);
	isp3_param_write(params_vdev, val, ISP32_CAC_OFFSET, id);

	val = arg->expo_thed_b & 0x1fffff;
	isp3_param_write(params_vdev, val, ISP32_CAC_EXPO_THED_B, id);

	val = arg->expo_thed_r & 0x1fffff;
	isp3_param_write(params_vdev, val, ISP32_CAC_EXPO_THED_R, id);

	val = arg->expo_adj_b & 0xfffff;
	isp3_param_write(params_vdev, val, ISP32_CAC_EXPO_ADJ_B, id);

	val = arg->expo_adj_r & 0xfffff;
	isp3_param_write(params_vdev, val, ISP32_CAC_EXPO_ADJ_R, id);

	for (i = 0; i < ISP39_MESH_BUF_NUM; i++) {
		if (!priv_val->buf_cac[id][i].mem_priv)
			continue;
		if (arg->buf_fd == priv_val->buf_cac[id][i].dma_fd)
			break;
	}

	if (i == ISP39_MESH_BUF_NUM) {
		if (arg->bypass_en)
			goto end;
		dev_err(dev->dev, "cannot find cac buf fd(%d)\n", arg->buf_fd);
		return;
	}

	if (!priv_val->buf_cac[id][i].vaddr) {
		dev_err(dev->dev, "no cac buffer allocated\n");
		return;
	}

	val = priv_val->buf_cac_idx[id];
	head = (struct isp2x_mesh_head *)priv_val->buf_cac[id][val].vaddr;
	head->stat = MESH_BUF_INIT;

	head = (struct isp2x_mesh_head *)priv_val->buf_cac[id][i].vaddr;
	head->stat = MESH_BUF_CHIPINUSE;
	priv_val->buf_cac_idx[id] = i;
	rkisp_prepare_buffer(dev, &priv_val->buf_cac[id][i]);
	val = priv_val->buf_cac[id][i].dma_addr + head->data_oft;
	isp3_param_write(params_vdev, val, ISP3X_MI_LUT_CAC_RD_BASE, id);
	isp3_param_write(params_vdev, arg->hsize, ISP3X_MI_LUT_CAC_RD_H_WSIZE, id);
	isp3_param_write(params_vdev, arg->vsize, ISP3X_MI_LUT_CAC_RD_V_SIZE, id);
	if (ctrl & ISP3X_CAC_EN)
		ctrl |= ISP3X_CAC_LUT_EN | ISP39_SELF_FORCE_UPD | ISP3X_CAC_LUT_MODE(3);
end:
	isp3_param_write(params_vdev, ctrl, ISP3X_CAC_CTRL, id);
}

static void
isp_cac_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 val, ctrl = isp3_param_read(params_vdev, ISP3X_CAC_CTRL, id);

	if (en == !!(ctrl & ISP3X_CAC_EN))
		return;

	ctrl &= ~(ISP3X_CAC_EN | ISP3X_CAC_LUT_EN | ISP39_SELF_FORCE_UPD);
	if (en) {
		ctrl |= ISP3X_CAC_EN;
		val = priv_val->buf_cac_idx[id];
		if (priv_val->buf_cac[id][val].vaddr)
			ctrl |= ISP3X_CAC_LUT_EN |
				ISP39_SELF_FORCE_UPD | ISP3X_CAC_LUT_MODE(3);
	}
	isp3_param_write(params_vdev, ctrl, ISP3X_CAC_CTRL, id);
}

static void
isp_csm_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp21_csm_cfg *arg, u32 id)
{
	u32 i, val;

	for (i = 0; i < ISP39_CSM_COEFF_NUM; i++) {
		if (i == 0)
			val = (arg->csm_y_offset & 0x3f) << 24 |
			      (arg->csm_c_offset & 0xff) << 16 |
			      (arg->csm_coeff[i] & 0x1ff);
		else
			val = arg->csm_coeff[i] & 0x1ff;
		isp3_param_write(params_vdev, val, ISP3X_ISP_CC_COEFF_0 + i * 4, id);
	}

	val = isp3_param_read_cache(params_vdev, ISP3X_ISP_CTRL0, id);
	val |= CIF_ISP_CTRL_ISP_CSM_Y_FULL_ENA | CIF_ISP_CTRL_ISP_CSM_C_FULL_ENA;
	isp3_param_write(params_vdev, val, ISP3X_ISP_CTRL0, id);
}

static void
isp_cgc_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp21_cgc_cfg *arg, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_ISP_CTRL0, id);
	u32 eff_ctrl, cproc_ctrl;

	params_vdev->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	val &= ~(ISP3X_SW_CGC_YUV_LIMIT | ISP3X_SW_CGC_RATIO_EN);
	if (arg->yuv_limit) {
		val |= ISP3X_SW_CGC_YUV_LIMIT;
		params_vdev->quantization = V4L2_QUANTIZATION_LIM_RANGE;
	}
	if (arg->ratio_en)
		val |= ISP3X_SW_CGC_RATIO_EN;
	isp3_param_write(params_vdev, val, ISP3X_ISP_CTRL0, id);

	cproc_ctrl = isp3_param_read(params_vdev, ISP3X_CPROC_CTRL, id);
	if (cproc_ctrl & CIF_C_PROC_CTR_ENABLE) {
		val = CIF_C_PROC_YOUT_FULL | CIF_C_PROC_YIN_FULL | CIF_C_PROC_COUT_FULL;
		if (arg->yuv_limit)
			cproc_ctrl &= ~val;
		else
			cproc_ctrl |= val;
		isp3_param_write(params_vdev, cproc_ctrl, ISP3X_CPROC_CTRL, id);
	}

	eff_ctrl = isp3_param_read(params_vdev, ISP3X_IMG_EFF_CTRL, id);
	if (eff_ctrl & CIF_IMG_EFF_CTRL_ENABLE) {
		if (arg->yuv_limit)
			eff_ctrl &= ~CIF_IMG_EFF_CTRL_YCBCR_FULL;
		else
			eff_ctrl |= CIF_IMG_EFF_CTRL_YCBCR_FULL;
		isp3_param_write(params_vdev, eff_ctrl, ISP3X_IMG_EFF_CTRL, id);
	}
}

static void
isp_yuvme_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp39_yuvme_cfg *arg, u32 id)
{
	u32 i, value = isp3_param_read(params_vdev, ISP39_YUVME_CTRL, id);

	value &= ISP39_MODULE_EN;
	value |= !!arg->bypass << 1 | !!arg->tnr_wgt0_en << 4;
	isp3_param_write(params_vdev, value, ISP39_YUVME_CTRL, id);

	value = arg->global_nr_strg |
		arg->wgt_fact3 << 8 |
		(arg->search_range_mode & 0xf) << 16 |
		(arg->static_detect_thred & 0x3f) << 20;
	isp3_param_write(params_vdev, value, ISP39_YUVME_PARA0, id);

	value = (arg->time_relevance_offset & 0xf) |
		(arg->space_relevance_offset & 0xf) << 4 |
		arg->nr_diff_scale << 8 |
		(arg->nr_fusion_limit & 0x3ff) << 20;
	isp3_param_write(params_vdev, value, ISP39_YUVME_PARA1, id);

	value = arg->nr_static_scale | (arg->nr_motion_scale & 0x1ff) << 8 |
		(arg->nr_fusion_mode & 0x3) << 17 | (arg->cur_weight_limit & 0x7ff) << 20;
	isp3_param_write(params_vdev, value, ISP39_YUVME_PARA2, id);

	for (i = 0; i < ISP39_YUVME_SIGMA_NUM / 3; i++) {
		value = (arg->nr_luma2sigma_val[i * 3] & 0x3ff) |
			(arg->nr_luma2sigma_val[i * 3 + 1] & 0x3ff) << 10 |
			(arg->nr_luma2sigma_val[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_YUVME_SIGMA0 + i * 4, id);
	}
	value = arg->nr_luma2sigma_val[i * 3] & 0x3ff;
	isp3_param_write(params_vdev, value, ISP39_YUVME_SIGMA0 + i * 4, id);
}

static void
isp_yuvme_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_device *ispdev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	u32 value, ctrl;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	ctrl = isp3_param_read_cache(params_vdev, ISP39_YUVME_CTRL, id);
	if ((en && (ctrl & ISP39_MODULE_EN)) || (!en && !(ctrl & ISP39_MODULE_EN)))
		return;

	if (en) {
		value = isp3_param_read_cache(params_vdev, ISP3X_BAY3D_CTRL, id);
		if (!(value & ISP39_MODULE_EN)) {
			dev_err(ispdev->dev, "yuvme need bay3d enable together\n");
			return;
		}
		if (!priv_val->buf_bay3d_cur.mem_priv) {
			dev_err(ispdev->dev, "no yuvme cur buffer available\n");
			return;
		}
		value = priv_val->bay3d_cur_size;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_CUR_WR_SIZE, id);
		isp3_param_write(params_vdev, value, ISP32_MI_BAY3D_CUR_RD_SIZE, id);
		value = priv_val->buf_bay3d_cur.dma_addr + value * id;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_CUR_WR_BASE, id);
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_CUR_RD_BASE, id);

		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP39_YUVME_FST_FRAME, id);
		ctrl |= ISP39_MODULE_EN;
	} else {
		ctrl &= ~ISP39_MODULE_EN;
	}
	isp3_param_write(params_vdev, ctrl, ISP39_YUVME_CTRL, id);
}

static void
isp_ldcv_config(struct rkisp_isp_params_vdev *params_vdev,
		const struct isp39_ldcv_cfg *arg, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	struct isp2x_mesh_head *head;
	int buf_idx, i;
	u32 value;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	priv_val->ldcv_out_vsize = arg->out_vsize;

	value = isp3_param_read(params_vdev, ISP39_LDCV_CTRL, id);
	value &= (ISP39_MODULE_EN | ISP39_LDCV_OUTPUT_YUV420 | ISP39_LDCV_UV_SWAP);
	value |= !!arg->thumb_mode << 1 | !!arg->dth_bypass << 6 |
		 !!arg->force_map_en << 8 | !!arg->map13p3_en << 9;
	isp3_param_write(params_vdev, value, ISP39_LDCV_CTRL, id);

	for (i = 0; i < ISP39_LDCV_BIC_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->bicubic[i * 4], arg->bicubic[i * 4 + 1],
				       arg->bicubic[i * 4 + 2], arg->bicubic[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP39_LDCV_BIC_TABLE0 + i * 4, id);
	}

	for (i = 0; i < ISP39_MESH_BUF_NUM; i++) {
		if (!priv_val->buf_ldcv[id][i].mem_priv)
			continue;
		if (arg->buf_fd == priv_val->buf_ldcv[id][i].dma_fd)
			break;
	}
	if (i == ISP39_MESH_BUF_NUM) {
		dev_err(dev->dev, "cannot find ldcv buf fd(%d)\n", arg->buf_fd);
		return;
	}

	if (!priv_val->buf_ldcv[id][i].vaddr) {
		dev_err(dev->dev, "no ldcv buffer allocated\n");
		return;
	}

	buf_idx = priv_val->buf_ldcv_idx[id];
	head = (struct isp2x_mesh_head *)priv_val->buf_ldcv[id][buf_idx].vaddr;
	head->stat = MESH_BUF_INIT;

	buf_idx = i;
	head = (struct isp2x_mesh_head *)priv_val->buf_ldcv[id][buf_idx].vaddr;
	head->stat = MESH_BUF_CHIPINUSE;
	priv_val->buf_ldcv_idx[id] = buf_idx;
	rkisp_prepare_buffer(dev, &priv_val->buf_ldcv[id][buf_idx]);
	value = priv_val->buf_ldcv[id][buf_idx].dma_addr + head->data_oft;
	isp3_param_write(params_vdev, value, ISP32L_IRLDCV_RD_BASE, id);
	isp3_param_write(params_vdev, arg->hsize, ISP32L_IRLDCV_RD_H_WSIZE, id);
	isp3_param_write(params_vdev, arg->vsize, ISP32L_IRLDCV_RD_V_SIZE, id);
}

static void
isp_ldcv_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_stream *stream = &dev->cap_dev.stream[RKISP_STREAM_LDC];
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 value = isp3_param_read(params_vdev, ISP39_LDCV_CTRL, id);
	u32 buf_idx;

	value &= ~(ISP39_LDCV_EN | ISP39_LDCV_FORCE_UPD);
	if (en) {
		buf_idx = priv_val->buf_ldcv_idx[id];
		if (!stream->streaming || !priv_val->buf_ldcv[id][buf_idx].vaddr) {
			dev_err(dev->dev, "output stream en:%d, ldcv map buf:%p\n",
				stream->streaming, priv_val->buf_ldcv[id][buf_idx].vaddr);
			goto end;
		}
		if (stream->out_fmt.width != priv_val->ldch_out_hsize ||
		    stream->out_fmt.height != priv_val->ldcv_out_vsize) {
			dev_err(dev->dev, "stream output %dx%d, but ldch:%d ldcv:%d\n",
				stream->out_fmt.width, stream->out_fmt.height,
				priv_val->ldch_out_hsize, priv_val->ldcv_out_vsize);
			goto end;
		}
		value |= ISP39_MODULE_EN;
	}
end:
	isp3_param_write(params_vdev, value, ISP39_LDCV_CTRL, id);
}

static void
isp_rgbir_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp39_rgbir_cfg *arg, u32 id)
{
	u32 i, value;

	value = arg->coe_theta & 0xfff;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_THETA, id);

	value = arg->coe_delta & 0x3fff;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_DELTA, id);

	for (i = 0; i < ISP39_RGBIR_SCALE_NUM; i++) {
		value = arg->scale[i] & 0x1ff;
		isp3_param_write(params_vdev, value, ISP39_RGBIR_SCALE0 + i * 4, id);
	}

	for (i = 0; i < ISP39_RGBIR_LUMA_POINT_NUM / 3; i++) {
		value = (arg->luma_point[i * 3] & 0x3ff) |
			(arg->luma_point[i * 3 + 1] & 0x3ff) << 10 |
			(arg->luma_point[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_RGBIR_LUMA_POINT0 + i * 4, id);
	}
	value = (arg->luma_point[i * 3] & 0x3ff) | (arg->luma_point[i * 3 + 1] & 0x7ff) << 10;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_LUMA_POINT0 + i * 4, id);

	for (i = 0; i < ISP39_RGBIR_SCALE_MAP_NUM / 3; i++) {
		value = (arg->scale_map[i * 3] & 0x1ff) |
			(arg->scale_map[i * 3 + 1] & 0x1ff) << 9 |
			(arg->scale_map[i * 3 + 2] & 0x1ff) << 18;
		isp3_param_write(params_vdev, value, ISP39_RGBIR_SCALE_MAP0 + i * 4, id);
	}
	value = (arg->scale_map[i * 3] & 0x1ff) | (arg->scale_map[i * 3 + 1] & 0x1ff) << 9;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_SCALE_MAP0 + i * 4, id);
}

static void
isp_rgbir_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 value = 0;

	if (en)
		value = ISP39_MODULE_EN;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_CTRL, id);
}

static __maybe_unused
void __isp_isr_other_config(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp39_isp_params_cfg *new_params,
			    enum rkisp_params_type type, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	u64 module_cfg_update = new_params->module_cfg_update;

	v4l2_dbg(4, rkisp_debug, &dev->v4l2_dev,
		 "%s id:%d seq:%d type:%d module_cfg_update:0x%llx\n",
		 __func__, id, new_params->frame_id, type, module_cfg_update);

	if (module_cfg_update & ISP39_MODULE_RGBIR && type != RKISP_PARAMS_LAT)
		isp_rgbir_config(params_vdev, &new_params->others.rgbir_cfg, id);
	if (module_cfg_update & ISP39_MODULE_SDG && type != RKISP_PARAMS_LAT)
		isp_sdg_config(params_vdev, &new_params->others.sdg_cfg, id);
	if (module_cfg_update & ISP39_MODULE_BLS)
		isp_bls_config(params_vdev, &new_params->others.bls_cfg, type, id);
	if (module_cfg_update & ISP39_MODULE_AWB_GAIN)
		isp_awbgain_config(params_vdev, &new_params->others.awb_gain_cfg, type, id);
	if (module_cfg_update & ISP39_MODULE_DPCC && type != RKISP_PARAMS_LAT)
		isp_dpcc_config(params_vdev, &new_params->others.dpcc_cfg, id);
	if (module_cfg_update & ISP39_MODULE_HDRMGE && type != RKISP_PARAMS_LAT)
		isp_hdrmge_config(params_vdev, &new_params->others.hdrmge_cfg, type, id);
	if (module_cfg_update & ISP39_MODULE_GAIN && type != RKISP_PARAMS_LAT)
		isp_gain_config(params_vdev, &new_params->others.gain_cfg, id);
	if (module_cfg_update & ISP39_MODULE_BAY3D && type != RKISP_PARAMS_LAT)
		isp_bay3d_config(params_vdev, &new_params->others.bay3d_cfg, id);

	if (type == RKISP_PARAMS_IMD && dev->is_aiisp_en)
		return;

	if (module_cfg_update & ISP39_MODULE_CAC)
		isp_cac_config(params_vdev, &new_params->others.cac_cfg, id);
	if (module_cfg_update & ISP39_MODULE_LSC)
		isp_lsc_config(params_vdev, &new_params->others.lsc_cfg, id);
	if (module_cfg_update & ISP39_MODULE_DEBAYER)
		isp_debayer_config(params_vdev, &new_params->others.debayer_cfg, id);
	if (module_cfg_update & ISP39_MODULE_DRC)
		isp_hdrdrc_config(params_vdev, &new_params->others.drc_cfg, type, id);
	if (module_cfg_update & ISP39_MODULE_CCM)
		isp_ccm_config(params_vdev, &new_params->others.ccm_cfg, id);
	if (module_cfg_update & ISP39_MODULE_GOC)
		isp_goc_config(params_vdev, &new_params->others.gammaout_cfg, id);
	if (module_cfg_update & ISP39_MODULE_3DLUT)
		isp_3dlut_config(params_vdev, &new_params->others.isp3dlut_cfg, id);
	/* range csm->cgc->cproc->ie */
	if (module_cfg_update & ISP39_MODULE_CSM)
		isp_csm_config(params_vdev, &new_params->others.csm_cfg, id);
	if (module_cfg_update & ISP39_MODULE_CGC)
		isp_cgc_config(params_vdev, &new_params->others.cgc_cfg, id);
	if (module_cfg_update & ISP39_MODULE_CPROC)
		isp_cproc_config(params_vdev, &new_params->others.cproc_cfg, id);
	if (module_cfg_update & ISP39_MODULE_GIC)
		isp_gic_config(params_vdev, &new_params->others.gic_cfg, id);
	if (module_cfg_update & ISP39_MODULE_DHAZ)
		isp_dhaz_config(params_vdev, &new_params->others.dhaz_cfg, id);
	if (module_cfg_update & ISP39_MODULE_LDCH)
		isp_ldch_config(params_vdev, &new_params->others.ldch_cfg, id);
	if (module_cfg_update & ISP39_MODULE_LDCV)
		isp_ldcv_config(params_vdev, &new_params->others.ldcv_cfg, id);
	if (module_cfg_update & ISP39_MODULE_YNR)
		isp_ynr_config(params_vdev, &new_params->others.ynr_cfg, id);
	if (module_cfg_update & ISP39_MODULE_CNR)
		isp_cnr_config(params_vdev, &new_params->others.cnr_cfg, id);
	if (module_cfg_update & ISP39_MODULE_SHARP)
		isp_sharp_config(params_vdev, &new_params->others.sharp_cfg, id);
	if (module_cfg_update & ISP39_MODULE_YUVME)
		isp_yuvme_config(params_vdev, &new_params->others.yuvme_cfg, id);
}

static __maybe_unused
void __isp_isr_other_en(struct rkisp_isp_params_vdev *params_vdev,
			const struct isp39_isp_params_cfg *new_params,
			enum rkisp_params_type type, u32 id)
{
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u64 module_en_update = new_params->module_en_update;
	u64 module_ens = new_params->module_ens;
	u64 mask;
	u32 gain_ctrl, cnr_ctrl, val;

	mask = ISP39_MODULE_YNR | ISP39_MODULE_CNR | ISP39_MODULE_SHARP;
	if  ((module_ens & mask) && ((module_ens & mask) != mask))
		dev_err(params_vdev->dev->dev, "ynr cnr sharp no enable together\n");
	v4l2_dbg(4, rkisp_debug, &params_vdev->dev->v4l2_dev,
		 "%s id:%d seq:%d module_en_update:0x%llx module_ens:0x%llx\n",
		 __func__, id, new_params->frame_id, module_en_update, module_ens);

	if (module_en_update & ISP39_MODULE_DPCC)
		isp_dpcc_enable(params_vdev, !!(module_ens & ISP39_MODULE_DPCC), id);

	if (module_en_update & ISP39_MODULE_BLS)
		isp_bls_enable(params_vdev, !!(module_ens & ISP39_MODULE_BLS), id);

	if (module_en_update & ISP39_MODULE_SDG)
		isp_sdg_enable(params_vdev, !!(module_ens & ISP39_MODULE_SDG), id);

	if (module_en_update & ISP39_MODULE_LSC)
		isp_lsc_enable(params_vdev, !!(module_ens & ISP39_MODULE_LSC), id);

	if (module_en_update & ISP39_MODULE_AWB_GAIN)
		isp_awbgain_enable(params_vdev, !!(module_ens & ISP39_MODULE_AWB_GAIN), id);

	if (module_en_update & ISP39_MODULE_DEBAYER)
		isp_debayer_enable(params_vdev, !!(module_ens & ISP39_MODULE_DEBAYER), id);

	if (module_en_update & ISP39_MODULE_CCM)
		isp_ccm_enable(params_vdev, !!(module_ens & ISP39_MODULE_CCM), id);

	if (module_en_update & ISP39_MODULE_GOC)
		isp_goc_enable(params_vdev, !!(module_ens & ISP39_MODULE_GOC), id);

	if (module_en_update & ISP39_MODULE_CPROC)
		isp_cproc_enable(params_vdev, !!(module_ens & ISP39_MODULE_CPROC), id);

	if (module_en_update & ISP39_MODULE_IE)
		isp_ie_enable(params_vdev, !!(module_ens & ISP39_MODULE_IE), id);

	if (module_en_update & ISP39_MODULE_HDRMGE)
		isp_hdrmge_enable(params_vdev, !!(module_ens & ISP39_MODULE_HDRMGE), id);

	if (module_en_update & ISP39_MODULE_DRC)
		isp_hdrdrc_enable(params_vdev, !!(module_ens & ISP39_MODULE_DRC), id);

	if (module_en_update & ISP39_MODULE_GIC)
		isp_gic_enable(params_vdev, !!(module_ens & ISP39_MODULE_GIC), id);

	if (module_en_update & ISP39_MODULE_DHAZ)
		isp_dhaz_enable(params_vdev, !!(module_ens & ISP39_MODULE_DHAZ), id);

	if (module_en_update & ISP39_MODULE_3DLUT)
		isp_3dlut_enable(params_vdev, !!(module_ens & ISP39_MODULE_3DLUT), id);

	if (module_en_update & ISP39_MODULE_LDCH)
		isp_ldch_enable(params_vdev, !!(module_ens & ISP39_MODULE_LDCH), id);

	if (module_en_update & ISP39_MODULE_LDCV)
		isp_ldcv_enable(params_vdev, !!(module_ens & ISP39_MODULE_LDCV), id);

	if (module_en_update & ISP39_MODULE_YNR)
		isp_ynr_enable(params_vdev, !!(module_ens & ISP39_MODULE_YNR), id);

	if (module_en_update & ISP39_MODULE_CNR)
		isp_cnr_enable(params_vdev, !!(module_ens & ISP39_MODULE_CNR), id);

	if (module_en_update & ISP39_MODULE_SHARP)
		isp_sharp_enable(params_vdev, !!(module_ens & ISP39_MODULE_SHARP), id);

	if (module_en_update & ISP39_MODULE_BAY3D)
		isp_bay3d_enable(params_vdev, !!(module_ens & ISP39_MODULE_BAY3D), id);

	if (module_en_update & ISP39_MODULE_YUVME)
		isp_yuvme_enable(params_vdev, !!(module_ens & ISP39_MODULE_YUVME), id);

	if (module_en_update & ISP39_MODULE_RGBIR)
		isp_rgbir_enable(params_vdev, !!(module_ens & ISP39_MODULE_RGBIR), id);

	if (module_en_update & ISP39_MODULE_CAC)
		isp_cac_enable(params_vdev, !!(module_ens & ISP39_MODULE_CAC), id);

	if (module_en_update & ISP39_MODULE_GAIN ||
	    ((priv_val->buf_info_owner == RKISP_INFO2DRR_OWNER_GAIN) &&
	     !(isp3_param_read(params_vdev, ISP3X_GAIN_CTRL, id) & ISP3X_GAIN_2DDR_EN)))
		isp_gain_enable(params_vdev, !!(module_ens & ISP39_MODULE_GAIN), id);

	/* gain disable, using global gain for cnr */
	gain_ctrl = isp3_param_read_cache(params_vdev, ISP3X_GAIN_CTRL, id);
	cnr_ctrl = isp3_param_read_cache(params_vdev, ISP3X_CNR_CTRL, id);
	if (!(gain_ctrl & ISP39_MODULE_EN) && cnr_ctrl & ISP39_MODULE_EN) {
		cnr_ctrl |= BIT(1);
		isp3_param_write(params_vdev, cnr_ctrl, ISP3X_CNR_CTRL, id);
		val = isp3_param_read(params_vdev, ISP3X_CNR_EXGAIN, id) & 0x3ff;
		isp3_param_write(params_vdev, val | 0x8000, ISP3X_CNR_EXGAIN, id);
	}
}

static __maybe_unused
void __isp_isr_meas_config(struct rkisp_isp_params_vdev *params_vdev,
			   struct isp39_isp_params_cfg *new_params,
			   enum rkisp_params_type type, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	u64 module_cfg_update = new_params->module_cfg_update;

	v4l2_dbg(4, rkisp_debug, &dev->v4l2_dev,
		 "%s id:%d seq:%d type:%d module_cfg_update:0x%llx\n",
		 __func__, id, new_params->frame_id, type, module_cfg_update);

	if (!dev->is_aiisp_en || type != RKISP_PARAMS_LAT) {
		if (module_cfg_update & ISP39_MODULE_RAWAE0)
			isp_rawae0_config(params_vdev, &new_params->meas.rawae0, id);
		if (module_cfg_update & ISP39_MODULE_RAWHIST0)
			isp_rawhst0_config(params_vdev, &new_params->meas.rawhist0, id);
		if ((module_cfg_update & ISP39_MODULE_RAWAF))
			isp_rawaf_config(params_vdev, &new_params->meas.rawaf, id);
		if (dev->is_aiisp_en && type == RKISP_PARAMS_IMD) {
			params_vdev->cur_fe_frame_id = new_params->frame_id;
			return;
		}
	}
	params_vdev->cur_frame_id = new_params->frame_id;
	params_vdev->exposure = new_params->exposure;
	if (module_cfg_update & ISP39_MODULE_RAWAE3)
		isp_rawae3_config(params_vdev, &new_params->meas.rawae3, id);
	if (module_cfg_update & ISP39_MODULE_RAWHIST3)
		isp_rawhst3_config(params_vdev, &new_params->meas.rawhist3, id);
	if (module_cfg_update & ISP39_MODULE_RAWAWB)
		isp_rawawb_config(params_vdev, &new_params->meas.rawawb, id);
}

static __maybe_unused
void __isp_isr_meas_en(struct rkisp_isp_params_vdev *params_vdev,
		       struct isp39_isp_params_cfg *new_params,
		       enum rkisp_params_type type, u32 id)
{
	u64 module_en_update = new_params->module_en_update;
	u64 module_ens = new_params->module_ens;

	v4l2_dbg(4, rkisp_debug, &params_vdev->dev->v4l2_dev,
		 "%s id:%d seq:%d module_en_update:0x%llx module_ens:0x%llx\n",
		 __func__, id, new_params->frame_id, module_en_update, module_ens);

	if (module_en_update & ISP39_MODULE_RAWAF)
		isp_rawaf_enable(params_vdev, !!(module_ens & ISP39_MODULE_RAWAF), id);

	if (module_en_update & ISP39_MODULE_RAWAE0)
		isp_rawae0_enable(params_vdev, !!(module_ens & ISP39_MODULE_RAWAE0), id);

	if (module_en_update & ISP39_MODULE_RAWAE3)
		isp_rawae3_enable(params_vdev, !!(module_ens & ISP39_MODULE_RAWAE3), id);

	if (module_en_update & ISP39_MODULE_RAWHIST0)
		isp_rawhst0_enable(params_vdev, !!(module_ens & ISP39_MODULE_RAWHIST0), id);

	if (module_en_update & ISP39_MODULE_RAWHIST3)
		isp_rawhst3_enable(params_vdev, !!(module_ens & ISP39_MODULE_RAWHIST3), id);

	if (module_en_update & ISP39_MODULE_RAWAWB)
		isp_rawawb_enable(params_vdev, !!(module_ens & ISP39_MODULE_RAWAWB), id);
}

static
void rkisp_params_cfgsram_v39(struct rkisp_isp_params_vdev *params_vdev, bool is_reset)
{
	struct rkisp_device *dev = params_vdev->dev;
	u32 id = dev->unite_index;
	struct isp39_isp_params_cfg *params = params_vdev->isp39_params + id;

	if (!dev->hw_dev->is_frm_buf && is_reset)
		params->others.dhaz_cfg.hist_iir_wr = true;
	isp_dhaz_cfg_sram(params_vdev, &params->others.dhaz_cfg, true, id);
	params->others.dhaz_cfg.hist_iir_wr = false;

	isp_lsc_matrix_cfg_sram(params_vdev, &params->others.lsc_cfg, true, id);
	isp_rawhstbig_cfg_sram(params_vdev, &params->meas.rawhist0,
			       ISP3X_RAWHIST_LITE_BASE, true, id);
	isp_rawhstbig_cfg_sram(params_vdev, &params->meas.rawhist3,
			       ISP3X_RAWHIST_BIG1_BASE, true, id);
	isp_rawawb_cfg_sram(params_vdev, &params->meas.rawawb, true, id);
}

static int
rkisp_alloc_internal_buf(struct rkisp_isp_params_vdev *params_vdev,
			 const struct isp39_isp_params_cfg *new_params)
{
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	int ret, i, id;

	for (id = 0; id < dev->unite_div; id++) {
		priv_val->buf_3dlut_idx[id] = 0;
		for (i = 0; i < ISP39_3DLUT_BUF_NUM; i++) {
			if (priv_val->buf_3dlut[id][i].mem_priv)
				continue;
			priv_val->buf_3dlut[id][i].is_need_vaddr = true;
			priv_val->buf_3dlut[id][i].size = ISP39_3DLUT_BUF_SIZE;
			ret = rkisp_alloc_buffer(dev, &priv_val->buf_3dlut[id][i]);
			if (ret) {
				dev_err(dev->dev, "alloc 3dlut buf fail:%d\n", ret);
				goto err_3dlut;
			}
		}
	}

	if (dev->hw_dev->is_frm_buf && !priv_val->buf_frm.mem_priv) {
		priv_val->buf_frm.size = ISP39_FRM_BUF_SIZE;
		priv_val->buf_frm.size *= dev->unite_div;
		ret = rkisp_alloc_buffer(dev, &priv_val->buf_frm);
		if (ret) {
			dev->hw_dev->is_frm_buf = false;
			dev_err(dev->dev, "alloc frm buf fail:%d\n", ret);
			goto err_frm_buf;
		}
	}

	priv_val->buf_lsclut_idx = 0;
	for (i = 0; i < ISP39_LSC_LUT_BUF_NUM; i++) {
		priv_val->buf_lsclut[i].is_need_vaddr = true;
		priv_val->buf_lsclut[i].size = ISP39_LSC_LUT_BUF_SIZE;
		ret = rkisp_alloc_buffer(dev, &priv_val->buf_lsclut[i]);
		if (ret) {
			dev_err(dev->dev, "alloc lsc buf fail:%d\n", ret);
			goto err_lsc;
		}
	}
	return 0;
err_lsc:
	if (priv_val->buf_frm.mem_priv)
		rkisp_free_buffer(dev, &priv_val->buf_frm);
	for (i -= 1; i >= 0; i--)
		rkisp_free_buffer(dev, &priv_val->buf_lsclut[i]);
err_frm_buf:
	id = dev->unite_div - 1;
	i = ISP39_3DLUT_BUF_NUM;
err_3dlut:
	for (; id >= 0; id--) {
		for (i -= 1; i >= 0; i--)
			rkisp_free_buffer(dev, &priv_val->buf_3dlut[id][i]);
		i = ISP39_3DLUT_BUF_NUM;
	}
	return ret;
}

static bool
rkisp_params_check_bigmode_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_device *ispdev = params_vdev->dev;
	struct device *dev = params_vdev->dev->dev;
	struct rkisp_hw_dev *hw = ispdev->hw_dev;
	struct v4l2_rect *crop = &params_vdev->dev->isp_sdev.in_crop;
	u32 width = hw->max_in.w, height = hw->max_in.h, size = width * height;
	u32 bigmode_max_size, bigmode_max_w = ISP39_AUTO_BIGMODE_WIDTH;
	int k = 0, idx1[DEV_MAX] = { 0 };
	int n = 0, idx2[DEV_MAX] = { 0 };
	int i = 0, j = 0;
	bool is_bigmode = false;

	ispdev->multi_mode = 0;
	ispdev->multi_index = ispdev->dev_id;
	if (!hw->is_single)
		hw->is_frm_buf = true;
using_frm_buf:
	if (hw->is_frm_buf) {
		bigmode_max_size = ISP39_NOBIG_OVERFLOW_SIZE;
		dev_info(dev, "virtual isp%d %dx%d using frm buf\n",
			 ispdev->dev_id, crop->width, crop->height);
		goto end;
	}

	switch (hw->dev_link_num) {
	case 4:
		bigmode_max_size = ISP39_VIR4_NOBIG_OVERFLOW_SIZE;
		ispdev->multi_mode = 2;
		/* internal buf of hw divided to four parts
		 *             bigmode             nobigmode
		 *  _________  max width:4672      max width:2688
		 * |_sensor0_| max size:3840*1664  max size:3840*1664/4
		 * |_sensor1_| max size:3840*1664  max size:3840*1664/4
		 * |_sensor2_| max size:3840*1664  max size:3840*1664/4
		 * |_sensor3_| max size:3840*1664  max size:3840*1664/4
		 */
		for (i = 0; i < hw->dev_num; i++) {
			if (hw->isp_size[i].size <= ISP39_VIR4_MAX_SIZE)
				continue;
			hw->is_frm_buf = true;
			goto using_frm_buf;
		}
		break;
	case 3:
		bigmode_max_size = ISP39_VIR4_NOBIG_OVERFLOW_SIZE;
		ispdev->multi_mode = 2;
		/* case0:      bigmode             nobigmode
		 *  _________  max width:4672      max width:2688
		 * |_sensor0_| max size:3840*1664  max size:3840*1664/4
		 * |_sensor1_| max size:3840*1664  max size:3840*1664/4
		 * |_sensor2_| max size:3840*1664  max size:3840*1664/4
		 * |_________|
		 *
		 * case1:      bigmode             special reg cfg
		 *  _________  max width:4672
		 * | sensor0 | max size:4416*2900  mode=0 index=0
		 * |_________|
		 * |_sensor1_| max size:3840*1664  mode=2 index=2
		 * |_sensor2_| max size:3840*1664  mode=2 index=3
		 */
		for (i = 0; i < hw->dev_num; i++) {
			if (!hw->isp_size[i].size) {
				if (i < hw->dev_link_num)
					idx2[n++] = i;
				continue;
			}
			if (hw->isp_size[i].size <= ISP39_VIR4_MAX_SIZE)
				continue;
			idx1[k++] = i;
		}
		if (k) {
			is_bigmode = true;
			if (k != 1 ||
			    (hw->isp_size[idx1[0]].size > ISP39_VIR2_MAX_SIZE)) {
				hw->is_frm_buf = true;
				goto using_frm_buf;
			} else {
				if (idx1[0] == ispdev->dev_id) {
					ispdev->multi_mode = 0;
					ispdev->multi_index = 0;
				} else {
					ispdev->multi_mode = 2;
					if (ispdev->multi_index == 0 ||
					    ispdev->multi_index == 1)
						ispdev->multi_index = 3;
				}
			}
		} else if (ispdev->multi_index >= hw->dev_link_num) {
			ispdev->multi_index = idx2[ispdev->multi_index - hw->dev_link_num];
		}
		break;
	case 2:
		bigmode_max_size = ISP39_VIR2_NOBIG_OVERFLOW_SIZE;
		ispdev->multi_mode = 1;
		/* case0:      bigmode            nobigmode
		 *  _________  max width:4672     max width:2688
		 * | sensor0 | max size:4416*2900 max size:4416*2900/4
		 * |_________|
		 * | sensor1 | max size:4416*2900 max size:4416*2900/4
		 * |_________|
		 *
		 * case1:      bigmode              special reg cfg
		 *  _________  max width:4672
		 * | sensor0 | max size:            mode=0 index=0
		 * |         | 4416*2900+3840*1664
		 * |_________|
		 * |_sensor1_| max size:3840*1664   mode=2 index=3
		 */
		for (i = 0; i < hw->dev_num; i++) {
			if (!hw->isp_size[i].size) {
				if (i < hw->dev_link_num)
					idx2[n++] = i;
				continue;
			}
			if (hw->isp_size[i].size <= ISP39_VIR2_MAX_SIZE) {
				if (hw->isp_size[i].size > ISP39_VIR4_MAX_SIZE)
					j++;
				continue;
			}
			idx1[k++] = i;
		}
		if (k) {
			is_bigmode = true;
			if (k == 2 || j ||
			    hw->isp_size[idx1[k - 1]].size > (ISP39_VIR2_MAX_SIZE + ISP39_VIR4_MAX_SIZE)) {
				hw->is_frm_buf = true;
				goto using_frm_buf;
			} else {
				if (idx1[0] == ispdev->dev_id) {
					ispdev->multi_mode = 0;
					ispdev->multi_index = 0;
				} else {
					ispdev->multi_mode = 2;
					ispdev->multi_index = 3;
				}
			}
		} else if (ispdev->multi_index >= hw->dev_link_num) {
			ispdev->multi_index = idx2[ispdev->multi_index - hw->dev_link_num];
		}
		break;
	default:
		bigmode_max_size = ISP39_NOBIG_OVERFLOW_SIZE;
		ispdev->multi_index = 0;
		width = crop->width;
		height = crop->height;
		size = width * height;
	}

end:
	if (!is_bigmode &&
	    (width > bigmode_max_w || size > bigmode_max_size))
		is_bigmode = true;
	return ispdev->is_bigmode = is_bigmode;
}

/* Not called when the camera active, thus not isr protection. */
static void
rkisp_params_first_cfg_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_stream *stream = &dev->cap_dev.stream[RKISP_STREAM_LDC];
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	struct isp39_isp_params_cfg *params = params_vdev->isp39_params;
	unsigned long flags = 0;
	int i;

	rkisp_params_check_bigmode_v39(params_vdev);
	spin_lock_irqsave(&params_vdev->config_lock, flags);
	if (dev->is_bigmode)
		rkisp_unite_set_bits(dev, ISP3X_ISP_CTRL1, 0,
				     ISP3X_BIGMODE_MANUAL | ISP3X_BIGMODE_FORCE_EN, false);

	for (i = 0; i < dev->unite_div; i++) {
		u64 module_cfg_update = params->module_cfg_update;
		u64 module_en_update = params->module_en_update;
		u64 module_ens = params->module_ens;

		if (!module_cfg_update || !module_en_update || !module_ens)
			dev_warn(dev->dev,
				 "id:%d no first iq setting cfg_upd:%llx en_upd:%llx ens:%llx\n",
				 i, module_cfg_update, module_en_update, module_ens);

		if (stream->streaming &&
		    (!(module_en_update & ISP39_MODULE_LDCH) ||
		     !(module_en_update & ISP39_MODULE_LDCV))) {
			dev_err(dev->dev,
				"id:%d ldch or ldcv no enable for first iq\n", i);
			isp3_param_clear_bits(params_vdev, ISP3X_LDCH_STS, 0x01, i);
			isp3_param_clear_bits(params_vdev, ISP39_LDCV_CTRL, 0x01, i);
		}
		__isp_isr_meas_config(params_vdev, params, RKISP_PARAMS_ALL, i);
		__isp_isr_other_config(params_vdev, params, RKISP_PARAMS_ALL, i);
		__isp_isr_other_en(params_vdev, params, RKISP_PARAMS_ALL, i);
		__isp_isr_meas_en(params_vdev, params, RKISP_PARAMS_ALL, i);
		params++;
	}
	spin_unlock_irqrestore(&params_vdev->config_lock, flags);

	if (dev->hw_dev->is_frm_buf && priv_val->buf_frm.mem_priv) {
		u32 size = priv_val->buf_frm.size;
		u32 addr = priv_val->buf_frm.dma_addr;

		if (dev->unite_div)
			size /= dev->unite_div;
		for (i = 0; i < dev->unite_div; i++) {
			isp3_param_write(params_vdev, size, ISP32L_FRM_BUF_WR_SIZE, i);
			isp3_param_write(params_vdev, addr + size * i, ISP32L_FRM_BUF_WR_BASE, i);
			isp3_param_write(params_vdev, addr + size * i, ISP32L_FRM_BUF_RD_BASE, i);
		}
	}
	if (dev->hw_dev->is_single && (dev->isp_state & ISP_START)) {
		rkisp_set_bits(dev, ISP3X_ISP_CTRL0, 0, CIF_ISP_CTRL_ISP_CFG_UPD, true);
		rkisp_clear_reg_cache_bits(dev, ISP3X_ISP_CTRL0, CIF_ISP_CTRL_ISP_CFG_UPD);
	}
}

static void rkisp_save_first_param_v39(struct rkisp_isp_params_vdev *params_vdev, void *param)
{
	memcpy(params_vdev->isp39_params, param, params_vdev->vdev_fmt.fmt.meta.buffersize);
	rkisp_alloc_internal_buf(params_vdev, params_vdev->isp39_params);
}

static void rkisp_clear_first_param_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	u32 mult = params_vdev->dev->hw_dev->unite ? ISP_UNITE_MAX : 1;
	u32 size = sizeof(struct isp39_isp_params_cfg) * mult;

	memset(params_vdev->isp39_params, 0, size);
}

static void rkisp_deinit_mesh_buf(struct rkisp_isp_params_vdev *params_vdev,
				  u64 module_id, u32 id)
{
	struct rkisp_isp_params_val_v39 *priv_val;
	struct rkisp_dummy_buffer *buf;
	int i;

	priv_val = params_vdev->priv_val;
	if (!priv_val)
		return;

	switch (module_id) {
	case ISP39_MODULE_CAC:
		buf = priv_val->buf_cac[id];
		break;
	case ISP39_MODULE_LDCV:
		buf = priv_val->buf_ldcv[id];
		break;
	case ISP39_MODULE_LDCH:
	default:
		buf = priv_val->buf_ldch[id];
		break;
	}

	for (i = 0; i < ISP39_MESH_BUF_NUM; i++)
		rkisp_free_buffer(params_vdev->dev, buf + i);
}

static int rkisp_init_mesh_buf(struct rkisp_isp_params_vdev *params_vdev,
			       struct rkisp_meshbuf_size *meshsize)
{
	struct rkisp_device *ispdev = params_vdev->dev;
	struct device *dev = ispdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	struct isp2x_mesh_head *mesh_head;
	struct rkisp_dummy_buffer *buf;
	u32 mesh_w = meshsize->meas_width;
	u32 mesh_h = meshsize->meas_height;
	u32 mesh_size, buf_size;
	int i, ret, buf_cnt = meshsize->buf_cnt;
	int id = meshsize->unite_isp_id;
	bool is_alloc;

	priv_val = params_vdev->priv_val;
	if (!priv_val) {
		dev_err(dev, "priv_val is NULL\n");
		return -EINVAL;
	}

	switch (meshsize->module_id) {
	case ISP39_MODULE_CAC:
		priv_val->buf_cac_idx[id] = 0;
		buf = priv_val->buf_cac[id];
		mesh_w = (mesh_w + 62) / 64 * 9;
		mesh_h = (mesh_h + 62) / 64 * 2;
		mesh_size = mesh_w * 4 * mesh_h;
		break;
	case ISP39_MODULE_LDCV:
		priv_val->buf_ldcv_idx[id] = 0;
		buf = priv_val->buf_ldcv[id];
		mesh_w = (((mesh_w + 15) / 16 + 1) + 1) / 2 * 2;
		mesh_h = (mesh_h + 7) / 8 + 1;
		mesh_size = (mesh_w * mesh_h + 3) / 4 * 4 * 2;
		break;
	case ISP39_MODULE_LDCH:
	default:
		priv_val->buf_ldch_idx[id] = 0;
		buf = priv_val->buf_ldch[id];
		mesh_w = ((mesh_w + 15) / 16 + 2) / 2;
		mesh_h = (mesh_h + 7) / 8 + 1;
		mesh_size = mesh_w * 4 * mesh_h;
		break;
	}

	if (buf_cnt <= 0 || buf_cnt > ISP39_MESH_BUF_NUM)
		buf_cnt = ISP39_MESH_BUF_NUM;
	buf_size = PAGE_ALIGN(mesh_size + ALIGN(sizeof(struct isp2x_mesh_head), 16));
	for (i = 0; i < buf_cnt; i++) {
		buf->is_need_vaddr = true;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		is_alloc = true;
		if (buf->mem_priv) {
			if (buf_size > buf->size) {
				rkisp_free_buffer(params_vdev->dev, buf);
			} else {
				is_alloc = false;
				buf->dma_fd = dma_buf_fd(buf->dbuf, O_CLOEXEC);
				if (buf->dma_fd < 0)
					goto err;
			}
		}
		if (is_alloc) {
			buf->size = buf_size;
			ret = rkisp_alloc_buffer(params_vdev->dev, buf);
			if (ret) {
				dev_err(dev, "%s failed\n", __func__);
				goto err;
			}
			mesh_head = (struct isp2x_mesh_head *)buf->vaddr;
			mesh_head->stat = MESH_BUF_INIT;
			mesh_head->data_oft = ALIGN(sizeof(struct isp2x_mesh_head), 16);
		}
		buf++;
	}

	return 0;
err:
	rkisp_deinit_mesh_buf(params_vdev, meshsize->module_id, id);
	return -ENOMEM;
}

static void
rkisp_get_param_size_v39(struct rkisp_isp_params_vdev *params_vdev,
			 unsigned int sizes[])
{
	u32 mult = params_vdev->dev->unite_div;

	sizes[0] = sizeof(struct isp39_isp_params_cfg) * mult;
	params_vdev->vdev_fmt.fmt.meta.buffersize = sizes[0];
}

static void
rkisp_params_get_meshbuf_inf_v39(struct rkisp_isp_params_vdev *params_vdev,
				 void *meshbuf_inf)
{
	struct rkisp_isp_params_val_v39 *priv_val;
	struct rkisp_meshbuf_info *meshbuf = meshbuf_inf;
	struct rkisp_dummy_buffer *buf;
	int i, id = meshbuf->unite_isp_id;

	priv_val = params_vdev->priv_val;
	switch (meshbuf->module_id) {
	case ISP39_MODULE_CAC:
		priv_val->buf_cac_idx[id] = 0;
		buf = priv_val->buf_cac[id];
		break;
	case ISP39_MODULE_LDCV:
		priv_val->buf_ldcv_idx[id] = 0;
		buf = priv_val->buf_ldcv[id];
		break;
	case ISP39_MODULE_LDCH:
	default:
		priv_val->buf_ldch_idx[id] = 0;
		buf = priv_val->buf_ldch[id];
		break;
	}

	for (i = 0; i < ISP39_MESH_BUF_NUM; i++) {
		if (!buf->mem_priv) {
			meshbuf->buf_fd[i] = -1;
			meshbuf->buf_size[i] = 0;
		} else {
			meshbuf->buf_fd[i] = buf->dma_fd;
			meshbuf->buf_size[i] = buf->size;
		}
		buf++;
	}
}

static int
rkisp_params_set_meshbuf_size_v39(struct rkisp_isp_params_vdev *params_vdev,
				  void *size)
{
	struct rkisp_meshbuf_size *meshsize = size;

	if (!params_vdev->dev->hw_dev->unite)
		meshsize->unite_isp_id = 0;
	return rkisp_init_mesh_buf(params_vdev, meshsize);
}

static void
rkisp_params_free_meshbuf_v39(struct rkisp_isp_params_vdev *params_vdev,
			      u64 module_id)
{
	int id;

	for (id = 0; id < params_vdev->dev->unite_div; id++)
		rkisp_deinit_mesh_buf(params_vdev, module_id, id);
}

static int
rkisp_params_info2ddr_cfg_v39(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	struct rkisp_info2ddr *cfg = arg;
	struct rkisp_dummy_buffer *buf;
	u32 reg, ctrl, mask, size, val, wsize = 0, vsize = 0;
	int i, ret;

	if (dev->is_aiisp_en) {
		dev_err(dev->dev, "%s no support for aiisp enable\n", __func__);
		return -EINVAL;
	}
	priv_val = params_vdev->priv_val;

	if (cfg->buf_cnt > RKISP_INFO2DDR_BUF_MAX)
		cfg->buf_cnt = RKISP_INFO2DDR_BUF_MAX;
	else if (cfg->buf_cnt == 0)
		cfg->buf_cnt = 1;
	for (val = 0; val < cfg->buf_cnt; val++)
		cfg->buf_fd[val] = -1;

	switch (cfg->owner) {
	case RKISP_INFO2DRR_OWNER_NULL:
		rkisp_clear_reg_cache_bits(dev, ISP3X_RAWAWB_CTRL,
					   ISP32_RAWAWB_2DDR_PATH_EN);
		rkisp_clear_reg_cache_bits(dev, ISP3X_GAIN_CTRL,
					   ISP3X_GAIN_2DDR_EN);
		priv_val->buf_info_owner = cfg->owner;
		return 0;
	case RKISP_INFO2DRR_OWNER_GAIN:
		ctrl = ISP3X_GAIN_2DDR_MODE(cfg->u.gain.gain2ddr_mode);
		ctrl |= ISP3X_GAIN_2DDR_EN;
		mask = ISP3X_GAIN_2DDR_MODE(3);
		reg = ISP3X_GAIN_CTRL;

		if (cfg->wsize)
			wsize = (cfg->wsize + 7) / 8;
		else
			wsize = (dev->isp_sdev.in_crop.width + 7) / 8;
		/* 0 or 3: 4x8mode, 1: 2x8 mode, 2: 1x8mode */
		val = cfg->u.gain.gain2ddr_mode;
		val = (val == 1) ? 2 : ((val == 2) ? 1 : 4);
		if (cfg->vsize)
			vsize = cfg->vsize;
		else
			vsize = dev->isp_sdev.in_crop.height / val;
		break;
	case RKISP_INFO2DRR_OWNER_AWB:
		ctrl = cfg->u.awb.awb2ddr_sel ? ISP32_RAWAWB_2DDR_PATH_DS : 0;
		ctrl |= ISP32_RAWAWB_2DDR_PATH_EN;
		mask = ISP32_RAWAWB_2DDR_PATH_DS;
		reg = ISP3X_RAWAWB_CTRL;

		val = cfg->u.awb.awb2ddr_sel ? 8 : 1;
		if (cfg->wsize)
			wsize = cfg->wsize;
		else
			wsize = dev->isp_sdev.in_crop.width * 4 / val;
		if (cfg->vsize)
			vsize = cfg->vsize;
		else
			vsize = dev->isp_sdev.in_crop.height / val;
		break;
	default:
		dev_err(dev->dev, "%s no support owner:%d\n", __func__, cfg->owner);
		return -EINVAL;
	}

	if (!wsize || !vsize) {
		dev_err(dev->dev, "%s inval wsize:%d vsize:%d\n", __func__, wsize, vsize);
		return -EINVAL;
	}

	wsize = ALIGN(wsize, 16);
	size = wsize * vsize;
	for (i = 0; i < cfg->buf_cnt; i++) {
		buf = &priv_val->buf_info[i];
		if (buf->mem_priv)
			rkisp_free_buffer(dev, buf);
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		buf->is_need_vaddr = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "%s alloc buf failed\n", __func__);
			goto err;
		}
		*(u32 *)buf->vaddr = RKISP_INFO2DDR_BUF_INIT;
		cfg->buf_fd[i] = buf->dma_fd;
	}
	buf = &priv_val->buf_info[0];
	isp3_param_write(params_vdev, buf->dma_addr, ISP3X_MI_GAIN_WR_BASE, 0);
	isp3_param_write(params_vdev, buf->size, ISP3X_MI_GAIN_WR_SIZE, 0);
	isp3_param_write(params_vdev, wsize, ISP3X_MI_GAIN_WR_LENGTH, 0);
	if (dev->hw_dev->is_single)
		rkisp_write(dev, ISP3X_MI_WR_CTRL2, ISP3X_GAINSELF_UPD, true);
	rkisp_set_reg_cache_bits(dev, reg, mask, ctrl);

	priv_val->buf_info_idx = 0;
	priv_val->buf_info_cnt = cfg->buf_cnt;
	priv_val->buf_info_owner = cfg->owner;

	cfg->wsize = wsize;
	cfg->vsize = vsize;
	return 0;
err:
	for (i -= 1; i >= 0; i--) {
		buf = &priv_val->buf_info[i];
		rkisp_free_buffer(dev, buf);
		cfg->buf_fd[i] = -1;
	}
	cfg->owner = RKISP_INFO2DRR_OWNER_NULL;
	cfg->buf_cnt = 0;
	return -ENOMEM;
}

static int
rkisp_params_init_bnr_buf_v39(struct rkisp_isp_params_vdev *params_vdev,
			      struct rkisp_bnr_buf_info *bnrbuf)
{
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_subdev *isp_sdev = &dev->isp_sdev;
	struct rkisp_dummy_buffer *buf;
	bool iirsparse_en = !!bnrbuf->u.v39.iirsparse_en;
	u32 w = ALIGN(isp_sdev->in_crop.width, 16);
	u32 h = isp_sdev->in_crop.height;
	int ret, i, cnt, size;

	INIT_LIST_HEAD(&priv_val->iir_list);
	INIT_LIST_HEAD(&priv_val->gain_list);
	if (dev->unite_div > ISP_UNITE_DIV1)
		w = ALIGN(isp_sdev->in_crop.width / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL, 16);
	if (dev->unite_div == ISP_UNITE_DIV4)
		h = ALIGN(isp_sdev->in_crop.height / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL, 16);
	if (!iirsparse_en) {
		w = w * 3 / 2 + w / 4;
		h /= 2;
	}
	size = ALIGN(w * h * 2, 16);
	priv_val->bay3d_iir_size = size;
	if (dev->unite_div > ISP_UNITE_DIV1)
		size *= dev->unite_div;
	cnt = bnrbuf->iir.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv_val->buf_bay3d_iir[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc bay3d iir buf%d fail:%d\n", i, ret);
			goto err_iir;
		}
		if (!i)
			priv_val->pbuf_bay3d_iir = buf;
		else
			list_add_tail(&buf->queue, &priv_val->iir_list);
		buf->index = i;
		bnrbuf->iir.buf_fd[i] = buf->dma_fd;
	}
	priv_val->bay3d_iir_cnt = cnt;
	bnrbuf->iir.buf_cnt = cnt;
	bnrbuf->iir.buf_size = size;

	cnt = bnrbuf->u.v39.aiisp.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv_val->buf_aiisp[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc aiisp buf%d fail:%d\n", i, ret);
			goto err_aiisp;
		}
		buf->index = i;
		bnrbuf->u.v39.aiisp.buf_fd[i] = buf->dma_fd;
	}
	priv_val->aiisp_cnt = cnt;
	bnrbuf->u.v39.aiisp.buf_cnt = cnt;
	bnrbuf->u.v39.aiisp.buf_size = size;

	size = ALIGN(w * h / 4, 16);
	priv_val->gain_size = size;
	if (dev->unite_div > ISP_UNITE_DIV1)
		size *= dev->unite_div;
	cnt = bnrbuf->u.v39.gain.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv_val->buf_gain[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc gain buf%d fail:%d\n", i, ret);
			goto err_gain;
		}
		if (!i)
			priv_val->pbuf_gain_wr = buf;
		else
			list_add_tail(&buf->queue, &priv_val->gain_list);
		buf->index = i;
		bnrbuf->u.v39.gain.buf_fd[i] = buf->dma_fd;
	}
	priv_val->gain_cnt = cnt;
	bnrbuf->u.v39.gain.buf_cnt = cnt;
	bnrbuf->u.v39.gain.buf_size = size;

	return 0;
err_gain:
	for (i -= 1; i >= 0; i--) {
		buf = &priv_val->buf_gain[i];
		rkisp_free_buffer(dev, buf);
	}
	priv_val->gain_cnt = 0;
	bnrbuf->u.v39.gain.buf_cnt = 0;
	bnrbuf->u.v39.gain.buf_size = 0;

	i = priv_val->aiisp_cnt;
err_aiisp:
	for (i -= 1; i >= 0; i--) {
		buf = &priv_val->buf_aiisp[i];
		rkisp_free_buffer(dev, buf);
	}
	priv_val->aiisp_cnt = 0;
	bnrbuf->u.v39.aiisp.buf_cnt = 0;
	bnrbuf->u.v39.aiisp.buf_size = 0;

	i = priv_val->bay3d_iir_cnt;
err_iir:
	for (i -= 1; i >= 0; i--) {
		buf = &priv_val->buf_bay3d_iir[i];
		rkisp_free_buffer(dev, buf);
	}
	priv_val->bay3d_iir_cnt = 0;
	bnrbuf->iir.buf_cnt = 0;
	bnrbuf->iir.buf_size = 0;
	return ret;
}

static void
rkisp_params_stream_stop_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_device *ispdev = params_vdev->dev;
	struct rkisp_isp_params_val_v39 *priv_val;
	int i, id;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	rkisp_free_buffer(ispdev, &priv_val->buf_frm);
	for (i = 0; i < priv_val->gain_cnt; i++)
		rkisp_free_buffer(ispdev, &priv_val->buf_gain[i]);
	priv_val->gain_cnt = 0;
	priv_val->gain_cur_idx = -1;
	for (i = 0; i < priv_val->aiisp_cnt; i++)
		rkisp_free_buffer(ispdev, &priv_val->buf_aiisp[i]);
	priv_val->aiisp_cnt = 0;
	priv_val->aiisp_cur_idx = -1;
	for (i = 0; i < priv_val->bay3d_iir_cnt; i++)
		rkisp_free_buffer(ispdev, &priv_val->buf_bay3d_iir[i]);
	priv_val->bay3d_iir_cnt = 0;
	priv_val->bay3d_iir_idx = -1;
	priv_val->bay3d_iir_cur_idx = -1;
	for (i = 0; i < ISP39_LSC_LUT_BUF_NUM; i++)
		rkisp_free_buffer(ispdev, &priv_val->buf_lsclut[i]);
	for (i = 0; i < RKISP_STATS_DDR_BUF_NUM; i++)
		rkisp_free_buffer(ispdev, &ispdev->stats_vdev.stats_buf[i]);
	for (id = 0; id < ispdev->unite_div; id++) {
		for (i = 0; i < ISP39_3DLUT_BUF_NUM; i++)
			rkisp_free_buffer(ispdev, &priv_val->buf_3dlut[id][i]);
	}
	priv_val->buf_info_owner = 0;
	priv_val->buf_info_cnt = 0;
	priv_val->buf_info_idx = -1;
	for (i = 0; i < RKISP_INFO2DDR_BUF_MAX; i++)
		rkisp_free_buffer(ispdev, &priv_val->buf_info[i]);
}

static void
rkisp_params_fop_release_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	int id;

	for (id = 0; id < params_vdev->dev->unite_div; id++) {
		rkisp_deinit_mesh_buf(params_vdev, ISP39_MODULE_LDCH, id);
		rkisp_deinit_mesh_buf(params_vdev, ISP39_MODULE_LDCV, id);
		rkisp_deinit_mesh_buf(params_vdev, ISP39_MODULE_CAC, id);
	}
}

/* Not called when the camera active, thus not isr protection. */
static void
rkisp_params_disable_isp_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	int i;

	params_vdev->isp39_params->module_ens = 0;
	params_vdev->isp39_params->module_en_update = ~ISP39_MODULE_FORCE;

	for (i = 0; i < params_vdev->dev->unite_div; i++) {
		__isp_isr_other_en(params_vdev, params_vdev->isp39_params, RKISP_PARAMS_ALL, i);
		__isp_isr_meas_en(params_vdev, params_vdev->isp39_params, RKISP_PARAMS_ALL, i);
	}
}

static void
module_data_abandon(struct rkisp_isp_params_vdev *params_vdev,
		    struct isp39_isp_params_cfg *params, u32 id)
{
	struct rkisp_isp_params_val_v39 *priv_val;
	struct isp2x_mesh_head *mesh_head;
	int i;

	priv_val = (struct rkisp_isp_params_val_v39 *)params_vdev->priv_val;
	if (params->module_cfg_update & ISP39_MODULE_LDCH) {
		const struct isp39_ldch_cfg *arg = &params->others.ldch_cfg;

		for (i = 0; i < ISP39_MESH_BUF_NUM; i++) {
			if (priv_val->buf_ldch[id][i].vaddr &&
			    arg->buf_fd == priv_val->buf_ldch[id][i].dma_fd) {
				mesh_head = priv_val->buf_ldch[id][i].vaddr;
				mesh_head->stat = MESH_BUF_CHIPINUSE;
				break;
			}
		}
	}

	if (params->module_cfg_update & ISP39_MODULE_LDCV) {
		const struct isp39_ldcv_cfg *arg = &params->others.ldcv_cfg;

		for (i = 0; i < ISP39_MESH_BUF_NUM; i++) {
			if (priv_val->buf_ldcv[id][i].vaddr &&
			    arg->buf_fd == priv_val->buf_ldcv[id][i].dma_fd) {
				mesh_head = priv_val->buf_ldcv[id][i].vaddr;
				mesh_head->stat = MESH_BUF_CHIPINUSE;
				break;
			}
		}
	}

	if (params->module_cfg_update & ISP39_MODULE_CAC) {
		const struct isp32_cac_cfg *arg = &params->others.cac_cfg;

		for (i = 0; i < ISP39_MESH_BUF_NUM; i++) {
			if (priv_val->buf_cac[id][i].vaddr &&
			    arg->buf_fd == priv_val->buf_cac[id][i].dma_fd) {
				mesh_head = priv_val->buf_cac[id][i].vaddr;
				mesh_head->stat = MESH_BUF_CHIPINUSE;
				break;
			}
		}
	}
}

static void
rkisp_params_cfg_latter_v39(struct rkisp_isp_params_vdev *params_vdev, u32 frame_id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct isp39_isp_params_cfg *new_params = NULL;
	struct rkisp_buffer *cur_buf = NULL;
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&params_vdev->config_lock, flags);
	if (!params_vdev->streamon)
		goto unlock;

	/* get buffer by frame_id */
	while (!list_empty(&params_vdev->params_be)) {
		cur_buf = list_first_entry(&params_vdev->params_be, struct rkisp_buffer, queue);
		new_params = cur_buf->vaddr[0];
		if (new_params->frame_id < frame_id) {
			list_del(&cur_buf->queue);
			for (i = 0; i < dev->unite_div; i++) {
				/* update en immediately */
				if (new_params->module_en_update ||
				    (new_params->module_cfg_update & ISP39_MODULE_FORCE)) {
					__isp_isr_meas_config(params_vdev,
							      new_params, RKISP_PARAMS_LAT, i);
					__isp_isr_other_config(params_vdev,
							       new_params, RKISP_PARAMS_LAT, i);
					__isp_isr_other_en(params_vdev,
							   new_params, RKISP_PARAMS_LAT, i);
					__isp_isr_meas_en(params_vdev,
							  new_params, RKISP_PARAMS_LAT, i);
					new_params->module_cfg_update = 0;
				}
				if (new_params->module_cfg_update &
				    (ISP39_MODULE_LDCH | ISP39_MODULE_CAC | ISP39_MODULE_LDCV))
					module_data_abandon(params_vdev, new_params, i);
				new_params++;
			}
			vb2_buffer_done(&cur_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
			cur_buf = NULL;
			continue;
		} else if (new_params->frame_id == frame_id) {
			list_del(&cur_buf->queue);
		} else {
			cur_buf = NULL;
		}
		break;
	}

	if (!cur_buf)
		goto unlock;

	new_params = cur_buf->vaddr[0];
	for (i = 0; i < dev->unite_div; i++) {
		__isp_isr_meas_config(params_vdev, new_params, RKISP_PARAMS_LAT, i);
		__isp_isr_other_config(params_vdev, new_params, RKISP_PARAMS_LAT, i);
		__isp_isr_other_en(params_vdev, new_params, RKISP_PARAMS_LAT, i);
		__isp_isr_meas_en(params_vdev, new_params, RKISP_PARAMS_LAT, i);
		new_params->module_cfg_update = 0;
		new_params++;
	}
	vb2_buffer_done(&cur_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
unlock:
	spin_unlock_irqrestore(&params_vdev->config_lock, flags);
}

static void
rkisp_params_cfg_v39(struct rkisp_isp_params_vdev *params_vdev,
		     u32 frame_id, enum rkisp_params_type type)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct isp39_isp_params_cfg *params_rec, *new_params = NULL;
	struct rkisp_buffer *cur_buf = NULL;
	unsigned long flags = 0;
	int i;

	if (type == RKISP_PARAMS_LAT) {
		rkisp_params_cfg_latter_v39(params_vdev, frame_id);
		return;
	}

	spin_lock_irqsave(&params_vdev->config_lock, flags);
	if (!params_vdev->streamon)
		goto unlock;

	/* get buffer by frame_id */
	while (!list_empty(&params_vdev->params)) {
		cur_buf = list_first_entry(&params_vdev->params, struct rkisp_buffer, queue);
		new_params = cur_buf->vaddr[0];
		if (new_params->frame_id < frame_id) {
			list_del(&cur_buf->queue);
			for (i = 0; i < dev->unite_div; i++) {
				/* update en immediately */
				if (new_params->module_en_update ||
				    (new_params->module_cfg_update & ISP39_MODULE_FORCE)) {
					if (!dev->is_aiisp_en)
						type = RKISP_PARAMS_ALL;
					__isp_isr_meas_config(params_vdev, new_params, type, i);
					__isp_isr_other_config(params_vdev, new_params, type, i);
					__isp_isr_other_en(params_vdev, new_params, type, i);
					__isp_isr_meas_en(params_vdev, new_params, type, i);
					if (!dev->is_aiisp_en)
						new_params->module_cfg_update = 0;
				}
				if (!dev->is_aiisp_en &&
				    (new_params->module_cfg_update &
				     (ISP39_MODULE_LDCH | ISP39_MODULE_CAC | ISP39_MODULE_LDCV)))
					module_data_abandon(params_vdev, new_params, i);
				new_params++;
			}
			if (!dev->is_aiisp_en)
				vb2_buffer_done(&cur_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
			else
				list_add_tail(&cur_buf->queue, &params_vdev->params_be);
			cur_buf = NULL;
			continue;
		} else if (new_params->frame_id == frame_id) {
			list_del(&cur_buf->queue);
		} else {
			cur_buf = NULL;
		}
		break;
	}

	if (!cur_buf)
		goto unlock;

	params_rec = params_vdev->isp39_params;
	new_params = cur_buf->vaddr[0];
	for (i = 0; i < dev->unite_div; i++) {
		__isp_isr_meas_config(params_vdev, new_params, type, i);
		__isp_isr_other_config(params_vdev, new_params, type, i);
		__isp_isr_other_en(params_vdev, new_params, type, i);
		__isp_isr_meas_en(params_vdev, new_params, type, i);
		if (IS_HDR_RDBK(dev->rd_mode) && !dev->is_aiisp_en) {
			params_rec->others.hdrmge_cfg = new_params->others.hdrmge_cfg;
			params_rec->others.drc_cfg = new_params->others.drc_cfg;
			params_rec->module_cfg_update = new_params->module_cfg_update;
			params_rec++;
		}
		if (!dev->is_aiisp_en)
			new_params->module_cfg_update = 0;
		new_params++;
	}
	if (!dev->is_aiisp_en)
		vb2_buffer_done(&cur_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	else
		list_add_tail(&cur_buf->queue, &params_vdev->params_be);
unlock:
	spin_unlock_irqrestore(&params_vdev->config_lock, flags);
}

static void
rkisp_params_clear_fstflg(struct rkisp_isp_params_vdev *params_vdev)
{
	u32 value = isp3_param_read(params_vdev, ISP3X_ISP_CTRL1, 0);
	int i;

	value &= (ISP3X_YNR_FST_FRAME | ISP3X_ADRC_FST_FRAME |
		  ISP3X_DHAZ_FST_FRAME | ISP3X_CNR_FST_FRAME |
		  ISP3X_RAW3D_FST_FRAME | ISP39_YUVME_FST_FRAME |
		  ISP32_SHP_FST_FRAME);
	for (i = 0; i < params_vdev->dev->unite_div && value; i++)
		isp3_param_clear_bits(params_vdev, ISP3X_ISP_CTRL1, value, i);
}

static void
rkisp_params_aiisp_update_buf(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_isp_params_val_v39 *priv = params_vdev->priv_val;
	unsigned long lock_flags = 0;
	u32 val;

	spin_lock_irqsave(&priv->buf_lock, lock_flags);
	val = isp3_param_read_cache(params_vdev, ISP3X_MI_BAY3D_IIR_WR_BASE, 0);
	isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_IIR_RD_BASE, 0);
	priv->pbuf_bay3d_iir = NULL;
	if (!list_empty(&priv->iir_list)) {
		priv->pbuf_bay3d_iir = list_first_entry(&priv->iir_list,
					struct rkisp_dummy_buffer, queue);
		list_del(&priv->pbuf_bay3d_iir->queue);

		val = priv->pbuf_bay3d_iir->dma_addr;
		isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_IIR_WR_BASE, 0);
		priv->bay3d_iir_cur_idx = priv->pbuf_bay3d_iir->index;
	}

	priv->pbuf_gain_wr = NULL;
	if (!list_empty(&priv->gain_list) && priv->pbuf_bay3d_iir) {
		priv->pbuf_gain_wr = list_first_entry(&priv->gain_list,
					struct rkisp_dummy_buffer, queue);
		list_del(&priv->pbuf_gain_wr->queue);

		val = priv->pbuf_gain_wr->dma_addr;
		isp3_param_write(params_vdev, val, ISP3X_MI_GAIN_WR_BASE, 0);
		priv->gain_cur_idx = priv->pbuf_gain_wr->index;
	}
	if (params_vdev->dev->hw_dev->is_single) {
		val = ISP3X_BAY3D_IIRSELF_UPD | ISP3X_BAY3D_RDSELF_UPD | ISP3X_GAINSELF_UPD;
		isp3_param_set_bits(params_vdev, MI_WR_CTRL2, val, 0);
	}
	v4l2_dbg(3, rkisp_debug, &params_vdev->dev->v4l2_dev,
		 "%s %x:%x %x:%x %x:%x, iir:%x gain:%x\n", __func__,
		 ISP3X_MI_BAY3D_IIR_WR_BASE_SHD,
		 isp3_param_read_direct(params_vdev, ISP3X_MI_BAY3D_IIR_WR_BASE_SHD),
		 ISP3X_MI_BAY3D_IIR_RD_BASE_SHD,
		 isp3_param_read_direct(params_vdev, ISP3X_MI_BAY3D_IIR_RD_BASE_SHD),
		 ISP3X_MI_GAIN_WR_BASE_SHD,
		 isp3_param_read_direct(params_vdev, ISP3X_MI_GAIN_WR_BASE_SHD),
		 priv->pbuf_bay3d_iir ? (u32)priv->pbuf_bay3d_iir->dma_addr : 0,
		 priv->pbuf_gain_wr ? (u32)priv->pbuf_gain_wr->dma_addr : 0);
	if (!priv->pbuf_gain_wr && priv->pbuf_bay3d_iir) {
		list_add_tail(&priv->pbuf_bay3d_iir->queue, &priv->iir_list);
		priv->pbuf_bay3d_iir = NULL;
	}
	spin_unlock_irqrestore(&priv->buf_lock, lock_flags);
}

static void
rkisp_params_aiisp_event_v39(struct rkisp_isp_params_vdev *params_vdev, u32 irq)
{
	struct rkisp_isp_params_val_v39 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_dummy_buffer *buf = NULL;
	struct v4l2_event ev = { 0 };
	struct rkisp_aiisp_ev_info *ev_info;
	unsigned long lock_flags = 0;
	u32 h = dev->isp_sdev.out_crop.height;
	u32 val, wr_line, rd_line;

	if (sizeof(*ev_info) > sizeof(ev.u)) {
		v4l2_err(&dev->v4l2_dev, "aiisp_ev_info too large\n");
		return;
	}
	ev.type = RKISP_V4L2_EVENT_AIISP_LINECNT;
	ev_info = (struct rkisp_aiisp_ev_info *)ev.u.data;
	ev_info->iir_index = -1;
	ev_info->gain_index = -1;
	ev_info->aiisp_index = -1;
	val = rkisp_read(dev, ISP39_AIISP_LINE_CNT, false);
	if (irq & ISP3X_OUT_FRM_QUARTER) {
		rd_line = ISP39_AIISP_RD_LINECNT(val);
		ev.id = RKISP_AIISP_RD_LINECNT_ID;
		ev_info->height = !rd_line ? h : rd_line;
		rkisp_dmarx_get_frame(dev, &ev_info->sequence, NULL, &ev_info->timestamp, false);
		spin_lock_irqsave(&priv->buf_lock, lock_flags);
		if (priv->pbuf_aiisp) {
			ev_info->aiisp_index = priv->pbuf_aiisp->index;
			priv->pbuf_aiisp = NULL;
		}
		if (priv->pbuf_gain_rd) {
			list_add_tail(&priv->pbuf_gain_rd->queue, &priv->gain_list);
			priv->pbuf_gain_rd = NULL;
		}
		spin_unlock_irqrestore(&priv->buf_lock, lock_flags);
		v4l2_event_queue(dev->isp_sdev.sd.devnode, &ev);
	} else {
		wr_line = ISP39_AIISP_WR_LINECNT(val);
		ev.id = RKISP_AIISP_WR_LINECNT_ID;
		ev_info->height = !wr_line ? h : wr_line;
		rkisp_dmarx_get_frame(dev, &ev_info->sequence, NULL, &ev_info->timestamp, true);

		buf = priv->pbuf_bay3d_iir;
		if (buf)
			ev_info->iir_index = buf->index;
		buf = priv->pbuf_gain_wr;
		if (buf) {
			ev_info->gain_index = buf->index;
			v4l2_event_queue(dev->isp_sdev.sd.devnode, &ev);
		}
	}
	v4l2_dbg(3, rkisp_debug, &dev->v4l2_dev,
		 "%s seq:%d height:%d idx(iir:%d gain:%d aiisp:%d)\n",
		 ev.id ? "isp_be" : "isp_fe", ev_info->sequence, ev_info->height,
		 ev_info->iir_index, ev_info->gain_index, ev_info->aiisp_index);
}

static int
rkisp_params_aiisp_start_v39(struct rkisp_isp_params_vdev *params_vdev,
			     struct rkisp_aiisp_st *st)
{
	struct rkisp_isp_params_val_v39 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_dummy_buffer *buf, *buf_tmp;
	unsigned long lock_flags = 0;
	u32 val, aiisp_rd, seq = st->sequence;

	if (!dev->is_aiisp_en)
		return -EINVAL;
	if (st->gain_index < 0 || st->gain_index >= priv->gain_cnt ||
	    st->iir_index < 0 || st->iir_index >= priv->bay3d_iir_cnt ||
	    st->aiisp_index >= priv->aiisp_cnt) {
		dev_err(dev->dev,
			"%s seq:%d error, gain idx:%d cnt:%d, iir idx:%d cnt:%d aiisp idx:%d cnt:%d\n",
			__func__, seq, st->gain_index, priv->gain_cnt,
			st->aiisp_index, priv->aiisp_cnt, st->iir_index, priv->bay3d_iir_cnt);
		return -EINVAL;
	}

	spin_lock_irqsave(&priv->buf_lock, lock_flags);
	buf = &priv->buf_bay3d_iir[st->iir_index];
	if (st->aiisp_index >= 0) {
		priv->pbuf_aiisp = &priv->buf_aiisp[st->aiisp_index];
		aiisp_rd = priv->pbuf_aiisp->dma_addr;
	} else {
		/* NPU no output, just using iir data */
		aiisp_rd = buf->dma_addr;
	}
	priv->aiisp_cur_idx = st->aiisp_index;
	list_for_each_entry(buf_tmp, &priv->iir_list, queue) {
		if (buf_tmp == buf) {
			dev_err(dev->dev, "iir idx:%d error\n", st->iir_index);
			spin_unlock_irqrestore(&priv->buf_lock, lock_flags);
			return 0;
		}
	}
	list_add_tail(&buf->queue, &priv->iir_list);
	priv->pbuf_gain_rd = &priv->buf_gain[st->gain_index];
	spin_unlock_irqrestore(&priv->buf_lock, lock_flags);

	rkisp_params_cfg(&dev->params_vdev, seq, RKISP_PARAMS_LAT);
	rkisp_write(dev, ISP39_AIISP_RD_BASE, aiisp_rd, false);
	val = priv->pbuf_gain_rd->dma_addr;
	rkisp_write(dev, ISP3X_MI_RAW0_RD_BASE, val, false);
	if (dev->hw_dev->is_single) {
		rkisp_set_bits(dev, ISP3X_MI_WR_CTRL2, 0, ISP3X_DBR_RDSELF_UPD, true);
		rkisp_set_bits(dev, ISP3X_CSI2RX_RAW_RD_CTRL, 0, ISP3X_RXSELF_FORCE_UPD, true);
	}
	v4l2_dbg(3, rkisp_debug, &dev->v4l2_dev,
		 "isp_be start seq:%d idx(%d %d) %x %x, %x:%x %x:%x\n",
		 seq, st->aiisp_index, st->gain_index, aiisp_rd,
		 (u32)priv->pbuf_gain_rd->dma_addr,
		 ISP3X_MI_DBR_RD_BASE_SHD, rkisp_read(dev, ISP3X_MI_DBR_RD_BASE_SHD, true),
		 ISP3X_MI_RAW0_RD_BASE_SHD, rkisp_read(dev, ISP3X_MI_RAW0_RD_BASE_SHD, true));
	return 0;
}

static void
rkisp_params_isr_v39(struct rkisp_isp_params_vdev *params_vdev, u32 isp_mis)
{
	struct rkisp_isp_params_val_v39 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	u32 i, val;

	if (isp_mis & CIF_ISP_V_START) {
		if (params_vdev->rdbk_times)
			params_vdev->rdbk_times--;
		if (!dev->is_aiisp_en && priv->bay3d_iir_cnt > 1 &&
		    (!IS_HDR_RDBK(dev->rd_mode) || !params_vdev->rdbk_times)) {
			priv->bay3d_iir_cur_idx = priv->bay3d_iir_idx;
			i = (priv->bay3d_iir_idx + 1) % priv->bay3d_iir_cnt;
			priv->bay3d_iir_idx = i;
			for (i = 0; i < dev->unite_div; i++) {
				val = isp3_param_read_cache(params_vdev, ISP3X_MI_BAY3D_IIR_WR_BASE, i);
				isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_IIR_RD_BASE, i);

				val = priv->buf_bay3d_iir[priv->bay3d_iir_idx].dma_addr;
				val += i * priv->bay3d_iir_size;
				isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_IIR_WR_BASE, i);
			}
		}
		if (IS_HDR_RDBK(dev->rd_mode) && !params_vdev->rdbk_times) {
			struct isp39_isp_params_cfg *params = params_vdev->isp39_params;

			for (i = 0; i < dev->unite_div; i++) {
				if (params->module_cfg_update & ISP39_MODULE_HDRMGE) {
					isp_hdrmge_config(params_vdev, &params->others.hdrmge_cfg, RKISP_PARAMS_SHD, i);
					params->module_cfg_update &= ~ISP39_MODULE_HDRMGE;
				}
				if (params->module_cfg_update & ISP39_MODULE_DRC && !dev->is_aiisp_en) {
					isp_hdrdrc_config(params_vdev, &params->others.drc_cfg, RKISP_PARAMS_SHD, i);
					params->module_cfg_update &= ~ISP39_MODULE_DRC;
				}
				params++;
			}
			return;
		}
	}

	if ((isp_mis & CIF_ISP_FRAME) && !params_vdev->rdbk_times && !dev->hw_dev->is_single)
		rkisp_params_clear_fstflg(params_vdev);

	rkisp_dmarx_get_frame(dev, &i, NULL, NULL, true);
	if (isp_mis & ISP3X_BAY3D_FRM_END && dev->is_aiisp_en) {
		rkisp_params_aiisp_update_buf(params_vdev);
		if (!IS_HDR_RDBK(dev->rd_mode))
			rkisp_params_cfg_v39(params_vdev, i + 1, RKISP_PARAMS_IMD);
	} else if (isp_mis & CIF_ISP_FRAME &&
		   !IS_HDR_RDBK(dev->rd_mode) && !dev->is_aiisp_en) {
		rkisp_params_cfg_v39(params_vdev, i + 1, RKISP_PARAMS_ALL);
	}
}

static struct rkisp_isp_params_ops rkisp_isp_params_ops_tbl = {
	.save_first_param = rkisp_save_first_param_v39,
	.clear_first_param = rkisp_clear_first_param_v39,
	.get_param_size = rkisp_get_param_size_v39,
	.first_cfg = rkisp_params_first_cfg_v39,
	.disable_isp = rkisp_params_disable_isp_v39,
	.isr_hdl = rkisp_params_isr_v39,
	.param_cfg = rkisp_params_cfg_v39,
	.param_cfgsram = rkisp_params_cfgsram_v39,
	.get_meshbuf_inf = rkisp_params_get_meshbuf_inf_v39,
	.set_meshbuf_size = rkisp_params_set_meshbuf_size_v39,
	.free_meshbuf = rkisp_params_free_meshbuf_v39,
	.stream_stop = rkisp_params_stream_stop_v39,
	.fop_release = rkisp_params_fop_release_v39,
	.check_bigmode = rkisp_params_check_bigmode_v39,
	.info2ddr_cfg = rkisp_params_info2ddr_cfg_v39,
	.init_bnr_buf = rkisp_params_init_bnr_buf_v39,
	.aiisp_event = rkisp_params_aiisp_event_v39,
	.aiisp_start = rkisp_params_aiisp_start_v39,
};

int rkisp_init_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_isp_params_val_v39 *priv_val;
	int size;

	priv_val = kzalloc(sizeof(*priv_val), GFP_KERNEL);
	if (!priv_val)
		return -ENOMEM;

	size = sizeof(struct isp39_isp_params_cfg);
	if (params_vdev->dev->hw_dev->unite)
		size *= ISP_UNITE_MAX;
	params_vdev->isp39_params = vmalloc(size);
	if (!params_vdev->isp39_params) {
		kfree(priv_val);
		return -ENOMEM;
	}

	spin_lock_init(&priv_val->buf_lock);
	params_vdev->priv_val = (void *)priv_val;
	params_vdev->ops = &rkisp_isp_params_ops_tbl;
	rkisp_clear_first_param_v39(params_vdev);
	priv_val->buf_info_owner = 0;
	priv_val->buf_info_cnt = 0;
	priv_val->buf_info_idx = -1;
	return 0;
}

void rkisp_uninit_params_vdev_v39(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;

	if (params_vdev->isp39_params)
		vfree(params_vdev->isp39_params);
	if (priv_val) {
		kfree(priv_val);
		params_vdev->priv_val = NULL;
	}
}

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V39_DBG)
static void rkisp_get_params_rawaf(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp39_isp_params_cfg *params)
{
	struct isp39_rawaf_meas_cfg *arg = &params->meas.rawaf;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_RAWAF_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_RAWAF;
	arg->gamma_en = !!(val & BIT(1));
	arg->gaus_en = !!(val & BIT(2));
	arg->h1_fv_mode = !!(val & BIT(8));
	arg->h2_fv_mode = !!(val & BIT(9));
	arg->v1_fv_mode = !!(val & BIT(10));
	arg->v2_fv_mode = !!(val & BIT(11));
	arg->ae_mode = !!(val & BIT(12));
	arg->y_mode = !!(val & BIT(13));
	arg->vldg_sel = !!(val & BIT(14));
	arg->v_dnscl_mode = (val >> 16) & 0x3;
	arg->from_ynr = !!(val & BIT(19));
	arg->ae_config_use = !!(val & BIT(20));
	arg->hiir_left_border_mode = !!(val & BIT(21));
	arg->avg_ds_en = !!(val & BIT(22));
	arg->avg_ds_mode = !!(val & BIT(23));
	arg->h1_acc_mode = !!(val & BIT(24));
	arg->h2_acc_mode = !!(val & BIT(25));
	arg->v1_acc_mode = !!(val & BIT(26));
	arg->v2_acc_mode = !!(val & BIT(27));

	val = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, 0);
	arg->bnr2af_sel = !!(val & BIT(28));
	arg->rawaf_sel = (val >> 18) & 0x3;

	val = isp3_param_read(params_vdev, ISP3X_RAWAF_OFFSET_WINA, 0);
	arg->win[0].v_offs = (val & 0x1fff);
	arg->win[0].h_offs = (val >> 16) & 0x1fff;
	val = isp3_param_read(params_vdev, ISP3X_RAWAF_SIZE_WINA, 0);
	arg->win[0].v_size = (val & 0x1fff);
	arg->win[0].h_size = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAF_OFFSET_WINB, 0);
	arg->win[1].v_offs = (val & 0x1fff);
	arg->win[1].h_offs = (val >> 16) & 0x1fff;
	val = isp3_param_read(params_vdev, ISP3X_RAWAF_SIZE_WINB, 0);
	arg->win[1].v_size = (val & 0x1fff);
	arg->win[1].h_size = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP32L_RAWAF_CTRL1, 0);
	arg->bls_offset = val & 0x1ff;
	arg->bls_en = !!(val & BIT(12));
	arg->aehgl_en = !!(val & BIT(13));
	arg->hldg_dilate_num = (val >> 16) & 0x7;
	arg->tnrin_shift = (val >> 20) & 0xf;

	for (i = 0; i < ISP39_RAWAF_GAMMA_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_RAWAF_GAMMA_Y0 + i * 4, 0);
		arg->gamma_y[2 * i] = val & 0x3ff;
		arg->gamma_y[2 * i + 1] = (val >> 16) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP3X_RAWAF_GAMMA_Y8, 0);
	arg->gamma_y[16] = val & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_RAWAF_HVIIR_VAR_SHIFT, 0);
	arg->h1iir_shift_wina = val & 0x7;
	arg->h2iir_shift_wina = (val >> 4) & 0x7;
	arg->v1iir_shift_wina = (val >> 8) & 0x7;
	arg->v2iir_shift_wina = (val >> 12) & 0x7;
	arg->h1iir_shift_winb = (val >> 16) & 0xf;
	arg->h2iir_shift_winb = (val >> 20) & 0xf;
	arg->v1iir_shift_winb = (val >> 24) & 0xf;
	arg->v2iir_shift_winb = (val >> 28) & 0xf;

	val = isp3_param_read(params_vdev, ISP3X_RAWAF_HIIR_THRESH, 0);
	arg->h_fv_thresh = val & 0xffff;
	arg->v_fv_thresh = (val >> 16) & 0xfff;

	for (i = 0; i < ISP39_RAWAF_VFIR_COE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP32_RAWAF_V_FIR_COE0 + i * 4, 0);
		arg->v1fir_coe[i] = val & 0xfff;
		arg->v2fir_coe[i] = (val >> 16) & 0xfff;
	}

	for (i = 0; i < ISP39_RAWAF_GAUS_COE_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP32_RAWAF_GAUS_COE03 + i * 4, 0);
		arg->gaus_coe[i * 4] = val & 0xff;
		arg->gaus_coe[i * 4 + 1] = (val >> 8) & 0xff;
		arg->gaus_coe[i * 4 + 2] = (val >> 16) & 0xff;
		arg->gaus_coe[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP32_RAWAF_GAUS_COE8, 0);
	arg->gaus_coe[ISP32_RAWAF_GAUS_COE_NUM - 1] = val & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAF_HIGHLIT_THRESH, 0);
	arg->highlit_thresh = val & 0x3ff;

	val = isp3_param_read(params_vdev, ISP32L_RAWAF_CORING_H, 0);
	arg->h_fv_limit = val & 0x3ff;
	arg->h_fv_slope = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP32L_RAWAF_CORING_V, 0);
	arg->v_fv_limit = val & 0x3ff;
	arg->v_fv_slope = (val >> 16) & 0x1ff;

	for (i = 0; i < ISP39_RAWAF_HIIR_COE_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_RAWAF_H1_IIR1_COE01 + i * 4, 0);
		arg->h1iir1_coe[i * 2] = val & 0xfff;
		arg->h1iir1_coe[i * 2 + 1] = (val >> 16) & 0xfff;

		val = isp3_param_read(params_vdev, ISP3X_RAWAF_H1_IIR2_COE01 + i * 4, 0);
		arg->h1iir2_coe[i * 2] = val & 0xfff;
		arg->h1iir2_coe[i * 2 + 1] = (val >> 16) & 0xfff;

		val = isp3_param_read(params_vdev, ISP3X_RAWAF_H2_IIR1_COE01 + i * 4, 0);
		arg->h2iir1_coe[i * 2] = val & 0xfff;
		arg->h2iir1_coe[i * 2 + 1] = (val >> 16) & 0xfff;

		val = isp3_param_read(params_vdev, ISP3X_RAWAF_H2_IIR2_COE01 + i * 4, 0);
		arg->h2iir2_coe[i * 2] = val & 0xfff;
		arg->h2iir2_coe[i * 2 + 1] = (val >> 16) & 0xfff;
	}

	for (i = 0; i < ISP39_RAWAF_VIIR_COE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_RAWAF_V_IIR_COE0 + i * 4, 0);
		arg->v1iir_coe[i] = val & 0xfff;
		arg->v2iir_coe[i] = (val >> 16) & 0xfff;
	}

	for (i = 0; i < ISP39_RAWAF_CURVE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_RAWAF_H_CURVEL + i * 16, 0);
		arg->curve_h[i].ldg_lumth = val & 0xff;
		arg->curve_h[i].ldg_gain = (val >> 8) & 0xff;
		arg->curve_h[i].ldg_gslp = (val >> 16) & 0x1fff;

		val = isp3_param_read(params_vdev, ISP3X_RAWAF_V_CURVEL + i * 16, 0);
		arg->curve_v[i].ldg_lumth = val & 0xff;
		arg->curve_v[i].ldg_gain = (val >> 8) & 0xff;
		arg->curve_v[i].ldg_gslp = (val >> 16) & 0x1fff;
	}
}

static void rkisp_get_params_rawawb(struct rkisp_isp_params_vdev *params_vdev,
				    struct isp39_isp_params_cfg *params)
{
	struct isp39_rawawb_meas_cfg *arg = &params->meas.rawawb;
	struct isp39_rawawb_meas_cfg *arg_rec = &params_vdev->isp39_params->meas.rawawb;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_RAWAWB;
	arg->uv_en0 = !!(val & BIT(1));
	arg->xy_en0 = !!(val & BIT(2));
	arg->yuv3d_en0 = !!(val & BIT(3));
	arg->yuv3d_ls_idx0 = (val >> 4) & 0x7;
	arg->yuv3d_ls_idx1 = (val >> 7) & 0x7;
	arg->yuv3d_ls_idx2 = (val >> 10) & 0x7;
	arg->yuv3d_ls_idx3 = (val >> 13) & 0x7;
	arg->in_rshift_to_12bit_en = !!(val & BIT(16));
	arg->in_overexposure_check_en = !!(val & BIT(17));
	arg->wind_size = !!(val & BIT(18));
	arg->rawlsc_bypass_en = !!(val & BIT(19));
	arg->light_num = (val >> 20) & 0x7;
	arg->uv_en1 = !!(val & BIT(24));
	arg->xy_en1 = !!(val & BIT(25));
	arg->yuv3d_en1 = !!(val & BIT(26));
	arg->low12bit_val = !!(val & BIT(28));

	val = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, 0);
	arg->rawawb_sel = (val >> 20) & 0x3;
	arg->bnr2awb_sel = !!(val & BIT(26));
	arg->drc2awb_sel = !!(val & BIT(27));

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_BLK_CTRL, 0);
	arg->blk_measure_enable = !!(val & BIT(0));
	arg->blk_measure_mode = !!(val & BIT(1));
	arg->blk_measure_xytype = !!(val & BIT(2));
	arg->blk_rtdw_measure_en = !!(val & BIT(3));
	arg->blk_measure_illu_idx = (val >> 4) & 0x7;
	arg->ds16x8_mode_en = !!(val & BIT(7));
	arg->blk_with_luma_wei_en = !!(val & BIT(8));
	arg->in_overexposure_threshold = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_WIN_OFFS, 0);
	arg->h_offs = val & 0x1fff;
	arg->v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_WIN_SIZE, 0);
	arg->h_size = val & 0x1fff;
	arg->v_size = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_LIMIT_RG_MAX, 0);
	arg->r_max = val & 0xfff;
	arg->g_max = (val >> 16) & 0xfff;
	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_LIMIT_BY_MAX, 0);
	arg->b_max = val & 0xfff;
	arg->y_max = (val >> 16) & 0xfff;
	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_LIMIT_RG_MIN, 0);
	arg->r_min = val & 0xfff;
	arg->g_min = (val >> 16) & 0xfff;
	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_LIMIT_BY_MIN, 0);
	arg->b_min = val & 0xfff;
	arg->y_min = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_WEIGHT_CURVE_CTRL, 0);
	arg->wp_luma_wei_en0 = !!(val & BIT(0));
	arg->wp_luma_wei_en1 = !!(val & BIT(1));
	arg->wp_blk_wei_en0 = !!(val & BIT(2));
	arg->wp_blk_wei_en1 = !!(val & BIT(3));
	arg->wp_hist_xytype = !!(val & BIT(4));

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YWEIGHT_CURVE_XCOOR03, 0);
	arg->wp_luma_weicurve_y0 = val & 0xff;
	arg->wp_luma_weicurve_y1 = (val >> 8) & 0xff;
	arg->wp_luma_weicurve_y2 = (val >> 16) & 0xff;
	arg->wp_luma_weicurve_y3 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YWEIGHT_CURVE_XCOOR47, 0);
	arg->wp_luma_weicurve_y4 = val & 0xff;
	arg->wp_luma_weicurve_y5 = (val >> 8) & 0xff;
	arg->wp_luma_weicurve_y6 = (val >> 16) & 0xff;
	arg->wp_luma_weicurve_y7 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YWEIGHT_CURVE_XCOOR8, 0);
	arg->wp_luma_weicurve_y8 = val & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YWEIGHT_CURVE_YCOOR03, 0);
	arg->wp_luma_weicurve_w0 = val & 0x3f;
	arg->wp_luma_weicurve_w1 = (val >> 8) & 0x3f;
	arg->wp_luma_weicurve_w2 = (val >> 16) & 0x3f;
	arg->wp_luma_weicurve_w3 = (val >> 24) & 0x3f;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YWEIGHT_CURVE_YCOOR47, 0);
	arg->wp_luma_weicurve_w4 = val & 0x3f;
	arg->wp_luma_weicurve_w5 = (val >> 8) & 0x3f;
	arg->wp_luma_weicurve_w6 = (val >> 16) & 0x3f;
	arg->wp_luma_weicurve_w7 = (val >> 24) & 0x3f;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YWEIGHT_CURVE_YCOOR8, 0);
	arg->wp_luma_weicurve_w8 = val & 0x3f;
	arg->pre_wbgain_inv_r = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_PRE_WBGAIN_INV, 0);
	arg->pre_wbgain_inv_g = val & 0x1fff;
	arg->pre_wbgain_inv_b = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX0_0, 0);
	arg->vertex0_u_0 = val & 0x1ff;
	arg->vertex0_v_0 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX1_0, 0);
	arg->vertex1_u_0 = val & 0x1ff;
	arg->vertex1_v_0 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX2_0, 0);
	arg->vertex2_u_0 = val & 0x1ff;
	arg->vertex2_v_0 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX3_0, 0);
	arg->vertex3_u_0 = val & 0x1ff;
	arg->vertex3_v_0 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE01_0, 0);
	arg->islope01_0 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE12_0, 0);
	arg->islope12_0 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE23_0, 0);
	arg->islope23_0 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE30_0, 0);
	arg->islope30_0 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX0_1, 0);
	arg->vertex0_u_1 = val & 0x1ff;
	arg->vertex0_v_1 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX1_1, 0);
	arg->vertex1_u_1 = val & 0x1ff;
	arg->vertex1_v_1 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX2_1, 0);
	arg->vertex2_u_1 = val & 0x1ff;
	arg->vertex2_v_1 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX3_1, 0);
	arg->vertex3_u_1 = val & 0x1ff;
	arg->vertex3_v_1 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE01_1, 0);
	arg->islope01_1 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE12_1, 0);
	arg->islope12_1 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE23_1, 0);
	arg->islope23_1 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE30_1, 0);
	arg->islope30_1 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX0_2, 0);
	arg->vertex0_u_2 = val & 0x1ff;
	arg->vertex0_v_2 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX1_2, 0);
	arg->vertex1_u_2 = val & 0x1ff;
	arg->vertex1_v_2 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX2_2, 0);
	arg->vertex2_u_2 = val & 0x1ff;
	arg->vertex2_v_2 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX3_2, 0);
	arg->vertex3_u_2 = val & 0x1ff;
	arg->vertex3_v_2 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE01_2, 0);
	arg->islope01_2 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE12_2, 0);
	arg->islope12_2 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE23_2, 0);
	arg->islope23_2 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE30_2, 0);
	arg->islope30_2 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX0_3, 0);
	arg->vertex0_u_3 = val & 0x1ff;
	arg->vertex0_v_3 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX1_3, 0);
	arg->vertex1_u_3 = val & 0x1ff;
	arg->vertex1_v_3 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX2_3, 0);
	arg->vertex2_u_3 = val & 0x1ff;
	arg->vertex2_v_3 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_VERTEX3_3, 0);
	arg->vertex3_u_3 = val & 0x1ff;
	arg->vertex3_v_3 = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE01_3, 0);
	arg->islope01_3 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE12_3, 0);
	arg->islope12_3 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE23_3, 0);
	arg->islope23_3 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_UV_DETC_ISLOPE30_3, 0);
	arg->islope30_3 = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_RGB2ROTY_0, 0);
	arg->rgb2ryuvmat0_y = val & 0x3ff;
	arg->rgb2ryuvmat1_y = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_RGB2ROTY_1, 0);
	arg->rgb2ryuvmat2_y = val & 0x3ff;
	arg->rgb2ryuvofs_y = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_RGB2ROTU_0, 0);
	arg->rgb2ryuvmat0_u = val & 0x3ff;
	arg->rgb2ryuvmat1_u = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_RGB2ROTU_1, 0);
	arg->rgb2ryuvmat2_u = val & 0x3ff;
	arg->rgb2ryuvofs_u = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_RGB2ROTV_0, 0);
	arg->rgb2ryuvmat0_v = val & 0x3ff;
	arg->rgb2ryuvmat1_v = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_RGB2ROTV_1, 0);
	arg->rgb2ryuvmat2_v = val & 0x3ff;
	arg->rgb2ryuvofs_v = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_Y_0, 0);
	arg->coor_x1_ls0_y = val & 0xfff;
	arg->vec_x21_ls0_y = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_U_0, 0);
	arg->coor_x1_ls0_u = val & 0xfff;
	arg->vec_x21_ls0_u = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_V_0, 0);
	arg->coor_x1_ls0_v = val & 0xfff;
	arg->vec_x21_ls0_v = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X1X2_DIS_0, 0);
	arg->dis_x1x2_ls0 = val & 0x1f;
	arg->rotu0_ls0 = (val >> 16) & 0xff;
	arg->rotu1_ls0 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_UCOOR_0, 0);
	arg->rotu2_ls0 = val & 0xff;
	arg->rotu3_ls0 = (val >> 8) & 0xff;
	arg->rotu4_ls0 = (val >> 16) & 0xff;
	arg->rotu5_ls0 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH0_0, 0);
	arg->th0_ls0 = val & 0xfff;
	arg->th1_ls0 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH1_0, 0);
	arg->th2_ls0 = val & 0xfff;
	arg->th3_ls0 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH2_0, 0);
	arg->th4_ls0 = val & 0xfff;
	arg->th5_ls0 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_Y_1, 0);
	arg->coor_x1_ls1_y = val & 0xfff;
	arg->vec_x21_ls1_y = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_U_1, 0);
	arg->coor_x1_ls1_u = val & 0xfff;
	arg->vec_x21_ls1_u = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_V_1, 0);
	arg->coor_x1_ls1_v = val & 0xfff;
	arg->vec_x21_ls1_v = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X1X2_DIS_1, 0);
	arg->dis_x1x2_ls1 = val & 0x1f;
	arg->rotu0_ls1 = (val >> 16) & 0xff;
	arg->rotu1_ls1 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_UCOOR_1, 0);
	arg->rotu2_ls1 = val & 0xff;
	arg->rotu3_ls1 = (val >> 8) & 0xff;
	arg->rotu4_ls1 = (val >> 16) & 0xff;
	arg->rotu5_ls1 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH0_1, 0);
	arg->th0_ls1 = val & 0xfff;
	arg->th1_ls1 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH1_1, 0);
	arg->th2_ls1 = val & 0xfff;
	arg->th3_ls1 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH2_1, 0);
	arg->th4_ls1 = val & 0xfff;
	arg->th5_ls1 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_Y_2, 0);
	arg->coor_x1_ls2_y = val & 0xfff;
	arg->vec_x21_ls2_y = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_U_2, 0);
	arg->coor_x1_ls2_u = val & 0xfff;
	arg->vec_x21_ls2_u = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_V_2, 0);
	arg->coor_x1_ls2_v = val & 0xfff;
	arg->vec_x21_ls2_v = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X1X2_DIS_2, 0);
	arg->dis_x1x2_ls2 = val & 0x1f;
	arg->rotu0_ls2 = (val >> 16) & 0xff;
	arg->rotu1_ls2 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_UCOOR_2, 0);
	arg->rotu2_ls2 = val & 0xff;
	arg->rotu3_ls2 = (val >> 8) & 0xff;
	arg->rotu4_ls2 = (val >> 16) & 0xff;
	arg->rotu5_ls2 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH0_2, 0);
	arg->th0_ls2 = val & 0xfff;
	arg->th1_ls2 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH1_2, 0);
	arg->th2_ls2 = val & 0xfff;
	arg->th3_ls2 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH2_2, 0);
	arg->th4_ls2 = val & 0xfff;
	arg->th5_ls2 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_Y_3, 0);
	arg->coor_x1_ls3_y = val & 0xfff;
	arg->vec_x21_ls3_y = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_U_3, 0);
	arg->coor_x1_ls3_u = val & 0xfff;
	arg->vec_x21_ls3_u = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X_COOR_V_3, 0);
	arg->coor_x1_ls3_v = val & 0xfff;
	arg->vec_x21_ls3_v = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_X1X2_DIS_3, 0);
	arg->dis_x1x2_ls3 = val & 0x1f;
	arg->rotu0_ls3 = (val >> 16) & 0xff;
	arg->rotu1_ls3 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_UCOOR_3, 0);
	arg->rotu2_ls3 = val & 0xff;
	arg->rotu3_ls3 = (val >> 8) & 0xff;
	arg->rotu4_ls3 = (val >> 16) & 0xff;
	arg->rotu5_ls3 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH0_3, 0);
	arg->th0_ls3 = val & 0xfff;
	arg->th1_ls3 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH1_3, 0);
	arg->th2_ls3 = val & 0xfff;
	arg->th3_ls3 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_YUV_INTERP_CURVE_TH2_3, 0);
	arg->th4_ls3 = val & 0xfff;
	arg->th5_ls3 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_RGB2XY_WT01, 0);
	arg->wt0 = val & 0xfff;
	arg->wt1 = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_RGB2XY_WT2, 0);
	arg->wt2 = val & 0xfff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_RGB2XY_MAT0_XY, 0);
	arg->mat0_x = val & 0x7fff;
	arg->mat0_y = (val >> 16) & 0x7fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_RGB2XY_MAT1_XY, 0);
	arg->mat1_x = val & 0x7fff;
	arg->mat1_y = (val >> 16) & 0x7fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_RGB2XY_MAT2_XY, 0);
	arg->mat2_x = val & 0x7fff;
	arg->mat2_y = (val >> 16) & 0x7fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_NOR_X_0, 0);
	arg->nor_x0_0 = val & 0x3fff;
	arg->nor_x1_0 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_NOR_Y_0, 0);
	arg->nor_y0_0 = val & 0x3fff;
	arg->nor_y1_0 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_BIG_X_0, 0);
	arg->big_x0_0 = val & 0x3fff;
	arg->big_x1_0 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_BIG_Y_0, 0);
	arg->big_y0_0 = val & 0x3fff;
	arg->big_y1_0 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_NOR_X_1, 0);
	arg->nor_x0_1 = val & 0x3fff;
	arg->nor_x1_1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_NOR_Y_1, 0);
	arg->nor_y0_1 = val & 0x3fff;
	arg->nor_y1_1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_BIG_X_1, 0);
	arg->big_x0_1 = val & 0x3fff;
	arg->big_x1_1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_BIG_Y_1, 0);
	arg->big_y0_1 = val & 0x3fff;
	arg->big_y1_1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_NOR_X_2, 0);
	arg->nor_x0_2 = val & 0x3fff;
	arg->nor_x1_2 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_NOR_Y_2, 0);
	arg->nor_y0_2 = val & 0x3fff;
	arg->nor_y1_2 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_BIG_X_2, 0);
	arg->big_x0_2 = val & 0x3fff;
	arg->big_x1_2 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_BIG_Y_2, 0);
	arg->big_y0_2 = val & 0x3fff;
	arg->big_y1_2 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_NOR_X_3, 0);
	arg->nor_x0_3 = val & 0x3fff;
	arg->nor_x1_3 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_NOR_Y_3, 0);
	arg->nor_y0_3 = val & 0x3fff;
	arg->nor_y1_3 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_BIG_X_3, 0);
	arg->big_x0_3 = val & 0x3fff;
	arg->big_x1_3 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_XY_DETC_BIG_Y_3, 0);
	arg->big_y0_3 = val & 0x3fff;
	arg->big_y1_3 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW_EXC_CTRL, 0);
	arg->exc_wp_region0_excen = val & 0x3;
	arg->exc_wp_region0_measen = !!(val & BIT(2));
	arg->exc_wp_region0_domain = !!(val & BIT(3));
	arg->exc_wp_region1_excen = (val >> 4) & 0x3;
	arg->exc_wp_region1_measen = !!(val & BIT(6));
	arg->exc_wp_region1_domain = !!(val & BIT(7));
	arg->exc_wp_region2_excen = (val >> 8) & 0x3;
	arg->exc_wp_region2_measen = !!(val & BIT(10));
	arg->exc_wp_region2_domain = !!(val & BIT(11));
	arg->exc_wp_region3_excen = (val >> 12) & 0x3;
	arg->exc_wp_region3_measen = !!(val & BIT(14));
	arg->exc_wp_region3_domain = !!(val & BIT(15));
	arg->exc_wp_region4_excen = (val >> 16) & 0x3;
	arg->exc_wp_region4_domain = !!(val & BIT(19));
	arg->exc_wp_region5_excen = (val >> 20) & 0x3;
	arg->exc_wp_region5_domain = !!(val & BIT(23));
	arg->exc_wp_region6_excen = (val >> 24) & 0x3;
	arg->exc_wp_region6_domain = !!(val & BIT(27));
	arg->multiwindow_en = !!(val & BIT(31));

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW0_OFFS, 0);
	arg->multiwindow0_h_offs = val & 0x1fff;
	arg->multiwindow0_v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW0_SIZE, 0);
	arg->multiwindow0_h_size = val & 0x1fff;
	arg->multiwindow0_v_size = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW1_OFFS, 0);
	arg->multiwindow1_h_offs = val & 0x1fff;
	arg->multiwindow1_v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW1_SIZE, 0);
	arg->multiwindow1_h_size = val & 0x1fff;
	arg->multiwindow1_v_size = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW2_OFFS, 0);
	arg->multiwindow2_h_offs = val & 0x1fff;
	arg->multiwindow2_v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW2_SIZE, 0);
	arg->multiwindow2_h_size = val & 0x1fff;
	arg->multiwindow2_v_size = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW3_OFFS, 0);
	arg->multiwindow3_h_offs = val & 0x1fff;
	arg->multiwindow3_v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_MULTIWINDOW3_SIZE, 0);
	arg->multiwindow3_h_size = val & 0x1fff;
	arg->multiwindow3_v_size = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION0_XU, 0);
	arg->exc_wp_region0_xu0 = val & 0x3fff;
	arg->exc_wp_region0_xu1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION0_YV, 0);
	arg->exc_wp_region0_yv0 = val & 0x3fff;
	arg->exc_wp_region0_yv1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION1_XU, 0);
	arg->exc_wp_region1_xu0 = val & 0x3fff;
	arg->exc_wp_region1_xu1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION1_YV, 0);
	arg->exc_wp_region1_yv0 = val & 0x3fff;
	arg->exc_wp_region1_yv1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION2_XU, 0);
	arg->exc_wp_region2_xu0 = val & 0x3fff;
	arg->exc_wp_region2_xu1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION2_YV, 0);
	arg->exc_wp_region2_yv0 = val & 0x3fff;
	arg->exc_wp_region2_yv1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION3_XU, 0);
	arg->exc_wp_region3_xu0 = val & 0x3fff;
	arg->exc_wp_region3_xu1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION3_YV, 0);
	arg->exc_wp_region3_yv0 = val & 0x3fff;
	arg->exc_wp_region3_yv1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION4_XU, 0);
	arg->exc_wp_region4_xu0 = val & 0x3fff;
	arg->exc_wp_region4_xu1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION4_YV, 0);
	arg->exc_wp_region4_yv0 = val & 0x3fff;
	arg->exc_wp_region4_yv1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION5_XU, 0);
	arg->exc_wp_region5_xu0 = val & 0x3fff;
	arg->exc_wp_region5_xu1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION5_YV, 0);
	arg->exc_wp_region5_yv0 = val & 0x3fff;
	arg->exc_wp_region5_yv1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION6_XU, 0);
	arg->exc_wp_region6_xu0 = val & 0x3fff;
	arg->exc_wp_region6_xu1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_EXC_WP_REGION6_YV, 0);
	arg->exc_wp_region6_yv0 = val & 0x3fff;
	arg->exc_wp_region6_yv1 = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP32_RAWAWB_EXC_WP_WEIGHT0_3, 0);
	arg->exc_wp_region0_weight = val & 0x3f;
	arg->exc_wp_region1_weight = (val >> 8) & 0x3f;
	arg->exc_wp_region2_weight = (val >> 16) & 0x3f;
	arg->exc_wp_region3_weight = (val >> 24) & 0x3f;

	val = isp3_param_read(params_vdev, ISP32_RAWAWB_EXC_WP_WEIGHT4_6, 0);
	arg->exc_wp_region4_weight = val & 0x3f;
	arg->exc_wp_region5_weight = (val >> 8) & 0x3f;
	arg->exc_wp_region6_weight = (val >> 16) & 0x3f;

	memcpy(arg->wp_blk_wei_w, arg_rec->wp_blk_wei_w, ISP39_RAWAWB_WEIGHT_NUM);
}

static void rkisp_get_params_rawae0(struct rkisp_isp_params_vdev *params_vdev,
				    struct isp39_isp_params_cfg *params)
{
	struct isp2x_rawaebig_meas_cfg *arg = &params->meas.rawae0;
	const u32 ae_wnd_num[] = {1, 5, 15, 15};
	u32 addr = ISP3X_RAWAE_LITE_BASE, i, val;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_RAWAE0;
	arg->wnd_num = (val >> 1) & 0x3;
	arg->subwin_en[0] = !!(val & BIT(4));
	arg->subwin_en[1] = !!(val & BIT(5));
	arg->subwin_en[2] = !!(val & BIT(6));
	arg->subwin_en[3] = !!(val & BIT(7));

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_OFFSET, 0);
	arg->win.h_offs = val & 0x1fff;
	arg->win.v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_BLK_SIZE, 0);
	arg->win.h_size = (val & 0x1fff) * ae_wnd_num[arg->wnd_num];
	arg->win.v_size = ((val >> 16) & 0x1fff) * ae_wnd_num[arg->wnd_num];

	for (i = 0; i < ISP39_RAWAEBIG_SUBWIN_NUM; i++) {
		val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_WND1_OFFSET + 8 * i, 0);
		arg->subwin[i].h_offs = val & 0x1fff;
		arg->subwin[i].v_offs = (val >> 16) & 0x1fff;

		val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_WND1_SIZE + 8 * i, 0);
		arg->subwin[i].h_size = (val & 0x1fff) - arg->subwin[i].h_offs;
		arg->subwin[i].v_size = ((val >> 16) & 0x1fff) - arg->subwin[i].v_offs;
	}

	val = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, 0);
	arg->rawae_sel = (val >> 22) & 0x3;
	if (val & BIT(30))
		arg->rawae_sel |= ISP39_BNR2AE0_SEL_EN;
}

static void rkisp_get_params_rawae3(struct rkisp_isp_params_vdev *params_vdev,
				    struct isp39_isp_params_cfg *params)
{
	struct isp2x_rawaebig_meas_cfg *arg = &params->meas.rawae3;
	const u32 ae_wnd_num[] = {1, 5, 15, 15};
	u32 addr = ISP3X_RAWAE_BIG1_BASE, i, val;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_RAWAE3;
	arg->wnd_num = (val >> 1) & 0x3;
	arg->subwin_en[0] = !!(val & BIT(4));
	arg->subwin_en[1] = !!(val & BIT(5));
	arg->subwin_en[2] = !!(val & BIT(6));
	arg->subwin_en[3] = !!(val & BIT(7));

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_OFFSET, 0);
	arg->win.h_offs = val & 0x1fff;
	arg->win.v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_BLK_SIZE, 0);
	arg->win.h_size = (val & 0x1fff) * ae_wnd_num[arg->wnd_num];
	arg->win.v_size = ((val >> 16) & 0x1fff) * ae_wnd_num[arg->wnd_num];

	for (i = 0; i < ISP39_RAWAEBIG_SUBWIN_NUM; i++) {
		val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_WND1_OFFSET + 8 * i, 0);
		arg->subwin[i].h_offs = val & 0x1fff;
		arg->subwin[i].v_offs = (val >> 16) & 0x1fff;

		val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_WND1_SIZE + 8 * i, 0);
		arg->subwin[i].h_size = (val & 0x1fff) - arg->subwin[i].h_offs;
		arg->subwin[i].v_size = ((val >> 16) & 0x1fff) - arg->subwin[i].v_offs;
	}

	val = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, 0);
	arg->rawae_sel = (val >> 16) & 0x3;
	if (val & BIT(29))
		arg->rawae_sel |= ISP39_BNR2AEBIG_SEL_EN;
}

static void rkisp_get_params_rawhist0(struct rkisp_isp_params_vdev *params_vdev,
				      struct isp39_isp_params_cfg *params)
{
	struct isp2x_rawhistbig_cfg *arg = &params->meas.rawhist0;
	struct isp2x_rawhistbig_cfg *arg_rec = &params_vdev->isp39_params->meas.rawhist0;
	const u32 hist_wnd_num[] = {5, 5, 15, 15};
	u32 addr = ISP3X_RAWHIST_LITE_BASE, val;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_RAWHIST0;
	arg->stepsize = (val >> 1) & 0x7;
	arg->mode = (val >> 8) & 0x7;
	arg->waterline = (val >> 12) & 0xfff;
	arg->data_sel = (val >> 24) & 0x7;
	arg->wnd_num = (val >> 28) & 0x3;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_OFFS, 0);
	arg->win.h_offs = val & 0x1fff;
	arg->win.v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_SIZE, 0);
	arg->win.h_size = (val & 0x1fff) * hist_wnd_num[arg->wnd_num];
	arg->win.v_size = ((val >> 16) & 0x1fff) * hist_wnd_num[arg->wnd_num];

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_RAW2Y_CC, 0);
	arg->rcc = val & 0xff;
	arg->gcc = (val >> 8) & 0xff;
	arg->bcc = (val >> 16) & 0xff;
	arg->off = (val >> 24) & 0xff;

	memcpy(arg->weight, arg_rec->weight, ISP2X_RAWHISTBIG_SUBWIN_NUM);
}

static void rkisp_get_params_rawhist3(struct rkisp_isp_params_vdev *params_vdev,
				      struct isp39_isp_params_cfg *params)
{
	struct isp2x_rawhistbig_cfg *arg = &params->meas.rawhist3;
	struct isp2x_rawhistbig_cfg *arg_rec = &params_vdev->isp39_params->meas.rawhist3;
	const u32 hist_wnd_num[] = {5, 5, 15, 15};
	u32 addr = ISP3X_RAWHIST_BIG1_BASE, val;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_RAWHIST3;
	arg->stepsize = (val >> 1) & 0x7;
	arg->mode = (val >> 8) & 0x7;
	arg->waterline = (val >> 12) & 0xfff;
	arg->data_sel = (val >> 24) & 0x7;
	arg->wnd_num = (val >> 28) & 0x3;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_OFFS, 0);
	arg->win.h_offs = val & 0x1fff;
	arg->win.v_offs = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_SIZE, 0);
	arg->win.h_size = (val & 0x1fff) * hist_wnd_num[arg->wnd_num];
	arg->win.v_size = ((val >> 16) & 0x1fff) * hist_wnd_num[arg->wnd_num];

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_RAW2Y_CC, 0);
	arg->rcc = val & 0xff;
	arg->gcc = (val >> 8) & 0xff;
	arg->bcc = (val >> 16) & 0xff;
	arg->off = (val >> 24) & 0xff;

	memcpy(arg->weight, arg_rec->weight, ISP2X_RAWHISTBIG_SUBWIN_NUM);
}

static void rkisp_get_params_bls(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp32_bls_cfg *arg = &params->others.bls_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_BLS_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_BLS;
	arg->enable_auto = !!(val & BIT(1));
	arg->en_windows = (val >> 2) & 0x3;
	arg->bls1_en = !!(val & BIT(4));
	params->meas.rawawb.bls2_en = !!(val & BIT(5));

	switch (params_vdev->raw_type) {
	case RAW_BGGR:
		val = isp3_param_read(params_vdev, ISP3X_BLS_D_FIXED, 0);
		arg->fixed_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_C_FIXED, 0);
		arg->fixed_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_B_FIXED, 0);
		arg->fixed_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_A_FIXED, 0);
		arg->fixed_val.b = (val & 0x1fff);

		val = isp3_param_read(params_vdev, ISP3X_BLS1_D_FIXED, 0);
		arg->bls1_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_C_FIXED, 0);
		arg->bls1_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_B_FIXED, 0);
		arg->bls1_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_A_FIXED, 0);
		arg->bls1_val.b = (val & 0x1fff);

		val = isp3_param_read(params_vdev, ISP32_BLS2_D_FIXED, 0);
		params->meas.rawawb.bls2_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_C_FIXED, 0);
		params->meas.rawawb.bls2_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_B_FIXED, 0);
		params->meas.rawawb.bls2_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_A_FIXED, 0);
		params->meas.rawawb.bls2_val.b = (val & 0x1fff);
		break;
	case RAW_GBRG:
		val = isp3_param_read(params_vdev, ISP3X_BLS_C_FIXED, 0);
		arg->fixed_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_D_FIXED, 0);
		arg->fixed_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_A_FIXED, 0);
		arg->fixed_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_B_FIXED, 0);
		arg->fixed_val.b = (val & 0x1fff);

		val = isp3_param_read(params_vdev, ISP3X_BLS1_C_FIXED, 0);
		arg->bls1_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_D_FIXED, 0);
		arg->bls1_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_A_FIXED, 0);
		arg->bls1_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_B_FIXED, 0);
		arg->bls1_val.b = (val & 0x1fff);

		val = isp3_param_read(params_vdev, ISP32_BLS2_C_FIXED, 0);
		params->meas.rawawb.bls2_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_D_FIXED, 0);
		params->meas.rawawb.bls2_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_A_FIXED, 0);
		params->meas.rawawb.bls2_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_B_FIXED, 0);
		params->meas.rawawb.bls2_val.b = (val & 0x1fff);
		break;
	case RAW_GRBG:
		val = isp3_param_read(params_vdev, ISP3X_BLS_B_FIXED, 0);
		arg->fixed_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_A_FIXED, 0);
		arg->fixed_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_D_FIXED, 0);
		arg->fixed_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_C_FIXED, 0);
		arg->fixed_val.b = (val & 0x1fff);

		val = isp3_param_read(params_vdev, ISP3X_BLS1_B_FIXED, 0);
		arg->bls1_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_A_FIXED, 0);
		arg->bls1_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_D_FIXED, 0);
		arg->bls1_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_C_FIXED, 0);
		arg->bls1_val.b = (val & 0x1fff);

		val = isp3_param_read(params_vdev, ISP32_BLS2_B_FIXED, 0);
		params->meas.rawawb.bls2_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_A_FIXED, 0);
		params->meas.rawawb.bls2_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_D_FIXED, 0);
		params->meas.rawawb.bls2_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_C_FIXED, 0);
		params->meas.rawawb.bls2_val.b = (val & 0x1fff);
		break;
	case RAW_RGGB:
	default:
		val = isp3_param_read(params_vdev, ISP3X_BLS_A_FIXED, 0);
		arg->fixed_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_B_FIXED, 0);
		arg->fixed_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_C_FIXED, 0);
		arg->fixed_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS_D_FIXED, 0);
		arg->fixed_val.b = (val & 0x1fff);

		val = isp3_param_read(params_vdev, ISP3X_BLS1_A_FIXED, 0);
		arg->bls1_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_B_FIXED, 0);
		arg->bls1_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_C_FIXED, 0);
		arg->bls1_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP3X_BLS1_D_FIXED, 0);
		arg->bls1_val.b = (val & 0x1fff);

		val = isp3_param_read(params_vdev, ISP32_BLS2_A_FIXED, 0);
		params->meas.rawawb.bls2_val.r = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_B_FIXED, 0);
		params->meas.rawawb.bls2_val.gr = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_C_FIXED, 0);
		params->meas.rawawb.bls2_val.gb = (val & 0x1fff);
		val = isp3_param_read(params_vdev, ISP32_BLS2_D_FIXED, 0);
		params->meas.rawawb.bls2_val.b = (val & 0x1fff);
	}

	val = isp3_param_read(params_vdev, ISP3X_BLS_SAMPLES, 0);
	arg->bls_samples = val & 0x1f;

	val = isp3_param_read(params_vdev, ISP3X_BLS_H1_START, 0);
	arg->bls_window1.h_offs = val & 0x3fff;
	val = isp3_param_read(params_vdev, ISP3X_BLS_H1_STOP, 0);
	arg->bls_window1.h_size = (val - arg->bls_window1.h_offs) & 0x3fff;
	val = isp3_param_read(params_vdev, ISP3X_BLS_V1_START, 0);
	arg->bls_window1.v_offs = val & 0x3fff;
	val = isp3_param_read(params_vdev, ISP3X_BLS_V1_STOP, 0);
	arg->bls_window1.v_size = (val - arg->bls_window1.v_offs) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP3X_BLS_H2_START, 0);
	arg->bls_window2.h_offs = val & 0x3fff;
	val = isp3_param_read(params_vdev, ISP3X_BLS_H2_STOP, 0);
	arg->bls_window2.h_size = (val - arg->bls_window2.h_offs) & 0x3fff;
	val = isp3_param_read(params_vdev, ISP3X_BLS_V2_START, 0);
	arg->bls_window2.v_offs = val & 0x3fff;
	val = isp3_param_read(params_vdev, ISP3X_BLS_V2_STOP, 0);
	arg->bls_window2.v_size = (val - arg->bls_window2.v_offs) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP32_BLS_ISP_OB_OFFSET, 0);
	arg->isp_ob_offset = val & 0x1ff;
	val = isp3_param_read(params_vdev, ISP32_BLS_ISP_OB_PREDGAIN, 0);
	arg->isp_ob_predgain = val & 0xffff;
	val = isp3_param_read(params_vdev, ISP32_BLS_ISP_OB_MAX, 0);
	arg->isp_ob_max = val & 0xfffff;
}

static void rkisp_get_params_dpcc(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp39_isp_params_cfg *params)
{
	struct isp39_dpcc_cfg *arg = &params->others.dpcc_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_MODE, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_DPCC;
	arg->grayscale_mode = !!(val & BIT(1));
	arg->stage1_enable = !!(val & BIT(2));

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_OUTPUT_MODE, 0);
	arg->stage1_incl_green_center = !!(val & BIT(0));
	arg->stage1_incl_rb_center = !!(val & BIT(1));
	arg->stage1_g_3x3 = !!(val & BIT(2));
	arg->stage1_rb_3x3 = !!(val & BIT(3));
	arg->sw_dpcc_output_sel = !!(val & BIT(4));
	arg->sw_rk_out_sel = (val >> 5) & 0x3;
	arg->border_bypass_mode = !!(val & BIT(8));

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_SET_USE, 0);
	arg->stage1_use_set_1 = !!(val & BIT(0));
	arg->stage1_use_set_2 = !!(val & BIT(1));
	arg->stage1_use_set_3 = !!(val & BIT(2));
	arg->stage1_use_fix_set = !!(val & BIT(3));

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_METHODS_SET_1, 0);
	arg->pg_green1_enable = !!(val & BIT(0));
	arg->lc_green1_enable = !!(val & BIT(1));
	arg->ro_green1_enable = !!(val & BIT(2));
	arg->rnd_green1_enable = !!(val & BIT(3));
	arg->rg_green1_enable = !!(val & BIT(4));
	arg->sw_rk_green1_en = !!(val & BIT(5));
	arg->pg_red_blue1_enable = !!(val & BIT(8));
	arg->lc_red_blue1_enable = !!(val & BIT(9));
	arg->ro_red_blue1_enable = !!(val & BIT(10));
	arg->rnd_red_blue1_enable = !!(val & BIT(11));
	arg->rg_red_blue1_enable = !!(val & BIT(12));
	arg->sw_rk_red_blue1_en = !!(val & BIT(13));

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_METHODS_SET_2, 0);
	arg->pg_green2_enable = !!(val & BIT(0));
	arg->lc_green2_enable = !!(val & BIT(1));
	arg->ro_green2_enable = !!(val & BIT(2));
	arg->rnd_green2_enable = !!(val & BIT(3));
	arg->rg_green2_enable = !!(val & BIT(4));
	arg->sw_rk_green2_en = !!(val & BIT(5));
	arg->pg_red_blue2_enable = !!(val & BIT(8));
	arg->lc_red_blue2_enable = !!(val & BIT(9));
	arg->ro_red_blue2_enable = !!(val & BIT(10));
	arg->rnd_red_blue2_enable = !!(val & BIT(11));
	arg->rg_red_blue2_enable = !!(val & BIT(12));
	arg->sw_rk_red_blue2_en = !!(val & BIT(13));

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_METHODS_SET_3, 0);
	arg->pg_green3_enable = !!(val & BIT(0));
	arg->lc_green3_enable = !!(val & BIT(1));
	arg->ro_green3_enable = !!(val & BIT(2));
	arg->rnd_green3_enable = !!(val & BIT(3));
	arg->rg_green3_enable = !!(val & BIT(4));
	arg->sw_rk_green3_en = !!(val & BIT(5));
	arg->pg_red_blue3_enable = !!(val & BIT(8));
	arg->lc_red_blue3_enable = !!(val & BIT(9));
	arg->ro_red_blue3_enable = !!(val & BIT(10));
	arg->rnd_red_blue3_enable = !!(val & BIT(11));
	arg->rg_red_blue3_enable = !!(val & BIT(12));
	arg->sw_rk_red_blue3_en = !!(val & BIT(13));

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_LINE_THRESH_1, 0);
	arg->line_thr_1_g = val & 0xff;
	arg->line_thr_1_rb = (val >> 8) & 0xff;
	arg->sw_mindis1_g = (val >> 16) & 0xff;
	arg->sw_mindis1_rb = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_LINE_MAD_FAC_1, 0);
	arg->line_mad_fac_1_g = val & 0xff;
	arg->line_mad_fac_1_rb = (val >> 8) & 0xff;
	arg->sw_dis_scale_max1 = (val >> 16) & 0xff;
	arg->sw_dis_scale_min1 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_PG_FAC_1, 0);
	arg->pg_fac_1_g = val & 0xff;
	arg->pg_fac_1_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_RND_THRESH_1, 0);
	arg->rnd_thr_1_g = val & 0xff;
	arg->rnd_thr_1_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_RG_FAC_1, 0);
	arg->rg_fac_1_g = val & 0xff;
	arg->rg_fac_1_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_LINE_THRESH_2, 0);
	arg->line_thr_2_g = val & 0xff;
	arg->line_thr_2_rb = (val >> 8) & 0xff;
	arg->sw_mindis2_g = (val >> 16) & 0xff;
	arg->sw_mindis2_rb = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_LINE_MAD_FAC_2, 0);
	arg->line_mad_fac_2_g = val & 0xff;
	arg->line_mad_fac_2_rb = (val >> 8) & 0xff;
	arg->sw_dis_scale_max2 = (val >> 16) & 0xff;
	arg->sw_dis_scale_min2 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_PG_FAC_2, 0);
	arg->pg_fac_2_g = val & 0xff;
	arg->pg_fac_2_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_RND_THRESH_2, 0);
	arg->rnd_thr_2_g = val & 0xff;
	arg->rnd_thr_2_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_RG_FAC_2, 0);
	arg->rg_fac_2_g = val & 0xff;
	arg->rg_fac_2_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_LINE_THRESH_3, 0);
	arg->line_thr_3_g = val & 0xff;
	arg->line_thr_3_rb = (val >> 8) & 0xff;
	arg->sw_mindis3_g = (val >> 16) & 0xff;
	arg->sw_mindis3_rb = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_LINE_MAD_FAC_3, 0);
	arg->line_mad_fac_3_g = val & 0xff;
	arg->line_mad_fac_3_rb = (val >> 8) & 0xff;
	arg->sw_dis_scale_max3 = (val >> 16) & 0xff;
	arg->sw_dis_scale_min3 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_PG_FAC_3, 0);
	arg->pg_fac_3_g = val & 0xff;
	arg->pg_fac_3_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_RND_THRESH_3, 0);
	arg->rnd_thr_3_g = val & 0xff;
	arg->rnd_thr_3_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_RG_FAC_3, 0);
	arg->rg_fac_3_g = val & 0xff;
	arg->rg_fac_3_rb = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_RO_LIMITS, 0);
	arg->ro_lim_1_g = val & 0x3;
	arg->ro_lim_1_rb = (val >> 2) & 0x3;
	arg->ro_lim_2_g = (val >> 4) & 0x3;
	arg->ro_lim_2_rb = (val >> 6) & 0x3;
	arg->ro_lim_3_g = (val >> 8) & 0x3;
	arg->ro_lim_3_rb = (val >> 10) & 0x3;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_RND_OFFS, 0);
	arg->rnd_offs_1_g = val & 0x3;
	arg->rnd_offs_1_rb = (val >> 2) & 0x3;
	arg->rnd_offs_2_g = (val >> 4) & 0x3;
	arg->rnd_offs_2_rb = (val >> 6) & 0x3;
	arg->rnd_offs_3_g = (val >> 8) & 0x3;
	arg->rnd_offs_3_rb = (val >> 10) & 0x3;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_PDAF_EN, 0);
	arg->sw_pdaf_en = !!(val & BIT(0));

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_PDAF_POINT_EN, 0);
	for (i = 0; i < ISP39_DPCC_PDAF_POINT_NUM; i++)
		arg->pdaf_point_en[i] = !!(val & BIT(i));

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_PDAF_OFFSET, 0);
	arg->pdaf_offsetx = val & 0xffff;
	arg->pdaf_offsety = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_PDAF_WRAP, 0);
	arg->pdaf_wrapx = val & 0xffff;
	arg->pdaf_wrapy = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP_DPCC0_PDAF_SCOPE, 0);
	arg->pdaf_wrapx_num = val & 0xffff;
	arg->pdaf_wrapy_num = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_DPCC_PDAF_POINT_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_DPCC0_PDAF_POINT_0 + 4 * i, 0);
		arg->point[2 * i].x = val & 0xff;
		arg->point[2 * i].y = (val >> 8) & 0xff;
		arg->point[2 * i + 1].x = (val >> 16) & 0xff;
		arg->point[2 * i + 1].y = (val >> 24) & 0xff;
	}

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_PDAF_FORWARD_MED, 0);
	arg->pdaf_forward_med = !!(val & BIT(0));
}

static void rkisp_get_params_lsc(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp3x_lsc_cfg *arg = &params->others.lsc_cfg;
	struct isp3x_lsc_cfg *arg_rec = &params_vdev->isp39_params->others.lsc_cfg;
	u32 val, i;

	val = isp3_param_read(params_vdev, ISP3X_LSC_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_LSC;
	arg->sector_16x16 = !!(val & BIT(2));

	for (i = 0; i < ISP39_LSC_SIZE_TBL_SIZE / 4; i++) {
		val = isp3_param_read(params_vdev, ISP3X_LSC_XSIZE_01 + i * 4, 0);
		arg->x_size_tbl[i * 2] = val & 0xffff;
		arg->x_size_tbl[i * 2 + 1] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_LSC_XSIZE_89 + i * 4, 0);
		arg->x_size_tbl[i * 2 + 8] = val & 0xffff;
		arg->x_size_tbl[i * 2 + 9] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_LSC_XGRAD_01 + i * 4, 0);
		arg->x_grad_tbl[i * 2] = val & 0xffff;
		arg->x_grad_tbl[i * 2 + 1] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_LSC_XGRAD_89 + i * 4, 0);
		arg->x_grad_tbl[i * 2 + 8] = val & 0xffff;
		arg->x_grad_tbl[i * 2 + 9] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_LSC_YSIZE_01 + i * 4, 0);
		arg->y_size_tbl[i * 2] = val & 0xffff;
		arg->y_size_tbl[i * 2 + 1] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_LSC_YSIZE_89 + i * 4, 0);
		arg->y_size_tbl[i * 2 + 8] = val & 0xffff;
		arg->y_size_tbl[i * 2 + 9] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_LSC_YGRAD_01 + i * 4, 0);
		arg->y_grad_tbl[i * 2] = val & 0xffff;
		arg->y_grad_tbl[i * 2 + 1] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_LSC_YGRAD_89 + i * 4, 0);
		arg->y_grad_tbl[i * 2 + 8] = val & 0xffff;
		arg->y_grad_tbl[i * 2 + 9] = (val >> 16) & 0xffff;
	}
	memcpy(arg->r_data_tbl, arg_rec->r_data_tbl, ISP3X_LSC_DATA_TBL_SIZE);
	memcpy(arg->gr_data_tbl, arg_rec->gr_data_tbl, ISP3X_LSC_DATA_TBL_SIZE);
	memcpy(arg->gb_data_tbl, arg_rec->gb_data_tbl, ISP3X_LSC_DATA_TBL_SIZE);
	memcpy(arg->b_data_tbl, arg_rec->b_data_tbl, ISP3X_LSC_DATA_TBL_SIZE);
}

static void rkisp_get_params_awbgain(struct rkisp_isp_params_vdev *params_vdev,
				     struct isp39_isp_params_cfg *params)
{
	struct isp32_awb_gain_cfg *arg = &params->others.awb_gain_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_ISP_CTRL0, 0);
	if (!(val & CIF_ISP_CTRL_ISP_AWB_ENA))
		return;
	params->module_ens |= ISP39_MODULE_AWB_GAIN;

	val = isp3_param_read(params_vdev, ISP3X_ISP_AWB_GAIN0_G, 0);
	arg->gain0_green_b = val & 0xffff;
	arg->gain0_green_r = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_ISP_AWB_GAIN0_RB, 0);
	arg->gain0_blue = val & 0xffff;
	arg->gain0_red = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_ISP_AWB_GAIN1_G, 0);
	arg->gain1_green_b = val & 0xffff;
	arg->gain1_green_r = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_ISP_AWB_GAIN1_RB, 0);
	arg->gain1_blue = val & 0xffff;
	arg->gain1_red = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_ISP_AWB_GAIN2_G, 0);
	arg->gain2_green_b = val & 0xffff;
	arg->gain2_green_r = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_ISP_AWB_GAIN2_RB, 0);
	arg->gain2_blue = val & 0xffff;
	arg->gain2_red = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP32_ISP_AWB1_GAIN_G, 0);
	arg->awb1_gain_gb = val & 0xffff;
	arg->awb1_gain_gr = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP32_ISP_AWB1_GAIN_RB, 0);
	arg->awb1_gain_b = val & 0xffff;
	arg->awb1_gain_r = (val >> 16) & 0xffff;
}

static void rkisp_get_params_gic(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp39_gic_cfg *arg = &params->others.gic_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_GIC_CONTROL, 0);
	if (!(val & (ISP39_MODULE_EN | BIT(31))))
		return;
	params->module_ens |= ISP39_MODULE_GIC;
	arg->bypass_en = (val == BIT(31)) ? 1 : !!(val & BIT(1));

	val = isp3_param_read(params_vdev, ISP3X_GIC_DIFF_PARA1, 0);
	arg->regminbusythre = val & 0x3ff;
	arg->regmingradthrdark1 = (val >> 10) & 0x3ff;
	arg->regmingradthrdark2 = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_GIC_DIFF_PARA2, 0);
	arg->regdarktthrehi = val & 0x7ff;
	arg->regmaxcorvboth = (val >> 11) & 0x3ff;
	arg->regdarkthre = (val >> 21) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP3X_GIC_DIFF_PARA3, 0);
	arg->reggbthre = val & 0xf;
	arg->regkgrad1 = (val >> 4) & 0xf;
	arg->regkgrad2 = (val >> 8) & 0xf;
	arg->regdarkthrestep = (val >> 12) & 0xf;
	arg->regstrengthglobal_fix = (val >> 16) & 0xff;
	arg->regkgrad1dark = (val >> 24) & 0xf;
	arg->regkgrad2dark = (val >> 28) & 0xf;

	val = isp3_param_read(params_vdev, ISP3X_GIC_DIFF_PARA4, 0);
	arg->regmingradthr1 = val & 0x3ff;
	arg->regmingradthr2 = (val >> 10) & 0x3ff;
	arg->regmaxcorv = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_GIC_NOISE_PARA1, 0);
	arg->noise_base = val & 0xfff;
	arg->noise_scale = (val >> 12) & 0x7f;
	arg->gr_ratio = (val >> 28) & 0x3;

	val = isp3_param_read(params_vdev, ISP3X_GIC_NOISE_PARA2, 0);
	arg->diff_clip = val & 0x7fff;

	for (i = 0; i < ISP39_GIC_SIGMA_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_GIC_SIGMA_VALUE0 + 4 * i, 0);
		arg->sigma_y[2 * i] = val & 0xffff;
		arg->sigma_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_GIC_SIGMA_VALUE0 + 4 * i, 0);
	arg->sigma_y[2 * i] = val & 0xffff;
}

static void rkisp_get_params_debayer(struct rkisp_isp_params_vdev *params_vdev,
				     struct isp39_isp_params_cfg *params)
{
	struct isp39_debayer_cfg *arg = &params->others.debayer_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_DEBAYER_CONTROL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_DEBAYER;
	arg->filter_g_en = !!(val & BIT(4));
	arg->filter_c_en = !!(val & BIT(8));

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_LUMA_DX, 0);
	arg->luma_dx[0] = val & 0xf;
	arg->luma_dx[1] = (val >> 4) & 0xf;
	arg->luma_dx[2] = (val >> 8) & 0xf;
	arg->luma_dx[3] = (val >> 12) & 0xf;
	arg->luma_dx[4] = (val >> 16) & 0xf;
	arg->luma_dx[5] = (val >> 20) & 0xf;
	arg->luma_dx[6] = (val >> 24) & 0xf;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP, 0);
	arg->clip_en = !!(val & BIT(0));
	arg->dist_scale = (val >> 4) & 0xf;
	arg->thed0 = (val >> 8) & 0xf;
	arg->thed1 = (val >> 12) & 0xf;
	arg->select_thed = (val >> 16) & 0xff;
	arg->max_ratio = (val >> 24) & 0x3f;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP_FILTER1, 0);
	arg->filter1_coe1 = val & 0x1f;
	arg->filter1_coe2 = (val >> 8) & 0x1f;
	arg->filter1_coe3 = (val >> 16) & 0x1f;
	arg->filter1_coe4 = (val >> 24) & 0x1f;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP_FILTER2, 0);
	arg->filter2_coe1 = val & 0x1f;
	arg->filter2_coe2 = (val >> 8) & 0x1f;
	arg->filter2_coe3 = (val >> 16) & 0x1f;
	arg->filter2_coe4 = (val >> 24) & 0x1f;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP_OFFSET_ALPHA, 0);
	arg->gain_offset = val & 0xfff;
	arg->gradloflt_alpha = (val >> 16) & 0x7f;
	arg->wgt_alpha = (val >> 24) & 0x7f;

	for (i = 0; i < ISP39_DEBAYER_DRCT_OFFSET_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP_DRCT_OFFSET0 + i * 4, 0);
		arg->drct_offset[i * 2] = val & 0xffff;
		arg->drct_offset[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_FILTER_MODE_OFFSET, 0);
	arg->gfilter_mode = !!(val & BIT(0));
	arg->bf_ratio = (val >> 4) & 0xfff;
	arg->offset = (val >> 16) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_FILTER_FILTER, 0);
	arg->filter_coe0 = val & 0xff;
	arg->filter_coe1 = (val >> 8) & 0xff;
	arg->filter_coe2 = (val >> 16) & 0xff;

	for (i = 0; i < ISP39_DEBAYER_VSIGMA_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_FILTER_VSIGMA0 + i * 4, 0);
		arg->vsigma[i * 2] = val & 0xffff;
		arg->vsigma[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_GUIDE_GAUS, 0);
	arg->guid_gaus_coe0 = val & 0xff;
	arg->guid_gaus_coe1 = (val >> 8) & 0xff;
	arg->guid_gaus_coe2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_CE_GAUS, 0);
	arg->ce_gaus_coe0 = val & 0xff;
	arg->ce_gaus_coe1 = (val >> 8) & 0xff;
	arg->ce_gaus_coe2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_ALPHA_GAUS, 0);
	arg->alpha_gaus_coe0 = val & 0xff;
	arg->alpha_gaus_coe1 = (val >> 8) & 0xff;
	arg->alpha_gaus_coe2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_LOG_OFFSET, 0);
	arg->loghf_offset = val & 0x1fff;
	arg->loggd_offset = (val >> 16) & 0xfff;
	arg->log_en = !!(val & BIT(31));

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_ALPHA, 0);
	arg->alpha_offset = val & 0xfff;
	arg->alpha_scale = (val >> 12) & 0xfffff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_EDGE, 0);
	arg->edge_offset = val & 0xfff;
	arg->edge_scale = (val >> 12) & 0xfffff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_IIR_0, 0);
	arg->ce_sgm = val & 0xff;
	arg->exp_shift = (val >> 8) & 0x3f;
	arg->wgtslope = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_IIR_1, 0);
	arg->wet_clip = val & 0x7f;
	arg->wet_ghost = (val >> 8) & 0x3f;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_BF, 0);
	arg->bf_sgm = val & 0xffff;
	arg->bf_clip = (val >> 16) & 0x7f;
	arg->bf_curwgt = (val >> 24) & 0x7f;
}

static void rkisp_get_params_ccm(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp39_ccm_cfg *arg = &params->others.ccm_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_CCM_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_CCM;
	arg->highy_adjust_dis = !!(val & BIT(1));
	arg->enh_adj_en = !!(val & BIT(2));
	arg->asym_adj_en = !!(val & BIT(3));
	arg->sat_decay_en = !!(val & BIT(4));

	val = isp3_param_read(params_vdev, ISP3X_CCM_COEFF0_R, 0);
	arg->coeff0_r = val & 0xffff;
	arg->coeff1_r = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_CCM_COEFF1_R, 0);
	arg->coeff2_r = val & 0xffff;
	arg->offset_r = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_CCM_COEFF0_G, 0);
	arg->coeff0_g = val & 0xffff;
	arg->coeff1_g = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_CCM_COEFF1_G, 0);
	arg->coeff2_g = val & 0xffff;
	arg->offset_g = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_CCM_COEFF0_B, 0);
	arg->coeff0_b = val & 0xffff;
	arg->coeff1_b = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_CCM_COEFF1_B, 0);
	arg->coeff2_b = val & 0xffff;
	arg->offset_b = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_CCM_COEFF0_Y, 0);
	arg->coeff0_y = val & 0xffff;
	arg->coeff1_y = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_CCM_COEFF1_Y, 0);
	arg->coeff2_y = val & 0xffff;

	for (i = 0; i < ISP39_CCM_CURVE_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_CCM_ALP_Y0 + 4 * i, 0);
		arg->alp_y[2 * i] = val & 0xffff;
		arg->alp_y[2 * i + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP3X_CCM_BOUND_BIT, 0);
	arg->bound_bit = val & 0xf;
	arg->right_bit = (val >> 4) & 0xf;

	val = isp3_param_read(params_vdev, ISP32_CCM_ENHANCE0, 0);
	arg->color_coef0_r2y = val & 0x7ff;
	arg->color_coef1_g2y = (val >> 16) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP32_CCM_ENHANCE1, 0);
	arg->color_coef2_b2y = val & 0x7ff;
	arg->color_enh_rat_max = (val >> 16) & 0x3fff;

	val = isp3_param_read(params_vdev, ISP39_CCM_HF_THD, 0);
	arg->hf_low = val & 0xff;
	arg->hf_up = (val >> 8) & 0xff;
	arg->hf_scale = (val >> 16) & 0x3fff;

	for (i = 0; i < ISP39_CCM_HF_FACTOR_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_HF_FACTOR0 + i * 4, 0);
		arg->hf_factor[i * 2] = val & 0xffff;
		arg->hf_factor[i * 2 + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP39_HF_FACTOR0 + i * 4, 0);
	arg->hf_factor[i * 2] = val & 0xffff;
}

static void rkisp_get_params_gammaout(struct rkisp_isp_params_vdev *params_vdev,
				      struct isp39_isp_params_cfg *params)
{
	struct isp3x_gammaout_cfg *arg = &params->others.gammaout_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_GOC;
	arg->equ_segm = !!(val & BIT(1));
	arg->finalx4_dense_en = !!(val & BIT(2));

	val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_OFFSET, 0);
	arg->offset = val & 0xffff;

	for (i = 0; i < ISP39_GAMMA_OUT_MAX_SAMPLES / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_Y0 + i * 4, 0);
		arg->gamma_y[2 * i] = val & 0xffff;
		arg->gamma_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_Y0 + i * 4, 0);
	arg->gamma_y[2 * i] = val & 0xffff;
}

static void rkisp_get_params_cproc(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp39_isp_params_cfg *params)
{
	struct isp2x_cproc_cfg *arg = &params->others.cproc_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_CPROC_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_CPROC;
	arg->y_out_range = !!(val & BIT(1));
	arg->y_in_range = !!(val & BIT(2));
	arg->c_out_range = !!(val & BIT(3));

	val = isp3_param_read(params_vdev, ISP3X_CPROC_CONTRAST, 0);
	arg->contrast = val & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_CPROC_HUE, 0);
	arg->hue = val & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_CPROC_SATURATION, 0);
	arg->sat = val & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_CPROC_BRIGHTNESS, 0);
	arg->brightness = val & 0xff;
}

static void rkisp_get_params_sdg(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp2x_sdg_cfg *arg = &params->others.sdg_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_ISP_CTRL0, 0);
	if (!(val & CIF_ISP_CTRL_ISP_GAMMA_IN_ENA))
		return;
	params->module_ens |= ISP39_MODULE_SDG;

	val = isp3_param_read(params_vdev, ISP3X_ISP_GAMMA_DX_LO, 0);
	arg->xa_pnts.gamma_dx0 = val;

	val = isp3_param_read(params_vdev, ISP3X_ISP_GAMMA_DX_HI, 0);
	arg->xa_pnts.gamma_dx1 = val;

	for (i = 0; i < ISP39_DEGAMMA_CURVE_SIZE; i++) {
		val = isp3_param_read(params_vdev, ISP3X_ISP_GAMMA_R_Y_0 + i * 4, 0);
		arg->curve_r.gamma_y[i] = val & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_ISP_GAMMA_G_Y_0 + i * 4, 0);
		arg->curve_g.gamma_y[i] = val & 0xffff;
		val = isp3_param_read(params_vdev, ISP3X_ISP_GAMMA_B_Y_0 + i * 4, 0);
		arg->curve_b.gamma_y[i] = val & 0xffff;
	}
}

static void rkisp_get_params_drc(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp39_drc_cfg *arg = &params->others.drc_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_DRC_CTRL0, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_DRC;
	arg->bypass_en = !!(val & BIT(1));
	arg->cmps_byp_en = !!(val & BIT(2));
	arg->gainx32_en = !!(val & BIT(3));
	arg->raw_dly_dis = !!(val & BIT(29));

	val = isp3_param_read(params_vdev, ISP3X_DRC_CTRL1, 0);
	arg->position = val & 0x3fff;
	arg->compres_scl = (val >> 14) & 0x1fff;
	arg->offset_pow2 = (val >> 28) & 0xf;

	val = isp3_param_read(params_vdev, ISP3X_DRC_LPRATIO, 0);
	arg->lpdetail_ratio = val & 0xfff;
	arg->hpdetail_ratio = (val >> 12) & 0xfff;
	arg->delta_scalein = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT0, 0);
	arg->bilat_wt_off = val & 0xff;
	arg->thumb_thd_neg = (val >> 8) & 0x1ff;
	arg->thumb_thd_enable = !!(val & BIT(23));
	arg->weicur_pix = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT1, 0);
	arg->cmps_offset_bits_int = val & 0xf;
	arg->cmps_fixbit_mode = !!(val & BIT(4));
	arg->drc_gas_t = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT2, 0);
	arg->thumb_clip = val & 0xfff;
	arg->thumb_scale = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT3, 0);
	arg->range_sgm_inv0 = val & 0x3ff;
	arg->range_sgm_inv1 = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT4, 0);
	arg->weig_bilat = val & 0x1f;
	arg->weight_8x8thumb = (val >> 8) & 0xff;
	arg->bilat_soft_thd = (val >> 16) & 0x7ff;
	arg->enable_soft_thd = !!(val & BIT(31));

	for (i = 0; i < ISP39_DRC_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_DRC_GAIN_Y0 + 4 * i, 0);
		arg->gain_y[2 * i] = val & 0xffff;
		arg->gain_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_DRC_GAIN_Y0 + 4 * i, 0);
	arg->gain_y[2 * i] = val & 0xffff;

	for (i = 0; i < ISP39_DRC_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_DRC_COMPRES_Y0 + 4 * i, 0);
		arg->compres_y[2 * i] = val & 0xffff;
		arg->compres_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_DRC_COMPRES_Y0 + 4 * i, 0);
	arg->compres_y[2 * i] = val & 0xffff;

	for (i = 0; i < ISP39_DRC_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_DRC_SCALE_Y0 + 4 * i, 0);
		arg->scale_y[2 * i] = val & 0xffff;
		arg->scale_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_DRC_SCALE_Y0 + 4 * i, 0);
	arg->scale_y[2 * i] = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_DRC_IIRWG_GAIN, 0);
	arg->min_ogain = val & 0xffff;

	for (i = 0; i < ISP39_DRC_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_DRC_SFTHD_Y0 + 4 * i, 0);
		arg->sfthd_y[2 * i] = val & 0xffff;
		arg->sfthd_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP39_DRC_SFTHD_Y0 + 4 * i, 0);
	arg->sfthd_y[2 * i] = val & 0xffff;
}

static void rkisp_get_params_hdrmge(struct rkisp_isp_params_vdev *params_vdev,
				    struct isp39_isp_params_cfg *params)
{
	struct isp32_hdrmge_cfg *arg = &params->others.hdrmge_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_HDRMGE;
	arg->s_base = !!(val & BIT(1));
	arg->mode = (val >> 2) & 0x3;
	arg->dbg_mode = (val >> 4) & 0x3;
	arg->each_raw_en = !!(val & BIT(6));

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_GAIN0, 0);
	arg->gain0 = val & 0xffff;
	arg->gain0_inv = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_GAIN1, 0);
	arg->gain1 = val & 0xffff;
	arg->gain1_inv = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_GAIN2, 0);
	arg->gain2 = val & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_LIGHTZ, 0);
	arg->ms_dif_0p8 = val & 0xff;
	arg->ms_diff_0p15 = (val >> 8) & 0xff;
	arg->lm_dif_0p9 = (val >> 16) & 0xff;
	arg->lm_dif_0p15 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_MS_DIFF, 0);
	arg->ms_scl = val & 0x7ff;
	arg->ms_thd0 = (val >> 12) & 0x3ff;
	arg->ms_thd1 = (val >> 22) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_LM_DIFF, 0);
	arg->lm_scl = val & 0x7ff;
	arg->lm_thd0 = (val >> 12) & 0x3ff;
	arg->lm_thd1 = (val >> 22) & 0x3ff;

	for (i = 0; i < ISP39_HDRMGE_L_CURVE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_HDRMGE_DIFF_Y0 + 4 * i, 0);
		arg->curve.curve_0[i] = val & 0xffff;
		arg->curve.curve_1[i] = (val >> 16) & 0xffff;
	}

	for (i = 0; i < ISP39_HDRMGE_E_CURVE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_HDRMGE_OVER_Y0 + 4 * i, 0);
		arg->e_y[i] = val & 0x3ff;
		arg->l_raw0[i] = (val >> 10) & 0x3ff;
		arg->l_raw1[i] = (val >> 20) & 0x3ff;
	}

	val = isp3_param_read(params_vdev, ISP32_HDRMGE_EACH_GAIN, 0);
	arg->each_raw_gain0 = val & 0xffff;
	arg->each_raw_gain1 = (val >> 16) & 0xffff;
}

static void rkisp_get_params_dhaz(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp39_isp_params_cfg *params)
{
	struct isp39_dhaz_cfg *arg = &params->others.dhaz_cfg;
	struct isp39_dhaz_cfg *arg_rec = &params_vdev->isp39_params->others.dhaz_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_DHAZ_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_DHAZ;
	arg->dc_en = !!(val & BIT(4));
	arg->hist_en = !!(val & BIT(8));
	arg->map_mode = !!(val & BIT(9));
	arg->mem_mode = !!(val & BIT(10));
	arg->mem_force = !!(val & BIT(11));
	arg->air_lc_en = !!(val & BIT(16));
	arg->enhance_en = !!(val & BIT(20));
	arg->soft_wr_en = !!(val & BIT(25));
	arg->round_en = !!(val & BIT(26));
	arg->color_deviate_en = !!(val & BIT(27));
	arg->enh_luma_en = !!(val & BIT(28));

	val = isp3_param_read(params_vdev, ISP3X_DHAZ_ADP0, 0);
	arg->dc_min_th = val & 0xff;
	arg->dc_max_th = (val >> 8) & 0xff;
	arg->yhist_th = (val >> 16) & 0xff;
	arg->yblk_th = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DHAZ_ADP1, 0);
	arg->bright_min = val & 0xff;
	arg->bright_max = (val >> 8) & 0xff;
	arg->wt_max = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_DHAZ_ADP2, 0);
	arg->air_min = val & 0xff;
	arg->air_max = (val >> 8) & 0xff;
	arg->dark_th = (val >> 16) & 0xff;
	arg->tmax_base = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_DHAZ_ADP_TMAX, 0);
	arg->tmax_off = val & 0xffff;
	arg->tmax_max = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_ENHANCE, 0);
	arg->enhance_chroma = val & 0xffff;
	arg->enhance_value = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_IIR0, 0);
	arg->stab_fnum = val & 0x1f;
	arg->iir_sigma = (val >> 8) & 0xff;
	arg->iir_wt_sigma = (val >> 16) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_IIR1, 0);
	arg->iir_air_sigma = val & 0xff;
	arg->iir_tmax_sigma = (val >> 8) & 0x7ff;
	arg->iir_pre_wet = (val >> 24) & 0xf;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_SOFT_CFG0, 0);
	arg->cfg_alpha = val & 0xff;
	arg->cfg_air = (val >> 8) & 0xff;
	arg->cfg_wt = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_SOFT_CFG1, 0);
	arg->cfg_tmax = val & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_BF_SIGMA, 0);
	arg->space_sigma_cur = val & 0xff;
	arg->space_sigma_pre = (val >> 8) & 0xff;
	arg->range_sima = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_BF_WET, 0);
	arg->bf_weight = val & 0xffff;
	arg->dc_weitcur = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_DHAZ_ENH_CURVE_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_DHAZ_ENH_CURVE0 + 4 * i, 0);
		arg->enh_curve[3 * i] = val & 0x3ff;
		arg->enh_curve[3 * i + 1] = (val >> 10) & 0x3ff;
		arg->enh_curve[3 * i + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_DHAZ_ENH_CURVE0 + 4 * i, 0);
	arg->enh_curve[3 * i] = val & 0x3ff;
	arg->enh_curve[3 * i + 1] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_GAUS, 0);
	arg->gaus_h0 = val & 0x3f;
	arg->gaus_h1 = (val >> 8) & 0x3f;
	arg->gaus_h2 = (val >> 16) & 0x3f;

	for (i = 0; i < ISP39_DHAZ_ENH_LUMA_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_DHAZ_ENH_LUMA0 + i * 4, 0);
		arg->enh_luma[3 * i] = val & 0x3ff;
		arg->enh_luma[3 * i + 1] = (val >> 10) & 0x3ff;
		arg->enh_luma[3 * i + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_DHAZ_ENH_LUMA0 + i * 4, 0);
	arg->enh_luma[3 * i] = val & 0x3ff;
	arg->enh_luma[3 * i + 1] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_ADP_WR0, 0);
	arg->adp_wt_wr = val & 0x1ff;
	arg->adp_air_wr = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_ADP_WR1, 0);
	arg->adp_tmax_wr = val & 0x1fff;

	for (i = 0; i < ISP39_DHAZ_SIGMA_IDX_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP39_DHAZ_GAIN_IDX0 + i * 4, 0);
		arg->sigma_idx[i * 4] = val & 0xff;
		arg->sigma_idx[i * 4 + 1] = (val >> 8) & 0xff;
		arg->sigma_idx[i * 4 + 2] = (val >> 16) & 0xff;
		arg->sigma_idx[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP39_DHAZ_GAIN_IDX0 + i * 4, 0);
	arg->sigma_idx[i * 4] = val & 0xff;
	arg->sigma_idx[i * 4 + 1] = (val >> 8) & 0xff;
	arg->sigma_idx[i * 4 + 2] = (val >> 16) & 0xff;

	for (i = 0; i < ISP39_DHAZ_SIGMA_LUT_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_DHAZ_GAIN_LUT0 + i * 4, 0);
		arg->sigma_lut[3 * i] = val & 0x3ff;
		arg->sigma_lut[3 * i + 1] = (val >> 10) & 0x3ff;
		arg->sigma_lut[3 * i + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_DHAZ_GAIN_LUT0 + i * 4, 0);
	arg->sigma_lut[3 * i] = val & 0x3ff;
	arg->sigma_lut[3 * i + 1] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_GAIN_FUSE, 0);
	arg->gain_fuse_alpha = val & 0x1ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_ADP_HF, 0);
	arg->hist_k = val & 0x1f;
	arg->hist_th_off = (val >> 8) & 0xff;
	arg->hist_min = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_HIST_CFG, 0);
	arg->cfg_k_alpha = val & 0x1ff;
	arg->cfg_k = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_HIST_GAIN, 0);
	arg->k_gain = val & 0x7ff;

	for (i = 0; i < ISP39_DHAZ_BLEND_WET_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_DHAZ_BLEND_WET0 + 4 * i, 0);
		arg->blend_wet[3 * i] = val & 0x1ff;
		arg->blend_wet[3 * i + 1] = (val >> 9) & 0x1ff;
		arg->blend_wet[3 * i + 2] = (val >> 18) & 0x1ff;
	}
	val = isp3_param_read(params_vdev, ISP39_DHAZ_BLEND_WET0 + 4 * i, 0);
	arg->blend_wet[3 * i] = val & 0x1ff;
	arg->blend_wet[3 * i + 1] = (val >> 9) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_BLOCK_SIZE, 0);
	arg->blk_het = val & 0xffff;
	arg->blk_wid = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_DHAZ_THUMB_SIZE, 0);
	arg->thumb_row = val & 0xff;
	arg->thumb_col = (val >> 8) & 0xff;

	memcpy(arg->hist_iir, arg_rec->hist_iir, sizeof(arg_rec->hist_iir));
}

static void rkisp_get_params_3dlut(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp39_isp_params_cfg *params)
{
	struct isp2x_3dlut_cfg *arg = &params->others.isp3dlut_cfg;
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 i, val, *data;

	val = isp3_param_read(params_vdev, ISP3X_3DLUT_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_3DLUT;

	val = isp3_param_read(params_vdev, ISP3X_MI_LUT_3D_RD_WSIZE, 0);
	arg->actual_size = val & 0xfff;

	val = priv_val->buf_3dlut_idx[0];
	if (!priv_val->buf_3dlut[0][val].vaddr)
		return;
	data = priv_val->buf_3dlut[0][val].vaddr;
	for (i = 0; i < arg->actual_size; i++) {
		arg->lut_b[i] = data[i] & 0x3ff;
		arg->lut_g[i] = (data[i] >> 10) & 0x3ff;
		arg->lut_r[i] = (data[i] >> 22) & 0x3ff;
	}
}

static void rkisp_get_params_ldch(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp39_isp_params_cfg *params)
{
	struct isp39_ldch_cfg *arg = &params->others.ldch_cfg;
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_LDCH_STS, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_LDCH;
	arg->frm_end_dis = !!(val & BIT(1));
	arg->sample_avr_en = !!(val & BIT(3));
	arg->bic_mode_en = !!(val & BIT(4));
	arg->force_map_en = !!(val & BIT(6));
	arg->map13p3_en = !!(val & BIT(7));

	for (i = 0; i < ISP39_LDCH_BIC_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP32_LDCH_BIC_TABLE0 + i * 4, 0);
		arg->bicubic[i * 4] = val & 0xff;
		arg->bicubic[i * 4 + 1] = (val >> 8) & 0xff;
		arg->bicubic[i * 4 + 2] = (val >> 16) & 0xff;
		arg->bicubic[i * 4 + 3] = (val >> 24) & 0xff;
	}

	val = isp3_param_read(params_vdev, ISP3X_MI_LUT_LDCH_RD_H_WSIZE, 0);
	arg->hsize = val & 0xfff;
	val = isp3_param_read(params_vdev, ISP3X_MI_LUT_LDCH_RD_V_SIZE, 0);
	arg->vsize = val & 0xffff;

	val = priv_val->buf_ldch_idx[0];
	arg->buf_fd = priv_val->buf_ldch[0][val].dma_fd;
}

static void rkisp_get_params_bay3d(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp39_isp_params_cfg *params)
{
	struct isp39_bay3d_cfg *arg = &params->others.bay3d_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_BAY3D_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_BAY3D;
	arg->bypass_en = !!(val & BIT(1));
	arg->iirsparse_en = !!(val & BIT(2));

	val = isp3_param_read(params_vdev, ISP39_BAY3D_CTRL1, 0);
	arg->transf_bypass_en = !!(val & BIT(0));
	arg->sigma_curve_double_en = !!(val & BIT(1));
	arg->cur_spnr_bypass_en = !!(val & BIT(2));
	arg->cur_spnr_sigma_idxfilt_bypass_en = !!(val & BIT(3));
	arg->cur_spnr_sigma_curve_double_en = !!(val & BIT(4));
	arg->pre_spnr_bypass_en = !!(val & BIT(5));
	arg->pre_spnr_sigma_idxfilt_bypass_en = !!(val & BIT(6));
	arg->pre_spnr_sigma_curve_double_en = !!(val & BIT(7));
	arg->lpf_hi_bypass_en = !!(val & BIT(8));
	arg->lo_diff_vfilt_bypass_en = !!(val & BIT(9));
	arg->lpf_lo_bypass_en = !!(val & BIT(10));
	arg->lo_wgt_hfilt_en = !!(val & BIT(11));
	arg->lo_diff_hfilt_en = !!(val & BIT(12));
	arg->sig_hfilt_en = !!(val & BIT(13));
	arg->spnr_pre_sigma_use_en = !!(val & BIT(15));
	arg->lo_detection_mode = (val >> 16) & 0x3;
	arg->md_wgt_out_en = !!(val & BIT(18));
	arg->md_bypass_en = !!(val & BIT(19));
	arg->iirspnr_out_en = !!(val & BIT(20));
	arg->lomdwgt_dbg_en = !!(val & BIT(21));
	arg->curdbg_out_en = !!(val & BIT(22));
	arg->noisebal_mode = !!(val & BIT(23));

	val = isp3_param_read(params_vdev, ISP39_BAY3D_CTRL2, 0);
	arg->transf_mode = !!(val & BIT(0));
	arg->wgt_cal_mode = !!(val & BIT(1));
	arg->mge_wgt_ds_mode = !!(val & BIT(2));
	arg->kalman_wgt_ds_mode = !!(val & BIT(3));
	arg->mge_wgt_hdr_sht_thred = (val >> 16) & 0x3f;
	arg->sigma_calc_mge_wgt_hdr_sht_thred = (val >> 24) & 0x3f;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TRANS0, 0);
	arg->transf_mode_offset = val & 0x1fff;
	arg->transf_mode_scale = !!(val & BIT(15));
	arg->itransf_mode_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TRANS1, 0);
	arg->transf_data_max_limit = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_CURDGAIN, 0);
	arg->cur_spnr_sigma_hdr_sht_scale = val & 0xffff;
	arg->cur_spnr_sigma_scale = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_BAY3D_XY_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_BAY3D_CURSIG_X0 + i * 4, 0);
		arg->cur_spnr_luma_sigma_x[i * 2] = val & 0xffff;
		arg->cur_spnr_luma_sigma_x[i * 2 + 1] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP39_BAY3D_CURSIG_Y0 + i * 4, 0);
		arg->cur_spnr_luma_sigma_y[i * 2] = val & 0xffff;
		arg->cur_spnr_luma_sigma_y[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP39_BAY3D_CURGAIN_OFF, 0);
	arg->cur_spnr_sigma_rgain_offset = val & 0xffff;
	arg->cur_spnr_sigma_bgain_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_CURSIG_OFF, 0);
	arg->cur_spnr_sigma_hdr_sht_offset = val & 0xffff;
	arg->cur_spnr_sigma_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_CURWTH, 0);
	arg->cur_spnr_pix_diff_max_limit = val & 0xffff;
	arg->cur_spnr_wgt_cal_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_CURBFALP, 0);
	arg->cur_spnr_wgt = val & 0xffff;
	arg->pre_spnr_wgt = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_BAY3D_WD_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_BAY3D_CURWDC0 + i * 4, 0);
		arg->cur_spnr_space_rb_wgt[i * 3] = val & 0x3ff;
		arg->cur_spnr_space_rb_wgt[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->cur_spnr_space_rb_wgt[i * 3 + 2] = (val >> 20) & 0x3ff;

		val = isp3_param_read(params_vdev, ISP39_BAY3D_CURWDY0 + i * 4, 0);
		arg->cur_spnr_space_gg_wgt[i * 3] = val & 0x3ff;
		arg->cur_spnr_space_gg_wgt[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->cur_spnr_space_gg_wgt[i * 3 + 2] = (val >> 20) & 0x3ff;
	}

	val = isp3_param_read(params_vdev, ISP39_BAY3D_IIRDGAIN, 0);
	arg->pre_spnr_sigma_hdr_sht_scale = val & 0xffff;
	arg->pre_spnr_sigma_scale = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_BAY3D_XY_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_BAY3D_IIRSIG_X0 + i * 4, 0);
		arg->pre_spnr_luma_sigma_x[i * 2] = val & 0xffff;
		arg->pre_spnr_luma_sigma_x[i * 2 + 1] = (val >> 16) & 0xffff;

		val = isp3_param_read(params_vdev, ISP39_BAY3D_IIRSIG_Y0 + i * 4, 0);
		arg->cur_spnr_luma_sigma_y[i * 2] = val & 0xffff;
		arg->cur_spnr_luma_sigma_y[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP39_BAY3D_IIRGAIN_OFF, 0);
	arg->pre_spnr_sigma_rgain_offset = val & 0xffff;
	arg->pre_spnr_sigma_bgain_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_IIRSIG_OFF, 0);
	arg->pre_spnr_sigma_hdr_sht_offset = val & 0xffff;
	arg->pre_spnr_sigma_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_IIRWTH, 0);
	arg->pre_spnr_pix_diff_max_limit = val & 0xffff;
	arg->pre_spnr_wgt_cal_offset = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_BAY3D_WD_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_BAY3D_IIRWDC0 + i * 4, 0);
		arg->pre_spnr_space_rb_wgt[i * 3] = val & 0x3ff;
		arg->pre_spnr_space_rb_wgt[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->pre_spnr_space_rb_wgt[i * 3 + 2] = (val >> 20) & 0x3ff;

		val = isp3_param_read(params_vdev, ISP39_BAY3D_IIRWDY0 + i * 4, 0);
		arg->pre_spnr_space_gg_wgt[i * 3] = val & 0x3ff;
		arg->pre_spnr_space_gg_wgt[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->pre_spnr_space_gg_wgt[i * 3 + 2] = (val >> 20) & 0x3ff;
	}

	val = isp3_param_read(params_vdev, ISP39_BAY3D_BFCOEF, 0);
	arg->cur_spnr_wgt_cal_scale = val & 0xffff;
	arg->pre_spnr_wgt_cal_scale = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_BAY3D_TNRSIG_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRSIG_X0 + i * 4, 0);
		arg->tnr_luma_sigma_x[i * 2] = val & 0xffff;
		arg->tnr_luma_sigma_x[i * 2 + 1] = (val >> 16) & 0xffff;

		val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRSIG_Y0 + i * 4, 0);
		arg->tnr_luma_sigma_y[i * 2] = val & 0xffff;
		arg->tnr_luma_sigma_y[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	for (i = 0; i < ISP39_BAY3D_COEFF_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRHIW0 + i * 4, 0);
		arg->tnr_lpf_hi_coeff[i * 3] = val & 0x3ff;
		arg->tnr_lpf_hi_coeff[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tnr_lpf_hi_coeff[i * 3 + 2] = (val >> 20) & 0x3ff;

		val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRLOW0 + i * 4, 0);
		arg->tnr_lpf_lo_coeff[i * 3] = val & 0x3ff;
		arg->tnr_lpf_lo_coeff[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tnr_lpf_lo_coeff[i * 3 + 2] = (val >> 20) & 0x3ff;
	}

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRGF3, 0);
	arg->tnr_wgt_filt_coeff0 = val & 0x3ff;
	arg->tnr_wgt_filt_coeff1 = (val >> 10) & 0x3ff;
	arg->tnr_wgt_filt_coeff2 = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRSIGSCL, 0);
	arg->tnr_sigma_scale = val & 0xffff;
	arg->tnr_sigma_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRVIIR, 0);
	arg->tnr_sig_vfilt_wgt = val & 0xf;
	arg->tnr_lo_diff_vfilt_wgt = (val >> 4) & 0xf;
	arg->tnr_lo_wgt_vfilt_wgt = (val >> 8) & 0xf;
	arg->tnr_sig_first_line_scale = (val >> 16) & 0x1f;
	arg->tnr_lo_diff_first_line_scale = (val >> 24) & 0x1f;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRLFSCL, 0);
	arg->tnr_lo_wgt_cal_offset = val & 0xffff;
	arg->tnr_lo_wgt_cal_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRLFSCLTH, 0);
	arg->tnr_low_wgt_cal_max_limit = val & 0xffff;
	arg->tnr_mode0_base_ratio = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRDSWGTSCL, 0);
	arg->tnr_lo_diff_wgt_cal_offset = val & 0xffff;
	arg->tnr_lo_diff_wgt_cal_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWLSTSCL, 0);
	arg->tnr_lo_mge_pre_wgt_offset = val & 0xffff;
	arg->tnr_lo_mge_pre_wgt_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWGT0SCL0, 0);
	arg->tnr_mode0_lo_wgt_scale = val & 0xffff;
	arg->tnr_mode0_lo_wgt_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWGT1SCL1, 0);
	arg->tnr_mode1_lo_wgt_scale = val & 0xffff;
	arg->tnr_mode1_lo_wgt_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWGT1SCL2, 0);
	arg->tnr_mode1_wgt_scale = val & 0xffff;
	arg->tnr_mode1_wgt_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWGTOFF, 0);
	arg->tnr_mode1_lo_wgt_offset = val & 0xffff;
	arg->tnr_mode1_lo_wgt_hdr_sht_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWGT1OFF, 0);
	arg->tnr_auto_sigma_count_wgt_thred = val & 0x3ff;
	arg->tnr_mode1_wgt_min_limit = (val >> 10) & 0x3ff;
	arg->tnr_mode1_wgt_offset = (val >> 20) & 0xfff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRSIGORG, 0);
	arg->tnr_out_sigma_sq = val;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWLO_THL, 0);
	arg->tnr_lo_wgt_clip_min_limit = val & 0xffff;
	arg->tnr_lo_wgt_clip_hdr_sht_min_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWLO_THH, 0);
	arg->tnr_lo_wgt_clip_max_limit = val & 0xffff;
	arg->tnr_lo_wgt_clip_hdr_sht_max_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWHI_THL, 0);
	arg->tnr_hi_wgt_clip_min_limit = val & 0xffff;
	arg->tnr_hi_wgt_clip_hdr_sht_min_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRWHI_THH, 0);
	arg->tnr_hi_wgt_clip_max_limit = val & 0xffff;
	arg->tnr_hi_wgt_clip_hdr_sht_max_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRKEEP, 0);
	arg->tnr_cur_spnr_hi_wgt_min_limit = val & 0xff;
	arg->tnr_pre_spnr_hi_wgt_min_limit = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_PIXMAX, 0);
	arg->tnr_pix_max = val & 0xfff;
	arg->lowgt_ctrl = (val >> 16) & 0x3;
	arg->lowgt_offint = (val >> 18) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_SIGNUMTH, 0);
	arg->tnr_auto_sigma_count_th = val;

	val = isp3_param_read(params_vdev, ISP39_BAY3D_TNRMO_STR, 0);
	arg->tnr_motion_nr_strg = val & 0x7ff;
	arg->tnr_gain_max = (val >> 16) & 0xff;
}

static void rkisp_get_params_ynr(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp39_ynr_cfg *arg = &params->others.ynr_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_YNR_GLOBAL_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_YNR;
	arg->lospnr_bypass = !!(val & BIT(1));
	arg->hispnr_bypass = !!(val & BIT(2));
	arg->exgain_bypass = !!(val & BIT(3));
	arg->global_set_gain = (val >> 8) & 0x3ff;
	arg->gain_merge_alpha = (val >> 20) & 0xf;
	arg->rnr_en = !!(val & BIT(26));

	val = isp3_param_read(params_vdev, ISP3X_YNR_RNR_MAX_R, 0);
	arg->rnr_max_radius = val & 0xffff;
	arg->local_gain_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_YNR_RNR_CENTER_COOR, 0);
	arg->rnr_center_coorh = val & 0xffff;
	arg->rnr_center_coorv = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_YNR_LOWNR_CTRL0, 0);
	arg->ds_filt_soft_thred_scale = val & 0x1ff;
	arg->ds_img_edge_scale = (val >> 10) & 0x1f;
	arg->ds_filt_wgt_thred_scale = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP3X_YNR_LOWNR_CTRL1, 0);
	arg->ds_filt_local_gain_alpha = val & 0x1f;
	arg->ds_iir_init_wgt_scale = (val >> 8) & 0x3f;
	arg->ds_filt_center_wgt = (val >> 16) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP3X_YNR_LOWNR_CTRL2, 0);
	arg->ds_filt_inv_strg = val & 0xffff;
	arg->lospnr_wgt = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_YNR_LOWNR_CTRL3, 0);
	arg->lospnr_center_wgt = val & 0xffff;
	arg->lospnr_strg = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_YNR_LOWNR_CTRL4, 0);
	arg->lospnr_dist_vstrg_scale = val & 0xffff;
	arg->lospnr_dist_hstrg_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP39_YNR_GAUSS_COEFF, 0);
	arg->pre_filt_coeff0 = val & 0xff;
	arg->pre_filt_coeff1 = (val >> 8) & 0xff;
	arg->pre_filt_coeff2 = (val >> 16) & 0xff;

	for (i = 0; i < ISP39_YNR_LOW_GAIN_ADJ_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP39_YNR_LOW_GAIN_ADJ_0_3 + i * 4, 0);
		arg->lospnr_gain2strg_val[i * 4] = val & 0xff;
		arg->lospnr_gain2strg_val[i * 4 + 1] = (val >> 8) & 0xff;
		arg->lospnr_gain2strg_val[i * 4 + 2] = (val >> 16) & 0xff;
		arg->lospnr_gain2strg_val[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP39_YNR_LOW_GAIN_ADJ_0_3 + i * 4, 0);
	arg->lospnr_gain2strg_val[i * 4] = val & 0xff;

	for (i = 0; i < ISP39_YNR_XY_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_YNR_SGM_DX_0_1 + 4 * i, 0);
		arg->luma2sima_idx[2 * i] = val & 0xffff;
		arg->luma2sima_idx[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP39_YNR_SGM_DX_0_1 + 4 * i, 0);
	arg->luma2sima_idx[2 * i] = val & 0xffff;

	for (i = 0; i < ISP39_YNR_XY_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_YNR_LSGM_Y_0_1 + 4 * i, 0);
		arg->luma2sima_val[2 * i] = val & 0xffff;
		arg->luma2sima_val[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP39_YNR_LSGM_Y_0_1 + 4 * i, 0);
	arg->luma2sima_val[2 * i] = val & 0xffff;

	for (i = 0; i < ISP39_YNR_XY_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP39_YNR_RNR_STRENGTH03 + 4 * i, 0);
		arg->radius2strg_val[4 * i] = val & 0xff;
		arg->radius2strg_val[4 * i + 1] = (val >> 8) & 0xff;
		arg->radius2strg_val[4 * i + 2] = (val >> 16) & 0xff;
		arg->radius2strg_val[4 * i + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP39_YNR_RNR_STRENGTH03 + 4 * i, 0);
	arg->radius2strg_val[4 * i] = val & 0xff;

	val = isp3_param_read(params_vdev, ISP39_YNR_NLM_STRONG_EDGE, 0);
	arg->hispnr_strong_edge = val & 0xff;

	val = isp3_param_read(params_vdev, ISP39_YNR_NLM_SIGMA_GAIN, 0);
	arg->hispnr_sigma_min_limit = val & 0x7ff;
	arg->hispnr_local_gain_alpha = (val >> 11) & 0x1f;
	arg->hispnr_strg = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_YNR_NLM_COE, 0);
	arg->hispnr_filt_coeff[0] = val & 0xf;
	arg->hispnr_filt_coeff[1] = (val >> 4) & 0xf;
	arg->hispnr_filt_coeff[2] = (val >> 8) & 0xf;
	arg->hispnr_filt_coeff[3] = (val >> 12) & 0xf;
	arg->hispnr_filt_coeff[4] = (val >> 16) & 0xf;
	arg->hispnr_filt_coeff[5] = (val >> 20) & 0xf;

	val = isp3_param_read(params_vdev, ISP39_YNR_NLM_WEIGHT, 0);
	arg->hispnr_filt_wgt_offset = val & 0x3ff;
	arg->hispnr_filt_center_wgt = (val >> 10) & 0x3ffff;

	val = isp3_param_read(params_vdev, ISP39_YNR_NLM_NR_WEIGHT, 0);
	arg->hispnr_filt_wgt = val & 0x7ff;
	arg->hispnr_gain_thred = (val >> 16) & 0x3ff;
}

static void rkisp_get_params_cnr(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp39_cnr_cfg *arg = &params->others.cnr_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_CNR_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_CNR;
	arg->exgain_bypass = !!(val & BIT(1));
	arg->yuv422_mode = !!(val & BIT(2));
	arg->thumb_mode = (val >> 4) & 0x3;
	arg->hiflt_wgt0_mode = !!(val & BIT(8));
	arg->loflt_coeff = (val >> 12) & 0x3f;

	val = isp3_param_read(params_vdev, ISP3X_CNR_EXGAIN, 0);
	arg->global_gain = val & 0x3ff;
	arg->global_gain_alpha = (val >> 12) & 0xf;
	arg->local_gain_scale = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP32_CNR_THUMB1, 0);
	arg->lobfflt_vsigma_uv = val & 0xffff;
	arg->lobfflt_vsigma_y = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP32_CNR_THUMB_BF_RATIO, 0);
	arg->lobfflt_alpha = val & 0x7ff;

	val = isp3_param_read(params_vdev, ISP32_CNR_LBF_WEITD, 0);
	arg->thumb_bf_coeff0 = val & 0xff;
	arg->thumb_bf_coeff1 = (val >> 8) & 0xff;
	arg->thumb_bf_coeff2 = (val >> 16) & 0xff;
	arg->thumb_bf_coeff3 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP32_CNR_IIR_PARA1, 0);
	arg->loflt_uv_gain = val & 0xf;
	arg->loflt_vsigma = (val >> 4) & 0xff;
	arg->exp_x_shift_bit = (val >> 12) & 0x3f;
	arg->loflt_wgt_slope = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP32_CNR_IIR_PARA2, 0);
	arg->loflt_wgt_min_thred = val & 0x3f;
	arg->loflt_wgt_max_limit = (val >> 8) & 0x7f;

	val = isp3_param_read(params_vdev, ISP32_CNR_GAUS_COE1, 0);
	arg->gaus_flt_coeff[0] = val & 0xff;
	arg->gaus_flt_coeff[1] = (val >> 8) & 0xff;
	arg->gaus_flt_coeff[2] = (val >> 16) & 0xff;
	arg->gaus_flt_coeff[3] = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP32_CNR_GAUS_COE2, 0);
	arg->gaus_flt_coeff[4] = val & 0xff;
	arg->gaus_flt_coeff[5] = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP32_CNR_GAUS_RATIO, 0);
	arg->gaus_flt_alpha = val & 0x7ff;
	arg->hiflt_wgt_min_limit = (val >> 12) & 0xff;
	arg->hiflt_alpha = (val >> 20) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP32_CNR_BF_PARA1, 0);
	arg->hiflt_uv_gain = val & 0x7f;
	arg->hiflt_global_vsigma = (val >> 8) & 0x3fff;
	arg->hiflt_cur_wgt = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP32_CNR_BF_PARA2, 0);
	arg->adj_offset = val & 0x1ff;
	arg->adj_scale = (val >> 16) & 0x7fff;

	for (i = 0; i < ISP39_CNR_SIGMA_Y_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP32_CNR_SIGMA0 + i * 4, 0);
		arg->sgm_ratio[i * 4] = val & 0xff;
		arg->sgm_ratio[i * 4 + 1] = (val >> 8) & 0xff;
		arg->sgm_ratio[i * 4 + 2] = (val >> 16) & 0xff;
		arg->sgm_ratio[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP32_CNR_SIGMA0 + i * 4, 0);
	arg->sgm_ratio[i * 4] = val & 0xff;

	val = isp3_param_read(params_vdev, ISP32_CNR_IIR_GLOBAL_GAIN, 0);
	arg->loflt_global_sgm_ratio = val & 0xff;
	arg->loflt_global_sgm_ratio_alpha = (val >> 8) & 0xff;

	for (i = 0; i < ISP39_CNR_WGT_SIGMA_Y_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP39_CNR_WGT_SIGMA0 + i * 4, 0);
		arg->cur_wgt[i * 4] = val & 0xff;
		arg->cur_wgt[i * 4 + 1] = (val >> 8) & 0xff;
		arg->cur_wgt[i * 4 + 2] = (val >> 16) & 0xff;
		arg->cur_wgt[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP39_CNR_WGT_SIGMA0 + i * 4, 0);
	arg->cur_wgt[i * 4] = val & 0xff;

	for (i = 0; i < ISP39_CNR_GAUS_SIGMAR_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_CNR_GAUS_X_SIGMAR0 + i * 4, 0);
		arg->hiflt_vsigma_idx[i * 3] = val & 0x3ff;
		arg->hiflt_vsigma_idx[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->hiflt_vsigma_idx[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_CNR_GAUS_X_SIGMAR0 + i * 4, 0);
	arg->hiflt_vsigma_idx[i * 3] = val & 0x3ff;
	arg->hiflt_vsigma_idx[i * 3 + 1] = (val >> 20) & 0x3ff;

	for (i = 0; i < ISP39_CNR_GAUS_SIGMAR_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_CNR_GAUS_Y_SIGMAR0 + i * 4, 0);
		arg->hiflt_vsigma[i * 2] = val & 0xffff;
		arg->hiflt_vsigma[i * 2 + 1] = (val >> 16) & 0xffff;
	}
}

static void rkisp_get_params_sharp(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp39_isp_params_cfg *params)
{
	struct isp39_sharp_cfg *arg = &params->others.sharp_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_EN, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_SHARP;
	arg->bypass = !!(val & BIT(1));
	arg->center_mode = !!(val & BIT(2));
	arg->local_gain_bypass = !!(val & BIT(3));
	arg->radius_step_mode = !!(val & BIT(4));
	arg->noise_clip_mode = !!(val & BIT(5));
	arg->clipldx_sel = !!(val & BIT(6));
	arg->baselmg_sel = !!(val & BIT(7));
	arg->noise_filt_sel = !!(val & BIT(8));
	arg->tex2wgt_en = !!(val & BIT(9));

	val = isp3_param_read(params_vdev, ISP39_SHARP_ALPHA, 0);
	arg->pre_bifilt_alpha = val & 0xff;
	arg->guide_filt_alpha = (val >> 8) & 0xff;
	arg->detail_bifilt_alpha = (val >> 16) & 0xff;
	arg->global_sharp_strg = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_LUMA_DX, 0);
	arg->luma2table_idx[0] = val & 0xf;
	arg->luma2table_idx[1] = (val >> 4) & 0xf;
	arg->luma2table_idx[2] = (val >> 8) & 0xf;
	arg->luma2table_idx[3] = (val >> 12) & 0xf;
	arg->luma2table_idx[4] = (val >> 16) & 0xf;
	arg->luma2table_idx[5] = (val >> 20) & 0xf;
	arg->luma2table_idx[6] = (val >> 24) & 0xf;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_PBF_SIGMA_INV_0, 0);
	arg->pbf_sigma_inv[0] = val & 0x3ff;
	arg->pbf_sigma_inv[1] = (val >> 10) & 0x3ff;
	arg->pbf_sigma_inv[2] = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_PBF_SIGMA_INV_1, 0);
	arg->pbf_sigma_inv[3] = val & 0x3ff;
	arg->pbf_sigma_inv[4] = (val >> 10) & 0x3ff;
	arg->pbf_sigma_inv[5] = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_PBF_SIGMA_INV_2, 0);
	arg->pbf_sigma_inv[6] = val & 0x3ff;
	arg->pbf_sigma_inv[7] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_BF_SIGMA_INV_0, 0);
	arg->bf_sigma_inv[0] = val & 0x3ff;
	arg->bf_sigma_inv[1] = (val >> 10) & 0x3ff;
	arg->bf_sigma_inv[2] = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_BF_SIGMA_INV_1, 0);
	arg->bf_sigma_inv[3] = val & 0x3ff;
	arg->bf_sigma_inv[4] = (val >> 10) & 0x3ff;
	arg->bf_sigma_inv[5] = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_BF_SIGMA_INV_2, 0);
	arg->bf_sigma_inv[6] = val & 0x3ff;
	arg->bf_sigma_inv[7] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_SIGMA_SHIFT, 0);
	arg->pbf_sigma_shift = val & 0xf;
	arg->bf_sigma_shift = (val >> 4) & 0xf;

	for (i = 0; i < ISP39_SHARP_Y_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_SHARP_LOCAL_STRG_0 + i * 4, 0);
		arg->luma2strg_val[i * 3] = val & 0x3ff;
		arg->luma2strg_val[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->luma2strg_val[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_SHARP_LOCAL_STRG_0 + i * 4, 0);
	arg->luma2strg_val[i * 3] = val & 0x3ff;
	arg->luma2strg_val[i * 3 + 1] = (val >> 10) & 0x3ff;

	for (i = 0; i < ISP39_SHARP_Y_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_SHARP_POS_CLIP_0 + i * 4, 0);
		arg->luma2posclip_val[i * 3] = val & 0x3ff;
		arg->luma2posclip_val[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->luma2posclip_val[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_SHARP_POS_CLIP_0 + i * 4, 0);
	arg->luma2posclip_val[i * 3] = val & 0x3ff;
	arg->luma2posclip_val[i * 3 + 1] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_PBF_COEF, 0);
	arg->pbf_coef0 = val & 0xff;
	arg->pbf_coef1 = (val >> 8) & 0xff;
	arg->pbf_coef2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_SHARP_DETAILBF_COEF, 0);
	arg->bf_coef0 = val & 0xff;
	arg->bf_coef1 = (val >> 8) & 0xff;
	arg->bf_coef2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_IMGLPF_COEF_0, 0);
	arg->img_lpf_coeff[0] = val & 0xff;
	arg->img_lpf_coeff[1] = (val >> 8) & 0xff;
	arg->img_lpf_coeff[2] = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_IMGLPF_COEF_1, 0);
	arg->img_lpf_coeff[3] = val & 0xff;
	arg->img_lpf_coeff[4] = (val >> 8) & 0xff;
	arg->img_lpf_coeff[5] = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP32_SHARP_GAIN, 0);
	arg->global_gain = val & 0x3ff;
	arg->gain_merge_alpha = (val >> 16) & 0xf;
	arg->local_gain_scale = (val >> 24) & 0xff;

	for (i = 0; i < ISP39_SHARP_GAIN_ADJ_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP32_SHARP_GAIN_ADJUST0 + i * 4, 0);
		arg->gain2strg_val[i * 2] = val & 0xffff;
		arg->gain2strg_val[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP32_SHARP_CENTER, 0);
	arg->center_x = val & 0xffff;
	arg->center_y = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_SHARP_STRENGTH_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP32_SHARP_GAIN_DIS_STRENGTH0 + i * 4, 0);
		arg->distance2strg_val[i * 4] = val & 0xff;
		arg->distance2strg_val[i * 4 + 1] = (val >> 8) & 0xff;
		arg->distance2strg_val[i * 4 + 2] = (val >> 16) & 0xff;
		arg->distance2strg_val[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP32_SHARP_GAIN_DIS_STRENGTH0 + i * 4, 0);
	arg->distance2strg_val[i * 4] = val & 0xff;
	arg->distance2strg_val[i * 4 + 1] = (val >> 8) & 0xff;

	for (i = 0; i < ISP39_SHARP_Y_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_SHARP_CLIP_NEG_0 + i * 4, 0);
		arg->luma2neg_clip_val[i * 3] = val & 0x3ff;
		arg->luma2neg_clip_val[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->luma2neg_clip_val[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_SHARP_CLIP_NEG_0 + i * 4, 0);
	arg->luma2neg_clip_val[i * 3] = val & 0x3ff;
	arg->luma2neg_clip_val[i * 3 + 1] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_SHARP_TEXTURE0, 0);
	arg->noise_max_limit = val & 0x3ff;
	arg->tex_reserve_level = (val >> 12) & 0xf;

	val = isp3_param_read(params_vdev, ISP39_SHARP_TEXTURE1, 0);
	arg->tex_wet_scale = val & 0x7fff;
	arg->noise_norm_bit = (val >> 16) & 0xf;
	arg->tex_wgt_mode = (val >> 20) & 0x3;

	for (i = 0; i < ISP39_SHARP_TEX_WET_LUT_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_SHARP_TEXTURE_LUT0 + i * 4, 0);
		arg->tex2wgt_val[i * 3] = val & 0x3ff;
		arg->tex2wgt_val[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tex2wgt_val[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_SHARP_TEXTURE_LUT0 + i * 4, 0);
	arg->tex2wgt_val[i * 3] = val & 0x3ff;
	arg->tex2wgt_val[i * 3 + 1] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_SHARP_TEXTURE2, 0);
	arg->noise_strg = val;

	for (i = 0; i < ISP39_SHARP_DETAIL_STRG_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_SHARP_DETAIL_STRG_LUT0 + i * 4, 0);
		arg->detail2strg_val[i * 2] = val & 0x7ff;
		arg->detail2strg_val[i * 2 + 1] = (val >> 16) & 0x7ff;
	}
	val = isp3_param_read(params_vdev, ISP39_SHARP_DETAIL_STRG_LUT0 + i * 4, 0);
	arg->detail2strg_val[i * 2] = val & 0x7ff;
}

static void rkisp_get_params_cac(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp32_cac_cfg *arg = &params->others.cac_cfg;
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_CAC_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_CAC;
	arg->bypass_en = !!(val & (BIT(1) | BIT(30)));
	arg->center_en = !!(val & BIT(3));
	arg->clip_g_mode = (val >> 5) & 0x3;
	arg->edge_detect_en = !!(val & BIT(7));
	arg->neg_clip0_en = !!(val & BIT(9));

	val = isp3_param_read(params_vdev, ISP3X_CAC_PSF_PARA, 0);
	arg->psf_sft_bit = val & 0xff;
	arg->cfg_num = (val >> 8) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP3X_CAC_STRENGTH_CENTER, 0);
	arg->center_width = val & 0xffff;
	arg->center_height = (val >> 16) & 0xffff;

	for (i = 0; i < ISP39_CAC_STRENGTH_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_CAC_STRENGTH0 + i * 4, 0);
		arg->strength[2 * i] = val & 0xffff;
		arg->strength[2 * i + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP32_CAC_FLAT_THED, 0);
	arg->flat_thed_b = val & 0x1f;
	arg->flat_thed_r = (val >> 8) & 0x1f;

	val = isp3_param_read(params_vdev, ISP32_CAC_OFFSET, 0);
	arg->offset_b = val & 0xffff;
	arg->offset_r = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP32_CAC_EXPO_THED_B, 0);
	arg->expo_thed_b = val & 0x1fffff;

	val = isp3_param_read(params_vdev, ISP32_CAC_EXPO_THED_R, 0);
	arg->expo_thed_r = val & 0x1fffff;

	val = isp3_param_read(params_vdev, ISP32_CAC_EXPO_ADJ_B, 0);
	arg->expo_adj_b = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP32_CAC_EXPO_ADJ_R, 0);
	arg->expo_adj_r = val & 0xfffff;

	val = isp3_param_read(params_vdev, ISP3X_MI_LUT_CAC_RD_H_WSIZE, 0);
	arg->hsize = val & 0xffff;
	val = isp3_param_read(params_vdev,  ISP3X_MI_LUT_CAC_RD_V_SIZE, 0);
	arg->vsize = val & 0xffff;

	val = priv_val->buf_cac_idx[0];
	arg->buf_fd = priv_val->buf_cac[0][val].dma_fd;
}

static void rkisp_get_params_gain(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp39_isp_params_cfg *params)
{
	struct isp3x_gain_cfg *arg = &params->others.gain_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_GAIN_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_GAIN;

	val = isp3_param_read(params_vdev, ISP3X_GAIN_G0, 0);
	arg->g0 = val & 0x3ffff;

	val = isp3_param_read(params_vdev, ISP3X_GAIN_G1_G2, 0);
	arg->g1 = val & 0xffff;
	arg->g2 = (val >> 16) & 0xffff;
}

static void rkisp_get_params_csm(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp21_csm_cfg *arg = &params->others.csm_cfg;
	u32 i, val;

	for (i = 0; i < ISP39_CSM_COEFF_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_ISP_CC_COEFF_0 + i * 4, 0);
		if (i == 0) {
			arg->csm_c_offset = (val >> 16) & 0xff;
			arg->csm_y_offset = (val >> 24) & 0x3f;
		}
		arg->csm_coeff[i] = val & 0x1ff;
	}
}

static void rkisp_get_params_cgc(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp39_isp_params_cfg *params)
{
	struct isp21_cgc_cfg *arg = &params->others.cgc_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_ISP_CTRL0, 0);
	arg->yuv_limit = !!(val & ISP3X_SW_CGC_YUV_LIMIT);
	arg->ratio_en = !!(val & ISP3X_SW_CGC_RATIO_EN);
}

static void rkisp_get_params_ie(struct rkisp_isp_params_vdev *params_vdev,
				struct isp39_isp_params_cfg *params)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_IMG_EFF_CTRL, 0);

	if (val & ISP39_MODULE_EN)
		params->module_ens |= ISP39_MODULE_IE;
}

static void rkisp_get_params_yuvme(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp39_isp_params_cfg *params)
{
	struct isp39_yuvme_cfg *arg = &params->others.yuvme_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP39_YUVME_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_YUVME;
	arg->bypass = !!(val & BIT(1));
	arg->tnr_wgt0_en = !!(val & BIT(4));

	val = isp3_param_read(params_vdev, ISP39_YUVME_PARA0, 0);
	arg->global_nr_strg = val & 0xff;
	arg->wgt_fact3 = (val >> 8) & 0xff;
	arg->search_range_mode = (val >> 16) & 0xf;
	arg->static_detect_thred = (val >> 20) & 0x3f;

	val = isp3_param_read(params_vdev, ISP39_YUVME_PARA1, 0);
	arg->time_relevance_offset = val & 0xf;
	arg->space_relevance_offset = (val >> 4) & 0xf;
	arg->nr_diff_scale = (val >> 8) & 0xff;
	arg->nr_fusion_limit = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_YUVME_PARA2, 0);
	arg->nr_static_scale = val & 0xff;
	arg->nr_motion_scale = (val >> 8) & 0x1ff;
	arg->nr_fusion_mode = (val >> 17) & 0x3;
	arg->cur_weight_limit = (val >> 20) & 0x7ff;

	for (i = 0; i < ISP39_YUVME_SIGMA_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_YUVME_SIGMA0 + i * 4, 0);
		arg->nr_luma2sigma_val[i * 3] = val & 0x3ff;
		arg->nr_luma2sigma_val[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->nr_luma2sigma_val[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_YUVME_SIGMA0 + i * 4, 0);
	arg->nr_luma2sigma_val[i * 3] = val & 0x3ff;
}

static void rkisp_get_params_ldcv(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp39_isp_params_cfg *params)
{
	struct isp39_ldcv_cfg *arg = &params->others.ldcv_cfg;
	struct rkisp_isp_params_val_v39 *priv_val = params_vdev->priv_val;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP39_LDCV_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_LDCV;
	arg->thumb_mode = !!(val & BIT(1));
	arg->dth_bypass = !!(val & BIT(6));
	arg->force_map_en = !!(val & BIT(8));
	arg->map13p3_en = !!(val & BIT(9));

	for (i = 0; i < ISP39_LDCV_BIC_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP39_LDCV_BIC_TABLE0 + i * 4, 0);
		arg->bicubic[i * 4] = val & 0xff;
		arg->bicubic[i * 4 + 1] = (val >> 8) & 0xff;
		arg->bicubic[i * 4 + 2] = (val >> 16) & 0xff;
		arg->bicubic[i * 4 + 3] = (val >> 24) & 0xff;
	}

	val = isp3_param_read(params_vdev, ISP32L_IRLDCV_RD_H_WSIZE, 0);
	arg->hsize = val & 0xffff;
	val = isp3_param_read(params_vdev, ISP32L_IRLDCV_RD_V_SIZE, 0);
	arg->vsize = val & 0xffff;

	arg->out_vsize = priv_val->ldcv_out_vsize;
	val = priv_val->buf_ldcv_idx[0];
	arg->buf_fd = priv_val->buf_ldcv[0][val].dma_fd;
}

static void rkisp_get_params_rgbir(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp39_isp_params_cfg *params)
{
	struct isp39_rgbir_cfg *arg = &params->others.rgbir_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP39_RGBIR_CTRL, 0);
	if (!(val & ISP39_MODULE_EN))
		return;
	params->module_ens |= ISP39_MODULE_RGBIR;

	val = isp3_param_read(params_vdev, ISP39_RGBIR_THETA, 0);
	arg->coe_theta = val & 0xfff;

	val = isp3_param_read(params_vdev, ISP39_RGBIR_DELTA, 0);
	arg->coe_delta = val & 0x3fff;

	for (i = 0; i < ISP39_RGBIR_SCALE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP39_RGBIR_SCALE0 + i * 4, 0);
		arg->scale[i] = val & 0x1ff;
	}

	for (i = 0; i < ISP39_RGBIR_LUMA_POINT_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_RGBIR_LUMA_POINT0 + i * 4, 0);
		arg->luma_point[i * 3] = val & 0x3ff;
		arg->luma_point[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->luma_point[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_RGBIR_LUMA_POINT0 + i * 4, 0);
	arg->luma_point[i * 3] = val & 0x3ff;
	arg->luma_point[i * 3 + 1] = (val >> 10) & 0x7ff;

	for (i = 0; i < ISP39_RGBIR_SCALE_MAP_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_RGBIR_SCALE_MAP0 + i * 4, 0);
		arg->scale_map[i * 3] = val & 0x1ff;
		arg->scale_map[i * 3 + 1] = (val >> 9) & 0x1ff;
		arg->scale_map[i * 3 + 2] = (val >> 18) & 0x1ff;
	}
	val = isp3_param_read(params_vdev, ISP39_RGBIR_SCALE_MAP0 + i * 4, 0);
	arg->scale_map[i * 3] = val & 0x1ff;
	arg->scale_map[i * 3 + 1] = (val >> 9) & 0x1ff;
}

int rkisp_get_params_v39(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	struct isp39_isp_params_cfg *params = arg;

	if (!params)
		return -EINVAL;
	memset(params, 0, sizeof(struct isp39_isp_params_cfg));
	rkisp_get_params_rawaf(params_vdev, params);
	rkisp_get_params_rawawb(params_vdev, params);
	rkisp_get_params_rawae0(params_vdev, params);
	rkisp_get_params_rawae3(params_vdev, params);
	rkisp_get_params_rawhist0(params_vdev, params);
	rkisp_get_params_rawhist3(params_vdev, params);

	rkisp_get_params_bls(params_vdev, params);
	rkisp_get_params_dpcc(params_vdev, params);
	rkisp_get_params_lsc(params_vdev, params);
	rkisp_get_params_awbgain(params_vdev, params);
	rkisp_get_params_gic(params_vdev, params);
	rkisp_get_params_debayer(params_vdev, params);
	rkisp_get_params_ccm(params_vdev, params);
	rkisp_get_params_gammaout(params_vdev, params);
	rkisp_get_params_cproc(params_vdev, params);
	rkisp_get_params_sdg(params_vdev, params);
	rkisp_get_params_drc(params_vdev, params);
	rkisp_get_params_hdrmge(params_vdev, params);
	rkisp_get_params_dhaz(params_vdev, params);
	rkisp_get_params_3dlut(params_vdev, params);
	rkisp_get_params_ldch(params_vdev, params);
	rkisp_get_params_bay3d(params_vdev, params);
	rkisp_get_params_ynr(params_vdev, params);
	rkisp_get_params_cnr(params_vdev, params);
	rkisp_get_params_sharp(params_vdev, params);
	rkisp_get_params_cac(params_vdev, params);
	rkisp_get_params_gain(params_vdev, params);
	rkisp_get_params_csm(params_vdev, params);
	rkisp_get_params_cgc(params_vdev, params);
	rkisp_get_params_ie(params_vdev, params);
	rkisp_get_params_yuvme(params_vdev, params);
	rkisp_get_params_ldcv(params_vdev, params);
	rkisp_get_params_rgbir(params_vdev, params);
	return 0;
}
#endif
