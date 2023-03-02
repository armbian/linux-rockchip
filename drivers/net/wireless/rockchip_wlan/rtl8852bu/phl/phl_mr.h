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
#ifndef _PHL_MR_H_
#define _PHL_MR_H_

static inline int
phl_mr_get_chanctx_num(struct phl_info_t *phl_info, struct hw_band_ctl_t *band_ctrl)
{
	void *drv = phl_to_drvpriv(phl_info);
	int chanctx_num = 0;

	_os_spinlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
	chanctx_num = band_ctrl->chan_ctx_queue.cnt;
	_os_spinunlock(drv, &band_ctrl->chan_ctx_queue.lock, _ps, NULL);
	return chanctx_num;
}

static inline u8
phl_mr_get_role_num(struct phl_info_t *phl_info,
				struct hw_band_ctl_t *band_ctrl)
{
	void *drv = phl_to_drvpriv(phl_info);
	u8 i;
	u8 role_num = 0;

	_os_spinlock(drv, &band_ctrl->lock, _ps, NULL);
	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		if (band_ctrl->role_map & BIT(i)) {
			role_num++;
		}
	}
	_os_spinunlock(drv, &band_ctrl->lock, _ps, NULL);

	return role_num;
}

enum rtw_phl_status
phl_mr_sync_chandef(struct phl_info_t *phl_info, struct hw_band_ctl_t *band_ctrl,
			struct rtw_chan_ctx *chanctx, struct rtw_chan_def *chandef);

enum rtw_phl_status
phl_mr_chandef_chg(struct phl_info_t *phl,
		struct rtw_wifi_role_t *wrole, struct rtw_chan_def *new_chan,
		struct rtw_chan_def *chctx_result);

enum rtw_phl_status
phl_mr_chandef_upt(struct phl_info_t *phl_info,
		struct hw_band_ctl_t *band_ctrl, struct rtw_chan_ctx *chanctx);

enum rtw_phl_status
phl_mr_get_chandef(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wifi_role,
			bool sync, struct rtw_chan_def *chandef);

enum rtw_phl_status
phl_mr_ctrl_init(struct phl_info_t *phl_info);

enum rtw_phl_status
phl_mr_ctrl_deinit(struct phl_info_t *phl_info);

#ifdef CONFIG_MCC_SUPPORT
enum rtw_phl_status
phl_mr_mcc_query_role_time_slot_lim (struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole,
								struct phl_mcc_dur_lim_req_info *limit_req_info);


enum rtw_phl_status
phl_mr_coex_handle(struct phl_info_t *phl, struct rtw_wifi_role_t *cur_wrole,
	u16 slot, enum phl_band_idx band_idx, enum mr_coex_trigger trgger);

enum rtw_phl_status
phl_mr_coex_disable(struct phl_info_t *phl,
		struct rtw_wifi_role_t *cur_wrole, enum phl_band_idx band_idx,
		enum mr_coex_trigger trgger);

#endif

#ifdef CONFIG_PHL_P2PPS
bool phl_mr_noa_dur_lim_change (struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole,
								struct rtw_phl_noa_desc *noa_desc);
#endif
enum rtw_phl_status
phl_mr_info_upt(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole);

enum rtw_phl_status
phl_mr_state_upt(struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole);

enum rtw_phl_status
rtw_phl_mr_rx_filter(void *phl, struct rtw_wifi_role_t *wrole);

enum rtw_phl_status
phl_mr_tsf_sync(void *phl, struct rtw_wifi_role_t *wrole,
					enum role_state state);

void phl_mr_stop_all_beacon(struct phl_info_t *phl_info,
			    struct rtw_wifi_role_t *wrole,
			    bool stop);

enum rtw_phl_status
phl_mr_offch_hdl(struct phl_info_t *phl_info,
		  struct rtw_wifi_role_t *wrole,
		  bool off_ch,
		  void *obj_priv,
		  u8 (*issue_null_data)(void *priv, u8 ridx, bool ps));

enum rtw_phl_status phl_mr_watchdog(struct phl_info_t *phl_info);

u8 rtw_phl_mr_get_opch_list(void *phl, struct rtw_wifi_role_t *wifi_role,
			    struct rtw_chan_def *chdef_list, u8 list_size);

void
phl_mr_check_ecsa(struct phl_info_t *phl_info,
		  struct rtw_wifi_role_t *wrole);

void
phl_mr_check_ecsa_cancel(struct phl_info_t *phl_info,
			 struct rtw_wifi_role_t *wrole);

#ifdef CONFIG_MCC_SUPPORT
u8 phl_mr_query_mcc_inprogress (struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole,
				enum rtw_phl_mcc_chk_inprocess_type check_type);
#endif

#ifdef DBG_PHL_MR
enum rtw_phl_status phl_mr_info_dbg(struct phl_info_t *phl_info);

void phl_mr_dump_cur_chandef(const char *caller, const int line, bool show_caller,
		struct phl_info_t *phl_info, struct rtw_wifi_role_t *wifi_role);
#define PHL_DUMP_CUR_CHANDEF(_phl_info, _wrole) \
	phl_mr_dump_cur_chandef(__FUNCTION__, __LINE__, false, _phl_info, _wrole);
#define PHL_DUMP_CUR_CHANDEF_EX(_phl_info, _wrole) \
	phl_mr_dump_cur_chandef(__FUNCTION__, __LINE__, true, _phl_info, _wrole);

void phl_mr_dump_chctx_info(const char *caller, const int line, bool show_caller,
	struct phl_info_t *phl_info, struct phl_queue *chan_ctx_queue, struct rtw_chan_ctx *chanctx);
#define PHL_DUMP_CHAN_CTX(_phl_info, _chctx_q, _chctx) \
		phl_mr_dump_chctx_info(__FUNCTION__, __LINE__, false, _phl_info, _chctx_q, _chctx);
#define PHL_DUMP_CHAN_CTX_EX(_phl_info, _chctx_q, _chctx) \
		phl_mr_dump_chctx_info(__FUNCTION__, __LINE__, true, _phl_info, _chctx_q, _chctx);

void phl_mr_dump_band_info(const char *caller, const int line, bool show_caller,
	struct phl_info_t *phl_info, struct hw_band_ctl_t *band_ctrl);
#define PHL_DUMP_BAND_CTL(_phl_info, band_ctl) \
		phl_mr_dump_band_info(__FUNCTION__, __LINE__, false, _phl_info, band_ctl)
#define PHL_DUMP_BAND_CTL_EX(_phl_info, band_ctl) \
		phl_mr_dump_band_info(__FUNCTION__, __LINE__, true, _phl_info, band_ctl)

void phl_mr_dump_role_info(const char *caller, const int line, bool show_caller,
	struct phl_info_t *phl_info, struct rtw_wifi_role_t *wrole);
#define PHL_DUMP_ROLE(_phl_info, _wrole) \
		phl_mr_dump_role_info(__FUNCTION__, __LINE__, false, _phl_info, _wrole)
#define PHL_DUMP_ROLE_EX(_phl_info, _wrole) \
		phl_mr_dump_role_info(__FUNCTION__, __LINE__, true, _phl_info, _wrole)

void phl_mr_dump_info(const char *caller, const int line, bool show_caller,
	struct phl_info_t *phl_info);
#define PHL_DUMP_MR(_phl_info) phl_mr_dump_info(__FUNCTION__, __LINE__, false, _phl_info)
#define PHL_DUMP_MR_EX(_phl_info) phl_mr_dump_info(__FUNCTION__, __LINE__, true, _phl_info)

#else
#define PHL_DUMP_CUR_CHANDEF(_phl_info, _wrole)
#define PHL_DUMP_CUR_CHANDEF_EX(_phl_info, _wrole)
#define PHL_DUMP_CHAN_CTX(_phl_info, _chctx_q, _chctx)
#define PHL_DUMP_CHAN_CTX_EX(_phl_info, _chctx_q, _chctx)
#define PHL_DUMP_BAND_CTL(_phl_info, band_ctl)
#define PHL_DUMP_BAND_CTL_EX(_phl_info, band_ctl)
#define PHL_DUMP_ROLE(_phl_info, _wrole)
#define PHL_DUMP_ROLE_EX(_phl_info, _wrole)
#define PHL_DUMP_MR(_phl_info)
#define PHL_DUMP_MR_EX(_phl_info)
#endif

#endif  /*_PHL_MR_H_*/

