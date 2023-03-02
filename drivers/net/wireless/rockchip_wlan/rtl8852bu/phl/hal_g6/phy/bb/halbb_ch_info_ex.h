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
#ifndef __HALBB_CH_INFO_EX_H__
#define __HALBB_CH_INFO_EX_H__

/*@--------------------------[Define] ---------------------------------------*/

/*@--------------------------[Enum]------------------------------------------*/
enum bb_ch_info_en_t {
	CH_INFO_DISABLE = 0,
	CH_INFO_FROM_PHY_STS = 1,
	CH_INFO_FROM_CH_STS = 2
};

enum bb_ch_info_t {
	BB_CH_INFO_SUCCESS = 0,
	BB_CH_INFO_LAST_SEG,
	BB_CH_INFO_FAIL,
};
/*@--------------------------[Structure]-------------------------------------*/

struct bb_ch_info_cr_cfg_info {
	bool	ch_i_phy0_en;
	bool	ch_i_phy1_en;
	bool	ch_i_data_src; /*0~1: CH-estimation, CH-smoothing*/
	bool	ch_i_cmprs; /*0~1: 8/16 bit*/
	u8	ch_i_grp_num_non_he; /*0~3: 1/2/4/16*/
	u8	ch_i_grp_num_he; /*0~3: 1/2/4/16*/
	u8	ch_i_blk_start_idx; /*1~10*/
	u8	ch_i_blk_end_idx; /*1~10*/
	u32	ch_i_ele_bitmap;
	bool	ch_i_type; /*0~1: L-CH, MIMO-CH*/
	u8	ch_i_seg_len; /*0~3: 12/28/60/124 (8byte)*/
};

struct bb_ch_info_buf_cfg_info {
	u8	ch_i_blk_start_idx; /*1~10*/
	u8	ch_i_blk_end_idx; /*1~10*/
	u8	ch_i_seg_len; /*0~3: 12/28/60/124 (8byte)*/
};

struct bb_ch_rpt_hdr_info {
	u16 total_len_l; /*header(16byte) + Raw data length(Unit: byte)*/
	#if (PLATFOM_IS_LITTLE_ENDIAN)
	u8 total_len_m:1;
	u8 total_seg_num:7;
	#else
	u8 total_seg_num:7;
	u8 total_len_m:1;
	#endif
	
	u8 avg_noise_pow;
	#if (PLATFOM_IS_LITTLE_ENDIAN)
	u8 is_pkt_end:1;
	u8 set_valid:1;
	u8 n_rx:3;
	u8 n_sts:3;
	#else
	u8 n_sts:3;
	u8 n_rx:3;
	u8 set_valid:1;
	u8 is_pkt_end:1;
	#endif
	u8 segment_size; /*unit (8Byte)*/
	u8 sts0_evm;
	u8 seq_num;
};

struct bb_phy_info_rpt {
	u8	rssi[2];
	u16	rsvd_0;
	u8	rssi_avg;
	#if (PLATFOM_IS_LITTLE_ENDIAN)
	u8	rxsc:4;
	u8	sts1_evm_l:4;
	u8	sts1_evm_m:4;
	u8	rsvd_1:4;
	#else
	u8	rsvd_1:4;
	u8	sts1_evm_m:4;
	u8	sts1_evm_l:4;
	u8	rxsc:4;
	#endif
	u8	rsvd_2;
};

struct bb_ch_info_drv_rpt {
	u32 raw_data_len;
	u8 seg_idx_curr;
};

struct bb_info;
/*@--------------------------[Prptotype]-------------------------------------*/
void halbb_cfg_ch_info_cr(struct bb_info *bb, struct bb_ch_info_cr_cfg_info *cfg);
void halbb_cfg_ch_info_en(struct bb_info *bb, enum bb_ch_info_en_t en,
			  enum bb_physts_bitmap_t bitmap, enum phl_phy_idx phy_idx);
void halbb_cfg_ch_info_buff(struct bb_info *bb, struct bb_ch_info_buf_cfg_info *cfg);
enum bb_ch_info_t halbb_ch_info_parsing(struct bb_info *bb, u8 *addr, u32 len,
					u8 *rpt_buf,
					struct bb_ch_rpt_hdr_info *hdr,
					struct bb_phy_info_rpt *phy_info,
					struct bb_ch_info_drv_rpt *drv);

#endif
