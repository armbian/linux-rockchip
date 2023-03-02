/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 ******************************************************************************/
#include "_pcie.h"
#include "pwr.h"
#if MAC_AX_PCIE_SUPPORT

static u32 init_cfg_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_PCIE_INIT_CFG1, R_AX_PCIE_INIT_CFG1,
	R_AX_HAXI_INIT_CFG1, R_AX_HAXI_INIT_CFG1
};

static u32 rxbd_mode_bit[MAC_AX_CHIP_ID_MAX] = {
	B_AX_RXBD_MODE, B_AX_RXBD_MODE,
	B_AX_RXBD_MODE_V1, B_AX_RXBD_MODE_V1
};

static u32 txhci_en_bit[MAC_AX_CHIP_ID_MAX] = {
	B_AX_TXHCI_EN, B_AX_TXHCI_EN,
	B_AX_TXHCI_EN_V1, B_AX_TXHCI_EN_V1
};

static u32 rxhci_en_bit[MAC_AX_CHIP_ID_MAX] = {
	B_AX_RXHCI_EN, B_AX_RXHCI_EN,
	B_AX_RXHCI_EN_V1, B_AX_RXHCI_EN_V1
};

static u32 dma_stop1_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_PCIE_DMA_STOP1, R_AX_PCIE_DMA_STOP1,
	R_AX_HAXI_DMA_STOP1, R_AX_HAXI_DMA_STOP1
};

static u32 dma_stop2_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_PCIE_DMA_STOP2, R_AX_PCIE_DMA_STOP2,
	R_AX_HAXI_DMA_STOP2, R_AX_HAXI_DMA_STOP2
};

static u32 dma_busy1_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_PCIE_DMA_BUSY1, R_AX_PCIE_DMA_BUSY1,
	R_AX_HAXI_DMA_BUSY1, R_AX_HAXI_DMA_BUSY1
};

static u32 txbd_rwptr_clr2_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_TXBD_RWPTR_CLR2, R_AX_TXBD_RWPTR_CLR2,
	R_AX_TXBD_RWPTR_CLR2_V1, R_AX_TXBD_RWPTR_CLR2_V1
};

static u32 dma_busy2_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_PCIE_DMA_BUSY2, R_AX_PCIE_DMA_BUSY2,
	R_AX_HAXI_DMA_BUSY2, R_AX_HAXI_DMA_BUSY2
};

static u32 rxbd_rwptr_clr_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_RXBD_RWPTR_CLR, R_AX_RXBD_RWPTR_CLR,
	R_AX_RXBD_RWPTR_CLR_V1, R_AX_RXBD_RWPTR_CLR_V1
};

static u32 exp_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_PCIE_EXP_CTRL, R_AX_PCIE_EXP_CTRL,
	R_AX_HAXI_EXP_CTRL, R_AX_HAXI_EXP_CTRL
};

static u32 max_tag_num_sh[MAC_AX_CHIP_ID_MAX] = {
	B_AX_MAX_TAG_NUM_SH, B_AX_MAX_TAG_NUM_SH,
	B_AX_MAX_TAG_NUM_V1_SH, B_AX_MAX_TAG_NUM_V1_SH
};

static u32 max_tag_num_msk[MAC_AX_CHIP_ID_MAX] = {
	B_AX_MAX_TAG_NUM_MSK, B_AX_MAX_TAG_NUM_MSK,
	B_AX_MAX_TAG_NUM_V1_MSK, B_AX_MAX_TAG_NUM_V1_MSK
};

static u32 dma_busy3_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_PCIE_DMA_BUSY1, R_AX_PCIE_DMA_BUSY1,
	R_AX_HAXI_DMA_BUSY3, R_AX_HAXI_DMA_BUSY3
};

static u32 ch10_txbd_num_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH10_TXBD_NUM, R_AX_CH10_TXBD_NUM,
	R_AX_CH10_TXBD_NUM_V1, R_AX_CH10_TXBD_NUM_V1
};

static u32 ch11_txbd_num_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH11_TXBD_NUM, R_AX_CH11_TXBD_NUM,
	R_AX_CH11_TXBD_NUM_V1, R_AX_CH11_TXBD_NUM_V1
};

static u32 ach0_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH0_TXBD_DESA_L, R_AX_ACH0_TXBD_DESA_L,
	R_AX_ACH0_TXBD_DESA_L_V1, R_AX_ACH0_TXBD_DESA_L_V1
};

static u32 ach0_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH0_TXBD_DESA_H, R_AX_ACH0_TXBD_DESA_H,
	R_AX_ACH0_TXBD_DESA_H_V1, R_AX_ACH0_TXBD_DESA_H_V1
};

static u32 ach1_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH1_TXBD_DESA_L, R_AX_ACH1_TXBD_DESA_L,
	R_AX_ACH1_TXBD_DESA_L_V1, R_AX_ACH1_TXBD_DESA_L_V1
};

static u32 ach1_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH1_TXBD_DESA_H, R_AX_ACH1_TXBD_DESA_H,
	R_AX_ACH1_TXBD_DESA_H_V1, R_AX_ACH1_TXBD_DESA_H_V1
};

static u32 ach2_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH2_TXBD_DESA_L, R_AX_ACH2_TXBD_DESA_L,
	R_AX_ACH2_TXBD_DESA_L_V1, R_AX_ACH2_TXBD_DESA_L_V1
};

static u32 ach2_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH2_TXBD_DESA_H, R_AX_ACH2_TXBD_DESA_H,
	R_AX_ACH2_TXBD_DESA_H_V1, R_AX_ACH2_TXBD_DESA_H_V1
};

static u32 ach3_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH3_TXBD_DESA_L, R_AX_ACH3_TXBD_DESA_L,
	R_AX_ACH3_TXBD_DESA_L_V1, R_AX_ACH3_TXBD_DESA_L_V1
};

static u32 ach3_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH3_TXBD_DESA_H, R_AX_ACH3_TXBD_DESA_H,
	R_AX_ACH3_TXBD_DESA_H_V1, R_AX_ACH3_TXBD_DESA_H_V1
};

static u32 ach4_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH4_TXBD_DESA_L, R_AX_ACH4_TXBD_DESA_L,
	R_AX_ACH4_TXBD_DESA_L_V1, R_AX_ACH4_TXBD_DESA_L_V1
};

static u32 ach4_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH4_TXBD_DESA_H, R_AX_ACH4_TXBD_DESA_H,
	R_AX_ACH4_TXBD_DESA_H_V1, R_AX_ACH4_TXBD_DESA_H_V1
};

static u32 ach5_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH5_TXBD_DESA_L, R_AX_ACH5_TXBD_DESA_L,
	R_AX_ACH5_TXBD_DESA_L_V1, R_AX_ACH5_TXBD_DESA_L_V1
};

static u32 ach5_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH5_TXBD_DESA_H, R_AX_ACH5_TXBD_DESA_H,
	R_AX_ACH5_TXBD_DESA_H_V1, R_AX_ACH5_TXBD_DESA_H_V1
};

static u32 ach6_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH6_TXBD_DESA_L, R_AX_ACH6_TXBD_DESA_L,
	R_AX_ACH6_TXBD_DESA_L_V1, R_AX_ACH6_TXBD_DESA_L_V1
};

static u32 ach6_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH6_TXBD_DESA_H, R_AX_ACH6_TXBD_DESA_H,
	R_AX_ACH6_TXBD_DESA_H_V1, R_AX_ACH6_TXBD_DESA_H_V1
};

static u32 ach7_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH7_TXBD_DESA_L, R_AX_ACH7_TXBD_DESA_L,
	R_AX_ACH7_TXBD_DESA_L_V1, R_AX_ACH7_TXBD_DESA_L_V1
};

static u32 ach7_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH7_TXBD_DESA_H, R_AX_ACH7_TXBD_DESA_H,
	R_AX_ACH7_TXBD_DESA_H_V1, R_AX_ACH7_TXBD_DESA_H_V1
};

static u32 ch8_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH8_TXBD_DESA_L, R_AX_CH8_TXBD_DESA_L,
	R_AX_CH8_TXBD_DESA_L_V1, R_AX_CH8_TXBD_DESA_L_V1
};

static u32 ch8_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH8_TXBD_DESA_H, R_AX_CH8_TXBD_DESA_H,
	R_AX_CH8_TXBD_DESA_H_V1, R_AX_CH8_TXBD_DESA_H_V1
};

static u32 ch9_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH9_TXBD_DESA_L, R_AX_CH9_TXBD_DESA_L,
	R_AX_CH9_TXBD_DESA_L_V1, R_AX_CH9_TXBD_DESA_L_V1
};

static u32 ch9_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH9_TXBD_DESA_H, R_AX_CH9_TXBD_DESA_H,
	R_AX_CH9_TXBD_DESA_H_V1, R_AX_CH9_TXBD_DESA_H_V1
};

static u32 ch10_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH10_TXBD_DESA_L, R_AX_CH10_TXBD_DESA_L,
	R_AX_CH10_TXBD_DESA_L_V1, R_AX_CH10_TXBD_DESA_L_V1
};

static u32 ch10_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH10_TXBD_DESA_H, R_AX_CH10_TXBD_DESA_H,
	R_AX_CH10_TXBD_DESA_H_V1, R_AX_CH10_TXBD_DESA_H_V1
};

static u32 ch11_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH11_TXBD_DESA_L, R_AX_CH11_TXBD_DESA_L,
	R_AX_CH11_TXBD_DESA_L_V1, R_AX_CH11_TXBD_DESA_L_V1
};

static u32 ch11_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH11_TXBD_DESA_H, R_AX_CH11_TXBD_DESA_H,
	R_AX_CH11_TXBD_DESA_H_V1, R_AX_CH11_TXBD_DESA_H_V1
};

static u32 ch12_txbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH12_TXBD_DESA_L, R_AX_CH12_TXBD_DESA_L,
	R_AX_CH12_TXBD_DESA_L_V1, R_AX_CH12_TXBD_DESA_L_V1
};

static u32 ch12_txbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH12_TXBD_DESA_H, R_AX_CH12_TXBD_DESA_H,
	R_AX_CH12_TXBD_DESA_H_V1, R_AX_CH12_TXBD_DESA_H_V1
};

static u32 ach0_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH0_BDRAM_CTRL, R_AX_ACH0_BDRAM_CTRL,
	R_AX_ACH0_BDRAM_CTRL_V1, R_AX_ACH0_BDRAM_CTRL_V1
};

static u32 ach1_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH1_BDRAM_CTRL, R_AX_ACH1_BDRAM_CTRL,
	R_AX_ACH1_BDRAM_CTRL_V1, R_AX_ACH1_BDRAM_CTRL_V1
};

static u32 ach2_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH2_BDRAM_CTRL, R_AX_ACH2_BDRAM_CTRL,
	R_AX_ACH2_BDRAM_CTRL_V1, R_AX_ACH2_BDRAM_CTRL_V1
};

static u32 ach3_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH3_BDRAM_CTRL, R_AX_ACH3_BDRAM_CTRL,
	R_AX_ACH3_BDRAM_CTRL_V1, R_AX_ACH3_BDRAM_CTRL_V1
};

static u32 ach4_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH4_BDRAM_CTRL, R_AX_ACH4_BDRAM_CTRL,
	R_AX_ACH4_BDRAM_CTRL_V1, R_AX_ACH4_BDRAM_CTRL_V1
};

static u32 ach5_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH5_BDRAM_CTRL, R_AX_ACH5_BDRAM_CTRL,
	R_AX_ACH5_BDRAM_CTRL_V1, R_AX_ACH5_BDRAM_CTRL_V1
};

static u32 ach6_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH6_BDRAM_CTRL, R_AX_ACH6_BDRAM_CTRL,
	R_AX_ACH6_BDRAM_CTRL_V1, R_AX_ACH6_BDRAM_CTRL_V1
};

static u32 ach7_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_ACH7_BDRAM_CTRL, R_AX_ACH7_BDRAM_CTRL,
	R_AX_ACH7_BDRAM_CTRL_V1, R_AX_ACH7_BDRAM_CTRL_V1
};

static u32 ch8_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH8_BDRAM_CTRL, R_AX_CH8_BDRAM_CTRL,
	R_AX_CH8_BDRAM_CTRL_V1, R_AX_CH8_BDRAM_CTRL_V1
};

static u32 ch9_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH9_BDRAM_CTRL, R_AX_CH9_BDRAM_CTRL,
	R_AX_CH9_BDRAM_CTRL_V1, R_AX_CH9_BDRAM_CTRL_V1
};

static u32 ch10_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH10_BDRAM_CTRL, R_AX_CH10_BDRAM_CTRL,
	R_AX_CH10_BDRAM_CTRL_V1, R_AX_CH10_BDRAM_CTRL_V1
};

static u32 ch11_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH11_BDRAM_CTRL, R_AX_CH11_BDRAM_CTRL,
	R_AX_CH11_BDRAM_CTRL_V1, R_AX_CH11_BDRAM_CTRL_V1
};

static u32 ch12_bdram_ctrl_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_CH12_BDRAM_CTRL, R_AX_CH12_BDRAM_CTRL,
	R_AX_CH12_BDRAM_CTRL_V1, R_AX_CH12_BDRAM_CTRL_V1
};

static u32 rxq_rxbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_RXQ_RXBD_DESA_L, R_AX_RXQ_RXBD_DESA_L,
	R_AX_RXQ_RXBD_DESA_L_V1, R_AX_RXQ_RXBD_DESA_L_V1
};

static u32 rxq_rxbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_RXQ_RXBD_DESA_H, R_AX_RXQ_RXBD_DESA_H,
	R_AX_RXQ_RXBD_DESA_H_V1, R_AX_RXQ_RXBD_DESA_H_V1
};

static u32 rpq_rxbd_desa_l_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_RPQ_RXBD_DESA_L, R_AX_RPQ_RXBD_DESA_L,
	R_AX_RPQ_RXBD_DESA_L_V1, R_AX_RPQ_RXBD_DESA_L_V1
};

static u32 rpq_rxbd_desa_h_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_RPQ_RXBD_DESA_H, R_AX_RPQ_RXBD_DESA_H,
	R_AX_RPQ_RXBD_DESA_H_V1, R_AX_RPQ_RXBD_DESA_H_V1
};

static u32 rxq_rxbd_num_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_RXQ_RXBD_NUM, R_AX_RXQ_RXBD_NUM,
	R_AX_RXQ_RXBD_NUM_V1, R_AX_RXQ_RXBD_NUM_V1
};

static u32 rpq_rxbd_num_reg[MAC_AX_CHIP_ID_MAX] = {
	R_AX_RPQ_RXBD_NUM, R_AX_RPQ_RXBD_NUM,
	R_AX_RPQ_RXBD_NUM_V1, R_AX_RPQ_RXBD_NUM_V1
};

struct txbd_ram mac_bdram_tbl_8852a[] = {
		/* ACH0_QUEUE_IDX_8852AE */ {0, 5, 2},
		/* ACH1_QUEUE_IDX_8852AE */ {5, 5, 2},
		/* ACH2_QUEUE_IDX_8852AE */ {10, 5, 2},
		/* ACH3_QUEUE_IDX_8852AE */ {15, 5, 2},
		/* ACH4_QUEUE_IDX_8852AE */ {20, 5, 2},
		/* ACH5_QUEUE_IDX_8852AE */ {25, 5, 2},
		/* ACH6_QUEUE_IDX_8852AE */ {30, 5, 2},
		/* ACH7_QUEUE_IDX_8852AE */ {35, 5, 2},
		/* MGQ_B0_QUEUE_IDX_8852AE */ {40, 5, 1},
		/* HIQ_B0_QUEUE_IDX_8852AE */ {45, 5, 1},
		/* MGQ_B1_QUEUE_IDX_8852AE */ {50, 5, 1},
		/* HIQ_B1_QUEUE_IDX_8852AE */ {55, 5, 1},
		/* FWCMD_QUEUE_IDX_8852AE */ {60, 4, 1}
};

struct txbd_ram mac_bdram_tbl_8852b[] = {
		/* ACH0_QUEUE_IDX_8852BE */ {0, 5, 2},
		/* ACH1_QUEUE_IDX_8852BE */ {5, 5, 2},
		/* ACH2_QUEUE_IDX_8852BE */ {10, 5, 2},
		/* ACH3_QUEUE_IDX_8852BE */ {15, 5, 2},
		/* MGQ_B0_QUEUE_IDX_8852BE */ {20, 4, 1},
		/* HIQ_B0_QUEUE_IDX_8852BE */ {24, 4, 1},
		/* FWCMD_QUEUE_IDX_8852BE */ {28, 4, 1}
};

struct txbd_ram mac_bdram_tbl_8852c[] = {
		/* ACH0_QUEUE_IDX_8852AE */ {0, 5, 2},
		/* ACH1_QUEUE_IDX_8852AE */ {5, 5, 2},
		/* ACH2_QUEUE_IDX_8852AE */ {10, 5, 2},
		/* ACH3_QUEUE_IDX_8852AE */ {15, 5, 2},
		/* ACH4_QUEUE_IDX_8852AE */ {20, 5, 2},
		/* ACH5_QUEUE_IDX_8852AE */ {25, 5, 2},
		/* ACH6_QUEUE_IDX_8852AE */ {30, 5, 2},
		/* ACH7_QUEUE_IDX_8852AE */ {35, 5, 2},
		/* MGQ_B0_QUEUE_IDX_8852AE */ {40, 5, 1},
		/* HIQ_B0_QUEUE_IDX_8852AE */ {45, 5, 1},
		/* MGQ_B1_QUEUE_IDX_8852AE */ {50, 5, 1},
		/* HIQ_B1_QUEUE_IDX_8852AE */ {55, 5, 1},
		/* FWCMD_QUEUE_IDX_8852AE */ {60, 4, 1}
};

struct txbd_ram mac_bdram_tbl_8192xb[] = {
		/* ACH0_QUEUE_IDX_8852AE */ {0, 5, 2},
		/* ACH1_QUEUE_IDX_8852AE */ {5, 5, 2},
		/* ACH2_QUEUE_IDX_8852AE */ {10, 5, 2},
		/* ACH3_QUEUE_IDX_8852AE */ {15, 5, 2},
		/* ACH4_QUEUE_IDX_8852AE */ {20, 5, 2},
		/* ACH5_QUEUE_IDX_8852AE */ {25, 5, 2},
		/* ACH6_QUEUE_IDX_8852AE */ {30, 5, 2},
		/* ACH7_QUEUE_IDX_8852AE */ {35, 5, 2},
		/* MGQ_B0_QUEUE_IDX_8852AE */ {40, 5, 1},
		/* HIQ_B0_QUEUE_IDX_8852AE */ {45, 5, 1},
		/* MGQ_B1_QUEUE_IDX_8852AE */ {50, 5, 1},
		/* HIQ_B1_QUEUE_IDX_8852AE */ {55, 5, 1},
		/* FWCMD_QUEUE_IDX_8852AE */ {60, 4, 1}
};

static struct mac_ax_intf_info intf_info_def_8852a = {
	MAC_AX_BD_TRUNC,
	MAC_AX_BD_TRUNC,
	MAC_AX_RXBD_PKT,
	MAC_AX_TAG_MULTI,
	MAC_AX_TX_BURST_2048B,
	MAC_AX_RX_BURST_128B,
	MAC_AX_WD_DMA_INTVL_256NS,
	MAC_AX_WD_DMA_INTVL_256NS,
	MAC_AX_TAG_NUM_8,
	0,
	NULL,
	NULL,
	0,
	NULL,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_LBC_TMR_2MS,
	MAC_AX_PCIE_DISABLE,
	MAC_AX_PCIE_DISABLE,
	MAC_AX_IO_RCY_ANA_TMR_60US
};

static struct mac_ax_intf_info intf_info_def_8852b = {
	MAC_AX_BD_TRUNC,
	MAC_AX_BD_TRUNC,
	MAC_AX_RXBD_PKT,
	MAC_AX_TAG_MULTI,
	MAC_AX_TX_BURST_2048B,
	MAC_AX_RX_BURST_128B,
	MAC_AX_WD_DMA_INTVL_256NS,
	MAC_AX_WD_DMA_INTVL_256NS,
	MAC_AX_TAG_NUM_8,
	0,
	NULL,
	NULL,
	0,
	NULL,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_LBC_TMR_2MS,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_DISABLE,
	MAC_AX_IO_RCY_ANA_TMR_60US
};

static struct mac_ax_intf_info intf_info_def_8852c = {
	MAC_AX_BD_NORM,
	MAC_AX_BD_NORM,
	MAC_AX_RXBD_SEP,
	MAC_AX_TAG_MULTI,
	MAC_AX_TX_BURST_V1_256B,
	MAC_AX_RX_BURST_V1_128B,
	MAC_AX_WD_DMA_INTVL_256NS,
	MAC_AX_WD_DMA_INTVL_256NS,
	MAC_AX_TAG_NUM_8,
	0,
	NULL,
	NULL,
	0,
	NULL,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_LBC_TMR_2MS,
	MAC_AX_PCIE_DISABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_IO_RCY_ANA_TMR_60US
};

static struct mac_ax_intf_info intf_info_def_8192xb = {
	MAC_AX_BD_NORM,
	MAC_AX_BD_NORM,
	MAC_AX_RXBD_SEP,
	MAC_AX_TAG_MULTI,
	MAC_AX_TX_BURST_V1_256B,
	MAC_AX_RX_BURST_V1_128B,
	MAC_AX_WD_DMA_INTVL_256NS,
	MAC_AX_WD_DMA_INTVL_256NS,
	MAC_AX_TAG_NUM_8,
	0,
	NULL,
	NULL,
	0,
	NULL,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_LBC_TMR_2MS,
};

static struct mac_ax_pcie_cfgspc_param pcie_cfgspc_param_def = {
	0,
	0,
	MAC_AX_PCIE_DISABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_CLKDLY_30US,
	MAC_AX_PCIE_L0SDLY_4US,
	MAC_AX_PCIE_L1DLY_16US
};

static struct mac_ax_pcie_cfgspc_param pcie_cfgspc_param_def_8852b = {
	0,
	0,
	MAC_AX_PCIE_DISABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_CLKDLY_100US,
	MAC_AX_PCIE_L0SDLY_4US,
	MAC_AX_PCIE_L1DLY_16US
};

static struct mac_ax_pcie_ltr_param pcie_ltr_param_def = {
	0,
	0,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_ENABLE,
	MAC_AX_PCIE_LTR_SPC_500US,
	MAC_AX_PCIE_LTR_IDLE_TIMER_3_2MS,
	{MAC_AX_PCIE_ENABLE, 0x28},
	{MAC_AX_PCIE_ENABLE, 0x28},
	{MAC_AX_PCIE_ENABLE, 0x90039003},
	{MAC_AX_PCIE_ENABLE, 0x880b880b}
};

u8 reg_read8_pcie(struct mac_ax_adapter *adapter, u32 addr)
{
	u8 offset, count = 0, val8;
	u32 val, addr_shift;

	if (addr >= R_AX_CMAC_FUNC_EN && addr <= R_AX_CMAC_REG_END) {
		offset = addr & (MAC_REG_OFFSET - 1);
		addr_shift = addr - offset;
		val = PLTFM_REG_R32(addr_shift);

		while (count < MAC_REG_POOL_COUNT) {
			if (val != MAC_AX_R32_DEAD)
				break;

			PLTFM_MSG_ERR("[ERR]addr 0x%x = 0xdeadbeef\n", addr);
			PLTFM_REG_W32(R_AX_CK_EN, CMAC_CLK_ALLEN);
			val = PLTFM_REG_R32(addr_shift);
			count++;
		}

		val8 = (u8)(val >> (offset << MAC_REG_OFFSET_SH_2));
	} else {
		val8 = PLTFM_REG_R8(addr);
	}

	if (count == MAC_REG_POOL_COUNT && adapter->sm.l2_st == MAC_AX_L2_EN) {
		adapter->sm.l2_st = MAC_AX_L2_TRIG;
		PLTFM_L2_NOTIFY(void);
	}

	return val8;
}

void reg_write8_pcie(struct mac_ax_adapter *adapter, u32 addr, u8 val)
{
	PLTFM_REG_W8(addr, val);
}

u16 reg_read16_pcie(struct mac_ax_adapter *adapter, u32 addr)
{
	u8 offset, count = 0;
	u16 val16;
	u32 val, addr_shift;

	if ((addr & (MAC_REG_OFFSET16 - 1)) != 0) {
		PLTFM_MSG_ERR("[ERR]read16 failaddr 0x%x\n", addr);
		return MAC_AX_R16_DEAD;
	}

	if (addr >= R_AX_CMAC_FUNC_EN && addr <= R_AX_CMAC_REG_END) {
		offset = addr & (MAC_REG_OFFSET - 1);
		addr_shift = addr - offset;
		val = PLTFM_REG_R32(addr_shift);

		while (count < MAC_REG_POOL_COUNT) {
			if (val != MAC_AX_R32_DEAD)
				break;

			PLTFM_MSG_ERR("[ERR]addr 0x%x = 0xdeadbeef\n", addr);
			PLTFM_REG_W32(R_AX_CK_EN, CMAC_CLK_ALLEN);
			val = PLTFM_REG_R32(addr_shift);
			count++;
		}
		val16 = (u16)(val >> (offset << MAC_REG_OFFSET_SH_2));
	} else {
		val16 = PLTFM_REG_R16(addr);
	}

	if (count == MAC_REG_POOL_COUNT && adapter->sm.l2_st == MAC_AX_L2_EN) {
		adapter->sm.l2_st = MAC_AX_L2_TRIG;
		PLTFM_L2_NOTIFY(void);
	}

	return val16;
}

void reg_write16_pcie(struct mac_ax_adapter *adapter, u32 addr, u16 val)
{
	PLTFM_REG_W16(addr, val);
}

u32 reg_read32_pcie(struct mac_ax_adapter *adapter, u32 addr)
{
	u8 count = 0;
	u32 val = PLTFM_REG_R32(addr);

	if (addr >= R_AX_CMAC_FUNC_EN && addr <= R_AX_CMAC_REG_END) {
		while (count < MAC_REG_POOL_COUNT) {
			if (val != MAC_AX_R32_DEAD)
				break;

			PLTFM_MSG_ERR("[ERR]addr 0x%x = 0xdeadbeef\n", addr);
			PLTFM_REG_W32(R_AX_CK_EN, CMAC_CLK_ALLEN);
			val = PLTFM_REG_R32(addr);
			count++;
		}
	}

	if (count == MAC_REG_POOL_COUNT && adapter->sm.l2_st == MAC_AX_L2_EN) {
		adapter->sm.l2_st = MAC_AX_L2_TRIG;
		PLTFM_L2_NOTIFY(void);
	}

	return val;
}

void reg_write32_pcie(struct mac_ax_adapter *adapter, u32 addr, u32 val)
{
	PLTFM_REG_W32(addr, val);
}

u32 dbi_r32_pcie(struct mac_ax_adapter *adapter, u16 addr, u32 *val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 r_addr = addr & DBI_ADDR_MASK;
	u32 val32, cnt;
	u32 ret = MACSUCCESS;

#if MAC_AX_FEATURE_HV
	if (adapter->env == HV_AX_FPGA)
		return MACSUCCESS;
#endif

	if (addr & DBI_ADDR_2LSB_MASK) {
		PLTFM_MSG_ERR("[ERR]DBI R32 addr 0x%X not 4B align\n", addr);
		return MACFUNCINPUT;
	}

	PLTFM_MUTEX_LOCK(&adapter->hw_info->dbi_lock);

	val32 = 0;
	val32 = SET_CLR_WORD(val32, r_addr, B_AX_DBI_ADDR);
	MAC_REG_W32(R_AX_DBI_FLAG, val32);

	val32 |= B_AX_DBI_RFLAG;
	MAC_REG_W32(R_AX_DBI_FLAG, val32);

	cnt = DBI_DLY_CNT;
	while (MAC_REG_R32(R_AX_DBI_FLAG) & B_AX_DBI_RFLAG && cnt) {
		PLTFM_DELAY_US(DBI_DLY_US);
		cnt--;
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]DBI R32 0x%X timeout\n", r_addr);
		ret = MACPOLLTO;
		goto end;
	}

	*val = MAC_REG_R32(R_AX_DBI_RDATA);
end:
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->dbi_lock);
	return ret;
}

u32 dbi_w32_pcie(struct mac_ax_adapter *adapter, u16 addr, u32 data)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 w_addr = addr & DBI_ADDR_MASK;
	u32 val32, cnt;
	u32 ret = MACSUCCESS;

#if MAC_AX_FEATURE_HV
	if (adapter->env == HV_AX_FPGA)
		return MACSUCCESS;
#endif

	PLTFM_MUTEX_LOCK(&adapter->hw_info->dbi_lock);

	MAC_REG_W32(R_AX_DBI_WDATA, data);

	val32 = 0;
	val32 = SET_CLR_WORD(val32, w_addr, B_AX_DBI_ADDR);
	val32 = SET_CLR_WORD(val32, DBI_WEN_DW, B_AX_DBI_WREN);
	MAC_REG_W32(R_AX_DBI_FLAG, val32);

	val32 |= B_AX_DBI_WFLAG;
	MAC_REG_W32(R_AX_DBI_FLAG, val32);

	cnt = DBI_DLY_CNT;
	while (MAC_REG_R32(R_AX_DBI_FLAG) & B_AX_DBI_WFLAG && cnt) {
		PLTFM_DELAY_US(DBI_DLY_US);
		cnt--;
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]DBI W32 0x%X = 0x%x timeout\n", w_addr, data);
		ret = MACPOLLTO;
		goto end;
	}
end:
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->dbi_lock);
	return ret;
}

u32 dbi_r8_pcie(struct mac_ax_adapter *adapter, u16 addr, u8 *val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 r_addr = addr & DBI_ADDR_MASK;
	u32 addr_2lsb = addr & DBI_ADDR_2LSB_MASK;
	u32 val32, cnt;
	u32 ret = MACSUCCESS;

#if MAC_AX_FEATURE_HV
	if (adapter->env == HV_AX_FPGA)
		return MACSUCCESS;
#endif

	PLTFM_MUTEX_LOCK(&adapter->hw_info->dbi_lock);

	val32 = 0;
	val32 = SET_CLR_WORD(val32, r_addr, B_AX_DBI_ADDR);
	MAC_REG_W32(R_AX_DBI_FLAG, val32);

	val32 |= B_AX_DBI_RFLAG;
	MAC_REG_W32(R_AX_DBI_FLAG, val32);

	cnt = DBI_DLY_CNT;
	while (MAC_REG_R32(R_AX_DBI_FLAG) & B_AX_DBI_RFLAG && cnt) {
		PLTFM_DELAY_US(DBI_DLY_US);
		cnt--;
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]DBI R8 0x%X timeout\n", r_addr);
		ret = MACPOLLTO;
		goto end;
	}

	*val = MAC_REG_R8(R_AX_DBI_RDATA + addr_2lsb);
end:
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->dbi_lock);
	return ret;
}

u32 dbi_w8_pcie(struct mac_ax_adapter *adapter, u16 addr, u8 data)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 w_addr = addr & DBI_ADDR_MASK;
	u32 addr_2lsb = addr & DBI_ADDR_2LSB_MASK;
	u32 val32, cnt;
	u32 ret = MACSUCCESS;

#if MAC_AX_FEATURE_HV
	if (adapter->env == HV_AX_FPGA)
		return MACSUCCESS;
#endif

	PLTFM_MUTEX_LOCK(&adapter->hw_info->dbi_lock);

	MAC_REG_W8(R_AX_DBI_WDATA + addr_2lsb, data);

	val32 = 0;
	val32 = SET_CLR_WORD(val32, w_addr, B_AX_DBI_ADDR);
	val32 = SET_CLR_WORD(val32, DBI_WEN_B << addr_2lsb, B_AX_DBI_WREN);
	MAC_REG_W32(R_AX_DBI_FLAG, val32);

	val32 |= B_AX_DBI_WFLAG;
	MAC_REG_W32(R_AX_DBI_FLAG, val32);

	cnt = DBI_DLY_CNT;
	while (MAC_REG_R32(R_AX_DBI_FLAG) & B_AX_DBI_WFLAG && cnt) {
		PLTFM_DELAY_US(DBI_DLY_US);
		cnt--;
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]DBI W8 0x%X = 0x%x timeout\n", w_addr, data);
		ret = MACPOLLTO;
		goto end;
	}
end:
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->dbi_lock);
	return ret;
}

u32 mdio_r16_pcie(struct mac_ax_adapter *adapter, u8 addr, u8 speed, u16 *val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u16 val16;
	u32 cnt;
	u32 ret = MACSUCCESS;

#if MAC_AX_FEATURE_HV
	if (adapter->env == HV_AX_FPGA)
		return MACSUCCESS;
#endif

	PLTFM_MUTEX_LOCK(&adapter->hw_info->mdio_lock);

	MAC_REG_W8(R_AX_MDIO_CFG, addr & B_AX_MDIO_ADDR_MSK);

	val16 = MAC_REG_R16(R_AX_MDIO_CFG);
	if (speed == MAC_AX_PCIE_PHY_GEN1 && addr < MDIO_ADDR_PG1) {
		val16 = SET_CLR_WORD(val16, MDIO_PG0_G1, B_AX_MDIO_PHY_ADDR);
	} else if (speed == MAC_AX_PCIE_PHY_GEN1) {
		val16 = SET_CLR_WORD(val16, MDIO_PG1_G1, B_AX_MDIO_PHY_ADDR);
	} else if (speed == MAC_AX_PCIE_PHY_GEN2 && addr < MDIO_ADDR_PG1) {
		val16 = SET_CLR_WORD(val16, MDIO_PG0_G2, B_AX_MDIO_PHY_ADDR);
	} else if (speed == MAC_AX_PCIE_PHY_GEN2) {
		val16 = SET_CLR_WORD(val16, MDIO_PG1_G2, B_AX_MDIO_PHY_ADDR);
	} else {
		PLTFM_MSG_ERR("[ERR]Error MDIO PHY Speed %d!\n", speed);
		ret = MACFUNCINPUT;
		goto end;
	}
	MAC_REG_W16(R_AX_MDIO_CFG, val16);

	MAC_REG_W16(R_AX_MDIO_CFG,
		    MAC_REG_R16(R_AX_MDIO_CFG) | B_AX_MDIO_RFLAG);

	cnt = MDIO_DLY_CNT;
	while (MAC_REG_R16(R_AX_MDIO_CFG) & B_AX_MDIO_RFLAG && cnt) {
		PLTFM_DELAY_US(MDIO_DLY_US);
		cnt--;
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]MDIO R16 0x%X timeout\n", addr);
		ret = MACPOLLTO;
		goto end;
	}

	*val = MAC_REG_R16(R_AX_MDIO_RDATA);
end:
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->mdio_lock);
	return ret;
}

u32 mdio_w16_pcie(struct mac_ax_adapter *adapter, u8 addr, u16 data, u8 speed)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u16 val16;
	u32 cnt;
	u32 ret = MACSUCCESS;

#if MAC_AX_FEATURE_HV
	if (adapter->env == HV_AX_FPGA)
		return MACSUCCESS;
#endif

	PLTFM_MUTEX_LOCK(&adapter->hw_info->mdio_lock);

	MAC_REG_W16(R_AX_MDIO_WDATA, data);
	MAC_REG_W8(R_AX_MDIO_CFG, addr & B_AX_MDIO_ADDR_MSK);

	val16 = MAC_REG_R16(R_AX_MDIO_CFG);
	if (speed == MAC_AX_PCIE_PHY_GEN1 && addr < MDIO_ADDR_PG1) {
		val16 = SET_CLR_WORD(val16, MDIO_PG0_G1, B_AX_MDIO_PHY_ADDR);
	} else if (speed == MAC_AX_PCIE_PHY_GEN1) {
		val16 = SET_CLR_WORD(val16, MDIO_PG1_G1, B_AX_MDIO_PHY_ADDR);
	} else if (speed == MAC_AX_PCIE_PHY_GEN2 && addr < MDIO_ADDR_PG1) {
		val16 = SET_CLR_WORD(val16, MDIO_PG0_G2, B_AX_MDIO_PHY_ADDR);
	} else if (speed == MAC_AX_PCIE_PHY_GEN2) {
		val16 = SET_CLR_WORD(val16, MDIO_PG1_G2, B_AX_MDIO_PHY_ADDR);
	} else {
		PLTFM_MSG_ERR("[ERR]Error MDIO PHY Speed %d!\n", speed);
		ret = MACFUNCINPUT;
		goto end;
	}
	MAC_REG_W16(R_AX_MDIO_CFG, val16);

	MAC_REG_W16(R_AX_MDIO_CFG,
		    MAC_REG_R16(R_AX_MDIO_CFG) | B_AX_MDIO_WFLAG);

	cnt = MDIO_DLY_CNT;
	while (MAC_REG_R16(R_AX_MDIO_CFG) & B_AX_MDIO_WFLAG && cnt) {
		PLTFM_DELAY_US(MDIO_DLY_US);
		cnt--;
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]MDIO W16 0x%X = 0x%x timeout!\n", addr, data);
		ret = MACPOLLTO;
		goto end;
	}

end:
	PLTFM_MUTEX_UNLOCK(&adapter->hw_info->mdio_lock);
	return ret;
}

static u32 get_target(struct mac_ax_adapter *adapter, u16 *target,
		      enum mac_ax_pcie_phy phy_rate)
{
	u16 tmp_u16, count;
	u16 tar;
	u32 ret = MACSUCCESS;

	/* Enable counter */
	ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &tmp_u16);
	if (ret != MACSUCCESS)
		return ret;
	ret = mdio_w16_pcie(adapter, RAC_CTRL_PPR_V1, tmp_u16 & ~BAC_AUTOK_ONCE_EN,
			    phy_rate);
	if (ret != MACSUCCESS)
		return ret;
	ret = mdio_w16_pcie(adapter, RAC_CTRL_PPR_V1, tmp_u16 | BAC_AUTOK_ONCE_EN,
			    phy_rate);
	if (ret != MACSUCCESS)
		return ret;

	ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &tar);
	if (ret != MACSUCCESS)
		return ret;

	count = PCIE_POLL_AUTOK_CNT;
	while (count && (tar & BAC_AUTOK_ONCE_EN)) {
		ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &tar);
		if (ret != MACSUCCESS)
			return ret;
		count--;
		PLTFM_DELAY_US(PCIE_POLL_AUTOK_DLY_US);
	}

	if (!count) {
		PLTFM_MSG_ERR("[ERR]AutoK get target timeout: %X\n", tar);
		return MACPOLLTO;
	}

	tar = tar & BAC_AUTOK_HW_TAR_MSK;
	if (tar == 0 || tar == BAC_AUTOK_HW_TAR_MSK) {
		PLTFM_MSG_ERR("[ERR]Get target failed.\n");
		return MACHWERR;
	}

	*target = tar;
	return MACSUCCESS;
}

static u32 mac_auto_refclk_cal_pcie(struct mac_ax_adapter *adapter,
				    enum mac_ax_pcie_func_ctrl en)
{
	u8 bdr_ori, val8;
	u16 tmp_u16;
	u16 mgn_set;
	u16 tar;
	u8 l1_flag = 0;
	u32 ret = MACSUCCESS;
	enum mac_ax_pcie_phy phy_rate = MAC_AX_PCIE_PHY_GEN1;

#if MAC_AX_FEATURE_HV
	if (adapter->env == HV_AX_FPGA)
		return MACSUCCESS;
#endif

#if (INTF_INTGRA_HOSTREF_V1 <= INTF_INTGRA_MINREF_V1)
	PLTFM_MSG_ERR("[ERR]INTF_INTGRA_MINREF_V1 define fail\n");
	return MACCMP;
#endif
	ret = dbi_r8_pcie(adapter, PCIE_PHY_RATE, &val8);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]dbi_r8_pcie 0x%x\n", PCIE_PHY_RATE);
		return ret;
	}

	if ((val8 & (BIT1 | BIT0)) == 0x1) {
		phy_rate = MAC_AX_PCIE_PHY_GEN1;
	} else if ((val8 & (BIT1 | BIT0)) == 0x2) {
		phy_rate = MAC_AX_PCIE_PHY_GEN2;
	} else {
		PLTFM_MSG_ERR("[ERR]PCIe PHY rate not support\n");
		return MACHWNOSUP;
	}

	/* Disable L1BD */
	ret = dbi_r8_pcie(adapter, PCIE_L1_CTRL, &bdr_ori);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]dbi_r8_pcie 0x%X\n", PCIE_L1_CTRL);
		return ret;
	}

	if (bdr_ori & PCIE_BIT_L1) {
		ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL,
				  bdr_ori & ~(PCIE_BIT_L1));
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]dbi_w8_pcie 0x%X\n", PCIE_L1_CTRL);
			return ret;
		}
		l1_flag = 1;
	}

	/* Disable function */
	ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &tmp_u16);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]mdio_r16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

	if (tmp_u16 & BAC_AUTOK_EN) {
		ret = mdio_w16_pcie(adapter, RAC_CTRL_PPR_V1,
				    tmp_u16 & ~(BAC_AUTOK_EN), phy_rate);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]mdio_w16_pcie 0x%X\n",
				      RAC_CTRL_PPR_V1);
			goto end;
		}
	}

	if (en != MAC_AX_PCIE_ENABLE)
		goto end;

	/* Set div */
	ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &tmp_u16);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]mdio_r16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

	tmp_u16 = SET_CLR_WOR2(tmp_u16, PCIE_AUTOK_DIV_2048, BAC_AUTOK_DIV_SH,
			       BAC_AUTOK_DIV_MSK);
	ret = mdio_w16_pcie(adapter, RAC_CTRL_PPR_V1, tmp_u16, phy_rate);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]mdio_w16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

	/*  Obtain div and margin */
	ret = get_target(adapter, &tar, phy_rate);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]1st get target fail %d\n", ret);
		goto end;
	}

	mgn_set = PCIE_AUTOK_MGN;

	ret = mdio_w16_pcie(adapter, RAC_SET_PPR_V1, (tar & BAC_AUTOK_TAR_MSK) |
			    (mgn_set << BAC_AUTOK_MGN_SH), phy_rate);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]mdio_w16_pcie 0x%X\n", RAC_SET_PPR_V1);
		goto end;
	}

	/* Enable function */
	ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &tmp_u16);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]mdio_r16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
		goto end;
	}
	ret = mdio_w16_pcie(adapter, RAC_CTRL_PPR_V1, tmp_u16 | BAC_AUTOK_EN,
			    phy_rate);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]mdio_w16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
		goto end;
	}

end:
	/* Set L1BD to ori */
	if (l1_flag == 1) {
		ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL, bdr_ori);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]dbi_w8_pcie 0x%X\n", PCIE_L1_CTRL);
			return ret;
		}
	}
	PLTFM_MSG_TRACE("[TRACE]%s: <==\n", __func__);

	return ret;
}

static void update_pcie_func(u8 *val, u8 bitmask,
			     enum mac_ax_pcie_func_ctrl ctrl,
			     enum mac_ax_pcie_func_ctrl def_ctrl)
{
	if ((ctrl == MAC_AX_PCIE_DEFAULT &&
	     (def_ctrl == MAC_AX_PCIE_IGNORE ||
	      def_ctrl == MAC_AX_PCIE_DEFAULT)) || ctrl == MAC_AX_PCIE_IGNORE)
		return;

	if ((ctrl == MAC_AX_PCIE_DEFAULT && def_ctrl == MAC_AX_PCIE_DISABLE) ||
	    ctrl == MAC_AX_PCIE_DISABLE)
		*val &= ~(bitmask);
	else
		*val |= bitmask;
}

static u32 chk_stus_l1ss(struct mac_ax_adapter *adapter, u8 *val)
{
	u16 cap_val;
	u8 stus_val;
	u8 sup_val;
	u32 ret;
	u8 val8_1, val8_2;

	ret = dbi_r8_pcie(adapter, PCIE_L1SS_CAP + 1, &val8_1);
	if (ret != MACSUCCESS)
		return ret;

	ret = dbi_r8_pcie(adapter, PCIE_L1SS_CAP, &val8_2);
	if (ret != MACSUCCESS)
		return ret;

	cap_val = (u16)((val8_1 << 8) | val8_2);

	ret = dbi_r8_pcie(adapter, PCIE_L1SS_SUP, &sup_val);
	if (ret != MACSUCCESS)
		return ret;

	ret = dbi_r8_pcie(adapter, PCIE_L1SS_STS, &stus_val);
	if (ret != MACSUCCESS)
		return ret;

	if (cap_val == PCIE_L1SS_ID &&
	    (sup_val & PCIE_BIT_L1SSSUP) &&
	    (sup_val & PCIE_L1SS_MASK) != 0 &&
	    (stus_val & PCIE_L1SS_MASK) != 0)
		*val = 1;
	else
		*val = 0;

	return ret;
}

u32 update_clkdly(struct mac_ax_adapter *adapter, u8 *val,
		  enum mac_ax_pcie_clkdly ctrl,
		  enum mac_ax_pcie_clkdly def_ctrl)
{
	u8 tmp;

	if (ctrl == MAC_AX_PCIE_CLKDLY_IGNORE ||
	    def_ctrl == MAC_AX_PCIE_CLKDLY_IGNORE)
		return MACSUCCESS;

	tmp = (ctrl == MAC_AX_PCIE_CLKDLY_DEF) ? def_ctrl : ctrl;
	switch (tmp) {
	case MAC_AX_PCIE_CLKDLY_0:
		*val = PCIE_CLKDLY_HW_0;
		break;

	case MAC_AX_PCIE_CLKDLY_30US:
		*val = PCIE_CLKDLY_HW_30US;
		break;

	case MAC_AX_PCIE_CLKDLY_50US:
		*val = PCIE_CLKDLY_HW_50US;
		break;

	case MAC_AX_PCIE_CLKDLY_100US:
		*val = PCIE_CLKDLY_HW_100US;
		break;

	case MAC_AX_PCIE_CLKDLY_150US:
		*val = PCIE_CLKDLY_HW_150US;
		break;

	case MAC_AX_PCIE_CLKDLY_200US:
		*val = PCIE_CLKDLY_HW_200US;
		break;
	case MAC_AX_PCIE_CLKDLY_300US:
		*val = PCIE_CLKDLY_HW_300US;
		break;
	case MAC_AX_PCIE_CLKDLY_400US:
		*val = PCIE_CLKDLY_HW_400US;
		break;
	case MAC_AX_PCIE_CLKDLY_500US:
		*val = PCIE_CLKDLY_HW_500US;
		break;
	case MAC_AX_PCIE_CLKDLY_1MS:
		*val = PCIE_CLKDLY_HW_1MS;
		break;
	case MAC_AX_PCIE_CLKDLY_3MS:
		*val = PCIE_CLKDLY_HW_3MS;
		break;
	default:
		PLTFM_MSG_ERR("[ERR]CLKDLY wt val illegal!\n");
		return MACSTCAL;
	}
	return MACSUCCESS;
}

static u32 update_pcie_clk(struct mac_ax_adapter *adapter, u8 *val)
{
	u32 ret = MACSUCCESS;

	if (*val & PCIE_BIT_CLK)
		return MACSUCCESS;

	if (*val & PCIE_BIT_L1) {
		*val &= ~(PCIE_BIT_L1);
		ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL, *val);
		if (ret != MACSUCCESS)
			return ret;

		*val |= PCIE_BIT_CLK;
		ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL, *val);
		if (ret != MACSUCCESS)
			return ret;

		*val |= PCIE_BIT_L1;
		ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL, *val);
		if (ret != MACSUCCESS)
			return ret;

		PLTFM_MSG_WARN("[WARN] L1 enable & CLKREQ disable!\n");
	}

	*val |= PCIE_BIT_CLK;
	ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL, *val);

	return ret;
}

u32 update_aspmdly(struct mac_ax_adapter *adapter, u8 *val,
		   struct mac_ax_pcie_cfgspc_param *param,
		   struct mac_ax_pcie_cfgspc_param *param_def)
{
	u8 l1mask = PCIE_ASPMDLY_MASK << SHFT_L1DLY;
	u8 l0smask = PCIE_ASPMDLY_MASK << SHFT_L0SDLY;
	u8 l1updval = param->l1dly_ctrl;
	u8 l0supdval = param->l0sdly_ctrl;
	u8 l1defval = param_def->l1dly_ctrl;
	u8 l0sdefval = param_def->l0sdly_ctrl;
	u8 tmp;
	u8 hwval;

	if (l1updval != MAC_AX_PCIE_L1DLY_IGNORE) {
		tmp = (l1updval == MAC_AX_PCIE_L1DLY_DEF) ? l1defval : l1updval;
		switch (tmp) {
		case MAC_AX_PCIE_L1DLY_16US:
			hwval = PCIE_L1DLY_HW_16US;
			break;

		case MAC_AX_PCIE_L1DLY_32US:
			hwval = PCIE_L1DLY_HW_32US;
			break;

		case MAC_AX_PCIE_L1DLY_64US:
			hwval = PCIE_L1DLY_HW_64US;
			break;

		case MAC_AX_PCIE_L1DLY_INFI:
			hwval = PCIE_L1DLY_HW_INFI;
			break;

		default:
			PLTFM_MSG_ERR("[ERR]L1DLY wt val illegal!\n");
			return MACSTCAL;
		}

		tmp = (hwval << SHFT_L1DLY) & l1mask;
		*val = (*val & ~(l1mask)) | tmp;
	}

	if (l0supdval != MAC_AX_PCIE_L0SDLY_IGNORE) {
		tmp = (l0supdval == MAC_AX_PCIE_L0SDLY_DEF) ?
		       l0sdefval : l0supdval;
		switch (tmp) {
		case MAC_AX_PCIE_L0SDLY_1US:
			hwval = PCIE_L0SDLY_HW_1US;
			break;

		case MAC_AX_PCIE_L0SDLY_2US:
			hwval = PCIE_L0SDLY_HW_2US;
			break;

		case MAC_AX_PCIE_L0SDLY_3US:
			hwval = PCIE_L0SDLY_HW_3US;
			break;

		case MAC_AX_PCIE_L0SDLY_4US:
			hwval = PCIE_L0SDLY_HW_4US;
			break;

		case MAC_AX_PCIE_L0SDLY_5US:
			hwval = PCIE_L0SDLY_HW_5US;
			break;

		case MAC_AX_PCIE_L0SDLY_6US:
			hwval = PCIE_L0SDLY_HW_6US;
			break;

		case MAC_AX_PCIE_L0SDLY_7US:
			hwval = PCIE_L0SDLY_HW_7US;
			break;

		default:
			PLTFM_MSG_ERR("[ERR]L0SDLY wt val illegal!\n");
			return MACSTCAL;
		}
		tmp = (hwval << SHFT_L0SDLY) & l0smask;
		*val = (*val & ~(l0smask)) | tmp;
	}

	return MACSUCCESS;
}

static u32 pcie_cfgspc_write(struct mac_ax_adapter *adapter,
			     struct mac_ax_pcie_cfgspc_param *param)
{
	u8 l1_val;
	u8 aspm_val;
	u8 l1ss_val;
	u8 clk_val;
	u8 tmp8;
	struct mac_ax_pcie_cfgspc_param *param_def;
	u32 ret = MACSUCCESS;
	u8 val8;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
		param_def = &pcie_cfgspc_param_def_8852b;
	else
		param_def = &pcie_cfgspc_param_def;

	ret = dbi_r8_pcie(adapter, PCIE_L1_CTRL, &l1_val);
	if (ret != MACSUCCESS)
		return ret;
	ret = dbi_r8_pcie(adapter, PCIE_ASPM_CTRL, &aspm_val);
	if (ret != MACSUCCESS)
		return ret;
	ret = dbi_r8_pcie(adapter, PCIE_L1SS_CTRL, &l1ss_val);
	if (ret != MACSUCCESS)
		return ret;
	ret = dbi_r8_pcie(adapter, PCIE_CLK_CTRL, &clk_val);
	if (ret != MACSUCCESS)
		return ret;

	if (l1_val == 0xFF || aspm_val == 0xFF || l1ss_val == 0xFF ||
	    clk_val == 0xFF) {
		PLTFM_MSG_ERR("[ERR] PCIE CFG reg read 0xFF!\n");
		return MACCMP;
	}

	ret = dbi_r8_pcie(adapter, PCIE_L1_STS, &tmp8);
	if (ret != MACSUCCESS)
		return ret;
	if (((tmp8 & PCIE_BIT_STS_L0S) && param->l0s_ctrl ==
	    MAC_AX_PCIE_DEFAULT) || (param->l0s_ctrl != MAC_AX_PCIE_DEFAULT &&
	    param->l0s_ctrl != MAC_AX_PCIE_IGNORE))
		update_pcie_func(&aspm_val, PCIE_BIT_L0S,
				 param->l0s_ctrl, param_def->l0s_ctrl);

	if (((tmp8 & PCIE_BIT_STS_L1) && param->l1_ctrl ==
	    MAC_AX_PCIE_DEFAULT) || (param->l1_ctrl != MAC_AX_PCIE_DEFAULT &&
	    param->l1_ctrl != MAC_AX_PCIE_IGNORE))
		update_pcie_func(&l1_val, PCIE_BIT_L1,
				 param->l1_ctrl, param_def->l1_ctrl);
	update_pcie_func(&l1_val, PCIE_BIT_WAKE,
			 param->wake_ctrl, param_def->wake_ctrl);
	update_pcie_func(&l1_val, PCIE_BIT_CLK,
			 param->crq_ctrl, param_def->crq_ctrl);
	ret = chk_stus_l1ss(adapter, &val8);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR] PCIE chk_stus_l1ss\n");
		return ret;
	}

	if ((val8 && param->l1ss_ctrl == MAC_AX_PCIE_DEFAULT) ||
	    (param->l1ss_ctrl != MAC_AX_PCIE_DEFAULT &&
	     param->l1ss_ctrl != MAC_AX_PCIE_IGNORE))
		update_pcie_func(&l1ss_val, PCIE_BIT_L1SS,
				 param->l1ss_ctrl, param_def->l1ss_ctrl);

	ret = update_clkdly(adapter, &clk_val, param->clkdly_ctrl,
			    param_def->clkdly_ctrl);
	if (ret != MACSUCCESS)
		return ret;

	ret = update_aspmdly(adapter, &aspm_val, param, param_def);
	if (ret != MACSUCCESS)
		return ret;

	if (param->clkdly_ctrl != MAC_AX_PCIE_CLKDLY_IGNORE) {
		ret = dbi_w8_pcie(adapter, PCIE_CLK_CTRL, clk_val);
		if (ret != MACSUCCESS)
			return ret;
	}

	if (param->l0s_ctrl != MAC_AX_PCIE_IGNORE ||
	    param->l1dly_ctrl != MAC_AX_PCIE_L1DLY_IGNORE ||
	    param->l0sdly_ctrl != MAC_AX_PCIE_L0SDLY_IGNORE) {
		ret = dbi_w8_pcie(adapter, PCIE_ASPM_CTRL, aspm_val);
		if (ret != MACSUCCESS)
			return ret;
	}
	if (param->l1_ctrl != MAC_AX_PCIE_IGNORE ||
	    param->wake_ctrl != MAC_AX_PCIE_IGNORE ||
	    param->crq_ctrl != MAC_AX_PCIE_IGNORE) {
		ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL, l1_val);
		if (ret != MACSUCCESS)
			return ret;
	}
	if (param->l1ss_ctrl != MAC_AX_PCIE_IGNORE) {
		ret = dbi_w8_pcie(adapter, PCIE_L1SS_CTRL, l1ss_val);
		if (ret != MACSUCCESS)
			return ret;
	}

	return ret;
}

u32 pcie_cfgspc_read(struct mac_ax_adapter *adapter,
		     struct mac_ax_pcie_cfgspc_param *param)
{
	u8 l1_val;
	u8 aspm_val;
	u8 l1ss_val;
	u8 clk_val;
	u8 l0smask;
	u8 l1mask;
	u32 ret;

	ret = dbi_r8_pcie(adapter, PCIE_L1_CTRL, &l1_val);
	if (ret != MACSUCCESS)
		return ret;

	ret = dbi_r8_pcie(adapter, PCIE_ASPM_CTRL, &aspm_val);
	if (ret != MACSUCCESS)
		return ret;

	ret = dbi_r8_pcie(adapter, PCIE_L1SS_CTRL, &l1ss_val);
	if (ret != MACSUCCESS)
		return ret;

	ret = dbi_r8_pcie(adapter, PCIE_CLK_CTRL, &clk_val);
	if (ret != MACSUCCESS)
		return ret;

	if (l1_val == 0xFF || aspm_val == 0xFF ||
	    l1ss_val == 0xFF || clk_val == 0xFF) {
		PLTFM_MSG_ERR("[ERR] (2nd)PCIE CFG reg read 0xFF!\n");
		return MACCMP;
	}

	param->l0s_ctrl = GET_PCIE_FUNC_STUS(aspm_val, PCIE_BIT_L0S);
	param->l1_ctrl = GET_PCIE_FUNC_STUS(l1_val, PCIE_BIT_L1);
	param->l1ss_ctrl = GET_PCIE_FUNC_STUS(l1ss_val, PCIE_BIT_L1SS);
	param->wake_ctrl = GET_PCIE_FUNC_STUS(l1_val, PCIE_BIT_WAKE);
	param->crq_ctrl = GET_PCIE_FUNC_STUS(l1_val, PCIE_BIT_CLK);

	switch (clk_val) {
	case PCIE_CLKDLY_HW_0:
		param->clkdly_ctrl = MAC_AX_PCIE_CLKDLY_0;
		break;

	case PCIE_CLKDLY_HW_30US:
		param->clkdly_ctrl = MAC_AX_PCIE_CLKDLY_30US;
		break;

	case PCIE_CLKDLY_HW_50US:
		param->clkdly_ctrl = MAC_AX_PCIE_CLKDLY_50US;
		break;

	case PCIE_CLKDLY_HW_100US:
		param->clkdly_ctrl = MAC_AX_PCIE_CLKDLY_100US;
		break;

	case PCIE_CLKDLY_HW_150US:
		param->clkdly_ctrl = MAC_AX_PCIE_CLKDLY_150US;
		break;

	case PCIE_CLKDLY_HW_200US:
		param->clkdly_ctrl = MAC_AX_PCIE_CLKDLY_200US;
		break;

	default:
		param->clkdly_ctrl = MAC_AX_PCIE_CLKDLY_R_ERR;
		break;
	}

	l0smask = PCIE_ASPMDLY_MASK << SHFT_L0SDLY;
	l1mask = PCIE_ASPMDLY_MASK << SHFT_L1DLY;

	switch ((aspm_val & l0smask) >> SHFT_L0SDLY) {
	case PCIE_L0SDLY_HW_1US:
		param->l0sdly_ctrl = MAC_AX_PCIE_L0SDLY_1US;
		break;

	case PCIE_L0SDLY_HW_3US:
		param->l0sdly_ctrl = MAC_AX_PCIE_L0SDLY_3US;
		break;

	case PCIE_L0SDLY_HW_5US:
		param->l0sdly_ctrl = MAC_AX_PCIE_L0SDLY_5US;
		break;

	case PCIE_L0SDLY_HW_7US:
		param->l0sdly_ctrl = MAC_AX_PCIE_L0SDLY_7US;
		break;

	default:
		param->l0sdly_ctrl = MAC_AX_PCIE_L0SDLY_R_ERR;
		break;
	}

	switch ((aspm_val & l1mask) >> SHFT_L1DLY) {
	case PCIE_L1DLY_HW_16US:
		param->l1dly_ctrl = MAC_AX_PCIE_L1DLY_16US;
		break;

	case PCIE_L1DLY_HW_32US:
		param->l1dly_ctrl = MAC_AX_PCIE_L1DLY_32US;
		break;

	case PCIE_L1DLY_HW_64US:
		param->l1dly_ctrl = MAC_AX_PCIE_L1DLY_64US;
		break;

	case PCIE_L1DLY_HW_INFI:
		param->l1dly_ctrl = MAC_AX_PCIE_L1DLY_INFI;
		break;

	default:
		param->l1dly_ctrl = MAC_AX_PCIE_L1DLY_R_ERR;
		break;
	}

	return ret;
}

u32 cfgspc_set_pcie(struct mac_ax_adapter *adapter,
		    struct mac_ax_pcie_cfgspc_param *param)
{
	u32 status = MACSUCCESS;

	if (param->write == 1)
		status = pcie_cfgspc_write(adapter, param);

	if (param->read == 1)
		status = pcie_cfgspc_read(adapter, param);

	return status;
}

static u32 pcie_ltr_write(struct mac_ax_adapter *adapter,
			  struct mac_ax_pcie_ltr_param *param)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 ltr_ctrl0, ltr_ctrl1, ltr_idle_ltcy, ltr_act_ltcy;
	u32 status = MACSUCCESS;
	struct mac_ax_pcie_ltr_param *param_def = &pcie_ltr_param_def;
	u32 val32;
	enum mac_ax_pcie_func_ctrl ctrl;

	ltr_ctrl0 = MAC_REG_R32(R_AX_LTR_CTRL_0);
	ltr_ctrl1 = MAC_REG_R32(R_AX_LTR_CTRL_1);
	ltr_idle_ltcy = MAC_REG_R32(R_AX_LTR_IDLE_LATENCY);
	ltr_act_ltcy = MAC_REG_R32(R_AX_LTR_ACTIVE_LATENCY);

	if (ltr_ctrl0 == MAC_AX_R32_FF || ltr_ctrl0 == MAC_AX_R32_EA ||
	    ltr_ctrl1 == MAC_AX_R32_FF || ltr_ctrl1 == MAC_AX_R32_EA ||
	    ltr_idle_ltcy == MAC_AX_R32_FF || ltr_idle_ltcy == MAC_AX_R32_EA ||
	    ltr_act_ltcy == MAC_AX_R32_FF || ltr_act_ltcy == MAC_AX_R32_EA) {
		PLTFM_MSG_ERR("[ERR] LTR reg read nothing!\n");
		return MACCMP;
	}

	if (!(param_def->ltr_ctrl == MAC_AX_PCIE_IGNORE ||
	      param->ltr_ctrl == MAC_AX_PCIE_IGNORE ||
	      param_def->ltr_ctrl == MAC_AX_PCIE_DEFAULT)) {
		ctrl = param->ltr_ctrl == MAC_AX_PCIE_DEFAULT ?
		       param_def->ltr_ctrl : param->ltr_ctrl;
		ltr_ctrl0 = ctrl == MAC_AX_PCIE_ENABLE ?
			    (ltr_ctrl0 | B_AX_LTR_EN) :
			    (ltr_ctrl0 & ~B_AX_LTR_EN);
	}

	if (!(param_def->ltr_hw_ctrl == MAC_AX_PCIE_IGNORE ||
	      param->ltr_hw_ctrl == MAC_AX_PCIE_IGNORE ||
	      param_def->ltr_hw_ctrl == MAC_AX_PCIE_DEFAULT)) {
		ctrl = param->ltr_hw_ctrl == MAC_AX_PCIE_DEFAULT ?
		       param_def->ltr_hw_ctrl : param->ltr_hw_ctrl;
		ltr_ctrl0 = ctrl == MAC_AX_PCIE_ENABLE ?
			    (ltr_ctrl0 | B_AX_LTR_HW_EN | B_AX_LTR_WD_NOEMP_CHK) :
			    (ltr_ctrl0 & ~(B_AX_LTR_HW_EN | B_AX_LTR_WD_NOEMP_CHK));
	}

	if (!(param_def->ltr_spc_ctrl == MAC_AX_PCIE_LTR_SPC_IGNORE ||
	      param->ltr_spc_ctrl == MAC_AX_PCIE_LTR_SPC_IGNORE ||
	      param_def->ltr_spc_ctrl == MAC_AX_PCIE_LTR_SPC_DEF)) {
		val32 = param->ltr_spc_ctrl == MAC_AX_PCIE_LTR_SPC_DEF ?
			param_def->ltr_spc_ctrl : param->ltr_spc_ctrl;
		ltr_ctrl0 = SET_CLR_WORD(ltr_ctrl0, val32, B_AX_LTR_SPACE_IDX);
	}

	if (!(param_def->ltr_idle_timer_ctrl ==
	      MAC_AX_PCIE_LTR_IDLE_TIMER_IGNORE ||
	      param->ltr_idle_timer_ctrl ==
	      MAC_AX_PCIE_LTR_IDLE_TIMER_IGNORE ||
	      param_def->ltr_idle_timer_ctrl ==
	      MAC_AX_PCIE_LTR_IDLE_TIMER_DEF)) {
		val32 = param->ltr_idle_timer_ctrl ==
			MAC_AX_PCIE_LTR_IDLE_TIMER_DEF ?
			param_def->ltr_idle_timer_ctrl :
			param->ltr_idle_timer_ctrl;
		ltr_ctrl0 = SET_CLR_WORD(ltr_ctrl0, val32,
					 B_AX_LTR_IDLE_TIMER_IDX);
	}

	if (!(param_def->ltr_rx0_th_ctrl.ctrl == MAC_AX_PCIE_IGNORE ||
	      param->ltr_rx0_th_ctrl.ctrl == MAC_AX_PCIE_IGNORE ||
	      param_def->ltr_rx0_th_ctrl.ctrl == MAC_AX_PCIE_DEFAULT)) {
		val32 = param->ltr_rx0_th_ctrl.ctrl == MAC_AX_PCIE_DEFAULT ?
			param_def->ltr_rx0_th_ctrl.val :
			param->ltr_rx0_th_ctrl.val;
		ltr_ctrl1 = SET_CLR_WORD(ltr_ctrl1, val32, B_AX_LTR_RX0_TH);
	}

	if (!(param_def->ltr_rx1_th_ctrl.ctrl == MAC_AX_PCIE_IGNORE ||
	      param->ltr_rx1_th_ctrl.ctrl == MAC_AX_PCIE_IGNORE ||
	      param_def->ltr_rx1_th_ctrl.ctrl == MAC_AX_PCIE_DEFAULT)) {
		val32 = param->ltr_rx1_th_ctrl.ctrl == MAC_AX_PCIE_DEFAULT ?
			param_def->ltr_rx1_th_ctrl.val :
			param->ltr_rx1_th_ctrl.val;
		ltr_ctrl1 = SET_CLR_WORD(ltr_ctrl1, val32, B_AX_LTR_RX1_TH);
	}

	if (!(param_def->ltr_idle_lat_ctrl.ctrl == MAC_AX_PCIE_IGNORE ||
	      param->ltr_idle_lat_ctrl.ctrl == MAC_AX_PCIE_IGNORE ||
	      param_def->ltr_idle_lat_ctrl.ctrl == MAC_AX_PCIE_DEFAULT)) {
		val32 = param->ltr_idle_lat_ctrl.ctrl == MAC_AX_PCIE_DEFAULT ?
			param_def->ltr_idle_lat_ctrl.val :
			param->ltr_idle_lat_ctrl.val;
		ltr_idle_ltcy =
			SET_CLR_WORD(ltr_idle_ltcy, val32, B_AX_LTR_IDLE_LTCY);
	}

	if (!(param_def->ltr_act_lat_ctrl.ctrl == MAC_AX_PCIE_IGNORE ||
	      param->ltr_act_lat_ctrl.ctrl == MAC_AX_PCIE_IGNORE ||
	      param_def->ltr_act_lat_ctrl.ctrl == MAC_AX_PCIE_DEFAULT)) {
		val32 = param->ltr_act_lat_ctrl.ctrl == MAC_AX_PCIE_DEFAULT ?
			param_def->ltr_act_lat_ctrl.val :
			param->ltr_act_lat_ctrl.val;
		ltr_act_ltcy =
			SET_CLR_WORD(ltr_act_ltcy, val32, B_AX_LTR_ACT_LTCY);
	}

	MAC_REG_W32(R_AX_LTR_CTRL_0, ltr_ctrl0);
	MAC_REG_W32(R_AX_LTR_CTRL_1, ltr_ctrl1);
	MAC_REG_W32(R_AX_LTR_IDLE_LATENCY, ltr_idle_ltcy);
	MAC_REG_W32(R_AX_LTR_ACTIVE_LATENCY, ltr_act_ltcy);

	return status;
}

static u32 pcie_ltr_read(struct mac_ax_adapter *adapter,
			 struct mac_ax_pcie_ltr_param *param)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 status = MACSUCCESS;
	u32 ltr_ctrl0, ltr_ctrl1, ltr_idle_ltcy, ltr_act_ltcy;

	ltr_ctrl0 = MAC_REG_R32(R_AX_LTR_CTRL_0);
	ltr_ctrl1 = MAC_REG_R32(R_AX_LTR_CTRL_1);
	ltr_idle_ltcy = MAC_REG_R32(R_AX_LTR_IDLE_LATENCY);
	ltr_act_ltcy = MAC_REG_R32(R_AX_LTR_ACTIVE_LATENCY);

	if (ltr_ctrl0 == MAC_AX_R32_FF || ltr_ctrl0 == MAC_AX_R32_EA ||
	    ltr_ctrl1 == MAC_AX_R32_FF || ltr_ctrl1 == MAC_AX_R32_EA ||
	    ltr_idle_ltcy == MAC_AX_R32_FF || ltr_idle_ltcy == MAC_AX_R32_EA ||
	    ltr_act_ltcy == MAC_AX_R32_FF || ltr_act_ltcy == MAC_AX_R32_EA) {
		PLTFM_MSG_ERR("[ERR] LTR reg read nothing!\n");
		return MACCMP;
	}

	param->ltr_ctrl = ltr_ctrl0 & B_AX_LTR_EN ?
			  MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE;

	param->ltr_hw_ctrl = ltr_ctrl0 & B_AX_LTR_HW_EN ?
			     MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE;

	param->ltr_spc_ctrl =
		(enum mac_ax_pcie_ltr_spc)
		GET_FIELD(ltr_ctrl0, B_AX_LTR_SPACE_IDX);
	param->ltr_idle_timer_ctrl =
		(enum mac_ax_pcie_ltr_idle_timer)
		GET_FIELD(ltr_ctrl0, B_AX_LTR_IDLE_TIMER_IDX);

	param->ltr_rx0_th_ctrl.val =
		(u16)GET_FIELD(ltr_ctrl1, B_AX_LTR_RX0_TH);
	param->ltr_rx1_th_ctrl.val =
		(u16)GET_FIELD(ltr_ctrl1, B_AX_LTR_RX1_TH);
	param->ltr_idle_lat_ctrl.val =
		GET_FIELD(ltr_idle_ltcy, B_AX_LTR_IDLE_LTCY);
	param->ltr_act_lat_ctrl.val =
		GET_FIELD(ltr_act_ltcy, B_AX_LTR_ACT_LTCY);

	return status;
}

u32 ltr_set_pcie(struct mac_ax_adapter *adapter,
		 struct mac_ax_pcie_ltr_param *param)
{
	u32 ret = MACSUCCESS;

	if (param->write) {
		ret = pcie_ltr_write(adapter, param);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]pcie ltr write fail %d\n", ret);
			return ret;
		}
	}

	if (param->read) {
		ret = pcie_ltr_read(adapter, param);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]pcie ltr read fail %d\n", ret);
			return ret;
		}
	}

	return ret;
}

u32 ltr_sw_trigger(struct mac_ax_adapter *adapter,
		   enum mac_ax_pcie_ltr_sw_ctrl ctrl)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;

	val32 = MAC_REG_R32(R_AX_LTR_CTRL_0);

	if (val32 & B_AX_LTR_HW_EN) {
		PLTFM_MSG_ERR("[ERR]LTR HW mode running\n");
		return MACPROCERR;
	} else if (!(val32 & B_AX_LTR_EN)) {
		PLTFM_MSG_ERR("[ERR]LTR not enable\n");
		return MACHWNOTEN;
	}

	switch (ctrl) {
	case MAC_AX_PCIE_LTR_SW_ACT:
		MAC_REG_W32(R_AX_LTR_CTRL_0, val32 | B_AX_APP_LTR_ACT);
		break;
	case MAC_AX_PCIE_LTR_SW_IDLE:
		MAC_REG_W32(R_AX_LTR_CTRL_0, val32 | B_AX_APP_LTR_IDLE);
		break;
	default:
		PLTFM_MSG_ERR("Invalid input for %s\n", __func__);
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

static u32 pcie_set_sic(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
		return MACSUCCESS;

	MAC_REG_W32(R_AX_PCIE_EXP_CTRL, MAC_REG_R32(R_AX_PCIE_EXP_CTRL) &
		    ~B_AX_SIC_EN_FORCE_CLKREQ);

	return MACSUCCESS;
}

static u32 pcie_set_lbc(struct mac_ax_adapter *adapter,
			enum mac_ax_pcie_func_ctrl ctrl,
			enum mac_ax_lbc_tmr tmr)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_intf_info *intf_info_def;
	u32 val32;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		intf_info_def = &intf_info_def_8852a;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		intf_info_def = &intf_info_def_8852b;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		intf_info_def = &intf_info_def_8852c;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		intf_info_def = &intf_info_def_8192xb;
	} else {
		PLTFM_MSG_ERR("Chip ID is undefined\n");
		return MACCHIPID;
	}

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		val32 = MAC_REG_R32(R_AX_LBC_WATCHDOG);
		if (ctrl == MAC_AX_PCIE_ENABLE ||
		    (ctrl == MAC_AX_PCIE_DEFAULT &&
		     intf_info_def->lbc_en == MAC_AX_PCIE_ENABLE)) {
			val32 = SET_CLR_WORD(val32, tmr == MAC_AX_LBC_TMR_DEF ?
					     intf_info_def->lbc_tmr : tmr,
					     B_AX_LBC_TIMER);
			val32 |= B_AX_LBC_FLAG | B_AX_LBC_EN;
		} else {
			val32 &= ~B_AX_LBC_EN;
		}
		MAC_REG_W32(R_AX_LBC_WATCHDOG, val32);
	}

	return MACSUCCESS;
}

static u32 pcie_set_dbg(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
		return MACSUCCESS;

	MAC_REG_W32(R_AX_PCIE_DBG_CTRL, MAC_REG_R32(R_AX_PCIE_DBG_CTRL) |
					B_AX_ASFF_FULL_NO_STK |
					B_AX_EN_STUCK_DBG);
	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
		MAC_REG_W32(R_AX_PCIE_EXP_CTRL,
			    MAC_REG_R32(R_AX_PCIE_EXP_CTRL) |
			    B_AX_EN_CHKDSC_NO_RX_STUCK);

	return MACSUCCESS;
}

static u32 pcie_set_keep_reg(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	MAC_REG_W32(R_AX_PCIE_INIT_CFG1, MAC_REG_R32(R_AX_PCIE_INIT_CFG1) |
					 B_AX_PCIE_TXRST_KEEP_REG |
					 B_AX_PCIE_RXRST_KEEP_REG);

	return MACSUCCESS;
}

static u32 pcie_set_io_rcy(struct mac_ax_adapter *adapter,
			   enum mac_ax_pcie_func_ctrl ctrl,
			   enum mac_ax_io_rcy_tmr tmr)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_intf_info *intf_info_def;
	u32 val32;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		intf_info_def = &intf_info_def_8852a;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		intf_info_def = &intf_info_def_8852b;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		intf_info_def = &intf_info_def_8852c;
	} else {
		PLTFM_MSG_ERR("Chip ID is undefined\n");
		return MACCHIPID;
	}

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		val32 = 0;
		if (ctrl == MAC_AX_PCIE_ENABLE ||
		    (ctrl == MAC_AX_PCIE_DEFAULT &&
		     intf_info_def->io_rcy_en == MAC_AX_PCIE_ENABLE)) {
			val32 = SET_CLR_WORD(val32,
					     tmr == MAC_AX_IO_RCY_ANA_TMR_DEF ?
					     intf_info_def->io_rcy_tmr : tmr,
					     B_AX_PCIE_WDT_TIMER_M1);
			MAC_REG_W32(R_AX_PCIE_WDT_TIMER_M1, val32);
			MAC_REG_W32(R_AX_PCIE_WDT_TIMER_M2, val32);
			MAC_REG_W32(R_AX_PCIE_WDT_TIMER_E0, val32);
			MAC_REG_W32(R_AX_PCIE_WDT_TIMER_S1, val32);

			val32 = MAC_REG_R32(R_AX_PCIE_IO_RCY_M1);
			val32 |= B_AX_PCIE_IO_RCY_WDT_MODE_M1;
			MAC_REG_W32(R_AX_PCIE_IO_RCY_M1, val32);

			val32 = MAC_REG_R32(R_AX_PCIE_IO_RCY_M2);
			val32 |= B_AX_PCIE_IO_RCY_WDT_MODE_M2;
			MAC_REG_W32(R_AX_PCIE_IO_RCY_M2, val32);

			val32 = MAC_REG_R32(R_AX_PCIE_IO_RCY_E0);
			val32 |= B_AX_PCIE_IO_RCY_WDT_MODE_E0;
			MAC_REG_W32(R_AX_PCIE_IO_RCY_E0, val32);

			val32 = MAC_REG_R32(R_AX_PCIE_IO_RCY_S1);
			val32 |= B_AX_PCIE_IO_RCY_WDT_MODE_S1;
			MAC_REG_W32(R_AX_PCIE_IO_RCY_S1, val32);
		} else {
			val32 = MAC_REG_R32(R_AX_PCIE_IO_RCY_M1);
			val32 &= ~B_AX_PCIE_IO_RCY_WDT_MODE_M1;
			MAC_REG_W32(R_AX_PCIE_IO_RCY_M1, val32);

			val32 = MAC_REG_R32(R_AX_PCIE_IO_RCY_M2);
			val32 &= ~B_AX_PCIE_IO_RCY_WDT_MODE_M2;
			MAC_REG_W32(R_AX_PCIE_IO_RCY_M2, val32);

			val32 = MAC_REG_R32(R_AX_PCIE_IO_RCY_E0);
			val32 &= ~B_AX_PCIE_IO_RCY_WDT_MODE_E0;
			MAC_REG_W32(R_AX_PCIE_IO_RCY_E0, val32);

			val32 = MAC_REG_R32(R_AX_PCIE_IO_RCY_S1);
			val32 &= ~B_AX_PCIE_IO_RCY_WDT_MODE_S1;
			MAC_REG_W32(R_AX_PCIE_IO_RCY_S1, val32);
		}
	}

	return MACSUCCESS;
}

static void ctrl_hci_dma_en_pcie(struct mac_ax_adapter *adapter,
				 enum mac_ax_func_sw en)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (en == MAC_AX_FUNC_EN)
		MAC_REG_W32(R_AX_HCI_FUNC_EN,
			    MAC_REG_R32(R_AX_HCI_FUNC_EN) | B_AX_HCI_TXDMA_EN |
			    B_AX_HCI_RXDMA_EN);
	else
		MAC_REG_W32(R_AX_HCI_FUNC_EN,
			    MAC_REG_R32(R_AX_HCI_FUNC_EN) &
			    ~(B_AX_HCI_TXDMA_EN | B_AX_HCI_RXDMA_EN));
}

static u32 poll_io_idle_pcie(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 cnt;

	cnt = PCIE_POLL_IO_IDLE_CNT;
	val32 = B_AX_PCIEIO_BUSY | B_AX_PCIEIO_TX_BUSY | B_AX_PCIEIO_RX_BUSY;
	while (cnt && (MAC_REG_R32(R_AX_PCIE_DMA_BUSY1) & val32)) {
		cnt--;
		PLTFM_DELAY_US(PCIE_POLL_IO_IDLE_DLY_US);
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]PCIE dmach busy1 0x%X\n",
			      MAC_REG_R32(R_AX_PCIE_DMA_BUSY1));
		return MACPOLLTO;
	}

	return MACSUCCESS;
}

static u32 get_txbd_desc_reg(struct mac_ax_adapter *adapter, u8 dma_ch,
			     u32 *addr_l, u32 *addr_h)
{
	u8 c_id = adapter->hw_info->chip_id;

	if ((is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) &&
	    ((dma_ch >= MAC_AX_DMA_ACH4 && dma_ch <= MAC_AX_DMA_ACH7) ||
	     (dma_ch >= MAC_AX_DMA_B1MG && dma_ch <= MAC_AX_DMA_B1HI))) {
		PLTFM_MSG_ERR("[ERR] TXBD desc CH%d not support in 8852B\n",
			      dma_ch);
		return MACHWNOSUP;
	}

	switch (dma_ch) {
	case MAC_AX_DMA_ACH0:
		*addr_l = ach0_txbd_desa_l_reg[c_id];
		*addr_h = ach0_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_ACH1:
		*addr_l = ach1_txbd_desa_l_reg[c_id];
		*addr_h = ach1_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_ACH2:
		*addr_l = ach2_txbd_desa_l_reg[c_id];
		*addr_h = ach2_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_ACH3:
		*addr_l = ach3_txbd_desa_l_reg[c_id];
		*addr_h = ach3_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_ACH4:
		*addr_l = ach4_txbd_desa_l_reg[c_id];
		*addr_h = ach4_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_ACH5:
		*addr_l = ach5_txbd_desa_l_reg[c_id];
		*addr_h = ach5_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_ACH6:
		*addr_l = ach6_txbd_desa_l_reg[c_id];
		*addr_h = ach6_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_ACH7:
		*addr_l = ach7_txbd_desa_l_reg[c_id];
		*addr_h = ach7_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_B0MG:
		*addr_l = ch8_txbd_desa_l_reg[c_id];
		*addr_h = ch8_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_B0HI:
		*addr_l = ch9_txbd_desa_l_reg[c_id];
		*addr_h = ch9_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_B1MG:
		*addr_l = ch10_txbd_desa_l_reg[c_id];
		*addr_h = ch10_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_B1HI:
		*addr_l = ch11_txbd_desa_l_reg[c_id];
		*addr_h = ch11_txbd_desa_h_reg[c_id];
		break;
	case MAC_AX_DMA_H2C:
		*addr_l = ch12_txbd_desa_l_reg[c_id];
		*addr_h = ch12_txbd_desa_h_reg[c_id];
		break;
	default:
		PLTFM_MSG_ERR("[ERR] TXBD desc CH%d invalid\n", dma_ch);
		return MACFUNCINPUT;
	}
	return MACSUCCESS;
}

static u32 get_rxbd_desc_reg(struct mac_ax_adapter *adapter, u8 dma_ch,
			     u32 *addr_l, u32 *addr_h)
{
	u8 c_id = adapter->hw_info->chip_id;

	switch (dma_ch) {
	case MAC_AX_RX_CH_RXQ:
		*addr_l = rxq_rxbd_desa_l_reg[c_id];
		*addr_h = rxq_rxbd_desa_h_reg[c_id];
		break;
	case MAC_AX_RX_CH_RPQ:
		*addr_l = rpq_rxbd_desa_l_reg[c_id];
		*addr_h = rpq_rxbd_desa_h_reg[c_id];
		break;
	default:
		PLTFM_MSG_ERR("[ERR] RXBD desc CH%d invalid\n", dma_ch);
		return MACFUNCINPUT;
	}
	return MACSUCCESS;
}

static u32 get_txbd_num_reg(struct mac_ax_adapter *adapter, u8 dma_ch, u32 *reg)
{
	u8 c_id = adapter->hw_info->chip_id;

	if ((is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) &&
	    ((dma_ch >= MAC_AX_DMA_ACH4 && dma_ch <= MAC_AX_DMA_ACH7) ||
	     (dma_ch >= MAC_AX_DMA_B1MG && dma_ch <= MAC_AX_DMA_B1HI))) {
		PLTFM_MSG_ERR("[ERR] TXBD num CH%d not support in 8852B\n",
			      dma_ch);
		return MACHWNOSUP;
	}

	switch (dma_ch) {
	case MAC_AX_DMA_ACH0:
		*reg = R_AX_ACH0_TXBD_NUM;
		break;
	case MAC_AX_DMA_ACH1:
		*reg = R_AX_ACH1_TXBD_NUM;
		break;
	case MAC_AX_DMA_ACH2:
		*reg = R_AX_ACH2_TXBD_NUM;
		break;
	case MAC_AX_DMA_ACH3:
		*reg = R_AX_ACH3_TXBD_NUM;
		break;
	case MAC_AX_DMA_ACH4:
		*reg = R_AX_ACH4_TXBD_NUM;
		break;
	case MAC_AX_DMA_ACH5:
		*reg = R_AX_ACH5_TXBD_NUM;
		break;
	case MAC_AX_DMA_ACH6:
		*reg = R_AX_ACH6_TXBD_NUM;
		break;
	case MAC_AX_DMA_ACH7:
		*reg = R_AX_ACH7_TXBD_NUM;
		break;
	case MAC_AX_DMA_B0MG:
		*reg = R_AX_CH8_TXBD_NUM;
		break;
	case MAC_AX_DMA_B0HI:
		*reg = R_AX_CH9_TXBD_NUM;
		break;
	case MAC_AX_DMA_B1MG:
		*reg = ch10_txbd_num_reg[c_id];
		break;
	case MAC_AX_DMA_B1HI:
		*reg = ch11_txbd_num_reg[c_id];
		break;
	case MAC_AX_DMA_H2C:
		*reg = R_AX_CH12_TXBD_NUM;
		break;
	default:
		PLTFM_MSG_ERR("[ERR] TXBD num CH%d invalid\n", dma_ch);
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

static u32 get_txbd_idx_reg(struct mac_ax_adapter *adapter, u8 dma_ch, u32 *reg)
{
	if ((is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) &&
	    ((dma_ch >= MAC_AX_DMA_ACH4 && dma_ch <= MAC_AX_DMA_ACH7) ||
	     (dma_ch >= MAC_AX_DMA_B1MG && dma_ch <= MAC_AX_DMA_B1HI))) {
		PLTFM_MSG_ERR("[ERR] TXBD idx CH%d not support in 8852B\n",
			      dma_ch);
		return MACHWNOSUP;
	}

	switch (dma_ch) {
	case MAC_AX_DMA_ACH0:
		*reg = R_AX_ACH0_TXBD_IDX;
		break;
	case MAC_AX_DMA_ACH1:
		*reg = R_AX_ACH1_TXBD_IDX;
		break;
	case MAC_AX_DMA_ACH2:
		*reg = R_AX_ACH2_TXBD_IDX;
		break;
	case MAC_AX_DMA_ACH3:
		*reg = R_AX_ACH3_TXBD_IDX;
		break;
	case MAC_AX_DMA_ACH4:
		*reg = R_AX_ACH4_TXBD_IDX;
		break;
	case MAC_AX_DMA_ACH5:
		*reg = R_AX_ACH5_TXBD_IDX;
		break;
	case MAC_AX_DMA_ACH6:
		*reg = R_AX_ACH6_TXBD_IDX;
		break;
	case MAC_AX_DMA_ACH7:
		*reg = R_AX_ACH7_TXBD_IDX;
		break;
	case MAC_AX_DMA_B0MG:
		*reg = R_AX_CH8_TXBD_IDX;
		break;
	case MAC_AX_DMA_B0HI:
		*reg = R_AX_CH9_TXBD_IDX;
		break;
	case MAC_AX_DMA_B1MG:
		*reg = R_AX_CH10_TXBD_IDX;
		break;
	case MAC_AX_DMA_B1HI:
		*reg = R_AX_CH11_TXBD_IDX;
		break;
	case MAC_AX_DMA_H2C:
		*reg = R_AX_CH12_TXBD_IDX;
		break;
	default:
		PLTFM_MSG_ERR("[ERR] TXBD idx CH%d invalid\n", dma_ch);
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

static u32 get_rxbd_num_reg(struct mac_ax_adapter *adapter, u8 dma_ch, u32 *reg)
{
	u8 c_id = adapter->hw_info->chip_id;

	switch (dma_ch) {
	case MAC_AX_RX_CH_RXQ:
		*reg = rxq_rxbd_num_reg[c_id];
		break;
	case MAC_AX_RX_CH_RPQ:
		*reg = rpq_rxbd_num_reg[c_id];
		break;
	default:
		PLTFM_MSG_ERR("[ERR] RXBD num CH%d invalid\n", dma_ch);
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

static u32 get_txbd_ram_reg(struct mac_ax_adapter *adapter, u8 dma_ch, u32 *reg)
{
	u8 c_id = adapter->hw_info->chip_id;

	if ((is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) &&
	    ((dma_ch >= MAC_AX_DMA_ACH4 && dma_ch <= MAC_AX_DMA_ACH7) ||
	     (dma_ch >= MAC_AX_DMA_B1MG && dma_ch <= MAC_AX_DMA_B1HI))) {
		PLTFM_MSG_ERR("[ERR] TXBD ram CH%d not support in 8852B\n",
			      dma_ch);
		return MACHWNOSUP;
	}

	switch (dma_ch) {
	case MAC_AX_DMA_ACH0:
		*reg = ach0_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_ACH1:
		*reg = ach1_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_ACH2:
		*reg = ach2_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_ACH3:
		*reg = ach3_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_ACH4:
		*reg = ach4_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_ACH5:
		*reg = ach5_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_ACH6:
		*reg = ach6_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_ACH7:
		*reg = ach7_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_B0MG:
		*reg = ch8_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_B0HI:
		*reg = ch9_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_B1MG:
		*reg = ch10_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_B1HI:
		*reg = ch11_bdram_ctrl_reg[c_id];
		break;
	case MAC_AX_DMA_H2C:
		*reg = ch12_bdram_ctrl_reg[c_id];
		break;
	default:
		PLTFM_MSG_ERR("[ERR] TXBD ram CH%d invalid\n", dma_ch);
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

static u32 mode_op(struct mac_ax_adapter *adapter,
		   struct mac_ax_intf_info *intf_info)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 c_id = adapter->hw_info->chip_id;
	u32 val32;
	struct tx_base_desc *txbd = (struct tx_base_desc *)intf_info->txbd_buf;
	struct rx_base_desc *rxbd = (struct rx_base_desc *)intf_info->rxbd_buf;
	enum mac_ax_bd_trunc_mode *txbd_trunc_mode =
		(&intf_info->txbd_trunc_mode);
	enum mac_ax_bd_trunc_mode *rxbd_trunc_mode =
		(&intf_info->rxbd_trunc_mode);
	enum mac_ax_rxbd_mode *rxbd_mode = (&intf_info->rxbd_mode);
	enum mac_ax_tag_mode *tag_mode = (&intf_info->tag_mode);
	enum mac_ax_multi_tag_num *multi_tag_num = (&intf_info->multi_tag_num);
	enum mac_ax_wd_dma_intvl *wd_dma_idle_intvl =
		(&intf_info->wd_dma_idle_intvl);
	enum mac_ax_wd_dma_intvl *wd_dma_act_intvl =
		(&intf_info->wd_dma_act_intvl);
	enum mac_ax_tx_burst *tx_burst = &intf_info->tx_burst;
	enum mac_ax_rx_burst *rx_burst = &intf_info->rx_burst;
	struct mac_ax_intf_info *intf_info_def;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		intf_info_def = &intf_info_def_8852a;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		intf_info_def = &intf_info_def_8852b;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		intf_info_def = &intf_info_def_8852c;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		intf_info_def = &intf_info_def_8192xb;
	} else {
		PLTFM_MSG_ERR("Chip ID is undefined\n");
		return MACCHIPID;
	}

	if (intf_info->txbd_trunc_mode == MAC_AX_BD_DEF)
		txbd_trunc_mode = (&intf_info_def->txbd_trunc_mode);

	if (intf_info->rxbd_trunc_mode == MAC_AX_BD_DEF)
		rxbd_trunc_mode = (&intf_info_def->rxbd_trunc_mode);

	if ((*txbd_trunc_mode) == MAC_AX_BD_TRUNC) {
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) &&
		    is_cv(adapter, CBV)) {
			val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1) | B_AX_TX_TRUNC_MODE;
			MAC_REG_W32(R_AX_PCIE_INIT_CFG1, val32);
			MAC_REG_W32(R_AX_TXDMA_ADDR_H, txbd->phy_addr_h);
		}
	} else if ((*txbd_trunc_mode) == MAC_AX_BD_NORM) {
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
			val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1) & ~B_AX_TX_TRUNC_MODE;
			MAC_REG_W32(R_AX_PCIE_INIT_CFG1, val32);
		}
	}

	if ((*rxbd_trunc_mode) == MAC_AX_BD_TRUNC) {
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) &&
		    is_cv(adapter, CBV)) {
			val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1) | B_AX_RX_TRUNC_MODE;
			PLTFM_REG_W32(R_AX_PCIE_INIT_CFG1, val32);
			PLTFM_REG_W32(R_AX_RXDMA_ADDR_H, rxbd->phy_addr_h);
		}
	} else if ((*rxbd_trunc_mode) == MAC_AX_BD_NORM) {
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
			val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1) & ~B_AX_RX_TRUNC_MODE;
			MAC_REG_W32(R_AX_PCIE_INIT_CFG1, val32);
		}
	}

	if (intf_info->rxbd_mode == MAC_AX_RXBD_DEF)
		rxbd_mode = (&intf_info_def->rxbd_mode);

	if ((*rxbd_mode) == MAC_AX_RXBD_PKT) {
		val32 = MAC_REG_R32(init_cfg_reg[c_id]) & ~rxbd_mode_bit[c_id];
		MAC_REG_W32(init_cfg_reg[c_id], val32);
	} else if ((*rxbd_mode) == MAC_AX_RXBD_SEP) {
		val32 = MAC_REG_R32(init_cfg_reg[c_id]) | rxbd_mode_bit[c_id];
		MAC_REG_W32(init_cfg_reg[c_id], val32);

		if (intf_info->rx_sep_append_len > 0x3FFF) {
			PLTFM_MSG_ERR("rx sep app len %d\n",
				      intf_info->rx_sep_append_len);
			return MACFUNCINPUT;
		}
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
			val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG2);
			val32 = SET_CLR_WORD(val32, intf_info->rx_sep_append_len,
					     B_AX_PCIE_RX_APPLEN);
			MAC_REG_W32(R_AX_PCIE_INIT_CFG2, val32);
		}
	}

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (intf_info->tx_burst == MAC_AX_TX_BURST_DEF)
			tx_burst = &intf_info_def->tx_burst;
		if (intf_info->rx_burst == MAC_AX_RX_BURST_DEF)
			rx_burst = &intf_info_def->rx_burst;
		val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1);
		val32 = SET_CLR_WORD(val32, *tx_burst, B_AX_PCIE_MAX_TXDMA);
		val32 = SET_CLR_WORD(val32, *rx_burst, B_AX_PCIE_MAX_RXDMA);
		MAC_REG_W32(R_AX_PCIE_INIT_CFG1, val32);
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		if (intf_info->tx_burst == MAC_AX_TX_BURST_DEF)
			tx_burst = &intf_info_def->tx_burst;
		if (intf_info->rx_burst == MAC_AX_RX_BURST_DEF)
			rx_burst = &intf_info_def->rx_burst;
		val32 = MAC_REG_R32(R_AX_HAXI_INIT_CFG1);
		val32 = SET_CLR_WORD(val32, *tx_burst, B_AX_HAXI_MAX_TXDMA);
		val32 = SET_CLR_WORD(val32, *rx_burst, B_AX_HAXI_MAX_RXDMA);
		MAC_REG_W32(R_AX_HAXI_INIT_CFG1, val32);
	} else {
		PLTFM_MSG_ERR("Chip ID is undefined\n");
		return MACCHIPID;
	}

	if (intf_info->tag_mode == MAC_AX_TAG_DEF)
		tag_mode = (&intf_info_def->tag_mode);
	if (intf_info->multi_tag_num == MAC_AX_TAG_NUM_DEF)
		multi_tag_num = (&intf_info_def->multi_tag_num);
	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if ((*tag_mode) == MAC_AX_TAG_SGL) {
			val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1) &
					    ~B_AX_LATENCY_CONTROL;
			MAC_REG_W32(R_AX_PCIE_INIT_CFG1, val32);
		} else if ((*tag_mode) == MAC_AX_TAG_MULTI) {
			val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1) |
					    B_AX_LATENCY_CONTROL;
			MAC_REG_W32(R_AX_PCIE_INIT_CFG1, val32);
		}
	}

	val32 = MAC_REG_R32(exp_ctrl_reg[c_id]);
	val32 = SET_CLR_WOR2(val32, *multi_tag_num, max_tag_num_sh[c_id],
			     max_tag_num_msk[c_id]);
	MAC_REG_W32(exp_ctrl_reg[c_id], val32);

	if (intf_info->wd_dma_act_intvl == MAC_AX_WD_DMA_INTVL_DEF)
		wd_dma_act_intvl = (&intf_info_def->wd_dma_act_intvl);
	if (intf_info->wd_dma_idle_intvl == MAC_AX_WD_DMA_INTVL_DEF)
		wd_dma_idle_intvl = (&intf_info_def->wd_dma_idle_intvl);
	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG2);
		val32 = SET_CLR_WORD(val32, *wd_dma_idle_intvl,
				     B_AX_WD_ITVL_IDLE);
		val32 = SET_CLR_WORD(val32, *wd_dma_act_intvl,
				     B_AX_WD_ITVL_ACT);
		MAC_REG_W32(R_AX_PCIE_INIT_CFG2, val32);
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		   is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		val32 = MAC_REG_R32(R_AX_HAXI_INIT_CFG1);
		val32 = SET_CLR_WORD(val32, *wd_dma_idle_intvl,
				     B_AX_WD_ITVL_IDLE_V1);
		val32 = SET_CLR_WORD(val32, *wd_dma_act_intvl,
				     B_AX_WD_ITVL_ACT_V1);
		MAC_REG_W32(R_AX_HAXI_INIT_CFG1, val32);
	} else {
		PLTFM_MSG_ERR("Chip ID is undefined\n");
		return MACCHIPID;
	}

	return MACSUCCESS;
}

static u32 trx_init_bd(struct mac_ax_adapter *adapter,
		       struct mac_ax_intf_info *intf_info)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct tx_base_desc *txbd = (struct tx_base_desc *)intf_info->txbd_buf;
	struct rx_base_desc *rxbd = (struct rx_base_desc *)intf_info->rxbd_buf;
	enum mac_ax_bd_trunc_mode *txbd_trunc_mode =
		(&intf_info->txbd_trunc_mode);
	enum mac_ax_bd_trunc_mode *rxbd_trunc_mode =
		(&intf_info->rxbd_trunc_mode);
	enum mac_ax_rxbd_mode *rxbd_mode = (&intf_info->rxbd_mode);
	struct txbd_ram *bdram_tbl;
	struct mac_ax_intf_info *intf_info_def;
	u32 reg_addr_l, reg_addr_h, reg32, val32, bd_num;
	u16 val16;
	u8 ch, bdram_idx;
	u32 ret = MACSUCCESS;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		bdram_tbl = mac_bdram_tbl_8852a;
		intf_info_def = &intf_info_def_8852a;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		bdram_tbl = mac_bdram_tbl_8852b;
		intf_info_def = &intf_info_def_8852b;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
		bdram_tbl = mac_bdram_tbl_8852c;
		intf_info_def = &intf_info_def_8852c;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		bdram_tbl = mac_bdram_tbl_8192xb;
		intf_info_def = &intf_info_def_8192xb;
	} else {
		PLTFM_MSG_ERR("Chip ID is undefined\n");
		return MACCHIPID;
	}

	if (intf_info->txbd_trunc_mode == MAC_AX_BD_DEF)
		txbd_trunc_mode = (&intf_info_def->txbd_trunc_mode);

	if (intf_info->rxbd_trunc_mode == MAC_AX_BD_DEF)
		rxbd_trunc_mode = (&intf_info_def->rxbd_trunc_mode);

	if (intf_info->rxbd_mode == MAC_AX_RXBD_DEF)
		rxbd_mode = (&intf_info_def->rxbd_mode);

	if (*txbd_trunc_mode == MAC_AX_BD_TRUNC) {
		val32 = MAC_REG_R32(R_AX_TX_ADDRESS_INFO_MODE_SETTING) |
			B_AX_HOST_ADDR_INFO_8B_SEL;
		MAC_REG_W32(R_AX_TX_ADDRESS_INFO_MODE_SETTING, val32);

		val32 = MAC_REG_R32(R_AX_PKTIN_SETTING) &
			~B_AX_WD_ADDR_INFO_LENGTH;
		MAC_REG_W32(R_AX_PKTIN_SETTING, val32);
	} else if (*txbd_trunc_mode == MAC_AX_BD_NORM) {
		val32 = MAC_REG_R32(R_AX_TX_ADDRESS_INFO_MODE_SETTING) &
			~B_AX_HOST_ADDR_INFO_8B_SEL;
		MAC_REG_W32(R_AX_TX_ADDRESS_INFO_MODE_SETTING, val32);

		val32 = MAC_REG_R32(R_AX_PKTIN_SETTING) |
			B_AX_WD_ADDR_INFO_LENGTH;
		MAC_REG_W32(R_AX_PKTIN_SETTING, val32);
	} else {
		PLTFM_MSG_ERR("pcie init txbd_trunc_mode %d illegal\n",
			      intf_info->txbd_trunc_mode);
		return MACFUNCINPUT;
	}

	bdram_idx = 0;
	for (ch = MAC_AX_DMA_ACH0; ch < MAC_AX_DMA_CH_NUM; ch++) {
		if ((is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) &&
		    ((ch >= MAC_AX_DMA_ACH4 && ch <= MAC_AX_DMA_ACH7) ||
		     (ch >= MAC_AX_DMA_B1MG && ch <= MAC_AX_DMA_B1HI)))
			continue;
		if (txbd[ch].phy_addr_l % TXBD_BYTE_ALIGN) {
			PLTFM_MSG_ERR("[ERR]ch%d txbd phyaddr 0x%X not %dB align\n",
				      ch, txbd[ch].phy_addr_l, TXBD_BYTE_ALIGN);
			return MACBADDR;
		}

		ret = get_txbd_desc_reg(adapter, ch, &reg_addr_l, &reg_addr_h);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]get_txbd_desc_reg\n");
			return ret;
		}
		MAC_REG_W32(reg_addr_l, txbd[ch].phy_addr_l);
		MAC_REG_W32(reg_addr_h, txbd[ch].phy_addr_h);

		bd_num = (*txbd_trunc_mode == MAC_AX_BD_TRUNC) ?
			 (txbd[ch].buf_len / BD_TRUNC_SIZE) :
			 (txbd[ch].buf_len / BD_NORM_SIZE);
		if (bd_num > BD_MAX_NUM) {
			PLTFM_MSG_ERR("ch%d txbd num %d\n", ch, bd_num);
			return MACFUNCINPUT;
		}

		ret = get_txbd_num_reg(adapter, ch, &reg32);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]get_txbd_num_reg\n");
			return ret;
		}

		val16 = SET_CLR_WORD(MAC_REG_R16(reg32), bd_num,
				     B_AX_ACH0_DESC_NUM);
		MAC_REG_W16(reg32, val16);

		ret = get_txbd_ram_reg(adapter, ch, &reg32);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]get_txbd_ram_reg\n");
			return ret;
		}

		val32 = MAC_REG_R32(reg32);
		val32 = SET_CLR_WORD(val32, bdram_tbl[bdram_idx].sidx,
				     B_AX_ACH0_BDRAM_SIDX);
		val32 = SET_CLR_WORD(val32, bdram_tbl[bdram_idx].max,
				     B_AX_ACH0_BDRAM_MAX);
		val32 = SET_CLR_WORD(val32, bdram_tbl[bdram_idx].min,
				     B_AX_ACH0_BDRAM_MIN);
		MAC_REG_W32(reg32, val32);
		bdram_idx++;
	}

	for (ch = MAC_AX_RX_CH_RXQ; ch < MAC_AX_RX_CH_NUM; ch++) {
		if (rxbd[ch].phy_addr_l % RXBD_BYTE_ALIGN) {
			PLTFM_MSG_ERR("[ERR]ch%d rxbd phyaddr 0x%X not %dB align\n",
				      ch, rxbd[ch].phy_addr_l, RXBD_BYTE_ALIGN);
			return MACBADDR;
		}

		ret = get_rxbd_desc_reg(adapter, ch, &reg_addr_l, &reg_addr_h);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]get_rxbd_desc_reg\n");
			return ret;
		}

		MAC_REG_W32(reg_addr_l, rxbd[ch].phy_addr_l);
		MAC_REG_W32(reg_addr_h, rxbd[ch].phy_addr_h);

		ret = get_rxbd_num_reg(adapter, ch, &reg32);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]get_rxbd_num_reg\n");
			return ret;
		}

		if (ch == MAC_AX_RX_CH_RXQ) {
			bd_num = (*rxbd_mode == MAC_AX_RXBD_PKT) ?
				 (rxbd[ch].buf_len / BD_TRUNC_SIZE) :
				 (rxbd[ch].buf_len / RXBD_SEP_TRUNC_NEW_SIZE);
		} else {
			bd_num = (*rxbd_trunc_mode == MAC_AX_BD_TRUNC) ?
			 (rxbd[ch].buf_len / BD_TRUNC_SIZE) :
			 (rxbd[ch].buf_len / BD_NORM_SIZE);
		}

		if (bd_num > BD_MAX_NUM) {
			PLTFM_MSG_ERR("ch%d rxbd num %d\n", ch, bd_num);
			return MACFUNCINPUT;
		}

		val16 = SET_CLR_WORD(MAC_REG_R16(reg32), bd_num,
				     B_AX_RXQ_DESC_NUM);
		MAC_REG_W16(reg32, val16);
	}

	return MACSUCCESS;
}

static u32 _patch_pcie_power_wake(struct mac_ax_adapter *adapter, u8 pwr_state)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (pwr_state == PC_POWER_UP) {
		MAC_REG_W32(R_AX_HCI_OPT_CTRL, MAC_REG_R32(R_AX_HCI_OPT_CTRL)
			    | BIT_WAKE_CTRL);
	} else if (pwr_state == PC_POWER_DOWN) {
		MAC_REG_W32(R_AX_HCI_OPT_CTRL, MAC_REG_R32(R_AX_HCI_OPT_CTRL)
			    & ~BIT_WAKE_CTRL);
	} else {
		PLTFM_MSG_ERR("[ERR] patch power wake input: %d\n", pwr_state);
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

static u32 _patch_pcie_dphy_delay(struct mac_ax_adapter *adapter)
{
	u16 val16;
	u32 ret = MACSUCCESS;

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
		return MACSUCCESS;

	ret = mdio_r16_pcie(adapter, RAC_REG_REV2, MAC_AX_PCIE_PHY_GEN1,
			    &val16);
	if (ret != MACSUCCESS)
		return ret;
	val16 = SET_CLR_WOR2(val16, PCIE_DPHY_DLY_25US, BAC_CMU_EN_DLY_SH,
			     BAC_CMU_EN_DLY_MSK);
	ret = mdio_w16_pcie(adapter, RAC_REG_REV2, val16,
			    MAC_AX_PCIE_PHY_GEN1);
	if (ret != MACSUCCESS)
		return ret;

	return MACSUCCESS;
}

static u32 _patch_pcie_autok_x(struct mac_ax_adapter *adapter)
{
	u16 val16;
	u32 ret = MACSUCCESS;

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
		return MACSUCCESS;

	ret = mdio_r16_pcie(adapter, RAC_REG_FLD_0, MAC_AX_PCIE_PHY_GEN1,
			    &val16);
	if (ret != MACSUCCESS)
		return ret;
	val16 = SET_CLR_WOR2(val16, PCIE_AUTOK_4, BAC_AUTOK_N_SH,
			     BAC_AUTOK_N_MSK);
	ret = mdio_w16_pcie(adapter, RAC_REG_FLD_0, val16,
			    MAC_AX_PCIE_PHY_GEN1);
	if (ret != MACSUCCESS)
		return ret;

	return MACSUCCESS;
}

static u32 _patch_pcie_refclk_autok(struct mac_ax_adapter *adapter,
				    struct mac_ax_intf_info *intf_info)
{
	u32 ret;
	struct mac_ax_intf_info *intf_info_def;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		if (is_cv(adapter, CBV))
			return MACSUCCESS;
	} else if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		return MACSUCCESS;
	} else {
		//cut_ver_checker
	}

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		intf_info_def = &intf_info_def_8852a;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		intf_info_def = &intf_info_def_8852b;
	} else {
		PLTFM_MSG_ERR("[ERR] Invalid IC\n");
		return MACNOTSUP;
	}

	if (intf_info->autok_en == MAC_AX_PCIE_DEFAULT)
		intf_info->autok_en = intf_info_def->autok_en;

	if (intf_info->autok_en != MAC_AX_PCIE_IGNORE) {
		ret = mac_auto_refclk_cal_pcie(adapter, intf_info->autok_en);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR] pcie autok fail %d\n", ret);
			return ret;
		}
	}

	return MACSUCCESS;
}

static u32 _patch_pcie_deglitch(struct mac_ax_adapter *adapter)
{
	u32 ret;
	u16 val16, bit_set;

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
		return MACSUCCESS;

	bit_set = BIT11 | BIT10 | BIT9 | BIT8;

	ret = mdio_r16_pcie(adapter, RAC_ANA24, MAC_AX_PCIE_PHY_GEN1, &val16);
	if (ret != MACSUCCESS)
		return ret;
	val16 &= ~bit_set;
	ret = mdio_w16_pcie(adapter, RAC_ANA24, val16, MAC_AX_PCIE_PHY_GEN1);
	if (ret != MACSUCCESS)
		return ret;

	ret = mdio_r16_pcie(adapter, RAC_ANA24, MAC_AX_PCIE_PHY_GEN2, &val16);
	if (ret != MACSUCCESS)
		return ret;
	val16 &= ~bit_set;
	ret = mdio_w16_pcie(adapter, RAC_ANA24, val16, MAC_AX_PCIE_PHY_GEN2);
	if (ret != MACSUCCESS)
		return ret;

	return MACSUCCESS;
}

static u32 _patch_pcie_l2_rxen_lat(struct mac_ax_adapter *adapter)
{
	u32 ret;
	u16 val16, bit_set;

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
		return MACSUCCESS;

	bit_set = BIT15 | BIT14;

	ret = mdio_r16_pcie(adapter, RAC_ANA26, MAC_AX_PCIE_PHY_GEN1, &val16);
	if (ret != MACSUCCESS)
		return ret;
	val16 &= ~bit_set;
	ret = mdio_w16_pcie(adapter, RAC_ANA26, val16, MAC_AX_PCIE_PHY_GEN1);
	if (ret != MACSUCCESS)
		return ret;

	ret = mdio_r16_pcie(adapter, RAC_ANA26, MAC_AX_PCIE_PHY_GEN2, &val16);
	if (ret != MACSUCCESS)
		return ret;
	val16 &= ~bit_set;
	ret = mdio_w16_pcie(adapter, RAC_ANA26, val16, MAC_AX_PCIE_PHY_GEN2);
	if (ret != MACSUCCESS)
		return ret;

	return MACSUCCESS;
}

static u32 _patch_pcie_l1off_pwroff(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
		return MACSUCCESS;

	MAC_REG_W32(R_AX_PCIE_PS_CTRL,
		    MAC_REG_R32(R_AX_PCIE_PS_CTRL) & ~B_AX_L1OFF_PWR_OFF_EN);

	return MACSUCCESS;
}

static u32 _patch_pcie_aphy_pwrcut(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
		return MACSUCCESS;

	MAC_REG_W32(R_AX_SYS_PW_CTRL,
		    MAC_REG_R32(R_AX_SYS_PW_CTRL) & ~B_AX_PSUS_OFF_CAPC_EN);

	return MACSUCCESS;
}

static u32 _patch_pcie_hci_ldo(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
		return MACSUCCESS;

	val32 = MAC_REG_R32(R_AX_SYS_SDIO_CTRL);
	val32 |= B_AX_PCIE_DIS_L2_CTRL_LDO_HCI;
	val32 &= ~B_AX_PCIE_DIS_WLSUS_AFT_PDN;
	MAC_REG_W32(R_AX_SYS_SDIO_CTRL, val32);

	return MACSUCCESS;
}

static u32 _patch_pcie_l2_hci_ldo(struct mac_ax_adapter *adapter)
{
	u32 ret;
	u16 bit_set;
	u8 val8;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
		return MACSUCCESS;

	bit_set = BIT0;

	ret = dbi_r8_pcie(adapter, CFG_RST_MSTATE, &val8);
	if (ret != MACSUCCESS)
		return ret;
	val8 |= bit_set;
	ret = dbi_w8_pcie(adapter, CFG_RST_MSTATE, val8);
	if (ret != MACSUCCESS)
		return ret;

	ret = dbi_r8_pcie(adapter, CFG_RST_MSTATE, &val8);
	if (ret != MACSUCCESS)
		return ret;
	val8 |= bit_set;
	ret = dbi_w8_pcie(adapter, CFG_RST_MSTATE, val8);
	if (ret != MACSUCCESS)
		return ret;

	return MACSUCCESS;
}

static u32 _patch_pcie_rxdma_prefth(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
		return MACSUCCESS;

	MAC_REG_W32(R_AX_PCIE_INIT_CFG1,
		    MAC_REG_R32(R_AX_PCIE_INIT_CFG1) | B_AX_DIS_RXDMA_PRE);

	return MACSUCCESS;
}

static u32 _patch_pcie_sw_ltr_setparm(struct mac_ax_adapter *adapter,
				      struct mac_ax_pcie_ltr_param *param)
{
	if (!(is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) &&
	      is_cv(adapter, CBV)))
		return MACSUCCESS;

	param->ltr_hw_ctrl = MAC_AX_PCIE_DISABLE;

	return MACSUCCESS;
}

static u32 _patch_pcie_sw_ltr(struct mac_ax_adapter *adapter,
			      enum mac_ax_pcie_ltr_sw_ctrl ctrl)
{
	u32 ret;

	if (!(is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) &&
	      is_cv(adapter, CBV)))
		return MACSUCCESS;

	ret = ltr_sw_trigger(adapter, ctrl);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pcie ltr sw trig %d %d\n", ctrl, ret);
		return ret;
	}

	return MACSUCCESS;
}

void ctrl_dma_all_pcie(struct mac_ax_adapter *adapter, enum mac_ax_func_sw en)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 c_id = adapter->hw_info->chip_id;

	if (en == MAC_AX_FUNC_EN) {
		if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
			MAC_REG_W32(R_AX_PCIE_DMA_STOP1,
				    MAC_REG_R32(R_AX_PCIE_DMA_STOP1) &
				    ~B_AX_STOP_PCIEIO);
		}
		MAC_REG_W32(init_cfg_reg[adapter->hw_info->chip_id],
			    MAC_REG_R32(init_cfg_reg[c_id]) |
			    txhci_en_bit[c_id] | rxhci_en_bit[c_id]);
	} else {
		if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
			MAC_REG_W32(R_AX_PCIE_DMA_STOP1,
				    MAC_REG_R32(R_AX_PCIE_DMA_STOP1) |
				    B_AX_STOP_PCIEIO);
		}
		MAC_REG_W32(init_cfg_reg[adapter->hw_info->chip_id],
			    MAC_REG_R32(init_cfg_reg[c_id]) &
			    ~(txhci_en_bit[c_id] | rxhci_en_bit[c_id]));
	}
}

u32 clr_idx_all_pcie(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 c_id = adapter->hw_info->chip_id;
	u32 val32;

	val32 = B_AX_CLR_ACH0_IDX | B_AX_CLR_ACH1_IDX | B_AX_CLR_ACH2_IDX |
		B_AX_CLR_ACH3_IDX | B_AX_CLR_CH8_IDX | B_AX_CLR_CH9_IDX |
		B_AX_CLR_CH12_IDX;

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
		val32 |= B_AX_CLR_ACH4_IDX | B_AX_CLR_ACH5_IDX |
			 B_AX_CLR_ACH6_IDX | B_AX_CLR_ACH7_IDX;

	MAC_REG_W32(R_AX_TXBD_RWPTR_CLR1, val32);

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		val32 = B_AX_CLR_CH10_IDX | B_AX_CLR_CH11_IDX;
		MAC_REG_W32(txbd_rwptr_clr2_reg[c_id], val32);
	}

	val32 = B_AX_CLR_RXQ_IDX | B_AX_CLR_RPQ_IDX;
	MAC_REG_W32(rxbd_rwptr_clr_reg[c_id], val32);

	PLTFM_MSG_ALWAYS("Clear all bd index done.\n");

	return MACSUCCESS;
}

static u32 clr_idx_ch_pcie(struct mac_ax_adapter *adapter,
			   struct mac_ax_txdma_ch_map *txch_map,
			   struct mac_ax_rxdma_ch_map *rxch_map)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 reg_clr_tx1 = R_AX_TXBD_RWPTR_CLR1;
	u32 reg_clr_tx2 = R_AX_TXBD_RWPTR_CLR2;
	u32 reg_clr_rx = R_AX_RXBD_RWPTR_CLR;
	u32 val32 = 0;

	if (txch_map->ch0 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_ACH0_IDX;

	if (txch_map->ch1 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_ACH1_IDX;

	if (txch_map->ch2 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_ACH2_IDX;

	if (txch_map->ch3 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_ACH3_IDX;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		if (txch_map->ch4 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_CLR_ACH4_IDX;

		if (txch_map->ch5 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_CLR_ACH5_IDX;

		if (txch_map->ch6 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_CLR_ACH6_IDX;

		if (txch_map->ch7 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_CLR_ACH7_IDX;
	}

	if (txch_map->ch8 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_CH8_IDX;

	if (txch_map->ch9 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_CH9_IDX;

	if (txch_map->ch12 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_CH12_IDX;

	MAC_REG_W32(reg_clr_tx1, val32);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		val32 = 0;
		if (txch_map->ch10 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_CLR_CH10_IDX;

		if (txch_map->ch11 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_CLR_CH11_IDX;

		MAC_REG_W32(reg_clr_tx2, val32);
	}

	val32 = 0;
	if (rxch_map->rxq == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_RXQ_IDX;
	if (rxch_map->rpq == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CLR_RPQ_IDX;

	MAC_REG_W32(reg_clr_rx, val32);

	return MACSUCCESS;
}

u32 ctrl_txdma_ch_pcie(struct mac_ax_adapter *adapter,
		       struct mac_ax_txdma_ch_map *ch_map)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 c_id = adapter->hw_info->chip_id;
	u32 val32;

	val32 = MAC_REG_R32(dma_stop1_reg[c_id]);
	if (ch_map->ch0 == MAC_AX_PCIE_ENABLE)
		val32 &= ~B_AX_STOP_ACH0;
	else if (ch_map->ch0 == MAC_AX_PCIE_DISABLE)
		val32 |= B_AX_STOP_ACH0;

	if (ch_map->ch1 == MAC_AX_PCIE_ENABLE)
		val32 &= ~B_AX_STOP_ACH1;
	else if (ch_map->ch1 == MAC_AX_PCIE_DISABLE)
		val32 |= B_AX_STOP_ACH1;

	if (ch_map->ch2 == MAC_AX_PCIE_ENABLE)
		val32 &= ~B_AX_STOP_ACH2;
	else if (ch_map->ch2 == MAC_AX_PCIE_DISABLE)
		val32 |= B_AX_STOP_ACH2;

	if (ch_map->ch3 == MAC_AX_PCIE_ENABLE)
		val32 &= ~B_AX_STOP_ACH3;
	else if (ch_map->ch3 == MAC_AX_PCIE_DISABLE)
		val32 |= B_AX_STOP_ACH3;
	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (ch_map->ch4 == MAC_AX_PCIE_ENABLE)
			val32 &= ~B_AX_STOP_ACH4;
		else if (ch_map->ch4 == MAC_AX_PCIE_DISABLE)
			val32 |= B_AX_STOP_ACH4;

		if (ch_map->ch5 == MAC_AX_PCIE_ENABLE)
			val32 &= ~B_AX_STOP_ACH5;
		else if (ch_map->ch5 == MAC_AX_PCIE_DISABLE)
			val32 |= B_AX_STOP_ACH5;

		if (ch_map->ch6 == MAC_AX_PCIE_ENABLE)
			val32 &= ~B_AX_STOP_ACH6;
		else if (ch_map->ch6 == MAC_AX_PCIE_DISABLE)
			val32 |= B_AX_STOP_ACH6;

		if (ch_map->ch7 == MAC_AX_PCIE_ENABLE)
			val32 &= ~B_AX_STOP_ACH7;
		else if (ch_map->ch7 == MAC_AX_PCIE_DISABLE)
			val32 |= B_AX_STOP_ACH7;
	}
	if (ch_map->ch8 == MAC_AX_PCIE_ENABLE)
		val32 &= ~B_AX_STOP_CH8;
	else if (ch_map->ch8 == MAC_AX_PCIE_DISABLE)
		val32 |= B_AX_STOP_CH8;

	if (ch_map->ch9 == MAC_AX_PCIE_ENABLE)
		val32 &= ~B_AX_STOP_CH9;
	else if (ch_map->ch9 == MAC_AX_PCIE_DISABLE)
		val32 |= B_AX_STOP_CH9;

	if (ch_map->ch12 == MAC_AX_PCIE_ENABLE)
		val32 &= ~B_AX_STOP_CH12;
	else if (ch_map->ch12 == MAC_AX_PCIE_DISABLE)
		val32 |= B_AX_STOP_CH12;
	MAC_REG_W32(dma_stop1_reg[c_id], val32);

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		val32 = MAC_REG_R32(dma_stop2_reg[c_id]);
		if (ch_map->ch10 == MAC_AX_PCIE_ENABLE)
			val32 &= ~B_AX_STOP_CH10;
		else if (ch_map->ch10 == MAC_AX_PCIE_DISABLE)
			val32 |= B_AX_STOP_CH10;

		if (ch_map->ch11 == MAC_AX_PCIE_ENABLE)
			val32 &= ~B_AX_STOP_CH11;
		else if (ch_map->ch11 == MAC_AX_PCIE_DISABLE)
			val32 |= B_AX_STOP_CH11;
		MAC_REG_W32(dma_stop2_reg[c_id], val32);
	}

	return MACSUCCESS;
}

u32 poll_txdma_ch_idle_pcie(struct mac_ax_adapter *adapter,
			    struct mac_ax_txdma_ch_map *ch_map)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 c_id = adapter->hw_info->chip_id;
	u32 val32;
	u32 cnt;

	val32 = 0;
	if (ch_map->ch0 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_ACH0_BUSY;

	if (ch_map->ch1 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_ACH1_BUSY;

	if (ch_map->ch2 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_ACH2_BUSY;

	if (ch_map->ch3 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_ACH3_BUSY;

	if (ch_map->ch8 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CH8_BUSY;

	if (ch_map->ch9 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CH9_BUSY;

	if (ch_map->ch12 == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_CH12_BUSY;
	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (ch_map->ch4 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_ACH4_BUSY;

		if (ch_map->ch5 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_ACH5_BUSY;

		if (ch_map->ch6 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_ACH6_BUSY;

		if (ch_map->ch7 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_ACH7_BUSY;
	}

	cnt = PCIE_POLL_DMACH_IDLE_CNT;
	while (cnt && (MAC_REG_R32(dma_busy1_reg[c_id]) & val32)) {
		cnt--;
		PLTFM_DELAY_US(PCIE_POLL_DMACH_IDLE_DLY_US);
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]PCIE dmach busy1 0x%X\n",
			      MAC_REG_R32(dma_busy1_reg[c_id]));
		return MACPOLLTO;
	}

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		val32 = 0;
		if (ch_map->ch10 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_CH10_BUSY;

		if (ch_map->ch11 == MAC_AX_PCIE_ENABLE)
			val32 |= B_AX_CH11_BUSY;

		cnt = PCIE_POLL_DMACH_IDLE_CNT;
		while (cnt && (MAC_REG_R32(dma_busy2_reg[c_id]) & val32)) {
			cnt--;
			PLTFM_DELAY_US(PCIE_POLL_DMACH_IDLE_DLY_US);
		}

		if (!cnt) {
			PLTFM_MSG_ERR("[ERR]PCIE dmach busy2 0x%X\n",
				      MAC_REG_R32(dma_busy2_reg[c_id]));
			return MACPOLLTO;
		}
	}

	return MACSUCCESS;
}

u32 poll_rxdma_ch_idle_pcie(struct mac_ax_adapter *adapter,
			    struct mac_ax_rxdma_ch_map *ch_map)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 c_id = adapter->hw_info->chip_id;
	u32 val32;
	u32 cnt;

	val32 = 0;
	if (ch_map->rxq == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_RXQ_BUSY;
	if (ch_map->rpq == MAC_AX_PCIE_ENABLE)
		val32 |= B_AX_RPQ_BUSY;

	cnt = PCIE_POLL_DMACH_IDLE_CNT;
	while (cnt && (MAC_REG_R32(dma_busy3_reg[c_id]) & val32)) {
		cnt--;
		PLTFM_DELAY_US(PCIE_POLL_DMACH_IDLE_DLY_US);
	}

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]PCIE dmach busy1 0x%X\n",
			      MAC_REG_R32(dma_busy3_reg[c_id]));
		return MACPOLLTO;
	}

	return MACSUCCESS;
}

u32 poll_dma_all_idle_pcie(struct mac_ax_adapter *adapter)
{
	struct mac_ax_txdma_ch_map txch_map;
	struct mac_ax_rxdma_ch_map rxch_map;
	u32 ret;

	txch_map.ch0 = MAC_AX_PCIE_ENABLE;
	txch_map.ch1 = MAC_AX_PCIE_ENABLE;
	txch_map.ch2 = MAC_AX_PCIE_ENABLE;
	txch_map.ch3 = MAC_AX_PCIE_ENABLE;
	txch_map.ch4 = MAC_AX_PCIE_ENABLE;
	txch_map.ch5 = MAC_AX_PCIE_ENABLE;
	txch_map.ch6 = MAC_AX_PCIE_ENABLE;
	txch_map.ch7 = MAC_AX_PCIE_ENABLE;
	txch_map.ch8 = MAC_AX_PCIE_ENABLE;
	txch_map.ch9 = MAC_AX_PCIE_ENABLE;
	txch_map.ch10 = MAC_AX_PCIE_ENABLE;
	txch_map.ch11 = MAC_AX_PCIE_ENABLE;
	txch_map.ch12 = MAC_AX_PCIE_ENABLE;
	ret = poll_txdma_ch_idle_pcie(adapter, &txch_map);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]PCIE poll txdma all ch idle\n");
		return ret;
	}

	rxch_map.rxq = MAC_AX_PCIE_ENABLE;
	rxch_map.rpq = MAC_AX_PCIE_ENABLE;
	ret = poll_rxdma_ch_idle_pcie(adapter, &rxch_map);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]PCIE poll rxdma all ch idle\n");
		return ret;
	}

	return ret;
}

u32 ctrl_txhci_pcie(struct mac_ax_adapter *adapter, enum mac_ax_func_sw en)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (en == MAC_AX_FUNC_EN) {
		MAC_REG_W32(R_AX_PCIE_INIT_CFG1,
			    MAC_REG_R32(R_AX_PCIE_INIT_CFG1) | B_AX_TXHCI_EN);
	} else if (en == MAC_AX_FUNC_DIS) {
		MAC_REG_W32(R_AX_PCIE_INIT_CFG1,
			    MAC_REG_R32(R_AX_PCIE_INIT_CFG1) & ~B_AX_TXHCI_EN);
	} else {
		PLTFM_MSG_ERR("[ERR]Wrong Input for Ctrl TX HCI\n");
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

u32 ctrl_rxhci_pcie(struct mac_ax_adapter *adapter, enum mac_ax_func_sw en)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (en == MAC_AX_FUNC_EN) {
		MAC_REG_W32(R_AX_PCIE_INIT_CFG1,
			    MAC_REG_R32(R_AX_PCIE_INIT_CFG1) | B_AX_RXHCI_EN);
	} else if (en == MAC_AX_FUNC_DIS) {
		MAC_REG_W32(R_AX_PCIE_INIT_CFG1,
			    MAC_REG_R32(R_AX_PCIE_INIT_CFG1) & ~B_AX_RXHCI_EN);
	} else {
		PLTFM_MSG_ERR("[ERR]Wrong Input for Ctrl RX HCI\n");
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

u32 ctrl_dma_io_pcie(struct mac_ax_adapter *adapter, enum mac_ax_func_sw en)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (en == MAC_AX_FUNC_EN) {
		MAC_REG_W32(R_AX_PCIE_DMA_STOP1,
			    MAC_REG_R32(R_AX_PCIE_DMA_STOP1)
			    & ~B_AX_STOP_PCIEIO);
	} else if (en == MAC_AX_FUNC_DIS) {
		MAC_REG_W32(R_AX_PCIE_DMA_STOP1,
			    MAC_REG_R32(R_AX_PCIE_DMA_STOP1)
			    | B_AX_STOP_PCIEIO);
	} else {
		PLTFM_MSG_ERR("[ERR]Wrong Input for DMA IO\n");
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

u32 pcie_pre_init(struct mac_ax_adapter *adapter, void *param)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_intf_info *intf_info = (struct mac_ax_intf_info *)param;
	struct mac_ax_txdma_ch_map ch_map;
	u32 ret = MACSUCCESS;

	ret = _patch_pcie_rxdma_prefth(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie dis rxdma prefth %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_l1off_pwroff(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie dis l1off pwroff %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_deglitch(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie deglitch %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_l2_rxen_lat(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie l2 rxen latency %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_aphy_pwrcut(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie aphy pwrcut %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_hci_ldo(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie hci ldo %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_dphy_delay(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie dphy delay %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_autok_x(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie autok_x %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_refclk_autok(adapter, intf_info);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]patch pcie refclk autok %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_power_wake(adapter, PC_POWER_UP);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]patch pcie power wake %d\n", ret);
		return ret;
	}

	ret = pcie_set_sic(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie sic %d\n", ret);
		return ret;
	}

	ret = pcie_set_lbc(adapter, intf_info->lbc_en, intf_info->lbc_tmr);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pcie set lbc %d\n", ret);
		return ret;
	}

	ret = pcie_set_io_rcy(adapter, intf_info->io_rcy_en,
			      intf_info->io_rcy_tmr);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pcie set io rcy %d\n", ret);
		return ret;
	}

	ret = pcie_set_dbg(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pcie set dbg %d\n", ret);
		return ret;
	}

	ret = pcie_set_keep_reg(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pcie set keep reg %d\n", ret);
		return ret;
	}

	if (intf_info->skip_all)
		return ret;

	if (!intf_info->txbd_buf || !intf_info->rxbd_buf ||
	    !intf_info->txch_map)
		return MACNPTR;

	MAC_REG_W32(dma_stop1_reg[adapter->hw_info->chip_id],
		    MAC_REG_R32(dma_stop1_reg[adapter->hw_info->chip_id]) |
		    B_AX_STOP_WPDMA);

	ctrl_dma_all_pcie(adapter, MAC_AX_FUNC_DIS);

	ret = clr_idx_all_pcie(adapter);
	if (ret)
		return ret;

	ret = poll_dma_all_idle_pcie(adapter);
	if (ret)
		return ret;

	ret = mode_op(adapter, intf_info);
	if (ret)
		return ret;

	ret = trx_init_bd(adapter, intf_info);
	if (ret)
		return ret;

	ret = rst_bdram_pcie(adapter, 0);
	if (ret)
		return ret;

	ch_map.ch0 = MAC_AX_PCIE_DISABLE;
	ch_map.ch1 = MAC_AX_PCIE_DISABLE;
	ch_map.ch2 = MAC_AX_PCIE_DISABLE;
	ch_map.ch3 = MAC_AX_PCIE_DISABLE;
	ch_map.ch4 = MAC_AX_PCIE_DISABLE;
	ch_map.ch5 = MAC_AX_PCIE_DISABLE;
	ch_map.ch6 = MAC_AX_PCIE_DISABLE;
	ch_map.ch7 = MAC_AX_PCIE_DISABLE;
	ch_map.ch8 = MAC_AX_PCIE_DISABLE;
	ch_map.ch9 = MAC_AX_PCIE_DISABLE;
	ch_map.ch10 = MAC_AX_PCIE_DISABLE;
	ch_map.ch11 = MAC_AX_PCIE_DISABLE;
	ch_map.ch12 = MAC_AX_PCIE_ENABLE;
	ret = ctrl_txdma_ch_pcie(adapter, &ch_map);
	if (ret)
		return ret;

	ctrl_dma_all_pcie(adapter, MAC_AX_FUNC_EN);

	return MACSUCCESS;
}

u32 pcie_init(struct mac_ax_adapter *adapter, void *param)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_intf_info *intf_info = (struct mac_ax_intf_info *)param;
	u8 c_id = adapter->hw_info->chip_id;
	u32 val32, ret = MACSUCCESS;
	struct mac_ax_pcie_ltr_param ltr_param = {
		1,
		0,
		MAC_AX_PCIE_DEFAULT,
		MAC_AX_PCIE_DEFAULT,
		MAC_AX_PCIE_LTR_SPC_DEF,
		MAC_AX_PCIE_LTR_IDLE_TIMER_DEF,
		{MAC_AX_PCIE_DEFAULT, 0},
		{MAC_AX_PCIE_DEFAULT, 0},
		{MAC_AX_PCIE_DEFAULT, 0},
		{MAC_AX_PCIE_DEFAULT, 0}
	};

	if (intf_info->skip_all)
		return MACSUCCESS;

	ret = _patch_pcie_sw_ltr_setparm(adapter, &ltr_param);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie sw ltr set param %d\n", ret);
		return ret;
	}

	ret = ltr_set_pcie(adapter, &ltr_param);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pcie ltr set fail %d\n", ret);
		return ret;
	}

	ret = _patch_pcie_sw_ltr(adapter, MAC_AX_PCIE_LTR_SW_ACT);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]patch pcie sw ltr act %d\n", ret);
		return ret;
	}

	if (!intf_info->txch_map) {
		PLTFM_MSG_ERR("[ERR] pcie init no txch map\n");
		return MACNPTR;
	}

	val32 = MAC_REG_R32(dma_stop1_reg[c_id]) &
		~(B_AX_STOP_WPDMA | B_AX_STOP_PCIEIO);
	MAC_REG_W32(dma_stop1_reg[c_id], val32);

	ret = ctrl_txdma_ch_pcie(adapter, intf_info->txch_map);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 pcie_deinit(struct mac_ax_adapter *adapter, void *param)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32, ret = MACSUCCESS;
	struct mac_ax_pcie_ltr_param ltr_param = {
		1,
		0,
		MAC_AX_PCIE_DISABLE,
		MAC_AX_PCIE_DISABLE,
		MAC_AX_PCIE_LTR_SPC_DEF,
		MAC_AX_PCIE_LTR_IDLE_TIMER_DEF,
		{MAC_AX_PCIE_DEFAULT, 0},
		{MAC_AX_PCIE_DEFAULT, 0},
		{MAC_AX_PCIE_DEFAULT, 0},
		{MAC_AX_PCIE_DEFAULT, 0}
	};

	ret = _patch_pcie_power_wake(adapter, PC_POWER_DOWN);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]patch pcie power wake %d\n", ret);
		return ret;
	}

	val32 = MAC_REG_R32(R_AX_IC_PWR_STATE);
	val32 = GET_FIELD(val32, B_AX_WLMAC_PWR_STE);
	if (val32 == MAC_AX_MAC_OFF) {
		PLTFM_MSG_WARN("PCIe deinit when MAC off\n");
		return MACSUCCESS;
	}

	ret = ltr_set_pcie(adapter, &ltr_param);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pcie ltr set fail %d\n", ret);
		return ret;
	}

	ctrl_dma_all_pcie(adapter, MAC_AX_FUNC_DIS);
	ret = clr_idx_all_pcie(adapter);
	if (ret)
		return ret;

	return ret;
}

u32 rst_bdram_pcie(struct mac_ax_adapter *adapter, u8 val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 cnt, val32;

	MAC_REG_W32(R_AX_PCIE_INIT_CFG1,
		    MAC_REG_R32(R_AX_PCIE_INIT_CFG1) | B_AX_RST_BDRAM);

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
		MAC_REG_W32(R_AX_PCIE_PS_CTRL, MAC_REG_R32(R_AX_PCIE_PS_CTRL) |
			    B_AX_PCIE_FORCE_L0);

	cnt = PCIE_POLL_BDRAM_RST_CNT;
	do {
		val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1);
		if (!(val32 & B_AX_RST_BDRAM))
			break;
		cnt--;
		PLTFM_DELAY_US(PCIE_POLL_BDRAM_RST_DLY_US);
	} while (cnt);

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
		MAC_REG_W32(R_AX_PCIE_PS_CTRL, MAC_REG_R32(R_AX_PCIE_PS_CTRL) &
			    ~B_AX_PCIE_FORCE_L0);

	if (!cnt) {
		PLTFM_MSG_ERR("[ERR]rst bdram timeout 0x%X\n", val32);
		return MACPOLLTO;
	}

	return MACSUCCESS;
}

u32 lv1rst_stop_dma_pcie(struct mac_ax_adapter *adapter, u8 val)
{
	u32 ret, reg32, dma_rst = 0;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	ctrl_dma_all_pcie(adapter, MAC_AX_FUNC_DIS);
	ret = poll_io_idle_pcie(adapter);
	if (ret != MACSUCCESS) {
		reg32 = MAC_REG_R32(R_AX_DBG_ERR_FLAG);
		if ((reg32 & B_AX_TX_STUCK) ||
		    (reg32 & B_AX_PCIE_TXBD_LEN0))
			dma_rst |= B_AX_HCI_TXDMA_EN;
		if (reg32 & B_AX_RX_STUCK)
			dma_rst |= B_AX_HCI_RXDMA_EN;
		reg32 = MAC_REG_R32(R_AX_HCI_FUNC_EN);
		MAC_REG_W32(R_AX_HCI_FUNC_EN, reg32 & ~dma_rst);
		MAC_REG_W32(R_AX_HCI_FUNC_EN, reg32 | dma_rst);
		ret = poll_io_idle_pcie(adapter);
	}

	return ret;
}

u32 lv1rst_start_dma_pcie(struct mac_ax_adapter *adapter, u8 val)
{
	u32 ret;

	ctrl_hci_dma_en_pcie(adapter, MAC_AX_FUNC_DIS);
	ctrl_hci_dma_en_pcie(adapter, MAC_AX_FUNC_EN);
	ret = clr_idx_all_pcie(adapter);
	if (ret)
		return ret;

	ret = rst_bdram_pcie(adapter, 0);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]rst bdram %d\n", ret);
		return ret;
	}

	ctrl_dma_all_pcie(adapter, MAC_AX_FUNC_EN);
	return ret;
}

u32 pcie_pwr_switch(void *vadapter, u8 pre_switch, u8 on)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;

	if (pre_switch == PWR_PRE_SWITCH)
		adapter->mac_pwr_info.pwr_seq_proc = 1;
	else if (pre_switch == PWR_POST_SWITCH)
		adapter->mac_pwr_info.pwr_seq_proc = 0;

	return MACSUCCESS;
}

u32 pcie_trx_mit(struct mac_ax_adapter *adapter,
		 struct mac_ax_pcie_trx_mitigation *mit_info)
{
	u8 tmr_unit = 0;
	u32 value32 = 0;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (mit_info->txch_map->ch0 == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_TXMIT_ACH0_SEL;
	else if (mit_info->txch_map->ch0 == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_TXMIT_ACH0_SEL;

	if (mit_info->txch_map->ch1 == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_TXMIT_ACH1_SEL;
	else if (mit_info->txch_map->ch1 == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_TXMIT_ACH1_SEL;

	if (mit_info->txch_map->ch2 == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_TXMIT_ACH2_SEL;
	else if (mit_info->txch_map->ch2 == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_TXMIT_ACH2_SEL;

	if (mit_info->txch_map->ch3 == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_TXMIT_ACH3_SEL;
	else if (mit_info->txch_map->ch3 == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_TXMIT_ACH3_SEL;

	if (mit_info->txch_map->ch8 == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_TXMIT_CH8_SEL;
	else if (mit_info->txch_map->ch8 == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_TXMIT_CH8_SEL;

	if (mit_info->txch_map->ch9 == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_TXMIT_CH9_SEL;
	else if (mit_info->txch_map->ch9 == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_TXMIT_CH9_SEL;

	if (mit_info->txch_map->ch12 == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_TXMIT_CH12_SEL;
	else if (mit_info->txch_map->ch12 == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_TXMIT_CH12_SEL;

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (mit_info->txch_map->ch4 == MAC_AX_PCIE_ENABLE)
			value32 |= B_AX_TXMIT_ACH4_SEL;
		else if (mit_info->txch_map->ch4 == MAC_AX_PCIE_DISABLE)
			value32 &= ~B_AX_TXMIT_ACH4_SEL;

		if (mit_info->txch_map->ch5 == MAC_AX_PCIE_ENABLE)
			value32 |= B_AX_TXMIT_ACH5_SEL;
		else if (mit_info->txch_map->ch5 == MAC_AX_PCIE_DISABLE)
			value32 &= ~B_AX_TXMIT_ACH5_SEL;

		if (mit_info->txch_map->ch6 == MAC_AX_PCIE_ENABLE)
			value32 |= B_AX_TXMIT_ACH6_SEL;
		else if (mit_info->txch_map->ch6 == MAC_AX_PCIE_DISABLE)
			value32 &= ~B_AX_TXMIT_ACH6_SEL;

		if (mit_info->txch_map->ch7 == MAC_AX_PCIE_ENABLE)
			value32 |= B_AX_TXMIT_ACH7_SEL;
		else if (mit_info->txch_map->ch7 == MAC_AX_PCIE_DISABLE)
			value32 &= ~B_AX_TXMIT_ACH7_SEL;

		if (mit_info->txch_map->ch10 == MAC_AX_PCIE_ENABLE)
			value32 |= B_AX_TXMIT_CH10_SEL;
		else if (mit_info->txch_map->ch10 == MAC_AX_PCIE_DISABLE)
			value32 &= ~B_AX_TXMIT_CH10_SEL;

		if (mit_info->txch_map->ch11 == MAC_AX_PCIE_ENABLE)
			value32 |= B_AX_TXMIT_CH11_SEL;
		else if (mit_info->txch_map->ch11 == MAC_AX_PCIE_DISABLE)
			value32 &= ~B_AX_TXMIT_CH11_SEL;
	}

	switch (mit_info->tx_timer_unit) {
	case MAC_AX_MIT_64US:
		tmr_unit = 0;
		break;
	case MAC_AX_MIT_128US:
		tmr_unit = 1;
		break;
	case MAC_AX_MIT_256US:
		tmr_unit = 2;
		break;
	case MAC_AX_MIT_512US:
		tmr_unit = 3;
		break;
	default:
		PLTFM_MSG_WARN("[WARN]Set TX MIT timer unit fail\n");
		break;
	}

	value32 = SET_CLR_WOR2(value32, tmr_unit,
			       B_AX_TXTIMER_UNIT_SH,
			       B_AX_TXTIMER_UNIT_MSK);

	value32 = SET_CLR_WOR2(value32, mit_info->tx_counter,
			       B_AX_TXCOUNTER_MATCH_SH,
			       B_AX_TXCOUNTER_MATCH_MSK);

	value32 = SET_CLR_WOR2(value32, mit_info->tx_timer,
			       B_AX_TXTIMER_MATCH_SH,
			       B_AX_TXTIMER_MATCH_MSK);

	MAC_REG_W32(R_AX_INT_MIT_TX, value32);

	value32 = 0;
	if (mit_info->rxch_map->rxq == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_RXMIT_RXP2_SEL;
	else if (mit_info->rxch_map->rxq == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_RXMIT_RXP2_SEL;

	if (mit_info->rxch_map->rpq == MAC_AX_PCIE_ENABLE)
		value32 |= B_AX_RXMIT_RXP1_SEL;
	else if (mit_info->rxch_map->rpq == MAC_AX_PCIE_DISABLE)
		value32 &= ~B_AX_RXMIT_RXP1_SEL;

	switch (mit_info->rx_timer_unit) {
	case MAC_AX_MIT_64US:
		tmr_unit = 0;
		break;
	case MAC_AX_MIT_128US:
		tmr_unit = 1;
		break;
	case MAC_AX_MIT_256US:
		tmr_unit = 2;
		break;
	case MAC_AX_MIT_512US:
		tmr_unit = 3;
		break;
	default:
		PLTFM_MSG_WARN("[WARN]Set RX MIT timer unit fail\n");
		break;
	}

	value32 = SET_CLR_WOR2(value32, tmr_unit,
			       B_AX_RXTIMER_UNIT_SH,
			       B_AX_RXTIMER_UNIT_MSK);

	value32 = SET_CLR_WOR2(value32, mit_info->rx_counter,
			       B_AX_RXCOUNTER_MATCH_SH,
			       B_AX_RXCOUNTER_MATCH_MSK);

	value32 = SET_CLR_WOR2(value32, mit_info->rx_timer,
			       B_AX_RXTIMER_MATCH_SH,
			       B_AX_RXTIMER_MATCH_MSK);

	MAC_REG_W32(R_AX_INT_MIT_RX, value32);

	return MACSUCCESS;
}

u32 set_pcie_wowlan(struct mac_ax_adapter *adapter, enum mac_ax_wow_ctrl w_c)
{
	u32 val32, ret;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (w_c == MAC_AX_WOW_ENTER) {
		MAC_REG_W32(R_AX_SYS_SDIO_CTRL,
			    MAC_REG_R32(R_AX_SYS_SDIO_CTRL) &
			    ~B_AX_PCIE_DIS_L2_CTRL_LDO_HCI);

		MAC_REG_W32(R_AX_RSV_CTRL, MAC_REG_R32(R_AX_RSV_CTRL) |
			    B_AX_WLOCK_1C_B6);
		MAC_REG_W32(R_AX_RSV_CTRL, MAC_REG_R32(R_AX_RSV_CTRL) |
			    B_AX_R_DIS_PRST);
		MAC_REG_W32(R_AX_RSV_CTRL, MAC_REG_R32(R_AX_RSV_CTRL) &
			    ~B_AX_WLOCK_1C_B6);

		val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1);
		val32 |= B_AX_PCIE_PERST_KEEP_REG | B_AX_PCIE_TRAIN_KEEP_REG;
		MAC_REG_W32(R_AX_PCIE_INIT_CFG1, val32);
	} else if (w_c == MAC_AX_WOW_LEAVE) {
		MAC_REG_W32(R_AX_SYS_SDIO_CTRL,
			    MAC_REG_R32(R_AX_SYS_SDIO_CTRL) |
			    B_AX_PCIE_DIS_L2_CTRL_LDO_HCI);

		MAC_REG_W32(R_AX_RSV_CTRL, MAC_REG_R32(R_AX_RSV_CTRL) |
			    B_AX_WLOCK_1C_B6);
		MAC_REG_W32(R_AX_RSV_CTRL, MAC_REG_R32(R_AX_RSV_CTRL) &
			    ~B_AX_R_DIS_PRST);
		MAC_REG_W32(R_AX_RSV_CTRL, MAC_REG_R32(R_AX_RSV_CTRL) &
			    ~B_AX_WLOCK_1C_B6);

		val32 = MAC_REG_R32(R_AX_PCIE_INIT_CFG1);
		val32 &= (~B_AX_PCIE_PERST_KEEP_REG &
			~B_AX_PCIE_TRAIN_KEEP_REG);
		MAC_REG_W32(R_AX_PCIE_INIT_CFG1, val32);

		ret = _patch_pcie_l2_hci_ldo(adapter);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]patch pcie l2 hci ldo %d\n", ret);
			return ret;
		}
	} else {
		PLTFM_MSG_ERR("[ERR] Invalid WoWLAN input.\n");
		return MACFUNCINPUT;
	}

	return MACSUCCESS;
}

u32 set_pcie_l2_leave(struct mac_ax_adapter *adapter, u8 set)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (set) {
		/* fix WoWLAN Power Consumption */
		MAC_REG_W32(R_AX_SYS_SDIO_CTRL,
			    MAC_REG_R32(R_AX_SYS_SDIO_CTRL) &
			    ~B_AX_PCIE_CALIB_EN_V1);
	}

	return MACSUCCESS;
}

u32 get_io_stat_pcie(struct mac_ax_adapter *adapter,
		     struct mac_ax_io_stat *out_st)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32, to_addr;

	val32 = MAC_REG_R32(R_AX_LBC_WATCHDOG);
	if (val32 & B_AX_LBC_FLAG) {
		adapter->sm.io_st = MAC_AX_IO_ST_HANG;
		to_addr = GET_FIELD(val32, B_AX_LBC_ADDR);
		PLTFM_MSG_ERR("[ERR]pcie io timeout addr 0x%X\n", to_addr);
		if (out_st) {
			out_st->to_flag = 1;
			out_st->io_st = adapter->sm.io_st;
			out_st->addr = to_addr;
		}
		MAC_REG_W32(R_AX_LBC_WATCHDOG, val32);
	} else if (out_st) {
		out_st->to_flag = 0;
		out_st->io_st = adapter->sm.io_st;
		out_st->addr = 0;
	}

	return MACSUCCESS;
}

u32 pcie_get_txagg_num(struct mac_ax_adapter *adapter, u8 band)
{
	return PCIE_DEFAULT_AGG_NUM;
}

u32 pcie_autok_counter_avg(struct mac_ax_adapter *adapter)
{
	u8 bdr_ori, val8, l1_flag = 0;
	u16 tar16, hw_tar16, tmp16;
	u32 ret = MACSUCCESS;
	enum mac_ax_pcie_phy phy_rate = MAC_AX_PCIE_PHY_GEN1;

#if MAC_AX_FEATURE_HV
	if (adapter->env == HV_AX_FPGA)
		return MACSUCCESS;
#endif
	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
		return MACSUCCESS;

	ret = dbi_r8_pcie(adapter, PCIE_PHY_RATE, &val8);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]dbi_r8_pcie 0x%x\n", PCIE_PHY_RATE);
		return ret;
	}

	if ((val8 & (BIT1 | BIT0)) == 0x1) {
		phy_rate = MAC_AX_PCIE_PHY_GEN1;
	} else if ((val8 & (BIT1 | BIT0)) == 0x2) {
		phy_rate = MAC_AX_PCIE_PHY_GEN2;
	} else {
		PLTFM_MSG_ERR("[ERR]PCIe PHY rate not support\n");
		return MACHWNOSUP;
	}

	ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &hw_tar16);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]mdio_r16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
		return ret;
	}
	PLTFM_MSG_TRACE("PCIe PHY %X: %X\n", RAC_CTRL_PPR_V1, hw_tar16);

	if (!(hw_tar16 & BAC_AUTOK_EN)) {
		PLTFM_MSG_ERR("[ERR]PCIe autok is not enabled\n: %X", hw_tar16);
		return MACPROCERR;
	}

	ret = mdio_r16_pcie(adapter, RAC_SET_PPR_V1, phy_rate, &tar16);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]mdio_r16_pcie 0x%X\n", RAC_SET_PPR_V1);
		return ret;
	}
	PLTFM_MSG_TRACE("PCIe PHY %X: %X\n", RAC_SET_PPR_V1, tar16);

	hw_tar16 = GET_FIELD(hw_tar16, BAC_AUTOK_HW_TAR);
	tar16 = GET_FIELD(tar16, BAC_AUTOK_TAR);

	if (tar16 > hw_tar16)
		tmp16 = tar16 - hw_tar16;
	else
		tmp16 = hw_tar16 - tar16;

	if (!(tmp16 < PCIE_AUTOK_MGN_2048)) {
		PLTFM_MSG_WARN("autok target is different from origin\n");
		return MACSUCCESS;
	}

	adapter->pcie_info.autok_total += hw_tar16;
	adapter->pcie_info.autok_2s_cnt++;

	if (adapter->pcie_info.autok_2s_cnt >= PCIE_AUTOK_UD_CNT) {
		/* Disable L1BD */
		ret = dbi_r8_pcie(adapter, PCIE_L1_CTRL, &bdr_ori);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]dbi_r8_pcie 0x%X\n", PCIE_L1_CTRL);
			return ret;
		}

		if (bdr_ori & PCIE_BIT_L1) {
			ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL,
					  bdr_ori & ~(PCIE_BIT_L1));
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]dbi_w8_pcie 0x%X\n", PCIE_L1_CTRL);
				return ret;
			}
			l1_flag = 1;
		}

		ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &hw_tar16);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]mdio_r16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
			goto end;
		}

		hw_tar16 &= ~BAC_AUTOK_EN;

		ret = mdio_w16_pcie(adapter, RAC_CTRL_PPR_V1, hw_tar16, phy_rate);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]mdio_w16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
			goto end;
		}

		ret = mdio_r16_pcie(adapter, RAC_SET_PPR_V1, phy_rate, &tar16);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]mdio_r16_pcie 0x%X\n", RAC_SET_PPR_V1);
			goto end;
		}

		tmp16 = adapter->pcie_info.autok_total / adapter->pcie_info.autok_2s_cnt;
		PLTFM_MSG_TRACE("Autok 30 times avg tar: %X\n", tmp16);

		tar16 = SET_CLR_WOR2(tar16, tmp16, BAC_AUTOK_TAR_SH,
				     BAC_AUTOK_TAR_MSK);

		ret = mdio_w16_pcie(adapter, RAC_SET_PPR_V1, tar16, phy_rate);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]mdio_w16_pcie 0x%X\n", RAC_SET_PPR_V1);
			goto end;
		}

		ret = mdio_r16_pcie(adapter, RAC_CTRL_PPR_V1, phy_rate, &hw_tar16);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]mdio_r16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
			goto end;
		}

		hw_tar16 |= BAC_AUTOK_EN;

		ret = mdio_w16_pcie(adapter, RAC_CTRL_PPR_V1, hw_tar16, phy_rate);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]mdio_w16_pcie 0x%X\n", RAC_CTRL_PPR_V1);
			goto end;
		}

end:
		if (l1_flag == 1) {
			ret = dbi_w8_pcie(adapter, PCIE_L1_CTRL, bdr_ori);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]dbi_w8_pcie 0x%X\n", PCIE_L1_CTRL);
				return ret;
			}
		}

		adapter->pcie_info.autok_total = 0;
		adapter->pcie_info.autok_2s_cnt = 0;
	}

	return ret;
}

u32 dbcc_hci_ctrl_pcie(struct mac_ax_adapter *adapter,
		       struct mac_ax_dbcc_hci_ctrl *info)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_dbcc_pcie_ctrl *ctrl;
	struct mac_ax_txdma_ch_map pause_txmap;
	struct mac_ax_rxdma_ch_map clr_rxch_map;
	enum mac_ax_band band;
	u32 ret, reg, val32;
	u8 ch, pause;

	if (!info)
		return MACNPTR;

	band = info->band;
	pause = info->pause;
	ctrl = &info->u.pcie_ctrl;

	if (pause) {
		pause_txmap.ch0 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch1 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch2 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch3 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch4 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch5 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch6 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch7 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch8 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch9 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch10 = MAC_AX_PCIE_DISABLE;
		pause_txmap.ch11 = MAC_AX_PCIE_DISABLE;
	} else {
		pause_txmap.ch0 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch1 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch2 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch3 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch4 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch5 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch6 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch7 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch8 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch9 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch10 = MAC_AX_PCIE_ENABLE;
		pause_txmap.ch11 = MAC_AX_PCIE_ENABLE;
	}
	pause_txmap.ch12 = MAC_AX_PCIE_IGNORE;

	ret = ops->ctrl_txdma_ch(adapter, &pause_txmap);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("dbcc%d ctrl%d txdma ch pcie %d\n", band, pause, ret);
		return ret;
	}

	if (!pause)
		return MACSUCCESS;

	pause_txmap.ch0 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch1 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch2 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch3 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch4 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch5 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch6 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch7 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch8 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch9 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch10 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch11 = MAC_AX_PCIE_ENABLE;
	pause_txmap.ch12 = MAC_AX_PCIE_IGNORE;
	ret = ops->poll_txdma_ch_idle(adapter, &pause_txmap);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("dbcc%d poll txdma ch pcie %d\n", band, ret);
		return ret;
	}

	for (ch = MAC_AX_DMA_ACH0; ch < MAC_AX_DMA_CH_NUM; ch++) {
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B) &&
		    ((ch >= MAC_AX_DMA_ACH4 && ch <= MAC_AX_DMA_ACH7) ||
		     (ch >= MAC_AX_DMA_B1MG && ch <= MAC_AX_DMA_B1HI))) {
			ctrl->out_host_idx_l[ch] = BD_IDX_INVALID;
			ctrl->out_hw_idx_l[ch] = BD_IDX_INVALID;
			continue;
		}

		ret = get_txbd_idx_reg(adapter, ch, &reg);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("get ch%d idx reg pcie %d\n", ch, ret);
			return ret;
		}

		val32 = MAC_REG_R32(reg);
		ctrl->out_host_idx_l[ch] = GET_FIELD(val32, B_AX_ACH0_HOST_IDX);
		ctrl->out_hw_idx_l[ch] = GET_FIELD(val32, B_AX_ACH0_HW_IDX);
	}

	clr_rxch_map.rxq = MAC_AX_PCIE_IGNORE;
	clr_rxch_map.rpq = MAC_AX_PCIE_IGNORE;
	ret = clr_idx_ch_pcie(adapter, &ctrl->clr_txch_map, &clr_rxch_map);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("dbcc%d clear ch idx pcie %d\n", band, ret);
		return ret;
	}

	return MACSUCCESS;
}
#endif /* #if MAC_AX_PCIE_SUPPORT */

