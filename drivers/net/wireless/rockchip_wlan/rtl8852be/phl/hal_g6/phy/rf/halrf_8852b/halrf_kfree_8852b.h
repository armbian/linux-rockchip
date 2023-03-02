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
#ifndef _HALRF_KFREE_8852B_H_
#define _HALRF_KFREE_8852B_H_
#ifdef RF_8852B_SUPPORT

/*@--------------------------Define Parameters-------------------------------*/

#define HIDE_EFUSE_START_ADDR_8852B	 0x5A0
#define HIDE_EFUSE_END_ADDR_8852B	 0x5F0
#define HIDE_EFUSE_SIZE_8852B		 HIDE_EFUSE_END_ADDR_8852B - HIDE_EFUSE_START_ADDR_8852B

#define THERMAL_TRIM_HIDE_EFUSE_A_8852B 0x5DF
#define THERMAL_TRIM_HIDE_EFUSE_B_8852B 0x5DC

#define PABIAS_TRIM_HIDE_EFUSE_A_8852B 0x5DE
#define PABIAS_TRIM_HIDE_EFUSE_B_8852B 0x5DB

#define TSSI_TRIM_HIDE_EFUSE_2GL_A_8852B 0x5D6
#define TSSI_TRIM_HIDE_EFUSE_2GH_A_8852B 0x5D5
#define TSSI_TRIM_HIDE_EFUSE_5GL1_A_8852B 0x5D4
#define TSSI_TRIM_HIDE_EFUSE_5GL2_A_8852B 0x5D3
#define TSSI_TRIM_HIDE_EFUSE_5GM1_A_8852B 0x5D2
#define TSSI_TRIM_HIDE_EFUSE_5GM2_A_8852B 0x5D1
#define TSSI_TRIM_HIDE_EFUSE_5GH1_A_8852B 0x5D0
#define TSSI_TRIM_HIDE_EFUSE_5GH2_A_8852B 0x5CF

#define TSSI_TRIM_HIDE_EFUSE_2GL_B_8852B 0x5AB
#define TSSI_TRIM_HIDE_EFUSE_2GH_B_8852B 0x5AA
#define TSSI_TRIM_HIDE_EFUSE_5GL1_B_8852B 0x5A9
#define TSSI_TRIM_HIDE_EFUSE_5GL2_B_8852B 0x5A8
#define TSSI_TRIM_HIDE_EFUSE_5GM1_B_8852B 0x5A7
#define TSSI_TRIM_HIDE_EFUSE_5GM2_B_8852B 0x5A6
#define TSSI_TRIM_HIDE_EFUSE_5GH1_B_8852B 0x5A5
#define TSSI_TRIM_HIDE_EFUSE_5GH2_B_8852B 0x5A4

/*@-----------------------End Define Parameters-----------------------*/

void halrf_get_efuse_trim_8852b(struct rf_info *rf,
					enum phl_phy_idx phy);

#endif
#endif	/*_HALRF_SET_PWR_TABLE_8852B_H_*/
