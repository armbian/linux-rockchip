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
#include "hw_seq.h"

#if MAC_AX_FW_REG_OFLD
u32 mac_set_hwseq_reg(struct mac_ax_adapter *adapter,
		      u8 idx,
		      u16 val)
{
	u32 ret = 0;
	u8 *buf;
#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
#else
	struct h2c_buf *h2cb;
#endif
	struct mac_ax_set_hwseq_reg *content;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct mac_ax_set_hwseq_reg));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	content = (struct mac_ax_set_hwseq_reg *)buf;

	content->reg_idx = idx;
	content->seq_val = val;

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_FW_OFLD,
			      FWCMD_H2C_FUNC_SET_HWSEQ_REG,
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
u32 mac_set_hwseq_reg(struct mac_ax_adapter *adapter,
		      u8 idx,
		      u16 val)
{
	u32 reg_val;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	switch (idx) {
	case R_AX_HW_SEQ_0:
		reg_val = MAC_REG_R32(R_AX_HW_SEQ_0_1);
		reg_val &= ~((u32)B_AX_HW_SEQ0_MSK << B_AX_HW_SEQ0_SH);
		reg_val |= (val << B_AX_HW_SEQ0_SH);
		MAC_REG_W32(R_AX_HW_SEQ_0_1, reg_val);
		break;
	case R_AX_HW_SEQ_1:
		reg_val = MAC_REG_R32(R_AX_HW_SEQ_0_1);
		reg_val &= ~((u32)B_AX_HW_SEQ1_MSK << B_AX_HW_SEQ1_SH);
		reg_val |= (val << B_AX_HW_SEQ1_SH);
		MAC_REG_W32(R_AX_HW_SEQ_0_1, reg_val);
		break;
	case R_AX_HW_SEQ_2:
		reg_val = MAC_REG_R32(R_AX_HW_SEQ_2_3);
		reg_val &= ~((u32)B_AX_HW_SEQ2_MSK << B_AX_HW_SEQ2_SH);
		reg_val |= (val << B_AX_HW_SEQ2_SH);
		MAC_REG_W32(R_AX_HW_SEQ_2_3, reg_val);
		break;
	case R_AX_HW_SEQ_3:
		reg_val = MAC_REG_R32(R_AX_HW_SEQ_2_3);
		reg_val &= ~((u32)B_AX_HW_SEQ3_MSK << B_AX_HW_SEQ3_SH);
		reg_val |= (val << B_AX_HW_SEQ3_SH);
		MAC_REG_W32(R_AX_HW_SEQ_2_3, reg_val);
		break;
	default:
		return MACNOITEM;
	}

	return MACSUCCESS;
}
#endif
