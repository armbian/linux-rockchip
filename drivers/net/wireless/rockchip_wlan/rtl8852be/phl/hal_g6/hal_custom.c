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
#define _HAL_CUSTOM_C_
#include "hal_headers.h"

#ifdef CONFIG_PHL_CUSTOM_FEATURE
enum rtw_hal_status
rtw_hal_custom_cfg_tx_ampdu(void *hal,
                            struct rtw_wifi_role_t *wrole,
                            struct rtw_phl_custom_ampdu_cfg *ampdu)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hsts = RTW_HAL_STATUS_FAILURE;
	if (wrole == NULL || ampdu == NULL)
		return hsts;
	hsts = rtw_hal_mac_set_hw_ampdu_cfg(hal_info,
	                                    wrole->hw_band,
	                                    (u16)ampdu->max_agg_num,
	                                    (u8)ampdu->max_agg_time_32us);
	return hsts;
}

enum rtw_hal_status
rtw_hal_get_ampdu_cfg(void *hal,
                      struct rtw_wifi_role_t *wrole,
                      struct rtw_phl_custom_ampdu_cfg *cfg)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hsts = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_ampdu_cfg ampdu_cfg = {0};

	if (wrole == NULL || cfg == NULL)
		return hsts;
	hsts = rtw_hal_mac_get_ampdu_cfg(hal_info->hal_com,
	                                 wrole->hw_band,
	                                 &ampdu_cfg);
	if (hsts == RTW_HAL_STATUS_SUCCESS) {
		cfg->max_agg_num = ampdu_cfg.max_agg_num;
		cfg->max_agg_time_32us = ampdu_cfg.max_agg_time_32us;
	}
	return hsts;
}

enum rtw_hal_status
rtw_hal_set_pop_en(void *hal, bool en, enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	return rtw_hal_bb_set_pop_en(hal_info, en, phy_idx);
}

bool
rtw_hal_query_pop_en(void *hal, enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	return rtw_hal_bb_query_pop_en(hal_info, phy_idx);
}

enum rtw_hal_status
rtw_hal_set_pkt_detect_thold(void *hal, u32 bound)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	return rtw_hal_bb_set_pkt_detect_thold(hal_info, bound);
}

u8
rtw_hal_query_pkt_detect_thold(void *hal,
                               bool get_en_info,
                               enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	return rtw_hal_bb_query_pkt_detect_thold(hal_info, get_en_info, phy_idx);
}
#endif

