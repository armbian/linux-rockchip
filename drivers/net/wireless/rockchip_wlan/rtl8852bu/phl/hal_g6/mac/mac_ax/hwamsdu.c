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

#include "hwamsdu.h"

#if MAC_AX_FW_REG_OFLD
u32 mac_enable_cut_hwamsdu(struct mac_ax_adapter *adapter,
			   u8 enable,
			   u8 low_th,
			   u16 high_th,
			   enum mac_ax_ex_shift aligned)
{
	u32 ret = 0;
	u8 *buf;
#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
#else
	struct h2c_buf *h2cb;
#endif
	struct mac_ax_en_amsdu_cut *content;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		if (is_cv(adapter, CBV))
			return MACNOTSUP;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (is_cv(adapter, CAV))
			return MACNOTSUP;
	}

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct mac_ax_en_amsdu_cut));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	content = (struct mac_ax_en_amsdu_cut *)buf;
	content->enable = enable;
	content->low_th = low_th;
	content->high_th = high_th;
	content->aligned = aligned;

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_FW_OFLD,
			      FWCMD_H2C_FUNC_AMSDU_CUT_REG,
			      0,
			      1);

	if (ret)
		goto fail;

	ret = h2c_pkt_build_txd(adapter, h2cb);
	if (ret)
		goto fail;

#if MAC_AX_PHL_H2C
	ret = PLTFM_TX(h2cb);
#else
	ret = PLTFM_TX(h2cb->data, h2cb->len);
#endif
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

u32 mac_enable_hwmasdu(struct mac_ax_adapter *adapter,
		       u8 enable,
		       enum mac_ax_amsdu_pkt_num max_num,
		       u8 en_single_amsdu,
		       u8 en_last_amsdu_padding)

{
	u32 ret = 0;
	u8 *buf;
#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
#else
	struct h2c_buf *h2cb;
#endif
	struct mac_ax_en_hwamsdu *content;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		if (is_cv(adapter, CBV))
			return MACNOTSUP;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (is_cv(adapter, CAV))
			return MACNOTSUP;
	}

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct mac_ax_en_hwamsdu));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	content = (struct mac_ax_en_hwamsdu *)buf;
	content->enable = enable;
	content->max_num = max_num;
	content->en_single_amsdu = en_single_amsdu;
	content->en_last_amsdu_padding = en_last_amsdu_padding;

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_FW_OFLD,
			      FWCMD_H2C_FUNC_HWAMSDU_REG,
			      0,
			      1);

	if (ret)
		goto fail;

	ret = h2c_pkt_build_txd(adapter, h2cb);
	if (ret)
		goto fail;

#if MAC_AX_PHL_H2C
	ret = PLTFM_TX(h2cb);
#else
	ret = PLTFM_TX(h2cb->data, h2cb->len);
#endif
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}
#else
u32 mac_enable_cut_hwamsdu(struct mac_ax_adapter *adapter,
			   u8 enable,
			   u8 low_th,
			   u16 high_th,
			   enum mac_ax_ex_shift aligned)
{
	//cut AMSDU
	u32 val;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		if (is_cv(adapter, CBV))
			return MACNOTSUP;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (is_cv(adapter, CAV))
			return MACNOTSUP;
	}

	if (aligned > MAC_AX_BYTE_ALIGNED_8)
		return MACNOITEM;
	val = MAC_REG_R32(R_AX_CUT_AMSDU_CTRL);
	val = SET_CLR_WORD(val, low_th, B_AX_BIT_CUT_AMSDU_CHKLEN_L_TH);
	val = (SET_CLR_WORD(val, high_th, B_AX_BIT_CUT_AMSDU_CHKLEN_H_TH) |
	       B_AX_BIT_CUT_AMSDU_CHKLEN_EN |
	       B_AX_BIT_EN_CUT_AMSDU);
	if (!enable)
		val &= ~B_AX_BIT_EN_CUT_AMSDU;

	MAC_REG_W32(R_AX_CUT_AMSDU_CTRL, val);

	//extra shift
	val = 0;
	val = (SET_CLR_WORD(val, aligned, B_AX_EXTRA_SHIFT));
	MAC_REG_W32(R_AX_CUT_AMSDU_CTRL_2, val);

	return MACSUCCESS;
}

u32 mac_enable_hwmasdu(struct mac_ax_adapter *adapter,
		       u8 enable,
		       enum mac_ax_amsdu_pkt_num max_num,
		       u8 en_single_amsdu,
		       u8 en_last_amsdu_padding)
{
	u32 val;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		if (is_cv(adapter, CBV))
			return MACNOTSUP;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		if (is_cv(adapter, CAV))
			return MACNOTSUP;
	}

	if (max_num >= MAC_AX_AMSDU_AGG_NUM_MAX)
		return MACNOITEM;

	//HW AMSDU register
	val = MAC_REG_R32(R_AX_HWAMSDU_CTRL);
	val = (SET_CLR_WORD(val, max_num, B_AX_MAX_AMSDU_NUM) |
	       B_AX_HWAMSDU_EN);
	if (!enable)
		val &= ~B_AX_HWAMSDU_EN;
	MAC_REG_W32(R_AX_HWAMSDU_CTRL, val);

	MAC_REG_W32(R_AX_HWAMSDU_CTRL, (MAC_REG_R32(R_AX_HWAMSDU_CTRL) &
		    (~B_AX_SINGLE_AMSDU)) |
		    (en_single_amsdu ? B_AX_SINGLE_AMSDU : 0));

	MAC_REG_W32(R_AX_DMAC_TABLE_CTRL, (MAC_REG_R32(R_AX_DMAC_TABLE_CTRL) &
		    (~B_AX_HWAMSDU_PADDING_MODE)) |
		    (en_last_amsdu_padding ? B_AX_HWAMSDU_PADDING_MODE : 0));

	return MACSUCCESS;
}
#endif
