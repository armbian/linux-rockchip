/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation.
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
#define _PHL_STA_C_
#include "phl_headers.h"

/*********** macid ctrl section ***********/
enum rtw_phl_status
phl_macid_ctrl_init(struct phl_info_t *phl)
{
	struct rtw_phl_com_t *phl_com = phl->phl_com;
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_com);
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl);
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	u8 i = 0;

	/* check invalid value or not */
	if (hal_spec->macid_num == 0) {
		PHL_ERR("Cannot get macid_num of hal\n");
		goto exit;
	}

	_os_spinlock_init(phl_to_drvpriv(phl), &macid_ctl->lock);

	macid_ctl->max_num = MIN(hal_spec->macid_num, PHL_MACID_MAX_NUM);

	PHL_INFO("%s macid max_num:%d\n", __func__, macid_ctl->max_num);

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++)
		macid_ctl->wrole_bmc[i] = macid_ctl->max_num;

	phl_status = RTW_PHL_STATUS_SUCCESS;

exit:
	return phl_status;
}

enum rtw_phl_status
phl_macid_ctrl_deinit(struct phl_info_t *phl)
{
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl);

	_os_spinlock_free(phl_to_drvpriv(phl), &macid_ctl->lock);
	macid_ctl->max_num = 0;

	return RTW_PHL_STATUS_SUCCESS;
}


static u8
_phl_macid_is_used(u32 *map, const u16 id)
{
	int map_idx = (int)id / 32;

	if (map[map_idx] & BIT(id % 32))
		return true;
	else
		return false;
}

static void
_phl_macid_map_set(u32 *map, const u16 id)
{
	int map_idx = (int)id / 32;
	map[map_idx] |=  BIT(id % 32);
}

static void
_phl_macid_map_clr(u32 *map, const u16 id)
{
	int map_idx = (int)id / 32;
	map[map_idx] &= ~BIT(id % 32);
}

static void _phl_wrole_bcmc_id_set(struct macid_ctl_t *macid_ctl,
				struct rtw_wifi_role_t *wrole, const u16 id)
{
	macid_ctl->wrole_bmc[wrole->id] = id;
}

static enum rtw_phl_status
_phl_alloc_macid(struct phl_info_t *phl_info,
			struct rtw_phl_stainfo_t *phl_sta)
{
	struct macid_ctl_t *mc = phl_to_mac_ctrl(phl_info);
	struct rtw_wifi_role_t *wrole = phl_sta->wrole;
	u8 bc_addr[MAC_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u16 mid = 0;
	u16 max_macid_num = 0;
	bool bmc_sta = false;

	if (wrole == NULL) {
		PHL_ERR("%s wrole=NULL!\n", __func__);
		return RTW_PHL_STATUS_FAILURE;
	}

	if (_os_mem_cmp(phl_to_drvpriv(phl_info),
			bc_addr, phl_sta->mac_addr, MAC_ALEN) == 0)
		bmc_sta = true;

	/* TODO
	if (wrole->type == PHL_RTYPE_STATION)
	else if (wrole->type == PHL_RTYPE_AP)*/

	/*TODO - struct mac_ax_hw_info-> u16 macid_num; need to check */
	max_macid_num = mc->max_num;
	_os_spinlock(phl_to_drvpriv(phl_info), &mc->lock, _bh, NULL);
	for(mid = 0; mid < max_macid_num; mid++) {
		if (!_phl_macid_is_used(mc->used_map, mid)) {
			_phl_macid_map_set(mc->used_map, mid);
			_phl_macid_map_set(&mc->wifi_role_usedmap[wrole->id][0], mid);
			mc->sta[mid] = phl_sta;

			if (bmc_sta) {
				_phl_macid_map_set(mc->bmc_map, mid);
				_phl_wrole_bcmc_id_set(mc, wrole, mid);
			}

			break;
		}
	}
	_os_spinunlock(phl_to_drvpriv(phl_info), &mc->lock, _bh, NULL);

	if (mid == max_macid_num) {
		phl_sta->macid = max_macid_num;
		PHL_ERR("%s cannot get macid\n", __func__);
		return RTW_PHL_STATUS_FAILURE;
	}

	phl_sta->macid = mid;

	PHL_INFO("%s allocate %02x:%02x:%02x:%02x:%02x:%02x for macid:%u\n", __func__,
	         phl_sta->mac_addr[0], phl_sta->mac_addr[1], phl_sta->mac_addr[2],
	         phl_sta->mac_addr[3], phl_sta->mac_addr[4], phl_sta->mac_addr[5],
	         phl_sta->macid);
	return RTW_PHL_STATUS_SUCCESS;
}

static enum rtw_phl_status
_phl_release_macid(struct phl_info_t *phl_info,
			struct rtw_phl_stainfo_t *phl_sta)
{
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct rtw_wifi_role_t *wrole = phl_sta->wrole;
	u8 bc_addr[MAC_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	u16 invalid_macid = macid_ctl->max_num;

	if (phl_sta->macid >= invalid_macid) {
		PHL_ERR("_phl_release_macid macid error (%d\n)", phl_sta->macid);
		phl_status = RTW_PHL_STATUS_FAILURE;
		goto exit;
	}


	_os_spinlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);

	if (!_phl_macid_is_used(macid_ctl->used_map, phl_sta->macid)) {
		PHL_WARN("_phl_release_macid macid unused (%d\n)", phl_sta->macid);
		_os_spinunlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);
		phl_status = RTW_PHL_STATUS_FAILURE;
		goto exit;
	}


	_phl_macid_map_clr(macid_ctl->used_map, phl_sta->macid);
	_phl_macid_map_clr(&macid_ctl->wifi_role_usedmap[wrole->id][0], phl_sta->macid);
	macid_ctl->sta[phl_sta->macid] = NULL;

	if (_os_mem_cmp(phl_to_drvpriv(phl_info),
			bc_addr, phl_sta->mac_addr, MAC_ALEN) == 0)
		_phl_macid_map_clr(macid_ctl->bmc_map, phl_sta->macid);

	phl_status = RTW_PHL_STATUS_SUCCESS;
	_os_spinunlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);

exit:
	PHL_INFO("%s release macid:%d - %02x:%02x:%02x:%02x:%02x:%02x \n",
		 __func__,
		 phl_sta->macid,
	         phl_sta->mac_addr[0], phl_sta->mac_addr[1], phl_sta->mac_addr[2],
	         phl_sta->mac_addr[3], phl_sta->mac_addr[4], phl_sta->mac_addr[5]);

	phl_sta->macid = invalid_macid;
	return phl_status;
}

u16 _phl_get_macid(struct phl_info_t *phl_info,
		struct rtw_phl_stainfo_t *phl_sta)
{
	/* TODO: macid management */
	return phl_sta->macid;
}

/**
 * This function export to core layer use
 * to get phl role bmc macid
 * @phl: see phl_info_t
 * @wrole: wifi role
 */
u16
rtw_phl_wrole_bcmc_id_get(void *phl, struct rtw_wifi_role_t *wrole)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);

	return macid_ctl->wrole_bmc[wrole->id];
}

/**
 * This function export to core layer use
 * to get maximum macid number
 * @phl: see phl_info_t
 */
u16
rtw_phl_get_macid_max_num(void *phl)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);

	return macid_ctl->max_num;
}

/**
 * This function export to core layer use
 * to check macid is bmc or not
 * @phl: see phl_info_t
 * @macid: macid
 */
u8
rtw_phl_macid_is_bmc(void *phl, u16 macid)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);

	if (macid >= macid_ctl->max_num) {
		PHL_ERR("%s macid(%d) is invalid\n", __func__, macid);
		return true;
	}

	return _phl_macid_is_used(macid_ctl->bmc_map, macid);
}


/**
 * This function export to core layer use
 * to check macid is used or not
 * @phl: see phl_info_t
 * @macid: macid
 */
u8
rtw_phl_macid_is_used(void *phl, u16 macid)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);

	if (macid >= macid_ctl->max_num) {
		PHL_ERR("%s macid(%d) is invalid\n", __func__, macid);
		return true;
	}


	return _phl_macid_is_used(macid_ctl->used_map, macid);
}

/**
 * This function is used to
 * check macid shared by all wifi role
 * @phl: see phl_info_t
 * @macid: macid
 */

u8
rtw_phl_macid_is_wrole_shared(void *phl, u16 macid)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);
	int i = 0;
	u8 iface_bmp = 0;

	if (macid >= macid_ctl->max_num) {
		PHL_ERR("%s macid(%d) is invalid\n", __func__, macid);
		return false;
	}

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		if (_phl_macid_is_used(&macid_ctl->wifi_role_usedmap[i][0], macid)) {
			if (iface_bmp)
				return true;
			iface_bmp |= BIT(i);
		}
	}
	return false;
}

/**
 * This function is used to
 * check macid not shared by all wifi role
 * and belong to wifi role
 * @phl: see phl_info_t
 * @macid: macid
 * @wrole: check id belong to this wifi role
 */
u8
rtw_phl_macid_is_wrole_specific(void *phl,
					u16 macid, struct rtw_wifi_role_t *wrole)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);
	int i = 0;
	u8 iface_bmp = 0;

	if (macid >= macid_ctl->max_num) {
		PHL_ERR("%s macid(%d) invalid\n", __func__, macid);
		return false;
	}

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		if (_phl_macid_is_used(&macid_ctl->wifi_role_usedmap[i][0], macid)) {
			if (iface_bmp || i != wrole->id)
				return false;
			iface_bmp |= BIT(i);
		}
	}

	return iface_bmp ? true : false;
}


/*********** stainfo_ctrl section ***********/
static enum rtw_phl_status
_phl_stainfo_init(struct phl_info_t *phl_info,
                  struct rtw_phl_stainfo_t *phl_sta)
{
	void *drv = phl_to_drvpriv(phl_info);

	INIT_LIST_HEAD(&phl_sta->list);
	_os_spinlock_init(drv, &phl_sta->tid_rx_lock);
	_os_mem_set(drv, phl_sta->tid_rx, 0, sizeof(phl_sta->tid_rx));
	_os_event_init(drv, &phl_sta->comp_sync);
	_os_init_timer(drv, &phl_sta->reorder_timer,
	               phl_sta_rx_reorder_timer_expired, phl_sta, "reorder_timer");

	_os_atomic_set(drv, &phl_sta->ps_sta, 0);

	if (rtw_hal_stainfo_init(phl_info->hal, phl_sta) !=
	    RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("hal_stainfo_init failed\n");
		FUNCOUT();
		return RTW_PHL_STATUS_FAILURE;
	}
	phl_sta->active = false;
#ifdef RTW_WKARD_CHECK_STAINFO_DOUBLE_DEL
	phl_sta->allocated = false;
#endif
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
_phl_stainfo_deinit(struct phl_info_t *phl_info,
				struct rtw_phl_stainfo_t *phl_sta)
{
	void *drv = phl_to_drvpriv(phl_info);

	_os_release_timer(drv, &phl_sta->reorder_timer);
	_os_spinlock_free(phl_to_drvpriv(phl_info), &phl_sta->tid_rx_lock);
	_os_event_free(drv, &phl_sta->comp_sync);

	if (rtw_hal_stainfo_deinit(phl_info->hal, phl_sta)!=
					RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("hal_stainfo_deinit failed\n");
		FUNCOUT();
		return RTW_PHL_STATUS_FAILURE;
	}
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_stainfo_enqueue(struct phl_info_t *phl_info,
			 struct phl_queue *sta_queue,
			 struct rtw_phl_stainfo_t *psta)
{
	void *drv = phl_to_drvpriv(phl_info);

	if (!psta)
		return RTW_PHL_STATUS_FAILURE;

	_os_spinlock(drv, &sta_queue->lock, _bh, NULL);
	list_add_tail(&psta->list, &sta_queue->queue);
	sta_queue->cnt++;
	_os_spinunlock(drv, &sta_queue->lock, _bh, NULL);
	return RTW_PHL_STATUS_SUCCESS;
}

struct rtw_phl_stainfo_t *
phl_stainfo_dequeue(struct phl_info_t *phl_info,
			struct phl_queue *sta_queue)
{
	struct rtw_phl_stainfo_t *psta = NULL;
	void *drv = phl_to_drvpriv(phl_info);

	_os_spinlock(drv, &sta_queue->lock, _bh, NULL);
	if (list_empty(&sta_queue->queue)) {
		psta = NULL;
	} else {
		psta = list_first_entry(&sta_queue->queue,
					struct rtw_phl_stainfo_t, list);

		list_del(&psta->list);
		sta_queue->cnt--;
	}
	_os_spinunlock(drv, &sta_queue->lock, _bh, NULL);

	return psta;
}

enum rtw_phl_status
phl_stainfo_queue_del(struct phl_info_t *phl_info,
			 struct phl_queue *sta_queue,
			 struct rtw_phl_stainfo_t *psta)
{
	void *drv = phl_to_drvpriv(phl_info);

	if (!psta)
		return RTW_PHL_STATUS_FAILURE;

	_os_spinlock(drv, &sta_queue->lock, _bh, NULL);
	if (sta_queue->cnt) {
		list_del(&psta->list);
		sta_queue->cnt--;
	}
	_os_spinunlock(drv, &sta_queue->lock, _bh, NULL);
	return RTW_PHL_STATUS_SUCCESS;
}

struct rtw_phl_stainfo_t *
phl_stainfo_queue_search(struct phl_info_t *phl_info,
			 struct phl_queue *sta_queue,
			 u8 *addr)
{
	struct rtw_phl_stainfo_t *sta = NULL;
	_os_list *sta_list = &sta_queue->queue;
	void *drv = phl_to_drvpriv(phl_info);
	bool sta_found = false;

	_os_spinlock(drv, &sta_queue->lock, _bh, NULL);
	if (list_empty(sta_list) == true)
		goto _exit;

	phl_list_for_loop(sta, struct rtw_phl_stainfo_t, sta_list, list) {
		if (_os_mem_cmp(phl_to_drvpriv(phl_info),
			sta->mac_addr, addr, MAC_ALEN) == 0) {
			sta_found = true;
			break;
		}
	}
_exit:
	_os_spinunlock(drv, &sta_queue->lock, _bh, NULL);

	if (sta_found == false)
		sta = NULL;

	return sta;
}

struct rtw_phl_stainfo_t *
phl_stainfo_queue_get_first(struct phl_info_t *phl_info,
			 struct phl_queue *sta_queue)
{

	_os_list *sta_list = &sta_queue->queue;
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_phl_stainfo_t *sta = NULL;

	/* first sta info in assoc_sta_queu is self sta info */
	_os_spinlock(drv, &sta_queue->lock, _bh, NULL);
	if (list_empty(sta_list) == true)
		goto _exit;

	sta = list_first_entry(sta_list, struct rtw_phl_stainfo_t, list);
_exit :
	_os_spinunlock(drv, &sta_queue->lock, _bh, NULL);

	return sta;
}

enum rtw_phl_status
phl_stainfo_ctrl_deinie(struct phl_info_t *phl_info)
{
	struct stainfo_ctl_t *sta_ctrl = phl_to_sta_ctrl(phl_info);
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_phl_stainfo_t *psta = NULL;
	struct phl_queue *fsta_queue = &sta_ctrl->free_sta_queue;

	FUNCIN();
	do {
		psta = phl_stainfo_dequeue(phl_info, fsta_queue);
		if (psta)
			_phl_stainfo_deinit(phl_info, psta);

	}while (psta != NULL);

	pq_deinit(drv, fsta_queue);

	if (sta_ctrl->allocated_stainfo_buf)
		_os_mem_free(drv, sta_ctrl->allocated_stainfo_buf,
					sta_ctrl->allocated_stainfo_sz);
	FUNCOUT();
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_stainfo_ctrl_init(struct phl_info_t *phl_info)
{
	struct stainfo_ctl_t *sta_ctrl = phl_to_sta_ctrl(phl_info);
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_phl_stainfo_t *psta = NULL;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct phl_queue *fsta_queue = NULL;

	u16 i;
	bool sta_init_fail = false;

	FUNCIN();
	sta_ctrl->phl_info = phl_info;

	sta_ctrl->allocated_stainfo_sz = sizeof(struct rtw_phl_stainfo_t) * PHL_MAX_STA_NUM;

	#ifdef MEM_ALIGNMENT
	sta_ctrl->allocated_stainfo_sz += MEM_ALIGNMENT_OFFSET;
	#endif

	sta_ctrl->allocated_stainfo_buf =
			_os_mem_alloc(drv, sta_ctrl->allocated_stainfo_sz);

	if (!sta_ctrl->allocated_stainfo_buf) {
		PHL_ERR("allocate stainfo buf failed\n");
		goto _exit;
	}
	sta_ctrl->stainfo_buf = sta_ctrl->allocated_stainfo_buf;

	#ifdef MEM_ALIGNMENT
	if (sta_ctrl->stainfo_buf & MEM_ALIGNMENT_PADDING)
		sta_ctrl->stainfo_buf += MEM_ALIGNMENT_OFFSET -
			(sta_ctrl->stainfo_buf & MEM_ALIGNMENT_PADDING);
	#endif

	fsta_queue = &sta_ctrl->free_sta_queue;

	pq_init(drv, fsta_queue);
	psta = (struct rtw_phl_stainfo_t *)(sta_ctrl->stainfo_buf);

	for (i = 0; i < PHL_MAX_STA_NUM; i++) {
		if (_phl_stainfo_init(phl_info, psta) != RTW_PHL_STATUS_SUCCESS) {
			sta_init_fail = true;
			break;
		}
		phl_stainfo_enqueue(phl_info, fsta_queue, psta);
		psta++;
	}

	if (sta_init_fail == true) {
		PHL_ERR("sta_init failed\n");
		phl_stainfo_ctrl_deinie(phl_info);
		goto _exit;
	}
	PHL_DUMP_STACTRL_EX(phl_info);

	pstatus = RTW_PHL_STATUS_SUCCESS;
_exit:
	FUNCOUT();
	return pstatus;
}

/*********** phl stainfo section ***********/
#ifdef DBG_PHL_STAINFO
void
phl_dump_stactrl(const char *caller, const int line, bool show_caller,
						struct phl_info_t *phl_info)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	u8 ridx = MAX_WIFI_ROLE_NUMBER;
	struct rtw_wifi_role_t *role;
	struct stainfo_ctl_t *sta_ctrl = NULL;

	sta_ctrl = phl_to_sta_ctrl(phl_info);

	if (show_caller)
		PHL_INFO("[PSTA] ###### FUN - %s LINE - %d #######\n", caller, line);
	PHL_INFO("[PSTA] PHL_MAX_STA_NUM:%d\n", PHL_MAX_STA_NUM);
	PHL_INFO("[PSTA] sta_ctrl - q_cnt :%d\n", sta_ctrl->free_sta_queue.cnt);
	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		role = &(phl_com->wifi_roles[ridx]);
		PHL_INFO("[PSTA] wrole_%d asoc_q cnt :%d\n",
				ridx, role->assoc_sta_queue.cnt);
	}
	if (show_caller)
		PHL_INFO("#################################\n");
}

static void _phl_dump_stainfo(struct rtw_phl_stainfo_t *phl_sta)
{
	PHL_INFO("\t[STA] MAC-ID:%d, AID:%d, MAC-ADDR:%02x-%02x-%02x-%02x-%02x-%02x, Active:%s\n",
			phl_sta->macid, phl_sta->aid,
			phl_sta->mac_addr[0],phl_sta->mac_addr[1],phl_sta->mac_addr[2],
			phl_sta->mac_addr[3],phl_sta->mac_addr[4],phl_sta->mac_addr[5],
			(phl_sta->active) ? "Y" : "N");
	PHL_INFO("\t[STA] WROLE-IDX:%d wlan_mode:0x%02x\n", phl_sta->wrole->id, phl_sta->wmode);
	PHL_DUMP_CHAN_DEF(&phl_sta->chandef);

	/****** statistic ******/
	PHL_INFO("\t[STA] TP -[Tx:%d Rx :%d BI:N/A] (KBits)\n",
		phl_sta->stats.tx_tp_kbits, phl_sta->stats.rx_tp_kbits);
	PHL_INFO("\t[STA] Total -[Tx:%llu Rx :%llu BI:N/A] (Bytes)\n",
		phl_sta->stats.tx_byte_total, phl_sta->stats.rx_byte_total);
	/****** asoc_cap ******/
	/****** protect ******/
	/****** sec_mode ******/
	/****** rssi_stat ******/
	PHL_INFO("\t\t[HAL STA] rssi:%d assoc_rssi:%d, ofdm:%d, cck:%d, rssi_ma:%d, ma_rssi:%d\n",
			(phl_sta->hal_sta->rssi_stat.rssi >> 1), phl_sta->hal_sta->rssi_stat.assoc_rssi,
			(phl_sta->hal_sta->rssi_stat.rssi_ofdm >> 1), (phl_sta->hal_sta->rssi_stat.rssi_cck >> 1),
			(phl_sta->hal_sta->rssi_stat.rssi_ma >> 5), phl_sta->hal_sta->rssi_stat.ma_rssi);

	/****** ra_info ******/
	PHL_INFO("\t\t[HAL STA] - RA info\n");

	PHL_INFO("\t\t[HAL STA] Tx rate:0x%04x ra_bw_mode:%d, curr_tx_bw:%d\n",
				phl_sta->hal_sta->ra_info.curr_tx_rate,
				phl_sta->hal_sta->ra_info.ra_bw_mode,
				phl_sta->hal_sta->ra_info.curr_tx_bw);

	PHL_INFO("\t\t[HAL STA] dis_ra:%s ra_registered:%s\n",
				(phl_sta->hal_sta->ra_info.dis_ra) ? "Y" : "N",
				(phl_sta->hal_sta->ra_info.ra_registered) ? "Y" : "N");

	PHL_INFO("\t\t[HAL STA] ra_mask:0x%08llx cur_ra_mask:0x%08llx, retry_ratio:%d\n",
				phl_sta->hal_sta->ra_info.ra_mask,
				phl_sta->hal_sta->ra_info.cur_ra_mask,
				phl_sta->hal_sta->ra_info.curr_retry_ratio);
	/****** ra_info - Report ******/
	PHL_INFO("\t\t[HAL STA] RA Report: gi_ltf:%d rate_mode:%d, bw:%d, mcs_ss_idx:%d\n",
				phl_sta->hal_sta->ra_info.rpt_rt_i.gi_ltf,
				phl_sta->hal_sta->ra_info.rpt_rt_i.mode,
				phl_sta->hal_sta->ra_info.rpt_rt_i.bw,
				phl_sta->hal_sta->ra_info.rpt_rt_i.mcs_ss_idx);

	PHL_INFO("\t\t[HAL STA] HAL rx_ok_cnt:%d rx_err_cnt:%d, rx_rate_plurality:%d\n\n",
				phl_sta->hal_sta->trx_stat.rx_ok_cnt,
				phl_sta->hal_sta->trx_stat.rx_err_cnt,
				phl_sta->hal_sta->trx_stat.rx_rate_plurality);

}
void phl_dump_stainfo_all(const char *caller, const int line, bool show_caller,
				struct phl_info_t *phl_info)
{
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	u16 max_macid_num = 0;
	u16 mid = 0;

	if (show_caller)
		PHL_INFO("###### FUN - %s LINE - %d #######\n", caller, line);

	max_macid_num = macid_ctl->max_num;
	PHL_INFO("max_macid_num:%d\n", max_macid_num);
	_os_spinlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);
	for(mid = 0; mid < max_macid_num; mid++) {
		if (_phl_macid_is_used(macid_ctl->used_map, mid)) {
			phl_sta = macid_ctl->sta[mid];
			if (phl_sta)
				_phl_dump_stainfo(phl_sta);
		}
	}
	_os_spinunlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);

	if (show_caller)
		PHL_INFO("#################################\n");
}

const char *const _rtype_str[] = {
	"NONE",
	"STA",
	"AP",
	"VAP",
	"ADHOC",
	"MASTER",
	"MESH",
	"MONITOR",
	"PD",
	"GC",
	"GO",
	"TDLS",
	"NAN",
	"NONE"
};

void phl_dump_stainfo_per_role(const char *caller, const int line, bool show_caller,
				struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_phl_stainfo_t *sta = NULL;
	int sta_cnt = 0;

	if (show_caller)
		PHL_INFO("[STA] ###### FUN - %s LINE - %d #######\n", caller, line);

	PHL_INFO("WR_IDX:%d RTYPE:%s, mac-addr:%02x-%02x-%02x-%02x-%02x-%02x\n",
			wrole->id,
			_rtype_str[wrole->type],
			wrole->mac_addr[0], wrole->mac_addr[1], wrole->mac_addr[2],
			wrole->mac_addr[3], wrole->mac_addr[4], wrole->mac_addr[5]);

	_os_spinlock(drv, &wrole->assoc_sta_queue.lock, _bh, NULL);

	if (wrole->type == PHL_RTYPE_STATION && wrole->mstate == MLME_LINKED)
		sta_cnt = 1;
	else if (wrole->type == PHL_RTYPE_TDLS)
		sta_cnt = wrole->assoc_sta_queue.cnt;
	else
		sta_cnt = wrole->assoc_sta_queue.cnt - 1;

	PHL_INFO("assoced STA num: %d\n", sta_cnt);
	phl_list_for_loop(sta, struct rtw_phl_stainfo_t, &wrole->assoc_sta_queue.queue, list) {
		if (sta)
			_phl_dump_stainfo(sta);
	}
	_os_spinunlock(drv, &wrole->assoc_sta_queue.lock, _bh, NULL);

	if (show_caller)
		PHL_INFO("#################################\n");
}

void rtw_phl_sta_dump_info(void *phl, bool show_caller, struct rtw_wifi_role_t *wr, u8 mode)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	if (mode == 1) {
		if (show_caller) {
			PHL_DUMP_STACTRL_EX(phl_info);
		} else {
			PHL_DUMP_STACTRL(phl_info);
		}
	} else if (mode == 2) {
		if (show_caller) {
			PHL_DUMP_STAINFO_EX(phl_info);
		} else {
			PHL_DUMP_STAINFO(phl_info);
		}
	} else if (mode == 3) {
		if (show_caller) {
			PHL_DUMP_ROLE_STAINFO_EX(phl_info, wr);
		} else {
			PHL_DUMP_ROLE_STAINFO(phl_info, wr);
		}
	} else {
		if (show_caller) {
			PHL_DUMP_STACTRL_EX(phl_info);
			PHL_DUMP_STAINFO_EX(phl_info);
			PHL_DUMP_ROLE_STAINFO_EX(phl_info, wr);
		}
		else {
			PHL_DUMP_STACTRL(phl_info);
			PHL_DUMP_STAINFO(phl_info);
			PHL_DUMP_ROLE_STAINFO(phl_info, wr);
		}
	}
}
#endif /*DBG_PHL_STAINFO*/

static bool _phl_self_stainfo_chk(struct phl_info_t *phl_info,
	struct rtw_wifi_role_t *wrole, struct rtw_phl_stainfo_t *sta)
{
	void *drv = phl_to_drvpriv(phl_info);
	bool is_self = false;

	switch (wrole->type) {
	case PHL_RTYPE_STATION:
	case PHL_RTYPE_P2P_GC:
		_os_mem_cpy(drv, sta->mac_addr, wrole->mac_addr, MAC_ALEN);
		is_self = true;
	break;

	case PHL_RTYPE_AP:
	case PHL_RTYPE_MESH:
	case PHL_RTYPE_P2P_GO:
	case PHL_RTYPE_TDLS:
		if (_os_mem_cmp(drv, wrole->mac_addr, sta->mac_addr, MAC_ALEN) == 0)
			is_self = true;
	break;

	case PHL_RTYPE_NONE:
	case PHL_RTYPE_VAP:
	case PHL_RTYPE_ADHOC:
	case PHL_RTYPE_ADHOC_MASTER:
	case PHL_RTYPE_MONITOR:
	case PHL_RTYPE_P2P_DEVICE:
	case PHL_RTYPE_NAN:
	case PHL_MLME_MAX:
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "_phl_self_stainfo_chk(): Unsupported case:%d, please check it\n",
				wrole->type);
		break;
	default:
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "_phl_self_stainfo_chk(): role-type(%d) not recognize\n",
				wrole->type);
		break;
	}
	return is_self;
}

enum rtw_phl_status
phl_free_stainfo_sw(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	if(sta == NULL) {
		PHL_ERR("%s sta is NULL\n", __func__);
		return RTW_PHL_STATUS_FAILURE;
	}

	phl_free_rx_reorder(phl_info, sta);

	pstatus = phl_deregister_tx_ring((void *)phl_info, sta->macid);
	if (pstatus != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("macid(%d) phl_deregister_tx_ring failed\n", sta->macid);
	}

	/* release macid for used_map */
	pstatus = _phl_release_macid(phl_info, sta);
	if (pstatus != RTW_PHL_STATUS_SUCCESS)
		PHL_ERR("_phl_release_macid failed\n");

	return pstatus;
}

enum rtw_phl_status
__phl_free_stainfo_sw(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct stainfo_ctl_t *sta_ctrl = phl_to_sta_ctrl(phl_info);
	struct rtw_wifi_role_t *wrole = NULL;

	FUNCIN();
	if(sta == NULL) {
		PHL_ERR("%s sta is NULL\n", __func__);
		goto _exit;
	}

#ifdef RTW_WKARD_CHECK_STAINFO_DOUBLE_DEL
	if (!sta->allocated) {
		PHL_INFO("%s sta has not been allocated\n", __func__);
		goto _exit;
	}
#endif

	wrole = sta->wrole;

	if (!is_broadcast_mac_addr(sta->mac_addr)) {
		if (_phl_self_stainfo_chk(phl_info, wrole, sta) == true) {
			pstatus = RTW_PHL_STATUS_SUCCESS;
			goto _exit;
		}
	}

	pstatus = phl_stainfo_queue_del(phl_info, &wrole->assoc_sta_queue, sta);
	if (pstatus != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("phl_stainfo_queue_del failed\n");
	}

#ifdef RTW_WKARD_CHECK_STAINFO_DOUBLE_DEL
	sta->allocated = false;
#endif

	pstatus = phl_free_stainfo_sw(phl_info, sta);
	if (pstatus != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("macid(%d) _phl_free_stainfo_sw failed\n", sta->macid);
	}

	pstatus = phl_stainfo_enqueue(phl_info, &sta_ctrl->free_sta_queue, sta);
	if (pstatus != RTW_PHL_STATUS_SUCCESS)
		PHL_ERR("phl_stainfo_enqueue to free queue failed\n");

#ifdef RTW_WKARD_AP_CLIENT_ADD_DEL_NTY
	if ((wrole->type == PHL_RTYPE_AP) ||
	    (wrole->type == PHL_RTYPE_VAP) ||
	    (wrole->type == PHL_RTYPE_MESH) ||
	    (wrole->type == PHL_RTYPE_P2P_GO))
		phl_role_ap_client_notify(phl_info, wrole, MLME_NO_LINK);
#endif

_exit:
	PHL_DUMP_STACTRL_EX(phl_info);
	FUNCOUT();
	return pstatus;
}

enum rtw_phl_status
rtw_phl_free_stainfo_sw(void *phl, struct rtw_phl_stainfo_t *sta)
{
	return __phl_free_stainfo_sw((struct phl_info_t *)phl, sta);
}

enum rtw_phl_status
phl_free_stainfo_hw(struct phl_info_t *phl_info,
					struct rtw_phl_stainfo_t *sta)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	if (sta == NULL) {
		PHL_ERR("%s sta == NULL\n", __func__);
		goto _exit;
	}

	phl_pkt_ofld_del_entry(phl_info, sta->macid);

	sta->active = false;
	if (rtw_hal_del_sta_entry(phl_info->hal, sta) == RTW_HAL_STATUS_SUCCESS)
		pstatus = RTW_PHL_STATUS_SUCCESS;
	else
		PHL_ERR("rtw_hal_del_sta_entry failed\n");
_exit:
	return pstatus;
}

enum rtw_phl_status
__phl_free_stainfo_hw(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta)
{
	struct rtw_wifi_role_t *wrole = sta->wrole;

	if (!is_broadcast_mac_addr(sta->mac_addr)) {
		if (_phl_self_stainfo_chk(phl_info, wrole, sta) == true)
			return RTW_PHL_STATUS_SUCCESS;
	}
	return phl_free_stainfo_hw(phl_info, sta);
}

static enum rtw_phl_status
__phl_free_stainfo(struct phl_info_t *phl, struct rtw_phl_stainfo_t *sta)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	pstatus = __phl_free_stainfo_hw(phl, sta);
	if (pstatus != RTW_PHL_STATUS_SUCCESS)
		PHL_ERR("__phl_free_stainfo_hw failed\n");

	pstatus = __phl_free_stainfo_sw(phl, sta);
	if (pstatus != RTW_PHL_STATUS_SUCCESS)
		PHL_ERR("__phl_free_stainfo_sw failed\n");
	return pstatus;
}


static enum rtw_phl_status
_phl_alloc_stainfo_sw(struct phl_info_t *phl_info,struct rtw_phl_stainfo_t *sta)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	pstatus = _phl_alloc_macid(phl_info, sta);
	if (pstatus != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s allocate macid failure!\n", __func__);
		goto error_alloc_macid;
	}

	if (phl_register_tx_ring(phl_info, sta->macid,
		sta->wrole->hw_band, sta->wrole->hw_wmm, sta->wrole->hw_port) !=
					RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s register_tx_ring failure!\n", __func__);
		goto error_register_tx_ring;
	}
	pstatus = RTW_PHL_STATUS_SUCCESS;
	return pstatus;

error_register_tx_ring:
	_phl_release_macid(phl_info, sta);
error_alloc_macid:
	return pstatus;
}

static void _phl_sta_set_default_value(struct phl_info_t *phl_info,
		struct rtw_phl_stainfo_t *phl_sta)
{
	phl_sta->bcn_hit_cond = 0; /* beacon:A3 probersp: A1 & A3 */

	/* fit rule
	 * 0: A1 & A2
	 * 1: A1 & A3
	 *
	 * Rule 0 should be used for both AP and STA modes.
	 *
	 * For STA, A3 is source address(SA) which can be any peer on the LAN.
	 *
	 * For AP, A3 is destination address(DA) which can also be any node
	 * on the LAN. A1 & A2 match find the address CAM entry that contains the
	 * correct security CAM ID and MAC ID.
	 */
	phl_sta->hit_rule = 0;
}

struct rtw_phl_stainfo_t *
phl_alloc_stainfo_sw(struct phl_info_t *phl_info,
                     u8 *sta_addr,
                     struct rtw_wifi_role_t *wrole)
{
	struct stainfo_ctl_t *sta_ctrl = phl_to_sta_ctrl(phl_info);
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	void *drv = phl_to_drvpriv(phl_info);
	bool bmc_sta = false;

	FUNCIN();
	if (is_broadcast_mac_addr(sta_addr))
		bmc_sta = true;

	/* if sta_addr is bmc addr, allocate new sta_info */
	if (wrole->type == PHL_RTYPE_STATION && !bmc_sta) {
		phl_sta = rtw_phl_get_stainfo_self(phl_info, wrole);
		if (phl_sta) {
			_os_mem_cpy(drv, phl_sta->mac_addr, sta_addr, MAC_ALEN);
			goto _exit;
		}
	}

	/* check station info exist */
	phl_sta = rtw_phl_get_stainfo_by_addr(phl_info, wrole, sta_addr);
	if (phl_sta) {
		PHL_INFO("%s phl_sta(%02x:%02x:%02x:%02x:%02x:%02x) exist\n",
		         __func__, sta_addr[0], sta_addr[1], sta_addr[2],
		         sta_addr[3], sta_addr[4], sta_addr[5]);
		goto _exit;
	}

	phl_sta = phl_stainfo_dequeue(phl_info, &sta_ctrl->free_sta_queue);
	if (phl_sta == NULL) {
		PHL_ERR("allocate phl_sta failure!\n");
		goto _exit;
	}

	_os_mem_cpy(drv, phl_sta->mac_addr, sta_addr, MAC_ALEN);
	phl_sta->wrole = wrole;

	if (_phl_alloc_stainfo_sw(phl_info, phl_sta) != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("_phl_alloc_stainfo_sw failed\n");
		goto error_alloc_sta;
	}
	_phl_sta_set_default_value(phl_info, phl_sta);

	phl_stainfo_enqueue(phl_info, &wrole->assoc_sta_queue, phl_sta);
#ifdef RTW_WKARD_CHECK_STAINFO_DOUBLE_DEL
	phl_sta->allocated = true;
#endif

	#ifdef RTW_WKARD_AP_CLIENT_ADD_DEL_NTY
	if (_phl_self_stainfo_chk(phl_info, wrole, phl_sta) == false) {
		if ((wrole->type == PHL_RTYPE_AP) ||
		     (wrole->type == PHL_RTYPE_VAP) ||
		     (wrole->type == PHL_RTYPE_MESH) ||
		     (wrole->type == PHL_RTYPE_P2P_GO)) {
			phl_role_ap_client_notify(phl_info, wrole, MLME_LINKING);
		}
	}
	#endif
_exit:
	PHL_DUMP_STACTRL_EX(phl_info);
	FUNCOUT();

	return phl_sta;

error_alloc_sta:
	phl_stainfo_enqueue(phl_info, &sta_ctrl->free_sta_queue, phl_sta);
	phl_sta = NULL;
	PHL_DUMP_STACTRL_EX(phl_info);
	FUNCOUT();
	return phl_sta;
}

struct rtw_phl_stainfo_t *
rtw_phl_alloc_stainfo_sw(void *phl, u8 *sta_addr,
				struct rtw_wifi_role_t *wrole)
{
	return phl_alloc_stainfo_sw((struct phl_info_t *)phl, sta_addr, wrole);
}

enum rtw_phl_status
phl_alloc_stainfo_hw(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	if (sta == NULL) {
		PHL_ERR("%s sta == NULL\n", __func__);
		goto _exit;
	}

	if (rtw_hal_add_sta_entry(phl_info->hal, sta) != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("%s rtw_hal_add_sta_entry failure!\n", __func__);
		goto _exit;
	}

	sta->active = true;

	pstatus = phl_pkt_ofld_add_entry(phl_info, sta->macid);
	if (RTW_PHL_STATUS_SUCCESS != pstatus)
		PHL_ERR("%s phl_pkt_ofld_add_entry failure!\n", __func__);

_exit:
	return pstatus;
}

enum rtw_phl_status
__phl_alloc_stainfo_hw(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta)
{
	return phl_alloc_stainfo_hw(phl_info, sta);
}

static enum rtw_phl_status
__phl_alloc_stainfo(struct phl_info_t *phl,
                    struct rtw_phl_stainfo_t **sta,
                    u8 *sta_addr,
                    struct rtw_wifi_role_t *wrole)
{
	struct rtw_phl_stainfo_t *alloc_sta = NULL;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	alloc_sta = phl_alloc_stainfo_sw(phl, sta_addr, wrole);
	if (alloc_sta == NULL) {
		PHL_ERR("%s can't alloc stainfo\n", __func__);
		*sta = alloc_sta;
		goto _exit;
	}

	if (alloc_sta->active == false) {
		pstatus = __phl_alloc_stainfo_hw(phl, alloc_sta);
		if (pstatus != RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("__phl_alloc_stainfo_hw failed\n");
			goto _err_alloc_sta_hw;
		}
	}

	PHL_INFO("%s success - macid:%u %02x:%02x:%02x:%02x:%02x:%02x\n",
	         __func__, alloc_sta->macid,
	         alloc_sta->mac_addr[0], alloc_sta->mac_addr[1], alloc_sta->mac_addr[2],
	         alloc_sta->mac_addr[3], alloc_sta->mac_addr[4], alloc_sta->mac_addr[5]);

	*sta = alloc_sta;
	return RTW_PHL_STATUS_SUCCESS;

_err_alloc_sta_hw:
	__phl_free_stainfo_sw(phl, alloc_sta);
	*sta = alloc_sta = NULL;
_exit:
	return RTW_PHL_STATUS_FAILURE;
}

static enum rtw_phl_status
_phl_alloc_stainfo(struct phl_info_t *phl,
                   struct rtw_phl_stainfo_t **sta,
                   u8 *sta_addr,
                   struct rtw_wifi_role_t *wrole,
                   bool alloc,
                   bool only_hw)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	if (alloc) {
		if (only_hw)
			pstatus = __phl_alloc_stainfo_hw(phl, *sta);
		else
			pstatus = __phl_alloc_stainfo(phl, sta, sta_addr, wrole);
	} else {
		if (only_hw)
			pstatus = __phl_free_stainfo_hw(phl, *sta);
		else
			pstatus = __phl_free_stainfo(phl, *sta);
	}
	return pstatus;
}

#ifdef CONFIG_CMD_DISP
struct cmd_stainfo_param {
	struct rtw_phl_stainfo_t **sta;
	u8 sta_addr[MAC_ALEN];
	struct rtw_wifi_role_t *wrole;
	bool alloc;
	bool only_hw;
};

static void
_phl_cmd_alloc_stainfo_done(void *drv_priv,
						u8 *cmd,
						u32 cmd_len,
						enum rtw_phl_status status)
{
	if (cmd)
		_os_kmem_free(drv_priv, cmd, cmd_len);
}

static enum rtw_phl_status
_phl_cmd_alloc_stainfo(struct phl_info_t *phl_info,
			struct rtw_phl_stainfo_t **sta,
			u8 *sta_addr,
			struct rtw_wifi_role_t *wrole,
			bool alloc, bool only_hw,
			enum phl_cmd_type cmd_type,
			u32 cmd_timeout)
{
	void *drv = phl_to_drvpriv(phl_info);
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	struct cmd_stainfo_param *param = NULL;
	u32 param_len = 0;

	if (cmd_type == PHL_CMD_DIRECTLY) {
		psts = _phl_alloc_stainfo(phl_info, sta, sta_addr, wrole, alloc, only_hw);
		goto _exit;
	}

	param_len = sizeof(struct cmd_stainfo_param);
	param = _os_kmem_alloc(drv, param_len);
	if (param == NULL) {
		PHL_ERR("%s: alloc param failed!\n", __func__);
		psts = RTW_PHL_STATUS_RESOURCE;
		goto _exit;
	}

	_os_mem_set(drv, param, 0, param_len);
	param->sta = sta;
	_os_mem_cpy(drv, param->sta_addr, sta_addr, MAC_ALEN);
	param->wrole = wrole;
	param->alloc = alloc;
	param->only_hw = only_hw;

	psts = phl_cmd_enqueue(phl_info,
	                       wrole->hw_band,
	                       MSG_EVT_STA_INFO_CTRL,
	                       (u8 *)param,
	                       param_len,
	                       _phl_cmd_alloc_stainfo_done,
	                       cmd_type,
	                       cmd_timeout);

	if (is_cmd_failure(psts)) {
		/* Send cmd success, but wait cmd fail*/
		psts = RTW_PHL_STATUS_FAILURE;
	} else if (psts != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		psts = RTW_PHL_STATUS_FAILURE;
		_os_kmem_free(drv, param, param_len);
	}
_exit:
	return psts;
}

enum rtw_phl_status
phl_cmd_alloc_stainfo_hdl(struct phl_info_t *phl_info, u8 *param)
{
	struct cmd_stainfo_param *cmd_sta_param = (struct cmd_stainfo_param *)param;

	return _phl_alloc_stainfo(phl_info,
					cmd_sta_param->sta,
					cmd_sta_param->sta_addr,
					cmd_sta_param->wrole,
					cmd_sta_param->alloc,
					cmd_sta_param->only_hw);
}

#endif /* CONFIG_CMD_DISP */

enum rtw_phl_status
rtw_phl_cmd_alloc_stainfo(void *phl,
                          struct rtw_phl_stainfo_t **sta,
                          u8 *sta_addr,
                          struct rtw_wifi_role_t *wrole,
                          bool alloc, bool only_hw,
                          enum phl_cmd_type cmd_type,
                          u32 cmd_timeout)
{
#ifdef CONFIG_CMD_DISP
	return _phl_cmd_alloc_stainfo(phl, sta, sta_addr, wrole, alloc, only_hw, cmd_type, cmd_timeout);
#else
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: not support alloc stainfo cmd\n",
				__func__);

	return _phl_alloc_stainfo((struct phl_info_t *)phl, sta, sta_addr, wrole, alloc, only_hw);
#endif /* CONFIG_CMD_DISP */
}

enum rtw_phl_status
phl_wifi_role_free_stainfo_hw(struct phl_info_t *phl_info,
                              struct rtw_wifi_role_t *wrole)
{
	struct macid_ctl_t *mc = phl_to_mac_ctrl(phl_info);
	u16 max_macid_num = mc->max_num;
	struct rtw_phl_stainfo_t *sta = NULL;
	u32 *used_map;
	u16 mid;

	used_map = &mc->wifi_role_usedmap[wrole->id][0];

	for(mid = 0; mid < max_macid_num; mid++) {
		if (_phl_macid_is_used(used_map, mid)) {
			sta = mc->sta[mid];
			if (sta) {
				PHL_INFO("%s [WR-%d] free sta_info(MID:%d)\n",
					__func__, wrole->id, sta->macid);
				phl_free_stainfo_hw(phl_info, sta);
			}
		}
	}
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_wifi_role_free_stainfo_sw(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *role)
{
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	struct stainfo_ctl_t *sta_ctrl = phl_to_sta_ctrl(phl_info);

	PHL_DUMP_STACTRL_EX(phl_info);
	do {
		phl_sta = phl_stainfo_dequeue(phl_info, &role->assoc_sta_queue);

		if (phl_sta) {
#ifdef RTW_WKARD_CHECK_STAINFO_DOUBLE_DEL
			phl_sta->allocated = false;
#endif
			phl_free_stainfo_sw(phl_info, phl_sta);
			phl_stainfo_enqueue(phl_info,
						&sta_ctrl->free_sta_queue, phl_sta);
		}
	} while(phl_sta != NULL);

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_wifi_role_free_stainfo(struct phl_info_t *phl_info,
                           struct rtw_wifi_role_t *role)
{
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	struct stainfo_ctl_t *sta_ctrl = phl_to_sta_ctrl(phl_info);

	PHL_DUMP_STACTRL_EX(phl_info);
	do {
		phl_sta = phl_stainfo_dequeue(phl_info, &role->assoc_sta_queue);

		if (phl_sta) {
#ifdef RTW_WKARD_CHECK_STAINFO_DOUBLE_DEL
			phl_sta->allocated = false;
#endif
			phl_free_stainfo_hw(phl_info, phl_sta);
			phl_free_stainfo_sw(phl_info, phl_sta);
			phl_stainfo_enqueue(phl_info,
			                    &sta_ctrl->free_sta_queue,
			                    phl_sta);
		}
	} while(phl_sta != NULL);

	return RTW_PHL_STATUS_SUCCESS;
}

/**
 * According to 802.11 spec 26.5.2.3.2
 * We shall not transmit HE TB PPDU with RU-26 on DFS channel
 */
static void
_phl_set_dfs_tb_ctrl(struct phl_info_t *phl_info,
		     struct rtw_wifi_role_t *wrole)
{
	struct rtw_regulation_channel reg_ch = {0};
	enum band_type band = wrole->chandef.band;
	u8 channel = wrole->chandef.chan;
	bool is_dfs = false;


	if (rtw_phl_regulation_query_ch(phl_info, band, channel, &reg_ch)) {
		if (reg_ch.property & CH_DFS)
			is_dfs = true;

		rtw_hal_set_dfs_tb_ctrl(phl_info->hal, is_dfs);
	}
}

static void
_phl_no_link_reset_sta_info(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta)
{
	void *drv = phl_to_drvpriv(phl_info);

	/* asoc cap */
	_os_mem_set(drv, &sta->asoc_cap, 0, sizeof(struct protocol_cap_t));

	/* other capabilities under stainfo need to reset with default value */
	sta->tf_trs = 0;

	/* protection mode */
	sta->protect = RTW_PROTECT_DISABLE;
}

/**
 * This function is called once station associated with AP
 * or incoming station got associated under AP mode.
 * Before calling this function, update address / net_type / ...
 * information of stainfo
 * It will configure some hw register, ex
 * address cam
 * @phl: see phl_info_t
 * @stainfo: information is updated through phl_station_info
 */
static enum rtw_phl_status
phl_update_media_status(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta,
			u8 *sta_addr, bool is_connect)
{
	struct rtw_wifi_role_t *wrole = sta->wrole;
	void *drv = phl_to_drvpriv(phl_info);
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	bool is_sta_linked = false;

	is_sta_linked = rtw_hal_is_sta_linked(phl_info->hal, sta);
	if (is_connect == true && is_sta_linked == true) {
		PHL_ERR("%s STA (MAC_ID:%d) had connected\n", __func__, sta->macid);
		goto _exit;
	}
	if (is_connect == false && is_sta_linked == false) {
		/* handle connect abort case */
		if (wrole->mstate == MLME_LINKING) {
			PHL_INFO("%s MAC_ID(%d) connect abort\n", __func__, sta->macid);
			pstatus = RTW_PHL_STATUS_SUCCESS;
		} else {
			PHL_ERR("%s MAC_ID(%d) had disconnected\n", __func__, sta->macid);
		}

		if (wrole->type == PHL_RTYPE_STATION || wrole->type == PHL_RTYPE_P2P_GC)
			wrole->mstate = MLME_NO_LINK;
		goto _exit;
	}

	/* reset trx statistics */
	if (is_connect == false) {
		phl_reset_tx_stats(&sta->stats);
		phl_reset_rx_stats(&sta->stats);
		_phl_no_link_reset_sta_info(phl_info, sta);
		CLEAR_STATUS_FLAG(wrole->status, WR_STATUS_TSF_SYNC);
	} else {
		phl_clean_sta_bcn_info(phl_info, sta);
	}

	/* Configure address cam, including net_type and sync_tsf */
	if ((wrole->type == PHL_RTYPE_STATION) || (wrole->type == PHL_RTYPE_P2P_GC)
	#ifdef CONFIG_PHL_TDLS
		/* STA disconnects with the associated AP before tearing down with TDLS peers */
		|| ((wrole->type == PHL_RTYPE_TDLS) && (!sta_addr))
	#endif
	) {
		if (is_connect) {
			wrole->mstate = MLME_LINKED;
			_os_mem_cpy(drv, sta->mac_addr, sta_addr, MAC_ALEN);
			_phl_set_dfs_tb_ctrl(phl_info, wrole);
		} else {
			wrole->mstate = MLME_NO_LINK;
		}
	}
	#ifdef RTW_WKARD_AP_CLIENT_ADD_DEL_NTY
	else if ((wrole->type == PHL_RTYPE_AP) ||
		  (wrole->type == PHL_RTYPE_VAP) ||
		  (wrole->type == PHL_RTYPE_MESH) ||
		  (wrole->type == PHL_RTYPE_P2P_GO)) {
			if (is_connect)
				phl_role_ap_client_notify(phl_info, wrole, MLME_LINKED);
	}
	#endif
	hstatus = rtw_hal_update_sta_entry(phl_info->hal, sta, is_connect);
	if (hstatus != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("rtw_hal_update_sta_entry failure!\n");
		goto _exit;
	}

	if (wrole->type == PHL_RTYPE_STATION
	#ifdef CONFIG_PHL_TDLS
		/* STA disconnects with the associated AP before tearing down with TDLS peers */
		|| ((wrole->type == PHL_RTYPE_TDLS) && (!sta_addr))
	#endif
	) {
		hstatus = rtw_hal_role_cfg(phl_info->hal, wrole);
		if (hstatus != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("rtw_hal_role_cfg failure!\n");
			goto _exit;
		}
	}

	pstatus = RTW_PHL_STATUS_SUCCESS;

	/* TODO: Configure RCR */
_exit:
	return pstatus;
}

#ifdef CONFIG_CMD_DISP
struct sta_media_param {
	struct rtw_phl_stainfo_t *sta;
	u8 sta_addr[MAC_ALEN];
	bool is_connect;
};

enum rtw_phl_status
phl_update_media_status_hdl(struct phl_info_t *phl_info, u8 *param)
{
	struct sta_media_param *media_sts = (struct sta_media_param *)param;

	return phl_update_media_status(phl_info,
			media_sts->sta, media_sts->sta_addr, media_sts->is_connect);
}

void phl_update_media_status_done(void *drv_priv, u8 *cmd, u32 cmd_len,
						enum rtw_phl_status status)
{
	if (cmd) {
		_os_kmem_free(drv_priv, cmd, cmd_len);
		cmd = NULL;
	}
}
#endif

enum rtw_phl_status
rtw_phl_cmd_update_media_status(void *phl,
                                struct rtw_phl_stainfo_t *sta,
                                u8 *sta_addr,
                                bool is_connect,
                                enum phl_cmd_type cmd_type,
                                u32 cmd_timeout)
{
#ifdef CONFIG_CMD_DISP
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_wifi_role_t *wrole = NULL;
	struct sta_media_param *sta_ms = NULL;
	u32 sta_ms_len = 0;

	if (cmd_type == PHL_CMD_DIRECTLY) {
		psts = phl_update_media_status(phl_info, sta, sta_addr, is_connect);
		goto _exit;
	}

	sta_ms_len = sizeof(struct sta_media_param);
	sta_ms = _os_kmem_alloc(drv, sta_ms_len);
	if (sta_ms == NULL) {
		PHL_ERR("%s: alloc sta media status param failed!\n", __func__);
		psts = RTW_PHL_STATUS_RESOURCE;
		goto _exit;
	}
	_os_mem_set(drv, sta_ms, 0, sta_ms_len);
	sta_ms->sta = sta;
	sta_ms->is_connect = is_connect;
	if (is_connect && sta_addr)
		_os_mem_cpy(drv, sta_ms->sta_addr, sta_addr, MAC_ALEN);

	wrole = sta->wrole;

	psts = phl_cmd_enqueue(phl_info,
	                       wrole->hw_band,
	                       MSG_EVT_STA_MEDIA_STATUS_UPT,
	                       (u8*)sta_ms,
	                       sta_ms_len,
	                       phl_update_media_status_done,
	                       cmd_type,
	                       cmd_timeout);

	if (is_cmd_failure(psts)) {
		/* Send cmd success, but wait cmd fail*/
		psts = RTW_PHL_STATUS_FAILURE;
	} else if (psts != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		psts = RTW_PHL_STATUS_FAILURE;
		_os_kmem_free(drv, sta_ms, sta_ms_len);
	}
_exit:
	return psts;
#else
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: not support cmd to update media status\n",
	          __func__);

	return phl_update_media_status((struct phl_info_t *)phl, sta, sta_addr, is_connect);
#endif
}

/**
 * This function is called once station info changed
 * (BW/NSS/RAMASK/SEC/ROLE/MACADDR........)
 * @phl: see phl_info_t
 * @stainfo: information is updated through phl_station_info
 * @mode: see phl_upd_mode
 */
enum rtw_phl_status
phl_change_stainfo(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta,
			enum phl_upd_mode mode)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;

	hstatus = rtw_hal_change_sta_entry(phl_info->hal, sta, mode);
	if (hstatus != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("rtw_hal_change_sta_entry failure!\n");
		return RTW_PHL_STATUS_FAILURE;
	}
	return RTW_PHL_STATUS_SUCCESS;
}

static enum rtw_phl_status
_change_stainfo(struct phl_info_t *phl_info,
	struct rtw_phl_stainfo_t *sta, enum sta_chg_id chg_id, u8 *chg_info, u8 chg_info_len)
{
	enum phl_upd_mode mode = PHL_UPD_STA_INFO_CHANGE;

	switch (chg_id) {
	case STA_CHG_BW:
	case STA_CHG_NSS:
	case STA_CHG_RAMASK:
	{
		PHL_INFO("%s MACID:%d %02x:%02x:%02x:%02x:%02x:%02x update bw\n",
		         __func__, sta->macid,
		         sta->mac_addr[0], sta->mac_addr[1], sta->mac_addr[2],
		         sta->mac_addr[3], sta->mac_addr[4], sta->mac_addr[5]);
	}
		break;
	case STA_CHG_SEC_MODE:
		sta->sec_mode = *((u8*)chg_info);
		break;
	case STA_CHG_MBSSID:
		sta->addr_sel = 1;
		sta->addr_msk = *((u8*)chg_info);
		break;
	case STA_CHG_RA_GILTF:
		sta->hal_sta->ra_info.cal_giltf = *((u8*)chg_info);
		sta->hal_sta->ra_info.fix_giltf_en = true;
		PHL_INFO("%s: Config RA GI LTF = %d\n", __FUNCTION__, *((u8*)chg_info));
		break;
	case STA_CHG_MAX:
		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "rtw_phl_change_stainfo(): Unsupported case:%d, please check it\n",
				chg_id);
		break;
	default:
		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "rtw_phl_change_stainfo(): Unrecognize case:%d, please check it\n",
				chg_id);
		break;
	}

	return phl_change_stainfo(phl_info, sta, mode);
}

#ifdef CONFIG_CMD_DISP
struct sta_chg_param {
	struct rtw_phl_stainfo_t *sta;
	enum sta_chg_id id;
	u8 *info;
	u8 info_len;
};

enum rtw_phl_status
phl_cmd_change_stainfo_hdl(struct phl_info_t *phl_info, u8 *param)
{
	struct sta_chg_param *sta_param = (struct sta_chg_param *)param;

	return _change_stainfo(phl_info,
			sta_param->sta, sta_param->id,
			sta_param->info, sta_param->info_len);
}

static void
_phl_cmd_change_stainfo_done(void *drv_priv, u8 *cmd, u32 cmd_len,
						enum rtw_phl_status status)
{
	struct sta_chg_param *sta_chg_info = NULL;

	if (cmd == NULL || cmd_len == 0) {
		PHL_ERR("%s buf == NULL || buf_len == 0\n", __func__);
		_os_warn_on(1);
		return;
	}

	sta_chg_info = (struct sta_chg_param *)cmd;
	PHL_INFO("%s - id:%d .....\n", __func__, sta_chg_info->id);

	if (sta_chg_info->info && sta_chg_info->info_len > 0)
		_os_kmem_free(drv_priv, sta_chg_info->info, sta_chg_info->info_len);

	_os_kmem_free(drv_priv, cmd, cmd_len);
	cmd = NULL;
}

static enum rtw_phl_status
_phl_cmd_change_stainfo(struct phl_info_t *phl_info,
	struct rtw_phl_stainfo_t *sta, enum sta_chg_id chg_id,
	u8 *chg_info, u8 chg_info_len,
	enum phl_cmd_type cmd_type, u32 cmd_timeout)
{
	void *drv = phl_to_drvpriv(phl_info);
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	struct rtw_wifi_role_t *wrole = sta->wrole;
	struct sta_chg_param *param = NULL;
	u8 param_len = 0;

	if (cmd_type == PHL_CMD_DIRECTLY) {
		psts = _change_stainfo(phl_info, sta, chg_id, chg_info, chg_info_len);
		goto _exit;
	}

	param_len = sizeof(struct sta_chg_param);
	param = _os_kmem_alloc(drv, param_len);
	if (param == NULL) {
		PHL_ERR("%s: alloc param failed!\n", __func__);
		psts = RTW_PHL_STATUS_RESOURCE;
		goto _exit;
	}

	_os_mem_set(drv, param, 0, param_len);
	param->sta = sta;
	param->id = chg_id;
	param->info_len = chg_info_len;

	if (chg_info_len > 0) {
		param->info = _os_kmem_alloc(drv, chg_info_len);
		if (param->info == NULL) {
			PHL_ERR("%s: alloc param->info failed!\n", __func__);
			psts = RTW_PHL_STATUS_RESOURCE;
			goto _err_info;
		}

		_os_mem_set(drv, param->info, 0, chg_info_len);
		_os_mem_cpy(drv, param->info, chg_info, chg_info_len);
	} else {
		param->info = NULL;
	}

	psts = phl_cmd_enqueue(phl_info,
	                       wrole->hw_band,
	                       MSG_EVT_STA_CHG_STAINFO,
	                       (u8 *)param,
	                       param_len,
	                       _phl_cmd_change_stainfo_done,
	                       cmd_type,
	                       cmd_timeout);

	if (is_cmd_failure(psts)) {
		/* Send cmd success, but wait cmd fail*/
		psts = RTW_PHL_STATUS_FAILURE;
	} else if (psts != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		psts = RTW_PHL_STATUS_FAILURE;
		goto _err_cmd;
	}

	return psts;

_err_cmd:
	if (param->info)
		_os_kmem_free(drv, param->info, param->info_len);
_err_info:
	if (param)
		_os_kmem_free(drv, param, param_len);
_exit:
	return psts;
}
#endif

enum rtw_phl_status
rtw_phl_cmd_change_stainfo(void *phl,
	struct rtw_phl_stainfo_t *sta, enum sta_chg_id chg_id,
	u8 *chg_info, u8 chg_info_len,
	enum phl_cmd_type cmd_type, u32 cmd_timeout)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

#ifdef CONFIG_CMD_DISP
	return _phl_cmd_change_stainfo(phl_info, sta, chg_id, chg_info, chg_info_len,
		cmd_type, cmd_timeout);
#else
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: not support alloc stainfo cmd\n",
				__func__);

	return _change_stainfo(phl_info, sta, chg_id, chg_info, chg_info_len);
#endif /* CONFIG_CMD_DISP */
}
/**
 * This function updates tx/rx traffic status of each active station info
 */
void
phl_sta_trx_tfc_upd(struct phl_info_t *phl_info)
{
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	struct rtw_stats *sta_stats = NULL;
	u16 max_macid_num = 0;
	u16 mid = 0;

	max_macid_num = macid_ctl->max_num;

	_os_spinlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);
	for(mid = 0; mid < max_macid_num; mid++) {
		if (_phl_macid_is_used(macid_ctl->used_map, mid)) {
			phl_sta = macid_ctl->sta[mid];
			if (phl_sta) {
				#ifdef CONFIG_PHL_RA_TXSTS_DBG
				/* issue H2C to get ra txsts report */
				rtw_phl_txsts_rpt_config(phl_info, phl_sta);
				#endif
				sta_stats = &phl_sta->stats;
				phl_tx_traffic_upd(sta_stats);
				phl_rx_traffic_upd(sta_stats);
			}
		}
	}
	_os_spinunlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);
}


/**
 * This function is used to get phl sta info
 * by macid
 * @phl: see phl_info_t
 * @macid: macid
 */
struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_by_macid(void *phl, u16 macid)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);
	struct rtw_phl_stainfo_t *phl_sta = NULL;

	if (macid >= macid_ctl->max_num) {
		PHL_ERR("%s macid(%d) is invalid\n", __func__, macid);
		return NULL;
	}
	_os_spinlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);
	if (_phl_macid_is_used(macid_ctl->used_map, macid))
		phl_sta = macid_ctl->sta[macid];

	if (phl_sta == NULL) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_,"%s sta info (macid:%d) is NULL\n", __func__, macid);
		#ifdef CONFIG_PHL_RELEASE_RPT_ENABLE
		/* comment temporarily since release report may report unused macid */
		/* and trigger call tracing */
		/* _os_warn_on(1); */
		#else
		_os_warn_on(1);
		#endif /* CONFIG_PHL_RELEASE_RPT_ENABLE */
	}
	_os_spinunlock(phl_to_drvpriv(phl_info), &macid_ctl->lock, _bh, NULL);

	return phl_sta;
}

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_by_addr_ex(void *phl, u8 *addr)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *mc = phl_to_mac_ctrl(phl_info);
	struct rtw_phl_stainfo_t *sta = NULL;
	u16 mid = 0;
	u16 max_macid_num = mc->max_num;
	bool sta_found = false;

	_os_spinlock(phl_to_drvpriv(phl_info), &mc->lock, _bh, NULL);
	for(mid = 0; mid < max_macid_num; mid++) {
		if (_phl_macid_is_used(mc->used_map, mid)) {
			sta = mc->sta[mid];
			if (_os_mem_cmp(phl_to_drvpriv(phl_info),
				sta->mac_addr, addr, MAC_ALEN) == 0) {
				sta_found = true;
				break;
			}
		}
	}
	_os_spinunlock(phl_to_drvpriv(phl_info), &mc->lock, _bh, NULL);

	if (sta_found == false)
		sta = NULL;
	return sta;
}

u16 rtw_phl_get_macid_by_addr(void *phl, u8 *addr)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *mc = phl_to_mac_ctrl(phl_info);
	struct rtw_phl_stainfo_t *sta = NULL;

	sta = rtw_phl_get_stainfo_by_addr_ex(phl, addr);
	if (sta)
		return sta->macid;
	return mc->max_num;
}

/**
 * This function is called to create phl_station_info
 * return pointer to rtw_phl_stainfo_t
 * @phl: see phl_info_t
 * @roleidx: index of wifi role(linux) port nubmer(windows)
 * @addr: current address of this station
 */
struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_by_addr(void *phl, struct rtw_wifi_role_t *wrole, u8 *addr)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);
	struct rtw_phl_stainfo_t *sta = NULL;

	if (is_broadcast_mac_addr(addr)) {
		u16 macid = macid_ctl->wrole_bmc[wrole->id];

		if (macid >= macid_ctl->max_num)
			sta = NULL;
		else
			sta = macid_ctl->sta[macid];
		goto _exit;
	}

	sta = phl_stainfo_queue_search(phl_info,
			 &wrole->assoc_sta_queue, addr);
_exit:
	return sta;
}

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_self(void *phl, struct rtw_wifi_role_t *wrole)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_stainfo_t *sta = NULL;

	#if 0
	if ((wrole->type == PHL_RTYPE_STATION) &&
		(wrole->mstate == MLME_LINKED))
			//????
		else
			sta = phl_stainfo_queue_search(phl_info,
				&wrole->assoc_sta_queue, wrole->mac_addr);
	}
	#else
	sta = phl_stainfo_queue_get_first(phl_info, &wrole->assoc_sta_queue);
	if (sta == NULL)
		PHL_ERR("%s sta == NULL\n", __func__);
	#endif
	return sta;
}

u8
rtw_phl_get_sta_rssi(struct rtw_phl_stainfo_t *sta)
{
	u8 rssi = rtw_hal_get_sta_rssi(sta);

	return rssi;
}

u8 phl_get_min_rssi_bcn(struct phl_info_t *phl_info)
{
	struct macid_ctl_t *macid_ctl = phl_to_mac_ctrl(phl_info);
	struct rtw_phl_stainfo_t *sta = NULL;
	u8 rssi_bcn_min = 0xFF;
	u16 i = 0;
	u8 rssi = 0;

	for (i = 0; i < macid_ctl->max_num; i++) {
		if (!_phl_macid_is_used(macid_ctl->used_map, i))
			continue;

		sta = rtw_phl_get_stainfo_by_macid(phl_info, i);

		if (NULL == sta)
			continue;

		rssi = rtw_hal_get_sta_rssi_bcn(sta);

		PHL_DBG("%s macid(%d) with rssi_bcn = %d\n",
			__func__, i, rssi);

		if (rssi == 0)
			continue;

		rssi_bcn_min = MIN(rssi, rssi_bcn_min);
	}

	return rssi_bcn_min;
}


enum rtw_phl_status
rtw_phl_query_rainfo(void *phl, struct rtw_phl_stainfo_t *phl_sta,
		     struct rtw_phl_rainfo *ra_info)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_FAILURE;

	do {
		if (NULL == phl_sta) {
			PHL_TRACE(COMP_PHL_XMIT, _PHL_ERR_,
				  "%s : phl_sta is NULL\n",
				  __func__);
			break;
		}

		if (NULL == ra_info) {
			PHL_TRACE(COMP_PHL_XMIT, _PHL_ERR_,
				  "%s : Input parameter is NULL\n",
				  __func__);
			break;
		}

		if (RTW_HAL_STATUS_SUCCESS ==
		    rtw_hal_query_rainfo(phl_info->hal, phl_sta->hal_sta,
					 ra_info)) {
			phl_sts = RTW_PHL_STATUS_SUCCESS;
			break;
		} else {
			break;
		}
	} while (false);

	return phl_sts;
}

enum rtw_phl_status
rtw_phl_get_rx_stat(void *phl, struct rtw_phl_stainfo_t *phl_sta,
		     u16 *rx_rate, u8 *bw, u8 *gi_ltf)
{
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_stainfo_t *hal_sta;

	if(phl_sta) {
		hal_sta = phl_sta->hal_sta;
		*rx_rate = hal_sta->trx_stat.rx_rate;
		*gi_ltf = hal_sta->trx_stat.rx_gi_ltf;
		*bw = hal_sta->trx_stat.rx_bw;
		phl_sts = RTW_PHL_STATUS_SUCCESS;
	}

	return phl_sts;
}

/**
 * rtw_phl_txsts_rpt_config() - issue h2c for txok and tx retry info
 * @phl:		struct phl_info_t *
 * @phl_sta:		indicate the first macid that you want to query.
 * Return rtw_phl_txsts_rpt_config's return value in enum rtw_phl_status type.
 */
enum rtw_phl_status
rtw_phl_txsts_rpt_config(void *phl, struct rtw_phl_stainfo_t *phl_sta)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_FAILURE;

	if (phl_sta) {
		if (RTW_HAL_STATUS_SUCCESS == rtw_hal_query_txsts_rpt(phl_info->hal, phl_sta->macid))
			phl_sts = RTW_PHL_STATUS_SUCCESS;
	}
	return phl_sts;
}

#if defined(CONFIG_USB_HCI) || defined(CONFIG_PCI_HCI)
/**
 * rtw_phl_get_tx_ok_rpt() - get txok info.
 * @phl:		struct phl_info_t *
 * @phl_sta:		information is updated through phl_station_info.
 * @tx_ok_cnt:		buffer address that we used to store tx ok statistics.
 * @qsel			indicate which AC queue, or fetch all by PHL_AC_QUEUE_TOTAL
 *
 * Return rtw_phl_get_tx_ok_rpt's return value in enum rtw_phl_status type.
 */
enum rtw_phl_status
rtw_phl_get_tx_ok_rpt(void *phl, struct rtw_phl_stainfo_t *phl_sta, u32 *tx_ok_cnt,
 enum phl_ac_queue qsel)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_SUCCESS;
	struct rtw_hal_stainfo_t *hal_sta;

	if(phl_sta) {
		hal_sta = phl_sta->hal_sta;

		if (tx_ok_cnt && qsel <= PHL_AC_QUEUE_TOTAL) {
			if (qsel == PHL_AC_QUEUE_TOTAL) {
				/* copy all AC counter */
				tx_ok_cnt[PHL_BE_QUEUE_SEL] = hal_sta->trx_stat.wp_rpt_stats[PHL_BE_QUEUE_SEL].tx_ok_cnt;
				tx_ok_cnt[PHL_BK_QUEUE_SEL] = hal_sta->trx_stat.wp_rpt_stats[PHL_BK_QUEUE_SEL].tx_ok_cnt;
				tx_ok_cnt[PHL_VI_QUEUE_SEL] = hal_sta->trx_stat.wp_rpt_stats[PHL_VI_QUEUE_SEL].tx_ok_cnt;
				tx_ok_cnt[PHL_VO_QUEUE_SEL] = hal_sta->trx_stat.wp_rpt_stats[PHL_VO_QUEUE_SEL].tx_ok_cnt;

				/* reset all counter */
				_os_spinlock(phl_to_drvpriv(phl_info), &hal_sta->trx_stat.tx_sts_lock, _bh, NULL);
				hal_sta->trx_stat.wp_rpt_stats[PHL_BE_QUEUE_SEL].tx_ok_cnt = 0;
				hal_sta->trx_stat.wp_rpt_stats[PHL_BK_QUEUE_SEL].tx_ok_cnt = 0;
				hal_sta->trx_stat.wp_rpt_stats[PHL_VI_QUEUE_SEL].tx_ok_cnt = 0;
				hal_sta->trx_stat.wp_rpt_stats[PHL_VO_QUEUE_SEL].tx_ok_cnt = 0;
				_os_spinunlock(phl_to_drvpriv(phl_info), &hal_sta->trx_stat.tx_sts_lock, _bh, NULL);
			} else {
				/*copy target AC queue counter*/
				*tx_ok_cnt = hal_sta->trx_stat.wp_rpt_stats[qsel].tx_ok_cnt;
				/* reset target AC queue counter */
				_os_spinlock(phl_to_drvpriv(phl_info), &hal_sta->trx_stat.tx_sts_lock, _bh, NULL);
				hal_sta->trx_stat.wp_rpt_stats[qsel].tx_ok_cnt = 0;
				_os_spinunlock(phl_to_drvpriv(phl_info), &hal_sta->trx_stat.tx_sts_lock, _bh, NULL);
			}
		} else {
			phl_sts = RTW_PHL_STATUS_FAILURE;
			PHL_ERR("tx_ok_cnt = %p, qsel = %d\n", tx_ok_cnt, qsel);
		}

	} else {
		phl_sts = RTW_PHL_STATUS_FAILURE;
		PHL_ERR("PHL STA NULL.\n");
	}
	return phl_sts;
}

static u32 rtw_phl_get_hw_tx_fail_cnt(struct rtw_hal_stainfo_t *hal_sta,
	enum phl_ac_queue qsel) {
	u32 total = 0;

	if (hal_sta) {
		total = hal_sta->trx_stat.wp_rpt_stats[qsel].rty_fail_cnt\
				+ hal_sta->trx_stat.wp_rpt_stats[qsel].lifetime_drop_cnt \
				+ hal_sta->trx_stat.wp_rpt_stats[qsel].macid_drop_cnt;
	}

	return total;
}

static void rtw_phl_reset_tx_fail_cnt(struct phl_info_t *phl_info,
	struct rtw_hal_stainfo_t *hal_sta, enum phl_ac_queue qsel) {

	if (hal_sta) {
		_os_spinlock(phl_to_drvpriv(phl_info), &hal_sta->trx_stat.tx_sts_lock, _bh, NULL);
		hal_sta->trx_stat.wp_rpt_stats[qsel].rty_fail_cnt = 0;
		hal_sta->trx_stat.wp_rpt_stats[qsel].lifetime_drop_cnt = 0;
		hal_sta->trx_stat.wp_rpt_stats[qsel].macid_drop_cnt = 0;
		_os_spinunlock(phl_to_drvpriv(phl_info), &hal_sta->trx_stat.tx_sts_lock, _bh, NULL);
	}
}

/**
 * rtw_phl_get_tx_fail_rpt() - get tx fail info.
 * @phl:		struct phl_info_t *
 * @phl_sta:		information is updated through phl_station_info.
 * @tx_fail_cnt:	buffer address that we used to store tx fail statistics.
 * @qsel			indicate which AC queue, or fetch all by PHL_AC_QUEUE_TOTAL
 *
 * Return rtw_phl_get_tx_fail_rpt's return value in enum rtw_phl_status type.
 */
enum rtw_phl_status
rtw_phl_get_tx_fail_rpt(void *phl, struct rtw_phl_stainfo_t *phl_sta, u32 *tx_fail_cnt,
 enum phl_ac_queue qsel)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_SUCCESS;
	struct rtw_hal_stainfo_t *hal_sta;

	if(phl_sta) {
		hal_sta = phl_sta->hal_sta;

		if (tx_fail_cnt && qsel <= PHL_AC_QUEUE_TOTAL) {
			if (qsel == PHL_AC_QUEUE_TOTAL) {
				/* copy all AC counter */
				tx_fail_cnt[PHL_BE_QUEUE_SEL] = rtw_phl_get_hw_tx_fail_cnt(hal_sta, PHL_BE_QUEUE_SEL);
				tx_fail_cnt[PHL_BK_QUEUE_SEL] = rtw_phl_get_hw_tx_fail_cnt(hal_sta, PHL_BK_QUEUE_SEL);
				tx_fail_cnt[PHL_VI_QUEUE_SEL] = rtw_phl_get_hw_tx_fail_cnt(hal_sta, PHL_VI_QUEUE_SEL);
				tx_fail_cnt[PHL_VO_QUEUE_SEL] = rtw_phl_get_hw_tx_fail_cnt(hal_sta, PHL_VO_QUEUE_SEL);
				/* reset all counter */
				rtw_phl_reset_tx_fail_cnt(phl_info, hal_sta, PHL_BE_QUEUE_SEL);
				rtw_phl_reset_tx_fail_cnt(phl_info, hal_sta, PHL_BK_QUEUE_SEL);
				rtw_phl_reset_tx_fail_cnt(phl_info, hal_sta, PHL_VI_QUEUE_SEL);
				rtw_phl_reset_tx_fail_cnt(phl_info, hal_sta, PHL_VO_QUEUE_SEL);
			} else {
				/*copy target AC queue counter*/
				tx_fail_cnt[qsel] = rtw_phl_get_hw_tx_fail_cnt(hal_sta, qsel);
				/* reset target AC queue counter */
				rtw_phl_reset_tx_fail_cnt(phl_info, hal_sta, qsel);
			}
		} else {
			phl_sts = RTW_PHL_STATUS_FAILURE;
			PHL_ERR("tx_fail_cnt = %p, qsel = %d\n", tx_fail_cnt, qsel);
		}
	} else {
		phl_sts = RTW_PHL_STATUS_FAILURE;
		PHL_ERR("PHL STA NULL.\n");
	}
	return phl_sts;
}

/**
 * rtw_phl_get_tx_retry_rpt() - get tx retry info.
 * @phl:		struct phl_info_t *
 * @phl_sta:		information is updated through phl_station_info.
 * @tx_retry_cnt:	buffer address that we used to store tx fail statistics.
 * @qsel			indicate which AC queue, or fetch all by PHL_AC_QUEUE_TOTAL
 *
 * Return rtw_phl_get_tx_retry_rpt's return value in enum rtw_phl_status type.
 */
enum rtw_phl_status
rtw_phl_get_tx_retry_rpt(void *phl, struct rtw_phl_stainfo_t *phl_sta, u32 *tx_retry_cnt,
 enum phl_ac_queue qsel)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	void *drv = phl_to_drvpriv(phl_info);
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_SUCCESS;
	struct rtw_hal_stainfo_t *hal_sta;

	if(phl_sta) {
		hal_sta = phl_sta->hal_sta;

		if (tx_retry_cnt && qsel <= PHL_AC_QUEUE_TOTAL) {
			if (qsel == PHL_AC_QUEUE_TOTAL) {
				/* copy all AC counter */
				_os_mem_cpy(drv, tx_retry_cnt, hal_sta->ra_info.tx_retry_cnt,
					sizeof(u32)*PHL_AC_QUEUE_TOTAL);
				/* reset all counter */
				/* TODO: Here needs lock, and so does halbb_get_txsts_rpt */
				_os_mem_set(drv, hal_sta->ra_info.tx_retry_cnt, 0, sizeof(u32)*PHL_AC_QUEUE_TOTAL);
			} else {
				/*copy target AC queue counter*/
				*tx_retry_cnt = hal_sta->ra_info.tx_retry_cnt[qsel];
				/* reset target AC queue counter */
				/* TODO: Here needs lock, and so does halbb_get_txsts_rpt */
				hal_sta->ra_info.tx_retry_cnt[qsel] = 0;
			}
		} else {
			phl_sts = RTW_PHL_STATUS_FAILURE;
			PHL_ERR("tx_retry_cnt = %p, qsel = %d\n", tx_retry_cnt, qsel);
		}
	} else {
		phl_sts = RTW_PHL_STATUS_FAILURE;
		PHL_ERR("PHL STA NULL.\n");
	}
	return phl_sts;
}
#endif /* defined(CONFIG_USB_HCI) || defined(CONFIG_PCI_HCI) */

/*
 * Get next idx
 */
u8 _get_fidx(u8 num, u8 cur_idx)
{
	u8 idx = 0;

	if (num == 0)
		idx = cur_idx;
	else {
		idx = cur_idx + 1;
		if (idx >= MAX_STORE_BCN_NUM)
			idx = 0;
	}
	return idx;
}

/*
 * Get previous idx
 */
u8 _get_bidx(u8 num, u8 cur_idx)
{
	u8 idx = 0;

	if (cur_idx == 0) {
		idx = num - 1;
	} else {
		idx = cur_idx - 1;
	}
	return idx;
}

void _phl_sta_up_bcn_offset_info(struct phl_info_t *phl,
			struct rtw_rx_bcn_info *bcn_i, u16 bcn_intvl)
{
	struct rtw_bcn_offset *offset_i = &bcn_i->offset_i;
	u16 offset = bcn_intvl;
	u16 similar_th = 2;/*Unit: TU*/
	u64 diff = 0;
	u8 idx = 0, jdx = 0, cur_idx = 0, bidx = 0, start_idx = 0;

	if (bcn_i->num == 1) {
		offset_i->offset = (u16)bcn_i->info[1][bcn_i->idx];
		offset_i->conf_lvl = CONF_LVL_LOW;
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "_phl_sta_up_bcn_offset_info(): bcn_i->num ==1, conf_lvl = CONF_LVL_LOW, offset(%d)\n",
			offset_i->offset);
		goto exit;
	}
	cur_idx = bcn_i->idx;
	start_idx = cur_idx;
	for (idx = 0; idx < bcn_i->num; idx++) {
		bidx = cur_idx;
		for (jdx = 1; jdx < bcn_i->num; jdx++) {
			bidx = _get_bidx(bcn_i->num, bidx);
			if (start_idx == bidx)
				break;
			diff = bcn_i->info[0][cur_idx] - bcn_i->info[0][bidx];
			diff = _os_division64(
					_os_modular64(diff, bcn_intvl * TU), TU);
			/*ex: diff = 99, BcnIntvl = 100, It's similar case
			 * diff = 2, BcnIntvl = 100, It's similar case
			 */
			if (!((diff < similar_th) ||
				((bcn_intvl - diff) < similar_th))) {
					continue;
			}
			if (offset > bcn_i->info[1][cur_idx])
				offset = (u16)bcn_i->info[1][cur_idx];
			if (offset > bcn_i->info[1][bidx])
				offset = (u16)bcn_i->info[1][bidx];
		}
		cur_idx = _get_bidx(bcn_i->num, cur_idx);
	}
	if (offset != bcn_intvl) {
		offset_i->conf_lvl = CONF_LVL_HIGH;
		if (offset < offset_i->offset) {
			offset_i->offset = offset;
		}
		goto exit;
	}
	for (idx = 0; idx < bcn_i->num; idx++) {
		if (bcn_i->info[1][idx] < offset_i->offset) {
			offset_i->offset = (u16)bcn_i->info[1][idx];
			offset_i->conf_lvl = CONF_LVL_MID;
		}
	}
exit:
	/*
	   if offset is small, maybe impact by environment, offset < 5% bcn_intvl, we consider offset is 0
	*/
	if ((offset_i->offset != 0) &&
		(offset_i->offset < ((bcn_intvl * 5) / 100))) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "_phl_sta_up_bcn_offset_info(): offset(%d) < (%d), set offset = 0\n",
			offset_i->offset, (bcn_intvl * 5) / 100);
		offset_i->offset = 0;
	}
	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "_phl_sta_up_bcn_offset_info(): bcn num(%d), offset(%d), conf_lvl(%d), current CR(%d)\n",
		bcn_i->num, offset_i->offset, offset_i->conf_lvl, offset_i->cr_tbtt_shift);
	return;
}

void rtw_phl_sta_up_rx_bcn(void *phl, struct rtw_bcn_pkt_info *info)
{
	struct rtw_rx_bcn_info *bcn_i = &info->sta->bcn_i;
	u16 bcn_intvl = info->sta->asoc_cap.bcn_interval;

	if (bcn_intvl == 0) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "bcn_intvl == 0");
		return;
	}

	bcn_i->idx = _get_fidx(bcn_i->num, bcn_i->idx);
	if (bcn_i->num < MAX_STORE_BCN_NUM)
		bcn_i->num++;
	bcn_i->info[0][bcn_i->idx] = info->tsf;
	bcn_i->info[1][bcn_i->idx] = _os_division64(
				_os_modular64(info->tsf, bcn_intvl * TU), TU);
	bcn_i->info[2][bcn_i->idx] = info->hw_tsf;
	_phl_sta_up_bcn_offset_info(phl, bcn_i, bcn_intvl);
}

void phl_clean_sta_bcn_info(struct phl_info_t *phl, struct rtw_phl_stainfo_t *sta)
{
	void *priv = phl_to_drvpriv(phl);
	struct rtw_rx_bcn_info *bcn_i = &sta->bcn_i;

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "phl_clean_sta_bcn_info(): sta->wrole->id(%d)\n",
		sta->wrole->id);
	_os_mem_set(priv, bcn_i, 0, sizeof(struct rtw_rx_bcn_info));
}

struct rtw_bcn_offset * phl_get_sta_bcn_offset_info(struct phl_info_t *phl,
					struct rtw_wifi_role_t *wrole)
{
	struct rtw_phl_stainfo_t *sta = rtw_phl_get_stainfo_self(phl, wrole);
	struct rtw_bcn_offset *offset_i = &sta->bcn_i.offset_i;

	return offset_i;
}

void phl_bcn_watchdog(struct phl_info_t *phl)
{
	u8 ridx = MAX_WIFI_ROLE_NUMBER;
	struct rtw_wifi_role_t *wrole = NULL;
	struct rtw_bcn_offset *b_ofst_i = NULL;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		wrole = rtw_phl_get_wrole_by_ridx(phl->phl_com, ridx);
		if (wrole->active == false)
			continue;

		if (rtw_phl_role_is_client_category(wrole) && wrole->mstate == MLME_LINKED) {
			b_ofst_i = phl_get_sta_bcn_offset_info(phl, wrole);

			if (b_ofst_i->conf_lvl >= CONF_LVL_MID &&
				b_ofst_i->offset != b_ofst_i->cr_tbtt_shift) {
				PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s(): update bcn offset to %d TU\n",
							__func__, b_ofst_i->offset);
				hstatus = rtw_hal_role_cfg_ex(phl->hal, wrole, PCFG_TBTT_SHIFT, &(b_ofst_i->offset));
				if (hstatus == RTW_HAL_STATUS_SUCCESS)
					b_ofst_i->cr_tbtt_shift = b_ofst_i->offset;
				else
					PHL_ERR("%s(): role cfg fail, status: %d\n", __func__, hstatus);
			}
		}
	}
}

/*
 * calculate the value between current TSF and TBTT
 * TSF   0       50    180    150              250
 * TBTT          ^                ^                ^
 * Curr T                 |
 *                   | 30 |
 *
 * TSF   0       80     120     180              280
 * TBTT           ^                 ^                 ^
 * Curr T                  |
 *                   | 40  |
 * @wrole: specific role, we get bcn offset info from the role.
 * @cur_t: current TSF
 * @ofst: output value, unit: TU
 */
bool phl_calc_offset_from_tbtt(struct phl_info_t *phl,
			struct rtw_wifi_role_t *wrole, u64 cur_t, u16 *ofst)
{
	struct rtw_bcn_offset *b_ofst_i = phl_get_sta_bcn_offset_info(phl, wrole);
	struct rtw_phl_stainfo_t *sta = rtw_phl_get_stainfo_self(phl, wrole);
	u64 b_ofst = b_ofst_i->offset;
	u64 b_intvl = 0;
	u32 mod = 0; /*TU*/

#ifdef RTW_PHL_BCN
	if (phl_role_is_ap_category(wrole))
		b_intvl = (u16)wrole->bcn_cmn.bcn_interval;
	else
#endif
		b_intvl = sta->asoc_cap.bcn_interval;
	if (0 == b_intvl) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "phl_calc_offset_from_tbtt(): Fail, b_intvl ==0, wrole->id(%d), type(%d)\n",
			wrole->id, wrole->type);
		return false;
	}
	mod = (u32)_os_division64(_os_modular64(cur_t, b_intvl * TU), TU);
	if (mod < b_ofst) {
		*ofst = (u16)(mod + (b_intvl - b_ofst));
	} else {
		*ofst = (u16)(mod - b_ofst);
	}
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "phl_calc_offset_from_tbtt(): wrole->id(%d), ofst(%d), cur_t: 0x%08x %08x modular(%d, TU), Bcn offset: conf_lvl(%d), offset(%d)\n",
		wrole->id, *ofst, (u32)(cur_t >> 32), (u32)cur_t, mod,
		b_ofst_i->conf_lvl, (u32)b_ofst);
	return true;
}

/*
 * Synchronize TBTT of target role with TBTT of sourec role
 * Assume TBTT of target role is locate in Mod(Tgt Tsf) = 0
 * @sync_ofst: Offset between TBTT of target role and TBTT of sourec role. Unit: TU
 * @sync_now_once: Sync once time right now.
 * @*diff_t : output diff_tsf. Unit: TU
 */
enum rtw_phl_status rtw_phl_tbtt_sync(struct phl_info_t *phl,
		struct rtw_wifi_role_t *src_role,
		struct rtw_wifi_role_t *tgt_role,
		u16 sync_ofst, bool sync_now_once, u16 *diff_t)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	u32 tsf_h = 0, tsf_l = 0;
	u64 tsf = 0, tgt_tsf = 0, bcn_intvl = 0;
	u16 ofst = 0;
	u64 diff_tsf = 0;
	enum hal_tsf_sync_act act = sync_now_once ? HAL_TSF_SYNC_NOW_ONCE :
							HAL_TSF_EN_SYNC_AUTO;

	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_get_tsf(phl->hal,
					src_role->hw_port, &tsf_h, &tsf_l)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "rtw_phl_tbtt_sync(): Get tsf fail, src_role->id(%d)\n",
			src_role->id);
		goto exit;
	}
	bcn_intvl = phl_role_get_bcn_intvl(phl, tgt_role);
	if (bcn_intvl == 0) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "rtw_phl_tbtt_sync(): bcn_intvl == 0, tgt_role->id(%d)\n",
			tgt_role->id);
		goto exit;
	}
	tsf = tsf_h;
	tsf = tsf << 32;
	tsf |= tsf_l;
	/*calculate the value between current TSF and TBTT*/
	phl_calc_offset_from_tbtt(phl, src_role, tsf, &ofst);
	tgt_tsf = (tsf + sync_ofst * TU) - ofst * TU;
	/*Find diff_tsf, let Mod((tgt_tsf + diff_tsf), bcn_intvl) = 0*/
	diff_tsf = bcn_intvl * TU - _os_modular64(tgt_tsf, bcn_intvl * TU);
	diff_tsf = _os_division64(diff_tsf, TU);
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "rtw_phl_tbtt_sync(): diff_tsf(%d), sync_ofst(%d), ofst(%d)\n",
		(u32)diff_tsf, sync_ofst, (u32)ofst);
	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_tsf_sync(phl->hal,
					src_role->hw_port, tgt_role->hw_port,
					src_role->hw_band, (s32)diff_tsf,
					act)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "rtw_phl_tbtt_sync(): Sync tsf fail\n");
		goto exit;
	}
	if (RTW_HAL_STATUS_SUCCESS == rtw_hal_get_tsf(phl->hal,
					src_role->hw_port, &tsf_h, &tsf_l)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "rtw_phl_tbtt_sync(): tsf_src(0x%08x %08x)\n",
			tsf_h, tsf_l);
	}
	if (RTW_HAL_STATUS_SUCCESS == rtw_hal_get_tsf(phl->hal,
					tgt_role->hw_port, &tsf_h, &tsf_l)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "rtw_phl_tbtt_sync(): tsf_tgt(0x%08x %08x)\n",
			tsf_h, tsf_l);
	}
	*diff_t = (u16)diff_tsf;
	status = RTW_PHL_STATUS_SUCCESS;
exit:
	return status;

}

bool phl_self_stainfo_chk(struct phl_info_t *phl_info,
                          struct rtw_wifi_role_t *wrole,
                          struct rtw_phl_stainfo_t *sta)
{
	return _phl_self_stainfo_chk(phl_info, wrole, sta);
}
