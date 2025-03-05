// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Guochun Huang <hero.huang@rock-chips.com>
 */

#include <linux/debugfs.h>
#include "rk628.h"
#include "rk628_cru.h"
#include "rk628_config.h"
#include "panel.h"
#include "rk628_rgb.h"

int rk628_rgb_parse(struct rk628 *rk628, struct device_node *rgb_np)
{
	int ret = 0;

	/* input/output: rgb/bt1120 */
	rk628->rgb.vccio_rgb = devm_regulator_get_optional(rk628->dev, "vccio-rgb");
	if (IS_ERR(rk628->rgb.vccio_rgb))
		rk628->rgb.vccio_rgb = NULL;

	/* input/output: bt1120 */
	if ((rk628_input_is_bt1120(rk628) || rk628_output_is_bt1120(rk628))) {
		if (of_property_read_bool(rk628->dev->of_node, "bt1120-dual-edge"))
			rk628->rgb.bt1120_dual_edge = true;

		if (of_property_read_bool(rk628->dev->of_node, "bt1120-yc-swap"))
			rk628->rgb.bt1120_yc_swap = true;

		if (of_property_read_bool(rk628->dev->of_node, "bt1120-uv-swap"))
			rk628->rgb.bt1120_uv_swap = true;
	}

	/* output: rgb/bt1120 */
	if (rk628_output_is_bt1120(rk628) || rk628_output_is_rgb(rk628))
		ret = rk628_panel_info_get(rk628, rgb_np);

	return ret;
}

static int rk628_rgb_resolution_show(struct seq_file *s, void *data)
{
	struct rk628 *rk628 = s->private;
	u16 width = 0, height = 0;
	u32 rgb_rx_eval_time, rgb_rx_clkrate;
	u64 ref_clk, pixel_clk;
	u32 val;

	rk628_i2c_read(rk628, GRF_RGB_RX_DBG_MEAS0, &val);
	rgb_rx_eval_time = (val & RGB_RX_EVAL_TIME_MASK) >> 16;

	rk628_i2c_read(rk628, GRF_RGB_RX_DBG_MEAS2, &val);
	rgb_rx_clkrate = val & RGB_RX_CLKRATE_MASK;

	ref_clk = rk628_cru_clk_get_rate(rk628, CGU_CLK_IMODET);
	pixel_clk = ref_clk * rgb_rx_clkrate;
	do_div(pixel_clk, rgb_rx_eval_time + 1);

	if (rk628_input_is_rgb(rk628)) {
		rk628_i2c_read(rk628, GRF_RGB_RX_DBG_MEAS4, &val);
		height = (val >> 16) & 0xffff;
		width = val & 0xffff;
	} else if (rk628_input_is_bt1120(rk628)) {
		rk628_i2c_read(rk628, GRF_SYSTEM_STATUS3, &val);
		height = val & DECODER_1120_LAST_LINE_NUM_MASK;

		rk628_i2c_read(rk628, GRF_SYSTEM_STATUS4, &val);
		width = val & DECODER_1120_LAST_PIX_NUM_MASK;
	}

	seq_printf(s, "%dx%d pclk:%llu\n", width, height, pixel_clk);

	return 0;
}

static int rk628_rgb_resolution_open(struct inode *inode, struct file *file)
{
	return single_open(file, rk628_rgb_resolution_show, inode->i_private);
}

static const struct file_operations rk628_rgb_resolution_fops = {
	.owner = THIS_MODULE,
	.open = rk628_rgb_resolution_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void rk628_rgb_decoder_create_debugfs_file(struct rk628 *rk628)
{
	if (rk628->version == RK628D_VERSION)
		return;

	if (rk628_input_is_rgb(rk628) || rk628_input_is_bt1120(rk628))
		debugfs_create_file("rgb_resolution", 0400, rk628->debug_dir,
				    rk628, &rk628_rgb_resolution_fops);
}

static void rk628_rgb_decoder_enable(struct rk628 *rk628)
{
	/* config sw_input_mode RGB */
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, SW_INPUT_MODE_MASK,
			      SW_INPUT_MODE(INPUT_MODE_RGB));

	if (rk628->version == RK628F_VERSION) {
		rk628_i2c_update_bits(rk628, GRF_RGB_RX_DBG_MEAS0,
				      RGB_RX_MODET_EN | RGB_RX_DCLK_EN,
				      RGB_RX_MODET_EN | RGB_RX_DCLK_EN);
		rk628_i2c_update_bits(rk628, GRF_RGB_RX_DBG_MEAS3,
				      RGB_RX_CNT_EN_MASK, RGB_RX_CNT_EN(1));
		rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON0, 0x10000000);
		rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON1, 0);
	}

	/* pinctrl for vop pin */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffffffff);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff5555);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b010b);
}

static void rk628_rgb_encoder_enable(struct rk628 *rk628)
{
	int voltage = 0;
	u32 d_strength, clk_strength;
	u64 dclk_delay;

	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
			      SW_BT_DATA_OEN_MASK | SW_OUTPUT_RGB_MODE_MASK,
			      SW_OUTPUT_RGB_MODE(OUTPUT_MODE_RGB >> 3));

	if (rk628->version != RK628F_VERSION)
		rk628_i2c_update_bits(rk628, GRF_POST_PROC_CON,
				      SW_DCLK_OUT_INV_EN, SW_DCLK_OUT_INV_EN);

	/* pinctrl for vop pin */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffffffff);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff5555);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b010b);

	/*
	 * Under the same drive strength and DCLK delay, the signal behaves
	 * differently under different voltage power domains. In order to
	 * center the eye diagram of the signal, have sufficient signal setup
	 * and hold time, and ensure that the signal does not overshoot, the
	 * drive strength and DCLK delay need to be set for the power domains
	 * of different voltages.
	 */
	if (rk628->rgb.vccio_rgb)
		voltage = regulator_get_voltage(rk628->rgb.vccio_rgb);

	switch (voltage) {
	case 1800000:
		d_strength = 3;
		clk_strength = 3;
		dclk_delay = 0x10000000;
		break;
	case 3300000:
		d_strength = 1;
		clk_strength = 2;
		dclk_delay = 0x100000000;
		break;
	default:
		d_strength = 1;
		clk_strength = 2;
		dclk_delay = 0x100000000;
	}

	/* rk628: modify IO drive strength for RGB */
	if (rk628->version == RK628F_VERSION)
		d_strength = d_strength * 0x1111 | 0xffff0000;
	else {
		d_strength = 0xffff7777;
		clk_strength = 7;
	}

	rk628_i2c_write(rk628, GRF_GPIO2A_D0_CON, d_strength);
	rk628_i2c_write(rk628, GRF_GPIO2A_D1_CON, d_strength);
	rk628_i2c_write(rk628, GRF_GPIO2B_D0_CON, d_strength);
	rk628_i2c_write(rk628, GRF_GPIO2B_D1_CON, d_strength);
	rk628_i2c_write(rk628, GRF_GPIO2C_D0_CON, d_strength);
	rk628_i2c_write(rk628, GRF_GPIO2C_D1_CON, d_strength);
	rk628_i2c_write(rk628, GRF_GPIO3A_D0_CON, d_strength & 0xf0fff0ff);
	rk628_i2c_write(rk628, GRF_GPIO3B_D_CON, clk_strength | 0x000f0000);

	/* rk628: modify DCLK delay for RGB */
	if (rk628->version == RK628F_VERSION) {
		rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON0,
				dclk_delay & 0xffffffff);
		rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON1,
				dclk_delay >> 32);
	}
}

static void rk628_rgb_encoder_disable(struct rk628 *rk628)
{
}


void rk628_rgb_rx_enable(struct rk628 *rk628)
{

	rk628_rgb_decoder_enable(rk628);

}

void rk628_rgb_tx_enable(struct rk628 *rk628)
{
	rk628_rgb_encoder_enable(rk628);

	rk628_panel_prepare(rk628);
	rk628_panel_enable(rk628);
}

void rk628_rgb_tx_disable(struct rk628 *rk628)
{
	rk628_panel_disable(rk628);
	rk628_panel_unprepare(rk628);

	rk628_rgb_encoder_disable(rk628);
}

static void rk628_bt1120_decoder_timing_cfg(struct rk628 *rk628)
{
	u32 src_hsync_len, src_hback_porch, src_hfront_porch, src_hactive;
	u32 src_vsync_len, src_vback_porch, src_vfront_porch, src_vactive;
	u32 dsp_htotal, dsp_hs_end, dsp_hact_st;
	u32 dsp_vtotal, dsp_vs_end, dsp_vact_st;
	u32 dsp_hbor_st, dsp_vbor_st;
	u16 bor_left = 0, bor_up = 0;
	struct rk628_display_mode *src = &rk628->src_mode;

	src_hactive = src->hdisplay;
	src_hsync_len = src->hsync_end - src->hsync_start;
	src_hback_porch = src->htotal - src->hsync_end;
	src_hfront_porch = src->hsync_start - src->hdisplay;
	src_vsync_len = src->vsync_end - src->vsync_start;
	src_vback_porch = src->vtotal - src->vsync_end;
	src_vfront_porch = src->vsync_start - src->vdisplay;
	src_vactive = src->vdisplay;

	dsp_htotal = src_hsync_len + src_hback_porch +
		     src_hactive + src_hfront_porch;
	dsp_vtotal = src_vsync_len + src_vback_porch +
		     src_vactive + src_vfront_porch;
	dsp_hs_end = src_hsync_len;
	dsp_vs_end = src_vsync_len;
	dsp_hbor_st = src_hsync_len + src_hback_porch;
	dsp_vbor_st = src_vsync_len + src_vback_porch;
	dsp_hact_st = dsp_hbor_st + bor_left;
	dsp_vact_st = dsp_vbor_st + bor_up;

	if (rk628->version == RK628F_VERSION)
		rk628_i2c_update_bits(rk628, GRF_RGB_RX_DBG_MEAS0,
				      RGB_RX_MODET_EN | RGB_RX_DCLK_EN,
				      RGB_RX_MODET_EN | RGB_RX_DCLK_EN);

	rk628_i2c_write(rk628, GRF_BT1120_TIMING_CTRL0, BT1120_DSP_HS_END(dsp_hs_end) |
			BT1120_DSP_HTOTAL(dsp_htotal));
	rk628_i2c_write(rk628, GRF_BT1120_TIMING_CTRL1, BT1120_DSP_HACT_ST(dsp_hact_st));
	rk628_i2c_write(rk628, GRF_BT1120_TIMING_CTRL2, BT1120_DSP_VS_END(dsp_vs_end) |
			BT1120_DSP_VTOTAL(dsp_vtotal));
	rk628_i2c_write(rk628, GRF_BT1120_TIMING_CTRL3, BT1120_DSP_VACT_ST(dsp_vact_st));
}

static void rk628_bt1120_decoder_enable(struct rk628 *rk628)
{
	struct rk628_display_mode *mode = rk628_display_get_src_mode(rk628);

	rk628_set_input_bus_format(rk628, BUS_FMT_YUV422);

	/* pinctrl for vop pin */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffffffff);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff5555);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b010b);

	/* config sw_input_mode bt1120 */
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, SW_INPUT_MODE_MASK,
			      SW_INPUT_MODE(INPUT_MODE_BT1120));

	if (rk628->version == RK628F_VERSION)
		rk628_bt1120_decoder_timing_cfg(rk628);

	/* operation resetn_bt1120dec */
	rk628_i2c_write(rk628, CRU_SOFTRST_CON00, 0x10001000);
	rk628_i2c_write(rk628, CRU_SOFTRST_CON00, 0x10000000);

	if (rk628->rgb.bt1120_dual_edge) {
		rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON0,
				DEC_DUALEDGE_EN, DEC_DUALEDGE_EN);
		rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON0, 0x10000000);
		rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON1, 0);
	} else {
		if (rk628->version == RK628F_VERSION) {
			rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON0,
					0x1);
			rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON1, 0);
		}
	}

	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON1, SW_SET_X_MASK,
			      SW_SET_X(mode->hdisplay));
	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON2, SW_SET_Y_MASK,
			      SW_SET_Y(mode->vdisplay));

	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
			      SW_BT_DATA_OEN_MASK | SW_INPUT_MODE_MASK,
			      SW_BT_DATA_OEN | SW_INPUT_MODE(INPUT_MODE_BT1120));

	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON0,
			      SW_CAP_EN_PSYNC | SW_CAP_EN_ASYNC |
			      SW_PROGRESS_EN |
			      SW_BT1120_YC_SWAP |
			      SW_BT1120_UV_SWAP,
			      SW_CAP_EN_PSYNC | SW_CAP_EN_ASYNC |
			      SW_PROGRESS_EN |
			      (rk628->rgb.bt1120_yc_swap ? SW_BT1120_YC_SWAP : 0) |
			      (rk628->rgb.bt1120_uv_swap ? SW_BT1120_UV_SWAP : 0));
}

static void rk628_bt1120_encoder_enable(struct rk628 *rk628)
{
	u32 val = 0;
	int voltage = 0;
	u32 strength;
	u64 dclk_delay;

	rk628_set_output_bus_format(rk628, BUS_FMT_YUV422);

	/* pinctrl for vop pin */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffffffff);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff5555);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b010b);

	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
			      SW_BT_DATA_OEN_MASK | SW_OUTPUT_RGB_MODE_MASK,
			      SW_OUTPUT_RGB_MODE(OUTPUT_MODE_BT1120 >> 3));

	if (rk628->version != RK628F_VERSION)
		rk628_i2c_update_bits(rk628, GRF_POST_PROC_CON,
				      SW_DCLK_OUT_INV_EN, SW_DCLK_OUT_INV_EN);

	/*
	 * Under the same drive strength and DCLK delay, the signal behaves
	 * differently under different voltage power domains. In order to
	 * center the eye diagram of the signal, have sufficient signal setup
	 * and hold time, and ensure that the signal does not overshoot, the
	 * drive strength and DCLK delay need to be set for the power domains
	 * of different voltages.
	 */
	if (rk628->rgb.vccio_rgb)
		voltage = regulator_get_voltage(rk628->rgb.vccio_rgb);

	switch (voltage) {
	case 1800000:
		strength = 3;
		dclk_delay = 0x100000000;
		break;
	case 3300000:
		strength = 1;
		dclk_delay = 0x1000000000;
		break;
	default:
		strength = 1;
		dclk_delay = 0x1000000000;
	}

	/* rk628: modify IO drive strength for BT1120 */
	if (rk628->version == RK628F_VERSION)
		strength = strength * 0x1111 | 0xffff0000;
	else
		strength = 0xffff1111;

	rk628_i2c_write(rk628, GRF_GPIO2A_D0_CON, strength);
	rk628_i2c_write(rk628, GRF_GPIO2A_D1_CON, strength);
	rk628_i2c_write(rk628, GRF_GPIO2B_D0_CON, strength);
	rk628_i2c_write(rk628, GRF_GPIO2B_D1_CON, strength);
	rk628_i2c_write(rk628, GRF_GPIO2C_D0_CON, strength);
	rk628_i2c_write(rk628, GRF_GPIO2C_D1_CON, strength);
	rk628_i2c_write(rk628, GRF_GPIO3A_D0_CON, strength & 0xf0fff0ff);
	rk628_i2c_write(rk628, GRF_GPIO3B_D_CON, strength & 0x000f000f);

	/* rk628: modify DCLK delay for BT1120 */
	if (rk628->rgb.bt1120_dual_edge) {
		val |= ENC_DUALEDGE_EN(1);
		rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON0, 0x10000000);
		rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON1, 0);
	} else {
		if (rk628->version == RK628F_VERSION) {
			rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON0,
					dclk_delay & 0xffffffff);
			rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON1,
					dclk_delay >> 32);
		}
	}

	val |= rk628->rgb.bt1120_yc_swap ? BT1120_YC_SWAP(1) : BT1120_YC_SWAP(0);
	val |= rk628->rgb.bt1120_uv_swap ? BT1120_UV_SWAP(1) : BT1120_UV_SWAP(0);
	rk628_i2c_write(rk628, GRF_RGB_ENC_CON, val);
}

void rk628_bt1120_rx_enable(struct rk628 *rk628)
{
	rk628_bt1120_decoder_enable(rk628);
}

void rk628_bt1120_tx_enable(struct rk628 *rk628)
{
	rk628_bt1120_encoder_enable(rk628);
}
