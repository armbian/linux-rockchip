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
#define _PHL_EXT_TX_PWR_LMT_C_
#include "phl_headers.h"

/**
 * The function to update current TX power limit value to HW register
 * @phl: see struct phl_info_t
 * @hw_band: 0x0: band0, 0x1: band1
 *
 */
enum rtw_phl_status
rtw_phl_set_power_lmt(void *phl, u8 hw_band)
{
	struct	phl_info_t *phl_info = (struct phl_info_t *)phl;
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;

	if (rtw_hal_set_power_lmt(phl_info->hal, hw_band)==RTW_HAL_STATUS_SUCCESS)
		phl_status = RTW_PHL_STATUS_SUCCESS;

	return phl_status;
}

/**
 * The function to get TX power limit value with specific parameters
 * @phl: see struct phl_info_t
 * @hw_band: 0x0: band0, 0x1: band1
 * @rate: data rate
 * @bandwidth: banddwidth
 * @beamforming: 0: TX w/o BF, 1: TX w/ BF
 * @tx_num: tx number, 0: 1TX, 1: 2TX
 * @channel: center channel
 *
 */
s8 rtw_phl_get_power_limit(void *phl, u8 hw_band,
	u16 rate, u8 bandwidth, u8 beamforming, u8 tx_num, u8 channel)
{
	struct	phl_info_t *phl_info = (struct phl_info_t *)phl;

	return rtw_hal_rf_get_power_limit(phl_info->hal, hw_band, rate,
		bandwidth, beamforming, tx_num, channel);
}

/**
 * The function to update user defined extended tx power limit to halrf
 * @phl: see struct phl_info_t
 * @hw_band: 0x0: band0, 0x1: band1
 * @ext_pwr_lmt_info: table of extended tx power limit value
 *
 * Note,
 * This function will enable extended tx power limit mechanism.
 * After enabled this mechanism, halrf will use
 * min(original tx power limit, extended tx power limit) to be
 * final tx power limit value.
 *
 */
void
rtw_phl_enable_ext_pwr_lmt(void *phl, u8 hw_band,
		struct rtw_phl_ext_pwr_lmt_info *ext_pwr_lmt_info)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_tpu_ext_pwr_lmt_info tpu_ext_pwr_lmt_info = {0};
	u8 i;

	for (i = 0; i < HAL_MAX_PATH; i++) {
		tpu_ext_pwr_lmt_info.ext_pwr_lmt_2_4g[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_2_4g[i];

		tpu_ext_pwr_lmt_info.ext_pwr_lmt_5g_band1[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_5g_band1[i];
		tpu_ext_pwr_lmt_info.ext_pwr_lmt_5g_band2[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_5g_band2[i];
		tpu_ext_pwr_lmt_info.ext_pwr_lmt_5g_band3[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_5g_band3[i];
		tpu_ext_pwr_lmt_info.ext_pwr_lmt_5g_band4[i]
			= ext_pwr_lmt_info->ext_pwr_lmt_5g_band4[i];
	}

	rtw_hal_enable_ext_pwr_lmt(phl_info->hal, hw_band,
				   &tpu_ext_pwr_lmt_info);
}

void
rtw_phl_set_ext_pwr_lmt_en(void *phl, bool enable)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	rtw_hal_set_ext_pwr_lmt_en(phl_info->hal, enable);
}

