/******************************************************************************
 *
 * Copyright(c) 2020 Realtek Corporation.
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
#define _PHL_THERMAL_C_
#include "phl_headers.h"

#ifdef CONFIG_PHL_THERMAL_PROTECT

static void _phl_thermal_protect_disable_all_txop(
	struct phl_info_t *phl_info,
	bool disable)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct rtw_wifi_role_t *wrole = NULL;
	struct rtw_phl_stainfo_t *sta = NULL;
	struct rtw_edca_param edca = {0};
	u8 i = 0;

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		if (mr_ctl->role_map & BIT(i)) {
			wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, i);
			if(wrole){
				if(wrole->mstate == MLME_LINKED)
					break;
			}
			wrole = NULL;
			continue;
		}
	}

	if(wrole == NULL)
		return;

	sta = rtw_phl_get_stainfo_self(phl_info, wrole);

	if(sta == NULL)
		return;

	for(i = 0; i < 4;i++){
		edca.ac = i;
		edca.param = sta->asoc_cap.edca[edca.ac].param;
		if(disable)
			edca.param &= 0x0000FFFF;
		if(rtw_hal_set_edca(phl_info->hal, wrole, edca.ac, edca.param)
		   != RTW_HAL_STATUS_SUCCESS)
			PHL_ERR("%s Config edca fail\n", __FUNCTION__);
	}
}

static void _phl_thermal_protect_reduce_ampdu_num(
	struct phl_info_t *phl_info,
	u8 ratio)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct mr_ctl_t *mr_ctl = phlcom_to_mr_ctrl(phl_com);
	struct rtw_wifi_role_t *wrole = NULL;
	struct rtw_phl_stainfo_t *sta = NULL;
	u8 i = 0;

	for (i = 0; i < MAX_WIFI_ROLE_NUMBER; i++) {
		if (mr_ctl->role_map & BIT(i)) {
			wrole = rtw_phl_get_wrole_by_ridx(phl_info->phl_com, i);
			if(wrole){
				if(wrole->mstate == MLME_LINKED)
					break;
			}
			wrole = NULL;
			continue;
		}
	}

	if(wrole == NULL)
		return;

	sta = rtw_phl_get_stainfo_self(phl_info, wrole);

	if(sta == NULL)
		return;

	if(ratio != 0){
		if(rtw_hal_thermal_protect_cfg_tx_ampdu(phl_info->hal, sta, ratio)
		   != RTW_HAL_STATUS_SUCCESS)
			PHL_ERR("%s Thermal protect cfg tx ampdu fail\n", __FUNCTION__);
	}
	else{
		if(sta->asoc_cap.num_ampdu_bk != 0){
			sta->asoc_cap.num_ampdu = sta->asoc_cap.num_ampdu_bk;
			sta->asoc_cap.num_ampdu_bk = 0;
		}
		if(rtw_hal_cfg_tx_ampdu(phl_info->hal, sta) !=
		   RTW_HAL_STATUS_SUCCESS)
			PHL_ERR("%s Thermal protect restore tx ampdu fail\n", __FUNCTION__);
	}

}

void phl_thermal_protect_watchdog(struct phl_info_t *phl_info)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	bool action_changed = false;

	if(phl_com->drv_mode != RTW_DRV_MODE_NORMAL &&
	   phl_com->drv_mode != RTW_DRV_MODE_HIGH_THERMAL)
		return;

	action_changed = rtw_hal_check_thermal_protect(phl_com, phl_info->hal);

	if(action_changed == false)
		return;

	switch (phl_com->thermal_protect_action){
		case PHL_THERMAL_PROTECT_ACTION_NONE:
			_phl_thermal_protect_disable_all_txop(phl_info, false);
			_phl_thermal_protect_reduce_ampdu_num(phl_info, 0);
			break;
		case PHL_THERMAL_PROTECT_ACTION_LEVEL1:
			_phl_thermal_protect_disable_all_txop(phl_info, true);
			_phl_thermal_protect_reduce_ampdu_num(phl_info, 70);
			break;
		case PHL_THERMAL_PROTECT_ACTION_LEVEL2:
			_phl_thermal_protect_reduce_ampdu_num(phl_info, 50);
			break;
		default:
			break;
	}
}

#endif /* CONFIG_PHL_THERMAL_PROTECT */

void phl_thermal_protect_cfg_tx_duty(
	struct phl_info_t *phl_info,
	u16 tx_duty_interval,
	u8 ratio)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	hal_status = rtw_hal_thermal_protect_cfg_tx_duty(phl_info->hal,
							 tx_duty_interval,
							 ratio);
	if(hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("%s Thermal protect cfg tx duty fail\n", __FUNCTION__);
}

void phl_thermal_protect_stop_tx_duty(struct phl_info_t *phl_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	hal_status = rtw_hal_thermal_protect_stop_tx_duty(phl_info->hal);
	if(hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("%s Thermal protect stop tx duty fail\n", __FUNCTION__);
}