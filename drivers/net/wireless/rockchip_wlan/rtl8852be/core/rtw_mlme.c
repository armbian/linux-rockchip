/******************************************************************************
 *
 * Copyright(c) 2007 - 2021 Realtek Corporation.
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
#define _RTW_MLME_C_
#include <drv_types.h>


extern u8 rtw_do_join(_adapter *padapter);


void rtw_init_mlme_timer(_adapter *padapter)
{
	struct	mlme_priv *pmlmepriv = &padapter->mlmepriv;

	rtw_init_timer(&(pmlmepriv->assoc_timer), rtw_join_timeout_handler, padapter);
	rtw_init_timer(&(pmlmepriv->scan_to_timer), rtw_scan_timeout_handler, padapter);

#ifdef CONFIG_SET_SCAN_DENY_TIMER
	rtw_init_timer(&(pmlmepriv->set_scan_deny_timer), rtw_set_scan_deny_timer_hdl, padapter);
#endif

#ifdef RTK_DMP_PLATFORM
	_init_workitem(&(pmlmepriv->Linkup_workitem), Linkup_workitem_callback, padapter);
	_init_workitem(&(pmlmepriv->Linkdown_workitem), Linkdown_workitem_callback, padapter);
#endif
}

sint	_rtw_init_mlme_priv(_adapter *padapter)
{
	sint	i;
	u8	*pbuf;
	struct wlan_network	*pnetwork;
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct rf_ctl_t *rfctl = adapter_to_rfctl(padapter);
	sint	res = _SUCCESS;
#ifdef CONFIG_RTW_MULTI_AP
	struct unassoc_sta_info *unassoc_sta;
#endif


	/* We don't need to memset padapter->XXX to zero, because adapter is allocated by rtw_zvmalloc(). */
	/* _rtw_memset((u8 *)pmlmepriv, 0, sizeof(struct mlme_priv)); */


	/*qos_priv*/
	/*pmlmepriv->qospriv.qos_option = pregistrypriv->wmm_enable;*/

	/*ht_priv*/
#ifdef CONFIG_80211N_HT
	pmlmepriv->htpriv.ampdu_enable = _FALSE;/*set to disabled*/
#endif

	pmlmepriv->nic_hdl = (u8 *)padapter;

	pmlmepriv->pscanned = NULL;
	init_fwstate(pmlmepriv, WIFI_STATION_STATE);
	pmlmepriv->cur_network.network.InfrastructureMode = Ndis802_11AutoUnknown;
	pmlmepriv->scan_mode = RTW_PHL_SCAN_ACTIVE; /* 1: active, 0: pasive. Maybe someday we should rename this varable to "active_mode" (Jeff) */

	_rtw_spinlock_init(&(pmlmepriv->lock));
	_rtw_init_queue(&(pmlmepriv->free_bss_pool));
	_rtw_init_queue(&(pmlmepriv->scanned_queue));

	set_scanned_network_val(pmlmepriv, 0);

	_rtw_memset(&pmlmepriv->assoc_ssid, 0, sizeof(NDIS_802_11_SSID));

	if (padapter->registrypriv.max_bss_cnt != 0)
		pmlmepriv->max_bss_cnt = padapter->registrypriv.max_bss_cnt;
	else if (rfctl->max_chan_nums <= MAX_CHANNEL_NUM_2G)
		pmlmepriv->max_bss_cnt = MAX_BSS_CNT;
	else
		pmlmepriv->max_bss_cnt = MAX_BSS_CNT + MAX_BSS_CNT;


	pbuf = rtw_zvmalloc(pmlmepriv->max_bss_cnt * (sizeof(struct wlan_network)));

	if (pbuf == NULL) {
		res = _FAIL;
		goto exit;
	}
	pmlmepriv->free_bss_buf = pbuf;

	pnetwork = (struct wlan_network *)pbuf;

	for (i = 0; i < pmlmepriv->max_bss_cnt; i++) {
		_rtw_init_listhead(&(pnetwork->list));

		rtw_list_insert_tail(&(pnetwork->list), &(pmlmepriv->free_bss_pool.queue));

		pnetwork++;
	}

#ifdef CONFIG_RTW_MULTI_AP
	if (is_primary_adapter(padapter)) {
		_rtw_init_queue(&(pmlmepriv->free_unassoc_sta_queue));
		_rtw_init_queue(&(pmlmepriv->unassoc_sta_queue));
		for (i = 0; i < UNASOC_STA_SRC_NUM; i++)
			pmlmepriv->unassoc_sta_mode_of_stype[i] = padapter->registrypriv.unassoc_sta_mode_of_stype[i];

		if (padapter->registrypriv.max_unassoc_sta_cnt != 0)
			pmlmepriv->max_unassoc_sta_cnt = padapter->registrypriv.max_unassoc_sta_cnt;
		else if (rfctl->max_chan_nums <= MAX_CHANNEL_NUM_2G)
			pmlmepriv->max_unassoc_sta_cnt = MAX_UNASSOC_STA_CNT;
		else
			pmlmepriv->max_unassoc_sta_cnt = MAX_UNASSOC_STA_CNT * 2;

		pbuf = rtw_zvmalloc(pmlmepriv->max_unassoc_sta_cnt * (sizeof(struct unassoc_sta_info)));
		if (pbuf == NULL) {
			res = _FAIL;
			goto exit;
		}

		pmlmepriv->free_unassoc_sta_buf = pbuf;
		unassoc_sta = (struct unassoc_sta_info *) pbuf;
		for (i = 0; i < pmlmepriv->max_unassoc_sta_cnt; i++) {
			_rtw_init_listhead(&(unassoc_sta->list));
			rtw_list_insert_tail(&(unassoc_sta->list), &(pmlmepriv->free_unassoc_sta_queue.queue));
			unassoc_sta++;
		}
	}
#endif
	/* allocate DMA-able/Non-Page memory for cmd_buf and rsp_buf */

	rtw_clear_scan_deny(padapter);
#ifdef CONFIG_ARP_KEEP_ALIVE
	pmlmepriv->bGetGateway = 0;
	pmlmepriv->GetGatewayTryCnt = 0;
#endif

#ifdef CONFIG_LAYER2_ROAMING
#define RTW_ROAM_SCAN_RESULT_EXP_MS (5*1000)
#define RTW_ROAM_RSSI_DIFF_TH 10
#define RTW_ROAM_SCAN_INTERVAL (5)    /* 5*(2 second)*/
#define RTW_ROAM_RSSI_THRESHOLD 70

	pmlmepriv->roam_flags = CONFIG_ROAMING_FLAG;

	pmlmepriv->roam_scanr_exp_ms = RTW_ROAM_SCAN_RESULT_EXP_MS;
	pmlmepriv->roam_rssi_diff_th = RTW_ROAM_RSSI_DIFF_TH;
	pmlmepriv->roam_scan_int 	 = RTW_ROAM_SCAN_INTERVAL;
	pmlmepriv->roam_rssi_threshold = RTW_ROAM_RSSI_THRESHOLD;
	pmlmepriv->need_to_roam = _FALSE;
	pmlmepriv->last_roaming = rtw_get_current_time();
#endif /* CONFIG_LAYER2_ROAMING */

#ifdef CONFIG_RTW_80211R
	rtw_ft_info_init(&pmlmepriv->ft_roam);
#endif

#ifdef CONFIG_RTW_MBO
#ifdef CONFIG_LAYER2_ROAMING
#if defined(CONFIG_RTW_WNM) || defined(CONFIG_RTW_80211K)
	rtw_roam_nb_info_init(padapter);
	pmlmepriv->ch_cnt = 0;
#endif	
#endif
#endif

#ifdef CONFIG_RTW_MBO
	rtw_mbo_attr_info_init(padapter);
#endif

	pmlmepriv->defs_lmt_sta = 2;
	pmlmepriv->defs_lmt_time = 5;

	rtw_init_mlme_timer(padapter);

exit:


	return res;
}

void rtw_mfree_mlme_priv_lock(struct mlme_priv *pmlmepriv)
{
	_rtw_spinlock_free(&pmlmepriv->lock);
	_rtw_spinlock_free(&(pmlmepriv->free_bss_pool.lock));
	_rtw_spinlock_free(&(pmlmepriv->scanned_queue.lock));
#ifdef CONFIG_RTW_MULTI_AP
	if (is_primary_adapter(mlme_to_adapter(pmlmepriv))) {
		_rtw_spinlock_free(&(pmlmepriv->unassoc_sta_queue.lock));
		_rtw_spinlock_free(&(pmlmepriv->free_unassoc_sta_queue.lock));
	}
#endif
}

static void rtw_free_mlme_ie_data(u8 **ppie, u32 *plen)
{
	if (*ppie) {
		rtw_mfree(*ppie, *plen);
		*plen = 0;
		*ppie = NULL;
	}
}

void rtw_free_mlme_priv_ie_data(struct mlme_priv *pmlmepriv)
{
#ifdef CONFIG_80211D
	rtw_buf_free(&pmlmepriv->recv_country_ie, &pmlmepriv->recv_country_ie_len);
#endif
#if defined(CONFIG_AP_MODE) && defined (CONFIG_NATIVEAP_MLME)
	rtw_buf_free(&pmlmepriv->assoc_req, &pmlmepriv->assoc_req_len);
	rtw_buf_free(&pmlmepriv->assoc_rsp, &pmlmepriv->assoc_rsp_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wps_beacon_ie, &pmlmepriv->wps_beacon_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wps_probe_req_ie, &pmlmepriv->wps_probe_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wps_probe_resp_ie, &pmlmepriv->wps_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wps_assoc_resp_ie, &pmlmepriv->wps_assoc_resp_ie_len);

	rtw_free_mlme_ie_data(&pmlmepriv->p2p_beacon_ie, &pmlmepriv->p2p_beacon_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_probe_req_ie, &pmlmepriv->p2p_probe_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_probe_resp_ie, &pmlmepriv->p2p_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_go_probe_resp_ie, &pmlmepriv->p2p_go_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_assoc_req_ie, &pmlmepriv->p2p_assoc_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->p2p_assoc_resp_ie, &pmlmepriv->p2p_assoc_resp_ie_len);
#endif

#if defined(CONFIG_WFD) && defined(CONFIG_IOCTL_CFG80211)
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_beacon_ie, &pmlmepriv->wfd_beacon_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_probe_req_ie, &pmlmepriv->wfd_probe_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_probe_resp_ie, &pmlmepriv->wfd_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_go_probe_resp_ie, &pmlmepriv->wfd_go_probe_resp_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_assoc_req_ie, &pmlmepriv->wfd_assoc_req_ie_len);
	rtw_free_mlme_ie_data(&pmlmepriv->wfd_assoc_resp_ie, &pmlmepriv->wfd_assoc_resp_ie_len);
#endif

#ifdef CONFIG_RTW_80211R
	rtw_free_mlme_ie_data(&pmlmepriv->auth_rsp, &pmlmepriv->auth_rsp_len);
#endif
#ifdef CONFIG_RTW_MBO
	rtw_buf_free(&pmlmepriv->pcell_data_cap_ie, &pmlmepriv->cell_data_cap_len);
#endif
}

#if defined(CONFIG_WFD) && defined(CONFIG_IOCTL_CFG80211)
int rtw_mlme_update_wfd_ie_data(struct mlme_priv *mlme, u8 type, u8 *ie, u32 ie_len)
{
	_adapter *adapter = mlme_to_adapter(mlme);
	struct wifi_display_info *wfd_info = &adapter->wfd_info;
	u8 clear = 0;
	u8 **t_ie = NULL;
	u32 *t_ie_len = NULL;
	int ret = _FAIL;

	if (!rtw_hw_chk_wl_func(adapter_to_dvobj(adapter), WL_FUNC_MIRACAST))
		goto success;

	if (wfd_info->wfd_enable == _TRUE)
		goto success; /* WFD IE is build by self */

	if (!ie && !ie_len)
		clear = 1;
	else if (!ie || !ie_len) {
		RTW_PRINT(FUNC_ADPT_FMT" type:%u, ie:%p, ie_len:%u"
			  , FUNC_ADPT_ARG(adapter), type, ie, ie_len);
		rtw_warn_on(1);
		goto exit;
	}

	switch (type) {
	case MLME_BEACON_IE:
		t_ie = &mlme->wfd_beacon_ie;
		t_ie_len = &mlme->wfd_beacon_ie_len;
		break;
	case MLME_PROBE_REQ_IE:
		t_ie = &mlme->wfd_probe_req_ie;
		t_ie_len = &mlme->wfd_probe_req_ie_len;
		break;
	case MLME_PROBE_RESP_IE:
		t_ie = &mlme->wfd_probe_resp_ie;
		t_ie_len = &mlme->wfd_probe_resp_ie_len;
		break;
	case MLME_GO_PROBE_RESP_IE:
		t_ie = &mlme->wfd_go_probe_resp_ie;
		t_ie_len = &mlme->wfd_go_probe_resp_ie_len;
		break;
	case MLME_ASSOC_REQ_IE:
		t_ie = &mlme->wfd_assoc_req_ie;
		t_ie_len = &mlme->wfd_assoc_req_ie_len;
		break;
	case MLME_ASSOC_RESP_IE:
		t_ie = &mlme->wfd_assoc_resp_ie;
		t_ie_len = &mlme->wfd_assoc_resp_ie_len;
		break;
	default:
		RTW_PRINT(FUNC_ADPT_FMT" unsupported type:%u"
			  , FUNC_ADPT_ARG(adapter), type);
		rtw_warn_on(1);
		goto exit;
	}

	if (*t_ie) {
		u32 free_len = *t_ie_len;
		*t_ie_len = 0;
		rtw_mfree(*t_ie, free_len);
		*t_ie = NULL;
	}

	if (!clear) {
		*t_ie = rtw_malloc(ie_len);
		if (*t_ie == NULL) {
			RTW_ERR(FUNC_ADPT_FMT" type:%u, rtw_malloc() fail\n"
				, FUNC_ADPT_ARG(adapter), type);
			goto exit;
		}
		_rtw_memcpy(*t_ie, ie, ie_len);
		*t_ie_len = ie_len;
	}

	if (*t_ie && *t_ie_len) {
		u8 *attr_content;
		u32 attr_contentlen = 0;

		attr_content = rtw_get_wfd_attr_content(*t_ie, *t_ie_len, WFD_ATTR_DEVICE_INFO, NULL, &attr_contentlen);
		if (attr_content && attr_contentlen) {
			if (RTW_GET_BE16(attr_content + 2) != wfd_info->rtsp_ctrlport) {
				wfd_info->rtsp_ctrlport = RTW_GET_BE16(attr_content + 2);
				RTW_INFO(FUNC_ADPT_FMT" type:%u, RTSP CTRL port = %u\n"
					, FUNC_ADPT_ARG(adapter), type, wfd_info->rtsp_ctrlport);
			}
		}
	}

success:
	ret = _SUCCESS;

exit:
	return ret;
}
#endif /* defined(CONFIG_WFD) && defined(CONFIG_IOCTL_CFG80211) */

void _rtw_free_mlme_priv(struct mlme_priv *pmlmepriv)
{
	_adapter *adapter = mlme_to_adapter(pmlmepriv);
	if (NULL == pmlmepriv) {
		rtw_warn_on(1);
		goto exit;
	}
	rtw_free_mlme_priv_ie_data(pmlmepriv);

	if (pmlmepriv) {
		rtw_mfree_mlme_priv_lock(pmlmepriv);

		if (pmlmepriv->free_bss_buf)
			rtw_vmfree(pmlmepriv->free_bss_buf, pmlmepriv->max_bss_cnt * sizeof(struct wlan_network));

#ifdef CONFIG_RTW_MULTI_AP
		if (is_primary_adapter(adapter)) {
			if (pmlmepriv->free_unassoc_sta_buf)
				rtw_vmfree(pmlmepriv->free_unassoc_sta_buf, pmlmepriv->max_unassoc_sta_cnt * sizeof(struct unassoc_sta_info));
		}
#endif
	}
exit:
	return;
}

sint	_rtw_enqueue_network(_queue *queue, struct wlan_network *pnetwork)
{

	if (pnetwork == NULL)
		goto exit;

	_rtw_spinlock_bh(&queue->lock);

	rtw_list_insert_tail(&pnetwork->list, &queue->queue);

	_rtw_spinunlock_bh(&queue->lock);

exit:


	return _SUCCESS;
}

/*
struct	wlan_network *_rtw_dequeue_network(_queue *queue)
{

	struct wlan_network *pnetwork;


	_rtw_spinlock_bh(&queue->lock);

	if (_rtw_queue_empty(queue) == _TRUE)

		pnetwork = NULL;

	else
	{
		pnetwork = LIST_CONTAINOR(get_next(&queue->queue), struct wlan_network, list);

		rtw_list_delete(&(pnetwork->list));
	}

	_rtw_spinunlock_bh(&queue->lock);


	return pnetwork;
}
*/

struct	wlan_network *_rtw_alloc_network(struct	mlme_priv *pmlmepriv) /* (_queue *free_queue) */
{
	struct	wlan_network	*pnetwork;
	_queue *free_queue = &pmlmepriv->free_bss_pool;
	_list *plist = NULL;


	_rtw_spinlock_bh(&free_queue->lock);

	if (_rtw_queue_empty(free_queue) == _TRUE) {
		pnetwork = NULL;
		goto exit;
	}
	plist = get_next(&(free_queue->queue));

	pnetwork = LIST_CONTAINOR(plist , struct wlan_network, list);

	rtw_list_delete(&pnetwork->list);

	pnetwork->network_type = 0;
	pnetwork->fixed = _FALSE;
	pnetwork->last_scanned = rtw_get_current_time();
	pnetwork->last_non_hidden_ssid_ap = pnetwork->last_scanned;
#if defined(CONFIG_RTW_MESH) && CONFIG_RTW_MESH_ACNODE_PREVENT
	pnetwork->acnode_stime = 0;
	pnetwork->acnode_notify_etime = 0;
#endif

	pnetwork->aid = 0;
	pnetwork->join_res = 0;

	pmlmepriv->num_of_scanned++;

exit:
	_rtw_spinunlock_bh(&free_queue->lock);


	return pnetwork;
}

void _rtw_free_network(struct	mlme_priv *pmlmepriv , struct wlan_network *pnetwork, u8 isfreeall)
{
	u32 delta_time;
	u32 lifetime = SCANQUEUE_LIFETIME;
	_queue *free_queue = &(pmlmepriv->free_bss_pool);


	if (pnetwork == NULL)
		goto exit;

	if (pnetwork->fixed == _TRUE)
		goto exit;

	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) == _TRUE) ||
	    (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE))
		lifetime = 1;

	if (!isfreeall) {
		delta_time = (u32) rtw_get_passing_time_ms(pnetwork->last_scanned);
		if (delta_time < lifetime) /* unit:msec */
			goto exit;
	}

	_rtw_spinlock_bh(&free_queue->lock);

	rtw_list_delete(&(pnetwork->list));

	rtw_list_insert_tail(&(pnetwork->list), &(free_queue->queue));

	pmlmepriv->num_of_scanned--;


	/* RTW_INFO("_rtw_free_network:SSID=%s\n", pnetwork->network.Ssid.Ssid); */

	_rtw_spinunlock_bh(&free_queue->lock);

exit:
	return;
}

void _rtw_free_network_nolock(struct	mlme_priv *pmlmepriv, struct wlan_network *pnetwork)
{

	_queue *free_queue = &(pmlmepriv->free_bss_pool);


	if (pnetwork == NULL)
		goto exit;

	if (pnetwork->fixed == _TRUE)
		goto exit;

	/* _rtw_spinlock_irq(&free_queue->lock, &sp_flags); */

	rtw_list_delete(&(pnetwork->list));

	rtw_list_insert_tail(&(pnetwork->list), get_list_head(free_queue));

	pmlmepriv->num_of_scanned--;

	/* _rtw_spinunlock_irq(&free_queue->lock, &sp_flags); */

exit:
	return;
}

void _rtw_free_network_queue(_adapter *padapter, u8 isfreeall)
{

	_list *phead, *plist;
	struct wlan_network *pnetwork;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	_queue *scanned_queue = &pmlmepriv->scanned_queue;



	_rtw_spinlock_bh(&scanned_queue->lock);

	phead = get_list_head(scanned_queue);
	plist = get_next(phead);

	while (rtw_end_of_queue_search(phead, plist) == _FALSE) {

		pnetwork = LIST_CONTAINOR(plist, struct wlan_network, list);

		plist = get_next(plist);

		_rtw_free_network(pmlmepriv, pnetwork, isfreeall);

	}

	_rtw_spinunlock_bh(&scanned_queue->lock);


}




sint rtw_if_up(_adapter *padapter)
{

	sint res;

	if (RTW_CANNOT_RUN(adapter_to_dvobj(padapter)) ||
	    (check_fwstate(&padapter->mlmepriv, WIFI_ASOC_STATE) == _FALSE)) {
		res = _FALSE;
	} else
		res =  _TRUE;

	return res;
}


void rtw_generate_random_ibss(u8 *pibss)
{
	*((u32 *)(&pibss[2])) = rtw_random32();
	pibss[0] = 0x02; /* in ad-hoc mode local bit must set to 1 */
	pibss[1] = 0x11;
	pibss[2] = 0x87;
}

u8 *rtw_get_capability_from_ie(u8 *ie)
{
	return ie + 8 + 2;
}


u16 rtw_get_capability(WLAN_BSSID_EX *bss)
{
	u16	val;

	_rtw_memcpy((u8 *)&val, rtw_get_capability_from_ie(bss->IEs), 2);

	return le16_to_cpu(val);
}

u8 *rtw_get_timestampe_from_ie(u8 *ie)
{
	return ie + 0;
}

u8 *rtw_get_beacon_interval_from_ie(u8 *ie)
{
	return ie + 8;
}


int	rtw_init_mlme_priv(_adapter *padapter) /* (struct	mlme_priv *pmlmepriv) */
{
	int	res;
	res = _rtw_init_mlme_priv(padapter);/* (pmlmepriv); */
	return res;
}

void rtw_free_mlme_priv(struct mlme_priv *pmlmepriv)
{
	_rtw_free_mlme_priv(pmlmepriv);
}

int rtw_enqueue_network(_queue *queue, struct wlan_network *pnetwork)
{
	int	res;
	res = _rtw_enqueue_network(queue, pnetwork);
	return res;
}

void rtw_free_network(struct mlme_priv *pmlmepriv, struct	wlan_network *pnetwork, u8 is_freeall)/* (struct	wlan_network *pnetwork, _queue	*free_queue) */
{
	_rtw_free_network(pmlmepriv, pnetwork, is_freeall);
}

void rtw_free_network_nolock(_adapter *padapter, struct wlan_network *pnetwork)
{
	_rtw_free_network_nolock(&(padapter->mlmepriv), pnetwork);
#ifdef CONFIG_IOCTL_CFG80211
	rtw_cfg80211_unlink_bss(padapter, pnetwork);
#endif /* CONFIG_IOCTL_CFG80211 */
}


void rtw_free_network_queue(_adapter *dev, u8 isfreeall)
{
	_rtw_free_network_queue(dev, isfreeall);
}

struct wlan_network *_rtw_find_network(_queue *scanned_queue, const u8 *addr)
{
	_list	*phead, *plist;
	struct	wlan_network *pnetwork = NULL;
	u8 zero_addr[ETH_ALEN] = {0, 0, 0, 0, 0, 0};

	if (_rtw_memcmp(zero_addr, addr, ETH_ALEN)) {
		pnetwork = NULL;
		goto exit;
	}

	phead = get_list_head(scanned_queue);
	plist = get_next(phead);

	while (plist != phead) {
		pnetwork = LIST_CONTAINOR(plist, struct wlan_network , list);

		if (_rtw_memcmp(addr, pnetwork->network.MacAddress, ETH_ALEN) == _TRUE)
			break;

		plist = get_next(plist);
	}

	if (plist == phead)
		pnetwork = NULL;

exit:
	return pnetwork;
}

struct wlan_network *rtw_find_network(_queue *scanned_queue, const u8 *addr)
{
	struct	wlan_network *pnetwork;

	 _rtw_spinlock_bh(&scanned_queue->lock);
	pnetwork = _rtw_find_network(scanned_queue, addr);
	_rtw_spinunlock_bh(&scanned_queue->lock);

	return pnetwork;
}

int rtw_is_same_ibss(_adapter *adapter, struct wlan_network *pnetwork)
{
	int ret = _TRUE;
	struct security_priv *psecuritypriv = &adapter->securitypriv;

	if ((psecuritypriv->dot11PrivacyAlgrthm != _NO_PRIVACY_) &&
	    (pnetwork->network.Privacy == 0))
		ret = _FALSE;
	else if ((psecuritypriv->dot11PrivacyAlgrthm == _NO_PRIVACY_) &&
		 (pnetwork->network.Privacy == 1))
		ret = _FALSE;
	else
		ret = _TRUE;

	return ret;

}

inline int is_same_ess(WLAN_BSSID_EX *a, WLAN_BSSID_EX *b)
{
	return (a->Ssid.SsidLength == b->Ssid.SsidLength)
	       &&  _rtw_memcmp(a->Ssid.Ssid, b->Ssid.Ssid, a->Ssid.SsidLength) == _TRUE;
}

int is_same_network(WLAN_BSSID_EX *src, WLAN_BSSID_EX *dst)
{
	u16 s_cap, d_cap;


	_rtw_memcpy((u8 *)&s_cap, rtw_get_capability_from_ie(src->IEs), 2);
	_rtw_memcpy((u8 *)&d_cap, rtw_get_capability_from_ie(dst->IEs), 2);


	s_cap = le16_to_cpu(s_cap);
	d_cap = le16_to_cpu(d_cap);

	/* Wi-Fi driver doesn't consider the situation of BCN and ProbRsp sent from the same hidden AP, 
	  * it considers these two packets are sent from different AP. 
	  * Therefore, the scan queue may store two scan results of the same hidden AP, likes below.
	  *
	  *  index            bssid              ch    RSSI   SdBm  Noise   age          flag             ssid
	  *    1    00:e0:4c:55:50:01    153   -73     -73        0     7044   [WPS][ESS]     RTK5G
	  *    3    00:e0:4c:55:50:01    153   -73     -73        0     7044   [WPS][ESS]
	  *
	  * Original rules will compare Ssid, SsidLength, MacAddress, s_cap, d_cap at the same time.
	  * Wi-Fi driver will assume that the BCN and ProbRsp sent from the same hidden AP are the same network
	  * after we add an additional rule to compare SsidLength and Ssid.
	  * It means the scan queue will not store two scan results of the same hidden AP, it only store ProbRsp.
	  * For customer request.
	  */
	  
	if (((_rtw_memcmp(src->MacAddress, dst->MacAddress, ETH_ALEN)) == _TRUE) &&
		((s_cap & WLAN_CAPABILITY_IBSS) == (d_cap & WLAN_CAPABILITY_IBSS)) &&
		((s_cap & WLAN_CAPABILITY_BSS) == (d_cap & WLAN_CAPABILITY_BSS))) {
		if ((src->Ssid.SsidLength == dst->Ssid.SsidLength) && 
			(((_rtw_memcmp(src->Ssid.Ssid, dst->Ssid.Ssid, src->Ssid.SsidLength)) == _TRUE) || //Case of normal AP
			(is_all_null(src->Ssid.Ssid, src->Ssid.SsidLength) == _TRUE || is_all_null(dst->Ssid.Ssid, dst->Ssid.SsidLength) == _TRUE))) //Case of hidden AP
			return _TRUE;
		else if ((src->Ssid.SsidLength == 0 || dst->Ssid.SsidLength == 0)) //Case of hidden AP
			return _TRUE;
		else
			return _FALSE;
	} else {
		return _FALSE;
	}
}

struct wlan_network *_rtw_find_same_network(_queue *scanned_queue, struct wlan_network *network)
{
	_list *phead, *plist;
	struct wlan_network *found = NULL;

	phead = get_list_head(scanned_queue);
	plist = get_next(phead);

	while (plist != phead) {
		found = LIST_CONTAINOR(plist, struct wlan_network , list);

		if (is_same_network(&network->network, &found->network))
			break;

		plist = get_next(plist);
	}

	if (plist == phead)
		found = NULL;

	return found;
}

struct wlan_network *rtw_find_same_network(_queue *scanned_queue, struct wlan_network *network)
{
	struct wlan_network *found = NULL;

	if (scanned_queue == NULL || network == NULL)
		goto exit;

	_rtw_spinlock_bh(&scanned_queue->lock);
	found = _rtw_find_same_network(scanned_queue, network);
	_rtw_spinunlock_bh(&scanned_queue->lock);

exit:
	return found;
}

struct	wlan_network	*rtw_get_oldest_wlan_network(_queue *scanned_queue)
{
	_list	*plist, *phead;


	struct	wlan_network	*pwlan = NULL;
	struct	wlan_network	*oldest = NULL;
	phead = get_list_head(scanned_queue);

	plist = get_next(phead);

	while (1) {

		if (rtw_end_of_queue_search(phead, plist) == _TRUE)
			break;

		pwlan = LIST_CONTAINOR(plist, struct wlan_network, list);

		if (pwlan->fixed != _TRUE) {
			if (oldest == NULL || rtw_time_after(oldest->last_scanned, pwlan->last_scanned))
				oldest = pwlan;
		}

		plist = get_next(plist);
	}
	return oldest;

}

void rtw_update_network(WLAN_BSSID_EX *dst, WLAN_BSSID_EX *src,
		    _adapter *padapter, bool update_ie)
{
#if defined(DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) && 1
	u8 ss_ori = dst->PhyInfo.SignalStrength;
	u8 sq_ori = dst->PhyInfo.SignalQuality;
	u8 ss_smp = src->PhyInfo.SignalStrength;
	s8 rssi_smp = src->PhyInfo.rssi;
#endif
	s8 rssi_ori = dst->PhyInfo.rssi;

	u8 sq_smp = src->PhyInfo.SignalQuality;
	u8 ss_final;
	u8 sq_final;
	s8 rssi_final;


#ifdef CONFIG_ANTENNA_DIVERSITY
	rtw_hal_antdiv_rssi_compared(padapter, dst, src); /* this will update src->PhyInfo.rssi, need consider again */
#endif

#if defined(DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) && 1
	if (strcmp(dst->Ssid.Ssid, DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) == 0) {
		RTW_INFO(FUNC_ADPT_FMT" %s("MAC_FMT", ch%u) ss_ori:%3u, sq_ori:%3u, rssi_ori:%d, ss_smp:%3u, sq_smp:%3u, rssi_smp:%d\n"
			 , FUNC_ADPT_ARG(padapter)
			, src->Ssid.Ssid, MAC_ARG(src->MacAddress), src->Configuration.DSConfig
			 , ss_ori, sq_ori, rssi_ori
			 , ss_smp, sq_smp, rssi_smp
			);
	}
#endif

	/* The rule below is 1/5 for sample value, 4/5 for history value */
	if (check_fwstate(&padapter->mlmepriv, WIFI_ASOC_STATE) && is_same_network(&(padapter->mlmepriv.cur_network.network), src)) {
		/* Take the recvpriv's value for the connected AP*/
		ss_final = padapter->recvinfo.signal_strength;
		sq_final = padapter->recvinfo.signal_qual;
		/* the rssi value here is undecorated, and will be used for antenna diversity */
		if (sq_smp != 101) /* from the right channel */
			rssi_final = (src->PhyInfo.rssi + dst->PhyInfo.rssi * 4) / 5;
		else
			rssi_final = rssi_ori;
	} else {
		if (sq_smp != 101) { /* from the right channel */
			ss_final = ((u32)(src->PhyInfo.SignalStrength) + (u32)(dst->PhyInfo.SignalStrength) * 4) / 5;
			sq_final = ((u32)(src->PhyInfo.SignalQuality) + (u32)(dst->PhyInfo.SignalQuality) * 4) / 5;
			rssi_final = (src->PhyInfo.rssi + dst->PhyInfo.rssi * 4) / 5;
		} else {
			/* bss info not receving from the right channel, use the original RX signal infos */
			ss_final = dst->PhyInfo.SignalStrength;
			sq_final = dst->PhyInfo.SignalQuality;
			rssi_final = dst->PhyInfo.rssi;
		}

	}

	if (update_ie) {
		dst->Reserved[0] = src->Reserved[0];
		dst->Reserved[1] = src->Reserved[1];
		_rtw_memcpy((u8 *)dst, (u8 *)src, get_WLAN_BSSID_EX_sz(src));
	}

	dst->PhyInfo.SignalStrength = ss_final;
	dst->PhyInfo.SignalQuality = sq_final;
	dst->PhyInfo.rssi = rssi_final;

#if defined(DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) && 1
	if (strcmp(dst->Ssid.Ssid, DBG_RX_SIGNAL_DISPLAY_SSID_MONITORED) == 0) {
		RTW_INFO(FUNC_ADPT_FMT" %s("MAC_FMT"), SignalStrength:%u, SignalQuality:%u, RawRSSI:%d\n"
			 , FUNC_ADPT_ARG(padapter)
			, dst->Ssid.Ssid, MAC_ARG(dst->MacAddress),
			dst->PhyInfo.SignalStrength, dst->PhyInfo.SignalQuality, dst->PhyInfo.rssi);
	}
#endif
}

#ifdef CONFIG_RTW_MULTI_AP
void rtw_map_config_monitor_act_non(_adapter *adapter)
{
	rtw_map_config_monitor(adapter, MLME_ACTION_NONE);
}

void rtw_map_config_monitor(_adapter *adapter, u8 self_act)
{
	struct mi_state mstate, mstate_s;
	void *phl = GET_PHL_INFO(adapter_to_dvobj(adapter));

	rtw_mi_status_no_self(adapter, &mstate);
	rtw_mi_status_no_others(adapter, &mstate_s);

	switch (self_act) {
	case MLME_AP_STARTED:
		mstate_s.ap_num = 1;
		break;
	case MLME_AP_STOPPED:
		mstate_s.ap_num = 0;
		break;
	case MLME_ACTION_NONE:
		break;
	default:
		break;
	}

	rtw_mi_status_merge(&mstate, &mstate_s);
	if (MSTATE_AP_NUM(&mstate) &&
	    rtw_unassoc_sta_src_chk(adapter, UNASOC_STA_SRC_RX_NMY_UC))
		rtw_phl_enter_mon_mode(phl, adapter->phl_role);
	else
		rtw_phl_leave_mon_mode(phl, adapter->phl_role);
}

void rtw_unassoc_sta_set_mode(_adapter *adapter, u8 stype, u8 mode)
{
	if (stype >= UNASOC_STA_SRC_NUM || mode >= UNASOC_STA_MODE_NUM)
		return;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	if (adapter->mlmepriv.unassoc_sta_mode_of_stype[stype] == mode)
		return;

	adapter->mlmepriv.unassoc_sta_mode_of_stype[stype] = mode;
	rtw_run_in_thread_cmd_wait(adapter, ((void *)(rtw_map_config_monitor_act_non)), adapter, 2000);
}

bool rtw_unassoc_sta_src_chk(_adapter *adapter, u8 stype)
{
	if (stype >= UNASOC_STA_SRC_NUM)
		return 0;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	return adapter->mlmepriv.unassoc_sta_mode_of_stype[stype] == UNASOC_STA_MODE_ALL
		|| (adapter->mlmepriv.unassoc_sta_mode_of_stype[stype] == UNASOC_STA_MODE_INTERESTED
		&& adapter->mlmepriv.interested_unassoc_sta_cnt);
}

const char *unasoc_sta_src_str[] = {
	"BMC",
	"NMY_UC",
};

const char *unasoc_sta_mode_str[] = {
	"DISABLED",
	"INTERESTED",
	"ALL",
};

void dump_unassoc_sta(void *sel, _adapter *adapter)
{
	struct mlme_priv *mlmepriv;
	_queue *queue;
	_list *list, *head;
	struct unassoc_sta_info **unassoc_sta_arr;
	struct unassoc_sta_info *unassoc_sta;
	u16 i, unassoc_sta_cnt = 0;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	mlmepriv = &(adapter->mlmepriv);
	queue = &(mlmepriv->unassoc_sta_queue);
	for (i = 0; i < UNASOC_STA_SRC_NUM; i++) {
		RTW_PRINT_SEL(sel, "[%u]%-6s:%u(%s)\n", i, unasoc_sta_src_str[i],
			      mlmepriv->unassoc_sta_mode_of_stype[i],
			      unasoc_sta_mode_str[mlmepriv->unassoc_sta_mode_of_stype[i]]);
	}

	RTW_PRINT_SEL(sel, "interested_unassoc_sta_cnt:%u\n",
		      mlmepriv->interested_unassoc_sta_cnt);

	unassoc_sta_arr = rtw_zvmalloc(mlmepriv->max_unassoc_sta_cnt * sizeof(struct unassoc_sta_info *));
	if (!unassoc_sta_arr)
		return;

	_rtw_spinlock_bh(&queue->lock);
	head = get_list_head(queue);
	list = get_next(head);
	while (rtw_end_of_queue_search(head, list) == _FALSE) {
		unassoc_sta = LIST_CONTAINOR(list, struct unassoc_sta_info, list);
		list = get_next(list);
		unassoc_sta_arr[unassoc_sta_cnt++] = unassoc_sta;
	}

	_rtw_spinunlock_bh(&queue->lock);
	RTW_PRINT_SEL(sel, "  %17s %18s %6s\n", "mac_addr", "measure_delta_time", "rssi");

	for (i = 0; i < unassoc_sta_cnt; i++) {
		u8 rcpi;
		s8 rx_power;
		u32 measure_delta_time;

		unassoc_sta = unassoc_sta_arr[i];
		measure_delta_time = rtw_systime_to_ms(rtw_get_current_time() - unassoc_sta->time);
		RTW_PRINT_SEL(sel, "%c "MAC_FMT" %18u %6d\n"
			      , unassoc_sta->interested ? '*' : ' '
			      , MAC_ARG(unassoc_sta->addr), measure_delta_time,
			      unassoc_sta->recv_signal_power);
	}

	rtw_vmfree(unassoc_sta_arr, mlmepriv->max_unassoc_sta_cnt * sizeof(struct unassoc_sta_info *));
}

static void del_unassoc_sta(struct mlme_priv *mlmepriv, struct unassoc_sta_info *unassoc_sta)
{
	_queue *free_queue = &(mlmepriv->free_unassoc_sta_queue);

	if (unassoc_sta->interested)
		mlmepriv->interested_unassoc_sta_cnt--;

	if (mlmepriv->interested_unassoc_sta_cnt == 0) {
		rtw_run_in_thread_cmd(mlme_to_adapter(mlmepriv)
			, ((void *)(rtw_map_config_monitor_act_non)), mlme_to_adapter(mlmepriv));
	}

	_rtw_spinlock_bh(&free_queue->lock);
	rtw_list_delete(&(unassoc_sta->list));
	rtw_list_insert_tail(&(unassoc_sta->list), &(free_queue->queue));
	_rtw_spinunlock_bh(&free_queue->lock);
}

static u8 del_unassoc_sta_chk(struct mlme_priv *mlmepriv, struct unassoc_sta_info *unassoc_sta)
{
	systime cur, lifetime;

	if (unassoc_sta == NULL)
		return UNASOC_STA_DEL_CHK_SKIP;

	if (unassoc_sta->interested)
		return UNASOC_STA_DEL_CHK_SKIP;

	cur = rtw_get_current_time();
	lifetime = unassoc_sta->time + rtw_ms_to_systime(UNASSOC_STA_LIFETIME_MS);
	if (rtw_time_before(cur, lifetime))
		return UNASOC_STA_DEL_CHK_ALIVE;

	del_unassoc_sta(mlmepriv, unassoc_sta);
	return UNASOC_STA_DEL_CHK_DELETED;
}

static struct unassoc_sta_info *alloc_unassoc_sta(struct mlme_priv *mlmepriv)
{
	struct	unassoc_sta_info *unassoc_sta;
	_queue *free_queue = &mlmepriv->free_unassoc_sta_queue;
	_list *list = NULL;

	_rtw_spinlock_bh(&free_queue->lock);
	if (_rtw_queue_empty(free_queue) == _TRUE) {
		unassoc_sta = NULL;
		goto exit;
	}

	list = get_next(&(free_queue->queue));
	unassoc_sta = LIST_CONTAINOR(list, struct unassoc_sta_info, list);
	rtw_list_delete(&unassoc_sta->list);
	_rtw_memset(unassoc_sta->addr, 0, ETH_ALEN);
	unassoc_sta->recv_signal_power = 0;
	unassoc_sta->time = 0;
	unassoc_sta->interested = 0;
exit:
	_rtw_spinunlock_bh(&free_queue->lock);
	return unassoc_sta;
}

void rtw_del_unassoc_sta_queue(_adapter *adapter)
{
	struct unassoc_sta_info *unassoc_sta;
	struct mlme_priv *mlmepriv;
	_queue *queue;
	_list *head, *list;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	mlmepriv = &(adapter->mlmepriv);
	queue = &(mlmepriv->unassoc_sta_queue);

	_rtw_spinlock_bh(&queue->lock);
	head = get_list_head(queue);
	list = get_next(head);
	while ((rtw_end_of_queue_search(head, list)) == _FALSE) {
		unassoc_sta = LIST_CONTAINOR(list , struct unassoc_sta_info, list);
		list = get_next(list);
		del_unassoc_sta(mlmepriv, unassoc_sta);
	}

	_rtw_spinunlock_bh(&queue->lock);
}

void rtw_del_unassoc_sta(_adapter *adapter, u8 *addr)
{
	struct unassoc_sta_info *unassoc_sta;
	struct mlme_priv *mlmepriv;
	_queue *queue;
	_list *head, *list;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	mlmepriv = &(adapter->mlmepriv);
	queue = &(mlmepriv->unassoc_sta_queue);

	_rtw_spinlock_bh(&queue->lock);

	head = get_list_head(queue);
	list = get_next(head);

	while ((rtw_end_of_queue_search(head, list)) == _FALSE) {
		unassoc_sta = LIST_CONTAINOR(list , struct unassoc_sta_info, list);
		list = get_next(list);

		if (_rtw_memcmp(addr, unassoc_sta->addr, ETH_ALEN) == _TRUE) {
			del_unassoc_sta(mlmepriv, unassoc_sta);
			goto unlock_unassoc_sta_queue;
		}
	}

unlock_unassoc_sta_queue:
	_rtw_spinunlock_bh(&queue->lock);
}

void rtw_rx_add_unassoc_sta(_adapter *adapter, u8 stype, u8 *addr, s8 recv_signal_power)
{
	struct unassoc_sta_info *unassoc_sta;
	struct unassoc_sta_info *oldest_unassoc_sta = NULL;
	struct mlme_priv *mlmepriv;
	_queue *queue;
	_list *head, *list;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	mlmepriv = &(adapter->mlmepriv);
	queue = &(mlmepriv->unassoc_sta_queue);

	_rtw_spinlock_bh(&queue->lock);
	head = get_list_head(queue);
	list = get_next(head);
	while ((rtw_end_of_queue_search(head, list)) == _FALSE) {
		unassoc_sta = LIST_CONTAINOR(list , struct unassoc_sta_info, list);
		list = get_next(list);
		if (_rtw_memcmp(addr, unassoc_sta->addr, ETH_ALEN) == _TRUE) {
			if (unassoc_sta->interested ||
			    mlmepriv->unassoc_sta_mode_of_stype[stype] >=
			    UNASOC_STA_MODE_ALL) {
				unassoc_sta->recv_signal_power = recv_signal_power;
				unassoc_sta->time = rtw_get_current_time();
				goto unlock_unassoc_sta_queue;
			}
		}

		if (del_unassoc_sta_chk(mlmepriv, unassoc_sta) == UNASOC_STA_DEL_CHK_ALIVE) {
			if (oldest_unassoc_sta == NULL)
				oldest_unassoc_sta = unassoc_sta;
			else if (rtw_time_before(unassoc_sta->time, oldest_unassoc_sta->time))
				oldest_unassoc_sta = unassoc_sta;
		}
	}

	if (mlmepriv->unassoc_sta_mode_of_stype[stype] <= UNASOC_STA_MODE_INTERESTED)
		goto unlock_unassoc_sta_queue;

	unassoc_sta = alloc_unassoc_sta(mlmepriv);
	if (unassoc_sta == NULL) {
		if (oldest_unassoc_sta) {
			del_unassoc_sta(mlmepriv, oldest_unassoc_sta);
			unassoc_sta = alloc_unassoc_sta(mlmepriv);
		} else
			goto unlock_unassoc_sta_queue;
	}

	_rtw_memcpy(unassoc_sta->addr, addr, ETH_ALEN);
	unassoc_sta->recv_signal_power = recv_signal_power;
	unassoc_sta->time = rtw_get_current_time();
	rtw_list_insert_tail(&(unassoc_sta->list), &(queue->queue));

unlock_unassoc_sta_queue:
	_rtw_spinunlock_bh(&queue->lock);
}

void rtw_add_interested_unassoc_sta(_adapter *adapter, u8 *addr)
{
	struct unassoc_sta_info *unassoc_sta;
	struct unassoc_sta_info *oldest_unassoc_sta = NULL;
	struct mlme_priv *mlmepriv;
	_queue *queue;
	_list *head, *list;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	mlmepriv = &(adapter->mlmepriv);
	queue = &(mlmepriv->unassoc_sta_queue);

	_rtw_spinlock_bh(&queue->lock);

	head = get_list_head(queue);
	list = get_next(head);

	while ((rtw_end_of_queue_search(head, list)) == _FALSE) {
		unassoc_sta = LIST_CONTAINOR(list , struct unassoc_sta_info, list);
		list = get_next(list);

		if (_rtw_memcmp(addr, unassoc_sta->addr, ETH_ALEN) == _TRUE) {
			if (!unassoc_sta->interested) {
				unassoc_sta->interested = 1;
				mlmepriv->interested_unassoc_sta_cnt++;
				if (mlmepriv->interested_unassoc_sta_cnt == 1) {
					rtw_run_in_thread_cmd(mlme_to_adapter(mlmepriv)
						, ((void *)(rtw_map_config_monitor_act_non)), mlme_to_adapter(mlmepriv));
				}
			}
			goto unlock_unassoc_sta_queue;
		}

		if (del_unassoc_sta_chk(mlmepriv, unassoc_sta) == UNASOC_STA_DEL_CHK_ALIVE) {
			if (oldest_unassoc_sta == NULL)
				oldest_unassoc_sta = unassoc_sta;
			else if (rtw_time_after(unassoc_sta->time, oldest_unassoc_sta->time))
				oldest_unassoc_sta = unassoc_sta;
		}
	}
	unassoc_sta = alloc_unassoc_sta(mlmepriv);
	if (unassoc_sta == NULL) {
		RTW_INFO(FUNC_ADPT_FMT": Allocate fail\n", FUNC_ADPT_ARG(adapter));
		if (oldest_unassoc_sta) {
			RTW_INFO(FUNC_ADPT_FMT": Delete oldest entry and try again.\n", FUNC_ADPT_ARG(adapter));
			del_unassoc_sta(mlmepriv, oldest_unassoc_sta);
			unassoc_sta = alloc_unassoc_sta(mlmepriv);
		} else
			goto unlock_unassoc_sta_queue;
	}

	_rtw_memcpy(unassoc_sta->addr, addr, ETH_ALEN);
	unassoc_sta->interested = 1;
	unassoc_sta->recv_signal_power = 0;
	unassoc_sta->time = rtw_get_current_time() - rtw_ms_to_systime(UNASSOC_STA_LIFETIME_MS);
	rtw_list_insert_tail(&(unassoc_sta->list), &(queue->queue));
	mlmepriv->interested_unassoc_sta_cnt++;
	if (mlmepriv->interested_unassoc_sta_cnt == 1) {
		rtw_run_in_thread_cmd(mlme_to_adapter(mlmepriv)
			, ((void *)(rtw_map_config_monitor_act_non)), mlme_to_adapter(mlmepriv));
	}

unlock_unassoc_sta_queue:
	_rtw_spinunlock_bh(&queue->lock);
}

void rtw_undo_interested_unassoc_sta(_adapter *adapter, u8 *addr)
{
	struct unassoc_sta_info *unassoc_sta;
	struct mlme_priv *mlmepriv;
	_queue *queue;
	_list *head, *list;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	mlmepriv = &(adapter->mlmepriv);
	queue = &(mlmepriv->unassoc_sta_queue);

	_rtw_spinlock_bh(&queue->lock);
	head = get_list_head(queue);
	list = get_next(head);

	while ((rtw_end_of_queue_search(head, list)) == _FALSE) {
		unassoc_sta = LIST_CONTAINOR(list , struct unassoc_sta_info, list);
		list = get_next(list);

		if (_rtw_memcmp(addr, unassoc_sta->addr, ETH_ALEN) == _TRUE) {
			if (unassoc_sta->interested) {
				unassoc_sta->interested = 0;
				mlmepriv->interested_unassoc_sta_cnt--;
				if (mlmepriv->interested_unassoc_sta_cnt == 0) {
					rtw_run_in_thread_cmd(mlme_to_adapter(mlmepriv)
						, ((void *)(rtw_map_config_monitor_act_non)), mlme_to_adapter(mlmepriv));
				}
			}

			goto unlock_unassoc_sta_queue;
		}
	}
unlock_unassoc_sta_queue:
	_rtw_spinunlock_bh(&queue->lock);
}

void rtw_undo_all_interested_unassoc_sta(_adapter *adapter)
{
	struct unassoc_sta_info *unassoc_sta;
	struct mlme_priv *mlmepriv;
	_queue *queue;
	_list *head, *list;

	adapter = GET_PRIMARY_ADAPTER(adapter);
	mlmepriv = &(adapter->mlmepriv);
	queue = &(mlmepriv->unassoc_sta_queue);

	_rtw_spinlock_bh(&queue->lock);
	head = get_list_head(queue);
	list = get_next(head);

	while ((rtw_end_of_queue_search(head, list)) == _FALSE) {
		unassoc_sta = LIST_CONTAINOR(list , struct unassoc_sta_info, list);
		list = get_next(list);

		if (unassoc_sta->interested) {
			unassoc_sta->interested = 0;
			mlmepriv->interested_unassoc_sta_cnt--;
			if (mlmepriv->interested_unassoc_sta_cnt == 0) {
				rtw_run_in_thread_cmd(mlme_to_adapter(mlmepriv)
					, ((void *)(rtw_map_config_monitor_act_non)), mlme_to_adapter(mlmepriv));
				goto unlock_unassoc_sta_queue;
			}
		}
	}
unlock_unassoc_sta_queue:
	_rtw_spinunlock_bh(&queue->lock);
}

#endif /* CONFIG_RTW_MULTI_AP */

/* select the desired network based on the capability of the (i)bss.
 * check items: (1) security
 *			   (2) network_type
 *			   (3) WMM
 *			   (4) HT
 * (5) others */
int rtw_is_desired_network(_adapter *adapter, struct wlan_network *pnetwork)
{
	struct security_priv *psecuritypriv = &adapter->securitypriv;
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	u32 desired_encmode;
	u32 privacy;

	/* u8 wps_ie[512]; */
	uint wps_ielen;

	int bselected = _TRUE;

	desired_encmode = psecuritypriv->ndisencryptstatus;
	privacy = pnetwork->network.Privacy;

	if (check_fwstate(pmlmepriv, WIFI_UNDER_WPS)) {
		if (rtw_get_wps_ie(pnetwork->network.IEs + _FIXED_IE_LENGTH_, pnetwork->network.IELength - _FIXED_IE_LENGTH_, NULL, &wps_ielen) != NULL)
			return _TRUE;
		else
			return _FALSE;
	}
	if (adapter->registrypriv.wifi_spec == 1) { /* for  correct flow of 8021X  to do.... */
		u8 *p = NULL;
		uint ie_len = 0;

		if ((desired_encmode == Ndis802_11EncryptionDisabled) && (privacy != 0))
			bselected = _FALSE;

		if (psecuritypriv->ndisauthtype == Ndis802_11AuthModeWPA2PSK) {
			p = rtw_get_ie(pnetwork->network.IEs + _BEACON_IE_OFFSET_, _RSN_IE_2_, &ie_len, (pnetwork->network.IELength - _BEACON_IE_OFFSET_));
			if (p && ie_len > 0)
				bselected = _TRUE;
			else
				bselected = _FALSE;
		}
	}


	if ((desired_encmode != Ndis802_11EncryptionDisabled) && (privacy == 0)) {
		RTW_INFO("desired_encmode: %d, privacy: %d\n", desired_encmode, privacy);
		bselected = _FALSE;
	}

	if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE) {
		if (pnetwork->network.InfrastructureMode != pmlmepriv->cur_network.network.InfrastructureMode)
			bselected = _FALSE;
	}

#ifdef CONFIG_RTW_MBO
	if (rtw_mbo_disallowed_network(pnetwork) == _TRUE)
		bselected = _FALSE;
#endif

	return bselected;
}

static void free_scanqueue(struct	mlme_priv *pmlmepriv)
{
	_queue *free_queue = &pmlmepriv->free_bss_pool;
	_queue *scan_queue = &pmlmepriv->scanned_queue;
	_list	*plist, *phead, *ptemp;


	_rtw_spinlock_bh(&scan_queue->lock);
	_rtw_spinlock_bh(&free_queue->lock);

	phead = get_list_head(scan_queue);
	plist = get_next(phead);

	while (plist != phead) {
		ptemp = get_next(plist);
		rtw_list_delete(plist);
		rtw_list_insert_tail(plist, &free_queue->queue);
		plist = ptemp;
		pmlmepriv->num_of_scanned--;
	}

	_rtw_spinunlock_bh(&free_queue->lock);
	_rtw_spinunlock_bh(&scan_queue->lock);

}

void rtw_reset_rx_info(_adapter *adapter)
{
	struct recv_info *precvinfo = &adapter->recvinfo;

	precvinfo->dbg_rx_ampdu_drop_count = 0;
	precvinfo->dbg_rx_ampdu_forced_indicate_count = 0;
	precvinfo->dbg_rx_ampdu_loss_count = 0;
	precvinfo->dbg_rx_dup_mgt_frame_drop_count = 0;
	precvinfo->dbg_rx_ampdu_window_shift_cnt = 0;
	precvinfo->dbg_rx_drop_count = 0;
	precvinfo->dbg_rx_conflic_mac_addr_cnt = 0;
}

/*
 * Note: There may be I/O in following functions:
 *	rtw_free_stainfo()
 *	rtw_init_self_stainfo()
 */
void rtw_free_assoc_resources(_adapter *adapter, u8 lock_scanned_queue)
{
	struct wlan_network *pwlan = NULL;
	struct	mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct wlan_network *tgt_network = &pmlmepriv->cur_network;


#ifdef CONFIG_TDLS
	struct tdls_info *ptdlsinfo = &adapter->tdlsinfo;
#endif /* CONFIG_TDLS */


	RTW_INFO("%s-"ADPT_FMT" tgt_network MacAddress=" MAC_FMT" ssid=%s\n",
		__func__, ADPT_ARG(adapter), MAC_ARG(tgt_network->network.MacAddress), tgt_network->network.Ssid.Ssid);

	if (MLME_IS_STA(adapter)) {
		struct sta_info *psta;

		psta = rtw_get_stainfo(&adapter->stapriv, tgt_network->network.MacAddress);
		if (!psta) {
			RTW_WARN("[TODO]" FUNC_ADPT_FMT ": fail to find stainfo"
				 "(" MAC_FMT ")\n",
				 FUNC_ADPT_ARG(adapter),
				 MAC_ARG(tgt_network->network.MacAddress));
		}

#ifdef CONFIG_TDLS
		rtw_free_all_tdls_sta(adapter, _TRUE);
		rtw_reset_tdls_info(adapter);

		if (ptdlsinfo->link_established == _TRUE)
			rtw_tdls_cmd(adapter, NULL, TDLS_RS_RCR);
#endif /* CONFIG_TDLS */

		/* _rtw_spinlock_bh(&(pstapriv->sta_hash_lock)); */
		rtw_free_stainfo(adapter, psta);
		rtw_init_self_stainfo(adapter);
		/* _rtw_spinunlock_bh(&(pstapriv->sta_hash_lock)); */

	}

	if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE)) {
		struct sta_info *psta;

		rtw_free_all_stainfo(adapter);
	}

	if (lock_scanned_queue)
		_rtw_spinlock_bh(&(pmlmepriv->scanned_queue.lock));

	if (check_fwstate(pmlmepriv, WIFI_UNDER_WPS) || (pmlmepriv->wpa_phase == _TRUE)){
		RTW_INFO("Dont free disconnecting network of scanned_queue due to uner %s %s phase\n\n",
			check_fwstate(pmlmepriv, WIFI_UNDER_WPS) ? "WPS" : "",
			(pmlmepriv->wpa_phase == _TRUE) ? "WPA" : "");
	} else {
		pwlan = _rtw_find_same_network(&pmlmepriv->scanned_queue, tgt_network);
		if (pwlan) {
			pwlan->fixed = _FALSE;

			RTW_INFO("Free disconnecting network of scanned_queue\n");
			rtw_free_network_nolock(adapter, pwlan);
#ifdef CONFIG_P2P
			if (rtw_p2p_chk_role(&adapter->wdinfo, P2P_ROLE_CLIENT) ||
			    rtw_p2p_chk_role(&adapter->wdinfo, P2P_ROLE_GO)) {
				rtw_set_scan_deny(adapter, 2000);
				/* rtw_clear_scan_deny(adapter); */
			}
#endif /* CONFIG_P2P */
		} else
			RTW_ERR("Free disconnecting network of scanned_queue failed due to pwlan == NULL\n\n");
	}

	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) && (adapter->stapriv.asoc_sta_count == 1))
	    /*||MLME_IS_STA(adapter)*/) {
		if (pwlan)
			rtw_free_network_nolock(adapter, pwlan);
	}

	if (lock_scanned_queue)
		_rtw_spinunlock_bh(&(pmlmepriv->scanned_queue.lock));

	adapter->securitypriv.key_mask = 0;

	rtw_reset_rx_info(adapter);


}

/*
*rtw_indicate_connect: the caller has to lock pmlmepriv->lock
*/
void rtw_indicate_connect(_adapter *padapter)
{
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;

	pmlmepriv->to_join = _FALSE;

	if (!check_fwstate(&padapter->mlmepriv, WIFI_ASOC_STATE)) {

		set_fwstate(pmlmepriv, WIFI_ASOC_STATE);

		rtw_led_control(padapter, LED_CTL_LINK);

		rtw_os_indicate_connect(padapter);

		#ifdef CONFIG_RTW_WDS
		if (MLME_IS_STA(padapter))
			rtw_wds_gptr_tbl_init(padapter);
		#endif
	}

	rtw_set_to_roam(padapter, 0);
	if (!MLME_IS_AP(padapter) && !MLME_IS_MESH(padapter))
		rtw_mi_set_scan_deny(padapter, 3000);


}


/*
*rtw_indicate_disconnect: the caller has to lock pmlmepriv->lock
*/
void rtw_indicate_disconnect(_adapter *padapter, u16 reason, u8 locally_generated)
{
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct mlme_ext_priv *pmlmeext = &(padapter->mlmeextpriv);
	struct mlme_ext_info *pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX	*cur_network = &(pmlmeinfo->network);
	u8 *wps_ie = NULL;
	uint wpsie_len = 0;

	if (check_fwstate(pmlmepriv, WIFI_UNDER_WPS))
		pmlmepriv->wpa_phase = _TRUE;

	_clr_fwstate_(pmlmepriv, WIFI_UNDER_LINKING | WIFI_UNDER_WPS | WIFI_OP_CH_SWITCHING | WIFI_UNDER_KEY_HANDSHAKE);

	/* force to clear cur_network_scanned's SELECTED REGISTRAR */
	if (pmlmepriv->cur_network_scanned) {
		WLAN_BSSID_EX	*current_joined_bss = &(pmlmepriv->cur_network_scanned->network);

		if (current_joined_bss) {
			wps_ie = rtw_get_wps_ie(current_joined_bss->IEs + _FIXED_IE_LENGTH_,
				current_joined_bss->IELength - _FIXED_IE_LENGTH_, NULL, &wpsie_len);
			if (wps_ie && wpsie_len > 0) {
				u8 *attr = NULL;
				u32 attr_len;
				attr = rtw_get_wps_attr(wps_ie, wpsie_len, WPS_ATTR_SELECTED_REGISTRAR,
							NULL, &attr_len);
				if (attr)
					*(attr + 4) = 0;
			}
		}
	}
	/* RTW_INFO("clear wps when %s\n", __func__); */

	if (rtw_to_roam(padapter) > 0)
		_clr_fwstate_(pmlmepriv, WIFI_ASOC_STATE);

#ifdef CONFIG_WAPI_SUPPORT
	if (MLME_IS_STA(padapter))
		rtw_wapi_return_one_sta_info(padapter, NULL);
	else if (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) ||
		 check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE))
		rtw_wapi_return_all_sta_info(padapter);
#endif

	if (check_fwstate(&padapter->mlmepriv, WIFI_ASOC_STATE)
	    || (rtw_to_roam(padapter) <= 0)
	   ) {
		#ifdef CONFIG_RTW_WDS
		adapter_set_use_wds(padapter, 0);
		rtw_wds_gptr_tbl_unregister(padapter);
		#endif
		#ifdef CONFIG_RTW_MULTI_AP
		padapter->multi_ap = 0;
		#endif

#ifdef CONFIG_RTW_TOKEN_BASED_XMIT
		if (ATOMIC_READ(&padapter->tbtx_tx_pause) == _TRUE) {
			ATOMIC_SET(&padapter->tbtx_tx_pause, _FALSE);
			rtw_tx_control_cmd(padapter);
		}
#endif
		/* rtw_phl_chanctx_del(adapter_to_dvobj(padapter)->phl, padapter->phl_role, NULL); */

		rtw_os_indicate_disconnect(padapter, reason, locally_generated);

		/* set ips_deny_time to avoid enter IPS before LPS leave */
		rtw_set_ips_deny(padapter, 3000);

		_clr_fwstate_(pmlmepriv, WIFI_ASOC_STATE);

		rtw_led_control(padapter, LED_CTL_NO_LINK);

		rtw_clear_scan_deny(padapter);
	}

#ifdef CONFIG_P2P_PS
	p2p_ps_wk_cmd(padapter, P2P_PS_DISABLE, 1);
#endif /* CONFIG_P2P_PS */

#ifdef CONFIG_LPS
	rtw_lps_ctrl_wk_cmd(padapter, LPS_CTRL_DISCONNECT, 0);
#endif
}

inline void rtw_indicate_scan_done(_adapter *padapter, bool aborted)
{
	RTW_INFO(FUNC_ADPT_FMT" aborted=%d\n", FUNC_ADPT_ARG(padapter), aborted);

	rtw_os_indicate_scan_done(padapter, aborted);

#ifdef CONFIG_IPS
	if (is_primary_adapter(padapter)
	    && (_FALSE == adapter_to_pwrctl(padapter)->bInSuspend)
	    && (check_fwstate(&padapter->mlmepriv, WIFI_ASOC_STATE | WIFI_UNDER_LINKING) == _FALSE)) {
		struct pwrctrl_priv *pwrpriv;

		pwrpriv = adapter_to_pwrctl(padapter);
		rtw_set_ips_deny(padapter, 0);
#ifdef CONFIG_IPS_CHECK_IN_WD
		#if 0 /*#ifdef CONFIG_CORE_DM_CHK_TIMER*/
		_set_timer(&adapter_to_dvobj(padapter)->dynamic_chk_timer, 1);
		#endif
#else /* !CONFIG_IPS_CHECK_IN_WD */
		_rtw_set_pwr_state_check_timer(pwrpriv, 1);
#endif /* !CONFIG_IPS_CHECK_IN_WD */
	}
#endif /* CONFIG_IPS */
}

static u32 _rtw_wait_join_done(_adapter *adapter, u8 abort, u32 timeout_ms)
{
	systime start;
	u32 pass_ms;
	struct mlme_priv *pmlmepriv = &(adapter->mlmepriv);
	struct mlme_ext_priv *pmlmeext = &(adapter->mlmeextpriv);

	start = rtw_get_current_time();

	pmlmeext->join_abort = abort;
	if (abort)
		set_link_timer(pmlmeext, 1);

	while (rtw_get_passing_time_ms(start) <= timeout_ms
		&& (check_fwstate(pmlmepriv, WIFI_UNDER_LINKING)
			#ifdef CONFIG_IOCTL_CFG80211
			|| rtw_cfg80211_is_connect_requested(adapter)
			#endif
			)
	) {
		if (RTW_CANNOT_RUN(adapter_to_dvobj(adapter)))
			break;

		RTW_INFO(FUNC_ADPT_FMT" linking...\n", FUNC_ADPT_ARG(adapter));
		rtw_msleep_os(20);
	}

	if (abort) {
		if (check_fwstate(pmlmepriv, WIFI_UNDER_LINKING)
			#ifdef CONFIG_IOCTL_CFG80211
			|| rtw_cfg80211_is_connect_requested(adapter)
			#endif
		) {
			if (!RTW_CANNOT_RUN(adapter_to_dvobj(adapter)))
				RTW_INFO(FUNC_ADPT_FMT" waiting for join_abort time out!\n", FUNC_ADPT_ARG(adapter));
		}
	}

	pmlmeext->join_abort = 0;
	pass_ms = rtw_get_passing_time_ms(start);

	return pass_ms;
}

u32 rtw_join_abort_timeout(_adapter *adapter, u32 timeout_ms)
{
	return _rtw_wait_join_done(adapter, _TRUE, timeout_ms);
}

static struct sta_info *rtw_joinbss_update_stainfo(_adapter *padapter, struct wlan_network *pnetwork)
{
	int i;
	struct sta_info *psta = NULL;
	struct recv_reorder_ctrl *preorder_ctrl;
	struct sta_priv *pstapriv = &padapter->stapriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
#if defined(CONFIG_RTW_MULTI_AP) || defined(CONFIG_RTS_FULL_BW)
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
#endif
#ifdef CONFIG_RTS_FULL_BW
	struct wlan_network  *cur_network = &(pmlmepriv->cur_network);
#endif/*CONFIG_RTS_FULL_BW*/

	psta = rtw_get_stainfo(pstapriv, pnetwork->network.MacAddress);
	if (psta == NULL) {
		RTW_ERR("%s: can not find "MAC_FMT"\n", __func__, MAC_ARG(pnetwork->network.MacAddress));
		psta = rtw_alloc_stainfo(pstapriv, pnetwork->network.MacAddress);
	}

	if (psta) { /* update ptarget_sta */
		RTW_INFO("%s\n", __FUNCTION__);

		/* assign aid */
		psta->phl_sta->aid  = pnetwork->join_res;

		update_sta_info(padapter, psta);

		/* update station supportRate */
		psta->bssratelen = rtw_get_rateset_len(pnetwork->network.SupportedRates);
		_rtw_memcpy(psta->bssrateset, pnetwork->network.SupportedRates, psta->bssratelen);
		update_sta_ra_info(padapter, psta);

		psta->phl_sta->wmode = pmlmeext->cur_wireless_mode;


		/* security related */
#ifdef CONFIG_RTW_80211R
		if ((padapter->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X)
			&& (psta->ft_pairwise_key_installed == _FALSE))
#else
		if (padapter->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X)
#endif
		{
			u8 *ie;
			sint ie_len;
			u8 mfp_opt = MFP_NO;
			u8 spp_opt = 0;

			padapter->securitypriv.binstallGrpkey = _FALSE;
			padapter->securitypriv.busetkipkey = _FALSE;
			padapter->securitypriv.bgrpkey_handshake = _FALSE;

			ie = rtw_get_ie(pnetwork->network.IEs + _BEACON_IE_OFFSET_, WLAN_EID_RSN
				, &ie_len, (pnetwork->network.IELength - _BEACON_IE_OFFSET_));
			if (ie && ie_len > 0
				&& rtw_parse_wpa2_ie(ie, ie_len + 2, NULL, NULL, NULL, NULL, &mfp_opt, &spp_opt) == _SUCCESS
			) {
				if (padapter->securitypriv.mfp_opt >= MFP_OPTIONAL && mfp_opt >= MFP_OPTIONAL)
					psta->flags |= WLAN_STA_MFP;
			}
			if (padapter->securitypriv.dot11PrivacyAlgrthm != _NO_PRIVACY_ ) {
				/*check if amsdu is allowed */
				if (rtw_check_amsdu_disable(padapter->registrypriv.amsdu_mode, spp_opt) == _TRUE)
					psta->flags |= WLAN_STA_AMSDU_DISABLE;
			}
			psta->ieee8021x_blocked = _TRUE;
			psta->dot118021XPrivacy = padapter->securitypriv.dot11PrivacyAlgrthm;

			_rtw_memset((u8 *)&psta->dot118021x_UncstKey, 0, sizeof(union Keytype));
			_rtw_memset((u8 *)&psta->dot11tkiprxmickey, 0, sizeof(union Keytype));
			_rtw_memset((u8 *)&psta->dot11tkiptxmickey, 0, sizeof(union Keytype));
		}

		/*	Commented by Albert 2012/07/21 */
		/*	When doing the WPS, the wps_ie_len won't equal to 0 */
		/*	And the Wi-Fi driver shouldn't allow the data packet to be tramsmitted. */
		if (padapter->securitypriv.wps_ie_len != 0) {
			psta->ieee8021x_blocked = _TRUE;
			padapter->securitypriv.wps_ie_len = 0;
		}


		/* for A-MPDU Rx reordering buffer control for sta_info */
		/* if A-MPDU Rx is enabled, reseting  rx_ordering_ctrl wstart_b(indicate_seq) to default value=0xffff */
		/* todo: check if AP can send A-MPDU packets */
		for (i = 0; i < 16 ; i++) {
			preorder_ctrl = &psta->recvreorder_ctrl[i];
			preorder_ctrl->enable = _FALSE;
			preorder_ctrl->indicate_seq = 0xffff;
			rtw_clear_bit(RTW_RECV_ACK_OR_TIMEOUT, &preorder_ctrl->rec_abba_rsp_ack);
			#ifdef DBG_RX_SEQ
			RTW_INFO("DBG_RX_SEQ "FUNC_ADPT_FMT" tid:%u SN_CLEAR indicate_seq:%u preorder_ctrl->rec_abba_rsp_ack:%lu\n"
				, FUNC_ADPT_ARG(padapter)
				, i
				, preorder_ctrl->indicate_seq
				,preorder_ctrl->rec_abba_rsp_ack
				);
			#endif
#ifdef CONFIG_RECV_REORDERING_CTRL
			preorder_ctrl->wsize_b = 64;/* max_ampdu_sz; */ /* ex. 32(kbytes) -> wsize_b=32 */
#endif
			preorder_ctrl->ampdu_size = RX_AMPDU_SIZE_INVALID;
		}

		#ifdef	CONFIG_RTW_80211K
		_rtw_memcpy(&psta->rm_en_cap, pnetwork->network.PhyInfo.rm_en_cap, 5);
		#endif
		#ifdef CONFIG_RTW_MULTI_AP
		if (padapter->multi_ap & MULTI_AP_BACKHAUL_STA) {
			u8 multi_ap = rtw_get_multi_ap_ie_ext(pmlmepriv->assoc_rsp + sizeof(struct rtw_ieee80211_hdr_3addr) + 6
					, pmlmepriv->assoc_rsp_len - sizeof(struct rtw_ieee80211_hdr_3addr) - 6);

			if (multi_ap & MULTI_AP_BACKHAUL_BSS) /* backhaul bss, enable WDS */
				psta->flags |= WLAN_STA_MULTI_AP | WLAN_STA_WDS;
			else if (multi_ap & MULTI_AP_FRONTHAUL_BSS) /* fronthaul bss only */
				psta->flags |= WLAN_STA_MULTI_AP;
		}
		#endif
		#ifdef CONFIG_RTS_FULL_BW
		rtw_parse_sta_vendor_ie_8812(padapter, psta, BSS_EX_TLV_IES(&cur_network->network), BSS_EX_TLV_IES_LEN(&cur_network->network));
		#endif
	}
	return psta;

}

/* pnetwork : returns from rtw_joinbss_event_callback
 * ptarget_wlan: found from scanned_queue */
static void rtw_joinbss_update_network(_adapter *padapter, struct wlan_network *ptarget_wlan, struct wlan_network  *pnetwork)
{
	struct mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	struct security_priv *psecuritypriv = &padapter->securitypriv;
	struct wlan_network  *cur_network = &(pmlmepriv->cur_network);
	sint tmp_fw_state = 0x0;

	RTW_INFO("%s\n", __FUNCTION__);

	/* why not use ptarget_wlan?? */
	_rtw_memcpy(&cur_network->network, &pnetwork->network, pnetwork->network.Length);
	/* some IEs in pnetwork is wrong, so we should use ptarget_wlan IEs */
	cur_network->network.IELength = ptarget_wlan->network.IELength;
	_rtw_memcpy(&cur_network->network.IEs[0], &ptarget_wlan->network.IEs[0], MAX_IE_SZ);

	cur_network->aid = pnetwork->join_res;
	#ifdef CONFIG_SIGNAL_STAT_PROCESS
	rtw_set_signal_stat_timer(&padapter->recvinfo);
	#endif

	/*RTW_WKARD_CORE_RSSI_V1 - GEORGIA MUST REFINE*/
	padapter->recvinfo.signal_strength = ptarget_wlan->network.PhyInfo.SignalStrength;
	padapter->recvinfo.signal_qual = ptarget_wlan->network.PhyInfo.SignalQuality;
	/* the ptarget_wlan->network.PhyInfo.rssi is raw data, we use ptarget_wlan->network.PhyInfo.SignalStrength instead (has scaled) */
	padapter->recvinfo.rssi = ptarget_wlan->network.PhyInfo.rssi;
#if defined(DBG_RX_SIGNAL_DISPLAY_PROCESSING) && 1
	RTW_INFO(FUNC_ADPT_FMT" signal_strength:%3u, rssi:%3d, signal_qual:%3u"
		 "\n"
		 , FUNC_ADPT_ARG(padapter)
		 , padapter->recvinfo.signal_strength
		 , padapter->recvinfo.rssi
		 , padapter->recvinfo.signal_qual
		);
#endif
	#ifdef CONFIG_SIGNAL_STAT_PROCESS
	rtw_set_signal_stat_timer(&padapter->recvinfo);
	#endif
	/* update fw_state */ /* will clr WIFI_UNDER_LINKING here indirectly */

	switch (pnetwork->network.InfrastructureMode) {
	case Ndis802_11Infrastructure:
		/* Check encryption */
		if (psecuritypriv->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X)
			tmp_fw_state = tmp_fw_state | WIFI_UNDER_KEY_HANDSHAKE;

		if (check_fwstate(pmlmepriv, WIFI_UNDER_WPS))
			tmp_fw_state = tmp_fw_state | WIFI_UNDER_WPS;

		init_fwstate(pmlmepriv, WIFI_STATION_STATE | tmp_fw_state);

		break;
	case Ndis802_11IBSS:
		/*pmlmepriv->fw_state = WIFI_ADHOC_STATE;*/
		init_fwstate(pmlmepriv, WIFI_ADHOC_STATE);
		break;
	default:
		/*pmlmepriv->fw_state = WIFI_NULL_STATE;*/
		init_fwstate(pmlmepriv, WIFI_NULL_STATE);
		break;
	}

	rtw_update_protection(padapter, (cur_network->network.IEs) + sizeof(NDIS_802_11_FIXED_IEs),
			      (cur_network->network.IELength));

#ifdef CONFIG_80211N_HT
	rtw_update_ht_cap(padapter, cur_network->network.IEs, cur_network->network.IELength, (u8) cur_network->network.Configuration.DSConfig);
#endif
}

/* Notes: the fucntion could be > passive_level (the same context as Rx tasklet)
 * pnetwork : returns from rtw_joinbss_event_callback
 * ptarget_wlan: found from scanned_queue
 * if join_res > 0, for (fw_state==WIFI_STATION_STATE), we check if  "ptarget_sta" & "ptarget_wlan" exist.
 * if join_res > 0, for (fw_state==WIFI_ADHOC_STATE), we only check if "ptarget_wlan" exist.
 * if join_res > 0, update "cur_network->network" from "pnetwork->network" if (ptarget_wlan !=NULL).
 */
/* #define REJOIN */
void rtw_joinbss_event_prehandle(_adapter *adapter, u8 *pbuf, u16 status)
{
	static u8 retry = 0;
	struct sta_info *ptarget_sta = NULL, *pcur_sta = NULL;
	struct	sta_priv *pstapriv = &adapter->stapriv;
	struct	mlme_priv	*pmlmepriv = &(adapter->mlmepriv);
	struct wlan_network	*pnetwork	= (struct wlan_network *)pbuf;
	struct wlan_network	*cur_network = &(pmlmepriv->cur_network);
	struct wlan_network	*pcur_wlan = NULL, *ptarget_wlan = NULL;
	unsigned int		the_same_macaddr = _FALSE;

	rtw_get_encrypt_decrypt_from_registrypriv(adapter);

	the_same_macaddr = _rtw_memcmp(pnetwork->network.MacAddress, cur_network->network.MacAddress, ETH_ALEN);

	pnetwork->network.Length = get_WLAN_BSSID_EX_sz(&pnetwork->network);
	if (pnetwork->network.Length > sizeof(WLAN_BSSID_EX))
		goto exit;

	_rtw_spinlock_bh(&pmlmepriv->lock);

	pmlmepriv->LinkDetectInfo.TrafficTransitionCount = 0;
	pmlmepriv->LinkDetectInfo.LowPowerTransitionCount = 0;


	if (pnetwork->join_res > 0) {
		_rtw_spinlock_bh(&(pmlmepriv->scanned_queue.lock));
		retry = 0;
		if (check_fwstate(pmlmepriv, WIFI_UNDER_LINKING)) {
			/* s1. find ptarget_wlan */
			if (check_fwstate(pmlmepriv, WIFI_ASOC_STATE)) {
				if (the_same_macaddr == _TRUE)
					ptarget_wlan = _rtw_find_network(&pmlmepriv->scanned_queue, cur_network->network.MacAddress);
				else {
					pcur_wlan = _rtw_find_network(&pmlmepriv->scanned_queue, cur_network->network.MacAddress);
					if (pcur_wlan)
						pcur_wlan->fixed = _FALSE;

					pcur_sta = rtw_get_stainfo(pstapriv, cur_network->network.MacAddress);
					if (pcur_sta) {
						/* _rtw_spinlock_bh(&(pstapriv->sta_hash_lock)); */
						rtw_free_stainfo(adapter,  pcur_sta);
						/* _rtw_spinunlock_bh(&(pstapriv->sta_hash_lock)); */
					}

					ptarget_wlan = _rtw_find_network(&pmlmepriv->scanned_queue, pnetwork->network.MacAddress);
					if (MLME_IS_STA(adapter)) {
						if (ptarget_wlan)
							ptarget_wlan->fixed = _TRUE;
					}
				}

			} else {
				ptarget_wlan = _rtw_find_same_network(&pmlmepriv->scanned_queue, pnetwork);
				if (MLME_IS_STA(adapter)) {
					if (ptarget_wlan)
						ptarget_wlan->fixed = _TRUE;
				}
			}

			/* s2. update cur_network */
			if (ptarget_wlan)
				rtw_joinbss_update_network(adapter, ptarget_wlan, pnetwork);
			else {
				RTW_PRINT("Can't find ptarget_wlan when joinbss_event callback\n");
				_rtw_spinunlock_bh(&(pmlmepriv->scanned_queue.lock));
				goto ignore_joinbss_callback;
			}


			/* s3. find ptarget_sta & update ptarget_sta after update cur_network only for station mode */
			if (MLME_IS_STA(adapter)) {
				ptarget_sta = rtw_joinbss_update_stainfo(adapter, pnetwork);
				if (ptarget_sta == NULL) {
					RTW_ERR("Can't update stainfo when joinbss_event callback\n");
					_rtw_spinunlock_bh(&(pmlmepriv->scanned_queue.lock));
					goto ignore_joinbss_callback;
				}

				/* Queue TX packets before FW/HW ready */
				/* clear in mlmeext_joinbss_event_callback() */
				rtw_xmit_queue_set(ptarget_sta);
			}

			/* s4. indicate connect			 */
			if (MLME_IS_STA(adapter) || MLME_IS_ADHOC(adapter)) {
				pmlmepriv->cur_network_scanned = ptarget_wlan;
				rtw_indicate_connect(adapter);
			}

			/* s5. Cancle assoc_timer					 */
			/*_cancel_timer_ex(&pmlmepriv->assoc_timer);*/
			cancel_assoc_timer(pmlmepriv);


		} else {
			_rtw_spinunlock_bh(&(pmlmepriv->scanned_queue.lock));
			goto ignore_joinbss_callback;
		}

		_rtw_spinunlock_bh(&(pmlmepriv->scanned_queue.lock));

	} else if (pnetwork->join_res == -4) {
		rtw_reset_securitypriv(adapter);
		pmlmepriv->join_status = status;
		/*_set_timer(&pmlmepriv->assoc_timer, 1);*/
		set_assoc_timer(pmlmepriv, 1);

		/* rtw_free_assoc_resources(adapter, _TRUE); */

		if ((check_fwstate(pmlmepriv, WIFI_UNDER_LINKING)) == _TRUE) {
			_clr_fwstate_(pmlmepriv, WIFI_UNDER_LINKING);
		}

	} else { /* if join_res < 0 (join fails), then try again */

#ifdef REJOIN
		res = _FAIL;
		if (retry < 2) {
			res = rtw_select_and_join_from_scanned_queue(pmlmepriv);
		}

		if (res == _SUCCESS) {
			/* extend time of assoc_timer */
			/*_set_timer(&pmlmepriv->assoc_timer, MAX_JOIN_TIMEOUT);*/
			set_assoc_timer(pmlmepriv, MAX_JOIN_TIMEOUT);
			retry++;
		} else if (res == 2) { /* there is no need to wait for join */
			_clr_fwstate_(pmlmepriv, WIFI_UNDER_LINKING);
			rtw_indicate_connect(adapter);
		} else {
#endif
			pmlmepriv->join_status = status;
			/*_set_timer(&pmlmepriv->assoc_timer, 1);*/
			set_assoc_timer(pmlmepriv, 1);
			/* rtw_free_assoc_resources(adapter, _TRUE); */
			_clr_fwstate_(pmlmepriv, WIFI_UNDER_LINKING);

#ifdef REJOIN
			retry = 0;
		}
#endif
	}

ignore_joinbss_callback:
	_rtw_spinunlock_bh(&pmlmepriv->lock);

exit:
	return;
}

void rtw_joinbss_event_callback(_adapter *adapter, u8 *pbuf)
{
	struct wlan_network *pnetwork = (struct wlan_network *)pbuf;


	rtw_set_hw_after_join(adapter, pnetwork->join_res);
	rtw_mi_os_xmit_schedule(adapter);
}

void rtw_sta_media_status_rpt(_adapter *adapter, struct sta_info *sta, bool connected)
{
/* ToDo H2C */
#if 0
	struct macid_ctl_t *macid_ctl = &adapter->dvobj->macid_ctl;
	bool miracast_enabled = 0;
	bool miracast_sink = 0;
	u8 role = H2C_MSR_ROLE_RSVD;

	if (sta == NULL) {
		RTW_PRINT(FUNC_ADPT_FMT" sta is NULL\n"
			  , FUNC_ADPT_ARG(adapter));
		rtw_warn_on(1);
		return;
	}

	if (sta->phl_sta->macid >= macid_ctl->num) {
		RTW_PRINT(FUNC_ADPT_FMT" invalid macid:%u\n"
			  , FUNC_ADPT_ARG(adapter), sta->phl_sta->macid);
		rtw_warn_on(1);
		return;
	}

	if (!rtw_macid_is_used(macid_ctl, sta->phl_sta->macid)) {
		RTW_PRINT(FUNC_ADPT_FMT" macid:%u not is used, set connected to 0\n"
			  , FUNC_ADPT_ARG(adapter), sta->phl_sta->macid);
		connected = 0;
		rtw_warn_on(1);
	}

	if (connected && !rtw_macid_is_bmc(macid_ctl, sta->phl_sta->macid)) {
		miracast_enabled = STA_OP_WFD_MODE(sta) != 0 && is_miracast_enabled(adapter);
		miracast_sink = miracast_enabled && (STA_OP_WFD_MODE(sta) & MIRACAST_SINK);

#ifdef CONFIG_TDLS
		if (sta->tdls_sta_state & TDLS_LINKED_STATE)
			role = H2C_MSR_ROLE_TDLS;
		else
#endif
		if (MLME_IS_STA(adapter)) {
			if (MLME_IS_GC(adapter))
				role = H2C_MSR_ROLE_GO;
			else
				role = H2C_MSR_ROLE_AP;
		} else if (MLME_IS_AP(adapter)) {
			if (MLME_IS_GO(adapter))
				role = H2C_MSR_ROLE_GC;
			else
				role = H2C_MSR_ROLE_STA;
		} else if (MLME_IS_ADHOC(adapter) || MLME_IS_ADHOC_MASTER(adapter))
			role = H2C_MSR_ROLE_ADHOC;
		else if (MLME_IS_MESH(adapter))
			role = H2C_MSR_ROLE_MESH;

#ifdef CONFIG_WFD
		if (role == H2C_MSR_ROLE_GC
			|| role == H2C_MSR_ROLE_GO
			|| role == H2C_MSR_ROLE_TDLS
		) {
			if (adapter->wfd_info.rtsp_ctrlport
				|| adapter->wfd_info.tdls_rtsp_ctrlport
				|| adapter->wfd_info.peer_rtsp_ctrlport)
				rtw_wfd_st_switch(sta, 1);
		}
#endif
	}

	rtw_hal_set_FwMediaStatusRpt_single_cmd(adapter
		, connected
		, miracast_enabled
		, miracast_sink
		, role
		, sta->phl_sta->macid
	);
#endif
}

u8 rtw_sta_media_status_rpt_cmd(_adapter *adapter, struct sta_info *sta, bool connected)
{
	struct cmd_priv	*cmdpriv = &adapter_to_dvobj(adapter)->cmdpriv;
	struct cmd_obj *cmdobj;
	struct drvextra_cmd_parm *cmd_parm;
	struct sta_media_status_rpt_cmd_parm *rpt_parm;
	u8	res = _SUCCESS;

	cmdobj = (struct cmd_obj *)rtw_zmalloc(sizeof(struct cmd_obj));
	if (cmdobj == NULL) {
		res = _FAIL;
		goto exit;
	}
	cmdobj->padapter = adapter;

	cmd_parm = (struct drvextra_cmd_parm *)rtw_zmalloc(sizeof(struct drvextra_cmd_parm));
	if (cmd_parm == NULL) {
		rtw_mfree((u8 *)cmdobj, sizeof(struct cmd_obj));
		res = _FAIL;
		goto exit;
	}

	rpt_parm = (struct sta_media_status_rpt_cmd_parm *)rtw_zmalloc(sizeof(struct sta_media_status_rpt_cmd_parm));
	if (rpt_parm == NULL) {
		rtw_mfree((u8 *)cmdobj, sizeof(struct cmd_obj));
		rtw_mfree((u8 *)cmd_parm, sizeof(struct drvextra_cmd_parm));
		res = _FAIL;
		goto exit;
	}

	rpt_parm->sta = sta;
	rpt_parm->connected = connected;

	cmd_parm->ec_id = STA_MSTATUS_RPT_WK_CID;
	cmd_parm->type = 0;
	cmd_parm->size = sizeof(struct sta_media_status_rpt_cmd_parm);
	cmd_parm->pbuf = (u8 *)rpt_parm;
	init_h2fwcmd_w_parm_no_rsp(cmdobj, cmd_parm, CMD_SET_DRV_EXTRA);

	res = rtw_enqueue_cmd(cmdpriv, cmdobj);

exit:
	return res;
}

inline void rtw_sta_media_status_rpt_cmd_hdl(_adapter *adapter, struct sta_media_status_rpt_cmd_parm *parm)
{
	rtw_sta_media_status_rpt(adapter, parm->sta, parm->connected);
}

void rtw_stassoc_event_callback(_adapter *adapter, u8 *pbuf)
{
	struct sta_info *psta;
	struct mlme_priv *pmlmepriv = &(adapter->mlmepriv);
	struct stassoc_event	*pstassoc	= (struct stassoc_event *)pbuf;
	struct wlan_network	*cur_network = &(pmlmepriv->cur_network);
	struct wlan_network	*ptarget_wlan = NULL;


#if CONFIG_RTW_MACADDR_ACL
	if (rtw_access_ctrl(adapter, pstassoc->macaddr) == _FALSE)
		return;
#endif

#if defined(CONFIG_AP_MODE) && defined (CONFIG_NATIVEAP_MLME)
	if (MLME_IS_AP(adapter) || MLME_IS_MESH(adapter)) {
		psta = rtw_get_stainfo(&adapter->stapriv, pstassoc->macaddr);
		if (psta) {
			u8 *passoc_req = NULL;
			u32 assoc_req_len = 0;

			rtw_alloc_stainfo_hw(&adapter->stapriv, psta);

#if 0
			rtw_sta_media_status_rpt(adapter, psta, 1);
#else
			rtw_hw_connected_apmode(adapter, psta);
#endif

#ifndef CONFIG_AUTO_AP_MODE
			if (!MLME_IS_MESH(adapter)) {
				/* report to upper layer */
				RTW_INFO("indicate_sta_assoc_event to upper layer - hostapd\n");
				#ifdef CONFIG_IOCTL_CFG80211
				_rtw_spinlock_bh(&psta->lock);
				if (psta->passoc_req && psta->assoc_req_len > 0) {
					passoc_req = rtw_zmalloc(psta->assoc_req_len);
					if (passoc_req) {
						assoc_req_len = psta->assoc_req_len;
						_rtw_memcpy(passoc_req, psta->passoc_req, assoc_req_len);
					}
				}
				_rtw_spinunlock_bh(&psta->lock);

				if (passoc_req && assoc_req_len > 0) {
					rtw_cfg80211_indicate_sta_assoc(adapter, passoc_req, assoc_req_len);
					rtw_mfree(passoc_req, assoc_req_len);
				}
				#else /* !CONFIG_IOCTL_CFG80211	 */
				rtw_indicate_sta_assoc_event(adapter, psta);
				#endif /* !CONFIG_IOCTL_CFG80211 */
			}
#endif /* !CONFIG_AUTO_AP_MODE */

			if (is_wep_enc(adapter->securitypriv.dot11PrivacyAlgrthm))
				rtw_ap_wep_pk_setting(adapter, psta);
		}
		goto exit;
	}
#endif /* defined (CONFIG_AP_MODE) && defined (CONFIG_NATIVEAP_MLME) */

	/* for AD-HOC mode */
	psta = rtw_get_stainfo(&adapter->stapriv, pstassoc->macaddr);
	if (psta == NULL) {
		RTW_ERR(FUNC_ADPT_FMT" get no sta_info with "MAC_FMT"\n"
			, FUNC_ADPT_ARG(adapter), MAC_ARG(pstassoc->macaddr));
		rtw_warn_on(1);
		goto exit;
	}

	rtw_sta_media_status_rpt(adapter, psta, 1);

	if (adapter->securitypriv.dot11AuthAlgrthm == dot11AuthAlgrthm_8021X)
		psta->dot118021XPrivacy = adapter->securitypriv.dot11PrivacyAlgrthm;


	psta->ieee8021x_blocked = _FALSE;

	_rtw_spinlock_bh(&pmlmepriv->lock);

	if ((check_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE) == _TRUE) ||
	    (check_fwstate(pmlmepriv, WIFI_ADHOC_STATE) == _TRUE)) {
		if (adapter->stapriv.asoc_sta_count == 2) {
			_rtw_spinlock_bh(&(pmlmepriv->scanned_queue.lock));
			ptarget_wlan = _rtw_find_network(&pmlmepriv->scanned_queue, cur_network->network.MacAddress);
			pmlmepriv->cur_network_scanned = ptarget_wlan;
			if (ptarget_wlan)
				ptarget_wlan->fixed = _TRUE;
			_rtw_spinunlock_bh(&(pmlmepriv->scanned_queue.lock));
			/* a sta + bc/mc_stainfo (not Ibss_stainfo) */
			rtw_indicate_connect(adapter);
		}
	}

	_rtw_spinunlock_bh(&pmlmepriv->lock);


	mlmeext_sta_add_event_callback(adapter, psta);

#ifdef CONFIG_RTL8711
	/* submit SetStaKey_cmd to tell fw, fw will allocate an CAM entry for this sta	 */
	rtw_setstakey_cmd(adapter, psta, GROUP_KEY, _TRUE);
#endif

exit:
#ifdef CONFIG_RTS_FULL_BW
	rtw_set_rts_bw(adapter);
#endif/*CONFIG_RTS_FULL_BW*/
	return;
}

#ifdef CONFIG_IEEE80211W
void rtw_sta_timeout_event_callback(_adapter *adapter, u8 *pbuf)
{
	struct sta_info *psta;
	struct stadel_event *pstadel = (struct stadel_event *)pbuf;
	struct sta_priv *pstapriv = &adapter->stapriv;


	psta = rtw_get_stainfo(&adapter->stapriv, pstadel->macaddr);

	if (psta) {
		u8 updated = _FALSE;

		_rtw_spinlock_bh(&pstapriv->asoc_list_lock);
		if (rtw_is_list_empty(&psta->asoc_list) == _FALSE) {
			rtw_list_delete(&psta->asoc_list);
			pstapriv->asoc_list_cnt--;
			#ifdef CONFIG_RTW_TOKEN_BASED_XMIT
			if (psta->tbtx_enable)
				pstapriv->tbtx_asoc_list_cnt--;
			#endif
			updated = ap_free_sta(adapter, psta, _TRUE, WLAN_REASON_PREV_AUTH_NOT_VALID, _TRUE, _FALSE);
		}
		_rtw_spinunlock_bh(&pstapriv->asoc_list_lock);

		associated_clients_update(adapter, updated, STA_INFO_UPDATE_ALL);
	}



}
#endif /* CONFIG_IEEE80211W */


/* Note: the caller should already have lock of a->mlmepriv.lock */
/* Note2: There may be I/O in rtw_free_assoc_resources() */
static void _stadel_posthandle_sta(struct _ADAPTER *a,
				   struct stadel_event *stadel)
{
	struct mlme_priv *mlme = &a->mlmepriv;
	u16 reason = *((unsigned short *)stadel->rsvd);
	bool roam = _FALSE;
	struct wlan_network *roam_target = NULL;


#ifdef CONFIG_LAYER2_ROAMING
#ifdef CONFIG_RTW_80211R
	if (rtw_ft_roam_expired(a, reason))
		mlme->ft_roam.ft_roam_on_expired = _TRUE;
	else
		mlme->ft_roam.ft_roam_on_expired = _FALSE;
#endif
	if (a->registrypriv.wifi_spec == 1) {
		roam = _FALSE;
	} else if ((reason == WLAN_REASON_EXPIRATION_CHK)
		   && rtw_chk_roam_flags(a, RTW_ROAM_ON_EXPIRED)) {
		roam = _TRUE;
	} else if ((reason == WLAN_REASON_ACTIVE_ROAM)
		   && rtw_chk_roam_flags(a, RTW_ROAM_ACTIVE)) {
		roam = _TRUE;
		roam_target = mlme->roam_network;
	}
#ifdef CONFIG_RTW_80211R
	if ((reason == WLAN_REASON_ACTIVE_ROAM)
	    && rtw_ft_chk_flags(a, RTW_FT_BTM_ROAM)) {
		roam = _TRUE;
		roam_target = mlme->roam_network;
	}
#endif

	if (roam == _TRUE) {
		if (rtw_to_roam(a) > 0)
			rtw_dec_to_roam(a); /* this stadel_event is caused by roaming, decrease to_roam */
		else if (rtw_to_roam(a) == 0)
			rtw_set_to_roam(a, a->registrypriv.max_roaming_times);
	} else {
		rtw_set_to_roam(a, 0);
	}
#endif /* CONFIG_LAYER2_ROAMING */

#if 0
	rtw_free_uc_swdec_pending_queue(adapter_to_dvobj(a));
#endif
	if (MLME_IS_ASOC(a))
		rtw_free_assoc_resources(a, _TRUE);
	rtw_free_mlme_priv_ie_data(mlme);

	rtw_indicate_disconnect(a, *(u16 *)stadel->rsvd, stadel->locally_generated);

	_rtw_roaming(a, roam_target);
}

void rtw_stadel_event_callback(_adapter *adapter, u8 *pbuf)
{
	struct sta_info *psta;
	struct wlan_network *pwlan = NULL;
	WLAN_BSSID_EX    *pdev_network = NULL;
	u8 *pibss = NULL;
	struct	mlme_priv	*pmlmepriv = &(adapter->mlmepriv);
	struct	stadel_event *pstadel	= (struct stadel_event *)pbuf;
	struct wlan_network *tgt_network = &(pmlmepriv->cur_network);

	RTW_INFO("%s(mac_id=%d)=" MAC_FMT "\n", __func__, pstadel->mac_id, MAC_ARG(pstadel->macaddr));

	psta = rtw_get_stainfo(&adapter->stapriv, pstadel->macaddr);

	if (psta == NULL) {
		RTW_INFO("%s(mac_id=%d)=" MAC_FMT " psta == NULL\n", __func__, pstadel->mac_id, MAC_ARG(pstadel->macaddr));
		/*rtw_warn_on(1);*/
	}

	if (psta) {
		rtw_wfd_st_switch(psta, 0);
		psta->hw_decrypted = _FALSE;
	}

	if (MLME_IS_MESH(adapter)) {
		rtw_free_stainfo(adapter, psta);
		goto exit;
	}

	if (MLME_IS_AP(adapter)) {
#ifdef CONFIG_IOCTL_CFG80211
#ifdef COMPAT_KERNEL_RELEASE

#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)) || defined(CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER)
		rtw_cfg80211_indicate_sta_disassoc(adapter, pstadel->macaddr, *(u16 *)pstadel->rsvd);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)) || defined(CONFIG_CFG80211_FORCE_COMPATIBLE_2_6_37_UNDER) */
#endif /* CONFIG_IOCTL_CFG80211 */

		rtw_hw_disconnect(adapter, psta);

		rtw_free_stainfo(adapter, psta);

		goto exit;
	}

	mlmeext_sta_del_event_callback(adapter);

	_rtw_spinlock_bh(&pmlmepriv->lock);

	if (MLME_IS_STA(adapter))
		_stadel_posthandle_sta(adapter, pstadel);

	if (MLME_IS_ADHOC_MASTER(adapter) ||
	    MLME_IS_ADHOC(adapter)) {

		/* _rtw_spinlock_bh(&(pstapriv->sta_hash_lock)); */
		rtw_free_stainfo(adapter,  psta);
		/* _rtw_spinunlock_bh(&(pstapriv->sta_hash_lock)); */

		if (adapter->stapriv.asoc_sta_count == 1) { /* a sta + bc/mc_stainfo (not Ibss_stainfo) */
			/* rtw_indicate_disconnect(adapter); */ /* removed@20091105 */
			_rtw_spinlock_bh(&(pmlmepriv->scanned_queue.lock));
			/* free old ibss network */
			/* pwlan = _rtw_find_network(&pmlmepriv->scanned_queue, pstadel->macaddr); */
			pwlan = _rtw_find_network(&pmlmepriv->scanned_queue, tgt_network->network.MacAddress);
			if (pwlan) {
				pwlan->fixed = _FALSE;
				rtw_free_network_nolock(adapter, pwlan);
			}
			_rtw_spinunlock_bh(&(pmlmepriv->scanned_queue.lock));
			/* re-create ibss */
			pdev_network = &(adapter->registrypriv.dev_network);
			pibss = adapter->registrypriv.dev_network.MacAddress;

			_rtw_memcpy(pdev_network, &tgt_network->network, get_WLAN_BSSID_EX_sz(&tgt_network->network));

			_rtw_memset(&pdev_network->Ssid, 0, sizeof(NDIS_802_11_SSID));
			_rtw_memcpy(&pdev_network->Ssid, &pmlmepriv->assoc_ssid, sizeof(NDIS_802_11_SSID));

			rtw_update_registrypriv_dev_network(adapter);

			rtw_generate_random_ibss(pibss);

			if (MLME_IS_ADHOC(adapter)) {
				set_fwstate(pmlmepriv, WIFI_ADHOC_MASTER_STATE);
				_clr_fwstate_(pmlmepriv, WIFI_ADHOC_STATE);
			}

			if (rtw_create_ibss_cmd(adapter, 0) != _SUCCESS)
				RTW_ERR("rtw_create_ibss_cmd FAIL\n");

		}

	}

	_rtw_spinunlock_bh(&pmlmepriv->lock);
exit:
	#ifdef CONFIG_RTS_FULL_BW
	rtw_set_rts_bw(adapter);
	#endif/*CONFIG_RTS_FULL_BW*/
	return;
}

void rtw_wmm_event_callback(_adapter *padapter, u8 *pbuf)
{

	WMMOnAssocRsp(padapter);


}

/*
* rtw_join_timeout_handler - Timeout/failure handler for CMD JoinBss
*/
void rtw_join_timeout_handler(void *ctx)
{
	_adapter *adapter = (_adapter *)ctx;
	struct	mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct mlme_ext_priv *pmlmeext = &adapter->mlmeextpriv;
	struct mlme_ext_info *pmlmeinfo = &pmlmeext->mlmext_info;

#if 0
	if (dev_is_drv_stopped(adapter_to_dvobj(adapter))) {
		_rtw_up_sema(&pmlmepriv->assoc_terminate);
		return;
	}
#endif



	RTW_INFO("%s, fw_state=%x\n", __FUNCTION__, get_fwstate(pmlmepriv));

	if (RTW_CANNOT_RUN(adapter_to_dvobj(adapter)))
		return;


	_rtw_spinlock_bh(&pmlmepriv->lock);

#ifdef CONFIG_LAYER2_ROAMING
	if (rtw_to_roam(adapter) > 0) { /* join timeout caused by roaming */
		while (1) {
			rtw_dec_to_roam(adapter);
			if (rtw_to_roam(adapter) != 0) { /* try another */
				int do_join_r;
				RTW_INFO("%s try another roaming\n", __FUNCTION__);
				do_join_r = rtw_do_join(adapter);
				if (_SUCCESS != do_join_r) {
					RTW_INFO("%s roaming do_join return %d\n", __FUNCTION__ , do_join_r);
					continue;
				}
				break;
			} else {
				RTW_INFO("%s We've try roaming but fail\n", __FUNCTION__);
#ifdef CONFIG_RTW_80211R
				/* rtw_ft_clr_flags(adapter, RTW_FT_PEER_EN|RTW_FT_PEER_OTD_EN); */
				rtw_ft_reset_status(adapter);
#endif
				rtw_indicate_disconnect(adapter, pmlmepriv->join_status, _FALSE);
				pmlmeinfo->disconnect_occurred_time = rtw_systime_to_ms(rtw_get_current_time());
				pmlmeinfo->disconnect_code = DISCONNECTION_BY_DRIVER_DUE_TO_JOINBSS_TIMEOUT;
				pmlmeinfo->wifi_reason_code = WLAN_REASON_UNSPECIFIED;
#ifdef CONFIG_STA_CMD_DISPR
				rtw_connect_abort(adapter);
#endif
				break;
			}
		}

	} else
#endif
	{
#ifdef CONFIG_STA_CMD_DISPR
		if (check_fwstate(pmlmepriv, WIFI_UNDER_LINKING) == _TRUE)
			rtw_connect_abort(adapter);
#endif /* CONFIG_STA_CMD_DISPR */

		rtw_indicate_disconnect(adapter, pmlmepriv->join_status, _FALSE);
		pmlmeinfo->disconnect_occurred_time = rtw_systime_to_ms(rtw_get_current_time());
		pmlmeinfo->disconnect_code = DISCONNECTION_BY_DRIVER_DUE_TO_JOINBSS_TIMEOUT;
		pmlmeinfo->wifi_reason_code = WLAN_REASON_UNSPECIFIED;
		free_scanqueue(pmlmepriv);/* ??? */

#ifdef CONFIG_IOCTL_CFG80211
		/* indicate disconnect for the case that join_timeout and check_fwstate != FW_LINKED */
		rtw_cfg80211_indicate_disconnect(adapter, pmlmepriv->join_status, _FALSE);
#endif /* CONFIG_IOCTL_CFG80211 */
	}

	pmlmepriv->join_status = 0; /* reset */

	_rtw_spinunlock_bh(&pmlmepriv->lock);


#ifdef CONFIG_DRVEXT_MODULE_WSC
	drvext_assoc_fail_indicate(&adapter->drvextpriv);
#endif



}

void rtw_mlme_reset_auto_scan_int(_adapter *adapter, u8 *reason)
{
#if defined(CONFIG_RTW_MESH) && defined(CONFIG_DFS_MASTER)
#if CONFIG_RTW_MESH_OFFCH_CAND 
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
#endif
#endif
	u8 u_ch;
	u32 interval_ms = 0xffffffff; /* 0xffffffff: special value to make min() works well, also means no auto scan */

	*reason = RTW_AUTO_SCAN_REASON_UNSPECIFIED;
	rtw_mi_get_ch_setting_union(adapter, &u_ch, NULL, NULL);

	if (rtw_hw_chk_bw_cap(adapter_to_dvobj(adapter), BW_CAP_40M)
		&& is_client_associated_to_ap(adapter) == _TRUE
		&& u_ch >= 1 && u_ch <= 14
		&& adapter->registrypriv.wifi_spec
		/* TODO: AP Connected is 40MHz capability? */
	) {
		interval_ms = rtw_min(interval_ms, 60 * 1000);
		*reason |= RTW_AUTO_SCAN_REASON_2040_BSS;
	}

#ifdef CONFIG_RTW_MESH
	#if CONFIG_RTW_MESH_OFFCH_CAND
	if (adapter->mesh_cfg.peer_sel_policy.offch_find_int_ms
		&& rtw_mesh_offch_candidate_accepted(adapter)
		#ifdef CONFIG_DFS_MASTER
		&& (!rfctl->radar_detect_ch || (IS_CH_WAITING(rfctl) && !IS_UNDER_CAC(rfctl)))
		#endif
	) {
		interval_ms = rtw_min(interval_ms, adapter->mesh_cfg.peer_sel_policy.offch_find_int_ms);
		*reason |= RTW_AUTO_SCAN_REASON_MESH_OFFCH_CAND;
	}
	#endif
#endif /* CONFIG_RTW_MESH */

	if (interval_ms == 0xffffffff)
		interval_ms = 0;

	rtw_mlme_set_auto_scan_int(adapter, interval_ms);
	return;
}


static void rtw_auto_scan_handler(_adapter *padapter)
{
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	u8 reason = RTW_AUTO_SCAN_REASON_UNSPECIFIED;
#ifdef CONFIG_LAYER2_ROAMING
	struct recv_info	*precvinfo = &padapter->recvinfo;
#endif

	rtw_mlme_reset_auto_scan_int(padapter, &reason);

#ifdef CONFIG_P2P
	if (rtw_p2p_chk_role(&padapter->wdinfo, P2P_ROLE_CLIENT) ||
	    rtw_p2p_chk_role(&padapter->wdinfo, P2P_ROLE_GO))
		goto exit;
#endif

#ifdef CONFIG_TDLS
	if (padapter->tdlsinfo.link_established == _TRUE)
		goto exit;
#endif

#ifdef CONFIG_LAYER2_ROAMING
	if (is_client_associated_to_ap(padapter) &&
		rtw_chk_roam_flags(padapter, RTW_ROAM_ACTIVE)) {
		RTW_INFO("avg_val = %d, need_to_roam=%d\n", precvinfo->signal_strength_data.avg_val, pmlmepriv->need_to_roam);
		if (precvinfo->signal_strength_data.avg_val < pmlmepriv->roam_rssi_threshold) {
			pmlmepriv->need_to_roam = _TRUE;
			if (rtw_get_passing_time_ms(pmlmepriv->last_roaming) >= pmlmepriv->roam_scan_int*2000) {
#ifdef CONFIG_RTW_80211K
				rtw_roam_nb_discover(padapter, _FALSE);
#endif
				reason = RTW_AUTO_SCAN_REASON_ROAM_ACTIVE;
				pmlmepriv->last_roaming = rtw_get_current_time();
				goto do_scan;
			}
		} else
			pmlmepriv->need_to_roam = _FALSE;
	}
#endif

	if (pmlmepriv->auto_scan_int_ms == 0
	    || rtw_get_passing_time_ms(pmlmepriv->scan_start_time) < pmlmepriv->auto_scan_int_ms)
		goto exit;
do_scan:
	rtw_drv_scan_by_self(padapter, reason);

exit:
	return;
}
static u8 is_drv_in_lps(_adapter *adapter)
{
	u8 is_in_lps = _FALSE;

	#ifdef CONFIG_LPS_LCLK_WD_TIMER /* to avoid leaving lps 32k frequently*/
	if ((adapter_to_pwrctl(adapter)->bFwCurrentInPSMode == _TRUE)
		)
		is_in_lps = _TRUE;
	#endif /* CONFIG_LPS_LCLK_WD_TIMER*/
	return is_in_lps;
}

void rtw_iface_dynamic_check_handlder(struct _ADAPTER *a)
{
	if (!a->netif_up)
		return;

	#ifdef CONFIG_ACTIVE_KEEP_ALIVE_CHECK
	#ifdef CONFIG_AP_MODE
	if (MLME_IS_AP(a) || MLME_IS_MESH(a)) {
		expire_timeout_chk(a);
		#ifdef CONFIG_RTW_MESH
		if (MLME_IS_MESH(a) && MLME_IS_ASOC(a))
			rtw_mesh_peer_status_chk(a);
		#endif
	}
	#endif
	#endif /* CONFIG_ACTIVE_KEEP_ALIVE_CHECK */

	/* auto site survey */
	rtw_auto_scan_handler(a);
	dynamic_update_bcn_check(a);
	linked_status_chk(a, 0);
	traffic_status_watchdog(a, 0);

#ifdef CONFIG_BR_EXT
if (!adapter_use_wds(a)) {
	#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35))
		rcu_read_lock();
	#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)) */

	#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35))
		if (a->pnetdev->br_port
	#else	/* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
		if (rcu_dereference(a->pnetdev->rx_handler_data)
	#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)) */
			&& (MLME_IS_STA(a) || MLME_IS_ADHOC(a))) {
			/* expire NAT2.5 entry */
			void nat25_db_expire(_adapter *priv);
			nat25_db_expire(a);

			if (a->pppoe_connection_in_progress > 0)
				a->pppoe_connection_in_progress--;
			if (a->pppoe_connection_in_progress > 0)
				a->pppoe_connection_in_progress--;
		}

	#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35))
		rcu_read_unlock();
	#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)) */
}
#endif /* CONFIG_BR_EXT */
#ifdef CONFIG_RTW_CFGVENDOR_RSSIMONITOR
        rtw_cfgvendor_rssi_monitor_evt(a);
#endif
}

void rtw_iface_dynamic_check_timer_handlder(_adapter *adapter)
{
#ifdef CONFIG_AP_MODE
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
#endif /* CONFIG_AP_MODE */

	if (!adapter->netif_up)
		return;
	#ifdef CONFIG_LPS_LCLK_WD_TIMER /* to avoid leaving lps 32k frequently*/
	if (is_drv_in_lps(adapter)) {
		u8 bEnterPS;

		linked_status_chk(adapter, 1);

		bEnterPS = traffic_status_watchdog(adapter, 1);
		#if 0 /*PS TODO ...*/
		if (bEnterPS) {
			/* rtw_lps_ctrl_wk_cmd(adapter, LPS_CTRL_ENTER, 0); */
			rtw_hal_dm_watchdog_in_lps(adapter);
		} else {
			/* call rtw_lps_ctrl_wk_cmd(padapter, LPS_CTRL_LEAVE, 0) in traffic_status_watchdog() */
		}
		#endif
	}
	#endif /* CONFIG_LPS_LCLK_WD_TIMER	*/

#ifdef CONFIG_AP_MODE
	if (MLME_IS_AP(adapter)|| MLME_IS_MESH(adapter)) {
		#ifndef CONFIG_ACTIVE_KEEP_ALIVE_CHECK
		expire_timeout_chk(adapter);
		#endif /* !CONFIG_ACTIVE_KEEP_ALIVE_CHECK */

		#ifdef CONFIG_BMC_TX_RATE_SELECT
		rtw_update_bmc_sta_tx_rate(adapter);
		#endif /*CONFIG_BMC_TX_RATE_SELECT*/
	}
#endif /*CONFIG_AP_MODE*/

	rtw_iface_dynamic_check_handlder(adapter);
}

/*TP_avg(t) = (1/10) * TP_avg(t-1) + (9/10) * TP(t) MBps*/
static void collect_sta_traffic_statistics(_adapter *adapter)
{
/* ToDo */
#if 0
	struct macid_ctl_t *macid_ctl = &adapter->dvobj->macid_ctl;
	struct sta_info *sta;
	u64 curr_tx_bytes = 0, curr_rx_bytes = 0;
	u32 curr_tx_mbytes = 0, curr_rx_mbytes = 0;
	int i;

	for (i = 0; i < MACID_NUM_SW_LIMIT; i++) {
		sta = macid_ctl->sta[i];
		if (sta && !is_broadcast_mac_addr(sta->phl_sta->mac_addr)) {
			if (sta->sta_stats.last_tx_bytes > sta->sta_stats.tx_bytes)
				sta->sta_stats.last_tx_bytes =  sta->sta_stats.tx_bytes;
			if (sta->sta_stats.last_rx_bytes > sta->sta_stats.rx_bytes)
				sta->sta_stats.last_rx_bytes = sta->sta_stats.rx_bytes;
			if (sta->sta_stats.last_rx_bc_bytes > sta->sta_stats.rx_bc_bytes)
				sta->sta_stats.last_rx_bc_bytes = sta->sta_stats.rx_bc_bytes;
			if (sta->sta_stats.last_rx_mc_bytes > sta->sta_stats.rx_mc_bytes)
				sta->sta_stats.last_rx_mc_bytes = sta->sta_stats.rx_mc_bytes;

			curr_tx_bytes = sta->sta_stats.tx_bytes - sta->sta_stats.last_tx_bytes;
			curr_rx_bytes = sta->sta_stats.rx_bytes - sta->sta_stats.last_rx_bytes;
			sta->sta_stats.tx_tp_kbits = (curr_tx_bytes * 8 / 2) >> 10;/*Kbps*/
			sta->sta_stats.rx_tp_kbits = (curr_rx_bytes * 8 / 2) >> 10;/*Kbps*/

			sta->sta_stats.smooth_tx_tp_kbits = (sta->sta_stats.smooth_tx_tp_kbits * 6 / 10) + (sta->sta_stats.tx_tp_kbits * 4 / 10);/*Kbps*/
			sta->sta_stats.smooth_rx_tp_kbits = (sta->sta_stats.smooth_rx_tp_kbits * 6 / 10) + (sta->sta_stats.rx_tp_kbits * 4 / 10);/*Kbps*/

			curr_tx_mbytes = (curr_tx_bytes / 2) >> 20;/*MBps*/
			curr_rx_mbytes = (curr_rx_bytes / 2) >> 20;/*MBps*/

			sta->phl_sta->tx_moving_average_tp =
				(sta->phl_sta->tx_moving_average_tp / 10) + (curr_tx_mbytes * 9 / 10); /*MBps*/

			sta->phl_sta->rx_moving_average_tp =
				(sta->phl_sta->rx_moving_average_tp / 10) + (curr_rx_mbytes * 9 /10); /*MBps*/

			rtw_collect_bcn_info(sta->padapter);

			if (adapter->bsta_tp_dump)
				rtw_hal_dump_sta_traffic(RTW_DBGDUMP, adapter, sta);

			sta->sta_stats.last_tx_bytes = sta->sta_stats.tx_bytes;
			sta->sta_stats.last_rx_bytes = sta->sta_stats.rx_bytes;
			sta->sta_stats.last_rx_bc_bytes = sta->sta_stats.rx_bc_bytes;
			sta->sta_stats.last_rx_mc_bytes = sta->sta_stats.rx_mc_bytes;
		}
	}
#endif
}

void rtw_sta_traffic_info(void *sel, _adapter *adapter)
{
/* ToDo */
#if 0
	struct macid_ctl_t *macid_ctl = &adapter->dvobj->macid_ctl;
	struct sta_info *sta;
	int i;

	for (i = 0; i < MACID_NUM_SW_LIMIT; i++) {
		sta = macid_ctl->sta[i];
		if (sta && !is_broadcast_mac_addr(sta->phl_sta->mac_addr))
			rtw_hal_dump_sta_traffic(sel, adapter, sta);
	}
#endif
}

/*#define DBG_TRAFFIC_STATISTIC*/
static void collect_traffic_statistics(_adapter *padapter)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(padapter);

	/*_rtw_memset(&pdvobjpriv->traffic_stat, 0, sizeof(struct rtw_traffic_statistics));*/

	/* Tx bytes reset*/
	pdvobjpriv->traffic_stat.tx_bytes = 0;
	pdvobjpriv->traffic_stat.tx_pkts = 0;
	pdvobjpriv->traffic_stat.tx_drop = 0;

	/* Rx bytes reset*/
	pdvobjpriv->traffic_stat.rx_bytes = 0;
	pdvobjpriv->traffic_stat.rx_pkts = 0;
	pdvobjpriv->traffic_stat.rx_drop = 0;

	rtw_mi_traffic_statistics(padapter);

	/* Calculate throughput in last interval */
	pdvobjpriv->traffic_stat.cur_tx_bytes = pdvobjpriv->traffic_stat.tx_bytes - pdvobjpriv->traffic_stat.last_tx_bytes;
	pdvobjpriv->traffic_stat.cur_rx_bytes = pdvobjpriv->traffic_stat.rx_bytes - pdvobjpriv->traffic_stat.last_rx_bytes;
	pdvobjpriv->traffic_stat.last_tx_bytes = pdvobjpriv->traffic_stat.tx_bytes;
	pdvobjpriv->traffic_stat.last_rx_bytes = pdvobjpriv->traffic_stat.rx_bytes;

	pdvobjpriv->traffic_stat.cur_tx_tp = (u32)(pdvobjpriv->traffic_stat.cur_tx_bytes * 8 / 2 / 1024 / 1024);/*Mbps*/
	pdvobjpriv->traffic_stat.cur_rx_tp = (u32)(pdvobjpriv->traffic_stat.cur_rx_bytes * 8 / 2 / 1024 / 1024);/*Mbps*/

	#ifdef DBG_TRAFFIC_STATISTIC
	RTW_INFO("\n========================\n");
	RTW_INFO("cur_tx_bytes:%lld\n", pdvobjpriv->traffic_stat.cur_tx_bytes);
	RTW_INFO("cur_rx_bytes:%lld\n", pdvobjpriv->traffic_stat.cur_rx_bytes);

	RTW_INFO("last_tx_bytes:%lld\n", pdvobjpriv->traffic_stat.last_tx_bytes);
	RTW_INFO("last_rx_bytes:%lld\n", pdvobjpriv->traffic_stat.last_rx_bytes);

	RTW_INFO("cur_tx_tp:%d (Mbps)\n", pdvobjpriv->traffic_stat.cur_tx_tp);
	RTW_INFO("cur_rx_tp:%d (Mbps)\n", pdvobjpriv->traffic_stat.cur_rx_tp);
	#endif

#ifdef CONFIG_RTW_NAPI
#ifdef CONFIG_RTW_NAPI_DYNAMIC
	dynamic_napi_th_chk (padapter);
#endif /* CONFIG_RTW_NAPI_DYNAMIC */
#endif
	
}

static void rtw_accumulate_fa_count(struct dvobj_priv *dvobj)
{
	u32 fa_count;
	enum phl_band_idx band_idx;

	for (band_idx = HW_BAND_0; band_idx < HW_BAND_MAX; band_idx++) {
		fa_count = rtw_phl_get_phy_stat_info(GET_PHL_INFO(dvobj),
						     band_idx,
						     STAT_INFO_FA_ALL);
		ATOMIC_ADD((ATOMIC_T *)&dvobj->fa_cnt_acc[band_idx],
			   (int)fa_count);
	}
}

#if 0 /*#ifdef CONFIG_CORE_DM_CHK_TIMER*/
void rtw_dynamic_check_timer_handlder(void *ctx)
{
	struct dvobj_priv *pdvobj = (struct dvobj_priv *)ctx;
	_adapter *adapter = dvobj_get_primary_adapter(pdvobj);

	if (!adapter)
		goto exit;

#if (MP_DRIVER == 1)
	if (adapter->registrypriv.mp_mode == 1 && adapter->mppriv.mp_dm == 0) { /* for MP ODM dynamic Tx power tracking */
		/* RTW_INFO("%s mp_dm =0 return\n", __func__); */
		goto exit;
	}
#endif

	if (!rtw_hw_is_init_completed(pdvobj))
		goto exit;

	if (RTW_CANNOT_RUN(pdvobj))
		goto exit;

	collect_traffic_statistics(adapter);
	collect_sta_traffic_statistics(adapter);
	rtw_mi_dynamic_check_timer_handlder(adapter);

	if (!is_drv_in_lps(adapter))
		rtw_dynamic_chk_wk_cmd(adapter);

exit:
	_set_timer(&pdvobj->dynamic_chk_timer, 2000);
}
#endif
#ifdef CONFIG_CMD_GENERAL
void rtw_core_watchdog_sw_hdlr(void *drv_priv)
{
	struct dvobj_priv *pdvobj = (struct dvobj_priv *)drv_priv;
	struct _ADAPTER *adapter = dvobj_get_primary_adapter(pdvobj);

	if (!adapter)
		goto exit;
#if (MP_DRIVER == 1)
	/* for MP ODM dynamic Tx power tracking */
	if (adapter->registrypriv.mp_mode == 1 && adapter->mppriv.mp_dm == 0) {
		/* RTW_INFO("%s mp_dm =0 return\n", __func__); */
		goto exit;
	}
#endif
	if (!rtw_hw_is_init_completed(pdvobj))
		goto exit;

	collect_traffic_statistics(adapter);
	collect_sta_traffic_statistics(adapter);
	rtw_mi_dynamic_check_handlder(adapter);
	rtw_dynamic_chk_wk_sw_hdl(adapter);
	rtw_accumulate_fa_count(pdvobj);

exit:
	return;
}

void rtw_core_watchdog_hw_hdlr(void *drv_priv)
{
	struct dvobj_priv *pdvobj = (struct dvobj_priv *)drv_priv;
	struct _ADAPTER *adapter = dvobj_get_primary_adapter(pdvobj);

	if (!adapter)
		goto exit;
#if (MP_DRIVER == 1)
	/* for MP ODM dynamic Tx power tracking */
	if (adapter->registrypriv.mp_mode == 1 && adapter->mppriv.mp_dm == 0) {
		/* RTW_INFO("%s mp_dm =0 return\n", __func__); */
		goto exit;
	}
#endif
	if (!rtw_hw_is_init_completed(pdvobj))
		goto exit;

	if (RTW_CANNOT_RUN(pdvobj))
		goto exit;

	rtw_dynamic_chk_wk_hw_hdl(adapter);

exit:
	return;
}
#else
int rtw_dynamic_check_handlder(void *ctx, void *parm, bool discard)
{
	struct dvobj_priv *pdvobj = (struct dvobj_priv *)ctx;
	struct _ADAPTER *adapter = dvobj_get_primary_adapter(pdvobj);

	if (!adapter)
		goto exit;

#if (MP_DRIVER == 1)
	/* for MP ODM dynamic Tx power tracking */
	if (adapter->registrypriv.mp_mode == 1 && adapter->mppriv.mp_dm == 0) {
		/* RTW_INFO("%s mp_dm =0 return\n", __func__); */
		goto exit;
	}
#endif

	if (!rtw_hw_is_init_completed(pdvobj))
		goto exit;

	if (RTW_CANNOT_RUN(pdvobj))
		goto exit;

	collect_traffic_statistics(adapter);
	collect_sta_traffic_statistics(adapter);
	rtw_mi_dynamic_check_handlder(adapter);
	rtw_dynamic_chk_wk_hdl(adapter);

exit:

	return 0;
}
#endif

#ifdef CONFIG_LAYER2_ROAMING
/*
* Select a new roaming candidate from the original @param candidate and @param competitor
* @return _TRUE: candidate is updated
* @return _FALSE: candidate is not updated
*/
static int rtw_check_roaming_candidate(struct mlme_priv *mlme
	, struct wlan_network **candidate, struct wlan_network *competitor)
{
	int updated = _FALSE;
	_adapter *adapter = container_of(mlme, _adapter, mlmepriv);
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	RT_CHANNEL_INFO *chset = rfctl->channel_set;
	u8 ch = competitor->network.Configuration.DSConfig;

	if (rtw_chset_search_ch(chset, ch) < 0)
		goto exit;
	if (IS_DFS_SLAVE_WITH_RD(rfctl)
		&& !rtw_rfctl_dfs_domain_unknown(rfctl)
		&& rtw_chset_is_ch_non_ocp(chset, ch))
		goto exit;

	if (is_same_ess(&competitor->network, &mlme->cur_network.network) == _FALSE)
		goto exit;

	if (rtw_is_desired_network(adapter, competitor) == _FALSE)
		goto exit;

#ifdef CONFIG_LAYER2_ROAMING
	if (mlme->need_to_roam == _FALSE)
		goto exit;
#endif

	RTW_INFO("roam candidate:%s %s("MAC_FMT", ch%3u) rssi:%d, age:%5d\n",
		 (competitor == mlme->cur_network_scanned) ? "*" : " " ,
		 competitor->network.Ssid.Ssid,
		 MAC_ARG(competitor->network.MacAddress),
		 competitor->network.Configuration.DSConfig,
		 (int)competitor->network.PhyInfo.rssi,
		 rtw_get_passing_time_ms(competitor->last_scanned)
		);

	/* got specific addr to roam */
	if (!is_zero_mac_addr(mlme->roam_tgt_addr)) {
		if (_rtw_memcmp(mlme->roam_tgt_addr, competitor->network.MacAddress, ETH_ALEN) == _TRUE)
			goto update;
		else
			goto exit;
	}

#ifdef CONFIG_RTW_80211R
	if (rtw_ft_chk_flags(adapter, RTW_FT_PEER_EN)) {
		if (rtw_ft_chk_roaming_candidate(adapter, competitor) == _FALSE)
		goto exit;
	}

#ifdef CONFIG_RTW_WNM
	if (rtw_wnm_btm_diff_bss(adapter) && 
		rtw_wnm_btm_roam_candidate(adapter, competitor)) {
		goto update;
	}	
#endif
#endif

#if 1
	if (rtw_get_passing_time_ms(competitor->last_scanned) >= mlme->roam_scanr_exp_ms)
		goto exit;

	if (competitor->network.PhyInfo.rssi - mlme->cur_network_scanned->network.PhyInfo.rssi < mlme->roam_rssi_diff_th)
		goto exit;

	if (*candidate != NULL && (*candidate)->network.PhyInfo.rssi >= competitor->network.PhyInfo.rssi)
		goto exit;
#else
	goto exit;
#endif

update:
	*candidate = competitor;
	updated = _TRUE;

exit:
	return updated;
}

int rtw_select_roaming_candidate(struct mlme_priv *mlme)
{
	int ret = _FAIL;
	_list	*phead;
	_adapter *adapter;
	_queue	*queue	= &(mlme->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	struct	wlan_network	*candidate = NULL;

	if (mlme->cur_network_scanned == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	_rtw_spinlock_bh(&(mlme->scanned_queue.lock));
	phead = get_list_head(queue);
	adapter = (_adapter *)mlme->nic_hdl;

	mlme->pscanned = get_next(phead);

	while (!rtw_end_of_queue_search(phead, mlme->pscanned)) {

		pnetwork = LIST_CONTAINOR(mlme->pscanned, struct wlan_network, list);
		if (pnetwork == NULL) {
			ret = _FAIL;
			goto exit;
		}

		mlme->pscanned = get_next(mlme->pscanned);

		if (0)
			RTW_INFO("%s("MAC_FMT", ch%u) rssi:%d\n"
				 , pnetwork->network.Ssid.Ssid
				 , MAC_ARG(pnetwork->network.MacAddress)
				 , pnetwork->network.Configuration.DSConfig
				 , (int)pnetwork->network.PhyInfo.rssi);

		rtw_check_roaming_candidate(mlme, &candidate, pnetwork);

	}

	if (candidate == NULL) {
	/*	if parent note lost the path to root and there is no other cadidate, report disconnection	*/
		RTW_INFO("%s: return _FAIL(candidate == NULL)\n", __FUNCTION__);
		ret = _FAIL;
		goto exit;
	} else {
		RTW_INFO("%s: candidate: %s("MAC_FMT", ch:%u)\n", __FUNCTION__,
			candidate->network.Ssid.Ssid, MAC_ARG(candidate->network.MacAddress),
			 candidate->network.Configuration.DSConfig);
		mlme->roam_network = candidate;

		if (_rtw_memcmp(candidate->network.MacAddress, mlme->roam_tgt_addr, ETH_ALEN) == _TRUE)
			_rtw_memset(mlme->roam_tgt_addr, 0, ETH_ALEN);
	}

	ret = _SUCCESS;
exit:
	_rtw_spinunlock_bh(&(mlme->scanned_queue.lock));

	return ret;
}
#endif /* CONFIG_LAYER2_ROAMING */

/*
* Select a new join candidate from the original @param candidate and @param competitor
* @return _TRUE: candidate is updated
* @return _FALSE: candidate is not updated
*/
static int rtw_check_join_candidate(struct mlme_priv *mlme
	    , struct wlan_network **candidate, struct wlan_network *competitor)
{
	int updated = _FALSE;
	_adapter *adapter = container_of(mlme, _adapter, mlmepriv);
	struct rf_ctl_t *rfctl = adapter_to_rfctl(adapter);
	RT_CHANNEL_INFO *chset = rfctl->channel_set;
	u8 ch = competitor->network.Configuration.DSConfig;

	if (rtw_chset_search_ch(chset, ch) < 0)
		goto exit;
	if (IS_DFS_SLAVE_WITH_RD(rfctl)
		&& !rtw_rfctl_dfs_domain_unknown(rfctl)
		&& rtw_chset_is_ch_non_ocp(chset, ch))
		goto exit;

	/* check bssid, if needed */
	if (mlme->assoc_by_bssid == _TRUE) {
		if (_rtw_memcmp(competitor->network.MacAddress, mlme->assoc_bssid, ETH_ALEN) == _FALSE)
			goto exit;
	}

	/* check ssid, if needed */
	if (mlme->assoc_ssid.Ssid[0] && mlme->assoc_ssid.SsidLength) {
		if (competitor->network.Ssid.SsidLength != mlme->assoc_ssid.SsidLength
		    || _rtw_memcmp(competitor->network.Ssid.Ssid, mlme->assoc_ssid.Ssid, mlme->assoc_ssid.SsidLength) == _FALSE
		   )
			goto exit;
	}

	if (rtw_is_desired_network(adapter, competitor)  == _FALSE)
		goto exit;

#ifdef CONFIG_LAYER2_ROAMING
	if (rtw_to_roam(adapter) > 0) {
		if (rtw_get_passing_time_ms(competitor->last_scanned) >= mlme->roam_scanr_exp_ms
		    || is_same_ess(&competitor->network, &mlme->cur_network.network) == _FALSE
		   )
			goto exit;
	}
#endif

	if (*candidate == NULL || (*candidate)->network.PhyInfo.rssi < competitor->network.PhyInfo.rssi) {
		*candidate = competitor;
		updated = _TRUE;
	}

	if (updated) {
		RTW_INFO("[by_bssid:%u][assoc_ssid:%s][to_roam:%u] "
			 "new candidate: %s("MAC_FMT", ch%u) rssi:%d\n",
			 mlme->assoc_by_bssid,
			 mlme->assoc_ssid.Ssid,
			 rtw_to_roam(adapter),
			 (*candidate)->network.Ssid.Ssid,
			 MAC_ARG((*candidate)->network.MacAddress),
			 (*candidate)->network.Configuration.DSConfig,
			 (int)(*candidate)->network.PhyInfo.rssi
			);
	}

exit:
	return updated;
}

/*
Calling context:
The caller of the sub-routine will be in critical section...

The caller must hold the following spinlock

pmlmepriv->lock


*/

int rtw_select_and_join_from_scanned_queue(struct mlme_priv *pmlmepriv)
{
	int ret;
	_list	*phead;
	_adapter *adapter;
	_queue	*queue	= &(pmlmepriv->scanned_queue);
	struct	wlan_network	*pnetwork = NULL;
	struct	wlan_network	*candidate = NULL;
#ifdef CONFIG_ANTENNA_DIVERSITY
	u8		bSupportAntDiv = _FALSE;
#endif
	struct mlme_ext_priv *pmlmeext;
	struct mlme_ext_info *pmlmeinfo;

	adapter = (_adapter *)pmlmepriv->nic_hdl;
	pmlmeext = &adapter->mlmeextpriv;
	pmlmeinfo = &pmlmeext->mlmext_info;

	_rtw_spinlock_bh(&(pmlmepriv->scanned_queue.lock));

#ifdef CONFIG_LAYER2_ROAMING
	if (pmlmepriv->roam_network) {
		candidate = pmlmepriv->roam_network;
		goto candidate_exist;
	}
#endif

	phead = get_list_head(queue);
	pmlmepriv->pscanned = get_next(phead);

	while (!rtw_end_of_queue_search(phead, pmlmepriv->pscanned)) {

		pnetwork = LIST_CONTAINOR(pmlmepriv->pscanned, struct wlan_network, list);
		if (pnetwork == NULL) {
			ret = _FAIL;
			goto exit;
		}

		pmlmepriv->pscanned = get_next(pmlmepriv->pscanned);

		if (0)
			RTW_INFO("%s("MAC_FMT", ch%u) rssi:%d\n"
				 , pnetwork->network.Ssid.Ssid
				 , MAC_ARG(pnetwork->network.MacAddress)
				 , pnetwork->network.Configuration.DSConfig
				 , (int)pnetwork->network.PhyInfo.rssi);

		rtw_check_join_candidate(pmlmepriv, &candidate, pnetwork);

	}

	if (candidate == NULL) {
		RTW_INFO("%s: return _FAIL(candidate == NULL)\n", __FUNCTION__);
#ifdef CONFIG_WOWLAN
		_clr_fwstate_(pmlmepriv, WIFI_ASOC_STATE | WIFI_UNDER_LINKING);
#endif
		ret = _FAIL;
		goto exit;
	} else {
		RTW_INFO("%s: candidate: %s("MAC_FMT", ch:%u)\n", __FUNCTION__,
			candidate->network.Ssid.Ssid, MAC_ARG(candidate->network.MacAddress),
			 candidate->network.Configuration.DSConfig);
		goto candidate_exist;
	}

candidate_exist:

	/* check for situation of  WIFI_ASOC_STATE */
	if (check_fwstate(pmlmepriv, WIFI_ASOC_STATE) == _TRUE) {
		RTW_INFO("%s: WIFI_ASOC_STATE while ask_for_joinbss!!!\n", __FUNCTION__);

#if 0 /* for WPA/WPA2 authentication, wpa_supplicant will expect authentication from AP, it is needed to reconnect AP... */
		if (is_same_network(&pmlmepriv->cur_network.network, &candidate->network)) {
			RTW_INFO("%s: WIFI_ASOC_STATE and is same network, it needn't join again\n", __FUNCTION__);

			rtw_indicate_connect(adapter);/* rtw_indicate_connect again */

			ret = 2;
			goto exit;
		} else
#endif
		{
			rtw_disassoc_cmd(adapter, 0, 0);
			if (1
#ifdef CONFIG_STA_CMD_DISPR
			    && (MLME_IS_STA(adapter) == _FALSE)
#endif /* CONFIG_STA_CMD_DISPR */
			   )
				rtw_free_assoc_resources_cmd(adapter, _TRUE, 0);
			rtw_indicate_disconnect(adapter, 0, _FALSE);

			pmlmeinfo->disconnect_occurred_time = rtw_systime_to_ms(rtw_get_current_time());
			pmlmeinfo->disconnect_code = DISCONNECTION_BY_DRIVER_DUE_TO_CONNECTION_EXIST;
			pmlmeinfo->wifi_reason_code = WLAN_REASON_DEAUTH_LEAVING;
		}
	}

	set_fwstate(pmlmepriv, WIFI_UNDER_LINKING);
	ret = rtw_joinbss_cmd(adapter, candidate);

exit:
	_rtw_spinunlock_bh(&(pmlmepriv->scanned_queue.lock));


	return ret;
}

sint rtw_set_auth(_adapter *adapter, struct security_priv *psecuritypriv)
{
	struct cmd_obj *pcmd;
	struct setauth_parm *psetauthparm;
	struct cmd_priv *pcmdpriv = &(adapter_to_dvobj(adapter)->cmdpriv);
	sint res = _SUCCESS;


	pcmd = (struct	cmd_obj *)rtw_zmalloc(sizeof(struct	cmd_obj));
	if (pcmd == NULL) {
		res = _FAIL; /* try again */
		goto exit;
	}
	pcmd->padapter = adapter;

	psetauthparm = (struct setauth_parm *)rtw_zmalloc(sizeof(struct setauth_parm));
	if (psetauthparm == NULL) {
		rtw_mfree((unsigned char *)pcmd, sizeof(struct	cmd_obj));
		res = _FAIL;
		goto exit;
	}

	_rtw_memset(psetauthparm, 0, sizeof(struct setauth_parm));
	psetauthparm->mode = (unsigned char)psecuritypriv->dot11AuthAlgrthm;

	pcmd->cmdcode = CMD_SET_AUTH; /* _SetAuth_CMD_;*/
	pcmd->parmbuf = (unsigned char *)psetauthparm;
	pcmd->cmdsz = (sizeof(struct setauth_parm));
	pcmd->rsp = NULL;
	pcmd->rspsz = 0;

	_rtw_init_listhead(&pcmd->list);

	res = rtw_enqueue_cmd(pcmdpriv, pcmd);
exit:
	return res;
}

#ifdef CONFIG_CMD_DISP
sint rtw_set_key(_adapter *adapter, struct security_priv *psecuritypriv, sint keyid, u8 set_tx, bool enqueue)
{
	u8 keylen;
	struct setkey_parm setkeyparm;
	sint res = _SUCCESS;

	_rtw_memset(&setkeyparm, 0, sizeof(struct setkey_parm));

	if (psecuritypriv->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) {
		/* BIP use keyid 4 or 5 */
		if (keyid >= 4)
			setkeyparm.algorithm = (unsigned char)psecuritypriv->dot11wCipher;
		else
			setkeyparm.algorithm = (unsigned char)psecuritypriv->dot118021XGrpPrivacy;
	} else {
		setkeyparm.algorithm = (u8)psecuritypriv->dot11PrivacyAlgrthm;

	}
	setkeyparm.keyid = (u8)keyid;/* 0~3 */
	setkeyparm.set_tx = set_tx;
	if (is_wep_enc(setkeyparm.algorithm))
		adapter->securitypriv.key_mask |= BIT(setkeyparm.keyid);

	RTW_INFO("==> rtw_set_key algorithm(%x),keyid(%x),key_mask(%x)\n", setkeyparm.algorithm, setkeyparm.keyid, adapter->securitypriv.key_mask);

	switch (setkeyparm.algorithm) {

	case _WEP40_:
		keylen = 5;
		_rtw_memcpy(&(setkeyparm.key[0]), &(psecuritypriv->dot11DefKey[keyid].skey[0]), keylen);
		break;
	case _WEP104_:
		keylen = 13;
		_rtw_memcpy(&(setkeyparm.key[0]), &(psecuritypriv->dot11DefKey[keyid].skey[0]), keylen);
		break;
	case _TKIP_:
		keylen = 16;
		_rtw_memcpy(&setkeyparm.key, &psecuritypriv->dot118021XGrpKey[keyid], keylen);
		break;
	case _AES_:
	case _GCMP_:
		keylen = 16;
		_rtw_memcpy(&setkeyparm.key, &psecuritypriv->dot118021XGrpKey[keyid], keylen);
		break;
	case _GCMP_256_:
	case _CCMP_256_:
		keylen = 32;
		_rtw_memcpy(&setkeyparm.key, &psecuritypriv->dot118021XGrpKey[keyid], keylen);
		break;
	#ifdef CONFIG_IEEE80211W
	case _BIP_CMAC_128_:
		keylen = 16;
		_rtw_memcpy(&setkeyparm.key, &psecuritypriv->dot11wBIPKey[keyid], keylen);
		break;
	#endif /* CONFIG_IEEE80211W */
	default:
		res = _FAIL;
		goto exit;
	}


	if (enqueue)
		setkey_hdl(adapter, &setkeyparm, PHL_CMD_NO_WAIT, 0);
	else
		setkey_hdl(adapter, &setkeyparm, PHL_CMD_DIRECTLY, 0);
exit:
	return res;

}
#else /* CONFIG_FSM */
sint rtw_set_key(_adapter *adapter, struct security_priv *psecuritypriv, sint keyid, u8 set_tx, bool enqueue)
{
	u8 keylen;
	struct cmd_obj *pcmd;
	struct setkey_parm *psetkeyparm;
	struct cmd_priv	 *pcmdpriv = &(adapter_to_dvobj(adapter)->cmdpriv);
	sint res = _SUCCESS;


	psetkeyparm = (struct setkey_parm *)rtw_zmalloc(sizeof(struct setkey_parm));
	if (psetkeyparm == NULL) {
		res = _FAIL;
		goto exit;
	}
	_rtw_memset(psetkeyparm, 0, sizeof(struct setkey_parm));

	if (psecuritypriv->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) {
		/* BIP use keyid 4 or 5 */
		if (keyid >= 4)
			psetkeyparm->algorithm = (unsigned char)psecuritypriv->dot11wCipher;
		else
			psetkeyparm->algorithm = (unsigned char)psecuritypriv->dot118021XGrpPrivacy;
	} else {
		psetkeyparm->algorithm = (u8)psecuritypriv->dot11PrivacyAlgrthm;

	}
	psetkeyparm->keyid = (u8)keyid;/* 0~3 */
	psetkeyparm->set_tx = set_tx;
	if (is_wep_enc(psetkeyparm->algorithm))
		adapter->securitypriv.key_mask |= BIT(psetkeyparm->keyid);

	RTW_INFO("==> rtw_set_key algorithm(%x),keyid(%x),key_mask(%x)\n", psetkeyparm->algorithm, psetkeyparm->keyid, adapter->securitypriv.key_mask);

	switch (psetkeyparm->algorithm) {

	case _WEP40_:
		keylen = 5;
		_rtw_memcpy(&(psetkeyparm->key[0]), &(psecuritypriv->dot11DefKey[keyid].skey[0]), keylen);
		break;
	case _WEP104_:
		keylen = 13;
		_rtw_memcpy(&(psetkeyparm->key[0]), &(psecuritypriv->dot11DefKey[keyid].skey[0]), keylen);
		break;
	case _TKIP_:
		keylen = 16;
		_rtw_memcpy(&psetkeyparm->key, &psecuritypriv->dot118021XGrpKey[keyid], keylen);
		break;
	case _AES_:
	case _GCMP_:
		keylen = 16;
		_rtw_memcpy(&psetkeyparm->key, &psecuritypriv->dot118021XGrpKey[keyid], keylen);
		break;
	case _GCMP_256_:
	case _CCMP_256_:
		keylen = 32;
		_rtw_memcpy(&psetkeyparm->key, &psecuritypriv->dot118021XGrpKey[keyid], keylen);
		break;
	#ifdef CONFIG_IEEE80211W
	case _BIP_CMAC_128_:
		keylen = 16;
		_rtw_memcpy(&psetkeyparm->key, &psecuritypriv->dot11wBIPKey[keyid], keylen);
		break;
	#endif /* CONFIG_IEEE80211W */
	default:
		res = _FAIL;
		rtw_mfree((unsigned char *)psetkeyparm, sizeof(struct setkey_parm));
		goto exit;
	}


	if (enqueue) {
		pcmd = (struct	cmd_obj *)rtw_zmalloc(sizeof(struct	cmd_obj));
		if (pcmd == NULL) {
			rtw_mfree((unsigned char *)psetkeyparm, sizeof(struct setkey_parm));
			res = _FAIL; /* try again */
			goto exit;
		}
		pcmd->padapter = adapter;

		pcmd->cmdcode = CMD_SET_KEY; /*_SetKey_CMD_*/
		pcmd->parmbuf = (u8 *)psetkeyparm;
		pcmd->cmdsz = (sizeof(struct setkey_parm));
		pcmd->rsp = NULL;
		pcmd->rspsz = 0;

		_rtw_init_listhead(&pcmd->list);

		/* _rtw_init_sema(&(pcmd->cmd_sem), 0); */

		res = rtw_enqueue_cmd(pcmdpriv, pcmd);
	} else {
		setkey_hdl(adapter, (u8 *)psetkeyparm);
		rtw_mfree((u8 *) psetkeyparm, sizeof(struct setkey_parm));
	}
exit:
	return res;

}
#endif
#ifdef CONFIG_WMMPS_STA
/*
 * rtw_uapsd_use_default_setting
 * This function is used for setting default uapsd max sp length to uapsd_max_sp_len
 * in qos_priv data structure from registry. In additional, it will also map default uapsd 
 * ac to each uapsd TID, delivery-enabled and trigger-enabled of corresponding TID. 
 * 
 * Arguments:
 * @padapter: _adapter pointer.
 *
 * Auther: Arvin Liu
 * Date: 2017/05/03
 */
void	rtw_uapsd_use_default_setting(_adapter *padapter)
{
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct qos_priv		*pqospriv = &pmlmepriv->qospriv;
	struct registry_priv		*pregistrypriv = &padapter->registrypriv;

	if (pregistrypriv->uapsd_ac_enable != 0) {
		pqospriv->uapsd_max_sp_len = pregistrypriv->uapsd_max_sp_len;
		
		CLEAR_FLAGS(pqospriv->uapsd_tid);
		CLEAR_FLAGS(pqospriv->uapsd_tid_delivery_enabled);
		CLEAR_FLAGS(pqospriv->uapsd_tid_trigger_enabled);

		/* check the uapsd setting of AC_VO from registry then map these setting to each TID if necessary  */
		if(TEST_FLAG(pregistrypriv->uapsd_ac_enable, DRV_CFG_UAPSD_VO)) {
			SET_FLAG(pqospriv->uapsd_tid, WMM_TID7);
			SET_FLAG(pqospriv->uapsd_tid_delivery_enabled, WMM_TID7);
			SET_FLAG(pqospriv->uapsd_tid_trigger_enabled, WMM_TID7);
			SET_FLAG(pqospriv->uapsd_tid, WMM_TID6);
			SET_FLAG(pqospriv->uapsd_tid_delivery_enabled, WMM_TID6);
			SET_FLAG(pqospriv->uapsd_tid_trigger_enabled, WMM_TID6);
		}

		/* check the uapsd setting of AC_VI from registry then map these setting to each TID if necessary  */
		if(TEST_FLAG(pregistrypriv->uapsd_ac_enable, DRV_CFG_UAPSD_VI)) {	
			SET_FLAG(pqospriv->uapsd_tid, WMM_TID5);
			SET_FLAG(pqospriv->uapsd_tid_delivery_enabled, WMM_TID5);
			SET_FLAG(pqospriv->uapsd_tid_trigger_enabled, WMM_TID5);
			SET_FLAG(pqospriv->uapsd_tid, WMM_TID4);
			SET_FLAG(pqospriv->uapsd_tid_delivery_enabled, WMM_TID4);
			SET_FLAG(pqospriv->uapsd_tid_trigger_enabled, WMM_TID4);
		}

		/* check the uapsd setting of AC_BK from registry then map these setting to each TID if necessary  */
		if(TEST_FLAG(pregistrypriv->uapsd_ac_enable, DRV_CFG_UAPSD_BK)) {
			SET_FLAG(pqospriv->uapsd_tid, WMM_TID2);
			SET_FLAG(pqospriv->uapsd_tid_delivery_enabled, WMM_TID2);
			SET_FLAG(pqospriv->uapsd_tid_trigger_enabled, WMM_TID2);
			SET_FLAG(pqospriv->uapsd_tid, WMM_TID1);
			SET_FLAG(pqospriv->uapsd_tid_delivery_enabled, WMM_TID1);
			SET_FLAG(pqospriv->uapsd_tid_trigger_enabled, WMM_TID1);
		}

		/* check the uapsd setting of AC_BE from registry then map these setting to each TID if necessary  */
		if(TEST_FLAG(pregistrypriv->uapsd_ac_enable, DRV_CFG_UAPSD_BE)) {
			SET_FLAG(pqospriv->uapsd_tid, WMM_TID3);
			SET_FLAG(pqospriv->uapsd_tid_delivery_enabled, WMM_TID3);
			SET_FLAG(pqospriv->uapsd_tid_trigger_enabled, WMM_TID3);
			SET_FLAG(pqospriv->uapsd_tid, WMM_TID0);
			SET_FLAG(pqospriv->uapsd_tid_delivery_enabled, WMM_TID0);
			SET_FLAG(pqospriv->uapsd_tid_trigger_enabled, WMM_TID0);
		}

		RTW_INFO("[WMMPS] UAPSD MAX SP Len = 0x%02x, UAPSD TID enabled = 0x%02x\n", 
			pqospriv->uapsd_max_sp_len, (u8)pqospriv->uapsd_tid);
	}

}

/*
 * rtw_is_wmmps_mode
 * This function is used for checking whether Driver and an AP support uapsd function or not.
 * If both of them support uapsd function, it will return true. Otherwise returns false.
 * 
 * Arguments:
 * @padapter: _adapter pointer.
 *
 * Auther: Arvin Liu
 * Date: 2017/06/12
 */
bool rtw_is_wmmps_mode(_adapter *padapter) 
{
	struct mlme_priv	*pmlmepriv = &(padapter->mlmepriv);
	struct qos_priv	*pqospriv = &pmlmepriv->qospriv;
		
	if ((pqospriv->uapsd_ap_supported) && ((pqospriv->uapsd_tid & BIT_MASK_TID_TC)  != 0))
		return _TRUE;

	return _FALSE;
}
#endif /* CONFIG_WMMPS_STA */

/* adjust IEs for rtw_joinbss_cmd in WMM */
int rtw_restruct_wmm_ie(_adapter *adapter, u8 *in_ie, u8 *out_ie, uint in_len, uint initial_out_len)
{
#ifdef CONFIG_WMMPS_STA
	struct mlme_priv		*pmlmepriv = &adapter->mlmepriv;
	struct qos_priv		*pqospriv = &pmlmepriv->qospriv;
#endif /* CONFIG_WMMPS_STA */
	unsigned	int ielength = 0;
	unsigned int i, j;
	u8 qos_info = 0;

	i = 12; /* after the fixed IE */
	while (i < in_len) {
		ielength = initial_out_len;

		if (in_ie[i] == 0xDD && in_ie[i + 2] == 0x00 && in_ie[i + 3] == 0x50  && in_ie[i + 4] == 0xF2 && in_ie[i + 5] == 0x02 && i + 5 < in_len) { /* WMM element ID and OUI */

			/* Append WMM IE to the last index of out_ie */
#if 0
			for (j = i; j < i + (in_ie[i + 1] + 2); j++) {
				out_ie[ielength] = in_ie[j];
				ielength++;
			}
			out_ie[initial_out_len + 8] = 0x00; /* force the QoS Info Field to be zero */
#endif

			for (j = i; j < i + 9; j++) {
				out_ie[ielength] = in_ie[j];
				ielength++;
			}
			out_ie[initial_out_len + 1] = 0x07;
			out_ie[initial_out_len + 6] = 0x00;

#ifdef CONFIG_WMMPS_STA
			switch(pqospriv->uapsd_max_sp_len) {
				case NO_LIMIT: 
					/* do nothing */
					break;
				case TWO_MSDU: 
					SET_FLAG(qos_info, BIT5);
					break;
				case FOUR_MSDU: 
					SET_FLAG(qos_info, BIT6);
					break;	
				case SIX_MSDU: 
					SET_FLAG(qos_info, BIT5);
					SET_FLAG(qos_info, BIT6);
					break;
				default:
					/* do nothing */
					break;
			};

			/* check TID7 and TID6 for AC_VO to set corresponding Qos_info bit in WMM IE  */
			if((TEST_FLAG(pqospriv->uapsd_tid, WMM_TID7)) && (TEST_FLAG(pqospriv->uapsd_tid, WMM_TID6)))
				SET_FLAG(qos_info, WMM_IE_UAPSD_VO);
			/* check TID5 and TID4 for AC_VI to set corresponding Qos_info bit in WMM IE  */
			if((TEST_FLAG(pqospriv->uapsd_tid, WMM_TID5)) && (TEST_FLAG(pqospriv->uapsd_tid, WMM_TID4)))
				SET_FLAG(qos_info, WMM_IE_UAPSD_VI);
			/* check TID2 and TID1 for AC_BK to set corresponding Qos_info bit in WMM IE  */
			if((TEST_FLAG(pqospriv->uapsd_tid, WMM_TID2)) && (TEST_FLAG(pqospriv->uapsd_tid, WMM_TID1)))
				SET_FLAG(qos_info, WMM_IE_UAPSD_BK);
			/* check TID3 and TID0 for AC_BE to set corresponding Qos_info bit in WMM IE  */
			if((TEST_FLAG(pqospriv->uapsd_tid, WMM_TID3)) && (TEST_FLAG(pqospriv->uapsd_tid, WMM_TID0)))
				SET_FLAG(qos_info, WMM_IE_UAPSD_BE);
#endif /* CONFIG_WMMPS_STA */
			
			out_ie[initial_out_len + 8] = qos_info;

			break;
		}

		i += (in_ie[i + 1] + 2); /* to the next IE element */
	}

	return ielength;

}


/*
 * Ported from 8185: IsInPreAuthKeyList(). (Renamed from SecIsInPreAuthKeyList(), 2006-10-13.)
 * Added by Annie, 2006-05-07.
 *
 * Search by BSSID,
 * Return Value:
 *		-1		:if there is no pre-auth key in the  table
 *		>=0		:if there is pre-auth key, and   return the entry id
 *
 *   */

static int SecIsInPMKIDList(_adapter *adapter, u8 *bssid)
{
	struct security_priv *psecuritypriv = &adapter->securitypriv;
	int i = 0;

	do {
		if ((psecuritypriv->PMKIDList[i].bUsed) &&
		    (_rtw_memcmp(psecuritypriv->PMKIDList[i].Bssid, bssid, ETH_ALEN) == _TRUE))
			break;
		else {
			i++;
			/* continue; */
		}

	} while (i < NUM_PMKID_CACHE);

	if (i == NUM_PMKID_CACHE) {
		i = -1;/* Could not find. */
	} else {
		/* There is one Pre-Authentication Key for the specific BSSID. */
	}

	return i;

}

int rtw_cached_pmkid(_adapter *adapter, u8 *bssid)
{
	return SecIsInPMKIDList(adapter, bssid);
}

int rtw_rsn_sync_pmkid(_adapter *adapter, u8 *ie, uint ie_len, int i_ent)
{
	struct security_priv *sec = &adapter->securitypriv;
	struct rsne_info info;
	u8 gm_cs[4];
	int i;

	rtw_rsne_info_parse(ie, ie_len, &info);

	if (info.err) {
		RTW_WARN(FUNC_ADPT_FMT" rtw_rsne_info_parse error\n"
			, FUNC_ADPT_ARG(adapter));
		return 0;
	}

	if (i_ent < 0 && info.pmkid_cnt == 0)
		goto exit;

	if (i_ent >= 0 && info.pmkid_cnt == 1 && _rtw_memcmp(info.pmkid_list, sec->PMKIDList[i_ent].PMKID, 16)) {
		RTW_INFO(FUNC_ADPT_FMT" has carried the same PMKID:"KEY_FMT"\n"
			, FUNC_ADPT_ARG(adapter), KEY_ARG(&sec->PMKIDList[i_ent].PMKID));
		goto exit;
	}

	/* bakcup group mgmt cs */
	if (info.gmcs)
		_rtw_memcpy(gm_cs, info.gmcs, 4);

	if (info.pmkid_cnt) {
		RTW_INFO(FUNC_ADPT_FMT" remove original PMKID, count:%u\n"
			 , FUNC_ADPT_ARG(adapter), info.pmkid_cnt);
		for (i = 0; i < info.pmkid_cnt; i++)
			RTW_INFO("    "KEY_FMT"\n", KEY_ARG(info.pmkid_list + i * 16));
	}

	if (i_ent >= 0) {
		RTW_INFO(FUNC_ADPT_FMT" append PMKID:"KEY_FMT"\n"
			, FUNC_ADPT_ARG(adapter), KEY_ARG(sec->PMKIDList[i_ent].PMKID));

		info.pmkid_cnt = 1; /* update new pmkid_cnt */
		_rtw_memcpy(info.pmkid_list, sec->PMKIDList[i_ent].PMKID, 16);
	} else
		info.pmkid_cnt = 0; /* update new pmkid_cnt */

	RTW_PUT_LE16(info.pmkid_list - 2, info.pmkid_cnt);
	if (info.gmcs)
		_rtw_memcpy(info.pmkid_list + 16 * info.pmkid_cnt, gm_cs, 4);

	ie_len = 1 + 1 + 2 + 4
		+ 2 + 4 * info.pcs_cnt
		+ 2 + 4 * info.akm_cnt
		+ 2
		+ 2 + 16 * info.pmkid_cnt
		+ (info.gmcs ? 4 : 0)
		;
	
	ie[1] = (u8)(ie_len - 2);

exit:
	return ie_len;
}

sint rtw_restruct_sec_ie(_adapter *adapter, u8 *out_ie)
{
	u8 authmode = 0x0;
	uint	ielength = 0;
	int iEntry;

	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct security_priv *psecuritypriv = &adapter->securitypriv;
	uint	ndisauthmode = psecuritypriv->ndisauthtype;

	if ((ndisauthmode == Ndis802_11AuthModeWPA) || (ndisauthmode == Ndis802_11AuthModeWPAPSK))
		authmode = _WPA_IE_ID_;
	if ((ndisauthmode == Ndis802_11AuthModeWPA2) || (ndisauthmode == Ndis802_11AuthModeWPA2PSK))
		authmode = _WPA2_IE_ID_;

	if (check_fwstate(pmlmepriv, WIFI_UNDER_WPS)) {
		_rtw_memcpy(out_ie, psecuritypriv->wps_ie, psecuritypriv->wps_ie_len);
		ielength = psecuritypriv->wps_ie_len;

	} else if ((authmode == _WPA_IE_ID_) || (authmode == _WPA2_IE_ID_)) {
		/* copy RSN or SSN		 */
		_rtw_memcpy(out_ie, psecuritypriv->supplicant_ie, psecuritypriv->supplicant_ie[1] + 2);
		/* debug for CONFIG_IEEE80211W
		{
			int jj;
			printk("supplicant_ie_length=%d &&&&&&&&&&&&&&&&&&&\n", psecuritypriv->supplicant_ie[1]+2);
			for(jj=0; jj < psecuritypriv->supplicant_ie[1]+2; jj++)
				printk(" %02x ", psecuritypriv->supplicant_ie[jj]);
			printk("\n");
		}*/
		ielength = psecuritypriv->supplicant_ie[1] + 2;
		rtw_report_sec_ie(adapter, authmode, psecuritypriv->supplicant_ie);
	}

	if (authmode == WLAN_EID_RSN) {
		iEntry = SecIsInPMKIDList(adapter, pmlmepriv->assoc_bssid);
		ielength = rtw_rsn_sync_pmkid(adapter, out_ie, ielength, iEntry);
	}

	if ((psecuritypriv->auth_type == MLME_AUTHTYPE_SAE) &&
		(psecuritypriv->rsnx_ie_len >= 3)) {
		u8 *_pos = out_ie + \
			(psecuritypriv->supplicant_ie[1] + 2);
		_rtw_memcpy(_pos, psecuritypriv->rsnx_ie,
			psecuritypriv->rsnx_ie_len);
		ielength += psecuritypriv->rsnx_ie_len;
		RTW_INFO_DUMP("update IE for RSNX :",
			out_ie, ielength);
	}


	return ielength;
}

void rtw_init_registrypriv_dev_network(_adapter *adapter)
{
	struct registry_priv *pregistrypriv = &adapter->registrypriv;
	WLAN_BSSID_EX    *pdev_network = &pregistrypriv->dev_network;
	u8 *myhwaddr = adapter_mac_addr(adapter);


	_rtw_memcpy(pdev_network->MacAddress, myhwaddr, ETH_ALEN);

	_rtw_memcpy(&pdev_network->Ssid, &pregistrypriv->ssid, sizeof(NDIS_802_11_SSID));

	pdev_network->Configuration.Length = sizeof(NDIS_802_11_CONFIGURATION);
	pdev_network->Configuration.BeaconPeriod = 100;
}

void rtw_update_registrypriv_dev_network(_adapter *adapter)
{
	int sz = 0;
	struct registry_priv *pregistrypriv = &adapter->registrypriv;
	WLAN_BSSID_EX    *pdev_network = &pregistrypriv->dev_network;
	struct	security_priv	*psecuritypriv = &adapter->securitypriv;
	struct	wlan_network	*cur_network = &adapter->mlmepriv.cur_network;
	/* struct	xmit_priv	*pxmitpriv = &adapter->xmitpriv; */
	struct mlme_ext_priv	*pmlmeext = &adapter->mlmeextpriv;


#if 0
	pxmitpriv->vcs_setting = pregistrypriv->vrtl_carrier_sense;
	pxmitpriv->vcs = pregistrypriv->vcs_type;
	pxmitpriv->vcs_type = pregistrypriv->vcs_type;
	/* pxmitpriv->rts_thresh = pregistrypriv->rts_thresh; */
	pxmitpriv->frag_len = pregistrypriv->frag_thresh;

	adapter->qospriv.qos_option = pregistrypriv->wmm_enable;
#endif

	pdev_network->Privacy = (psecuritypriv->dot11PrivacyAlgrthm > 0 ? 1 : 0) ; /* adhoc no 802.1x */

	pdev_network->PhyInfo.rssi = 0;

	pdev_network->Configuration.DSConfig = (pregistrypriv->channel);

	if (cur_network->network.InfrastructureMode == Ndis802_11IBSS) {
		pdev_network->Configuration.ATIMWindow = (0);

		if (pmlmeext->chandef.chan != 0)
			pdev_network->Configuration.DSConfig = pmlmeext->chandef.chan;
		else
			pdev_network->Configuration.DSConfig = 1;
	}

	pdev_network->InfrastructureMode = (cur_network->network.InfrastructureMode);

	/* 1. Supported rates */
	/* 2. IE */

	/* rtw_set_supported_rate(pdev_network->SupportedRates, pregistrypriv->wireless_mode) ; */ /* will be called in rtw_generate_ie */
	sz = rtw_generate_ie(pregistrypriv);

	pdev_network->IELength = sz;

	pdev_network->Length = get_WLAN_BSSID_EX_sz((WLAN_BSSID_EX *)pdev_network);

	/* notes: translate IELength & Length after assign the Length to cmdsz in createbss_cmd(); */
	/* pdev_network->IELength = cpu_to_le32(sz); */


}

void rtw_get_encrypt_decrypt_from_registrypriv(_adapter *adapter)
{



}

/* the fucntion is at passive_level */
void rtw_joinbss_reset(_adapter *padapter)
{
	u8	threshold;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	/* todo: if you want to do something io/reg/hw setting before join_bss, please add code here */

#ifdef CONFIG_80211N_HT
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;

	pmlmepriv->num_FortyMHzIntolerant = 0;

	pmlmepriv->num_sta_no_ht = 0;

	phtpriv->ampdu_enable = _FALSE;/* reset to disabled */

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	/* TH=1 => means that invalidate usb rx aggregation */
	/* TH=0 => means that validate usb rx aggregation, use init value. */
	if (phtpriv->ht_option) {
		if (padapter->registrypriv.wifi_spec == 1)
			threshold = 1;
		else
			threshold = 0;
		rtw_hal_set_hwreg(padapter, HW_VAR_RXDMA_AGG_PG_TH, (u8 *)(&threshold));
	} else {
		threshold = 1;
		rtw_hal_set_hwreg(padapter, HW_VAR_RXDMA_AGG_PG_TH, (u8 *)(&threshold));
	}
#endif/* #if defined( CONFIG_USB_HCI) || defined (CONFIG_SDIO_HCI) */

#endif/* #ifdef CONFIG_80211N_HT */

}


#ifdef CONFIG_80211N_HT
void rtw_ht_get_dft_setting(_adapter *padapter,
				struct protocol_cap_t *dft_proto_cap,
				struct role_cap_t *dft_cap)
{
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
	struct registry_priv	*pregistrypriv = &padapter->registrypriv;
	BOOLEAN		bHwLDPCSupport = _FALSE, bHwSTBCSupport = _FALSE;
	u8		stbc_rx = 0;
#ifdef CONFIG_BEAMFORMING
	BOOLEAN		bHwSupportBeamformer = _FALSE, bHwSupportBeamformee = _FALSE;
#endif /* CONFIG_BEAMFORMING */

	if (pregistrypriv->wifi_spec)
		phtpriv->bss_coexist = 1;
	else
		phtpriv->bss_coexist = 0;

	/*dft_proto_cap->sgi_40;*/
	phtpriv->sgi_40m = TEST_FLAG(pregistrypriv->short_gi, BIT1) ? _TRUE : _FALSE;
	/*dft_proto_cap->sgi_20;*/
	phtpriv->sgi_20m = TEST_FLAG(pregistrypriv->short_gi, BIT0) ? _TRUE : _FALSE;

	/* LDPC support */
	CLEAR_FLAGS(phtpriv->ldpc_cap);
	bHwLDPCSupport = (dft_proto_cap->ht_ldpc) ? _TRUE : _FALSE;
	if (bHwLDPCSupport) {
		if (TEST_FLAG(pregistrypriv->ldpc_cap, BIT4))
			SET_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_RX);
	}

	bHwLDPCSupport = (dft_cap->tx_ht_ldpc) ? _TRUE : _FALSE;
	if (bHwLDPCSupport) {
		if (TEST_FLAG(pregistrypriv->ldpc_cap, BIT5))
			SET_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_TX);
	}
	if (phtpriv->ldpc_cap)
		RTW_INFO("[HT] HAL Support LDPC = 0x%02X\n", phtpriv->ldpc_cap);

	/* STBC */
	CLEAR_FLAGS(phtpriv->stbc_cap);
	if (dft_proto_cap->stbc_ht_tx)
		SET_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_TX);

	if (dft_proto_cap->stbc_ht_rx)
		SET_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_RX);

	if (phtpriv->stbc_cap)
		RTW_INFO("[HT] HAL Support STBC = 0x%02X\n", phtpriv->stbc_cap);

	/* Beamforming setting */
	CLEAR_FLAGS(phtpriv->beamform_cap);
#ifdef CONFIG_BEAMFORMING
	/* only enable beamforming in STA client mode */
	if (MLME_IS_STA(padapter) && !MLME_IS_GC(padapter)
				  && !MLME_IS_ADHOC(padapter)
				  && !MLME_IS_MESH(padapter))
	{
		bHwSupportBeamformer = (dft_proto_cap->ht_su_bfmr) ? _TRUE : _FALSE;
		bHwSupportBeamformee = (dft_proto_cap->ht_su_bfme) ? _TRUE : _FALSE;

		if (TEST_FLAG(pregistrypriv->beamform_cap, BIT4) && bHwSupportBeamformer) {
			SET_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMER_ENABLE);
			RTW_INFO("[HT] HAL Support Beamformer\n");
		}
		if (TEST_FLAG(pregistrypriv->beamform_cap, BIT5) && bHwSupportBeamformee) {
			SET_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMEE_ENABLE);
			RTW_INFO("[HT] HAL Support Beamformee\n");
		}
	}
#endif /* CONFIG_BEAMFORMING */
}

void	rtw_ht_use_default_setting(_adapter *padapter)
{
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
	struct registry_priv	*pregistrypriv = &padapter->registrypriv;
	BOOLEAN		bHwLDPCSupport = _FALSE, bHwSTBCSupport = _FALSE;
	u8		stbc_rx = 0;
#ifdef CONFIG_BEAMFORMING
	BOOLEAN		bHwSupportBeamformer = _FALSE, bHwSupportBeamformee = _FALSE;
#endif /* CONFIG_BEAMFORMING */

	if (pregistrypriv->wifi_spec)
		phtpriv->bss_coexist = 1;
	else
		phtpriv->bss_coexist = 0;

	phtpriv->sgi_40m = TEST_FLAG(pregistrypriv->short_gi, BIT1) ? _TRUE : _FALSE;
	phtpriv->sgi_20m = TEST_FLAG(pregistrypriv->short_gi, BIT0) ? _TRUE : _FALSE;

	/* LDPC support */
	if (padapter->phl_role->proto_role_cap.ht_ldpc)
		SET_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_RX);

	if (padapter->phl_role->cap.tx_ht_ldpc)
		SET_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_TX);

	if (phtpriv->ldpc_cap)
		RTW_INFO("[HT] HAL Support LDPC = 0x%02X\n", phtpriv->ldpc_cap);

	/* STBC */
	if (padapter->phl_role->proto_role_cap.stbc_ht_tx)
		SET_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_TX);

	if (padapter->phl_role->proto_role_cap.stbc_ht_rx)
		SET_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_RX);

	if (phtpriv->stbc_cap)
		RTW_INFO("[HT] HAL Support STBC = 0x%02X\n", phtpriv->stbc_cap);

	/* Beamforming setting */
	CLEAR_FLAGS(phtpriv->beamform_cap);
#ifdef CONFIG_BEAMFORMING
	/* only enable beamforming in STA client mode */
	if (MLME_IS_STA(padapter) && !MLME_IS_GC(padapter)
				  && !MLME_IS_ADHOC(padapter)
				  && !MLME_IS_MESH(padapter))
	{
		if (padapter->phl_role->proto_role_cap.ht_su_bfmr) {
			SET_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMER_ENABLE);
			RTW_INFO("[HT] HAL Support Beamformer\n");
		}

		if (padapter->phl_role->proto_role_cap.ht_su_bfme) {
			SET_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMEE_ENABLE);
			RTW_INFO("[HT] HAL Support Beamformee\n");
		}
	}
#endif /* CONFIG_BEAMFORMING */
}
void rtw_build_wmm_ie_ht(_adapter *padapter, u8 *out_ie, uint *pout_len)
{
	unsigned char WMM_IE[] = {0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, 0x00};
	int out_len;
	u8 *pframe;

	if (padapter->mlmepriv.qospriv.qos_option == 0) {
		out_len = *pout_len;
		pframe = rtw_set_ie(out_ie + out_len, _VENDOR_SPECIFIC_IE_,
				    _WMM_IE_Length_, WMM_IE, pout_len);

		padapter->mlmepriv.qospriv.qos_option = 1;
	}
}
#if defined(CONFIG_80211N_HT)
/* the fucntion is >= passive_level */
unsigned int rtw_restructure_ht_ie(_adapter *padapter, u8 *in_ie, u8 *out_ie, uint in_len, uint *pout_len, u8 channel, struct country_chplan *req_chplan)
{
	u32 ielen, out_len;
	HT_CAP_AMPDU_FACTOR max_rx_ampdu_factor = 0;
	HT_CAP_AMPDU_DENSITY best_ampdu_density = 0;
	unsigned char *p, *pframe;
	enum band_type band = channel > 14 ? BAND_ON_5G : BAND_ON_24G;
	struct rtw_ieee80211_ht_cap ht_capie;
	u8	cbw40_enable = 0, rf_num = 0, rx_stbc_nss = 0, rx_nss = 0;
	struct registry_priv *pregistrypriv = &padapter->registrypriv;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
#ifdef CONFIG_80211AC_VHT
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	struct vht_priv	*pvhtpriv = &pmlmepriv->vhtpriv;
#endif /* CONFIG_80211AC_VHT */

	phtpriv->ht_option = _FALSE;

	out_len = *pout_len;

	_rtw_memset(&ht_capie, 0, sizeof(struct rtw_ieee80211_ht_cap));

	ht_capie.cap_info = IEEE80211_HT_CAP_DSSSCCK40;

	if (phtpriv->sgi_20m)
		ht_capie.cap_info |= IEEE80211_HT_CAP_SGI_20;

	/* check if 40MHz is allowed according to hal cap and registry */
	if (rtw_hw_chk_bw_cap(adapter_to_dvobj(padapter), BW_CAP_40M)) {
		if (band == BAND_ON_5G) {
			if (REGSTY_IS_BW_5G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		} else if (band == BAND_ON_24G) {
			if (REGSTY_IS_BW_2G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		}
	}

	if (cbw40_enable) {
		struct rf_ctl_t *rfctl = adapter_to_rfctl(padapter);
		RT_CHANNEL_INFO *chset = rfctl->channel_set;
		u8 oper_bw = CHANNEL_WIDTH_20, oper_offset = CHAN_OFFSET_NO_EXT;

		if (in_ie == NULL) {
			/* TDLS: TODO 20/40 issue */
			if (MLME_IS_STA(padapter)) {
				oper_bw = padapter->mlmeextpriv.chandef.bw;
				if (oper_bw > CHANNEL_WIDTH_40)
					oper_bw = CHANNEL_WIDTH_40;
			} else
				/* TDLS: TODO 40? */
				oper_bw = CHANNEL_WIDTH_40;
		} else {
			p = rtw_get_ie(in_ie, WLAN_EID_HT_OPERATION, &ielen, in_len);
			if (p && ielen == HT_OP_IE_LEN) {
				if (GET_HT_OP_ELE_STA_CHL_WIDTH(p + 2)) {
					switch (GET_HT_OP_ELE_2ND_CHL_OFFSET(p + 2)) {
					case IEEE80211_SCA:
						oper_bw = CHANNEL_WIDTH_40;
						oper_offset = CHAN_OFFSET_UPPER;
						break;
					case IEEE80211_SCB:
						oper_bw = CHANNEL_WIDTH_40;
						oper_offset = CHAN_OFFSET_LOWER;
						break;
					}
				}
			}
			/* IOT issue : AP TP-Link WDR6500 */
			if(oper_bw == CHANNEL_WIDTH_40){ 
				p = rtw_get_ie(in_ie, WLAN_EID_HT_CAP, &ielen, in_len);
				if (p && ielen == HT_CAP_IE_LEN) {
					oper_bw = GET_HT_CAP_ELE_CHL_WIDTH(p + 2)  ? CHANNEL_WIDTH_40 : CHANNEL_WIDTH_20;
					if(oper_bw == CHANNEL_WIDTH_20)
						oper_offset = CHAN_OFFSET_NO_EXT;
				}
			}
		}

		/* adjust bw to fit in channel plan setting */
		if (oper_bw == CHANNEL_WIDTH_40
			&& oper_offset != CHAN_OFFSET_NO_EXT /* check this because TDLS has no info to set offset */
		) {
			if ((req_chplan && !rtw_country_chplan_is_chbw_valid(req_chplan, band, channel, oper_bw, oper_offset, 1, 1, pregistrypriv))
				|| (!req_chplan && !rtw_chset_is_chbw_valid(chset, channel, oper_bw, oper_offset, 1, 1))
				|| (IS_DFS_SLAVE_WITH_RD(rfctl)
					&& !rtw_rfctl_dfs_domain_unknown(rfctl)
					&& rtw_chset_is_chbw_non_ocp(chset, channel, oper_bw, oper_offset))
			) {
				oper_bw = CHANNEL_WIDTH_20;
				oper_offset = CHAN_OFFSET_NO_EXT;
				rtw_warn_on(req_chplan && !rtw_country_chplan_is_chbw_valid(req_chplan, band, channel, oper_bw, oper_offset, 1, 1, pregistrypriv));
				rtw_warn_on(!req_chplan && !rtw_chset_is_chbw_valid(chset, channel, oper_bw, oper_offset, 1, 1));
				if (IS_DFS_SLAVE_WITH_RD(rfctl) && !rtw_rfctl_dfs_domain_unknown(rfctl))
					rtw_warn_on(rtw_chset_is_chbw_non_ocp(chset, channel, oper_bw, oper_offset));
			}
		}

		if (oper_bw == CHANNEL_WIDTH_40) {
			ht_capie.cap_info |= IEEE80211_HT_CAP_SUP_WIDTH;
			if (phtpriv->sgi_40m)
				ht_capie.cap_info |= IEEE80211_HT_CAP_SGI_40;
		}

		cbw40_enable = oper_bw == CHANNEL_WIDTH_40 ? 1 : 0;
	}

	/* todo: disable SM power save mode */
	ht_capie.cap_info |= IEEE80211_HT_CAP_SM_PS;

	/* RX LDPC */
	if (TEST_FLAG(phtpriv->ldpc_cap, LDPC_HT_ENABLE_RX)) {
		ht_capie.cap_info |= IEEE80211_HT_CAP_LDPC_CODING;
		RTW_INFO("[HT] Declare supporting RX LDPC\n");
	}

	/* TX STBC */
	if (TEST_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_TX)) {
		ht_capie.cap_info |= IEEE80211_HT_CAP_TX_STBC;
		RTW_INFO("[HT] Declare supporting TX STBC\n");
	}

	/* RX STBC */
	if (TEST_FLAG(phtpriv->stbc_cap, STBC_HT_ENABLE_RX) ||
			(pregistrypriv->wifi_spec == 1)) {
		rx_stbc_nss = padapter->phl_role->proto_role_cap.stbc_ht_rx;
		if (rx_stbc_nss > 3)
			rx_stbc_nss = 3;
		SET_HT_CAP_ELE_RX_STBC(&ht_capie, rx_stbc_nss);
		RTW_INFO("[HT] Declare supporting RX STBC = %d\n", rx_stbc_nss);
	}

	/* fill default supported_mcs_set */
	_rtw_memcpy(ht_capie.supp_mcs_set, pmlmeext->default_supported_mcs_set, 16);

	/* update default supported_mcs_set */
	rx_nss = get_phy_rx_nss(padapter);

	switch (rx_nss) {
	case 1:
		set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_1R);
		break;
	case 2:
		#ifdef CONFIG_DISABLE_MCS13TO15
		if (cbw40_enable && pregistrypriv->wifi_spec != 1)
			set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_2R_13TO15_OFF);
		else
		#endif
			set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_2R);
		break;
	case 3:
		set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_3R);
		break;
	case 4:
		set_mcs_rate_by_mask(ht_capie.supp_mcs_set, MCS_RATE_4R);
		break;
	default:
		RTW_WARN("rf_type:%d or rx_nss:%u is not expected\n",
			GET_HAL_RFPATH(adapter_to_dvobj(padapter)), rx_nss);
	}

	if (padapter->phl_role->proto_role_cap.max_amsdu_len > 0) {
		/*SET_HT_CAP_ELE_MAX_AMSDU_LENGTH(&ht_capie, 1);*/
		RTW_INFO("%s IEEE80211_HT_CAP_MAX_AMSDU is set\n", __func__);
		ht_capie.cap_info = ht_capie.cap_info | IEEE80211_HT_CAP_MAX_AMSDU;
	}

	if (padapter->driver_rx_ampdu_factor != 0xFF)
		max_rx_ampdu_factor = (HT_CAP_AMPDU_FACTOR)padapter->driver_rx_ampdu_factor;
	else
		rtw_hal_get_def_var(padapter, HW_VAR_MAX_RX_AMPDU_FACTOR, &max_rx_ampdu_factor);

	/* rtw_hal_get_def_var(padapter, HW_VAR_MAX_RX_AMPDU_FACTOR, &max_rx_ampdu_factor); */
	ht_capie.ampdu_params_info = (max_rx_ampdu_factor & 0x03);

	if (padapter->driver_rx_ampdu_spacing != 0xFF)
		ht_capie.ampdu_params_info |= ((padapter->driver_rx_ampdu_spacing & 0x07) << 2);
	else {
		if (padapter->securitypriv.dot11PrivacyAlgrthm == _AES_) {
			/*
			*	Todo : Each chip must to ask DD , this chip best ampdu_density setting
			*	By yiwei.sun
			*/
			rtw_hal_get_def_var(padapter, HW_VAR_BEST_AMPDU_DENSITY, &best_ampdu_density);

			ht_capie.ampdu_params_info |= (IEEE80211_HT_CAP_AMPDU_DENSITY & (best_ampdu_density << 2));

		} else
			ht_capie.ampdu_params_info |= (IEEE80211_HT_CAP_AMPDU_DENSITY & 0x00);
	}
#ifdef CONFIG_BEAMFORMING
	ht_capie.tx_BF_cap_info = 0;

	/* HT Beamformer*/
	if (TEST_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMER_ENABLE)) {
		/* Transmit NDP Capable */
		SET_HT_CAP_TXBF_TRANSMIT_NDP_CAP(&ht_capie, 1);
		/* Explicit Compressed Steering Capable */
		SET_HT_CAP_TXBF_EXPLICIT_COMP_STEERING_CAP(&ht_capie, 1);
		/* Compressed Steering Number Antennas */
		SET_HT_CAP_TXBF_COMP_STEERING_NUM_ANTENNAS(&ht_capie, 1);
		rtw_hal_get_def_var(padapter, HAL_DEF_BEAMFORMER_CAP, (u8 *)&rf_num);
		if (rf_num > 3)
			rf_num = 3;
		SET_HT_CAP_TXBF_CHNL_ESTIMATION_NUM_ANTENNAS(&ht_capie, rf_num);
	}

	/* HT Beamformee */
	if (TEST_FLAG(phtpriv->beamform_cap, BEAMFORMING_HT_BEAMFORMEE_ENABLE)) {
		/* Receive NDP Capable */
		SET_HT_CAP_TXBF_RECEIVE_NDP_CAP(&ht_capie, 1);
		/* Explicit Compressed Beamforming Feedback Capable */
		SET_HT_CAP_TXBF_EXPLICIT_COMP_FEEDBACK_CAP(&ht_capie, 2);

		rtw_hal_get_def_var(padapter, HAL_DEF_BEAMFORMEE_CAP, (u8 *)&rf_num);
		if (rf_num > 3)
			rf_num = 3;
#ifdef CONFIG_80211AC_VHT
		/* IOT action suggested by Yu Chen 2017/3/3 */
		if ((pmlmeinfo->assoc_AP_vendor == HT_IOT_PEER_BROADCOM) &&
			!pvhtpriv->ap_bf_cap.is_mu_bfer &&
			pvhtpriv->ap_bf_cap.su_sound_dim == 2)
			rf_num = (rf_num >= 2 ? 2 : rf_num);
#endif
		SET_HT_CAP_TXBF_COMP_STEERING_NUM_ANTENNAS(&ht_capie, rf_num);
	}
#endif/*CONFIG_BEAMFORMING*/

	pframe = rtw_set_ie(out_ie + out_len, _HT_CAPABILITY_IE_,
		sizeof(struct rtw_ieee80211_ht_cap), (unsigned char *)&ht_capie, pout_len);

	phtpriv->ht_option = _TRUE;

	if (in_ie != NULL) {
		p = rtw_get_ie(in_ie, _HT_ADD_INFO_IE_, &ielen, in_len);
		if (p && (ielen == sizeof(struct ieee80211_ht_addt_info))) {
			out_len = *pout_len;
			pframe = rtw_set_ie(out_ie + out_len, _HT_ADD_INFO_IE_, ielen, p + 2 , pout_len);
		}
	}

	return phtpriv->ht_option;

}

/* the fucntion is > passive_level (in critical_section) */
void rtw_update_ht_cap(_adapter *padapter, u8 *pie, uint ie_len, u8 channel)
{
	u8 *p, max_ampdu_sz;
	int len;
	/* struct sta_info *bmc_sta, *psta; */
	struct rtw_ieee80211_ht_cap *pht_capie;
	struct ieee80211_ht_addt_info *pht_addtinfo;
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
	struct registry_priv *pregistrypriv = &padapter->registrypriv;
	/* struct wlan_network *pcur_network = &(pmlmepriv->cur_network);; */
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u8 cbw40_enable = 0;


	if (!phtpriv->ht_option)
		return;

	if ((!pmlmeinfo->HT_info_enable) || (!pmlmeinfo->HT_caps_enable))
		return;

	RTW_INFO("+rtw_update_ht_cap()\n");

	/* maybe needs check if ap supports rx ampdu. */
	if ((phtpriv->ampdu_enable == _FALSE) && (pregistrypriv->ampdu_enable == 1)) {
		if (pregistrypriv->wifi_spec == 1) {
			/* remove this part because testbed AP should disable RX AMPDU */
			/* phtpriv->ampdu_enable = _FALSE; */
			phtpriv->ampdu_enable = _TRUE;
		} else
			phtpriv->ampdu_enable = _TRUE;
	} 


	/* check Max Rx A-MPDU Size */
	len = 0;
	p = rtw_get_ie(pie + sizeof(NDIS_802_11_FIXED_IEs), _HT_CAPABILITY_IE_, &len, ie_len - sizeof(NDIS_802_11_FIXED_IEs));
	if (p && len > 0) {
		pht_capie = (struct rtw_ieee80211_ht_cap *)(p + 2);
		max_ampdu_sz = (pht_capie->ampdu_params_info & IEEE80211_HT_CAP_AMPDU_FACTOR);
		max_ampdu_sz = 1 << (max_ampdu_sz + 3); /* max_ampdu_sz (kbytes); */

		/* RTW_INFO("rtw_update_ht_cap(): max_ampdu_sz=%d\n", max_ampdu_sz); */
		phtpriv->rx_ampdu_maxlen = max_ampdu_sz;

	}


	len = 0;
	p = rtw_get_ie(pie + sizeof(NDIS_802_11_FIXED_IEs), _HT_ADD_INFO_IE_, &len, ie_len - sizeof(NDIS_802_11_FIXED_IEs));
	if (p && len > 0) {
		pht_addtinfo = (struct ieee80211_ht_addt_info *)(p + 2);
		/* todo: */
	}

	if (rtw_hw_chk_bw_cap(adapter_to_dvobj(padapter), BW_CAP_40M)) {
		if (channel > 14) {
			if (REGSTY_IS_BW_5G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		} else {
			if (REGSTY_IS_BW_2G_SUPPORT(pregistrypriv, CHANNEL_WIDTH_40))
				cbw40_enable = 1;
		}
	}

	/* update cur_bwmode & cur_ch_offset */
	if ((cbw40_enable) &&
	    (pmlmeinfo->HT_caps.u.HT_cap_element.HT_caps_info & BIT(1)) &&
	    (pmlmeinfo->HT_info.infos[0] & BIT(2))) {
		int i;
		u8 rx_nss = 0;

		rx_nss = get_phy_rx_nss(padapter);


		/* update the MCS set */
		for (i = 0; i < 16; i++)
			pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate[i] &= pmlmeext->default_supported_mcs_set[i];

		/* update the MCS rates */
		switch (rx_nss) {
		case 1:
			set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_1R);
			break;
		case 2:
			#ifdef CONFIG_DISABLE_MCS13TO15
			if (pmlmeext->chandef.bw == CHANNEL_WIDTH_40 && pregistrypriv->wifi_spec != 1)
				set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_2R_13TO15_OFF);
			else
			#endif
				set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_2R);
			break;
		case 3:
			set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_3R);
			break;
		case 4:
			set_mcs_rate_by_mask(pmlmeinfo->HT_caps.u.HT_cap_element.MCS_rate, MCS_RATE_4R);
			break;
		default:
			RTW_WARN("rx_nss:%u is not expected\n", rx_nss);
		}

		/* switch to the 40M Hz mode accoring to the AP */
		/* pmlmeext->cur_bwmode = CHANNEL_WIDTH_40; */
		/* Secondary Channel Offset */
		switch ((pmlmeinfo->HT_info.infos[0] & 0x3)) {
		case IEEE80211_SCA:
			pmlmeext->chandef.offset = CHAN_OFFSET_UPPER;
			break;

		case IEEE80211_SCB:
			pmlmeext->chandef.offset = CHAN_OFFSET_LOWER;
			break;

		default:
			pmlmeext->chandef.bw = CHANNEL_WIDTH_20;
			pmlmeext->chandef.offset = CHAN_OFFSET_NO_EXT;
			RTW_INFO("%s : ch offset is not assigned for HT40 mod , update cur_bwmode=%u, cur_ch_offset=%u\n", 
					__func__, pmlmeext->chandef.bw, pmlmeext->chandef.offset);
			break;
		}
	}

	/*  */
	/* Config SM Power Save setting */
	/*  */
	pmlmeinfo->SM_PS = (pmlmeinfo->HT_caps.u.HT_cap_element.HT_caps_info & 0x0C) >> 2;
	if (pmlmeinfo->SM_PS == WLAN_HT_CAP_SM_PS_STATIC) {
#if 0
		u8 i;
		/* update the MCS rates */
		for (i = 0; i < 16; i++)
			pmlmeinfo->HT_caps.HT_cap_element.MCS_rate[i] &= MCS_rate_1R[i];
#endif
		RTW_INFO("%s(): WLAN_HT_CAP_SM_PS_STATIC\n", __FUNCTION__);
	}

	/*  */
	/* Config current HT Protection mode. */
	/*  */
	pmlmeinfo->HT_protection = pmlmeinfo->HT_info.infos[1] & 0x3;
}
#endif

#ifdef CONFIG_TDLS
void rtw_issue_addbareq_cmd_tdls(_adapter *padapter, struct xmit_frame *pxmitframe)
{
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	struct sta_info *ptdls_sta = NULL;
	u8 issued;
	int priority;
	struct ht_priv	*phtpriv;

	priority = pattrib->priority;

	if (pattrib->direct_link == _TRUE) {
		ptdls_sta = rtw_get_stainfo(&padapter->stapriv, pattrib->dst);
		if ((ptdls_sta != NULL) && (ptdls_sta->tdls_sta_state & TDLS_LINKED_STATE)) {
			phtpriv = &ptdls_sta->htpriv;

			if ((phtpriv->ht_option == _TRUE) && (phtpriv->ampdu_enable == _TRUE)) {
				issued = (phtpriv->agg_enable_bitmap >> priority) & 0x1;
				issued |= (phtpriv->candidate_tid_bitmap >> priority) & 0x1;

				if (0 == issued) {
					RTW_INFO("[%s], p=%d\n", __FUNCTION__, priority);
					ptdls_sta->htpriv.candidate_tid_bitmap |= BIT((u8)priority);
					rtw_addbareq_cmd(padapter, (u8)priority, pattrib->dst);
				}
			}
		}
	}
}
#endif /* CONFIG_TDLS */

#ifdef CONFIG_80211N_HT
static u8 rtw_issue_addbareq_check(_adapter *padapter, struct xmit_frame *pxmitframe, u8 issue_when_busy)
{
	struct registry_priv *pregpriv = &padapter->registrypriv;
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct registry_priv *pregistry = &padapter->registrypriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;
	s32 bmcst = IS_MCAST(pattrib->ra);

	if (bmcst)
		return _FALSE;

	if (pregpriv->wifi_spec == 1)
		return _TRUE;

	if (pregistry->tx_quick_addba_req == 0) {
		if ((issue_when_busy == _TRUE) && (pmlmepriv->LinkDetectInfo.bBusyTraffic == _FALSE))
			return _FALSE;

		if (pmlmepriv->LinkDetectInfo.NumTxOkInPeriod < 100)
			return _FALSE;
	}

	return _TRUE;
}

void rtw_issue_addbareq_cmd(_adapter *padapter, struct xmit_frame *pxmitframe, u8 issue_when_busy)
{
	u8 issued;
	int priority;
	struct sta_info *psta = NULL;
	struct ht_priv	*phtpriv;
	struct pkt_attrib *pattrib = &pxmitframe->attrib;

	if (rtw_issue_addbareq_check(padapter,pxmitframe, issue_when_busy) == _FALSE)
		return;

	priority = pattrib->priority;

#ifdef CONFIG_TDLS
	rtw_issue_addbareq_cmd_tdls(padapter, pxmitframe);
#endif /* CONFIG_TDLS */

	psta = rtw_get_stainfo(&padapter->stapriv, pattrib->ra);
	if (pattrib->psta != psta) {
		RTW_INFO("%s, pattrib->psta(%p) != psta(%p)\n", __func__, pattrib->psta, psta);
		return;
	}

	if (psta == NULL) {
		RTW_INFO("%s, psta==NUL\n", __func__);
		return;
	}

	if (!(psta->state & WIFI_ASOC_STATE)) {
		RTW_INFO("%s, psta->state(0x%x) != WIFI_ASOC_STATE\n", __func__, psta->state);
		return;
	}


	phtpriv = &psta->htpriv;

	if ((phtpriv->ht_option == _TRUE) && (phtpriv->ampdu_enable == _TRUE)) {
		issued = (phtpriv->agg_enable_bitmap >> priority) & 0x1;
		issued |= (phtpriv->candidate_tid_bitmap >> priority) & 0x1;

		if (0 == issued) {
			RTW_INFO("rtw_issue_addbareq_cmd, p=%d\n", priority);
			psta->htpriv.candidate_tid_bitmap |= BIT((u8)priority);
			rtw_addbareq_cmd(padapter, (u8) priority, pattrib->ra);
		}
	}

}
#endif /* CONFIG_80211N_HT */
void rtw_append_extended_cap(_adapter *padapter, u8 *out_ie, uint *pout_len)
{
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct ht_priv		*phtpriv = &pmlmepriv->htpriv;
#ifdef CONFIG_80211AC_VHT
	struct vht_priv	*pvhtpriv = &pmlmepriv->vhtpriv;
#endif /* CONFIG_80211AC_VHT */
	u8 *ext_cap_data = pmlmepriv->ext_capab_ie_data;
	u8 *ext_cap_data_len = &(pmlmepriv->ext_capab_ie_len);

	if (phtpriv->bss_coexist)
		rtw_add_ext_cap_info(ext_cap_data, ext_cap_data_len, BSS_COEXT);

#ifdef CONFIG_80211AC_VHT
	if (pvhtpriv->vht_option)
		rtw_add_ext_cap_info(ext_cap_data, ext_cap_data_len, OP_MODE_NOTIFICATION);
#endif /* CONFIG_80211AC_VHT */
#ifdef CONFIG_RTW_WNM
	rtw_add_ext_cap_info(ext_cap_data, ext_cap_data_len, BSS_TRANSITION);
#endif

#ifdef CONFIG_RTW_MBO
	rtw_add_ext_cap_info(ext_cap_data, ext_cap_data_len, INTERWORKING);
#endif

#ifdef CONFIG_80211AX_HE
	/* CONFIG_80211AX_HE_TODO */
#endif /* CONFIG_80211AX_HE */

#ifdef CONFIG_STA_MULTIPLE_BSSID
	rtw_add_ext_cap_info(ext_cap_data, ext_cap_data_len, MULTI_BSSID);
#endif

	/*
		From 802.11 specification,if a STA does not support any of capabilities defined
		in the Extended Capabilities element, then the STA is not required to
		transmit the Extended Capabilities element.
	*/
	rtw_update_ext_cap_ie(ext_cap_data, *ext_cap_data_len, out_ie, pout_len, _BEACON_IE_OFFSET_);
}
#endif

#ifdef CONFIG_LAYER2_ROAMING
inline void rtw_set_to_roam(_adapter *adapter, u8 to_roam)
{
	if (to_roam == 0)
		adapter->mlmepriv.to_join = _FALSE;
	adapter->mlmepriv.to_roam = to_roam;
}

inline u8 rtw_dec_to_roam(_adapter *adapter)
{
	adapter->mlmepriv.to_roam--;
	return adapter->mlmepriv.to_roam;
}

inline u8 rtw_to_roam(_adapter *adapter)
{
	return adapter->mlmepriv.to_roam;
}

void rtw_roaming(_adapter *padapter, struct wlan_network *tgt_network)
{
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;

	_rtw_spinlock_bh(&pmlmepriv->lock);
	_rtw_roaming(padapter, tgt_network);
	_rtw_spinunlock_bh(&pmlmepriv->lock);
}
void _rtw_roaming(_adapter *padapter, struct wlan_network *tgt_network)
{
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct wlan_network *cur_network = &pmlmepriv->cur_network;
	struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info *pmlmeinfo = &pmlmeext->mlmext_info;
	int do_join_r;

	if (0 < rtw_to_roam(padapter)) {
		RTW_INFO("roaming from %s("MAC_FMT"), length:%d\n",
			cur_network->network.Ssid.Ssid, MAC_ARG(cur_network->network.MacAddress),
			 cur_network->network.Ssid.SsidLength);
		_rtw_memcpy(&pmlmepriv->assoc_ssid, &cur_network->network.Ssid, sizeof(NDIS_802_11_SSID));
		pmlmepriv->assoc_ch = 0;
		pmlmepriv->assoc_by_bssid = _FALSE;

#ifdef CONFIG_WAPI_SUPPORT
		rtw_wapi_return_all_sta_info(padapter);
#endif

		while (1) {
			do_join_r = rtw_do_join(padapter);
			if (_SUCCESS == do_join_r)
				break;
			else {
				RTW_INFO("roaming do_join return %d\n", do_join_r);
				rtw_dec_to_roam(padapter);

				if (rtw_to_roam(padapter) > 0)
					continue;
				else {
					RTW_INFO("%s(%d) -to roaming fail, indicate_disconnect\n", __FUNCTION__, __LINE__);
#ifdef CONFIG_RTW_80211R
					/* rtw_ft_clr_flags(padapter, RTW_FT_PEER_EN|RTW_FT_PEER_OTD_EN); */
					rtw_ft_reset_status(padapter);
#endif
					rtw_indicate_disconnect(padapter, 0, _FALSE);
					pmlmeinfo->disconnect_occurred_time = rtw_systime_to_ms(rtw_get_current_time());
					pmlmeinfo->disconnect_code = DISCONNECTION_BY_DRIVER_DUE_TO_LAYER2_ROAMING_TERMINATE;
					pmlmeinfo->wifi_reason_code = WLAN_REASON_UNSPECIFIED;
					break;
				}
			}
		}
	}

}
#endif /* CONFIG_LAYER2_ROAMING */

bool rtw_adjust_chbw(_adapter *adapter, u8 req_ch, u8 *req_bw, u8 *req_offset)
{
	struct registry_priv *regsty = adapter_to_regsty(adapter);
	u8 allowed_bw;

	if (req_ch < 14)
		allowed_bw = REGSTY_BW_2G(regsty);
	else if (req_ch == 14)
		allowed_bw = CHANNEL_WIDTH_20;
	else
		allowed_bw = REGSTY_BW_5G(regsty);

	allowed_bw = rtw_hw_largest_bw(adapter_to_dvobj(adapter), allowed_bw);

	if (allowed_bw == CHANNEL_WIDTH_80 && *req_bw > CHANNEL_WIDTH_80)
		*req_bw = CHANNEL_WIDTH_80;
	else if (allowed_bw == CHANNEL_WIDTH_40 && *req_bw > CHANNEL_WIDTH_40)
		*req_bw = CHANNEL_WIDTH_40;
	else if (allowed_bw == CHANNEL_WIDTH_20 && *req_bw > CHANNEL_WIDTH_20) {
		*req_bw = CHANNEL_WIDTH_20;
		*req_offset = CHAN_OFFSET_NO_EXT;
	} else
		return _FALSE;

	return _TRUE;
}

sint rtw_linked_check(_adapter *padapter)
{
	if (MLME_IS_AP(padapter) || MLME_IS_MESH(padapter)
		|| MLME_IS_ADHOC(padapter) || MLME_IS_ADHOC_MASTER(padapter)
	) {
		if (padapter->stapriv.asoc_sta_count > 1)
			return _TRUE;
	} else {
		/* Station mode */
		if (check_fwstate(&padapter->mlmepriv, WIFI_ASOC_STATE) == _TRUE)
			return _TRUE;
	}
	return _FALSE;
}
/*#define DBG_ADAPTER_STATE_CHK*/
u8 rtw_is_adapter_up(_adapter *padapter)
{
	struct dvobj_priv *dvobj;

	if (padapter == NULL)
		return _FALSE;

	dvobj = adapter_to_dvobj(padapter);

	if (RTW_CANNOT_RUN(dvobj)) {
		#ifdef DBG_ADAPTER_STATE_CHK
		RTW_INFO(FUNC_ADPT_FMT " FALSE -bDriverStopped(%s) bSurpriseRemoved(%s)\n"
			, FUNC_ADPT_ARG(padapter)
			, dev_is_drv_stopped(dvobj) ? "True" : "False"
			, dev_is_surprise_removed(dvobj) ? "True" : "False");
		#endif
		return _FALSE;
	}

	if (padapter->netif_up == _FALSE) {
		#ifdef DBG_ADAPTER_STATE_CHK
		RTW_INFO(FUNC_ADPT_FMT " FALSE -(netif_up == _FALSE)\n", FUNC_ADPT_ARG(padapter));
		#endif
		return _FALSE;
	}

	if (padapter->phl_role == NULL) {
		#ifdef DBG_ADAPTER_STATE_CHK
		RTW_INFO(FUNC_ADPT_FMT " FALSE -(phl_role == NULL)\n", FUNC_ADPT_ARG(padapter));
		#endif
		return _FALSE;
	}

	return _TRUE;
}

bool is_miracast_enabled(_adapter *adapter)
{
	bool enabled = 0;
#ifdef CONFIG_WFD
	struct wifi_display_info *wfdinfo = &adapter->wfd_info;

	enabled = (wfdinfo->stack_wfd_mode & (MIRACAST_SOURCE | MIRACAST_SINK))
		  || (wfdinfo->op_wfd_mode & (MIRACAST_SOURCE | MIRACAST_SINK));
#endif

	return enabled;
}

bool rtw_chk_miracast_mode(_adapter *adapter, u8 mode)
{
	bool ret = 0;
#ifdef CONFIG_WFD
	struct wifi_display_info *wfdinfo = &adapter->wfd_info;

	ret = (wfdinfo->stack_wfd_mode & mode) || (wfdinfo->op_wfd_mode & mode);
#endif

	return ret;
}

const char *get_miracast_mode_str(int mode)
{
	if (mode == MIRACAST_SOURCE)
		return "SOURCE";
	else if (mode == MIRACAST_SINK)
		return "SINK";
	else if (mode == (MIRACAST_SOURCE | MIRACAST_SINK))
		return "SOURCE&SINK";
	else if (mode == MIRACAST_DISABLED)
		return "DISABLED";
	else
		return "INVALID";
}

#ifdef CONFIG_WFD
static bool wfd_st_match_rule(_adapter *adapter, u8 *local_naddr, u8 *local_port, u8 *remote_naddr, u8 *remote_port)
{
	struct wifi_display_info *wfdinfo = &adapter->wfd_info;

	if (ntohs(*((u16 *)local_port)) == wfdinfo->rtsp_ctrlport
	    || ntohs(*((u16 *)local_port)) == wfdinfo->tdls_rtsp_ctrlport
	    || ntohs(*((u16 *)remote_port)) == wfdinfo->peer_rtsp_ctrlport)
		return _TRUE;
	return _FALSE;
}

static struct st_register wfd_st_reg = {
	.s_proto = 0x06,
	.rule = wfd_st_match_rule,
};
#endif /* CONFIG_WFD */

inline void rtw_wfd_st_switch(struct sta_info *sta, bool on)
{
#ifdef CONFIG_WFD
	if (on)
		rtw_st_ctl_register(&sta->st_ctl, SESSION_TRACKER_REG_ID_WFD, &wfd_st_reg);
	else
		rtw_st_ctl_unregister(&sta->st_ctl, SESSION_TRACKER_REG_ID_WFD);
#endif
}

void dump_arp_pkt(void *sel, u8 *da, u8 *sa, u8 *arp, bool tx)
{
	RTW_PRINT_SEL(sel, "%s ARP da="MAC_FMT", sa="MAC_FMT"\n"
		, tx ? "send" : "recv", MAC_ARG(da), MAC_ARG(sa));
	RTW_PRINT_SEL(sel, "htype=%u, ptype=0x%04x, hlen=%u, plen=%u, oper=%u\n"
		, GET_ARP_HTYPE(arp), GET_ARP_PTYPE(arp), GET_ARP_HLEN(arp)
		, GET_ARP_PLEN(arp), GET_ARP_OPER(arp));
	RTW_PRINT_SEL(sel, "sha="MAC_FMT", spa="IP_FMT"\n"
		, MAC_ARG(ARP_SENDER_MAC_ADDR(arp)), IP_ARG(ARP_SENDER_IP_ADDR(arp)));
	RTW_PRINT_SEL(sel, "tha="MAC_FMT", tpa="IP_FMT"\n"
		, MAC_ARG(ARP_TARGET_MAC_ADDR(arp)), IP_ARG(ARP_TARGET_IP_ADDR(arp)));
}

#ifdef CONFIG_STA_CMD_DISPR
/* software setting top half for connect abort */
static void _connect_abort_sw_top_half(struct _ADAPTER *a)
{
	cancel_link_timer(&a->mlmeextpriv);
	cancel_assoc_timer(&a->mlmepriv);
	a->mlmeextpriv.join_abort = 1;
}

/* software setting bottom half for connect abort */
static void _connect_abort_sw_bottom_half(struct _ADAPTER *a)
{
	/* ref: rtw_joinbss_event_prehandle(), join_res == -4 */
	_clr_fwstate_(&a->mlmepriv, WIFI_UNDER_LINKING);
	rtw_reset_securitypriv(a);
	a->mlmeextpriv.join_abort = 0;
}

/*
 * _connect_disconncet_hw - Handle hardware part of connect abort and fail
 * @a:		struct _ADAPTER *
 *
 * Handle hardware part of connect fail.
 * Most implement is reference from bottom half of rtw_joinbss_event_callback()
 * with join_res < 0.
 *
 * Reference functions:
 *	1. rtw_joinbss_event_callback()
 *	2. rtw_set_hw_after_join(a, -1)
 *	3. rtw_hw_connect_abort()
 */
static void _connect_disconnect_hw(struct _ADAPTER *a)
{
	struct dvobj_priv *d;
	void *phl;
	u8 *mac;
	struct sta_info *sta;


	d = adapter_to_dvobj(a);
	phl = GET_PHL_INFO(d);
	/* ref: rtw_set_hw_after_join(a, -1) */
	mac = (u8*)a->mlmeextpriv.mlmext_info.network.MacAddress;
	sta = rtw_get_stainfo(&a->stapriv, mac);
	if (!sta) {
		RTW_ERR(FUNC_ADPT_FMT ": stainfo(" MAC_FMT ") not exist!\n",
			FUNC_ADPT_ARG(a), MAC_ARG(mac));
		return;
	}

	/* bottom half of rtw_hw_connect_abort() - start */
	rtw_phl_chanctx_del(phl, a->phl_role, NULL);
	/* restore original union ch */
	rtw_join_done_chk_ch(a, -1);
	/* free connecting AP sta info */
	rtw_free_stainfo(a, sta);
	rtw_init_self_stainfo(a);
	/* bottom half of rtw_hw_connect_abort() - end */

	/* bottom half of rtw_joinbss_event_callback() */
	rtw_mi_os_xmit_schedule(a);
}

static void _connect_abort_notify_cb(void *priv, struct phl_msg *msg)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;


	RTW_DBG(FUNC_ADPT_FMT ": connect_st=%u\n",
		FUNC_ADPT_ARG(a), a->connect_state);

	_connect_disconnect_hw(a);
	_connect_abort_sw_bottom_half(a);

	_rtw_spinlock(&a->connect_st_lock);
	if ((a->connect_state != CONNECT_ST_ACQUIRED) || !a->connect_abort) {
		RTW_ERR(FUNC_ADPT_FMT ": connect_st=%u, abort is %s !\n",
			FUNC_ADPT_ARG(a), a->connect_state,
			a->connect_abort?"true":"false");
	} else {
		a->connect_state = CONNECT_ST_IDLE;
		a->connect_abort = false;
	}
	_rtw_spinunlock(&a->connect_st_lock);
}

static enum rtw_phl_status _connect_abort_notify(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	enum rtw_phl_status status;


	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_FG_MDL_CONNECT);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_DISCONNECT);
	msg.band_idx = a->phl_role->hw_band;
	msg.rsvd[0] = (u8*)a->phl_role;

	attr.opt = MSG_OPT_SEND_IN_ABORT;
	attr.completion.completion = _connect_abort_notify_cb;
	attr.completion.priv = a;

	status = rtw_phl_send_msg_to_dispr(GET_PHL_INFO(d), &msg, &attr, NULL);
	if (status != RTW_PHL_STATUS_SUCCESS)
		RTW_ERR(FUNC_ADPT_FMT ": send_msg_to_dispr fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);

	return status;
}

static void _connect_swch_done_notify_cb(void *priv, struct phl_msg *msg)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;


	RTW_DBG(FUNC_ADPT_FMT ": connect_st=%u\n",
		FUNC_ADPT_ARG(a), a->connect_state);

	if (msg->inbuf) {
		rtw_vmfree(msg->inbuf, msg->inlen);
		msg->inbuf = NULL;
	}
}

static enum rtw_phl_status _connect_swch_done_notify(struct _ADAPTER *a,
						struct rtw_chan_def *chandef)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	u8 *info = NULL;
	enum rtw_phl_status status;


	info = rtw_vmalloc(sizeof(struct rtw_chan_def));
	if (!info) {
		RTW_ERR(FUNC_ADPT_FMT ": Allocate msg hub buffer fail!\n",
			FUNC_ADPT_ARG(a));
		return RTW_PHL_STATUS_RESOURCE;
	}
	_rtw_memcpy(info, chandef, sizeof(struct rtw_chan_def));

	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_FG_MDL_CONNECT);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_SWCH_DONE);
	msg.band_idx = a->phl_role->hw_band;
	msg.inbuf = info;
	msg.inlen = sizeof(struct rtw_chan_def);

	attr.completion.completion = _connect_swch_done_notify_cb;
	attr.completion.priv = a;

	status = rtw_phl_send_msg_to_dispr(GET_PHL_INFO(d),
					       &msg, &attr, NULL);
	if (status != RTW_PHL_STATUS_SUCCESS) {
		rtw_vmfree(info, sizeof(struct rtw_chan_def));
		RTW_ERR(FUNC_ADPT_FMT ": send_msg_to_dispr fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);
	}

	return status;
}

static void _connect_cmd_done(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	enum rtw_phl_status status;


	RTW_DBG(FUNC_ADPT_FMT ": +\n", FUNC_ADPT_ARG(a));

	if (!a->connect_token){
		RTW_ERR(FUNC_ADPT_FMT ": token is NULL!\n", FUNC_ADPT_ARG(a));
		return;
	}

	_rtw_spinlock(&a->connect_st_lock);
	status = rtw_phl_free_cmd_token(GET_PHL_INFO(d),
					role->hw_band, &a->connect_token);
	a->connect_token = 0;
	if (status != RTW_PHL_STATUS_SUCCESS)
		RTW_ERR(FUNC_ADPT_FMT ": free_cmd_token fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);
	a->connect_state = CONNECT_ST_IDLE;
	a->connect_abort = false;
	_rtw_spinunlock(&a->connect_st_lock);

	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));
}

static enum phl_mdl_ret_code _connect_acquired(void* dispr, void *priv)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct _WLAN_BSSID_EX *network = &a->mlmeextpriv.mlmext_info.network;


	RTW_DBG(FUNC_ADPT_FMT ": +\n", FUNC_ADPT_ARG(a));

	_rtw_spinlock(&a->connect_st_lock);
	if (a->connect_state != CONNECT_ST_REQUESTING)
		RTW_ERR(FUNC_ADPT_FMT ": connect_st=%u, not requesting?!\n",
			FUNC_ADPT_ARG(a), a->connect_state);
	a->connect_state = CONNECT_ST_ACQUIRED;
	_rtw_spinunlock(&a->connect_st_lock);

	/*rtw_hw_prepare_connect(a, NULL, network->MacAddress);*/
	rtw_phl_connect_prepare(GET_PHL_INFO(d), a->phl_role,
				network->MacAddress);

	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));

	return MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code _connect_abort(void* dispr, void *priv)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;
	bool inner_abort = false;
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;


	RTW_DBG(FUNC_ADPT_FMT ": +\n", FUNC_ADPT_ARG(a));

	_rtw_spinlock(&a->connect_st_lock);
	RTW_INFO(FUNC_ADPT_FMT ": connect_st=%u, abort is %s\n",
		 FUNC_ADPT_ARG(a), a->connect_state,
		 a->connect_abort?"true":"false");
	if (a->connect_state == CONNECT_ST_IDLE) {
		_rtw_spinunlock(&a->connect_st_lock);
		return MDL_RET_SUCCESS;
	}
	if (!a->connect_abort) {
		RTW_INFO(FUNC_ADPT_FMT ": framework asking abort!\n",
			 FUNC_ADPT_ARG(a));
		a->connect_abort = true;
		inner_abort = true;
	}
	_rtw_spinunlock(&a->connect_st_lock);

	_connect_abort_sw_top_half(a);
	if (inner_abort) {
		/* ref: rtw_join_timeout_handler() */
		_rtw_spinlock_bh(&a->mlmepriv.lock);
		a->mlmepriv.join_status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		rtw_indicate_disconnect(a, a->mlmepriv.join_status, _FALSE);
#ifdef CONFIG_IOCTL_CFG80211
		rtw_cfg80211_indicate_disconnect(a, a->mlmepriv.join_status, _FALSE);
#endif /* CONFIG_IOCTL_CFG80211 */
		a->mlmepriv.join_status = 0;
		_rtw_spinunlock_bh(&a->mlmepriv.lock);
	}

	if (a->connect_state == CONNECT_ST_ACQUIRED)
		phl_status = _connect_abort_notify(a);
	a->connect_token = 0; /* framework will free this token later */
	if (phl_status != RTW_PHL_STATUS_SUCCESS) {
		/* No callback function, everything should be done here */
		_connect_abort_sw_bottom_half(a);
		_rtw_spinlock(&a->connect_st_lock);
		a->connect_state = CONNECT_ST_IDLE;
		a->connect_abort = false;
		_rtw_spinunlock(&a->connect_st_lock);
	}

	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));

	return MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code _connect_msg_hdlr(void* dispr, void* priv,
					       struct phl_msg* msg)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = NULL;
	struct _WLAN_BSSID_EX *network = &a->mlmeextpriv.mlmext_info.network;
	struct sta_info *sta = NULL;
	u8 u_ch;
	enum channel_width u_bw;
	enum chan_offset u_offset;
	struct phl_msg nextmsg = {0};
	struct phl_msg_attribute attr = {0};
	enum rtw_phl_status status;
	enum phl_mdl_ret_code mdl_err;
	int err;
	u32 res;


	RTW_INFO(FUNC_ADPT_FMT ": + msg_id=0x%08x\n",
		FUNC_ADPT_ARG(a), msg->msg_id);

	if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_CONNECT) {
		RTW_INFO(FUNC_ADPT_FMT ": Message is not from connect module, "
			 "skip msg_id=0x%08x\n", FUNC_ADPT_ARG(a), msg->msg_id);
		RTW_INFO(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));
		return MDL_RET_IGNORE;
	}

	if (IS_MSG_FAIL(msg->msg_id)) {
		RTW_WARN(FUNC_ADPT_FMT ": cmd dispatcher notify cmd failure on "
			 "msg_id=0x%08x\n", FUNC_ADPT_ARG(a), msg->msg_id);
		if (MSG_EVT_ID_FIELD(msg->msg_id) != MSG_EVT_DISCONNECT)
			goto send_disconnect;
	}

	role = a->phl_role;
	SET_MSG_MDL_ID_FIELD(nextmsg.msg_id, PHL_FG_MDL_CONNECT);
	nextmsg.band_idx = role->hw_band;

	switch (MSG_EVT_ID_FIELD(msg->msg_id)) {
	case MSG_EVT_CONNECT_START:
		RTW_INFO(FUNC_ADPT_FMT ": MSG_EVT_CONNECT_START\n",
			FUNC_ADPT_ARG(a));

		/* ref: top half of rtw_join_cmd_hdl() */

		sta = rtw_get_stainfo(&a->stapriv, a->phl_role->mac_addr);
		rtw_free_stainfo(a, sta);
		sta = rtw_alloc_stainfo(&a->stapriv, network->MacAddress);
		if (sta == NULL) {
			RTW_ERR(FUNC_ADPT_FMT ": alloc sta " MAC_FMT " fail!\n",
				FUNC_ADPT_ARG(a), MAC_ARG(network->MacAddress));
			rtw_init_self_stainfo(a);
			goto send_disconnect;
		}

		/* check channel, bandwidth, offset and switch */
		u_ch = a->mlmeextpriv.chandef.chan;
		u_bw = a->mlmeextpriv.chandef.bw;
		u_offset = a->mlmeextpriv.chandef.offset;
		if (!rtw_phl_chanctx_add(GET_PHL_INFO(d), role,
					 &u_ch, &u_bw, &u_offset))
			goto send_disconnect;

		rtw_hw_update_chan_def(a);
		rtw_mi_update_union_chan_inf(a, u_ch, (u8)u_offset, (u8)u_bw);

#ifdef CONFIG_ANTENNA_DIVERSITY
		rtw_antenna_select_cmd(a, network->PhyInfo.Optimum_antenna, _FALSE);
#endif

		#ifdef CONFIG_80211D
		rtw_joinbss_update_regulatory(a, network);
		#endif

		SET_MSG_EVT_ID_FIELD(nextmsg.msg_id, MSG_EVT_SWCH_START);
		status = rtw_phl_send_msg_to_dispr(GET_PHL_INFO(d),
						   &nextmsg, &attr, NULL);
		break;

	case MSG_EVT_SWCH_START:
		RTW_INFO(FUNC_ADPT_FMT ": MSG_EVT_SWCH_START\n",
			FUNC_ADPT_ARG(a));
		if (!role) {
			RTW_ERR(FUNC_ADPT_FMT ": role == NULL\n",
				FUNC_ADPT_ARG(a));
			break;
		}

		/* ref: bottom half of rtw_join_cmd_hdl() */
		RTW_INFO(FUNC_ADPT_FMT ": Switch to channel before link: "
			"chan(%d), bw(%d), offset(%d)\n",
			 FUNC_ADPT_ARG(a),
			 d->iface_state.union_ch, d->iface_state.union_bw,
			 d->iface_state.union_offset);
		set_channel_bwmode(a, d->iface_state.union_ch,
				   d->iface_state.union_offset,
				   d->iface_state.union_bw, _TRUE);

		status = _connect_swch_done_notify(a, &role->chandef);
		break;

	case MSG_EVT_SWCH_DONE:
		RTW_INFO(FUNC_ADPT_FMT ": MSG_EVT_SWCH_DONE\n",
			FUNC_ADPT_ARG(a));

		/* ref: last part of rtw_join_cmd_hdl() */
		cancel_link_timer(&a->mlmeextpriv);
		start_clnt_join(a);

		break;

	case MSG_EVT_CONNECT_LINKED:
		RTW_INFO(FUNC_ADPT_FMT ": MSG_EVT_CONNECT_LINKED\n",
			FUNC_ADPT_ARG(a));

		/* ref: top half of rtw_joinbss_event_callback() */
		err = rtw_set_hw_after_join(a, 0);
		if (err) {
			RTW_ERR(FUNC_ADPT_FMT ": set hardware fail(%d) during "
				"connecting!\n",
				FUNC_ADPT_ARG(a), err);
			goto send_disconnect;
		}

		break;

	case MSG_EVT_CONNECT_END:
		RTW_INFO(FUNC_ADPT_FMT ": MSG_EVT_CONNECT_END\n",
			FUNC_ADPT_ARG(a));

		/* ref: bottom half of rtw_set_hw_after_join() */
		sta = rtw_get_stainfo(&a->stapriv, network->MacAddress);
		if (sta)
			rtw_xmit_queue_clear(sta);
		else
			RTW_ERR(FUNC_ADPT_FMT ": stainfo(" MAC_FMT ") not exist!\n",
				FUNC_ADPT_ARG(a), MAC_ARG(network->MacAddress));

		/* ref: bottom half of rtw_joinbss_event_callback() */
		rtw_mi_os_xmit_schedule(a);

		_connect_cmd_done(a);
#ifdef CONFIG_LAYER2_ROAMING
		if (a->securitypriv.dot11PrivacyAlgrthm == _NO_PRIVACY_)
			dequeuq_roam_pkt(a);
#endif
		break;

	case MSG_EVT_DISCONNECT_PREPARE:
		RTW_WARN(FUNC_ADPT_FMT ": MSG_EVT_DISCONNECT_PREPARE\n",
			 FUNC_ADPT_ARG(a));

		/* STA connect fail case, top half */

		/* top half of rtw_joinbss_event_callback() */
		err = rtw_set_hw_after_join(a, -1);
		if (err) {
			RTW_ERR(FUNC_ADPT_FMT ": set hardware fail(%d) during "
				"connect abort!\n",
				FUNC_ADPT_ARG(a), err);
		}

		rtw_phl_disconnect(GET_PHL_INFO(d), a->phl_role, false);

		break;

	case MSG_EVT_DISCONNECT:
		RTW_WARN(FUNC_ADPT_FMT ": MSG_EVT_DISCONNECT\n",
			 FUNC_ADPT_ARG(a));

		/* STA connect fail case, bottom half */
		_connect_disconnect_hw(a);

		_connect_cmd_done(a);
		break;

	default:
		break;
	}

	goto exit;

send_disconnect:
	/*
	 * Trigger software handle and notify OS by rtw_joinbss_event_prehandle()
	 * Trigger hardware handle by sending MSG_EVT_DISCONNECT
	 */
	res = report_join_res(a, -4, WLAN_STATUS_UNSPECIFIED_FAILURE);
	if (res != _SUCCESS) {
		/*
		 * Fail to send MSG_EVT_DISCONNECT_PREPARE, do jobs in
		 * MSG_EVT_DISCONNECT_PREPARE and MSG_EVT_DISCONNECT directly
		 * here.
		 */

		/* ref: rtw_set_hw_after_join(a, -1) */
		a->mlmepriv.wpa_phase = _FALSE;
		sta = rtw_get_stainfo(&a->stapriv, network->MacAddress);
		if (sta) {
			/* rtw_hw_connect_abort(a, sta) */
			rtw_hw_del_all_key(a, sta, PHL_CMD_DIRECTLY, 0);
			status = rtw_phl_cmd_update_media_status(
						GET_PHL_INFO(d),
						sta->phl_sta, NULL, false,
						PHL_CMD_DIRECTLY, 0);
			if (status != RTW_PHL_STATUS_SUCCESS) {
				RTW_ERR(FUNC_ADPT_FMT ": update media status "
					"fail(0x%x)!\n",
					FUNC_ADPT_ARG(a), status);
			}
			_connect_disconnect_hw(a);
		} else {
			RTW_ERR(FUNC_ADPT_FMT ": stainfo(" MAC_FMT ") not exist!\n",
				FUNC_ADPT_ARG(a), MAC_ARG(network->MacAddress));
		}

		_connect_cmd_done(a);
	}

exit:
	RTW_INFO(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));
	return MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code _connect_set_info(void* dispr, void* priv,
					       struct phl_module_op_info* info)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;


	RTW_DBG(FUNC_ADPT_FMT ": +\n", FUNC_ADPT_ARG(a));
	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));

	return MDL_RET_IGNORE;
}

static enum phl_mdl_ret_code _connect_query_info(void* dispr, void* priv,
						struct phl_module_op_info* info)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;
	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;


	RTW_DBG(FUNC_ADPT_FMT ": +\n", FUNC_ADPT_ARG(a));

	switch (info->op_code) {
	case FG_REQ_OP_GET_ROLE:
		info->outbuf = (u8*)a->phl_role;
		ret = MDL_RET_SUCCESS;
		break;

#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
	case FG_REQ_OP_GET_ISSUE_NULL_OPS:
		{
		u8 (*issue_null)(void *, u8, bool) = scan_issu_null_data_cb;
		info->outbuf = (u8 *)issue_null;
		info->outlen = 0;
		ret = MDL_RET_SUCCESS;
		}
		break;
#endif

	default:
		break;
	}

	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));
	return ret;
}

enum rtw_phl_status rtw_connect_cmd(struct _ADAPTER *a,
				    struct _WLAN_BSSID_EX *network)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	struct rtw_chan_def *chdef;
	struct mi_state mstate = {0};
	struct phl_cmd_token_req *cmd_req;
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;


	RTW_DBG(FUNC_ADPT_FMT ": st=%u\n",
		FUNC_ADPT_ARG(a), a->connect_state);

	_rtw_spinlock(&a->connect_st_lock);

	if (a->connect_state != CONNECT_ST_IDLE) {
		status = RTW_PHL_STATUS_SUCCESS;
		RTW_WARN(FUNC_ADPT_FMT ": connect is on going...\n",
			 FUNC_ADPT_ARG(a));
		goto exit;
	}

	if ((network->IELength > MAX_IE_SZ) || (network->IELength < 2)) {
		status = RTW_PHL_STATUS_INVALID_PARAM;
		RTW_ERR(FUNC_ADPT_FMT ": invalid IE length(%u)\n",
			 FUNC_ADPT_ARG(a), network->IELength);
		goto exit;
	}

	/* ref: top half of rtw_join_cmd_hdl(), software only */
	/* Todo: disconnect before connecting */
	/*set_hw_before_join(a);*/

	/* Update HT/VHT/HE CAP and chan/bw/offset to a->mlmeextpriv.mlmext_info.network */
	update_join_info(a, network);

	/* check channel, bandwidth, offset and switch */
	chdef = &a->mlmeextpriv.chandef;
	if (!rtw_phl_chanctx_chk(GET_PHL_INFO(d), role,
				chdef->chan, chdef->bw, chdef->offset)) {
		/* ref: not group case in rtw_chk_start_clnt_join() */
		rtw_mi_status_no_self(a, &mstate);
		RTW_WARN(FUNC_ADPT_FMT ": channel group fail! ld_sta_num:%u, "
			 "ap_num:%u, mesh_num:%u\n",
			 FUNC_ADPT_ARG(a), MSTATE_STA_LD_NUM(&mstate),
			 MSTATE_AP_NUM(&mstate), MSTATE_MESH_NUM(&mstate));
		if ((MSTATE_STA_LD_NUM(&mstate) + MSTATE_AP_LD_NUM(&mstate)
		     + MSTATE_MESH_LD_NUM(&mstate)) >= 4) {
			status = RTW_PHL_STATUS_RESOURCE;
			goto exit;
		}
		rtw_mi_buddy_disconnect(a, DISCONNECTION_BY_DRIVER_DUE_TO_EACH_IFACE_CHBW_NOT_SYNC);
	}

	cmd_req = &a->connect_req;
	cmd_req->role = role;
	status = rtw_phl_add_cmd_token_req(GET_PHL_INFO(d),
					   role->hw_band,
					   cmd_req, &a->connect_token);
	if ((status != RTW_PHL_STATUS_SUCCESS)
	    && (status != RTW_PHL_STATUS_PENDING)) {
		RTW_ERR(FUNC_ADPT_FMT ": add_cmd_token_req fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);
		goto exit;
	}

	a->connect_state = CONNECT_ST_REQUESTING;
	status = RTW_PHL_STATUS_SUCCESS;

exit:
	_rtw_spinunlock(&a->connect_st_lock);

	RTW_DBG(FUNC_ADPT_FMT ": - st=%u ret=%u\n",
		FUNC_ADPT_ARG(a), a->connect_state, status);
	return status;
}

void rtw_connect_abort(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	enum rtw_phl_status status;


	RTW_WARN(FUNC_ADPT_FMT ": connect_st=%u, abort=%u\n",
		 FUNC_ADPT_ARG(a), a->connect_state, a->connect_abort);
	if (a->connect_state == CONNECT_ST_NOT_READY)
		return;

	_rtw_spinlock(&a->connect_st_lock);
	if ((a->connect_state == CONNECT_ST_IDLE) || a->connect_abort) {
		_rtw_spinunlock(&a->connect_st_lock);
		return;
	}
	a->mlmepriv.wpa_phase = _FALSE;
	a->connect_abort = true;
	_rtw_spinunlock(&a->connect_st_lock);

	status = rtw_phl_cancel_cmd_token(GET_PHL_INFO(d),
					  role->hw_band,
					  &a->connect_token);
	a->connect_token = 0;
	if (status != RTW_PHL_STATUS_SUCCESS) {
		RTW_ERR(FUNC_ADPT_FMT ": cancel_cmd_token fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);
		/* Cancel fail, and something needed to be handled by self */
		/* Release connect resource (software) */
		_connect_abort_sw_top_half(a);
		_connect_abort_sw_bottom_half(a);

		_rtw_spinlock(&a->connect_st_lock);
		a->connect_state = CONNECT_ST_IDLE;
		a->connect_abort = false;
		_rtw_spinunlock(&a->connect_st_lock);
	}
}

#define _CONNECT_ABORT_TO_		5000	/* unit: ms */
/**
 * rtw_connect_abort_wait() - Abort connect FG and wait to finish
 * @a		pointer of struct _ADAPTER
 *
 * Send cancel cmd to framework and wait connect FG to stop.
 *
 * Return 0 for connect FG already stopped, -1 for sending cancel command
 * fail, or -2 for waiting connect FG stop timeout.
 */
int rtw_connect_abort_wait(struct _ADAPTER *a)
{
	u32 timeout = _CONNECT_ABORT_TO_;
	systime start_t;
	u32 pass_t = 0;
	u32 msg_i;	/* message interval */
	u32 msg_t = 0;	/* next time point to print message */
	int err = 0;


	if (a->connect_state == CONNECT_ST_NOT_READY)
		return err;

	msg_i = timeout / 10;	/* at most print 10 msg in whole waiting time */
	if (!msg_i)
		msg_i = 1;

	_rtw_spinlock(&a->connect_st_lock);
	start_t = rtw_get_current_time();
	while (a->connect_state != CONNECT_ST_IDLE) {
		if (pass_t >= msg_t) {
			RTW_INFO(FUNC_ADPT_FMT ": connect st=%u, %u ms/%u pass...\n",
				 FUNC_ADPT_ARG(a), a->connect_state, pass_t, timeout);
			msg_t += msg_i;
		}
		_rtw_spinunlock(&a->connect_st_lock);

		pass_t = rtw_get_passing_time_ms(start_t);
		if (pass_t > timeout) {
			RTW_ERR(FUNC_ADPT_FMT ": Timeout, fail to abort connect!"
				" used:%u > max:%u ms\n",
				FUNC_ADPT_ARG(a), pass_t, timeout);
			err = -2; /* Timeout */
			_rtw_spinlock(&a->connect_st_lock);
			break;
		}
		rtw_connect_abort(a);
		rtw_msleep_os(1);

		_rtw_spinlock(&a->connect_st_lock);
	}
	_rtw_spinunlock(&a->connect_st_lock);

	return err;
}

void rtw_connect_req_free(struct _ADAPTER *a)
{
	if (a->connect_state == CONNECT_ST_NOT_READY)
		return;

	rtw_connect_abort_wait(a);

	_rtw_spinlock_free(&a->connect_st_lock);
	/* Terminate state, lock protection is not necessary */
	a->connect_state = CONNECT_ST_NOT_READY;
}

void rtw_connect_req_init(struct _ADAPTER *a)
{
	struct phl_cmd_token_req *req;


	if (a->connect_state != CONNECT_ST_NOT_READY) {
		RTW_WARN(FUNC_ADPT_FMT ": connect_st=%u, not NOT_READY?!\n",
			 FUNC_ADPT_ARG(a), a->connect_state);
		return;
	}

	_rtw_spinlock_init(&a->connect_st_lock);

	req = &a->connect_req;
	req->module_id = PHL_FG_MDL_CONNECT;
	req->priv = a;
	req->role = NULL; /* a->phl_role, but role will change by time */
	req->acquired = _connect_acquired;
	req->abort = _connect_abort;
	req->msg_hdlr = _connect_msg_hdlr;
	req->set_info = _connect_set_info;
	req->query_info = _connect_query_info;

	/* initialize state, lock protection is not necessary */
	a->connect_state = CONNECT_ST_IDLE;
	a->connect_abort = false;
}

enum rtw_phl_status rtw_connect_disconnect_prepare(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	enum rtw_phl_status status;


	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_FG_MDL_CONNECT);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_DISCONNECT_PREPARE);
	msg.band_idx = a->phl_role->hw_band;
	msg.rsvd[0] = (u8*)a->phl_role;

	status = rtw_phl_send_msg_to_dispr(GET_PHL_INFO(d), &msg, &attr, NULL);
	if (status != RTW_PHL_STATUS_SUCCESS)
		RTW_ERR(FUNC_ADPT_FMT ": send_msg_to_dispr fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);

	return status;
}

static enum rtw_phl_status _disconnect_done_notify(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	struct rtw_wifi_role_t *wrole = a->phl_role;
	enum rtw_phl_status status;


	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_FG_MDL_DISCONNECT);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_DISCONNECT);
	msg.band_idx = wrole->hw_band;
	msg.rsvd[0] = (u8*)wrole;

	status = rtw_phl_send_msg_to_dispr(GET_PHL_INFO(d), &msg, &attr, NULL);
	if (status != RTW_PHL_STATUS_SUCCESS)
		RTW_ERR(FUNC_ADPT_FMT ": send_msg_to_dispr fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);

	return status;
}

static void _disconnect_free_cmdobj(struct _ADAPTER *a)
{
	struct cmd_obj *cmd;


	cmd = a->discon_cmd;
	if (!cmd)
		return;

	_rtw_spinlock(&a->disconnect_lock);

	a->discon_cmd = NULL;

	if (cmd->sctx) {
		if (cmd->res == H2C_SUCCESS)
			rtw_sctx_done(&cmd->sctx);
		else
			rtw_sctx_done_err(&cmd->sctx, RTW_SCTX_DONE_CMD_ERROR);
	}

	_rtw_spinunlock(&a->disconnect_lock);

	rtw_free_cmd_obj(cmd);
}

static void _disconnect_cmd_done(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	enum rtw_phl_status status;


	RTW_DBG(FUNC_ADPT_FMT ": + token=0x%08x\n",
		FUNC_ADPT_ARG(a), a->disconnect_token);

	_rtw_spinlock(&a->disconnect_lock);
	/* avoid to race with rtw_disconnect_abort() */

	if (!a->disconnect_token) {
		RTW_WARN(FUNC_ADPT_FMT ": Others try to stop disconnect!\n",
			 FUNC_ADPT_ARG(a));
		_rtw_spinunlock(&a->disconnect_lock);
		return;
	}

	status = rtw_phl_free_cmd_token(GET_PHL_INFO(d),
					role->hw_band, &a->disconnect_token);
	if (status != RTW_PHL_STATUS_SUCCESS)
		RTW_ERR(FUNC_ADPT_FMT ": free_cmd_token fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);
	a->disconnect_token = 0;

	_rtw_spinunlock(&a->disconnect_lock);

	_disconnect_free_cmdobj(a);
}

static enum phl_mdl_ret_code _disconnect_acquired(void* dispr, void *priv)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;
	struct dvobj_priv *d = adapter_to_dvobj(a);


	RTW_DBG(FUNC_ADPT_FMT ": +\n", FUNC_ADPT_ARG(a));

	rtw_phl_disconnect(GET_PHL_INFO(d), a->phl_role, true);

	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));

	return MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code _disconnect_abort(void* dispr, void *priv)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;


	RTW_DBG(FUNC_ADPT_FMT ": + token=0x%08x\n",
		FUNC_ADPT_ARG(a), a->disconnect_token);

	/*
	 * Framework will free disconnect token automatically after abort,
	 * so do all in _disconnect_cmd_done() besides rtw_phl_free_cmd_token().
	 * ref: _disconnect_cmd_done()
	 */
	if (a->disconnect_token) {
		RTW_WARN(FUNC_ADPT_FMT ": framework asking abort!\n",
			 FUNC_ADPT_ARG(a));
		a->disconnect_token = 0;
	}

	_disconnect_free_cmdobj(a);

	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));

	return MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code _disconnect_msg_hdlr(void* dispr, void* priv,
						  struct phl_msg* msg)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;
	struct disconnect_parm *discon;
	struct stadel_event *stadel;
	struct _WLAN_BSSID_EX *network;
	u8 is_issue_deauth;
	u32 retry = 0;
	enum rtw_phl_status status = RTW_PHL_STATUS_SUCCESS;


	RTW_DBG(FUNC_ADPT_FMT ": + msg_id=0x%08x\n",
		FUNC_ADPT_ARG(a), msg->msg_id);

	if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_DISCONNECT) {
		RTW_INFO(FUNC_ADPT_FMT ": Message is not from disconnect module, "
			 "skip msg_id=0x%08x\n", FUNC_ADPT_ARG(a), msg->msg_id);
		RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));
		return MDL_RET_IGNORE;
	}

	/* Whether msg fail or not */
	switch (MSG_EVT_ID_FIELD(msg->msg_id)) {
	case MSG_EVT_DISCONNECT_PREPARE:
		RTW_DBG(FUNC_ADPT_FMT ": MSG_EVT_DISCONNECT_PREPARE\n",
			FUNC_ADPT_ARG(a));

		/*
		 * ref: Top half of disconnect_hdl(), before and including
		 *	rtw_mlmeext_disconnect().
		 * Note: The bottom half, after rtw_mlmeext_disconnect(),
		 *	 would be put to case MSG_EVT_DISCONNECT.
		 */

		if ((a->discon_cmd->cmdcode == CMD_SET_MLME_EVT)
#if CONFIG_DFS
			|| IS_RADAR_DETECTED(adapter_to_rfctl(a))
			|| adapter_to_rfctl(a)->csa_chandef.chan
#endif
		   )
			is_issue_deauth = 0;
		else
			is_issue_deauth = 1;

		if (is_issue_deauth) {
#ifdef CONFIG_PLATFORM_ROCKCHIPS
			/*
			 * To avoid connecting to AP fail during resume process,
			 * change retry count from 5 to 1
			 */
			retry = 1;
#else /* !CONFIG_PLATFORM_ROCKCHIPS */
			discon = (struct disconnect_parm*)a->discon_cmd->parmbuf;
			retry = discon->deauth_timeout_ms / 100;
#endif /* !CONFIG_PLATFORM_ROCKCHIPS */

			network = &a->mlmeextpriv.mlmext_info.network;
			issue_deauth_ex(a, network->MacAddress,
					WLAN_REASON_DEAUTH_LEAVING,
					retry, 100);
		}

		rtw_mlmeext_disconnect(a);

		status = _disconnect_done_notify(a);
		if (status == RTW_PHL_STATUS_SUCCESS)
			break;
		fallthrough;
	case MSG_EVT_DISCONNECT:
		RTW_DBG(FUNC_ADPT_FMT ": MSG_EVT_DISCONNECT\n",
			FUNC_ADPT_ARG(a));


		if (a->discon_cmd->cmdcode == CMD_SET_MLME_EVT) {
			/* EVT_DEL_STA case */
			/* ref: bottom half of rtw_stadel_event_callback() */
			stadel = (struct stadel_event*)(a->discon_cmd->parmbuf
							+ sizeof(struct rtw_evt_header));
			_stadel_posthandle_sta(a, stadel);
		} else {
			rtw_free_assoc_resources(a, _TRUE);
		}

		rtw_phl_disconnected_resume_hdlr(GET_PHL_INFO(adapter_to_dvobj(a)),
							a->phl_role);
		_disconnect_cmd_done(a);
		break;

	default:
		break;
	}

	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));
	return MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code _disconnect_set_info(void* dispr, void* priv,
						struct phl_module_op_info* info)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;


	RTW_DBG(FUNC_ADPT_FMT ": +\n", FUNC_ADPT_ARG(a));
	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));
	return MDL_RET_IGNORE;
}

static enum phl_mdl_ret_code _disconnect_query_info(void* dispr, void* priv,
						struct phl_module_op_info* info)
{
	struct _ADAPTER *a = (struct _ADAPTER *)priv;
	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;


	RTW_DBG(FUNC_ADPT_FMT ": +\n", FUNC_ADPT_ARG(a));

	switch (info->op_code) {
	case FG_REQ_OP_GET_ROLE:
		info->outbuf = (u8 *)a->phl_role;
		ret = MDL_RET_SUCCESS;
		break;
	default:
		break;
	}

	RTW_DBG(FUNC_ADPT_FMT ": -\n", FUNC_ADPT_ARG(a));
	return ret;
}

enum rtw_phl_status rtw_disconnect_cmd(struct _ADAPTER *a, struct cmd_obj *pcmd)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	struct rtw_evt_header *hdr;
	struct stadel_event *stadel;
	struct sta_info *sta;
	struct phl_cmd_token_req *cmd_req;
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;


	RTW_DBG(FUNC_ADPT_FMT ": + token=0x%08x\n",
		FUNC_ADPT_ARG(a), a->disconnect_token);

	if (a->disconnect_token) {
		RTW_WARN(FUNC_ADPT_FMT ": disconnect is on going...\n",
			 FUNC_ADPT_ARG(a));
		return RTW_PHL_STATUS_FAILURE;
	}

	if (pcmd->cmdcode == CMD_SET_MLME_EVT) {
		/* EVT_DEL_STA case */
		/* ref: top half of rtw_stadel_event_callback() */
		hdr = (struct rtw_evt_header*)pcmd->parmbuf;
		stadel = (struct stadel_event *)(pcmd->parmbuf + sizeof(*hdr));
		sta = rtw_get_stainfo(&a->stapriv, stadel->macaddr);
		if (!sta) {
			RTW_ERR(FUNC_ADPT_FMT ": stainfo(" MAC_FMT ") not exist!\n",
				FUNC_ADPT_ARG(a), MAC_ARG(stadel->macaddr));
			return RTW_PHL_STATUS_FAILURE;
		}
		rtw_wfd_st_switch(sta, 0);
		sta->hw_decrypted = _FALSE;
	}

	a->discon_cmd = pcmd;
	cmd_req = &a->disconnect_req;
	cmd_req->role = role;

	phl_status = rtw_phl_add_cmd_token_req(GET_PHL_INFO(d),
					       role->hw_band,
					       cmd_req, &a->disconnect_token);
	if ((phl_status != RTW_PHL_STATUS_SUCCESS)
	    && (phl_status != RTW_PHL_STATUS_PENDING)) {
		RTW_WARN(FUNC_ADPT_FMT ": add_cmd_token_req fail(0x%x)!\n",
			 FUNC_ADPT_ARG(a), phl_status);
		return RTW_PHL_STATUS_FAILURE;
	}

	RTW_DBG(FUNC_ADPT_FMT ": - token=0x%08x\n",
		FUNC_ADPT_ARG(a), a->disconnect_token);
	return RTW_PHL_STATUS_SUCCESS;
}

/* Wait disconnect FG to finish */
static bool _disconnect_wait(struct _ADAPTER *a, u32 timeout)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	systime start;
	u32 pass_t;
	u32 msg_i;	/* message interval */
	u32 msg_t = 0;	/* next time point to print message */
	enum rtw_phl_status status;
	bool terminated = false;


	RTW_DBG(FUNC_ADPT_FMT ": + token=0x%08x\n",
		FUNC_ADPT_ARG(a), a->disconnect_token);

	msg_i = timeout / 10;	/* at most print 10 msg in whole waiting time */
	if (!msg_i)
		msg_i = 1;
	start = rtw_get_current_time();
	do {
		_rtw_spinlock(&a->disconnect_lock);
		pass_t = rtw_get_passing_time_ms(start);
		if (!a->discon_cmd) {
			_rtw_spinunlock(&a->disconnect_lock);
			terminated = true;
			break;
		}
		_rtw_spinunlock(&a->disconnect_lock);

		if (pass_t >= msg_t) {
			RTW_DBG(FUNC_ADPT_FMT ": Waiting disconnect FG, %u ms/%u pass...\n",
				FUNC_ADPT_ARG(a), pass_t, timeout);
			msg_t += msg_i;
		}
		if (pass_t > timeout) {
			RTW_ERR(FUNC_ADPT_FMT ": Timeout! used:%u > max:%u ms\n",
				FUNC_ADPT_ARG(a), pass_t, timeout);
			break;
		}

		rtw_usleep_os(_rtw_systime_to_us(1)); /* sleep 1/HZ sec */
	} while (1);
	RTW_DBG(FUNC_ADPT_FMT ": Stop waiting disconnect FG, cost %u ms\n",
		FUNC_ADPT_ARG(a), pass_t);

exit:
	RTW_DBG(FUNC_ADPT_FMT ": - %s\n", FUNC_ADPT_ARG(a), terminated?"OK":"Fail!");
	return terminated;
}

/**
 * rtw_disconnect_abort() - Send cmd to abort disconnect FG
 * @a		pointer of struct _ADAPTER
 *
 * Send cancel cmd to framework to stop disconnect FG.
 *
 * Return 0 for sending command ok, 1 for not necessary to send command because
 * no disconnect FG exist, or negative status code returned from function
 * rtw_phl_cancel_cmd_token().
 */
static int rtw_disconnect_abort(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	u32 token;
	enum rtw_phl_status status;
	int err = 0;


	RTW_DBG(FUNC_ADPT_FMT ": + token=0x%08x\n",
		FUNC_ADPT_ARG(a), a->disconnect_token);

	_rtw_spinlock(&a->disconnect_lock);
	/* avoid to race with _disconnect_cmd_done() */

	if (!a->disconnect_token) {
		_rtw_spinunlock(&a->disconnect_lock);
		err = 1;
		goto exit;
	}

	token = a->disconnect_token;
	status = rtw_phl_cancel_cmd_token(GET_PHL_INFO(d),
					  role->hw_band, &token);
	if (status == RTW_PHL_STATUS_SUCCESS) {
		a->disconnect_token = 0;
	} else {
		err = -status;
		RTW_ERR(FUNC_ADPT_FMT ": cancel_cmd_token fail(0x%x)!\n",
			FUNC_ADPT_ARG(a), status);
	}

	_rtw_spinunlock(&a->disconnect_lock);

exit:
	RTW_DBG(FUNC_ADPT_FMT ": - err=%d\n", FUNC_ADPT_ARG(a), err);
	return err;
}

#define _DISCONNECT_ABORT_TO_		3000	/* unit: ms */
/**
 * rtw_disconnect_abort_wait() - Abort disconnect FG and wait to finish
 * @a		pointer of struct _ADAPTER
 *
 * Send cancel cmd to framework and wait disconnect FG to stop.
 *
 * Return 0 for disconnect FG already stopped, -1 for sending cancel command
 * fail, or -2 for waiting disconnect FG stop timeout.
 */
int rtw_disconnect_abort_wait(struct _ADAPTER *a)
{
	struct dvobj_priv *d = adapter_to_dvobj(a);
	struct rtw_wifi_role_t *role = a->phl_role;
	u32 token;
	enum rtw_phl_status status;
	int ret, err = 0;


	RTW_DBG(FUNC_ADPT_FMT ": + token=0x%08x\n",
		FUNC_ADPT_ARG(a), a->disconnect_token);

	if (!a->disconnect_token)
		goto exit;

	ret = rtw_disconnect_abort(a);
	if (ret) {
		if (ret < 0)
			err = -1; /* Fail to send cancel cmd */
		goto exit;
	}

	if (!_disconnect_wait(a, _DISCONNECT_ABORT_TO_))
		err = -2; /* Timeout */

exit:
	RTW_DBG(FUNC_ADPT_FMT ": - err=%d\n", FUNC_ADPT_ARG(a), err);
	return err;
}

void rtw_disconnect_req_free(struct _ADAPTER *a)
{
	rtw_disconnect_abort_wait(a);

	_rtw_spinlock_free(&a->disconnect_lock);
}

void rtw_disconnect_req_init(struct _ADAPTER *a)
{
	struct phl_cmd_token_req *req;


	_rtw_spinlock_init(&a->disconnect_lock);

	req = &a->disconnect_req;
	req->module_id = PHL_FG_MDL_DISCONNECT;
	req->priv = a;
	req->role = NULL;  /* a->phl_role, but role will change by time */
	req->acquired = _disconnect_acquired;
	req->abort = _disconnect_abort;
	req->msg_hdlr = _disconnect_msg_hdlr;
	req->set_info = _disconnect_set_info;
	req->query_info = _disconnect_query_info;
}
#endif /* CONFIG_STA_CMD_DISPR */
