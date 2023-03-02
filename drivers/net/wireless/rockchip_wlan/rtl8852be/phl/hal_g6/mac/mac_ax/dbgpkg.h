/** @file */
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

#ifndef _MAC_AX_DBGPKG_H_
#define _MAC_AX_DBGPKG_H_

#include "../mac_def.h"
#include "../mac_ax.h"
#include "fwcmd.h"
#include "trx_desc.h"
#include "trxcfg.h"
#include "dle.h"

#define FW_RSVD_PLE_SIZE 0x800
#define RSVD_PLE_OFST_8852A 0x6f800
#define RSVD_PLE_OFST_8852B 0x2f800
#define FW_RSVD_PLE_DBG_SIZE 0x100
#define RSVD_PLE_OFST_DBG_START 0x400
#define SHARE_BUFFER_SIZE_8852A 0x70000
#define SHARE_BUFFER_SIZE_8852B 0x30000
#define RSVD_PLE_OFST_8852C 0x6f800
#define RSVD_PLE_OFST_8192XB 0x6f800
#define RSVD_PLE_OFST_1115E 0x87800
#define STA_SCHED_MEM_SIZE 0x1200
#define RXPLD_FLTR_CAM_MEM_SIZE 0x200
#define SECURITY_CAM_MEM_SIZE 0x800
#define WOW_CAM_MEM_SIZE 0x240
#define ADDR_CAM_MEM_SIZE 0x4000
#define TXD_FIFO_SIZE 0x200
#define DBG_PORT_DUMP_DLY_US 10
#define FW_BACKTRACE_MAX_SIZE 512 // 8 * 64(entry)
#define FW_BACKTRACE_KEY 0xBACEBACE
#define FW_BACKTRACE_SIZE_OFST 4
#define FW_BACKTRACE_KEY_OFST 8

#define B_AX_AXIDMA_INT_SEL_SH 22
#define B_AX_AXIDMA_INT_SEL_MSK 0x7

#define TMAC_TX_CNT_NUM 11
#define RMAC_RX_CNT_NUM 37
#define TRX_CNT_REPT_CNT 5
#define TRX_CNT_REPT_DLY_US 10
#define TRX_CNT_READ_DLY_US 1
#define PTCL_ST_READ_DLY_US 1
#define PTCL_SEL_FSM_0 0x0
#define PTCL_SEL_FSM_1 0x1
#define PTCL_SEL_PHY_DBG 0x10
#define SCH_SEL_PREBKF_DBG_1 0x03
#define SCH_SEL_TX_NAV_ABORT_DBG 0x07
#define PTCL_DBG_DMP_CNT 5
#define SCH_DBG_DMP_CNT 5
#define TX_FLOW_DMP_NUM 10
#define TXFLOW_TRX_CNT_REPT_CNT 1

#define MAC_DBG_DMP_CNT 15
#define MAC_DBG_DUMP_DLY_US 10

#define DBG_SEL_FW_PROG_CNTR 0xF200F2
#define FW_PROG_CNTR_DMP_CNT 15
#define FW_PROG_CNTR_DMP_DLY_US 10

/* Wait for BCN parser idle shall consider RX beacon max time */
#define BCN_PSR_WAIT_CNT 900
#define BCN_PSR_WAIT_US 10

/* REG dump*/
#define MAC_PAGE_SRT		0
#define	MAC_PAGE_AON_END	0x4
#define	MAC_PAGE_TOP_END	0xF
#define	MAC_PAGE_HCI_SRT	0x10
#define	MAC_PAGE_HCI_END	0x1F
#define MAC_PAGE_DMAC_SRT	0x80
#define	MAC_PAGE_DMAC_END	0x9F
#define	MAC_PAGE_CMAC0_SRT	0xC0
#define	MAC_PAGE_CMAC0_END	0xDF
#define	MAC_PAGE_CMAC1_SRT	0xE0
#define MAC_PAGE_END		0xFF
#define BB_PAGE_SRT		0x100
#define BB_PAGE_END		0x17F
#define IQK_PAGE_SRT		0x180
#define IQK_PAGE_END		0x1BF
#define RFC_PAGE_SRT		0x1C0
#define RFC_PAGE_END		0x1FF

/* STA scheduler */
#define SS_MACID_SH		8
#define SS_TX_LEN_MSK		0x1FFFFF
#define SS_CTRL1_R_TX_LEN	5
#define SS_CTRL1_R_NEXT_LINK	20
#define SS_WMM_NUM_8852A   4
#define SS_WMM_NUM_8852B   2
#define SS_WMM_NUM_8852C   4
#define SS_WMM_NUM_8192XB   4
#define SS_UL_SUPPORT_8852A    1
#define SS_UL_SUPPORT_8852B    0
#define SS_UL_SUPPORT_8852C    1
#define SS_UL_SUPPORT_8192XB    1
#define SS_FW_SUPPORT_8852A    1
#define SS_FW_SUPPORT_8852B    0
#define SS_FW_SUPPORT_8852C    1
#define SS_FW_SUPPORT_8192XB    1
#define SS_POLL_UNEXPECTED	0xFFFFFFFF

/* MAC debug port */
#define CMAC_DMA_DBG_SEL_C0 0xA0
#define TMAC_DBG_SEL_C0 0xA5
#define RMAC_DBG_SEL_C0 0xA6
#define TRXPTCL_DBG_SEL_C0 0xA7
#define CMAC_DMA_DBG_SEL_C1 0xB0
#define TMAC_DBG_SEL_C1 0xB5
#define RMAC_DBG_SEL_C1 0xB6
#define TRXPTCL_DBG_SEL_C1 0xB7
#define PCIE_TXDMA_DBG_SEL 0x30
#define PCIE_RXDMA_DBG_SEL 0x31
#define PCIE_CVT_DBG_SEL 0x32
#define PCIE_EMAC04_DBG_SEL 0x33
#define PCIE_EMAC5_DBG_SEL 0x34
#define PCIE_EMAC6_DBG_SEL 0x35
#define PCIE_EMAC7_DBG_SEL 0x36
#define PCIE_PNP_IO_DBG_SEL 0x37
#define PCIE_EMAC814_DBG_SEL 0x38
#define PCIE_EMAC15_DBG_SEL 0x39
#define PCIE_EMAC16_DBG_SEL 0x3A
#define PCIE_EMAC17_DBG_SEL 0x3B
#define PCIE_EMAC18_DBG_SEL 0x3C
#define PCIE_IO_DBG_SEL 0x37
#define PCIE_MISC_DBG_SEL 0x38
#define PCIE_MISC2_DBG_SEL 0x00
#define USB2_PHY_DBG_SEL 0x50
#define USB2_SIE_DBG_SEL 0x51
#define USB2_UTMI_DBG_SEL 0x52
#define USB2_SIE_MMU_DBG_SEL 0x53
#define USB2_SIE_PCE_DBG_SEL 0x54
#define USB2_UTMI_IF_DBG_SEL 0x55
#define USB_WLTX_DBG_SEL 0x56
#define USB_WLRX_DBG_SEL 0x57
#define USB3_DBG_SEL 0x58
#define USB_SETUP_DBG_SEL 0x59
#define USB_WLTXDMA_DBG_SEL 0x5A
#define USB_WLRXDMA_DBG_SEL 0x5B
#define USB_AINST_DBG_SEL 0x5C
#define USB_MISC_DBG_SEL 0x5D
#define USB_BTTX_DBG_SEL 0x5E
#define USB2_BT_DBG_SEL 0x5F
#define DISPATCHER_DBG_SEL 0x80
#define MAC_DBG_SEL 1
#define RMAC_CMAC_DBG_SEL 1

/* TRXPTCL dbg port sel */
#define TRXPTRL_DBG_SEL_TMAC 0
#define TRXPTRL_DBG_SEL_RMAC 1

#define MAC_AX_RX_CNT_NUM 48
#define MAC_AX_RX_CNT_IDX_MAX MAC_AX_RX_CNT_NUM
#define MAC_AX_RX_CNT_TYPE_NUM 5

/* the order is CCK, OFDM, HT, VHTSU, VHTMU, HESU, HEMU, HETB */
#define MAC_AX_RXCRC_OK_IDX \
	{3, 0, 6, 10, 14, 18, 22, 26}
#define MAC_AX_RXCRC_FAIL_IDX \
	{4, 1, 7, 11, 15, 19, 23, 27}
#define MAC_AX_RXPPDU_IDX \
	{MAC_AX_RX_CNT_IDX_MAX, MAC_AX_RX_CNT_IDX_MAX, 8, 16, 20, 24, 28}
#define MAC_AX_RXFA_IDX \
	{5, 2, 9, 17, 21, 25, 29}

#define Read_DBG_FS_REG() GET_FIELD(MAC_REG_R32(R_AX_UDM0), B_AX_UDM0_FS_CODE)

/**
 * @enum mac_ax_sram_dbg_sel
 *
 * @brief mac_ax_sram_dbg_sel
 *
 * @var mac_ax_sram_dbg_sel::CPU_LOCAL_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::AXIDMA_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::STA_SCHED_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::RXPLD_FLTR_CAM_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::SEC_CAM_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::WOW_CAM_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::CMAC_TBL_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::ADDR_CAM_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::BSSID_CAM_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::BA_CAM_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::BCN_IE_CAM0_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::SHARED_BUF_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::DMAC_TBL_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::SHCUT_MACHDR_SEL
 * Please Place Description here.
 * @var mac_ax_sram_dbg_sel::BCN_IE_CAM1_SEL
 * Please Place Description here.
 */
enum mac_ax_sram_dbg_sel {
	CPU_LOCAL_SEL,
	AXIDMA_SEL,
	STA_SCHED_SEL,
	RXPLD_FLTR_CAM_SEL,
	SEC_CAM_SEL,
	WOW_CAM_SEL,
	CMAC_TBL_SEL,
	ADDR_CAM_SEL,
	BSSID_CAM_SEL,
	BA_CAM_SEL,
	BCN_IE_CAM0_SEL,
	SHARED_BUF_SEL,
	DMAC_TBL_SEL,
	SHCUT_MACHDR_SEL,
	BCN_IE_CAM1_SEL,
};

/**
 * @enum mac_ax_dle_dfi_sel
 *
 * @brief mac_ax_dle_dfi_sel
 *
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_WDE_BUFMGN_FREEPG
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_WDE_BUFMGN_QUOTA
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_WDE_BUFMGN_PAGELLT
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_WDE_BUFMGN_PKTINFO
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_WDE_QUEMGN_PREPKT
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_WDE_QUEMGN_NXTPKT
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_WDE_QUEMGN_QLNKTBL
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_WDE_QUEMGN_QEMPTY
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_PLE_BUFMGN_FREEPG
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_PLE_BUFMGN_QUOTA
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_PLE_BUFMGN_PAGELLT
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_PLE_BUFMGN_PKTINFO
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_PLE_QUEMGN_PREPKT
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_PLE_QUEMGN_NXTPKT
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_PLE_QUEMGN_QLNKTBL
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DLE_DFI_SEL_PLE_QUEMGN_QEMPTY
 * Please Place Description here.
 */
enum mac_ax_dle_dfi_sel {
	MAC_AX_DLE_DFI_SEL_WDE_BUFMGN_FREEPG = 0,
	MAC_AX_DLE_DFI_SEL_WDE_BUFMGN_QUOTA,
	MAC_AX_DLE_DFI_SEL_WDE_BUFMGN_PAGELLT,
	MAC_AX_DLE_DFI_SEL_WDE_BUFMGN_PKTINFO,
	MAC_AX_DLE_DFI_SEL_WDE_QUEMGN_PREPKT,
	MAC_AX_DLE_DFI_SEL_WDE_QUEMGN_NXTPKT,
	MAC_AX_DLE_DFI_SEL_WDE_QUEMGN_QLNKTBL,
	MAC_AX_DLE_DFI_SEL_WDE_QUEMGN_QEMPTY,
	MAC_AX_DLE_DFI_SEL_PLE_BUFMGN_FREEPG,
	MAC_AX_DLE_DFI_SEL_PLE_BUFMGN_QUOTA,
	MAC_AX_DLE_DFI_SEL_PLE_BUFMGN_PAGELLT,
	MAC_AX_DLE_DFI_SEL_PLE_BUFMGN_PKTINFO,
	MAC_AX_DLE_DFI_SEL_PLE_QUEMGN_PREPKT,
	MAC_AX_DLE_DFI_SEL_PLE_QUEMGN_NXTPKT,
	MAC_AX_DLE_DFI_SEL_PLE_QUEMGN_QLNKTBL,
	MAC_AX_DLE_DFI_SEL_PLE_QUEMGN_QEMPTY,

	/* keep last */
	MAC_AX_DLE_DFI_SEL_LAST,
	MAC_AX_DLE_DFI_SEL_MAX = MAC_AX_DLE_DFI_SEL_LAST,
	MAC_AX_DLE_DFI_SEL_INVALID = MAC_AX_DLE_DFI_SEL_LAST,
};

/**
 * @struct mac_ax_dle_dfi_info
 * @brief mac_ax_dle_dfi_info
 *
 * @var mac_ax_dle_dfi_info::srt
 * Please Place Description here.
 * @var mac_ax_dle_dfi_info::end
 * Please Place Description here.
 * @var mac_ax_dle_dfi_info::inc_num
 * Please Place Description here.
 */
struct mac_ax_dle_dfi_info {
	u32 srt;
	u32 end;
	u32 inc_num;
};

/**
 * @enum mac_ax_dbg_port_sel
 *
 * @brief mac_ax_dbg_port_sel
 *
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PTCL_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_SCH_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TMAC_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_RMAC_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_RMACST_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_RMAC_PLCP_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TRXPTCL_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TX_INFOL_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TX_INFOH_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXTF_INFOL_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXTF_INFOH_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_CMAC_DMA0_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_CMAC_DMA1_C0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PTCL_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_SCH_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TMAC_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_RMAC_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_RMACST_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_RMAC_PLCP_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TRXPTCL_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TX_INFOL_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TX_INFOH_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXTF_INFOL_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXTF_INFOH_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_CMAC_DMA0_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_CMAC_DMA1_C1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PKTINFO
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXPKT_CTRL0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXPKT_CTRL1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXPKT_CTRL2
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXPKT_CTRL3
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_TXPKT_CTRL4
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_TXDMA
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_RXDMA
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_CVT
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC04
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC5
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC6
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC7
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_PNP_IO
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC814
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC15
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC16
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC17
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_PCIE_EMAC18
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB2_PHY
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB2_SIE
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB2_UTMI
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB2_SIE_MMU
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB2_SIE_PCE
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB2_UTMI_IF
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB_WLTX
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB_WLRX
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB3
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB_SETUP
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB_WLTX_DMA
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB_WLRX_DMA
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB_AINST
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB_MISC
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB_BTTX
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_USB2_BT
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX2
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX3
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX4
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX5
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX6
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX7
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX8
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX9
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TXA
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TXB
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TXC
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_TXD
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX3
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX4
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX5
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX6
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX7
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX8
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX9
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_RX0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_RX1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_RX2
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_HDT_RX3
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_RX_P0
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_CDT_RX_P1
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_STF_CTRL
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_ADDR_CTRL
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_WDE_INTF
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_PLE_INTF
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_DSPT_FLOW_CTRL
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_AXI_TXDMA_CTRL
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_AXI_RXDMA_CTRL
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_AXI_MST_WLAN
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_AXI_INT_WLAN
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_AXI_PAGE_FLOW_CTRL
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_LAST
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_MAX
 * Please Place Description here.
 * @var mac_ax_dbg_port_sel::MAC_AX_DBG_PORT_SEL_INVALID
 * Please Place Description here.
 */
enum mac_ax_dbg_port_sel {
	/* CMAC 0 related */
	MAC_AX_DBG_PORT_SEL_PTCL_C0 = 0,
	MAC_AX_DBG_PORT_SEL_SCH_C0,
	MAC_AX_DBG_PORT_SEL_TMAC_C0,
	MAC_AX_DBG_PORT_SEL_RMAC_C0,
	MAC_AX_DBG_PORT_SEL_RMACST_C0,
	MAC_AX_DBG_PORT_SEL_RMAC_PLCP_C0,
	MAC_AX_DBG_PORT_SEL_TRXPTCL_C0,
	MAC_AX_DBG_PORT_SEL_TX_INFOL_C0,
	MAC_AX_DBG_PORT_SEL_TX_INFOH_C0,
	MAC_AX_DBG_PORT_SEL_TXTF_INFOL_C0,
	MAC_AX_DBG_PORT_SEL_TXTF_INFOH_C0,
	MAC_AX_DBG_PORT_SEL_CMAC_DMA0_C0,
	MAC_AX_DBG_PORT_SEL_CMAC_DMA1_C0,
	/* CMAC 1 related */
	MAC_AX_DBG_PORT_SEL_PTCL_C1,
	MAC_AX_DBG_PORT_SEL_SCH_C1,
	MAC_AX_DBG_PORT_SEL_TMAC_C1,
	MAC_AX_DBG_PORT_SEL_RMAC_C1,
	MAC_AX_DBG_PORT_SEL_RMACST_C1,
	MAC_AX_DBG_PORT_SEL_RMAC_PLCP_C1,
	MAC_AX_DBG_PORT_SEL_TRXPTCL_C1,
	MAC_AX_DBG_PORT_SEL_TX_INFOL_C1,
	MAC_AX_DBG_PORT_SEL_TX_INFOH_C1,
	MAC_AX_DBG_PORT_SEL_TXTF_INFOL_C1,
	MAC_AX_DBG_PORT_SEL_TXTF_INFOH_C1,
	MAC_AX_DBG_PORT_SEL_CMAC_DMA0_C1,
	MAC_AX_DBG_PORT_SEL_CMAC_DMA1_C1,
	/* DLE related */
	MAC_AX_DBG_PORT_SEL_PKTINFO,
	MAC_AX_DBG_PORT_SEL_WDRLS,
	/* TXPKT_CTRL related */
	MAC_AX_DBG_PORT_SEL_TXPKT_CTRL0,
	MAC_AX_DBG_PORT_SEL_TXPKT_CTRL1,
	MAC_AX_DBG_PORT_SEL_TXPKT_CTRL2,
	MAC_AX_DBG_PORT_SEL_TXPKT_CTRL3,
	MAC_AX_DBG_PORT_SEL_TXPKT_CTRL4,
	/* PCIE related */
	MAC_AX_DBG_PORT_SEL_PCIE_TXDMA,
	MAC_AX_DBG_PORT_SEL_PCIE_RXDMA,
	MAC_AX_DBG_PORT_SEL_PCIE_CVT,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC04,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC5,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC6,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC7,
	MAC_AX_DBG_PORT_SEL_PCIE_PNP_IO,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC814,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC15,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC16,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC17,
	MAC_AX_DBG_PORT_SEL_PCIE_EMAC18,
	/* USB related */
	MAC_AX_DBG_PORT_SEL_USB2_PHY,
	MAC_AX_DBG_PORT_SEL_USB2_SIE,
	MAC_AX_DBG_PORT_SEL_USB2_UTMI,
	MAC_AX_DBG_PORT_SEL_USB2_SIE_MMU,
	MAC_AX_DBG_PORT_SEL_USB2_SIE_PCE,
	MAC_AX_DBG_PORT_SEL_USB2_UTMI_IF,
	MAC_AX_DBG_PORT_SEL_USB_WLTX,
	MAC_AX_DBG_PORT_SEL_USB_WLRX,
	MAC_AX_DBG_PORT_SEL_USB3,
	MAC_AX_DBG_PORT_SEL_USB_SETUP,
	MAC_AX_DBG_PORT_SEL_USB_WLTX_DMA,
	MAC_AX_DBG_PORT_SEL_USB_WLRX_DMA,
	MAC_AX_DBG_PORT_SEL_USB_AINST,
	MAC_AX_DBG_PORT_SEL_USB_MISC,
	MAC_AX_DBG_PORT_SEL_USB_BTTX,
	MAC_AX_DBG_PORT_SEL_USB2_BT,
	/* DISPATCHER related */
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX0,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX1,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX2,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX3,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX4,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX5,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX6,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX7,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX8,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TX9,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TXA,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TXB,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TXC,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_TXD,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX0,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX1,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX3,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX4,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX5,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX6,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX7,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX8,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_TX9,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_RX0,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_RX1,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_RX2,
	MAC_AX_DBG_PORT_SEL_DSPT_HDT_RX3,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_RX_P0,
	MAC_AX_DBG_PORT_SEL_DSPT_CDT_RX_P1,
	MAC_AX_DBG_PORT_SEL_DSPT_STF_CTRL,
	MAC_AX_DBG_PORT_SEL_DSPT_ADDR_CTRL,
	MAC_AX_DBG_PORT_SEL_DSPT_WDE_INTF,
	MAC_AX_DBG_PORT_SEL_DSPT_PLE_INTF,
	MAC_AX_DBG_PORT_SEL_DSPT_FLOW_CTRL,
	/*AXIDMAC related*/
	MAC_AX_DBG_PORT_SEL_AXI_TXDMA_CTRL,
	MAC_AX_DBG_PORT_SEL_AXI_RXDMA_CTRL,
	MAC_AX_DBG_PORT_SEL_AXI_MST_WLAN,
	MAC_AX_DBG_PORT_SEL_AXI_INT_WLAN,
	MAC_AX_DBG_PORT_SEL_AXI_PAGE_FLOW_CTRL,

	/* keep last */
	MAC_AX_DBG_PORT_SEL_LAST,
	MAC_AX_DBG_PORT_SEL_MAX = MAC_AX_DBG_PORT_SEL_LAST,
	MAC_AX_DBG_PORT_SEL_INVALID = MAC_AX_DBG_PORT_SEL_LAST,
};

struct ss_link_info {
	u8 wmm;
	u8 ac;
	u8 ul;
};

/**
 * @struct mac_ax_dbg_port_info
 * @brief mac_ax_dbg_port_info
 *
 * @var mac_ax_dbg_port_info::sel_addr
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::sel_byte
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::sel_sh
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::sel_msk
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::srt
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::end
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::inc_num
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::rd_addr
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::rd_byte
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::rd_sh
 * Please Place Description here.
 * @var mac_ax_dbg_port_info::rd_msk
 * Please Place Description here.
 */
struct mac_ax_dbg_port_info {
	u32 sel_addr;
	u8 sel_byte;
	u32 sel_sh;
	u32 sel_msk;
	u32 srt;
	u32 end;
	u32 inc_num;
	u32 rd_addr;
	u8 rd_byte;
	u32 rd_sh;
	u32 rd_msk;
};

/**
 * @struct iecam_cfg_info
 * @brief iecam_cfg_info
 *
 * @var iecam_cfg_info::camctrl_bkp
 * Please Place Description here.
 * @var iecam_cfg_info::ioctrl_bkp
 * Please Place Description here.
 * @var iecam_cfg_info::rbp_bkp
 * Please Place Description here.
 */
struct iecam_cfg_info {
	u16 camctrl_bkp;
	u16 ioctrl_bkp;
	u32 rbp_bkp;
};

struct fw_backtrace_info {
	u32 ra;
	u32 sp;
};

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_fwcmd_lb
 *
 * @param *adapter
 * @param len
 * @param burst
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_fwcmd_lb(struct mac_ax_adapter *adapter, u32 len, u8 burst);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief c2h_sys_cmd_path
 *
 * @param *adapter
 * @param *buf
 * @param len
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 c2h_sys_cmd_path(struct mac_ax_adapter *adapter, u8 *buf, u32 len,
		     struct rtw_c2h_info *info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief c2h_sys_plat_autotest
 *
 * @param *adapter
 * @param *buf
 * @param len
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 c2h_sys_plat_autotest(struct mac_ax_adapter *adapter,  u8 *buf, u32 len,
			  struct rtw_c2h_info *info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief iecam_aces_cfg
 *
 * @param *adapter
 * @param band
 * @param en
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 iecam_aces_cfg(struct mac_ax_adapter *adapter, u8 band, u8 en,
		   struct iecam_cfg_info *info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_mem_dump
 *
 * @param *adapter
 * @param sel
 * @param strt_addr
 * @param *buf
 * @param len
 * @param dbg_path
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_mem_dump(struct mac_ax_adapter *adapter, enum mac_ax_mem_sel sel,
		 u32 strt_addr, u8 *buf, u32 len, u32 dbg_path);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_get_mem_size
 *
 * @param *adapter
 * @param sel
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_get_mem_size(struct mac_ax_adapter *adapter, enum mac_ax_mem_sel sel);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_reg_dump
 *
 * @param *adapter
 * @param sel
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_reg_dump(struct mac_ax_adapter *adapter, enum mac_ax_reg_sel sel);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief dle_dfi_sel
 *
 * @param *adapter
 * @param **info
 * @param *ctrl
 * @param sel
 * @return Please Place Description here.
 * @retval u32
 */
u32 dle_dfi_sel(struct mac_ax_adapter *adapter,
		struct mac_ax_dle_dfi_info **info,
		u32 *target, u32 sel);
/**
 * @}
 * @}
 */

/**
 * @brief is_dbg_port_not_valid
 *
 * @param *adapter
 * @param dbg_sel
 * @return Please Place Description here.
 * @retval u8
 */
u8 is_dbg_port_not_valid(struct mac_ax_adapter *adapter, u32 dbg_sel);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_dbg_status_dump
 *
 * @param *adapter
 * @param *val
 * @param *en
 * @return Please Place Description here.
 * @retval void
 */
void mac_dbg_status_dump(struct mac_ax_adapter *adapter,
			 struct mac_ax_dbgpkg *val,
			 struct mac_ax_dbgpkg_en *en);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief dbg_port_sel
 *
 * @param *adapter
 * @param **info
 * @param sel
 * @return Please Place Description here.
 * @retval u32
 */
u32 dbg_port_sel(struct mac_ax_adapter *adapter,
		 struct mac_ax_dbg_port_info **info, u32 sel);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_sram_dbg_write
 *
 * @param *adapter
 * @param offset
 * @param val
 * @param sel
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_sram_dbg_write(struct mac_ax_adapter *adapter, u32 offset,
		       u32 val, enum mac_ax_sram_dbg_sel sel);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_sram_dbg_read
 *
 * @param *adapter
 * @param offset
 * @param sel
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_sram_dbg_read(struct mac_ax_adapter *adapter, u32 offset,
		      enum mac_ax_sram_dbg_sel sel);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_rx_cnt
 *
 * @param *adapter
 * @param *rxcnt
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_rx_cnt(struct mac_ax_adapter *adapter,
	       struct mac_ax_rx_cnt *rxcnt);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_dump_fw_rsvd_ple
 *
 * @param *adapter
 * @param **buf
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_dump_fw_rsvd_ple(struct mac_ax_adapter *adapter, u8 **buf);
/**
 * @}
 * @}
 */

/**
 * @brief mac_dump_ple_dbg_page
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval void
 */
void mac_dump_ple_dbg_page(struct mac_ax_adapter *adapter, u8 page_num);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_fw_dbg_dump
 *
 * @param *adapter
 * @param **buf
 * @param *en
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_fw_dbg_dump(struct mac_ax_adapter *adapter,
		    u8 **buf,
		    struct mac_ax_fwdbg_en *en);
/**
 * @}
 * @}
 */
u32 fw_st_dbg_dump(struct mac_ax_adapter *adapter);

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_event_notify
 * for Sta mode debug usage
 * @param *adapter
 * @param **buf
 * @param *en
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_event_notify(struct mac_ax_adapter *adapter, enum phl_msg_evt_id id,
		     u8 band);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup DebugPackage
 * @{
 */

/**
 * @brief mac_fwdbg_lock
 * lock or unlock fwdbgreg
 * @param *adapter
 * @param lock
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_fw_dbg_dle_cfg(struct mac_ax_adapter *adapter, bool lock);
/**
 * @}
 * @}
 */

/**
 * @brief pltfm_dbg_dump
 * dump cpu platform for dbg
 * @param *adapter
 */
void pltfm_dbg_dump(struct mac_ax_adapter *adapter);

/**
 * @}
 * @}
 */
/**
 * @brief mac_get_fw_status
 * get fw status
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_get_fw_status(struct mac_ax_adapter *adapter);
#endif
