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

#ifdef HALBB_RUA_SUPPORT

u32 halbb_rua_tbl_hdr_cfg(struct bb_info *bb, 
			struct rtw_rua_tbl_hdr *rtw_tbl_hdr,
			struct halbb_rua_tbl_hdr_info *rua_tbl_hdr)
{
	u32 ret = RTW_HAL_STATUS_SUCCESS;
	
	/*struct rtw_rua_tbl *rtw_rua = &bb->rtw_rua_t;*/
	BB_DBG(bb, DBG_RUA_TBL, "halbb_rau_tbl_hdr_cfg\n");
	rua_tbl_hdr->idx = rtw_tbl_hdr->idx;
	rua_tbl_hdr->rw = rtw_tbl_hdr->rw;
	rua_tbl_hdr->len_l= (u8)(rtw_tbl_hdr->len&0x0007);
	rua_tbl_hdr->len_m= (u8)((rtw_tbl_hdr->len&0x03f8)>>3);
	rua_tbl_hdr->offset= (u8)rtw_tbl_hdr->offset;
	rua_tbl_hdr->type = (u8)rtw_tbl_hdr->type;
	rua_tbl_hdr->tbl_class = rtw_tbl_hdr->tbl_class;

	return ret;
}

void halbb_ru_rate_cfg(struct bb_info *bb, 
			struct rtw_ru_rate_ent *rate_ent,
			struct halbb_ru_rate_info *rate_i)
{
	BB_DBG(bb, DBG_RUA_TBL, "halbb_ru_rate_cfg\n");
	rate_i->dcm = rate_ent->dcm;
	rate_i->mcs = rate_ent->mcs;
	rate_i->ss = rate_ent->ss;
}

u32 halbb_dlfix_sta_i_ax4ru_cfg(struct bb_info *bb, 
			struct rtw_dlfix_sta_i_ax4ru *sta_ent,
			struct halbb_dl_fix_sta_info *fix_sta_i)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 i;
	
	/*struct rtw_rua_tbl *rtw_rua = &bb->rtw_rua_t;*/
	BB_DBG(bb, DBG_RUA_TBL, "halbb_rua_sta_info_cfg\n");

	if ((!fix_sta_i) || (!sta_ent)) {
		BB_WARNING("halbb_rua_sta_info_cfg: NULL pointer!!\n");
		return ret;
	}

	fix_sta_i->mac_id = sta_ent->mac_id;
	for (i = 0; i < 3; i++)
		fix_sta_i->ru_pos[i] = sta_ent->ru_pos[i];
	fix_sta_i->fix_rate = sta_ent->fix_rate;
	fix_sta_i->fix_coding = sta_ent->fix_coding;
	fix_sta_i->fix_txbf = sta_ent->fix_txbf;
	fix_sta_i->fix_pwr_fac = sta_ent->fix_pwr_fac;
	halbb_ru_rate_cfg(bb, &(sta_ent->rate), &(fix_sta_i->rate_i));
	fix_sta_i->txbf = sta_ent->txbf;
	fix_sta_i->coding = sta_ent->coding;
	fix_sta_i->pwr_boost_fac = sta_ent->pwr_boost_fac;
	ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}

u32 halbb_ulfix_sta_i_ax4ru_cfg(struct bb_info *bb, 
			struct rtw_ulfix_sta_i_ax4ru *sta_ent,
			struct halbb_ul_fix_sta_info *fix_sta_i)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 i;
	
	/*struct rtw_rua_tbl *rtw_rua = &bb->rtw_rua_t;*/
	BB_DBG(bb, DBG_RUA_TBL, "halbb_rua_sta_info_cfg\n");

	if ((!fix_sta_i) || (!sta_ent)) {
		BB_WARNING("halbb_rua_sta_info_cfg: NULL pointer!!\n");
		return ret;
	}

	fix_sta_i->mac_id = sta_ent->mac_id;
	for (i = 0; i < 3; i++) {
		fix_sta_i->ru_pos[i] = sta_ent->ru_pos[i];
		fix_sta_i->tgt_rssi[i] = sta_ent->tgt_rssi[i];
	}
	fix_sta_i->fix_tgt_rssi = sta_ent->fix_tgt_rssi;
	fix_sta_i->fix_rate = sta_ent->fix_rate;
	fix_sta_i->fix_coding = sta_ent->fix_coding;
	fix_sta_i->coding = sta_ent->coding;
	halbb_ru_rate_cfg(bb, &(sta_ent->rate), &(fix_sta_i->rate_i));
	ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}

u32 halbb_tf_ba_tbl_info_cfg(struct bb_info *bb, 
			struct rtw_tf_ba_tbl *tf_tbl,
			struct halbb_tf_ba_tbl_info *tf_i)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	
	/*struct rtw_rua_tbl *rtw_rua = &bb->rtw_rua_t;*/
	BB_DBG(bb, DBG_RUA_TBL, "halbb_rua_sta_info_cfg\n");

	if ((!tf_i) || (!tf_tbl)) {
		BB_WARNING("halbb_tf_ba_tbl_info_cfg: NULL pointer!!\n");
		return ret;
	}
	tf_i->fix_ba = (u8)tf_tbl->fix_ba;
	tf_i->ru_psd_l = (u8)(tf_tbl->ru_psd&0x007f);
	tf_i->ru_psd_m = (u8)((tf_tbl->ru_psd&0x0180)>>7);	
	tf_i->tf_rate_l = (u8)(tf_tbl->tf_rate&0x003f);
	tf_i->tf_rate_m = (u8)((tf_tbl->tf_rate&0x01c0)>>6);
	tf_i->rf_gain_fix = (u8)tf_tbl->rf_gain_fix;
	tf_i->rf_gain_idx_l = (u8)(tf_tbl->rf_gain_idx&0x0000000f);
	tf_i->rf_gain_idx_m = (u8)((tf_tbl->rf_gain_idx&0x000003f0)>>4);
	tf_i->tb_ppdu_bw = (u8)tf_tbl->tb_ppdu_bw;
	halbb_ru_rate_cfg(bb, &(tf_tbl->rate), &(tf_i->rate_i));
	
	tf_i->gi_ltf = tf_tbl->gi_ltf;
	tf_i->doppler = tf_tbl->doppler;
	tf_i->stbc = tf_tbl->stbc;
	tf_i->sta_coding = tf_tbl->sta_coding;
	tf_i->tb_t_pe_nom = tf_tbl->tb_t_pe_nom;
	tf_i->pr20_bw_en = tf_tbl->pr20_bw_en;
	tf_i->ma_type = tf_tbl->ma_type;
	ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}

void halbb_rua_tbl_init(struct bb_info *bb)
{

	BB_DBG(bb, DBG_RUA_TBL, "RUA TBL Init\n");

}

u32 halbb_upd_dlru_fixtbl_ax4ru(struct bb_info *bb,
			struct rtw_dlru_fixtbl_ax4ru *info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 i;
	bool ret_v= false;
	u8 len = sizeof(struct rtw_dlru_fixtbl_ax4ru);
	struct halbb_dl_ru_fix_tbl_info *fix_tbl_i;
	u8 pkt_len = sizeof(struct halbb_dl_ru_fix_tbl_info);
	u32 *bb_h2c = NULL;
	/*u8 *buf;*/

	BB_DBG(bb, DBG_RUA_TBL, "halbb_upd_dlru_fixtbl: in_len = %d, out_len = %d\n", len, pkt_len);
	//if (len != pkt_len)
	//	BB_WARNING("halbb_upd_dlru_fixtbl: tble length mismatch!!\n");

	fix_tbl_i = hal_mem_alloc(bb->hal_com, pkt_len);
	
	bb_h2c = (u32 *) fix_tbl_i;
	info->tbl_hdr.len= sizeof(struct halbb_dl_ru_fix_tbl_info)-sizeof(struct halbb_rua_tbl_hdr_info);
	info->tbl_hdr.tbl_class = DL_RU_FIX_TBL;
	ret = halbb_rua_tbl_hdr_cfg(bb, &(info->tbl_hdr), &(fix_tbl_i->tbl_hdr_i));
	if (ret == RTW_HAL_STATUS_FAILURE)
		goto out;
	fix_tbl_i->max_sta_num = info->max_sta_num;
	fix_tbl_i->min_sta_num = info->min_sta_num;
	fix_tbl_i->doppler = info->doppler;
	fix_tbl_i->stbc = info->stbc;
	fix_tbl_i->gi_ltf = info->gi_ltf;
	fix_tbl_i->ma_type = info->ma_type;
	fix_tbl_i->fixru_flag = info->fixru_flag;
	fix_tbl_i->rupos_csht_flag = info->rupos_csht_flag;
	fix_tbl_i->ru_swp_flg = info->ru_swp_flg;
	for (i = 0; i < HALBB_AX4RU_STA_NUM; i++)
		halbb_dlfix_sta_i_ax4ru_cfg(bb, &(info->sta[i]), &(fix_tbl_i->sta_i[i]));
	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_TABLE, HALBB_H2C_RUA, bb_h2c);
out:
	if (fix_tbl_i)
		hal_mem_free(bb->hal_com, fix_tbl_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}

u32 halbb_upd_dlru_fixtbl(struct bb_info *bb,
			union rtw_dlru_fixtbl *union_info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	switch (bb->ic_type) {

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		ret = halbb_upd_dlru_fixtbl_ax4ru(bb, &(union_info->ax4ru));
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		ret = halbb_upd_dlru_fixtbl_ax4ru(bb, &(union_info->ax4ru));
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		ret = halbb_upd_dlru_fixtbl_ax4ru(bb, &(union_info->ax4ru));
		break;
	#endif

	default:
		ret = RTW_HAL_STATUS_FAILURE;
		break;
	}

	return ret;
}

u32 halbb_upd_ulru_fixtbl_ax4ru(struct bb_info *bb,
			struct rtw_ulru_fixtbl_ax4ru *info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 i;
	bool ret_v= false;
	u8 len = sizeof(struct rtw_ulru_fixtbl_ax4ru);
	struct halbb_ul_ru_fix_tbl_info *fix_tbl_i;
	u8 pkt_len = sizeof(struct halbb_ul_ru_fix_tbl_info);
	u32 *bb_h2c = NULL;
	/*u8 *buf;*/

	BB_DBG(bb, DBG_RUA_TBL, "halbb_upd_ulru_fixtbl: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_upd_ulru_fixtbl: tble length mismatch!!\n");

	fix_tbl_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) fix_tbl_i;
	info->tbl_hdr.len= sizeof(struct halbb_ul_ru_fix_tbl_info)-sizeof(struct halbb_rua_tbl_hdr_info);
	info->tbl_hdr.tbl_class = UL_RU_FIX_TBL;
	ret = halbb_rua_tbl_hdr_cfg(bb, &(info->tbl_hdr), &(fix_tbl_i->tbl_hdr_i));
	if (ret == RTW_HAL_STATUS_FAILURE)
		goto out;

	fix_tbl_i->max_sta_num = info->max_sta_num;
	fix_tbl_i->min_sta_num = info->min_sta_num;
	fix_tbl_i->doppler = info->doppler;
	fix_tbl_i->ma_type = info->ma_type;
	fix_tbl_i->gi_ltf = info->gi_ltf;
	fix_tbl_i->stbc = info->stbc;
	fix_tbl_i->fix_tb_t_pe_nom = info->fix_tb_t_pe_nom;
	fix_tbl_i->tb_t_pe_nom = info->tb_t_pe_nom;
	fix_tbl_i->fixru_flag = info->fixru_flag;
	for (i = 0; i < HALBB_AX4RU_STA_NUM; i++)
		halbb_ulfix_sta_i_ax4ru_cfg(bb, &(info->sta[i]), &(fix_tbl_i->sta_i[i]));
	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_TABLE, HALBB_H2C_RUA, bb_h2c);
out:
	if (fix_tbl_i)
		hal_mem_free(bb->hal_com, fix_tbl_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}


u32 halbb_upd_ulru_fixtbl(struct bb_info *bb,
			union rtw_ulru_fixtbl *union_info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	switch (bb->ic_type) {

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		ret = halbb_upd_ulru_fixtbl_ax4ru(bb, &(union_info->ax4ru));
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		ret = halbb_upd_ulru_fixtbl_ax4ru(bb, &(union_info->ax4ru));
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		ret = halbb_upd_ulru_fixtbl_ax4ru(bb, &(union_info->ax4ru));
		break;
	#endif

	default:
		ret = RTW_HAL_STATUS_FAILURE;
		break;
	}

	return ret;
}

u32 halbb_upd_dlru_grptbl(struct bb_info *bb,
			struct rtw_dl_ru_gp_tbl *info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	bool ret_v= false;
	u8 len = sizeof(struct rtw_dl_ru_gp_tbl);
	struct halbb_dl_ru_gp_tbl_info *gp_tbl_i;
	u8 pkt_len = sizeof(struct halbb_dl_ru_gp_tbl_info);
	u32 *bb_h2c = NULL;
	
	BB_DBG(bb, DBG_RUA_TBL, "halbb_upd_dlru_grptbl: in_len = %d, out_len = %d\n", len, pkt_len);
	//if (len != pkt_len)
	//	BB_WARNING("halbb_upd_dlru_grptbl: tble length mismatch!!\n");
	gp_tbl_i = hal_mem_alloc(bb->hal_com, pkt_len);
	bb_h2c=(u32 *) gp_tbl_i;
	info->tbl_hdr.len= sizeof(struct halbb_dl_ru_gp_tbl_info)-sizeof(struct halbb_rua_tbl_hdr_info);
	info->tbl_hdr.tbl_class = DL_RU_GP_TBL;
	ret = halbb_rua_tbl_hdr_cfg(bb, &(info->tbl_hdr), &(gp_tbl_i->tbl_hdr_i));
	if (ret == RTW_HAL_STATUS_FAILURE)
		goto out;
	gp_tbl_i->ppdu_bw = (u8)info->ppdu_bw;
	gp_tbl_i->tx_pwr_l = (u8)(info->tx_pwr&0x003f);
	gp_tbl_i->tx_pwr_m = (u8)((info->tx_pwr&0x01c0)>>6);
	gp_tbl_i->pwr_boost_fac = (u8)info->pwr_boost_fac;
	gp_tbl_i->fix_mode_flag = info->fix_mode_flag;
	ret = halbb_tf_ba_tbl_info_cfg(bb, &(info->tf), &(gp_tbl_i->tf));
	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_TABLE, HALBB_H2C_RUA, bb_h2c);
out:
	if (gp_tbl_i)
		hal_mem_free(bb->hal_com, gp_tbl_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}

u32 halbb_upd_ulru_grptbl(struct bb_info *bb,
			struct rtw_ul_ru_gp_tbl *info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	bool ret_v= false;
	u8 len = sizeof(struct rtw_ul_ru_gp_tbl);
	struct halbb_ul_ru_gp_tbl_info *gp_tbl_i;
	u8 pkt_len = sizeof(struct halbb_ul_ru_gp_tbl_info);
	u32 *bb_h2c = NULL;
	
	BB_DBG(bb, DBG_RUA_TBL, "halbb_upd_ulru_grptbl: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_upd_ulru_grptbl: tble length mismatch!!\n");
	gp_tbl_i = hal_mem_alloc(bb->hal_com, pkt_len);
	bb_h2c = (u32 *) gp_tbl_i;

	info->tbl_hdr.len= sizeof(struct halbb_ul_ru_gp_tbl_info)-sizeof(struct halbb_rua_tbl_hdr_info);
	info->tbl_hdr.tbl_class = UL_RU_GP_TBL;
	ret = halbb_rua_tbl_hdr_cfg(bb, &(info->tbl_hdr), &(gp_tbl_i->tbl_hdr_i));
	if (ret == RTW_HAL_STATUS_FAILURE)
		goto out;

	gp_tbl_i->grp_psd_max_l = (u8)(info->grp_psd_max&0x00ff);
	gp_tbl_i->grp_psd_max_m = (u8)((info->grp_psd_max>>8)&0x0001);
	gp_tbl_i->grp_psd_min_l = (u8)(info->grp_psd_min&0x007f);
	gp_tbl_i->grp_psd_min_m = (u8)((info->grp_psd_min>>7)&0x0003);
	gp_tbl_i->tf_rate_l = (u8)(info->tf_rate&0x003f);
	gp_tbl_i->tf_rate_m = (u8)((info->tf_rate>>6)&0x0007);
	gp_tbl_i->fix_tf_rate = (u8)info->fix_tf_rate;
	gp_tbl_i->ppdu_bw = (u8)info->ppdu_bw;
	gp_tbl_i->rf_gain_fix = (u8)info->rf_gain_fix;
	gp_tbl_i->rf_gain_idx_l = (u8)(info->rf_gain_idx&0x001f);
	gp_tbl_i->rf_gain_idx_m = (u8)((info->rf_gain_idx>>5)&0x001f);
	gp_tbl_i->fix_mode_flag = (u8)info->fix_mode_flag;
	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_TABLE, HALBB_H2C_RUA, bb_h2c);
out:
	if (gp_tbl_i)
		hal_mem_free(bb->hal_com, gp_tbl_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}

u32 halbb_upd_rusta_info(struct bb_info *bb,
		       struct rtw_ru_sta_info *info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_ru_sta_info);
	struct halbb_ru_sta_info *ru_sta_i;
	u8 pkt_len = sizeof(struct halbb_ru_sta_info);
	u32 *bb_h2c = NULL;
	u8 i = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_upd_rusta_info: in_len = %d, out_len = %d\n", len, pkt_len);
	//if (len != pkt_len)
	//	BB_WARNING("halbb_upd_rusta_info: tble length mismatch!!\n");
	ru_sta_i = hal_mem_alloc(bb->hal_com, pkt_len);
	
	bb_h2c=(u32 *) ru_sta_i;

	info->tbl_hdr.len = sizeof(struct halbb_ru_sta_info)-sizeof(struct halbb_rua_tbl_hdr_info);
	info->tbl_hdr.tbl_class = RU_STA_INFO;
	
	ret = halbb_rua_tbl_hdr_cfg(bb, &(info->tbl_hdr), &(ru_sta_i->tbl_hdr_i));
	if (ret == RTW_HAL_STATUS_FAILURE)
		goto out;
	ru_sta_i->gi_ltf_48spt = info->gi_ltf_48spt;
	ru_sta_i->gi_ltf_18spt = info->gi_ltf_18spt;
	ru_sta_i->dlsu_info_en = info->dlsu_info_en;
	ru_sta_i->dlsu_bw = info->dlsu_bw;
	ru_sta_i->dlsu_gi_ltf = info->dlsu_gi_ltf;
	ru_sta_i->dlsu_doppler_ctrl = info->dlsu_doppler_ctrl;
	ru_sta_i->dlsu_coding = info->dlsu_coding;
	ru_sta_i->dlsu_txbf = info->dlsu_txbf;
	ru_sta_i->dlsu_stbc = info->dlsu_stbc;
	ru_sta_i->dl_fwcqi_flag = info->dl_fwcqi_flag;
	ru_sta_i->dlru_ratetbl_ridx = info->dlru_ratetbl_ridx;
	ru_sta_i->csi_info_bitmap = info->csi_info_bitmap;
	for (i = 0; i < 4; i++)
		ru_sta_i->dl_swgrp_bitmap[i] = (u8)((info->dl_swgrp_bitmap) >> (i<<3))&0xff;
	ru_sta_i->dlsu_dcm = (u8)info->dlsu_dcm;
	ru_sta_i->dlsu_rate_l = (u8)(info->dlsu_rate&0x0001);
	ru_sta_i->dlsu_rate_m = (u8)((info->dlsu_rate>>1)&0x00ff);
	ru_sta_i->dlsu_pwr = info->dlsu_pwr;
	ru_sta_i->ulsu_info_en = info->ulsu_info_en;
	ru_sta_i->ulsu_bw = info->ulsu_bw;
	ru_sta_i->ulsu_gi_ltf = info->ulsu_gi_ltf;
	ru_sta_i->ulsu_doppler_ctrl = info->ulsu_doppler_ctrl;
	ru_sta_i->ulsu_dcm = info->ulsu_dcm;
	ru_sta_i->ulsu_ss = info->ulsu_ss;
	ru_sta_i->ulsu_mcs = info->ulsu_mcs;
	ru_sta_i->ul_fwcqi_flag = (u8)info->ul_fwcqi_flag;
	ru_sta_i->ulru_ratetbl_ridx = (u8)info->ulru_ratetbl_ridx;
	ru_sta_i->ulsu_stbc = (u8)info->ulsu_stbc;
	ru_sta_i->ulsu_coding = (u8)info->ulsu_coding;
	ru_sta_i->ulsu_rssi_m_l = (u8)(info->ulsu_rssi_m&0x0001);
	ru_sta_i->ulsu_rssi_m_m = (u8)((info->ulsu_rssi_m>>1)&0x00ff);
	for (i = 0; i < 4; i++)
		ru_sta_i->ul_swgrp_bitmap[i] = (u8)((info->ul_swgrp_bitmap) >> (i<<3))&0xff;	

	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_TABLE, HALBB_H2C_RUA, bb_h2c);

out:
	if (ru_sta_i)
		hal_mem_free(bb->hal_com, ru_sta_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}

u32 halbb_upd_ba_infotbl(struct bb_info *bb,
		       struct rtw_ba_tbl_info *info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_ba_tbl_info);
	struct halbb_ba_tbl_info *ba_i;
	u8 pkt_len = sizeof(struct halbb_ba_tbl_info);
	u32 *bb_h2c = NULL;
	u8 i = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_upd_ba_infotbl: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_upd_ba_infotbl: tble length mismatch!!\n");
	ba_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) ba_i;
	
	info->tbl_hdr.len = sizeof(struct halbb_ba_tbl_info)-sizeof(struct halbb_rua_tbl_hdr_info);
	info->tbl_hdr.tbl_class = BA_INFO_TBL;
	
	ret = halbb_rua_tbl_hdr_cfg(bb, &(info->tbl_hdr), &(ba_i->tbl_hdr_i));
	if (ret == RTW_HAL_STATUS_FAILURE)
		goto out;	
	ret = halbb_tf_ba_tbl_info_cfg( bb, &(info->tf_ba_t), &(ba_i->tf_i));
	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_TABLE, HALBB_H2C_RUA, bb_h2c);
	
out:
	if (ba_i)
		hal_mem_free(bb->hal_com, ba_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;
}

u32 halbb_swgrp_hdl(struct bb_info *bb, struct rtw_sw_grp_set *info)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_sw_grp_set);
	struct halbb_sw_grp_set *swgrp_i;
	u8 pkt_len = sizeof(struct halbb_sw_grp_set);
	u32 *bb_h2c = NULL;
	u8 i,j = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_swgrp_hdl: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_swgrp_hdl: tble length mismatch!!\n");
	swgrp_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) swgrp_i;

	/*
	info->tbl_hdr.len = sizeof(struct halbb_tf_ba_tbl_info)-sizeof(struct halbb_rua_tbl_hdr_info);
	info->tbl_hdr.tbl_class = BA_INFO_TBL;
	
	ret = halbb_rua_tbl_hdr_cfg(bb, &(info->tbl_hdr), &(ba_i->tbl_hdr_i));
	if (ret == RTW_HAL_STATUS_FAILURE)
		goto out;
	*/
	for (i = 0; i < 8; i++) {
		swgrp_i->swgrp_bitmap[i].macid= info->swgrp_bitmap[i].macid;
		swgrp_i->swgrp_bitmap[i].en_upd_dl_swgrp = info->swgrp_bitmap[i].en_upd_dl_swgrp;
		swgrp_i->swgrp_bitmap[i].en_upd_ul_swgrp = info->swgrp_bitmap[i].en_upd_ul_swgrp;
		for (j = 0; j < 4; j++) {
			swgrp_i->swgrp_bitmap[i].dl_sw_grp_bitmap[j] = (u8)((info->swgrp_bitmap[i].dl_sw_grp_bitmap) >> (j<<3))&0xff;
			swgrp_i->swgrp_bitmap[i].ul_sw_grp_bitmap[j] = (u8)((info->swgrp_bitmap[i].ul_sw_grp_bitmap) >> (j<<3))&0xff;
		}
		swgrp_i->swgrp_bitmap[i].cmdend= info->swgrp_bitmap[i].cmdend;
		if (swgrp_i->swgrp_bitmap[i].cmdend)
			break;
	}
	
	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c+1), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_SWGRP, HALBB_H2C_RUA, bb_h2c);
	
//out:
	if (swgrp_i)
		hal_mem_free(bb->hal_com, swgrp_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;

}

u32 halbb_dlmacid_cfg(struct bb_info *bb, struct rtw_dl_macid_cfg *cfg)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_dl_macid_cfg);
	struct dl_macid_cfg *dlmac_i;
	u8 pkt_len = sizeof(struct dl_macid_cfg);
	u32 *bb_h2c = NULL;
	//u8 i,j = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_dlmacid_cfg: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_dlmacid_cfg: tble length mismatch!!\n");
	dlmac_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) dlmac_i;

	dlmac_i->macid = (u8)cfg->macid;
	
	dlmac_i->dl_su_rate_cfg = (u8)cfg->dl_su_rate_cfg;
	dlmac_i->dl_su_rate_l = (u8)(cfg->dl_su_rate & 0x7f);
	dlmac_i->dl_su_rate_m= (u8)(cfg->dl_su_rate & 0x180)>>7;
	dlmac_i->dl_su_bw = (u8)cfg->dl_su_bw;
	
	dlmac_i->dl_su_pwr_cfg = (u8)cfg->dl_su_pwr_cfg;
	dlmac_i->dl_su_pwr_l= (u8)(cfg->dl_su_pwr & 0x7);
	dlmac_i->dl_su_pwr_m= (u8)(cfg->dl_su_pwr & 0x38)>>3;
	
	dlmac_i->gi_ltf_4x8_support = (u8)cfg->gi_ltf_4x8_support;
	dlmac_i->gi_ltf_1x8_support = (u8)cfg->gi_ltf_1x8_support;
	
	dlmac_i->dl_su_info_en = (u8)cfg->dl_su_info_en;
	
	dlmac_i->dl_su_gi_ltf = (u8)cfg->dl_su_gi_ltf;
	dlmac_i->dl_su_doppler_ctrl = (u8)cfg->dl_su_doppler_ctrl;
	dlmac_i->dl_su_coding = (u8)cfg->dl_su_coding;
	dlmac_i->dl_su_txbf = (u8)cfg->dl_su_txbf;
	dlmac_i->dl_su_stbc = (u8)cfg->dl_su_stbc;
	dlmac_i->dl_su_dcm = (u8)cfg->dl_su_dcm;

	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c+1), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_DL_MACID, HALBB_H2C_RUA, bb_h2c);	
//out:
	if (dlmac_i)
		hal_mem_free(bb->hal_com, dlmac_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;

}


u32 halbb_ulmacid_cfg(struct bb_info *bb, struct rtw_ul_macid_set *cfg)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_ul_macid_set);
	struct halbb_ul_macid_set *ulmac_i;
	u8 pkt_len = sizeof(struct halbb_ul_macid_set);
	u32 *bb_h2c = NULL;
	u8 i = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_ulmacid_cfg: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_ulmacid_cfg: tble length mismatch!!\n");
	ulmac_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) ulmac_i;

	for (i = 0; i < 8; i++) {
		ulmac_i->ul_macid_cfg[i].macid= (u8)cfg->ul_macid_cfg[i].macid;
		ulmac_i->ul_macid_cfg[i].endcmd = (u8)cfg->ul_macid_cfg[i].endcmd;
		
		ulmac_i->ul_macid_cfg[i].ul_su_info_en = (u8)cfg->ul_macid_cfg[i].ul_su_info_en;

		ulmac_i->ul_macid_cfg[i].ul_su_bw = (u8)cfg->ul_macid_cfg[i].ul_su_bw;
		ulmac_i->ul_macid_cfg[i].ul_su_gi_ltf= (u8)cfg->ul_macid_cfg[i].ul_su_gi_ltf;
		ulmac_i->ul_macid_cfg[i].ul_su_doppler_ctrl = (u8)cfg->ul_macid_cfg[i].ul_su_doppler_ctrl;
		ulmac_i->ul_macid_cfg[i].ul_su_dcm= (u8)cfg->ul_macid_cfg[i].ul_su_dcm;
		
		ulmac_i->ul_macid_cfg[i].ul_su_ss = (u8)cfg->ul_macid_cfg[i].ul_su_ss;
		ulmac_i->ul_macid_cfg[i].ul_su_mcs= (u8)cfg->ul_macid_cfg[i].ul_su_mcs;
		ulmac_i->ul_macid_cfg[i].ul_su_coding = (u8)cfg->ul_macid_cfg[i].ul_su_coding;
		ulmac_i->ul_macid_cfg[i].ul_su_rssi_m_l= (u8)(cfg->ul_macid_cfg[i].ul_su_rssi_m & 0x1);
		ulmac_i->ul_macid_cfg[i].ul_su_rssi_m_m= (u8)(cfg->ul_macid_cfg[i].ul_su_rssi_m & 0x1fe)>>1;
		
		if (ulmac_i->ul_macid_cfg[i].endcmd)
			break;
	}

	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c+1), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_UL_MACID, HALBB_H2C_RUA, bb_h2c);	
//out:
	if (ulmac_i)
		hal_mem_free(bb->hal_com, ulmac_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;

}

u32 halbb_csiinfo_cfg(struct bb_info *bb, struct rtw_csiinfo_cfg *cfg)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_csiinfo_cfg);
	struct csiinfo_cfg *csiinfo_i;
	u8 pkt_len = sizeof(struct csiinfo_cfg);
	u32 *bb_h2c = NULL;
	//u8 i,j = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_csiinfo_cfg: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_csiinfo_cfg: tble length mismatch!!\n");
	csiinfo_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) csiinfo_i;

	csiinfo_i->macid = (u8)cfg->macid;
	csiinfo_i->csi_info_bitmap = (u8)cfg->csi_info_bitmap;

	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c+1), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_CSIINFO, HALBB_H2C_RUA, bb_h2c);	
//out:
	if (csiinfo_i)
		hal_mem_free(bb->hal_com, csiinfo_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;

}


u32 halbb_cqi_cfg(struct bb_info *bb, struct rtw_cqi_set *cfg)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_cqi_set);
	struct halbb_cqi_set *cqi_i;
	u8 pkt_len = sizeof(struct halbb_cqi_set);
	u32 *bb_h2c = NULL;
	u8 i,j = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_ulmacid_cfg: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_ulmacid_cfg: tble length mismatch!!\n");
	cqi_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) cqi_i;

	for (i = 0; i < 8; i++) {
		cqi_i->cqi_info[i].macid= (u8)cfg->cqi_info[i].macid;
		cqi_i->cqi_info[i].fw_cqi_flag= (u8)cfg->cqi_info[i].fw_cqi_flag;
		cqi_i->cqi_info[i].ru_rate_table_row_idx= (u8)cfg->cqi_info[i].ru_rate_table_row_idx;
		cqi_i->cqi_info[i].ul_dl= (u8)cfg->cqi_info[i].ul_dl;
		cqi_i->cqi_info[i].endcmd = (u8)cfg->cqi_info[i].endcmd;
		
		for (j=0;j<19;j++)
			cqi_i->cqi_info[i].cqi_diff_table[j]= (u8)cfg->cqi_info[i].cqi_diff_table[j];
		
		if (cqi_i->cqi_info[i].endcmd)
			break;
	}

	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c+1), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_CQI, HALBB_H2C_RUA, bb_h2c);	
//out:
	if (cqi_i)
		hal_mem_free(bb->hal_com, cqi_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;

}

u32 halbb_bbinfo_cfg(struct bb_info *bb, struct rtw_bbinfo_cfg *cfg)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_bbinfo_cfg);
	struct halbb_bb_info_cfg *bbinfo_i;
	u8 pkt_len = sizeof(struct halbb_bb_info_cfg);
	u32 *bb_h2c = NULL;
	//u8 i,j = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_bbinfo_cfg: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_bbinfo_cfg: tble length mismatch!!\n");
	bbinfo_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) bbinfo_i;

	bbinfo_i->p20_ch_bitmap = (u8)cfg->p20_ch_bitmap;

	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c+1), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_BBINFO, HALBB_H2C_RUA, bb_h2c);	
//out:
	if (bbinfo_i)
		hal_mem_free(bb->hal_com, bbinfo_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;

}


u32 halbb_pbr_tbl_cfg(struct bb_info *bb, struct rtw_pwr_by_rt_tbl *cfg)
{
	u32 ret = RTW_HAL_STATUS_FAILURE;
	u8 len = sizeof(struct rtw_pwr_by_rt_tbl);
	struct halbb_pwr_by_rt_tbl *pbr_i;
	u8 pkt_len = sizeof(struct halbb_pwr_by_rt_tbl);
	u32 *bb_h2c = NULL;
	u8 i = 0;
	bool ret_v = false;

	BB_DBG(bb, DBG_RUA_TBL, "halbb_bbinfo_cfg: in_len = %d, out_len = %d\n", len, pkt_len);
	// if (len != pkt_len)
	// 	BB_WARNING("halbb_bbinfo_cfg: tble length mismatch!!\n");
	pbr_i = hal_mem_alloc(bb->hal_com, pkt_len);

	bb_h2c = (u32 *) pbr_i;
	
	for (i=0;i<32;i++){
		pbr_i->pwr_by_rt[2*i] = (u8)(cfg->pwr_by_rt[i] & 0xff);
		pbr_i->pwr_by_rt[2*i+1] = (u8)((cfg->pwr_by_rt[i] & 0xff00)>>8);
	}

	BB_DBG(bb, DBG_RUA_TBL, "content %x %x %x \n", *bb_h2c, *(bb_h2c+1), *(bb_h2c+2));
	ret_v = halbb_fill_h2c_cmd(bb, pkt_len, RUA_H2C_PWR_TBL, HALBB_H2C_RUA, bb_h2c);	
//out:
	if (pbr_i)
		hal_mem_free(bb->hal_com, pbr_i, pkt_len);
	if (ret_v)
		ret = RTW_HAL_STATUS_SUCCESS;
	return ret;

}

/* For Test mode */
void halbb_test_dlru_gp_tbl(struct bb_info *bb, struct rtw_dl_ru_gp_tbl *tbl)
{
	tbl->tbl_hdr.rw = 1; /* write */
	tbl->tbl_hdr.idx = 0;
	tbl->tbl_hdr.offset = 0;
	tbl->tbl_hdr.len = sizeof(struct rtw_dl_ru_gp_tbl);
	tbl->tbl_hdr.type = 0;
	tbl->ppdu_bw = 2;
	tbl->tx_pwr = 0x3c;
	tbl->pwr_boost_fac = 0;
	tbl->fix_mode_flag = 1;
	tbl->tf.fix_ba = 1;
	tbl->tf.ru_psd = 20;
	tbl->tf.tf_rate = 388;
	tbl->tf.rf_gain_fix = 0;
	tbl->tf.rf_gain_idx = 0;
	tbl->tf.tb_ppdu_bw = 2;
	tbl->tf.rate.dcm = 0;
	tbl->tf.rate.mcs = 7;
	tbl->tf.rate.ss = 0;
	tbl->tf.gi_ltf = 2;
	tbl->tf.doppler = 0;
	tbl->tf.stbc = 0;
	tbl->tf.sta_coding = 1;
	tbl->tf.tb_t_pe_nom = 2;
	tbl->tf.pr20_bw_en = 0;
	tbl->tf.ma_type = 0;
}

void halbb_test_dl_sta_ent0(struct bb_info *bb, struct rtw_dlfix_sta_i_ax4ru *sta_ent)
{
	sta_ent->mac_id = 0;
	sta_ent->ru_pos[0] = 122;
	sta_ent->ru_pos[1] = 122;
	sta_ent->ru_pos[2] = 122;
	sta_ent->fix_rate = 1;
	sta_ent->fix_coding = 1;
	sta_ent->fix_txbf = 1;
	sta_ent->fix_pwr_fac = 1;
	sta_ent->rate.dcm = 0;
	sta_ent->rate.mcs = 11;
	sta_ent->rate.ss = 1;
	sta_ent->txbf = 0;
	sta_ent->coding = 1;
	sta_ent->pwr_boost_fac = 0;
}

void halbb_test_dl_sta_ent1(struct bb_info *bb, struct rtw_dlfix_sta_i_ax4ru *sta_ent)
{
	sta_ent->mac_id = 1;
	sta_ent->ru_pos[0] = 124;
	sta_ent->ru_pos[1] = 124;
	sta_ent->ru_pos[2] = 124;
	sta_ent->fix_rate = 1;
	sta_ent->fix_coding = 1;
	sta_ent->fix_txbf = 1;
	sta_ent->fix_pwr_fac = 1;
	sta_ent->rate.dcm = 0;
	sta_ent->rate.mcs = 10;
	sta_ent->rate.ss = 1;
	sta_ent->txbf = 0;
	sta_ent->coding = 1;
	sta_ent->pwr_boost_fac = 0;
}

void halbb_test_dl_sta_ent2(struct bb_info *bb, struct rtw_dlfix_sta_i_ax4ru *sta_ent)
{
	sta_ent->mac_id = 2;
	sta_ent->ru_pos[0] = 0;
	sta_ent->ru_pos[1] = 126;
	sta_ent->ru_pos[2] = 126;
	sta_ent->fix_rate = 1;
	sta_ent->fix_coding = 1;
	sta_ent->fix_txbf = 1;
	sta_ent->fix_pwr_fac = 1;
	sta_ent->rate.dcm = 0;
	sta_ent->rate.mcs = 9;
	sta_ent->rate.ss = 1;
	sta_ent->txbf = 0;
	sta_ent->coding = 1;
	sta_ent->pwr_boost_fac = 0;
}

void halbb_test_dl_sta_ent3(struct bb_info *bb, struct rtw_dlfix_sta_i_ax4ru *sta_ent)
{
	sta_ent->mac_id = 255;
	sta_ent->ru_pos[0] = 0;
	sta_ent->ru_pos[1] = 0;
	sta_ent->ru_pos[2] = 128;
	sta_ent->fix_rate = 1;
	sta_ent->fix_coding = 1;
	sta_ent->fix_txbf = 1;
	sta_ent->fix_pwr_fac = 1;
	sta_ent->rate.dcm = 0;
	sta_ent->rate.mcs = 11;
	sta_ent->rate.ss = 0;
	sta_ent->txbf = 0;
	sta_ent->coding = 1;
	sta_ent->pwr_boost_fac = 0;
}

void halbb_test_dl_fix_tbl(struct bb_info *bb, struct rtw_dlru_fixtbl_ax4ru *tbl)
{
	tbl->tbl_hdr.rw = 1; /* write */
	tbl->tbl_hdr.idx = 0;
	tbl->tbl_hdr.offset = 0;
	tbl->tbl_hdr.len = sizeof(struct rtw_dlru_fixtbl_ax4ru);
	tbl->tbl_hdr.type = 0;
	/* Need finish */
	tbl->max_sta_num = 4;
	tbl->min_sta_num = 2;
	tbl->doppler = 0;
	tbl->stbc = 0;
	tbl->gi_ltf = 3;
	tbl->ma_type = 0;
	tbl->fixru_flag = 1;
	halbb_test_dl_sta_ent0(bb, &(tbl->sta[0]));
	halbb_test_dl_sta_ent1(bb, &(tbl->sta[1]));
	halbb_test_dl_sta_ent2(bb, &(tbl->sta[2]));
	halbb_test_dl_sta_ent3(bb, &(tbl->sta[3]));
}

void halbb_test_ru_sta_info(struct bb_info *bb, struct rtw_ru_sta_info *tbl)
{
	tbl->tbl_hdr.rw = 1; /* write */
	tbl->tbl_hdr.idx = 0;
	tbl->tbl_hdr.offset = 0;
	tbl->tbl_hdr.len = sizeof(struct rtw_ru_sta_info);
	tbl->tbl_hdr.type = 0;
	
	tbl->gi_ltf_18spt = 0;
	tbl->gi_ltf_48spt = 0;
	tbl->dlsu_info_en = 1;
	tbl->dlsu_bw = 2;
	tbl->dlsu_gi_ltf = 2;
	tbl->dlsu_doppler_ctrl = 0;
	tbl->dlsu_coding = 1;
	tbl->dlsu_txbf = 0;
	tbl->dlsu_stbc = 0;
	tbl->dl_fwcqi_flag = 0;
	tbl->dlru_ratetbl_ridx = 12;
	tbl->csi_info_bitmap = 0;
	tbl->dl_swgrp_bitmap = 1;
	tbl->dlsu_dcm = 0;
	tbl->dlsu_rate = 390;
	tbl->dlsu_pwr = 25;

	tbl->ulsu_info_en = 1;
	tbl->ulsu_bw = 2;
	tbl->ulsu_gi_ltf = 3;
	tbl->ulsu_doppler_ctrl = 0;
	tbl->ulsu_dcm = 0;
	tbl->ulsu_ss = 0;
	tbl->ulsu_mcs = 7;
	tbl->ul_fwcqi_flag = 1;
	tbl->ulru_ratetbl_ridx = 13;
	tbl->ulsu_stbc = 0;
	tbl->ulsu_coding = 1;
	tbl->ulsu_rssi_m = 200;
	tbl->ul_swgrp_bitmap = 2;
}

void halbb_test_ul_sta_ent0(struct bb_info *bb, struct rtw_ulfix_sta_i_ax4ru *sta_ent)
{
	sta_ent->mac_id = 0;
	sta_ent->ru_pos[0] = 122;
	sta_ent->ru_pos[1] = 122;
	sta_ent->ru_pos[2] = 122;

	sta_ent->fix_tgt_rssi=1;
	sta_ent->tgt_rssi[0] = 115;
	sta_ent->tgt_rssi[1] = 110;
	sta_ent->tgt_rssi[2] = 105;

	sta_ent->fix_rate = 1;
	sta_ent->fix_coding = 1;
	sta_ent->rate.dcm = 0;
	sta_ent->rate.mcs = 11;
	sta_ent->rate.ss = 1;
	sta_ent->coding = 1;

}

void halbb_test_ul_sta_ent1(struct bb_info *bb, struct rtw_ulfix_sta_i_ax4ru *sta_ent)
{

	sta_ent->mac_id = 1;
	sta_ent->ru_pos[0] = 124;
	sta_ent->ru_pos[1] = 124;
	sta_ent->ru_pos[2] = 124;

	sta_ent->fix_tgt_rssi=1;
	sta_ent->tgt_rssi[0] = 115;
	sta_ent->tgt_rssi[1] = 110;
	sta_ent->tgt_rssi[2] = 105;

	sta_ent->fix_rate = 1;
	sta_ent->fix_coding = 1;
	sta_ent->rate.dcm = 0;
	sta_ent->rate.mcs = 11;
	sta_ent->rate.ss = 1;
	sta_ent->coding = 1;

}

void halbb_test_ul_sta_ent2(struct bb_info *bb, struct rtw_ulfix_sta_i_ax4ru *sta_ent)
{
	sta_ent->mac_id = 255;
	sta_ent->ru_pos[0] = 0;
	sta_ent->ru_pos[1] = 126;
	sta_ent->ru_pos[2] = 126;

	sta_ent->fix_tgt_rssi=1;
	sta_ent->tgt_rssi[0] = 0;
	sta_ent->tgt_rssi[1] = 110;
	sta_ent->tgt_rssi[2] = 105;

	sta_ent->fix_rate = 1;
	sta_ent->fix_coding = 1;
	sta_ent->rate.dcm = 0;
	sta_ent->rate.mcs = 11;
	sta_ent->rate.ss = 1;
	sta_ent->coding = 1;
}

void halbb_test_ul_sta_ent3(struct bb_info *bb, struct rtw_ulfix_sta_i_ax4ru *sta_ent)
{

	sta_ent->mac_id = 255;
	sta_ent->ru_pos[0] = 0;
	sta_ent->ru_pos[1] = 0;
	sta_ent->ru_pos[2] = 128;

	sta_ent->fix_tgt_rssi=1;
	sta_ent->tgt_rssi[0] = 0;
	sta_ent->tgt_rssi[1] = 0;
	sta_ent->tgt_rssi[2] = 105;

	sta_ent->fix_rate = 1;
	sta_ent->fix_coding = 1;
	sta_ent->rate.dcm = 0;
	sta_ent->rate.mcs = 11;
	sta_ent->rate.ss = 1;
	sta_ent->coding = 1;
}


void halbb_test_ul_fix_tbl(struct bb_info *bb, struct rtw_ulru_fixtbl_ax4ru *tbl)
{
	tbl->tbl_hdr.rw = 1; /* write */
	tbl->tbl_hdr.idx = 0;
	tbl->tbl_hdr.offset = 0;
	tbl->tbl_hdr.type = 0;
	/* Need finish */

	tbl->max_sta_num = 4;
	tbl->min_sta_num = 2;
	tbl->doppler = 0;
	tbl->stbc = 0;
	tbl->gi_ltf = 2;
	tbl->ma_type = 0;
	tbl->fix_tb_t_pe_nom=1;
	tbl->tb_t_pe_nom=2;
	tbl->fixru_flag = 1;
	halbb_test_ul_sta_ent0(bb, &(tbl->sta[0]));
	halbb_test_ul_sta_ent1(bb, &(tbl->sta[1]));
	halbb_test_ul_sta_ent2(bb, &(tbl->sta[2]));
	halbb_test_ul_sta_ent3(bb, &(tbl->sta[3]));
}

void halbb_test_ulru_gp_tbl(struct bb_info *bb, struct rtw_ul_ru_gp_tbl *tbl)
{
	tbl->tbl_hdr.rw = 1; /* write */
	tbl->tbl_hdr.idx = 0;
	tbl->tbl_hdr.offset = 0;
	tbl->tbl_hdr.type = 0;

	tbl->grp_psd_max = 100;
	tbl->grp_psd_min = 80;	
	tbl->ppdu_bw = 2;
	tbl->tf_rate = 390;
	tbl->fix_tf_rate = 1;
	tbl->rf_gain_fix = 0;
	tbl->rf_gain_idx = 0;
	tbl->fix_mode_flag= 1;
}

void halbb_test_ba_tbl(struct bb_info *bb, struct rtw_ba_tbl_info *tbl)
{

	tbl->tbl_hdr.rw = 1; 
	tbl->tbl_hdr.idx = 0;
	tbl->tbl_hdr.offset = 0;
	tbl->tbl_hdr.len = sizeof(struct rtw_ba_tbl_info);
	tbl->tbl_hdr.type = 0;	
	tbl->tf_ba_t.fix_ba = 1;
	tbl->tf_ba_t.ru_psd = 20;
	tbl->tf_ba_t.tf_rate = 388;
	tbl->tf_ba_t.rf_gain_fix = 0;
	tbl->tf_ba_t.rf_gain_idx = 0;
	tbl->tf_ba_t.tb_ppdu_bw = 2;
	tbl->tf_ba_t.rate.dcm = 0;
	tbl->tf_ba_t.rate.mcs = 7;
	tbl->tf_ba_t.rate.ss = 0;
	tbl->tf_ba_t.gi_ltf = 2;
	tbl->tf_ba_t.doppler = 0;
	tbl->tf_ba_t.stbc = 0;
	tbl->tf_ba_t.sta_coding = 1;
	tbl->tf_ba_t.tb_t_pe_nom = 2;
	tbl->tf_ba_t.pr20_bw_en = 0;
	tbl->tf_ba_t.ma_type = 0;

}

void halbb_test_swgrp_hdl(struct bb_info *bb, struct rtw_sw_grp_set *hdl)
{
	hdl->swgrp_bitmap[0].macid = 3;
	hdl->swgrp_bitmap[0].en_upd_dl_swgrp = 1;
	hdl->swgrp_bitmap[0].en_upd_ul_swgrp = 0;
	hdl->swgrp_bitmap[0].dl_sw_grp_bitmap = 92;
	hdl->swgrp_bitmap[0].ul_sw_grp_bitmap = 5;
	hdl->swgrp_bitmap[0].cmdend = 0;

	hdl->swgrp_bitmap[1].macid = 7;
	hdl->swgrp_bitmap[1].en_upd_dl_swgrp = 0;
	hdl->swgrp_bitmap[1].en_upd_ul_swgrp = 1;
	hdl->swgrp_bitmap[1].dl_sw_grp_bitmap = 99;
	hdl->swgrp_bitmap[1].ul_sw_grp_bitmap = 58;
	hdl->swgrp_bitmap[1].cmdend = 1;
}

void halbb_test_dlmacid_cfg(struct bb_info *bb, struct rtw_dl_macid_cfg *cfg)
{
	cfg->macid = 5;
	cfg->dl_su_rate_cfg = 1;
	cfg->dl_su_rate = 3;
	cfg->dl_su_bw = 2;
	cfg->dl_su_pwr_cfg = 1;
	cfg->dl_su_pwr = 15;
	cfg->gi_ltf_4x8_support = 1;
	cfg->gi_ltf_1x8_support = 0;
	cfg->dl_su_info_en = 1;
	cfg->dl_su_gi_ltf = 3;
	cfg->dl_su_doppler_ctrl = 2;
	cfg->dl_su_coding = 1;
	cfg->dl_su_txbf = 0;
	cfg->dl_su_stbc = 0;
	cfg->dl_su_dcm = 0;
}

void halbb_test_ulmacid_cfg(struct bb_info *bb, struct rtw_ul_macid_set *cfg)
{
	cfg->ul_macid_cfg[0].macid = 5;
	cfg->ul_macid_cfg[0].endcmd = 0;

	cfg->ul_macid_cfg[0].ul_su_info_en = 1;
	cfg->ul_macid_cfg[0].ul_su_bw = 0;
	cfg->ul_macid_cfg[0].ul_su_gi_ltf = 1;
	cfg->ul_macid_cfg[0].ul_su_doppler_ctrl = 0;
	cfg->ul_macid_cfg[0].ul_su_dcm = 1;
	cfg->ul_macid_cfg[0].ul_su_ss = 2;
	cfg->ul_macid_cfg[0].ul_su_mcs = 7;

	cfg->ul_macid_cfg[0].ul_su_stbc = 0;
	cfg->ul_macid_cfg[0].ul_su_coding = 1;
	cfg->ul_macid_cfg[0].ul_su_rssi_m = 100;

	cfg->ul_macid_cfg[1].macid = 3;
	cfg->ul_macid_cfg[1].endcmd = 0;

	cfg->ul_macid_cfg[1].ul_su_info_en = 1;
	cfg->ul_macid_cfg[1].ul_su_bw = 0;
	cfg->ul_macid_cfg[1].ul_su_gi_ltf = 2;
	cfg->ul_macid_cfg[1].ul_su_doppler_ctrl = 0;
	cfg->ul_macid_cfg[1].ul_su_dcm = 1;
	cfg->ul_macid_cfg[1].ul_su_ss = 2;
	cfg->ul_macid_cfg[1].ul_su_mcs = 8;

	cfg->ul_macid_cfg[1].ul_su_stbc = 0;
	cfg->ul_macid_cfg[1].ul_su_coding = 1;
	cfg->ul_macid_cfg[1].ul_su_rssi_m = 101;

	cfg->ul_macid_cfg[2].macid = 6;
	cfg->ul_macid_cfg[2].endcmd = 1;

	cfg->ul_macid_cfg[2].ul_su_info_en = 1;
	cfg->ul_macid_cfg[2].ul_su_bw = 0;
	cfg->ul_macid_cfg[2].ul_su_gi_ltf = 3;
	cfg->ul_macid_cfg[2].ul_su_doppler_ctrl = 0;
	cfg->ul_macid_cfg[2].ul_su_dcm = 1;
	cfg->ul_macid_cfg[2].ul_su_ss = 2;
	cfg->ul_macid_cfg[2].ul_su_mcs = 9;

	cfg->ul_macid_cfg[2].ul_su_stbc = 0;
	cfg->ul_macid_cfg[2].ul_su_coding = 1;
	cfg->ul_macid_cfg[2].ul_su_rssi_m = 102;

	 
}


void halbb_test_sta_modify(struct bb_info *bb, struct rtw_dlru_fixtbl_ax4ru *fix_tbl, u8 mcs, u8 ss)
{	
	fix_tbl->max_sta_num = 2;	
	fix_tbl->min_sta_num = 2;	
	fix_tbl->doppler=0;	
	fix_tbl->stbc=0;	
	fix_tbl->gi_ltf=0;	
	fix_tbl->ma_type=0;	
	fix_tbl->fixru_flag = true;		
	fix_tbl->sta[0].mac_id=2;	
	fix_tbl->sta[0].ru_pos[0]=130;	
	fix_tbl->sta[0].ru_pos[1]=122;	
	fix_tbl->sta[0].ru_pos[2]=122;	
	fix_tbl->sta[0].fix_rate=1;	
	fix_tbl->sta[0].rate.mcs=mcs;	
	fix_tbl->sta[0].rate.ss=ss;	
	fix_tbl->sta[0].rate.dcm=0;	
	fix_tbl->sta[0].fix_coding=1;	
	fix_tbl->sta[0].coding=1;	
	fix_tbl->sta[0].fix_txbf=1;	
	fix_tbl->sta[0].txbf=0;	
	fix_tbl->sta[0].fix_pwr_fac=1;	
	fix_tbl->sta[0].pwr_boost_fac=0;
	
	fix_tbl->sta[1].mac_id=3;	
	fix_tbl->sta[1].ru_pos[0]=132;	
	fix_tbl->sta[1].ru_pos[1]=124;	
	fix_tbl->sta[1].ru_pos[2]=124;	
	fix_tbl->sta[1].fix_rate=1;	
	fix_tbl->sta[1].rate.mcs=mcs;	
	fix_tbl->sta[1].rate.ss=ss;	
	fix_tbl->sta[1].rate.dcm=0;	
	fix_tbl->sta[1].fix_coding=1;	
	fix_tbl->sta[1].coding=1;	
	fix_tbl->sta[1].fix_txbf=1;	
	fix_tbl->sta[1].txbf=0;	
	fix_tbl->sta[1].fix_pwr_fac=1;	
	fix_tbl->sta[1].pwr_boost_fac=0;
	
	fix_tbl->sta[2].mac_id=4;	
	fix_tbl->sta[2].ru_pos[0]=0;	
	fix_tbl->sta[2].ru_pos[1]=126;	
	fix_tbl->sta[2].ru_pos[2]=126;	
	fix_tbl->sta[2].fix_rate=1;	
	fix_tbl->sta[2].rate.mcs=mcs;	
	fix_tbl->sta[2].rate.ss=ss;	
	fix_tbl->sta[2].rate.dcm=0;	
	fix_tbl->sta[2].fix_coding=1;	
	fix_tbl->sta[2].coding=1;	
	fix_tbl->sta[2].fix_txbf=1;	
	fix_tbl->sta[2].txbf=0;	
	fix_tbl->sta[2].fix_pwr_fac=1;	
	fix_tbl->sta[2].pwr_boost_fac=0;
	
	fix_tbl->sta[3].mac_id=5;	
	fix_tbl->sta[3].ru_pos[0]=0;	
	fix_tbl->sta[3].ru_pos[1]=0;	
	fix_tbl->sta[3].ru_pos[2]=128;	
	fix_tbl->sta[3].fix_rate=1;	
	fix_tbl->sta[3].rate.mcs=mcs;	
	fix_tbl->sta[3].rate.ss=ss;	
	fix_tbl->sta[3].rate.dcm=0;	
	fix_tbl->sta[3].fix_coding=1;	
	fix_tbl->sta[3].coding=1;	
	fix_tbl->sta[3].fix_txbf=1;	
	fix_tbl->sta[3].txbf=0;	
	fix_tbl->sta[3].fix_pwr_fac=1;	
	fix_tbl->sta[3].pwr_boost_fac=0;
}

void halbb_test_grppwr_modify(struct bb_info *bb, struct rtw_dl_ru_gp_tbl *tbl, u8 grp_pwr)
{	
	tbl->ppdu_bw = CHANNEL_WIDTH_80;
	tbl->tx_pwr = grp_pwr; /*TODO:get from bb api*/
	tbl->pwr_boost_fac = 0;/*TODO:get from bb api*/
	tbl->fix_mode_flag = 1;

	/* Trigger BA settings */
	tbl->tf.tf_rate = RTW_DATA_RATE_OFDM24;
	tbl->tf.tb_ppdu_bw = CHANNEL_WIDTH_80;
	tbl->tf.rate.dcm = 0;
	tbl->tf.rate.mcs = 3;
	tbl->tf.rate.ss = 0;
	tbl->tf.fix_ba = 0;
	tbl->tf.ru_psd = 20;/*TODO:get from bb api*/
	tbl->tf.rf_gain_fix = 0;/*TODO:get from bb api*/
	tbl->tf.rf_gain_idx = 0;/*TODO:get from bb api*/
	tbl->tf.gi_ltf = RTW_GILTF_2XHE16;
	tbl->tf.doppler = 0;
	tbl->tf.stbc = 0;
	tbl->tf.sta_coding = 0;
	tbl->tf.tb_t_pe_nom = 2;
	tbl->tf.pr20_bw_en = 0;
	tbl->tf.ma_type = 0;
}

void halbb_test_csiinfo_cfg(struct bb_info *bb, struct rtw_csiinfo_cfg *cfg)
{
	cfg->macid = 5;
	cfg->csi_info_bitmap= 99;
}


void halbb_test_cqi_cfg(struct bb_info *bb, struct rtw_cqi_set *cfg)
{
	u8 i;
	
	cfg->cqi_info[0].macid = 5;
	cfg->cqi_info[0].fw_cqi_flag= 1;
	cfg->cqi_info[0].ru_rate_table_row_idx= 4;
	cfg->cqi_info[0].ul_dl= 0;
	cfg->cqi_info[0].endcmd= 0;
	
	for (i=10;i<29;i++)
		cfg->cqi_info[0].cqi_diff_table[i-10]= i;

	

	cfg->cqi_info[1].macid = 3;
	cfg->cqi_info[1].fw_cqi_flag= 1;
	cfg->cqi_info[1].ru_rate_table_row_idx= 2;
	cfg->cqi_info[1].ul_dl= 1;
	cfg->cqi_info[1].endcmd= 0;

	for (i=20;i<39;i++)
		cfg->cqi_info[1].cqi_diff_table[i-20]= i;



	cfg->cqi_info[2].macid = 1;
	cfg->cqi_info[2].fw_cqi_flag= 1;
	cfg->cqi_info[2].ru_rate_table_row_idx= 0;
	cfg->cqi_info[2].ul_dl= 0;
	cfg->cqi_info[2].endcmd= 0;
	
	for (i=30;i<49;i++)
		cfg->cqi_info[2].cqi_diff_table[i-30]= i-40;

	

	cfg->cqi_info[3].macid = 7;
	cfg->cqi_info[3].fw_cqi_flag= 1;
	cfg->cqi_info[3].ru_rate_table_row_idx= 6;
	cfg->cqi_info[3].ul_dl= 0;
	cfg->cqi_info[3].endcmd= 1;

	for (i=40;i<59;i++)
		cfg->cqi_info[3].cqi_diff_table[i-40]= i-60;
	

	 
}

void halbb_test_bbinfo_cfg(struct bb_info *bb, struct rtw_bbinfo_cfg *cfg)
{
	cfg->p20_ch_bitmap= 168;
}

void halbb_test_pbr_tbl_cfg(struct bb_info *bb, struct rtw_pwr_by_rt_tbl *cfg)
{	u8 i;

	for (i=0;i<32;i++)
		cfg->pwr_by_rt[i]= -200 + i*10;
}


u32 halbb_set_rua_tbl(struct bb_info *bb, u8 rua_tbl_idx)
{

	u32 ret = 0;
	struct rtw_dl_ru_gp_tbl dl_ru_gp_t = {{0}};
	struct rtw_ul_ru_gp_tbl ul_ru_gp_t = {{0}};
	struct rtw_ru_sta_info ru_sta_i = {{0}};
	struct rtw_dlru_fixtbl_ax4ru dl_ru_fix_t = {{0}};
	struct rtw_ulru_fixtbl_ax4ru ul_ru_fix_t = {{0}};
	struct rtw_ba_tbl_info ba_tbl_i = {{0}};

	BB_DBG(bb, DBG_RUA_TBL, "SET RUA TBL (%d)\n", rua_tbl_idx);
	switch(rua_tbl_idx) {
	case DL_RU_GP_TBL:
		halbb_test_dlru_gp_tbl(bb, &dl_ru_gp_t);
		halbb_upd_dlru_grptbl(bb, &dl_ru_gp_t);
		break;
	case UL_RU_GP_TBL:
		halbb_test_ulru_gp_tbl(bb, &ul_ru_gp_t);
		halbb_upd_ulru_grptbl(bb, &ul_ru_gp_t);
		break;
	case RU_STA_INFO:
		halbb_test_ru_sta_info(bb, &ru_sta_i);
		halbb_upd_rusta_info(bb, &ru_sta_i);
		break;
	case DL_RU_FIX_TBL:
		halbb_test_dl_fix_tbl(bb, &dl_ru_fix_t);
		halbb_upd_dlru_fixtbl_ax4ru(bb, &dl_ru_fix_t);//shall be revised
		break;
	case UL_RU_FIX_TBL:
		halbb_test_ul_fix_tbl(bb, &ul_ru_fix_t);
		halbb_upd_ulru_fixtbl_ax4ru(bb, &ul_ru_fix_t);//shall be revised
		break;
	case BA_INFO_TBL:
		halbb_test_ba_tbl(bb, &ba_tbl_i);
		halbb_upd_ba_infotbl(bb, &ba_tbl_i);
		break;
	default:
		break;
	}
	return ret;
}

u32 halbb_set_rua_cfg(struct bb_info *bb, u8 rua_cfg_idx)
{

	u32 ret = 0;
	struct rtw_sw_grp_set swgrp_hdl;
	struct rtw_dl_macid_cfg dlmacid_cfg;
	struct rtw_ul_macid_set ulmacid_cfg;
	struct rtw_csiinfo_cfg csiinfo_cfg;
	struct rtw_cqi_set cqi_info;
	struct rtw_bbinfo_cfg bbinfo_cfg;
	struct rtw_pwr_by_rt_tbl pbr_tbl;

	halbb_mem_set(bb, &swgrp_hdl, 0, sizeof(swgrp_hdl));
	halbb_mem_set(bb, &dlmacid_cfg, 0, sizeof(dlmacid_cfg));
	halbb_mem_set(bb, &ulmacid_cfg, 0, sizeof(ulmacid_cfg));
	halbb_mem_set(bb, &csiinfo_cfg, 0, sizeof(csiinfo_cfg));
	halbb_mem_set(bb, &cqi_info, 0, sizeof(cqi_info));
	halbb_mem_set(bb, &bbinfo_cfg, 0, sizeof(bbinfo_cfg));
	halbb_mem_set(bb, &pbr_tbl, 0, sizeof(pbr_tbl));

	BB_DBG(bb, DBG_RUA_TBL, "SET RUA TBL (%d)\n", rua_cfg_idx);
	switch(rua_cfg_idx) {
	case SW_GRP_HDL:
		halbb_test_swgrp_hdl(bb, &swgrp_hdl);
		halbb_swgrp_hdl(bb, &swgrp_hdl);
		break;
	case DL_MACID_CFG:
		halbb_test_dlmacid_cfg(bb, &dlmacid_cfg);
		halbb_dlmacid_cfg(bb, &dlmacid_cfg);
		break;
		
	case UL_MACID_CFG:
		halbb_test_ulmacid_cfg(bb, &ulmacid_cfg);
		halbb_ulmacid_cfg(bb, &ulmacid_cfg);
		
		break;
	case CSI_INFO_CFG:
		halbb_test_csiinfo_cfg(bb, &csiinfo_cfg);
		halbb_csiinfo_cfg(bb, &csiinfo_cfg);
		break;
	case CQI_CFG:
		halbb_test_cqi_cfg(bb, &cqi_info);
		halbb_cqi_cfg(bb, &cqi_info);
		break;
	case BB_INFO_CFG:
		halbb_test_bbinfo_cfg(bb, &bbinfo_cfg);
		halbb_bbinfo_cfg(bb, &bbinfo_cfg);
		break;
	case PWR_TBL_init:
		halbb_test_pbr_tbl_cfg(bb, &pbr_tbl);
		halbb_pbr_tbl_cfg(bb, &pbr_tbl);
		break;
	default:
		break;
	}
	return ret;
}

u32 halbb_set_rua_sta_rate_ss(struct bb_info *bb, u8 hdr_type, u8 ent, u8 mcs, u8 ss)
{

	u32 ret = 0;
	//struct rtw_dlru_fixtbl_ax4ru dl_ru_fix_t;
	union rtw_dlru_fixtbl dl_ru_fix_t;
	halbb_mem_set(bb, &dl_ru_fix_t,0,sizeof(dl_ru_fix_t));

	BB_DBG(bb, DBG_RUA_TBL, "SET DL FIX RUA TBL with sta_info change\n");
	BB_DBG(bb, DBG_RUA_TBL, "hdr_type: %d, ent: %d, mcs: %d ,ss: %d\n", hdr_type, ent, mcs, ss);

	dl_ru_fix_t.ax4ru.tbl_hdr.rw = 1; /* write */
	dl_ru_fix_t.ax4ru.tbl_hdr.idx = ent;
	dl_ru_fix_t.ax4ru.tbl_hdr.offset = 0;
	dl_ru_fix_t.ax4ru.tbl_hdr.len = sizeof(struct rtw_dlru_fixtbl_ax4ru);
	dl_ru_fix_t.ax4ru.tbl_hdr.type = hdr_type;

	halbb_test_sta_modify(bb, &dl_ru_fix_t.ax4ru, mcs, ss);
	
	ret = halbb_upd_dlru_fixtbl(bb, &dl_ru_fix_t);


	return ret;
}


u32 halbb_set_rua_grp_pwr(struct bb_info *bb, u8 hdr_type, u8 ent, u8 grp_pwr)
{

	u32 ret = 0;
	struct rtw_dl_ru_gp_tbl dl_ru_gp_t = {0};

	BB_DBG(bb, DBG_RUA_TBL, "SET DL GRP RUA TBL with grp_pwr change\n");
	BB_DBG(bb, DBG_RUA_TBL, "hdr_type: %d, ent: %d, grp_pwr: %d\n", hdr_type, ent, grp_pwr);

	dl_ru_gp_t.tbl_hdr.rw = 1; /* write */
	dl_ru_gp_t.tbl_hdr.idx = ent;
	dl_ru_gp_t.tbl_hdr.offset = 0;
	dl_ru_gp_t.tbl_hdr.len = sizeof(struct rtw_dl_ru_gp_tbl);
	dl_ru_gp_t.tbl_hdr.type = hdr_type;

	halbb_test_grppwr_modify(bb, &dl_ru_gp_t, grp_pwr);

	ret = halbb_upd_dlru_grptbl(bb, &dl_ru_gp_t);
	
	return ret;
}


void halbb_rua_tbl_dbg(struct bb_info *bb, char input[][16], u32 *_used,
			 char *output, u32 *_out_len)
{
	char help[] = "-h";
	u32 val[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i;

	if (_os_strcmp(input[1], help) == 0) {
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (0 [dlru_grptbl])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (1 [ulru_grptbl])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (2 [rusta_info])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (3 [dlru_fixtbl])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (4 [ulru_fixtbl])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (5 [ba_infotbl])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (6 0 [SW_GRP_HDL])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (6 1 [DL_MACID_CFG])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (6 2 [UL_MACID_CFG])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (6 3 [CSI_INFO_CFG])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (6 4 [CQI_CFG])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (6 5 [BB_INFO_CFG])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (6 6 [PWR_TBL_init])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (7 1:hw,0:sw ent mcs ss [dlru_fixtbl with sta_info change])}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{rua (8 1:hw,0:sw ent grp_pwr [dlru_fixtbl with grp_pwr change])}\n");
		goto out;
	}
	for (i = 0; i < 5; i++) {
		if (input[i + 1])
			HALBB_SCAN(input[i + 1], DCMD_DECIMAL, &val[i]);
	}
	if (val[0] < 6)
		halbb_set_rua_tbl(bb, (u8)val[0]);
	else if(val[0] == 6 )
		halbb_set_rua_cfg(bb, (u8)val[1]);	
	else if(val[0] == 7 )
		halbb_set_rua_sta_rate_ss(bb, (u8)val[1],(u8)val[2],(u8)val[3],(u8)val[4]);
	else 
		halbb_set_rua_grp_pwr(bb, (u8)val[1],(u8)val[2],(u8)val[3]);

out:
	*_used = used;
	*_out_len = out_len;
}
#endif /* HALBB_RUA_SUPPORT */
