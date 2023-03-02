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
#include "halrf_precomp.h"

bool halrf_init_reg_by_hdr(void *rf_void)
{
	bool result = true;
#if 0
	struct rf_info *rf = (struct rf_info *)rf_void;

#ifdef RF_8852A_SUPPORT
	if (rf->ic_type == RF_RTL8852A) {		
		halrf_config_8852a_radio_a_reg(rf, is_form_folder,
					   folder_len, folder_array);
		halrf_config_8852a_radio_b_reg(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif
#endif
	return result;
}

bool halrf_nctl_init_reg_by_hdr(void *rf_void)
{
	struct rf_info *rf = (struct rf_info *)rf_void;

	bool result = true;
#ifdef RF_8852A_SUPPORT
	if (rf->ic_type == RF_RTL8852A)	{
		halrf_config_8852a_nctl_reg(rf);
	}
#endif
#ifdef RF_8852B_SUPPORT
	if (rf->ic_type == RF_RTL8852B) {
		halrf_config_8852b_nctl_reg(rf);
	}
#endif

	return result;
}

bool halrf_config_radio_a_reg(void *rf_void, bool is_form_folder,
			   u32 folder_len, u32 *folder_array)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct rtw_hal_com_t *hal_com = rf->hal_com;
	bool result = true;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A) {
		halrf_config_8852a_radio_a_reg(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B) {
		halrf_config_8852b_radio_a_reg(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

	return result;
}

bool halrf_config_radio_b_reg(void *rf_void, bool is_form_folder,
			   u32 folder_len, u32 *folder_array)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct rtw_hal_com_t *hal_com = rf->hal_com;
	bool result = true;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A) {
		halrf_config_8852a_radio_b_reg(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B) {
		halrf_config_8852b_radio_b_reg(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

	return result;
}

bool halrf_config_store_power_by_rate(void *rf_void,
		bool is_form_folder, u32 folder_len, u32 *folder_array)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct rtw_hal_com_t *hal_com = rf->hal_com;
	bool result = true;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A) {
		halrf_config_8852a_store_power_by_rate(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B) {
		halrf_config_8852b_store_power_by_rate(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

	return result;
}

bool halrf_config_store_power_limit(void *rf_void,
		bool is_form_folder, u32 folder_len, u32 *folder_array)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct rtw_hal_com_t *hal_com = rf->hal_com;
	bool result = true;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A) {
		halrf_config_8852a_store_power_limit(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B) {
		halrf_config_8852b_store_power_limit(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

	return result;
}

bool halrf_config_store_power_limit_ru(void *rf_void,
		bool is_form_folder, u32 folder_len, u32 *folder_array)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct rtw_hal_com_t *hal_com = rf->hal_com;
	bool result = true;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A) {
		halrf_config_8852a_store_power_limit_ru(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B) {
		halrf_config_8852b_store_power_limit_ru(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

	return result;
}

bool halrf_config_store_power_track(void *rf_void,
		bool is_form_folder, u32 folder_len, u32 *folder_array)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct rtw_hal_com_t *hal_com = rf->hal_com;
	bool result = true;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A) {
		halrf_config_8852a_store_pwr_track(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B) {
		halrf_config_8852b_store_pwr_track(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

	return result;
}

bool halrf_config_store_xtal_track(void *rf_void,
		bool is_form_folder, u32 folder_len, u32 *folder_array)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct rtw_hal_com_t *hal_com = rf->hal_com;
	bool result = true;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A) {
		halrf_config_8852a_store_xtal_track(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif

#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B) {
		halrf_config_8852b_store_xtal_track(rf, is_form_folder,
					   folder_len, folder_array);
	}
#endif
	return result;
}

