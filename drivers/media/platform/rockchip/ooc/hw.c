// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include "dev.h"
#include "regs.h"
#include <linux/delay.h>

void rkooc_hw_init(struct rkooc_dev *dev)
{
	rkooc_write_reg(dev, 0x00, 0x0000000f);
	rkooc_write_reg(dev, 0x04, 0x20241206);
	rkooc_write_reg(dev, 0x08, 0x00000000);
	rkooc_write_reg(dev, 0x0c, 0x00000000);
	rkooc_write_reg(dev, 0x10, 0x00000000);
	rkooc_write_reg(dev, 0x14, 0x00000002);
	rkooc_write_reg(dev, 0x18, 0x00000000);
	rkooc_write_reg(dev, 0x1c, 0x00000000);
	rkooc_write_reg(dev, 0x20, 0xffff0000);
	rkooc_write_reg(dev, 0x24, 0x00000000);
	rkooc_write_reg(dev, 0x28, 0x00000000);
	rkooc_write_reg(dev, 0x2c, 0x00000100);
	rkooc_write_reg(dev, 0x30, 0x00000000);
	rkooc_write_reg(dev, 0x34, 0x00000000);
	rkooc_write_reg(dev, 0x38, 0x00000000);
	rkooc_write_reg(dev, 0x3c, 0x00000000);
	rkooc_write_reg(dev, 0x40, 0x00000000);
	rkooc_write_reg(dev, 0x44, 0x00000000);
	rkooc_write_reg(dev, 0x48, 0x00000000);
	rkooc_write_reg(dev, 0x4c, 0x00000000);
	rkooc_write_reg(dev, 0x50, 0x032000c8);
	rkooc_write_reg(dev, 0x54, 0x01900258);
	rkooc_write_reg(dev, 0x58, 0x00140002);
	rkooc_write_reg(dev, 0x5c, 0x00040012);
	rkooc_write_reg(dev, 0x60, 0x00000000);
	rkooc_write_reg(dev, 0x64, 0x00100000);
	rkooc_write_reg(dev, 0x68, 0x00000000);
	rkooc_write_reg(dev, 0x6c, 0x00000000);
	rkooc_write_reg(dev, 0x70, 0x00000000);
	rkooc_write_reg(dev, 0x74, 0x00000000);
	rkooc_write_reg(dev, 0x78, 0x00000000);
	rkooc_write_reg(dev, 0x7c, 0x00000000);
	rkooc_write_reg(dev, 0x80, 0x00000000);
	rkooc_write_reg(dev, 0x84, 0x00000000);
	rkooc_write_reg(dev, 0x88, 0x00000000);
	rkooc_write_reg(dev, 0x8c, 0x00000000);
	rkooc_write_reg(dev, 0x90, 0x00000000);
	rkooc_write_reg(dev, 0x94, 0x00000000);
	rkooc_write_reg(dev, 0x98, 0x00000000);
	rkooc_write_reg(dev, 0x9c, 0x00000000);
	rkooc_write_reg(dev, 0x00, 0xffffffff);
}

void rkooc_hw_deinit(struct rkooc_dev *dev)
{
	rkooc_hw_init(dev);
}

void rkooc_hw_pmclk_enable(struct rkooc_dev *dev)
{
	rkooc_hw_init(dev);

	rkooc_write_reg(dev, 0x14, 0x0000);
	rkooc_write_reg(dev, 0x00, 0x2002);
}

void rkooc_hw_pmclk_disable(struct rkooc_dev *dev)
{
	rkooc_write_reg(dev, 0x14, 0x2);
	rkooc_write_reg(dev, 0x00, 0x2002);
}

void rkooc_hw_enable_irq(struct rkooc_dev *dev)
{
	u32 value = 0;

	rkooc_set_field(value, WRITE_MASK, 0xffff);
	rkooc_set_field(value, VP_INTR_EN, 1);
	//rkooc_set_field(value, WIN1_EMPTY_INTR_EN, 1);
	//rkooc_set_field(value, PDAF_EMPTY_INTR_EN, 1);
	//rkooc_set_field(value, LINE_FLAG0_INTR_EN, 1);
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

void rkooc_hw_update_win_addr(struct rkooc_dev *dev, u32 addr)
{
	// OOC_WIN1_MST
	rkooc_write_reg(dev, OOC_WIN1_MST, addr);
	rkooc_write_reg(dev, OOC_REG_CFG_DONE, 0x40004);
}

static void rkooc_hw_h3812c1sh_config(struct rkooc_dev *dev)
{
	u32 value = 0;

	// REG: PDAF_WIN_FRM_MST
	value = (u32) dev->reglist.dma_addr;
	rkooc_write_reg(dev, OOC_PDAF_WIN_FRM_MST, value);

	// OOC_WIN1_MST
	value = (u32) dev->dummy.dma_addr;
	rkooc_write_reg(dev, OOC_WIN1_MST, value);

	u16 h_pw = 0;
	u16 h_bp = 0;
	u16 h_vd = 444;
	u16 h_fp = 196;
	u16 v_pw = 1;
	u16 v_bp = 19;
	u16 v_vd = 336;
	u16 v_fp = 4;
	u16 reg_bytes = 384 / 8;

	u16 hact_st = h_pw + h_bp;
	u16 hact_end = hact_st + h_vd;
	u16 hact_total = hact_end + h_fp;
	u16 vact_st = v_pw + v_bp;
	u16 vact_end = vact_st + v_vd;
	u16 vact_total = vact_end + v_fp;
	u16 win_vir_stride = h_vd / 4;

	u16 pdaf_width = hact_total;
	u16 pdaf_height = reg_bytes / pdaf_width + 1;
	u16 ex0_num = reg_bytes % pdaf_width;
	u16 pdaf_vact_st = vact_st - pdaf_height;

	rkooc_write_reg(dev, 0x0c, 0x00000000);
	rkooc_write_reg(dev, 0x10, 0x00000000);
	rkooc_write_reg(dev, 0x14, 0x20070001);
	rkooc_write_reg(dev, 0x1c, 0x00000064);
	rkooc_write_reg(dev, 0x20, 0xffff0000);
	rkooc_write_reg(dev, 0x24, 0xffffffff);
	rkooc_write_reg(dev, 0x30, 0x00002011);
	rkooc_write_reg(dev, 0x34, 0x00000530);
	rkooc_write_reg(dev, 0x3c, win_vir_stride);

	value = 0;
	rkooc_set_field(value, DSP_WIN_WIDTH, h_vd - 1);
	rkooc_set_field(value, DSP_WIN_HEIGHT, v_vd - 1);
	rkooc_write_reg(dev, 0x40, value);

	value = 0;
	rkooc_set_field(value, DSP_WIN_XST, hact_st);
	rkooc_set_field(value, DSP_WIN_YST, vact_st);
	rkooc_write_reg(dev, 0x44, value);

	rkooc_write_reg(dev, 0x48, 0x04030201);
	rkooc_write_reg(dev, 0x4c, 0x44332211);

	value = 0;
	rkooc_set_field(value, DSP_HS_END, h_pw);
	rkooc_set_field(value, DSP_HTOTAL, hact_total);
	rkooc_write_reg(dev, 0x50, value);

	value = 0;
	rkooc_set_field(value, DSP_HACT_END, hact_end);
	rkooc_set_field(value, DSP_HACT_ST, hact_st);
	rkooc_write_reg(dev, 0x54, value);

	value = 0;
	rkooc_set_field(value, DSP_VS_END, v_pw);
	rkooc_set_field(value, DSP_VTOTAL, vact_total);
	rkooc_write_reg(dev, 0x58, value);

	value = 0;
	rkooc_set_field(value, DSP_VACT_END, vact_end);
	rkooc_set_field(value, DSP_VACT_ST, vact_st);
	rkooc_write_reg(dev, 0x5c, value);

	rkooc_write_reg(dev, 0x60, 0x0000c301);
	rkooc_write_reg(dev, 0x64, 0x00170000);
	rkooc_write_reg(dev, 0x68, 0x80050001);

	// REG: PDAF_WIN_VIR_INFO
	value = 0;
	rkooc_set_field(value, SW_VIR_STRIDE_FRM, win_vir_stride);
	rkooc_write_reg(dev, OOC_PDAF_WIN_VIR_INFO, value);

	// REG: PDAF_WIN_LAST_VALID_NUM
	value = 0;
	rkooc_set_field(value, PDAF_EX0_LINE_VALID_NUM, ex0_num - 1);
	rkooc_write_reg(dev, OOC_PDAF_WIN_LAST_VALID_NUM, value);

	// REG: PDAF_WIN_DSP_INFO
	value = 0;
	rkooc_set_field(value, SW_PDAF_DSP_WIDTH, pdaf_width - 1);
	rkooc_set_field(value, SW_PDAF_DSP_HEIGHT, pdaf_height - 1);
	rkooc_write_reg(dev, OOC_PDAF_WIN_DSP_INFO, value);

	// REG: PDAF_WIN_DSP_ST
	value = 0;
	rkooc_set_field(value, SW_PDAF_HACT_ST, hact_st);
	rkooc_set_field(value, SW_PDAF_VACT_ST, pdaf_vact_st);
	rkooc_write_reg(dev, OOC_PDAF_WIN_DSP_ST, value);

	// REG: OOC_DSP_TIMING_CTRL
	value = 0;
	rkooc_set_field(value, SW_DSP_HOLD_EN, 1);
	rkooc_set_field(value, SW_DSP_HOLD_CYCLE, 0x41);
	rkooc_write_reg(dev, 0x90, 0x00008041);

	rkooc_write_reg(dev, 0x00, 0xffffffff);
}

static void rkooc_hw_gst412c_config(struct rkooc_dev *dev)
{
	u32 value = 0;

	// OOC_WIN1_MST
	value = (u32) dev->dummy.dma_addr;
	rkooc_write_reg(dev, OOC_WIN1_MST, value);

	rkooc_write_reg(dev, 0x0c, 0x00000020);
	rkooc_write_reg(dev, 0x10, 0x00000000);
	rkooc_write_reg(dev, 0x14, 0x400b8002);
	rkooc_write_reg(dev, 0x1c, 0x00000000);
	rkooc_write_reg(dev, 0x30, 0x30042001);
	rkooc_write_reg(dev, 0x34, 0x00000300);
	rkooc_write_reg(dev, 0x3c, 0x00000064);
	rkooc_write_reg(dev, 0x40, 0x0133018f);
	rkooc_write_reg(dev, 0x44, 0x00040004);
	rkooc_write_reg(dev, 0x48, 0x111d2a3f);
	rkooc_write_reg(dev, 0x4c, 0x151d2a3f);
	rkooc_write_reg(dev, 0x50, 0x01ea0000);
	rkooc_write_reg(dev, 0x54, 0x00000190);
	rkooc_write_reg(dev, 0x58, 0x02270002);
	rkooc_write_reg(dev, 0x5c, 0x00040138);
	rkooc_write_reg(dev, 0x60, 0x00000000);
	rkooc_write_reg(dev, 0x64, 0x00100000);
	rkooc_write_reg(dev, 0x68, 0x00000000);
	rkooc_write_reg(dev, 0x78, 0x00000000);
	rkooc_write_reg(dev, 0x7c, 0x00000000);
	rkooc_write_reg(dev, 0x80, 0x00000000);
	rkooc_write_reg(dev, 0x84, 0x00000000);
	rkooc_write_reg(dev, 0x90, 0x00000000);

	rkooc_write_reg(dev, 0x14, 0x400b8001);
	rkooc_write_reg(dev, 0x00, 0xffffffff);
}

void rkooc_hw_start(struct rkooc_dev *dev)
{
	if (dev->sensor == RKOOC_SENSOR_H3812C1SH)
		rkooc_hw_h3812c1sh_config(dev);
	if (dev->sensor == RKOOC_SENSOR_GST412C)
		rkooc_hw_gst412c_config(dev);
}

void rkooc_hw_stop(struct rkooc_dev *dev)
{
	rkooc_hw_init(dev);
}
