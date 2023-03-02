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
#ifndef __HALRF_DPK_H__
#define __HALRF_DPK_H__

/*@--------------------------Define Parameters-------------------------------*/
#define AVG_THERMAL_NUM_DPK 8
#define THERMAL_DPK_AVG_NUM 1

#if defined (RF_8852A_SUPPORT) || defined (RF_8852B_SUPPORT)
#define KPATH 2
#else
#define KPATH 1
#endif
#define DPK_BKUP_NUM 2

enum dpk_id {
	LBK_RXIQK	= 0x06,
	SYNC		= 0x10,
	MDPK_IDL	= 0x11,
	MDPK_MPA	= 0x12,
	GAIN_LOSS	= 0x13,
	GAIN_CAL	= 0x14,
	DPK_RXAGC	= 0x15,
	KIP_PRESET	= 0x16,
	KIP_RESTORE	= 0x17,
	DPK_TXAGC	= 0x19
};

struct dpk_bkup_para {
	enum band_type	band;		/* 2.4G,5G,6G*/
	enum channel_width	bw;
	u8	ch;
	u8	path_ok;
	u8	txagc_dpk;		/*txagc@dpk with path*/	
	u8	ther_dpk;		/*thermal@dpk with path*/	
	//u8	trk_idx_dpk;		/*track_idx@dpk with path*/	
	//u8	ther_tssi;		/*thermal@tssi with path*/
	u8	gs;
	u16	pwsf;
};

/*@---------------------------End Define Parameters---------------------------*/

struct halrf_dpk_info {

	bool	is_dpk_enable;
	bool	is_dpk_reload_en;
	u8	dpk_gs[2];			/*PHY*/
	u8	ther_avg[KPATH][AVG_THERMAL_NUM_DPK];	/*path*/
	u8	pre_pwsf[KPATH];
	u8	ther_avg_idx;
	u32	dpk_cal_cnt;
	u32	dpk_ok_cnt;
	u32	dpk_reload_cnt;
	u16	dc_i[KPATH][DPK_BKUP_NUM];		/*path*/
	u16	dc_q[KPATH][DPK_BKUP_NUM];		/*path*/
	u8	corr_val[KPATH][DPK_BKUP_NUM];		/*path*/
	u8	corr_idx[KPATH][DPK_BKUP_NUM];		/*path*/
	u8	cur_idx[KPATH];				/*path*/

	struct dpk_bkup_para bp[KPATH][DPK_BKUP_NUM];	/*path/index*/
};

#endif /*__HALRF_DPK_H__*/

