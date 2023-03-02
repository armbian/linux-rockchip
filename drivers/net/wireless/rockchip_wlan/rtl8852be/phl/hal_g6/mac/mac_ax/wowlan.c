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

#include "wowlan.h"

static u32 wow_bk_status[4] = {0};
static u32 tgt_ind_orig;
static u32 frm_tgt_ind_orig;
static u32 wol_pattern_orig;
static u32 wol_uc_orig;
static u32 wol_magic_orig;
static u8 nlo_enable_record;

static u32 send_h2c_keep_alive(struct mac_ax_adapter *adapter,
			       struct keep_alive *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_keep_alive *fwcmd_kalive;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_keep_alive));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_kalive = (struct fwcmd_keep_alive *)buf;
	fwcmd_kalive->dword0 =
	cpu_to_le32((parm->keepalive_en ?
		     FWCMD_H2C_KEEP_ALIVE_KEEPALIVE_EN : 0) |
		SET_WORD(parm->packet_id, FWCMD_H2C_KEEP_ALIVE_PACKET_ID) |
		SET_WORD(parm->period, FWCMD_H2C_KEEP_ALIVE_PERIOD) |
		SET_WORD(parm->mac_id, FWCMD_H2C_KEEP_ALIVE_MAC_ID));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_KEEP_ALIVE,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_disconnect_detect(struct mac_ax_adapter *adapter,
				      struct disconnect_detect *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_disconnect_detect *fwcmd_disconnect_det;
	u32 ret = 0;
	u32 tmp;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_disconnect_detect));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_disconnect_det = (struct fwcmd_disconnect_detect *)buf;
	fwcmd_disconnect_det->dword0 =
	cpu_to_le32((parm->disconnect_detect_en ?
		     FWCMD_H2C_DISCONNECT_DETECT_DISCONNECT_DETECT_EN : 0) |
	(parm->tryok_bcnfail_count_en ?
	 FWCMD_H2C_DISCONNECT_DETECT_TRYOK_BCNFAIL_COUNT_EN : 0) |
	(parm->disconnect_en ? FWCMD_H2C_DISCONNECT_DETECT_DISCONNECT_EN : 0) |
	SET_WORD(parm->mac_id, FWCMD_H2C_DISCONNECT_DETECT_MAC_ID) |
	SET_WORD(parm->check_period, FWCMD_H2C_DISCONNECT_DETECT_CHECK_PERIOD) |
	SET_WORD(parm->try_pkt_count,
		 FWCMD_H2C_DISCONNECT_DETECT_TRY_PKT_COUNT));

	tmp = SET_WORD(parm->tryok_bcnfail_count_limit,
		       FWCMD_H2C_DISCONNECT_DETECT_TRYOK_BCNFAIL_COUNT_LIMIT);
	fwcmd_disconnect_det->dword1 = cpu_to_le32(tmp);

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_DISCONNECT_DETECT,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_wow_global(struct mac_ax_adapter *adapter,
			       struct wow_global *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_wow_global *fwcmd_wow_glo;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_DATA);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_wow_global)
		       + sizeof(struct mac_ax_remotectrl_info_parm_) - 4);
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_wow_glo = (struct fwcmd_wow_global *)buf;
	fwcmd_wow_glo->dword0 =
	cpu_to_le32((parm->wow_en ? FWCMD_H2C_WOW_GLOBAL_WOW_EN : 0) |
		(parm->drop_all_pkt ? FWCMD_H2C_WOW_GLOBAL_DROP_ALL_PKT : 0) |
		(parm->rx_parse_after_wake ?
		 FWCMD_H2C_WOW_GLOBAL_RX_PARSE_AFTER_WAKE : 0) |
		SET_WORD(parm->mac_id, FWCMD_H2C_WOW_GLOBAL_MAC_ID) |
		SET_WORD(parm->pairwise_sec_algo,
			 FWCMD_H2C_WOW_GLOBAL_PAIRWISE_SEC_ALGO) |
		SET_WORD(parm->group_sec_algo,
			 FWCMD_H2C_WOW_GLOBAL_GROUP_SEC_ALGO));

	//fwcmd_wow_glo->dword1 =
	//	cpu_to_le32(parm->remotectrl_info_content);
	PLTFM_MEMCPY(&fwcmd_wow_glo->dword1, &parm->remotectrl_info_content,
		     sizeof(struct mac_ax_remotectrl_info_parm_));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_WOW_GLOBAL,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_gtk_ofld(struct mac_ax_adapter *adapter,
			     struct gtk_ofld *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_gtk_ofld *fwcmd_gtk_ofl;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_DATA);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_gtk_ofld)
		       + sizeof(struct mac_ax_gtk_info_parm_) - 4);
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_gtk_ofl = (struct fwcmd_gtk_ofld *)buf;
	fwcmd_gtk_ofl->dword0 =
	cpu_to_le32((parm->gtk_en ? FWCMD_H2C_GTK_OFLD_GTK_EN : 0) |
		(parm->tkip_en ? FWCMD_H2C_GTK_OFLD_TKIP_EN : 0) |
		(parm->ieee80211w_en ? FWCMD_H2C_GTK_OFLD_IEEE80211W_EN : 0) |
		(parm->pairwise_wakeup ?
		 FWCMD_H2C_GTK_OFLD_PAIRWISE_WAKEUP : 0) |
		SET_WORD(parm->mac_id, FWCMD_H2C_GTK_OFLD_MAC_ID) |
		SET_WORD(parm->gtk_rsp_id, FWCMD_H2C_GTK_OFLD_GTK_RSP_ID));

	fwcmd_gtk_ofl->dword1 =
	cpu_to_le32(SET_WORD(parm->pmf_sa_query_id, FWCMD_H2C_GTK_OFLD_PMF_SA_QUERY_ID) |
		    SET_WORD(parm->bip_sec_algo, FWCMD_H2C_GTK_OFLD_PMF_BIP_SEC_ALGO) |
		    SET_WORD(parm->algo_akm_suit, FWCMD_H2C_GTK_OFLD_ALGO_AKM_SUIT));

	PLTFM_MEMCPY(&fwcmd_gtk_ofl->dword2, &parm->gtk_info_content,
		     sizeof(struct mac_ax_gtk_info_parm_));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_GTK_OFLD,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_arp_ofld(struct mac_ax_adapter *adapter,
			     struct arp_ofld *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_arp_ofld *fwcmd_arp_ofl;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_arp_ofld));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_arp_ofl = (struct fwcmd_arp_ofld *)buf;
	fwcmd_arp_ofl->dword0 =
	cpu_to_le32((parm->arp_en ? FWCMD_H2C_ARP_OFLD_ARP_EN : 0) |
		(parm->arp_action ? FWCMD_H2C_ARP_OFLD_ARP_ACTION : 0) |
		SET_WORD(parm->mac_id, FWCMD_H2C_ARP_OFLD_MAC_ID) |
		SET_WORD(parm->arp_rsp_id, FWCMD_H2C_ARP_OFLD_ARP_RSP_ID));

	fwcmd_arp_ofl->dword1 =
		cpu_to_le32(parm->arp_info_content);

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_ARP_OFLD,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_ndp_ofld(struct mac_ax_adapter *adapter,
			     struct ndp_ofld *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_ndp_ofld *fwcmd_ndp_ofl;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_DATA);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_ndp_ofld) + 2 *
		       sizeof(struct mac_ax_ndp_info_parm_) - 4);
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_ndp_ofl = (struct fwcmd_ndp_ofld *)buf;
	fwcmd_ndp_ofl->dword0 =
	cpu_to_le32((parm->ndp_en ? FWCMD_H2C_NDP_OFLD_NDP_EN : 0) |
		    SET_WORD(parm->mac_id, FWCMD_H2C_NDP_OFLD_MAC_ID) |
		    SET_WORD(parm->na_id, FWCMD_H2C_NDP_OFLD_NA_ID));

	PLTFM_MEMCPY(&fwcmd_ndp_ofl->dword1, &parm->ndp_info_content, 2 *
		     sizeof(struct mac_ax_ndp_info_parm_));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_NDP_OFLD,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_realwow(struct mac_ax_adapter *adapter,
			    struct realwow *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_realwow *fwcmd_realw;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_realwow) +
		       sizeof(struct mac_ax_realwowv2_info_parm_) - 4);
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_realw = (struct fwcmd_realwow *)buf;
	fwcmd_realw->dword0 =
	cpu_to_le32((parm->realwow_en ? FWCMD_H2C_REALWOW_REALWOW_EN : 0) |
		(parm->auto_wakeup ? FWCMD_H2C_REALWOW_AUTO_WAKEUP : 0) |
		SET_WORD(parm->mac_id, FWCMD_H2C_REALWOW_MAC_ID));

	fwcmd_realw->dword1 =
	cpu_to_le32(SET_WORD(parm->keepalive_id,
			     FWCMD_H2C_REALWOW_KEEPALIVE_ID) |
	SET_WORD(parm->wakeup_pattern_id, FWCMD_H2C_REALWOW_WAKEUP_PATTERN_ID) |
	SET_WORD(parm->ack_pattern_id, FWCMD_H2C_REALWOW_ACK_PATTERN_ID));

	fwcmd_realw->dword2 =
		cpu_to_le32(parm->realwow_info_content);

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_REALWOW,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_nlo(struct mac_ax_adapter *adapter,
			struct nlo *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	u32 ret = 0;
	u32 *h2cb_u32;
	u32 *nlo_parm_u32;
	u8 sh;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_LONG_DATA);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_nlo) +
		       sizeof(struct mac_ax_nlo_networklist_parm_) - 4);
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	nlo_parm_u32 = &parm->nlo_networklistinfo_content;
	h2cb_u32 = (u32 *)buf;
	*h2cb_u32 = cpu_to_le32((parm->nlo_en ? FWCMD_H2C_NLO_NLO_EN : 0) |
				(parm->nlo_32k_en ? FWCMD_H2C_NLO_NLO_32K_EN : 0) |
				(parm->ignore_cipher_type ? FWCMD_H2C_NLO_IGNORE_CIPHER_TYPE : 0) |
				SET_WORD(parm->mac_id, FWCMD_H2C_NLO_MAC_ID));
	h2cb_u32++;

	*h2cb_u32 = cpu_to_le32(*nlo_parm_u32);
	h2cb_u32++;
	nlo_parm_u32 = parm->nlo_networklistinfo_more;

	for (sh = 0; sh < (sizeof(struct mac_ax_nlo_networklist_parm_) / 4 - 1); sh++)
		*(h2cb_u32 + sh) = cpu_to_le32(*(nlo_parm_u32 + sh));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_NLO,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_wakeup_ctrl(struct mac_ax_adapter *adapter,
				struct wakeup_ctrl *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_wakeup_ctrl *fwcmd_wakeup_ctr;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_wakeup_ctrl));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_wakeup_ctr = (struct fwcmd_wakeup_ctrl *)buf;
	fwcmd_wakeup_ctr->dword0 =
	cpu_to_le32((parm->pattern_match_en ?
		     FWCMD_H2C_WAKEUP_CTRL_PATTERN_MATCH_EN : 0) |
	(parm->magic_en ? FWCMD_H2C_WAKEUP_CTRL_MAGIC_EN : 0) |
	(parm->hw_unicast_en ? FWCMD_H2C_WAKEUP_CTRL_HW_UNICAST_EN : 0) |
	(parm->fw_unicast_en ? FWCMD_H2C_WAKEUP_CTRL_FW_UNICAST_EN : 0) |
	(parm->deauth_wakeup ? FWCMD_H2C_WAKEUP_CTRL_DEAUTH_WAKEUP : 0) |
	(parm->rekey_wakeup ? FWCMD_H2C_WAKEUP_CTRL_REKEY_WAKEUP : 0) |
	(parm->eap_wakeup ? FWCMD_H2C_WAKEUP_CTRL_EAP_WAKEUP : 0) |
	(parm->all_data_wakeup ? FWCMD_H2C_WAKEUP_CTRL_ALL_DATA_WAKEUP : 0) |
	SET_WORD(parm->mac_id, FWCMD_H2C_WAKEUP_CTRL_MAC_ID));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_WAKEUP_CTRL,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_negative_pattern(struct mac_ax_adapter *adapter,
				     struct negative_pattern *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_negative_pattern *fwcmd_negative_patter;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_negative_pattern));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_negative_patter = (struct fwcmd_negative_pattern *)buf;

	fwcmd_negative_patter->dword0 =
	cpu_to_le32((parm->negative_pattern_en ?
			FWCMD_H2C_NEGATIVE_PATTERN_NEGATIVE_PATTERN_EN : 0) |
	SET_WORD(parm->pattern_count,
		 FWCMD_H2C_NEGATIVE_PATTERN_PATTERN_COUNT) |
	SET_WORD(parm->mac_id, FWCMD_H2C_NEGATIVE_PATTERN_MAC_ID));

	fwcmd_negative_patter->dword1 =
		cpu_to_le32(parm->pattern_content);

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_NEGATIVE_PATTERN,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

u32 mac_cfg_dev2hst_gpio(struct mac_ax_adapter *adapter,
			 struct mac_ax_dev2hst_gpio_info *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_dev2hst_gpio *fwcmd_dev2hst_gpi;
	u32 ret = 0;

	if (parm->gpio_output_input == MAC_AX_DEV2HST_GPIO_INPUT &&
	    parm->toggle_pulse == MAC_AX_DEV2HST_PULSE) {
		PLTFM_MSG_ERR("pulse mode not supported under input mode");
		return MACNOITEM;
	}
	if (parm->gpio_num > MAC_AX_GPIO15) {
		PLTFM_MSG_ERR("gpio num > 15");
		return MACNOITEM;
	}
	if (parm->toggle_pulse == MAC_AX_DEV2HST_PULSE) {
		if (parm->gpio_pulse_dura == 0) {
			PLTFM_MSG_ERR("gpio pulse duration cant be 0");
			return MACNOITEM;
		}
		if (parm->gpio_pulse_period <= parm->gpio_pulse_dura) {
			PLTFM_MSG_ERR("gpio pulse period can less than duration");
			return MACNOITEM;
		}
		if (!parm->gpio_pulse_nonstop && parm->gpio_pulse_count == 0) {
			PLTFM_MSG_ERR("gpio pulse count cant be 0");
			return MACNOITEM;
		}
	}

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_dev2hst_gpio));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}
	fwcmd_dev2hst_gpi = (struct fwcmd_dev2hst_gpio *)buf;
	fwcmd_dev2hst_gpi->dword0 =
	cpu_to_le32((parm->dev2hst_gpio_en ? FWCMD_H2C_DEV2HST_GPIO_DEV2HST_GPIO_EN : 0) |
		    (parm->disable_inband ? FWCMD_H2C_DEV2HST_GPIO_DISABLE_INBAND : 0) |
		    (parm->gpio_output_input ? FWCMD_H2C_DEV2HST_GPIO_GPIO_OUTPUT_INPUT : 0) |
		    (parm->gpio_active ? FWCMD_H2C_DEV2HST_GPIO_GPIO_ACTIVE : 0) |
		    (parm->toggle_pulse ? FWCMD_H2C_DEV2HST_GPIO_TOGGLE_PULSE : 0) |
		    (parm->data_pin_wakeup ? FWCMD_H2C_DEV2HST_GPIO_DATA_PIN_WAKEUP : 0) |
		    (parm->data_pin_wakeup ? FWCMD_H2C_DEV2HST_GPIO_DATA_PIN_WAKEUP : 0) |
		    (parm->gpio_pulse_nonstop ? FWCMD_H2C_DEV2HST_GPIO_GPIO_PULSE_NONSTOP : 0) |
		    (parm->gpio_time_unit ? FWCMD_H2C_DEV2HST_GPIO_GPIO_TIME_UNIT : 0) |
		    SET_WORD(parm->gpio_num, FWCMD_H2C_DEV2HST_GPIO_GPIO_NUM) |
		    SET_WORD(parm->gpio_pulse_dura, FWCMD_H2C_DEV2HST_GPIO_GPIO_PULSE_DURATION) |
		    SET_WORD(parm->gpio_pulse_period, FWCMD_H2C_DEV2HST_GPIO_GPIO_PULSE_PERIOD));

	fwcmd_dev2hst_gpi->dword1 =
	cpu_to_le32(SET_WORD(parm->gpio_pulse_count,
			     FWCMD_H2C_DEV2HST_GPIO_GPIO_PULSE_COUNT));

	fwcmd_dev2hst_gpi->dword2 =
	cpu_to_le32(SET_WORD(parm->customer_id,
			     FWCMD_H2C_DEV2HST_GPIO_CUSTOMER_ID));

	fwcmd_dev2hst_gpi->dword3 =
	cpu_to_le32((parm->rsn_a_en ? FWCMD_H2C_DEV2HST_GPIO_RSN_A_EN : 0) |
		    (parm->rsn_a_toggle_pulse ? FWCMD_H2C_DEV2HST_GPIO_RSN_A_TOGGLE_PULSE : 0) |
		    (parm->rsn_a_pulse_nonstop ? FWCMD_H2C_DEV2HST_GPIO_RSN_A_PULSE_NONSTOP : 0) |
		    (parm->rsn_a_time_unit ? FWCMD_H2C_DEV2HST_GPIO_RSN_A_TIME_UNIT : 0));

	fwcmd_dev2hst_gpi->dword4 =
	cpu_to_le32(SET_WORD(parm->rsn_a, FWCMD_H2C_DEV2HST_GPIO_RSN_A) |
		    SET_WORD(parm->rsn_a_pulse_duration,
			     FWCMD_H2C_DEV2HST_GPIO_RSN_A_PULSE_DURATION) |
		    SET_WORD(parm->rsn_a_pulse_period, FWCMD_H2C_DEV2HST_GPIO_RSN_A_PULSE_PERIOD) |
		    SET_WORD(parm->rsn_a_pulse_count, FWCMD_H2C_DEV2HST_GPIO_RSN_A_PULSE_COUNT));

	fwcmd_dev2hst_gpi->dword5 =
	cpu_to_le32((parm->rsn_b_en ? FWCMD_H2C_DEV2HST_GPIO_RSN_B_EN : 0) |
		    (parm->rsn_b_toggle_pulse ? FWCMD_H2C_DEV2HST_GPIO_RSN_B_TOGGLE_PULSE : 0) |
		    (parm->rsn_b_pulse_nonstop ? FWCMD_H2C_DEV2HST_GPIO_RSN_B_PULSE_NONSTOP : 0) |
		    (parm->rsn_b_time_unit ? FWCMD_H2C_DEV2HST_GPIO_RSN_B_TIME_UNIT : 0));

	fwcmd_dev2hst_gpi->dword6 =
	cpu_to_le32(SET_WORD(parm->rsn_b, FWCMD_H2C_DEV2HST_GPIO_RSN_B) |
		    SET_WORD(parm->rsn_b_pulse_duration,
			     FWCMD_H2C_DEV2HST_GPIO_RSN_B_PULSE_DURATION) |
		    SET_WORD(parm->rsn_b_pulse_period, FWCMD_H2C_DEV2HST_GPIO_RSN_B_PULSE_PERIOD) |
		    SET_WORD(parm->rsn_b_pulse_count, FWCMD_H2C_DEV2HST_GPIO_RSN_B_PULSE_COUNT));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_DEV2HST_GPIO,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);
	PLTFM_MSG_TRACE("ok");
	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);
	return ret;
}

static u32 send_h2c_uphy_ctrl(struct mac_ax_adapter *adapter,
			      struct uphy_ctrl *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_uphy_ctrl *fwcmd_uphy_ctr;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_uphy_ctrl));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_uphy_ctr = (struct fwcmd_uphy_ctrl *)buf;
	fwcmd_uphy_ctr->dword0 =
	cpu_to_le32((parm->disable_uphy ?
			FWCMD_H2C_UPHY_CTRL_DISABLE_UPHY : 0) |
	SET_WORD(parm->handshake_mode, FWCMD_H2C_UPHY_CTRL_HANDSHAKE_MODE) |
	(parm->rise_hst2dev_dis_uphy ? FWCMD_H2C_UPHY_CTRL_RISE_HST2DEV_DIS_UPHY
									: 0) |
	(parm->uphy_dis_delay_unit ? FWCMD_H2C_UPHY_CTRL_UPHY_DIS_DELAY_UNIT
									: 0) |
	(parm->pdn_as_uphy_dis ? FWCMD_H2C_UPHY_CTRL_PDN_AS_UPHY_DIS : 0) |
	(parm->pdn_to_enable_uphy ? FWCMD_H2C_UPHY_CTRL_PDN_TO_ENABLE_UPHY
									: 0) |
	SET_WORD(parm->hst2dev_gpio_num, FWCMD_H2C_UPHY_CTRL_HST2DEV_GPIO_NUM) |
	SET_WORD(parm->uphy_dis_delay_count,
		 FWCMD_H2C_UPHY_CTRL_UPHY_DIS_DELAY_COUNT));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_UPHY_CTRL,
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

static u32 send_h2c_wowcam_upd(struct mac_ax_adapter *adapter,
			       struct wowcam_upd *parm)
{
	u8 *buf;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	struct fwcmd_wow_cam_upd *fwcmd_wowcam_upd;
	u32 ret = 0;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_CMD);
	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_wow_cam_upd));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_wowcam_upd = (struct fwcmd_wow_cam_upd *)buf;
	fwcmd_wowcam_upd->dword0 =
		cpu_to_le32((parm->r_w ? FWCMD_H2C_WOW_CAM_UPD_R_W : 0) |
		SET_WORD(parm->idx, FWCMD_H2C_WOW_CAM_UPD_IDX));

	fwcmd_wowcam_upd->dword1 =
		cpu_to_le32(parm->wkfm1);

	fwcmd_wowcam_upd->dword2 =
		cpu_to_le32(parm->wkfm2);

	fwcmd_wowcam_upd->dword3 =
		cpu_to_le32(parm->wkfm3);

	fwcmd_wowcam_upd->dword4 =
		cpu_to_le32(parm->wkfm4);

	fwcmd_wowcam_upd->dword5 =
		cpu_to_le32(SET_WORD(parm->crc, FWCMD_H2C_WOW_CAM_UPD_CRC) |
		(parm->negative_pattern_match ? FWCMD_H2C_WOW_CAM_UPD_NEGATIVE_PATTERN_MATCH : 0) |
		(parm->skip_mac_hdr ? FWCMD_H2C_WOW_CAM_UPD_SKIP_MAC_HDR : 0) |
		(parm->uc ? FWCMD_H2C_WOW_CAM_UPD_UC : 0) |
		(parm->mc ? FWCMD_H2C_WOW_CAM_UPD_MC : 0) |
		(parm->bc ? FWCMD_H2C_WOW_CAM_UPD_BC : 0) |
		(parm->valid ? FWCMD_H2C_WOW_CAM_UPD_VALID : 0));

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C,
			      FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW,
			      FWCMD_H2C_FUNC_WOW_CAM_UPD,
			      0,
			      0);
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
	if (ret)
		goto fail;

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

u32 mac_cfg_wow_wake(struct mac_ax_adapter *adapter,
		     u8 macid,
		     struct mac_ax_wow_wake_info *info,
		     struct mac_ax_remotectrl_info_parm_ *content)
{
	u32 ret = 0;
	struct wow_global parm1;
	struct wakeup_ctrl parm2;
	struct mac_role_tbl *role;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	parm2.pattern_match_en = info->pattern_match_en;
	parm2.magic_en = info->magic_en;
	parm2.hw_unicast_en = info->hw_unicast_en;
	parm2.fw_unicast_en = info->fw_unicast_en;
	parm2.deauth_wakeup = info->deauth_wakeup;
	parm2.rekey_wakeup = info->rekey_wakeup;
	parm2.eap_wakeup = info->eap_wakeup;
	parm2.all_data_wakeup = info->all_data_wakeup;
	parm2.mac_id = macid;
	ret = send_h2c_wakeup_ctrl(adapter, &parm2);
	if (ret) {
		PLTFM_MSG_ERR("send h2c wakeup ctrl failed\n");
		return ret;
	}

	parm1.wow_en = info->wow_en;
	parm1.drop_all_pkt = info->drop_all_pkt;
	parm1.rx_parse_after_wake = info->rx_parse_after_wake;
	parm1.mac_id = macid;
	parm1.pairwise_sec_algo = info->pairwise_sec_algo;
	parm1.group_sec_algo = info->group_sec_algo;
	//parm1.remotectrl_info_content =
	//info->remotectrl_info_content;
	if (content)
		PLTFM_MEMCPY(&parm1.remotectrl_info_content,
			     content,
			     sizeof(struct mac_ax_remotectrl_info_parm_));

	if (info->wow_en) {
		role = mac_role_srch(adapter, macid);
		if (role) {
			tgt_ind_orig = role->info.tgt_ind;
			frm_tgt_ind_orig = role->info.frm_tgt_ind;
			wol_pattern_orig = role->info.wol_pattern;
			wol_uc_orig = role->info.wol_uc;
			wol_magic_orig = role->info.wol_magic;
			wow_bk_status[(macid >> 5)] |= BIT(macid & 0x1F);
			role->info.wol_pattern = (u8)parm2.pattern_match_en;
			role->info.wol_uc = info->hw_unicast_en;
			role->info.wol_magic = info->magic_en;

			ret = mac_change_role(adapter, &role->info);
			if (ret) {
				PLTFM_MSG_ERR("role change failed\n");
				return ret;
			}
		} else {
			PLTFM_MSG_ERR("role search failed\n");
			return MACNOITEM;
		}
	} else {
		if (wow_bk_status[(macid >> 5)] & BIT(macid & 0x1F)) {
			//restore address cam
			role = mac_role_srch(adapter, macid);
			if (role) {
				role->info.tgt_ind = (u8)tgt_ind_orig;
				role->info.frm_tgt_ind = (u8)frm_tgt_ind_orig;
				role->info.wol_pattern = (u8)wol_pattern_orig;
				role->info.wol_uc = (u8)wol_uc_orig;
				role->info.wol_magic = (u8)wol_magic_orig;
				ret = mac_change_role(adapter, &role->info);
				if (ret) {
					PLTFM_MSG_ERR("role change failed\n");
					return ret;
				}
			}
			wow_bk_status[(macid >> 5)] &= ~BIT(macid & 0x1F);
		} else {
			PLTFM_MSG_ERR("role search failed\n");
			return MACNOITEM;
		}
	}

	ret = send_h2c_wow_global(adapter, &parm1);
	if (ret)
		PLTFM_MSG_ERR("set wow global failed\n");

	return ret;
}

u32 mac_cfg_disconnect_det(struct mac_ax_adapter *adapter,
			   u8 macid,
			   struct mac_ax_disconnect_det_info *info)
{
	u32 ret = 0;
	struct disconnect_detect parm;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	parm.disconnect_detect_en = info->disconnect_detect_en;
	parm.tryok_bcnfail_count_en =
	    info->tryok_bcnfail_count_en;
	parm.disconnect_en = info->disconnect_en;
	parm.mac_id = macid;
	parm.check_period = info->check_period;
	parm.try_pkt_count = info->try_pkt_count;
	parm.tryok_bcnfail_count_limit =
	    info->tryok_bcnfail_count_limit;

	ret = send_h2c_disconnect_detect(adapter, &parm);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 mac_cfg_keep_alive(struct mac_ax_adapter *adapter,
		       u8 macid,
		       struct mac_ax_keep_alive_info *info)
{
	u32 ret = 0;
	struct keep_alive parm;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	parm.keepalive_en = info->keepalive_en;
	parm.packet_id = info->packet_id;
	parm.period = info->period;
	parm.mac_id = macid;

	ret = send_h2c_keep_alive(adapter, &parm);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 mac_cfg_gtk_ofld(struct mac_ax_adapter *adapter,
		     u8 macid,
		     struct mac_ax_gtk_ofld_info *info,
		     struct mac_ax_gtk_info_parm_ *content)
{
	u32 ret = 0;
	struct gtk_ofld parm;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	parm.gtk_en = info->gtk_en;
	parm.tkip_en = info->tkip_en;
	parm.ieee80211w_en = info->ieee80211w_en;
	parm.pairwise_wakeup = info->pairwise_wakeup;
	parm.mac_id = macid;
	parm.gtk_rsp_id = info->gtk_rsp_id;
	parm.pmf_sa_query_id = info->pmf_sa_query_id;
	parm.bip_sec_algo = info->bip_sec_algo;
	parm.algo_akm_suit = info->algo_akm_suit;

	if (content)
		PLTFM_MEMCPY(&parm.gtk_info_content, content,
			     sizeof(struct mac_ax_gtk_info_parm_));

	ret = send_h2c_gtk_ofld(adapter, &parm);
	if (ret)
		return ret;
	return MACSUCCESS;
}

u32 mac_cfg_arp_ofld(struct mac_ax_adapter *adapter,
		     u8 macid,
		     struct mac_ax_arp_ofld_info *info,
		     void *parp_info_content)
{
	u32 ret = 0;
	struct arp_ofld parm;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	PLTFM_MEMSET(&parm, 0, sizeof(struct arp_ofld));
	parm.arp_en = info->arp_en;
	parm.arp_action = info->arp_action;
	parm.mac_id = macid;
	parm.arp_rsp_id = info->arp_rsp_id;

	//if (parp_info_content)
	//	PLTFM_MEMCPY(&parm.ndp_info_content, parp_info_content,
	//		     sizeof(struct _arp_info_parm_) * 2);

	ret = send_h2c_arp_ofld(adapter, &parm);
	if (ret)
		return ret;
	return MACSUCCESS;
}

u32 mac_cfg_ndp_ofld(struct mac_ax_adapter *adapter,
		     u8 macid,
		     struct mac_ax_ndp_ofld_info *info,
		     struct mac_ax_ndp_info_parm_ *content)
{
	u32 ret = 0;
	struct ndp_ofld parm;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	PLTFM_MEMSET(&parm, 0, sizeof(struct ndp_ofld));
	parm.ndp_en = info->ndp_en;
	parm.na_id = info->na_id;
	parm.mac_id = macid;

	if (content)
		PLTFM_MEMCPY(&parm.ndp_info_content, content,
			     sizeof(struct mac_ax_ndp_info_parm_) * 2);

	ret = send_h2c_ndp_ofld(adapter, &parm);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 mac_cfg_realwow(struct mac_ax_adapter *adapter,
		    u8 macid,
		    struct mac_ax_realwow_info *info,
		    struct mac_ax_realwowv2_info_parm_ *content)
{
	u32 ret = 0;
	struct realwow parm;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	PLTFM_MEMSET(&parm, 0, sizeof(struct realwow));
	parm.realwow_en = info->realwow_en;
	parm.auto_wakeup = info->auto_wakeup;
	parm.mac_id = macid;
	parm.keepalive_id = info->keepalive_id;
	parm.wakeup_pattern_id = info->wakeup_pattern_id;
	parm.ack_pattern_id = info->ack_pattern_id;
	if (content)
		PLTFM_MEMCPY(&parm.realwow_info_content, content,
			     sizeof(struct mac_ax_realwowv2_info_parm_));

	ret = send_h2c_realwow(adapter, &parm);
	if (ret)
		return ret;
	return MACSUCCESS;
}

u32 mac_cfg_nlo(struct mac_ax_adapter *adapter,
		u8 macid,
		struct mac_ax_nlo_info *info,
		struct mac_ax_nlo_networklist_parm_ *content)
{
	u32 ret = 0;
	struct nlo parm;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	PLTFM_MEMSET(&parm, 0, sizeof(struct nlo));
	parm.nlo_en = info->nlo_en;
	parm.nlo_32k_en = info->nlo_32k_en;
	parm.ignore_cipher_type = info->ignore_cipher_type;
	parm.mac_id = macid;

	if (content)
		PLTFM_MEMCPY(&parm.nlo_networklistinfo_content,
			     content,
			     sizeof(struct mac_ax_nlo_networklist_parm_));

	ret = send_h2c_nlo(adapter, &parm);
	if (ret)
		return ret;
	if (info->nlo_en)
		nlo_enable_record = 1;
	return MACSUCCESS;
}

u32 mac_cfg_uphy_ctrl(struct mac_ax_adapter *adapter,
		      struct mac_ax_uphy_ctrl_info *info)
{
	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	return MACSUCCESS;
}

u32 mac_cfg_wowcam_upd(struct mac_ax_adapter *adapter,
		       struct mac_ax_wowcam_upd_info *info)
{
	u32 ret = 0;
	struct wowcam_upd parm;

	if (adapter->sm.fwdl != MAC_AX_FWDL_INIT_RDY)
		return MACNOFW;

	PLTFM_MEMSET(&parm, 0, sizeof(struct wowcam_upd));
	parm.r_w = info->r_w;
	parm.idx = info->idx;
	parm.wkfm1 = info->wkfm1;
	parm.wkfm2 = info->wkfm2;
	parm.wkfm3 = info->wkfm3;
	parm.wkfm4 = info->wkfm4;
	parm.crc = info->crc;
	parm.negative_pattern_match = info->negative_pattern_match;
	parm.skip_mac_hdr = info->skip_mac_hdr;
	parm.uc = info->uc;
	parm.mc = info->mc;
	parm.bc = info->bc;
	parm.valid = info->valid;

	ret = send_h2c_wowcam_upd(adapter, &parm);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 get_wake_reason(struct mac_ax_adapter *adapter, u8 *wowlan_wake_reason)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
		*wowlan_wake_reason = MAC_REG_R8(R_AX_C2HREG_DATA3_V1 + 3);
	else
		*wowlan_wake_reason = MAC_REG_R8(R_AX_C2HREG_DATA3 + 3);

	return MACSUCCESS;
}

u32 mac_get_wow_wake_rsn(struct mac_ax_adapter *adapter, u8 *wake_rsn,
			 u8 *reset)
{
	u32 ret = MACSUCCESS;

	ret = get_wake_reason(adapter, wake_rsn);
	if (ret != MACSUCCESS)
		return ret;

	switch (*wake_rsn) {
	case MAC_AX_WOW_DMAC_ERROR_OCCURRED:
	case MAC_AX_WOW_EXCEPTION_OCCURRED:
	case MAC_AX_WOW_L0_TO_L1_ERROR_OCCURRED:
	case MAC_AX_WOW_ASSERT_OCCURRED:
	case MAC_AX_WOW_L2_ERROR_OCCURRED:
	case MAC_AX_WOW_WDT_TIMEOUT_WAKE:
		*reset = 1;
		break;
	default:
		*reset = 0;
		break;
	}

	return MACSUCCESS;
}

u32 mac_cfg_wow_sleep(struct mac_ax_adapter *adapter,
		      u8 sleep)
{
	u32 ret;
	u32 val32;
	u8 dbg_page;
	struct mac_ax_phy_rpt_cfg cfg;
	struct mac_ax_ops *mac_ops = adapter_to_mac_ops(adapter);
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	PLTFM_MEMSET(&cfg, 0, sizeof(struct mac_ax_phy_rpt_cfg));
#if MAC_AX_FW_REG_OFLD
	if (adapter->sm.fwdl == MAC_AX_FWDL_INIT_RDY) {
		if (sleep) {
			ret = _patch_redu_rx_qta(adapter);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]patch reduce rx qta %d\n", ret);
				return ret;
			}

			cfg.type = MAC_AX_PPDU_STATUS;
			cfg.en = 0;
			ret = mac_ops->cfg_phy_rpt(adapter, &cfg);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]cfg_phy_rpt failed %d\n", ret);
				return ret;
			}

			ret = MAC_REG_W_OFLD(R_AX_RX_FUNCTION_STOP, B_AX_HDR_RX_STOP, 1, 0);
			if (ret)
				return ret;
			if (nlo_enable_record == 0) {
				ret = MAC_REG_W_OFLD(R_AX_RX_FLTR_OPT, B_AX_SNIFFER_MODE, 0, 0);
			} else {
				ret = MAC_REG_W_OFLD(R_AX_RX_FLTR_OPT, B_AX_SNIFFER_MODE, 1, 0);
				PLTFM_MSG_TRACE("Enable sniffer mode since nlo enable");
				nlo_enable_record = 0;
			}
			if (ret)
				return ret;
			ret = MAC_REG_W32_OFLD(R_AX_ACTION_FWD0, 0x00000000, 0);
			if (ret)
				return ret;
			ret = MAC_REG_W32_OFLD(R_AX_ACTION_FWD1, 0x00000000, 0);
			if (ret)
				return ret;
			ret = MAC_REG_W32_OFLD(R_AX_TF_FWD, 0x00000000, 0);
			if (ret)
				return ret;
			ret = MAC_REG_W32_OFLD(R_AX_HW_RPT_FWD, 0x00000000, 1);
			if (ret)
				return ret;
		} else {
			ret = _patch_restr_rx_qta(adapter);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]patch resume rx qta %d\n", ret);
				return ret;
			}

			ret = MAC_REG_W_OFLD(R_AX_RX_FUNCTION_STOP, B_AX_HDR_RX_STOP, 0, 0);
			if (ret)
				return ret;
			ret = MAC_REG_W_OFLD(R_AX_RX_FLTR_OPT, B_AX_SNIFFER_MODE, 1, 0);
			if (ret)
				return ret;
			ret = MAC_REG_W32_OFLD(R_AX_ACTION_FWD0, TRXCFG_MPDU_PROC_ACT_FRWD, 0);
			if (ret)
				return ret;
			ret = MAC_REG_W32_OFLD(R_AX_TF_FWD, TRXCFG_MPDU_PROC_TF_FRWD, 1);
			if (ret)
				return ret;

			cfg.type = MAC_AX_PPDU_STATUS;
			cfg.en = 1;
			ret = mac_ops->cfg_phy_rpt(adapter, &cfg);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]cfg_phy_rpt failed %d\n", ret);
				return ret;
			}
		}
		return MACSUCCESS;
	}
#endif
	if (sleep) {
		ret = _patch_redu_rx_qta(adapter);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]patch reduce rx qta %d\n", ret);
			return ret;
		}
		val32 = MAC_REG_R32(R_AX_RX_FUNCTION_STOP);
		val32 |= B_AX_HDR_RX_STOP;
		MAC_REG_W32(R_AX_RX_FUNCTION_STOP, val32);

		if (nlo_enable_record == 0) {
			val32 = MAC_REG_R32(R_AX_RX_FLTR_OPT);
			val32 &= ~B_AX_SNIFFER_MODE;
			MAC_REG_W32(R_AX_RX_FLTR_OPT, val32);
		} else {
			val32 = MAC_REG_R32(R_AX_RX_FLTR_OPT);
			val32 |= B_AX_SNIFFER_MODE;
			MAC_REG_W32(R_AX_RX_FLTR_OPT, val32);
			PLTFM_MSG_TRACE("enable sniffer for nlo\n");
			nlo_enable_record = 0;
		}

		cfg.type = MAC_AX_PPDU_STATUS;
		cfg.en = 0;
		ret = mac_ops->cfg_phy_rpt(adapter, &cfg);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]cfg_phy_rpt failed %d\n", ret);
			return ret;
		}

		MAC_REG_W32(R_AX_ACTION_FWD0, 0x00000000);
		MAC_REG_W32(R_AX_ACTION_FWD1, 0x00000000);
		MAC_REG_W32(R_AX_TF_FWD, 0x00000000);
		MAC_REG_W32(R_AX_HW_RPT_FWD, 0x00000000);
	} else {
		ret = _patch_restr_rx_qta(adapter);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]patch resume rx qta %d\n", ret);
			return ret;
		}
		val32 = MAC_REG_R32(R_AX_RX_FUNCTION_STOP);
		val32 &= ~B_AX_HDR_RX_STOP;
		MAC_REG_W32(R_AX_RX_FUNCTION_STOP, val32);
		val32 = MAC_REG_R32(R_AX_RX_FLTR_OPT);
		MAC_REG_W32(R_AX_RX_FLTR_OPT, val32);

		cfg.type = MAC_AX_PPDU_STATUS;
		cfg.en = 1;
		ret = mac_ops->cfg_phy_rpt(adapter, &cfg);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]cfg_phy_rpt failed %d\n", ret);
			return ret;
		}

		MAC_REG_W32(R_AX_ACTION_FWD0, TRXCFG_MPDU_PROC_ACT_FRWD);
		MAC_REG_W32(R_AX_TF_FWD, TRXCFG_MPDU_PROC_TF_FRWD);

		PLTFM_MSG_ERR("[wow] Start to dump PLE debug pages\n");
		for (dbg_page = 0; dbg_page < 4; dbg_page++)
			mac_ops->dump_ple_dbg_page(adapter, dbg_page);
	}

	return MACSUCCESS;
}

u32 mac_get_wow_fw_status(struct mac_ax_adapter *adapter, u8 *status,
			  u8 func_en)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (func_en)
		func_en = 1;

	*status = !!((MAC_REG_R8(R_AX_WOW_CTRL) & B_AX_WOW_WOWEN));

	if (func_en == *status)
		*status = 1;
	else
		*status = 0;

	return MACSUCCESS;
}

u32 _mac_request_aoac_report_rx_rdy(struct mac_ax_adapter *adapter)
{
	u32 ret;
	#if MAC_AX_PHL_H2C
	struct rtw_h2c_pkt *h2cb;
	#else
	struct h2c_buf *h2cb;
	#endif
	u8 *buf;
	struct fwcmd_aoac_report_req *fwcmd_aoac_rpt_req;

	h2cb = h2cb_alloc(adapter, H2CB_CLASS_DATA);

	if (!h2cb)
		return MACNPTR;

	buf = h2cb_put(h2cb, sizeof(struct fwcmd_aoac_report_req));
	if (!buf) {
		ret = MACNOBUF;
		goto fail;
	}

	fwcmd_aoac_rpt_req = (struct fwcmd_aoac_report_req *)buf;
	fwcmd_aoac_rpt_req->dword0 =
	cpu_to_le32(FWCMD_H2C_AOAC_REPORT_REQ_RX_READY);

	ret = h2c_pkt_set_hdr(adapter, h2cb,
			      FWCMD_TYPE_H2C, FWCMD_H2C_CAT_MAC,
			      FWCMD_H2C_CL_WOW, FWCMD_H2C_FUNC_AOAC_REPORT_REQ,
			      1, 0);
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
	if (ret) {
		PLTFM_MSG_ERR("[ERR]platform tx: %d\n", ret);
		adapter->sm.aoac_rpt = MAC_AX_AOAC_RPT_ERROR;
		goto fail;
	}

	h2cb_free(adapter, h2cb);

	return MACSUCCESS;
fail:
	h2cb_free(adapter, h2cb);

	return ret;
}

u32 _mac_request_aoac_report_rx_not_rdy(struct mac_ax_adapter *adapter)
{
	struct mac_ax_wowlan_info *wow_info = &adapter->wowlan_info;
	struct mac_ax_aoac_report *aoac_rpt = (struct mac_ax_aoac_report *)wow_info->aoac_report;
	struct mac_ax_h2creg_info h2c_info = {0};
	struct mac_ax_c2hreg_poll c2h_poll = {0};
	struct fwcmd_c2hreg *c2h_content = &c2h_poll.c2hreg_cont.c2h_content;
	u32 ret;
	u8 *p_iv;

	h2c_info.id = FWCMD_H2CREG_FUNC_AOAC_RPT_1;
	h2c_info.content_len = sizeof(struct fwcmd_aoac_rpt_1);

	c2h_poll.polling_id = FWCMD_C2HREG_FUNC_AOAC_RPT_1;
	c2h_poll.retry_cnt = WOW_GET_AOAC_RPT_C2H_CNT;
	c2h_poll.retry_wait_us = WOW_GET_AOAC_RPT_C2H_DLY;

	ret = proc_msg_reg(adapter, &h2c_info, &c2h_poll);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("%s: get aoac rpt(%d) fail: %d\n",
			      __func__, FWCMD_C2HREG_FUNC_AOAC_RPT_1, ret);
		return ret;
	}

	aoac_rpt->key_idx = GET_FIELD(c2h_content->dword0,
				      FWCMD_C2HREG_AOAC_RPT_1_KEY_IDX);
	aoac_rpt->rekey_ok = GET_FIELD(c2h_content->dword0,
				       FWCMD_C2HREG_AOAC_RPT_1_REKEY_OK);
	p_iv = (&aoac_rpt->gtk_rx_iv_0[0] + (aoac_rpt->key_idx * IV_LENGTH));
	p_iv[0] = GET_FIELD(c2h_content->dword1,
			    FWCMD_C2HREG_AOAC_RPT_1_IV_0);
	p_iv[1] = GET_FIELD(c2h_content->dword1,
			    FWCMD_C2HREG_AOAC_RPT_1_IV_1);
	p_iv[2] = GET_FIELD(c2h_content->dword1,
			    FWCMD_C2HREG_AOAC_RPT_1_IV_2);
	p_iv[3] = GET_FIELD(c2h_content->dword1,
			    FWCMD_C2HREG_AOAC_RPT_1_IV_3);
	p_iv[4] = GET_FIELD(c2h_content->dword2,
			    FWCMD_C2HREG_AOAC_RPT_1_IV_4);
	p_iv[5] = GET_FIELD(c2h_content->dword2,
			    FWCMD_C2HREG_AOAC_RPT_1_IV_5);
	p_iv[6] = GET_FIELD(c2h_content->dword2,
			    FWCMD_C2HREG_AOAC_RPT_1_IV_6);
	p_iv[7] = GET_FIELD(c2h_content->dword2,
			    FWCMD_C2HREG_AOAC_RPT_1_IV_7);
	aoac_rpt->ptk_rx_iv[0] = GET_FIELD(c2h_content->dword3,
					   FWCMD_C2HREG_AOAC_RPT_1_PKT_IV_0);
	aoac_rpt->ptk_rx_iv[1] = GET_FIELD(c2h_content->dword3,
					   FWCMD_C2HREG_AOAC_RPT_1_PKT_IV_1);
	aoac_rpt->ptk_rx_iv[2] = GET_FIELD(c2h_content->dword3,
					   FWCMD_C2HREG_AOAC_RPT_1_PKT_IV_2);
	aoac_rpt->ptk_rx_iv[3] = GET_FIELD(c2h_content->dword3,
					   FWCMD_C2HREG_AOAC_RPT_1_PKT_IV_3);

	h2c_info.id = FWCMD_H2CREG_FUNC_AOAC_RPT_2;
	h2c_info.content_len = sizeof(struct fwcmd_aoac_rpt_2);

	c2h_poll.polling_id = FWCMD_C2HREG_FUNC_AOAC_RPT_2;
	c2h_poll.retry_cnt = WOW_GET_AOAC_RPT_C2H_CNT;
	c2h_poll.retry_wait_us = WOW_GET_AOAC_RPT_C2H_DLY;
	ret = proc_msg_reg(adapter, &h2c_info, &c2h_poll);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("%s: get aoac rpt(%d) fail: %d\n",
			      __func__, FWCMD_C2HREG_FUNC_AOAC_RPT_2, ret);
		return ret;
	}

	aoac_rpt->ptk_rx_iv[4] = GET_FIELD(c2h_content->dword0,
					   FWCMD_C2HREG_AOAC_RPT_2_PKT_IV_4);
	aoac_rpt->ptk_rx_iv[5] = GET_FIELD(c2h_content->dword0,
					   FWCMD_C2HREG_AOAC_RPT_2_PKT_IV_5);
	aoac_rpt->ptk_rx_iv[6] = GET_FIELD(c2h_content->dword1,
					   FWCMD_C2HREG_AOAC_RPT_2_PKT_IV_6);
	aoac_rpt->ptk_rx_iv[7] = GET_FIELD(c2h_content->dword1,
					   FWCMD_C2HREG_AOAC_RPT_2_PKT_IV_7);
	aoac_rpt->igtk_ipn[0] = GET_FIELD(c2h_content->dword1,
					  FWCMD_C2HREG_AOAC_RPT_2_IGTK_IPN_0);
	aoac_rpt->igtk_ipn[1] = GET_FIELD(c2h_content->dword1,
					  FWCMD_C2HREG_AOAC_RPT_2_IGTK_IPN_1);
	aoac_rpt->igtk_ipn[2] = GET_FIELD(c2h_content->dword2,
					  FWCMD_C2HREG_AOAC_RPT_2_IGTK_IPN_2);
	aoac_rpt->igtk_ipn[3] = GET_FIELD(c2h_content->dword2,
					  FWCMD_C2HREG_AOAC_RPT_2_IGTK_IPN_3);
	aoac_rpt->igtk_ipn[4] = GET_FIELD(c2h_content->dword2,
					  FWCMD_C2HREG_AOAC_RPT_2_IGTK_IPN_4);
	aoac_rpt->igtk_ipn[5] = GET_FIELD(c2h_content->dword2,
					  FWCMD_C2HREG_AOAC_RPT_2_IGTK_IPN_5);
	aoac_rpt->igtk_ipn[6] = GET_FIELD(c2h_content->dword3,
					  FWCMD_C2HREG_AOAC_RPT_2_IGTK_IPN_6);
	aoac_rpt->igtk_ipn[7] = GET_FIELD(c2h_content->dword3,
					  FWCMD_C2HREG_AOAC_RPT_2_IGTK_IPN_7);

	return MACSUCCESS;
}

u32 mac_request_aoac_report(struct mac_ax_adapter *adapter,
			    u8 rx_ready)
{
	u32 ret;
	struct mac_ax_wowlan_info *wow_info = &adapter->wowlan_info;

	if (adapter->sm.aoac_rpt != MAC_AX_AOAC_RPT_IDLE)
		return MACPROCERR;

	if (wow_info->aoac_report) {
		PLTFM_FREE(wow_info->aoac_report,
			   sizeof(struct mac_ax_aoac_report));
	}
	wow_info->aoac_report = (u8 *)PLTFM_MALLOC(sizeof(struct mac_ax_aoac_report));
	if (!wow_info->aoac_report) {
		PLTFM_MSG_ERR("%s: malloc fail\n", __func__);
		return MACBUFALLOC;
	}

	adapter->sm.aoac_rpt = MAC_AX_AOAC_RPT_H2C_SENDING;

	if (rx_ready)
		ret = _mac_request_aoac_report_rx_rdy(adapter);
	else
		ret = _mac_request_aoac_report_rx_not_rdy(adapter);

	return ret;
}

u32 mac_read_aoac_report(struct mac_ax_adapter *adapter,
			 struct mac_ax_aoac_report *rpt_buf, u8 rx_ready)
{
	struct mac_ax_wowlan_info *wow_info = &adapter->wowlan_info;
	u32 ret = MACSUCCESS;
	u8 cnt = 100;

	while ((rx_ready) && (adapter->sm.aoac_rpt != MAC_AX_AOAC_RPT_H2C_DONE)) {
		PLTFM_DELAY_MS(1);
		if (--cnt == 0) {
			PLTFM_MSG_ERR("[ERR] read aoac report(%d) fail\n",
				      adapter->sm.aoac_rpt);
			adapter->sm.aoac_rpt = MAC_AX_AOAC_RPT_IDLE;
			return MACPOLLTO;
		}
	}

	if (wow_info->aoac_report) {
		PLTFM_MEMCPY(rpt_buf, wow_info->aoac_report,
			     sizeof(struct mac_ax_aoac_report));
		PLTFM_FREE(wow_info->aoac_report,
			   sizeof(struct mac_ax_aoac_report));
		wow_info->aoac_report = NULL;
	} else {
		PLTFM_MSG_ERR("[ERR] aoac report memory allocate fail\n");
		ret = MACBUFALLOC;
	}

	adapter->sm.aoac_rpt = MAC_AX_AOAC_RPT_IDLE;

	return ret;
}

u32 mac_check_aoac_report_done(struct mac_ax_adapter *adapter)
{
	PLTFM_MSG_TRACE("[TRACE]%s: curr state: %d\n", __func__,
			adapter->sm.aoac_rpt);

	if (adapter->sm.aoac_rpt == MAC_AX_AOAC_RPT_H2C_DONE)
		return MACSUCCESS;
	else
		return MACPROCBUSY;
}

u32 mac_wow_stop_trx(struct mac_ax_adapter *adapter)
{
	struct mac_ax_h2creg_info h2c_info;
	struct mac_ax_c2hreg_poll c2h_poll;
	u32 ret;

	if (adapter->sm.wow_stoptrx_stat == MAC_AX_WOW_STOPTRX_BUSY) {
		PLTFM_MSG_ERR("[ERR]wow stop trx busy\n");
		return MACPROCERR;
	} else if (adapter->sm.wow_stoptrx_stat == MAC_AX_WOW_STOPTRX_FAIL) {
		PLTFM_MSG_WARN("[WARN]prev wow stop trx fail\n");
	}

	adapter->sm.wow_stoptrx_stat = MAC_AX_WOW_STOPTRX_BUSY;

	h2c_info.id = FWCMD_H2CREG_FUNC_WOW_TRX_STOP;
	h2c_info.content_len = 0;
	h2c_info.h2c_content.dword0 = 0;
	h2c_info.h2c_content.dword1 = 0;
	h2c_info.h2c_content.dword2 = 0;
	h2c_info.h2c_content.dword3 = 0;

	c2h_poll.polling_id = FWCMD_C2HREG_FUNC_WOW_TRX_STOP;
	c2h_poll.retry_cnt = WOW_GET_STOP_TRX_C2H_CNT;
	c2h_poll.retry_wait_us = WOW_GET_STOP_TRX_C2H_DLY;

	ret = proc_msg_reg(adapter, &h2c_info, &c2h_poll);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]wow stoptrx proc msg reg %d\n", ret);
		adapter->sm.wow_stoptrx_stat = MAC_AX_WOW_STOPTRX_FAIL;
		return ret;
	}

	adapter->sm.wow_stoptrx_stat = MAC_AX_WOW_STOPTRX_IDLE;

	return MACSUCCESS;
}

u32 free_aoac_report(struct mac_ax_adapter *adapter)
{
	struct mac_ax_wowlan_info *wow_info = &adapter->wowlan_info;

	if (wow_info->aoac_report) {
		PLTFM_FREE(wow_info->aoac_report,
			   sizeof(struct mac_ax_aoac_report));
		wow_info->aoac_report = NULL;
	} else {
		PLTFM_MSG_ERR("[ERR] aoac report pointer null\n");
	}

	return MACSUCCESS;
}
