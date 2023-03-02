/******************************************************************************
 *
 * Copyright(c) 2007 - 2020  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/
#include "../halrf_precomp.h"

#ifdef RF_8852B_SUPPORT

bool halrf_get_efuse_info_8852b(struct rf_info *rf, u8 *efuse_map,
		       enum rtw_efuse_info id, void *value, u32 length,
		       u8 autoload_status)
{
	struct rtw_hal_com_t *hal = rf->hal_com;

	u32 offset = 0;
	u8 default_value = 0;

	if (length != 1)
		return false;

	switch (id) {
		case EFUSE_INFO_RF_PKG_TYPE:
			offset = 0;
			default_value = 0;
			break;
		case EFUSE_INFO_RF_PA:
			offset = 0;
			default_value = 0;
			break;
		case EFUSE_INFO_RF_VALID_PATH:
			offset = 0;
			default_value = 0;
			break;
		case EFUSE_INFO_RF_RFE:
			offset = EFUSE_INFO_RF_RFE_8852B_ADDR;
			default_value = EFUSE_INFO_RF_RFE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_TXPWR:
			offset = 0;
			default_value = 0;
			break;
		case EFUSE_INFO_RF_BOARD_OPTION:
			offset = EFUSE_INFO_RF_BOARD_OPTION_8852B_ADDR;
			default_value = EFUSE_INFO_RF_BOARD_OPTION_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_CHAN_PLAN:
			offset = EFUSE_INFO_RF_CHAN_PLAN_8852B_ADDR;
			default_value = EFUSE_INFO_RF_CHAN_PLAN_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_COUNTRY:
			offset = 0;
			default_value = 0;
			break;
		/*TSSI DE PathA CCK*/
		case EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_1:
			offset = EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_1_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_2:
			offset = EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_2_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_3:
			offset = EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_3_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_4:
			offset = EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_4_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_5:
			offset = EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_5_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_6:
			offset = EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_6_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;

		/*TSSI DE PathA 2G MCS7 BW40M*/
		case EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_1:
			offset = EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_1_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_2:
			offset = EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_2_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_3:
			offset = EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_3_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_4:
			offset = EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_4_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_5:
			offset = EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_5_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;

		/*TSSI DE PathA 5G MCS7 BW40M*/
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_1:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_1_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_2:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_2_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_3:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_3_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_4:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_4_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_5:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_5_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_6:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_6_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_7:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_7_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_8:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_8_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_9:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_9_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_10:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_10_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_11:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_11_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_12:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_12_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_13:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_13_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_14:
			offset = EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_14_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;

		/*TSSI DE PathB CCK*/
		case EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_1:
			offset = EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_1_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_2:
			offset = EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_2_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_3:
			offset = EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_3_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_4:
			offset = EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_4_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_5:
			offset = EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_5_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_6:
			offset = EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_6_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;

		/*TSSI DE PathB 2G MCS7 BW40M*/
		case EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_1:
			offset = EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_1_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_2:
			offset = EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_2_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_3:
			offset = EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_3_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_4:
			offset = EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_4_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_5:
			offset = EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_5_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;

		/*TSSI DE PathB 5G MCS7 BW40M*/
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_1:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_1_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_2:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_2_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_3:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_3_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_4:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_4_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_5:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_5_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_6:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_6_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_7:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_7_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_8:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_8_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_9:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_9_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_10:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_10_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_11:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_11_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_12:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_12_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_13:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_13_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_14:
			offset = EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_14_8852B_ADDR;
			default_value = EFUSE_INFO_RF_TSSI_DE_8852B_VALUE;
			break;

		case EFUSE_INFO_RF_RX_GAIN_K_A_2G_CCK:
			offset = EFUSE_INFO_RF_RX_GAIN_K_A_2G_CCK_8852B_ADDR;
			default_value = EFUSE_INFO_RF_RX_GAIN_K_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_RX_GAIN_K_A_2G_OFMD:
			offset = EFUSE_INFO_RF_RX_GAIN_K_A_2G_OFMD_8852B_ADDR;
			default_value = EFUSE_INFO_RF_RX_GAIN_K_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_RX_GAIN_K_A_5GL:
			offset = EFUSE_INFO_RF_RX_GAIN_K_A_5GL_8852B_ADDR;
			default_value = EFUSE_INFO_RF_RX_GAIN_K_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_RX_GAIN_K_A_5GM:
			offset = EFUSE_INFO_RF_RX_GAIN_K_A_5GM_8852B_ADDR;
			default_value = EFUSE_INFO_RF_RX_GAIN_K_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_RX_GAIN_K_A_5GH:
			offset = EFUSE_INFO_RF_RX_GAIN_K_A_5GH_8852B_ADDR;
			default_value = EFUSE_INFO_RF_RX_GAIN_K_8852B_VALUE;
			break;
		/*Thermal*/
		case EFUSE_INFO_RF_THERMAL_A:
			offset = EFUSE_INFO_RF_THERMAL_A_8852B_ADDR;
			default_value = EFUSE_INFO_RF_THERMAL_A_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_THERMAL_B:
			offset = EFUSE_INFO_RF_THERMAL_B_8852B_ADDR;
			default_value = EFUSE_INFO_RF_THERMAL_B_8852B_VALUE;
			break;
		case EFUSE_INFO_RF_XTAL:
			offset = EFUSE_INFO_RF_XTAL_8852B_ADDR;
			default_value = EFUSE_INFO_RF_XTAL_8852B_VALUE;
			break;
		default:
			return false;
	}

	if (autoload_status == 0)
		hal_mem_cpy(hal, value, &default_value, 1);
	else {
		hal_mem_cpy(hal, value, efuse_map + offset, length);

		if (id == EFUSE_INFO_RF_XTAL) {
			if (*((u8 *)value) == 0xff)
				hal_mem_set(hal, value, 0x3f, 1);
		}	
	}

	return true;
}

#endif	/*#ifdef RF_8852B_SUPPORT*/