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
#define _HAL_BTC_ACTION_C_
#include "../hal_headers_le.h"
#include "hal_btc.h"
#include "halbtc_fw.h"
#include "halbtc_fwdef.h"
#include "halbtc_action.h"
#include "halbtc_def.h"

#ifdef CONFIG_BTCOEX

#define _set_cx_ctrl(btc, val) rtw_hal_mac_set_coex_ctrl(btc->hal, val)

/* tdma policy template */
struct fbtc_tdma t_def[] = {
	{ CXTDMA_OFF,    CXFLC_OFF, CXTPS_OFF, 0, 0, 0, 0, 0}, /*CXTD_OFF*/
	{ CXTDMA_OFF,    CXFLC_OFF, CXTPS_OFF, 0, 0, 1, 0, 0}, /*CXTD_B2*/
	{ CXTDMA_OFF,    CXFLC_OFF, CXTPS_OFF, 0, 0, 2, 0, 0}, /*CXTD_OFF_EXT*/
	{ CXTDMA_FIX,    CXFLC_OFF, CXTPS_OFF, 0, 0, 0, 0, 0}, /* CXTD_FIX */
	{ CXTDMA_FIX,  CXFLC_NULLP,  CXTPS_ON, 0, 5, 0, 0, 0}, /* CXTD_PFIX */
	{ CXTDMA_AUTO,   CXFLC_OFF, CXTPS_OFF, 0, 0, 0, 0, 0}, /* CXTD_AUTO */
	{ CXTDMA_AUTO, CXFLC_NULLP,  CXTPS_ON, 0, 5, 0, 0, 0}, /* CXTD_PAUTO */
	{CXTDMA_AUTO2,   CXFLC_OFF, CXTPS_OFF, 0, 0, 0, 0, 0}, /* CXTD_AUTO2 */
	{CXTDMA_AUTO2, CXFLC_NULLP,  CXTPS_ON, 0, 5, 0, 0, 0} /* CXTD_PAUTO2 */
};

/* slot policy template */
struct fbtc_slot s_def[] = {
	{100, 0x55555555, SLOT_MIX}, /* CXST_OFF */
	{  5, 0xea5a5a5a, SLOT_ISO}, /* CXST_B2W */
	{ 70, 0xea5a5a5a, SLOT_ISO}, /* CXST_W1 */
	{ 15, 0xea5a5a5a, SLOT_ISO}, /* CXST_W2 */
	{ 15, 0xea5a5a5a, SLOT_ISO}, /* CXST_W2B */
	{250, 0xe5555555, SLOT_MIX}, /* CXST_B1 */
	{  7, 0xea5a5a5a, SLOT_MIX}, /* CXST_B2 */
	{  5, 0xe5555555, SLOT_MIX}, /* CXST_B3 */
	{ 50, 0xe5555555, SLOT_MIX}, /* CXST_B4 */
	{ 20, 0xea5a5a5a, SLOT_ISO}, /* CXST_LK */
	{400, 0x55555555, SLOT_MIX}, /* CXST_BLK */
	{  0, 0xea5a5a5a, SLOT_MIX}, /* CXST_E2G */
	{  0, 0xffffffff, SLOT_MIX}, /* CXST_E5G */
	{  0, 0xe5555555, SLOT_MIX}, /* CXST_EBT */
	{  0, 0xaaaaaaaa, SLOT_ISO}, /* CXST_ENULL */
	{250, 0xea5a5a5a, SLOT_MIX}, /* CXST_WLK */
	{ 35, 0xfafafafa, SLOT_ISO}, /* CXST_W1FDD */
	{250, 0xffffffff, SLOT_MIX}  /* CXST_B1FDD */
};

const u32 cxtbl[] = {
	0xffffffff, /* 0 */
	0xaaaaaaaa, /* 1 */
	0xe5555555, /* 2 */
	0xee555555, /* 3 */
	0xd5555555, /* 4 */
	0x5a5a5a5a, /* 5 */
	0x5a5a5aaa, /* 6 */
	0xda5a5a5a, /* 7 */
	0xea5a5a5a, /* 8 */
	0x6a5a5aaa, /* 9 */
	0x6a5a6a5a, /* 10 */
	0x6a5a6aaa, /* 11 */
	0x6afa5afa, /* 12 */
	0xaaaa5aaa, /* 13 */
	0xaaffffaa, /* 14 */
	0xaa5555aa, /* 15 */
	0xfafafafa, /* 16 */
	0xffffddff, /* 17 */
	0xdaffdaff, /* 18 */
	0xfafadafa  /* 19 */
};

void _set_bt_ignore_wlan_act(struct btc_t *btc, u8 enable)
{
	u8 buf = 0;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): set bt %s wlan_act\n",
		  __func__, (enable? "ignore" : "do not ignore"));

	buf = enable;
	hal_btc_fw_set_bt(btc, SET_BT_IGNORE_WLAN_ACT, 1, &buf);
}

void _set_wl_tx_power(struct btc_t *btc, u32 level)
{
	struct btc_wl_info *wl = &btc->cx.wl;

	if (wl->rf_para.tx_pwr_freerun == level)
		return;

	wl->rf_para.tx_pwr_freerun = level;
	btc->dm.rf_trx_para.wl_tx_power = level;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): level = %d\n",
		  __func__, level);

	btc->chip->ops->wl_tx_power(btc, level);
}

void _set_wl_rx_gain(struct btc_t *btc, u32 level)
{
	struct btc_wl_info *wl = &btc->cx.wl;

	if (wl->rf_para.rx_gain_freerun == level)
		return;

	wl->rf_para.rx_gain_freerun = level;
	btc->dm.rf_trx_para.wl_rx_gain = level;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): level = %d\n",
		  __func__, level);

	btc->chip->ops->wl_rx_gain(btc, level);
}

void _set_bt_tx_power(struct btc_t *btc, u32 level)
{
	struct btc_bt_info *bt = &btc->cx.bt;
	u8 buf = 0;

	if (bt->rf_para.tx_pwr_freerun == level)
		return;

	bt->rf_para.tx_pwr_freerun = level;
	btc->dm.rf_trx_para.bt_tx_power = level;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): level = %d\n",
		  __func__, level);

	buf = (0x100 - level) & bMASKB0;
	hal_btc_fw_set_bt(btc, SET_BT_TX_PWR, 1, &buf);
}

void _set_bt_rx_gain(struct btc_t *btc, u32 level)
{
	struct btc_bt_info *bt = &btc->cx.bt;
	u8 buf = 0;

	if (bt->rf_para.rx_gain_freerun == level ||
	    level > BTC_BT_RX_NORMAL_LVL)
		return;

	bt->rf_para.rx_gain_freerun = level;
	btc->dm.rf_trx_para.bt_rx_gain = level;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): level = %d\n",
		  __func__, level);

	buf = level & bMASKB0;

	if (buf == BTC_BT_RX_NORMAL_LVL)
		_write_scbd(btc, BTC_WSCB_RXGAIN, false);
	else
		_write_scbd(btc, BTC_WSCB_RXGAIN, true);

	hal_btc_fw_set_bt(btc, SET_BT_LNA_CONSTRAIN, 1, &buf);
}

static void _set_rf_trx_para(struct btc_t *btc)
{
	struct btc_dm *dm = &btc->dm;
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	struct btc_bt_link_info *b = &bt->link_info;
	struct btc_rf_trx_para para;
	u8 level_id = 0;
	u32 wl_stb_chg = 0;

	if (!dm->freerun) {
		dm->trx_para_level = 0;

#if 1
		/* fix LNA2 = level-5 for BT ACI issue at BTG */
		if (btc->hal->chip_id == CHIP_WIFI6_8852A &&
		    btc->dm.wl_btg_rx && b->profile_cnt.now != 0)
			dm->trx_para_level = 1;
#endif
	}

	level_id = (u8)dm->trx_para_level;

	if (level_id >= btc->chip->rf_para_dlink_num ||
	    level_id >= btc->chip->rf_para_ulink_num) {
	    PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): invalid level_id: %d\n",
		      __func__, level_id);
	    return;
	}

	if (wl->status.map.traffic_dir & BIT(TRAFFIC_UL))
		para = btc->chip->rf_para_ulink[level_id];
	else
		para = btc->chip->rf_para_dlink[level_id];

	_set_wl_tx_power(btc, para.wl_tx_power);
	_set_wl_rx_gain(btc, para.wl_rx_gain);
	_set_bt_tx_power(btc, para.bt_tx_power);
	_set_bt_rx_gain(btc, para.bt_rx_gain);

	if (bt->enable.now == 0 || wl->status.map.rf_off == 1 ||
	    wl->status.map.lps)
	    wl_stb_chg = 0;
	else
	    wl_stb_chg = 1;

	if (wl_stb_chg != dm->wl_stb_chg) {
		dm->wl_stb_chg = wl_stb_chg;
		btc->chip->ops->wl_s1_standby(btc, dm->wl_stb_chg);
	}

}

void _update_btc_state_map(struct btc_t *btc)
{
	struct btc_cx *cx = &btc->cx;
	struct btc_wl_info *wl = &cx->wl;
	struct btc_bt_info *bt = &cx->bt;
	struct btc_bt_link_info *bt_linfo = &bt->link_info;

	if (wl->status.map.connecting || wl->status.map._4way ||
	    wl->status.map.roaming) {
		cx->state_map = BTC_WLINKING;
	} else if (wl->status.map.scan) { /* wl scan */
		if (bt_linfo->status.map.inq_pag)
			cx->state_map = BTC_WSCAN_BSCAN;
		else
			cx->state_map = BTC_WSCAN_BNOSCAN;
	} else if (wl->status.map.busy) { /* only busy */
		if (bt_linfo->status.map.inq_pag)
			cx->state_map = BTC_WBUSY_BSCAN;
		else
			cx->state_map = BTC_WBUSY_BNOSCAN;
	} else { /* wl idle */
		cx->state_map = BTC_WIDLE;
	}
}

static void _set_bt_golden_rx_range(struct btc_t *btc, u8 p_id, u8 level)
{
	struct btc_bt_link_info *bt_linfo = &btc->cx.bt.link_info;
	u8 buf[2] = {0}, pos = 0, i;

	if (p_id > BTC_BT_PAN)
		return;

	for (i = 0; i< BTC_PROFILE_MAX; i++) {
		if ((p_id >> i) & 0x1) {
			pos = i;
			break;
		}
	}

	if (bt_linfo->golden_rx_shift[pos] == level)
		return;

	bt_linfo->golden_rx_shift[pos] = level;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): p_id=%d, level=%d\n",
		  __func__, p_id, level);

	buf[0] = level;
	buf[1] = pos;
	hal_btc_fw_set_bt(btc, SET_BT_GOLDEN_RX_RANGE, sizeof(buf), buf);
}

static void _set_bt_afh_info(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	struct btc_bt_link_info *b = &bt->link_info;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	/*struct btc_module *module = &btc->mdinfo;*/
	u8 en = 0, i, ch = 0, bw = CHANNEL_WIDTH_MAX;
	u8 max_role_cnt = BTC_TDMA_WLROLE_MAX;

	if (btc->ctrl.manual || wl->status.map.scan)
		return;

	if (btc->hal->dbcc_en)
		max_role_cnt++;

	/* TBD if include module->ant.type == BTC_ANT_SHARED */
	if (wl->status.map.rf_off || bt->whql_test ||
	    wl_rinfo->link_mode == BTC_WLINK_NOLINK ||
	    wl_rinfo->link_mode == BTC_WLINK_5G ||
	    wl_rinfo->connect_cnt > max_role_cnt) {
		en = false;
		ch = 0;
		bw = 0;
		goto exit;
	} else {
		for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
			/* get p2p channel for MCC */
			if (wl_rinfo->link_mode == BTC_WLINK_2G_MCC) {
				if (wl_rinfo->active_role[i].role == PHL_RTYPE_AP ||
				    wl_rinfo->active_role[i].role == PHL_RTYPE_P2P_GO ||
				    wl_rinfo->active_role[i].role == PHL_RTYPE_P2P_GC) {
					en = true;
					ch = wl_rinfo->active_role[i].ch;
					bw = wl_rinfo->active_role[i].bw;
					break;
				}
			} else if (wl_rinfo->active_role[i].connected &&
				   wl_rinfo->active_role[i].band == BAND_ON_24G) {
				en = true;
				ch = wl_rinfo->active_role[i].ch;
				bw = wl_rinfo->active_role[i].bw;

				/* get the max bw if scc */
				if (wl_rinfo->link_mode != BTC_WLINK_2G_SCC ||
				    wl_rinfo->active_role[i].bw == CHANNEL_WIDTH_40) {
					break;
				}
			}
		}
	}


	if (!en || ch > 14 || ch == 0)
		bw = CHANNEL_WIDTH_MAX;

	/* default AFH channel sapn = center-ch +- 6MHz  */
	switch (bw) {
	case CHANNEL_WIDTH_20:
		if (btc->dm.freerun)
			bw = 48;
		else
			bw = 20 + btc->chip->afh_guard_ch * 2;
		break;
	case CHANNEL_WIDTH_40:
		bw = 40 + btc->chip->afh_guard_ch * 2;
		break;
	case CHANNEL_WIDTH_5:
		bw = 5 + btc->chip->afh_guard_ch * 2;
		break;
	case CHANNEL_WIDTH_10:
		bw = 10 + btc->chip->afh_guard_ch * 2;
		break;
	default:
	case CHANNEL_WIDTH_MAX:
		en = false; /* turn off AFH info if invalid BW */
		bw = 0;
		ch = 0;
		break;
	}

exit:
	if (wl->afh_info.en == en &&
	    wl->afh_info.ch == ch &&
	    wl->afh_info.bw == bw &&
	    b->profile_cnt.last == b->profile_cnt.now)
		return;

	wl->afh_info.en = en;
	wl->afh_info.ch = ch;
	wl->afh_info.bw = bw;
	hal_btc_fw_set_bt(btc, SET_BT_WL_CH_INFO, 3, (u8*)&wl->afh_info);

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): en=%d, ch=%d, bw=%d\n",
		  __func__, en, ch, bw);
	btc->cx.cnt_wl[BTC_WCNT_CH_UPDATE]++;
}

static void _set_halbb_btg_ctrl(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	struct btc_wl_dbcc_info *wl_dinfo = &wl->dbcc_info;
	bool is_btg = false;

	if (btc->ctrl.manual)
		return;

	/* notify halbb ignore GNT_BT or not for WL BB Rx-AGC control */
	if (wl_rinfo->link_mode == BTC_WLINK_5G) /* always 0 if 5G */
		is_btg = false;
	else if (btc->dm.freerun)
		is_btg = false;
	else if (wl_rinfo->link_mode == BTC_WLINK_25G_DBCC &&
		 wl_dinfo->real_band[1] != BAND_ON_24G)
		is_btg = false;
	else
		is_btg = true;

	if (!run_rsn("_ntfy_init_coex") && is_btg == btc->dm.wl_btg_rx)
		return;

	btc->dm.wl_btg_rx = is_btg;
	/* rtw_hal_bb_ctrl_btg control the following:
	 * if btc->dm.wl_btg_rx = false (gnt_wl always = 1)
	 * Lte_rx:    0x10980[17]=1, 0x10980[29]=0
	 * Gnt_wl:    0x10980[18]=1, 0x10980[28]=1
	 * Gnt_bt_tx: 0x10980[19]=1, 0x10980[27]=0
	 * Gnt_bt:    0x10980[20]=1, 0x10980[26]=0
	 * if if btc->dm.wl_btg_rx = true (gnt from MAC)
	 * Lte_rx:    0x10980[17]=0, 0x10980[29]=0
	 * Gnt_wl:    0x10980[18]=0, 0x10980[28]=1
	 * Gnt_bt_tx: 0x10980[19]=0, 0x10980[27]=0
	 * Gnt_bt:    0x10980[20]=0, 0x10980[26]=0
	 */

	/* skip if MCC dual_band, call ctrl_btg8852a() in WL FW by slot */
	if (wl_rinfo->link_mode == BTC_WLINK_25G_MCC)
		return;

	rtw_hal_bb_ctrl_btg(btc->hal, is_btg);
}

static void _set_bt_slot_req(struct btc_t *btc)
{
	struct btc_bt_link_info *b = &btc->cx.bt.link_info;
	u8 len = 0;

	/*  don't change bt slot req state during RFK for p2p/mcc case*/
	if (!run_rsn("_ntfy_wl_rfk") &&
	    btc->cx.wl.rfk_info.state == BTC_WRFK_STOP)
		btc->bt_req_en = false;

	/* enable bt-slot req if ext-slot-control  */
	if (btc->dm.tdma_now.type == CXTDMA_OFF &&
	    btc->dm.tdma_now.ext_ctrl == CXECTL_EXT)
	    	btc->bt_req_en = true;

	if (!btc->bt_req_en) {
	    	len = 0;
	} else {
		if (b->a2dp_desc.exist || b->a2dp_desc.active ||
		   (b->profile_cnt.now == 1 &&
		    b->hid_desc.pair_cnt == 1 &&
		    b->multi_link.now)) /*a2dp, hid + acl-idle  */ {
		        if (b->profile_cnt.now >= 2)
				len = BTC_BSLOT_A2DP_HID;
			else
				len = BTC_BSLOT_A2DP;
		} else if (b->pan_desc.exist || b->status.map.inq_pag) {
			len = BTC_BSLOT_INQ;
		} else {
			len = BTC_BSLOT_IDLE;
		}
	}

	if (len == btc->bt_req_len)
		return;

	btc->bt_req_len = len;

	hal_btc_send_event(btc, (u8*)&btc->bt_req_len, 4, BTC_HMSG_SET_BT_REQ_SLOT);

	PHL_INFO("[BTC], %s(): bt_req_len=%d\n", __func__, btc->bt_req_len);
}

static void _set_bt_rx_agc(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	struct btc_bt_info *bt = &btc->cx.bt;
	bool bt_hi_lna_rx = false;

	if (wl_rinfo->link_mode != BTC_WLINK_NOLINK && btc->dm.wl_btg_rx)
		bt_hi_lna_rx = true;

	if (bt_hi_lna_rx == bt->hi_lna_rx)
		return;

	/* true: bt use Hi-LNA rx gain table (f/e/3/2) in -3x~-9xdBm for co-rx
	 * false: bt use original rx gain table (f/b/7/3/2)
	 */

	_write_scbd(btc, BTC_WSCB_BT_HILNA, bt_hi_lna_rx);
}

static void _set_bt_rx_scan_pri(struct btc_t *btc)
{
	struct btc_bt_info *bt = &btc->cx.bt;

	_write_scbd(btc, BTC_WSCB_RXSCAN_PRI, (bool)(!!bt->scan_rx_low_pri));
}

static void _set_halmac_tx_limit(struct btc_t *btc)
{
	struct btc_cx *cx = &btc->cx;
	struct btc_dm *dm = &btc->dm;
	struct btc_wl_info *wl = &cx->wl;
	struct btc_bt_info *bt = &cx->bt;
	struct btc_bt_link_info *b = &bt->link_info;
	struct btc_bt_hfp_desc *hfp = &b->hfp_desc;
	struct btc_bt_hid_desc *hid = &b->hid_desc;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	struct btc_wl_link_info *plink = NULL;
	u8 mode = wl_rinfo->link_mode, i;
	u8 tx_retry = 0;
	u32 tx_time = 0;
	u16 enable = 0;
	bool reenable = false;

	if (btc->ctrl.manual)
		return;

	if (btc->dm.freerun || btc->ctrl.igno_bt || b->profile_cnt.now == 0 ||
	    mode == BTC_WLINK_5G || mode == BTC_WLINK_NOLINK) {
		enable = 0;
		tx_time = BTC_MAX_TX_TIME_DEF;
		tx_retry = BTC_MAX_TX_RETRY_DEF;
	} else if ((hfp->exist && hid->exist) || hid->pair_cnt > 1) {
		enable = 1;
		tx_time = BTC_MAX_TX_TIME_L2;
		tx_retry = BTC_MAX_TX_RETRY_L1;
	} else if (hfp->exist || hid->exist) {
		enable = 1;
		tx_time = BTC_MAX_TX_TIME_L3;
		tx_retry = BTC_MAX_TX_RETRY_L1;
	} else {
		enable = 0;
		tx_time = BTC_MAX_TX_TIME_DEF;
		tx_retry = BTC_MAX_TX_RETRY_DEF;
	}

	if (dm->wl_tx_limit.enable == enable &&
	    dm->wl_tx_limit.tx_time == tx_time &&
	    dm->wl_tx_limit.tx_retry == tx_retry)
		return;

	if (!dm->wl_tx_limit.enable && enable)
		reenable = true;

	dm->wl_tx_limit.enable = enable;
	dm->wl_tx_limit.tx_time = tx_time;
	dm->wl_tx_limit.tx_retry = tx_retry;

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		plink = &wl->link_info[i];

		if (!plink->connected)
			continue;

		/* backup the original tx time before tx-limit on */
		if (reenable) {
			rtw_hal_mac_get_tx_time(btc->hal,
						(u8)plink->mac_id,
						&plink->tx_time);
			rtw_hal_mac_get_tx_retry_limit(btc->hal,
						       (u8)plink->mac_id,
						       &plink->tx_retry);
		}

		/* restore the original tx time if no tx-limit */
		if (!enable) {
			rtw_hal_mac_set_tx_time(btc->hal, 1, 1,
						(u8)plink->mac_id,
						plink->tx_time);
			rtw_hal_mac_set_tx_retry_limit(btc->hal, 1, 1,
						       (u8)plink->mac_id,
						       plink->tx_retry);
		} else {
			rtw_hal_mac_set_tx_time(btc->hal, 1, 0,
						(u8)plink->mac_id, tx_time);
			rtw_hal_mac_set_tx_retry_limit(btc->hal, 1, 0,
						       (u8)plink->mac_id,
						       tx_retry);
		}
	}
}

static bool _check_freerun(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	struct btc_bt_link_info *bt_linfo = &bt->link_info;
	struct btc_bt_hid_desc *hid = &bt_linfo->hid_desc;
	u8 max_role_cnt = BTC_TDMA_WLROLE_MAX;

	if (btc->hal->dbcc_en)
		max_role_cnt++;

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) {
		btc->dm.trx_para_level = 0;
		return false;
	}

	/* The below is dedicated antenna case */

	if (wl_rinfo->connect_cnt > max_role_cnt) {
		btc->dm.trx_para_level = 5;
		return true;
	}

	if (bt_linfo->profile_cnt.now == 0) {
		btc->dm.trx_para_level = 5;
		return true;
	}

	if (hid->pair_cnt > BTC_TDMA_BTHID_MAX) {
		btc->dm.trx_para_level = 5;
		return true;
	}

	/* Todo: get isolation by BT psd */
	if (btc->mdinfo.ant.isolation >= BTC_FREERUN_ANTISO_MIN) {
		btc->dm.trx_para_level = 5;
		return true;
	}

	if (!wl->status.map.busy) {/* wl idle -> freerun */
		btc->dm.trx_para_level = 5;
		return true;
	} else if (wl->rssi_level > 1) {/* WL rssi < 50% (-60dBm) */
		btc->dm.trx_para_level = 0;
		return false;
	} else if (wl->status.map.traffic_dir & BIT(TRAFFIC_UL)) { /* uplink */
		if (wl->rssi_level == 0 && bt_linfo->rssi > 31) {
			btc->dm.trx_para_level = 6;
			return true;
		} else if (wl->rssi_level == 1 && bt_linfo->rssi > 36) {
			btc->dm.trx_para_level = 7;
			return true;
		} else {
			btc->dm.trx_para_level = 0;
			return false;
		}
	} else if (wl->status.map.traffic_dir & BIT(TRAFFIC_DL)) { /*downlink*/
		if (bt_linfo->rssi > 28) {
			btc->dm.trx_para_level = 6;
			return true;
		}
	}

	btc->dm.trx_para_level = 0;
	return false;
}

static void _set_policy(struct btc_t *btc, u16 policy_type, const char* action)
{
	struct btc_dm *dm = &btc->dm;
	struct fbtc_tdma *t = &dm->tdma;
	struct fbtc_slot *s = dm->slot;
	struct btc_wl_role_info *wl_rinfo = &btc->cx.wl.role_info;
	struct btc_bt_hid_desc *hid = &btc->cx.bt.link_info.hid_desc;
	u8 type, null_role = 0;
	u16 dur = 0;
	u32 tbl_w1, tbl_b1, tbl_b4;
	bool mode = NM_EXEC;

	type = (u8)((policy_type & bMASKB1) >> 8);

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) {
		if (btc->cx.wl.status.map._4way)
			tbl_w1 = cxtbl[1];
		else if (hid->exist && hid->type == BTC_HID_218)
			tbl_w1 = cxtbl[7]; /* Ack/BA no break bt Hi-Pri-rx */
		else
			tbl_w1 = cxtbl[8];

		if (dm->leak_ap &&
		    (type == BTC_CXP_PFIX || type == BTC_CXP_PAUTO2)) {
			tbl_b1 = cxtbl[3];
			tbl_b4 = cxtbl[3];
		} else if (hid->exist && hid->type == BTC_HID_218) {
			tbl_b1 = cxtbl[4]; /* Ack/BA no break bt Hi-Pri-rx */
			tbl_b4 = cxtbl[4];
		} else {
			tbl_b1 = cxtbl[2];
			tbl_b4 = cxtbl[2];
		}
	} else {
		tbl_w1 = cxtbl[16]; /* 19 for HID exist */
		tbl_b1 = cxtbl[17];
		tbl_b4 = cxtbl[17];
	}

	switch(type) {
	case BTC_CXP_USERDEF0:
		mode = FC_EXEC;
		_tdma_cpy(t, &t_def[CXTD_OFF]);
		_slot_cpy(&s[CXST_OFF], &s_def[CXST_OFF]);
		_slot_set_tbl(CXST_OFF, cxtbl[2]);
		break;
	case BTC_CXP_OFF: /* TDMA off */
		_write_scbd(btc, BTC_WSCB_TDMA, false);
		_tdma_cpy(t, &t_def[CXTD_OFF]);
		_slot_cpy(&s[CXST_OFF], &s_def[CXST_OFF]);

		switch (policy_type) {
		case BTC_CXP_OFF_BT:
			_slot_set_tbl(CXST_OFF, cxtbl[2]);
			break;
		case BTC_CXP_OFF_WL:
			_slot_set_tbl(CXST_OFF, cxtbl[1]);
			break;
		case BTC_CXP_OFF_EQ0:
			_slot_set_tbl(CXST_OFF, cxtbl[0]);
			break;
		case BTC_CXP_OFF_EQ1:
			_slot_set_tbl(CXST_OFF, cxtbl[16]);
			break;
		case BTC_CXP_OFF_EQ2:
			_slot_set_tbl(CXST_OFF, cxtbl[17]);
			break;
		case BTC_CXP_OFF_EQ3:
			_slot_set_tbl(CXST_OFF, cxtbl[18]);
			break;
		case BTC_CXP_OFF_BWB0:
			_slot_set_tbl(CXST_OFF, cxtbl[5]);
			break;
		case BTC_CXP_OFF_BWB1:
			_slot_set_tbl(CXST_OFF, cxtbl[8]);
			break;
		case BTC_CXP_OFF_BWB2:
			_slot_set_tbl(CXST_OFF, cxtbl[7]);
			break;
		}
		break;
	case BTC_CXP_OFFB: /* TDMA off + beacon protect */
		_write_scbd(btc, BTC_WSCB_TDMA, false);
		_tdma_cpy(t, &t_def[CXTD_OFF_B2]);
		_slot_cpy(&s[CXST_OFF], &s_def[CXST_OFF]);
		switch (policy_type) {
		case BTC_CXP_OFFB_BWB0:
			_slot_set_tbl(CXST_OFF, cxtbl[8]);
			break;
		}
		break;
	case BTC_CXP_OFFE: /* TDMA off + beacon protect + Ext_control */
		_write_scbd(btc, BTC_WSCB_TDMA, true);
		_tdma_cpy(t, &t_def[CXTD_OFF_EXT]);

		if (wl_rinfo->link_mode == BTC_WLINK_2G_SCC &&
	    	    dm->wl_scc.ebt_null) {
			_tdma_set_rxflctrl(1);
			_tdma_set_txflctrl(1);
			if ((wl_rinfo->mrole_noa_duration > 0) &&
			    (wl_rinfo->mrole_noa_duration +
			     2 * BTC_NULL1_EBT_EARLY_T < BTC_SCC_CYCLE))
				dur = BTC_SCC_CYCLE - BTC_NULL1_EBT_EARLY_T -
				      wl_rinfo->mrole_noa_duration;
		}

		switch (policy_type) {
		case BTC_CXP_OFFE_2GBWISOB: /* for normal-case */
			_slot_set(CXST_E2G, dur, tbl_w1, SLOT_ISO);
			_slot_cpy(&s[CXST_EBT], &s_def[CXST_EBT]);
			break;
		case BTC_CXP_OFFE_2GISOB: /* for bt no-link */
			_slot_set(CXST_E2G, dur, cxtbl[1], SLOT_ISO);
			_slot_cpy(&s[CXST_EBT], &s_def[CXST_EBT]);
			break;
		case BTC_CXP_OFFE_2GBWMIXB: /* for GC-only NOA < bt_slot_req */
			_slot_set(CXST_E2G, dur, tbl_w1, SLOT_MIX);
			_slot_cpy(&s[CXST_EBT], &s_def[CXST_EBT]);
			break;
		case BTC_CXP_OFFE_2GBWMIXB2: /* for GC+STA NOA < bt_slot_req */
			_slot_set(CXST_E2G, dur, tbl_w1, SLOT_MIX);
			_slot_set(CXST_EBT, BTC_BSLOT_A2DP_HID,
				  cxtbl[2], SLOT_MIX);
			break;
		case BTC_CXP_OFFE_WL: /*for 4-way  */
			_slot_set(CXST_E2G, dur, cxtbl[1], SLOT_MIX);
			_slot_set(CXST_EBT, 0, cxtbl[1], SLOT_MIX);
			break;
		}

		_slot_cpy(&s[CXST_E5G], &s_def[CXST_E5G]);
		_slot_cpy(&s[CXST_ENULL], &s_def[CXST_ENULL]);
		break;
	case BTC_CXP_FIX: /* TDMA Fix-Slot */
		_write_scbd(btc, BTC_WSCB_TDMA, true);
		_tdma_cpy(t, &t_def[CXTD_FIX]);
		switch (policy_type) {
		case BTC_CXP_FIX_TD3030: /* W1:B1 = 30:30 */
			_slot_set(CXST_W1, 30, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 30, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TD5050: /* W1:B1 = 50:50 */
			_slot_set(CXST_W1, 50, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 50, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TD2030: /* W1:B1 = 20:30 */
			_slot_set(CXST_W1, 20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 30, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TD4010: /* W1:B1 = 40:10 */
			_slot_set(CXST_W1, 40, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 10, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TD4010ISO: /* W1:B1 = 40:10 for WL busy/scan */
			_slot_set(CXST_W1, 40, cxtbl[1], SLOT_ISO);
			_slot_set(CXST_B1, 10, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TD7010: /* W1:B1 = 70:10 */
			_slot_set(CXST_W1, 70, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 10, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TD2060: /* W1:B1 = 20:60 */
			_slot_set(CXST_W1, 20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 60, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TD3060: /* W1:B1 = 30:60 */
			_slot_set(CXST_W1, 30, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 60, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TD2080: /* W1:B1 = 20:80 */
			_slot_set(CXST_W1, 20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 80, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_FIX_TDW1B1: /* W1:B1 = user-define */
			_slot_set(CXST_W1, dm->slot_dur[CXST_W1],
				  tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, dm->slot_dur[CXST_B1],
				  tbl_b1, SLOT_MIX);
			break;
		}
		break;
	case BTC_CXP_PFIX: /* PS-TDMA Fix-Slot */
		_write_scbd(btc, BTC_WSCB_TDMA, true);
		_tdma_cpy(t, &t_def[CXTD_PFIX]);

		switch (policy_type) {
		case BTC_CXP_PFIX_TD3030: /* W1:B1 = 30:30 */
			_slot_set(CXST_W1, 30, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 30, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PFIX_TD5050: /* W1:B1 = 50:50 */
			_slot_set(CXST_W1, 50, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 50, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PFIX_TD2030: /* W1:B1 = 20:30 */
			_slot_set(CXST_W1, 20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 30, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PFIX_TD2060: /* W1:B1 = 20:60 */
			_slot_set(CXST_W1, 20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 60, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PFIX_TD3070: /* W1:B1 = 30:60 */
			_slot_set(CXST_W1, 30, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 60, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PFIX_TD2080: /* W1:B1 = 20:80 */
			_slot_set(CXST_W1, 20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, 80, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PFIX_TDW1B1: /* W1:B1 = user-define */
			_slot_set(CXST_W1, dm->slot_dur[CXST_W1],
				  tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, dm->slot_dur[CXST_B1],
				  tbl_b1, SLOT_MIX);
			break;
		}
		break;
	case BTC_CXP_AUTO: /* TDMA Auto-Slot */
		_write_scbd(btc, BTC_WSCB_TDMA, true);
		_tdma_cpy(t, &t_def[CXTD_AUTO]);
		switch (policy_type) {
		case BTC_CXP_AUTO_TD50B1: /* W1:B1 = 50:BTC_B1_MAX */
			_slot_set(CXST_W1,  50, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_AUTO_TD60B1: /* W1:B1 = 60:BTC_B1_MAX */
			_slot_set(CXST_W1,  60, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_AUTO_TD20B1:  /* W1:B1 = 20:BTC_B1_MAX */
			_slot_set(CXST_W1,  20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_AUTO_TDW1B1: /* W1:B1 = user-define */
			_slot_set(CXST_W1, dm->slot_dur[CXST_W1],
				  tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, dm->slot_dur[CXST_B1],
				  tbl_b1, SLOT_MIX);
			break;
		}
		break;
	case BTC_CXP_PAUTO: /* PS-TDMA Auto-Slot */
		_write_scbd(btc, BTC_WSCB_TDMA, true);
		_tdma_cpy(t, &t_def[CXTD_PAUTO]);
		switch (policy_type) {
		case BTC_CXP_PAUTO_TD50B1: /* W1:B1 = 50:BTC_B1_MAX */
			_slot_set(CXST_W1,  50, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PAUTO_TD60B1: /* W1:B1 = 60:BTC_B1_MAX */
			_slot_set(CXST_W1,  60, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PAUTO_TD20B1:  /* W1:B1 = 20:BTC_B1_MAX */
			_slot_set(CXST_W1,  20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			break;
		case BTC_CXP_PAUTO_TDW1B1: /* W1:B1 = user-define */
			_slot_set(CXST_W1, dm->slot_dur[CXST_W1],
				  tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, dm->slot_dur[CXST_B1],
				  tbl_b1, SLOT_MIX);
			break;
		}
		break;
	case BTC_CXP_AUTO2: /* TDMA Auto-Slot2 */
		_write_scbd(btc, BTC_WSCB_TDMA, true);
		_tdma_cpy(t, &t_def[CXTD_AUTO2]);
		switch (policy_type) {
		case BTC_CXP_AUTO2_TD3050: /* W1:B4 = 30:50 */
			_slot_set(CXST_W1,  30, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  50, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_AUTO2_TD3070: /* W1:B4 = 30:70 */
			_slot_set(CXST_W1,  30, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  70, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_AUTO2_TD5050: /* W1:B4 = 50:50 */
			_slot_set(CXST_W1,  50, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  50, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_AUTO2_TD6060: /* W1:B4 = 60:60 */
			_slot_set(CXST_W1,  60, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  60, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_AUTO2_TD2080: /* W1:B4 = 20:80 */
			_slot_set(CXST_W1,  20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  80, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_AUTO2_TDW1B4: /* W1:B1 = user-define */
			_slot_set(CXST_W1, dm->slot_dur[CXST_W1],
				  tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, dm->slot_dur[CXST_B1],
				  tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4, dm->slot_dur[CXST_B4],
				  tbl_b4, SLOT_MIX);
			break;
		}
		break;
	case BTC_CXP_PAUTO2: /* PS-TDMA Auto-Slot2 */
		_write_scbd(btc, BTC_WSCB_TDMA, true);
		_tdma_cpy(t, &t_def[CXTD_PAUTO2]);
		switch (policy_type) {
		case BTC_CXP_PAUTO2_TD3050: /* W1:B4 = 30:50 */
			_slot_set(CXST_W1,  30, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  50, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_PAUTO2_TD3070: /* W1:B4 = 30:70 */
			_slot_set(CXST_W1,  30, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, cxtbl[3], SLOT_MIX);
			_slot_set(CXST_B4,  70, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_PAUTO2_TD5050: /* W1:B4 = 50:50 */
			_slot_set(CXST_W1,  50, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  50, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_PAUTO2_TD6060: /* W1:B4 = 60:60 */
			_slot_set(CXST_W1,  60, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  60, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_PAUTO2_TD2080: /* W1:B4 = 20:80 */
			_slot_set(CXST_W1,  20, tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, BTC_B1_MAX, tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4,  80, tbl_b4, SLOT_MIX);
			break;
		case BTC_CXP_PAUTO2_TDW1B4: /* W1:B1 = user-define */
			_slot_set(CXST_W1, dm->slot_dur[CXST_W1],
				  tbl_w1, SLOT_ISO);
			_slot_set(CXST_B1, dm->slot_dur[CXST_B1],
				  tbl_b1, SLOT_MIX);
			_slot_set(CXST_B4, dm->slot_dur[CXST_B4],
				  tbl_b4, SLOT_MIX);
			break;
		}
		break;
	}

	if (wl_rinfo->link_mode == BTC_WLINK_2G_SCC && dm->tdma.rxflctrl) {
		null_role = (dm->wl_scc.null_role1 & 0xf) +
			    ((dm->wl_scc.null_role2 & 0xf) << 4);
		_tdma_set_flctrl_role(null_role);
	}

	/* enter leak_slot after each null-1 */
	if (dm->leak_ap && dm->tdma.leak_n > 1)
		_tdma_set_lek(1);

	if (run_rsn("_cmd_set_coex"))
		mode = FC_EXEC;

	_update_poicy(btc, mode, policy_type, action);
}

void _set_gnt_wl(struct btc_t *btc, u8 phy_map, u8 state)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_dm *dm = &btc->dm;
	struct btc_gnt_ctrl *g = dm->gnt;
	u8 i;

	if (phy_map > BTC_PHY_ALL)
		return;

	for (i = 0; i < BTC_PHY_MAX; i++) {
		if (!(phy_map & BIT(i)))
			continue;

		switch (state) {
		case BTC_GNT_HW:
			g[i].gnt_wl_sw_en = 0;
			g[i].gnt_wl = 0;
			break;
		case BTC_GNT_SW_LO:
			g[i].gnt_wl_sw_en = 1;
			g[i].gnt_wl = 0;
			break;
		case BTC_GNT_SW_HI:
			g[i].gnt_wl_sw_en = 1;
			g[i].gnt_wl = 1;
			break;
		}
	}

	rtw_hal_mac_set_grant(h, (u8*)g);
}

void _set_gnt_bt(struct btc_t *btc, u8 phy_map, u8 state)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_dm *dm = &btc->dm;
	struct btc_gnt_ctrl *g = dm->gnt;
	u8 i;

	if (phy_map > BTC_PHY_ALL)
		return;

	for (i = 0; i < BTC_PHY_MAX; i++) {
		if (!(phy_map & BIT(i)))
			continue;

		switch (state) {
		case BTC_GNT_HW:
			g[i].gnt_bt_sw_en = 0;
			g[i].gnt_bt = 0;
			break;
		case BTC_GNT_SW_LO:
			g[i].gnt_bt_sw_en = 1;
			g[i].gnt_bt = 0;
			break;
		case BTC_GNT_SW_HI:
			g[i].gnt_bt_sw_en = 1;
			g[i].gnt_bt = 1;
			break;
		}
	}

	rtw_hal_mac_set_grant(h, (u8*)g);
}

static void _set_bt_plut(struct btc_t *btc, u8 phy_map, u8 tx_val, u8 rx_val)
{
	if (phy_map & BTC_PHY_0) {
		rtw_hal_mac_set_polluted(btc->hal, HW_PHY_0, tx_val, rx_val);
		btc->cx.wl.bt_polut_type[HW_PHY_0] = tx_val;
	}

	if (!btc->hal->dbcc_en)
		return;

	if (phy_map & BTC_PHY_1) {
		rtw_hal_mac_set_polluted(btc->hal, HW_PHY_1, tx_val, rx_val);
		btc->cx.wl.bt_polut_type[HW_PHY_1] = tx_val;
	}
}

static void _set_ant(struct btc_t *btc, bool force_exec, u8 phy_map, u8 type)
{
	struct btc_dm *dm = &btc->dm;
	struct btc_cx *cx = &btc->cx;
	struct btc_wl_info *wl = &cx->wl;
	struct btc_bt_info *bt = &cx->bt;
	struct btc_wl_dbcc_info *wl_dinfo = &wl->dbcc_info;
	u8 gnt_wl_ctrl, gnt_bt_ctrl, plt_ctrl, i, b2g = 0;
	u32 ant_path_type;

	ant_path_type = ((phy_map << 8) + type);

	if (run_rsn("_cmd_set_coex") || run_rsn("_ntfy_power_off") ||
	    run_rsn("_ntfy_radio_state"))
		force_exec = FC_EXEC;

	if (!force_exec && (ant_path_type == dm->set_ant_path)) {
		 PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): return by no change!!\n",
		 	   __func__);
		return;
	} else if (bt->rfk_info.map.run) {
		PHL_INFO("[BTC], %s(): return by bt rfk!!\n", __func__);
		return;
	} else if (!run_rsn("_ntfy_wl_rfk") &&
		   wl->rfk_info.state != BTC_WRFK_STOP) {
		PHL_INFO("[BTC], %s(): return by wl rfk!!\n", __func__);
		return;
	}

	dm->set_ant_path = ant_path_type;

	PHL_INFO("[BTC], %s(): path=0x%x, set_type=%s\n", __func__,
		 phy_map, id_to_str(BTC_STR_ANTPATH, dm->set_ant_path & 0xff));

	switch (type){
	case BTC_ANT_WPOWERON:
		_set_cx_ctrl(btc, BTC_CTRL_BY_BT);
		break;
	case BTC_ANT_WINIT:
		/* To avoid BT MP driver case (bt_enable but no mailbox) */
		if (bt->enable.now && bt->run_patch_code) {
			_set_gnt_wl(btc, phy_map, BTC_GNT_SW_LO);
			_set_gnt_bt(btc, phy_map, BTC_GNT_SW_HI);
		} else {
			_set_gnt_wl(btc, phy_map, BTC_GNT_SW_HI);
			_set_gnt_bt(btc, phy_map, BTC_GNT_SW_LO);
		}
		_set_cx_ctrl(btc, BTC_CTRL_BY_WL);
		_set_bt_plut(btc, BTC_PHY_ALL, BTC_PLT_BT, BTC_PLT_BT);
		break;
	case BTC_ANT_WONLY:
		_set_gnt_wl(btc, phy_map, BTC_GNT_SW_HI);
		_set_gnt_bt(btc, phy_map, BTC_GNT_SW_LO);
		_set_cx_ctrl(btc, BTC_CTRL_BY_WL);
		_set_bt_plut(btc, BTC_PHY_ALL, BTC_PLT_NONE, BTC_PLT_NONE);
		break;
	case BTC_ANT_WOFF:
		_set_cx_ctrl(btc, BTC_CTRL_BY_BT);
		_set_bt_plut(btc, BTC_PHY_ALL, BTC_PLT_NONE, BTC_PLT_NONE);
		break;
	case BTC_ANT_W2G:
		_set_cx_ctrl(btc, BTC_CTRL_BY_WL);
		if (btc->hal->dbcc_en) {
			for (i = 0; i < HW_PHY_MAX; i++) {
				b2g = (wl_dinfo->real_band[i] == BAND_ON_24G);

				gnt_wl_ctrl = (b2g? BTC_GNT_HW : BTC_GNT_SW_HI);
				_set_gnt_wl(btc, BIT(i), gnt_wl_ctrl);

				gnt_bt_ctrl = BTC_GNT_HW;
				_set_gnt_bt(btc, BIT(i), gnt_bt_ctrl);

				plt_ctrl = (b2g? BTC_PLT_BT : BTC_PLT_NONE);
				_set_bt_plut(btc, BIT(i), plt_ctrl, plt_ctrl);
			}
		} else {
			_set_gnt_wl(btc, phy_map, BTC_GNT_HW);
			_set_gnt_bt(btc, phy_map, BTC_GNT_HW);
			plt_ctrl = BTC_PLT_BT;
			_set_bt_plut(btc, BTC_PHY_ALL, plt_ctrl, plt_ctrl);
		}
		break;
	case BTC_ANT_W5G:
		_set_cx_ctrl(btc, BTC_CTRL_BY_WL);
		_set_gnt_wl(btc, phy_map, BTC_GNT_SW_HI);
		_set_gnt_bt(btc, phy_map, BTC_GNT_HW);
		_set_bt_plut(btc, BTC_PHY_ALL, BTC_PLT_NONE, BTC_PLT_NONE);
		break;
	case BTC_ANT_W25G:
		_set_cx_ctrl(btc, BTC_CTRL_BY_WL);
		if (btc->hal->dbcc_en) {
			/* if BAND_ON_24G, the other role in the sane PHY = 5G*/
			for (i = 0; i < HW_PHY_MAX; i++) {
				b2g = (wl_dinfo->real_band[i] == BAND_ON_24G);

				gnt_wl_ctrl = (b2g? BTC_GNT_HW : BTC_GNT_SW_HI);
				_set_gnt_wl(btc, BIT(i), gnt_wl_ctrl);

				gnt_bt_ctrl = BTC_GNT_HW;
				_set_gnt_bt(btc, BIT(i), gnt_bt_ctrl);

				plt_ctrl = (b2g? BTC_PLT_GNT_WL : BTC_PLT_NONE);
				_set_bt_plut(btc, BIT(i), plt_ctrl, plt_ctrl);
			}
		} else {
			_set_gnt_wl(btc, phy_map, BTC_GNT_HW);
			_set_gnt_bt(btc, phy_map, BTC_GNT_HW);
			plt_ctrl = BTC_PLT_GNT_WL;
			_set_bt_plut(btc, BTC_PHY_ALL, plt_ctrl, plt_ctrl);
		}
		break;
	case BTC_ANT_FREERUN:
		_set_cx_ctrl(btc, BTC_CTRL_BY_WL);
		_set_gnt_wl(btc, phy_map, BTC_GNT_SW_HI);
		_set_gnt_bt(btc, phy_map, BTC_GNT_SW_HI);
		_set_bt_plut(btc, BTC_PHY_ALL, BTC_PLT_NONE, BTC_PLT_NONE);
		break;
	case BTC_ANT_WRFK:
		_set_cx_ctrl(btc, BTC_CTRL_BY_WL);
		_set_gnt_wl(btc, phy_map, BTC_GNT_SW_HI);
		_set_gnt_bt(btc, phy_map, BTC_GNT_SW_LO);
		_set_bt_plut(btc, phy_map, BTC_PLT_NONE, BTC_PLT_NONE);
		break;
	case BTC_ANT_BRFK:
		_set_cx_ctrl(btc, BTC_CTRL_BY_BT);
		_set_gnt_wl(btc, phy_map, BTC_GNT_SW_LO);
		_set_gnt_bt(btc, phy_map, BTC_GNT_SW_HI);
		_set_bt_plut(btc, phy_map, BTC_PLT_NONE, BTC_PLT_NONE);
		break;
	}
}

void _action_wl_only(struct btc_t *btc)
{
	PHL_INFO("[BTC], %s !!\n", __func__);

	_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_WONLY);
	_set_policy(btc, BTC_CXP_OFF_BT, __func__);
}

void _action_wl_init(struct btc_t *btc)
{
	PHL_INFO("[BTC], %s !!\n", __func__);

	_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_WINIT);
	_set_policy(btc, BTC_CXP_OFF_BT, __func__);
}

void _action_wl_off(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;

	PHL_INFO("[BTC], %s !!\n", __func__);

	/* don't set PTA control if LPS */
	if (wl->status.map.rf_off || btc->dm.bt_only) {
		_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_WOFF);
	} else if (wl->status.map.lps == 2) {
		if (wl->role_info.link_mode == BTC_WLINK_5G)
			_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_W5G);
		else
			_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	}

	if (wl->role_info.link_mode == BTC_WLINK_5G) {
		_set_policy(btc, BTC_CXP_OFF_EQ0, __func__);
	} else if (wl->status.map.lps == 2) {
		if (btc->cx.bt.link_info.a2dp_desc.active)
			_set_policy(btc, BTC_CXP_OFF_BT, __func__);
		else
			_set_policy(btc, BTC_CXP_OFF_BWB1, __func__);
	} else {
		_set_policy(btc, BTC_CXP_OFF_BT, __func__);
	}
}

void _action_freerun(struct btc_t *btc)
{
	PHL_INFO("[BTC], %s !!\n", __func__);
	btc->dm.freerun = true;

	_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_FREERUN);
	_set_policy(btc, BTC_CXP_OFF_BT, __func__);
}

void _action_bt_whql(struct btc_t *btc)
{
	PHL_INFO("[BTC], %s !!\n", __func__);

	_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, BTC_CXP_OFF_BT, __func__);
}

void _action_bt_rfk(struct btc_t *btc)
{
	PHL_INFO("[BTC], %s !!\n", __func__);

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_BRFK);
	_set_policy(btc, BTC_CXP_OFF_BT, __func__);
}

void _action_bt_off(struct btc_t *btc)
{
	PHL_INFO("[BTC], %s !!\n", __func__);

	_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_WONLY);
	_set_policy(btc, BTC_CXP_OFF_BT, __func__);
}

static void _action_bt_idle(struct btc_t *btc)
{
	struct btc_bt_info *bt = &btc->cx.bt;
	struct btc_bt_link_info *b = &btc->cx.bt.link_info;
	u16 policy_type = BTC_CXP_OFF_BT;

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) { /* shared-antenna */
		switch(btc->cx.state_map) {
		case BTC_WBUSY_BNOSCAN: /* wl-busy + bt idle*/
		case BTC_WSCAN_BNOSCAN: /* wl-scan + bt-idle */
			if (b->status.map.connect)
				policy_type = BTC_CXP_FIX_TD4010;
			else
				policy_type = BTC_CXP_FIX_TD4010ISO;
			break;
		case BTC_WBUSY_BSCAN: /*wl-busy + bt-inq */
			if (bt->pag)
				policy_type = BTC_CXP_PFIX_TD2080;
			else
				policy_type = BTC_CXP_PFIX_TD5050;
			break;
		case BTC_WSCAN_BSCAN: /* wl-scan + bt-inq */
			if (bt->pag)
				policy_type = BTC_CXP_FIX_TD2080;
			else
				policy_type = BTC_CXP_FIX_TD5050;
			break;
		case BTC_WLINKING: /* wl-connecting + bt-inq or bt-idle */
			if (bt->pag)
				policy_type = BTC_CXP_FIX_TD2080;
			else
				policy_type = BTC_CXP_FIX_TD7010;
			break;
		case BTC_WIDLE:  /* wl-idle + bt-idle */
			policy_type = BTC_CXP_OFF_BWB1;
			break;
		}
	} else { /* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ0;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_hfp(struct btc_t *btc)
{
	u16 policy_type = BTC_CXP_OFF_BT;

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) { /* shared-antenna */
		if (btc->cx.wl.status.map._4way)
			policy_type = BTC_CXP_OFF_WL;
		else
			policy_type = BTC_CXP_OFF_BWB1;
	} else { /* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ2;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_hid(struct btc_t *btc)
{
	struct btc_bt_hid_desc *hid = &btc->cx.bt.link_info.hid_desc;
	u16 policy_type = BTC_CXP_OFF_BT;

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) { /* shared-antenna */
		if (btc->cx.wl.status.map._4way) {
			policy_type = BTC_CXP_OFF_WL;
		} else if (hid->type == BTC_HID_218) {
			btc->cx.bt.scan_rx_low_pri = true;
			policy_type = BTC_CXP_OFF_BWB2;
		} else {
			policy_type = BTC_CXP_OFF_BWB1;
		}
	} else { /* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ3;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_a2dp(struct btc_t *btc)
{
	struct btc_bt_link_info *bt_linfo = &btc->cx.bt.link_info;
	struct btc_bt_a2dp_desc a2dp = bt_linfo->a2dp_desc;
	struct btc_dm *dm = &btc->dm;
	u16 policy_type = BTC_CXP_OFF_BT;

	switch(btc->cx.state_map) {
	case BTC_WBUSY_BNOSCAN: /* wl-busy + bt-A2DP */
		if (a2dp.vendor_id == 0x4c || dm->leak_ap ||
		    bt_linfo->slave_role) {
			if (bt_linfo->slave_role)
				dm->slot_dur[CXST_W1] = 20;
			else
				dm->slot_dur[CXST_W1] = 40;
			dm->slot_dur[CXST_B1] = BTC_B1_MAX;
			policy_type = BTC_CXP_PAUTO_TDW1B1;
		} else {
			policy_type = BTC_CXP_PAUTO_TD50B1;
		}
		break;
	case BTC_WBUSY_BSCAN: /* wl-busy + bt-inq + bt-A2DP */
		policy_type = BTC_CXP_PAUTO2_TD3050;
		break;
	case BTC_WSCAN_BSCAN: /* wl-scan + bt-inq + bt-A2DP */
		policy_type = BTC_CXP_AUTO2_TD3050;
		break;
	case BTC_WSCAN_BNOSCAN: /* wl-scan + bt-A2DP */
	case BTC_WLINKING: /* wl-connecting + bt-A2DP */
		if (a2dp.vendor_id == 0x4c || dm->leak_ap ||
		    bt_linfo->slave_role) {
			if (bt_linfo->slave_role)
				dm->slot_dur[CXST_W1] = 20;
			else
				dm->slot_dur[CXST_W1] = 40;
			dm->slot_dur[CXST_B1] = BTC_B1_MAX;
			policy_type = BTC_CXP_AUTO_TDW1B1;
		} else {
			policy_type = BTC_CXP_AUTO_TD50B1;
		}
		break;
	case BTC_WIDLE:  /* wl-idle + bt-A2DP */
		policy_type = BTC_CXP_AUTO_TD20B1;
		break;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_a2dpsink(struct btc_t *btc)
{
	u16 policy_type = BTC_CXP_OFF_BT;

	switch(btc->cx.state_map) {
	case BTC_WBUSY_BNOSCAN: /* wl-busy + bt-A2dp_Sink */
		policy_type = BTC_CXP_PFIX_TD2030;
		break;
	case BTC_WBUSY_BSCAN: /* wl-busy + bt-inq + bt-A2dp_Sink */
		policy_type = BTC_CXP_PFIX_TD2060;
		break;
	case BTC_WSCAN_BNOSCAN: /* wl-scan + bt-A2dp_Sink */
		policy_type = BTC_CXP_FIX_TD2030;
		break;
	case BTC_WSCAN_BSCAN: /* wl-scan + bt-inq + bt-A2dp_Sink */
		policy_type = BTC_CXP_FIX_TD2060;
		break;
	case BTC_WLINKING: /* wl-connecting + bt-A2dp_Sink */
		policy_type = BTC_CXP_FIX_TD3030;
		break;
	case BTC_WIDLE: /* wl-idle + bt-A2dp_Sink */
		policy_type = BTC_CXP_FIX_TD2080;
		break;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_pan(struct btc_t *btc)
{
	u16 policy_type = BTC_CXP_OFF_BT;

	switch(btc->cx.state_map) {
	case BTC_WBUSY_BNOSCAN: /* wl-busy + bt-PAN */
		policy_type = BTC_CXP_PFIX_TD5050;
		break;
	case BTC_WBUSY_BSCAN: /* wl-busy + bt-inq + bt-PAN */
		policy_type = BTC_CXP_PFIX_TD3070;
		break;
	case BTC_WSCAN_BNOSCAN: /* wl-scan + bt-PAN */
		policy_type = BTC_CXP_FIX_TD3030;
		break;
	case BTC_WSCAN_BSCAN: /* wl-scan + bt-inq + bt-PAN */
		policy_type = BTC_CXP_FIX_TD3060;
		break;
	case BTC_WLINKING: /* wl-connecting + bt-PAN */
		policy_type = BTC_CXP_FIX_TD4010ISO;
		break;
	case BTC_WIDLE: /* wl-idle + bt-pan */
		policy_type = BTC_CXP_PFIX_TD2080;
		break;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_a2dp_hid(struct btc_t *btc)
{
	struct btc_bt_link_info *bt_linfo = &btc->cx.bt.link_info;
	struct btc_bt_a2dp_desc a2dp = bt_linfo->a2dp_desc;
	struct btc_dm *dm = &btc->dm;
	u16 policy_type = BTC_CXP_OFF_BT;

	switch(btc->cx.state_map) {
	case BTC_WBUSY_BNOSCAN: /* wl-busy + bt-A2DP */
	case BTC_WIDLE:  /* wl-idle + bt-A2DP */
		if (a2dp.vendor_id == 0x4c || dm->leak_ap ||
		    bt_linfo->slave_role) {
			if (bt_linfo->slave_role)
				dm->slot_dur[CXST_W1] = 20;
			else
				dm->slot_dur[CXST_W1] = 40;
			dm->slot_dur[CXST_W1] = 40;
			dm->slot_dur[CXST_B1] = BTC_B1_MAX;
			policy_type = BTC_CXP_PAUTO_TDW1B1;
		} else {
			policy_type = BTC_CXP_PAUTO_TD50B1;
		}
		break;
	case BTC_WBUSY_BSCAN: /* wl-busy + bt-inq + bt-A2DP */
		policy_type = BTC_CXP_PAUTO2_TD3050;
		break;
	case BTC_WSCAN_BSCAN: /* wl-scan + bt-inq + bt-A2DP */
		policy_type = BTC_CXP_AUTO2_TD3050;
		break;
	case BTC_WSCAN_BNOSCAN: /* wl-scan + bt-A2DP */
	case BTC_WLINKING: /* wl-connecting + bt-A2DP */
		if (a2dp.vendor_id == 0x4c || dm->leak_ap ||
		    bt_linfo->slave_role) {
			if (bt_linfo->slave_role)
				dm->slot_dur[CXST_W1] = 20;
			else
				dm->slot_dur[CXST_W1] = 40;
			dm->slot_dur[CXST_B1] = BTC_B1_MAX;
			policy_type = BTC_CXP_AUTO_TDW1B1;
		} else {
			policy_type = BTC_CXP_AUTO_TD50B1;
		}
		break;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_a2dp_pan(struct btc_t *btc)
{
	u16 policy_type = BTC_CXP_OFF_BT;

	switch(btc->cx.state_map) {
	case BTC_WBUSY_BNOSCAN: /* wl-busy + bt-A2DP+PAN */
		policy_type = BTC_CXP_PAUTO2_TD3070;
		break;
	case BTC_WBUSY_BSCAN: /* wl-busy + bt-inq + bt-A2DP+PAN */
		policy_type = BTC_CXP_PAUTO2_TD3070;
		break;
	case BTC_WSCAN_BNOSCAN: /* wl-scan + bt-A2DP+PAN */
		policy_type = BTC_CXP_AUTO2_TD5050;
		break;
	case BTC_WSCAN_BSCAN: /* wl-scan + bt-inq + bt-A2DP+PAN */
		policy_type = BTC_CXP_AUTO2_TD3070;
		break;
	case BTC_WLINKING: /* wl-connecting + bt-A2DP+PAN */
		policy_type = BTC_CXP_AUTO2_TD3050;
		break;
	case BTC_WIDLE:  /* wl-idle + bt-A2DP+PAN */
		policy_type = BTC_CXP_PAUTO2_TD2080;
		break;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_pan_hid(struct btc_t *btc)
{
	u16 policy_type = BTC_CXP_OFF_BT;

	switch(btc->cx.state_map) {
	case BTC_WBUSY_BNOSCAN: /* wl-busy + bt-PAN+HID */
		policy_type = BTC_CXP_PFIX_TD3030;
		break;
	case BTC_WBUSY_BSCAN: /* wl-busy + bt-inq + bt-PAN+HID */
		policy_type = BTC_CXP_PFIX_TD3070;
		break;
	case BTC_WSCAN_BNOSCAN: /* wl-scan + bt-PAN+HID */
		policy_type = BTC_CXP_FIX_TD3030;
		break;
	case BTC_WSCAN_BSCAN: /* wl-scan + bt-inq + bt-PAN+HID */
		policy_type = BTC_CXP_FIX_TD3060;
		break;
	case BTC_WLINKING: /* wl-connecting + bt-PAN+HID */
		policy_type = BTC_CXP_FIX_TD4010;
		break;
	case BTC_WIDLE: /* wl-idle + bt-PAN+HID */
		policy_type = BTC_CXP_PFIX_TD2080;
		break;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

static void _action_bt_a2dp_pan_hid(struct btc_t *btc)
{
	u16 policy_type = BTC_CXP_OFF_BT;

	switch(btc->cx.state_map) {
	case BTC_WBUSY_BNOSCAN: /* wl-busy + bt-A2DP+PAN+HID */
		policy_type = BTC_CXP_PAUTO2_TD3070;
		break;
	case BTC_WBUSY_BSCAN: /* wl-busy + bt-inq + bt-A2DP+PAN+HID */
		policy_type = BTC_CXP_PAUTO2_TD3070;
		break;
	case BTC_WSCAN_BSCAN: /* wl-scan + bt-inq + bt-A2DP+PAN+HID */
		policy_type = BTC_CXP_AUTO2_TD3070;
		break;
	case BTC_WSCAN_BNOSCAN: /* wl-scan + bt-A2DP+PAN+HID */
	case BTC_WLINKING: /* wl-connecting + bt-A2DP+PAN+HID */
		policy_type = BTC_CXP_AUTO2_TD3050;
		break;
	case BTC_WIDLE:  /* wl-idle + bt-A2DP+PAN+HID */
		policy_type = BTC_CXP_PAUTO2_TD2080;
		break;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

void _action_wl_5g(struct btc_t *btc)
{
	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W5G);
	_set_policy(btc, BTC_CXP_OFF_EQ0, __func__);
}

void _action_wl_other(struct btc_t *btc)
{
	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED)
		_set_policy(btc, BTC_CXP_OFFB_BWB0, __func__);
	else
		_set_policy(btc, BTC_CXP_OFF_EQ0, __func__);
}

void _action_wl_idle(struct btc_t *btc)
{
	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED)
		_set_policy(btc, BTC_CXP_OFFB_BWB0, __func__);
	else
		_set_policy(btc, BTC_CXP_OFF_EQ0, __func__);
}

void _action_wl_nc(struct btc_t *btc)
{
	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, BTC_CXP_OFF_BT, __func__);
}

void _action_wl_rfk(struct btc_t *btc)
{
	struct btc_wl_rfk_info rfk = btc->cx.wl.rfk_info;

	if (rfk.state != BTC_WRFK_START)
		return;

	PHL_INFO("[BTC], %s(): band = %d\n", __func__, rfk.band);

	_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_WRFK);
	_set_policy(btc, BTC_CXP_OFF_WL, __func__);

#if 0
	if (rfk.band == BAND_ON_24G) {
		_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_WRFK);
		_set_policy(btc, BTC_CXP_OFF_WL, __func__);
	} else {
		_set_ant(btc, FC_EXEC, BTC_PHY_ALL, BTC_ANT_FREERUN);
		_set_policy(btc, BTC_CXP_OFF_EQ0, __func__);
	}
#endif
}

void _action_common(struct btc_t *btc)
{
	_set_halbb_btg_ctrl(btc);
	_set_halmac_tx_limit(btc);
	_set_bt_afh_info(btc);
	_set_bt_rx_agc(btc); /* must call after _set_halbb_btg_ctrl */
	_set_bt_slot_req(btc);
	_set_bt_rx_scan_pri(btc);
	_set_rf_trx_para(btc);
}

static void _action_by_bt(struct btc_t *btc)
{
	struct btc_bt_info *bt = &btc->cx.bt;
	struct btc_bt_link_info *bt_linfo = &bt->link_info;
	struct btc_bt_hid_desc hid = bt_linfo->hid_desc;
	struct btc_bt_a2dp_desc a2dp = bt_linfo->a2dp_desc;
	struct btc_bt_pan_desc pan = bt_linfo->pan_desc;
	u8 profile_map = 0;

	if (bt_linfo->hfp_desc.exist)
		profile_map |= BTC_BT_HFP;

	if (bt_linfo->hid_desc.exist)
		profile_map |= BTC_BT_HID;

	if (bt_linfo->a2dp_desc.exist)
		profile_map |= BTC_BT_A2DP;

	if (bt_linfo->pan_desc.exist)
		profile_map |= BTC_BT_PAN;

	switch (profile_map) {
	case 0:
		if (_check_freerun(btc))
			_action_freerun(btc);
		else if (a2dp.active || pan.active)
			_action_bt_pan(btc);
		else
			_action_bt_idle(btc);
		break;
	case BTC_BT_HFP:
		if (_check_freerun(btc))
			_action_freerun(btc);
		else
			_action_bt_hfp(btc);
		break;
	case BTC_BT_HFP | BTC_BT_HID:
	case BTC_BT_HID:
		if (_check_freerun(btc))
			_action_freerun(btc);
		else
			_action_bt_hid(btc);
		break;
	case BTC_BT_A2DP:
		if (_check_freerun(btc))
			_action_freerun(btc);
		else if (a2dp.sink)
			_action_bt_a2dpsink(btc);
		else if (bt_linfo->multi_link.now && !hid.pair_cnt)
			 _action_bt_a2dp_pan(btc);
		else
			_action_bt_a2dp(btc);
		break;
	case BTC_BT_PAN:
		_action_bt_pan(btc);
		break;
	case BTC_BT_A2DP | BTC_BT_HFP:
	case BTC_BT_A2DP | BTC_BT_HID:
	case BTC_BT_A2DP | BTC_BT_HFP | BTC_BT_HID:
		if (_check_freerun(btc))
			_action_freerun(btc);
		else
			_action_bt_a2dp_hid(btc);
		break;
	case BTC_BT_A2DP | BTC_BT_PAN:
		_action_bt_a2dp_pan(btc);
		break;
	case BTC_BT_PAN | BTC_BT_HFP:
	case BTC_BT_PAN | BTC_BT_HID:
	case BTC_BT_PAN | BTC_BT_HFP | BTC_BT_HID:
		_action_bt_pan_hid(btc);
		break;
	case BTC_BT_A2DP | BTC_BT_PAN | BTC_BT_HID:
	case BTC_BT_A2DP | BTC_BT_PAN | BTC_BT_HFP:
	default:
		_action_bt_a2dp_pan_hid(btc);
		break;
	}
}

void _action_wl_2g_sta(struct btc_t *btc)
{
	_action_by_bt(btc);
}

void _action_wl_scan(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_wl_dbcc_info *wl_dinfo = &wl->dbcc_info;

	if (btc->hal->dbcc_en) {
		if (wl_dinfo->real_band[HW_PHY_0] != BAND_ON_24G &&
		    wl_dinfo->real_band[HW_PHY_1] != BAND_ON_24G)
			_action_wl_5g(btc);
		else
			_action_by_bt(btc);
	} else {
		if (wl->scan_info.band[HW_PHY_0] != BAND_ON_24G)
			_action_wl_5g(btc);
		else
			_action_by_bt(btc);
	}
}

void _action_wl_25g_mcc(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	u16 policy_type = BTC_CXP_OFF_BT;

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED)/* shared-antenna */ {
		if (wl->status.map._4way)
			policy_type = BTC_CXP_OFFE_WL;
		else if (bt->link_info.status.map.connect == 0)
			policy_type = BTC_CXP_OFFE_2GISOB;
		else
			policy_type = BTC_CXP_OFFE_2GBWISOB;
	} else { /* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ0;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W25G);
	_set_policy(btc, policy_type, __func__);
}

void _action_wl_2g_mcc(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	u16 policy_type = BTC_CXP_OFF_BT;

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) { /* shared-antenna */
		if (wl->status.map._4way)
			policy_type = BTC_CXP_OFFE_WL;
		else if (bt->link_info.status.map.connect == 0)
			policy_type = BTC_CXP_OFFE_2GISOB;
		else
			policy_type = BTC_CXP_OFFE_2GBWISOB;
	} else { /* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ0;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

void _action_wl_2g_scc(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	struct btc_dm *dm = &btc->dm;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	u16 policy_type = BTC_CXP_OFF_BT;
	u32 dur;

	if (btc->mdinfo.ant.type == BTC_ANT_DEDICATED) {
		policy_type = BTC_CXP_OFF_EQ0;
		goto exit;
	}

	/* shared-antenna */
	switch(wl_rinfo->mrole_type) {
	default:
#if 0
	case BTC_WLMROLE_STA_GO:
	case BTC_WLMROLE_STA_GO_NOA:
		dm->wl_scc.null_role1 = PHL_RTYPE_STATION;
		dm->wl_scc.null_role2 = PHL_RTYPE_NONE;
		dm->wl_scc.ebt_null = 0;
		policy_type = BTC_CXP_OFF_BWB1;
#endif
		break;
	case BTC_WLMROLE_STA_GC:
		dm->wl_scc.null_role1 = PHL_RTYPE_STATION;
		dm->wl_scc.null_role2 = PHL_RTYPE_P2P_GC;
		dm->wl_scc.ebt_null = 0; /* no ext-slot-control */
		_action_by_bt(btc);
		return;
	case BTC_WLMROLE_STA_STA:
		dm->wl_scc.null_role1 = PHL_RTYPE_STATION;
		dm->wl_scc.null_role2 = PHL_RTYPE_STATION;
		dm->wl_scc.ebt_null = 0; /* no ext-slot-control */
		_action_by_bt(btc);
		return;
	case BTC_WLMROLE_STA_GC_NOA:
	case BTC_WLMROLE_STA_GO:
	case BTC_WLMROLE_STA_GO_NOA:
		dm->wl_scc.null_role1 = PHL_RTYPE_STATION;
		dm->wl_scc.null_role2 = PHL_RTYPE_NONE;
		dur = wl_rinfo->mrole_noa_duration;

		if (wl->status.map._4way) {
			dm->wl_scc.ebt_null = 0;
			policy_type = BTC_CXP_OFFE_WL;
		} else if (bt->link_info.status.map.connect == 0) {
			dm->wl_scc.ebt_null = 0;
			policy_type = BTC_CXP_OFFE_2GISOB;
		} else if (bt->link_info.a2dp_desc.exist &&
			   dur < btc->bt_req_len) {
			dm->wl_scc.ebt_null = 1; /* tx null at EBT */
			policy_type = BTC_CXP_OFFE_2GBWMIXB2;
		} else if (bt->link_info.a2dp_desc.exist ||
			   bt->link_info.pan_desc.exist) {
			dm->wl_scc.ebt_null = 1; /* tx null at EBT */
			policy_type = BTC_CXP_OFFE_2GBWISOB;
		}else {
			dm->wl_scc.ebt_null = 0;
			policy_type = BTC_CXP_OFFE_2GBWISOB;
		}

		break;
	}

exit:
	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

void _action_wl_2g_ap(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	u16 policy_type = BTC_CXP_OFF_BT;

	/* Todo:if client issue Null-P, ap should follow Null/Null-P slot */
	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) { /* shared-antenna */
		if (wl->status.map._4way)
			policy_type = BTC_CXP_OFFE_WL;
		else if (bt->link_info.status.map.connect == 0)
			policy_type = BTC_CXP_OFFE_2GISOB;
		else
			policy_type = BTC_CXP_OFFE_2GBWISOB;
	} else {/* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ0;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

void _action_wl_2g_go(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	u16 policy_type = BTC_CXP_OFF_BT;

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) { /* shared-antenna */
		if (wl->status.map._4way)
			policy_type = BTC_CXP_OFFE_WL;
		else if (bt->link_info.status.map.connect == 0)
			policy_type = BTC_CXP_OFFE_2GISOB;
		else
			policy_type = BTC_CXP_OFFE_2GBWISOB;
	} else { /* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ0;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

void _action_wl_2g_gc(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	struct btc_wl_role_info *wl_rinfo = &wl->role_info;
	u8 noa = 0, pid = 0;
	u16 policy_type = BTC_CXP_OFF_BT;
	u32 noa_duration = 0;

	pid = _get_wl_role_idx(btc, PHL_RTYPE_P2P_GC);

	if (pid < MAX_WIFI_ROLE_NUMBER) {
		noa = wl_rinfo->active_role[pid].noa;
		noa_duration = wl_rinfo->active_role[pid].noa_duration;
	}
	/* Check GC, GC+NOA */
	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) { /* shared-antenna */
		if (!noa) { /* Gc without NOA */
			_action_by_bt(btc);
			return;
		} else { /* Gc with NOA */
			if (wl->status.map._4way)
				policy_type = BTC_CXP_OFFE_WL;
			else if (bt->link_info.status.map.connect == 0)
				policy_type = BTC_CXP_OFFE_2GISOB;
			else if (bt->link_info.a2dp_desc.exist &&
				 noa_duration < btc->bt_req_len)
				policy_type = BTC_CXP_OFFE_2GBWMIXB;
			else
				policy_type = BTC_CXP_OFFE_2GBWISOB;
		}
	} else {/* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ0;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

void _action_wl_2g_nan(struct btc_t *btc)
{
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	u16 policy_type = BTC_CXP_OFF_BT;

	if (btc->mdinfo.ant.type == BTC_ANT_SHARED) { /* shared-antenna */
		if (wl->status.map._4way)
			policy_type = BTC_CXP_OFFE_WL;
		else if (bt->link_info.status.map.connect == 0)
			policy_type = BTC_CXP_OFFE_2GISOB;
		else
			policy_type = BTC_CXP_OFFE_2GBWISOB;
	} else { /* dedicated-antenna */
		policy_type = BTC_CXP_OFF_EQ0;
	}

	_set_ant(btc, NM_EXEC, BTC_PHY_ALL, BTC_ANT_W2G);
	_set_policy(btc, policy_type, __func__);
}

#endif
