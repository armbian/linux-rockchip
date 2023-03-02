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

#ifndef _MAC_AX_STA_SCH_H_
#define _MAC_AX_STA_SCH_H_

#include "../type.h"

/*--------------------Define -------------------------------------------*/
#define STA_SCH_WMM_NUM_8852A 4
#define STA_SCH_WMM_NUM_8852B 2
#define STA_SCH_WMM_NUM_8852C 4

#define STA_SCH_UL_SUPPORT_8852A 1
#define STA_SCH_UL_SUPPORT_8852B 0
#define STA_SCH_UL_SUPPORT_8852C 1

#define CTRL1_R_NEXT_LINK 20

/*--------------------Define MACRO--------------------------------------*/
/*--------------------Define Enum---------------------------------------*/

/**
 * @enum mac_ax_ss_link_cfg
 *
 * @brief mac_ax_ss_link_cfg
 *
 * @var mac_ax_ss_link_cfg::mac_AX_SS_LINK_CFG_GET
 * Please Place Description here.
 * @var mac_ax_ss_link_cfg::mac_AX_SS_LINK_CFG_ADD
 * Please Place Description here.
 * @var mac_ax_ss_link_cfg::mac_AX_SS_LINK_CFG_DEL
 * Please Place Description here.
 * @var mac_ax_ss_link_cfg::mac_AX_SS_LINK_CFG_CLEAN
 * Please Place Description here.
 */
enum mac_ax_ss_link_cfg {
	MAC_AX_SS_LINK_CFG_GET,
	MAC_AX_SS_LINK_CFG_ADD,
	MAC_AX_SS_LINK_CFG_DEL,
	MAC_AX_SS_LINK_CFG_CLEAN,
};

/**
 * @enum mac_ax_ss_wmm_tbl_cfg
 *
 * @brief mac_ax_ss_wmm_tbl_cfg
 *
 * @var mac_ax_ss_wmm_tbl_cfg::mac_AX_SS_WMM_TBL_SET
 * Please Place Description here.
 */
enum mac_ax_ss_wmm_tbl_cfg {
	MAC_AX_SS_WMM_TBL_SET,
};

/*--------------------Define Struct-------------------------------------*/

/**
 * @struct mac_ax_sta_bmp_ctrl
 * @brief mac_ax_sta_bmp_ctrl
 *
 * @var mac_ax_sta_bmp_ctrl::macid
 * Please Place Description here.
 * @var mac_ax_sta_bmp_ctrl::bmp
 * Please Place Description here.
 * @var mac_ax_sta_bmp_ctrl::mask
 * Please Place Description here.
 */
struct mac_ax_sta_bmp_ctrl {
	u8 macid;
	u32 bmp;
	u32 mask;
};

/*--------------------Function Prototype--------------------------------*/

/**
 * @brief mac_ss_wmm_sta_move
 *
 * @param *adapter
 * @param src_wmm
 * @param dst_link
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_ss_wmm_sta_move(struct mac_ax_adapter *adapter,
			enum mac_ax_ss_wmm src_wmm,
			enum mac_ax_ss_wmm_tbl dst_link);

#endif
