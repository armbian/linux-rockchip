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
#define _PHL_MR_C_
#include "phl_headers.h"
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
#include "phl_scan.h"
#endif

#ifdef DBG_PHL_MR
void phl_mr_dump_role_info(const char *caller, const int line, bool show_caller,
	struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	if (show_caller)
		PHL_INFO("###### FUN - %s LINE - %d #######\n", caller, line);

	PHL_INFO("\t[WROLE] RIDX:%d - MAC-Addr:%02x-%02x-%02x-%02x-%02x-%02x RTYPE:%d MSTS:%d\n",
		wrole->id, wrole->mac_addr[0], wrole->mac_addr[1], wrole->mac_addr[2],
		wrole->mac_addr[3], wrole->mac_addr[4], wrole->mac_addr[5],
		wrole->type, wrole->mstate);
	PHL_INFO("\t[WROLE] HW Band_idx:%d, Port_idx:%d, WMM_idx:%d\n", wrole->hw_band, wrole->hw_port, wrole->hw_wmm);

	if (wrole->type == PHL_RTYPE_AP || wrole->type == PHL_RTYPE_P2P_GO || wrole->type == PHL_RTYPE_MESH) {
		#ifdef RTW_PHL_BCN
		PHL_INFO("\t[WROLE AP] BSSID:%02x-%02x-%02x-%02x-%02x-%02x\n",
			wrole->bcn_cmn.bssid[0], wrole->bcn_cmn.bssid[1], wrole->bcn_cmn.bssid[2],
			wrole->bcn_cmn.bssid[3], wrole->bcn_cmn.bssid[4], wrole->bcn_cmn.bssid[5]);
		PHL_INFO("\t[WROLE AP] BCN id:%d, interval:%d, rate:0x%04x, DTIM:%d\n",
			wrole->bcn_cmn.bcn_id, wrole->bcn_cmn.bcn_interval,
			wrole->bcn_cmn.bcn_rate, wrole->bcn_cmn.bcn_dtim);
		PHL_INFO("\t[WROLE AP] HW MBSSID idx:%d, MBID NUM:%d\n",
			wrole->hw_mbssid, wrole->mbid_num);
		#endif
	}
	PHL_INFO("\n");
	if (show_caller)
		PHL_INFO("#################################\n");
}


void phl_mr_dump_chctx_info(const char *caller, const int line, bool show_caller,
	struct phl_info_t *phl_info, struct phl_queue *chan_ctx_queue, struct rtw_chan_ctx *chanctx)
{
	u8 role_num = 0;
	role_num = phl_chanctx_get_rnum(phl_info, chan_ctx_queue, chanctx);

	if (show_caller)
		PHL_INFO("###### FUN - %s LINE - %d #######\n", caller, line);

	PHL_INFO("\t[CH-CTX] role num:%d map:0x%02x, DFS enable:%s\n",
		role_num, chanctx->role_map,
		(chanctx->dfs_enabled) ? "Y" : "N");
	PHL_DUMP_CHAN_DEF(&chanctx->chan_def);

	if (show_caller)
		PHL_INFO("#################################\n");
}

const char *const _opmod_str[] = {
	"MR_OP_NON",
	"MR_OP_SCC",
	"MR_OP_MCC",
	"MR_OP_MAX"
};
#define _get_opmod_str(opmod) (((opmod) >= MR_OP_MAX) ? _opmod_str[MR_OP_MAX] : _opmod_str[(opmod)])

void phl_mr_dump_band_info(const char *caller, const int line, bool show_caller,
	struct phl_info_t *phl_info, struct hw_band_ctl_t *band_ctrl)
{
	u8 role_num = 0;
	int chanctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);

	role_num = phl_mr_get_role_num(phl_info, band_ctrl);

	if (show_caller)
		PHL_INFO("###### FUN - %s LINE - %d #######\n", caller, line);

	PHL_INFO("\t[BAND-%d] op_mode:%s port map:0x%02x, role num:%d map:0x%02x\n",
		band_ctrl->id, _get_opmod_str(band_ctrl->op_mode),
		band_ctrl->port_map, role_num, band_ctrl->role_map);

	/*dump mr_info*/
	PHL_INFO("\t[BAND-%d] sta_num:%d, ld_sta_num:%d, lg_sta_num:%d\n",
		band_ctrl->id, band_ctrl->cur_info.sta_num,
		band_ctrl->cur_info.ld_sta_num, band_ctrl->cur_info.lg_sta_num);
	PHL_INFO("\t[BAND-%d] ap_num:%d, ld_ap_num:%d\n",
		band_ctrl->id, band_ctrl->cur_info.ap_num, band_ctrl->cur_info.ld_ap_num);

	PHL_INFO("\t[BAND-%d] chan_ctx num:%d\n", band_ctrl->id, chanctx_num);

	if (chanctx_num) {
		struct rtw_chan_ctx *chanctx = NULL;
		_os_list *chctx_list = &band_ctrl->chan_ctx_queue.queue;
		void *drv = phl_to_drvpriv(phl_info);

		_os_spinlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
		phl_list_for_loop(chanctx, struct rtw_chan_ctx, chctx_list, list) {
			PHL_DUMP_CHAN_CTX(phl_info, &band_ctrl->chan_ctx_queue, chanctx);
		}
		_os_spinunlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
	}
	PHL_INFO("\n");
	if (show_caller)
		PHL_INFO("#################################\n");
}

void phl_mr_dump_info(const char *caller, const int line, bool show_caller,
	struct phl_info_t *phl_info)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct rtw_wifi_role_t *wrole;
	u8 i;
	u8 role_num = 0;

	if (show_caller)
		PHL_INFO("###### FUN - %s LINE - %d #######\n", caller, line);

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		if (mr_ctl->role_map & BIT(i))
			role_num++;
	}

	PHL_INFO("[MR] MAX wrole num:%d, created num:%d map:0x%02x\n",
			MAX_WIFI_ROLE_NUMBER, role_num, mr_ctl->role_map);
	PHL_INFO("[MR] is_sb:%s\n", (mr_ctl->is_sb) ? "Y" : "N");

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		if (mr_ctl->role_map & BIT(i)) {
			wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, i);
			PHL_DUMP_ROLE(phl_info, wrole);
		}
	}

	for (i = 0; i < MAX_BAND_NUM; i++)
		PHL_DUMP_BAND_CTL(phl_info, &mr_ctl->band_ctrl[i]);

	if (show_caller)
		PHL_INFO("#################################\n");
}

void phl_mr_dump_cur_chandef(const char *caller, const int line, bool show_caller,
		struct phl_info_t *phl_info, struct rtw_wifi_role_t *wifi_role)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct rtw_chan_def *chan_def = NULL;

	if (show_caller)
		PHL_INFO("###### FUN - %s LINE - %d #######\n", caller, line);

	if (wifi_role->chanctx) {
		chan_def = &wifi_role->chanctx->chan_def;
		PHL_INFO("==== MR Chan-def ===\n");
		PHL_DUMP_CHAN_DEF(chan_def);
	}

	chan_def = &wifi_role->chandef;
	PHL_INFO("==== WR-%d Chan-def ===\n", wifi_role->id);
	PHL_DUMP_CHAN_DEF(chan_def);

	chan_def = &mr_ctl->hal_com->band[wifi_role->hw_band].cur_chandef;
	PHL_INFO("==== HAL Band-%d Chan-def ===\n", wifi_role->hw_band);
	PHL_DUMP_CHAN_DEF(chan_def);

	if (show_caller)
		PHL_INFO("#################################\n");
}

#ifdef	PHL_MR_PROC_CMD
void rtw_phl_mr_dump_info(void *phl, bool show_caller)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	if (show_caller)
		PHL_DUMP_MR(phl_info);
	else
		PHL_DUMP_MR_EX(phl_info);
}
void rtw_phl_mr_dump_band_ctl(void *phl, bool show_caller)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	int i;

	for (i = 0; i < MAX_BAND_NUM; i++) {
		if (show_caller)
			PHL_DUMP_BAND_CTL(phl_info, &mr_ctl->band_ctrl[i]);
		else
			PHL_DUMP_BAND_CTL_EX(phl_info, &mr_ctl->band_ctrl[i]);

	}
}
#endif /*PHL_MR_PROC_CMD*/
#endif /*DBG_PHL_MR*/

static struct rtw_wifi_role_t *_search_ld_sta_wrole(struct rtw_wifi_role_t *wrole, u8 exclude_self)
{
	u8 ridx = 0;
	struct rtw_phl_com_t *phl_com = wrole->phl_com;
	struct rtw_chan_ctx *chanctx = wrole->chanctx;
	struct rtw_wifi_role_t *wr = NULL;

	if (chanctx == NULL) {
		PHL_ERR("%s wifi role(%d) chan ctx is null\n", __func__, wrole->id);
		goto exit;
	}

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (chanctx->role_map & BIT(ridx)) {
			wr = &phl_com->wifi_roles[ridx];
			if (wr) {
				if ((exclude_self) && (wr == wrole))
					continue;
				if (wr->type == PHL_RTYPE_STATION || wr->type == PHL_RTYPE_TDLS)
					break;
			}
		}
	}

	if (wr)
		PHL_INFO("search Linked STA wifi role (%d)\n", wr->id);
exit:
	return wr;
}

void rtw_phl_mr_dump_cur_chandef(void *phl, struct rtw_wifi_role_t *wrole)
{
	#ifdef	PHL_MR_PROC_CMD
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	PHL_DUMP_CUR_CHANDEF(phl_info, wrole);
	#endif
}


static enum rtw_phl_status
_phl_band_ctrl_init(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_info->phl_com);
	struct hw_band_ctl_t *band_ctrl;
	u8 band_idx = 0;

	for (band_idx = 0; band_idx < MAX_BAND_NUM; band_idx++) {
		band_ctrl = &(mr_ctl->band_ctrl[band_idx]);
		band_ctrl->id = band_idx;
		_os_spinlock_init(drv, &(band_ctrl->lock));
		pq_init(drv, &band_ctrl->chan_ctx_queue);
		band_ctrl->op_mode = MR_OP_NON;
		band_ctrl->tsf_sync_port = HW_PORT_MAX;
	}

	return RTW_PHL_STATUS_SUCCESS;
}

#ifdef CONFIG_CMD_DISP
static enum phl_mdl_ret_code
_phl_mrc_module_init(void *phl_info, void *dispr, void **priv)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	*priv = phl_info;
	FUNCOUT();

	ret = MDL_RET_SUCCESS;
	return ret;
}

static void
_phl_mrc_module_deinit(void *dispr, void *priv)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	FUNCOUT();

	ret = MDL_RET_SUCCESS;
}

static enum phl_mdl_ret_code
_phl_mrc_module_start(void *dispr, void *priv)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	FUNCOUT();

	ret = MDL_RET_SUCCESS;
	return ret;
}

static enum phl_mdl_ret_code
_phl_mrc_module_stop(void *dispr, void *priv)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	FUNCOUT();

	ret = MDL_RET_SUCCESS;
	return ret;
}

/* Same behaviour as rtw_phl_connect_prepare without cmd dispr */
enum rtw_phl_status
_phl_mrc_module_connect_start_hdlr(struct phl_info_t *phl_info,
				   struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
	wrole->mstate = MLME_LINKING;
	psts = phl_role_notify(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s role notify failed\n", __func__);
		goto _exit;
	}
	psts = phl_mr_info_upt(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s mr info upt failed\n", __func__);
		goto _exit;
	}

	psts = rtw_phl_mr_rx_filter(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_rx_filter failed\n", __func__);
		goto _exit;
	}

#ifdef CONFIG_PHL_P2PPS
	phl_p2pps_noa_all_role_pause(phl_info, wrole->hw_band);
#endif
#ifdef CONFIG_MCC_SUPPORT
	/* disable MR coex mechanism(if runing) */
	psts = phl_mr_coex_disable(phl_info, wrole, wrole->hw_band,
					MR_COEX_TRIG_BY_LINKING);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s: MR coex disable fail\n", __func__);
		goto _exit;
	}
#endif
	PHL_DUMP_MR_EX(phl_info);
_exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

/* Same behaviour as rtw_phl_connected without cmd dispr */
enum rtw_phl_status
_phl_mrc_module_connect_end_hdlr(struct phl_info_t *phl_info,
				 struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
	if (wrole->type == PHL_RTYPE_STATION || wrole->type == PHL_RTYPE_P2P_GC) {
		psts = phl_role_notify(phl_info, wrole);
		if (psts != RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("%s role notify failed\n", __func__);
			goto _exit;
		}
	}

	psts = phl_mr_info_upt(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s mr info upt failed\n", __func__);
		goto _exit;
	}

	psts = rtw_phl_mr_rx_filter(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_rx_filter failed\n", __func__);
		goto _exit;
	}

	psts = phl_mr_tsf_sync(phl_info, wrole, PHL_ROLE_MSTS_STA_CONN_END);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_tsf_sync failed\n", __func__);
		goto _exit;
	}
	#if 0
	psts = phl_mr_state_upt(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s phl_mr_state_upt failed\n", __func__);
		goto _exit;
	}
	#endif

	PHL_DUMP_MR_EX(phl_info);
_exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

enum rtw_phl_status
_phl_mrc_module_disconnect_pre_hdlr(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);

#ifdef CONFIG_PHL_P2PPS
	/* disable NoA for this role */
	phl_p2pps_noa_disable_all(phl_info, wrole);
	/* pasue buddy NoA */
	phl_p2pps_noa_all_role_pause(phl_info, wrole->hw_band);
#endif

#ifdef CONFIG_MCC_SUPPORT
	/* disable MR coex mechanism(if runing) */
	psts = phl_mr_coex_disable(phl_info, wrole, wrole->hw_band,
					MR_COEX_TRIG_BY_DIS_LINKING);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s: MR coex disable fail\n", __func__);
		goto _exit;
	}
#endif

_exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

/* Same behaviour as rtw_phl_disconnect without cmd dispr */
enum rtw_phl_status
_phl_mrc_module_disconnect_hdlr(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
#ifdef CONFIG_PHL_TDLS
	enum role_type rtype = PHL_RTYPE_STATION;
#endif
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
#ifdef CONFIG_PHL_TDLS
	/* Disconnected with AP while there still exist linked TDLS peers */
	if (wrole->type == PHL_RTYPE_TDLS && wrole->mstate != MLME_LINKED) {
		psts = phl_wifi_role_change(phl_info, wrole, WR_CHG_TYPE, (u8*)&rtype);
		if (psts != RTW_PHL_STATUS_SUCCESS) {
			RTW_ERR("%s - change to phl role type = %d fail with error = %d\n", __func__, rtype, psts);
			goto _exit;
		}
	}
#endif

	if (wrole->type == PHL_RTYPE_STATION || wrole->type == PHL_RTYPE_P2P_GC) {
		psts = phl_role_notify(phl_info, wrole);
		if (psts != RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("%s role notify failed\n", __func__);
			goto _exit;
		}
	}

	psts = phl_mr_info_upt(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s mr info upt failed\n", __func__);
		goto _exit;
	}

	psts = rtw_phl_mr_rx_filter(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_rx_filter failed\n", __func__);
		goto _exit;
	}

	psts = phl_mr_tsf_sync(phl_info, wrole, PHL_ROLE_MSTS_STA_DIS_CONN);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_tsf_sync failed\n", __func__);
		goto _exit;
	}

	rtw_hal_disconnect_notify(phl_info->hal, &wrole->chandef);
	PHL_DUMP_MR_EX(phl_info);
_exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

enum rtw_phl_status
_phl_mrc_module_tsf_sync_done_hdlr(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole)
{
	SET_STATUS_FLAG(wrole->status, WR_STATUS_TSF_SYNC);
	return phl_mr_state_upt(phl_info, wrole);
}

enum rtw_phl_status
_phl_mrc_module_ap_start_pre_hdlr(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
#ifdef CONFIG_PHL_P2PPS
	/* pasue all NoA */
	phl_p2pps_noa_all_role_pause(phl_info, wrole->hw_band);
#endif
#ifdef CONFIG_MCC_SUPPORT
	/* disable MR coex mechanism(if runing) */
	psts = phl_mr_coex_disable(phl_info, wrole, wrole->hw_band,
					MR_COEX_TRIG_BY_LINKING);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s: MR coex disable fail\n", __func__);
		goto _exit;
	}
#endif

_exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}
/* Same behaviour as rtw_phl_ap_started without cmd dispr */
enum rtw_phl_status
_phl_mrc_module_ap_started_hdlr(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
	psts = phl_role_notify(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s role notify failed\n", __func__);
		goto _exit;
	}
	psts = phl_mr_info_upt(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s mr info upt failed\n", __func__);
		goto _exit;
	}
	psts = rtw_phl_mr_rx_filter(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_rx_filter failed\n", __func__);
		goto _exit;
	}

	psts = phl_mr_tsf_sync(phl_info, wrole, PHL_ROLE_MSTS_AP_START);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_tsf_sync failed\n", __func__);
		goto _exit;
	}
	psts = phl_mr_state_upt(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s phl_mr_state_upt failed\n", __func__);
		goto _exit;
	}

	PHL_DUMP_MR_EX(phl_info);
_exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

enum rtw_phl_status
_phl_mrc_module_ap_stop_pre_hdlr(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);

#ifdef CONFIG_PHL_P2PPS
	/* disable NoA for this role */
	phl_p2pps_noa_disable_all(phl_info, wrole);
	/* pasue buddy NoA */
	phl_p2pps_noa_all_role_pause(phl_info, wrole->hw_band);
#endif
#ifdef CONFIG_MCC_SUPPORT
	/* disable MR coex mechanism(if runing) */
	psts = phl_mr_coex_disable(phl_info, wrole, wrole->hw_band,
					MR_COEX_TRIG_BY_DIS_LINKING);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s: MR coex disable fail\n", __func__);
		goto _exit;
	}
#endif

_exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

/* Same behaviour as rtw_phl_ap_stop without cmd dispr */
enum rtw_phl_status
_phl_mrc_module_ap_stop_hdlr(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
	wrole->mstate = MLME_NO_LINK;
	psts = phl_role_notify(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s role notify failed\n", __func__);
		goto _exit;
	}

	psts = phl_mr_info_upt(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s mr info upt failed\n", __func__);
		goto _exit;
	}
	psts = rtw_phl_mr_rx_filter(phl_info, wrole);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_rx_filter failed\n", __func__);
		goto _exit;
	}

	psts = phl_mr_tsf_sync(phl_info, wrole, PHL_ROLE_MSTS_AP_STOP);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s set mr_tsf_sync failed\n", __func__);
		goto _exit;
	}

	rtw_hal_disconnect_notify(phl_info->hal, &wrole->chandef);

	PHL_DUMP_MR_EX(phl_info);
_exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

enum phl_mdl_ret_code
_phl_mrc_module_swch_start_hdlr(void *dispr,
				void *priv,
				struct phl_msg *msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;
	struct phl_module_op_info op_info = {0};
	struct rtw_wifi_role_t *role = NULL;
	struct rtw_chan_def chandef = {0};
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	u8 module_id = MSG_MDL_ID_FIELD(msg->msg_id);
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
	struct rtw_phl_scan_param* scan_param = NULL;
	u8 (*scan_issue_null_data)(void *, u8, bool) = NULL;
#endif
	u8 idx = 0xff;

	phl_dispr_get_idx(dispr, &idx);
	/*
	* Handle mr offchan before switching channel when
	* STA connect & AP start.
	*/
	if((module_id != PHL_FG_MDL_CONNECT) &&
	   (module_id != PHL_FG_MDL_AP_START) &&
	   (module_id != PHL_FG_MDL_SCAN)){
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
		"%s: not connect/apstart/scan\n", __FUNCTION__);
		ret = MDL_RET_IGNORE;
		goto _exit;
	}
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
		"%s: MSG_EVT_SWCH_START\n", __FUNCTION__);
	op_info.op_code = FG_REQ_OP_GET_ROLE;

	if(phl_disp_eng_query_cur_cmd_info(phl_info, idx, &op_info)
		!= RTW_PHL_STATUS_SUCCESS){
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"Query wifi role fail!\n");
		goto _exit;
	}
	role = (struct rtw_wifi_role_t *)op_info.outbuf;

	if(role == NULL){
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			"%s: role is NULL\n", __FUNCTION__);
		goto _exit;
	}

	/*
	* If we are already on STA/AP channel,
	* offch is unnecessary.
	*/
	if((module_id == PHL_FG_MDL_CONNECT) ||
	   (module_id == PHL_FG_MDL_AP_START)){
	   	#ifdef CONFIG_MR_SUPPORT
		chandef = mr_ctl->hal_com->band[role->hw_band].cur_chandef;

		if(role->chandef.chan == chandef.chan){
			ret = MDL_RET_SUCCESS;
			goto _exit;
		}
		#else
		ret = MDL_RET_SUCCESS;
		goto _exit;
		#endif
	}
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
	if(module_id == PHL_FG_MDL_SCAN)
		op_info.op_code = FG_REQ_OP_GET_SCAN_PARAM;
	else
		op_info.op_code = FG_REQ_OP_GET_ISSUE_NULL_OPS;

	if(phl_disp_eng_query_cur_cmd_info(phl_info, idx, &op_info)
		!= RTW_PHL_STATUS_SUCCESS){
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			  "Query fail! (opcode %d)\n", op_info.op_code);
		goto _exit;
	}

	if(op_info.outbuf == NULL){
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			  "%s: op_info.outbuf is NULL (opcode %d)\n",
			  __FUNCTION__, op_info.op_code);
		goto _exit;
	}
	if(module_id == PHL_FG_MDL_SCAN){
		scan_param = (struct rtw_phl_scan_param*)op_info.outbuf;
		scan_issue_null_data = scan_param->ops->scan_issue_null_data;
	}
	else{
		scan_issue_null_data = (u8 (*)(void *, u8, bool))op_info.outbuf;
	}

	phl_mr_offch_hdl(phl_info,
			  role,
			  true,
			  phl_com->drv_priv,
			  scan_issue_null_data);
#endif
	ret = MDL_RET_SUCCESS;
_exit:
	return ret;
}

enum phl_mdl_ret_code
_phl_mrc_module_swch_done_hdlr(void *dispr,
			       void *priv,
			       struct phl_msg *msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct phl_module_op_info op_info = {0};
	struct rtw_wifi_role_t *role = NULL;
	u8 module_id = MSG_MDL_ID_FIELD(msg->msg_id);
	struct phl_scan_channel scan_ch = {0};
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
	struct rtw_phl_scan_param* scan_param = NULL;
#endif
	u8 idx = 0xff;

	phl_dispr_get_idx(dispr, &idx);
	/*
	* Handle mr offchan after switching channel to op channel
	*/
	if(module_id != PHL_FG_MDL_SCAN){
		ret = MDL_RET_IGNORE;
		goto _exit;
	}
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
		"%s: MSG_EVT_SWCH_DONE\n", __FUNCTION__);
	scan_ch = *(struct phl_scan_channel *)(msg->inbuf);

	/* Issue null 0 and resume beacon when BACKOP during scanning */
	if(scan_ch.scan_mode != BACKOP_MODE){
		ret = MDL_RET_SUCCESS;
		goto _exit;
	}

	role = (struct rtw_wifi_role_t *)msg->rsvd[0];
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
	op_info.op_code = FG_REQ_OP_GET_SCAN_PARAM;

	if(phl_disp_eng_query_cur_cmd_info(phl_info, idx, &op_info)
		!= RTW_PHL_STATUS_SUCCESS){
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			  "Query scan_param fail!\n");
		goto _exit;
	}
	scan_param = (struct rtw_phl_scan_param*)op_info.outbuf;

	if(op_info.outbuf == NULL){
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			  "%s: scan_param is NULL\n", __FUNCTION__);
		goto _exit;
	}

	phl_mr_offch_hdl(phl_info,
			 role,
			 false,
			 phl_com->drv_priv,
			 scan_param->ops->scan_issue_null_data);
#endif
	ret = MDL_RET_SUCCESS;
_exit:
	return ret;
}

static enum rtw_phl_status
_mrc_module_chg_op_chdef_start_pre_hdlr(u8 *param)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	struct chg_opch_param *ch_param = (struct chg_opch_param *)param;
	struct rtw_wifi_role_t *wrole = ch_param->wrole;
	struct phl_info_t *phl = wrole->phl_com->phl_priv;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
#ifdef CONFIG_MCC_SUPPORT
	/* disable MR coex mechanism(if runing) */
	psts = phl_mr_coex_disable(phl, wrole, wrole->hw_band,
					MR_COEX_TRIG_BY_CHG_OP_CHDEF);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s: MR coex disable fail\n", __func__);
	}
#endif
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

static enum rtw_phl_status
_mrc_module_chg_op_chdef_end_pre_hdlr(u8 *param)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	struct chg_opch_param *ch_param = (struct chg_opch_param *)param;
	struct rtw_wifi_role_t *wrole = ch_param->wrole;
	struct phl_info_t *phl = wrole->phl_com->phl_priv;
	struct rtw_phl_evt_ops *ops = ops = &phl->phl_com->evt_ops;
	u8 (*core_issue_null)(void *, u8, bool) = NULL;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
	/* Handle mr offchan after switching channel to new op channel */
	/* If the new pri-ch is as same as ori pri-ch, offch is unnecessary. */
	if(ch_param->new_chdef.chan == ch_param->ori_chdef.chan) {
		psts = RTW_PHL_STATUS_SUCCESS;
		goto exit;
	}
	if (ops->issue_null_data) {
		core_issue_null = ops->issue_null_data;
	} else {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Ops issue_null_data is NULL\n",
			__func__);
	}
	if (RTW_PHL_STATUS_SUCCESS != phl_mr_offch_hdl(phl, wrole, false,
				phl->phl_com->drv_priv, core_issue_null)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "%s: Fail to offch\n",
			__func__);
		goto exit;
	}
	psts = RTW_PHL_STATUS_SUCCESS;
exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: psts(%d)\n",
		  __func__, psts);
	return psts;
}

static enum phl_mdl_ret_code
_mrc_module_chg_op_chdef_start_hdlr(void *dispr, void *priv,
			 struct phl_msg *msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;
	struct phl_info_t *phl = (struct phl_info_t *)priv;
	struct rtw_phl_evt_ops *ops = &phl->phl_com->evt_ops;
	struct chg_opch_param *ch_param = NULL;
	struct rtw_wifi_role_t *wrole = NULL;
	u8 (*core_issue_null)(void *, u8, bool) = NULL;
	u8 *cmd = NULL;
	u32 cmd_len;

	if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_GENERAL) {
		ret = MDL_RET_IGNORE;
		goto exit;
	}
	if (phl_cmd_get_cur_cmdinfo(phl, msg->band_idx, msg, &cmd, &cmd_len)
						!= RTW_PHL_STATUS_SUCCESS) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Fail to get cmd info \n",
			  __func__);
		goto exit;
	}
	ch_param = (struct chg_opch_param *)cmd;
	wrole = ch_param->wrole;
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
	/* Handle mr offchan before switching channel */
	/* If the new pri-ch is as same as old pri-ch, offch is unnecessary. */
	if (ch_param->new_chdef.chan == ch_param->ori_chdef.chan) {
		ret = MDL_RET_SUCCESS;
		goto exit;
	}
	if (ops->issue_null_data) {
		core_issue_null = ops->issue_null_data;
	} else {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Ops issue_null_data is NULL\n",
			__func__);
	}
	if (RTW_PHL_STATUS_SUCCESS != phl_mr_offch_hdl(phl, wrole, true,
				phl->phl_com->drv_priv, core_issue_null)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "%s: Fail to offch\n",
			__func__);
		goto exit;
	}
	ret = MDL_RET_SUCCESS;
exit:
	return ret;
}

enum phl_mdl_ret_code
_mrc_module_chg_op_chdef_end_hdlr(void *dispr, void *priv,
				struct phl_msg *msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;
	struct phl_info_t *phl = (struct phl_info_t *)priv;
	struct chg_opch_param *ch_param = NULL;
	struct rtw_wifi_role_t *wrole = NULL;
	u8 *cmd = NULL;
	u32 cmd_len;

	if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_GENERAL) {
		ret = MDL_RET_IGNORE;
		goto exit;
	}
	if (phl_cmd_get_cur_cmdinfo(phl, msg->band_idx, msg, &cmd, &cmd_len)
						!= RTW_PHL_STATUS_SUCCESS) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Fail to get cmd info \n",
			  __func__);
		goto exit;
	}
	ch_param = (struct chg_opch_param *)cmd;
	wrole = ch_param->wrole;
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: wrole->id(%d)\n",
		  __func__, wrole->id);
	if (RTW_PHL_STATUS_SUCCESS != phl_role_notify(phl, wrole)) {
		PHL_ERR("%s role notify failed\n", __func__);
		goto exit;
	}
#ifdef CONFIG_MCC_SUPPORT
	/* Enable MR coex mechanism(if needed) */
	if (RTW_PHL_STATUS_SUCCESS != phl_mr_coex_handle(phl, wrole, 0,
				wrole->hw_band, MR_COEX_TRIG_BY_CHG_OP_CHDEF)) {
		PHL_ERR("%s: MR coex handle fail\n", __func__);
		goto exit;
	}
#endif
	ret = MDL_RET_SUCCESS;
exit:
	PHL_INFO("%s: ret(%d)\n", __func__, ret);
	return ret;
}

static enum phl_mdl_ret_code
_mrc_module_msg_pre_hdlr(void *dispr, void *priv, struct phl_msg *msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;
	struct phl_info_t *phl = (struct phl_info_t *)priv;
	u8 *cmd = NULL;
	u32 cmd_len;

	switch (MSG_EVT_ID_FIELD(msg->msg_id)) {
	case MSG_EVT_CHG_OP_CH_DEF_START:
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_GENERAL) {
			ret = MDL_RET_IGNORE;
			break;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: MSG_EVT_CHG_OP_CH_DEF_START\n",
			__FUNCTION__);
		if (RTW_PHL_STATUS_SUCCESS != phl_cmd_get_cur_cmdinfo(phl,
					msg->band_idx, msg, &cmd, &cmd_len)) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Fail to get cmd info \n",
				  __func__);
			break;
		}
		if (RTW_PHL_STATUS_SUCCESS !=
			_mrc_module_chg_op_chdef_start_pre_hdlr(cmd)) {
			break;
		}
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_CHG_OP_CH_DEF_END:
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_GENERAL) {
			ret = MDL_RET_IGNORE;
			break;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: MSG_EVT_CHG_OP_CH_DEF_END\n",
			__FUNCTION__);
		if (RTW_PHL_STATUS_SUCCESS != phl_cmd_get_cur_cmdinfo(phl,
					msg->band_idx, msg, &cmd, &cmd_len)) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Fail to get cmd info \n",
				  __func__);
			break;
		}
		if (RTW_PHL_STATUS_SUCCESS !=
			_mrc_module_chg_op_chdef_end_pre_hdlr(cmd)) {
			break;
		}
		ret = MDL_RET_SUCCESS;
		break;
	default:
		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "%s: MDL ID(%d), Event ID(%d), Not handle event in pre-phase\n",
			__FUNCTION__, MSG_MDL_ID_FIELD(msg->msg_id),
			MSG_EVT_ID_FIELD(msg->msg_id));
		ret = MDL_RET_IGNORE;
		break;
	}
	return ret;
}

static enum phl_mdl_ret_code
_mrc_module_msg_post_hdl(void *dispr, void *priv, struct phl_msg *msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;
	struct phl_info_t *phl_info = (struct phl_info_t *)priv;
	struct phl_module_op_info op_info = {0};
	struct rtw_wifi_role_t *role = NULL;
	struct rtw_chan_def chandef = {0};
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
	struct rtw_phl_scan_param* scan_param = NULL;
	u8 (*scan_issue_null_data)(void *, u8, bool) = NULL;
#endif
	u8 idx = 0xff;

	phl_dispr_get_idx(dispr, &idx);
	switch (MSG_EVT_ID_FIELD(msg->msg_id)) {
	case MSG_EVT_CHG_OP_CH_DEF_START:
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_CHG_OP_CH_DEF_START\n", __FUNCTION__);
		ret = _mrc_module_chg_op_chdef_start_hdlr(dispr, priv, msg);
		break;
	case MSG_EVT_CHG_OP_CH_DEF_END:
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_CHG_OP_CH_DEF_END\n", __FUNCTION__);
		ret = _mrc_module_chg_op_chdef_end_hdlr(dispr, priv, msg);
		break;
	case MSG_EVT_SWCH_START:
	ret = _phl_mrc_module_swch_start_hdlr(dispr, priv, msg);
		break;
	case MSG_EVT_SWCH_DONE:
	ret = _phl_mrc_module_swch_done_hdlr(dispr, priv, msg);
		break;
	case MSG_EVT_TSF_SYNC_DONE:
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_MRC) {
			return MDL_RET_IGNORE;
		}

		/*
		 * MR decides to call mcc enable or not
		 */
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_TSF_SYNC_DONE\n", __FUNCTION__);
		role = (struct rtw_wifi_role_t *)msg->inbuf;
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (_phl_mrc_module_tsf_sync_done_hdlr(phl_info, role) != RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "phl_mr_state_upt failed\n");
			break;
		}
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_TDLS_SYNC:
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_MRC) {
			return MDL_RET_IGNORE;
		}

		/*
		 * MR decides to call mcc enable or not
		 */
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_TDLS_SYNC\n", __FUNCTION__);

		role = (struct rtw_wifi_role_t *)msg->inbuf;

		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}

		if (phl_mr_info_upt(phl_info, role) != RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "phl_mr_info_upt failed\n");
			break;
		}
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_TX_RESUME:
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_MRC) {
			return MDL_RET_IGNORE;
		}

		/*
		 * MR resume the tx of the role in remain chanctx
		 */
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s:MSG_EVT_TX_RESUME\n", __FUNCTION__);
		role = (struct rtw_wifi_role_t *)msg->rsvd[0];
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
		scan_issue_null_data = (u8 (*)(void *, u8, bool))msg->rsvd[1];
		phl_mr_offch_hdl(phl_info,
				 role,
				 false,
				 phl_com->drv_priv,
				 scan_issue_null_data);
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_CONNECT_START:
#ifdef CONFIG_STA_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_CONNECT) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_CONNECT_START\n", __FUNCTION__);

		op_info.op_code = FG_REQ_OP_GET_ROLE;
		if (phl_disp_eng_query_cur_cmd_info(phl_info, idx, &op_info)
			!= RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "Query wifi role fail!\n");
			break;
		}
		role = (struct rtw_wifi_role_t *)op_info.outbuf;
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;

		}
		if (_phl_mrc_module_connect_start_hdlr(phl_info, role) !=
			RTW_PHL_STATUS_SUCCESS) {
			break;
		}
		rtw_hal_notification(phl_info->hal, MSG_EVT_ID_FIELD(msg->msg_id),
				     role->hw_band);
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_CONNECT_END:
#ifdef CONFIG_STA_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_CONNECT) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_CONNECT_END\n", __FUNCTION__);
		op_info.op_code = FG_REQ_OP_GET_ROLE;
		if (phl_disp_eng_query_cur_cmd_info(phl_info, idx, &op_info)
			!= RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "Query wifi role fail!\n");
			break;
		}
		role = (struct rtw_wifi_role_t *)op_info.outbuf;
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (_phl_mrc_module_connect_end_hdlr(phl_info, role) !=
			RTW_PHL_STATUS_SUCCESS) {
			break;
		}
		rtw_hal_notification(phl_info->hal, MSG_EVT_ID_FIELD(msg->msg_id),
				     role->hw_band);
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_DISCONNECT_PREPARE:
#ifdef CONFIG_STA_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_CONNECT &&
		    MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_DISCONNECT) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_DISCONNECT_PREPARE\n", __FUNCTION__);
		role = (struct rtw_wifi_role_t *)msg->rsvd[0];
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (_phl_mrc_module_disconnect_pre_hdlr(phl_info, role) !=
			RTW_PHL_STATUS_SUCCESS) {
			break;
		}
#ifdef CONFIG_TWT
		rtw_phl_twt_disable_all_twt_by_role(phl_info, role);
#endif
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_DISCONNECT:
#ifdef CONFIG_STA_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_CONNECT &&
		   MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_DISCONNECT) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_DISCONNECT\n", __FUNCTION__);

		role = (struct rtw_wifi_role_t *)msg->rsvd[0];
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (_phl_mrc_module_disconnect_hdlr(phl_info, role) !=
			RTW_PHL_STATUS_SUCCESS) {
			break;
		}
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_AP_START_PREPARE:
#ifdef CONFIG_AP_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_AP_START) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_AP_START_PREPARE\n", __FUNCTION__);
		op_info.op_code = FG_REQ_OP_GET_ROLE;
		if (phl_disp_eng_query_cur_cmd_info(phl_info, idx, &op_info)
			!= RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "Query wifi role fail!\n");
			break;
		}
		role = (struct rtw_wifi_role_t *)op_info.outbuf;
		if (role == NULL){
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (_phl_mrc_module_ap_start_pre_hdlr(phl_info, role) !=
			RTW_PHL_STATUS_SUCCESS) {
			break;
		}
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_AP_START:
#ifdef CONFIG_AP_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_AP_START) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_AP_START\n", __FUNCTION__);
		op_info.op_code = FG_REQ_OP_GET_ROLE;
		if (phl_disp_eng_query_cur_cmd_info(phl_info, idx, &op_info)
			!= RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "Query wifi role fail!\n");
			break;
		}
		role = (struct rtw_wifi_role_t *)op_info.outbuf;
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (_phl_mrc_module_ap_started_hdlr(phl_info, role) !=
			RTW_PHL_STATUS_SUCCESS) {
			break;
		}
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_AP_START_END:
#ifdef CONFIG_AP_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_AP_START) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_AP_START_END\n", __FUNCTION__);

		role = (struct rtw_wifi_role_t *)msg->rsvd[0];
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (msg->inbuf == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s:AP start status info not found!\n",
				  __FUNCTION__);
			ret = MDL_RET_FAIL;
			break;
		}
		if (*(msg->inbuf) != RTW_PHL_STATUS_SUCCESS) {
			if (_phl_mrc_module_ap_stop_hdlr(phl_info, role)
			   != RTW_PHL_STATUS_SUCCESS) {
				break;
			}
		}
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_AP_STOP_PREPARE:
#ifdef CONFIG_AP_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_AP_STOP) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_AP_STOP_PREPARE\n", __FUNCTION__);

		role = (struct rtw_wifi_role_t *)msg->rsvd[0];
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (_phl_mrc_module_ap_stop_pre_hdlr(phl_info, role) !=
			RTW_PHL_STATUS_SUCCESS) {
			break;
		}
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_AP_STOP:
#ifdef CONFIG_AP_CMD_DISPR
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_AP_STOP) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_AP_STOP\n", __FUNCTION__);

		role = (struct rtw_wifi_role_t *)msg->rsvd[0];
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (_phl_mrc_module_ap_stop_hdlr(phl_info, role) !=
			RTW_PHL_STATUS_SUCCESS) {
			break;
		}
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_SCAN_START:
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_SCAN) {
			return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_SCAN_START\n", __FUNCTION__);
		role = (struct rtw_wifi_role_t *)msg->rsvd[0];
#ifdef CONFIG_MCC_SUPPORT
		if (phl_mr_coex_disable(phl_info, role, role->hw_band,
			MR_COEX_TRIG_BY_SCAN) != RTW_PHL_STATUS_SUCCESS)
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "disable TDMRA fail!\n");
#endif /* CONFIG_MCC_SUPPORT */
		phl_p2pps_noa_pause_all(phl_info, role);
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
		op_info.op_code = FG_REQ_OP_GET_SCAN_PARAM;
		if (phl_disp_eng_query_cur_cmd_info(phl_info, idx, &op_info)
			!= RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "Query scan_param fail!\n");
			break;
		}
		scan_param = (struct rtw_phl_scan_param*)op_info.outbuf;
		if (op_info.outbuf == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: scan_param is NULL\n", __FUNCTION__);
			break;
		}
		phl_mr_offch_hdl(phl_info,
				  role,
				  true,
				  phl_com->drv_priv,
				  scan_param->ops->scan_issue_null_data);
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_SCAN_END:
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_SCAN) {
				return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_SCAN_END\n", __FUNCTION__);
		role = (struct rtw_wifi_role_t *)msg->rsvd[0];
		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		if (phl_mr_get_chandef(phl_info,
				       role,
				       false,
				       &chandef) !=
				RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("%s phl_mr_get_chandef failed\n", __func__);
			break;
		}
		PHL_DUMP_CHAN_DEF_EX(&chandef);
		phl_set_ch_bw(role, &chandef, false);
#ifdef RTW_WKARD_MRC_ISSUE_NULL_WITH_SCAN_OPS
		/*
		 * Use msg to send ops pointer to prevent query fail in
		 * abort case
		 */
		scan_issue_null_data = (u8 (*)(void *, u8, bool))msg->rsvd[1];
		phl_mr_offch_hdl(phl_info,
				  role,
				  false,
				  phl_com->drv_priv,
				  scan_issue_null_data);
#endif
		phl_p2pps_noa_resume_all(phl_info, role);
#ifdef CONFIG_MCC_SUPPORT
		/* Enable MR coex mechanism(if needed) */
		if (phl_mr_coex_handle(phl_info, role, 0, role->hw_band,
						MR_COEX_TRIG_BY_SCAN)
					!= RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "enable MCC fail!\n");
		}
#endif /* CONFIG_MCC_SUPPORT */
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_ECSA_SWITCH_START:
#ifdef CONFIG_PHL_ECSA
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_ECSA) {
				return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_ECSA_SWITCH_START\n", __FUNCTION__);
		role = (struct rtw_wifi_role_t *)msg->rsvd[0];

		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
#ifdef CONFIG_MCC_SUPPORT
		if (phl_mr_coex_disable(phl_info, role, role->hw_band,
			MR_COEX_TRIG_BY_ECSA) != RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_ECSA, _PHL_WARNING_,
				  "Disable MCC failed\n");
			break;
		}
#endif /* CONFIG_MCC_SUPPORT */
#endif /* CONFIG_PHL_ECSA */
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_ECSA_SWITCH_DONE:
#ifdef CONFIG_PHL_ECSA
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_FG_MDL_ECSA) {
				return MDL_RET_IGNORE;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
			  "%s: MSG_EVT_ECSA_SWITCH_DONE\n", __FUNCTION__);
		role = (struct rtw_wifi_role_t *)msg->rsvd[0];

		if (role == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				  "%s: role is NULL\n", __FUNCTION__);
			break;
		}
		phl_mr_stop_all_beacon(phl_info, role, false);
#ifdef CONFIG_MCC_SUPPORT
		/* STA ECSA might switch to a channel still in MCC state */
		/* Enable MR coex mechanism(if needed) */
		if (phl_mr_coex_handle(phl_info, role, 0,
				role->hw_band, MR_COEX_TRIG_BY_ECSA)
						!= RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_ECSA, _PHL_WARNING_,
				  "Enable TDMRA failed\n");
			break;
		}
#endif /* CONFIG_MCC_SUPPORT */

		if (RTW_PHL_STATUS_SUCCESS != phl_role_notify(phl_info, role)) {
			PHL_ERR("%s role notify failed\n", __func__);
			ret = MDL_RET_FAIL;
		} else
#endif /* CONFIG_PHL_ECSA */
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_BTC_REQ_BT_SLOT:
#ifdef CONFIG_MCC_SUPPORT
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_BTC)
			return MDL_RET_IGNORE;

		PHL_INFO("%s: MSG_EVT_BTC_REQ_BT_SLOT, BT slot = %d\n", __func__, (u16)(*(u32 *)msg->inbuf));
		phl_mr_coex_handle(phl_info, NULL, (u16)(*(u32 *)msg->inbuf),
			msg->band_idx, MR_COEX_TRIG_BY_BT);
#endif
		ret = MDL_RET_SUCCESS;
		break;
	case MSG_EVT_SER_M5_READY:
		if (MSG_MDL_ID_FIELD(msg->msg_id) != PHL_MDL_SER)
			return MDL_RET_IGNORE;

		PHL_INFO("%s: MSG_EVT_SER_M5_READY\n", __func__);
		phl_mr_err_recovery(phl_info, MSG_EVT_ID_FIELD(msg->msg_id));
		ret = MDL_RET_SUCCESS;
		break;
	default:
		ret = MDL_RET_SUCCESS;
		break;
	}
	FUNCOUT();
	return ret;
}

static enum phl_mdl_ret_code
_phl_mrc_module_msg_hdlr(void *dispr, void *priv, struct phl_msg *msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	if (IS_MSG_FAIL(msg->msg_id)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			  "%s: cmd dispatcher notify cmd failure: 0x%x.\n",
			   __FUNCTION__, msg->msg_id);
	FUNCOUT();
		return MDL_RET_FAIL;
	}
	if (IS_MSG_IN_PRE_PHASE(msg->msg_id)) {
		ret = _mrc_module_msg_pre_hdlr(dispr, priv, msg);
	} else {
		ret = _mrc_module_msg_post_hdl(dispr, priv, msg);
	}
	FUNCOUT();
	return ret;
}

static enum phl_mdl_ret_code
_phl_mrc_module_set_info(void *dispr,
			 void *priv,
			 struct phl_module_op_info *info)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	FUNCOUT();

	ret = MDL_RET_SUCCESS;
	return ret;
}

static enum phl_mdl_ret_code
_phl_mrc_module_query_info(void *dispr,
			   void *priv,
			   struct phl_module_op_info *info)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	FUNCOUT();

	ret = MDL_RET_SUCCESS;
	return ret;
}

static enum rtw_phl_status
_phl_role_bk_module_init(struct phl_info_t *phl_info)
{
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_info->phl_com);
	struct phl_bk_module_ops *bk_ops = &mr_ctl->bk_ops;

	bk_ops->init = _phl_mrc_module_init;
	bk_ops->deinit = _phl_mrc_module_deinit;
	bk_ops->start = _phl_mrc_module_start;
	bk_ops->stop = _phl_mrc_module_stop;
	bk_ops->msg_hdlr = _phl_mrc_module_msg_hdlr;
	bk_ops->set_info = _phl_mrc_module_set_info;
	bk_ops->query_info = _phl_mrc_module_query_info;

	return RTW_PHL_STATUS_SUCCESS;
}
#endif /*CONFIG_CMD_DISP*/
/*
 * init wifi_role control components
 * init band_ctrl
 * init bk module
 * init wifi_role[]
 */
enum rtw_phl_status
phl_mr_ctrl_init(struct phl_info_t *phl_info)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	void *drv = phl_to_drvpriv(phl_info);
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	u8 ridx = MAX_WIFI_ROLE_NUMBER;
	struct rtw_wifi_role_t *role = NULL;
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;

	mr_ctl->hal_com = rtw_hal_get_halcom(phl_info->hal);
	if (mr_ctl->hal_com == NULL) {
		PHL_ERR("%s mr_ctl->hal_com is NULL\n", __func__);
		_os_warn_on(1);
		return RTW_PHL_STATUS_FAILURE;
	}
	_os_spinlock_init(drv, &(mr_ctl->lock));
	mr_ctl->is_sb = true;
	_phl_band_ctrl_init(phl_info);
	#ifdef CONFIG_CMD_DISP
	_phl_role_bk_module_init(phl_info);
	#endif
	_os_mem_set(phl_to_drvpriv(phl_info), phl_com->wifi_roles,
		0, sizeof(*phl_com->wifi_roles));

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		role = &(phl_com->wifi_roles[ridx]);
		pq_init(drv, &role->assoc_sta_queue);
		role->phl_com = phl_com;
		role->id = ridx;
		role->active = false;
		role->chanctx = NULL;
	}
	if (RTW_PHL_STATUS_SUCCESS != (status = rtw_phl_mcc_init(phl_info))) {
		PHL_ERR("%s mcc init fail\n", __func__);
		/* todo, need to discuss with Georgia*/
	}
	return RTW_PHL_STATUS_SUCCESS;
}

static enum rtw_phl_status
_phl_band_ctrl_deinit(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_info->phl_com);
	struct hw_band_ctl_t *band_ctrl;
	u8 band_idx = 0;

	rtw_phl_mcc_deinit(phl_info);
	for (band_idx = 0; band_idx < MAX_BAND_NUM; band_idx++) {
		band_ctrl = &(mr_ctl->band_ctrl[band_idx]);
		phl_chanctx_free(phl_info, band_ctrl);
		_os_spinlock_free(drv, &(band_ctrl->lock));
		pq_deinit(drv , &band_ctrl->chan_ctx_queue);
	}

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_mr_ctrl_deinit(struct phl_info_t *phl_info)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	u8 ridx = MAX_WIFI_ROLE_NUMBER;
	struct rtw_wifi_role_t *role;
	void *drv = phl_to_drvpriv(phl_info);
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);

	_os_spinlock_free(drv, &(mr_ctl->lock));
	_phl_band_ctrl_deinit(phl_info);

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		role = &(phl_com->wifi_roles[ridx]);
		pq_deinit(drv, &role->assoc_sta_queue);
	}

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_mr_chandef_sync(struct phl_info_t *phl_info, struct hw_band_ctl_t *band_ctrl,
			struct rtw_chan_ctx *chanctx, struct rtw_chan_def *chandef)
{
	void *drv = phl_to_drvpriv(phl_info);
	u8 ridx;
	u8 role_num = 0;
	enum band_type band_ret = BAND_MAX;
	u8 ch_ret = 0;
	enum channel_width bw_ret = CHANNEL_WIDTH_20;
	enum chan_offset offset_ret = CHAN_OFFSET_NO_EXT;
	struct rtw_wifi_role_t *wrole;
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_FAILURE;

	if (!chanctx) {
		PHL_ERR("%s failed, chanctx == NULL\n", __func__);
		goto _exit;
	}
	if (!chandef) {
		PHL_ERR("%s failed, chandef == NULL\n", __func__);
		goto _exit;
	}

	_os_spinlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (chanctx->role_map & BIT(ridx)) {
			wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, ridx);
			if (wrole == NULL) {
				PHL_ERR("ridx :%d wrole == NULL\n", ridx);
				_os_warn_on(1);
				continue;
			}
			if (role_num == 0) {
				band_ret = wrole->chandef.band;
				ch_ret = wrole->chandef.chan;
				bw_ret = wrole->chandef.bw;
				offset_ret = wrole->chandef.offset;
				role_num++;
				continue;
			}
			if (band_ret != wrole->chandef.band) {
				PHL_ERR("band_ret(%d) !=  ridx(%d)-band_ret(%d)\n",
					band_ret, ridx, wrole->chandef.band);
				_os_warn_on(1);
				role_num = 0;
				break;
			}
			if (ch_ret !=  wrole->chandef.chan) {
				PHL_ERR("ch_ret(%d) !=  ridx(%d)-chan(%d)\n",
					ch_ret, ridx, wrole->chandef.chan);
				_os_warn_on(1);
				role_num = 0;
				break;
			}

			if (bw_ret < wrole->chandef.bw) {
				bw_ret = wrole->chandef.bw;
				offset_ret = wrole->chandef.offset;
			} else if (bw_ret == wrole->chandef.bw && offset_ret != wrole->chandef.offset) {
				role_num = 0;
				break;
			}
			role_num++;
		}
	}
	_os_spinunlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);

	if (role_num == 0) {
		PHL_ERR("%s role_num=0\n", __func__);
		_os_warn_on(!role_num);
		goto _exit;
	}

	PHL_INFO("%s org_chctx - band:%d, chan:%d, bw:%d, offset:%d\n",
		__func__, chandef->band, chandef->chan, chandef->bw, chandef->offset);
	PHL_INFO("%s mi_upt - band:%d, chan:%d, bw:%d, offset:%d\n",
		__func__, band_ret, ch_ret, bw_ret, offset_ret);

	chandef->band = band_ret;
	chandef->chan = ch_ret;
	chandef->bw = bw_ret;
	chandef->offset = offset_ret;
	phl_sts = RTW_PHL_STATUS_SUCCESS;

_exit:
	return phl_sts;
}

/*
 * MR change chctx from wrole->chdef to new chdef
 * @wrole: specific role, and we can get original chdef.
 * @new_chan: new chdef
 * @chctx_result: The final ch ctx after change new chdef to MR.
 * ex: In the scc case, it will be the group chdef.
 */
enum rtw_phl_status
phl_mr_chandef_chg(struct phl_info_t *phl,
		struct rtw_wifi_role_t *wrole, struct rtw_chan_def *new_chan,
		struct rtw_chan_def *chctx_result)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	struct rtw_chan_def chan_def = {0};
	void *drv = phl_to_drvpriv(phl);
	u8 chanctx_num = 0;

	chanctx_num = (u8)rtw_phl_chanctx_del(phl, wrole, &chan_def);
	_os_mem_cpy(drv, &chan_def, new_chan, sizeof(struct rtw_chan_def));
	if (rtw_phl_chanctx_add((void *)phl, wrole, &chan_def.chan,
		&chan_def.bw, &chan_def.offset)) {
		_os_mem_cpy(drv, chctx_result, &chan_def,
				sizeof(struct rtw_chan_def));
		psts = RTW_PHL_STATUS_SUCCESS;
		goto exit;
	}
	PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Add new chandef fail, something wrong!\n",
		__FUNCTION__);
	/* Error handle: Recover the chctx */
	_os_mem_cpy(drv, &chan_def, &wrole->chandef,
			sizeof(struct rtw_chan_def));
	if (!rtw_phl_chanctx_add((void *)phl, wrole, &chan_def.chan,
		&chan_def.bw, &chan_def.offset)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s: Error handle failed for recovery!\n",
			__FUNCTION__);
		goto exit;
	}
exit:
	return psts;
}

enum rtw_phl_status
phl_mr_chandef_upt(struct phl_info_t *phl_info,
		struct hw_band_ctl_t *band_ctrl, struct rtw_chan_ctx *chanctx)
{
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_FAILURE;

	if (!chanctx) {
		PHL_ERR("%s chanctx == NULL\n", __func__);
		goto _exit;
	}

	phl_sts = phl_mr_chandef_sync(phl_info, band_ctrl, chanctx, &chanctx->chan_def);
	if (phl_sts != RTW_PHL_STATUS_SUCCESS)
		PHL_ERR("%s phl_mr_sync_chandef failed\n", __func__);
_exit:
	return phl_sts;
}

enum rtw_phl_status
rtw_phl_mr_upt_chandef(void *phl, struct rtw_wifi_role_t *wifi_role)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wifi_role->hw_band]);
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_FAILURE;

	if (!wifi_role->chanctx) {
		PHL_ERR("%s failed - wifi_role->chanctx == NULL\n", __func__);
		goto _exit;
	}
	phl_sts = phl_mr_chandef_upt(phl_info, band_ctrl, wifi_role->chanctx);
	if (phl_sts != RTW_PHL_STATUS_SUCCESS)
		PHL_ERR("%s phl_mr_chandef_upt failed\n", __func__);

_exit:
	return phl_sts;
}

enum rtw_phl_status
phl_mr_get_chandef(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wifi_role,
			bool sync, struct rtw_chan_def *chandef)
{
	enum rtw_phl_status phl_sts = RTW_PHL_STATUS_SUCCESS;
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wifi_role->hw_band]);
	struct phl_queue *chan_ctx_queue = &band_ctrl->chan_ctx_queue;
	struct rtw_chan_ctx *chanctx = NULL;
	int chctx_num = 0;
	u8 chctx_role_num = 0;

	if (!chandef) {
		PHL_ERR("%s failed - chandef == NULL\n", __func__);
		phl_sts = RTW_PHL_STATUS_FAILURE;
		goto _exit;
	}

	/*init chandef*/
	chandef->chan = 0;
	if (wifi_role->chanctx) {
		chctx_role_num = phl_chanctx_get_rnum_with_lock(phl_info, chan_ctx_queue, wifi_role->chanctx);

		if (chctx_role_num == 0) {
			PHL_ERR("%s-%d chctx_role_num == 0\n", __FUNCTION__, __LINE__);
			_os_warn_on(1);
		}
		if (sync && chctx_role_num >= 2) {
			phl_sts = phl_mr_chandef_sync(phl_info, band_ctrl,
						wifi_role->chanctx, chandef);
			if (phl_sts != RTW_PHL_STATUS_SUCCESS) {
				PHL_ERR("%s phl_mr_chandef_sync failed\n", __func__);
				_os_warn_on(1);
			}
		} else { /*chctx_role_num == 1*/
			_os_mem_cpy(drv, chandef, &wifi_role->chanctx->chan_def,
							sizeof(struct rtw_chan_def));
		}
	}
	else {
		chctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);
		if (chctx_num == 0) {
			_os_mem_cpy(drv, chandef, &wifi_role->chandef,
						sizeof(struct rtw_chan_def));

		} else if (chctx_num == 1) {
			_os_spinlock(drv, &chan_ctx_queue->lock, _ps, NULL);
			if (list_empty(&chan_ctx_queue->queue)) {
				PHL_ERR("%s chan_ctx_queue->queue is empty\n", __func__);
				_os_warn_on(1);
			}

			chanctx = list_first_entry(&chan_ctx_queue->queue,
						struct rtw_chan_ctx, list);
			chctx_role_num = phl_chanctx_get_rnum(phl_info, chan_ctx_queue, chanctx);
			if (chctx_role_num == 0) {
				PHL_ERR("%s-%d chctx_role_num == 0\n", __FUNCTION__, __LINE__);
				_os_warn_on(1);
			}
			if (sync && chctx_role_num >= 2) {
				phl_sts = phl_mr_chandef_sync(phl_info, band_ctrl,
								chanctx, chandef);
				if (phl_sts != RTW_PHL_STATUS_SUCCESS)
					PHL_ERR("%s phl_mr_chandef_sync failed\n", __func__);
			} else { /*chctx_role_num == 1*/
				_os_mem_cpy(drv, chandef, &chanctx->chan_def,
						sizeof(struct rtw_chan_def));
			}
			_os_spinunlock(drv, &chan_ctx_queue->lock, _ps, NULL);

		} else if (chctx_num == 2) { /*MCC*/

		} else {
			PHL_ERR("%s chctx_num(%d) is invalid\n", __func__, chctx_num);
			_os_warn_on(1);
			goto _exit;
		}
	}

_exit:
	return phl_sts;
}
enum rtw_phl_status
rtw_phl_mr_get_chandef(void *phl, struct rtw_wifi_role_t *wifi_role,
					struct rtw_chan_def *chandef)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	return phl_mr_get_chandef(phl_info, wifi_role, false, chandef);

}
int rtw_phl_mr_get_chanctx_num(void *phl, struct rtw_wifi_role_t *wifi_role)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_info->phl_com);
	u8 band_idx = wifi_role->hw_band;
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[band_idx]);

	return phl_mr_get_chanctx_num(phl_info, band_ctrl);
}

enum rtw_phl_status
rtw_phl_mr_rx_filter(void *phl, struct rtw_wifi_role_t *wrole)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	enum rtw_rx_fltr_mode mode = RX_FLTR_MODE_STA_NORMAL;
#ifdef CONFIG_MR_SUPPORT
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_info->phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);

	if (band_ctrl->cur_info.lg_sta_num >= 1)
		mode = RX_FLTR_MODE_STA_LINKING;
	else if (band_ctrl->cur_info.ap_num >= 1)
		mode = RX_FLTR_MODE_AP_NORMAL;
	else if (band_ctrl->cur_info.ld_sta_num >= 1)
		mode = RX_FLTR_MODE_STA_NORMAL;	/* Accpet BSSID matched frames */
	else
		mode = RX_FLTR_MODE_STA_NORMAL; /* For STA no link case */

#else
	if (wrole->type == PHL_RTYPE_STATION && wrole->mstate == MLME_LINKED)
		mode = RX_FLTR_MODE_STA_NORMAL;
	else if (wrole->type == PHL_RTYPE_STATION && wrole->mstate == MLME_LINKING)
		mode = RX_FLTR_MODE_STA_LINKING;
	else if (wrole->type == PHL_RTYPE_AP)
		mode = RX_FLTR_MODE_AP_NORMAL;
	else
		mode = RX_FLTR_MODE_STA_NORMAL;/* For STA no link case */
#endif /*CONFIG_MR_SUPPORT*/

	rtw_hal_set_rxfltr_by_mode(phl_info->hal, wrole->hw_band, mode);
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_mr_tsf_sync(void *phl, struct rtw_wifi_role_t *wrole,
					enum role_state state)
{
	enum rtw_phl_status ret = RTW_PHL_STATUS_SUCCESS;
#ifdef CONFIG_MR_SUPPORT
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_info->phl_com);
	struct hw_band_ctl_t *band_ctl = &mr_ctl->band_ctrl[wrole->hw_band];
	struct rtw_chan_ctx *chanctx = wrole->chanctx;
	struct rtw_phl_com_t *phl_com = wrole->phl_com;
	struct rtw_wifi_role_t *wr_sync_from = NULL;
	struct rtw_wifi_role_t *wr_sync_to = NULL;
	enum phl_band_idx band = wrole->hw_band;
	s8 tsf_sync_offset_tu = 50;/* unit is TU(1024us) */
	u8 ridx = 0;
	u8 ap_num = band_ctl->cur_info.ap_num;
	u8 ld_sta_num = band_ctl->cur_info.ld_sta_num;
	int chanctx_num = 0;

	chanctx_num = phl_mr_get_chanctx_num(phl_info, band_ctl);
	PHL_INFO("%s:state(%d), ap_num=%d, ld_sta_num=%d, tsf_sync_port=%d,chanctx_num=%d\n",
			__func__, state, ap_num, ld_sta_num, band_ctl->tsf_sync_port, chanctx_num);

	#ifdef CONFIG_MCC_SUPPORT
	if (chanctx_num > 1) {
		if (phl_com->dev_cap.mcc_sup == true) {
			PHL_INFO("%s: will process MCC, skip tsf sync\n", __func__);
			ret = RTW_PHL_STATUS_SUCCESS;
			goto exit;
		} else {
			PHL_ERR("%s: chanctx_num(%d) > 1, check chanctx\n", __func__, chanctx_num);
			ret = RTW_PHL_STATUS_FAILURE;
			goto exit;
		}
	}
	#endif

	switch (state) {
	case PHL_ROLE_MSTS_STA_CONN_END:
		if (chanctx == NULL) {
			PHL_WARN("%s: state%d wifi role (id=%d)chanctx=NULL\n", __func__, state, wrole->id);
			ret = RTW_PHL_STATUS_FAILURE;
			goto exit;
		}
		/* SoftAP already started, and no tsf sync before */
		if (ap_num >= 1 && band_ctl->tsf_sync_port == HW_PORT_MAX) {
			/* tsf sync for all softap */
			wr_sync_from = wrole;
			for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
				if (chanctx->role_map & BIT(ridx)) {
					wr_sync_to = &phl_com->wifi_roles[ridx];
					if (wr_sync_to != wr_sync_from) {
						switch (wr_sync_to->type) {
						case PHL_RTYPE_AP:
						case PHL_RTYPE_VAP:
						case PHL_RTYPE_P2P_GO:
						case PHL_RTYPE_MESH:
							if (rtw_hal_tsf_sync(phl_info->hal, wr_sync_from->hw_port,
								wr_sync_to->hw_port, band, tsf_sync_offset_tu, HAL_TSF_EN_SYNC_AUTO) == RTW_HAL_STATUS_SUCCESS) {
								ret = RTW_PHL_STATUS_SUCCESS;
								PHL_INFO("%s, enable wrole:%d(port:%d) sync from wrole:%d(port:%d) success\n",
									__func__, wr_sync_to->id, wr_sync_to->hw_port,
									wr_sync_from->id, wr_sync_from->hw_port);
							} else {
								ret = RTW_PHL_STATUS_FAILURE;
								PHL_ERR("%s, enable wrole:%d(port:%d) sync from wrole:%d(port:%d) fail\n",
									__func__, wr_sync_to->id, wr_sync_to->hw_port,
									wr_sync_from->id, wr_sync_from->hw_port);
								break;
							}
							break;
						default :
							break;
						}
					}
				}
			}

			if (ret == RTW_PHL_STATUS_SUCCESS)
				band_ctl->tsf_sync_port = wr_sync_from->hw_port;
			else
				band_ctl->tsf_sync_port = HW_PORT_MAX;
		}
		break;
	case PHL_ROLE_MSTS_STA_DIS_CONN:
		if (chanctx == NULL) {
			PHL_WARN("%s: state%d wifi role (id=%d)chanctx=NULL\n", __func__, state, wrole->id);
			ret = RTW_PHL_STATUS_FAILURE;
			goto exit;
		}

		/* if TSF sync do not enable, skip disable flow */
		if (band_ctl->tsf_sync_port == HW_PORT_MAX) {
			ret = RTW_PHL_STATUS_SUCCESS;
			goto exit;
		}

		/* if disconnected STA is sync port and SoftAP exists,
		find new sync port */
		if (wrole->hw_port == band_ctl->tsf_sync_port &&
			ap_num >= 1 && ld_sta_num > 0) {
			/* find linked sta */
			wr_sync_from = _search_ld_sta_wrole(wrole, true);
			if (wr_sync_from) {
				/* re-sync tsf for all softap */
				for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
					if (chanctx->role_map & BIT(ridx)) {
						wr_sync_to = &phl_com->wifi_roles[ridx];
						if (wr_sync_to != wr_sync_from) {
							switch (wr_sync_to->type) {
							case PHL_RTYPE_AP:
							case PHL_RTYPE_VAP:
							case PHL_RTYPE_P2P_GO:
							case PHL_RTYPE_MESH:
								if (rtw_hal_tsf_sync(phl_info->hal, wr_sync_from->hw_port,
									wr_sync_to->hw_port, band, tsf_sync_offset_tu, HAL_TSF_EN_SYNC_AUTO) == RTW_HAL_STATUS_SUCCESS) {
									ret = RTW_PHL_STATUS_SUCCESS;
									PHL_INFO("%s, enable wrole:%d(port:%d) sync from wrole:%d(port:%d) success\n",
										__func__, wr_sync_to->id, wr_sync_to->hw_port,
										wr_sync_from->id, wr_sync_from->hw_port);
								} else {
									ret = RTW_PHL_STATUS_FAILURE;
									PHL_ERR("%s, enable wrole:%d(port:%d) sync from wrole:%d(port:%d) fail\n",
										__func__, wr_sync_to->id, wr_sync_to->hw_port,
										wr_sync_from->id, wr_sync_from->hw_port);
									break;
								}
								break;
							default :
								break;
							}
						}
					}
				}

				if (ret == RTW_PHL_STATUS_SUCCESS)
					band_ctl->tsf_sync_port = wr_sync_from->hw_port;
				else
					band_ctl->tsf_sync_port = HW_PORT_MAX;
			}
		}

		/* if disconnected STA is sync port and no other linked sta exist,
		disable sofap tsf sync */
		if (wrole->hw_port == band_ctl->tsf_sync_port && ld_sta_num == 0) {
			for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
				if (chanctx->role_map & BIT(ridx)){
					wr_sync_to = &phl_com->wifi_roles[ridx];
					if (wr_sync_to != wr_sync_from) {
						switch (wr_sync_to->type) {
						case PHL_RTYPE_AP:
						case PHL_RTYPE_VAP:
						case PHL_RTYPE_P2P_GO:
						case PHL_RTYPE_MESH:
							if (wr_sync_to->mstate == MLME_LINKED) {
								if (rtw_hal_tsf_sync(phl_info->hal, band_ctl->tsf_sync_port,
									wr_sync_to->hw_port, band, tsf_sync_offset_tu, HAL_TSF_DIS_SYNC_AUTO) == RTW_HAL_STATUS_SUCCESS) {
									ret = RTW_PHL_STATUS_SUCCESS;
									PHL_INFO("%s, disable wrole:%d(port:%d) sync from wrole (port:%d) success\n",
										__func__, wr_sync_to->id, wr_sync_to->hw_port,
										band_ctl->tsf_sync_port);
								} else {
									ret = RTW_PHL_STATUS_FAILURE;
									PHL_ERR("%s, disable wrole:%d(port:%d) sync from wrole:(port:%d) fail\n",
										__func__, wr_sync_to->id, wr_sync_to->hw_port,
										band_ctl->tsf_sync_port);
									break;
								}
							}
							break;
						default :
							break;
						}
					}
				}
			}

			if (ret == RTW_PHL_STATUS_SUCCESS)
				band_ctl->tsf_sync_port = HW_PORT_MAX;
		}
		break;
	case PHL_ROLE_MSTS_AP_START:
		if (chanctx == NULL) {
			PHL_WARN("%s: state%d wifi role (id=%d)chanctx=NULL\n", __func__, state, wrole->id);
			ret = RTW_PHL_STATUS_FAILURE;
			goto exit;
		}
		/* no linked sta, don't enable tsf sync */
		if (ld_sta_num == 0)
			break;
		/* New softAP start, and no tsf sync before, find sync port  */
		if (band_ctl->tsf_sync_port == HW_PORT_MAX) {
			wr_sync_to = wrole;
			wr_sync_from = _search_ld_sta_wrole(wrole, true);
			if (wr_sync_from) {
				if (rtw_hal_tsf_sync(phl_info->hal, wr_sync_from->hw_port,
					wr_sync_to->hw_port, band, tsf_sync_offset_tu, HAL_TSF_EN_SYNC_AUTO) == RTW_HAL_STATUS_SUCCESS) {
					band_ctl->tsf_sync_port = wr_sync_from->hw_port;
					ret = RTW_PHL_STATUS_SUCCESS;
					PHL_INFO("%s, enable wrole:%d(port:%d) sync from wrole:%d(port:%d) success\n",
						__func__, wr_sync_to->id, wr_sync_to->hw_port,
						wr_sync_from->id, wr_sync_from->hw_port);
				} else {
					ret = RTW_PHL_STATUS_FAILURE;
					PHL_ERR("%s, enable wrole:%d(port:%d) sync from wrole:%d(port:%d) fail\n",
						__func__, wr_sync_to->id, wr_sync_to->hw_port,
						wr_sync_from->id, wr_sync_from->hw_port);
				}
			}
		} else if (band_ctl->tsf_sync_port != HW_PORT_MAX) {
			/* New softAP start, enable tsf sync before, follow original sync port  */
			wr_sync_to = wrole;
			if (rtw_hal_tsf_sync(phl_info->hal, band_ctl->tsf_sync_port,
				wr_sync_to->hw_port, band, tsf_sync_offset_tu, HAL_TSF_EN_SYNC_AUTO) == RTW_HAL_STATUS_SUCCESS) {
				ret = RTW_PHL_STATUS_SUCCESS;
				PHL_INFO("%s, enable wrole:%d(port:%d) sync from wrole(port:%d) success\n",
					__func__, wr_sync_to->id, wr_sync_to->hw_port,
					band_ctl->tsf_sync_port);
			} else {
				ret = RTW_PHL_STATUS_FAILURE;
				PHL_ERR("%s, enable wrole:%d(port:%d) sync from wrole(port:%d) fail\n",
					__func__, wr_sync_to->id, wr_sync_to->hw_port,
					band_ctl->tsf_sync_port);
			}
		}
		break;
	case PHL_ROLE_MSTS_AP_STOP:
		/* if TSF sync do not enable, skip disable flow */
		if (band_ctl->tsf_sync_port == HW_PORT_MAX) {
			ret = RTW_PHL_STATUS_SUCCESS;
			goto exit;
		}
		wr_sync_to = wrole;
		if (rtw_hal_tsf_sync(phl_info->hal, band_ctl->tsf_sync_port,
					wr_sync_to->hw_port, band, tsf_sync_offset_tu, HAL_TSF_DIS_SYNC_AUTO) == RTW_HAL_STATUS_SUCCESS) {
			ret = RTW_PHL_STATUS_SUCCESS;
			PHL_INFO("%s, disable wrole:%d(port:%d) sync from wrole(port:%d) success\n",
				__func__, wr_sync_to->id, wr_sync_to->hw_port,
				band_ctl->tsf_sync_port);
		} else {
			ret = RTW_PHL_STATUS_FAILURE;
			PHL_ERR("%s, disable wrole:%d(port:%d) sync from wrole(port:%d) fail\n",
				__func__, wr_sync_to->id, wr_sync_to->hw_port,
				band_ctl->tsf_sync_port);
		}

		if (ap_num == 0)
			band_ctl->tsf_sync_port = HW_PORT_MAX;
		break;
	default:
		PHL_ERR("%s unsupport state(%d)\n", __func__, state);
		ret = RTW_PHL_STATUS_FAILURE;
		break;
	}

exit:
#endif
	return ret;
}

#ifdef RTW_WKARD_ISSUE_NULL_SLEEP_PROTECTION
#define ISSUE_NULL_TIME 50
#endif

struct mr_scan_chctx {
	struct rtw_chan_def *chandef;
	u8 role_map_ps;/*STA, MESH*/
	u8 role_map_ap;
};
enum rtw_phl_status
phl_mr_offch_hdl(struct phl_info_t *phl_info,
		 struct rtw_wifi_role_t *wrole,
		 bool off_ch,
		 void *obj_priv,
		 u8 (*issue_null_data)(void *priv, u8 ridx, bool ps))
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;
#ifdef CONFIG_MR_SUPPORT
	struct rtw_phl_com_t *phl_com = wrole->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	u8 hw_band = wrole->hw_band;
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[hw_band]);
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_chan_ctx *chanctx = NULL;
	struct rtw_wifi_role_t *wr = NULL;
	struct mr_scan_chctx mctx = {0};
	int ctx_num = 0;
	u8 ridx = 0;
	u8 cur_ch = rtw_hal_get_cur_ch(phl_info->hal, hw_band);
	bool found = false;

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: wrole->id(%d, off_ch(%d)\n",
		  __func__, wrole->id, off_ch);
	ctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);

	if (ctx_num == 0){
		PHL_DBG("ctx_num == 0!\n");
		return psts;
	}

	_os_spinlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
	phl_list_for_loop(chanctx, struct rtw_chan_ctx, &band_ctrl->chan_ctx_queue.queue, list) {
		/* Find the chanctx same as the current channel */
		if(chanctx->chan_def.chan != cur_ch){
			continue;
		}
		PHL_INFO("%s current chanctx found!\n", __FUNCTION__);
		mctx.chandef = &chanctx->chan_def;
		found = true;

		for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
			if (chanctx->role_map & BIT(ridx)) {
				wr = &phl_com->wifi_roles[ridx];
				if(wr->mstate != MLME_LINKED)
					continue;
				if (wr->type == PHL_RTYPE_STATION || wr->type == PHL_RTYPE_MESH || wr->type == PHL_RTYPE_TDLS){
					PHL_INFO("WR-ID:%d, STA found\n", ridx);
					mctx.role_map_ps |= BIT(ridx);
				}
				else if (wr->type == PHL_RTYPE_AP || wr->type == PHL_RTYPE_VAP || wr->type == PHL_RTYPE_MESH){
					PHL_INFO("WR-ID:%d, AP found\n", ridx);
					mctx.role_map_ap |= BIT(ridx);
				}
			}
		}
	}
	_os_spinunlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);

	if(!found){
		PHL_WARN("No chanctx is the same as current channel!\n");
		return psts;
	}

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if ((mctx.role_map_ap) && (mctx.role_map_ap & BIT(ridx))) {
			wr = &phl_com->wifi_roles[ridx];

			if(((TEST_STATUS_FLAG(wr->status, WR_STATUS_BCN_STOP)) && off_ch) ||
			   ((!TEST_STATUS_FLAG(wr->status, WR_STATUS_BCN_STOP)) && !off_ch))
				continue;
			if(off_ch){
				rtw_hal_beacon_stop(phl_info->hal, wr, off_ch);
				SET_STATUS_FLAG(wr->status, WR_STATUS_BCN_STOP);
			}
			else{
				rtw_hal_beacon_stop(phl_info->hal, wr, off_ch);
				CLEAR_STATUS_FLAG(wr->status, WR_STATUS_BCN_STOP);
			}

		}
		/* issue null-data on current channel */
		if ((mctx.role_map_ps) && (mctx.role_map_ps & BIT(ridx))) {
			wr = &phl_com->wifi_roles[ridx];
			if(issue_null_data == NULL){
				PHL_ERR("WR-ID:%d, issue null_data function not found\n", ridx);
				continue;
			}

			if(((TEST_STATUS_FLAG(wr->status, WR_STATUS_PS_ANN)) && off_ch) ||
			   ((!TEST_STATUS_FLAG(wr->status, WR_STATUS_PS_ANN)) && !off_ch))
				continue;

			if (issue_null_data(obj_priv, ridx, off_ch) != _SUCCESS) {
				PHL_ERR("WR-ID:%d, issue null_data failed\n", ridx);
				_os_warn_on(1);
				psts = RTW_PHL_STATUS_FAILURE;
			}
			else{
				if(off_ch)
					SET_STATUS_FLAG(wr->status, WR_STATUS_PS_ANN);
				else
					CLEAR_STATUS_FLAG(wr->status, WR_STATUS_PS_ANN);
#ifdef RTW_WKARD_ISSUE_NULL_SLEEP_PROTECTION
				if(off_ch)
					_os_sleep_ms(phl_to_drvpriv(phl_info), ISSUE_NULL_TIME);
#endif
			}

		}
	}
#else
	if ((wrole->type == PHL_RTYPE_STATION || wrole->type == PHL_RTYPE_TDLS) && wrole->mstate == MLME_LINKED) {
		if (issue_null_data && issue_null_data(obj_priv, wrole->id, off_ch) != _SUCCESS) {
			PHL_ERR("WR-ID:%d, issue null_data failed\n", wrole->id);
			_os_warn_on(1);
			psts = RTW_PHL_STATUS_FAILURE;
		}else{
#ifdef RTW_WKARD_ISSUE_NULL_SLEEP_PROTECTION
			if(off_ch)
				_os_sleep_ms(phl_to_drvpriv(phl_info), ISSUE_NULL_TIME);
#endif
		}

	} else if (wrole->type == PHL_RTYPE_AP) {
		rtw_hal_beacon_stop(phl_info->hal, wrole, off_ch);
	}
#endif
	return psts;
}

#ifdef CONFIG_FSM
enum rtw_phl_status
rtw_phl_mr_offch_hdl(void *phl,
		     struct rtw_wifi_role_t *wrole,
		     bool off_ch,
		     void *obj_priv,
		     u8 (*issue_null_data)(void *priv, u8 ridx, bool ps),
		     struct rtw_chan_def *chandef)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;
	u8 hw_band = wrole->hw_band;
	u8 cur_ch = rtw_hal_get_cur_ch(phl_info->hal, hw_band);

	if(off_ch){
		if(chandef->chan != cur_ch)
			psts = phl_mr_offch_hdl(phl_info,
						wrole,
						off_ch,
						obj_priv,
						issue_null_data);
	}
	else{
		psts = phl_mr_offch_hdl(phl_info,
					wrole,
					off_ch,
					obj_priv,
					issue_null_data);
	}

	return psts;
}
#endif

void phl_mr_stop_all_beacon(struct phl_info_t *phl_info,
			    struct rtw_wifi_role_t *wrole,
			    bool stop)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);
	u8 role_map = band_ctrl->role_map;
	struct rtw_wifi_role_t *wr = NULL;
	u8 ridx;

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (role_map & BIT(ridx)) {
			wr = &(phl_com->wifi_roles[ridx]);

			if((wr->type == PHL_RTYPE_AP) ||
			   (wr->type == PHL_RTYPE_VAP) ||
			   (wr->type == PHL_RTYPE_P2P_GO) ||
			   (wr->type == PHL_RTYPE_MESH)){
				if(((TEST_STATUS_FLAG(wr->status, WR_STATUS_BCN_STOP)) && stop) ||
				   ((!TEST_STATUS_FLAG(wr->status, WR_STATUS_BCN_STOP)) && !stop))
					continue;

				rtw_hal_beacon_stop(phl_info->hal, wr, stop);

				if(stop)
					SET_STATUS_FLAG(wr->status, WR_STATUS_BCN_STOP);
				else
					CLEAR_STATUS_FLAG(wr->status, WR_STATUS_BCN_STOP);
			}
		}
	}
}

#ifdef DBG_PHL_MR
enum rtw_phl_status
phl_mr_info_dbg(struct phl_info_t *phl_info)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	u8 ridx = MAX_WIFI_ROLE_NUMBER;
	u8 bidx = 0;
	int chanctx_num = 0;
	struct rtw_wifi_role_t *role = NULL;
	struct hw_band_ctl_t *band_ctrl = NULL;

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		role = &(phl_com->wifi_roles[ridx]);
		if (role->assoc_sta_queue.cnt) {
			PHL_DUMP_STACTRL_EX(phl_info);
			PHL_ERR("role_idx:%d assoc_sta_queue(%d) not empty!\n",
				ridx, role->assoc_sta_queue.cnt);
			_os_warn_on(1);
		}
	}
	for (bidx = 0; bidx < MAX_BAND_NUM; bidx++) {
		band_ctrl = &(mr_ctl->band_ctrl[bidx]);
		chanctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);
		if (chanctx_num)
			PHL_ERR("band_idx:%d chanctx_num(%d) not empty!\n", bidx, chanctx_num);
	}

	return RTW_PHL_STATUS_SUCCESS;
}
#endif

#if defined(CONFIG_PHL_P2PPS) && defined(CONFIG_MCC_SUPPORT)
static void _noa_desc_to_mcc_limit_req_info(struct rtw_phl_noa_desc *noa_desc,
	struct phl_mcc_dur_lim_req_info *limit_req_info)
{
	if (noa_desc->enable && (noa_desc->tag != P2PPS_TRIG_MCC)) {
		/* limited by NoA */
		limit_req_info->tag = RTW_MCC_DUR_LIM_NOA;
		limit_req_info->enable = true;
		limit_req_info->start_t_h = noa_desc->start_t_h;
		limit_req_info->start_t_l = noa_desc->start_t_l;
		limit_req_info->dur = noa_desc->duration;
		limit_req_info->intvl = noa_desc->interval;
	} else {
		/* No limit for NoA disable */
		limit_req_info->tag = RTW_MCC_DUR_LIM_NONE;
		limit_req_info->enable = false;
	}
}
#endif
#ifdef CONFIG_MCC_SUPPORT

/*
 * check all role state
 * Output: True: can enabe TDMRA, False: can't enable TDMRA
 */
static bool _mr_tdmra_role_state_check(struct phl_info_t *phl,
				enum phl_band_idx band_idx)
{
	struct rtw_wifi_role_t *wr = NULL;
	u8 ridx = INVALID_WIFI_ROLE_IDX, role_map = 0;

	role_map = phl_get_chanctx_rolemap(phl, band_idx);
	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (!(role_map & BIT(ridx)))
			continue;
		wr = &(phl->phl_com->wifi_roles[ridx]);
		if (MLME_LINKED != wr->mstate) {
			PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s: MLME_LINKED != wr->mstate, we can't enable tdmra now, ridx(%d), type(%d), mstate(%d)\n",
				__FUNCTION__, ridx, wr->type, wr->mstate);
			return false;
		}
		if (phl_role_is_client_category(wr)) {
			if (!TEST_STATUS_FLAG(wr->status, WR_STATUS_TSF_SYNC)) {
				PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s: Test WR_STATUS_TSF_SYNC fail, we can't enable tdmra now, ridx(%d), type(%d), status(%d)\n",
					__FUNCTION__, ridx, wr->type,
					wr->status);
				return false;
			}
		}
	}
	return true;
}

static bool _mr_role_is_in_tdmra_chctx_q
		(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wr)
{
	bool ret = false;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	void *drv = phl_to_drvpriv(phl_info);
	struct hw_band_ctl_t *band_ctrl = NULL;
	struct rtw_chan_ctx *chanctx = NULL;
	_os_list *chan_ctx_list = NULL;
	u8 role_map = 0;

	if (wr == NULL)
		goto exit;

	band_ctrl = &(mr_ctl->band_ctrl[wr->hw_band]);
	chan_ctx_list = &band_ctrl->chan_ctx_queue.queue;

	/* find wr is under existed chanctx durin TDMRA */
	_os_spinlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
	phl_list_for_loop(chanctx, struct rtw_chan_ctx, chan_ctx_list, list) {
		role_map = chanctx->role_map;
		if (role_map & BIT(wr->id)) {
			ret = true;
			break;
		}
	}
	_os_spinunlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
exit:
	return ret;
}

static bool _mr_tdmra_need(struct phl_info_t *phl_info,
				enum phl_band_idx band_idx, u8 *ap_role_idx)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[band_idx]);
	struct mr_info *cur_info = &band_ctrl->cur_info;
	u8 role_map = band_ctrl->role_map;
	struct rtw_wifi_role_t *wr = NULL, *ap_wr = NULL;
	struct rtw_chan_def *chandef = NULL;
	int ctx_num = 0;
	u8 ridx;
	u8 tdmra_need = false;

	if (phl_com->dev_cap.mcc_sup == false) {
		PHL_INFO("%s: don't support MCC\n", __func__);
		tdmra_need = false;
		goto exit;
	}

	ctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);
	if (ctx_num == 0)
		goto exit;

	PHL_INFO("[MR]%s: band_idx=%d,ctx_num=%d,ap_num=%d,op_mode=%d\n",
		__func__, band_idx, ctx_num, cur_info->ap_num, band_ctrl->op_mode);
	if (ctx_num == 1) {
		if (cur_info->ap_num == 1) {
			/* only for sole AP, check op mode */
			if (band_ctrl->op_mode == MR_OP_NON) {
				/* find sole AP */
				for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
					if (role_map & BIT(ridx)) {
						wr = &(phl_com->wifi_roles[ridx]);
						if (wr->type == PHL_RTYPE_AP && wr->mstate == MLME_LINKED) {
							ap_wr = wr;
							*ap_role_idx = ap_wr->id;
							break;
						}
					}
				}

				chandef = &wr->chandef;
				/* enable tdmra for 2.4G sole AP */
				if (rtw_hal_get_btc_req_slot(phl_info->hal) > 0
					&& chandef->band == BAND_ON_24G) {
					tdmra_need = true;
				} else {
					tdmra_need = false;
				}
			} else {
				/* SCC case */
				tdmra_need = false;
			}
		} else {
			if (rtw_hal_get_btc_req_slot(phl_info->hal) > 0)
				PHL_INFO("[MR]%s: Do not support for nonAP + BT in one ch ctx\n", __func__);

			tdmra_need = false;
		}
	} else if (ctx_num == 2) {
		tdmra_need = true;
	} else {
		PHL_INFO("[MR]%s: Do not support ctx_num(%d)\n",
			__func__, ctx_num);
	}
exit:
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "<<< %s: tdmra_need(%d)\n",
		__func__, tdmra_need);
	return tdmra_need;
}

/* find any existed role */
struct rtw_wifi_role_t *_mr_find_existed_role(struct phl_info_t *phl_info,
							enum phl_band_idx band_idx)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct rtw_wifi_role_t *wr = NULL;
	u8 role_map;
	u8 ridx = 0;

	role_map = phl_get_chanctx_rolemap(phl_info, band_idx);
	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (role_map & BIT(ridx)) {
			wr = &phl_com->wifi_roles[ridx];
			break;
		}
	}

	return wr;
}

enum rtw_phl_status _phl_mr_tdmra_disable(struct phl_info_t *phl_info,
			struct rtw_wifi_role_t *cur_wrole, enum phl_band_idx band_idx)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;
	struct rtw_wifi_role_t *spec_role = NULL;

	if (!rtw_phl_mcc_inprogress(phl_info, band_idx)) {
		psts = RTW_PHL_STATUS_SUCCESS;
		goto exit;
	} else {
		/* find existed tdmra role */
		spec_role = _mr_find_existed_role(phl_info, band_idx);
		if (spec_role == NULL) {
			PHL_ERR("%s: find no tdmra role for tdmra disable\n", __func__);
			psts = RTW_PHL_STATUS_FAILURE;
			goto exit;
		}
	}

	if (cur_wrole == NULL) {
		/* check wrole is null or not */
		psts = rtw_phl_tdmra_disable(phl_info, spec_role);
	} else {
		/* check wrole is in chan ctx queue */
		if (_mr_role_is_in_tdmra_chctx_q(phl_info, cur_wrole))
			psts = rtw_phl_tdmra_disable(phl_info, cur_wrole);
		else
			psts = rtw_phl_tdmra_disable(phl_info, spec_role);
	}

exit:
	return psts;
}

/*
 * Handle MR coex mechanism for 2g_1ap_btc, mcc, mcc_btc
 * Specific concurrent mode : 2g ap category x1 + BTC, MCC, MCC + BTC
 * @handle: True: handle specific concurrent mode for all interfaces; False: Not handleand maybe handle it by other coex mechanism.
 */
enum rtw_phl_status
_phl_mr_tdmra_handle(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *cur_wrole, u16 slot,
				enum phl_band_idx band_idx,
				enum mr_coex_trigger trgger,
				enum mr_coex_mode *coex_mode)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct phl_tdmra_dur_change_info info = {0};
	bool tdmra_inprogress = false, tdmra_need = false;
	u8 ap_role_id = INVALID_WIFI_ROLE_IDX;
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;
	struct rtw_wifi_role_t * existed_role = NULL;

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, ">>> %s: slot(%d), band_idx(%d), trgger(%d)\n",
		__func__, slot, band_idx, trgger);
	if (!_mr_tdmra_role_state_check(phl_info, band_idx)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "%s: Fail to check role state\n",
			__func__);
		goto exit;
	}
	tdmra_inprogress =  rtw_phl_mcc_inprogress(phl_info, band_idx);
	tdmra_need = _mr_tdmra_need(phl_info, band_idx, &ap_role_id);
	if (tdmra_need) {
		switch (trgger){
		case MR_COEX_TRIG_BY_BT:
			if (tdmra_inprogress) {
				/* update TDMRA (if TDMRAing) */
				info.bt_role = true;
				info.hw_band = band_idx;
				/* find existed tdmra role */
				info.role = _mr_find_existed_role(phl_info, band_idx);
				info.dur = slot;
				if (info.role != NULL)
					psts = rtw_phl_tdmra_duration_change(phl_info, &info);
				else
					PHL_ERR("%s: find no tdmra role for tdmra duration change\n", __func__);
			} else {
				/* enable TDMRA under solo AP (no TDMRA) */
				if (ap_role_id < INVALID_WIFI_ROLE_IDX) {
					*coex_mode = MR_COEX_MODE_TDMRA;
					psts = rtw_phl_tdmra_enable(phl_info, &phl_com->wifi_roles[ap_role_id]);
				} else {
					PHL_ERR("%s: Do not find solo AP\n", __func__);
				}
			}
			break;
		case MR_COEX_TRIG_BY_LINKING:
			/* enable TDMRA (for link flow) */
			if (cur_wrole != NULL) {
				*coex_mode = MR_COEX_MODE_TDMRA;
				phl_mr_stop_all_beacon(phl_info, cur_wrole, false);
				psts = rtw_phl_tdmra_enable(phl_info, cur_wrole);
			} else {
				PHL_ERR("%s: cur_wrole = NULL, check code flow\n", __func__);
			}
			break;
		case MR_COEX_TRIG_BY_DIS_LINKING:
			*coex_mode = MR_COEX_MODE_TDMRA;
			/* find any existed role to trigger TDMRA */
			existed_role = _mr_find_existed_role(phl_info, band_idx);
			psts = rtw_phl_tdmra_enable(phl_info, existed_role);
			break;
		case MR_COEX_TRIG_BY_CHG_SLOT:
			if (tdmra_inprogress) {
				/* update TDMRA (if TDMRAing) */
				info.bt_role = false;
				info.hw_band = band_idx;
				/* find existed tdmra role */
				info.role = _mr_find_existed_role(phl_info, band_idx);
				info.dur = slot;
				if (info.role != NULL)
					psts = rtw_phl_tdmra_duration_change(phl_info, &info);
				else
					PHL_ERR("%s: find no tdmra role\n", __func__);
			}
			break;
		case MR_COEX_TRIG_BY_SCAN:
			*coex_mode = MR_COEX_MODE_TDMRA;
			psts = rtw_phl_tdmra_enable(phl_info, cur_wrole);
			break;
		case MR_COEX_TRIG_BY_CHG_OP_CHDEF:
			*coex_mode = MR_COEX_MODE_TDMRA;
			psts = rtw_phl_tdmra_enable(phl_info, cur_wrole);
			break;
		default:
			break;
		}
	} else {
			/* disable TDMRA (if TDMRAing) */
		if (*coex_mode == MR_COEX_MODE_TDMRA) {
			psts = _phl_mr_tdmra_disable(phl_info, cur_wrole, band_idx);
			if (psts != RTW_PHL_STATUS_SUCCESS)
				PHL_ERR("%s: MR TDMRA disable fail\n", __func__);
			else
				*coex_mode = MR_COEX_MODE_NONE;
		}
	}
exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: coex_mode(%d), psts(%d)\n",
		__func__, *coex_mode, psts);
	return psts;
}

enum rtw_phl_status
phl_mr_mcc_query_role_time_slot_lim (struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole,
								struct phl_mcc_dur_lim_req_info *limit_req_info)
{
#ifdef CONFIG_PHL_P2PPS
	struct rtw_phl_noa_desc noa_desc = {0};

	/* limited by NoA */
	phl_p2pps_query_noa_with_cnt255(phl_info, wrole, &noa_desc);
	_noa_desc_to_mcc_limit_req_info(&noa_desc, limit_req_info);
#else
	/* No limit for NoA disable */
	limit_req_info->tag = RTW_MCC_DUR_LIM_NONE;
	limit_req_info->enable = false;
#endif
	return RTW_PHL_STATUS_SUCCESS;
}
#endif
#ifdef CONFIG_PHL_P2PPS
/* call by noa module for noa enable/disable */
bool phl_mr_noa_dur_lim_change (struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole,
								struct rtw_phl_noa_desc *noa_desc)
{
	u8 tdmra_inprogress = false, ctrl_by_tdmra = false, need_tdmra = false;
#ifdef CONFIG_MCC_SUPPORT
	struct phl_mcc_dur_lim_req_info lim_req = {0};
	u8 ap_role_idx;
#endif

	tdmra_inprogress = rtw_phl_mcc_inprogress(phl_info, wrole->hw_band);

	/* tdmra inprogress */
	if (tdmra_inprogress) {
		ctrl_by_tdmra = true;
		#ifdef CONFIG_MCC_SUPPORT
		/* under MCC, control by MCC module */
		_noa_desc_to_mcc_limit_req_info(noa_desc, &lim_req);
		rtw_phl_mcc_dur_lim_change(phl_info, wrole, &lim_req);
		#endif
	} else {
		/*
			tdmra not inprogress,
			but will process tdmra if need_tdmra = true
			then, tdmra will qurey NoA parameter
		*/
#ifdef CONFIG_MCC_SUPPORT
		need_tdmra = _mr_tdmra_need(phl_info, wrole->hw_band, &ap_role_idx);
#endif
		if (need_tdmra)
			ctrl_by_tdmra = true;
		else
			ctrl_by_tdmra = false;
	}

	return ctrl_by_tdmra;
}
#endif
enum rtw_phl_status
phl_mr_info_upt(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);
	u8 role_map = band_ctrl->role_map;
	/*struct rtw_chan_ctx *chanctx = wrole->chanctx;
	u8 role_map = chanctx->role_map;*/
	struct rtw_wifi_role_t *wr = NULL;
	u8 ridx;

	_os_mem_set(drv, &band_ctrl->cur_info, 0, sizeof(struct mr_info));

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (role_map & BIT(ridx)) {
			wr = &(phl_com->wifi_roles[ridx]);

			switch (wr->type) {
			case PHL_RTYPE_STATION:
			case PHL_RTYPE_P2P_GC:
			case PHL_RTYPE_P2P_DEVICE:
			case PHL_RTYPE_TDLS:
				band_ctrl->cur_info.sta_num++;
				if (wr->mstate == MLME_LINKED)
					band_ctrl->cur_info.ld_sta_num++;
				if (wr->mstate == MLME_LINKING)
					band_ctrl->cur_info.lg_sta_num++;
				if (wr->type == PHL_RTYPE_P2P_GC)
					band_ctrl->cur_info.p2p_gc_num++;
				if (wr->type == PHL_RTYPE_P2P_DEVICE)
					band_ctrl->cur_info.p2p_device_num++;
			#ifdef CONFIG_PHL_TDLS
				if (wr->type == PHL_RTYPE_TDLS)
					band_ctrl->cur_info.ld_tdls_num++;
			#endif
				break;

			case PHL_RTYPE_AP:
			case PHL_RTYPE_VAP:
			case PHL_RTYPE_P2P_GO:
			case PHL_RTYPE_MESH:
				if (wr->mstate == MLME_LINKED)
					band_ctrl->cur_info.ap_num++;
				if (wr->assoc_sta_queue.cnt >= 2)
					band_ctrl->cur_info.ld_ap_num++;
				if (wr->type == PHL_RTYPE_P2P_GO)
					band_ctrl->cur_info.p2p_go_num++;
				break;

			case PHL_RTYPE_MONITOR:
			default :
				break;
			}
			#if 0
			if (wr->type == PHL_RTYPE_STATION)
				band_ctrl->cur_info.sta_num++;
			else if (wr->type == PHL_RTYPE_AP || wr->type == PHL_RTYPE_VAP)
				band_ctrl->cur_info.ap_num++;
			else if (wr->type == PHL_RTYPE_ADHOC || wr->type == PHL_RTYPE_ADHOC_MASTER)
			else if (wr->type == PHL_RTYPE_MESH)
			else if (wr->type == PHL_RTYPE_MONITOR)
			else if (wr->type == PHL_RTYPE_P2P_DEVICE)
			else if (wr->type == PHL_RTYPE_P2P_GC)
			else if (wr->type == PHL_RTYPE_P2P_GO)
			else if (wr->type == PHL_RTYPE_TDLS)
			else if (wr->type == PHL_RTYPE_NAN)
			#endif
		}
	}
	if(band_ctrl->op_mode == MR_OP_SCC ||
	   band_ctrl->op_mode == MR_OP_MCC){
		if(band_ctrl->cur_info.ld_sta_num && band_ctrl->cur_info.ap_num)
			band_ctrl->op_type = MR_OP_TYPE_STATION_AP;
		else if(band_ctrl->cur_info.ld_sta_num)
			band_ctrl->op_type = MR_OP_TYPE_STATION_ONLY;
		else
			band_ctrl->op_type = MR_OP_TYPE_STATION_AP;
	}
	else{
		band_ctrl->op_type = MR_OP_TYPE_NONE;
	}
	/*dump mr_info*/
	PHL_INFO("%s sta_num:%d, ld_sta_num:%d, lg_sta_num:%d\n",
		__func__, band_ctrl->cur_info.sta_num,
		band_ctrl->cur_info.ld_sta_num, band_ctrl->cur_info.lg_sta_num);
#ifdef CONFIG_PHL_TDLS
	PHL_INFO("%s ld_tdls_num:%d\n", __func__, band_ctrl->cur_info.ld_tdls_num);
#endif
	PHL_INFO("%s ap_num:%d, ld_ap_num:%d\n",
		__func__, band_ctrl->cur_info.ap_num, band_ctrl->cur_info.ld_ap_num);
	PHL_INFO("%s op mode:%d op type:%d\n",
		__func__, band_ctrl->op_mode, band_ctrl->op_type);
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_mr_state_upt(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;
#ifdef CONFIG_MR_SUPPORT
	bool mcc_en = false;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);
	int chanctx_num = 0;

	chanctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);
	PHL_INFO("%s chanctx_num:%d\n", __func__, chanctx_num);

	if (chanctx_num == 2) {
		/*MR - MCC section*/
		mcc_en = (wrole->mstate == MLME_LINKED) ? true : false;
		if(mcc_en == false)
			phl_mr_check_ecsa_cancel(phl_info, wrole);
	}
#ifdef CONFIG_MCC_SUPPORT
	/* Enable MR coex mechanism(if needed) */
	psts = phl_mr_coex_handle(phl_info, wrole, 0, wrole->hw_band, MR_COEX_TRIG_BY_LINKING);
	if (psts != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s: MR coex handle fail(%d)\n", __func__, psts);
		goto exit;
	}
#endif /* CONFIG_MCC_SUPPORT */
#ifdef CONFIG_PHL_P2PPS
	/* Process NOA */
	phl_p2pps_noa_all_role_resume(phl_info, wrole->hw_band);
#endif

	if(psts == RTW_PHL_STATUS_SUCCESS && mcc_en)
		phl_mr_check_ecsa(phl_info, wrole);

exit:
#endif
	return psts;
}

enum rtw_phl_status phl_mr_watchdog(struct phl_info_t *phl_info)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_SUCCESS;
#if defined(CONFIG_MR_SUPPORT) &&  defined(CONFIG_MCC_SUPPORT)
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	u8 band_idx = 0;
	int chanctx_num = 0;
	struct hw_band_ctl_t *band_ctrl = NULL;
	#if 0
	struct rtw_chan_ctx *chanctx = NULL;
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_wifi_role_t *wr = NULL;
	#endif

	for (band_idx = 0; band_idx < MAX_BAND_NUM; band_idx++) {
		band_ctrl = &(mr_ctl->band_ctrl[band_idx]);
		chanctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);
		if (chanctx_num == 0)
			continue;

		#if 0
		_os_spinlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
		phl_list_for_loop(chanctx, struct rtw_chan_ctx, &band_ctrl->chan_ctx_queue.queue, list) {
			for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
				if (chanctx->role_map & BIT(ridx)) {
					wr = &phl_com->wifi_roles[ridx];
					if(wr->type == PHL_RTYPE_STATION) {
						/*Sounding check*/
						/*phl_snd_watchdog(phl_info, wr);*/
					}
				}
			}
		}
		_os_spinunlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
		#endif

		if (chanctx_num == 2)
			rtw_phl_mcc_watchdog(phl_info, band_idx);
	}
#endif /*CONFIG_MCC_SUPPORT*/
	return psts;
}

static inline u8
__phl_mr_process(struct rtw_wifi_role_t *wrole,
		u8 role_map, bool exclude_self, void *data,
		u8(*ops_func)(struct rtw_wifi_role_t *wrole, void *data))
{
	u8 ridx = 0;
	u8 ret = 0;
	struct rtw_phl_com_t *phl_com = wrole->phl_com;
	struct rtw_wifi_role_t *wr = NULL;

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (role_map & BIT(ridx)) {
			wr = &phl_com->wifi_roles[ridx];
			_os_warn_on(!wr->active);
			if ((exclude_self) && (wr == wrole))
				continue;
			if (ops_func)
				if (true == ops_func(wr, data))
					ret++;
		}
	}
	return ret;
}

static u8 _phl_mr_process_by_mrc(struct phl_info_t *phl_info,
		struct rtw_wifi_role_t *wrole, bool exclude_self, void *data,
		u8(*ops_func)(struct rtw_wifi_role_t *wrole, void *data))
{
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(wrole->phl_com);

	return __phl_mr_process(wrole, mr_ctl->role_map, exclude_self, data, ops_func);
}

static u8 _phl_mr_process_by_band(struct phl_info_t *phl_info,
		struct rtw_wifi_role_t *wrole, bool exclude_self, void *data,
		u8(*ops_func)(struct rtw_wifi_role_t *wrole, void *data))
{
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(wrole->phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);

	return __phl_mr_process(wrole, band_ctrl->role_map, exclude_self, data, ops_func);
}

static u8 _phl_mr_process_by_chctx(struct phl_info_t *phl_info,
		struct rtw_wifi_role_t *wrole, bool exclude_self, void *data,
		u8(*ops_func)(struct rtw_wifi_role_t *wrole, void *data))
{
	struct rtw_chan_ctx *chanctx = wrole->chanctx;

	if (!chanctx)
		return 0;
	return __phl_mr_process(wrole, chanctx->role_map, exclude_self, data, ops_func);
}

static u8 _phl_mr_process(struct phl_info_t *phl_info,
		struct rtw_wifi_role_t *wrole, bool exclude_self, void *data,
		u8(*ops_func)(struct rtw_wifi_role_t *wrole, void *data))
{
	u8 ridx = 0;
	u8 ret = 0;
	struct rtw_phl_com_t *phl_com = wrole->phl_com;
	struct rtw_wifi_role_t *wr = NULL;

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		wr = &phl_com->wifi_roles[ridx];
		if (wr && wr->active) {
			if ((exclude_self) && (wr == wrole))
				continue;
			if (ops_func)
				if (true == ops_func(wr, data))
					ret++;
		}
	}
	return ret;
}

bool rtw_phl_mr_query_info(void *phl, struct rtw_wifi_role_t *wrole,
			struct mr_query_info *info)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);

	info->op_mode = band_ctrl->op_mode;
	info->op_type = band_ctrl->op_type;
	_os_mem_cpy(phl_to_drvpriv(phl_info), &info->cur_info,
			&band_ctrl->cur_info, sizeof(struct mr_info));
	return true;
}

static u8 _phl_mr_dump_mac_addr(struct rtw_wifi_role_t *wrole, void *data)
{
	PHL_INFO("RIDX:%d - MAC-Addr:%02x-%02x-%02x-%02x-%02x-%02x\n",
			wrole->id, wrole->mac_addr[0], wrole->mac_addr[1], wrole->mac_addr[2],
			wrole->mac_addr[3], wrole->mac_addr[4], wrole->mac_addr[5]);
	return true;
}
u8 rtw_phl_mr_dump_mac_addr(void *phl,
					struct rtw_wifi_role_t *wifi_role)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	return _phl_mr_process(phl_info, wifi_role, false, NULL, _phl_mr_dump_mac_addr);
}
u8 rtw_phl_mr_buddy_dump_mac_addr(void *phl,
					struct rtw_wifi_role_t *wifi_role)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	return _phl_mr_process(phl_info, wifi_role, true, NULL, _phl_mr_dump_mac_addr);
}


void rtw_phl_mr_ops_init(void *phl, struct rtw_phl_mr_ops *mr_ops)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);

	mr_ctl->mr_ops.priv = mr_ops->priv;
#ifdef CONFIG_PHL_P2PPS
	mr_ctl->mr_ops.phl_mr_update_noa = mr_ops->phl_mr_update_noa;
#endif /* CONFIG_PHL_P2PPS */
#ifdef CONFIG_MCC_SUPPORT
	if (phl_com->dev_cap.mcc_sup == true) {
		rtw_phl_mcc_init_ops(phl_info, mr_ops->mcc_ops);
	}
#endif
}

u8 rtw_phl_mr_get_opch_list(void *phl, struct rtw_wifi_role_t *wifi_role,
			    struct rtw_chan_def *chdef_list, u8 list_size)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wifi_role->hw_band]);
	void *drv = phl_to_drvpriv(phl_info);
	struct rtw_chan_ctx *chanctx = NULL;
	int ctx_num = 0;
	u8 total_op_num = 0;

	if((chdef_list == NULL) || (list_size == 0)){
		PHL_ERR("Parameter Invalid!\n");
		goto _exit;
	}

	ctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);

	if (ctx_num == 0){
		PHL_DBG("ctx_num == 0!\n");
		goto _exit;
	}

	_os_spinlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
	phl_list_for_loop(chanctx, struct rtw_chan_ctx, &band_ctrl->chan_ctx_queue.queue, list) {
		if(total_op_num >= list_size)
			break;

		*(chdef_list + total_op_num) = chanctx->chan_def;
		total_op_num++;
	}
	_os_spinunlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
_exit:
	return total_op_num;
}

enum mr_op_mode
rtw_phl_mr_get_opmode(void *phl, struct rtw_wifi_role_t *wrole)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);

	return band_ctrl->op_mode;
}

void
phl_mr_check_ecsa(struct phl_info_t *phl_info,
		  struct rtw_wifi_role_t *wrole)
{
#ifdef CONFIG_PHL_ECSA
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);
	u8 role_map = band_ctrl->role_map;
	struct rtw_wifi_role_t *wr = NULL, *ap_wr = NULL, *sta_wr = NULL;
	u8 ridx;
	enum band_type ap_band_type = BAND_ON_24G, sta_band_type = BAND_ON_24G;
	enum phl_ecsa_start_reason reason = ECSA_START_UNKNOWN;
	u32 delay_start_ms = 0;
#ifdef CONFIG_PHL_ECSA_EXTEND_OPTION
	u32 extend_option = 0;
#endif
	bool ecsa_allow = false;
	struct rtw_phl_ecsa_param param = {0};
	void *d = phlcom_to_drvpriv(phl_com);


	if(band_ctrl->op_mode != MR_OP_MCC)
		return;
	if(band_ctrl->op_type != MR_OP_TYPE_STATION_AP)
		return;

	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (role_map & BIT(ridx)) {
			wr = &(phl_com->wifi_roles[ridx]);

			switch (wr->type) {
			case PHL_RTYPE_STATION:
			case PHL_RTYPE_P2P_GC:
			case PHL_RTYPE_TDLS:
				if (wr->mstate == MLME_LINKED){
					sta_band_type = wr->chandef.band;
					sta_wr = wr;
				}
				break;
			case PHL_RTYPE_AP:
			case PHL_RTYPE_VAP:
			case PHL_RTYPE_P2P_GO:
			case PHL_RTYPE_MESH:
				if (wr->mstate == MLME_LINKED){
					ap_band_type = wr->chandef.band;
					ap_wr = wr;
				}
				break;
			case PHL_RTYPE_MONITOR:
			case PHL_RTYPE_P2P_DEVICE:
			default :
				break;
			}
		}
	}

	if (sta_wr == NULL) {
		PHL_TRACE(COMP_PHL_ECSA, _PHL_WARNING_, "[ECSA] Sta role not found!\n");
		return;
	}

	if (ap_wr == NULL) {
		PHL_TRACE(COMP_PHL_ECSA, _PHL_WARNING_, "[ECSA] AP role not found!\n");
		return;
	}

	if(sta_band_type == BAND_ON_24G){
		if(ap_band_type == BAND_ON_24G)
			reason = ECSA_START_MCC_24G_TO_24G;
		else if(ap_band_type == BAND_ON_5G)
			reason = ECSA_START_MCC_5G_TO_24G;
		else
			reason = ECSA_START_UNKNOWN;
	}
	else if(sta_band_type == BAND_ON_5G){
		if(ap_band_type == BAND_ON_24G)
			reason = ECSA_START_MCC_24G_TO_5G;
		else if(ap_band_type == BAND_ON_5G)
			reason = ECSA_START_MCC_5G_TO_5G;
		else
			reason = ECSA_START_UNKNOWN;
	}
	else{
		reason = ECSA_START_UNKNOWN;
	}

	if(reason != ECSA_START_UNKNOWN){
		ecsa_allow = rtw_phl_ecsa_check_allow(phl_info,
						      ap_wr,
						      sta_wr->chandef,
						      reason,
#ifdef CONFIG_PHL_ECSA_EXTEND_OPTION
						      &extend_option,
#endif
						      &delay_start_ms);
	}

	if(ecsa_allow){
		param.ecsa_type = ECSA_TYPE_AP;
		param.ch = sta_wr->chandef.chan;
		param.op_class = rtw_phl_get_operating_class(sta_wr->chandef);
		param.count = ECSA_DEFAULT_CHANNEL_SWITCH_COUNT;
		param.delay_start_ms = delay_start_ms;
		param.flag = 0;
		param.mode = CHANNEL_SWITCH_MODE_NORMAL;
		_os_mem_cpy(d, &(param.new_chan_def), &(sta_wr->chandef),
			    sizeof(struct rtw_chan_def));

#ifdef CONFIG_PHL_ECSA_EXTEND_OPTION
		rtw_phl_ecsa_extend_option_hdlr(extend_option,
						&param);
#endif

		pstatus = rtw_phl_ecsa_start(phl_info,
					     ap_wr,
					     &param);
		if(pstatus == RTW_PHL_STATUS_SUCCESS)
			PHL_INFO("%s: ECSA start OK!\n", __FUNCTION__);
		else
			PHL_INFO("%s: ECSA start fail!\n", __FUNCTION__);
	}
#endif /* CONFIG_PHL_ECSA */
}

void
phl_mr_check_ecsa_cancel(struct phl_info_t *phl_info,
			 struct rtw_wifi_role_t *wrole)
{
#ifdef CONFIG_PHL_ECSA
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_SUCCESS;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);

	if(band_ctrl->op_mode != MR_OP_MCC)
		return;
	if(band_ctrl->op_type != MR_OP_TYPE_STATION_AP)
		return;

	pstatus = rtw_phl_ecsa_cancel(phl_info, wrole);
	if(pstatus == RTW_PHL_STATUS_SUCCESS)
		PHL_INFO("%s: ECSA cancel OK!\n", __FUNCTION__);
	else
		PHL_INFO("%s: ECSA cancel fail!\n", __FUNCTION__);
#endif /* CONFIG_PHL_ECSA */
}
#ifdef CONFIG_MCC_SUPPORT
u8 phl_mr_query_mcc_inprogress (struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole,
				enum rtw_phl_mcc_chk_inprocess_type check_type)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct hw_band_ctl_t *band_ctrl = &(mr_ctl->band_ctrl[wrole->hw_band]);
	int chanctx_num = 0;
	u8 ret = false;
	u8 mcc_inprogress = false;

	if (phl_com->dev_cap.mcc_sup == false) {
		ret = false;
		goto _exit;
	}

	chanctx_num = phl_mr_get_chanctx_num(phl_info, band_ctrl);
	mcc_inprogress = rtw_phl_mcc_inprogress(phl_info, wrole->hw_band);

	switch (check_type) {
	case RTW_PHL_MCC_CHK_INPROGRESS:
		ret = mcc_inprogress;
		break;
	case RTW_PHL_MCC_CHK_INPROGRESS_SINGLE_CH:
		if (mcc_inprogress && chanctx_num == 1)
			ret = true;
		else
			ret = false;
		break;
	case RTW_PHL_MCC_CHK_INPROGRESS_MULTI_CH:
		if (mcc_inprogress && chanctx_num >= 2)
			ret = true;
		else
			ret = false;
		break;
	case RTW_PHL_MCC_CHK_MAX:
		ret = false;
		break;
	}

_exit:
	return ret;
}

u8 rtw_phl_mr_query_mcc_inprogress (void *phl, struct rtw_wifi_role_t *wrole,
							enum rtw_phl_mcc_chk_inprocess_type check_type)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	return phl_mr_query_mcc_inprogress(phl_info, wrole, check_type);
}
#endif

enum rtw_phl_status
phl_mr_err_recovery(struct phl_info_t *phl, enum phl_msg_evt_id eid)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_SUCCESS;

	if (eid == MSG_EVT_SER_M5_READY) {
		/* SER L1 DONE event */
		rtw_phl_mcc_recovery(phl, HW_BAND_0);
	}

	return status;
}

/* MR coex related code */
#ifdef CONFIG_MCC_SUPPORT
#ifdef CONFIG_PHL_P2PPS
static bool
_mr_coex_up_noa_for_bt_req(struct phl_info_t *phl, u16 bt_slot,
	struct rtw_wifi_role_t *wrole, enum p2pps_trig_tag tag)
{
	bool ret = false;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl->phl_com);
	struct rtw_phl_noa param = {0};
	u32 tsf_h = 0, tsf_l = 0;
	u64 tsf = 0, start_tsf = 0;
	u16 offset = 0;

	if (!mr_ctl->mr_ops.phl_mr_update_noa) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "%s(): ops.phl_mr_update_noa == NULL\n",
			__func__);
		goto exit;
	}
	param.wrole = wrole;
	param.tag = tag;
	param.dur = bt_slot;
	if (param.dur == 0)
		goto _ops;
	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_get_tsf(phl->hal,
				wrole->hw_port, &tsf_h, &tsf_l)) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s(): Get tsf fail, hw port(%d)\n",
			__func__, wrole->hw_port);
		goto exit;
	}
	tsf = tsf_h;
	tsf = tsf << 32;
	tsf |= tsf_l;
	if (!phl_calc_offset_from_tbtt(phl, wrole, tsf, &offset)) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s(): Get offset fail\n",
			__func__);
		goto exit;
	}
	param.cnt = 255;
	param.interval = phl_role_get_bcn_intvl(phl, wrole);
	start_tsf = tsf - offset * TU + ((param.interval - param.dur) / 2) * TU;
	start_tsf = start_tsf + param.interval;/*Next beacon start Noa*/
	param.start_t_h = (u32)(start_tsf >> 32);
	param.start_t_l = (u32)start_tsf;
_ops:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s(): NOA_start(0x%08x %08x), dur(%d), cnt(%d), interval(%d)\n",
		__func__, param.start_t_h, param.start_t_l, param.dur,
		param.cnt, param.interval);
	mr_ctl->mr_ops.phl_mr_update_noa(mr_ctl->mr_ops.priv, &param);
	ret = true;
exit:
	return ret;
}

/*
 * Return True: If concurrent mode is ap category x1 and client category x1
 */
static bool
_mr_is_2g_scc_1ap_1sta(struct phl_info_t *phl,
			enum phl_band_idx band_idx, struct rtw_wifi_role_t **ap_wr)
{
	bool need = false;
	struct hw_band_ctl_t *band_ctrl = get_band_ctrl(phl, band_idx);
	struct mr_info *cur_info = &band_ctrl->cur_info;
	u8 ridx = 0, role_map = band_ctrl->role_map;
	struct rtw_wifi_role_t *wr = NULL;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: band_idx(%d), op_mode(%d), ap_num(%d), p2p_go_num(%d), ld_sta_num(%d)\n",
		__FUNCTION__, band_idx, band_ctrl->op_mode,
		cur_info->ap_num, cur_info->p2p_go_num,
		cur_info->ld_sta_num);
	if (band_ctrl->op_mode != MR_OP_SCC)
		goto exit;
	if (!(1 == cur_info->ap_num || 1 == cur_info->p2p_go_num))
		goto exit;
	if (cur_info->ld_sta_num == 0)
		goto exit;
	/*get ap role*/
	for (ridx = 0; ridx < MAX_WIFI_ROLE_NUMBER; ridx++) {
		if (!(role_map & BIT(ridx)))
			continue;
		wr = &(phl->phl_com->wifi_roles[ridx]);
		if (phl_role_is_ap_category(wr)&&
			wr->mstate == MLME_LINKED) {
			*ap_wr = wr;
			break;
		}
	}
	if (*ap_wr == NULL)
		goto exit;
	if ((*ap_wr)->chandef.band != BAND_ON_24G)
		goto exit;
	need = true;
exit:
	return need;
}

/*
 * Handle MR coex mechanism for 2g_scc_1ap_1sta_btc
 * Specific concurrent mode : ap category x1, client category x1 + BTC
 * @handle: True: handle specific concurrent mode for all interfaces; False: Not handleand maybe handle it by other coex mechanism.
 */
static enum rtw_phl_status
_phl_mr_2g_scc_1ap_1sta_btc_handle(struct phl_info_t *phl,
		enum phl_band_idx band_idx, enum mr_coex_trigger trgger,
		enum mr_coex_mode *coex_mode)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_SUCCESS;
	struct rtw_wifi_role_t *ap_wr = NULL;
	u16 bt_slot = 0;

	if (!_mr_is_2g_scc_1ap_1sta(phl, band_idx, &ap_wr)) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s: It's not 2g_scc_1ap_1sta\n",
			__FUNCTION__);
		goto exit;
	}
	bt_slot = (u16)rtw_hal_get_btc_req_slot(phl->hal);
	if (trgger != MR_COEX_TRIG_BY_BT && bt_slot == 0)
		goto exit;
	if (_mr_coex_up_noa_for_bt_req(phl, bt_slot, ap_wr,
						P2PPS_TRIG_2G_SCC_1AP_1STA_BT)) {
		if (bt_slot > 0)
			*coex_mode = MR_COEX_MODE_2GSCC_1AP_1STA_BTC;
		else
			*coex_mode = MR_COEX_MODE_NONE;
		PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s: Up Noa ok\n",
			__FUNCTION__);
	} else {
		status = RTW_PHL_STATUS_FAILURE;
		PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s: Up Noa fail\n",
			__FUNCTION__);
	}
exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: coex_mode(%d), status(%d), trgger(%d), bt_slot(%d)\n",
		__FUNCTION__, *coex_mode, status, trgger, bt_slot);
	return status;
}

/*
 * Disable MR coex mechanism of 2g_scc_1ap_1sta_btc
 */
enum rtw_phl_status
_phl_mr_2g_scc_1ap_1sta_btc_disable(struct phl_info_t *phl,
			enum phl_band_idx band_idx)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	struct rtw_wifi_role_t *ap_wr = NULL;

	if (!_mr_is_2g_scc_1ap_1sta(phl, band_idx, &ap_wr)) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s: It¡¦s not 2g_scc_1ap_1sta\n",
			__FUNCTION__);
		goto exit;
	}
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: band_idx(%d)\n",
		__FUNCTION__, band_idx);
	if (_mr_coex_up_noa_for_bt_req(phl, 0, ap_wr, P2PPS_TRIG_2G_SCC_1AP_1STA_BT)) {
		status = RTW_PHL_STATUS_SUCCESS;
	} else {
		status = RTW_PHL_STATUS_FAILURE;
		PHL_TRACE(COMP_PHL_MCC, _PHL_WARNING_, "%s: Up Noa fail\n",
			__FUNCTION__);
	}
exit:
	return status;
}
#endif /* CONFIG_PHL_P2PPS */

/*
 * Disable MR coex mechanism which is TDMRA or 2g_scc_1ap_1sta_btc mechanism
 */
enum rtw_phl_status
phl_mr_coex_disable(struct phl_info_t *phl,
		struct rtw_wifi_role_t *cur_wrole, enum phl_band_idx band_idx,
		enum mr_coex_trigger trgger)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	struct hw_band_ctl_t *band_ctrl = get_band_ctrl(phl, band_idx);

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: band_idx(%d), trgger(%d)\n",
		__FUNCTION__, band_idx, trgger);
#ifdef CONFIG_PHL_P2PPS
	if (band_ctrl->coex_mode == MR_COEX_MODE_2GSCC_1AP_1STA_BTC) {
		status = _phl_mr_2g_scc_1ap_1sta_btc_disable(phl, band_idx);
		if (status == RTW_PHL_STATUS_SUCCESS)
			band_ctrl->coex_mode = MR_COEX_MODE_NONE;
	} else
#endif /* CONFIG_PHL_P2PPS */
#ifdef CONFIG_MCC_SUPPORT
	if (band_ctrl->coex_mode == MR_COEX_MODE_TDMRA) {
		status = _phl_mr_tdmra_disable(phl, cur_wrole, band_idx);
		if (status == RTW_PHL_STATUS_SUCCESS)
			band_ctrl->coex_mode = MR_COEX_MODE_NONE;
	} else
#endif /* CONFIG_MCC_SUPPORT */
	{
		status = RTW_PHL_STATUS_SUCCESS;
	}
	if (trgger == MR_COEX_TRIG_BY_SCAN) {
		phl_mr_stop_all_beacon(phl, cur_wrole, true);
	}
	if (status != RTW_PHL_STATUS_SUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "%s: Handle by %d fail\n",
			__FUNCTION__, band_ctrl->coex_mode);
	}
	return status;
}

/*
 * MR will excute suitable coex mechanism for Multi-interface
 * @cur_wrole: current role
 * @slot: time slot, Interpretation by trgger event.
 * Ignore it, if trgger event is MR_COEX_TRIG_BY_LINKING/MR_COEX_TRIG_BY_DIS_LINKING/
 * MR_COEX_TRIG_BY_SCAN/MR_COEX_TRIG_BY_ECSA.
 * @band_idx: hw band
 * @trgger: Trigger event
 */
enum rtw_phl_status
phl_mr_coex_handle(struct phl_info_t *phl, struct rtw_wifi_role_t *cur_wrole,
	u16 slot, enum phl_band_idx band_idx, enum mr_coex_trigger trgger)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	struct hw_band_ctl_t *band_ctrl = get_band_ctrl(phl, band_idx);

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: band_idx(%d), trgger(%d), slot(%d)\n",
		__FUNCTION__, band_idx, trgger, slot);
#ifdef CONFIG_PHL_P2PPS
	status = _phl_mr_2g_scc_1ap_1sta_btc_handle(phl, band_idx, trgger,
							&band_ctrl->coex_mode);
	if (status != RTW_PHL_STATUS_SUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "%s: Handle 2g_scc_1ap_1sta_btc fail\n",
			__FUNCTION__);
		goto exit;
	}
#endif /* CONFIG_PHL_P2PPS */
#ifdef CONFIG_MCC_SUPPORT
	if (band_ctrl->coex_mode != MR_COEX_MODE_2GSCC_1AP_1STA_BTC) {
		status = _phl_mr_tdmra_handle(phl, cur_wrole, slot, band_idx,
						trgger, &band_ctrl->coex_mode);
		if (status != RTW_PHL_STATUS_SUCCESS) {
			PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "%s: Handle TDMRA fail\n",
			__FUNCTION__);
			goto exit;
		}
	}
#endif /* CONFIG_MCC_SUPPORT */
	if (trgger == MR_COEX_TRIG_BY_SCAN) {
		phl_mr_stop_all_beacon(phl, cur_wrole, false);
	}
exit:
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "%s: status(%d), coex_mode %d\n",
			__FUNCTION__, status, band_ctrl->coex_mode);
	return status;
}
#endif /* CONFIG_MCC_SUPPORT */
