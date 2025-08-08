// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Sugar Zhang <sugar.zhang@rock-chips.com>
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/genalloc.h>
#include <linux/interrupt.h>
#include <linux/io-64-nonatomic-hi-lo.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "virt-dma.h"

#define DRIVER_NAME			"rk-dma"
#define DMA_MAX_SIZE			(0x1000000)
#define LLI_BLOCK_SIZE			(SZ_4K)
#define RK_DMA_VER(major, minor)	(((major) << 16) | ((minor) << 0))

#define RK_MAX_BURST_LEN		16
#define RK_DMA_BUSWIDTHS \
	(BIT(DMA_SLAVE_BUSWIDTH_1_BYTE)  | \
	 BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) | \
	 BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) | \
	 BIT(DMA_SLAVE_BUSWIDTH_8_BYTES) | \
	 BIT(DMA_SLAVE_BUSWIDTH_16_BYTES))

#define HIWORD_UPDATE(v, h, l)	(((v) << (l)) | (GENMASK((h), (l)) << 16))
#define GENMASK_VAL(v, h, l)	(((v) & GENMASK(h, l)) >> l)

#define RK_DMA_CMN_GROUP_SIZE		0x100
#define RK_DMA_LCH_GROUP_SIZE		0x40

#define RK_DMA_CMN_REG(x)		(d->base + (x))
#define RK_DMA_LCH_REG(x)		(l->base + (x))
#define RK_DMA_LCHn_REG(n, x)		(d->base + RK_DMA_CMN_GROUP_SIZE + \
					 (RK_DMA_LCH_GROUP_SIZE * (n)) + (x))

/* RK_DMA Common Register Define */
#define RK_DMA_CMN_VER			RK_DMA_CMN_REG(0x0000) /* Address Offset: 0x0000 */
#define RK_DMA_CMN_CFG			RK_DMA_CMN_REG(0x0004) /* Address Offset: 0x0004 */
#define RK_DMA_CMN_CTL0			RK_DMA_CMN_REG(0x0008) /* Address Offset: 0x0008 */
#define RK_DMA_CMN_CTL1			RK_DMA_CMN_REG(0x000c) /* Address Offset: 0x000C */
#define RK_DMA_CMN_AXICTL		RK_DMA_CMN_REG(0x0010) /* Address Offset: 0x0010 */
#define RK_DMA_CMN_DYNCTL		RK_DMA_CMN_REG(0x0014) /* Address Offset: 0x0014 */
#define RK_DMA_CMN_IS0			RK_DMA_CMN_REG(0x0018) /* Address Offset: 0x0018 */
#define RK_DMA_CMN_IS1			RK_DMA_CMN_REG(0x001c) /* Address Offset: 0x001C */
#define RK_DMA_CMN_CAP0			RK_DMA_CMN_REG(0x0030) /* Address Offset: 0x0030 */
#define RK_DMA_CMN_CAP1			RK_DMA_CMN_REG(0x0034) /* Address Offset: 0x0034 */
#define RK_DMA_CMN_PCH_EN		RK_DMA_CMN_REG(0x0040) /* Address Offset: 0x0040 */
#define RK_DMA_CMN_PCH_SEN		RK_DMA_CMN_REG(0x0044) /* Address Offset: 0x0044 */

/* RK_DMA_Logic Channel Register Define */
#define RK_DMA_LCH_CTL0			RK_DMA_LCH_REG(0x0000) /* Address Offset: 0x0000 */
#define RK_DMA_LCH_CTL1			RK_DMA_LCH_REG(0x0004) /* Address Offset: 0x0004 */
#define RK_DMA_LCH_CMDBA		RK_DMA_LCH_REG(0x0008) /* Address Offset: 0x0008 */
#define RK_DMA_LCH_TRF_CMD		RK_DMA_LCH_REG(0x000c) /* Address Offset: 0x000C */
#define RK_DMA_LCH_CMDBA_HIGH		RK_DMA_LCH_REG(0x0010) /* Address Offset: 0x0010 */
#define RK_DMA_LCH_IS			RK_DMA_LCH_REG(0x0014) /* Address Offset: 0x0014 */
#define RK_DMA_LCH_IE			RK_DMA_LCH_REG(0x0018) /* Address Offset: 0x0018 */
#define RK_DMA_LCH_DBGS0		RK_DMA_LCH_REG(0x001c) /* Address Offset: 0x001C */
#define RK_DMA_LCH_DBGC0		RK_DMA_LCH_REG(0x0020) /* Address Offset: 0x0020 */

/* RK_DMA_Logic Channel-N Register Define */
#define RK_DMA_LCHn_CTL0(n)		RK_DMA_LCHn_REG(n, 0x0000) /* Address Offset: 0x0000 */
#define RK_DMA_LCHn_CTL1(n)		RK_DMA_LCHn_REG(n, 0x0004) /* Address Offset: 0x0004 */
#define RK_DMA_LCHn_CMDBA(n)		RK_DMA_LCHn_REG(n, 0x0008) /* Address Offset: 0x0008 */
#define RK_DMA_LCHn_TRF_CMD(n)		RK_DMA_LCHn_REG(n, 0x000c) /* Address Offset: 0x000C */
#define RK_DMA_LCHn_CMDBA_HIGH		RK_DMA_LCHn_REG(n, 0x0010) /* Address Offset: 0x0010 */
#define RK_DMA_LCHn_IS(n)		RK_DMA_LCHn_REG(n, 0x0014) /* Address Offset: 0x0014 */
#define RK_DMA_LCHn_IE(n)		RK_DMA_LCHn_REG(n, 0x0018) /* Address Offset: 0x0018 */
#define RK_DMA_LCHn_DBGS0(n)		RK_DMA_LCHn_REG(n, 0x001c) /* Address Offset: 0x001C */
#define RK_DMA_LCHn_DBGC0(n)		RK_DMA_LCHn_REG(n, 0x0020) /* Address Offset: 0x0020 */

/* CMN_VER */
#define CMN_VER_MAJOR(v)		GENMASK_VAL(v, 31, 16)
#define CMN_VER_MINOR(v)		GENMASK_VAL(v, 15, 0)

/* CMN_CFG */
#define CMN_CFG_EN			HIWORD_UPDATE(1, 0, 0)
#define CMN_CFG_DIS			HIWORD_UPDATE(0, 0, 0)
#define CMN_CFG_SRST			HIWORD_UPDATE(1, 1, 1)
#define CMN_CFG_IE_EN			HIWORD_UPDATE(1, 2, 2)
#define CMN_CFG_IE_DIS			HIWORD_UPDATE(0, 2, 2)

/* CMN_AXICTL */
#define CMN_AXICTL_AWLOCK_MASK		GENMASK(1, 0)
#define CMN_AXICTL_AWCACHE_MASK		GENMASK(5, 2)
#define CMN_AXICTL_AWPROT_MASK		GENMASK(8, 6)
#define CMN_AXICTL_ARLOCK_MASK		GENMASK(17, 16)
#define CMN_AXICTL_ARCACHE_MASK		GENMASK(21, 18)
#define CMN_AXICTL_ARPROT_MASK		GENMASK(24, 22)

/* CMN_DYNCTL */
#define CMN_DYNCTL_LCH_DYNEN_MASK	BIT(0)
#define CMN_DYNCTL_LCH_DYNEN_EN		BIT(0)
#define CMN_DYNCTL_LCH_DYNEN_DIS	0
#define CMN_DYNCTL_PCH_DYNEN_MASK	BIT(1)
#define CMN_DYNCTL_PCH_DYNEN_EN		BIT(1)
#define CMN_DYNCTL_PCH_DYNEN_DIS	0
#define CMN_DYNCTL_AXI_DYNEN_MASK	BIT(2)
#define CMN_DYNCTL_AXI_DYNEN_EN		BIT(2)
#define CMN_DYNCTL_AXI_DYNEN_DIS	0
#define CMN_DYNCTL_ARB_DYNEN_MASK	BIT(3)
#define CMN_DYNCTL_ARB_DYNEN_EN		BIT(3)
#define CMN_DYNCTL_ARB_DYNEN_DIS	0
#define CMN_DYNCTL_MEM_DYNEN_MASK	BIT(4)
#define CMN_DYNCTL_MEM_DYNEN_EN		BIT(4)
#define CMN_DYNCTL_MEM_DYNEN_DIS	0

/* CMN_CAP0 */
#define CMN_LCH_NUM(v)			(GENMASK_VAL(v, 5, 0) + 1)
#define CMN_PCH_NUM(v)			(GENMASK_VAL(v, 11, 6) + 1)
#define CMN_STATIC_SUP			BIT(12)
#define CMN_STRIDE_SUP			BIT(13)
#define CMN_TT_SUP_M2M			BIT(14)
#define CMN_TT_SUP_M2P			BIT(15)
#define CMN_TT_SUP_P2M			BIT(16)
#define CMN_TT_SUP_P2P			BIT(17)
#define CMN_MB_SUP_CONT			BIT(18)
#define CMN_MB_SUP_RELOAD		BIT(19)
#define CMN_MB_SUP_LLI			BIT(20)
#define CMN_BUF_DEPTH(v)		(GENMASK_VAL(v, 31, 21) + 1)

/* CMN_CAP1 */
#define CMN_AXI_SIZE(v)			(1 << GENMASK_VAL(v, 2, 0))
#define CMN_AXI_LEN(v)			(GENMASK_VAL(v, 10, 3) + 1)
#define CMN_AXBURST_SUP_FIXED		BIT(11)
#define CMN_AXBURST_SUP_INCR		BIT(12)
#define CMN_AXBURST_SUP_WRAP		BIT(13)
#define CMN_AXADDR_WIDTH(v)		(32 + GENMASK_VAL(v, 18, 14) - 3)
#define CMN_AXOSR_SUP(v)		(GENMASK_VAL(v, 23, 19) + 1)

/* CMN_PCH_EN */
#define CMN_PCH_EN(n)			HIWORD_UPDATE(1, (n), (n))
#define CMN_PCH_DIS(n)			HIWORD_UPDATE(0, (n), (n))

/* CMN_PCH_SEN */
#define CMN_PCH_S_EN(n)			HIWORD_UPDATE(1, (n), (n))
#define CMN_PCH_S_DIS(n)		HIWORD_UPDATE(0, (n), (n))

/* LCH_CTL0 */
#define LCH_CTL0_CH_EN_MASK		BIT(0)
#define LCH_CTL0_CH_EN			BIT(0)
#define LCH_CTL0_CH_DIS			0

/* LCH_CTL1 */
#define LCH_CTL1_WFE_ID_MASK		GENMASK(5, 0)
#define LCH_CTL1_WFE_ID(x)		((x) << 0)
#define LCH_CTL1_SEV_ID_MASK		GENMASK(11, 6)
#define LCH_CTL1_SEV_ID(x)		((x) << 6)
#define LCH_CTL1_BIND_PIDX_S_MASK	GENMASK(19, 16)
#define LCH_CTL1_BIND_PIDX_S(x)		((x) << 16)
#define LCH_CTL1_WFE_EN_MASK		BIT(24)
#define LCH_CTL1_WFE_EN			BIT(24)
#define LCH_CTL1_WFE_DIS		0
#define LCH_CTL1_SEV_EN_MASK		BIT(25)
#define LCH_CTL1_SEV_EN			BIT(25)
#define LCH_CTL1_SEV_DIS		0
#define LCH_CTL1_BIND_S_EN_MASK		BIT(26)
#define LCH_CTL1_BIND_S_EN		BIT(26)
#define LCH_CTL1_BIND_S_DIS		0

/* LCH_TRF_CMD */
#define LCH_TRF_CMD_DMA_START		HIWORD_UPDATE(1, 0, 0)
#define LCH_TRF_CMD_DMA_KILL		HIWORD_UPDATE(1, 1, 1)
#define LCH_TRF_CMD_DMA_RESUME		HIWORD_UPDATE(1, 2, 2)
#define LCH_TRF_CMD_DMA_FLUSH		HIWORD_UPDATE(1, 3, 3)
#define LCH_TRF_CMD_SRC_MT(x)		HIWORD_UPDATE(x, 11, 10)
#define LCH_TRF_CMD_DST_MT(x)		HIWORD_UPDATE(x, 13, 12)
#define LCH_TRF_CMD_TT_FC(x)		HIWORD_UPDATE(x, 15, 14)

/* LCH_IS */
#define LCH_IS_DMA_DONE_IS		BIT(0)
#define LCH_IS_BLOCK_DONE_IS		BIT(1)
#define LCH_IS_SRC_BTRANS_DONE_IS	BIT(2)
#define LCH_IS_DST_BTRANS_DONE_IS	BIT(3)
#define LCH_IS_SRC_STRANS_DONE_IS	BIT(4)
#define LCH_IS_DST_STRANS_DONE_IS	BIT(5)
#define LCH_IS_SRC_RD_ERR_IS		BIT(6)
#define LCH_IS_DST_WR_ERR_IS		BIT(7)
#define LCH_IS_CMD_RD_ERR_IS		BIT(8)
#define LCH_IS_CMD_WR_ERR_IS		BIT(9)
#define LCH_IS_LLI_RD_ERR_IS		BIT(10)
#define LCH_IS_LLI_INVALID_IS		BIT(11)
#define LCH_IS_LLI_WAIT_IS		BIT(12)

/* LCH_IE */
#define LCH_IE_DMA_DONE_IE_MASK		BIT(0)
#define LCH_IE_DMA_DONE_IE_EN		BIT(0)
#define LCH_IE_DMA_DONE_IE_DIS		0
#define LCH_IE_BLOCK_DONE_IE_MASK	BIT(1)
#define LCH_IE_BLOCK_DONE_IE_EN		BIT(1)
#define LCH_IE_BLOCK_DONE_IE_DIS	0
#define LCH_IE_SRC_BTRANS_DONE_IE_MASK	BIT(2)
#define LCH_IE_SRC_BTRANS_DONE_IE_EN	BIT(2)
#define LCH_IE_SRC_BTRANS_DONE_IE_DIS	0
#define LCH_IE_DST_BTRANS_DONE_IE_MASK	BIT(3)
#define LCH_IE_DST_BTRANS_DONE_IE_EN	BIT(3)
#define LCH_IE_DST_BTRANS_DONE_IE_DIS	0
#define LCH_IE_SRC_STRANS_DONE_IE_MASK	BIT(4)
#define LCH_IE_SRC_STRANS_DONE_IE_EN	BIT(4)
#define LCH_IE_SRC_STRANS_DONE_IE_DIS	0
#define LCH_IE_DST_STRANS_DONE_IE_MASK	BIT(5)
#define LCH_IE_DST_STRANS_DONE_IE_EN	BIT(5)
#define LCH_IE_DST_STRANS_DONE_IE_DIS	0
#define LCH_IE_SRC_RD_ERR_IE_MASK	BIT(6)
#define LCH_IE_SRC_RD_ERR_IE_EN		BIT(6)
#define LCH_IE_SRC_RD_ERR_IE_DIS	0
#define LCH_IE_DST_WR_ERR_IE_MASK	BIT(7)
#define LCH_IE_DST_WR_ERR_IE_EN		BIT(7)
#define LCH_IE_DST_WR_ERR_IE_DIS	0
#define LCH_IE_CMD_RD_ERR_IE_MASK	BIT(8)
#define LCH_IE_CMD_RD_ERR_IE_EN		BIT(8)
#define LCH_IE_CMD_RD_ERR_IE_DIS	0
#define LCH_IE_CMD_WR_ERR_IE_MASK	BIT(9)
#define LCH_IE_CMD_WR_ERR_IE_EN		BIT(9)
#define LCH_IE_CMD_WR_ERR_IE_DIS	0
#define LCH_IE_LLI_RD_ERR_IE_MASK	BIT(10)
#define LCH_IE_LLI_RD_ERR_IE_EN		BIT(10)
#define LCH_IE_LLI_RD_ERR_IE_DIS	0
#define LCH_IE_LLI_INVALID_IE_MASK	BIT(11)
#define LCH_IE_LLI_INVALID_IE_EN	BIT(11)
#define LCH_IE_LLI_INVALID_IE_DIS	0
#define LCH_IE_LLI_WAIT_IE_MASK		BIT(12)
#define LCH_IE_LLI_WAIT_IE_EN		BIT(12)
#define LCH_IE_LLI_WAIT_IE_DIS		0

/* LCH_DBGS0 */
#define LCH_DBGS0_DMA_CS(v)		GENMASK_VAL(v, 3, 0)
#define LCH_DBGS0_DMA_CS_IDLE		0
#define LCH_DBGS0_DMA_CS_WFE		1
#define LCH_DBGS0_DMA_CS_BLK_START	2
#define LCH_DBGS0_DMA_CS_PERI_WAIT	3
#define LCH_DBGS0_DMA_CS_BIND		4
#define LCH_DBGS0_DMA_CS_TRANS		5
#define LCH_DBGS0_DMA_CS_REL		6
#define LCH_DBGS0_DMA_CS_PERI_ACK	7
#define LCH_DBGS0_DMA_CS_PERI_FNSH	8
#define LCH_DBGS0_DMA_CS_SEV		9
#define LCH_DBGS0_DMA_CS_BLK_DONE	10
#define LCH_DBGS0_DMA_CS_SUSPD		11
#define LCH_DBGS0_DMA_CS_ERR		12
#define LCH_DBGS0_DMA_CS_DONE		13
#define LCH_DBGS0_BIND_VLD		BIT(11)
#define LCH_DBGS0_BIND_PIDX(v)		GENMASK_VAL(v, 15, 12)

/* LCH_DBGC0 */
#define LCH_DBGC0_SOFT_SEV		BIT(24)

/* LCH_LLI_CNT */
#define LCH_LLI_CNT(v)			GENMASK_VAL(v, 15, 0)

/* CMD Entry Memory Structure Define */
/* TRF_CTL0 */
#define TRF_CTL0_LLI_VALID_MASK		BIT(0)
#define TRF_CTL0_LLI_VALID		BIT(0)
#define TRF_CTL0_LLI_INVALID		0
#define TRF_CTL0_LLI_LAST_MASK		BIT(1)
#define TRF_CTL0_LLI_LAST		BIT(1)
#define TRF_CTL0_LLI_WAIT_MASK		BIT(2)
#define TRF_CTL0_LLI_WAIT_EN		BIT(2)
#define TRF_CTL0_IOC_MASK		BIT(3)
#define TRF_CTL0_IOC_EN			BIT(3)
#define TRF_CTL0_CNT_CLR		BIT(4)
#define TRF_CTL0_MSIZE_MASK		GENMASK(31, 15)
#define TRF_CTL0_MSIZE(x)		((x) << 15)

/* TRF_CTL1 */
#define TRF_CTL1_ARBURST_MASK		BIT(0)
#define TRF_CTL1_ARBURST_INCR		BIT(0)
#define TRF_CTL1_ARBURST_FIXED		0
#define TRF_CTL1_ARSIZE_MASK		GENMASK(4, 2)
#define TRF_CTL1_ARSIZE(x)		((x) << 2)
#define TRF_CTL1_ARLEN_MASK		GENMASK(10, 5)
#define TRF_CTL1_ARLEN(x)		((x - 1) << 5)
#define TRF_CTL1_AROSR_MASK		GENMASK(14, 11)
#define TRF_CTL1_AROSR(x)		((x) << 11)
#define TRF_CTL1_AWBURST_MASK		BIT(16)
#define TRF_CTL1_AWBURST_INCR		BIT(16)
#define TRF_CTL1_AWBURST_FIXED		0
#define TRF_CTL1_AWSIZE_MASK		GENMASK(20, 18)
#define TRF_CTL1_AWSIZE(x)		((x) << 18)
#define TRF_CTL1_AWLEN_MASK		GENMASK(26, 21)
#define TRF_CTL1_AWLEN(x)		((x - 1) << 21)
#define TRF_CTL1_AWOSR_MASK		GENMASK(30, 27)
#define TRF_CTL1_AWOSR(x)		((x) << 27)

/* BLOCK_TS */
#define BLOCK_TS_MASK			GENMASK(24, 0)
#define BLOCK_TS(x)			((x) & GENMASK(24, 0))
#define BLOCK_ON			BIT(31)

/* TRF_CFG */
#define TRF_CFG_SRC_MT_MASK		GENMASK(1, 0)
#define TRF_CFG_SRC_MT(x)		((x) << 0)
#define TRF_CFG_DST_MT_MASK		GENMASK(5, 4)
#define TRF_CFG_DST_MT(x)		((x) << 4)
#define TRF_CFG_TT_FC_MASK		GENMASK(9, 8)
#define TRF_CFG_TT_FC(x)		((x) << 8)
#define TRF_CFG_STRIDE_EN_MASK		BIT(12)
#define TRF_CFG_STRIDE_EN		BIT(12)
#define TRF_CFG_STRIDE_DIS		0
#define TRF_CFG_STRIDE_MODE_MASK	GENMASK(14, 13)
#define TRF_CFG_STRIDE_MODE_TRANS	(0 << 13)
#define TRF_CFG_STRIDE_MODE_SRC		(1 << 13)
#define TRF_CFG_STRIDE_MODE_DST		(2 << 13)

/* BLOCK_CS */
#define BLOCK_CS_MASK			GENMASK(24, 0)
#define BLOCK_CS(x)			((x) & GENMASK(24, 0))

/* STRIDE_CH */
#define STRIDE_CH_SIZE_MASK		GENMASK(11, 8)
#define STRIDE_CH_SIZE(x)		((x) << 8)
#define STRIDE_CH_NUM_MASK		GENMASK(7, 0)
#define STRIDE_CH_NUM(x)		((x) << 0)

/* STRIDE_INC */
#define STRIDE_INC_MASK			GENMASK(23, 0)
#define STRIDE_INC(x)			((x) << 0)

#define to_rk_dma(dmadev) container_of(dmadev, struct rk_dma_dev, slave)

enum rk_dma_mt_transfer_type {
	DMA_MT_TRANSFER_CONTIGUOUS,
	DMA_MT_TRANSFER_AUTO_RELOAD,
	DMA_MT_TRANSFER_LINK_LIST,
};

enum rk_dma_lch_state {
	DMA_LCH_IDLE,
	DMA_LCH_BLOCK_START,
	DMA_LCH_PERI_WAIT,
	DMA_LCH_BIND_REQ,
	DMA_LCH_TRANS,
	DMA_LCH_MEM_BDONE,
	DMA_LCH_PERI_BDONE,
	DMA_LCH_PERI_BNDONE,
	DMA_LCH_BLOCK_DONE,
	DMA_LCH_SUSPD,
	DMA_LCH_ERR,
	DMA_LCH_DONE,
};

enum rk_dma_pch_trans_state {
	DMA_PCH_TRANS_IDLE,
	DMA_PCH_TRANS_CHK_REQ,
	DMA_PCH_TRANS_REQ,
	DMA_PCH_TRANS_RESP,
	DMA_PCH_TRANS_END,
	DMA_PCH_TRANS_ERR,
};

enum rk_dma_pch_cmd_state {
	DMA_PCH_CMD_IDLE,
	DMA_PCH_CMD_ENTRY_RD,
	DMA_PCH_CMD_LLI_RD,
	DMA_PCH_CMD_DAT_TRANS,
	DMA_PCH_CMD_ENTRY_WR,
	DMA_PCH_CMD_REL,
	DMA_PCH_CMD_ERR,
};

enum rk_dma_burst_width {
	DMA_BURST_WIDTH_1_BYTE,
	DMA_BURST_WIDTH_2_BYTES,
	DMA_BURST_WIDTH_4_BYTES,
	DMA_BURST_WIDTH_8_BYTES,
	DMA_BURST_WIDTH_16_BYTES,
};

struct rk_desc_hw {
	u32 trf_ctl0;
	u32 trf_ctl1;
	u32 sar;
	u32 dar;
	u32 block_ts;
	u32 llp_nxt;
	u16 sar_high;
	u16 dar_high;
	u16 llp_nxt_high;
	u16 lli_idx;
	u32 trf_cfg;
	u32 block_cs;
	u32 sar_reload;
	u32 dar_reload;
	u16 sar_reload_high;
	u16 dar_reload_high;
	u8  stride_ch_num;
	u8  stride_ch_size;
	u16 rsv0;
	u32 stride_inc;
	u32 rsv1;
} __aligned(32);

struct rk_dma_desc_sw {
	struct virt_dma_desc	vd;
	struct rk_desc_hw	*desc_hw;
	dma_addr_t		desc_hw_lli;
	size_t			desc_num;
	size_t			size;
	enum dma_transfer_direction dir;
};

struct rk_dma_lch;

struct rk_dma_chan {
	struct virt_dma_chan	vc;
	struct rk_dma_lch	*lch;
	struct list_head	node;
	struct dma_slave_config slave_cfg;
	u32			id; /* lch chan id */
	u32			ctl0;
	u32			ctl1;
	u32			ccfg;
	u32			cyclic;
	dma_addr_t		dev_addr;
	enum dma_status		status;
};

struct rk_dma_lch {
	struct rk_dma_chan	*vchan;
	struct rk_dma_desc_sw	*ds_run;
	struct rk_dma_desc_sw	*ds_done;
	void __iomem		*base;
	u32			id;
};

struct rk_dma_dev {
	struct dma_device	slave;
	struct list_head	chan_pending;
	struct rk_dma_lch	*lch;
	struct rk_dma_chan	*chans;
	struct clk_bulk_data	*clks;
	struct dma_pool		*pool;
	struct gen_pool		*gpool;
	void __iomem		*base;
	int			irq;
	int			num_clks;
	u32			bus_width;
	u32			buf_dep;
	u32			dma_channels;
	u32			dma_requests;
	u32			version;
	spinlock_t		lock; /* lock for ch and lch */
};

static struct rk_dma_chan *to_rk_chan(struct dma_chan *chan)
{
	return container_of(chan, struct rk_dma_chan, vc.chan);
}

static void rk_dma_terminate_chan(struct rk_dma_lch *l, struct rk_dma_dev *d)
{
	writel(LCH_CTL0_CH_DIS, RK_DMA_LCH_CTL0);
	writel(0x0, RK_DMA_LCH_IE);
	writel(readl(RK_DMA_LCH_IS), RK_DMA_LCH_IS);
}

static void rk_dma_set_desc(struct rk_dma_chan *c, struct rk_dma_desc_sw *ds)
{
	struct rk_dma_lch *l = c->lch;

	writel(LCH_CTL0_CH_EN, RK_DMA_LCH_CTL0);

	if (c->cyclic)
		writel(LCH_IE_BLOCK_DONE_IE_EN, RK_DMA_LCH_IE);
	else
		writel(LCH_IE_DMA_DONE_IE_EN, RK_DMA_LCH_IE);

	writel(ds->desc_hw_lli, RK_DMA_LCH_CMDBA);
	writel(LCH_TRF_CMD_DST_MT(DMA_MT_TRANSFER_LINK_LIST) |
	       LCH_TRF_CMD_SRC_MT(DMA_MT_TRANSFER_LINK_LIST) |
	       LCH_TRF_CMD_TT_FC(ds->dir) | LCH_TRF_CMD_DMA_START,
	       RK_DMA_LCH_TRF_CMD);

	dev_dbg(c->vc.chan.device->dev,
		"%s: id: %d, desc_sw: %px, desc_hw_lli: %pad\n",
		__func__, l->id, ds, &ds->desc_hw_lli);
}

static u32 rk_dma_get_chan_stat(struct rk_dma_lch *l)
{
	return readl(RK_DMA_LCH_CTL0);
}

static int rk_dma_init(struct rk_dma_dev *d)
{
	struct device *dev = d->slave.dev;
	int i, lch, pch, buswidth, maxburst, dep, addrwidth;
	u32 cap0, cap1, ver;

	writel(CMN_CFG_EN | CMN_CFG_IE_EN, RK_DMA_CMN_CFG);

	ver  = readl(RK_DMA_CMN_VER);
	cap0 = readl(RK_DMA_CMN_CAP0);
	cap1 = readl(RK_DMA_CMN_CAP1);

	lch = CMN_LCH_NUM(cap0);
	pch = CMN_PCH_NUM(cap0);
	dep = CMN_BUF_DEPTH(cap0);

	addrwidth = CMN_AXADDR_WIDTH(cap1);
	buswidth = CMN_AXI_SIZE(cap1);
	maxburst = CMN_AXI_LEN(cap1);

	d->version = ver;
	d->bus_width = buswidth;
	d->buf_dep = dep;
	d->dma_channels = CMN_LCH_NUM(cap0);
	d->dma_requests = CMN_LCH_NUM(cap0);

	writel(0xffffffff, RK_DMA_CMN_DYNCTL);
	writel(0xffffffff, RK_DMA_CMN_IS0);
	writel(0xffffffff, RK_DMA_CMN_IS1);

	for (i = 0; i < pch; i++)
		writel(CMN_PCH_EN(i), RK_DMA_CMN_PCH_EN);

	dev_info(dev, "NR_LCH-%d NR_PCH-%d PCH_BUF-%dx%dBytes AXI_LEN-%d ADDR-%dBits V%lu.%lu\n",
		 lch, pch, dep, buswidth, maxburst, addrwidth,
		 CMN_VER_MAJOR(ver), CMN_VER_MINOR(ver));

	return 0;
}

static int rk_dma_start_txd(struct rk_dma_chan *c)
{
	struct virt_dma_desc *vd = vchan_next_desc(&c->vc);

	if (!c->lch)
		return -EAGAIN;

	if (rk_dma_get_chan_stat(c->lch))
		return -EAGAIN;

	if (vd) {
		struct rk_dma_desc_sw *ds = container_of(vd, struct rk_dma_desc_sw, vd);
		/*
		 * fetch and remove request from vc->desc_issued
		 * so vc->desc_issued only contains desc pending
		 */
		list_del(&ds->vd.node);
		c->lch->ds_run = ds;
		c->lch->ds_done = NULL;
		/* start dma */
		rk_dma_set_desc(c, ds);
		return 0;
	}

	c->lch->ds_done = NULL;
	c->lch->ds_run = NULL;

	return -EAGAIN;
}

static void rk_dma_task(struct rk_dma_dev *d)
{
	struct rk_dma_lch *l;
	struct rk_dma_chan *c, *cn;
	unsigned int i = 0;
	unsigned long flags;
	u64 lch_alloc = 0;

	/* check new dma request of running channel in vc->desc_issued */
	list_for_each_entry_safe(c, cn, &d->slave.channels, vc.chan.device_node) {
		spin_lock_irqsave(&c->vc.lock, flags);
		l = c->lch;
		if (l && l->ds_done && rk_dma_start_txd(c)) {
			dev_dbg(d->slave.dev, "lch-%u: free\n", l->id);
			rk_dma_terminate_chan(l, d);
			c->lch = NULL;
			l->vchan = NULL;
		}
		spin_unlock_irqrestore(&c->vc.lock, flags);
	}

	/* check new channel request in d->chan_pending */
	spin_lock_irqsave(&d->lock, flags);
	while (!list_empty(&d->chan_pending)) {
		c = list_first_entry(&d->chan_pending, struct rk_dma_chan, node);
		l = &d->lch[c->id];
		if (!l->vchan) {
			list_del_init(&c->node);
			lch_alloc |= BIT_ULL(c->id);
			l->vchan = c;
			c->lch = l;
		} else {
			dev_dbg(d->slave.dev, "lch-%u: busy\n", l->id);
		}
	}
	spin_unlock_irqrestore(&d->lock, flags);

	for (i = 0; i < d->dma_channels; i++) {
		if (lch_alloc & BIT_ULL(i)) {
			l = &d->lch[i];
			c = l->vchan;
			if (c) {
				spin_lock_irqsave(&c->vc.lock, flags);
				rk_dma_start_txd(c);
				spin_unlock_irqrestore(&c->vc.lock, flags);
			}
		}
	}
}

static irqreturn_t rk_dma_irq_handler(int irq, void *dev_id)
{
	struct rk_dma_dev *d = (struct rk_dma_dev *)dev_id;
	struct rk_dma_lch *l;
	struct rk_dma_chan *c;
	u64 is = 0, is_raw = 0;
	u32 i = 0, task = 0;

	is = readq(RK_DMA_CMN_IS0);
	is_raw = is;
	while (is) {
		i = __ffs64(is);
		is &= ~BIT_ULL(i);
		l = &d->lch[i];
		c = l->vchan;
		if (c) {
			spin_lock(&c->vc.lock);
			if (l->ds_run) {
				if (c->cyclic) {
					vchan_cyclic_callback(&l->ds_run->vd);
				} else {
					vchan_cookie_complete(&l->ds_run->vd);
					l->ds_done = l->ds_run;
					task = 1;
				}
			}
			spin_unlock(&c->vc.lock);
			writel(readl(RK_DMA_LCH_IS), RK_DMA_LCH_IS);
		}
	}

	writeq(is_raw, RK_DMA_CMN_IS0);

	if (task)
		rk_dma_task(d);

	return IRQ_HANDLED;
}

static void rk_dma_free_chan_resources(struct dma_chan *chan)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_dev *d = to_rk_dma(chan->device);
	unsigned long flags;

	spin_lock_irqsave(&d->lock, flags);
	list_del_init(&c->node);
	spin_unlock_irqrestore(&d->lock, flags);

	vchan_free_chan_resources(&c->vc);
	c->ccfg = 0;
	c->ctl0 = 0;
	c->ctl1 = 0;
}

static int rk_dma_lch_get_bytes_xfered(struct rk_dma_lch *l)
{
	struct rk_dma_desc_sw *ds = l->ds_run;
	int bytes = 0;

	if (!ds)
		return 0;

	/* cmd_entry holds the current LLI being processed */
	if (ds->dir == DMA_MEM_TO_DEV)
		bytes = ds->desc_hw[0].sar - ds->desc_hw[1].sar;
	else
		bytes = ds->desc_hw[0].dar - ds->desc_hw[1].dar;

	/*
	 * The transferred bytes are calculated by subtracting first_lli.base from
	 * current position (cur_pos). However, cur_pos remains 0 until the first
	 * burst transfer completes, which could result in a negative value.
	 * This leads to incorrect byte count reporting.
	 *
	 * Fix the overflow by clamping the calculated bytes to 0 when negative,
	 * ensuring the reported transfer position matches hardware state before
	 * the first burst completion.
	 */
	if (bytes < 0)
		bytes = 0;

	return bytes;
}

static enum dma_status rk_dma_tx_status(struct dma_chan *chan,
					dma_cookie_t cookie,
					struct dma_tx_state *state)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_lch *l;
	struct virt_dma_desc *vd;
	unsigned long flags;
	enum dma_status ret;
	size_t bytes = 0;

	ret = dma_cookie_status(&c->vc.chan, cookie, state);
	if (ret == DMA_COMPLETE || !state)
		return ret;

	spin_lock_irqsave(&c->vc.lock, flags);
	l = c->lch;
	ret = c->status;

	/*
	 * If the cookie is on our issue queue, then the residue is
	 * its total size.
	 */
	vd = vchan_find_desc(&c->vc, cookie);
	if (vd) {
		bytes = container_of(vd, struct rk_dma_desc_sw, vd)->size;
	} else if ((!l) || (!l->ds_run)) {
		bytes = 0;
	} else {
		bytes = l->ds_run->size - rk_dma_lch_get_bytes_xfered(l);
	}
	spin_unlock_irqrestore(&c->vc.lock, flags);
	dma_set_residue(state, bytes);

	return ret;
}

static void rk_dma_issue_pending(struct dma_chan *chan)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_dev *d = to_rk_dma(chan->device);
	unsigned long flags;
	int issue = 0;

	spin_lock_irqsave(&c->vc.lock, flags);
	/* add request to vc->desc_issued */
	if (vchan_issue_pending(&c->vc)) {
		spin_lock(&d->lock);
		if (!c->lch && list_empty(&c->node)) {
			/* if new channel, add chan_pending */
			list_add_tail(&c->node, &d->chan_pending);
			issue = 1;
			dev_dbg(d->slave.dev, "vch-%px: id-%u issued\n", &c->vc, c->id);
		}
		spin_unlock(&d->lock);
	} else {
		dev_dbg(d->slave.dev, "vch-%px: nothing to issue\n", &c->vc);
	}
	spin_unlock_irqrestore(&c->vc.lock, flags);

	if (issue)
		rk_dma_task(d);
}

static void rk_dma_fill_desc(struct rk_dma_desc_sw *ds, dma_addr_t dst,
			     dma_addr_t src, size_t len, u32 num, u32 cc0, u32 cc1, u32 ccfg)
{
	/* assign llp_nxt for cmd_entry */
	if (num == 0) {
		ds->desc_hw[0].llp_nxt = ds->desc_hw_lli + sizeof(struct rk_desc_hw);
		ds->desc_hw[0].trf_cfg = ccfg;

		return;
	}

	if ((num + 1) < ds->desc_num)
		ds->desc_hw[num].llp_nxt = ds->desc_hw_lli + (num + 1) * sizeof(struct rk_desc_hw);

	ds->desc_hw[num].sar = src;
	ds->desc_hw[num].dar = dst;
	ds->desc_hw[num].block_ts = BLOCK_TS(len);
	ds->desc_hw[num].trf_ctl0 = cc0;
	ds->desc_hw[num].trf_ctl1 = cc1;
}

static void *rk_dma_pool_alloc(struct rk_dma_dev *d, struct rk_dma_desc_sw *ds)
{
	size_t size = ds->desc_num * sizeof(struct rk_desc_hw);

	if (d->gpool)
		return gen_pool_dma_zalloc(d->gpool, size, &ds->desc_hw_lli);

	return dma_pool_zalloc(d->pool, GFP_NOWAIT, &ds->desc_hw_lli);
}

static struct rk_dma_desc_sw *rk_alloc_desc_resource(int num, struct dma_chan *chan)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_desc_sw *ds;
	struct rk_dma_dev *d = to_rk_dma(chan->device);
	int lli_limit = LLI_BLOCK_SIZE / sizeof(struct rk_desc_hw);

	if (num > lli_limit) {
		dev_err(chan->device->dev, "vch-%px: sg num %d exceed max %d\n",
			&c->vc, num, lli_limit);
		return NULL;
	}

	ds = kzalloc(sizeof(*ds), GFP_ATOMIC);
	if (!ds)
		return NULL;

	ds->desc_num = num;
	ds->desc_hw = rk_dma_pool_alloc(d, ds);
	if (!ds->desc_hw) {
		dev_err(chan->device->dev, "vch-%px: dma alloc fail\n", &c->vc);
		kfree(ds);
		return NULL;
	}

	dev_dbg(chan->device->dev, "vch-%px, desc_sw: %px, desc_hw_lli: %pad\n",
		&c->vc, ds, &ds->desc_hw_lli);

	return ds;
}

static enum rk_dma_burst_width rk_dma_burst_width(enum dma_slave_buswidth width)
{
	switch (width) {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
	case DMA_SLAVE_BUSWIDTH_8_BYTES:
	case DMA_SLAVE_BUSWIDTH_16_BYTES:
		return ffs(width) - 1;
	default:
		return DMA_BURST_WIDTH_4_BYTES;
	}
}

static int rk_pre_config(struct dma_chan *chan, enum dma_transfer_direction dir)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_dev *d = to_rk_dma(chan->device);
	struct dma_slave_config *cfg = &c->slave_cfg;
	enum rk_dma_burst_width src_width;
	enum rk_dma_burst_width dst_width;
	u32 maxburst = 0;

	switch (dir) {
	case DMA_MEM_TO_MEM:
		/* DMAC use the min(addr_align, bus_width, len) automatically */
		src_width = rk_dma_burst_width(d->bus_width);
		maxburst = min_t(u32, d->buf_dep, RK_MAX_BURST_LEN);
		c->ctl0 = TRF_CTL0_LLI_VALID | TRF_CTL0_MSIZE(0);
		c->ctl1 = TRF_CTL1_AROSR(4) |
			  TRF_CTL1_AWOSR(4) |
			  TRF_CTL1_ARLEN(maxburst) |
			  TRF_CTL1_AWLEN(maxburst) |
			  TRF_CTL1_ARSIZE(src_width) |
			  TRF_CTL1_AWSIZE(src_width) |
			  TRF_CTL1_ARBURST_INCR |
			  TRF_CTL1_AWBURST_INCR;
		c->ccfg = TRF_CFG_TT_FC(DMA_MEM_TO_MEM) |
			  TRF_CFG_DST_MT(DMA_MT_TRANSFER_LINK_LIST) |
			  TRF_CFG_SRC_MT(DMA_MT_TRANSFER_LINK_LIST);
		break;
	case DMA_MEM_TO_DEV:
		c->dev_addr = cfg->dst_addr;
		/* dst len is calculated from src width, len and dst width.
		 * We need make sure dst len not exceed MAX LEN.
		 * Trailing single transaction that does not fill a full
		 * burst also require identical src/dst data width.
		 */
		dst_width = rk_dma_burst_width(cfg->dst_addr_width);
		maxburst = min3(cfg->dst_maxburst, d->buf_dep, (u32)RK_MAX_BURST_LEN);

		c->ctl0 = TRF_CTL0_MSIZE(maxburst * cfg->dst_addr_width) |
			  TRF_CTL0_LLI_VALID;
		if (c->cyclic)
			c->ctl0 |= TRF_CTL0_IOC_EN;
		c->ctl1 = TRF_CTL1_AROSR(4) |
			  TRF_CTL1_AWOSR(4) |
			  TRF_CTL1_ARLEN(maxburst) |
			  TRF_CTL1_AWLEN(maxburst) |
			  TRF_CTL1_ARSIZE(dst_width) |
			  TRF_CTL1_AWSIZE(dst_width) |
			  TRF_CTL1_ARBURST_INCR |
			  TRF_CTL1_AWBURST_FIXED;
		c->ccfg = TRF_CFG_TT_FC(DMA_MEM_TO_DEV) |
			  TRF_CFG_DST_MT(DMA_MT_TRANSFER_LINK_LIST) |
			  TRF_CFG_SRC_MT(DMA_MT_TRANSFER_LINK_LIST);
		break;
	case DMA_DEV_TO_MEM:
		c->dev_addr = cfg->src_addr;
		src_width = rk_dma_burst_width(cfg->src_addr_width);
		maxburst = min3(cfg->src_maxburst, d->buf_dep, (u32)RK_MAX_BURST_LEN);

		c->ctl0 = TRF_CTL0_MSIZE(maxburst * cfg->src_addr_width) |
			  TRF_CTL0_LLI_VALID;
		if (c->cyclic)
			c->ctl0 |= TRF_CTL0_IOC_EN;
		c->ctl1 = TRF_CTL1_AROSR(4) |
			  TRF_CTL1_AWOSR(4) |
			  TRF_CTL1_ARLEN(maxburst) |
			  TRF_CTL1_AWLEN(maxburst) |
			  TRF_CTL1_ARSIZE(src_width) |
			  TRF_CTL1_AWSIZE(src_width) |
			  TRF_CTL1_ARBURST_FIXED |
			  TRF_CTL1_AWBURST_INCR;
		c->ccfg = TRF_CFG_TT_FC(DMA_DEV_TO_MEM) |
			  TRF_CFG_DST_MT(DMA_MT_TRANSFER_LINK_LIST) |
			  TRF_CFG_SRC_MT(DMA_MT_TRANSFER_LINK_LIST);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static struct dma_async_tx_descriptor *rk_dma_prep_memcpy(
	struct dma_chan *chan,	dma_addr_t dst, dma_addr_t src,
	size_t len, unsigned long flags)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_desc_sw *ds;
	size_t copy = 0;
	int num = 0;

	if (!len)
		return NULL;

	if (rk_pre_config(chan, DMA_MEM_TO_MEM))
		return NULL;

	num = DIV_ROUND_UP(len, DMA_MAX_SIZE);

	/* one more for cmd entry */
	num++;

	ds = rk_alloc_desc_resource(num, chan);
	if (!ds)
		return NULL;

	ds->size = len;
	ds->dir = DMA_MEM_TO_MEM;

	/* the first one used as cmd entry */
	rk_dma_fill_desc(ds, dst, src, copy, 0, c->ctl0, c->ctl1, c->ccfg);
	num = 1;

	do {
		copy = min_t(size_t, len, DMA_MAX_SIZE);
		rk_dma_fill_desc(ds, dst, src, copy, num++, c->ctl0, c->ctl1, c->ccfg);

		src += copy;
		dst += copy;
		len -= copy;
	} while (len);

	ds->desc_hw[num - 1].llp_nxt = 0;
	ds->desc_hw[num - 1].trf_ctl0 |= TRF_CTL0_LLI_LAST;

	c->cyclic = 0;

	return vchan_tx_prep(&c->vc, &ds->vd, flags);
}

static struct dma_async_tx_descriptor *
rk_dma_prep_slave_sg(struct dma_chan *chan, struct scatterlist *sgl, unsigned int sglen,
		     enum dma_transfer_direction dir, unsigned long flags, void *context)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_desc_sw *ds;
	size_t len, avail, total = 0;
	struct scatterlist *sg;
	dma_addr_t addr, src = 0, dst = 0;
	int num = sglen, i;

	if (!sgl)
		return NULL;

	if (rk_pre_config(chan, dir))
		return NULL;

	for_each_sg(sgl, sg, sglen, i) {
		avail = sg_dma_len(sg);
		if (avail > DMA_MAX_SIZE)
			num += DIV_ROUND_UP(avail, DMA_MAX_SIZE) - 1;
	}

	ds = rk_alloc_desc_resource(num + 1, chan);
	if (!ds)
		return NULL;

	c->cyclic = 0;
	/* the first one used as cmd entry */
	rk_dma_fill_desc(ds, dst, src, 0, 0, c->ctl0, c->ctl1, c->ccfg);
	num = 1;

	for_each_sg(sgl, sg, sglen, i) {
		addr = sg_dma_address(sg);
		avail = sg_dma_len(sg);
		total += avail;

		do {
			len = min_t(size_t, avail, DMA_MAX_SIZE);

			if (dir == DMA_MEM_TO_DEV) {
				src = addr;
				dst = c->dev_addr;
			} else if (dir == DMA_DEV_TO_MEM) {
				src = c->dev_addr;
				dst = addr;
			}

			rk_dma_fill_desc(ds, dst, src, len, num++, c->ctl0, c->ctl1, c->ccfg);

			addr += len;
			avail -= len;
		} while (avail);
	}

	ds->desc_hw[num - 1].llp_nxt = 0;	/* end of link */
	ds->desc_hw[num - 1].trf_ctl0 |= TRF_CTL0_LLI_LAST;
	ds->size = total;
	ds->dir = dir;
	return vchan_tx_prep(&c->vc, &ds->vd, flags);
}

static struct dma_async_tx_descriptor *
rk_dma_prep_dma_cyclic(struct dma_chan *chan, dma_addr_t dma_addr, size_t buf_len,
		       size_t period_len, enum dma_transfer_direction dir,
		       unsigned long flags)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_desc_sw *ds;
	dma_addr_t src = 0, dst = 0;
	int num_periods = buf_len / period_len;
	int buf = 0, num = 0;

	if (period_len > DMA_MAX_SIZE) {
		dev_err(chan->device->dev, "maximum period size exceeded\n");
		return NULL;
	}

	c->cyclic = 1;
	if (rk_pre_config(chan, dir))
		return NULL;

	ds = rk_alloc_desc_resource(num_periods + 1, chan);
	if (!ds)
		return NULL;

	/* the first one used as cmd entry */
	rk_dma_fill_desc(ds, dst, src, 0, 0, c->ctl0, c->ctl1, c->ccfg);
	num = 1;

	while (buf < buf_len) {
		if (dir == DMA_MEM_TO_DEV) {
			src = dma_addr;
			dst = c->dev_addr;
		} else if (dir == DMA_DEV_TO_MEM) {
			src = c->dev_addr;
			dst = dma_addr;
		}
		rk_dma_fill_desc(ds, dst, src, period_len, num++, c->ctl0, c->ctl1, c->ccfg);
		dma_addr += period_len;
		buf += period_len;
	}

	ds->desc_hw[num - 1].llp_nxt = ds->desc_hw_lli + sizeof(struct rk_desc_hw);
	ds->desc_hw[num - 1].trf_ctl0 |= TRF_CTL0_CNT_CLR;
	ds->size = buf_len;
	ds->dir = dir;
	return vchan_tx_prep(&c->vc, &ds->vd, flags);
}

static struct dma_async_tx_descriptor *
rk_dma_prep_interleaved_dma(struct dma_chan *chan, struct dma_interleaved_template *xt,
			   unsigned long flags)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_desc_sw *ds;
	size_t size, src_icg, dst_icg, period_bytes, full_period_bytes, full_buffer_bytes;
	size_t nump = 0, numf = 0;
	int num_periods, num = 0;
	dma_addr_t src = 0, dst = 0, dma_addr = 0;

	if (!xt->numf || !xt->sgl[0].size || xt->frame_size != 1)
		return NULL;

#ifdef CONFIG_NO_GKI
	nump = xt->nump;
#else
	nump = xt->sgl[1].size;
#endif
	numf = xt->numf;
	size = xt->sgl[0].size;
	period_bytes = size * nump;

	if (flags & DMA_PREP_REPEAT && (!nump || (numf % nump)))
		return NULL;

	if (period_bytes > DMA_MAX_SIZE) {
		dev_err(chan->device->dev, "maximum period size exceeded\n");
		return NULL;
	}

	src_icg = dmaengine_get_src_icg(xt, &xt->sgl[0]);
	dst_icg = dmaengine_get_dst_icg(xt, &xt->sgl[0]);

	if (flags & DMA_PREP_REPEAT) {
		num_periods = numf / nump;
		c->cyclic = 1;
	} else {
		num_periods = 1;
		c->cyclic = 0;
	}

	if (rk_pre_config(chan, xt->dir))
		return NULL;

	ds = rk_alloc_desc_resource(num_periods + 1, chan);
	if (!ds)
		return NULL;

	c->ctl0 &= ~TRF_CTL0_MSIZE_MASK;
	c->ctl0 |= TRF_CTL0_MSIZE(size);
	c->ccfg |= TRF_CFG_STRIDE_EN | TRF_CFG_STRIDE_MODE_TRANS;
	if (xt->dir == DMA_MEM_TO_DEV) {
		ds->desc_hw[0].stride_inc = STRIDE_INC(src_icg);
		dma_addr = xt->src_start;
		full_period_bytes = (size + src_icg) * nump;
		full_buffer_bytes = (size + src_icg) * numf;
	} else {
		ds->desc_hw[0].stride_inc = STRIDE_INC(dst_icg);
		dma_addr = xt->dst_start;
		full_period_bytes = (size + dst_icg) * nump;
		full_buffer_bytes = (size + dst_icg) * numf;
	}

	/* the first one used as cmd entry */
	rk_dma_fill_desc(ds, dst, src, 0, 0, c->ctl0, c->ctl1, c->ccfg);
	num = 1;

	while (num < (num_periods + 1)) {
		if (xt->dir == DMA_MEM_TO_DEV) {
			src = dma_addr;
			dst = c->dev_addr;
		} else if (xt->dir == DMA_DEV_TO_MEM) {
			src = c->dev_addr;
			dst = dma_addr;
		}
		rk_dma_fill_desc(ds, dst, src, period_bytes, num++, c->ctl0, c->ctl1, c->ccfg);
		dma_addr += full_period_bytes;
	}

	ds->desc_hw[num - 1].llp_nxt = ds->desc_hw_lli + sizeof(struct rk_desc_hw);
	ds->desc_hw[num - 1].trf_ctl0 |= TRF_CTL0_CNT_CLR;
	ds->size = full_buffer_bytes;
	ds->dir = xt->dir;

	dev_dbg(chan->device->dev, "size: %zu, src_icg: %zu, dst_icg: %zu, nump: %zu, numf: %zu\n",
		size, src_icg, dst_icg, nump, numf);

	return vchan_tx_prep(&c->vc, &ds->vd, flags);
}

static int rk_dma_config(struct dma_chan *chan, struct dma_slave_config *cfg)
{
	struct rk_dma_chan *c = to_rk_chan(chan);

	if (!cfg)
		return -EINVAL;

	memcpy(&c->slave_cfg, cfg, sizeof(*cfg));

	return 0;
}

static int rk_dma_terminate_all(struct dma_chan *chan)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_dev *d = to_rk_dma(chan->device);
	struct rk_dma_lch *l;
	unsigned long flags;
	LIST_HEAD(head);

	dev_dbg(d->slave.dev, "vch-%px: terminate all\n", &c->vc);

	spin_lock_irqsave(&d->lock, flags);
	list_del_init(&c->node);
	spin_unlock_irqrestore(&d->lock, flags);

	spin_lock_irqsave(&c->vc.lock, flags);
	l = c->lch;
	if (l) {
		rk_dma_terminate_chan(l, d);
		if (l->ds_run)
			vchan_terminate_vdesc(&l->ds_run->vd);

		c->lch = NULL;
		l->vchan = NULL;
		l->ds_run = NULL;
		l->ds_done = NULL;
	}
	vchan_get_all_descriptors(&c->vc, &head);
	spin_unlock_irqrestore(&c->vc.lock, flags);

	vchan_dma_desc_free_list(&c->vc, &head);

	return 0;
}

static int rk_dma_transfer_pause(struct dma_chan *chan)
{
	//TBD
	return 0;
}

static int rk_dma_transfer_resume(struct dma_chan *chan)
{
	struct rk_dma_chan *c = to_rk_chan(chan);
	struct rk_dma_lch *l;
	unsigned long flags;

	spin_lock_irqsave(&c->vc.lock, flags);
	l = c->lch;
	if (l)
		writel(LCH_TRF_CMD_DMA_RESUME, RK_DMA_LCH_TRF_CMD);
	spin_unlock_irqrestore(&c->vc.lock, flags);

	return 0;
}

static void rk_dma_pool_free(struct rk_dma_dev *d, struct rk_dma_desc_sw *ds)
{
	size_t size = ds->desc_num * sizeof(struct rk_desc_hw);

	if (d->gpool)
		return gen_pool_free(d->gpool, (unsigned long)ds->desc_hw, size);

	return dma_pool_free(d->pool, ds->desc_hw, ds->desc_hw_lli);
}

static void rk_dma_free_desc(struct virt_dma_desc *vd)
{
	struct rk_dma_dev *d = to_rk_dma(vd->tx.chan->device);
	struct rk_dma_desc_sw *ds = container_of(vd, struct rk_dma_desc_sw, vd);

	dev_dbg(d->slave.dev, "desc_sw: %px free\n", ds);

	rk_dma_pool_free(d, ds);
	kfree(ds);
}

static const struct of_device_id rk_dma_dt_ids[] = {
	{ .compatible = "rockchip,dma", },
	{}
};
MODULE_DEVICE_TABLE(of, rk_dma_dt_ids);

static struct dma_chan *rk_of_dma_simple_xlate(struct of_phandle_args *dma_spec,
					       struct of_dma *ofdma)
{
	struct rk_dma_dev *d = ofdma->of_dma_data;
	unsigned int request = dma_spec->args[0];
	struct dma_chan *chan;
	struct rk_dma_chan *c;

	if (request >= d->dma_requests)
		return NULL;

	chan = dma_get_any_slave_channel(&d->slave);
	if (!chan) {
		dev_err(d->slave.dev, "Failed to get chan for req %u\n", request);
		return NULL;
	}

	c = to_rk_chan(chan);
	c->id = request;

	dev_dbg(d->slave.dev, "Xlate lch-%u for req-%u\n", c->id, request);

	return chan;
}

static int rk_dma_pool_create(struct rk_dma_dev *d, struct device *dev)
{
	d->gpool = of_gen_pool_get(dev->of_node, "sram", 0);
	if (d->gpool) {
		dev_info(dev, "Use sram for dma desc\n");
		return 0;
	}

	/* A DMA memory pool for LLIs, align on 64-bytes boundary */
	d->pool = dmam_pool_create(DRIVER_NAME, dev, LLI_BLOCK_SIZE, SZ_64, 0);
	if (!d->pool)
		return -ENOMEM;

	return 0;
}

static int rk_dma_probe(struct platform_device *pdev)
{
	struct rk_dma_dev *d;
	int i, ret;

	d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(d->base))
		return PTR_ERR(d->base);

	d->num_clks = devm_clk_bulk_get_all(&pdev->dev, &d->clks);
	if (d->num_clks < 1) {
		dev_err(&pdev->dev, "Failed to get clk\n");
		return -ENODEV;
	}

	d->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, d->irq, rk_dma_irq_handler, 0, dev_name(&pdev->dev), d);
	if (ret)
		return ret;

	ret = rk_dma_pool_create(d, &pdev->dev);
	if (ret)
		return ret;

	spin_lock_init(&d->lock);
	INIT_LIST_HEAD(&d->chan_pending);
	INIT_LIST_HEAD(&d->slave.channels);
	dma_cap_set(DMA_SLAVE, d->slave.cap_mask);
	dma_cap_set(DMA_MEMCPY, d->slave.cap_mask);
	dma_cap_set(DMA_CYCLIC, d->slave.cap_mask);
	dma_cap_set(DMA_PRIVATE, d->slave.cap_mask);
	dma_cap_set(DMA_INTERLEAVE, d->slave.cap_mask);
	dma_cap_set(DMA_REPEAT, d->slave.cap_mask);
	dma_cap_set(DMA_LOAD_EOT, d->slave.cap_mask);
	d->slave.dev = &pdev->dev;
	d->slave.device_free_chan_resources = rk_dma_free_chan_resources;
	d->slave.device_tx_status = rk_dma_tx_status;
	d->slave.device_prep_dma_memcpy = rk_dma_prep_memcpy;
	d->slave.device_prep_slave_sg = rk_dma_prep_slave_sg;
	d->slave.device_prep_dma_cyclic = rk_dma_prep_dma_cyclic;
	d->slave.device_prep_interleaved_dma = rk_dma_prep_interleaved_dma;
	d->slave.device_issue_pending = rk_dma_issue_pending;
	d->slave.device_config = rk_dma_config;
	d->slave.device_terminate_all = rk_dma_terminate_all;
	d->slave.device_pause = rk_dma_transfer_pause;
	d->slave.device_resume = rk_dma_transfer_resume;
	d->slave.src_addr_widths = RK_DMA_BUSWIDTHS;
	d->slave.dst_addr_widths = RK_DMA_BUSWIDTHS;
	d->slave.directions = BIT(DMA_MEM_TO_MEM) | BIT(DMA_MEM_TO_DEV) | BIT(DMA_DEV_TO_MEM);
	d->slave.residue_granularity = DMA_RESIDUE_GRANULARITY_BURST;

	platform_set_drvdata(pdev, d);

	/* Enable clock before access registers */
	ret = clk_bulk_prepare_enable(d->num_clks, d->clks);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable clk: %d\n", ret);
		return ret;
	}

	rk_dma_init(d);

	/* init lch channel */
	d->lch = devm_kcalloc(&pdev->dev, d->dma_channels, sizeof(struct rk_dma_lch), GFP_KERNEL);
	if (!d->lch) {
		ret = -ENOMEM;
		goto err_disable_clk;
	}

	for (i = 0; i < d->dma_channels; i++) {
		struct rk_dma_lch *l = &d->lch[i];

		l->id = i;
		l->base = RK_DMA_LCHn_REG(i, 0);
	}

	/* init virtual channel */
	d->chans = devm_kcalloc(&pdev->dev, d->dma_requests,
				sizeof(struct rk_dma_chan), GFP_KERNEL);
	if (!d->chans) {
		ret = -ENOMEM;
		goto err_disable_clk;
	}

	for (i = 0; i < d->dma_requests; i++) {
		struct rk_dma_chan *c = &d->chans[i];

		c->status = DMA_IN_PROGRESS;
		c->id = i;
		INIT_LIST_HEAD(&c->node);
		c->vc.desc_free = rk_dma_free_desc;
		vchan_init(&c->vc, &d->slave);
	}

	ret = dmaenginem_async_device_register(&d->slave);
	if (ret)
		goto err_disable_clk;

	ret = of_dma_controller_register((&pdev->dev)->of_node, rk_of_dma_simple_xlate, d);
	if (ret)
		goto err_disable_clk;

	return 0;

err_disable_clk:
	clk_bulk_disable_unprepare(d->num_clks, d->clks);

	return ret;
}

static int rk_dma_remove(struct platform_device *pdev)
{
	struct rk_dma_chan *c, *cn;
	struct rk_dma_dev *d = platform_get_drvdata(pdev);

	of_dma_controller_free((&pdev->dev)->of_node);

	list_for_each_entry_safe(c, cn, &d->slave.channels, vc.chan.device_node) {
		list_del(&c->vc.chan.device_node);
	}
	clk_bulk_disable_unprepare(d->num_clks, d->clks);

	return 0;
}

static int rk_dma_suspend_dev(struct device *dev)
{
	struct rk_dma_dev *d = dev_get_drvdata(dev);

	//TBD dma all chan idle
	clk_bulk_disable_unprepare(d->num_clks, d->clks);

	return 0;
}

static int rk_dma_resume_dev(struct device *dev)
{
	struct rk_dma_dev *d = dev_get_drvdata(dev);
	int ret = 0;

	ret = clk_bulk_prepare_enable(d->num_clks, d->clks);
	if (ret < 0) {
		dev_err(d->slave.dev, "Failed to enable clk: %d\n", ret);
		return ret;
	}

	rk_dma_init(d);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(rk_dma_pmops, rk_dma_suspend_dev, rk_dma_resume_dev);

static struct platform_driver rk_pdma_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.pm	= pm_sleep_ptr(&rk_dma_pmops),
		.of_match_table = rk_dma_dt_ids,
	},
	.probe		= rk_dma_probe,
	.remove		= rk_dma_remove,
};

#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT
static int __init rk_pdma_driver_init(void)
{
	return platform_driver_register(&rk_pdma_driver);
}
arch_initcall_sync(rk_pdma_driver_init);

static void __exit rk_pdma_driver_exit(void)
{
	platform_driver_unregister(&rk_pdma_driver);
}
module_exit(rk_pdma_driver_exit);
#else
module_platform_driver(rk_pdma_driver);
#endif

MODULE_DESCRIPTION("Rockchip DMA Driver");
MODULE_AUTHOR("Sugar.Zhang@rock-chips.com");
MODULE_LICENSE("GPL");
