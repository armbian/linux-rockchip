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
#include "ftm.h"

#define FWCMD_H2C_FTM_INFO_PKTID_SH 0
#define FWCMD_H2C_FTM_INFO_PKTID_MSK 0xff
#define FWCMD_H2C_FTM_INFO_RSP_CH_SH 8
#define FWCMD_H2C_FTM_INFO_RSP_CH_MSK 0xff
#define FWCMD_H2C_FTM_INFO_TSF_TIMER_OFFSET_SH 16
#define FWCMD_H2C_FTM_INFO_TSF_TIMER_OFFSET_MSK 0xff
#define FWCMD_H2C_FTM_INFO_ASAP_SH 24
#define FWCMD_H2C_FTM_INFO_ASAP_MSK 0xff

u32 mac_update_ftm_info(struct mac_ax_adapter *adapter,
			struct fwcmd_ftm_info *info)
{
	u32 ret = 0;
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_ftm_info *tbl;

	/*h2c access*/
	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_ftm_info));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	tbl = (struct fwcmd_ftm_info *)buf;

	tbl->dword0 = info->dword0;

	if (adapter->sm.fwdl == MAC_AX_FWDL_INIT_RDY) {
		ret = h2c_pkt_set_hdr(adapter, h2cb,
				      FWCMD_TYPE_H2C,
				      FWCMD_H2C_CAT_MAC,
				      FWCMD_H2C_CL_SEC_CAM,
				      FWCMD_H2C_FUNC_SECCAM_FTM,
				      0,
				      0);

		if (ret != MACSUCCESS)
			goto fail;

		// return MACSUCCESS if h2c aggregation is enabled and enqueued successfully.
		// H2C shall be sent by mac_h2c_agg_tx.
		ret = h2c_agg_enqueue(adapter, h2cb);
		if (ret == MACSUCCESS)
			return MACSUCCESS;

		ret = h2c_pkt_build_txd(adapter, h2cb);
		if (ret != MACSUCCESS)
			goto fail;

		#if MAC_AX_PHL_H2C
		ret = PLTFM_TX(h2cb);
		#else
		ret = PLTFM_TX(h2cb->data, h2cb->len);
		#endif
		if (ret != MACSUCCESS)
			goto fail;

		h2cb_free(adapter, h2cb);
		return MACSUCCESS;
fail:
		h2cb_free(adapter, h2cb);
	} else {
		return MACNOFW;
	}

	return ret;
}

u32 fill_ftm_para(struct mac_ax_adapter *adapter,
		  struct mac_ax_ftm_para *ftm_info,
		  struct fwcmd_ftm_info *ftm_fw_info)
{
	ftm_fw_info->dword0 =
	cpu_to_le32(SET_WORD(ftm_info->pktid, FWCMD_H2C_FTM_INFO_PKTID) |
		    SET_WORD(ftm_info->rsp_ch, FWCMD_H2C_FTM_INFO_RSP_CH) |
		    SET_WORD(ftm_info->tsf_timer_offset, FWCMD_H2C_FTM_INFO_TSF_TIMER_OFFSET) |
		    SET_WORD(ftm_info->asap, FWCMD_H2C_FTM_INFO_ASAP));

	return MACSUCCESS;
}

u32 mac_ista_ftm_proc(struct mac_ax_adapter *adapter,
		      struct mac_ax_ftm_para *ftmr)
{
	u32 ftm_fw_info = 0, ret = 0;

	fill_ftm_para(adapter, ftmr,
		      (struct fwcmd_ftm_info *)(&ftm_fw_info));

	ret = (u8)mac_update_ftm_info(adapter,
				      (struct fwcmd_ftm_info *)(&ftm_fw_info));
	if (ret != MACSUCCESS)
		return ret;

	return MACSUCCESS;
}

