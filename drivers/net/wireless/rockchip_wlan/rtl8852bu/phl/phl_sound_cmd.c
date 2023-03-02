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

#ifdef CONFIG_CMD_DISP

/* START of sounding / beamform cmd_disp module */
void
_phl_snd_cmd_set_eng_busy(struct phl_info_t *phl_info, enum snd_cmd_disp_ctrl ctrl)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)phl_info->snd_obj;

	if (snd != NULL) {
		snd->msg_busy |= BIT(ctrl);
	}
}

void
_phl_snd_cmd_set_eng_idle(struct phl_info_t *phl_info, enum snd_cmd_disp_ctrl ctrl)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)phl_info->snd_obj;

	if (snd != NULL) {
		snd->msg_busy &= ~(BIT(ctrl));
	}
}

enum rtw_phl_status
_phl_snd_cmd_get_eng_status(struct phl_info_t *phl_info, enum snd_cmd_disp_ctrl ctrl)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)phl_info->snd_obj;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	if (snd != NULL) {
		if (0 != (snd->msg_busy & BIT(ctrl))) {
			pstatus = RTW_PHL_STATUS_PENDING;
		} else {
			pstatus = RTW_PHL_STATUS_SUCCESS;
		}
	} else {
		pstatus = RTW_PHL_STATUS_RESOURCE;
	}
	return pstatus;
}
enum rtw_phl_status
_phl_snd_aquire_eng(struct phl_info_t *phl_info, enum snd_cmd_disp_ctrl ctrl)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)phl_info->snd_obj;
	void *d = phl_to_drvpriv(phl_info);
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;

	if (snd == NULL)
		return RTW_PHL_STATUS_FAILURE;

	/* acuired cmd_disp_eng control */
	_os_spinlock(d, &snd->cmd_lock, _bh, NULL);
	pstatus = _phl_snd_cmd_get_eng_status(phl_info, ctrl);
	if (pstatus != RTW_PHL_STATUS_SUCCESS) {
		_os_spinunlock(d, &snd->cmd_lock, _bh, NULL);
		return pstatus;
	}
	_phl_snd_cmd_set_eng_busy(phl_info, SND_CMD_DISP_CTRL_BFEE);
	_os_spinunlock(d, &snd->cmd_lock, _bh, NULL);

	return pstatus;
}

void
_phl_snd_free_eng(struct phl_info_t *phl_info, enum snd_cmd_disp_ctrl ctrl)
{
	struct phl_sound_obj *snd = (struct phl_sound_obj *)phl_info->snd_obj;
	void *d = phl_to_drvpriv(phl_info);

	if (snd != NULL) {
		_os_spinlock(d, &snd->cmd_lock, _bh, NULL);
		_phl_snd_cmd_set_eng_idle(phl_info, ctrl);
		_os_spinunlock(d, &snd->cmd_lock, _bh, NULL);
	}
}

static enum phl_mdl_ret_code
_phl_snd_cmd_module_init(void *phl, void *dispr, void **priv)
{
	enum phl_mdl_ret_code ret = MDL_RET_SUCCESS;
#ifdef CONFIG_SND_CMD
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	void *d = phl_to_drvpriv(phl_info);
	struct phl_sound_obj *snd_obj = NULL;

	PHL_INFO("--> %s\n", __func__);
	if (NULL == phl_info->snd_obj) {
		phl_info->snd_obj = _os_kmem_alloc(d, sizeof(struct phl_sound_obj));
	}
	snd_obj = phl_info->snd_obj;
	if (snd_obj == NULL) {
		ret = MDL_RET_FAIL;
	} else {
		/* Init the snd static resources here */
		snd_obj->phl_info = (struct phl_info_t *)phl;
		_os_spinlock_init(d, &snd_obj->snd_lock);
		_os_spinlock_init(d, &snd_obj->cmd_lock);
	}
#endif
	*priv = (void *)phl;
	return ret;
}

static void _phl_snd_cmd_module_deinit(void *dispr, void *priv)
{
#ifdef CONFIG_SND_CMD
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;
	struct phl_sound_obj *snd_obj = (struct phl_sound_obj *)phl_info->snd_obj;
	void *d = phl_to_drvpriv(phl_info);

	PHL_INFO("--> %s\n", __func__);
	if (NULL != snd_obj) {
		_os_spinlock_free(d, &snd_obj->snd_lock);
		_os_spinlock_free(d, &snd_obj->cmd_lock);
		_os_kmem_free(d, snd_obj, sizeof(struct phl_sound_obj));
	}
	phl_info->snd_obj = NULL;
#endif
	return;
}

static enum phl_mdl_ret_code _phl_snd_cmd_module_start(void *dispr, void *priv)
{
	return MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code _phl_snd_cmd_module_stop(void *dispr, void *priv)
{
	return MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code
_phl_snd_cmd_module_set_info(void *dispr, void *priv,
			     struct phl_module_op_info *info)
{
	enum phl_mdl_ret_code mstatus = MDL_RET_IGNORE;
	struct phl_info_t *phl = (struct phl_info_t *)priv;

	PHL_INFO("[SND_CMD], %s(): opcode %d.\n", __func__, info->op_code);

	switch (info->op_code) {
		case SND_CMD_OP_SET_AID:
		{
			struct snd_cmd_set_aid *cmdbuf = (struct snd_cmd_set_aid *)info->inbuf;

			if (NULL == cmdbuf) {
				mstatus = MDL_RET_FAIL;
				break;
			}
			if (NULL == cmdbuf->sta_info) {
				mstatus = MDL_RET_FAIL;
				cmdbuf->cmd_sts = MDL_RET_FAIL;
				break;
			}
			if (RTW_HAL_STATUS_SUCCESS != rtw_hal_beamform_set_aid(
						phl->hal, cmdbuf->sta_info, cmdbuf->aid)) {
				mstatus = MDL_RET_FAIL;
				cmdbuf->cmd_sts = MDL_RET_FAIL;
				break;
			}
#ifdef RTW_WKARD_BFEE_SET_AID
			cmdbuf->sta_info->wrole->last_set_aid = cmdbuf->aid;
#endif
			cmdbuf->cmd_sts = MDL_RET_SUCCESS;
			mstatus = MDL_RET_SUCCESS;
		}
		break;
		case SND_CMD_OP_NONE:
		case SND_CMD_OP_MAX:
		default:
		break;
	}

	return mstatus;
}

static enum phl_mdl_ret_code
_phl_snd_cmd_module_query_info(void *dispr, void *priv,
			       struct phl_module_op_info *info)
{
	return MDL_RET_IGNORE;
}

static enum phl_mdl_ret_code
_phl_snd_cmd_module_msg_msg_hdlr_pre(struct phl_info_t *phl,
				     struct phl_msg *msg)
{
	enum phl_mdl_ret_code mstatus = MDL_RET_IGNORE;
	switch (MSG_EVT_ID_FIELD(msg->msg_id)) {
	case MSG_EVT_SET_VHT_GID:
		/* do nothing in pre phase */
	break;
	case MSG_EVT_SET_BFEE_AID:
		/* do nothing in pre phase */
	break;
	default:
		break;
	}

	return mstatus;
}

static enum phl_mdl_ret_code
_phl_snd_cmd_module_msg_msg_hdlr_post(struct phl_info_t *phl,
				      struct phl_msg *msg)
{
	enum phl_mdl_ret_code mstatus = MDL_RET_IGNORE;
	switch (MSG_EVT_ID_FIELD(msg->msg_id)) {
	case MSG_EVT_SET_VHT_GID:
	{
		struct rtw_phl_gid_pos_tbl *gid_tbl = (struct rtw_phl_gid_pos_tbl *)msg->inbuf;
		if (msg->inlen != sizeof(struct rtw_phl_gid_pos_tbl)) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
				  "%s() VHT-BFEE : Error, size mis-match \n", __func__);
			mstatus = MDL_RET_FAIL;
			break;
		}
		rtw_hal_beamform_set_vht_gid(phl->hal, msg->band_idx, gid_tbl);
		mstatus = MDL_RET_SUCCESS;
	}
	break;
	case MSG_EVT_SET_BFEE_AID:
	{
		struct snd_cmd_set_aid *cmdbuf = (struct snd_cmd_set_aid *)msg->inbuf;

		if (NULL == cmdbuf) {
			mstatus = MDL_RET_FAIL;
			break;
		}
		if (NULL == cmdbuf->sta_info) {
			mstatus = MDL_RET_FAIL;
			cmdbuf->cmd_sts = MDL_RET_FAIL;
			break;
		}
		if (RTW_HAL_STATUS_SUCCESS != rtw_hal_beamform_set_aid(
					phl->hal, cmdbuf->sta_info, cmdbuf->aid)) {
			mstatus = MDL_RET_FAIL;
			cmdbuf->cmd_sts = MDL_RET_FAIL;
			break;
		}
		cmdbuf->cmd_sts = MDL_RET_SUCCESS;
		mstatus = MDL_RET_SUCCESS;
	}
	break;
	default:
		break;
	}

	return mstatus;
}

static enum phl_mdl_ret_code
_phl_snd_cmd_module_msg_hdlr(void *dispr, void *priv,
			     struct phl_msg *msg)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;
	enum phl_mdl_ret_code mstatus = MDL_RET_IGNORE;

	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "===> %s() event id : 0x%x \n", __func__, MSG_EVT_ID_FIELD(msg->msg_id));

	if (IS_MSG_FAIL(msg->msg_id)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			  "%s: cmd dispatcher notify cmd failure: 0x%x.\n",
			  __FUNCTION__, msg->msg_id);
		mstatus = MDL_RET_FAIL;
		return mstatus;
	}

	if (IS_MSG_IN_PRE_PHASE(msg->msg_id)) {
		mstatus = _phl_snd_cmd_module_msg_msg_hdlr_pre(phl_info, msg);
	} else {
		mstatus = _phl_snd_cmd_module_msg_msg_hdlr_post(phl_info, msg);
	}

	return mstatus;
}

enum rtw_phl_status phl_snd_cmd_register_module(struct phl_info_t *phl_info)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct phl_bk_module_ops bk_ops;
	struct phl_cmd_dispatch_engine *disp_eng = &(phl_info->disp_eng);
	u8 i = 0;

	bk_ops.init = _phl_snd_cmd_module_init;
	bk_ops.deinit = _phl_snd_cmd_module_deinit;
	bk_ops.start = _phl_snd_cmd_module_start;
	bk_ops.stop = _phl_snd_cmd_module_stop;
	bk_ops.msg_hdlr = _phl_snd_cmd_module_msg_hdlr;
	bk_ops.set_info = _phl_snd_cmd_module_set_info;
	bk_ops.query_info = _phl_snd_cmd_module_query_info;


	for (i = 0; i < disp_eng->phy_num; i++) {
		phl_status = phl_disp_eng_register_module(phl_info, i,
						PHL_MDL_SOUND, &bk_ops);
		if (phl_status != RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("%s register SOUND module in cmd disp band[%d] failed\n",
				__func__, i);
			phl_status = RTW_PHL_STATUS_FAILURE;
			break;
		}
	}

	return phl_status;
}


/* Start of APIs for Core/OtherModule */

/* sub-functions */
/**
 * _phl_snd_cmd_post_set_vht_gid(...)
 * 	used by rtw_phl_snd_cmd_set_vht_gid(..)
 **/
void _phl_snd_cmd_post_set_vht_gid(void* priv, struct phl_msg* msg)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "--> %s : release memeory \n", __func__);
	if (msg->inbuf && msg->inlen){
		_os_kmem_free(phl_to_drvpriv(phl_info), msg->inbuf, msg->inlen);
	}
	/* release cmd_disp_eng control */
	_phl_snd_free_eng(phl_info, SND_CMD_DISP_CTRL_BFEE);
}
/* main-functions */
/**
 * rtw_phl_snd_cmd_set_vht_gid (...)
 * input : struct rtw_phl_gid_pos_tbl *tbl
 * 		the received VHT GID management frame's GID / Position informaion.
 **/
enum rtw_phl_status
rtw_phl_snd_cmd_set_vht_gid(void *phl,
			    struct rtw_wifi_role_t *wrole,
			    struct rtw_phl_gid_pos_tbl *tbl)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	void *d = phl_to_drvpriv(phl_info);
	struct rtw_phl_gid_pos_tbl *gid_tbl;

	/* acuired cmd_disp_eng control */
	if (RTW_PHL_STATUS_SUCCESS !=
		_phl_snd_aquire_eng(phl_info, SND_CMD_DISP_CTRL_BFEE)) {
			return RTW_PHL_STATUS_FAILURE;
	}

	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_MDL_SOUND);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_SET_VHT_GID);

	msg.band_idx = wrole->hw_band;

	attr.completion.completion = _phl_snd_cmd_post_set_vht_gid;
	attr.completion.priv = phl_info;
	gid_tbl = (struct rtw_phl_gid_pos_tbl *)_os_kmem_alloc(d, sizeof(struct rtw_phl_gid_pos_tbl));
	_os_mem_cpy(d, gid_tbl, tbl, sizeof(struct rtw_phl_gid_pos_tbl));

	msg.inbuf = (u8 *)gid_tbl;
	msg.inlen = sizeof(struct rtw_phl_gid_pos_tbl);

	phl_status = phl_disp_eng_send_msg(phl_info, &msg, &attr, NULL);
	if (phl_status != RTW_PHL_STATUS_SUCCESS) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Dispr send msg fail!\n",
			  __func__);
		goto exit;
	}

	return phl_status;

exit:
	_os_kmem_free(d, gid_tbl,
		      sizeof(struct rtw_phl_gid_pos_tbl));
	/* release cmd_disp_eng control */
	_phl_snd_free_eng(phl_info, SND_CMD_DISP_CTRL_BFEE);
	return phl_status;

}

void _phl_snd_cmd_post_set_aid(void* priv, struct phl_msg* msg)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;
#ifdef RTW_WKARD_BFEE_SET_AID
	struct snd_cmd_set_aid *cmdbuf = NULL;
#endif

	if (msg->inbuf && msg->inlen){
#ifdef RTW_WKARD_BFEE_SET_AID
		cmdbuf = (struct snd_cmd_set_aid *)msg->inbuf;
		/* backup aid */
		if (MDL_RET_SUCCESS == cmdbuf->cmd_sts) {
			cmdbuf->sta_info->wrole->last_set_aid = cmdbuf->aid;
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
				"%s : set aid (%d)\n",
				__func__, cmdbuf->sta_info->wrole->last_set_aid);
		}
#endif
		_os_kmem_free(phl_to_drvpriv(phl_info), msg->inbuf, msg->inlen);
	}
	/* release cmd_disp_eng control */
	_phl_snd_free_eng(phl_info, SND_CMD_DISP_CTRL_BFEE);
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s : set aid complete\n", __func__);
}

enum rtw_phl_status
rtw_phl_snd_cmd_set_aid(void *phl,
			struct rtw_wifi_role_t *wrole,
			struct rtw_phl_stainfo_t *sta,
			u16 aid)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	void *d = phl_to_drvpriv(phl_info);
	struct snd_cmd_set_aid *cmdbuf = NULL;

	/* acuired cmd_disp_eng control */
	if (RTW_PHL_STATUS_SUCCESS !=
		_phl_snd_aquire_eng(phl_info, SND_CMD_DISP_CTRL_BFEE)) {
			return RTW_PHL_STATUS_FAILURE;
	}

	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_MDL_SOUND);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_SET_BFEE_AID);

	msg.band_idx = wrole->hw_band;

	attr.completion.completion = _phl_snd_cmd_post_set_aid;
	attr.completion.priv = phl_info;

	cmdbuf = (struct snd_cmd_set_aid *)_os_kmem_alloc(d, sizeof(struct snd_cmd_set_aid));
	cmdbuf->aid = aid;
	cmdbuf->sta_info = sta;

	msg.inbuf = (u8 *)cmdbuf;
	msg.inlen = sizeof(struct snd_cmd_set_aid);

	phl_status = phl_disp_eng_send_msg(phl_info, &msg, &attr, NULL);

	if (phl_status != RTW_PHL_STATUS_SUCCESS) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Dispr send msg fail!\n",
			  __func__);
		goto exit;
	}

	return phl_status;

exit:
	_os_kmem_free(d, cmdbuf, sizeof(struct snd_cmd_set_aid));
	/* release cmd_disp_eng control */
	_phl_snd_free_eng(phl_info, SND_CMD_DISP_CTRL_BFEE);
	return phl_status;

}

/**
 * For other module, such as LPS, direct set aid.
 **/
enum rtw_phl_status
phl_snd_cmd_set_aid_info(struct phl_info_t *phl,
			 struct rtw_wifi_role_t *wrole,
			 struct rtw_phl_stainfo_t *sta,
			 u16 aid)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct phl_module_op_info op_info = {0};
	struct snd_cmd_set_aid cmdbuf = {0};
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "--> %s\n", __func__);

	cmdbuf.aid = aid;
	cmdbuf.sta_info = sta;

	op_info.op_code = SND_CMD_OP_SET_AID;
	op_info.inbuf = (u8 *)&cmdbuf;
	op_info.inlen = sizeof(struct snd_cmd_set_aid);

	phl_status = phl_disp_eng_set_bk_module_info(phl, wrole->hw_band,
						     PHL_MDL_SOUND, &op_info);

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "<-- %s\n", __func__);
	return phl_status;
}

#ifdef RTW_WKARD_BFEE_SET_AID
enum rtw_phl_status
_phl_cmd_snd_restore_aid(struct phl_info_t *phl,
			 struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_SUCCESS;
	struct rtw_phl_stainfo_t *sta = NULL;

	do {
		if (MLME_NO_LINK == wrole->mstate)
			break;

		sta = rtw_phl_get_stainfo_self(phl, wrole);

		if (NULL == sta)
			break;

		if (wrole->last_set_aid == sta->aid)
			break;

		phl_status = phl_snd_cmd_set_aid_info(phl, wrole, sta, sta->aid);

	} while (0);

	return phl_status;
}
#endif


enum rtw_phl_status
_phl_snd_cmd_ntfy_ps_enter(struct phl_info_t *phl,
			   struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_SUCCESS;

#ifdef RTW_WKARD_BFEE_SET_AID
	phl->phl_com->is_in_lps = 1;

	if (PHL_RTYPE_STATION == wrole->type) {
		phl_status= _phl_cmd_snd_restore_aid(phl, wrole);
	}
#endif

	/* TODO: stop BFer period sounding if in progress */
	return phl_status;
}

enum rtw_phl_status
_phl_snd_cmd_ntfy_ps_leave(struct phl_info_t *phl,
			   struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_SUCCESS;

#ifdef RTW_WKARD_BFEE_SET_AID
	phl->phl_com->is_in_lps = 0;
#endif

	/* TODO: restart BFer period sounding if sounding needed */
	return phl_status;
}


enum rtw_phl_status
phl_snd_cmd_ntfy_ps(struct phl_info_t *phl,
		    struct rtw_wifi_role_t *wrole,
		    bool enter)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_SUCCESS;

	if (true == enter)
		phl_status = _phl_snd_cmd_ntfy_ps_enter(phl, wrole);
	else
		phl_status = _phl_snd_cmd_ntfy_ps_leave(phl, wrole);

	return phl_status;
}

#endif