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
#define _HAL_DBCC_C_
#include "hal_headers.h"

#ifdef CONFIG_DBCC_SUPPORT
enum rtw_hal_status
rtw_hal_dbcc_pre_cfg(void *hal, struct rtw_phl_com_t *phl_com, bool dbcc_en)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	enum rtw_hal_status hsts = RTW_HAL_STATUS_FAILURE;

	hal_com->dbcc_en = (dbcc_en) ? true : false;

	/*mac_dbcc_pre_cfg*/
	hsts = rtw_hal_mac_dbcc_pre_cfg(phl_com, hal_info, dbcc_en);
	if (hsts != RTW_HAL_STATUS_SUCCESS) {
		PHL_ASSERT("%s mac_dbcc_pre_cfg failed\n", __func__);
		hal_com->dbcc_en = false;
		goto exit_func;
	}
	/*phy_dbcc_pre_cfg*/
	hsts = rtw_hal_phy_dbcc_pre_cfg(hal_info, phl_com, dbcc_en);
	if (hsts != RTW_HAL_STATUS_SUCCESS) {
		PHL_ASSERT("%s phy_dbcc_pre_cfg failed\n", __func__);
		hal_com->dbcc_en = false;
		goto exit_func;
	}

	PHL_INFO("%s-%s done and success\n", __func__, (dbcc_en) ? "EN" : "DIS");

exit_func:

	return hsts;
}

enum rtw_hal_status
rtw_hal_dbcc_cfg(void *hal, struct rtw_phl_com_t *phl_com, bool dbcc_en)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	enum rtw_hal_status hsts = RTW_HAL_STATUS_FAILURE;

	/*if(phl_com->drv_mode == RTW_DRV_MODE_MP)*/

	if(hal_info->hal_com->chip_id ==CHIP_WIFI6_8852B)
	{
		PHL_ASSERT("%s 8852B doesn't support dbcc\n", __func__);
		hal_com->dbcc_en = false;
		goto exit_func;
	}
	
#if 1
	/* band 0  tx pause */
	rtw_hal_tx_pause(hal_com, HW_BAND_0, true, PAUSE_RSON_DBCC);
#else
	/* mac-id pause - rtw_hal_mac_set_macid_pause */
	rtw_hal_mac_set_macid_pause(hal_info->hal_com, 0, true);
	rtw_hal_mac_set_macid_pause(hal_info->hal_com, 1, true);
#endif
	/* FW tx pause , BT tx pause*/
	/* bb cca pause */
	rtw_hal_bb_ctrl_rx_cca(hal_info->hal_com, false, HW_PHY_0);

	/*mac_dbcc_cfg*/
	hsts = rtw_hal_mac_dbcc_cfg(phl_com, hal_info, dbcc_en);
	if (hsts != RTW_HAL_STATUS_SUCCESS) {
		PHL_ASSERT("%s mac_dbcc_cfg failed\n", __func__);
		hal_com->dbcc_en = false;
		goto exit_func;
	}

	/*bb_dbcc_cfg*/
	hsts = rtw_hal_phy_dbcc_cfg(hal_info, phl_com, dbcc_en);
	if (hsts != RTW_HAL_STATUS_SUCCESS) {
		PHL_ASSERT("%s phy_dbcc_cfg failed\n", __func__);
		hal_com->dbcc_en = false;
		goto exit_func;
	}

#if 1
	/* band 0 tx unpause */
	rtw_hal_tx_pause(hal_com, HW_BAND_0, false, PAUSE_RSON_DBCC);
#else
	/* mac-id unpause - rtw_hal_mac_set_macid_pause */
	rtw_hal_mac_set_macid_pause(hal_info->hal_com, 0, false);
	rtw_hal_mac_set_macid_pause(hal_info->hal_com, 1, false);
#endif

	/* FW tx unpause , BT tx unpause*/
	/* bb cca unpause */
	rtw_hal_bb_ctrl_rx_cca(hal_info->hal_com, true, HW_PHY_0);

	PHL_INFO("%s-%s done and success\n", __func__, (dbcc_en) ? "EN" : "DIS");

	/*mac role recovery*/
	/*if (has_ap)*/
	/*mac_port_cfg(BCN_EN-MAC_AX_PCFG_TX_SW)*/
	/*mac_port_cfg(BCN_DIS-MAC_AX_PCFG_MBSSID_EN)*/
exit_func:

	return hsts;
}

enum rtw_hal_status
rtw_hal_clean_tx_queue(void *hal)
{
#if 0
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	u32 start_time;

	start_time = _os_get_cur_time_ms();

	// set retry limit =1 -rtw_hal_mac_set_rty_lmt
	/* rtw_hal_mac_set_rty_lmt(hal_com, sta->macid,
			rts_txcnt_lmt_sel, rts_txcnt_lmt,
			data_txcnt_lmt_sel, data_tx_cnt_lmt);*/

	//polling mac_is_txq_empty - rtw_hal_mac_is_txq_empty
	//chk_allq_empty - rtw_hal_mac_is_allq_empty

	//polling TX idle
	//FW tx pause/unpause

	// if (has ap)
	//polling HIQ & WLCPU queue,
	//disable beacon TX mode
	//mac_port_cfg(BCN_DIS-MAC_AX_PCFG_TX_SW)
	//mac_port_cfg(BCN_DIS-MAC_AX_PCFG_MBSSID_EN)

	PHL_INFO("[DBCC] %s: pass %u ms\n", __FUNCTION__,
		 _get_passing_time_ms(start_time));
#endif
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_dbcc_tx_cnt(void *hal, struct rtw_phl_com_t *phl_com, u8 band)
{
#if 1 /* no rtw_hal_mac_get_tx_cnt api */
	return RTW_HAL_STATUS_SUCCESS;
#else
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_get_tx_cnt(hal_info, band, MAC_AX_TX_ALLTYPE, NULL);
#endif
}

enum rtw_hal_status
rtw_hal_dbcc_reset_tx_cnt(void *hal, struct rtw_phl_com_t *phl_com, u8 band)
{
#if 1 /* no rtw_hal_mac_set_reset_tx_cnt */
	return RTW_HAL_STATUS_SUCCESS;
#else
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_set_reset_tx_cnt(hal_info, band);
#endif
}
#endif /*CONFIG_DBCC_SUPPORT*/
