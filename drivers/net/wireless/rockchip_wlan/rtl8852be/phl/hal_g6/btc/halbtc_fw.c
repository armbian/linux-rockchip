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
#define _HAL_BTC_FW_C_
#include "../hal_headers_le.h"
#include "hal_btc.h"
#include "halbtc_fw.h"
#include "halbtc_def.h"

#ifdef CONFIG_BTCOEX

void _chk_btc_err(struct btc_t *btc, u8 type, u32 cnt)
{
	struct btc_cx *cx = &btc->cx;
	struct btc_dm *dm = &btc->dm;
	struct btc_wl_info *wl = &cx->wl;
	struct btc_bt_info *bt = &cx->bt;

	switch (type) {
	case BTC_DCNT_RPT_FREEZE:
		if (wl->status.map.lps == BTC_LPS_RF_OFF ||
		    wl->status.map.rf_off)
			return;

		if (dm->cnt_dm[BTC_DCNT_RPT] == cnt && btc->fwinfo.rpt_en_map)
			dm->cnt_dm[BTC_DCNT_RPT_FREEZE]++;
		else
			dm->cnt_dm[BTC_DCNT_RPT_FREEZE] = 0;

		if (dm->cnt_dm[BTC_DCNT_RPT_FREEZE] >= BTC_CHK_HANG_MAX)
			dm->error.map.wl_fw_hang = true;
		else
			dm->error.map.wl_fw_hang = false;

		dm->cnt_dm[BTC_DCNT_RPT] = cnt;
		break;
	case BTC_DCNT_CYCLE_FREEZE:
		if (dm->cnt_dm[BTC_DCNT_CYCLE] == cnt &&
		    (dm->tdma_now.type != CXTDMA_OFF ||
		     dm->tdma_now.ext_ctrl == CXECTL_EXT))
			dm->cnt_dm[BTC_DCNT_CYCLE_FREEZE]++;
		else
			dm->cnt_dm[BTC_DCNT_CYCLE_FREEZE] = 0;

		if (dm->cnt_dm[BTC_DCNT_CYCLE_FREEZE] >= BTC_CHK_HANG_MAX)
			dm->error.map.cycle_hang = true;
		else
			dm->error.map.cycle_hang = false;

		dm->cnt_dm[BTC_DCNT_CYCLE] = cnt;
		break;
	case BTC_DCNT_W1_FREEZE:
		if (dm->cnt_dm[BTC_DCNT_W1] == cnt &&
		    dm->tdma_now.type != CXTDMA_OFF)
			dm->cnt_dm[BTC_DCNT_W1_FREEZE]++;
		else
			dm->cnt_dm[BTC_DCNT_W1_FREEZE] = 0;

		if (dm->cnt_dm[BTC_DCNT_W1_FREEZE] >= BTC_CHK_HANG_MAX)
			dm->error.map.w1_hang = true;
		else
			dm->error.map.w1_hang = false;

		dm->cnt_dm[BTC_DCNT_W1] = cnt;
		break;
	case BTC_DCNT_B1_FREEZE:
		if (dm->cnt_dm[BTC_DCNT_B1] == cnt &&
		    dm->tdma_now.type != CXTDMA_OFF)
			dm->cnt_dm[BTC_DCNT_B1_FREEZE]++;
		else
			dm->cnt_dm[BTC_DCNT_B1_FREEZE] = 0;

		if (dm->cnt_dm[BTC_DCNT_B1_FREEZE] >= BTC_CHK_HANG_MAX)
			dm->error.map.b1_hang = true;
		else
			dm->error.map.b1_hang = false;

		dm->cnt_dm[BTC_DCNT_B1] = cnt;
		break;
	case BTC_DCNT_TDMA_NONSYNC:
		if (cnt != 0) /* if tdma not sync between drv/fw  */
			dm->cnt_dm[BTC_DCNT_TDMA_NONSYNC]++;
		else
			dm->cnt_dm[BTC_DCNT_TDMA_NONSYNC] = 0;

		if (dm->cnt_dm[BTC_DCNT_TDMA_NONSYNC] >= BTC_CHK_HANG_MAX)
			dm->error.map.tdma_no_sync = true;
		else
			dm->error.map.tdma_no_sync = false;
		break;
	case BTC_DCNT_SLOT_NONSYNC:
		if (cnt != 0) /* if slot not sync between drv/fw  */
			dm->cnt_dm[BTC_DCNT_SLOT_NONSYNC]++;
		else
			dm->cnt_dm[BTC_DCNT_SLOT_NONSYNC] = 0;

		if (dm->cnt_dm[BTC_DCNT_SLOT_NONSYNC] >= BTC_CHK_HANG_MAX)
			dm->error.map.tdma_no_sync = true;
		else
			dm->error.map.tdma_no_sync = false;
		break;
	case BTC_DCNT_BTCNT_FREEZE:
		cnt = cx->cnt_bt[BTC_BCNT_HIPRI_RX] +
		      cx->cnt_bt[BTC_BCNT_HIPRI_TX] +
		      cx->cnt_bt[BTC_BCNT_LOPRI_RX] +
		      cx->cnt_bt[BTC_BCNT_LOPRI_TX];

		if (cnt == 0)
			dm->cnt_dm[BTC_DCNT_BTCNT_FREEZE]++;
		else
			dm->cnt_dm[BTC_DCNT_BTCNT_FREEZE] = 0;

		if ((dm->cnt_dm[BTC_DCNT_BTCNT_FREEZE] >= BTC_CHK_HANG_MAX &&
		     bt->enable.now) || (!dm->cnt_dm[BTC_DCNT_BTCNT_FREEZE] &&
		     !bt->enable.now))
			_update_bt_scbd(btc, false);
		break;
	case BTC_DCNT_WL_SLOT_DRIFT:
		if (cnt >= BTC_CHK_WLSLOT_DRIFT_MAX)
			dm->cnt_dm[BTC_DCNT_WL_SLOT_DRIFT]++;
		else
			dm->cnt_dm[BTC_DCNT_WL_SLOT_DRIFT] = 0;

		if (dm->cnt_dm[BTC_DCNT_WL_SLOT_DRIFT] >= BTC_CHK_HANG_MAX)
			dm->error.map.wl_slot_drift = true;
		else
			dm->error.map.wl_slot_drift = false;
		break;
	case BTC_DCNT_BT_SLOT_DRIFT:
		if (cnt >= BTC_CHK_BTSLOT_DRIFT_MAX)
			dm->cnt_dm[BTC_DCNT_BT_SLOT_DRIFT]++;
		else
			dm->cnt_dm[BTC_DCNT_BT_SLOT_DRIFT] = 0;

		if (dm->cnt_dm[BTC_DCNT_BT_SLOT_DRIFT] >= BTC_CHK_HANG_MAX)
			dm->error.map.bt_slot_drift = true;
		else
			dm->error.map.bt_slot_drift = false;
		break;
	}
}

static void _update_bt_report(struct btc_t *btc, u8 rpt_type, u8* pfinfo)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_bt_info *bt = &btc->cx.bt;
	struct btc_bt_link_info *bt_linfo = &bt->link_info;
	struct btc_bt_a2dp_desc *a2dp = &bt_linfo->a2dp_desc;

	struct fbtc_btver* pver = (struct fbtc_btver*) pfinfo;
	struct fbtc_btscan* pscan = (struct fbtc_btscan*) pfinfo;
	struct fbtc_btafh* pafh = (struct fbtc_btafh*) pfinfo;
	struct fbtc_btdevinfo* pdev = (struct fbtc_btdevinfo*) pfinfo;

	switch (rpt_type) {
	case BTC_RPT_TYPE_BT_VER:
		bt->ver_info.fw = pver->fw_ver;
		bt->ver_info.fw_coex = (pver->coex_ver & bMASKB0);
		bt->feature = pver->feature;
		break;
	case BTC_RPT_TYPE_BT_SCAN:
		hal_mem_cpy(h, bt->scan_info, pscan->scan, BTC_SCAN_MAX1);
		break;
	case BTC_RPT_TYPE_BT_AFH:
		hal_mem_cpy(h, &bt_linfo->afh_map[0], pafh->afh_l, 4);
		hal_mem_cpy(h, &bt_linfo->afh_map[4], pafh->afh_m, 4);
		hal_mem_cpy(h, &bt_linfo->afh_map[8], pafh->afh_h, 2);
		break;
	case BTC_RPT_TYPE_BT_DEVICE:
		a2dp->device_name = pdev->dev_name;
		a2dp->vendor_id = pdev->vendor_id;
		a2dp->flush_time = pdev->flush_time;
		break;
	default:
		break;
	}
}

static u32 _chk_btc_report(struct btc_t *btc, struct btf_fwinfo *pfwinfo,
			u8 *prptbuf, u32 index)
{
	struct btc_dm *dm = &btc->dm;
	struct rtw_hal_com_t *hal = btc->hal;
	struct btc_rpt_cmn_info *pcinfo = NULL;
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_bt_info *bt = &btc->cx.bt;
	struct fbtc_rpt_ctrl *prpt = NULL;
	struct fbtc_cysta *pcysta = NULL;
	u8 rpt_type = 0, *rpt_content = NULL, *pfinfo = NULL;
	u16 wl_slot_set = 0, polt_cnt = 0;
	u32 rpt_len = 0, diff_t, bt_slot_real = 0;

	if (!prptbuf) {
		pfwinfo->err[BTFRE_INVALID_INPUT]++;
		return 0;
	}

	rpt_type = prptbuf[index];
	rpt_len = (prptbuf[index+2] << 8) + prptbuf[index+1];
	rpt_content = &prptbuf[index+3];

	switch (rpt_type) {
	case BTC_RPT_TYPE_CTRL:
		pcinfo = &pfwinfo->rpt_ctrl.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_ctrl.finfo);
		pcinfo->req_len = sizeof(struct fbtc_rpt_ctrl);
		pcinfo->req_fver = FCX_BTCRPT_VER;
		pcinfo->rsp_fver = *rpt_content;
		break;
	case BTC_RPT_TYPE_TDMA:
		pcinfo = &pfwinfo->rpt_fbtc_tdma.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_tdma.finfo);
		pcinfo->req_len = sizeof(struct fbtc_1tdma);
		pcinfo->req_fver = FCX_TDMA_VER;
		break;
	case BTC_RPT_TYPE_SLOT:
		pcinfo = &pfwinfo->rpt_fbtc_slots.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_slots.finfo);
		pcinfo->req_len = sizeof(struct fbtc_slots);
		pcinfo->req_fver = FCX_SLOT_VER;
		break;
	case BTC_RPT_TYPE_CYSTA:
		pcinfo = &pfwinfo->rpt_fbtc_cysta.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_cysta.finfo);
		pcysta = &pfwinfo->rpt_fbtc_cysta.finfo;
		pcinfo->req_len = sizeof(struct fbtc_cysta);
		pcinfo->req_fver = FCX_CYSTA_VER;
		break;
	case BTC_RPT_TYPE_STEP:
		pcinfo = &pfwinfo->rpt_fbtc_step.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_step.finfo);
		pcinfo->req_len = 8 + sizeof(struct fbtc_step) * FCXDEF_STEP;
		pcinfo->req_fver = FCX_STEP_VER;
		break;
	case BTC_RPT_TYPE_NULLSTA:
		pcinfo = &pfwinfo->rpt_fbtc_nullsta.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_nullsta.finfo);
		pcinfo->req_len = sizeof(struct fbtc_cynullsta);
		pcinfo->req_fver = FCX_NULLSTA_VER;
		break;
	case BTC_RPT_TYPE_MREG:
		pcinfo = &pfwinfo->rpt_fbtc_mregval.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_mregval.finfo);
		pcinfo->req_len = sizeof(struct fbtc_mreg_val);
		pcinfo->req_fver = FCX_MREG_VER;
		break;
	case BTC_RPT_TYPE_GPIO_DBG:
		pcinfo = &pfwinfo->rpt_fbtc_gpio_dbg.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_gpio_dbg.finfo);
		pcinfo->req_len = sizeof(struct fbtc_gpio_dbg);
		pcinfo->req_fver = FCX_GPIODBG_VER;
		break;
	case BTC_RPT_TYPE_BT_VER:
		pcinfo = &pfwinfo->rpt_fbtc_btver.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_btver.finfo);
		pcinfo->req_len = sizeof(struct fbtc_btver);
		pcinfo->req_fver = FCX_BTVER_VER;
		break;
	case BTC_RPT_TYPE_BT_SCAN:
		pcinfo = &pfwinfo->rpt_fbtc_btscan.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_btscan.finfo);
		pcinfo->req_len = sizeof(struct fbtc_btscan);
		pcinfo->req_fver = FCX_BTSCAN_VER;
		break;
	case BTC_RPT_TYPE_BT_AFH:
		pcinfo = &pfwinfo->rpt_fbtc_btafh.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_btafh.finfo);
		pcinfo->req_len = sizeof(struct fbtc_btafh);
		pcinfo->req_fver = FCX_BTAFH_VER;
		break;
	case BTC_RPT_TYPE_BT_DEVICE:
		pcinfo = &pfwinfo->rpt_fbtc_btdev.cinfo;
		pfinfo = (u8 *)(&pfwinfo->rpt_fbtc_btdev.finfo);
		pcinfo->req_len = sizeof(struct fbtc_btdevinfo);
		pcinfo->req_fver = FCX_BTDEVINFO_VER;
		break;
	default:
		pfwinfo->err[BTFRE_UNDEF_TYPE]++;
		return 0;
	}

	pcinfo->rsp_fver = *rpt_content;
	pcinfo->rx_len = rpt_len;
	pcinfo->rx_cnt++;

	if (rpt_len != pcinfo->req_len) {
		if (rpt_type < BTC_RPT_TYPE_MAX)
			pfwinfo->len_mismch |= (0x1 << rpt_type);
		else
			pfwinfo->len_mismch |= BIT31;

		pcinfo->valid = 0;
		return 0;
	} else if (pcinfo->req_fver != pcinfo->rsp_fver) {
		if (rpt_type < BTC_RPT_TYPE_MAX)
			pfwinfo->fver_mismch |= (0x1 << rpt_type);
		else
			pfwinfo->fver_mismch |= BIT31;
		pcinfo->valid = 0;
		return 0;
	} else if (!pfinfo || !rpt_content || !pcinfo->req_len) {
		pfwinfo->err[BTFRE_EXCEPTION]++;
		pcinfo->valid = 0;
		return 0;
	}


	hal_mem_cpy(hal, (void *)pfinfo, (void *)rpt_content, pcinfo->req_len);
	pcinfo->valid = 1;

	if (rpt_type == BTC_RPT_TYPE_TDMA) {
#if BTC_CX_FW_OFFLOAD /* update tdma_now if fw offload for debug */
		_tdma_cpy(&dm->tdma_now, &pfwinfo->rpt_fbtc_tdma.finfo.tdma);
#else
		_chk_btc_err(btc, BTC_DCNT_TDMA_NONSYNC,
			     _tdma_cmp(&dm->tdma_now,
				       &pfwinfo->rpt_fbtc_tdma.finfo.tdma));
#endif
	}

	if (rpt_type == BTC_RPT_TYPE_SLOT) {
#if BTC_CX_FW_OFFLOAD /* update slot_now if fw offload for debug */
		_slots_cpy(dm->slot_now, pfwinfo->rpt_fbtc_slots.finfo.slot);
#else
		_chk_btc_err(btc, BTC_DCNT_SLOT_NONSYNC,
			     _tdma_cmp(dm->slot_now,
				       pfwinfo->rpt_fbtc_slots.finfo.slot));
#endif
	}

	if (rpt_type == BTC_RPT_TYPE_CYSTA &&
	    pcysta->cycles >= BTC_CYSTA_CHK_PERIOD) {
		/* Check Leak-AP */
		if (pcysta->slot_cnt[CXST_LK] != 0 &&
		    pcysta->leak_slot.cnt_rximr != 0 && dm->tdma_now.rxflctrl) {
			if (pcysta->slot_cnt[CXST_LK] <
			    BTC_LEAK_AP_TH * pcysta->leak_slot.cnt_rximr)
				dm->leak_ap = 1;
		}

		/* Check diff time between real WL slot and W1 slot */
		if (dm->tdma_now.type != CXTDMA_OFF) {
			wl_slot_set = dm->slot_now[CXST_W1].dur;

			if (pcysta->cycle_time.tavg[CXT_WL] > wl_slot_set) {
				diff_t = pcysta->cycle_time.tavg[CXT_WL] -
					 wl_slot_set;
				_chk_btc_err(btc, BTC_DCNT_WL_SLOT_DRIFT, diff_t);
			}
		}

		/* Check diff time between real BT slot and EBT/E5G slot */
		if (dm->tdma_now.type == CXTDMA_OFF &&
		    dm->tdma_now.ext_ctrl == CXECTL_EXT &&
		    btc->bt_req_len != 0) {
			bt_slot_real = pcysta->cycle_time.tavg[CXT_BT];

			if (btc->bt_req_len > bt_slot_real) {
				diff_t = btc->bt_req_len - bt_slot_real;
				_chk_btc_err(btc, BTC_DCNT_BT_SLOT_DRIFT, diff_t);
			}

		}

		_chk_btc_err(btc, BTC_DCNT_W1_FREEZE, pcysta->slot_cnt[CXST_W1]);
		_chk_btc_err(btc, BTC_DCNT_B1_FREEZE, pcysta->slot_cnt[CXST_B1]);
		_chk_btc_err(btc, BTC_DCNT_CYCLE_FREEZE, (u32)pcysta->cycles);
	}

	if (rpt_type == BTC_RPT_TYPE_CTRL) {
		prpt = &pfwinfo->rpt_ctrl.finfo;
		btc->fwinfo.rpt_en_map = prpt->rpt_info.en;
		wl->ver_info.fw_coex = prpt->wl_fw_info.cx_ver;
		wl->ver_info.fw = prpt->wl_fw_info.fw_ver;
		dm->wl_fw_cx_offload = !!(prpt->wl_fw_info.cx_offload);

		if (dm->wl_fw_cx_offload != BTC_CX_FW_OFFLOAD)
			dm->error.map.offload_mismatch = true;
		else
			dm->error.map.offload_mismatch = false;

		if (prpt->bt_rfk_cnt[BTC_BCNT_RFK_TIMEOUT] > 0)
			bt->rfk_info.map.timeout = 1;
		else
			bt->rfk_info.map.timeout = 0;

		dm->error.map.bt_rfk_timeout = bt->rfk_info.map.timeout;

		_chk_btc_err(btc, BTC_DCNT_RPT_FREEZE,
			     pfwinfo->event[BTF_EVNT_RPT]);

		/* To avoid I/O if WL LPS or power-off  */
		if (wl->status.map.lps != BTC_LPS_RF_OFF &&
		    !wl->status.map.rf_off) {
			btc->chip->ops->update_bt_cnt(btc);
			_chk_btc_err(btc, BTC_DCNT_BTCNT_FREEZE, 0);
			rtw_hal_mac_get_bt_polt_cnt(btc->hal, HW_PHY_0,
						    &polt_cnt);
			btc->cx.cnt_bt[BTC_BCNT_POLUT] = polt_cnt;
		}
	}

	if (rpt_type >= BTC_RPT_TYPE_BT_VER &&
	    rpt_type <= BTC_RPT_TYPE_BT_DEVICE)
		_update_bt_report(btc, rpt_type, pfinfo);

	return (rpt_len + BTC_RPT_HDR_SIZE);
}

static void _parse_btc_report(struct btc_t *btc, struct btf_fwinfo *pfwinfo,
			      u8 *pbuf, u32 buf_len)
{
	u32 index = 0, rpt_len = 0;

	while (pbuf) {
		if (index+2 >= RTW_PHL_BTC_FWINFO_BUF)
			break;
		/* At least 3 bytes: type(1) & len(2) */
		rpt_len = (pbuf[index+2] << 8) + pbuf[index+1];
		if ((index + rpt_len + BTC_RPT_HDR_SIZE) > buf_len)
			break;

		rpt_len = _chk_btc_report(btc, pfwinfo, pbuf, index);
		if (!rpt_len)
			break;
		index += rpt_len;
	}
}

static void _append_tdma(struct btc_t *btc, bool force_exec)
{
	struct btc_dm *dm = &btc->dm;
	struct btf_tlv *tlv = NULL;
	struct fbtc_1tdma *v = NULL;
	u16 len = btc->policy_len;

	if (!force_exec && !_tdma_cmp(&dm->tdma, &dm->tdma_now)) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s(): tdma no change!\n",
			  __func__);
		return;
	}

	tlv = (struct btf_tlv *)&btc->policy[len];
	tlv->type = CXPOLICY_TDMA;
	tlv->len = sizeof(struct fbtc_1tdma);
	v = (struct fbtc_1tdma *)&tlv->val[0];
	v->fver = FCX_TDMA_VER;

	_tdma_cpy(&v->tdma, &dm->tdma);

	btc->policy_len = len + 2 + sizeof(struct fbtc_1tdma);

	PHL_INFO("[BTC], %s: type:%d, rxflctrl=%d, txflctrl=%d, wtgle_n=%d, leak_n=%d, ext_ctrl=%d, rxflctrl_role=0x%x\n",
		 __func__, dm->tdma.type, dm->tdma.rxflctrl, dm->tdma.txflctrl,
		 dm->tdma.wtgle_n, dm->tdma.leak_n, dm->tdma.ext_ctrl,
		 dm->tdma.rxflctrl_role);
}

static void _append_slot(struct btc_t *btc, bool force_exec)
{
	struct btc_dm *dm = &btc->dm;
	struct btf_tlv *tlv = NULL;
	struct fbtc_1slot *v = NULL;
	u16 len = 0;
	u8 i, cnt = 0;

	for (i = 0; i < CXST_MAX; i++) {
		if (!force_exec && !_slot_cmp(&dm->slot[i], &dm->slot_now[i]))
			continue;

		len = btc->policy_len;

		tlv = (struct btf_tlv *)&btc->policy[len];
		tlv->type = CXPOLICY_SLOT;
		tlv->len = sizeof(struct fbtc_1slot);
		v = (struct fbtc_1slot *)&tlv->val[0];

		v->fver = FCX_SLOT_VER;
		v->sid = i;
		_slot_cpy(&v->slot, &dm->slot[i]);

		PHL_INFO("[BTC], %s: slot-%d: dur=%d, table=0x%08x, type=%d\n",
			 __func__, i,dm->slot[i].dur, dm->slot[i].cxtbl,
			 dm->slot[i].cxtype);
		cnt++;
		btc->policy_len = len + 2 + sizeof(struct fbtc_1slot);
	}

	if (cnt > 0)
		PHL_INFO("[BTC], %s: slot update (cnt=%d)!!\n", __func__, cnt);
}

/*
 * extern functions
 */

void hal_btc_fw_en_rpt(struct btc_t *btc, u32 rpt_map, u32 rpt_state)
{
	struct btc_ops *ops = btc->ops;
	struct btf_set_report r = {0};
	struct btf_fwinfo* fwinfo = &btc->fwinfo;
	u32 val = 0;
	u8 en;

	if (!ops || !ops->fw_cmd)
		return;

	en = rpt_state & 0x1;
	if (en)
		val = fwinfo->rpt_en_map | rpt_map;
	else
		val = fwinfo->rpt_en_map & (~rpt_map);

	if (val == fwinfo->rpt_en_map)
		return;

	fwinfo->rpt_en_map = val;

	r.fver = FCX_BTCRPT_VER;
	r.enable = val;
	r.para = en;

	ops->fw_cmd(btc, BTFC_SET, SET_REPORT_EN, (u8 *)&r, sizeof(r));
}

void hal_btc_fw_set_slots(struct btc_t *btc, u8 num, struct fbtc_slot *s)
{
#if !BTC_CX_FW_OFFLOAD
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_ops *ops = btc->ops;
	struct btf_set_slot_table *tbl = NULL;
	u8 *ptr = NULL;
	u16 n = 0;

	if (!ops || !ops->fw_cmd)
		return;

	n = (sizeof(struct fbtc_slot) * num) + sizeof(*tbl) - 1;
	tbl = hal_mem_alloc(h, n);
	if (!tbl)
		return;

	tbl->fver = FCX_SLOT_VER;
	tbl->tbl_num = num;
	ptr = &tbl->buf[0];
	hal_mem_cpy(h, (void*)ptr, s, num * sizeof(struct fbtc_slot));

	ops->fw_cmd(btc, BTFC_SET, SET_SLOT_TABLE, (u8*)tbl, n);
	hal_mem_free(h, (void*)tbl, n);
#endif
}

/* set RPT_EN_MREG = 0 to stop 2s monitor timer in WL FW,
 * before SET_MREG_TABLE, and set RPT_EN_MREG = 1 after
 * SET_MREG_TABLE
 */
void hal_btc_fw_set_monreg(struct btc_t *btc)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_ops *ops = btc->ops;
	struct btf_set_mon_reg *monreg = NULL;
	u8 n, *ptr = NULL, ulen;
	u16 sz = 0;

	if (!ops || !ops->fw_cmd)
		return;

	n = btc->chip->mon_reg_num;

	if (n > CXMREG_MAX) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], mon reg count %d > %d\n",
			  n, CXMREG_MAX);
		return;
	}

	ulen = sizeof(struct fbtc_mreg);
	sz = (ulen * n) + sizeof(*monreg) - 1;
	monreg = hal_mem_alloc(h, sz);
	if (!monreg)
		return;

	monreg->fver = FCX_MREG_VER;
	monreg->reg_num = n;
	ptr = &monreg->buf[0];
	hal_mem_cpy(h, (void *)ptr, btc->chip->mon_reg, n * ulen);

	ops->fw_cmd(btc, BTFC_SET, SET_MREG_TABLE, (u8 *)monreg, sz);
	hal_mem_free(h, (void *)monreg, sz);
	hal_btc_fw_en_rpt(btc, RPT_EN_MREG, 1);
}

bool hal_btc_fw_set_1tdma(struct btc_t *btc, u16 len, u8 *buf)
{ /* for wlcli manual control  */
	struct btc_dm *dm = &btc->dm;

	if (len != sizeof(struct fbtc_tdma)) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): return because len != %d\n",
			 __func__, (int)sizeof(struct fbtc_tdma));
		return false;
	} else if (buf[0] >= CXTDMA_MAX) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): return because tdma_type >= %d\n",
			 __func__, CXTDMA_MAX);
		return false;
	}

	_tdma_cpy(&dm->tdma, buf);
	return true;
}

bool hal_btc_fw_set_1slot(struct btc_t *btc, u16 len, u8 *buf)
{ /* for wlcli manual control  */
	struct btc_dm *dm = &btc->dm;

	if (len != sizeof(struct fbtc_slot) + 1) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): return because len != %d\n",
			 __func__, (int)sizeof(struct fbtc_slot) + 1);
		return false;
	} else if (buf[0] >= CXST_MAX) {
		PHL_TRACE(COMP_PHL_BTC, _PHL_ERR_, "[BTC], %s(): return because slot_id >= %d\n",
			 __func__, CXST_MAX);
		return false;
	}

	_slot_cpy(&dm->slot[buf[0]], &buf[1]);
	return true;
}

bool hal_btc_fw_set_policy(struct btc_t *btc, bool force_exec, u16 policy_type,
			   const char* action)
{
	struct btc_dm *dm = &btc->dm;
	struct btc_ops *ops = btc->ops;

	if (!ops || !ops->fw_cmd)
		return false;

	_act_cpy(dm->run_action, (char*)action);
	_update_dm_step(btc, action);
	_update_dm_step(btc, id_to_str(BTC_STR_POLICY, (u32)policy_type));

	btc->policy_len = 0; /* clear length before append */
	btc->policy_type = policy_type;

	_append_tdma(btc, force_exec);
	_append_slot(btc, force_exec);

	if (btc->policy_len == 0 || btc->policy_len > BTC_POLICY_MAXLEN)
		return false;

	PHL_INFO("[BTC], %s(): action=%s -> policy type/len: 0x%04x/%d\n",
		 __func__, action, policy_type, btc->policy_len);

	if (dm->tdma.rxflctrl == CXFLC_NULLP)
		btc->hal->btc_ctrl.lps = 1;
	else
		btc->hal->btc_ctrl.lps = 0;

	if (btc->hal->btc_ctrl.lps == 1)
		hal_btc_notify_ps_tdma(btc, btc->hal->btc_ctrl.lps);

	ops->fw_cmd(btc, BTFC_SET, SET_CX_POLICY, btc->policy, btc->policy_len);

	_tdma_cpy(&dm->tdma_now, &dm->tdma);
	_slots_cpy(dm->slot_now, dm->slot);

	if (btc->hal->btc_ctrl.lps == 0)
		hal_btc_notify_ps_tdma(btc, btc->hal->btc_ctrl.lps);

	return true;
}

void hal_btc_fw_set_gpio_dbg(struct btc_t *btc, u8 type, u32 val)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_ops *ops = btc->ops;
	u8 data[7] = {0}, len = 0;

	if (!ops || !ops->fw_cmd || type >= CXDGPIO_MAX)
		return;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s()\n", __func__);

	data[0] = FCX_GPIODBG_VER;
	data[1] = 0;
	data[2] = type;

	switch(type) {
	case CXDGPIO_EN_MAP:
		len = sizeof(u32) + 3;
		hal_mem_cpy(h, &data[3], &val, sizeof(u32));
		break;
	case CXDGPIO_MUX_MAP:
		len = sizeof(8) * 2 + 3;
		data[3] = (u8)(val & bMASKB0);
		data[4] = (u8)((val & bMASKB1) >> 8);
		break;
	default:
		return;
	}

	ops->fw_cmd(btc, BTFC_SET, SET_GPIO_DBG, data, len);
}

void hal_btc_fw_set_drv_info(struct btc_t *btc, u8 type)
{
	struct rtw_hal_com_t *h = btc->hal;
	struct btc_wl_info *wl = &btc->cx.wl;
	struct btc_dm *dm = &btc->dm;
	struct btc_ops *ops = btc->ops;
	u8 buf[256] = {0};
	u8 sz = 0, n = 0;

	if (!ops || !ops->fw_cmd || type >= CXDRVINFO_MAX)
		return;

	switch (type) {
	case CXDRVINFO_INIT:
		n = sizeof(dm->init_info);
		sz = n + 2;

		if (sz > sizeof(buf))
			return;

		buf[0] = CXDRVINFO_INIT;
		buf[1] = n;
		hal_mem_cpy(h, (void *)&buf[2], &dm->init_info, n);
		break;
	case CXDRVINFO_ROLE:
		n = sizeof(wl->role_info);
		sz = n + 2;

		if (sz > sizeof(buf))
			return;

		buf[0] = CXDRVINFO_ROLE;
		buf[1] = n;
		hal_mem_cpy(h, (void *)&buf[2], &wl->role_info, n);
		break;
	case CXDRVINFO_CTRL:
		n = sizeof(btc->ctrl);
		sz = n + 2;

		if (sz > sizeof(buf))
			return;

		buf[0] = CXDRVINFO_CTRL;
		buf[1] = n;
		hal_mem_cpy(h, (void *)&buf[2], &btc->ctrl, n);
		break;
	case CXDRVINFO_RFK:
		n = sizeof(wl->rfk_info);
		sz = n + 2;

		if (sz > sizeof(buf))
			return;

		buf[0] = CXDRVINFO_RFK;
		buf[1] = n;
		hal_mem_cpy(h, (void *)&buf[2], &wl->rfk_info, n);
		break;
#if BTC_CX_FW_OFFLOAD
	case CXDRVINFO_DBCC:
		n = sizeof(wl->dbcc_info);
		sz = n + 2;

		if (sz > sizeof(buf))
			return;

		buf[0] = CXDRVINFO_DBCC;
		buf[1] = n;
		hal_mem_cpy(h, (void *)&buf[2], &wl->dbcc_info, n);
		break;
	case CXDRVINFO_SMAP:
		n = sizeof(wl->status);
		sz = n + 2;

		if (sz > sizeof(buf))
			return;

		buf[0] = CXDRVINFO_SMAP;
		buf[1] = n;
		hal_mem_cpy(h, (void *)&buf[2], &wl->status, n);
		break;
	case CXDRVINFO_RUN:
		n = BTC_RSN_MAXLEN;
		sz = n + 2;

		if (sz > sizeof(buf))
			return;

		buf[0] = CXDRVINFO_RUN;
		buf[1] = n;
		hal_mem_cpy(h, (void *)&buf[2], dm->run_reason, n);
		break;
	case CXDRVINFO_SCAN:
		n = sizeof(wl->scan_info);
		sz = n + 2;

		if (sz > sizeof(buf))
			return;

		buf[0] = CXDRVINFO_SCAN;
		buf[1] = n;
		hal_mem_cpy(h, (void *)&buf[2], &wl->scan_info, n);
		break;
#endif
	default:
		return;
	}

	ops->fw_cmd(btc, BTFC_SET, SET_DRV_INFO, (u8*)buf, sz);
}

void hal_btc_fw_set_drv_event(struct btc_t *btc, u8 type)
{
	struct btc_ops *ops = btc->ops;

	if (!ops || !ops->fw_cmd)
		return;

	ops->fw_cmd(btc, BTFC_SET, SET_DRV_EVENT, &type, 1);
}

void hal_btc_fw_set_bt(struct btc_t *btc, u8 type, u16 len, u8* buf)
{
	struct btc_ops *ops = btc->ops;

	if (!ops || !ops->fw_cmd ||
	    (type < SET_BT_WREG_ADDR || type > SET_BT_GOLDEN_RX_RANGE))
		return;

	PHL_TRACE(COMP_PHL_BTC, _PHL_DEBUG_, "[BTC], %s()\n", __func__);

	ops->fw_cmd(btc, BTFC_SET, type, buf, len);
}

void hal_btc_fw_event(struct btc_t *btc, u8 evt_id, void *data, u32 len)
{
	struct btf_fwinfo *pfwinfo = &btc->fwinfo;

	if (!len || !data)
		return;

	switch (evt_id) {
	case BTF_EVNT_RPT:
		_parse_btc_report(btc, pfwinfo, data, len);
		break;
	default:
		break;
	}
}

#endif
