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

#ifndef _MAC_AX_DBCC_H_
#define _MAC_AX_DBCC_H_

#include "../type.h"
#include "trxcfg.h"
#include "sta_sch.h"

/*--------------------Define -------------------------------------------*/
/*--------------------Define MACRO--------------------------------------*/
/*--------------------Define Enum---------------------------------------*/
/*--------------------Define Struct-------------------------------------*/
/*--------------------Function Prototype--------------------------------*/

/**
 * @brief mac_dbcc_enable
 *
 * @param *adapter
 * @param *info
 * @param dbcc_en
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_dbcc_enable(struct mac_ax_adapter *adapter,
		    struct mac_ax_trx_info *info, u8 dbcc_en);

/**
 * @brief mac_dbcc_move_macid
 *
 * @param *adapter
 * @param macid
 * @param dbcc_en
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_dbcc_move_macid(struct mac_ax_adapter *adapter, u8 macid, u8 dbcc_en);

#endif
