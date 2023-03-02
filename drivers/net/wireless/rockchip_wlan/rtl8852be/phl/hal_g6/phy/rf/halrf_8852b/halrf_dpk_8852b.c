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

/*8852B DPK ver:0x9 20210713*/

void _dpk_bkup_kip_8852b(
	struct rf_info *rf,
	u32 *reg,
	u32 reg_bkup[][DPK_KIP_REG_NUM_8852B],
	u8 path)
{
	u8 i;

	for (i = 0; i < DPK_KIP_REG_NUM_8852B; i++) {
		reg_bkup[path][i] = halrf_rreg(rf, reg[i] + (path << 8), MASKDWORD);
		if (DPK_REG_DBG)
			RF_DBG(rf, DBG_RF_DPK, "[DPK] Backup 0x%x = %x\n", reg[i]+ (path << 8), reg_bkup[path][i]);
	}
}

void _dpk_bkup_bb_8852b(
	struct rf_info *rf,
	u32 *reg,
	u32 reg_bkup[DPK_BB_REG_NUM_8852B])
{
	u8 i;

	for (i = 0; i < DPK_BB_REG_NUM_8852B; i++) {
		reg_bkup[i] = halrf_rreg(rf, reg[i], MASKDWORD);
		if (DPK_REG_DBG)
			RF_DBG(rf, DBG_RF_DPK, "[DPK] Backup 0x%x = %x\n", reg[i], reg_bkup[i]);
	}
}

void _dpk_bkup_rf_8852b(
	struct rf_info *rf,
	u32 *rf_reg,
	u32 rf_bkup[][DPK_RF_REG_NUM_8852B],
	u8 path)
{
	u8 i;

	for (i = 0; i < DPK_RF_REG_NUM_8852B; i++) {
		rf_bkup[path][i] = halrf_rrf(rf, path, rf_reg[i], MASKRF);
		if (DPK_REG_DBG)
			RF_DBG(rf, DBG_RF_DPK, "[DPK] Backup RF S%d 0x%x = %x\n",
				path, rf_reg[i], rf_bkup[path][i]);
	}
}

void _dpk_reload_kip_8852b(
	struct rf_info *rf,
	u32 *reg,
	u32 reg_bkup[][DPK_KIP_REG_NUM_8852B],
	u8 path) 
{
	u8 i;

	for (i = 0; i < DPK_KIP_REG_NUM_8852B; i++) {
		halrf_wreg(rf, reg[i] + (path << 8), MASKDWORD, reg_bkup[path][i]);
		if (DPK_REG_DBG)
			RF_DBG(rf, DBG_RF_DPK, "[DPK] Reload 0x%x = %x\n", reg[i] + (path << 8),
				   reg_bkup[path][i]);
	}
}

void _dpk_reload_bb_8852b(
	struct rf_info *rf,
	u32 *reg,
	u32 reg_bkup[DPK_BB_REG_NUM_8852B]) 
{
	u8 i;

	for (i = 0; i < DPK_BB_REG_NUM_8852B; i++) {
		halrf_wreg(rf, reg[i], MASKDWORD, reg_bkup[i]);
		if (DPK_REG_DBG)
			RF_DBG(rf, DBG_RF_DPK, "[DPK] Reload 0x%x = %x\n", reg[i],
				   reg_bkup[i]);
	}
}

void _dpk_reload_rf_8852b(
	struct rf_info *rf,
	u32 *rf_reg,
	u32 rf_bkup[][DPK_RF_REG_NUM_8852B],
	u8 path)
{
	u8 i;

	for (i = 0; i < DPK_RF_REG_NUM_8852B; i++) {
		halrf_wrf(rf, path, rf_reg[i], MASKRF, rf_bkup[path][i]);
		if (DPK_REG_DBG)
			RF_DBG(rf, DBG_RF_DPK, "[DPK] Reload RF S%d 0x%x = %x\n",
				path, rf_reg[i], rf_bkup[path][i]);
	}
}

u8 _dpk_one_shot_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	enum dpk_id id)
{
	u8 r_bff8 = 0x0, phy_map;
	u16 dpk_cmd = 0x0, count = 0;

	phy_map = (BIT(phy) << 4) | BIT(path);

	dpk_cmd = (u16)((id << 8) | (0x19 + (path << 4)));

	halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_DPK, RFK_ONESHOT_START);

	halrf_wreg(rf, 0x8000, MASKDWORD, dpk_cmd);

	r_bff8 = (u8)halrf_rreg(rf, 0xbff8, MASKBYTE0);
	while (r_bff8 != 0x55 && count < 2000) {
		halrf_delay_us(rf, 10);
		r_bff8 = (u8)halrf_rreg(rf, 0xbff8, MASKBYTE0);
		count++;
	}

	halrf_wreg(rf, 0x8010, MASKBYTE0, 0x0);

	halrf_btc_rfk_ntfy(rf, phy_map, RF_BTC_DPK, RFK_ONESHOT_STOP);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] one-shot for %s = 0x%04x (count=%d)\n",
	       id == 0x06 ? "LBK_RXIQK" : (id == 0x10 ? "SYNC" :
	       (id == 0x11 ? "MDPK_IDL" : (id == 0x12 ? "MDPK_MPA" :
	        (id == 0x13 ? "GAIN_LOSS" : (id == 0x14 ? "PWR_CAL" :
		(id == 0x15 ? "DPK_RXAGC" : (id == 0x16 ? "KIP_PRESET" :
		(id == 0x17 ? "KIP_RESOTRE" : "DPK_TXAGC")))))))),
	       dpk_cmd, count);

	if (count == 2000) {
		RF_DBG(rf, DBG_RF_DPK, "[DPK] one-shot over 20ms!!!!\n");
		return 1;
	} else
		return 0;
}

void _dpk_information_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	u8 kidx = dpk->cur_idx[path];

	dpk->bp[path][kidx].band = rf->hal_com->band[phy].cur_chandef.band;
	dpk->bp[path][kidx].ch = rf->hal_com->band[phy].cur_chandef.center_ch;
	dpk->bp[path][kidx].bw = rf->hal_com->band[phy].cur_chandef.bw;

	RF_DBG(rf, DBG_RF_DPK, "[DPK] S%d[%d] (PHY%d): TSSI %s/ DBCC %s/ %s/ CH%d/ %s\n",
	       path, dpk->cur_idx[path], phy, rf->is_tssi_mode[path] ? "on" : "off",
	       rf->hal_com->dbcc_en ? "on" : "off",
	       dpk->bp[path][kidx].band == 0 ? "2G" : (dpk->bp[path][kidx].band == 1 ? "5G" : "6G"),
	       dpk->bp[path][kidx].ch,
	       dpk->bp[path][kidx].bw == 0 ? "20M" : (dpk->bp[path][kidx].bw == 1 ? "40M" : "80M"));
}

void _dpk_bb_afe_setting_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kpath)
{
	halrf_wreg(rf, 0x20fc, MASKHWORD, 0x0303);
	halrf_wreg(rf, 0x12b8, BIT(30), 0x1);
	halrf_wreg(rf, 0x32b8, BIT(30), 0x1);
	halrf_wreg(rf, 0x030c, MASKBYTE3, 0x13);
	halrf_wreg(rf, 0x032c, MASKHWORD, 0x0041);
	halrf_wreg(rf, 0x12b8, BIT(28), 0x1);
	halrf_wreg(rf, 0x58c8, BIT(24), 0x1);
	halrf_wreg(rf, 0x78c8, BIT(24), 0x1);
	halrf_wreg(rf, 0x5864, BIT(31) | BIT(30), 0x3);
	halrf_wreg(rf, 0x7864, BIT(31) | BIT(30), 0x3);
	halrf_wreg(rf, 0x2008, 0x01FFFFFF, 0x1ffffff);
	halrf_wreg(rf, 0x0c1c, BIT(2), 0x1);
	halrf_wreg(rf, 0x0700, BIT(27), 0x1);
	halrf_wreg(rf, 0x0c70, 0x000003FF, 0x3ff);
	halrf_wreg(rf, 0x0c60, BIT(1) | BIT(0), 0x3);
	halrf_wreg(rf, 0x0c6c, BIT(0), 0x1);
	halrf_wreg(rf, 0x58ac, BIT(27), 0x1);
	halrf_wreg(rf, 0x78ac, BIT(27), 0x1);
	halrf_wreg(rf, 0x0c3c, BIT(9), 0x1); /*block OFDM CCA*/
	halrf_wreg(rf, 0x2344, BIT(31), 0x1); /*block CCK CCA*/
	halrf_wreg(rf, 0x4490, BIT(31), 0x1);
	//halrf_wreg(rf, 0x12a0, BIT(14) | BIT(13) | BIT(12), 0x7);
	//halrf_wreg(rf, 0x12a0, BIT(15), 0x1);
	//halrf_wreg(rf, 0x12a0, BIT(18) | BIT(17) | BIT(16), 0x3);
	//halrf_wreg(rf, 0x12a0, BIT(19), 0x1);
	halrf_wreg(rf, 0x12a0, 0x000FF000, 0xbf); /*[19:12]*/
	//halrf_wreg(rf, 0x32a0, BIT(18) | BIT(17) | BIT(16), 0x3);
	//halrf_wreg(rf, 0x32a0, BIT(19), 0x1);
	halrf_wreg(rf, 0x32a0, BIT(19) | BIT(18) | BIT(17) | BIT(16), 0xb);
	//halrf_wreg(rf, 0x0700, BIT(24), 0x1);
	//halrf_wreg(rf, 0x0700, BIT(26) | BIT(25), 0x2);
	halrf_wreg(rf, 0x0700, BIT(26) | BIT(25) | BIT(24), 0x5);
	halrf_wreg(rf, 0x20fc, MASKHWORD, 0x3333);

	halrf_wreg(rf, 0x580c, BIT(15), 0x1); /*txbb_force_rdy*/
	halrf_wreg(rf, 0x5800, MASKLWORD, 0x0000);/*txbb_max_min*/
	halrf_wreg(rf, 0x780c, BIT(15), 0x1); /*txbb_force_rdy*/
	halrf_wreg(rf, 0x7800, MASKLWORD, 0x0000); /*txbb_max_min*/

	if (rf->hal_com->band[phy].cur_chandef.bw == 2) {/*extend ADC LPF BW*/
		halrf_wreg(rf, 0xc0d8, BIT(13), 0x1);
		halrf_wreg(rf, 0xc1d8, BIT(13), 0x1);
	}
}

void _dpk_bb_afe_restore_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kpath)
{
	halrf_wreg(rf, 0x20fc, MASKHWORD, 0x0303);
	halrf_wreg(rf, 0x12b8, BIT(30), 0x0);
	halrf_wreg(rf, 0x32b8, BIT(30), 0x0);
	halrf_wreg(rf, 0x5864, BIT(31) | BIT(30), 0x0);
	halrf_wreg(rf, 0x7864, BIT(31) | BIT(30), 0x0);
	halrf_wreg(rf, 0x2008, 0x01FFFFFF, 0x0);
	halrf_wreg(rf, 0x0c1c, BIT(2), 0x0);
	halrf_wreg(rf, 0x0700, BIT(27), 0x0);
	halrf_wreg(rf, 0x0c70, 0x000003FF, 0x63);
	halrf_wreg(rf, 0x12a0, 0x000FF000, 0x00);
	halrf_wreg(rf, 0x32a0, 0x000FF000, 0x00);
	halrf_wreg(rf, 0x0700, BIT(26) | BIT(25) | BIT(24), 0x0);
	halrf_wreg(rf, 0x5864, BIT(29), 0x0);
	halrf_wreg(rf, 0x7864, BIT(29), 0x0);
	//halrf_wreg(rf, 0x0c3c, BIT(9), 0x0); /*block OFDM CCA*/
	//halrf_wreg(rf, 0x2344, BIT(31), 0x0); /*block CCK CCA*/
	halrf_wreg(rf, 0x20fc, MASKHWORD, 0x0000);
	halrf_wreg(rf, 0x58c8, BIT(24), 0x0);
	halrf_wreg(rf, 0x78c8, BIT(24), 0x0);
	halrf_wreg(rf, 0x0c3c, BIT(9), 0x0); /*block OFDM CCA*/

	halrf_wreg(rf, 0x580c, BIT(15), 0x0); /*txbb_force_rdy*/
	halrf_wreg(rf, 0x58e4, BIT(28) | BIT(27), 0x1); /*force rst*/
	halrf_wreg(rf, 0x58e4, BIT(28) | BIT(27), 0x2); /*force rst release*/
	halrf_wreg(rf, 0x780c, BIT(15), 0x0); /*txbb_force_rdy*/
	halrf_wreg(rf, 0x78e4, BIT(28) | BIT(27), 0x1); /*force rst*/
	halrf_wreg(rf, 0x78e4, BIT(28) | BIT(27), 0x2); /*force rst release*/

	if (rf->hal_com->band[phy].cur_chandef.bw == 2) {/*extend ADC LPF BW*/
		halrf_wreg(rf, 0xc0d8, BIT(13), 0x0);
		halrf_wreg(rf, 0xc1d8, BIT(13), 0x0);
	}
}

void _dpk_tssi_pause_8852b(
	struct rf_info *rf,
	enum rf_path path,
	bool is_pause)
{
	halrf_wreg(rf, 0x5818 + (path << 13), BIT(30), is_pause);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] S%d TSSI %s\n", path,
	       is_pause ? "pause" : "resume");
}

void _dpk_tpg_sel_8852b(
	struct rf_info *rf,
	enum rf_path path,
	u8 kidx)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	if (dpk->bp[path][kidx].bw == CHANNEL_WIDTH_80)
		halrf_wreg(rf, 0x806c, BIT(2) | BIT (1), 0x0);
	else if (dpk->bp[path][kidx].bw == CHANNEL_WIDTH_40)
		halrf_wreg(rf, 0x806c, BIT(2) | BIT (1), 0x2);
	else
		halrf_wreg(rf, 0x806c, BIT(2) | BIT (1), 0x1);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] TPG Select for %s\n",
	       dpk->bp[path][kidx].bw == CHANNEL_WIDTH_80 ? "80M" : 
	       (dpk->bp[path][kidx].bw == CHANNEL_WIDTH_40 ? "40M" : "20M"));
}

void _dpk_kip_pwr_clk_on_8852b(
	struct rf_info *rf,
	enum rf_path path)
{
	/*cip power on*/
	halrf_wreg(rf, 0x8008, MASKDWORD, 0x00000080);	
	/*320M*/
	halrf_wreg(rf, 0x8088, MASKDWORD, 0x807f030a);
	halrf_wreg(rf, 0x8120 + (path << 8), MASKDWORD, 0xce000a08);

	//RF_DBG(rf, DBG_RF_DPK, "[DPK] KIP Power/CLK on\n");
}

void _dpk_kip_preset_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kidx)
{
	_dpk_tpg_sel_8852b(rf, path, kidx);
	_dpk_one_shot_8852b(rf, phy, path, KIP_PRESET);
}

void _dpk_kip_restore_8852b(
	struct rf_info *rf,
	enum rf_path path)
{
	/*cip power on*/
	halrf_wreg(rf, 0x8008, MASKDWORD, 0x00000000);
	/*CFIR CLK restore*/
	halrf_wreg(rf, 0x8088, MASKDWORD, 0x80000000);
	/*clk/en/misc*/
	//halrf_wreg(rf, 0x808c, MASKDWORD, 0x00000000);

	//halrf_wreg(rf, 0x8120 + (path << 8), MASKDWORD, 0x10010000);

	if (rf->hal_com->cv > 0x0) /*hw txagc_offset*/
		halrf_wreg(rf, 0x81c8 + (path << 8), BIT(15), 0x1);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] S%d restore KIP\n", path);
}

void _dpk_kip_set_txagc_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 txagc)
{
	halrf_wrf(rf, path, 0x10001, MASKRF, txagc);
	halrf_wreg(rf, 0x5864, BIT(29), 0x1);
	_dpk_one_shot_8852b(rf, phy, path, DPK_TXAGC);
	halrf_wreg(rf, 0x5864, BIT(29), 0x0);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] set TXAGC = 0x%x\n", txagc);
}

void _dpk_kip_set_rxagc_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path)
{
	halrf_wreg(rf, 0x8078, 0x000FFFFF, halrf_rrf(rf, path, 0x00, MASKRF)); /*copy ARFC 0x00*/
	halrf_wreg(rf, 0x5864, BIT(29), 0x1); /*enable kip control RFC*/
	_dpk_one_shot_8852b(rf, phy, path, DPK_RXAGC);
	halrf_wreg(rf, 0x5864, BIT(29), 0x0); /*disable kip control RFC*/

#if 1
	halrf_wreg(rf, 0x80d4, 0x000F0000, 0x8);
	RF_DBG(rf, DBG_RF_DPK, "[DPK] set RXBB = 0x%x (RF0x0[9:5] = 0x%x)\n",
		halrf_rreg(rf, 0x80fc, 0x0000001F),
		halrf_rrf(rf, path, 0x00, MASKRFRXBB));
#endif
}

void _dpk_read_rxsram_8852b(
	struct rf_info *rf)
{
	u32 addr;

	halrf_wreg(rf, 0x80e8, BIT(7), 0x1);	/*web_iqrx*/
	halrf_wreg(rf, 0x8074, BIT(31), 0x1);	/*rxsram_ctrl_sel*/
	halrf_wreg(rf, 0x80d4, MASKDWORD, 0x00020000);	/*rpt_sel*/

	for (addr = 0; addr < 0x200; addr++) {
		halrf_wreg(rf, 0x80d8, MASKDWORD, 0x00010000 | addr);
		RF_DBG(rf, DBG_RF_DPK, "[DPK] RXSRAM[%03d] = 0x%x\n", addr,
			halrf_rreg(rf, 0x80fc, MASKDWORD));
	}
	halrf_wreg(rf, 0x80e8, BIT(7), 0x0);	/*web_iqrx*/
	halrf_wreg(rf, 0x8074, BIT(31), 0x0);	/*rxsram_ctrl_sel*/
}

void _dpk_lbk_rxiqk_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path)
{
	u8 i, cur_rxbb;

	cur_rxbb = (u8)halrf_rrf(rf, path, 0x00, MASKRFRXBB);
	
	halrf_wreg(rf, 0x8074, BIT(31), 0x1); /*RxSRAM_ctrl_sel 0:MDPK; 1:IQK*/
	halrf_wreg(rf, 0x8124 + (path << 8), 0x0000000F, 0x0); /*[3:0] disable all rx_cfir_en*/

	/*RF setting*/
	halrf_wrf(rf, path, 0x1f, MASKRF, halrf_rrf(rf, path, 0x18, MASKRF));
	halrf_wrf(rf, path, 0x00, MASKRFMODE, 0xd);
	halrf_wrf(rf, path, 0x20, BIT(5), 0x1); /*IQKPLL_EN_BCN_A*/

	if (cur_rxbb >= 0x11)
		halrf_wrf(rf, path, 0x98, 0x0000007F, 0x13); /*[6:0] DPK_RXIQK Att*/
	else if (cur_rxbb <= 0xa)
		halrf_wrf(rf, path, 0x98, 0x0000007F, 0x00); /*[6:0] DPK_RXIQK Att*/
	else
		halrf_wrf(rf, path, 0x98, 0x0000007F, 0x05); /*[6:0] DPK_RXIQK Att*/

	halrf_wrf(rf, path, 0x85, BIT(1) | BIT(0), 0x0); /*lower LNA LDO out*/

	halrf_wrf(rf, path, 0x1e, BIT(19), 0x0);
	halrf_wrf(rf, path, 0x1e, MASKRF, 0x80014); /*POW IQKPLL, 9.25MHz offset for IQKPLL*/

	for (i = 0; i < 7; i++)
		halrf_delay_us(rf, 10); /*IQKPLL's settling time*/	

	halrf_wreg(rf, 0x5864, BIT(29), 0x1);
	halrf_wreg(rf, 0x802c, 0x0FFF0000, 0x025); /*[27:16] Rx_tone_idx=0x025 (9.25MHz)*/

	_dpk_one_shot_8852b(rf, phy, path, LBK_RXIQK);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] S%d LBK RXIQC = 0x%x\n", path, halrf_rreg(rf, 0x813c, MASKDWORD));

	halrf_wreg(rf, 0x5864, BIT(29), 0x0);

	halrf_wrf(rf, path, 0x20, BIT(5), 0x0); /*disable RXIQK PLL*/
	//halrf_wrf(rf, path, 0x1e, BIT(19), 0x0); /*POW IQKPLL*/

	halrf_wreg(rf, 0x8074, BIT(31), 0x0); /*RxSRAM_ctrl_sel 0:MDPK; 1:IQK*/
	halrf_wreg(rf, 0x80d0, BIT(21) | BIT(20), 0x0);
	halrf_wreg(rf, 0x81dc + (path << 8), BIT(1), 0x1); /*auto*/
	halrf_wrf(rf, path, 0x00, MASKRFMODE, 0x5);
}

void _dpk_get_thermal_8852b(struct rf_info *rf, u8 kidx, enum rf_path path)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	//halrf_wreg(rf, 0x80d4, 0x003F0000, 0x32);

	//RF_DBG(rf, DBG_RF_DPK, "[DPK] thermal@DPK (by KIP)= 0x%x\n", halrf_rreg(rf, 0x80fc, 0x0000003F));

	dpk->bp[path][kidx].ther_dpk = halrf_get_thermal_8852b(rf, path);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] thermal@DPK (by RFC)= 0x%x\n", dpk->bp[path][kidx].ther_dpk);
}

void _dpk_tx_mapping_8852b(
	struct rf_info *rf,
	enum rf_path path,
	u8 kidx,
	u8 txagc)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	u16 table_g[18] = {0xa043, 0xb043, 0xc043, 0xd043, 0xe043,
			0xf043, 0xa053, 0xb053, 0xc053, 0xd053,
			0xe053, 0xf053, 0x9063, 0xa063, 0xb063,
			0xc063, 0xd063, 0xe063};
	u16 table_a[18] = {0xd033, 0xe033, 0xf033, 0x3043, 0x4043,
			0x5043, 0x4053, 0x5053, 0x6053, 0x3063,
			0x4063, 0x5063, 0x6063, 0x7063, 0x8063,
			0x9063, 0xa063, 0xb063};
	u16 tx_gain = 0;

	if (dpk->bp[path][kidx].band == BAND_ON_24G)
		tx_gain = table_g[txagc - 0x2e];
	if (dpk->bp[path][kidx].band == BAND_ON_5G)
		tx_gain = table_a[txagc - 0x2e];

	halrf_wrf(rf, path, 0x11, MASKRF, tx_gain);
}

void _dpk_rf_setting_8852b(
	struct rf_info *rf,
	u8 gain,
	enum rf_path path,
	u8 kidx)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	if (dpk->bp[path][kidx].band == BAND_ON_24G) { /*2G*/
		halrf_wrf(rf, path, 0x00, MASKRF, 0x50220);
		/*att*/
		halrf_wrf(rf, path, 0x83, 0x000FF, 0xf2);
		/*TIA*/
		halrf_wrf(rf, path, 0xdf, BIT(12), 0x1);
		halrf_wrf(rf, path, 0x9e, BIT(8), 0x1);
	} else { /*5G*/
		halrf_wrf(rf, path, 0x00, MASKRF, 0x50220);
		/*switch + att*/
		halrf_wrf(rf, path, 0x8c, 0x0FE00, 0x5); /*[15:9]*/
		/*TIA*/
		halrf_wrf(rf, path, 0xdf, BIT(12), 0x1);
		halrf_wrf(rf, path, 0x9e, BIT(8), 0x1);
		/*RXCIM3*/
		halrf_wrf(rf, path, 0x8b, MASKRF, 0x920FC);
		halrf_wrf(rf, path, 0x90, MASKRF, 0x002C0);
		halrf_wrf(rf, path, 0x97, MASKRF, 0x38800);
	}
	/*debug rtxbw*/
	halrf_wrf(rf, path, 0xde, BIT(2), 0x1);
	/*txbb filter*/
	halrf_wrf(rf, path, 0x1a, BIT(14) | BIT(13) | BIT(12), dpk->bp[path][kidx].bw + 1);
	/*rxbb filter*/
	halrf_wrf(rf, path, 0x1a, BIT(11) | BIT(10), 0x0);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] ARF 0x0/0x11/0x1a = 0x%x/ 0x%x/ 0x%x\n",
	       halrf_rrf(rf, path, 0x00, MASKRF),
	       halrf_rrf(rf, path, 0x11, MASKRF),
	       halrf_rrf(rf, path, 0x1a, MASKRF));
}

void _dpk_manual_txcfir_8852b(
	struct rf_info *rf,
	enum rf_path path,
	bool is_manual)
{
	u8 tmp_pad, tmp_txbb;

	if (is_manual) {
		halrf_wreg(rf, 0x8140 + (path << 8), BIT(8), 0x1);
		/*set pad to pad_man*/
		tmp_pad = (u8)halrf_rrf(rf, path, 0x56, 0x003e0); /*[9:5]*/
		halrf_wreg(rf, 0x8144 + (path << 8), 0x0001f, tmp_pad); /*[4:0]*/

		/*set txbb to txbb_man*/
		tmp_txbb = (u8)halrf_rrf(rf, path, 0x56, 0x0001f); /*[4:0]*/
		halrf_wreg(rf, 0x8144 + (path << 8), 0x01f00, tmp_txbb); /*[12:8]*/

		/*cfir load shot*/
		halrf_wreg(rf, 0x81dc + (path << 8), BIT(1) | BIT(0), 0x1);
		halrf_wreg(rf, 0x81dc + (path << 8), BIT(1) | BIT(0), 0x0);

		halrf_wreg(rf, 0x81dc + (path << 8), BIT(1), 0x1); /*auto*/

		RF_DBG(rf, DBG_RF_DPK, "[DPK] PAD_man / TXBB_man = 0x%x / 0x%x\n",
		       tmp_pad, tmp_txbb);
	} else {
		halrf_wreg(rf, 0x8140 + (path << 8), BIT(8), 0x0);
		RF_DBG(rf, DBG_RF_DPK, "[DPK] disable manual switch TXCFIR\n");
	}
}

void _dpk_bypass_rxcfir_8852b(
	struct rf_info *rf,
	enum rf_path path,
	bool is_bypass)
{
	if (is_bypass) {
		halrf_wreg(rf, 0x813c + (path << 8), BIT(2), 0x1);
		halrf_wreg(rf, 0x813c + (path << 8), BIT(0), 0x1);
		//halrf_wreg(rf, 0x813c + (path << 8), MASKDWORD, 0x00004002);
		RF_DBG(rf, DBG_RF_DPK, "[DPK] Bypass RXIQC (0x8%d3c = 0x%x)\n",
		       1 + path, halrf_rreg(rf, 0x813c + (path << 8), MASKDWORD));
	} else {
		halrf_wreg(rf, 0x813c + (path << 8), BIT(2), 0x0);
		halrf_wreg(rf, 0x813c + (path << 8), BIT(0), 0x0);
		RF_DBG(rf, DBG_RF_DPK, "[DPK] restore 0x8%d3c = 0x%x\n",
		       1 + path, halrf_rreg(rf, 0x813c + (path << 8), MASKDWORD));
	}
}

void _dpk_table_select_8852b(
	struct rf_info *rf,
	enum rf_path path,
	u8 kidx,
	u8 gain)
{
	u8 val;

	val =  0x80 + kidx * 0x20 + gain * 0x10;

	halrf_wreg(rf, 0x81ac + (path << 8), MASKBYTE3, val);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] table select for Kidx[%d], Gain[%d] (0x%x)\n",
		kidx, gain, val);
}

bool _dpk_sync_check_8852b(
	struct rf_info *rf,
	enum rf_path path,
	u8 kidx)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	u16 dc_i, dc_q;
	u8 corr_val, corr_idx;

	halrf_wreg(rf, 0x80d4, 0x003F0000, 0x0);	/*rpt_sel*/

	corr_idx = (u8)halrf_rreg(rf, 0x80fc, 0x000000ff);
	corr_val = (u8)halrf_rreg(rf, 0x80fc, 0x0000ff00);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] S%d Corr_idx / Corr_val = %d / %d\n",
	       path, corr_idx, corr_val);

	dpk->corr_idx[path][kidx] = corr_idx;
	dpk->corr_val[path][kidx] = corr_val;

	halrf_wreg(rf, 0x80d4, 0x003F0000, 0x9);	/*rpt_sel*/

	dc_i = (u16)halrf_rreg(rf, 0x80fc, 0x0fff0000); /*[27:16]*/
	dc_q = (u16)halrf_rreg(rf, 0x80fc, 0x00000fff); /*[11:0]*/

	if (dc_i >> 11 == 1)
		dc_i = 0x1000 - dc_i;
	if (dc_q >> 11 == 1)
		dc_q = 0x1000 - dc_q;

	RF_DBG(rf, DBG_RF_DPK, "[DPK] S%d DC I/Q, = %d / %d\n", path, dc_i, dc_q);

	dpk->dc_i[path][kidx] = dc_i;
	dpk->dc_q[path][kidx] = dc_q;

	if ((dc_i > 200) || (dc_q > 200) || (corr_val < 170))
		return true;
	else
		return false;
}

bool _dpk_sync_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kidx)
{
	_dpk_one_shot_8852b(rf, phy, path, SYNC);

	return _dpk_sync_check_8852b(rf, path, kidx); /*1= fail*/
}

u16 _dpk_dgain_read_8852b(
	struct rf_info *rf)
{
	u16 dgain = 0x0;

	halrf_wreg(rf, 0x80d4, 0x003F0000, 0x0);	/*rpt_sel*/

	dgain = (u16)halrf_rreg(rf, 0x80fc, 0x0FFF0000);	/*[27:16]*/

	RF_DBG(rf, DBG_RF_DPK, "[DPK] DGain = 0x%x\n", dgain);

	return dgain;
}

s8 _dpk_dgain_mapping_8852b(
	struct rf_info *rf,
	u16 dgain)
{
	u16 bnd[15] = {0xbf1, 0xaa5, 0x97d, 0x875, 0x789,
			0x6b7, 0x5fc, 0x556, 0x4c1, 0x43d,
			0x3c7, 0x35e, 0x2ac, 0x262, 0x220};
	s8 offset = 0;

	if (dgain >= bnd[0])
		offset = 0x6;
	else if ((bnd[0] > dgain) && (dgain >= bnd[1]))
		offset = 0x6;
	else if ((bnd[1] > dgain) && (dgain >= bnd[2]))
		offset = 0x5;
	else if ((bnd[2] > dgain) && (dgain >= bnd[3]))
		offset = 0x4;
	else if ((bnd[3] > dgain) && (dgain >= bnd[4]))
		offset = 0x3;
	else if ((bnd[4] > dgain) && (dgain >= bnd[5]))
		offset = 0x2;
	else if ((bnd[5] > dgain) && (dgain >= bnd[6]))
		offset = 0x1;
	else if ((bnd[6] > dgain) && (dgain >= bnd[7]))
		offset = 0x0;
	else if ((bnd[7] > dgain) && (dgain >= bnd[8]))
		offset = 0xff;
	else if ((bnd[8] > dgain) && (dgain >= bnd[9]))
		offset = 0xfe;
	else if ((bnd[9] > dgain) && (dgain >= bnd[10]))
		offset = 0xfd;
	else if ((bnd[10] > dgain) && (dgain >= bnd[11]))
		offset = 0xfc;
	else if ((bnd[11] > dgain) && (dgain >= bnd[12]))
		offset = 0xfb;
	else if ((bnd[12] > dgain) && (dgain >= bnd[13]))
		offset = 0xfa;
	else if ((bnd[13] > dgain) && (dgain >= bnd[14]))
		offset = 0xf9;
	else if (bnd[14] > dgain)
		offset = 0xf8;
	else
		offset = 0x0;

	//RF_DBG(rf, DBG_RF_DPK, "[DPK] DGain offset = %d\n", offset);

	return offset;
}

u8 _dpk_pas_check_8852b(
	struct rf_info *rf)
{
	u8 fail = 0;

	halrf_wreg(rf, 0x80d4, MASKBYTE2, 0x06); /*0x80d6, ctrl_out_Kpack*/
	halrf_wreg(rf, 0x80bc, BIT(14), 0x0);	/*query status*/
	halrf_wreg(rf, 0x80c0, MASKBYTE2, 0x08);

	halrf_wreg(rf, 0x80c0, MASKBYTE3, 0x00); /*0x80C3*/
	if (halrf_rreg(rf, 0x80fc, MASKHWORD) == 0x0800) {
		fail = 1;
		RF_DBG(rf, DBG_RF_DPK, "[DPK] PAS check Fail!!\n");
	}
	
	return fail;
}

u8 _dpk_gainloss_read_8852b(
	struct rf_info *rf)
{
	u8 result;

	halrf_wreg(rf, 0x80d4, 0x003F0000, 0x6);	/*rpt_sel*/
	halrf_wreg(rf, 0x80bc, BIT(14), 0x1);		/*query status*/

	result = (u8)halrf_rreg(rf, 0x80fc, 0x000000F0); /*[7:4]*/

	RF_DBG(rf, DBG_RF_DPK, "[DPK] tmp GL = %d\n", result);

	return result;
}

void _dpk_gainloss_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kidx)
{
	//_dpk_table_select_8852b(rf, path, kidx, 1);

	_dpk_one_shot_8852b(rf, phy, path, GAIN_LOSS);
}

u8 _dpk_set_offset_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	s8 gain_offset)
{
	u8 txagc;

	txagc = (u8)halrf_rrf(rf, path, 0x10001, MASKRF);

	if (txagc - gain_offset < 0x2e)
		txagc = 0x2e;
	else if (txagc - gain_offset > 0x3f)
		txagc = 0x3f;
	else
		txagc = txagc - gain_offset;

	_dpk_kip_set_txagc_8852b(rf, phy, path, txagc);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] Adjust TxAGC (offset %d) = 0x%x\n", gain_offset, txagc);

	return txagc;
}

u8 _dpk_pas_read_8852b(
	struct rf_info *rf,
	u8 is_check)
{
	u8 i;
	u32 val1_i = 0, val1_q = 0, val2_i = 0, val2_q = 0;

	halrf_wreg(rf, 0x80d4, MASKBYTE2, 0x06); /*0x80d6, ctrl_out_Kpack*/
	halrf_wreg(rf, 0x80bc, BIT(14), 0x0);	/*query status*/
	halrf_wreg(rf, 0x80c0, MASKBYTE2, 0x08);

	if (is_check) {
		halrf_wreg(rf, 0x80c0, MASKBYTE3, 0x00);
		val1_i = halrf_rreg(rf, 0x80fc, MASKHWORD);
		if (val1_i >= 0x800)
			val1_i = 0x1000 - val1_i;
		val1_q = halrf_rreg(rf, 0x80fc, MASKLWORD);
		if (val1_q >= 0x800)
			val1_q = 0x1000 - val1_q;
		halrf_wreg(rf, 0x80c0, MASKBYTE3, 0x1f);
		val2_i = halrf_rreg(rf, 0x80fc, MASKHWORD);
		if (val2_i >= 0x800)
			val2_i = 0x1000 - val2_i;
		val2_q = halrf_rreg(rf, 0x80fc, MASKLWORD);
		if (val2_q >= 0x800)
			val2_q = 0x1000 - val2_q;

		RF_DBG(rf, DBG_RF_DPK, "[DPK] PAS_delta = 0x%x\n",
			(val1_i * val1_i + val1_q * val1_q) / 
			(val2_i * val2_i + val2_q * val2_q));
	} else {
		for (i = 0; i < 32; i++) {
			halrf_wreg(rf, 0x80c0, MASKBYTE3, i); /*0x80C3*/
			RF_DBG(rf, DBG_RF_DPK, "[DPK] PAS_Read[%02d]= 0x%08x\n", i,
				   halrf_rreg(rf, 0x80fc, MASKDWORD));
		}
	}

	if ((val1_i * val1_i + val1_q * val1_q) >= ((val2_i * val2_i + val2_q * val2_q) * 8 / 5))
		return 1;
	else
		return 0;
}

u8 _dpk_agc_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kidx,
	u8 init_txagc,
	u8 loss_only)
{
	u8 tmp_txagc, tmp_rxbb = 0, i = 0, tmp_gl_idx = 0;
	u8 goout = 0, agc_cnt = 0, limited_rxbb = 0;
	s8 offset = 0;
	u16 dgain = 0;

	tmp_txagc = init_txagc;
	
	do {
		switch (i) {
		case 0: /*SYNC and Dgain*/
			if (_dpk_sync_8852b(rf, phy, path, kidx) == true) {
				tmp_txagc = 0xff;
				goout = 1;
				break;
			}
	
			dgain = _dpk_dgain_read_8852b(rf);

			if (loss_only == 1 || limited_rxbb == 1)
				i = 2;
			else
				i = 1;
			break;

		case 1: /*Gain adjustment*/
			tmp_rxbb = (u8)halrf_rrf(rf, path, 0x00, MASKRFRXBB);
			offset = _dpk_dgain_mapping_8852b(rf, dgain);

			if (tmp_rxbb + offset > 0x1f) {
				tmp_rxbb = 0x1f;
				limited_rxbb = 1;
			} else if (tmp_rxbb + offset < 0) {
				tmp_rxbb = 0;
				limited_rxbb = 1;
			} else
				tmp_rxbb = tmp_rxbb + offset;

			halrf_wrf(rf, path, 0x00, MASKRFRXBB, tmp_rxbb);
			RF_DBG(rf, DBG_RF_DPK, "[DPK] Adjust RXBB (%d) = 0x%x\n", offset, tmp_rxbb);
#if 1
			if (offset != 0 || agc_cnt == 0) {
				if (rf->hal_com->band[phy].cur_chandef.bw < 2)
					_dpk_bypass_rxcfir_8852b(rf, path, true);
				else
					_dpk_lbk_rxiqk_8852b(rf, phy, path);
			}
#endif
			if ((dgain > 1922) || (dgain < 342))
				i = 0;
			else
				i = 2;

			agc_cnt++;
			break;

		case 2: /*GAIN_LOSS and idx*/
			_dpk_gainloss_8852b(rf, phy, path, kidx);
			tmp_gl_idx = _dpk_gainloss_read_8852b(rf);
			/*_dpk_pas_read_8852a(rf, false);*/

			if ((tmp_gl_idx == 0 && _dpk_pas_read_8852b(rf, true) == 1) || tmp_gl_idx >= 7)
				i = 3; /*GL > criterion*/
			else if (tmp_gl_idx == 0)
				i = 4; /*GL < criterion*/
			else 
				i = 5;
			break;

		case 3: /*GL > criterion*/
			if (tmp_txagc == 0x2e) {
				goout = 1;
				RF_DBG(rf, DBG_RF_DPK, "[DPK] Txagc@lower bound!!\n");
			} else {
				tmp_txagc = _dpk_set_offset_8852b(rf, phy, path, 0x3); /*tx gain -3*/
#if 0
				if (0x1f - tmp_rxbb > 2)
					tmp_rxbb = tmp_rxbb + 2;
				else
					tmp_rxbb = 0x1f;

				halrf_wrf(rf, path, 0x00, MASKRFRXBB, tmp_rxbb);
				RF_DBG(rf, DBG_RF_DPK, "[DPK] Adjust RXBB = 0x%x\n", tmp_rxbb);
#endif
			}
			i = 2;
			agc_cnt++;
			break;

		case 4:	/*GL < criterion*/
			if (tmp_txagc == 0x3f) {
				goout = 1;
				RF_DBG(rf, DBG_RF_DPK, "[DPK] Txagc@upper bound!!\n");
			} else {
				tmp_txagc = _dpk_set_offset_8852b(rf, phy, path, 0xfe); /*tx gain +2*/
#if 0
				if (tmp_rxbb - 2 > 0)
					tmp_rxbb = tmp_rxbb - 2;
				else
					tmp_rxbb = 0x0;

				halrf_wrf(rf, path, 0x00, MASKRFRXBB, tmp_rxbb);
				RF_DBG(rf, DBG_RF_DPK, "[DPK] Adjust RXBB = 0x%x\n", tmp_rxbb);
#endif
			}
			i = 2;
			agc_cnt++;
			break;

		case 5:	/*set tx gain for DPK*/
			tmp_txagc =_dpk_set_offset_8852b(rf, phy, path, tmp_gl_idx);
#if 0
			if (tmp_rxbb + tmp_gl_idx >= 0x1f)
				tmp_rxbb = 0x1f;
			else
				tmp_rxbb = tmp_rxbb + tmp_gl_idx;

			halrf_wrf(rf, path, 0x00, MASKRFRXBB, tmp_rxbb);
#endif
			goout = 1;
			agc_cnt++;
			break;

		default:
			goout = 1;
			break;
		}	
	} while (!goout && (agc_cnt < 6));

	RF_DBG(rf, DBG_RF_DPK, "[DPK] Txagc / RXBB for DPK = 0x%x / 0x%x\n",
		tmp_txagc, tmp_rxbb);

	return tmp_txagc;
}

void _dpk_set_mdpd_para_8852b(
	struct rf_info *rf,
	u8 order)
{
	switch (order) {
	case 0: /*(5,3,1)*/
		halrf_wreg(rf, 0x80a0, BIT(1) | BIT(0), order);
		halrf_wreg(rf, 0x80a0, 0x00001F00, 0x3); /*[12:8] phase normalize tap*/
		halrf_wreg(rf, 0x8070, 0xF0000000, 0x1); /*[31:28] tx_delay_man*/
		break;

	case 1: /*(5,3,0)*/
		halrf_wreg(rf, 0x80a0, BIT(1) | BIT(0), order);
		halrf_wreg(rf, 0x80a0, 0x00001F00, 0x0); /*[12:8] phase normalize tap*/
		halrf_wreg(rf, 0x8070, 0xF0000000, 0x0); /*[31:28] tx_delay_man*/
		break;

	case 2: /*(5,0,0)*/
		halrf_wreg(rf, 0x80a0, BIT(1) | BIT(0), order);
		halrf_wreg(rf, 0x80a0, 0x00001F00, 0x0); /*[12:8] phase normalize tap*/
		halrf_wreg(rf, 0x8070, 0xF0000000, 0x0); /*[31:28] tx_delay_man*/
		break;
	default:
		RF_DBG(rf, DBG_RF_DPK, "[DPK] Wrong MDPD order!!(0x%x)\n", order);
		break;
	}

	RF_DBG(rf, DBG_RF_DPK, "[DPK] Set %s for IDL\n", order == 0x0 ? "(5,3,1)" :
		(order == 0x1 ? "(5,3,0)" : "(5,0,0)"));
}

void _dpk_idl_mpa_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kidx,
	u8 gain)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	/*IDL*/
	if (dpk->bp[path][kidx].bw < CHANNEL_WIDTH_80 &&
	    dpk->bp[path][kidx].band == BAND_ON_5G)
		_dpk_set_mdpd_para_8852b(rf, 0x2); /*5,0,0*/
	else
		_dpk_set_mdpd_para_8852b(rf, 0x0); /*5,3,1*/
	//_dpk_table_select_8852b(rf, path, kidx, 1);

	_dpk_one_shot_8852b(rf, phy, path, MDPK_IDL);
}

u8 _dpk_order_convert_8852b(
	struct rf_info *rf)
{
	u8 val;

	val = 0x3 >> (u8)halrf_rreg(rf, 0x80a0, 0x00000003);

	/*0x80a0 [1:0] = 0x0 => 0x81bc[26:25] = 0x3   //(5,3,1)*/
	/*0x80a0 [1:0] = 0x1 => 0x81bc[26:25] = 0x1   //(5,3,0)*/
	/*0x80a0 [1:0] = 0x2 => 0x81bc[26:25] = 0x0   //(5,0,0)*/

	/*0x80a0->val : 0->3; 1->1; 2->0*/

	RF_DBG(rf, DBG_RF_DPK, "[DPK] convert MDPD order to 0x%x\n", val);

	return val;
}

u8 _dpk_pwsf_addr_cal_8852b(
	struct rf_info *rf,
	u8 t1,
	u8 t2)
{
	u8 addr;
	s8 offset;

	/*w/o TSSI : t2 = cur_thermal*/
	offset = t2 - t1;
	addr = 0x78 + (offset << 3); /*due to TPG -3dB, start from 0x78*/

	return addr;
}

void _dpk_gs_normalize_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kidx,
	u8 txagc,
	u8 gs_ori,
	u16 pwsf)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	u8 gs;
	u16 sqrt_out;

	if (dpk->bp[path][kidx].bw == 2) /*80M*/
		halrf_wreg(rf, 0x819c + (path << 8), MASKDWORD, 0x000300c0);
	else
		halrf_wreg(rf, 0x819c + (path << 8), MASKDWORD, 0x00030200);

	halrf_wreg(rf, 0x81c8 + (path << 8), 0x0000003F, txagc); /*man_txagc_vall*/
	halrf_wreg(rf, 0x81c8 + (path << 8), BIT(6), 0x1); /*man_txagc_en*/
	halrf_wreg(rf, 0x81c8 + (path << 8), MASKBYTE2, 0x08); /*man_pwsf_en*/
	halrf_wreg(rf, 0x81c8 + (path << 8), MASKBYTE1, pwsf - 24); /*man_pwsf_val*/

	_dpk_one_shot_8852b(rf, phy, path, GAIN_CAL);

	halrf_wreg(rf, 0x81d4, 0x003F0000, 0x13);	/*rpt_sel = pow_diff */
	sqrt_out = (u16)halrf_rreg(rf, 0x81fc, 0x000001FF); /*[8:0]*/

	gs = (u8)(gs_ori * sqrt_out >> 8);

	/*ch0/gain0 [6:0]*/
	halrf_wreg(rf, 0x81bc + (path << 8), 0x0000007F, gs);
	/*ch0/gain1 [14:8]*/
	halrf_wreg(rf, 0x81bc + (path << 8), 0x00007F00, gs);
	/*ch1/gain0 [6:0]*/
	//halrf_wreg(rf, 0x81c0 + (path << 8), 0x0000007F, gs);
	/*ch1/gain1 [14:8]*/
	//halrf_wreg(rf, 0x81c0 + (path << 8), 0x00007F00, gs);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] gs_ori/ sqrt_out/ gs_new = 0x%x/ %d/ 0x%x\n",
		   gs_ori, sqrt_out, gs);

}

void _dpk_fill_result_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 kidx,
	u8 gain,
	u8 txagc)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	u16 pwsf = 0x78;
	u8 gs = dpk->dpk_gs[phy];

	halrf_wreg(rf, 0x8104 + (path << 8), BIT(8), kidx);

	/*read agc*/
	//txagc = (u8)(halrf_rrf(rf, path, 0x01, MASKTXPWR) + gain_offset); /*U(6.0)*/

	/*cal pwsf*/
	//pwsf = _dpk_pwsf_addr_cal_8852b(rf, 0, 0);
#if 0
	/*read gs and normalize*/
	gs = (u8)halrf_rreg(rf, 0x81bc + path * PATH_OFST_8852B + ch * CH_OFST_8852B,
						0x7F << (gain * 8));
	gs = _dpk_gs_normalize_8852b(rf, gs, path);
#endif
	RF_DBG(rf, DBG_RF_DPK, "[DPK] Fill txagc/ pwsf/ gs = 0x%x/ 0x%x/ 0x%x\n",
		   txagc, pwsf, gs);

	/*========== txagc_rf ==========*/
	dpk->bp[path][kidx].txagc_dpk = txagc;
	halrf_wreg(rf, 0x81c4 + (path << 8), 0x3F << ((gain << 3) + (kidx << 4)), txagc);
	/*ch0/gain0 [5:0]*/
	//halrf_wreg(rf, 0x81c4 + (path << 8), 0x0000003F, txagc); /*txagc_rf*/
	//halrf_wreg(rf, 0x8190 + (path << 8), 0x000001F8, txagc); /*txagc_orig*/
	/*ch0/gain1 [13:8]*/
	//halrf_wreg(rf, 0x81c4 + (path << 8), 0x00003F00, txagc); /*txagc_rf*/
	//halrf_wreg(rf, 0x8190 + (path << 8), 0x01F80000, txagc); /*txagc_orig*/
	/*ch1/gain0 [21:16]*/
	//halrf_wreg(rf, 0x81c4 + (path << 8), 0x003F0000, txagc);
	/*ch1/gain1 [29:24]*/
	//halrf_wreg(rf, 0x81c4 + (path << 8), 0x3F000000, txagc);

	/*========== txagc_bb ==========*/
	/*ch0/gain0 [9:0]*/
	//halrf_wreg(rf, 0x81a4 + (path << 8), 0x000003FF, txagc_bb);
	/*ch0/gain1 [25:16]*/
	//halrf_wreg(rf, 0x81a4 + (path << 8), 0x03FF0000, bb_gain);
	/*ch1/gain0 [9:0]*/
	//halrf_wreg(rf, 0x81a8 + (path << 8), 0x000003FF, txagc_bb);
	/*ch1/gain1 [25:16]*/
	//halrf_wreg(rf, 0x81a8 + (path << 8), 0x03FF0000, txagc_bb);

	/*========== pwsf ==========*/
	dpk->bp[path][kidx].pwsf = pwsf;
	halrf_wreg(rf, 0x81b4 + (path << 8) + (kidx << 2), 0x1FF << (gain << 4), pwsf);
	/*ch0/gain0 [8:0]*/
	//halrf_wreg(rf, 0x81b4 + (path << 8), 0x000001FF, pwsf);
	/*ch0/gain1 [24:16]*/
	//halrf_wreg(rf, 0x81b4 + (path << 8), 0x01FF0000, pwsf);
	/*ch1/gain0 [8:0]*/
	//halrf_wreg(rf, 0x81b8 + (path << 8), 0x000001FF, pwsf);
	/*ch1/gain1 [24:16]*/
	//halrf_wreg(rf, 0x81b8 + (path << 8), 0x01FF0000, pwsf);

	/*========== road shot MDPD==========*/
	halrf_wreg(rf, 0x81dc + (path << 8), BIT(16), 0x1);
	halrf_wreg(rf, 0x81dc + (path << 8), BIT(16), 0x0);

	//halrf_wreg(rf, 0x81bc + (path << 8), MASKDWORD, 0x075b5b5b);

	/*========== gs & MDPD order ==========*/
	dpk->bp[path][kidx].gs = gs;
	if (dpk->dpk_gs[phy] == 0x7f)
		halrf_wreg(rf, 0x81bc + (path << 8) + (kidx << 2), MASKDWORD, 0x007f7f7f);
	else
		halrf_wreg(rf, 0x81bc + (path << 8) + (kidx << 2), MASKDWORD, 0x005b5b5b);

	halrf_wreg(rf, 0x81bc + (path << 8) + (kidx << 2), BIT(26) | BIT(25), _dpk_order_convert_8852b(rf));
	/*order [26:25]*/
	/*ch0*/
	//halrf_wreg(rf, 0x81bc + (path << 8), 0x06000000, _dpk_order_convert_8852b(rf));
	/*ch1*/
	//halrf_wreg(rf, 0x81c0 + (path << 8), 0x06000000, _dpk_order_convert_8852b(rf));
	//halrf_wreg(rf, 0x81bc + (path << 8), 0x007F0000, 0x5b); /*gs2 set to -3dB*/
	/*ch0/gain0 [6:0]*/
	//halrf_wreg(rf, 0x81bc + (path << 8), 0x0000007F, gs);
	/*ch0/gain1 [14:8]*/
	//halrf_wreg(rf, 0x81bc + (path << 8), 0x00007F00, gs);
	/*ch1/gain0 [6:0]*/
	//halrf_wreg(rf, 0x81c0 + (path << 8), 0x0000007F, gs);
	/*ch1/gain1 [14:8]*/
	//halrf_wreg(rf, 0x81c0 + (path << 8), 0x00007F00, gs);

	/*========== mdpd_en ==========*/
	/*ch0*/
	//halrf_wreg(rf, 0x81bc + (path << 8), BIT(24), 0x1);
	/*ch1*/
	//halrf_wreg(rf, 0x81c0 + (path << 8), BIT(24), 0x1);

	/*========== release all setting for K==========*/
	//halrf_wreg(rf, 0x81c8, MASKDWORD, 0x0);
	halrf_wreg(rf, 0x81a0 + (path << 8), MASKDWORD, 0x0);			

	halrf_wreg(rf, 0x8070, 0x80000000, 0x0); /*BIT(31)*/

	//_dpk_gs_normalize_8852b(rf, path, txagc, gs, pwsf);
}

void _dpk_coef_read_8852b(
	struct rf_info *rf,
	enum rf_path path,
	u8 kidx,
	u8 gain)
{
	u32 reg, reg_start, reg_stop;

	halrf_wreg(rf, 0x81d8 + (path << 8), MASKDWORD, 0x00010000);

	reg_start = 0x9500 + kidx * 0xa0 + path * 0x200 + gain * 0x50;

	reg_stop = reg_start + 0x50;

	RF_DBG(rf, DBG_RF_DPK, "[DPK] ===== [Coef of S%d[%d], gain%d] =====\n",
		path, kidx, gain);

	for (reg = reg_start; reg < reg_stop ; reg += 4) {
		RF_DBG(rf, DBG_RF_DPK, "[DPK][coef_r] 0x%x = 0x%08x\n", reg,
			   halrf_rreg(rf, reg, MASKDWORD));
	}
	halrf_wreg(rf, 0x81d8 + (path << 8), MASKDWORD, 0x00000000);
}

bool _dpk_reload_check_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	bool is_reload = false;
	u8 idx, cur_band, cur_ch;

	cur_band = rf->hal_com->band[phy].cur_chandef.band;
	cur_ch = rf->hal_com->band[phy].cur_chandef.center_ch;

	for (idx = 0; idx < DPK_BKUP_NUM; idx++) {
		if ((cur_band == dpk->bp[path][idx].band) && (cur_ch == dpk->bp[path][idx].ch)) {
			halrf_wreg(rf, 0x8104 + (path << 8), BIT(8), idx);
			dpk->cur_idx[path] = idx;
			is_reload = true;
			RF_DBG(rf, DBG_RF_DPK, "[DPK] reload S%d[%d] success\n", path, idx);
		}
	}

	return is_reload;
}

bool _dpk_main_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	enum rf_path path,
	u8 gain)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	u8 txagc = 0x38, kidx = dpk->cur_idx[path];
	bool is_fail = false;

	RF_DBG(rf, DBG_RF_DPK, "[DPK] ========= S%d[%d] DPK Start =========\n", path, kidx);

	halrf_rf_direct_cntrl_8852b(rf, path, false); /*switch control to direct write*/
	halrf_drf_direct_cntrl_8852b(rf, path, false); /*switch control to direct write*/

	_dpk_kip_pwr_clk_on_8852b(rf, path);
	_dpk_kip_set_txagc_8852b(rf, phy, path, txagc);
	_dpk_rf_setting_8852b(rf, gain, path, kidx);
	halrf_set_rx_dck_8852b(rf, phy, path, false);

	_dpk_kip_preset_8852b(rf, phy, path, kidx);
	_dpk_kip_set_rxagc_8852b(rf, phy, path);
	_dpk_table_select_8852b(rf, path, kidx, gain);
#if 0
	is_fail = _dpk_sync_8852b(rf, phy, path, kidx);

	if (is_fail)
		goto _error;

	_dpk_dgain_read_8852b(rf);
	_dpk_gainloss_8852b(rf, phy, path, kidx);
	//tmp_gl_idx = _dpk_gainloss_read_8852b(rf);

	txagc = _dpk_set_offset_8852b(rf, phy, path, _dpk_gainloss_read_8852b(rf));
#else
	txagc = _dpk_agc_8852b(rf, phy, path, kidx, txagc, false);

	if (txagc == 0xff) {
		is_fail = true;
		goto _error;
	}
#endif
	/*_dpk_pas_read_8852b(rf, false);*/
	_dpk_get_thermal_8852b(rf, kidx, path);

	_dpk_idl_mpa_8852b(rf, phy, path, kidx, gain);

	halrf_wrf(rf, path, 0x00, MASKRFMODE, RF_RX);
#if 0
	_dpk_coef_read_8852b(rf, path, kidx, gain);
#endif
	_dpk_fill_result_8852b(rf, phy, path, kidx, gain, txagc);
_error:

	if (is_fail == false)
		dpk->bp[path][kidx].path_ok = 1;
	else
		dpk->bp[path][kidx].path_ok = 0;

	RF_DBG(rf, DBG_RF_DPK, "[DPK] S%d[%d] DPK %s\n", path, kidx, is_fail ? "Check" : "Success");

	return is_fail;

}

void _dpk_cal_select_8852b(
	struct rf_info *rf,
	bool force,
	enum phl_phy_idx phy,
	u8 kpath)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	u32 kip_bkup[DPK_RF_PATH_MAX_8852B][DPK_KIP_REG_NUM_8852B] = {{0}};
	u32 bb_bkup[DPK_BB_REG_NUM_8852B] = {0};
	u32 rf_bkup[DPK_RF_PATH_MAX_8852B][DPK_RF_REG_NUM_8852B] = {{0}};

	u32 kip_reg[] = {0x813c, 0x8124, 0x8120};
	u32 bb_reg[] = {0x2344, 0x5800, 0x7800};
	u32 rf_reg[DPK_RF_REG_NUM_8852B] = {0xde, 0xdf, 0x8b, 0x90, 0x97,
					    0x85, 0x5, 0x10005};

	u8 path;
	bool is_fail = true, reloaded[DPK_RF_PATH_MAX_8852B] = {false};

	if (rf->phl_com->drv_mode != RTW_DRV_MODE_MP && DPK_RELOAD_EN_8852B) {
		for (path = 0; path < DPK_RF_PATH_MAX_8852B; path++) {
			reloaded[path] = _dpk_reload_check_8852b(rf, phy, path);
			if ((reloaded[path] == false) && (dpk->bp[path][0].ch != 0))
				dpk->cur_idx[path] = !dpk->cur_idx[path];
			else
				halrf_dpk_onoff_8852b(rf, path, false);
		}
	} else {
		for (path = 0; path < DPK_RF_PATH_MAX_8852B; path++)
			dpk->cur_idx[path] = 0;
	}

	_dpk_bkup_bb_8852b(rf, bb_reg, bb_bkup);

	for (path = 0; path < DPK_RF_PATH_MAX_8852B; path++) {
		_dpk_bkup_kip_8852b(rf, kip_reg, kip_bkup, path);
		_dpk_bkup_rf_8852b(rf, rf_reg, rf_bkup, path);
		_dpk_information_8852b(rf, phy, path);
		if (rf->is_tssi_mode[path])
			_dpk_tssi_pause_8852b(rf, path, true);
	}

	_dpk_bb_afe_setting_8852b(rf, phy, path, kpath);

	for (path = 0; path < DPK_RF_PATH_MAX_8852B; path++) {
		is_fail = _dpk_main_8852b(rf, phy, path, 1);
		halrf_dpk_onoff_8852b(rf, path, is_fail);
	}

	_dpk_bb_afe_restore_8852b(rf, phy, path, kpath);
	_dpk_reload_bb_8852b(rf, bb_reg, bb_bkup);

	for (path = 0; path < DPK_RF_PATH_MAX_8852B; path++) {
		_dpk_kip_restore_8852b(rf, path);
		_dpk_reload_kip_8852b(rf, kip_reg, kip_bkup, path);
		_dpk_reload_rf_8852b(rf, rf_reg, rf_bkup, path);
		if (rf->is_tssi_mode[path])
			_dpk_tssi_pause_8852b(rf, path, false);
	}
}

u8 _dpk_bypass_check_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy)
{
	struct halrf_fem_info *fem = &rf->fem;

	u8 result;

	if (fem->epa_2g && (rf->hal_com->band[phy].cur_chandef.band == BAND_ON_24G)) {
		RF_DBG(rf, DBG_RF_DPK, "[DPK] Skip DPK due to 2G_ext_PA exist!!\n");
		result = 1;
	} else if (fem->epa_5g && (rf->hal_com->band[phy].cur_chandef.band == BAND_ON_5G)) {
		RF_DBG(rf, DBG_RF_DPK, "[DPK] Skip DPK due to 5G_ext_PA exist!!\n");
		result = 1;
	} else if (fem->epa_6g && (rf->hal_com->band[phy].cur_chandef.band == BAND_ON_6G)) {
		RF_DBG(rf, DBG_RF_DPK, "[DPK] Skip DPK due to 6G_ext_PA exist!!\n");
		result = 1;
	} else
		result = 0;

	return result;
}

void _dpk_force_bypass_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy)
{
	u8 path, kpath;

	kpath = halrf_kpath_8852b(rf, phy);

	for (path = 0; path < DPK_RF_PATH_MAX_8852B; path++) {
		if (kpath & BIT(path))
			halrf_dpk_onoff_8852b(rf, path, true);
	}
}

void halrf_dpk_8852b(
	struct rf_info *rf,
	enum phl_phy_idx phy,
	bool force)
{
	RF_DBG(rf, DBG_RF_DPK, "[DPK] ****** DPK Start (Ver: 0x%x, Cv: %d, RF_para: %d) ******\n",
		DPK_VER_8852B, rf->hal_com->cv, RF_RELEASE_VERSION_8852B);

	RF_DBG(rf, DBG_RF_DPK, "[DPK] Driver mode = %d\n", rf->phl_com->drv_mode);
#if 1
	if (_dpk_bypass_check_8852b(rf, phy))
		_dpk_force_bypass_8852b(rf, phy);
	else
		_dpk_cal_select_8852b(rf, force, phy, RF_AB);
#else
	_dpk_information_8852b(rf, 0, RF_PATH_A);
	_dpk_bb_afe_setting_8852b(rf, 0, RF_PATH_A);
	
	//_dpk_main_8852b(rf, 0, 0, 1, RF_PATH_A);
	_dpk_set_tx_pwr_8852b(rf, 1, RF_PATH_A);
	_dpk_rf_setting_8852b(rf, 1, RF_PATH_A);
	halrf_set_rx_dck_8852b(rf, RF_PATH_A, false);

	_dpk_cip_setting_8852b(rf, RF_PATH_A);
	_dpk_manual_txcfir_8852b(rf, RF_PATH_A, true);
	_dpk_bypass_rxcfir_8852b(rf, RF_PATH_A, true);
	
	//_dpk_sync_8852b(rf, RF_PATH_A);
	//_dpk_dgain_read_8852b(rf);
	//_dpk_gainloss_8852b(rf, RF_PATH_A);
	_dpk_idl_mpa_8852b(rf, 0, 1, RF_PATH_A);
	_dpk_fill_result_8852b(rf, 0, 1, RF_PATH_A, 0x36);
	//halrf_dpk_onoff_8852b(rf, RF_PATH_A, 0);
	//_dpk_reload_rf_8852b(rf, rf_reg, rf_bkup, kpath);
	//halrf_rf_direct_cntrl_8852b(rf, RF_PATH_A, true);
	//_dpk_bb_afe_restore_8852b(rf, phy, RF_PATH_A);

#endif
}

void halrf_dpk_onoff_8852b(
	struct rf_info *rf,
	enum rf_path path,
	bool off)
{
	struct halrf_dpk_info *dpk = &rf->dpk;
	bool off_reverse;
	u8 val, kidx = dpk->cur_idx[path];

	if (off)
		off_reverse = false;
	else
		off_reverse = true;

	val = dpk->is_dpk_enable & off_reverse & dpk->bp[path][kidx].path_ok;

	halrf_wreg(rf, 0x81bc + (path << 8) + (kidx << 2), MASKBYTE3,
			_dpk_order_convert_8852b(rf) << 1 | val);

	//halrf_wreg(rf, 0x81bc + (path << 8), BIT(24), dpk->is_dpk_enable & (!off)); /*ch0*/
	//halrf_wreg(rf, 0x81c0 + (path << 8), BIT(24), dpk->is_dpk_enable & (!off)); /*ch1*/

	RF_DBG(rf, DBG_RF_DPK, "[DPK] S%d[%d] DPK %s !!!\n", path, kidx,
		   (dpk->is_dpk_enable & off_reverse) ? "enable" : "disable");
}

void halrf_dpk_track_8852b(
	struct rf_info *rf)
{
	struct halrf_dpk_info *dpk = &rf->dpk;

	u8 path, i, kidx;
	u8 trk_idx = 0, txagc_rf = 0;
	s8 txagc_bb = 0, txagc_bb_tp = 0, ini_diff = 0, txagc_ofst = 0;
	u16 pwsf[2];
	u8 cur_ther, ther_avg_cnt = 0;
	u32 ther_avg[2] = {0};
	s8 delta_ther[2] = {0};

	for (path = 0; path < DPK_RF_PATH_MAX_8852B; path++) {

		kidx = dpk->cur_idx[path];

		RF_DBG(rf, DBG_RF_DPK_TRACK,
		       "[DPK_TRK] ================[S%d[%d] (CH %d)]================\n",
		       path, kidx, dpk->bp[path][kidx].ch);

		//cur_ther = halrf_get_thermal_8852b(rf, path);
		cur_ther = (u8)halrf_rreg(rf, 0x1c10 + (path << 13), 0x3F000000); /*[29:24]*/

		RF_DBG(rf, DBG_RF_DPK_TRACK, "[DPK_TRK] thermal now = %d\n", cur_ther);

		dpk->ther_avg[path][dpk->ther_avg_idx] = cur_ther;

		/*Average times */
		ther_avg_cnt = 0;
		for (i = 0; i < THERMAL_DPK_AVG_NUM; i++) {
			if (dpk->ther_avg[path][i]) {
				ther_avg[path] += dpk->ther_avg[path][i];
				ther_avg_cnt++;
#if 0
				RF_DBG(rf, DBG_RF_DPK_TRACK,
				       "[DPK_TRK] thermal avg[%d] = %d\n", i,
					       dpk->thermal_dpk_avg[path][i]);
#endif
			}
		}

		/*Calculate Average ThermalValue after average enough times*/
		if (ther_avg_cnt) {
			cur_ther = (u8)(ther_avg[path] / ther_avg_cnt);
#if 0
			RF_DBG(rf, DBG_RF_DPK_TRACK,
			       "[DPK_TRK] thermal avg total = %d, avg_cnt = %d\n",
			       ther_avg[path], ther_avg_cnt);
#endif
			RF_DBG(rf, DBG_RF_DPK_TRACK,
			       "[DPK_TRK] thermal avg = %d (DPK @ %d)\n",
			       cur_ther, dpk->bp[path][kidx].ther_dpk);
		}

		if (dpk->bp[path][kidx].ch != 0 && cur_ther != 0)
			delta_ther[path] = dpk->bp[path][kidx].ther_dpk - cur_ther;

		if (dpk->bp[path][kidx].band == 0) /*2G*/
			delta_ther[path] = delta_ther[path] * 3 / 2;
		else
			delta_ther[path] = delta_ther[path] * 5 / 2;

		txagc_rf = (u8)halrf_rreg(rf, 0x1c60 + (path << 13), 0x0000003f); /*[5:0]*/

		if (rf->is_tssi_mode[path]) { /*TSSI mode*/
			trk_idx = (u8)halrf_rrf(rf, path, 0x5D, 0xFC000); /*[19:14] for integer*/

			RF_DBG(rf, DBG_RF_DPK_TRACK, "[DPK_TRK] txagc_RF / track_idx = 0x%x / %d\n",
			       txagc_rf, trk_idx);

			txagc_bb = (s8)halrf_rreg(rf, 0x1c60 + (path << 13), MASKBYTE2); /*[23:16]*/
			txagc_bb_tp = (u8)halrf_rreg(rf, 0x1c04 + (path << 13), 0x00000007); /*[2:0]*/

			RF_DBG(rf, DBG_RF_DPK_TRACK, "[DPK_TRK] txagc_bb_tp / txagc_bb = 0x%x / 0x%x\n",
			       txagc_bb_tp, txagc_bb);

			txagc_ofst = (s8)halrf_rreg(rf, 0x1c60 + (path << 13), MASKBYTE3); /*[31:24]*/


			RF_DBG(rf, DBG_RF_DPK_TRACK, "[DPK_TRK] txagc_offset / delta_ther = %d / %d\n", txagc_ofst, delta_ther[path]);

			if (halrf_rreg(rf, 0x81c8 + (path << 8), BIT(15)) == 0x1) {
				txagc_ofst = 0; /*hw txagc_offset*/
				RF_DBG(rf, DBG_RF_DPK_TRACK, "[DPK_TRK] HW txagc offset mode\n");
			}

			if (txagc_rf != 0 && cur_ther != 0)
				ini_diff = txagc_ofst + (delta_ther[path]);

			if (halrf_rreg(rf, 0x58d4 + (path << 13), 0xf0000000) == 0x0) {
				pwsf[0] = dpk->bp[path][kidx].pwsf + txagc_bb_tp - txagc_bb + ini_diff; /*gain0*/
				pwsf[1] = dpk->bp[path][kidx].pwsf + txagc_bb_tp - txagc_bb + ini_diff; /*gain1*/
			} else {
				pwsf[0] = dpk->bp[path][kidx].pwsf + ini_diff; /*gain0*/
				pwsf[1] = dpk->bp[path][kidx].pwsf + ini_diff; /*gain1*/
			}

		} else { /*without any tx power tracking mechanism*/
			pwsf[0] = (dpk->bp[path][kidx].pwsf + delta_ther[path]) & 0x1ff; /*gain0*/
			pwsf[1] = (dpk->bp[path][kidx].pwsf + delta_ther[path]) & 0x1ff; /*gain1*/
		}

		if (rf->rfk_is_processing != true && halrf_rreg(rf, 0x80f0, BIT(31)) == 0x0 && txagc_rf != 0) {
			RF_DBG(rf, DBG_RF_DPK_TRACK, "[DPK_TRK] New pwsf[0] / pwsf[1] = 0x%x / 0x%x\n",
			       pwsf[0], pwsf[1]);

			halrf_wreg(rf, 0x81b4 + (path << 8) + (kidx << 2), 0x000001FF, pwsf[0]);
			halrf_wreg(rf, 0x81b4 + (path << 8) + (kidx << 2), 0x01FF0000, pwsf[1]);
		}
	}
	dpk->ther_avg_idx++;

	if (dpk->ther_avg_idx == THERMAL_DPK_AVG_NUM)
		dpk->ther_avg_idx = 0;
}

#endif
