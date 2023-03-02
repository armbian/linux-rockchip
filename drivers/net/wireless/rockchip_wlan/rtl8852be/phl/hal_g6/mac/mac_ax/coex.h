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

#ifndef _MAC_AX_COEX_H_
#define _MAC_AX_COEX_H_

#include "../mac_def.h"

#define MAC_AX_NOTIFY_TP_MAJOR 0x81
#define MAC_AX_NOTIFY_PWR_MAJOR 0x80

#define MAC_AX_NOTIFY_SH 24

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_coex_init
 *
 * @param *adapter
 * @param *coex
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_coex_init(struct mac_ax_adapter *adapter,
		  struct mac_ax_coex *coex);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_cfg_gnt
 *
 * @param *adapter
 * @param *gnt_cfg
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_cfg_gnt(struct mac_ax_adapter *adapter,
		struct mac_ax_coex_gnt *gnt_cfg);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_get_gnt
 *
 * @param *adapter
 * @param *gnt_cfg
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_get_gnt(struct mac_ax_adapter *adapter,
		struct mac_ax_coex_gnt *gnt_cfg);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_cfg_plt
 *
 * @param *adapter
 * @param *plt
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_cfg_plt(struct mac_ax_adapter *adapter, struct mac_ax_plt *plt);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_write_coex_reg
 *
 * @param *adapter
 * @param offset
 * @param val
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_write_coex_reg(struct mac_ax_adapter *adapter,
		       const u32 offset, const u32 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_read_coex_reg
 *
 * @param *adapter
 * @param offset
 * @param *val
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_read_coex_reg(struct mac_ax_adapter *adapter,
		      const u32 offset, u32 *val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_cfg_sb
 *
 * @param *adapter
 * @param val
 * @return Please Place Description here.
 * @retval void
 */

void mac_cfg_sb(struct mac_ax_adapter *adapter, u32 val);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_cfg_ctrl_path
 *
 * @param *adapter
 * @param wl
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_cfg_ctrl_path(struct mac_ax_adapter *adapter, u32 wl);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_get_ctrl_path
 *
 * @param *adapter
 * @param *wl
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_get_ctrl_path(struct mac_ax_adapter *adapter, u32 *wl);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_get_bt_polt_cnt
 *
 * @param *adapter
 * @param *cnt
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_get_bt_polt_cnt(struct mac_ax_adapter *adapter,
			struct mac_ax_bt_polt_cnt *cnt);
/**
 * @}
 * @}
 */

/**
 * @addtogroup Common
 * @{
 * @addtogroup BTCoex
 * @{
 */

/**
 * @brief mac_write_coex_mask
 *
 * Write coex reg with mask
 *
 * @param *adapter
 * @param offset
 * @param mask
 * @param val
 * @return 0 for success, others are fail
 * @retval u32
 */

u32 mac_write_coex_mask(struct mac_ax_adapter *adapter,
			u32 offset, u32 mask, u32 val);

/**
 * @}
 * @}
 */

#endif
