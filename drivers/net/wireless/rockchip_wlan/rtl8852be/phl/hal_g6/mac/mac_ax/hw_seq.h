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

#ifndef _MAC_AX_HW_SEQ_H_
#define _MAC_AX_HW_SEQ_H_

#include "../type.h"
#include "fwcmd.h"

enum {
	R_AX_HW_SEQ_0 = 0,
	R_AX_HW_SEQ_1 = 1,
	R_AX_HW_SEQ_2 = 2,
	R_AX_HW_SEQ_3 = 3,
};

/**
 * @struct mac_ax_set_hwseq_reg
 * @brief mac_ax_set_hwseq_reg
 *
 * @var mac_ax_set_hwseq_reg::reg_idx
 * Please Place Description here.
 * @var mac_ax_set_hwseq_reg::seq_val
 * Please Place Description here.
 * @var mac_ax_set_hwseq_reg::rsvd0
 * Please Place Description here.
 */
struct mac_ax_set_hwseq_reg {
	u32 reg_idx: 2;
	u32 seq_val: 12;
	u32 rsvd0: 18;
};

/**
 * @addtogroup Basic_TRX
 * @{
 * @addtogroup TX_ShortCut
 * @{
 */
/**
 * @brief mac_set_hwseq_reg
 *
 * @param *adapter
 * @param idx
 * @param val
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_set_hwseq_reg(struct mac_ax_adapter *adapter,
		      u8 idx,
		      u16 val);
/**
 * @}
 * @}
 */

#endif
