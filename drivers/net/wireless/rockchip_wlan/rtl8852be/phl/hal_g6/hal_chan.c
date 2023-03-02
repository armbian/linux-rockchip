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
#define _HAL_CHAN_C_
#include "hal_headers.h"

#ifdef CONFIG_PHL_DFS
enum rtw_hal_status
rtw_hal_radar_detect_cfg(void *hal, bool dfs_enable)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	u8 rpt_num = 0;/*52A - 29/61/93/125 pkt*/
	u8 rpt_to = 1;/*52A CAV - 0 , 80us ,2ms, CBV - 0 / 20ms / 40ms / 80ms*/

	PHL_INFO("====>%s dfs_en:%d ============\n", __func__, dfs_enable);
	if (dfs_enable) {
		rtw_hal_mac_dfs_rpt_cfg(hal_info, true, rpt_num, rpt_to);
		rtw_hal_bb_dfs_rpt_cfg(hal_info, true);
	}
	else {
		rtw_hal_mac_dfs_rpt_cfg(hal_info, false, 0, 0);
		rtw_hal_bb_dfs_rpt_cfg(hal_info, true);
	}
	return RTW_HAL_STATUS_SUCCESS;
}
#endif /*CONFIG_PHL_DFS*/

enum rtw_hal_status rtw_hal_set_ch_bw(void *hal, u8 band_idx,
		struct rtw_chan_def *chdef, bool do_rfk)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	struct rtw_chan_def *cur_chdef = &(hal_com->band[band_idx].cur_chandef);
	enum rtw_hal_status status = RTW_HAL_STATUS_SUCCESS;
	u8 center_ch = 0;
	u8 central_ch_seg1 = 0;
	enum band_type change_band;
	enum phl_phy_idx phy_idx = HW_PHY_0;

	if ((chdef->chan != cur_chdef->chan) ||
	    (chdef->bw != cur_chdef->bw) ||
	    (chdef->offset != cur_chdef->offset)) {
		if (band_idx == 1)
			phy_idx = HW_PHY_1;

		status = rtw_hal_reset(hal_com, phy_idx, band_idx, true);
		if(status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("%s rtw_hal_reset en - failed\n", __func__);
			_os_warn_on(1);
		}
		/* if central channel changed, reset BB & MAC */
		center_ch = rtw_phl_get_center_ch(chdef->chan, chdef->bw, chdef->offset);
		PHL_INFO("Using central channel %u for primary channel %u BW %u\n",
		         center_ch, chdef->chan, chdef->bw);

		change_band = chdef->band;

		status = rtw_hal_mac_set_bw(hal_info, band_idx, chdef->chan,
					      center_ch, central_ch_seg1, change_band,
					      chdef->bw);
		if(status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("%s rtw_hal_mac_set_bw - failed\n", __func__);
			return status;
		}


		if(chdef->bw == CHANNEL_WIDTH_80_80 && central_ch_seg1 == 0) {
			PHL_ERR("%s mising info for 80+80M configuration\n", __func__);
			return RTW_HAL_STATUS_FAILURE;
		}
		status = rtw_hal_bb_set_ch_bw(hal_info, phy_idx, chdef->chan,
					      center_ch, central_ch_seg1, change_band,
					      chdef->bw);
		if(status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("%s rtw_hal_bb_set_ch_bw - failed\n", __func__);
			return status;
		}

		status = rtw_hal_rf_set_ch_bw(hal_info, phy_idx, center_ch, change_band, chdef->bw);

		if(status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("%s rtw_hal_rf_set_ch_bw - failed\n", __func__);
			return status;
		}

		cur_chdef->chan = chdef->chan;
		cur_chdef->bw = chdef->bw;
		cur_chdef->center_ch = center_ch;

		if (cur_chdef->band != change_band) {
			cur_chdef->band = change_band;
			rtw_hal_notify_switch_band(hal, change_band, phy_idx);
		}

		status = rtw_hal_rf_set_power(hal_info, phy_idx, PWR_LIMIT);

		if(status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("%s rtw_hal_rf_set_power - failed\n", __func__);
			return status;
		}

		status = rtw_hal_rf_set_power(hal_info, phy_idx, PWR_LIMIT_RU);

		if(status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("%s rtw_hal_rf_set_power - failed\n", __func__);
			return status;
		}

		PHL_INFO("%s band_idx:%d, ch:%d, bw:%d, offset:%d\n",
			__func__, band_idx, chdef->chan, chdef->bw, chdef->offset);

		status = rtw_hal_reset(hal_com, phy_idx, band_idx, false);
		if(status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("%s rtw_hal_reset dis- failed\n", __func__);
			_os_warn_on(1);
		}

		/*PHL_DUMP_CHAN_DEF_EX(chandef);*/
	}
	if (do_rfk) {
		status = rtw_hal_rf_chl_rfk_trigger(hal_info, phy_idx, true);
		if (status != RTW_HAL_STATUS_SUCCESS)
			PHL_ERR("rtw_hal_rf_chl_rfk_trigger fail!\n");
	}
	PHL_INFO("%s: Switch chdef done, do_rfk:%s\n", __func__,
		(do_rfk) ? "Y" : "N");
	return status;
}

u8 rtw_hal_get_cur_ch(void *hal, u8 band_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	struct rtw_chan_def *chandef = &(hal_com->band[band_idx].cur_chandef);

	return chandef->chan;
}

void rtw_hal_get_cur_chdef(void *hal, u8 band_idx,
				struct rtw_chan_def *cur_chandef)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	struct rtw_chan_def *chandef = &(hal_com->band[band_idx].cur_chandef);
	void *drv = hal_com->drv_priv;

	_os_mem_cpy(drv, cur_chandef, chandef, sizeof(struct rtw_chan_def));
}

void rtw_hal_sync_cur_ch(void *hal, u8 band_idx, struct rtw_chan_def chandef)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	struct rtw_chan_def *cur_chandef = &(hal_com->band[band_idx].cur_chandef);

	PHL_INFO("%s: Sync cur chan to ch:%d bw:%d offset:%d\n",
		 __FUNCTION__, chandef.chan, chandef.bw, chandef.offset);
	cur_chandef->chan = chandef.chan;
	cur_chandef->bw = chandef.bw;
	cur_chandef->offset = chandef.offset;
}
