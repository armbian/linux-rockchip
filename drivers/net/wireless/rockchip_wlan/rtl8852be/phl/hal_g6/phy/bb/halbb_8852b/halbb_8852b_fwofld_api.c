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
#ifdef HALBB_FW_OFLD_SUPPORT
bool halbb_fwcfg_bb_phy_8852b(struct bb_info *bb, u32 addr, u32 data,
			    enum phl_phy_idx phy_idx)
{
	bool ret = true;

	if (addr == 0xfe) {
		halbb_delay_ms(bb, 50);
		BB_DBG(bb, DBG_INIT, "Delay 50 ms\n");
	} else if (addr == 0xfd) {
		halbb_delay_ms(bb, 5);
		BB_DBG(bb, DBG_INIT, "Delay 5 ms\n");
	} else if (addr == 0xfc) {
		halbb_delay_ms(bb, 1);
		BB_DBG(bb, DBG_INIT, "Delay 1 ms\n");
	} else if (addr == 0xfb) {
		halbb_delay_us(bb, 50);
		BB_DBG(bb, DBG_INIT, "Delay 50 us\n");
	} else if (addr == 0xfa) {
		halbb_delay_us(bb, 5);
		BB_DBG(bb, DBG_INIT, "Delay 5 us\n");
	} else if (addr == 0xf9) {
		halbb_delay_us(bb, 1);
		BB_DBG(bb, DBG_INIT, "Delay 1 us\n");
	} else {
		/*FWOFLD in init BB reg flow */
		if (halbb_check_fw_ofld(bb)) {
			ret &= halbb_fw_set_reg(bb, addr, MASKDWORD, data, 0);
			BB_DBG(bb, DBG_INIT, "[REG FWOFLD]0x%04X = 0x%08X, ret = %d\n", addr, data, (u8)ret);
		}
		else {
			halbb_set_reg(bb, addr, MASKDWORD, data);
			#ifdef HALBB_DBCC_SUPPORT
			BB_DBG(bb, DBG_INIT, "[REG][%d]0x%04X = 0x%08X\n", phy_idx, addr, data);
			#else
			BB_DBG(bb, DBG_INIT, "[REG]0x%04X = 0x%08X\n", addr, data);
			#endif
		}
	}

	return ret;
}

void halbb_fwofld_bb_reset_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	// === [TSSI protect on] === //
	halbb_fw_set_reg(bb, 0x58dc, BIT(30), 0x1, 0);
	halbb_fw_set_reg(bb, 0x5818, BIT(30), 0x1, 0);
	halbb_fw_set_reg(bb, 0x78dc, BIT(30), 0x1, 0);
	halbb_fw_set_reg(bb, 0x7818, BIT(30), 0x1, 0);
	// === [BB reset] === //
	halbb_fw_set_reg(bb, 0x704, BIT(1), 1, 0);
	halbb_fw_set_reg(bb, 0x704, BIT(1), 0, 0);
	halbb_fw_set_reg(bb, 0x704, BIT(1), 1, 0);
	// === [TSSI protect off] === //
	halbb_fw_set_reg(bb, 0x58dc, BIT(30), 0x0, 0);
	halbb_fw_set_reg(bb, 0x5818, BIT(30), 0x0, 0);
	halbb_fw_set_reg(bb, 0x78dc, BIT(30), 0x0, 0);
	halbb_fw_set_reg(bb, 0x7818, BIT(30), 0x0, 0);
}

void halbb_fwofld_dfs_en_8852b(struct bb_info *bb, bool en)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	if (en)
		halbb_fw_set_reg(bb, 0x0, BIT(31), 1, 0);
	else
		halbb_fw_set_reg(bb, 0x0, BIT(31), 0, 0);
}

void halbb_fwofld_adc_en_8852b(struct bb_info *bb, bool en)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	if (en)
		halbb_fw_set_reg(bb, 0x20fc, 0xff000000, 0x0, 0);
	else
		halbb_fw_set_reg(bb, 0x20fc, 0xff000000, 0xf, 0);
}

void halbb_fwofld_tssi_cont_en_8852b(struct bb_info *bb, bool en, enum rf_path path)
{
	u32 tssi_trk_man[2] = {0x5818, 0x7818};

	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	if (en)
		halbb_fw_set_reg(bb, tssi_trk_man[path], BIT(30), 0x0, 0);
	else
		halbb_fw_set_reg(bb, tssi_trk_man[path], BIT(30), 0x1, 0);
}
void halbb_fwofld_bb_reset_all_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	//Protest SW-SI 
	halbb_fw_set_reg_cmn(bb, 0x1200, BIT(28) | BIT(29) | BIT(30), 0x7, phy_idx, 0);
	halbb_fw_set_reg_cmn(bb, 0x3200, BIT(28) | BIT(29) | BIT(30), 0x7, phy_idx, 0);
	halbb_fw_delay(bb, 1);
	// === [BB reset] === //
	halbb_fw_set_reg_cmn(bb, 0x704, BIT(1), 1, phy_idx, 0);
	halbb_fw_set_reg_cmn(bb, 0x704, BIT(1), 0, phy_idx, 0);
	
	halbb_fw_set_reg_cmn(bb, 0x1200, BIT(28) | BIT(29) | BIT(30), 0x0, phy_idx, 0);
	halbb_fw_set_reg_cmn(bb, 0x3200, BIT(28) | BIT(29) | BIT(30), 0x0, phy_idx, 0);
	halbb_fw_set_reg_cmn(bb, 0x704, BIT(1), 1, phy_idx, 0);
}

void halbb_fwofld_bb_reset_en_8852b(struct bb_info *bb, bool en, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_DBG_API, "%s\n", __func__);

	if (en) {
		halbb_fw_set_reg_cmn(bb, 0x1200, BIT(28) | BIT(29) | BIT(30), 0x0, phy_idx, 0);
		halbb_fw_set_reg_cmn(bb, 0x3200, BIT(28) | BIT(29) | BIT(30), 0x0, phy_idx, 0);
		halbb_fw_set_reg_cmn(bb, 0x704, BIT(1), 1, phy_idx, 0);
		//PD Enable
		if(bb->hal_com->band[0].cur_chandef.band == BAND_ON_24G)
			halbb_fw_set_reg(bb,0x2344, BIT(31), 0x0, 0);
		halbb_fw_set_reg(bb,0xc3c, BIT(9), 0x0, 0);
	} else {
		//PD Disable
		halbb_fw_set_reg(bb,0x2344, BIT(31), 0x1, 0);
		halbb_fw_set_reg(bb,0xc3c, BIT(9), 0x1, 0);
		//Protest SW-SI 
		halbb_fw_set_reg_cmn(bb, 0x1200, BIT(28) | BIT(29) | BIT(30), 0x7, phy_idx, 0);
		halbb_fw_set_reg_cmn(bb, 0x3200, BIT(28) | BIT(29) | BIT(30), 0x7, phy_idx, 0);
		halbb_fw_delay(bb, 1);
		halbb_fw_set_reg_cmn(bb, 0x704, BIT(1), 0, phy_idx, 0);
	}
}
#if 0
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
#endif
void halbb_fwofld_5m_mask_8852b(struct bb_info *bb, u8 pri_ch, enum channel_width bw,
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
		halbb_fw_set_reg(bb, 0x46f8, BIT(12), 0x0, 0);
		halbb_fw_set_reg(bb, 0x47b8, BIT(12), 0x0, 0);
		halbb_fw_set_reg_cmn(bb, 0x4440, BIT(31), 0x0, phy_idx, 0);
	} else {
		if (mask_5m_low) {
			halbb_fw_set_reg(bb, 0x46f8, 0x3f, 0x4, 0);
			halbb_fw_set_reg(bb, 0x46f8, BIT(12), 0x1, 0);
			halbb_fw_set_reg(bb, 0x46f8, BIT(8), 0x0, 0);
			halbb_fw_set_reg(bb, 0x46f8, BIT(6), 0x1, 0);
			halbb_fw_set_reg(bb, 0x47b8, 0x3f, 0x4, 0);
			halbb_fw_set_reg(bb, 0x47b8, BIT(12), 0x1, 0);
			halbb_fw_set_reg(bb, 0x47b8, BIT(8), 0x0, 0);
			halbb_fw_set_reg(bb, 0x47b8, BIT(6), 0x1, 0);
		} else {
			halbb_fw_set_reg(bb, 0x46f8, 0x3f, 0x4, 0);
			halbb_fw_set_reg(bb, 0x46f8, BIT(12), 0x1, 0);
			halbb_fw_set_reg(bb, 0x46f8, BIT(8), 0x1, 0);
			halbb_fw_set_reg(bb, 0x46f8, BIT(6), 0x0, 0);
			halbb_fw_set_reg(bb, 0x47b8, 0x3f, 0x4, 0);
			halbb_fw_set_reg(bb, 0x47b8, BIT(12), 0x1, 0);
			halbb_fw_set_reg(bb, 0x47b8, BIT(8), 0x1, 0);
			halbb_fw_set_reg(bb, 0x47b8, BIT(6), 0x0, 0);
		}
		halbb_fw_set_reg_cmn(bb, 0x4440, BIT(31), 0x1, phy_idx, 0);
	}
}

u8 halbb_fwofld_sco_mapping_8852b(struct bb_info *bb,  u8 central_ch)
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

bool halbb_fwofld_ctrl_sco_cck_8852b(struct bb_info *bb, u8 pri_ch)
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

	halbb_fw_set_reg(bb, 0x23b0, 0x7ffff, sco_barker_threshold[pri_ch - 1], 0);
	halbb_fw_set_reg(bb, 0x23b4, 0x7ffff, sco_cck_threshold[pri_ch - 1], 0);
	BB_DBG(bb, DBG_PHY_CONFIG, "[CCK SCO Success]");
	return true;
}

void halbb_fwofld_ctrl_btg_8852b(struct bb_info *bb, bool btg)
{
	struct rtw_phl_com_t *phl = bb->phl_com;
	struct dev_cap_t *dev = &phl->dev_cap;
	
	
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if(bb->hal_com->band[0].cur_chandef.band != BAND_ON_24G)
		return;

	if (btg) {
		// Path A
		halbb_fw_set_reg(bb, 0x4738, BIT(19), 0x1, 0);
		halbb_fw_set_reg(bb, 0x4738, BIT(22), 0x0, 0);
		// Path B
		halbb_fw_set_reg(bb, 0x4AA4, BIT(19), 0x1, 0);
		halbb_fw_set_reg(bb, 0x4AA4, BIT(22), 0x1, 0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Apply BTG Setting\n");
		// Apply Grant BT by TMAC Setting
		halbb_fw_set_reg(bb, 0x980, 0x1e0000, 0x0, 0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Apply Grant BT by TMAC Setting\n");
		// Add BT share
		halbb_fw_set_reg(bb, 0x49C4, BIT(14), 0x1, 0);
		halbb_fw_set_reg(bb, 0x49C0, 0x3c00000, 0x2, 0);
		/* To avoid abnormal 1R CCA without BT, set rtl only 0xc6c[21] = 0x1 */
		halbb_fw_set_reg(bb, 0x4420, BIT(31), 0x1, 0);
		halbb_fw_set_reg(bb, 0xc6c, BIT(21), 0x1, 0); 
	} else {
		// Path A
		halbb_fw_set_reg(bb, 0x4738, BIT(19), 0x0, 0);
		halbb_fw_set_reg(bb, 0x4738, BIT(22), 0x0, 0);
		// Path B
		halbb_fw_set_reg(bb, 0x4AA4, BIT(19), 0x0, 0);
		halbb_fw_set_reg(bb, 0x4AA4, BIT(22), 0x0, 0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Disable BTG Setting\n");
		// Ignore Grant BT by PMAC Setting
		halbb_fw_set_reg(bb, 0x980, 0x1e0000, 0x0, 0);
		BB_DBG(bb, DBG_PHY_CONFIG, "[BT] Ignore Grant BT by PMAC Setting\n");
		// Reset BT share
		halbb_fw_set_reg(bb, 0x49C4, BIT(14), 0x0, 0);
		halbb_fw_set_reg(bb, 0x49C0, 0x3c00000, 0x0, 0);
		/* To avoid abnormal 1R CCA without BT, set rtl only 0xc6c[21] = 0x1 */
		halbb_fw_set_reg(bb, 0x4420, BIT(31), 0x1, 0);
		halbb_fw_set_reg(bb, 0xc6c, BIT(21), 0x0, 0); 
	}
}

void halbb_fwofld_ctrl_btc_preagc_8852b(struct bb_info *bb, bool bt_en)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (bt_en) {
		// DFIR Corner
		halbb_fw_set_reg(bb, 0x46D0, BIT(1) | BIT(0), 0x3, 0);
		halbb_fw_set_reg(bb, 0x4790, BIT(1) | BIT(0), 0x3, 0);
		// LNA Backoff at Normal
		halbb_fw_set_reg(bb, 0x46a0, 0x3f, 0x8, 0);
		halbb_fw_set_reg(bb, 0x49f4, 0x3f, 0x8, 0);
		// LNA, TIA, ADC backoff at BT TX
		halbb_fw_set_reg(bb, 0x4ae4, 0xffffff, 0x78899e, 0);
		halbb_fw_set_reg(bb, 0x4aec, 0xffffff, 0x78899e, 0);
		
	} else {
		// DFIR Corner
		halbb_fw_set_reg(bb, 0x46D0, BIT(1) | BIT(0), 0x0, 0);
		halbb_fw_set_reg(bb, 0x4790, BIT(1) | BIT(0), 0x0, 0);
		// LNA Backoff at Normal
		halbb_fw_set_reg(bb, 0x46a0, 0x3f, 0x1e, 0);
		halbb_fw_set_reg(bb, 0x49f4, 0x3f, 0x1e, 0);
		// LNA, TIA, ADC backoff at BT TX
		halbb_fw_set_reg(bb, 0x4ae4, 0xffffff, 0x4d34d2, 0);
		halbb_fw_set_reg(bb, 0x4aec, 0xffffff, 0x4d34d2, 0);
		
	}
}
bool halbb_fwofld_bw_setting_8852b(struct bb_info *bb, enum channel_width bw,
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
			halbb_fw_set_reg(bb, adc_sel[path], 0x6000, 0x1, 0);
			halbb_fw_set_reg(bb, wbadc_sel[path], 0x30, 0x0, 0);
		} else if (bw == CHANNEL_WIDTH_10) {
			/*ADC clock = 40M & WB ADC clock = 80M for BW10 */
			halbb_fw_set_reg(bb, adc_sel[path], 0x6000, 0x2, 0);
			halbb_fw_set_reg(bb, wbadc_sel[path], 0x30, 0x1, 0);
		} else if (bw == CHANNEL_WIDTH_20) {
			/*ADC clock = 80M & WB ADC clock = 160M for BW20 */
			halbb_fw_set_reg(bb, adc_sel[path], 0x6000, 0x0, 0);
			halbb_fw_set_reg(bb, wbadc_sel[path], 0x30, 0x2, 0);
		}
		break;
	case CHANNEL_WIDTH_40:
		/*ADC clock = 80M & WB ADC clock = 160M for BW40 */
		halbb_fw_set_reg(bb, adc_sel[path], 0x6000, 0x0, 0);
		halbb_fw_set_reg(bb, wbadc_sel[path], 0x30, 0x2, 0);
		break;
	case CHANNEL_WIDTH_80:
		/*ADC clock = 160M & WB ADC clock = 160M for BW40 */
		halbb_fw_set_reg(bb, adc_sel[path], 0x6000, 0x0, 0);
		halbb_fw_set_reg(bb, wbadc_sel[path], 0x30, 0x2, 0);
		break;
	default:
		BB_WARNING("Fail to set ADC\n");
	}

	BB_DBG(bb, DBG_PHY_CONFIG,
	       "[Success][bw_setting] ADC setting for Path-%d\n", path);
	return true;
}

bool halbb_fwofld_ctrl_bw_8852b(struct bb_info *bb, u8 pri_ch, enum channel_width bw,
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
			halbb_fw_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x0, phy_idx, 0);
			/*small BW:[13:12]=0x1 */
			halbb_fw_set_reg_cmn(bb, 0x49C4, 0x3000, 0x1, phy_idx, 0);
			/*Pri ch:[11:8]=0x0 */
			halbb_fw_set_reg_cmn(bb, 0x49C4, 0xf00, 0x0, phy_idx, 0);
		} else if (bw == CHANNEL_WIDTH_10) {
			/*RF_BW:[31:30]=0x0 */
			halbb_fw_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x0, phy_idx, 0);
			/*small BW:[13:12]=0x2 */
			halbb_fw_set_reg_cmn(bb, 0x49C4, 0x3000, 0x2, phy_idx, 0);
			/*Pri ch:[11:8]=0x0 */
			halbb_fw_set_reg_cmn(bb, 0x49C4, 0xf00, 0x0, phy_idx, 0);
		} else if (bw == CHANNEL_WIDTH_20) {
			/*RF_BW:[31:30]=0x0 */
			halbb_fw_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x0, phy_idx, 0);
			/*small BW:[13:12]=0x0 */
			halbb_fw_set_reg_cmn(bb, 0x49C4, 0x3000, 0x0, phy_idx, 0);
			/*Pri ch:[11:8]=0x0 */
			halbb_fw_set_reg_cmn(bb, 0x49C4, 0xf00, 0x0, phy_idx, 0);
		}

		break;
	case CHANNEL_WIDTH_40:
		/*RF_BW:[31:30]=0x1 */
		halbb_fw_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x1, phy_idx, 0);
		/*small BW:[13:12]=0x0 */
		halbb_fw_set_reg_cmn(bb, 0x49C4, 0x3000, 0x0, phy_idx, 0);
		/*Pri ch:[11:8] */
		halbb_fw_set_reg_cmn(bb, 0x49C4, 0xf00, pri_ch, phy_idx, 0);
		/*CCK primary channel */
		if (pri_ch == 1)
			halbb_fw_set_reg(bb, 0x237c, BIT(0), 1, 0);
		else
			halbb_fw_set_reg(bb, 0x237c, BIT(0), 0, 0);

		break;
	case CHANNEL_WIDTH_80:
		/*RF_BW:[31:30]=0x2 */
		halbb_fw_set_reg_cmn(bb, 0x49C0, 0xC0000000, 0x2, phy_idx, 0);
		/*small BW:[13:12]=0x0 */
		halbb_fw_set_reg_cmn(bb, 0x49C4, 0x3000, 0x0, phy_idx, 0);
		/*Pri ch:[11:8] */
		halbb_fw_set_reg_cmn(bb, 0x49C4, 0xf00, pri_ch, phy_idx, 0);

		break;
	default:
		BB_WARNING("Fail to switch bw (bw:%d, pri ch:%d)\n", bw,
			   pri_ch);
	}

	/*============== [Path A] ==============*/
	halbb_fwofld_bw_setting_8852b(bb, bw, RF_PATH_A);
	/*============== [Path B] ==============*/
	halbb_fwofld_bw_setting_8852b(bb, bw, RF_PATH_B);

	BB_DBG(bb, DBG_PHY_CONFIG,
		  "[Switch BW Success] BW: %d for PHY%d\n", bw, phy_idx);

	return true;
}

bool halbb_fwofld_ch_setting_8852b(struct bb_info *bb, u8 central_ch, enum rf_path path,
			    bool *is_2g_ch)
{
	u32 rf_reg18 = 0;
	//*is_2g_ch = (central_ch <= 14) ? true : false;
	//RF_18 R/W already move to RF API
	BB_DBG(bb, DBG_PHY_CONFIG, "[Success][ch_setting] CH: %d for Path-%d\n",
	       central_ch, path);
	return true;
}

bool halbb_fwofld_ctrl_ch_8852b(struct bb_info *bb, u8 central_ch, enum band_type band,
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
	if (band != BAND_ON_6G) {
		if ((central_ch > 14 && central_ch < 36) ||
			(central_ch > 64 && central_ch < 100) ||
			(central_ch > 144 && central_ch < 149) ||
			central_ch > 177 ||
			central_ch== 0) {
			BB_WARNING("Invalid CH:%d for PHY%d\n", central_ch,
				phy_idx);
			return false;
			}
	} else {
		if (central_ch > 253) {
			BB_WARNING("Invalid 6G CH:%d for PHY%d\n", central_ch,
				   phy_idx);
			return false;
		}
	}

	is_2g_ch = (band == BAND_ON_24G) ? true : false;
	
	/*============== [Path A] ==============*/
	//halbb_ch_setting_8852b(bb, central_ch, RF_PATH_A, &is_2g_ch);
	//------------- [Mode Sel - Path A] ------------//
	if (is_2g_ch)
		halbb_fw_set_reg_cmn(bb, 0x4738, BIT(17), 1, phy_idx, 0); 
	else
		halbb_fw_set_reg_cmn(bb, 0x4738, BIT(17), 0, phy_idx, 0);

	/*============== [Path B] ==============*/
	//halbb_ch_setting_8852b(bb, central_ch, RF_PATH_B, &is_2g_ch);
	//------------- [Mode Sel - Path B] ------------//
	if (is_2g_ch)
		halbb_fw_set_reg_cmn(bb, 0x4AA4, BIT(17), 1, phy_idx, 0); 
	else
		halbb_fw_set_reg_cmn(bb, 0x4AA4, BIT(17), 0, phy_idx, 0);

	/*==== [SCO compensate fc setting] ====*/
	sco_comp = halbb_fwofld_sco_mapping_8852b(bb, central_ch);
	halbb_fw_set_reg_cmn(bb, 0x49C0, 0x7f, sco_comp, phy_idx, 0); 

	/* === CCK Parameters === */
	if (band != BAND_ON_6G) {
		if (central_ch == 14) {
			halbb_fw_set_reg(bb, 0x2300, 0xffffff, 0x3b13ff, 0);
			halbb_fw_set_reg(bb, 0x2304, 0xffffff, 0x1c42de, 0);
			halbb_fw_set_reg(bb, 0x2308, 0xffffff, 0xfdb0ad, 0);
			halbb_fw_set_reg(bb, 0x230c, 0xffffff, 0xf60f6e, 0);
			halbb_fw_set_reg(bb, 0x2310, 0xffffff, 0xfd8f92, 0);
			halbb_fw_set_reg(bb, 0x2314, 0xffffff, 0x2d011, 0);
			halbb_fw_set_reg(bb, 0x2318, 0xffffff, 0x1c02c, 0);
			halbb_fw_set_reg(bb, 0x231c, 0xffffff, 0xfff00a, 0);
		} else {	
			halbb_fw_set_reg(bb, 0x2300, 0xffffff, 0x3d23ff, 0);
			halbb_fw_set_reg(bb, 0x2304, 0xffffff, 0x29b354, 0);
			halbb_fw_set_reg(bb, 0x2308, 0xffffff, 0xfc1c8, 0);
			halbb_fw_set_reg(bb, 0x230c, 0xffffff, 0xfdb053, 0);
			halbb_fw_set_reg(bb, 0x2310, 0xffffff, 0xf86f9a, 0);
			halbb_fw_set_reg(bb, 0x2314, 0xffffff, 0xfaef92, 0);
			halbb_fw_set_reg(bb, 0x2318, 0xffffff, 0xfe5fcc, 0);
			halbb_fw_set_reg(bb, 0x231c, 0xffffff, 0xffdff5, 0);
		}
		/* === Set Gain Error === */
		halbb_fwofld_set_gain_error_8852b(bb, central_ch);
		/* === Set Efuse === */
		halbb_fwofld_set_efuse_8852b(bb, central_ch, HW_PHY_0);
		/* === Set RXSC RPL Comp === */
		halbb_fwofld_set_rxsc_rpl_comp_8852b(bb, central_ch);
	}

	/* === Set Ch idx report in phy-sts === */
	halbb_fw_set_reg_cmn(bb, 0x0734, 0x0ff0000, central_ch, phy_idx, 0);
	bb->bb_ch_i.rf_central_ch_cfg = central_ch;

	BB_DBG(bb, DBG_PHY_CONFIG, "[Switch CH Success] CH: %d for PHY%d\n",
	       central_ch, phy_idx);
	return true;
}

void halbb_fwofld_ctrl_cck_en_8852b(struct bb_info *bb, bool cck_en,
			     enum phl_phy_idx phy_idx)
{
	if (cck_en) {
		//halbb_set_reg(bb, 0x2300, BIT(27), 0);
		halbb_fw_set_reg(bb, 0x700, BIT(5), 1, 0);
		halbb_fw_set_reg(bb, 0x2344, BIT(31), 0, 0);		
	} else {
		//halbb_set_reg(bb, 0x2300, BIT(27), 1);
		halbb_fw_set_reg(bb, 0x700, BIT(5), 0, 0);
		halbb_fw_set_reg(bb, 0x2344, BIT(31), 1, 0);
	}
	BB_DBG(bb, DBG_PHY_CONFIG, "[CCK Enable for PHY%d]\n", phy_idx);
}

bool halbb_fwofld_ctrl_bw_ch_8852b(struct bb_info *bb, u8 pri_ch, u8 central_ch,
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
		halbb_fw_set_reg(bb, 0x4498, BIT(30), 1, 0);
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
		rpt &= halbb_fwofld_ctrl_sco_cck_8852b(bb, pri_ch);

		cck_en = true;
	} else {
		// === 5G === //
		switch (bw) {
		case CHANNEL_WIDTH_20:
#ifdef BANDEDGE_FILTER_CFG_FOR_ULOFDMA	
			/*==== [UL-OFDMA 2x 1p6 Tx WA] ====*/
			halbb_fw_set_reg(bb, 0x4498, BIT(30), 1, 0);
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
			halbb_fw_set_reg(bb, 0x4498, BIT(30), 0, 0);
#endif
			break;

		default:
			break;
		}
		cck_en = false;
	}

	/*==== [Switch CH] ====*/
	rpt &= halbb_fwofld_ctrl_ch_8852b(bb, central_ch, band, phy_idx);
	/*==== [Switch BW] ====*/
	rpt &= halbb_fwofld_ctrl_bw_8852b(bb, pri_ch_idx, bw, phy_idx);
	/*==== [CCK Enable / Disable] ====*/
	halbb_fwofld_ctrl_cck_en_8852b(bb, cck_en, phy_idx);
	/*==== [Spur elimination] ====*/
		//TBD
	/*==== [BTG Ctrl] ====*/
	if (is_2g_ch && ((bb->rx_path == RF_PATH_B) || (bb->rx_path == RF_PATH_AB))){
		halbb_fwofld_ctrl_btg_8852b(bb, true);
	} else if (is_2g_ch && (bb->rx_path == RF_PATH_A)) {
		halbb_fwofld_ctrl_btg_8852b(bb, false);
	} else {
		// Path A
		halbb_fw_set_reg(bb, 0x4738, BIT(19), 0x0, 0);
		halbb_fw_set_reg(bb, 0x4738, BIT(22), 0x0, 0);
		// Path B
		halbb_fw_set_reg(bb, 0x4AA4, BIT(19), 0x0, 0);
		halbb_fw_set_reg(bb, 0x4AA4, BIT(22), 0x0, 0);
		// Ignore Grant BT by PMAC Setting
		halbb_fw_set_reg(bb, 0x980, 0x1e0000, 0xf, 0);
		// Reset BT share
		halbb_fw_set_reg(bb, 0x49C4, BIT(14), 0x0, 0);
		halbb_fw_set_reg(bb, 0x49C0, 0x3c00000, 0x0, 0);
		/* To avoid abnormal 1R CCA without BT, set rtl only 0xc6c[21] = 0x1 */
		halbb_fw_set_reg(bb, 0x4420, BIT(31), 0x0, 0);
		halbb_fw_set_reg(bb, 0xc6c, BIT(21), 0x0, 0);
	}

	/* Dynamic 5M Mask Setting */
	halbb_fwofld_5m_mask_8852b(bb, pri_ch, bw, phy_idx);
	
	/*==== [BB reset] ====*/
	halbb_fwofld_bb_reset_all_8852b(bb, phy_idx);

	return rpt;
}

void halbb_fwofld_set_efuse_8852b(struct bb_info *bb, u8 central_ch, enum phl_phy_idx phy_idx)
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
			halbb_fw_set_reg(bb, gain_err_addr[path], 0xff, gain_val, 0);
			
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
		halbb_fw_set_reg_cmn(bb, 0x49B0, 0xff, (tmp & 0xff), phy_idx, 0);
		// r_tb_rssi_bias_comp
		tmp = (normal_efuse << 4) + bb->bb_efuse_i.efuse_ofst_tb;
		halbb_fw_set_reg_cmn(bb, 0x4A00, 0xff, (tmp & 0xff), phy_idx, 0);	
		// CCK normal efuse
		if (band == 0) {
			tmp = (normal_efuse_cck << 3) + (bb->bb_efuse_i.efuse_ofst >>1);
			halbb_fw_set_reg(bb, 0x23ac, 0x7f, (tmp & 0x7f), 0);
		}
		BB_DBG(bb, DBG_PHY_CONFIG, "[Efuse] Normal efuse dynamic setting!!\n");
	} else {
		BB_DBG(bb, DBG_PHY_CONFIG, "[Efuse] Values of normal efuse are all 0xff, bypass dynamic setting!!\n");
	}
}

void halbb_fwofld_set_gain_error_8852b(struct bb_info *bb, u8 central_ch)
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
				halbb_fw_set_reg(bb, lna_gain_g[path][lna_idx], lna_gain_mask[lna_idx], tmp, 0);
			} else {
				tmp = gain->lna_gain[band][path][lna_idx];
				halbb_fw_set_reg(bb, lna_gain_a[path][lna_idx], lna_gain_mask[lna_idx], tmp, 0);
			}
		}
			
		for (tia_idx = 0; tia_idx < 2; tia_idx++) {
			if (central_ch >= 0 && central_ch <= 14) {
				tmp = gain->tia_gain[band][path][tia_idx];
				halbb_fw_set_reg(bb, tia_gain_g[path][tia_idx], tia_gain_mask[tia_idx], tmp, 0);
			} else {
				tmp = gain->tia_gain[band][path][tia_idx];
				halbb_fw_set_reg(bb, tia_gain_a[path][tia_idx], tia_gain_mask[tia_idx], tmp, 0);
			}
		}
	}

}

void halbb_fwofld_set_rxsc_rpl_comp_8852b(struct bb_info *bb, u8 central_ch)
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

	halbb_fw_set_reg(bb, 0x49b0, 0xffffff00, tmp_val1 >> 8, 0);
	halbb_fw_set_reg(bb, 0x4a00, 0xffffff00, tmp_val1 >> 8, 0);
	halbb_fw_set_reg(bb, 0x49b4, MASKDWORD, tmp_val2, 0);
	halbb_fw_set_reg(bb, 0x4a04, MASKDWORD, tmp_val2, 0);
	halbb_fw_set_reg(bb, 0x49b8, MASKDWORD, tmp_val3, 0);
	halbb_fw_set_reg(bb, 0x4a08, MASKDWORD, tmp_val3, 0);
}
#endif
#endif

