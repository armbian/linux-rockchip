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
#define _PHL_CMD_SCAN_C_
#include "phl_headers.h"
#include "phl_scan.h"

#define param_to_phlcom(_param) 	(_param->wifi_role->phl_com)

#ifdef CONFIG_PHL_CMD_SCAN


enum _CMD_SCAN_STATE {
	CMD_SCAN_ACQUIRE = BIT0,
	CMD_SCAN_STARTED = BIT1,
	CMD_SCAN_DF_IO = BIT2, /* Disable Function : IO */
};

static void _cmd_scan_timer(void* role);

#define DBG_SCAN_CHAN_DUMP

#ifdef DBG_SCAN_CHAN_DUMP
static void
_cmd_estimated_swch_seq(struct rtw_phl_scan_param *param, u8 op_num)
{
	u8 chidx = 0;
	u8 opidx = 0;
	u8 total_ch_num = 0;

	if ((param->back_op_mode == SCAN_BKOP_CNT) &&
		(param->back_op_ch_cnt == 0)) {
		PHL_ERR("%s bkop_cnt == 0\n", __func__);
		_os_warn_on(1);
		return;
	}

	/*swicth channel sequence by cmd_scan's estimated */
	PHL_INFO("%s:: Estimated channel sequence:\n", __func__);

	if (param->back_op_mode == SCAN_BKOP_CNT) {
		PHL_INFO("[SCAN_BKOP_CNT]\n");
		for(chidx = 0; chidx < param->ch_num; chidx++) {
			PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "%3d, ", param->ch[chidx].channel);
			total_ch_num++;
			if(!((chidx + 1) % param->back_op_ch_cnt)) {
				if (op_num) {
					for(opidx = 0; opidx < op_num; opidx++) {
						PHL_DATA(COMP_PHL_DBG, _PHL_INFO_,
							"[%3d], ", param->back_op_ch[opidx].channel);
						total_ch_num++;
					}
				}
			}
			if(!((chidx + 1) % (param->back_op_ch_cnt * 2)))
				PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "\n");
		}
		PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "\n");

	}
	#ifdef CONFIG_PHL_CMD_SCAN_BKOP_TIME
	else if (param->back_op_mode == SCAN_BKOP_TIMER) {
		u16 ch_dur;

		PHL_INFO("[SCAN_BKOP_TIMER]\n");
		for(chidx = 0; chidx < param->ch_num; chidx++) {
			total_ch_num++;
			ch_dur = (op_num) ? param->back_op_off_ch_dur_ms : param->ch[chidx].duration;
			PHL_INFO("\t%3d, dur:%d(ms)\n", param->ch[chidx].channel, ch_dur);
		}
		if (op_num) {
			for(opidx = 0; opidx < op_num; opidx++) {
				total_ch_num++;
				PHL_INFO("\t[%3d], dur:%d(ms)\n",
					param->back_op_ch[opidx].channel, param->back_op_ch[opidx].duration);
			}
		}
		PHL_INFO("max_listen_time:%d (ms)\n", param->max_listen_time);
		PHL_INFO("op_ch_dur_ms:%d, off_ch_dur_ms:%d, off_ch_ext_dur_ms:%d (ms)",
			param->back_op_ch_dur_ms,
			param->back_op_off_ch_dur_ms,
			param->back_op_off_ch_ext_dur_ms);
	}
	#endif
	else if (param->back_op_mode == SCAN_BKOP_NONE) {
		PHL_INFO("[SCAN_BKOP_NONE]\n");
		for(chidx = 0; chidx < param->ch_num; chidx++) {
			PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "%3d, ", param->ch[chidx].channel);
			total_ch_num++;
			if(!((chidx + 1) % 6))
				PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "\n");
		}
		PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "\n");
	}

	PHL_INFO("Scan chan num:%d , Total num:%d, repeat:%d\n",
		param->ch_num, total_ch_num, param->repeat);
	PHL_INFO("--------\n");
}
#endif /*DBG_SCAN_CHAN_DUMP*/

static void
_cmd_scan_update_chlist(void *drv, struct rtw_phl_scan_param *param)
{
	u8 idx = 0;

	INIT_LIST_HEAD(&param->chlist.queue);
	for(idx = 0; idx < param->ch_num; idx++) {
		INIT_LIST_HEAD(&param->ch[idx].list);
		pq_push(drv, &param->chlist, &param->ch[idx].list, _tail, _ps);
	}
}

/*
 * Insert op channel list
 * Ex.	ch_intvl =3
 * ch_idx : -1, 0, 1, 2,   3, 4, 5, 6
 *					   ^		  ^
 *					   op0~op5	  op0~op5
 *
 * => 0,1,2,[op0],[op1],3,4,5,[op0],[op1],6,7,8,[op0],[op1]
 */

static inline void
_cmd_scan_enqueue_opch(void *drv, struct rtw_phl_scan_param *param, struct phl_queue *q)
{
	u8 idx = 0;

	for(idx = 0; idx < MAX_WIFI_ROLE_NUMBER; idx ++) {
		if(param->back_op_ch[idx].channel)
			pq_push(drv, &param->chlist, &param->back_op_ch[idx].list, _first, _ps);
		else
			break;
	}
}
static struct phl_scan_channel *_cmd_scan_select_chnl(
	void *drv, struct rtw_phl_scan_param *param)
{
	struct phl_scan_channel *scan_ch = NULL;
	_os_list* obj = NULL;
	bool back_op_is_required = (param->back_op_ch[0].channel)? true:false;

next_ch:
	if(pq_pop(drv, &param->chlist, &obj, _first, _ps)) {
		scan_ch = (struct phl_scan_channel*)obj;

		if(scan_ch->scan_mode == NORMAL_SCAN_MODE) {
			/* 1- enable,  2- BK_CNT mode, 3- prev is non-op, 4- ch_intvl's turn */
			if (back_op_is_required && param->back_op_mode == SCAN_BKOP_CNT) {
				if(param->scan_ch &&
				   !((param->ch_idx + 1) % param->back_op_ch_cnt)) {
					_cmd_scan_enqueue_opch(drv, param, &param->chlist);
				}
			}
			param->ch_idx++;
		}
		#ifdef CONFIG_PHL_CMD_SCAN_BKOP_TIME
		else if (scan_ch->scan_mode == P2P_LISTEN_MODE) {
			if (back_op_is_required && param->back_op_mode == SCAN_BKOP_TIMER) {
				scan_ch->duration = param->back_op_off_ch_dur_ms;
				_cmd_scan_enqueue_opch(drv, param, &param->chlist);
			}
			param->ch_idx++;
		}
		#endif
		param->scan_ch = scan_ch;
	}
	else if(param->repeat > 0) {
		_cmd_scan_update_chlist(drv, param);
		param->ch_idx = 0;
		/* 255 means loop forever */
		if (param->repeat != 255)
			param->repeat--;
		goto next_ch;
	}
	else {
		return NULL;
	}

	PHL_INFO("%s: repeat[%d] ch_idx=[%d/%d], ch_number=%d, scan_mode= %s\n", __func__,
		 param->repeat, param->ch_idx, param->ch_num, param->scan_ch->channel,
		(param->scan_ch->scan_mode == BACKOP_MODE)? "OP_CH": "Non-OP");

	return param->scan_ch;
}

/* Notification complete */
void _cmd_scan_timer_notify_cb(
	void *role, struct phl_msg *msg)
{
	if (IS_MSG_CANNOT_IO(msg->msg_id)) {
		PHL_ERR("%s: SWCH_DONE failure by CANNOT IO\n", __func__);
		return;
	}

	if (IS_MSG_CANCEL(msg->msg_id)) {
		/* Last event occured MSG_STATUS_PENDING */
		PHL_ERR("%s: SWCH_DONE pending or abort Occurred!\n", __func__);

		_cmd_scan_timer(role);
	}
}

static void _cmd_scan_timer(void* role)
{
	struct rtw_wifi_role_t *wifi_role = (struct rtw_wifi_role_t *)role;
	struct rtw_phl_com_t *phl_com = wifi_role->phl_com;
	struct phl_info_t *phl_info = phl_com->phl_priv;
	u8 band_idx = wifi_role->hw_band;

	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};

	PHL_INFO("%s \n", __func__);

	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_FG_MDL_SCAN);
/** When listen state of each channel entry expired,
 * timer callback send MSG_EVT_LISTEN_STATE_EXPIRE for additional condition check
 * If nothing specitail occured, then send MSG_EVT_SWCH_START to proceed for the rest of channel list
 * therefore, additional process delay for MSG_EVT_LISTEN_STATE_EXPIRE would prolong listen period
 * */
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_LISTEN_STATE_EXPIRE);
	msg.rsvd[0] = (u8*)role;
	msg.band_idx = band_idx;

	attr.completion.completion = _cmd_scan_timer_notify_cb;
	attr.completion.priv = role;

	if(phl_disp_eng_send_msg(phl_info, &msg, &attr, NULL) != RTW_PHL_STATUS_SUCCESS)
		PHL_ERR("%s: [SCAN_TIMER] phl_disp_eng_send_msg failed !\n", __func__);
}

/* Notification complete */
void _cmd_swch_done_notify_cb(
	void *drv, struct phl_msg *msg)
{
	if (msg->inbuf) {
		_os_mem_free(drv, msg->inbuf, msg->inlen);
	}
}

enum rtw_phl_status _cmd_swch_done_notify(
	void *dispr, void *drv, struct rtw_phl_scan_param *param)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	struct phl_scan_channel *scan_ch = param->scan_ch;
	struct rtw_phl_com_t *phl_com = param_to_phlcom(param);
	struct phl_info_t *phl = (struct phl_info_t *) phl_com->phl_priv;
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	u8 *info = NULL;
	u8 idx = 0xff;

	info = _os_mem_alloc(drv, sizeof(struct phl_scan_channel));
	if (info == NULL) {
		PHL_ERR("%s: [SWCH_DONE] alloc buffer failed!\n", __func__);
		return RTW_PHL_STATUS_FAILURE;
	}

	_os_mem_cpy(drv, info, scan_ch, sizeof(*scan_ch));

	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_FG_MDL_SCAN);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_SWCH_DONE);

	attr.opt = MSG_OPT_CLR_SNDR_MSG_IF_PENDING;
	attr.completion.completion = _cmd_swch_done_notify_cb;
	attr.completion.priv = drv;

	msg.inbuf = info;
	msg.inlen = sizeof(struct phl_scan_channel);
	msg.rsvd[0] = (u8*)param->wifi_role;
	phl_dispr_get_idx(dispr, &idx);
	msg.band_idx = idx;
	pstatus = phl_disp_eng_send_msg(phl, &msg, &attr, NULL);
	if(pstatus != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s: [SWCH_DONE] phl_disp_eng_send_msg failed!\n", __func__);
		_os_mem_free(drv, info, sizeof(struct phl_scan_channel));
	}
	return pstatus;
}

void _cmd_scan_end(
	void *drv, struct rtw_phl_scan_param *param)
{
	struct rtw_wifi_role_t *wifi_role = param->wifi_role;
	struct rtw_phl_com_t *phl_com = wifi_role->phl_com;
	struct phl_info_t *phl_info = phl_com->phl_priv;

	PHL_INFO("_cmd_scan_end \n");

	param->end_time = _os_get_cur_time_ms();

	/* dump scan time */
	param->total_scan_time =
		phl_get_passing_time_ms(param->enqueue_time);

	pq_deinit(drv, &param->chlist);

	/* acquire state */
	_os_cancel_timer(drv, &param->scan_timer);
	_os_release_timer(drv, &param->scan_timer);

	if(TEST_STATUS_FLAG(param->state, CMD_SCAN_STARTED) &&
	   !TEST_STATUS_FLAG(param->state, CMD_SCAN_DF_IO) )
	{
		rtw_hal_com_scan_restore_tx_lifetime(phl_info->hal, wifi_role->hw_band);

		rtw_hal_scan_set_rxfltr_by_mode(phl_info->hal, wifi_role->hw_band,
						false, &param->fltr_mode);
		rtw_hal_scan_pause_tx_fifo(phl_info->hal, wifi_role->hw_band, false);

		rtw_hal_notification(phl_info->hal, MSG_EVT_SCAN_END, wifi_role->hw_band);
	}

	if (param->ops->scan_complete)
		param->ops->scan_complete(param->priv, param);
}

/* Notification complete */
void _cmd_abort_notify_cb(
	void *drv, struct phl_msg *msg)
{
	struct rtw_phl_scan_param *param = (struct rtw_phl_scan_param *)msg->inbuf;

	if(IS_MSG_CANNOT_IO(msg->msg_id))
		SET_STATUS_FLAG(param->state, CMD_SCAN_DF_IO);

	_cmd_scan_end(drv, param);
}

void _cmd_abort_notify(void *dispr, void *drv,
	struct rtw_phl_scan_param *param, bool abort)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	struct rtw_phl_com_t *phl_com = param_to_phlcom(param);
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	struct phl_info_t *phl = (struct phl_info_t *) phl_com->phl_priv;
	u8 idx = 0xff;

	if(TEST_STATUS_FLAG(param->state, CMD_SCAN_ACQUIRE))
	{
		param->result = SCAN_REQ_CANCEL;

		SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_FG_MDL_SCAN);
		SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_SCAN_END);

		if(abort)
			attr.opt = MSG_OPT_SEND_IN_ABORT;
		attr.completion.completion = _cmd_abort_notify_cb;
		attr.completion.priv = drv;

		msg.inbuf = (u8*)param;
		msg.rsvd[0] = (u8*)param->wifi_role;
		#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
		msg.rsvd[1] = (u8*)param->ops->scan_issue_null_data;
		#endif

		phl_dispr_get_idx(dispr, &idx);
		msg.band_idx = idx;
		pstatus = phl_disp_eng_send_msg(phl, &msg, &attr, NULL);
		if (RTW_PHL_STATUS_SUCCESS != pstatus) {
			/* (1) dispr_stop
			   (2) idle msg empty .*/

			PHL_ERR("%s :: [Abort] dispr_send_msg failed (0x%X)\n",
				__func__, pstatus);

			if(pstatus == RTW_PHL_STATUS_UNEXPECTED_ERROR ||
			   TEST_STATUS_FLAG(phl_com->dev_state, RTW_DEV_SURPRISE_REMOVAL)) {
				/* clean sw resource only */
				/* (1) driver is going to unload */
				/* (2) Supprise remove */
				SET_STATUS_FLAG(param->state, CMD_SCAN_DF_IO);
			}
			_cmd_abort_notify_cb(drv, &msg);
		}
	}
	else {
		param->result = SCAN_REQ_ABORT;
		pq_deinit(drv, &param->chlist);

		if (param->ops->scan_complete)
			param->ops->scan_complete(param->priv, param);
	}
}

enum phl_mdl_ret_code _cmd_scan_fail_ev_hdlr(
	void* dispr, void* priv, struct phl_msg* msg)
{
	struct rtw_phl_scan_param *param = (struct rtw_phl_scan_param*)priv;
	struct rtw_wifi_role_t *wifi_role = param->wifi_role;
	struct rtw_phl_com_t *phl_com = wifi_role->phl_com;
	struct phl_info_t *phl_info = phl_com->phl_priv;
	void *d = phlcom_to_drvpriv(phl_com);
	u8 idx = 0xff;
	struct phl_msg nextmsg = {0};
	struct phl_msg_attribute attr = {0};
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;

	SET_MSG_MDL_ID_FIELD(nextmsg.msg_id, PHL_FG_MDL_SCAN);
	phl_dispr_get_idx(dispr, &idx);
	nextmsg.band_idx = idx;
	switch(MSG_EVT_ID_FIELD(msg->msg_id)) {
		case MSG_EVT_SCAN_START:
			/* fall through */
		case MSG_EVT_SWCH_START:
			/* fall through */
		case MSG_EVT_SWCH_DONE:
			PHL_INFO("SCAN_START/SWCH_START/SWCH_DONE:: failed/timeout handler \n");

			SET_MSG_EVT_ID_FIELD(nextmsg.msg_id, MSG_EVT_SCAN_END);
			nextmsg.rsvd[0] = (u8*)param->wifi_role;
			#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
			nextmsg.rsvd[1] = (u8*)param->ops->scan_issue_null_data;
			#endif
			pstatus = phl_disp_eng_send_msg(phl_info, &nextmsg, &attr, NULL);
			if(pstatus != RTW_PHL_STATUS_SUCCESS)
				PHL_ERR("%s :: [SWCH_START][1] phl_disp_eng_send_msg failed\n", __func__);
		break;

		case MSG_EVT_SCAN_END:
			// free token
			// release timer
			PHL_INFO("MSG_EVT_SCAN_END:: failed/timeout handler \n");
			pstatus = phl_disp_eng_free_token(phl_info, idx, &param->token);
			if(pstatus == RTW_PHL_STATUS_SUCCESS)
			{
				if(IS_MSG_CANNOT_IO(msg->msg_id))
					SET_STATUS_FLAG(param->state, CMD_SCAN_DF_IO);

				_cmd_scan_end(d, param);
			}
		break;

		default:
			/* unknown state */
		break;
	}

	return MDL_RET_SUCCESS;
}

enum phl_mdl_ret_code _cmd_scan_hdl_external_evt(
	void* dispr, void* priv, struct phl_msg* msg)
{
	PHL_DBG("%s :: From others MDL =%d , EVT_ID=%d\n", __func__,
		 MSG_MDL_ID_FIELD(msg->msg_id), MSG_EVT_ID_FIELD(msg->msg_id));
	return MDL_RET_IGNORE;
}

u8 _cmd_chk_ext_act_scan(struct rtw_phl_scan_param *param)
{
#ifdef RTW_WKARD_CMD_SCAN_EXTEND_ACTIVE_SCAN
	/** suppose to query the time of last recieved beacon in current channel here
	 * then change state to EXT_ACT_SCAN_TRIGGER if needed
	 * but, PHL does not store bss list at the moment, therefore,
	 * core layer use set_info (FG_REQ_OP_NOTIFY_BCN_RCV) to notify scan module incoming bcn
	 * and change state to EXT_ACT_SCAN_TRIGGER accordingly.
	*/
#endif
	if (param->scan_ch->type == RTW_PHL_SCAN_PASSIVE &&
	    param->scan_ch->ext_act_scan == EXT_ACT_SCAN_TRIGGER) {
		if (param->ops->scan_issue_pbreq)
			param->ops->scan_issue_pbreq(param->priv, param);
		param->scan_ch->ext_act_scan = EXT_ACT_SCAN_DONE;
		return true;
	}
	return false;
}

void
_cmd_scan_start(struct phl_info_t *phl_info,
		struct rtw_wifi_role_t *wifi_role,
		struct rtw_phl_scan_param *param)
{
	rtw_hal_scan_pause_tx_fifo(phl_info->hal, wifi_role->hw_band, true);
	rtw_hal_scan_set_rxfltr_by_mode(phl_info->hal, wifi_role->hw_band,
					true, &param->fltr_mode);
	rtw_hal_com_scan_set_tx_lifetime(phl_info->hal, wifi_role->hw_band);
}

enum phl_mdl_ret_code _cmd_scan_hdl_internal_evt(
	void* dispr, void* priv, struct phl_msg* msg)
{
	struct rtw_phl_scan_param *param = (struct rtw_phl_scan_param*)priv;
	struct rtw_wifi_role_t *wifi_role = param->wifi_role;
	struct rtw_phl_com_t *phl_com = wifi_role->phl_com;
	struct phl_info_t *phl_info = phl_com->phl_priv;
	void *d = phlcom_to_drvpriv(phl_com);
	u32 diff_time = 0;

	struct phl_msg nextmsg = {0};
	struct phl_msg_attribute attr = {0};
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	u8 idx = 0xff;
	struct phl_scan_channel *scan_ch = NULL;
	bool tx_pause = true;
	struct rtw_chan_def chdef = {0};

	phl_dispr_get_idx(dispr, &idx);
	diff_time = phl_get_passing_time_ms(param->enqueue_time);

	if (param->max_scan_time && diff_time >= param->max_scan_time) {
		PHL_WARN("%s:: Timeout! %d > max_time %d\n",
				 __func__, diff_time, param->max_scan_time);

		/* Abort scan request */
		/* Based on [CN3AXSW-552]
		 * ex. max_scan_time = 4sec
		 * Usb dongle would abort scan_req in 29~33th chnl
		 * If chnllist insert op-chnl, scan_req would be aborted in 21~23th chnl.
		 * It means that usb dongle always can't do fully scan.
		 * So, abort scan_req or not, depend on core layer.
		*/
		/*_cmd_scan_fail_ev_hdlr(dispr, priv, msg);
		return MDL_RET_FAIL; */
	}
	else {
		PHL_INFO("%s:: TimeIntvl: %u \n", __func__, diff_time);
	}

	#ifdef CONFIG_PHL_CMD_SCAN_BKOP_TIME
	if (param->max_listen_time && diff_time >= param->max_listen_time) {
		PHL_WARN("%s:: Timeout! %d > max_listen_time %d\n",
				 __func__, diff_time, param->max_listen_time);
		#if 0
		_cmd_scan_fail_ev_hdlr(dispr, priv, msg);
		return MDL_RET_FAIL;
		#endif
	}
	#endif

	SET_MSG_MDL_ID_FIELD(nextmsg.msg_id, PHL_FG_MDL_SCAN);
	nextmsg.band_idx = idx;

	switch(MSG_EVT_ID_FIELD(msg->msg_id)) {
		case MSG_EVT_SCAN_START:
			_cmd_scan_start(phl_info, wifi_role, param);
			rtw_hal_notification(phl_info->hal, MSG_EVT_SCAN_START,
					     wifi_role->hw_band);

			/* [scan start notify] */
			if (param->ops->scan_start)
				param->ops->scan_start(param->priv, param);

			SET_STATUS_FLAG(param->state, CMD_SCAN_STARTED);

			SET_MSG_EVT_ID_FIELD(nextmsg.msg_id, MSG_EVT_SWCH_START);
			nextmsg.rsvd[0] = (u8*)wifi_role;
			pstatus = phl_disp_eng_send_msg(phl_info, &nextmsg, &attr, NULL);
			if(pstatus != RTW_PHL_STATUS_SUCCESS)
				PHL_ERR("%s :: [SCAN_START] phl_disp_eng_send_msg failed\n", __func__);
		break;
		case MSG_EVT_LISTEN_STATE_EXPIRE:
			if (_cmd_chk_ext_act_scan(param)) {
				_os_set_timer(d, &param->scan_timer, param->ext_act_scan_period);
				PHL_INFO("%s :: extend listen state of ch %d by %d ms, and reset timer\n",
					__func__, param->scan_ch->channel, param->ext_act_scan_period);
				break;
			}
			SET_MSG_EVT_ID_FIELD(nextmsg.msg_id, MSG_EVT_SWCH_START);
			nextmsg.rsvd[0] = (u8*)wifi_role;

			pstatus = phl_disp_eng_send_msg(phl_info, &nextmsg, &attr, NULL);
			if(pstatus != RTW_PHL_STATUS_SUCCESS)
				PHL_ERR("%s :: [LISTEN_STATE_EXPIRE] dispr_send_msg failed\n", __func__);

		break;

		case MSG_EVT_SWCH_START:
			/*	ycx++
				ycx > length(yclist) ? SCAN_EV_END : switch channel */

			PHL_INFO("MSG_EVT_SWCH_START \n");

			/* For the first time, param->scan_ch would be NULL */
			/* Current channel scan_mode */
			if (param->scan_ch && param->scan_ch->scan_mode == BACKOP_MODE) {
				tx_pause = false;
			}

			scan_ch = _cmd_scan_select_chnl(d, param);
			if (scan_ch == NULL) {
				/* no more channel, we are done */
				SET_MSG_EVT_ID_FIELD(nextmsg.msg_id, MSG_EVT_SCAN_END);
				nextmsg.rsvd[0] = (u8*)param->wifi_role;
				#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
				nextmsg.rsvd[1] = (u8*)param->ops->scan_issue_null_data;
				#endif
				pstatus = phl_disp_eng_send_msg(phl_info, &nextmsg, &attr, NULL);
				if(pstatus != RTW_PHL_STATUS_SUCCESS)
					PHL_ERR("%s :: [SWCH_START][1] dispr_send_msg failed\n", __func__);
				break;
			}

			/* Next channel scan_mode */
			if ((scan_ch->scan_mode != BACKOP_MODE) && !tx_pause) {
				/* Tx pause */
				rtw_hal_scan_pause_tx_fifo(phl_info->hal, wifi_role->hw_band, true);
				tx_pause = true;
			}
			chdef.band = scan_ch->band;
			chdef.chan = (u8)scan_ch->channel;
			chdef.bw = scan_ch->bw;
			chdef.offset = scan_ch->offset;

			phl_set_ch_bw(wifi_role, &chdef, false);

			if ((scan_ch->scan_mode != BACKOP_MODE) &&
			    (scan_ch->type == RTW_PHL_SCAN_ACTIVE)) {
			    /* Notify RF to do tssi backup */
				rtw_hal_notification(phl_info->hal, MSG_EVT_SWCH_START, wifi_role->hw_band);
				if (param->ops->scan_issue_pbreq)
					param->ops->scan_issue_pbreq(param->priv, param);
			}

			if ((scan_ch->scan_mode == BACKOP_MODE) && tx_pause) {
				/* Tx un-pause */
				rtw_hal_scan_pause_tx_fifo(phl_info->hal, wifi_role->hw_band, false);
			}

			_os_set_timer(d, &param->scan_timer, scan_ch->duration);
			#ifdef DBG_SCAN_CHAN_DUMP
			PHL_INFO("[SCAN] band:%d chan:%d bw:%d offset:%d duration:%d (ms)\n",
				scan_ch->band, scan_ch->channel, scan_ch->bw,
					scan_ch->offset, scan_ch->duration);
			#endif

			pstatus = _cmd_swch_done_notify(dispr, d, param);
		break;

		case MSG_EVT_SWCH_DONE:
			if (param->ops->scan_ch_ready)
				param->ops->scan_ch_ready(param->priv, param);

			PHL_INFO("MSG_EVT_SWCH_DONE :: duration=%d\n", param->scan_ch->duration);
		break;

		case MSG_EVT_SCAN_END:
			PHL_INFO("MSG_EVT_SCAN_END \n");
			pstatus = phl_disp_eng_free_token(phl_info, idx, &param->token);
			if(pstatus == RTW_PHL_STATUS_SUCCESS) {

				param->result = SCAN_REQ_COMPLETE;

				_cmd_scan_end(d, param);
			}
			else
				PHL_WARN("%s :: [SCAN_END] Abort occurred, skip!\n", __func__);

		break;

		default:
			/* unknown state */
		break;
	}

	return MDL_RET_SUCCESS;
}


enum phl_mdl_ret_code _phl_cmd_scan_req_acquired(
	void* dispr, void* priv)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	struct rtw_phl_scan_param *param = (struct rtw_phl_scan_param*)priv;
	struct rtw_phl_com_t *phl_com = param_to_phlcom(param);
	void *d = phlcom_to_drvpriv(phl_com);
	u32 diff_time = 0;
	struct phl_info_t *phl_info = phl_com->phl_priv;
	u8 idx = 0xff;

	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};

	FUNCIN();

	param->start_time = _os_get_cur_time_ms();

	/* check max scan time */
	if (param->max_scan_time > 0) {
		diff_time = phl_get_passing_time_ms(param->enqueue_time);

		if (diff_time >= param->max_scan_time) {
			PHL_WARN("%s:: Timeout! %u > max_time %d\n",
				 __func__, diff_time, param->max_scan_time);
			goto error;
		}
	}

	_os_init_timer(d, &param->scan_timer, _cmd_scan_timer,
		       param->wifi_role, "phl_cmd_scan_req_timer");

	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_FG_MDL_SCAN);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_SCAN_START);
	msg.rsvd[0] = (u8*)param->wifi_role;
	phl_dispr_get_idx(dispr, &idx);
	msg.band_idx = idx;
	pstatus = phl_disp_eng_send_msg(phl_info, &msg, &attr, NULL);

	if(pstatus != RTW_PHL_STATUS_SUCCESS) {
		_os_release_timer(d, &param->scan_timer);
		goto error;
	}
	else {
		SET_STATUS_FLAG(param->state, CMD_SCAN_ACQUIRE);
		return MDL_RET_SUCCESS;
	}

error:
	_cmd_abort_notify(dispr, d, param, false);
	return MDL_RET_FAIL;
}

enum phl_mdl_ret_code _phl_cmd_scan_req_abort(
	void* dispr, void* priv)
{
	struct rtw_phl_scan_param *param = (struct rtw_phl_scan_param*)priv;
	struct rtw_phl_com_t *phl_com = param_to_phlcom(param);
	void *d = phlcom_to_drvpriv(phl_com);

	PHL_INFO("_phl_cmd_scan_req_abort \n");
	_cmd_abort_notify(dispr, d, param, true);
	return MDL_RET_SUCCESS;
}

enum phl_mdl_ret_code _phl_cmd_scan_req_ev_hdlr(
	void* dispr, void* priv,
	struct phl_msg* msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;

	if(IS_MSG_FAIL(msg->msg_id)) {
		PHL_INFO("%s :: MSG(%d)_FAIL - EVT_ID=%d \n", __func__,
			 MSG_MDL_ID_FIELD(msg->msg_id), MSG_EVT_ID_FIELD(msg->msg_id));

		_cmd_scan_fail_ev_hdlr(dispr, priv, msg);
		return MDL_RET_FAIL;
	}

	switch(MSG_MDL_ID_FIELD(msg->msg_id)) {
		case PHL_FG_MDL_SCAN:
			ret = _cmd_scan_hdl_internal_evt(dispr, priv, msg);
			break;

		default:
			ret = _cmd_scan_hdl_external_evt(dispr, priv, msg);
			break;
	}
	return ret;
}

enum phl_mdl_ret_code _phl_cmd_scan_req_set_info(
	void* dispr, void* priv, struct phl_module_op_info* info)
{
	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;

#ifdef RTW_WKARD_CMD_SCAN_EXTEND_ACTIVE_SCAN
	switch(info->op_code) {
		case FG_REQ_OP_NOTIFY_BCN_RCV:
		{
			struct rtw_phl_scan_param *param = (struct rtw_phl_scan_param*)priv;
			u16 channel = 0;

			/* this workaround might have race condition with background thread*/
			channel = *(u8*)info->inbuf;
			if (param->scan_ch &&
			    param->scan_ch->channel == channel &&
			    param->scan_ch->ext_act_scan == EXT_ACT_SCAN_ENABLE) {
				param->scan_ch->ext_act_scan = EXT_ACT_SCAN_TRIGGER;
				PHL_INFO(" %s :: channel %d extend for active scan\n", __func__, channel);
			}
			if (param->scan_ch &&
			    param->scan_ch->channel != channel)
			    PHL_INFO(" %s :: channel %d mismatch from listen channel %d\n", __func__, channel, param->scan_ch->channel);
			ret = MDL_RET_SUCCESS;
		}
			break;
		default:
			break;
	}
#endif
	/* PHL_INFO(" %s :: info->op_code=%d \n", __func__, info->op_code); */
	return ret;
}

enum phl_mdl_ret_code _phl_cmd_scan_req_query_info(
	void* dispr, void* priv, struct phl_module_op_info* info)
{
	struct rtw_phl_scan_param *param = (struct rtw_phl_scan_param*)priv;
	struct rtw_phl_com_t *phl_com = param_to_phlcom(param);
	void *d = phlcom_to_drvpriv(phl_com);
	u8 ucInfo = 0;
	void* pInfo = NULL;

	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;
	/* PHL_INFO(" %s :: info->op_code=%d \n", __func__, info->op_code); */

	switch(info->op_code) {
		case FG_REQ_OP_GET_ROLE:
			info->outbuf = (u8*)param->wifi_role;
			ret = MDL_RET_SUCCESS;
			break;

		case FG_REQ_OP_GET_MDL_ID:
			ucInfo= PHL_FG_MDL_SCAN;
			pInfo = (void*) &ucInfo;
			info->outlen=1;
			_os_mem_cpy(d, (void*)info->outbuf, pInfo, info->outlen);
			ret = MDL_RET_SUCCESS;
			break;
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
		case FG_REQ_OP_GET_SCAN_PARAM:
			info->outbuf = (u8*)param;
			ret = MDL_RET_SUCCESS;
			break;
#endif
		default:
			break;
	}

	return ret;
}

#ifdef CONFIG_PHL_CMD_SCAN_BKOP_TIME
static void
_cmd_scan_update_chparam(void *drv, struct rtw_phl_scan_param *param)
{
	u8 idx = 0;
	u16 scan_section_ms = 0;
	u16 total_scan_ms = 0;

	for(idx = 0; idx < param->ch_num; idx++) {
		if (param->ch[idx].scan_mode == P2P_LISTEN_MODE) {
			param->max_listen_time = param->ch[idx].duration;
			total_scan_ms = param->ch[idx].duration;
			break;
		}
	}

	scan_section_ms = param->back_op_ch_dur_ms + param->back_op_off_ch_dur_ms;
	if (scan_section_ms)
		param->repeat = total_scan_ms / scan_section_ms;
}
#endif

static void
_phl_cmd_scan_req_init(void *phl, struct phl_cmd_token_req *fgreq,
		       struct rtw_phl_scan_param *param)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_chan_def chdef_list[MAX_WIFI_ROLE_NUMBER] = {0};
	u8 op_num = 0;
	u8 idx = 0;

	pq_init(drv, &param->chlist);
	param->enqueue_time = _os_get_cur_time_ms();

	_cmd_scan_update_chlist(drv, param);
	if (param->back_op_mode != SCAN_BKOP_NONE) {
		op_num = rtw_phl_mr_get_opch_list(phl_info, param->wifi_role,
						chdef_list, (u8)MAX_WIFI_ROLE_NUMBER);
		if(op_num) {
			for(idx = 0; idx < op_num; idx++) {
				if(chdef_list[idx].chan) {
					INIT_LIST_HEAD(&param->back_op_ch[idx].list);
					param->back_op_ch[idx].channel = chdef_list[idx].chan;
					param->back_op_ch[idx].bw = chdef_list[idx].bw;
					param->back_op_ch[idx].offset = chdef_list[idx].offset;
					param->back_op_ch[idx].duration = param->back_op_ch_dur_ms;
					param->back_op_ch[idx].scan_mode = BACKOP_MODE;
				}
			}
		}
		#ifdef CONFIG_PHL_CMD_SCAN_BKOP_TIME
		if (op_num && param->back_op_mode == SCAN_BKOP_TIMER)
			_cmd_scan_update_chparam(drv, param);
		#endif
	}

	#ifdef DBG_SCAN_CHAN_DUMP
	/* debug information*/
	_cmd_estimated_swch_seq(param, op_num);
	#endif

	/* Fill foreground command request */
	fgreq->module_id= PHL_FG_MDL_SCAN;
	fgreq->priv = param;
	fgreq->role = param->wifi_role;

	fgreq->acquired = _phl_cmd_scan_req_acquired;
	fgreq->abort = _phl_cmd_scan_req_abort;
	fgreq->msg_hdlr = _phl_cmd_scan_req_ev_hdlr;
	fgreq->set_info = _phl_cmd_scan_req_set_info;
	fgreq->query_info = _phl_cmd_scan_req_query_info;
}


/* For EXTERNAL application to request scan (expose) */
/* @pscan: scan object
 * @pbuf: scan parameter, will be freed by caller after retrun
 * @order: queuing order
 */
enum rtw_phl_status rtw_phl_cmd_scan_request(void *phl,
	struct rtw_phl_scan_param *param, enum PRECEDE order)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	u8 band_idx = param->wifi_role->hw_band;
	struct phl_cmd_token_req fgreq={0};

	_phl_cmd_scan_req_init(phl, &fgreq, param);

	/* cmd_dispatcher would copy whole phl_cmd_token_req */
	pstatus = phl_disp_eng_add_token_req(phl, band_idx, &fgreq, &param->token);
	if((pstatus != RTW_PHL_STATUS_SUCCESS) &&
	   (pstatus != RTW_PHL_STATUS_PENDING))
		goto error;

	pstatus = RTW_PHL_STATUS_SUCCESS;

error:
	return pstatus;
}
enum rtw_phl_status rtw_phl_cmd_scan_cancel(void *phl,
				struct rtw_phl_scan_param *param)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	u8 band_idx = param->wifi_role->hw_band;

	return phl_disp_eng_cancel_token_req(phl_info, band_idx, &param->token);
}

int rtw_phl_cmd_scan_inprogress(void *phl, u8 band_idx)
{
	struct phl_module_op_info op_info = {0};
	u32	mdl = 0;

	op_info.op_code = FG_REQ_OP_GET_MDL_ID;
	op_info.outbuf = (u8*)&mdl;
	op_info.outlen = 4;

	if(phl_disp_eng_query_cur_cmd_info(phl, band_idx,
				&op_info)== RTW_PHL_STATUS_SUCCESS ) {

		if(mdl == PHL_FG_MDL_SCAN)
			return true;
	}
	return false;
}

#endif /* CONFIG_PHL_CMD_SCAN */
