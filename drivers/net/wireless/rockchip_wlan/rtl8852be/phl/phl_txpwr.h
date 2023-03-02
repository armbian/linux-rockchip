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
#ifndef _PHL_TXPWR_H_
#define _PHL_TXPWR_H_

const char *rtw_phl_get_pw_lmt_regu_type_str(void *phl, enum band_type band);

bool rtw_phl_get_pwr_lmt_en(void *phl, u8 band_idx);

enum rtw_phl_status rtw_phl_set_tx_power(void *phl, u8 band_idx);

enum rtw_phl_status rtw_phl_get_txinfo_pwr(void *phl, s16 *pwr_dbm);

#endif /*_PHL_TXPWR_H_*/
