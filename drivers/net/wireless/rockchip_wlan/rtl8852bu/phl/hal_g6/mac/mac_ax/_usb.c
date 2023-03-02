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
#include "_usb.h"
#include "../mac_ax.h"

#if MAC_AX_USB_SUPPORT

u32 usb_flush_mode(struct mac_ax_adapter *adapter, u8 mode)
{
	u32 reg, val32;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
		reg = R_AX_USB_WLAN0_1;
	else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
		reg = R_AX_USB_WLAN0_1;
	else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
		reg = R_AX_USB_WLAN0_1_V1;
	else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
		reg = R_AX_USB_WLAN0_1_V1;
	else
		return MACCHIPID;

	if (mode == MAC_AX_FUNC_DIS) {
		val32 = PLTFM_REG_R32(reg) & ~(B_AX_USBRX_RST | B_AX_USBTX_RST);
		PLTFM_REG_W32(reg, val32);
		return MACSUCCESS;
	} else if (mode == MAC_AX_FUNC_EN) {
		val32 = PLTFM_REG_R32(reg) | B_AX_USBRX_RST | B_AX_USBTX_RST;
		PLTFM_REG_W32(reg, val32);
		return MACSUCCESS;
	} else {
		return MACLV1STEPERR;
	}
}

u32 get_usb_mode(struct mac_ax_adapter *adapter)
{
	u32 reg, val32, hs;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
		reg = R_AX_USB_STATUS;
	else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B))
		reg = R_AX_USB_STATUS;
	else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C))
		reg = R_AX_USB_STATUS_V1;
	else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
		reg = R_AX_USB_STATUS_V1;
	else
		return MACCHIPID;

	val32 = PLTFM_REG_R32(reg) & B_AX_R_USB2_SEL;
	hs = PLTFM_REG_R32(reg) & B_AX_MODE_HS;
	if (val32 == B_AX_R_USB2_SEL)
		val32 = MAC_AX_USB3;
	else if ((val32 != B_AX_R_USB2_SEL) && (hs == B_AX_MODE_HS))
		val32 = MAC_AX_USB2;
	else
		val32 = MAC_AX_USB11;
	return val32;
}

u32 usb_autok_counter_avg(struct mac_ax_adapter *adapter)
{
	return MACSUCCESS;
}

#endif /* #if MAC_AX_USB_SUPPORT */
