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
#define _HAL_STA_C_
#include "hal_headers.h"

void
_hal_sta_rssi_init(struct rtw_phl_stainfo_t *sta)
{
	sta->hal_sta->rssi_stat.assoc_rssi = 0;
	sta->hal_sta->rssi_stat.ma_rssi = 0;
}

static enum rtw_hal_status
_hal_bfee_init(struct hal_info_t *hal_info,
	       struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct rtw_wifi_role_t *wrole = NULL;
	bool enable_bfee = false;

	do {
		if(NULL == sta)
			break;
		wrole = sta->wrole;
		if(NULL == wrole)
			break;

		/*only init BFee when wrole cap's bfee and sta cap 's bfer matched */

		if ((wrole->proto_role_cap.he_su_bfme ||
		     wrole->proto_role_cap.he_mu_bfme) &&
		    (sta->asoc_cap.he_su_bfmr || sta->asoc_cap.he_mu_bfmr)) {
			enable_bfee = true;
		}

		if ((wrole->proto_role_cap.vht_su_bfme ||
		     wrole->proto_role_cap.vht_mu_bfme) &&
		    (sta->asoc_cap.vht_su_bfmr || sta->asoc_cap.vht_mu_bfmr)) {
			enable_bfee = true;
		}

		if (wrole->proto_role_cap.ht_su_bfme &&
		    sta->asoc_cap.ht_su_bfmr)
			enable_bfee = true;

		if (true == enable_bfee) {
			/* BFee Functions */
			if (RTW_HAL_STATUS_SUCCESS !=
				hal_bf_hw_mac_init_bfee(hal_info,
							sta->wrole->hw_band)) {
				PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
					  "%s : Init HW MAC BFee Fail\n",
					  __func__);
				break;
			}
			/* BFee CSI parameters*/
			hal_info->hal_com->csi_para_ctrl_sel = false;
			if (RTW_HAL_STATUS_SUCCESS !=
				hal_bf_set_bfee_csi_para(hal_info,
					hal_info->hal_com->csi_para_ctrl_sel,
					sta)) {
				PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
					  "%s : Set BFee CSI Para Fail\n",
					  __func__);
				break;
			}
#ifdef RTW_WKARD_DYNAMIC_BFEE_CAP
			/* BB Workaround */
			rtw_hal_bb_dcr_en(hal_info, true);
#endif

			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
				  "%s : Enable HW BFee Function Success\n",
				  __func__);
		}
		hstatus = RTW_HAL_STATUS_SUCCESS;
	} while (0);

	return hstatus;
}

static enum rtw_hal_status
_hal_set_default_cctrl_tbl(struct hal_info_t *hal_info,
				 struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status sts = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_cctl_info cctrl, cctl_info_mask;
#ifdef RTW_WKARD_DEF_CMACTBL_CFG
	u16 cfg;
	enum rf_path path = (hal_info->hal_com->rfpath_tx_num == 1)?RF_PATH_B:RF_PATH_AB;
#endif
	_os_mem_set(hal_to_drvpriv(hal_info), &cctrl, 0, sizeof(struct mac_ax_cctl_info));
	_os_mem_set(hal_to_drvpriv(hal_info), &cctl_info_mask, 0, sizeof(struct mac_ax_cctl_info));

	if (NULL == sta)
		goto out;

	cctrl.txpwr_mode = 0;
	cctl_info_mask.txpwr_mode = 0x7;
#ifdef RTW_WKARD_DEF_CMACTBL_CFG
	cfg = rtw_hal_bb_cfg_cmac_tx_ant(hal_info, path);
	cctrl.ntx_path_en = cfg & 0x0f;
	cctl_info_mask.ntx_path_en = 0xF;
	cctrl.path_map_a = ((cfg>>4) & 0x03);
	cctl_info_mask.path_map_a = 0x3;
	cctrl.path_map_b = ((cfg>>6) & 0x03);
	cctl_info_mask.path_map_b = 0x3;
	cctrl.path_map_c = ((cfg>>8) & 0x03);
	cctl_info_mask.path_map_c = 0x3;
	cctrl.path_map_d = ((cfg>>10) & 0x03);
	cctl_info_mask.path_map_d = 0x3;
#else
	cctrl.ntx_path_en = 0x3;
	cctl_info_mask.ntx_path_en = 0xF;
	cctrl.path_map_a = 0x0;
	cctl_info_mask.path_map_a = 0x3;
	cctrl.path_map_b = 0x1;
	cctl_info_mask.path_map_b = 0x3;
	cctrl.path_map_c = 0x2;
	cctl_info_mask.path_map_c = 0x3;
	cctrl.path_map_d = 0x3;
	cctl_info_mask.path_map_d = 0x3;
#endif
	cctrl.antsel_a = 0x0;
	cctl_info_mask.antsel_a = 0x1;
	cctrl.antsel_b = 0x0;
	cctl_info_mask.antsel_b = 0x1;
	cctrl.antsel_c = 0x0;
	cctl_info_mask.antsel_c = 0x1;
	cctrl.antsel_d = 0x0;
	cctl_info_mask.antsel_d = 0x1;
	cctrl.doppler_ctrl = 0;
	cctl_info_mask.doppler_ctrl = 0x3;
	cctrl.txpwr_tolerence = 0;
	cctl_info_mask.txpwr_tolerence = 0xF;

#ifdef CONFIG_PHL_DEFAULT_MGNT_Q_RPT_EN
	cctrl.mgq_rpt_en = 1;
	cctl_info_mask.mgq_rpt_en = 1;
#endif
	sts = rtw_hal_cmc_tbl_cfg(hal_info, &cctrl ,&cctl_info_mask, sta->macid);

out:
	return sts;
}

static enum rtw_hal_status
_hal_update_cctrl_tbl(struct hal_info_t *hal_info,
			    struct rtw_phl_stainfo_t *sta)
{
	struct rtw_wifi_role_t *wrole = sta->wrole;
	enum rtw_hal_status sts = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_cctl_info cctrl, cctl_info_mask;

	_os_mem_set(hal_to_drvpriv(hal_info), &cctrl, 0, sizeof(struct mac_ax_cctl_info));
	_os_mem_set(hal_to_drvpriv(hal_info), &cctl_info_mask, 0, sizeof(struct mac_ax_cctl_info));

	if (NULL == sta)
		goto out;

	/*TODO - update cctrl tab from stainfo*/
	cctrl.disrtsfb = 1;
	cctl_info_mask.disrtsfb = 1;

	cctrl.disdatafb = 1;
	cctl_info_mask.disdatafb = 1;

	/*
	if (!cctrl.disdatafb)
		cctrl.arfr_ctrl = rtw_hal_bb_get_arfr_idx(hal_info, sta);
	*/

	if (wrole->cap.rty_lmt_rts == 0xFF) {
		cctrl.rts_txcnt_lmt_sel = 0;
	} else {
		cctrl.rts_txcnt_lmt_sel = 1;
		cctrl.rts_txcnt_lmt = wrole->cap.rty_lmt_rts & 0xF;
		cctl_info_mask.rts_txcnt_lmt = 0xF;
	}
	cctl_info_mask.rts_txcnt_lmt_sel = 1;
	cctrl.rts_rty_lowest_rate = (sta->chandef.band == BAND_ON_24G) ? (RTW_DATA_RATE_CCK1) : (RTW_DATA_RATE_OFDM6);
	cctl_info_mask.rts_rty_lowest_rate = 0xF;

	if (wrole->cap.rty_lmt == 0xFF) {
		cctrl.data_txcnt_lmt_sel = 0;
	} else {
		cctrl.data_txcnt_lmt_sel = 1;
		cctrl.data_tx_cnt_lmt = wrole->cap.rty_lmt & 0x3F;
		cctl_info_mask.data_tx_cnt_lmt = 0x3F;
	}
	cctl_info_mask.data_txcnt_lmt_sel = 1;

	/* hana_todo: follow wd agg_num settings before updating cmac tbl while addba handshake is ready */
/*	cctrl.max_agg_num_sel = 1;
	cctl_info_mask.max_agg_num_sel = 1;

	cctrl.max_agg_num = sta->asoc_cap.num_ampdu - 1;
	cctl_info_mask.max_agg_num = 0xFF;
*/
	if (cctrl.max_agg_num > 0x3F) {
		cctrl.ba_bmap = 1;
		cctl_info_mask.ba_bmap = 0x3;
	}

	if (sta->wrole->type == PHL_RTYPE_STATION || sta->wrole->type == PHL_RTYPE_TDLS) {
		cctrl.uldl = 1;
		cctl_info_mask.uldl = 1;
	} else {
		cctrl.uldl = 0;
		cctl_info_mask.uldl = 1;
	}

	cctrl.multi_port_id = sta->wrole->hw_port;
	cctl_info_mask.multi_port_id = 0x7;

	if (sta->wrole->type == PHL_RTYPE_AP) {
		cctrl.data_dcm = 0; /*(sta->asoc_cap.dcm_max_const_rx > 0)*/
		cctl_info_mask.data_dcm = 1;
	}

	if (sta->asoc_cap.pkt_padding == 3) {
		/* follow PPE threshold */
		u8 ppe16 = 0, ppe8 = 0;
		u8 nss = sta->asoc_cap.nss_rx;

		/* bw = 20MHz */
		ppe16 = (sta->asoc_cap.ppe_thr[nss - 1][CHANNEL_WIDTH_20] & 0x7);
		ppe8 = (sta->asoc_cap.ppe_thr[nss - 1][CHANNEL_WIDTH_20]>>3) & 0x7;

		if ((ppe16 != 7) && (ppe8 == 7)) {
			cctrl.nominal_pkt_padding = 2;
			cctl_info_mask.nominal_pkt_padding = 0x3;
		} else if (ppe8 != 7) {
			cctrl.nominal_pkt_padding = 1;
			cctl_info_mask.nominal_pkt_padding = 0x3;
		} else {
			cctrl.nominal_pkt_padding = 0;
			cctl_info_mask.nominal_pkt_padding = 0x3;
		}
		/* bw = 40MHz */
		ppe16 = (sta->asoc_cap.ppe_thr[nss - 1][CHANNEL_WIDTH_40] & 0x7);
		ppe8 = (sta->asoc_cap.ppe_thr[nss - 1][CHANNEL_WIDTH_40]>>3) & 0x7;

		if ((ppe16 != 7) && (ppe8 == 7)) {
			cctrl.nominal_pkt_padding40 = 2;
			cctl_info_mask.nominal_pkt_padding40 = 0x3;
		} else if (ppe8 != 7) {
			cctrl.nominal_pkt_padding40 = 1;
			cctl_info_mask.nominal_pkt_padding40 = 0x3;
		} else {
			cctrl.nominal_pkt_padding40 = 0;
			cctl_info_mask.nominal_pkt_padding40 = 0x3;
		}
		/* bw = 80MHz */
		ppe16 = (sta->asoc_cap.ppe_thr[nss - 1][CHANNEL_WIDTH_80] & 0x7);
		ppe8 = (sta->asoc_cap.ppe_thr[nss - 1][CHANNEL_WIDTH_80]>>3) & 0x7;

		if ((ppe16 != 7) && (ppe8 == 7)) {
			cctrl.nominal_pkt_padding80 = 2;
			cctl_info_mask.nominal_pkt_padding80 = 0x3;
		} else if (ppe8 != 7) {
			cctrl.nominal_pkt_padding80 = 1;
			cctl_info_mask.nominal_pkt_padding80 = 0x3;
		} else {
			cctrl.nominal_pkt_padding80 = 0;
			cctl_info_mask.nominal_pkt_padding80 = 0x3;
		}
	} else {
		cctrl.nominal_pkt_padding = sta->asoc_cap.pkt_padding;
		cctrl.nominal_pkt_padding40 = sta->asoc_cap.pkt_padding;
		cctrl.nominal_pkt_padding80 = sta->asoc_cap.pkt_padding;
		cctl_info_mask.nominal_pkt_padding = 0x3;
		cctl_info_mask.nominal_pkt_padding40 = 0x3;
		cctl_info_mask.nominal_pkt_padding80 = 0x3;
	}

	if (sta->wmode&WLAN_MD_11AX) {
          	/**
                 * bsr_queue_size_format:
                 * 1: buffer status unit is 802.11, HE mode
                 * 0: buffer status unit is 802.11, legacy mode
                 **/
		cctrl.bsr_queue_size_format = 1;
		cctl_info_mask.bsr_queue_size_format = 1;
	}

	sts = rtw_hal_cmc_tbl_cfg(hal_info, &cctrl, &cctl_info_mask, sta->macid);

out:
	return sts;
}

static enum rtw_hal_status
_hal_update_dctrl_tbl(struct hal_info_t *hal_info,
		      struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status sts = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_dctl_info dctrl, dctl_info_mask;

	if (NULL == sta)
		goto out;

	_os_mem_set(hal_to_drvpriv(hal_info), &dctrl, 0, sizeof(struct mac_ax_dctl_info));
	_os_mem_set(hal_to_drvpriv(hal_info), &dctl_info_mask, 0, sizeof(struct mac_ax_dctl_info));

#ifdef CONFIG_PHL_CSUM_OFFLOAD_RX
	dctrl.chksum_offload_en = 1;
	dctl_info_mask.chksum_offload_en = 1;
	dctrl.with_llc = 1;
	dctl_info_mask.with_llc = 1;
#endif /*CONFIG_PHL_CSUM_OFFLOAD_RX*/
	sts = rtw_hal_dmc_tbl_cfg(hal_info, &dctrl, &dctl_info_mask, sta->macid);

out:
	return sts;
}

enum rtw_hal_status
_hal_update_ba_cam(struct hal_info_t *hal_info, u8 valid, u16 macid,
		   u8 dialog_token, u16 timeout, u16 start_seq_num,
		   u16 ba_policy, u16 tid, u16 buf_size, u8 camid)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	void *drv = hal_to_drvpriv(hal_info);
	struct mac_ax_bacam_info ba_cam = {0};
	FUNCIN();
	_os_mem_set(drv, &ba_cam, 0, sizeof(ba_cam));
	ba_cam.valid = valid;
	ba_cam.init_req = 1;
	ba_cam.entry_idx = camid;
	ba_cam.tid = tid;
	ba_cam.macid = (u8)macid;
	if (buf_size > 64)
		ba_cam.bmap_size = 4;
	else
		ba_cam.bmap_size = 0;
	ba_cam.ssn = start_seq_num;

	PHL_INFO("[BACAM] ba_cam.valid = %d, ba_cam.init_req = %d, ba_cam.entry_idx = %d\n",
			 ba_cam.valid,
			 ba_cam.init_req,
			 ba_cam.entry_idx);
	PHL_INFO("[BACAM] ba_cam.tid = %d, ba_cam.macid = %d, ba_cam.bmap_size = %d\n",
			 ba_cam.tid,
			 ba_cam.macid,
			 ba_cam.bmap_size);
	PHL_INFO("[BACAM] ba_cam.ssn = 0x%X\n",
			 ba_cam.ssn);

	hal_status = rtw_hal_bacam_cfg(hal_info, &ba_cam);

	if (RTW_HAL_STATUS_FAILURE == hal_status) {
		PHL_WARN("rtw_hal_bacam_cfg fail 0x%08X\n", hal_status);

	}

	FUNCOUT();
	return hal_status;
}

enum rtw_hal_status
rtw_hal_start_ba_session(void *hal, struct rtw_phl_stainfo_t *sta,
                            u8 dialog_token, u16 timeout, u16 start_seq_num,
                            u16 ba_policy, u16 tid, u16 buf_size)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	u8 idx = 0;

	if (sta->hal_sta->ba_ctl.count == MAX_BAENTRY) {
		PHL_INFO("No avail standard entry found\n");
		if (tid != 0)
			goto out;
		else {
			for (idx = 0; idx < MAX_BAENTRY; idx++) {
				if (sta->hal_sta->ba_ctl.tid[idx] != 0) {
					PHL_INFO("Remove old entry(%d) tid(%d)\n",
						idx, sta->hal_sta->ba_ctl.tid[idx]);
					hal_status = _hal_update_ba_cam(hal_info, 0, sta->macid, 0,
											0, 0, 0, 0, 0, idx);
					if (hal_status == RTW_HAL_STATUS_SUCCESS) {
						sta->hal_sta->ba_ctl.used_map[idx] = false;
						sta->hal_sta->ba_ctl.count--;
						break;
					}
				} else {
					PHL_INFO("Use existing entry(%d)\n", idx);
					sta->hal_sta->ba_ctl.used_map[idx] = false;
					sta->hal_sta->ba_ctl.count--;
					break;
				}
			}
		}
	}
	for (idx = 0; idx < MAX_BAENTRY; idx++) {
		if (!sta->hal_sta->ba_ctl.used_map[idx])
			break;
	}
	if (idx == MAX_BAENTRY) {
		PHL_WARN("No avail standard entry found but count is(%d)\n",
			sta->hal_sta->ba_ctl.count);
		goto out;
	}
	hal_status = _hal_update_ba_cam(hal_info, 1, sta->macid, dialog_token,
									timeout, start_seq_num, ba_policy, tid,
									buf_size, idx);
	if (RTW_HAL_STATUS_SUCCESS != hal_status)
		goto out;

	sta->hal_sta->ba_ctl.used_map[idx] = 1;
	sta->hal_sta->ba_ctl.tid[idx] = (u8)tid;
	sta->hal_sta->ba_ctl.count++;
out:
	return hal_status;
}

enum rtw_hal_status
rtw_hal_stop_ba_session(void *hal, struct rtw_phl_stainfo_t *sta, u16 tid)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	u8 i;

	for (i = 0; i < MAX_BAENTRY; i++) {
		if (sta->hal_sta->ba_ctl.used_map[i] &&
				((u8)tid == sta->hal_sta->ba_ctl.tid[i])) {
			hal_status = _hal_update_ba_cam(hal_info, 0, sta->macid, 0,
											0, 0, 0, 0, 0, i);
			break;
		}
	}
	if (RTW_HAL_STATUS_SUCCESS != hal_status)
		goto out;
	sta->hal_sta->ba_ctl.used_map[i] = 0;
	sta->hal_sta->ba_ctl.tid[i] = 0xff;
	sta->hal_sta->ba_ctl.count--;
out:
	return hal_status;
}



enum rtw_hal_status
rtw_hal_stainfo_init(void *hal, struct rtw_phl_stainfo_t *sta)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	void *drv = hal_to_drvpriv(hal_info);

	sta->hal_sta = _os_mem_alloc(drv, sizeof(struct rtw_hal_stainfo_t));
	if (sta->hal_sta == NULL) {
		PHL_ERR("alloc hal_sta failed\n");
		goto error_exit;
	}

	sta->hal_sta->hw_cfg_tab =
		_os_mem_alloc(drv, sizeof(struct rtw_hw_cfg_tab));
	if (sta->hal_sta->hw_cfg_tab == NULL) {
		PHL_ERR("alloc hw_cfg_tab failed\n");
		goto error_hsta_mem;
	}

	hal_status = rtw_hal_bb_stainfo_init(hal_info, sta);
	if (hal_status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("alloc bb_stainfo failed\n");
		goto error_hw_cfg_tab;
	}
	/* Init lock for tx statistics */
	_os_spinlock_init(drv, &sta->hal_sta->trx_stat.tx_sts_lock);
	/* Init STA RSSI Statistics */
	_hal_sta_rssi_init(sta);

	return hal_status;

error_hw_cfg_tab :
	if (sta->hal_sta->hw_cfg_tab) {
		_os_mem_free(drv, sta->hal_sta->hw_cfg_tab,
				sizeof(struct rtw_hw_cfg_tab));
		sta->hal_sta->hw_cfg_tab = NULL;
	}
error_hsta_mem :
	if (sta->hal_sta) {
		_os_mem_free(drv, sta->hal_sta,
				sizeof(struct rtw_hal_stainfo_t));
		sta->hal_sta = NULL;
	}
error_exit :
	return hal_status;
}

enum rtw_hal_status
rtw_hal_stainfo_deinit(void *hal, struct rtw_phl_stainfo_t *sta)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	void *drv = hal_to_drvpriv(hal_info);

	if (sta->hal_sta) {
		/* Free lock for tx statistics */
		_os_spinlock_free(drv, &sta->hal_sta->trx_stat.tx_sts_lock);
		hal_status = rtw_hal_bb_stainfo_deinit(hal_info, sta);
		if (hal_status != RTW_HAL_STATUS_SUCCESS)
			PHL_ERR("bb_stainfo deinit failed\n");

		if (sta->hal_sta->hw_cfg_tab) {
			_os_mem_free(drv, sta->hal_sta->hw_cfg_tab,
					sizeof(struct rtw_hw_cfg_tab));
			sta->hal_sta->hw_cfg_tab = NULL;
		}

		_os_mem_free(drv, sta->hal_sta,
				sizeof(struct rtw_hal_stainfo_t));
		sta->hal_sta = NULL;
	}

	return hal_status;
}
static void _hal_sta_set_default_value(struct hal_info_t *hal_info,
        struct rtw_phl_stainfo_t *sta)
{
	u8 i;
	sta->hal_sta->ra_info.ra_registered = false;
	sta->hal_sta->ba_ctl.count = 0;
	for (i = 0; i<MAX_BAENTRY; i++) {
		sta->hal_sta->ba_ctl.tid[i] = 0xff;
		sta->hal_sta->ba_ctl.used_map[i] = 0;
	}
}

enum rtw_hal_status
rtw_hal_add_sta_entry(void *hal, struct rtw_phl_stainfo_t *sta)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	_hal_sta_set_default_value(hal_info, sta);

	/*add mac address-cam*/
	if (rtw_hal_mac_addr_cam_add_entry(hal_info, sta) !=
					RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("rtw_hal_mac_addr_cam_add_entry failed\n");
		goto _exit;
	}

	/*update default cmac table*/
	if (_hal_set_default_cctrl_tbl(hal_info, sta) !=
					RTW_HAL_STATUS_SUCCESS) {
		PHL_WARN("_hal_set_default_cctrl_tbl failed\n");
		/* goto _exit; */ /* shall be unmark after header FW is ready */
	}

	if (_hal_update_dctrl_tbl(hal_info, sta) !=
					RTW_HAL_STATUS_SUCCESS) {
		PHL_WARN("_hal_set_default_dctrl_tbl failed\n");
		/* goto _exit; */
	}

	/*add bb stainfo*/
	if (rtw_hal_bb_stainfo_add(hal_info, sta) !=
					RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("rtw_hal_bb_stainfo_add failed\n");
		goto _err_bbsta_add;
	}
	return RTW_HAL_STATUS_SUCCESS;

_err_bbsta_add:
	rtw_hal_mac_addr_cam_del_entry(hal_info, sta);
_exit:
	return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_update_sta_entry(void *hal, struct rtw_phl_stainfo_t *sta,
				bool is_connect)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	enum phl_upd_mode mode = PHL_UPD_STA_CON_DISCONN;

	/*update cmac table*/
	if (RTW_HAL_STATUS_SUCCESS != _hal_update_cctrl_tbl(hal_info, sta))
		PHL_WARN("_hal_update_cctrl_tbl failed\n");

	/*update dmac table*/
	if (RTW_HAL_STATUS_SUCCESS != _hal_update_dctrl_tbl(hal_info, sta))
		PHL_WARN("_hal_update_dctrl_tbl failed\n");

	/*change mac address-cam & mac_h2c_join_info*/
	hal_status = rtw_hal_mac_addr_cam_change_entry(hal_info, sta, mode, is_connect);
	if (hal_status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("rtw_hal_mac_addr_cam_change_entry failed\n");
	}

	if (is_connect) {

		if (RTW_HAL_STATUS_SUCCESS != _hal_bfee_init(hal_info, sta)) {
			PHL_ERR("_hal_bfee_init Fail!\n");
		}

		if (sta->hal_sta->rssi_stat.assoc_rssi == 0
                #ifdef CONFIG_PHL_TDLS
			/* There is no association frame for TDLS connection */
			&& sta->wrole->type != PHL_RTYPE_TDLS
                #endif
		) {
			PHL_ERR("%s macid:%d assoc_rssi == 0\n", __func__, sta->macid);
			_os_warn_on(1);
		}

		hal_status = rtw_hal_bb_upt_ramask(hal_info, sta);
		if (hal_status != RTW_HAL_STATUS_SUCCESS)
			PHL_ERR("rtw_hal_bb_upt_ramask failed\n");

		hal_status = rtw_hal_bb_ra_register(hal_info, sta);
		if (hal_status != RTW_HAL_STATUS_SUCCESS)
			PHL_ERR("rtw_hal_bb_ra_register failed\n");
		hal_info->hal_com->assoc_sta_cnt++;

		if (sta->wmode & WLAN_MD_11AX) {
			rtw_hal_bb_set_sta_id(hal_info, sta->aid, sta->wrole->hw_band);
			rtw_hal_bb_set_bss_color(hal_info, sta->asoc_cap.bsscolor,
				sta->wrole->hw_band);
			rtw_hal_bb_set_tb_pwr_ofst(hal_info, 0, sta->wrole->hw_band);
		}
		/* reset rssi stat value */
		sta->hal_sta->rssi_stat.ma_rssi_mgnt = 0;
	}
	else {
		hal_status = rtw_hal_bb_ra_deregister(hal_info, sta);
		if (hal_status != RTW_HAL_STATUS_SUCCESS)
			PHL_ERR("rtw_hal_bb_ra_deregister failed\n");
		hal_info->hal_com->assoc_sta_cnt--;
		/* reset drv rssi_stat */
		_hal_sta_rssi_init(sta);
		_hal_sta_set_default_value(hal_info, sta);
	}
	/* reset bb rssi_stat */
	rtw_hal_bb_media_status_update(hal_info, sta, is_connect);

	return hal_status;
}

enum rtw_hal_status
rtw_hal_change_sta_entry(void *hal, struct rtw_phl_stainfo_t *sta,
						enum phl_upd_mode mode)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	bool is_connect = false;

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s: sta->macid(0x%X), mode(%d)\n",
		__FUNCTION__, sta->macid , mode);
	/*update cmac table*/
	if (RTW_HAL_STATUS_SUCCESS != _hal_update_cctrl_tbl(hal_info, sta))
		PHL_WARN("_hal_update_cctrl_tbl failed\n");

	/*update dmac table*/
	if (RTW_HAL_STATUS_SUCCESS != _hal_update_dctrl_tbl(hal_info, sta))
		PHL_WARN("_hal_update_dctrl_tbl failed\n");

	/*change mac address-cam & mac_h2c_join_info */
	is_connect = (sta->wrole->mstate == MLME_LINKED) ? true : false;
	hal_status = rtw_hal_mac_addr_cam_change_entry(hal_info, sta, mode, is_connect);
	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("rtw_hal_mac_addr_cam_change_entry failed\n");

	hal_status = rtw_hal_bb_ra_update(hal_info, sta);
	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("rtw_hal_bb_ra_update failed\n");

	return hal_status;
}

enum rtw_hal_status
rtw_hal_del_sta_entry(void *hal, struct rtw_phl_stainfo_t *sta)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	_hal_sta_set_default_value(hal_info, sta);

	hal_status = rtw_hal_mac_addr_cam_del_entry(hal_info, sta);
	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("mac_addr_cam_del_entry failed\n");

	hal_status = rtw_hal_bb_stainfo_delete(hal_info, sta);
	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("bb_stainfo deinit failed\n");

	return hal_status;
}

u8 rtw_hal_get_sta_rssi(struct rtw_phl_stainfo_t *sta)
{
	u8 rssi = (sta->hal_sta->rssi_stat.rssi >> 1);

	return rssi;
}

u8 rtw_hal_get_sta_rssi_bcn(struct rtw_phl_stainfo_t *sta)
{
	u8 rssi = (sta->hal_sta->rssi_stat.rssi_bcn >> 1);

	return rssi;
}

bool rtw_hal_is_sta_linked(void *hal, struct rtw_phl_stainfo_t *sta)
{
	return (sta->hal_sta->ra_info.ra_registered == true) ? true : false;
}

enum rtw_hal_status
rtw_hal_set_edca(void *hal, struct rtw_wifi_role_t *wrole, u8 ac, u32 edca)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status;


	hal_status = rtw_hal_mac_set_edca(hal_info->hal_com, wrole->hw_band,
					  wrole->hw_wmm, ac, edca);

	return hal_status;
}

enum rtw_hal_status
rtw_hal_cfg_tx_ampdu(void *hal, struct rtw_phl_stainfo_t *sta)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hsts = RTW_HAL_STATUS_FAILURE;

	/* update ampdu configuration */
	if (64 == sta->asoc_cap.num_ampdu)
		hsts = rtw_hal_mac_set_hw_ampdu_cfg(hal_info, 0, 0x3F, 0xA5);
	else if (128 == sta->asoc_cap.num_ampdu)
		hsts = rtw_hal_mac_set_hw_ampdu_cfg(hal_info, 0, 0x7F, 0xAB);

	if (RTW_HAL_STATUS_SUCCESS != hsts)
		goto out;

	/* todo: update cmac table */

out:
	return hsts;
}

enum rtw_hal_status
rtw_hal_set_sta_rx_sts(struct rtw_phl_stainfo_t *sta, u8 rst,
									struct rtw_r_meta_data *meta)
{
	if (rst) {
		sta->hal_sta->trx_stat.rx_ok_cnt = 0;
		sta->hal_sta->trx_stat.rx_err_cnt = 0;
	} else {
		if (meta->crc32 || meta->icverr)
			sta->hal_sta->trx_stat.rx_err_cnt++;
		else
			sta->hal_sta->trx_stat.rx_ok_cnt++;
	}
	/* TODO: rx_rate_plurality */
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_query_rainfo(void *hal, struct rtw_hal_stainfo_t *hal_sta,
		     struct rtw_phl_rainfo *phl_rainfo)
{
	enum rtw_hal_status hal_sts = RTW_HAL_STATUS_FAILURE;

	hal_sts = rtw_hal_bb_query_rainfo(hal, hal_sta, phl_rainfo);


	return hal_sts;
}

/**
 * rtw_hal_query_txsts_rpt() - get txok and tx retry info
 * @hal:		struct hal_info_t *
 * @macid:		indicate the first macid that you want to query.
 * Return rtw_hal_bb_query_txsts_rpt's return value in enum rtw_hal_status type.
 */
enum rtw_hal_status
rtw_hal_query_txsts_rpt(void *hal, u16 macid)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	/*get tx ok and tx retry statistics*/
	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_bb_query_txsts_rpt(hal_info, macid, 0xFFFF))
		return RTW_HAL_STATUS_FAILURE;
	else
		return RTW_HAL_STATUS_SUCCESS;
}

