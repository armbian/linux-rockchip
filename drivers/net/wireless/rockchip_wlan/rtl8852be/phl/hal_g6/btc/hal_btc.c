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
#define _HAL_BTC_C_
#include "../hal_headers_le.h"
#include "hal_btc.h"
#include "halbtc_fw.h"
#include "halbtc_def.h"
#include "halbtc_action.h"

#ifdef CONFIG_BTCOEX

/* [31:24] -> main-version
 * [23:16] -> sub-version
 * [15:8]  -> Hot-Fix-version
 * [7:0]   -> branch ID, ex: 0x00-> Main-Branch
 * Modify bt_8852x.c chip_8852x member: btcx_desired, wlcx_desired if required
 * btcx_desired: BT FW coex version -> main-version + 1 if update.
 * wlcx_desired: WL FW coex version -> sub-version + 1 if update
 */
const u32 coex_ver = 0x0601040f;

static struct btc_ops _btc_ops = {
	_send_fw_cmd,
	_ntfy_power_on,
	_ntfy_power_off,
	_ntfy_init_coex,
	_ntfy_scan_start,
	_ntfy_scan_finish,
	_ntfy_switch_band,
	_ntfy_specific_packet,
	_ntfy_role_info,
	_ntfy_radio_state,
	_ntfy_customerize,
	_ntfy_wl_rfk,
	_ntfy_wl_sta,
	_ntfy_fwinfo,
	_ntfy_timer
};

#define _update_dbcc_band(phy_idx) \
	btc->cx.wl.dbcc_info.real_band[phy_idx] =\
	(btc->cx.wl.scan_info.phy_map & BIT(phy_idx) ?\
	btc->cx.wl.dbcc_info.scan_band[phy_idx] :\
	btc->cx.wl.dbcc_info.op_band[phy_idx])

/******************************************************************************
 *
 * coex internal functions
 *
 *****************************************************************************/
static void _set_btc_timer(struct btc_t *btc, u16 tmr_id, u32 ms)
{
	struct btc_tmr *btmr = NULL;

	if (tmr_id < BTC_TIMER_MAX) {
		btmr = &btc->timer[tmr_id];
		_os_set_timer(halcom_to_drvpriv(btc->hal), &btmr->tmr, ms);
	}
}

static void _btmr_stop(struct btc_t *btc)
{
	struct btc_tmr *btmr = NULL;
	u8 i = 0;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): stop btc timers!!\n",
		  __func__);

	btc->tmr_stop = true;

	for (i = 0; i < BTC_TIMER_MAX; i++) {
		btmr = &btc->timer[i];
		_os_cancel_timer(halcom_to_drvpriv(btc->hal), &btmr->tmr);
	}
	_os_cancel_timer(halcom_to_drvpriv(btc->hal), &btc->delay_tmr);
}

static void _btmr_release(struct btc_t *btc)
{
	struct btc_tmr *btmr = NULL;
	u8 i = 0;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): release btc timers!!\n",
		  __func__);

	for (i = 0; i < BTC_TIMER_MAX; i++) {
		btmr = &btc->timer[i];
		_os_release_timer(halcom_to_drvpriv(btc->hal), &btmr->tmr);
	}
	_os_release_timer(halcom_to_drvpriv(btc->hal), &btc->delay_tmr);
}

static void _btmr_start(struct btc_t *btc)
{
	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): start btc periodic timer!!\n",
		  __func__);

	btc->tmr_stop = false;

	/* Wait 2 sec for phl starting then periodic timer will be started */
	_os_set_timer(halcom_to_drvpriv(btc->hal),
			&btc->delay_tmr, BTC_DELAYED_PERIODIC_TIME);
}

static void _delay_tmr_cb(void *ctx)
{
	struct btc_t *btc = NULL;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s() !!\n", __func__);

	if (!ctx)
		return;

	btc = (struct btc_t *)ctx;

	_set_btc_timer(btc, BTC_TIMER_PERIODIC, BTC_PERIODIC_TIME);
}

static void _btmr_cb(void *ctx)
{
	struct btc_tmr *btmr = NULL;
	struct btc_t *btc = NULL;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(), ctx = 0x%p !!\n",
		  __func__, ctx);

	if (!ctx)
		return;

	btmr = (struct btc_tmr *)ctx;
	btc = (struct btc_t *)btmr->btc;

	if (!btc->tmr_init || btc->tmr_stop)
		return;

	hal_btc_send_event(btc, (u8 *)btmr, sizeof(struct btc_tmr *),
			   BTC_HMSG_TMR_EN);
}

static void _btmr_init(struct btc_t *btc)
{
	struct btc_tmr *btmr = NULL;
	u8 i = 0;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): !!\n", __func__);

	for (i = 0; i < BTC_TIMER_MAX; i++) {
		btmr = &btc->timer[i];
		btmr->btc = btc;
		btmr->id = i;
		PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
			  "[BTC], init btc timer(%d) = 0x%p !!\n", i, btmr);
		_os_init_timer(halcom_to_drvpriv(btc->hal), &btmr->tmr,
			       _btmr_cb, btmr, "btc_tmr");
	}

	_os_init_timer(halcom_to_drvpriv(btc->hal), &btc->delay_tmr,
			       _delay_tmr_cb, btc, NULL);

	btc->tmr_init = true;
	btc->tmr_stop = true;
}

static void _btmr_deinit(struct btc_t *btc)
{
	if (btc->tmr_init) {
		_btmr_stop(btc);
		_btmr_release(btc);
		btc->tmr_init = false;
	}
}

static void
_send_fw_cmd(struct btc_t *btc, u8 h2c_class, u8 h2c_func, u8 *param, u16 len)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_wl_info *wl = &btc->cx.wl;
	struct rtw_g6_h2c_hdr hdr = {0};

	hdr.h2c_class = h2c_class;
	hdr.h2c_func = h2c_func;
	hdr.type = H2CB_TYPE_DATA;
	hdr.content_len = len;
	hdr.done_ack = 1;

	if (!wl->status.map.init_ok) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): return by btc not init!!\n",
			  __func__);
		btc->fwinfo.cnt_h2c_fail++;
		return;
	} else if ((wl->status.map.rf_off_pre == 1 &&
		   wl->status.map.rf_off == 1) ||
		   (wl->status.map.lps_pre == 1 &&
		   wl->status.map.lps == 1)) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): return by wl off!!\n",
			  __func__);
		btc->fwinfo.cnt_h2c_fail++;
		return;
	}

	btc->fwinfo.cnt_h2c++;

	if (rtw_hal_mac_send_h2c(h, &hdr, (u32 *)param) != 0)
		btc->fwinfo.cnt_h2c_fail++;
}

u32 _read_cx_reg(struct btc_t *btc, u32 offset)
{
	u32 val = 0;
	rtw_hal_mac_coex_reg_read(btc->hal, offset, &val);
	return val;
}

u8 _read_cx_ctrl(struct btc_t *btc)
{
	u32 val = 0;
	rtw_hal_mac_get_coex_ctrl(btc->hal, &val);

	return ((u8)val);
}

u32 _read_scbd(struct btc_t *btc)
{
	const struct btc_chip *chip = btc->chip;
	u32 scbd_val = 0;

	if (!chip->scbd)
		return 0;

	rtw_hal_mac_get_scoreboard(btc->hal, &scbd_val);

	PHL_INFO("[BTC], read scbd : 0x%08x \n", scbd_val);
	btc->cx.cnt_bt[BTC_BCNT_SCBDREAD]++;
	return (scbd_val);
}

void _write_scbd(struct btc_t *btc, u32 val, bool state)
{
	const struct btc_chip *chip = btc->chip;
	struct btc_wl_info *wl = &btc->cx.wl;
	u32 scbd_val = 0;

	if (!chip->scbd)
		return;

	/* only use bit23~0 */
	scbd_val = (state ? (wl->scbd | val) : (wl->scbd & (~val)));

	if (scbd_val != wl->scbd) {
		rtw_hal_mac_set_scoreboard(btc->hal, &scbd_val);
		PHL_INFO("[BTC], write scbd : 0x%08x \n", scbd_val);
		wl->scbd = scbd_val;

		btc->cx.cnt_wl[BTC_WCNT_SCBDUPDATE]++;
		/* Add delay to avoid BT FW loss information */
		_os_delay_us(btc->hal, BTC_SCBD_REWRITE_DELAY);
	}
}

static u8
_update_rssi_state(struct btc_t *btc, u8 pre_state, u8 rssi, u8 thresh)
{
	u8 next_state, tol = btc->chip->rssi_tol;

	if (pre_state == BTC_RSSI_ST_LOW ||
	    pre_state == BTC_RSSI_ST_STAY_LOW) {
		if (rssi >= (thresh + tol))
			next_state = BTC_RSSI_ST_HIGH;
		else
			next_state = BTC_RSSI_ST_STAY_LOW;
	} else {
		if (rssi < thresh)
			next_state = BTC_RSSI_ST_LOW;
		else
			next_state = BTC_RSSI_ST_STAY_HIGH;
	}

	return next_state;
}

void _write_bt_reg(struct btc_t *btc, u8 reg_type, u16 addr, u32 val)
{
	u8 buf[4] = {0};

	/* set write address */
	buf[0] = reg_type;
	buf[1] = addr & bMASKB0;
	buf[2] = (addr & bMASKB1) >> 8;
	hal_btc_fw_set_bt(btc, SET_BT_WREG_ADDR, 3, buf);

	/* set write value */
	buf[0] = val & bMASKB0;
	buf[1] = (val & bMASKB1) >> 8;
	buf[2] = (val & bMASKB2) >> 16;
	buf[3] = (val & bMASKB3) >> 23;
	hal_btc_fw_set_bt(btc, SET_BT_WREG_VAL, 4, buf);
}

void _read_bt_reg(struct btc_t *btc, u8 reg_type, u16 addr)
{
	/* this function is only for API call.
	 * If BTC should use hal_btc_fw_set_monreg to read bt reg.
	 */
	u8 buf[3] = {0};

	/* set write address */
	buf[0] = reg_type;
	buf[1] = addr & bMASKB0;
	buf[2] = (addr & bMASKB1) >> 8;
	hal_btc_fw_set_bt(btc, SET_BT_RREG_ADDR, sizeof(buf), buf);

	/* To do wait FW event -> BTF_EVNT_BT_REG*/
}

void _set_bt_psd_report(struct btc_t *btc, u8 start_idx, u8 rpt_type)
{
	u8 buf[2] = {0};

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): set bt psd\n",
		  __func__);

	buf[0] = start_idx;
	buf[1] = rpt_type;
	hal_btc_fw_set_bt(btc, SET_BT_PSD_REPORT, 2, buf);
}

static void _set_bt_info_report(struct btc_t *btc, u8 trigger)
{
	u8 buf = 0;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): query bt info\n",
		  __func__);

	buf = trigger;
	hal_btc_fw_set_bt(btc, SET_BT_INFO_REPORT, 1, &buf);
}

static void _reset_btc_var(struct btc_t *btc, u8 type)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_cx *cx = &btc->cx;
	struct btc_wl_info *wl = &cx->wl;
	struct btc_bt_info *bt = &cx->bt;
	struct btc_bt_link_info *bt_linfo = &bt->link_info;
	struct btc_wl_link_info *wl_linfo = wl->link_info;
	u8 i;

	PHL_INFO("[BTC], %s()\n", __func__);

	/* Reset Coex variable */
	if (type & BTC_RESET_CX)
		hal_mem_set(h, cx, 0, sizeof(struct btc_cx));
	else if (type & BTC_RESET_BTINFO) /* only for BT enable */
		hal_mem_set(h, bt, 0, sizeof(struct btc_bt_info));

	if (type & BTC_RESET_CTRL)
		hal_mem_set(h, &btc->ctrl, 0, sizeof(struct btc_ctrl));

	/* Init Coex variables that are not zero */
	if (type & BTC_RESET_DM) {
		hal_mem_set(h, &btc->dm, 0, sizeof(struct btc_dm));
		hal_mem_set(h, bt_linfo->rssi_state, 0, BTC_BT_RSSI_THMAX);

		for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++)
			hal_mem_set(h, wl_linfo[i].rssi_state, 0,
				    BTC_WL_RSSI_THMAX);

		/* set the slot_now table to original */
		_tdma_cpy(&btc->dm.tdma_now, &t_def[CXTD_OFF]);
		_tdma_cpy(&btc->dm.tdma, &t_def[CXTD_OFF]);
		_slots_cpy(btc->dm.slot_now, s_def);
		_slots_cpy(btc->dm.slot, s_def);
		btc->policy_len = 0;
		btc->bt_req_len = 0;
		btc->hubmsg_cnt = 0;

		btc->dm.coex_info_map = BTC_COEX_INFO_ALL;
		btc->dm.wl_tx_limit.tx_time = BTC_MAX_TX_TIME_DEF;
		btc->dm.wl_tx_limit.tx_retry = BTC_MAX_TX_RETRY_DEF;
	}

	if (type & BTC_RESET_MDINFO)
		hal_mem_set(h, &btc->mdinfo, 0, sizeof(struct btc_module));
}

static bool _chk_wl_rfk_request(struct btc_t *btc)
{
	struct btc_cx *cx = &btc->cx;
	struct btc_bt_info *bt = &cx->bt;

	_update_bt_scbd(btc, true);

	cx->cnt_wl[BTC_WCNT_RFK_REQ]++;

	if ((bt->rfk_info.map.run || bt->rfk_info.map.req) &&
	    (!bt->rfk_info.map.timeout)) {
		cx->cnt_wl[BTC_WCNT_RFK_REJECT]++;
		return BTC_WRFK_REJECT;
	} else {
		cx->cnt_wl[BTC_WCNT_RFK_GO]++;
		return BTC_WRFK_ALLOW;
	}
}

void _set_init_info(struct btc_t *btc)
{
	struct btc_dm *dm = &btc->dm;
	struct btc_wl_info *wl = &btc->cx.wl;

	dm->init_info.wl_only = (u8)dm->wl_only;
	dm->init_info.bt_only = (u8)dm->bt_only;
	dm->init_info.wl_init_ok = (u8)wl->status.map.init_ok;
	dm->init_info.dbcc_en = btc->hal->dbcc_en;
	dm->init_info.cx_other = btc->cx.other.type;
	dm->init_info.wl_guard_ch = btc->chip->afh_guard_ch;
	dm->init_info.module = btc->mdinfo;
}

u8 _get_role_link_mode(struct btc_t *btc, u8 role)
{
	switch(role) {
	case PHL_RTYPE_STATION:
		return BTC_WLINK_2G_STA;
	case PHL_RTYPE_P2P_GO:
		return BTC_WLINK_2G_GO;
	case PHL_RTYPE_P2P_GC:
		return BTC_WLINK_2G_GC;
	case PHL_RTYPE_AP:
		return BTC_WLINK_2G_AP;
	default:
		return BTC_WLINK_OTHER;
	}
}

u8 _get_wl_role_idx(struct btc_t *btc, u8 role)
{
	struct btc_wl_role_info *wl_rinfo = &btc->cx.wl.role_info;
	u8 i, pid = 0;

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		if (wl_rinfo->active_role[i].role == role)
			break;
	}

	pid = i;
	return pid; /*cation: return MAX_WIFI_ROLE_NUMBER if role not found */
}

void _set_wl_req_mac(struct btc_t *btc, u8 mac_id)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_chip_ops *ops = btc->chip->ops;

	if (mac_id == wl->pta_req_mac)
		return;

	if (ops && ops->wl_req_mac)
		ops->wl_req_mac(btc, mac_id);

	wl->pta_req_mac = mac_id;
}

static bool _chk_role_ch_group(struct btc_t *btc, struct rtw_chan_def r1, struct rtw_chan_def r2)
{
	bool is_grouped = false;

	if (r1.chan != r2.chan) {
		/* primary ch is different */
		goto exit;
	} else if (r1.bw == CHANNEL_WIDTH_40 && r2.bw == CHANNEL_WIDTH_40) {
		if (r1.offset != r2.offset)
			goto exit;
	}

	is_grouped = true;

exit:
	return is_grouped;
}

static void _update_wl_info(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_wl_link_info *wl_linfo = wl->link_info;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	struct btc_wl_dbcc_info *wl_dinfo = &wl->dbcc_info;
	struct btc_wl_active_role *active_role = NULL;
	struct rtw_hal_com_t *h = btc->hal;
	struct rtw_chan_def cid_ch[MAX_WIFI_ROLE_NUMBER];
	u8 i, j, k, cnt_connect = 0, cnt_connecting = 0, cnt_active = 0;
	u8 cnt_2g = 0, cnt_5g = 0, max_role_cnt = BTC_TDMA_WLROLE_MAX;
	u8 cid_phy[MAX_WIFI_ROLE_NUMBER] = {0};
	u8 cid_role[MAX_WIFI_ROLE_NUMBER] = {0};
	u8 dbcc_2g_phy = 0, dbcc_2g_cid = 0, dbcc_2g_role = 0;
	u32 noa_duration = 0;
	bool b2g = false, b5g = false, client_joined = false, noa_exist = false;

	hal_mem_set(h, wl_rinfo, 0, sizeof(struct btc_wl_role_info));
	hal_mem_set(h, wl_dinfo, 0, sizeof(struct btc_wl_dbcc_info));
	hal_mem_set(h, cid_ch, 0, MAX_WIFI_ROLE_NUMBER * sizeof(struct rtw_chan_def));

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		/* check if role active? */
		if (!wl_linfo[i].active || wl_linfo[i].phy >= HW_PHY_MAX)
			continue;

		cnt_active++;
		active_role = &wl_rinfo->active_role[cnt_active-1];
		active_role->role = wl_linfo[i].role;
		active_role->pid = wl_linfo[i].pid;
		active_role->phy = wl_linfo[i].phy;
		active_role->band = wl_linfo[i].chdef.band;
		active_role->noa = (u8)wl_linfo[i].noa;
		active_role->noa_duration = wl_linfo[i].noa_duration/1000;

		/* check if role connect? */
		if (wl_linfo[i].connected == MLME_NO_LINK) {
			wl_rinfo->active_role[cnt_active-1].connected = 0;
			continue;
		} else if (wl_linfo[i].connected == MLME_LINKING) {
			cnt_connecting++;
		} else {
			cnt_connect++;
			/* only if client connect for p2p-Go/AP */
			if ((wl_linfo[i].role == PHL_RTYPE_P2P_GO ||
			     wl_linfo[i].role == PHL_RTYPE_AP) &&
			     wl_linfo[i].client_cnt > 1)
				client_joined = true;

			cid_phy[cnt_connect-1] = wl_linfo[i].phy;
			hal_mem_cpy(h, &cid_ch[cnt_connect-1],
				    &wl_linfo[i].chdef, sizeof(struct rtw_chan_def));

			cid_role[cnt_connect-1] = wl_linfo[i].role;
		}

		wl_rinfo->role_map.val |= BIT(wl_linfo[i].role);
		active_role->ch = wl_linfo[i].chdef.center_ch;
		active_role->bw = wl_linfo[i].chdef.bw;
		active_role->connected = 1;

		/* only one noa-role exist */
		if (active_role->noa && active_role->noa_duration > 0) {
			noa_exist = true;
			noa_duration = active_role->noa_duration;
		}

		/* Check dbcc role,the last role may overwrite the former role,
		 * This will be modified in the below code.
		 */
		if (h->dbcc_en) {
			wl_dinfo->role[wl_linfo[i].phy] = wl_linfo[i].role;
			wl_dinfo->op_band[wl_linfo[i].phy] = wl_linfo[i].chdef.band;
			max_role_cnt = BTC_TDMA_WLROLE_MAX + 1;
		}

		if (wl_linfo[i].chdef.band != BAND_ON_24G) {
			cnt_5g++;
			b5g = true;
		} else {
			cnt_2g++;
			b2g = true;
		}
	}

	wl_rinfo->connect_cnt = cnt_connect;

	/* Be careful to change the following sequence!! */
	if (cnt_connect == 0) {
		wl_rinfo->link_mode = BTC_WLINK_NOLINK;
		wl_rinfo->role_map.role.none = 1;
	} else if (!b2g && b5g) {
		wl_rinfo->link_mode = BTC_WLINK_5G;
	} else if (wl_rinfo->role_map.role.nan) {
		wl_rinfo->link_mode = BTC_WLINK_2G_NAN;
	} else if (cnt_connect > max_role_cnt) {
		wl_rinfo->link_mode = BTC_WLINK_OTHER;
	} else if (h->dbcc_en) {
		/* find out the 2G-PHY by connect-id ->ch  */
		for (j = 0; j < cnt_connect; j++) {
			if (cid_ch[j].center_ch <= 14 || j == cnt_connect-1)
				break;
		}

		dbcc_2g_cid = j;
		dbcc_2g_phy = cid_phy[j];
		dbcc_2g_role = cid_role[j];

		/* find out the other role in the 2.4G_band-PHY*/
		if (cnt_connect == 3) {
			for (k = 0; k < cnt_connect; k++) {
				if (k == j)
					continue;
				else if (cid_phy[k] == dbcc_2g_phy)
					break;
			}

			if (k < cnt_connect) { /* if find -> DBCC + MCC/SCC */
				if (cid_ch[k].center_ch > 14)
					wl_rinfo->link_mode = BTC_WLINK_25G_MCC;
				else if (_chk_role_ch_group(btc, cid_ch[j], cid_ch[k]))
					wl_rinfo->link_mode = BTC_WLINK_2G_SCC;
				else
					wl_rinfo->link_mode = BTC_WLINK_2G_MCC;

				/* correct 2G-located PHY role/band
				 * for gnt ctrl.
				 */
				wl_dinfo->role[dbcc_2g_phy] = dbcc_2g_role;
				wl_dinfo->op_band[dbcc_2g_phy] = BAND_ON_24G;
				goto exit;
			}
		}

		wl_rinfo->link_mode = _get_role_link_mode(btc, cid_role[j]);
	} else if (b2g && b5g && cnt_connect == 2) {
		wl_rinfo->link_mode = BTC_WLINK_25G_MCC;
	} else if (!b5g && cnt_connect == 2) { /* cnt_connect = 2 */
		if (_chk_role_ch_group(btc, cid_ch[0], cid_ch[cnt_connect - 1]))
			wl_rinfo->link_mode = BTC_WLINK_2G_SCC;
		else
			wl_rinfo->link_mode= BTC_WLINK_2G_MCC;
	} else if (!b5g && cnt_connect == 1) { /* cnt_connect = 1 */
		wl_rinfo->link_mode = _get_role_link_mode(btc, cid_role[0]);
	}

	/* if no client_joined, don't care P2P-GO/AP role */
	if ((wl_rinfo->role_map.role.p2p_go || wl_rinfo->role_map.role.ap) &&
	     !client_joined) {
#if 0 /* work-around for Go/AP(no-client) can not move channel to same as STA*/
		if (wl_rinfo->link_mode == BTC_WLINK_2G_SCC ||
		    wl_rinfo->link_mode == BTC_WLINK_2G_MCC)
#else
		if (wl_rinfo->link_mode == BTC_WLINK_2G_SCC)
#endif

		{
			wl_rinfo->link_mode = BTC_WLINK_2G_STA;
			wl_rinfo->connect_cnt--;
		} else if (wl_rinfo->link_mode == BTC_WLINK_2G_GO ||
			wl_rinfo->link_mode == BTC_WLINK_2G_AP) {
			wl_rinfo->link_mode = BTC_WLINK_NOLINK;
			wl_rinfo->connect_cnt--;
		}
	}

exit:
	/* Identify 2-Role type  */
	if (wl_rinfo->link_mode == BTC_WLINK_2G_SCC ||
	    wl_rinfo->link_mode == BTC_WLINK_2G_MCC ||
	    wl_rinfo->link_mode == BTC_WLINK_25G_MCC ||
	    (wl_rinfo->link_mode == BTC_WLINK_5G && cnt_connect > 1)) {
		if (wl_rinfo->role_map.role.p2p_go ||
		    wl_rinfo->role_map.role.ap) {
			if (noa_exist)
				wl_rinfo->mrole_type = BTC_WLMROLE_STA_GO_NOA;
			else
				wl_rinfo->mrole_type = BTC_WLMROLE_STA_GO;
		} else if (wl_rinfo->role_map.role.p2p_gc) {
			if (noa_exist)
				wl_rinfo->mrole_type = BTC_WLMROLE_STA_GC_NOA;
			else
				wl_rinfo->mrole_type = BTC_WLMROLE_STA_GC;
		} else {
			wl_rinfo->mrole_type = BTC_WLMROLE_STA_STA;
		}

		wl_rinfo->mrole_noa_duration = noa_duration;
	} else {
		wl_rinfo->mrole_type = BTC_WLMROLE_NONE;
		wl_rinfo->mrole_noa_duration = 0;
	}

	hal_btc_fw_set_drv_info(btc, CXDRVINFO_ROLE);

	if (h->dbcc_en) {
		_set_wl_req_mac(btc, dbcc_2g_phy);
		_update_dbcc_band(HW_PHY_0);
		_update_dbcc_band(HW_PHY_1);
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_DBCC);
	} else {
		_set_wl_req_mac(btc, HW_PHY_0);
	}
}

void _run_coex(struct btc_t *btc, const char *reason)
{
	struct btc_dm *dm = &btc->dm;
	struct btc_cx *cx = &btc->cx;
	struct btc_wl_info *wl = &cx->wl;
	struct btc_bt_info *bt = &cx->bt;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	u8 mode = wl_rinfo->link_mode;

	PHL_INFO("[BTC], %s(): reason = %s, mode=%d\n", __func__, reason, mode);
	_rsn_cpy(dm->run_reason, (char*)reason);
	_update_dm_step(btc, reason);

#if BTC_CX_FW_OFFLOAD
	if (wl->rfk_info.state != BTC_WRFK_STOP && mode != BTC_WLINK_5G) {
		_action_wl_rfk(btc);
	} else  if (wl->rfk_info.state == BTC_WRFK_STOP) {
		PHL_INFO("[BTC], %s(): offload to WL_FW\n", __func__);
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_SMAP);
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_RUN);
	}

	goto exit;
#else

	_update_btc_state_map(btc);
	_get_wl_nhm_dbm(btc);

	/* Be careful to change the following function sequence!! */
	if (btc->ctrl.manual) {
		PHL_INFO("[BTC], %s(): return for Manual CTRL!!\n", __func__);
		return;
	}

	if (btc->ctrl.igno_bt &&
	    (run_rsn("_update_bt_info") || run_rsn("_update_bt_scbd"))) {
		PHL_INFO("[BTC], %s(): return for Stop Coex DM!!\n", __func__);
		return;
	}

	if (!wl->status.map.init_ok) {
		PHL_INFO("[BTC], %s(): return for WL init fail!!\n", __func__);
		return;
	}

	if (wl->status.map.rf_off_pre == wl->status.map.rf_off &&
	    wl->status.map.lps_pre == wl->status.map.lps) {
	    	if (run_rsn("_ntfy_power_off") ||
		    run_rsn("_ntfy_radio_state")) {
			PHL_INFO("[BTC], %s(): return for WL rf off state no change!!\n",
			 __func__);
			return;
	    	}

		if (wl->status.map.rf_off == 1 ||
		    wl->status.map.lps == BTC_LPS_RF_OFF) {
			PHL_INFO("[BTC], %s(): return for WL rf off state!!\n",
				 __func__);
			return;
		}
	}

	dm->cnt_dm[BTC_DCNT_RUN]++;
	dm->freerun = false;
	btc->ctrl.igno_bt = false;
	bt->scan_rx_low_pri = false;

	if (btc->ctrl.always_freerun) {
		_action_freerun(btc);
		btc->ctrl.igno_bt = true;
		goto exit;
	}

	if (dm->wl_only) {
		_action_wl_only(btc);
		btc->ctrl.igno_bt = true;
		goto exit;
	}

	if (wl->status.map.rf_off || wl->status.map.lps || dm->bt_only) {
		_action_wl_off(btc);
		btc->ctrl.igno_bt = true;
		goto exit;
	}

	if (run_rsn("_ntfy_init_coex")) {
		_action_wl_init(btc);
		goto exit;
	}

	if (!cx->bt.enable.now && !cx->other.type) {
		_action_bt_off(btc);
		goto exit;
	}

	if (cx->bt.whql_test) {
		_action_bt_whql(btc);
		goto exit;
	}

	if (wl->rfk_info.state != BTC_WRFK_STOP) {
		_action_wl_rfk(btc);
		goto exit;
	}

	if (cx->state_map == BTC_WLINKING &&
	    (mode == BTC_WLINK_NOLINK || mode == BTC_WLINK_2G_STA ||
	     mode == BTC_WLINK_5G)) {
		_action_wl_scan(btc);
		goto exit;
	}

	if (wl->status.map.scan) {
		_action_wl_scan(btc);
		goto exit;
	}

	switch (mode) {
	case BTC_WLINK_NOLINK:
		_action_wl_nc(btc);
		break;
	case BTC_WLINK_2G_STA:
		_action_wl_2g_sta(btc);
		break;
	case BTC_WLINK_2G_AP:
		bt->scan_rx_low_pri = true;
		_action_wl_2g_ap(btc);
		break;
	case BTC_WLINK_2G_GO:
		bt->scan_rx_low_pri = true;
		_action_wl_2g_go(btc);
		break;
	case BTC_WLINK_2G_GC:
		bt->scan_rx_low_pri = true;
		_action_wl_2g_gc(btc);
		break;
	case BTC_WLINK_2G_SCC:
		bt->scan_rx_low_pri = true;
		_action_wl_2g_scc(btc);
		break;
	case BTC_WLINK_2G_MCC:
		bt->scan_rx_low_pri = true;
		_action_wl_2g_mcc(btc);
		break;
	case BTC_WLINK_25G_MCC:
		bt->scan_rx_low_pri = true;
		_action_wl_25g_mcc(btc);
		break;
	case BTC_WLINK_5G:
		_action_wl_5g(btc);
		break;
	case BTC_WLINK_2G_NAN:
		_action_wl_2g_nan(btc);
		break;
	default:
		_action_wl_other(btc);
		break;
	}
#endif
exit:
	_action_common(btc);
}

static void _update_offload_runinfo(struct btc_t *btc, u8 *buf, u32 len)
{
#if BTC_CX_FW_OFFLOAD
	u32 val;
	struct btc_cxr_result *r = NULL;

	if (!buf || buf[0] >= BTC_CXR_MAX)
		return;

	switch(buf[0]) {
	case BTC_CXR_WSCBD:
		val = (buf[4] << 24) + (buf[3] << 16) + (buf[2] << 8) + buf[1];
		if (val & BIT(31)) /* if val & BIT(31) --> write scbd bit false */
			_write_scbd(btc, val & 0x7fffffff, false);
		else
			_write_scbd(btc, val, true);
		break;
	case BTC_CXR_RESULT:
		r = (struct btc_cxr_result*) &buf[1];

		btc->dm.freerun = r->dm.freerun;
		btc->dm.wl_ps_ctrl = r->dm.wl_ps_ctrl;
		btc->dm.leak_ap = r->dm.leak_ap;
		btc->ctrl.igno_bt = (u16)r->dm.igno_bt;
		btc->dm.noisy_level = r->dm.noisy_level;
		btc->dm.set_ant_path = r->dm.set_ant_path;

		btc->dm.rf_trx_para = r->rf_trx_para;
		btc->cx.state_map = r->cx_state_map;
		btc->policy_type = (u16)r->policy_type;
		btc->dm.cnt_dm[BTC_DCNT_RUN] = r->run_cnt;

		hal_mem_cpy(btc->hal, btc->dm.run_reason, r->run_reason,
			    BTC_RSN_MAXLEN);
		hal_mem_cpy(btc->hal, btc->dm.run_action, r->run_action,
			    BTC_ACT_MAXLEN);
		break;
	}
#endif
}

void _update_bt_scbd(struct btc_t *btc, bool only_update)
{
	struct btc_cx *cx = &btc->cx;
	struct btc_bt_info *bt = &cx->bt;
	u32 val;
	bool status_change = false, bt_link_change = false;

	if (!btc->chip->scbd)
		return;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s()\n", __func__);

	val = _read_scbd(btc);

	if (val == 0xffffffff) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s return by invalid scbd value\n",
			  __func__);
		return;
	}

	if (!(val & BTC_BSCB_ON) ||
	    btc->dm.cnt_dm[BTC_DCNT_BTCNT_FREEZE] >= BTC_CHK_HANG_MAX)
		bt->enable.now = 0;
	else
		bt->enable.now = 1;

	if (bt->enable.now != bt->enable.last) {
		status_change = true;
		bt_link_change = true;
	}

	/* reset bt info if bt re-enable */
	if (bt->enable.now && !bt->enable.last) {
		_reset_btc_var(btc, BTC_RESET_BTINFO);
		cx->cnt_bt[BTC_BCNT_REENABLE]++;
		bt->enable.now = 1;
	}

	bt->enable.last = bt->enable.now;

	bt->scbd = val;
	bt->mbx_avl = !!(val & BTC_BSCB_ACT);

        if (bt->whql_test != (u32)(!!(val & BTC_BSCB_WHQL)))
		status_change = true;

	bt->whql_test = !!(val & BTC_BSCB_WHQL);

	bt->btg_type = (val & BTC_BSCB_BT_S1 ? BTC_BT_BTG: BTC_BT_ALONE);

	bt->link_info.a2dp_desc.active = !!(val & BTC_BSCB_A2DP_ACT);

	/* if rfk run 1->0 */
	if (bt->rfk_info.map.run && !(val & BTC_BSCB_RFK_RUN))
		status_change = true;

	bt->rfk_info.map.run  = !!(val & BTC_BSCB_RFK_RUN);

	bt->rfk_info.map.req = !!(val & BTC_BSCB_RFK_REQ);

	bt->hi_lna_rx = !!(val & BTC_BSCB_BT_HILNA);

	/* if connect change */
	if ((bt->link_info.status.map.connect && (!(val & BTC_BSCB_BT_CONNECT))) ||
	    (!bt->link_info.status.map.connect && (val & BTC_BSCB_BT_CONNECT))) {
		status_change = true;
		bt_link_change = true;
	}

	bt->link_info.status.map.connect = !!(val & BTC_BSCB_BT_CONNECT);

	bt->run_patch_code = !!(val & BTC_BSCB_PATCH_CODE);

#if !BTC_CX_FW_OFFLOAD
	if (bt_link_change) {
		PHL_INFO("[BTC], %s: bt status change!!\n", __func__);
		hal_btc_send_event(btc, NULL, 0, BTC_HMSG_BT_LINK_CHG);
	}

	if (!only_update && status_change)
		_run_coex(btc, __func__);
#endif
}

/******************************************************************************
 *
 * coexistence operations for external notifications
 *
 *****************************************************************************/
static void _ntfy_power_on(struct btc_t *btc)
{
	/* no action for power on, beacuse power-on move halmac API
	* the _ntfy_power_on = _ntfy_init_coex
	*/
}

static void _ntfy_power_off(struct btc_t *btc)
{
	PHL_INFO("[BTC], %s()\n", __func__);
	btc->dm.cnt_notify[BTC_NCNT_POWER_OFF]++;

	btc->cx.wl.status.map.rf_off = 1;
	btc->cx.wl.status.map.busy = 0;
	_write_scbd(btc, BTC_WSCB_ALL, false);
	_run_coex(btc, __func__);

	hal_btc_fw_en_rpt(btc, RPT_EN_ALL, 0);

	btc->cx.wl.status.map.rf_off_pre = btc->cx.wl.status.map.rf_off;
}

static void _ntfy_init_coex(struct btc_t *btc, u8 mode)
{
	struct btc_dm *dm = &btc->dm;
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_chip_ops *ops = btc->chip->ops;

	PHL_INFO("[BTC], %s(): mode=%d\n", __func__, mode);
	dm->cnt_notify[BTC_NCNT_INIT_COEX]++;
	dm->wl_only = (mode == BTC_MODE_WL? 1 : 0);
	dm->bt_only = (mode == BTC_MODE_BT? 1 : 0);
	wl->status.map.rf_off = (mode == BTC_MODE_WLOFF? 1 : 0);

	if (!wl->status.map.init_ok) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): return for WL init fail!!\n",
			  __func__);
		dm->error.map.init = true;
		return;
	}

	/* Setup RF front end type from EFuse RFE type*/
	if (ops && ops->set_rfe)
		ops->set_rfe(btc);

	if (ops && ops->init_cfg)
		ops->init_cfg(btc);

	_write_scbd(btc, BTC_WSCB_ACTIVE | BTC_WSCB_ON | BTC_WSCB_BTLOG, true);
	_update_bt_scbd(btc, true);

	/* check PTA control owner to avoid BT coex issue */
	if (_read_cx_ctrl(btc) == BTC_CTRL_BY_WL) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): PTA owner warning!!\n",
			  __func__);
		dm->error.map.pta_owner = true;
	}

	_set_init_info(btc);
	_set_wl_tx_power(btc, BTC_WL_DEF_TX_PWR); /* original tx power, no Tx power adjust */
	hal_btc_fw_set_slots(btc, CXST_MAX, dm->slot);
	hal_btc_fw_set_monreg(btc);
	hal_btc_fw_set_drv_info(btc, CXDRVINFO_INIT);
	hal_btc_fw_set_drv_info(btc, CXDRVINFO_CTRL);

	_run_coex(btc, __func__);
	_btmr_start(btc);

#if 0
	hal_btc_fw_set_gpio_dbg(btc, CXDGPIO_EN_MAP,
				BIT(BTC_DBG_GNT_WL) | BIT(BTC_DBG_GNT_BT));
#endif
}

static void _ntfy_scan_start(struct btc_t *btc, u8 phy_idx, u8 band)
{
	struct btc_wl_info *wl = &btc->cx.wl;

	PHL_INFO("[BTC], %s(): phy_idx=%d, band=%d\n", __func__, phy_idx, band);
	btc->dm.cnt_notify[BTC_NCNT_SCAN_START]++;

	wl->status.map.scan = true;
	wl->scan_info.band[phy_idx] = band;
	wl->scan_info.phy_map |= BIT(phy_idx);
	hal_btc_fw_set_drv_info(btc, CXDRVINFO_SCAN);

	if (btc->hal->dbcc_en) {
		wl->dbcc_info.scan_band[phy_idx] = band;
		_update_dbcc_band(phy_idx);
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_DBCC);
	}

	_run_coex(btc, __func__);
}

static void _ntfy_scan_finish(struct btc_t *btc, u8 phy_idx)
{
	struct btc_wl_info *wl = &btc->cx.wl;

	PHL_INFO("[BTC], %s(): phy_idx=%d\n", __func__, phy_idx);
	btc->dm.cnt_notify[BTC_NCNT_SCAN_FINISH]++;

	wl->status.map.scan = false;
	wl->scan_info.phy_map &= ~BIT(phy_idx);
	hal_btc_fw_set_drv_info(btc, CXDRVINFO_SCAN);

	if (btc->hal->dbcc_en) {
		_update_dbcc_band(phy_idx);
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_DBCC);
	}

	_run_coex(btc, __func__);
}

static void _ntfy_switch_band(struct btc_t *btc, u8 phy_idx, u8 band)
{
	struct btc_wl_info *wl = &btc->cx.wl;

	PHL_INFO("[BTC], %s(): phy_idx=%d, band=%d\n", __func__, phy_idx, band);
	btc->dm.cnt_notify[BTC_NCNT_SWITCH_BAND]++;

	wl->scan_info.band[phy_idx] = band;
	wl->scan_info.phy_map |= BIT(phy_idx);
	hal_btc_fw_set_drv_info(btc, CXDRVINFO_SCAN);

	if (btc->hal->dbcc_en) {
		wl->dbcc_info.scan_band[phy_idx] = band;
		_update_dbcc_band(phy_idx);
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_DBCC);
	}

	_run_coex(btc, __func__);
}

static void _ntfy_specific_packet(struct btc_t *btc, u8 pkt_type)
{
	struct btc_cx *cx = &btc->cx;
	struct btc_wl_info *wl = &cx->wl;
	struct btc_bt_link_info *b = &cx->bt.link_info;
	struct btc_bt_hfp_desc *hfp = &b->hfp_desc;
	struct btc_bt_hid_desc *hid = &b->hid_desc;
	u32 cnt, delay = BTC_SPECPKT_MAXT;

	switch (pkt_type) {
	case BTC_PKT_EVT_DHCP:
		cnt = ++cx->cnt_wl[BTC_WCNT_DHCP];
		PHL_INFO("[BTC], %s(): pkt_type=%d, DHCP cnt=%d \n", __func__,
			 pkt_type, cnt);
		wl->status.map.connecting = true;
		_set_btc_timer(btc, BTC_TIMER_WL_SPECPKT, delay);
		break;
	case BTC_PKT_EVT_EAPOL:
		cnt = ++cx->cnt_wl[BTC_WCNT_EAPOL];
		PHL_INFO("[BTC], %s(): pkt_type=%d, EAPOL cnt=%d \n", __func__,
			 pkt_type, cnt);
		wl->status.map._4way = true;

		if (hfp->exist || hid->exist)
			delay = delay / 2;

		_set_btc_timer(btc, BTC_TIMER_WL_SPECPKT, delay);
		break;
	case BTC_PKT_EVT_ADD_KEY:
		cnt = ++cx->cnt_wl[BTC_WCNT_EAPOL];
		PHL_INFO("[BTC], %s(): pkt_type=%d, EAPOL_End cnt=%d \n", __func__,
			 pkt_type, cnt);
		wl->status.map._4way = false;
		break;
	default:
	case BTC_PKT_EVT_ARP:
		cnt = ++cx->cnt_wl[BTC_WCNT_ARP];
		PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): pkt_type=%d, ARP cnt=%d\n",
			  __func__, pkt_type, cnt);
		return;
	}

	btc->dm.cnt_notify[BTC_NCNT_SPECIAL_PACKET]++;

	_run_coex(btc, __func__);
}

static void _update_bt_psd(struct btc_t *btc, u8 *buf, u32 len)
{
	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s():\n", __func__);
}

void _update_dm_step(struct btc_t *btc, const char *strin)
{
	struct btc_dm *dm = &btc->dm;
	u32 store_index = 0;

	dm->dm_step.cnt++;
	if (dm->dm_step.cnt == 0)
		dm->dm_step.cnt = 1;

	store_index = ((dm->dm_step.cnt-1) % BTC_DM_MAXSTEP);
	_rsn_cpy(dm->dm_step.step[store_index], (char*)strin);
}

static void _update_bt_info(struct btc_t *btc, u8 *buf, u32 len)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_cx *cx = &btc->cx;
	struct btc_bt_info *bt = &cx->bt;
	struct btc_bt_link_info *b = &bt->link_info;
	struct btc_bt_hfp_desc *hfp = &b->hfp_desc;
	struct btc_bt_hid_desc *hid = &b->hid_desc;
	struct btc_bt_a2dp_desc *a2dp = &b->a2dp_desc;
	struct btc_bt_pan_desc *pan = &b->pan_desc;
	union btc_btinfo btinfo;
	bool bt_link_change = false;

	if (buf[BTC_BTINFO_L1] != BTC_BT_INFO_LEN)
		return;

	/* return if bt info match last bt-info  */
	if (!hal_mem_cmp(h, bt->raw_info, buf, BTC_BTINFO_MAX)) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s return by bt-info duplicate!!\n",
			  __func__);
		cx->cnt_bt[BTC_BCNT_INFOSAME]++;
		return;
	}

	hal_mem_cpy(h, bt->raw_info, buf, BTC_BTINFO_MAX);

	PHL_INFO("[BTC], %s: bt_info[2]=0x%02x\n", __func__, bt->raw_info[2]);

	/* reset to mo-connect before update */
	b->profile_cnt.last = b->profile_cnt.now;
	b->relink.last = b->relink.now;
	a2dp->exist_last = a2dp->exist;
	b->multi_link.last = b->multi_link.now;
	bt->inq_pag.last = bt->inq_pag.now;
	b->profile_cnt.now = 0;
	hid->type = 0;

	/* ======= parse raw info low-Byte2 ======= */
	btinfo.val = bt->raw_info[BTC_BTINFO_L2];
	b->status.map.connect = btinfo.lb2.connect;
	b->status.map.sco_busy = btinfo.lb2.sco_busy;
	b->status.map.acl_busy = btinfo.lb2.acl_busy;
	b->status.map.inq_pag = btinfo.lb2.inq_pag;
	bt->inq_pag.now = btinfo.lb2.inq_pag;
	cx->cnt_bt[BTC_BCNT_INQPAG] += !!(bt->inq_pag.now && !bt->inq_pag.last);

	hfp->exist = btinfo.lb2.hfp;
	b->profile_cnt.now += (u8)hfp->exist;
	hid->exist = btinfo.lb2.hid;
	b->profile_cnt.now += (u8)hid->exist;
	a2dp->exist = btinfo.lb2.a2dp;
	b->profile_cnt.now += (u8)a2dp->exist;
	pan->exist = btinfo.lb2.pan;
	b->profile_cnt.now += (u8)pan->exist;

	/* ======= parse raw info low-Byte3 ======= */
	btinfo.val = bt->raw_info[BTC_BTINFO_L3];
	if (btinfo.lb3.retry != 0)
		cx->cnt_bt[BTC_BCNT_RETRY]++;
	b->cqddr = btinfo.lb3.cqddr;
	cx->cnt_bt[BTC_BCNT_INQ] += !!(btinfo.lb3.inq && !bt->inq);
	bt->inq = btinfo.lb3.inq;
	cx->cnt_bt[BTC_BCNT_PAGE] += !!(btinfo.lb3.pag && !bt->pag);
	bt->pag = btinfo.lb3.pag;

	b->status.map.mesh_busy = btinfo.lb3.mesh_busy;
	/* ======= parse raw info high-Byte0 ======= */
	btinfo.val = bt->raw_info[BTC_BTINFO_H0];
	/* raw val is dBm unit, translate from -100~ 0dBm to 0~100%*/
	b->rssi = btc->chip->ops->bt_rssi(btc, btinfo.hb0.rssi);

	/* ======= parse raw info high-Byte1 ======= */
	btinfo.val = bt->raw_info[BTC_BTINFO_H1];
	b->status.map.ble_connect = btinfo.hb1.ble_connect;
	if (btinfo.hb1.ble_connect)
		hid->type |= (hid->exist? BTC_HID_BLE : BTC_HID_RCU);

	cx->cnt_bt[BTC_BCNT_REINIT] += !!(btinfo.hb1.reinit && !bt->reinit);
	bt->reinit = btinfo.hb1.reinit;
	cx->cnt_bt[BTC_BCNT_RELINK] += !!(btinfo.hb1.relink && !b->relink.now);
	b->relink.now = btinfo.hb1.relink;
	cx->cnt_bt[BTC_BCNT_IGNOWL] += !!(btinfo.hb1.igno_wl && !bt->igno_wl);
	bt->igno_wl = btinfo.hb1.igno_wl;

	hid->type |= (btinfo.hb1.voice? BTC_HID_RCU_VOICE : 0);
	bt->ble_scan_en = btinfo.hb1.ble_scan;

	cx->cnt_bt[BTC_BCNT_ROLESW] += !!(btinfo.hb1.role_sw && !b->role_sw);
	b->role_sw = btinfo.hb1.role_sw;

	b->multi_link.now = btinfo.hb1.multi_link;

	if (b->multi_link.now != b->multi_link.last)
		bt_link_change = true;

	/* ======= parse raw info high-Byte2 ======= */
	btinfo.val = bt->raw_info[BTC_BTINFO_H2];
	pan->active = !!btinfo.hb2.pan_active;

	cx->cnt_bt[BTC_BCNT_AFH] += !!(btinfo.hb2.afh_update && !b->afh_update);
	b->afh_update = btinfo.hb2.afh_update;
	a2dp->active = btinfo.hb2.a2dp_active;
	b->slave_role = btinfo.hb2.slave;
	hid->slot_info = btinfo.hb2.hid_slot;

	if (hid->pair_cnt != btinfo.hb2.hid_cnt)
		bt_link_change = true;

	hid->pair_cnt = btinfo.hb2.hid_cnt;
	hid->type |= (hid->slot_info == BTC_HID_218? BTC_HID_218 : BTC_HID_418);

	/* ======= parse raw info high-Byte3 ======= */
	btinfo.val = bt->raw_info[BTC_BTINFO_H3];
	a2dp->bitpool = btinfo.hb3.a2dp_bitpool;

	if (b->tx_3M != (u32)btinfo.hb3.tx_3M)
		cx->cnt_bt[BTC_BCNT_RATECHG]++;
	b->tx_3M = (u32)btinfo.hb3.tx_3M;

	a2dp->sink = btinfo.hb3.a2dp_sink;

#if !BTC_CX_FW_OFFLOAD
	if (bt->igno_wl && !cx->wl.status.map.rf_off)
		_set_bt_ignore_wlan_act(btc, false);

	if (b->profile_cnt.now || b->status.map.ble_connect)
		hal_btc_fw_en_rpt(btc, RPT_EN_BT_AFH_MAP, 1);
	else
		hal_btc_fw_en_rpt(btc, RPT_EN_BT_AFH_MAP, 0);

	if (bt_link_change) {
		PHL_INFO("[BTC], %s: bt link change!!\n", __func__);
		hal_btc_send_event(btc, NULL, 0, BTC_HMSG_BT_LINK_CHG);
	}

	/* reset after A2DP stop->play */
	if (!a2dp->exist_last && a2dp->exist) {
		a2dp->vendor_id = 0;
		a2dp->flush_time = 0;
		a2dp->play_latency = 1;
		_set_btc_timer(btc, BTC_TIMER_BT_A2DPPLAY, BTC_A2DP_RESUME_MAXT);
	}

	if (a2dp->exist && (a2dp->flush_time == 0 || a2dp->vendor_id == 0 ||
	    a2dp->play_latency == 1))
		hal_btc_fw_en_rpt(btc, RPT_EN_BT_DEVICE_INFO, 1);
	else
		hal_btc_fw_en_rpt(btc, RPT_EN_BT_DEVICE_INFO, 0);

	_run_coex(btc, __func__);
#endif
}

static void _ntfy_role_info(struct btc_t *btc, u8 rid,
			    struct rtw_wifi_role_t *wrole,
			    struct rtw_phl_stainfo_t *sta,
			    enum role_state reason)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_wl_link_info *r = NULL;
#ifdef CONFIG_PHL_P2PPS
	u8 i =0;
#endif /* CONFIG_PHL_P2PPS */

	PHL_INFO("[BTC], %s(), role_id=%d, reason=%d\n", __func__, rid, reason);

	if (rid >= MAX_WIFI_ROLE_NUMBER)
		return;

	btc->dm.cnt_notify[BTC_NCNT_ROLE_INFO]++;

	r = &wl->link_info[rid];

	if (wrole) {
		r->role = wrole->type;
#ifdef RTW_WKARD_ROLE_TYPE
		if (wrole->mstate != MLME_NO_LINK &&
			wrole->real_type != PHL_RTYPE_NONE) {
			r->role = wrole->real_type;
			PHL_INFO("[BTC], rtw_hal_btc_update_role_info_ntfy(): set r.role from type(%d) to real_type(%d)\n",
				 wrole->type, wrole->real_type);
		}
#endif /* RTW_WKARD_ROLE_TYPE */
#ifdef CONFIG_PHL_P2PPS
		r->noa = 0;
		r->noa_duration = 0;
		for (i = 0; i < MAX_NOA_DESC; i++) {
			if (wrole->noa_desc[i].enable) {
				r->noa = 1;
				r->noa_duration = wrole->noa_desc[i].duration;
				break;
			}
		}
#endif /* CONFIG_PHL_P2PPS */
		r->phy = wrole->hw_band;
		r->pid = wrole->hw_port;
		r->active = wrole->active;
		r->connected = wrole->mstate;
		r->mode = wrole->cap.wmode;
		r->client_cnt = wrole->assoc_sta_queue.cnt;
#ifdef RTW_PHL_BCN
		r->bcn_period = wrole->bcn_cmn.bcn_interval;
		r->dtim_period = wrole->dtim_period;
#endif
		hal_mem_cpy(h, &r->chdef, &wrole->chandef,
			    sizeof(struct rtw_chan_def));
		hal_mem_cpy(h, r->mac_addr, wrole->mac_addr, MAC_ALEN);
	}

	if (sta && wrole->type == PHL_RTYPE_STATION) {/*associated node info??*/
		r->mac_id = sta->macid;
		r->mode = (u8)sta->wmode;
		r->stbc_he_tx = (u32)sta->asoc_cap.stbc_he_tx;
		r->stbc_vht_tx = (u32)sta->asoc_cap.stbc_vht_tx;
		r->stbc_ht_tx = (u32)sta->asoc_cap.stbc_ht_tx;
	}

	/* refresh wifi info */
	_update_wl_info(btc);

	if (r->role == PHL_RTYPE_STATION &&
	    r->connected == MLME_NO_LINK)
	    btc->dm.leak_ap = 0;

	if (reason == PHL_ROLE_MSTS_STA_CONN_START)
		wl->status.map.connecting = 1;
	else
		wl->status.map.connecting = 0;

	if (reason == PHL_ROLE_MSTS_STA_DIS_CONN)
		wl->status.map._4way = false;

	_run_coex(btc, __func__);
}

static void _ntfy_radio_state(struct btc_t *btc, u8 rf_state)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_chip_ops *ops = btc->chip->ops;

	PHL_INFO("[BTC], %s(): rf_state =%d\n", __func__, rf_state);
	btc->dm.cnt_notify[BTC_NCNT_RADIO_STATE]++;

	switch(rf_state) {
	case BTC_RFCTRL_WL_OFF:
		wl->status.map.rf_off = 1;
		wl->status.map.lps = BTC_LPS_OFF;
		wl->status.map.busy = 0;
		break;
	case BTC_RFCTRL_FW_CTRL: /*  LPS-PG, LPS-CG */
		wl->status.map.rf_off = 0;
		wl->status.map.lps = BTC_LPS_RF_OFF;
		wl->status.map.busy = 0;
		break;
	case BTC_RFCTRL_LPS_WL_ON: /* LPS-Protocol (RFon) */
		wl->status.map.rf_off = 0;
		wl->status.map.lps = BTC_LPS_RF_ON;
		wl->status.map.busy = 0;
		break;
	case BTC_RFCTRL_WL_ON:
	default:
		wl->status.map.rf_off = 0;
		wl->status.map.lps = BTC_LPS_OFF;
		break;
	}

	if (rf_state == BTC_RFCTRL_WL_ON) {
		hal_btc_fw_en_rpt(btc, RPT_EN_MREG, 1);
		_write_scbd(btc, BTC_WSCB_ACTIVE | BTC_WSCB_ON | BTC_WSCB_BTLOG, true);
		_update_bt_scbd(btc, true);
		if (ops && ops->init_cfg)
			ops->init_cfg(btc);
	} else {
		hal_btc_fw_en_rpt(btc, RPT_EN_ALL, 0);
		/* for BT only isolated issue,
		 * clear all scbd if LPS-PG or "LPS-off to LPS-Protocol"
		 */
		if (rf_state == BTC_RFCTRL_LPS_WL_ON &&
		    wl->status.map.lps_pre != 0) {
			_write_scbd(btc, BTC_WSCB_ON, true);
		} else {
			_write_scbd(btc, BTC_WSCB_ALL, false);
		}
	}

	_run_coex(btc, __func__);

	wl->status.map.rf_off_pre = wl->status.map.rf_off;
	wl->status.map.lps_pre = wl->status.map.lps;
}

static void _ntfy_customerize(struct btc_t *btc, u8 type, u16 len, u8 *buf)
{
	struct btc_bt_info *bt = &btc->cx.bt;

	if (!buf)
		return;

	PHL_INFO("[BTC], %s !! \n", __func__);
	btc->dm.cnt_notify[BTC_NCNT_CUSTOMERIZE]++;

	switch (type) {
	case PHL_BTC_CNTFY_BTINFO:
		if (len != 1)
			return;
		buf[0] = bt->raw_info[BTC_BTINFO_L2];
		break;
	}
}

static u8 _ntfy_wl_rfk(struct btc_t *btc, u8 phy_path, u8 type, u8 state)
{
	struct btc_cx *cx = &btc->cx;
	struct btc_wl_info *wl = &cx->wl;
	bool result = BTC_WRFK_REJECT;

	wl->rfk_info.type = type;
	wl->rfk_info.path_map = phy_path & BTC_RFK_PATH_MAP;
	wl->rfk_info.phy_map = (phy_path & BTC_RFK_PHY_MAP) >> 4;
	wl->rfk_info.band = (phy_path & BTC_RFK_BAND_MAP) >> 6;
	state &= (BIT(0) | BIT(1));

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s()_start: phy=0x%x, path=0x%x, type=%d, state=%d\n",
		 __func__, wl->rfk_info.phy_map, wl->rfk_info.path_map,
		 type, state);

	switch (state) {
	case BTC_WRFK_START:
		result = _chk_wl_rfk_request(btc);
		wl->rfk_info.state = (result? BTC_WRFK_START : BTC_WRFK_STOP);
#if 0
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_RFK);
		_write_scbd(btc, BTC_WSCB_WLRFK, result);
#endif

		btc->dm.cnt_notify[BTC_NCNT_WL_RFK]++;
		break;
	case BTC_WRFK_ONESHOT_START:
	case BTC_WRFK_ONESHOT_STOP:
		if (wl->rfk_info.state == BTC_WRFK_STOP) {
			result = BTC_WRFK_REJECT;
		} else {
			result = BTC_WRFK_ALLOW;
			wl->rfk_info.state = state;
		}
		break;
	case BTC_WRFK_STOP:
		result = BTC_WRFK_ALLOW;
		wl->rfk_info.state = BTC_WRFK_STOP;
#if 0
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_RFK);
		_write_scbd(btc, BTC_WSCB_WLRFK, false);
#endif
		break;
	}

	if (result == BTC_WRFK_ALLOW) {
		/* Only update coex for RFK START and STOP
		 * because Start -> OneSHOT_START time is short
		 */
		if (wl->rfk_info.state == BTC_WRFK_START ||
		    wl->rfk_info.state == BTC_WRFK_STOP)
			_run_coex(btc, __func__);

		if (wl->rfk_info.state == BTC_WRFK_START) /* wait 300ms */
			_set_btc_timer(btc, BTC_TIMER_WL_RFKTO, BTC_WRFK_MAXT);

	}

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,"[BTC], %s()_finish: rfk_cnt=%d, result=%d\n",
		 __func__, btc->dm.cnt_notify[BTC_NCNT_WL_RFK], result);

	return result;
}

static void _ntfy_wl_sta(struct btc_t *btc, struct rtw_stats *phl_stats,
			u8 ntfy_num, struct rtw_phl_stainfo_t *sta[],
			u8 reason)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_dm *dm = &btc->dm;
	struct btc_module *module = &btc->mdinfo;
	struct btc_wl_stat_info w[MAX_WIFI_ROLE_NUMBER] = {0};
	struct btc_traffic *t = NULL, *link_info_t = NULL;
	struct btc_wl_link_info *link_info = NULL;
	struct rtw_phl_rainfo ra_info = {0};
	u8 i, j, busy = 0, dir = 0, rssi_map = 0;
	u8 busy_all = 0, dir_all = 0, rssi_map_all = 0;
	u8 *rssi_state = NULL, rssi_thres = 0;
	bool is_sta_change = false, is_traffic_change = false;
	u16 last_tx_rate, last_rx_rate, last_tx_lvl, last_rx_lvl;
	u32 chk_intvl = 20;

	/* rssi_map = 4 bits for rssi locate in which {60, 50, 40, 30}
	 * if rssi >= 60% (-50dBm) --> map = 4b'0000 --> rssi_level = 0
	 * if 50% <= rssi < 60%    --> map = 4b'0001 --> rssi_level = 1
	 * if 40% <= rssi < 50%    --> map = 4b'0011 --> rssi_level = 2
	 * if 30% <= rssi < 40%    --> map = 4b'0111 --> rssi_level = 3
	 * if rssi < 20%           --> map = 4b'1111 --> rssi_level = 4
	 */

	dm->cnt_notify[BTC_NCNT_WL_STA]++;

	for (i = 0; i < ntfy_num; i++) {
		/* Extract btc_wl_stat_info from rtw_phl_stainfo_t */
		hal_mem_set(h, &ra_info, 0, sizeof(ra_info));
		w[i].pid = sta[i]->wrole->id;
		w[i].stat.rssi = sta[i]->hal_sta->rssi_stat.rssi >> 1;

		t = &w[i].stat.traffic;
		t->tx_lvl = phl_stats->tx_traffic.lvl;
		t->tx_sts = phl_stats->tx_traffic.sts;

		t->rx_lvl = phl_stats->rx_traffic.lvl;
		t->rx_sts = phl_stats->rx_traffic.sts;

		 if (RTW_HAL_STATUS_SUCCESS ==
		    rtw_hal_bb_query_rainfo(h, sta[i]->hal_sta, &ra_info))
			t->tx_rate = ra_info.rate;
		else
			t->tx_rate = RTW_DATA_RATE_MAX;

		t->rx_rate = h->trx_stat.rx_rate_plurality;

		/* transfer btc_wl_stat_info to btc_wl_link_info */
		link_info = &wl->link_info[w[i].pid];
		link_info_t = &link_info->stat.traffic;

		if (link_info->connected == MLME_NO_LINK) {
			link_info->rx_rate_drop_cnt = 0;
			rssi_map_all |= rssi_map;
			continue;
		}

		last_tx_rate = link_info_t->tx_rate;
		last_rx_rate = link_info_t->rx_rate;
		last_tx_lvl = (u16)link_info_t->tx_lvl;
		last_rx_lvl = (u16)link_info_t->rx_lvl;

		/* refresh link_info traffic/rssi related info from t */
		hal_mem_cpy(h, (void *)link_info_t, (void *)t,
			    sizeof(struct btc_traffic));

		link_info->stat.rssi = w[i].stat.rssi;
		rssi_map = 0;

		/* check if rssi across wl_rssi_thres boundary */
		for (j = 0; j < BTC_WL_RSSI_THMAX; j++) {
			if (module->ant.type == BTC_ANT_SHARED && j == 0)
				rssi_thres = BTC_WL_RSSI_MAX_BTG;
			else
				rssi_thres = btc->chip->wl_rssi_thres[j];

			rssi_state = &link_info->rssi_state[j];
			*rssi_state = _update_rssi_state(btc, *rssi_state,
							link_info->stat.rssi,
							rssi_thres);

			/* fill rssi bit map 0~3 if rssi < threshold  */
			if (BTC_RSSI_LOW(*rssi_state))
				rssi_map |= BIT(j);

			if (module->ant.type == BTC_ANT_DEDICATED &&
			    BTC_RSSI_CHANGE(*rssi_state))
				is_sta_change = true;
		}

		/* OR STA/GC role rssi map */
		if (link_info->role == PHL_RTYPE_STATION ||
		    link_info->role == PHL_RTYPE_P2P_GC)
			rssi_map_all |= rssi_map;

		/* set busy once idle->busy immediately */
		if (t->tx_lvl != RTW_TFC_IDLE || t->rx_lvl != RTW_TFC_IDLE) {
			busy = 1;
			link_info->busy_t = _os_get_cur_time_ms();

			if (t->tx_lvl > t->rx_lvl)
				dir = TRAFFIC_UL;
			else
				dir = TRAFFIC_DL;
		} else {/*set idle if busy -> idle after BTC_BUSY2IDLE_THRES*/
			if (phl_get_passing_time_ms(link_info->busy_t) >
			    BTC_BUSY2IDLE_THRES ||
			    !wl->role_info.role_map.role.station ||
			    wl->role_info.link_mode == BTC_WLINK_NOLINK) {
				busy = 0;
				dir = TRAFFIC_DL;
			} else {
				busy = link_info->busy;
				dir = link_info->dir;
			}
		}

		if (link_info->busy != busy || link_info->dir != dir) {
			is_sta_change = true;
			link_info->busy = busy;
			link_info->dir = dir;
		}

		/* OR all role busy/dir state */
		busy_all |= link_info->busy;
		dir_all |= BIT(link_info->dir);

		if (link_info_t->rx_rate <= RTW_DATA_RATE_CCK2 &&
		    last_rx_rate > RTW_DATA_RATE_CCK2 &&
		    link_info_t->rx_lvl > 0)
			link_info->rx_rate_drop_cnt++;

		if (last_tx_rate != link_info_t->tx_rate ||
		    last_rx_rate != link_info_t->rx_rate ||
		    last_tx_lvl != link_info_t->tx_lvl ||
		    last_rx_lvl != link_info_t->rx_lvl )
			is_traffic_change = true;

		wl->role_info.active_role[w[i].pid].tx_lvl = (u16)t->tx_lvl;
		wl->role_info.active_role[w[i].pid].rx_lvl = (u16)t->rx_lvl;
		wl->role_info.active_role[w[i].pid].tx_rate = t->tx_rate;
		wl->role_info.active_role[w[i].pid].rx_rate = t->rx_rate;
	}

	wl->rssi_level = 0;
	for (j = BTC_WL_RSSI_THMAX; j > 0; j--) {
		/* set RSSI level 4 ~ 0 if rssi bit map match */
		if (rssi_map_all & BIT(j-1)) {
			wl->rssi_level = j;
			break;
		}
	}

	chk_intvl = BTC_RPT_PERIOD/BTC_PERIODIC_TIME;

	if (dm->cnt_notify[BTC_NCNT_WL_STA] >=
	    dm->cnt_dm[BTC_DCNT_WL_STA_LAST] + chk_intvl)
		_get_wl_nhm_dbm(btc);

	dm->cnt_dm[BTC_DCNT_WL_STA_LAST] = dm->cnt_notify[BTC_NCNT_WL_STA];

	/* for TDD/FDD packet estimation in WL FW */
	if (is_traffic_change)
		hal_btc_fw_set_drv_info(btc, CXDRVINFO_ROLE);

	if (is_sta_change) {
		wl->status.map.busy = (u32)busy_all;
		wl->status.map.traffic_dir = (u32)dir_all;
		_write_scbd(btc, BTC_WSCB_WLBUSY, (bool)(!!busy_all));
		_run_coex(btc, __func__);
	}
}

static void _ntfy_fwinfo(struct btc_t *btc, u8 *buf, u32 len, u8 cls, u8 func)
{
	struct btf_fwinfo *pfwinfo = &btc->fwinfo;

	if (!buf || !len)
		return;

	btc->dm.cnt_notify[BTC_NCNT_FWINFO]++;
	pfwinfo->cnt_c2h++;

	if (cls == BTFC_FW_EVENT) {
		switch (func) {
		case BTF_EVNT_RPT:
		case BTF_EVNT_BUF_OVERFLOW:
			pfwinfo->event[func]++;
			hal_btc_fw_event(btc, func, buf, len);
			break;
		case BTF_EVNT_BT_INFO:
			btc->cx.cnt_bt[BTC_BCNT_INFOUPDATE]++;
			_update_bt_info(btc, buf, len);
			break;
		case BTF_EVNT_BT_SCBD:
			btc->cx.cnt_bt[BTC_BCNT_SCBDUPDATE]++;
			_update_bt_scbd(btc, false);
			break;
		case BTF_EVNT_BT_PSD:
			_update_bt_psd(btc, buf, len);
			break;
		case BTF_EVNT_BT_REG:
			btc->dbg.rb_done = true;
			btc->dbg.rb_val = ((buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0]));
			break;
		case BTF_EVNT_C2H_LOOPBACK:
			btc->dbg.rb_done = true;
			btc->dbg.rb_val = buf[0];
			break;
		case BTF_EVNT_CX_RUNINFO:
			btc->dm.cnt_dm[BTC_DCNT_CX_RUNINFO]++;
			_update_offload_runinfo(btc, buf, len);
			break;
		}
	}
}

static void _ntfy_timer(struct btc_t *btc, u16 tmr_id)
{
	struct btc_dm *dm = &btc->dm;
	struct btc_cx *cx = &btc->cx;
	struct btc_wl_info *wl = &cx->wl;
	struct btc_bt_a2dp_desc *a2dp = &cx->bt.link_info.a2dp_desc;
	bool is_sta_change = false;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_,
		  "[BTC], %s(): tmr_id =%d\n", __func__, tmr_id);

	dm->cnt_notify[BTC_NCNT_TIMER]++;

	if (tmr_id == BTC_TIMER_PERIODIC) {
		/* start next periodic timer */
		_set_btc_timer(btc, BTC_TIMER_PERIODIC, BTC_PERIODIC_TIME);
	} else if (tmr_id == BTC_TIMER_WL_RFKTO) {
		if (wl->rfk_info.state != BTC_WRFK_STOP) {
			cx->cnt_wl[BTC_WCNT_RFK_TIMEOUT]++;
			dm->error.map.wl_rfk_timeout = true;
			wl->rfk_info.state = BTC_WRFK_STOP;
			/* _write_scbd(btc, BTC_WSCB_WLRFK, false); */
			is_sta_change = true;
		}
	} else if (tmr_id == BTC_TIMER_WL_SPECPKT) {
		wl->status.map._4way = false;
		wl->status.map.connecting = 0;
		is_sta_change = true;
	} else if (tmr_id == BTC_TIMER_BT_A2DPPLAY) {
		a2dp->play_latency = 0;
		is_sta_change = true;
	}

	if (is_sta_change)
		_run_coex(btc, __func__);
}

/******************************************************************************
 *
 * coexistence extern functions
 *
 *****************************************************************************/
/*
 * btc related sw initialization
 */
bool hal_btc_init(struct btc_t *btc)
{
	switch (btc->hal->chip_id) {
#ifdef BTC_8852A_SUPPORT
	case CHIP_WIFI6_8852A:
		PHL_INFO("[BTC], %s(): Init 8852A!!\n", __func__);
		btc->chip = &chip_8852a;
		break;
#endif
#ifdef BTC_8852B_SUPPORT
	case CHIP_WIFI6_8852B:
		PHL_INFO("[BTC], %s(): Init 8852B!!\n", __func__);
		btc->chip = &chip_8852b;
		break;
#endif
#ifdef BTC_8852C_SUPPORT
	case CHIP_WIFI6_8852C:
		PHL_INFO("[BTC], %s(): Init 8852C!!\n", __func__);
		btc->chip = &chip_8852c;
		break;
#endif
	default:
		PHL_ERR("[BTC], %s(): no matched IC!!\n", __func__);
		btc->cx.wl.status.map.init_ok = false;
		return false;
	}

	_reset_btc_var(btc, BTC_RESET_ALL);
	_btmr_init(btc);

	_rsn_cpy(btc->dm.run_reason, "None");
	_act_cpy(btc->dm.run_action, "None");

	btc->hal->btc_vc.btc_ver = coex_ver;
	btc->ops = &_btc_ops;
	btc->mlen = BTC_MSG_MAXLEN;
	btc->ctrl.igno_bt = true;
	btc->cx.wl.status.map.init_ok = true;

	return true;
}

void hal_btc_deinit(struct btc_t *btc)
{
	_btmr_deinit(btc);
}

#endif

