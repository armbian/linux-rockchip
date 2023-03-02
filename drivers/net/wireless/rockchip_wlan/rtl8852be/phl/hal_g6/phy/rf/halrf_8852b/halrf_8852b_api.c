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

u8 halrf_get_thermal_8852b(struct rf_info *rf, enum rf_path rf_path)
{
	halrf_wrf(rf, rf_path, 0x42, BIT(19), 0x1);
	halrf_wrf(rf, rf_path, 0x42, BIT(19), 0x0);
	halrf_wrf(rf, rf_path, 0x42, BIT(19), 0x1);

	halrf_delay_10us(rf, 20);

	return (u8)halrf_rrf(rf, rf_path, 0x42, 0x0007e);
}

u32 halrf_mac_get_pwr_reg_8852b(struct rf_info *rf, enum phl_phy_idx phy,
	u32 addr, u32 mask)
{
	struct rtw_hal_com_t *hal = rf->hal_com;
	u32 result, ori_val, bit_shift, reg_val;

	result = rtw_hal_mac_get_pwr_reg(hal, phy, addr, &ori_val);
	if (result)
		RF_WARNING("=======>%s Get MAC(0x%x) fail, error code=%d\n",
			__func__, addr, result);
	else
		RF_DBG(rf, DBG_RF_POWER, "Get MAC(0x%x) ok!!! 0x%08x\n",
			addr, ori_val);

	bit_shift = halrf_cal_bit_shift(mask);
	reg_val = (ori_val & mask) >> bit_shift;

	return reg_val;
}

u32 halrf_mac_set_pwr_reg_8852b(struct rf_info *rf, enum phl_phy_idx phy,
	u32 addr, u32 mask, u32 val)
{
	struct rtw_hal_com_t *hal = rf->hal_com;
	u32 result;

	result = rtw_hal_mac_write_msk_pwr_reg(hal, phy, addr, mask, val);
	if (result) {
		RF_WARNING("=======>%s Set MAC(0x%x[0x%08x]) fail, error code=%d\n",
			__func__, addr, mask, result);
		return false;
	} else
		RF_DBG(rf, DBG_RF_POWER, "Set MAC(0x%x[0x%08x])=0x%08x ok!!! \n",
			addr, mask, val);

	return result;
}

bool halrf_wl_tx_power_control_8852b(struct rf_info *rf, u32 tx_power_val)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u32 result; 
	s32 tmp_pwr;
	u8 phy = 0;
	u32 all_time_control = 0;
	u32 gnt_bt_control = 0;

	RF_DBG(rf, DBG_RF_POWER, "=======>%s\n", __func__);

	all_time_control = tx_power_val & 0xffff;
	gnt_bt_control = (tx_power_val & 0xffff0000) >> 16;

	RF_DBG(rf, DBG_RF_POWER, "[Pwr Ctrl]tx_power_val=0x%x   all_time_control=0x%x   gnt_bt_control=0x%x\n",
		tx_power_val, all_time_control, gnt_bt_control);

	if (all_time_control == 0xffff) {
		/*Coex Disable*/
		pwr->coex_pwr_ctl_enable = false;
		pwr->coex_pwr = 0;
		RF_DBG(rf, DBG_RF_POWER, "[Pwr Ctrl] Coex Disable all_time_control=0xffff!!!\n");
	} else if (all_time_control == 0xeeee) {
		/*DPK Disable*/
		pwr->dpk_pwr_ctl_enable = false;
		pwr->dpk_pwr = 0;
		RF_DBG(rf, DBG_RF_POWER, "[Pwr Ctrl] DPK Disable all_time_control=0xeeee\n");
	} else {
		if (all_time_control & BIT(15)) {
			/*DPK*/
			pwr->dpk_pwr_ctl_enable = true;
			pwr->dpk_pwr = all_time_control & 0x1ff;

			RF_DBG(rf, DBG_RF_POWER, "[Pwr Ctrl] DPK Enable Set pwr->dpk_pwr = %d\n",
				pwr->dpk_pwr);
		} else {
			/*Coex*/
			pwr->coex_pwr_ctl_enable = true;
			pwr->coex_pwr = all_time_control & 0x1ff;

			RF_DBG(rf, DBG_RF_POWER, "[Pwr Ctrl] Coex Enable Set pwr->coex_pwr = %d\n",
				pwr->coex_pwr);
		}
	}

	if (pwr->coex_pwr_ctl_enable == true && pwr->dpk_pwr_ctl_enable == false) {
		tmp_pwr = pwr->coex_pwr;

		RF_DBG(rf, DBG_RF_POWER, "[Pwr Ctrl] coex_pwr_ctl_enable=true dpk_pwr_ctl_enable=false tmp_pwr=%d\n",
			tmp_pwr);
	} else if (pwr->coex_pwr_ctl_enable == false && pwr->dpk_pwr_ctl_enable == true) {
		tmp_pwr = pwr->dpk_pwr;

		RF_DBG(rf, DBG_RF_POWER, "[Pwr Ctrl] coex_pwr_ctl_enable=false dpk_pwr_ctl_enable=true tmp_pwr=%d\n",
			tmp_pwr);
	} else if (pwr->coex_pwr_ctl_enable == true && pwr->dpk_pwr_ctl_enable == true) {
		if (pwr->coex_pwr > pwr->dpk_pwr)
			tmp_pwr = pwr->dpk_pwr;
		else
			tmp_pwr = pwr->coex_pwr;

		RF_DBG(rf, DBG_RF_POWER, "[Pwr Ctrl] coex_pwr_ctl_enable=true dpk_pwr_ctl_enable=true tmp_pwr=%d\n",
			tmp_pwr);
	} else
		tmp_pwr = 0;

	if (pwr->coex_pwr_ctl_enable == false && pwr->dpk_pwr_ctl_enable == false) {
		/*all-time control Disable*/
		result = halrf_mac_set_pwr_reg_8852b(rf, phy, 0xd200, 0x3ff, 0x0);

		if (result) {
			RF_WARNING("=======>%s Set MAC(0xd200) fail, error code=%d\n",
				__func__, result);
			return false;
		} else {
			RF_DBG(rf, DBG_RF_POWER, "Set MAC(0xd200) ok!!!\n");
			rf->is_coex = false;
		}
	} else {
		/*all-time control*/
		result = halrf_mac_set_pwr_reg_8852b(rf, phy, 0xd200, 0x3ff, ((tmp_pwr & 0x1ff) | BIT(9)));
		if (result) {
			RF_WARNING("=======>%s Set MAC(0xd200) fail, error code=%d\n",
				__func__, result);
			return false;
		} else {
			RF_DBG(rf, DBG_RF_POWER, "Set MAC(0xd200) ok!!!\n");
			rf->is_coex = true;
		}
	}

	if (gnt_bt_control == 0xffff) {
		/*GNT_BT control*/

		RF_DBG(rf, DBG_RF_POWER, "=======>%s   gnt_bt_control = 0x%x\n",
			__func__, gnt_bt_control);

		result = halrf_mac_set_pwr_reg_8852b(rf, phy, 0xd220, BIT(1), 0x0);
		result = halrf_mac_set_pwr_reg_8852b(rf, phy, 0xd220, 0xff8, 0x0);
		if (result) {
			RF_WARNING("=======>%s Set MAC(0xd220) fail, error code=%d\n",
				__func__, result);
			return false;
		} else {
			RF_DBG(rf, DBG_RF_POWER, "Set MAC(0xd220) ok!!!\n");
			rf->is_coex = false;
		}
	} else {
		/*GNT_BT control*/

		RF_DBG(rf, DBG_RF_POWER, "=======>%s   gnt_bt_control = 0x%x\n",
			__func__, gnt_bt_control);

		result = halrf_mac_set_pwr_reg_8852b(rf, phy, 0xd220, BIT(1), 0x1);
		result = halrf_mac_set_pwr_reg_8852b(rf, phy, 0xd220, 0xff8, gnt_bt_control & 0x1ff);
		if (result) {
			RF_WARNING("=======>%s Set MAC(0xd220) fail, error code=%d\n",
				__func__, result);
			return false;
		} else {
			RF_DBG(rf, DBG_RF_POWER, "Set MAC(0xd220) ok!!!\n");
			rf->is_coex = true;
		}
	}

	return true;
}

s8 halrf_get_ther_protected_threshold_8852b(struct rf_info *rf)
{
	u8 tmp_a, tmp_b, tmp;

	tmp_a = halrf_get_thermal(rf, RF_PATH_A);
	tmp_b = halrf_get_thermal(rf, RF_PATH_B);

	if (tmp_a > tmp_b)
		tmp = tmp_a;
	else
		tmp = tmp_b;

	if (tmp > 0x32)
		return -1;	/*Tx duty reduce*/
	else if (tmp < 0x31)
		return 1;	/*Tx duty up*/
	else 
		return 0;	/*Tx duty the same*/
}

s8 halrf_xtal_tracking_offset_8852b(struct rf_info *rf,
					enum phl_phy_idx phy)
{
	struct halrf_xtal_info *xtal_trk = &rf->xtal_track;
	u8 thermal_a = 0xff, thermal_b = 0xff;
	u8 tmp_a, tmp_b, tmp;
	s8 xtal_ofst = 0;

	RF_DBG(rf, DBG_RF_XTAL_TRACK, "======>%s   phy=%d\n",
		__func__, phy);

	tmp_a = halrf_get_thermal(rf, RF_PATH_A);
	tmp_b = halrf_get_thermal(rf, RF_PATH_B);
	halrf_efuse_get_info(rf, EFUSE_INFO_RF_THERMAL_A, &thermal_a, 1);
	halrf_efuse_get_info(rf, EFUSE_INFO_RF_THERMAL_B, &thermal_b, 1);

	if (thermal_a == 0xff || thermal_b == 0xff ||
		thermal_a == 0x0 || thermal_b == 0x0) {
		RF_DBG(rf, DBG_RF_XTAL_TRACK, "======>%s PG ThermalA=%d ThermalB=%d\n",
			__func__, thermal_a, thermal_b);
		return 0;
	}

	if (tmp_a > tmp_b) {
		if (tmp_a > thermal_a) {
			tmp = tmp_a - thermal_a;
			if (tmp > DELTA_SWINGIDX_SIZE)
				tmp = DELTA_SWINGIDX_SIZE - 1;
			xtal_ofst = xtal_trk->delta_swing_xtal_table_idx_p[tmp];
		} else {
			tmp = thermal_a - tmp_a;
			if (tmp > DELTA_SWINGIDX_SIZE)
				tmp = DELTA_SWINGIDX_SIZE - 1;
			xtal_ofst = xtal_trk->delta_swing_xtal_table_idx_n[tmp];
		}
	} else {
		if (tmp_b > thermal_b) {
			tmp = tmp_b - thermal_b;
			if (tmp > DELTA_SWINGIDX_SIZE)
				tmp = DELTA_SWINGIDX_SIZE - 1;
			xtal_ofst = xtal_trk->delta_swing_xtal_table_idx_p[tmp];
		} else {
			tmp = thermal_b - tmp_b;
			if (tmp > DELTA_SWINGIDX_SIZE)
				tmp = DELTA_SWINGIDX_SIZE - 1;
			xtal_ofst = xtal_trk->delta_swing_xtal_table_idx_n[tmp];
		}
	}

	RF_DBG(rf, DBG_RF_XTAL_TRACK, "PG ThermalA=%d   ThermalA=%d\n",
		thermal_a, tmp_a);

	RF_DBG(rf, DBG_RF_XTAL_TRACK, "PG ThermalB=%d   ThermalB=%d\n",
		thermal_b, tmp_b);

	RF_DBG(rf, DBG_RF_XTAL_TRACK, "xtal_ofst[%d]=%d\n",
		tmp, xtal_ofst);

	return xtal_ofst;
}

#endif
