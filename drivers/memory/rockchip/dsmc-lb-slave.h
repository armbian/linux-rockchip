/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __ROCKCHIP_DSMC_LB_SLAVE_H
#define __ROCKCHIP_DSMC_LB_SLAVE_H

#define S2H_INT_FOR_DMA_NUM				(15)

/* LBC_SLAVE_CMN register */
#define CMN_CON(n)					(0x4 * (n))
#define CMN_STATUS					(0x80)
#define RGN_CMN_CON(rgn, com)				(0x100 + 0x100 * (rgn) + 0x4 * (com))
#define DBG_STATUS(n)					(0x900 + 0x4 * (n))

/* LBC_SLAVE_CSR register */
#define APP_CON(n)					(0x4 * (n))
#define APP_H2S_INT_STA					(0x80)
#define APP_H2S_INT_STA_EN				(0x84)
#define APP_H2S_INT_STA_SIG_EN				(0x88)
#define LBC_CON(n)					(0x100 + 0x4 * (n))
#define LBC_S2H_INT_STA					(0x180)
#define LBC_S2H_INT_STA_EN				(0x184)
#define LBC_S2H_INT_STA_SIG_EN				(0x188)
#define AXI_WR_ADDR_BASE				(0x800)
#define AXI_RD_ADDR_BASE				(0x804)
#define DBG_STA(n)					(0x900 + 0x4 * (n))

/* LBC_SLAVE_CMN_CMN_CON0 */
#define CA_CYC_16BIT					(0)
#define CA_CYC_32BIT					(1)
#define CA_CYC_SHIFT					(0)
#define CA_CYC_MASK					(0x1)
#define WR_LATENCY_CYC_SHIFT				(4)
#define WR_LATENCY_CYC_MASK				(0x7)
#define RD_LATENCY_CYC_SHIFT				(8)
#define RD_LATENCY_CYC_MASK				(0x7)
#define WR_DATA_CYC_EXTENDED_SHIFT			(11)
#define WR_DATA_CYC_EXTENDED_MASK			(0x1)

/* LBC_SLAVE_CMN_CMN_CON3 */
#define DATA_WIDTH_SHIFT				(0)
#define DATA_WIDTH_MASK					(0x1)
#define RDYN_GEN_CTRL_SHIFT				(4)
#define RDYN_GEN_CTRL_MASK				(0x1)

/* APP_H2S_INT_STA */
#define APP_H2S_INT_STA_SHIFT				(0)
#define APP_H2S_INT_STA_MASK				(0xFFFF)

/* APP_H2S_INT_STA_EN */
#define APP_H2S_INT_STA_EN_SHIFT			(0)
#define APP_H2S_INT_STA_EN_MASK				(0xFFFF)

/* APP_H2S_INT_STA_SIG_EN */
#define APP_H2S_INT_STA_SIG_EN_SHIFT			(0)
#define APP_H2S_INT_STA_SIG_EN_MASK			(0xFFFF)

/* LBC_S2H_INT_STA */
#define LBC_S2H_INT_STA_SHIFT				(0)
#define LBC_S2H_INT_STA_MASK				(0xFFFF)
/* LBC_S2H_INT_STA_EN */
#define LBC_S2H_INT_STA_EN_SHIFT			(0)
#define LBC_S2H_INT_STA_EN_MASK				(0xFFFF)
/* LBC_S2H_INT_STA_SIG_EN */
#define LBC_S2H_INT_STA_SIG_EN_SHIFT			(0)
#define LBC_S2H_INT_STA_SIG_EN_MASK			(0xFFFF)

#define DSMC_SLAVE_ENABLE(n)				((0x1 << (4 + 16)) | ((n) << 4))
#define DSMC_SLAVE_RDYN_MODE(n)				((0x1 << (5 + 16)) | ((n) << 5))

#define AXI_ADDR_4GB_RANGE				(1ULL << 32)

#endif /* __BUS_ROCKCHIP_ROCKCHIP_DSMC_SLAVE_H */
