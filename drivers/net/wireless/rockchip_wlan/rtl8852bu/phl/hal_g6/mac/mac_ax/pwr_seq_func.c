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

#include "pwr_seq_func.h"
#define MAC_AX_PWR_POLL_CNT 20
#define MAC_AX_PWR_POLL_MS 1

u32 pwr_poll_u32(struct mac_ax_adapter *adapter, u32 offset,
		 u32 mask, u32 pwr_val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 cnt = MAC_AX_PWR_POLL_CNT, val;

	while (--cnt) {
		val = MAC_REG_R32(offset);
		if ((val & mask) == pwr_val)
			return MACSUCCESS;
		PLTFM_DELAY_MS(MAC_AX_PWR_POLL_MS);
	}

	PLTFM_MSG_ERR("[ERR] Power sequence polling timeout\n");
	PLTFM_MSG_ERR("[ERR] offset: %08X\n", offset);
	PLTFM_MSG_ERR("[ERR] read val: %08X, targe val: %08X\n", (val & mask), pwr_val);

	return MACPOLLTO;
}

u32 pwr_poll_u16(struct mac_ax_adapter *adapter, u32 offset,
		 u16 mask, u16 pwr_val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 cnt = MAC_AX_PWR_POLL_CNT;
	u16 val;

	while (--cnt) {
		val = MAC_REG_R16(offset);
		if ((val & mask) == pwr_val)
			return MACSUCCESS;
		PLTFM_DELAY_MS(MAC_AX_PWR_POLL_MS);
	}

	PLTFM_MSG_ERR("[ERR] Power sequence polling timeout\n");
	PLTFM_MSG_ERR("[ERR] offset: %08X\n", offset);
	PLTFM_MSG_ERR("[ERR] read val: %04X, targe val: %04X\n", (val & mask), pwr_val);

	return MACPOLLTO;
}

u32 pwr_poll_u8(struct mac_ax_adapter *adapter, u32 offset,
		u8 mask, u8 pwr_val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 cnt = MAC_AX_PWR_POLL_CNT;
	u8 val;

	while (--cnt) {
		val = MAC_REG_R8(offset);
		if ((val & mask) == pwr_val)
			return MACSUCCESS;
		PLTFM_DELAY_MS(MAC_AX_PWR_POLL_MS);
	}

	PLTFM_MSG_ERR("[ERR] Power sequence polling timeout\n");
	PLTFM_MSG_ERR("[ERR] offset: %08X\n", offset);
	PLTFM_MSG_ERR("[ERR] read val: %02X, targe val: %02X\n", (val & mask), pwr_val);

	return MACPOLLTO;
}
