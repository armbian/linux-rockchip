// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include <media/v4l2-common.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>
#include "dev.h"
#include "regs.h"
#include "isp_params_v35.h"

#define ISP35_MODULE_EN				BIT(0)
#define ISP35_SELF_FORCE_UPD			BIT(31)

static inline void
isp3_param_write_direct(struct rkisp_isp_params_vdev *params_vdev,
			u32 value, u32 addr)
{
	void __iomem *base = params_vdev->dev->hw_dev->base_addr;

	writel(value, base + addr);
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
	for (i = 0; i < ISP35_DPCC_PDAF_POINT_NUM; i++)
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

	for (i = 0; i < ISP35_DPCC_PDAF_POINT_NUM / 2; i++) {
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
	u32 val = isp3_param_read(params_vdev, ISP3X_DPCC0_MODE, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_DPCC0_MODE, id);
	isp3_param_write(params_vdev, val, ISP3X_DPCC1_MODE, id);
}

static void
isp_bls_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp35_bls_cfg *arg,
	       enum rkisp_params_type type, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	const struct isp2x_bls_fixed_val *pval;
	unsigned long lock_flags = 0;
	u32 new_control, value;
	bool is_lock = false;

	if (!dev->is_aiisp_en ||
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
		if (type == RKISP_PARAMS_LAT) {
			spin_lock_irqsave(&dev->hw_dev->reg_lock, lock_flags);
			value = isp3_param_read(params_vdev, ISP32_BLS_ISP_OB_OFFSET, id);
			value &= 0xffff;
			value |= arg->isp_ob_offset1 << 16;
			isp3_param_write(params_vdev, value, ISP32_BLS_ISP_OB_OFFSET, id);
			spin_unlock_irqrestore(&dev->hw_dev->reg_lock, lock_flags);
			return;
		}
	}

	new_control = isp3_param_read(params_vdev, ISP3X_BLS_CTRL, id);
	new_control &= (ISP_BLS_ENA | ISP32_BLS_BLS2_EN | ISP35_BLS_BLS3_EN);
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
			value = arg->bls_window2.h_offs;
			isp3_param_write(params_vdev, value, ISP3X_BLS_H2_START, id);
			value = arg->bls_window2.h_offs + arg->bls_window2.h_size;
			isp3_param_write(params_vdev, value, ISP3X_BLS_H2_STOP, id);
			value = arg->bls_window2.v_offs;
			isp3_param_write(params_vdev, value, ISP3X_BLS_V2_START, id);
			value = arg->bls_window2.v_offs + arg->bls_window2.v_size;
			isp3_param_write(params_vdev, value, ISP3X_BLS_V2_STOP, id);
			new_control |= ISP_BLS_WINDOW_2;
		}

		if (arg->en_windows & BIT(0)) {
			value = arg->bls_window1.h_offs;
			isp3_param_write(params_vdev, value, ISP3X_BLS_H1_START, id);
			value = arg->bls_window1.h_offs + arg->bls_window1.h_size;
			isp3_param_write(params_vdev, value, ISP3X_BLS_H1_STOP, id);
			value = arg->bls_window1.v_offs;
			isp3_param_write(params_vdev, value, ISP3X_BLS_V1_START, id);
			value = arg->bls_window1.v_offs + arg->bls_window1.v_size;
			isp3_param_write(params_vdev, value, ISP3X_BLS_V1_STOP, id);
			new_control |= ISP_BLS_WINDOW_1;
		}

		isp3_param_write(params_vdev, arg->bls_samples, ISP3X_BLS_SAMPLES, id);

		new_control |= ISP_BLS_MODE_MEASURED;
	}
	isp3_param_write(params_vdev, new_control, ISP3X_BLS_CTRL, id);

	isp3_param_write(params_vdev, arg->isp_ob_predgain, ISP32_BLS_ISP_OB_PREDGAIN, id);
	isp3_param_write(params_vdev, arg->isp_ob_max, ISP32_BLS_ISP_OB_MAX, id);

	if (dev->is_aiisp_en && type != RKISP_PARAMS_ALL)
		is_lock = true;
	if (is_lock) {
		spin_lock_irqsave(&dev->hw_dev->reg_lock, lock_flags);

		value = isp3_param_read(params_vdev, ISP32_BLS_ISP_OB_OFFSET, id);
		value &= 0xffff0000;
		value |= arg->isp_ob_offset;
	} else {
		value = ISP_PACK_2SHORT(arg->isp_ob_offset, arg->isp_ob_offset1);
	}
	isp3_param_write(params_vdev, value, ISP32_BLS_ISP_OB_OFFSET, id);
	if (is_lock)
		spin_unlock_irqrestore(&dev->hw_dev->reg_lock, lock_flags);
}

static void
isp_bls_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_BLS_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_BLS_CTRL, id);
}

static void
isp_lsc_matrix_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
			const struct isp3x_lsc_cfg *pconfig,
			bool is_check, u32 id)
{
	u32 data = isp3_param_read(params_vdev, ISP3X_LSC_CTRL, id);
	int i, j;

	if (is_check && !(data & ISP35_MODULE_EN))
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
isp_lsc_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp3x_lsc_cfg *arg, u32 id)
{
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct rkisp_device *dev = params_vdev->dev;
	u32 i, data, ctrl;

	ctrl = isp3_param_read(params_vdev, ISP3X_LSC_CTRL, id);
	if (!(ctrl & ISP35_MODULE_EN))
		isp3_param_clear_bits(params_vdev, ISP3X_VI_ISP_PATH, ISP3X_LSC_CFG_SEL(3), id);
	ctrl &= (ISP35_MODULE_EN | ISP3X_LSC_PRE_RD_ST_MODE);
	ctrl |= !!arg->sector_16x16 << 2;
	isp3_param_write(params_vdev, ctrl, ISP3X_LSC_CTRL, id);

	for (i = 0; i < ISP35_LSC_SIZE_TBL_SIZE / 4; i++) {
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

	if (dev->hw_dev->is_single &&
	    (!(dev->isp_state & ISP_START) || ctrl & ISP35_MODULE_EN))
		isp_lsc_matrix_cfg_sram(params_vdev, arg, false, id);
	params_rec->others.lsc_cfg = *arg;
}

static void
isp_lsc_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	u32 val = isp3_param_read(params_vdev, ISP3X_LSC_CTRL, id);
	u32 path_sel;

	if (en == !!(val & ISP35_MODULE_EN))
		return;

	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);

	if (dev->is_aiisp_en && !dev->is_aiisp_sync) {
		val &= ~ISP3X_LSC_PRE_RD_ST_MODE;

		path_sel = isp3_param_read_cache(params_vdev, ISP3X_VI_ISP_PATH, id);
		/* drcLSC default frame end read table */
		path_sel |= ISP3X_LSC_CFG_SEL(3);
		isp3_param_write(params_vdev, path_sel, ISP3X_VI_ISP_PATH, id);
		isp3_param_write(params_vdev, val, ISP3X_LSC_CTRL, id);
		/* awbLSC default frame end read table */
		path_sel &= ~ISP3X_LSC_CFG_SEL(3);
		path_sel |= ISP3X_LSC_CFG_SEL(2);
		isp3_param_write(params_vdev, path_sel, ISP3X_VI_ISP_PATH, id);
		isp3_param_write(params_vdev, val, ISP3X_LSC_CTRL, id);
		/* mainLSC default frame start read table and change to frame end */
		path_sel &= ~ISP3X_LSC_CFG_SEL(3);
		path_sel |= ISP3X_LSC_CFG_SEL(1);
		isp3_param_write(params_vdev, path_sel, ISP3X_VI_ISP_PATH, id);

		val |= ISP3X_LSC_PRE_RD_ST_MODE;
	}
	isp3_param_write(params_vdev, val, ISP3X_LSC_CTRL, id);
}

static void
isp_debayer_config(struct rkisp_isp_params_vdev *params_vdev,
		   const struct isp35_debayer_cfg *arg, u32 id)
{
	u32 i, value;

	value = isp3_param_read(params_vdev, ISP3X_DEBAYER_CONTROL, id);
	value &= ISP_DEBAYER_EN;

	value |= !!arg->bypass << 1 | !!arg->g_out_flt_en << 4 |
		 !!arg->cnt_flt_en << 8;
	isp3_param_write(params_vdev, value, ISP3X_DEBAYER_CONTROL, id);

	value = 0;
	for (i = 0; i < ISP35_DEBAYER_LUMA_NUM; i++)
		value |= ((arg->luma_dx[i] & 0xf) << (i * 4));
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_LUMA_DX, id);

	value = (arg->g_interp_sharp_strg_max_limit & 0x3F) << 24 | arg->drct_method_thred << 16 |
		(arg->lo_drct_thred & 0x0F) << 12 | (arg->hi_drct_thred & 0x0F) << 8 |
		(arg->hi_texture_thred & 0x0F) << 4 | !!arg->g_interp_clip_en;
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP, id);

	value = (arg->lo_drct_flt_coeff4 & 0x1F) << 24 | (arg->lo_drct_flt_coeff3 & 0x1F) << 16 |
		(arg->lo_drct_flt_coeff2 & 0x1F) << 8 | (arg->lo_drct_flt_coeff1 & 0x1F);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP_FILTER1, id);

	value = (arg->hi_drct_flt_coeff4 & 0x1F) << 24 | (arg->hi_drct_flt_coeff3 & 0x1F) << 16 |
		(arg->hi_drct_flt_coeff2 & 0x1F) << 8 | (arg->hi_drct_flt_coeff1 & 0x1F);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP_FILTER2, id);

	value = (arg->grad_lo_flt_alpha & 0x7f) << 16 | (arg->g_interp_sharp_strg_offset & 0xfff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP_OFFSET_ALPHA, id);

	for (i = 0; i < ISP35_DEBAYER_DRCT_OFFSET_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->drct_offset[i * 2], arg->drct_offset[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_INTERP_DRCT_OFFSET0 + i * 4, id);
	}

	value = (arg->gflt_offset & 0x7ff) << 16 | (arg->gflt_ratio & 0x7ff) << 4 | !!arg->gflt_mode;
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_FILTER_MODE_OFFSET, id);

	value = ISP_PACK_4BYTE(arg->gflt_coe0, arg->gflt_coe1, arg->gflt_coe2, 0);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_FILTER_FILTER, id);

	for (i = 0; i < ISP35_DEBAYER_VSIGMA_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->gflt_vsigma[i * 2], arg->gflt_vsigma[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_DEBAYER_G_FILTER_VSIGMA0 + i * 4, id);
	}

	value = ISP_PACK_4BYTE(arg->cnr_lo_guide_lpf_coe0, arg->cnr_lo_guide_lpf_coe1,
			       arg->cnr_lo_guide_lpf_coe2, 0);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_GUIDE_GAUS, id);

	value = ISP_PACK_4BYTE(arg->cnr_pre_flt_coe0, arg->cnr_pre_flt_coe1,
			       arg->cnr_pre_flt_coe2, 0);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_CE_GAUS, id);

	value = ISP_PACK_4BYTE(arg->cnr_alpha_lpf_coe0, arg->cnr_alpha_lpf_coe1,
			       arg->cnr_alpha_lpf_coe2, 0);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_ALPHA_GAUS, id);

	value = !!arg->cnr_trans_en << 31 | (arg->cnr_log_guide_offset & 0xfff) << 16 |
		(arg->cnr_log_grad_offset & 0x1fff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_LOG_OFFSET, id);

	value = (arg->cnr_moire_alpha_scale & 0xfffff) << 12 | (arg->cnr_moire_alpha_offset & 0xfff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_ALPHA, id);

	value = (arg->cnr_edge_alpha_scale & 0xfffff) << 12 | (arg->cnr_edge_alpha_offset & 0xfff);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_EDGE, id);

	value = (arg->cnr_lo_flt_wgt_slope & 0xfff) << 16 |
		(arg->cnr_lo_flt_strg_shift & 0x3f) << 8 | arg->cnr_lo_flt_strg_inv;
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_IIR_0, id);

	value = (arg->cnr_lo_flt_wgt_min_thred & 0x3f) << 8 | (arg->cnr_lo_flt_wgt_max_limit & 0x7f);
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_IIR_1, id);

	value = (arg->cnr_hi_flt_cur_wgt & 0x7f) << 24 |
		(arg->cnr_hi_flt_wgt_min_limit & 0x7f) << 16 | arg->cnr_hi_flt_vsigma;
	isp3_param_write(params_vdev, value, ISP39_DEBAYER_C_FILTER_BF, id);
}

static void
isp_debayer_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_DEBAYER_CONTROL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_DEBAYER_CONTROL, id);
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
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_ISP_CTRL0, id);

	if (en == !!(val & CIF_ISP_CTRL_ISP_AWB_ENA))
		return;
	if (en)
		val |= CIF_ISP_CTRL_ISP_AWB_ENA;
	else
		val &= CIF_ISP_CTRL_ISP_AWB_ENA;
	isp3_param_write(params_vdev, val, ISP3X_ISP_CTRL0, id);
}

static void
isp_ccm_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp33_ccm_cfg *arg, u32 id)
{
	u32 value;
	u32 i;

	value = isp3_param_read(params_vdev, ISP3X_CCM_CTRL, id);
	value &= ISP_CCM_EN;

	value |= !!arg->sat_decay_en << 4 |
		 !!arg->asym_adj_en << 3 |
		 !!arg->enh_adj_en << 2 |
		 !!arg->highy_adjust_dis << 1;
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

	for (i = 0; i < ISP35_CCM_CURVE_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->alp_y[2 * i], arg->alp_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_CCM_ALP_Y0 + 4 * i, id);
	}

	value = (arg->right_bit & 0xf) << 4 | (arg->bound_bit & 0xf);
	isp3_param_write(params_vdev, value, ISP3X_CCM_BOUND_BIT, id);

	value = ISP_PACK_2SHORT(arg->color_coef0_r2y, arg->color_coef1_g2y);
	isp3_param_write(params_vdev, value, ISP32_CCM_ENHANCE0, id);

	value = ISP_PACK_2SHORT(arg->color_coef2_b2y, arg->color_enh_rat_max);
	isp3_param_write(params_vdev, value, ISP32_CCM_ENHANCE1, id);

	value = arg->hf_low | arg->hf_up << 8 | arg->hf_scale << 16;
	isp3_param_write(params_vdev, value, ISP33_CCM_HF_THD, id);

	for (i = 0; i < ISP35_CCM_HF_FACTOR_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->hf_factor[i * 2],
					arg->hf_factor[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_CCM_HF_FACTOR0 + i * 4, id);
	}
	value = arg->hf_factor[i * 2];
	isp3_param_write(params_vdev, value, ISP33_CCM_HF_FACTOR8, id);
}

static void
isp_ccm_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_CCM_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_CCM_CTRL, id);
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
	for (i = 0; i < ISP35_GAMMA_OUT_MAX_SAMPLES / 2; i++) {
		value = ISP_PACK_2SHORT(arg->gamma_y[2 * i],
					arg->gamma_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_GAMMA_OUT_Y0 + i * 4, id);
	}
	isp3_param_write(params_vdev, arg->gamma_y[2 * i], ISP3X_GAMMA_OUT_Y0 + i * 4, id);
}

static void
isp_goc_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_GAMMA_OUT_CTRL, id);
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
	u32 val = isp3_param_read(params_vdev, ISP3X_CPROC_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_CPROC_CTRL, id);
}

static void
isp_ie_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_IMG_EFF_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val =  ISP35_MODULE_EN;
	else
		val = 0;
	isp3_param_write(params_vdev, val, ISP3X_IMG_EFF_CTRL, id);
}

static void
isp_rawaf_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp35_rawaf_meas_cfg *arg, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	u32 width = out_crop->width, height = out_crop->height;
	u32 i, var, ctrl;
	u16 h_size, v_size;
	u16 h_offs, v_offs;
	u8 gaus_en, viir_en;
	size_t num_of_win = min_t(size_t, ARRAY_SIZE(arg->win), arg->num_afm_win);

	if (dev->unite_div > ISP_UNITE_DIV1)
		width = width / 2 + dev->hw_dev->unite_extend_pixel;
	if (dev->unite_div == ISP_UNITE_DIV4)
		height = height / 2 + dev->hw_dev->unite_extend_pixel;

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

	for (i = 0; i < ISP35_RAWAF_GAMMA_NUM / 2; i++) {
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

	for (i = 0; i < ISP35_RAWAF_VFIR_COE_NUM; i++) {
		var = ISP_PACK_2SHORT(arg->v1fir_coe[i], arg->v2fir_coe[i]);
		isp3_param_write(params_vdev, var, ISP32_RAWAF_V_FIR_COE0 + i * 4, id);
	}

	for (i = 0; i < ISP35_RAWAF_GAUS_COE_NUM / 4; i++) {
		var = ISP_PACK_4BYTE(arg->gaus_coe[i * 4], arg->gaus_coe[i * 4 + 1],
				     arg->gaus_coe[i * 4 + 2], arg->gaus_coe[i * 4 + 3]);
		isp3_param_write(params_vdev, var, ISP32_RAWAF_GAUS_COE03 + i * 4, id);
	}
	var = ISP_PACK_4BYTE(arg->gaus_coe[ISP35_RAWAF_GAUS_COE_NUM - 1], 0, 0, 0);
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
		for (i = 0; i < ISP35_RAWAF_HIIR_COE_NUM / 2; i++) {
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
		for (i = 0; i < ISP35_RAWAF_VIIR_COE_NUM; i++) {
			var = ISP_PACK_2SHORT(arg->v1iir_coe[i], arg->v2iir_coe[i]);
			isp3_param_write(params_vdev, var, ISP3X_RAWAF_V_IIR_COE0 + i * 4, id);
		}
	}
	if (arg->ldg_en) {
		ctrl |= ISP3X_RAWAF_LDG_EN;
		for (i = 0; i < ISP35_RAWAF_CURVE_NUM; i++) {
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
		!!arg->bnr_be_sel << 20 | !!arg->from_ynr << 19 |
		!!arg->hiir_left_border_mode << 21 | !!arg->avg_ds_en << 22 |
		!!arg->avg_ds_mode << 23 | !!arg->h1_acc_mode << 24 |
		!!arg->h2_acc_mode << 25 | !!arg->v1_acc_mode << 26 |
		!!arg->v2_acc_mode << 27;
	isp3_param_write(params_vdev, ctrl, ISP3X_RAWAF_CTRL, id);

	ctrl = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, id);
	ctrl &= ~(ISP3X_RAWAF_SEL(3) | ISP32L_BNR2AF_SEL);
	ctrl |= ISP3X_RAWAF_SEL(arg->rawaf_sel) | !!arg->bnr2af_sel << 28;
	isp3_param_write(params_vdev, ctrl, ISP3X_VI_ISP_PATH, id);
	priv->is_af_fe = true;
	if (arg->from_ynr ||
	    (arg->bnr2af_sel && arg->bnr_be_sel) ||
	    (!arg->bnr2af_sel && arg->rawaf_sel == 3))
		priv->is_af_fe = false;
}

static void
isp_rawaf_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 ctrl = isp3_param_read(params_vdev, ISP3X_RAWAF_CTRL, id);

	if (en == !!(ctrl & ISP35_MODULE_EN))
		return;

	if (en)
		ctrl |= ISP35_MODULE_EN;
	else
		ctrl &= ~ISP35_MODULE_EN;
	isp3_param_write(params_vdev, ctrl, ISP3X_RAWAF_CTRL, id);
}

static void
isp_rawae_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp35_rawae_meas_cfg *arg,
		 u32 addr, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *ispdev = params_vdev->dev;
	struct v4l2_rect *out_crop = &ispdev->isp_sdev.out_crop;
	u32 width = out_crop->width, height = out_crop->height;
	u32 value, h_size, v_size, h_offs, v_offs;
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
	value |= ISP3X_RAWAE_BIG_WND0_NUM(wnd_num_idx) |
		 !!arg->wnd1_en << 4 |
		 !!arg->debug_en << 8 |
		 !!arg->bnr_be_sel << 9;
	isp3_param_write(params_vdev, value, addr + ISP3X_RAWAE_BIG_CTRL, id);

	h_offs = arg->win0_h_offset & ~0x1;
	v_offs = arg->win0_v_offset & ~0x1;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(h_offs, v_offs),
			 addr + ISP3X_RAWAE_BIG_OFFSET, id);

	if (ispdev->unite_div > ISP_UNITE_DIV1)
		width = width / 2 + ispdev->hw_dev->unite_extend_pixel;
	if (ispdev->unite_div == ISP_UNITE_DIV4)
		height = height / 2 + ispdev->hw_dev->unite_extend_pixel;

	h_size = arg->win0_h_size;
	v_size = arg->win0_v_size;
	if (!h_size || h_size + h_offs + 1 > width)
		h_size = width - h_offs - 1;
	if (!v_size || v_size + v_offs + 2 > height)
		v_size = height - v_offs - 2;
	block_hsize = (h_size / ae_wnd_num[wnd_num_idx]) & ~0x1;
	block_vsize = (v_size / ae_wnd_num[wnd_num_idx]) & ~0x1;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(block_hsize, block_vsize),
			 addr + ISP3X_RAWAE_BIG_BLK_SIZE, id);

	h_offs = arg->win1_h_offset & ~0x1;
	v_offs = arg->win1_v_offset & ~0x1;
	isp3_param_write(params_vdev, ISP_PACK_2SHORT(h_offs, v_offs),
			 addr + ISP3X_RAWAE_BIG_WND1_OFFSET, id);

	v_size = arg->win1_h_size;
	h_size = arg->win1_v_size;
	if (!h_size || h_size + h_offs > width)
		h_size = width - h_offs;
	if (!v_size || v_size + v_offs > height)
		v_size = height - v_offs;
	h_size = (h_size + h_offs) & ~0x1;
	v_size = (v_size + v_offs) & ~0x1;
	isp3_param_write(params_vdev, ISP_PACK_2SHORT(h_size, v_size),
			 addr + ISP3X_RAWAE_BIG_WND1_SIZE, id);

	value = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, id);
	if (addr == ISP3X_RAWAE_BIG1_BASE) {
		value &= ~(ISP3X_RAWAE3_SEL(3) | BIT(29));
		value |= ISP3X_RAWAE3_SEL(arg->rawae_sel & 0xf);
		value |= !!arg->bnr2ae_sel << 29;
		isp3_param_write(params_vdev, value, ISP3X_VI_ISP_PATH, id);
		priv->is_ae3_fe = true;
		if ((arg->bnr2ae_sel && arg->bnr_be_sel) ||
		    (!arg->bnr2ae_sel && arg->rawae_sel == 3))
			priv->is_ae3_fe = false;
	} else {
		value &= ~(ISP3X_RAWAE012_SEL(3) | BIT(30));
		value |= ISP3X_RAWAE012_SEL(arg->rawae_sel & 0xf);
		value |= !!arg->bnr2ae_sel << 30;
		isp3_param_write(params_vdev, value, ISP3X_VI_ISP_PATH, id);
		priv->is_ae0_fe = true;
		if (arg->bnr2ae_sel && arg->bnr_be_sel)
			priv->is_ae0_fe = false;
	}
}

static void
isp_rawae_enable(struct rkisp_isp_params_vdev *params_vdev,
		 bool en, u32 addr, u32 id)
{
	u32 val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, addr + ISP3X_RAWAE_BIG_CTRL, id);
}

static void
isp_rawae0_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp35_rawae_meas_cfg *arg, u32 id)
{
	isp_rawae_config(params_vdev, arg, ISP3X_RAWAE_LITE_BASE, id);
}

static void
isp_rawae0_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	isp_rawae_enable(params_vdev, en, ISP3X_RAWAE_LITE_BASE, id);
}

static void
isp_rawae3_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp35_rawae_meas_cfg *arg, u32 id)
{
	isp_rawae_config(params_vdev, arg, ISP3X_RAWAE_BIG1_BASE, id);
}

static void
isp_rawae3_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	isp_rawae_enable(params_vdev, en, ISP3X_RAWAE_BIG1_BASE, id);
}

static void
isp_rawawb_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
		    const struct isp35_rawawb_meas_cfg *arg, bool is_check, u32 id)
{
	u32 i, val = isp3_param_read(params_vdev, ISP3X_RAWAWB_CTRL, id);

	if (is_check && !(val & ISP35_MODULE_EN))
		return;

	isp3_param_write_direct(params_vdev, ISP33_RAWAWB_WRAM_CLR, ISP3X_RAWAWB_WRAM_CTRL);
	for (i = 0; i < ISP35_RAWAWB_WEIGHT_NUM / 5; i++) {
		val = (arg->wp_blk_wei_w[5 * i] & 0x3f) |
		      (arg->wp_blk_wei_w[5 * i + 1] & 0x3f) << 6 |
		      (arg->wp_blk_wei_w[5 * i + 2] & 0x3f) << 12 |
		      (arg->wp_blk_wei_w[5 * i + 3] & 0x3f) << 18 |
		      (arg->wp_blk_wei_w[5 * i + 4] & 0x3f) << 24;
		isp3_param_write_direct(params_vdev, val, ISP3X_RAWAWB_WRAM_DATA_BASE);
	}
}

static void
isp_rawawb_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp35_rawawb_meas_cfg *arg, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct isp35_rawawb_meas_cfg *arg_rec = &params_rec->meas.rawawb;
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
		!!arg->bnr_be_sel << 10 |
		!!arg->ovexp_2ddr_dis << 9 |
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
		width = width / 2 + dev->hw_dev->unite_extend_pixel;
	if (dev->unite_div == ISP_UNITE_DIV4)
		height = height / 2 + dev->hw_dev->unite_extend_pixel;

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

	value = ISP_PACK_2SHORT(arg->ccm_coeff0_r, arg->ccm_coeff1_r);
	isp3_param_write(params_vdev, value, ISP33_RAWAWB_CCM_COEFF0_R, id);
	value = arg->ccm_coeff2_r;
	isp3_param_write(params_vdev, value, ISP33_RAWAWB_CCM_COEFF1_R, id);
	value = ISP_PACK_2SHORT(arg->ccm_coeff0_g, arg->ccm_coeff1_g);
	isp3_param_write(params_vdev, value, ISP33_RAWAWB_CCM_COEFF0_G, id);
	value = arg->ccm_coeff2_g;
	isp3_param_write(params_vdev, value, ISP33_RAWAWB_CCM_COEFF1_G, id);
	value = ISP_PACK_2SHORT(arg->ccm_coeff0_b, arg->ccm_coeff1_b);
	isp3_param_write(params_vdev, value, ISP33_RAWAWB_CCM_COEFF0_B, id);
	value = arg->ccm_coeff2_b;
	isp3_param_write(params_vdev, value, ISP33_RAWAWB_CCM_COEFF1_B, id);

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

	if (dev->hw_dev->is_single)
		isp_rawawb_cfg_sram(params_vdev, arg, false, id);
	memcpy(arg_rec->wp_blk_wei_w, arg->wp_blk_wei_w, ISP35_RAWAWB_WEIGHT_NUM);

	/* avoid to override the old enable value */
	value = isp3_param_read_cache(params_vdev, ISP3X_RAWAWB_CTRL, id);
	value &= (ISP35_MODULE_EN |
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
	priv->is_awb_fe = true;
	if (arg->drc2awb_sel ||
	    (arg->bnr2awb_sel && arg->bnr_be_sel))
		priv->is_awb_fe = false;
}

static void
isp_rawawb_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_RAWAWB_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_RAWAWB_CTRL, id);
}

static void
isp_rawhist_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
		     const struct isp35_rawhist_meas_cfg *arg,
		     u32 addr, bool is_check, u32 id)
{
	u32 i, j, wnd_num_idx, value;
	u8 weight15x15[ISP35_RAWHISTBIG_WEIGHT_REG_SIZE];
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
			weight15x15[i * ISP35_RAWHISTBIG_ROW_NUM + j] =
				arg->weight[i * hist_wnd_num[wnd_num_idx] + j];
		}
	}

	for (i = 0; i < (ISP35_RAWHISTBIG_WEIGHT_REG_SIZE / 5); i++) {
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
isp_rawhist_config(struct rkisp_isp_params_vdev *params_vdev,
		   const struct isp35_rawhist_meas_cfg *arg, u32 addr, u32 id)
{
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	u32 width = out_crop->width, height = out_crop->height;
	struct isp35_rawhist_meas_cfg *arg_rec;
	u32 ctrl, block_hsize, block_vsize, wnd_num_idx;
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
	ctrl = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, id);
	ctrl &= ISP3X_RAWHIST_EN;
	ctrl |= (arg->stepsize & 0x7) << 1 |
		!!arg->debug_en << 7 |
		(arg->mode & 0x7) << 8 |
		(arg->waterline & 0xfff) << 12 |
		(arg->data_sel & 0x7) << 24 |
		(arg->wnd_num & 0x3) << 28;
	isp3_param_write(params_vdev, ctrl, addr + ISP3X_RAWHIST_BIG_CTRL, id);

	h_offs = arg->h_offset & ~0x1;
	v_offs = arg->v_offset & ~0x1;
	isp3_param_write(params_vdev,
			 ISP_PACK_2SHORT(h_offs, v_offs),
			 addr + ISP3X_RAWHIST_BIG_OFFS, id);

	if (dev->unite_div > ISP_UNITE_DIV1)
		width = width / 2 + dev->hw_dev->unite_extend_pixel;
	if (dev->unite_div == ISP_UNITE_DIV4)
		height = height / 2 + dev->hw_dev->unite_extend_pixel;

	h_size = arg->h_size;
	v_size = arg->v_size;
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

	if (dev->hw_dev->is_single)
		isp_rawhist_cfg_sram(params_vdev, arg, addr, false, id);

	arg_rec = (addr == ISP3X_RAWHIST_LITE_BASE) ?
		  &params_rec->meas.rawhist0 : &params_rec->meas.rawhist3;
	*arg_rec = *arg;
}

static void
isp_rawhist_enable(struct rkisp_isp_params_vdev *params_vdev,
		   bool en, u32 addr, u32 id)
{
	u32 val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	val &= ~(ISP35_SELF_FORCE_UPD | ISP35_MODULE_EN);
	if (en)
		val |= ISP35_MODULE_EN;
	isp3_param_write(params_vdev, val, addr + ISP3X_RAWHIST_BIG_CTRL, id);
}

static void
isp_rawhist0_config(struct rkisp_isp_params_vdev *params_vdev,
		    const struct isp35_rawhist_meas_cfg *arg, u32 id)
{
	isp_rawhist_config(params_vdev, arg, ISP3X_RAWHIST_LITE_BASE, id);
}

static void
isp_rawhist0_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	isp_rawhist_enable(params_vdev, en, ISP3X_RAWHIST_LITE_BASE, id);
}

static void
isp_rawhist3_config(struct rkisp_isp_params_vdev *params_vdev,
		    const struct isp35_rawhist_meas_cfg *arg, u32 id)
{
	isp_rawhist_config(params_vdev, arg, ISP3X_RAWHIST_BIG1_BASE, id);
}

static void
isp_rawhist3_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	isp_rawhist_enable(params_vdev, en, ISP3X_RAWHIST_BIG1_BASE, id);
}

static void
isp_aiawb_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp35_aiawb_meas_cfg *arg, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	const struct isp2x_bls_fixed_val *pval = &arg->bls3_val;
	u32 value;

	/* bug no base on bayer pattern */
	value = ISP_PACK_2SHORT(pval->r, pval->gr);
	isp3_param_write(params_vdev, value, ISP35_BLS3_AB_FIXED, id);
	value = ISP_PACK_2SHORT(pval->gb, pval->b);
	isp3_param_write(params_vdev, value, ISP35_BLS3_CD_FIXED, id);

	value = isp3_param_read(params_vdev, ISP3X_BLS_CTRL, id);
	value &= ~ISP35_BLS_BLS3_EN;
	if (arg->bls3_en)
		value |= ISP35_BLS_BLS3_EN;
	isp3_param_write(params_vdev, value, ISP3X_BLS_CTRL, id);

	value = isp3_param_read(params_vdev, ISP39_W3A_CTRL0, id);
	if ((!arg->path_sel && !(value & ISP35_W3A_RAWLSC_SEL)) ||
	    (arg->path_sel && value & ISP35_W3A_RAWLSC_SEL)) {
		if (arg->path_sel)
			value &= ~ISP35_W3A_RAWLSC_SEL;
		else
			value |= ISP35_W3A_RAWLSC_SEL;
		isp3_param_write(params_vdev, value, ISP39_W3A_CTRL0, id);
	}

	value = isp3_param_read(params_vdev, ISP35_AIAWB_CTRL0, id);
	value &= (ISP35_MODULE_EN | ISP35_AIAWB_SYS_UPD_DIS | ISP35_AIAWB_FRMEND_UPD_DIS);
	value |= !!arg->ds_mode_config_en << 1 |
		 (arg->ds_mode & 0x3) << 2 |
		 !!arg->rgb2w_mode << 4 |
		 !!arg->rawout_sel << 7 |
		 (arg->path_sel & 0x7) << 8 |
		 (arg->in_shift & 0xf) << 12;
	isp3_param_write(params_vdev, value, ISP35_AIAWB_CTRL0, id);
	priv->is_aiawb_fe = true;
	if (arg->path_sel == 2 || arg->path_sel == 3)
		priv->is_aiawb_fe = false;
	else if (arg->path_sel == 4)
		priv->is_aiawb_fe = priv->is_awb_fe;

	value = arg->exp_thr | (arg->saturation_hthr & 0xfff) << 8 |
		(arg->saturation_lthr & 0x7ff) << 20 | !!arg->exp1_check_en << 31;
	isp3_param_write(params_vdev, value, ISP35_AIAWB_CTRL1, id);

	value = ISP_PACK_2SHORT(arg->h_offs, arg->v_offs);
	isp3_param_write(params_vdev, value, ISP35_AIAWB_WIN_OFFS, id);

	value = ISP_PACK_2SHORT(arg->h_size, arg->v_size);
	isp3_param_write(params_vdev, value, ISP35_AIAWB_WIN_SIZE, id);

	value = ISP_PACK_4BYTE(arg->flt_coe[0], arg->flt_coe[1],
			       arg->flt_coe[2], arg->flt_coe[3]);
	isp3_param_write(params_vdev, value, ISP35_AIAWB_FLT_COE0, id);
	value = arg->flt_coe[4] & 0xff;
	isp3_param_write(params_vdev, value, ISP35_AIAWB_FLT_COE1, id);

	value = ISP_PACK_2SHORT(arg->wbgain_inv_g, arg->wbgain_inv_b);
	isp3_param_write(params_vdev, value, ISP35_AIAWB_WBGAIN_INV0, id);
	value = ISP_PACK_2SHORT(arg->wbgain_inv_r, arg->expand);
	isp3_param_write(params_vdev, value, ISP35_AIAWB_WBGAIN_INV1, id);

	value = ISP_PACK_2SHORT(arg->ms00, arg->ms01);
	isp3_param_write(params_vdev, value, ISP35_AIAWB_MATRIX_SCALE, id);
	value = ISP_PACK_2SHORT(arg->mr00, arg->mr01);
	isp3_param_write(params_vdev, value, ISP35_AIAWB_MATRIX_ROT0, id);
	value = ISP_PACK_2SHORT(arg->mr10, arg->mr11);
	isp3_param_write(params_vdev, value, ISP35_AIAWB_MATRIX_ROT1, id);
}

static void
isp_aiawb_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	u32 val, ctrl = isp3_param_read_cache(params_vdev, ISP35_AIAWB_CTRL0, id);

	if (en == !!(ctrl & ISP35_MODULE_EN))
		return;
	if (en) {
		if (!priv->buf_aiawb[0].mem_priv) {
			dev_err(dev->dev, "no aiawb buffer allocated\n");
			return;
		}
		priv->buf_aiawb_idx = 0;
		ctrl |= ISP35_MODULE_EN | ISP35_AIAWB_SELF_UPD | ISP35_AIAWB_SYS_UPD_DIS;
		val = priv->buf_aiawb[0].dma_addr;
		isp3_param_write(params_vdev, val, ISP35_AIAWB_WR_BASE, id);
	} else {
		ctrl &= ~(ISP35_MODULE_EN | ISP35_AIAWB_SELF_UPD);
	}
	isp3_param_write(params_vdev, ctrl, ISP35_AIAWB_CTRL0, id);
}

static void
isp_awbsync_config(struct rkisp_isp_params_vdev *params_vdev,
		   const struct isp35_awbsync_meas_cfg *arg, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP35_AWBSYNC_CTRL, id);

	val &= ISP35_MODULE_EN;
	val |= ISP35_AWBSYNC_FRM_PROT |
	       !!arg->sumval_check_en << 2 | !!arg->sumval_mode << 3;
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_CTRL, id);

	val = (arg->scl_b & 0x3ff) | (arg->scl_g & 0x3ff) << 10 |
	      (arg->scl_r & 0x3ff) << 20;
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_SCL, id);

	val = (arg->sumval_minb & 0x3ff) | (arg->sumval_ming & 0x3ff) << 10 |
	      (arg->sumval_minr & 0x3ff) << 20;
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_SUMVAL_MIN, id);

	val = (arg->sumval_maxb & 0x3ff) | (arg->sumval_maxg & 0x3ff) << 10 |
	      (arg->sumval_maxr & 0x3ff) << 20;
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_SUMVAL_MAX, id);

	val = ISP_PACK_2SHORT(arg->win0_h_offs, arg->win0_v_offs);
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_WIN0_OFFS, id);
	val = ISP_PACK_2SHORT(arg->win0_r_coor, arg->win0_d_coor);
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_WIN0_RD_COOR, id);

	val = ISP_PACK_2SHORT(arg->win1_h_offs, arg->win1_v_offs);
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_WIN1_OFFS, id);
	val = ISP_PACK_2SHORT(arg->win1_r_coor, arg->win1_d_coor);
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_WIN1_RD_COOR, id);

	val = ISP_PACK_2SHORT(arg->win2_h_offs, arg->win2_v_offs);
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_WIN2_OFFS, id);
	val = ISP_PACK_2SHORT(arg->win2_r_coor, arg->win2_d_coor);
	isp3_param_write(params_vdev, val, ISP35_AWBSYNC_WIN2_RD_COOR, id);
}

static void
isp_awbsync_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 ctrl = isp3_param_read_cache(params_vdev, ISP35_AWBSYNC_CTRL, id);

	if (en == !!(ctrl & ISP35_MODULE_EN))
		return;
	if (en)
		ctrl |= ISP35_MODULE_EN;
	else
		ctrl &= ~ISP35_MODULE_EN;
	isp3_param_write(params_vdev, ctrl, ISP35_AWBSYNC_CTRL, id);
}

static void
isp_hdrmge_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp35_hdrmge_cfg *arg,
		  enum rkisp_params_type type, u32 id)
{
	u32 value;
	int i;

	if (type == RKISP_PARAMS_SHD || type == RKISP_PARAMS_ALL) {
		value = ISP_PACK_2SHORT(arg->short_gain, arg->short_inv_gain);
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_GAIN0, id);

		value = ISP_PACK_2SHORT(arg->medium_gain, arg->medium_inv_gain);
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_GAIN1, id);

		value = arg->long_gain;
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_GAIN2, id);

		value = isp3_param_read_cache(params_vdev, ISP3X_HDRMGE_CTRL, id);
		value &= ~(BIT(1) | BIT(4) | BIT(5) | BIT(6) | BIT(7));
		value |= !!arg->short_base_en << 1 |
			 (arg->dbg_mode & 0x3) << 4 |
			 !!arg->channel_detection_en << 6 |
			 !!arg->s_base_mode << 7;
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_CTRL, id);
	}

	if (type == RKISP_PARAMS_IMD || type == RKISP_PARAMS_ALL) {
		value = ISP_PACK_4BYTE(arg->ms_diff_scale, arg->ms_diff_offset,
				       arg->lm_diff_scale, arg->lm_diff_offset);
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_LIGHTZ, id);
		value = (arg->ms_abs_diff_scale & 0x7ff) |
			(arg->ms_abs_diff_thred_min_limit & 0x3ff) << 12 |
			(arg->ms_adb_diff_thred_max_limit & 0x3ff) << 22;
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_MS_DIFF, id);
		value = (arg->lm_abs_diff_scale & 0x7ff) |
			(arg->lm_abs_diff_thred_min_limit & 0x3ff) << 12 |
			(arg->lm_abs_diff_thred_max_limit & 0x3ff) << 22;
		isp3_param_write(params_vdev, value, ISP3X_HDRMGE_LM_DIFF, id);

		for (i = 0; i < ISP35_HDRMGE_WGT_NUM; i++) {
			value = ISP_PACK_2SHORT(arg->ms_luma_diff2wgt[i], arg->lm_luma_diff2wgt[i]);
			isp3_param_write(params_vdev, value, ISP3X_HDRMGE_DIFF_Y0 + 4 * i, id);
		}

		for (i = 0; i < ISP35_HDRMGE_WGT_NUM; i++) {
			value = (arg->lm_raw_diff2wgt[i] & 0x3ff) << 20 |
				(arg->ms_raw_diff2wgt[i] & 0x3ff) << 10 |
				(arg->luma2wgt[i] & 0x3ff);
			isp3_param_write(params_vdev, value, ISP3X_HDRMGE_OVER_Y0 + 4 * i, id);
		}

		value = ISP_PACK_2SHORT(arg->channel_detn_short_gain, arg->channel_detn_medium_gain);
		isp3_param_write(params_vdev, value, ISP32_HDRMGE_EACH_GAIN, id);

		value = arg->mid_luma_scale;
		isp3_param_write(params_vdev, value, ISP35_HDRMGE_FORCE_LONG0, id);
		value = ISP_PACK_2SHORT(arg->mid_luma_thred_max_limit, arg->mid_luma_thred_min_limit);
		isp3_param_write(params_vdev, value, ISP35_HDRMGE_FORCE_LONG1, id);
	}
}

static void
isp_hdrdrc_config(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp35_drc_cfg *arg,
		  enum rkisp_params_type type, u32 id)
{
	u32 i, value, ctrl;

	ctrl = isp3_param_read(params_vdev, ISP3X_DRC_CTRL0, id);
	ctrl &= ISP35_MODULE_EN;
	ctrl |= !!arg->gainx32_en << 3 |
		!!arg->cmps_byp_en << 2 | !!arg->bypass_en << 1;
	isp3_param_write(params_vdev, ctrl, ISP3X_DRC_CTRL0, id);
	if (ctrl & BIT(29))
		dev_warn(params_vdev->dev->dev, "drc raw_dly_dis=1\n");
	value = isp3_param_read_cache(params_vdev, ISP3X_HDRMGE_CTRL, id);
	if (ctrl & BIT(2) && (value & ISP35_MODULE_EN))
		dev_warn(params_vdev->dev->dev, "drc cmps_byp_en=1 but hdr\n");

	if (type == RKISP_PARAMS_IMD)
		return;

	value = (arg->log_transform_offset_bits & 0x0F) << 28 |
		(arg->comps_idx_luma_scale & 0x1FFF) << 14 |
		(arg->gain_idx_luma_scale & 0x03FFF);
	isp3_param_write(params_vdev, value, ISP3X_DRC_CTRL1, id);

	value = arg->adj_gain_idx_luma_scale << 24 |
		(arg->hi_detail_ratio & 0xFFF) << 12 |
		(arg->lo_detail_ratio & 0xFFF);
	isp3_param_write(params_vdev, value, ISP3X_DRC_LPRATIO, id);

	value = arg->bifilt_cur_pixel_wgt << 24 | !!arg->thumb_thred_en << 23 |
		(arg->thumb_thred_neg & 0x1ff) << 8 | arg->bifilt_wgt_offset;
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT0, id);

	value = (arg->filt_luma_soft_thred & 0x3ff) << 16 | !!arg->cmps_mode << 4 |
		(arg->cmps_offset_bits & 0xf);
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT1, id);

	value = arg->thumb_scale << 16 | (arg->thumb_max_limit & 0xfff);
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT2, id);

	value = (arg->lo_range_inv_sigma & 0x3ff) << 16 | (arg->hi_range_inv_sigma & 0x3ff);
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT3, id);

	value = !!arg->bifilt_soft_thred_en << 31 | (arg->bifilt_soft_thred & 0x7ff) << 16 |
		arg->bifilt_hi_wgt << 8 | (arg->bifilt_wgt & 0x1f);
	isp3_param_write(params_vdev, value, ISP39_DRC_BILAT4, id);

	for (i = 0; i < ISP35_DRC_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->gain_y[2 * i],
					arg->gain_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_DRC_GAIN_Y0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->gain_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP3X_DRC_GAIN_Y0 + 4 * i, id);

	for (i = 0; i < ISP35_DRC_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->compres_y[2 * i],
					arg->compres_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_DRC_COMPRES_Y0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->compres_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP3X_DRC_COMPRES_Y0 + 4 * i, id);

	for (i = 0; i < ISP35_DRC_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->scale_y[2 * i],
					arg->scale_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP3X_DRC_SCALE_Y0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->scale_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP3X_DRC_SCALE_Y0 + 4 * i, id);

	value = arg->comps_gain_min_limit;
	isp3_param_write(params_vdev, value, ISP3X_DRC_IIRWG_GAIN, id);

	for (i = 0; i < ISP35_DRC_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->sfthd_y[2 * i], arg->sfthd_y[2 * i + 1]);
		isp3_param_write(params_vdev, value, ISP39_DRC_SFTHD_Y0 + 4 * i, id);
	}
	value = ISP_PACK_2SHORT(arg->sfthd_y[2 * i], 0);
	isp3_param_write(params_vdev, value, ISP39_DRC_SFTHD_Y0 + 4 * i, id);

	value = arg->max_luma_wgt | arg->mid_luma_wgt << 8 | arg->min_luma_wgt << 16;
	isp3_param_write(params_vdev, value, ISP35_DRC_LUMA_MIX, id);
}

static void
isp_hdrdrc_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 value;
	bool real_en;

	value = isp3_param_read(params_vdev, ISP3X_DRC_CTRL0, id);
	real_en = !!(value & ISP35_MODULE_EN);
	if ((en && real_en) || (!en && !real_en))
		return;

	if (en)
		value |= ISP35_MODULE_EN;
	else
		value &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, value, ISP3X_DRC_CTRL0, id);
}

static void
isp_gic_cfg_noise_curve(struct rkisp_isp_params_vdev *params_vdev,
			const struct isp33_gic_cfg *arg, u32 id, bool direct)
{
	struct rkisp_device *dev = params_vdev->dev;
	u32 i, val;

	for (i = 0; i < ISP35_GIC_SIGMA_Y_NUM / 2; i++) {
		val = ISP_PACK_2SHORT(arg->bfflt_vsigma_y[i * 2],
				      arg->bfflt_vsigma_y[i * 2 + 1]);
		rkisp_idx_write(dev, ISP33_GIC_SIGMA_Y0 + i * 4, val, id, direct);
	}
	val = arg->bfflt_vsigma_y[i * 2];
	rkisp_idx_write(dev, ISP33_GIC_SIGMA_Y8, val, id, direct);
}

static void
isp_gic_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp33_gic_cfg *arg, u32 id)
{
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct isp33_gic_cfg *arg_rec = &params_rec->others.gic_cfg;
	u32 value, ctrl;
	s32 i;

	ctrl = isp3_param_read(params_vdev, ISP3X_GIC_CONTROL, id);
	ctrl &= ISP35_MODULE_EN;
	ctrl |= !!arg->bypass_en << 1 |
		!!arg->pro_mode << 2 |
		!!arg->manualnoisecurve_en << 3 |
		!!arg->manualnoisethred_en << 4 |
		!!arg->gain_bypass_en << 5;
	isp3_param_write(params_vdev, ctrl, ISP3X_GIC_CONTROL, id);

	value = (arg->medflt_minthred & 0xf) |
		(arg->medflt_maxthred & 0xf) << 4 | arg->medflt_ratio << 16;
	isp3_param_write(params_vdev, value, ISP33_GIC_MEDFLT_PARA, id);

	value = (arg->medfltuv_minthred & 0xf) |
		(arg->medfltuv_maxthred & 0xf) << 4 | arg->medfltuv_ratio << 16;
	isp3_param_write(params_vdev, value, ISP33_GIC_MEDFLTUV_PARA, id);

	value = arg->noisecurve_scale;
	isp3_param_write(params_vdev, value, ISP33_GIC_NOISE_SCALE, id);

	value = arg->bffltwgt_offset | arg->bffltwgt_scale << 16;
	isp3_param_write(params_vdev, value, ISP33_GIC_BILAT_PARA1, id);

	value = arg->bfflt_ratio;
	isp3_param_write(params_vdev, value, ISP33_GIC_BILAT_PARA2, id);

	value = ISP_PACK_4BYTE(arg->bfflt_coeff0, arg->bfflt_coeff1,
			       arg->bfflt_coeff2, 0);
	isp3_param_write(params_vdev, value, ISP33_GIC_DISWGT_COEFF, id);

	if (!(ctrl & ISP35_MODULE_EN) || arg->manualnoisecurve_en)
		memcpy(arg_rec->bfflt_vsigma_y, arg->bfflt_vsigma_y, sizeof(arg->bfflt_vsigma_y));
	isp_gic_cfg_noise_curve(params_vdev, arg_rec, id, false);

	value = (arg->luma_dx[0] & 0xf) | (arg->luma_dx[1] & 0xf) << 4 |
		(arg->luma_dx[2] & 0xf) << 8 | (arg->luma_dx[3] & 0xf) << 12 |
		(arg->luma_dx[4] & 0xf) << 16 | (arg->luma_dx[5] & 0xf) << 20 |
		(arg->luma_dx[6] & 0xf) << 24;
	isp3_param_write(params_vdev, value, ISP33_GIC_LUMA_DX, id);

	for (i = 0; i < ISP35_GIC_THRED_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->thred_y[i * 2],
					arg->thred_y[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_GIC_THRED_Y0 + i * 4, id);

		value = ISP_PACK_2SHORT(arg->minthred_y[i * 2],
					arg->minthred_y[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_GIC_MIN_THRED_Y0 + i * 4, id);
	}

	value = arg->autonoisethred_scale;
	isp3_param_write(params_vdev, value, ISP33_GIC_THRED_SCALE, id);

	value = ISP_PACK_4BYTE(arg->lofltgr_coeff0, arg->lofltgr_coeff1,
			       arg->lofltgr_coeff2, arg->lofltgr_coeff3);
	isp3_param_write(params_vdev, value, ISP33_GIC_LOFLTGR_COEFF, id);

	value = ISP_PACK_4BYTE(arg->lofltgb_coeff0, arg->lofltgb_coeff1, 0, 0);
	isp3_param_write(params_vdev, value, ISP33_GIC_LOFLTGB_COEFF, id);

	value = arg->sumlofltcoeff_inv;
	isp3_param_write(params_vdev, value, ISP33_GIC_SUM_LOFLT_INV, id);

	value = ISP_PACK_4BYTE(arg->lofltthred_coeff0, arg->lofltthred_coeff1, 0, 0);
	isp3_param_write(params_vdev, value, ISP33_GIC_LOFLTTHRED_COEFF, id);

	value = (arg->global_gain & 0x3ff) |
		(arg->globalgain_alpha & 0xf) << 12 |
		arg->globalgain_scale << 16;
	isp3_param_write(params_vdev, value, ISP33_GIC_GAIN, id);

	value = ISP_PACK_2SHORT(arg->gain_offset, arg->gain_scale);
	isp3_param_write(params_vdev, value, ISP33_GIC_GAIN_SLOPE, id);

	value = ISP_PACK_2SHORT(arg->gainadjflt_minthred,
				arg->gainadjflt_maxthred);
	isp3_param_write(params_vdev, value,  ISP33_GIC_GAIN_THRED, id);
}

static void
isp_gic_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_GIC_CONTROL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en) {
		val |= ISP35_MODULE_EN;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1,
				    ISP33_GIC_FST_FRAME, id);
	} else {
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	}
	isp3_param_write(params_vdev, val, ISP3X_GIC_CONTROL, id);
}

static void
isp_enh_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp35_enh_cfg *arg, bool is_check, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	u32 i, j, val, ctrl = isp3_param_read(params_vdev, ISP33_ENH_CTRL, id);

	if (is_check && (!(ctrl & ISP35_MODULE_EN) || !arg->iir_wr))
		return;

	val = (arg->pre_wet_frame_cnt0 & 0xf) |
	      (arg->pre_wet_frame_cnt1 & 0xf) << 4;
	isp3_param_write_direct(params_vdev, val, ISP33_ENH_PRE_FRAME);
	for (i = 0; i < priv->enh_row; i++) {
		val = ISP33_IIR_WR_ID(i) | ISP33_IIR_WR_CLEAR;
		isp3_param_write_direct(params_vdev, val, ISP33_ENH_IIR_RW);
		for (j = 0; j < priv->enh_col / 4; j++) {
			val = ISP_PACK_4BYTE(arg->iir[i][j * 4], arg->iir[i][j * 4 + 1],
					     arg->iir[i][j * 4 + 2], arg->iir[i][j * 4 + 3]);
			isp3_param_write_direct(params_vdev, val, ISP33_ENH_IIR0 + j * 4);
		}
	}
}

static void
isp_enh_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp35_enh_cfg *arg, u32 id)
{
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct isp35_enh_cfg *arg_rec = &params_rec->others.enh_cfg;
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	u32 w = out_crop->width, h = out_crop->height;
	u32 i, value, ctrl, het_aliquant;

	if (dev->unite_div > ISP_UNITE_DIV1)
		w = w / 2 + dev->hw_dev->unite_extend_pixel;
	if (dev->unite_div == ISP_UNITE_DIV4)
		h = h / 2 + dev->hw_dev->unite_extend_pixel;
	priv->enh_col = ALIGN((w + 127) / 128, 4);
	if (priv->enh_col > ISP35_ENH_IIR_COL_MAX)
		priv->enh_col = ISP33_ENH_IIR_COL_MAX;
	priv->enh_row = (h + 128) / 129;
	if (priv->enh_row > ISP35_ENH_IIR_ROW_MAX)
		priv->enh_row = ISP33_ENH_IIR_ROW_MAX;
	het_aliquant = h % 3;

	ctrl = isp3_param_read(params_vdev, ISP33_ENH_CTRL, id);
	ctrl &= ISP35_MODULE_EN;
	ctrl |= !!arg->bypass << 1 |
		!!arg->blf3_bypass << 2 |
		(het_aliquant & 0x3) << 4 |
		(priv->enh_row & 0x1f) << 8;
	isp3_param_write(params_vdev, ctrl, ISP33_ENH_CTRL, id);

	value = arg->iir_inv_sigma |
		arg->iir_soft_thed << 16 |
		arg->iir_cur_wgt << 24;
	isp3_param_write(params_vdev, value, ISP33_ENH_IIR_FLT, id);

	value = (arg->blf3_inv_sigma & 0x1ff) |
		(arg->blf3_cur_wgt & 0x1ff) << 16 |
		(arg->blf3_thumb_cur_wgt & 0xf) << 28;
	isp3_param_write(params_vdev, value, ISP33_ENH_BILAT_FLT3X3, id);

	value = arg->blf5_inv_sigma | arg->blf5_cur_wgt << 16;
	isp3_param_write(params_vdev, value, ISP33_ENH_BILAT_FLT5X5, id);

	value = arg->global_strg;
	isp3_param_write(params_vdev, value, ISP33_ENH_GLOBAL_STRG, id);

	for (i = 0; i < ISP35_ENH_LUMA_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->lum2strg[i * 2], arg->lum2strg[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_ENH_LUMA_LUT0 + i * 4, id);
	}
	value = arg->lum2strg[i * 2];
	isp3_param_write(params_vdev, value, ISP33_ENH_LUMA_LUT8, id);

	for (i = 0; i < ISP35_ENH_DETAIL_NUM / 3; i++) {
		value = (arg->detail2strg_idx[i * 3] & 0x3ff) |
			(arg->detail2strg_idx[i * 3 + 1] & 0x3ff) << 10 |
			(arg->detail2strg_idx[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_ENH_DETAIL_IDX0 + i * 4, id);
	}
	value = (arg->detail2strg_idx[i * 3] & 0x3ff) |
		(arg->detail2strg_idx[i * 3 + 1] & 0x7ff) << 10;
	isp3_param_write(params_vdev, value, ISP33_ENH_DETAIL_IDX2, id);

	value = (arg->detail2strg_power0 & 0xf) |
		(arg->detail2strg_power1 & 0xf) << 4 |
		(arg->detail2strg_power2 & 0xf) << 8 |
		(arg->detail2strg_power3 & 0xf) << 12 |
		(arg->detail2strg_power4 & 0xf) << 16 |
		(arg->detail2strg_power5 & 0xf) << 20 |
		(arg->detail2strg_power6 & 0xf) << 24;
	isp3_param_write(params_vdev, value, ISP33_ENH_DETAIL_POWER, id);

	for (i = 0; i < ISP35_ENH_DETAIL_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->detail2strg_val[i * 2],
					arg->detail2strg_val[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_ENH_DETAIL_VALUE0 + i * 4, id);
	}

	if (dev->hw_dev->is_single && arg->iir_wr)
		isp_enh_cfg_sram(params_vdev, arg, false, id);
	else if (arg->iir_wr)
		memcpy(arg_rec, arg, sizeof(struct isp35_enh_cfg));
}

static void
isp_enh_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP33_ENH_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en) {
		val |= ISP35_MODULE_EN;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1,
				    ISP33_ENH_FST_FRAME, id);
	} else {
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	}
	isp3_param_write(params_vdev, val, ISP33_ENH_CTRL, id);
}

static void
isp_hist_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
		  const struct isp33_hist_cfg *arg, bool is_check, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	u32 i, j, val, ctrl = isp3_param_read(params_vdev, ISP33_HIST_CTRL, id);

	if (is_check && (!(ctrl & ISP35_MODULE_EN) || !arg->iir_wr))
		return;

	val = (arg->stab_frame_cnt0 & 0xf) |
	      (arg->stab_frame_cnt1 & 0xf) << 4;
	isp3_param_write_direct(params_vdev, val, ISP33_HIST_STAB);
	for (i = 0; i < priv->hist_blk_num; i++) {
		val = ISP33_IIR_WR_ID(i) | ISP33_IIR_WR_CLEAR;
		isp3_param_write_direct(params_vdev, val, ISP33_HIST_RW);
		for (j = 0; j < ISP35_HIST_IIR_NUM / 2; j++) {
			val = ISP_PACK_2SHORT(arg->iir[i][2 * j], arg->iir[i][2 * j + 1]);
			isp3_param_write_direct(params_vdev, val, ISP33_HIST_IIR0 + 4 * j);
		}
	}
}

static void
isp_hist_config(struct rkisp_isp_params_vdev *params_vdev,
		const struct isp33_hist_cfg *arg, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct isp33_hist_cfg *arg_rec = &params_rec->others.hist_cfg;
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	u32 w = out_crop->width, h = out_crop->height;
	u32 value, ctrl, thumb_row, thumb_col, blk_het, blk_wid;
	int i;

	if (dev->unite_div > ISP_UNITE_DIV1)
		w = w / 2 + dev->hw_dev->unite_extend_pixel;
	if (dev->unite_div == ISP_UNITE_DIV4)
		h = h / 2 + dev->hw_dev->unite_extend_pixel;

	ctrl = isp3_param_read(params_vdev, ISP33_HIST_CTRL, id);
	ctrl &= ISP35_MODULE_EN;
	ctrl |= !!arg->bypass << 1 | !!arg->mem_mode << 4;
	isp3_param_write(params_vdev, ctrl, ISP33_HIST_CTRL, id);

	value = arg->count_scale | arg->count_offset << 8 |
		arg->count_min_limit << 16;
	isp3_param_write(params_vdev, value, ISP33_HIST_HF_STAT, id);

	value = ISP_PACK_2SHORT(arg->merge_alpha, arg->user_set);
	isp3_param_write(params_vdev, value, ISP33_HIST_MAP0, id);

	value = arg->map_count_scale | arg->gain_ref_wgt << 16;
	isp3_param_write(params_vdev, value, ISP33_HIST_MAP1, id);

	value = arg->flt_inv_sigma | arg->flt_cur_wgt << 16;
	isp3_param_write(params_vdev, value, ISP33_HIST_IIR, id);

	for (i = 0; i < ISP35_HIST_ALPHA_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->pos_alpha[i * 4],
				       arg->pos_alpha[i * 4 + 1],
				       arg->pos_alpha[i * 4 + 2],
				       arg->pos_alpha[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_HIST_POS_ALPHA0 + i * 4, id);
		value = ISP_PACK_4BYTE(arg->neg_alpha[i * 4],
				       arg->neg_alpha[i * 4 + 1],
				       arg->neg_alpha[i * 4 + 2],
				       arg->neg_alpha[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_HIST_NEG_ALPHA0 + i * 4, id);
	}
	value = arg->pos_alpha[i * 4];
	isp3_param_write(params_vdev, value, ISP33_HIST_POS_ALPHA4, id);
	value = arg->neg_alpha[i * 4];
	isp3_param_write(params_vdev, value, ISP33_HIST_NEG_ALPHA4, id);

	value = arg->saturate_scale;
	isp3_param_write(params_vdev, value, ISP33_HIST_UV_SCL, id);

	thumb_row = arg->thumb_row > ISP33_HIST_THUMB_ROW_MAX ?
		    ISP33_HIST_THUMB_ROW_MAX : arg->thumb_row & ~1;
	thumb_col = arg->thumb_col > ISP33_HIST_THUMB_COL_MAX ?
		    ISP33_HIST_THUMB_COL_MAX : arg->thumb_col & ~1;
	blk_het = ALIGN(h / thumb_row, 2);
	blk_wid = ALIGN(w / thumb_col, 2);
	priv->hist_blk_num = thumb_row * thumb_col;
	value = ISP_PACK_2SHORT(blk_het, blk_wid);
	isp3_param_write(params_vdev, value, ISP33_HIST_BLOCK_SIZE, id);
	value = ISP_PACK_4BYTE(thumb_row, thumb_col, 0, 0);
	isp3_param_write(params_vdev, value, ISP33_HIST_THUMB_SIZE, id);

	if (dev->hw_dev->is_single && arg->iir_wr)
		isp_hist_cfg_sram(params_vdev, arg, false, id);
	else if (arg->iir_wr)
		memcpy(arg_rec, arg, sizeof(struct isp33_hist_cfg));
}

static void
isp_hist_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP33_HIST_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en) {
		val |= ISP35_MODULE_EN;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1,
				    ISP33_YHIST_FST_FRAME, id);
	} else {
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	}
	isp3_param_write(params_vdev, val, ISP33_HIST_CTRL, id);
}

static void
isp_hsv_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp35_hsv_cfg *arg, bool is_check, u32 id)
{
	u32 ctrl = isp3_param_read(params_vdev, ISP3X_3DLUT_CTRL, id);
	u32 val, i, j;

	if (is_check && !(ctrl & ISP35_MODULE_EN))
		return;

	ctrl &= ~ISP35_SELF_FORCE_UPD;
	ctrl |= ISP35_HSV_FIX_RW_CONFLICT | ISP35_HSV_TBL_CLR;
	isp3_param_write_direct(params_vdev, ctrl, ISP3X_3DLUT_CTRL);
	for (i = 0; i < ISP35_HSV_2DLUT_ROW; i++) {
		for (j = 0; j < ISP35_HSV_2DLUT_COL - 1; j += 2) {
			val = ISP_PACK_2SHORT(arg->lut0_2d[i][j], arg->lut0_2d[i][j + 1]);
			isp3_param_write_direct(params_vdev, val, ISP35_HSV_2DLUT0);
		}
		val = arg->lut0_2d[i][ISP35_HSV_2DLUT_COL - 1];
		isp3_param_write_direct(params_vdev, val, ISP35_HSV_2DLUT0);
	}
	if (arg->hsv_2dlut12_cfg) {
		for (i = 0; i < ISP35_HSV_2DLUT_ROW; i++) {
			for (j = 0; j < ISP35_HSV_2DLUT_COL - 1; j += 2) {
				val = ISP_PACK_2SHORT(arg->lut1_2d[i][j], arg->lut1_2d[i][j + 1]);
				isp3_param_write_direct(params_vdev, val, ISP35_HSV_2DLUT1);
			}
			val = arg->lut1_2d[i][ISP35_HSV_2DLUT_COL - 1];
			isp3_param_write_direct(params_vdev, val, ISP35_HSV_2DLUT1);
		}
		for (i = 0; i < ISP35_HSV_2DLUT_ROW; i++) {
			for (j = 0; j < ISP35_HSV_2DLUT_COL - 1; j += 2) {
				val = ISP_PACK_2SHORT(arg->lut2_2d[i][j], arg->lut2_2d[i][j + 1]);
				isp3_param_write_direct(params_vdev, val, ISP35_HSV_2DLUT2);
			}
			val = arg->lut2_2d[i][ISP35_HSV_2DLUT_COL - 1];
			isp3_param_write_direct(params_vdev, val, ISP35_HSV_2DLUT2);
		}
	} else {
		for (i = 0; i < ISP35_HSV_1DLUT_NUM / 2; i++) {
			val = ISP_PACK_2SHORT(arg->lut0_1d[i * 2], arg->lut0_1d[i * 2 + 1]);
			isp3_param_write_direct(params_vdev, val, ISP35_HSV_1DLUT);
		}
		val = arg->lut0_1d[ISP35_HSV_1DLUT_NUM - 1];
		isp3_param_write_direct(params_vdev, val, ISP35_HSV_1DLUT);

		for (i = 0; i < ISP35_HSV_1DLUT_NUM / 2; i++) {
			val = ISP_PACK_2SHORT(arg->lut1_1d[i * 2], arg->lut1_1d[i * 2 + 1]);
			isp3_param_write_direct(params_vdev, val, ISP35_HSV_1DLUT);
		}
		val = arg->lut1_1d[ISP35_HSV_1DLUT_NUM - 1];
		isp3_param_write_direct(params_vdev, val, ISP35_HSV_1DLUT);
	}
	ctrl &= ~ISP35_HSV_FIX_RW_CONFLICT;
	isp3_param_write_direct(params_vdev, ctrl, ISP3X_3DLUT_CTRL);
}

static void
isp_hsv_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp35_hsv_cfg *arg, u32 id)
{
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct rkisp_device *dev = params_vdev->dev;
	u32 val = isp3_param_read(params_vdev, ISP3X_3DLUT_CTRL, id);

	val &= ISP35_MODULE_EN;
	val |= !!arg->hsv_1dlut0_en << 1 |
	       !!arg->hsv_1dlut1_en << 2 |
	       !!arg->hsv_2dlut0_en << 3 |
	       !!arg->hsv_2dlut1_en << 4 |
	       !!arg->hsv_2dlut2_en << 5 |
	       !!arg->hsv_2dlut12_cfg << 6;
	isp3_param_write(params_vdev, val, ISP3X_3DLUT_CTRL, id);

	val = (arg->hsv_1dlut0_idx_mode & 0x3) |
	      (arg->hsv_1dlut1_idx_mode & 0x3) << 2 |
	      (arg->hsv_2dlut0_idx_mode & 0x3) << 4 |
	      (arg->hsv_2dlut1_idx_mode & 0x3) << 6 |
	      (arg->hsv_2dlut2_idx_mode & 0x3) << 8 |
	      (arg->hsv_1dlut0_item_mode & 0x7) << 10 |
	      (arg->hsv_1dlut1_item_mode & 0x7) << 13 |
	      (arg->hsv_2dlut0_item_mode & 0x3) << 16 |
	      (arg->hsv_2dlut1_item_mode & 0x3) << 18 |
	      (arg->hsv_2dlut2_item_mode & 0x3) << 20;
	isp3_param_write(params_vdev, val, ISP35_HSV_MODE_CTRL, id);
	if (dev->hw_dev->is_single)
		isp_hsv_cfg_sram(params_vdev, arg, false, id);
	params_rec->others.hsv_cfg = *arg;
}

static void
isp_hsv_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_3DLUT_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_3DLUT_CTRL, id);
}

static void
isp_ldch_config(struct rkisp_isp_params_vdev *params_vdev,
		const struct isp32_ldch_cfg *arg, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct isp2x_mesh_head *head;
	int buf_idx, i;
	u32 value;

	value = isp3_param_read(params_vdev, ISP3X_LDCH_STS, id);
	value &= ISP35_MODULE_EN;
	value |= !!arg->map13p3_en << 7 |
		 !!arg->force_map_en << 6 |
		 !!arg->bic_mode_en << 4 |
		 !!arg->sample_avr_en << 3 |
		 !!arg->frm_end_dis << 1;
	isp3_param_write(params_vdev, value, ISP3X_LDCH_STS, id);
	if (arg->bic_mode_en) {
		for (i = 0; i < ISP35_LDCH_BIC_NUM / 4; i++) {
			value = ISP_PACK_4BYTE(arg->bicubic[i * 4], arg->bicubic[i * 4 + 1],
					       arg->bicubic[i * 4 + 2], arg->bicubic[i * 4 + 3]);
			isp3_param_write(params_vdev, value, ISP32_LDCH_BIC_TABLE0 + i * 4, id);
		}
	}

	for (i = 0; i < ISP35_MESH_BUF_NUM; i++) {
		if (!priv->buf_ldch[id][i].mem_priv)
			continue;
		if (arg->buf_fd == priv->buf_ldch[id][i].dma_fd)
			break;
	}
	if (i == ISP35_MESH_BUF_NUM) {
		dev_err(dev->dev, "cannot find ldch buf fd(%d)\n", arg->buf_fd);
		return;
	}

	if (!priv->buf_ldch[id][i].vaddr) {
		dev_err(dev->dev, "no ldch buffer allocated\n");
		return;
	}

	buf_idx = priv->buf_ldch_idx[id];
	head = (struct isp2x_mesh_head *)priv->buf_ldch[id][buf_idx].vaddr;
	head->stat = MESH_BUF_INIT;

	buf_idx = i;
	head = (struct isp2x_mesh_head *)priv->buf_ldch[id][buf_idx].vaddr;
	head->stat = MESH_BUF_CHIPINUSE;
	priv->buf_ldch_idx[id] = buf_idx;
	rkisp_prepare_buffer(dev, &priv->buf_ldch[id][buf_idx]);
	value = priv->buf_ldch[id][buf_idx].dma_addr + head->data_oft;
	isp3_param_write(params_vdev, value, ISP3X_MI_LUT_LDCH_RD_BASE, id);
	isp3_param_write(params_vdev, arg->hsize, ISP3X_MI_LUT_LDCH_RD_H_WSIZE, id);
	isp3_param_write(params_vdev, arg->vsize, ISP3X_MI_LUT_LDCH_RD_V_SIZE, id);
}

static void
isp_ldch_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	u32 val = isp3_param_read(params_vdev, ISP3X_LDCH_STS, id);
	u32 buf_idx;

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en) {
		buf_idx = priv->buf_ldch_idx[id];
		if (!priv->buf_ldch[id][buf_idx].vaddr) {
			dev_err(dev->dev, "no ldch buffer allocated\n");
			return;
		}
		val |= ISP35_MODULE_EN;
	} else {
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	}
	isp3_param_write(params_vdev, val, ISP3X_LDCH_STS, id);
}

static void
isp_ynr_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp35_ynr_cfg *arg, u32 id)
{
	u32 i, value;

	value = isp3_param_read(params_vdev, ISP3X_YNR_GLOBAL_CTRL, id);
	value &= ISP35_MODULE_EN;

	value |= !!arg->hi_spnr_bypass << 1 |
		 !!arg->mi_spnr_bypass << 2 |
		 !!arg->lo_spnr_bypass << 3 |
		 !!arg->rnr_en << 4 |
		 !!arg->tex2lo_strg_en << 5 |
		 !!arg->hi_lp_en << 6 |
		 !!arg->dsfilt_bypass << 7 |
		 !!arg->tex2wgt_en << 8;
	isp3_param_write(params_vdev, value, ISP3X_YNR_GLOBAL_CTRL, id);

	value = (arg->global_set_gain & 0x3ff) |
		(arg->gain_merge_alpha & 0xf) << 12 |
		arg->local_gain_scale << 16;
	isp3_param_write(params_vdev, value, ISP33_YNR_GAIN_CTRL, id);

	for (i = 0; i < ISP35_YNR_ADJ_NUM / 3; i++) {
		value = (arg->lo_spnr_gain2strg[i * 3] & 0x1ff) |
			(arg->lo_spnr_gain2strg[i * 3 + 1] & 0x1ff) << 10 |
			(arg->lo_spnr_gain2strg[i * 3 + 2] & 0x1ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_YNR_GAIN_ADJ_0_2 + i * 4, id);
	}

	value = arg->rnr_max_radius;
	isp3_param_write(params_vdev, value, ISP33_YNR_RNR_MAX_R, id);

	value = ISP_PACK_2SHORT(arg->rnr_center_h, arg->rnr_center_v);
	isp3_param_write(params_vdev, value, ISP33_YNR_RNR_CENTER_COOR, id);

	for (i = 0; i < ISP35_YNR_XY_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->radius2strg[i * 4],
				       arg->radius2strg[i * 4 + 1],
				       arg->radius2strg[i * 4 + 2],
				       arg->radius2strg[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_YNR_RNR_STRENGTH03 + i * 4, id);
	}
	value = arg->radius2strg[i * 4];
	isp3_param_write(params_vdev, value, ISP33_YNR_RNR_STRENGTH16, id);

	for (i = 0; i < ISP35_YNR_XY_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->luma2sima_x[i * 2],
					arg->luma2sima_x[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_YNR_SGM_DX_0_1 + i * 4, id);

		value = ISP_PACK_2SHORT(arg->luma2sima_y[i * 2],
					arg->luma2sima_y[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_YNR_SGM_Y_0_1 + i * 4, id);
	}
	value = arg->luma2sima_x[i * 2];
	isp3_param_write(params_vdev, value, ISP33_YNR_SGM_DX_16, id);
	value = arg->luma2sima_y[i * 2];
	isp3_param_write(params_vdev, value, ISP33_YNR_SGM_Y_16, id);

	for (i = 0; i < ISP35_YNR_TEX2WGT_NUM / 3; i++) {
		value = arg->mi_spnr_tex2wgt_scale[i * 3] |
			arg->mi_spnr_tex2wgt_scale[i * 3 + 1] << 10 |
			arg->mi_spnr_tex2wgt_scale[i * 3 + 2] << 20;
		isp3_param_write(params_vdev, value, ISP35_YNR_MI_TEX2WGT_SCALE_0_1_2 + i * 4, id);
		value = arg->lo_spnr_tex2wgt_scale[i * 3] |
			arg->lo_spnr_tex2wgt_scale[i * 3 + 1] << 10 |
			arg->lo_spnr_tex2wgt_scale[i * 3 + 2] << 20;
		isp3_param_write(params_vdev, value, ISP35_YNR_LO_TEX2WGT_SCALE_0_1_2 + i * 4, id);
	}

	value = (arg->hi_spnr_sigma_min_limit & 0x7ff) |
		(arg->hi_spnr_local_gain_alpha & 0x1f) << 11 |
		(arg->hi_spnr_strg & 0x3ff) << 16;
	isp3_param_write(params_vdev, value, ISP33_YNR_HI_SIGMA_GAIN, id);

	value = (arg->hi_spnr_filt_coeff[0] & 0x3f) |
		(arg->hi_spnr_filt_coeff[1] & 0x3f) << 6 |
		(arg->hi_spnr_filt_coeff[2] & 0x3f) << 12 |
		(arg->hi_spnr_filt_coeff[3] & 0x3f) << 18;
	isp3_param_write(params_vdev, value, ISP33_YNR_HI_GAUS_COE, id);

	value = (arg->hi_spnr_filt_wgt_offset & 0x3ff) |
		(arg->hi_spnr_filt_center_wgt & 0x1fff) << 10;
	isp3_param_write(params_vdev, value, ISP33_YNR_HI_WEIGHT, id);

	value = (arg->hi_spnr_filt1_coeff[0] & 0x1ff) |
		(arg->hi_spnr_filt1_coeff[1] & 0x1ff) << 10 |
		(arg->hi_spnr_filt1_coeff[2] & 0x1ff) << 20;
	isp3_param_write(params_vdev, value, ISP33_YNR_HI_GAUS1_COE_0_2, id);
	value = (arg->hi_spnr_filt1_coeff[3] & 0x1ff) |
		(arg->hi_spnr_filt1_coeff[4] & 0x1ff) << 10 |
		(arg->hi_spnr_filt1_coeff[5] & 0x1ff) << 20;
	isp3_param_write(params_vdev, value, ISP33_YNR_HI_GAUS1_COE_3_5, id);

	value = (arg->hi_spnr_filt1_tex_thred & 0x7ff) |
		(arg->hi_spnr_filt1_tex_scale & 0x3ff) << 12 |
		(arg->hi_spnr_filt1_wgt_alpha & 0x1ff) << 22;
	isp3_param_write(params_vdev, value, ISP33_YNR_HI_TEXT, id);

	value = arg->mi_spnr_filt_coeff0 |
		arg->mi_spnr_filt_coeff1 << 10 |
		arg->mi_spnr_filt_coeff2 << 20;
	isp3_param_write(params_vdev, value, ISP33_YNR_MI_GAUS_COE, id);
	value = arg->mi_spnr_filt_coeff3 | arg->mi_spnr_filt_coeff4 << 10;
	isp3_param_write(params_vdev, value, ISP35_YNR_MI_GAUS_COE1, id);

	value = ISP_PACK_2SHORT(arg->mi_spnr_strg, arg->mi_spnr_soft_thred_scale);
	isp3_param_write(params_vdev, value, ISP33_YNR_MI_STRG_DETAIL, id);

	value = arg->mi_spnr_wgt |
		(arg->mi_spnr_filt_center_wgt & 0x7ff) << 10 |
		!!arg->mi_ehance_scale_en << 23 |
		arg->mi_ehance_scale << 24;
	isp3_param_write(params_vdev, value, ISP33_YNR_MI_WEIGHT, id);

	value = (arg->dsfilt_diff_offset & 0x3ff) |
		(arg->dsfilt_center_wgt & 0x7ff) << 10 |
		(arg->dsfilt_strg & 0x3ff) << 21;
	isp3_param_write(params_vdev, value, ISP35_YNR_DSIIR_COE, id);

	value = ISP_PACK_2SHORT(arg->lo_spnr_strg, arg->lo_spnr_soft_thred_scale);
	isp3_param_write(params_vdev, value, ISP33_YNR_LO_STRG_DETAIL, id);

	value = (arg->lo_spnr_thumb_thred_scale & 0x3ff) |
		(arg->tex2lo_strg_mantissa & 0x7ff) << 12 |
		(arg->tex2lo_strg_exponent & 0xf) << 24;
	isp3_param_write(params_vdev, value, ISP33_YNR_LO_LIMIT_SCALE, id);

	value = arg->lo_spnr_wgt |
		(arg->lo_spnr_filt_center_wgt & 0x1fff) << 10 |
		arg->lo_enhance_scale << 24;
	isp3_param_write(params_vdev, value, ISP33_YNR_LO_WEIGHT, id);

	value = (arg->tex2lo_strg_upper_thred & 0x3ff) |
		(arg->tex2lo_strg_lower_thred & 0x3ff) << 12;
	isp3_param_write(params_vdev, value, ISP33_YNR_LO_TEXT_THRED, id);

	for (i = 0; i < ISP35_YNR_ADJ_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->lo_gain2wgt[i * 4],
				       arg->lo_gain2wgt[i * 4 + 1],
				       arg->lo_gain2wgt[i * 4 + 2],
				       arg->lo_gain2wgt[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_YNR_FUSION_WEIT_ADJ_0_3 + i * 4, id);
	}
	value = arg->lo_gain2wgt[i * 4];
	isp3_param_write(params_vdev, value, ISP33_YNR_FUSION_WEIT_ADJ_8, id);
}

static void
isp_ynr_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_YNR_GLOBAL_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en) {
		val |= ISP35_MODULE_EN;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1,
				    ISP3X_YNR_FST_FRAME, id);
	} else {
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	}
	isp3_param_write(params_vdev, val, ISP3X_YNR_GLOBAL_CTRL, id);
}

static void
isp_cnr_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp35_cnr_cfg *arg, u32 id)
{
	u32 i, value, ctrl, gain_ctrl;

	gain_ctrl = isp3_param_read(params_vdev, ISP3X_GAIN_CTRL, id);
	ctrl = isp3_param_read(params_vdev, ISP3X_CNR_CTRL, id);
	ctrl &= ISP35_MODULE_EN;

	ctrl |= !!arg->hsv_alpha_en << 18 |
		(arg->loflt_coeff & 0x3f) << 12 |
		!!arg->local_alpha_dis << 11 |
		!!arg->hiflt_wgt0_mode << 8 |
		!!arg->uv_dis << 6 |
		(arg->thumb_mode & 0x3) << 4 |
		!!arg->yuv422_mode << 2 |
		!!arg->exgain_bypass << 1;
	value = (arg->global_gain & 0x3ff) |
		(arg->global_gain_alpha & 0xf) << 12 |
		arg->local_gain_scale << 16;
	/* gain disable, using global gain for cnr */
	if (ctrl & ISP35_MODULE_EN && !(gain_ctrl & ISP35_MODULE_EN)) {
		ctrl |= BIT(1);
		value &= ~ISP3X_CNR_GLOBAL_GAIN_ALPHA_MAX;
		value |= BIT(15);
	}
	isp3_param_write(params_vdev, ctrl, ISP3X_CNR_CTRL, id);
	isp3_param_write(params_vdev, value, ISP3X_CNR_EXGAIN, id);

	value = ISP_PACK_2SHORT(arg->lobfflt_vsigma_uv, arg->lobfflt_vsigma_y);
	isp3_param_write(params_vdev, value, ISP32_CNR_THUMB1, id);

	value = arg->lobfflt_alpha;
	isp3_param_write(params_vdev, value, ISP32_CNR_THUMB_BF_RATIO, id);

	value = ISP_PACK_4BYTE(arg->thumb_bf_coeff0, arg->thumb_bf_coeff1,
			       arg->thumb_bf_coeff2, arg->thumb_bf_coeff3);
	isp3_param_write(params_vdev, value, ISP32_CNR_LBF_WEITD, id);

	value = (arg->loflt_uv_gain & 0xf) |
		arg->loflt_vsigma << 4 |
		(arg->exp_x_shift_bit & 0x3f) << 12 |
		(arg->loflt_wgt_slope & 0x3ff) << 20;
	isp3_param_write(params_vdev, value, ISP32_CNR_IIR_PARA1, id);

	value = ISP_PACK_4BYTE(arg->loflt_wgt_min_thred, arg->loflt_wgt_max_limit, 0, 0);
	isp3_param_write(params_vdev, value, ISP32_CNR_IIR_PARA2, id);

	value = ISP_PACK_4BYTE(arg->gaus_flt_coeff[0], arg->gaus_flt_coeff[1],
			       arg->gaus_flt_coeff[2], arg->gaus_flt_coeff[3]);
	isp3_param_write(params_vdev, value, ISP32_CNR_GAUS_COE1, id);

	value = ISP_PACK_4BYTE(arg->gaus_flt_coeff[4], arg->gaus_flt_coeff[5], 0, 0);
	isp3_param_write(params_vdev, value, ISP32_CNR_GAUS_COE2, id);

	value = (arg->gaus_flt_alpha & 0x7ff) |
		arg->hiflt_wgt_min_limit << 12 |
		(arg->hiflt_alpha & 0x7ff) << 20;
	isp3_param_write(params_vdev, value, ISP32_CNR_GAUS_RATIO, id);

	value = arg->hiflt_uv_gain |
		(arg->hiflt_global_vsigma & 0x3fff) << 8 |
		arg->hiflt_cur_wgt << 24;
	isp3_param_write(params_vdev, value, ISP32_CNR_BF_PARA1, id);

	value = ISP_PACK_2SHORT(arg->adj_offset, arg->adj_scale);
	isp3_param_write(params_vdev, value, ISP32_CNR_BF_PARA2, id);

	for (i = 0; i < ISP35_CNR_SIGMA_Y_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->sgm_ratio[i * 4], arg->sgm_ratio[i * 4 + 1],
				       arg->sgm_ratio[i * 4 + 2], arg->sgm_ratio[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP32_CNR_SIGMA0 + i * 4, id);
	}
	value = arg->sgm_ratio[i * 4] | arg->bf_merge_max_limit << 16;
	isp3_param_write(params_vdev, value, ISP32_CNR_SIGMA0 + i * 4, id);

	value = arg->loflt_global_sgm_ratio |
		(arg->loflt_global_sgm_ratio_alpha & 0xf) << 8 |
		(arg->bf_alpha_max_limit & 0x7ff) << 16;
	isp3_param_write(params_vdev, value, ISP32_CNR_IIR_GLOBAL_GAIN, id);

	for (i = 0; i < ISP35_CNR_WGT_SIGMA_Y_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->cur_wgt[i * 4], arg->cur_wgt[i * 4 + 1],
				       arg->cur_wgt[i * 4 + 2], arg->cur_wgt[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP39_CNR_WGT_SIGMA0 + i * 4, id);
	}
	value = arg->cur_wgt[i * 4];
	isp3_param_write(params_vdev, value, ISP39_CNR_WGT_SIGMA3, id);

	for (i = 0; i < ISP35_CNR_GAUS_SIGMAR_NUM / 3; i++) {
		value = (arg->hiflt_vsigma_idx[i * 3] & 0x3ff) |
			(arg->hiflt_vsigma_idx[i * 3 + 1] & 0x3ff) << 10 |
			(arg->hiflt_vsigma_idx[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_CNR_GAUS_X_SIGMAR0 + i * 4, id);
	}
	value = (arg->hiflt_vsigma_idx[i * 3] & 0x3ff) |
		(arg->hiflt_vsigma_idx[i * 3 + 1] & 0x3ff) << 10;
	isp3_param_write(params_vdev, value, ISP39_CNR_GAUS_X_SIGMAR2, id);

	for (i = 0; i < ISP35_CNR_GAUS_SIGMAR_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->hiflt_vsigma[i * 2], arg->hiflt_vsigma[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP39_CNR_GAUS_Y_SIGMAR0 + i * 4, id);
	}

	for (i = 0; i < ISP35_CNR_WGT_SIGMA_Y_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->lo_flt_vsigma[i * 4], arg->lo_flt_vsigma[i * 4 + 1],
				       arg->lo_flt_vsigma[i * 4 + 2], arg->lo_flt_vsigma[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP35_CNR_IIR_SIGMAR0 + i * 4, id);
	}
	value = arg->lo_flt_vsigma[i * 4];
	isp3_param_write(params_vdev, value, ISP35_CNR_IIR_SIGMAR3, id);

	for (i = 0; i < ISP35_CNR_CURVE_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->hsv_adj_alpha_table[i * 4],
				       arg->hsv_adj_alpha_table[i * 4 + 1],
				       arg->hsv_adj_alpha_table[i * 4 + 2],
				       arg->hsv_adj_alpha_table[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP35_CNR_HSV_CURVE0 + i * 4, id);
		value = ISP_PACK_4BYTE(arg->sat_adj_alpha_table[i * 4],
				       arg->sat_adj_alpha_table[i * 4 + 1],
				       arg->sat_adj_alpha_table[i * 4 + 2],
				       arg->sat_adj_alpha_table[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP35_CNR_SAT_CURVE0 + i * 4, id);
		value = ISP_PACK_4BYTE(arg->gain_adj_alpha_table[i * 4],
				       arg->gain_adj_alpha_table[i * 4 + 1],
				       arg->gain_adj_alpha_table[i * 4 + 2],
				       arg->gain_adj_alpha_table[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP35_CNR_GAIN_ADJ_CURVE0 + i * 4, id);
	}
	value = arg->hsv_adj_alpha_table[i * 4] |
		arg->hsv_adj_alpha_table[i * 4 + 1] << 8;
	isp3_param_write(params_vdev, value, ISP35_CNR_HSV_CURVE2, id);
	value = arg->sat_adj_alpha_table[i * 4] |
		arg->sat_adj_alpha_table[i * 4 + 1] << 8;
	isp3_param_write(params_vdev, value, ISP35_CNR_SAT_CURVE2, id);
	value = arg->gain_adj_alpha_table[i * 4] |
		arg->gain_adj_alpha_table[i * 4 + 1] << 8;
	isp3_param_write(params_vdev, value, ISP35_CNR_GAIN_ADJ_CURVE2, id);
}

static void
isp_cnr_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_CNR_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en) {
		val |= ISP35_MODULE_EN;
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1,
				    ISP3X_CNR_FST_FRAME, id);
	} else {
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	}
	isp3_param_write(params_vdev, val, ISP3X_CNR_CTRL, id);
}

static void
isp_sharp_cfg_noise_curve(struct rkisp_isp_params_vdev *params_vdev,
			  const struct isp35_sharp_cfg *arg, u32 id, bool direct)
{
	struct rkisp_device *dev = params_vdev->dev;
	u32 i, value;

	for (i = 0; i < ISP35_SHARP_NOISE_CURVE_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->noise_curve_ext[i * 2],
					arg->noise_curve_ext[i * 2 + 1]);
		rkisp_idx_write(dev, ISP33_SHARP_NOISE_CURVE0 + i * 4, value, id, direct);
	}
	value = (arg->noise_curve_ext[i * 2] & 0x7ff) |
		arg->noise_count_thred_ratio << 12 |
		arg->noise_clip_scale << 20;
	rkisp_idx_write(dev, ISP33_SHARP_NOISE_CURVE8, value, id, direct);
}

static void
isp_sharp_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp35_sharp_cfg *arg, u32 id)
{
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct isp35_sharp_cfg *arg_rec = &params_rec->others.sharp_cfg;
	u32 i, value, ctrl;

	ctrl = isp3_param_read(params_vdev, ISP3X_SHARP_EN, id);
	ctrl &= ISP35_MODULE_EN;
	ctrl |= !!arg->bypass << 1 |
		!!arg->local_gain_bypass << 2 |
		!!arg->tex_est_mode << 3 |
		!!arg->max_min_flt_mode << 4 |
		!!arg->detail_fusion_wgt_mode << 5 |
		!!arg->noise_calc_mode << 6 |
		!!arg->radius_step_mode << 7 |
		!!arg->noise_curve_mode << 8 |
		!!arg->gain_wgt_mode << 9 |
		!!arg->detail_lp_en << 10 |
		(arg->debug_mode & 0x7) << 12;
	isp3_param_write(params_vdev, ctrl, ISP3X_SHARP_EN, id);

	value = ISP_PACK_2SHORT(arg->fst_noise_scale, arg->fst_sigma_scale);
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEXTURE0, id);

	value = ISP_PACK_2SHORT(arg->fst_sigma_offset, arg->fst_wgt_scale);
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEXTURE1, id);

	value = (arg->tex_wgt_mode & 0x3) << 8 |
		(arg->noise_est_alpha & 0x3f) << 12;
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEXTURE2, id);

	value = ISP_PACK_2SHORT(arg->sec_noise_scale, arg->sec_sigma_scale);
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEXTURE3, id);

	value = ISP_PACK_2SHORT(arg->sec_sigma_offset, arg->sec_wgt_scale);
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEXTURE4, id);

	value = arg->img_hpf_coeff[0] << 24;
	isp3_param_write(params_vdev, value, ISP33_SHARP_HPF_KERNEL0, id);
	value = ISP_PACK_4BYTE(arg->img_hpf_coeff[1], arg->img_hpf_coeff[2],
			       arg->img_hpf_coeff[3], arg->img_hpf_coeff[4]);
	isp3_param_write(params_vdev, value, ISP33_SHARP_HPF_KERNEL1, id);

	value = ISP_PACK_4BYTE(arg->img_hpf_coeff[5], arg->texWgt_flt_coeff0,
			       arg->texWgt_flt_coeff1, arg->texWgt_flt_coeff2);
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEXFLT_KERNEL, id);

	value = arg->detail_in_alpha |
		(arg->pre_bifilt_slope_fix & 0x7ff) << 8 |
		(arg->pre_bifilt_alpha & 0x3f) << 20 |
		!!arg->fusion_wgt_min_limit << 28 |
		!!arg->fusion_wgt_max_limit << 29;
	isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL0, id);

	value = (arg->luma_dx[6] & 0x0F) << 24 |
		(arg->luma_dx[5] & 0x0F) << 20 |
		(arg->luma_dx[4] & 0x0F) << 16 |
		(arg->luma_dx[3] & 0x0F) << 12 |
		(arg->luma_dx[2] & 0x0F) << 8 |
		(arg->luma_dx[1] & 0x0F) << 4 |
		(arg->luma_dx[0] & 0x0F);
	isp3_param_write(params_vdev, value, ISP33_SHARP_LUMA_DX, id);

	for (i = 0; i < ISP35_SHARP_Y_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->pre_bifilt_vsigma_inv[i * 2],
					arg->pre_bifilt_vsigma_inv[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_SHARP_PBF_VSIGMA0 + i * 4, id);
	}

	value = (arg->pre_bifilt_coeff0 & 0x3f) |
		(arg->pre_bifilt_coeff1 & 0x3f) << 8 |
		(arg->pre_bifilt_coeff2 & 0x3f) << 16;
	isp3_param_write(params_vdev, value, ISP33_SHARP_PBF_KERNEL, id);

	value = ISP_PACK_4BYTE(arg->hi_detail_lpf_coeff[0], arg->hi_detail_lpf_coeff[1],
			       arg->hi_detail_lpf_coeff[2], arg->hi_detail_lpf_coeff[3]);
	isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_KERNEL0, id);
	value = ISP_PACK_4BYTE(arg->hi_detail_lpf_coeff[4], arg->hi_detail_lpf_coeff[5],
			       arg->mi_detail_lpf_coeff[0], arg->mi_detail_lpf_coeff[1]);
	isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_KERNEL1, id);
	value = ISP_PACK_4BYTE(arg->mi_detail_lpf_coeff[2], arg->mi_detail_lpf_coeff[3],
			       arg->mi_detail_lpf_coeff[4], arg->mi_detail_lpf_coeff[5]);
	isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_KERNEL2, id);

	value = arg->global_gain | arg->gain_merge_alpha << 16 | arg->local_gain_scale << 24;
	isp3_param_write(params_vdev, value, ISP33_SHARP_GAIN, id);

	value = ISP_PACK_4BYTE(arg->edge_gain_max_limit, arg->edge_gain_min_limit,
			       arg->detail_gain_max_limit, arg->detail_gain_min_limit);
	isp3_param_write(params_vdev, value, ISP33_SHARP_GAIN_ADJ0, id);

	value = ISP_PACK_4BYTE(arg->hitex_gain_max_limit, arg->hitex_gain_min_limit, 0, 0);
	isp3_param_write(params_vdev, value, ISP33_SHARP_GAIN_ADJ1, id);

	value = ISP_PACK_4BYTE(arg->edge_gain_slope, arg->detail_gain_slope,
			       arg->hitex_gain_slope, 0);
	isp3_param_write(params_vdev, value, ISP33_SHARP_GAIN_ADJ2, id);

	value = (arg->edge_gain_offset & 0x3ff) |
		(arg->detail_gain_offset & 0x3ff) << 10 |
		(arg->hitex_gain_offset & 0x3ff) << 20;
	isp3_param_write(params_vdev, value, ISP33_SHARP_GAIN_ADJ3, id);

	value = ISP_PACK_2SHORT(arg->edge_gain_sigma, arg->detail_gain_sigma);
	isp3_param_write(params_vdev, value, ISP33_SHARP_GAIN_ADJ4, id);

	value = ISP_PACK_2SHORT(arg->pos_edge_wgt_scale, arg->neg_edge_wgt_scale);
	isp3_param_write(params_vdev, value, ISP33_SHARP_EDGE0, id);

	value = ISP_PACK_4BYTE(arg->pos_edge_strg, arg->neg_edge_strg,
			       arg->overshoot_alpha, arg->undershoot_alpha);
	isp3_param_write(params_vdev, value, ISP33_SHARP_EDGE1, id);

	for (i = 0; i < ISP35_SHARP_EDGE_KERNEL_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->edge_bpf_coeff[i * 4],
				       arg->edge_bpf_coeff[i * 4 + 1],
				       arg->edge_bpf_coeff[i * 4 + 2],
				       arg->edge_bpf_coeff[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_SHARP_EDGE_KERNEL0 + i * 4, id);
	}
	value = ISP_PACK_4BYTE(arg->edge_bpf_coeff[i * 4], arg->edge_bpf_coeff[i * 4 + 1], 0, 0);
	isp3_param_write(params_vdev, value, ISP33_SHARP_EDGE_KERNEL2, id);

	for (i = 0; i < ISP35_SHARP_EDGE_WGT_NUM / 3; i++) {
		value = (arg->edge_wgt_val[i * 3] & 0x3ff) |
			(arg->edge_wgt_val[i * 3 + 1] & 0x3ff) << 10 |
			(arg->edge_wgt_val[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_SHARP_EDGE_WGT_VAL0 + i * 4, id);
	}
	value = (arg->edge_wgt_val[i * 3] & 0x3ff) |
		(arg->edge_wgt_val[i * 3 + 1] & 0x3ff) << 10;
	isp3_param_write(params_vdev, value, ISP33_SHARP_EDGE_WGT_VAL5, id);

	for (i = 0; i < ISP35_SHARP_LUMA_STRG_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->luma2strg[i * 4], arg->luma2strg[i * 4 + 1],
				       arg->luma2strg[i * 4 + 2], arg->luma2strg[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_SHARP_LUMA_ADJ_STRG0 + i * 4, id);
	}

	value = ISP_PACK_2SHORT(arg->center_x, arg->center_y);
	isp3_param_write(params_vdev, value, ISP33_SHARP_CENTER, id);

	value = ISP_PACK_2SHORT(arg->flat_max_limit, arg->edge_min_limit);
	isp3_param_write(params_vdev, value, ISP33_SHARP_OUT_LIMIT, id);

	value = arg->tex_x_inv_fix0;
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEX_X_INV_FIX0, id);
	value = arg->tex_x_inv_fix1;
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEX_X_INV_FIX1, id);
	value = arg->tex_x_inv_fix2;
	isp3_param_write(params_vdev, value, ISP33_SHARP_TEX_X_INV_FIX2, id);

	value = (arg->tex2loss_tex_in_hinr_strg[0] & 0x3ff) << 10 |
		(arg->tex2loss_tex_in_hinr_strg[1] & 0x3ff) << 20;
	isp3_param_write(params_vdev, value, ISP33_SHARP_LOCAL_STRG1, id);
	value = (arg->tex2loss_tex_in_hinr_strg[2] & 0x3ff) |
		(arg->tex2loss_tex_in_hinr_strg[3] & 0x3ff) << 10;
	isp3_param_write(params_vdev, value, ISP33_SHARP_LOCAL_STRG2, id);

	for (i = 0; i < ISP35_SHARP_CONTRAST_STRG_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->contrast2pos_strg[i * 4],
				       arg->contrast2pos_strg[i * 4 + 1],
				       arg->contrast2pos_strg[i * 4 + 2],
				       arg->contrast2pos_strg[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_SCALE_TAB0 + i * 4, id);
		value = ISP_PACK_4BYTE(arg->contrast2neg_strg[i * 4],
				       arg->contrast2neg_strg[i * 4 + 1],
				       arg->contrast2neg_strg[i * 4 + 2],
				       arg->contrast2neg_strg[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_SCALE_TAB3 + i * 4, id);
	}
	value = arg->contrast2pos_strg[i * 4] | arg->pos_detail_strg << 8;
	isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_SCALE_TAB2, id);
	value = arg->contrast2neg_strg[i * 4] | arg->neg_detail_strg << 8;
	isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_SCALE_TAB5, id);

	for (i = 0; i < ISP35_SHARP_TEX_CLIP_NUM / 3; i++) {
		value = (arg->tex2detail_pos_clip[i * 3] & 0x3ff) |
			(arg->tex2detail_pos_clip[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tex2detail_pos_clip[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_TEX_CLIP0 + i * 4, id);
		value = (arg->tex2detail_neg_clip[i * 3] & 0x3ff) |
			(arg->tex2detail_neg_clip[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tex2detail_neg_clip[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_TEX_CLIP3 + i * 4, id);

		value = (arg->tex2grain_pos_clip[i * 3] & 0x3ff) |
			(arg->tex2grain_pos_clip[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tex2grain_pos_clip[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_SHARP_GRAIN_TEX_CLIP0 + i * 4, id);
		value = (arg->tex2grain_neg_clip[i * 3] & 0x3ff) |
			(arg->tex2grain_neg_clip[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tex2grain_neg_clip[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_SHARP_GRAIN_TEX_CLIP3 + i * 4, id);
	}

	for (i = 0; i < ISP35_SHARP_LUM_CLIP_NUM / 3; i++) {
		value = (arg->luma2detail_pos_clip[i * 3] & 0x3ff) |
			(arg->luma2detail_pos_clip[i * 3 + 1] & 0x3ff) << 10 |
			(arg->luma2detail_pos_clip[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_LUMA_CLIP0 + i * 4, id);

		value = (arg->luma2detail_neg_clip[i * 3] & 0x3ff) |
			(arg->luma2detail_neg_clip[i * 3 + 1] & 0x3ff) << 10 |
			(arg->luma2detail_neg_clip[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_LUMA_CLIP3 + i * 4, id);
	}
	value = (arg->luma2detail_pos_clip[i * 3] & 0x3ff) |
		(arg->luma2detail_pos_clip[i * 3 + 1] & 0x3ff) << 10;
	isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_LUMA_CLIP2, id);
	value = (arg->luma2detail_neg_clip[i * 3] & 0x3ff) |
		(arg->luma2detail_neg_clip[i * 3 + 1] & 0x3ff) << 10;
	isp3_param_write(params_vdev, value, ISP33_SHARP_DETAIL_LUMA_CLIP5, id);

	value = arg->grain_strg;
	isp3_param_write(params_vdev, value, ISP33_SHARP_GRAIN_STRG, id);

	for (i = 0; i < ISP35_SHARP_HUE_NUM / 3; i++) {
		value = (arg->hue2strg[i * 3] & 0x3ff) |
			(arg->hue2strg[i * 3 + 1] & 0x3ff) << 10 |
			(arg->hue2strg[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_SHARP_HUE_ADJ_TAB0 + i * 4, id);
	}

	for (i = 0; i < ISP35_SHARP_DISATANCE_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->distance2strg[i * 4],
				       arg->distance2strg[i * 4 + 1],
				       arg->distance2strg[i * 4 + 2],
				       arg->distance2strg[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP33_SHARP_DISATANCE_ADJ0 + i * 4, id);
	}
	value = ISP_PACK_4BYTE(arg->distance2strg[i * 4],
			       arg->distance2strg[i * 4 + 1],
			       arg->distance2strg[i * 4 + 2], 0);
	isp3_param_write(params_vdev, value, ISP33_SHARP_DISATANCE_ADJ2, id);

	for (i = 0; i < ISP35_SHARP_TEX_NUM / 3; i++) {
		value = (arg->tex2detail_strg[i * 3] & 0x3ff) |
			(arg->tex2detail_strg[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tex2detail_strg[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP35_SHARP_TEX2DETAIL_STRG0 + i * 4, id);
	}

	for (i = 0; i < ISP35_SHARP_TEX_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->hi_tex_threshold[i * 2],
					arg->hi_tex_threshold[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_SHARP_NOISE_SIGMA0 + i * 4, id);
	}
	value = arg->hi_tex_threshold[i * 2];
	isp3_param_write(params_vdev, value, ISP33_SHARP_NOISE_SIGMA4, id);

	for (i = 0; i < ISP35_SHARP_TEX_NUM / 3; i++) {
		value = (arg->tex2mf_detail_strg[i * 3] & 0x3ff) |
			(arg->tex2mf_detail_strg[i * 3 + 1] & 0x3ff) << 10 |
			(arg->tex2mf_detail_strg[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP35_SHARP_TEX2MFDETAIL_STRG0 + i * 4, id);
	}

	value = arg->loss_tex_in_hinr_strg;
	isp3_param_write(params_vdev, value, ISP33_SHARP_LOSSTEXINHINR_STRG, id);

	value = ISP_PACK_2SHORT(arg->noise_clip_min_limit, arg->noise_clip_max_limit);
	isp3_param_write(params_vdev, value, ISP33_SHARP_NOISE_CLIP, id);

	value = arg->edge_wgt_flt_coeff0 |
		arg->edge_wgt_flt_coeff1 << 8 |
		arg->edge_wgt_flt_coeff2 << 16;
	isp3_param_write(params_vdev, value, ISP35_SHARP_EDGEWGTFLT_KERNEL, id);

	value = (arg->edge_glb_clip_thred & 0x3ff) |
		(arg->pos_edge_clip & 0x3ff) << 10 |
		(arg->neg_edge_clip & 0x3ff) << 20;
	isp3_param_write(params_vdev, value, ISP35_SHARP_EDGE_GLOBAL_CLIP, id);

	value = arg->mf_detail_data_alpha |
		arg->pos_mf_detail_strg << 8 |
		arg->neg_mf_detail_strg << 16;
	isp3_param_write(params_vdev, value, ISP35_SHARP_MFDETAIL, id);

	value = (arg->mf_detail_pos_clip & 0x3ff) |
		(arg->sharp_mf_detail_neg_clip & 0x3ff) << 10;
	isp3_param_write(params_vdev, value, ISP35_SHARP_MFDETAIL_CLIP, id);

	for (i = 0; i < ISP35_SHARP_SATURATION_NUM / 4; i++) {
		value = ISP_PACK_4BYTE(arg->staturation2strg[i * 4],
				       arg->staturation2strg[i * 4 + 1],
				       arg->staturation2strg[i * 4 + 2],
				       arg->staturation2strg[i * 4 + 3]);
		isp3_param_write(params_vdev, value, ISP35_SHARP_SATURATION_STRG0 + i * 4, id);
	}
	value = (arg->staturation2strg[i * 4] & 0x1f) | arg->lo_saturation_strg << 8;
	isp3_param_write(params_vdev, value, ISP35_SHARP_SATURATION_STRG2, id);

	/* SHARP_NOISE_CURVE read back is not the config value, need to save */
	if (!(ctrl & ISP35_MODULE_EN) || arg->noise_curve_mode)
		memcpy(arg_rec->noise_curve_ext,
		       arg->noise_curve_ext, sizeof(arg->noise_curve_ext));
	arg_rec->noise_count_thred_ratio = arg->noise_count_thred_ratio;
	arg_rec->noise_clip_scale = arg->noise_clip_scale;
	isp_sharp_cfg_noise_curve(params_vdev, arg_rec, id, false);
}

static void
isp_sharp_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_SHARP_EN, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en) {
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP32_SHP_FST_FRAME, id);
		val |= ISP35_MODULE_EN;
	} else {
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	}
	isp3_param_write(params_vdev, val, ISP3X_SHARP_EN, id);
}

static void
isp_bay3d_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp35_bay3d_cfg *arg, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct isp2x_mesh_head *head;
	u32 i, value, ctrl, buf_idx;

	ctrl = isp3_param_read(params_vdev, ISP33_BAY3D_CTRL0, id);
	if (ctrl & BIT(1) && !arg->bypass_en)
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP3X_RAW3D_FST_FRAME, id);
	ctrl &= ISP35_MODULE_EN;

	value = arg->iir_rw_fmt;
	if (value != priv->bay3d_iir_rw_fmt) {
		dev_err(dev->dev, "%s iir_rw_fmt:%d unequal to init fmt:%d\n",
			__func__, value, priv->bay3d_iir_rw_fmt);
		value = priv->bay3d_iir_rw_fmt;
	}
	ctrl |= (value & 0x3) << 13 |
		!!arg->motion_est_en << 8 |
		(arg->out_use_pre_mode & 0x7) << 5 |
		!!arg->iir_wr_src << 3 |
		!!arg->bypass_en << 1;
	isp3_param_write(params_vdev, ctrl, ISP33_BAY3D_CTRL0, id);

	value = isp3_param_read(params_vdev, ISP39_W3A_CTRL0, id);
	if ((arg->transf_bypass_en && !(value & ISP35_W3A_B3DNROUT_ILG_BYPASS)) ||
	    (!arg->transf_bypass_en && value & ISP35_W3A_B3DNROUT_ILG_BYPASS)) {
		if (arg->transf_bypass_en)
			value |= ISP35_W3A_B3DNROUT_ILG_BYPASS;
		else
			value &= ~ISP35_W3A_B3DNROUT_ILG_BYPASS;
		isp3_param_write(params_vdev, value, ISP39_W3A_CTRL0, id);
	}

	value = !!arg->md_wgt_out_en << 25 |
		!!arg->cur_spnr_out_en << 22 |
		!!arg->md_only_lo_en << 21 |
		!!arg->pre_spnr_out_en << 20 |
		(arg->lo_mge_wgt_mode & 0x3) << 16 |
		!!arg->lo_detection_bypass_en << 15 |
		!!arg->sig_hfilt_en << 13 |
		!!arg->lo_diff_hfilt_en << 12 |
		!!arg->lo_wgt_hfilt_en << 11 |
		!!arg->lpf_lo_bypass_en << 10 |
		!!arg->lo_diff_vfilt_bypass_en << 9 |
		!!arg->lpf_hi_bypass_en << 8 |
		!!arg->motion_detect_bypass_en << 7 |
		!!arg->pre_pix_out_mode << 6 |
		!!arg->md_large_lo_md_wgt_bypass_en << 5 |
		!!arg->md_large_lo_gauss_filter_bypass_en << 4 |
		!!arg->md_large_lo_min_filter_bypass_en << 3 |
		!!arg->md_large_lo_use_mode << 2 |
		!!arg->tnrsigma_curve_double_en << 1 |
		!!arg->transf_bypass_en;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_CTRL1, id);

	value = !!arg->pre_spnr_dpc_flt_prewgt_en << 26 |
		!!arg->pre_spnr_dpc_flt_mode << 25 |
		!!arg->pre_spnr_dpc_nr_bal_mode << 24 |
		!!arg->pre_spnr_dpc_flt_en << 23 |
		!!arg->pre_lo_avg_lp_en << 22 |
		!!arg->pre_hi_bf_lp_en << 21 |
		!!arg->pre_hi_gic_lp_en << 20 |
		!!arg->pre_spnr_lo_filter_rb_wgt_mode << 15 |
		!!arg->pre_spnr_hi_filter_rb_wgt_mode << 14 |
		!!arg->pre_spnr_lo_filter_wgt_mode << 13 |
		!!arg->pre_spnr_hi_filter_wgt_mode << 12 |
		!!arg->pre_spnr_hi_noise_ctrl_en << 11 |
		!!arg->pre_spnr_sigma_idx_filt_mode << 10 |
		!!arg->pre_spnr_sigma_idx_filt_bypass_en << 9 |
		!!arg->pre_spnr_hi_guide_filter_bypass_en << 8 |
		!!arg->pre_spnr_sigma_curve_double_en << 7 |
		!!arg->pre_spnr_hi_filter_bypass_en << 6 |
		!!arg->pre_spnr_lo_filter_bypass_en << 5 |
		!!arg->spnr_presigma_use_en << 4 |
		!!arg->pre_spnr_hi_filter_gic_enhance_en << 3 |
		!!arg->pre_spnr_hi_filter_gic_en << 2 |
		!!arg->cur_spnr_filter_bypass_en;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_CTRL2, id);

	value = (arg->sigma_calc_mge_wgt_hdr_sht_thred & 0x3f) << 24 |
		(arg->mge_wgt_hdr_sht_thred & 0x3f) << 16 |
		(arg->kalman_wgt_ds_mode & 0x3) << 3 |
		!!arg->mge_wgt_ds_mode << 2 |
		!!arg->wgt_cal_mode << 1 |
		!!arg->transf_mode;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_CTRL3, id);

	value = arg->itransf_mode_offset << 16 |
		(arg->transf_mode_scale & 0x3) << 14 |
		(arg->transf_mode_offset & 0x1fff);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_TRANS0, id);

	value = arg->transf_data_max_limit;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_TRANS1, id);

	value = arg->pre_spnr_sigma_ctrl_scale;
	isp3_param_write(params_vdev, value, ISP35_BAY3D_PREHI_SIGSCL, id);

	value = arg->pre_spnr_hi_guide_out_wgt;
	isp3_param_write(params_vdev, value, ISP35_BAY3D_PREHI_SIGOF, id);

	value = arg->cur_spnr_filter_coeff[0] |
		arg->cur_spnr_filter_coeff[1] << 8 |
		arg->cur_spnr_filter_coeff[2] << 16;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_CURHISPW0, id);
	value = arg->cur_spnr_filter_coeff[3] |
		arg->cur_spnr_filter_coeff[4] << 8 |
		arg->cur_spnr_filter_coeff[5] << 16;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_CURHISPW1, id);

	for (i = 0; i < ISP35_BAY3D_XY_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->pre_spnr_luma2sigma_x[i * 2],
					arg->pre_spnr_luma2sigma_x[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_BAY3D_IIRSX0 + i * 4, id);
		value = ISP_PACK_2SHORT(arg->pre_spnr_luma2sigma_y[i * 2],
					arg->pre_spnr_luma2sigma_y[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_BAY3D_IIRSY0 + i * 4, id);
	}

	value = arg->pre_spnr_hi_sigma_scale;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PREHI_SIGSCL, id);

	value = arg->pre_spnr_hi_wgt_calc_scale;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PREHI_WSCL, id);

	value = arg->pre_spnr_hi_filter_wgt_min_limit |
		arg->pre_spnr_hi_wgt_calc_offset << 16;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PREHIWMM, id);

	value = arg->pre_spnr_sigma_hdr_sht_offset << 24 |
		arg->pre_spnr_sigma_offset << 16 |
		arg->pre_spnr_hi_filter_out_wgt;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PREHISIGOF, id);

	value = ISP_PACK_2SHORT(arg->pre_spnr_sigma_scale,
				arg->pre_spnr_sigma_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PREHISIGSCL, id);

	value = arg->pre_spnr_hi_filter_coeff[0] |
		arg->pre_spnr_hi_filter_coeff[1] << 8 |
		arg->pre_spnr_hi_filter_coeff[2] << 16;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PREHISPW0, id);
	value = arg->pre_spnr_hi_filter_coeff[3] |
		arg->pre_spnr_hi_filter_coeff[4] << 8 |
		arg->pre_spnr_hi_filter_coeff[5] << 16;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PREHISPW1, id);

	value = arg->pre_spnr_lo_sigma_scale;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PRELOSIGCSL, id);

	value = arg->pre_spnr_lo_wgt_calc_offset |
		arg->pre_spnr_lo_wgt_calc_scale << 16;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PRELOSIGOF, id);

	value = arg->pre_spnr_hi_noise_ctrl_offset << 16 |
		arg->pre_spnr_hi_noise_ctrl_scale;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PREHI_NRCT, id);

	for (i = 0; i < ISP35_BAY3D_TNRSIG_NUM / 2; i++) {
		value = ISP_PACK_2SHORT(arg->tnr_luma2sigma_x[i * 2],
					arg->tnr_luma2sigma_x[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_BAY3D_TNRSX0 + i * 4, id);
		value = ISP_PACK_2SHORT(arg->tnr_luma2sigma_y[i * 2],
					arg->tnr_luma2sigma_y[i * 2 + 1]);
		isp3_param_write(params_vdev, value, ISP33_BAY3D_TNRSY0 + i * 4, id);
	}

	for (i = 0; i < ISP35_BAY3D_LPF_COEFF_NUM / 3; i++) {
		value = (arg->lpf_hi_coeff[i * 3] & 0x3ff) |
			(arg->lpf_hi_coeff[i * 3 + 1] & 0x3ff) << 10 |
			(arg->lpf_hi_coeff[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_BAY3D_HIWD0 + i * 4, id);
		value = (arg->lpf_lo_coeff[i * 3] & 0x3ff) |
			(arg->lpf_lo_coeff[i * 3 + 1] & 0x3ff) << 10 |
			(arg->lpf_lo_coeff[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP33_BAY3D_LOWD0 + i * 4, id);
	}

	value = ISP_PACK_4BYTE(arg->sigma_idx_filt_coeff[0],
			       arg->sigma_idx_filt_coeff[1],
			       arg->sigma_idx_filt_coeff[2],
			       arg->sigma_idx_filt_coeff[3]);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_GF3, id);
	value = arg->sigma_idx_filt_coeff[4] |
		arg->sigma_idx_filt_coeff[5] << 8 |
		arg->lo_wgt_cal_first_line_sigma_scale << 16;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_GF4, id);

	value = (arg->lo_diff_first_line_scale & 0x3f) << 22 |
		(arg->sig_first_line_scale & 0x3f) << 16 |
		(arg->lo_wgt_vfilt_wgt & 0x1f) << 5 |
		(arg->lo_diff_vfilt_wgt & 0x1f);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_VIIR, id);

	value = ISP_PACK_2SHORT(arg->lo_wgt_cal_offset,
				arg->lo_wgt_cal_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_LFSCL, id);

	value = ISP_PACK_2SHORT(arg->lo_wgt_cal_max_limit,
				arg->mode0_base_ratio);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_LFSCLTH, id);

	value = ISP_PACK_2SHORT(arg->lo_diff_wgt_cal_offset,
				arg->lo_diff_wgt_cal_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_DSWGTSCL, id);

	value = ISP_PACK_2SHORT(arg->lo_mge_pre_wgt_offset,
				arg->lo_mge_pre_wgt_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTLASTSCL, id);

	value = ISP_PACK_2SHORT(arg->mode0_lo_wgt_scale,
				arg->mode0_lo_wgt_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTSCL0, id);

	value = ISP_PACK_2SHORT(arg->mode1_lo_wgt_scale,
				arg->mode1_lo_wgt_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTSCL1, id);

	value = ISP_PACK_2SHORT(arg->mode1_wgt_scale,
				arg->mode1_wgt_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTSCL2, id);

	value = ISP_PACK_2SHORT(arg->mode1_lo_wgt_offset,
				arg->mode1_lo_wgt_hdr_sht_offset);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTOFF, id);

	value = (arg->mode1_wgt_offset & 0xfff) << 20 |
		(arg->mode1_wgt_min_limit & 0x3ff) << 10 |
		(arg->auto_sigma_count_wgt_thred & 0x3ff);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGT1OFF, id);

	value = arg->tnr_out_sigma_sq;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_SIGORG, id);

	value = ISP_PACK_2SHORT(arg->lo_wgt_clip_min_limit,
				arg->lo_wgt_clip_hdr_sht_min_limit);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTLO_L, id);

	value = ISP_PACK_2SHORT(arg->lo_wgt_clip_max_limit,
				arg->lo_wgt_clip_hdr_sht_max_limit);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTLO_H, id);

	value = ISP_PACK_2SHORT(arg->lo_pre_gg_soft_thresh_scale,
				arg->lo_pre_rb_soft_thresh_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_STH_SCL, id);

	value = ISP_PACK_2SHORT(arg->lo_pre_soft_thresh_max_limit,
				arg->lo_pre_soft_thresh_min_limit);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_STH_LIMIT, id);

	value = (arg->motion_est_lo_wgt_thred & 0x3ff) << 16 |
		arg->pre_spnr_hi_wgt_min_limit << 8 |
		arg->cur_spnr_hi_wgt_min_limit;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_HIKEEP, id);

	value = arg->pix_max_limit;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PIXMAX, id);

	value = arg->sigma_num_th;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_SIGNUMTH, id);

	value = arg->gain_out_max_limit << 24 |
		(arg->out_use_md_noise_bal_nr_strg & 0x7ff) << 11 |
		(arg->out_use_hi_noise_bal_nr_strg & 0x7ff);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_MONR, id);

	value = ISP_PACK_2SHORT(arg->sigma_scale, arg->sigma_hdr_sht_scale);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_SIGSCL, id);

	value = arg->lo_wgt_cal_first_line_vfilt_wgt << 24 |
		(arg->lo_diff_vfilt_offset & 0xfff) << 10 |
		(arg->lo_wgt_vfilt_offset & 0x3ff);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_DSOFF, id);

	value = ISP_PACK_4BYTE(arg->lo_wgt_vfilt_scale,
			       arg->lo_diff_vfilt_scale_bit,
			       arg->lo_diff_vfilt_scale,
			       arg->lo_diff_first_line_vfilt_wgt);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_DSSCL, id);

	value = (arg->motion_est_sad_vert_wgt0 & 0x3) << 28 |
		(arg->motion_est_up_mvx_cost_scale & 0x7ff) << 16 |
		arg->motion_est_up_mvx_cost_offset;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_ME0, id);

	value = (arg->motion_est_sad_vert_wgt1 & 0x3) << 28 |
		(arg->motion_est_up_left_mvx_cost_scale & 0x7ff) << 16 |
		arg->motion_est_up_left_mvx_cost_offset;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_ME1, id);

	value = (arg->motion_est_sad_vert_wgt2 & 0x3) << 28 |
		(arg->motion_est_up_right_mvx_cost_scale & 0x7ff) << 16 |
		arg->motion_est_up_right_mvx_cost_offset;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_ME2, id);

	value = arg->lo_wgt_clip_motion_max_limit;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTMAX, id);

	value = arg->mode1_wgt_max_limit;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGT1MAX, id);

	value = ISP_PACK_2SHORT(arg->mode0_wgt_out_max_limit, arg->mode0_wgt_out_offset);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_WGTM0, id);

	value = (arg->lo_wgt_hflt_coeff2 & 0x7) |
		(arg->lo_wgt_hflt_coeff1 & 0xf) << 4 |
		(arg->lo_wgt_hflt_coeff0 & 0x1f) << 8 |
		(arg->sig_hflt_coeff2 & 0x7) << 16 |
		(arg->sig_hflt_coeff1 & 0xf) << 20 |
		(arg->sig_hflt_coeff0 & 0x1f) << 24;
	isp3_param_write(params_vdev, value, ISP35_BAY3D_LOCOEF0, id);
	value = (arg->lo_dif_hflt_coeff2 & 0x7) |
		(arg->lo_dif_hflt_coeff1 & 0xf) << 4 |
		(arg->lo_dif_hflt_coeff0 & 0x1f) << 8;
	isp3_param_write(params_vdev, value, ISP35_BAY3D_LOCOEF1, id);

	value = (arg->pre_spnr_dpc_bright_str & 0x3) |
		(arg->pre_spnr_dpc_dark_str & 0x3) << 2 |
		(arg->pre_spnr_dpc_str & 0x7) << 4 |
		arg->pre_spnr_dpc_wk_scale << 8 |
		arg->pre_spnr_dpc_wk_offset << 16;
	isp3_param_write(params_vdev, value, ISP35_BAY3D_DPC0, id);
	value = ISP_PACK_2SHORT(arg->pre_spnr_dpc_nr_bal_str,
				arg->pre_spnr_dpc_soft_thr_scale);
	isp3_param_write(params_vdev, value, ISP35_BAY3D_DPC1, id);

	value = ISP_PACK_4BYTE(arg->pre_spnr_lo_val_wgt_out_wgt,
			       arg->pre_spnr_lo_filter_out_wgt,
			       arg->pre_spnr_lo_filter_wgt_min, 0);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_PRELOWGT, id);

	value = arg->md_large_lo_md_wgt_scale << 16 |
		arg->md_large_lo_md_wgt_offset;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_MIDBIG0, id);

	value = ISP_PACK_2SHORT(arg->md_large_lo_wgt_cut_offset,
				arg->md_large_lo_wgt_add_offset);
	isp3_param_write(params_vdev, value, ISP33_BAY3D_MIDBIG1, id);

	value = arg->md_large_lo_wgt_scale;
	isp3_param_write(params_vdev, value, ISP33_BAY3D_MIDBIG2, id);

	value = (arg->out_use_hi_noise_bal_nr_off & 0xfff) |
		(arg->out_use_md_noise_bal_nr_off & 0xfff) << 16;
	isp3_param_write(params_vdev, value, ISP35_BAY3D_MONROFF, id);

	if (params_vdev->dev->hw_dev->is_single && ctrl & ISP35_MODULE_EN)
		isp3_param_write(params_vdev, ctrl | ISP35_SELF_FORCE_UPD, ISP33_BAY3D_CTRL0, id);

	for (i = 0; i < ISP35_MESH_BUF_NUM; i++) {
		if (!priv->buf_b3dldc[id][i].mem_priv)
			continue;
		if (arg->lut_buf_fd == priv->buf_b3dldc[id][i].dma_fd)
			break;
	}
	if (i == ISP35_MESH_BUF_NUM) {
		if (arg->btnr_ldc_en)
			dev_err(dev->dev, "cannot find b3dldc buf fd(%d)\n", arg->lut_buf_fd);
		return;
	}
	if (!priv->buf_b3dldc[id][i].vaddr) {
		dev_err(dev->dev, "no b3dldc buffer allocated\n");
		return;
	}
	buf_idx = priv->buf_b3dldc_idx[id];
	head = (struct isp2x_mesh_head *)priv->buf_b3dldc[id][buf_idx].vaddr;
	head->stat = MESH_BUF_INIT;
	buf_idx = i;
	head = (struct isp2x_mesh_head *)priv->buf_b3dldc[id][buf_idx].vaddr;
	head->stat = MESH_BUF_CHIPINUSE;
	priv->buf_b3dldc_idx[id] = buf_idx;
	rkisp_prepare_buffer(dev, &priv->buf_b3dldc[id][buf_idx]);

	value = !!arg->btnr_ldcltp_mode << 16 |
		arg->btnr_ldc_wrap_ext_bound_offset;
	isp3_param_write(params_vdev, value, ISP35_B3DLDC_EXTBOUND1, id);

	ctrl = 0;
	if (arg->b3dldch_en) {
		value = priv->buf_b3dldc[id][buf_idx].dma_addr + head->data_oft;
		isp3_param_write(params_vdev, value, ISP35_B3DLDCH_RD_BASE, id);
		value = priv->b3dldc_hsize;
		isp3_param_write(params_vdev, value, ISP35_B3DLDCH_RD_HWSIZE, id);
		value = priv->b3dldch_vsize;
		isp3_param_write(params_vdev, value, ISP35_B3DLDCH_RD_VSIZE, id);
		ctrl |= !!arg->b3dldch_map13p3_en << 6 |
			!!arg->b3dldch_force_map_en << 7 |
			ISP35_B3DLDC_EN;
	}
	isp3_param_write(params_vdev, ctrl, ISP35_B3DLDC_ADR_STS, id);

	ctrl = 0;
	if (arg->btnr_ldc_en) {
		value = priv->buf_b3dldc[id][buf_idx].dma_addr + head->data1_oft;
		isp3_param_write(params_vdev, value, ISP35_B3DLDCV_RD_BASE, id);
		value = priv->b3dldc_hsize;
		isp3_param_write(params_vdev, value, ISP35_B3DLDCV_RD_HWSIZE, id);
		value = priv->b3dldcv_vsize;
		isp3_param_write(params_vdev, value, ISP35_B3DLDCV_RD_VSIZE, id);
		ctrl |= !!arg->b3dldcv_map13p3_en << 7 |
			!!arg->b3dldcv_force_map_en << 8 |
			ISP35_B3DLDC_EN;
	}
	isp3_param_write(params_vdev, ctrl, ISP35_B3DLDC_CTRL, id);
}

static void
isp_bay3d_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	u32 value, ctrl, b3dldc_ctrl;

	ctrl = isp3_param_read_cache(params_vdev, ISP33_BAY3D_CTRL0, id);
	if (en == !!(ctrl & ISP35_MODULE_EN))
		return;

	b3dldc_ctrl = isp3_param_read_cache(params_vdev, ISP35_B3DLDC_CTRL, id);
	if (en) {
		if (!priv->buf_bay3d_iir[0].mem_priv ||
		    !priv->buf_bay3d_ds[0].mem_priv ||
		    !priv->buf_bay3d_wgt[0].mem_priv) {
			dev_err(dev->dev, "no bay3d buffer available\n");
			return;
		}

		priv->bay3d_iir_idx = 0;
		priv->bay3d_iir_cur_idx = 0;
		value = priv->bay3d_iir_size;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_IIR_WR_SIZE, id);
		value = priv->buf_bay3d_iir[0].dma_addr + value * id;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_IIR_RD_BASE, id);
		if (priv->bay3d_iir_rw_fmt == 3) {
			isp3_param_write(params_vdev, value, ISP35_B3DLDC_WR_ADDR, id);
			if (b3dldc_ctrl & ISP35_B3DLDC_EN) {
				b3dldc_ctrl |= ISP35_B3DLDC_FORCE_UPD;
				isp3_param_write(params_vdev, b3dldc_ctrl, ISP35_B3DLDC_CTRL, id);
			}
			value += priv->bay3d_iir_offs;
		}
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_IIR_WR_BASE, id);
		if (priv->buf_aiisp[0].mem_priv) {
			priv->aiisp_cur_idx = 0;
			value = priv->buf_aiisp[0].dma_addr + value * id;
			isp3_param_write(params_vdev, value, ISP39_AIISP_RD_BASE, id);
		}
		value = priv->bay3d_iir_stride;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_IIR_WR_LENGTH, id);
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_IIR_RD_LENGTH, id);
		isp3_param_write(params_vdev, value, ISP3X_MI_DBR_RD_LENGTH, id);
		isp3_param_write(params_vdev, value, ISP35_B3DLDC_WR_STRIDE, id);

		priv->bay3d_ds_idx = 0;
		priv->bay3d_ds_cur_idx = 0;
		value = priv->bay3d_ds_size;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_DS_WR_SIZE, id);
		value = priv->buf_bay3d_ds[0].dma_addr + value * id;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_DS_WR_BASE, id);
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_DS_RD_BASE, id);

		priv->bay3d_wgt_idx = 0;
		priv->bay3d_wgt_cur_idx = 0;
		value = priv->bay3d_wgt_size;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_CUR_WR_SIZE, id);
		isp3_param_write(params_vdev, value, ISP32_MI_BAY3D_CUR_RD_SIZE, id);
		value = priv->buf_bay3d_wgt[0].dma_addr + value * id;
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_CUR_WR_BASE, id);
		isp3_param_write(params_vdev, value, ISP3X_MI_BAY3D_CUR_RD_BASE, id);

		if (priv->buf_gain[0].mem_priv) {
			value = priv->gain_size;
			isp3_param_write(params_vdev, value, ISP3X_MI_GAIN_WR_SIZE, id);
			if (!params_vdev->is_hdr)
				isp3_param_write(params_vdev, 0, ISP32_MI_RAW0_RD_SIZE, id);
			value = priv->buf_gain[0].dma_addr + value * id;
			isp3_param_write(params_vdev, value, ISP3X_MI_GAIN_WR_BASE, id);
			if (!params_vdev->is_hdr)
				isp3_param_write(params_vdev, value, ISP3X_MI_RAW0_RD_BASE, id);
			else
				isp3_param_write(params_vdev, value, ISP35_B3DLDCH_RD_BASE, id);
			priv->gain_cur_idx = 0;
		}

		ctrl |= ISP35_MODULE_EN;
		isp3_param_write(params_vdev, ctrl, ISP33_BAY3D_CTRL0, id);

		value = ISP3X_BAY3D_IIR_WR_AUTO_UPD | ISP3X_BAY3D_CUR_WR_AUTO_UPD |
			ISP3X_BAY3D_DS_WR_AUTO_UPD | ISP3X_BAY3D_IIRSELF_UPD |
			ISP3X_BAY3D_CURSELF_UPD | ISP3X_BAY3D_DSSELF_UPD |
			ISP3X_BAY3D_RDSELF_UPD;
		if (priv->buf_gain[0].mem_priv)
			value |= ISP3X_GAIN_WR_AUTO_UPD | ISP3X_GAINSELF_UPD;
		isp3_param_set_bits(params_vdev, MI_WR_CTRL2, value, id);

		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1, ISP3X_RAW3D_FST_FRAME, id);
	} else {
		ctrl &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
		isp3_param_write(params_vdev, ctrl, ISP33_BAY3D_CTRL0, id);
		if (b3dldc_ctrl & ISP35_B3DLDC_EN) {
			b3dldc_ctrl &= ~(ISP35_B3DLDC_FORCE_UPD | ISP35_B3DLDC_EN);
			isp3_param_write(params_vdev, b3dldc_ctrl, ISP35_B3DLDC_CTRL, id);

			isp3_param_clear_bits(params_vdev, ISP35_B3DLDC_ADR_STS, ISP35_B3DLDC_EN, id);
		}
	}
}

static void
isp_gain_config(struct rkisp_isp_params_vdev *params_vdev,
		const struct isp3x_gain_cfg *arg, u32 id)
{
	u32 val;

	val = arg->g0;
	isp3_param_write(params_vdev, val, ISP3X_GAIN_G0, id);
	val = ISP_PACK_2SHORT(arg->g1, arg->g2);
	isp3_param_write(params_vdev, val, ISP3X_GAIN_G1_G2, id);
}

static void
isp_gain_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read_cache(params_vdev, ISP3X_GAIN_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_GAIN_CTRL, id);
}

static void
isp_cac_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp33_cac_cfg *arg, u32 id)
{
	struct isp33_cac_cfg *arg_rec = &params_vdev->isp35_params->others.cac_cfg;
	u32 i, val, ctrl;

	ctrl = isp3_param_read(params_vdev, ISP3X_CAC_CTRL, id);
	ctrl &= ISP35_MODULE_EN;
	ctrl |= !!arg->bypass_en << 1 |
		!!arg->edge_detect_en << 2 |
		!!arg->neg_clip0_en << 3 |
		!!arg->wgt_color_en << 5;
	isp3_param_write(params_vdev, ctrl, ISP3X_CAC_CTRL, id);

	val = arg->psf_table_fix_bit;
	isp3_param_write(params_vdev, val, ISP3X_CAC_PSF_PARA, id);

	val = arg->hi_drct_ratio;
	isp3_param_write(params_vdev, val, ISP33_CAC_HIGH_DIRECT, id);

	val = arg->over_expo_thred;
	isp3_param_write(params_vdev, val, ISP33_CAC_OVER_EXPO0, id);

	val = arg->over_expo_adj;
	isp3_param_write(params_vdev, val, ISP33_CAC_OVER_EXPO1, id);

	val = arg->flat_thred | arg->flat_offset << 16;
	isp3_param_write(params_vdev, val, ISP33_CAC_FLAT, id);

	val = (arg->chroma_lo_flt_coeff0 & 0x7) |
	      (arg->chroma_lo_flt_coeff1 & 0x7) << 4 |
	      (arg->color_lo_flt_coeff0 & 0x7) << 8 |
	      (arg->color_lo_flt_coeff1 & 0x7) << 12;
	isp3_param_write(params_vdev, val, ISP33_CAC_GAUSS_COEFF, id);

	val = ISP_PACK_2SHORT(arg->search_range_ratio, arg->residual_chroma_ratio);
	isp3_param_write(params_vdev, val, ISP33_CAC_RATIO, id);

	val = arg->wgt_color_b_min_thred;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_COLOR_B, id);
	val = arg->wgt_color_r_min_thred;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_COLOR_R, id);

	val = arg->wgt_color_b_slope;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_COLOR_SLOPE_B, id);
	val = arg->wgt_color_r_slope;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_COLOR_SLOPE_R, id);

	val = arg->wgt_color_min_luma;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_COLOR_LUMA0, id);
	val = arg->wgt_color_luma_slope;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_COLOR_LUMA1, id);

	val = arg->wgt_over_expo_min_thred;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_OVER_EXPO0, id);
	val = arg->wgt_over_expo_slope;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_OVER_EXPO1, id);

	val = arg->wgt_contrast_min_thred;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_CONTRAST0, id);
	val = arg->wgt_contrast_slope;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_CONTRAST1, id);
	val = arg->wgt_contrast_offset;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_CONTRAST2, id);

	val = arg->wgt_dark_thed;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_DARK_AREA0, id);
	val = arg->wgt_dark_slope;
	isp3_param_write(params_vdev, val, ISP33_CAC_WGT_DARK_AREA1, id);

	for (i = 0; i < ISP35_CAC_PSF_NUM / 4; i++) {
		val = ISP_PACK_4BYTE(arg->psf_b_ker[i * 4], arg->psf_b_ker[i * 4 + 1],
				     arg->psf_b_ker[i * 4 + 2], arg->psf_b_ker[i * 4 + 3]);
		isp3_param_write(params_vdev, val, ISP33_CAC_PSF_B0 + i * 4, id);
		val = ISP_PACK_4BYTE(arg->psf_r_ker[i * 4], arg->psf_r_ker[i * 4 + 1],
				     arg->psf_r_ker[i * 4 + 2], arg->psf_r_ker[i * 4 + 3]);
		isp3_param_write(params_vdev, val, ISP33_CAC_PSF_R0 + i * 4, id);
	}
	val = ISP_PACK_4BYTE(arg->psf_b_ker[i * 4], arg->psf_b_ker[i * 4 + 1],
			     arg->psf_b_ker[i * 4 + 2], 0);
	isp3_param_write(params_vdev, val, ISP33_CAC_PSF_B2, id);
	val = ISP_PACK_4BYTE(arg->psf_r_ker[i * 4], arg->psf_r_ker[i * 4 + 1],
			     arg->psf_r_ker[i * 4 + 2], 0);
	isp3_param_write(params_vdev, val, ISP33_CAC_PSF_R2, id);

	memcpy(arg_rec->psf_b_ker, arg->psf_b_ker, sizeof(arg_rec->psf_b_ker));
	memcpy(arg_rec->psf_r_ker, arg->psf_r_ker, sizeof(arg_rec->psf_r_ker));
}

static void
isp_cac_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_CAC_CTRL, id);

	if (en == !!(val & ISP35_MODULE_EN))
		return;
	if (en)
		val |= ISP35_MODULE_EN;
	else
		val &= ~(ISP35_MODULE_EN | ISP35_SELF_FORCE_UPD);
	isp3_param_write(params_vdev, val, ISP3X_CAC_CTRL, id);
}

static void
isp_csm_config(struct rkisp_isp_params_vdev *params_vdev,
	       const struct isp21_csm_cfg *arg, u32 id)
{
	u32 i, val;

	for (i = 0; i < ISP35_CSM_COEFF_NUM; i++) {
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
isp_rgbir_config(struct rkisp_isp_params_vdev *params_vdev,
		 const struct isp39_rgbir_cfg *arg, u32 id)
{
	u32 i, value;

	value = arg->coe_theta & 0xfff;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_THETA, id);

	value = arg->coe_delta & 0x3fff;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_DELTA, id);

	for (i = 0; i < ISP35_RGBIR_SCALE_NUM; i++) {
		value = arg->scale[i] & 0x1ff;
		isp3_param_write(params_vdev, value, ISP39_RGBIR_SCALE0 + i * 4, id);
	}

	for (i = 0; i < ISP35_RGBIR_LUMA_POINT_NUM / 3; i++) {
		value = (arg->luma_point[i * 3] & 0x3ff) |
			(arg->luma_point[i * 3 + 1] & 0x3ff) << 10 |
			(arg->luma_point[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, value, ISP39_RGBIR_LUMA_POINT0 + i * 4, id);
	}
	value = (arg->luma_point[i * 3] & 0x3ff) | (arg->luma_point[i * 3 + 1] & 0x7ff) << 10;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_LUMA_POINT0 + i * 4, id);

	for (i = 0; i < ISP35_RGBIR_SCALE_MAP_NUM / 3; i++) {
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
		value = ISP35_MODULE_EN;
	isp3_param_write(params_vdev, value, ISP39_RGBIR_CTRL, id);
}

static void vpsl_update_buf(struct rkisp_isp_params_vdev *params_vdev,
			    struct rkisp_dummy_buffer *pbuf, bool is_force)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	u32 i, val, ds_cnt;

	if (!pbuf)
		return;
	priv->vpsl_idx = pbuf->index;
	ds_cnt = priv->yraw_sel ? VPSL_YRAW_CHN_MAX / 2 : VPSL_YRAW_CHN_MAX;
	for (i = 0; i < ds_cnt; i++) {
		val = pbuf->dma_addr + priv->vpsl_yraw_offs[i];
		vpsl_write(dev, VPSL_MI_CHN0_WR_BASE + i * 0x100, val, false);
		val = priv->vpsl_yraw_stride[i];
		vpsl_write(dev, VPSL_MI_CHN0_WR_STRIDE + i * 0x100, val, false);
		vpsl_write(dev, VPSL_MI_CHN0_WR_CTRL + i * 0x100, VPSL_CHN_WR_AUTO_UPD, false);
	}
	ds_cnt = priv->yraw_sel ? VPSL_SIG_CHN_MAX - 1 : VPSL_SIG_CHN_MAX;
	for (i = 0; i < ds_cnt; i++) {
		val = pbuf->dma_addr + priv->vpsl_sig_offs[i];
		vpsl_write(dev, VPSL_MI_CHN6_WR_BASE + i * 0x100, val, false);
		val = priv->vpsl_sig_stride[i];
		vpsl_write(dev, VPSL_MI_CHN6_WR_STRIDE + i * 0x100, val, false);
		vpsl_write(dev, VPSL_MI_CHN6_WR_CTRL + i * 0x100, VPSL_CHN_WR_AUTO_UPD, false);
	}

	vpsl_write(dev, VPSL_MI_IMSC, 0xffffffff, false);
	val = VPSL_MI_WR_ID_POLL_DIS | VPSL_MI_WR_INIT_OFFSET_EN | VPSL_MI_WR_INIT_BASE_EN;
	vpsl_write(dev, VPSL_MI_CTRL, val, false);
	if (dev->hw_dev->is_single && is_force)
		vpsl_write(dev, VPSL_MI_WR_INIT, 0x7ff0, true);
}

static void vpsl_cfg_sram(struct rkisp_isp_params_vdev *params_vdev,
			  const struct isp35_ai_cfg *arg)
{
	struct rkisp_device *dev = params_vdev->dev;
	u32 i, val;

	if (!arg->pyr_sigma_en)
		return;
	for (i = 0; i < ISP35_VPSL_SIGMA_NUM; i++) {
		val = arg->pyr_sigma_y[i];
		vpsl_write(dev, VPSL_PYR_SIGMA_LUT, val, true);
	}
}

static void vpsl_config(struct rkisp_isp_params_vdev *params_vdev,
			const struct isp35_ai_cfg *arg, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	u32 val;

	if (!arg->pyr_yraw_mode && !arg->pyr_sigma_en)
		return;
	if (!priv->buf_vpsl[0].mem_priv) {
		dev_err(dev->dev, "no vpsl buffer available\n");
		return;
	}

	val = VPSL_CHN0_EN | VPSL_CHN1_EN | VPSL_CHN2_EN |
	      VPSL_CHN6_EN | VPSL_CHN7_EN | VPSL_CHN8_EN | VPSL_CHN9_EN;
	if (!priv->yraw_sel)
		val |= VPSL_CHN3_EN | VPSL_CHN4_EN | VPSL_CHN5_EN | VPSL_CHN10_EN;
	vpsl_write(dev, VPSL_PYR_CHN, val, false);

	val = (arg->pyr_yraw_mode & 0x3) |
	      !!arg->pyr_sigma_en << 2 |
	      !!arg->pyr_yraw_sel << 4 |
	      (arg->pyr_gain_leftshift & 0x7) << 8 |
	      arg->pyr_blacklvl_sig << 16;
	vpsl_write(dev, VPSL_PYR_CTRL, val, false);

	//vpsl_write(dev, VPSL_IMSC, 0xffffffff, false);
	if (dev->hw_dev->is_single) {
		vpsl_cfg_sram(params_vdev, arg);
		val = VPSL_CFG_GEN_UPD | VPSL_YRAW_CHN_FORCE_UPD | VPSL_SIGMA_CHN_FORCE_UPD;
		vpsl_write(dev, VPSL_UPDATE, val, true);
	}
}

static void
isp_ai_config(struct rkisp_isp_params_vdev *params_vdev,
	      const struct isp35_ai_cfg *arg, u32 id)
{
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params + id;
	struct isp35_ai_cfg *arg_rec = &params_rec->others.ai_cfg;
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP35_AI_CTRL, id);
	val &= (ISP35_AIISP_EN | ISP35_AIPRE_IIR2DDR_EN | ISP35_AIPRE_GIAN2DDR_EN |
		ISP35_AIPRE_IIR_EN | ISP35_AIPRE_GAIN_EN);
	val |= //!!arg->aiisp_raw12_msb << 2 |
	       (arg->aiisp_gain_mode & 0x3) << 4 |
	       !!arg->aiisp_curve_en << 6 |
	       //!!arg->aipre_iir_en << 8 |
	       //!!arg->aipre_iir2ddr_en << 9 |
	       //!!arg->aipre_gain_en << 10 |
	       //!!arg->aipre_gain2ddr_en << 11 |
	       !!arg->aipre_yraw_sel << 12 |
	       !!arg->aipre_nl_ddr_mode << 13 |
	       !!arg->aipre_gain_bypass << 14 |
	       !!arg->aipre_gain_mode << 15 |
	       !!arg->aipre_narmap_inv << 16 |
	       !!arg->aipre_luma2gain_dis << 17;
	if (params_vdev->is_hdr)
		val |= ISP35_AIISP_HDR_EN;
	if (priv->bay3d_iir_rw_fmt == 2 && params_vdev->dev->is_aiisp_en)
		val |= ISP35_AIISP_RAW12_MSB;
	isp3_param_write(params_vdev, val, ISP35_AI_CTRL, id);
	for (i = 0; i < ISP35_AI_SIGMA_NUM / 2; i++) {
		val = ISP_PACK_2SHORT(arg->aiisp_sigma_y[i * 2], arg->aiisp_sigma_y[i * 2 + 1]);
		isp3_param_write(params_vdev, val, ISP35_AI_SIGMA_Y0 + i * 4, id);
	}
	val = arg->aiisp_sigma_y[ISP35_AI_SIGMA_NUM - 1];
	isp3_param_write(params_vdev, val, ISP35_AI_SIGMA_Y16, id);

	val = arg->aipre_scale | (arg->aipre_zp & 0xff) << 8 |
	      (arg->aipre_black_lvl & 0x1ff) << 20;
	isp3_param_write(params_vdev, val, ISP35_AI_PRE_NL_PRE, id);

	val = (arg->aipre_gain_alpha & 0xf) | arg->aipre_global_gain << 4 |
	      arg->aipre_gain_ratio << 12;
	isp3_param_write(params_vdev, val, ISP35_AI_PRE_GAIN_PARA, id);

	for (i = 0; i < ISP35_AI_SIGMA_NUM / 3; i++) {
		val = (arg->aipre_sigma_y[i * 3] & 0x3ff) |
		      (arg->aipre_sigma_y[i * 3 + 1] & 0x3ff) << 10 |
		      (arg->aipre_sigma_y[i * 3 + 2] & 0x3ff) << 20;
		isp3_param_write(params_vdev, val, ISP35_AI_PRE_SIGMA_CURVE0 + i * 4, id);
	}

	val = arg->aipre_noise_mot_offset | (arg->aipre_noise_mot_gain & 0x7f) << 8 |
	      (arg->aipre_noise_luma_offset & 0x3ff) << 16;
	isp3_param_write(params_vdev, val, ISP35_AI_PRE_NOISE0, id);

	val = (arg->aipre_noise_luma_gain & 0x7ff) |
	      (arg->aipre_noise_luma_clip & 0x3ff) << 12 |
	      arg->aipre_noise_luma_static << 24;
	isp3_param_write(params_vdev, val, ISP35_AI_PRE_NOISE1, id);

	val = arg->aipre_nar_manual |
	      (arg->aipre_nar_manual_alpha & 0x3f) << 8;
	isp3_param_write(params_vdev, val, ISP35_AI_PRE_NOISE2, id);

	vpsl_config(params_vdev, arg, id);
	*arg_rec = *arg;
}

static void
isp_ai_enable(struct rkisp_isp_params_vdev *params_vdev, bool en, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	u32 val, ctrl = isp3_param_read(params_vdev, ISP35_AI_CTRL, id);

	if (en == !!(ctrl & ISP35_MODULE_EN))
		return;
	ctrl &= ~(ISP35_AIISP_ST | ISP35_AIPRE_IIR2DDR_EN | ISP35_AIPRE_GIAN2DDR_EN |
		  ISP35_AIPRE_IIR_EN | ISP35_AIPRE_GAIN_EN);
	if (en) {
		if (priv->buf_aipre_gain[0].mem_priv) {
			priv->aipre_gain_idx = 0;
			priv->aipre_gain_cur_idx = 0;
			val = priv->buf_aipre_gain[0].dma_addr;
			isp3_param_write(params_vdev, val, ISP35_AI_PRE_GAIN_WR_BASE, id);
			val = priv->aipre_gain_stride;
			isp3_param_write(params_vdev, val, ISP35_AI_PRE_GAIN_WR_STRIDE, id);
			ctrl |= ISP35_AIPRE_GIAN2DDR_EN | ISP35_AIPRE_GAIN_EN;
		}
		if (priv->buf_vpsl[0].mem_priv) {
			if (priv->yraw_sel)
				ctrl |= ISP35_AIPRE_IIR_EN;
			vpsl_update_buf(params_vdev, priv->pbuf_vpsl, true);
			priv->vpsl_cur_idx = priv->vpsl_idx;
			if (params_vdev->dev->is_aiisp_sync)
				params_vdev->dev->irq_ends_mask |= ISP_FRAME_VPSL;
		}
		ctrl |= ISP35_AIPRE_ITS_FORCE_UPD;
		if (params_vdev->dev->is_aiisp_en)
			ctrl |= ISP35_AIISP_EN;
		else if (params_vdev->dev->is_aiisp_stop)
			ctrl &= ~(ISP35_AIPRE_GIAN2DDR_EN | ISP35_AIPRE_GAIN_EN | ISP35_AIPRE_IIR_EN);
	} else {
		ctrl &= ~ISP35_AIISP_EN;
		params_vdev->dev->irq_ends_mask &= ~ISP_FRAME_VPSL;
	}
	isp3_param_write(params_vdev, ctrl, ISP35_AI_CTRL, id);
	if (en) {
		ctrl &= ~ISP35_AIPRE_ITS_FORCE_UPD;
		isp3_param_write(params_vdev, ctrl, ISP35_AI_CTRL, id);
	}
}

static __maybe_unused
void __isp_isr_other_config(struct rkisp_isp_params_vdev *params_vdev,
			    const struct isp35_isp_params_cfg *new_params,
			    enum rkisp_params_type type, u32 id)
{
	struct rkisp_device *dev = params_vdev->dev;
	u64 module_cfg_update = new_params->module_cfg_update;

	v4l2_dbg(4, rkisp_debug, &dev->v4l2_dev,
		 "%s id:%d seq:%d type:%d module_cfg_update:0x%llx\n",
		 __func__, id, new_params->frame_id, type, module_cfg_update);

	if (module_cfg_update & ISP35_MODULE_RGBIR && type != RKISP_PARAMS_LAT)
		isp_rgbir_config(params_vdev, &new_params->others.rgbir_cfg, id);
	if (module_cfg_update & ISP35_MODULE_BLS)//bls0 ob TNR blc1, blc2 for awb
		isp_bls_config(params_vdev, &new_params->others.bls_cfg, type, id);
	if (module_cfg_update & ISP35_MODULE_AWB_GAIN)//awb0 TNR awb1
		isp_awbgain_config(params_vdev, &new_params->others.awb_gain_cfg, type, id);
	if (module_cfg_update & ISP35_MODULE_DPCC && type != RKISP_PARAMS_LAT)
		isp_dpcc_config(params_vdev, &new_params->others.dpcc_cfg, id);
	if (module_cfg_update & ISP35_MODULE_HDRMGE && type != RKISP_PARAMS_LAT)
		isp_hdrmge_config(params_vdev, &new_params->others.hdrmge_cfg, type, id);
	if (module_cfg_update & ISP35_MODULE_GAIN && type != RKISP_PARAMS_LAT)
		isp_gain_config(params_vdev, &new_params->others.gain_cfg, id);
	if (module_cfg_update & ISP35_MODULE_AI && type != RKISP_PARAMS_LAT)
		isp_ai_config(params_vdev, &new_params->others.ai_cfg, id);
	if (module_cfg_update & ISP35_MODULE_BAY3D && type != RKISP_PARAMS_LAT)
		isp_bay3d_config(params_vdev, &new_params->others.bay3d_cfg, id);

	if (type == RKISP_PARAMS_IMD && dev->is_aiisp_en)
		return;

	if (module_cfg_update & ISP35_MODULE_CAC)
		isp_cac_config(params_vdev, &new_params->others.cac_cfg, id);
	if (module_cfg_update & ISP35_MODULE_LSC)
		isp_lsc_config(params_vdev, &new_params->others.lsc_cfg, id);
	if (module_cfg_update & ISP35_MODULE_DEBAYER)
		isp_debayer_config(params_vdev, &new_params->others.debayer_cfg, id);
	if (module_cfg_update & ISP35_MODULE_DRC)
		isp_hdrdrc_config(params_vdev, &new_params->others.drc_cfg, type, id);
	if (module_cfg_update & ISP35_MODULE_CCM)
		isp_ccm_config(params_vdev, &new_params->others.ccm_cfg, id);
	if (module_cfg_update & ISP35_MODULE_GOC)
		isp_goc_config(params_vdev, &new_params->others.gammaout_cfg, id);
	if (module_cfg_update & ISP35_MODULE_HSV)
		isp_hsv_config(params_vdev, &new_params->others.hsv_cfg, id);
	/* range csm->cgc->cproc->ie */
	if (module_cfg_update & ISP35_MODULE_CSM)
		isp_csm_config(params_vdev, &new_params->others.csm_cfg, id);
	if (module_cfg_update & ISP35_MODULE_GIC)
		isp_gic_config(params_vdev, &new_params->others.gic_cfg, id);
	if (module_cfg_update & ISP35_MODULE_CNR)
		isp_cnr_config(params_vdev, &new_params->others.cnr_cfg, id);
	if (module_cfg_update & ISP35_MODULE_YNR)
		isp_ynr_config(params_vdev, &new_params->others.ynr_cfg, id);
	if (module_cfg_update & ISP35_MODULE_SHARP)
		isp_sharp_config(params_vdev, &new_params->others.sharp_cfg, id);
	if (module_cfg_update & ISP35_MODULE_ENH)
		isp_enh_config(params_vdev, &new_params->others.enh_cfg, id);
	if (module_cfg_update & ISP35_MODULE_HIST)
		isp_hist_config(params_vdev, &new_params->others.hist_cfg, id);
	if (module_cfg_update & ISP35_MODULE_LDCH)
		isp_ldch_config(params_vdev, &new_params->others.ldch_cfg, id);
	if (module_cfg_update & ISP35_MODULE_CGC)
		isp_cgc_config(params_vdev, &new_params->others.cgc_cfg, id);
	if (module_cfg_update & ISP35_MODULE_CPROC)
		isp_cproc_config(params_vdev, &new_params->others.cproc_cfg, id);
}

static __maybe_unused
void __isp_isr_other_en(struct rkisp_isp_params_vdev *params_vdev,
			const struct isp35_isp_params_cfg *new_params,
			enum rkisp_params_type type, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	u64 module_en_update = new_params->module_en_update;
	u64 module_ens = new_params->module_ens;
	u64 mask;
	u32 gain_ctrl, cnr_ctrl, val;

	mask = ISP35_MODULE_YNR | ISP35_MODULE_CNR | ISP35_MODULE_SHARP;
	if ((module_ens & mask) && ((module_ens & mask) != mask))
		dev_err(dev->dev, "ynr cnr sharp no enable together\n");
	v4l2_dbg(4, rkisp_debug, &dev->v4l2_dev,
		 "%s id:%d seq:%d type:%d module_en_update:0x%llx module_ens:0x%llx\n",
		 __func__, id, new_params->frame_id, type, module_en_update, module_ens);

	if (module_en_update & ISP35_MODULE_RGBIR && type != RKISP_PARAMS_LAT)
		isp_rgbir_enable(params_vdev, !!(module_ens & ISP35_MODULE_RGBIR), id);
	if (module_en_update & ISP35_MODULE_BLS)
		isp_bls_enable(params_vdev, !!(module_ens & ISP35_MODULE_BLS), id);
	if (module_en_update & ISP35_MODULE_AWB_GAIN)
		isp_awbgain_enable(params_vdev, !!(module_ens & ISP35_MODULE_AWB_GAIN), id);
	if (module_en_update & ISP35_MODULE_DPCC && type != RKISP_PARAMS_LAT)
		isp_dpcc_enable(params_vdev, !!(module_ens & ISP35_MODULE_DPCC), id);
	if ((module_en_update & ISP35_MODULE_GAIN && type != RKISP_PARAMS_LAT) ||
	    ((priv->buf_info_owner == RKISP_INFO2DRR_OWNER_GAIN) &&
	     !(isp3_param_read(params_vdev, ISP3X_GAIN_CTRL, id) & ISP3X_GAIN_2DDR_EN)))
		isp_gain_enable(params_vdev, !!(module_ens & ISP35_MODULE_GAIN), id);
	if (module_en_update & ISP35_MODULE_AI && type != RKISP_PARAMS_LAT)
		isp_ai_enable(params_vdev, !!(module_ens & ISP35_MODULE_AI), id);
	if (module_en_update & ISP35_MODULE_BAY3D && type != RKISP_PARAMS_LAT)
		isp_bay3d_enable(params_vdev, !!(module_ens & ISP35_MODULE_BAY3D), id);

	if (type == RKISP_PARAMS_IMD && dev->is_aiisp_en)
		return;

	if (module_en_update & ISP35_MODULE_CAC)
		isp_cac_enable(params_vdev, !!(module_ens & ISP35_MODULE_CAC), id);
	if (module_en_update & ISP35_MODULE_LSC)
		isp_lsc_enable(params_vdev, !!(module_ens & ISP35_MODULE_LSC), id);
	if (module_en_update & ISP35_MODULE_DEBAYER)
		isp_debayer_enable(params_vdev, !!(module_ens & ISP35_MODULE_DEBAYER), id);
	if (module_en_update & ISP35_MODULE_DRC)
		isp_hdrdrc_enable(params_vdev, !!(module_ens & ISP35_MODULE_DRC), id);
	if (module_en_update & ISP35_MODULE_CCM)
		isp_ccm_enable(params_vdev, !!(module_ens & ISP35_MODULE_CCM), id);
	if (module_en_update & ISP35_MODULE_GOC)
		isp_goc_enable(params_vdev, !!(module_ens & ISP35_MODULE_GOC), id);
	if (module_en_update & ISP35_MODULE_HSV)
		isp_hsv_enable(params_vdev, !!(module_ens & ISP35_MODULE_HSV), id);
	if (module_en_update & ISP35_MODULE_GIC)
		isp_gic_enable(params_vdev, !!(module_ens & ISP35_MODULE_GIC), id);
	if (module_en_update & ISP35_MODULE_CNR)
		isp_cnr_enable(params_vdev, !!(module_ens & ISP35_MODULE_CNR), id);
	if (module_en_update & ISP35_MODULE_YNR)
		isp_ynr_enable(params_vdev, !!(module_ens & ISP35_MODULE_YNR), id);
	if (module_en_update & ISP35_MODULE_SHARP)
		isp_sharp_enable(params_vdev, !!(module_ens & ISP35_MODULE_SHARP), id);
	if (module_en_update & ISP35_MODULE_ENH)
		isp_enh_enable(params_vdev, !!(module_ens & ISP35_MODULE_ENH), id);
	if (module_en_update & ISP35_MODULE_HIST)
		isp_hist_enable(params_vdev, !!(module_ens & ISP35_MODULE_HIST), id);
	if (module_en_update & ISP35_MODULE_LDCH)
		isp_ldch_enable(params_vdev, !!(module_ens & ISP35_MODULE_LDCH), id);
	if (module_en_update & ISP35_MODULE_CPROC)
		isp_cproc_enable(params_vdev, !!(module_ens & ISP35_MODULE_CPROC), id);
	if (module_en_update & ISP35_MODULE_IE)
		isp_ie_enable(params_vdev, !!(module_ens & ISP35_MODULE_IE), id);
	/* gain disable, using global gain for cnr */
	gain_ctrl = isp3_param_read_cache(params_vdev, ISP3X_GAIN_CTRL, id);
	cnr_ctrl = isp3_param_read_cache(params_vdev, ISP3X_CNR_CTRL, id);
	if (!(gain_ctrl & ISP35_MODULE_EN) && cnr_ctrl & ISP35_MODULE_EN) {
		cnr_ctrl |= BIT(1);
		isp3_param_write(params_vdev, cnr_ctrl, ISP3X_CNR_CTRL, id);
		val = isp3_param_read(params_vdev, ISP3X_CNR_EXGAIN, id) & 0x3ff;
		isp3_param_write(params_vdev, val | 0x8000, ISP3X_CNR_EXGAIN, id);
	}
}

static __maybe_unused
void __isp_isr_meas_config(struct rkisp_isp_params_vdev *params_vdev,
			   struct isp35_isp_params_cfg *new_params,
			   enum rkisp_params_type type, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	u64 module_cfg_update = new_params->module_cfg_update;
	bool is_ae0_cfg = !!(module_cfg_update & ISP35_MODULE_RAWAE0);
	bool is_hist0_cfg = !!(module_cfg_update & ISP35_MODULE_RAWHIST0);
	bool is_ae3_cfg = !!(module_cfg_update & ISP35_MODULE_RAWAE3);
	bool is_hist3_cfg = !!(module_cfg_update & ISP35_MODULE_RAWHIST3);
	bool is_af_cfg = !!(module_cfg_update & ISP35_MODULE_RAWAF);
	bool is_awb_cfg = !!(module_cfg_update & ISP35_MODULE_RAWAWB);
	bool is_aiawb_cfg = !!(module_cfg_update & ISP35_MODULE_AIAWB);

	v4l2_dbg(4, rkisp_debug, &dev->v4l2_dev,
		 "%s id:%d seq:%d type:%d module_cfg_update:0x%llx\n",
		 __func__, id, new_params->frame_id, type, module_cfg_update);
	if (dev->is_aiisp_en && type != RKISP_PARAMS_ALL) {
		if ((priv->is_ae0_fe && type == RKISP_PARAMS_LAT) ||
		    (!priv->is_ae0_fe && type == RKISP_PARAMS_IMD)) {
			is_ae0_cfg = false;
			is_hist0_cfg = false;
		}
		if ((priv->is_ae3_fe && type == RKISP_PARAMS_LAT) ||
		    (!priv->is_ae3_fe && type == RKISP_PARAMS_IMD)) {
			is_ae3_cfg = false;
			is_hist3_cfg = false;
		}
		if ((priv->is_af_fe && type == RKISP_PARAMS_LAT) ||
		    (!priv->is_af_fe && type == RKISP_PARAMS_IMD))
			is_af_cfg = false;
		if ((priv->is_awb_fe && type == RKISP_PARAMS_LAT) ||
		    (!priv->is_awb_fe && type == RKISP_PARAMS_IMD))
			is_awb_cfg = false;
		if ((priv->is_aiawb_fe && type == RKISP_PARAMS_LAT) ||
		    (!priv->is_aiawb_fe && type == RKISP_PARAMS_IMD))
			is_aiawb_cfg = false;
	}

	if (is_ae0_cfg)
		isp_rawae0_config(params_vdev, &new_params->meas.rawae0, id);
	if (is_hist0_cfg)
		isp_rawhist0_config(params_vdev, &new_params->meas.rawhist0, id);
	if (is_ae3_cfg)
		isp_rawae3_config(params_vdev, &new_params->meas.rawae3, id);
	if (is_hist3_cfg)
		isp_rawhist3_config(params_vdev, &new_params->meas.rawhist3, id);
	if (is_af_cfg)
		isp_rawaf_config(params_vdev, &new_params->meas.rawaf, id);
	if (is_awb_cfg)
		isp_rawawb_config(params_vdev, &new_params->meas.rawawb, id);
	if (is_aiawb_cfg)
		isp_aiawb_config(params_vdev, &new_params->meas.aiawb, id);

	if (dev->is_aiisp_en && type == RKISP_PARAMS_IMD) {
		params_vdev->cur_fe_frame_id = new_params->frame_id;
		return;
	}

	params_vdev->cur_frame_id = new_params->frame_id;
	params_vdev->exposure = new_params->exposure;

	if (module_cfg_update & ISP35_MODULE_AWBSYNC)
		isp_awbsync_config(params_vdev, &new_params->meas.awbsync, id);
}

static __maybe_unused
void __isp_isr_meas_en(struct rkisp_isp_params_vdev *params_vdev,
		       struct isp35_isp_params_cfg *new_params,
		       enum rkisp_params_type type, u32 id)
{
	u64 module_en_update = new_params->module_en_update;
	u64 module_ens = new_params->module_ens;

	v4l2_dbg(4, rkisp_debug, &params_vdev->dev->v4l2_dev,
		 "%s id:%d seq:%d type:%d module_en_update:0x%llx module_ens:0x%llx\n",
		 __func__, id, new_params->frame_id, type, module_en_update, module_ens);

	if (module_en_update & ISP35_MODULE_RAWAE0)
		isp_rawae0_enable(params_vdev, !!(module_ens & ISP35_MODULE_RAWAE0), id);
	if (module_en_update & ISP35_MODULE_RAWHIST0)
		isp_rawhist0_enable(params_vdev, !!(module_ens & ISP35_MODULE_RAWHIST0), id);
	if (module_en_update & ISP35_MODULE_RAWAE3)
		isp_rawae3_enable(params_vdev, !!(module_ens & ISP35_MODULE_RAWAE3), id);
	if (module_en_update & ISP35_MODULE_RAWHIST3)
		isp_rawhist3_enable(params_vdev, !!(module_ens & ISP35_MODULE_RAWHIST3), id);
	if (module_en_update & ISP35_MODULE_AIAWB)
		isp_aiawb_enable(params_vdev, !!(module_ens & ISP35_MODULE_AIAWB), id);
	if (module_en_update & ISP35_MODULE_AWBSYNC)
		isp_awbsync_enable(params_vdev, !!(module_ens & ISP35_MODULE_AWBSYNC), id);
	if (module_en_update & ISP35_MODULE_RAWAWB)
		isp_rawawb_enable(params_vdev, !!(module_ens & ISP35_MODULE_RAWAWB), id);
	if (module_en_update & ISP35_MODULE_RAWAF)
		isp_rawaf_enable(params_vdev, !!(module_ens & ISP35_MODULE_RAWAF), id);
}

static
void rkisp_params_cfgsram_v35(struct rkisp_isp_params_vdev *params_vdev, bool is_reset)
{
	u32 id = params_vdev->dev->unite_index;
	struct isp35_isp_params_cfg *params = params_vdev->isp35_params + id;

	if (is_reset) {
		isp3_param_set_bits(params_vdev, ISP3X_ISP_CTRL1,
				    ISP33_GIC_FST_FRAME | ISP32_SHP_FST_FRAME, id);
		isp_sharp_cfg_noise_curve(params_vdev, &params->others.sharp_cfg, id, true);
		isp_gic_cfg_noise_curve(params_vdev, &params->others.gic_cfg, id, true);
		params->others.enh_cfg.iir_wr = true;
		params->others.hist_cfg.iir_wr = true;
	}
	isp_enh_cfg_sram(params_vdev, &params->others.enh_cfg, true, id);
	isp_hist_cfg_sram(params_vdev, &params->others.hist_cfg, true, id);
	params->others.enh_cfg.iir_wr = false;
	params->others.hist_cfg.iir_wr = false;

	isp_lsc_matrix_cfg_sram(params_vdev, &params->others.lsc_cfg, true, id);
	isp_hsv_cfg_sram(params_vdev, &params->others.hsv_cfg, true, id);
	isp_rawawb_cfg_sram(params_vdev, &params->meas.rawawb, true, id);
	isp_rawhist_cfg_sram(params_vdev, &params->meas.rawhist0,
			     ISP3X_RAWHIST_LITE_BASE, true, id);
	isp_rawhist_cfg_sram(params_vdev, &params->meas.rawhist3,
			     ISP3X_RAWHIST_BIG1_BASE, true, id);
}

static bool
rkisp_params_check_bigmode_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_device *dev = params_vdev->dev;

	dev->multi_index = 0;
	dev->multi_mode = 0;
	if (!dev->hw_dev->is_single) {
		dev->is_frm_rd = true;
		dev->multi_index = dev->dev_id;
	}

	return dev->is_bigmode = false;
}

static void
rkisp_params_first_cfg_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	struct isp35_isp_params_cfg *params = params_vdev->isp35_params;
	struct rkisp_device *dev = params_vdev->dev;
	unsigned long flags = 0;
	int i;

	rkisp_params_check_bigmode_v35(params_vdev);
	spin_lock_irqsave(&params_vdev->config_lock, flags);
	for (i = 0; i < dev->unite_div; i++) {
		u64 module_cfg_update = params->module_cfg_update;
		u64 module_en_update = params->module_en_update;
		u64 module_ens = params->module_ens;

		if (!module_cfg_update || !module_en_update || !module_ens)
			dev_warn(dev->dev,
				 "id:%d no first iq setting cfg_upd:%llx en_upd:%llx ens:%llx\n",
				 i, module_cfg_update, module_en_update, module_ens);

		__isp_isr_meas_config(params_vdev, params + i, RKISP_PARAMS_ALL, i);
		__isp_isr_other_config(params_vdev, params + i, RKISP_PARAMS_ALL, i);
		__isp_isr_other_en(params_vdev, params + i, RKISP_PARAMS_ALL, i);
		__isp_isr_meas_en(params_vdev, params + i, RKISP_PARAMS_ALL, i);
	}
	spin_unlock_irqrestore(&params_vdev->config_lock, flags);

	if (dev->hw_dev->is_single && (dev->isp_state & ISP_START)) {
		u32 val = CIF_ISP_CTRL_ISP_CFG_UPD;

		if (dev->is_aiisp_en)
			val |= ISP35_ISP_CFG_UPD_FE;
		rkisp_set_bits(dev, ISP3X_ISP_CTRL0, 0, val, true);
		rkisp_clear_reg_cache_bits(dev, CIF_ISP_CTRL, val);
	}
}

static void rkisp_save_first_param_v35(struct rkisp_isp_params_vdev *params_vdev, void *param)
{
	u32 size;

	if (!params_vdev->dev->is_rtt_first) {
		size = params_vdev->vdev_fmt.fmt.meta.buffersize;
		memcpy(params_vdev->isp35_params, param, size);
	} else {
		/* left and right params for unit fast case */
		size = sizeof(struct isp35_isp_params_cfg);
		memcpy(params_vdev->isp35_params, param, size);
		if (params_vdev->dev->unite_div == ISP_UNITE_DIV2)
			memcpy(params_vdev->isp35_params + 1, param, size);
	}
}

static void rkisp_clear_first_param_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	u32 mult = params_vdev->dev->hw_dev->unite ? ISP_UNITE_MAX : 1;
	u32 size = sizeof(struct isp35_isp_params_cfg) * mult;

	memset(params_vdev->isp33_params, 0, size);
}

static void rkisp_deinit_mesh_buf(struct rkisp_isp_params_vdev *params_vdev,
				  u64 module_id, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_dummy_buffer *buf = NULL;
	int i;

	if (!priv)
		return;

	switch (module_id) {
	case ISP35_MODULE_LDCH:
		buf = priv->buf_ldch[id];
		break;
	case ISP35_MODULE_BAY3D:
		buf = priv->buf_b3dldc[id];
		break;
	default:
		return;
	}

	for (i = 0; i < ISP35_MESH_BUF_NUM; i++)
		rkisp_free_buffer(params_vdev->dev, buf + i);
}

static int rkisp_init_mesh_buf(struct rkisp_isp_params_vdev *params_vdev,
			       struct rkisp_meshbuf_size *meshsize)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *ispdev = params_vdev->dev;
	struct device *dev = ispdev->dev;
	struct isp2x_mesh_head *mesh_head;
	struct rkisp_dummy_buffer *buf;
	u32 mesh_w = meshsize->meas_width;
	u32 mesh_h = meshsize->meas_height;
	u32 mesh_size, buf_size;
	int i, ret, buf_cnt = meshsize->buf_cnt;
	int id = meshsize->unite_isp_id;
	bool is_alloc;

	if (!priv) {
		dev_err(dev, "priv_val is NULL\n");
		return -EINVAL;
	}

	switch (meshsize->module_id) {
	case ISP35_MODULE_LDCH:
		priv->buf_ldch_idx[id] = 0;
		buf = priv->buf_ldch[id];
		mesh_w = ((mesh_w + 15) / 16 + 2) / 2;
		mesh_h = (mesh_h + 7) / 8 + 1;
		mesh_size = mesh_w * 4 * mesh_h;
		break;
	case ISP35_MODULE_BAY3D:
		priv->buf_b3dldc_idx[id] = 0;
		buf = priv->buf_b3dldc[id];
		/* b3d_ldch */
		mesh_w = DIV_ROUND_UP(ALIGN(mesh_w, 16) / 16 + 1, 2);
		mesh_h = ALIGN(mesh_h, 8) / 8 + 1;
		mesh_size = ALIGN(mesh_w * 4 * mesh_h, 16);
		priv->b3dldc_hsize = mesh_w;
		priv->b3dldch_vsize = mesh_h;
		/* b3d_ldcv */
		mesh_h = ALIGN(meshsize->meas_height, 16) / 16 + 2;
		mesh_size += (mesh_w * 4 * mesh_h);
		priv->b3dldcv_vsize = mesh_h;
		break;
	default:
		return -EINVAL;
	}

	if (buf_cnt <= 0 || buf_cnt > ISP35_MESH_BUF_NUM)
		buf_cnt = ISP35_MESH_BUF_NUM;
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
				if (rkisp_buf_get_fd(ispdev, buf, false) < 0)
					goto err;
				mesh_head = (struct isp2x_mesh_head *)buf->vaddr;
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
		}
		mesh_head->stat = MESH_BUF_INIT;
		mesh_head->data_oft = ALIGN(sizeof(struct isp2x_mesh_head), 16);
		if (meshsize->module_id == ISP35_MODULE_BAY3D)
			mesh_head->data1_oft = mesh_head->data_oft +
				ALIGN(priv->b3dldc_hsize * 4 * priv->b3dldch_vsize, 16);
		buf++;
	}

	return 0;
err:
	rkisp_deinit_mesh_buf(params_vdev, meshsize->module_id, id);
	return -ENOMEM;
}

static void
rkisp_get_param_size_v35(struct rkisp_isp_params_vdev *params_vdev,
			 unsigned int sizes[])
{
	u32 mult = params_vdev->dev->unite_div;

	sizes[0] = sizeof(struct isp35_isp_params_cfg) * mult;
	params_vdev->vdev_fmt.fmt.meta.buffersize = sizes[0];
}

static void
rkisp_params_get_meshbuf_inf_v35(struct rkisp_isp_params_vdev *params_vdev,
				 void *meshbuf_inf)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_meshbuf_info *meshbuf = meshbuf_inf;
	struct rkisp_dummy_buffer *buf;
	int i, id = meshbuf->unite_isp_id;

	switch (meshbuf->module_id) {
	case ISP35_MODULE_LDCH:
		priv->buf_ldch_idx[id] = 0;
		buf = priv->buf_ldch[id];
		break;
	case ISP35_MODULE_BAY3D:
		priv->buf_b3dldc_idx[id] = 0;
		buf = priv->buf_b3dldc[id];
		break;
	default:
		return;
	}

	for (i = 0; i < ISP35_MESH_BUF_NUM; i++) {
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
rkisp_params_set_meshbuf_size_v35(struct rkisp_isp_params_vdev *params_vdev,
				  void *size)
{
	struct rkisp_meshbuf_size *meshsize = size;

	if (!params_vdev->dev->hw_dev->unite)
		meshsize->unite_isp_id = 0;
	return rkisp_init_mesh_buf(params_vdev, meshsize);
}

static void
rkisp_params_free_meshbuf_v35(struct rkisp_isp_params_vdev *params_vdev,
			      u64 module_id)
{
	int id;

	for (id = 0; id < params_vdev->dev->unite_div; id++)
		rkisp_deinit_mesh_buf(params_vdev, module_id, id);
}

static int
rkisp_params_info2ddr_cfg_v35(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_info2ddr *cfg = arg;
	struct rkisp_dummy_buffer *buf;
	u32 reg, ctrl, mask, size, val, wsize = 0, vsize = 0;
	int i, ret;

	if (dev->is_aiisp_en) {
		dev_err(dev->dev, "%s no support for aiisp enable\n", __func__);
		return -EINVAL;
	}

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
		priv->buf_info_owner = cfg->owner;
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
		buf = &priv->buf_info[i];
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
	buf = &priv->buf_info[0];
	isp3_param_write(params_vdev, buf->dma_addr, ISP3X_MI_GAIN_WR_BASE, 0);
	isp3_param_write(params_vdev, buf->size, ISP3X_MI_GAIN_WR_SIZE, 0);
	isp3_param_write(params_vdev, wsize, ISP3X_MI_GAIN_WR_LENGTH, 0);
	if (dev->hw_dev->is_single)
		rkisp_write(dev, ISP3X_MI_WR_CTRL2, ISP3X_GAINSELF_UPD, true);
	rkisp_set_reg_cache_bits(dev, reg, mask, ctrl);

	priv->buf_info_idx = 0;
	priv->buf_info_cnt = cfg->buf_cnt;
	priv->buf_info_owner = cfg->owner;

	cfg->wsize = wsize;
	cfg->vsize = vsize;
	return 0;
err:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_info[i];
		rkisp_free_buffer(dev, buf);
		cfg->buf_fd[i] = -1;
	}
	cfg->owner = RKISP_INFO2DRR_OWNER_NULL;
	cfg->buf_cnt = 0;
	return -ENOMEM;
}

static int
rkisp_alloc_vpsl_buf(struct rkisp_isp_params_vdev *params_vdev,
		     struct rkisp_bnr_buf_info *bnrbuf)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_dummy_buffer *buf;
	u32 w = dev->isp_sdev.out_crop.width;
	u32 h = dev->isp_sdev.out_crop.height;
	u32 size, vpsl_size, stride, ds_w, ds_h, ds_ch;
	int i, ret, cnt;

	/* yraw down sample */
	if (priv->yraw_sel) {
		ds_ch = VPSL_YRAW_CHN_MAX / 2;
		ds_w = ALIGN((w + 1) / 2, 2);
	} else {
		ds_ch = VPSL_YRAW_CHN_MAX;
		ds_w = (w + 1) / 2;
	}
	ds_h = (h + 1) / 2;
	vpsl_size = 0;
	for (i = 0; i < ds_ch; i++) {
		if (priv->yraw_sel)
			stride = ALIGN(((ds_w * 11) + 7) / 8, 16);
		else
			stride = ALIGN(ds_w, 16);
		priv->vpsl_yraw_stride[i] = stride;
		priv->vpsl_yraw_offs[i] = vpsl_size;
		bnrbuf->u.v35.vpsl_yraw_stride[i] = stride;
		bnrbuf->u.v35.vpsl_yraw_offs[i] = vpsl_size;
		size = stride * ds_h;
		vpsl_size += size;

		ds_w = priv->yraw_sel ? ALIGN((ds_w + 1) / 2, 2) : (ds_w + 1) / 2;
		ds_h = (ds_h + 1) / 2;
	}
	/* Sigma down sample */
	ds_ch = priv->yraw_sel ? VPSL_SIG_CHN_MAX - 1 : VPSL_SIG_CHN_MAX;
	ds_w = (w + 1) / 2;
	ds_h = (h + 1) / 2;
	for (i = 0; i < ds_ch; i++) {
		stride = ALIGN(ds_w, 16);
		priv->vpsl_sig_stride[i] = stride;
		priv->vpsl_sig_offs[i] = vpsl_size;
		bnrbuf->u.v35.vpsl_sig_stride[i] = stride;
		bnrbuf->u.v35.vpsl_sig_offs[i] = vpsl_size;
		size = stride * ds_h;
		vpsl_size += size;

		ds_w = (ds_w + 1) / 2;
		ds_h = (ds_h + 1) / 2;
	}

	cnt = bnrbuf->u.v35.vpsl.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv->buf_vpsl[i];
		buf->size = vpsl_size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc vpsl buf%d fail:%d\n", i, ret);
			goto err_vpsl;
		}
		if (!i)
			priv->pbuf_vpsl = buf;
		else
			list_add_tail(&buf->queue, &priv->vpsl_list);
		buf->index = i;
		bnrbuf->u.v35.vpsl.buf_fd[i] = buf->dma_fd;
	}
	priv->vpsl_cnt = cnt;
	bnrbuf->u.v35.vpsl.buf_cnt = cnt;
	bnrbuf->u.v35.vpsl.buf_size = vpsl_size;
	return 0;
err_vpsl:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_vpsl[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->vpsl_cnt = 0;
	bnrbuf->u.v35.vpsl.buf_cnt = 0;
	bnrbuf->u.v35.vpsl.buf_size = 0;
	return ret;
}

static int
rkisp_params_init_bnr_buf_v35(struct rkisp_isp_params_vdev *params_vdev,
			      struct rkisp_bnr_buf_info *bnrbuf)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_subdev *isp_sdev = &dev->isp_sdev;
	struct rkisp_dummy_buffer *buf;
	u32 w = isp_sdev->out_crop.width;
	u32 h = isp_sdev->out_crop.height;
	u32 iir_rw_fmt, size, val, w16, w32, w128, iir_size = 0;
	int ret, i, cnt;

	INIT_LIST_HEAD(&priv->iir_list);
	INIT_LIST_HEAD(&priv->gain_list);
	INIT_LIST_HEAD(&priv->vpsl_list);
	INIT_LIST_HEAD(&priv->aipre_gain_list);
	INIT_LIST_HEAD(&priv->y_src_list);

	iir_rw_fmt = bnrbuf->u.v35.iir_rw_fmt;
	if (dev->unite_div > ISP_UNITE_DIV1)
		w = w / 2 + dev->hw_dev->unite_extend_pixel;
	if (dev->unite_div == ISP_UNITE_DIV4)
		h = h / 2 + dev->hw_dev->unite_extend_pixel;
	w16 = ALIGN(w, 16);
	w32 = ALIGN(w, 32);
	w128 = ALIGN(w, 128);
	priv->bay3d_iir_stride = 0;
	priv->bay3d_iir_offs = 0;
	switch (iir_rw_fmt) {
	case 0:
		val = w16 * 7 / 4;
		size = val * h;
		break;
	case 1:
		size = w16 * h * 2;
		break;
	case 2:
	case 4:
		val = ALIGN(w16 * 9 / 4, 16);
		size = val * h;
		priv->bay3d_iir_stride = val;
		break;
	case 3:
		val = ALIGN((w32 + w128 / 8) * 2, 16);
		size = val * h;
		priv->bay3d_iir_stride = val;
		priv->bay3d_iir_offs = w32 * 2;
		break;
	default:
		dev_err(dev->dev, "bay3d iir_rw_fmt:%d error\n", iir_rw_fmt);
		return -EINVAL;
	}
	size = ALIGN(size, 16);
	priv->bay3d_iir_size = size;
	if (dev->unite_div > ISP_UNITE_DIV1)
		size *= dev->unite_div;
	cnt = bnrbuf->iir.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	if (iir_rw_fmt == 3 && cnt < 2)
		cnt = 2;
	for (i = 0; i < cnt; i++) {
		buf = &priv->buf_bay3d_iir[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc bay3d iir buf%d fail:%d\n", i, ret);
			goto err_iir;
		}
		if (!i)
			priv->pbuf_bay3d_iir = buf;
		else
			list_add_tail(&buf->queue, &priv->iir_list);
		buf->index = i;
		bnrbuf->iir.buf_fd[i] = buf->dma_fd;
	}
	priv->bay3d_iir_cnt = cnt;
	bnrbuf->iir.buf_cnt = cnt;
	bnrbuf->iir.buf_size = size;
	iir_size = size;

	val = (w16 * 36 / 8 + 31) / 32 * 4;
	size = ALIGN(val * ((h + 7) / 8), 16);
	priv->bay3d_ds_size = size;
	if (dev->unite_div > ISP_UNITE_DIV1)
		size *= dev->unite_div;
	cnt = bnrbuf->u.v35.ds.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv->buf_bay3d_ds[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc bay3d ds buf:%d fail:%d\n", i, ret);
			goto err_ds;
		}
		buf->index = i;
		bnrbuf->u.v35.ds.buf_fd[i] = buf->dma_fd;
	}
	priv->bay3d_ds_cnt = cnt;
	bnrbuf->u.v35.ds.buf_cnt = cnt;
	bnrbuf->u.v35.ds.buf_size = size;

	val = (((w + 31) / 32 + 1) / 2 * 2 + 3) / 4 * 4;
	size = ALIGN(val * ((h + 31) / 32), 16);
	priv->bay3d_wgt_size = size;
	if (dev->unite_div > ISP_UNITE_DIV1)
		size *= dev->unite_div;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv->buf_bay3d_wgt[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc bay3d wgt buf:%d fail:%d\n", i, ret);
			goto err_wgt;
		}
		buf->index = i;
		bnrbuf->u.v35.wgt.buf_fd[i] = buf->dma_fd;
	}
	priv->bay3d_wgt_cnt = cnt;
	bnrbuf->u.v35.wgt.buf_cnt = cnt;
	bnrbuf->u.v35.wgt.buf_size = size;

	cnt = bnrbuf->u.v35.aiisp.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt && iir_size; i++) {
		buf = &priv->buf_aiisp[i];
		buf->size = iir_size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc aiisp buf%d fail:%d\n", i, ret);
			goto err_aiisp;
		}
		buf->index = i;
		bnrbuf->u.v35.aiisp.buf_fd[i] = buf->dma_fd;
	}
	priv->aiisp_cnt = cnt;
	bnrbuf->u.v35.aiisp.buf_cnt = cnt;
	bnrbuf->u.v35.aiisp.buf_size = iir_size;

	size = ALIGN(w * h / 4, 16);
	priv->gain_size = size;
	if (dev->unite_div > ISP_UNITE_DIV1)
		size *= dev->unite_div;
	cnt = bnrbuf->u.v35.gain.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv->buf_gain[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc gain buf%d fail:%d\n", i, ret);
			goto err_gain;
		}
		if (!i)
			priv->pbuf_gain_wr = buf;
		else
			list_add_tail(&buf->queue, &priv->gain_list);
		buf->index = i;
		bnrbuf->u.v35.gain.buf_fd[i] = buf->dma_fd;
	}
	priv->gain_cnt = cnt;
	bnrbuf->u.v35.gain.buf_cnt = cnt;
	bnrbuf->u.v35.gain.buf_size = size;

	val = ALIGN(w / 4, 16);
	priv->aipre_gain_stride = val;
	size = ALIGN(val * (h / 2), 16);
	if (dev->unite_div > ISP_UNITE_DIV1)
		size *= dev->unite_div;
	cnt = bnrbuf->u.v35.aipre_gain.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv->buf_aipre_gain[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc aipre gain buf%d fail:%d\n", i, ret);
			goto err_aipre_gain;
		}
		if (!i)
			priv->pbuf_aipre_gain = buf;
		else
			list_add_tail(&buf->queue, &priv->aipre_gain_list);
		buf->index = i;
		bnrbuf->u.v35.aipre_gain.buf_fd[i] = buf->dma_fd;
	}
	priv->aipre_gain_cnt = cnt;
	bnrbuf->u.v35.aipre_gain.buf_cnt = cnt;
	bnrbuf->u.v35.aipre_gain.buf_size = size;

	priv->bay3d_iir_rw_fmt = iir_rw_fmt;
	priv->yraw_sel = !!bnrbuf->u.v35.yraw_sel;
	ret = rkisp_alloc_vpsl_buf(params_vdev, bnrbuf);
	if (ret)
		goto err_vpsl;

	size = ALIGN(w, 16) * h;
	if (dev->unite_div > ISP_UNITE_DIV1)
		size *= dev->unite_div;
	cnt = bnrbuf->u.v35.y_src.buf_cnt;
	if (cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	for (i = 0; i < cnt; i++) {
		buf = &priv->buf_y_src[i];
		buf->size = size;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "alloc y buf%d fail:%d\n", i, ret);
			goto err_y_src;
		}
		if (!i)
			priv->pbuf_y_src = buf;
		else
			list_add_tail(&buf->queue, &priv->y_src_list);
		buf->index = i;
		bnrbuf->u.v35.y_src.buf_fd[i] = buf->dma_fd;
	}
	if (cnt && !priv->yraw_sel)
		dev->is_aiisp_yuv = true;
	priv->y_src_cnt = cnt;
	bnrbuf->u.v35.y_src.buf_cnt = cnt;
	bnrbuf->u.v35.y_src.buf_size = size;
	return 0;
err_y_src:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_y_src[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->y_src_cnt = 0;
	bnrbuf->u.v35.y_src.buf_cnt = 0;
	bnrbuf->u.v35.y_src.buf_size = 0;

	i = priv->vpsl_cnt;
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_vpsl[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->vpsl_cnt = 0;
	bnrbuf->u.v35.vpsl.buf_cnt = 0;
	bnrbuf->u.v35.vpsl.buf_size = 0;
err_vpsl:
	i = priv->aipre_gain_cnt;
err_aipre_gain:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_aipre_gain[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->aipre_gain_cnt = 0;
	bnrbuf->u.v35.aipre_gain.buf_cnt = 0;
	bnrbuf->u.v35.aipre_gain.buf_size = 0;

	i = priv->gain_cnt;
err_gain:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_gain[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->gain_cnt = 0;
	bnrbuf->u.v35.gain.buf_cnt = 0;
	bnrbuf->u.v35.gain.buf_size = 0;

	i = priv->aiisp_cnt;
err_aiisp:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_aiisp[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->aiisp_cnt = 0;
	bnrbuf->u.v35.aiisp.buf_cnt = 0;
	bnrbuf->u.v35.aiisp.buf_size = 0;

	i = priv->bay3d_wgt_cnt;
err_wgt:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_bay3d_wgt[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->bay3d_wgt_cnt = 0;
	bnrbuf->u.v35.wgt.buf_cnt = 0;
	bnrbuf->u.v35.wgt.buf_size = 0;

	i = priv->bay3d_ds_cnt;
err_ds:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_bay3d_ds[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->bay3d_ds_cnt = 0;
	bnrbuf->u.v35.ds.buf_cnt = 0;
	bnrbuf->u.v35.ds.buf_size = 0;

	i = priv->bay3d_iir_cnt;
err_iir:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_bay3d_iir[i];
		rkisp_free_buffer(dev, buf);
	}
	priv->bay3d_iir_cnt = 0;
	bnrbuf->iir.buf_cnt = 0;
	bnrbuf->iir.buf_size = 0;
	return ret;
}

static int
rkisp_params_get_aiawb_buffd_v35(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_aiawb_buffd *cfg = arg;
	struct rkisp_dummy_buffer *buf;
	struct v4l2_rect *out_crop = &dev->isp_sdev.out_crop;
	u32 width = out_crop->width, height = out_crop->height;
	int i, size, ret, cnt = cfg->info.buf_cnt;

	if (cnt <= 0 || cnt >= RKISP_BUFFER_MAX)
		cnt = RKISP_BUFFER_MAX - 1;
	switch (cfg->ds) {
	case RKISP_AIAWB_DS_4X4:
		size = (width / 4) * (height / 4) * 8;
		break;
	case RKISP_AIAWB_DS_8X4:
		size = (width / 8) * (height / 4) * 8;
		break;
	case RKISP_AIAWB_DS_8X8:
		size = (width / 8) * (height / 8) * 8;
		break;
	default:
	case RKISP_AIAWB_DS_16X16:
		size = (width / 16) * (height / 16) * 8;
		break;
	}
	for (i = 0; i < cnt; i++) {
		buf = &priv->buf_aiawb[i];
		if (buf->mem_priv)
			rkisp_free_buffer(dev, buf);
		buf->size = size;
		buf->is_need_vaddr = true;
		buf->is_need_dbuf = true;
		buf->is_need_dmafd = true;
		ret = rkisp_alloc_buffer(dev, buf);
		if (ret) {
			dev_err(dev->dev, "%s alloc buf failed\n", __func__);
			goto err;
		}
		buf->index = i;
		cfg->info.buf_fd[i] = buf->dma_fd;
		v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev,
			 "%s ds:%d idx:%d dma:0x%x fd:%d\n",
			 __func__, cfg->ds, i, (u32)buf->dma_addr, buf->dma_fd);
	}
	cfg->info.buf_cnt = cnt;
	cfg->info.buf_size = size;
	priv->buf_aiawb_idx = 0;
	priv->buf_aiawb_cnt = cnt;
	return 0;
err:
	for (i -= 1; i >= 0; i--) {
		buf = &priv->buf_aiawb[i];
		rkisp_free_buffer(dev, buf);
		cfg->info.buf_fd[i] = -1;
	}
	cfg->info.buf_cnt = 0;
	return -ENOMEM;
}

static void
rkisp_params_stream_stop_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	int i;

	for (i = 0; i < priv->y_src_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_y_src[i]);
	priv->y_src_cnt = 0;
	for (i = 0; i < priv->vpsl_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_vpsl[i]);
	priv->vpsl_cnt = 0;
	for (i = 0; i < priv->aipre_gain_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_aipre_gain[i]);
	priv->aipre_gain_cnt = 0;
	for (i = 0; i < priv->gain_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_gain[i]);
	priv->gain_cnt = 0;
	for (i = 0; i < priv->aiisp_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_aiisp[i]);
	priv->aiisp_cnt = 0;
	for (i = 0; i < priv->bay3d_wgt_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_bay3d_wgt[i]);
	priv->bay3d_wgt_cnt = 0;
	for (i = 0; i < priv->bay3d_ds_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_bay3d_ds[i]);
	priv->bay3d_ds_cnt = 0;
	for (i = 0; i < priv->bay3d_iir_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_bay3d_iir[i]);
	priv->bay3d_iir_cnt = 0;
	for (i = 0; i < RKISP_STATS_DDR_BUF_NUM; i++)
		rkisp_free_buffer(dev, &dev->stats_vdev.stats_buf[i]);
	for (i = 0; i < priv->buf_aiawb_cnt; i++)
		rkisp_free_buffer(dev, &priv->buf_aiawb[i]);
	for (i = 0; i < RKISP_INFO2DDR_BUF_MAX; i++)
		rkisp_free_buffer(dev, &priv->buf_info[i]);
	priv->buf_aiawb_cnt = 0;
	priv->buf_aiawb_idx = -1;
	priv->buf_info_owner = 0;
	priv->buf_info_cnt = 0;
	priv->buf_info_idx = -1;
}

static void
rkisp_params_fop_release_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	int id;

	for (id = 0; id < params_vdev->dev->unite_div; id++)
		rkisp_deinit_mesh_buf(params_vdev, ISP35_MODULE_LDCH, id);
}

static void
rkisp_params_disable_isp_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	int i;

	params_vdev->isp35_params->module_ens = 0;
	params_vdev->isp35_params->module_en_update = ~ISP35_MODULE_FORCE;
	for (i = 0; i < params_vdev->dev->unite_div; i++) {
		__isp_isr_other_en(params_vdev, params_vdev->isp35_params, RKISP_PARAMS_ALL, i);
		__isp_isr_meas_en(params_vdev, params_vdev->isp35_params, RKISP_PARAMS_ALL, i);
	}
}

static void
module_data_abandon(struct rkisp_isp_params_vdev *params_vdev,
		    struct isp35_isp_params_cfg *params, u32 id)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct isp2x_mesh_head *mesh_head;
	int i;

	if (params->module_cfg_update & ISP35_MODULE_LDCH) {
		const struct isp32_ldch_cfg *arg = &params->others.ldch_cfg;

		for (i = 0; i < ISP35_MESH_BUF_NUM; i++) {
			if (priv->buf_ldch[id][i].vaddr &&
			    arg->buf_fd == priv->buf_ldch[id][i].dma_fd) {
				mesh_head = priv->buf_ldch[id][i].vaddr;
				mesh_head->stat = MESH_BUF_CHIPINUSE;
				break;
			}
		}
	}

	if (params->module_cfg_update & ISP35_MODULE_BAY3D) {
		const struct isp35_bay3d_cfg *arg = &params->others.bay3d_cfg;

		for (i = 0; i < ISP35_MESH_BUF_NUM; i++) {
			if (priv->buf_b3dldc[id][i].vaddr &&
			    arg->lut_buf_fd == priv->buf_b3dldc[id][i].dma_fd) {
				mesh_head = priv->buf_b3dldc[id][i].vaddr;
				mesh_head->stat = MESH_BUF_CHIPINUSE;
				break;
			}
		}
	}
}

static void
rkisp_params_cfg_latter_v35(struct rkisp_isp_params_vdev *params_vdev, u32 frame_id)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct isp35_isp_params_cfg *new_params = NULL;
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
				    (new_params->module_cfg_update & ISP35_MODULE_FORCE)) {
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
				if (new_params->module_cfg_update & (ISP35_MODULE_LDCH | ISP35_MODULE_BAY3D))
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
rkisp_params_cfg_v35(struct rkisp_isp_params_vdev *params_vdev,
		     u32 frame_id, enum rkisp_params_type type)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct isp35_isp_params_cfg *params_rec, *new_params = NULL;
	struct rkisp_buffer *cur_buf = NULL;
	unsigned long flags = 0;
	int i;

	if (type == RKISP_PARAMS_LAT) {
		rkisp_params_cfg_latter_v35(params_vdev, frame_id);
		return;
	}

	spin_lock_irqsave(&params_vdev->config_lock, flags);
	if (!params_vdev->streamon)
		goto unlock;

	/* get buffer by frame_id */
	while (!list_empty(&params_vdev->params)) {
		cur_buf = list_first_entry(&params_vdev->params, struct rkisp_buffer, queue);
		new_params = (struct isp35_isp_params_cfg *)(cur_buf->vaddr[0]);
		if (new_params->frame_id < frame_id) {
			list_del(&cur_buf->queue);
			for (i = 0; i < dev->unite_div; i++) {
				/* update en immediately */
				if (new_params->module_en_update ||
				    (new_params->module_cfg_update & ISP35_MODULE_FORCE)) {
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
				     (ISP35_MODULE_LDCH | ISP35_MODULE_BAY3D)))
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

	params_rec = params_vdev->isp35_params;
	new_params = (struct isp35_isp_params_cfg *)(cur_buf->vaddr[0]);
	for (i = 0; i < dev->unite_div; i++) {
		__isp_isr_meas_config(params_vdev, new_params, type, i);
		__isp_isr_other_config(params_vdev, new_params, type, i);
		__isp_isr_other_en(params_vdev, new_params, type, i);
		__isp_isr_meas_en(params_vdev, new_params, type, i);
		if (new_params->module_cfg_update & ISP35_MODULE_HDRMGE) {
			params_rec->others.hdrmge_cfg = new_params->others.hdrmge_cfg;
			params_rec->module_cfg_update |= ISP35_MODULE_HDRMGE;
		}
		if (new_params->module_cfg_update & ISP35_MODULE_DRC && !dev->is_aiisp_en) {
			params_rec->others.drc_cfg = new_params->others.drc_cfg;
			params_rec->module_cfg_update |= ISP35_MODULE_DRC;
		}
		if (!dev->is_aiisp_en)
			new_params->module_cfg_update = 0;
		new_params++;
		params_rec++;
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

	if (params_vdev->dev->hw_dev->is_single)
		return;
	value &= (ISP3X_YNR_FST_FRAME | ISP33_YHIST_FST_FRAME |
		  ISP3X_CNR_FST_FRAME | ISP3X_RAW3D_FST_FRAME |
		  ISP33_ENH_FST_FRAME);
	for (i = 0; i < params_vdev->dev->unite_div && value; i++)
		isp3_param_clear_bits(params_vdev, ISP3X_ISP_CTRL1, value, i);
}

static void
rkisp_params_aiisp_update_buf(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
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
	if (!list_empty(&priv->gain_list)) {
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
	priv->pbuf_aipre_gain = NULL;
	if (!list_empty(&priv->aipre_gain_list)) {
		priv->pbuf_aipre_gain = list_first_entry(&priv->aipre_gain_list,
					struct rkisp_dummy_buffer, queue);
		list_del(&priv->pbuf_aipre_gain->queue);

		val = priv->pbuf_aipre_gain->dma_addr;
		isp3_param_write(params_vdev, val, ISP35_AI_PRE_GAIN_WR_BASE, 0);
		priv->aipre_gain_cur_idx = priv->pbuf_aipre_gain->index;

		if (params_vdev->dev->hw_dev->is_single) {
			val = isp3_param_read(params_vdev, ISP35_AI_CTRL, 0);
			val &= ~ISP35_AIISP_ST;
			val |= ISP35_AIPRE_ITS_FORCE_UPD;
			isp3_param_write(params_vdev, val, ISP35_AI_CTRL, 0);
			val &= ~ISP35_AIPRE_ITS_FORCE_UPD;
			isp3_param_write(params_vdev, val, ISP35_AI_CTRL, 0);
		}
	}

	v4l2_dbg(3, rkisp_debug, &params_vdev->dev->v4l2_dev,
		 "aiisp_update %x:%x %x:%x %x:%x %x:%x, iir:%x gain:%x aipre:%x\n",
		 ISP3X_MI_BAY3D_IIR_WR_BASE_SHD,
		 isp3_param_read_direct(params_vdev, ISP3X_MI_BAY3D_IIR_WR_BASE_SHD),
		 ISP3X_MI_BAY3D_IIR_RD_BASE_SHD,
		 isp3_param_read_direct(params_vdev, ISP3X_MI_BAY3D_IIR_RD_BASE_SHD),
		 ISP3X_MI_GAIN_WR_BASE_SHD,
		 isp3_param_read_direct(params_vdev, ISP3X_MI_GAIN_WR_BASE_SHD),
		 ISP35_AI_PRE_GAIN_WR_BASE,
		 isp3_param_read_direct(params_vdev, ISP35_AI_PRE_GAIN_WR_BASE),
		 priv->pbuf_bay3d_iir ? (u32)priv->pbuf_bay3d_iir->dma_addr : 0,
		 priv->pbuf_gain_wr ? (u32)priv->pbuf_gain_wr->dma_addr : 0,
		 priv->pbuf_aipre_gain ? (u32)priv->pbuf_aipre_gain->dma_addr : 0);
	if (!priv->pbuf_gain_wr || !priv->pbuf_aipre_gain || !priv->pbuf_bay3d_iir) {
		if (priv->pbuf_bay3d_iir) {
			list_add_tail(&priv->pbuf_bay3d_iir->queue, &priv->iir_list);
			priv->pbuf_bay3d_iir = NULL;
		}
		if (priv->pbuf_gain_wr) {
			list_add_tail(&priv->pbuf_gain_wr->queue, &priv->gain_list);
			priv->pbuf_gain_wr = NULL;
		}
		if (priv->pbuf_aipre_gain) {
			list_add_tail(&priv->pbuf_aipre_gain->queue, &priv->aipre_gain_list);
			priv->pbuf_aipre_gain = NULL;
		}
	}
	spin_unlock_irqrestore(&priv->buf_lock, lock_flags);
}

static void
rkisp_params_aiisp_event_v35(struct rkisp_isp_params_vdev *params_vdev, u32 irq)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_stream *stream = &dev->cap_dev.stream[RKISP_STREAM_MP];
	struct rkisp_dummy_buffer *buf = NULL;
	struct v4l2_event ev = { 0 };
	struct rkisp_aiisp_ev_info *ev_info;
	unsigned long lock_flags = 0;
	u32 h = dev->isp_sdev.out_crop.height;
	u32 val, wr_line, rd_line;
	bool is_event_queue = true;

	if (sizeof(*ev_info) > sizeof(ev.u)) {
		v4l2_err(&dev->v4l2_dev, "aiisp_ev_info too large\n");
		return;
	}
	ev.type = RKISP_V4L2_EVENT_AIISP_LINECNT;
	ev_info = (struct rkisp_aiisp_ev_info *)ev.u.data;
	ev_info->iir_index = -1;
	ev_info->gain_index = -1;
	ev_info->aiisp_index = -1;
	ev_info->vpsl_index = -1;
	ev_info->aipre_gain_index = -1;
	ev_info->y_src_index = -1;
	ev_info->y_dest_index = -1;
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
	} else {
		wr_line = ISP39_AIISP_WR_LINECNT(val);
		ev.id = RKISP_AIISP_WR_LINECNT_ID;
		ev_info->height = !wr_line ? h : wr_line;
		rkisp_dmarx_get_frame(dev, &ev_info->sequence, NULL, &ev_info->timestamp, true);

		if (dev->aiisp_cfg.wr_mode) {
			wr_line += dev->aiisp_cfg.wr_linecnt;
			if (wr_line > h)
				wr_line = h - 1;
			rkisp_write(dev, ISP32_ISP_IRQ_CFG1, wr_line << 16, true);
		}

		if (dev->is_aiisp_yuv) {
			if (!stream->curr_buf) {
				v4l2_dbg(2, rkisp_debug, &dev->v4l2_dev,
					 "%s seq:%d stream output no buf, drop event\n",
					 __func__, ev_info->sequence);
				return;
			}
			ev_info->y_dest_index = stream->curr_buf->index;
			ev_info->y_src_index = priv->y_src_cur_idx;
		}
		spin_lock_irqsave(&priv->buf_lock, lock_flags);
		if (dev->is_aiisp_en &&
		    (!priv->pbuf_bay3d_iir || !priv->pbuf_vpsl ||
		     !priv->pbuf_gain_wr || !priv->pbuf_aipre_gain)) {
			if (priv->pbuf_bay3d_iir) {
				list_add_tail(&priv->pbuf_bay3d_iir->queue, &priv->iir_list);
				priv->pbuf_bay3d_iir = NULL;
			}
			if (priv->pbuf_gain_wr) {
				list_add_tail(&priv->pbuf_gain_wr->queue, &priv->gain_list);
				priv->pbuf_gain_wr = NULL;
			}
			if (priv->pbuf_aipre_gain) {
				list_add_tail(&priv->pbuf_aipre_gain->queue, &priv->aipre_gain_list);
				priv->pbuf_aipre_gain = NULL;
			}
			if (priv->pbuf_vpsl) {
				list_add_tail(&priv->pbuf_vpsl->queue, &priv->vpsl_list);
				priv->pbuf_vpsl = NULL;
			}
			is_event_queue = false;
		}

		buf = priv->pbuf_vpsl;
		if (buf)
			ev_info->vpsl_index = buf->index;
		buf = priv->pbuf_aipre_gain;
		if (buf)
			ev_info->aipre_gain_index = buf->index;
		buf = priv->pbuf_bay3d_iir;
		if (buf)
			ev_info->iir_index = buf->index;
		buf = priv->pbuf_gain_wr;
		if (buf)
			ev_info->gain_index = buf->index;
		spin_unlock_irqrestore(&priv->buf_lock, lock_flags);
	}
	if (is_event_queue)
		v4l2_event_queue(dev->isp_sdev.sd.devnode, &ev);
	v4l2_dbg(3, rkisp_debug, &dev->v4l2_dev,
		 "%s seq:%d height:%d idx(iir:%d gain:%d vpsl:%d aipre:%d aiisp:%d ysrc:%d ydst:%d)\n",
		 ev.id ? "isp_be" : "isp_fe", ev_info->sequence, ev_info->height,
		 ev_info->iir_index, ev_info->gain_index,
		 ev_info->vpsl_index, ev_info->aipre_gain_index, ev_info->aiisp_index,
		 ev_info->y_src_index, ev_info->y_dest_index);
}

static int
rkisp_params_aiisp_start_v35(struct rkisp_isp_params_vdev *params_vdev,
			     struct rkisp_aiisp_st *st)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_dummy_buffer *buf, *buf_tmp;
	unsigned long lock_flags = 0;
	u32 val, aiisp_rd, seq = st->sequence;

	if (!dev->is_aiisp_en && !dev->is_aiisp_stop)
		return -EINVAL;
	v4l2_dbg(3, rkisp_debug, &dev->v4l2_dev,
		 "isp_be input seq:%d idx(iir:%d gain:%d vpsl:%d aipre:%d aiisp:%d)\n",
		 seq, st->iir_index, st->gain_index, st->vpsl_index,
		 st->aipre_gain_index, st->aiisp_index);
	if (st->gain_index < 0 || st->gain_index >= priv->gain_cnt ||
	    st->iir_index < 0 || st->iir_index >= priv->bay3d_iir_cnt ||
	    st->aiisp_index >= priv->aiisp_cnt ||
	    st->vpsl_index >= priv->vpsl_cnt ||
	    st->aipre_gain_index >= priv->aipre_gain_cnt) {
		dev_err(dev->dev, "%s seq:%d error, aiisp(%d cnt:%d)\n"
			"iir(%d cnt:%d) gain(%d cnt:%d) aipre(%d cnt:%d) vpsl(%d cnt:%d)\n",
			__func__, seq, st->aiisp_index, priv->aiisp_cnt,
			st->iir_index, priv->bay3d_iir_cnt, st->gain_index, priv->gain_cnt,
			st->aipre_gain_index, priv->aipre_gain_cnt, st->vpsl_index, priv->vpsl_cnt);
		return -EINVAL;
	}

	if (!dev->is_aiisp_stop)
		rkisp_params_cfg(params_vdev, seq, RKISP_PARAMS_LAT);

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

	if (st->aipre_gain_index >= 0) {
		buf = &priv->buf_aipre_gain[st->aipre_gain_index];
		list_for_each_entry(buf_tmp, &priv->aipre_gain_list, queue) {
			if (buf_tmp == buf) {
				dev_err(dev->dev, "aipre idx:%d error\n", st->aipre_gain_index);
				spin_unlock_irqrestore(&priv->buf_lock, lock_flags);
				return 0;
			}
		}
		list_add_tail(&buf->queue, &priv->aipre_gain_list);
	}

	if (st->vpsl_index >= 0) {
		buf = &priv->buf_vpsl[st->vpsl_index];
		list_for_each_entry(buf_tmp, &priv->vpsl_list, queue) {
			if (buf_tmp == buf) {
				dev_err(dev->dev, "vpsl idx:%d error\n", st->vpsl_index);
				spin_unlock_irqrestore(&priv->buf_lock, lock_flags);
				return 0;
			}
		}
		list_add_tail(&buf->queue, &priv->vpsl_list);
	}
	priv->pbuf_gain_rd = &priv->buf_gain[st->gain_index];

	rkisp_write(dev, ISP39_AIISP_RD_BASE, aiisp_rd, false);
	val = priv->pbuf_gain_rd->dma_addr;
	if (!params_vdev->is_hdr) {
		rkisp_write(dev, ISP3X_MI_RAW0_RD_BASE, val, false);
		if (dev->hw_dev->is_single) {
			rkisp_set_bits(dev, ISP3X_CSI2RX_RAW_RD_CTRL, 0, ISP35_RX0_FORCE_UPD, true);
			rkisp_set_bits(dev, ISP3X_MI_WR_CTRL2, 0, ISP3X_DBR_RDSELF_UPD, true);
		}
	} else {
		rkisp_write(dev, ISP35_B3DLDCH_RD_BASE, val, false);
		if (dev->hw_dev->is_single) {
			val = ISP3X_DBR_RDSELF_UPD | ISP3X_BAY3D_RDSELF_UPD;
			rkisp_set_bits(dev, ISP3X_MI_WR_CTRL2, 0, val, true);
		}
	}
	spin_unlock_irqrestore(&priv->buf_lock, lock_flags);

	if (dev->is_aiisp_stop) {
		dev->hw_dev->is_be_idle = true;
		rkisp_params_aiisp_event_v35(params_vdev, ISP3X_OUT_FRM_QUARTER);
		return -EINVAL;
	}
	val = params_vdev->is_hdr ? ISP35_B3DLDCH_RD_BASE_SHD : ISP3X_MI_RAW0_RD_BASE_SHD;
	v4l2_dbg(3, rkisp_debug, &dev->v4l2_dev,
		 "isp_be start seq:%d (%x %x | %x:%x %x:%x)\n",
		 seq, aiisp_rd, (u32)priv->pbuf_gain_rd->dma_addr,
		 ISP3X_MI_DBR_RD_BASE_SHD, rkisp_read(dev, ISP3X_MI_DBR_RD_BASE_SHD, true),
		 val, rkisp_read(dev, val, true));
	return 0;
}

static void
rkisp_vpsl_update_regs_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	struct isp35_isp_params_cfg *params = params_vdev->isp35_params;
	struct rkisp_device *isp, *dev = params_vdev->dev;
	struct rkisp_hw_dev *hw = dev->hw_dev;
	void __iomem *base = hw->vpsl_base_addr;
	u32 i, *val, *flag;

	for (i = 0; i < hw->dev_link_num; i++) {
		isp = hw->isp[i];
		if (isp && isp->is_aiisp_en)
			break;
	}
	if (i == hw->dev_link_num)
		return;

	for (i = VPSL_CTRL; i < VPSL_SW_REG_SIZE; i += 4) {
		val = dev->sw_vpsl_base_addr + i;
		flag = dev->sw_vpsl_base_addr + i + VPSL_SW_REG_SIZE;

		if (*flag == SW_REG_CACHE)
			writel(*val, base + i);
	}
	vpsl_cfg_sram(params_vdev, &params->others.ai_cfg);
	writel(VPSL_CFG_GEN_UPD | VPSL_CFG_FORCE_UPD, base + VPSL_UPDATE);
	writel(VPSL_MI_FORCE_UPD, base + VPSL_MI_WR_INIT);
}

static void
rkisp_params_aiisp_switch_v35(struct rkisp_isp_params_vdev *params_vdev, bool on)
{
	struct rkisp_device *dev = params_vdev->dev;
	struct rkisp_isp_stats_vdev *stats_vdev = &dev->stats_vdev;
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct isp35_isp_params_cfg *params;
	struct rkisp_buffer *params_buf;
	unsigned long flags = 0;
	u32 val;

	v4l2_dbg(2, rkisp_debug, &dev->v4l2_dev,
		 "%s on:%d\n", __func__, on);
	if (!on) {
		spin_lock_irqsave(&params_vdev->config_lock, flags);
		while (!list_empty(&params_vdev->params_be)) {
			params_buf = list_first_entry(&params_vdev->params_be, struct rkisp_buffer, queue);
			list_del(&params_buf->queue);
			params = params_buf->vaddr[0];
			__isp_isr_meas_config(params_vdev, params, RKISP_PARAMS_LAT, 0);
			__isp_isr_other_config(params_vdev, params, RKISP_PARAMS_LAT, 0);
			__isp_isr_other_en(params_vdev, params, RKISP_PARAMS_LAT, 0);
			__isp_isr_meas_en(params_vdev, params, RKISP_PARAMS_LAT, 0);
			params->module_cfg_update = 0;
			vb2_buffer_done(&params_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
		}
		spin_unlock_irqrestore(&params_vdev->config_lock, flags);

		val = rkisp_read(dev, ISP35_AI_CTRL, false);
		val &= ~(ISP35_AIISP_ST | ISP35_AIISP_EN |
			 ISP35_AIPRE_IIR_EN | ISP35_AIPRE_GAIN_EN |
			 ISP35_AIPRE_IIR2DDR_EN | ISP35_AIPRE_GIAN2DDR_EN);
		val |= ISP35_AIPRE_ITS_FORCE_UPD;
		rkisp_write(dev, ISP35_AI_CTRL, val, false);
		val &= ~ISP35_AIPRE_ITS_FORCE_UPD;
		rkisp_write(dev, ISP35_AI_CTRL, val, false);

		val = rkisp_read(dev, ISP3X_MI_BAY3D_IIR_RD_BASE_SHD, true);
		rkisp_write(dev, ISP3X_MI_BAY3D_IIR_WR_BASE, val, false);
		if (dev->hw_dev->is_single) {
			val = ISP3X_BAY3D_IIRSELF_UPD | ISP3X_BAY3D_RDSELF_UPD |
			      ISP3X_GAINSELF_UPD;
			rkisp_set_bits(dev, MI_WR_CTRL2, 0, val, false);
		}

		rkisp_stats_first_ddr_config(stats_vdev);
		rkisp_stats_next_ddr_config(stats_vdev);

		val = rkisp_read(dev, ISP3X_ISP_CTRL1, false);
		val &= ~ISP35_BAYER_UPD_FE_EN;
		rkisp_write(dev, ISP3X_ISP_CTRL1, val, false);

		val = ISP39_AIISP_LINECNT_DONE | ISP3X_OUT_FRM_QUARTER | ISP3X_BAY3D_FRM_END;
		rkisp_clear_bits(dev, CIF_ISP_IMSC, val, false);

		if (IS_HDR_RDBK(dev->rd_mode))
			dev->irq_ends_mask &= ~(ISP_FRAME_BNR | ISP_FRAME_VPSL);
	} else {
		val = rkisp_read(dev, ISP3X_ISP_CTRL1, false);
		val |= ISP35_BAYER_PAT_FE(params_vdev->raw_type) | ISP35_BAYER_UPD_FE_EN;
		rkisp_write(dev, ISP3X_ISP_CTRL1, val, false);

		if (priv->pbuf_bay3d_iir) {
			val = priv->pbuf_bay3d_iir->dma_addr;
			rkisp_write(dev, ISP3X_MI_BAY3D_IIR_WR_BASE, val, false);
			if (dev->hw_dev->is_single) {
				val = ISP3X_BAY3D_IIRSELF_UPD | ISP3X_BAY3D_RDSELF_UPD |
				      ISP3X_GAINSELF_UPD;
				rkisp_set_bits(dev, MI_WR_CTRL2, 0, val, false);
			}
		}

		spin_lock_irqsave(&stats_vdev->rd_lock, flags);
		if (stats_vdev->cur_buf) {
			list_add_tail(&stats_vdev->cur_buf->queue, &stats_vdev->stat);
			stats_vdev->cur_buf = NULL;
		}
		if (stats_vdev->nxt_buf) {
			list_add_tail(&stats_vdev->nxt_buf->queue, &stats_vdev->stat);
			stats_vdev->nxt_buf = NULL;
		}
		spin_unlock_irqrestore(&stats_vdev->rd_lock, flags);
		rkisp_stats_first_ddr_config(stats_vdev);

		val = rkisp_read(dev, ISP35_AI_CTRL, false);
		val &= ~ISP35_AIISP_ST;
		val |= ISP35_AIPRE_GIAN2DDR_EN | ISP35_AIISP_EN |
		       ISP35_AIPRE_IIR_EN | ISP35_AIPRE_GAIN_EN |
		       ISP35_AIISP_RAW12_MSB | ISP35_AIPRE_ITS_FORCE_UPD;
		rkisp_write(dev, ISP35_AI_CTRL, val, false);
		val &= ~ISP35_AIPRE_ITS_FORCE_UPD;
		rkisp_write(dev, ISP35_AI_CTRL, val, false);

		params_vdev->cur_fe_frame_id = params_vdev->cur_frame_id;
		if (IS_HDR_RDBK(dev->rd_mode)) {
			dev->is_first_frame = true;
			dev->irq_ends_mask = ISP_FRAME_BNR | ISP_FRAME_VPSL;
		}
	}
}

static void
rkisp_params_isr_v35(struct rkisp_isp_params_vdev *params_vdev, u32 isp_mis)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct isp35_isp_params_cfg *params_rec = params_vdev->isp35_params;
	struct rkisp_device *dev = params_vdev->dev;
	u32 i, val, reg;

	if (isp_mis & CIF_ISP_V_START) {
		if (params_vdev->rdbk_times)
			params_vdev->rdbk_times--;

		if (!params_vdev->rdbk_times) {
			if (!dev->is_aiisp_en && !dev->is_aiisp_stop && priv->bay3d_iir_cnt > 1) {
				priv->pbuf_bay3d_iir = &priv->buf_bay3d_iir[priv->bay3d_iir_idx];
				priv->bay3d_iir_cur_idx = priv->bay3d_iir_idx;
				i = (priv->bay3d_iir_idx + 1) % priv->bay3d_iir_cnt;
				priv->bay3d_iir_idx = i;
				for (i = 0; i < dev->unite_div; i++) {
					if (priv->bay3d_iir_rw_fmt == 3)
						reg = ISP35_B3DLDC_WR_ADDR;
					else
						reg = ISP3X_MI_BAY3D_IIR_WR_BASE;
					val = isp3_param_read_cache(params_vdev, reg, i);
					isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_IIR_RD_BASE, i);

					val = priv->buf_bay3d_iir[priv->bay3d_iir_idx].dma_addr;
					val += i * priv->bay3d_iir_size;
					if (priv->bay3d_iir_rw_fmt == 3) {
						isp3_param_write(params_vdev, val, ISP35_B3DLDC_WR_ADDR, i);
						val += priv->bay3d_iir_offs;
					}
					isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_IIR_WR_BASE, i);
				}
			}
			if (priv->bay3d_ds_cnt > 1) {
				priv->bay3d_ds_cur_idx = priv->bay3d_ds_idx;
				i = (priv->bay3d_ds_idx + 1) % priv->bay3d_ds_cnt;
				priv->bay3d_ds_idx = i;
				for (i = 0; i < dev->unite_div; i++) {
					val = isp3_param_read_cache(params_vdev, ISP3X_MI_BAY3D_DS_WR_BASE, i);
					isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_DS_RD_BASE, i);

					val = priv->buf_bay3d_ds[priv->bay3d_ds_idx].dma_addr;
					val += i * priv->bay3d_ds_size;
					isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_DS_WR_BASE, i);
				}
			}
			if (priv->bay3d_wgt_cnt > 1) {
				priv->bay3d_wgt_cur_idx = priv->bay3d_wgt_idx;
				i = (priv->bay3d_wgt_idx + 1) % priv->bay3d_wgt_cnt;
				priv->bay3d_wgt_idx = i;
				for (i = 0; i < dev->unite_div; i++) {
					val = isp3_param_read_cache(params_vdev, ISP3X_MI_BAY3D_CUR_WR_BASE, i);
					isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_CUR_RD_BASE, i);

					val = priv->buf_bay3d_wgt[priv->bay3d_wgt_idx].dma_addr;
					val += i * priv->bay3d_wgt_size;
					isp3_param_write(params_vdev, val, ISP3X_MI_BAY3D_CUR_WR_BASE, i);
				}
			}
			if (!dev->is_aiisp_en && !dev->is_aiisp_stop && priv->aipre_gain_cnt > 1) {
				priv->pbuf_aipre_gain = &priv->buf_aipre_gain[priv->aipre_gain_idx];
				priv->aipre_gain_cur_idx = priv->aipre_gain_idx;
				i = (priv->aipre_gain_idx + 1) % priv->aipre_gain_cnt;
				priv->aipre_gain_idx = i;

				val = priv->buf_aipre_gain[i].dma_addr;
				isp3_param_write(params_vdev, val, ISP35_AI_PRE_GAIN_WR_BASE, 0);
			}
			if (!dev->is_aiisp_en && !dev->is_aiisp_stop && priv->vpsl_cnt > 1) {
				priv->pbuf_vpsl = &priv->buf_vpsl[priv->vpsl_idx];
				priv->vpsl_cur_idx = priv->vpsl_idx;
				i = (priv->vpsl_idx + 1) % priv->vpsl_cnt;
				vpsl_update_buf(params_vdev, &priv->buf_vpsl[i], false);
			}
			for (i = 0; i < dev->unite_div; i++) {
				if (params_rec->module_cfg_update & ISP35_MODULE_HDRMGE &&
				    (dev->is_aiisp_en || IS_HDR_RDBK(dev->rd_mode))) {
					isp_hdrmge_config(params_vdev, &params_rec->others.hdrmge_cfg, RKISP_PARAMS_SHD, i);
					params_rec->module_cfg_update &= ~ISP35_MODULE_HDRMGE;
				}
				if (params_rec->module_cfg_update & ISP35_MODULE_DRC &&
				    (!dev->is_aiisp_en && IS_HDR_RDBK(dev->rd_mode))) {
					isp_hdrdrc_config(params_vdev, &params_rec->others.drc_cfg, RKISP_PARAMS_SHD, i);
					params_rec->module_cfg_update &= ~ISP35_MODULE_DRC;
				}
				params_rec++;
			}
		}
	}

	if ((isp_mis & CIF_ISP_FRAME) && !params_vdev->rdbk_times)
		rkisp_params_clear_fstflg(params_vdev);

	rkisp_dmarx_get_frame(dev, &i, NULL, NULL, true);
	if (isp_mis & ISP3X_BAY3D_FRM_END && dev->is_aiisp_en) {
		rkisp_params_aiisp_update_buf(params_vdev);
		if (!IS_HDR_RDBK(dev->rd_mode))
			rkisp_params_cfg_v35(params_vdev, i + 1, RKISP_PARAMS_IMD);
	} else if (isp_mis & CIF_ISP_FRAME && !IS_HDR_RDBK(dev->rd_mode) &&
		   !params_vdev->rdbk_times && !dev->is_aiisp_en) {
		rkisp_params_cfg_v35(params_vdev, i + 1, RKISP_PARAMS_ALL);
	}

}

void rkisp_params_vpsl_mi_isr_v35(struct rkisp_isp_params_vdev *params_vdev, u32 mis_val)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct rkisp_device *dev = params_vdev->dev;
	unsigned long lock_flags = 0;

	if (!dev->is_aiisp_en)
		return;
	spin_lock_irqsave(&priv->buf_lock, lock_flags);
	if (mis_val & VPSL_MI_YRAW_ALL_END) {
		priv->pbuf_vpsl = NULL;
		if (!list_empty(&priv->vpsl_list)) {
			priv->pbuf_vpsl = list_first_entry(&priv->vpsl_list,
						struct rkisp_dummy_buffer, queue);
			list_del(&priv->pbuf_vpsl->queue);
			vpsl_update_buf(params_vdev, priv->pbuf_vpsl, true);
			priv->vpsl_cur_idx = priv->vpsl_idx;
		}
		if (dev->is_aiisp_sync)
			rkisp_check_idle(dev, ISP_FRAME_VPSL);
	}
	spin_unlock_irqrestore(&priv->buf_lock, lock_flags);
}

static struct rkisp_isp_params_ops rkisp_isp_params_ops_tbl = {
	.save_first_param = rkisp_save_first_param_v35,
	.clear_first_param = rkisp_clear_first_param_v35,
	.get_param_size = rkisp_get_param_size_v35,
	.first_cfg = rkisp_params_first_cfg_v35,
	.disable_isp = rkisp_params_disable_isp_v35,
	.isr_hdl = rkisp_params_isr_v35,
	.param_cfg = rkisp_params_cfg_v35,
	.param_cfgsram = rkisp_params_cfgsram_v35,
	.get_meshbuf_inf = rkisp_params_get_meshbuf_inf_v35,
	.set_meshbuf_size = rkisp_params_set_meshbuf_size_v35,
	.free_meshbuf = rkisp_params_free_meshbuf_v35,
	.stream_stop = rkisp_params_stream_stop_v35,
	.fop_release = rkisp_params_fop_release_v35,
	.check_bigmode = rkisp_params_check_bigmode_v35,
	.info2ddr_cfg = rkisp_params_info2ddr_cfg_v35,
	.get_aiawb_buffd = rkisp_params_get_aiawb_buffd_v35,
	.init_bnr_buf = rkisp_params_init_bnr_buf_v35,
	.aiisp_event = rkisp_params_aiisp_event_v35,
	.aiisp_start = rkisp_params_aiisp_start_v35,
	.vpsl_update_regs = rkisp_vpsl_update_regs_v35,
	.aiisp_switch = rkisp_params_aiisp_switch_v35,
};

int rkisp_init_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_isp_params_val_v35 *priv;
	int size;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	size = sizeof(struct isp35_isp_params_cfg);
	if (params_vdev->dev->hw_dev->unite)
		size *= ISP_UNITE_MAX;
	params_vdev->isp35_params = vmalloc(size);
	if (!params_vdev->isp35_params) {
		kfree(priv);
		return -ENOMEM;
	}

	spin_lock_init(&priv->buf_lock);
	params_vdev->priv_val = priv;
	params_vdev->ops = &rkisp_isp_params_ops_tbl;
	rkisp_clear_first_param_v35(params_vdev);
	priv->buf_info_owner = 0;
	priv->buf_info_cnt = 0;
	priv->buf_info_idx = -1;
	return 0;
}

void rkisp_uninit_params_vdev_v35(struct rkisp_isp_params_vdev *params_vdev)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;

	if (params_vdev->isp35_params)
		vfree(params_vdev->isp35_params);
	kfree(priv);
	params_vdev->priv_val = NULL;
}

#if IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_ISP_VERSION_V35_DBG)
static void rkisp_get_params_rawaf(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp35_isp_params_cfg *params)
{
	struct isp35_rawaf_meas_cfg *arg = &params->meas.rawaf;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_RAWAF_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_RAWAF;
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
	arg->bnr_be_sel = !!(val & BIT(20));
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

	for (i = 0; i < ISP35_RAWAF_GAMMA_NUM / 2; i++) {
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

	for (i = 0; i < ISP35_RAWAF_VFIR_COE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP32_RAWAF_V_FIR_COE0 + i * 4, 0);
		arg->v1fir_coe[i] = val & 0xfff;
		arg->v2fir_coe[i] = (val >> 16) & 0xfff;
	}

	for (i = 0; i < ISP35_RAWAF_GAUS_COE_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP32_RAWAF_GAUS_COE03 + i * 4, 0);
		arg->gaus_coe[i * 4] = val & 0xff;
		arg->gaus_coe[i * 4 + 1] = (val >> 8) & 0xff;
		arg->gaus_coe[i * 4 + 2] = (val >> 16) & 0xff;
		arg->gaus_coe[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP32_RAWAF_GAUS_COE8, 0);
	arg->gaus_coe[ISP35_RAWAF_GAUS_COE_NUM - 1] = val & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_RAWAF_HIGHLIT_THRESH, 0);
	arg->highlit_thresh = val & 0x3ff;

	val = isp3_param_read(params_vdev, ISP32L_RAWAF_CORING_H, 0);
	arg->h_fv_limit = val & 0x3ff;
	arg->h_fv_slope = (val >> 16) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP32L_RAWAF_CORING_V, 0);
	arg->v_fv_limit = val & 0x3ff;
	arg->v_fv_slope = (val >> 16) & 0x1ff;

	for (i = 0; i < ISP35_RAWAF_HIIR_COE_NUM / 2; i++) {
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

	for (i = 0; i < ISP35_RAWAF_VIIR_COE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_RAWAF_V_IIR_COE0 + i * 4, 0);
		arg->v1iir_coe[i] = val & 0xfff;
		arg->v2iir_coe[i] = (val >> 16) & 0xfff;
	}

	for (i = 0; i < ISP35_RAWAF_CURVE_NUM; i++) {
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
				    struct isp35_isp_params_cfg *params)
{
	struct isp35_rawawb_meas_cfg *arg = &params->meas.rawawb;
	struct isp35_rawawb_meas_cfg *arg_rec = &params_vdev->isp35_params->meas.rawawb;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_RAWAWB_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_RAWAWB;
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
	arg->ovexp_2ddr_dis = !!(val & BIT(9));
	arg->bnr_be_sel = !!(val & BIT(10));
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

	val = isp3_param_read(params_vdev, ISP33_RAWAWB_CCM_COEFF0_R, 0);
	arg->ccm_coeff0_r = val & 0xffff;
	arg->ccm_coeff1_r = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_RAWAWB_CCM_COEFF1_R, 0);
	arg->ccm_coeff2_r = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_RAWAWB_CCM_COEFF0_G, 0);
	arg->ccm_coeff0_g = val & 0xffff;
	arg->ccm_coeff1_g = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_RAWAWB_CCM_COEFF1_G, 0);
	arg->ccm_coeff2_g = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_RAWAWB_CCM_COEFF0_B, 0);
	arg->ccm_coeff0_b = val & 0xffff;
	arg->ccm_coeff1_b = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_RAWAWB_CCM_COEFF1_B, 0);
	arg->ccm_coeff2_b = val & 0xffff;

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
				    struct isp35_isp_params_cfg *params)
{
	struct isp35_rawae_meas_cfg *arg = &params->meas.rawae0;
	const u32 ae_wnd_num[] = {1, 5, 15, 15};
	u32 addr = ISP3X_RAWAE_LITE_BASE, val;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_RAWAE0;
	arg->wnd_num = (val >> 1) & 0x3;
	arg->wnd1_en = !!(val & BIT(4));
	arg->debug_en = !!(val & BIT(8));
	arg->bnr_be_sel = !!(val & BIT(9));

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_OFFSET, 0);
	arg->win0_h_offset = val & 0x1fff;
	arg->win0_v_offset = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_BLK_SIZE, 0);
	arg->win0_h_size = (val & 0x1fff) * ae_wnd_num[arg->wnd_num];
	arg->win0_v_size = ((val >> 16) & 0x1fff) * ae_wnd_num[arg->wnd_num];

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_WND1_OFFSET, 0);
	arg->win1_h_offset = val & 0x1fff;
	arg->win1_v_offset = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_WND1_SIZE, 0);
	arg->win1_h_offset = (val & 0x1fff) - arg->win1_h_offset;
	arg->win1_v_offset = ((val >> 16) & 0x1fff) - arg->win1_v_offset;

	val = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, 0);
	arg->rawae_sel = (val >> 22) & 0x3;
	arg->bnr2ae_sel = !!(val & BIT(30));
}

static void rkisp_get_params_rawae3(struct rkisp_isp_params_vdev *params_vdev,
				    struct isp35_isp_params_cfg *params)
{
	struct isp35_rawae_meas_cfg *arg = &params->meas.rawae3;
	const u32 ae_wnd_num[] = {1, 5, 15, 15};
	u32 addr = ISP3X_RAWAE_BIG1_BASE, val;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_RAWAE3;
	arg->wnd_num = (val >> 1) & 0x3;
	arg->wnd1_en = !!(val & BIT(4));
	arg->debug_en = !!(val & BIT(8));
	arg->bnr_be_sel = !!(val & BIT(9));

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_OFFSET, 0);
	arg->win0_h_offset = val & 0x1fff;
	arg->win0_v_offset = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_BLK_SIZE, 0);
	arg->win0_h_size = (val & 0x1fff) * ae_wnd_num[arg->wnd_num];
	arg->win0_v_size = ((val >> 16) & 0x1fff) * ae_wnd_num[arg->wnd_num];

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_WND1_OFFSET, 0);
	arg->win1_h_offset = val & 0x1fff;
	arg->win1_v_offset = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWAE_BIG_WND1_SIZE, 0);
	arg->win1_h_size = (val & 0x1fff) - arg->win1_h_offset;
	arg->win1_v_size = ((val >> 16) & 0x1fff) - arg->win1_v_offset;

	val = isp3_param_read(params_vdev, ISP3X_VI_ISP_PATH, 0);
	arg->rawae_sel = (val >> 16) & 0x3;
	arg->bnr2ae_sel = !!(val & BIT(29));
}

static void rkisp_get_params_rawhist0(struct rkisp_isp_params_vdev *params_vdev,
				      struct isp35_isp_params_cfg *params)
{
	struct isp35_rawhist_meas_cfg *arg = &params->meas.rawhist0;
	struct isp35_rawhist_meas_cfg *arg_rec = &params_vdev->isp35_params->meas.rawhist0;
	const u32 hist_wnd_num[] = {5, 5, 15, 15};
	u32 addr = ISP3X_RAWHIST_LITE_BASE, val;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_RAWHIST0;
	arg->stepsize = (val >> 1) & 0x7;
	arg->debug_en = !!(val & BIT(7));
	arg->mode = (val >> 8) & 0x7;
	arg->waterline = (val >> 12) & 0xfff;
	arg->data_sel = (val >> 24) & 0x7;
	arg->wnd_num = (val >> 28) & 0x3;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_OFFS, 0);
	arg->h_offset = val & 0x1fff;
	arg->v_offset = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_SIZE, 0);
	arg->h_size = (val & 0x1fff) * hist_wnd_num[arg->wnd_num];
	arg->v_size = ((val >> 16) & 0x1fff) * hist_wnd_num[arg->wnd_num];

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_RAW2Y_CC, 0);
	arg->rcc = val & 0xff;
	arg->gcc = (val >> 8) & 0xff;
	arg->bcc = (val >> 16) & 0xff;
	arg->off = (val >> 24) & 0xff;

	memcpy(arg->weight, arg_rec->weight, ISP35_RAWHIST_WEIGHT_NUM);
}

static void rkisp_get_params_rawhist3(struct rkisp_isp_params_vdev *params_vdev,
				      struct isp35_isp_params_cfg *params)
{
	struct isp35_rawhist_meas_cfg *arg = &params->meas.rawhist3;
	struct isp35_rawhist_meas_cfg *arg_rec = &params_vdev->isp35_params->meas.rawhist3;
	const u32 hist_wnd_num[] = {5, 5, 15, 15};
	u32 addr = ISP3X_RAWHIST_BIG1_BASE, val;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_RAWHIST3;
	arg->stepsize = (val >> 1) & 0x7;
	arg->debug_en = !!(val & BIT(7));
	arg->mode = (val >> 8) & 0x7;
	arg->waterline = (val >> 12) & 0xfff;
	arg->data_sel = (val >> 24) & 0x7;
	arg->wnd_num = (val >> 28) & 0x3;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_OFFS, 0);
	arg->h_offset = val & 0x1fff;
	arg->v_offset = (val >> 16) & 0x1fff;

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_SIZE, 0);
	arg->h_size = (val & 0x1fff) * hist_wnd_num[arg->wnd_num];
	arg->v_size = ((val >> 16) & 0x1fff) * hist_wnd_num[arg->wnd_num];

	val = isp3_param_read(params_vdev, addr + ISP3X_RAWHIST_BIG_RAW2Y_CC, 0);
	arg->rcc = val & 0xff;
	arg->gcc = (val >> 8) & 0xff;
	arg->bcc = (val >> 16) & 0xff;
	arg->off = (val >> 24) & 0xff;

	memcpy(arg->weight, arg_rec->weight, ISP35_RAWHIST_WEIGHT_NUM);
}

static void rkisp_get_params_bls(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp35_bls_cfg *arg = &params->others.bls_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_BLS_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_BLS;
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
	arg->isp_ob_offset1 = (val >> 16) & 0x1ff;
	val = isp3_param_read(params_vdev, ISP32_BLS_ISP_OB_PREDGAIN, 0);
	arg->isp_ob_predgain = val & 0xffff;
	val = isp3_param_read(params_vdev, ISP32_BLS_ISP_OB_MAX, 0);
	arg->isp_ob_max = val & 0xfffff;
}

static void rkisp_get_params_dpcc(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp35_isp_params_cfg *params)
{
	struct isp39_dpcc_cfg *arg = &params->others.dpcc_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_DPCC0_MODE, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_DPCC;
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
	for (i = 0; i < ISP35_DPCC_PDAF_POINT_NUM; i++)
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

	for (i = 0; i < ISP35_DPCC_PDAF_POINT_NUM / 2; i++) {
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
				 struct isp35_isp_params_cfg *params)
{
	struct isp3x_lsc_cfg *arg = &params->others.lsc_cfg;
	struct isp3x_lsc_cfg *arg_rec = &params_vdev->isp35_params->others.lsc_cfg;
	u32 val, i;

	val = isp3_param_read(params_vdev, ISP3X_LSC_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_LSC;
	arg->sector_16x16 = !!(val & BIT(2));

	for (i = 0; i < ISP35_LSC_SIZE_TBL_SIZE / 4; i++) {
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
				     struct isp35_isp_params_cfg *params)
{
	struct isp32_awb_gain_cfg *arg = &params->others.awb_gain_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_ISP_CTRL0, 0);
	if (!(val & CIF_ISP_CTRL_ISP_AWB_ENA))
		return;
	params->module_ens |= ISP35_MODULE_AWB_GAIN;

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
				 struct isp35_isp_params_cfg *params)
{
	struct isp33_gic_cfg *arg = &params->others.gic_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_GIC_CONTROL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_GIC;
	arg->bypass_en = !!(val & BIT(1));
	arg->pro_mode = !!(val & BIT(2));
	arg->manualnoisecurve_en = !!(val & BIT(3));
	arg->manualnoisethred_en = !!(val & BIT(4));
	arg->gain_bypass_en = !!(val & BIT(5));

	val = isp3_param_read(params_vdev, ISP33_GIC_MEDFLT_PARA, 0);
	arg->medflt_minthred = val & 0xf;
	arg->medflt_maxthred = (val >> 4) & 0xf;
	arg->medflt_ratio = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_GIC_MEDFLTUV_PARA, 0);
	arg->medfltuv_minthred = val & 0xf;
	arg->medfltuv_maxthred = (val >> 4) & 0xf;
	arg->medfltuv_ratio = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_GIC_NOISE_SCALE, 0);
	arg->noisecurve_scale = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_GIC_BILAT_PARA1, 0);
	arg->bffltwgt_offset = val & 0xffff;
	arg->bffltwgt_scale = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_GIC_BILAT_PARA2, 0);
	arg->bfflt_ratio = val & 0xff;

	val = isp3_param_read(params_vdev, ISP33_GIC_DISWGT_COEFF, 0);
	arg->bfflt_coeff0 = val & 0xff;
	arg->bfflt_coeff1 = (val >> 8) & 0xff;
	arg->bfflt_coeff2 = (val >> 16) & 0xff;

	for (i = 0; i < ISP35_GIC_SIGMA_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_GIC_SIGMA_Y0 + 4 * i, 0);
		arg->bfflt_vsigma_y[2 * i] = val & 0xffff;
		arg->bfflt_vsigma_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP33_GIC_SIGMA_Y8, 0);
	arg->bfflt_vsigma_y[2 * i] = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_GIC_LUMA_DX, 0);
	for (i = 0; i < ISP35_GIC_LUMA_DX_NUM; i++)
		arg->luma_dx[i] = (val >> (i * 4)) & 0xf;

	for (i = 0; i < ISP35_GIC_THRED_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_GIC_THRED_Y0 + i * 4, 0);
		arg->thred_y[i * 2] = val & 0xffff;
		arg->thred_y[i * 2 + 1] = (val >> 16) & 0xffff;

		val = isp3_param_read(params_vdev, ISP33_GIC_MIN_THRED_Y0 + i * 4, 0);
		arg->minthred_y[i * 2] = val & 0xffff;
		arg->minthred_y[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP33_GIC_THRED_SCALE, 0);
	arg->autonoisethred_scale = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_GIC_LOFLTGR_COEFF, 0);
	arg->lofltgr_coeff0 = val & 0xff;
	arg->lofltgr_coeff1 = (val >> 8) & 0xff;
	arg->lofltgr_coeff2 = (val >> 16) & 0xff;
	arg->lofltgr_coeff3 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_GIC_LOFLTGB_COEFF, 0);
	arg->lofltgb_coeff0 = val & 0xff;
	arg->lofltgb_coeff1 = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_GIC_SUM_LOFLT_INV, 0);
	arg->sumlofltcoeff_inv = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_GIC_LOFLTTHRED_COEFF, 0);
	arg->lofltthred_coeff0 = val & 0xff;
	arg->lofltthred_coeff1 = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_GIC_GAIN, 0);
	arg->global_gain = val & 0x3ff;
	arg->globalgain_alpha = (val >> 12) & 0xf;
	arg->globalgain_scale = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_GIC_GAIN_SLOPE, 0);
	arg->gain_offset = val & 0xffff;
	arg->gain_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_GIC_GAIN_THRED, 0);
	arg->gainadjflt_minthred = val & 0xffff;
	arg->gainadjflt_maxthred = (val >> 16) & 0xffff;
}

static void rkisp_get_params_debayer(struct rkisp_isp_params_vdev *params_vdev,
				     struct isp35_isp_params_cfg *params)
{
	struct isp35_debayer_cfg *arg = &params->others.debayer_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_DEBAYER_CONTROL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_DEBAYER;
	arg->bypass = !!(val & BIT(1));
	arg->g_out_flt_en = !!(val & BIT(4));
	arg->cnt_flt_en = !!(val & BIT(8));

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_LUMA_DX, 0);
	arg->luma_dx[0] = val & 0xf;
	arg->luma_dx[1] = (val >> 4) & 0xf;
	arg->luma_dx[2] = (val >> 8) & 0xf;
	arg->luma_dx[3] = (val >> 12) & 0xf;
	arg->luma_dx[4] = (val >> 16) & 0xf;
	arg->luma_dx[5] = (val >> 20) & 0xf;
	arg->luma_dx[6] = (val >> 24) & 0xf;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP, 0);
	arg->g_interp_clip_en = !!(val & BIT(0));
	arg->hi_texture_thred = (val >> 4) & 0xf;
	arg->hi_drct_thred = (val >> 8) & 0xf;
	arg->lo_drct_thred = (val >> 12) & 0xf;
	arg->drct_method_thred = (val >> 16) & 0xff;
	arg->g_interp_sharp_strg_max_limit = (val >> 24) & 0x3f;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP_FILTER1, 0);
	arg->lo_drct_flt_coeff1 = val & 0x1f;
	arg->lo_drct_flt_coeff2 = (val >> 8) & 0x1f;
	arg->lo_drct_flt_coeff3 = (val >> 16) & 0x1f;
	arg->lo_drct_flt_coeff4 = (val >> 24) & 0x1f;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP_FILTER2, 0);
	arg->hi_drct_flt_coeff1 = val & 0x1f;
	arg->hi_drct_flt_coeff2 = (val >> 8) & 0x1f;
	arg->hi_drct_flt_coeff3 = (val >> 16) & 0x1f;
	arg->hi_drct_flt_coeff4 = (val >> 24) & 0x1f;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP_OFFSET_ALPHA, 0);
	arg->g_interp_sharp_strg_offset = val & 0xfff;
	arg->grad_lo_flt_alpha = (val >> 16) & 0x7f;

	for (i = 0; i < ISP35_DEBAYER_DRCT_OFFSET_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_INTERP_DRCT_OFFSET0 + i * 4, 0);
		arg->drct_offset[i * 2] = val & 0xffff;
		arg->drct_offset[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_FILTER_MODE_OFFSET, 0);
	arg->gflt_mode = !!(val & BIT(0));
	arg->gflt_ratio = (val >> 4) & 0x7ff;
	arg->gflt_offset = (val >> 16) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_FILTER_FILTER, 0);
	arg->gflt_coe0 = val & 0xff;
	arg->gflt_coe1 = (val >> 8) & 0xff;
	arg->gflt_coe2 = (val >> 16) & 0xff;

	for (i = 0; i < ISP35_DEBAYER_VSIGMA_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_DEBAYER_G_FILTER_VSIGMA0 + i * 4, 0);
		arg->gflt_vsigma[i * 2] = val & 0xffff;
		arg->gflt_vsigma[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_GUIDE_GAUS, 0);
	arg->cnr_lo_guide_lpf_coe0 = val & 0xff;
	arg->cnr_lo_guide_lpf_coe1 = (val >> 8) & 0xff;
	arg->cnr_lo_guide_lpf_coe2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_CE_GAUS, 0);
	arg->cnr_pre_flt_coe0 = val & 0xff;
	arg->cnr_pre_flt_coe1 = (val >> 8) & 0xff;
	arg->cnr_pre_flt_coe2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_ALPHA_GAUS, 0);
	arg->cnr_alpha_lpf_coe0 = val & 0xff;
	arg->cnr_alpha_lpf_coe1 = (val >> 8) & 0xff;
	arg->cnr_alpha_lpf_coe2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_LOG_OFFSET, 0);
	arg->cnr_log_grad_offset = val & 0x1fff;
	arg->cnr_log_guide_offset = (val >> 16) & 0xfff;
	arg->cnr_trans_en = !!(val & BIT(31));

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_ALPHA, 0);
	arg->cnr_moire_alpha_offset = val & 0xfff;
	arg->cnr_moire_alpha_scale = (val >> 12) & 0xfffff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_EDGE, 0);
	arg->cnr_edge_alpha_offset = val & 0xfff;
	arg->cnr_edge_alpha_scale = (val >> 12) & 0xfffff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_IIR_0, 0);
	arg->cnr_lo_flt_strg_inv = val & 0xff;
	arg->cnr_lo_flt_strg_shift = (val >> 8) & 0x3f;
	arg->cnr_lo_flt_wgt_slope = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_IIR_1, 0);
	arg->cnr_lo_flt_wgt_max_limit = val & 0x7f;
	arg->cnr_lo_flt_wgt_min_thred = (val >> 8) & 0x3f;

	val = isp3_param_read(params_vdev, ISP39_DEBAYER_C_FILTER_BF, 0);
	arg->cnr_hi_flt_vsigma = val & 0xffff;
	arg->cnr_hi_flt_wgt_min_limit = (val >> 16) & 0x7f;
	arg->cnr_hi_flt_cur_wgt = (val >> 24) & 0x7f;
}

static void rkisp_get_params_ccm(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp33_ccm_cfg *arg = &params->others.ccm_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_CCM_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_CCM;
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

	for (i = 0; i < ISP35_CCM_CURVE_NUM / 2; i++) {
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

	for (i = 0; i < ISP35_CCM_HF_FACTOR_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_CCM_HF_FACTOR0 + i * 4, 0);
		arg->hf_factor[i * 2] = val & 0xffff;
		arg->hf_factor[i * 2 + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP33_CCM_HF_FACTOR8 + i * 4, 0);
	arg->hf_factor[i * 2] = val & 0xffff;
}

static void rkisp_get_params_gammaout(struct rkisp_isp_params_vdev *params_vdev,
				      struct isp35_isp_params_cfg *params)
{
	struct isp3x_gammaout_cfg *arg = &params->others.gammaout_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_GOC;
	arg->equ_segm = !!(val & BIT(1));
	arg->finalx4_dense_en = !!(val & BIT(2));

	val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_OFFSET, 0);
	arg->offset = val & 0xffff;

	for (i = 0; i < ISP35_GAMMA_OUT_MAX_SAMPLES / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_Y0 + i * 4, 0);
		arg->gamma_y[2 * i] = val & 0xffff;
		arg->gamma_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_GAMMA_OUT_Y0 + i * 4, 0);
	arg->gamma_y[2 * i] = val & 0xffff;
}

static void rkisp_get_params_cproc(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp35_isp_params_cfg *params)
{
	struct isp2x_cproc_cfg *arg = &params->others.cproc_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_CPROC_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_CPROC;
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

static void rkisp_get_params_drc(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp35_drc_cfg *arg = &params->others.drc_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_DRC_CTRL0, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_DRC;
	arg->bypass_en = !!(val & BIT(1));
	arg->cmps_byp_en = !!(val & BIT(2));
	arg->gainx32_en = !!(val & BIT(3));

	val = isp3_param_read(params_vdev, ISP3X_DRC_CTRL1, 0);
	arg->gain_idx_luma_scale = val & 0x3fff;
	arg->comps_idx_luma_scale = (val >> 14) & 0x1fff;
	arg->log_transform_offset_bits = (val >> 28) & 0xf;

	val = isp3_param_read(params_vdev, ISP3X_DRC_LPRATIO, 0);
	arg->lo_detail_ratio = val & 0xfff;
	arg->hi_detail_ratio = (val >> 12) & 0xfff;
	arg->adj_gain_idx_luma_scale = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT0, 0);
	arg->bifilt_wgt_offset = val & 0xff;
	arg->thumb_thred_neg = (val >> 8) & 0x1ff;
	arg->thumb_thred_en = !!(val & BIT(23));
	arg->bifilt_cur_pixel_wgt = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT1, 0);
	arg->cmps_offset_bits = val & 0xf;
	arg->cmps_mode = !!(val & BIT(4));
	arg->filt_luma_soft_thred = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT2, 0);
	arg->thumb_max_limit = val & 0xfff;
	arg->thumb_scale = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT3, 0);
	arg->hi_range_inv_sigma = val & 0x3ff;
	arg->lo_range_inv_sigma = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP39_DRC_BILAT4, 0);
	arg->bifilt_wgt = val & 0x1f;
	arg->bifilt_hi_wgt = (val >> 8) & 0xff;
	arg->bifilt_soft_thred = (val >> 16) & 0x7ff;
	arg->bifilt_soft_thred_en = !!(val & BIT(31));

	for (i = 0; i < ISP35_DRC_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_DRC_GAIN_Y0 + 4 * i, 0);
		arg->gain_y[2 * i] = val & 0xffff;
		arg->gain_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_DRC_GAIN_Y0 + 4 * i, 0);
	arg->gain_y[2 * i] = val & 0xffff;

	for (i = 0; i < ISP35_DRC_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_DRC_COMPRES_Y0 + 4 * i, 0);
		arg->compres_y[2 * i] = val & 0xffff;
		arg->compres_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_DRC_COMPRES_Y0 + 4 * i, 0);
	arg->compres_y[2 * i] = val & 0xffff;

	for (i = 0; i < ISP35_DRC_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP3X_DRC_SCALE_Y0 + 4 * i, 0);
		arg->scale_y[2 * i] = val & 0xffff;
		arg->scale_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP3X_DRC_SCALE_Y0 + 4 * i, 0);
	arg->scale_y[2 * i] = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_DRC_IIRWG_GAIN, 0);
	arg->comps_gain_min_limit = val & 0xffff;

	for (i = 0; i < ISP35_DRC_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_DRC_SFTHD_Y0 + 4 * i, 0);
		arg->sfthd_y[2 * i] = val & 0xffff;
		arg->sfthd_y[2 * i + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP39_DRC_SFTHD_Y0 + 4 * i, 0);
	arg->sfthd_y[2 * i] = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP35_DRC_LUMA_MIX, 0);
	arg->max_luma_wgt = val & 0xff;
	arg->mid_luma_wgt = (val >> 8) & 0xff;
	arg->min_luma_wgt = (val >> 16) & 0xff;
}

static void rkisp_get_params_hdrmge(struct rkisp_isp_params_vdev *params_vdev,
				    struct isp35_isp_params_cfg *params)
{
	struct isp35_hdrmge_cfg *arg = &params->others.hdrmge_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_HDRMGE;
	arg->short_base_en = !!(val & BIT(1));
	arg->frame_mode = (val >> 2) & 0x3;
	arg->dbg_mode = (val >> 4) & 0x3;
	arg->channel_detection_en = !!(val & BIT(6));
	arg->s_base_mode = !!(val & BIT(7));

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_GAIN0, 0);
	arg->short_gain = val & 0xffff;
	arg->short_inv_gain = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_GAIN1, 0);
	arg->medium_gain = val & 0xffff;
	arg->medium_inv_gain = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_GAIN2, 0);
	arg->long_gain = val & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_LIGHTZ, 0);
	arg->ms_diff_scale = val & 0xff;
	arg->ms_diff_offset = (val >> 8) & 0xff;
	arg->lm_diff_scale = (val >> 16) & 0xff;
	arg->lm_diff_offset = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_MS_DIFF, 0);
	arg->ms_abs_diff_scale = val & 0x7ff;
	arg->ms_abs_diff_thred_min_limit = (val >> 12) & 0x3ff;
	arg->ms_adb_diff_thred_max_limit = (val >> 22) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP3X_HDRMGE_LM_DIFF, 0);
	arg->lm_abs_diff_scale = val & 0x7ff;
	arg->lm_abs_diff_thred_min_limit = (val >> 12) & 0x3ff;
	arg->lm_abs_diff_thred_max_limit = (val >> 22) & 0x3ff;

	for (i = 0; i < ISP35_HDRMGE_WGT_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_HDRMGE_DIFF_Y0 + 4 * i, 0);
		arg->ms_luma_diff2wgt[i] = val & 0xffff;
		arg->lm_luma_diff2wgt[i] = (val >> 16) & 0xffff;
	}

	for (i = 0; i < ISP35_HDRMGE_WGT_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_HDRMGE_OVER_Y0 + 4 * i, 0);
		arg->luma2wgt[i] = val & 0x3ff;
		arg->ms_raw_diff2wgt[i] = (val >> 10) & 0x3ff;
		arg->lm_raw_diff2wgt[i] = (val >> 20) & 0x3ff;
	}

	val = isp3_param_read(params_vdev, ISP32_HDRMGE_EACH_GAIN, 0);
	arg->channel_detn_short_gain = val & 0xffff;
	arg->channel_detn_medium_gain = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP35_HDRMGE_FORCE_LONG0, 0);
	arg->mid_luma_scale = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP35_HDRMGE_FORCE_LONG1, 0);
	arg->mid_luma_thred_max_limit = val & 0xffff;
	arg->mid_luma_thred_min_limit = (val >> 16) & 0xffff;
}

static void rkisp_get_params_ldch(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp35_isp_params_cfg *params)
{
	struct isp32_ldch_cfg *arg = &params->others.ldch_cfg;
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_LDCH_STS, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_LDCH;
	arg->frm_end_dis = !!(val & BIT(1));
	arg->sample_avr_en = !!(val & BIT(3));
	arg->bic_mode_en = !!(val & BIT(4));
	arg->force_map_en = !!(val & BIT(6));
	arg->map13p3_en = !!(val & BIT(7));

	for (i = 0; i < ISP35_LDCH_BIC_NUM / 4; i++) {
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

	val = priv->buf_ldch_idx[0];
	arg->buf_fd = priv->buf_ldch[0][val].dma_fd;
}

static void rkisp_get_params_bay3d(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp35_isp_params_cfg *params)
{
	struct rkisp_isp_params_val_v35 *priv = params_vdev->priv_val;
	struct isp35_bay3d_cfg *arg = &params->others.bay3d_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_CTRL0, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_BAY3D;
	arg->bypass_en = !!(val & BIT(1));
	arg->iir_wr_src = !!(val & BIT(3));
	arg->out_use_pre_mode = (val >> 5) & 0x7;
	arg->motion_est_en = !!(val & BIT(8));
	arg->iir_rw_fmt = (val >> 13) & 0x3;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_CTRL1, 0);
	arg->transf_bypass_en = !!(val & BIT(0));
	arg->tnrsigma_curve_double_en = !!(val & BIT(1));
	arg->md_large_lo_use_mode = !!(val & BIT(2));
	arg->md_large_lo_min_filter_bypass_en = !!(val & BIT(3));
	arg->md_large_lo_gauss_filter_bypass_en = !!(val & BIT(4));
	arg->md_large_lo_md_wgt_bypass_en = !!(val & BIT(5));
	arg->pre_pix_out_mode = !!(val & BIT(6));
	arg->motion_detect_bypass_en = !!(val & BIT(7));
	arg->lpf_hi_bypass_en = !!(val & BIT(8));
	arg->lo_diff_vfilt_bypass_en = !!(val & BIT(9));
	arg->lpf_lo_bypass_en = !!(val & BIT(10));
	arg->lo_wgt_hfilt_en = !!(val & BIT(11));
	arg->lo_diff_hfilt_en = !!(val & BIT(12));
	arg->sig_hfilt_en = !!(val & BIT(13));
	arg->lo_detection_bypass_en = !!(val & BIT(15));
	arg->lo_mge_wgt_mode = (val >> 16) & 0x3;
	arg->pre_spnr_out_en = !!(val & BIT(20));
	arg->md_only_lo_en = !!(val & BIT(21));
	arg->cur_spnr_out_en = !!(val & BIT(22));
	arg->md_wgt_out_en = !!(val & BIT(25));

	val = isp3_param_read(params_vdev, ISP33_BAY3D_CTRL2, 0);
	arg->cur_spnr_filter_bypass_en = !!(val & BIT(0));
	arg->pre_spnr_hi_filter_gic_en = !!(val & BIT(2));
	arg->pre_spnr_hi_filter_gic_enhance_en = !!(val & BIT(3));
	arg->spnr_presigma_use_en = !!(val & BIT(4));
	arg->pre_spnr_lo_filter_bypass_en = !!(val & BIT(5));
	arg->pre_spnr_hi_filter_bypass_en = !!(val & BIT(6));
	arg->pre_spnr_sigma_curve_double_en = !!(val & BIT(7));
	arg->pre_spnr_hi_guide_filter_bypass_en = !!(val & BIT(8));
	arg->pre_spnr_sigma_idx_filt_bypass_en = !!(val & BIT(9));
	arg->pre_spnr_sigma_idx_filt_mode = !!(val & BIT(10));
	arg->pre_spnr_hi_noise_ctrl_en = !!(val & BIT(11));
	arg->pre_spnr_hi_filter_wgt_mode = !!(val & BIT(12));
	arg->pre_spnr_lo_filter_wgt_mode = !!(val & BIT(13));
	arg->pre_spnr_hi_filter_rb_wgt_mode = !!(val & BIT(14));
	arg->pre_spnr_lo_filter_rb_wgt_mode = !!(val & BIT(15));
	arg->pre_hi_gic_lp_en = !!(val & BIT(20));
	arg->pre_hi_bf_lp_en = !!(val & BIT(21));
	arg->pre_lo_avg_lp_en = !!(val & BIT(22));
	arg->pre_spnr_dpc_flt_en = !!(val & BIT(23));
	arg->pre_spnr_dpc_nr_bal_mode = !!(val & BIT(24));
	arg->pre_spnr_dpc_flt_mode = !!(val & BIT(25));
	arg->pre_spnr_dpc_flt_prewgt_en = !!(val & BIT(26));

	val = isp3_param_read(params_vdev, ISP33_BAY3D_CTRL3, 0);
	arg->transf_mode = !!(val & BIT(0));
	arg->wgt_cal_mode = !!(val & BIT(1));
	arg->mge_wgt_ds_mode = !!(val & BIT(2));
	arg->kalman_wgt_ds_mode = (val >> 3) & 0x3;
	arg->mge_wgt_hdr_sht_thred = (val >> 16) & 0x3f;
	arg->sigma_calc_mge_wgt_hdr_sht_thred = (val >> 24) & 0x3f;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_TRANS0, 0);
	arg->transf_mode_offset = val & 0x1fff;
	arg->transf_mode_scale = (val >> 14) & 0x3;
	arg->itransf_mode_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_TRANS1, 0);
	arg->transf_data_max_limit = val;

	val = isp3_param_read(params_vdev, ISP35_BAY3D_PREHI_SIGSCL, 0);
	arg->pre_spnr_sigma_ctrl_scale = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP35_BAY3D_PREHI_SIGOF, 0);
	arg->pre_spnr_hi_guide_out_wgt = val & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_CURHISPW0, 0);
	arg->cur_spnr_filter_coeff[0] = val & 0xff;
	arg->cur_spnr_filter_coeff[1] = (val >> 8) & 0xff;
	arg->cur_spnr_filter_coeff[2] = (val >> 16) & 0xff;
	val = isp3_param_read(params_vdev, ISP33_BAY3D_CURHISPW1, 0);
	arg->cur_spnr_filter_coeff[3] = val & 0xff;
	arg->cur_spnr_filter_coeff[4] = (val >> 8) & 0xff;
	arg->cur_spnr_filter_coeff[5] = (val >> 16) & 0xff;

	for (i = 0; i < ISP35_BAY3D_XY_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_BAY3D_IIRSX0 + i * 4, 0);
		arg->pre_spnr_luma2sigma_x[i * 2] = val & 0xffff;
		arg->pre_spnr_luma2sigma_x[i * 2 + 1] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP33_BAY3D_IIRSY0 + i * 4, 0);
		arg->pre_spnr_luma2sigma_y[i * 2] = val & 0xffff;
		arg->pre_spnr_luma2sigma_y[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PREHI_SIGSCL, 0);
	arg->pre_spnr_hi_sigma_scale = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PREHI_WSCL, 0);
	arg->pre_spnr_hi_wgt_calc_scale = val & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PREHIWMM, 0);
	arg->pre_spnr_hi_filter_wgt_min_limit = val & 0xff;
	arg->pre_spnr_hi_wgt_calc_offset = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PREHISIGOF, 0);
	arg->pre_spnr_hi_filter_out_wgt = val & 0xff;
	arg->pre_spnr_sigma_offset = (val >> 16) & 0xff;
	arg->pre_spnr_sigma_hdr_sht_offset = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PREHISIGSCL, 0);
	arg->pre_spnr_sigma_scale = val & 0xffff;
	arg->pre_spnr_sigma_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PREHISPW0, 0);
	arg->pre_spnr_hi_filter_coeff[0] = val & 0xff;
	arg->pre_spnr_hi_filter_coeff[1] = (val >> 8) & 0xff;
	arg->pre_spnr_hi_filter_coeff[2] = (val >> 16) & 0xff;
	val = isp3_param_read(params_vdev, ISP33_BAY3D_PREHISPW1, 0);
	arg->pre_spnr_hi_filter_coeff[3] = val & 0xff;
	arg->pre_spnr_hi_filter_coeff[4] = (val >> 8) & 0xff;
	arg->pre_spnr_hi_filter_coeff[5] = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PRELOSIGCSL, 0);
	arg->pre_spnr_lo_sigma_scale = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PRELOSIGOF, 0);
	arg->pre_spnr_lo_wgt_calc_offset = val & 0xff;
	arg->pre_spnr_lo_wgt_calc_scale = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PREHI_NRCT, 0);
	arg->pre_spnr_hi_noise_ctrl_scale = val & 0xffff;
	arg->pre_spnr_hi_noise_ctrl_offset = (val >> 16) & 0xff;

	for (i = 0; i < ISP35_BAY3D_TNRSIG_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_BAY3D_TNRSX0 + i * 4, 0);
		arg->tnr_luma2sigma_x[i * 2] = val & 0xffff;
		arg->tnr_luma2sigma_x[i * 2 + 1] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP33_BAY3D_TNRSY0 + i * 4, 0);
		arg->tnr_luma2sigma_y[i * 2] = val & 0xffff;
		arg->tnr_luma2sigma_y[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	for (i = 0; i < ISP35_BAY3D_LPF_COEFF_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP33_BAY3D_HIWD0 + i * 4, 0);
		arg->lpf_hi_coeff[i * 3] = val & 0x3ff;
		arg->lpf_hi_coeff[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->lpf_hi_coeff[i * 3 + 2] = (val >> 20) & 0x3ff;
		val = isp3_param_read(params_vdev, ISP33_BAY3D_LOWD0 + i * 4, 0);
		arg->lpf_lo_coeff[i * 3] = val & 0x3ff;
		arg->lpf_lo_coeff[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->lpf_lo_coeff[i * 3 + 2] = (val >> 20) & 0x3ff;
	}

	val = isp3_param_read(params_vdev, ISP33_BAY3D_GF3, 0);
	arg->sigma_idx_filt_coeff[0] = val & 0xff;
	arg->sigma_idx_filt_coeff[1] = (val >> 8) & 0xff;
	arg->sigma_idx_filt_coeff[2] = (val >> 16) & 0xff;
	arg->sigma_idx_filt_coeff[3] = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_GF4, 0);
	arg->sigma_idx_filt_coeff[4] = val & 0xff;
	arg->sigma_idx_filt_coeff[5] = (val >> 8) & 0xff;
	arg->lo_wgt_cal_first_line_sigma_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_VIIR, 0);
	arg->lo_diff_vfilt_wgt = val & 0x1f;
	arg->lo_wgt_vfilt_wgt = (val >> 5) & 0x1f;
	arg->sig_first_line_scale = (val >> 16) & 0x3f;
	arg->lo_diff_first_line_scale = (val >> 22) & 0x3f;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_LFSCL, 0);
	arg->lo_wgt_cal_offset = val & 0xffff;
	arg->lo_wgt_cal_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_LFSCLTH, 0);
	arg->lo_wgt_cal_max_limit = val & 0xffff;
	arg->mode0_base_ratio = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_DSWGTSCL, 0);
	arg->lo_diff_wgt_cal_offset = val & 0xffff;
	arg->lo_diff_wgt_cal_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTLASTSCL, 0);
	arg->lo_mge_pre_wgt_offset = val & 0xffff;
	arg->lo_mge_pre_wgt_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTSCL0, 0);
	arg->mode0_lo_wgt_scale = val & 0xffff;
	arg->mode0_lo_wgt_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTSCL1, 0);
	arg->mode1_lo_wgt_scale = val & 0xffff;
	arg->mode1_lo_wgt_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTSCL2, 0);
	arg->mode1_wgt_scale = val & 0xffff;
	arg->mode1_wgt_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTOFF, 0);
	arg->mode1_lo_wgt_offset = val & 0xffff;
	arg->mode1_lo_wgt_hdr_sht_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGT1OFF, 0);
	arg->auto_sigma_count_wgt_thred = val & 0x3ff;
	arg->mode1_wgt_min_limit = (val >> 10) & 0x3ff;
	arg->mode1_wgt_offset = (val >> 20) & 0xfff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_SIGORG, 0);
	arg->tnr_out_sigma_sq = val;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTLO_L, 0);
	arg->lo_wgt_clip_min_limit = val & 0xffff;
	arg->lo_wgt_clip_hdr_sht_min_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTLO_H, 0);
	arg->lo_wgt_clip_max_limit = val & 0xffff;
	arg->lo_wgt_clip_hdr_sht_max_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_STH_SCL, 0);
	arg->lo_pre_gg_soft_thresh_scale = val & 0xffff;
	arg->lo_pre_rb_soft_thresh_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_STH_LIMIT, 0);
	arg->lo_pre_soft_thresh_max_limit = val & 0xffff;
	arg->lo_pre_soft_thresh_min_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_HIKEEP, 0);
	arg->cur_spnr_hi_wgt_min_limit = val & 0xff;
	arg->pre_spnr_hi_wgt_min_limit = (val >> 8) & 0xff;
	arg->motion_est_lo_wgt_thred = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PIXMAX, 0);
	arg->pix_max_limit = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_SIGNUMTH, 0);
	arg->sigma_num_th = val;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_MONR, 0);
	arg->out_use_hi_noise_bal_nr_strg = val & 0x7ff;
	arg->out_use_md_noise_bal_nr_strg = (val >> 11) & 0x7ff;
	arg->gain_out_max_limit = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_SIGSCL, 0);
	arg->sigma_scale = val & 0xffff;
	arg->sigma_hdr_sht_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_DSOFF, 0);
	arg->lo_wgt_vfilt_offset = val & 0x3ff;
	arg->lo_diff_vfilt_offset = (val >> 10) & 0xfff;
	arg->lo_wgt_cal_first_line_vfilt_wgt = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_DSSCL, 0);
	arg->lo_wgt_vfilt_scale = val & 0xff;
	arg->lo_diff_vfilt_scale_bit = (val >> 8) & 0xff;
	arg->lo_diff_vfilt_scale = (val >> 16) & 0xff;
	arg->lo_diff_first_line_vfilt_wgt = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_ME0, 0);
	arg->motion_est_up_mvx_cost_offset = val & 0xffff;
	arg->motion_est_up_mvx_cost_scale = (val >> 16) & 0x7ff;
	arg->motion_est_sad_vert_wgt0 = (val >> 28) & 0x3;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_ME1, 0);
	arg->motion_est_up_left_mvx_cost_offset = val & 0x16;
	arg->motion_est_up_left_mvx_cost_scale = (val >> 16) & 0x7ff;
	arg->motion_est_sad_vert_wgt1 = (val >> 28) & 0x3;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_ME2, 0);
	arg->motion_est_up_right_mvx_cost_offset = val & 0xffff;
	arg->motion_est_up_right_mvx_cost_scale = (val >> 16) & 0x7ff;
	arg->motion_est_sad_vert_wgt2 = (val >> 28) & 0x3;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTMAX, 0);
	arg->lo_wgt_clip_motion_max_limit = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGT1MAX, 0);
	arg->mode1_wgt_max_limit = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_WGTM0, 0);
	arg->mode0_wgt_out_max_limit = val & 0xffff;
	arg->mode0_wgt_out_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PRELOWGT, 0);
	arg->pre_spnr_lo_val_wgt_out_wgt = val & 0xff;
	arg->pre_spnr_lo_filter_out_wgt = (val >> 8) & 0xff;
	arg->pre_spnr_lo_filter_wgt_min = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP35_BAY3D_LOCOEF0, 0);
	arg->lo_wgt_hflt_coeff2 = val & 0x7;
	arg->lo_wgt_hflt_coeff1 = (val >> 4) & 0xf;
	arg->lo_wgt_hflt_coeff0 = (val >> 8) & 0x1f;
	arg->sig_hflt_coeff2 = (val >> 16) & 0x7;
	arg->sig_hflt_coeff1 = (val >> 20) & 0xf;
	arg->sig_hflt_coeff0 = (val >> 24) & 0x1f;
	val = isp3_param_read(params_vdev, ISP35_BAY3D_LOCOEF1, 0);
	arg->lo_dif_hflt_coeff2 = val & 0x7;
	arg->lo_dif_hflt_coeff1 = (val >> 4) & 0xf;
	arg->lo_dif_hflt_coeff0 = (val >> 8) & 0x1f;

	val = isp3_param_read(params_vdev, ISP35_BAY3D_DPC0, 0);
	arg->pre_spnr_dpc_bright_str = val & 0x3;
	arg->pre_spnr_dpc_dark_str = (val >> 2) & 0x3;
	arg->pre_spnr_dpc_str = (val >> 3) & 0x7;
	arg->pre_spnr_dpc_wk_scale = (val >> 8) & 0xff;
	arg->pre_spnr_dpc_wk_offset = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP35_BAY3D_DPC1, 0);
	arg->pre_spnr_dpc_nr_bal_str = val & 0xffff;
	arg->pre_spnr_dpc_soft_thr_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_PRELOWGT, 0);
	arg->pre_spnr_lo_val_wgt_out_wgt = val & 0xff;
	arg->pre_spnr_lo_filter_out_wgt = (val >> 8) & 0xff;
	arg->pre_spnr_lo_filter_wgt_min = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_MIDBIG0, 0);
	arg->md_large_lo_md_wgt_offset = val & 0xff;
	arg->md_large_lo_md_wgt_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_MIDBIG1, 0);
	arg->md_large_lo_wgt_cut_offset = val & 0xffff;
	arg->md_large_lo_wgt_add_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_BAY3D_MIDBIG2, 0);
	arg->md_large_lo_wgt_scale = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP35_BAY3D_MONROFF, 0);
	arg->out_use_hi_noise_bal_nr_off = val & 0xfff;
	arg->out_use_md_noise_bal_nr_off = (val >> 16) & 0xfff;

	val = isp3_param_read(params_vdev, ISP35_B3DLDC_CTRL, 0);
	arg->btnr_ldc_en = !!(val & BIT(0));
	arg->b3dldcv_map13p3_en = !!(val & BIT(7));
	arg->b3dldcv_force_map_en = !!(val & BIT(8));

	val = isp3_param_read(params_vdev, ISP35_B3DLDC_ADR_STS, 0);
	arg->b3dldch_en = !!(val & BIT(0));
	arg->b3dldch_map13p3_en = !!(val & BIT(6));
	arg->b3dldch_force_map_en = !!(val & BIT(7));

	val = isp3_param_read(params_vdev, ISP35_B3DLDC_EXTBOUND1, 0);
	arg->btnr_ldc_wrap_ext_bound_offset = val & 0xffff;
	arg->btnr_ldcltp_mode = !!(val & BIT(16));

	val = priv->buf_b3dldc_idx[0];
	arg->lut_buf_fd = priv->buf_b3dldc[0][val].dma_fd;
}

static void rkisp_get_params_ynr(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp35_ynr_cfg *arg = &params->others.ynr_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_YNR_GLOBAL_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_YNR;
	arg->hi_spnr_bypass = !!(val & BIT(1));
	arg->mi_spnr_bypass = !!(val & BIT(2));
	arg->lo_spnr_bypass = !!(val & BIT(3));
	arg->rnr_en = !!(val & BIT(4));
	arg->tex2lo_strg_en = !!(val & BIT(5));
	arg->hi_lp_en = !!(val & BIT(6));
	arg->dsfilt_bypass = !!(val & BIT(7));
	arg->tex2wgt_en = !!(val & BIT(8));

	val = isp3_param_read(params_vdev, ISP33_YNR_GAIN_CTRL, 0);
	arg->global_set_gain = val & 0x3ff;
	arg->gain_merge_alpha = (val >> 12) & 0xf;
	arg->local_gain_scale = (val >> 16) & 0xff;

	for (i = 0; i < ISP35_YNR_ADJ_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP33_YNR_GAIN_ADJ_0_2 + i * 4, 0);
		arg->lo_spnr_gain2strg[i * 3] = val & 0x1ff;
		arg->lo_spnr_gain2strg[i * 3 + 1] = (val >> 10) & 0x1ff;
		arg->lo_spnr_gain2strg[i * 3 + 2] = (val >> 20) & 0x1ff;
	}

	val = isp3_param_read(params_vdev, ISP33_YNR_RNR_MAX_R, 0);
	arg->rnr_max_radius = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_YNR_RNR_CENTER_COOR, 0);
	arg->rnr_center_h = val & 0xffff;
	arg->rnr_center_v = (val >> 16) & 0xffff;

	for (i = 0; i < ISP35_YNR_XY_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP33_YNR_RNR_STRENGTH03 + i * 4, 0);
		arg->radius2strg[i * 4] = val & 0xff;
		arg->radius2strg[i * 4 + 1] = (val >> 8) & 0xff;
		arg->radius2strg[i * 4 + 2] = (val >> 16) & 0xff;
		arg->radius2strg[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP33_YNR_RNR_STRENGTH16, 0);
	arg->radius2strg[i * 4] = val & 0xff;

	for (i = 0; i < ISP35_YNR_XY_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_YNR_SGM_DX_0_1 + i * 4, 0);
		arg->luma2sima_x[i * 2] = val & 0xffff;
		arg->luma2sima_x[i * 2 + 1] = (val >> 16) & 0xffff;
		val = isp3_param_read(params_vdev, ISP33_YNR_SGM_Y_0_1 + i * 4, 0);
		arg->luma2sima_y[i * 2] = val & 0xffff;
		arg->luma2sima_y[i * 2 + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP33_YNR_SGM_DX_16, 0);
	arg->luma2sima_x[i * 2] = val & 0xffff;
	val = isp3_param_read(params_vdev, ISP33_YNR_SGM_Y_16, 0);
	arg->luma2sima_y[i * 2] = val & 0xffff;

	for (i = 0; i < ISP35_YNR_TEX2WGT_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP35_YNR_MI_TEX2WGT_SCALE_0_1_2 + i * 4, 0);
		arg->mi_spnr_tex2wgt_scale[i * 3] = val & 0xff;
		arg->mi_spnr_tex2wgt_scale[i * 3 + 1] = (val >> 10) & 0xff;
		arg->mi_spnr_tex2wgt_scale[i * 3 + 2] = (val >> 20) & 0xff;
		val = isp3_param_read(params_vdev, ISP35_YNR_LO_TEX2WGT_SCALE_0_1_2 + i * 4, 0);
		arg->lo_spnr_tex2wgt_scale[i * 3] = val & 0xff;
		arg->lo_spnr_tex2wgt_scale[i * 3 + 1] = (val >> 10) & 0xff;
		arg->lo_spnr_tex2wgt_scale[i * 3 + 2] = (val >> 20) & 0xff;
	}

	val = isp3_param_read(params_vdev, ISP33_YNR_HI_SIGMA_GAIN, 0);
	arg->hi_spnr_sigma_min_limit = val & 0x7ff;
	arg->hi_spnr_local_gain_alpha = (val >> 11) & 0x1f;
	arg->hi_spnr_strg = (val >> 16) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP33_YNR_HI_GAUS_COE, 0);
	arg->hi_spnr_filt_coeff[0] = val & 0x3f;
	arg->hi_spnr_filt_coeff[1] = (val >> 6) & 0x3f;
	arg->hi_spnr_filt_coeff[2] = (val >> 12) & 0x3f;
	arg->hi_spnr_filt_coeff[3] = (val >> 18) & 0x3f;

	val = isp3_param_read(params_vdev, ISP33_YNR_HI_WEIGHT, 0);
	arg->hi_spnr_filt_wgt_offset = val & 0x3ff;
	arg->hi_spnr_filt_center_wgt = (val >> 10) & 0x1fff;

	val = isp3_param_read(params_vdev, ISP33_YNR_HI_GAUS1_COE_0_2, 0);
	arg->hi_spnr_filt1_coeff[0] = val & 0x1ff;
	arg->hi_spnr_filt1_coeff[1] = (val >> 10) & 0x1ff;
	arg->hi_spnr_filt1_coeff[2] = (val >> 20) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP33_YNR_HI_GAUS1_COE_3_5, 0);
	arg->hi_spnr_filt1_coeff[3] = val & 0x1ff;
	arg->hi_spnr_filt1_coeff[4] = (val >> 10) & 0x1ff;
	arg->hi_spnr_filt1_coeff[5] = (val >> 20) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP33_YNR_HI_TEXT, 0);
	arg->hi_spnr_filt1_tex_thred = val & 0x7ff;
	arg->hi_spnr_filt1_tex_scale = (val >> 12) & 0x3ff;
	arg->hi_spnr_filt1_wgt_alpha = (val >> 22) & 0x1ff;

	val = isp3_param_read(params_vdev, ISP33_YNR_MI_GAUS_COE, 0);
	arg->mi_spnr_filt_coeff0 = val & 0xff;
	arg->mi_spnr_filt_coeff1 = (val >> 10) & 0xff;
	arg->mi_spnr_filt_coeff2 = (val >> 20) & 0xff;
	val = isp3_param_read(params_vdev, ISP35_YNR_MI_GAUS_COE1, 0);
	arg->mi_spnr_filt_coeff3 = val & 0xff;
	arg->mi_spnr_filt_coeff4 = (val >> 10) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_YNR_MI_STRG_DETAIL, 0);
	arg->mi_spnr_strg = val & 0xffff;
	arg->mi_spnr_soft_thred_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_YNR_MI_WEIGHT, 0);
	arg->mi_spnr_wgt = val & 0xff;
	arg->mi_spnr_filt_center_wgt = (val >> 10) & 0x7ff;
	arg->mi_ehance_scale_en = !!(val & BIT(23));
	arg->mi_ehance_scale = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP35_YNR_DSIIR_COE, 0);
	arg->dsfilt_diff_offset = val & 0x3ff;
	arg->dsfilt_center_wgt = (val >> 10) & 0x7ff;
	arg->dsfilt_strg = (val >> 21) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP33_YNR_LO_STRG_DETAIL, 0);
	arg->lo_spnr_strg = val & 0xffff;
	arg->lo_spnr_soft_thred_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_YNR_LO_LIMIT_SCALE, 0);
	arg->lo_spnr_thumb_thred_scale = val & 0x3ff;
	arg->tex2lo_strg_mantissa = (val >> 12) & 0x7ff;
	arg->tex2lo_strg_exponent = (val >> 24) & 0xf;

	val = isp3_param_read(params_vdev, ISP33_YNR_LO_WEIGHT, 0);
	arg->lo_spnr_wgt = val & 0xff;
	arg->lo_spnr_filt_center_wgt = (val >> 10) & 0x1fff;
	arg->lo_enhance_scale = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_YNR_LO_TEXT_THRED, 0);
	arg->tex2lo_strg_upper_thred = val & 0x3ff;
	arg->tex2lo_strg_lower_thred = (val >> 12) & 0x3ff;

	for (i = 0; i < ISP35_YNR_ADJ_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP33_YNR_FUSION_WEIT_ADJ_0_3 + i * 4, 0);
		arg->lo_gain2wgt[i * 4] = val & 0xff;
		arg->lo_gain2wgt[i * 4 + 1] = (val >> 8) & 0xff;
		arg->lo_gain2wgt[i * 4 + 2] = (val >> 16) & 0xff;
		arg->lo_gain2wgt[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP33_YNR_FUSION_WEIT_ADJ_8, 0);
	arg->lo_gain2wgt[i * 4] = val & 0xff;
}

static void rkisp_get_params_cnr(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp35_cnr_cfg *arg = &params->others.cnr_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_CNR_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_CNR;
	arg->exgain_bypass = !!(val & BIT(1));
	arg->yuv422_mode = !!(val & BIT(2));
	arg->thumb_mode = (val >> 4) & 0x3;
	arg->uv_dis = !!(val & BIT(6));
	arg->hiflt_wgt0_mode = !!(val & BIT(8));
	arg->local_alpha_dis = !!(val & BIT(11));
	arg->loflt_coeff = (val >> 12) & 0x3f;
	arg->hsv_alpha_en = !!(val & BIT(18));

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

	for (i = 0; i < ISP35_CNR_SIGMA_Y_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP32_CNR_SIGMA0 + i * 4, 0);
		arg->sgm_ratio[i * 4] = val & 0xff;
		arg->sgm_ratio[i * 4 + 1] = (val >> 8) & 0xff;
		arg->sgm_ratio[i * 4 + 2] = (val >> 16) & 0xff;
		arg->sgm_ratio[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP32_CNR_SIGMA0 + i * 4, 0);
	arg->sgm_ratio[i * 4] = val & 0xff;
	arg->bf_merge_max_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP32_CNR_IIR_GLOBAL_GAIN, 0);
	arg->loflt_global_sgm_ratio = val & 0xff;
	arg->loflt_global_sgm_ratio_alpha = (val >> 8) & 0xff;
	arg->bf_alpha_max_limit = (val >> 16) & 0xffff;

	for (i = 0; i < ISP35_CNR_WGT_SIGMA_Y_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP39_CNR_WGT_SIGMA0 + i * 4, 0);
		arg->cur_wgt[i * 4] = val & 0xff;
		arg->cur_wgt[i * 4 + 1] = (val >> 8) & 0xff;
		arg->cur_wgt[i * 4 + 2] = (val >> 16) & 0xff;
		arg->cur_wgt[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP39_CNR_WGT_SIGMA0 + i * 4, 0);
	arg->cur_wgt[i * 4] = val & 0xff;

	for (i = 0; i < ISP35_CNR_GAUS_SIGMAR_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_CNR_GAUS_X_SIGMAR0 + i * 4, 0);
		arg->hiflt_vsigma_idx[i * 3] = val & 0x3ff;
		arg->hiflt_vsigma_idx[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->hiflt_vsigma_idx[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_CNR_GAUS_X_SIGMAR0 + i * 4, 0);
	arg->hiflt_vsigma_idx[i * 3] = val & 0x3ff;
	arg->hiflt_vsigma_idx[i * 3 + 1] = (val >> 20) & 0x3ff;

	for (i = 0; i < ISP35_CNR_GAUS_SIGMAR_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP39_CNR_GAUS_Y_SIGMAR0 + i * 4, 0);
		arg->hiflt_vsigma[i * 2] = val & 0xffff;
		arg->hiflt_vsigma[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	for (i = 0; i < ISP35_CNR_WGT_SIGMA_Y_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP35_CNR_IIR_SIGMAR0 + i * 4, 0);
		arg->lo_flt_vsigma[i * 4] = val & 0xff;
		arg->lo_flt_vsigma[i * 4 + 1] = (val >> 8) & 0xff;
		arg->lo_flt_vsigma[i * 4 + 2] = (val >> 16) & 0xff;
		arg->lo_flt_vsigma[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP35_CNR_IIR_SIGMAR3, 0);
	arg->lo_flt_vsigma[i * 4] = val & 0xff;

	for (i = 0; i < ISP35_CNR_CURVE_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP35_CNR_HSV_CURVE0 + i * 4, 0);
		arg->hsv_adj_alpha_table[i * 4] = val & 0xff;
		arg->hsv_adj_alpha_table[i * 4 + 1] = (val >> 8) & 0xff;
		arg->hsv_adj_alpha_table[i * 4 + 2] = (val >> 16) & 0xff;
		arg->hsv_adj_alpha_table[i * 4 + 3] = (val >> 24) & 0xff;
		val = isp3_param_read(params_vdev, ISP35_CNR_SAT_CURVE0 + i * 4, 0);
		arg->sat_adj_alpha_table[i * 4] = val & 0xff;
		arg->sat_adj_alpha_table[i * 4 + 1] = (val >> 8) & 0xff;
		arg->sat_adj_alpha_table[i * 4 + 2] = (val >> 16) & 0xff;
		arg->sat_adj_alpha_table[i * 4 + 3] = (val >> 24) & 0xff;
		val = isp3_param_read(params_vdev, ISP35_CNR_GAIN_ADJ_CURVE0 + i * 4, 0);
		arg->gain_adj_alpha_table[i * 4] = val & 0xff;
		arg->gain_adj_alpha_table[i * 4 + 1] = (val >> 8) & 0xff;
		arg->gain_adj_alpha_table[i * 4 + 2] = (val >> 16) & 0xff;
		arg->gain_adj_alpha_table[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP35_CNR_HSV_CURVE2, 0);
	arg->hsv_adj_alpha_table[i * 4] = val & 0xff;
	arg->hsv_adj_alpha_table[i * 4 + 1] = (val >> 8) & 0xff;
	val = isp3_param_read(params_vdev, ISP35_CNR_SAT_CURVE2, 0);
	arg->sat_adj_alpha_table[i * 4] = val & 0xff;
	arg->sat_adj_alpha_table[i * 4 + 1] = (val >> 8) & 0xff;
	val = isp3_param_read(params_vdev, ISP35_CNR_GAIN_ADJ_CURVE2, 0);
	arg->gain_adj_alpha_table[i * 4] = val & 0xff;
	arg->gain_adj_alpha_table[i * 4 + 1] = (val >> 8) & 0xff;
}

static void rkisp_get_params_sharp(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp35_isp_params_cfg *params)
{
	struct isp35_sharp_cfg *arg = &params->others.sharp_cfg;
	struct isp35_sharp_cfg *arg_rec = &params_vdev->isp35_params->others.sharp_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP3X_SHARP_EN, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_SHARP;
	arg->bypass = !!(val & BIT(1));
	arg->local_gain_bypass = !!(val & BIT(2));
	arg->tex_est_mode = !!(val & BIT(3));
	arg->max_min_flt_mode = !!(val & BIT(4));
	arg->detail_fusion_wgt_mode = !!(val & BIT(5));
	arg->noise_calc_mode = !!(val & BIT(6));
	arg->radius_step_mode = !!(val & BIT(7));
	arg->noise_curve_mode = !!(val & BIT(8));
	arg->gain_wgt_mode = !!(val & BIT(9));
	arg->detail_lp_en = !!(val & BIT(10));
	arg->debug_mode = (val >> 12) & 0x7;

	val = isp3_param_read(params_vdev, ISP33_SHARP_TEXTURE0, 0);
	arg->fst_noise_scale = val & 0xffff;
	arg->fst_sigma_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_TEXTURE1, 0);
	arg->fst_sigma_offset = val & 0xffff;
	arg->fst_wgt_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_TEXTURE2, 0);
	arg->tex_wgt_mode = (val >> 8) & 0x3;
	arg->noise_est_alpha = (val >> 12) & 0x3f;

	val = isp3_param_read(params_vdev, ISP33_SHARP_TEXTURE3, 0);
	arg->sec_noise_scale = val & 0xffff;
	arg->sec_sigma_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_TEXTURE4, 0);
	arg->sec_sigma_offset = val & 0xffff;
	arg->sec_wgt_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_HPF_KERNEL0, 0);
	arg->img_hpf_coeff[0] = (val >> 24) & 0xff;
	val = isp3_param_read(params_vdev, ISP33_SHARP_HPF_KERNEL1, 0);
	arg->img_hpf_coeff[1] = val & 0xff;
	arg->img_hpf_coeff[2] = (val >> 8) & 0xff;
	arg->img_hpf_coeff[3] = (val >> 16) & 0xff;
	arg->img_hpf_coeff[4] = (val >> 24) & 0xff;
	val = isp3_param_read(params_vdev, ISP33_SHARP_TEXFLT_KERNEL, 0);
	arg->img_hpf_coeff[5] = val & 0xff;
	arg->texWgt_flt_coeff0 = (val >> 8) & 0xff;
	arg->texWgt_flt_coeff1 = (val >> 16) & 0xff;
	arg->texWgt_flt_coeff2 = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL0, 0);
	arg->detail_in_alpha = val & 0xff;
	arg->pre_bifilt_slope_fix = (val >> 8) & 0x7ff;
	arg->pre_bifilt_alpha = (val >> 20) & 0x3f;
	arg->fusion_wgt_min_limit = !!(val & BIT(28));
	arg->fusion_wgt_max_limit = !!(val & BIT(29));

	val = isp3_param_read(params_vdev, ISP33_SHARP_LUMA_DX, 0);
	for (i = 0; i < ISP35_SHARP_X_NUM; i++)
		arg->luma_dx[i] = (val >> (i * 4)) & 0xf;

	for (i = 0; i < ISP35_SHARP_Y_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_PBF_VSIGMA0 + i * 4, 0);
		arg->pre_bifilt_vsigma_inv[i * 2] = val & 0xffff;
		arg->pre_bifilt_vsigma_inv[i * 2 + 1] = (val >> 16) & 0xffff;
	}

	val = isp3_param_read(params_vdev, ISP33_SHARP_PBF_KERNEL, 0);
	arg->pre_bifilt_coeff0 = val & 0x3f;
	arg->pre_bifilt_coeff1 = (val >> 8) & 0x3f;
	arg->pre_bifilt_coeff2 = (val >> 16) & 0x3f;

	val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_KERNEL0, 0);
	arg->hi_detail_lpf_coeff[0] = val & 0xff;
	arg->hi_detail_lpf_coeff[1] = (val >> 8) & 0xff;
	arg->hi_detail_lpf_coeff[2] = (val >> 16) & 0xff;
	arg->hi_detail_lpf_coeff[3] = (val >> 24) & 0xff;
	val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_KERNEL1, 0);
	arg->hi_detail_lpf_coeff[4] = val & 0xff;
	arg->hi_detail_lpf_coeff[5] = (val >> 8) & 0xff;
	arg->mi_detail_lpf_coeff[0] = (val >> 16) & 0xff;
	arg->mi_detail_lpf_coeff[1] = (val >> 24) & 0xff;
	val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_KERNEL2, 0);
	arg->mi_detail_lpf_coeff[2] = val & 0xff;
	arg->mi_detail_lpf_coeff[3] = (val >> 8) & 0xff;
	arg->mi_detail_lpf_coeff[4] = (val >> 16) & 0xff;
	arg->mi_detail_lpf_coeff[5] = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_GAIN, 0);
	arg->global_gain = val & 0xffff;
	arg->gain_merge_alpha = (val >> 16) & 0xff;
	arg->local_gain_scale = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_GAIN_ADJ0, 0);
	arg->edge_gain_max_limit = val & 0xff;
	arg->edge_gain_min_limit = (val >> 8) & 0xff;
	arg->detail_gain_max_limit = (val >> 16) & 0xff;
	arg->detail_gain_min_limit = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_GAIN_ADJ1, 0);
	arg->hitex_gain_max_limit = val & 0xff;
	arg->hitex_gain_min_limit = (val >> 8) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_GAIN_ADJ2, 0);
	arg->edge_gain_slope = val & 0xff;
	arg->detail_gain_slope = (val >> 8) & 0xff;
	arg->hitex_gain_slope = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_GAIN_ADJ3, 0);
	arg->edge_gain_offset = val & 0x3ff;
	arg->detail_gain_offset = (val >> 10) & 0x3ff;
	arg->hitex_gain_offset = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_GAIN_ADJ4, 0);
	arg->edge_gain_sigma = val & 0xffff;
	arg->detail_gain_sigma = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_EDGE0, 0);
	arg->pos_edge_wgt_scale = val & 0xffff;
	arg->neg_edge_wgt_scale = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_EDGE1, 0);
	arg->pos_edge_strg = val & 0xff;
	arg->neg_edge_strg = (val >> 8) & 0xff;
	arg->overshoot_alpha = (val >> 16) & 0xff;
	arg->undershoot_alpha = (val >> 24) & 0xff;

	for (i = 0; i < ISP35_SHARP_EDGE_KERNEL_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_EDGE_KERNEL0 + i * 4, 0);
		arg->edge_bpf_coeff[i * 4] = val & 0xff;
		arg->edge_bpf_coeff[i * 4 + 1] = (val >> 8) & 0xff;
		arg->edge_bpf_coeff[i * 4 + 2] = (val >> 16) & 0xff;
		arg->edge_bpf_coeff[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP33_SHARP_EDGE_KERNEL2, 0);
	arg->edge_bpf_coeff[i * 4] = val & 0xff;
	arg->edge_bpf_coeff[i * 4 + 1] = (val >> 8) & 0xff;

	for (i = 0; i < ISP35_SHARP_EDGE_WGT_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_EDGE_WGT_VAL0 + i * 4, 0);
		arg->edge_wgt_val[i * 3] = val & 0x3ff;
		arg->edge_wgt_val[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->edge_wgt_val[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP33_SHARP_EDGE_WGT_VAL5, 0);
	arg->edge_wgt_val[i * 3] = val & 0x3ff;
	arg->edge_wgt_val[i * 3 + 1] = (val >> 10) & 0x3ff;

	for (i = 0; i < ISP35_SHARP_LUMA_STRG_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_LUMA_ADJ_STRG0 + i * 4, 0);
		arg->luma2strg[i * 4] = val & 0xff;
		arg->luma2strg[i * 4 + 1] = (val >> 8) & 0xff;
		arg->luma2strg[i * 4 + 2] = (val >> 16) & 0xff;
		arg->luma2strg[i * 4 + 3] = (val >> 24) & 0xff;
	}

	val = isp3_param_read(params_vdev, ISP33_SHARP_CENTER, 0);
	arg->center_x = val & 0xffff;
	arg->center_y = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_OUT_LIMIT, 0);
	arg->flat_max_limit = val & 0xffff;
	arg->edge_min_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_TEX_X_INV_FIX0, 0);
	arg->tex_x_inv_fix0 = val;
	val = isp3_param_read(params_vdev, ISP33_SHARP_TEX_X_INV_FIX1, 0);
	arg->tex_x_inv_fix1 = val;
	val = isp3_param_read(params_vdev, ISP33_SHARP_TEX_X_INV_FIX2, 0);
	arg->tex_x_inv_fix2 = val;

	val = isp3_param_read(params_vdev, ISP33_SHARP_LOCAL_STRG1, 0);
	arg->tex2loss_tex_in_hinr_strg[0] = (val >> 10) & 0x3ff;
	arg->tex2loss_tex_in_hinr_strg[1] = (val >> 20) & 0x3ff;
	val = isp3_param_read(params_vdev, ISP33_SHARP_LOCAL_STRG2, 0);
	arg->tex2loss_tex_in_hinr_strg[2] = val & 0x3ff;
	arg->tex2loss_tex_in_hinr_strg[3] = (val >> 10) & 0x3ff;

	for (i = 0; i < ISP35_SHARP_CONTRAST_STRG_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_SCALE_TAB0 + i * 4, 0);
		arg->contrast2pos_strg[i * 4] = val & 0xff;
		arg->contrast2pos_strg[i * 4 + 1] = (val >> 8) & 0xff;
		arg->contrast2pos_strg[i * 4 + 2] = (val >> 16) & 0xff;
		arg->contrast2pos_strg[i * 4 + 3] = (val >> 24) & 0xff;
		val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_SCALE_TAB3 + i * 4, 0);
		arg->contrast2neg_strg[i * 4] = val & 0xff;
		arg->contrast2neg_strg[i * 4 + 1] = (val >> 8) & 0xff;
		arg->contrast2neg_strg[i * 4 + 2] = (val >> 16) & 0xff;
		arg->contrast2neg_strg[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_SCALE_TAB2, 0);
	arg->contrast2pos_strg[i * 4] = val & 0xff;
	arg->pos_detail_strg = (val >> 8) & 0xff;
	val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_SCALE_TAB5, 0);
	arg->contrast2neg_strg[i * 4] = val & 0xff;
	arg->neg_detail_strg = (val >> 8) & 0xff;

	for (i = 0; i < ISP35_SHARP_TEX_CLIP_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_TEX_CLIP0 + i * 4, 0);
		arg->tex2detail_pos_clip[i * 3] = val & 0x3ff;
		arg->tex2detail_pos_clip[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tex2detail_pos_clip[i * 3 + 2] = (val >> 20) & 0x3ff;
		val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_TEX_CLIP3 + i * 4, 0);
		arg->tex2detail_neg_clip[i * 3] = val & 0x3ff;
		arg->tex2detail_neg_clip[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tex2detail_neg_clip[i * 3 + 2] = (val >> 20) & 0x3ff;
		val = isp3_param_read(params_vdev, ISP33_SHARP_GRAIN_TEX_CLIP0 + i * 4, 0);
		arg->tex2grain_pos_clip[i * 3] = val & 0x3ff;
		arg->tex2grain_pos_clip[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tex2grain_pos_clip[i * 3 + 2] = (val >> 20) & 0x3ff;
		val = isp3_param_read(params_vdev, ISP33_SHARP_GRAIN_TEX_CLIP3 + i * 4, 0);
		arg->tex2grain_neg_clip[i * 3] = val & 0x3ff;
		arg->tex2grain_neg_clip[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tex2grain_neg_clip[i * 3 + 2] = (val >> 20) & 0x3ff;
	}

	for (i = 0; i < ISP35_SHARP_LUM_CLIP_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_LUMA_CLIP0 + i * 4, 0);
		arg->luma2detail_pos_clip[i * 3] = val & 0x3ff;
		arg->luma2detail_pos_clip[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->luma2detail_pos_clip[i * 3 + 2] = (val >> 20) & 0x3ff;
		val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_LUMA_CLIP3 + i * 4, 0);
		arg->luma2detail_neg_clip[i * 3] = val & 0x3ff;
		arg->luma2detail_neg_clip[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->luma2detail_neg_clip[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_LUMA_CLIP2, 0);
	arg->luma2detail_pos_clip[i * 3] = val & 0x3ff;
	arg->luma2detail_pos_clip[i * 3 + 1] = (val >> 10) & 0x3ff;
	val = isp3_param_read(params_vdev, ISP33_SHARP_DETAIL_LUMA_CLIP5, 0);
	arg->luma2detail_neg_clip[i * 3] = val & 0x3ff;
	arg->luma2detail_neg_clip[i * 3 + 1] = (val >> 10) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_GRAIN_STRG, 0);
	arg->grain_strg = val & 0xff;

	for (i = 0; i < ISP35_SHARP_HUE_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_HUE_ADJ_TAB0 + i * 4, 0);
		arg->hue2strg[i * 3] = val & 0x3ff;
		arg->hue2strg[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->hue2strg[i * 3 + 2] = (val >> 20) & 0x3ff;
	}

	for (i = 0; i < ISP35_SHARP_DISATANCE_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_DISATANCE_ADJ0 + i * 4, 0);
		arg->distance2strg[i * 4] = val & 0xff;
		arg->distance2strg[i * 4 + 1] = (val >> 8) & 0xff;
		arg->distance2strg[i * 4 + 2] = (val >> 16) & 0xff;
		arg->distance2strg[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP33_SHARP_DISATANCE_ADJ2, 0);
	arg->distance2strg[i * 4] = val & 0xff;
	arg->distance2strg[i * 4 + 1] = (val >> 8) & 0xff;
	arg->distance2strg[i * 4 + 2] = (val >> 16) & 0xff;

	for (i = 0; i < ISP35_SHARP_TEX_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP35_SHARP_TEX2DETAIL_STRG0 + i * 4, 0);
		arg->tex2detail_strg[i * 3] = val & 0x3ff;
		arg->tex2detail_strg[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tex2detail_strg[i * 3 + 2] = (val >> 20) & 0x3ff;
	}

	for (i = 0; i < ISP35_SHARP_TEX_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_SHARP_NOISE_SIGMA0 + i * 4, 0);
		arg->hi_tex_threshold[i * 2] = val & 0xffff;
		arg->hi_tex_threshold[i * 2 + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP33_SHARP_NOISE_SIGMA4, 0);
	arg->hi_tex_threshold[i * 2] = val & 0xffff;

	for (i = 0; i < ISP35_SHARP_TEX_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP35_SHARP_TEX2MFDETAIL_STRG0 + i * 4, 0);
		arg->tex2mf_detail_strg[i * 3] = val & 0x3ff;
		arg->tex2mf_detail_strg[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->tex2mf_detail_strg[i * 3 + 2] = (val >> 20) & 0x3ff;
	}

	val = isp3_param_read(params_vdev, ISP33_SHARP_LOSSTEXINHINR_STRG, 0);
	arg->loss_tex_in_hinr_strg = val & 0xff;

	val = isp3_param_read(params_vdev, ISP33_SHARP_NOISE_CLIP, 0);
	arg->noise_clip_min_limit = val & 0xffff;
	arg->noise_clip_max_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP35_SHARP_EDGEWGTFLT_KERNEL, 0);
	arg->edge_wgt_flt_coeff0 = val & 0xff;
	arg->edge_wgt_flt_coeff1 = (val >> 8) & 0xff;
	arg->edge_wgt_flt_coeff2 = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP35_SHARP_EDGE_GLOBAL_CLIP, 0);
	arg->edge_glb_clip_thred = val & 0x3ff;
	arg->pos_edge_clip = (val >> 10) & 0x3ff;
	arg->neg_edge_clip = (val >> 20) & 0x3ff;

	val = isp3_param_read(params_vdev, ISP35_SHARP_MFDETAIL, 0);
	arg->mf_detail_data_alpha = val & 0xff;
	arg->pos_mf_detail_strg = (val >> 8) & 0xff;
	arg->neg_mf_detail_strg = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP35_SHARP_MFDETAIL_CLIP, 0);
	arg->mf_detail_pos_clip = val & 0x3ff;
	arg->sharp_mf_detail_neg_clip = (val >> 10) & 0x3ff;

	for (i = 0; i < ISP35_SHARP_SATURATION_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP35_SHARP_SATURATION_STRG0 + i * 4, 0);
		arg->staturation2strg[i * 4] = val & 0xff;
		arg->staturation2strg[i * 4 + 1] = (val >> 8) & 0xff;
		arg->staturation2strg[i * 4 + 2] = (val >> 16) & 0xff;
		arg->staturation2strg[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP35_SHARP_SATURATION_STRG2, 0);
	arg->staturation2strg[i * 4] = val & 0x1f;
	arg->lo_saturation_strg = (val >> 8) & 0x3ff;

	memcpy(arg->noise_curve_ext, arg_rec->noise_curve_ext, sizeof(arg->noise_curve_ext));
	arg->noise_count_thred_ratio = arg_rec->noise_count_thred_ratio;
	arg->noise_clip_scale = arg_rec->noise_clip_scale;
}

static void rkisp_get_params_cac(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp33_cac_cfg *arg = &params->others.cac_cfg;
	struct isp33_cac_cfg *arg_rec = &params_vdev->isp33_params->others.cac_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_CAC_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_CAC;
	arg->bypass_en = !!(val & BIT(1));
	arg->edge_detect_en = !!(val & BIT(2));
	arg->neg_clip0_en = !!(val & BIT(3));
	arg->wgt_color_en = !!(val & BIT(5));

	val = isp3_param_read(params_vdev, ISP3X_CAC_PSF_PARA, 0);
	arg->psf_table_fix_bit = val & 0xff;

	val = isp3_param_read(params_vdev, ISP33_CAC_HIGH_DIRECT, 0);
	arg->hi_drct_ratio = val & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_CAC_OVER_EXPO0, 0);
	arg->over_expo_thred = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_OVER_EXPO1, 0);
	arg->over_expo_adj = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_FLAT, 0);
	arg->flat_thred = val & 0xff;
	arg->flat_offset = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_CAC_GAUSS_COEFF, 0);
	arg->chroma_lo_flt_coeff0 = val & 0x7;
	arg->chroma_lo_flt_coeff1 = (val >> 4) & 0x7;
	arg->color_lo_flt_coeff0 = (val >> 8) & 0x7;
	arg->color_lo_flt_coeff1 = (val >> 12) & 0x7;

	val = isp3_param_read(params_vdev, ISP33_CAC_RATIO, 0);
	arg->search_range_ratio = val & 0xffff;
	arg->residual_chroma_ratio = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_COLOR_B, 0);
	arg->wgt_color_b_min_thred = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_COLOR_R, 0);
	arg->wgt_color_r_min_thred = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_COLOR_SLOPE_B, 0);
	arg->wgt_color_b_slope = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_COLOR_SLOPE_R, 0);
	arg->wgt_color_r_slope = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_COLOR_LUMA0, 0);
	arg->wgt_color_min_luma = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_COLOR_LUMA1, 0);
	arg->wgt_color_luma_slope = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_OVER_EXPO0, 0);
	arg->wgt_over_expo_min_thred = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_OVER_EXPO1, 0);
	arg->wgt_over_expo_slope = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_CONTRAST0, 0);
	arg->wgt_contrast_min_thred = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_CONTRAST1, 0);
	arg->wgt_contrast_slope = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_CONTRAST2, 0);
	arg->wgt_contrast_offset = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_DARK_AREA0, 0);
	arg->wgt_dark_thed = val;

	val = isp3_param_read(params_vdev, ISP33_CAC_WGT_DARK_AREA1, 0);
	arg->wgt_dark_slope = val;

	memcpy(arg->psf_b_ker, arg_rec->psf_b_ker, sizeof(arg->psf_b_ker));
	memcpy(arg->psf_r_ker, arg_rec->psf_r_ker, sizeof(arg->psf_r_ker));
}

static void rkisp_get_params_gain(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp35_isp_params_cfg *params)
{
	struct isp3x_gain_cfg *arg = &params->others.gain_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_GAIN_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_GAIN;

	val = isp3_param_read(params_vdev, ISP3X_GAIN_G0, 0);
	arg->g0 = val & 0x3ffff;

	val = isp3_param_read(params_vdev, ISP3X_GAIN_G1_G2, 0);
	arg->g1 = val & 0xffff;
	arg->g2 = (val >> 16) & 0xffff;
}

static void rkisp_get_params_csm(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp21_csm_cfg *arg = &params->others.csm_cfg;
	u32 i, val;

	for (i = 0; i < ISP35_CSM_COEFF_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP3X_ISP_CC_COEFF_0 + i * 4, 0);
		if (i == 0) {
			arg->csm_c_offset = (val >> 16) & 0xff;
			arg->csm_y_offset = (val >> 24) & 0x3f;
		}
		arg->csm_coeff[i] = val & 0x1ff;
	}
}

static void rkisp_get_params_cgc(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp21_cgc_cfg *arg = &params->others.cgc_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_ISP_CTRL0, 0);
	arg->yuv_limit = !!(val & ISP3X_SW_CGC_YUV_LIMIT);
	arg->ratio_en = !!(val & ISP3X_SW_CGC_RATIO_EN);
}

static void rkisp_get_params_ie(struct rkisp_isp_params_vdev *params_vdev,
				struct isp35_isp_params_cfg *params)
{
	u32 val = isp3_param_read(params_vdev, ISP3X_IMG_EFF_CTRL, 0);

	if (val & ISP35_MODULE_EN)
		params->module_ens |= ISP35_MODULE_IE;
}

static void rkisp_get_params_enh(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp33_enh_cfg *arg = &params->others.enh_cfg;
	struct isp33_enh_cfg *arg_rec = &params_vdev->isp35_params->others.enh_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP33_ENH_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_ENH;
	arg->bypass = !!(val & BIT(1));
	arg->blf3_bypass = !!(val & BIT(2));

	val = isp3_param_read(params_vdev, ISP33_ENH_IIR_FLT, 0);
	arg->iir_inv_sigma = val & 0xffff;
	arg->iir_soft_thed = (val >> 16) & 0xff;
	arg->iir_cur_wgt = (val >> 24) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_ENH_BILAT_FLT3X3, 0);
	arg->blf3_inv_sigma = val & 0x1ff;
	arg->blf3_cur_wgt = (val >> 16) & 0x1ff;
	arg->blf3_thumb_cur_wgt = (val >> 28) & 0xf;

	val = isp3_param_read(params_vdev, ISP33_ENH_BILAT_FLT5X5, 0);
	arg->blf5_inv_sigma = val & 0xffff;
	arg->blf5_cur_wgt = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_ENH_GLOBAL_STRG, 0);
	arg->global_strg = val & 0xffff;

	for (i = 0; i < ISP35_ENH_LUMA_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_ENH_LUMA_LUT0 + i * 4, 0);
		arg->lum2strg[i * 2] = val & 0xffff;
		arg->lum2strg[i * 2 + 1] = (val >> 16) & 0xffff;
	}
	val = isp3_param_read(params_vdev, ISP33_ENH_LUMA_LUT8, 0);
	arg->lum2strg[i * 2] = val & 0xffff;

	for (i = 0; i < ISP35_ENH_DETAIL_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP33_ENH_DETAIL_IDX0 + i * 4, 0);
		arg->detail2strg_idx[i * 3] = val & 0x3ff;
		arg->detail2strg_idx[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->detail2strg_idx[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP33_ENH_DETAIL_IDX2, 0);
	arg->detail2strg_idx[i * 3] = val & 0x3ff;
	arg->detail2strg_idx[i * 3 + 1] = (val >> 10) & 0x7ff;

	val = isp3_param_read(params_vdev, ISP33_ENH_DETAIL_POWER, 0);
	arg->detail2strg_power0 = val & 0xf;
	arg->detail2strg_power1 = (val >> 4) & 0xf;
	arg->detail2strg_power2 = (val >> 8) & 0xf;
	arg->detail2strg_power3 = (val >> 12) & 0xf;
	arg->detail2strg_power4 = (val >> 16) & 0xf;
	arg->detail2strg_power5 = (val >> 20) & 0xf;
	arg->detail2strg_power6 = (val >> 24) & 0xf;

	for (i = 0; i < ISP35_ENH_DETAIL_NUM / 2; i++) {
		val = isp3_param_read(params_vdev, ISP33_ENH_DETAIL_VALUE0 + i * 4, 0);
		arg->detail2strg_val[i * 2] = val & 0xffff;
		arg->detail2strg_val[i * 2 + 1] = (val >> 16) & 0xffff;
	}
	arg->pre_wet_frame_cnt0 = arg_rec->pre_wet_frame_cnt0;
	arg->pre_wet_frame_cnt1 = arg_rec->pre_wet_frame_cnt1;
	memcpy(arg->iir, arg_rec->iir, sizeof(arg->iir));
}

static void rkisp_get_params_hist(struct rkisp_isp_params_vdev *params_vdev,
				  struct isp35_isp_params_cfg *params)
{
	struct isp33_hist_cfg *arg = &params->others.hist_cfg;
	struct isp33_hist_cfg *arg_rec = &params_vdev->isp35_params->others.hist_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP33_HIST_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_HIST;
	arg->bypass = !!(val & BIT(1));
	arg->mem_mode = !!(val & BIT(4));

	val = isp3_param_read(params_vdev, ISP33_HIST_HF_STAT, 0);
	arg->count_scale = val & 0xff;
	arg->count_offset = (val >> 8) & 0xff;
	arg->count_min_limit = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_HIST_MAP0, 0);
	arg->merge_alpha = val & 0xffff;
	arg->user_set = (val >> 16) & 0xffff;

	val = isp3_param_read(params_vdev, ISP33_HIST_MAP1, 0);
	arg->map_count_scale = val & 0xffff;
	arg->gain_ref_wgt = (val >> 16) & 0xff;

	val = isp3_param_read(params_vdev, ISP33_HIST_IIR, 0);
	arg->flt_inv_sigma = val & 0xffff;
	arg->flt_cur_wgt = (val >> 16) & 0xff;

	for (i = 0; i < ISP35_HIST_ALPHA_NUM / 4; i++) {
		val = isp3_param_read(params_vdev, ISP33_HIST_POS_ALPHA0 + i * 4, 0);
		arg->pos_alpha[i * 4] = val & 0xff;
		arg->pos_alpha[i * 4 + 1] = (val >> 8) & 0xff;
		arg->pos_alpha[i * 4 + 2] = (val >> 16) & 0xff;
		arg->pos_alpha[i * 4 + 3] = (val >> 24) & 0xff;
		val = isp3_param_read(params_vdev, ISP33_HIST_NEG_ALPHA0 + i * 4, 0);
		arg->neg_alpha[i * 4] = val & 0xff;
		arg->neg_alpha[i * 4 + 1] = (val >> 8) & 0xff;
		arg->neg_alpha[i * 4 + 2] = (val >> 16) & 0xff;
		arg->neg_alpha[i * 4 + 3] = (val >> 24) & 0xff;
	}
	val = isp3_param_read(params_vdev, ISP33_HIST_POS_ALPHA4, 0);
	arg->pos_alpha[i * 4] = val & 0xff;
	val = isp3_param_read(params_vdev, ISP33_HIST_NEG_ALPHA4, 0);
	arg->neg_alpha[i * 4] = val & 0xff;

	val = isp3_param_read(params_vdev, ISP33_HIST_UV_SCL, 0);
	arg->saturate_scale = val & 0xff;

	arg->stab_frame_cnt0 = arg_rec->stab_frame_cnt0;
	arg->stab_frame_cnt1 = arg_rec->stab_frame_cnt1;
	memcpy(arg->iir, arg_rec->iir, sizeof(arg->iir));
}

static void rkisp_get_params_hsv(struct rkisp_isp_params_vdev *params_vdev,
				 struct isp35_isp_params_cfg *params)
{
	struct isp35_hsv_cfg *arg = &params->others.hsv_cfg;
	struct isp35_hsv_cfg *arg_rec = &params_vdev->isp35_params->others.hsv_cfg;
	u32 val;

	val = isp3_param_read(params_vdev, ISP3X_3DLUT_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_HSV;
	arg->hsv_1dlut0_en = !!(val & BIT(1));
	arg->hsv_1dlut1_en = !!(val & BIT(2));
	arg->hsv_2dlut0_en = !!(val & BIT(3));
	arg->hsv_2dlut1_en = !!(val & BIT(4));
	arg->hsv_2dlut2_en = !!(val & BIT(8));
	arg->hsv_2dlut12_cfg = !!(val & BIT(6));

	val = isp3_param_read(params_vdev, ISP35_HSV_MODE_CTRL, 0);
	arg->hsv_1dlut0_idx_mode = val & 0x3;
	arg->hsv_1dlut1_idx_mode = (val >> 2) & 0x3;
	arg->hsv_2dlut0_idx_mode = (val >> 4) & 0x3;
	arg->hsv_2dlut1_idx_mode = (val >> 6) & 0x3;
	arg->hsv_2dlut2_idx_mode = (val >> 8) & 0x3;
	arg->hsv_1dlut0_item_mode = (val >> 10) & 0x7;
	arg->hsv_1dlut1_item_mode = (val >> 13) & 0x7;
	arg->hsv_2dlut0_item_mode = (val >> 16) & 0x3;
	arg->hsv_2dlut1_item_mode = (val >> 18) & 0x3;
	arg->hsv_2dlut2_item_mode = (val >> 20) & 0x3;

	memcpy(arg->lut0_1d, arg_rec->lut0_1d, sizeof(arg->lut0_1d));
	memcpy(arg->lut1_1d, arg_rec->lut1_1d, sizeof(arg->lut1_1d));
	memcpy(arg->lut0_2d, arg_rec->lut0_2d, sizeof(arg->lut0_2d));
	memcpy(arg->lut1_2d, arg_rec->lut1_2d, sizeof(arg->lut1_2d));
	memcpy(arg->lut2_2d, arg_rec->lut2_2d, sizeof(arg->lut2_2d));
}

static void rkisp_get_params_rgbir(struct rkisp_isp_params_vdev *params_vdev,
				   struct isp35_isp_params_cfg *params)
{
	struct isp39_rgbir_cfg *arg = &params->others.rgbir_cfg;
	u32 i, val;

	val = isp3_param_read(params_vdev, ISP39_RGBIR_CTRL, 0);
	if (!(val & ISP35_MODULE_EN))
		return;
	params->module_ens |= ISP35_MODULE_RGBIR;

	val = isp3_param_read(params_vdev, ISP39_RGBIR_THETA, 0);
	arg->coe_theta = val & 0xfff;

	val = isp3_param_read(params_vdev, ISP39_RGBIR_DELTA, 0);
	arg->coe_delta = val & 0x3fff;

	for (i = 0; i < ISP35_RGBIR_SCALE_NUM; i++) {
		val = isp3_param_read(params_vdev, ISP39_RGBIR_SCALE0 + i * 4, 0);
		arg->scale[i] = val & 0x1ff;
	}

	for (i = 0; i < ISP35_RGBIR_LUMA_POINT_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_RGBIR_LUMA_POINT0 + i * 4, 0);
		arg->luma_point[i * 3] = val & 0x3ff;
		arg->luma_point[i * 3 + 1] = (val >> 10) & 0x3ff;
		arg->luma_point[i * 3 + 2] = (val >> 20) & 0x3ff;
	}
	val = isp3_param_read(params_vdev, ISP39_RGBIR_LUMA_POINT0 + i * 4, 0);
	arg->luma_point[i * 3] = val & 0x3ff;
	arg->luma_point[i * 3 + 1] = (val >> 10) & 0x7ff;

	for (i = 0; i < ISP35_RGBIR_SCALE_MAP_NUM / 3; i++) {
		val = isp3_param_read(params_vdev, ISP39_RGBIR_SCALE_MAP0 + i * 4, 0);
		arg->scale_map[i * 3] = val & 0x1ff;
		arg->scale_map[i * 3 + 1] = (val >> 9) & 0x1ff;
		arg->scale_map[i * 3 + 2] = (val >> 18) & 0x1ff;
	}
	val = isp3_param_read(params_vdev, ISP39_RGBIR_SCALE_MAP0 + i * 4, 0);
	arg->scale_map[i * 3] = val & 0x1ff;
	arg->scale_map[i * 3 + 1] = (val >> 9) & 0x1ff;
}

int rkisp_get_params_v35(struct rkisp_isp_params_vdev *params_vdev, void *arg)
{
	struct isp35_isp_params_cfg *params = arg;

	if (!params)
		return -EINVAL;
	memset(params, 0, sizeof(struct isp35_isp_params_cfg));
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
	rkisp_get_params_drc(params_vdev, params);
	rkisp_get_params_hdrmge(params_vdev, params);
	rkisp_get_params_ldch(params_vdev, params);
	rkisp_get_params_bay3d(params_vdev, params);
	rkisp_get_params_ynr(params_vdev, params);
	rkisp_get_params_cnr(params_vdev, params);
	rkisp_get_params_sharp(params_vdev, params);
	rkisp_get_params_gain(params_vdev, params);
	rkisp_get_params_csm(params_vdev, params);
	rkisp_get_params_cgc(params_vdev, params);
	rkisp_get_params_ie(params_vdev, params);
	rkisp_get_params_enh(params_vdev, params);
	rkisp_get_params_hist(params_vdev, params);
	rkisp_get_params_hsv(params_vdev, params);
	rkisp_get_params_cac(params_vdev, params);
	rkisp_get_params_rgbir(params_vdev, params);
	return 0;
}
#endif
