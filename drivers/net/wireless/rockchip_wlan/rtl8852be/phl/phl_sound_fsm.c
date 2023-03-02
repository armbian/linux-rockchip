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

/*
 * SND sub function
 */
#ifdef CONFIG_FSM
static void snd_set_timer(struct phl_sound_obj *snd, int duration, u16 event)
{
	FSM_INFO(snd->fsm, "%s, duration=%d\n", __func__, duration);
	phl_fsm_set_alarm(snd->fsm_obj, duration, event);
}


enum rtw_phl_status
phl_snd_upd_snd_para(struct phl_info_t *phl_info, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)phl_info->snd_obj;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	struct phl_snd_start_req *req = (struct phl_snd_start_req *)param;
	struct phl_sound_param *snd_param = &snd->snd_param;
	struct rtw_wifi_role_t *wrole = NULL;
	FUNCIN_WSTS(pstatus);

	snd_param->snd_func_grp_num = 0;
	snd_param->grp_used_map = 0;
	snd_param->snd_fail_counter = 0;

	if (req != NULL) {
		snd_param->m_wrole = req->wrole;
		wrole = (struct rtw_wifi_role_t *)req->wrole;
		snd_param->snd_proc_timeout_ms = req->proc_timeout_ms;/* ms */
		snd_param->snd_dialog_token = req->dialog_token;
		snd_param->snd_proc_period = req->proc_period;
		snd_param->test_flag = req->test_flag;
		snd_param->bypass_snd_sts_chk = req->bypass_sts_chk;
	}

	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "wrole->type = 0x%x \n",
		  wrole->type);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "timeout = %d ms\n", snd_param->snd_proc_timeout_ms);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "dialog tkn = %d\n", snd_param->snd_dialog_token);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "snd period = %d ms\n", snd_param->snd_proc_period);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "test flag = %d \n", snd_param->test_flag);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "by pass sts check = %d \n", snd_param->bypass_snd_sts_chk);

	FUNCIN_WSTS(pstatus);
	return pstatus;
}



/*
 * SND state handler
 */

static int snd_fsm_func_init_st_hdl(void *obj, u16 event, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)obj;
	void *d = phl_to_drvpriv(snd->phl_info);
	int ret = 0;

	switch (event) {
	case FSM_EV_STATE_IN:
		/* Pending and waiting sounding start event */
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "snd_fsm_func_init_st_hdl : SND FUNC pending and wait SND START\n");
	break;
	case SND_FUNC_EV_SND_START:
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "snd_fsm_func_init_st_hdl : Recv SND START Event at timestamp %d\n",
			  _os_get_cur_time_us());
		/* SND_FUNC_EV_SND_START Shall have input param */
		_os_spinlock(d, &snd->snd_lock, _bh, NULL);
		snd->snd_in_progress = 1; /*TODO: Check set flag timing*/
		_os_spinunlock(d, &snd->snd_lock, _bh, NULL);

		phl_snd_upd_snd_para(snd->phl_info, param);
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_FUNC_READY);
	break;
	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case FSM_EV_CANCEL:
	case FSM_EV_TIMER_EXPIRE:
	break;
	default:
	break;
	}
	return ret;

}

static int snd_fsm_func_deinit_st_hdl(void *obj, u16 event, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)obj;
	int ret = 0;


	switch (event) {
	case FSM_EV_STATE_IN:
	break;
	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case FSM_EV_CANCEL:
	case FSM_EV_TIMER_EXPIRE:
	break;
	default:
	break;
	}

	return ret;

}

static int snd_fsm_func_ready_st_hdl(void *obj, u16 event, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)obj;
	int ret = 0;
	struct rtw_wifi_role_t *m_wrole =
			(struct rtw_wifi_role_t *)snd->snd_param.m_wrole;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;

	switch (event) {
	case FSM_EV_STATE_IN:
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "snd_fsm_func_ready_st_hdl : FSM_EV_STATE_IN\n");
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "snd_fsm_func_ready_st_hdl : (Re)Start Sounding Timestamp %d\n",
				_os_get_cur_time_us());
		if(NULL == m_wrole) {
			snd_set_timer(snd, 0, SND_EV_TERMINATE);
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "snd_fsm_func_ready_st_hdl : NULL == m_wrole\n");
			break;
		}

		if ((PHL_RTYPE_STATION == m_wrole->type) &&
		    (MLME_LINKED != m_wrole->mstate)) {
				PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
					  "snd_fsm_func_ready_st_hdl : role STA in not in linked : 0x%x\n",
					   m_wrole->mstate);
				snd_set_timer(snd, 0, SND_EV_TERMINATE);
				break;
		}
		snd_set_timer(snd, 10, SND_FUNC_EV_PROC_GRP);
	break;
	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case FSM_EV_CANCEL:
	case FSM_EV_TIMER_EXPIRE:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case SND_FUNC_EV_PROC_GRP:
		phl_snd_func_remove_grp_all(snd->phl_info);
		/*TODO: loop for all of the wrole */
		if(NULL == m_wrole) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "==> ROLE == NULL !!!\n STOP SND Function!!!\n");
			snd_set_timer(snd, 10, SND_EV_TERMINATE);
			break;
		}
		pstatus = phl_snd_func_grouping(snd->phl_info, m_wrole->id);
		if (pstatus != RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "==> SND_FUNC_EV_PROC_GRP FAIL !!!\n STOP SND Function!!!\n");
			snd_set_timer(snd, 10, SND_EV_TERMINATE);
			break;
		}
		if (0 == snd->snd_param.snd_func_grp_num) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "==> Available SND GRP == 0 !!!\n STOP SND Function!!!\n");
			snd_set_timer(snd, 10, SND_EV_TERMINATE);
			break;
		}
		/* fall through */
	/*go through*/
	case SND_FUNC_EV_START_SND_PROC:
		pstatus = phl_snd_func_pre_config(snd->phl_info);
		if (pstatus != RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "==> SND_FUNC_EV_START_SND_PROC FAIL !!!\n STOP SND Function!!!\n");
			snd_set_timer(snd, 10, SND_EV_TERMINATE);
			break;
		}
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_PROC_IDLE);
	break;
	case SND_EV_TERMINATE:
		snd->is_terminated = true;
		phl_fsm_cancel_alarm(snd->fsm_obj);
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_PROC_TERMINATE);
	break;
	default:
	break;
	}

	return ret;
}

static int snd_fsm_func_leave_st_hdl(void *obj, u16 event, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)obj;
	void *d = phl_to_drvpriv(snd->phl_info);
	int ret = 0;


	switch (event) {
	case FSM_EV_STATE_IN:
		_os_spinlock(d, &snd->snd_lock, _bh, NULL);
		snd->snd_in_progress = 0;
		_os_spinunlock(d, &snd->snd_lock, _bh, NULL);
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_FUNC_INIT);
	break;
	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case FSM_EV_CANCEL:
	case FSM_EV_TIMER_EXPIRE:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case SND_EV_TERMINATE:
		_os_spinlock(d, &snd->snd_lock, _bh, NULL);
		snd->is_terminated = 1;
		snd->snd_in_progress = 0;
		_os_spinunlock(d, &snd->snd_lock, _bh, NULL);
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_FUNC_INIT);
	break;
	default:
	break;
	}
	return ret;
}



static int snd_fsm_proc_idle_st_hdl(void *obj, u16 event, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)obj;
	struct phl_sound_param *snd_param = &snd->snd_param;
	int ret = 0;
	struct phl_snd_grp *grp = NULL;

	switch (event) {
	case FSM_EV_STATE_IN:

	/* go through */
	case SND_PROC_EV_IDLE_FREE_PRE_RES :
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "pre_proc_grp_idx : %d ; cur_proc_grp_idx : %d \n",
		  snd_param->pre_proc_grp_idx, snd_param->cur_proc_grp_idx);

		if (snd_param->cur_proc_grp_idx !=
			snd_param->pre_proc_grp_idx) {
			grp = phl_snd_get_grp_byidx(snd->phl_info,
						snd_param->pre_proc_grp_idx);
			if (grp != NULL) {
				phl_snd_proc_chk_prev_grp(snd->phl_info, grp);
			}
		}
		/* fall through */
	/* go through */
	case SND_PROC_EV_IDLE_GET_SND_GRP :
		if(MAX_SND_GRP_NUM == snd_param->cur_proc_grp_idx) {
			snd_set_timer(snd, 10, SND_EV_TERMINATE);
			break;
		}

		grp = phl_snd_get_grp_byidx(snd->phl_info,
					    snd_param->cur_proc_grp_idx);
		/* TODO: Check Grp STA is still available */
		/* phl_snd_check_condition() */
		if (NULL == grp) {
			snd_set_timer(snd, 0, SND_EV_TERMINATE);
			break;
		}
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_PROC_BUSY); /* For SND FSM LOOP Test */
	break;
	case FSM_EV_CANCEL:
	case FSM_EV_TIMER_EXPIRE:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case SND_EV_TERMINATE:
		snd->is_terminated = true;
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_PROC_TERMINATE);
	break;
	default:
	break;
	}
	return ret;
}
static int snd_fsm_proc_busy_st_hdl(void *obj, u16 event, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)obj;
	struct phl_sound_param *snd_param = &snd->snd_param;
	int ret = 0;
	struct phl_snd_grp *grp = NULL;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	u8 sta_num = 0;
	struct rtw_wifi_role_t *wrole =
		(struct rtw_wifi_role_t *)snd->snd_param.m_wrole;

	switch (event) {
	case FSM_EV_STATE_IN:
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "SND PROC BUSY : Process Grp idx = %d\n",
			  snd_param->cur_proc_grp_idx);
		grp = phl_snd_get_grp_byidx(snd->phl_info,
					    snd_param->cur_proc_grp_idx);
		if (NULL == grp) {
			snd_set_timer(snd, 0, SND_PROC_EV_BUSY_SND_DOWN);
			break;
		}
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "SND_PROC_BUSY : grp sta number = %d \n",
			  grp->num_sta);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "SND_PROC_BUSY : grp snd_fb_t = 0x%x \n",
			  grp->sta[0].snd_fb_t);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "SND_PROC_BUSY : grp primary sta macid = 0x%x \n",
			  grp->sta[0].macid);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "SND_PROC_BUSY : grp sta-1 macid = 0x%x \n",
			  grp->sta[1].macid);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "SND_PROC_BUSY : grp sta-2 macid = 0x%x \n",
			  grp->sta[2].macid);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "SND_PROC_BUSY : grp sta-3 macid = 0x%x \n",
			  grp->sta[3].macid);
		/* fall through */
	case SND_PROC_EV_BUSY_GET_BF_RES:
		pstatus = phl_snd_proc_get_res(snd->phl_info, grp, &sta_num);
		if ((sta_num == 0) || (RTW_PHL_STATUS_SUCCESS != pstatus)) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "SND_PROC_BUSY : sta_num = 0 !!! \n");
			grp->snd_sts = PHL_SND_STS_FAILURE;
			snd_set_timer(snd, 0, SND_PROC_EV_BUSY_SND_DOWN);
			break;
		}
		/* fall through */
	/* go through */
	case SND_PROC_EV_BUSY_PRE_CFG:
		pstatus = phl_snd_proc_precfg(snd->phl_info, grp);
		/* fall through */
	/* go through */
	case SND_PROC_EV_BUSY_TRIG_SND:
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "[SND] Dialog token = 0x%x\n",
			  snd->snd_param.snd_dialog_token);
		pstatus = phl_snd_proc_start_sounding_fw(snd->phl_info, grp);
		if (RTW_PHL_STATUS_SUCCESS != pstatus) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "phl_snd_proc_start_sounding_fw FAIL\n");
			grp->snd_sts = PHL_SND_STS_FAILURE;
			if((grp->snd_type == PHL_SND_TYPE_HE_HW) ||
			   (grp->snd_type == PHL_SND_TYPE_VHT_HW)) {
				phl_snd_mac_ctrl(snd->phl_info, wrole, 1);
			}
			/* TODO : FW / halmac might crashed , stop sounding fsm module */
			snd_set_timer(snd, 0, SND_EV_TERMINATE);
		}
		snd->snd_param.snd_dialog_token++;
		snd_set_timer(snd, 10, SND_PROC_EV_BUSY_REL_SND_CTRL);
	break;
	case SND_PROC_EV_BUSY_REL_SND_CTRL :
		grp = phl_snd_get_grp_byidx(snd->phl_info,
					    snd_param->cur_proc_grp_idx);
		if (NULL == grp) {
			snd_set_timer(snd, 0, SND_PROC_EV_BUSY_SND_DOWN);
			break;
		}
		if((grp->snd_type == PHL_SND_TYPE_HE_HW) ||
		   (grp->snd_type == PHL_SND_TYPE_VHT_HW)) {
			phl_snd_mac_ctrl(snd->phl_info, wrole, 1);
		}
		snd_set_timer(snd, snd->snd_param.snd_proc_timeout_ms,
			      SND_PROC_EV_BUSY_SND_DOWN);
	break;
	case SND_PROC_EV_BUSY_SND_DOWN:
		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "SND_PROC_BUSY: Recv C2H or Timeout , Switch to PROC DOWN\n");
		grp = phl_snd_get_grp_byidx(snd->phl_info,
					    snd_param->cur_proc_grp_idx);
		if (NULL != grp)
			phl_snd_polling_pri_sta_sts(snd->phl_info, grp);
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_PROC_DOWN); /* For SND FSM Periodic LOOP Test */
	break;
	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case FSM_EV_CANCEL:
	case FSM_EV_TIMER_EXPIRE:
	break;
	case SND_EV_TERMINATE:
		snd->is_terminated = true;
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_PROC_TERMINATE);
	break;
	default:
	break;
	}
	return ret;
}

static int snd_fsm_proc_down_st_hdl(void *obj, u16 event, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)obj;
	int ret = 0;
	struct phl_sound_param *snd_param = &snd->snd_param;
	struct phl_snd_grp *grp = NULL;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;

	switch (event) {
	case FSM_EV_STATE_IN:
		grp = phl_snd_get_grp_byidx(snd->phl_info,
					    snd_param->cur_proc_grp_idx);
		if (NULL == grp) {
			snd_set_timer(snd, snd_param->snd_proc_period,
						SND_PROC_EV_DOWN_SND_END);
			break;
		}

		if (PHL_SND_STS_FAILURE == grp->snd_sts) {
			snd_set_timer(snd, snd_param->snd_proc_period,
				      SND_PROC_EV_DOWN_SND_END);
		}
		/* fall through */
	/* go through */
	case SND_PROC_EV_DOWN_POST_CFG:
		pstatus = phl_snd_proc_postcfg(snd->phl_info, grp);
		if (pstatus == RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "POST CONFIG SUCCESS!!!!\n");
		}
		/* fall through */
	/* go through */
	case SND_PROC_EV_DOWN_SND_END:
		/* Check Test Mode */
		if (snd_param->test_flag&PHL_SND_TEST_F_ONE_TIME) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "TEST MODE : Set EV to TERMINATE by test_flag\n");
			snd_set_timer(snd, snd_param->snd_proc_period,
						SND_EV_TERMINATE);
			break;
		}
		/* TODO: check next sounding needed
			if any sta disassociate or new sta associate, need re-grouping? */
		grp = phl_snd_get_grp_byidx(snd->phl_info,
					    snd_param->cur_proc_grp_idx);
		if (grp == NULL) {
			snd_set_timer(snd, snd_param->snd_proc_period,
				      SND_EV_TERMINATE);
		}
		if (RTW_PHL_STATUS_SUCCESS ==
			phl_snd_proc_chk_condition(snd->phl_info, grp)) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "Status : Linked ; Set Timer for Next Sounding \n");
			snd_set_timer(snd, snd_param->snd_proc_period,
						SND_PROC_EV_DOWN_NEXT_SND);
		} else {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
				  "Status : ERROR ; Set Timer to Terminate \n");
			snd_set_timer(snd, snd_param->snd_proc_period,
						SND_EV_TERMINATE);
		}
	break;
	case FSM_EV_CANCEL:
	case FSM_EV_TIMER_EXPIRE:
	break;
	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case SND_PROC_EV_DOWN_NEXT_SND:
		snd_param->pre_proc_grp_idx = snd_param->cur_proc_grp_idx;
		snd_param->cur_proc_grp_idx++;

		if (snd_param->cur_proc_grp_idx >=
			snd_param->snd_func_grp_num) {
			/* start sounding process from group list head or terminated */
			if (snd->is_terminated)
				snd_param->cur_proc_grp_idx = MAX_SND_GRP_NUM;
			else
				snd_param->cur_proc_grp_idx = 0;
		}

		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "Next Sounding Group Idx = %d",
			  snd_param->cur_proc_grp_idx);

		phl_fsm_state_goto(snd->fsm_obj, SND_ST_PROC_IDLE);
	break;
	case SND_EV_TERMINATE:
		snd->is_terminated = true;
		phl_fsm_state_goto(snd->fsm_obj, SND_ST_PROC_TERMINATE);
	break;
	default:
	break;
	}
	return ret;
}

static int snd_fsm_proc_terminiate_st_hdl(void *obj, u16 event, void *param)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)obj;
	int ret = 0;

	switch (event) {
	case FSM_EV_STATE_IN:
		/* Free all sounding resource and goto FUNC Leave */
		phl_snd_func_remove_grp_all(snd->phl_info);

		phl_fsm_state_goto(snd->fsm_obj, SND_ST_FUNC_LEAVE);
	break;
	case FSM_EV_CANCEL:
	case FSM_EV_TIMER_EXPIRE:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	case FSM_EV_STATE_OUT:
		phl_fsm_cancel_alarm(snd->fsm_obj);
	break;
	default:
	break;
	}
	return ret;
}

static void snd_dump_obj(void *obj, char *s, int *sz)
{
	/* nothing to do for now */
}

static void snd_dump_fsm(void *fsm, char *s, int *sz)
{
	/* nothing to do for now */
}

/* STATE table */
static struct fsm_state_ent snd_state_tbl[] = {
	ST_ENT(SND_ST_FUNC_INIT, snd_fsm_func_init_st_hdl),
	ST_ENT(SND_ST_FUNC_DEINIT, snd_fsm_func_deinit_st_hdl),
	ST_ENT(SND_ST_FUNC_READY, snd_fsm_func_ready_st_hdl),
	ST_ENT(SND_ST_FUNC_LEAVE, snd_fsm_func_leave_st_hdl),
	ST_ENT(SND_ST_PROC_IDLE, snd_fsm_proc_idle_st_hdl),
	ST_ENT(SND_ST_PROC_BUSY, snd_fsm_proc_busy_st_hdl),
	ST_ENT(SND_ST_PROC_DOWN, snd_fsm_proc_down_st_hdl),
	ST_ENT(SND_ST_PROC_TERMINATE,snd_fsm_proc_terminiate_st_hdl)
};

/* EVENT table */
static struct fsm_event_ent snd_event_tbl[] = {
	EV_ENT(SND_FUNC_EV_INIT),
	EV_ENT(SND_FUNC_EV_DEINIT),
	EV_ENT(SND_FUNC_EV_SND_START),
	EV_ENT(SND_FUNC_EV_NOTIFY_PENDING_REQ),
	EV_ENT(SND_FUNC_EV_CHK_SND_STS),
	EV_ENT(SND_FUNC_EV_PROC_GRP),
	EV_ENT(SND_FUNC_EV_START_SND_PROC),
	EV_ENT(SND_FUNC_EV_END_SND),
	/*PROC IDLE*/
	EV_ENT(SND_PROC_EV_IDLE_GET_SND_GRP),
	/*PROC BUSY*/
	EV_ENT(SND_PROC_EV_BUSY_GET_BF_RES),
	EV_ENT(SND_PROC_EV_IDLE_FREE_PRE_RES),
	EV_ENT(SND_PROC_EV_BUSY_PRE_CFG),
	EV_ENT(SND_PROC_EV_BUSY_TRIG_SND),
	EV_ENT(SND_PROC_EV_BUSY_REL_SND_CTRL),
	EV_ENT(SND_PROC_EV_BUSY_POLLING_STS),
	EV_ENT(SND_PROC_EV_BUSY_SND_DOWN),
	/*PROC DOWN*/
	EV_ENT(SND_PROC_EV_DOWN_POST_CFG),
	EV_ENT(SND_PROC_EV_DOWN_SND_END),
	EV_ENT(SND_PROC_EV_DOWN_NEXT_SND),
	/*COMMON*/
	EV_ENT(SND_EV_TERMINATE),
	EV_ENT(SND_EV_DONOTHING),
	EV_ENT(SND_EV_MAX),
};

/* For EXTERNAL application to create a SND FSM */
/* @root: FSM root structure
 * @phl_info: private data structure to invoke hal/phl function
 *
 * return
 * fsm_main: FSM main structure (Do NOT expose)
 */
struct fsm_main *phl_sound_new_fsm(struct fsm_root *root,
	struct phl_info_t *phl_info)
{
	void *d = phl_to_drvpriv(phl_info);
	struct fsm_main *fsm = NULL;
	struct rtw_phl_fsm_tb tb;

	_os_mem_set(d, &tb, 0, sizeof(tb));


	tb.state_tbl = snd_state_tbl;
	tb.max_state = sizeof(snd_state_tbl)/sizeof(snd_state_tbl[0]);
	tb.max_event = sizeof(snd_event_tbl)/sizeof(snd_event_tbl[0]);
	tb.evt_tbl = snd_event_tbl;
	tb.dump_obj = snd_dump_obj;
	tb.dump_obj = snd_dump_fsm;
	tb.dbg_level = FSM_DBG_INFO;
	tb.evt_level = FSM_DBG_INFO;

	fsm = phl_fsm_init_fsm(root, "sound", phl_info, &tb);

	return fsm;
}

/* For EXTERNAL application to destory SND FSM */
/* @fsm: see fsm_main
 */
void phl_snd_destory_fsm(struct fsm_main *fsm)
{
	if (fsm == NULL)
		return;

	/* deinit fsm local variable if has */

	/* call FSM Framewro to deinit fsm */
	phl_fsm_deinit_fsm(fsm);
}

/* For EXTERNAL application to destory sound object */
/* @pcmd: local created command object
 *
 */
void phl_snd_destory_obj(struct phl_sound_obj *snd)
{
	void *d = phl_to_drvpriv(snd->phl_info);
	if (snd == NULL)
		return;

	/* deinit obj local variable if has */
	_os_spinlock_free(d, &snd->snd_lock);
	_os_spinlock_free(d, &snd->cmd_lock);

	/* inform FSM framewory to recycle fsm_obj */
	phl_fsm_destory_obj(snd->fsm_obj);
}

/* For EXTERNAL interrupt handler to send event into snd fsm (expose) */

enum rtw_phl_status phl_snd_fsm_ev_c2h_snd_down(void *phl)
{
	enum RTW_PHL_SND_NOTIFY_EVENT notify = RTW_PHL_SND_C2H_SND_DOWN;

	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			"phl_snd_event_c2h_snd_down notify 0x%x\n", notify);
	return phl_snd_fsm_send_msg(phl, notify, NULL, 0);
}


enum rtw_phl_status
phl_snd_fsm_ev_start_func(void *phl, void *req)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	void *d = phl_to_drvpriv(phl_info);
	struct phl_snd_start_req *snd_req = NULL;//(struct phl_snd_start_req *)req;

	enum RTW_PHL_SND_NOTIFY_EVENT notify = RTW_PHL_SND_START;

	do {
		snd_req = (struct phl_snd_start_req *)_os_kmem_alloc(d,
				sizeof(*snd_req));

		if (snd_req == NULL)
			break;

		_os_mem_cpy(d, snd_req, req, sizeof(*snd_req));

		PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			  "phl_snd_event_start_func notify 0x%x\n", notify);

		status = phl_snd_fsm_send_msg(phl, notify, snd_req,
					      sizeof(*snd_req));

		_os_kmem_free(d, snd_req, sizeof(*snd_req));

	} while (0);

	return status;
}


enum rtw_phl_status phl_snd_fsm_ev_abort(void *phl)
{
	enum RTW_PHL_SND_NOTIFY_EVENT notify = RTW_PHL_SND_ABORT;

	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
			"phl_snd_event_abort notify 0x%x\n", notify);
	return phl_snd_fsm_send_msg(phl, notify, NULL, 0);
}

/* @phl: phl_info_t
 * @notify: event to snd fsm
 */
enum rtw_phl_status
phl_snd_fsm_send_msg(void *phl, enum RTW_PHL_SND_NOTIFY_EVENT notify,
				void *buf, u32 buf_sz)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct phl_sound_obj *snd = phl_info->snd_obj;
	u16 event;

	switch (notify) {
	case RTW_PHL_SND_START: /* Start Sound Func */
		event = SND_FUNC_EV_SND_START;
		break;
	case RTW_PHL_SND_ABORT: /* Go Terminate*/
		event = SND_EV_TERMINATE;
		break;
	case RTW_PHL_SND_C2H_SND_DOWN: /* Sound Dwon Configuration */
		event = SND_PROC_EV_BUSY_SND_DOWN;
		break;
	default:
		return RTW_PHL_STATUS_FAILURE;
	}
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "phl_snd_send_msg event %d\n",
		  event);
	return phl_fsm_gen_msg(phl, snd->fsm_obj, buf, buf_sz, event);
}


/* For EXTERNAL application notify from upper layer*/
void phl_snd_notify_from_upper_watchdog_status(void *phl, bool inprogress)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct phl_sound_obj *snd = NULL;

	snd = phl_info->snd_obj;
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "is snd in progress : %d \n",
		  snd->snd_in_progress);
	/*TODO:*/
}
#endif /*CONFIG_FSM*/

