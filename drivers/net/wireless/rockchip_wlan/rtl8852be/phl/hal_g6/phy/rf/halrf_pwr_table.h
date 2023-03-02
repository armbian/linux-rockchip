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
#ifndef _HALRF_PWR_TABLE_H_
#define _HALRF_PWR_TABLE_H_

/*@--------------------------Define Parameters-------------------------------*/
#define TX_PWR_BY_RATE_NUM_BAND 3
#define TX_PWR_BY_RATE_NUM_RF 4

#define PW_LMT_MAX_2G_BANDWITH_NUM 2
#define PW_LMT_MAX_CHANNEL_NUMBER_2G 14
#define PW_LMT_MAX_CHANNEL_NUMBER_5G 53

#define TX_PWR_BY_RATE_NUM_MAC 44
#define TX_PWR_LIMIT_NUM_MAC 80
#define TX_PWR_LIMIT_RU_NUM_MAC 30

#define RADIO_TO_FW_PAGE_SIZE 6
#define RADIO_TO_FW_DATA_SIZE 500


/*@-----------------------End Define Parameters-----------------------*/
/*power by rate*/
enum halrf_pw_by_rate_para_type {
	PW_BYRATE_PARA_NSS1,
	PW_BYRATE_PARA_NSS2,
	PW_BYRATE_PARA_OFFS = 0xF
};

enum halrf_pw_by_rate_rate_type {
	PW_BYRATE_RATE_11M_1M,
	PW_BYRATE_RATE_18M_6M,
	PW_BYRATE_RATE_54M_24M,
	PW_BYRATE_RATE_MCS3_0,
	PW_BYRATE_RATE_MCS7_4,
	PW_BYRATE_RATE_MCS11_8,
	PW_BYRATE_RATE_DCM4_0,
	PW_BYRATE_RATE_AllRate2_1,  /* CCK, OFDM, HT, VHT */
	PW_BYRATE_RATE_AllRate2_2,  /* HE_HEDCM */
	PW_BYRATE_RATE_AllRate5_1,  /* OFDM, HT, VHT, HE_HEDCM */
	PW_BYRATE_RATE_NULL = 0xF
};

struct _halrf_file_regd_ext {
	u16 domain;
	char country[2];
	char reg_name[10];
};

/*power limit*/
struct halrf_tx_pw_lmt {
	u8 band;
	u8 bw;
	u8 ntx;
	u8 rs;
	u8 bf;
	u8 reg;
	u8 ch;
	s8 val;
	u8 tx_shap_idx;
};

struct halrf_tx_pw_lmt_ru {
	u8			band;
	u8			bw;
	u8			ntx;
	u8			rs;
	u8			reg;
	u8			ch;
	s8			val;
	u8 tx_shap_idx;
};

enum halrf_tx_pw_lmt_ru_bandwidth_type {
	PW_LMT_RU_BW_RU26 = 0,
	PW_LMT_RU_BW_RU52,
	PW_LMT_RU_BW_RU106,
	PW_LMT_RU_BW_NULL
};

enum halrf_pw_lmt_regulation_type {
	PW_LMT_REGU_WW13 = 0,
	PW_LMT_REGU_ETSI = 1,
	PW_LMT_REGU_FCC = 2,
	PW_LMT_REGU_MKK = 3,
	PW_LMT_REGU_NA = 4,
	PW_LMT_REGU_IC = 5,
	PW_LMT_REGU_KCC = 6,
	PW_LMT_REGU_ACMA = 7,
	PW_LMT_REGU_NCC = 8,
	PW_LMT_REGU_MEXICO = 9,
	PW_LMT_REGU_CHILE = 10,
	PW_LMT_REGU_UKRAINE = 11,
	PW_LMT_REGU_CN = 12,
	PW_LMT_REGU_QATAR = 13,
	PW_LMT_REGU_UK = 14,
	/* place predefined ones above */
	PW_LMT_REGU_EXT_PWR,
	PW_LMT_REGU_PREDEF_NUM,
	PW_LMT_REGU_NULL, /* declare this to PW_LMT_MAX_REGULATION_NUM after limit array remove usage of PW_LMT_REGU_NULL */
	PW_LMT_MAX_REGULATION_NUM = 32
};

enum halrf_tx_shape_modu_type {
	TX_SHAPE_CCK,
	TX_SHAPE_OFDM,
	TX_SHAPE_MAX
};

enum halrf_pw_lmt_band_type {
	PW_LMT_BAND_2_4G = 0,
	PW_LMT_BAND_5G = 1,
	PW_LMT_BAND_6G = 2,
	PW_LMT_MAX_BAND = 3
};

enum halrf_pw_lmt_bandwidth_type {
	PW_LMT_BW_20M = 0,
	PW_LMT_BW_40M = 1,
	PW_LMT_BW_80M = 2,
	PW_LMT_BW_160M = 3,
	PW_LMT_MAX_BANDWIDTH_NUM = 4
};

enum halrf_pw_lmt_ratesection_type {
	PW_LMT_RS_CCK = 0,
	PW_LMT_RS_OFDM = 1,
	PW_LMT_RS_HT = 2,
	PW_LMT_RS_VHT = 3,
	PW_LMT_RS_HE = 4,
	PW_LMT_MAX_RS_NUM = 5
};

enum halrf_pw_lmt_rfpath_type {
	PW_LMT_PH_1T = 0,
	PW_LMT_PH_2T = 1,
	PW_LMT_PH_3T = 2,
	PW_LMT_PH_4T = 3,
	PW_LMT_MAX_PH_NUM = 4
};

enum halrf_pw_lmt_beamforming_type {
	PW_LMT_NONBF = 0,
	PW_LMT_BF = 1,
	PW_LMT_MAX_BF_NUM = 2
};

enum halrf_data_rate {
	HALRF_DATA_RATE_CCK1 = 0,
	HALRF_DATA_RATE_CCK2 = 0x1,
	HALRF_DATA_RATE_CCK5_5,
	HALRF_DATA_RATE_CCK11,
	HALRF_DATA_RATE_OFDM6,
	HALRF_DATA_RATE_OFDM9,
	HALRF_DATA_RATE_OFDM12,
	HALRF_DATA_RATE_OFDM18,
	HALRF_DATA_RATE_OFDM24,
	HALRF_DATA_RATE_OFDM36,
	HALRF_DATA_RATE_OFDM48 = 10,
	HALRF_DATA_RATE_OFDM54,
	HALRF_DATA_RATE_MCS0,
	HALRF_DATA_RATE_MCS1,
	HALRF_DATA_RATE_MCS2,
	HALRF_DATA_RATE_MCS3,
	HALRF_DATA_RATE_MCS4,
	HALRF_DATA_RATE_MCS5,
	HALRF_DATA_RATE_MCS6,
	HALRF_DATA_RATE_MCS7,
	HALRF_DATA_RATE_MCS8 = 20,
	HALRF_DATA_RATE_MCS9,
	HALRF_DATA_RATE_MCS10,
	HALRF_DATA_RATE_MCS11,
	HALRF_DATA_RATE_MCS12,
	HALRF_DATA_RATE_MCS13,
	HALRF_DATA_RATE_MCS14,
	HALRF_DATA_RATE_MCS15,
	HALRF_DATA_RATE_MCS16,
	HALRF_DATA_RATE_MCS17,
	HALRF_DATA_RATE_MCS18 = 30,
	HALRF_DATA_RATE_MCS19,
	HALRF_DATA_RATE_MCS20,
	HALRF_DATA_RATE_MCS21,
	HALRF_DATA_RATE_MCS22,
	HALRF_DATA_RATE_MCS23,
	HALRF_DATA_RATE_MCS24,
	HALRF_DATA_RATE_MCS25,
	HALRF_DATA_RATE_MCS26,
	HALRF_DATA_RATE_MCS27,
	HALRF_DATA_RATE_MCS28 = 40,
	HALRF_DATA_RATE_MCS29,
	HALRF_DATA_RATE_MCS30,
	HALRF_DATA_RATE_MCS31,
	HALRF_DATA_RATE_VHT_NSS1_MCS0,
	HALRF_DATA_RATE_VHT_NSS1_MCS1,
	HALRF_DATA_RATE_VHT_NSS1_MCS2,
	HALRF_DATA_RATE_VHT_NSS1_MCS3,
	HALRF_DATA_RATE_VHT_NSS1_MCS4,
	HALRF_DATA_RATE_VHT_NSS1_MCS5,
	HALRF_DATA_RATE_VHT_NSS1_MCS6 = 50,
	HALRF_DATA_RATE_VHT_NSS1_MCS7,
	HALRF_DATA_RATE_VHT_NSS1_MCS8,
	HALRF_DATA_RATE_VHT_NSS1_MCS9,
	HALRF_DATA_RATE_VHT_NSS2_MCS0,
	HALRF_DATA_RATE_VHT_NSS2_MCS1,
	HALRF_DATA_RATE_VHT_NSS2_MCS2,
	HALRF_DATA_RATE_VHT_NSS2_MCS3,
	HALRF_DATA_RATE_VHT_NSS2_MCS4,
	HALRF_DATA_RATE_VHT_NSS2_MCS5,
	HALRF_DATA_RATE_VHT_NSS2_MCS6 = 60,
	HALRF_DATA_RATE_VHT_NSS2_MCS7,
	HALRF_DATA_RATE_VHT_NSS2_MCS8,
	HALRF_DATA_RATE_VHT_NSS2_MCS9,
	HALRF_DATA_RATE_VHT_NSS3_MCS0,
	HALRF_DATA_RATE_VHT_NSS3_MCS1,
	HALRF_DATA_RATE_VHT_NSS3_MCS2,
	HALRF_DATA_RATE_VHT_NSS3_MCS3,
	HALRF_DATA_RATE_VHT_NSS3_MCS4,
	HALRF_DATA_RATE_VHT_NSS3_MCS5,
	HALRF_DATA_RATE_VHT_NSS3_MCS6 = 70,
	HALRF_DATA_RATE_VHT_NSS3_MCS7,
	HALRF_DATA_RATE_VHT_NSS3_MCS8,
	HALRF_DATA_RATE_VHT_NSS3_MCS9,
	HALRF_DATA_RATE_VHT_NSS4_MCS0,
	HALRF_DATA_RATE_VHT_NSS4_MCS1,
	HALRF_DATA_RATE_VHT_NSS4_MCS2,
	HALRF_DATA_RATE_VHT_NSS4_MCS3,
	HALRF_DATA_RATE_VHT_NSS4_MCS4,
	HALRF_DATA_RATE_VHT_NSS4_MCS5,
	HALRF_DATA_RATE_VHT_NSS4_MCS6 = 80,
	HALRF_DATA_RATE_VHT_NSS4_MCS7,
	HALRF_DATA_RATE_VHT_NSS4_MCS8,
	HALRF_DATA_RATE_VHT_NSS4_MCS9,
	HALRF_DATA_RATE_HE_NSS1_MCS0,
	HALRF_DATA_RATE_HE_NSS1_MCS1,
	HALRF_DATA_RATE_HE_NSS1_MCS2,
	HALRF_DATA_RATE_HE_NSS1_MCS3,
	HALRF_DATA_RATE_HE_NSS1_MCS4,
	HALRF_DATA_RATE_HE_NSS1_MCS5,
	HALRF_DATA_RATE_HE_NSS1_MCS6 = 90,
	HALRF_DATA_RATE_HE_NSS1_MCS7,
	HALRF_DATA_RATE_HE_NSS1_MCS8,
	HALRF_DATA_RATE_HE_NSS1_MCS9,
	HALRF_DATA_RATE_HE_NSS1_MCS10,
	HALRF_DATA_RATE_HE_NSS1_MCS11,
	HALRF_DATA_RATE_HE_NSS2_MCS0,
	HALRF_DATA_RATE_HE_NSS2_MCS1,
	HALRF_DATA_RATE_HE_NSS2_MCS2,
	HALRF_DATA_RATE_HE_NSS2_MCS3,
	HALRF_DATA_RATE_HE_NSS2_MCS4 = 100,
	HALRF_DATA_RATE_HE_NSS2_MCS5,
	HALRF_DATA_RATE_HE_NSS2_MCS6,
	HALRF_DATA_RATE_HE_NSS2_MCS7,
	HALRF_DATA_RATE_HE_NSS2_MCS8,
	HALRF_DATA_RATE_HE_NSS2_MCS9,
	HALRF_DATA_RATE_HE_NSS2_MCS10,
	HALRF_DATA_RATE_HE_NSS2_MCS11,
	HALRF_DATA_RATE_HE_NSS3_MCS0,
	HALRF_DATA_RATE_HE_NSS3_MCS1,
	HALRF_DATA_RATE_HE_NSS3_MCS2 = 110,
	HALRF_DATA_RATE_HE_NSS3_MCS3,
	HALRF_DATA_RATE_HE_NSS3_MCS4,
	HALRF_DATA_RATE_HE_NSS3_MCS5,
	HALRF_DATA_RATE_HE_NSS3_MCS6,
	HALRF_DATA_RATE_HE_NSS3_MCS7,
	HALRF_DATA_RATE_HE_NSS3_MCS8,
	HALRF_DATA_RATE_HE_NSS3_MCS9,
	HALRF_DATA_RATE_HE_NSS3_MCS10,
	HALRF_DATA_RATE_HE_NSS3_MCS11,
	HALRF_DATA_RATE_HE_NSS4_MCS0 = 120,
	HALRF_DATA_RATE_HE_NSS4_MCS1,
	HALRF_DATA_RATE_HE_NSS4_MCS2,
	HALRF_DATA_RATE_HE_NSS4_MCS3,
	HALRF_DATA_RATE_HE_NSS4_MCS4,
	HALRF_DATA_RATE_HE_NSS4_MCS5,
	HALRF_DATA_RATE_HE_NSS4_MCS6,
	HALRF_DATA_RATE_HE_NSS4_MCS7,
	HALRF_DATA_RATE_HE_NSS4_MCS8,
	HALRF_DATA_RATE_HE_NSS4_MCS9,
	HALRF_DATA_RATE_HE_NSS4_MCS10 = 130,
	HALRF_DATA_RATE_HE_NSS4_MCS11,
	HALRF_DATA_RATE_HEDCM_NSS1_MCS0,
	HALRF_DATA_RATE_HEDCM_NSS1_MCS1,
	HALRF_DATA_RATE_HEDCM_NSS1_MCS3,
	HALRF_DATA_RATE_HEDCM_NSS1_MCS4,
	HALRF_DATA_RATE_HEDCM_NSS2_MCS0,
	HALRF_DATA_RATE_HEDCM_NSS2_MCS1,
	HALRF_DATA_RATE_HEDCM_NSS2_MCS3,
	HALRF_DATA_RATE_HEDCM_NSS2_MCS4,
	HALRF_DATA_RATE_HEDCM_OFFSET = 140,
	HALRF_DATA_RATE_VHT_OFFSET,
	HALRF_DATA_RATE_HT_OFFSET,
	HALRF_DATA_RATE_OFDM_OFFSET,
	HALRF_DATA_RATE_CCK_OFFSET,
	HALRF_DATA_RATE_MAX
};

struct halrf_pwr_info {
	/*Power by Rate and Power Limit Switch*/
	u8 pwr_table_switch_efuse;
	u8 pwr_by_rate_switch;
	u8 pwr_limit_switch;

	bool regulation[PW_LMT_MAX_BAND][PW_LMT_MAX_REGULATION_NUM];
	u8 tx_shap_idx[PW_LMT_MAX_BAND][TX_SHAPE_MAX][PW_LMT_MAX_REGULATION_NUM];
	s8 tx_pwr_by_rate[TX_PWR_BY_RATE_NUM_BAND][HALRF_DATA_RATE_MAX];

	s8 tx_pwr_limit_2g[PW_LMT_MAX_REGULATION_NUM][PW_LMT_MAX_2G_BANDWITH_NUM]
			[PW_LMT_MAX_RS_NUM][PW_LMT_MAX_BF_NUM][PW_LMT_MAX_CHANNEL_NUMBER_2G][MAX_HALRF_PATH];

	s8 tx_pwr_limit_5g[PW_LMT_MAX_REGULATION_NUM][PW_LMT_MAX_BANDWIDTH_NUM]
			[PW_LMT_MAX_RS_NUM][PW_LMT_MAX_BF_NUM][PW_LMT_MAX_CHANNEL_NUMBER_5G][MAX_HALRF_PATH];

	s8 tx_pwr_limit_ru_2g[PW_LMT_MAX_REGULATION_NUM][PW_LMT_RU_BW_NULL]
			[PW_LMT_MAX_RS_NUM][PW_LMT_MAX_CHANNEL_NUMBER_2G][MAX_HALRF_PATH];

	s8 tx_pwr_limit_ru_5g[PW_LMT_MAX_REGULATION_NUM][PW_LMT_RU_BW_NULL]
			[PW_LMT_MAX_RS_NUM][PW_LMT_MAX_CHANNEL_NUMBER_5G][MAX_HALRF_PATH];

	s8 tx_pwr_by_rate_mac[HW_PHY_MAX][TX_PWR_BY_RATE_NUM_MAC];

	s8 tx_pwr_limit_mac[HW_PHY_MAX][TX_PWR_LIMIT_NUM_MAC];

	s8 tx_pwr_limit_ru_mac[HW_PHY_MAX][TX_PWR_LIMIT_RU_NUM_MAC];
	s16 tx_pwr_limit_ru26_mac[HW_PHY_MAX];
	s16 tx_pwr_limit_ru52_mac[HW_PHY_MAX];
	s16 tx_pwr_limit_ru106_mac[HW_PHY_MAX];
	bool coex_pwr_ctl_enable;
	bool dpk_pwr_ctl_enable;
	s32 coex_pwr;
	s32 dpk_pwr;
	u8 mp_regulation;
	u8 regulation_idx;
	u8 regulation_str[10];
	bool fix_power[MAX_HALRF_PATH];
	s8 fix_power_dbm[MAX_HALRF_PATH];
	bool set_tx_ptrn_shap_en;
	u8 set_tx_ptrn_shap_idx[PW_LMT_MAX_BAND][TX_SHAPE_MAX];
};

extern const char * const _pw_lmt_regu_type_str[PW_LMT_MAX_REGULATION_NUM];
#define pw_lmt_regu_type_str(lmt) ((lmt) < PW_LMT_MAX_REGULATION_NUM ? _pw_lmt_regu_type_str[(lmt)] : NULL)

extern const enum halrf_pw_lmt_regulation_type _regulation_to_pw_lmt_regu_type[REGULATION_MAX];
#define regulation_to_pw_lmt_regu_type(reg) ((reg) < REGULATION_MAX ? _regulation_to_pw_lmt_regu_type[(reg)] : PW_LMT_REGU_WW13)

extern const enum halrf_pw_lmt_regulation_type _tpo_to_pw_lmt_regu_type[TPO_NA];
#define tpo_to_pw_lmt_regu_type(reg) ((reg) < TPO_NA ? _tpo_to_pw_lmt_regu_type[(reg)] : PW_LMT_REGU_WW13)

const char *halrf_get_pw_lmt_regu_type_str_extra(struct rf_info *rf, u8 band);

u8 halrf_get_regulation_info(struct rf_info *rf, u8 band);

void halrf_power_by_rate_store_to_array(struct rf_info *rf,
			u32 band, u32 tx_num, u32 rate_id, u32 data);
void halrf_power_limit_store_to_array(struct rf_info *rf,
			u8 regulation, u8 band, u8 bandwidth, u8 rate,
			u8 tx_num, u8 beamforming, u8 chnl, s8 val);
void halrf_power_limit_set_worldwide(struct rf_info *rf);
void halrf_power_limit_ru_store_to_array(struct rf_info *rf,
			u8 band, u8 bandwidth, u8 tx_num, u8 rate,
			u8 regulation, u8 chnl, s8 val);
void halrf_power_limit_ru_set_worldwide(struct rf_info *rf);

u8 halrf_get_power_limit_extra(struct rf_info *rf);

#endif
