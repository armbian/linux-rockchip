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
#ifdef HALBB_DBCC_SUPPORT

struct bb_info *halbb_get_curr_bb_pointer(struct bb_info *bb,
					  enum phl_phy_idx phy_idx)
{
	if (phy_idx == bb->bb_phy_idx) {
		BB_DBG(bb, DBG_DBCC, "new_phy_idx(%d) = curr_bb_phy_idx(%d)\n", phy_idx, bb->bb_phy_idx);
		return bb;
	} else {
		BB_DBG(bb, DBG_DBCC, "new_phy_idx(%d) != curr_bb_phy_idx(%d)\n", phy_idx, bb->bb_phy_idx);
		if (bb->bb_phy_hooker) {
			return bb->bb_phy_hooker;
		} else {
			BB_WARNING("[%s] bb_phy_hooker==NULL", __func__);
			return bb;
		}
	}
}

void halbb_dbcc_demo_func(struct bb_info *bb)
{
	BB_DBG(bb, DBG_DBCC, "[%s]phy_idx={%d}\n", __func__, bb->bb_phy_idx);

}

u32
halbb_buffer_init_phy1(struct bb_info *bb_0)
{
	struct bb_info *bb_1 = NULL;

	if (!bb_0) {
		BB_WARNING("[%s]*bb_phy_0 = NULL\n", __func__);
		return RTW_HAL_STATUS_BB_INIT_FAILURE;
	}

	bb_1 = halbb_mem_alloc(bb_0, sizeof(struct bb_info));

	if (!bb_1) {
		BB_WARNING("[%s]*bb_phy_1 = NULL\n", __func__);
		return RTW_HAL_STATUS_BB_INIT_FAILURE;
	}

	bb_1->bb_phy_hooker = bb_0;
	bb_1->bb_phy_idx = HW_PHY_1;
	bb_0->bb_phy_hooker = bb_1;
	bb_1->bb_phy_hooker = bb_0;
	bb_1->bb_cmn_hooker = bb_0->bb_cmn_hooker;

	bb_1->phl_com = bb_0->phl_com;/*shared memory for all components*/
	bb_1->hal_com = bb_0->hal_com;/*shared memory for phl and hal*/
	//bb_phy_1->phl_sta_info = bb_phy_0->phl_sta_info;

	halbb_dbg_comp_init(bb_1);
	halbb_cmn_info_self_init(bb_1);
	//halbb_timer_ctrl(bb_1, BB_INIT_TIMER);
	//halbb_dm_init(bb_1, bb->bb_phy_idx);

	BB_DBG(bb_0, DBG_DBCC, "phy_idx[0,1]={%d, %d}\n", bb_0->bb_phy_idx, bb_1->bb_phy_idx);
	BB_DBG(bb_1, DBG_DBCC, "phy_idx[0,1]={%d, %d}\n", bb_0->bb_phy_idx, bb_1->bb_phy_idx);

	BB_DBG(bb_0, DBG_DBCC, "phy_idx[0,1]={%d, %d}\n", bb_1->bb_phy_hooker->bb_phy_idx, bb_0->bb_phy_hooker->bb_phy_idx);
	BB_DBG(bb_1, DBG_DBCC, "phy_idx[0,1]={%d, %d}\n", bb_1->bb_phy_hooker->bb_phy_idx, bb_0->bb_phy_hooker->bb_phy_idx);

	return RTW_HAL_STATUS_SUCCESS;
}

void halbb_dbcc_dbg(struct bb_info *bb, char input[][16], u32 *_used,
		       char *output, u32 *_out_len)
{
	u32 val[10] = {0};
	u16 i = 0;

	if (_os_strcmp(input[1], "-h") == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "phy {0/1}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "init\n");
		return;
	}

	if (_os_strcmp(input[1], "phy") == 0) {
		HALBB_SCAN(input[2], DCMD_DECIMAL, &val[0]);
		if (val[0] == 1)
			bb->bb_cmn_hooker->bb_echo_cmd_i.echo_phy_idx = HW_PHY_1;
		else
			bb->bb_cmn_hooker->bb_echo_cmd_i.echo_phy_idx = HW_PHY_0;
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "echo cmd convert to phy-%d mode\n",
			    bb->bb_cmn_hooker->bb_echo_cmd_i.echo_phy_idx);

		bb = halbb_get_curr_bb_pointer(bb, bb->bb_cmn_hooker->bb_echo_cmd_i.echo_phy_idx);

		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			    "bb_phy_idx=%d\n", bb->bb_phy_idx);
		
	} else if (_os_strcmp(input[1], "init") == 0) {
		//for (i = 0; i < 4; i++) {
		//	HALBB_SCAN(input[2 + i], DCMD_DECIMAL, &val[i]);
		//}
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "[DBG]phy_idx={%d}\n", bb->bb_phy_idx);

		BB_DBG(bb, DBG_DBCC, "phy0->phy1\n");
		bb = halbb_get_curr_bb_pointer(bb, HW_PHY_1); /*phy0->phy1*/
		BB_DBG(bb, DBG_DBCC, "phy1->phy1\n");
		bb = halbb_get_curr_bb_pointer(bb, HW_PHY_1); /*phy1->phy1*/
		BB_DBG(bb, DBG_DBCC, "phy1->phy0\n");
		bb = halbb_get_curr_bb_pointer(bb, HW_PHY_0); /*phy1->phy0*/
		BB_DBG(bb, DBG_DBCC, "phy0->phy0\n");
		bb = halbb_get_curr_bb_pointer(bb, HW_PHY_0); /*phy0->phy0*/
		BB_DBG(bb, DBG_DBCC, "phy0->phy1\n");
		bb = halbb_get_curr_bb_pointer(bb, HW_PHY_1); /*phy0->phy1*/

		//halbb_buffer_init_phy1(bb);
		//halbb_dbg_comp_init(bb);
		//halbb_dm_init(bb, bb->bb_phy_idx);
		
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "[DBG]phy_idx={%d} Init OK\n", bb->bb_phy_idx);
	}
}

#endif



