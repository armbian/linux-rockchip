// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include "dev.h"
#include "regs.h"

void rkooc_hw_init(struct rkooc_dev *dev)
{
	rkooc_write_reg(dev, OOC_DSP_BLANK_KEY, 0);
	rkooc_write_reg(dev, OOC_DSP_VCNT, 0);
	rkooc_write_reg(dev, OOC_LINE_FLAG, 0);
	rkooc_write_reg(dev, OOC_INTR_EN, 0xffff0000);
	rkooc_write_reg(dev, OOC_INTR_CLEAR, 0xffffffff);

	rkooc_write_reg(dev, OOC_WIN1_CTRL0, 0);
	rkooc_write_reg(dev, OOC_WIN1_CTRL1, 0);
	rkooc_write_reg(dev, OOC_WIN1_MST, 0);
	rkooc_write_reg(dev, OOC_WIN1_VIR, 0);

	rkooc_write_reg(dev, OOC_WIN1_DSP_INFO, 0);
	rkooc_write_reg(dev, OOC_WIN1_DSP_ST, 0);
	rkooc_write_reg(dev, OOC_WIN1_FMR_KEY, 0);
	rkooc_write_reg(dev, OOC_WIN1_LINE_KEY, 0);

	rkooc_write_reg(dev, OOC_DSP_HTOTAL_HS_END, 0x032000C8);
	rkooc_write_reg(dev, OOC_DSP_HACT_ST_END, 0x01900258);
	rkooc_write_reg(dev, OOC_DSP_VTOTAL_VS_END, 0x01400002);
	rkooc_write_reg(dev, OOC_DSP_VACT_ST_END, 0x00040012);

	rkooc_write_reg(dev, OOC_SYS_CTRL0, 0);
	rkooc_write_reg(dev, OOC_SYS_CTRL1, 0x2);

	rkooc_hw_pmclk_disable(dev);
}

void rkooc_hw_deinit(struct rkooc_dev *dev)
{
	rkooc_write_reg(dev, OOC_SYS_CTRL1, 0x2);

	// update sys
	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0x20002);
}

int rkooc_hw_pmclk_enable(struct rkooc_dev *dev)
{
	u32 value;
	int ret;

	value = rkooc_read_reg(dev, OOC_SYS_CTRL1);
	rkooc_set_field(value, VOP_STANDBY_EN, 0);
	rkooc_write_reg(dev, OOC_SYS_CTRL1, value);

	ret = clk_prepare_enable(dev->pmclk);
	if (ret < 0)
		v4l2_err(&dev->v4l2_dev, "Failed to enable pmclk!\n");

	// update sys
	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0x20002);
	return ret;
}

void rkooc_hw_pmclk_disable(struct rkooc_dev *dev)
{
	u32 value;

	clk_disable_unprepare(dev->pmclk);

	value = rkooc_read_reg(dev, OOC_SYS_CTRL1);
	rkooc_set_field(value, VOP_STANDBY_EN, 1);
	rkooc_write_reg(dev, OOC_SYS_CTRL1, value);

	// update sys
	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0x20002);
}

void rkooc_hw_enable_irq(struct rkooc_dev *dev)
{
	u32 value = 0;

	rkooc_set_field(value, WRITE_MASK, 0xffff);
	rkooc_set_field(value, VP_INTR_EN, 1);
	rkooc_write_reg(dev, OOC_INTR_EN, value);

	// update sys
	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0x20002);
}

void rkooc_hw_disable_irq(struct rkooc_dev *dev)
{
	u32 value = 0;

	rkooc_set_field(value, WRITE_MASK, 0xffff);
	rkooc_write_reg(dev, OOC_INTR_EN, value);

	// update sys
	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0x20002);
}

void rkooc_hw_win1_config(struct rkooc_dev *dev)
{
	u32 value = 0;
	u16 dsp_width;
	u16 dsp_height;
	u16 win_vir_stride;

	dsp_width = dev->ooc_width;
	dsp_height = dev->ooc_height;

	win_vir_stride = (dsp_width + 3) / 4;

	// REG: WIN1_CTRL0
	value = 0;
	rkooc_set_field(value, SW_WIN_EN, 1);	// win1 enable
	rkooc_set_field(value, SW_WIN_DATA_FMT, 0);	// raw8
	rkooc_set_field(value, SW_WIN_RAW_ALIGN, 0);	// Valid data low aligned
	rkooc_set_field(value, SW_WIN_NO_OUTSTAND, 0);
	rkooc_set_field(value, SW_WIN_VALID_BIT, 0);	// 6-bit valid
	rkooc_set_field(value, SW_WIN_BIT_CYCLE, 0);	// Win1_out_data pix_bit valid cycle num
	rkooc_set_field(value, SW_WIN_DATA_REVERSE, 0);	// Win1_data_o = win1_data[7:0] (valid bit)
	rkooc_set_field(value, SW_WIN_LINE_MODE, 1);	// 3-line out mode
	rkooc_set_field(value, SW_FRM_CODE_LINE_YST, 4);
	rkooc_set_field(value, SW_FRM_CODE_LINE_YST_EN, 1);
	rkooc_set_field(value, SW_LINE_CODE_SEL, 1);
	rkooc_write_reg(dev, OOC_WIN1_CTRL0, value);

	// REG: WIN1_CTRL1
	value = 0;
	rkooc_set_field(value, SW_WIN_AXI_GATHER_EN, 0);
	rkooc_set_field(value, SW_WIN_BURST_LENGTH, 0);
	rkooc_set_field(value, SW_WIN_AXI_GATHER_NUM, 0);
	rkooc_set_field(value, SW_WIN_RID, 3);
	rkooc_write_reg(dev, OOC_WIN1_CTRL1, value);

	// OOC_WIN1_MST
	value = (u32) dev->dummy.dma_addr;
	rkooc_write_reg(dev, OOC_WIN1_MST, value);

	// OOC_WIN1_VIR
	value = 0;
	rkooc_set_field(value, WIN_VIR_STRIDE, win_vir_stride);
	rkooc_write_reg(dev, OOC_WIN1_VIR, value);

	// OOC_WIN1_DSP_INFO
	value = 0;
	rkooc_set_field(value, DSP_WIN_WIDTH, dsp_width - 1);
	rkooc_set_field(value, DSP_WIN_HEIGHT, dsp_height - 1);
	rkooc_write_reg(dev, OOC_WIN1_DSP_INFO, value);

	// WIN1_DSP_ST
	value = 0;
	rkooc_set_field(value, DSP_WIN_XST, 4);
	rkooc_set_field(value, DSP_WIN_YST, 4);
	rkooc_write_reg(dev, OOC_WIN1_DSP_ST, value);
}

void rkooc_hw_update_win_addr(struct rkooc_dev *dev, u32 addr)
{
	// OOC_WIN1_MST
	rkooc_write_reg(dev, OOC_WIN1_MST, addr);
	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0x40004);
}

void rkooc_hw_win1_disable(struct rkooc_dev *dev)
{
	u32 value = 0;

	value = rkooc_read_reg(dev, OOC_WIN1_CTRL0);
	rkooc_set_field(value, SW_WIN_EN, 0);
	rkooc_write_reg(dev, OOC_WIN1_CTRL0, value);

	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0x40004);
}

void rkooc_hw_sys_config(struct rkooc_dev *dev)
{
	u32 value = 0;

	u16 h_pw = 0;
	u16 h_bp = 0;
	u16 h_vd = 400;
	u16 h_fp = 90;
	u16 v_pw = 2;
	u16 v_bp = 2;
	u16 v_vd = 308;
	u16 v_fp = 239;

	u16 hact_st;
	u16 hact_end;
	u16 hact_total;
	u16 vact_st;
	u16 vact_end;
	u16 vact_total;

	u8 line_delay = 6 * 2;

	hact_st = h_pw + h_bp;
	hact_end = hact_st + h_vd;
	hact_total = hact_end + h_fp;
	vact_st = v_pw + v_bp;
	vact_end = vact_st + v_vd;
	vact_total = vact_end + v_fp;

	// REG: DSP_BLANK_CODE
	value = 0;
	rkooc_set_field(value, SW_BANK_CODE, 0x20);
	rkooc_write_reg(dev, OOC_DSP_BLANK_KEY, value);

	// REG: SYS_CTRL0
	value = 0;
	rkooc_set_field(value, SW_NOC_QOS_EN, 0);
	rkooc_set_field(value, SW_NOC_QOS_VALUE, 0);
	rkooc_set_field(value, SW_NOC_HURRY_EN, 0);
	rkooc_set_field(value, SW_NOC_HURRY_VALUE, 0);
	rkooc_set_field(value, SW_NOC_HURRY_THRESHOLD, 0);
	rkooc_set_field(value, SW_AXI_MAX_OUTSTAND_EN, 0);
	rkooc_set_field(value, SW_AXI_MAX_OUTSTAND_NUM, 0);
	rkooc_write_reg(dev, OOC_SYS_CTRL0, value);

	// REG: SYS_CTRL1
	value = 0;
	rkooc_set_field(value, AUTO_GATING_EN, 1);
	rkooc_set_field(value, VOP_STANDBY_EN, 0);
	rkooc_set_field(value, IMD_DSP_DATA_OUT_MODE, 0);
	rkooc_set_field(value, SW_IO_PAD_CLK_SEL, 0);
	rkooc_set_field(value, SW_REG_OUT_SEL_IMD, 1);
	rkooc_set_field(value, SW_FRM_LINE_DELAY_NUM, (line_delay - 1));
	rkooc_set_field(value, SW_FRM_LINE_CODE_NUM, 4);
	rkooc_write_reg(dev, OOC_SYS_CTRL1, value);

	// REG: LINE_FLAG
	value = 0;
	rkooc_set_field(value, DSP_LINE_FLAG0_NUM, 0);
	rkooc_set_field(value, DSP_LINE_FLAG1_NUM, 0);
	rkooc_write_reg(dev, OOC_LINE_FLAG, value);

	// REG: INTR_EN
	value = 0;
	rkooc_set_field(value, FS_INTR_EN, 0);
	rkooc_set_field(value, VP_INTR_EN, 0);
	rkooc_set_field(value, LINE_FLAG0_INTR_EN, 0);
	rkooc_set_field(value, LINE_FLAG1_INTR_EN, 0);
	rkooc_set_field(value, BUS_ERROR_INTR_EN, 0);
	rkooc_set_field(value, WIN1_EMPTY_INTR_EN, 0);
	rkooc_set_field(value, PDAF_EMPTY_INTR_EN, 0);
	rkooc_set_field(value, DMA_FRM_FSH_INTR_EN, 0);
	rkooc_set_field(value, WRITE_MASK, 0xffff);
	rkooc_write_reg(dev, OOC_INTR_EN, value);

	// REG: DSP_HTOTAL_HS_END
	value = 0;
	rkooc_set_field(value, DSP_HS_END, h_pw);
	rkooc_set_field(value, DSP_HTOTAL, hact_total);
	rkooc_write_reg(dev, OOC_DSP_HTOTAL_HS_END, value);

	// REG: DSP_HACT_ST_END
	value = 0;
	rkooc_set_field(value, DSP_HACT_END, hact_end);
	rkooc_set_field(value, DSP_HACT_ST, hact_st);
	rkooc_write_reg(dev, OOC_DSP_HACT_ST_END, value);

	// REG: DSP_VTOTAL_VS_END
	value = 0;
	rkooc_set_field(value, DSP_VS_END, v_pw);
	rkooc_set_field(value, DSP_VTOTAL, vact_total);
	rkooc_write_reg(dev, OOC_DSP_VTOTAL_VS_END, value);

	// REG: DSP_VACT_ST_END
	value = 0;
	rkooc_set_field(value, DSP_VACT_END, vact_end);
	rkooc_set_field(value, DSP_VACT_ST, vact_st);
	rkooc_write_reg(dev, OOC_DSP_VACT_ST_END, value);

	// REG: WIN1_FMR_CODE
	value = 0;
	rkooc_set_field(value, SW_CODE_C0, 0x3f);
	rkooc_set_field(value, SW_CODE_C1, 0x2a);
	rkooc_set_field(value, SW_CODE_C2, 0x1d);
	rkooc_set_field(value, SW_CODE_B, 0x11);
	rkooc_write_reg(dev, OOC_WIN1_FMR_KEY, value);

	value = 0;
	rkooc_set_field(value, SW_CODE_C0, 0x3f);
	rkooc_set_field(value, SW_CODE_C1, 0x2a);
	rkooc_set_field(value, SW_CODE_C2, 0x1d);
	rkooc_set_field(value, SW_CODE_B, 0x15);
	rkooc_write_reg(dev, OOC_WIN1_LINE_KEY, value);

	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0xffffffff);

}
