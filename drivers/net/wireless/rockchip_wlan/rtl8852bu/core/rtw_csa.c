/******************************************************************************
 *
 * Copyright(c) 2019 - 2021 Realtek Corporation.
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
 *****************************************************************************/
#define _RTW_CSA_C_
#include <drv_types.h>

#if CONFIG_DFS
void reset_csa_param(struct rf_ctl_t *rfctl)
{
	rfctl->csa_mode = 0;
	rfctl->csa_switch_cnt = 0;
	rfctl->csa_ch_width = 0;
	_rtw_memset(&(rfctl->csa_chandef), 0, sizeof(struct rtw_chan_def));
}

/* Get ch/bw/offset of CSA from rfctl, and check these parameters is valid or not */
bool rtw_get_csa_setting(struct dvobj_priv *d, s16 *req_ch, u8 *req_bw, u8 *req_offset)
{
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(d);
	struct _ADAPTER *a = dvobj_get_primary_adapter(d);
	u8 ifbmp_m = rtw_mi_get_ap_mesh_ifbmp(a);
	bool valid = _TRUE;

	*req_ch = REQ_CH_NONE;
	*req_bw = CHANNEL_WIDTH_20;
	*req_offset = CHAN_OFFSET_NO_EXT;

	if (rtw_chset_search_ch(rfctl->channel_set, rfctl->csa_chandef.chan) >= 0
		&& !rtw_chset_is_ch_non_ocp(rfctl->channel_set, rfctl->csa_chandef.chan)
	) {
		/* CSA channel available and valid */
		*req_ch = rfctl->csa_chandef.chan;
		RTW_INFO("CSA : "FUNC_ADPT_FMT" valid CSA ch%u\n", FUNC_ADPT_ARG(a),
			rfctl->csa_chandef.chan);
	} else if (ifbmp_m) {
		/* no available or valid CSA channel, having AP/MESH ifaces */
		*req_ch = REQ_CH_NONE;
		valid = _FALSE;
		RTW_INFO("CSA : "FUNC_ADPT_FMT" ch sel by AP/MESH ifaces\n", FUNC_ADPT_ARG(a));
		goto exit;
	} else {
		/* no available or valid CSA channel and no AP/MESH ifaces */
		if (!is_supported_24g(dvobj_to_regsty(d)->band_type))
			*req_ch = 36;
		else
			*req_ch = 1;
		valid = _FALSE;
		RTW_INFO("CSA : "FUNC_ADPT_FMT" switch to ch%d, then disconnect with AP\n",
			FUNC_ADPT_ARG(a), *req_ch);
		goto exit;
	}

	if (rfctl->csa_ch_width == 1) {
		*req_bw = CHANNEL_WIDTH_80;
		*req_offset = rfctl->csa_chandef.offset;
	} else if (rfctl->csa_ch_width == 0 && rfctl->csa_chandef.offset != CHAN_OFFSET_NO_EXT) {
		*req_bw = CHANNEL_WIDTH_40;
		*req_offset = rfctl->csa_chandef.offset;
	} else {
		*req_bw = CHANNEL_WIDTH_20;
		*req_offset = CHAN_OFFSET_NO_EXT;
	}

	/* get correct offset and check ch/bw/offset is valid or not */
	if (!rtw_get_offset_by_chbw(*req_ch, *req_bw, req_offset)) {
		*req_bw = CHANNEL_WIDTH_20;
		*req_offset = CHAN_OFFSET_NO_EXT;
	}

exit:
	return valid;
}

#ifdef CONFIG_ECSA_PHL
static void reset_ecsa_param(struct _ADAPTER *a)
{
	struct core_ecsa_info *ecsa_info = &(a->ecsa_info);
	struct rtw_phl_ecsa_param *ecsa_param = &(ecsa_info->phl_ecsa_param);

	SET_ECSA_STATE(a, ECSA_ST_NONE);
	ecsa_info->ecsa_allow_case = 0;
	ecsa_info->ecsa_delay_time = 0;
	ecsa_info->channel_width = 0;
	ecsa_info->bss_param = NULL;
	_rtw_memset(ecsa_param, 0, sizeof(struct rtw_phl_ecsa_param));
}

bool rtw_mr_is_ecsa_running(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct _ADAPTER *iface;
	struct core_ecsa_info *ecsa_info = &(a->ecsa_info);
	u8 i;

	for (i = 0; i < d->iface_nums; i++) {
		iface = d->padapters[i];
		if (!iface)
			continue;
		if (!CHK_ECSA_STATE(iface, ECSA_ST_NONE))
			return _TRUE;
	}
	return _FALSE;
}

static void rtw_ecsa_update_sta_chan_info(struct _ADAPTER *a, struct rtw_chan_def new_chan_def)
{
	struct mlme_ext_priv *pmlmeext = &a->mlmeextpriv;
	struct mlme_priv *pmlmepriv = &a->mlmepriv;
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(d);
	u8 new_ch = new_chan_def.chan;
	u8 new_bw = (u8)new_chan_def.bw;
	u8 new_offset = (u8)new_chan_def.offset;
	bool is_chctx_add = _FALSE;

	pmlmeext->chandef.chan= new_ch;
	pmlmeext->chandef.bw = new_bw;
	pmlmeext->chandef.offset = new_offset;
	pmlmepriv->cur_network.network.Configuration.DSConfig = new_ch;

	/* update wifi role chandef */
	rtw_hw_update_chan_def(a);

	/* update chanctx */
	rtw_phl_chanctx_del(d->phl, a->phl_role, NULL);
	is_chctx_add = rtw_phl_chanctx_add(d->phl, a->phl_role,
						&new_chan_def.chan, &new_chan_def.bw, &new_chan_def.offset);
	if (is_chctx_add == _FALSE)
		RTW_ERR("CSA : "FUNC_ADPT_FMT" chan_ctx add fail!", FUNC_ADPT_ARG(a));

	set_fwstate(pmlmepriv, WIFI_CSA_UPDATE_BEACON);

	/* STA wait 70 seconds for receiving beacons in DFS channel */
	if (rtw_chset_is_dfs_chbw(rfctl->channel_set, new_ch, new_bw, new_offset)) {
		RTW_INFO("CSA : set csa_timer to 70 seconds\n");
		_set_timer(&pmlmeext->csa_timer, CAC_TIME_MS + 10000);
	}

	#ifdef CONFIG_DFS_MASTER
	rtw_dfs_rd_en_decision(a, MLME_OPCH_SWITCH, 0);
	#endif
}

static void rtw_ecsa_update_ap_chan_info(struct _ADAPTER *a, struct rtw_chan_def new_chan_def)
{
	struct core_ecsa_info *ecsa_info = &(a->ecsa_info);
	struct createbss_parm *parm;

	ecsa_info->bss_param = (struct createbss_parm *)rtw_zmalloc(sizeof(struct createbss_parm));
	if (ecsa_info->bss_param) {
		parm = ecsa_info->bss_param;
		parm->adhoc = 0;
		parm->ifbmp = BIT(a->iface_id);
		parm->excl_ifbmp = 0;
		parm->req_ch = new_chan_def.chan;
		parm->req_bw = new_chan_def.bw;
		parm->req_offset = new_chan_def.offset;
		parm->ifbmp_ch_changed = 0;
		parm->ch_to_set = 0;
		parm->bw_to_set = 0;
		parm->offset_to_set = 0;
		parm->do_rfk = _FALSE;

		start_bss_network(a, parm);
	} else {
		RTW_ERR("CSA : can't allocate memory for bss_param\n");
	}
}

void rtw_ecsa_update_probe_resp(struct xmit_frame *xframe)
{
	struct _ADAPTER *a = xframe->padapter;
	struct core_ecsa_info *ecsa_info = &(a->ecsa_info);
	struct rtw_phl_ecsa_param *ecsa_param = &(ecsa_info->phl_ecsa_param);
	struct pkt_attrib *pattrib = &xframe->attrib;
	u8 hdr_len = sizeof(struct rtw_ieee80211_hdr_3addr);
	u8 *ies;
	sint ies_len;
	u8 *csa_ie;
	sint csa_ie_len;

	ies = xframe->buf_addr + TXDESC_OFFSET + hdr_len + _BEACON_IE_OFFSET_;
	ies_len = pattrib->pktlen - hdr_len - _BEACON_IE_OFFSET_; 

	csa_ie = rtw_get_ie(ies, WLAN_EID_CHANNEL_SWITCH, &csa_ie_len, ies_len);

	if (csa_ie == NULL)
		return;

	csa_ie[2 + CSA_SWITCH_COUNT] = ecsa_param->count;

	#ifdef DBG_CSA
	RTW_INFO("CSA : update csa count of probe response = %u\n", csa_ie[2 + CSA_SWITCH_COUNT]);
	#endif
}

void rtw_ecsa_update_beacon(void *priv, struct rtw_wifi_role_t *role)
{
#ifdef CONFIG_AP_MODE
	struct dvobj_priv *d = (struct dvobj_priv *)priv;
	struct _ADAPTER *a = d->padapters[role->id];
	_update_beacon(a, WLAN_EID_CHANNEL_SWITCH, NULL, _TRUE, 0, "update CSA count");
#endif
}

/* PHL MR module check core layer if AP mode can switch channel now */
bool rtw_ap_check_ecsa_allow(
	void *priv,
	struct rtw_wifi_role_t *role,
	struct rtw_chan_def chan_def,
	enum phl_ecsa_start_reason reason,
	u32 *delay_start_ms
)
{
	struct dvobj_priv *d = (struct dvobj_priv *)priv;
	struct _ADAPTER *a = d->padapters[role->id];
	struct core_ecsa_info *ecsa_info = &(a->ecsa_info);
	bool ecsa_allow = _TRUE;
	u8 i;

	/* TODO : need to check MCC-ECSA case */
	/* if (!(ecsa_info->ecsa_allow_case & BIT(reason))) { */
	if (0) {
		RTW_INFO("CSA : "FUNC_ADPT_FMT" : Case %u not support!\n", FUNC_ADPT_ARG(a), reason);
		ecsa_allow = _FALSE;
		goto exit;
	}

	/* Check DFS channel */
	if (!rtw_phl_regulation_valid_channel(GET_PHL_INFO(d),
						chan_def.band,
						chan_def.chan,
						(CH_PASSIVE | CH_DFS))) {
		RTW_ERR("CSA : "FUNC_ADPT_FMT" : DFS channel (%u) not support!\n",
				FUNC_ADPT_ARG(a), chan_def.chan);
		ecsa_allow = _FALSE;
		goto exit;
	}

	if (ecsa_info->ecsa_delay_time != 0)
		*delay_start_ms = ecsa_info->ecsa_delay_time;
	else if (reason <= ECSA_START_MCC_5G_TO_24G)
		*delay_start_ms = MCC_ECSA_DELAY_START_TIME;
	RTW_INFO("CSA : %s : ECSA will delay %u ms\n", __func__, *delay_start_ms);

	SET_ECSA_STATE(a, ECSA_ST_SW_START);
exit:
	return ecsa_allow;
}

void rtw_ecsa_mr_update_chan_info_by_role(
	void *priv,
	struct rtw_wifi_role_t *role,
	struct rtw_chan_def new_chan_def
)
{
	struct dvobj_priv *d = (struct dvobj_priv *)priv;
	struct _ADAPTER *a = d->padapters[role->id];
	struct mlme_ext_priv *pmlmeext = &a->mlmeextpriv;
	struct mlme_priv *pmlmepriv = &a->mlmepriv;

	RTW_INFO("CSA : "FUNC_ADPT_FMT", new ch/bw/offset = %u,%u,%u\n", FUNC_ADPT_ARG(a), \
		new_chan_def.chan, new_chan_def.bw, new_chan_def.offset);

	if (role->type == PHL_RTYPE_STATION || role->type == PHL_RTYPE_P2P_GC)
		rtw_ecsa_update_sta_chan_info(a, new_chan_def);
	else if (role->type == PHL_RTYPE_AP ||role->type == PHL_RTYPE_P2P_GO)
		rtw_ecsa_update_ap_chan_info(a, new_chan_def);
}

bool rtw_ecsa_check_tx_resume_allow(void *priv, struct rtw_wifi_role_t *role)
{
	/* TODO */
	/* Is DFS slave still monitoring channel ?
	If Yes, return False to PHL; If no, return True to PHL */
	struct dvobj_priv *d = (struct dvobj_priv *)priv;
	struct _ADAPTER *a = d->padapters[role->id];

	RTW_INFO("CSA : "FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(a));
	return 1;
}

void rtw_ecsa_complete(void *priv, struct rtw_wifi_role_t *role)
{
	struct dvobj_priv *d = (struct dvobj_priv *)priv;
	struct _ADAPTER *a = d->padapters[role->id];
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(d);
	struct core_ecsa_info *ecsa_info = &(a->ecsa_info);
	struct createbss_parm *parm = ecsa_info->bss_param;
	u8 i, ht_option = 0;

	RTW_INFO("CSA : "FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(a));
	rtw_phl_mr_dump_cur_chandef(d->phl, role);

	if (role->type == PHL_RTYPE_STATION || role->type == PHL_RTYPE_P2P_GC) {
		/* 
		* TODO
		* STA mode need to update RA if it receive CHANNEL_SWITCH_WRAPPER IE
		* STA mode update its RA at rtw_check_bcn_info() now
		*/
		rtw_rfctl_update_op_mode(rfctl, 0, 0);
	} else if (role->type == PHL_RTYPE_AP ||role->type == PHL_RTYPE_P2P_GO) {
		#if defined(CONFIG_IOCTL_CFG80211) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0))
		for (i = 0; i < d->iface_nums; i++) {
			if (!(parm->ifbmp_ch_changed & BIT(i)) || !d->padapters[i])
				continue;

			#ifdef CONFIG_80211N_HT
			ht_option = d->padapters[i]->mlmepriv.htpriv.ht_option;
			#endif

			rtw_cfg80211_ch_switch_notify(d->padapters[i]
				, d->padapters[i]->mlmeextpriv.chandef.chan
				, d->padapters[i]->mlmeextpriv.chandef.bw
				, d->padapters[i]->mlmeextpriv.chandef.offset
				, ht_option, 0);
		}
		#endif
		rtw_rfctl_update_op_mode(adapter_to_rfctl(a), parm->ifbmp, 1);

		rtw_core_ap_start(a, parm);

		rtw_ap_update_clients_rainfo(a, PHL_CMD_DIRECTLY);

		rtw_mfree((u8 *)parm, sizeof(struct createbss_parm));
	}

	rtw_mi_os_xmit_schedule(a);

	reset_ecsa_param(a);
}

/* Get ch/bw/offset of CSA from adapter, and check these parameters is valid or not */
static bool rtw_sta_get_ecsa_setting(struct _ADAPTER *a, s16 *req_ch, u8 *req_bw, u8 *req_offset)
{
	struct rf_ctl_t *rfctl = adapter_to_rfctl(a);
	struct core_ecsa_info *ecsa_info = &(a->ecsa_info);
	struct rtw_phl_ecsa_param *ecsa_param = &(ecsa_info->phl_ecsa_param);
	u8 ifbmp_m = rtw_mi_get_ap_mesh_ifbmp(a);
	u8 csa_chan = ecsa_param->new_chan_def.chan;
	u8 csa_offset = ecsa_param->new_chan_def.offset;
	bool valid = _TRUE;

	*req_ch = REQ_CH_NONE;
	*req_bw = CHANNEL_WIDTH_20;
	*req_offset = CHAN_OFFSET_NO_EXT;

	if (rtw_chset_search_ch(rfctl->channel_set, csa_chan) >= 0
		&& !rtw_chset_is_ch_non_ocp(rfctl->channel_set, csa_chan)
	) {
		/* CSA channel available and valid */
		*req_ch = csa_chan;
		RTW_INFO("CSA : "FUNC_ADPT_FMT" valid CSA ch%u\n", FUNC_ADPT_ARG(a), csa_chan);
	} else if (ifbmp_m) {
		/* no available or valid CSA channel, having AP/MESH ifaces */
		*req_ch = REQ_CH_NONE;
		valid = _FALSE;
		RTW_INFO("CSA : "FUNC_ADPT_FMT" ch sel by AP/MESH ifaces\n", FUNC_ADPT_ARG(a));
		goto exit;
	} else {
		/* no available or valid CSA channel and no AP/MESH ifaces */
		/* TODO : DFS slave may need to switch channel as soon as possible before disconnect */
		#if 0
		if (!is_supported_24g(adapter_to_regsty(a)->band_type))
			*req_ch = 36;
		else
			*req_ch = 1;
		#endif
		valid = _FALSE;
		RTW_INFO("CSA : "FUNC_ADPT_FMT" switch to ch %d, then disconnect with AP\n",
			FUNC_ADPT_ARG(a), *req_ch);
		goto exit;
	}

	/* Transform channel_width to bandwidth 20/40/80M */
	if (ecsa_info->channel_width == 1) {
		*req_bw = CHANNEL_WIDTH_80;
		*req_offset = csa_offset;
	} else if (ecsa_info->channel_width == 0 && csa_offset != CHAN_OFFSET_NO_EXT) {
		*req_bw = CHANNEL_WIDTH_40;
		*req_offset = csa_offset;
	} else {
		*req_bw = CHANNEL_WIDTH_20;
		*req_offset = CHAN_OFFSET_NO_EXT;
	}

	/* Get correct offset and check ch/bw/offset is valid or not */
	if (!rtw_get_offset_by_chbw(*req_ch, *req_bw, req_offset)) {
		*req_bw = CHANNEL_WIDTH_20;
		*req_offset = CHAN_OFFSET_NO_EXT;
	}

	/* Update result to ecsa_param */
	ecsa_param->new_chan_def.chan = *req_ch;
	ecsa_param->new_chan_def.bw = *req_bw;
	ecsa_param->new_chan_def.offset = *req_offset;

exit:
	return valid;
}

static void rtw_sta_ecsa_invalid_hdl(struct _ADAPTER *a, s16 req_ch, u8 req_bw, u8 req_offset)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rf_ctl_t *rfctl = dvobj_to_rfctl(d);
	u8 ifbmp_s = rtw_mi_get_ld_sta_ifbmp(a);
	struct rtw_chan_def mr_chdef = {0};

	if (!ifbmp_s)
		return;

	set_fwstate(&a->mlmepriv,  WIFI_OP_CH_SWITCHING);
	issue_deauth(a, get_bssid(&a->mlmepriv), WLAN_REASON_DEAUTH_LEAVING);

	/* Decide whether enable DFS slave radar detection or not */
	#ifdef CONFIG_DFS_MASTER
	rtw_dfs_rd_en_decision(a, MLME_OPCH_SWITCH, ifbmp_s);
	#endif

	/* TODO : DFS slave may need to switch channel as soon as possible before disconnect */

	/* This context can't I/O, so use RTW_CMDF_DIRECTLY */
	rtw_disassoc_cmd(a, 0, RTW_CMDF_DIRECTLY);
	rtw_indicate_disconnect(a, 0, _FALSE);
	#ifndef CONFIG_STA_CMD_DISPR
	rtw_free_assoc_resources(a, _TRUE);
	#endif
	rtw_free_network_queue(a, _TRUE);
	RTW_INFO("CSA : "FUNC_ADPT_FMT" disconnect with AP\n", FUNC_ADPT_ARG(a));

	reset_ecsa_param(a);

	rtw_mi_os_xmit_schedule(a);
}

void rtw_trigger_phl_ecsa_start(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	struct core_ecsa_info *ecsa_info = &(a->ecsa_info);
	struct rtw_phl_ecsa_param *ecsa_param = &(ecsa_info->phl_ecsa_param);

	/* STA need to check ecsa setting */
	if (ecsa_param->ecsa_type == ECSA_TYPE_STA) {
		s16 req_ch;
		u8 req_bw, req_offset;

		if (!rtw_sta_get_ecsa_setting(a, &req_ch, &req_bw, &req_offset)) {
			/* we should handle error case by core layer self */
			rtw_sta_ecsa_invalid_hdl(a, req_ch, req_bw, req_offset);
			return;
		}
	}

	if (rtw_phl_ecsa_start(GET_PHL_INFO(d), role, ecsa_param) != RTW_PHL_STATUS_SUCCESS)
		RTW_ERR("CSA : Start PHL ECSA fail\n");

#if 0 /* TODO : ECSA */
	if(0) {
		if(!rtw_phl_get_chandef_from_operating_class(param.ch,
								param.op_class,
								&(param.new_chan_def))){
			RTW_INFO("[CSA] Get chandef fail!Use 20 MHz to switch\n");
			param.new_chan_def.band = rtw_phl_get_band_type(param.ch);
			param.new_chan_def.chan = param.ch;
			param.new_chan_def.bw = CHANNEL_WIDTH_20;
			param.new_chan_def.offset = CHAN_OFFSET_NO_EXT;
		}
	}
#endif
}
#endif /* CONFIG_ECSA_PHL */
#endif /* CONFIG_DFS */
