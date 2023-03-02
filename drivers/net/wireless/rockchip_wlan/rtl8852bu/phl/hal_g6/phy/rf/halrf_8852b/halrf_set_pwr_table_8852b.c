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
#include "../halrf_precomp.h"

#ifdef RF_8852B_SUPPORT

s8 _halrf_avg_power_8852b(struct rf_info *rf, enum phl_phy_idx phy, s8 *value, s8 n)
{
	u8 i;
	s16 total = 0;

	RF_DBG(rf, DBG_RF_POWER, "=======>%s\n", __func__);

	for (i = 0; i < n; i++) {
		total = total + value[i];
		RF_DBG(rf, DBG_RF_POWER, "value[%d]=%d total=%d n=%d\n", i, value[i], total, n);
	}

	total = total / n;

	return (s8)total;
}

void _halrf_bub_sort_8852b(struct rf_info *rf, enum phl_phy_idx phy, s8 *data, u32 n)
{
	s32 i, j, sp;
	s8 temp;
	u32 k;

	for (k = 0; k < n; k++)
		RF_DBG(rf, DBG_RF_POWER, "===> %s  Before data[%d]=%d\n", __func__, k, data[k]);

	for (i = n - 1; i >= 0; i--) {
		sp = 1;
		for (j = 0; j < i; j++) {
			if (data[j] < data[j + 1]) {
				temp = data[j];
				data[j] = data[j + 1];
				data[j + 1] = temp;
				sp = 0;
			}
		}
		if (sp == 1)
			break;
	}

	for (k = 0; k < n; k++)
		RF_DBG(rf, DBG_RF_POWER, "<=== %s  After data[%d]=%d\n", __func__, k, data[k]);
}

bool halrf_set_power_by_rate_to_struct_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy].rtw_tpu_i;
	struct rtw_tpu_pwr_by_rate_info *rate = &tpu->rtw_tpu_pwr_by_rate_i;
	u32 i, j;

	RF_DBG(rf, DBG_RF_POWER, "======>%s\n", __func__);

	rate->pwr_by_rate_lgcy[0] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK1, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[1] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK2, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[2] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK5_5, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[3] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, 0, 0) / 2;

	rate->pwr_by_rate_lgcy[4] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM6, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[5] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM9, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[6] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM12, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[7] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM18, 0, 0) / 2;

	rate->pwr_by_rate_lgcy[8] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM24, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[9] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM36, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[10] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM48, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[11] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_1T][0] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS0, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][1] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS1, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][2] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS2, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][3] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS3, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_1T][4] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS4, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][5] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS5, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][6] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS6, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][7] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_1T][8] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS8, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][9] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS9, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][10] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS10, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][11] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS11, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_1T][12] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS0, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][13] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS1, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][14] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS3, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][15] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS4, 1, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][0] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS0, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][1] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS1, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][2] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS2, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][3] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS3, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][4] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS4, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][5] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS5, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][6] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS6, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][7] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][8] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS8, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][9] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS9, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][10] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS10, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][11] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS11, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][12] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS0, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][13] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS1, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][14] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS3, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][15] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS4, 1, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][12] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS0, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][13] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS1, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][14] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS3, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][15] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS4, 1, 0) / 2;

	/*CCK, Legacy, HT, VHT, HE*/
	tpu->pwr_ofst_mode[0] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS11, 0, 1) / 2;
	tpu->pwr_ofst_mode[1] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_VHT_NSS1_MCS9, 0, 1) / 2;
	tpu->pwr_ofst_mode[2] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_MCS7, 0, 1) / 2;
	tpu->pwr_ofst_mode[3] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 1) / 2;
	tpu->pwr_ofst_mode[4] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, 0, 1) / 2;

	for (i = 0; i < 12; i = i + 4)
		RF_DBG(rf, DBG_RF_POWER, "power_by_rate ==> %d   %d   %d   %d\n",
			rate->pwr_by_rate_lgcy[i + 3],
			rate->pwr_by_rate_lgcy[i + 2],
			rate->pwr_by_rate_lgcy[i + 1],
			rate->pwr_by_rate_lgcy[i]);

	for (j = 0; j < 2; j++)
		for (i = 0; i < 16; i = i + 4)
			RF_DBG(rf, DBG_RF_POWER, "power_by_rate ==> %d   %d   %d   %d\n",
				rate->pwr_by_rate[j][i + 3],
				rate->pwr_by_rate[j][i + 2],
				rate->pwr_by_rate[j][i + 1],
				rate->pwr_by_rate[j][i]);

	RF_DBG(rf, DBG_RF_POWER, "power_by_rate_offset ==> %d   %d   %d   %d   %d\n",
		tpu->pwr_ofst_mode[4],
		tpu->pwr_ofst_mode[3],
		tpu->pwr_ofst_mode[2],
		tpu->pwr_ofst_mode[1],
		tpu->pwr_ofst_mode[0]);
	return true;
}

bool halrf_set_power_by_rate_all_the_smae_to_struct_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy].rtw_tpu_i;
	struct rtw_tpu_pwr_by_rate_info *rate = &tpu->rtw_tpu_pwr_by_rate_i;
	u32 i, j;

	RF_DBG(rf, DBG_RF_POWER, "======>%s\n", __func__);

	rate->pwr_by_rate_lgcy[0] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[1] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[2] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[3] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, 0, 0) / 2;

	rate->pwr_by_rate_lgcy[4] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[5] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[6] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[7] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;

	rate->pwr_by_rate_lgcy[8] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[9] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[10] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;
	rate->pwr_by_rate_lgcy[11] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_1T][0] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][1] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][2] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][3] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_1T][4] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][5] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][6] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][7] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_1T][8] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][9] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][10] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][11] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_1T][12] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS4, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][13] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS4, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][14] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS4, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_1T][15] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS4, 1, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][0] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][1] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][2] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][3] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][4] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][5] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][6] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][7] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][8] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][9] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][10] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][11] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS7, 0, 0) / 2;

	rate->pwr_by_rate[PW_LMT_PH_2T][12] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS4, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][13] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS4, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][14] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS4, 1, 0) / 2;
	rate->pwr_by_rate[PW_LMT_PH_2T][15] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS2_MCS4, 1, 0) / 2;

	/*CCK, Legacy, HT, VHT, HE*/
	tpu->pwr_ofst_mode[0] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS11, 0, 1) / 2;
	tpu->pwr_ofst_mode[1] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_VHT_NSS1_MCS9, 0, 1) / 2;
	tpu->pwr_ofst_mode[2] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_MCS7, 0, 1) / 2;
	tpu->pwr_ofst_mode[3] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, 0, 1) / 2;
	tpu->pwr_ofst_mode[4] = halrf_get_power_by_rate(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, 0, 1) / 2;

	for (i = 0; i < 12; i = i + 4)
		RF_DBG(rf, DBG_RF_POWER, "power_by_rate ==> %d   %d   %d   %d\n",
			rate->pwr_by_rate_lgcy[i + 3],
			rate->pwr_by_rate_lgcy[i + 2],
			rate->pwr_by_rate_lgcy[i + 1],
			rate->pwr_by_rate_lgcy[i]);

	for (j = 0; j < 2; j++)
		for (i = 0; i < 16; i = i + 4)
			RF_DBG(rf, DBG_RF_POWER, "power_by_rate ==> %d   %d   %d   %d\n",
				rate->pwr_by_rate[j][i + 3],
				rate->pwr_by_rate[j][i + 2],
				rate->pwr_by_rate[j][i + 1],
				rate->pwr_by_rate[j][i]);

	RF_DBG(rf, DBG_RF_POWER, "power_by_rate_offset ==> %d   %d   %d   %d   %d\n",
		tpu->pwr_ofst_mode[0],
		tpu->pwr_ofst_mode[1],
		tpu->pwr_ofst_mode[2],
		tpu->pwr_ofst_mode[3],
		tpu->pwr_ofst_mode[4]);
	return true;
}


void halrf_get_power_limit_to_struct_20m_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_pwr_imt_info *lmt = &rf->hal_com->band[phy].rtw_tpu_i.rtw_tpu_pwr_imt_i;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u8 ch = rf->hal_com->band[phy].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_POWER, "======>%s channel=%d\n", __func__, ch);

	hal_mem_set(hal, lmt, 0, sizeof(*lmt));

	/*1Tx CCK 20m  NOBF*/
	lmt->pwr_lmt_cck_20m[PW_LMT_PH_1T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx CCK 20m BF*/
	lmt->pwr_lmt_cck_20m[PW_LMT_PH_1T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx CCK 40m  NOBF*/
	lmt->pwr_lmt_cck_40m[PW_LMT_PH_1T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx CCK 40m BF*/
	lmt->pwr_lmt_cck_40m[PW_LMT_PH_1T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx OFDM  NOBF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_1T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx OFDM BF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_1T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx 20m NOBF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_1T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx 20m BF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_1T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch) / 2;

	/*2Tx CCK 20m NOBF*/
	lmt->pwr_lmt_cck_20m[PW_LMT_PH_2T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx CCK 20m BF*/
	lmt->pwr_lmt_cck_20m[PW_LMT_PH_2T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx CCK 40m NOBF*/
	lmt->pwr_lmt_cck_40m[PW_LMT_PH_2T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx CCK 40m BF*/
	lmt->pwr_lmt_cck_40m[PW_LMT_PH_2T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx OFDM NOBF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_2T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx OFDM BF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_2T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx 20m NOBF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_2T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx 20m BF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_2T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch) / 2;
}

void halrf_get_power_limit_to_struct_40m_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_pwr_imt_info *lmt = &rf->hal_com->band[phy].rtw_tpu_i.rtw_tpu_pwr_imt_i;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u8 ch = rf->hal_com->band[phy].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_POWER, "======>%s channel=%d\n", __func__, ch);

	hal_mem_set(hal, lmt, 0, sizeof(*lmt));

	/*1Tx CCK 20m NONBF*/
	lmt->pwr_lmt_cck_20m[PW_LMT_PH_1T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch - 2) / 2;

	/*1Tx CCK 20m BF*/
	lmt->pwr_lmt_cck_20m[PW_LMT_PH_1T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch - 2) / 2;

	/*1Tx CCK 40m NONBF*/
	lmt->pwr_lmt_cck_40m[PW_LMT_PH_1T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx CCK 40m BF*/
	lmt->pwr_lmt_cck_40m[PW_LMT_PH_1T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx OFDM NONBF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_1T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch - 2) / 2;

	/*1Tx OFDM BF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_1T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch - 2) / 2;

	/*1Tx 20m NOBF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_1T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch - 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_1T][1][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch + 2) / 2;

	/*1Tx 20m BF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_1T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch - 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_1T][1][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch + 2) / 2;

	/*1Tx 40m NOBF*/
	lmt->pwr_lmt_40m[PW_LMT_PH_1T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx 40m BF*/
	lmt->pwr_lmt_40m[PW_LMT_PH_1T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_1T, ch) / 2;

	/*2Tx CCK 20m NONBF*/
	lmt->pwr_lmt_cck_20m[PW_LMT_PH_2T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch - 2) / 2;

	/*2Tx CCK 20m BF*/
	lmt->pwr_lmt_cck_20m[PW_LMT_PH_2T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch - 2) / 2;

	/*2Tx CCK 40m NONBF*/
	lmt->pwr_lmt_cck_40m[PW_LMT_PH_2T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx CCK 40m BF*/
	lmt->pwr_lmt_cck_40m[PW_LMT_PH_2T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_CCK11, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx OFDM NONBF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_2T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch - 2) / 2;

	/*2Tx OFDM BF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_2T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch - 2) / 2;

	/*2Tx 20m NOBF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_2T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch - 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_2T][1][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch + 2) / 2;

	/*2Tx 20m BF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_2T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch - 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_2T][1][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch + 2) / 2;

	/*2Tx 40m NOBF*/
	lmt->pwr_lmt_40m[PW_LMT_PH_2T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx 40m BF*/
	lmt->pwr_lmt_40m[PW_LMT_PH_2T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_2T, ch) / 2;
}

void halrf_get_power_limit_to_struct_80m_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_pwr_imt_info *lmt = &rf->hal_com->band[phy].rtw_tpu_i.rtw_tpu_pwr_imt_i;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u8 ch = rf->hal_com->band[phy].cur_chandef.center_ch;
	s8 tmp, tmp1, tmp2;

	RF_DBG(rf, DBG_RF_POWER, "======>%s channel=%d\n", __func__, ch);

	hal_mem_set(hal, lmt, 0, sizeof(*lmt));

	/*1Tx OFDM NONBF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_1T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch - 6) / 2;

	/*1Tx OFDM BF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_1T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch - 6) / 2;

	/*1Tx 20m NOBF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_1T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch - 6) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_1T][1][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch - 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_1T][2][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch + 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_1T][3][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_1T, ch + 6) / 2;

	/*1Tx 20m BF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_1T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch - 6) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_1T][1][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch - 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_1T][2][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch + 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_1T][3][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_1T, ch + 6) / 2;

	/*1Tx 40m NOBF*/
	lmt->pwr_lmt_40m[PW_LMT_PH_1T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_1T, ch - 4) / 2;

	lmt->pwr_lmt_40m[PW_LMT_PH_1T][1][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_1T, ch + 4) / 2;

	/*1Tx 40m BF*/
	lmt->pwr_lmt_40m[PW_LMT_PH_1T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_1T, ch - 4) / 2;

	lmt->pwr_lmt_40m[PW_LMT_PH_1T][1][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_1T, ch + 4) / 2;

	/*1Tx 80m NOBF*/
	lmt->pwr_lmt_80m[PW_LMT_PH_1T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_80M, PW_LMT_NONBF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx 80m BF*/
	lmt->pwr_lmt_80m[PW_LMT_PH_1T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_80M, PW_LMT_BF, PW_LMT_PH_1T, ch) / 2;

	/*1Tx 40m 0p5 NOBF*/
	tmp1 = halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_1T, ch - 4) / 2;
	tmp2 = halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_1T, ch + 4) / 2;
	if (tmp1 >= tmp2)
		tmp = tmp2;
	else
		tmp = tmp1;

	lmt->pwr_lmt_40m_0p5[PW_LMT_PH_1T][PW_LMT_NONBF] = tmp;

	/*1Tx 40m 0p5 BF*/
	tmp1 = halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_1T, ch - 4) / 2;
	tmp2 = halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_1T, ch + 4) / 2;
	if (tmp1 >= tmp2)
		tmp = tmp2;
	else
		tmp = tmp1;

	lmt->pwr_lmt_40m_0p5[PW_LMT_PH_1T][PW_LMT_BF] = tmp;

	/*2Tx OFDM NONBF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_2T][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch - 6) / 2;

	/*2Tx OFDM BF*/
	lmt->pwr_lmt_lgcy_20m[PW_LMT_PH_2T][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_OFDM54, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch - 6) / 2;

	/*2Tx 20m NOBF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_2T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch - 6) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_2T][1][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch - 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_2T][2][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch + 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_2T][3][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_NONBF, PW_LMT_PH_2T, ch + 6) / 2;

	/*2Tx 20m BF*/
	lmt->pwr_lmt_20m[PW_LMT_PH_2T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch - 6) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_2T][1][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch - 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_2T][2][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch + 2) / 2;

	lmt->pwr_lmt_20m[PW_LMT_PH_2T][3][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_20M, PW_LMT_BF, PW_LMT_PH_2T, ch + 6) / 2;

	/*2Tx 40m NOBF*/
	lmt->pwr_lmt_40m[PW_LMT_PH_2T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_2T, ch - 4) / 2;

	lmt->pwr_lmt_40m[PW_LMT_PH_2T][1][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_2T, ch + 4) / 2;

	/*2Tx 40m BF*/
	lmt->pwr_lmt_40m[PW_LMT_PH_2T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_2T, ch - 4) / 2;

	lmt->pwr_lmt_40m[PW_LMT_PH_2T][1][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_2T, ch + 4) / 2;

	/*2Tx 80m NOBF*/
	lmt->pwr_lmt_80m[PW_LMT_PH_2T][0][PW_LMT_NONBF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_80M, PW_LMT_NONBF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx 80m BF*/
	lmt->pwr_lmt_80m[PW_LMT_PH_2T][0][PW_LMT_BF] =
		halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_80M, PW_LMT_BF, PW_LMT_PH_2T, ch) / 2;

	/*2Tx 40m 0p5 NOBF*/
	tmp1 = halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_2T, ch - 4) / 2;
	tmp2 = halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_NONBF, PW_LMT_PH_2T, ch + 4) / 2;
	if (tmp1 >= tmp2)
		tmp = tmp2;
	else
		tmp = tmp1;

	lmt->pwr_lmt_40m_0p5[PW_LMT_PH_2T][PW_LMT_NONBF] = tmp;

	/*2Tx 40m 0p5 BF*/
	tmp1 = halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_2T, ch - 4) / 2;
	tmp2 = halrf_get_power_limit(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_BW_40M, PW_LMT_BF, PW_LMT_PH_2T, ch + 4) / 2;
	if (tmp1 >= tmp2)
		tmp = tmp2;
	else
		tmp = tmp1;

	lmt->pwr_lmt_40m_0p5[PW_LMT_PH_2T][PW_LMT_BF] = tmp;
}

bool halrf_set_power_limit_to_struct_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_pwr_imt_info *lmt = &rf->hal_com->band[phy].rtw_tpu_i.rtw_tpu_pwr_imt_i;
	u8 bw = rf->hal_com->band[phy].cur_chandef.bw;
	u32 i;
	s8 tmp;

	RF_DBG(rf, DBG_RF_POWER, "======>%s   bandwidth=%d\n",
		__func__, bw);

	if (bw == CHANNEL_WIDTH_20)
		halrf_get_power_limit_to_struct_20m_8852b(rf, phy);
	else if (bw == CHANNEL_WIDTH_40)
		halrf_get_power_limit_to_struct_40m_8852b(rf, phy);
	else
		halrf_get_power_limit_to_struct_80m_8852b(rf, phy);

	tmp = lmt->pwr_lmt_cck_40m[0][0];

	for (i = 0; i < 2; i++) {
		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d   %d\n",
			lmt->pwr_lmt_cck_40m[i][1], lmt->pwr_lmt_cck_40m[i][0],
			lmt->pwr_lmt_cck_20m[i][1], lmt->pwr_lmt_cck_20m[i][0]);

		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d   %d\n",
			lmt->pwr_lmt_20m[i][0][1], lmt->pwr_lmt_20m[i][0][0],
			lmt->pwr_lmt_lgcy_20m[i][1], lmt->pwr_lmt_lgcy_20m[i][0]);

		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d   %d\n",
			lmt->pwr_lmt_20m[i][2][1], lmt->pwr_lmt_20m[i][2][0],
			lmt->pwr_lmt_20m[i][1][1], lmt->pwr_lmt_20m[i][1][0]);

		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d	 %d\n",
			lmt->pwr_lmt_20m[i][4][1], lmt->pwr_lmt_20m[i][4][0],
			lmt->pwr_lmt_20m[i][3][1], lmt->pwr_lmt_20m[i][3][0]);

		
		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d	 %d\n",
			lmt->pwr_lmt_20m[i][6][1], lmt->pwr_lmt_20m[i][6][0],
			lmt->pwr_lmt_20m[i][5][1], lmt->pwr_lmt_20m[i][5][0]);

		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d	 %d\n",
			lmt->pwr_lmt_40m[i][0][1], lmt->pwr_lmt_40m[i][0][0],
			lmt->pwr_lmt_20m[i][7][1], lmt->pwr_lmt_20m[i][7][0]);

		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d	 %d\n",
			lmt->pwr_lmt_40m[i][2][1], lmt->pwr_lmt_40m[i][2][0],
			lmt->pwr_lmt_40m[i][1][1], lmt->pwr_lmt_40m[i][1][0]);

		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d	 %d\n",
			lmt->pwr_lmt_80m[i][0][1], lmt->pwr_lmt_80m[i][0][0],
			lmt->pwr_lmt_40m[i][3][1], lmt->pwr_lmt_40m[i][3][0]);

		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d	 %d\n",
			lmt->pwr_lmt_160m[i][1], lmt->pwr_lmt_160m[i][0],
			lmt->pwr_lmt_80m[i][1][1], lmt->pwr_lmt_80m[i][1][0]);

		RF_DBG(rf, DBG_RF_POWER, "power_limit ==> %d   %d   %d	 %d\n",
			lmt->pwr_lmt_40m_2p5[i][1], lmt->pwr_lmt_40m_2p5[i][0],
			lmt->pwr_lmt_40m_0p5[i][1], lmt->pwr_lmt_40m_0p5[i][0]);
	}

	return true;
}

void halrf_get_power_limit_ru_to_struct_20m_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy].rtw_tpu_i;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u8 ch = rf->hal_com->band[phy].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_POWER, "======>%s channel=%d\n", __func__, ch);

	hal_mem_set(hal, tpu->pwr_lmt_ru, 0, sizeof(tpu->pwr_lmt_ru));

	/*1Tx RU26*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU26][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_1T, ch) / 2;

	/*1Tx RU52*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU52][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_1T, ch) / 2;

	/*1Tx RU106*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU106][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_1T, ch) / 2;

	/*2Tx RU26*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU26][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_2T, ch) / 2;

	/*2Tx RU52*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU52][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_2T, ch) / 2;

	/*2Tx RU106*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU106][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_2T, ch) / 2;
}

void halrf_get_power_limit_ru_to_struct_40m_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy].rtw_tpu_i;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u8 ch = rf->hal_com->band[phy].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_POWER, "======>%s channel=%d\n", __func__, ch);

	hal_mem_set(hal, tpu->pwr_lmt_ru, 0, sizeof(tpu->pwr_lmt_ru));

	/*1Tx RU26*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU26][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_1T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU26][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_1T, ch + 2) / 2;

	/*1Tx RU52*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU52][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_1T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU52][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_1T, ch + 2) / 2;

	/*1Tx RU106*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU106][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_1T, ch - 2) / 2;
	
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU106][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_1T, ch + 2) / 2;

	/*2TX RU26*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU26][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_2T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU26][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_2T, ch + 2) / 2;

	/*2TX RU52*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU52][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_2T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU52][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_2T, ch + 2) / 2;

	/*2TX RU106*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU106][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_2T, ch - 2) / 2;
	
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU106][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_2T, ch + 2) / 2;
}

void halrf_get_power_limit_ru_to_struct_80m_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy].rtw_tpu_i;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u8 ch = rf->hal_com->band[phy].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_POWER, "======>%s channel=%d\n", __func__, ch);

	hal_mem_set(hal, tpu->pwr_lmt_ru, 0, sizeof(tpu->pwr_lmt_ru));

	/*1TX RU26*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU26][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_1T, ch - 6) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU26][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_1T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU26][2] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_1T, ch + 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU26][3] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_1T, ch + 6) / 2;

	/*1TX RU52*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU52][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_1T, ch - 6) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU52][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_1T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU52][2] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_1T, ch + 2) / 2;
	
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU52][3] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_1T, ch + 6) / 2;

	/*1TX RU106*/
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU106][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_1T, ch - 6) / 2;
	
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU106][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_1T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU106][2] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_1T, ch + 2) / 2;
	
	tpu->pwr_lmt_ru[PW_LMT_PH_1T][PW_LMT_RU_BW_RU106][3] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_1T, ch + 6) / 2;

	/*2TX RU26*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU26][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_2T, ch - 6) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU26][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_2T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU26][2] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_2T, ch + 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU26][3] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU26, PW_LMT_PH_2T, ch + 6) / 2;

	/*2TX RU52*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU52][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_2T, ch - 6) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU52][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_2T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU52][2] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_2T, ch + 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU52][3] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU52, PW_LMT_PH_2T, ch + 6) / 2;

	/*2TX RU106*/
	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU106][0] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_2T, ch - 6) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU106][1] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_2T, ch - 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU106][2] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_2T, ch + 2) / 2;

	tpu->pwr_lmt_ru[PW_LMT_PH_2T][PW_LMT_RU_BW_RU106][3] =
		halrf_get_power_limit_ru(rf, phy, RF_PATH_A, RTW_DATA_RATE_HE_NSS1_MCS7, PW_LMT_RU_BW_RU106, PW_LMT_PH_2T, ch + 6) / 2;
}

bool halrf_set_power_limit_ru_to_struct_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy].rtw_tpu_i;
	u8 bw = rf->hal_com->band[phy].cur_chandef.bw;
	u8 i, j, k;

	RF_DBG(rf, DBG_RF_POWER, "======>%s   bandwidth=%d\n",
		__func__, bw);

	if (bw == CHANNEL_WIDTH_20)
		halrf_get_power_limit_ru_to_struct_20m_8852b(rf, phy);
	else if (bw == CHANNEL_WIDTH_40) {
		halrf_get_power_limit_ru_to_struct_40m_8852b(rf, phy);
	} else if (bw == CHANNEL_WIDTH_80) {
		halrf_get_power_limit_ru_to_struct_80m_8852b(rf, phy);
	}

	tpu->pwr_lmt_ru_mem_size = tpu->pwr_lmt_ru_mem_size;

	for (k = 0; k < 2; k++) {
		for (j = 0; j < PW_LMT_RU_BW_NULL; j++) {
			for (i = 0; i < 8; i = i + 4) {
				RF_DBG(rf, DBG_RF_POWER,
					"power_limit_ru %dTx ==> %d   %d   %d   %d\n",
					k + 1,
					tpu->pwr_lmt_ru[k][j][i + 3],
					tpu->pwr_lmt_ru[k][j][i + 2],
					tpu->pwr_lmt_ru[k][j][i + 1],
					tpu->pwr_lmt_ru[k][j][i]);
			}
		}
	}
	return true;
}

void _halrf_set_tx_shape_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy].rtw_tpu_i;

	u8 ch = rf->hal_com->band[phy].cur_chandef.center_ch;
	u8 reg;

	RF_DBG(rf, DBG_RF_POWER, "======>%s   channel=%d\n", __func__, ch);

	if (!(rf->support_ability & HAL_RF_TX_SHAPE)) {
		RF_DBG(rf, DBG_RF_POWER, "======>%s   rf->support_ability & HAL_RF_TX_SHAPE=%d   Return!!!\n",
			__func__, rf->support_ability & HAL_RF_TX_SHAPE);
		return;
	}

	if (pwr->set_tx_ptrn_shap_en == true) {
		RF_DBG(rf, DBG_RF_POWER, "======>%s   pwr->set_tx_ptrn_shap_en == true   Return!!!\n",
			__func__);
		return;
	} else {
		RF_DBG(rf, DBG_RF_POWER, "======>%s   pwr->set_tx_ptrn_shap_en == false  Set TX shape Default\n",
			__func__);
	}

	if (ch >= 1 && ch <= 14) {
		reg = halrf_get_regulation_info(rf, BAND_ON_24G);

		RF_DBG(rf, DBG_RF_POWER, "======>%s   channel=%d   regulation=%d\n", __func__, ch, reg);
		RF_DBG(rf, DBG_RF_POWER, "tpu->tx_ptrn_shap_idx=%d   pwr->tx_shap_idx[%d][CCK][%d]=%d   pwr->tx_shap_idx[%d][OFDM][%d]=%d\n",
			tpu->tx_ptrn_shap_idx, PW_LMT_BAND_2_4G, reg,
			pwr->tx_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_CCK][reg],
			PW_LMT_BAND_2_4G, reg,
			pwr->tx_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_OFDM][reg]);

		tpu->tx_ptrn_shap_idx = pwr->tx_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_CCK][reg];
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_CCK] =
			pwr->tx_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_CCK][reg];

		halrf_bb_set_pow_patten_sharp(rf, ch, true,
			pwr->tx_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_CCK][reg], phy);	/*cck*/

		RF_DBG(rf, DBG_RF_POWER, "[TX shape] tpu->tx_ptrn_shap_idx=%d  channel=%d  Set CCK Tx Shape!!!\n",
			tpu->tx_ptrn_shap_idx, ch);

		tpu->tx_ptrn_shap_idx = pwr->tx_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_OFDM][reg];

		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_OFDM] =
			pwr->tx_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_OFDM][reg];

		halrf_bb_set_pow_patten_sharp(rf, ch, false,
			pwr->tx_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_OFDM][reg], phy);	/*ofdm*/

		RF_DBG(rf, DBG_RF_POWER, "[TX shape] tpu->tx_ptrn_shap_idx=%d  channel=%d  Set OFDM Tx Shape!!!\n",
			tpu->tx_ptrn_shap_idx, ch);
	} else if (ch >= 36 && ch <= 177) {
		reg = halrf_get_regulation_info(rf, BAND_ON_5G);

		RF_DBG(rf, DBG_RF_POWER, "======>%s   channel=%d   regulation=%d\n", __func__, ch, reg);
		RF_DBG(rf, DBG_RF_POWER, "tpu->tx_ptrn_shap_idx=%d   pwr->tx_shap_idx[%d][OFDM][%d]=%d\n",
			tpu->tx_ptrn_shap_idx, PW_LMT_BAND_5G, reg,
			pwr->tx_shap_idx[PW_LMT_BAND_5G][TX_SHAPE_OFDM][reg]);

		tpu->tx_ptrn_shap_idx = pwr->tx_shap_idx[PW_LMT_BAND_5G][TX_SHAPE_OFDM][reg];

		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_5G][TX_SHAPE_OFDM] =
			pwr->tx_shap_idx[PW_LMT_BAND_5G][TX_SHAPE_OFDM][reg];

		halrf_bb_set_pow_patten_sharp(rf, ch, false,
			pwr->tx_shap_idx[PW_LMT_BAND_5G][TX_SHAPE_OFDM][reg], phy);	/*ofdm*/

		RF_DBG(rf, DBG_RF_POWER, "[TX shape] tpu->tx_ptrn_shap_idx=%d  channel=%d  Set Tx Shape!!!\n",
			tpu->tx_ptrn_shap_idx, ch);
	} else {
#if 0
		reg = halrf_get_regulation_info(rf, BAND_ON_6G);
		pwr->tx_shap_idx[PW_LMT_BAND_6G][reg];
#endif
	}
}

bool _halrf_set_power_8852b(struct rf_info *rf, enum phl_phy_idx phy,
	enum phl_pwr_table pwr_table)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u8 i;

	RF_DBG(rf, DBG_RF_POWER, "======>%s phl_pwr_table=%d\n", __func__, pwr_table);

	RF_DBG(rf, DBG_RF_POWER, "======>%s From Driver pwr->pwr_by_rate_switch=%d\n",
			__func__, pwr->pwr_by_rate_switch);

	RF_DBG(rf, DBG_RF_POWER, "======>%s From Driver pwr->pwr_limit_switch=%d\n",
			__func__, pwr->pwr_limit_switch);

	RF_DBG(rf, DBG_RF_POWER, "======>%s From Efuse pwr->pwr_table_switch_efuse=%d\n",
			__func__, pwr->pwr_table_switch_efuse);

	if (pwr_table & PWR_BY_RATE) {
		if (pwr->pwr_by_rate_switch == PW_BY_RATE_ALL_SAME) {
			halrf_set_power_by_rate_all_the_smae_to_struct_8852b(rf, phy);
		} else
			halrf_set_power_by_rate_to_struct_8852b(rf, phy);

		if (pwr->pwr_limit_switch == PWLMT_BY_EFUSE) {
			if (pwr->pwr_table_switch_efuse == 0) {
				halrf_set_power_by_rate_to_struct_8852b(rf, phy);
			} else if (pwr->pwr_table_switch_efuse == 2) {
				halrf_set_power_by_rate_all_the_smae_to_struct_8852b(rf, phy);
			} else {
				halrf_set_power_by_rate_to_struct_8852b(rf, phy);
			}
		}

		halrf_mac_write_pwr_by_rate_reg(rf, phy);
		halrf_mac_write_pwr_ofst_mode(rf, phy);

		if (rf->dbg_component & DBG_RF_POWER) {
			halrf_delay_ms(rf, 100);

			for (i = 0; i < TX_PWR_BY_RATE_NUM_MAC; i = i + 4)
				halrf_mac_get_pwr_reg_8852b(rf, phy, (0xd2c0 + i), 0xffffffff);
		}
	}

	if (pwr_table & PWR_LIMIT) {
		_halrf_set_tx_shape_8852b(rf, phy);
		if (!halrf_set_power_limit_to_struct_8852b(rf, phy)) {
			RF_DBG(rf, DBG_RF_POWER, "halrf_set_power_limit_to_struct_8852b return fail\n");
			return false;
		}

		halrf_mac_write_pwr_limit_reg(rf, phy);

		if (rf->dbg_component & DBG_RF_POWER) {
			halrf_delay_ms(rf, 100);

			for (i = 0; i < TX_PWR_LIMIT_NUM_MAC; i = i + 4)
				halrf_mac_get_pwr_reg_8852b(rf, phy, (0xd2ec + i), 0xffffffff);
		}
	}

	if (pwr_table & PWR_LIMIT_RU) {
		_halrf_set_tx_shape_8852b(rf, phy);
		if (!halrf_set_power_limit_ru_to_struct_8852b(rf, phy)) {
			RF_DBG(rf, DBG_RF_POWER, "halrf_set_power_limit_ru_to_struct_8852b return fail\n");
			return false;
		}

		halrf_mac_write_pwr_limit_rua_reg(rf, phy);

		if (rf->dbg_component & DBG_RF_POWER) {
			halrf_delay_ms(rf, 100);

			for (i = 0; i < 48; i = i + 4)
				halrf_mac_get_pwr_reg_8852b(rf, phy, (0xd33c + i), 0xffffffff);
		}
	}

	return true;
}

void halrf_set_ref_power_to_struct_8852b(struct rf_info *rf, enum phl_phy_idx phy)
{
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy].rtw_tpu_i;
	u8 rfe_type = rf->phl_com->dev_cap.rfe_type;

	RF_DBG(rf, DBG_RF_POWER, "======>%s\n", __func__);

	tpu->ofst_int = 0;
	tpu->ofst_fraction = 0;

	/*Set ref power*/
	if (rfe_type > 50)
		tpu->base_cw_0db = 0x21;
	else
		tpu->base_cw_0db = 0x27;

	tpu->tssi_16dBm_cw = 0x12c;

	/*[Ref Pwr]*/
	tpu->ref_pow_ofdm = 0;		/*0dBm*/
	tpu->ref_pow_cck = 0;		/*0dBm*/

	halrf_bb_set_tx_pow_ref(rf, phy);
}


bool halrf_set_power_8852b(struct rf_info *rf, enum phl_phy_idx phy,
	enum phl_pwr_table pwr_table)
{
#if 0
	if (rf->is_coex == true) {
		RF_DBG(rf, DBG_RF_POWER, "===>%s rf->is_coex==true return!!!\n", __func__);
		return true;
	}
#endif

	if (!_halrf_set_power_8852b(rf, phy, pwr_table)) {
		RF_DBG(rf, DBG_RF_POWER, "_halrf_set_power_8852b return fail\n");
		return false;
	}

	return true;
}

void halrf_pwr_by_rate_info_8852b(struct rf_info *rf,
		char input[][16], u32 *_used, char *output, u32 *_out_len)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u8 channel = rf->hal_com->band[0].cur_chandef.center_ch;
	u32 bw =  rf->hal_com->band[0].cur_chandef.bw;
	u32 band = rf->hal_com->band[0].cur_chandef.band;
	u32 reg_tmp, cck_ref, ofdm_ref;
	s32 s_cck_ref, s_ofdm_ref;
	s32 int_tmp[2], float_tmp[2];

	u32 used = *_used;
	u32 out_len = *_out_len;

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d\n",
		 "RF Para Ver", RF_RELEASE_VERSION_8852B);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d\n",
		 "RFE type", rf->phl_com->dev_cap.rfe_type);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s / %d / %s\n",
		 "Band / CH / BW", band == BAND_ON_24G ? "2G" : (band == BAND_ON_5G ? "5G" : "6G"),
		 channel,
		 bw == 0 ? "20M" : (bw == 1 ? "40M" : "80M"));

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s / %s\n",
		 "Driver Swith / Efuse Switch",
		 (pwr->pwr_limit_switch == PWLMT_DISABLE) ? "Disable Limit" :
		 (pwr->pwr_limit_switch == PWBYRATE_AND_PWLMT) ? "Enable Limit" : "From Efuse",
		 (pwr->pwr_limit_switch == PWLMT_BY_EFUSE) ?
		 ((pwr->pwr_table_switch_efuse == 0) ? "Disable Limit" : (pwr->pwr_table_switch_efuse == 2) ? "The Same" : "Enable Limit")
		 : "From Driver");

	cck_ref = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd200, 0x0007fc00);
	ofdm_ref = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd200, 0x0ff80000);
	(cck_ref & BIT(8)) ? (s_cck_ref = cck_ref | 0xfffffe00) : (s_cck_ref = cck_ref);
	(ofdm_ref & BIT(8)) ? (s_ofdm_ref = ofdm_ref | 0xfffffe00) : (s_ofdm_ref = ofdm_ref);
	int_tmp[0] = s_cck_ref / 4;
	float_tmp[0] = s_cck_ref * 100 / 4 % 100;
	float_tmp[0] < 0 ? float_tmp[0] = float_tmp[0] * -1 : 0;
	int_tmp[1] = s_ofdm_ref / 4;
	float_tmp[1] = s_ofdm_ref * 100 / 4 % 100;
	float_tmp[1] < 0 ? float_tmp[1] = float_tmp[1] * -1 : 0;
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s%d.%ddB / %s%d.%ddB\n",
		 "CCK REF / OFDM REF",
		 (int_tmp[0] == 0 && s_cck_ref < 0) ? "-" : "",
		 int_tmp[0], float_tmp[0],
		 (int_tmp[1] == 0 && s_ofdm_ref < 0) ? "-" : "",
		 int_tmp[1], float_tmp[1]);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, "1SS\n");

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2c0, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "CCK 11M / 5.5M / 2M / 1M",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2c4, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "OFDM 18M / 12M / 9M / 6M",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2c8, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "OFDM 54M / 48M / 36M / 24M",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s\n",
		 "HT / VHT / HE");

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2cc, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "MCS3 / MCS2 / MCS1 / MCS0",
		((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2d0, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "MCS7 / MCS6 / MCS5 / MCS4",
		((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2d4, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "MCS11 / MCS10 / MCS9 / MCS8",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2d8, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "DCM MCS4 / MCS3 / MCS1 / MCS0",
		((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, "2SS\n");

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s\n",
		 "HT / VHT / HE");

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2dc, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "MCS3 / MCS2 / MCS1 / MCS0",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2e0, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "MCS7 / MCS6 / MCS5 / MCS4",
		((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2e4, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "MCS11 / MCS10 / MCS9 / MCS8",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2e8, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-36s = %d.%d / %d.%d / %d.%d / %d.%d\n",
		 "DCM MCS4 / MCS3 / MCS1 / MCS0",
		((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f00) >> 8) / 2, ((reg_tmp & 0x7f00) >> 8) * 10 / 2 % 10,
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10
		 );

	*_used = used;
	*_out_len = out_len;

}

void halrf_pwr_limit_info_8852b(struct rf_info *rf,
		char input[][16], u32 *_used, char *output, u32 *_out_len)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u8 channel = rf->hal_com->band[0].cur_chandef.center_ch;
	u32 bw =  rf->hal_com->band[0].cur_chandef.bw;
	u32 band = rf->hal_com->band[0].cur_chandef.band;
	u32 reg_tmp, cck_ref, ofdm_ref;
	s32 s_cck_ref, s_ofdm_ref;
	s32 int_tmp[2], float_tmp[2];

	u32 used = *_used;
	u32 out_len = *_out_len;

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d\n",
		 "RF Para Ver", RF_RELEASE_VERSION_8852B);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d\n",
		 "RFE type", rf->phl_com->dev_cap.rfe_type);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s / %d / %s\n",
		 "Band / CH / BW", band == BAND_ON_24G ? "2G" : (band == BAND_ON_5G ? "5G" : "6G"),
		 channel,
		 bw == 0 ? "20M" : (bw == 1 ? "40M" : "80M"));
	
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s / %s / %s\n",
		 "Regulation 2G / 5G / 6G",
		 halrf_get_pw_lmt_regu_type_str(rf, BAND_ON_24G),
		 halrf_get_pw_lmt_regu_type_str(rf, BAND_ON_5G),
		 halrf_get_pw_lmt_regu_type_str(rf, BAND_ON_6G));

	cck_ref = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd200, 0x0007fc00);
	ofdm_ref = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd200, 0x0ff80000);
	(cck_ref & BIT(8)) ? (s_cck_ref = cck_ref | 0xfffffe00) : (s_cck_ref = cck_ref);
	(ofdm_ref & BIT(8)) ? (s_ofdm_ref = ofdm_ref | 0xfffffe00) : (s_ofdm_ref = ofdm_ref);
	int_tmp[0] = s_cck_ref / 4;
	float_tmp[0] = s_cck_ref * 100 / 4 % 100;
	float_tmp[0] < 0 ? float_tmp[0] = float_tmp[0] * -1 : 0;
	int_tmp[1] = s_ofdm_ref / 4;
	float_tmp[1] = s_ofdm_ref * 100 / 4 % 100;
	float_tmp[1] < 0 ? float_tmp[1] = float_tmp[1] * -1 : 0;
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s%d.%ddB / %s%d.%ddB\n",
		 "CCK REF / OFDM REF",
		 (int_tmp[0] == 0 && s_cck_ref < 0) ? "-" : "",
		 int_tmp[0], float_tmp[0],
		 (int_tmp[1] == 0 && s_ofdm_ref < 0) ? "-" : "",
		 int_tmp[1], float_tmp[1]);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s\n",
		 "Power Limit (Reg)",
		 halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd208, BIT(21)) ? "Enable Limit" : "Disable Limit");

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s / %s\n",
		 "Driver Swith / Efuse Switch",
		 (pwr->pwr_limit_switch == PWLMT_DISABLE) ? "Disable Limit" :
		 (pwr->pwr_limit_switch == PWBYRATE_AND_PWLMT) ? "Enable Limit" : "From Efuse",
		 (pwr->pwr_limit_switch == PWLMT_BY_EFUSE) ?
		 ((pwr->pwr_table_switch_efuse == 0) ? "Disable Limit" : (pwr->pwr_table_switch_efuse == 2) ? "The Same" : "Enable Limit")
		 : "From Driver");

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d / %d / %d\n",
		"TX Shape CCK / 2G / 5G",
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_CCK],
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_OFDM],
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_5G][TX_SHAPE_OFDM]);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, "1SS\n");

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2ec, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "CCK 20M NOBF", (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "CCK 40M NOBF", ((reg_tmp & 0x7f0000) >> 16) / 2,
		 ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd2f0, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "OFDM NOBF", (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "HT 20M NOBF", ((reg_tmp & 0x7f0000) >> 16) / 2,
		 ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd300, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "HT 40M NOBF", ((reg_tmp & 0x7f0000) >> 16) / 2,
		 ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd308, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "HT 80M NOBF", ((reg_tmp & 0x7f0000) >> 16) / 2,
		 ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, "2SS\n");

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd314, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "CCK 20M NOBF",
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "CCK 40M NOBF",
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd318, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "OFDM NOBF",
		 (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d / %d.%d\n",
		 "HT 20M BF / NOBF",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd328, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d / %d.%d\n",
		 "HT 40M BF / NOBF",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd330, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d / %d.%d\n",
		 "HT 80M BF / NOBF",
		 ((reg_tmp & 0x7f000000) >> 24) / 2, ((reg_tmp & 0x7f000000) >> 24) * 10 / 2 % 10,
		 ((reg_tmp & 0x7f0000) >> 16) / 2, ((reg_tmp & 0x7f0000) >> 16) * 10 / 2 % 10);

	*_used = used;
	*_out_len = out_len;

}

void halrf_pwr_limit_ru_info_8852b(struct rf_info *rf,
		char input[][16], u32 *_used, char *output, u32 *_out_len)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u8 channel = rf->hal_com->band[0].cur_chandef.center_ch;
	u32 bw =  rf->hal_com->band[0].cur_chandef.bw;
	u32 band = rf->hal_com->band[0].cur_chandef.band;
	u32 reg_tmp, cck_ref, ofdm_ref;
	s32 s_cck_ref, s_ofdm_ref;
	s32 int_tmp[2], float_tmp[2];

	u32 used = *_used;
	u32 out_len = *_out_len;

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d\n",
		 "RF Para Ver", RF_RELEASE_VERSION_8852B);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d\n",
		 "RFE type", rf->phl_com->dev_cap.rfe_type);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s / %d / %s\n",
		 "Band / CH / BW", band == BAND_ON_24G ? "2G" : (band == BAND_ON_5G ? "5G" : "6G"),
		 channel,
		 bw == 0 ? "20M" : (bw == 1 ? "40M" : "80M"));
	
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s / %s / %s\n",
		 "Regulation 2G / 5G / 6G",
		 halrf_get_pw_lmt_regu_type_str(rf, BAND_ON_24G),
		 halrf_get_pw_lmt_regu_type_str(rf, BAND_ON_5G),
		 halrf_get_pw_lmt_regu_type_str(rf, BAND_ON_6G));

	cck_ref = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd200, 0x0007fc00);
	ofdm_ref = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd200, 0x0ff80000);
	(cck_ref & BIT(8)) ? (s_cck_ref = cck_ref | 0xfffffe00) : (s_cck_ref = cck_ref);
	(ofdm_ref & BIT(8)) ? (s_ofdm_ref = ofdm_ref | 0xfffffe00) : (s_ofdm_ref = ofdm_ref);
	int_tmp[0] = s_cck_ref / 4;
	float_tmp[0] = s_cck_ref * 100 / 4 % 100;
	float_tmp[0] < 0 ? float_tmp[0] = float_tmp[0] * -1 : 0;
	int_tmp[1] = s_ofdm_ref / 4;
	float_tmp[1] = s_ofdm_ref * 100 / 4 % 100;
	float_tmp[1] < 0 ? float_tmp[1] = float_tmp[1] * -1 : 0;
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s%d.%ddB / %s%d.%ddB\n",
		 "CCK REF / OFDM REF",
		 (int_tmp[0] == 0 && s_cck_ref < 0) ? "-" : "",
		 int_tmp[0], float_tmp[0],
		 (int_tmp[1] == 0 && s_ofdm_ref < 0) ? "-" : "",
		 int_tmp[1], float_tmp[1]);

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s\n",
		 "Power Limit (Reg)",
		 halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd208, BIT(20)) ? "Enable Limit" : "Disable Limit");

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %s / %s\n",
		 "Driver Swith / Efuse Switch",
		 (pwr->pwr_limit_switch == PWLMT_DISABLE) ? "Disable Limit" :
		 (pwr->pwr_limit_switch == PWBYRATE_AND_PWLMT) ? "Enable Limit" : "From Efuse",
		 (pwr->pwr_limit_switch == PWLMT_BY_EFUSE) ?
		 ((pwr->pwr_table_switch_efuse == 0) ? "Disable Limit" : (pwr->pwr_table_switch_efuse == 2) ? "The Same" : "Enable Limit")
		 : "From Driver");

	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d / %d / %d\n",
		"TX Shape CCK / 2G / 5G",
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_CCK],
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_OFDM],
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_5G][TX_SHAPE_OFDM]);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd33c, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "RU26", (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd344, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "RU52", (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10);

	reg_tmp = halrf_mac_get_pwr_reg_8852b(rf, 0, 0xd34c, 0xffffffff);
	RF_DBG_CNSL(out_len, used, output + used, out_len - used, " %-30s = %d.%d\n",
		 "RU106", (reg_tmp & 0x7f) / 2, (reg_tmp & 0x7f) * 10 / 2 % 10);

	*_used = used;
	*_out_len = out_len;

}

void halrf_set_tx_shape_8852b(struct rf_info *rf, u8 tx_shape_idx)
{
	struct rtw_tpu_info *tpu = &rf->hal_com->band[0].rtw_tpu_i;
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u8 ch = rf->hal_com->band[0].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_POWER, "======>%s   ch=%d   tx_shape_idx=%d\n",
		__func__, ch, tx_shape_idx);

	tpu->tx_ptrn_shap_idx = tx_shape_idx;

	if (tx_shape_idx == 255) {
		pwr->set_tx_ptrn_shap_en = false;
		_halrf_set_tx_shape_8852b(rf, 0);
		return;
	} else
		pwr->set_tx_ptrn_shap_en = true;

	if (ch <= 14) {
		halrf_bb_set_pow_patten_sharp(rf, ch, true, tx_shape_idx, 0);	/*cck*/
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_CCK] = tx_shape_idx;
	}

	halrf_bb_set_pow_patten_sharp(rf, ch, false, tx_shape_idx, 0);	/*ofdm*/

	if (ch >= 1 && ch <= 14)
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_2_4G][TX_SHAPE_OFDM] = tx_shape_idx;
	else
		pwr->set_tx_ptrn_shap_idx[PW_LMT_BAND_5G][TX_SHAPE_OFDM] = tx_shape_idx;
}
#endif	/*RF_8852B_SUPPORT*/
