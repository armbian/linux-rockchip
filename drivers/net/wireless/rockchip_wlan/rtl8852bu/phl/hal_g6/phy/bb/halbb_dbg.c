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

#define HALBB_DBG_DVLP_FLAG 1

void halbb_dbg_comp_init(struct bb_info *bb)
{
	#if 0
	if (bb->phl_com->bb_dbg_comp_manual_cfg != 0xffffffff) {
		bb->dbg_component = bb->phl_com->bb_dbg_comp_manual_cfg;
		return;
	}
	#endif

	bb->dbg_component =
		/*DBG_RA |		*/
		/*DBG_FA_CNT |		*/
		/*DBG_RSSI_MNTR |	*/
		/*DBG_DFS |		*/
		/*DBG_EDCCA |		*/
		/*DBG_ENV_MNTR |	*/
		/*DBG_CFO_TRK |		*/
		/*DBG_PHY_STATUS |	*/
		/*DBG_COMMON_FLOW |	*/
		/*DBG_IC_API |		*/
		/*DBG_DBG_API |		*/
		/*DBG_DBCC |		*/
		/*DBG_DM_SUMMARY |	*/
		/*DBG_PHY_CONFIG |	*/
		/*DBG_INIT |		*/
		/*DBG_DIG |		*/
		/*DBG_CMN |		*/
		0;

	BB_DBG(bb, DBG_INIT, "dbg_comp = 0x%llx\n", bb->dbg_component);

}

void halbb_print_devider(struct bb_info *bb, u8 len, bool with_space)
{

	if (len == BB_DEVIDER_LEN_32)
		BB_TRACE("--------------------------------\n");
	else
		BB_TRACE("----------------\n");

	if (with_space)
		BB_TRACE("\n");
}

#ifdef HALBB_TDMA_CR_SUPPORT

void halbb_tdma_cr_sel_io_en(struct bb_info *bb)
{
	BB_DBG(bb, DBG_DBG_API, "[%s]===>\n", __func__);

	halbb_tdma_cr_sel_main(bb);
}

void halbb_tdma_cr_sel_callback(void *context)
{
	struct bb_info *bb = (struct bb_info *)context;
	struct halbb_timer_info *timer = &bb->bb_dbg_i.tdma_cr_timer_i;

	BB_DBG(bb, DBG_DBG_API, "[%s]===>\n", __func__);
	timer->timer_state = BB_TIMER_IDLE;

	if (bb->phl_com->hci_type == RTW_HCI_PCIE)
		halbb_tdma_cr_sel_io_en(bb);
	else
		rtw_hal_cmd_notify(bb->phl_com, MSG_EVT_NOTIFY_BB, (void *)(&timer->event_idx), bb->bb_phy_idx);
}

void halbb_tdma_cr_timer_init(struct bb_info *bb)
{
	struct halbb_timer_info *timer = &bb->bb_dbg_i.tdma_cr_timer_i;
		
	BB_DBG(bb, DBG_DBG_API, "[%s]\n", __func__);

	timer->event_idx = BB_EVENT_TIMER_TDMA_CR;
	timer->timer_state = BB_TIMER_IDLE;

	halbb_init_timer(bb, &timer->timer_list, halbb_tdma_cr_sel_callback, bb, "halbb_tdma_cr");
}

void halbb_tdma_cr_sel_main(struct bb_info *bb)
{
	struct bb_dbg_info *dbg = &bb->bb_dbg_i;
	u32 period = 0;
	u32 val_new = 0, val_old = 0;

	if (!dbg->tdma_cr_en) {
		BB_DBG(bb, DBG_DBG_API, "[%s] tdma_cr_en = %d\n", __func__, dbg->tdma_cr_en);
		return;
	}

	if (dbg->tdma_cr_state == 0) {
		val_old = dbg->tdma_cr_val_1;
		val_new = dbg->tdma_cr_val_0;
		period = dbg->tdma_cr_period_0;
		dbg->tdma_cr_state = 1; /*CR0*/
	} else { /*PFD_LEGACY*/
		val_old = dbg->tdma_cr_val_0;
		val_new = dbg->tdma_cr_val_1;
		period = dbg->tdma_cr_period_1;
		dbg->tdma_cr_state= 0;  /*CR1*/
	}

	halbb_set_reg(bb, dbg->tdma_cr_idx, dbg->tdma_cr_mask, val_new);

	dbg->tdma_cr_timer_i.cb_time = period;
	halbb_cfg_timers(bb, BB_SET_TIMER, &dbg->tdma_cr_timer_i);

	BB_DBG(bb, DBG_DBG_API, "Reg 0x%x[0x%x] = {0x%x -> 0x%x}, period=%d ms\n",
	       dbg->tdma_cr_idx, dbg->tdma_cr_mask,
	       val_old , val_new, period);
}

void halbb_tdma_cr_sel_deinit(struct bb_info *bb)
{
	BB_DBG(bb, DBG_DBG_API, "[%s]\n", __func__);
}

void halbb_tdma_cr_sel_init(struct bb_info *bb)
{
	struct bb_dbg_info *dbg = &bb->bb_dbg_i;

	BB_DBG(bb, DBG_DBG_API, "[%s]\n", __func__);
	dbg->tdma_cr_en = false;
	dbg->tdma_cr_period_0 = 50;
	dbg->tdma_cr_period_1 = 50;
	dbg->tdma_cr_idx = 0;
	dbg->tdma_cr_state = 0;
}
#endif

#if 1 /*debug port - relative*/
void halbb_bb_dbg_port_clock_en(struct bb_info *bb, u8 enable)
{
	struct bb_dbg_cr_info *cr = &bb->bb_dbg_i.bb_dbg_cr_i;
	u32 reg_value = 0;

	reg_value = enable ? 1 : 0;
	halbb_set_reg(bb, cr->clk_en, cr->clk_en_m, reg_value);
	halbb_set_reg(bb, cr->dbgport_en, cr->dbgport_en_m, reg_value);

}

u32 halbb_get_bb_dbg_port_idx(struct bb_info *bb)
{
	struct bb_dbg_cr_info *cr = &bb->bb_dbg_i.bb_dbg_cr_i;
	u32 val = 0;
	u32 dbg_port, ip;

	ip = halbb_get_reg(bb, cr->dbgport_ip, cr->dbgport_ip_m);
	dbg_port = halbb_get_reg(bb, cr->dbgport_idx, cr->dbgport_idx_m);

	val = (ip << 8) | (dbg_port & 0xff);

	return val;
}

void halbb_set_bb_dbg_port_ip(struct bb_info *bb, enum bb_dbg_port_ip_t ip)
{
	struct bb_dbg_cr_info *cr = &bb->bb_dbg_i.bb_dbg_cr_i;

	halbb_set_reg(bb, cr->dbgport_ip, cr->dbgport_ip_m, ip);
}

void halbb_set_bb_dbg_port(struct bb_info *bb, u32 dbg_port)
{
	struct bb_dbg_cr_info *cr = &bb->bb_dbg_i.bb_dbg_cr_i;

	halbb_set_reg(bb, cr->dbgport_idx, cr->dbgport_idx_m, dbg_port);
}

bool halbb_bb_dbg_port_racing(struct bb_info *bb, u8 curr_dbg_priority)
{
	
	bool dbg_port_result = false;

	if (curr_dbg_priority > bb->pre_dbg_priority) {

		halbb_bb_dbg_port_clock_en(bb, true);

		BB_DBG(bb, DBG_DBG_API,
		       "DbgPort racing success, Cur_priority=((%d)), Pre_priority=((%d))\n",
		       curr_dbg_priority, bb->pre_dbg_priority);

		bb->pre_dbg_priority = curr_dbg_priority;
		dbg_port_result = true;
	}

	return dbg_port_result;
}

void halbb_release_bb_dbg_port(struct bb_info *bb)
{
	halbb_bb_dbg_port_clock_en(bb, false);
	bb->pre_dbg_priority = DBGPORT_RELEASE;
	BB_DBG(bb, DBG_DBG_API, "Release BB dbg_port\n");
}

u32 halbb_get_bb_dbg_port_val(struct bb_info *bb)
{
	struct bb_dbg_cr_info *cr = &bb->bb_dbg_i.bb_dbg_cr_i;
	u32 dbg_port_value = 0;

	dbg_port_value = halbb_get_reg(bb, cr->dbgport_val, cr->dbgport_val_m);
	BB_DBG(bb, DBG_DBG_API, "dbg_port_value = 0x%x\n", dbg_port_value);
	
	return dbg_port_value;
}

void halbb_dbgport_dump_all(struct bb_info *bb, u32 *_used, char *output,
			    u32 *_out_len)
{
	switch (bb->ic_type) {
	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		halbb_dbgport_dump_all_8852a(bb, _used, output, _out_len);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		halbb_dbgport_dump_all_8852a_2(bb, _used, output, _out_len);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		halbb_dbgport_dump_all_8852b(bb, _used, output, _out_len);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		halbb_dbgport_dump_all_8852c(bb, _used, output, _out_len);
		break;
	#endif

	default:
		break;
	}
}

void halbb_dbgport_dbg(struct bb_info *bb, char input[][16], u32 *_used,
			 char *output, u32 *_out_len)
{
	u32 val[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;
	u32 dp = 0; /*debug port value*/
	u8 dbg[32];
	u8 tmp = 0;
	u8 i;

	if (_os_strcmp(input[1], "-h") == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{dbg_port_ip} {dbg_port_idx}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{dump_all}\n");
		return;
	} 

	if (!halbb_bb_dbg_port_racing(bb, DBGPORT_PRI_3)) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "{Racing Fail}\n");
		return;
	}

	if (_os_strcmp(input[1], "dump_all") == 0) {
		halbb_dbgport_dump_all(bb, _used, output, _out_len);
		return;
	} else {

		HALBB_SCAN(input[1], DCMD_HEX, &val[0]);
		HALBB_SCAN(input[2], DCMD_HEX, &val[1]);

		halbb_set_bb_dbg_port_ip(bb, (enum bb_dbg_port_ip_t)val[0]);
		halbb_set_bb_dbg_port(bb, val[1]);
		dp = halbb_get_bb_dbg_port_val(bb);
		halbb_release_bb_dbg_port(bb);

		for (i = 0; i < 32; i++)
			dbg[i] = (u8)((dp & BIT(i)) >> i);

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Dbg Port[0x%02x, 0x%03x] = 0x08%x\n", val[0], val[1], dp);

		for (i = 4; i != 0; i--) {
			tmp = 8 * (i - 1);
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				 "val[%02d:%02d] = 8b'%d %d %d %d %d %d %d %d\n",
				 tmp + 7, tmp, dbg[tmp + 7], dbg[tmp + 6],
				 dbg[tmp + 5], dbg[tmp + 4], dbg[tmp + 3],
				 dbg[tmp + 2], dbg[tmp + 1], dbg[tmp + 0]);
		}
	}
}
#endif

#if HALBB_DBG_DVLP_FLAG /*Common debug message - relative*/
void halbb_rx_rate_distribution_su_cnsl(struct bb_info *bb, u32 *_used,
			      char *output, u32 *_out_len)
{
	struct bb_cmn_rpt_info	*cmn_rpt = &bb->bb_cmn_rpt_i;
	struct bb_pkt_cnt_su_info *pkt_cnt = &cmn_rpt->bb_pkt_cnt_su_i;
	u8 i = 0;
	u8 rate_num = bb->num_rf_path, ss_ofst = 0;

	BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used, "[RxRate Cnt] =============>\n");

	/*@======CCK=========================================================*/

	BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
		  "* CCK = {%d, %d, %d, %d}\n",
		  pkt_cnt->pkt_cnt_legacy[0], pkt_cnt->pkt_cnt_legacy[1],
		  pkt_cnt->pkt_cnt_legacy[2], pkt_cnt->pkt_cnt_legacy[3]);

	/*@======OFDM========================================================*/
	BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
		  "* OFDM = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
		  pkt_cnt->pkt_cnt_legacy[4], pkt_cnt->pkt_cnt_legacy[5],
		  pkt_cnt->pkt_cnt_legacy[6], pkt_cnt->pkt_cnt_legacy[7],
		  pkt_cnt->pkt_cnt_legacy[8], pkt_cnt->pkt_cnt_legacy[9],
		  pkt_cnt->pkt_cnt_legacy[10], pkt_cnt->pkt_cnt_legacy[11]);

	/*@======HT==========================================================*/
	for (i = 0; i < rate_num; i++) {
		ss_ofst = (i << 3);

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			  "* HT MCS[%d :%d ] = {%d, %d, %d, %d, %d, %d, %d, %d}\n",
			  (ss_ofst), (ss_ofst + 7),
			  pkt_cnt->pkt_cnt_ht[ss_ofst + 0],
			  pkt_cnt->pkt_cnt_ht[ss_ofst + 1],
			  pkt_cnt->pkt_cnt_ht[ss_ofst + 2],
			  pkt_cnt->pkt_cnt_ht[ss_ofst + 3],
			  pkt_cnt->pkt_cnt_ht[ss_ofst + 4],
			  pkt_cnt->pkt_cnt_ht[ss_ofst + 5],
			  pkt_cnt->pkt_cnt_ht[ss_ofst + 6],
			  pkt_cnt->pkt_cnt_ht[ss_ofst + 7]);
	}
	

	/*@======VHT==========================================================*/

	for (i = 0; i < rate_num; i++) {
		ss_ofst = 12 * i;

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			  "* VHT-%d ss MCS[0:11] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
			  (i + 1),
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 0],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 1],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 2],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 3],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 4],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 5],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 6],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 7],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 8],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 9],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 10],
			  pkt_cnt->pkt_cnt_vht[ss_ofst + 11]);
	}

	/*@======HE==========================================================*/
	for (i = 0; i < rate_num; i++) {
		ss_ofst = 12 * i;

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			  "* HE-%d ss MCS[0:11] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
			  (i + 1),
			  pkt_cnt->pkt_cnt_he[ss_ofst + 0],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 1],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 2],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 3],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 4],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 5],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 6],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 7],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 8],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 9],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 10],
			  pkt_cnt->pkt_cnt_he[ss_ofst + 11]);
	}
	/*@======SC_BW========================================================*/
	for (i = 0; i < rate_num; i++) {
		ss_ofst = 12 * i;

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			  "*[Low BW 20M] %d-ss MCS[0:11] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
			  (i + 1),
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 0],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 1],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 2],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 3],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 4],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 5],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 6],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 7],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 8],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 9],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 10],
			  pkt_cnt->pkt_cnt_sc20[ss_ofst + 11]);
	}

	for (i = 0; i < rate_num; i++) {
		ss_ofst = 12 * i;

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			  "*[Low BW 40M] %d-ss MCS[0:11] = {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
			  (i + 1),
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 0],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 1],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 2],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 3],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 4],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 5],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 6],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 7],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 8],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 9],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 10],
			  pkt_cnt->pkt_cnt_sc40[ss_ofst + 11]);
	}
}

u16 halbb_rx_utility(struct bb_info *bb, u16 avg_phy_rate, u8 rx_max_ss,
		     enum channel_width bw)
{
	struct bb_cmn_rpt_info	*cmn_rpt = &bb->bb_cmn_rpt_i;
	struct bb_pkt_cnt_su_info *pkt_cnt = &cmn_rpt->bb_pkt_cnt_su_i;
	u16 utility_primitive = 0, utility = 0;

	if (pkt_cnt->he_pkt_not_zero) {
	/*@ HE 1SS MCS11[3.2] 20M: tp = 122, 1000/122 = 8.2, 122*8.25 = 1006.5*/
		utility_primitive = avg_phy_rate * 8 + (avg_phy_rate >> 2);
	} else if (pkt_cnt->vht_pkt_not_zero) {
	/*@ VHT 1SS MCS9(fake) 20M: tp = 87, 1000/87 = 11.49, 87*11.5 = 1000.5*/
		utility_primitive = avg_phy_rate * 11 + (avg_phy_rate >> 1);
	} else if (pkt_cnt->ht_pkt_not_zero) {
	/*@ MCS7 20M: tp = 65, 1000/65 = 15.38, 65*15.5 = 1007*/
		utility_primitive = avg_phy_rate * 15 + (avg_phy_rate >> 1);
	} else {
	/*@ 54M, 1000/54 = 18.5, 54*18.5 = 999*/
		utility_primitive = avg_phy_rate * 18 + (avg_phy_rate >> 1);
	}

	utility = (utility_primitive / rx_max_ss) >> bw;

	if (utility > 1000)
		utility = 1000;

	return utility;
}

u16 halbb_rx_avg_phy_rate(struct bb_info *bb)
{	
	struct bb_cmn_rpt_info	*cmn_rpt = &bb->bb_cmn_rpt_i;
	struct bb_pkt_cnt_su_info *pkt_cnt = &cmn_rpt->bb_pkt_cnt_su_i;
	u16 i = 0;
	u8 base = LEGACY_RATE_NUM;
	u16 rate = 0;
	u32 pkt_cnt_tmp = 0, phy_rate_sum = 0;
	enum channel_width bw = bb->hal_com->band[0].cur_chandef.bw;

	//BB_DBG(bb, DBG_CMN, "bw=%d\n", bb->hal_com->band[0].cur_chandef.bw);

	if (pkt_cnt->he_pkt_not_zero) {
	/*HE Mode*/
		for (i = 0; i < HE_RATE_NUM; i++) {
			if (pkt_cnt->pkt_cnt_he[i] == 0)
				continue;
			rate = VHT_2_HE32_RATE(bb_phy_rate_table[i + base] << bw);
			phy_rate_sum += pkt_cnt->pkt_cnt_he[i] * rate;
			pkt_cnt_tmp += pkt_cnt->pkt_cnt_he[i];

			//BB_DBG(bb, DBG_CMN, "HE  sum:%d +={%d * %d} idx=%d cnt=%d\n", phy_rate_sum, pkt_cnt->pkt_cnt_he[i], rate, i + base, pkt_cnt_tmp);
		}
	} else if (pkt_cnt->vht_pkt_not_zero) {
	/*VHT Mode*/
		for (i = 0; i < VHT_RATE_NUM; i++) {
			if (pkt_cnt->pkt_cnt_vht[i] == 0)
				continue;
			rate = bb_phy_rate_table[i + base] << bw;
			phy_rate_sum += pkt_cnt->pkt_cnt_vht[i] * rate;
			pkt_cnt_tmp += pkt_cnt->pkt_cnt_vht[i];

			//BB_DBG(bb, DBG_CMN, "VHT  sum:%d +={%d * %d} idx=%d cnt=%d\n", phy_rate_sum, pkt_cnt->pkt_cnt_vht[i], rate, i + base, pkt_cnt_tmp);
		}
		
	} else if (pkt_cnt->ht_pkt_not_zero) {
	/*HT Mode*/
		for (i = 0; i < HT_RATE_NUM; i++) {
			if (pkt_cnt->pkt_cnt_ht[i] == 0)
				continue;
			rate = bb_phy_rate_table[i + base] << bw;
			phy_rate_sum += pkt_cnt->pkt_cnt_ht[i] * rate;
			pkt_cnt_tmp += pkt_cnt->pkt_cnt_ht[i];

			//BB_DBG(bb, DBG_CMN, "HT  sum:%d +={%d * %d} idx=%d cnt=%d\n", phy_rate_sum, pkt_cnt->pkt_cnt_ht[i], rate, i + base, pkt_cnt_tmp);
		}

	} else {
	/*Legacy mode*/
		for (i = BB_01M; i <= BB_54M; i++) {
			/*SKIP beacon*/
			if (i == cmn_rpt->bb_pkt_cnt_bcn_i.beacon_phy_rate)
				continue;

			if (pkt_cnt->pkt_cnt_legacy[i] == 0)
				continue;

			rate = bb_phy_rate_table[i];
			phy_rate_sum += pkt_cnt->pkt_cnt_legacy[i] * rate;
			pkt_cnt_tmp += pkt_cnt->pkt_cnt_legacy[i];

			//BB_DBG(bb, DBG_CMN, "LAG  sum:%d +={%d * %d} idx=%d cnt=%d\n", phy_rate_sum, pkt_cnt->pkt_cnt_legacy[i], rate, i + base, pkt_cnt_tmp);
		}
	}

	/*SC Data*/
	if (pkt_cnt->sc40_occur) {
		for (i = 0; i < LOW_BW_RATE_NUM; i++) {
			if (pkt_cnt->pkt_cnt_sc40[i] == 0)
				continue;
			rate = bb_phy_rate_table[i + base] << CHANNEL_WIDTH_40;
			phy_rate_sum += pkt_cnt->pkt_cnt_sc40[i] * rate;
			pkt_cnt_tmp += pkt_cnt->pkt_cnt_sc40[i];
		}
	}

	if (pkt_cnt->sc20_occur) {
		for (i = 0; i < LOW_BW_RATE_NUM; i++) {
			if (pkt_cnt->pkt_cnt_sc20[i] == 0)
				continue;
			rate = bb_phy_rate_table[i + base];
			phy_rate_sum += pkt_cnt->pkt_cnt_sc20[i] * rate;
			pkt_cnt_tmp += pkt_cnt->pkt_cnt_sc20[i];
		}
	}
	//BB_DBG(bb, DBG_CMN, "sum=%d, cnt=%d\n", phy_rate_sum, pkt_cnt_tmp);
	return (u16)HALBB_DIV(phy_rate_sum, pkt_cnt_tmp); /*avg_phy_rate*/
}

void halbb_basic_dbg_msg_mac_phy_intf(struct bb_info *bb)
{
	struct bb_dbg_cr_info *cr = &bb->bb_dbg_i.bb_dbg_cr_i;
	s32 pw = 0;
	//u8 i = 0, usr_ofst = 0;
	u32 tmp_32 = 0;
	u32 tx_pw = 0, l_sig = 0, sig_a1 = 0, sig_a2 = 0;
	u8 txpath_en =0, type = 0, mcs = 0;
	char ppdu[][10] = {{"L-CCK"}, {"S-CCK"}, {"Legacy"}, {"HT"},
			   {"HT GF"}, {"VHT SU"}, {"VHT MU"}, {"HE SU"},
			   {"HE ER SU"}, {"HE MU"}, {"HE TB"}};
	char gi_type[][4] = {{"0.4"}, {"0.8"}, {"1.6"}, {"3.2"}};
	char fec_type[][5] = {{"BCC"}, {"LDPC"}};
	char *txcmd = NULL;
	u8 txcmd2 = 0;
	bool no_txcmd_hit = false;

	if (bb->bb_watchdog_mode != BB_WATCHDOG_NORMAL)
		return;

	txpath_en = (u8)halbb_get_reg(bb, cr->mac_phy_txpath_en, 0xF000);
	l_sig = halbb_get_reg(bb, cr->mac_phy_lsig, MASKDWORD);
	tx_pw = halbb_get_reg(bb, cr->mac_phy_tx_pw, 0x7FC0000);

	if ((txpath_en == 0) && (l_sig == 0) && (tx_pw == 0)) {
		BB_DBG(bb, DBG_CMN,
		       "[MAC/PHY Intf]Txinfo is empty!BB reset has been probably toggled.\n");
		return;
	}

	type = (u8)halbb_get_reg(bb, cr->mac_phy_ppdu_type, 0xf);
	mcs = (u8)(halbb_get_reg(bb, cr->mac_phy_mcs_3_0, 0xf0000000) +
	      (halbb_get_reg(bb, cr->mac_phy_mcs_5_4, 0x3) << 4));

	halbb_print_sign_frac_digit(bb, tx_pw, 9, 2, bb->dbg_buf, HALBB_SNPRINT_SIZE);

	tmp_32 = halbb_get_reg(bb, cr->mac_phy_txcmd, 0x3f000000);
	switch (tmp_32) {
	case 0:
		txcmd = "data";
		break;
	case 1:
		txcmd = "beacon";
		break;
	case 2:
		txcmd = "HT-NDPA";
		break;
	case 3:
		txcmd = "VHT-NDPA";
		break;
	case 4:
		txcmd = "HE-NDPA";
		break;
	case 8:
		txcmd = "RTS";
		break;
	case 9:
		txcmd = "CTS2self";
		break;
	case 10:
		txcmd = "CF_end";
		break;
	case 11:
		txcmd = "compressed-BAR";
		break;
	case 12:
		txcmd = "BFRP";
		break;
	case 13:
		txcmd = "NDP";
		break;
	case 14:
		txcmd = "QoS_Null";
		break;
	case 16:
		txcmd = "ACK";
		break;
	case 17:
		txcmd = "CTS";
		break;
	case 18:
		txcmd = "compressed-BA";
		break;
	case 19:
		txcmd = "Multi-STA-BA";
		break;
	case 20:
		txcmd = "HT-CSI";
		break;
	case 21:
		txcmd = "VHT-CSI";
		break;
	case 22:
		txcmd = "HE-CSI";
		break;
	case 31:
		txcmd = "TB_PPDU";
		break;
	case 32:
		txcmd = "TRIG-BASIC";
		break;
	case 33:
		txcmd = "TRIG-BFRP";
		break;
	case 34:
		txcmd = "TRIG-MUBAR";
		break;
	case 35:
		txcmd = "TRIG-MU-RTS";
		break;
	case 36:
		txcmd = "TRIG-BSRP";
		break;
	case 37:
		txcmd = "TRIG-BQRP";
		break;
	case 38:
		txcmd = "TRIG-NFRP";
		break;
	case 48:
		txcmd = "TRIG-BASIC-DATA";
		break;
	default:
		txcmd = "RSVD";
		txcmd2 = (u8)tmp_32;
		no_txcmd_hit = true;
		break;
	}

	if (no_txcmd_hit)
		BB_DBG(bb, DBG_CMN,
		       "[MAC/PHY Intf][%s][RSVD-%d] BW=%dM, TxSC=%d, TxPw=%s dBm, TxPathEn=%d\n",
		       ppdu[type], txcmd2,
		       20 << (halbb_get_reg(bb, cr->mac_phy_bw, 0x30000)),
		       halbb_get_reg(bb, cr->mac_phy_txsc, 0xf0),
		       bb->dbg_buf, txpath_en);
	else
		BB_DBG(bb, DBG_CMN,
		       "[MAC/PHY Intf][%s][%s] BW=%dM, TxSC=%d, TxPw=%s dBm, TxPathEn=%d\n",
		       ppdu[type], txcmd, 
		       20 << (halbb_get_reg(bb, cr->mac_phy_bw, 0x30000)),
		       halbb_get_reg(bb, cr->mac_phy_txsc, 0xf0),
		       bb->dbg_buf, txpath_en);

	BB_DBG(bb, DBG_CMN,
	       "User_num=%d, STBC=%d, FEC=%s, GILTF=%dx%s, NDP_en=%d, N_sts=%d, MCS=%d\n",
	       halbb_get_reg(bb, cr->mac_phy_n_usr, 0xff0),
	       halbb_get_reg(bb, cr->mac_phy_stbc, BIT(0)),
	       fec_type[halbb_get_reg(bb, cr->mac_phy_fec, BIT(27))],
	       1 << halbb_get_reg(bb, cr->mac_phy_ltf, 0xC0),
	       gi_type[halbb_get_reg(bb, cr->mac_phy_gi, 0x30)],
	       halbb_get_reg(bb, cr->mac_phy_ndp_en, BIT(4)),
	       halbb_get_reg(bb, cr->mac_phy_n_sts, 0x7000000), mcs);

	/*SIG*/
	tmp_32 = halbb_get_reg(bb, cr->mac_phy_siga_0, MASKDWORD);
	if (type > 6) { // === HE === //
		sig_a1 = tmp_32 & 0x3ffffff;
		sig_a2 = (halbb_get_reg(bb, cr->mac_phy_siga_1, 0xfffff) << 6) |
			 ((tmp_32 & 0xfc000000) >> 26);
	} else if (type > 2) { // === HT / VHT === //
		sig_a1 = tmp_32 & 0xffffff;
		sig_a2 = (halbb_get_reg(bb, cr->mac_phy_siga_1, 0xffff) << 8) |
			 ((tmp_32 & 0xff000000) >> 24);
	}
	BB_DBG(bb, DBG_CMN, "SIG-L/A1/A2= {0x%08x, 0x%08x, 0x%08x}\n", l_sig, sig_a1,
	       sig_a2);
#if 0
	BB_DBG(bb, DBG_CMN, "============ [User-specified Info] ============\n");
	for (i = 0; i < n_user; i++) {
		BB_DBG(bb, DBG_CMN, "------------- [User-%d] -------------\n", i);
		usr_ofst = i << 3;
		/* FEC */
		txinfo_value = halbb_get_reg(bb, 0x4718 + usr_ofst, BIT(27));
		BB_DBG(bb, DBG_CMN, "FEC: %s\n", fec_type[txinfo_value]);
		/* MCS */
		txinfo_value = (halbb_get_reg(bb, 0x471c + usr_ofst, BIT(1) |
				BIT(0)) << 4) + halbb_get_reg(bb, 0x4718 +
							       usr_ofst,
							       0xf0000000);
		BB_DBG(bb, DBG_CMN, "MCS: %d\n", txinfo_value);
		/* DCM */
		txinfo_value = halbb_get_reg(bb, 0x471c + usr_ofst, BIT(2));
		BB_DBG(bb, DBG_CMN, "DCM En: %d\n", txinfo_value);
		/* TxBF */
		txinfo_value = halbb_get_reg(bb, 0x471c + usr_ofst, BIT(14));
		BB_DBG(bb, DBG_CMN, "TxBF En: %d\n", txinfo_value);
	}
#endif

}

void halbb_basic_dbg_msg_pmac(struct bb_info *bb)
{

#ifdef HALBB_STATISTICS_SUPPORT
	struct bb_stat_info *stat = &bb->bb_stat_i;
	struct bb_fa_info *fa = &stat->bb_fa_i;
	struct bb_cck_fa_info *cck_fa = &fa->bb_cck_fa_i;
	struct bb_legacy_fa_info *legacy_fa = &fa->bb_legacy_fa_i;
	struct bb_ht_fa_info *ht_fa = &fa->bb_ht_fa_i;
	struct bb_vht_fa_info *vht_fa = &fa->bb_vht_fa_i;
	struct bb_he_fa_info *he_fa = &fa->bb_he_fa_i;
	struct bb_cca_info *cca = &stat->bb_cca_i;
	struct bb_crc_info *crc = &stat->bb_crc_i;
	//struct bb_crc2_info *crc2 = &stat_t->bb_crc2_i;

	if (bb->bb_watchdog_mode != BB_WATCHDOG_NORMAL)
		return;

	BB_DBG(bb, DBG_CMN,
	       "[Tx]{CCK_TxEN, CCK_TxON, OFDM_TxEN, OFDM_TxON}: {%d, %d, %d, %d}\n",
	       stat->bb_tx_cnt_i.cck_mac_txen, stat->bb_tx_cnt_i.cck_phy_txon,
	       stat->bb_tx_cnt_i.ofdm_mac_txen,
	       stat->bb_tx_cnt_i.ofdm_phy_txon);
	BB_DBG(bb, DBG_CMN,
	       "[CRC]{B/G/N/AC/AX/All/MPDU} OK:{%d, %d, %d, %d, %d, %d, %d} Err:{%d, %d, %d, %d, %d, %d, %d}\n",
	       crc->cnt_cck_crc32_ok, crc->cnt_ofdm_crc32_ok,
	       crc->cnt_ht_crc32_ok, crc->cnt_vht_crc32_ok,
	       crc->cnt_he_crc32_ok, crc->cnt_crc32_ok_all,
	       crc->cnt_ampdu_crc_ok, crc->cnt_cck_crc32_error,
	       crc->cnt_ofdm_crc32_error, crc->cnt_ht_crc32_error,
	       crc->cnt_vht_crc32_error, crc->cnt_he_crc32_error,
	       crc->cnt_crc32_error_all, crc->cnt_ampdu_crc_error);
	BB_DBG(bb, DBG_CMN,
	       "[CCA]{CCK, OFDM, All}: %d, %d, %d\n",
	       cca->cnt_cck_cca, cca->cnt_ofdm_cca, cca->cnt_cca_all);
	BB_DBG(bb, DBG_CMN,
	       "[FA]{CCK, OFDM, All}: %d, %d, %d\n",
	       fa->cnt_cck_fail, fa->cnt_ofdm_fail, fa->cnt_fail_all);
	BB_DBG(bb, DBG_CMN,
	       " *[CCK]sfd/sig_GG=%d/%d, *[OFDM]Prty=%d, Rate=%d, LSIG_brk_s/l=%d/%d, SBD=%d\n",
	       cck_fa->sfd_gg_cnt, cck_fa->sig_gg_cnt,
	       legacy_fa->cnt_parity_fail, legacy_fa->cnt_rate_illegal,
	       legacy_fa->cnt_lsig_brk_s_th, legacy_fa->cnt_lsig_brk_l_th,
	       legacy_fa->cnt_sb_search_fail);
	BB_DBG(bb, DBG_CMN,
	       " *[HT]CRC8=%d, MCS=%d, *[VHT]SIGA_CRC8=%d, MCS=%d\n",
	       ht_fa->cnt_crc8_fail, ht_fa->cnt_mcs_fail,
	       vht_fa->cnt_crc8_fail_vhta, vht_fa->cnt_mcs_fail_vht);
	BB_DBG(bb, DBG_CMN,
	       " *[HE]SIGA_CRC4{SU/ERSU/MU}=%d/%d/%d, SIGB_CRC4{ch1/ch2}=%d/%d, MCS{nrml/bcc/dcm}=%d/%d/%d\n",
	       he_fa->cnt_crc4_fail_hea_su, he_fa->cnt_crc4_fail_hea_ersu,
	       he_fa->cnt_crc4_fail_hea_mu, he_fa->cnt_crc4_fail_heb_ch1_mu,
	       he_fa->cnt_crc4_fail_heb_ch2_mu, he_fa->cnt_mcs_fail_he,
	       he_fa->cnt_mcs_fail_he_bcc, he_fa->cnt_mcs_fail_he_dcm);
#endif
}

void halbb_basic_dbg_msg_rx_info(struct bb_info *bb)
{
	struct bb_ch_info *ch = &bb->bb_ch_i;
#ifdef HALBB_CFO_TRK_SUPPORT
	struct bb_cfo_trk_info *cfo_trk = &bb->bb_cfo_trk_i;
#endif
	struct bb_cmn_rpt_info	*cmn_rpt = &bb->bb_cmn_rpt_i;
	struct bb_pkt_cnt_cap_info *pkt_cnt_cap = &cmn_rpt->bb_pkt_cnt_all_i;
	struct bb_physts_pop_info *pop_info = &cmn_rpt->bb_physts_pop_i;
	struct bb_dbg_cr_info *cr = &bb->bb_dbg_i.bb_dbg_cr_i;
	u8 tmp = 0;
	u32 bb_monitor1 = 0;

	if (bb->bb_watchdog_mode != BB_WATCHDOG_NORMAL)
		return;	

	BB_DBG(bb, DBG_CMN, "rxsc_idx {Lgcy, 20, 40, 80} = {%d, %d, %d, %d}\n",
	       ch->rxsc_l, ch->rxsc_20, ch->rxsc_40, ch->rxsc_80);
	BB_DBG(bb, DBG_CMN, "RX Pkt Cnt: LDPC=(%d), BCC=(%d), STBC=(%d), SU_BF=(%d), MU_BF=(%d), \n",
	       pkt_cnt_cap->pkt_cnt_ldpc, pkt_cnt_cap->pkt_cnt_bcc,
	       pkt_cnt_cap->pkt_cnt_stbc, pkt_cnt_cap->pkt_cnt_subf,
	       pkt_cnt_cap->pkt_cnt_mubf);
#ifdef HALBB_CFO_TRK_SUPPORT
	halbb_print_sign_frac_digit(bb, cfo_trk->cfo_avg_pre, 16, 2, bb->dbg_buf, HALBB_SNPRINT_SIZE);

	BB_DBG(bb, DBG_CMN, "CFO[T-1]=(%s kHz), cryst_cap=(%s%d), cfo_ofst=%d\n",
		  bb->dbg_buf,
		  ((cfo_trk->crystal_cap > cfo_trk->def_x_cap) ? "+" : "-"),
		  DIFF_2(cfo_trk->crystal_cap, cfo_trk->def_x_cap),
		  cfo_trk->x_cap_ofst);
#endif
	BB_DBG(bb, DBG_CMN, "Dly_sprd=(%d)\n", tmp);
	BB_DBG(bb, DBG_CMN,
	       "[POP] cnt=%d, hist_cck/ofdm[0:3]={%d | %d, %d, %d}/{%d | %d, %d, %d}\n",
	       bb->bb_stat_i.bb_cca_i.pop_cnt,
	       pop_info->pop_hist_cck[0], pop_info->pop_hist_cck[1],
	       pop_info->pop_hist_cck[2], pop_info->pop_hist_cck[3],
	       pop_info->pop_hist_ofdm[0], pop_info->pop_hist_ofdm[1],
	       pop_info->pop_hist_ofdm[2], pop_info->pop_hist_ofdm[3]);

	halbb_set_reg(bb, cr->bb_monitor_sel1, cr->bb_monitor_sel1_m, 1);
	bb_monitor1 = halbb_get_reg(bb, cr->bb_monitor1, cr->bb_monitor1_m);
	BB_DBG(bb, DBG_CMN, "BB monitor1 = (0x%x)\n", bb_monitor1);
}

void halbb_basic_dbg_msg_tx_info(struct bb_info *bb)
{
	struct bb_ch_info *ch = &bb->bb_ch_i;
	struct rtw_phl_stainfo_t *sta;
	struct rtw_ra_sta_info	*ra;
	//char dbg_buf[HALBB_SNPRINT_SIZE] = {0};
	u16 sta_cnt = 0;
	u8 i = 0;
	u8 tmp = 0;
	u16 curr_tx_rt = 0;
	enum rtw_gi_ltf curr_gi_ltf = RTW_GILTF_LGI_4XHE32;
	enum hal_rate_bw curr_bw = HAL_RATE_BW_20;

	for (i = 0; i < PHL_MAX_STA_NUM; i++) {
		if (!bb->sta_exist[i])
			continue;
		sta = bb->phl_sta_info[i];
		if (!is_sta_active(sta))
			continue;

		ra = &sta->hal_sta->ra_info;
		curr_tx_rt = (u16)(ra->rpt_rt_i.mcs_ss_idx) | ((u16)(ra->rpt_rt_i.mode) << 7);
		curr_gi_ltf = ra->rpt_rt_i.gi_ltf;
		curr_bw = ra->rpt_rt_i.bw;

		halbb_print_rate_2_buff(bb, curr_tx_rt, curr_gi_ltf, bb->dbg_buf, HALBB_SNPRINT_SIZE);
		BB_DBG(bb, DBG_CMN, "TxRate[%d]=%s (0x%x-%d), PER=(%d), TXBW=(%d)\n",
		       i, bb->dbg_buf, curr_tx_rt, curr_gi_ltf,
		       ra->curr_retry_ratio, (20<<curr_bw));
		sta_cnt++;
		if (sta_cnt >= bb->hal_com->assoc_sta_cnt)
			break;
	}
	//BB_DBG(bb, DBG_CMN, "TSSI val=(%d)\n", tmp);
	//BB_DBG(bb, DBG_CMN, "EDCA val=(%d)\n", tmp);
}

void halbb_basic_dbg_msg_physts_mu(struct bb_info *bb)
{
	struct bb_ch_info *ch = &bb->bb_ch_i;
	struct bb_link_info *link = &bb->bb_link_i;

	if (bb->bb_cmn_rpt_i.bb_pkt_cnt_mu_i.pkt_cnt_all == 0) {
	    BB_DBG(bb, DBG_CMN, "NO MU pkt\n");
	    return;
	}

	/*RX Rate*/
	halbb_print_rate_2_buff(bb, link->rx_rate_plurality_mu,
				RTW_GILTF_LGI_4XHE32, bb->dbg_buf, 32);

	BB_DBG(bb, DBG_CMN, "Plurality_RxRate:%s (0x%x)\n",
	       bb->dbg_buf, link->rx_rate_plurality);

	/*RX Rate Distribution & RSSI*/
	halbb_show_rssi_and_rate_distribution_mu(bb);
}

void halbb_basic_dbg_msg_physts_su(struct bb_info *bb)
{
	struct bb_ch_info *ch = &bb->bb_ch_i;
	struct bb_link_info *link = &bb->bb_link_i;
	struct bb_cmn_rpt_info	*cmn_rpt = &bb->bb_cmn_rpt_i;
	//struct rtw_phl_stainfo_t *sta;
	//char dbg_buf[HALBB_SNPRINT_SIZE] = {0};
	char dbg_buf2[32] = {0};
	u16 avg_phy_rate = 0, utility = 0;

	/*RX Rate*/
	halbb_print_rate_2_buff(bb, link->rx_rate_plurality,
				RTW_GILTF_LGI_4XHE32, dbg_buf2, 32);

	halbb_print_rate_2_buff(bb, cmn_rpt->bb_pkt_cnt_bcn_i.beacon_phy_rate,
				RTW_GILTF_LGI_4XHE32, bb->dbg_buf, HALBB_SNPRINT_SIZE);
	BB_DBG(bb, DBG_CMN, "Plurality_RxRate:%s (0x%x), Bcn_Rate=%s (0x%x), Bcn_cnt=%d\n",
	       dbg_buf2, link->rx_rate_plurality,
	       bb->dbg_buf ,cmn_rpt->bb_pkt_cnt_bcn_i.beacon_phy_rate,
	       cmn_rpt->bb_pkt_cnt_bcn_i.pkt_cnt_beacon);

	/*RX Rate Distribution & RSSI*/
	halbb_show_rssi_and_rate_distribution_su(bb);

	/*RX Utility*/
	avg_phy_rate = halbb_rx_avg_phy_rate(bb);
	utility = halbb_rx_utility(bb, avg_phy_rate, bb->num_rf_path, bb->hal_com->band[0].cur_chandef.bw);

	BB_DBG(bb, DBG_CMN, "Avg_rx_rate = %d, rx_utility=( %d / 1000 )\n",
		  avg_phy_rate, utility);
}

void halbb_basic_dbg_message(struct bb_info *bb)
{
	struct bb_link_info	*link = &bb->bb_link_i;
	struct bb_ch_info	*ch = &bb->bb_ch_i;
	struct bb_dbg_info	*dbg = &bb->bb_dbg_i;
	struct bb_physts_info	*physts = &bb->bb_physts_i;
	enum channel_width bw = bb->hal_com->band[0].cur_chandef.bw;
	u8 fc = bb->hal_com->band[0].cur_chandef.center_ch;
	u8 sta_cnt = 0;
	u8 i;

#ifdef HALBB_DBG_TRACE_SUPPORT
	if (!(bb->dbg_component & DBG_CMN))
		return;

	if (bb->cmn_dbg_msg_cnt >= bb->cmn_dbg_msg_period) { /*unit: Sec*/
		bb->cmn_dbg_msg_cnt = HALBB_WATCHDOG_PERIOD;
	} else {
		bb->cmn_dbg_msg_cnt += HALBB_WATCHDOG_PERIOD;
		return;
	}
#endif

	BB_DBG(bb, DBG_CMN, "[%s]%s %s\n", __func__, HLABB_CODE_BASE, HALBB_RELEASE_DATE);
	BB_DBG(bb, DBG_CMN, "====[1. System] (%08d sec) (Ability=0x%08llx)\n",
	       bb->bb_sys_up_time, bb->support_ability);
	BB_DBG(bb, DBG_CMN, "[%s mode], TP{T,R,ALL}={%d, %d, %d}, BW:%d, CH_fc:%d\n",
	       ((bb->bb_watchdog_mode == BB_WATCHDOG_NORMAL) ? "Normal" :
	       ((bb->bb_watchdog_mode == BB_WATCHDOG_LOW_IO) ? "LowIO" : "NonIO")),
	       link->tx_tp, link->rx_tp, link->total_tp, 20 << bw, fc);
	BB_DBG(bb, DBG_CMN,
	       "Phy:%d, linked: %d, Num_sta: %d, rssi_max/min= {%02d.%d, %02d.%d}, Noisy:%d\n",
	       bb->bb_phy_idx,
	       link->is_linked, bb->hal_com->assoc_sta_cnt,
	       ch->rssi_max >> 1, (ch->rssi_max & 1) * 5,
	       ch->rssi_min >> 1, (ch->rssi_min & 1) * 5,
	       ch->is_noisy);
	BB_DBG(bb, DBG_CMN, "physts_cnt{all, 2_self, err_len, ok_ie, err_ie}={%d,%d,%d,%d,%d}\n",
	       physts->bb_physts_cnt_i.all_cnt, physts->bb_physts_cnt_i.is_2_self_cnt,
	       physts->bb_physts_cnt_i.ok_ie_cnt, physts->bb_physts_cnt_i.err_ie_cnt,
	       physts->bb_physts_cnt_i.err_len_cnt);

	for (i = 0; i< PHL_MAX_STA_NUM; i++) {
		BB_DBG(bb, DBG_CMN, "[%d] Linked macid=%d\n",
		       i, bb->sta_exist[i]);

		sta_cnt++;
		if (sta_cnt >= bb->hal_com->assoc_sta_cnt)
			break;
	}
	BB_DBG(bb, DBG_CMN, "\n");
	BB_DBG(bb, DBG_CMN, "====[2. ENV Mntr]\n");
	halbb_env_mntr_log(bb, DBG_CMN);
	BB_DBG(bb, DBG_CMN, "\n");
	BB_DBG(bb, DBG_CMN, "====[3. PMAC]\n");
	halbb_basic_dbg_msg_pmac(bb);
	BB_DBG(bb, DBG_CMN, "\n");
	BB_DBG(bb, DBG_CMN, "====[4. TX General]\n");
	halbb_basic_dbg_msg_mac_phy_intf(bb);

	if (bb->bb_link_i.is_linked) {
		halbb_basic_dbg_msg_tx_info(bb);
		BB_DBG(bb, DBG_CMN, "\n");
		BB_DBG(bb, DBG_CMN, "====[5. RX General]\n");
		halbb_basic_dbg_msg_rx_info(bb);
		BB_DBG(bb, DBG_CMN, "\n");
		BB_DBG(bb, DBG_CMN, "====[6. AVG RSSI/RxRate]\n");
		halbb_basic_dbg_msg_physts_su(bb);
		BB_DBG(bb, DBG_CMN, "\n");
		BB_DBG(bb, DBG_CMN, "====[7. BB Hist]\n");
		halbb_show_phy_hitogram_su(bb);
		BB_DBG(bb, DBG_CMN, "\n");
		BB_DBG(bb, DBG_CMN, "====[8. [MU] AVG RSSI/RxRate]\n");
		halbb_basic_dbg_msg_physts_mu(bb);
	}
	BB_DBG(bb, DBG_CMN, "============================================\n");
	BB_DBG(bb, DBG_CMN, "\n");
}

void halbb_dm_summary(struct bb_info *bb, u8 macid)
{
}

#endif
void halbb_basic_profile_dbg(struct bb_info *bb, u32 *_used, char *output, u32 *_out_len)
{
	char *cv = NULL;
	char *ic_type = NULL;
	char *support = NULL;
	u32 used = *_used;
	u32 out_len = *_out_len;
	u32 date = 0;
	u32 release_ver = 0;

	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "%s\n",
		 "% [Basic Info] %");

	switch (bb->ic_type) {
	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		ic_type = "RTL8852A(Acut)";
		date = BB_REG_RELEASE_DATE_8852A;
		release_ver = BB_REG_RELEASE_VERSION_8852A;
		break;
	#endif
	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		ic_type = "RTL8852A(>Bcut)";
		date = BB_REG_RELEASE_DATE_8852A_2;
		release_ver = BB_REG_RELEASE_VERSION_8852A_2;
		break;
	#endif
	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		ic_type = "RTL8852B";
		date = BB_REG_RELEASE_DATE_8852B;
		release_ver = BB_REG_RELEASE_VERSION_8852B;
		break;
	#endif
	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		ic_type = "RTL8852C";
		date = BB_REG_RELEASE_DATE_8852C;
		release_ver = BB_REG_RELEASE_VERSION_8852C;
		break;
	#endif
	default:
		BB_WARNING("[%s]\n", __func__);
		break;
	}
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		 "IC", ic_type);
	BB_DBG_CNSL(out_len, used, output + used, out_len - used,
		 "  %-25s: %s \n", "Normal Mode",
		 (bb->phl_com->drv_mode == RTW_DRV_MODE_NORMAL)? "Y" : "N");

	if (bb->hal_com->cv == CAV)
		cv = "CAV";
	else if (bb->hal_com->cv == CBV)
		cv = "CBV";
	else if (bb->hal_com->cv == CCV)
		cv = "CCV";
	else if (bb->hal_com->cv == CDV)
		cv = "CDV";
	else if (bb->hal_com->cv == CEV)
		cv = "CEV";
	else
		cv = "NA";

	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %d\n",
		 "RFE", bb->phl_com->dev_cap.rfe_type);
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %d\n",
		 "PKG", bb->phl_com->dev_cap.pkg_type);
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		 "CV", cv);
	BB_DBG_CNSL(out_len, used, output + used, out_len - used,
		 "  %-25s: %d.%d\n", "FW Ver", bb->u8_dummy,
		 bb->u8_dummy); /*TBD*/

	/*[HALBB Info]*/
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "%s\n",
		 "% [HALBB Info] %");

	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s (%s)\n",
		 "Branch", HLABB_CODE_BASE, HALBB_RELEASE_DATE);
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %02d (%d)\n",
		 "BB CR Ver", release_ver, date);
	/*Feature Compile List*/
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "%s\n",
		 "% [Support List] %");

	#ifdef HALBB_DBG_TRACE_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "DBG_TRACE", support);

	#ifdef HALBB_TIMER_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "TIMER", support);

	#ifdef HALBB_PHYSTS_PARSING_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "PHYSTS", support);
	#ifdef HALBB_ENV_MNTR_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "ENV_MNTR", support);
	#ifdef HALBB_STATISTICS_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "STATISTICS", support);
	#ifdef HALBB_RA_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "RA", support);
	#ifdef HALBB_EDCCA_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "EDCCA", support);
	#ifdef HALBB_CFO_TRK_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "CFO_TRK", support);
	#ifdef HALBB_LA_MODE_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "LA_MODE", support);
	#ifdef HALBB_PSD_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "PSD", support);
	#ifdef HALBB_PWR_CTRL_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "PWR_CTRL", support);
	#ifdef HALBB_RUA_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "RUA", support);
	#ifdef HALBB_PMAC_TX_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "PMAC_TX", support);
	#ifdef HALBB_CH_INFO_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "CH_INFO", support);
	#ifdef HALBB_AUTO_DBG_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "AUTO_DBG", support);
	#ifdef HALBB_ANT_DIV_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "ANT_DIV", support);
	#ifdef HALBB_DBCC_SUPPORT
	support = "Y";
	#else
	support = ".";
	#endif
	BB_DBG_CNSL(out_len, used, output + used, out_len - used, "  %-25s: %s\n",
		    "DBCC", support);

	*_used = used;
	*_out_len = out_len;
}

#if HALBB_DBG_DVLP_FLAG /*Dump register - relative*/

void halbb_cr_table_dump(struct bb_info *bb, u32 *cr_table, u32 cr_len)
{
	u8 i = 0;
	u32 cr_tmp, val;

	for (i = 0; i < cr_len; i++) {
		cr_tmp = cr_table[i];
		if (cr_tmp == 0)
			continue;

		val = halbb_get_reg(bb, cr_tmp, MASKDWORD);
		BB_TRACE("[%03d]Reg[0x%04x] = 0x%08x\n", i, cr_tmp, val);
	}
}

void halbb_dump_bb_reg(struct bb_info *bb, u32 *_used, char *output,
			       u32 *_out_len, bool dump_2_buff)
{
	if (dump_2_buff) {
		if (*_out_len < 100) {
			BB_WARNING("[%s] out_len=%d", __func__, *_out_len);
			return;
		}
	}

	switch (bb->ic_type) {
	#ifdef BB_8852A_CAV_SUPPORT
	case BB_RTL8852AA:
		halbb_dump_bb_reg_8852a(bb, _used, output, _out_len);
		break;
	#endif

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		halbb_dump_bb_reg_8852a_2(bb, _used, output, _out_len, dump_2_buff);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		halbb_dump_bb_reg_8852b(bb, _used, output, _out_len, dump_2_buff);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		halbb_dump_bb_reg_8852c(bb, _used, output, _out_len, dump_2_buff);
		break;
	#endif

	default:
		break;
	}
}

void halbb_dump_reg_dbg(struct bb_info *bb, char input[][16], u32 *_used, char *output,
		    u32 *_out_len)
{
	char help[] = "-h";
	u32 val[10] = {0};
	u32 addr = 0;

	if (input[1])
		HALBB_SCAN(input[1], DCMD_DECIMAL, &val[0]);

	if (_os_strcmp(input[1], help) == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			"dumpreg 0\n");
	} else {
		halbb_dump_bb_reg(bb, _used, output, _out_len, true);
	}
}

void halbb_dd_dump_dbg(struct bb_info *bb, char input[][16], u32 *_used,
		       char *output, u32 *_out_len)
{
	char help[] = "-h";
	u32 val[10] = {0};
	u32 used = *_used;
	u32 out_len = *_out_len;

	HALBB_SCAN(input[1], DCMD_DECIMAL, &val[0]);

	if (_os_strcmp(input[1], help) == 0) {
		BB_DBG_CNSL(out_len, *_used, output + *_used, out_len - *_used,
			    "{dd_dbg}\n");
		return;
	}
	/*[Reg]*/
	halbb_dump_bb_reg(bb, &used, output, &out_len, true);
	/*[Dbg Port]*/
	halbb_dbgport_dump_all(bb, _used, output, _out_len);
	/*[Analog Parameters]*/
	//halbb_get_anapar_table(bb, &used, output, &out_len);

}
#endif

void halbb_show_rx_rate(struct bb_info *bb, char input[][16], u32 *_used,
			      char *output, u32 *_out_len)
{
	u32 val[10] = {0};

	HALBB_SCAN(input[2], DCMD_HEX, &val[0]);

	if (val[0] == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[SU RX Rate]\n");
		halbb_rx_rate_distribution_su_cnsl(bb, _used, output, _out_len);
	} else {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[MU RX Rate]\n");
	}
}

void halbb_cmn_dbg(struct bb_info *bb, char input[][16], u32 *_used,
			char *output, u32 *_out_len)
{
	u32 val[10] = {0};
	u32 cr = 0;
	bool rpt = true;
	enum phl_phy_idx phy_idx;
	struct rtw_para_info_t *reg = NULL;

	if (_os_strcmp(input[1], "-h") == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{cr_rec, cr_rec_rf} {en}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "wd {0:Normal/1:LowIo/2:NonIO}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "event {phl_evt_id}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "period {val}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "init {cr, gain} {phy_idx}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "init dbg_mode {en} {rfe} {cv}\n");

	} else if (_os_strcmp(input[1], "event") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		halbb_wifi_event_notify(bb, (enum phl_msg_evt_id)val[0], bb->bb_phy_idx);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "phl_evt_id=%d\n", val[0]);
	} else if (_os_strcmp(input[1], "period") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		bb->bb_watchdog_period = (u8)(val[0] & 0xfe);
		if (bb->bb_watchdog_period < 2)
			bb->bb_watchdog_period = 2;
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "wd_period=%d\n", bb->bb_watchdog_period);
	} else if (_os_strcmp(input[1], "cr_rec") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		bb->bb_dbg_i.cr_recorder_en = (bool)val[0];
	} else if (_os_strcmp(input[1], "cr_rec_rf") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		bb->bb_dbg_i.cr_recorder_rf_en = (bool)val[0];
	} else if (_os_strcmp(input[1], "wd") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		halbb_watchdog(bb, (enum bb_watchdog_mode_t)val[0], bb->bb_phy_idx);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "Watchdog trigger, mode=%d\n", val[0]);
	} else if (_os_strcmp(input[1], "init") == 0) {
		HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);
		HALBB_SCAN(input[4], DCMD_DECIMAL, &val[1]);
		HALBB_SCAN(input[5], DCMD_DECIMAL, &val[2]);

		if (_os_strcmp(input[2], "cr") == 0) {
			phy_idx = (enum phl_phy_idx)val[0];
			reg = &bb->phl_com->phy_sw_cap[phy_idx].bb_phy_reg_info;
			rpt = halbb_init_cr_default(bb, false, 0, &val[0], phy_idx);
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "CR init Success=%d\n", rpt);
		} else if (_os_strcmp(input[2], "gain") == 0) {
			phy_idx = (enum phl_phy_idx)val[0];
			reg = &bb->phl_com->phy_sw_cap[phy_idx].bb_phy_reg_gain_info;
			rpt = halbb_init_gain_table(bb, false, 0, &val[0], phy_idx);
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Gain init Success=%d\n", rpt);
		} else if (_os_strcmp(input[2], "dbg_mode") == 0) {
			bb->bb_dbg_i.cr_dbg_mode_en = (bool)val[0];
			bb->bb_dbg_i.rfe_type_curr_dbg = val[1];
			bb->bb_dbg_i.cut_curr_dbg = val[2];
		} else {
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
				    "Err\n");
			return;
		}
	} else if (_os_strcmp(input[1], "1") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		bb->hal_com->assoc_sta_cnt = (u8)val[0];
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "assoc_sta_cnt=%d\n", bb->hal_com->assoc_sta_cnt);
	} else if (_os_strcmp(input[1], "cr_demo") == 0) {	
		cr = halbb_get_reg(bb, LA_CLK_EN, LA_CLK_EN_M);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[Old] cr = %d\n", cr);

		HALBB_SET_CR(bb, LA_CLK_EN, ~cr);
		cr = HALBB_GET_CR(bb, LA_CLK_EN);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[New] ~cr = %d\n", cr);

		HALBB_SET_CR(bb, LA_CLK_EN, ~cr);
		cr = HALBB_GET_CR(bb, LA_CLK_EN);
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "[New] cr = %d\n", cr);
	}
}


void halbb_dbg_setting_init(struct bb_info *bb)
{
	halbb_cmd_parser_init(bb);
	bb->fw_dbg_component = 0;
	bb->cmn_dbg_msg_cnt = HALBB_WATCHDOG_PERIOD;
	bb->cmn_dbg_msg_period = HALBB_WATCHDOG_PERIOD;
	halbb_bb_dbg_port_clock_en(bb, true);

	bb->bb_dbg_i.cr_recorder_en = false;
	bb->bb_dbg_i.cr_dbg_mode_en = false;
}

void halbb_cr_cfg_dbg_init(struct bb_info *bb)
{
	struct bb_dbg_cr_info *cr = &bb->bb_dbg_i.bb_dbg_cr_i;

	switch (bb->cr_type) {

	#ifdef HALBB_52AA_SERIES
	case BB_52AA:
		cr->dbgport_ip = 0x09F4;
		cr->dbgport_ip_m = 0xff;
		cr->dbgport_idx = 0x09F0;
		cr->dbgport_idx_m = MASKLWORD;
		cr->dbgport_val = 0x40B0;
		cr->dbgport_val_m = MASKDWORD;
		cr->clk_en = 0x09F4;
		cr->clk_en_m = BIT(24);
		cr->dbgport_en = 0x09F8;
		cr->dbgport_en_m = BIT(31);
		break;
	#endif
	#ifdef HALBB_COMPILE_AP_SERIES
	case BB_AP:
		cr->dbgport_ip = DBG_PORT_IP_SEL_A;
		cr->dbgport_ip_m = DBG_PORT_IP_SEL_A_M;
		cr->dbgport_idx = DBG_PORT_SEL_A;
		cr->dbgport_idx_m = DBG_PORT_SEL_A_M;
		cr->dbgport_val = DBG32_D_A;
		cr->dbgport_val_m = DBG32_D_A_M;
		cr->clk_en = DBG_PORT_REF_CLK_EN_A;
		cr->clk_en_m = DBG_PORT_REF_CLK_EN_A_M;
		cr->dbgport_en = DBG_PORT_EN_A;
		cr->dbgport_en_m = DBG_PORT_EN_A_M;
		cr->bb_monitor_sel1 = MONITOR_SEL1_A;
		cr->bb_monitor_sel1_m = MONITOR_SEL1_A_M;
		cr->bb_monitor1 = 0x1b14;
		cr->bb_monitor1_m = MASKDWORD;
		/*mac_phy_intf*/
		cr->mac_phy_ppdu_type = 0x1800;
		cr->mac_phy_txpath_en = 0x1800;
		cr->mac_phy_txcmd = 0x1800;
		cr->mac_phy_txsc = 0x1804;
		cr->mac_phy_bw = 0x1804;
		cr->mac_phy_tx_pw = 0x1804;
		cr->mac_phy_n_usr = 0x1808;
		cr->mac_phy_stbc = 0x1810;
		cr->mac_phy_gi = 0x1810;
		cr->mac_phy_ltf = 0x1810;
		cr->mac_phy_ndp_en = 0x1814;
		cr->mac_phy_n_sts = 0x1818;
		cr->mac_phy_fec = 0x1818;
		cr->mac_phy_mcs_3_0 = 0x1818;
		cr->mac_phy_mcs_5_4 = 0x181c;
		cr->mac_phy_lsig = 0x1840;
		cr->mac_phy_siga_0 = 0x1848;
		cr->mac_phy_siga_1 = 0x184c;
		break;

	#endif
	#ifdef HALBB_COMPILE_CLIENT_SERIES
	case BB_CLIENT:
		cr->dbgport_ip = DBG_PORT_IP_SEL_C;
		cr->dbgport_ip_m = DBG_PORT_IP_SEL_C_M;
		cr->dbgport_idx = DBG_PORT_SEL_C;
		cr->dbgport_idx_m = DBG_PORT_SEL_C_M;
		cr->dbgport_val = DBG32_D_C;
		cr->dbgport_val_m = DBG32_D_C_M;
		cr->clk_en = DBG_PORT_REF_CLK_EN_C;
		cr->clk_en_m = DBG_PORT_REF_CLK_EN_C_M;
		cr->dbgport_en = DBG_PORT_EN_C;
		cr->dbgport_en_m = DBG_PORT_EN_C_M;
		cr->bb_monitor_sel1 = MONITOR_SEL1_C;
		cr->bb_monitor_sel1_m = MONITOR_SEL1_C_M;
		cr->bb_monitor1 = 0x1b14;
		cr->bb_monitor1_m = MASKDWORD;
		/*mac_phy_intf*/
		cr->mac_phy_ppdu_type = 0x1800;
		cr->mac_phy_txpath_en = 0x1800;
		cr->mac_phy_txcmd = 0x1800;
		cr->mac_phy_txsc = 0x1804;
		cr->mac_phy_bw = 0x1804;
		cr->mac_phy_tx_pw = 0x1804;
		cr->mac_phy_n_usr = 0x1808;
		cr->mac_phy_stbc = 0x1810;
		cr->mac_phy_gi = 0x1810;
		cr->mac_phy_ltf = 0x1810;
		cr->mac_phy_ndp_en = 0x1814;
		cr->mac_phy_n_sts = 0x1818;
		cr->mac_phy_fec = 0x1818;
		cr->mac_phy_mcs_3_0 = 0x1818;
		cr->mac_phy_mcs_5_4 = 0x181c;
		cr->mac_phy_lsig = 0x1840;
		cr->mac_phy_siga_0 = 0x1848;
		cr->mac_phy_siga_1 = 0x184c;

		break;
	#endif

	default:
		break;
	}

}

