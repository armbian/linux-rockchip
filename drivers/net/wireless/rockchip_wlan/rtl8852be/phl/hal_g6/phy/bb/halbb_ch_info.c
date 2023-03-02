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

#ifdef HALBB_CH_INFO_SUPPORT

bool halbb_ch_info_wait_from_physts(struct bb_info *bb, u32 dly, u32 dly_max,
				    enum bb_physts_bitmap_t type)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	struct bb_ch_info_physts_info *ch_physts = &ch_rpt->bb_ch_info_physts_i;
	struct bb_ch_info_cr_cfg_info *cfg = &ch_rpt->bb_ch_info_cr_cfg_i;
	bool get_ch_rpt_success = false;
	u32 wait_time = 0;

	BB_DBG(bb, DBG_IC_API, "dly=%d, dly_max=%d ms\n", dly, dly_max);

	if (type == LEGACY_OFDM_PKT)
		cfg->ch_i_type = 0;
	else
		cfg->ch_i_type = 1;

	halbb_cfg_ch_info_cr(bb, cfg);

	ch_physts->ch_info_state = CH_RPT_START_TO_WAIT;
	halbb_cfg_ch_info_en(bb, CH_INFO_FROM_PHY_STS, type, bb->bb_phy_idx);

	while (wait_time <= dly_max) {
		//Delay for get physts
		BB_DBG(bb, DBG_IC_API, "wait=%d ms\n", wait_time);
		halbb_delay_ms(bb, dly);
		wait_time += dly;

		if (ch_physts->ch_info_state == CH_RPT_GETTED) {
			get_ch_rpt_success = true;
			break;
		}
	}
	halbb_cfg_ch_info_en(bb, CH_INFO_DISABLE, type, bb->bb_phy_idx);

	return get_ch_rpt_success;
}

bool halbb_chk_ch_info_cr_valid(struct bb_info *bb, struct bb_ch_info_cr_cfg_info *cfg)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	struct bb_ch_rpt_size_info *ch_rpt_size = &ch_rpt->bb_ch_rpt_size_i;
	u8 ch_matrix_nr[4];
	u8 i = 0, j = 0;
	u8 nc = 1, nr = 1;
	u8 msb_bit = 0;
	u8 mask_tmp = 0;
	u16 per_tone_size;
	u16 tone_num_table[] = {64, 128, 256, 512};
	u16 tone_num_table_he[] = {64, 128, 256, 512};
	u8 ofst_table[] = {0, 1, 2, 4};

	/*{Data_bit}*/
	if (cfg->ch_i_cmprs == 0)
		ch_rpt_size->data_byte = 1;
	else
		ch_rpt_size->data_byte = 2;

	BB_DBG(bb, DBG_IC_API, "ch_i_ele_bitmap = 0x%x\n", cfg->ch_i_ele_bitmap);

	/*{Nc}*/
	for (i = 0; i < 4; i++) {
		ch_matrix_nr[i] = (cfg->ch_i_ele_bitmap >> (8 * i)) & 0xff;
		if (i == 0 && ch_matrix_nr[i] == 0) {
			BB_WARNING("ch_matrix_nr[0] = 0\n");
			return false;
		} else if (i >= 1) {
			if (ch_matrix_nr[i] != 0) {
				nc++;
				if (ch_matrix_nr[i] != ch_matrix_nr[i - 1]) {
				    BB_WARNING("matrix_nr[%d]/[%d]=0x%x/0x%x\n",
					       i - 1, i, ch_matrix_nr[i-1], ch_matrix_nr[i]);
					return false;
				}
			}
		}
	}

	/*{Nr}*/
	for (i = 0; i < 8; i++) {
		if (ch_matrix_nr[0] & BIT(i))
			msb_bit = i;
	}
	nr = msb_bit + 1;

	mask_tmp = (u8)halbb_gen_mask_from_0(nr);
	if (ch_matrix_nr[0] != mask_tmp) {
		BB_WARNING("ch_matrix_nr[0]=0x%x, mask_tmp = 0x%x\n",
			   ch_matrix_nr[0], mask_tmp);
		return false;
	}

	ch_rpt_size->n_c = nc;
	ch_rpt_size->n_r = nr;

	/*
	Length = {Data_bit} * {I,Q} * {Nc * Nr} * {N_tone(BW) / group_num}
	       = {8 or 16} * 2 * {1'number in ele_bitmap} * {N_tone} / {1/2/4/16}
	*/
	per_tone_size = ch_rpt_size->data_byte * 2 * nc * nr;
	ch_rpt_size->per_tone_ch_rpt_size = per_tone_size;

	ch_rpt_size->ch_info_rpt_len_legcy = per_tone_size >> ofst_table[cfg->ch_i_grp_num_non_he];

	for (i = 0; i < 4; i++) {
		//N_tone;
		ch_rpt_size->ch_info_rpt_len[i] = per_tone_size >> ofst_table[cfg->ch_i_grp_num_non_he];
		ch_rpt_size->ch_info_rpt_len_he[i] = per_tone_size >> ofst_table[cfg->ch_i_grp_num_he];
	}

	return true;
}

void halbb_cfg_ch_info_cr(struct bb_info *bb, struct bb_ch_info_cr_cfg_info *cfg)
{
	struct bb_ch_info_cr_info *cr = &bb->bb_ch_rpt_i.bb_ch_info_cr_i;
	u32 val_32;

	BB_DBG(bb, DBG_IC_API,
	       "en_0/1=%d/%d, src=%d, cmprs=%d, grp_num_nhe/he=%d/%d\n",
	       cfg->ch_i_phy0_en, cfg->ch_i_phy1_en,
	       cfg->ch_i_data_src, cfg->ch_i_cmprs,
	       cfg->ch_i_grp_num_non_he, cfg->ch_i_grp_num_he);
	BB_DBG(bb, DBG_IC_API,
	       "blk_start/end_=%d/%d, bitmap=0x%x, type=%d, seg_len=%d,\n",
	       cfg->ch_i_blk_start_idx, cfg->ch_i_blk_end_idx,
	       cfg->ch_i_ele_bitmap, cfg->ch_i_type, cfg->ch_i_seg_len);

	if (!halbb_chk_ch_info_cr_valid(bb, cfg)) {
		BB_DBG(bb, DBG_IC_API, "[%s] invalid\n", __func__);
		//return;
	}

	/*CH-info Common Settings*/
	val_32 = (cfg->ch_i_data_src & 0x1) |
		 ((cfg->ch_i_cmprs & 0x1) << 1) |
		 ((cfg->ch_i_grp_num_non_he & 0x3) << 2) |
		 ((cfg->ch_i_grp_num_he & 0x3) << 4);

	halbb_set_reg(bb, cr->ch_info_en_0, 0xfc, val_32);
	halbb_set_reg(bb, cr->ele_bitmap, MASKDWORD, cfg->ch_i_ele_bitmap);
	halbb_set_reg(bb, cr->ch_info_type, cr->ch_info_type_m, cfg->ch_i_type);
}

void halbb_cfg_ch_info_en(struct bb_info *bb, enum bb_ch_info_en_t en,
			  enum bb_physts_bitmap_t bitmap, enum phl_phy_idx phy_idx)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	struct bb_ch_info_cr_info *cr = &ch_rpt->bb_ch_info_cr_i;
	u32 val_32;

	BB_DBG(bb, DBG_IC_API, "[%s] en=%d, bitmap=0x%x\n", __func__, en, bitmap);

	/*CH Report Enable*/
	val_32 = (en == CH_INFO_DISABLE) ? 0 : 1;

	if (phy_idx == HW_PHY_0)
		halbb_set_reg(bb, cr->ch_info_en_0, BIT0, val_32);
	else
		halbb_set_reg(bb, cr->ch_info_en_0, BIT1, val_32);

	/*Phy-sts IE 8 Enable*/
	if(en == CH_INFO_DISABLE || en == CH_INFO_FROM_CH_STS)
		halbb_physts_ie_bitmap_en(bb, bitmap, IE08_FTR_CH, false);
	else
		halbb_physts_ie_bitmap_en(bb, bitmap, IE08_FTR_CH, true);
}

void halbb_cfg_ch_info_buff(struct bb_info *bb, struct bb_ch_info_buf_cfg_info *cfg)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	struct bb_ch_info_cr_info *cr = &ch_rpt->bb_ch_info_cr_i;
	u32 val_32;

	BB_DBG(bb, DBG_IC_API, "[%s]\n", __func__);

	val_32 = (cfg->ch_i_blk_start_idx & 0xf) |
		 ((cfg->ch_i_blk_end_idx & 0xf) << 4);

	halbb_set_reg(bb, cr->ch_info_en_0, 0xff00, val_32);
	halbb_set_reg(bb, cr->seg_len, cr->seg_len_m, cfg->ch_i_seg_len);
}

void halbb_chanifo_self_test(struct bb_info *bb)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;

	BB_DBG(bb, DBG_IC_API, "[%s]ch_rpt = %d\n", __func__, ch_rpt->seg_idx_pre);
}

enum bb_ch_info_t halbb_ch_info_parsing(struct bb_info *bb, u8 *addr, u32 len,
					u8 *rpt_buf,
					struct bb_ch_rpt_hdr_info *hdr,
					struct bb_phy_info_rpt *phy_info,
					struct bb_ch_info_drv_rpt *drv)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	u32 last_seg_size = 0;
	u32 len_tmp = 0;

	drv->raw_data_len = len; /*@TEST_CH_SEG_LEN (12 * 8 = 96)*/

	if (!addr)
		return BB_CH_INFO_FAIL;

	if (len == 0)
		return BB_CH_INFO_FAIL;

	halbb_mem_cpy(bb, hdr, addr, ch_rpt->ch_rpt_hdr_len);

	if (hdr->set_valid == 0)
		return BB_CH_INFO_FAIL;

	drv->raw_data_len -= ch_rpt->ch_rpt_hdr_len;

	addr += ch_rpt->ch_rpt_hdr_len;

	BB_DBG(bb, DBG_IC_API, "[IN] skip=%d\n", ch_rpt->skip_ch_info);

	BB_DBG(bb, DBG_IC_API, "[Hdr]lenth_curr: %d = %d - %d\n",
		       drv->raw_data_len, len, ch_rpt->ch_rpt_hdr_len); /*@88 = 96 - 8*/

	if (hdr->is_pkt_end) {
	/*SEG = N-1 (LAST SEG)*/
		drv->seg_idx_curr = hdr->total_seg_num - 1; /*@3*/
		ch_rpt->seg_idx_pre = drv->seg_idx_curr;/*@3*/
		ch_rpt->seg_total_num = hdr->total_seg_num; /*4*/
		ch_rpt->total_len = (hdr->total_len_m << 16 | hdr->total_len_l) 
				  - ch_rpt->ch_rpt_hdr_len - ch_rpt->phy_info_len; /*@320 - 16 = 304*/

		BB_DBG(bb, DBG_IC_API,
		       "[SEG_%d Last] seg_total_num=%d, total_len_l=%d, total_len=%d, remnant=%d, skip=%d\n",
		       drv->seg_idx_curr, ch_rpt->seg_total_num, hdr->total_len_l,
		       ch_rpt->total_len, ch_rpt->total_len_remnant, ch_rpt->skip_ch_info);

		 if (ch_rpt->skip_ch_info) {

			ch_rpt->skip_ch_info = false;
			ch_rpt->total_len_remnant = ch_rpt->total_len; /*@304*/
			return BB_CH_INFO_FAIL;
		} else if (drv->seg_idx_curr != hdr->seq_num) {
			BB_DBG(bb, DBG_IC_API, "[LAST][FAIL]drv_idx=%d != hdr_idx=%d\n",
			       drv->seg_idx_curr, hdr->seq_num);

			ch_rpt->total_len_remnant = ch_rpt->total_len;  /*@304*/
			ch_rpt->skip_ch_info = false;
			return BB_CH_INFO_FAIL;
		} else if (drv->raw_data_len < ch_rpt->total_len_remnant) {
			BB_DBG(bb, DBG_IC_API, "[LAST][FAIL]raw_data_len=%d < total_len_remnant=%d\n",
			       drv->raw_data_len, ch_rpt->total_len_remnant);

			ch_rpt->total_len_remnant = ch_rpt->total_len;  /*@304*/
			ch_rpt->skip_ch_info = false;
			return BB_CH_INFO_FAIL;
		} else {

			BB_DBG(bb, DBG_IC_API, "[LAST] len_remnant=%d\n", ch_rpt->total_len_remnant);

			drv->raw_data_len = ch_rpt->total_len_remnant; /*Last remnant raw data*/
			ch_rpt->total_len_remnant = ch_rpt->total_len; /*@304*/
			halbb_mem_cpy(bb, rpt_buf, addr, drv->raw_data_len);
			return BB_CH_INFO_LAST_SEG;
		}
	}

	if (ch_rpt->skip_ch_info) {
		BB_DBG(bb, DBG_IC_API, "[OUT]skip=%d\n", ch_rpt->skip_ch_info);
		return BB_CH_INFO_FAIL;
	}

	BB_DBG(bb, DBG_IC_API, "seg_idx_pre=%d\n", ch_rpt->seg_idx_pre);

	if (ch_rpt->seg_idx_pre == (hdr->total_seg_num - 1)) { /*@3 = 4 - 1*/
	/*SEG = 0*/
		drv->seg_idx_curr = 0;
		ch_rpt->seg_idx_pre = 0;

		//phy_info = (struct bb_phy_info_rpt *)addr;
		halbb_mem_cpy(bb, phy_info, addr, ch_rpt->phy_info_len);
		addr += ch_rpt->phy_info_len;

		len_tmp = drv->raw_data_len;
		drv->raw_data_len -= ch_rpt->phy_info_len;  /*@80 = 88 - 8*/

		BB_DBG(bb, DBG_IC_API, "[PHY_info]lenth_curr: %d = %d - %d\n",
			drv->raw_data_len, len_tmp, ch_rpt->phy_info_len);

		BB_DBG(bb, DBG_IC_API, "[PHY_info]RSSI= {%d, %d, %d}\n",
			phy_info->rssi[0], phy_info->rssi[1], phy_info->rssi_avg);

		BB_DBG(bb, DBG_IC_API,
		       "[SEG_0 Fist] total{num, len}={%d, %d}, remnant=%d, skip=%d\n",
		       ch_rpt->seg_total_num, ch_rpt->total_len, ch_rpt->total_len_remnant, ch_rpt->skip_ch_info);

		if (drv->raw_data_len > ch_rpt->total_len_remnant) {
			BB_DBG(bb, DBG_IC_API, "[FAIL]raw_data_len > total_len_remnant\n");
			ch_rpt->skip_ch_info = true;
			return BB_CH_INFO_FAIL;
		} else if (phy_info->rsvd_0 != 0 || phy_info->rsvd_1 != 0 ||
		    phy_info->rsvd_2 != 0) {
		    	BB_DBG(bb, DBG_IC_API, "[FAIL]rsvd_0,1,2: %d, %d, %d\n",
		       		phy_info->rsvd_0, phy_info->rsvd_1, phy_info->rsvd_2);
			ch_rpt->skip_ch_info = true;
			return BB_CH_INFO_FAIL;
		} else if (drv->seg_idx_curr != hdr->seq_num) {
			BB_DBG(bb, DBG_IC_API, "[FAIL]drv_idx=%d != hdr_idx=%d\n",
			       drv->seg_idx_curr, hdr->seq_num);

			ch_rpt->skip_ch_info = true;
			return BB_CH_INFO_FAIL;
		} else {
			ch_rpt->total_len_remnant -= drv->raw_data_len;

			halbb_mem_cpy(bb, rpt_buf, addr, drv->raw_data_len);
			return BB_CH_INFO_SUCCESS;
		}
	} else {
	/*SEG = 1~N-2*/
		drv->seg_idx_curr = ch_rpt->seg_idx_pre + 1;
		ch_rpt->seg_idx_pre = drv->seg_idx_curr;

		if (drv->seg_idx_curr >= (hdr->total_seg_num - 1)) { /*@ >= 3*/
			BB_DBG(bb, DBG_IC_API, "[FAIL]Los Last Seg\n");
			ch_rpt->skip_ch_info = true;
			return BB_CH_INFO_FAIL;
		}

		BB_DBG(bb, DBG_IC_API,
		       "[SEG_%d Mid] total{num, len}={%d, %d}, remnant=%d, skip=%d\n",
		       drv->seg_idx_curr, ch_rpt->seg_total_num, ch_rpt->total_len, ch_rpt->total_len_remnant, ch_rpt->skip_ch_info);

		BB_DBG(bb, DBG_IC_API, "lenth_curr: %d\n", drv->raw_data_len); /*@88 = 96 - 8*/

		if (drv->raw_data_len > ch_rpt->total_len_remnant) {
			BB_DBG(bb, DBG_IC_API, "[FAIL]raw_data_len > total_len_remnant\n");
			ch_rpt->skip_ch_info = true;
			return BB_CH_INFO_FAIL;
		} else if (drv->seg_idx_curr != hdr->seq_num) {
			BB_DBG(bb, DBG_IC_API, "[FAIL]drv_idx=%d != hdr_idx=%d\n",
			       drv->seg_idx_curr, hdr->seq_num);
			ch_rpt->skip_ch_info = true;
			return BB_CH_INFO_FAIL;
		} else {
			ch_rpt->total_len_remnant -= drv->raw_data_len;
			halbb_mem_cpy(bb, rpt_buf, addr, drv->raw_data_len);
			return BB_CH_INFO_SUCCESS;
		}
	}
}

void halbb_ch_info_bbcr_init(struct bb_info *bb)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	struct bb_ch_info_cr_cfg_info *cfg = &ch_rpt->bb_ch_info_cr_cfg_i;

	BB_DBG(bb, DBG_IC_API, "[%s]\n", __func__);

	cfg->ch_i_data_src = 0;
	cfg->ch_i_cmprs = 1;
	cfg->ch_i_grp_num_non_he = 3;
	cfg->ch_i_grp_num_he = 3;
	cfg->ch_i_blk_start_idx = 9;
	cfg->ch_i_blk_end_idx = 10;
	cfg->ch_i_ele_bitmap = 0x303; /*Nr X Nc: 2 X 2*/
	cfg->ch_i_type = 1;
	cfg->ch_i_seg_len = 0;
	halbb_cfg_ch_info_cr(bb, cfg);
}

void halbb_ch_info_init(struct bb_info *bb)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	struct bb_ch_info_physts_info *physts = &bb->bb_ch_rpt_i.bb_ch_info_physts_i;
	struct bb_ch_info_raw_info *buf = &bb->bb_ch_rpt_i.bb_ch_info_raw_i;

	halbb_ch_info_bbcr_init(bb);
	ch_rpt->ch_rpt_hdr_len = sizeof(struct bb_ch_rpt_hdr_info); /*8852A: 8Byte*/
	ch_rpt->phy_info_len = sizeof(struct bb_phy_info_rpt); /*8852A: 8Byte*/
	ch_rpt->skip_ch_info = true;

	//Init Parameter
	buf->ch_info_buf_len = 1024;
	physts->bitmap_type_auto_en = true;

	BB_DBG(bb, DBG_IC_API, "[%s] hdr_len = %d, phy_info_len=%d\n", __func__, ch_rpt->ch_rpt_hdr_len, ch_rpt->phy_info_len);
}

void halbb_ch_info_print_buf(struct bb_info *bb)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	u16 i = 0;
	u8 *buf_tmp;

	if (!ch_rpt->test_buf)
		return;

	buf_tmp = ch_rpt->test_buf;
	BB_DBG(bb, DBG_IC_API, "----------------------------\n");
	for (i = 0; i < ((TEST_CH_INFO_MAX_SEG * TEST_CH_SEG_LEN) >> 3); i++) {
			BB_DBG(bb, DBG_IC_API, "[%02d] 0x%016llx\n", i, *(u64 *)buf_tmp);
			buf_tmp += 8;

			if (i == 9 || i == 20 || i == 31 || i == 37 || i == 42)
				BB_DBG(bb, DBG_IC_API, "----------------------------\n");
	}
	BB_DBG(bb, DBG_IC_API, "----------------------------\n");
}

void halbb_ch_info_buf_rls(struct bb_info *bb)
{
	struct bb_ch_info_raw_info *buf = &bb->bb_ch_rpt_i.bb_ch_info_raw_i;

	if (!buf->octet)
		return;

	BB_DBG(bb, DBG_IC_API,"[%s]\n", __func__);

	halbb_mem_free(bb, buf->octet, buf->ch_info_buf_len);
	buf->octet = 0;
}

bool halbb_ch_info_buf_alloc(struct bb_info *bb)
{
	struct bb_ch_info_raw_info *buf = &bb->bb_ch_rpt_i.bb_ch_info_raw_i;

	if (buf->octet)
		return true;

	BB_DBG(bb, DBG_IC_API,"[%s]\n", __func__);

	buf->octet = (s16 *)halbb_mem_alloc(bb, buf->ch_info_buf_len);

	if (!buf->octet)
		return false;
	
	return true;
}

void halbb_ch_info_reset(struct bb_info *bb)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;

	if (ch_rpt->test_buf)
		halbb_mem_set(bb, ch_rpt->test_buf, 0, (TEST_CH_INFO_MAX_SEG * TEST_CH_SEG_LEN));
}

void halbb_ch_info_deinit(struct bb_info *bb)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;

	halbb_ch_info_buf_rls(bb);
	if (ch_rpt->test_buf)
		halbb_mem_free(bb, ch_rpt->test_buf, (TEST_CH_INFO_MAX_SEG * TEST_CH_SEG_LEN));
}
void halbb_ch_info_dbg(struct bb_info *bb, char input[][16], u32 *_used,
		       char *output, u32 *_out_len)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	struct bb_ch_info_raw_info *buf = &ch_rpt->bb_ch_info_raw_i;
	struct bb_ch_info_cr_cfg_info *cfg = &ch_rpt->bb_ch_info_cr_cfg_i;
	struct bb_ch_rpt_size_info *ch_rpt_size = &ch_rpt->bb_ch_rpt_size_i;
	struct bb_ch_info_physts_info *ch_physts = &ch_rpt->bb_ch_info_physts_i;
	struct bb_pkt_cnt_su_store_info *store = &bb->bb_cmn_rpt_i.bb_pkt_cnt_su_store_i;
	struct bb_ch_rpt_hdr_info ch_rpt_hdr = {0};
	struct bb_phy_info_rpt phy_rpt_in;
	struct bb_ch_rpt_hdr_info hdr; /*output*/
	struct bb_phy_info_rpt phy_rpt; /*output*/
	struct bb_ch_info_drv_rpt drv_rpt; /*output*/
	enum bb_ch_info_t rpt; /*output*/
	enum bb_physts_bitmap_t bitmap_type;
	u8 grp_num_tab[4] = {1, 2, 4, 16};
	bool val_bool;
	u8 seg_idx;
	u8 *addr, *addr_tmp, *addr_ori;
	u32 len = TEST_CH_SEG_LEN; /*Byte*/
	u32 val[11] = {0};
	u16 i = 0;
	u16 tone_num = 0;

	halbb_mem_set(bb, &phy_rpt_in, 0, sizeof(phy_rpt_in));
	halbb_mem_set(bb, &hdr, 0, sizeof(hdr));
	halbb_mem_set(bb, &phy_rpt, 0, sizeof(phy_rpt));
	halbb_mem_set(bb, &drv_rpt, 0, sizeof(drv_rpt));

	if (_os_strcmp(input[1], "-h") == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{self_test}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{print}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{rst}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{test} {seg_idx:0~3}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "trig_psts\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "cfg show\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "cfg para0 {0: auto, 6~7: HE/VHT_MU, 12~15: Lgcy/HT/VHT/HE}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "cfg para1 {0/1: 1/2 Byte} {grp: 0~3:1/2/4/16} {H_map(Hex)}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "cfg para2 {seg_len}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "cfg para3 {0:LS, 1:CS}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "cfg blk {start_idx} {end_idx}\n");
		return;
	}

	if (_os_strcmp(input[1], "self_test") == 0) {
		halbb_chanifo_self_test(bb);
	} else if (_os_strcmp(input[1], "print") == 0) {
		halbb_ch_info_print_buf(bb);
	} else if (_os_strcmp(input[1], "rst") == 0) {
		halbb_ch_info_reset(bb);
	} else if (_os_strcmp(input[1], "trig_physts") == 0) {

		if(!halbb_ch_info_buf_alloc(bb)) {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Buff alloc fail\n");
			return;
		}

		if (ch_physts->bitmap_type_auto_en) {
			if (store->he_pkt_not_zero)
				bitmap_type = HE_PKT;
			else if (store->vht_pkt_not_zero)
				bitmap_type = VHT_PKT;
			else if (store->ht_pkt_not_zero)
				bitmap_type = HT_PKT;
			else
				bitmap_type = LEGACY_OFDM_PKT;
		} else {
			bitmap_type = ch_physts->force_bitmap_type;
		}
		
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Auto Type=%d\n", ch_physts->bitmap_type_auto_en);

		val_bool = halbb_ch_info_wait_from_physts(bb, 100, 500, bitmap_type);

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Trigger Type[%s], BW=%d, success=%d\n",
			    bb_physts_bitmap_type_t[bitmap_type],
			    20 << bb->hal_com->band[0].cur_chandef.bw, val_bool);

		if (!val_bool)
			return;

		tone_num = HALBB_DIV(ch_physts->ch_info_len, ch_rpt_size->per_tone_ch_rpt_size);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[CH-Info Rpt][%s] len:%d, per_tone_size=%d, tone_num=%d\n",
			    bb_physts_bitmap_type_t[ch_physts->bitmap_type_rpt],
			    ch_physts->ch_info_len, ch_rpt_size->per_tone_ch_rpt_size,
			    tone_num);
#if 0
		if (buf->octet &&
		    buf->ch_info_buf_len > ch_physts->ch_info_len &&
		    ch_physts->ch_info_len != 0) {
			halbb_print_buff_64(bb, (u8*)buf->octet, ch_physts->ch_info_len);
		}
#endif
	} else if (_os_strcmp(input[1], "cfg") == 0) {

		for (i = 0; i <= 4; i++)
			HALBB_SCAN(input[3 + i], DCMD_DECIMAL, &val[i]);

		if (_os_strcmp(input[2], "show") == 0) {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "src=%d, cmprs=%d, grp_num_nhe/he=%d/%d\n",
				    cfg->ch_i_data_src, cfg->ch_i_cmprs,
				    cfg->ch_i_grp_num_non_he, cfg->ch_i_grp_num_he);
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "blk_start/end_=%d/%d, bitmap=0x%x, type=%d, seg_len=%d\n",
				    cfg->ch_i_blk_start_idx, cfg->ch_i_blk_end_idx,
				    cfg->ch_i_ele_bitmap, cfg->ch_i_type, cfg->ch_i_seg_len);
			return;
		} else if (_os_strcmp(input[2], "para0") == 0) {
			if (val[0] == 0) {
				ch_physts->bitmap_type_auto_en = true;
				return;
			}
			ch_physts->bitmap_type_auto_en = false;
			ch_physts->force_bitmap_type = (enum bb_physts_bitmap_t)val[0];

			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "force_bitmap_type=%s\n",
				    bb_physts_bitmap_type_t[ch_physts->force_bitmap_type]);
		} else if (_os_strcmp(input[2], "para1") == 0) {
			HALBB_SCAN(input[5], DCMD_HEX, &val[2]);
			
			cfg->ch_i_cmprs = (bool)val[0];
			if (val[1] < 4) {
				cfg->ch_i_grp_num_non_he = (u8)val[1];
				cfg->ch_i_grp_num_he = (u8)val[1];
			}
			cfg->ch_i_ele_bitmap = val[2];

			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "data_wl=%d Byte, grp_num=%d, H_map=0x%x\n",
				    1 << cfg->ch_i_cmprs, grp_num_tab[val[1]],
				    cfg->ch_i_ele_bitmap);
		} else if (_os_strcmp(input[2], "para2") == 0) {
			cfg->ch_i_seg_len = (u8)val[0];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "seg_len=%d\n",
				    cfg->ch_i_seg_len);
		} else if (_os_strcmp(input[2], "para3") == 0) {
			cfg->ch_i_data_src = (bool)val[0];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "src=%d\n", cfg->ch_i_data_src);
		} else if (_os_strcmp(input[2], "blk") == 0) {
			cfg->ch_i_blk_start_idx = (u8)val[0];
			cfg->ch_i_blk_end_idx = (u8)val[1];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "blk_start/end_=%d/%d\n",
				    cfg->ch_i_blk_start_idx, cfg->ch_i_blk_end_idx);
		} else {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Set Err\n");
			return;
		}
		halbb_cfg_ch_info_cr(bb, cfg);

	} else if (_os_strcmp(input[1], "test") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		seg_idx = (u8)val[0];

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[SEG:%d] Input\n", seg_idx);

		/*Rpt Buff*/
		if (!ch_rpt->test_buf)
			ch_rpt->test_buf = (u8 *)halbb_mem_alloc(bb, (TEST_CH_INFO_MAX_SEG * TEST_CH_SEG_LEN));

		/*Data Buff*/
		addr = (u8 *)halbb_mem_alloc(bb, len);
		addr_ori = addr;

		if (!addr)
			return;

		/*HDR Info*/
		/*seg_0:[2(hdr) + 10(data)] + seg_1:11(data) + seg_2:11(data) + seg_3:6(data) = 40, 40*8=320*/
		ch_rpt_hdr.total_len_l = 320;
		ch_rpt_hdr.total_seg_num = TEST_CH_INFO_MAX_SEG;
		ch_rpt_hdr.set_valid = 1;
		ch_rpt_hdr.segment_size = TEST_CH_SEG_LEN >> 3;
		ch_rpt_hdr.seq_num= seg_idx;
		
		if (seg_idx == TEST_CH_INFO_MAX_SEG - 1) {
			ch_rpt_hdr.is_pkt_end = 1;
		} else {
			ch_rpt_hdr.is_pkt_end = 0;
		}

		halbb_mem_cpy(bb, addr, &ch_rpt_hdr, ch_rpt->ch_rpt_hdr_len);
		addr += ch_rpt->ch_rpt_hdr_len;

		if (seg_idx == 0) {
			phy_rpt_in.rssi[0] = 50;
			phy_rpt_in.rssi[1] = 70;
			phy_rpt_in.rssi_avg = 60;
			phy_rpt_in.rsvd_0 = 0;
			phy_rpt_in.rsvd_1 = 0;
			phy_rpt_in.rsvd_2 = 0;

			halbb_mem_cpy(bb, addr, &phy_rpt_in, ch_rpt->phy_info_len);
			addr += ch_rpt->phy_info_len;
			ch_rpt->test_buf_curr = ch_rpt->test_buf;
		}

		if (seg_idx == TEST_CH_INFO_MAX_SEG - 1) {
		/*SEG3 (LAST)*/
			for (i = 0; i < (TEST_CH_SEG_LEN - 8 - 40); i++) {
				*addr = (u8)i;
				addr++;
			}
		} else if (seg_idx == 0) {
		/*SEG 0*/
			for (i = 0; i < (TEST_CH_SEG_LEN - 16); i++) {
				*addr = (u8)i;
				addr++;
			}
		} else {
		/*SEG 1,2*/
			for (i = 0; i < (TEST_CH_SEG_LEN - 8); i++) {
				*addr = (u8)i;
				addr++;
			}
		}

		addr_tmp = addr_ori;
		for (i = 0; i < (TEST_CH_SEG_LEN >> 3); i++) {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "[%02d] 0x%016llx\n", i, *(u64 *)addr_tmp);
			addr_tmp += 8;
		}

		rpt = halbb_ch_info_parsing(bb, addr_ori, len, ch_rpt->test_buf_curr, &hdr, &phy_rpt, &drv_rpt);

		ch_rpt->raw_data_len_acc += drv_rpt.raw_data_len;
		ch_rpt->test_buf_curr += drv_rpt.raw_data_len;

		if (rpt == BB_CH_INFO_SUCCESS) {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "[Result] Success\n");
		} else {
			if (rpt == BB_CH_INFO_LAST_SEG) {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "[Result] Success, Last Seg\n");
				halbb_ch_info_print_buf(bb);

			} else {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					    "[Result] Fail\n");
			}
			halbb_ch_info_reset(bb);
			ch_rpt->raw_data_len_acc = 0;
			ch_rpt->test_buf_curr = ch_rpt->test_buf;
		}
			
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "[ACC] remnant=%d, raw_data_len_acc=%d\n",
			ch_rpt->total_len_remnant, ch_rpt->raw_data_len_acc);

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "[SEG] all_len=%d = Hdr_len + raw_data_len=%d\n",
			 len, drv_rpt.raw_data_len);

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "[SEG] total_len=%d, seg_size=%d, end=%d, sts0_evm=%d, seq_num=%d\n",
			 hdr.total_len_l, hdr.segment_size, hdr.is_pkt_end, hdr.sts0_evm, hdr.seq_num);

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "[IN]  SEG[%d]: rssi{0,1,avg}={%02d,%02d,%02d}, sts1_evm=%d, rsvd{0,1,2}={%d,%d,%d}\n",
			 ch_rpt_hdr.seq_num,
			 phy_rpt_in.rssi[0], phy_rpt_in.rssi[1],
			 phy_rpt_in.rssi_avg, (phy_rpt_in.sts1_evm_m << 4 | phy_rpt_in.sts1_evm_l) ,
			 phy_rpt_in.rsvd_0, phy_rpt_in.rsvd_1,
			 phy_rpt_in.rsvd_2);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "[OUT] SEG[%d]: rssi{0,1,avg}={%02d,%02d,%02d}, sts1_evm=%d, rsvd{0,1,2}={%d,%d,%d}\n\n",
			 drv_rpt.seg_idx_curr,
			 phy_rpt.rssi[0], phy_rpt.rssi[1],
			 phy_rpt.rssi_avg, (phy_rpt.sts1_evm_m << 4 | phy_rpt.sts1_evm_l),
			 phy_rpt.rsvd_0, phy_rpt.rsvd_1,
			 phy_rpt.rsvd_2);

		halbb_mem_free(bb, addr_ori, len);
	} else {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Set Err\n");
	}
}

void halbb_cr_cfg_ch_info_init(struct bb_info *bb)
{
	struct bb_ch_rpt_info *ch_rpt = &bb->bb_ch_rpt_i;
	struct bb_ch_info_cr_info *cr = &ch_rpt->bb_ch_info_cr_i;
	
	switch (bb->cr_type) {

	#ifdef HALBB_COMPILE_AP_SERIES
	case BB_AP:
		cr->ch_info_en_0 = INTF_R_CH_INFO_EN_P0_A;
		cr->ele_bitmap = INTF_R_ELE_BITMAP_A;
		cr->ch_info_type = CH_INFO_TYPE_A;
		cr->ch_info_type_m = CH_INFO_TYPE_A_M;
		cr->seg_len = CH_INFO_SEG_LEN_A;
		cr->seg_len_m = CH_INFO_SEG_LEN_A_M;
		break;
	#endif
	#ifdef HALBB_COMPILE_CLIENT_SERIES
	case BB_CLIENT:
		cr->ch_info_en_0 = INTF_R_CH_INFO_EN_P0_C;
		cr->ele_bitmap = INTF_R_ELE_BITMAP_C;
		cr->ch_info_type = CH_INFO_TYPE_C;
		cr->ch_info_type_m = CH_INFO_TYPE_C_M;
		cr->seg_len = CH_INFO_SEG_LEN_C;
		cr->seg_len_m = CH_INFO_SEG_LEN_C_M;
		break;
	#endif

	default:
		break;
	}

}

#endif
