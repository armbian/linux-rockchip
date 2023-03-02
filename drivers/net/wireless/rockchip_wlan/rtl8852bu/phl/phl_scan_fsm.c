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
 * Author: vincent_fann@realtek.com
 *
 *****************************************************************************/
#include "phl_headers.h"

#ifdef CONFIG_FSM
#include "phl_scan.h"

#ifdef FSM_DBG_MEM_OVERWRITE
#define _os_kmem_alloc(a, b) fsm_kmalloc(b)
#define _os_kmem_free(a, b, c) fsm_kfree(b, c)
#endif

#define SCAN_PROBE_TIMES	3 /* issue max 3 probe_req per channel */
#define SCAN_PROBE_INTERVAL	10 /* preobe_req interval 10ms */
#define SCAN_NULL_PKT_TIME	50 /* 50ms */
#define ALL_TOKEN		0
#define MAX_POWER_ON_TIME	500
#define BACK_OP_CH_DUR_MS	100

#define ALM_MAX_SCAN_TIME	1
#define ALM_PROBE_REQ		2
#define ALM_NEXT_CH		3
#define ALM_BKOP_OFF_CH		4
#ifndef pstr
#define pstr(s) (s + _os_strlen((u8 *)s))
#endif
#ifndef lstr
#define lstr(s, l) (size_t)(l - _os_strlen((u8 *)s))
#endif

enum SCAN_EV_ID {
	SCAN_EV_START,
	SCAN_EV_REQ_PWR_OK,
	SCAN_EV_REQ_PWR_FAIL,
	SCAN_EV_REQ_PWR_TIMEOUT,
	SCAN_EV_NEXT_CH,
	SCAN_EV_NEXT_PROBE_REQ,
	SCAN_EV_PAUSE,
	SCAN_EV_RESUME,
	SCAN_EV_FORCE_ACTIVE,
	SCAN_EV_NOTIFY_PENDING_SCAN_REQ,
	SCAN_EV_PS_ANN_DONE,
	SCAN_EV_BKOP_OFF_CH_EXPIRE,
	SCAN_EV_BKOP_ON_CH_EXPIRE,
	SCAN_EV_NOTIFY_OFF_CH_CMD,
	SCAN_EV_MAX
};

enum SCAN_STATE_ST {
	SCAN_ST_IDLE,
	SCAN_ST_REQ_PWR,
	SCAN_ST_OFF_CH,
	SCAN_ST_BACK_OP
};

static int scan_idle_st_hdl(void *obj, u16 event, void *param);
static int scan_req_pwr_st_hdl(void *obj, u16 event, void *param);
static int scan_off_ch_st_hdl(void *obj, u16 event, void *param);
static int scan_back_op_st_hdl(void *obj, u16 event, void *param);

/* STATE table */
static struct fsm_state_ent scan_state_tbl[] = {
	ST_ENT(SCAN_ST_IDLE, scan_idle_st_hdl),
	ST_ENT(SCAN_ST_REQ_PWR, scan_req_pwr_st_hdl),
	ST_ENT(SCAN_ST_OFF_CH, scan_off_ch_st_hdl),
	ST_ENT(SCAN_ST_BACK_OP, scan_back_op_st_hdl)
};

/* EVENT table */
static struct fsm_event_ent scan_event_tbl[] = {
	EV_ENT(SCAN_EV_START),
	EV_ENT(SCAN_EV_REQ_PWR_OK),
	EV_ENT(SCAN_EV_REQ_PWR_FAIL),
	EV_ENT(SCAN_EV_REQ_PWR_TIMEOUT),
	EV_ENT(SCAN_EV_NEXT_CH),
	EV_ENT(SCAN_EV_NEXT_PROBE_REQ),
	EV_ENT(SCAN_EV_PAUSE),
	EV_ENT(SCAN_EV_RESUME),
	EV_ENT(SCAN_EV_FORCE_ACTIVE),
	EV_ENT(SCAN_EV_NOTIFY_PENDING_SCAN_REQ),
	EV_ENT(SCAN_EV_PS_ANN_DONE),
	EV_ENT(SCAN_EV_BKOP_OFF_CH_EXPIRE),
	EV_ENT(SCAN_EV_BKOP_ON_CH_EXPIRE),
	EV_ENT(SCAN_EV_NOTIFY_OFF_CH_CMD),
	EV_ENT(SCAN_EV_MAX)
};

struct scan_obj {
	struct fsm_main *fsm;
	struct fsm_obj *fsm_obj;

	struct rtw_phl_scan_param *param;
	struct phl_info_t *phl_info;
	struct phl_queue req_q;
	struct phl_queue off_ch_cmd_q;

	u32 token; /* global increase when new scan request is comming */
	u32 token_running; /* token of running scan */

	u8 fltr_mode; /* backup filter mode before off channel */
	u8 candidate; /* Guarantee we have one candidate */
	u8 probe_cnts;
	u8 off_ch_step;

	/* back op */
	bool back_op_is_required;
	struct phl_scan_channel back_op_ch;

	struct rtw_phl_scan_ops *ops;
};

/*
 * SCAN state sub function
 */
#define OFF_CH_STEP_CLEAR	0x00
#define OFF_CH_SET_FILTER	0x01
#define OFF_CH_PAUSE_TX		0x02

void off_ch_set_step(struct scan_obj *pscan, u8 step)
{
	pscan->off_ch_step |= step;
}

void off_ch_clr_step(struct scan_obj *pscan, u8 step)
{
	pscan->off_ch_step &= ~step;
}

bool off_ch_chk_step(struct scan_obj *pscan, u8 step)
{
	if (pscan->off_ch_step & step)
		return true;
	return false;
}

/*
 * stop netif queue
 * notify the AP about us leaving the channel and stop all STA interfaces.
 * Stop queues and transmit all frames queued by the driver before
 * sending nullfunc to enable powersave at the AP.
 */
static int drv_offchannel_stop_vifs(struct scan_obj *pscan)
{
	// reference ieee80211_offchannel_stop_vifs();

	/* TODO stop upper netif ac queues */
	/* stop lower macid */
	return 0;
}


/* return to OP channel */
static int off_ch_return_set_ch_bw(struct scan_obj *pscan)
{
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_FAILURE;
	struct phl_info_t *phl_info = pscan->phl_info;
	struct rtw_wifi_role_t *wrole = pscan->param->wifi_role;
	struct rtw_chan_def chandef = {0};

	if (wrole == NULL)
		return -1;

	phl_sts = phl_mr_get_chandef(phl_info, wrole, false, &chandef);
	if (phl_sts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s phl_mr_get_chandef failed\n", __func__);
		return -2;
	}

	PHL_DUMP_CHAN_DEF_EX(&chandef);

	phl_set_ch_bw(wrole, &chandef, false);

	FSM_INFO(pscan->fsm, "%s %s() channel=%d, bw=%d, offest=%d\n",
		phl_fsm_obj_name(pscan->fsm_obj), __func__,
		chandef.chan, chandef.bw, chandef.offset);

	return 0;
}

#ifdef CONFIG_RTW_ACS
static void scan_acs_mntr_trigger(struct scan_obj *pscan,
	struct phl_scan_channel *scan_ch)
{
	u16 monitor_time = scan_ch->duration - MONITOR_TIME_TOLERANCE;

	phl_acs_mntr_trigger(pscan->phl_info,
		(u8)pscan->param->ch_idx, scan_ch->channel, monitor_time);
}

static void scan_acs_mntr_result(struct scan_obj *pscan)
{
	if (pscan->param->ch_idx < 0)
		return;

	phl_acs_mntr_result(pscan->phl_info);
}
#endif /* CONFIG_RTW_ACS */

/* inform CORE ahd HAL SCAN is complete */
static int scan_complete(struct scan_obj *pscan)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)pscan->phl_info;
	struct rtw_phl_scan_param *param = pscan->param;
	u8 band_idx = 0;
	enum phl_phy_idx phy_idx = HW_PHY_0;

	if (param->ch_num == (pscan->param->ch_idx + 1))
		param->result = SCAN_REQ_COMPLETE;

	param->end_time = _os_get_cur_time_ms();

	/* dump scan time */
	param->total_scan_time =
		phl_fsm_time_pass(param->enqueue_time);

	FSM_INFO(pscan->fsm, "%s takes %d ms to scan %d/%d channels\n",
		phl_fsm_obj_name(pscan->fsm_obj),
		param->total_scan_time, param->ch_idx + 1, param->ch_num);

	if (pscan->param->wifi_role)
		band_idx = pscan->param->wifi_role->hw_band;
	else
		band_idx = 0;

	if (band_idx == HW_BAND_1)
		phy_idx = HW_PHY_1;
	rtw_hal_btc_scan_finish_ntfy(phl_info->hal, phy_idx);
	rtw_hal_notification(phl_info->hal, MSG_EVT_SCAN_END, band_idx);

	phl_p2pps_noa_resume_all(phl_info, param->wifi_role);
#ifdef CONFIG_MCC_SUPPORT
	if (phl_mr_coex_handle(phl_info, param->wifi_role, 0,
			param->wifi_role->hw_band, MR_COEX_TRIG_BY_SCAN)
						!= RTW_PHL_STATUS_SUCCESS) {
		FSM_ERR(pscan->fsm,
			"%s enable MCC failed\n",
			phl_fsm_obj_name(pscan->fsm_obj));
		return -1;
	}
#endif /* CONFIG_MCC_SUPPORT */
	if (pscan->ops && pscan->ops->scan_complete)
		pscan->ops->scan_complete(pscan->param->priv, pscan->param);

	return 0;
}

static void scan_set_timer(struct scan_obj *pscan, u32 duration, u16 event)
{
	FSM_DBG(pscan->fsm, "%s %s() duration=%d\n",
		phl_fsm_obj_name(pscan->fsm_obj), __func__, duration);

	phl_fsm_set_alarm(pscan->fsm_obj, duration, event);
}

/* Announce null data if needing
 * @pscan: scan obj
 * @off_ch:
 *	true: go off channel (issue null-1)
 *	false: back to OP channel (issue null-0)
 *	need_wait:
 *	  false: issue null packet is not necessary
 *	  true: null packet issued (caller have to wait null data transmission)
 *
 * return: 0 success, negativ value: fail
 */
static int scan_off_chan(struct scan_obj *pscan, bool off_ch, bool *need_wait)
{
	struct rtw_phl_com_t *phl_com = pscan->phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct rtw_wifi_role_t *wrole = pscan->param->wifi_role;
	struct hw_band_ctl_t *band_ctl = &(mr_ctl->band_ctrl[wrole->hw_band]);
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;

	*need_wait = false;

	psts = phl_mr_offch_hdl(pscan->phl_info, pscan->param->wifi_role,
		off_ch, pscan->param->priv, pscan->ops->scan_issue_null_data);

	/* call back fail */
	if (psts != RTW_PHL_STATUS_SUCCESS)
		return -1;

	if ((band_ctl->cur_info.ld_sta_num) > 0)
		*need_wait = false;

	/* ap is currently operating */
	if ((band_ctl->cur_info.ap_num) > 0)
		pscan->back_op_is_required = true;

	if ((band_ctl->cur_info.ld_sta_num + band_ctl->cur_info.ld_ap_num) > 0)
		pscan->back_op_is_required = true;

	FSM_INFO(pscan->fsm, "%s %s() off_ch=%d, back_op_is_requred=%d\n",
		phl_fsm_obj_name(pscan->fsm_obj), __func__,
		off_ch, pscan->back_op_is_required);

	return 0;
}

/* Move channel index and decide scan channel */
static struct phl_scan_channel *scan_select_channel(
	struct scan_obj *pscan)
{
	struct rtw_phl_scan_param *p = pscan->param;

	if (p->ch_idx == -1)
		goto next_ch;

	/* back_op acording to op_ch_count */
	if (p->back_op_mode == SCAN_BKOP_CNT &&
		pscan->back_op_is_required && p->scan_ch &&
		(p->scan_ch->scan_mode != BACKOP_MODE) &&
		((p->ch_idx + 1) % p->back_op_ch_cnt == 0)) {

		/* goto back op channel */
		p->scan_ch = &pscan->back_op_ch;
		goto done; /* do back_op */

	} else if (p->back_op_mode == SCAN_BKOP_TIMER && p->scan_ch &&
		(p->scan_ch->scan_mode == BACKOP_MODE)) {
		/* we just return form op ch;
		 * stay at the same channel for now
		 */
		p->scan_ch = &p->ch[p->ch_idx];
		goto done;
	}

next_ch:
	p->ch_idx++;

	if (p->ch_idx == p->ch_num) {
		/* no more channel for now */
		if (p->repeat > 0) {

			/* go back to the first channel */
			p->ch_idx = 0;
			FSM_INFO(pscan->fsm, "%s repeat=%d\n",
				phl_fsm_obj_name(pscan->fsm_obj), p->repeat);

			/* 255 means loop forever */
			if (p->repeat != 255)
				p->repeat--;
		} else {
			p->ch_idx--;
			return NULL; /* we are done */
		}
	}
	p->scan_ch = &p->ch[p->ch_idx];

	if (p->scan_ch->scan_mode == BACKOP_MODE &&
		pscan->back_op_is_required == false)
		goto next_ch;
done:
	FSM_INFO(pscan->fsm, "%s ch_num = %d/%d, ch = %d\n",
		phl_fsm_obj_name(pscan->fsm_obj), (p->ch_idx + 1),
		p->ch_num, p->scan_ch->channel);

	return p->scan_ch;
}

/* Switch channel */
static void scan_set_channel_bw(struct scan_obj *pscan, u16 ch,
	enum channel_width bw, enum chan_offset offset, u8 phy_idx)
{
	struct rtw_chan_def chdef = {0};

	FSM_INFO(pscan->fsm,
		"%s %s() ch=%d, bw=%d, offest=%d, duration=%d\n",
		phl_fsm_obj_name(pscan->fsm_obj), __func__,
		ch, bw, offset, pscan->param->scan_ch->duration);

	chdef.chan = (u8)ch;
	chdef.bw = bw;
	chdef.offset = offset;

	phl_set_ch_bw(pscan->param->wifi_role, &chdef, false);

	if (pscan->ops->scan_ch_ready)
		pscan->ops->scan_ch_ready(pscan->param->priv, pscan->param);
}

/* call back caller to issue probe request */
static void scan_issue_probereq(struct scan_obj *pscan)
{
	if (pscan->param->scan_ch->type == RTW_PHL_SCAN_PASSIVE)
		return;

	if (pscan->ops->scan_issue_pbreq)
		pscan->ops->scan_issue_pbreq(pscan->param->priv, pscan->param);
}

/* flush NIC TX FIFO */
static int scan_flush_tx_queue(struct scan_obj *pscan)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	hal_status = rtw_hal_scan_flush_queue(pscan->phl_info->hal,
		pscan->param->wifi_role);

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		FSM_WARN(pscan->fsm, "%s flush tx queue fail\n",
			phl_fsm_obj_name(pscan->fsm_obj));
	return hal_status;
}

/* Pause NIC TX FIFO */
static int scan_pause_tx(struct scan_obj *pscan)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)pscan->phl_info;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	bool off_ch = true;

	/* we want to off-ch - pause TX */
	hal_status = rtw_hal_scan_pause_tx_fifo(phl_info->hal,
		pscan->param->wifi_role->hw_band, off_ch);

	if (hal_status == RTW_HAL_STATUS_SUCCESS)
		off_ch_set_step(pscan, OFF_CH_PAUSE_TX);
	else
		FSM_WARN(pscan->fsm, "%s pause tx fifo fail\n",
			phl_fsm_obj_name(pscan->fsm_obj));

	return hal_status;
}

static int scan_resume_tx(struct scan_obj *pscan)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)pscan->phl_info;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	bool off_ch = false;

	/* we want to op-ch - resume TX */
	/* Do not resume un-paused TX */
	if (!off_ch_chk_step(pscan, OFF_CH_PAUSE_TX))
		return RTW_HAL_STATUS_SUCCESS;

	hal_status = rtw_hal_scan_pause_tx_fifo(phl_info->hal,
		pscan->param->wifi_role->hw_band, off_ch);

	off_ch_clr_step(pscan, OFF_CH_PAUSE_TX);
	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		FSM_WARN(pscan->fsm, "%s resume tx fifo fail\n",
			phl_fsm_obj_name(pscan->fsm_obj));

	return hal_status;
}

static int scan_set_filter(struct scan_obj *pscan)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	bool off_ch = true;

	hal_status = rtw_hal_scan_set_rxfltr_by_mode(pscan->phl_info->hal,
		pscan->param->wifi_role->hw_band,
		off_ch, &pscan->fltr_mode);

	if (hal_status == RTW_HAL_STATUS_SUCCESS)
		off_ch_set_step(pscan, OFF_CH_SET_FILTER);

	return hal_status;
}

static int scan_clr_filter(struct scan_obj *pscan)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	bool off_ch = false;

	/* Do not clear clear filter */
	if (!off_ch_chk_step(pscan, OFF_CH_SET_FILTER))
		return hal_status;

	hal_status = rtw_hal_scan_set_rxfltr_by_mode(pscan->phl_info->hal,
		pscan->param->wifi_role->hw_band,
		off_ch, &pscan->fltr_mode);

	off_ch_clr_step(pscan, OFF_CH_SET_FILTER);

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		FSM_WARN(pscan->fsm, "%s clear filter fail\n",
			phl_fsm_obj_name(pscan->fsm_obj));

	return hal_status;

}

/* inform CORE and HAL SCAN is starting */
static int scan_start(struct scan_obj *pscan)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)pscan->phl_info;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_phl_scan_param *param = pscan->param;
	u32 pass_time, remain_time, end_time;
	enum band_type band = BAND_ON_5G;
	u8 band_idx = 0;
	enum phl_phy_idx phy_idx = HW_PHY_0;

	FSM_INFO(pscan->fsm, "%s %s()\n",
		phl_fsm_obj_name(pscan->fsm_obj), __func__);
#ifdef CONFIG_MCC_SUPPORT
	if (phl_mr_coex_disable(phl_info, param->wifi_role,
				param->wifi_role->hw_band, MR_COEX_TRIG_BY_SCAN)
				!= RTW_PHL_STATUS_SUCCESS) {
		FSM_ERR(pscan->fsm,
			"%s disable MCC failed\n",
			phl_fsm_obj_name(pscan->fsm_obj));
		return -1;
	}
#endif /* CONFIG_MCC_SUPPORT */
	phl_p2pps_noa_pause_all(phl_info, param->wifi_role);
	param->start_time = _os_get_cur_time_ms();

	/* check max scan time */
	if (param->max_scan_time > 0) {
		end_time = param->enqueue_time + param->max_scan_time;
		pass_time = phl_fsm_time_pass(param->enqueue_time);

		if (pass_time >= param->max_scan_time) {
			FSM_ERR(pscan->fsm,
				"%s Timeout! pass_time %d > max_time %d\n",
				phl_fsm_obj_name(pscan->fsm_obj),
				pass_time, param->max_scan_time);
			return -1;
		}
		remain_time = phl_fsm_time_left(param->enqueue_time, end_time);

		FSM_INFO(pscan->fsm,
			"%s: max_time = %d ms, remain = %d ms\n",
			phl_fsm_obj_name(pscan->fsm_obj),
			param->max_scan_time, remain_time);

		phl_fsm_set_alarm_ext(pscan->fsm_obj, remain_time,
			FSM_EV_CANCEL, ALM_MAX_SCAN_TIME, NULL);
	}

	if (param->wifi_role)
		band_idx = param->wifi_role->hw_band;
	else
		band_idx = 0;

	if (band_idx == HW_BAND_1)
		phy_idx = HW_PHY_1;
	band = hal_com->band[band_idx].cur_chandef.band;
	rtw_hal_btc_scan_start_ntfy(phl_info->hal, phy_idx, band);
	rtw_hal_notification(phl_info->hal, MSG_EVT_SCAN_START, band_idx);

	/* [scan start notify] */
	if (pscan->ops->scan_start)
		pscan->ops->scan_start(pscan->param->priv, param);

	pscan->token_running = param->token;
	pscan->param->result = SCAN_REQ_CANCEL;
	return 0;
}

static void scan_free_req(struct scan_obj *pscan, struct fsm_msg *msg)
{
	void *d = phl_to_drvpriv(pscan->phl_info);
	struct rtw_phl_scan_param *scan_param =
		(struct rtw_phl_scan_param *)msg->param;

	if (msg->param != NULL) {
		if (scan_param->ch != NULL)
			_os_kmem_free(d, scan_param->ch, scan_param->ch_sz);
		_os_kmem_free(d, msg->param, msg->param_sz);
	}
	_os_kmem_free(d, (void *)msg, sizeof(*msg));
}

static void scan_free_msg(struct scan_obj *pscan, struct fsm_msg *msg)
{
	void *d = phl_to_drvpriv(pscan->phl_info);

	_os_kmem_free(d, msg->param, msg->param_sz);
	_os_kmem_free(d, msg, sizeof(*msg));
}

void scan_ps_cb(void *phl, void *hdl, void *ctx, enum rtw_phl_status stat)
{
	struct scan_obj *pscan = (struct scan_obj *)ctx;

	if (stat == RTW_PHL_STATUS_SUCCESS)
		phl_fsm_gen_msg(phl,
			pscan->fsm_obj, NULL, 0, SCAN_EV_REQ_PWR_OK);
	else
		phl_fsm_gen_msg(phl, pscan->fsm_obj,
			NULL, 0, SCAN_EV_REQ_PWR_FAIL);
}

static bool chk_pending_req(struct scan_obj *pscan)
{
	void *d = phl_to_drvpriv(pscan->phl_info);
	_os_list *obj;

	/* Make sure we only have one candidate */
	if (pscan->candidate)
		return true;

	/* Check pending scan request
	 * Dequeue extra_queue and enqueue back to msg_queue
	 */
	if (!pq_pop(d, &pscan->req_q, &obj, _first, _bh))
		return false;

	pscan->candidate = 1;
	if (phl_fsm_sent_msg(pscan->fsm_obj, (struct fsm_msg *)obj) !=
		RTW_PHL_STATUS_SUCCESS) {
		scan_free_req(pscan, (struct fsm_msg *)obj);
		return false;
	}
	return true; /* has pending req */
}

/*
 * SCAN state handler
 */

/*
 * scan idle handler
 */
static int scan_idle_st_hdl(void *obj, u16 event, void *param)
{
	struct scan_obj *pscan = (struct scan_obj *)obj;
	struct rtw_phl_scan_param *scan_param;
	void *d = phl_to_drvpriv(pscan->phl_info);
	int rtn = FSM_FREE_PARAM;

	switch (event) {

	case FSM_EV_STATE_IN:

		phl_fsm_cancel_alarm_ext(pscan->fsm_obj, ALM_MAX_SCAN_TIME);
		scan_complete(pscan);

		/* clear all used data */
		_os_kmem_free(d, pscan->param->ch, pscan->param->ch_sz);
		_os_kmem_free(d, pscan->param, sizeof(*pscan->param));
		pscan->param = NULL;
		pscan->token_running = 0;
		pscan->back_op_is_required = false;
		pscan->back_op_ch.duration = BACK_OP_CH_DUR_MS;

		if (chk_pending_req(pscan))
			break;

		break;

	case SCAN_EV_START:

		/* save param from caller */
		scan_param = (struct rtw_phl_scan_param *)param;
		pscan->param = scan_param;
		pscan->ops = scan_param->ops;

		if (scan_param->back_op_ch_dur_ms)
			pscan->back_op_ch.duration =
				scan_param->back_op_ch_dur_ms;

		/* Tell FSM framework DON'T free param
		 * Scan fsm will free it when scan is done
		 */
		rtn = FSM_KEEP_PARAM;

		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_REQ_PWR);
		pscan->candidate = 0;
		break;

	case SCAN_EV_NOTIFY_PENDING_SCAN_REQ:

		chk_pending_req(pscan);
		break;

	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(pscan->fsm_obj);
		break;

	default:
		break;
	}

	return rtn;
}

static int scan_req_pwr_st_hdl(void *obj, u16 event, void *param)
{
	struct scan_obj *pscan = (struct scan_obj *)obj;
	int rtn = FSM_FREE_PARAM;
	enum rtw_phl_status phl_st = RTW_PHL_STATUS_SUCCESS;

	switch (event) {

	case FSM_EV_STATE_IN:

		if (phl_st == RTW_PHL_STATUS_PENDING) {
			/* we have to wait SCAN_EV_REQ_PWR_OK */
			phl_fsm_set_alarm(pscan->fsm_obj,
				MAX_POWER_ON_TIME, SCAN_EV_REQ_PWR_TIMEOUT);
			break;
		}

		if (phl_st != RTW_PHL_STATUS_SUCCESS) {
			FSM_ERR(pscan->fsm, "%s power on fail(%d)\n",
				phl_fsm_obj_name(pscan->fsm_obj), phl_st);
			phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_IDLE);
			break;
		}
		/* fall through */

	case SCAN_EV_REQ_PWR_OK:

		if (scan_start(pscan) < 0) {
			phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_IDLE);
			break;
		}

		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_OFF_CH);
		break;

	case SCAN_EV_REQ_PWR_FAIL:
	case SCAN_EV_REQ_PWR_TIMEOUT:

		FSM_ERR(pscan->fsm, "%s power on fail\n",
			phl_fsm_obj_name(pscan->fsm_obj));
		/* fall through */

	case FSM_EV_CANCEL:
		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_IDLE);
		break;

	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(pscan->fsm_obj);
		break;

	default:
		break;
	}

	return rtn;
}

int handle_off_ch_cmd(struct scan_obj *pscan)
{
	struct rtw_phl_scan_param *param = pscan->param;
	void *d = phl_to_drvpriv(pscan->phl_info);
	int off_ch_tx_num = pscan->off_ch_cmd_q.cnt;
	struct fsm_msg *msg;
	_os_list *obj;


	while (pq_pop(d, &pscan->off_ch_cmd_q, &obj, _first, _bh)) {
		msg = (struct fsm_msg *)obj;
		if (param && param->ops && param->ops->scan_off_ch_tx)
			param->ops->scan_off_ch_tx(param->priv,
				param, msg->param);
		scan_free_msg(pscan, msg);
	}
	return off_ch_tx_num;
}

/* swtich channel and scan
 */
static int scan_off_ch_st_hdl(void *obj, u16 event, void *param)
{
	struct scan_obj *pscan = (struct scan_obj *)obj;
	struct rtw_phl_scan_param *p = pscan->param;
	struct phl_scan_channel *scan_ch;
	bool need_wait;

	switch (event) {
	case FSM_EV_STATE_IN:

		pscan->off_ch_step = OFF_CH_STEP_CLEAR;
		if (scan_off_chan(pscan, true, &need_wait) < 0) {
			/* issue Null-1 fail */
			phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_IDLE);
			break;
		}

		if (need_wait == true) {
			/* TODO wait tx report */
			scan_set_timer(pscan,
				SCAN_NULL_PKT_TIME, SCAN_EV_PS_ANN_DONE);
			break;
		}

	/* fall through */
	case SCAN_EV_PS_ANN_DONE:
		scan_flush_tx_queue(pscan);
		scan_pause_tx(pscan);
		scan_set_filter(pscan);

		/* start back_op_off_ch_dur */
		if ((p->back_op_mode == SCAN_BKOP_TIMER) &&
			pscan->back_op_is_required)
			phl_fsm_set_alarm_ext(pscan->fsm_obj,
				p->back_op_off_ch_dur_ms,
				SCAN_EV_BKOP_OFF_CH_EXPIRE,
				ALM_BKOP_OFF_CH, NULL);

		/* dequeue off_ch_cmd */
		if (handle_off_ch_cmd(pscan) > 0) {
			if (pscan->back_op_is_required == true)
				phl_fsm_extend_alarm_ext(pscan->fsm_obj,
					pscan->param->back_op_off_ch_ext_dur_ms,
					ALM_BKOP_OFF_CH);
			else
				phl_fsm_extend_alarm_ext(pscan->fsm_obj,
					pscan->param->back_op_off_ch_ext_dur_ms,
					ALM_NEXT_CH);
		}

	/* fall through */
	case SCAN_EV_NEXT_CH:

		phl_fsm_cancel_alarm_ext(pscan->fsm_obj, ALM_PROBE_REQ);

		#ifdef CONFIG_RTW_ACS
		scan_acs_mntr_result(pscan);
		#endif

		scan_ch = scan_select_channel(pscan);
		if (scan_ch == NULL) {
			/* no more channel, we are done */
			phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_IDLE);
			break;
		}

		if (scan_ch->scan_mode == BACKOP_MODE) {
			phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_BACK_OP);
			break;
		}

		pscan->probe_cnts = SCAN_PROBE_TIMES;
		scan_set_channel_bw(pscan, scan_ch->channel, CHANNEL_WIDTH_20,
					CHAN_OFFSET_NO_EXT, 0);
		#ifdef CONFIG_RTW_ACS
		scan_acs_mntr_trigger(pscan, scan_ch);
		#endif

		if (phl_fsm_is_alarm_off_ext(
			pscan->fsm_obj, ALM_NEXT_CH)) {
			phl_fsm_set_alarm_ext(pscan->fsm_obj, scan_ch->duration,
				SCAN_EV_NEXT_CH, ALM_NEXT_CH, NULL);
		}

	/* fall through */
	case SCAN_EV_NEXT_PROBE_REQ:

		if (pscan->probe_cnts-- == 0)
			break;

		if (pscan->ops->scan_issue_pbreq == NULL ||
			 p->scan_ch->type == RTW_PHL_SCAN_PASSIVE)
			break;

		scan_issue_probereq(pscan);
		phl_fsm_set_alarm_ext(pscan->fsm_obj, SCAN_PROBE_INTERVAL,
			SCAN_EV_NEXT_PROBE_REQ, ALM_PROBE_REQ, NULL);
		break;

	case SCAN_EV_BKOP_OFF_CH_EXPIRE:
		p->scan_ch = &pscan->back_op_ch;
		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_BACK_OP);
		break;

	case SCAN_EV_FORCE_ACTIVE:

		/* Are we in active scan channel */
		if (p->scan_ch->type == RTW_PHL_SCAN_ACTIVE)
			break;

		scan_ch = (struct phl_scan_channel *)param;

		/* request ch mismaches current scan ch */
		if (scan_ch->channel != p->scan_ch->channel)
			break;

#ifdef RTW_WKARD_PHL_FSM_SCAN_PASSIVE_TO_ACTIVE
		if (p->scan_ch->ext_act_scan == EXT_ACT_SCAN_ENABLE)
			p->scan_ch->ext_act_scan = EXT_ACT_SCAN_DONE;
		else {
			FSM_INFO(pscan->fsm, "ch(%d) scan type not changed, ext_act_scan(%d) \n",
					 scan_ch->channel, p->scan_ch->ext_act_scan);
			break;
		}
#endif

		FSM_INFO(pscan->fsm, "ch(%d) PASSIVE->ACTIVE!\n", scan_ch->channel);

		p->scan_ch->type = RTW_PHL_SCAN_ACTIVE;
		scan_issue_probereq(pscan);
		phl_fsm_set_alarm_ext(pscan->fsm_obj, SCAN_PROBE_INTERVAL,
			SCAN_EV_NEXT_PROBE_REQ, ALM_PROBE_REQ, NULL);
		break;

	case SCAN_EV_NOTIFY_OFF_CH_CMD:

		/* dequeue off_ch_cmd */
		if (handle_off_ch_cmd(pscan) > 0) {
			if (pscan->back_op_is_required == true)
				phl_fsm_extend_alarm_ext(pscan->fsm_obj,
					pscan->param->back_op_off_ch_ext_dur_ms,
					ALM_BKOP_OFF_CH);
			else
				phl_fsm_extend_alarm_ext(pscan->fsm_obj,
					pscan->param->back_op_off_ch_ext_dur_ms,
					ALM_NEXT_CH);
		}
		break;

	case SCAN_EV_PAUSE:
		phl_fsm_pause_alarm(pscan->fsm_obj);
		phl_fsm_pause_alarm_ext(pscan->fsm_obj, ALM_NEXT_CH);
		phl_fsm_pause_alarm_ext(pscan->fsm_obj, ALM_PROBE_REQ);
		phl_fsm_pause_alarm_ext(pscan->fsm_obj, ALM_BKOP_OFF_CH);
		break;

	case SCAN_EV_RESUME:
		phl_fsm_resume_alarm(pscan->fsm_obj);
		phl_fsm_resume_alarm_ext(pscan->fsm_obj, ALM_NEXT_CH);
		phl_fsm_resume_alarm_ext(pscan->fsm_obj, ALM_PROBE_REQ);
		phl_fsm_resume_alarm_ext(pscan->fsm_obj, ALM_BKOP_OFF_CH);
		break;

	case FSM_EV_CANCEL:
		phl_fsm_cancel_alarm_ext(pscan->fsm_obj, ALM_NEXT_CH);
		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_IDLE);
		break;

	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(pscan->fsm_obj);
		phl_fsm_cancel_alarm_ext(pscan->fsm_obj, ALM_PROBE_REQ);
		phl_fsm_cancel_alarm_ext(pscan->fsm_obj, ALM_BKOP_OFF_CH);
		scan_clr_filter(pscan);
		off_ch_return_set_ch_bw(pscan);
		scan_resume_tx(pscan);
		scan_off_chan(pscan, false, &need_wait);

		if (p->scan_ch->scan_mode != BACKOP_MODE)
			phl_fsm_cancel_alarm_ext(pscan->fsm_obj, ALM_NEXT_CH);
		break;

	default:
		break;
	}
	return 0;
}

/* Stay in OP channel for a while
 * OP channel normal data TRx
 */
static int scan_back_op_st_hdl(void *obj, u16 event, void *param)
{
	struct scan_obj *pscan = (struct scan_obj *)obj;
	u32 start_time = _os_get_cur_time_ms();


	switch (event) {
	case FSM_EV_STATE_IN:

		scan_set_timer(pscan,
			pscan->param->scan_ch->duration,
			SCAN_EV_BKOP_ON_CH_EXPIRE);

		break;

	case SCAN_EV_BKOP_ON_CH_EXPIRE:

		/* leve back_op channel */
		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_OFF_CH);
		break;

	case SCAN_EV_NEXT_CH:
		phl_fsm_set_alarm_ext(pscan->fsm_obj,
			phl_fsm_time_pass(start_time) + SCAN_NULL_PKT_TIME,
			SCAN_EV_NEXT_CH, ALM_NEXT_CH, NULL);
		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_OFF_CH);
		break;

	case SCAN_EV_NOTIFY_OFF_CH_CMD:
		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_OFF_CH);
		break;

	case SCAN_EV_PAUSE:
		phl_fsm_pause_alarm(pscan->fsm_obj);
		break;

	case SCAN_EV_RESUME:
		phl_fsm_resume_alarm(pscan->fsm_obj);
		break;

	case FSM_EV_CANCEL:
		phl_fsm_state_goto(pscan->fsm_obj, SCAN_ST_IDLE);
		break;

	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(pscan->fsm_obj);
		break;

	default:
		break;
	}
	return 0;
}

static u32 scan_get_token(struct scan_obj *pscan)
{
	pscan->token++;
	if (pscan->token == ALL_TOKEN)
		pscan->token++;
	return pscan->token;
}

static void scan_dump_obj(void *obj, char *s, int *sz)
{
	/* nothing to do for now */
}

static void scan_dump_fsm(void *fsm, char *s, int *sz)
{
	/* nothing to do for now */
}

static void scan_dbg_help(struct scan_obj *pscan, char *p, int *sz)
{
	int len = *sz;

	_os_snprintf(pstr(p), lstr(p, len),
		"usage:\n\t<%s> req del <0|token>\n\t<%s> req <pause|resume>\n",
		phl_fsm_obj_name(pscan->fsm_obj),
		phl_fsm_obj_name(pscan->fsm_obj));
	*sz = len;
}

static void scan_debug(void *obj, char input[][MAX_ARGV], u32 input_num,
	char *output, u32 *out_len)
{
	struct scan_obj *pscan = (struct scan_obj *)obj;
	char *ptr = output;
	int len = *out_len;
	int token;

	if (input_num <  2) {
		scan_dbg_help(pscan, ptr, &len);
		goto done;
	}

	if (!_os_strcmp(input[0], "req")) {
		if (!_os_strcmp(input[1], "del")) {

			/* del scan request */
			/* cmd: scan-1 req del <token> */

			/* read token */
			if (_os_sscanf(input[2], "%d", &token) != 1) {
				_os_snprintf(pstr(ptr), lstr(ptr, len),
					"%s Err missing token\n",
					phl_fsm_obj_name(pscan->fsm_obj));
				goto done;
			}

			_os_snprintf(pstr(ptr), lstr(ptr, len),
				"%s del token %d\n",
				phl_fsm_obj_name(pscan->fsm_obj), token);
			rtw_phl_scan_del_request(pscan->phl_info, token);

		} else if (!_os_strcmp(input[1], "pause")) {
			/* cmd: scan-1 req pauase */
			_os_snprintf(pstr(ptr), lstr(ptr, len),
				"%s pause\n",
				phl_fsm_obj_name(pscan->fsm_obj));
			rtw_phl_scan_pause(pscan->phl_info);

		} else if (!_os_strcmp(input[1], "resume")) {
			/* cmd: scan-1 req resume */
			_os_snprintf(pstr(ptr), lstr(ptr, len),
				"%s resume\n",
				phl_fsm_obj_name(pscan->fsm_obj));
			rtw_phl_scan_resume(pscan->phl_info);
		}
	} else
		scan_dbg_help(pscan, ptr, &len);
done:
	*out_len = len;
}

/* For EXTERNAL application to create a scan FSM */
/* @root: FSM root structure
 * @phl: private data structure to invoke hal/phl function
 *
 * return
 * fsm_main: FSM main structure (Do NOT expose)
 */
struct fsm_main *phl_scan_new_fsm(struct fsm_root *root,
	struct phl_info_t *phl_info)
{
	void *d = phl_to_drvpriv(phl_info);
	struct fsm_main *fsm = NULL;
	struct rtw_phl_fsm_tb tb;


	_os_mem_set(d, &tb, 0, sizeof(tb));
	tb.max_state = sizeof(scan_state_tbl)/sizeof(scan_state_tbl[0]);
	tb.max_event = sizeof(scan_event_tbl)/sizeof(scan_event_tbl[0]);
	tb.state_tbl = scan_state_tbl;
	tb.evt_tbl = scan_event_tbl;
	tb.dump_obj = scan_dump_obj;
	tb.dump_fsm = scan_dump_fsm;
	tb.debug = scan_debug;
	tb.dbg_level = FSM_DBG_INFO;
	tb.evt_level = FSM_DBG_INFO;

	fsm = phl_fsm_init_fsm(root, "scan", phl_info, &tb);

	return fsm;
}

/* For EXTERNAL application to destory scan fsm */
/* @fsm: see fsm_main
 */
void phl_scan_destory_fsm(struct fsm_main *fsm)
{
	if (fsm == NULL)
		return;

	/* deinit fsm local variable if has */

	/* call FSM Framewro to deinit fsm */
	phl_fsm_deinit_fsm(fsm);
}

/* For EXTERNAL application to create scan object */
/* @fsm: FSM main structure which created by phl_scan_new_fsm()
 * @phl_info: private data structure of caller
 *
 * return
 * scan_obj: structure of scan object (Do NOT expose)
 */
struct scan_obj *phl_scan_new_obj(struct fsm_main *fsm,
	struct phl_info_t *phl_info)
{
	void *d = phl_to_drvpriv(phl_info);
	struct fsm_obj *obj;
	struct scan_obj *pscan;


	pscan = phl_fsm_new_obj(fsm, (void **)&obj, sizeof(*pscan));

	if (pscan == NULL) {
		/* TODO free fsm; currently will be freed in deinit process */
		FSM_ERR(fsm, "scan: malloc obj fail\n");
		return NULL;
	}
	pscan->fsm = fsm;
	pscan->fsm_obj = obj;
	pscan->phl_info = phl_info;

	/* init obj local use variable */
	pq_init(d, &pscan->req_q);
	pq_init(d, &pscan->off_ch_cmd_q);

	pscan->param = NULL;
	pscan->token_running = 0;
	pscan->off_ch_step = OFF_CH_STEP_CLEAR;

	pscan->back_op_is_required = false;
	pscan->back_op_ch.scan_mode = BACKOP_MODE;
	pscan->back_op_ch.duration = BACK_OP_CH_DUR_MS;

	return pscan;
}

/* For EXTERNAL application to destory scan object */
/* @pscan: local created scan object
 */
void phl_scan_destory_obj(struct scan_obj *pscan)
{
	void *d;
	_os_list *obj;

	if (pscan == NULL)
		return;

	d = phl_to_drvpriv(pscan->phl_info);

	/* deinit and free all local variables */
	while (pq_pop(d, &pscan->req_q, &obj, _first, _bh))
		scan_free_req(pscan, (struct fsm_msg *)obj);
	pq_deinit(d, &pscan->req_q);

	while (pq_pop(d, &pscan->off_ch_cmd_q, &obj, _first, _bh))
		scan_free_msg(pscan, (struct fsm_msg *)obj);
	pq_deinit(d, &pscan->off_ch_cmd_q);

	/* clear scan param */
	if (pscan->param) {
		if (pscan->param && pscan->param->ch && pscan->param->ch_sz)
			_os_kmem_free(d, pscan->param->ch, pscan->param->ch_sz);
		_os_kmem_free(d, pscan->param, sizeof(*pscan->param));
		pscan->param = NULL;
	}

	/* inform FSM framewory to recycle fsm_obj */
	phl_fsm_destory_obj(pscan->fsm_obj);
}

/* For EXTERNAL application to request scan (expose) */
/* @pscan: scan object
 * @pbuf: scan parameter, will be freed by caller after retrun
 * @order: queuing order
 */
enum rtw_phl_status rtw_phl_scan_request(void *phl,
	struct rtw_phl_scan_param *pbuf, enum PRECEDE order)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;
	void *d = phl_to_drvpriv(pscan->phl_info);
	struct rtw_phl_scan_param *param = NULL;
	struct phl_scan_channel *pch = NULL;
	struct fsm_msg *msg;

	pch = (struct phl_scan_channel *)_os_kmem_alloc(d, pbuf->ch_sz);
	if (pch == NULL)
		goto pch_fail;

	param = (struct rtw_phl_scan_param *)_os_kmem_alloc(d, sizeof(*pbuf));
	if (param == NULL)
		goto param_fail;

	/* NEW message to start scan */
	msg = phl_fsm_new_msg(pscan->fsm_obj, SCAN_EV_START);
	if (msg == NULL)
		goto msg_fail;

	/* fill token */
	pbuf->token = scan_get_token(pscan);
	_os_mem_cpy(d, pch, pbuf->ch, pbuf->ch_sz);
	_os_mem_cpy(d, param, pbuf, sizeof(*param));
	param->ch = pch;
	param->ch_idx = -1;
	param->scan_ch = &param->ch[0];
	param->result = SCAN_REQ_ABORT;

	param->enqueue_time = _os_get_cur_time_ms();
	msg->param = param;
	msg->param_sz = sizeof(*param);

	/*
	 * Enqueue scan_request into extra queue (pending scan)
	 */
	switch (order) {
	case IMMEDIATE:
		pq_push(d, &pscan->req_q, (_os_list *)msg, _first, _bh);
		/* Cancel running scan process */
		phl_fsm_gen_msg(phl, pscan->fsm_obj, NULL, 0, FSM_EV_CANCEL);
		/* fall through */
		break;
	case TO_HEAD:
		pq_push(d, &pscan->req_q, (_os_list *)msg, _first, _bh);
		break;
	case TO_TAIL:
	default:
		pq_push(d, &pscan->req_q, (_os_list *)msg, _tail, _bh);
		break;
	}

	/* notify scan-obj to dequeue from extra queue */
	return phl_fsm_gen_msg(phl, pscan->fsm_obj,
		NULL, 0, SCAN_EV_NOTIFY_PENDING_SCAN_REQ);

msg_fail:
	_os_kmem_free(d, param, sizeof(*param));
param_fail:
	_os_kmem_free(d, pch, sizeof(*pch));
pch_fail:
	FSM_ERR(pscan->fsm, "%s: %s() malloc fail\n",
		phl_fsm_obj_name(pscan->fsm_obj), __func__);
	return RTW_PHL_STATUS_RESOURCE;
}

/* For EXTERNAL application to del scan request (expose) */
/* @phl: phl_info_t
 * @token: token to be deleted
 *	   0 means del all requests and cancel running scan
 */
enum rtw_phl_status rtw_phl_scan_del_request(void *phl, u32 token)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;
	void *d = phl_to_drvpriv(phl_info);
	struct fsm_msg *msg, *msg_t;
	struct rtw_phl_scan_param *param = NULL;
	_os_list *obj;

	if (token == ALL_TOKEN) {
		while (pq_pop(d, &pscan->req_q, &obj, _first, _bh)) {
			msg = (struct fsm_msg *)obj;
			param = msg->param;

			FSM_INFO(pscan->fsm, "%s abort token %d\n",
				phl_fsm_obj_name(pscan->fsm_obj), param->token);

			if (param && param->ops && param->ops->scan_complete)
				param->ops->scan_complete(param->priv, param);
			scan_free_req(pscan, (struct fsm_msg *)obj);
		}

	} else {

		/* search token in scan request queue */
		_os_spinlock(d, &pscan->req_q.lock, _bh, NULL);
		phl_list_for_loop_safe(msg, msg_t,
			struct fsm_msg, &pscan->req_q.queue, list) {

			param = (struct rtw_phl_scan_param *)msg->param;
			if (param && param->token == token) {
				list_del(&msg->list);
				pscan->req_q.cnt--;
				_os_spinunlock(d, &pscan->req_q.lock, _bh, NULL);

				FSM_INFO(pscan->fsm, "%s abort token %d\n",
					phl_fsm_obj_name(pscan->fsm_obj),
					token);

				/* callback to caller */
				if (param->ops && param->ops->scan_complete)
					param->ops->scan_complete(param->priv,
						param);
				scan_free_req(pscan, msg);
				return RTW_PHL_STATUS_SUCCESS;
			}
		}
		_os_spinunlock(d, &pscan->req_q.lock, _bh, NULL);

		FSM_INFO(pscan->fsm, "%s del token %d not found\n",
			phl_fsm_obj_name(pscan->fsm_obj), token);
	}

	if ((pscan->param) &&
		((pscan->param->token == token) || ALL_TOKEN == token)) {
		/* we have a running scan, cancel it */
		FSM_INFO(pscan->fsm, "%s cancel running scan token %d\n",
			phl_fsm_obj_name(pscan->fsm_obj), pscan->param->token);

		phl_fsm_cancel_obj(pscan->fsm_obj);
	}
	return RTW_PHL_STATUS_SUCCESS;
}

/* For EXTERNAL application to cancel scan (expose) */
/* @pscan: scan object will be cancelled
 */
enum rtw_phl_status rtw_phl_scan_cancel(void *phl)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;

	return phl_fsm_cancel_obj(pscan->fsm_obj);
}

/* For EXTERNAL application to pause scan (expose)
 * @phl: phl pirvate
 */
enum rtw_phl_status rtw_phl_scan_pause(void *phl)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;

	return phl_fsm_gen_msg(phl, pscan->fsm_obj, NULL, 0, SCAN_EV_PAUSE);
}

/* For EXTERNAL application to resume scan (expose)
 * @phl: phl pirvate
 */
enum rtw_phl_status rtw_phl_scan_resume(void *phl)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;

	return phl_fsm_gen_msg(phl, pscan->fsm_obj, NULL, 0, SCAN_EV_RESUME);
}

/* For EXTERNAL application to change passive scan to active scan (expose)
 * @phl: phl pirvate
 * @ch: channel to be changed to active scan
 * @duration: scan time for the channel; 0: half of original duration
 *
 * Usually triggered when receiving Beacon in a passive scan channel.
 * Changing current scan ch from passive to active scan.
 * (issue probe request in current scan channel)
 * More easy to find a hidden ssid AP in passive scan channel.
 */
enum rtw_phl_status rtw_phl_scan_force_active_scan(void *phl,
	u16 channel, u16 duration)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;
	struct phl_scan_channel ch;

	_os_mem_set(phl_to_drvpriv(phl_info), &ch, 0, sizeof(ch));
	ch.channel = channel;
	ch.duration = duration;

	return phl_fsm_gen_msg(phl, pscan->fsm_obj, &ch,
		sizeof(ch), SCAN_EV_FORCE_ACTIVE);
}

/* For EXTERNAL application to get inprogress scan and requests num (expose)
 * @phl: phl pirvate
 *
 * return: 0: no scan is inprogressing
 * otherwise: number of queuing scan requests + inprogress scan
 */
int rtw_phl_scan_inprogress_req_num(void *phl)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;
	u8 running = 0;

	if (pscan->param != NULL)
		running = 1;

	return pscan->req_q.cnt + running;
}

bool rtw_phl_scan_scanning_req(void *phl, u32 token, u32 *token_running)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;

	*token_running = pscan->token_running;
	return (token == *token_running);
}

enum rtw_phl_status rtw_phl_scan_off_ch_tx(void *phl, void *data, int len)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct scan_obj *pscan = phl_info->scan_obj;
	struct rtw_phl_scan_param *param = pscan->param;
	void *d = phl_to_drvpriv(pscan->phl_info);
	struct fsm_msg *msg;
	void *buf;

	if (!param || !param->ops || !param->ops->scan_off_ch_tx)
		return RTW_PHL_STATUS_FAILURE;

	buf = _os_kmem_alloc(d, len);
	if (buf == NULL)
		goto buf_fail;

	msg = phl_fsm_new_msg(pscan->fsm_obj, SCAN_EV_NOTIFY_OFF_CH_CMD);
	if (msg == NULL)
		goto msg_fail;

	_os_mem_cpy(d, buf, data, len);
	msg->param = buf;
	msg->param_sz = len;
	pq_push(d, &pscan->off_ch_cmd_q, (_os_list *)msg, _tail, _bh);

	FSM_INFO(pscan->fsm, "%s %s(), q_cnt = %d\n",
		phl_fsm_obj_name(pscan->fsm_obj), __func__,
		pscan->off_ch_cmd_q.cnt);

	/* notify scan-obj there is an off_ch_cmd */
	return phl_fsm_gen_msg(phl, pscan->fsm_obj,
		NULL, 0, SCAN_EV_NOTIFY_OFF_CH_CMD);
msg_fail:
	_os_kmem_free(d, buf, len);

buf_fail:
	FSM_ERR(pscan->fsm, "%s %s() malloc fail\n",
		phl_fsm_obj_name(pscan->fsm_obj), __func__);
	return RTW_PHL_STATUS_RESOURCE;
}
#endif /*CONFIG_FSM*/

