/******************************************************************************
 *
 * Copyright(c) 2007 - 2020  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/
#include "halbb_precomp.h"

#ifdef HALBB_PWR_CTRL_SUPPORT
/* @ Dynamic CCA TH part */

void halbb_set_ccath_macid(struct bb_info *bb, u16 macid, s8 cca_th, bool cca_th_en)
{
	u32 ret_v = 0;
	u32 mask_ccath = 0xff0000;
	u32 mask_en = BIT(26);
	struct rtw_phl_stainfo_t *phl_sta_i = NULL;
	enum phl_band_idx hw_band;
	u32 reg_ofst = REG_PWRMACID_OFST + (macid << 2) + 0xd200;

	if (!bb->sta_exist[macid]) {
		BB_WARNING("Error Set Pwr Macid for STA not exist!!\n");
		return;
	}

	phl_sta_i = bb->phl_sta_info[macid];

	if (!phl_sta_i)
		return;

	//phl_sta_i = bb->phl_sta_info[macid];
	hw_band = phl_sta_i->wrole->hw_band;
	/* pwr : S(8,1)*/
	ret_v |= rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, hw_band, reg_ofst, mask_ccath, cca_th);
	ret_v |= rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, hw_band, reg_ofst, mask_en, cca_th_en);
	BB_DBG(bb, DBG_PWR_CTRL, "halbb_set_ccath_macid = %x %x\n", cca_th, cca_th_en);
	if (ret_v != 0)
		BB_WARNING("Error Set Pwr Macid for API return fail!!\n");
}

void halbb_ccath_init(struct bb_info *bb)
{
	u8 i;
	struct bb_dyncca_info *dyncca_i = &bb->bb_dyncca_i;
	
	BB_DBG(bb, DBG_PWR_CTRL, "[%s]\n", __func__);
	dyncca_i->dyncca_offset = 10; /* clean */
	dyncca_i->dyncca_offset_n = 6; /* noisy */

	for (i = 0; i < PHL_MAX_STA_NUM; i++) {
		dyncca_i->macidcca_i[i].cca_th = 0; 
		dyncca_i->macidcca_i[i].cca_th_en= 0;
	}
}

void halbb_cca_th_per_sta(struct bb_info *bb, u16 macid)
{
	bool noisy_state = bb->is_noisy;
	struct bb_dyncca_info *dyncca_i = &bb->bb_dyncca_i;
	s8 dyn_ccath = 0;
	s8 offset = dyncca_i->dyncca_offset;
	struct rtw_phl_stainfo_t *sta;
	u8 rssi = 0;

	#if 0
	if (!dyncca_i) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL pointer!\n");
		return;
	}
	#endif

	sta = bb->phl_sta_info[macid];
	if (!sta) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL PHL STA info\n");
		return;
	}
	if (!is_sta_active(sta))
		return;

	rssi = sta->hal_sta->rssi_stat.rssi;

	if (noisy_state)
		offset = dyncca_i->dyncca_offset_n;

	dyn_ccath = (s8)rssi - 110 - offset; 
}

void halbb_dyncca_th(struct bb_info *bb)
{
	struct bb_dyncca_info *dyncca_i = &bb->bb_dyncca_i;
	u8 i;

	if (!dyncca_i) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL pointer!\n");
		return;
	}
	if (!dyncca_i->dyncca_en)
		return;
	/* Need to add support ability here */
	/*if (!(bb->support_ability & BB_PWR_CTRL))
		return;
	*/
	BB_DBG(bb, DBG_PWR_CTRL, "[%s]\n", __func__);

	for (i = 0; i < PHL_MAX_STA_NUM; i++)
		halbb_cca_th_per_sta( bb, i);
}

/* @ Power Ctrl part */
s8 halbb_get_pwr_macid_idx(struct bb_info *bb, u16 macid, u8 idx)
{
	s8 tx_pwr = 0;
	struct bb_dtp_info *dtp = &bb->bb_pwr_ctrl_i.dtp_i[macid];
	
	if (!dtp) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL pointer macid = %d\n", macid);
		return 0;
	}

	tx_pwr = dtp->pwr_val[idx];
	return tx_pwr; /* S(8,1)*/
}

bool halbb_get_pwr_macid_en_idx(struct bb_info *bb, u16 macid, u8 idx)
{
	bool pwr_en = false;
	struct bb_dtp_info *dtp = &bb->bb_pwr_ctrl_i.dtp_i[macid];
	
	if (!dtp) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL pointer macid = %d\n", macid);
		return 0;
	}
	pwr_en = dtp->en_pwr[idx];
	return pwr_en; /* bool*/
}

void halbb_set_pwr_macid_idx(struct bb_info *bb, u16 macid, s8 pwr, bool pwr_en, u8 idx)
{
	u32 ret_v = 0;
	u32 mask_pwr = 0xff;
	u32 mask_en = BIT(24);
	struct rtw_phl_stainfo_t *phl_sta_i = NULL;
	enum phl_band_idx hw_band;
	u32 reg_ofst = REG_PWRMACID_OFST + (macid << 2) + 0xd200;
	
	if (!bb->sta_exist[macid]) {
		BB_WARNING("Error Set Pwr Macid for STA not exist!!\n");
		return;
	}
	phl_sta_i = bb->phl_sta_info[macid];
	if (!phl_sta_i)
		return;
	//phl_sta_i = bb->phl_sta_info[macid];

	if (idx == 1) {
		mask_pwr = 0x0000ff00;
		mask_en = BIT(25);
	}

	hw_band = phl_sta_i->wrole->hw_band;
	/* pwr : S(8,1)*/
	ret_v |= rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, hw_band, reg_ofst, mask_pwr, pwr);
	ret_v |= rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, hw_band, reg_ofst, mask_en, pwr_en);
	BB_DBG(bb, DBG_PWR_CTRL, "halbb_set_pwr_macid(%d) = %x %x\n", idx, pwr, pwr_en);
	if (ret_v != 0)
		BB_WARNING("Error Set Pwr Macid for API return fail!!\n");
}

void halbb_pwr_ctrl_en(struct bb_info *bb, bool pwr_ctrl_en)
{
	u32 ret_v = 0;
	u32 mask_en = BIT(29);
	u32 reg_ofst = 0xd20c;

	BB_DBG(bb, DBG_PWR_CTRL, "halbb_pwr_ctrl_en() = %x\n", (u32)pwr_ctrl_en);

	if(pwr_ctrl_en) {
		ret_v |= rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, 0, reg_ofst, mask_en, 0x1);
		if (bb->hal_com->dbcc_en) 
			ret_v |= rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, 1, reg_ofst, mask_en, 0x1);
	} else {
		ret_v |= rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, 0, reg_ofst, mask_en, 0x0);
		if (bb->hal_com->dbcc_en) 
			ret_v |= rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, 1, reg_ofst, mask_en, 0x0);
	}
	
	if (ret_v != 0)
		BB_WARNING("Error Enable Power Control for API return fail!!\n");
}
void halbb_pwr_ctrl_th(struct bb_info *bb)
{
	bool noisy_state = bb->is_noisy;
	struct bb_pwr_ctrl_info *pwr_ctrl_i = &bb->bb_pwr_ctrl_i;

	if (!pwr_ctrl_i) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL pointer!\n");
		return;
	}

	if (noisy_state == 0) {
		pwr_ctrl_i->enhance_pwr_th[0] = pwr_ctrl_i->set_pwr_th[0];
		pwr_ctrl_i->enhance_pwr_th[1] = pwr_ctrl_i->set_pwr_th[1];
		pwr_ctrl_i->enhance_pwr_th[2] = pwr_ctrl_i->set_pwr_th[2];
	} else {
		pwr_ctrl_i->enhance_pwr_th[0] = pwr_ctrl_i->set_pwr_th[0] + 8;
		pwr_ctrl_i->enhance_pwr_th[1] = pwr_ctrl_i->set_pwr_th[1] + 5;
		pwr_ctrl_i->enhance_pwr_th[2] = pwr_ctrl_i->set_pwr_th[2];
	}

	BB_DBG(bb, DBG_PWR_CTRL,
		  "halbb pwr ctrl pwr_th: Lv1_th =%d ,Lv2_th = %d ,Lv3_th = %d\n",
		  pwr_ctrl_i->enhance_pwr_th[0], pwr_ctrl_i->enhance_pwr_th[1],
		  pwr_ctrl_i->enhance_pwr_th[2]);
}

u8 halbb_pwr_lvl_check(struct bb_info *bb, u8 rssi_in, u8 last_pwr_lvl)
{
	u8 i;
	u8 th[HALBB_PWR_STATE_NUM];
	struct bb_pwr_ctrl_info *pwr_ctrl_i = &bb->bb_pwr_ctrl_i;

	if (!pwr_ctrl_i) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL pointer!\n");
		return 0;
	}

	for (i = 0; i < HALBB_PWR_STATE_NUM; i++)
		th[i] = pwr_ctrl_i->set_pwr_th[i];

	BB_DBG(bb, DBG_PWR_CTRL,
		  "Ori-DTP th: Lv1_th = %d, Lv2_th = %d, Lv3_th = %d\n",
		  th[0], th[1], th[2]);

	for (i = 0; i < HALBB_PWR_STATE_NUM; i++) {
		if (i >= last_pwr_lvl)
			th[i] += DTP_FLOOR_UP_GAP;
	}
	BB_DBG(bb, DBG_PWR_CTRL,
		  "Mod-DTP th: Lv1_th = %d, Lv2_th = %d, Lv3_th = %d\n",
		  th[0], th[1], th[2]);

	if (rssi_in >= th[2])
		return TX_HP_LV_3;
	else if (rssi_in < (th[2] - 3) && rssi_in >= th[1])
		return TX_HP_LV_2;
	else if (rssi_in < (th[1] - 3) && rssi_in >= th[0])
		return TX_HP_LV_1;
	else if (rssi_in < (th[0] - 3))
		return TX_HP_LV_0;
	else
		return TX_HP_LV_UNCHANGE;
}

void halbb_set_pwr_ctrl(struct bb_info *bb, u16 macid, u8 pwr_lv)
{
	struct rtw_phl_stainfo_t *sta;
	s8 pwr = 0;
	bool pwr_ctrl_en = false;
	struct bb_pwr_ctrl_info *pwr_ctrl_i = &bb->bb_pwr_ctrl_i;

	if (!(bb->support_ability & BB_PWR_CTRL))
		return;

	sta = bb->phl_sta_info[macid];
	if (!sta) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL PHL STA info\n");
		return;
	}

	if (pwr_lv == TX_HP_LV_3) {
		pwr_ctrl_en = true;
		pwr = pwr_ctrl_i->pwr_lv_dbm[2];
	} else if (pwr_lv == TX_HP_LV_2) {
		pwr_ctrl_en = true;
		pwr = pwr_ctrl_i->pwr_lv_dbm[1];
	} else if (pwr_lv == TX_HP_LV_1) {
		pwr_ctrl_en = true;
		pwr = pwr_ctrl_i->pwr_lv_dbm[0];
	} else {
		pwr_ctrl_en = false;
		pwr = 0;
	}
	halbb_set_pwr_macid_idx(bb, macid, pwr, pwr_ctrl_en, 0);
	/* only use pwr_tbl_0 */
}

void halbb_pwr_ctrl_per_sta(struct bb_info *bb, u16 macid)
{
	struct rtw_phl_stainfo_t *sta;
	struct bb_dtp_info *dtp = &bb->bb_pwr_ctrl_i.dtp_i[macid];
	u8 lst_pwr_lv = 0;
	u8 rssi = 0;

	sta = bb->phl_sta_info[macid];
	if (!sta) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL PHL STA info\n");
		return;
	}
	if (is_sta_active(sta)) {
		rssi = (sta->hal_sta->rssi_stat.rssi) >> 1;
		lst_pwr_lv = dtp->last_pwr_lvl;
		dtp->dyn_tx_pwr_lvl = halbb_pwr_lvl_check(bb, rssi, lst_pwr_lv);
		BB_DBG(bb, DBG_PWR_CTRL,
			  "STA=%d , RSSI: %d , GetPwrLv: %d\n", macid,
			  rssi, dtp->dyn_tx_pwr_lvl);

		if (dtp->dyn_tx_pwr_lvl == TX_HP_LV_UNCHANGE ||
		    dtp->dyn_tx_pwr_lvl == lst_pwr_lv) {
			dtp->dyn_tx_pwr_lvl = lst_pwr_lv;
			BB_DBG(bb, DBG_PWR_CTRL,
				  "DTP_lv not change: ((%d))\n",
				  dtp->dyn_tx_pwr_lvl);
			return;
		}

		BB_DBG(bb, DBG_PWR_CTRL,
			  "DTP_lv update: ((%d)) -> ((%d))\n", lst_pwr_lv,
			  dtp->dyn_tx_pwr_lvl);

		dtp->last_pwr_lvl = dtp->dyn_tx_pwr_lvl;
		halbb_set_pwr_ctrl(bb, macid, dtp->dyn_tx_pwr_lvl);
	}
}

void halbb_pwr_ctrl(struct bb_info *bb)
{
	struct bb_pwr_ctrl_info *pwr_ctrl = &bb->bb_pwr_ctrl_i;
	u16 i;

	if (!(bb->support_ability & BB_PWR_CTRL)) {
		/* Disable*/
		halbb_pwr_ctrl_en(bb, false);
		return;
	}

	BB_DBG(bb, DBG_PWR_CTRL, "[%s]\n", __func__);
	/*Enable*/
	halbb_pwr_ctrl_en(bb, true);
	for (i = 0; i < PHL_MAX_STA_NUM; i++)
		halbb_pwr_ctrl_per_sta( bb, i);

	BB_DBG(bb, DBG_PWR_CTRL, "pwr = %d\n", pwr_ctrl->pwr);
}
void halbb_pwr_ctrl_para_init(struct bb_info *bb)
{
	struct bb_pwr_ctrl_info *pwr_ctrl_i = &bb->bb_pwr_ctrl_i;

	pwr_ctrl_i->set_pwr_th[0] = TX_PWR_TH_LVL1;
	pwr_ctrl_i->set_pwr_th[1] = TX_PWR_TH_LVL2;
	pwr_ctrl_i->set_pwr_th[2] = TX_PWR_TH_LVL3;

	pwr_ctrl_i->pwr_lv_dbm[0] = TX_PWR_LVL1;
	pwr_ctrl_i->pwr_lv_dbm[1] = TX_PWR_LVL2;
	pwr_ctrl_i->pwr_lv_dbm[2] = TX_PWR_LVL3;
}

void halbb_pwr_ctrl_txpwr_cfg(struct bb_info *bb, u8 pwr_lv1, u8 pwr_lv2, u8 pwr_lv3)
{
	struct bb_pwr_ctrl_info *pwr_ctrl_i = &bb->bb_pwr_ctrl_i;

	pwr_ctrl_i->pwr_lv_dbm[0] = pwr_lv1;
	pwr_ctrl_i->pwr_lv_dbm[1] = pwr_lv2;
	pwr_ctrl_i->pwr_lv_dbm[2] = pwr_lv3;
}

void halbb_pwr_ctrl_th_cfg(struct bb_info *bb, u8 th_lv1, u8 th_lv2, u8 th_lv3)
{
	struct bb_pwr_ctrl_info *pwr_ctrl_i = &bb->bb_pwr_ctrl_i;

	pwr_ctrl_i->set_pwr_th[0] = th_lv1;
	pwr_ctrl_i->set_pwr_th[1] = th_lv2;
	pwr_ctrl_i->set_pwr_th[2] = th_lv3;
}

void halbb_pwr_ctrl_init(struct bb_info *bb)
{
	struct bb_pwr_ctrl_info *pwr_ctrl_i = &bb->bb_pwr_ctrl_i;
	u8 i = 0;

	if (!pwr_ctrl_i) {
		BB_DBG(bb, DBG_PWR_CTRL, "NULL pointer!\n");
		return;
	}

	for (i = 0; i < PHL_MAX_STA_NUM; i++) {
		pwr_ctrl_i->dtp_i[i].last_pwr_lvl= TX_HP_LV_0;
		pwr_ctrl_i->dtp_i[i].dyn_tx_pwr_lvl= TX_HP_LV_0;
	}

	halbb_pwr_ctrl_para_init(bb);

	for (i = 0; i < HALBB_PWR_STATE_NUM; i++)
		pwr_ctrl_i->enhance_pwr_th[i] = 0xff;

	BB_DBG(bb, DBG_PWR_CTRL, "pwr = %d\n", pwr_ctrl_i->pwr);
}

void halbb_pwr_ctrl_dbg(struct bb_info *bb, char input[][16], u32 *_used,
		   char *output, u32 *_out_len)
{
	char help[] = "-h";
	u32 val[10] = {0};
	u8 i = 0;
	struct bb_pwr_ctrl_info *pwr_ctrl_i = &bb->bb_pwr_ctrl_i;

	if (_os_strcmp(input[1], help) == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{Use Org TH and PWR} [pwr_ctrl] [0]\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{Modify LV1(H)~LV3(L) Power} [pwr_ctrl] [1] [LV1] [LV2] [LV3]\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{Modify LV1(H)~LV3(L) TH} [pwr_ctrl] [2] [LV1] [LV2] [LV3]\n");
		return;
	}
	for (i = 0; i < 8; i++) {
		if (input[i + 1])
			HALBB_SCAN(input[i + 1], DCMD_DECIMAL, &val[i]);
	}
	switch (val[0]) {
	case 0:
		halbb_pwr_ctrl_para_init(bb);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			"Reset Parameters\n");
		break;
	case 1:
		halbb_pwr_ctrl_txpwr_cfg(bb, (u8)val[1], (u8)val[2], (u8)val[3]);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			"PWRCTRL TxPWR for each LV = {%d, %d, %d}\n",
			pwr_ctrl_i->pwr_lv_dbm[0], pwr_ctrl_i->pwr_lv_dbm[1],
			pwr_ctrl_i->pwr_lv_dbm[2]);
		break;
	case 2:
		halbb_pwr_ctrl_th_cfg(bb, (u8)val[1], (u8)val[2], (u8)val[3]);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			"PWRCTRL TH for each LV = {%d, %d, %d}\n",
			pwr_ctrl_i->set_pwr_th[0], pwr_ctrl_i->set_pwr_th[1],
			pwr_ctrl_i->set_pwr_th[2]);
		break;
	default:
		break;
	}
}

#endif


bool halbb_set_pwr_ul_tb_ofst(struct bb_info *bb, s16 pw_ofst,
			      enum phl_phy_idx phy_idx)
{
	switch (bb->ic_type) {

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		halbb_set_pwr_ul_tb_ofst_8852a_2(bb, pw_ofst, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		halbb_set_pwr_ul_tb_ofst_8852b(bb, pw_ofst, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		halbb_set_pwr_ul_tb_ofst_8852c(bb, (s8)pw_ofst, phy_idx);
		break;
	#endif

	default:
		break;
	}

	return true;
}

void halbb_macid_ctrl_init(struct bb_info *bb)
{
	u8 i = 0;
	u32 reg_ofst = 0;
	u32 ret_v = 0;

	BB_DBG(bb, DBG_PWR_CTRL, "[%s]\n", __func__);
    if (bb->hal_com == NULL) {
        BB_WARNING("hal_com is not allocated in halbb_macid_ctrl_init!!\n");
        return;
    }
	for (i = 0; i < PHL_MAX_STA_NUM; i++) {
		reg_ofst = REG_PWRMACID_OFST + (i<<2) + 0xd200;
		ret_v = rtw_hal_mac_set_pwr_reg( bb->hal_com, 0, reg_ofst, 0);
		if (bb->hal_com->dbcc_en)
			ret_v = rtw_hal_mac_set_pwr_reg( bb->hal_com, 1, reg_ofst, 0);
	}
}

void halbb_tpu_mac_cr_init(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	u32 tpu_array_type1[] = {0xD248, 0x07763333,
				 0xD220, 0x01ebf004,
				 0xD240, 0x0002f8ff};
	u32 *tpu_array = NULL;
	u8 cr_size = 0;
	u8 i = 0;
	enum phl_band_idx band;

	band = (phy_idx == HW_PHY_0) ? HW_BAND_0 : HW_BAND_1;

	switch (bb->ic_type) {

	#if defined(BB_8852A_2_SUPPORT) || defined(BB_8852B_SUPPORT)
	case BB_RTL8852A:
	case BB_RTL8852B:
		tpu_array = (u32 *)tpu_array_type1;
		cr_size = sizeof(tpu_array_type1)/sizeof(u32);
		break;
	#endif
	default:
		break;
	}

	BB_DBG(bb, DBG_INIT, "[%s] size=%d\n", __func__, cr_size >> 1);

	if (!tpu_array) {
		BB_WARNING("[%s]\n", __func__);
		return;
	}

	for (i = 0; i < cr_size; i+=2) {
		BB_DBG(bb, DBG_INIT, "0x%x = 0x%x\n", tpu_array[i], tpu_array[i+1]);
		rtw_hal_mac_set_pwr_reg(bb->hal_com, band, tpu_array[i], tpu_array[i+1]);
	}
}

void halbb_pwr_dbg(struct bb_info *bb, char input[][16], u32 *_used,
		   char *output, u32 *_out_len)
{
	struct rtw_tpu_info *tpu = &bb->hal_com->band[bb->bb_phy_idx].rtw_tpu_i;
	struct rtw_tpu_pwr_by_rate_info *by_rate = &tpu->rtw_tpu_pwr_by_rate_i;
	struct rtw_tpu_pwr_imt_info *lmt = &tpu->rtw_tpu_pwr_imt_i;
	bool rpt_tmp;
	u16 size_tmp = 0;
	u32 val[10] = {0};
	u16 i = 0, j = 0;
	u8 rate_idx = 0, path = 0;
	s8 *tmp_s8, val_s8;

	if (_os_strcmp(input[1], "-h") == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "dbg_en {en}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "show\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "set all {s(7,1) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "set lgcy {idx} {s(7,1) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "set mcs {path:0~3} {idx} {s(7,1) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "ofst bw {all:255, 0~4: 80_80/160/80/40/20} {0:+, 1:-} {u(3,1) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "ofst mode {all:255 0~4: HE/VHT/HT/Legacy/CCK} {0:+, 1:-} {u(3,1) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "ref {cck, ofdm} {s(9,2) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "ref ofst {s(8,3) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "cw {0:rf_0db_cw(39), 1:tssi_16dBm_cw(300)} {val}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "lmt en {en}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "lmt {all, ru_all} {s(7,1) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "tb_ofst {s(5,0) dB}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "tx_shap {ch} {shap_idx} {is_ofdm}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "tpu 0\n");

		return;
	}
	if (_os_strcmp(input[1], "tpu") == 0) {
		halbb_set_tx_pow_ref(bb, bb->bb_phy_idx);
		rtw_hal_mac_write_pwr_by_rate_reg(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
		rtw_hal_mac_write_pwr_limit_rua_reg(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
		rtw_hal_mac_write_pwr_limit_reg(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
		rtw_hal_mac_write_pwr_ofst_mode(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
		rtw_hal_mac_write_pwr_ofst_bw(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Set all TPU component\n");
	} else if (_os_strcmp(input[1], "dbg_en") == 0) {
		HALBB_SCAN(input[2], DCMD_HEX, &val[0]);
		rtw_hal_mac_set_tpu_mode(bb->hal_com, (enum rtw_tpu_op_mode)val[0], bb->bb_phy_idx);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "dbg_en=%d, Locking driver set TPU = %d\n", val[0], tpu->normal_mode_lock_en);
	} else if (_os_strcmp(input[1], "show") == 0) {
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "================\n\n");
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[PW Ref]\n");
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "%-10s {%d}\n", "[base_cw_0db]", tpu->base_cw_0db);

		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "%-10s {%s dB}\n", "[ref_ofst]",
			    halbb_print_sign_frac_digit2(bb, tpu->ofst_int, 8, 3));

		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "%-10s {%s dBm} pw_cw=0x%03x\n", "[CCK]",
			    halbb_print_sign_frac_digit2(bb, tpu->ref_pow_cck, 16, 2),
			    tpu->ref_pow_cck_cw);

		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "%-10s {%s dBm} pw_cw=0x%03x\n", "[OFDM]",
			    halbb_print_sign_frac_digit2(bb, tpu->ref_pow_ofdm, 16, 2),
			    tpu->ref_pow_ofdm_cw);
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "================\n\n");
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[PW Offset] (s41)\n");
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "%-20s {%02d, %02d, %02d, %02d, %02d}\n", "[AX/AC/N/G/B]",
			    tpu->pwr_ofst_mode[0], tpu->pwr_ofst_mode[1],
			    tpu->pwr_ofst_mode[2], tpu->pwr_ofst_mode[3],
			    tpu->pwr_ofst_mode[4]);
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "%-20s {%02d, %02d, %02d, %02d, %02d}\n", "[80_80/160/80/40/20]",
			    tpu->pwr_ofst_bw[0], tpu->pwr_ofst_bw[1],
			    tpu->pwr_ofst_bw[2], tpu->pwr_ofst_bw[3],
			    tpu->pwr_ofst_bw[4]);
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "================\n\n");
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[Pwr By Rate] (s71)\n");
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "%-10s {%02d, %02d, %02d, %02d}\n", "[CCK]",
			    by_rate->pwr_by_rate_lgcy[0], by_rate->pwr_by_rate_lgcy[1],
			    by_rate->pwr_by_rate_lgcy[2], by_rate->pwr_by_rate_lgcy[3]);
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "%-10s {%02d, %02d, %02d, %02d, %02d, %02d, %02d, %02d}\n","[Lgcy]",
			    by_rate->pwr_by_rate_lgcy[4], by_rate->pwr_by_rate_lgcy[5],
			    by_rate->pwr_by_rate_lgcy[6], by_rate->pwr_by_rate_lgcy[7],
			    by_rate->pwr_by_rate_lgcy[8], by_rate->pwr_by_rate_lgcy[9],
			    by_rate->pwr_by_rate_lgcy[10], by_rate->pwr_by_rate_lgcy[11]);
		for (i = 0; i < HAL_MAX_PATH; i++) {
			BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "[%d]%-7s {%02d, %02d, %02d, %02d, %02d, %02d, %02d, %02d, %02d, %02d, %02d, %02d}\n",
				    i, "[OFDM]",
				    by_rate->pwr_by_rate[i][0], by_rate->pwr_by_rate[i][1],
				    by_rate->pwr_by_rate[i][2], by_rate->pwr_by_rate[i][3],
				    by_rate->pwr_by_rate[i][4], by_rate->pwr_by_rate[i][5],
				    by_rate->pwr_by_rate[i][6], by_rate->pwr_by_rate[i][7],
				    by_rate->pwr_by_rate[i][8], by_rate->pwr_by_rate[i][9],
				    by_rate->pwr_by_rate[i][10], by_rate->pwr_by_rate[i][11]);

			BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "[%d]%-7s {%02d, %02d, %02d, %02d}\n",
				    i,"[DCM]",
				    by_rate->pwr_by_rate[i][12], by_rate->pwr_by_rate[i][13],
				    by_rate->pwr_by_rate[i][14], by_rate->pwr_by_rate[i][15]);
		}

		for (j = 0; j < TPU_SIZE_BF; j++) {
			BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "================\n\n");

			BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    	"[Pwr Lmt][%sBF]\n", (j == 0) ? "non-" : "");

			for (i = 0; i < HAL_MAX_PATH; i++) {
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d}\n", "[CCK-20M]",
					    i, lmt->pwr_lmt_cck_20m[i][j]);
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d}\n", "[CCK-40M]",
					    i, lmt->pwr_lmt_cck_40m[i][j]);
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d}\n", "[Lgcy-20M]",
					    i, lmt->pwr_lmt_lgcy_20m[i][j]);
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d, %02d, %02d, %02d, %02d, %02d, %02d, %02d}\n", "[OFDM-20M]",
					    i, lmt->pwr_lmt_20m[i][0][j], lmt->pwr_lmt_20m[i][1][j],
					    lmt->pwr_lmt_20m[i][2][j], lmt->pwr_lmt_20m[i][3][j],
					    lmt->pwr_lmt_20m[i][4][j], lmt->pwr_lmt_20m[i][5][j],
					    lmt->pwr_lmt_20m[i][6][j], lmt->pwr_lmt_20m[i][7][j]);
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d, %02d, %02d, %02d}\n", "[OFDM-40M]",
					    i, lmt->pwr_lmt_40m[i][0][j], lmt->pwr_lmt_40m[i][1][j],
					    lmt->pwr_lmt_40m[i][2][j], lmt->pwr_lmt_40m[i][3][j]);
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d, %02d}\n", "[OFDM-80M]",
					    i, lmt->pwr_lmt_80m[i][0][j], lmt->pwr_lmt_80m[i][1][j]);
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s[%d]{%02d}\n", "[OFDM-160M]",
					    i, lmt->pwr_lmt_160m[i][j]);
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d}\n", "[40m_0p5]",
					    i, lmt->pwr_lmt_40m_0p5[i][j]);
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d}\n", "[40m_2p5]",
					    i, lmt->pwr_lmt_40m_2p5[i][j]);
				    
			}
		}
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "================\n\n");
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[Pwr Lmt RUA]\n");

		for (j = 0; j < TPU_SIZE_RUA; j++) {
			BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 	    "[RU-%3d]\n", (j == 0) ? 26 : ((j == 1) ? 52 : 106));
			for (i = 0; i < HAL_MAX_PATH; i++) {
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s [%d]{%02d, %02d, %02d, %02d, %02d, %02d, %02d, %02d}\n", "[OFDM-20M]",
					    i, tpu->pwr_lmt_ru[i][j][0], tpu->pwr_lmt_ru[i][j][1],
					    tpu->pwr_lmt_ru[i][j][2], tpu->pwr_lmt_ru[i][j][3],
					    tpu->pwr_lmt_ru[i][j][4], tpu->pwr_lmt_ru[i][j][5],
					    tpu->pwr_lmt_ru[i][j][6], tpu->pwr_lmt_ru[i][j][7]);
			}
		}
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "================\n\n");
	} else if (_os_strcmp(input[1], "cw") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		HALBB_SCAN(input[3], DCMD_DECIMAL, &val[1]);
		if (val[0] == 0) {
			tpu->base_cw_0db = (u8)val[1];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "rf_cw_0dbm=%d\n", tpu->base_cw_0db);
		} else if (val[0] == 1) {
			tpu->tssi_16dBm_cw = (u16)val[1];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "tssi_16dBm_cw=%d\n", tpu->tssi_16dBm_cw);
		} else {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Set Err\n");
			return;
		}
		halbb_set_tx_pow_ref(bb, bb->bb_phy_idx);
	} else if (_os_strcmp(input[1], "ofst") == 0) {
		HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);
		HALBB_SCAN(input[4], DCMD_DECIMAL, &val[1]);
		HALBB_SCAN(input[5], DCMD_DECIMAL, &val[2]);

		if (val[1] == 0) {
			if (val[2] > 7) {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "[Set Err] max = +3.5 dB\n");
				return;
			}

			val_s8 = (s8)val[2];
		} else {
			if (val[2] > 8) {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "[Set Err] max = -4 dB\n");
				return;
			}
			val_s8 = (s8)val[2] * -1;
		}

		if (_os_strcmp(input[2], "bw") == 0) {
			if (val[0] == 255) {
				for (i = 0; i < TPU_SIZE_BW; i++)
					tpu->pwr_ofst_bw[i] = val_s8;

			} else if (val[0] >= TPU_SIZE_BW) {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "Set Err\n");
				return;
			} else {
				tpu->pwr_ofst_bw[val[0]] = val_s8;
			}
			rtw_hal_mac_write_pwr_ofst_bw(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
		} else if (_os_strcmp(input[2], "mode") == 0) {
			if (val[0] == 255) {
				for (i = 0; i < TPU_SIZE_MODE; i++)
					tpu->pwr_ofst_mode[i] = val_s8;

			} else if (val[0] >= TPU_SIZE_MODE) {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "Set Err\n");
				return;
			} else {
				tpu->pwr_ofst_mode[val[0]] = val_s8;
			}
			rtw_hal_mac_write_pwr_ofst_mode(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
		} else {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Set Err\n");
			return;
		}

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[%s]pw ofst[%d]=(%s%d.%d)dB\n",
			    (_os_strcmp(input[2], "bw") == 0) ? "BW" : "MODE",
			    val[0], (val[1] == 0) ? "+" : "-",
			    val[2] >> 1, (val[2] & 0x1) * 5);
	} else if (_os_strcmp(input[1], "ref") == 0) {
		if (_os_strcmp(input[2], "ofst") == 0) {
			HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);

			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "ref_ofst=(%s)dB\n",
				 halbb_print_sign_frac_digit2(bb, tpu->ofst_int, 8, 3));
		} else {
			HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);
			if (_os_strcmp(input[2], "ofdm") == 0) {
				tpu->ref_pow_ofdm = (s16)val[0];
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s ref_pw={%s dBm} cw=0x%09x\n", "[OFDM]",
					    halbb_print_sign_frac_digit2(bb, tpu->ref_pow_ofdm, 16, 2),
					    tpu->ref_pow_ofdm_cw);
			} else if (_os_strcmp(input[2], "cck") == 0) {
				tpu->ref_pow_cck = (s16)val[0];
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "%-10s ref_pw={%s dBm} cw=0x%09x\n", "[CCK]",
					    halbb_print_sign_frac_digit2(bb, tpu->ref_pow_cck, 16, 2),
					    tpu->ref_pow_cck_cw);
			} else {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "Set Err\n");
				return;
			}
			//halbb_print_sign_frac_digit2(bb, val[0], 32, 2);
			//BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used, "ref_pw = (%s)dBm\n", bb->dbg_buf);
		}
		halbb_set_tx_pow_ref(bb, bb->bb_phy_idx);
	} else if (_os_strcmp(input[1], "set") == 0) {
		HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);
		HALBB_SCAN(input[4], DCMD_DECIMAL, &val[1]);
		HALBB_SCAN(input[5], DCMD_DECIMAL, &val[2]);
		if (_os_strcmp(input[2], "lgcy") == 0) {
			rate_idx = (u8)val[0];
			if (rate_idx > TPU_SIZE_PWR_TAB_lGCY)
				return;

			tpu->rtw_tpu_pwr_by_rate_i.pwr_by_rate_lgcy[rate_idx] = (s8)val[1];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "[TX Pw] lgcy[%d] = (%d.%d)dBm\n", rate_idx, val[1]>>1, (val[1] & 1)*5);
		} else if (_os_strcmp(input[2], "mcs") == 0) {
			path = (u8)val[0];
			if (path > HAL_MAX_PATH)
				return;
			rate_idx = (u8)val[1];
			if (rate_idx > TPU_SIZE_PWR_TAB)
				return;

			tpu->rtw_tpu_pwr_by_rate_i.pwr_by_rate[path][rate_idx] = (s8)val[2];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "[TX Pw] Path[%d] MCS[%d] = (%d.%d)dBm\n", path, rate_idx, val[2]>>1, (val[2] & 1)*5);
		} else if (_os_strcmp(input[2], "all") == 0) {
			HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);
			for (i = 0; i < TPU_SIZE_PWR_TAB_lGCY; i++)
				tpu->rtw_tpu_pwr_by_rate_i.pwr_by_rate_lgcy[i] = (s8)val[0];

			for (i = 0; i < HAL_MAX_PATH; i++) {
				for (j = 0; j < TPU_SIZE_PWR_TAB; j++)
					tpu->rtw_tpu_pwr_by_rate_i.pwr_by_rate[i][j] = (s8)val[0];
			}
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "[TX Pw] All rate = (%d.%d)dBm\n", val[0]>>1, (val[0] & 1)*5);
			
		} else {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Set Err\n");
			return;
		}
		rtw_hal_mac_write_pwr_by_rate_reg(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
	} else if (_os_strcmp(input[1], "lmt") == 0) {
		if (_os_strcmp(input[2], "en") == 0) {
			HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);

			tpu->pwr_lmt_en = (bool)val[0];
			rtw_hal_mac_write_pwr_limit_en(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "pwr_lmt_en = %d\n", tpu->pwr_lmt_en);
		} else if (_os_strcmp(input[2], "all") == 0) {
			HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);

			size_tmp = sizeof(struct rtw_tpu_pwr_imt_info) / sizeof(s8);
			tmp_s8 = &lmt->pwr_lmt_cck_20m[0][0];

			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "pwr_lmt_size = %d\n", size_tmp);

			for (i = 0; i < size_tmp; i++) {
				*tmp_s8 = (u8)val[0];
				tmp_s8++;
			}
			rtw_hal_mac_write_pwr_limit_reg(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "Set all Pwr Lmt = (%d.%d)dBm\n", val[0]>>1, (val[0] & 1)*5);
			
		} else if (_os_strcmp(input[2], "ru_all") == 0) {
			HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);

			size_tmp = HAL_MAX_PATH * TPU_SIZE_RUA * TPU_SIZE_BW20_SC;
			tmp_s8 = &tpu->pwr_lmt_ru[0][0][0];

			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "pwr_lmt_size_ru = %d\n", size_tmp);

			for (i = 0; i < size_tmp; i++) {
				*tmp_s8 = (s8)val[0];
				tmp_s8++;
			}
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "Set all RUA Pwr Lmt = (%d.%d)dBm\n", val[0]>>1, (val[0] & 1)*5);	
			rtw_hal_mac_write_pwr_limit_rua_reg(bb->hal_com, (enum phl_band_idx)bb->bb_phy_idx);
		} else {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Set Err\n");
			return;
		}
	} else if (_os_strcmp(input[1], "tb_ofst") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);

		rpt_tmp = halbb_set_pwr_ul_tb_ofst(bb, (s16)val[0], bb->bb_phy_idx);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[ULTB Ofst]Set succcess=%d,  en = %d, pw_ofst=%d\n",
			    rpt_tmp, val[0], (s16)val[1]);
	} else if (_os_strcmp(input[1], "tx_shap") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		HALBB_SCAN(input[3], DCMD_DECIMAL, &val[1]);
		HALBB_SCAN(input[4], DCMD_DECIMAL, &val[2]);

		tpu->tx_ptrn_shap_idx = (u8)val[1];
		halbb_set_tx_pow_pattern_shap(bb, (u8)val[0], (bool)val[2], bb->bb_phy_idx);

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[Tx Shap] ch=%d, shap_idx=%d\n", val[0], tpu->tx_ptrn_shap_idx);
	} else {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Set Err\n");
	}
}
