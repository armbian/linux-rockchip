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
#ifndef __HALBB_CH_INFO_H__
#define __HALBB_CH_INFO_H__

#ifdef HALBB_CH_INFO_SUPPORT
/*@--------------------------[Define] ---------------------------------------*/
#define TEST_CH_INFO_MAX_SEG	4
#define TEST_CH_SEG_LEN	(12 * 8) /*Byte*/
/*@--------------------------[Enum]------------------------------------------*/

/*@--------------------------[Structure]-------------------------------------*/

enum bb_ch_info_state_type {
	CH_RPT_START_TO_WAIT = 0,
	CH_RPT_GETTED = 1
};

struct bb_ch_info_physts_info {
	enum bb_ch_info_state_type ch_info_state;
	enum bb_physts_bitmap_t force_bitmap_type; /*force setting*/
	enum bb_physts_bitmap_t bitmap_type_rpt; /*report*/
	bool bitmap_type_auto_en;
	u16 ch_info_len;
};

struct bb_ch_info_raw_info {
	s16 *octet;
	u32 ch_info_buf_len; /*Byte*/
};

struct bb_ch_info_cr_info {
	u32 ch_info_en_0;
	u32 ele_bitmap;
	u32 ch_info_type;
	u32 ch_info_type_m;
	u32 seg_len;
	u32 seg_len_m;
};

struct bb_ch_rpt_size_info {
	u8 data_byte;
	u8 n_c;
	u8 n_r;
	u16 n_tone;
	u8 n_group;
	u16 ch_info_rpt_len_legcy; /*Lgacy; Byte; ch_info rpt length calculated by BB CR configuration*/
	u16 ch_info_rpt_len[4]; /*HT/VHT 20,40,80; Byte; ch_info rpt length calculated by BB CR configuration*/
	u16 ch_info_rpt_len_he[4]; /*HE 20,40,80; Byte; ch_info rpt length calculated by BB CR configuration*/
	u16 per_tone_ch_rpt_size;
};

struct bb_ch_rpt_info {
	u8 seg_idx_pre;
	u8 seg_total_num;
	u32 total_len;  /*Raw data length(Unit: byte) = total_len - 16*/
	u32 total_len_remnant;
	u16 ch_rpt_hdr_len;
	u16 phy_info_len;
	bool skip_ch_info;
	u32 raw_data_len_acc;
	u8 *test_buf;
	u8 *test_buf_curr;
	struct bb_ch_info_cr_cfg_info bb_ch_info_cr_cfg_i;
	struct bb_ch_info_cr_info bb_ch_info_cr_i; /*CR callback table*/
	struct bb_ch_rpt_size_info bb_ch_rpt_size_i;
	struct bb_ch_info_raw_info	bb_ch_info_raw_i;
	struct bb_ch_info_physts_info bb_ch_info_physts_i;
};

struct bb_info;
/*@--------------------------[Prptotype]-------------------------------------*/
bool halbb_ch_info_wait_from_physts(struct bb_info *bb, u32 dly, u32 dly_max,
				    enum bb_physts_bitmap_t type);
void halbb_ch_info_buf_rls(struct bb_info *bb);
bool halbb_ch_info_buf_alloc(struct bb_info *bb);
void halbb_ch_info_dbg(struct bb_info *bb, char input[][16], u32 *_used,
		       char *output, u32 *_out_len);
void halbb_ch_info_deinit(struct bb_info *bb);
void halbb_ch_info_init(struct bb_info *bb);
void halbb_cr_cfg_ch_info_init(struct bb_info *bb);
#endif
#endif