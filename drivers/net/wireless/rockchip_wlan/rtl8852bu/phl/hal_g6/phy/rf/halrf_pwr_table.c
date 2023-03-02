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
#include "halrf_precomp.h"

const char * const _pw_lmt_regu_type_str[PW_LMT_MAX_REGULATION_NUM] = {
	/* elements not listed here will get NULL */
	[PW_LMT_REGU_WW13]		= "WW",
	[PW_LMT_REGU_ETSI]		= "ETSI",
	[PW_LMT_REGU_FCC]		= "FCC",
	[PW_LMT_REGU_MKK]		= "MKK",
	[PW_LMT_REGU_NA]		= "NONE",
	[PW_LMT_REGU_IC]		= "IC",
	[PW_LMT_REGU_KCC]		= "KCC",
	[PW_LMT_REGU_ACMA]		= "ACMA",
	[PW_LMT_REGU_NCC]		= "NCC",
	[PW_LMT_REGU_MEXICO]		= "MEXICO",
	[PW_LMT_REGU_CHILE]		= "CHILE",
	[PW_LMT_REGU_UKRAINE]		= "UKRAINE",
	[PW_LMT_REGU_CN]		= "CN",
	[PW_LMT_REGU_QATAR]		= "QATAR",
	[PW_LMT_REGU_EXT_PWR]		= "EXT",
	[PW_LMT_REGU_NULL]		= NULL,
};

int halrf_get_predefined_pw_lmt_regu_type_from_str(const char *str)
{
	int i;

	for (i = 0; i < PW_LMT_MAX_REGULATION_NUM; i++)
		if (_pw_lmt_regu_type_str[i] && _os_strcmp(_pw_lmt_regu_type_str[i], str) == 0)
			return i;
	return -1;
}

const char * const *halrf_get_predefined_pw_lmt_regu_type_str_array(u8 *num)
{	
	if (num)
		*num = PW_LMT_REGU_PREDEF_NUM;
	return _pw_lmt_regu_type_str;
}

const enum halrf_pw_lmt_regulation_type _regulation_to_pw_lmt_regu_type[REGULATION_MAX] = {
	/* elements not listed here will get PW_LMT_REGU_WW13(0) */
	[REGULATION_WW]		= PW_LMT_REGU_WW13,
	[REGULATION_ETSI]	= PW_LMT_REGU_ETSI,
	[REGULATION_FCC]	= PW_LMT_REGU_FCC,
	[REGULATION_MKK]	= PW_LMT_REGU_MKK,
	[REGULATION_KCC]	= PW_LMT_REGU_KCC,
	[REGULATION_NCC]	= PW_LMT_REGU_NCC,
	[REGULATION_ACMA]	= PW_LMT_REGU_ACMA,
	[REGULATION_NA]		= PW_LMT_REGU_NA,
	[REGULATION_IC]		= PW_LMT_REGU_IC,
	[REGULATION_CHILE]	= PW_LMT_REGU_CHILE,
	[REGULATION_MEX]	= PW_LMT_REGU_MEXICO,
};

const enum halrf_pw_lmt_regulation_type _tpo_to_pw_lmt_regu_type[TPO_NA] = {
	/* elements not listed here will get PW_LMT_REGU_WW13(0) */
	[TPO_CHILE]		= PW_LMT_REGU_CHILE,
	[TPO_QATAR]		= PW_LMT_REGU_QATAR,
	[TPO_UKRAINE]		= PW_LMT_REGU_UKRAINE,
	[TPO_CN]		= PW_LMT_REGU_CN,
};

u8 halrf_get_regulation_info(struct rf_info *rf, u8 band)
{
	struct rtw_regulation_info rg_info = {0};
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	enum halrf_pw_lmt_regulation_type pw_lmt_type = PW_LMT_REGU_NULL;
	u8 reg = REGULATION_NA;
	u8 extra_regd_idx;
	const char *bstr =  NULL;

	halrf_query_regulation_info(rf, &rg_info);

	RF_DBG(rf, DBG_RF_INIT, "======>%s band=%d\n", __func__, band);

	RF_DBG(rf, DBG_RF_INIT, "domain_code=%d   regulation_2g=%d   regulation_5g=%d   chplan_ver=%d   country_ver=%d   rg_info.tpor=%d\n",
		rg_info.domain_code, rg_info.regulation_2g, rg_info.regulation_5g, rg_info.chplan_ver, rg_info.country_ver, rg_info.tpo);

	extra_regd_idx = halrf_get_power_limit_extra(rf);
	if (extra_regd_idx != 0xff)
		return extra_regd_idx;
	
#ifdef RTW_FLASH_98D
	if (pwr->regulation_idx != 0xff)
		return pwr->regulation_idx;
#else
	if (band == BAND_ON_24G) {
		reg = rg_info.regulation_2g;
		bstr = "2g";
	} else if (band == BAND_ON_5G) {
		reg = rg_info.regulation_5g;
		bstr = "5g";
	} else if (band == BAND_ON_6G) {
		/* TODO: 6G regulation */
		bstr = "6g";
	}

	/* TODO: consider _hal_file_regd_ext */

	if (rg_info.tpo != TPO_NA) {
		pw_lmt_type = tpo_to_pw_lmt_regu_type(rg_info.tpo);	
		RF_DBG(rf, DBG_RF_INIT, "%s extra pw_lmt_regu=%s(%d)\n",
			bstr, pw_lmt_regu_type_str(pw_lmt_type), pw_lmt_type);
	} else {
		if (reg == REGULATION_NA)
			return PW_LMT_REGU_NULL; /* return this to distinguish from PW_LMT_REGU_NA done by NONE option from TXPWR_LMT.txt */
		pw_lmt_type = regulation_to_pw_lmt_regu_type(reg);
	}

	if (pwr->regulation[band][pw_lmt_type] != true) {
		RF_DBG(rf, DBG_RF_INIT, "%s pw_lmt_regu=%s(%d) is not exist return WW13 !!!\n",
			bstr, pw_lmt_regu_type_str(pw_lmt_type), pw_lmt_type);
		return PW_LMT_REGU_WW13;
	}
#endif
	
	if (rf->phl_com->drv_mode == RTW_DRV_MODE_MP)
		return regulation_to_pw_lmt_regu_type(pwr->mp_regulation);
	
	return pw_lmt_type;
}

const char *halrf_get_pw_lmt_regu_type_str(struct rf_info *rf, u8 band)
{
	u8 reg = halrf_get_regulation_info(rf, band);

	return pw_lmt_regu_type_str(reg);
}

void halrf_power_by_rate_store_to_array(struct rf_info *rf,
			u32 band, u32 tx_num, u32 rate_id, u32 data)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	
	RF_DBG(rf, DBG_RF_INIT, "======>%s band=%d tx_num=%d rate_id=%d data=0x%x\n",
				__func__, band, tx_num, rate_id, data);

	if (band != (u32)BAND_ON_24G && band != (u32)BAND_ON_5G && band != (u32)BAND_ON_6G)
		RF_DBG(rf, DBG_RF_INIT, "Invalid Band %d\n", band);

	if (tx_num > 4 && tx_num != 15)
		RF_DBG(rf, DBG_RF_INIT, "Invalid TxNum %d\n", tx_num);

	if (band == 0 && tx_num == 0 && rate_id == 0) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_CCK1] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_CCK2] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_CCK5_5] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_CCK11] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 0 && rate_id == 1) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM6] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM9] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM12] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM18] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 0 && rate_id == 2) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM24] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM36] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM48] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM54] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 0 && rate_id == 3) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS0] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS1] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS2] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS3] = (s8)((data & 0xff000000) >> 24);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS0] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS1] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS2] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS3] = (s8)((data & 0xff000000) >> 24);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS0] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS1] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS2] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS3] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 0 && rate_id == 4) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS4] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS5] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS6] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS7] = (s8)((data & 0xff000000) >> 24);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS4] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS5] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS6] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS7] = (s8)((data & 0xff000000) >> 24);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS4] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS5] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS6] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS7] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 0 && rate_id == 5) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS8] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS1_MCS9] = (s8)((data & 0xff00) >> 8);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS8] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS9] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS10] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS1_MCS11] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 0 && rate_id == 6) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_NSS1_MCS0] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_NSS1_MCS1] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_NSS1_MCS3] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_NSS1_MCS4] = (s8)((data & 0xff000000) >> 24);
	} else if (band == 0 && tx_num == 15 && rate_id == 7) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_OFFSET] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HT_OFFSET] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM_OFFSET] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_CCK_OFFSET] = (s8)((data & 0xff000000) >> 24);
	} else if (band == 0 && tx_num == 15 && rate_id == 8) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_OFFSET] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 1 && rate_id == 3) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS8] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS9] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS10] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS11] = (s8)((data & 0xff000000) >> 24);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS0] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS1] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS2] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS3] = (s8)((data & 0xff000000) >> 24);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS0] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS1] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS2] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS3] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 1 && rate_id == 4) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS12] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS13] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS14] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_MCS15] = (s8)((data & 0xff000000) >> 24);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS4] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS5] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS6] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS7] = (s8)((data & 0xff000000) >> 24);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS4] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS5] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS6] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS7] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 1 && rate_id == 5) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS8] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_NSS2_MCS9] = (s8)((data & 0xff00) >> 8);

		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS8] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS9] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS10] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HE_NSS2_MCS11] = (s8)((data & 0xff000000) >> 24);
	} else if (tx_num == 1 && rate_id == 6) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_NSS2_MCS0] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_NSS2_MCS1] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_NSS2_MCS3] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_NSS2_MCS4] = (s8)((data & 0xff000000) >> 24);
	} else if (band == 1 && tx_num == 15 && rate_id == 9) {
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HEDCM_OFFSET] = (s8)(data & 0xff);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_VHT_OFFSET] = (s8)((data & 0xff00) >> 8);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_HT_OFFSET] = (s8)((data & 0xff0000) >> 16);
		pwr->tx_pwr_by_rate[band][HALRF_DATA_RATE_OFDM_OFFSET] = (s8)((data & 0xff000000) >> 24);
	}
}

u8 halrf_get_ch_idx_to_limit_array(struct rf_info *rf, u8 channel)
{
	u8	channelIndex;

	if (channel >= 1 && channel <= 14)
		channelIndex = channel - 1;
	else if (channel >= 36 && channel <= 64)
		channelIndex = (channel - 36) / 2;
	else if (channel >= 100 && channel <= 144)
		channelIndex = ((channel - 100) / 2) + 15;
	else if (channel >= 149 && channel <= 177)
		channelIndex = ((channel - 149) / 2) + 38;
	else
		channelIndex = 0;

	return channelIndex;
}

u8 halrf_get_limit_ch_idx_to_ch_idx(struct rf_info *rf, u8 band, u8 channel)
{
	u8	channelIndex;

	if (band == PW_LMT_BAND_2_4G) {
		if (channel >= 0 && channel <= 13)
			channelIndex = channel + 1;
		else
			channelIndex = 0;
	} else {
		if (channel >= 0 && channel <= 14)
			channelIndex = channel * 2 + 36;
		else if (channel >= 15 && channel <= 37)
			channelIndex = (channel - 15) * 2 + 100;
		else if (channel >= 38 && channel <= 52)
			channelIndex = (channel - 38) * 2 + 149;
		else
			channelIndex = 0;
	}

	return channelIndex;
}

u16 halrf_hw_rate_to_pwr_by_rate(struct rf_info *rf, u16 rate)
{
	u16 ret_rate = HALRF_DATA_RATE_OFDM6;

	switch(rate) {
	case RTW_DATA_RATE_CCK1:	ret_rate = HALRF_DATA_RATE_CCK1; break;
	case RTW_DATA_RATE_CCK2:	ret_rate = HALRF_DATA_RATE_CCK2; break;
	case RTW_DATA_RATE_CCK5_5:	ret_rate = HALRF_DATA_RATE_CCK5_5; break;
	case RTW_DATA_RATE_CCK11:	ret_rate = HALRF_DATA_RATE_CCK11; break;
	case RTW_DATA_RATE_OFDM6:	ret_rate = HALRF_DATA_RATE_OFDM6; break;
	case RTW_DATA_RATE_OFDM9:	ret_rate = HALRF_DATA_RATE_OFDM9; break;
	case RTW_DATA_RATE_OFDM12:	ret_rate = HALRF_DATA_RATE_OFDM12; break;
	case RTW_DATA_RATE_OFDM18:	ret_rate = HALRF_DATA_RATE_OFDM18; break;
	case RTW_DATA_RATE_OFDM24:	ret_rate = HALRF_DATA_RATE_OFDM24; break;
	case RTW_DATA_RATE_OFDM36:	ret_rate = HALRF_DATA_RATE_OFDM36; break;
	case RTW_DATA_RATE_OFDM48:	ret_rate = HALRF_DATA_RATE_OFDM48; break;
	case RTW_DATA_RATE_OFDM54:	ret_rate = HALRF_DATA_RATE_OFDM54; break;

	case RTW_DATA_RATE_MCS0:	ret_rate = HALRF_DATA_RATE_MCS0; break;
	case RTW_DATA_RATE_MCS1:	ret_rate = HALRF_DATA_RATE_MCS1; break;
	case RTW_DATA_RATE_MCS2:	ret_rate = HALRF_DATA_RATE_MCS2; break;
	case RTW_DATA_RATE_MCS3:	ret_rate = HALRF_DATA_RATE_MCS3; break;
	case RTW_DATA_RATE_MCS4:	ret_rate = HALRF_DATA_RATE_MCS4; break;
	case RTW_DATA_RATE_MCS5:	ret_rate = HALRF_DATA_RATE_MCS5; break;
	case RTW_DATA_RATE_MCS6:	ret_rate = HALRF_DATA_RATE_MCS6; break;
	case RTW_DATA_RATE_MCS7:	ret_rate = HALRF_DATA_RATE_MCS7; break;
	case RTW_DATA_RATE_MCS8:	ret_rate = HALRF_DATA_RATE_MCS8; break;
	case RTW_DATA_RATE_MCS9:	ret_rate = HALRF_DATA_RATE_MCS9; break;
	case RTW_DATA_RATE_MCS10:	ret_rate = HALRF_DATA_RATE_MCS10; break;
	case RTW_DATA_RATE_MCS11:	ret_rate = HALRF_DATA_RATE_MCS11; break;
	case RTW_DATA_RATE_MCS12:	ret_rate = HALRF_DATA_RATE_MCS12; break;
	case RTW_DATA_RATE_MCS13:	ret_rate = HALRF_DATA_RATE_MCS13; break;
	case RTW_DATA_RATE_MCS14:	ret_rate = HALRF_DATA_RATE_MCS14; break;
	case RTW_DATA_RATE_MCS15:	ret_rate = HALRF_DATA_RATE_MCS15; break;
	case RTW_DATA_RATE_MCS16:	ret_rate = HALRF_DATA_RATE_MCS16; break;
	case RTW_DATA_RATE_MCS17:	ret_rate = HALRF_DATA_RATE_MCS17; break;
	case RTW_DATA_RATE_MCS18:	ret_rate = HALRF_DATA_RATE_MCS18; break;
	case RTW_DATA_RATE_MCS19:	ret_rate = HALRF_DATA_RATE_MCS19; break;
	case RTW_DATA_RATE_MCS20:	ret_rate = HALRF_DATA_RATE_MCS20; break;
	case RTW_DATA_RATE_MCS21:	ret_rate = HALRF_DATA_RATE_MCS21; break;
	case RTW_DATA_RATE_MCS22:	ret_rate = HALRF_DATA_RATE_MCS22; break;
	case RTW_DATA_RATE_MCS23:	ret_rate = HALRF_DATA_RATE_MCS23; break;
	case RTW_DATA_RATE_MCS24:	ret_rate = HALRF_DATA_RATE_MCS24; break;
	case RTW_DATA_RATE_MCS25:	ret_rate = HALRF_DATA_RATE_MCS25; break;
	case RTW_DATA_RATE_MCS26:	ret_rate = HALRF_DATA_RATE_MCS26; break;
	case RTW_DATA_RATE_MCS27:	ret_rate = HALRF_DATA_RATE_MCS27; break;
	case RTW_DATA_RATE_MCS28:	ret_rate = HALRF_DATA_RATE_MCS28; break;
	case RTW_DATA_RATE_MCS29:	ret_rate = HALRF_DATA_RATE_MCS29; break;
	case RTW_DATA_RATE_MCS30:	ret_rate = HALRF_DATA_RATE_MCS30; break;
	case RTW_DATA_RATE_MCS31:	ret_rate = HALRF_DATA_RATE_MCS31; break;

	case RTW_DATA_RATE_VHT_NSS1_MCS0:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS0; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS1:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS1; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS2:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS2; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS3:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS3; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS4:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS4; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS5:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS5; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS6:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS6; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS7:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS7; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS8:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS8; break;
	case RTW_DATA_RATE_VHT_NSS1_MCS9:	ret_rate = HALRF_DATA_RATE_VHT_NSS1_MCS9; break;	
	case RTW_DATA_RATE_VHT_NSS2_MCS0:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS0; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS1:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS1; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS2:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS2; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS3:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS3; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS4:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS4; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS5:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS5; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS6:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS6; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS7:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS7; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS8:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS8; break;
	case RTW_DATA_RATE_VHT_NSS2_MCS9:	ret_rate = HALRF_DATA_RATE_VHT_NSS2_MCS9; break;	
	case RTW_DATA_RATE_VHT_NSS3_MCS0:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS0; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS1:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS1; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS2:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS2; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS3:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS3; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS4:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS4; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS5:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS5; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS6:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS6; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS7:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS7; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS8:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS8; break;
	case RTW_DATA_RATE_VHT_NSS3_MCS9:	ret_rate = HALRF_DATA_RATE_VHT_NSS3_MCS9; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS0:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS0; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS1:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS1; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS2:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS2; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS3:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS3; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS4:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS4; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS5:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS5; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS6:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS6; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS7:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS7; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS8:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS8; break;
	case RTW_DATA_RATE_VHT_NSS4_MCS9:	ret_rate = HALRF_DATA_RATE_VHT_NSS4_MCS9; break;

	case RTW_DATA_RATE_HE_NSS1_MCS0:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS0; break;
	case RTW_DATA_RATE_HE_NSS1_MCS1:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS1; break;
	case RTW_DATA_RATE_HE_NSS1_MCS2:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS2; break;
	case RTW_DATA_RATE_HE_NSS1_MCS3:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS3; break;
	case RTW_DATA_RATE_HE_NSS1_MCS4:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS4; break;
	case RTW_DATA_RATE_HE_NSS1_MCS5:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS5; break;
	case RTW_DATA_RATE_HE_NSS1_MCS6:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS6; break;
	case RTW_DATA_RATE_HE_NSS1_MCS7:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS7; break;
	case RTW_DATA_RATE_HE_NSS1_MCS8:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS8; break;
	case RTW_DATA_RATE_HE_NSS1_MCS9:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS9; break; 
	case RTW_DATA_RATE_HE_NSS1_MCS10:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS10; break;
	case RTW_DATA_RATE_HE_NSS1_MCS11:	ret_rate = HALRF_DATA_RATE_HE_NSS1_MCS11; break; 
	case RTW_DATA_RATE_HE_NSS2_MCS0:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS0; break;
	case RTW_DATA_RATE_HE_NSS2_MCS1:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS1; break;
	case RTW_DATA_RATE_HE_NSS2_MCS2:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS2; break;
	case RTW_DATA_RATE_HE_NSS2_MCS3:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS3; break;
	case RTW_DATA_RATE_HE_NSS2_MCS4:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS4; break;
	case RTW_DATA_RATE_HE_NSS2_MCS5:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS5; break;
	case RTW_DATA_RATE_HE_NSS2_MCS6:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS6; break;
	case RTW_DATA_RATE_HE_NSS2_MCS7:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS7; break;
	case RTW_DATA_RATE_HE_NSS2_MCS8:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS8; break;
	case RTW_DATA_RATE_HE_NSS2_MCS9:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS9; break;
	case RTW_DATA_RATE_HE_NSS2_MCS10:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS10; break;
	case RTW_DATA_RATE_HE_NSS2_MCS11:	ret_rate = HALRF_DATA_RATE_HE_NSS2_MCS11; break; 
	case RTW_DATA_RATE_HE_NSS3_MCS0:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS0; break;
	case RTW_DATA_RATE_HE_NSS3_MCS1:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS1; break;
	case RTW_DATA_RATE_HE_NSS3_MCS2:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS2; break;
	case RTW_DATA_RATE_HE_NSS3_MCS3:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS3; break;
	case RTW_DATA_RATE_HE_NSS3_MCS4:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS4; break;
	case RTW_DATA_RATE_HE_NSS3_MCS5:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS5; break;
	case RTW_DATA_RATE_HE_NSS3_MCS6:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS6; break;
	case RTW_DATA_RATE_HE_NSS3_MCS7:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS7; break;
	case RTW_DATA_RATE_HE_NSS3_MCS8:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS8; break;
	case RTW_DATA_RATE_HE_NSS3_MCS9:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS9; break;
	case RTW_DATA_RATE_HE_NSS3_MCS10:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS10; break;
	case RTW_DATA_RATE_HE_NSS3_MCS11:	ret_rate = HALRF_DATA_RATE_HE_NSS3_MCS11; break; 
	case RTW_DATA_RATE_HE_NSS4_MCS0:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS0; break;
	case RTW_DATA_RATE_HE_NSS4_MCS1:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS1; break;
	case RTW_DATA_RATE_HE_NSS4_MCS2:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS2; break;
	case RTW_DATA_RATE_HE_NSS4_MCS3:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS3; break;
	case RTW_DATA_RATE_HE_NSS4_MCS4:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS4; break;
	case RTW_DATA_RATE_HE_NSS4_MCS5:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS5; break;
	case RTW_DATA_RATE_HE_NSS4_MCS6:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS6; break;
	case RTW_DATA_RATE_HE_NSS4_MCS7:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS7; break;
	case RTW_DATA_RATE_HE_NSS4_MCS8:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS8; break;
	case RTW_DATA_RATE_HE_NSS4_MCS9:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS9; break;
	case RTW_DATA_RATE_HE_NSS4_MCS10:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS10; break;
	case RTW_DATA_RATE_HE_NSS4_MCS11:	ret_rate = HALRF_DATA_RATE_HE_NSS4_MCS11; break; 

	}

	return ret_rate;
}

u16 halrf_get_dcm_offset_pwr_by_rate(struct rf_info *rf, u16 rate,
						u8 dcm, u8 offset)
{
	u16 rate_tmp = 0;

	if (dcm == 0)
		rate_tmp = halrf_hw_rate_to_pwr_by_rate(rf, rate);
	else if (dcm == 1) {
		if (rate == RTW_DATA_RATE_HE_NSS1_MCS0)
			rate_tmp = HALRF_DATA_RATE_HEDCM_NSS1_MCS0;
		else if (rate == RTW_DATA_RATE_HE_NSS1_MCS1)
			rate_tmp = HALRF_DATA_RATE_HEDCM_NSS1_MCS1;
		else if (rate == RTW_DATA_RATE_HE_NSS1_MCS3)
			rate_tmp = HALRF_DATA_RATE_HEDCM_NSS1_MCS3;
		else if (rate == RTW_DATA_RATE_HE_NSS1_MCS4)
			rate_tmp = HALRF_DATA_RATE_HEDCM_NSS1_MCS4;
		else if (rate == RTW_DATA_RATE_HE_NSS2_MCS0)
			rate_tmp = HALRF_DATA_RATE_HEDCM_NSS2_MCS0;
		else if (rate == RTW_DATA_RATE_HE_NSS2_MCS1)
			rate_tmp = HALRF_DATA_RATE_HEDCM_NSS2_MCS1;
		else if (rate == RTW_DATA_RATE_HE_NSS2_MCS3)
			rate_tmp = HALRF_DATA_RATE_HEDCM_NSS2_MCS3;
		else if (rate == RTW_DATA_RATE_HE_NSS2_MCS4)
			rate_tmp = HALRF_DATA_RATE_HEDCM_NSS2_MCS4;
	}

	if (offset == 1) {
		/*if (dcm == 1)*/
		if (rate >= RTW_DATA_RATE_HE_NSS1_MCS0 && rate <= RTW_DATA_RATE_HE_NSS4_MCS11)
			rate_tmp = HALRF_DATA_RATE_HEDCM_OFFSET;
		if (rate >= RTW_DATA_RATE_VHT_NSS1_MCS0 && rate <= RTW_DATA_RATE_VHT_NSS4_MCS9)
			rate_tmp = HALRF_DATA_RATE_VHT_OFFSET;
		if (rate >= RTW_DATA_RATE_MCS0 && rate <= RTW_DATA_RATE_MCS31)
			rate_tmp = HALRF_DATA_RATE_HT_OFFSET;
		if (rate >= RTW_DATA_RATE_OFDM6 && rate <= RTW_DATA_RATE_OFDM54)
			rate_tmp = HALRF_DATA_RATE_OFDM_OFFSET;
		if (rate >= RTW_DATA_RATE_CCK1 && rate <= RTW_DATA_RATE_CCK11)
			rate_tmp = HALRF_DATA_RATE_CCK_OFFSET;
		
	}

	return rate_tmp;

}

u8 halrf_hw_rate_to_limit_rate_tx_num(struct rf_info *rf, u16 rate)
{
	if (rate >= RTW_DATA_RATE_CCK1 && rate <= RTW_DATA_RATE_CCK11)
		return PW_LMT_RS_CCK;
	else if (rate >= RTW_DATA_RATE_OFDM6 && rate <= RTW_DATA_RATE_OFDM54)
		return PW_LMT_RS_OFDM;
	else if (rate >= RTW_DATA_RATE_MCS0 && rate <= RTW_DATA_RATE_MCS7)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_MCS8 && rate <= RTW_DATA_RATE_MCS15)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_MCS16 && rate <= RTW_DATA_RATE_MCS23)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_MCS24 && rate <= RTW_DATA_RATE_MCS31)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_VHT_NSS1_MCS0 && rate <= RTW_DATA_RATE_VHT_NSS1_MCS9)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_VHT_NSS2_MCS0 && rate <= RTW_DATA_RATE_VHT_NSS2_MCS9)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_VHT_NSS3_MCS0 && rate <= RTW_DATA_RATE_VHT_NSS3_MCS9)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_VHT_NSS4_MCS0 && rate <= RTW_DATA_RATE_VHT_NSS4_MCS9)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_HE_NSS1_MCS0 && rate <= RTW_DATA_RATE_HE_NSS1_MCS11)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_HE_NSS2_MCS0 && rate <= RTW_DATA_RATE_HE_NSS2_MCS11)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_HE_NSS3_MCS0 && rate <= RTW_DATA_RATE_HE_NSS3_MCS11)
		return PW_LMT_RS_HE;
	else if (rate >= RTW_DATA_RATE_HE_NSS4_MCS0 && rate <= RTW_DATA_RATE_HE_NSS4_MCS11)
		return PW_LMT_RS_HE;
	else
		return PW_LMT_RS_HE;

}

void halrf_power_limit_store_to_array(struct rf_info *rf,
			u8 regulation, u8 band, u8 bandwidth, u8 rate,
			u8 tx_num, u8 beamforming, u8 chnl, s8 val)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u8 ch = halrf_get_ch_idx_to_limit_array(rf, chnl);

	RF_DBG(rf, DBG_RF_INIT, "======>%s regulation=%d band=%d bandwidth=%d rate=%d\n",
		__func__, regulation, band, bandwidth, rate);

	RF_DBG(rf, DBG_RF_INIT, "======>%s tx_num=%d beamforming=%d chnl=%d ch=%d val=%d\n",
		__func__, tx_num, beamforming, chnl, ch, val);

	if (band == PW_LMT_BAND_2_4G) {
		pwr->tx_pwr_limit_2g[regulation][bandwidth][rate][beamforming][ch][tx_num] = val;		
		RF_DBG(rf, DBG_RF_POWER, "pwr_limit(%d) = pwr->tx_pwr_limit_2g[%d][%d][%d][%d][%d][%d]\n",
			val, regulation, bandwidth, rate, beamforming, ch, tx_num);
	} else if (band == PW_LMT_BAND_5G) {
		pwr->tx_pwr_limit_5g[regulation][bandwidth][rate][beamforming][ch][tx_num] = val;
		RF_DBG(rf, DBG_RF_POWER, "pwr_limit(%d) = pwr->tx_pwr_limit_5g[%d][%d][%d][%d][%d][%d]\n",
			val, regulation, bandwidth, rate, beamforming, ch, tx_num);
	}
}

void halrf_power_limit_set_worldwide(struct rf_info *rf)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u32 reg, bw, rate, bf, ch, tx_num;
	s8 tmp;

	RF_DBG(rf, DBG_RF_INIT, "======>%s\n", __func__);

	for (tx_num = 0; tx_num < MAX_HALRF_PATH; tx_num++) {
		for (ch = 0; ch < PW_LMT_MAX_CHANNEL_NUMBER_2G; ch++) {
			for (bf = 0; bf < PW_LMT_MAX_BF_NUM; bf++) {
				for (rate = 0; rate < PW_LMT_MAX_RS_NUM; rate++) {
					for (bw = 0; bw < PW_LMT_MAX_2G_BANDWITH_NUM; bw++) {
						tmp = 127;
						for (reg = 0; reg < PW_LMT_MAX_REGULATION_NUM; reg++) {
							RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_2g[%d][%d][%d][%d][%d][%d]=   %d\n",
								reg, bw, rate, bf, ch, tx_num ,pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num]);
							if (tmp > pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num] &&
								pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num] > 0) {
								tmp = pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num];
							}
						}
						if (tmp == 127)
							tmp = 0;
						pwr->tx_pwr_limit_2g[PW_LMT_REGU_WW13][bw][rate][bf][ch][tx_num] = tmp;

						RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_2g[%d][%d][%d][%d][%d][%d]=   %d\n",
								PW_LMT_REGU_WW13, bw, rate, bf, ch, tx_num ,pwr->tx_pwr_limit_2g[PW_LMT_REGU_WW13][bw][rate][bf][ch][tx_num]);
					}
				}
			}
		}
	}

	for (tx_num = 0; tx_num < MAX_HALRF_PATH; tx_num++) {
		for (ch = 0; ch < PW_LMT_MAX_CHANNEL_NUMBER_5G; ch++) {
			for (bf = 0; bf < PW_LMT_MAX_BF_NUM; bf++) {
				for (rate = 0; rate < PW_LMT_MAX_RS_NUM; rate++) {
					for (bw = 0; bw < PW_LMT_MAX_BANDWIDTH_NUM; bw++) {
						tmp = 127;
						for (reg = 0; reg < PW_LMT_MAX_REGULATION_NUM; reg++) {
							RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_5g[%d][%d][%d][%d][%d][%d]=   %d\n",
								reg, bw, rate, bf, ch, tx_num, pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num]);
							if (tmp > pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num] &&
								pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num] > 0) {
								tmp = pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num];
							}
						}
						if (tmp == 127)
							tmp = 0;
						pwr->tx_pwr_limit_5g[PW_LMT_REGU_WW13][bw][rate][bf][ch][tx_num] = tmp;

						RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_5g[%d][%d][%d][%d][%d][%d]=   %d\n",
								PW_LMT_REGU_WW13, bw, rate, bf, ch, tx_num, pwr->tx_pwr_limit_5g[PW_LMT_REGU_WW13][bw][rate][bf][ch][tx_num]);
					}
				}
			}
		}
	}

	RF_DBG(rf, DBG_RF_INIT, "<======%s finish!!!\n", __func__);

}

void halrf_power_limit_ru_store_to_array(struct rf_info *rf,
			u8 band, u8 bandwidth, u8 tx_num, u8 rate,
			u8 regulation, u8 chnl, s8 val)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u8 ch = halrf_get_ch_idx_to_limit_array(rf, chnl);

	RF_DBG(rf, DBG_RF_INIT, "======>%s regulation=%d band=%d bandwidth=%d rate=%d\n",
		__func__, regulation, band, bandwidth, rate);

	RF_DBG(rf, DBG_RF_INIT, "======>%s tx_num=%d chnl=%d ch=%d val=%d\n",
		__func__, tx_num, chnl, ch, val);

	if (band == PW_LMT_BAND_2_4G) {
		pwr->tx_pwr_limit_ru_2g[regulation][bandwidth][rate][ch][tx_num] = val;		
		RF_DBG(rf, DBG_RF_POWER, "pwr_limit_ru(%d) = pwr->tx_pwr_limit_ru_2g[%d][%d][%d][%d][%d]\n",
			val, regulation, bandwidth, rate, ch, tx_num);
	} else if (band == PW_LMT_BAND_5G) {
		pwr->tx_pwr_limit_ru_5g[regulation][bandwidth][rate][ch][tx_num] = val;
		RF_DBG(rf, DBG_RF_POWER, "pwr_limit_ru(%d) = pwr->tx_pwr_limit_ru_5g[%d][%d][%d][%d][%d]\n",
			val, regulation, bandwidth, rate, ch, tx_num);
	}
}

void halrf_power_limit_ru_set_worldwide(struct rf_info *rf)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u32 reg, bw, rate, ch, tx_num;
	s8 tmp;

	RF_DBG(rf, DBG_RF_INIT, "======>%s\n", __func__);

	for (tx_num = 0; tx_num < MAX_HALRF_PATH; tx_num++) {
		for (ch = 0; ch < PW_LMT_MAX_CHANNEL_NUMBER_2G; ch++) {
			for (rate = 0; rate < PW_LMT_MAX_RS_NUM; rate++) {
				for (bw = 0; bw < PW_LMT_RU_BW_NULL; bw++) {
					tmp = 127;
					for (reg = 0; reg < PW_LMT_MAX_REGULATION_NUM; reg++) {
						RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_ru_2g[%d][%d][%d][%d][%d]=   %d\n",
							reg, bw, rate, ch, tx_num, pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num]);

						if (tmp > pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num] &&
							pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num] > 0) {
							tmp = pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num];
						}
					}
					if (tmp == 127)
						tmp = 0;
					pwr->tx_pwr_limit_ru_2g[PW_LMT_REGU_WW13][bw][rate][ch][tx_num] = tmp;

					RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_ru_2g[%d][%d][%d][%d][%d]=   %d\n",
							PW_LMT_REGU_WW13, bw, rate, ch, tx_num, pwr->tx_pwr_limit_ru_2g[PW_LMT_REGU_WW13][bw][rate][ch][tx_num]);

				}
			}
		}
	}

	for (tx_num = 0; tx_num < MAX_HALRF_PATH; tx_num++) {
		for (ch = 0; ch < PW_LMT_MAX_CHANNEL_NUMBER_5G; ch++) {
			for (rate = 0; rate < PW_LMT_MAX_RS_NUM; rate++) {
				for (bw = 0; bw < PW_LMT_RU_BW_NULL; bw++) {
					tmp = 127;
					for (reg = 0; reg < PW_LMT_MAX_REGULATION_NUM; reg++) {
						RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_ru_5g[%d][%d][%d][%d][%d]=   %d\n",
							reg, bw, rate, ch, tx_num, pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num]);

						if (tmp > pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num] &&
							pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num] > 0) {
							tmp = pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num];
						}
					}
					if (tmp == 127)
						tmp = 0;
					pwr->tx_pwr_limit_ru_5g[PW_LMT_REGU_WW13][bw][rate][ch][tx_num] = tmp;

					RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_ru_5g[%d][%d][%d][%d][%d]=   %d\n",
							PW_LMT_REGU_WW13, bw, rate, ch, tx_num, pwr->tx_pwr_limit_ru_5g[PW_LMT_REGU_WW13][bw][rate][ch][tx_num]);

				}
			}
		}
	}

	RF_DBG(rf, DBG_RF_INIT, "<======%s finish!!!\n", __func__);

}

void halrf_power_limit_set_ext_pwr_limit_table(struct rf_info *rf,
							enum phl_phy_idx phy)
{
#ifdef SPF_PHL_RF_019_SAR
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u32 reg, bw, rate, bf, ch, tx_num;
	s8 ext_pwr = 0;
	u8 real_ch;

	RF_DBG(rf, DBG_RF_INIT, "======>%s   phy=%d\n", __func__, phy);

	reg = (u32)halrf_get_regulation_info(rf, BAND_ON_24G);

	for (tx_num = 0; tx_num < MAX_HALRF_PATH; tx_num++) {
		ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_2_4g[tx_num];
		for (ch = 0; ch < PW_LMT_MAX_CHANNEL_NUMBER_2G; ch++) {
			for (bf = 0; bf < PW_LMT_MAX_BF_NUM; bf++) {
				for (rate = 0; rate < PW_LMT_MAX_RS_NUM; rate++) {
					for (bw = 0; bw < PW_LMT_MAX_2G_BANDWITH_NUM; bw++) {
						
						RF_DBG(rf, DBG_RF_INIT, "ext_pwr_lmt_2_4g[%d]=%d   pwr->tx_pwr_limit_2g[%d][%d][%d][%d][%d][%d]=   %d\n",
							tx_num, ext_pwr,
							reg, bw, rate, bf, ch, tx_num , pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num]);

						if (ext_pwr > 0) {
							if (pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num] > 0 &&
									pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num] < ext_pwr) {
								pwr->tx_pwr_limit_2g[PW_LMT_REGU_EXT_PWR][bw][rate][bf][ch][tx_num] =
									pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num];
							} else {
								pwr->tx_pwr_limit_2g[PW_LMT_REGU_EXT_PWR][bw][rate][bf][ch][tx_num] = ext_pwr;
							}
						} else {
							pwr->tx_pwr_limit_2g[PW_LMT_REGU_EXT_PWR][bw][rate][bf][ch][tx_num] =
								pwr->tx_pwr_limit_2g[reg][bw][rate][bf][ch][tx_num];
						}

						RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_2g[%d][%d][%d][%d][%d][%d] =   %d\n",
								PW_LMT_REGU_EXT_PWR, bw, rate, bf, ch, tx_num ,
								pwr->tx_pwr_limit_2g[PW_LMT_REGU_EXT_PWR][bw][rate][bf][ch][tx_num]);
					}
				}
			}
		}
	}

	reg = (u32)halrf_get_regulation_info(rf, BAND_ON_5G);

	for (tx_num = 0; tx_num < MAX_HALRF_PATH; tx_num++) {
		for (ch = 0; ch < PW_LMT_MAX_CHANNEL_NUMBER_5G; ch++) {
			real_ch = halrf_get_limit_ch_idx_to_ch_idx(rf, PW_LMT_BAND_5G, (u8)ch);
			if (real_ch >= 36 && real_ch <= 48)
				ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_5g_band1[tx_num];
			else if (real_ch >= 50 && real_ch <= 64)
				ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_5g_band2[tx_num];
			else if (real_ch >= 100 && real_ch <= 144)
				ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_5g_band3[tx_num];
			else if (real_ch >= 149 && real_ch <= 177)
				ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_5g_band4[tx_num];

			for (bf = 0; bf < PW_LMT_MAX_BF_NUM; bf++) {
				for (rate = 0; rate < PW_LMT_MAX_RS_NUM; rate++) {
					for (bw = 0; bw < PW_LMT_MAX_BANDWIDTH_NUM; bw++) {

						RF_DBG(rf, DBG_RF_INIT, "ext_pwr_lmt_5g[%d]=%d   pwr->tx_pwr_limit_5g[%d][%d][%d][%d][%d][%d]=   %d\n",
							tx_num, ext_pwr,
							reg, bw, rate, bf, ch, tx_num , pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num]);

						if (ext_pwr > 0) {
							if (pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num] > 0 &&
									pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num] < ext_pwr) {
								pwr->tx_pwr_limit_5g[PW_LMT_REGU_EXT_PWR][bw][rate][bf][ch][tx_num] =
									pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num];
							} else {
								pwr->tx_pwr_limit_5g[PW_LMT_REGU_EXT_PWR][bw][rate][bf][ch][tx_num] = ext_pwr;
							}
						} else {
							pwr->tx_pwr_limit_5g[PW_LMT_REGU_EXT_PWR][bw][rate][bf][ch][tx_num] =
								pwr->tx_pwr_limit_5g[reg][bw][rate][bf][ch][tx_num];
						}

						RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_5g[%d][%d][%d][%d][%d][%d]=   %d\n",
							PW_LMT_REGU_EXT_PWR, bw, rate, bf, ch, tx_num,
							pwr->tx_pwr_limit_5g[PW_LMT_REGU_EXT_PWR][bw][rate][bf][ch][tx_num]);
					}
				}
			}
		}
	}

	RF_DBG(rf, DBG_RF_INIT, "<======%s finish!!!\n", __func__);
#endif
}

void halrf_power_limit_set_ext_pwr_limit_ru_table(struct rf_info *rf,
							enum phl_phy_idx phy)
{
#ifdef SPF_PHL_RF_019_SAR
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	struct rtw_hal_com_t *hal = rf->hal_com;
	u32 reg, bw, rate, ch, tx_num;
	s8 ext_pwr = 0;
	u8 real_ch; 

	RF_DBG(rf, DBG_RF_INIT, "======>%s   phy=%d\n", __func__, phy);

	reg = (u32)halrf_get_regulation_info(rf, BAND_ON_24G);

	for (tx_num = 0; tx_num < MAX_HALRF_PATH; tx_num++) {
		ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_2_4g[tx_num];
		for (ch = 0; ch < PW_LMT_MAX_CHANNEL_NUMBER_2G; ch++) {
			for (rate = 0; rate < PW_LMT_MAX_RS_NUM; rate++) {
				for (bw = 0; bw < PW_LMT_RU_BW_NULL; bw++) {

					RF_DBG(rf, DBG_RF_INIT, "ext_pwr_lmt_2_4g[%d]=%d   pwr->tx_pwr_limit_ru_2g[%d][%d][%d][%d][%d]=   %d\n",
						tx_num, ext_pwr,
						reg, bw, rate, ch, tx_num, pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num]);

					if (ext_pwr > 0) {
						if (pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num] > 0 &&
								pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num] < ext_pwr) {
							pwr->tx_pwr_limit_ru_2g[PW_LMT_REGU_EXT_PWR][bw][rate][ch][tx_num] =
								pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num];
						} else {
							pwr->tx_pwr_limit_ru_2g[PW_LMT_REGU_EXT_PWR][bw][rate][ch][tx_num] = ext_pwr;
						}
					} else {
						pwr->tx_pwr_limit_ru_2g[PW_LMT_REGU_EXT_PWR][bw][rate][ch][tx_num] =
							pwr->tx_pwr_limit_ru_2g[reg][bw][rate][ch][tx_num];
					}

					RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_ru_2g[%d][%d][%d][%d][%d] =   %d\n",
						PW_LMT_REGU_EXT_PWR, bw, rate, ch, tx_num,
						pwr->tx_pwr_limit_ru_2g[PW_LMT_REGU_EXT_PWR][bw][rate][ch][tx_num]);
				}
			}
		}
	}

	reg = (u32)halrf_get_regulation_info(rf, BAND_ON_5G);

	for (tx_num = 0; tx_num < MAX_HALRF_PATH; tx_num++) {
		for (ch = 0; ch < PW_LMT_MAX_CHANNEL_NUMBER_5G; ch++) {
			real_ch = halrf_get_limit_ch_idx_to_ch_idx(rf, PW_LMT_BAND_5G, (u8)ch);
			if (real_ch >= 36 && real_ch <= 48)
				ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_5g_band1[tx_num];
			else if (real_ch >= 50 && real_ch <= 64)
				ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_5g_band2[tx_num];
			else if (real_ch >= 100 && real_ch <= 144)
				ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_5g_band3[tx_num];
			else if (real_ch >= 149 && real_ch <= 177)
				ext_pwr = hal->rtw_tpu_ext_pwr_lmt_i[phy].ext_pwr_lmt_5g_band4[tx_num];

			for (rate = 0; rate < PW_LMT_MAX_RS_NUM; rate++) {
				for (bw = 0; bw < PW_LMT_RU_BW_NULL; bw++) {

					RF_DBG(rf, DBG_RF_INIT, "ext_pwr_lmt_5g[%d]=%d   pwr->tx_pwr_limit_ru_5g[%d][%d][%d][%d][%d]=   %d\n",
						tx_num, ext_pwr,
						reg, bw, rate, ch, tx_num, pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num]);

					if (ext_pwr > 0) {
						if (pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num] > 0 &&
								pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num] < ext_pwr) {
							pwr->tx_pwr_limit_ru_5g[PW_LMT_REGU_EXT_PWR][bw][rate][ch][tx_num] =
								pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num];
						} else {
							pwr->tx_pwr_limit_ru_5g[PW_LMT_REGU_EXT_PWR][bw][rate][ch][tx_num] = ext_pwr;
						}
					} else {
						pwr->tx_pwr_limit_ru_5g[PW_LMT_REGU_EXT_PWR][bw][rate][ch][tx_num] =
							pwr->tx_pwr_limit_ru_5g[reg][bw][rate][ch][tx_num];
					}

					RF_DBG(rf, DBG_RF_INIT, "pwr->tx_pwr_limit_ru_5g[%d][%d][%d][%d][%d]=   %d\n",
						PW_LMT_REGU_EXT_PWR, bw, rate, ch, tx_num,
						pwr->tx_pwr_limit_ru_5g[PW_LMT_REGU_EXT_PWR][bw][rate][ch][tx_num]);
				}
			}
		}
	}

	RF_DBG(rf, DBG_RF_INIT, "<======%s finish!!!\n", __func__);
#endif
}


s8 halrf_get_power_by_rate(struct rf_info *rf,
	enum phl_phy_idx phy,
	u8 rf_path, u16 rate, u8 dcm, u8 offset)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u8 band;
	u16 rate_tmp;
	s8 pwr_by_rate;
	u8 channel = rf->hal_com->band[phy].cur_chandef.center_ch;

	RF_DBG(rf, DBG_RF_POWER, "======>%s rf_path=%d rate=0x%x dcm=%d\n",
		__func__, rf_path, rate, dcm);

	if (channel >= 1 && channel <= 14)
		band = PW_LMT_BAND_2_4G;
	else
		band = PW_LMT_BAND_5G;

	rate_tmp = halrf_get_dcm_offset_pwr_by_rate(rf, rate, dcm, offset);

	if ((rate_tmp >= HALRF_DATA_RATE_CCK1 && rate_tmp <= HALRF_DATA_RATE_CCK11) ||
		rate_tmp == HALRF_DATA_RATE_CCK_OFFSET) {
		pwr_by_rate = pwr->tx_pwr_by_rate[PW_LMT_BAND_2_4G][rate_tmp];

		RF_DBG(rf, DBG_RF_POWER, "pwr_by_rate(%d)=pwr->tx_pwr_by_rate[%d][%d] channel=%d\n",
			pwr_by_rate, PW_LMT_BAND_2_4G, rate_tmp, channel);
	} else {
		pwr_by_rate = pwr->tx_pwr_by_rate[band][rate_tmp];

		RF_DBG(rf, DBG_RF_POWER, "pwr_by_rate(%d)=pwr->tx_pwr_by_rate[%d][%d] channel=%d\n",
			pwr_by_rate, band, rate_tmp, channel);
	}

	return pwr_by_rate;
}

s8 halrf_get_power_by_rate_band(struct rf_info *rf,
	enum phl_phy_idx phy, u16 rate, u8 dcm, u8 offset, u32 band)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	u16 rate_tmp;
	s8 pwr_by_rate;

	RF_DBG(rf, DBG_RF_POWER, "======>%s rate=0x%x dcm=%d band=%d\n",
		__func__, rate, dcm, band);

	rate_tmp = halrf_get_dcm_offset_pwr_by_rate(rf, rate, dcm, offset);

	if ((rate_tmp >= HALRF_DATA_RATE_CCK1 && rate_tmp <= HALRF_DATA_RATE_CCK11) ||
		rate_tmp == HALRF_DATA_RATE_CCK_OFFSET) {
		pwr_by_rate = pwr->tx_pwr_by_rate[PW_LMT_BAND_2_4G][rate_tmp];

		RF_DBG(rf, DBG_RF_POWER, "pwr_by_rate(%d)=pwr->tx_pwr_by_rate[%d][%d] band=%d\n",
			pwr_by_rate, PW_LMT_BAND_2_4G, rate_tmp, band);
	} else {
		pwr_by_rate = pwr->tx_pwr_by_rate[band][rate_tmp];

		RF_DBG(rf, DBG_RF_POWER, "pwr_by_rate(%d)=pwr->tx_pwr_by_rate[%d][%d] band=%d\n",
			pwr_by_rate, band, rate_tmp, band);
	}

	return pwr_by_rate;
}

s8 halrf_get_power_limit(struct rf_info *rf,
	enum phl_phy_idx phy, u8 rf_path, u16 rate, u8 bandwidth,
	u8 beamforming, u8 tx_num, u8 channel)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	struct rtw_phl_com_t *phl = rf->phl_com;
	
#ifdef SPF_PHL_RF_019_SAR
	struct rtw_hal_com_t *hal = rf->hal_com;
#endif
	s8 pwr_limit;
	u8 limit_rate = PW_LMT_RS_CCK, limit_ch, reg;

	RF_DBG(rf, DBG_RF_POWER, "======>%s rf_path=%d rate=0x%x beamforming=%d\n",
		__func__, rf_path, rate, beamforming);

	limit_rate = halrf_hw_rate_to_limit_rate_tx_num(rf, rate);
	limit_ch = halrf_get_ch_idx_to_limit_array(rf, channel);

	if (channel >= 1 && channel <= 14) {

#ifdef SPF_PHL_RF_019_SAR
		if (hal->ext_pwr_lmt_en == true)
			reg = PW_LMT_REGU_EXT_PWR;
		else
#endif
			reg = halrf_get_regulation_info(rf, BAND_ON_24G);

		pwr_limit = pwr->tx_pwr_limit_2g[reg][bandwidth][limit_rate][beamforming][limit_ch][tx_num];
		RF_DBG(rf, DBG_RF_POWER, "pwr_limit(%d) = pwr->tx_pwr_limit_2g[%d][%d][%d][%d][%d][%d]\n",
			pwr_limit, reg, bandwidth, limit_rate, beamforming, limit_ch, tx_num);
	} else {
#ifdef SPF_PHL_RF_019_SAR
		if (hal->ext_pwr_lmt_en == true)
			reg = PW_LMT_REGU_EXT_PWR;
		else
#endif
			reg = halrf_get_regulation_info(rf, BAND_ON_5G);

		pwr_limit = pwr->tx_pwr_limit_5g[reg][bandwidth][limit_rate][beamforming][limit_ch][tx_num];
		RF_DBG(rf, DBG_RF_POWER, "pwr_limit(%d) = pwr->tx_pwr_limit_5g[%d][%d][%d][%d][%d][%d]\n",
			pwr_limit, reg, bandwidth, limit_rate, beamforming, limit_ch, tx_num);
	}

	if (pwr_limit == 127)
		pwr_limit = 0;

	return pwr_limit;
}

s8 halrf_get_power_limit_ru(struct rf_info *rf,
	enum phl_phy_idx phy, u8 rf_path, u16 rate, u8 bandwidth,
	u8 tx_num, u8 channel)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	struct rtw_phl_com_t *phl = rf->phl_com;

#ifdef SPF_PHL_RF_019_SAR
	struct rtw_hal_com_t *hal = rf->hal_com;
#endif
	s8 pwr_limit_ru;
	u8 limit_rate = PW_LMT_RS_CCK, limit_ch, reg;

	RF_DBG(rf, DBG_RF_POWER, "======>%s phy=%d rf_path=%d rate=0x%x\n",
		__func__, phy, rf_path, rate);

	limit_rate = halrf_hw_rate_to_limit_rate_tx_num(rf, rate);
	limit_ch = halrf_get_ch_idx_to_limit_array(rf, channel);

	if (channel >= 1 && channel <= 14) {
#ifdef SPF_PHL_RF_019_SAR
		if (hal->ext_pwr_lmt_en == true)
			reg = PW_LMT_REGU_EXT_PWR;
		else
#endif
			reg = halrf_get_regulation_info(rf, BAND_ON_24G);

		pwr_limit_ru = pwr->tx_pwr_limit_ru_2g[reg][bandwidth][limit_rate][limit_ch][tx_num];
		RF_DBG(rf, DBG_RF_POWER, "pwr_limit_ru(%d) = pwr->tx_pwr_limit_ru_2g[%d][%d][%d][%d][%d]\n",
			pwr_limit_ru, reg, bandwidth, limit_rate, limit_ch, tx_num);
	} else {
#ifdef SPF_PHL_RF_019_SAR
		if (hal->ext_pwr_lmt_en == true)
			reg = PW_LMT_REGU_EXT_PWR;
		else
#endif
			reg = halrf_get_regulation_info(rf, BAND_ON_5G);

		pwr_limit_ru = pwr->tx_pwr_limit_ru_5g[reg][bandwidth][limit_rate][limit_ch][tx_num];
		RF_DBG(rf, DBG_RF_POWER, "pwr_limit_ru(%d) = pwr->tx_pwr_limit_ru_5g[%d][%d][%d][%d][%d]\n",
			pwr_limit_ru, reg, bandwidth, limit_rate, limit_ch, tx_num);
	}

	if (pwr_limit_ru == 127)
		pwr_limit_ru = 0;

	return pwr_limit_ru;
}

s16 halrf_get_power(void *rf_void,
	u8 rf_path, u16 rate, u8 dcm, u8 offset, u8 bandwidth,
	u8 beamforming, u8 channel)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	struct rtw_phl_com_t *phl = rf->phl_com;
	u8 band, limit_rate = PW_LMT_RS_CCK, tx_num = PW_LMT_PH_1T, limit_ch;
	u16 rate_tmp;
	s16 pwr_by_rate, pwr_limit, power;

	RF_DBG(rf, DBG_RF_INIT, "======>%s rf_path=%d rate=%d dcm=%d bw=%d bf=%d ch=%d\n",
		__func__, rf_path, rate, dcm, bandwidth, beamforming, channel);

	if (channel >= 1 && channel <= 14)
		band = PW_LMT_BAND_2_4G;
	else
		band = PW_LMT_BAND_5G;

	tx_num = rf_path;

	rate_tmp = halrf_get_dcm_offset_pwr_by_rate(rf, rate, dcm, offset);

	pwr_by_rate = (s16)pwr->tx_pwr_by_rate[band][rate_tmp];

	RF_DBG(rf, DBG_RF_INIT, "pwr_by_rate(%d)=(s16)pwr->tx_pwr_by_rate[%d][%d]\n",
		pwr_by_rate, band, rate_tmp);

	limit_rate = halrf_hw_rate_to_limit_rate_tx_num(rf, rate);
	limit_ch = halrf_get_ch_idx_to_limit_array(rf, channel);

	if (channel >= 1 && channel <= 14) {
		pwr_limit = (s16)pwr->tx_pwr_limit_2g[halrf_get_regulation_info(rf, BAND_ON_24G)][bandwidth][limit_rate][beamforming][limit_ch][tx_num];
		RF_DBG(rf, DBG_RF_INIT, "pwr_limit(%d) = (s16)pwr->tx_pwr_limit_2g[%d][%d][%d][%d][%d][%d]\n",
			pwr_limit, halrf_get_regulation_info(rf, BAND_ON_24G), bandwidth, limit_rate, beamforming, limit_ch, tx_num);
	} else {
		pwr_limit = (s16)pwr->tx_pwr_limit_5g[halrf_get_regulation_info(rf, BAND_ON_5G)][bandwidth][limit_rate][beamforming][limit_ch][tx_num];
		RF_DBG(rf, DBG_RF_INIT, "pwr_limit(%d) = (s16)pwr->tx_pwr_limit_5g[%d][%d][%d][%d][%d][%d]\n",
			pwr_limit, halrf_get_regulation_info(rf, BAND_ON_5G), bandwidth, limit_rate, beamforming, limit_ch, tx_num);
	}

	if (pwr_by_rate > pwr_limit)
		power = pwr_limit;
	else
		power = pwr_by_rate;

	RF_DBG(rf, DBG_RF_INIT, "power = %d\n", power);

	return power;
}

bool halrf_set_power(struct rf_info *rf, enum phl_phy_idx phy,
	enum phl_pwr_table pwr_table)
{
	struct rtw_hal_com_t *hal_com = rf->hal_com;
	bool result = true;

#ifdef RF_8852A_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852A) {
		halrf_set_power_8852a(rf, phy, pwr_table);
	}
#endif

#ifdef RF_8852B_SUPPORT
	if (hal_com->chip_id == CHIP_WIFI6_8852B) {
		halrf_set_power_8852b(rf, phy, pwr_table);
	}
#endif

	return result;
}

bool halrf_get_efuse_power_table_switch(struct rf_info *rf, enum phl_phy_idx phy_idx)
{
	struct rtw_phl_com_t *phl_com = rf->phl_com;
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	struct rtw_tpu_info *tpu = &rf->hal_com->band[phy_idx].rtw_tpu_i;
	bool result = true;

	pwr->pwr_table_switch_efuse = phl_com->dev_cap.rf_board_opt & 0x3;
	pwr->pwr_by_rate_switch = phl_com->dev_cap.pwrbyrate_off; 
	pwr->pwr_limit_switch =  phl_com->dev_cap.pwrlmt_type;

	RF_DBG(rf, DBG_RF_POWER, "======>%s From Driver pwr->pwr_by_rate_switch=%d\n",
		__func__, pwr->pwr_by_rate_switch);

	RF_DBG(rf, DBG_RF_POWER, "======>%s From Driver pwr->pwr_limit_switch=%d\n",
			__func__, pwr->pwr_limit_switch);

	RF_DBG(rf, DBG_RF_POWER, "======>%s From Efuse pwr->pwr_table_switch_efuse=%d\n",
			__func__, pwr->pwr_table_switch_efuse);

	if (pwr->pwr_limit_switch == PWLMT_DISABLE)
		tpu->pwr_lmt_en = false;
	else if (pwr->pwr_limit_switch == PWBYRATE_AND_PWLMT)
		tpu->pwr_lmt_en = true;
	else {
		if (pwr->pwr_table_switch_efuse == 0)
			tpu->pwr_lmt_en = false;
		else if (pwr->pwr_table_switch_efuse == 2)
			tpu->pwr_lmt_en = false;
		else
			tpu->pwr_lmt_en = true;
	}

	halrf_mac_write_pwr_limit_en(rf, phy_idx);

	return result;
}

void halrf_set_power_table_switch(struct rf_info *rf,
	enum phl_phy_idx phy, u8 pwr_by_rate, u8 pwr_limt)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;

	pwr->pwr_by_rate_switch = pwr_by_rate;
	pwr->pwr_limit_switch = pwr_limt;
}

u8 halrf_get_power_limit_extra(struct rf_info *rf)
{
	struct halrf_pwr_info *pwr = &rf->pwr_info;
	struct rtw_para_pwrlmt_info_t *pwrlmt_info = NULL;
	struct _halrf_file_regd_ext *pregd_codemap = NULL;
	struct _halrf_file_regd_ext *array = NULL;
	struct rtw_regulation_info rg_info = {0};
	struct rtw_hal_com_t *hal = rf->hal_com;
	u8 i, j, regd_idx = 0xff;

	pwrlmt_info = &rf->phl_com->phy_sw_cap[HW_PHY_0].rf_txpwrlmt_info;
	pregd_codemap = (struct _halrf_file_regd_ext *) pwrlmt_info->ext_reg_codemap;

	if (pwrlmt_info->para_src == 0) {
		RF_DBG(rf, DBG_RF_POWER, "======>%s   is_form_folder=%d   Return!!!\n",
			__func__, pwrlmt_info->para_src);
		return 0xff;
	}

	RF_DBG(rf, DBG_RF_POWER, "======>%s  total_regd_idx=%d  extra_limit_idx=%d\n",
		__func__, pwrlmt_info->ext_regd_arridx, pwrlmt_info->ext_reg_map_num);

	halrf_query_regulation_info(rf, &rg_info);
		
	RF_DBG(rf, DBG_RF_POWER, "domain_code=0x%x   country=%s   regulation_2g=%d   regulation_5g=%d	chplan_ver=%d	country_ver=%d\n",
		rg_info.domain_code, rg_info.country, rg_info.regulation_2g, rg_info.regulation_5g, rg_info.chplan_ver, rg_info.country_ver);

	for (i = 0; i <= pwrlmt_info->ext_regd_arridx; i++)
		RF_DBG(rf, DBG_RF_POWER, "ext_regd_name[%d]=%s\n", i, pwrlmt_info->ext_regd_name[i]);

	RF_DBG(rf, DBG_RF_POWER, "======>   Start domain code Search\n");
	for (i = 0; i < pwrlmt_info->ext_reg_map_num; i++) {
		array = (struct _halrf_file_regd_ext *)&pregd_codemap[i];

		RF_DBG(rf, DBG_RF_POWER, "======>   %d\n", i);
		RF_DBG(rf, DBG_RF_POWER, "ent->domain=0x%x\n", array->domain);
		RF_DBG(rf, DBG_RF_POWER, "ent->country=%s\n", array->country);
		RF_DBG(rf, DBG_RF_POWER, "ent->reg_name=%s\n", array->reg_name);

		if (rg_info.domain_code == (u8)array->domain) {
			RF_DBG(rf, DBG_RF_POWER, "Search rg_info.domain_code == Power limit array->domain\n");
			for (j = 0; j <= pwrlmt_info->ext_regd_arridx; j++) {
				if (_os_strcmp(array->reg_name, pwrlmt_info->ext_regd_name[j]) == 0) {
					regd_idx = j;
					i = (u8)pwrlmt_info->ext_reg_map_num;
					RF_DBG(rf, DBG_RF_POWER, "======>   Search regd_idx=%d\n", regd_idx);
					break;
				}
			}
		}
	}
	RF_DBG(rf, DBG_RF_POWER, "<======   End domain code Search\n");

	RF_DBG(rf, DBG_RF_POWER, "======>   Start Country code Search\n");
	for (i = 0; i < pwrlmt_info->ext_reg_map_num; i++) {
		array = (struct _halrf_file_regd_ext *)&pregd_codemap[i];
		
		RF_DBG(rf, DBG_RF_POWER, "======>   %d\n", i);
		RF_DBG(rf, DBG_RF_POWER, "ent->domain=0x%x\n", array->domain);
		RF_DBG(rf, DBG_RF_POWER, "ent->country=%s\n", array->country);
		RF_DBG(rf, DBG_RF_POWER, "ent->reg_name=%s\n", array->reg_name);

		if (hal_mem_cmp(hal, rg_info.country, array->country, 2) == 0 && _os_strcmp(rg_info.country, "") != 0) {
			RF_DBG(rf, DBG_RF_POWER, "Search rg_info.country == Power limit array->country\n");
			for (j = 0; j <= pwrlmt_info->ext_regd_arridx; j++) {
				if (_os_strcmp(array->reg_name, pwrlmt_info->ext_regd_name[j]) == 0) {
					regd_idx = j;
					i = (u8)pwrlmt_info->ext_reg_map_num;
					RF_DBG(rf, DBG_RF_POWER, "======>   Search regd_idx=%d\n", regd_idx);
					break;
				}
			}
		}
	}
	RF_DBG(rf, DBG_RF_POWER, "<======   End Country code Search\n");

	RF_DBG(rf, DBG_RF_POWER, "Return regd_idx = %d\n", regd_idx);

	return regd_idx;
}

void halrf_reload_pwr_limit_tbl_and_set(struct rf_info *rf,
	enum phl_phy_idx phy, enum phl_pwr_table pwr_table)
{
	struct rtw_para_pwrlmt_info_t *pwrlmt_info = NULL;
	
	bool is_form_folder;
	u32 folder_len;
	u32 *folder_array;

	RF_DBG(rf, DBG_RF_POWER, "======>%s\n", __func__);

	if (pwr_table == PWR_LIMIT) {
		pwrlmt_info = &rf->phl_com->phy_sw_cap[HW_PHY_0].rf_txpwrlmt_info;

		is_form_folder = pwrlmt_info->para_src;
		folder_len = pwrlmt_info->para_data_len;
		folder_array = pwrlmt_info->para_data;

		RF_DBG(rf, DBG_RF_POWER, "Reload power limit table and set. is_form_folder=%d folder_len=%d\n",
			is_form_folder, folder_len);

		halrf_config_store_power_limit(rf , is_form_folder,
				folder_len, folder_array);

		halrf_set_power(rf, phy, pwr_table);
	}

	if (pwr_table == PWR_LIMIT_RU) {
		pwrlmt_info = &rf->phl_com->phy_sw_cap[HW_PHY_0].rf_txpwrlmt_ru_info;

		is_form_folder = pwrlmt_info->para_src;
		folder_len = pwrlmt_info->para_data_len;
		folder_array = pwrlmt_info->para_data;
		
		RF_DBG(rf, DBG_RF_POWER, "Reload power limit RU table and set. is_form_folder=%d folder_len=%d\n",
			is_form_folder, folder_len);

		halrf_config_store_power_limit_ru(rf , is_form_folder,
				folder_len, folder_array);

		halrf_set_power(rf, phy, pwr_table);
	}

	RF_DBG(rf, DBG_RF_POWER, "<======%s finish!\n", __func__);
}
