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
#define _PHL_ROLE_C_
#include "phl_headers.h"


static enum rtw_phl_status
_phl_alloc_hw_resource(struct phl_info_t *phl,
				struct rtw_wifi_role_t *wrole, u8 hw_band)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	void *drv = phl_to_drvpriv(phl);
	struct rtw_phl_com_t *phl_com = phl->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_com);
	struct hw_band_ctl_t *band_ctrl;
	u8 port_num, max_port_num;
	u8 wmm_num, max_wmm_num;
	int ridx;

	band_ctrl = &mr_ctl->band_ctrl[hw_band];
	max_port_num = hal_spec->port_num;
	max_wmm_num = hal_spec->wmm_num;
	ridx = wrole->id;

	_os_spinlock(drv, &band_ctrl->lock, _ps, NULL);
	/*alloc hw_port*/
	for (port_num = 0; port_num < max_port_num; port_num++) {
		if (!(band_ctrl->port_map & BIT(port_num))) {
			band_ctrl->port_map |= BIT(port_num);
			break;
		}
	}
	if (port_num == max_port_num) {
		PHL_ERR("%s Can't allocate hw port\n", __func__);
		_os_warn_on(1);
		goto _exit;
	}
	wrole->hw_port = port_num;

	/*alloc hw_wmm*/
	for (wmm_num = 0; wmm_num < max_wmm_num; wmm_num++) {
		if (!(band_ctrl->wmm_map & BIT(wmm_num))) {
			band_ctrl->wmm_map |= BIT(wmm_num);
			break;
		}
	}
	if (wmm_num == max_wmm_num) {
		PHL_INFO("%s WID:%d - assigne wmm_id to 0\n", __func__, ridx);
		wrole->hw_wmm = 0;
	} else {
		wrole->hw_wmm = wmm_num;
	}
	#ifdef RTW_WKARD_HW_WMM_ALLOCATE
	if (wrole->hw_wmm == 0)
		_os_atomic_inc(phl_to_drvpriv(phl), &wrole->hw_wmm0_ref_cnt);
	#endif

	/*set mr_role_map*/
	band_ctrl->role_map |= BIT(ridx);
	wrole->hw_band = hw_band;
	psts = RTW_PHL_STATUS_SUCCESS;
_exit:
	_os_spinunlock(drv, &band_ctrl->lock, _ps, NULL);

	if (psts == RTW_PHL_STATUS_SUCCESS) {
		PHL_INFO("%s wridx:%d success\n", __func__, ridx);
		PHL_DUMP_ROLE_EX(phl, wrole);
	}
	return psts;
}

static enum rtw_phl_status
_phl_free_hw_resource(struct phl_info_t *phl,
			struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	void *drv = phl_to_drvpriv(phl);
	struct rtw_phl_com_t *phl_com = phl->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl;
	int ridx = wrole->id;
	u8 hw_band = wrole->hw_band;

	band_ctrl = &mr_ctl->band_ctrl[hw_band];
	PHL_DUMP_ROLE_EX(phl, wrole);

	_os_spinlock(drv, &band_ctrl->lock, _ps, NULL);
	/*release hw_port*/
	band_ctrl->port_map &= ~BIT(wrole->hw_port);

	/*release hw_wmm*/
	#ifdef RTW_WKARD_HW_WMM_ALLOCATE
	if (wrole->hw_wmm == 0) {
		_os_atomic_dec(phl_to_drvpriv(phl), &wrole->hw_wmm0_ref_cnt);
		if (_os_atomic_read(phl_to_drvpriv(phl), &wrole->hw_wmm0_ref_cnt) == 0)
			band_ctrl->wmm_map &= ~BIT(wrole->hw_wmm);
	} else
	#endif
	{
		band_ctrl->wmm_map &= ~BIT(wrole->hw_wmm);
	}
	/*release mr_role_map*/
	band_ctrl->role_map &= ~BIT(ridx);
	_os_spinunlock(drv, &band_ctrl->lock, _ps, NULL);

	psts = RTW_PHL_STATUS_SUCCESS;
	PHL_INFO("%s wr-id:%d success\n", __func__, ridx);
	return psts;
}

#if 0 /*TODO:DBCC*/
static enum rtw_phl_status
_phl_realloc_hw_resource(struct phl_info_t *phl,
					struct rtw_wifi_role_t *wrole, u8 new_band)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;

	if (new_band == wrole->hw_band) {
		PHL_ERR("%s widx:%d at the same band(%d)\n", __func__, wrole->id, new_band);
		return psts;
	}

	_phl_free_hw_resource(phl, wrole);
	_phl_alloc_hw_resource(phl, wrole, new_band);
	return psts;
}
#endif
static u8 _phl_search_roleidx_by_addr(
			struct phl_info_t *phl_info, u8 *mac_addr)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	u8 ridx = MAX_WIFI_ROLE_NUMBER;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (mr_ctl->role_map & BIT(ridx)) {
			if (_os_mem_cmp(phl_to_drvpriv(phl_info),
				phl_com->wifi_roles[ridx].mac_addr,
				mac_addr, MAC_ALEN) == 0)
				break;
		}
	}

	#if 0
	if (ridx == MAX_WIFI_ROLE_NUMBER)
		PHL_INFO("%s cannot get rid\n", __func__);
	#endif
	return ridx;
}

struct rtw_wifi_role_t *
phl_get_wrole_by_addr(struct phl_info_t *phl_info, u8 *mac_addr)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	int ridx = MAX_WIFI_ROLE_NUMBER;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct rtw_wifi_role_t *wrole = NULL;
	bool found = false;

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (mr_ctl->role_map & BIT(ridx)) {
			wrole = &(phl_com->wifi_roles[ridx]);
			if (_os_mem_cmp(phl_to_drvpriv(phl_info),
				wrole->mac_addr, mac_addr, MAC_ALEN) == 0) {
				found = true;
				break;
			}
		}
	}

	if (found == false)
		wrole = NULL;

	return wrole;
}

struct rtw_wifi_role_t *
rtw_phl_get_wrole_by_ridx(struct rtw_phl_com_t *phl_com, u8 rold_idx)
{
	if (rold_idx < MAX_WIFI_ROLE_NUMBER)
		return &(phl_com->wifi_roles[rold_idx]);

	return NULL;
}

static void
_phl_role_notify_buf_done(void* priv, struct phl_msg* msg)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;

	if(msg->inbuf && msg->inlen){
		_os_mem_free(phl_to_drvpriv(phl_info), msg->inbuf, msg->inlen);
	}
}

#ifdef CONFIG_CMD_DISP
static void
_phl_role_cmd_done(void* priv, struct phl_msg* msg)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;

	if(msg->inbuf && msg->inlen){
		_os_kmem_free(phl_to_drvpriv(phl_info),
			msg->inbuf, msg->inlen);
	}
}

static void
_phl_send_role_notify_cmd(struct phl_info_t *phl_info,
			struct rtw_wifi_role_t *wrole, enum role_state rstate)
{
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	struct rtw_role_cmd *rcmd = NULL;

	rcmd = (struct rtw_role_cmd *)_os_kmem_alloc(
		phl_to_drvpriv(phl_info), sizeof(struct rtw_role_cmd));
	if (rcmd == NULL) {
		PHL_ERR("%s: alloc role cmd fail.\n", __func__);
		return;
	}

	rcmd->wrole = wrole;
	rcmd->rstate = rstate;
	msg.inbuf = (u8 *)rcmd;
	msg.inlen = sizeof(struct rtw_role_cmd);

	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_MDL_MRC);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_ROLE_NTFY);
	msg.band_idx = HW_BAND_0;
	attr.completion.completion = _phl_role_cmd_done;
	attr.completion.priv = phl_info;

	if (phl_disp_eng_send_msg(phl_info, &msg, &attr, NULL) !=
				RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s: dispr_send_msg failed !\n", __func__);
		goto cmd_fail;
	}

	return;

cmd_fail:
	_os_mem_free(phl_to_drvpriv(phl_info), rcmd,
				sizeof(struct rtw_role_cmd));
}
#endif

static enum rtw_phl_status
_phl_role_notify(struct phl_info_t *phl_info,
		struct rtw_wifi_role_t *wrole,
		enum role_state rstate)
{
	if (rstate >= PHL_ROLE_STATE_UNKNOWN) {
		PHL_ERR("%s unknow rstate:%d\n", __func__, rstate);
		return RTW_PHL_STATUS_FAILURE;
	}

#ifdef CONFIG_PHL_CMD_BTC
	_phl_send_role_notify_cmd(phl_info, wrole, rstate);
#else
#ifdef CONFIG_BTCOEX
	rtw_phl_btc_role_notify(phl_info, wrole->id, rstate);
#endif
#endif

	return RTW_PHL_STATUS_SUCCESS;
}

static struct rtw_wifi_role_t *
_phl_wifi_role_alloc_sw(struct phl_info_t *phl_info,
				u8 *mac_addr, enum role_type type, u8 ridx)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct rtw_wifi_role_t *role = NULL;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	u8 role_idx = INVALID_WIFI_ROLE_IDX;
	u8 hw_band;

	_os_spinlock(phl_to_drvpriv(phl_info), &mr_ctl->lock, _ps, NULL);
	if (ridx == UNSPECIFIED_ROLE_ID) {
		/*search avaliable rid */
		for (role_idx = 0; role_idx < MAX_WIFI_ROLE_NUMBER; role_idx++) {
			if (!(mr_ctl->role_map & BIT(role_idx))) {
				mr_ctl->role_map |= BIT(role_idx);
				break;
			}
		}
	} else {
		if (mr_ctl->role_map & BIT(ridx)) {
			PHL_ERR("role_idx:%d has used\n", ridx);
			_os_warn_on(1);
		} else {
			mr_ctl->role_map |= BIT(ridx);
			role_idx = ridx;
		}
	}
	_os_spinunlock(phl_to_drvpriv(phl_info), &mr_ctl->lock, _ps, NULL);

	if (role_idx == INVALID_WIFI_ROLE_IDX) {
		PHL_ERR("%s Can't get available role idx\n", __func__);
		_os_warn_on(1);
		return role;
	}

	role = &phl_com->wifi_roles[role_idx];
	role->type = type;
	role->mstate = MLME_NO_LINK;
	role->chanctx = NULL;
	hw_band = 0;

	/*alloc hw_band and hw_port,hw_wmm*/
	if (_phl_alloc_hw_resource(phl_info, role, hw_band) != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s alloc_hw resource failed\n", __func__);
		_os_warn_on(1);
		role = NULL;
		return role;
	}

#ifdef RTW_WKARD_STA_BCN_INTERVAL
#ifdef RTW_PHL_BCN
	if (role->type == PHL_RTYPE_AP) {
		role->bcn_cmn.bcn_interval = 100;
		role->mstate = MLME_LINKED;
	}
#endif
#endif

	_os_mem_cpy(phl_to_drvpriv(phl_info),
			role->mac_addr, mac_addr, MAC_ALEN);

	phl_init_role_cap(phl_info, role);
	phl_init_protocol_cap(phl_info, role);

	return role;
}

static enum rtw_phl_status
_phl_wifi_role_start(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole,
				struct rtw_phl_stainfo_t *sta)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hsts = RTW_HAL_STATUS_FAILURE;

	if (sta->active == false) {
		psts = phl_alloc_stainfo_hw(phl_info, sta);
		if (psts != RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("%s can't alloc self stainfo_hw\n", __func__);
			_os_warn_on(1);
			goto _exit;
		}
		PHL_INFO("%s sta_info hw - macid:%u %02x:%02x:%02x:%02x:%02x:%02x\n",
			__func__, sta->macid,
			sta->mac_addr[0], sta->mac_addr[1], sta->mac_addr[2],
			sta->mac_addr[3], sta->mac_addr[4], sta->mac_addr[5]);
	}

	hsts = rtw_hal_role_cfg(phl_info->hal, wrole);
	if(hsts != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("%s hal role cfg failed\n", __func__);
		_os_warn_on(1);
		goto _exit;
	}
	wrole->active = true;
	_phl_role_notify(phl_info, wrole, PHL_ROLE_START);
	psts = RTW_PHL_STATUS_SUCCESS;
	return psts;

_exit:
	return psts;
}

static enum rtw_phl_status
_phl_wifi_role_free_sw(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);

	_phl_free_hw_resource(phl_info, wrole);
	_os_mem_set(phl_to_drvpriv(phl_info), wrole->mac_addr, 0, MAC_ALEN);
	_os_spinlock(phl_to_drvpriv(phl_info), &mr_ctl->lock, _ps, NULL);
	mr_ctl->role_map &= ~BIT(wrole->id);
	_os_spinunlock(phl_to_drvpriv(phl_info), &mr_ctl->lock, _ps, NULL);
	wrole->active = false;
	return RTW_PHL_STATUS_SUCCESS;
}

#ifdef CONFIG_CMD_DISP
struct wr_start_param {
	struct rtw_wifi_role_t *wrole;
	struct rtw_phl_stainfo_t *sta;
};

enum rtw_phl_status
phl_wifi_role_start_hdl(struct phl_info_t *phl_info, u8 *param)
{
	struct wr_start_param *cmd_wr = (struct wr_start_param *)param;

	return _phl_wifi_role_start(phl_info, cmd_wr->wrole, cmd_wr->sta);
}

void phl_wifi_role_start_done(void *drv_priv, u8 *cmd, u32 cmd_len,
						enum rtw_phl_status status)
{
	if (cmd) {
		_os_kmem_free(drv_priv, cmd, cmd_len);
		cmd = NULL;
		PHL_INFO("%s.....\n", __func__);
	}
}

enum rtw_phl_status
phl_wifi_role_start(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole,
				struct rtw_phl_stainfo_t *sta)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	struct wr_start_param *wr_start = NULL;
	void *drv = phl_to_drvpriv(phl_info);
	u32 wr_start_len;

	wr_start_len = sizeof(struct wr_start_param);
	wr_start = _os_kmem_alloc(drv, wr_start_len);
	if (wr_start == NULL) {
		PHL_ERR("%s: alloc wr_start_param failed!\n", __func__);
		psts = RTW_PHL_STATUS_RESOURCE;
		goto _exit;
	}
	wr_start->wrole = wrole;
	wr_start->sta = sta;

	psts = phl_cmd_enqueue(phl_info,
				wrole->hw_band,
				MSG_EVT_ROLE_START,
				(u8*)wr_start,
				wr_start_len,
				phl_wifi_role_start_done,
				PHL_CMD_WAIT,
				0);

	if (is_cmd_failure(psts)) {
		/* Send cmd success, but wait cmd fail*/
		psts = RTW_PHL_STATUS_FAILURE;
		PHL_INFO("%s wr-id:%d failure\n", __func__, wrole->id);
	} else if (psts != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		_os_kmem_free(drv, wr_start, wr_start_len);
		psts = RTW_PHL_STATUS_FAILURE;
		PHL_INFO("%s wr-id:%d failure\n", __func__, wrole->id);
	} else {
		PHL_INFO("%s wr-id:%d success\n", __func__, wrole->id);
	}

_exit:
	return psts;
}
#else
enum rtw_phl_status
phl_wifi_role_start(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole,
				struct rtw_phl_stainfo_t *sta)
{
	return _phl_wifi_role_start(phl_info, wrole, sta);
}
#endif

u8 rtw_phl_wifi_role_alloc(void *phl, u8 *mac_addr, enum role_type type,
				u8 ridx, struct rtw_wifi_role_t **wifi_role, bool ignore_hw_fail)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	u8 role_idx = INVALID_WIFI_ROLE_IDX;
	struct rtw_wifi_role_t *role = NULL;
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;

	if (mac_addr == NULL) {
		PHL_ERR("%s mac_addr == NULL\n", __func__);
		goto _exit;
	}

	/*search rid by mac_addr, if had assigned then return role index*/
	role_idx = _phl_search_roleidx_by_addr(phl_info, mac_addr);
	if (role_idx != INVALID_WIFI_ROLE_IDX) {
		PHL_INFO("%s wifi-role(%d) had allociated\n", __func__, role_idx);
		*wifi_role = &phl_com->wifi_roles[role_idx];
		goto _exit;
	}

	role = _phl_wifi_role_alloc_sw(phl_info, mac_addr, type, ridx);
	if (role == NULL) {
		PHL_ERR("%s role alloc sw failed\n", __func__);
		_os_warn_on(1);
		goto _exit;
	}

	/*alloc sta_info for self*/
	phl_sta = phl_alloc_stainfo_sw(phl_info, role->mac_addr, role);
	if (phl_sta == NULL) {
		PHL_ERR("%s can't alloc self stainfo_sw\n", __func__);
		_os_warn_on(1);
		goto _err_stainfo_sw;
	}

	psts = phl_wifi_role_start(phl_info, role, phl_sta);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s role start failed\n", __func__);
		if (!ignore_hw_fail) {
			_os_warn_on(1);
			goto _err_role_start;
		}
	}

	*wifi_role = role;
	role_idx = role->id;
	PHL_DUMP_MR_EX(phl_info);
	return role_idx;

_err_role_start:
	phl_wifi_role_free_stainfo_sw(phl_info, role);

_err_stainfo_sw:
	_phl_wifi_role_free_sw(phl_info, role);

_exit:
	return role_idx;
}

enum rtw_phl_status
phl_role_noa_notify(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	_phl_role_notify(phl_info, wrole, PHL_ROLE_UPDATE_NOA);

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_role_ap_client_notify(struct phl_info_t *phl_info,
			struct rtw_wifi_role_t *wrole, u8 link_sts)
{
	enum role_state rstate;

	if (link_sts == MLME_LINKING)
		rstate = PHL_ROLE_MSTS_STA_CONN_START;
	else if (link_sts == MLME_LINKED)
		rstate = PHL_ROLE_MSTS_STA_CONN_END;
	else
		rstate = PHL_ROLE_MSTS_STA_DIS_CONN;

	_phl_role_notify(phl_info, wrole, rstate);
#ifdef CONFIG_MCC_SUPPORT
	phl_mcc_client_link_notify_for_ap(phl_info, wrole, rstate);
#endif /* CONFIG_MCC_SUPPORT */
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_role_notify(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	enum role_state rstate = PHL_ROLE_STATE_UNKNOWN;

	switch (wrole->type) {
	case PHL_RTYPE_STATION:
	case PHL_RTYPE_P2P_GC:
	{
		if (wrole->mstate == MLME_LINKING)
			rstate = PHL_ROLE_MSTS_STA_CONN_START;
		else if (wrole->mstate == MLME_LINKED)
			rstate = PHL_ROLE_MSTS_STA_CONN_END;
		else
			rstate = PHL_ROLE_MSTS_STA_DIS_CONN;
	}
	break;

	case PHL_RTYPE_AP:
	case PHL_RTYPE_VAP:
	case PHL_RTYPE_MESH:
	case PHL_RTYPE_P2P_GO:
	{
		rstate = (wrole->mstate == MLME_NO_LINK)
			? PHL_ROLE_MSTS_AP_STOP
			: PHL_ROLE_MSTS_AP_START;
	}
	break;

	case PHL_RTYPE_NONE:
	case PHL_RTYPE_ADHOC:
	case PHL_RTYPE_ADHOC_MASTER:
	case PHL_RTYPE_MONITOR:
	case PHL_RTYPE_P2P_DEVICE:
	case PHL_RTYPE_NAN:
	case PHL_MLME_MAX:
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_,
			"%s: Unsupported case:%d in wrole notify, please check it\n",
			__func__, wrole->type);
		break;
	default:
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_,
			"%s role-type(%d) not support\n",
			__func__, wrole->type);
		break;
	}

	_phl_role_notify(phl_info, wrole, rstate);
	return RTW_PHL_STATUS_SUCCESS;
}


/**
 * This function is called once wifi info changed
 * (see enum wr_chg_id)
 * @phl: see phl_info_t
 * @wrole: information is updated for this wifi role
 * @chg_id: see enum wr_chg_id
 * @chg_info: the change info to be update
 */
enum rtw_phl_status
phl_wifi_role_change(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole,
				enum wr_chg_id chg_id, void *chg_info)
{
	enum rtw_phl_status pstate = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hstate;

	struct rtw_phl_stainfo_t *sta = NULL;
	enum phl_upd_mode mode = PHL_UPD_ROLE_MAX;
	void *drv = phl_to_drvpriv(phl_info);

	switch (chg_id) {
	case WR_CHG_TYPE:
	{
		enum role_type type = *(enum role_type *)chg_info;

		if (wrole->type == type) {
			PHL_WARN("wrole type(%d) not change\n", wrole->type);
			pstate = RTW_PHL_STATUS_SUCCESS;
			return pstate;
		}
		PHL_INFO("wrole type(%d) change to type(%d)\n",
							wrole->type, type);

		sta = rtw_phl_get_stainfo_self(phl_info, wrole);
		if (sta) {
			if (type == PHL_RTYPE_TDLS || wrole->type == PHL_RTYPE_TDLS) {
				wrole->type = type;
				pstate = RTW_PHL_STATUS_SUCCESS;
			}
			else {
				wrole->type = type;
				wrole->mstate = MLME_NO_LINK;

				#ifdef RTW_WKARD_STA_BCN_INTERVAL
				#ifdef RTW_PHL_BCN
				if (wrole->type == PHL_RTYPE_AP) {
					wrole->bcn_cmn.bcn_interval = 100;
					wrole->mstate = MLME_LINKED;
					wrole->hiq_win = 16; /* unit: ms */
				}
				#endif
				#endif
				phl_init_role_cap(phl_info, wrole);
				phl_init_protocol_cap(phl_info, wrole);

				rtw_hal_role_cfg(phl_info->hal, wrole);

				mode = PHL_UPD_ROLE_TYPE_CHANGE;
				_os_mem_cpy(drv, sta->mac_addr, wrole->mac_addr, MAC_ALEN);
				pstate = phl_change_stainfo(phl_info, sta, mode);
				/*rtw_hal_change_sta_entry(phl_info->hal, sta);*/
				_phl_role_notify(phl_info, wrole, PHL_ROLE_CHG_TYPE);
			}
		}
		else {
			PHL_ERR("cannot get stainfo_self\n");
		}
		PHL_DUMP_MR_EX(phl_info);
	}
		break;
	case WR_CHG_MADDR:
	{
		u8 *maddr = (u8 *)chg_info;

		if(_os_mem_cmp(drv, wrole->mac_addr, maddr,  MAC_ALEN) == 0) {
			PHL_WARN("wrole maddr %02x:%02x:%02x:%02x:%02x:%02x not change\n",
			         maddr[0], maddr[1], maddr[2],
			         maddr[3], maddr[4], maddr[5]);
			return RTW_PHL_STATUS_SUCCESS;
		}
		PHL_INFO("wrole maddr %02x:%02x:%02x:%02x:%02x:%02x change to %02x:%02x:%02x:%02x:%02x:%02x\n",
		         wrole->mac_addr[0], wrole->mac_addr[1],
		         wrole->mac_addr[2], wrole->mac_addr[3],
		         wrole->mac_addr[4], wrole->mac_addr[5],
		         maddr[0], maddr[1], maddr[2],
		         maddr[3], maddr[4], maddr[5]);

		_os_mem_cpy(drv, wrole->mac_addr, maddr, MAC_ALEN);
		sta = rtw_phl_get_stainfo_self(phl_info, wrole);
		if (sta) {
			mode = PHL_UPD_ROLE_INFO_CHANGE;
			_os_mem_cpy(drv, sta->mac_addr, wrole->mac_addr, MAC_ALEN);
			pstate = phl_change_stainfo(phl_info, sta, mode);
			/*rtw_hal_change_sta_entry(phl_info->hal, sta);*/
		}
	}
		break;
	case WR_CHG_AP_PARAM:
	{
		struct rtw_ap_param *param = (struct rtw_ap_param *)chg_info;

		if (wrole->type != PHL_RTYPE_AP)
			break;

		if (param->cfg_id == CFG_BSS_CLR) {
			u32 bsscolor = param->value;

			wrole->proto_role_cap.bsscolor = (u8)bsscolor;
			rtw_hal_role_cfg_ex(phl_info->hal, wrole, PCFG_BSS_CLR, &bsscolor);
		}
		/* else if (param->cfg_id == CFG_HIQ_DTIM) */
		else {
			PHL_INFO("wrole->type(%d) WR_CHG_AP_PARAM todo.....\n", wrole->type);
		}

		pstate = RTW_PHL_STATUS_SUCCESS;
	}
		break;
	case WR_CHG_EDCA_PARAM:
	{
		struct rtw_edca_param *param = (struct rtw_edca_param*)chg_info;

		hstate = rtw_hal_set_edca(phl_info->hal, wrole, param->ac,
						param->param);
		if (hstate == RTW_HAL_STATUS_SUCCESS)
			pstate = RTW_PHL_STATUS_SUCCESS;
	}
		break;
	case WR_CHG_MU_EDCA_PARAM:
	{
		struct rtw_mu_edca_param *param = (struct rtw_mu_edca_param*)chg_info;
		hstate = rtw_hal_set_mu_edca(phl_info->hal, wrole->hw_band, param->ac,
					param->timer, (param->cw & 0x0f), (param->cw>>4),
					param->aifsn);
		if (hstate == RTW_HAL_STATUS_SUCCESS)
			pstate = RTW_PHL_STATUS_SUCCESS;
	}
		break;
	case WR_CHG_MU_EDCA_CFG:
	{
		u8 val = *(u8 *)chg_info;
		hstate = rtw_hal_set_mu_edca_ctrl(phl_info->hal,
					wrole->hw_band, wrole->hw_wmm, val);
		if (hstate == RTW_HAL_STATUS_SUCCESS)
			pstate = RTW_PHL_STATUS_SUCCESS;
	}
		break;
	case WR_CHG_BSS_COLOR:
	{
		u32 val = *(u8 *)chg_info;
		hstate = rtw_hal_role_cfg_ex(phl_info->hal, wrole, PCFG_BSS_CLR, &val);
		if (hstate == RTW_HAL_STATUS_SUCCESS)
			pstate = RTW_PHL_STATUS_SUCCESS;
	}
		break;
	case WR_CHG_RTS_TH:
	{
		struct rtw_rts_threshold *val =
				(struct rtw_rts_threshold *)chg_info;

		hstate = rtw_hal_config_rts_th(phl_info->hal,
					       wrole->hw_band,
					       val->rts_time_th,
					       val->rts_len_th);
		if (hstate == RTW_HAL_STATUS_SUCCESS)
			pstate = RTW_PHL_STATUS_SUCCESS;
	}
	break;
	case WR_CHG_DFS_HE_TB_CFG:
	{
		u8 val = *(u8 *)chg_info;

		hstate = rtw_hal_set_dfs_tb_ctrl(phl_info->hal, val);
		if (hstate == RTW_HAL_STATUS_SUCCESS)
			pstate = RTW_PHL_STATUS_SUCCESS;
	}
	break;
	case WR_CHG_TRX_PATH:
	{
		struct rtw_trx_path_param *param = (struct rtw_trx_path_param*)chg_info;

		hstate = rtw_hal_cfg_trx_path(phl_info->hal,
					      param->tx,
					      param->tx_nss,
					      param->rx,
					      param->rx_nss);

		if (hstate == RTW_HAL_STATUS_SUCCESS)
			pstate = RTW_PHL_STATUS_SUCCESS;
	}
	break;
	case WR_CHG_MAX:
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_,
				"%s: Unsupported case:%d, please check it\n",
				__func__, chg_id);
		break;
	default :
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_,
				"%s: unknown chg_id(%d), please check it\n",
				__func__, chg_id);
		break;
	};
	return pstate;
}

#ifdef CONFIG_CMD_DISP
struct wr_chg_param {
	struct rtw_wifi_role_t *wrole;
	enum wr_chg_id id;
	u8 *info;
	u8 info_len;
};

enum rtw_phl_status
phl_wifi_role_chg_hdl(struct phl_info_t *phl_info, u8 *param)
{
	struct wr_chg_param *wr_chg = (struct wr_chg_param *)param;

	return phl_wifi_role_change(phl_info, wr_chg->wrole, wr_chg->id, wr_chg->info);
}

void phl_wifi_role_chg_done(void *drv_priv, u8 *cmd, u32 cmd_len,
						enum rtw_phl_status status)
{
	struct wr_chg_param *wr_chg = NULL;

	if (cmd == NULL || cmd_len == 0) {
		PHL_ERR("%s buf == NULL || buf_len == 0\n", __func__);
		_os_warn_on(1);
		return;
	}

	wr_chg = (struct wr_chg_param *)cmd;
	PHL_INFO("%s - id:%d .....\n", __func__, wr_chg->id);

	if (wr_chg->info)
		_os_kmem_free(drv_priv, wr_chg->info, wr_chg->info_len);

	_os_kmem_free(drv_priv, cmd, cmd_len);
	cmd = NULL;
}

enum rtw_phl_status
rtw_phl_cmd_wrole_change(void *phl,
                         struct rtw_wifi_role_t *wrole,
                         enum wr_chg_id chg_id,
                         u8 *chg_info,
                         u8 chg_info_len,
                         enum phl_cmd_type cmd_type,
                         u32 cmd_timeout)
{

	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	void *drv = phl_to_drvpriv(phl_info);
	struct wr_chg_param *wr_chg = NULL;
	u32 wr_chg_len = 0;

	if (cmd_type == PHL_CMD_DIRECTLY) {
		psts = phl_wifi_role_change(phl_info, wrole, chg_id, chg_info);
		goto _exit;
	}

	wr_chg_len = sizeof(struct wr_chg_param);
	wr_chg = _os_kmem_alloc(drv, wr_chg_len);
	if (wr_chg == NULL) {
		PHL_ERR("%s: alloc wr_chg_param failed!\n", __func__);
		psts = RTW_PHL_STATUS_RESOURCE;
		goto _exit;
	}
	_os_mem_set(drv, wr_chg, 0, wr_chg_len);
	wr_chg->wrole = wrole;
	wr_chg->id = chg_id;

	wr_chg->info_len = chg_info_len;
	wr_chg->info = _os_kmem_alloc(drv, chg_info_len);
	if (wr_chg->info == NULL) {
		PHL_ERR("%s: alloc wr_chg_info failed!\n", __func__);
		psts = RTW_PHL_STATUS_RESOURCE;
		goto _err_info;
	}
	_os_mem_set(drv, wr_chg->info, 0, chg_info_len);
	_os_mem_cpy(drv, wr_chg->info, chg_info, chg_info_len);

	psts = phl_cmd_enqueue(phl_info,
	                       wrole->hw_band,
	                       MSG_EVT_ROLE_CHANGE,
	                       (u8*)wr_chg,
	                       wr_chg_len,
	                       phl_wifi_role_chg_done,
	                       cmd_type,
	                       cmd_timeout);

	if (is_cmd_failure(psts)) {
		/* Send cmd success, but wait cmd fail*/
		psts = RTW_PHL_STATUS_FAILURE;
		PHL_INFO("%s wr-id:%d failure\n", __func__, wrole->id);
	} else if (psts != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		psts = RTW_PHL_STATUS_FAILURE;
		PHL_INFO("%s wr-id:%d failure\n", __func__, wrole->id);
		goto _err_cmd;
	} else {
		PHL_INFO("%s wr-id:%d success\n", __func__, wrole->id);
	}

	return psts;

_err_cmd:
	if (wr_chg->info)
		_os_kmem_free(drv, wr_chg->info, wr_chg->info_len);
_err_info:
	if (wr_chg)
		_os_kmem_free(drv, wr_chg, wr_chg_len);
_exit:
	return psts;
}

#else
enum rtw_phl_status
rtw_phl_cmd_wrole_change(void *phl,
			struct rtw_wifi_role_t *wrole,
			enum wr_chg_id chg_id, u8 *chg_info, u8 chg_info_len,
			enum phl_cmd_type cmd_type, u32 cmd_timeout)
{
	return phl_wifi_role_change((struct phl_info_t *)phl, wrole, chg_id, chg_info);
}
#endif /*CONFIG_CMD_DISP*/

enum rtw_phl_status
_phl_wifi_role_stop(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;

	wrole->active = false;
	_phl_role_notify(phl_info, wrole, PHL_ROLE_STOP);
	psts = phl_wifi_role_free_stainfo_hw(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s wr free stainfo_hw failed\n", __func__);
		_os_warn_on(1);
	}

	/*hw port cfg - mac_port_deinit*/
	return psts;
}
#ifdef CONFIG_CMD_DISP
enum rtw_phl_status
phl_wifi_role_stop_hdl(struct phl_info_t *phl_info, u8 *param)
{
	struct rtw_wifi_role_t *wrole = (struct rtw_wifi_role_t *)param;

	return _phl_wifi_role_stop(phl_info, wrole);
}
void phl_wifi_role_stop_done(void *drv_priv, u8 *cmd, u32 cmd_len,
						enum rtw_phl_status status)
{
	if (cmd) {
		struct rtw_wifi_role_t *wrole = NULL;

		wrole = (struct rtw_wifi_role_t *)cmd;
		if (is_cmd_failure(status) && (RTW_PHL_STATUS_SUCCESS != status))
			PHL_ERR("%s wrole(%d) fail status(%d).....\n",
				__func__, wrole->id, status);
		else
			PHL_INFO("%s wrole(%d) success.....\n",
				__func__, wrole->id);
	}
}

enum rtw_phl_status
phl_wifi_role_stop(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;

	psts = phl_cmd_enqueue(phl_info,
			wrole->hw_band,
			MSG_EVT_ROLE_STOP,
			(u8 *)wrole,
			0,
			phl_wifi_role_stop_done,
			PHL_CMD_WAIT, 0);

	if (is_cmd_failure(psts)) {
		/* Send cmd success, but wait cmd fail*/
		psts = RTW_PHL_STATUS_FAILURE;
		PHL_INFO("%s wr-id:%d failure\n", __func__, wrole->id);
	} else if (psts != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		psts = RTW_PHL_STATUS_FAILURE;
		PHL_INFO("%s wr-id:%d failure\n", __func__, wrole->id);
	} else {
		PHL_INFO("%s wr-id:%d success\n", __func__, wrole->id);
	}

	return psts;
}
#else
enum rtw_phl_status
phl_wifi_role_stop(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	return _phl_wifi_role_stop(phl_info, wrole);
}
#endif
void rtw_phl_wifi_role_free(void *phl, u8 role_idx)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct rtw_wifi_role_t *wrole = NULL;

	if (role_idx >= MAX_WIFI_ROLE_NUMBER) {
		PHL_ERR("%s invalid role index :%d", __func__, role_idx);
		return;
	}

	wrole = &phl_com->wifi_roles[role_idx];

	if (phl_wifi_role_stop(phl_info, wrole) != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s role_stop failed :%d", __func__, role_idx);
		_os_warn_on(1);
	}

	if (phl_wifi_role_free_stainfo_sw(phl_info, wrole) != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s wr free stainfo_sw failed\n", __func__);
		_os_warn_on(1);
	}
	_phl_wifi_role_free_sw(phl_info, wrole);
	PHL_DUMP_MR_EX(phl_info);
}

#ifdef CONFIG_CMD_DISP
enum rtw_phl_status
phl_register_mrc_module(struct phl_info_t *phl_info)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_info->phl_com);
	struct phl_bk_module_ops bk_ops = mr_ctl->bk_ops;
	struct phl_cmd_dispatch_engine *disp_eng = &(phl_info->disp_eng);
	u8 i = 0;

	for(i = 0; i < disp_eng->phy_num; i++)
	{
		phl_status = phl_disp_eng_register_module(phl_info, i,
						       PHL_MDL_MRC,
						       &bk_ops);
		if (RTW_PHL_STATUS_SUCCESS != phl_status) {
			PHL_ERR("%s register MRC module in cmd disp failed :%d\n", __func__, i+1);
			goto error_register_bk;
		}
	}

	return phl_status;

error_register_bk:
	while(i > 0){
		phl_status = phl_disp_eng_deregister_module(phl_info, --i,
							 PHL_MDL_MRC);
		if (RTW_PHL_STATUS_SUCCESS != phl_status) {
			PHL_ERR("%s deregister MRC module in cmd disp failed :%d\n", __func__, i+1);
		}
	}
	return RTW_PHL_STATUS_FAILURE;
}
#endif

enum rtw_phl_status
phl_role_recover(struct phl_info_t *phl_info)
{
	u8 role_idx;
	struct rtw_wifi_role_t *wrole;
	struct rtw_phl_stainfo_t *sta;
	enum rtw_phl_status pstatus;

	for (role_idx = 0; role_idx < MAX_WIFI_ROLE_NUMBER; role_idx++) {
		wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, role_idx);
		if(wrole == NULL)
			continue;

		PHL_INFO("%s with role_idx %d\n", __func__, role_idx);

		sta = rtw_phl_get_stainfo_self(phl_info, wrole);
		if(sta == NULL)
			continue;

		pstatus = _phl_wifi_role_start(phl_info, wrole, sta);
		if (pstatus != RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("%s wifi role start failed\n", __func__);
			return RTW_PHL_STATUS_FAILURE;
		}

		if (wrole->target_type != PHL_RTYPE_NONE) {
			PHL_INFO("%s with role_idx %d change to role type %d\n", __func__, role_idx, wrole->target_type);
			phl_wifi_role_change(phl_info, wrole, WR_CHG_TYPE, (u8 *)&wrole->target_type);
			wrole->target_type = PHL_RTYPE_NONE;
		}
	}

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_cmd_role_recover(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

#ifdef CONFIG_CMD_DISP
	pstatus = phl_cmd_enqueue(phl_info, HW_BAND_0, MSG_EVT_ROLE_RECOVER, NULL, 0, NULL, PHL_CMD_WAIT, 0);
	if (is_cmd_failure(pstatus)) {
		/* Send cmd success, but wait cmd fail*/
		pstatus = RTW_PHL_STATUS_FAILURE;
	} else if (pstatus != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		pstatus = RTW_PHL_STATUS_FAILURE;
	}
#else
	pstatus = phl_role_recover(phl_info);
#endif
	return pstatus;
}

enum rtw_phl_status
phl_role_suspend(struct phl_info_t *phl_info)
{
	u8 role_idx;
	struct rtw_wifi_role_t *wrole;
	enum rtw_phl_status pstatus;

	for (role_idx = 0; role_idx < MAX_WIFI_ROLE_NUMBER; role_idx++) {
		wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, role_idx);
		if(wrole == NULL)
			continue;

		PHL_INFO("%s with role_idx %d\n", __func__, role_idx);

		pstatus = _phl_wifi_role_stop(phl_info, wrole);
		if (pstatus != RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("%s wifi role stop failed\n", __func__);
			return RTW_PHL_STATUS_FAILURE;
		}
	}

	return RTW_PHL_STATUS_SUCCESS;
}
enum rtw_phl_status
phl_cmd_role_suspend(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

#ifdef CONFIG_CMD_DISP
	pstatus = phl_cmd_enqueue(phl_info, HW_BAND_0, MSG_EVT_ROLE_SUSPEND, NULL, 0, NULL, PHL_CMD_WAIT, 0);
	if (is_cmd_failure(pstatus)) {
		/* Send cmd success, but wait cmd fail*/
		pstatus = RTW_PHL_STATUS_FAILURE;
	} else if (pstatus != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		pstatus = RTW_PHL_STATUS_FAILURE;
	}
#else
	pstatus = phl_role_suspend(phl_info);
#endif
	return pstatus;
}

void phl_role_config_tbtt_agg(struct phl_info_t *phl_info,
	struct rtw_wifi_role_t *cur_wrole, u32 tbtt_agg_val)
{
	u8 role_idx;
	struct rtw_wifi_role_t * wrole;
	u32 tbtt_agg = tbtt_agg_val;

	if (cur_wrole == NULL) {
		PHL_ERR("%s cur role is NULL\n", __func__);
		return;
	}

	for (role_idx = 0; role_idx < MAX_WIFI_ROLE_NUMBER; role_idx++) {
		wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, role_idx);
		if(wrole == NULL || !wrole->active)
			continue;

		if (wrole == cur_wrole)
			continue;

		PHL_INFO("%s role %d config tbtt agg = %d\n", __func__, role_idx, tbtt_agg_val);
		rtw_hal_role_cfg_ex(phl_info->hal, wrole, PCFG_TBTT_AGG, &tbtt_agg);
	}
}

bool rtw_phl_role_is_client_category(struct rtw_wifi_role_t *wrole)
{
	bool ret = false;

	if (wrole->type == PHL_RTYPE_STATION || wrole->type == PHL_RTYPE_P2P_GC)
		ret = true;
	return ret;
}

u16 phl_role_get_bcn_intvl(struct phl_info_t *phl, struct rtw_wifi_role_t *wrole)
{
	struct rtw_phl_stainfo_t *sta = rtw_phl_get_stainfo_self(phl, wrole);
	u16 bcn_intvl = 0;

	if (phl_role_is_client_category(wrole)) {
		bcn_intvl = sta->asoc_cap.bcn_interval;
#ifdef RTW_PHL_BCN
	} else if (phl_role_is_ap_category(wrole)) {
		bcn_intvl = (u16)wrole->bcn_cmn.bcn_interval;
#endif
	} else {
		PHL_WARN("phl_role_get_bcn_intvl() Unknown category, role id(%d), type(%d)\n",
			wrole->id, wrole->type);
	}
	return bcn_intvl;
}

enum rtw_phl_status
phl_wifi_role_macid_all_pause(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole, bool pause)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	u8 mutli_macid = false;

	switch (wrole->type) {
	case PHL_RTYPE_NONE:
	case PHL_RTYPE_STATION:
	case PHL_RTYPE_ADHOC:
	case PHL_RTYPE_P2P_DEVICE:
	case PHL_RTYPE_P2P_GC:
		mutli_macid = false;
		break;
	default:
		mutli_macid = true;
		break;
	}

	if (mutli_macid) {
		void *drv = phl_to_drvpriv(phl_info);
		struct macid_ctl_t *mctrl = phl_to_mac_ctrl(phl_info);
		u32 macid_pause[PHL_MACID_MAX_ARRAY_NUM] = {0};

		_os_spinlock(phl_to_drvpriv(phl_info), &mctrl->lock, _bh, NULL);
		_os_mem_cpy(drv, macid_pause, &mctrl->wifi_role_usedmap[wrole->id][0], PHL_MACID_MAX_ARRAY_NUM);
		_os_spinunlock(phl_to_drvpriv(phl_info), &mctrl->lock, _bh, NULL);
		hstatus = rtw_hal_set_macid_grp_pause(phl_info->hal, macid_pause, PHL_MACID_MAX_ARRAY_NUM, pause);
		if (hstatus != RTW_HAL_STATUS_SUCCESS) {
			PHL_INFO("%s fail(hstatus=%d)\n", __func__, hstatus);
			pstatus = RTW_PHL_STATUS_FAILURE;
		}
	} else {
		struct rtw_phl_stainfo_t * self_sta = rtw_phl_get_stainfo_self((void *)phl_info, wrole);

		hstatus = rtw_hal_set_macid_pause(phl_info->hal, self_sta->macid, pause);
		if (hstatus != RTW_HAL_STATUS_SUCCESS) {
			PHL_INFO("%s fail(hstatus=%d)\n", __func__, hstatus);
			pstatus = RTW_PHL_STATUS_FAILURE;
		}
	}

	return pstatus;
}
