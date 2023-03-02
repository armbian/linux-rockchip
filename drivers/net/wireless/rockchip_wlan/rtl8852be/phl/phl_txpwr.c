/******************************************************************************
 *
 * Copyright(c) 2021 Realtek Corporation.
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
#define _PHL_TXPWR_C_
#include "phl_headers.h"

const char *rtw_phl_get_pw_lmt_regu_type_str(void *phl, enum band_type band)
{
	struct phl_info_t *phl_info = phl;

	return rtw_hal_get_pw_lmt_regu_type_str(phl_info->hal, band);
}

bool rtw_phl_get_pwr_lmt_en(void *phl, u8 band_idx)
{
	struct phl_info_t *phl_info = phl;

	return rtw_hal_get_pwr_lmt_en(phl_info->hal, band_idx);
}

enum rtw_phl_status rtw_phl_set_tx_power(void *phl, u8 band_idx)
{
	struct phl_info_t *phl_info = phl;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;

	hstatus = rtw_hal_set_tx_power(phl_info->hal, band_idx, PWR_BY_RATE | PWR_LIMIT | PWR_LIMIT_RU);
	if (hstatus != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("%s rtw_hal_set_tx_power: statuts = %u\n", __func__, hstatus);

	return hstatus == RTW_HAL_STATUS_SUCCESS ? RTW_PHL_STATUS_SUCCESS : RTW_PHL_STATUS_FAILURE;
}

enum rtw_phl_status rtw_phl_get_txinfo_pwr(void *phl, s16 *pwr_dbm)
{
	struct phl_info_t *phl_info = phl;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	s16 power_dbm = 0;

	hstatus = rtw_hal_get_txinfo_power(phl_info->hal, &power_dbm);
	*pwr_dbm = power_dbm;

	return hstatus == RTW_HAL_STATUS_SUCCESS ? RTW_PHL_STATUS_SUCCESS : RTW_PHL_STATUS_FAILURE;
}


