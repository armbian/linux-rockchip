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
#define _HAL_TEST_MP_TX_C_
#include "../../hal_headers.h"
#include "../../../test/mp/phl_test_mp_def.h"

#ifdef CONFIG_HAL_TEST_MP
enum rtw_hal_status rtw_hal_mp_tx_plcp_gen(
	struct mp_context *mp, struct mp_tx_arg *arg, struct mp_plcp_param_t *plcp_tx_struct)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;

	hal_status = rtw_hal_bb_set_plcp_tx(hal_info->hal_com,
										plcp_tx_struct,
										mp->usr,
										mp->cur_phy,
										&arg->plcp_sts);
	PHL_INFO("%s: phy index: %d\n", __FUNCTION__, mp->cur_phy);
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_tx_pmac_packet(
	struct mp_context *mp, struct mp_tx_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;

	PHL_INFO("%s: phy index: %d\n", __FUNCTION__, mp->cur_phy);
	PHL_INFO("%s: start tx: %d\n", __FUNCTION__, arg->start_tx);
	PHL_INFO("%s: is cck: %d\n", __FUNCTION__, arg->is_cck);
	PHL_INFO("%s: tx count: %d\n", __FUNCTION__, arg->tx_cnt);
	PHL_INFO("%s: period: %d\n", __FUNCTION__, arg->period);
	PHL_INFO("%s: tx time: %d\n", __FUNCTION__, arg->tx_time);

	hal_status = rtw_hal_bb_set_pmac_packet_tx(hal_info->hal_com,
						   arg->start_tx,
						   arg->is_cck,
						   arg->tx_cnt,
						   arg->period,
						   arg->tx_time,
						   mp->cur_phy);
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_tx_pmac_continuous(
	struct mp_context *mp, struct mp_tx_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;

	PHL_INFO("%s: phy index: %d\n", __FUNCTION__, mp->cur_phy);
	PHL_INFO("%s: start tx: %d\n", __FUNCTION__, arg->start_tx);
	PHL_INFO("%s: is cck: %d\n", __FUNCTION__, arg->is_cck);

	hal_status = rtw_hal_bb_set_pmac_cont_tx(hal_info->hal_com,
						 arg->start_tx,
						 arg->is_cck,
						 mp->cur_phy);
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_tx_pmac_fw_trigger(
	struct mp_context *mp, struct mp_tx_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;

	PHL_INFO("%s: phy index: %d\n", __FUNCTION__, mp->cur_phy);
	PHL_INFO("%s: start tx: %d\n", __FUNCTION__, arg->start_tx);
	PHL_INFO("%s: is cck: %d\n", __FUNCTION__, arg->is_cck);
	PHL_INFO("%s: tx count: %d\n", __FUNCTION__, arg->tx_cnt);
	PHL_INFO("%s: tx duty: %d\n", __FUNCTION__, arg->tx_time);

	hal_status = rtw_hal_bb_set_pmac_fw_trigger_tx(hal_info->hal_com,
						   arg->start_tx,
						   arg->is_cck,
						   arg->tx_cnt,
						   (u8)arg->tx_time,
						   mp->cur_phy);
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}



enum rtw_hal_status rtw_hal_mp_tx_single_tone(
	struct mp_context *mp, struct mp_tx_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s: start tx: %d\n", __FUNCTION__, arg->start_tx);
	PHL_INFO("%s: rf path: %d\n", __FUNCTION__, arg->tx_path);

	hal_status = rtw_hal_rf_set_singletone_tx(mp->hal,
						  arg->start_tx,
						  arg->tx_path);
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_tx_carrier_suppression(
	struct mp_context *mp, struct mp_tx_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s\n", __FUNCTION__);

	hal_status = rtw_hal_rf_set_suppression_tx(mp->hal);
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_tx_phy_ok_cnt(
	struct mp_context *mp, struct mp_tx_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s\n", __FUNCTION__);
	/*
	hal_status = rtw_hal_bb_get_tx_ok(phl_info->hal,
					mp->cur_phy, &arg->tx_ok);
	*/
	hal_status = RTW_HAL_STATUS_SUCCESS;
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);
	PHL_INFO("%s: phy ok cnt = %d\n", __FUNCTION__, arg->tx_ok);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_tx_mode_switch(
	struct mp_context *mp, struct mp_tx_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;
	
	PHL_INFO("%s\n", __FUNCTION__);
	
	/* mode: 0 = tmac, 1 = pmac */
	hal_status = rtw_hal_bb_tx_mode_switch(hal_info->hal_com, mp->cur_phy, arg->tx_mode);
	
	hal_status = RTW_HAL_STATUS_SUCCESS;
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);
	PHL_INFO("%s: tx_mode = %d\n", __FUNCTION__, arg->tx_mode);
	PHL_INFO("%s: phy index = %d\n", __FUNCTION__, mp->cur_phy);
	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_tx_f2p_cmd(
	struct mp_context *mp, struct mp_tx_arg *arg, struct mp_mac_ax_f2p_test_para *f2p_para_struct,
	struct mp_mac_ax_f2p_wd *f2p_wd_struct, struct mp_mac_ax_f2p_tx_cmd *f2p_tx_cmd_struct
	)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;
	u8 psigb_addr[64] = {0};

	PHL_INFO("%s\n", __FUNCTION__);

	hal_status = rtw_hal_mac_f2p_test_cmd(hal_info, f2p_para_struct, f2p_wd_struct, f2p_tx_cmd_struct, psigb_addr);

	hal_status = RTW_HAL_STATUS_SUCCESS;

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_set_dpd_bypass(
	struct mp_context *mp, struct mp_tx_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;

	PHL_INFO("%s: phy index: %d\n", __FUNCTION__, mp->cur_phy);
	PHL_INFO("%s: dpd_bypass: %d\n", __FUNCTION__, arg->dpd_bypass);

	hal_status = rtw_hal_bb_set_dpd_bypass(hal_info->hal_com,
						   arg->dpd_bypass,
						   mp->cur_phy);

	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

void rtw_hal_mp_check_tx_idle(struct mp_context *mp, struct mp_tx_arg *arg)
{
	arg->tx_state = rtw_hal_bb_check_tx_idle(mp->hal, mp->cur_phy);
}

#endif /* CONFIG_HAL_TEST_MP */
