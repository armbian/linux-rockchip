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
#define _PHL_ACS_C_
#include "phl_headers.h"

#ifdef CONFIG_RTW_ACS
void phl_acs_mntr_trigger(struct phl_info_t *phl_info, u8 idx, u16 channel, u16 monitor_time)
{
	struct auto_chan_sel *acs = &phl_info->acs;

	if ((idx < 0) || (idx >= MAX_CHANNEL_NUM)) {
		PHL_ERR("[ACS][%s] idx(%d) invalid\n", __func__, idx);
		return;
	} else if (idx == 0) {
		_os_mem_set(phl_to_drvpriv(phl_info), acs, 0, sizeof(struct auto_chan_sel));
	}

	acs->curr_idx = idx;
	acs->chset[idx] = channel;

	rtw_hal_acs_mntr_trigger(phl_info->hal, monitor_time);
}

void phl_acs_mntr_result(struct phl_info_t *phl_info)
{
	struct auto_chan_sel *acs = &phl_info->acs;
	u8 idx = acs->curr_idx;
	enum rtw_hal_status status;
	struct auto_chan_sel_report rpt = {0};

	status = rtw_hal_acs_mntr_result(phl_info->hal, &rpt);


	if (idx >= MAX_CHANNEL_NUM) {
		PHL_ERR("[ACS][%s] idx(%d) invalid\n", __func__, idx);
		return;
	}

	if (status == RTW_HAL_STATUS_SUCCESS) {
		acs->clm_ratio[idx] = rpt.clm_ratio;
		acs->nhm_pwr[idx] = rpt.nhm_pwr;
	}
}

u16 rtw_phl_acs_get_channel_by_idx(void *phl, u8 idx)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct auto_chan_sel *acs = &phl_info->acs;

	if (idx >= MAX_CHANNEL_NUM) {
		PHL_ERR("[ACS][%s] idx(%d) invalid\n", __func__, idx);
		return 0;
	}

	return acs->chset[idx];
}

u8 rtw_phl_acs_get_clm_ratio_by_idx(void *phl, u8 idx)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct auto_chan_sel *acs = &phl_info->acs;

	if (idx >= MAX_CHANNEL_NUM) {
		PHL_ERR("%s [ACS] idx(%d) is invalid\n", __func__, idx);
		return 0;
	}

	return acs->clm_ratio[idx];
}

s8 rtw_phl_noise_query_by_idx(void *phl, u8 idx)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct auto_chan_sel *acs = &phl_info->acs;

	if (idx >= MAX_CHANNEL_NUM) {
		PHL_ERR("%s [ACS] idx(%d) is invalid\n", __func__, idx);
		return 0;
	}

	/* dBm = RPL - 110 */
	return acs->nhm_pwr[idx] - 110;
}
#endif /* CONFIG_RTW_ACS */

void rtw_phl_get_env_rpt(void *phl, struct rtw_env_report *env_rpt, struct rtw_wifi_role_t *wrole)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);

	rtw_hal_env_rpt(hal_com, env_rpt, wrole);

}
