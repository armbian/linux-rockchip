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
#define _HAL_EXT_TX_PWR_LMT_C_
#include "hal_headers.h"


enum rtw_hal_status
rtw_hal_set_power_lmt(void *hal, u8 band_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status status = RTW_HAL_STATUS_SUCCESS;
	enum phl_phy_idx phy_idx = HW_PHY_0;

	if (band_idx == 1)
		phy_idx = HW_PHY_1;

	status = rtw_hal_rf_set_power(hal_info, phy_idx, PWR_LIMIT);

	return status;
}

void
rtw_hal_set_ext_pwr_lmt_en(void *hal,
		bool enable)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;

	PHL_INFO("%s: en(%d)\n", __func__, enable);

	hal_com->ext_pwr_lmt_en = enable;
}

void
rtw_hal_enable_ext_pwr_lmt(void *hal, u8 hw_band,
		struct rtw_tpu_ext_pwr_lmt_info *ext_pwr_lmt_info)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	struct rtw_tpu_ext_pwr_lmt_info *tpu_ext_pwr_lmt_info;
	u8 i;

	PHL_INFO("%s\n", __func__);

	tpu_ext_pwr_lmt_info = &(hal_com->rtw_tpu_ext_pwr_lmt_i[hw_band]);

	/* Fill external tx power limit into hal_com->rtw_tpu_ext_pwr_lmt_info */
	for (i = 0; i < HAL_MAX_PATH; i++) {
		tpu_ext_pwr_lmt_info->ext_pwr_lmt_2_4g[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_2_4g[i];

		tpu_ext_pwr_lmt_info->ext_pwr_lmt_5g_band1[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_5g_band1[i];
		tpu_ext_pwr_lmt_info->ext_pwr_lmt_5g_band2[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_5g_band2[i];
		tpu_ext_pwr_lmt_info->ext_pwr_lmt_5g_band3[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_5g_band3[i];
		tpu_ext_pwr_lmt_info->ext_pwr_lmt_5g_band4[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_5g_band4[i];
	}

	rtw_hal_rf_update_ext_pwr_lmt_table(hal_info);

	/* enable external tx power limit mechanism */
	hal_com->ext_pwr_lmt_en = true;
}

