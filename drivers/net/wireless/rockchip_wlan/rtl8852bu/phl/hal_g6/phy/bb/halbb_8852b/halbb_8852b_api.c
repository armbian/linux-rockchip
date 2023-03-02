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
#include "../halbb_precomp.h"

#ifdef BB_8852B_SUPPORT

bool halbb_set_pwr_ul_tb_ofst_8852b(struct bb_info *bb,
				    s16 pw_ofst, enum phl_phy_idx phy_idx)
{
	/*S(5,0) for 8852A/8852B*/
	if (pw_ofst < -16 || pw_ofst > 15) {
		BB_WARNING("[%s] ofst=%d\n", __func__, pw_ofst);
		return false;
	}

	/*ECO en*/
	rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, (u8)phy_idx, 0xD288, BIT31, 1);

	/*1 TX*/
	rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, (u8)phy_idx, 0xD28c, 0x1f, pw_ofst);

	/*2 TX*/
	if (pw_ofst < -13)
		pw_ofst = -13;
	rtw_hal_mac_write_msk_pwr_reg(bb->hal_com, (u8)phy_idx, 0xD290, 0x1f, pw_ofst - 3);

	return true;
}

void halbb_tx_triangular_shap_cfg_8852b(struct bb_info *bb, u8 shape_idx,
					enum phl_phy_idx phy_idx) {

	halbb_set_reg(bb, 0x4494, 0x3000000, shape_idx);
	/*0:0dB 1:-4dB 2:-5dB 3:-6dB*/
}

void halbb_tx_dfir_shap_cck_8852b(struct bb_info *bb, u8 ch, u8 shape_idx,
				  enum phl_phy_idx phy_idx) {

	u32 para_flat[8] = {0x023D23FF, 0x0029B354, 0x000FC1C8, 0x00FDB053,
			    0x00F86F9A, 0x06FAEF92, 0x00FE5FCC, 0x00FFDFF5};
	u32 para_sharp[8] = {0x023D83FF, 0x002C636A, 0x0013F204, 0x00008090,
			     0x00F87FB0, 0x06F99F83, 0x00FDBFBA, 0x00003FF5};
	u32 para_sharp_14[8] = {0x023B13FF, 0x001C42DE, 0x00FDB0AD, 0x00F60F6E,
			        0x00FD8F92, 0x0602D011, 0x0001C02C, 0x00FFF00A};
	u32 *para = NULL;
	u8 i = 0;

	BB_DBG(bb, DBG_DBG_API, "[%s] ch=%d, shape_idx=%d\n", __func__, ch, shape_idx);

	if (ch > 14)
		return;

	if (ch == 14) {
		para = para_sharp_14;
	} else {
		if (shape_idx == 0) {
		/*flat CH1~14*/
			para = para_flat;
		} else {
		/*Sharp( b mode tx dfir)*/
			para = para_sharp;
		}
	}

	for (i = 0; i < 8; i++) {
		halbb_set_reg_cmn(bb, 0x2300 + (i << 2), MASKDWORD, para[i], phy_idx);
		BB_DBG(bb, DBG_DBG_API, "Reg0x%08x = 0x%08x\n", 0x2300 + (i << 2), para[i]);
	}
}

void halbb_bb_reset_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	// === [TSSI protect on] === //
	halbb_set_reg(bb, 0x58dc, BIT(30), 0x1);
	halbb_set_reg(bb, 0x5818, BIT(30), 0x1);
	halbb_set_reg(bb, 0x78dc, BIT(30), 0x1);
	halbb_set_reg(bb, 0x7818, BIT(30), 0x1);
	// === [BB reset] === //
	halbb_set_reg_cmn(bb, 0x704, BIT(1), 1, phy_idx);
	halbb_set_reg_cmn(bb, 0x704, BIT(1), 0, phy_idx);
	halbb_set_reg_cmn(bb, 0x704, BIT(1), 1, phy_idx);
	// === [TSSI protect off] === //
	halbb_set_reg(bb, 0x58dc, BIT(30), 0x0);
	halbb_set_reg(bb, 0x5818, BIT(30), 0x0);
	halbb_set_reg(bb, 0x78dc, BIT(30), 0x0);
	halbb_set_reg(bb, 0x7818, BIT(30), 0x0);
}

void halbb_dfs_en_8852b(struct bb_info *bb, bool en)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	if (en)
		halbb_set_reg(bb, 0x0, BIT(31), 1);
	else
		halbb_set_reg(bb, 0x0, BIT(31), 0);
}

void halbb_adc_en_8852b(struct bb_info *bb, bool en)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	if (en)
		halbb_set_reg(bb, 0x20fc, 0xff000000, 0x0);
	else
		halbb_set_reg(bb, 0x20fc, 0xff000000, 0xf);
}

void halbb_tssi_cont_en_8852b(struct bb_info *bb, bool en, enum rf_path path)
{
	u32 tssi_trk_man[2] = {0x5818, 0x7818};

	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	if (en) {
		halbb_set_reg(bb, tssi_trk_man[path], BIT(30), 0x0);
		rtw_hal_rf_tssi_scan_ch(bb->hal_com, HW_PHY_0, path);
	} else {
		halbb_set_reg(bb, tssi_trk_man[path], BIT(30), 0x1);
	}
}
void halbb_bb_reset_all_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	//Protest SW-SI 
	halbb_set_reg_cmn(bb, 0x1200, BIT(28) | BIT(29) | BIT(30), 0x7, phy_idx);
	halbb_set_reg_cmn(bb, 0x3200, BIT(28) | BIT(29) | BIT(30), 0x7, phy_idx);
	halbb_delay_us(bb, 1);
	// === [BB reset] === //
	halbb_set_reg_cmn(bb, 0x704, BIT(1), 1, phy_idx);
	halbb_set_reg_cmn(bb, 0x704, BIT(1), 0, phy_idx);
	halbb_set_reg_cmn(bb, 0x1200, BIT(28) | BIT(29) | BIT(30), 0x0, phy_idx);
	halbb_set_reg_cmn(bb, 0x3200, BIT(28) | BIT(29) | BIT(30), 0x0, phy_idx);

	halbb_set_reg_cmn(bb, 0x704, BIT(1), 1, phy_idx);
}

void halbb_bb_reset_en_8852b(struct bb_info *bb, bool en, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	if (en) {
		halbb_set_reg_cmn(bb, 0x1200, BIT(28) | BIT(29) | BIT(30), 0x0, phy_idx);
		halbb_set_reg_cmn(bb, 0x3200, BIT(28) | BIT(29) | BIT(30), 0x0, phy_idx);
		halbb_set_reg_cmn(bb, 0x704, BIT(1), 1, phy_idx);
		//PD Enable
		if(bb->hal_com->band[0].cur_chandef.band == BAND_ON_24G)
			halbb_set_reg(bb,0x2344, BIT(31), 0x0);
		halbb_set_reg(bb,0xc3c, BIT(9), 0x0);
	} else {
		//PD Disable
		halbb_set_reg(bb,0x2344, BIT(31), 0x1);
		halbb_set_reg(bb,0xc3c, BIT(9), 0x1);
		//Protest SW-SI 
		halbb_set_reg_cmn(bb, 0x1200, BIT(28) | BIT(29) | BIT(30), 0x7, phy_idx);
		halbb_set_reg_cmn(bb, 0x3200, BIT(28) | BIT(29) | BIT(30), 0x7, phy_idx);
		halbb_delay_us(bb, 1);
		halbb_set_reg_cmn(bb, 0x704, BIT(1), 0, phy_idx);
	}
}

u32 halbb_read_rf_reg_8852b_a(struct bb_info *bb, enum rf_path path,
			      u32 reg_addr, u32 bit_mask)
{
	u8 path_tmp=0;
	u32 i = 0, j = 0, readback_value = INVALID_RF_DATA, r_reg = 0;
	bool is_r_busy = true, is_w_busy = true, is_r_done = false;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	/*==== Error handling ====*/
	while (is_w_busy || is_r_busy) {
		is_w_busy = (bool)halbb_get_reg(bb, 0x174c, BIT(24));
		is_r_busy = (bool)halbb_get_reg(bb, 0x174c, BIT(25));
		halbb_delay_us(bb, 1);
		/*BB_WARNING("[%s] is_w_busy = %d, is_r_busy = %d\n",
				__func__, is_w_busy, is_r_busy);*/
		i++;
		if (i > 30)
			break;
	}
	if (is_w_busy || is_r_busy) {
		BB_WARNING("[%s] is_w_busy = (%d), is_r_busy = (%d)\n",
			   __func__, is_w_busy, is_r_busy);
		return INVALID_RF_DATA;
	}

	
	if (path > RF_PATH_B) {
		BB_WARNING("[%s] Unsupported path (%d)\n", __func__, path);
		return INVALID_RF_DATA;
	}

	/*==== Calculate offset ====*/
	path_tmp = (u8)path & 0x7;
	reg_addr &= 0xff;

	/*==== RF register only has 20bits ====*/
	bit_mask &= RFREGOFFSETMASK;

	r_reg = (path_tmp << 8 | reg_addr) & 0x7ff;
	halbb_set_reg(bb, 0x378, 0x7ff, r_reg);
	halbb_delay_us(bb, 2);

	
	/*==== Read RF register ====*/
	while (!is_r_done) {
		is_r_done = (bool)halbb_get_reg(bb, 0x174c, BIT(26));
		halbb_delay_us(bb, 1);
		j++;
		if (j > 30)
			break;
	}
	if (is_r_done) {
		readback_value = halbb_get_reg(bb, 0x174c, bit_mask);
	} else {
		BB_WARNING("[%s] is_r_done = (%d)\n", __func__, is_r_done);
		return INVALID_RF_DATA;
	}
	BB_DBG(bb, DBG_PHY_CONFIG, "A die RF-%d 0x%x = 0x%x, bit mask = 0x%x, i=%x, j =%x\n",
	       path_tmp, reg_addr, readback_value, bit_mask,i,j);
	return readback_value;
}

u32 halbb_read_rf_reg_8852b_d(struct bb_info *bb, enum rf_path path,
			      u32 reg_addr, u32 bit_mask)
{
	u32 readback_value = 0, direct_addr = 0;
	u32 offset_read_rf[2] = {0xe000, 0xf000};

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	/*==== Error handling ====*/
	if (path > RF_PATH_B) {
		BB_WARNING("[%s] Unsupported path (%d)\n", __func__, path);
		return INVALID_RF_DATA;
	}

	/*==== Calculate offset ====*/
	reg_addr &= 0xff;
	direct_addr = offset_read_rf[path] + (reg_addr << 2);

	/*==== RF register only has 20bits ====*/
	bit_mask &= RFREGOFFSETMASK;

	/*==== Read RF register directly ====*/
	readback_value = halbb_get_reg(bb, direct_addr, bit_mask);
	BB_DBG(bb, DBG_PHY_CONFIG, "D die RF-%d 0x100%x = 0x%x, bit mask = 0x%x\n",
	       path, reg_addr, readback_value, bit_mask);
	return readback_value;
}

u32 halbb_read_rf_reg_8852b(struct bb_info *bb, enum rf_path path, u32 reg_addr,
			    u32 bit_mask)
{
	u32 readback_value = INVALID_RF_DATA;
	enum rtw_dv_sel ad_sel = (enum rtw_dv_sel)((reg_addr & 0x10000) >> 16);

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	/*==== Error handling ====*/
	if (path > RF_PATH_B) {
		BB_WARNING("[%s] Unsupported path (%d)\n", __func__, path);
		return INVALID_RF_DATA;
	}

	if (ad_sel == DAV) {
		readback_value = halbb_read_rf_reg_8852b_a(bb, path, reg_addr,
				 bit_mask);
		/*BB_DBG(bb, DBG_PHY_CONFIG, "A-die RF-%d 0x%x = 0x%x, bit mask = 0x%x\n",
		       path, reg_addr, readback_value, bit_mask);*/
	} else if (ad_sel == DDV) {
		readback_value = halbb_read_rf_reg_8852b_d(bb, path, reg_addr,
				 bit_mask);
		/*BB_DBG(bb, DBG_PHY_CONFIG, "D-die RF-%d 0x%x = 0x%x, bit mask = 0x%x\n",
		       path, reg_addr, readback_value, bit_mask);*/
	} else {
		BB_DBG(bb, DBG_PHY_CONFIG, "Fail Read RF RF-%d 0x%x = 0x%x, bit mask = 0x%x\n",
		       path, reg_addr, readback_value, bit_mask);
		return INVALID_RF_DATA;
	}
	return readback_value;
}

void halbb_5m_mask_8852b(struct bb_info *bb, u8 pri_ch, enum channel_width bw,
			   enum phl_phy_idx phy_idx)
{
	bool mask_5m_low = false;
	bool mask_5m_en = false;

	switch (bw) {
		case CHANNEL_WIDTH_40:
			/* Prich=1 : Mask 5M High
			   Prich=2 : Mask 5M Low */
			mask_5m_en = true;
			mask_5m_low = pri_ch == 2 ? true : false;
			break;
		case CHANNEL_WIDTH_80:
			/* Prich=3 : Mask 5M High
			   Prich=4 : Mask 5M Low
			   Else    : Mask 5M Disable */
			mask_5m_en = ((pri_ch == 3) || (pri_ch == 4)) ? true : false;
			mask_5m_low = pri_ch == 4 ? true : false;
			break;
		default:
			mask_5m_en = false;
			break;
	}

	BB_DBG(bb, DBG_PHY_CONFIG, "[5M Mask] pri_ch = %d, bw = %d", pri_ch, bw);

	if (!mask_5m_en) {
		halbb_set_reg(bb, 0x46f8, BIT(12), 0x0);
		halbb_set_reg(bb, 0x47b8, BIT(12), 0x0);
		halbb_set_reg_cmn(bb, 0x4440, BIT(31), 0x0, phy_idx);
	} else {
		if (mask_5m_low) {
			halbb_set_reg(bb, 0x46f8, 0x3f, 0x4);
			halbb_set_reg(bb, 0x46f8, BIT(12), 0x1);
			halbb_set_reg(bb, 0x46f8, BIT(8), 0x0);
			halbb_set_reg(bb, 0x46f8, BIT(6), 0x1);
			halbb_set_reg(bb, 0x47b8, 0x3f, 0x4);
			halbb_set_reg(bb, 0x47b8, BIT(12), 0x1);
			halbb_set_reg(bb, 0x47b8, BIT(8), 0x0);
			halbb_set_reg(bb, 0x47b8, BIT(6), 0x1);
		} else {
			halbb_set_reg(bb, 0x46f8, 0x3f, 0x4);
			halbb_set_reg(bb, 0x46f8, BIT(12), 0x1);
			halbb_set_reg(bb, 0x46f8, BIT(8), 0x1);
			halbb_set_reg(bb, 0x46f8, BIT(6), 0x0);
			halbb_set_reg(bb, 0x47b8, 0x3f, 0x4);
			halbb_set_reg(bb, 0x47b8, BIT(12), 0x1);
			halbb_set_reg(bb, 0x47b8, BIT(8), 0x1);
			halbb_set_reg(bb, 0x47b8, BIT(6), 0x0);
		}
		halbb_set_reg_cmn(bb, 0x4440, BIT(31), 0x1, phy_idx);
	}
}

u8 halbb_sco_mapping_8852b(struct bb_info *bb,  u8 central_ch)
{
	/*=== SCO compensate : (BIT(0) << 18) / central_ch ===*/
	if (central_ch == 1)
		/*=== 2G ===*/
		return 109;
	else if (central_ch >= 2 && central_ch <= 6)
		return 108;
	else if (central_ch >= 7 && central_ch <= 10)
		return 107;
	else if (central_ch >= 11 && central_ch <= 14)
		return 106;
	else if (central_ch == 36 || central_ch == 38)
		return 51;
	else if (central_ch >= 40 && central_ch <= 58)
		return 50;
	else if (central_ch >= 60 && central_ch <= 64)
		return 49;
	else if (central_ch == 100 || central_ch == 102)
		return 48;
	else if (central_ch >= 104 && central_ch <= 126)
		return 47;
	else if (central_ch >= 128 && central_ch <= 151)
		return 46;
	else if (central_ch >= 153 && central_ch <= 177)
		return 45;
	else
		return 0;
}

bool halbb_ctrl_sco_cck_8852b(struct bb_info *bb, u8 pri_ch)
{
	u32 sco_barker_threshold[14] = {0x1cfea, 0x1d0e1, 0x1d1d7, 0x1d2cd,
					0x1d3c3, 0x1d4b9, 0x1d5b0, 0x1d6a6,
					0x1d79c, 0x1d892, 0x1d988, 0x1da7f,
					0x1db75, 0x1ddc4};
	u32 sco_cck_threshold[14] = {0x27de3, 0x27f35, 0x28088, 0x281da,
				     0x2832d, 0x2847f, 0x285d2, 0x28724,
				     0x28877, 0x289c9, 0x28b1c, 0x28c6e,
				     0x28dc1, 0x290ed};

	if (pri_ch > 14) {
		BB_DBG(bb, DBG_PHY_CONFIG, "[CCK SCO Fail]");
		return false;
	}

	halbb_set_reg(bb, 0x23b0, 0x7ffff, sco_barker_threshold[pri_ch - 1]);
	halbb_set_reg(bb, 0x23b4, 0x7ffff, sco_cck_threshold[pri_ch - 1]);
	BB_DBG(bb, DBG_PHY_CONFIG, "[CCK SCO Success]");
	return true;
}

bool halbb_write_rf_reg_8852b_a(struct bb_info *bb, enum rf_path path,
				u32 reg_addr, u32 bit_mask, u32 data)
{
	u8 path_tmp = 0, b_msk_en = 0, bit_shift = 0;
	u32 i =0, w_reg = 0;
	bool is_r_busy = true, is_w_busy = true;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	/*==== Error handling ====*/
	while (is_w_busy || is_r_busy) {
		is_w_busy = (bool)halbb_get_reg(bb, 0x174c, BIT(24));
		is_r_busy = (bool)halbb_get_reg(bb, 0x174c, BIT(25));
		halbb_delay_us(bb, 1);
		/*BB_WARNING("[%s] is_w_busy = %d, is_r_busy = %d\n",
				__func__, is_w_busy, is_r_busy);*/
		i++;
		if (i > 30)
			break;
	}
	if (is_w_busy || is_r_busy) {
		BB_WARNING("[%s] is_w_busy = (%d), is_r_busy = (%d)\n",
			   __func__, is_w_busy, is_r_busy);
		return false;
	}
	if (path > RF_PATH_B) {
		BB_WARNING("[%s] Unsupported path (%d)\n", __func__, path);
		return false;
	}

	/*==== Calculate offset ====*/
	path_tmp = (u8)path & 0x7;
	reg_addr &= 0xff;
	
	/*==== RF register only has 20bits ====*/
	data &= RFREGOFFSETMASK;
	bit_mask &= RFREGOFFSETMASK;

	/*==== Check if mask needed  ====*/
	if (bit_mask != RFREGOFFSETMASK) {
		b_msk_en = 1;
		halbb_set_reg(bb, 0x374, RFREGOFFSETMASK, bit_mask);
		for (bit_shift = 0; bit_shift <= 19; bit_shift++) {
			if ((bit_mask >> bit_shift) & 0x1)
				break;
		}
		data = (data << bit_shift) & RFREGOFFSETMASK;
	}

	w_reg = b_msk_en << 31 | path_tmp << 28 | reg_addr << 20 | data;

	/*==== Write RF register  ====*/
	halbb_set_reg(bb, 0x370, MASKDWORD, w_reg);
	//halbb_delay_us(bb, 5);

	BB_DBG(bb, DBG_PHY_CONFIG, "A die RF-%d 0x%x = 0x%x , bit mask = 0x%x, i=%x\n",
	       path_tmp, reg_addr, data, bit_mask,i);

	return true;
}

bool halbb_write_rf_reg_8852b_d(struct bb_info *bb, enum rf_path path,
				u32 reg_addr, u32 bit_mask, u32 data)
{
	u32 direct_addr = 0;
	u32 offset_write_rf[2] = {0xe000, 0xf000};

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	/*==== Error handling ====*/
	if (path > RF_PATH_B) {
		BB_WARNING("[%s] Unsupported path (%d)\n", __func__, path);
		return false;
	}

	/*==== Calculate offset ====*/
	reg_addr &= 0xff;
	direct_addr = offset_write_rf[path] + (reg_addr << 2);

	/*==== RF register only has 20bits ====*/
	bit_mask &= RFREGOFFSETMASK;

	/*==== Write RF register directly ====*/
	halbb_set_reg(bb, direct_addr, bit_mask, data);

	halbb_delay_us(bb, 1);

	BB_DBG(bb, DBG_PHY_CONFIG, "D die RF-%d 0x%x = 0x%x , bit mask = 0x%x\n",
	       path, reg_addr, data, bit_mask);

	return true;
}

bool halbb_write_rf_reg_8852b(struct bb_info *bb, enum rf_path path,
			      u32 reg_addr, u32 bit_mask, u32 data)
{
	u8 path_tmp = 0, b_msk_en = 0;
	u32 w_reg = 0;
	bool rpt = true;
	enum rtw_dv_sel ad_sel = (enum rtw_dv_sel)((reg_addr & 0x10000) >> 16);

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	/*==== Error handling ====*/
	if (path > RF_PATH_B) {
		BB_WARNING("[%s] Unsupported path (%d)\n", __func__, path);
		return false;
	}

	if (ad_sel == DAV) {
		rpt = halbb_write_rf_reg_8852b_a(bb, path, reg_addr, bit_mask,
		      data);
		/*BB_DBG(bb, DBG_PHY_CONFIG, "A-die RF-%d 0x%x = 0x%x , bit mask = 0x%x\n",
		       path, reg_addr, data, bit_mask);*/
	} else if (ad_sel == DDV) {
		rpt = halbb_write_rf_reg_8852b_d(bb, path, reg_addr, bit_mask,
		      data);
		/*BB_DBG(bb, DBG_PHY_CONFIG, "D-die RF-%d 0x%x = 0x%x , bit mask = 0x%x\n",
		       path, reg_addr, data, bit_mask);*/
	} else {
		rpt = false;
		BB_DBG(bb, DBG_PHY_CONFIG, "Fail Write RF-%d 0x%x = 0x%x , bit mask = 0x%x\n",
		       path, reg_addr, data, bit_mask);
	}

	return rpt;
}
void halbb_ctrl_btg_8852b(struct bb_info *bb, bool btg)
{
	struct rtw_phl_com_t *phl = bb->phl_com;
	struct dev_cap_t *dev = &phl->dev_cap;
	
	
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if(bb->hal_com->band[0].cur_chandef.band != BAND_ON_24G)
		return;

	if (btg) {
		// Path A
		halbb_set_reg(bb, 0x4738, BIT(19), 0x1);
		halbb_set_reg(bb, 0x4738, BIT(22), 0x0);
		// Path B
		halbb_set_reg(bb, 0x4AA4, BIT(19), 0x1);
		halbb_set_reg(bb, 0x4AA4, BIT(22), 0x1);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Apply BTG Setting\n");
		// Apply Grant BT by TMAC Setting
		halbb_set_reg(bb, 0x980, 0x1e0000, 0x0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Apply Grant BT by TMAC Setting\n");
		// Add BT share
		halbb_set_reg(bb, 0x49C4, BIT(14), 0x1);
		halbb_set_reg(bb, 0x49C0, 0x3c00000, 0x2);
		/* To avoid abnormal 1R CCA without BT, set rtl only 0xc6c[21] = 0x1 */
		halbb_set_reg(bb, 0x4420, BIT(31), 0x1);
		halbb_set_reg(bb, 0xc6c, BIT(21), 0x1); 
	} else {
		// Path A
		halbb_set_reg(bb, 0x4738, BIT(19), 0x0);
		halbb_set_reg(bb, 0x4738, BIT(22), 0x0);
		// Path B
		halbb_set_reg(bb, 0x4AA4, BIT(19), 0x0);
		halbb_set_reg(bb, 0x4AA4, BIT(22), 0x0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Disable BTG Setting\n");
		// Ignore Grant BT by PMAC Setting
		halbb_set_reg(bb, 0x980, 0x1e0000, 0x0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Ignore Grant BT by PMAC Setting\n");
		// Reset BT share
		halbb_set_reg(bb, 0x49C4, BIT(14), 0x0);
		halbb_set_reg(bb, 0x49C0, 0x3c00000, 0x0);
		/* To avoid abnormal 1R CCA without BT, set rtl only 0xc6c[21] = 0x1 */
		halbb_set_reg(bb, 0x4420, BIT(31), 0x1);
		halbb_set_reg(bb, 0xc6c, BIT(21), 0x0); 
	}
}

void halbb_ctrl_btc_preagc_8852b(struct bb_info *bb, bool bt_en)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (bt_en) {
		// DFIR Corner
		halbb_set_reg(bb, 0x46D0, BIT(1) | BIT(0), 0x3);
		halbb_set_reg(bb, 0x4790, BIT(1) | BIT(0), 0x3);
		// LNA Backoff at Normal
		halbb_set_reg(bb, 0x46a0, 0x3f, 0x8);
		halbb_set_reg(bb, 0x49f4, 0x3f, 0x8);
		// LNA, TIA, ADC backoff at BT TX
		halbb_set_reg(bb, 0x4ae4, 0xffffff, 0x78899e);
		halbb_set_reg(bb, 0x4aec, 0xffffff, 0x78899e);
	} else {
		// DFIR Corner
		halbb_set_reg(bb, 0x46D0, BIT(1) | BIT(0), 0x0);
		halbb_set_reg(bb, 0x4790, BIT(1) | BIT(0), 0x0);
		// LNA Backoff at Normal
		halbb_set_reg(bb, 0x46a0, 0x3f, 0x1e);
		halbb_set_reg(bb, 0x49f4, 0x3f, 0x1e);
		// LNA, TIA, ADC backoff at BT TX
		halbb_set_reg(bb, 0x4ae4, 0xffffff, 0x4d34d2);
		halbb_set_reg(bb, 0x4aec, 0xffffff, 0x4d34d2);
	}
}
bool halbb_bw_setting_8852b(struct bb_info *bb, enum channel_width bw,
			    enum rf_path path)
{
	u32 rf_reg18 = 0;
	u32 adc_sel[2] = {0xC0EC, 0xC1EC};
	u32 wbadc_sel[2] = {0xC0E4, 0xC1E4};

	//rf_reg18 = halbb_read_rf_reg_8852b(bb, path, 0x18, RFREGOFFSETMASK);
	/*==== [Error handling] ====*/
	//if (rf_reg18 == INVALID_RF_DATA) {
	//	BB_WARNING("Invalid RF_0x18 for Path-%d\n", path);
	//	return false;
	//}
	//rf_reg18 &= ~(BIT(11) | BIT(10));
	/*==== [Switch bandwidth] ====*/
	switch (bw) {
	case CHANNEL_WIDTH_5:
	case CHANNEL_WIDTH_10:
	case CHANNEL_WIDTH_20:
		if (bw == CHANNEL_WIDTH_5) {
			/*ADC clock = 20M & WB ADC clock = 40M for BW5 */
			halbb_set_reg(bb, adc_sel[path], 0x6000, 0x1);
			halbb_set_reg(bb, wbadc_sel[path], 0x30, 0x0);
		} else if (bw == CHANNEL_WIDTH_10) {
			/*ADC clock = 40M & WB ADC clock = 80M for BW10 */
			halbb_set_reg(bb, adc_sel[path], 0x6000, 0x2);
			halbb_set_reg(bb, wbadc_sel[path], 0x30, 0x1);
		} else if (bw == CHANNEL_WIDTH_20) {
			/*ADC clock = 80M & WB ADC clock = 160M for BW20 */
			halbb_set_reg(bb, adc_sel[path], 0x6000, 0x0);
			halbb_set_reg(bb, wbadc_sel[path], 0x30, 0x2);
		}

		/*RF bandwidth */
		//rf_reg18 |= (BIT(11) | BIT(10));

		break;
	case CHANNEL_WIDTH_40:
		/*ADC clock = 80M & WB ADC clock = 160M for BW40 */
		halbb_set_reg(bb, adc_sel[path], 0x6000, 0x0);
		halbb_set_reg(bb, wbadc_sel[path], 0x30, 0x2);

		/*RF bandwidth */
		//rf_reg18 |= BIT(11);

		break;
	case CHANNEL_WIDTH_80:
		/*ADC clock = 160M & WB ADC clock = 160M for BW40 */
		halbb_set_reg(bb, adc_sel[path], 0x6000, 0x0);
		halbb_set_reg(bb, wbadc_sel[path], 0x30, 0x2);

		/*RF bandwidth */
		//rf_reg18 |= BIT(10);

		break;
	default:
		BB_WARNING("Fail to set ADC\n");
	}

	/*==== [Write RF register] ====*/
	//Already Move to RF API
	
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success][bw_setting] ADC setting for Path-%d\n", path);
	return true;
}

bool halbb_ctrl_bw_8852b(struct bb_info *bb, u8 pri_ch, enum channel_width bw,
			 enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (bb->is_disable_phy_api) {
		BB_DBG(bb, DBG_PHY_CONFIG, "[%s] Disable PHY API\n", __func__);
		return true;
	}

	/*==== Error handling ====*/
	if (bw >= CHANNEL_WIDTH_MAX || (bw == CHANNEL_WIDTH_40 && pri_ch > 2) ||
	    (bw == CHANNEL_WIDTH_80 && pri_ch > 4)) {
		BB_WARNING("Fail to switch bw(bw:%d, pri ch:%d)\n", bw,
			   pri_ch);
		return false;
	}

	/*==== Switch bandwidth ====*/
	switch (bw) {
	case CHANNEL_WIDTH_5:
	case CHANNEL_WIDTH_10:
	case CHANNEL_WIDTH_20:
		if (bw == CHANNEL_WIDTH_5) {
			/*RF_BW:[31:30]=0x0 */
			halbb_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x0,
					  phy_idx);
			/*small BW:[13:12]=0x1 */
			halbb_set_reg_cmn(bb, 0x49C4, 0x3000, 0x1, phy_idx);
			/*Pri ch:[11:8]=0x0 */
			halbb_set_reg_cmn(bb, 0x49C4, 0xf00, 0x0, phy_idx);
		} else if (bw == CHANNEL_WIDTH_10) {
			/*RF_BW:[31:30]=0x0 */
			halbb_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x0,
					  phy_idx);
			/*small BW:[13:12]=0x2 */
			halbb_set_reg_cmn(bb, 0x49C4, 0x3000, 0x2, phy_idx);
			/*Pri ch:[11:8]=0x0 */
			halbb_set_reg_cmn(bb, 0x49C4, 0xf00, 0x0, phy_idx);
		} else if (bw == CHANNEL_WIDTH_20) {
			/*RF_BW:[31:30]=0x0 */
			halbb_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x0,
					  phy_idx);
			/*small BW:[13:12]=0x0 */
			halbb_set_reg_cmn(bb, 0x49C4, 0x3000, 0x0, phy_idx);
			/*Pri ch:[11:8]=0x0 */
			halbb_set_reg_cmn(bb, 0x49C4, 0xf00, 0x0, phy_idx);
		}

		break;
	case CHANNEL_WIDTH_40:
		/*RF_BW:[31:30]=0x1 */
		halbb_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x1, phy_idx);
		/*small BW:[13:12]=0x0 */
		halbb_set_reg_cmn(bb, 0x49C4, 0x3000, 0x0, phy_idx);
		/*Pri ch:[11:8] */
		halbb_set_reg_cmn(bb, 0x49C4, 0xf00, pri_ch, phy_idx);
		/*CCK primary channel */
		if (pri_ch == 1)
			halbb_set_reg(bb, 0x237c, BIT(0), 1);
		else
			halbb_set_reg(bb, 0x237c, BIT(0), 0);

		break;
	case CHANNEL_WIDTH_80:
		/*RF_BW:[31:30]=0x2 */
		halbb_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x2, phy_idx);
		/*small BW:[13:12]=0x0 */
		halbb_set_reg_cmn(bb, 0x49C4, 0x3000, 0x0, phy_idx);
		/*Pri ch:[11:8] */
		halbb_set_reg_cmn(bb, 0x49C4, 0xf00, pri_ch, phy_idx);

		break;
	default:
		BB_WARNING("Fail to switch bw (bw:%d, pri ch:%d)\n", bw,
			   pri_ch);
	}

	
	/*============== [Path A] ==============*/
	halbb_bw_setting_8852b(bb, bw, RF_PATH_A);
	/*============== [Path B] ==============*/
	halbb_bw_setting_8852b(bb, bw, RF_PATH_B);
	

	BB_DBG(bb, DBG_PHY_CONFIG,
		  "[Switch BW Success] BW: %d for PHY%d\n", bw, phy_idx);

	return true;
}

bool halbb_ch_setting_8852b(struct bb_info *bb, u8 central_ch, enum rf_path path,
			    bool *is_2g_ch)
{
	u32 rf_reg18 = 0;

	*is_2g_ch = (central_ch <= 14) ? true : false;
	//RF_18 R/W already move to RF API
	BB_DBG(bb, DBG_PHY_CONFIG, "[Success][ch_setting] CH: %d for Path-%d\n",
	       central_ch, path);
	return true;
}

bool halbb_ctrl_ch_8852b(struct bb_info *bb, u8 central_ch,
			 enum phl_phy_idx phy_idx)
{
	u8 sco_comp;
	bool is_2g_ch;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (bb->is_disable_phy_api) {
		BB_DBG(bb, DBG_PHY_CONFIG, "[%s] Disable PHY API\n", __func__);
		return true;
	}
	/*==== Error handling ====*/
	if ((central_ch > 14 && central_ch < 36) ||
	    (central_ch > 64 && central_ch < 100) ||
	    (central_ch > 144 && central_ch < 149) ||
	    central_ch > 177 ||
	    central_ch== 0) {
		BB_WARNING("Invalid CH:%d for PHY%d\n", central_ch,
			   phy_idx);
		return false;
	}

	
	/*============== [Path A] ==============*/
	halbb_ch_setting_8852b(bb, central_ch, RF_PATH_A, &is_2g_ch);
	//------------- [Mode Sel - Path A] ------------//
	if (is_2g_ch)
		halbb_set_reg_cmn(bb, 0x4738, BIT(17), 1, phy_idx); 
	else
		halbb_set_reg_cmn(bb, 0x4738, BIT(17), 0, phy_idx);

	/*============== [Path B] ==============*/

	halbb_ch_setting_8852b(bb, central_ch, RF_PATH_B, &is_2g_ch);
	//------------- [Mode Sel - Path B] ------------//
	if (is_2g_ch)
		halbb_set_reg_cmn(bb, 0x4AA4, BIT(17), 1, phy_idx); 
	else
		halbb_set_reg_cmn(bb, 0x4AA4, BIT(17), 0, phy_idx);

	/*==== [SCO compensate fc setting] ====*/
	sco_comp = halbb_sco_mapping_8852b(bb, central_ch);
	halbb_set_reg_cmn(bb, 0x49C0, 0x7f, sco_comp, phy_idx); 

	/* === CCK Parameters === */
	if (central_ch == 14) {
		halbb_set_reg(bb, 0x2300, 0xffffff, 0x3b13ff);
		halbb_set_reg(bb, 0x2304, 0xffffff, 0x1c42de);
		halbb_set_reg(bb, 0x2308, 0xffffff, 0xfdb0ad);
		halbb_set_reg(bb, 0x230c, 0xffffff, 0xf60f6e);
		halbb_set_reg(bb, 0x2310, 0xffffff, 0xfd8f92);
		halbb_set_reg(bb, 0x2314, 0xffffff, 0x2d011);
		halbb_set_reg(bb, 0x2318, 0xffffff, 0x1c02c);
		halbb_set_reg(bb, 0x231c, 0xffffff, 0xfff00a);
	} else {	
		halbb_set_reg(bb, 0x2300, 0xffffff, 0x3d23ff);
		halbb_set_reg(bb, 0x2304, 0xffffff, 0x29b354);
		halbb_set_reg(bb, 0x2308, 0xffffff, 0xfc1c8);
		halbb_set_reg(bb, 0x230c, 0xffffff, 0xfdb053);
		halbb_set_reg(bb, 0x2310, 0xffffff, 0xf86f9a);
		halbb_set_reg(bb, 0x2314, 0xffffff, 0xfaef92);
		halbb_set_reg(bb, 0x2318, 0xffffff, 0xfe5fcc);
		halbb_set_reg(bb, 0x231c, 0xffffff, 0xffdff5);
	}
	/* === Set Gain Error === */
	halbb_set_gain_error_8852b(bb, central_ch);
	/* === Set Efuse === */
	halbb_set_efuse_8852b(bb, central_ch, HW_PHY_0);
	/* === Set RXSC RPL Comp === */
	halbb_set_rxsc_rpl_comp_8852b(bb, central_ch);

	/* === Set Ch idx report in phy-sts === */
	halbb_set_reg_cmn(bb, 0x0734, 0x0ff0000, central_ch, phy_idx);
	bb->bb_ch_i.rf_central_ch_cfg = central_ch;

	BB_DBG(bb, DBG_PHY_CONFIG, "[Switch CH Success] CH: %d for PHY%d\n",
	       central_ch, phy_idx);
	return true;
}

void halbb_ctrl_cck_en_8852b(struct bb_info *bb, bool cck_en,
			     enum phl_phy_idx phy_idx)
{
	if (cck_en) {
		//halbb_set_reg(bb, 0x2300, BIT(27), 0);
		halbb_set_reg(bb, 0x700, BIT(5), 1);
		halbb_set_reg(bb, 0x2344, BIT(31), 0);		
	} else {
		//halbb_set_reg(bb, 0x2300, BIT(27), 1);
		halbb_set_reg(bb, 0x700, BIT(5), 0);
		halbb_set_reg(bb, 0x2344, BIT(31), 1);
	}
	BB_DBG(bb, DBG_PHY_CONFIG, "[CCK Enable for PHY%d]\n", phy_idx);
}

bool halbb_ctrl_bw_ch_8852b(struct bb_info *bb, u8 pri_ch, u8 central_ch,
			    enum channel_width bw, enum band_type band,
			    enum phl_phy_idx phy_idx)
{
	bool rpt = true;
	bool cck_en = false;
	u8 pri_ch_idx = 0;
	bool is_2g_ch;
	
	is_2g_ch = (band == BAND_ON_24G) ? true : false;
	/*==== [Set pri_ch idx] ====*/
	if (central_ch <= 14) {
#ifdef BANDEDGE_FILTER_CFG_FOR_ULOFDMA
		/*==== [UL-OFDMA 2x 1p6 Tx WA] ====*/
		halbb_set_reg(bb, 0x4498, BIT(30), 1);
#endif

		// === 2G === //
		switch (bw) {
		case CHANNEL_WIDTH_20:
			break;

		case CHANNEL_WIDTH_40:
			pri_ch_idx = pri_ch > central_ch ? 1 : 2;
			break;

		default:
			break;
		}

		/*==== [CCK SCO Compesate] ====*/
		rpt &= halbb_ctrl_sco_cck_8852b(bb, pri_ch);

		cck_en = true;
	} else {
		// === 5G === //
		switch (bw) {
		case CHANNEL_WIDTH_20:
#ifdef BANDEDGE_FILTER_CFG_FOR_ULOFDMA
			/*==== [UL-OFDMA 2x 1p6 Tx WA] ====*/
			halbb_set_reg(bb, 0x4498, BIT(30), 1);
#endif
			break;
		case CHANNEL_WIDTH_40:
		case CHANNEL_WIDTH_80:
			if (pri_ch > central_ch)
				pri_ch_idx = (pri_ch - central_ch) >> 1;
			else
				pri_ch_idx = ((central_ch - pri_ch) >> 1) + 1;
#ifdef BANDEDGE_FILTER_CFG_FOR_ULOFDMA
			/*==== [UL-OFDMA 2x 1p6 Tx WA] ====*/
			halbb_set_reg(bb, 0x4498, BIT(30), 0);
#endif
			break;

		default:
			break;
		}
		cck_en = false;
	}

	/*==== [Switch CH] ====*/
	rpt &= halbb_ctrl_ch_8852b(bb, central_ch, phy_idx);
	/*==== [Switch BW] ====*/
	rpt &= halbb_ctrl_bw_8852b(bb, pri_ch_idx, bw, phy_idx);
	/*==== [CCK Enable / Disable] ====*/
	halbb_ctrl_cck_en_8852b(bb, cck_en, phy_idx);
	/*==== [Spur elimination] ====*/
		//TBD
	if (is_2g_ch && ((bb->rx_path == RF_PATH_B) || (bb->rx_path == RF_PATH_AB))){
		halbb_ctrl_btg_8852b(bb, true);
	} else if (is_2g_ch && (bb->rx_path == RF_PATH_A)) {
		halbb_ctrl_btg_8852b(bb, false);
	} else {
		// Path A
		halbb_set_reg(bb, 0x4738, BIT(19), 0x0);
		halbb_set_reg(bb, 0x4738, BIT(22), 0x0);
		// Path B
		halbb_set_reg(bb, 0x4AA4, BIT(19), 0x0);
		halbb_set_reg(bb, 0x4AA4, BIT(22), 0x0);
		// Ignore Grant BT by PMAC Setting
		halbb_set_reg(bb, 0x980, 0x1e0000, 0xf);
		// Reset BT share
		halbb_set_reg(bb, 0x49C4, BIT(14), 0x0);
		halbb_set_reg(bb, 0x49C0, 0x3c00000, 0x0);
		/* To avoid abnormal 1R CCA without BT, set rtl only 0xc6c[21] = 0x1 */
		halbb_set_reg(bb, 0x4420, BIT(31), 0x0);
		halbb_set_reg(bb, 0xc6c, BIT(21), 0x0);
	}

	/* Dynamic 5M Mask Setting */
	halbb_5m_mask_8852b(bb, pri_ch, bw, phy_idx);

	/*==== [BB reset] ====*/
	halbb_bb_reset_all_8852b(bb, phy_idx);

	return rpt;
}

bool halbb_ctrl_rx_path_8852b(struct bb_info *bb, enum rf_path rx_path)
{
	u32 ofdm_rx = 0x0;

	ofdm_rx = (u32)rx_path;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	bb->rx_path = rx_path;

	if (ofdm_rx == RF_PATH_A) {
		halbb_set_reg(bb, 0x49C4, 0xf, 0x1);
		/*==== 1rcca ====*/
		halbb_set_reg(bb, 0x49C0, 0x3C000, 1);
		halbb_set_reg(bb, 0x49C0, 0x3C0000, 1);
		/*==== Rx HT nss_limit / mcs_limit ====*/
		halbb_set_reg(bb, 0xd18, BIT(9) | BIT(8), 0);
		halbb_set_reg(bb, 0xd18, BIT(22) | BIT(21), 0);
		/*==== Rx HE n_user_max / tb_max_nss ====*/
		halbb_set_reg(bb, 0xd80, 0x3fc0, 4);
		halbb_set_reg(bb, 0xd80, BIT(16) | BIT(15) | BIT(14), 0);
		halbb_set_reg(bb, 0xd80, BIT(25) | BIT(24) | BIT(23), 0);
	} else if (ofdm_rx == RF_PATH_B) {
		halbb_set_reg(bb, 0x49C4, 0xf, 0x2);
		/*==== 1rcca ====*/
		halbb_set_reg(bb, 0x49C0, 0x3C000, 0x2);
		halbb_set_reg(bb, 0x49C0, 0x3C0000, 0x2);
		/*==== Rx HT nss_limit / mcs_limit ====*/
		halbb_set_reg(bb, 0xd18, BIT(9) | BIT(8), 0);
		halbb_set_reg(bb, 0xd18, BIT(22) | BIT(21), 0);
		/*==== Rx HE n_user_max / tb_max_nss ====*/
		halbb_set_reg(bb, 0xd80, 0x3fc0, 4);
		halbb_set_reg(bb, 0xd80, BIT(16) | BIT(15) | BIT(14), 0);
		halbb_set_reg(bb, 0xd80, BIT(25) | BIT(24) | BIT(23), 0);
	} else if (ofdm_rx == RF_PATH_AB) {
		halbb_set_reg(bb, 0x49C4, 0xf, 0x3);
		/*==== 1rcca ====*/
		halbb_set_reg(bb, 0x49C0, 0x3C000, 0x3);
		halbb_set_reg(bb, 0x49C0, 0x3C0000, 0x3);
		/*==== Rx HT nss_limit / mcs_limit ====*/
		halbb_set_reg(bb, 0xd18, BIT(9) | BIT(8), 1);
		halbb_set_reg(bb, 0xd18, BIT(22) | BIT(21), 1);
		/*==== Rx HE n_user_max / tb_max_nss ====*/
		halbb_set_reg(bb, 0xd80, 0x3fc0, 4);
		halbb_set_reg(bb, 0xd80, BIT(16) | BIT(15) | BIT(14), 1);
		halbb_set_reg(bb, 0xd80, BIT(25) | BIT(24) | BIT(23), 1);
	}
	/*==== [Set Efuse] =====*/
	halbb_set_efuse_8852b(bb, bb->hal_com->band[0].cur_chandef.center_ch, HW_PHY_0);
	
	/* === [BTG setting] === */
	if ((bb->hal_com->band[0].cur_chandef.band == BAND_ON_24G) && ((rx_path == RF_PATH_B) || (rx_path == RF_PATH_AB)))
		halbb_ctrl_btg_8852b(bb, true);
	else
		halbb_ctrl_btg_8852b(bb, false);
	
	/*==== [TSSI reset] ====*/
	if (rx_path == RF_PATH_A) {
		halbb_set_reg(bb, 0x58dc, BIT(31) | BIT(30), 0x1);
		halbb_set_reg(bb, 0x58dc, BIT(31) | BIT(30), 0x3);
	} else {
		halbb_set_reg(bb, 0x78dc, BIT(31) | BIT(30), 0x1);
		halbb_set_reg(bb, 0x78dc, BIT(31) | BIT(30), 0x3);
	}

	/*==== [BB reset] ====*/
	//halbb_bb_reset_all_8852b(bb, HW_PHY_0);
	BB_DBG(bb, DBG_PHY_CONFIG, "[Rx Success]RX_en=%x\n", rx_path);
	return true;

}

bool halbb_ctrl_tx_path_8852b(struct bb_info *bb, enum rf_path tx_path)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	/*==== [P-MAC] Path & Path_map Enable ====*/
	halbb_set_reg_cmn(bb, 0x09a4, 0x1c, 0x7, HW_PHY_0);
	halbb_set_reg_cmn(bb, 0x09a4, 0x1c, 0x7, HW_PHY_1);


	if (tx_path == RF_PATH_A) {
		halbb_set_reg(bb, 0x458C, 0xf0000000, 0x1);
		halbb_set_reg(bb, 0x45B4, 0x1e0000, 0x0);
	} else if (tx_path == RF_PATH_B) {
		halbb_set_reg(bb, 0x458C, 0xf0000000, 0x2);
		halbb_set_reg(bb, 0x45B4, 0x1e0000, 0x0);
	} else if (tx_path == RF_PATH_AB) {
		halbb_set_reg(bb, 0x458C, 0xf0000000, 0x3);
		halbb_set_reg(bb, 0x45B4, 0x1e0000, 0x4);
	} else {
		BB_WARNING("Invalid Tx Path\n");
		return false;
	}

	/*==== [BB reset] ====*/
	halbb_bb_reset_all_8852b(bb, HW_PHY_0);
	BB_DBG(bb, DBG_PHY_CONFIG, "[Success] [P-MAC] Tx Path Config\n");
	return true;
}

void halbb_ctrl_rf_mode_8852b(struct bb_info *bb,  enum phl_rf_mode mode)
{
	if (mode == RF_MODE_STANDBY) {
		halbb_set_reg(bb, 0x12ac, 0xfffffff0, 0x1111111);
		halbb_set_reg(bb, 0x12b0, 0xfff, 0x111);
		halbb_set_reg(bb, 0x32ac, 0xfffffff0, 0x1111111);
		halbb_set_reg(bb, 0x32b0, 0xfff, 0x111);
	} else if (mode == RF_MODE_SHUTDOWN) {
		halbb_set_reg(bb, 0x12ac, 0xfffffff0, 0x0);
		halbb_set_reg(bb, 0x12b0, 0xfff, 0x0);
		halbb_set_reg(bb, 0x32ac, 0xfffffff0, 0x0);
		halbb_set_reg(bb, 0x32b0, 0xfff, 0x0);
	} else {
		halbb_set_reg(bb, 0x12ac, 0xfffffff0, 0x233302);
		halbb_set_reg(bb, 0x12b0, 0xfff, 0x333);
		halbb_set_reg(bb, 0x32ac, 0xfffffff0, 0x233302);
		halbb_set_reg(bb, 0x32b0, 0xfff, 0x333);
	}
	BB_DBG(bb, DBG_PHY_CONFIG, "[RF Mode] Mode = %d", mode);
}

u16 halbb_cfg_cmac_tx_ant_8852b(struct bb_info *bb, enum rf_path tx_path)
{
	// Return CMAC [OFST 20] Tx settings //
	/* [19:16] path_en[3:0] ||
	|| [21:20] map_a[1:0]   ||
	|| [23:22] map_b[1:0]   ||
	|| [25:24] map_c[1:0]   ||
	|| [27:26] map_d[1:0]   ||
	|| [28] ant_sel_a[0]    ||
	|| [29] ant_sel_b[0]    ||
	|| [30] ant_sel_c[0]    ||
	|| [31] ant_sel_d[0]    */
	u16 cmac_tx_info = 0;

	if (tx_path == RF_PATH_A) {
		cmac_tx_info = 0x1;
	} else if (tx_path == RF_PATH_B) {
		cmac_tx_info = 0x2;
	} else if (tx_path == RF_PATH_AB) {
		cmac_tx_info = 0x43;
	} else {
		cmac_tx_info = 0xffff;
		BB_WARNING("Invalid Tx Path: %d\n", tx_path);
	}
	return cmac_tx_info;

}

void halbb_ctrl_trx_path_8852b(struct bb_info *bb, enum rf_path tx_path,
			       u8 tx_nss, enum rf_path rx_path, u8 rx_nss)
{
	// Rx Config
	halbb_ctrl_rx_path_8852b(bb, rx_path);

	if ((rx_nss > 2) || (tx_nss > 2)) {
		BB_WARNING("[Invalid Nss]Tx Nss: %d, Rx Nss: %d\n", tx_nss,
			   rx_nss);
		return;
	}

	if (rx_nss == 1) {
		/*==== [PHY0] Rx HT nss_limit / mcs_limit ====*/
		halbb_set_reg(bb, 0xd18, BIT(9) | BIT(8), 0);
		halbb_set_reg(bb, 0xd18, BIT(22) | BIT(21), 0);
		/*==== [PHY0] Rx HE n_user_max / tb_max_nss ====*/
		halbb_set_reg(bb, 0xd80, BIT(16) | BIT(15) | BIT(14), 0);
		halbb_set_reg(bb, 0xd80, BIT(25) | BIT(24) | BIT(23), 0);
	} else {
		/*==== [PHY0] Rx HT nss_limit / mcs_limit ====*/
		halbb_set_reg(bb, 0xd18, BIT(9) | BIT(8), 1);
		halbb_set_reg(bb, 0xd18, BIT(22) | BIT(21), 1);
		/*==== [PHY0] Rx HE n_user_max / tb_max_nss ====*/
		halbb_set_reg(bb, 0xd80, BIT(16) | BIT(15) | BIT(14), 1);
		halbb_set_reg(bb, 0xd80, BIT(25) | BIT(24) | BIT(23), 1);
	}

	// Tx Config (to do)
	// Need to Add MP flag for Tx_path API since Normal Drv will also call this function
	// ==== [T-MAC] Path & Path_map Enable ==== //
	halbb_set_reg_cmn(bb, 0x09a4, 0x1c, 0x0, HW_PHY_0);

}

void halbb_ctrl_rx_cca_8852b(struct bb_info *bb, bool cca_en, enum phl_phy_idx phy_idx)
{
	if (cca_en) {
		halbb_set_reg_cmn(bb, 0xc3c, BIT(9), 0, phy_idx);
		halbb_set_reg(bb, 0x2344, BIT(31), 0);
	} else {
		halbb_set_reg_cmn(bb, 0xc3c, BIT(9), 1, phy_idx);
		halbb_set_reg(bb, 0x2344, BIT(31), 1);
	}
	BB_DBG(bb, DBG_PHY_CONFIG, "[Rx CCA] CCA_EN = %d\n", cca_en);
}

void halbb_ctrl_ofdm_en_8852b(struct bb_info *bb, bool ofdm_en,
			      enum phl_phy_idx phy_idx)
{
	if (ofdm_en)
		halbb_set_reg_cmn(bb, 0x700, BIT(4), 1, phy_idx);
	else
		halbb_set_reg_cmn(bb, 0x700, BIT(4), 0, phy_idx);

	BB_DBG(bb, DBG_PHY_CONFIG, "[OFDM Enable for PHY%d]\n", phy_idx);
}



// =================== [Power Module] =================== //
bool halbb_set_txpwr_dbm_8852b(struct bb_info *bb, s16 power_dbm,
			       enum phl_phy_idx phy_idx)
{
	bool tmp = false;
	
	power_dbm &= 0x1ff;
	halbb_set_reg_cmn(bb, 0x09a4, BIT(16), 1, phy_idx);
	halbb_set_reg_cmn(bb, 0x4594, 0x7fc00000, power_dbm, phy_idx);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] Set Tx pwr(dBm) for [PHY-%d] : %d\n", phy_idx,
	       power_dbm);
	tmp = true;
	return tmp;
}

s16 halbb_get_txpwr_dbm_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	u32 txpwr_dbm;
	s16 output;
	
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);
	
	txpwr_dbm = halbb_get_reg_cmn(bb, 0x4594, 0x7fc00000, phy_idx);
	output = (s16)halbb_cnvrt_2_sign(txpwr_dbm, 9);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] Get Tx pwr(dBm) for [PHY-%d] : %d\n", phy_idx,
	       output);
	return output;
}

s16 halbb_get_txinfo_txpwr_dbm_8852b(struct bb_info *bb)
{
	u32 txpwr_dbm;
	s16 output;
	
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	txpwr_dbm = halbb_get_reg(bb, 0x1804, 0x7FC0000);
	output = (s16)halbb_cnvrt_2_sign(txpwr_dbm, 9);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] Get TxInfo pwr(dBm) : %d\n", output);
	return output;
}

bool halbb_set_cck_txpwr_idx_8852b(struct bb_info *bb, u16 power_idx,
				   enum rf_path tx_path)
{
	u32 pwr_idx_addr[2] = {0x5808, 0x7808};
	
	/*==== Power index Check ====*/
	if ((power_idx & ~0x1ff) != 0) {
		BB_WARNING("Power Idx: %x\n", power_idx);
		return false;
	}
	/*==== Tx Path Check ====*/
	if (tx_path > RF_PATH_B) {
		BB_WARNING("Invalid Tx Path for CCK Txpwr_idx setting (52A)\n");
		return false;
	}
	halbb_set_reg(bb, pwr_idx_addr[tx_path], 0x3fe00, power_idx);

	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [CCK] Set Tx pwr idx for [Path-%d] : %x\n",
	       tx_path, power_idx);
	return true;
}

u16 halbb_get_cck_txpwr_idx_8852b(struct bb_info *bb, enum rf_path tx_path)
{
	u16 cck_pwr_idx;
	u32 pwr_idx_addr[2] = {0x5808, 0x7808};

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	cck_pwr_idx = (u16)halbb_get_reg(bb, pwr_idx_addr[tx_path], 0x3fe00);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [CCK] Get Tx pwr idx for [Path-%d] : %x\n",
	       tx_path, cck_pwr_idx);
	return cck_pwr_idx;
}

s16 halbb_get_cck_ref_dbm_8852b(struct bb_info *bb, enum rf_path tx_path)
{
	u32 cck_ref_dbm;
	u32 pwr_ref_addr[2] = {0x5808, 0x7808};
	s16 output;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	cck_ref_dbm = halbb_get_reg(bb, pwr_ref_addr[tx_path], 0x1ff);
	output = (s16)halbb_cnvrt_2_sign(cck_ref_dbm, 9);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [CCK] Get Tx ref pwr(dBm) for [Path-%d] : %d\n",
	       tx_path, output);
	return output;
}

bool halbb_set_ofdm_txpwr_idx_8852b(struct bb_info *bb, u16 power_idx,
				    enum rf_path tx_path)
{
	u32 pwr_idx_addr[2] = {0x5804, 0x7804};
	
	/*==== Power index Check ====*/
	if ((power_idx & ~0x1ff) != 0) {
		BB_WARNING("Power Idx: %x\n", power_idx);
		return false;
	}
	/*==== Tx Path Check ====*/
	if (tx_path > RF_PATH_B) {
		BB_WARNING("Invalid Tx Path for CCK Txpwr_idx setting (52A)\n");
		return false;
	}

	halbb_set_reg(bb, pwr_idx_addr[tx_path], 0x3fe00, power_idx);

	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [OFDM] Set Tx pwr idx for [Path-%d] : %x\n",
	       tx_path, power_idx);
	return true;
}

u16 halbb_get_ofdm_txpwr_idx_8852b(struct bb_info *bb, enum rf_path tx_path)
{
	u16 ofdm_pwr_idx;
	u32 pwr_idx_addr[2] = {0x5804, 0x7804};

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	ofdm_pwr_idx = (u16)halbb_get_reg(bb, pwr_idx_addr[tx_path], 0x3fe00);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [OFDM] Get Tx pwr idx for [Path-%d] : %x\n",
	       tx_path, ofdm_pwr_idx);
	return ofdm_pwr_idx;
}

s16 halbb_get_ofdm_ref_dbm_8852b(struct bb_info *bb, enum rf_path tx_path)
{
	u32 ofdm_ref_dbm;
	u32 pwr_ref_addr[2] = {0x5804, 0x7804};
	s16 output;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	ofdm_ref_dbm = halbb_get_reg(bb, pwr_ref_addr[tx_path], 0x1ff);
	output = (s16)halbb_cnvrt_2_sign(ofdm_ref_dbm, 9);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [OFDM] Get Tx ref pwr(dBm) for [Path-%d] : %d\n",
	       tx_path, output);
	return output;
}

void halbb_reset_bb_hw_cnt_8852b(struct bb_info *bb)
{

	/*@ Reset all counter*/
	halbb_set_reg(bb, 0x730, BIT(0), 1);
	halbb_set_reg(bb, 0x730, BIT(0), 0);

}

void halbb_backup_info_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	/*==== This Backup info is for RF TSSI calibration =====*/
	bb->bb_cmn_backup_i.cur_tx_path = (u8)halbb_get_reg_cmn(bb, 0x458c, 0xf0000000, phy_idx);
	bb->bb_cmn_backup_i.cur_rx_path = (u8)halbb_get_reg_cmn(bb, 0x49c4, 0xf, phy_idx);
	bb->bb_cmn_backup_i.cur_tx_pwr = halbb_get_txpwr_dbm_8852b(bb, phy_idx);

	BB_DBG(bb, DBG_PHY_CONFIG, "[Backup Info] [PHY%d] Tx path = %x\n", phy_idx, bb->bb_cmn_backup_i.cur_tx_path);
	BB_DBG(bb, DBG_PHY_CONFIG, "[Backup Info] [PHY%d] Tx pwr = %x\n", phy_idx, (u16)bb->bb_cmn_backup_i.cur_tx_pwr);
}

void halbb_restore_info_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	/*==== This Restore info is for RF TSSI calibration =====*/
	halbb_set_reg_cmn(bb, 0x458c, 0xf0000000, bb->bb_cmn_backup_i.cur_tx_path, phy_idx);
	if (bb->bb_cmn_backup_i.cur_tx_path == 0x3) {
		halbb_set_reg(bb, 0x45B4, 0x1e0000, 0x4);
	} else {
		halbb_set_reg(bb, 0x45B4, 0x1e0000, 0x0);
	}
	halbb_set_reg_cmn(bb, 0x49c4, 0xf, bb->bb_cmn_backup_i.cur_rx_path, phy_idx);
	halbb_set_txpwr_dbm_8852b(bb, bb->bb_cmn_backup_i.cur_tx_pwr, phy_idx);

	BB_DBG(bb, DBG_PHY_CONFIG, "[Restore Info] [PHY%d] Tx path = %x\n", phy_idx, bb->bb_cmn_backup_i.cur_tx_path);
	BB_DBG(bb, DBG_PHY_CONFIG, "[Restore Info] [PHY%d] Tx pwr = %x\n", phy_idx, (u16)bb->bb_cmn_backup_i.cur_tx_pwr);
}

bool halbb_set_txsc_8852b(struct bb_info *bb, u8 txsc, enum phl_phy_idx phy_idx)
{
	/*==== txsc Check ====*/
	if ((txsc & ~0xf) != 0) {
		BB_WARNING("TXSC: %x\n", txsc);
		return false;
	}
	halbb_set_reg_cmn(bb, 0x45ac, 0x7800000, txsc, phy_idx);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [P-MAC] TXSC for [PHY-%d] : %x\n", phy_idx,
	       txsc);
	return true;
}

#if 0
void halbb_ctrl_btg_8852b(struct bb_info *bb, bool btg)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (btg) {
		// Path A
		halbb_set_reg(bb, 0x466c, BIT(18) | BIT(17), 0x1);
		// Path B
		halbb_set_reg(bb, 0x4740, BIT(18) | BIT(17), 0x3);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Apply BTG Setting\n");
		// Apply Grant BT by TMAC Setting
		halbb_set_reg(bb, 0x980, 0x1e0000, 0x0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Apply Grant BT by TMAC Setting\n");
	} else {
		// Path A
		halbb_set_reg(bb, 0x466c, BIT(18) | BIT(17), 0x0);
		// Path B
		halbb_set_reg(bb, 0x4740, BIT(18) | BIT(17), 0x0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Disable BTG Setting\n");
		// Ignore Grant BT by PMAC Setting
		halbb_set_reg(bb, 0x980, 0x1e0000, 0xf);
		halbb_set_reg(bb, 0x980, 0x3c000000, 0x4);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Ignore Grant BT by PMAC Setting\n");
	}
}
#endif
bool halbb_set_bss_color_8852b(struct bb_info *bb, u8 bss_color, 
			       enum phl_phy_idx phy_idx)
{
	/*==== BSS color Check ====*/
	if ((bss_color & ~0x3f) != 0) {
		BB_WARNING("BSS color: %x\n", bss_color);
		return false;
	}
	//=== [Enable BSS color mapping] ===//
	halbb_set_reg_cmn(bb, 0x43b0, BIT(28), 0x1, phy_idx);
	halbb_set_reg_cmn(bb, 0x43b0, 0xfc00000, bss_color, phy_idx);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [P-MAC] BSS Color for [PHY-%d] : %x\n", phy_idx,
	       bss_color);
	return true;
}

bool halbb_set_sta_id_8852b(struct bb_info *bb, u16 sta_id,
			    enum phl_phy_idx phy_idx)
{
	/*==== Station ID Check ====*/
	if ((sta_id & ~0x7ff) != 0) {
		BB_WARNING("Station ID: %x\n", sta_id);
		return false;
	}
	//=== [Set Station ID] ===//
	halbb_set_reg_cmn(bb, 0x43b0, 0x3ff800, sta_id, phy_idx);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success] [P-MAC] Station ID for [PHY-%d] : %x\n", phy_idx,
	       sta_id);

	return true;
}

void halbb_set_igi_8852b(struct bb_info *bb, u8 lna_idx, bool tia_idx,
			u8 rxbb_idx, enum rf_path path)
{
	u8 lna = 0;
	bool tia = 0;
	u8 rxbb = 0;

	u32 lna_addr[2] = {0x472c, 0x4a80};
	u32 tia_addr[2] = {0x473c, 0x4aa8};
	u32 rxbb_addr[2] = {0x46a8, 0x4a5c};

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	halbb_set_reg(bb, lna_addr[path], 0x7000000, lna_idx);
	halbb_set_reg(bb, tia_addr[path], BIT(9), tia_idx);
	halbb_set_reg(bb, rxbb_addr[path], 0x7c00, rxbb_idx);

	lna = (u8)halbb_get_reg(bb, lna_addr[path], BIT(26) | BIT(25) | BIT(24));
	tia = (bool)halbb_get_reg(bb, tia_addr[path], BIT(9));
	rxbb = (u8)halbb_get_reg(bb, rxbb_addr[path], 0x7c00);

	BB_DBG(bb, DBG_PHY_CONFIG, "[IGI] LNA for [Path-%d] : %d\n", path, lna);
	BB_DBG(bb, DBG_PHY_CONFIG, "[IGI] TIA for [Path-%d] : %d\n", path, tia);
	BB_DBG(bb, DBG_PHY_CONFIG, "[IGI] RxBB for [Path-%d] : %d\n", path, rxbb);
}

void halbb_set_tx_pow_ref_8852b(struct bb_info *bb, s16 pw_dbm_ofdm, /*s(9,2)*/
				  s16 pw_dbm_cck, s8 ofst,
				  u8 base_cw_0db, u16 tssi_16dBm_cw,
				  u16 *ofdm_cw, u16 *cck_cw,
				  enum phl_phy_idx phy_idx)
{
	s16 rf_pw_cw = 0;
	u32 pw_cw = 0;
	u32 val = 0;
	s16 pw_s10_3 = 0;
	u32 tssi_ofst_cw = 0;

	/*OFDM*/
	pw_s10_3 = (pw_dbm_ofdm * 2) + (s16)(ofst) + (s16)(base_cw_0db * 8);


	pw_cw = pw_s10_3;
	rf_pw_cw = (pw_s10_3 & 0x1F8) >> 3;

	if (rf_pw_cw > 63)
		pw_cw = (63 << 3) | (pw_s10_3 & 0x7); /*upper bound (+24dBm)*/
	else if (rf_pw_cw < 15)
		pw_cw = (15 << 3) | (pw_s10_3 & 0x7); /*lower bound (-24dBm)*/

	/* ===[Set TSSI Offset]===============================================*/
	/*
	172 = 300 -  (55 - 39) * 8;
	tssi_ofst_cw = tssi_16dBm_cw -  (tx_pow_16dBm_ref_cw - tx_pow_ref_cw) * 8;
	             = tssi_16dBm_cw + tx_pow_ref * 8  - tx_pow_16dBm_ref * 8
	*/
	tssi_ofst_cw = (u32)((s16)tssi_16dBm_cw + (pw_dbm_ofdm * 2) - (16 * 8));

	BB_DBG(bb, DBG_DBG_API, "[OFDM]tssi_ofst_cw=%d, rf_cw=0x%x, bb_cw=0x%x\n", tssi_ofst_cw, pw_cw >> 3, pw_cw & 0x7);
	
	*ofdm_cw = (u16)pw_cw;
	val = tssi_ofst_cw << 18 | pw_cw << 9 | (u32)(pw_dbm_ofdm & 0x1ff);
	halbb_set_reg_cmn(bb, 0x5804, 0x7FFFFFF, val, phy_idx);
	halbb_set_reg_cmn(bb, 0x7804, 0x7FFFFFF, val, phy_idx);

	/*CCK*/
	pw_s10_3 = (pw_dbm_cck * 2) + (s16)(ofst) + (s16)(base_cw_0db * 8);

	pw_cw = pw_s10_3;
	rf_pw_cw = (pw_s10_3 & 0x1F8) >> 3;

	if (rf_pw_cw > 63)
		pw_cw = (63 << 3) | (pw_s10_3 & 0x7); /*upper bound (+24dBm)*/
	else if (rf_pw_cw < 15)
		pw_cw = (15 << 3) | (pw_s10_3 & 0x7); /*lower bound (-24dBm)*/

	/* ===[Set TSSI Offset]===============================================*/
	/*
	172 = 300 -  (55 - 39) * 8;
	tssi_ofst_cw = tssi_16dBm_cw -  (tx_pow_16dBm_ref_cw - tx_pow_ref_cw) * 8;
	             = tssi_16dBm_cw + tx_pow_ref * 8  - tx_pow_16dBm_ref * 8
	*/
	tssi_ofst_cw = (u32)((s16)tssi_16dBm_cw + (pw_dbm_cck * 2) - (16 * 8));
	
	BB_DBG(bb, DBG_DBG_API, "[CCK] tssi_ofst_cw=%d, rf_cw=0x%x, bb_cw=0x%x\n", tssi_ofst_cw, pw_cw >> 3, pw_cw & 0x7);

	*cck_cw = (u16)pw_cw;
	val = tssi_ofst_cw << 18 | pw_cw << 9 | (u32)(pw_dbm_cck & 0x1ff);
	halbb_set_reg_cmn(bb, 0x5808, 0x7FFFFFF, val, phy_idx);
	halbb_set_reg_cmn(bb, 0x7808, 0x7FFFFFF, val, phy_idx);

}

void halbb_dump_bb_reg_8852b(struct bb_info *bb, u32 *_used, char *output,
			       u32 *_out_len, bool dump_2_buff)
{
	u32 i = 0, addr = 0;
	u32 cr_start = 0, cr_end = 0;
	u32 dump_cr_table[][2] = {{0x0000, 0x04FC},
				  {0x0600, 0x0DFC},
				  {0x1000, 0x10FC},
				  {0x1200, 0x13FC},
				  {0x1700, 0x20FC},
				  {0x2200, 0x24FC},
				  {0x2600, 0x2DFC},
				  {0x3000, 0x30FC},
				  {0x3200, 0x33FC},
				  {0x3900, 0x4FFC},
				  {0x5800, 0x6FFC},
				  {0x7800, 0x7FFC}};
	u32 table_len = sizeof(dump_cr_table)/(sizeof(u32) * 2);

	BB_TRACE("BBCR: dump all ==>\n");
	BB_TRACE("table_len=%d\n", table_len);

	for (i = 0; i < table_len; i ++) {
		if (dump_2_buff)
			BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "[%02d] Reg[0x%04x ~ 0x%04x]\n\n",
				    i, dump_cr_table[i][0], dump_cr_table[i][1]);
		else
			BB_TRACE("[%02d] Reg[0x%04x ~ 0x%04x]\n\n",
				 i, dump_cr_table[i][0], dump_cr_table[i][1]);

	}

	for (i = 0; i < table_len; i ++) {
		cr_start = dump_cr_table[i][0];
		cr_end = dump_cr_table[i][1];
		for (addr = cr_start; addr <= cr_end; addr += 4) {
			if (dump_2_buff)
				BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "0x%04x 0x%08x\n",
					    addr, halbb_get_reg(bb, addr, MASKDWORD));
			else
				BB_TRACE("0x%04x 0x%08x\n",
					 addr, halbb_get_reg(bb, addr, MASKDWORD));
		}
	}
}

void halbb_dbgport_dump_all_8852b(struct bb_info *bb, u32 *_used, char *output,
				    u32 *_out_len)
{
	const u32 dump_dbgport_table[][3] = {
					{DBGPORT_IP_TD, 0x001, 0x026},
					{DBGPORT_IP_TD, 0x200, 0x2ff},
					{DBGPORT_IP_TD, 0xb01, 0xb27},
					{DBGPORT_IP_RX_INNER, 0x0, 0x29},
					{DBGPORT_IP_TX_INNER, 0x0, 0x8},
					{DBGPORT_IP_OUTER, 0x0, 0xaa},
					{DBGPORT_IP_OUTER, 0xc0, 0xc4},
					{DBGPORT_IP_INTF, 0x0, 0x40},
					{DBGPORT_IP_CCK, 0x0, 0x3e},
					{DBGPORT_IP_BF, 0x0, 0x59},
					{DBGPORT_IP_RX_OUTER, 0x00, 0x63},
					{DBGPORT_IP_RX_OUTER, 0x90, 0x98},
					{DBGPORT_IP_RX_OUTER, 0xc0, 0xc3},
					{DBGPORT_IP_RX_OUTER, 0xe0, 0xe3}};
	u32 table_len;
	u32 dp = 0; /*debug port value*/
	u8 i;
	u32 j;
	u32 dbg_start = 0, dbg_end = 0;

	table_len = sizeof(dump_dbgport_table) / (sizeof(u32) * 3);

	BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
		    "BB DBG Port: dump all ==>\n");

	for (i = 0; i < table_len; i++) {
		BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[%02d][IP=%02d] Dbgport[0x%03x ~ 0x%03x]\n",
			    i, dump_dbgport_table[i][0],
			    dump_dbgport_table[i][1], dump_dbgport_table[i][2]);
	}

	for (i = 0; i < table_len; i++) {
		halbb_set_bb_dbg_port_ip(bb, dump_dbgport_table[i][0]);
		dbg_start = dump_dbgport_table[i][1];
		dbg_end = dump_dbgport_table[i][2];
		for (j = dbg_start; j <= dbg_end; j ++) {
			halbb_set_bb_dbg_port(bb, j);
			dp = halbb_get_bb_dbg_port_val(bb);
			BB_DBG_VAST(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "[0x%02x, 0x%03x] = 0x%08x\n",
				    dump_dbgport_table[i][0], j, dp);
		}
	}
	halbb_release_bb_dbg_port(bb);
}

void halbb_physts_brk_fail_pkt_rpt_8852b(struct bb_info *bb, bool enable,
					   enum phl_phy_idx phy_idx)
{
	u32 val32 = (enable) ? 0 : 0x3;

	halbb_set_reg_cmn(bb, 0x0738, 0xC, 0x3, phy_idx);
}

bool halbb_rf_write_bb_reg_8852b(struct bb_info *bb, u32 addr, u32 mask, u32 data)
{
	u32 page = (addr & 0xff00) >> 8;

	if (page != 0x0c && page != 0x20 && page != 0x2c &&
	    page != 0x58 && page != 0x78 &&
	    addr != 0x0700 && addr != 0x12a0 && addr != 0x12b8 &&
	    addr != 0x2320 && addr != 0x2700 &&
	    addr != 0x32a0 && addr != 0x32b8) {
		return false;
	}

	halbb_set_reg(bb, addr, mask, data);
	return true;
}

void halbb_pre_agc_en_8852b(struct bb_info *bb, bool enable)
{
	u8 en = 0;

	en = (enable == true) ? 1 : 0;
	halbb_set_reg(bb, 0x4730, BIT(31), en);
	halbb_set_reg(bb, 0x4A9C, BIT(31), en);

	BB_DBG(bb, DBG_DBG_API, "PreAGC en: 0x4730C[31]=(0x%x),0x4A9C[31]=(0x%x)\n",
	       en, en);
}

s8 halbb_efuse_exchange_8852b(struct bb_info *bb, u8 value,
				enum efuse_bit_mask mask)
{
	s8 tmp = 0;

	if (mask == LOW_MASK) {
		tmp = value & 0xf;

		if (tmp & BIT(3))
			tmp = tmp | 0xf0;
	} else {
		tmp = (value & 0xf0) >> 4;

		if (tmp & BIT(3))
			tmp = tmp | 0xf0;
	}

	return tmp;
}

void halbb_get_normal_efuse_init_8852b(struct bb_info *bb)
{
	struct bb_efuse_info *gain = &bb->bb_efuse_i;
	u8 tmp;
	u8 tmp_ofst;
	u32 check_tmp = 0, i, j;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	tmp_ofst = (u8)halbb_get_reg(bb, 0x49B0, 0xff);
	bb->bb_efuse_i.efuse_ofst = (s8)halbb_cnvrt_2_sign(tmp_ofst, 8);
	tmp_ofst = (u8)halbb_get_reg(bb, 0x4A00, 0xff);
	bb->bb_efuse_i.efuse_ofst_tb = (s8)halbb_cnvrt_2_sign(tmp_ofst, 8);

	halbb_efuse_get_info(bb, EFUSE_INFO_RF_RX_GAIN_K_A_2G_CCK, &tmp, 1);
	gain->gain_offset[RF_PATH_A][0] = halbb_efuse_exchange_8852b(bb, tmp, HIGH_MASK);
	gain->gain_offset[RF_PATH_B][0] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);
	
	halbb_efuse_get_info(bb, EFUSE_INFO_RF_RX_GAIN_K_A_2G_OFMD, &tmp, 1);
	gain->gain_offset[RF_PATH_A][1] = halbb_efuse_exchange_8852b(bb, tmp, HIGH_MASK);
	gain->gain_offset[RF_PATH_B][1] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_efuse_get_info(bb, EFUSE_INFO_RF_RX_GAIN_K_A_5GL, &tmp, 1);
	gain->gain_offset[RF_PATH_A][2] = halbb_efuse_exchange_8852b(bb, tmp, HIGH_MASK);
	gain->gain_offset[RF_PATH_B][2] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_efuse_get_info(bb, EFUSE_INFO_RF_RX_GAIN_K_A_5GM, &tmp, 1);
	gain->gain_offset[RF_PATH_A][3] = halbb_efuse_exchange_8852b(bb, tmp, HIGH_MASK);
	gain->gain_offset[RF_PATH_B][3] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_efuse_get_info(bb, EFUSE_INFO_RF_RX_GAIN_K_A_5GH, &tmp, 1);
	gain->gain_offset[RF_PATH_A][4] = halbb_efuse_exchange_8852b(bb, tmp, HIGH_MASK);
	gain->gain_offset[RF_PATH_B][4] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	for (i = 0; i < HALBB_MAX_PATH; i++) {
		for (j = 0; j < BB_EFUSE_BAND_NUM; j++) {
			BB_DBG(bb, DBG_INIT, "[Efuse]gain->gain_offset[%d][%d]=0x%x\n", i, j, gain->gain_offset[i][j]);
			if ((gain->gain_offset[i][j] & 0xf) == 0xf)
				check_tmp++;
		}
	}

	BB_DBG(bb, DBG_INIT, "[Efuse]check_tmp = %d\n", check_tmp);
	BB_DBG(bb, DBG_INIT, "[Efuse]HALBB_MAX_PATH * BB_EFUSE_BAND_NUM = %d\n", HALBB_MAX_PATH * BB_EFUSE_BAND_NUM);

	if (check_tmp == HALBB_MAX_PATH * BB_EFUSE_BAND_NUM)
		bb->bb_efuse_i.normal_efuse_check = false;
	else
		bb->bb_efuse_i.normal_efuse_check = true;
/*
	BB_DBG(bb, DBG_INIT,
	       "[Efuse][Gain 2G][CCK] Path-A: %d, Path-B: %d\n",
	       gain->gain_offset[RF_PATH_A][0], gain->gain_offset[RF_PATH_B][0]);
	BB_DBG(bb, DBG_INIT,
	       "[Efuse][Gain 2G][OFDM] Path-A: %d, Path-B: %d\n",
	       gain->gain_offset[RF_PATH_A][1], gain->gain_offset[RF_PATH_B][1]);
	BB_DBG(bb, DBG_INIT,
	       "[Efuse][Gain 5GL] Path-A: %d, Path-B: %d\n",
	       gain->gain_offset[RF_PATH_A][2], gain->gain_offset[RF_PATH_B][2]);
	BB_DBG(bb, DBG_INIT,
	       "[Efuse][Gain 5GM] Path-A: %d, Path-B: %d\n",
	       gain->gain_offset[RF_PATH_A][3], gain->gain_offset[RF_PATH_B][3]);
	BB_DBG(bb, DBG_INIT,
	       "[Efuse][Gain 5GH] Path-A: %d, Path-B: %d\n",
	       gain->gain_offset[RF_PATH_A][4], gain->gain_offset[RF_PATH_B][4]);
*/
}

void halbb_get_hide_efuse_init_8852b(struct bb_info *bb)
{
	struct bb_efuse_info *gain = &bb->bb_efuse_i;
	u8 tmp;
	u32 check_tmp = 0, i , j;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	halbb_phy_efuse_get_info(bb, GAIN_HIDE_EFUSE_A_2G_8852B, 1, &tmp);
	gain->gain_cs[RF_PATH_A][0] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);
	
	halbb_phy_efuse_get_info(bb, GAIN_HIDE_EFUSE_A_5GL_8852B, 1, &tmp);
	gain->gain_cs[RF_PATH_A][1] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_phy_efuse_get_info(bb, GAIN_HIDE_EFUSE_A_5GM_8852B, 1, &tmp);
	gain->gain_cs[RF_PATH_A][2] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_phy_efuse_get_info(bb, GAIN_HIDE_EFUSE_A_5GH_8852B, 1, &tmp);
	gain->gain_cs[RF_PATH_A][3] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_phy_efuse_get_info(bb, GAIN_HIDE_EFUSE_B_2G_8852B, 1, &tmp);
	gain->gain_cs[RF_PATH_B][0] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_phy_efuse_get_info(bb, GAIN_HIDE_EFUSE_B_5GL_8852B, 1, &tmp);
	gain->gain_cs[RF_PATH_B][1] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_phy_efuse_get_info(bb, GAIN_HIDE_EFUSE_B_5GM_8852B, 1, &tmp);
	gain->gain_cs[RF_PATH_B][2] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	halbb_phy_efuse_get_info(bb, GAIN_HIDE_EFUSE_B_5GH_8852B, 1, &tmp);
	gain->gain_cs[RF_PATH_B][3] = halbb_efuse_exchange_8852b(bb, tmp, LOW_MASK);

	for (i = 0; i < HALBB_MAX_PATH; i++) {
		for (j = 0; j < BB_GAIN_BAND_NUM; j++) {
			BB_DBG(bb, DBG_INIT, "[Efuse]gain->gain_cs[%d][%d]=0x%x\n", i, j, gain->gain_cs[i][j]);
			if ((gain->gain_cs[i][j] & 0xf) == 0xf)
				check_tmp++;
		}
	}

	BB_DBG(bb, DBG_INIT, "[Efuse]check_tmp = %d\n", check_tmp);
	BB_DBG(bb, DBG_INIT, "[Efuse]HALBB_MAX_PATH * BB_GAIN_BAND_NUM = %d\n", HALBB_MAX_PATH * BB_GAIN_BAND_NUM);

	if (check_tmp == HALBB_MAX_PATH * BB_GAIN_BAND_NUM)
		bb->bb_efuse_i.hidden_efuse_check = false;
	else
		bb->bb_efuse_i.hidden_efuse_check = true;
	

/*	
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Hidden Efuse][Gain 2G][Path-A] CS: %d , %d, %x\n",
	       gain->gain_cs[RF_PATH_A][0],gain->gain_cs[RF_PATH_A][0]<<2,gain->gain_cs[RF_PATH_A][0]<<2);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Hidden Efuse][Gain 5GL][Path-A] CS: %d, %d, %x\n",
	       gain->gain_cs[RF_PATH_A][1],gain->gain_cs[RF_PATH_A][1]<<2,gain->gain_cs[RF_PATH_A][1]<<2);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Hidden Efuse][Gain 5GM][Path-A] CS: %d, %d, %x\n",
	       gain->gain_cs[RF_PATH_A][2],gain->gain_cs[RF_PATH_A][2]<<2,gain->gain_cs[RF_PATH_A][2]<<2);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Hidden Efuse][Gain 5GH][Path-A] CS: %d, %d, %x\n",
	       gain->gain_cs[RF_PATH_A][3],gain->gain_cs[RF_PATH_A][3]<<2,gain->gain_cs[RF_PATH_A][3]<<2);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Hidden Efuse][Gain 2G][Path-B] CS: %d\n",
	       gain->gain_cs[RF_PATH_B][0]);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Hidden Efuse][Gain 5GL][Path-B] CS: %d\n",
	       gain->gain_cs[RF_PATH_B][1]);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Hidden Efuse][Gain 5GM][Path-B] CS: %d\n",
	       gain->gain_cs[RF_PATH_B][2]);
	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Hidden Efuse][Gain 5GH][Path-B] CS: %d\n",
	       gain->gain_cs[RF_PATH_B][3]);
*/
	
}

void halbb_set_efuse_8852b(struct bb_info *bb, u8 central_ch, enum phl_phy_idx phy_idx)
{
	u8 band;
	u8 gain_val = 0;
	s32 hidden_efuse = 0, normal_efuse = 0, normal_efuse_cck = 0;
	s32 tmp = 0;
	u8 path = 0;
	u32 gain_err_addr[2] = {0x4ACC, 0x4AD8}; //Wait for Bcut Def
	
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);
	
	// 2G Band: (0)
	// 5G Band: (1):Low, (2): Mid, (3):High
	if (central_ch >= 0 && central_ch <= 14)
		band = 0;
	else if (central_ch >= 36 && central_ch <= 64)
		band = 1;
	else if (central_ch >= 100 && central_ch <= 144)
		band = 2;
	else if (central_ch >= 149 && central_ch <= 177)
		band = 3;
	else
		band = 0;

	// === [Set hidden efuse] === //
	if (bb->bb_efuse_i.hidden_efuse_check) {
		for (path = RF_PATH_A; path < BB_PATH_MAX_8852B; path++) {
			gain_val = bb->bb_efuse_i.gain_cs[path][band] << 2;
			halbb_set_reg(bb, gain_err_addr[path], 0xff, gain_val);	
		}
		BB_DBG(bb, DBG_PHY_CONFIG, "[Efuse] Hidden efuse dynamic setting!!\n");
		
	} else {
		BB_DBG(bb, DBG_PHY_CONFIG, "[Efuse] Values of hidden efuse are all 0xff, bypass dynamic setting!!\n");
	}

	// === [Set normal efuse] === //
	if (bb->bb_efuse_i.normal_efuse_check) {
		if ((bb->rx_path == RF_PATH_A) || (bb->rx_path == RF_PATH_AB)) {
			normal_efuse = bb->bb_efuse_i.gain_offset[RF_PATH_A][band + 1];
			normal_efuse_cck = bb->bb_efuse_i.gain_offset[RF_PATH_A][0];
		} else if (bb->rx_path == RF_PATH_B) {
			normal_efuse = bb->bb_efuse_i.gain_offset[RF_PATH_B][band + 1];
			normal_efuse_cck = bb->bb_efuse_i.gain_offset[RF_PATH_B][0];
		}
		normal_efuse *= (-1);
		normal_efuse_cck *= (-1);

		// OFDM normal efuse
		// r_1_rpl_bias_comp
		tmp = (normal_efuse << 4) + bb->bb_efuse_i.efuse_ofst;
		halbb_set_reg_cmn(bb, 0x49B0, 0xff, (tmp & 0xff), phy_idx);
		// r_tb_rssi_bias_comp
		tmp = (normal_efuse << 4) + bb->bb_efuse_i.efuse_ofst_tb;
		halbb_set_reg_cmn(bb, 0x4A00, 0xff, (tmp & 0xff), phy_idx);	
		// CCK normal efuse
		if (band == 0) {
			tmp = (normal_efuse_cck << 3) + (bb->bb_efuse_i.efuse_ofst >>1);
			halbb_set_reg(bb, 0x23ac, 0x7f, (tmp & 0x7f));
		}
		BB_DBG(bb, DBG_PHY_CONFIG, "[Efuse] Normal efuse dynamic setting!!\n");
	} else {
		BB_DBG(bb, DBG_PHY_CONFIG, "[Efuse] Values of normal efuse are all 0xff, bypass dynamic setting!!\n");
	}
}

void halbb_set_gain_error_8852b(struct bb_info *bb, u8 central_ch)
{
	struct bb_gain_info *gain = &bb->bb_gain_i;

	u8 band;
	u8 path = 0, lna_idx = 0, tia_idx = 0;
	s32 tmp = 0;
	u32 lna_gain_g[BB_PATH_MAX_8852B][7] = {{0x4678, 0x4678, 0x467C,
						   0x467C, 0x467C, 0x467C,
						   0x4680}, {0x475C, 0x475C,
						   0x4760, 0x4760, 0x4760,
						   0x4760, 0x4764}};
	u32 lna_gain_a[BB_PATH_MAX_8852B][7] = {{0x45DC, 0x45DC, 0x4660,
						   0x4660, 0x4660, 0x4660,
						   0x4664}, {0x4740, 0x4740,
						   0x4744, 0x4744, 0x4744,
						   0x4744, 0x4748}};
	u32 lna_gain_mask[7] = {0x00ff0000, 0xff000000, 0x000000ff,
				    0x0000ff00, 0x00ff0000, 0xff000000,
				    0x000000ff};
	u32 tia_gain_g[BB_PATH_MAX_8852B][2] = {{0x4680, 0x4680}, {0x4764,
						   0x4764}};
	u32 tia_gain_a[BB_PATH_MAX_8852B][2] = {{0x4664, 0x4664}, {0x4748,
						   0x4748}};
	u32 tia_gain_mask[2] = {0x00ff0000, 0xff000000};


	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	// 2G Band: (0)
	// 5G Band: (1):Low, (2): Mid, (3):High
	if (central_ch >= 0 && central_ch <= 14)
		band = 0;
	else if (central_ch >= 36 && central_ch <= 64)
		band = 1;
	else if (central_ch >= 100 && central_ch <= 144)
		band = 2;
	else if (central_ch >= 149 && central_ch <= 177)
		band = 3;
	else
		band = 0;

	for (path = RF_PATH_A; path < BB_PATH_MAX_8852B; path++) {
		for (lna_idx = 0; lna_idx < 7; lna_idx++) {
			if (central_ch >= 0 && central_ch <= 14) {
				tmp = gain->lna_gain[band][path][lna_idx];
				halbb_set_reg(bb, lna_gain_g[path][lna_idx], lna_gain_mask[lna_idx], tmp);
			} else {
				tmp = gain->lna_gain[band][path][lna_idx];
				halbb_set_reg(bb, lna_gain_a[path][lna_idx], lna_gain_mask[lna_idx], tmp);
			}
		}
			
		for (tia_idx = 0; tia_idx < 2; tia_idx++) {
			if (central_ch >= 0 && central_ch <= 14) {
				tmp = gain->tia_gain[band][path][tia_idx];
				halbb_set_reg(bb, tia_gain_g[path][tia_idx], tia_gain_mask[tia_idx], tmp);
			} else {
				tmp = gain->tia_gain[band][path][tia_idx];
				halbb_set_reg(bb, tia_gain_a[path][tia_idx], tia_gain_mask[tia_idx], tmp);
			}
		}
	}

}

void halbb_set_rxsc_rpl_comp_8852b(struct bb_info *bb, u8 central_ch)
{
	struct bb_gain_info *gain = &bb->bb_gain_i;
	u8 band;
	u8 path = 0;
	u8 i = 0;
	u8 rxsc = 0;
	s8 ofst = 0;
	s8 bw20_avg = 0;
	s8 bw40_avg = 0, bw40_avg_1 = 0, bw40_avg_2 = 0;
	s8 bw80_avg = 0;
	s8 bw80_avg_1 = 0, bw80_avg_2 = 0, bw80_avg_3 = 0, bw80_avg_4 = 0;
	s8 bw80_avg_9 = 0, bw80_avg_10 = 0;
	u32 tmp_val1 = 0, tmp_val2 = 0, tmp_val3 = 0;


	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (central_ch >= 0 && central_ch <= 14) {
		band = 0;
	} else if (central_ch >= 36 && central_ch <= 64) {
		band = 1;
	} else if (central_ch >= 100 && central_ch <= 144) {
		band = 2;
	} else if (central_ch >= 149 && central_ch <= 177) {
		band = 3;
	} else {
		band = 0;
	}
	//20M RPL
	bw20_avg = (gain->rpl_ofst_20[band][RF_PATH_A] +
		    gain->rpl_ofst_20[band][RF_PATH_B]) >> 1;
	tmp_val1 |= (((u32)bw20_avg & 0xff) << 8);
	//40M RPL
	bw40_avg = (gain->rpl_ofst_40[band][RF_PATH_A][0] +
		    gain->rpl_ofst_40[band][RF_PATH_B][0]) >> 1;
	tmp_val1 |= (((u32)bw40_avg & 0xff) << 16);
	bw40_avg_1 = (gain->rpl_ofst_40[band][RF_PATH_A][1] +
		      gain->rpl_ofst_40[band][RF_PATH_B][1]) >> 1;
	tmp_val1 |= (((u32)bw40_avg_1 & 0xff) << 24);
	
	bw40_avg_2 = (gain->rpl_ofst_40[band][RF_PATH_A][2] +
		      gain->rpl_ofst_40[band][RF_PATH_B][2]) >> 1;
	tmp_val2 |= ((u32)bw40_avg_2 & 0xff);
	//80M RPL
	bw80_avg = (gain->rpl_ofst_80[band][RF_PATH_A][0] +
		    gain->rpl_ofst_80[band][RF_PATH_B][0]) >> 1;
	tmp_val2 |= ((u32)(bw80_avg & 0xff) << 8);
	bw80_avg_1 = (gain->rpl_ofst_80[band][RF_PATH_A][1] +
		      gain->rpl_ofst_80[band][RF_PATH_B][1]) >> 1;
	tmp_val2 |= (((u32)bw80_avg_1 & 0xff) << 16);
	bw80_avg_10 = (gain->rpl_ofst_80[band][RF_PATH_A][10] +
		       gain->rpl_ofst_80[band][RF_PATH_B][10]) >> 1;
	tmp_val2 |= (((u32)bw80_avg_10 & 0xff) << 24);
	
	bw80_avg_2 = (gain->rpl_ofst_80[band][RF_PATH_A][2] +
		      gain->rpl_ofst_80[band][RF_PATH_B][2]) >> 1;
	tmp_val3 |= ((u32)bw80_avg_2 & 0xff);
	bw80_avg_3 = (gain->rpl_ofst_80[band][RF_PATH_A][3] +
		      gain->rpl_ofst_80[band][RF_PATH_B][3]) >> 1;
	tmp_val3 |= (((u32)bw80_avg_3 & 0xff) << 8);
	bw80_avg_4 = (gain->rpl_ofst_80[band][RF_PATH_A][4] +
		      gain->rpl_ofst_80[band][RF_PATH_B][4]) >> 1;
	tmp_val3 |= (((u32)bw80_avg_4 & 0xff) << 16);
	bw80_avg_9 = (gain->rpl_ofst_80[band][RF_PATH_A][9] +
		      gain->rpl_ofst_80[band][RF_PATH_B][9]) >> 1;
	tmp_val3 |= (((u32)bw80_avg_9 & 0xff) << 24);


	BB_DBG(bb, DBG_PHY_CONFIG, "[20M RPL] gain ofst = 0x%2x\n",
		bw20_avg&0xff);
	BB_DBG(bb, DBG_PHY_CONFIG, "[40M RPL] gain ofst = 0x%2x, 0x%2x, 0x%2x\n",
		bw40_avg&0xff, bw40_avg_1&0xff, bw40_avg_2&0xff);
	BB_DBG(bb, DBG_PHY_CONFIG, "[80M RPL] gain ofst = 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x\n",
		bw80_avg&0xff,bw80_avg_1&0xff,bw80_avg_2&0xff,bw80_avg_3&0xff,bw80_avg_4&0xff,bw80_avg_9&0xff,bw80_avg_10&0xff);
	BB_DBG(bb, DBG_PHY_CONFIG, "tmp1 = 0x%x, tmp2 = 0x%x, tmp3 = 0x%x\n",
		tmp_val1, tmp_val2, tmp_val3);

	halbb_set_reg(bb, 0x49b0, 0xffffff00, tmp_val1 >> 8);
	halbb_set_reg(bb, 0x4a00, 0xffffff00, tmp_val1 >> 8);
	halbb_set_reg(bb, 0x49b4, MASKDWORD, tmp_val2);
	halbb_set_reg(bb, 0x4a04, MASKDWORD, tmp_val2);
	halbb_set_reg(bb, 0x49b8, MASKDWORD, tmp_val3);
	halbb_set_reg(bb, 0x4a08, MASKDWORD, tmp_val3);
}

void halbb_normal_efuse_verify_8852b(struct bb_info *bb, s8 rx_gain_offset,
				       enum rf_path rx_path,
				       enum phl_phy_idx phy_idx)
{
	s32 normal_efuse = 0;
	s32 tmp = 0;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	BB_DBG(bb, DBG_PHY_CONFIG, "[Normal Efuse] rx_gain_offset = %d\n",
		rx_gain_offset);

	rx_gain_offset *= (-1);

	// === [Set normal efuse] === //
	// r_1_rpl_bias_comp
	tmp = (rx_gain_offset << 4) + bb->bb_efuse_i.efuse_ofst;
	halbb_set_reg_cmn(bb, 0x49B0, 0xff, (tmp & 0xff), phy_idx);
	// r_tb_rssi_bias_comp
	tmp = (rx_gain_offset << 4) + bb->bb_efuse_i.efuse_ofst_tb;
	halbb_set_reg_cmn(bb, 0x4A00, 0xff, (bb->bb_efuse_i.efuse_ofst_tb & 0xff), phy_idx);
	
	BB_DBG(bb, DBG_PHY_CONFIG, "[Normal Efuse] 0x49B0[7:0] = 0x%x\n",
		halbb_get_reg(bb, 0x49B0, 0xff));
	BB_DBG(bb, DBG_PHY_CONFIG, "[Normal Efuse] 0x4A00[7:0] = 0x%x\n",
		halbb_get_reg(bb, 0x4A00, 0xff));

}

void halbb_normal_efuse_verify_cck_8852b(struct bb_info *bb, s8 rx_gain_offset,
					   enum rf_path rx_path,
					   enum phl_phy_idx phy_idx)
{
	s32 tmp = 0;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	rx_gain_offset *= (-1);
	tmp = (rx_gain_offset << 3) + (bb->bb_efuse_i.efuse_ofst >>1);
	halbb_set_reg(bb, 0x23ac, 0x7f, (tmp & 0x7f));

	BB_DBG(bb, DBG_PHY_CONFIG, "[Normal Efuse] gain ofst = 0x%x\n",
		rx_gain_offset);
	BB_DBG(bb, DBG_PHY_CONFIG, "[Normal Efuse] 0x23ac[6:0] = 0x%x\n",
		halbb_get_reg(bb, 0x23ac, 0x7f));
}
#endif

