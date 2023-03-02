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

#ifndef _MAC_AX_HDR_CONV_H_
#define _MAC_AX_HDR_CONV_H_

#include "../type.h"

/**
 * @struct mac_ax_en_hdr_conv
 * @brief mac_ax_en_hdr_conv
 *
 * @var mac_ax_en_hdr_conv::enable
 * Please Place Description here.
 * @var mac_ax_en_hdr_conv::rsvd0
 * Please Place Description here.
 */
struct mac_ax_en_hdr_conv {
	u32 enable: 1;
	u32 rsvd0: 31;
};

/**
 * @addtogroup Basic_TRX
 * @{
 * @addtogroup TX_ShortCut
 * @{
 */
/**
 * @brief mac_hdr_conv
 *
 * @param *adapter
 * @param en_hdr_conv
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_hdr_conv(struct mac_ax_adapter *adapter, u8 en_hdr_conv);
/**
 * @}
 * @}
 */

#endif
