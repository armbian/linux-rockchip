
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
#ifndef _PHL_EXT_TX_PWR_LMT_DEF_H_
#define _PHL_EXT_TX_PWR_LMT_DEF_H_


struct rtw_phl_ext_pwr_lmt_info { /* external tx power limit information */
	s8 ext_pwr_lmt_2_4g[RTW_PHL_MAX_RF_PATH];
	s8 ext_pwr_lmt_5g_band1[RTW_PHL_MAX_RF_PATH]; /*CH36 ~ CH48*/
	s8 ext_pwr_lmt_5g_band2[RTW_PHL_MAX_RF_PATH]; /*CH52 ~ CH64*/
	s8 ext_pwr_lmt_5g_band3[RTW_PHL_MAX_RF_PATH]; /*CH100 ~ CH144*/
	s8 ext_pwr_lmt_5g_band4[RTW_PHL_MAX_RF_PATH]; /*CH149 ~ CH165*/
};


#endif /*_PHL_EXT_TX_PWR_LMT_DEF_H_*/
