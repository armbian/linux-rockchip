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

#ifndef _MAC_AX_PCIE_H_
#define _MAC_AX_PCIE_H_

#include "../type.h"
#include "../pcie_reg.h"
#include "../mac_ax.h"

/*--------------------Define -------------------------------------------*/
#define INTF_INTGRA_MINREF_V1	90
#define INTF_INTGRA_HOSTREF_V1	100
#define GET_PCIE_FUNC_STUS(val, mask) (((val) & (mask)) ? \
			   MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE)

#define PCIE_POLL_IO_IDLE_CNT 100
#define PCIE_POLL_IO_IDLE_DLY_US 10
#define PCIE_POLL_DMACH_IDLE_CNT 100
#define PCIE_POLL_DMACH_IDLE_DLY_US 10

#define PCIE_POLL_BDRAM_RST_CNT 10000
#define PCIE_POLL_BDRAM_RST_DLY_US 50

#define PCIE_POLL_AUTOK_CNT 1000
#define PCIE_POLL_AUTOK_DLY_US 50

#define DBI_ADDR_MASK 0xFFC
#define DBI_ADDR_2LSB_MASK 0x3
#define DBI_WEN_DW 0xF
#define DBI_WEN_B 1
#define DBI_DLY_CNT 20
#define DBI_DLY_US 10

#define MDIO_ADDR_PG1 0x20
#define MDIO_DLY_CNT 20
#define MDIO_DLY_US 10

#define BDRAM_SIDX_MSK 0x1f
#define BDRAM_MAX_MSK 0x1f00
#define BDRAM_MIN_MSK 0x1f0000

#define MDIO_PG0_G1 0
#define MDIO_PG1_G1 1
#define MDIO_PG0_G2 2
#define MDIO_PG1_G2 3

#define BD_NORM_SIZE 12
#define BD_TRUNC_SIZE 8
#define RXBD_SEP_NORM_SIZE 20
#define RXBD_SEP_TRUNC_OLD_SIZE 12
#define RXBD_SEP_TRUNC_NEW_SIZE 16

#define BD_MAX_NUM 0x3FF
#define BD_IDX_INVALID 0xFFFF
#define TXBD_BYTE_ALIGN 8
#define RXBD_BYTE_ALIGN 4

#define CMAC_CLK_ALLEN 0xFFFFFFFF

#define PC_POWER_UP 1
#define PC_POWER_DOWN 0
#define BIT_WAKE_CTRL BIT5
#define PCIE_DEFAULT_AGG_NUM 0x40

#define PCIE_AUTOK_DIV_2048 0x0
#define PCIE_AUTOK_MGN 0x8
#define PCIE_AUTOK_MGN_2048 64
#define PCIE_AUTOK_UD_CNT 30
#define PCIE_DPHY_DLY_25US 0x1
#define PCIE_AUTOK_4 0x3

/*--------------------Define MACRO--------------------------------------*/
/*--------------------Define Enum---------------------------------------*/
enum pcie_clkdly_hw {
	PCIE_CLKDLY_HW_0 = 0,
	PCIE_CLKDLY_HW_30US = 0x1,
	PCIE_CLKDLY_HW_50US = 0x2,
	PCIE_CLKDLY_HW_80US = 0x3,
	PCIE_CLKDLY_HW_100US = 0x4,
	PCIE_CLKDLY_HW_120US = 0x5,
	PCIE_CLKDLY_HW_150US = 0x6,
	PCIE_CLKDLY_HW_180US = 0x7,
	PCIE_CLKDLY_HW_200US = 0x8,
	PCIE_CLKDLY_HW_300US = 0x9,
	PCIE_CLKDLY_HW_400US = 0xA,
	PCIE_CLKDLY_HW_500US = 0xB,
	PCIE_CLKDLY_HW_1MS = 0xC,
	PCIE_CLKDLY_HW_3MS = 0xD,
	PCIE_CLKDLY_HW_5MS = 0xE,
	PCIE_CLKDLY_HW_10MS = 0xF
};

enum pcie_l1dly_hw {
	PCIE_L1DLY_HW_16US = 4,
	PCIE_L1DLY_HW_32US = 5,
	PCIE_L1DLY_HW_64US = 6,
	PCIE_L1DLY_HW_INFI = 7
};

enum pcie_l0sdly_hw {
	PCIE_L0SDLY_HW_1US = 0,
	PCIE_L0SDLY_HW_2US = 1,
	PCIE_L0SDLY_HW_3US = 2,
	PCIE_L0SDLY_HW_4US = 3,
	PCIE_L0SDLY_HW_5US = 4,
	PCIE_L0SDLY_HW_6US = 5,
	PCIE_L0SDLY_HW_7US = 6
};

/*--------------------Define Struct-------------------------------------*/
struct txbd_ram {
	u8 sidx;
	u8 max;
	u8 min;
};

/**
 * @brief reg_read8_pcie
 *
 * @param *adapter
 * @param addr
 * @return Please Place Description here.
 * @retval u8
 */
u8 reg_read8_pcie(struct mac_ax_adapter *adapter, u32 addr);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */

/**
 * @brief reg_write8_pcie
 *
 * @param *adapter
 * @param addr
 * @param val
 * @return Please Place Description here.
 * @retval void
 */
void reg_write8_pcie(struct mac_ax_adapter *adapter, u32 addr, u8 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */

/**
 * @brief reg_read16_pcie
 *
 * @param *adapter
 * @param addr
 * @return Please Place Description here.
 * @retval u16
 */
u16 reg_read16_pcie(struct mac_ax_adapter *adapter, u32 addr);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */
/**
 * @brief reg_write16_pcie
 *
 * @param *adapter
 * @param addr
 * @param val
 * @return Please Place Description here.
 * @retval void
 */
void reg_write16_pcie(struct mac_ax_adapter *adapter, u32 addr, u16 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */

/**
 * @brief reg_read32_pcie
 *
 * @param *adapter
 * @param addr
 * @return Please Place Description here.
 * @retval u32
 */
u32 reg_read32_pcie(struct mac_ax_adapter *adapter, u32 addr);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup BasicIO
 * @{
 */

/**
 * @brief reg_write32_pcie
 *
 * @param *adapter
 * @param addr
 * @param val
 * @return Please Place Description here.
 * @retval void
 */
void reg_write32_pcie(struct mac_ax_adapter *adapter, u32 addr, u32 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief dbi_r8_pcie
 *
 * @param *adapter
 * @param addr
 * @param *val
 * @return Please Place Description here.
 * @retval u32
 */
u32 dbi_r8_pcie(struct mac_ax_adapter *adapter, u16 addr, u8 *val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief dbi_w8_pcie
 *
 * @param *adapter
 * @param addr
 * @param data
 * @return Please Place Description here.
 * @retval u32
 */
u32 dbi_w8_pcie(struct mac_ax_adapter *adapter, u16 addr, u8 data);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief dbi_r32_pcie
 *
 * @param *adapter
 * @param addr
 * @param *val
 * @return Please Place Description here.
 * @retval u32
 */
u32 dbi_r32_pcie(struct mac_ax_adapter *adapter, u16 addr, u32 *val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief dbi_w32_pcie
 *
 * @param *adapter
 * @param addr
 * @param data
 * @return Please Place Description here.
 * @retval u32
 */
u32 dbi_w32_pcie(struct mac_ax_adapter *adapter, u16 addr, u32 data);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief mdio_r16_pcie
 *
 * @param *adapter
 * @param addr
 * @param speed
 * @param *val
 * @return Please Place Description here.
 * @retval u32
 */
u32 mdio_r16_pcie(struct mac_ax_adapter *adapter, u8 addr, u8 speed, u16 *val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief mdio_w16_pcie
 *
 * @param *adapter
 * @param addr
 * @param data
 * @param speed
 * @return Please Place Description here.
 * @retval u32
 */
u32 mdio_w16_pcie(struct mac_ax_adapter *adapter, u8 addr, u16 data, u8 speed);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief cfgspc_set_pcie
 *
 * @param *adapter
 * @param *param
 * @return Please Place Description here.
 * @retval u32
 */
u32 cfgspc_set_pcie(struct mac_ax_adapter *adapter,
		    struct mac_ax_pcie_cfgspc_param *param);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief ltr_set_pcie
 *
 * @param *adapter
 * @param *param
 * @return Please Place Description here.
 * @retval u32
 */
u32 ltr_set_pcie(struct mac_ax_adapter *adapter,
		 struct mac_ax_pcie_ltr_param *param);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief ltr_sw_trigger
 *
 * @param *adapter
 * @param ctrl
 * @return Please Place Description here.
 * @retval u32
 */
u32 ltr_sw_trigger(struct mac_ax_adapter *adapter,
		   enum mac_ax_pcie_ltr_sw_ctrl ctrl);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief ctrl_dma_all_pcie
 *
 * @param *adapter
 * @param en
 * @return Please Place Description here.
 * @retval void
 */
void ctrl_dma_all_pcie(struct mac_ax_adapter *adapter, enum mac_ax_func_sw en);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief clr_idx_all_pcie
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 clr_idx_all_pcie(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief ctrl_txdma_ch_pcie
 *
 * @param *adapter
 * @param *ch_map
 * @return Please Place Description here.
 * @retval u32
 */
u32 ctrl_txdma_ch_pcie(struct mac_ax_adapter *adapter,
		       struct mac_ax_txdma_ch_map *ch_map);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief poll_txdma_ch_idle_pcie
 *
 * @param *adapter
 * @param *ch_map
 * @return Please Place Description here.
 * @retval u32
 */
u32 poll_txdma_ch_idle_pcie(struct mac_ax_adapter *adapter,
			    struct mac_ax_txdma_ch_map *ch_map);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief poll_rxdma_ch_idle_pcie
 *
 * @param *adapter
 * @param *ch_map
 * @return Please Place Description here.
 * @retval u32
 */
u32 poll_rxdma_ch_idle_pcie(struct mac_ax_adapter *adapter,
			    struct mac_ax_rxdma_ch_map *ch_map);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief poll_dma_all_idle_pcie
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 poll_dma_all_idle_pcie(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief ctrl_txhci_pcie
 *
 * @param *adapter
 * @param en
 * @return Please Place Description here.
 * @retval u32
 */
u32 ctrl_txhci_pcie(struct mac_ax_adapter *adapter, enum mac_ax_func_sw en);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief ctrl_rxhci_pcie
 *
 * @param *adapter
 * @param en
 * @return Please Place Description here.
 * @retval u32
 */
u32 ctrl_rxhci_pcie(struct mac_ax_adapter *adapter, enum mac_ax_func_sw en);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief ctrl_dma_io_pcie
 *
 * @param *adapter
 * @param en
 * @return Please Place Description here.
 * @retval u32
 */
u32 ctrl_dma_io_pcie(struct mac_ax_adapter *adapter, enum mac_ax_func_sw en);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief pcie_pre_init
 *
 * @param *adapter
 * @param *param
 * @return Please Place Description here.
 * @retval u32
 */
u32 pcie_pre_init(struct mac_ax_adapter *adapter, void *param);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief pcie_init
 *
 * @param *adapter
 * @param *param
 * @return Please Place Description here.
 * @retval u32
 */
u32 pcie_init(struct mac_ax_adapter *adapter, void *param);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief pcie_deinit
 *
 * @param *adapter
 * @param *param
 * @return Please Place Description here.
 * @retval u32
 */
u32 pcie_deinit(struct mac_ax_adapter *adapter, void *param);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief rst_bdram_pcie
 *
 * @param *adapter
 * @param val
 * @return Please Place Description here.
 * @retval u32
 */
u32 rst_bdram_pcie(struct mac_ax_adapter *adapter, u8 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief lv1rst_stop_dma_pcie
 *
 * @param *adapter
 * @param val
 * @return Please Place Description here.
 * @retval u32
 */
u32 lv1rst_stop_dma_pcie(struct mac_ax_adapter *adapter, u8 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief lv1rst_start_dma_pcie
 *
 * @param *adapter
 * @param val
 * @return Please Place Description here.
 * @retval u32
 */
u32 lv1rst_start_dma_pcie(struct mac_ax_adapter *adapter, u8 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief pcie_pwr_switch
 *
 * @param *vadapter
 * @param pre_switch
 * @param on
 * @return Please Place Description here.
 * @retval u32
 */
u32 pcie_pwr_switch(void *vadapter,
		    u8 pre_switch, u8 on);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief set_pcie_wowlan
 *
 * @param *adapter
 * @param w_c
 * @return Please Place Description here.
 * @retval u32
 */
u32 set_pcie_wowlan(struct mac_ax_adapter *adapter, enum mac_ax_wow_ctrl w_c);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief pcie_trx_mit
 *
 * @param *adapter
 * @param *mit_info
 * @return Please Place Description here.
 * @retval u32
 */
u32 pcie_trx_mit(struct mac_ax_adapter *adapter,
		 struct mac_ax_pcie_trx_mitigation *mit_info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief set_pcie_l2_leave
 *
 * @param *adapter
 * @param set
 * @return Please Place Description here.
 * @retval u32
 */
u32 set_pcie_l2_leave(struct mac_ax_adapter *adapter, u8 set);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief get_io_stat_pcie
 *
 * @param *adapter
 * @param *out_st
 * @return Please Place Description here.
 * @retval u32
 */
u32 get_io_stat_pcie(struct mac_ax_adapter *adapter,
		     struct mac_ax_io_stat *out_st);
/**
 * @}
 * @}
 */

/**
 * @brief pcie_get_txagg_num
 *
 * @param *adapter
 * @param band
 * @return Please Place Description here.
 * @retval u32
 */
u32 pcie_get_txagg_num(struct mac_ax_adapter *adapter, u8 band);
/**
 * @}
 * @}
 */

/**
 * @brief pcie_autok_counter_avg
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 pcie_autok_counter_avg(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup HCI
 * @{
 * @addtogroup PCIE
 * @{
 */

/**
 * @brief dbcc_hci_ctrl_pcie
 *
 * @param *adapter
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 dbcc_hci_ctrl_pcie(struct mac_ax_adapter *adapter,
		       struct mac_ax_dbcc_hci_ctrl *info);
/**
 * @}
 * @}
 */
#endif
