/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation.
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
 *****************************************************************************/
#ifndef _HAL_TX_8852BE_H_
#define _HAL_TX_8852BE_H_

#define MAX_RX_TAG_VALUE 0x1FFF

#define ACH0_QUEUE_IDX_8852BE 0x0
#define ACH1_QUEUE_IDX_8852BE 0x1
#define ACH2_QUEUE_IDX_8852BE 0x2
#define ACH3_QUEUE_IDX_8852BE 0x3
#define ACH4_QUEUE_IDX_8852BE 0x4
#define ACH5_QUEUE_IDX_8852BE 0x5
#define ACH6_QUEUE_IDX_8852BE 0x6
#define ACH7_QUEUE_IDX_8852BE 0x7
#define MGQ_B0_QUEUE_IDX_8852BE 0x8
#define HIQ_B0_QUEUE_IDX_8852BE 0x9
#define MGQ_B1_QUEUE_IDX_8852BE 0xa
#define HIQ_B1_QUEUE_IDX_8852BE 0xb
#define FWCMD_QUEUE_IDX_8852BE	0xc

#define RX_QUEUE_IDX_8852BE	0x10
#define RP_QUEUE_IDX_8852BE	0x11

#define	RTW_TXDESC_QSEL_BE_0 0x0
#define	RTW_TXDESC_QSEL_BK_0 0x1
#define	RTW_TXDESC_QSEL_VI_0 0x2
#define	RTW_TXDESC_QSEL_VO_0 0x3
#define	RTW_TXDESC_QSEL_BE_1 0x4
#define	RTW_TXDESC_QSEL_BK_1 0x5
#define	RTW_TXDESC_QSEL_VI_1 0x6
#define	RTW_TXDESC_QSEL_VO_1 0x7
#define	RTW_TXDESC_QSEL_BE_2 0x8
#define	RTW_TXDESC_QSEL_BK_2 0x9
#define	RTW_TXDESC_QSEL_VI_2 0xa
#define	RTW_TXDESC_QSEL_VO_2 0xb
#define	RTW_TXDESC_QSEL_BE_3 0xc
#define	RTW_TXDESC_QSEL_BK_3 0xd
#define	RTW_TXDESC_QSEL_VI_3 0xe
#define	RTW_TXDESC_QSEL_VO_3 0xf

#define	RTW_TXDESC_QSEL_BCN_0 0x10
#define	RTW_TXDESC_QSEL_HIGH_0 0x11
#define	RTW_TXDESC_QSEL_MGT_0 0x12
#define	RTW_TXDESC_QSEL_MGT_NOPS_0 0x13
#define	RTW_TXDESC_QSEL_CPU_MGT_0 0x14
#define	RTW_TXDESC_QSEL_BCN_1 0x18
#define	RTW_TXDESC_QSEL_HIGH_1 0x19
#define	RTW_TXDESC_QSEL_MGT_1 0x1a
#define	RTW_TXDESC_QSEL_MGT_NOPS_1 0x1b
#define	RTW_TXDESC_QSEL_CPU_MGT_1 0x1c


/* AC channel * 8 + MGQ * 2 + HIQ * 2 + FW CMDQ * 1 */
#define TX_DMA_CHANNEL_ENTRY_8852BE 13
/* RXQ * 1 + RPQ * 1 */
#define RX_DMA_CHANNEL_ENTRY_8852BE 2


/* temp register definitions, will be replaced by halmac */
#define R_AX_RXQ_RXBD_IDX 0x1050
#define R_AX_RPQ_RXBD_IDX 0x1054
#define R_AX_ACH0_TXBD_IDX 0x1058
#define R_AX_ACH1_TXBD_IDX 0x105C
#define R_AX_ACH2_TXBD_IDX 0x1060
#define R_AX_ACH3_TXBD_IDX 0x1064
#define R_AX_ACH4_TXBD_IDX 0x1068
#define R_AX_ACH5_TXBD_IDX 0x106C
#define R_AX_ACH6_TXBD_IDX 0x1070
#define R_AX_ACH7_TXBD_IDX 0x1074
#define R_AX_CH8_TXBD_IDX 0x1078  /* Mgnt Queue band 0 */
#define R_AX_CH9_TXBD_IDX 0x107C  /* HI Queue band 0 */
#define R_AX_CH10_TXBD_IDX 0x137C /* Mgnt Queue band 1 */
#define R_AX_CH11_TXBD_IDX 0x1380 /* HI Queue band 1 */
#define R_AX_CH12_TXBD_IDX 0x1080 /* FW CMD */

#define R_AX_ACH0_TXBD_DESA_L 0x1110
#define R_AX_ACH0_TXBD_DESA_H 0x1114
#define R_AX_ACH1_TXBD_DESA_L 0x1118
#define R_AX_ACH1_TXBD_DESA_H 0x111C
#define R_AX_ACH2_TXBD_DESA_L 0x1120
#define R_AX_ACH2_TXBD_DESA_H 0x1124
#define R_AX_ACH3_TXBD_DESA_L 0x1128
#define R_AX_ACH3_TXBD_DESA_H 0x112C
#define R_AX_ACH4_TXBD_DESA_L 0x1130
#define R_AX_ACH4_TXBD_DESA_H 0x1134
#define R_AX_ACH5_TXBD_DESA_L 0x1138
#define R_AX_ACH5_TXBD_DESA_H 0x113C
#define R_AX_ACH6_TXBD_DESA_L 0x1140
#define R_AX_ACH6_TXBD_DESA_H 0x1144
#define R_AX_ACH7_TXBD_DESA_L 0x1148
#define R_AX_ACH7_TXBD_DESA_H 0x114C
#define R_AX_CH8_TXBD_DESA_L 0x1150
#define R_AX_CH8_TXBD_DESA_H 0x1154
#define R_AX_CH9_TXBD_DESA_L 0x1158
#define R_AX_CH9_TXBD_DESA_H 0x115C
#define R_AX_CH10_TXBD_DESA_L 0x1358
#define R_AX_CH10_TXBD_DESA_H 0x135C
#define R_AX_CH11_TXBD_DESA_L 0x1360
#define R_AX_CH11_TXBD_DESA_H 0x1364
#define R_AX_CH12_TXBD_DESA_L 0x1160
#define R_AX_CH12_TXBD_DESA_H 0x1164
#define R_AX_RXQ_RXBD_DESA_L 0x1100
#define R_AX_RXQ_RXBD_DESA_H 0x1104
#define R_AX_RPQ_RXBD_DESA_L 0x1108
#define R_AX_RPQ_RXBD_DESA_H 0x110C

#define B_AX_DESC_NUM_MSK 0xfff

#define R_AX_RXQ_RXBD_NUM 0x1020
#define R_AX_RPQ_RXBD_NUM 0x1022
#define R_AX_ACH0_TXBD_NUM 0x1024
#define R_AX_ACH1_TXBD_NUM 0x1026
#define R_AX_ACH2_TXBD_NUM 0x1028
#define R_AX_ACH3_TXBD_NUM 0x102A
#define R_AX_ACH4_TXBD_NUM 0x102C
#define R_AX_ACH5_TXBD_NUM 0x102E
#define R_AX_ACH6_TXBD_NUM 0x1030
#define R_AX_ACH7_TXBD_NUM 0x1032
#define R_AX_CH8_TXBD_NUM 0x1034
#define R_AX_CH9_TXBD_NUM 0x1036
#define R_AX_CH10_TXBD_NUM 0x1338
#define R_AX_CH11_TXBD_NUM 0x133A
#define R_AX_CH12_TXBD_NUM 0x1038


#define R_AX_ACH0_BDRAM_CTRL 0x1200
#define R_AX_ACH1_BDRAM_CTRL 0x1204
#define R_AX_ACH2_BDRAM_CTRL 0x1208
#define R_AX_ACH3_BDRAM_CTRL 0x120C
#define R_AX_ACH4_BDRAM_CTRL 0x1210
#define R_AX_ACH5_BDRAM_CTRL 0x1214
#define R_AX_ACH6_BDRAM_CTRL 0x1218
#define R_AX_ACH7_BDRAM_CTRL 0x121C
#define R_AX_CH8_BDRAM_CTRL 0x1220
#define R_AX_CH9_BDRAM_CTRL 0x1224
#define R_AX_CH10_BDRAM_CTRL 0x1320
#define R_AX_CH11_BDRAM_CTRL 0x1324
#define R_AX_CH12_BDRAM_CTRL 0x1228

#define R_AX_PCIE_INIT_CFG1 0x1000
#define B_AX_PCIE_RXRST_KEEP_REG BIT(23)
#define B_AX_PCIE_TXRST_KEEP_REG BIT(22)
#define B_AX_PCIE_PERST_KEEP_REG BIT(21)
#define B_AX_PCIE_FLR_KEEP_REG BIT(20)
#define B_AX_PCIE_TRAIN_KEEP_REG BIT(19)
#define B_AX_RXBD_MODE BIT(18)
#define B_AX_PCIE_MAX_RXDMA_SH 14
#define B_AX_PCIE_MAX_RXDMA_MSK 0x7
#define B_AX_RXHCI_EN BIT(13)
#define B_AX_LATENCY_CONTROL BIT(12)
#define B_AX_TXHCI_EN BIT(11)
#define B_AX_PCIE_MAX_TXDMA_SH 8
#define B_AX_PCIE_MAX_TXDMA_MSK 0x7
#define B_AX_TX_TRUNC_MODE BIT(5)
#define B_AX_RX_TRUNC_MODE BIT(4)
#define B_AX_RST_BDRAM BIT(3)
#define B_AX_DIS_RXDMA_PRE BIT(2)

#define R_AX_TXDMA_ADDR_H 0x10F0
#define R_AX_RXDMA_ADDR_H 0x10F4

#define R_AX_PCIE_DMA_STOP1 0x1010
#define B_AX_STOP_WPDMA BIT(19)
#define B_AX_STOP_CH12 BIT(18)
#define B_AX_STOP_CH9 BIT(17)
#define B_AX_STOP_CH8 BIT(16)
#define B_AX_STOP_ACH7 BIT(15)
#define B_AX_STOP_ACH6 BIT(14)
#define B_AX_STOP_ACH5 BIT(13)
#define B_AX_STOP_ACH4 BIT(12)
#define B_AX_STOP_ACH3 BIT(11)
#define B_AX_STOP_ACH2 BIT(10)
#define B_AX_STOP_ACH1 BIT(9)
#define B_AX_STOP_ACH0 BIT(8)
#define B_AX_STOP_RPQ BIT(1)
#define B_AX_STOP_RXQ BIT(0)


#define R_AX_PCIE_DMA_STOP2 0x1310
#define B_AX_STOP_CH11 BIT(1)
#define B_AX_STOP_CH10 BIT(0)

#define R_AX_TXBD_RWPTR_CLR1 0x1014
#define B_AX_CLR_CH12_IDX BIT(10)
#define B_AX_CLR_CH9_IDX BIT(9)
#define B_AX_CLR_CH8_IDX BIT(8)
#define B_AX_CLR_ACH7_IDX BIT(7)
#define B_AX_CLR_ACH6_IDX BIT(6)
#define B_AX_CLR_ACH5_IDX BIT(5)
#define B_AX_CLR_ACH4_IDX BIT(4)
#define B_AX_CLR_ACH3_IDX BIT(3)
#define B_AX_CLR_ACH2_IDX BIT(2)
#define B_AX_CLR_ACH1_IDX BIT(1)
#define B_AX_CLR_ACH0_IDX BIT(0)

#define R_AX_RXBD_RWPTR_CLR 0x1018
#define B_AX_CLR_RPQ_IDX BIT(1)
#define B_AX_CLR_RXQ_IDX BIT(0)


#define R_AX_TXBD_RWPTR_CLR2 0x1314
#define B_AX_CLR_CH11_IDX BIT(1)
#define B_AX_CLR_CH10_IDX BIT(0)

#define R_AX_PCIE_DMA_BUSY2 0x131C
#define B_AX_CH11_BUSY BIT(1)
#define B_AX_CH10_BUSY BIT(0)

#define GET_RX_RP_PKT_POLLUTED(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 31, 1)
#define GET_RX_RP_PKT_PCIE_SEQ(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 16, 15)
#define GET_RX_RP_PKT_TX_STS(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 13, 3)
#define GET_RX_RP_PKT_QSEL(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 8, 5)
#define GET_RX_RP_PKT_MAC_ID(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 0, 8)

/* TX BD */
#define SET_TXBUFFER_DESC_LEN(__pTxDesc, __Value) \
	SET_BITS_TO_LE_4BYTE(__pTxDesc + 0x00, 0, 16, __Value)
#define SET_TXBUFFER_DESC_ADD_HIGH(__pTxDesc, __Value) \
	SET_BITS_TO_LE_4BYTE(__pTxDesc + 0x00, 22, 8, __Value)
#define SET_TXBUFFER_DESC_LS(__pTxDesc, __Value)		\
	SET_BITS_TO_LE_4BYTE(__pTxDesc + 0x00, 30, 1, __Value)
#define SET_TXBUFFER_DESC_ADD_LOW(__pTxDesc, __Value) \
	SET_BITS_TO_LE_4BYTE(__pTxDesc + 0x04, 0, 32, __Value)

#define GET_TXBUFFER_DESC_LEN(_pTxDesc) \
	LE_BITS_TO_4BYTE(_pTxDesc + 0x00, 0, 16)
#define GET_TXBUFFER_DESC_ADD_HIGH(_pTxDesc) \
	LE_BITS_TO_4BYTE(_pTxDesc + 0x00, 22, 8)
#define GET_TXBUFFER_DESC_LS(_pTxDesc) \
	LE_BITS_TO_4BYTE(_pTxDesc + 0x00, 30, 1)
#define GET_TXBUFFER_DESC_ADD_LOW(_pTxDesc) \
	LE_BITS_TO_4BYTE(_pTxDesc + 0x04, 0, 32)



/* PCIE_SEQ Info */
#define SET_PCIE_SEQ_INFO_0(__seq_info, __value) \
	SET_BITS_TO_LE_4BYTE(__seq_info + 0x00, 0, 15, __value)
#define SET_PCIE_SEQ_INFO_0_VALID(__seq_info, __value) \
	SET_BITS_TO_LE_4BYTE(__seq_info + 0x00, 15, 1, __value)
#define SET_PCIE_SEQ_INFO_1(__seq_info, __value) \
	SET_BITS_TO_LE_4BYTE(__seq_info + 0x00, 16, 15, __value)
#define SET_PCIE_SEQ_INFO_1_VALID(__seq_info, __value) \
	SET_BITS_TO_LE_4BYTE(__seq_info + 0x00, 31, 1, __value)
#define SET_PCIE_SEQ_INFO_2(__seq_info, __value) \
	SET_BITS_TO_LE_4BYTE(__seq_info + 0x00, 0, 15, __value)
#define SET_PCIE_SEQ_INFO_2_VALID(__seq_info, __value) \
	SET_BITS_TO_LE_4BYTE(__seq_info + 0x00, 15, 1, __value)
#define SET_PCIE_SEQ_INFO_3(__seq_info, __value) \
	SET_BITS_TO_LE_4BYTE(__seq_info + 0x00, 16, 15, __value)
#define SET_PCIE_SEQ_INFO_3_VALID(__seq_info, __value) \
	SET_BITS_TO_LE_4BYTE(__seq_info + 0x00, 31, 1, __value)

/* PCIE_SEQ Info */
#define GET_PCIE_SEQ_INFO_0(__seq_info) \
	LE_BITS_TO_4BYTE(__seq_info + 0x00, 0, 15)
#define GET_PCIE_SEQ_INFO_0_VALID(__seq_info) \
	LE_BITS_TO_4BYTE(__seq_info + 0x00, 15, 1)
#define GET_PCIE_SEQ_INFO_1(__seq_info) \
	LE_BITS_TO_4BYTE(__seq_info + 0x00, 16, 15)
#define GET_PCIE_SEQ_INFO_1_VALID(__seq_info) \
	LE_BITS_TO_4BYTE(__seq_info + 0x00, 31, 1)
#define GET_PCIE_SEQ_INFO_2(__seq_info) \
	LE_BITS_TO_4BYTE(__seq_info + 0x00, 0, 15)
#define GET_PCIE_SEQ_INFO_2_VALID(__seq_info) \
	LE_BITS_TO_4BYTE(__seq_info + 0x00, 15, 1)
#define GET_PCIE_SEQ_INFO_3(__seq_info) \
	LE_BITS_TO_4BYTE(__seq_info + 0x00, 16, 15)
#define GET_PCIE_SEQ_INFO_3_VALID(__seq_info) \
	LE_BITS_TO_4BYTE(__seq_info + 0x00, 31, 1)


/* Addr Info */
#define SET_ADDR_INFO_LEN(__addr_info, __value) \
	SET_BITS_TO_LE_4BYTE(__addr_info + 0x00, 0, 16, __value)
#define SET_ADDR_INFO_NUM(__addr_info, __value) \
	SET_BITS_TO_LE_4BYTE(__addr_info + 0x00, 16, 6, __value)
#define SET_ADDR_INFO_ADDR_HIGH(__addr_info, __value) \
	SET_BITS_TO_LE_4BYTE(__addr_info + 0x00, 22, 8, __value)
#define SET_ADDR_INFO_LS(__addr_info, __value)				\
	SET_BITS_TO_LE_4BYTE(__addr_info + 0x00, 30, 1, __value)
#define SET_ADDR_INFO_MSDU_LS(__addr_info, __value) \
	SET_BITS_TO_LE_4BYTE(__addr_info + 0x00, 31, 1, __value)
#define SET_ADDR_INFO_ADDR_LOW(__addr_info, __value) \
	SET_BITS_TO_LE_4BYTE(__addr_info + 0x04, 0, 32, __value)

/* Addr Info */
#define GET_ADDR_INFO_LEN(__addr_info) \
	LE_BITS_TO_4BYTE(__addr_info + 0x00, 0, 16)
#define GET_ADDR_INFO_NUM(__addr_info) \
	LE_BITS_TO_4BYTE(__addr_info + 0x00, 16, 6)
#define GET_ADDR_INFO_ADDR_HIGH(__addr_info) \
	LE_BITS_TO_4BYTE(__addr_info + 0x00, 22, 8)
#define GET_ADDR_INFO_LS(__addr_info)			\
	LE_BITS_TO_4BYTE(__addr_info + 0x00, 30, 1)
#define GET_ADDR_INFO_MSDU_LS(__addr_info) \
	LE_BITS_TO_4BYTE(__addr_info + 0x00, 31, 1)
#define GET_ADDR_INFO_ADDR_LOW(__addr_info) \
	LE_BITS_TO_4BYTE(__addr_info + 0x04, 0, 32)


#define RX_RP_PACKET_SIZE 4

/* RX BD */
#define SET_RX_BD_RXBUFFSIZE(__pRxBd, __Value) \
	SET_BITS_TO_LE_4BYTE(__pRxBd + 0x00, 0, 14, __Value)
#define SET_RX_BD_PHYSICAL_ADDR_HIGH(__pRxBd, __Value) \
	SET_BITS_TO_LE_4BYTE(__pRxBd + 0x00, 22, 8, __Value)
#define SET_RX_BD_PHYSICAL_ADDR_LOW(__pRxBd, __Value) \
	SET_BITS_TO_LE_4BYTE(__pRxBd + 0x04, 0, 32, __Value)

/* RX RD INFO */
#define SET_RX_BD_INFO_TAG(rxdesc, value) \
	SET_BITS_TO_LE_4BYTE(rxdesc + 0x00, 16, 13, value)
#define SET_RX_BD_INFO_FS(rxdesc, value) \
	SET_BITS_TO_LE_4BYTE(rxdesc + 0x00, 15, 1, value)
#define SET_RX_BD_INFO_LS(rxdesc, value) \
	SET_BITS_TO_LE_4BYTE(rxdesc + 0x00, 14, 1, value)
#define SET_RX_BD_INFO_HW_W_SIZE(rxdesc, value) \
	SET_BITS_TO_LE_4BYTE(rxdesc + 0x00, 0, 14, value)

#define GET_RX_BD_INFO_TAG(rxdesc) LE_BITS_TO_4BYTE(rxdesc + 0x00, 16, 13)
#define GET_RX_BD_INFO_FS(rxdesc) LE_BITS_TO_4BYTE(rxdesc + 0x00, 15, 1)
#define GET_RX_BD_INFO_LS(rxdesc) LE_BITS_TO_4BYTE(rxdesc + 0x00, 14, 1)
#define GET_RX_BD_INFO_HW_W_SIZE(rxdesc) LE_BITS_TO_4BYTE(rxdesc + 0x00, 0, 14)

/* RX RP PACKET */
#define SET_RX_RP_PKT_POLLUTED(rppkt, value) \
	SET_BITS_TO_LE_4BYTE(rppkt + 0x00, 31, 1, value)
#define SET_RX_RP_PKT_PCIE_SEQ(rppkt, value) \
	SET_BITS_TO_LE_4BYTE(rppkt + 0x00, 16, 15, value)
#define SET_RX_RP_PKT_TX_STS(rppkt, value) \
	SET_BITS_TO_LE_4BYTE(rppkt + 0x00, 13, 3, value)
#define SET_RX_RP_PKT_QSEL(rppkt, value) \
	SET_BITS_TO_LE_4BYTE(rppkt + 0x00, 8, 5, value)
#define SET_RX_RP_PKT_MAC_ID(rppkt, value) \
	SET_BITS_TO_LE_4BYTE(rppkt + 0x00, 0, 8, value)

#define GET_RX_RP_PKT_POLLUTED(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 31, 1)
#define GET_RX_RP_PKT_PCIE_SEQ(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 16, 15)
#define GET_RX_RP_PKT_TX_STS(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 13, 3)
#define GET_RX_RP_PKT_QSEL(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 8, 5)
#define GET_RX_RP_PKT_MAC_ID(rppkt) LE_BITS_TO_4BYTE(rppkt + 0x00, 0, 8)

/* CONFIG_PHL_TXSC */
#define TID_0_QSEL 0
#define TID_1_QSEL 1
#define TID_2_QSEL 1
#define TID_3_QSEL 0
#define TID_4_QSEL 2
#define TID_5_QSEL 2
#define TID_6_QSEL 3
#define TID_7_QSEL 3
#define TID_0_IND 0
#define TID_1_IND 0
#define TID_2_IND 1
#define TID_3_IND 1
#define TID_4_IND 0
#define TID_5_IND 1
#define TID_6_IND 0
#define TID_7_IND 1

enum rxbd_mode_8852BE {
	RXBD_MODE_PACKET = 0,
	RXBD_MODE_SEPARATION = 1,
	RXBD_MODE_MAX = 0xFF
};

struct bd_ram {
	u8 sidx;
	u8 max;
	u8 min;
};

#endif

