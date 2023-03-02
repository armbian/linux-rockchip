/******************************************************************************
 *
 * Copyright(c) 2019 - 2020 Realtek Corporation.
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
#include "phl_headers.h"

#ifdef CONFIG_FSM
#include "phl_btc_fsm.h"

struct btc_obj {
	struct fsm_main *fsm;
	struct fsm_obj *fsm_obj;
	struct phl_info_t *phl_info;

	struct rtw_phl_btc_ntfy *ntfy;
};

enum BTC_STATE_ST {
	BTC_ST_IDLE,
	BTC_ST_SERVICE
};

enum BTC_EV_ID {
	BTC_EV_START,
	BTC_EV_UPDATE_ROLE_INFO,
	BTC_EV_FWINFO,
	BTC_EV_WLSTA,
	BTC_EV_TIMER,
	BTC_EV_PACKET_EVT,
	BTC_EV_MAX
};

static int btc_idle_st_hdl(void *obj, u16 event, void *param);
static int btc_service_st_hdl(void *obj, u16 event, void *param);

/* STATE table */
static struct fsm_state_ent btc_state_tbl[] = {
	ST_ENT(BTC_ST_IDLE, btc_idle_st_hdl),
	ST_ENT(BTC_ST_SERVICE, btc_service_st_hdl)
	//{BTC_ST_MAX, "BTC_ST_MAX", NULL} /* ST_MAX for fsm safety checking */
};

/* EVENT table */
static struct fsm_event_ent btc_event_tbl[] = {
	EV_ENT(BTC_EV_START),
	EV_ENT(BTC_EV_UPDATE_ROLE_INFO),
	EV_ENT(BTC_EV_FWINFO),
	EV_ENT(BTC_EV_WLSTA),
	EV_DBG(BTC_EV_TIMER),
	EV_ENT(BTC_EV_PACKET_EVT),
	EV_ENT(BTC_EV_MAX) /* EV_MAX for fsm safety checking */
};


/*
 * btc idle handler
 */
static int btc_idle_st_hdl(void *obj, u16 event, void *param)
{
	struct btc_obj *pbtc = (struct btc_obj *)obj;

	FSM_DBG(pbtc->fsm, "[BTC], %s, event : 0x%x\n", __func__, event);
	switch (event) {
	case FSM_EV_STATE_IN:
		break;

	case FSM_EV_TIMER_EXPIRE:
		break;

	case BTC_EV_START:
		phl_fsm_state_goto(pbtc->fsm_obj, BTC_ST_SERVICE);
		break;

	case FSM_EV_STATE_OUT:
		break;

	default:
		break;
	}
	return 0;
}

static int btc_service_st_hdl(void *obj, u16 event, void *param)
{
	struct btc_obj *pbtc = (struct btc_obj *)obj;
	struct phl_info_t *phl_info = pbtc->phl_info;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	int rtn = FSM_FREE_PARAM;

	FSM_DBG(pbtc->fsm, "[BTC], %s, event : 0x%x\n", __func__, event);
	switch (event) {
	case FSM_EV_STATE_IN:
		break;

	case BTC_EV_UPDATE_ROLE_INFO:
		if (param) {
			struct rtw_phl_btc_role_info_param *prinfo = NULL;
			struct rtw_wifi_role_t *wrole = NULL;
			struct rtw_phl_stainfo_t *sta = NULL;

			pbtc->ntfy = param;
			prinfo = &pbtc->ntfy->u.rinfo;

			if (prinfo->role_id < MAX_WIFI_ROLE_NUMBER) {
				wrole = &phl_com->wifi_roles[prinfo->role_id];
				sta = rtw_phl_get_stainfo_self(phl_info, wrole);
			}
#ifdef CONFIG_BTCOEX
			rtw_hal_btc_update_role_info_ntfy(phl_info->hal,
					prinfo->role_id, wrole, sta, prinfo->rstate);
#endif
			if (pbtc->ntfy->ntfy_cb)
				pbtc->ntfy->ntfy_cb(pbtc->ntfy->priv,
						pbtc->ntfy->notify,
						pbtc->ntfy);

			rtn = FSM_FREE_PARAM;
		}
		break;

	case BTC_EV_FWINFO:
#ifdef CONFIG_BTCOEX
		rtw_hal_btc_fwinfo_ntfy(phl_info->hal);
#endif
		break;

	case BTC_EV_TIMER:
	{
#ifdef CONFIG_BTCOEX
		struct rtw_phl_btc_tmr_param *tmr = NULL;

		pbtc->ntfy = param;
		tmr = &pbtc->ntfy->u.tmr;

		rtw_hal_btc_timer(phl_info->hal, tmr->timer);
#endif
	}
		break;

	case BTC_EV_PACKET_EVT:
		if (param) {
			struct rtw_phl_btc_pkt_param *pkt = NULL;
			struct rtw_wifi_role_t *wrole = NULL;
			pbtc->ntfy = param;
			pkt = &pbtc->ntfy->u.pkt;

			if (pkt->role_id < MAX_WIFI_ROLE_NUMBER) {
				wrole = &phl_com->wifi_roles[pkt->role_id];
			}
#ifdef CONFIG_BTCOEX
			rtw_hal_btc_packet_event_ntfy(phl_info->hal,
							pkt->pkt_evt_type);
#endif
			if (pbtc->ntfy->ntfy_cb)
				pbtc->ntfy->ntfy_cb(pbtc->ntfy->priv,
						pbtc->ntfy->notify,
						pbtc->ntfy);

			rtn = FSM_FREE_PARAM;
		}
		break;

	case FSM_EV_TIMER_EXPIRE:
		break;

	case FSM_EV_CANCEL:
		phl_fsm_state_goto(pbtc->fsm_obj, BTC_ST_IDLE);
		break;

	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(pbtc->fsm_obj);
		break;

	default:
		break;
	}
	return rtn;
}

static void btc_dump_obj(void *obj, char *s, int *sz)
{
	/* nothing to do for now */
}

static void btc_dump_fsm(void *fsm, char *s, int *sz)
{
	/* nothing to do for now */
}

/* For EXTERNAL application to create a btc FSM */
/* @root: FSM root structure
 * @phl_info: private data structure to invoke hal/phl function
 *
 * return
 * fsm_main: FSM main structure (Do NOT expose)
 */
struct fsm_main *phl_btc_new_fsm(struct fsm_root *root,
				struct phl_info_t *phl_info)
{
	void *d = phl_to_drvpriv(phl_info);
	struct fsm_main *fsm = NULL;
	struct rtw_phl_fsm_tb tb;

	_os_mem_set(d, &tb, 0, sizeof(tb));
	tb.max_state = sizeof(btc_state_tbl)/sizeof(btc_state_tbl[0]);
	tb.max_event = sizeof(btc_event_tbl)/sizeof(btc_event_tbl[0]);
	tb.state_tbl = btc_state_tbl;
	tb.evt_tbl = btc_event_tbl;
	tb.dump_obj = btc_dump_obj;
	tb.dump_obj = btc_dump_fsm;
	tb.dbg_level = FSM_DBG_INFO;
	tb.evt_level = FSM_DBG_INFO;

	fsm = phl_fsm_init_fsm(root, "btc", phl_info, &tb);

	return fsm;
}

/* For EXTERNAL application to destory btc fsm */
/* @fsm: see fsm_main
 */
void phl_btc_destory_fsm(struct fsm_main *fsm)
{
	if (fsm == NULL)
		return;

	/* deinit fsm local variable if has */

	/* call FSM Framewro to deinit fsm */
	phl_fsm_deinit_fsm(fsm);
}

/* For EXTERNAL application to create btcoex object */
/* @fsm: FSM main structure which created by phl_btc_new_fsm()
 * @phl_info: private data structure to invoke hal/phl function
 *
 * return
 * btc_obj: structure of command object (Do NOT expose)
 */
struct btc_obj *phl_btc_new_obj(struct fsm_main *fsm,
				struct phl_info_t *phl_info)
{
	struct fsm_obj *obj;
	struct btc_obj *pbtc;

	pbtc = phl_fsm_new_obj(fsm, (void **)&obj, sizeof(*pbtc));

	if (pbtc == NULL || obj == NULL) {
		/* TODO free fsm; currently will be freed in deinit process */
		FSM_ERR(fsm, "btc: malloc obj fail\n");
		return NULL;
	}
	pbtc->fsm = fsm;
	pbtc->fsm_obj = obj;
	pbtc->phl_info = phl_info;

	return pbtc;
}

/* For EXTERNAL application to destory btc object */
/* @pbtc: local created command object
 *
 */
void phl_btc_destory_obj(struct btc_obj *pbtc)
{
	if (pbtc == NULL)
		return;

	/* inform FSM framewory to recycle fsm_obj */
	phl_fsm_destory_obj(pbtc->fsm_obj);
}

/* For EXTERNAL application to start btcoex service (expose) */
/* @pbtc: btc object
 */
enum rtw_phl_status phl_btc_start(struct btc_obj *pbtc)
{
	void *d = phl_to_drvpriv(pbtc->phl_info);
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_SUCCESS;
	struct fsm_msg *msg;

	/* Start FSM */
	phl_status = phl_fsm_start_fsm(pbtc->fsm);
	if (phl_status != RTW_PHL_STATUS_SUCCESS)
		return phl_status;

	if (pbtc->fsm_obj == NULL)
		return RTW_PHL_STATUS_FAILURE;

	/* NEW message to start btc object */
	msg = phl_fsm_new_msg(pbtc->fsm_obj, BTC_EV_START);
	if (msg == NULL) {
		FSM_ERR(pbtc->fsm, "btc: alloc msg fail\n");
		return RTW_PHL_STATUS_RESOURCE;
	}
	if (phl_fsm_sent_msg(pbtc->fsm_obj, msg) != RTW_PHL_STATUS_SUCCESS) {
		_os_kmem_free(d, msg, sizeof(*msg));
		return RTW_PHL_STATUS_FAILURE;
	}

	return phl_status;
}

/* For EXTERNAL application to stop btc service (expose) */
/* @pbtc: btc to be cancelled
 */
enum rtw_phl_status phl_btc_cancel(struct btc_obj *pbtc)
{
	return phl_fsm_cancel_obj(pbtc->fsm_obj);
}

/* For EXTERNAL application to notify btc (expose) */
/* @phl: refer to phl_infi_t
 * @notify: notification event
 * @ntfy: notify information (optional)
 * return value:
 *	0: no wait
 *	1: have to wait call back info->ntfy_cb()
 *	-1: Failure
 */
int rtw_phl_btc_notify(void *phl, enum RTW_PHL_BTC_NOTIFY notify,
				struct rtw_phl_btc_ntfy *ntfy)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct btc_obj *pbtc = phl_info->btc_obj;
	int wait = 0, sz = 0;
	u16 event;

	if (ntfy)
		sz = sizeof(*ntfy);

	switch (notify) {
	case PHL_BTC_NTFY_ROLE_INFO:
		event = BTC_EV_UPDATE_ROLE_INFO;
		break;
	case PHL_BTC_NTFY_FWINFO:
		event = BTC_EV_FWINFO;
		break;
	case PHL_BTC_NTFY_PACKET_EVT:
		event = BTC_EV_PACKET_EVT;
		break;
	case PHL_BTC_NTFY_MAX:
		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "rtw_phl_btc_notify(): Unsupported case:%d, please check it\n",
				notify);
		return wait;
	case PHL_BTC_NTFY_TIMER:
		event = BTC_EV_TIMER;
		break;
	default:
		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "rtw_phl_btc_notify(): Unrecognize case:%d, please check it\n",
				notify);
		return wait;
	}
	phl_fsm_gen_msg(phl, pbtc->fsm_obj, ntfy, sz, event);
	return wait;
}

void rtw_phl_btc_role_notify(void *phl, u8 role_id, enum role_state rstate)
{
	struct rtw_phl_btc_ntfy ntfy = {0};
	struct rtw_phl_btc_role_info_param *prinfo = &ntfy.u.rinfo;

	prinfo->role_id = role_id;
	prinfo->rstate = rstate;

	ntfy.notify = PHL_BTC_NTFY_ROLE_INFO;
	ntfy.ops = NULL;
	ntfy.priv = NULL;
	ntfy.ntfy_cb = NULL;

	rtw_phl_btc_notify(phl, ntfy.notify, &ntfy);
}

#ifndef CONFIG_PHL_CMD_BTC
void rtw_phl_btc_packet_event_notify(void *phl, u8 role_id, u8 pkt_evt_type)
{
	struct rtw_phl_btc_ntfy ntfy = {0};
	struct rtw_phl_btc_pkt_param *pkt = &ntfy.u.pkt;

	pkt->role_id = role_id;
	pkt->pkt_evt_type = pkt_evt_type;

	ntfy.notify = PHL_BTC_NTFY_PACKET_EVT;
	ntfy.ops = NULL;
	ntfy.priv = NULL;
	ntfy.ntfy_cb = NULL;

	rtw_phl_btc_notify(phl, ntfy.notify, &ntfy);
}

u8 rtw_phl_btc_pkt_2_evt_type(u8 packet_type)
{
	u8 pkt_evt_type = BTC_PKT_EVT_MAX;

	switch (packet_type) {
	case PACKET_NORMAL:
		pkt_evt_type = BTC_PKT_EVT_NORMAL;
		break;
	case PACKET_DHCP:
		pkt_evt_type = BTC_PKT_EVT_DHCP;
		break;
	case PACKET_ARP:
		pkt_evt_type = BTC_PKT_EVT_ARP;
		break;
	case PACKET_EAPOL:
		pkt_evt_type = BTC_PKT_EVT_EAPOL;
		break;
	case PACKET_EAPOL_START:
		pkt_evt_type = BTC_PKT_EVT_EAPOL_START;
		break;
	default:
		PHL_ERR("%s packet type(%d) not support\n",
			__func__, packet_type);
		break;
	}

	return pkt_evt_type;
}
#endif

static void _btc_fsm_tmr(void *phl, void *timer)
{
	struct rtw_phl_btc_ntfy ntfy = {0};
	struct rtw_phl_btc_tmr_param *tmr = &ntfy.u.tmr;

	tmr->timer = timer;

	ntfy.notify = PHL_BTC_NTFY_TIMER;
	ntfy.ops = NULL;
	ntfy.priv = NULL;
	ntfy.ntfy_cb = NULL;

	rtw_phl_btc_notify(phl, ntfy.notify, &ntfy);
}

void rtw_phl_btc_hub_msg_hdl(void *phl, struct phl_msg *msg)
{
	u8 mdl_id = MSG_MDL_ID_FIELD(msg->msg_id);
	u16 evt_id = MSG_EVT_ID_FIELD(msg->msg_id);

	switch (mdl_id) {
	case PHL_MDL_BTC:
		if (evt_id == BTC_HMSG_TMR_EN) {
			_btc_fsm_tmr(phl, msg->inbuf);
		} else if (evt_id == BTC_HMSG_FW_EV) {
			rtw_phl_btc_notify(phl, PHL_BTC_NTFY_FWINFO, NULL);
		}
		break;
	default:
		break;
	}
}
#endif /*CONFIG_FSM*/

