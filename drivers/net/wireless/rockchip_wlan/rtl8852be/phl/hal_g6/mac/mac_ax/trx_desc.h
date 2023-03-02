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

#ifndef _MAC_AX_TRX_DESC_H_
#define _MAC_AX_TRX_DESC_H_

#include "../type.h"
#include "role.h"
#if MAC_AX_8852C_SUPPORT
#include "mac_8852c/trx_desc_8852c.h"
#endif
#if MAC_AX_8192XB_SUPPORT
#include "mac_8192xb/trx_desc_8192xb.h"
#endif

/* VHT/HE nss = rx_rate[6:4] */
#define GET_NSS_FROM_RX_RATE(rate) (((rate) & 0x70) >> 4)
#define SET_NSS_TO_RX_RATE(rate, nss) (((rate) & 0xFF8F) | (((nss) & 0x7) << 4))
/**
 * 0000: WIFI packet
 * 0001: PPDU status
 * 0010: channel info
 * 0011: BB scope mode
 * 0100: F2P TX CMD report
 * 0101: SS2FW report
 * 0110: TX report
 * 0111: TX payload release to host
 * 1000: DFS report
 * 1001: TX payload release to WLCPU
 * 1010: C2H packet
 */
#define RX_8852A_DESC_PKT_T_WIFI 0
#define RX_8852A_DESC_PKT_T_PPDU_STATUS 1
#define RX_8852A_DESC_PKT_T_CHANNEL_INFO 2
#define RX_8852A_DESC_PKT_T_BB_SCOPE 3
#define RX_8852A_DESC_PKT_T_F2P_TX_CMD_RPT 4
#define RX_8852A_DESC_PKT_T_SS2FW_RPT 5
#define RX_8852A_DESC_PKT_T_TX_RPT 6
#define RX_8852A_DESC_PKT_T_TX_PD_RELEASE_HOST 7
#define RX_8852A_DESC_PKT_T_DFS_RPT 8
#define RX_8852A_DESC_PKT_T_TX_PD_RELEASE_WLCPU 9
#define RX_8852A_DESC_PKT_T_C2H 10

#define RX_8852A_DESC_PPDU_T_LCCK 0
#define RX_8852A_DESC_PPDU_T_SCCK 1
#define RX_8852A_DESC_PPDU_T_OFDM 2
#define RX_8852A_DESC_PPDU_T_HT 3
#define RX_8852A_DESC_PPDU_T_HTGF 4
#define RX_8852A_DESC_PPDU_T_VHT_SU 5
#define RX_8852A_DESC_PPDU_T_VHT_MU 6
#define RX_8852A_DESC_PPDU_T_HE_SU 7
#define RX_8852A_DESC_PPDU_T_HE_ERSU 8
#define RX_8852A_DESC_PPDU_T_HE_MU 9
#define RX_8852A_DESC_PPDU_T_HE_TB 10
#define RX_8852A_DESC_PPDU_T_UNKNOWN 15

#define RX_8852A_DESC_RX_GI_LTF_4X_3_2 0
#define RX_8852A_DESC_RX_GI_LTF_4X_0_8 1
#define RX_8852A_DESC_RX_GI_LTF_2X_1_6 2
#define RX_8852A_DESC_RX_GI_LTF_2X_0_8 3
#define RX_8852A_DESC_RX_GI_LTF_1X_1_6 4
#define RX_8852A_DESC_RX_GI_LTF_1X_0_8 5

#define NSS_8 7
#define NSS_7 6
#define NSS_6 5
#define NSS_5 4
#define NSS_4 3
#define NSS_3 2
#define NSS_2 1
#define NSS_1 0

/**
 * @addtogroup Basic_TRX
 * @{
 * @addtogroup TX_Config
 * @{
 */
/**
 * @brief mac_txdesc_len
 *
 * @param *adapter
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_txdesc_len(struct mac_ax_adapter *adapter,
		   struct rtw_t_meta_data *info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Basic_TRX
 * @{
 * @addtogroup TX_Config
 * @{
 */

/**
 * @brief mac_build_txdesc
 *
 * @param *adapter
 * @param *info
 * @param *buf
 * @param len
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_build_txdesc(struct mac_ax_adapter *adapter,
		     struct rtw_t_meta_data *info, u8 *buf, u32 len);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Basic_TRX
 * @{
 * @addtogroup TX_Config
 * @{
 */

/**
 * @brief mac_refill_txdesc
 *
 * @param *adapter
 * @param *txpkt_info
 * @param *mask
 * @param *info
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_refill_txdesc(struct mac_ax_adapter *adapter,
		      struct rtw_t_meta_data *txpkt_info,
		      struct mac_ax_refill_info *mask,
		      struct mac_ax_refill_info *info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Basic_TRX
 * @{
 * @addtogroup TX_Config
 * @{
 */

/**
 * @brief mac_parse_rxdesc
 *
 * @param *adapter
 * @param *info
 * @param *buf
 * @param len
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_parse_rxdesc(struct mac_ax_adapter *adapter,
		     struct mac_ax_rxpkt_info *info, u8 *buf, u32 len);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Basic_TRX
 * @{
 * @addtogroup TX_Config
 * @{
 */

/**
 * @brief mac_wd_checksum
 *
 * @param *adapter
 * @param *info
 * @param *wddesc
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_wd_checksum(struct mac_ax_adapter *adapter,
		    struct rtw_t_meta_data *info, u8 *wddesc);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Basic_TRX
 * @{
 * @addtogroup TX_Config
 * @{
 */

/**
 * @brief mac_patch_rx_rate
 *
 * @param *adapter
 * @param *rtw_r_meta_data
 * @return For RMAC rate patch func
 * @retval u32
 */
u32 mac_patch_rx_rate(struct mac_ax_adapter *adapter,
		      struct rtw_r_meta_data *info);
/**
 * @}
 * @}
 */

#endif
