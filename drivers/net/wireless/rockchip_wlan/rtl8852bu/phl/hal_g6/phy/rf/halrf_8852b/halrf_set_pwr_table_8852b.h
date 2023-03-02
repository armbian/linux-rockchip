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
#ifndef _HALRF_SET_PWR_TABLE_8852B_H_
#define _HALRF_SET_PWR_TABLE_8852B_H_

#ifdef RF_8852B_SUPPORT

/*@--------------------------Define Parameters-------------------------------*/
#define MAX_TX_PATH	2
#define TPU_SIZE_RUA	3 /*{26, 52, 106}*/
#define TPU_SIZE_BW20_SC  8 /*8 * 20M = 160M*/

/*@-----------------------End Define Parameters-----------------------*/

bool halrf_set_power_by_rate_to_mac_8852b(struct rf_info *rf,
						enum phl_phy_idx phy);

bool halrf_set_power_limit_to_mac_8852b(struct rf_info *rf,
	enum phl_phy_idx phy);

void halrf_set_ref_power_to_struct_8852b(struct rf_info *rf,
						enum phl_phy_idx phy);

bool halrf_set_power_8852b(struct rf_info *rf, enum phl_phy_idx phy,
	enum phl_pwr_table pwr_table);

void halrf_pwr_by_rate_info_8852b(struct rf_info *rf,
		char input[][16], u32 *_used, char *output, u32 *_out_len);

void halrf_pwr_limit_info_8852b(struct rf_info *rf,
		char input[][16], u32 *_used, char *output, u32 *_out_len);

void halrf_pwr_limit_ru_info_8852b(struct rf_info *rf,
		char input[][16], u32 *_used, char *output, u32 *_out_len);

void halrf_set_tx_shape_8852b(struct rf_info *rf, u8 tx_shape_idx);

#endif	/*RF_8852B_SUPPORT*/
#endif	/*_HALRF_SET_PWR_TABLE_8852B_H_*/