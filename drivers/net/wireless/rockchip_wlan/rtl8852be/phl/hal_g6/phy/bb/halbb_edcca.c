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

#ifdef HALBB_EDCCA_SUPPORT
bool halbb_edcca_abort(struct bb_info *bb)
{
	if (!(bb->support_ability & BB_EDCCA)) {
		BB_DBG(bb, DBG_EDCCA, "edcca disable\n");
		return true;
	}

	if (bb->pause_ability & BB_EDCCA) {
		BB_DBG(bb, DBG_EDCCA, "Return edcca pause\n");
		return true;
	}

	return false;
}

void halbb_set_edcca_thre(struct bb_info *bb)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;
	struct bb_edcca_cr_info *cr = &bb->bb_edcca_i.bb_edcca_cr_i;
	u32 l2h = bb_edcca->th_h;

	halbb_set_reg(bb, cr->r_edcca_level_p, cr->r_edcca_level_p_m, l2h);
	halbb_set_reg(bb, cr->r_edcca_level, cr->r_edcca_level_m, l2h);
	
	halbb_set_reg(bb, cr->r_dwn_level, cr->r_dwn_level_m, (u32)bb_edcca->th_hl_diff);
}

u8 halbb_edcca_thre_transfer_rssi(struct bb_info *bb)
{
	u8 rssi_min = bb->bb_ch_i.rssi_min >> 1;
	u8 edcca_thre = 0;

	/*mapping between rssi and edcca thre */
	edcca_thre = rssi_min -110 + 128;

	if (edcca_thre <= EDCCA_TH_L2H_LB)
		edcca_thre = EDCCA_TH_L2H_LB;

	return edcca_thre;
}

void halbb_edcca_thre_calc(struct bb_info *bb)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;
	u8 band = bb->hal_com->band[0].cur_chandef.band;
	u8 th_h = 0;

	BB_DBG(bb, DBG_EDCCA, "[EDCCA] Mode=%d, Band=%d\n",
	       bb_edcca->edcca_mode, band);

	BB_DBG(bb, DBG_EDCCA,
	       "[EDCCA] Adapt-5G_th=%d(dBm), Adapt-2.4G_th=%d(dBm),Carrier-sense_th=%d(dBm)\n", 
	       bb_edcca->th_h_5g - 128, bb_edcca->th_h_2p4g - 128,
	       bb_edcca->th_h_cs - 128);

	if (bb_edcca->edcca_mode == EDCCA_NORMAL_MODE) {
		BB_DBG(bb, DBG_EDCCA, "Normal Mode without EDCCA\n");
		th_h = halbb_edcca_thre_transfer_rssi(bb);
		bb_edcca->th_hl_diff = EDCCA_HL_DIFF_NORMAL;
	} else if (bb_edcca->edcca_mode == EDCCA_ADAPT_MODE) {
		if (band == BAND_ON_24G)
			th_h = bb_edcca->th_h_2p4g;
		else
			th_h = bb_edcca->th_h_5g;
		bb_edcca->th_hl_diff = EDCCA_HL_DIFF_ADPTVTY;
	} else if (bb_edcca->edcca_mode == EDCCA_CARRIER_SENSE_MODE) {
		th_h = bb_edcca->th_h_cs;
		bb_edcca->th_hl_diff = EDCCA_HL_DIFF_ADPTVTY;
	}
	bb_edcca->th_h = th_h;
	bb_edcca->th_l = bb_edcca->th_h - bb_edcca->th_hl_diff;

	halbb_set_edcca_thre(bb);
}

void halbb_set_edcca_pause_val(struct bb_info *bb, u32 *val_buf, u8 val_len)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;

	if (val_len != 1) {
		BB_DBG(bb, DBG_EDCCA, "[Error][EDCCA]Need val_len=1\n");
		return;
	}
	BB_DBG(bb, DBG_EDCCA, "[%s] len=%d, val[0]=0x%x\n", __func__, val_len, val_buf[0]);

	bb_edcca->th_h = (u8)val_buf[0];
	halbb_set_edcca_thre(bb);
}

void halbb_edcca_event_nofity(struct bb_info *bb, u8 pause_type)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;
	u8 pause_result = 0;
	u32 val[5] = {0};

	if (bb_edcca->edcca_mode != EDCCA_NORMAL_MODE)
		return;

	val[0] = EDCCA_MAX;
	pause_result = halbb_pause_func(bb, F_EDCCA, pause_type, HALBB_PAUSE_LV_2, 1, val);
}

void halbb_edcca_log(struct bb_info *bb)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;
	struct edcca_hw_rpt *rpt = &bb_edcca->edcca_rpt;
	struct bb_edcca_cr_info *cr = &bb->bb_edcca_i.bb_edcca_cr_i;
	enum channel_width bw = 0;
	u8 edcca_p_th = 0;
	u8 edcca_s_th = 0;
	u8 edcca_diff = 0;
	bool edcca_en = 0;

	bw = bb->hal_com->band[0].cur_chandef.bw;

	switch (bw) {
	case CHANNEL_WIDTH_80_80:
	case CHANNEL_WIDTH_160:
		BB_DBG(bb, DBG_EDCCA,
		       "pwdb per20{0,1,2,3,4,5,6,7}={%d,%d,%d,%d,%d,%d,%d,%d}(dBm)\n",
		       rpt->pwdb_0, rpt->pwdb_1, rpt->pwdb_2, rpt->pwdb_3,
		       rpt->pwdb_4, rpt->pwdb_5, rpt->pwdb_6, rpt->pwdb_7);
		BB_DBG(bb, DBG_EDCCA,
		       "path=%d, flag {FB,p20,s20,s40,s80}={%d,%d,%d,%d,%d}\n",
		       rpt->path, rpt->flag_fb, rpt->flag_p20, rpt->flag_s20,
		       rpt->flag_s40, rpt->flag_s80);
		BB_DBG(bb, DBG_EDCCA,
		       "pwdb {FB,p20,s20,s40,s80}={%d,%d,%d,%d,%d}(dBm)\n",
		       rpt->pwdb_fb, rpt->pwdb_p20, rpt->pwdb_s20, rpt->pwdb_s40,
		       rpt->pwdb_s80);
		break;
	case CHANNEL_WIDTH_80:
		BB_DBG(bb, DBG_EDCCA,
		       "pwdb per20{0,1,2,3}={%d,%d,%d,%d}(dBm)\n",
		       rpt->pwdb_0, rpt->pwdb_1, rpt->pwdb_2, rpt->pwdb_3);
		BB_DBG(bb, DBG_EDCCA, "path=%d, flag {FB,p20,s20,s40}={%d,%d,%d,%d}\n",
		       rpt->path, rpt->flag_fb, rpt->flag_p20, rpt->flag_s20,
		       rpt->flag_s40);
		BB_DBG(bb, DBG_EDCCA,
		       "pwdb {FB,p20,s20,s40}={%d,%d,%d,%d}(dBm)\n",
		       rpt->pwdb_fb, rpt->pwdb_p20, rpt->pwdb_s20, rpt->pwdb_s40);
		break;
	case CHANNEL_WIDTH_40:
		BB_DBG(bb, DBG_EDCCA, "pwdb per20{0,1}={%d,%d}(dBm)\n", rpt->pwdb_0,
		       rpt->pwdb_1);
		BB_DBG(bb, DBG_EDCCA, "path=%d, flag {FB,p20,s20}={%d,%d,%d}\n",
		       rpt->path, rpt->flag_fb, rpt->flag_p20, rpt->flag_s20);
		BB_DBG(bb, DBG_EDCCA, "pwdb {FB,p20,s20}={%d,%d,%d}(dBm)\n",
		       rpt->pwdb_fb, rpt->pwdb_p20, rpt->pwdb_s20);
		break;
	case CHANNEL_WIDTH_20:
		BB_DBG(bb, DBG_EDCCA, "pwdb per20{0}={%d}(dBm)\n", rpt->pwdb_0);
		BB_DBG(bb, DBG_EDCCA, "path=%d, flag {FB,p20}={%d,%d}\n", rpt->path,
		       rpt->flag_fb, rpt->flag_p20);
		BB_DBG(bb, DBG_EDCCA, "pwdb {FB,p20}={%d,%d}(dBm)\n", rpt->pwdb_fb,
		       rpt->pwdb_p20);
		break;
	default:
		break;
	}

	edcca_en = (bool)halbb_get_reg(bb, cr->r_snd_en, cr->r_snd_en_m);
	edcca_p_th = (u8)halbb_get_reg(bb, cr->r_edcca_level_p, cr->r_edcca_level_p_m);
	edcca_s_th = (u8)halbb_get_reg(bb, cr->r_edcca_level, cr->r_edcca_level_m);
	edcca_diff = (u8)halbb_get_reg(bb, cr->r_dwn_level, cr->r_dwn_level_m);

	BB_DBG(bb, DBG_EDCCA,
	       "reg val{en, p20_h_th, sec_h_th, diff}:{%d, %d, %d, %d}\n",
	       edcca_en, edcca_p_th, edcca_s_th, edcca_diff);
}

void halbb_edcca_get_result(struct bb_info *bb)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;
	struct edcca_hw_rpt *rpt = &bb_edcca->edcca_rpt;
	struct bb_edcca_cr_info *cr = &bb->bb_edcca_i.bb_edcca_cr_i;
	u32 tmp = 0;
	u64 tmp_linear = 0;
	enum channel_width bw = 0;

	bw = bb->hal_com->band[0].cur_chandef.bw;

	halbb_set_reg(bb, cr->r_edcca_rpt_sel, cr->r_edcca_rpt_sel_m, 0x0);
	tmp = halbb_get_reg(bb, cr->r_edcca_rpt_a, cr->r_edcca_rpt_a_m);
	rpt->pwdb_1 = (s8)(((tmp & MASKBYTE2) >> 16) - 256);
	rpt->pwdb_0 = (s8)(((tmp & MASKBYTE3) >> 24) - 256);
	tmp = halbb_get_reg(bb, cr->r_edcca_rpt_b, cr->r_edcca_rpt_b_m);
	rpt->path = (u8)((tmp & 0x6) >> 1);
	rpt->flag_s80 = (bool)((tmp & BIT(3)) >> 3);
	rpt->flag_s40 = (bool)((tmp & BIT(4)) >> 4);
	rpt->flag_s20 = (bool)((tmp & BIT(5)) >> 5);
	rpt->flag_p20 = (bool)((tmp & BIT(6)) >> 6);
	rpt->flag_fb = (bool)((tmp & BIT(7)) >> 7);
	rpt->pwdb_s20 = (s8)(((tmp & MASKBYTE1) >> 8) - 256);
	rpt->pwdb_p20 = (s8)(((tmp & MASKBYTE2) >> 16) - 256);
	rpt->pwdb_fb = (s8)(((tmp & MASKBYTE3) >> 24) - 256);

	switch (bw) {
	case CHANNEL_WIDTH_80_80:
	case CHANNEL_WIDTH_160:
		halbb_set_reg(bb, cr->r_edcca_rpt_sel, cr->r_edcca_rpt_sel_m,
			      0x5);
		tmp = halbb_get_reg(bb, cr->r_edcca_rpt_a, cr->r_edcca_rpt_a_m);
		rpt->pwdb_3 = (s8)(((tmp & MASKBYTE2) >> 16) - 256);
		rpt->pwdb_2 = (s8)(((tmp & MASKBYTE3) >> 24) - 256);
		tmp = halbb_get_reg(bb, cr->r_edcca_rpt_b, cr->r_edcca_rpt_b_m);
		rpt->pwdb_s80 = (s8)(((tmp & MASKBYTE1) >> 8) - 256);
		rpt->pwdb_s40 = (s8)(((tmp & MASKBYTE2) >> 16) - 256);

		halbb_set_reg(bb, cr->r_edcca_rpt_sel, cr->r_edcca_rpt_sel_m,
			      0x2);
		tmp = halbb_get_reg(bb, cr->r_edcca_rpt_a, cr->r_edcca_rpt_a_m);
		rpt->pwdb_5 = (s8)(((tmp & MASKBYTE2) >> 16) - 256);
		rpt->pwdb_4 = (s8)(((tmp & MASKBYTE3) >> 24) - 256);
		
		halbb_set_reg(bb, cr->r_edcca_rpt_sel, cr->r_edcca_rpt_sel_m,
			      0x3);
		tmp = halbb_get_reg(bb, cr->r_edcca_rpt_a, cr->r_edcca_rpt_a_m);
		rpt->pwdb_7 = (s8)(((tmp & MASKBYTE2) >> 16) - 256);
		rpt->pwdb_6 = (s8)(((tmp & MASKBYTE3) >> 24) - 256);
		break;
	case CHANNEL_WIDTH_80:
		halbb_set_reg(bb, cr->r_edcca_rpt_sel, cr->r_edcca_rpt_sel_m,
			      0x5);
		tmp = halbb_get_reg(bb, cr->r_edcca_rpt_a, cr->r_edcca_rpt_a_m);
		rpt->pwdb_3 = (s8)(((tmp & MASKBYTE2) >> 16) - 256);
		rpt->pwdb_2 = (s8)(((tmp & MASKBYTE3) >> 24) - 256);
		tmp = halbb_get_reg(bb, cr->r_edcca_rpt_b, cr->r_edcca_rpt_b_m);
		rpt->pwdb_s80 = (s8)(((tmp & MASKBYTE1) >> 8) - 256);
		rpt->pwdb_s40 = (s8)(((tmp & MASKBYTE2) >> 16) - 256);
		break;
	case CHANNEL_WIDTH_40:
		/*52A/52B/52C has hw bug of pwdb-FB is 0 when bw=40M*/
		if ((bb->ic_type == BB_RTL8852A) || (bb->ic_type == BB_RTL8852B) ||
		    (bb->ic_type == BB_RTL8852C)) {
			if ((rpt->pwdb_p20 == (s8)(EDCCA_PWDB_EXCLU_TX)) ||
			    (rpt->pwdb_s20 == (s8)(EDCCA_PWDB_EXCLU_TX))) {
				rpt->pwdb_fb = (s8)(EDCCA_PWDB_EXCLU_TX);
			} else {
				tmp = (u32)EDCCA_PWDB_TO_RSSI(rpt->pwdb_p20);
				tmp_linear = halbb_db_2_linear(tmp);
				tmp = (u32)EDCCA_PWDB_TO_RSSI(rpt->pwdb_s20);
				tmp_linear += halbb_db_2_linear(tmp);
				tmp_linear = (tmp_linear + (1 << (FRAC_BITS - 1))) >> FRAC_BITS;
				tmp = halbb_convert_to_db(tmp_linear);
				rpt->pwdb_fb = (s8)(tmp - 110);
			}
		}
		break;
	default:
		break;
	}
}

void halbb_edcca(struct bb_info *bb)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;

	if (halbb_edcca_abort(bb))
		return;

	bb_edcca->edcca_mode = bb->phl_com->edcca_mode;
	halbb_edcca_thre_calc(bb);
	BB_DBG(bb, DBG_EDCCA, "th_h=%d(dBm), th_l=%d(dBm)\n",
	       bb_edcca->th_h - 128, bb_edcca->th_l - 128);
	halbb_edcca_get_result(bb);
	halbb_edcca_log(bb);
}

void halbb_fw_edcca(struct bb_info *bb)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;
	struct rtw_phl_com_t *phl = bb->phl_com;
	struct rtw_hal_com_t *hal = bb->hal_com;
	u8 band = bb->hal_com->band[0].cur_chandef.band;
	struct bb_h2c_fw_edcca *fw_edcca_i = &bb->bb_fw_edcca_i;
	u8 cmdlen;
	bool ret_val = false;
	u32 *bb_h2c = (u32 *)fw_edcca_i;
	cmdlen = sizeof(struct bb_h2c_fw_edcca);

	bb_edcca->edcca_mode = phl->edcca_mode;
	//bb_edcca->edcca_mode = EDCCA_ADAPT_MODE;

	if (halbb_edcca_abort(bb))
		return;

	/* FW workaround only for 8852A CAV */
	if (!((hal->cv == CAV) && (hal->chip_id == CHIP_WIFI6_8852A)))
		return;
	
	if (bb_edcca->edcca_mode == EDCCA_NORMAL_MODE) {
		BB_DBG(bb, DBG_EDCCA, "Normal Mode without FW EDCCA\n");
		return;
	}

	BB_DBG(bb, DBG_EDCCA, "FW EDCCA start\n");

	fw_edcca_i->mode = bb_edcca->edcca_mode;
	fw_edcca_i->band = bb->hal_com->band[0].cur_chandef.band;

	BB_DBG(bb, DBG_EDCCA, "[EDCCA] Mode=%d, Band=%d\n", 
				fw_edcca_i->mode, fw_edcca_i->band);
	BB_DBG(bb, DBG_EDCCA, "[EDCCA] Adapt-5G_th=-%d, Adapt-2.4G_th=-%d,Carrier-sense_th=-%d\n", 
				fw_edcca_i->pwr_th_5g, fw_edcca_i->pwr_th_2p4, 
				fw_edcca_i->pwr_th_cs);
	
	BB_DBG(bb, DBG_FW_INFO, "[FW][H2C] h2c conent[0]=%x\n", bb_h2c[0]);
	BB_DBG(bb, DBG_FW_INFO, "[FW][H2C] h2c conent[1]=%x\n", bb_h2c[1]);

	ret_val = halbb_fill_h2c_cmd(bb, cmdlen, DM_H2C_FW_EDCCA, 
					     HALBB_H2C_DM, bb_h2c);
	if (ret_val == false)
		BB_WARNING(" H2C cmd: FW Tx error!!\n");
}

void halbb_edcca_dbg(struct bb_info *bb, char input[][16], u32 *_used,
			      char *output, u32 *_out_len)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;
	struct edcca_hw_rpt *rpt = &bb_edcca->edcca_rpt;
	struct bb_h2c_fw_edcca *fw_edcca_i = &bb->bb_fw_edcca_i;
	enum channel_width bw = 0;
	char help[] = "-h";
	u32 var[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;

	bw = bb->hal_com->band[0].cur_chandef.bw;

	if ((_os_strcmp(input[1], help) == 0)) {
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "[EDCCA] Set power threshold(-dBm): {1} {Adapt-5G_th} {Adapt-2.4G_th} {Carrier-sense_th}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "[FW EDCCA][8852A CAV] Set power threshold(-dBm): {2} {Adapt-5G_th} {Adapt-2.4G_th} {Carrier-sense_th}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "[EDCCA] Set EDCCA mode: {3} {mode 0:normal mode, 1:Adaptivity, 2: Carrier sense}\n");
		BB_DBG_CNSL(out_len, used, output + used, out_len - used,
			 "Show Power threshold: {100}\n");
	} else {
		HALBB_SCAN(input[1], DCMD_DECIMAL, &var[0]);

		if (var[0] == 1) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
			HALBB_SCAN(input[4], DCMD_DECIMAL, &var[3]);

			bb_edcca->th_h_5g = (u8)var[1];
			bb_edcca->th_h_2p4g = (u8)var[2];
			bb_edcca->th_h_cs = (u8)var[3];

			BB_DBG_CNSL(out_len, used, output + used, out_len - used,
				    "Set Adapt-5G_th=-%d(dBm), Adapt-2.4G_th=-%d(dBm), Carrier-sense_th=-%d(dBm)\n",
				    bb_edcca->th_h_5g, bb_edcca->th_h_2p4g,
				    bb_edcca->th_h_cs);
			
			bb_edcca->th_h_5g = 0 - (bb_edcca->th_h_5g) + 128;
			bb_edcca->th_h_2p4g = 0 - (bb_edcca->th_h_2p4g) + 128;
			bb_edcca->th_h_cs = 0 - (bb_edcca->th_h_cs) + 128;

			
		} else if (var[0] == 2) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);
			HALBB_SCAN(input[3], DCMD_DECIMAL, &var[2]);
			HALBB_SCAN(input[4], DCMD_DECIMAL, &var[3]);

			fw_edcca_i->pwr_th_5g = (u8)var[1];
			fw_edcca_i->pwr_th_2p4 = (u8)var[2];
			fw_edcca_i->pwr_th_cs = (u8)var[3];
			
			BB_DBG_CNSL(out_len, used, output + used, out_len - used,
				 "Set FW Adapt-5G_th=-%d, Adapt-2.4G_th=-%d, Carrier-sense_th=-%d\n",
				  fw_edcca_i->pwr_th_5g, fw_edcca_i->pwr_th_2p4, 
				  fw_edcca_i->pwr_th_cs);

			halbb_fw_edcca(bb);
			
		} else if (var[0] == 3) {
			HALBB_SCAN(input[2], DCMD_DECIMAL, &var[1]);

			bb_edcca->edcca_mode = (u8)var[1];
			
			BB_DBG_CNSL(out_len, used, output + used, out_len - used,
				 "Set FW EDCCA mode = %s\n", (bb_edcca->edcca_mode == EDCCA_NORMAL_MODE) ? "Normal mode" : "Adaptivity/Carrier Sense mode");

			halbb_fw_edcca(bb);
			
		} else if (var[0] == 100) {
			BB_DBG_CNSL(out_len, used, output + used, out_len - used,
				    "Adapt-5G_th=%d(dBm), Adapt-2.4G_th=%d(dBm), Carrier-sense_th=%d(dBm)\n",
				    bb_edcca->th_h_5g - 128,
				    bb_edcca->th_h_2p4g -128,
				    bb_edcca->th_h_cs - 128);
			BB_DBG_CNSL(out_len, used, output + used, out_len - used,
				    "Mode=%d, th_h=%d(dBm), th_l=%d(dBm)\n",
				    bb_edcca->edcca_mode, bb_edcca->th_h - 128,
				    bb_edcca->th_l - 128);

			halbb_edcca_get_result(bb);
			switch (bw) {
			case CHANNEL_WIDTH_80_80:
			case CHANNEL_WIDTH_160:
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "pwdb per20{0,1,2,3,4,5,6,7}={%d,%d,%d,%d,%d,%d,%d,%d}(dBm)\n",
					    rpt->pwdb_0, rpt->pwdb_1, rpt->pwdb_2,
					    rpt->pwdb_3, rpt->pwdb_4, rpt->pwdb_5,
					    rpt->pwdb_6, rpt->pwdb_7);
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "path-%d, flag {FB,p20,s20,s40,s80}={%d,%d,%d,%d,%d}\n",
					    rpt->path, rpt->flag_fb, rpt->flag_p20,
					    rpt->flag_s20, rpt->flag_s40,
					    rpt->flag_s80);
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "pwdb {FB,p20,s20,s40,s80}={%d,%d,%d,%d,%d}(dBm)\n",
					    rpt->pwdb_fb, rpt->pwdb_p20,
					    rpt->pwdb_s20, rpt->pwdb_s40,
					    rpt->pwdb_s80);
				break;
			case CHANNEL_WIDTH_80:
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "pwdb per20{0,1,2,3}={%d,%d,%d,%d}(dBm)\n",
					    rpt->pwdb_0, rpt->pwdb_1, rpt->pwdb_2,
					    rpt->pwdb_3);
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "path-%d, flag {FB,p20,s20,s40}={%d,%d,%d,%d}\n",
					    rpt->path, rpt->flag_fb, rpt->flag_p20,
					    rpt->flag_s20, rpt->flag_s40);
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "pwdb {FB,p20,s20,s40}={%d,%d,%d,%d}(dBm)\n",
					    rpt->pwdb_fb, rpt->pwdb_p20,
					    rpt->pwdb_s20, rpt->pwdb_s40);
				break;
			case CHANNEL_WIDTH_40:
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "pwdb per20{0,1}={%d,%d}(dBm)\n",
					    rpt->pwdb_0, rpt->pwdb_1);
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "path-%d, flag {FB,p20,s20}={%d,%d,%d}\n",
					    rpt->path, rpt->flag_fb, rpt->flag_p20,
					    rpt->flag_s20);
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "pwdb {FB,p20,s20}={%d,%d,%d}(dBm)\n",
					    rpt->pwdb_fb, rpt->pwdb_p20,
					    rpt->pwdb_s20);
				break;
			case CHANNEL_WIDTH_20:
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "pwdb per20{0}={%d}(dBm)\n",
					    rpt->pwdb_0);
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "path-%d, flag {FB,p20}={%d,%d}\n",
					    rpt->path, rpt->flag_fb, rpt->flag_p20);
				BB_DBG_CNSL(out_len, used, output + used,
					    out_len - used,
					    "pwdb {FB,p20}={%d,%d}(dBm)\n",
					    rpt->pwdb_fb, rpt->pwdb_p20);
				break;
			default:
				break;
			}

			#ifdef BB_8852A_52AA_CUT_SUPPORT
			BB_DBG_CNSL(out_len, used, output + used, out_len - used,
				 "FW Adapt-5G_th=-%d, Adapt-2.4G_th=-%d, Carrier-sense_th=-%d\n",
				  fw_edcca_i->pwr_th_5g, fw_edcca_i->pwr_th_2p4, 
				  fw_edcca_i->pwr_th_cs);
			#endif
		}
	}
	*_used = used;
	*_out_len = out_len;
}

void halbb_edcca_dev_hw_cap(struct bb_info *bb)
{
	struct rtw_hal_com_t *hal = bb->hal_com;

	hal->dev_hw_cap.edcca_cap.edcca_adap_th_5g = EDCCA_5G;
	hal->dev_hw_cap.edcca_cap.edcca_adap_th_2g = EDCCA_2G;
	if (bb->ic_type == BB_RTL8852B) /*[HALBB-126] for SingleTone shift 1MHz*/
		hal->dev_hw_cap.edcca_cap.edcca_carrier_sense_th = CARRIER_SENSE - 6;
	else
		hal->dev_hw_cap.edcca_cap.edcca_carrier_sense_th = CARRIER_SENSE;
}

void halbb_edcca_init(struct bb_info *bb)
{
	struct bb_edcca_info *bb_edcca = &bb->bb_edcca_i;
	struct bb_h2c_fw_edcca *fw_edcca_i = &bb->bb_fw_edcca_i;
	struct bb_edcca_cr_info *cr = &bb->bb_edcca_i.bb_edcca_cr_i;
	struct rtw_phl_com_t *phl = bb->phl_com;

	if(phl_is_mp_mode(bb->phl_com))
		return;

	bb_edcca->edcca_mode = phl->edcca_mode;

	bb_edcca->th_h = EDCCA_MAX;
	bb_edcca->th_l = EDCCA_MAX;
	bb_edcca->th_h_lb = 46;

	// EDCCA
	bb_edcca->th_h_5g = phl->dev_cap.edcca_cap.edcca_adap_th_5g;
	bb_edcca->th_h_2p4g = phl->dev_cap.edcca_cap.edcca_adap_th_2g;
	bb_edcca->th_h_cs = phl->dev_cap.edcca_cap.edcca_carrier_sense_th;

	// FW EDCCA
	fw_edcca_i->pwr_th_5g = EDCCA_5G_TH;
	fw_edcca_i->pwr_th_2p4 = EDCCA_2p4G_TH;
	fw_edcca_i->pwr_th_cs = CARRIER_SENSE_TH;
}


void halbb_cr_cfg_edcca_init(struct bb_info *bb)
{
	struct bb_edcca_cr_info *cr = &bb->bb_edcca_i.bb_edcca_cr_i;

	switch (bb->cr_type) {

	#ifdef BB_8852A_52AA_CUT_SUPPORT
	case BB_52AA:
		cr->r_snd_en = SEG0R_SND_EN_52AA;
		cr->r_snd_en_m = SEG0R_SND_EN_52AA_M;
		cr->r_dwn_level = SEG0R_DWN_LVL_52AA;
		cr->r_dwn_level_m = SEG0R_DWN_LVL_52AA_M;
		cr->r_edcca_level = SEG0R_EDCCA_LVL_52AA;
		cr->r_edcca_level_m = SEG0R_EDCCA_LVL_52AA_M;
		cr->r_edcca_level_p = SEG0R_EDCCA_LVL_P_52AA;
		cr->r_edcca_level_p_m = SEG0R_EDCCA_LVL_P_52AA_M;
		break;

	#endif
	#ifdef HALBB_COMPILE_AP_SERIES
	case BB_AP:
		cr->r_snd_en = SEG0R_SND_EN_A;
		cr->r_snd_en_m = SEG0R_SND_EN_A_M;
		cr->r_dwn_level = SEG0R_DWN_LVL_A;
		cr->r_dwn_level_m = SEG0R_DWN_LVL_A_M;
		cr->r_edcca_level = SEG0R_EDCCA_LVL_A;
		cr->r_edcca_level_m = SEG0R_EDCCA_LVL_A_M;
		cr->r_edcca_level_p = SEG0R_EDCCA_LVL_P_A;
		cr->r_edcca_level_p_m = SEG0R_EDCCA_LVL_P_A_M;
		cr->r_edcca_rpt_a = EDCCA_IOQ_P0_A_A;
		cr->r_edcca_rpt_a_m = EDCCA_IOQ_P0_A_A_M;
		cr->r_edcca_rpt_b = EDCCA_IOQ_P0_B_A;
		cr->r_edcca_rpt_b_m = EDCCA_IOQ_P0_B_A_M;
		cr->r_edcca_rpt_sel = EDCCA_RPTREG_SEL_P0_A;
		cr->r_edcca_rpt_sel_m = EDCCA_RPTREG_SEL_P0_A_M;
		break;

	#endif
	#ifdef HALBB_COMPILE_CLIENT_SERIES
	case BB_CLIENT:
		cr->r_snd_en = SEG0R_SND_EN_C;
		cr->r_snd_en_m = SEG0R_SND_EN_C_M;
		cr->r_dwn_level = SEG0R_DWN_LVL_C;
		cr->r_dwn_level_m = SEG0R_DWN_LVL_C_M;
		cr->r_edcca_level = SEG0R_EDCCA_LVL_C;
		cr->r_edcca_level_m = SEG0R_EDCCA_LVL_C_M;
		cr->r_edcca_level_p = SEG0R_EDCCA_LVL_P_C;
		cr->r_edcca_level_p_m = SEG0R_EDCCA_LVL_P_C_M;
		cr->r_edcca_rpt_a = EDCCA_IOQ_P0_A_C;
		cr->r_edcca_rpt_a_m = EDCCA_IOQ_P0_A_C_M;
		cr->r_edcca_rpt_b = EDCCA_IOQ_P0_B_C;
		cr->r_edcca_rpt_b_m = EDCCA_IOQ_P0_B_C_M;
		cr->r_edcca_rpt_sel = EDCCA_RPTREG_SEL_P0_C;
		cr->r_edcca_rpt_sel_m = EDCCA_RPTREG_SEL_P0_C_M;
		break;
	#endif

	default:
		break;
	}

}
#endif
