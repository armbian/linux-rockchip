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
#ifndef _HAL_GENERAL_DEF_H_
#define _HAL_GENERAL_DEF_H_

#define RTW_MAC_TBTT_AGG_DEF 1

enum rtw_chip_id {
	CHIP_WIFI6_8852A,
	CHIP_WIFI6_8834A,
	CHIP_WIFI6_8852B,
	CHIP_WIFI6_8852C,
	CHIP_WIFI6_MAX
};

enum rtw_efuse_info {
	/* MAC Part */
	EFUSE_INFO_MAC_ADDR,
	EFUSE_INFO_MAC_PID,
	EFUSE_INFO_MAC_DID,
	EFUSE_INFO_MAC_VID,
	EFUSE_INFO_MAC_SVID,
	EFUSE_INFO_MAC_SMID,
	EFUSE_INFO_MAC_MAX,
	/* BB Part */
	EFUSE_INFO_BB_ANTDIV,
	EFUSE_INFO_BB_MAX,
	/* RF Part */
	EFUSE_INFO_RF_PKG_TYPE,
	EFUSE_INFO_RF_PA,
	EFUSE_INFO_RF_VALID_PATH,
	EFUSE_INFO_RF_RFE,
	EFUSE_INFO_RF_TXPWR,
	EFUSE_INFO_RF_BOARD_OPTION,
	EFUSE_INFO_RF_CHAN_PLAN,
	EFUSE_INFO_RF_CHAN_PLAN_FORCE_HW,
	EFUSE_INFO_RF_COUNTRY,
	EFUSE_INFO_RF_THERMAL,
	EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_1,
	EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_2,
	EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_3,
	EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_4,
	EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_5,
	EFUSE_INFO_RF_2G_CCK_A_TSSI_DE_6,
	EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_1,
	EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_2,
	EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_3,
	EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_4,
	EFUSE_INFO_RF_2G_BW40M_A_TSSI_DE_5,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_1,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_2,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_3,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_4,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_5,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_6,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_7,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_8,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_9,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_10,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_11,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_12,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_13,
	EFUSE_INFO_RF_5G_BW40M_A_TSSI_DE_14,
	EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_1,
	EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_2,
	EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_3,
	EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_4,
	EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_5,
	EFUSE_INFO_RF_2G_CCK_B_TSSI_DE_6,
	EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_1,
	EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_2,
	EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_3,
	EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_4,
	EFUSE_INFO_RF_2G_BW40M_B_TSSI_DE_5,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_1,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_2,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_3,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_4,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_5,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_6,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_7,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_8,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_9,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_10,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_11,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_12,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_13,
	EFUSE_INFO_RF_5G_BW40M_B_TSSI_DE_14,
	EFUSE_INFO_RF_2G_CCK_C_TSSI_DE_1,
	EFUSE_INFO_RF_2G_CCK_C_TSSI_DE_2,
	EFUSE_INFO_RF_2G_CCK_C_TSSI_DE_3,
	EFUSE_INFO_RF_2G_CCK_C_TSSI_DE_4,
	EFUSE_INFO_RF_2G_CCK_C_TSSI_DE_5,
	EFUSE_INFO_RF_2G_CCK_C_TSSI_DE_6,
	EFUSE_INFO_RF_2G_BW40M_C_TSSI_DE_1,
	EFUSE_INFO_RF_2G_BW40M_C_TSSI_DE_2,
	EFUSE_INFO_RF_2G_BW40M_C_TSSI_DE_3,
	EFUSE_INFO_RF_2G_BW40M_C_TSSI_DE_4,
	EFUSE_INFO_RF_2G_BW40M_C_TSSI_DE_5,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_1,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_2,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_3,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_4,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_5,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_6,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_7,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_8,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_9,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_10,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_11,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_12,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_13,
	EFUSE_INFO_RF_5G_BW40M_C_TSSI_DE_14,
	EFUSE_INFO_RF_2G_CCK_D_TSSI_DE_1,
	EFUSE_INFO_RF_2G_CCK_D_TSSI_DE_2,
	EFUSE_INFO_RF_2G_CCK_D_TSSI_DE_3,
	EFUSE_INFO_RF_2G_CCK_D_TSSI_DE_4,
	EFUSE_INFO_RF_2G_CCK_D_TSSI_DE_5,
	EFUSE_INFO_RF_2G_CCK_D_TSSI_DE_6,
	EFUSE_INFO_RF_2G_BW40M_D_TSSI_DE_1,
	EFUSE_INFO_RF_2G_BW40M_D_TSSI_DE_2,
	EFUSE_INFO_RF_2G_BW40M_D_TSSI_DE_3,
	EFUSE_INFO_RF_2G_BW40M_D_TSSI_DE_4,
	EFUSE_INFO_RF_2G_BW40M_D_TSSI_DE_5,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_1,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_2,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_3,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_4,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_5,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_6,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_7,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_8,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_9,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_10,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_11,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_12,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_13,
	EFUSE_INFO_RF_5G_BW40M_D_TSSI_DE_14,
	EFUSE_INFO_RF_THERMAL_A,
	EFUSE_INFO_RF_THERMAL_B,
	EFUSE_INFO_RF_THERMAL_C,
	EFUSE_INFO_RF_THERMAL_D,
	EFUSE_INFO_RF_XTAL,
	/*RX Gain K*/
        EFUSE_INFO_RF_RX_GAIN_K_A_2G_CCK,
        EFUSE_INFO_RF_RX_GAIN_K_A_2G_OFMD,
        EFUSE_INFO_RF_RX_GAIN_K_A_5GL,
        EFUSE_INFO_RF_RX_GAIN_K_A_5GM,
        EFUSE_INFO_RF_RX_GAIN_K_A_5GH,
	EFUSE_INFO_RF_MAX,
	/* BTCOEX Part */
	EFUSE_INFO_BTCOEX_COEX,
	EFUSE_INFO_BTCOEX_ANT_NUM,
	EFUSE_INFO_BTCOEX_ANT_PATH,
	EFUSE_INFO_BTCOEX_MAX,
};

enum rtw_cv {
	CAV,
	CBV,
	CCV,
	CDV,
	CEV,
	CFV,
	CGV,
	CTV,
	CMAXV,
};

enum rtw_fv {
	FTV,
	FUV,
	FSV,
};

enum rtw_dv_sel {
	DAV,
	DDV,
};

enum hal_pwr_by_rate_setting {
	PW_BY_RATE_ON = 0,
	PW_BY_RATE_ALL_SAME = 1
};

enum hal_pwr_limit_type {
	PWLMT_BY_EFUSE = 0,
	PWLMT_DISABLE = 1,
	PWBYRATE_AND_PWLMT = 2
};

enum rtw_mac_gfunc {
	RTW_MAC_GPIO_WL_PD,
	RTW_MAC_GPIO_BT_PD,
	RTW_MAC_GPIO_WL_EXTWOL,
	RTW_MAC_GPIO_BT_GPIO,
	RTW_MAC_GPIO_WL_SDIO_INT,
	RTW_MAC_GPIO_BT_SDIO_INT,
	RTW_MAC_GPIO_WL_FLASH,
	RTW_MAC_GPIO_BT_FLASH,
	RTW_MAC_GPIO_SIC,
	RTW_MAC_GPIO_LTE_UART,
	RTW_MAC_GPIO_LTE_3W,
	RTW_MAC_GPIO_WL_PTA,
	RTW_MAC_GPIO_BT_PTA,
	RTW_MAC_GPIO_MAILBOX,
	RTW_MAC_GPIO_WL_LED,
	RTW_MAC_GPIO_OSC,
	RTW_MAC_GPIO_XTAL_CLK,
	RTW_MAC_GPIO_EXT_XTAL_CLK,
	RTW_MAC_GPIO_DBG_GNT,
	RTW_MAC_GPIO_WL_RFE_CTRL,
	RTW_MAC_GPIO_BT_UART_RQB,
	RTW_MAC_GPIO_BT_WAKE_HOST,
	RTW_MAC_GPIO_HOST_WAKE_BT,
	RTW_MAC_GPIO_DBG,
	RTW_MAC_GPIO_WL_UART_TX,
	RTW_MAC_GPIO_WL_UART_RX,
	RTW_MAC_GPIO_WL_JTAG,
	RTW_MAC_GPIO_SW_IO,

	/* keep last */
	RTW_MAC_GPIO_LAST,
	RTW_MAC_GPIO_MAX = RTW_MAC_GPIO_LAST,
	RTW_MAC_GPIO_INVALID = RTW_MAC_GPIO_LAST,
	RTW_MAC_GPIO_DFLT = RTW_MAC_GPIO_LAST,
};


#ifdef CONFIG_FW_IO_OFLD_SUPPORT
enum rtw_mac_src_cmd_ofld {
	RTW_MAC_BB_CMD_OFLD = 0,
	RTW_MAC_RF_CMD_OFLD,
	RTW_MAC_MAC_CMD_OFLD,
	RTW_MAC_OTHER_CMD_OFLD
};
enum rtw_mac_cmd_type_ofld {
	RTW_MAC_WRITE_OFLD = 0,
	RTW_MAC_COMPARE_OFLD,
	RTW_MAC_DELAY_OFLD
};
enum rtw_mac_rf_path {
	RTW_MAC_RF_PATH_A = 0,   //Radio Path A
	RTW_MAC_RF_PATH_B,	//Radio Path B
	RTW_MAC_RF_PATH_C,	//Radio Path C
	RTW_MAC_RF_PATH_D,	//Radio Path D
};
struct rtw_mac_cmd {
	enum rtw_mac_src_cmd_ofld src;
	enum rtw_mac_cmd_type_ofld type;
	u8 lc;
	enum rtw_mac_rf_path rf_path;
	u16 offset;
	u16 id;
	u32 value;
	u32 mask;
};
enum rtw_fw_ofld_cap {
	FW_CAP_IO_OFLD = BIT(0),
};
#endif

enum wl_func {
	EFUSE_WL_FUNC_NONE = 0,
	EFUSE_WL_FUNC_DRAGON = 0xe,
	EFUSE_WL_FUNC_GENERAL = 0xf
};

enum hw_stype{
	EFUSE_HW_STYPE_NONE = 0x0,
	EFUSE_HW_STYPE_GENERAL = 0xf
};

struct rtw_hal_mac_ax_cctl_info {
	/* dword 0 */
	u32 datarate:9;
	u32 force_txop:1;
	u32 data_bw:2;
	u32 data_gi_ltf:3;
	u32 darf_tc_index:1;
	u32 arfr_ctrl:4;
	u32 acq_rpt_en:1;
	u32 mgq_rpt_en:1;
	u32 ulq_rpt_en:1;
	u32 twtq_rpt_en:1;
	u32 rsvd0:1;
	u32 disrtsfb:1;
	u32 disdatafb:1;
	u32 tryrate:1;
	u32 ampdu_density:4;
	/* dword 1 */
	u32 data_rty_lowest_rate:9;
	u32 ampdu_time_sel:1;
	u32 ampdu_len_sel:1;
	u32 rts_txcnt_lmt_sel:1;
	u32 rts_txcnt_lmt:4;
	u32 rtsrate:9;
	u32 rsvd1:2;
	u32 vcs_stbc:1;
	u32 rts_rty_lowest_rate:4;
	/* dword 2 */
	u32 data_tx_cnt_lmt:6;
	u32 data_txcnt_lmt_sel:1;
	u32 max_agg_num_sel:1;
	u32 rts_en:1;
	u32 cts2self_en:1;
	u32 cca_rts:2;
	u32 hw_rts_en:1;
	u32 rts_drop_data_mode:2;
	u32 rsvd2:1;
	u32 ampdu_max_len:11;
	u32 ul_mu_dis:1;
	u32 ampdu_max_time:4;
	/* dword 3 */
	u32 max_agg_num:8;
	u32 ba_bmap:2;
	u32 rsvd3:6;
	u32 vo_lftime_sel:3;
	u32 vi_lftime_sel:3;
	u32 be_lftime_sel:3;
	u32 bk_lftime_sel:3;
	u32 sectype:4;
	/* dword 4 */
	u32 multi_port_id:3;
	u32 bmc:1;
	u32 mbssid:4;
	u32 navusehdr:1;
	u32 txpwr_mode:3;
	u32 data_dcm:1;
	u32 data_er:1;
	u32 data_ldpc:1;
	u32 data_stbc:1;
	u32 a_ctrl_bqr:1;
	u32 a_ctrl_uph:1;
	u32 a_ctrl_bsr:1;
	u32 a_ctrl_cas:1;
	u32 data_bw_er:1;
	u32 lsig_txop_en:1;
	u32 rsvd4:5;
	u32 ctrl_cnt_vld:1;
	u32 ctrl_cnt:4;
	/* dword 5 */
	u32 resp_ref_rate:9;
	u32 rsvd5:3;
	u32 all_ack_support:1;
	u32 bsr_queue_size_format:1;
	u32 rsvd6:1;
	u32 rsvd7:1;
	u32 ntx_path_en:4;
	u32 path_map_a:2;
	u32 path_map_b:2;
	u32 path_map_c:2;
	u32 path_map_d:2;
	u32 antsel_a:1;
	u32 antsel_b:1;
	u32 antsel_c:1;
	u32 antsel_d:1;
	/* dword 6 */
	u32 addr_cam_index:8;
	u32 paid:9;
	u32 uldl:1;
	u32 doppler_ctrl:2;
	u32 nominal_pkt_padding:2;
	u32 nominal_pkt_padding40:2;
	u32 txpwr_tolerence:4;
	u32 rsvd9:2;
	u32 nominal_pkt_padding80:2;
	/* dword 7 */
	u32 nc:3;
	u32 nr:3;
	u32 ng:2;
	u32 cb:2;
	u32 cs:2;
	u32 csi_txbf_en:1;
	u32 csi_stbc_en:1;
	u32 csi_ldpc_en:1;
	u32 csi_para_en:1;
	u32 csi_fix_rate:9;
	u32 csi_gi_ltf:3;
	u32 nominal_pkt_padding160:2;
	u32 csi_bw:2;
};

#endif /* _HAL_GENERAL_DEF_H_*/

