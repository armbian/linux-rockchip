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

#ifdef HALBB_DFS_SUPPORT
void halbb_dfs(struct bb_info *bb)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;

	BB_DBG(bb, DBG_DFS, "[%s]===>\n", __func__);

	if (!(bb->support_ability & BB_DFS))
		return;
	if (bb_dfs->dfs_dyn_setting_en)
		halbb_dfs_dyn_setting(bb);
}

void halbb_dfs_init(struct bb_info *bb)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;

	BB_DBG(bb, DBG_DFS, "[%s]===>\n", __func__);

	/*DFS Parameter Initialization*/
	bb_dfs->dfs_rgn_domain = bb->phl_com->dfs_info.region_domain;
	halbb_dfs_rgn_dmn_dflt_cnfg(bb);
	bb_dfs->chrp_obsrv_flag = false;
	bb_dfs->dfs_sw_trgr_mode = false;
	bb_dfs->dfs_dbg_mode = false;
	bb_dfs->dfs_dyn_setting_en = true;
	bb_dfs->dbg_prnt_en = false;
	bb_dfs->is_mic_w53 = false;
	bb_dfs->is_mic_w56 = false;
	bb_dfs->chrp_th = DFS_CHIRP_TH;
	bb_dfs->ppb_prcnt = DFS_PPB_IDLE_PRCNT;
	bb_dfs->fk_dfs_num_th = 5;
	bb_dfs->dfs_tp_th = 2;
	bb_dfs->dfs_idle_prd_th = 50;
	bb_dfs->dfs_fa_th = 20;
	bb_dfs->dfs_nhm_th = 2;
	bb_dfs->dfs_n_cnfd_lvl_th = 5;
}

void halbb_radar_detect_reset(struct bb_info *bb)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;

	struct bb_dfs_cr_info *cr = &bb_dfs->bb_dfs_cr_i;

	halbb_set_reg_phy0_1(bb, cr->dfs_en, cr->dfs_en_m, 0);
	halbb_set_reg_phy0_1(bb, cr->dfs_en, cr->dfs_en_m, 1);
}

void halbb_radar_detect_disable(struct bb_info *bb)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	struct bb_dfs_cr_info *cr = &bb_dfs->bb_dfs_cr_i;

	halbb_set_reg_phy0_1(bb, cr->dfs_en, cr->dfs_en_m, 0);
}

void halbb_radar_detect_enable(struct bb_info *bb)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	struct bb_dfs_cr_info *cr = &bb_dfs->bb_dfs_cr_i;

	halbb_set_reg_phy0_1(bb, cr->dfs_en, cr->dfs_en_m, 1);
}

bool halbb_is_dfs_band(struct bb_info *bb, u8 ch, u8 bw)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	bool is_w53_band = false, is_w56_band = false;

	if ((ch >= 52) && (ch <= 64))
		is_w53_band = true;
	else if ((ch >= 100) && (ch <= 144))
		is_w56_band = true;

#ifdef CONFIG_PHL_DFS_REGD_JAP
	if (bb_dfs->dfs_rgn_domain == DFS_REGD_JAP)
		halbb_dfs_rgn_dmn_cnfg_by_ch(bb, is_w53_band, is_w56_band);
#endif

	if ((is_w53_band) || (is_w56_band))
		return true;
	else
		return false;
}

void halbb_dfs_rgn_dmn_dflt_cnfg(struct bb_info *bb)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	u8 i;

	/* PW unit: 200ns ; PRI unit: 25us */

#ifdef CONFIG_PHL_DFS_REGD_FCC
	/*Type {0,1,2,3,4,6,L}*/
	u8 pw_min_fcc_tab[DFS_RDR_TYP_NUM] = {5,5,5,30,55,5,250,0};
	u16 pw_max_fcc_tab[DFS_RDR_TYP_NUM] = {5,5,25,50,100,5,500,1000};
	u8 pri_min_fcc_tab[DFS_RDR_TYP_NUM] = {57,20,6,8,8,13,40,0};
	u8 pri_max_fcc_tab[DFS_RDR_TYP_NUM] = {58,123,10,20,20,14,80,0};
	u8 ppb_fcc_tab[DFS_RDR_TYP_NUM] = {18,18,23,16,12,9,8,255};
#endif
#ifdef CONFIG_PHL_DFS_REGD_ETSI
	/*Type {1,2,3,4,5,6,R}*/
	u8 pw_min_etsi_tab[DFS_RDR_TYP_NUM] = {2,2,2,100,2,2,5,0};
	u16 pw_max_etsi_tab[DFS_RDR_TYP_NUM] = {25,75,75,150,10,10,5,1000};
	u8 pri_min_etsi_tab[DFS_RDR_TYP_NUM] = {40,25,10,10,100,33,57,0};
	u8 pri_max_etsi_tab[DFS_RDR_TYP_NUM] = {200,200,18,20,134,100,58,0};
	u8 ppb_etsi_tab[DFS_RDR_TYP_NUM] = {10,15,25,20,10,15,18,255};
	/*
	etsi 302 Type {1,2,3,4,5,X,L}
	u8 pw_min_etsi2_tab[DFS_RDR_TYP_NUM] =  {5,5,50,5,5,0,100,0};
	u16 pw_max_etsi2_tab[DFS_RDR_TYP_NUM] = {5,25,75,75,75,1000,150,1000};
	u8 pri_min_etsi2_tab[DFS_RDR_TYP_NUM] = {53,40,40,25,10,0,10,0};
	u8 pri_max_etsi2_tab[DFS_RDR_TYP_NUM] = {54,200,200,34,18,0,20,0};
	u8 ppb_etsi2_tab[DFS_RDR_TYP_NUM] = {15,10,15,15,25,255,20,255};
	*/
#endif

#ifdef CONFIG_PHL_DFS_REGD_FCC
	if (bb_dfs->dfs_rgn_domain == DFS_REGD_FCC) {
		bb_dfs->l_rdr_exst_flag = true;
		for (i = 0; i < DFS_RDR_TYP_NUM ; i++) {
			bb_dfs->pw_min_tab[i] = pw_min_fcc_tab[i];
			bb_dfs->pw_max_tab[i] = pw_max_fcc_tab[i];
			bb_dfs->pri_min_tab[i] = pri_min_fcc_tab[i];
			bb_dfs->pri_max_tab[i] = pri_max_fcc_tab[i];
			bb_dfs->ppb_tab[i] = ppb_fcc_tab[i];
		}
	}
#endif
#ifdef CONFIG_PHL_DFS_REGD_ETSI
	if (bb_dfs->dfs_rgn_domain == DFS_REGD_ETSI) {
		for (i = 0; i < DFS_RDR_TYP_NUM ; i++) {
			bb_dfs->pw_min_tab[i] = pw_min_etsi_tab[i];
			bb_dfs->pw_max_tab[i] = pw_max_etsi_tab[i];
			bb_dfs->pri_min_tab[i] = pri_min_etsi_tab[i];
			bb_dfs->pri_max_tab[i] = pri_max_etsi_tab[i];
			bb_dfs->ppb_tab[i] = ppb_etsi_tab[i];
		}
	}
#endif
}

void halbb_dfs_rgn_dmn_cnfg_by_ch(struct bb_info *bb, bool w53_band,
				  bool w56_band)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	u8 i;

	/* PW unit: 200ns ; PRI unit: 25us */

	/*Type {1,2,3,4,5,6,7,8}*/
	u8 pw_min_mic_w53_tab[DFS_RDR_TYP_NUM] = {2,2,2,2,2,2,2,2};
	u16 pw_max_mic_w53_tab[DFS_RDR_TYP_NUM] = {25,75,25,75,8,8,8,8};
	u8 pri_min_mic_w53_tab[DFS_RDR_TYP_NUM] = {40,25,40,25,35,42,44,53};
	u8 pri_max_mic_w53_tab[DFS_RDR_TYP_NUM] = {200,200,200,200,36,44,46,55};
	u8 ppb_mic_w53_tab[DFS_RDR_TYP_NUM] = {10,15,22,22,30,25,24,20};

	/*Type {1,2,3,4,5,6,L,8}, ,ppb of Type3 is set as 15 due to RPT TO*/
	u8 pw_min_mic_w56_tab[DFS_RDR_TYP_NUM] = {2,5,10,5,30,55,250,5};
	u16 pw_max_mic_w56_tab[DFS_RDR_TYP_NUM] = {3,5,10,25,50,100,500,5};
	u8 pri_min_mic_w56_tab[DFS_RDR_TYP_NUM] = {55,57,160,6,8,8,40,13};
	u8 pri_max_mic_w56_tab[DFS_RDR_TYP_NUM] = {56,58,160,10,20,20,80,14};
	u8 ppb_mic_w56_tab[DFS_RDR_TYP_NUM] = {18,18,15,23,16,12,8,9};

	if (bb_dfs->dfs_rgn_domain == DFS_REGD_JAP) {
		if (w53_band) {
			bb_dfs->is_mic_w53 = true;
			bb_dfs->l_rdr_exst_flag = false;
			for (i = 0; i < DFS_RDR_TYP_NUM ; i++) {
				bb_dfs->pw_min_tab[i] = pw_min_mic_w53_tab[i];
				bb_dfs->pw_max_tab[i] = pw_max_mic_w53_tab[i];
				bb_dfs->pri_min_tab[i] = pri_min_mic_w53_tab[i];
				bb_dfs->pri_max_tab[i] = pri_max_mic_w53_tab[i];
				bb_dfs->ppb_tab[i] = ppb_mic_w53_tab[i];
			}
		} else if (w56_band) {
			bb_dfs->is_mic_w56 = true;
			bb_dfs->l_rdr_exst_flag = true;
			for (i = 0; i < DFS_RDR_TYP_NUM ; i++) {
				bb_dfs->pw_min_tab[i] = pw_min_mic_w56_tab[i];
				bb_dfs->pw_max_tab[i] = pw_max_mic_w56_tab[i];
				bb_dfs->pri_min_tab[i] = pri_min_mic_w56_tab[i];
				bb_dfs->pri_max_tab[i] = pri_max_mic_w56_tab[i];
				bb_dfs->ppb_tab[i] = ppb_mic_w56_tab[i];
			}
		}
	}
}

void halbb_radar_chrp_mntr(struct bb_info *bb, bool chrp_flag)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	u8 i = 0;

	if (bb->bb_sys_up_time - bb_dfs->chrp_srt_t >= DFS_FCC_LP_LNGTH) {
		bb_dfs->chrp_obsrv_flag = false;
		bb_dfs->chrp_srt_t = 0;
		bb_dfs->chrp_cnt = 0;
		bb_dfs->lng_rdr_cnt = 0;
	} 

	if ((chrp_flag) && !(bb_dfs->chrp_obsrv_flag)) {
		bb_dfs->chrp_srt_t = bb->bb_sys_up_time;
		bb_dfs->chrp_obsrv_flag = true;
	}

	if (bb_dfs->dbg_prnt_en)
		BB_DBG(bb, DBG_DFS, "[mntr_prd, sys_t, chrp_srt_t]: [%d, %d, %d]\n",
		      (bb->bb_sys_up_time - bb_dfs->chrp_srt_t),
		       bb->bb_sys_up_time, bb_dfs->chrp_srt_t);
}

void halbb_radar_seq_inspctn(struct bb_info *bb, u16 dfs_rpt_idx,
				u8 c_num, u8 p_num)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;

	if (dfs_rpt_idx != 0) {
		if (p_num == DFS_MAX_SEQ_NUM) {
			if (c_num != 0)
				bb_dfs->n_seq_flag = true;
		} else {
			if (ABS_8(c_num - p_num) > 1)
				bb_dfs->n_seq_flag = true;
		}
	}

	if (bb_dfs->dbg_prnt_en) {
		if (bb_dfs->n_seq_flag)
			BB_DBG(bb, DBG_DFS, "[cur_seq_num, pre_seq_num] = [%d, %d]\n",
			       c_num, p_num);
	}

	bb_dfs->lst_seq_num = c_num;
}

void halbb_radar_ptrn_cmprn(struct bb_info *bb, u16 dfs_rpt_idx,
				u8 pri, u16 pw, bool chrp_flag)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	u8 j = 0, pw_lbd = 0, pri_lbd = 0, pri_ubd = 0;
	u16 pw_ubd = 0;
	u8 pw_factor = 0,pri_factor = 0;

	if (bb_dfs->l_rdr_exst_flag)
		halbb_radar_chrp_mntr(bb, chrp_flag);

	if (!bb_dfs->idle_flag ){
		pw_factor = PW_FTR ;
		pri_factor = PRI_FTR;
	}
	else {
		pw_factor = PW_FTR_IDLE;
		pri_factor = PRI_FTR_IDLE;

	}

	for (j = 0; j < DFS_RDR_TYP_NUM ; j++) {
		pw_lbd = (bb_dfs->pw_min_tab[j] * (8 - pw_factor) >> 3);
		pw_ubd = (bb_dfs->pw_max_tab[j] * (8 + pw_factor) >> 3);
		pri_lbd = (bb_dfs->pri_min_tab[j] * (8 - pri_factor) >> 3);
		if ((bb_dfs->pri_max_tab[j] * (8 + pri_factor) >> 3) <= 0xDC)
			pri_ubd = (bb_dfs->pri_max_tab[j] * (8 + pri_factor) >> 3);
		else
			pri_ubd = 0xDC;

		if (bb_dfs->is_mic_w53) {
			if (j < 2) {
				if ((pw_lbd <= pw) && (pw_ubd >= pw) &&
				    (pri_lbd <= pri) && (pri_ubd >= pri))
					bb_dfs->srt_rdr_cnt[j]++;
			} else if (j < 4) {
				if ((pw_lbd <= pw) && (pw_ubd >= pw)) {
					if (bb_dfs->pw_tmp >= 100 &&
					    bb_dfs->pw_tmp <= 550 &&
					    ABS_16(pw - bb_dfs->pw_tmp) >= 15 &&
					    pri + bb_dfs->pri_tmp >= pri_lbd &&
					    pri + bb_dfs->pri_tmp <= pri_ubd)
						bb_dfs->srt_rdr_cnt[j]++;
				}
			} else {
				if ((pw_lbd <= pw) && (pw_ubd >= pw)) {
					if (bb_dfs->pw_tmp >= 150 &&
					    bb_dfs->pw_tmp <= 160 &&
					    pri + bb_dfs->pri_tmp >= pri_lbd &&
					    pri + bb_dfs->pri_tmp <= pri_ubd)
						bb_dfs->srt_rdr_cnt[j]++;
				}
			}
		} else {
			if ((j == DFS_L_RDR_IDX) && (bb_dfs->l_rdr_exst_flag)) {
				if ((pw_lbd <= pw) && (pw_ubd >= pw))
					bb_dfs->lng_rdr_cnt++;
			} else {
				if ((pw_lbd <= pw) && (pw_ubd >= pw) &&
				    (pri_lbd <= pri) && (pri_ubd >= pri))
					bb_dfs->srt_rdr_cnt[j]++;
			}
		}

		if (dfs_rpt_idx == 0) {
			if (bb_dfs->dbg_prnt_en) {
				BB_DBG(bb, DBG_DFS, "pw_factor =  %d, pri_factor = %d\n", pw_factor,pri_factor);
				BB_DBG(bb, DBG_DFS, "Type %d: [pw_lbd-pw_ubd], [pri_lbd-pri_ubd] = [%d-%d], [%d-%d]\n",
				       (j + 1), pw_lbd, pw_ubd, pri_lbd, pri_ubd);
			}
		}
		bb_dfs->pw_lbd[j] = pw_lbd;
		bb_dfs->pw_ubd[j] = pw_ubd;
		bb_dfs->pri_lbd[j] = pri_lbd;
		bb_dfs->pri_ubd[j] = pri_ubd;
	}

	if (chrp_flag)
		bb_dfs->chrp_cnt++;

	bb_dfs->pri_tmp = pri;
	bb_dfs->pw_tmp = pw;
}

void halbb_radar_info_processing(struct bb_info *bb,
				 struct hal_dfs_rpt *rpt, u16 dfs_rpt_idx)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	struct bb_rdr_info *dfs_rdr_info = NULL;
	
	u8 pri = 0, cur_seq_num = 0, pre_seq_num = 0;
	u16 pw = 0;
	bool chrp_flag = false;

	dfs_rdr_info = (struct bb_rdr_info *)rpt->dfs_ptr;
	cur_seq_num = dfs_rdr_info->rdr_info_seq;
	pre_seq_num = bb_dfs->lst_seq_num;

	if (rpt->phy_idx == HW_PHY_0) {
		pw = (dfs_rdr_info->rdr_info_sg0_pw_m << 7) |
		     (dfs_rdr_info->rdr_info_sg0_pw_l);
		pri = (dfs_rdr_info->rdr_info_sg0_pri_m << 7) |
		      (dfs_rdr_info->rdr_info_sg0_pri_l);
		chrp_flag = dfs_rdr_info->rdr_info_sg0_chirp_flag;
	} else if (rpt->phy_idx == HW_PHY_1) {
		pw = (dfs_rdr_info->rdr_info_sg1_pw_m << 4) |
		     (dfs_rdr_info->rdr_info_sg1_pw_l);
		pri = (dfs_rdr_info->rdr_info_sg1_pri_m << 4) |
		      (dfs_rdr_info->rdr_info_sg1_pri_l);
		chrp_flag = dfs_rdr_info->rdr_info_sg1_chirp_flag;
	}

	halbb_radar_ptrn_cmprn(bb, dfs_rpt_idx, pri, pw, chrp_flag);
	halbb_radar_seq_inspctn(bb, dfs_rpt_idx, cur_seq_num, pre_seq_num);

	if (bb_dfs->dbg_prnt_en)
		BB_DBG(bb, DBG_DFS, "DFS_RPT: [pw, pri, c_flag] = [%d, %d, %d]\n",
		       pw, pri, chrp_flag);

	if (dfs_rpt_idx == (rpt->dfs_num - 1)) {
		if (bb_dfs->dbg_prnt_en) {
			BB_DBG(bb, DBG_DFS, "\n");
			BB_DBG(bb, DBG_DFS, "lng_rdr_cnt = %d\n", bb_dfs->lng_rdr_cnt);
			BB_DBG(bb, DBG_DFS, "srt_rdr_cnt = [%d, %d, %d, %d, %d, %d, %d, %d]\n",
			       bb_dfs->srt_rdr_cnt[0], bb_dfs->srt_rdr_cnt[1],
			       bb_dfs->srt_rdr_cnt[2], bb_dfs->srt_rdr_cnt[3],
			       bb_dfs->srt_rdr_cnt[4], bb_dfs->srt_rdr_cnt[5],
			       bb_dfs->srt_rdr_cnt[6], bb_dfs->srt_rdr_cnt[7]);
			BB_DBG(bb, DBG_DFS, "\n");
		}
	}

	if (pri == 0)
		bb_dfs->n_cnfd_lvl++;
	if (bb_dfs->n_cnfd_lvl > bb_dfs->dfs_n_cnfd_lvl_th)
		bb_dfs->n_cnfd_flag = true;

	bb_dfs->pw_rpt[dfs_rpt_idx] = pw;
	bb_dfs->pri_rpt[dfs_rpt_idx] = pri;
	bb_dfs->chrp_rpt[dfs_rpt_idx] = chrp_flag;

	rpt->dfs_ptr += DFS_RPT_LENGTH;

	/* BB_DBG(bb, DBG_DFS, "dfs_ptr = %p\n", rpt->dfs_ptr); */
}

bool halbb_radar_detect(struct bb_info *bb, struct hal_dfs_rpt *dfs_rpt)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;

	bool rdr_detected = false;
	u8 ppb_typ_th;
	u16 i = 0;

	if (bb_dfs->ppb_prcnt == DFS_PPB_ADPTV_PRCNT){
		if (bb_dfs->dbg_prnt_en)
			BB_DBG(bb, DBG_DFS, "Enable NHM-aided DFS!\n");
		return rdr_detected;
	}

	if (!(bb->support_ability & BB_DFS)) {
		BB_DBG(bb, DBG_DFS, "Not support DFS function!\n");
		return false;
	}

	if (!(bb_dfs->l_rdr_exst_flag)) {
		/* Check Fake DFS rpt */
		if (dfs_rpt->dfs_num < bb_dfs->fk_dfs_num_th) {
			if (bb_dfs->dbg_prnt_en) {
				BB_DBG(bb, DBG_DFS, "Non-existent form of DFS!\n");
				BB_DBG(bb, DBG_DFS, "\n");
			}
			return false;
		}
	}


	if (bb_dfs->dbg_prnt_en) {
		BB_DBG(bb, DBG_DFS, "[%s]===>\n", __func__);
		BB_DBG(bb, DBG_DFS, "phy_idx = %d, dfs_num = %d\n",
		       dfs_rpt->phy_idx, dfs_rpt->dfs_num);
	}

	/* DFS Info Parsing/Processing*/
	for (i = 0; i < (dfs_rpt->dfs_num) ; i++)
		halbb_radar_info_processing(bb, dfs_rpt, i);

	for (i = 0; i < DFS_RDR_TYP_NUM ; i++) {
		ppb_typ_th = ((bb_dfs->ppb_tab[i] * bb_dfs->ppb_prcnt) >> 3);
		if ((i == DFS_L_RDR_IDX) && (bb_dfs->l_rdr_exst_flag)) {
			if ((bb_dfs->lng_rdr_cnt >= ppb_typ_th) &&
			    (bb_dfs->chrp_cnt >= bb_dfs->chrp_th)) {
				rdr_detected = true;
				BB_DBG(bb, DBG_DFS, "Chrp Rdr Appeared!\n");
				BB_DBG(bb, DBG_DFS, "Long Rdr reaches threshold (ppb_th:%d / chirp_th:%d)!\n",
					ppb_typ_th,bb_dfs->chrp_th);
			}
		} else {
			if (bb_dfs->srt_rdr_cnt[i] >= ppb_typ_th) {
				if (bb_dfs->n_seq_flag) {
					rdr_detected = false;
					BB_DBG(bb, DBG_DFS, "Non-sequential DFS Dropped!\n");
				} else if (bb_dfs->n_cnfd_flag) {
					rdr_detected = false;
					BB_DBG(bb, DBG_DFS, "Non-confidential DFS Blocked!\n");
				} else {
					rdr_detected = true;
					BB_DBG(bb, DBG_DFS, "Rdr Type %d reaches threshold (ppb_th:%d)!\n",
					      (i+1), ppb_typ_th);
				}
			}
		}
	}

	/* Debug Mode */
	if (rdr_detected) {
		if (!(bb_dfs->dbg_prnt_en)) {
			BB_DBG(bb, DBG_DFS, "[%s]===>\n", __func__);
			BB_DBG(bb, DBG_DFS, "phy_idx = %d, dfs_num = %d\n",
			       dfs_rpt->phy_idx, dfs_rpt->dfs_num);
			for (i = 0; i < DFS_RDR_TYP_NUM ; i++) {
				BB_DBG(bb, DBG_DFS, "Type %d: [pw_lbd-pw_ubd], [pri_lbd-pri_ubd] = [%d-%d], [%d-%d]\n",
				      (i+1), bb_dfs->pw_lbd[i],
				      bb_dfs->pw_ubd[i], bb_dfs->pri_lbd[i],
				      bb_dfs->pri_ubd[i]);
			}
			for (i = 0; i < dfs_rpt->dfs_num ; i++) {
				BB_DBG(bb, DBG_DFS, "DFS_RPT %d: [pw, pri, c_flag] = [%d, %d, %d]\n",
			              (i + 1), bb_dfs->pw_rpt[i], bb_dfs->pri_rpt[i],
				       bb_dfs->chrp_rpt[i]);
			}
			BB_DBG(bb, DBG_DFS, "lng_rdr_cnt = %d, chrp_cnt =  %d\n", bb_dfs->lng_rdr_cnt,bb_dfs->chrp_cnt);
			BB_DBG(bb, DBG_DFS, "srt_rdr_cnt = [%d, %d, %d, %d, %d, %d, %d, %d]\n",
			       bb_dfs->srt_rdr_cnt[0], bb_dfs->srt_rdr_cnt[1],
			       bb_dfs->srt_rdr_cnt[2], bb_dfs->srt_rdr_cnt[3],
			       bb_dfs->srt_rdr_cnt[4], bb_dfs->srt_rdr_cnt[5],
			       bb_dfs->srt_rdr_cnt[6], bb_dfs->srt_rdr_cnt[7]);
		}
		if (bb_dfs->dfs_dbg_mode) {
			rdr_detected = false;
			BB_DBG(bb, DBG_DFS, "Radar is detected in DFS debug mode!\n");
		}
	}

	/* SW Trigger Mode */
	if (bb_dfs->dfs_sw_trgr_mode) {
		rdr_detected = true;
		BB_DBG(bb, DBG_DFS, "[HALBB] Radar SW-Trigger Mode!\n");
	}

	/* Reset SW Counter/Flag */
	bb_dfs->n_seq_flag = false;
	bb_dfs->n_cnfd_flag = false;
	bb_dfs->n_cnfd_lvl = 0;
	for (i = 0; i < DFS_RDR_TYP_NUM ; i++)
		bb_dfs->srt_rdr_cnt[i] = 0;
	for (i = 0; i < dfs_rpt->dfs_num ; i++) {
		bb_dfs->pw_rpt[i] = 0;
		bb_dfs->pri_rpt[i] = 0;
		bb_dfs->chrp_rpt[i] = 0;
	}

	return rdr_detected;
}

void halbb_dfs_dyn_setting(struct bb_info *bb)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	struct bb_link_info *link = &bb->bb_link_i;
	struct bb_env_mntr_info *env_mntr = &bb->bb_env_mntr_i;
#ifdef HALBB_STATISTICS_SUPPORT
	struct bb_stat_info *stat = &bb->bb_stat_i;
	struct bb_fa_info *fa = &stat->bb_fa_i;
#endif

	if (link->total_tp < bb_dfs->dfs_tp_th)
		bb_dfs->idle_flag = true;
	else
		bb_dfs->idle_flag = false;

	if ((env_mntr->nhm_idle_ratio > bb_dfs->dfs_idle_prd_th) &&
	    (fa->cnt_fail_all < bb_dfs->dfs_fa_th) &&
	    (env_mntr->nhm_ratio < bb_dfs->dfs_nhm_th)) {
		if (bb_dfs->idle_flag)
			bb_dfs->ppb_prcnt = DFS_PPB_IDLE_PRCNT;
		else
			bb_dfs->ppb_prcnt = DFS_PPB_PRCNT;
		BB_DBG(bb, DBG_DFS, "[DFS Status] Normal DFS Mode\n");
	} else {
		bb_dfs->ppb_prcnt = DFS_PPB_ADPTV_PRCNT;
		BB_DBG(bb, DBG_DFS, "[DFS Status] Adaptive DFS Mode\n");
	}

	BB_DBG(bb, DBG_DFS, "[T_TP / I_RTO / FA_CNT / N_RTO] = [%d, %d, %d, %d]\n",
	       link->total_tp, env_mntr->nhm_idle_ratio,
	       fa->cnt_fail_all, env_mntr->nhm_ratio);
}

void halbb_dfs_debug(struct bb_info *bb, char input[][16], u32 *_used,
			 char *output, u32 *_out_len)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	char help[] = "-h";
	u32 var[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i = 0;

	HALBB_SCAN(input[1], DCMD_DECIMAL, &var[0]);

	if ((_os_strcmp(input[1], help) == 0)) {
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{1} Set DFS_SW_TRGR_MODE => {0}: Disable, {1}: Enable\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{2} Set DFS_DBG_MODE => {0}: Disable, {1}: Enable\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{3} Set DBG_PRINT => {0}: Disable, {1}: Enable\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{4} Set DYN_SETTING_EN => {0}: Disable, {1}: Enable\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{5} Set Detection Parameter => \n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---{1} Set the threshold of fake DFS number => {Num}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---{2} Set the threshold of chirp number => {Num}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---{3} Set the threshold of ppb percent => {Percent: 1-8}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---{4} Set the threshold of DFS_TP Threshold => {Mbps}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---{5} Set the threshold of DFS_Idle_Period Threshold => {Percent: 0-100}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---{6} Set the threshold of DFS_FA Threshold => {Num}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---{7} Set the threshold of DFS_NHM Threshold => {Percent: 0-100}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---{8} Set the threshold of DFS_N_CNFD_Level Threshold => {Num}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "{100} Show all parameter\n");
	} else if (var[0] == 100) {
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DFS Region Domain: %s\n",
			   (bb_dfs->dfs_rgn_domain > 1) ?
			   (bb_dfs->dfs_rgn_domain > 2) ?
			   "ETSI": "MIC" : "FCC");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DFS_SW_TRGR_MODE = %d\n",
			    bb_dfs->dfs_sw_trgr_mode);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DFS_DBG_MODE = %d\n",
			    bb_dfs->dfs_dbg_mode);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DBG_PRINT = %d\n",
			    bb_dfs->dbg_prnt_en);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DYN_SETTING_EN = %d\n",
			    bb_dfs->dfs_dyn_setting_en);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "Fake DFS Num Threshold = %d\n",
			    bb_dfs->fk_dfs_num_th);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "Chirp Number = %d\n",
			    bb_dfs->chrp_th);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "PPB Percent = %d\n",
			    bb_dfs->ppb_prcnt);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DFS_TP Threshold = %d\n",
			    bb_dfs->dfs_tp_th);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DFS_Idle_Period Threshold = %d\n",
			    bb_dfs->dfs_idle_prd_th);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DFS_FA Threshold = %d\n",
			    bb_dfs->dfs_fa_th);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DFS_NHM Threshold = %d\n",
			    bb_dfs->dfs_nhm_th);
		BB_DBG_CNSL(out_len, used, output + used, out_len - used, "DFS_N_CNFD_Level Threshold = %d\n",
			    bb_dfs->dfs_n_cnfd_lvl_th);
	} else {
		if (var[0] == 1) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			bb_dfs->dfs_sw_trgr_mode = (bool)var[1];
			BB_DBG_CNSL(out_len, used, output + used,
				    out_len - used, "DFS_SW_TRGR_MODE = %d\n",
				    bb_dfs->dfs_sw_trgr_mode);
		} else if (var[0] == 2) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			bb_dfs->dfs_dbg_mode = (bool)var[1];
			BB_DBG_CNSL(out_len, used, output + used,
				    out_len - used, "DFS_DBG_MODE = %d\n",
				    bb_dfs->dfs_dbg_mode);
		} else if (var[0] == 3) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			bb_dfs->dbg_prnt_en = (bool)var[1];
			BB_DBG_CNSL(out_len, used, output + used,
				    out_len - used, "DBG_PRINT = %d\n",
				    bb_dfs->dbg_prnt_en);
		} else if (var[0] == 4) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			bb_dfs->dfs_dyn_setting_en = (bool)var[1];
			BB_DBG_CNSL(out_len, used, output + used,
				    out_len - used, "DYN_SETTING_EN = %d\n",
				    bb_dfs->dfs_dyn_setting_en);	
		} else if (var[0] == 5) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			if (var[1] == 1) {
				HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
				bb_dfs->fk_dfs_num_th = (u8)var[2];
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used, "fk_dfs_num_th = %d\n",
					    bb_dfs->fk_dfs_num_th);
			} else if (var[1] == 2) {
				HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
				bb_dfs->chrp_th= (u8)var[2];
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used, "chrp_th = %d\n",
					    bb_dfs->chrp_th);
			} else if (var[1] == 3) {
				HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
				bb_dfs->ppb_prcnt = (u8)var[2];
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used, "ppb_prcnt = %d\n",
					    bb_dfs->ppb_prcnt);
			} else if (var[1] == 4) {
				HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
				bb_dfs->dfs_tp_th = (u8)var[2];
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used, "dfs_tp_th = %d\n",
					    bb_dfs->dfs_tp_th);
			} else if (var[1] == 5) {
				HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
				bb_dfs->dfs_idle_prd_th = (u8)var[2];
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used, "dfs_idle_prd_th = %d\n",
					    bb_dfs->dfs_idle_prd_th);
			} else if (var[1] == 6) {
				HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
				bb_dfs->dfs_fa_th= (u8)var[2];
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used, "dfs_fa_th = %d\n",
					    bb_dfs->dfs_fa_th);
			} else if (var[1] == 7) {
				HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
				bb_dfs->dfs_nhm_th= (u8)var[2];
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used, "dfs_nhm_th = %d\n",
					    bb_dfs->dfs_nhm_th);
			} else if (var[1] == 8) {
				HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
				bb_dfs->dfs_n_cnfd_lvl_th= (u8)var[2];
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used, "dfs_n_cnfd_lvl_th = %d\n",
					    bb_dfs->dfs_n_cnfd_lvl_th);
			}
		}
	}

	*_used = used;
	*_out_len = out_len;
}

void halbb_cr_cfg_dfs_init(struct bb_info *bb)
{
	struct bb_dfs_info *bb_dfs = &bb->bb_dfs_i;
	struct bb_dfs_cr_info *cr = &bb_dfs->bb_dfs_cr_i;

	switch (bb->cr_type) {

#ifdef BB_8852A_CAV_SUPPORT
	case BB_52AA:
		/*cr->dfs_en = DFS_EN_52AA;
		  cr->dfs_en_m = DFS_EN_52AA_M;
		 */
		break;
#endif
#ifdef HALBB_COMPILE_AP_SERIES
	case BB_AP:
		cr->dfs_en = DFS_EN_A;
		cr->dfs_en_m = DFS_EN_A_M;
		break;
#endif
#ifdef HALBB_COMPILE_CLIENT_SERIES
	case BB_CLIENT:
		cr->dfs_en = DFS_EN_C;
		cr->dfs_en_m = DFS_EN_C_M;
		break;
#endif
	default:
		break;
	}
}
#endif
