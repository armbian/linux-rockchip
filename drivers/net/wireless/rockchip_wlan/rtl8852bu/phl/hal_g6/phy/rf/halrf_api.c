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

 
#include "halrf_precomp.h"

u32 phlrf_psd_log2base(struct rf_info *rf, u32 val)
{
	u32 j;
	u32 tmp, tmp2, val_integerd_b = 0, tindex, shiftcount = 0;
	u32 result, val_fractiond_b = 0;
	u32 table_fraction[21] = {
		0, 432, 332, 274, 232, 200, 174, 151, 132, 115,
		100, 86, 74, 62, 51, 42, 32, 23, 15, 7, 0};

	if (val == 0)
		return 0;

	tmp = val;

	while (1) {
		if (tmp == 1)
			break;

		tmp = (tmp >> 1);
		shiftcount++;
	}

	val_integerd_b = shiftcount + 1;

	tmp2 = 1;
	for (j = 1; j <= val_integerd_b; j++)
		tmp2 = tmp2 * 2;

	tmp = (val * 100) / tmp2;
	tindex = tmp / 5;

	if (tindex > 20)
		tindex = 20;

	val_fractiond_b = table_fraction[tindex];

	result = val_integerd_b * 100 - val_fractiond_b;

	return result;
}

void phlrf_rf_lna_setting(struct rf_info *rf, enum phlrf_lna_set type)
{
	struct rtw_hal_com_t *hal_i = rf->hal_com;

	switch (hal_i->chip_id) {
#ifdef RF_8852A_SUPPORT
			case CHIP_WIFI6_8852A:
				break;
#endif
			default:
				break;
		}

}

void halrf_bkp(struct rf_info *rf, u32 *bp_reg, u32 *bp, u32 reg_num)
{
	u32 i;

	for (i = 0; i < reg_num; i++)
		bp[i] = halrf_rreg(rf, bp_reg[i], MASKDWORD);
}

void halrf_bkprf(struct rf_info *rf, u32 *bp_reg, u32 bp[][4], u32 reg_num, u32 path_num)
{
	u32 i, j;

	for (i = 0; i < reg_num; i++) {
		for (j = 0; j < path_num; j++)
			bp[i][j] = halrf_rrf(rf, j, bp_reg[i], MASKRF);
	}
}

void halrf_reload_bkp(struct rf_info *rf, u32 *bp_reg, u32 *bp, u32 reg_num)
{
	u32 i;

	for (i = 0; i < reg_num; i++)
		halrf_wreg(rf, bp_reg[i], MASKDWORD, bp[i]);
}

void halrf_reload_bkprf(struct rf_info *rf,
		       u32 *bp_reg,
		       u32 bp[][4],
		       u32 reg_num,
		       u8 path_num)
{
	u32 i, path;

	for (i = 0; i < reg_num; i++) {
		for (path = 0; path < path_num; path++)
			halrf_wrf(rf, (enum rf_path)path, bp_reg[i],
				       MASKRF, bp[i][path]);
	}
}

u8 halrf_kpath(struct rf_info *rf, enum phl_phy_idx phy_idx)
{
	struct rtw_hal_com_t *hal_i = rf->hal_com;

u8 path = 0;

	switch (hal_i->chip_id) {
#ifdef RF_8852A_SUPPORT
		case CHIP_WIFI6_8852A:
			path = halrf_kpath_8852a(rf, phy_idx);
			break;
#endif
		default:
			break;
	}
	return path;
}

void halrf_wait_rx_mode(struct rf_info *rf, u8 kpath)
{
	u8 path, rf_mode = 0;
	u16 count = 0;

	for (path = 0; path < 4; path++) {
		if (kpath & BIT(path)) {
			rf_mode = (u8)halrf_rrf(rf, path, 0x00, MASKRFMODE);

			while (rf_mode == 2 && count < 2500) {
				rf_mode = (u8)halrf_rrf(rf, path, 0x00, MASKRFMODE);
				halrf_delay_us(rf, 2);
				count++;
			}
			RF_DBG(rf, DBG_RF_RFK,
			       "[RFK] Wait S%d to Rx mode!! (count = %d)\n", path, count);
		}
	}
}

void halrf_tmac_tx_pause(struct rf_info *rf, enum phl_phy_idx band_idx, bool is_pause)
{
	halrf_tx_pause(rf, band_idx, is_pause, PAUSE_RSON_RFK);

	RF_DBG(rf, DBG_RF_RFK,"[RFK] Band%d Tx Pause %s!!\n",
	       band_idx, is_pause ? "on" : "off");

	if (is_pause)
		halrf_wait_rx_mode(rf, halrf_kpath(rf, band_idx));
}

void halrf_trigger_thermal(struct rf_info *rf)
{
	struct rtw_hal_com_t *hal_i = rf->hal_com;

	switch (hal_i->chip_id) {
#ifdef RF_8852A_SUPPORT
		case CHIP_WIFI6_8852A:
			halrf_trigger_thermal_8852a(rf, RF_PATH_A);
			halrf_trigger_thermal_8852a(rf, RF_PATH_B);
			break;
#endif
		default:
			break;
	}
}

u8 halrf_only_get_thermal(struct rf_info *rf, enum rf_path path)
{
	struct rtw_hal_com_t *hal_i = rf->hal_com;

	switch (hal_i->chip_id) {
#ifdef RF_8852A_SUPPORT
		case CHIP_WIFI6_8852A:
			return halrf_only_get_thermal_8852a(rf, path);
			break;
#endif
		default:
			break;
	}

	return 0;
}

void halrf_btc_rfk_ntfy(struct rf_info *rf, u8 phy_map, enum halrf_rfk_type type,
			enum halrf_rfk_process process)
{
	u32 cnt = 0;
	u8 band;
	/*idx : use BIT mask for RF path PATH A: 1, PATH B:2, PATH AB:3*/

	band = rf->hal_com->band[(phy_map & 0x30) >> 5].cur_chandef.band;

	phy_map = (band << 6) | phy_map;

	RF_DBG(rf, DBG_RF_RFK, "[RFK] RFK notify (%s / PHY%d / K_type = %d / path_idx = %d / process = %s)\n",
		band == 0 ? "2G" : (band == 1 ? "5G" : "6G"), (phy_map & 0x30) >> 5, type,
		phy_map & 0xf, process == 0 ? "RFK_STOP" : (process == 1 ? "RFK_START" :
		(process == 2 ? "ONE-SHOT_START" : "ONE-SHOT_STOP")));
#if 1
	if (process == RFK_START && rf->is_bt_iqk_timeout == false) {
		while (halrf_btc_ntfy(rf, phy_map, type, process) == 0 && cnt < 2500) {
			halrf_delay_us(rf, 40);
			cnt++;
		}
		if (cnt == 2500) {
			RF_DBG(rf, DBG_RF_RFK, "[RFK] Wait BT IQK timeout!!!!\n");
			rf->is_bt_iqk_timeout = true;
		}
	} else
		halrf_btc_ntfy(rf, phy_map, type, process);
#endif
}

void halrf_fcs_init(struct rf_info *rf)
{
	struct rtw_hal_com_t *hal_com = rf->hal_com;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A)
		halrf_fcs_init_8852a(rf);
#endif
}

void halrf_fast_chl_sw_backup(struct rf_info *rf, u8 chl_index, u8 t_index)
{
	u32 t[2];

	t[0] = chl_index;
	t[1] = t_index;

	halrf_fill_h2c_cmd(rf, 8, FWCMD_H2C_BACKUP_RFK, 0xa, H2CB_TYPE_DATA, t);
	RF_DBG(rf, DBG_RF_RFK, "FWCMD_H2C_BACKUP_RFK chl=%d t=%d\n", chl_index, t_index);
}

void halrf_fast_chl_sw_reload(struct rf_info *rf, u8 chl_index, u8 t_index)
{
	u32 t[2];

	t[0] = chl_index;
	t[1] = t_index;

	halrf_fill_h2c_cmd(rf, 8, FWCMD_H2C_RELOAD_RFK, 0xa, H2CB_TYPE_DATA, t);
	RF_DBG(rf, DBG_RF_RFK, "FWCMD_H2C_RELOAD_RFK chl=%d t=%d\n", chl_index, t_index);
}

void  halrf_quick_check_rf(void *rf_void)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct rtw_hal_com_t *hal_com = rf->hal_com;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A)
		halrf_quick_check_rfrx_8852a(rf);
#endif
#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B)
		halrf_quick_checkrf_8852b(rf);
#endif
}

void halrf_wifi_event_notify(void *rf_void,
			enum phl_msg_evt_id event, enum phl_phy_idx phy_idx)
{
	struct rf_info *rf = (struct rf_info *)rf_void;

	if (event == MSG_EVT_SCAN_START) {
		halrf_tssi_default_txagc(rf, phy_idx, true);
		halrf_tssi_set_avg(rf, phy_idx, true);
		halrf_dpk_track_onoff(rf, false);
	} else if (event == MSG_EVT_SCAN_END) {
		halrf_tssi_default_txagc(rf, phy_idx, false);
		halrf_tssi_set_avg(rf, phy_idx, false);
		halrf_dpk_track_onoff(rf, true);
	} else if (event == MSG_EVT_DBG_RX_DUMP) {
		halrf_quick_check_rf(rf);
	} else if (event == MSG_EVT_SWCH_START) {
		halrf_tssi_backup_txagc(rf, phy_idx, true);
	}
}

void halrf_write_fwofld_start(struct rf_info *rf)
{
#ifdef HALRF_CONFIG_FW_IO_OFLD_SUPPORT
	bool fw_ofld = rf->phl_com->dev_cap.fw_cap.offload_cap & BIT(0);

	rf->fw_ofld_enable = true;

	RF_DBG(rf, DBG_RF_FW, "======> %s   fw_ofld=%d   rf->fw_ofld_enable=%d\n",
		__func__, fw_ofld, rf->fw_ofld_enable);
#endif
}

void halrf_write_fwofld_end(struct rf_info *rf)
{
#ifdef HALRF_CONFIG_FW_IO_OFLD_SUPPORT
	struct rtw_mac_cmd cmd = {0};
	struct halrf_fw_offload *fwofld_info = &rf->fwofld;
	bool fw_ofld = rf->phl_com->dev_cap.fw_cap.offload_cap & BIT(0);
	u32 rtn;

	RF_DBG(rf, DBG_RF_FW,
		"======>%s src=%d type=%d lc=%d rf_path=%d\n",
		__func__, fwofld_info->src, fwofld_info->type,
		fwofld_info->lc, fwofld_info->rf_path);

	RF_DBG(rf, DBG_RF_FW,
		"======>%s offset=0x%x mask=0x%x value=0x%x fw_ofld=%d\n",
		__func__, fwofld_info->offset, fwofld_info->mask,
		fwofld_info->value, fw_ofld);

	cmd.src = fwofld_info->src;
	cmd.type = fwofld_info->type;
	cmd.lc = 1;
	cmd.rf_path = fwofld_info->rf_path;
	cmd.offset = fwofld_info->offset;
	cmd.value = fwofld_info->value;
	cmd.mask = fwofld_info->mask;

	if (fw_ofld) {
		rtn = halrf_mac_add_cmd_ofld(rf, &cmd);
		if (rtn) {
			RF_WARNING("======>%s return fail error code = %d !!!\n",
				__func__, rtn);
		}
	}

	rf->fw_ofld_enable = false;
#endif
}

void halrf_ctrl_bw_ch(void *rf_void, enum phl_phy_idx phy, u8 central_ch,
				enum band_type band, enum channel_width bw)
{
	struct rf_info *rf = (struct rf_info *)rf_void;

	halrf_ctl_ch(rf, central_ch);
	halrf_ctl_bw(rf, bw);
	halrf_rxbb_bw(rf, phy, bw);
}

u32 halrf_test_event_trigger(void *rf_void,
		enum phl_phy_idx phy, enum halrf_event_idx idx, enum halrf_event_func func) {

	struct rf_info *rf = (struct rf_info *)rf_void;

	switch (idx) {
		case RF_EVENT_PWR_TRK:
			if (func == RF_EVENT_OFF)
				halrf_tssi_disable(rf, phy);
			else if (func == RF_EVENT_ON)
				halrf_tssi_enable(rf, phy);
			else if (func == RF_EVENT_TRIGGER)
				halrf_tssi_trigger(rf, phy);
		break;

		case RF_EVENT_IQK:
			if (func == RF_EVENT_OFF)
				halrf_iqk_onoff(rf, true);
			else if (func == RF_EVENT_ON)
				halrf_iqk_onoff(rf, false);
			else if (func == RF_EVENT_TRIGGER) {
				halrf_nbiqk_enable(rf, false); 		
				halrf_iqk_trigger(rf, phy, false);
			}
		break;

		case RF_EVENT_DPK:
			if (func == RF_EVENT_OFF)
				halrf_dpk_onoff(rf, false);
			else if (func == RF_EVENT_ON)
				halrf_dpk_onoff(rf, true);
			else if (func == RF_EVENT_TRIGGER)
				halrf_dpk_trigger(rf, phy, false);
		break;

		case RF_EVENT_TXGAPK:
			if (func == RF_EVENT_OFF)
				halrf_gapk_disable(rf, phy);
			else if (func == RF_EVENT_ON)
				halrf_gapk_enable(rf, phy);
			else if (func == RF_EVENT_TRIGGER)
				halrf_gapk_trigger(rf, phy, true);
		break;

		case RF_EVENT_DACK:
			if (func == RF_EVENT_OFF)
				halrf_dack_onoff(rf, false);
			else if (func == RF_EVENT_ON)
				halrf_dack_onoff(rf, true);
			else if (func == RF_EVENT_TRIGGER)
				halrf_dack_trigger(rf, true);
		break;

		default:
		break;
	}
	return 0;
}


