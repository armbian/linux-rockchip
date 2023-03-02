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
#include "hdr_conv.h"

#if MAC_AX_FW_REG_OFLD
u32 mac_hdr_conv(struct mac_ax_adapter *adapter, u8 en_hdr_conv)
{
			u32 ret = 0;
			u8 *buf;
#if MAC_AX_PHL_H2C
			struct rtw_h2c_pkt *h2cb;
#else
			struct h2c_buf *h2cb;
#endif
			struct mac_ax_en_hdr_conv *content;

			h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
			if (!h2cb)
				return MACNPTR;

			buf = h2cb_put(h2cb, sizeof(struct mac_ax_en_hdr_conv));
			if (!buf) {
				ret = MACNOBUF;
				goto fail;
			}

			content = (struct mac_ax_en_hdr_conv *)buf;
			content->enable = en_hdr_conv;

			ret = h2c_pkt_set_hdr(adapter, h2cb,
					      FWCMD_TYPE_H2C,
					      FWCMD_H2C_CAT_MAC,
					      FWCMD_H2C_CL_FW_OFLD,
					      FWCMD_H2C_FUNC_EN_MAC_HDR_CONV,
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
u32 mac_hdr_conv(struct mac_ax_adapter *adapter, u8 en_hdr_conv)
{
	u32 val;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (en_hdr_conv) {
		val = MAC_REG_R32(R_AX_HDR_SHCUT_SETTING);
		val |= (B_AX_MAC_MPDU_PROC_EN |
			B_AX_SHCUT_LLC_WR_LOCK |
			B_AX_SHCUT_PARSE_DASA);
		MAC_REG_W32(R_AX_HDR_SHCUT_SETTING, val);
	} else {
		val = MAC_REG_R32(R_AX_HDR_SHCUT_SETTING);
		val &= (~(B_AX_MAC_MPDU_PROC_EN |
			  B_AX_SHCUT_LLC_WR_LOCK |
			  B_AX_SHCUT_PARSE_DASA));
		MAC_REG_W32(R_AX_HDR_SHCUT_SETTING, val);
	}

	return MACSUCCESS;
}
#endif

