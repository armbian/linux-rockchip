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
#ifndef __PHL_SOUND_FSM_H__
#define __PHL_SOUND_FSM_H__

struct phl_sound_obj;

enum RTW_PHL_SND_NOTIFY_EVENT {
	RTW_PHL_SND_START,
	RTW_PHL_SND_ABORT,
	RTW_PHL_SND_C2H_SND_DOWN /* C2H Sounding Down */
};

enum SND_EV_ID {
	SND_FUNC_EV_INIT,
	SND_FUNC_EV_DEINIT,
	SND_FUNC_EV_SND_START,
	SND_FUNC_EV_NOTIFY_PENDING_REQ,
	SND_FUNC_EV_CHK_SND_STS,
	SND_FUNC_EV_PROC_GRP,
	SND_FUNC_EV_START_SND_PROC,
	SND_FUNC_EV_END_SND,
	SND_PROC_EV_IDLE_GET_SND_GRP,
	SND_PROC_EV_IDLE_FREE_PRE_RES,
	SND_PROC_EV_BUSY_GET_BF_RES,
	SND_PROC_EV_BUSY_PRE_CFG,
	SND_PROC_EV_BUSY_TRIG_SND,
	SND_PROC_EV_BUSY_REL_SND_CTRL,
	SND_PROC_EV_BUSY_POLLING_STS,
	SND_PROC_EV_BUSY_SND_DOWN,
	SND_PROC_EV_DOWN_POST_CFG,
	SND_PROC_EV_DOWN_SND_END,
	SND_PROC_EV_DOWN_NEXT_SND,
	SND_EV_TERMINATE,
	SND_EV_DONOTHING,
	SND_EV_MAX
};

enum SND_STATE_ST {
	/* Initializae and de-initialize*/
	SND_ST_FUNC_INIT,
	SND_ST_FUNC_DEINIT,
	/* FUNC MAIN STATE */
	SND_ST_FUNC_READY,
	SND_ST_FUNC_LEAVE,
	/* SND PROC - periodic */
	SND_ST_PROC_IDLE,
	SND_ST_PROC_BUSY,
	SND_ST_PROC_DOWN,
	SND_ST_PROC_TERMINATE
};

struct phl_snd_start_req {
	void *wrole;
	u8 dialog_token;
	u8 proc_timeout_ms;
	u8 proc_period;
	bool bypass_sts_chk;
	u8 test_flag;
};

struct fsm_main *phl_sound_new_fsm(struct fsm_root *root,
	struct phl_info_t *phl_info);

void phl_snd_destory_fsm(struct fsm_main *fsm);

enum rtw_phl_status phl_snd_fsm_ev_c2h_snd_down(void *phl);

void phl_snd_destory_obj(struct phl_sound_obj *snd);

enum rtw_phl_status phl_snd_fsm_ev_abort(void *phl);

enum rtw_phl_status
phl_snd_fsm_ev_start_func(void *phl, void *req);

enum rtw_phl_status
phl_snd_fsm_send_msg(void *phl, enum RTW_PHL_SND_NOTIFY_EVENT notify,
		     void *buf, u32 buf_sz);

u8 phl_snd_is_inprogress(void *phl);

#endif
