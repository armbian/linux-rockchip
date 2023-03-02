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

/*============================ [Tx Settings] =============================*/
void halbb_set_pmac_tx(struct bb_info *bb, struct halbb_pmac_info *tx_info,
		       enum phl_phy_idx phy_idx)
{

	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		halbb_set_pmac_tx_8852a(bb, tx_info, phy_idx);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		halbb_set_pmac_tx_8852a_2(bb, tx_info, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		halbb_set_pmac_tx_8852b(bb, tx_info, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		halbb_set_pmac_tx_8852c(bb, tx_info, phy_idx);
		break;
	#endif
	default:
		break;
	}
}

void halbb_set_tmac_tx(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		halbb_set_tmac_tx_8852a(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		halbb_set_tmac_tx_8852a_2(bb, phy_idx);
		break;
	#endif
	
	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		halbb_set_tmac_tx_8852b(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		halbb_set_tmac_tx_8852c(bb, phy_idx);
		break;
	#endif
	default:
		break;
	}
}
/*============================ [Power Module] =============================*/
bool halbb_cfg_lbk(struct bb_info *bb, bool lbk_en, bool is_dgt_lbk,
		   enum rf_path tx_path, enum rf_path rx_path,
		   enum channel_width bw, enum phl_phy_idx phy_idx)
{
	bool rpt = true;

	switch (bb->ic_type) {

	#ifdef BB_8852A_A_CUT_SUPPORT
	case BB_RTL8852AA:
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		break;
	#endif
	
	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_cfg_lbk_8852c(bb, lbk_en, is_dgt_lbk, tx_path,
					  rx_path, bw, phy_idx);
		break;
	#endif
	default:
		break;
	}

	return rpt;
}

/*============================ [Power Module] =============================*/
bool halbb_set_txpwr_dbm(struct bb_info *bb, s16 pwr_dbm,
			 enum phl_phy_idx phy_idx)
{
	bool rpt = true;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_set_txpwr_dbm_8852a(bb, pwr_dbm, phy_idx);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_set_txpwr_dbm_8852a_2(bb, pwr_dbm, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_set_txpwr_dbm_8852b(bb, pwr_dbm, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_set_txpwr_dbm_8852c(bb, pwr_dbm, phy_idx);
		break;
	#endif

	default:
		rpt = false;
		break;
	}

	return rpt;
}

s16 halbb_get_txpwr_dbm(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	s16 rpt;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_get_txpwr_dbm_8852a(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_get_txpwr_dbm_8852a_2(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_get_txpwr_dbm_8852b(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_get_txpwr_dbm_8852c(bb, phy_idx);
		break;
	#endif

	default:
		rpt = false;
		break;
	}

	return rpt;
}

s16 halbb_get_txinfo_txpwr_dbm(struct bb_info *bb)
{
	s16 rpt;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_get_txinfo_txpwr_dbm_8852a(bb);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_get_txinfo_txpwr_dbm_8852a_2(bb);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_get_txinfo_txpwr_dbm_8852b(bb);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_get_txinfo_txpwr_dbm_8852c(bb);
		break;
	#endif
	default:
		rpt = false;
		break;
	}

	return rpt;
}

bool halbb_set_cck_txpwr_idx(struct bb_info *bb, u16 pwr_idx,
			     enum rf_path tx_path)
{
	bool rpt = true;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_set_cck_txpwr_idx_8852a(bb, pwr_idx, tx_path);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_set_cck_txpwr_idx_8852a_2(bb, pwr_idx, tx_path);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_set_cck_txpwr_idx_8852b(bb, pwr_idx, tx_path);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_set_cck_txpwr_idx_8852c(bb, pwr_idx, tx_path);
		break;
	#endif
	default:
		rpt = false;
		break;
	}

	return rpt;
}

u16 halbb_get_cck_txpwr_idx(struct bb_info *bb, enum rf_path tx_path)
{
	u16 rpt;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_get_cck_txpwr_idx_8852a(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_get_cck_txpwr_idx_8852a_2(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_get_cck_txpwr_idx_8852b(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_get_cck_txpwr_idx_8852c(bb, tx_path);
		break;
	#endif
	
	default:
		rpt= false;
		break;
	}
	return rpt;
}

s16 halbb_get_cck_ref_dbm(struct bb_info *bb, enum rf_path tx_path)
{
	s16 rpt;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_get_cck_ref_dbm_8852a(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_get_cck_ref_dbm_8852a_2(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_get_cck_ref_dbm_8852b(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_get_cck_ref_dbm_8852c(bb, tx_path);
		break;
	#endif

	default:
		rpt= false;
		break;
	}
	return rpt;
}

bool halbb_set_ofdm_txpwr_idx(struct bb_info *bb, u16 pwr_idx,
			      enum rf_path tx_path)
{
	bool rpt = true;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_set_ofdm_txpwr_idx_8852a(bb, pwr_idx, tx_path);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_set_ofdm_txpwr_idx_8852a_2(bb, pwr_idx, tx_path);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_set_ofdm_txpwr_idx_8852b(bb, pwr_idx, tx_path);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_set_ofdm_txpwr_idx_8852c(bb, pwr_idx, tx_path);
		break;
	#endif
	
	default:
		rpt = false;
		break;
	}

	return rpt;
}

u16 halbb_get_ofdm_txpwr_idx(struct bb_info *bb, enum rf_path tx_path)
{
	u16 rpt;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_get_ofdm_txpwr_idx_8852a(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_get_ofdm_txpwr_idx_8852a_2(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_get_ofdm_txpwr_idx_8852b(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_get_ofdm_txpwr_idx_8852c(bb, tx_path);
		break;
	#endif
	
	default:
		rpt= false;
		break;
	}
	return rpt;
}

s16 halbb_get_ofdm_ref_dbm(struct bb_info *bb, enum rf_path tx_path)
{
	s16 rpt;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_get_ofdm_ref_dbm_8852a(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_get_ofdm_ref_dbm_8852a_2(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_get_ofdm_ref_dbm_8852b(bb, tx_path);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_get_ofdm_ref_dbm_8852c(bb, tx_path);
		break;
	#endif
	
	default:
		rpt= false;
		break;
	}
	return rpt;
}


/*============================ [Others] =============================*/
bool halbb_chk_tx_idle(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	bool rpt = true;

	switch (bb->ic_type) {

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_chk_tx_idle_8852a_2(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_chk_tx_idle_8852c(bb, phy_idx);
		break;
	#endif

	default:
		rpt = false;
		break;
	}

	return rpt;
}

void halbb_dpd_bypass(struct bb_info *bb, bool pdp_bypass,
		      enum phl_phy_idx phy_idx)
{
	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		halbb_dpd_bypass_8852a_2(bb, pdp_bypass, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		//halbb_dpd_bypass_8852b(bb, pdp_bypass, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		halbb_dpd_bypass_8852c(bb, pdp_bypass, phy_idx);
		break;
	#endif
	
	default:
		break;
	}
}

void halbb_backup_info(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	switch (bb->ic_type) {

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		halbb_backup_info_8852a_2(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		halbb_backup_info_8852b(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		halbb_backup_info_8852c(bb, phy_idx);
		break;
	#endif

	default:
		break;
	}
}

void halbb_restore_info(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	BB_DBG(bb, DBG_PHY_CONFIG, "<====== %s ======>\n", __func__);

	switch (bb->ic_type) {

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		halbb_restore_info_8852a_2(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		halbb_restore_info_8852b(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		halbb_restore_info_8852c(bb, phy_idx);
		break;
	#endif

	default:
		break;
	}
}

bool halbb_set_txsc(struct bb_info *bb, u8 txsc, enum phl_phy_idx phy_idx)
{
	bool rpt = true;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		rpt = halbb_set_txsc_8852a(bb, txsc, phy_idx);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		rpt = halbb_set_txsc_8852a_2(bb, txsc, phy_idx);
		break;
	#endif
	
	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		rpt = halbb_set_txsc_8852b(bb, txsc, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		rpt = halbb_set_txsc_8852c(bb, txsc, phy_idx);
		break;
	#endif
	
	default:
		rpt = false;
		break;
	}

	return rpt;
}

bool halbb_set_bss_color(struct bb_info *bb, u8 bss_color, 
			 enum phl_phy_idx phy_idx)
{
	bool rpt = true;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
		case BB_RTL8852AA:
			rpt = halbb_set_bss_color_8852a(bb, bss_color, phy_idx);
			break;
	#endif
	
	#ifdef BB_8852A_2_SUPPORT
		case BB_RTL8852A:
			rpt = halbb_set_bss_color_8852a_2(bb, bss_color, phy_idx);
			break;
	#endif

	#ifdef BB_8852B_SUPPORT
		case BB_RTL8852B:
			rpt = halbb_set_bss_color_8852b(bb, bss_color, phy_idx);
			break;
	#endif

	#ifdef BB_8852C_SUPPORT
		case BB_RTL8852C:
			rpt = halbb_set_bss_color_8852c(bb, bss_color, phy_idx);
			break;
	#endif

	default:
		rpt = false;
		break;
	}

	return rpt;
}

bool halbb_set_sta_id(struct bb_info *bb, u16 sta_id, enum phl_phy_idx phy_idx)
{
	bool rpt = true;

	switch (bb->ic_type) {

	#ifdef BB_8852A_CAV_SUPPORT
		case BB_RTL8852AA:
			rpt = halbb_set_sta_id_8852a(bb, sta_id, phy_idx);
			break;
	#endif
	
	#ifdef BB_8852A_2_SUPPORT
		case BB_RTL8852A:
			rpt = halbb_set_sta_id_8852a_2(bb, sta_id, phy_idx);
			break;
	#endif

	#ifdef BB_8852B_SUPPORT
		case BB_RTL8852B:
			rpt = halbb_set_sta_id_8852b(bb, sta_id, phy_idx);
			break;
	#endif

	#ifdef BB_8852C_SUPPORT
		case BB_RTL8852C:
			rpt = halbb_set_sta_id_8852c(bb, sta_id, phy_idx);
			break;
	#endif

	default:
		rpt = false;
		break;
	}

	return rpt;
}

void halbb_pmac_tx_dbg(struct bb_info *bb, char input[][16], u32 *_used,
		       char *output, u32 *_out_len)
{
	u32 val[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;

	if (_os_strcmp(input[1], "-h") == 0) {
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			  "{bw} {pri_ch bw} {phy_idx}\n");

	} else if (_os_strcmp(input[1], "tx_path") == 0) {
		HALBB_SCAN(input[1], DCMD_DECIMAL, &val[0]);

		//halbb_set_pmac_tx_path(bb, (enum bb_path)val[0]);
		
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "Cfg Tx Path API \n");
	}
	*_used = used;
	*_out_len = out_len;
}

