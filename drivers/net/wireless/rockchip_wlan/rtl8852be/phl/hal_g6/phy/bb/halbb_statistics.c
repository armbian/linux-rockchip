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

#ifdef HALBB_STATISTICS_SUPPORT

void halbb_set_crc32_cnt2_rate(struct bb_info *bb, u16 rate_idx)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_usr_set_info *usr_set = &stat_t->bb_usr_set_i;
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;
	bool is_ofdm_rate = halbb_is_ofdm_rate(bb, rate_idx);
	bool is_ht_rate = halbb_is_ht_rate(bb, rate_idx);
	bool is_vht_rate = halbb_is_vht_rate(bb, rate_idx);
	bool is_he_rate = halbb_is_he_rate(bb, rate_idx);
	u32 reg_addr = cr->intf_r_rate;
	u32 ofdm_rate_bitmask = cr->intf_r_rate_m;
	u32 ht_mcs_bitmask = cr->intf_r_mcs_m;
	u32 vht_mcs_bitmask = cr->intf_r_vht_mcs_m;
	u32 vht_ss_bitmask = cr->intf_r_vht_nss_m;
	u32 he_mcs_bitmask =cr->intf_r_he_mcs_m;
	u32 he_ss_bitmask = cr->intf_r_he_nss_m;
	u8 rate_digi = 0x0;
	u8 ss = 0x0;

	if (!is_ofdm_rate && !is_ht_rate && !is_vht_rate && !is_he_rate)
		BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] rate_idx = (0x%x) Not support !!\n",
			  rate_idx);

	if (is_ofdm_rate) {
		rate_digi = halbb_legacy_rate_2_spec_rate(bb, rate_idx);
		halbb_set_reg(bb, reg_addr, ofdm_rate_bitmask, rate_digi);
		usr_set->ofdm2_rate_idx = rate_idx;
		usr_set->ht2_rate_idx = 0;
		usr_set->vht2_rate_idx = 0;
		usr_set->he2_rate_idx = 0;
	} else if (is_ht_rate) {
		rate_digi = halbb_rate_2_rate_digit(bb, rate_idx);
		halbb_set_reg(bb, reg_addr, ht_mcs_bitmask, rate_digi);
		usr_set->ht2_rate_idx = rate_idx;
		usr_set->ofdm2_rate_idx = 0;
		usr_set->vht2_rate_idx = 0;
		usr_set->he2_rate_idx = 0;
	} else if (is_vht_rate) {
		rate_digi = halbb_rate_2_rate_digit(bb, rate_idx);
		ss = halbb_rate_to_num_ss(bb, rate_idx);
		halbb_set_reg(bb, reg_addr, vht_mcs_bitmask, rate_digi);
		halbb_set_reg(bb, reg_addr, vht_ss_bitmask, ss - 1);
		usr_set->vht2_rate_idx = rate_idx;
		usr_set->ofdm2_rate_idx = 0;
		usr_set->ht2_rate_idx = 0;
		usr_set->he2_rate_idx = 0;
	} else if (is_he_rate) {
		rate_digi = halbb_rate_2_rate_digit(bb, rate_idx);
		ss = halbb_rate_to_num_ss(bb, rate_idx);
		halbb_set_reg(bb, reg_addr, he_mcs_bitmask, rate_digi);
		halbb_set_reg(bb, reg_addr, he_ss_bitmask, ss - 1);
		usr_set->he2_rate_idx = rate_idx;
		usr_set->ofdm2_rate_idx = 0;
		usr_set->ht2_rate_idx = 0;
		usr_set->vht2_rate_idx = 0;
	}
}

void halbb_set_crc32_cnt3_format(struct bb_info *bb, u8 usr_type_sel)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_fa_info *fa = &stat_t->bb_fa_i;
	struct bb_cca_info *cca = &stat_t->bb_cca_i;
	struct bb_crc_info *crc = &stat_t->bb_crc_i;
	struct bb_crc2_info *crc2 = &stat_t->bb_crc2_i;
	struct bb_usr_set_info *usr_set = &stat_t->bb_usr_set_i;
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;
	u32 reg_addr = cr->intf_r_mac_hdr_type;
	u32 type_bitmask = cr->intf_r_mac_hdr_type_m;
	
	usr_set->stat_type_sel_i = usr_type_sel;
	usr_set->stat_mac_type_i = TYPE_DATA;

	switch(usr_set->stat_type_sel_i) {
		case STATE_PROBE_RESP:
			usr_set->stat_mac_type_i = TYPE_PROBE_RESP;
			break;
		case STATE_BEACON:
			usr_set->stat_mac_type_i = TYPE_BEACON;
			break;
		case STATE_ACTION:
			usr_set->stat_mac_type_i = TYPE_ACTION;
			break;
		case STATE_BFRP:
			usr_set->stat_mac_type_i = TYPE_BFRP;
			break;
		case STATE_NDPA:
			usr_set->stat_mac_type_i = TYPE_NDPA;
			break;
		case STATE_BA:
			usr_set->stat_mac_type_i = TYPE_BA;
			break;
		case STATE_RTS:
			usr_set->stat_mac_type_i = TYPE_RTS;
			break;
		case STATE_CTS:
			usr_set->stat_mac_type_i = TYPE_CTS;
			break;
		case STATE_ACK:
			usr_set->stat_mac_type_i = TYPE_ACK;
			break;
		case STATE_DATA:
			usr_set->stat_mac_type_i = TYPE_DATA;
			break;
		case STATE_NULL:
			usr_set->stat_mac_type_i = TYPE_NULL;
			break;
		case STATE_QOS:
			usr_set->stat_mac_type_i = TYPE_QOS;
			break;
		default:
			BB_DBG(bb, DBG_FA_CNT,
  			  "[STAT] MAC frame type cnt: Not support !!!\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] Please choose one of the following options\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {1: Probe Request}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {2: Beacon}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {3: Action}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {4: Beamforming Report Poll}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {5: NDPA}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {6: BA}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {7: RTS}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {8: CTS}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {9: ACK}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {10: Data}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {11: Null}\n");
			BB_DBG(bb, DBG_FA_CNT,
			  "[STAT] {12: QoS Data}\n");
			break;
	}
	halbb_set_reg(bb, reg_addr, type_bitmask, usr_set->stat_mac_type_i);
}

void halbb_crc32_cnt_dbg(struct bb_info *bb, char input[][16], u32 *_used,
			 char *output, u32 *_out_len)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	char help[] = "-h";
	u32 var[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u8 i = 0;
	u16 rate = 0x0;
	u8 usr_type_sel = 0;

	if ((_os_strcmp(input[1], help) == 0)) {
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "Get CRC_OK/error for specific rate_idx or mac hdr type\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "============== Specific rate cnt ==============\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {1} {rate_idx in decimal}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "---------- Specific MAC header type ----------\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {1: Probe Request}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {2: Beacon}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {3: Action}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {4: Beamforming Report Poll}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {5: NDPA}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {6: BA}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {7: RTS}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {8: CTS}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {9: ACK}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {10: Data}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {11: Null}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "stat {2} {12: QoS Data}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Chk hang Auto recovery enable: {3} {en}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Chk hang limit: {4} {#limit}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "============== Notes ==============\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "------------ Rate_idx ------------\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "CCK_idx: 0~3\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "OFDM_idx: 4~11\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "HT_idx: 128~\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "VHT_1ss_idx: 256~265\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "VHT_2ss_idx: 272~281\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "HE_1ss_idx: 384~395\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "HE_2ss_idx: 400~411\n");
	} else {
		HALBB_SCAN(input[1], DCMD_DECIMAL, &var[0]);

		if (var[0] == 1) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			rate = (u16)var[1];
			BB_DBG_CNSL(out_len, used, output + used, out_len - used,
				   "{rate}={0x%x}", rate);
			halbb_set_crc32_cnt2_rate(bb, rate);
		} else if (var[0] == 2) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			usr_type_sel = (u8)var[1];
			BB_DBG_CNSL(out_len, used, output + used, out_len - used,
				   "{MAC header type}={%d}", usr_type_sel);
			halbb_set_crc32_cnt3_format(bb, usr_type_sel);
		} else if (var[0] == 3) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			stat_t->hang_recovery_en = (u8)var[1];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Chk hang Auto recovery enable=%d\n", stat_t->hang_recovery_en);
		} else if (var[0] == 4) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			stat_t->chk_hang_limit = (u8)var[1];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Chk hang limit=%d\n", stat_t->chk_hang_limit);
		} 
	}	
	
	*_used = used;
	*_out_len = out_len;
}

void halbb_print_cnt3(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_fa_info *fa = &stat_t->bb_fa_i;
	struct bb_cca_info *cca = &stat_t->bb_cca_i;
	struct bb_crc_info *crc = &stat_t->bb_crc_i;
	struct bb_usr_set_info *usr_set = &stat_t->bb_usr_set_i;
	struct bb_crc2_info *crc2 = &stat_t->bb_crc2_i;
	//char dbg_buf[HALBB_SNPRINT_SIZE] = {0};
	u32 tmp = 0;
	u8 pcr = 0;
	tmp = crc2->cnt_ofdm3_crc32_ok + crc2->cnt_ofdm3_crc32_error;
	
	if (bb->hal_com->dbcc_en) {
		BB_DBG(bb, DBG_FA_CNT, "[DBCC!!!!]===>\n");
		BB_DBG(bb, DBG_FA_CNT, "[The following statistics is at %s]===>\n", phy_idx == HW_PHY_0 ? "PHY-0" : "PHY-1");
	}
	
	switch(usr_set->stat_type_sel_i) {
	case STATE_PROBE_RESP:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[Probe Response Data CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_BEACON:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[Beacon CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_ACTION:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[Action CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_BFRP:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[BFRP CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_NDPA:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[NDPA CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_BA:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[BA CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_RTS:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[RTS CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_CTS:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[CTS CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_ACK:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[ACK CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_DATA:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[DATA CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_NULL:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[Null CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	case STATE_QOS:
		pcr = (u8)HALBB_DIV(crc2->cnt_ofdm3_crc32_ok * 100, tmp);
		BB_DBG(bb, DBG_FA_CNT,
		  "[QoS CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
		  crc2->cnt_ofdm3_crc32_error,
		  crc2->cnt_ofdm3_crc32_ok, pcr);
		break;
	default:
		break;
	}

}

void halbb_print_cnt2(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_crc2_info *crc2 = &stat_t->bb_crc2_i;
	struct bb_usr_set_info *usr_set = &stat_t->bb_usr_set_i;
	u32 tmp = 0;
	//char dbg_buf[HALBB_SNPRINT_SIZE] = {0};

	if (bb->hal_com->dbcc_en) {
		BB_DBG(bb, DBG_FA_CNT, "[DBCC!!!!]===>\n");
		BB_DBG(bb, DBG_FA_CNT, "[The following statistics is at %s]===>\n", phy_idx == HW_PHY_0 ? "PHY-0" : "PHY-1");
	}
	
	if (usr_set->ofdm2_rate_idx) {
		tmp = crc2->cnt_ofdm2_crc32_error + crc2->cnt_ofdm2_crc32_ok;
		crc2->ofdm2_pcr = (u8)HALBB_DIV(crc2->cnt_ofdm2_crc32_ok * 100,
						tmp);
		halbb_print_rate_2_buff(bb, usr_set->ofdm2_rate_idx, RTW_GILTF_LGI_4XHE32, bb->dbg_buf,
					HALBB_SNPRINT_SIZE);
		BB_DBG(bb, DBG_FA_CNT,
			  "[OFDM:%s CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
			  bb->dbg_buf, crc2->cnt_ofdm2_crc32_error,
			  crc2->cnt_ofdm2_crc32_ok, crc2->ofdm2_pcr);
	} else if (usr_set->ht2_rate_idx) {
		tmp = crc2->cnt_ht2_crc32_error + crc2->cnt_ht2_crc32_ok;
		crc2->ht2_pcr = (u8)HALBB_DIV(crc2->cnt_ht2_crc32_ok * 100,
					      tmp);
		halbb_print_rate_2_buff(bb, usr_set->ht2_rate_idx, RTW_GILTF_LGI_4XHE32, bb->dbg_buf,
					HALBB_SNPRINT_SIZE);
		BB_DBG(bb, DBG_FA_CNT,
			  "[HT:%s CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
			  bb->dbg_buf, crc2->cnt_ht2_crc32_error,
			  crc2->cnt_ht2_crc32_ok, crc2->ht2_pcr);
	} else if(usr_set->vht2_rate_idx) {
		tmp = crc2->cnt_vht2_crc32_error +
		      crc2->cnt_vht2_crc32_ok;
		crc2->vht2_pcr = (u8)HALBB_DIV(crc2->cnt_vht2_crc32_ok *
					       100, tmp);
		halbb_print_rate_2_buff(bb, usr_set->vht2_rate_idx,
					RTW_GILTF_LGI_4XHE32, bb->dbg_buf, HALBB_SNPRINT_SIZE);
		BB_DBG(bb, DBG_FA_CNT,
			  "[VHT:%s CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
			  bb->dbg_buf, crc2->cnt_vht2_crc32_error,
			  crc2->cnt_vht2_crc32_ok, crc2->vht2_pcr);
	} else if (usr_set->he2_rate_idx) {
		tmp = crc2->cnt_he2_crc32_error +
		      crc2->cnt_he2_crc32_ok;
		crc2->he2_pcr = (u8)HALBB_DIV(crc2->cnt_he2_crc32_ok *
					       100, tmp);
		halbb_print_rate_2_buff(bb, usr_set->he2_rate_idx,
					RTW_GILTF_LGI_4XHE32, bb->dbg_buf, HALBB_SNPRINT_SIZE);
		BB_DBG(bb, DBG_FA_CNT,
			  "[HE:%s CRC32 Cnt] {error, ok}= {%d, %d} (PCR=%d percent)\n",
			  bb->dbg_buf, crc2->cnt_he2_crc32_error,
			  crc2->cnt_he2_crc32_ok, crc2->he2_pcr);
	}
}

void halbb_chk_hang(struct bb_info *bb)
{
	struct bb_stat_info *stat = &bb->bb_stat_i;
    struct bb_cca_info *cca = &stat->bb_cca_i;
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;
    bool chk_hang_en = false;

    /* According to sd4 info., when rx hang --> all cca brk, half of brk is l-sig brk */
    chk_hang_en = (cca->cnt_cca_all == 0);

    if (chk_hang_en) {
		BB_DBG(bb, DBG_FA_CNT, "[CHK-HANG] ReasonCode:RHAX-2 (POP HANG)\n\n");
		stat->chk_hang_cnt = stat->chk_hang_cnt + 1;

		BB_DBG(bb, DBG_FA_CNT, "[CHK-HANG] hang_cnt=%d, hang_limit=%d, recovery_en=%d\n",
			  stat->chk_hang_cnt, stat->chk_hang_limit, stat->hang_recovery_en);

		if (stat->hang_recovery_en && (stat->chk_hang_cnt >= stat->chk_hang_limit)) {

			BB_DBG(bb, DBG_FA_CNT, "[CHK-HANG] Change PoP counter limit\n");
			halbb_set_reg(bb, cr->max_cnt_pop, cr->max_cnt_pop_m, 0x0);
			halbb_delay_us(bb, 1);
			halbb_set_reg(bb, cr->max_cnt_pop, cr->max_cnt_pop_m, 0x50);

			stat->chk_hang_cnt = 0;
		}
	}
}

void halbb_print_cnt(struct bb_info *bb, bool cck_en, enum phl_phy_idx phy_idx, enum phl_phy_idx phy_idx_2)
{

	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_fa_info *fa = &stat_t->bb_fa_i;
	struct bb_cck_fa_info *cck_fa = &fa->bb_cck_fa_i;
	struct bb_legacy_fa_info *legacy_fa = &fa->bb_legacy_fa_i;
	struct bb_ht_fa_info *ht_fa = &fa->bb_ht_fa_i;
	struct bb_vht_fa_info *vht_fa = &fa->bb_vht_fa_i;
	struct bb_he_fa_info *he_fa = &fa->bb_he_fa_i;
	struct bb_cca_info *cca = &stat_t->bb_cca_i;
	struct bb_crc_info *crc = &stat_t->bb_crc_i;
	struct bb_crc2_info *crc2 = &stat_t->bb_crc2_i;
	struct bb_tx_cnt_info *tx = &stat_t->bb_tx_cnt_i;
	struct rtw_hal_com_t *hal = bb->hal_com;
	struct rtw_hal_stat_info *stat_info = &hal->band[bb->bb_phy_idx].stat_info;

	if (bb->hal_com->dbcc_en) {
		if (!cck_en) {
			fa->cnt_fail_all = fa->cnt_ofdm_fail;
			cca->cnt_cca_all = cca->cnt_ofdm_cca;
		} else {
			fa->cnt_fail_all = fa->cnt_ofdm_fail +
					fa->cnt_cck_fail;
			cca->cnt_cca_all = cca->cnt_cck_cca +
					    cca->cnt_ofdm_cca;
		}
	} else {
		if (!cck_en) {
			fa->cnt_fail_all = fa->cnt_ofdm_fail;
			cca->cnt_cca_all = cca->cnt_ofdm_cca;
		} else {
			fa->cnt_fail_all = fa->cnt_ofdm_fail +
					fa->cnt_cck_fail;
			cca->cnt_cca_all = cca->cnt_cck_cca +
					    cca->cnt_ofdm_cca;
		}
		
	}

	crc->cnt_crc32_error_all = crc->cnt_he_crc32_error +
				   crc->cnt_vht_crc32_error +
				   crc->cnt_ht_crc32_error +
				   crc->cnt_ofdm_crc32_error +
				   crc->cnt_cck_crc32_error;

	crc->cnt_crc32_ok_all = crc->cnt_he_crc32_ok +
				 crc->cnt_vht_crc32_ok +
				 crc->cnt_ht_crc32_ok +
				 crc->cnt_ofdm_crc32_ok +
				 crc->cnt_cck_crc32_ok;

	stat_info->cnt_fail_all = fa->cnt_fail_all;
	stat_info->cnt_cck_fail = fa->cnt_cck_fail;
	stat_info->cnt_ofdm_fail = fa->cnt_ofdm_fail;
	stat_info->cnt_cca_all = cca->cnt_cca_all;
	stat_info->cnt_ofdm_cca = cca->cnt_ofdm_cca;
	stat_info->cnt_cck_cca = cca->cnt_cck_cca;
	stat_info->cnt_crc32_error_all = crc->cnt_crc32_error_all;
	stat_info->cnt_he_crc32_error = crc->cnt_he_crc32_error;
	stat_info->cnt_vht_crc32_error = crc->cnt_vht_crc32_error;
	stat_info->cnt_ht_crc32_error = crc->cnt_ht_crc32_error;
	stat_info->cnt_ofdm_crc32_error = crc->cnt_ofdm_crc32_error;
	stat_info->cnt_cck_crc32_error = crc->cnt_cck_crc32_error;
	stat_info->cnt_crc32_ok_all = crc->cnt_crc32_ok_all;
	stat_info->cnt_he_crc32_ok = crc->cnt_he_crc32_ok;
	stat_info->cnt_vht_crc32_ok = crc->cnt_vht_crc32_ok;
	stat_info->cnt_ht_crc32_ok = crc->cnt_ht_crc32_ok;
	stat_info->cnt_ofdm_crc32_ok = crc->cnt_ofdm_crc32_ok;
	stat_info->cnt_cck_crc32_ok = crc->cnt_cck_crc32_ok;
	stat_info->igi_fa_rssi = bb->bb_dig_i.p_cur_dig_unit->igi_fa_rssi;

	if (bb->hal_com->dbcc_en) {
		BB_DBG(bb, DBG_FA_CNT, "[DBCC!!!!]===>\n");
		BB_DBG(bb, DBG_FA_CNT, "[The following statistics is at %s]===>\n", phy_idx == HW_PHY_0 ? "PHY-0" : "PHY-1");
#if 1
		if (cck_en) {
			if (phy_idx_2 != HW_PHY_MAX)
				BB_DBG(bb, DBG_FA_CNT, "[The following CCK statistics is at %s]===>\n", 
				     phy_idx_2 == HW_PHY_0 ? "PHY-0" : "PHY-1");
		}
#endif
	}
	
	BB_DBG(bb, DBG_FA_CNT, "[Tx counter]===>\n");
	BB_DBG(bb, DBG_FA_CNT,
	       "[Tx Cnt]{CCK_TXEN, CCK_TXON, OFDM_TXEN, OFDM_TXON}: {%d, %d, %d, %d}\n",
	       tx->cck_mac_txen, tx->cck_phy_txon, tx->ofdm_mac_txen,
	       tx->ofdm_phy_txon);

	BB_DBG(bb, DBG_FA_CNT, "[Rx counter]===>\n");
	BB_DBG(bb, DBG_FA_CNT,
		  "[CCA Cnt] {CCK, OFDM, Total} = {%d, %d, %d}\n",
		  cca->cnt_cck_cca, cca->cnt_ofdm_cca, cca->cnt_cca_all);
	BB_DBG(bb, DBG_FA_CNT,
		  "[CCA Spoofing Cnt] {CCK, OFDM} = {%d, %d}\n",
		  cca->cnt_cck_spoofing, cca->cnt_ofdm_spoofing);
	BB_DBG(bb, DBG_FA_CNT,
		  "[MPDU] {miss, CRC ok, CRC err} = {%d, %d, %d}\n",
		  crc->cnt_ampdu_miss, crc->cnt_ampdu_crc_ok,
		  crc->cnt_ampdu_crc_error);
	BB_DBG(bb, DBG_FA_CNT,
		  "[Total HW Break counter] = {%d}\n", fa->cnt_total_brk);
	BB_DBG(bb, DBG_FA_CNT,
		  "[FA Cnt] {CCK, OFDM, Total} = {%d, %d, %d}\n",
		  fa->cnt_cck_fail, fa->cnt_ofdm_fail, fa->cnt_fail_all);
	BB_DBG(bb, DBG_FA_CNT,
		  "[CCK FA] SFD_err=%d, SIG_err=%d CRC16=%d\n",
		  cck_fa->sfd_gg_cnt, cck_fa->sig_gg_cnt, cck_fa->cnt_cck_crc_16);
	BB_DBG(bb, DBG_FA_CNT,
		  "[OFDM FA] Parity_err=%d, Rate=%d, LSIG_brk_s=%d, LSIG_brk_l=%d, SBD=%d\n",
		  legacy_fa->cnt_parity_fail, legacy_fa->cnt_rate_illegal,
		  legacy_fa->cnt_lsig_brk_s_th, legacy_fa->cnt_lsig_brk_l_th,
		  legacy_fa->cnt_sb_search_fail);
	BB_DBG(bb, DBG_FA_CNT, "[HT FA] CRC8=%d, MCS=%d\n",
		  ht_fa->cnt_crc8_fail, ht_fa->cnt_mcs_fail);
	BB_DBG(bb, DBG_FA_CNT,
		  "[VHT FA] SIGA_CRC8=%d, MCS=%d\n",
		  vht_fa->cnt_crc8_fail_vhta, vht_fa->cnt_mcs_fail_vht);
#if 0
	BB_DBG(bb, DBG_FA_CNT,
		  "[HE FA] SIGA_CRC4_SU=%d, SIGA_CRC4_ERSU=%d, SIGA_CRC4_MU=%d, SIGB_CRC4_ch1=%d, SIGB_CRC4_ch2=%d, MCS=%d, MCS_bcc=%d, MCS_DCM=%d\n",
		  he_fa->cnt_crc4_fail_hea_su, he_fa->cnt_crc4_fail_hea_ersu,
		  he_fa->cnt_crc4_fail_hea_mu, he_fa->cnt_crc4_fail_heb_ch1_mu,
		  he_fa->cnt_crc4_fail_heb_ch2_mu, he_fa->cnt_mcs_fail_he,
		  he_fa->cnt_mcs_fail_he_bcc, he_fa->cnt_mcs_fail_he_dcm);
#endif

	BB_DBG(bb, DBG_FA_CNT,
		  "[CRC32 OK Cnt] {CCK, OFDM, HT, VHT, HE, Total} = {%d, %d, %d, %d, %d, %d}\n",
		  crc->cnt_cck_crc32_ok, crc->cnt_ofdm_crc32_ok,
		  crc->cnt_ht_crc32_ok, crc->cnt_vht_crc32_ok,
		  crc->cnt_he_crc32_ok, crc->cnt_crc32_ok_all);
	BB_DBG(bb, DBG_FA_CNT,
		  "[CRC32 Err Cnt] {CCK, OFDM, HT, VHT, HE, Total} = {%d, %d, %d, %d, %d, %d}\n",
		  crc->cnt_cck_crc32_error, crc->cnt_ofdm_crc32_error,
		  crc->cnt_ht_crc32_error, crc->cnt_vht_crc32_error,
		  crc->cnt_he_crc32_error, crc->cnt_crc32_error_all);
	BB_DBG(bb, DBG_FA_CNT, "[Halbb DM status]===>\n");
	BB_DBG(bb, DBG_FA_CNT, "[DIG] IGI=%d\n", stat_info->igi_fa_rssi);
}

void halbb_cnt_reg_reset(struct bb_info *bb)
{
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;
	/* @reset CCK FA counter */
	halbb_set_reg(bb, cr->r1b_rx_rpt_rst, cr->r1b_rx_rpt_rst_m, 0);
	halbb_set_reg(bb, cr->r1b_rx_rpt_rst, cr->r1b_rx_rpt_rst_m, 1);

	/* @make sure cnt is enable */
	halbb_set_reg_phy0_1(bb, cr->enable_all_cnt, cr->enable_all_cnt_m, 1);

	/* @reset all bb hw cnt */
	halbb_mp_reset_cnt(bb);
}

void halbb_cck_cnt_statistics(struct bb_info *bb)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_fa_info *fa = &stat_t->bb_fa_i;
	struct bb_cck_fa_info *cck_fa = &fa->bb_cck_fa_i;
	struct bb_cca_info *cca = &stat_t->bb_cca_i;
	struct bb_crc_info *crc = &stat_t->bb_crc_i;
	struct bb_crc2_info *crc2 = &stat_t->bb_crc2_i;
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;

	u32 ret_value = 0;

#if (defined(HALBB_COMPILE_AP_SERIES) || defined(HALBB_COMPILE_CLIENT_SERIES))
	/* select cck dbg port */
	halbb_set_reg(bb, cr->r1b_rr_sel, cr->r1b_rr_sel_m, 2);

	/* read CCK CCA counter */
	ret_value = halbb_get_reg(bb, cr->cck_cca, cr->cck_cca_m);
	cca->cnt_cck_cca = ret_value;

	/* select cck dbg port */
	halbb_set_reg(bb, cr->r1b_rr_sel, cr->r1b_rr_sel_m, 1);

	/* read CCK CRC32 counter */
	ret_value = halbb_get_reg(bb, cr->cck_crc32ok, MASKDWORD);
	crc->cnt_cck_crc32_ok = ret_value & cr->cck_crc32ok_m;
	crc->cnt_cck_crc32_error = (ret_value & cr->cck_crc32fail_m) >> 16;

	/* Read CCK FA counter */
	ret_value = halbb_get_reg(bb, 0x23e0, MASKLWORD); // Reg. doc. doesn't have CCK report reg. 0x78(0x23), need change these addr. one by one
	cck_fa->sfd_gg_cnt = ret_value;

	ret_value = halbb_get_reg(bb, 0x23e0, MASKHWORD);
	cck_fa->cnt_cck_crc_16 = ret_value;

	ret_value = halbb_get_reg(bb, 0x23e8, MASKLWORD);
	cck_fa->sig_gg_cnt = ret_value;

	/* Number of spoofing*/
	ret_value = halbb_get_reg(bb, 0x23ec, MASKBYTE0);
	cca->cnt_cck_spoofing = ret_value;

	//fa->cnt_cck_fail = cck_fa->sfd_gg_cnt + cck_fa->sig_gg_cnt;

	/* Adjust FA computation due to repeated caculatation of brk_cnt when pop starting*/
	fa->cnt_cck_fail = cca->cnt_cck_cca - crc->cnt_cck_crc32_ok -
		           crc->cnt_cck_crc32_error - cca->cnt_cck_spoofing;
#elif (defined(HALBB_COMPILE_AP2_SERIES))
	/*Wait for b mode report CR docs. preparation*/
#endif
}

void halbb_ofdm_cnt_statistics(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_fa_info *fa = &stat_t->bb_fa_i;
	struct bb_legacy_fa_info *legacy_fa = &fa->bb_legacy_fa_i;
	struct bb_ht_fa_info *ht_fa = &fa->bb_ht_fa_i;
	struct bb_vht_fa_info *vht_fa = &fa->bb_vht_fa_i;
	struct bb_he_fa_info *he_fa = &fa->bb_he_fa_i;
	struct bb_cca_info *cca = &stat_t->bb_cca_i;
	struct bb_crc_info *crc = &stat_t->bb_crc_i;
	struct bb_crc2_info *crc2 = &stat_t->bb_crc2_i;
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;
	u32 ret_value = 0;

	/* read OFDM CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->l_crc_ok, MASKDWORD, phy_idx);
	crc->cnt_ofdm_crc32_ok = ret_value & cr->l_crc_ok_m;
	crc->cnt_ofdm_crc32_error = (ret_value & cr->l_crc_err_m) >> 16;

	/* read OFDM2 CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->l_crc_ok2, MASKDWORD, phy_idx);
	crc2->cnt_ofdm2_crc32_ok = ret_value & cr->l_crc_ok2_m;
	crc2->cnt_ofdm2_crc32_error = (ret_value & cr->l_crc_err2_m) >> 16;

	/* read OFDM3 CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->l_crc_ok3, MASKDWORD, phy_idx);
	crc2->cnt_ofdm3_crc32_ok = ret_value & cr->l_crc_ok3_m;
	crc2->cnt_ofdm3_crc32_error = (ret_value & cr->l_crc_err3_m) >> 16;

	/* read HT CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->ht_crc_ok, MASKDWORD, phy_idx);
	crc->cnt_ht_crc32_ok = ret_value & cr->ht_crc_ok_m;
	crc->cnt_ht_crc32_error = (ret_value & cr->ht_crc_err_m) >> 16;

	/* read HT2 CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->ht_crc_ok2, MASKDWORD, phy_idx);
	crc2->cnt_ht2_crc32_ok = ret_value & cr->ht_crc_ok2_m;
	crc2->cnt_ht2_crc32_error = (ret_value & cr->ht_crc_err2_m) >> 16;

	/*read VHT CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->vht_crc_ok, MASKDWORD, phy_idx);
	crc->cnt_vht_crc32_ok = ret_value & cr->vht_crc_ok_m;
	crc->cnt_vht_crc32_error = (ret_value & cr->vht_crc_err_m) >> 16;

	/*read VHT2 CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->vht_crc_ok2, MASKDWORD, phy_idx);
	crc2->cnt_vht2_crc32_ok = ret_value & cr->vht_crc_ok2_m;
	crc2->cnt_vht2_crc32_error = (ret_value & cr->vht_crc_err2_m) >> 16;

	/*read HE CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->he_crc_ok, MASKDWORD, phy_idx);
	crc->cnt_he_crc32_ok = ret_value & cr->he_crc_ok_m;
	crc->cnt_he_crc32_error = (ret_value & cr->he_crc_err_m) >> 16;

	/*read HE2 CRC32 counter */
	ret_value = halbb_get_reg_cmn(bb, cr->he_crc_ok2, MASKDWORD, phy_idx);
	crc2->cnt_he2_crc32_ok = ret_value & cr->he_crc_ok2_m;
	crc2->cnt_he2_crc32_error = (ret_value & cr->he_crc_err2_m) >> 16;

	ret_value = halbb_get_reg_cmn(bb, cr->brk, cr->brk_m, phy_idx);
	fa->cnt_total_brk = ret_value;

	/* Acut workaround because of no HE cnt */
	fa->cnt_ofdm_fail= ret_value;

	/* @calculate OFDM FA counter instead of reading brk_cnt*/
	ret_value = halbb_get_reg_cmn(bb, cr->search_fail, cr->search_fail_m, phy_idx);
	legacy_fa->cnt_sb_search_fail = ret_value;
	
	/* Legacy portion */
	ret_value = halbb_get_reg_cmn(bb, cr->lsig_brk_s_th, cr->lsig_brk_s_th_m, phy_idx);
	legacy_fa->cnt_lsig_brk_s_th = ret_value;

	ret_value = halbb_get_reg_cmn(bb, cr->lsig_brk_l_th, cr->lsig_brk_l_th_m, phy_idx);
	legacy_fa->cnt_lsig_brk_l_th = ret_value;

	ret_value = halbb_get_reg_cmn(bb, cr->rxl_err_parity, cr->rxl_err_parity_m, phy_idx);
	legacy_fa->cnt_parity_fail = ret_value;
	
	ret_value = halbb_get_reg_cmn(bb, cr->rxl_err_rate, cr->rxl_err_rate_m, phy_idx);
	legacy_fa->cnt_rate_illegal = ret_value;

	/* HT portion */
	ret_value = halbb_get_reg_cmn(bb, cr->ht_not_support_mcs, cr->ht_not_support_mcs_m, phy_idx);
	ht_fa->cnt_mcs_fail = ret_value;

	ret_value = halbb_get_reg_cmn(bb, cr->htsig_crc8_err_s_th, cr->htsig_crc8_err_s_th_m, phy_idx);
	ht_fa->cnt_crc8_fail_s_th = ret_value;

	ret_value = halbb_get_reg_cmn(bb, cr->htsig_crc8_err_l_th, cr->htsig_crc8_err_l_th_m, phy_idx);
	ht_fa->cnt_crc8_fail_l_th = ret_value;

	ht_fa->cnt_crc8_fail = ht_fa->cnt_crc8_fail_s_th + ht_fa->cnt_crc8_fail_l_th;

	/* VHT portion */
	ret_value = halbb_get_reg_cmn(bb, cr->vht_not_support_mcs, cr->vht_not_support_mcs_m, phy_idx);
	vht_fa->cnt_mcs_fail_vht = ret_value;

	ret_value = halbb_get_reg_cmn(bb, cr->vht_err_siga_crc8, cr->vht_err_siga_crc8_m, phy_idx);
	vht_fa->cnt_crc8_fail_vhta = ret_value;

#if 0
	/* HE portion need to ECO for CBV */
	ret_value = halbb_get_reg_cmn(bb, cr->hesu_err_sig_a_crc4, MASKDWORD, phy_idx);
	he_fa->cnt_crc4_fail_hea_su = ret_value & cr->hesu_err_sig_a_crc4_m;
	he_fa->cnt_crc4_fail_hea_ersu = ret_value & cr->heersu_err_sig_a_crc4_m;

	ret_value = halbb_get_reg_cmn(bb, cr->hemu_err_sig_a_crc4, MASKDWORD, phy_idx);
	he_fa->cnt_crc4_fail_hea_mu = ret_value & cr->hemu_err_sig_a_crc4_m;
	he_fa->cnt_crc4_fail_heb_ch1_mu = (ret_value & cr->hemu_err_sigb_ch1_comm_crc4_m) >> 16;

	ret_value = halbb_get_reg_cmn(bb, 0x4a08, cr->hemu_err_sigb_ch1_comm_crc4, phy_idx);
	he_fa->cnt_crc4_fail_heb_ch2_mu = ret_value & cr->hemu_err_sigb_ch1_comm_crc4_m;
	he_fa->cnt_mcs_fail_he_bcc = ret_value & cr->he_u0_err_bcc_mcs_m;

	ret_value = halbb_get_reg_cmn(bb, 0x4a0c, cr->he_u0_err_mcs, phy_idx);
	he_fa->cnt_mcs_fail_he = ret_value & cr->he_u0_err_m;
	he_fa->cnt_mcs_fail_he_dcm = ret_value & cr->he_u0_err_mcs_m;


	fa->cnt_ofdm_fail = legacy_fa->cnt_lsig_brk_s_th +
			    legacy_fa->cnt_sb_search_fail +
			    legacy_fa->cnt_lsig_brk_l_th +
			    legacy_fa->cnt_rate_illegal +
			    legacy_fa->cnt_parity_fail +
			    ht_fa->cnt_mcs_fail + ht_fa->cnt_crc8_fail +
			    vht_fa->cnt_mcs_fail_vht +
			    vht_fa->cnt_crc8_fail_vhta +
			    he_fa->cnt_crc4_fail_hea_su +
			    he_fa->cnt_crc4_fail_hea_ersu +
			    he_fa->cnt_crc4_fail_hea_mu +
			    he_fa->cnt_crc4_fail_heb_ch1_mu +
			    he_fa->cnt_crc4_fail_heb_ch2_mu +
			    he_fa->cnt_mcs_fail_he_bcc +
			    he_fa->cnt_mcs_fail_he +
			    he_fa->cnt_mcs_fail_he_dcm
#endif

	/* read OFDM CCA counter */
	ret_value = halbb_get_reg_cmn(bb, cr->ofdm_cca, cr->ofdm_cca_m, phy_idx);
	cca->cnt_ofdm_cca = ret_value;
	ret_value = halbb_get_reg_cmn(bb, cr->cca_spoofing, cr->cca_spoofing_m, phy_idx);
	cca->cnt_ofdm_spoofing = ret_value;
	ret_value = halbb_get_reg_cmn(bb, cr->ampdu_miss, cr->ampdu_miss_m, phy_idx);
	crc->cnt_ampdu_miss = ret_value;
	ret_value = halbb_get_reg_cmn(bb, cr->ampdu_crc_ok, cr->ampdu_crc_ok_m, phy_idx);
	crc->cnt_ampdu_crc_ok = ret_value;
	ret_value = halbb_get_reg_cmn(bb, cr->ampdu_crc_err, cr->ampdu_crc_err_m, phy_idx);
	crc->cnt_ampdu_crc_error = ret_value;
	/* POP counter */
	ret_value = halbb_get_reg_cmn(bb, cr->cnt_pop_trig, cr->cnt_pop_trig_m, phy_idx);
	cca->pop_cnt = ret_value;

}

void halbb_cck_tx_cnt_statistics(struct bb_info *bb)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_tx_cnt_info *tx = &stat_t->bb_tx_cnt_i;
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;
	u32 ret_value = 0;

	/* read Tx counter */
	ret_value = halbb_get_reg(bb, cr->ccktxon, cr->ccktxon_m);
	tx->cck_phy_txon = ret_value;
	ret_value = halbb_get_reg(bb, cr->ccktxen, cr->ccktxen_m);
	tx->cck_mac_txen = ret_value;
}

void halbb_ofdm_tx_cnt_statistics(struct bb_info *bb, enum phl_phy_idx phy_idx)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_tx_cnt_info *tx = &stat_t->bb_tx_cnt_i;
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;
	u32 ret_value = 0;

	/* read Tx counter */
	ret_value = halbb_get_reg_cmn(bb, cr->ofdmtxon, MASKDWORD, phy_idx);
	tx->ofdm_phy_txon = ret_value & cr->ofdmtxon_m;
	tx->ofdm_mac_txen = (ret_value & cr->ofdmtxen_m) >> 16;
}

void halbb_statistics_reset(struct bb_info *bb)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_fa_info *fa = &stat_t->bb_fa_i;
	struct bb_cca_info *cca = &stat_t->bb_cca_i;
	struct bb_crc_info *crc = &stat_t->bb_crc_i;
	struct bb_crc2_info *crc2 = &stat_t->bb_crc2_i;
	struct bb_tx_cnt_info *tx = &stat_t->bb_tx_cnt_i;
	
	/* @reset sw mem */
	halbb_mem_set(bb, tx, 0, sizeof(struct bb_tx_cnt_info));
	halbb_mem_set(bb, fa, 0, sizeof(struct bb_fa_info));
	halbb_mem_set(bb, cca, 0, sizeof(struct bb_cca_info));
	halbb_mem_set(bb, crc, 0, sizeof(struct bb_crc_info));
	halbb_mem_set(bb, crc2, 0, sizeof(struct bb_crc2_info));
}

void halbb_statistics(struct bb_info *bb)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;
	//char dbg_buf[HALBB_SNPRINT_SIZE] = {0};
	u32 tmp = 0;
	u8 path_a_ch  = 0;
	u8 path_b_ch = 0;
	bool cck_en = 0;
	u8 cck_band_sel = 0;

	/* Always turn on*/
	if (!(bb->support_ability & BB_FA_CNT))
		return;

	BB_DBG(bb, DBG_FA_CNT, "[%s]===>\n", __func__);

	/*Need to provide API by HALRF . Dino 2020.02.21*/
	path_a_ch = (u8)halbb_read_rf_reg(bb, RF_PATH_A, 0x18, 0x3ff);
	path_b_ch = (u8)halbb_read_rf_reg(bb, RF_PATH_B, 0x18, 0x3ff);

	cck_en = (path_a_ch <= 14) || (path_b_ch <= 14);
	cck_band_sel = (u8)halbb_get_reg(bb, cr->dbcc, cr->dbcc_m);

	if (bb->hal_com->dbcc_en) {
		if (!cck_en) {
			halbb_ofdm_tx_cnt_statistics(bb, HW_PHY_0);
			halbb_ofdm_cnt_statistics(bb, HW_PHY_0);

			halbb_print_cnt(bb, cck_en, HW_PHY_0, HW_PHY_MAX);
			halbb_print_cnt2(bb, HW_PHY_0);
			halbb_print_cnt3(bb, HW_PHY_0);

			halbb_ofdm_tx_cnt_statistics(bb, HW_PHY_1);
			halbb_ofdm_cnt_statistics(bb, HW_PHY_1);

			halbb_print_cnt(bb, cck_en, HW_PHY_1, HW_PHY_MAX);
			halbb_print_cnt2(bb, HW_PHY_1);
			halbb_print_cnt3(bb, HW_PHY_1);
		} else {
			halbb_cck_tx_cnt_statistics(bb);
			halbb_ofdm_tx_cnt_statistics(bb, HW_PHY_0);
			halbb_cck_cnt_statistics(bb);
			halbb_ofdm_cnt_statistics(bb, HW_PHY_0);

			halbb_print_cnt(bb, cck_en, HW_PHY_0, HW_PHY_0);
			halbb_print_cnt2(bb, HW_PHY_0);
			halbb_print_cnt3(bb, HW_PHY_0);

			halbb_ofdm_tx_cnt_statistics(bb, HW_PHY_1);
			halbb_ofdm_cnt_statistics(bb, HW_PHY_1);

			halbb_print_cnt(bb, cck_en, HW_PHY_1, HW_PHY_MAX);
			halbb_print_cnt2(bb, HW_PHY_1);
			halbb_print_cnt3(bb, HW_PHY_1);
		}/*else if (cck_en && (cck_band_sel == 1)) {
			halbb_cck_cnt_statistics(bb, HW_PHY_1);
			halbb_ofdm_cnt_statistics(bb, HW_PHY_0);
			
			halbb_print_cnt(bb, cck_en, HW_PHY_0, HW_PHY_1);
			halbb_print_cnt2(bb, HW_PHY_0);
			halbb_print_cnt3(bb, HW_PHY_0);
		}*/
	} else {
		if (!cck_en) {
			halbb_ofdm_tx_cnt_statistics(bb, HW_PHY_0);
			halbb_ofdm_cnt_statistics(bb, HW_PHY_0);

			halbb_print_cnt(bb, cck_en, HW_PHY_0, HW_PHY_MAX);
			halbb_print_cnt2(bb, HW_PHY_0);
			halbb_print_cnt3(bb, HW_PHY_0);
		} else {
			halbb_cck_tx_cnt_statistics(bb);
			halbb_ofdm_tx_cnt_statistics(bb, HW_PHY_0);
			halbb_cck_cnt_statistics(bb);
			halbb_ofdm_cnt_statistics(bb, HW_PHY_0);

			halbb_print_cnt(bb, cck_en, HW_PHY_0, HW_PHY_0);
			halbb_print_cnt2(bb, HW_PHY_0);
			halbb_print_cnt3(bb, HW_PHY_0);
		}
	}

/*==52A CBV CCV/52B/52C Rx hang workaround==*/
	halbb_chk_hang(bb);
/*================================*/

	halbb_cnt_reg_reset(bb);
}

void halbb_statistics_init(struct bb_info *bb)
{
	struct bb_stat_info *stat_t = &bb->bb_stat_i;
	stat_t->chk_hang_cnt = 0;
	stat_t->hang_recovery_en = HANG_RECOVERY;
	stat_t->chk_hang_limit = HANG_LIMIT;
	halbb_statistics_reset(bb);
	halbb_set_crc32_cnt2_rate(bb, BB_06M);
	halbb_set_crc32_cnt3_format(bb, STATE_BEACON);
}

void halbb_cr_cfg_stat_init(struct bb_info *bb)
{
	struct bb_stat_cr_info *cr = &bb->bb_stat_i.bb_stat_cr_i;

	switch (bb->cr_type) {

	#ifdef BB_8852A_CAV_SUPPORT
	case BB_52AA:
		cr->cck_cca = CNT_CCK_CCA_P0_52AA;
		cr->cck_cca_m = CNT_CCK_CCA_P0_52AA_M;
		cr->cck_crc16fail = CNT_CCK_CRC16FAIL_P0_52AA;
		cr->cck_crc16fail_m = CNT_CCK_CRC16FAIL_P0_52AA_M;
		cr->cck_crc32ok = CNT_CCK_CRC32OK_P0_52AA;
		cr->cck_crc32ok_m = CNT_CCK_CRC32OK_P0_52AA_M;
		cr->cck_crc32fail = CNT_CCK_CRC32FAIL_P0_52AA;
		cr->cck_crc32fail_m = CNT_CCK_CRC32FAIL_P0_52AA_M;
		cr->cca_spoofing = CNT_CCA_SPOOFING_52AA;
		cr->cca_spoofing_m = CNT_CCA_SPOOFING_52AA_M;
		cr->lsig_brk_s_th = CNT_LSIG_BRK_S_TH_52AA;
		cr->lsig_brk_s_th_m = CNT_LSIG_BRK_S_TH_52AA_M;
		cr->lsig_brk_l_th = CNT_LSIG_BRK_L_TH_52AA;
		cr->lsig_brk_l_th_m = CNT_LSIG_BRK_L_TH_52AA_M;
		cr->htsig_crc8_err_s_th = CNT_HTSIG_CRC8_ERR_S_TH_52AA;
		cr->htsig_crc8_err_s_th_m = CNT_HTSIG_CRC8_ERR_S_TH_52AA_M;
		cr->htsig_crc8_err_l_th = CNT_HTSIG_CRC8_ERR_L_TH_52AA;
		cr->htsig_crc8_err_l_th_m = CNT_HTSIG_CRC8_ERR_L_TH_52AA_M;
		cr->brk = CNT_BRK_52AA;
		cr->brk_m = CNT_BRK_52AA_M;
		cr->brk_sel = CNT_BRK_SEL_52AA;
		cr->brk_sel_m = CNT_BRK_SEL_52AA_M;
		cr->rxl_err_parity = CNT_RXL_ERR_PARITY_52AA;
		cr->rxl_err_parity_m = CNT_RXL_ERR_PARITY_52AA_M;
		cr->rxl_err_rate = CNT_RXL_ERR_RATE_52AA;
		cr->rxl_err_rate_m = CNT_RXL_ERR_RATE_52AA_M;
		cr->ht_err_crc8 = CNT_HT_ERR_CRC8_52AA;
		cr->ht_err_crc8_m = CNT_HT_ERR_CRC8_52AA_M;
		cr->vht_err_siga_crc8 = CNT_VHT_ERR_SIGA_CRC8_52AA;
		cr->vht_err_siga_crc8_m = CNT_VHT_ERR_SIGA_CRC8_52AA_M;
		cr->ht_not_support_mcs = CNT_HT_NOT_SUPPORT_MCS_52AA;
		cr->ht_not_support_mcs_m = CNT_HT_NOT_SUPPORT_MCS_52AA_M;
		cr->vht_not_support_mcs = CNT_VHT_NOT_SUPPORT_MCS_52AA;
		cr->vht_not_support_mcs_m = CNT_VHT_NOT_SUPPORT_MCS_52AA_M;
		cr->err_during_bt_tx = CNT_ERR_DURING_BT_TX_52AA;
		cr->err_during_bt_tx_m = CNT_ERR_DURING_BT_TX_52AA_M;
		cr->err_during_bt_rx = CNT_ERR_DURING_BT_RX_52AA;
		cr->err_during_bt_rx_m = CNT_ERR_DURING_BT_RX_52AA_M;
		cr->edge_murx_nsts0 = CNT_EDGE_MURX_NSTS0_52AA;
		cr->edge_murx_nsts0_m = CNT_EDGE_MURX_NSTS0_52AA_M;
		cr->search_fail = CNT_SEARCH_FAIL_52AA;
		cr->search_fail_m = CNT_SEARCH_FAIL_52AA_M;
		cr->ofdm_cca = CNT_OFDM_CCA_52AA;
		cr->ofdm_cca_m = CNT_OFDM_CCA_52AA_M;
		cr->ofdm_cca_s20 = CNT_OFDM_CCA_S20_52AA;
		cr->ofdm_cca_s20_m = CNT_OFDM_CCA_S20_52AA_M;
		cr->ofdm_cca_s40 = CNT_OFDM_CCA_S40_52AA;
		cr->ofdm_cca_s40_m = CNT_OFDM_CCA_S40_52AA_M;
		cr->ofdm_cca_s80 = CNT_OFDM_CCA_S80_52AA;
		cr->ofdm_cca_s80_m = CNT_OFDM_CCA_S80_52AA_M;
		cr->ofdmtxon = CNT_OFDMTXON_52AA;
		cr->ofdmtxon_m = CNT_OFDMTXON_52AA_M;
		cr->ofdmtxen = CNT_OFDMTXEN_52AA;
		cr->ofdmtxen_m = CNT_OFDMTXEN_52AA_M;
		cr->drop_trig = CNT_DROP_TRIG_52AA;
		cr->drop_trig_m = CNT_DROP_TRIG_52AA_M;
		cr->pop_trig = CNT_POP_TRIG_52AA;
		cr->pop_trig_m = CNT_POP_TRIG_52AA_M;
		cr->tx_conflict = CNT_TX_CONFLICT_52AA;
		cr->tx_conflict_m = CNT_TX_CONFLICT_52AA_M;
		cr->wmac_rstb = CNT_WMAC_RSTB_52AA;
		cr->wmac_rstb_m = CNT_WMAC_RSTB_52AA_M;
		cr->en_tb_ppdu_fix_gain = CNT_EN_TB_PPDU_FIX_GAIN_52AA;
		cr->en_tb_ppdu_fix_gain_m = CNT_EN_TB_PPDU_FIX_GAIN_52AA_M;
		cr->en_tb_cca_pw_th = CNT_EN_TB_CCA_PW_TH_52AA;
		cr->en_tb_cca_pw_th_m = CNT_EN_TB_CCA_PW_TH_52AA_M;
		cr->he_crc_ok = CNT_HE_CRC_OK_52AA;
		cr->he_crc_ok_m = CNT_HE_CRC_OK_52AA_M;
		cr->he_crc_err = CNT_HE_CRC_ERR_52AA;
		cr->he_crc_err_m = CNT_HE_CRC_ERR_52AA_M;
		cr->vht_crc_ok = CNT_VHT_CRC_OK_52AA;
		cr->vht_crc_ok_m = CNT_VHT_CRC_OK_52AA_M;
		cr->vht_crc_err = CNT_VHT_CRC_ERR_52AA;
		cr->vht_crc_err_m = CNT_VHT_CRC_ERR_52AA_M;
		cr->ht_crc_ok = CNT_HT_CRC_OK_52AA;
		cr->ht_crc_ok_m = CNT_HT_CRC_OK_52AA_M;
		cr->ht_crc_err = CNT_HT_CRC_ERR_52AA;
		cr->ht_crc_err_m = CNT_HT_CRC_ERR_52AA_M;
		cr->l_crc_ok = CNT_L_CRC_OK_52AA;
		cr->l_crc_ok_m = CNT_L_CRC_OK_52AA_M;
		cr->l_crc_err = CNT_L_CRC_ERR_52AA;
		cr->l_crc_err_m = CNT_L_CRC_ERR_52AA_M;
		cr->he_crc_ok2 = CNT_HE_CRC_OK2_52AA;
		cr->he_crc_ok2_m = CNT_HE_CRC_OK2_52AA_M;
		cr->he_crc_err2 = CNT_HE_CRC_ERR2_52AA;
		cr->he_crc_err2_m = CNT_HE_CRC_ERR2_52AA_M;
		cr->vht_crc_ok2 = CNT_VHT_CRC_OK2_52AA;
		cr->vht_crc_ok2_m = CNT_VHT_CRC_OK2_52AA_M;
		cr->vht_crc_err2 = CNT_VHT_CRC_ERR2_52AA;
		cr->vht_crc_err2_m = CNT_VHT_CRC_ERR2_52AA_M;
		cr->ht_crc_ok2 = CNT_HT_CRC_OK2_52AA;
		cr->ht_crc_ok2_m = CNT_HT_CRC_OK2_52AA_M;
		cr->ht_crc_err2 = CNT_HT_CRC_ERR2_52AA;
		cr->ht_crc_err2_m = CNT_HT_CRC_ERR2_52AA_M;
		cr->l_crc_ok2 = CNT_L_CRC_OK2_52AA;
		cr->l_crc_ok2_m = CNT_L_CRC_OK2_52AA_M;
		cr->l_crc_err2 = CNT_L_CRC_ERR2_52AA;
		cr->l_crc_err2_m = CNT_L_CRC_ERR2_52AA_M;
		cr->l_crc_ok3 = CNT_L_CRC_OK3_52AA;
		cr->l_crc_ok3_m = CNT_L_CRC_OK3_52AA_M;
		cr->l_crc_err3 = CNT_L_CRC_ERR3_52AA;
		cr->l_crc_err3_m = CNT_L_CRC_ERR3_52AA_M;
		cr->ampdu_rxon = CNT_AMPDU_RXON_52AA;
		cr->ampdu_rxon_m = CNT_AMPDU_RXON_52AA_M;
		cr->ampdu_miss = CNT_AMPDU_MISS_52AA;
		cr->ampdu_miss_m = CNT_AMPDU_MISS_52AA_M;
		cr->hesu_err_sig_a_crc4 = CNT_HESU_ERR_SIG_A_CRC4_52AA;
		cr->hesu_err_sig_a_crc4_m = CNT_HESU_ERR_SIG_A_CRC4_52AA_M;
		cr->heersu_err_sig_a_crc4 = CNT_HEERSU_ERR_SIG_A_CRC4_52AA;
		cr->heersu_err_sig_a_crc4_m = CNT_HEERSU_ERR_SIG_A_CRC4_52AA_M;
		cr->hemu_err_sig_a_crc4 = CNT_HEMU_ERR_SIG_A_CRC4_52AA;
		cr->hemu_err_sig_a_crc4_m = CNT_HEMU_ERR_SIG_A_CRC4_52AA_M;
		cr->hemu_err_sigb_ch1_comm_crc4 = CNT_HEMU_ERR_SIGB_CH1_COMM_CRC4_52AA;
		cr->hemu_err_sigb_ch1_comm_crc4_m = CNT_HEMU_ERR_SIGB_CH1_COMM_CRC4_52AA_M;
		cr->hemu_err_sigb_ch2_comm_crc4 = CNT_HEMU_ERR_SIGB_CH2_COMM_CRC4_52AA;
		cr->hemu_err_sigb_ch2_comm_crc4_m = CNT_HEMU_ERR_SIGB_CH2_COMM_CRC4_52AA_M;
		cr->he_u0_err_bcc_mcs = CNT_HE_U0_ERR_BCC_MCS_52AA;
		cr->he_u0_err_bcc_mcs_m = CNT_HE_U0_ERR_BCC_MCS_52AA_M;
		cr->he_u0_err_mcs = CNT_HE_U0_ERR_MCS_52AA;
		cr->he_u0_err_mcs_m = CNT_HE_U0_ERR_MCS_52AA_M;
		cr->he_u0_err_dcm_mcs = CNT_HE_U0_ERR_DCM_MCS_52AA;
		cr->he_u0_err_dcm_mcs_m = CNT_HE_U0_ERR_DCM_MCS_52AA_M;
		cr->r1b_rx_rpt_rst = R1B_RX_RPT_RST_52AA;
		cr->r1b_rx_rpt_rst_m = R1B_RX_RPT_RST_52AA_M;
		cr->r1b_rr_sel = R1B_RR_SEL_52AA;
		cr->r1b_rr_sel_m = R1B_RR_SEL_52AA_M;
		cr->rst_all_cnt = RST_ALL_CNT_52AA;
		cr->rst_all_cnt_m = RST_ALL_CNT_52AA_M;
		cr->enable_all_cnt = ENABLE_ALL_CNT_52AA;
		cr->enable_all_cnt_m = ENABLE_ALL_CNT_52AA_M;
		cr->enable_ofdm = ENABLE_OFDM_52AA;
		cr->enable_ofdm_m = ENABLE_OFDM_52AA_M;
		cr->enable_cck = ENABLE_CCK_52AA;
		cr->enable_cck_m = ENABLE_CCK_52AA_M;
		cr->r1b_rx_dis_cca = R1B_RX_DIS_CCA_52AA;
		cr->r1b_rx_dis_cca_m = R1B_RX_DIS_CCA_52AA_M;
		cr->intf_r_rate = INTF_R_CNT_RATE_52AA;
		cr->intf_r_rate_m = INTF_R_CNT_RATE_52AA_M;
		cr->intf_r_mcs = INTF_R_CNT_MCS_52AA;
		cr->intf_r_mcs_m = INTF_R_CNT_MCS_52AA_M;
		cr->intf_r_vht_mcs = INTF_R_CNT_VHT_MCS_52AA;
		cr->intf_r_vht_mcs_m = INTF_R_CNT_VHT_MCS_52AA_M;
		cr->intf_r_he_mcs = INTF_R_CNT_HE_MCS_52AA;
		cr->intf_r_he_mcs_m = INTF_R_CNT_HE_MCS_52AA_M;
		cr->intf_r_vht_nss = INTF_R_CNT_VHT_NSS_52AA;
		cr->intf_r_vht_nss_m = INTF_R_CNT_VHT_NSS_52AA_M;
		cr->intf_r_he_nss = INTF_R_CNT_HE_NSS_52AA;
		cr->intf_r_he_nss_m = INTF_R_CNT_HE_NSS_52AA_M;
		cr->intf_r_mac_hdr_type = INTF_R_MAC_HDR_TYPE_52AA;
		cr->intf_r_mac_hdr_type_m = INTF_R_MAC_HDR_TYPE_52AA_M;
		cr->intf_r_pkt_type = INTF_R_PKT_TYPE_52AA;
		cr->intf_r_pkt_type_m = INTF_R_PKT_TYPE_52AA_M;
		cr->dbcc = DBCC_52AA;
		cr->dbcc_m = DBCC_52AA_M;
		cr->dbcc_2p4g_band_sel = DBCC_2P4G_BAND_SEL_52AA;
		cr->dbcc_2p4g_band_sel_m = DBCC_2P4G_BAND_SEL_52AA_M;
		cr->max_cnt_pop = MAX_CNT_POP_52AA;
		cr->max_cnt_pop_m = MAX_CNT_POP_52AA_M;
		break;

	#endif
	#ifdef HALBB_COMPILE_AP_SERIES
	case BB_AP:
		cr->cck_cca = CNT_CCK_CCA_P0_A;
		cr->cck_cca_m = CNT_CCK_CCA_P0_A_M;
		cr->cck_crc16fail = CNT_CCK_CRC16FAIL_P0_A;
		cr->cck_crc16fail_m = CNT_CCK_CRC16FAIL_P0_A_M;
		cr->cck_crc32ok = CNT_CCK_CRC32OK_P0_A;
		cr->cck_crc32ok_m = CNT_CCK_CRC32OK_P0_A_M;
		cr->cck_crc32fail = CNT_CCK_CRC32FAIL_P0_A;
		cr->cck_crc32fail_m = CNT_CCK_CRC32FAIL_P0_A_M;
		cr->cca_spoofing = CNT_CCA_SPOOFING_A;
		cr->cca_spoofing_m = CNT_CCA_SPOOFING_A_M;
		cr->lsig_brk_s_th = CNT_LSIG_BRK_S_TH_A;
		cr->lsig_brk_s_th_m = CNT_LSIG_BRK_S_TH_A_M;
		cr->lsig_brk_l_th = CNT_LSIG_BRK_L_TH_A;
		cr->lsig_brk_l_th_m = CNT_LSIG_BRK_L_TH_A_M;
		cr->htsig_crc8_err_s_th = CNT_HTSIG_CRC8_ERR_S_TH_A;
		cr->htsig_crc8_err_s_th_m = CNT_HTSIG_CRC8_ERR_S_TH_A_M;
		cr->htsig_crc8_err_l_th = CNT_HTSIG_CRC8_ERR_L_TH_A;
		cr->htsig_crc8_err_l_th_m = CNT_HTSIG_CRC8_ERR_L_TH_A_M;
		cr->brk = CNT_BRK_A;
		cr->brk_m = CNT_BRK_A_M;
		cr->brk_sel = CNT_BRK_SEL_A;
		cr->brk_sel_m = CNT_BRK_SEL_A_M;
		cr->rxl_err_parity = CNT_RXL_ERR_PARITY_A;
		cr->rxl_err_parity_m = CNT_RXL_ERR_PARITY_A_M;
		cr->rxl_err_rate = CNT_RXL_ERR_RATE_A;
		cr->rxl_err_rate_m = CNT_RXL_ERR_RATE_A_M;
		cr->ht_err_crc8 = CNT_HT_ERR_CRC8_A;
		cr->ht_err_crc8_m = CNT_HT_ERR_CRC8_A_M;
		cr->vht_err_siga_crc8 = CNT_VHT_ERR_SIGA_CRC8_A;
		cr->vht_err_siga_crc8_m = CNT_VHT_ERR_SIGA_CRC8_A_M;
		cr->ht_not_support_mcs = CNT_HT_NOT_SUPPORT_MCS_A;
		cr->ht_not_support_mcs_m = CNT_HT_NOT_SUPPORT_MCS_A_M;
		cr->vht_not_support_mcs = CNT_VHT_NOT_SUPPORT_MCS_A;
		cr->vht_not_support_mcs_m = CNT_VHT_NOT_SUPPORT_MCS_A_M;
		cr->err_during_bt_tx = CNT_ERR_DURING_BT_TX_A;
		cr->err_during_bt_tx_m = CNT_ERR_DURING_BT_TX_A_M;
		cr->err_during_bt_rx = CNT_ERR_DURING_BT_RX_A;
		cr->err_during_bt_rx_m = CNT_ERR_DURING_BT_RX_A_M;
		cr->edge_murx_nsts0 = CNT_EDGE_MURX_NSTS0_A;
		cr->edge_murx_nsts0_m = CNT_EDGE_MURX_NSTS0_A_M;
		cr->search_fail = CNT_SEARCH_FAIL_A;
		cr->search_fail_m = CNT_SEARCH_FAIL_A_M;
		cr->ofdm_cca = CNT_OFDM_CCA_A;
		cr->ofdm_cca_m = CNT_OFDM_CCA_A_M;
		cr->ofdm_cca_s20 = CNT_OFDM_CCA_S20_A;
		cr->ofdm_cca_s20_m = CNT_OFDM_CCA_S20_A_M;
		cr->ofdm_cca_s40 = CNT_OFDM_CCA_S40_A;
		cr->ofdm_cca_s40_m = CNT_OFDM_CCA_S40_A_M;
		cr->ofdm_cca_s80 = CNT_OFDM_CCA_S80_A;
		cr->ofdm_cca_s80_m = CNT_OFDM_CCA_S80_A_M;
		cr->ccktxon = CNT_CCKTXON_A;
		cr->ccktxon_m = CNT_CCKTXON_A_M;
		cr->ccktxen = CNT_CCKTXEN_A;
		cr->ccktxen_m = CNT_CCKTXEN_A_M;
		cr->ofdmtxon = CNT_OFDMTXON_A;
		cr->ofdmtxon_m = CNT_OFDMTXON_A_M;
		cr->ofdmtxen = CNT_OFDMTXEN_A;
		cr->ofdmtxen_m = CNT_OFDMTXEN_A_M;
		cr->drop_trig = CNT_DROP_TRIG_A;
		cr->drop_trig_m = CNT_DROP_TRIG_A_M;
		cr->pop_trig = CNT_POP_TRIG_A;
		cr->pop_trig_m = CNT_POP_TRIG_A_M;
		cr->tx_conflict = CNT_TX_CONFLICT_A;
		cr->tx_conflict_m = CNT_TX_CONFLICT_A_M;
		cr->wmac_rstb = CNT_WMAC_RSTB_A;
		cr->wmac_rstb_m = CNT_WMAC_RSTB_A_M;
		cr->en_tb_ppdu_fix_gain = CNT_EN_TB_PPDU_FIX_GAIN_A;
		cr->en_tb_ppdu_fix_gain_m = CNT_EN_TB_PPDU_FIX_GAIN_A_M;
		cr->en_tb_cca_pw_th = CNT_EN_TB_CCA_PW_TH_A;
		cr->en_tb_cca_pw_th_m = CNT_EN_TB_CCA_PW_TH_A_M;
		cr->he_crc_ok = CNT_HE_CRC_OK_A;
		cr->he_crc_ok_m = CNT_HE_CRC_OK_A_M;
		cr->he_crc_err = CNT_HE_CRC_ERR_A;
		cr->he_crc_err_m = CNT_HE_CRC_ERR_A_M;
		cr->vht_crc_ok = CNT_VHT_CRC_OK_A;
		cr->vht_crc_ok_m = CNT_VHT_CRC_OK_A_M;
		cr->vht_crc_err = CNT_VHT_CRC_ERR_A;
		cr->vht_crc_err_m = CNT_VHT_CRC_ERR_A_M;
		cr->ht_crc_ok = CNT_HT_CRC_OK_A;
		cr->ht_crc_ok_m = CNT_HT_CRC_OK_A_M;
		cr->ht_crc_err = CNT_HT_CRC_ERR_A;
		cr->ht_crc_err_m = CNT_HT_CRC_ERR_A_M;
		cr->l_crc_ok = CNT_L_CRC_OK_A;
		cr->l_crc_ok_m = CNT_L_CRC_OK_A_M;
		cr->l_crc_err = CNT_L_CRC_ERR_A;
		cr->l_crc_err_m = CNT_L_CRC_ERR_A_M;
		cr->he_crc_ok2 = CNT_HE_CRC_OK2_A;
		cr->he_crc_ok2_m = CNT_HE_CRC_OK2_A_M;
		cr->he_crc_err2 = CNT_HE_CRC_ERR2_A;
		cr->he_crc_err2_m = CNT_HE_CRC_ERR2_A_M;
		cr->vht_crc_ok2 = CNT_VHT_CRC_OK2_A;
		cr->vht_crc_ok2_m = CNT_VHT_CRC_OK2_A_M;
		cr->vht_crc_err2 = CNT_VHT_CRC_ERR2_A;
		cr->vht_crc_err2_m = CNT_VHT_CRC_ERR2_A_M;
		cr->ht_crc_ok2 = CNT_HT_CRC_OK2_A;
		cr->ht_crc_ok2_m = CNT_HT_CRC_OK2_A_M;
		cr->ht_crc_err2 = CNT_HT_CRC_ERR2_A;
		cr->ht_crc_err2_m = CNT_HT_CRC_ERR2_A_M;
		cr->l_crc_ok2 = CNT_L_CRC_OK2_A;
		cr->l_crc_ok2_m = CNT_L_CRC_OK2_A_M;
		cr->l_crc_err2 = CNT_L_CRC_ERR2_A;
		cr->l_crc_err2_m = CNT_L_CRC_ERR2_A_M;
		cr->l_crc_ok3 = CNT_L_CRC_OK3_A;
		cr->l_crc_ok3_m = CNT_L_CRC_OK3_A_M;
		cr->l_crc_err3 = CNT_L_CRC_ERR3_A;
		cr->l_crc_err3_m = CNT_L_CRC_ERR3_A_M;
		cr->ampdu_rxon = CNT_AMPDU_RXON_A;
		cr->ampdu_rxon_m = CNT_AMPDU_RXON_A_M;
		cr->ampdu_miss = CNT_AMPDU_MISS_A;
		cr->ampdu_miss_m = CNT_AMPDU_MISS_A_M;
		cr->ampdu_crc_ok = CNT_AMPDU_RX_CRC32_OK_A;
		cr->ampdu_crc_ok_m = CNT_AMPDU_RX_CRC32_OK_A_M;
		cr->ampdu_crc_err = CNT_AMPDU_RX_CRC32_ERR_A;
		cr->ampdu_crc_err_m = CNT_AMPDU_RX_CRC32_ERR_A_M;
		cr->hesu_err_sig_a_crc4 = CNT_HESU_ERR_SIG_A_CRC4_A;
		cr->hesu_err_sig_a_crc4_m = CNT_HESU_ERR_SIG_A_CRC4_A_M;
		cr->heersu_err_sig_a_crc4 = CNT_HEERSU_ERR_SIG_A_CRC4_A;
		cr->heersu_err_sig_a_crc4_m = CNT_HEERSU_ERR_SIG_A_CRC4_A_M;
		cr->hemu_err_sig_a_crc4 = CNT_HEMU_ERR_SIG_A_CRC4_A;
		cr->hemu_err_sig_a_crc4_m = CNT_HEMU_ERR_SIG_A_CRC4_A_M;
		cr->hemu_err_sigb_ch1_comm_crc4 = CNT_HEMU_ERR_SIGB_CH1_COMM_CRC4_A;
		cr->hemu_err_sigb_ch1_comm_crc4_m = CNT_HEMU_ERR_SIGB_CH1_COMM_CRC4_A_M;
		cr->hemu_err_sigb_ch2_comm_crc4 = CNT_HEMU_ERR_SIGB_CH2_COMM_CRC4_A;
		cr->hemu_err_sigb_ch2_comm_crc4_m = CNT_HEMU_ERR_SIGB_CH2_COMM_CRC4_A_M;
		cr->he_u0_err_bcc_mcs = CNT_HE_U0_ERR_BCC_MCS_A;
		cr->he_u0_err_bcc_mcs_m = CNT_HE_U0_ERR_BCC_MCS_A_M;
		cr->he_u0_err_mcs = CNT_HE_U0_ERR_MCS_A;
		cr->he_u0_err_mcs_m = CNT_HE_U0_ERR_MCS_A_M;
		cr->he_u0_err_dcm_mcs = CNT_HE_U0_ERR_DCM_MCS_A;
		cr->he_u0_err_dcm_mcs_m = CNT_HE_U0_ERR_DCM_MCS_A_M;
		cr->r1b_rx_rpt_rst = R1B_RX_RPT_RST_A;
		cr->r1b_rx_rpt_rst_m = R1B_RX_RPT_RST_A_M;
		cr->r1b_rr_sel = R1B_RR_SEL_A;
		cr->r1b_rr_sel_m = R1B_RR_SEL_A_M;
		cr->rst_all_cnt = RST_ALL_CNT_A;
		cr->rst_all_cnt_m = RST_ALL_CNT_A_M;
		cr->enable_all_cnt = ENABLE_ALL_CNT_A;
		cr->enable_all_cnt_m = ENABLE_ALL_CNT_A_M;
		cr->enable_ofdm = ENABLE_OFDM_A;
		cr->enable_ofdm_m = ENABLE_OFDM_A_M;
		cr->enable_cck = ENABLE_CCK_A;
		cr->enable_cck_m = ENABLE_CCK_A_M;
		cr->r1b_rx_dis_cca = R1B_RX_DIS_CCA_A;
		cr->r1b_rx_dis_cca_m = R1B_RX_DIS_CCA_A_M;
		cr->intf_r_rate = INTF_R_CNT_RATE_A;
		cr->intf_r_rate_m = INTF_R_CNT_RATE_A_M;
		cr->intf_r_mcs = INTF_R_CNT_MCS_A;
		cr->intf_r_mcs_m = INTF_R_CNT_MCS_A_M;
		cr->intf_r_vht_mcs = INTF_R_CNT_VHT_MCS_A;
		cr->intf_r_vht_mcs_m = INTF_R_CNT_VHT_MCS_A_M;
		cr->intf_r_he_mcs = INTF_R_CNT_HE_MCS_A;
		cr->intf_r_he_mcs_m = INTF_R_CNT_HE_MCS_A_M;
		cr->intf_r_vht_nss = INTF_R_CNT_VHT_NSS_A;
		cr->intf_r_vht_nss_m = INTF_R_CNT_VHT_NSS_A_M;
		cr->intf_r_he_nss = INTF_R_CNT_HE_NSS_A;
		cr->intf_r_he_nss_m = INTF_R_CNT_HE_NSS_A_M;
		cr->intf_r_mac_hdr_type = INTF_R_MAC_HDR_TYPE_A;
		cr->intf_r_mac_hdr_type_m = INTF_R_MAC_HDR_TYPE_A_M;
		cr->intf_r_pkt_type = INTF_R_PKT_TYPE_A;
		cr->intf_r_pkt_type_m = INTF_R_PKT_TYPE_A_M;
		cr->dbcc = DBCC_A;
		cr->dbcc_m = DBCC_A_M;
		cr->dbcc_2p4g_band_sel = DBCC_2P4G_BAND_SEL_A;
		cr->dbcc_2p4g_band_sel_m = DBCC_2P4G_BAND_SEL_A_M;
		cr->cnt_pop_trig = CNT_POP_TRIG_A;
		cr->cnt_pop_trig_m = CNT_POP_TRIG_A_M;
		cr->max_cnt_pop = MAX_CNT_POP_A;
		cr->max_cnt_pop_m = MAX_CNT_POP_A_M;
		break;

	#endif
	#ifdef HALBB_COMPILE_CLIENT_SERIES
	case BB_CLIENT:
		cr->cck_cca = CNT_CCK_CCA_P0_C;
		cr->cck_cca_m = CNT_CCK_CCA_P0_C_M;
		cr->cck_crc16fail = CNT_CCK_CRC16FAIL_P0_C;
		cr->cck_crc16fail_m = CNT_CCK_CRC16FAIL_P0_C_M;
		cr->cck_crc32ok = CNT_CCK_CRC32OK_P0_C;
		cr->cck_crc32ok_m = CNT_CCK_CRC32OK_P0_C_M;
		cr->cck_crc32fail = CNT_CCK_CRC32FAIL_P0_C;
		cr->cck_crc32fail_m = CNT_CCK_CRC32FAIL_P0_C_M;
		cr->cca_spoofing = CNT_CCA_SPOOFING_C;
		cr->cca_spoofing_m = CNT_CCA_SPOOFING_C_M;
		cr->lsig_brk_s_th = CNT_LSIG_BRK_S_TH_C;
		cr->lsig_brk_s_th_m = CNT_LSIG_BRK_S_TH_C_M;
		cr->lsig_brk_l_th = CNT_LSIG_BRK_L_TH_C;
		cr->lsig_brk_l_th_m = CNT_LSIG_BRK_L_TH_C_M;
		cr->htsig_crc8_err_s_th = CNT_HTSIG_CRC8_ERR_S_TH_C;
		cr->htsig_crc8_err_s_th_m = CNT_HTSIG_CRC8_ERR_S_TH_C_M;
		cr->htsig_crc8_err_l_th = CNT_HTSIG_CRC8_ERR_L_TH_C;
		cr->htsig_crc8_err_l_th_m = CNT_HTSIG_CRC8_ERR_L_TH_C_M;
		cr->brk = CNT_BRK_C;
		cr->brk_m = CNT_BRK_C_M;
		cr->brk_sel = CNT_BRK_SEL_C;
		cr->brk_sel_m = CNT_BRK_SEL_C_M;
		cr->rxl_err_parity = CNT_RXL_ERR_PARITY_C;
		cr->rxl_err_parity_m = CNT_RXL_ERR_PARITY_C_M;
		cr->rxl_err_rate = CNT_RXL_ERR_RATE_C;
		cr->rxl_err_rate_m = CNT_RXL_ERR_RATE_C_M;
		cr->ht_err_crc8 = CNT_HT_ERR_CRC8_C;
		cr->ht_err_crc8_m = CNT_HT_ERR_CRC8_C_M;
		cr->vht_err_siga_crc8 = CNT_VHT_ERR_SIGA_CRC8_C;
		cr->vht_err_siga_crc8_m = CNT_VHT_ERR_SIGA_CRC8_C_M;
		cr->ht_not_support_mcs = CNT_HT_NOT_SUPPORT_MCS_C;
		cr->ht_not_support_mcs_m = CNT_HT_NOT_SUPPORT_MCS_C_M;
		cr->vht_not_support_mcs = CNT_VHT_NOT_SUPPORT_MCS_C;
		cr->vht_not_support_mcs_m = CNT_VHT_NOT_SUPPORT_MCS_C_M;
		cr->err_during_bt_tx = CNT_ERR_DURING_BT_TX_C;
		cr->err_during_bt_tx_m = CNT_ERR_DURING_BT_TX_C_M;
		cr->err_during_bt_rx = CNT_ERR_DURING_BT_RX_C;
		cr->err_during_bt_rx_m = CNT_ERR_DURING_BT_RX_C_M;
		cr->edge_murx_nsts0 = CNT_EDGE_MURX_NSTS0_C;
		cr->edge_murx_nsts0_m = CNT_EDGE_MURX_NSTS0_C_M;
		cr->search_fail = CNT_SEARCH_FAIL_C;
		cr->search_fail_m = CNT_SEARCH_FAIL_C_M;
		cr->ofdm_cca = CNT_OFDM_CCA_C;
		cr->ofdm_cca_m = CNT_OFDM_CCA_C_M;
		cr->ofdm_cca_s20 = CNT_OFDM_CCA_S20_C;
		cr->ofdm_cca_s20_m = CNT_OFDM_CCA_S20_C_M;
		cr->ofdm_cca_s40 = CNT_OFDM_CCA_S40_C;
		cr->ofdm_cca_s40_m = CNT_OFDM_CCA_S40_C_M;
		cr->ofdm_cca_s80 = CNT_OFDM_CCA_S80_C;
		cr->ofdm_cca_s80_m = CNT_OFDM_CCA_S80_C_M;
		cr->ccktxon = CNT_CCKTXON_C;
		cr->ccktxon_m = CNT_CCKTXON_C_M;
		cr->ccktxen = CNT_CCKTXEN_C;
		cr->ccktxen_m = CNT_CCKTXEN_C_M;
		cr->ofdmtxon = CNT_OFDMTXON_C;
		cr->ofdmtxon_m = CNT_OFDMTXON_C_M;
		cr->ofdmtxen = CNT_OFDMTXEN_C;
		cr->ofdmtxen_m = CNT_OFDMTXEN_C_M;
		cr->drop_trig = CNT_DROP_TRIG_C;
		cr->drop_trig_m = CNT_DROP_TRIG_C_M;
		cr->pop_trig = CNT_POP_TRIG_C;
		cr->pop_trig_m = CNT_POP_TRIG_C_M;
		cr->tx_conflict = CNT_TX_CONFLICT_C;
		cr->tx_conflict_m = CNT_TX_CONFLICT_C_M;
		cr->wmac_rstb = CNT_WMAC_RSTB_C;
		cr->wmac_rstb_m = CNT_WMAC_RSTB_C_M;
		cr->en_tb_ppdu_fix_gain = CNT_EN_TB_PPDU_FIX_GAIN_C;
		cr->en_tb_ppdu_fix_gain_m = CNT_EN_TB_PPDU_FIX_GAIN_C_M;
		cr->en_tb_cca_pw_th = CNT_EN_TB_CCA_PW_TH_C;
		cr->en_tb_cca_pw_th_m = CNT_EN_TB_CCA_PW_TH_C_M;
		cr->he_crc_ok = CNT_HE_CRC_OK_C;
		cr->he_crc_ok_m = CNT_HE_CRC_OK_C_M;
		cr->he_crc_err = CNT_HE_CRC_ERR_C;
		cr->he_crc_err_m = CNT_HE_CRC_ERR_C_M;
		cr->vht_crc_ok = CNT_VHT_CRC_OK_C;
		cr->vht_crc_ok_m = CNT_VHT_CRC_OK_C_M;
		cr->vht_crc_err = CNT_VHT_CRC_ERR_C;
		cr->vht_crc_err_m = CNT_VHT_CRC_ERR_C_M;
		cr->ht_crc_ok = CNT_HT_CRC_OK_C;
		cr->ht_crc_ok_m = CNT_HT_CRC_OK_C_M;
		cr->ht_crc_err = CNT_HT_CRC_ERR_C;
		cr->ht_crc_err_m = CNT_HT_CRC_ERR_C_M;
		cr->l_crc_ok = CNT_L_CRC_OK_C;
		cr->l_crc_ok_m = CNT_L_CRC_OK_C_M;
		cr->l_crc_err = CNT_L_CRC_ERR_C;
		cr->l_crc_err_m = CNT_L_CRC_ERR_C_M;
		cr->he_crc_ok2 = CNT_HE_CRC_OK2_C;
		cr->he_crc_ok2_m = CNT_HE_CRC_OK2_C_M;
		cr->he_crc_err2 = CNT_HE_CRC_ERR2_C;
		cr->he_crc_err2_m = CNT_HE_CRC_ERR2_C_M;
		cr->vht_crc_ok2 = CNT_VHT_CRC_OK2_C;
		cr->vht_crc_ok2_m = CNT_VHT_CRC_OK2_C_M;
		cr->vht_crc_err2 = CNT_VHT_CRC_ERR2_C;
		cr->vht_crc_err2_m = CNT_VHT_CRC_ERR2_C_M;
		cr->ht_crc_ok2 = CNT_HT_CRC_OK2_C;
		cr->ht_crc_ok2_m = CNT_HT_CRC_OK2_C_M;
		cr->ht_crc_err2 = CNT_HT_CRC_ERR2_C;
		cr->ht_crc_err2_m = CNT_HT_CRC_ERR2_C_M;
		cr->l_crc_ok2 = CNT_L_CRC_OK2_C;
		cr->l_crc_ok2_m = CNT_L_CRC_OK2_C_M;
		cr->l_crc_err2 = CNT_L_CRC_ERR2_C;
		cr->l_crc_err2_m = CNT_L_CRC_ERR2_C_M;
		cr->l_crc_ok3 = CNT_L_CRC_OK3_C;
		cr->l_crc_ok3_m = CNT_L_CRC_OK3_C_M;
		cr->l_crc_err3 = CNT_L_CRC_ERR3_C;
		cr->l_crc_err3_m = CNT_L_CRC_ERR3_C_M;
		cr->ampdu_rxon = CNT_AMPDU_RXON_C;
		cr->ampdu_rxon_m = CNT_AMPDU_RXON_C_M;
		cr->ampdu_miss = CNT_AMPDU_MISS_C;
		cr->ampdu_miss_m = CNT_AMPDU_MISS_C_M;
		cr->ampdu_crc_ok = CNT_AMPDU_RX_CRC32_OK_C;
		cr->ampdu_crc_ok_m = CNT_AMPDU_RX_CRC32_OK_C_M;
		cr->ampdu_crc_err = CNT_AMPDU_RX_CRC32_ERR_C;
		cr->ampdu_crc_err_m = CNT_AMPDU_RX_CRC32_ERR_C_M;
		cr->hesu_err_sig_a_crc4 = CNT_HESU_ERR_SIG_A_CRC4_C;
		cr->hesu_err_sig_a_crc4_m = CNT_HESU_ERR_SIG_A_CRC4_C_M;
		cr->heersu_err_sig_a_crc4 = CNT_HEERSU_ERR_SIG_A_CRC4_C;
		cr->heersu_err_sig_a_crc4_m = CNT_HEERSU_ERR_SIG_A_CRC4_C_M;
		cr->hemu_err_sig_a_crc4 = CNT_HEMU_ERR_SIG_A_CRC4_C;
		cr->hemu_err_sig_a_crc4_m = CNT_HEMU_ERR_SIG_A_CRC4_C_M;
		cr->hemu_err_sigb_ch1_comm_crc4 = CNT_HEMU_ERR_SIGB_CH1_COMM_CRC4_C;
		cr->hemu_err_sigb_ch1_comm_crc4_m = CNT_HEMU_ERR_SIGB_CH1_COMM_CRC4_C_M;
		cr->hemu_err_sigb_ch2_comm_crc4 = CNT_HEMU_ERR_SIGB_CH2_COMM_CRC4_C;
		cr->hemu_err_sigb_ch2_comm_crc4_m = CNT_HEMU_ERR_SIGB_CH2_COMM_CRC4_C_M;
		cr->he_u0_err_bcc_mcs = CNT_HE_U0_ERR_BCC_MCS_C;
		cr->he_u0_err_bcc_mcs_m = CNT_HE_U0_ERR_BCC_MCS_C_M;
		cr->he_u0_err_mcs = CNT_HE_U0_ERR_MCS_C;
		cr->he_u0_err_mcs_m = CNT_HE_U0_ERR_MCS_C_M;
		cr->he_u0_err_dcm_mcs = CNT_HE_U0_ERR_DCM_MCS_C;
		cr->he_u0_err_dcm_mcs_m = CNT_HE_U0_ERR_DCM_MCS_C_M;
		cr->r1b_rx_rpt_rst = R1B_RX_RPT_RST_C;
		cr->r1b_rx_rpt_rst_m = R1B_RX_RPT_RST_C_M;
		cr->r1b_rr_sel = R1B_RR_SEL_C;
		cr->r1b_rr_sel_m = R1B_RR_SEL_C_M;
		cr->rst_all_cnt = RST_ALL_CNT_C;
		cr->rst_all_cnt_m = RST_ALL_CNT_C_M;
		cr->enable_all_cnt = ENABLE_ALL_CNT_C;
		cr->enable_all_cnt_m = ENABLE_ALL_CNT_C_M;
		cr->enable_ofdm = ENABLE_OFDM_C;
		cr->enable_ofdm_m = ENABLE_OFDM_C_M;
		cr->enable_cck = ENABLE_CCK_C;
		cr->enable_cck_m = ENABLE_CCK_C_M;
		cr->r1b_rx_dis_cca = R1B_RX_DIS_CCA_C;
		cr->r1b_rx_dis_cca_m = R1B_RX_DIS_CCA_C_M;
		cr->intf_r_rate = INTF_R_CNT_RATE_C;
		cr->intf_r_rate_m = INTF_R_CNT_RATE_C_M;
		cr->intf_r_mcs = INTF_R_CNT_MCS_C;
		cr->intf_r_mcs_m = INTF_R_CNT_MCS_C_M;
		cr->intf_r_vht_mcs = INTF_R_CNT_VHT_MCS_C;
		cr->intf_r_vht_mcs_m = INTF_R_CNT_VHT_MCS_C_M;
		cr->intf_r_he_mcs = INTF_R_CNT_HE_MCS_C;
		cr->intf_r_he_mcs_m = INTF_R_CNT_HE_MCS_C_M;
		cr->intf_r_vht_nss = INTF_R_CNT_VHT_NSS_C;
		cr->intf_r_vht_nss_m = INTF_R_CNT_VHT_NSS_C_M;
		cr->intf_r_he_nss = INTF_R_CNT_HE_NSS_C;
		cr->intf_r_he_nss_m = INTF_R_CNT_HE_NSS_C_M;
		cr->intf_r_mac_hdr_type = INTF_R_MAC_HDR_TYPE_C;
		cr->intf_r_mac_hdr_type_m = INTF_R_MAC_HDR_TYPE_C_M;
		cr->intf_r_pkt_type = INTF_R_PKT_TYPE_C;
		cr->intf_r_pkt_type_m = INTF_R_PKT_TYPE_C_M;
		cr->dbcc = DBCC_C;
		cr->dbcc_m = DBCC_C_M;
		cr->dbcc_2p4g_band_sel = DBCC_2P4G_BAND_SEL_C;
		cr->dbcc_2p4g_band_sel_m = DBCC_2P4G_BAND_SEL_C_M;
		cr->cnt_pop_trig = CNT_POP_TRIG_C;
		cr->cnt_pop_trig_m = CNT_POP_TRIG_C_M;
		cr->max_cnt_pop = MAX_CNT_POP_C;
		cr->max_cnt_pop_m = MAX_CNT_POP_C_M;
		break;
	#endif
	default:
		break;
	}

}

#endif
