/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __LINUX_MFD_ROCKCHIP_FLEXBUS_H__
#define __LINUX_MFD_ROCKCHIP_FLEXBUS_H__

#define FLEXBUS_ENR			0x000
#define FLEXBUS_FREE_SCLK		0x004
#define FLEXBUS_CSN_CFG			0x008
#define FLEXBUS_COM_CTL			0x00C
#define FLEXBUS_REMAP			0x010
#define FLEXBUS_STOP			0x014
#define FLEXBUS_SLAVE_MODE		0x018
#define FLEXBUS_DVP_POL			0x01C
#define FLEXBUS_DVP_CROP_SIZE		0x020
#define FLEXBUS_DVP_CROP_START		0x024
#define FLEXBUS_DVP_ORDER		0x028
#define FLEXBUS_DVP_YUV2RGB		0x02C
#define FLEXBUS_TX_CTL			0x040
#define FLEXBUS_TX_NUM			0x044
#define FLEXBUS_TXWAT_START		0x048
#define FLEXBUS_TXFIFO_DNUM		0x04C
#define FLEXBUS_TX_WIDTH		0x050
#define FLEXBUS_TX_CSN_DUMMY		0x054
#define FLEXBUS_TX_CMD_LEN		0x058
#define FLEXBUS_TX_CMD0			0x05C
#define FLEXBUS_TX_CMD1			0x060
#define FLEXBUS_RX_CTL			0x080
#define FLEXBUS_RX_NUM			0x084
#define FLEXBUS_RXFIFO_DNUM		0x088
#define FLEXBUS_DLL_EN			0x08C
#define FLEXBUS_DLL_NUM			0x090
#define FLEXBUS_RXCLK_DUMMY		0x094
#define FLEXBUS_RXCLK_CAP_CNT		0x098
#define FLEXBUS_DMA_RD_OUTSTD		0x100
#define FLEXBUS_DMA_WR_OUTSTD		0x104
#define FLEXBUS_DMA_SRC_ADDR0		0x108
#define FLEXBUS_DMA_DST_ADDR0		0x10C
#define FLEXBUS_DMA_SRC_ADDR1		0x110
#define FLEXBUS_DMA_DST_ADDR1		0x114
#define FLEXBUS_DMA_SRC_LEN0		0x118
#define FLEXBUS_DMA_DST_LEN0		0x11C
#define FLEXBUS_DMA_SRC_LEN1		0x120
#define FLEXBUS_DMA_DST_LEN1		0x124
#define FLEXBUS_DMA_WAT_INT		0x128
#define FLEXBUS_DMA_TIMEOUT		0x12C
#define FLEXBUS_DMA_RD_LEN		0x130
#define FLEXBUS_STATUS			0x160
#define FLEXBUS_IMR			0x164
#define FLEXBUS_RISR			0x168
#define FLEXBUS_ISR			0x16C
#define FLEXBUS_ICR			0x170
#define FLEXBUS_REVISION		0x1F0

/* Bit fields in ENR */
#define FLEXBUS_RX_ENR			(BIT(16 + 1) | BIT(1))
#define FLEXBUS_RX_DIS			BIT(16 + 1)
#define FLEXBUS_TX_ENR			(BIT(16) | BIT(0))
#define FLEXBUS_TX_DIS			BIT(16)

/* Bit fields in FREE_SCLK */
#define FLEXBUS_RX_FREE_MODE		(BIT(16 + 1) | BIT(1))
#define FLEXBUS_TX_FREE_MODE		(BIT(16) | BIT(0))

/* Bit fields in COM_CTL */
#define FLEXBUS_TX_AND_RX		0x0
#define FLEXBUS_TX_ONLY			0x1
#define FLEXBUS_RX_ONLY			0x2
#define FLEXBUS_TX_THEN_RX		0x3
#define FLEXBUS_SCLK_SHARE		BIT(2)
#define FLEXBUS_TX_USE_RX		BIT(3)

/* Bit fields in SLAVE_MODE */
#define FLEXBUS_DVP_SEL			BIT(1)
#define FLEXBUS_CLK1_IN			BIT(0)

/* Bit fields in TX_CTL and RX_CTL */
#define FLEXBUS_CONTINUE_MODE		BIT(4)
#define FLEXBUS_CPOL			BIT(3)
#define FLEXBUS_CPHA			BIT(2)

/* Bit fields in TX_CTL */
#define FLEXBUS_TX_CTL_UNIT_BYTE	BIT(14)
#define FLEXBUS_TX_CTL_MSB		BIT(13)
#define FLEXBUS_TX_CTL_CPHA_SHIFT	2

/* Bit fields in RX_CTL */
#define FLEXBUS_RX_CTL_FILL_DUMMY	BIT(17)
#define FLEXBUS_RX_CTL_UNIT_BYTE	BIT(16)
#define FLEXBUS_RX_CTL_MSB		BIT(15)
#define FLEXBUS_AUTOPAD			BIT(14)
#define FLEXBUS_RXD_DY			BIT(5)
#define FLEXBUS_RX_CTL_CPHA_SHIFT	2

/* Bit fields in DMA_WAT_INT */
#define FLEXBUS_SRC_WAT_LVL_MASK	0x3
#define FLEXBUS_SRC_WAT_LVL_SHIFT	2
#define FLEXBUS_DST_WAT_LVL_MASK	0x3
#define FLEXBUS_DST_WAT_LVL_SHIFT	0

/* Bit fields in IMR, RISR, ISR and ICR */
#define FLEXBUS_DMA_TIMEOUT_ISR		BIT(13)
#define FLEXBUS_DMA_ERR_ISR		BIT(12)
#define FLEXBUS_DMA_DST1_ISR		BIT(11)
#define FLEXBUS_DMA_DST0_ISR		BIT(10)
#define FLEXBUS_DMA_SRC1_ISR		BIT(9)
#define FLEXBUS_DMA_SRC0_ISR		BIT(8)
#define FLEXBUS_DVP_FRAME_START_ISR	BIT(7)
#define FLEXBUS_DVP_FRAME_AB_ISR	BIT(6)
#define FLEXBUS_RX_DONE_ISR		BIT(5)
#define FLEXBUS_RX_UDF_ISR		BIT(4)
#define FLEXBUS_RX_OVF_ISR		BIT(3)
#define FLEXBUS_TX_DONE_ISR		BIT(2)
#define FLEXBUS_TX_UDF_ISR		BIT(1)
#define FLEXBUS_TX_OVF_ISR		BIT(0)

struct rockchip_flexbus;

struct rockchip_flexbus_dfs_reg {
	u32 dfs_1bit;
	u32 dfs_2bit;
	u32 dfs_4bit;
	u32 dfs_8bit;
	u32 dfs_16bit;
	u32 dfs_mask;
};

struct rockchip_flexbus_config {
	void (*init_config)(struct rockchip_flexbus *rkfb);
	void (*grf_config)(struct rockchip_flexbus *rkfb, bool slave_mode, bool cpol, bool cpha);
	u32 txwat_start_max;
};

struct rockchip_flexbus {
	struct device		*dev;
	void __iomem		*base;
	struct regmap		*regmap_grf;
	unsigned int		opmode0;
	unsigned int		opmode1;
	struct clk_bulk_data	*clks;
	int			num_clks;
	void			*fb0_data;
	void			*fb1_data;
	void (*fb0_isr)(struct rockchip_flexbus *rkfb, u32 isr);
	void (*fb1_isr)(struct rockchip_flexbus *rkfb, u32 isr);
	struct rockchip_flexbus_dfs_reg		*dfs_reg;
	const struct rockchip_flexbus_config	*config;
};

static inline void rockchip_flexbus_set_fb0(struct rockchip_flexbus *rkfb, void *fb0_data,
					    void (*fb0_isr)(struct rockchip_flexbus *rkfb, u32 isr))
{
	rkfb->fb0_data = fb0_data;
	rkfb->fb0_isr = fb0_isr;
}

static inline void rockchip_flexbus_set_fb1(struct rockchip_flexbus *rkfb, void *fb1_data,
					    void (*fb1_isr)(struct rockchip_flexbus *rkfb, u32 isr))
{
	rkfb->fb1_data = fb1_data;
	rkfb->fb1_isr = fb1_isr;
}

unsigned int rockchip_flexbus_readl(struct rockchip_flexbus *rkfb, unsigned int reg);
void rockchip_flexbus_writel(struct rockchip_flexbus *rkfb, unsigned int reg, unsigned int val);
void rockchip_flexbus_clrbits(struct rockchip_flexbus *rkfb, unsigned int reg,
			      unsigned int clr_val);
void rockchip_flexbus_setbits(struct rockchip_flexbus *rkfb, unsigned int reg,
			      unsigned int set_val);
void rockchip_flexbus_clrsetbits(struct rockchip_flexbus *rkfb, unsigned int reg,
				 unsigned int clr_val, unsigned int set_val);

#endif /* __LINUX_MFD_ROCKCHIP_FLEXBUS_H__ */
