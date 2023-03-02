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
#include "../halrf_precomp.h"
#ifdef RF_8852B_SUPPORT
bool halrf_bw_setting_8852b(struct rf_info *rf, enum rf_path path, enum channel_width bw, bool is_dav)
{
	u32 rf_reg18 = 0;
	u32 reg_reg18_addr = 0x0;

	RF_DBG(rf, DBG_RF_RFK, "[RFK]===> %s\n", __func__);	
	if(is_dav)
		reg_reg18_addr =0x18;
	else
		reg_reg18_addr =0x10018;

	rf_reg18 = halrf_rrf(rf, path, reg_reg18_addr, MASKRF);
	/*==== [Error handling] ====*/
	if (rf_reg18 == INVALID_RF_DATA) {		
		RF_DBG(rf, DBG_RF_RFK, "[RFK]Invalid RF_0x18 for Path-%d\n", path);
		return false;
	}
	rf_reg18 &= ~(BIT(11) | BIT(10));
	/*==== [Switch bandwidth] ====*/
	switch (bw) {
	case CHANNEL_WIDTH_5:
	case CHANNEL_WIDTH_10:
	case CHANNEL_WIDTH_20:
		/*RF bandwidth */
		rf_reg18 |= (BIT(11) | BIT(10));
		break;
	case CHANNEL_WIDTH_40:
		/*RF bandwidth */
		rf_reg18 |= BIT(11);
		break;
	case CHANNEL_WIDTH_80:
		/*RF bandwidth */
		rf_reg18 |= BIT(10);
		break;
	default:
		RF_DBG(rf, DBG_RF_RFK, "[RFK]Fail to set CH\n");
	}

	/*==== [Write RF register] ====*/
	halrf_wrf(rf, path, reg_reg18_addr, MASKRF, rf_reg18);
	RF_DBG(rf, DBG_RF_RFK, "[RFK] set %x at path%d, %x =0x%x\n",bw, path, reg_reg18_addr, halrf_rrf(rf, path, reg_reg18_addr, MASKRF));
	return true;
}

bool halrf_ctrl_bw_8852b(struct rf_info *rf, enum channel_width bw)
{
	bool is_dav;
	//RF_DBG(rf, DBG_RF_RFK, "[RFK]===> %s\n", __func__);

	/*==== Error handling ====*/
	if (bw >= CHANNEL_WIDTH_MAX ) {
		RF_DBG(rf, DBG_RF_RFK,"[RFK]Fail to switch bw(bw:%d)\n", bw);
		return false;
	}

	//DAV
	is_dav = true;
	halrf_bw_setting_8852b(rf, RF_PATH_A, bw, is_dav);
	halrf_bw_setting_8852b(rf, RF_PATH_B, bw, is_dav);
	//DDV	
	is_dav = false;
	halrf_bw_setting_8852b(rf, RF_PATH_A, bw, is_dav);	
	halrf_bw_setting_8852b(rf, RF_PATH_B, bw, is_dav);

	//RF_DBG(rf, DBG_RF_RFK, "[RFK] BW: %d\n", bw);
	//RF_DBG(rf, DBG_RF_RFK, "[RFK] 0x18 = 0x%x\n",halrf_rrf(rf, RF_PATH_A, 0x18, MASKRF));

	return true;
}

bool halrf_ch_setting_8852b(struct rf_info *rf,   enum rf_path path, u8 central_ch,
			    bool *is_2g_ch, bool is_dav)
{
	u32 rf_reg18 = 0;	
	u32 reg_reg18_addr = 0x0;

	RF_DBG(rf, DBG_RF_RFK, "[RFK]===> %s\n", __func__);
	
	if(is_dav)
		reg_reg18_addr = 0x18;
	else
		reg_reg18_addr =0x10018;

	rf_reg18 = halrf_rrf(rf, path, reg_reg18_addr, MASKRF);
	/*==== [Error handling] ====*/
	if (rf_reg18 == INVALID_RF_DATA) {
		RF_DBG(rf, DBG_RF_RFK, "[RFK]Invalid RF_0x18 for Path-%d\n", path);
		return false;
	}
	*is_2g_ch = (central_ch <= 14) ? true : false;
	/*==== [Set RF Reg 0x18] ====*/
	rf_reg18 &= ~0x303ff; /*[17:16],[9:8],[7:0]*/
	rf_reg18 |= central_ch; /* Channel*/
	/*==== [5G Setting] ====*/
	if (!*is_2g_ch)
		rf_reg18 |= (BIT(16) | BIT(8));
	halrf_wrf(rf, path, reg_reg18_addr, MASKRF, rf_reg18);
	halrf_delay_us(rf, 100);
	halrf_wrf(rf, path, 0xcf, BIT(0), 0);
	halrf_wrf(rf, path, 0xcf, BIT(0), 1);
	RF_DBG(rf, DBG_RF_RFK, "[RFK]CH: %d for Path-%d, reg0x%x = 0x%x\n", central_ch, path, reg_reg18_addr, halrf_rrf(rf, path, reg_reg18_addr, MASKRF));
	return true;
}

bool halrf_ctrl_ch_8852b(struct rf_info *rf,  u8 central_ch)
{
	bool is_2g_ch;
	bool is_dav;

	//RF_DBG(rf, DBG_RF_RFK, "[RFK]===> %s\n", __func__);

	/*==== Error handling ====*/
	if ((central_ch > 14 && central_ch < 36) ||
		    (central_ch > 64 && central_ch < 100) ||
		    (central_ch > 144 && central_ch < 149) ||
		central_ch > 177 ) {
		RF_DBG(rf, DBG_RF_RFK, "[RFK]Invalid CH:%d \n", central_ch);
		return false;
	}
	//DAV
	is_dav = true;
	halrf_ch_setting_8852b(rf, RF_PATH_A, central_ch, &is_2g_ch, is_dav);
	halrf_ch_setting_8852b(rf, RF_PATH_B, central_ch, &is_2g_ch, is_dav);
	//DDV
	is_dav = false;
	halrf_ch_setting_8852b(rf, RF_PATH_A, central_ch, &is_2g_ch, is_dav);	
	halrf_ch_setting_8852b(rf, RF_PATH_B, central_ch, &is_2g_ch, is_dav);
	//RF_DBG(rf, DBG_RF_RFK, "[RFK] CH: %d\n", central_ch);
	return true;
}

void halrf_set_lo_8852b(struct rf_info *rf, bool is_on, enum rf_path path)
{
	if (is_on) {
		halrf_rf_direct_cntrl_8852b(rf, path, false);
		halrf_wrf(rf, path, 0x0, MASKRFMODE, 0x2);
		halrf_wrf(rf, path, 0x58, BIT(1), 0x1);
		halrf_wrf(rf, path, 0xde, 0x1800, 0x3);
		halrf_wrf(rf, path, 0x56, 0x1c00, 0x1);
		halrf_wrf(rf, path, 0x56, 0x1e0, 0x1);
	} else {
		halrf_wrf(rf, path, 0x58, BIT(1), 0x0);
		halrf_rf_direct_cntrl_8852b(rf, path, true);
		halrf_wrf(rf, path, 0xde, 0x1800, 0x0);
	}
}

void halrf_rf_direct_cntrl_8852b(struct rf_info *rf, enum rf_path path, bool is_bybb)
{
	if (is_bybb)
		halrf_wrf(rf, path, 0x5, BIT(0), 0x1);
	else
		halrf_wrf(rf, path, 0x5, BIT(0), 0x0);
}

void halrf_drf_direct_cntrl_8852b(struct rf_info *rf, enum rf_path path, bool is_bybb)
{
	if (is_bybb)
		halrf_wrf(rf, path, 0x10005, BIT(0), 0x1);
	else
		halrf_wrf(rf, path, 0x10005, BIT(0), 0x0);
}


void halrf_lo_test_8852b(struct rf_info *rf, bool is_on, enum rf_path path)
{
	switch (path) {
		case RF_PATH_A:
			halrf_set_lo_8852b(rf, is_on, RF_PATH_A);
			halrf_set_lo_8852b(rf, false, RF_PATH_B);
			break;
		case RF_PATH_B:
			halrf_set_lo_8852b(rf, false, RF_PATH_A);
			halrf_set_lo_8852b(rf, is_on, RF_PATH_B);
			break;
		case RF_PATH_AB:
			halrf_set_lo_8852b(rf, is_on, RF_PATH_A);
			halrf_set_lo_8852b(rf, is_on, RF_PATH_B);
			break;
		default:
			break;
	}
}

u8 halrf_kpath_8852b(struct rf_info *rf, enum phl_phy_idx phy_idx) {

	RF_DBG(rf, DBG_RF_RFK, "[RFK]dbcc_en: %x,  PHY%d\n", rf->hal_com->dbcc_en, phy_idx);

	if (!rf->hal_com->dbcc_en) {
		return RF_AB;
	} else {
		if (phy_idx == HW_PHY_0)
			return RF_A;
		else
			return RF_B;
	}
}

void _rx_dck_info_8852b(struct rf_info *rf, enum phl_phy_idx phy, enum rf_path path, bool is_afe)
{
	struct halrf_rx_dck_info *rx_dck = &rf->rx_dck;

	rx_dck->is_afe = is_afe;
	rx_dck->loc[path].cur_band = rf->hal_com->band[phy].cur_chandef.band;
	rx_dck->loc[path].cur_bw = rf->hal_com->band[phy].cur_chandef.bw;
	rx_dck->loc[path].cur_ch = rf->hal_com->band[phy].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_RXDCK, "[RX_DCK] ==== S%d RX DCK (%s / CH%d / %s / by %s)====\n", path,
		rx_dck->loc[path].cur_band == 0 ? "2G" :
		(rx_dck->loc[path].cur_band == 1 ? "5G" : "6G"),
		rx_dck->loc[path].cur_ch,
	       rx_dck->loc[path].cur_bw == 0 ? "20M" :
	       (rx_dck->loc[path].cur_bw == 1 ? "40M" : "80M"),
	       	rx_dck->is_afe ? "AFE" : "RFC");	
}

void halrf_set_rx_dck_8852b(struct rf_info *rf, enum phl_phy_idx phy, enum rf_path path, bool is_afe)
{
	u8 phy_map;
	u32 ori_val, i = 0;

	phy_map = (BIT(phy) << 4) | BIT(path);

	_rx_dck_info_8852b(rf, phy, path, is_afe);

	if (is_afe) {
		ori_val = halrf_rreg(rf, 0x12a0 + (path << 13), MASKDWORD);

		halrf_wreg(rf, 0x12b8 + (path << 13), BIT(30), 0x1); /*debug en*/
		//halrf_wreg(rf, 0x12a0 + (path << 13), BIT(19), 0x1);
		//halrf_wreg(rf, 0x12a0 + (path << 13), 0x00070000, 0x3); /*ADC 320M*/
		halrf_wreg(rf, 0x12a0 + (path << 13), 0x000F0000, 0xb);
		//halrf_wreg(rf, 0x12d8 + (path << 13), BIT(5) | BIT(4), 0x3); /*offset manual en*/
		//halrf_wreg(rf, 0x12d8 + (path << 13), BIT(7) | BIT(6), 0x3); /*avg 0:16; 1:32; 2:64; 3:128*/
		halrf_wreg(rf, 0xc0f4 + (path << 8), 0x000000F0, 0xf); /*offset manual en*/
		halrf_wreg(rf, 0x030c, 0x0f000000, 0x3); /*adc en*/
		halrf_wreg(rf, 0x032c, BIT(30), 0x0); /*adc clk*/
		halrf_wreg(rf, 0x032c, BIT(22), 0x0); /*filter reset*/
		halrf_wreg(rf, 0x032c, BIT(22), 0x1); /*filter reset release*/
		halrf_wreg(rf, 0x032c, BIT(18) | BIT(17) | BIT(16), 0x1); /*connect with RXBB*/

		halrf_wreg(rf, 0x5864, BIT(29), 0x1); /*iqk_control_sw_si*/

		halrf_wreg(rf, 0x8008, MASKDWORD, 0x00000080);
		halrf_wreg(rf, 0x8034, MASKDWORD, 0x00000020);
		halrf_wreg(rf, 0x80d4, MASKDWORD, 0x00000100);
		halrf_wreg(rf, 0x81e0, MASKDWORD, 0x02000000);
		halrf_wreg(rf, 0x82e0, MASKDWORD, 0x02100000);
		halrf_wreg(rf, 0x8000, BIT(2) | BIT(1), path); /*subpage_id*/

		halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_RXDCK, RFK_ONESHOT_START);

		halrf_wreg(rf, 0x80ac, MASKDWORD, 0xfc030000);
		halrf_wreg(rf, 0x80ac, MASKDWORD, 0x7c030000);

		halrf_wreg(rf, 0x80d4, 0x003F0000, 0x34);

		while ((halrf_rreg(rf, 0x80fc, BIT(16)) == 0x0) && (i < 500)) {
			halrf_delay_us(rf, 2);
			i++;
		}

		halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_RXDCK, RFK_ONESHOT_STOP);

		halrf_wreg(rf, 0x8008, MASKDWORD, 0x00000000);
		halrf_wreg(rf, 0x12b8 + (path << 13), BIT(30), 0x0); /*debug en*/
		halrf_wreg(rf, 0x12a0 + (path << 13), MASKDWORD, ori_val);
		halrf_wreg(rf, 0x5864, BIT(29), 0x0); /*iqk_control_sw_si*/
	
	} else {
		halrf_wrf(rf, path, 0x93, 0x0000f, 0x0); /*0: from RFC; 1: from AFE*/

		halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_RXDCK, RFK_ONESHOT_START);

		halrf_wrf(rf, path, 0x92, BIT(0), 0x0);
		halrf_wrf(rf, path, 0x92, BIT(0), 0x1);

		for (i = 0; i < 30; i++) /*delay 600us*/
			halrf_delay_us(rf, 20);

		halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_RXDCK, RFK_ONESHOT_STOP);
	}
#if 0
	RF_DBG(rf, DBG_RF_RFK, "[RX_DCK] 0x92 = 0x%x, 0x93 = 0x%x\n",
	       halrf_rrf(rf, path, 0x92, MASKRF),
	       halrf_rrf(rf, path, 0x93, MASKRF));
#endif
}

bool halrf_rx_dck_check_8852b(struct rf_info *rf, enum rf_path path)
{
	u8 addr;
	bool is_fail = false;

	if (halrf_rreg(rf, 0xc400 + path * 0x1000, 0xF0000) == 0x0)
		 return is_fail = true;
	else if (halrf_rreg(rf, 0xc400 + path * 0x1000, 0x0F000) == 0x0)
		return is_fail = true;
	else if (halrf_rreg(rf, 0xc440 + path * 0x1000, 0xF0000) == 0x0)
		return is_fail = true;
	else if (halrf_rreg(rf, 0xc440 + path * 0x1000, 0x0F000) == 0x0)
		return is_fail = true;
	else {
		for (addr = 0x0; addr < 0x20; addr++) {
			if (halrf_rreg(rf, 0xc400 + path * 0x1000 + addr * 4, 0x00FC0) == 0x0)
				return is_fail = true;
		}

		for (addr = 0x0; addr < 0x20; addr++) {
			if (halrf_rreg(rf, 0xc400 + path * 0x1000 + addr * 4, 0x0003F) == 0x0)
				return is_fail = true;
		}
	}

	return is_fail;
}

void halrf_rx_dck_8852b(struct rf_info *rf, enum phl_phy_idx phy, bool is_afe) 
{
	u8 path, dck_tune;
	u32 rf_reg5;

	RF_DBG(rf, DBG_RF_RXDCK, "[RX_DCK] ****** RXDCK Start (Ver: 0x%x, Cv: %d) ******\n",
		RXDCK_VER_8852B, rf->hal_com->cv);

	for (path = 0; path < 2; path++) {
		rf_reg5 = halrf_rrf(rf, path, 0x5, MASKRF);
		dck_tune = (u8)halrf_rrf(rf, path, 0x92, BIT(1));

		if (rf->is_tssi_mode[path]) 
			halrf_wreg(rf, 0x5818 + (path << 13), BIT(30), 0x1); /*TSSI pause*/

		halrf_wrf(rf, path, 0x5, BIT(0), 0x0);
		halrf_wrf(rf, path, 0x92, BIT(1), 0x0);
		//halrf_wrf(rf, path, 0x8f, BIT(11) | BIT(10), 0x1); /*EN_TIA_IDAC_LSB[1:0]*/
		halrf_wrf(rf, path, 0x00, MASKRFMODE, RF_RX);
		halrf_set_rx_dck_8852b(rf, phy, path, is_afe);
#if 0
		if (halrf_rx_dck_check_8852b(rf, path)) {
			RF_DBG(rf, DBG_RF_RFK, "[RX_DCK] S%d RX_DCK value = 0 happen!!!\n", path);
			halrf_wrf(rf, path, 0x8f, BIT(11) | BIT(10), 0x2); /*EN_TIA_IDAC_LSB[1:0]*/
			halrf_set_rx_dck_8852b(rf, phy, path, is_afe);
		}
#endif
		halrf_wrf(rf, path, 0x92, BIT(1), dck_tune);
		halrf_wrf(rf, path, 0x5, MASKRF, rf_reg5);

		if (rf->is_tssi_mode[path])
			halrf_wreg(rf, 0x5818 + (path << 13), BIT(30), 0x0); /*TSSI resume*/

	}
}

void halrf_rx_dck_onoff_8852b(struct rf_info *rf, bool is_enable)
{
	u8 path;

	for (path = 0; path < 2; path++) {
		halrf_wrf(rf, path, 0x93, BIT(0), !is_enable);
		if (!is_enable) {
			halrf_wrf(rf, path, 0x92, 0xFFC00, 0x220); /*[19:10]*/
			halrf_wrf(rf, path, 0x93, 0xFFC00, 0x220); /*[19:10]*/
		}
	}
}

void halrf_rck_8852b(struct rf_info *rf, enum rf_path path)
{
	u8 cnt = 0;
	u32 rf_reg5;
	u32 rck_val = 0;

	RF_DBG(rf, DBG_RF_RFK, "[RCK] ====== S%d RCK ======\n", path);

	rf_reg5 = halrf_rrf(rf, path, 0x5, MASKRF);

	halrf_wrf(rf, path, 0x5, BIT(0), 0x0);
	halrf_wrf(rf, path, 0x0, MASKRFMODE, RF_RX);
	
	RF_DBG(rf, DBG_RF_RFK, "[RCK] RF0x00 = 0x%05x\n", halrf_rrf(rf, path, 0x00, MASKRF));

	/*RCK trigger*/
	halrf_wrf(rf, path, 0x1b, MASKRF, 0x00240);

	while ((halrf_rrf(rf, path, 0x1c, BIT(3)) == 0x00) && (cnt < 10)) {
		halrf_delay_us(rf, 2);
	cnt++;
	}

	rck_val = halrf_rrf(rf, path, 0x1b, 0x07C00); /*[14:10]*/

	RF_DBG(rf, DBG_RF_RFK, "[RCK] rck_val = 0x%x, count = %d\n", rck_val, cnt);

	halrf_wrf(rf, path, 0x1b, MASKRF, rck_val);

	halrf_wrf(rf, path, 0x5, MASKRF, rf_reg5);

	RF_DBG(rf, DBG_RF_RFK, "[RCK] RF 0x1b = 0x%x\n",
	       halrf_rrf(rf, path, 0x1b, MASKRF));
}

void iqk_backup_8852b(struct rf_info *rf, enum rf_path path) 
{
	return;
}

void halrf_bf_config_rf_8852b(struct rf_info *rf)
{
	u8 i;

	for (i = 0; i < 2; i++) {
		halrf_wrf(rf, (enum rf_path)i, 0xef, BIT(19), 0x1);
		halrf_wrf(rf, (enum rf_path)i, 0x33, 0xf, 0x1);
		halrf_wrf(rf, (enum rf_path)i, 0x3e, MASKRF, 0x00001);
		halrf_wrf(rf, (enum rf_path)i, 0x3f, MASKRF, 0xb2120);
		halrf_wrf(rf, (enum rf_path)i, 0x33, 0xf, 0x2);
		halrf_wrf(rf, (enum rf_path)i, 0x3e, MASKRF, 0x00001);
		halrf_wrf(rf, (enum rf_path)i, 0x3f, MASKRF, 0xfe124);
		halrf_wrf(rf, (enum rf_path)i, 0x33, 0xf, 0x3);
		halrf_wrf(rf, (enum rf_path)i, 0x3e, MASKRF, 0x00001);
		halrf_wrf(rf, (enum rf_path)i, 0x3f, MASKRF, 0x30d7c);
		halrf_wrf(rf, (enum rf_path)i, 0x33, 0xf, 0xa);
		halrf_wrf(rf, (enum rf_path)i, 0x3e, MASKRF, 0x00001);
		halrf_wrf(rf, (enum rf_path)i, 0x3f, MASKRF, 0x30d7e);
		halrf_wrf(rf, (enum rf_path)i, 0x33, 0xf, 0xb);
		halrf_wrf(rf, (enum rf_path)i, 0x3e, MASKRF, 0x00001);
		halrf_wrf(rf, (enum rf_path)i, 0x3f, MASKRF, 0x30d7d);
		halrf_wrf(rf, (enum rf_path)i, 0xef, BIT(19), 0x0);
	}
}

void halrf_set_dpd_backoff_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct halrf_dpk_info *dpk = &rf->dpk;
	u8 tx_scale, ofdm_bkof, path, kpath;

	kpath = halrf_kpath_8852b(rf, phy);

	ofdm_bkof = (u8)halrf_rreg(rf, 0x44a0 + (phy << 13), 0x0001F000); /*[16:12]*/
	tx_scale = (u8)halrf_rreg(rf, 0x44a0 + (phy << 13), 0x0000007F); /*[6:0]*/

	if ((ofdm_bkof + tx_scale) >= 44) { /*move dpd backoff to bb, and set dpd backoff to 0*/
		dpk->dpk_gs[phy] = 0x7f;
		for (path = 0; path < DPK_RF_PATH_MAX_8852B; path++) {
			if (kpath & BIT(path)) {
				halrf_wreg(rf, 0x81bc + (path << 8), 0x007FFFFF, 0x7f7f7f); /*[22:0]*/
				RF_DBG(rf, DBG_RF_RFK, "[RFK] Set S%d DPD backoff to 0dB\n", path);
			}
		}
	} else
		dpk->dpk_gs[phy] = 0x5b;
}

void halrf_dpk_init_8852b(struct rf_info *rf)
{
	halrf_set_dpd_backoff_8852b(rf, HW_PHY_0);
}

void halrf_set_rxbb_bw_8852b(struct rf_info *rf, enum channel_width bw, enum rf_path path)
{
	halrf_wrf(rf, path, 0xee, BIT(2), 0x1);
	halrf_wrf(rf, path, 0x33, 0x0001F, 0x12); /*[4:0]*/

	if (bw == CHANNEL_WIDTH_20)
		halrf_wrf(rf, path, 0x3f, 0x0003F, 0x1b); /*[5:0]*/
	else if (bw == CHANNEL_WIDTH_40)
		halrf_wrf(rf, path, 0x3f, 0x0003F, 0x13); /*[5:0]*/
	else if (bw == CHANNEL_WIDTH_80)
		halrf_wrf(rf, path, 0x3f, 0x0003F, 0xb); /*[5:0]*/
	else
		halrf_wrf(rf, path, 0x3f, 0x0003F, 0x3); /*[5:0]*/

	RF_DBG(rf, DBG_RF_RFK, "[RFK] set S%d RXBB BW 0x3F = 0x%x\n", path,
		halrf_rrf(rf, path, 0x3f, 0x0003F));

	halrf_wrf(rf, path, 0xee, BIT(2), 0x0);
}

void halrf_rxbb_bw_8852b(struct rf_info *rf, enum phl_phy_idx phy, enum channel_width bw)
{
	u8 kpath, path;

	kpath = halrf_kpath_8852b(rf, phy);
	
	for (path = 0; path < 2; path++) {
		if ((kpath & BIT(path)) && (rf->pre_rxbb_bw[path] != bw)) {
			halrf_set_rxbb_bw_8852b(rf, bw, path);
			rf->pre_rxbb_bw[path] = bw;
		} else
			RF_DBG(rf, DBG_RF_RFK,
			       "[RFK] S%d RXBB BW unchanged (pre_bw = 0x%x)\n",
			       path, rf->pre_rxbb_bw[path]);
	}
}

void halrf_disconnect_notify_8852b(struct rf_info *rf, struct rtw_chan_def *chandef  ) {

	struct halrf_iqk_info *iqk_info = &rf->iqk;
	struct halrf_gapk_info *txgapk_info = &rf->gapk;
	u8 path, ch;
	
	RF_DBG(rf, DBG_RF_RFK, "[IQK]===>%s\n", __func__);
	/*[IQK disconnect]*/
	for (ch = 0; ch < 2; ch++) {
		for (path = 0; path < KPATH; path++) {
			if (iqk_info->iqk_mcc_ch[ch][path] == chandef->center_ch)
				iqk_info->iqk_mcc_ch[ch][path] = 0x0;
		}

	}
	/*TXGAPK*/
	for (ch = 0; ch < 2; ch++) {		
		if (txgapk_info->txgapk_mcc_ch[ch] == chandef->center_ch)
				txgapk_info->txgapk_mcc_ch[ch] = 0x0;
	}
}

bool halrf_check_mcc_ch_8852b(struct rf_info *rf, struct rtw_chan_def *chandef) {

	struct halrf_iqk_info *iqk_info = &rf->iqk;
	u8 path, ch;

	bool check = false;
	RF_DBG(rf, DBG_RF_RFK, "[IQK]===>%s, center_ch(%d)\n", __func__, chandef->center_ch);
	/*[IQK check_mcc_ch]*/
	for (ch = 0; ch < 2; ch++) {
		for (path = 0; path < KPATH; path++) {
			if (iqk_info->iqk_mcc_ch[ch][path] == chandef->center_ch) {
				check = true;
				return check;
			}
		}
	}
	return check;
}

void halrf_fw_ntfy_8852b(struct rf_info *rf, enum phl_phy_idx phy_idx) {
	struct halrf_iqk_info *iqk_info = &rf->iqk;
	u8 i = 0x0;
	u32 data_to_fw[5] = {0};
	u16 len = (u16) (sizeof(data_to_fw) / sizeof(u32))*4;
	
	data_to_fw[0] = (u32) iqk_info->iqk_mcc_ch[0][0];
	data_to_fw[1] = (u32) iqk_info->iqk_mcc_ch[0][1];
	data_to_fw[2] = (u32) iqk_info->iqk_mcc_ch[1][0];
	data_to_fw[3] = (u32) iqk_info->iqk_mcc_ch[1][1];
	data_to_fw[4] = rf->hal_com->band[phy_idx].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_RFK, "[IQK] len = 0x%x\n", len);
	for (i =0; i < 5; i++)
		RF_DBG(rf, DBG_RF_RFK, "[IQK] data_to_fw[%x] = 0x%x\n", i, data_to_fw[i]);

	halrf_fill_h2c_cmd(rf, len, FWCMD_H2C_GET_MCCCH, 0xa, H2CB_TYPE_DATA, (u32 *) data_to_fw);	

	return;
}

void halrf_before_one_shot_enable_8852b(struct rf_info *rf) {
	
	halrf_wreg(rf, 0x8010, 0x000000ff, 0x00);

	/* set 0x80d4[21:16]=0x03 (before oneshot NCTL) to get report later */
	halrf_wreg(rf, 0x80d4, 0x003F0000, 0x03);
		
	 RF_DBG(rf, DBG_RF_RFK, "======> before set one-shot bit, 0x%x= 0x%x\n", 0x8010, halrf_rreg(rf, 0x8010, MASKDWORD)); 
}


bool halrf_one_shot_nctl_done_check_8852b(struct rf_info *rf, enum rf_path path) {
	
	/* for check status */
	u32 r_bff8 = 0;
	u32 r_80fc = 0;
	bool is_ready = false;
	u16 count = 1;

	rf->nctl_ck_times[0] = 0;
	rf->nctl_ck_times[1] = 0;
	
	/* for 0xbff8 check NCTL DONE */
	while (count < 2000) {	
		r_bff8 = halrf_rreg(rf, 0xbff8, MASKBYTE0);
				
		if (r_bff8 == 0x55) {
			is_ready = true;
			break;
		}	
		halrf_delay_us(rf, 10);
		count++;
	}
	
	halrf_delay_us(rf, 1);
	/* txgapk_info->txgapk_chk_cnt[path][id][0] = count; */
	rf->nctl_ck_times[0] = count;
	
	 RF_DBG(rf, DBG_RF_RFK, "======> check 0xBFF8[7:0] = 0x%x, IsReady = %d, ReadTimes = %d,delay 1 us\n", r_bff8, is_ready, count); 

	
	/* for 0x80fc check NCTL DONE */	
	count = 1;
	is_ready = false;
	while (count < 2000) {			
		r_80fc = halrf_rreg(rf, 0x80fc, MASKLWORD);
	
		if (r_80fc == 0x8000) {
			is_ready = true;
			break;
		}	
		halrf_delay_us(rf, 1);
		count++;
	}

	halrf_delay_us(rf, 1);
	/* txgapk_info->txgapk_chk_cnt[path][id][1] = count; */
	rf->nctl_ck_times[1] = count;
		
	halrf_wreg(rf, 0x8010, 0x000000ff, 0x00);

	RF_DBG(rf, DBG_RF_RFK, "======> check 0x80fc[15:0] = 0x%x, IsReady = %d, ReadTimes = %d, 0x%x= 0x%x \n", r_80fc, is_ready, count, 0x8010, halrf_rreg(rf, 0x8010, MASKDWORD) ); 

	return is_ready;
}

static u32 check_rfc_reg[] = {0x9f, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x10000, 0x1, 0x10001,
	0x5, 0x10005, 0x8, 0x18, 0x10018,
	0x2, 0x10002, 0x11, 0x10011, 0x53,
	0x10055,	0x58, 0x63, 0x6e, 0x6f,
	0x7e, 0x7f, 0x80, 0x81, 0x8d,
	0x8f, 0x90, 0x92, 0x93, 0xa0,
	0xb2, 0xc5};
static u32 check_dack_reg[] = {0x12a0, 0x32a0, 0x12b8, 0x32b8, 0x030c,
	0x032c, 0xc000, 0xc004, 0xc020, 0xc024,
	0xc100, 0xc104, 0xc120, 0xc124, 0xc0d4,
	0xc1d4, 0xc0f0, 0xc0f4, 0xc1f0, 0xc1f4,
	0xc05c, 0xc080, 0xc048, 0xc06c, 0xc060,
	0xc084, 0xc05c, 0xc080, 0xc048, 0xc06c,
	0xc060, 0xc084};
static u32 check_iqk_reg[] = {0x8000, 0x8004, 0x8008, 0x8080, 0x808c,
	0x8120, 0x8124, 0x8138, 0x813c, 0x81dc,
	0x8220, 0x8224, 0x8238, 0x823c, 0x82dc,
	0x9fe0, 0x9fe4, 0x9fe8, 0x9fec, 0x9f30,
	0x9f40, 0x9f50, 0x9f60, 0x9f70, 0x9f80,
	0x9f90, 0x9fa0};
static u32 check_dpk_reg[] = {0x80b0, 0x81bc, 0x82bc, 0x81b4, 0x82b4,
	0x81c4, 0x82c4, 0x81c8, 0x82c8, 0x58d4,
	0x78d4};
static u32 check_tssi_reg[] = {0x0304, 0x5818, 0x581c, 0x5820, 0x1c60,
	0x1c44, 0x5838, 0x5630, 0x5634, 0x58f8,
	0x12c0, 0x120c, 0x1c04, 0x1c0c, 0x1c18,
	0x7630, 0x7634, 0x7818, 0x781c, 0x7820,
	0x3c60, 0x3c44, 0x7838, 0x78f8, 0x32c0,
	0x320c, 0x3c04, 0x3c0c, 0x3c18};

void halrf_quick_checkrf_8852b(struct rf_info *rf)
{
	u32 path, temp, i;
	u32	len = sizeof(check_rfc_reg) / sizeof(u32);
	u32	*add = (u32 *)check_rfc_reg;

	/*check RFC*/
	RF_DBG(rf, DBG_RF_CHK, "======RFC======\n");
	for (path = 0; path < 2; path++) {
		for (i = 0; i < len; i++ ) {
			temp = halrf_rrf(rf, path, add[i], MASKRF);
			RF_DBG(rf, DBG_RF_CHK, "RF%d 0x%x = 0x%x\n",
				path,
				add[i],
				temp);
			}
	}
	/*check DACK*/
	RF_DBG(rf, DBG_RF_CHK, "======DACK======\n");
	len = sizeof(check_dack_reg) / sizeof(u32);
	add = check_dack_reg;
	for (i = 0; i < len; i++ ) {
		temp = halrf_rreg(rf, add[i], MASKDWORD);
			RF_DBG(rf, DBG_RF_CHK, "0x%x = 0x%x\n",
				add[i],
				temp);
			}
	/*check IQK*/
	RF_DBG(rf, DBG_RF_CHK, "======IQK======\n");
	len = sizeof(check_iqk_reg) / sizeof(u32);
	add = check_iqk_reg;

	for (i = 0; i < len; i++ ) {
		temp = halrf_rreg(rf, add[i], MASKDWORD);
			RF_DBG(rf, DBG_RF_CHK, "0x%x = 0x%x\n",
				add[i],
				temp);
			}
	/*check DPK*/
	RF_DBG(rf, DBG_RF_CHK, "======DPK======\n");
	len = sizeof(check_dpk_reg) / sizeof(u32);
	add = check_dpk_reg;
	for (i = 0; i < len; i++ ) {
		temp = halrf_rreg(rf, add[i], MASKDWORD);
			RF_DBG(rf, DBG_RF_CHK, "0x%x = 0x%x\n",
				add[i],
				temp);
			}
	/*check TSSI*/
	RF_DBG(rf, DBG_RF_CHK, "======TSSI======\n");
	len = sizeof(check_tssi_reg) / sizeof(u32);
	add = check_tssi_reg;
	for (i = 0; i < len; i++ ) {
		temp = halrf_rreg(rf, add[i], MASKDWORD);
			RF_DBG(rf, DBG_RF_CHK, "0x%x = 0x%x\n",
				add[i],
				temp);
			}
}

static u32 backup_mac_reg_8852b[] = {0x0};
static u32 backup_bb_reg_8852b[] = {0x2344};
static u32 backup_rf_reg_8852b[] = {0xef, 0xde, 0x0, 0x1e, 0x2, 0x85, 0x90, 0x5};

#if 1
static struct halrf_iqk_ops iqk_ops= {
    .iqk_kpath = halrf_kpath_8852b,
    .iqk_mcc_page_sel = iqk_mcc_page_sel_8852b,
    .iqk_get_ch_info = iqk_get_ch_info_8852b,
    .iqk_preset = iqk_preset_8852b,
    .iqk_macbb_setting = iqk_macbb_setting_8852b,
    .iqk_start_iqk = iqk_start_iqk_8852b,
    .iqk_restore = iqk_restore_8852b,
    .iqk_afebb_restore = iqk_afebb_restore_8852b,  
};

struct rfk_iqk_info rf_iqk_hwspec_8852b = {
  	.rf_iqk_ops = &iqk_ops,
	.rf_max_path_num = 2,
	.rf_iqk_version = iqk_version_8852b,
	.rf_iqk_ch_num = 2,
	.rf_iqk_path_num = 2,

#if 0
	.backup_mac_reg = backup_mac_reg_8852b,
	.backup_mac_reg_num = ARRAY_SIZE(backup_mac_reg_8852b),
#else
	.backup_mac_reg = backup_mac_reg_8852b,
	.backup_mac_reg_num = 0,
#endif

    	.backup_bb_reg = backup_bb_reg_8852b,
    	.backup_bb_reg_num = ARRAY_SIZE(backup_bb_reg_8852b),
    	.backup_rf_reg = backup_rf_reg_8852b,
    	.backup_rf_reg_num = ARRAY_SIZE(backup_rf_reg_8852b),
};

#endif
#endif
