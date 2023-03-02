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
#define _HAL_TXPWR_C_
#include "hal_headers.h"

const char *rtw_hal_get_pw_lmt_regu_type_str(void *hal, enum band_type band)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_rf_get_pw_lmt_regu_type_str(hal_info, band);
}

bool rtw_hal_get_pwr_lmt_en(void *hal, u8 band_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_get_pwr_lmt_en_val(hal_info->hal_com, band_idx);
}

enum rtw_hal_status rtw_hal_set_tx_power(void *hal, u8 band_idx,
					enum phl_pwr_table pwr_table)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum phl_phy_idx phy_idx = HW_PHY_0;

	if (band_idx == 1)
		phy_idx = HW_PHY_1;

	return rtw_hal_rf_set_power(hal_info, phy_idx, pwr_table);
}

enum rtw_hal_status rtw_hal_get_txinfo_power(void *hal,
					s16 *txinfo_power_dbm)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	s16 power_dbm = 0;

	hal_status = rtw_hal_bb_get_txinfo_power(hal_info, &power_dbm);
	*txinfo_power_dbm = power_dbm;

	return hal_status;
}

