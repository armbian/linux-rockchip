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
#define _HAL_API_BTC_C_
#include "hal_headers.h"
#include "btc/hal_btc.h"

#ifdef CONFIG_BTCOEX
/*******************************************
 * C2H FW message
 *******************************************/
#define BTC_FWBUF_NUM 4
struct fw_msg_entry {
	_os_list list;
	u8 c2h_class;
	u8 c2h_func;
	u16 len;
	u8 buf[RTW_PHL_BTC_FWINFO_BUF];
};

static void _bt_msg_init(struct rtw_hal_com_t *hal_com,
				struct hal_bt_msg *msg)
{
	void *d = halcom_to_drvpriv(hal_com);

	_os_spinlock_init(d, &msg->lock);
	_os_mem_set(d, &msg->latest[0], 0, RTW_BTC_OVERWRITE_BUF_LEN);
	_os_mem_set(d, &msg->working[0], 0, RTW_BTC_OVERWRITE_BUF_LEN);
	msg->len = 0;
	msg->cnt = 0;
}

static void _bt_msg_deinit(struct rtw_hal_com_t *hal_com,
				struct hal_bt_msg *msg)
{
	void *d = halcom_to_drvpriv(hal_com);

	_os_spinlock_free(d, &msg->lock);
}

static void _msg_enq(struct rtw_hal_com_t *hal_com,
			struct phl_queue *q, struct fw_msg_entry *entry)
{
	pq_push(halcom_to_drvpriv(hal_com), q, &entry->list, _tail, _bh);
}

static struct fw_msg_entry *_msg_deq(struct rtw_hal_com_t *hal_com,
					struct phl_queue *q)
{
	struct fw_msg_entry *entry = NULL;
	_os_list *list = NULL;

	if (pq_pop(halcom_to_drvpriv(hal_com), q, &list, _first, _bh))
		entry = (struct fw_msg_entry *)list;

	return entry;
}

static bool _fw_msg_init(struct rtw_hal_com_t *hal_com)
{
	void *d = halcom_to_drvpriv(hal_com);
	struct btc_fw_msg *fw_msg = &hal_com->btc_msg;
	struct fw_msg_entry *entry = NULL;
	u16 i = 0;

	_os_spinlock_init(d, &fw_msg->lock);
	fw_msg->fev_cnt = 0;

	_bt_msg_init(hal_com, &fw_msg->btinfo);
	_bt_msg_init(hal_com, &fw_msg->scbd);

	pq_init(d, &fw_msg->idleq);
	pq_init(d, &fw_msg->waitq);
	for (i = 0; i < BTC_FWBUF_NUM; i++) {
		entry = (struct fw_msg_entry *)_os_kmem_alloc(d,
					sizeof(struct fw_msg_entry));
		if (entry)
			_msg_enq(hal_com, &fw_msg->idleq, entry);
		else
			return false;
	}

	return true;
}

static void _fw_msg_free(struct rtw_hal_com_t *hal_com)
{
	void *d = halcom_to_drvpriv(hal_com);
	struct btc_fw_msg *fw_msg = &hal_com->btc_msg;
	struct fw_msg_entry *entry = NULL;

	_bt_msg_deinit(hal_com, &fw_msg->btinfo);
	_bt_msg_deinit(hal_com, &fw_msg->scbd);

	while (1) {
		entry = _msg_deq(hal_com, &fw_msg->waitq);
		if (entry)
			_os_kmem_free(d, entry, sizeof(struct fw_msg_entry));
		else
			break;
	}
	pq_deinit(d, &fw_msg->waitq);

	while (1) {
		entry = _msg_deq(hal_com, &fw_msg->idleq);
		if (entry)
			_os_kmem_free(d, entry, sizeof(struct fw_msg_entry));
		else
			break;
	}
	pq_deinit(d, &fw_msg->idleq);

	_os_spinlock_free(d, &fw_msg->lock);
}

static void _copy_btmsg(struct rtw_hal_com_t *hal_com,
			struct hal_bt_msg *msg, u16 len, u8 *buf)
{
	void *d = halcom_to_drvpriv(hal_com);

	if (len > RTW_BTC_OVERWRITE_BUF_LEN)
		return;

	_os_spinlock(d, &msg->lock, _bh, NULL);
	msg->cnt++;
	msg->len = len;
	_os_mem_cpy(d, &msg->latest[0], buf, len);
	_os_spinunlock(d, &msg->lock, _bh, NULL);
}

static bool _fw_evnt_enq(struct rtw_hal_com_t *hal_com,
			u8 cls, u8 func, u16 len, u8 *buf)
{
	struct btc_fw_msg *fmsg = &hal_com->btc_msg;
	void *d = halcom_to_drvpriv(hal_com);
	struct fw_msg_entry *entry = NULL;

	entry = _msg_deq(hal_com, &fmsg->idleq);
	if (!entry)
		return false;

	entry->c2h_class = cls;
	entry->c2h_func = func;
	entry->len = len;
	_os_mem_cpy(d, &entry->buf[0], buf, len);
	_msg_enq(hal_com, &fmsg->waitq, entry);

	return true;
}

enum rtw_hal_status rtw_hal_btc_init(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct btc_t *btc = NULL;
	void *drv_priv = NULL;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "%s\n", __FUNCTION__);

	drv_priv = halcom_to_drvpriv(hal_info->hal_com);
	btc = _os_mem_alloc(drv_priv, sizeof(struct btc_t));
	if (!btc) {
		hal_status = RTW_HAL_STATUS_RESOURCE;
		goto error_btc_init;
	}

	/* assign phl_com & hal_com */
	btc->phl = phl_com;
	btc->hal = hal_info->hal_com;

	if (!hal_btc_init(btc)) {
		hal_status = RTW_HAL_STATUS_BTC_INIT_FAILURE;
		goto error_btc_init;
	}

	if (!_fw_msg_init(hal_info->hal_com)) {
		hal_status = RTW_HAL_STATUS_BTC_INIT_FAILURE;
		goto error_msg_init;
	}


	hal_status = RTW_HAL_STATUS_SUCCESS;
	hal_info->btc = btc;

	return hal_status;

error_msg_init:
	_fw_msg_free(hal_info->hal_com);

error_btc_init:
	if (btc) {
		_os_mem_free(drv_priv, (void *)btc, sizeof(struct btc_t));
		hal_info->btc = NULL;
	}

	return hal_status;
}

void rtw_hal_btc_deinit(struct rtw_phl_com_t *phl_com,
				struct hal_info_t *hal_info)
{
	struct btc_t *btc = hal_info->btc;
	void *drv_priv = NULL;

	_fw_msg_free(hal_info->hal_com);

	drv_priv = halcom_to_drvpriv(hal_info->hal_com);
	if (drv_priv && btc) {
		hal_btc_deinit(btc);
		_os_mem_free(drv_priv, (void *)btc, sizeof(struct btc_t));
	}
}

/**********************/
/* called by non-hal layers */
/**********************/
void rtw_hal_btc_update_role_info_ntfy(void *hinfo,  u8 role_id,
						struct rtw_wifi_role_t *wrole,
						struct rtw_phl_stainfo_t *sta,
						enum role_state rstate)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;
	struct btc_wl_link_info r = {0};
#ifdef CONFIG_PHL_P2PPS
	u8 i =0;
#endif /* CONFIG_PHL_P2PPS */

	if (role_id >= MAX_WIFI_ROLE_NUMBER)
		return;


	if (wrole) {
		r.role = wrole->type;
#ifdef RTW_WKARD_ROLE_TYPE
		if (wrole->mstate != MLME_NO_LINK &&
			wrole->real_type != PHL_RTYPE_NONE) {
			r.role = wrole->real_type;
			PHL_INFO("[BTC], rtw_hal_btc_update_role_info_ntfy(): set r.role from type(%d) to real_type(%d)\n",
				wrole->type, wrole->real_type);
		}
#endif /* RTW_WKARD_ROLE_TYPE */
#ifdef CONFIG_PHL_P2PPS
		r.noa = 0;
		r.noa_duration = 0;
		for (i = 0; i < MAX_NOA_DESC; i++) {
			if (wrole->noa_desc[i].enable) {
				r.noa = 1;
				r.noa_duration = wrole->noa_desc[i].duration;
				break;
			}
		}
#endif /* CONFIG_PHL_P2PPS */
		r.phy = wrole->hw_band;
		r.pid = wrole->hw_port;
		r.active = wrole->active;
		r.connected = wrole->mstate;
		r.mode = wrole->cap.wmode;
		r.client_cnt = wrole->assoc_sta_queue.cnt;
		#ifdef RTW_PHL_BCN
		r.bcn_period = wrole->bcn_cmn.bcn_interval;
		r.dtim_period = wrole->dtim_period;
		#endif
		/* Remove it after btc ready */
		r.band = wrole->chandef.band;
		r.ch = wrole->chandef.center_ch;
		r.bw = wrole->chandef.bw;
		hal_mem_cpy(h->hal_com, &r.chdef, &wrole->chandef,
			    sizeof(struct rtw_chan_def));
		hal_mem_cpy(h->hal_com, r.mac_addr, wrole->mac_addr, MAC_ALEN);
	}

	if (sta && wrole->type == PHL_RTYPE_STATION) {/*associated node info??*/
		r.mac_id = sta->macid;
		r.mode = (u8)sta->wmode;
	}

	if (ops && ops->ntfy_role_info)
		ops->ntfy_role_info(btc, role_id, &r, rstate);
}

void rtw_hal_btc_power_on_ntfy(void *hinfo)
{
}

void rtw_hal_btc_power_off_ntfy(void *hinfo)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;

	if (ops && ops->ntfy_power_off)
		ops->ntfy_power_off(btc);
}

void rtw_hal_btc_init_coex_cfg_ntfy(void *hinfo)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;
	u8 mode = btc->phl->dev_cap.btc_mode;

	if (ops && ops->ntfy_init_coex)
		ops->ntfy_init_coex(btc, mode);
}

void rtw_hal_btc_scan_start_ntfy(void *hinfo, enum phl_phy_idx phy_idx,
				  enum band_type band)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;

	if (ops && ops->ntfy_scan_start)
		ops->ntfy_scan_start(btc, phy_idx, band);
}

void rtw_hal_btc_scan_finish_ntfy(void *hinfo, enum phl_phy_idx phy_idx)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;

	if (ops && ops->ntfy_scan_finish)
		ops->ntfy_scan_finish(btc, phy_idx);
}

void rtw_hal_btc_switch_band_ntfy(void *hinfo, enum phl_phy_idx phy_idx,
				  enum band_type band)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;

	if (ops && ops->ntfy_switch_band)
		ops->ntfy_switch_band(btc, phy_idx, band);
}

void rtw_hal_btc_packet_event_ntfy(void *hinfo, u8 pkt_evt_type)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;

	if (ops && ops->ntfy_specific_packet)
		ops->ntfy_specific_packet(btc, pkt_evt_type);
}

void rtw_hal_btc_radio_state_ntfy(void *hinfo, u8 rf_state)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;

	if (ops && ops->ntfy_radio_state)
		ops->ntfy_radio_state(btc, rf_state);
}

void rtw_hal_btc_customerize_ntfy(void *hinfo, u8 type, u16 len, u8 *buf)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;

	if (ops && ops->ntfy_customerize)
		ops->ntfy_customerize(btc, type, len, buf);
}

u8 rtw_hal_btc_wl_rfk_ntfy(struct rtw_hal_com_t *hal_com, u8 phy_idx, u8 rfk_type, u8 rfk_process)
{
	struct hal_info_t *h = hal_com->hal_priv;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;
	u8 val = 0;

	if (ops && ops->ntfy_wl_rfk)
		val = ops->ntfy_wl_rfk(btc, phy_idx, rfk_type, rfk_process);

	return val;
}

void rtw_hal_btc_wl_status_ntfy(void *hinfo, struct rtw_phl_com_t *phl_com, u8 ntfy_num,
				struct rtw_phl_stainfo_t *sta[], u8 reason)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;
	struct btc_wl_stat_info stat_info[MAX_WIFI_ROLE_NUMBER] = {0};
	struct btc_traffic *t;
	struct rtw_stats *phl_stats = &phl_com->phl_stats;
	struct rtw_phl_rainfo ra_info = {0};
	u8 i;

	if(ntfy_num == 0)
		return;

	for (i = 0; i < ntfy_num; i++) {
		_os_mem_set(halcom_to_drvpriv(h->hal_com), &ra_info, 0,
			    sizeof(ra_info));

		stat_info[i].pid = sta[i]->wrole->id;
		stat_info[i].stat.rssi = sta[i]->hal_sta->rssi_stat.rssi >> 1;

		t = &stat_info[i].stat.traffic;
		t->tx_lvl = phl_stats->tx_traffic.lvl;
		t->tx_sts = phl_stats->tx_traffic.sts;
		t->tx_1ss_limit = sta[i]->hal_sta->ra_info.ra_nss_limit;

		t->rx_lvl = phl_stats->rx_traffic.lvl;
		t->rx_sts = phl_stats->rx_traffic.sts;

	        if (RTW_HAL_STATUS_SUCCESS ==
		    rtw_hal_bb_query_rainfo(h, sta[i]->hal_sta, &ra_info))
			t->tx_rate = ra_info.rate;
		else
			t->tx_rate = RTW_DATA_RATE_MAX;

		t->rx_rate = h->hal_com->trx_stat.rx_rate_plurality;
	}

	if (ops && ops->ntfy_wl_sta)
		ops->ntfy_wl_sta(btc, ntfy_num, stat_info, reason);

}

void rtw_hal_btc_fwinfo_ntfy(void *hinfo)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;
	struct rtw_hal_com_t *hal_com = h->hal_com;
	void *d = halcom_to_drvpriv(hal_com);
	struct btc_fw_msg *fmsg = &hal_com->btc_msg;
	struct hal_bt_msg *bmsg = NULL;
	struct fw_msg_entry *entry = NULL;

	_os_spinlock(d, &fmsg->lock, _bh, NULL);
	fmsg->fev_cnt = 0;
	_os_spinunlock(d, &fmsg->lock, _bh, NULL);

	if (!ops || !ops->ntfy_fwinfo)
		return;

	/* bt score board notification */
	while (1) {
		bmsg = &fmsg->scbd;
		if (bmsg->cnt) {
			_os_spinlock(d, &bmsg->lock, _bh, NULL);
			bmsg->cnt = 0;
			_os_mem_cpy(d, &bmsg->working[0],
				&bmsg->latest[0], bmsg->len);
			_os_spinunlock(d, &bmsg->lock, _bh, NULL);
			PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
				  "[BTC], scoreboard notify !! \n");
			ops->ntfy_fwinfo(btc, &bmsg->working[0], bmsg->len,
					BTC_CLASS_FEV, BTC_FEV_BT_SCBD);
		} else
			break;
	}

	/* bt info notification */
	while (1) {
		bmsg = &fmsg->btinfo;
		if (bmsg->cnt) {
			_os_spinlock(d, &bmsg->lock, _bh, NULL);
			bmsg->cnt = 0;
			_os_mem_cpy(d, &bmsg->working[0],
				&bmsg->latest[0], bmsg->len);
			_os_spinunlock(d, &bmsg->lock, _bh, NULL);
			PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
				  "[BTC], bt info notify !! \n");
			ops->ntfy_fwinfo(btc, &bmsg->working[0], bmsg->len,
					BTC_CLASS_FEV, BTC_FEV_BT_INFO);
		} else
			break;
	}

	/* common btc fw events */
	while (1) {
		entry = _msg_deq(hal_com, &fmsg->waitq);
		if (entry) {
			PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
				  "[BTC], fw event notify !! \n");
			ops->ntfy_fwinfo(btc, entry->buf, entry->len,
					entry->c2h_class, entry->c2h_func);
			_msg_enq(hal_com, &fmsg->idleq, entry);
		}
		else
			break;
	}
}

void rtw_hal_btc_timer(void *hinfo, void *timer)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct btc_ops *ops = btc->ops;
	struct btc_tmr *btmr = NULL;
	u8 i = 0;
	bool found = false;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
		"[BTC], %s(), timer = 0x%p !!\n", __func__, timer);

	if (!hinfo || !timer)
		return;

	btmr = (struct btc_tmr *)timer;

	for (i = 0; i < BTC_TIMER_MAX; i++) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
			"[BTC], %s(): btmr = 0x%p, &btc->timer[%d] = 0x%p \n",
				__func__, btmr, i, &btc->timer[i]);
		if (btmr == &btc->timer[i]) {
			found = true;
			PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
				"[BTC], %s(): found timer %p, id = %d \n",
				__func__, btmr, btmr->id);
			break;
		}
	}

	if (ops && found) {
		if (btmr->id == BTC_TIMER_PERIODIC) {
			struct rtw_wifi_role_t *wrole = NULL;
			struct rtw_phl_stainfo_t *sta = NULL;
			struct rtw_phl_stainfo_t *wrole_sta[MAX_WIFI_ROLE_NUMBER] = {0};
			u8 ntfy_num = 0;

			for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
				wrole = &(btc->phl->wifi_roles[i]);
				if(wrole->mstate == MLME_LINKED) {
					sta = rtw_phl_get_stainfo_self(
						btc->phl->phl_priv, wrole);
					if(sta != NULL) {
						wrole_sta[ntfy_num] = sta;
						ntfy_num++;
					}
				}
			}
			rtw_hal_btc_wl_status_ntfy(hinfo, btc->phl, ntfy_num,
					wrole_sta, PHL_BTC_NTFY_RSN_PERIOTIC);
		}
		PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
			"[BTC], %s(): btmr->id = %d \n",
			__func__, btmr->id);
		if(ops->ntfy_timer)
			ops->ntfy_timer(btc, btmr->id);
	} else {
		PHL_INFO("[BTC], %s(): not found, ops = 0x%p, found = %d \n",
				__func__, ops, found);
	}
}

u32 rtw_hal_btc_req_bt_slot_t(void *hinfo)
{
	struct hal_info_t *h = (struct hal_info_t *)hinfo;
	struct btc_t *btc = (struct btc_t *)h->btc;

	return btc->bt_req_len;
}

/***********************/
/* Called by BTC submodule */
/***********************/
void hal_btc_send_event(struct btc_t *btc, u8 *buf, u32 len, u16 ev_id)
{
#ifdef CONFIG_PHL_CMD_BTC
	rtw_phl_btc_send_cmd(btc->phl, buf, len, ev_id);
#endif
}

void hal_btc_notify_ps_tdma(struct btc_t *btc, bool tdma_start)
{
#ifdef CONFIG_POWER_SAVE
	enum rtw_hal_status hstatus;

	hstatus = rtw_hal_ps_pwr_req(btc->phl, HAL_BTC_PWR_REQ, (tdma_start ? true : false));
	if (hstatus != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("[BTC], %s, TDMA (%d) request pwr fail, status: %d \n",
			__func__, tdma_start, hstatus);
	}
#endif
}

bool rtw_hal_btc_proc_cmd(struct hal_info_t *hal_info, struct rtw_proc_cmd *incmd,
						char *output, u32 out_len)
{
	if(incmd->in_type == RTW_ARG_TYPE_BUF)
		halbtc_cmd(hal_info->btc, incmd->in.buf, output, out_len);
	else if(incmd->in_type == RTW_ARG_TYPE_ARRAY){
		halbtc_cmd_parser(hal_info->btc, incmd->in.vector,
					incmd->in_cnt_len, output, out_len);
	}

	return true;
}


#endif

enum rtw_hal_status
rtw_hal_btc_get_efuse_info(struct rtw_hal_com_t *hal_com,
	u8 *efuse_map, enum rtw_efuse_info info_type, void *value,
	u8 size, u8 map_valid)
{
	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

u32 rtw_hal_btc_process_c2h(void *hal, struct rtw_c2h_info *c2h)
{
	struct hal_info_t *h = (struct hal_info_t *)hal;
	struct btc_t *btc = (struct btc_t *)h->btc;
	struct rtw_hal_com_t *hal_com = h->hal_com;
	struct btc_fw_msg *fmsg = &hal_com->btc_msg;
	void *d = halcom_to_drvpriv(hal_com);
	u8 cls = c2h->c2h_class;
	u8 func = c2h->c2h_func;
	u16 len = c2h->content_len;
	u8 *buf = c2h->content;

	if (len && len < RTW_PHL_BTC_FWINFO_BUF) {
		if (cls == BTC_CLASS_FEV && func == BTC_FEV_BT_INFO)
			_copy_btmsg(hal_com, &fmsg->btinfo, len, buf);
		else if (cls == BTC_CLASS_FEV && func == BTC_FEV_BT_SCBD)
			_copy_btmsg(hal_com, &fmsg->scbd, len, buf);
		else
			_fw_evnt_enq(hal_com, cls, func, len, buf);

		_os_spinlock(d, &fmsg->lock, _bh, NULL);
		if (fmsg->fev_cnt == 0) {
			if (rtw_phl_btc_send_cmd(btc->phl, NULL, 0,
						BTC_HMSG_FW_EV))
				fmsg->fev_cnt++;
		}
		_os_spinunlock(d, &fmsg->lock, _bh, NULL);
	} else {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): Invalid c2h packet len : %d\n",
			  __func__, len);
	}

	return 0;
}

