/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#define _HAL_API_C_
#include "hal_headers.h"

enum rtw_hal_status rtw_hal_get_tsf(void *hal, u8 port, u32 *tsf_h, u32 *tsf_l)
{
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;
	status = rtw_hal_mac_get_tsf(hal, &port, tsf_h, tsf_l);

	if (status != RTW_HAL_STATUS_SUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_get_tsf(): get failed\n");
	}
	return status;
}

enum rtw_hal_status
rtw_hal_tsf_sync(void *hal, u8 port_sync_from,
					u8 port_sync_to, enum phl_band_idx band,
					s32 sync_offset_tu, enum hal_tsf_sync_act act)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_tsf_sync(hal_info, port_sync_from,
						port_sync_to, band, sync_offset_tu, act);
}

enum rtw_hal_status
rtw_hal_config_rts_th(void *hal, u8 band_idx, u16 rts_time_th, u16 rts_len_th)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_set_hw_rts_th(hal_info, band_idx, rts_time_th, rts_len_th);
}

u32 rtw_hal_get_btc_req_slot(void *hal)
{
	return rtw_hal_btc_req_bt_slot_t(hal);
}

/**
 * This function is used to pause/unpause multiple macid
 * @hal: see hal_info_t
 * @macid_arr: macid array to be pause/unpause
 * 1 means to be set (pause/unpause)
 * 0 means don't care
 * @macid_arr_sz: size of macid array
 * @pause: all macid of this array to be paused/unpaused
 *  1=paused,0=unpaused
 */
enum rtw_hal_status
rtw_hal_set_macid_grp_pause(void *hal, u32 *macid_arr, u8 arr_size, bool pause)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;

	return rtw_hal_mac_set_macid_grp_pause(hal_com, macid_arr, arr_size, pause);
}

/**
 * This function is used to pause/unpause single macid
 * @hal: see hal_info_t
 * @macid: macid be pause/unpause
 * @pause: all macid of this array to be paused/unpaused
 *  1=paused,0=unpaused
 */
enum rtw_hal_status
rtw_hal_set_macid_pause(void *hal, u16 macid, bool pause)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;

	return rtw_hal_mac_set_macid_pause(hal_com, macid, pause);
}

enum rtw_hal_status rtw_hal_set_dfs_tb_ctrl(void *hal, u8 set)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_set_dfs_tb_ctrl(hal_info, set);
}

u32 rtw_hal_get_phy_stat_info(void *hal, enum phl_band_idx hw_band,
			      enum phl_stat_info_query phy_stat)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	struct rtw_hal_stat_info *stat_info = &hal_com->band[hw_band].stat_info;

	switch (phy_stat) {
	case STAT_INFO_FA_ALL:
		return stat_info->cnt_fail_all;
	case STAT_INFO_CCA_ALL:
		return stat_info->cnt_cca_all;
	default:
		return 0;
	}
}