/******************************************************************************
 *
 * Copyright(c)2021 Realtek Corporation.
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
#ifndef _HAL_TXPWR_H_
#define _HAL_TXPWR_H_

const char *rtw_hal_get_pw_lmt_regu_type_str(void *hal, enum band_type band);

bool rtw_hal_get_pwr_lmt_en(void *hal, u8 band_idx);

enum rtw_hal_status rtw_hal_set_tx_power(void *hal, u8 band_idx,
					enum phl_pwr_table pwr_table);

#endif