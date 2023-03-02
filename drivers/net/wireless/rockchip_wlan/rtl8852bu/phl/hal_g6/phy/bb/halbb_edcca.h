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
#ifndef __HALBB_EDCCA_H__
#define __HALBB_EDCCA_H__

/*@--------------------------[Define] ---------------------------------------*/
#define EDCCA_HL_DIFF_ADPTVTY 7
#define EDCCA_HL_DIFF_NORMAL 8

// EDCCA
#define EDCCA_5G                         63 /*@-62 dBm -3 dB margin*/
#define EDCCA_2G                        68 /*@-57 dBm -3 dB margin*/
#define CARRIER_SENSE                        75 /*@-50dBm -3 dB margin*/
#define EDCCA_MAX                        249 /*@ 127dBm for normal mode*/
#define EDCCA_PWDB_EXCLU_TX		128 /*128 - 256 = -128dBm when Tx*/
#define EDCCA_PWDB_TO_RSSI(pwdb)		((pwdb + 110) < 0 ? 0 : (pwdb + 110))
#define EDCCA_TH_L2H_LB                  66 /*@ -62 dBm from IEEE*/

// FW EDCCA
#define EDCCA_5G_TH                         70 // -62
#define EDCCA_2p4G_TH                        65// -57
#define CARRIER_SENSE_TH                        58 // -50

/*@--------------------------[Enum]------------------------------------------*/
/*@--------------------------[Structure]-------------------------------------*/
struct bb_h2c_fw_edcca {
	u8 mode;
	u8 band;
	u8 pwr_th_5g;
	u8 pwr_th_2p4;
	u8 pwr_th_cs;
	u8 rsvd0;
	u8 rsvd1;
	u8 rsvd2;
};

struct bb_edcca_cr_info {
	u32 r_snd_en;
	u32 r_snd_en_m;
	u32 r_dwn_level;
	u32 r_dwn_level_m;
	u32 r_edcca_level;
	u32 r_edcca_level_m;
	u32 r_edcca_level_p;
	u32 r_edcca_level_p_m;
	u32 r_edcca_rpt_a;
	u32 r_edcca_rpt_a_m;
	u32 r_edcca_rpt_b;
	u32 r_edcca_rpt_b_m;
	u32 r_edcca_rpt_sel;
	u32 r_edcca_rpt_sel_m;
};

struct edcca_hw_rpt {
	s8 pwdb_fb; /*52A/52B is 0 when BW=40, 92XB would fix*/
	s8 pwdb_p20;
	s8 pwdb_s20;
	s8 pwdb_s40;
	s8 pwdb_s80;
	bool flag_fb;
	bool flag_p20;
	bool flag_s20;
	bool flag_s40;
	bool flag_s80;
	s8 pwdb_0;
	s8 pwdb_1;
	s8 pwdb_2;
	s8 pwdb_3;
	s8 pwdb_4;
	s8 pwdb_5;
	s8 pwdb_6;
	s8 pwdb_7;
	s8 pwdb_8;
	u8 path;
};

struct bb_edcca_info {
	struct bb_edcca_cr_info bb_edcca_cr_i;
	u8 th_l;
	u8 th_h;
	u8 th_hl_diff;
	u8 edcca_mode;
	u8 th_h_lb;
	u8 th_h_5g;
	u8 th_h_2p4g;
	u8 th_h_cs;
	u32 rvrt_val; /*all rvrt_val for pause API must set to u32*/
	struct edcca_hw_rpt edcca_rpt;
};
#ifdef HALBB_DYN_L2H_SUPPORT
struct bb_dyn_l2h_info {
	bool en_dyn_l2h;
	u32 low_rate_rty_cnt;
	u32 drop_cnt;
	u8 l2h_th;
};
#endif

struct bb_info;
/*@--------------------------[Prptotype]-------------------------------------*/
void halbb_edcca(struct bb_info *bb);
void halbb_edcca_thre_calc(struct bb_info * bb);
void halbb_set_edcca_pause_val(struct bb_info *bb, u32 *val_buf, u8 val_len);
void halbb_edcca_event_nofity(struct bb_info * bb, u8 pause_type);
void halbb_edcca_dev_hw_cap(struct bb_info * bb);
void halbb_edcca_init(struct bb_info *bb);
void halbb_cr_cfg_edcca_init(struct bb_info *bb);
void halbb_edcca_dbg(struct bb_info *bb, char input[][16], u32 *_used,
			      char *output, u32 *_out_len);

#endif

