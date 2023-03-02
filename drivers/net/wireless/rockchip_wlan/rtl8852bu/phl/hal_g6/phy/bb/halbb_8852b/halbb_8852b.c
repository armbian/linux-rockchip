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

bool halbb_chk_pkg_valid_8852b(struct bb_info *bb, u8 bb_ver, u8 rf_ver)
{
	bool valid = true;

#if 0
	if (bb_ver >= X && rf_ver >= Y)
		valid = true;
	else if (bb_ver < X && rf_ver < Y)
		valid = true;
	else
		valid = false;
#endif

	if (!valid) {
		/*halbb_set_reg(bb, 0x1c3c, (BIT(0) | BIT(1)), 0x0);*/
		BB_WARNING("[%s] Pkg_ver{bb, rf}={%d, %d} disable all BB block\n",
			 __func__, bb_ver, rf_ver);
	}

	return valid;
}


void halbb_stop_pmac_tx_8852b(struct bb_info *bb,
			      struct halbb_pmac_info *tx_info,
			      enum phl_phy_idx phy_idx)
{
	if (tx_info->is_cck) { // CCK
		if (tx_info->mode == CONT_TX) {
			halbb_set_reg(bb, 0x2300, BIT(26), 1);
			halbb_set_reg(bb, 0x2338, BIT(17), 0);
			halbb_set_reg(bb, 0x2300, BIT(28), 0);
			halbb_set_reg(bb, 0x2300, BIT(26), 0);
		} else if (tx_info->mode == PKTS_TX) {
			halbb_set_reg_cmn(bb, 0x9c4, BIT(4), 0, phy_idx);
		}
	} else { // OFDM
		if (tx_info->mode == CONT_TX)
			halbb_set_reg_cmn(bb, 0x9c4, BIT(0), 0, phy_idx);
		else if (tx_info->mode == PKTS_TX)
			halbb_set_reg_cmn(bb, 0x9c4, BIT(4), 0, phy_idx);
	}
}


void halbb_start_pmac_tx_8852b(struct bb_info *bb,
			       struct halbb_pmac_info *tx_info,
			       enum halbb_pmac_mode mode, u32 pkt_cnt,u16 period,
			       enum phl_phy_idx phy_idx)
{
	if (mode == CONT_TX) {
		if (tx_info->is_cck) {
			halbb_set_reg(bb, 0x2338, BIT(17), 1);
			halbb_set_reg(bb, 0x2300, BIT(28), 0);
		} else {
			halbb_set_reg_cmn(bb, 0x9c4, BIT(0), 1, phy_idx);
		}
	} else if (mode == PKTS_TX) {
		/*Tx_N_PACKET_EN */
		halbb_set_reg_cmn(bb, 0x9c4, BIT(4), 1, phy_idx);
		/*Tx_N_PERIOD */
		halbb_set_reg_cmn(bb, 0x9c4, 0xffffff00, period, phy_idx);
		/*Tx_N_PACKET */
		halbb_set_reg_cmn(bb, 0x9c8, 0xffffffff, pkt_cnt, phy_idx);
	} else if (mode == CCK_CARRIER_SIPPRESSION_TX) {
		if (tx_info->is_cck) {
			/*Carrier Suppress Tx*/
			halbb_set_reg(bb, 0x2338, BIT(18), 1);
			/*Disable scrambler at payload part*/
			halbb_set_reg(bb, 0x2304, BIT(26), 1);
		} else {
			return;
		}
		/*Tx_N_PACKET_EN */
		halbb_set_reg_cmn(bb, 0x9c4, BIT(4), 1, phy_idx);
		/*Tx_N_PERIOD */
		halbb_set_reg_cmn(bb, 0x9c4, 0xffffff00, period, phy_idx);
		/*Tx_N_PACKET */
		halbb_set_reg_cmn(bb, 0x9c8, 0xffffffff, pkt_cnt, phy_idx);
	}
	/*Tx_EN */
	halbb_set_reg_cmn(bb, 0x9c0, BIT(0), 1, phy_idx);
	halbb_set_reg_cmn(bb, 0x9c0, BIT(0), 0, phy_idx);
}


void halbb_set_pmac_tx_8852b(struct bb_info *bb, struct halbb_pmac_info *tx_info,
			     enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (!tx_info->en_pmac_tx) {
		halbb_stop_pmac_tx_8852b(bb, tx_info, phy_idx);
		/* PD hit enable */
		halbb_set_reg_cmn(bb, 0xc3c, BIT(9), 0, phy_idx);
		halbb_set_reg(bb, 0x2344, BIT(31), 0);
		return;
	}
	/*Turn on PMAC */
	/* Tx */
	halbb_set_reg_cmn(bb, 0x0980, BIT(0), 1, phy_idx);
	/* Rx */
	halbb_set_reg_cmn(bb, 0x0980, BIT(16), 1, phy_idx);
	halbb_set_reg_cmn(bb, 0x0988, 0x3f, 0x3f, phy_idx);

	/* PD hit enable */
	halbb_set_reg(bb, 0x704, BIT(1), 0);
	halbb_set_reg_cmn(bb, 0xc3c, BIT(9), 1, phy_idx);
	halbb_set_reg(bb, 0x2344, BIT(31), 1);
	halbb_set_reg(bb, 0x704, BIT(1), 1);

	halbb_start_pmac_tx_8852b(bb, tx_info, tx_info->mode, tx_info->tx_cnt,
		       tx_info->period, phy_idx);
}

void halbb_set_tmac_tx_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	/* To do: 0x0d80[16] [25] / 0x0d88[5:0] Should be set to default value in parameter package*/
	/* Turn on TMAC */
	halbb_set_reg_cmn(bb, 0x0980, BIT(0), 0, phy_idx);
	halbb_set_reg_cmn(bb, 0x0980, BIT(16), 0, phy_idx);
	halbb_set_reg_cmn(bb, 0x0988, 0xfff, 0, phy_idx);
	halbb_set_reg_cmn(bb, 0x0994, 0xf0, 0, phy_idx);
	
	// PDP bypass from TMAC
	halbb_set_reg_cmn(bb, 0x09a4, BIT(10), 0, phy_idx);
	// TMAC Tx path
	halbb_set_reg_cmn(bb, 0x09a4, 0x1c, 0, phy_idx);
	// TMAC Tx power
	halbb_set_reg_cmn(bb, 0x09a4, BIT(16), 0, phy_idx);
}

void halbb_ic_hw_setting_init_8852b(struct bb_info *bb)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if(phl_is_mp_mode(bb->phl_com)) {
		// r_en_sound_wo_ndp
		halbb_set_reg(bb, 0xd7c, BIT(1), 1);
		halbb_set_reg(bb, 0x2d7c, BIT(1), 1);
	} else {
		// r_en_sound_wo_ndp
		halbb_set_reg(bb, 0xd7c, BIT(1), 0);
		halbb_set_reg(bb, 0x2d7c, BIT(1), 0);
	}
}

bool halbb_set_pd_lower_bound_8852b(struct bb_info *bb, u8 bound,
				      enum channel_width bw,
				      enum phl_phy_idx phy_idx)
{
	/* 
	Range of bound value: 
	BW20: 95~33
	BW40: 92~30
	BW80: 89~27
	*/
	u8 bw_attenuation = 0;
	u8 subband_filter_atteniation = 7;
	u8 bound_idx = 0;
	bool rpt = true;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (bound == 0) {
		halbb_set_reg_cmn(bb, 0x4860, BIT(30), 0, phy_idx);
		BB_DBG(bb, DBG_PHY_CONFIG,
		       "[PD Bound] Set Boundary to default!\n");
		return true;
	}

	bb->bb_cmn_backup_i.cur_pd_lower_bound = bound;

	if (bw == CHANNEL_WIDTH_20) {
		bw_attenuation = 0;
	} else if (bw == CHANNEL_WIDTH_40) {
		bw_attenuation = 3;
	} else if (bw == CHANNEL_WIDTH_80) {
		bw_attenuation = 6;
	} else {
		BB_DBG(bb, DBG_PHY_CONFIG,
		       "[PD Bound] Only support BW20/40/80 !\n");
		return false;
	}

	bound += (bw_attenuation + subband_filter_atteniation);
	// If Boundary dbm is odd, set it to even number
	bound = bound % 2 ? bound + 1 : bound;

	if (bound < 40) {
		BB_DBG(bb, DBG_PHY_CONFIG,
		       "[PD Bound] Threshold too high, set to highest level!\n");
		bound = 40;
		rpt = false;
	}

	if (bound > 102) {
		BB_DBG(bb, DBG_PHY_CONFIG,
		       "[PD Bound] Threshold too low, disable PD lower bound function!\n");
		halbb_set_reg_cmn(bb, 0x4860, BIT(30), 0, phy_idx);
		return true;
	}

	bound_idx =  (102 - bound) >> 1;

	halbb_set_reg_cmn(bb, 0x4860, 0x7c0, bound_idx, phy_idx);
	halbb_set_reg_cmn(bb, 0x4860, BIT(30), 1, phy_idx);

	BB_DBG(bb, DBG_PHY_CONFIG, "[PD Bound] Set Boundary Success!\n");

	return rpt;
}

bool halbb_set_pd_lower_bound_cck_8852b(struct bb_info *bb, u8 bound,
				      enum channel_width bw,
				      enum phl_phy_idx phy_idx)
{
	u8 bw_attenuation = 0;
	u8 subband_filter_atteniation = 5;
	s8 bound_tmp = 0;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (bb->hal_com->cv < CBV) {
		BB_DBG(bb, DBG_PHY_CONFIG, "[PD Bound] CCK pd_lower_bound CV not support!\n");
		return false;
	}
	
	if (bound == 0) {
		halbb_set_reg_cmn(bb, 0x23b0, BIT(23), 0, phy_idx);
		BB_DBG(bb, DBG_PHY_CONFIG,
			"[PD Bound] Set Boundary to default!\n");
		return true;
	}

	if (bw == CHANNEL_WIDTH_20) {
		bw_attenuation = 0;
	}
	else if (bw == CHANNEL_WIDTH_40) {
		bw_attenuation = 3;
	}
	else if (bw == CHANNEL_WIDTH_80) {
		bw_attenuation = 6;
	}
	else {
		BB_DBG(bb, DBG_PHY_CONFIG,
			"[PD Bound] Only support BW20/40/80 !\n");
		return true;
	}

	bound += (bw_attenuation + subband_filter_atteniation);
	bound_tmp = (-1) * MIN_2(bound, 128);

	halbb_set_reg_cmn(bb, 0x23b0, 0xff000000, bound_tmp, phy_idx);
	halbb_set_reg_cmn(bb, 0x23b4, 0xff000000, 0x7f, phy_idx);
	halbb_set_reg_cmn(bb, 0x23b0, BIT(23), 1, phy_idx);

	BB_DBG(bb, DBG_PHY_CONFIG, "[PD Bound] Set CCK Boundary Success!\n");

	return true;
}

u8 halbb_querry_pd_lower_bound_8852b(struct bb_info *bb, bool get_en_info, enum phl_phy_idx phy_idx)
{
	u8 tmp;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (get_en_info)
		tmp = (u8)halbb_get_reg_cmn(bb, 0x4860, BIT(30), phy_idx);
	else
		tmp = bb->bb_cmn_backup_i.cur_pd_lower_bound;

	return tmp;
}

void halbb_pop_en_8852b(struct bb_info *bb, bool en, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	if (en) {
		halbb_set_reg_cmn(bb, 0x47D4, BIT(8), 1, phy_idx);

	} else {
		halbb_set_reg_cmn(bb, 0x47D4, BIT(8), 0, phy_idx);
	}
}

bool halbb_querry_pop_en_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	bool en;

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	en = (bool)halbb_get_reg_cmn(bb, 0x47D4, BIT(8), phy_idx);

	return en;
}
#endif
