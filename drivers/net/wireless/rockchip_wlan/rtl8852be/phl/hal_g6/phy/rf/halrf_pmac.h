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
#ifndef _HALRF_PMAC_H_
#define _HALRF_PMAC_H_
/*@--------------------------[Define] ---------------------------------------*/

/*@--------------------------[Enum]------------------------------------------*/
enum rf_ppdu_type {
	T_CCK		= 0,
	T_LEGACY	= 1,
	T_HT_MF 	= 2,
	T_VHT		= 3,
	T_HE_SU		= 4,
	T_HE_ER_SU	= 5,
	T_HE_MU_OFDMA	= 6,
	T_HE_TB		= 7,
};
 
/*@--------------------------[Structure]-------------------------------------*/
struct rf_pmac_tx_info {
	enum rf_ppdu_type ppdu;
	u8 mcs;
	u8 bw;
	u8 nss;
	u8 long_preamble_en;
	u8 gi;
	u16 case_id;
	s16 dbm;
	u16 txagc_cw;
	u8 is_cck;
	u16 cnt;
	u16 time;
	u16 period;
	u16 length;
};

struct rf_usr_plcp_gen_in {
	u32 mcs : 6;
	u32 mpdu_len : 14;
	u32 n_mpdu : 9;
	u32 fec : 1;
	u32 dcm : 1;
	u32 rsvd0 : 1;
	u32 aid : 12;
	u32 scrambler_seed : 8; // rand (1~255)
	u32 random_init_seed : 8; // rand (1~255)
	u32 rsvd1 : 4;
	u32 apep : 22;
	u32 ru_alloc : 8;
	u32 rsvd2 : 2;
	u32 nss : 4;
	u32 txbf : 1;
	u32 pwr_boost_db : 5;
	u32 rsvd3 : 22;
};

struct rf_plcp_param_t {
	u32 dbw : 2; //0:BW20, 1:BW40, 2:BW80, 3:BW160/BW80+80
	u32 source_gen_mode : 2;
	u32 locked_clk : 1;
	u32 dyn_bw : 1;
	u32 ndp_en : 1;
	u32 long_preamble_en : 1; //bmode
	u32 stbc : 1;
	u32 gi : 2; //0:0.4,1:0.8,2:1.6,3:3.2
	u32 tb_l_len : 12;
	u32 tb_ru_tot_sts_max : 3;
	u32 vht_txop_not_allowed : 1;
	u32 tb_disam : 1;
	u32 doppler : 2;
	u32 he_ltf_type : 2;//0:1x,1:2x,2:4x

	u32 ht_l_len : 12;
	u32 preamble_puncture : 1;
	u32 he_mcs_sigb : 3;//0~5
	u32 he_dcm_sigb : 1;
	u32 he_sigb_compress_en : 1;
	u32 max_tx_time_0p4us : 14;


	u32 ul_flag : 1;
	u32 tb_ldpc_extra : 1;
	u32 bss_color : 6;
	u32 sr : 4;
	u32 beamchange_en : 1;
	u32 he_er_u106ru_en : 1;
	u32 ul_srp1 : 4;
	u32 ul_srp2 : 4;
	u32 ul_srp3 : 4;
	u32 ul_srp4 : 4;
	u32 mode : 2;

	u32 group_id : 6;
	u32 ppdu_type : 4;//0: bmode,1:Legacy,2:HT_MF,3:HT_GF,4:VHT,5:HE_SU,6:HE_ER_SU,7:HE_MU,8:HE_TB
	u32 txop : 7;
	u32 tb_strt_sts : 3;
	u32 tb_pre_fec_padding_factor : 2;
	u32 cbw : 2;
	u32 txsc : 4;
	u32 tb_mumimo_mode_en : 1;
	u32 rsvd1 : 3;

	u8 nominal_t_pe : 2; // def = 2
	u8 ness : 2; // def = 0
	u8 rsvd2 : 4;

	u8 n_user;
	u16 tb_rsvd : 9;//def = 0
	u16 rsvd3 : 7;

	struct rf_usr_plcp_gen_in usr[4];
};

/*@--------------------------[Prptotype]-------------------------------------*/
void halrf_set_pmac_cw(struct rf_info *rf, enum rf_path path, u16 txagc_cw, bool en);

void halrf_set_pmac_tx(struct rf_info *rf, enum phl_phy_idx phy_idx,
			enum rf_path path, struct rf_pmac_tx_info *tx,
			u8 enable, bool by_cw);

void halrf_set_pseudo_cw(struct rf_info *rf, enum rf_path path,
			u16 txagc_cw, bool en);

#if 0
void halrf_set_pmac_tx(struct rf_info *rf, enum phl_phy_idx phy_idx,
						enum rf_path path, u8 ppdu_type, u8 case_id, s16 dbm,
						u8 enable, u8 is_cck, u16 cnt ,u16 time, u16 period);
#endif

#endif
