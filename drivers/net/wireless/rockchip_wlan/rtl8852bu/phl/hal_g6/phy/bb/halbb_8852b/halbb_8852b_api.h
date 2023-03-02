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
#ifndef __HALBB_8852B_API_H__
#define __HALBB_8852B_API_H__
#ifdef BB_8852B_SUPPORT
#ifdef HALBB_CONFIG_RUN_IN_DRV
#include "../halbb_api.h"
#endif

/*@--------------------------[Define]-------------------------------------*/
#define INVALID_RF_DATA 0xffffffff
#define BB_PATH_MAX_8852B 2
#define GAIN_HIDE_EFUSE_A_2G_8852B 0x5BB
#define GAIN_HIDE_EFUSE_A_5GL_8852B 0x5BA
#define GAIN_HIDE_EFUSE_A_5GM_8852B 0x5B9
#define GAIN_HIDE_EFUSE_A_5GH_8852B 0x5B8
#define GAIN_HIDE_EFUSE_B_2G_8852B 0x590
#define GAIN_HIDE_EFUSE_B_5GL_8852B 0x58F
#define GAIN_HIDE_EFUSE_B_5GM_8852B 0x58E
#define GAIN_HIDE_EFUSE_B_5GH_8852B 0x58D

/*@--------------------------[Enum]------------------------------------------*/
/*@--------------------------[Prptotype]-------------------------------------*/
struct bb_info;
bool halbb_set_pwr_ul_tb_ofst_8852b(struct bb_info *bb,
				    s16 pw_ofst, enum phl_phy_idx phy_idx);
void halbb_tx_triangular_shap_cfg_8852b(struct bb_info *bb, u8 shape_idx,
					enum phl_phy_idx phy_idx);
void halbb_tx_dfir_shap_cck_8852b(struct bb_info *bb, u8 ch, u8 shape_idx,
				  enum phl_phy_idx phy_idx);
bool halbb_ctrl_bw_ch_8852b(struct bb_info *bb, u8 pri_ch, u8 central_ch,
			    enum channel_width bw, enum band_type band,
			    enum phl_phy_idx phy_idx);

bool halbb_ctrl_rx_path_8852b(struct bb_info *bb, enum rf_path rx_path);

bool halbb_ctrl_tx_path_8852b(struct bb_info *bb, enum rf_path tx_path);

#ifdef HALBB_CONFIG_RUN_IN_DRV

void halbb_gpio_ctrl_dump_8852b(struct bb_info *bb);

void halbb_gpio_rfm_8852b(struct bb_info *bb, enum bb_path path,
			    enum bb_rfe_src_sel src, bool dis_tx_gnt_wl,
			    bool active_tx_opt, bool act_bt_en, u8 rfm_output_val);

void halbb_gpio_trsw_table_8852b(struct bb_info *bb, enum bb_path path,
				   bool path_en, bool trsw_tx,
				   bool trsw_rx, bool trsw, bool trsw_b);

void halbb_gpio_setting_8852b(struct bb_info *bb, u8 gpio_idx,
			        enum bb_path path, bool inv,
			        enum bb_rfe_src_sel src);

void halbb_gpio_setting_all_8852b(struct bb_info *bb, u8 rfe_idx);

void halbb_gpio_setting_init_8852b(struct bb_info *bb);

void halbb_bb_reset_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx);

void halbb_dfs_en_8852b(struct bb_info *bb, bool en);

void halbb_adc_en_8852b(struct bb_info *bb, bool en);

void halbb_tssi_cont_en_8852b(struct bb_info *bb, bool en, enum rf_path path);

void halbb_bb_reset_en_8852b(struct bb_info *bb, bool en, enum phl_phy_idx phy_idx);

u32 halbb_read_rf_reg_8852b_a(struct bb_info *bb, enum rf_path path,
			      u32 reg_addr, u32 bit_mask);

u32 halbb_read_rf_reg_8852b_d(struct bb_info *bb, enum rf_path path,
			      u32 reg_addr, u32 bit_mask);

u32 halbb_read_rf_reg_8852b(struct bb_info *bb, enum rf_path path, u32 reg_addr,
			    u32 bit_mask);

bool halbb_write_rf_reg_8852b_a(struct bb_info *bb, enum rf_path path,
				u32 reg_addr, u32 bit_mask, u32 data);

bool halbb_write_rf_reg_8852b_d(struct bb_info *bb, enum rf_path path,
				u32 reg_addr, u32 bit_mask, u32 data);

bool halbb_write_rf_reg_8852b(struct bb_info *bb, enum rf_path path, u32 reg_addr,
			      u32 bit_mask, u32 data);

bool halbb_ctrl_bw_8852b(struct bb_info *bb, u8 pri_ch, enum channel_width bw,
			 enum phl_phy_idx phy_idx);

bool halbb_ctrl_ch_8852b(struct bb_info *bb, u8 central_ch,
			 enum phl_phy_idx phy_idx);

void halbb_ctrl_cck_en_8852b(struct bb_info *bb, bool cck_en,
			     enum phl_phy_idx phy_idx);

void halbb_ctrl_trx_path_8852b(struct bb_info *bb, enum rf_path tx_path,
			       u8 tx_nss, enum rf_path rx_path, u8 rx_nss);

void halbb_tssi_bb_reset_8852b(struct bb_info *bb);

u8 halbb_sco_mapping_8852b(struct bb_info *bb,  u8 central_ch);

bool halbb_ctrl_sco_cck_8852b(struct bb_info *bb, u8 pri_ch);

bool halbb_bw_setting_8852b(struct bb_info *bb, enum channel_width bw,
			    enum rf_path path);

bool halbb_ch_setting_8852b(struct bb_info *bb, u8 central_ch, enum rf_path path,
			    bool *is_2g_ch);

void halbb_ctrl_rf_mode_8852b(struct bb_info *bb,  enum phl_rf_mode mode);

bool halbb_ctrl_ch2_80p80_8852b(struct bb_info *bb, u8 central_ch);

void halbb_ctrl_rx_cca_8852b(struct bb_info *bb, bool cca_en, enum phl_phy_idx phy_idx);

void halbb_ctrl_ofdm_en_8852b(struct bb_info *bb, bool ofdm_en,
			      enum phl_phy_idx phy_idx);
bool halbb_set_txpwr_dbm_8852b(struct bb_info *bb, s16 power_dbm,
			       enum phl_phy_idx phy_idx);

void halbb_reset_bb_hw_cnt_8852b(struct bb_info *bb);
	
s16 halbb_get_txpwr_dbm_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx);

s16 halbb_get_txinfo_txpwr_dbm_8852b(struct bb_info *bb);

bool halbb_set_cck_txpwr_idx_8852b(struct bb_info *bb, u16 power_idx,
				   enum rf_path tx_path);
u16 halbb_get_cck_txpwr_idx_8852b(struct bb_info *bb, enum rf_path tx_path);

s16 halbb_get_cck_ref_dbm_8852b(struct bb_info *bb, enum rf_path tx_path);

bool halbb_set_ofdm_txpwr_idx_8852b(struct bb_info *bb, u16 power_idx,
				    enum rf_path tx_path);

u16 halbb_get_ofdm_txpwr_idx_8852b(struct bb_info *bb, enum rf_path tx_path);

s16 halbb_get_ofdm_ref_dbm_8852b(struct bb_info *bb, enum rf_path tx_path);

void halbb_backup_info_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx);

void halbb_restore_info_8852b(struct bb_info *bb, enum phl_phy_idx phy_idx);

void halbb_ctrl_btg_8852b(struct bb_info *bb, bool btg);

void halbb_ctrl_btc_preagc_8852b(struct bb_info *bb, bool bt_en);

bool halbb_set_txsc_8852b(struct bb_info *bb, u8 txsc, enum phl_phy_idx phy_idx);

bool halbb_set_bss_color_8852b(struct bb_info *bb, u8 bss_color, 
			       enum phl_phy_idx phy_idx);

bool halbb_set_sta_id_8852b(struct bb_info *bb, u16 sta_id,
			    enum phl_phy_idx phy_idx);

void halbb_set_igi_8852b(struct bb_info *bb, u8 lna_idx, bool tia_idx,
			u8 rxbb_idx, enum rf_path path);
void halbb_set_tx_pow_ref_8852b(struct bb_info *bb, s16 pw_dbm_ofdm,
				  s16 pw_dbm_cck, s8 ofst,
				  u8 base_cw_0db, u16 tssi_16dBm_cw,
				  u16 *ofdm_cw, u16 *cck_cw,
				  enum phl_phy_idx phy_idx);
void halbb_dump_bb_reg_8852b(struct bb_info *bb, u32 *_used, char *output,
			       u32 *_out_len, bool dump_2_buff);
void halbb_dbgport_dump_all_8852b(struct bb_info *bb, u32 *_used, char *output,
				    u32 *_out_len);
void halbb_physts_brk_fail_pkt_rpt_8852b(struct bb_info *bb, bool enable,
					   enum phl_phy_idx phy_idx);

bool halbb_rf_write_bb_reg_8852b(struct bb_info *bb, u32 addr, u32 mask, u32 data);

void halbb_pre_agc_en_8852b(struct bb_info *bb, bool enable);

void halbb_set_gain_error_8852b(struct bb_info *bb, u8 central_ch);
void halbb_set_efuse_8852b(struct bb_info *bb, u8 central_ch, enum phl_phy_idx phy_idx);
void halbb_get_normal_efuse_init_8852b(struct bb_info *bb);
void halbb_get_hide_efuse_init_8852b(struct bb_info *bb);
void halbb_set_rxsc_rpl_comp_8852b(struct bb_info *bb, u8 central_ch);
void halbb_normal_efuse_verify_8852b(struct bb_info *bb, s8 rx_gain_offset,
				       enum rf_path rx_path,
				       enum phl_phy_idx phy_idx);
void halbb_normal_efuse_verify_cck_8852b(struct bb_info *bb, s8 rx_gain_offset,
					   enum rf_path rx_path,
					   enum phl_phy_idx phy_idx);
#endif
#endif
#endif /*  __INC_PHYDM_API_H_8852A__ */
