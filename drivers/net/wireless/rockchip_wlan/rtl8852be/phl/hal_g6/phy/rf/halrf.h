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

#ifndef __HALRF_H__
#define __HALRF_H__

/*@--------------------------[Define] ---------------------------------------*/

/*H2C cmd ID*/
/*Class 8*/
/*Class 9*/
/*Class a*/
#define FWCMD_H2C_BACKUP_RFK 0
#define FWCMD_H2C_RELOAD_RFK 1
#define FWCMD_H2C_GET_MCCCH 2
#define FWCMD_H2C_DPK_OFFLOAD 3
#define FWCMD_H2C_IQK_OFFLOAD 4

/*@--------------------------[Enum]------------------------------------------*/
enum halrf_func_idx {
	RF00_PWR_TRK = 0,
	RF01_IQK = 1,
	RF02_LCK = 2,
	RF03_DPK = 3,
	RF04_TXGAPK = 4,
	RF05_DACK = 5,
	RF06_DPK_TRK = 6,
	RF07_2GBAND_SHIFT = 7,
	RF08_RXDCK = 8,
	RF09_RFK = 9,
	RF10_RF_INIT = 10,
	RF11_RF_POWER = 11,
	RF12_RXGAINK = 12,
	RF13_THER_TRIM = 13,
	RF14_PABIAS_TRIM = 14,
	RF15_TSSI_TRIM = 15,
	RF16_PSD = 16,
	RF17_TSSI_TRK = 17,
	RF18_XTAL_TRK = 18,
	RF19_TX_SHAPE = 19
};

enum halrf_rf_mode {
	RF_SHUT_DOWN = 0x0,
	RF_STANDBY = 0x1,
	RF_TX = 0x2,
	RF_RX = 0x3,
	RF_TXIQK = 0x4,
	RF_DPK = 0x5,
	RF_RXK1 = 0x6,
	RF_RXK2 = 0x7
};

enum halrf_rfe_src_sel {
	HALRF_PAPE_RFM		= 0,
	HALRF_GNT_BT_INV	= 1,
	HALRF_LNA0N		= 2,
	HALRF_LNAON_RFM		= 3,
	HALRF_TRSW_RFM		= 4,
	HALRF_TRSW_RFM_B	= 5,
	HALRF_GNT_BT		= 6,
	HALRF_ZERO		= 7,
	HALRF_ANTSEL_0		= 8,
	HALRF_ANTSEL_1		= 9,
	HALRF_ANTSEL_2		= 0xa,
	HALRF_ANTSEL_3		= 0xb,
	HALRF_ANTSEL_4		= 0xc,
	HALRF_ANTSEL_5		= 0xd,
	HALRF_ANTSEL_6		= 0xe,
	HALRF_ANTSEL_7		= 0xf
};

/*@=[HALRF supportability]=======================================*/
enum halrf_ability {
	HAL_RF_TX_PWR_TRACK = BIT(RF00_PWR_TRK),
	HAL_RF_IQK = BIT(RF01_IQK),
	HAL_RF_LCK = BIT(RF02_LCK),
	HAL_RF_DPK = BIT(RF03_DPK),
	HAL_RF_TXGAPK = BIT(RF04_TXGAPK),
	HAL_RF_DACK = BIT(RF05_DACK),
	HAL_RF_DPK_TRACK = BIT(RF06_DPK_TRK),
	HAL_2GBAND_SHIFT = BIT(RF07_2GBAND_SHIFT),
	HAL_RF_RXDCK = BIT(RF08_RXDCK),
	HAL_RF_RXGAINK = BIT(RF12_RXGAINK),
	HAL_RF_THER_TRIM = BIT(RF13_THER_TRIM),
	HAL_RF_PABIAS_TRIM = BIT(RF14_PABIAS_TRIM),
	HAL_RF_TSSI_TRIM = BIT(RF15_TSSI_TRIM),
	HAL_RF_TSSI_TRK = BIT(RF17_TSSI_TRK),
	HAL_RF_XTAL_TRACK = BIT(RF18_XTAL_TRK),
	HAL_RF_TX_SHAPE = BIT(RF19_TX_SHAPE)
};

/*@=[HALRF Debug Component]=====================================*/
enum halrf_dbg_comp {
	DBG_RF_TX_PWR_TRACK = BIT(RF00_PWR_TRK),
	DBG_RF_IQK = BIT(RF01_IQK),
	DBG_RF_LCK = BIT(RF02_LCK),
	DBG_RF_DPK = BIT(RF03_DPK),
	DBG_RF_TXGAPK = BIT(RF04_TXGAPK),
	DBG_RF_DACK = BIT(RF05_DACK),
	DBG_RF_DPK_TRACK = BIT(RF06_DPK_TRK),
	DBG_RF_RXDCK = BIT(RF08_RXDCK),
	DBG_RF_RFK = BIT(RF09_RFK),
	DBG_RF_INIT = BIT(RF10_RF_INIT),
	DBG_RF_POWER = BIT(RF11_RF_POWER),
	DBG_RF_RXGAINK = BIT(RF12_RXGAINK),
	DBG_RF_THER_TRIM = BIT(RF13_THER_TRIM),
	DBG_RF_PABIAS_TRIM = BIT(RF14_PABIAS_TRIM),
	DBG_RF_TSSI_TRIM = BIT(RF15_TSSI_TRIM),
	DBG_RF_PSD = BIT(RF16_PSD),
	DBG_RF_XTAL_TRACK = BIT(RF18_XTAL_TRK),
	DBG_RF_FW = BIT(28),
	DBG_RF_MP = BIT(29),
	DBG_RF_TMP = BIT(30),
	DBG_RF_CHK = BIT(31)
};

 
/*@--------------------------[Structure]-------------------------------------*/

struct rfk_location {
	enum band_type cur_band;
	enum channel_width cur_bw;
	u8 cur_ch;
};

struct halrf_fem_info {
	u8 elna_2g;		/*@with 2G eLNA  NO/Yes = 0/1*/
	u8 elna_5g;		/*@with 5G eLNA  NO/Yes = 0/1*/
	u8 elna_6g;		/*@with 6G eLNA  NO/Yes = 0/1*/
	u8 epa_2g;		/*@with 2G ePA    NO/Yes = 0/1*/
	u8 epa_5g;		/*@with 5G ePA    NO/Yes = 0/1*/
	u8 epa_6g;		/*@with 6G ePA    NO/Yes = 0/1*/
};

#if 1 /* all rf operation usage (header) */

/* clang-format on */
#define RF_PATH_MAX_NUM (8)
#define RF_TASK_RECORD_MAX_TIMES (16)
#define RF_BACKUP_MAC_REG_MAX_NUM (16)
#define RF_BACKUP_BB_REG_MAX_NUM (16)
#define RF_BACKUP_RF_REG_MAX_NUM (16)

struct halrf_iqk_ops {
	u8 (*iqk_kpath)(struct rf_info *rf, enum phl_phy_idx phy_idx);	
	bool (*iqk_mcc_page_sel)(struct rf_info *rf, enum phl_phy_idx phy,  u8 path);
	void (*iqk_get_ch_info)(struct rf_info *rf, enum phl_phy_idx phy,  u8 path);	
	void (*iqk_preset)(struct rf_info *rf, u8 path);	
	void (*iqk_macbb_setting)(struct rf_info *rf, enum phl_phy_idx phy_idx, u8 path);
	void (*iqk_start_iqk)(struct rf_info *rf, enum phl_phy_idx phy_idx, u8 path);
	void (*iqk_restore)(struct rf_info *rf, u8 path);
	void (*iqk_afebb_restore)(struct rf_info *rf, enum phl_phy_idx phy_idx, u8 path);	
};

struct rfk_iqk_info {
	struct halrf_iqk_ops *rf_iqk_ops;
	u8 rf_max_path_num;
	u32 rf_iqk_version;
	u8 rf_iqk_ch_num;
	u8 rf_iqk_path_num;

	const u32 *backup_mac_reg;
	u32 backup_mac_reg_num;
	const u32 *backup_bb_reg;
	u32 backup_bb_reg_num;
	const u32 *backup_rf_reg;
	u32 backup_rf_reg_num;
};
/* clang-format off */
#endif /* all rf operation usage (header) */

#ifdef HALRF_CONFIG_FW_IO_OFLD_SUPPORT
struct halrf_fw_offload {
	enum rtw_mac_src_cmd_ofld src;
	enum rtw_mac_cmd_type_ofld type;
	u8 lc;
	enum rtw_mac_rf_path rf_path;
	u16 offset;
	u16 id;
	u32 value;
	u32 mask;
};
#endif

struct halrf_rx_dck_info {
	bool is_afe;
	struct rfk_location loc[KPATH]; /*max RF path*/
};

struct rf_info {
	struct rtw_phl_com_t	*phl_com;
	struct rtw_hal_com_t	*hal_com;
	/*[Common Info]*/
	u32			ic_type;
	u8			num_rf_path;
	/*[System Info]*/
	bool			rf_init_ready;
	u32			rf_sys_up_time;
	bool			rf_watchdog_en;
	bool			rf_ic_api_en;
	/*[DM Info]*/
	u32			support_ability;
	u32			manual_support_ability;
	/*[FW Info]*/
	u32			fw_dbg_component;
	/*[Drv Dbg Info]*/
	u32			dbg_component;
	u8			cmn_dbg_msg_period;
	u8			cmn_dbg_msg_cnt;
	/*[BTC / RFK Info ]*/
	bool 			rfk_is_processing;
	bool			is_bt_iqk_timeout;
	/*[initial]*/
	u8 		pre_rxbb_bw[KPATH];
	/*[TSSI Info]*/
	bool		is_tssi_mode[4]; /*S0/S1*/
	/*[Thermal Trigger]*/
	bool		is_thermal_trigger;
	/*[Do Coex]*/
	bool		is_coex;
	/*[watchdog]*/
	bool		is_watchdog_stop;
	/*[thermal rek indictor]*/	
	bool rfk_do_thr_rek;
	/*reg check*/
	u32	rfk_reg[2048];
	u32	rfc_reg[2][10];
	u32	rfk_check_fail_count;
	/*fast channel switch*/
	u8	ther_init;
	u32	fcs_rfk_ok_map;
	u8	pre_chl_idx;
	u8	pre_ther_idx;
	/* [Check NCTL Done status Read Times] */
	u32 nctl_ck_times[2];  /* 0xbff8 0x80fc*/
	u32	fw_ofld_enable;
	
	/*@=== [HALRF Structure] ============================================*/
	struct halrf_pwr_track_info	pwr_track;
	struct halrf_tssi_info		tssi;
	struct halrf_xtal_info		xtal_track;
	struct halrf_iqk_info		iqk;
	struct halrf_dpk_info	dpk;
	struct halrf_rx_dck_info rx_dck;
	struct halrf_dack_info	dack;
	struct halrf_gapk_info	gapk;
	struct halrf_pwr_info	pwr_info;
	struct halrf_radio_info	radio_info;
	struct halrf_fem_info	fem;
	struct rf_dbg_cmd_info	rf_dbg_cmd_i;
	struct halrf_kfree_info	kfree_info;
	struct halrf_psd_data	psd;
	struct rfk_location		iqk_loc[2];	/*S0/S1*/
	struct rfk_location		dpk_loc[2];	/*S0/S1*/
	struct rfk_location		gapk_loc[2];	/*S0/S1*/
	struct rfk_iqk_info	*rfk_iqk_info;
#ifdef HALRF_CONFIG_FW_IO_OFLD_SUPPORT
	struct halrf_fw_offload fwofld;
#endif
};

/*@--------------------------[Prptotype]-------------------------------------*/


#endif

