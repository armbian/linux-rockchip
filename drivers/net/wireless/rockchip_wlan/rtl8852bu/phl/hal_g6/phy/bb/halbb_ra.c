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
#include "halbb_precomp.h"
#ifdef HALBB_RA_SUPPORT

const u16 bb_phy_rate_table[] = {
	/*CCK*/
	1, 2, 5, 11,
	/*OFDM*/
	6, 9, 12, 18, 24, 36, 48, 54,
	/*HT/VHT-1ss LGI*/
	6, 13, 19, 26, 39, 52, 58, 65, 78, 87, 98, 108,
	/*HT/VHT-2ss LGI*/
	13, 26, 39, 52, 78, 104, 117, 130, 156, 173, 195, 217,
	/*HT/VHT-3ss LGI*/
	19, 39, 58, 78, 117, 156, 175, 195, 234, 260, 293, 325,
	/*HT/VHT-4ss LGI*/
	26, 52, 78, 104, 156, 208, 234, 260, 312, 347, 390, 433,
}; /*HE[3.2] = VHT[LGI] * 1.125*/

bool halbb_is_he_rate(struct bb_info *bb, u16 rate)
{
	return ((((rate & 0x1ff) >= BB_HE_1SS_MCS0) &&
		((rate & 0x1ff) <= BB_HE_1SS_MCS(11))) ||
		(((rate & 0x1ff) >= BB_HE_2SS_MCS0) &&
		((rate & 0x1ff) <= BB_HE_2SS_MCS(11))) ||
		(((rate & 0x1ff) >= BB_HE_3SS_MCS0) &&
		((rate & 0x1ff) <= BB_HE_3SS_MCS(11))) ||
		(((rate & 0x1ff) >= BB_HE_4SS_MCS0) &&
		((rate & 0x1ff) <= BB_HE_4SS_MCS(11)))) ? true : false;
}

bool halbb_is_vht_rate(struct bb_info *bb, u16 rate)
{
	return ((((rate & 0x1ff) >= BB_VHT_1SS_MCS0) &&
		((rate & 0x1ff) <= BB_VHT_1SS_MCS(9))) ||
		(((rate & 0x1ff) >= BB_VHT_2SS_MCS0) &&
		((rate & 0x1ff) <= BB_VHT_2SS_MCS(9))) ||
		(((rate & 0x1ff) >= BB_VHT_3SS_MCS0) &&
		((rate & 0x1ff) <= BB_VHT_3SS_MCS(9))) ||
		(((rate & 0x1ff) >= BB_VHT_4SS_MCS0) &&
		((rate & 0x1ff) <= BB_VHT_4SS_MCS(9)))) ? true : false;
}

bool halbb_is_ht_rate(struct bb_info *bb, u16 rate)
{
	return (((rate & 0x1ff) >= BB_HT_MCS0) &&
		((rate & 0x1ff) <= BB_HT_MCS(31))) ? true : false;
}

bool halbb_is_ofdm_rate(struct bb_info *bb, u16 rate)
{
	return (((rate & 0x1ff) >= BB_06M) &&
		((rate & 0x1ff) <= BB_54M)) ? true : false;
}

bool halbb_is_cck_rate(struct bb_info *bb, u16 rate)
{
	return ((rate & 0x1ff) <= BB_11M) ? true : false;
}

u8 halbb_legacy_rate_2_spec_rate(struct bb_info *bb, u16 rate)
{
	u8 rate_idx = 0x0;
	u8 legacy_spec_rate_t[8] = {BB_SPEC_RATE_6M, BB_SPEC_RATE_9M,
				    BB_SPEC_RATE_12M, BB_SPEC_RATE_18M,
				    BB_SPEC_RATE_24M, BB_SPEC_RATE_36M,
				    BB_SPEC_RATE_48M, BB_SPEC_RATE_54M};

	rate_idx = rate - BB_06M;
	return legacy_spec_rate_t[rate_idx];
}

u8 halbb_mgnt_2_hw_rate(u8 rate)
{
	u8 ret = 0;

	/*exclude BSS basic bit*/
	rate &= 0x7f;

	/*a/b/g mode only, there is no requirement for n/ac/ax mode*/
	if (rate > 108)
		return ret;

	/*unit:0.5Mbps*/
	switch (rate) {
	case 2:
		ret = BB_01M;
		break;
	case 4:
		ret = BB_02M;
		break;
	case 11:
		ret = BB_05M;
		break;
	case 22:
		ret = BB_11M;
		break;
	case 12:
		ret = BB_06M;
		break;
	case 18:
		ret = BB_09M;
		break;
	case 24:
		ret = BB_12M;
		break;
	case 36:
		ret = BB_18M;
		break;
	case 48:
		ret = BB_24M;
		break;
	case 72:
		ret = BB_36M;
		break;
	case 96:
		ret = BB_48M;
		break;
	case 108:
		ret = BB_54M;
		break;
	default:
		break;
	}

	return ret;
}

u8 halbb_mcs_ss_to_fw_rate_idx(u8 mode, u8 mcs, u8 ss)
{
	u8 fw_rate_idx = 0;

	if (mode == 3) {
		fw_rate_idx = RATE_HE1SS_MCS0 + (ss - 1) * 12; // MAX HE MCS is MCS11 (Totally, 12 MCS index)
	} else if (mode == 2) {
		fw_rate_idx = RATE_VHT1SS_MCS0 + (ss - 1) * 10; // MAX VHT MCS is MCS9 (Totally, 10 MCS index)
	} else if (mode == 1) { // HT
		fw_rate_idx = RATE_HT_MCS0 + (ss - 1) * 8;
	} else {
		fw_rate_idx = 0;
	}

	return fw_rate_idx;
}

void halbb_rate_idx_parsor(struct bb_info *bb, u16 rate_idx, enum rtw_gi_ltf gi_ltf, struct bb_rate_info *ra_i)
{
	ra_i->rate_idx_all = rate_idx | (((u16)gi_ltf & 0xf) << 12);
	ra_i->rate_idx = rate_idx;
	ra_i->gi_ltf = gi_ltf;
	ra_i->mode = (enum bb_mode_type)((rate_idx & 0x180) >> 7);

	if (ra_i->mode == BB_LEGACY_MODE) {
		ra_i->ss = 1;
		ra_i->idx = rate_idx & 0x1f;
	} else if (ra_i->mode == BB_HT_MODE) {
		ra_i->ss = (ra_i->idx >> 3) + 1;
		ra_i->idx = rate_idx & 0x1f;
	} else {
		ra_i->ss = ((rate_idx & 0x70) >> 4) + 1;
		ra_i->idx = rate_idx & 0xf;
	}

	/* Transfer to fw used rate_idx*/
	if (ra_i->mode == BB_LEGACY_MODE) {
		ra_i->fw_rate_idx = ra_i->idx;
		return;
	}

	ra_i->fw_rate_idx = halbb_mcs_ss_to_fw_rate_idx(ra_i->mode, ra_i->idx, ra_i->ss);

	return;
}

u8 halbb_rate_2_rate_digit(struct bb_info *bb, u16 rate)
{
	u8 legacy_table[12] = {1, 2, 5, 11, 6, 9, 12, 18, 24, 36, 48, 54};
	u8 rate_digit = 0;

	if (rate >= BB_HE_8SS_MCS0)
		rate_digit = (rate - BB_HE_8SS_MCS0);
	else if (rate >= BB_HE_7SS_MCS0)
		rate_digit = (rate - BB_HE_7SS_MCS0);
	else if (rate >= BB_HE_6SS_MCS0)
		rate_digit = (rate - BB_HE_6SS_MCS0);
	else if (rate >= BB_HE_5SS_MCS0)
		rate_digit = (rate - BB_HE_5SS_MCS0);
	else if (rate >= BB_HE_4SS_MCS0)
		rate_digit = (rate - BB_HE_4SS_MCS0);
	else if (rate >= BB_HE_3SS_MCS0)
		rate_digit = (rate - BB_HE_3SS_MCS0);
	else if (rate >= BB_HE_2SS_MCS0)
		rate_digit = (rate - BB_HE_2SS_MCS0);
	else if (rate >= BB_HE_1SS_MCS0)
		rate_digit = (rate - BB_HE_1SS_MCS0);
	else if (rate >= BB_VHT_8SS_MCS0)
		rate_digit = (rate - BB_VHT_8SS_MCS0);
	else if (rate >= BB_VHT_7SS_MCS0)
		rate_digit = (rate - BB_VHT_7SS_MCS0);
	else if (rate >= BB_VHT_6SS_MCS0)
		rate_digit = (rate - BB_VHT_6SS_MCS0);
	else if (rate >= BB_VHT_5SS_MCS0)
		rate_digit = (rate - BB_VHT_5SS_MCS0);
	else if (rate >= BB_VHT_4SS_MCS0)
		rate_digit = (rate - BB_VHT_4SS_MCS0);
	else if (rate >= BB_VHT_3SS_MCS0)
		rate_digit = (rate - BB_VHT_3SS_MCS0);
	else if (rate >= BB_VHT_2SS_MCS0)
		rate_digit = (rate - BB_VHT_2SS_MCS0);
	else if (rate >= BB_VHT_1SS_MCS0)
		rate_digit = (rate - BB_VHT_1SS_MCS0);
	else if (rate >= BB_HT_MCS0)
		rate_digit = (rate - BB_HT_MCS0);
	else if (rate <= BB_54M)
		rate_digit = legacy_table[rate];

	return rate_digit;
}

/*u8 halbb_get_rx_stream_num(struct bb_info *bb, enum rf_type type)
{
	u8 rx_num = 1;

	if (type == RF_1T1R)
		rx_num = 1;
	else if (type == RF_2T2R || type == RF_1T2R)
		rx_num = 2;
	else if (type == RF_3T3R || type == RF_2T3R)
		rx_num = 3;
	else if (type == RF_4T4R || type == RF_3T4R || type == RF_2T4R)
		rx_num = 4;
	else
		BB_WARNING("%s\n", __func__);

	return rx_num;
}*/

u8 halbb_rate_type_2_num_ss(struct bb_info *bb, enum halbb_rate_type type)
{
	u8 num_ss = 1;

	switch (type) {
	case BB_CCK:
	case BB_OFDM:
	case BB_1SS:
		num_ss = 1;
		break;
	case BB_2SS:
		num_ss = 2;
		break;
	case BB_3SS:
		num_ss = 3;
		break;
	case BB_4SS:
		num_ss = 4;
		break;
	default:
		break;
	}

	return num_ss;
}

u8 halbb_rate_to_num_ss(struct bb_info *bb, u16 rate)
{
	u8 num_ss = 1;

	if (rate <= BB_54M)
		num_ss = 1;
	else if (rate <= BB_HT_MCS(31))
		num_ss = ((rate - BB_HT_MCS0) >> 3) + 1;
	else if (rate <= BB_VHT_1SS_MCS(9))
		num_ss = 1;
	else if (rate <= BB_VHT_2SS_MCS(9))
		num_ss = 2;
	else if (rate <= BB_VHT_3SS_MCS(9))
		num_ss = 3;
	else if (rate <= BB_VHT_4SS_MCS(9))
		num_ss = 4;
	else if (rate <= BB_VHT_5SS_MCS(9))
		num_ss = 5;
	else if (rate <= BB_VHT_6SS_MCS(9))
		num_ss = 6;
	else if (rate <= BB_VHT_7SS_MCS(9))
		num_ss = 7;
	else if (rate <= BB_VHT_8SS_MCS(9))
		num_ss = 8;
	else if (rate <= BB_HE_1SS_MCS(11))
		num_ss = 1;
	else if (rate <= BB_HE_2SS_MCS(11))
		num_ss = 2;
	else if (rate <= BB_HE_3SS_MCS(11))
		num_ss = 3;
	else if (rate <= BB_HE_4SS_MCS(11))
		num_ss = 4;
	else if (rate <= BB_HE_5SS_MCS(11))
		num_ss = 5;
	else if (rate <= BB_HE_6SS_MCS(11))
		num_ss = 6;
	else if (rate <= BB_HE_7SS_MCS(11))
		num_ss = 7;
	else if (rate <= BB_HE_8SS_MCS(11))
		num_ss = 8;

	return num_ss;
}

void halbb_print_rate_2_buff(struct bb_info *bb, u16 rate_idx, enum rtw_gi_ltf gi_ltf, char *buf, u16 buf_size)
{
	struct bb_rate_info rate;
	char *ss = NULL;
	char *mode = NULL;
	char *gi = NULL;

	halbb_rate_idx_parsor(bb, rate_idx, gi_ltf, &rate);

	if (rate.mode == BB_HE_MODE)
		mode = "HE ";
	else if (rate.mode == BB_VHT_MODE)
		mode = "VHT ";
	else if (rate.mode == BB_HT_MODE)
		mode = "HT";
	else
		mode = "";

	if (rate.ss == 4)
		ss = "4";
	else if (rate.ss == 3)
		ss = "3";
	else if (rate.ss == 2)
		ss = "2";
	else
		ss = "1";

	if (rate.mode == BB_HE_MODE) {
		if (rate.gi_ltf == RTW_GILTF_LGI_4XHE32)
			gi = "[4X32]";
		else if (rate.gi_ltf == RTW_GILTF_SGI_4XHE08)
			gi = "[4X08]";
		else if (rate.gi_ltf == RTW_GILTF_2XHE16)
			gi = "[2X16]";
		else if (rate.gi_ltf == RTW_GILTF_2XHE08)
			gi = "[2X08]";
		else if (rate.gi_ltf == RTW_GILTF_1XHE16)
			gi = "[1X16]";
		else
			gi = "[1X08]";
	} else if (rate.mode >= BB_HT_MODE) {
		if (rate.gi_ltf == RTW_GILTF_SGI_4XHE08)
			gi = "[sgi]";
		else
			gi = "";
	} else {
		gi = "";
	}

	/* BB_SNPRINTF wait driver porting */
	_os_snprintf(buf, buf_size, "(%s%s%s%s%d%s%s)",
		     mode,
		     (rate.mode >= BB_VHT_MODE) ? ss : "",
		     (rate.mode >= BB_VHT_MODE) ? "-ss " : "",
		     (rate.rate_idx >= BB_HT_MCS0) ? "MCS" : "",
		     (rate.rate_idx >= BB_HT_MCS0) ? rate.idx : bb_phy_rate_table[rate.idx],
		     gi,
		     (rate.rate_idx < BB_HT_MCS0) ? "M" : "");
}

enum bb_qam_type halbb_get_qam_order(struct bb_info *bb, u16 rate_idx)
{
	u16 tmp_idx = rate_idx;
	enum bb_qam_type qam_order = BB_QAM_BPSK;
	enum bb_qam_type qam[10] = {BB_QAM_BPSK, BB_QAM_QPSK,
				    BB_QAM_QPSK, BB_QAM_16QAM,
				    BB_QAM_16QAM, BB_QAM_64QAM,
				    BB_QAM_64QAM, BB_QAM_64QAM,
				    BB_QAM_256QAM, BB_QAM_256QAM};

	if (rate_idx <= BB_11M)
		return BB_QAM_CCK;

	if ((rate_idx >= BB_VHT_MCS(1, 0)) && (rate_idx <= BB_VHT_MCS(4, 9))) {
		if (rate_idx >= BB_VHT_MCS(4, 0))
			tmp_idx -= BB_VHT_MCS(4, 0);
		else if (rate_idx >= BB_VHT_MCS(3, 0))
			tmp_idx -= BB_VHT_MCS(3, 0);
		else if (rate_idx >= BB_VHT_MCS(2, 0))
			tmp_idx -= BB_VHT_MCS(2, 0);
		else
			tmp_idx -= BB_VHT_MCS(1, 0);

		qam_order = qam[tmp_idx];
	} else if ((rate_idx >= BB_HT_MCS(0)) && (rate_idx <= BB_HT_MCS(31))) {
		if (rate_idx >= BB_HT_MCS(24))
			tmp_idx -= BB_HT_MCS(24);
		else if (rate_idx >= BB_HT_MCS(16))
			tmp_idx -= BB_HT_MCS(16);
		else if (rate_idx >= BB_HT_MCS(8))
			tmp_idx -= BB_HT_MCS(8);
		else
			tmp_idx -= BB_HT_MCS(0);

		qam_order = qam[tmp_idx];
	} else {
		if ((rate_idx > BB_06M) && (rate_idx <= BB_54M)) {
			tmp_idx -= BB_06M;
			qam_order = qam[tmp_idx - 1];
		} else { /* OFDM 6M & all other undefine rate*/
			qam_order = BB_QAM_BPSK;
		}
	}
	return qam_order;
}

u8 halbb_rate_order_compute(struct bb_info *bb, u16 rate_idx)
{
	u16 rate_order = rate_idx & 0x7f;

	rate_idx &= 0x7f;

	if (rate_idx >= BB_VHT_MCS(4, 0))
		rate_order -= BB_VHT_MCS(4, 0);
	else if (rate_idx >= BB_VHT_MCS(3, 0))
		rate_order -= BB_VHT_MCS(3, 0);
	else if (rate_idx >= BB_VHT_MCS(2, 0))
		rate_order -= BB_VHT_MCS(2, 0);
	else if (rate_idx >= BB_VHT_MCS(1, 0))
		rate_order -= BB_VHT_MCS(1, 0);
	else if (rate_idx >= BB_HT_MCS(24))
		rate_order -= BB_HT_MCS(24);
	else if (rate_idx >= BB_HT_MCS(16))
		rate_order -= BB_HT_MCS(16);
	else if (rate_idx >= BB_HT_MCS(8))
		rate_order -= BB_HT_MCS(8);
	else if (rate_idx >= BB_HT_MCS(0))
		rate_order -= BB_HT_MCS(0);
	else if (rate_idx >= BB_06M)
		rate_order -= BB_06M;
	else
		rate_order -= BB_01M;

	if (rate_idx >= BB_HT_MCS(0))
		rate_order++;

	return (u8)rate_order;
}

u8 halbb_init_ra_by_rssi(struct bb_info *bb, u8 rssi_assoc)
{
	u8 init_ra_lv = 0;
	BB_DBG(bb, DBG_RA, "====>%s\n", __func__);
	if (rssi_assoc > 50)
		init_ra_lv = 1;
	else if (rssi_assoc > 30)
		init_ra_lv = 2;
	else if (rssi_assoc > 1)
		init_ra_lv = 3;
	else
		init_ra_lv = 0;
	return init_ra_lv;
}

bool halbb_set_csi_rate(struct bb_info *bb, struct rtw_phl_stainfo_t *phl_sta_i)
{
	u8 macid;
	struct bb_h2c_ra_cfg_info *ra_cfg;
	struct rtw_hal_stainfo_t *hal_sta_i;
	struct rtw_ra_sta_info * ra_sta_i;

	if (!phl_sta_i || !bb)
		return false;
	macid = (u8)(phl_sta_i->macid);
	hal_sta_i = phl_sta_i->hal_sta;
	if (!hal_sta_i)
		return false;
	ra_cfg = &bb->bb_ra_i[macid].ra_cfg;
	ra_sta_i = &(hal_sta_i->ra_info);
	if (!ra_sta_i)
		return false;
	/*if ((!ra_sta_i->ra_csi_rate_en) && (!ra_sta_i->fixed_csi_rate_en))
		return false;*/
	/* Set csi rate ctrl enable */
	ra_cfg->ramask[7] |= BIT(7);
	ra_cfg->ra_csi_rate_en = ra_sta_i->ra_csi_rate_en;
	ra_cfg->fixed_csi_rate_en = ra_sta_i->fixed_csi_rate_en;
	ra_cfg->cr_tbl_sel = bb->hal_com->csi_para_ctrl_sel;
	ra_cfg->band_num = ra_sta_i->band_num;
	ra_cfg->fixed_csi_rate_l = ra_sta_i->csi_rate.mcs_ss_idx;
	ra_cfg->fixed_csi_rate_m = ra_sta_i->csi_rate.mode |
				   ra_sta_i->csi_rate.gi_ltf << 2 |
				   ra_sta_i->csi_rate.bw << 5;
	return true;
}

u8 halbb_rssi_lv_dec(struct bb_info *bb, u8 rssi, u8 ratr_state)
{
	/*@MCS0 ~ MCS4 , VHT1SS MCS0 ~ MCS4 , G 6M~24M*/
	/*u8 rssi_lv_t[RA_FLOOR_TABLE_SIZE] = {20, 34, 38, 42, 46, 50, 100};*/
	u8 rssi_lv_t[RA_FLOOR_TABLE_SIZE] = {30, 44, 48, 52, 56, 60, 100};
	/*@ RSSI definition changed in AX*/
	u8 new_rssi_lv = 0;
	u8 i;

	BB_DBG(bb, DBG_RA,
	       "curr RA level=(%d), Table_ori=[%d, %d, %d, %d, %d, %d]\n",
	       ratr_state, rssi_lv_t[0], rssi_lv_t[1], rssi_lv_t[2],
	       rssi_lv_t[3], rssi_lv_t[4], rssi_lv_t[5]);
	for (i = 0; i < RA_FLOOR_TABLE_SIZE; i++) {
		if (i >= (ratr_state))
			rssi_lv_t[i] += RA_FLOOR_UP_GAP;
	}
	BB_DBG(bb, DBG_RA,
	       "RSSI=(%d), Table_mod=[%d, %d, %d, %d, %d, %d]\n", rssi,
	       rssi_lv_t[0], rssi_lv_t[1], rssi_lv_t[2], rssi_lv_t[3],
	       rssi_lv_t[4], rssi_lv_t[5]);
	for (i = 0; i < RA_FLOOR_TABLE_SIZE; i++) {
		if (rssi < rssi_lv_t[i]) {
			new_rssi_lv = i;
			break;
		}
	}
	return new_rssi_lv;
}

u64 halbb_ramask_by_rssi(struct bb_info *bb, u8 rssi_lv, u64 ramask)
{
	u64 ra_mask_bitmap = ramask;

	if (rssi_lv == 0)
		ra_mask_bitmap &= 0xffffffffffffffff;
	else if (rssi_lv == 1)
		ra_mask_bitmap &= 0xfffffffffffffff0;
	else if (rssi_lv == 2)
		ra_mask_bitmap &= 0xffffffffffffefe0;
	else if (rssi_lv == 3)
		ra_mask_bitmap &= 0xffffffffffffcfc0;
	else if (rssi_lv == 4)
		ra_mask_bitmap &= 0xffffffffffff8f80;
	else if (rssi_lv >= 5)
		ra_mask_bitmap &= 0xffffffffffff0f00;

	/*Avoid empty HT/VHT/HE ramask when HT/VHT/HE mode is enabled*/
	if ((ra_mask_bitmap >> 12) == 0x0) {
		ra_mask_bitmap |= (ramask & 0xfffffffffffff000);
		BB_DBG(bb, DBG_RA,
		       "Empty HT/VHT/HE ramask! Bypass HT/VHT/HE ramask_by_rssi\n");
	}

	/*Avoid empty legacy ramask after foolproof of HT/VHT/HE mode*/
	if (ra_mask_bitmap == 0x0) {
		ra_mask_bitmap |= (ramask & 0xfff);
		BB_DBG(bb, DBG_RA,
		       "Empty ramask! Bypass a/b/g ramask_by_rssi\n");
	}

	return ra_mask_bitmap;
}

u64 halbb_ramask_mod(struct bb_info *bb, u8 macid, u64 ramask, u8 rssi, u8 mode,
	u8 nss)
{
	struct bb_ra_info *bb_ra = &bb->bb_ra_i[macid];
	u64 mod_mask = ramask;
	u64 mod_mask_rssi = ramask;
	u8 new_rssi_lv = 0;
	u8 wifi_mode = RA_non_ht;

	if (mode == CCK_SUPPORT) { /* B mode */
		mod_mask &= RAMASK_B;
		wifi_mode = RA_CCK;
		BB_DBG(bb, DBG_RA, "RA mask B mode\n");
	} else if (mode == OFDM_SUPPORT) { /* AG mode */
		mod_mask &= RAMASK_AG;
		wifi_mode = RA_non_ht;
		BB_DBG(bb, DBG_RA, "RA mask A mode\n");
	} else if (mode == (CCK_SUPPORT|OFDM_SUPPORT)) {
		/* BG mode */
		mod_mask &= RAMASK_BG;
		wifi_mode = RA_non_ht;
		BB_DBG(bb, DBG_RA, "RA mask 2.4G BG mode\n");
	} else if (mode == (CCK_SUPPORT|OFDM_SUPPORT|HT_SUPPORT)) {
		/* 2G N mode */
		mod_mask &= RAMASK_HT_2G;
		wifi_mode = RA_HT;
		BB_DBG(bb, DBG_RA, "RA mask 2.4G HT mode\n");
	} else if (mode == (OFDM_SUPPORT|HT_SUPPORT)) {
		/* 5G N mode */
		mod_mask &= RAMASK_HT_5G;
		wifi_mode = RA_HT;
		BB_DBG(bb, DBG_RA, "RA mask 5G HT mode\n");
	} else if (mode == (CCK_SUPPORT|OFDM_SUPPORT|VHT_SUPPORT_TX)) {
		/* 2G AC mode */
		mod_mask &= RAMASK_VHT_2G;
		wifi_mode = RA_VHT;
		BB_DBG(bb, DBG_RA, "RA mask 2.4G VHT mode\n");
	} else if (mode == (OFDM_SUPPORT|VHT_SUPPORT_TX)) {
		/* 5G AC mode */
		mod_mask &= RAMASK_VHT_5G;
		wifi_mode = RA_VHT;
		BB_DBG(bb, DBG_RA, "RA mask 5G VHT mode\n");
	} else if (mode == (CCK_SUPPORT|OFDM_SUPPORT|HE_SUPPORT)) {
		/* 2G AX mode */
		mod_mask &= RAMASK_HE_2G;
		wifi_mode = RA_HE;
		BB_DBG(bb, DBG_RA, "RA mask 2.4G HE mode\n");
	} else if (mode == (OFDM_SUPPORT|HE_SUPPORT)) {
		/* 5G AX mode */
		mod_mask &= RAMASK_HE_5G;
		wifi_mode = RA_HE;
		BB_DBG(bb, DBG_RA, "RA mask 5G HE mode\n");
	} else {
		BB_WARNING("MD id %x, RA mask not found\n", mode);
	}
	BB_DBG(bb, DBG_RA, "RA mask SS NUM : %d\n", nss);
	if (wifi_mode == RA_HT) {
		switch (nss) {
		case RA_1SS_MODE:
			mod_mask &= RAMASK_1SS_HT;
			break;
		case RA_2SS_MODE:
			mod_mask &= RAMASK_2SS_HT;
			break;
		case RA_3SS_MODE:
			mod_mask &= RAMASK_3SS_HT;
			break;
		case RA_4SS_MODE:
			mod_mask &= RAMASK_4SS_HT;
			break;
		default:
			mod_mask &= RAMASK_1SS_HT;
			break;
		}
	} else if (wifi_mode == RA_VHT) {
		switch (nss) {
		case RA_1SS_MODE:
			mod_mask &= RAMASK_1SS_VHT;
			break;
		case RA_2SS_MODE:
			mod_mask &= RAMASK_2SS_VHT;
			break;
		case RA_3SS_MODE:
			mod_mask &= RAMASK_3SS_VHT;
			break;
		case RA_4SS_MODE:
			mod_mask &= RAMASK_4SS_VHT;
			break;
		default:
			mod_mask &= RAMASK_1SS_VHT;
			break;
		}
	} else if (wifi_mode == RA_HE) {
		switch (nss) {
		case RA_1SS_MODE:
			mod_mask &= RAMASK_1SS_HE;
			break;
		case RA_2SS_MODE:
			mod_mask &= RAMASK_2SS_HE;
			break;
		case RA_3SS_MODE:
			mod_mask &= RAMASK_3SS_HE;
			break;
		case RA_4SS_MODE:
			mod_mask &= RAMASK_4SS_HE;
			break;
		default:
			mod_mask &= RAMASK_1SS_HE;
			break;
		}
	} else {
		BB_DBG(bb, DBG_RA, "RA mask non-ht mode\n");
	}
	BB_DBG(bb, DBG_RA, "RA mask modify : %llx\n", mod_mask);
	new_rssi_lv = halbb_rssi_lv_dec(bb, rssi, bb_ra->rssi_lv);
	if (wifi_mode != RA_CCK) {
		mod_mask_rssi = halbb_ramask_by_rssi(bb, new_rssi_lv, mod_mask);
		//BB_DBG(bb, DBG_RA, "RA mask modify by rssi : 0x%016llx\n", mod_mask_rssi);
	} else {
		mod_mask_rssi = mod_mask;
	}
	return mod_mask_rssi;
}

void rtw_halbb_mudbg(struct bb_info *bb, u8 type, u8 mu_entry, u8 macid, 
			   bool en_256q, bool en_1024q)
{
	struct bb_h2c_mu_cfg mucfg = {0};
	u32 *bb_h2c = (u32 *)&mucfg;
	u8 cmdlen = sizeof(mucfg);

	mucfg.cmd_type = type;
	mucfg.entry = mu_entry;
	mucfg.macid = macid;
	halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_MUCFG, HALBB_H2C_RA, bb_h2c);
}

u64 halbb_gen_abg_mask(struct bb_info *bb, struct rtw_phl_stainfo_t *phl_sta_i)
{
	u64 tmp_mask = 0;
	u64 abg_mask = 0;
	u8 i;
	struct protocol_cap_t *asoc_cap_i;

	if (!phl_sta_i)
		return 0;

	asoc_cap_i = &phl_sta_i->asoc_cap;

	BB_DBG(bb, DBG_RA,
	       "supported rates(L->H) = [%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x]\n",
	       asoc_cap_i->supported_rates[0], asoc_cap_i->supported_rates[1],
	       asoc_cap_i->supported_rates[2], asoc_cap_i->supported_rates[3],
	       asoc_cap_i->supported_rates[4], asoc_cap_i->supported_rates[5],
	       asoc_cap_i->supported_rates[6], asoc_cap_i->supported_rates[7],
	       asoc_cap_i->supported_rates[8], asoc_cap_i->supported_rates[9],
	       asoc_cap_i->supported_rates[10], asoc_cap_i->supported_rates[11]);
	for (i = 0; i < MAX_ABG_RATE_NUM; i++) {
		if (asoc_cap_i->supported_rates[i] == 0x0)
			continue;

		tmp_mask = (u64)halbb_mgnt_2_hw_rate(asoc_cap_i->supported_rates[i]);
		abg_mask |= (u64)BIT(tmp_mask);
	}
	BB_DBG(bb, DBG_RA, "gen_abgmask: 0x%llx\n", abg_mask);

	return abg_mask;
}

u64 halbb_gen_vht_mask(struct bb_info *bb,
				struct rtw_phl_stainfo_t *phl_sta_i)
{
	u8 vht_cap[2] = {0};
	u8 tmp_cap = 0;
	u8 cap_ss;
	u64 tmp_mask_nss = 0;
	u8 i;
	struct protocol_cap_t *asoc_cap_i;

	/*@Becareful RA use our "Tx" capability which means the capability of their "Rx"*/
	if (!phl_sta_i)
		return 0;
	asoc_cap_i = &phl_sta_i->asoc_cap;
	vht_cap[0] = asoc_cap_i->vht_rx_mcs[0];
	vht_cap[1] = asoc_cap_i->vht_rx_mcs[1];
	BB_DBG(bb, DBG_RA, "%s : vhtcap:%x %x\n", __func__, vht_cap[0], vht_cap[1]);
	for (i = 0; i < MAX_NSS_VHT; i++) {
		if (i == 0)
			tmp_cap = vht_cap[0];
		else if (i == 4)
			tmp_cap = vht_cap[1];

		cap_ss = tmp_cap & 0x03;
		tmp_cap = tmp_cap >> 2;
		if (cap_ss == 0)
			tmp_mask_nss |= ((u64)0xff << (i * 12));
		else if (cap_ss == 1)
			tmp_mask_nss |= ((u64)0x1ff << (i * 12));
		else if (cap_ss == 2)
			tmp_mask_nss |= ((u64)0x3ff << (i * 12));

		BB_DBG(bb, DBG_RA, "gen_vhtmask:cap%x, ss%x, hemask: 0x%llx\n",
		       cap_ss, i, tmp_mask_nss);
	}
	return tmp_mask_nss;
}

u64 halbb_gen_ht_mask(struct bb_info *bb,
				struct rtw_phl_stainfo_t *phl_sta_i)
{
	u8 ht_cap[4] = {0};
	u64 cap_ss;
	u64 tmp_mask_nss = 0;
	u8 i;
	struct protocol_cap_t *asoc_cap_i;

	/*@Becareful RA use our "Tx" capability which means the capability of their "Rx"*/
	if (!phl_sta_i)
		return 0;
	asoc_cap_i = &phl_sta_i->asoc_cap;
	for (i = 0; i < MAX_NSS_HT; i++)/* can use pointer after merge code*/
		ht_cap[i] = asoc_cap_i->ht_rx_mcs[i];
	BB_DBG(bb, DBG_RA, "%s : htcap: %x %x\n", __func__, ht_cap[0], ht_cap[1]);
	for (i = 0; i < MAX_NSS_HT; i++) {
		cap_ss = (u64)ht_cap[i];
		tmp_mask_nss = tmp_mask_nss | (cap_ss << (i * 12));
		BB_DBG(bb, DBG_RA, "gen_htmask:cap%llx, ss%x, htmask: 0x%llx\n", cap_ss, i, tmp_mask_nss);
	}

	return tmp_mask_nss;
}

u64 halbb_gen_he_mask(struct bb_info *bb,
				struct rtw_phl_stainfo_t *phl_sta_i, enum channel_width bw)
{
	u8 he_cap[2] = {0};
	u8 tmp_cap = 0;
	u8 cap_ss;
	u64 tmp_mask_nss = 0;
	u8 i;
	struct protocol_cap_t *asoc_cap_i;
	/*@Becareful RA use our "Tx" capability which means the capability of their "Rx"*/
	/*@In HE cap, mcs is correspond to channel bw"*/

	if (!phl_sta_i)
		return 0;
	asoc_cap_i = &phl_sta_i->asoc_cap;
	if (bw == CHANNEL_WIDTH_80_80) {
		he_cap[0] = asoc_cap_i->he_rx_mcs[4];
		he_cap[1] = asoc_cap_i->he_rx_mcs[5];
	} else if (bw == CHANNEL_WIDTH_160) {
		he_cap[0] = asoc_cap_i->he_rx_mcs[2];
		he_cap[1] = asoc_cap_i->he_rx_mcs[3];
	} else {
		he_cap[0] = asoc_cap_i->he_rx_mcs[0];
		he_cap[1] = asoc_cap_i->he_rx_mcs[1];
	}
	BB_DBG(bb, DBG_RA, "%s: hecap:%x %x\n", __func__, he_cap[0], he_cap[1]);
	for (i = 0; i < MAX_NSS_HE; i++) {
		if (i == 0)
			tmp_cap = he_cap[0];
		else if (i == 4)
			tmp_cap = he_cap[1];
		cap_ss = tmp_cap & 0x03;
		tmp_cap = tmp_cap >> 2;
		if (cap_ss == 0)
			tmp_mask_nss |= ((u64)0xff << (i * 12));
		else if (cap_ss == 1)
			tmp_mask_nss |= ((u64)0x3ff << (i * 12));
		else if (cap_ss == 2)
			tmp_mask_nss |= ((u64)0xfff << (i * 12));

		BB_DBG(bb, DBG_RA, "gen_hemask:cap%x, ss%x, hemask: 0x%llx\n",
		       cap_ss, i, tmp_mask_nss);
	}
	return tmp_mask_nss;
}

bool halbb_chk_bw_under_20(struct bb_info *bb, u8 bw)
{
	bool ret_val = true;

	switch (bw) {
	case CHANNEL_WIDTH_5:
	case CHANNEL_WIDTH_10:
	case CHANNEL_WIDTH_20:
		ret_val = true;
		break;
	case CHANNEL_WIDTH_40:
	case CHANNEL_WIDTH_80:
	case CHANNEL_WIDTH_80_80:
	case CHANNEL_WIDTH_160:
		ret_val = false;
		break;
	default:
		ret_val = false;
		break;
	}
	return ret_val;
}

bool halbb_chk_bw_under_40(struct bb_info *bb, u8 bw)
{
	bool ret_val = true;

	switch (bw) {
	case CHANNEL_WIDTH_5:
	case CHANNEL_WIDTH_10:
	case CHANNEL_WIDTH_20:
	case CHANNEL_WIDTH_40:
		ret_val = true;
		break;
	case CHANNEL_WIDTH_80:
	case CHANNEL_WIDTH_80_80:
	case CHANNEL_WIDTH_160:
		ret_val = false;
		break;
	default:
		ret_val = false;
		break;
	}
	return ret_val;
}

bool halbb_hw_bw_mode_chk(struct bb_info *bb, u8 bw, u8 hw_mode)
{
	bool ret_val = true;

	switch (hw_mode) {
	case CCK_SUPPORT:
	case OFDM_SUPPORT:
	case (CCK_SUPPORT | OFDM_SUPPORT):
		ret_val = halbb_chk_bw_under_20(bb, bw);
		break;
	case HT_SUPPORT:
	case (HT_SUPPORT | CCK_SUPPORT):
	case (HT_SUPPORT | OFDM_SUPPORT):
	case (HT_SUPPORT | OFDM_SUPPORT | CCK_SUPPORT):
		ret_val = halbb_chk_bw_under_40(bb, bw);
		break;
	case VHT_SUPPORT_TX:
	case (VHT_SUPPORT_TX | CCK_SUPPORT):
	case (VHT_SUPPORT_TX | OFDM_SUPPORT):
	case (VHT_SUPPORT_TX | OFDM_SUPPORT | CCK_SUPPORT):
	case HE_SUPPORT:
	case (HE_SUPPORT | CCK_SUPPORT):
	case (HE_SUPPORT | OFDM_SUPPORT):
	case (HE_SUPPORT | OFDM_SUPPORT | CCK_SUPPORT):
		ret_val = true;
		break;
	default:
		ret_val = true;
		break;
	}
	if (!ret_val)
		BB_WARNING("WRONG BW setting !!!!\n");
	return ret_val;
}

u8 halbb_hw_bw_mapping(struct bb_info *bb, u8 bw, u8 hw_mode)
{
	u8 hw_bw_map = CHANNEL_WIDTH_20;
	bool ret_val;

	if (bw <= CHANNEL_WIDTH_80)
		hw_bw_map = bw;
	else if (bw == CHANNEL_WIDTH_160 || bw == CHANNEL_WIDTH_80_80)
		hw_bw_map = CHANNEL_WIDTH_160;
	else
		hw_bw_map = CHANNEL_WIDTH_20;
	ret_val = halbb_hw_bw_mode_chk(bb, bw, hw_mode);
	return hw_bw_map;
}

u8 halbb_hw_mode_mapping(struct bb_info *bb, u8 wifi_mode)
{
	u8 hw_mode_map = 0;
	/* Driver wifi mode mapping */

	if (wifi_mode & WLAN_MD_11B) /*11B*/
		hw_mode_map |= CCK_SUPPORT;
	if ((wifi_mode & WLAN_MD_11A) || (wifi_mode & WLAN_MD_11G)) /*11G, 11A*/
		hw_mode_map |= OFDM_SUPPORT;

	/* To prevent unnecessary mode from driver, causing confusing ra mask selection after then*/
	if (wifi_mode & WLAN_MD_11AX) /*11AX*/
		hw_mode_map |= HE_SUPPORT;
	else if (wifi_mode & WLAN_MD_11AC) /*11AC*/
		hw_mode_map |= VHT_SUPPORT_TX;
	else if (wifi_mode & WLAN_MD_11N) /* 11_N*/
		hw_mode_map |= HT_SUPPORT;

	if (hw_mode_map == 0)
		BB_WARNING("WRONG Wireless mode !!!!\n");
	return hw_mode_map;
}

bool halbb_ac_n_sgi_chk(struct bb_info *bb, u8 hw_mode, bool en_sgi)
{
	bool sgi_chk = en_sgi;

	/* Driver wifi mode mapping */
	if (hw_mode & HE_SUPPORT) {
		sgi_chk = false;
		BB_DBG(bb, DBG_RA, "HE mode sgi is not used!\n");
	}
	return sgi_chk;
}

u8 halbb_ax_giltf_chk(struct bb_info *bb, u8 hw_mode, u8 giltf_cap)
{
	u8 giltf_chk = giltf_cap;
	
	/* Driver wifi mode mapping */
	if (!(hw_mode & HE_SUPPORT)) {
		giltf_chk = 0;
		BB_DBG(bb, DBG_RA, "In non-HE mode gi_ltf is not used!\n");
	}
	return giltf_chk;
}

bool halbb_ldpc_chk(struct bb_info *bb,  struct rtw_phl_stainfo_t *phl_sta_i, u8 hw_mode)
{
	struct protocol_cap_t *asoc_cap_i;
	bool ldpc_en = false;

	/* Driver wifi mode mapping */
	if (!phl_sta_i)
		return false;
	asoc_cap_i = &phl_sta_i->asoc_cap;
	if (hw_mode & HE_SUPPORT) {
		ldpc_en |= asoc_cap_i->he_ldpc;
		BB_DBG(bb, DBG_RA, "Enable HE LDPC\n");
	} else if (hw_mode&VHT_SUPPORT_TX) {
		ldpc_en |= asoc_cap_i->vht_ldpc;
		BB_DBG(bb, DBG_RA, "Enable VHT LDPC\n");
	} else if  (hw_mode&HT_SUPPORT) {
		ldpc_en |= asoc_cap_i->ht_ldpc;
		BB_DBG(bb, DBG_RA, "Enable HT LDPC\n");
	}
	return ldpc_en;
}

u8 halbb_nss_mapping(struct bb_info *bb,  u8 nss)
{
	u8 mapping_nss = 0;

	if (nss != 0)
		mapping_nss = nss - 1;
	/* Driver tx_nss mapping */
	if (mapping_nss > (bb->hal_com->rfpath_tx_num - 1))
		mapping_nss = bb->hal_com->rfpath_tx_num - 1;
	return mapping_nss;
}

bool halbb_stbc_mapping(struct bb_info *bb,  struct rtw_phl_stainfo_t *phl_sta_i, u8 hw_mode)
{
	struct protocol_cap_t *asoc_cap_i;
	bool stbc_en = false;

	/* Driver wifi mode mapping */
	if (!phl_sta_i)
		return false;
	asoc_cap_i = &phl_sta_i->asoc_cap;
	if (hw_mode & HE_SUPPORT) {
		if (asoc_cap_i->stbc_he_rx != 0)
			stbc_en = true;
		BB_DBG(bb, DBG_RA, "HE STBC %d\n", stbc_en);
	} else if (hw_mode & VHT_SUPPORT_TX) {
		if (asoc_cap_i->stbc_vht_rx != 0)
			stbc_en = true;
		BB_DBG(bb, DBG_RA, "VHT STBC %d\n", stbc_en);
	} else if  (hw_mode & HT_SUPPORT) {
		if (asoc_cap_i->stbc_ht_rx != 0)
			stbc_en = true;
		BB_DBG(bb, DBG_RA, "HT STBC %d\n", stbc_en);
	}
	return stbc_en;
}

bool halbb_sgi_chk(struct bb_info *bb,  struct rtw_phl_stainfo_t *phl_sta_i, u8 hw_bw)
{
	struct protocol_cap_t *asoc_cap_i;
	bool sgi_en = false;

	/* Driver wifi mode mapping */
	if (!phl_sta_i)
		return false;
	asoc_cap_i = &phl_sta_i->asoc_cap;
	if (hw_bw == CHANNEL_WIDTH_20) {
		sgi_en = asoc_cap_i->sgi_20;
		BB_DBG(bb, DBG_RA, "Enable 20M SGI\n");
	} else if (hw_bw == CHANNEL_WIDTH_40) {
		sgi_en = asoc_cap_i->sgi_40;
		BB_DBG(bb, DBG_RA, "Enable 40M SGI\n");
	} else if  (hw_bw == CHANNEL_WIDTH_80) {
		sgi_en = asoc_cap_i->sgi_80;
		BB_DBG(bb, DBG_RA, "Enable 80M SGI\n");
	} else if  (hw_bw == CHANNEL_WIDTH_160) {
		sgi_en = asoc_cap_i->sgi_160;
		BB_DBG(bb, DBG_RA, "Enable 160M SGI\n");
	}
	return sgi_en;
}

void halbb_ramask_trans(struct bb_info *bb, u8 macid, u64 mask)
{
	struct bb_h2c_ra_cfg_info *ra_cfg = &bb->bb_ra_i[macid].ra_cfg;

	ra_cfg->ramask[0] = (u8)(mask & 0x00000000000000ff);
	ra_cfg->ramask[1] = (u8)((mask & 0x000000000000ff00)>>8);
	ra_cfg->ramask[2] = (u8)((mask & 0x0000000000ff0000)>>16);
	ra_cfg->ramask[3] = (u8)((mask & 0x00000000ff000000)>>24);
	ra_cfg->ramask[4] = (u8)((mask & 0x000000ff00000000)>>32);
	ra_cfg->ramask[5] = (u8)((mask & 0x0000ff0000000000)>>40);
	ra_cfg->ramask[6] = (u8)((mask & 0x00ff000000000000)>>48);
	ra_cfg->ramask[7] = (u8)((mask & 0xff00000000000000)>>56);
}

u8 halbb_get_opt_giltf(struct bb_info *bb, u8 assoc_giltf)
{
	u8 i =0;
	u8 opt_gi_ltf = 0;

	if (assoc_giltf & BIT(1)) /* cap. for 4x0.8*/
		opt_gi_ltf |= BIT(BB_OPT_GILTF_4XHE08);
	else if (assoc_giltf & BIT(5)) /* cap. for 1x0.8*/
		opt_gi_ltf |= BIT(BB_OPT_GILTF_1XHE08);

	BB_DBG(bb, DBG_RA, "Ass GILTF=%x,opt GILTF=%x\n", assoc_giltf, opt_gi_ltf);
	return opt_gi_ltf;
}

u8 halbb_giltf_trans(struct bb_info *bb, u8 assoc_giltf, u8 cal_giltf)
{
	u8 i =0;

	BB_DBG(bb, DBG_RA, "Ass GILTF=%x,Cal GILTF=%x\n", assoc_giltf, cal_giltf);
	if (cal_giltf == RTW_GILTF_LGI_4XHE32 && (assoc_giltf & BIT(0)))
		return cal_giltf;
	else if (cal_giltf == RTW_GILTF_SGI_4XHE08 && (assoc_giltf & BIT(1)))
		return cal_giltf;
	else if (cal_giltf == RTW_GILTF_2XHE16 && (assoc_giltf & BIT(2)))
		return cal_giltf;
	else if (cal_giltf == RTW_GILTF_2XHE08 && (assoc_giltf & BIT(3)))
		return cal_giltf;
	else if (cal_giltf == RTW_GILTF_1XHE16 && (assoc_giltf & BIT(4)))
		return cal_giltf;
	else if (cal_giltf == RTW_GILTF_1XHE08 && (assoc_giltf & BIT(5)))
		return cal_giltf;
	if (assoc_giltf & BIT(3))
		return RTW_GILTF_2XHE08;
	else if (assoc_giltf & BIT(2))
		return RTW_GILTF_2XHE16;
	else if (assoc_giltf & BIT(1))
		return RTW_GILTF_SGI_4XHE08;
	else if (assoc_giltf & BIT(5))
		return RTW_GILTF_1XHE08;
	else if (assoc_giltf & BIT(4))
		return RTW_GILTF_1XHE16;
	else
		return RTW_GILTF_LGI_4XHE32;
}

bool rtw_halbb_dft_mask(struct bb_info *bb,
				struct rtw_phl_stainfo_t *phl_sta_i)
{
	u8 mode = 0; /* connect to phl->assoc*/
	u8 hw_md;
	u64 init_mask = 0;
	u64 get_mask = 0;
	u32 mask0, mask1;
	enum channel_width bw;
	struct rtw_hal_stainfo_t *hal_sta_i;
	struct protocol_cap_t *asoc_cap_i;

	if (!phl_sta_i || !bb) {
		BB_WARNING("Error RA registered !!! Pointer is NULL!!!\n");
		return false;
	}
	hal_sta_i = phl_sta_i->hal_sta;
	if (!hal_sta_i) {
		BB_WARNING("Error RA registered !!! Pointer is NULL!!!\n");
		return false;
	}
	asoc_cap_i = &phl_sta_i->asoc_cap;
	mode = phl_sta_i->wmode;
	bw = phl_sta_i->chandef.bw;
	hw_md = halbb_hw_mode_mapping(bb, mode);
	BB_DBG(bb, DBG_RA, "Gen Dftmask: mode = %x, hw_md = %x\n", mode, hw_md);

	if (hw_md & (CCK_SUPPORT | OFDM_SUPPORT))
		init_mask = halbb_gen_abg_mask(bb, phl_sta_i);

	if (init_mask == 0) {
		if (hw_md & CCK_SUPPORT) {
			init_mask |= 0x0000000f;
			BB_DBG(bb, DBG_RA,
			       "[%s]abg mask is null!, set b mask=0xf\n", __func__);
		}

		if (hw_md & OFDM_SUPPORT) {
			init_mask |= 0x00000ff0;
			BB_DBG(bb, DBG_RA,
			       "[%s]abg mask is null!, set ag mask=0xff\n", __func__);
		}
	}

	if (hw_md & HE_SUPPORT)
		get_mask = halbb_gen_he_mask(bb, phl_sta_i, bw);
	else if (hw_md & VHT_SUPPORT_TX)
		get_mask = halbb_gen_vht_mask(bb, phl_sta_i);
	else if (hw_md & HT_SUPPORT)
		get_mask = halbb_gen_ht_mask(bb, phl_sta_i);
	else
		get_mask = 0;
	init_mask |= (get_mask << 12);
	mask0 = (u32)(init_mask & 0xffffffff);
	mask1 = (u32)((init_mask >> 32) & 0xffffffff);
	if (init_mask != 0) {
		hal_sta_i->ra_info.cur_ra_mask = init_mask;
		BB_DBG(bb, DBG_RA, "Default mask = %x %x\n", mask0, mask1);
		return true;
	} else {
		BB_WARNING("Error default mask, it should not zero\n");
		return false;
	}
}

u8 rtw_halbb_arfr_trans(struct bb_info *bb,
	struct rtw_phl_stainfo_t *phl_sta_i)
{
	struct rtw_hal_stainfo_t *hal_sta_i;
	u8 mode;
	u8 arfr_ret = 0x0;

	if (!phl_sta_i)
		return false;
	hal_sta_i = phl_sta_i->hal_sta;
	if (!hal_sta_i)
		return false;
	mode = phl_sta_i->wmode;
	if (mode & WLAN_MD_11B) /*11B*/
		arfr_ret |= CCK_SUPPORT;
	if ((mode & WLAN_MD_11A)||(mode & WLAN_MD_11G)) /*11G, 11A*/
		arfr_ret |= OFDM_SUPPORT;
	if (mode & WLAN_MD_11N) /* 11_N*/
		arfr_ret |= HT_SUPPORT;
	if (mode & WLAN_MD_11AC) /*11AC*/
		arfr_ret |= VHT_SUPPORT_TX;
	return arfr_ret;
	/*if (mode|WLAN_MD_11AX ) // 11AX usually can use arfr
		hw_mode_map |= HE_SUPPORT;*/
}

bool rtw_halbb_raregistered(struct bb_info *bb, struct rtw_phl_stainfo_t *phl_sta_i)
{
	/*struct rtw_phl_stainfo_t *phl_sta_i =  bb->phl_sta_info[macid];*/
	u8 macid;
	struct bb_h2c_ra_cfg_info *ra_cfg;
	struct rtw_hal_stainfo_t *hal_sta_i;
	bool tx_ldpc;
	bool tx_stbc;
	bool ret_val = false;
	u8 tx_nss;
	u8 rssi;
	struct protocol_cap_t *asoc_cap_i;
	u8 rssi_assoc ;
	u8 mode;
	bool en_sgi = false;
	u8 giltf_cap = 0;
	u8 init_lv;
	/* Need to mapping with driver wifi mode*/
	u32 *bb_h2c;
	u8 cmdlen = sizeof(struct bb_h2c_ra_cfg_info);
	u64 mod_mask;

	BB_DBG(bb, DBG_RA, "====>%s\n", __func__);
	if (!phl_sta_i || !bb) {
		BB_WARNING("Error RA registered !!! Pointer is NULL!!!\n");
		return ret_val;
	}
	macid = (u8)(phl_sta_i->macid);
	ra_cfg = &bb->bb_ra_i[macid].ra_cfg;
	bb_h2c = (u32 *) ra_cfg;
	hal_sta_i = phl_sta_i->hal_sta;
	if (!hal_sta_i) {
		BB_WARNING("Error RA registered !!! Pointer is NULL!!!\n");
		return ret_val;
	}
	asoc_cap_i = &phl_sta_i->asoc_cap;
	/*@ use assoc rssi to init ra, only use in ra register, it is an integer using U(8,0)*/
	rssi_assoc = (u8)(hal_sta_i->rssi_stat.assoc_rssi);
	BB_DBG(bb, DBG_RA, "Assoc rssi = %d\n", rssi_assoc);
	rssi = (u8)(hal_sta_i->rssi_stat.rssi) >> 1;
	mode = phl_sta_i->wmode;
	init_lv = halbb_init_ra_by_rssi(bb, rssi_assoc);
	/*@Becareful RA use our "Tx" capability which means the capability of their "Rx"*/
	tx_nss = halbb_nss_mapping(bb, asoc_cap_i->nss_rx);
	if (asoc_cap_i->dcm_max_const_rx)
		ra_cfg->dcm_cap = 1;
	else
		ra_cfg->dcm_cap = 0;
	mode = halbb_hw_mode_mapping(bb, mode);

	/* ONLY need to get the optional gi-ltf combination for H2C FW*/
	/* bit(0)=4x0.8, bit(1)=1x0.8 -> different definition from the drver giltf*/
	giltf_cap = halbb_get_opt_giltf(bb, asoc_cap_i->ltf_gi);
	ra_cfg->giltf_cap = halbb_ax_giltf_chk(bb, mode, giltf_cap);

	/* giltf assigned by phl/halbb or trained by FW*/
	if (hal_sta_i->ra_info.fix_giltf_en == false) { /*giltf from halbb*/
		if (bb->ic_type & (BB_RTL8852A | BB_RTL8852B | BB_RTL8852C)) { /*Disable FW train GI_LTF*/
			ra_cfg->fix_giltf_en = true;
			ra_cfg->fix_giltf = RTW_GILTF_2XHE08;
			/* gi_ltf is decided by delay spread. This is not ready.
			if (is_giltf_decided_by_delay_sp)
				ra_cfg->fix_giltf = halbb_ra_giltf_ctrl(bb, macid, delay_sp, asoc_cap_i->ltf_gi);
			*/
		} else { /*Enable FW train GI_LTF*/
			ra_cfg->fix_giltf_en = false;
		}
	} else { /*giltf from phl*/
		ra_cfg->fix_giltf = halbb_giltf_trans(bb, asoc_cap_i->ltf_gi, hal_sta_i->ra_info.cal_giltf);
	}
	BB_DBG(bb, DBG_RA, "fix_giltf_en(phl)=%d, fix_giltf_en(Final)=%d, fix_giltf=%d\n",
	       hal_sta_i->ra_info.fix_giltf_en, ra_cfg->fix_giltf_en, ra_cfg->fix_giltf);

	ra_cfg->is_dis_ra = hal_sta_i->ra_info.dis_ra;
	mod_mask = hal_sta_i->ra_info.cur_ra_mask;
	ra_cfg->er_cap = asoc_cap_i->er_su;
	tx_stbc = halbb_stbc_mapping(bb, phl_sta_i, mode);
	ra_cfg->upd_all= true;
	ra_cfg->upd_bw_nss_mask= false;
	ra_cfg->upd_mask= false;
	if (mode == 0)
		return ret_val;
	tx_ldpc = halbb_ldpc_chk(bb, phl_sta_i, mode);
	ra_cfg->mode_ctrl = mode;
	ra_cfg->bw_cap = halbb_hw_bw_mapping(bb, phl_sta_i->chandef.bw, mode);
	en_sgi = halbb_sgi_chk(bb, phl_sta_i, ra_cfg->bw_cap);
	ra_cfg->macid = macid;
	ra_cfg->init_rate_lv = init_lv;
	ra_cfg->en_sgi = halbb_ac_n_sgi_chk(bb, mode, en_sgi);
	ra_cfg->ldpc_cap = tx_ldpc;
	ra_cfg->stbc_cap = tx_stbc;
	ra_cfg->ss_num = tx_nss;
	/*@ modify ra mask by assoc rssi*/
	mod_mask = halbb_ramask_mod(bb, macid, mod_mask, rssi_assoc, mode, tx_nss);
	halbb_ramask_trans(bb, macid, mod_mask);
	ret_val = halbb_set_csi_rate(bb, phl_sta_i);
	if (!ret_val)
		return ret_val;
	BB_DBG(bb, DBG_RA, "RA Register=>In: Dis_ra=%x, MD=%x, BW=%x, macid=%x\n",
		   hal_sta_i->ra_info.dis_ra, mode, phl_sta_i->chandef.bw, macid);
	BB_DBG(bb, DBG_RA, "RA Register=>In: DCM =%x, ER=%x, in_rt=%x, upd_a=%x, sgi=%x, ldpc=%x, stbc=%x\n",
		   ra_cfg->dcm_cap, ra_cfg->er_cap, init_lv, ra_cfg->upd_all,
		   en_sgi, tx_ldpc, tx_stbc);
	BB_DBG(bb, DBG_RA, "RA Register=>In: SS=%x, GILTF_cap=%x, upd_bnm=%x, upd_m=%x, mask=%llx\n",
		   tx_nss, giltf_cap, ra_cfg->upd_bw_nss_mask, ra_cfg->upd_mask, mod_mask);
	BB_DBG(bb, DBG_RA, "RA Register=>Out racfg: dis%x bw%x md%x mid%x\n", ra_cfg->is_dis_ra,
		   ra_cfg->bw_cap, ra_cfg->mode_ctrl, ra_cfg->macid);
	BB_DBG(bb, DBG_RA, "RA Register=>Out h2cp: %x %x %x %x\n", bb_h2c[0], bb_h2c[1],
	       bb_h2c[2], bb_h2c[3]);
	ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_MACIDCFG, HALBB_H2C_RA, bb_h2c);
	return ret_val;
}

bool rtw_halbb_ra_deregistered(struct bb_info *bb,
	struct rtw_phl_stainfo_t *phl_sta_i)
{
	return true;
}

bool rtw_halbb_raupdate(struct bb_info *bb,
	struct rtw_phl_stainfo_t *phl_sta_i)
{
	/* Update only change bw, nss, ramask */
	u8 macid;
	struct bb_h2c_ra_cfg_info *ra_cfg;
	struct rtw_hal_stainfo_t *hal_sta_i;
	struct protocol_cap_t *asoc_cap_i;
	u8 tx_nss;
	u8 rssi;
	u8 init_lv = 0;
	u8 mode = 0;
	u8 giltf_cap = 0;
	bool ret_val = false;
	/* Need to mapping with driver wifi mode*/
	u32 *bb_h2c;
	u8 cmdlen = sizeof(struct bb_h2c_ra_cfg_info);
	u64 mod_mask;

	BB_DBG(bb, DBG_RA, "====>%s\n", __func__);
	if (!phl_sta_i || !bb) {
		BB_WARNING("Error RA registered !!! Pointer is NULL!!!\n");
		return ret_val;
	}
	macid = (u8) (phl_sta_i->macid);
	hal_sta_i = phl_sta_i->hal_sta;
	if (!hal_sta_i) {
		BB_WARNING("Error RA registered !!! Pointer is NULL!!!\n");
		return ret_val;
	}
	ra_cfg = &bb->bb_ra_i[macid].ra_cfg;
	mode = ra_cfg->mode_ctrl;
	bb_h2c = (u32 *) ra_cfg;
	asoc_cap_i = &phl_sta_i->asoc_cap;
	rssi = hal_sta_i->rssi_stat.rssi >> 1;
	/*@Becareful RA use our "Tx" capability which means the capability of their "Rx"*/
	tx_nss = halbb_nss_mapping(bb, asoc_cap_i->nss_rx);
	ra_cfg->is_dis_ra = hal_sta_i->ra_info.dis_ra;
	mod_mask = hal_sta_i->ra_info.cur_ra_mask;
	ra_cfg->upd_all= false;
	ra_cfg->upd_bw_nss_mask= true;
	ra_cfg->upd_mask= false;
	mode = ra_cfg->mode_ctrl;
	if (mode == 0)
		return ret_val;
	ra_cfg->mode_ctrl = mode;

	/* ONLY need to get the optional gi-ltf combination for H2C FW*/
	/* bit(0)=4x0.8, bit(1)=1x0.8 -> different definition from the drver giltf*/
	giltf_cap = halbb_get_opt_giltf(bb, asoc_cap_i->ltf_gi);
	ra_cfg->giltf_cap = halbb_ax_giltf_chk(bb, mode, giltf_cap);

	/* giltf assigned by phl/halbb or trained by FW*/
	if (hal_sta_i->ra_info.fix_giltf_en == false) { /*giltf from halbb*/
		if (bb->ic_type & (BB_RTL8852A | BB_RTL8852B | BB_RTL8852C)) { /*Disable FW train GI_LTF*/
			ra_cfg->fix_giltf_en = true;
			ra_cfg->fix_giltf = RTW_GILTF_2XHE08;
			/* gi_ltf is decided by delay spread. This is not ready.
			if (is_giltf_decided_by_delay_sp)
				ra_cfg->fix_giltf = halbb_ra_giltf_ctrl(bb, macid, delay_sp, asoc_cap_i->ltf_gi);
			*/
		} else { /*Enable FW train GI_LTF*/
			ra_cfg->fix_giltf_en = false;
		}
	} else { /*giltf from phl*/
		ra_cfg->fix_giltf = halbb_giltf_trans(bb, asoc_cap_i->ltf_gi, hal_sta_i->ra_info.cal_giltf);
	}
	BB_DBG(bb, DBG_RA, "fix_giltf_en(phl)=%d, fix_giltf_en(Final)=%d, fix_giltf=%d\n",
	       hal_sta_i->ra_info.fix_giltf_en, ra_cfg->fix_giltf_en, ra_cfg->fix_giltf);

	ra_cfg->bw_cap = halbb_hw_bw_mapping(bb, phl_sta_i->chandef.bw, mode);
	ra_cfg->init_rate_lv = 0;
	ra_cfg->ss_num = tx_nss;
	mod_mask = halbb_ramask_mod(bb, macid, mod_mask, rssi, mode, tx_nss);
	halbb_ramask_trans(bb, macid, mod_mask);
	BB_DBG(bb, DBG_RA, "RA update: %x %x %x\n", bb_h2c[0], bb_h2c[1],
	       bb_h2c[2]);
	ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_MACIDCFG, HALBB_H2C_RA, bb_h2c);
	return ret_val;
}

bool halbb_raupdate_mask(struct bb_info *bb, struct rtw_phl_stainfo_t *phl_sta_i)
{
	u8 macid;
	struct bb_h2c_ra_cfg_info *ra_cfg;
	struct rtw_hal_stainfo_t *hal_sta_i;
	struct protocol_cap_t *asoc_cap_i;
	u8 tx_nss;
	u8 rssi;
	u8 init_lv = 0;
	u8 mode = 0;
	u8 giltf_cap = 0;
	bool ret_val = false;
	/* Need to mapping with driver wifi mode*/
	u32 *bb_h2c;
	u8 cmdlen = sizeof(struct bb_h2c_ra_cfg_info);
	u64 mod_mask;

	macid = (u8)(phl_sta_i->macid);
	hal_sta_i = phl_sta_i->hal_sta;

	ra_cfg = &bb->bb_ra_i[macid].ra_cfg;
	mode = ra_cfg->mode_ctrl;
	bb_h2c = (u32 *) ra_cfg;
	asoc_cap_i = &phl_sta_i->asoc_cap;
	rssi = hal_sta_i->rssi_stat.rssi >> 1;
	/*@Becareful RA use our "Tx" capability which means the capability of their "Rx"*/
	tx_nss = halbb_nss_mapping(bb, asoc_cap_i->nss_rx);
	ra_cfg->is_dis_ra = hal_sta_i->ra_info.dis_ra;
	mod_mask = hal_sta_i->ra_info.cur_ra_mask;
	ra_cfg->upd_all= false;
	ra_cfg->upd_bw_nss_mask= false;
	ra_cfg->upd_mask= true;
	/* while ra mask is updated, gi_ltf can also be update */
	mode = ra_cfg->mode_ctrl;
	if (mode == 0)
		return ret_val;
	ra_cfg->mode_ctrl = mode;
	ra_cfg->init_rate_lv = 0;
	/* ONLY need to get the optional gi-ltf combination for H2C FW*/
	/* bit(0)=4x0.8, bit(1)=1x0.8 -> different definition from the drver giltf*/
	giltf_cap = halbb_get_opt_giltf(bb, asoc_cap_i->ltf_gi);
	ra_cfg->giltf_cap = halbb_ax_giltf_chk(bb, mode, giltf_cap);

	/* giltf assigned by phl/halbb or trained by FW*/
	if (hal_sta_i->ra_info.fix_giltf_en == false) { /*giltf from halbb*/
		if (bb->ic_type & (BB_RTL8852A | BB_RTL8852B | BB_RTL8852C)) { /*Disable FW train GI_LTF*/
			ra_cfg->fix_giltf_en = true;
			ra_cfg->fix_giltf = RTW_GILTF_2XHE08;
			/* gi_ltf is decided by delay spread. This is not ready.
			if (is_giltf_decided_by_delay_sp)
				ra_cfg->fix_giltf = halbb_ra_giltf_ctrl(bb, macid, delay_sp, asoc_cap_i->ltf_gi);
			*/
		} else { /*Enable FW train GI_LTF*/
			ra_cfg->fix_giltf_en = false;
		}
	} else { /*giltf from phl*/
		ra_cfg->fix_giltf = halbb_giltf_trans(bb, asoc_cap_i->ltf_gi, hal_sta_i->ra_info.cal_giltf);
	}
	BB_DBG(bb, DBG_RA, "fix_giltf_en(phl)=%d, fix_giltf_en(Final)=%d, fix_giltf=%d\n",
	       hal_sta_i->ra_info.fix_giltf_en, ra_cfg->fix_giltf_en, ra_cfg->fix_giltf);

	mod_mask = halbb_ramask_mod(bb, macid, mod_mask, rssi, mode, tx_nss);
	halbb_ramask_trans(bb, macid, mod_mask);
	BB_DBG(bb, DBG_RA, "RA update mask: %x %x %x\n", bb_h2c[0], bb_h2c[1],
		   bb_h2c[2]);
	ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_MACIDCFG, HALBB_H2C_RA, bb_h2c);
	return ret_val;
}

u32 halbb_get_fw_ra_rpt(struct bb_info *bb, u16 len, u8 *c2h)
{
	u16 macid_rpt;
	struct rtw_hal_stainfo_t *hal_sta_i;
	struct rtw_phl_stainfo_t *phl_sta_i;
	struct rtw_rate_info *rt_i;
	struct halbb_ra_rpt_info *ra_rpt_i;

	if (!c2h)
		return 0;
	ra_rpt_i = (struct halbb_ra_rpt_info *)c2h;
	macid_rpt = ra_rpt_i->rpt_macid_l + (ra_rpt_i->rpt_macid_m << 8);
	if (macid_rpt >= PHL_MAX_STA_NUM) {
		BB_WARNING("[%s]Error macid = %d!!\n", __func__, macid_rpt);
		return 0;
	}
	if (!bb->sta_exist[macid_rpt]) {
		BB_WARNING("[%s]Error macid = %d!!\n", __func__, macid_rpt);
		return 0;
	}
	phl_sta_i = bb->phl_sta_info[(u8)macid_rpt];
	if (!phl_sta_i) {
		BB_WARNING("[%s]phl_sta==NULL, Wrong C2H RA macid !!\n", __func__);
		return 0;
	}
	hal_sta_i = phl_sta_i->hal_sta;
	if (!hal_sta_i) {
		BB_WARNING("[%s]hal_sta==NULL, Wrong C2H RA macid !!\n", __func__);
		return 0;
	}
	hal_sta_i->ra_info.curr_retry_ratio = ra_rpt_i->retry_ratio;
	rt_i = &hal_sta_i->ra_info.rpt_rt_i;
	rt_i->mcs_ss_idx = ra_rpt_i->rpt_mcs_nss;
	rt_i->gi_ltf = ra_rpt_i->rpt_gi_ltf;
	rt_i->bw = ra_rpt_i->rpt_bw;
	rt_i->mode = ra_rpt_i->rpt_md_sel;
	BB_DBG(bb, DBG_RA, "RA RPT: macid = %d, mode = %d, giltf = %x, mcs_nss = %x\n",
		   macid_rpt, rt_i->mode, rt_i->gi_ltf, rt_i->mcs_ss_idx);
	return 0;
}

bool rtw_halbb_query_txsts(struct bb_info *bb, u16 macid0, u16 macid1)
{
	u8 content[4];
	u32 *bb_h2c = (u32 *)content;
	bool ret_val;
	u16 cmdlen = 4;

	BB_DBG(bb, DBG_RA, "====> QuerryTxSts : macid = %d %d\n", macid0, macid1);
	content[0] = (u8)(macid0& 0xff);
	content[1] = (u8)((macid0>>8)& 0xff);
	content[2] = (u8)(macid1& 0xff);
	content[3] = (u8)((macid1>>8)& 0xff);
	ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_GET_TXSTS, HALBB_H2C_RA, bb_h2c);
	if (!ret_val)
		BB_WARNING("Error H2C cmd in querry txsts !!\n");
	return ret_val;
}

void halbb_drv_cmac_rpt_parsing(struct bb_info *bb, u8 *rpt)
{
	struct bb_fw_cmac_rpt_info *c_rpt;
	u16 rpt_len, i;
	u8 *rpt_tmp = rpt;

	if (!rpt)
		return;
	c_rpt = (struct bb_fw_cmac_rpt_info *)rpt;
	rpt_len = sizeof(struct bb_fw_cmac_rpt_info);
	if (rpt_len % 4) {
		return;
	}
	BB_DBG(bb, DBG_RA,
	       "[%d] try_rate=%d, fix_rate=0x%x, final{rate, giltf}={0x%x,0x%x}, data_bw = 0x%x\n",
	       c_rpt->macid, c_rpt->try_rate, c_rpt->fix_rate,
	       c_rpt->final_rate, c_rpt->final_gi_ltf, c_rpt->data_bw);
	for (i = 0; i < rpt_len; i++) {
		BB_DBG(bb, DBG_RA, "[%d] 0x%x\n", i, *(u32*)rpt);
		rpt_tmp += 4;
	}
}

u32 halbb_get_txsts_rpt(struct bb_info *bb, u16 len, u8 *c2h)
{
	u16 macid_rpt;
	u8 i = 0;
	struct rtw_hal_stainfo_t *hal_sta_i;
	struct rtw_phl_stainfo_t *phl_sta_i;
	struct rtw_ra_sta_info *ra_sta_i;
	struct halbb_txsts_info *txsts_i;
	u16 tx_total;
	u16 tx_ok[4];
	u16 tx_retry[4];

	if (!c2h)
		return 0;
	txsts_i = (struct halbb_txsts_info *)c2h;
	macid_rpt = txsts_i->rpt_macid_l + (txsts_i->rpt_macid_m << 8);
	if (macid_rpt >= PHL_MAX_STA_NUM) {
		BB_WARNING("report macid = %d, Error macid !!\n", macid_rpt);
		return 0;
	}
	if (!bb->sta_exist[macid_rpt]) {
		BB_WARNING("report macid = %d, Error macid !!\n", macid_rpt);
		return 0;
	}
	phl_sta_i = bb->phl_sta_info[(u8)macid_rpt];
	if (phl_sta_i == NULL) {
		BB_WARNING("phl_sta == NULL, Wrong C2H RA macid !!\n");
		return 0;
	}
	hal_sta_i = phl_sta_i->hal_sta;
	if (hal_sta_i == NULL) {
		BB_WARNING("hal_sta == NULL, Wrong C2H RA macid !!\n");
		return 0;
	}
	ra_sta_i = &hal_sta_i->ra_info;
	tx_ok[0] = txsts_i->tx_ok_be_l + (txsts_i->tx_ok_be_m << 8);
	tx_ok[1] = txsts_i->tx_ok_bk_l + (txsts_i->tx_ok_bk_m << 8);
	tx_ok[2] = txsts_i->tx_ok_vi_l + (txsts_i->tx_ok_vi_m << 8);
	tx_ok[3] = txsts_i->tx_ok_vo_l + (txsts_i->tx_ok_vo_m << 8);
	tx_retry[0] = txsts_i->tx_retry_be_l + (txsts_i->tx_retry_be_m << 8);
	tx_retry[1] = txsts_i->tx_retry_bk_l + (txsts_i->tx_retry_bk_m << 8);
	tx_retry[2] = txsts_i->tx_retry_vi_l + (txsts_i->tx_retry_vi_m << 8);
	tx_retry[3] = txsts_i->tx_retry_vo_l + (txsts_i->tx_retry_vo_m << 8);
	for ( i = 0; i <= 3; i++) {
		if ((0xffffffff - ra_sta_i->tx_ok_cnt[i]) > (tx_ok[i]))
			ra_sta_i->tx_ok_cnt[i] += tx_ok[i];
		if ((0xffffffff - ra_sta_i->tx_retry_cnt[i]) > (tx_retry[i]))
			ra_sta_i->tx_retry_cnt[i] += tx_retry[i];
		BB_DBG(bb, DBG_RA, "TxOk[%d] = %d, TxRty[%d] = %d\n", i, tx_ok[i], i, tx_retry[i]);
	}
	tx_total = txsts_i->tx_total_l + (txsts_i->tx_total_m << 8);
	if ((0xffffffff - ra_sta_i->tx_total_cnt) > tx_total)
		ra_sta_i->tx_total_cnt += tx_total;
	BB_DBG(bb, DBG_RA, "====> GetTxSts : TxTotal = %d\n", tx_total);
	return 0;
}

void halbb_ra_rssisetting(struct bb_info *bb)
{
	u8 macid = 0;
	u8 i = 0, sta_cnt = 0;
	u16 cmdlen;
	bool ret_val = true;
	struct rtw_ra_sta_info *bb_ra;
	struct rtw_hal_stainfo_t *hal_sta_i;
	struct bb_h2c_rssi_setting *rssi_i;
	struct bb_link_info *bb_link = &bb->bb_link_i;
	u16 rssi_len = 0;
	u32 *bb_h2c;
	u8 rssi_a = 0;
	u8 rssi_b = 0;

	rssi_len = sizeof(struct bb_h2c_rssi_setting) * PHL_MAX_STA_NUM;
	rssi_i = hal_mem_alloc(bb->hal_com, rssi_len);
	if (!rssi_i) {
		BB_WARNING(" Error RSSI allocat failed!!\n");
		return;
	}
	bb_h2c = (u32 *)rssi_i;
	cmdlen = rssi_len;
	for (i = 0; i < PHL_MAX_STA_NUM; i++) {
		if (!bb->sta_exist[i] || !bb->phl_sta_info[i])
			continue;

		hal_sta_i = bb->phl_sta_info[i]->hal_sta;
		if (!hal_sta_i)
			continue;

		bb_ra = &hal_sta_i->ra_info;
		if (!(bb_ra->ra_registered))
			continue;
		
		macid = (u8)(bb->phl_sta_info[i]->macid);
		BB_DBG(bb, DBG_RA, "Add BB rssi info[%d], macid=%d\n", i, macid);
		/* Need modify for Nss > 2 */
		rssi_a = (hal_sta_i->rssi_stat.rssi_ma_path[0] >> 5) & 0x7f;
		if (!bb->hal_com->dbcc_en) 
			rssi_b = (hal_sta_i->rssi_stat.rssi_ma_path[1] >> 5) & 0x7f;
		rssi_i[sta_cnt].macid = macid;
		rssi_i[sta_cnt].rssi = rssi_a | BIT(7);
		rssi_i[sta_cnt].rainfo1 = 0;
		rssi_i[sta_cnt].rainfo2 = 0; /* RSVD */
		rssi_i[sta_cnt].drv_ractrl = 0; /* RSVD */
		rssi_i[sta_cnt].is_fixed_rate = bb_ra->fixed_rt_en;
		rssi_i[sta_cnt].fixed_rate = bb_ra->fixed_rt_i.mcs_ss_idx;
		rssi_i[sta_cnt].fixed_rate_md = bb_ra->fixed_rt_i.mode;
		rssi_i[sta_cnt].fixed_giltf = bb_ra->fixed_rt_i.gi_ltf;
		rssi_i[sta_cnt].fixed_bw = bb_ra->fixed_rt_i.bw;
		rssi_i[sta_cnt].rsvd2_rssi_b = rssi_b;
		sta_cnt++;

		if (sta_cnt == bb_link->num_linked_client)
			break;
	}

	if (sta_cnt == STA_NUM_RSSI_CMD) {
		/* Fill endcmd = 1 for last sta */
		rssi_i[sta_cnt - 1].endcmd = 1;
		ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_RSSISETTING,
				   HALBB_H2C_RA, bb_h2c);
		BB_DBG(bb, DBG_RA, "sta_cnt=%d, RSSI cmd end 1\n", sta_cnt);
	} else if ((sta_cnt > 0) && (sta_cnt < STA_NUM_RSSI_CMD)) {
		/* Fill endcmd = 1 for last sta */
		rssi_i[sta_cnt - 1].endcmd = 1;
		rssi_i[sta_cnt].macid = 0xff;
		ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_RSSISETTING,
				   HALBB_H2C_RA, bb_h2c);
		BB_DBG(bb, DBG_RA, "sta_cnt=%d, RSSI cmd end 2\n", sta_cnt);
	}

	BB_DBG(bb, DBG_RA, "RSSI=>h2c[0~3]: %x %x %x %x\n", bb_h2c[0], bb_h2c[1],
	       bb_h2c[2], bb_h2c[3]);

	if (ret_val == false)
		BB_WARNING(" H2C cmd error!!\n");

	if (rssi_i)
		hal_mem_free(bb->hal_com, rssi_i, rssi_len);

}

void halbb_ra_giltf_ctrl(struct bb_info *bb, u8 macid, u8 delay_sp, u8 assoc_giltf)
{
	struct bb_ra_info *bb_ra = &bb->bb_ra_i[macid];
	struct bb_h2c_ra_cfg_info *ra_cfg = &bb->bb_ra_i[macid].ra_cfg;
	enum rtw_gi_ltf giltf = RTW_GILTF_LGI_4XHE32;

	if (!bb)
		return;
	/*This dhould be decision before ra mask h2c*/
	/* GI LTF decision algorithm is needed*/
	/*if (delay_sp)
		giltf = RTW_GILTF_LGI_4XHE32;
	else
		giltf = RTW_GILTF_LGI_4XHE32;
	ra_cfg->giltf = giltf;*/
}

void halbb_ra_watchdog(struct bb_info *bb)
{
	struct bb_link_info *bb_link = &bb->bb_link_i;
	u8 i = 0, sta_cnt = 0, macid = 0;
#if 0
	if (!(bb->support_ability & BB_RA))
		return;
#endif

	halbb_ra_rssisetting(bb);
	for (i = 0; i < PHL_MAX_STA_NUM; i++) {
		if (!(bb->sta_exist[i]))
			continue;

		if (!(bb->phl_sta_info[i]) || !bb) {
			BB_WARNING("Error RA registered !!! Pointer is NULL!!!\n");
			continue;
		}

		if (!(bb->phl_sta_info[i]->hal_sta)) {
			BB_WARNING("Error RA registered !!! Pointer is NULL!!!\n");
			continue;
		}

		if (!(bb->phl_sta_info[i]->hal_sta->ra_info.ra_registered))
			continue;

		macid = (u8)(bb->phl_sta_info[i]->macid);
		BB_DBG(bb, DBG_RA, "====>ra update mask[%d], macid=%d\n", i, macid);
		halbb_raupdate_mask(bb, bb->phl_sta_info[i]);

		sta_cnt++;

		if (sta_cnt == bb_link->num_linked_client)
			break;
	}
}

void halbb_ra_init(struct bb_info *bb)
{
	struct bb_ra_info *bb_ra = NULL;
	u8 macid = 0;

	for (macid = 0; macid < PHL_MAX_STA_NUM; macid ++) {
		bb_ra = &bb->bb_ra_i[macid];
		if (!bb_ra)
			halbb_mem_set(bb, bb_ra, 0, sizeof (struct bb_ra_info));
	}
}

u8 rtw_halbb_rts_rate(struct bb_info *bb, u16 tx_rate, bool is_erp_prot)
{

	u8 rts_ini_rate = RTW_DATA_RATE_OFDM6;

	if (is_erp_prot) { /* use CCK rate as RTS */
		rts_ini_rate = RTW_DATA_RATE_CCK1;
	} else {
		switch (tx_rate) {
		case RTW_DATA_RATE_VHT_NSS4_MCS9:
		case RTW_DATA_RATE_VHT_NSS4_MCS8:
		case RTW_DATA_RATE_VHT_NSS4_MCS7:
		case RTW_DATA_RATE_VHT_NSS4_MCS6:
		case RTW_DATA_RATE_VHT_NSS4_MCS5:
		case RTW_DATA_RATE_VHT_NSS4_MCS4:
		case RTW_DATA_RATE_VHT_NSS4_MCS3:
		case RTW_DATA_RATE_VHT_NSS3_MCS9:
		case RTW_DATA_RATE_VHT_NSS3_MCS8:
		case RTW_DATA_RATE_VHT_NSS3_MCS7:
		case RTW_DATA_RATE_VHT_NSS3_MCS6:
		case RTW_DATA_RATE_VHT_NSS3_MCS5:
		case RTW_DATA_RATE_VHT_NSS3_MCS4:
		case RTW_DATA_RATE_VHT_NSS3_MCS3:
		case RTW_DATA_RATE_VHT_NSS2_MCS9:
		case RTW_DATA_RATE_VHT_NSS2_MCS8:
		case RTW_DATA_RATE_VHT_NSS2_MCS7:
		case RTW_DATA_RATE_VHT_NSS2_MCS6:
		case RTW_DATA_RATE_VHT_NSS2_MCS5:
		case RTW_DATA_RATE_VHT_NSS2_MCS4:
		case RTW_DATA_RATE_VHT_NSS2_MCS3:
		case RTW_DATA_RATE_VHT_NSS1_MCS9:
		case RTW_DATA_RATE_VHT_NSS1_MCS8:
		case RTW_DATA_RATE_VHT_NSS1_MCS7:
		case RTW_DATA_RATE_VHT_NSS1_MCS6:
		case RTW_DATA_RATE_VHT_NSS1_MCS5:
		case RTW_DATA_RATE_VHT_NSS1_MCS4:
		case RTW_DATA_RATE_VHT_NSS1_MCS3:
		case RTW_DATA_RATE_MCS31:
		case RTW_DATA_RATE_MCS30:
		case RTW_DATA_RATE_MCS29:
		case RTW_DATA_RATE_MCS28:
		case RTW_DATA_RATE_MCS27:
		case RTW_DATA_RATE_MCS23:
		case RTW_DATA_RATE_MCS22:
		case RTW_DATA_RATE_MCS21:
		case RTW_DATA_RATE_MCS20:
		case RTW_DATA_RATE_MCS19:
		case RTW_DATA_RATE_MCS15:
		case RTW_DATA_RATE_MCS14:
		case RTW_DATA_RATE_MCS13:
		case RTW_DATA_RATE_MCS12:
		case RTW_DATA_RATE_MCS11:
		case RTW_DATA_RATE_MCS7:
		case RTW_DATA_RATE_MCS6:
		case RTW_DATA_RATE_MCS5:
		case RTW_DATA_RATE_MCS4:
		case RTW_DATA_RATE_MCS3:
		case RTW_DATA_RATE_OFDM54:
		case RTW_DATA_RATE_OFDM48:
		case RTW_DATA_RATE_OFDM36:
		case RTW_DATA_RATE_OFDM24:
			rts_ini_rate = RTW_DATA_RATE_OFDM24;
			break;
		case RTW_DATA_RATE_VHT_NSS4_MCS2:
		case RTW_DATA_RATE_VHT_NSS4_MCS1:
		case RTW_DATA_RATE_VHT_NSS3_MCS2:
		case RTW_DATA_RATE_VHT_NSS3_MCS1:
		case RTW_DATA_RATE_VHT_NSS2_MCS2:
		case RTW_DATA_RATE_VHT_NSS2_MCS1:
		case RTW_DATA_RATE_VHT_NSS1_MCS2:
		case RTW_DATA_RATE_VHT_NSS1_MCS1:
		case RTW_DATA_RATE_MCS26:
		case RTW_DATA_RATE_MCS25:
		case RTW_DATA_RATE_MCS18:
		case RTW_DATA_RATE_MCS17:
		case RTW_DATA_RATE_MCS10:
		case RTW_DATA_RATE_MCS9:
		case RTW_DATA_RATE_MCS2:
		case RTW_DATA_RATE_MCS1:
		case RTW_DATA_RATE_OFDM18:
		case RTW_DATA_RATE_OFDM12:
			rts_ini_rate = RTW_DATA_RATE_OFDM12;
			break;
		case RTW_DATA_RATE_VHT_NSS4_MCS0:
		case RTW_DATA_RATE_VHT_NSS3_MCS0:
		case RTW_DATA_RATE_VHT_NSS2_MCS0:
		case RTW_DATA_RATE_VHT_NSS1_MCS0:
		case RTW_DATA_RATE_MCS24:
		case RTW_DATA_RATE_MCS16:
		case RTW_DATA_RATE_MCS8:
		case RTW_DATA_RATE_MCS0:
		case RTW_DATA_RATE_OFDM9:
		case RTW_DATA_RATE_OFDM6:
			rts_ini_rate = RTW_DATA_RATE_OFDM6;
			break;
		case RTW_DATA_RATE_CCK11:
		case RTW_DATA_RATE_CCK5_5:
		case RTW_DATA_RATE_CCK2:
		case RTW_DATA_RATE_CCK1:
			rts_ini_rate = RTW_DATA_RATE_CCK1;
			break;
		default:
			rts_ini_rate = RTW_DATA_RATE_OFDM6;
			break;
		}
	}

	if (bb->hal_com->band[0].cur_chandef.band == BAND_ON_5G) {
		if (rts_ini_rate < RTW_DATA_RATE_OFDM6)
			rts_ini_rate = RTW_DATA_RATE_OFDM6;
	}
	return rts_ini_rate;
}

void halbb_ra_dbg(struct bb_info *bb, char input[][16], u32 *_used,
			 char *output, u32 *_out_len)
{
	char help[] = "-h";
	u32 val[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i;
	u8 rssi_assoc = 60;
	bool ret_val = false;
	struct rtw_ra_sta_info *bb_ra;
	struct rtw_hal_stainfo_t *hal_sta_i;
	//struct bb_h2c_rssi_setting *rssi_i;
	u16 rssi_len = 0;
	u8 rssi_a = 0;
	u8 rssi_b = 0;

	if (_os_strcmp(input[1], help) == 0) {
		//BB_DBG_CNSL(out_len, used, output + used, out_len - used,
		//	 "{Test ra mode}: [ra] [100] [macid] [mode 0: fixed rate, 1:fixed macidcfg]\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{Fix rate} [ra] [1] [macid] [mode] [giltf] [ss_mcs] [bw])\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{Auto rate}: [ra] [2] [macid]\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "=============Notes=============>\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "[mode]: 0:(legacy), 1:(HT), 2:(VHT), 3:(HE)\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "[giltf]: 0: (4xHE-LTF 3.2usGI), 1: (4xHE-LTF 0.8usGI), 2: (2xHE-LTF 1.6usGI)\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "[giltf]: 3: (2xHE-LTF 0.8usGI), 4: (1xHE-LTF 1.6usGI), 5: (1xHE-LTF 0.8usGI)\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "[ss_mcs]: (Bitmap format) [6:4]: Nsts [3:0]: MCS\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "==============================>\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{Drvier shift rate up/down threshold}: [ra] [3] [macid] [0: Increase th. (Tend to RU) 1: Decrease th (Tend to RD)] [percent]\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{Manually adjust RA mask}: [ra] [4] [macid] [0: dis. manual adj. RA mask; 1: en. manual adj. RA mask] [0: mask; 1: reveal] [rate_mode] [ss_mcs]\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{d_o_timer}: [ra] [5] [macid] [en] [timer (FW default 20)]\n");

		//BB_DBG_CNSL(out_len, used, output + used, out_len - used,
		//	 "{Fix rate & ra mask}: ra (3 [macid] [mode] [giltf] [ss_mcs] [mask1] [mask0])}\n");
		goto out;
	}
	for (i = 0; i < 8; i++) {
		if (input[i + 1])
			HALBB_SCAN(input[i + 1], DCMD_DECIMAL, &val[i]);
	}
	//rssi_len = sizeof(struct bb_h2c_rssi_setting);
	//rssi_i = hal_mem_alloc(bb->hal_com, rssi_len);

	if (0) { /* Test RA mode */
		/*
		bb_h2c = (u32 *) &ra_cfg;
		ra_cfg.bw_cap = 0;
		ra_cfg.mode_ctrl = (u8)val[2];
		ra_cfg.is_dis_ra= false;
		ra_cfg.macid = (u8)val[1];
		ra_cfg.stbc_cap = false;
		ra_cfg.ldpc_cap = false;
		ra_cfg.en_sgi = false;
		ra_cfg.upd_all = true;
		ra_cfg.init_rate_lv = 1;
		ra_cfg.er_cap = false;
		ra_cfg.dcm_cap = false;
		ra_cfg.upd_mask = false;
		ra_cfg.upd_bw_nss_mask = false;
		ra_cfg.giltf = 0;
		ra_cfg.ss_num = 1;
		for (i = 0; i < 8; i++)
			ra_cfg.ramask[i] = 0xff;
		BB_DBG(bb, DBG_RA, "RA Register: %x %x %x\n", bb_h2c[0], bb_h2c[1],
		       bb_h2c[2]);
		ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_MACIDCFG, HALBB_H2C_RA, bb_h2c);
		*/
	} else if (val[0] == 1) {
		struct bb_h2c_rssi_setting rssi_i;
		u32 *bb_h2c = (u32 *) &rssi_i;
		u8 cmdlen = sizeof(struct bb_h2c_rssi_setting);
		
		hal_sta_i = bb->phl_sta_info[(u8)val[1]]->hal_sta;
		bb_ra = &hal_sta_i->ra_info;
		if (bb_ra->ra_registered) {
			BB_DBG(bb, DBG_RA, "RA fix rate macid=[%d]\n", (u8)val[1]);
			/* Need modify for Nss > 2 */
			rssi_a = (hal_sta_i->rssi_stat.rssi_ma_path[0] >> 5) & 0x7f;
			if (!bb->hal_com->dbcc_en) 
				rssi_b = (hal_sta_i->rssi_stat.rssi_ma_path[1] >> 5) & 0x7f;
			rssi_i.macid = (u8)val[1];
			rssi_i.rssi = rssi_a | BIT(7);
			rssi_i.is_fixed_rate = true;
			rssi_i.fixed_rate = (u8)val[4];
			rssi_i.fixed_giltf = (u8)val[3];
			rssi_i.fixed_bw = (u8)val[5];
			rssi_i.fixed_rate_md = (u8)val[2];
			rssi_i.rsvd2_rssi_b = rssi_b;
			rssi_i.endcmd = 1;

			bb_ra->fixed_rt_en = rssi_i.is_fixed_rate;
			bb_ra->fixed_rt_i.mcs_ss_idx = rssi_i.fixed_rate;
			bb_ra->fixed_rt_i.mode = rssi_i.fixed_rate_md;
			bb_ra->fixed_rt_i.gi_ltf = rssi_i.fixed_giltf;
			bb_ra->fixed_rt_i.bw = rssi_i.fixed_bw;
			BB_DBG(bb, DBG_RA, "RA fix rate H2C: %x %x\n", bb_h2c[0], bb_h2c[1]);

			ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_RSSISETTING, HALBB_H2C_RA, bb_h2c);
		} else {
			BB_DBG(bb, DBG_RA, "No Link ! RA rssi cmd fail!\n");
		}
	} else if (val[0] == 2) {
		struct bb_h2c_rssi_setting rssi_i;
		u32 *bb_h2c = (u32 *) &rssi_i;
		u8 cmdlen = sizeof(struct bb_h2c_rssi_setting);
		
		hal_sta_i = bb->phl_sta_info[(u8)val[1]]->hal_sta;
		bb_ra = &hal_sta_i->ra_info;
		if (bb_ra->ra_registered) {
			BB_DBG(bb, DBG_RA, "RA auto rate macid=[%d]\n", (u8)val[1]);
			/* Need modify for Nss > 2 */
			rssi_a = (hal_sta_i->rssi_stat.rssi_ma_path[0] >> 5) & 0x7f;
			if (!bb->hal_com->dbcc_en) 
				rssi_b = (hal_sta_i->rssi_stat.rssi_ma_path[1] >> 5) & 0x7f;
			rssi_i.macid = (u8)val[1];
			rssi_i.rssi = rssi_a | BIT(7);
			rssi_i.is_fixed_rate = false;
			rssi_i.rsvd2_rssi_b = rssi_b;
			rssi_i.endcmd = 1;

			bb_ra->fixed_rt_en = rssi_i.is_fixed_rate;
			BB_DBG(bb, DBG_RA, "RA auto rate H2C: %x %x\n", bb_h2c[0], bb_h2c[1]);
			
			ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_RSSISETTING, HALBB_H2C_RA, bb_h2c);
		} else {
			BB_DBG(bb, DBG_RA, "No Link ! RA rssi cmd fail!\n");
		}
	} else if (val[0] == 3) {
		struct bb_h2c_ra_adjust ra_th_i;
		u32 *bb_h2c = (u32 *) &ra_th_i;
		u8 cmdlen = sizeof(struct bb_h2c_ra_adjust);
		
		hal_sta_i = bb->phl_sta_info[(u8)val[1]]->hal_sta;
		bb_ra = &hal_sta_i->ra_info;
		if (bb_ra->ra_registered) {
			BB_DBG(bb, DBG_RA, "RA adjust th. macid=[%d]\n", (u8)val[1]);
			ra_th_i.macid = (u8)val[1];
			ra_th_i.drv_shift_en = (u8)val[2] & 0x01;
			ra_th_i.drv_shift_value= (u8)val[3] & 0x7f;

			BB_DBG(bb, DBG_RA, "RA adjust %s th =[%d]\n", 
			ra_th_i.drv_shift_en == 0x1 ? "RD": "RU", ra_th_i.drv_shift_value);
			
			BB_DBG(bb, DBG_RA, "RA adjust th H2C: %x %x\n", bb_h2c[0], bb_h2c[1]);
			
			ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_RA_ADJUST, HALBB_H2C_RA, bb_h2c);
		} else {
			BB_DBG(bb, DBG_RA, "No Link ! RA rssi cmd fail!\n");
		}
	} else if (val[0] == 4) {
		struct bb_h2c_ra_mask ra_mask_i;
		u32 *bb_h2c = (u32 *) &ra_mask_i;
		u8 cmdlen = sizeof(struct bb_h2c_ra_mask);
		
		hal_sta_i = bb->phl_sta_info[(u8)val[1]]->hal_sta;
		bb_ra = &hal_sta_i->ra_info;

		if (bb_ra->ra_registered) {
			BB_DBG(bb, DBG_RA, "macid=[%d]\n", (u8)val[1]);

			ra_mask_i.macid = (u8)val[1];
			ra_mask_i.is_manual_adjust_ra_mask = (u8)val[2] & 0x01;
			ra_mask_i.mask_or_reveal = (u8)val[3] & 0x01;
			ra_mask_i.mask_rate_md= (u8)val[4];
			ra_mask_i.mask_rate= (u8)val[5];

			BB_DBG(bb, DBG_RA, "Manual adjust RA mask = %d\n",
			       ra_mask_i.is_manual_adjust_ra_mask);
			BB_DBG(bb, DBG_RA, "Adjust mode=%d, rate(d')=%d\n",
			       ra_mask_i.mask_rate_md, ra_mask_i.mask_rate);
			BB_DBG(bb, DBG_RA, "[%s] this rate in RA mask\n",
			       ra_mask_i.mask_or_reveal ? "Reveal": "Mask");
			BB_DBG(bb, DBG_RA, "RA adjust th H2C: %x %x\n",
			       bb_h2c[0], bb_h2c[1]);

			ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_ADJUST_RA_MASK, HALBB_H2C_RA, bb_h2c);
		} else {
			BB_DBG(bb, DBG_RA, "No Link ! RA rssi cmd fail!\n");
		}
	} else if (val[0] == 5) {
		struct bb_h2c_ra_d_o_timer ra_d_o_timer_i;
		u32 *bb_h2c = (u32 *) &ra_d_o_timer_i;
		u8 cmdlen = sizeof(struct bb_h2c_ra_d_o_timer);
		
		hal_sta_i = bb->phl_sta_info[(u8)val[1]]->hal_sta;
		bb_ra = &hal_sta_i->ra_info;
		if (bb_ra->ra_registered) {
			BB_DBG(bb, DBG_RA, "RA d_o_timer macid=[%d]\n", (u8)val[1]);
			ra_d_o_timer_i.macid = (u8)val[1];
			ra_d_o_timer_i.d_o_timer_en = (u8)val[2] & 0x01;
			ra_d_o_timer_i.d_o_timer_value= (u8)val[3] & 0x7f;

			BB_DBG(bb, DBG_RA, "RA d_o_timer {en,value} = {%d,%d}\n", 
			ra_d_o_timer_i.d_o_timer_en , ra_d_o_timer_i.d_o_timer_value);
			
			BB_DBG(bb, DBG_RA, "RA d_o_timer H2C: %x\n", bb_h2c[0]);
			
			ret_val = halbb_fill_h2c_cmd(bb, cmdlen, RA_H2C_RA_D_O_TIMER, HALBB_H2C_RA, bb_h2c);
		} else {
			BB_DBG(bb, DBG_RA, "No Link ! RA d_o_timer cmd fail!\n");
		}
	}
	//if (rssi_i)
	//	hal_mem_free(bb->hal_com, rssi_i, rssi_len);
out:
	*_used = used;
	*_out_len = out_len;
}

#endif


