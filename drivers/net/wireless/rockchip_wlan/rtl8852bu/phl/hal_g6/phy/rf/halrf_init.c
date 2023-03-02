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

void halrf_cmn_info_self_init(struct rf_info *rf)
{
	struct rtw_hal_com_t *hal_i = rf->hal_com;

	if (hal_i->chip_id == CHIP_WIFI6_8852A)
		rf->ic_type = RF_RTL8852A;
	else if (hal_i->chip_id == CHIP_WIFI6_8834A)
		rf->ic_type = RF_RTL8834A;
	else if (hal_i->chip_id == CHIP_WIFI6_8852B)
		rf->ic_type = RF_RTL8852B;
	else if (hal_i->chip_id == CHIP_WIFI6_8852C)
		rf->ic_type = RF_RTL8852C;

	if (rf->ic_type & RF_AX_1SS)
		rf->num_rf_path = 1;
	else if (rf->ic_type & RF_AX_2SS)
		rf->num_rf_path = 2;
	else if (rf->ic_type & RF_AX_3SS)
		rf->num_rf_path = 3;
	else if (rf->ic_type & RF_AX_4SS)
		rf->num_rf_path = 4;
	else
		rf->num_rf_path = 1;

	rf->manual_support_ability = 0xffffffff;
	rf->rf_init_ready = false;
	rf->rf_sys_up_time = 0;
	rf->rf_watchdog_en = true;
	rf->rf_ic_api_en = true;
	/*[Drv Dbg Info]*/
	rf->dbg_component = 0;
	rf->cmn_dbg_msg_period = 2;
	rf->cmn_dbg_msg_cnt = 0;

	/*@=== [HALRF Structure] ============================================*/

	switch (hal_i->chip_id) {
#ifdef RF_8852A_SUPPORT
	case CHIP_WIFI6_8852A:
		rf->rfk_iqk_info = &rf_iqk_hwspec_8852a;
	break;
#endif
#ifdef RF_8852B_SUPPORT
	case CHIP_WIFI6_8852B:
		rf->rfk_iqk_info = &rf_iqk_hwspec_8852b;
	break;
#endif
	default:
	break;
	}
}

void halrf_rfk_self_init(struct rf_info *rf)
{
	struct halrf_iqk_info *iqk_info = &rf->iqk;
	struct halrf_gapk_info *txgapk_info = &rf->gapk;

	u8 path;

	RF_DBG(rf, DBG_RF_RFK, "===> %s\n", __func__);

	/* [TXGAPK init] */
	txgapk_info->is_gapk_init = false;
	
	/*[IQK init]*/
	iqk_info->is_iqk_init = false;

	/*[DPK init]*/
	halrf_dpk_init(rf);

	/*[RXBB BW]*/
	for (path = 0; path < KPATH; path++)
		rf->pre_rxbb_bw[path] = 0xff;
}

void halrf_rfability_init_mp(struct rf_info *rf)
{
	//u64 support_ability = 0;

	switch (rf->ic_type) {
#ifdef RF_8852A_SUPPORT
	case RF_RTL8852A:
		rf->support_ability |=
			/*HAL_RF_TX_PWR_TRACK |*/
			HAL_RF_TSSI_TRK |
			HAL_RF_IQK |
			/*HAL_RF_LCK |*/
			HAL_RF_DPK |
			HAL_RF_DACK |
			HAL_RF_TXGAPK |
			HAL_RF_DPK_TRACK |
			HAL_RF_RXDCK |
			HAL_RF_RXGAINK |
			HAL_RF_THER_TRIM |
			HAL_RF_PABIAS_TRIM |
			HAL_RF_TSSI_TRIM |
			/*HAL_RF_XTAL_TRACK |*/
			0;
		break;
#endif
#ifdef RF_8852B_SUPPORT
	case RF_RTL8852B:
		rf->support_ability |=
			/*HAL_RF_TX_PWR_TRACK |*/
			HAL_RF_IQK |
			/*HAL_RF_LCK |*/
			HAL_RF_DPK |
			HAL_RF_DACK |
			HAL_RF_TXGAPK |
			HAL_RF_DPK_TRACK |
			HAL_RF_RXDCK |
			/*HAL_RF_RXGAINK |*/
			HAL_RF_THER_TRIM |
			HAL_RF_PABIAS_TRIM |
			HAL_RF_TSSI_TRIM |
			/*HAL_RF_XTAL_TRACK |*/
			HAL_RF_TX_SHAPE |
			0;
		break;
#endif
#ifdef RF_8852C_SUPPORT
	case RF_RTL8852C:
		rf->support_ability =
			/*HAL_RF_TX_PWR_TRACK |*/
			/*HAL_RF_IQK |*/
			/*HAL_RF_LCK |*/
			/*HAL_RF_DPK |*/
			/*HAL_RF_DACK |*/
			/*HAL_RF_TXGAPK |*/
			/*HAL_RF_DPK_TRACK |*/
			0;
		break;
#endif
	default:
		rf->support_ability =
			/*HAL_RF_TX_PWR_TRACK |*/
			/*HAL_RF_IQK |*/
			/*HAL_RF_LCK |*/
			/*HAL_RF_DPK |*/
			/*HAL_RF_DACK |*/
			/*HAL_RF_TXGAPK |*/
			/*HAL_RF_DPK_TRACK |*/
			0;
		break;
	}
}

void halrf_rfability_init(struct rf_info *rf)
{

	switch (rf->ic_type) {
#ifdef RF_8852A_SUPPORT
	case RF_RTL8852A:
		rf->support_ability =
			HAL_RF_TX_PWR_TRACK |
			HAL_RF_TSSI_TRK |
			HAL_RF_IQK |
			/*HAL_RF_LCK |*/
			HAL_RF_DPK |
			HAL_RF_DACK |
			HAL_RF_TXGAPK |
			HAL_RF_DPK_TRACK |
			HAL_RF_RXDCK |
			HAL_RF_RXGAINK |
			HAL_RF_THER_TRIM |
			HAL_RF_PABIAS_TRIM |
			HAL_RF_TSSI_TRIM |
			HAL_RF_XTAL_TRACK |
			0;
		break;
#endif
#ifdef RF_8852B_SUPPORT
	case RF_RTL8852B:
		rf->support_ability |=
			HAL_RF_TX_PWR_TRACK |
			HAL_RF_IQK |
			/*HAL_RF_LCK |*/
			HAL_RF_DPK |
			HAL_RF_DACK |
			HAL_RF_TXGAPK |
			HAL_RF_DPK_TRACK |
			HAL_RF_RXDCK |
			/*HAL_RF_RXGAINK |*/
			HAL_RF_THER_TRIM |
			HAL_RF_PABIAS_TRIM |
			HAL_RF_TSSI_TRIM |
			/*HAL_RF_XTAL_TRACK |*/
			HAL_RF_TX_SHAPE |
			0;
		break;
#endif
#ifdef RF_8852C_SUPPORT
	case RF_RTL8852C:
		rf->support_ability |=
			/*HAL_RF_TX_PWR_TRACK |*/
			/*HAL_RF_IQK |*/
			/*HAL_RF_LCK |*/
			/*HAL_RF_DPK |*/
			/*HAL_RF_DACK |*/
			/*HAL_RF_TXGAPK |*/
			/*HAL_RF_DPK_TRACK |*/
			0;
		break;
#endif
	default:
		rf->support_ability =
			/*HAL_RF_TX_PWR_TRACK |*/
			/*HAL_RF_IQK |*/
			/*HAL_RF_LCK |*/
			/*HAL_RF_DPK |*/
			/*HAL_RF_DACK |*/
			/*HAL_RF_TXGAPK |*/
			/*HAL_RF_DPK_TRACK |*/
			0;
		break;
	}
}

void halrf_set_rfability(struct rf_info *rf)
{
	if (rf->manual_support_ability &&
	    rf->manual_support_ability != 0xffffffff)
		rf->support_ability = rf->manual_support_ability;
	else if (rf->phl_com->drv_mode == RTW_DRV_MODE_MP)
		halrf_rfability_init_mp(rf);
	else
		halrf_rfability_init(rf);

	PHL_INFO("[PHL]%x\n", rf->dbg_component);
	RF_DBG(rf, DBG_RF_INIT,
	       "IC = ((0x%x)), mp=%d,  RF_Supportability Init = ((0x%x))\n",
	       rf->ic_type, rf->phl_com->drv_mode, rf->support_ability);
}

void halrf_rfe_init(struct rf_info *rf)
{
	u8 rfe_type = rf->phl_com->dev_cap.rfe_type;

	switch (rf->ic_type) {
#ifdef RF_8852A_SUPPORT
	case RF_RTL8852A:
		/*2G FEM check*/
		if (rfe_type == 11 || rfe_type == 12 || rfe_type == 17 ||
		    rfe_type == 18 || rfe_type == 51 || rfe_type == 52 ||
		    rfe_type == 53 || rfe_type == 54) {
			rf->fem.epa_2g = 1;
			rf->fem.elna_2g = 1;
		}
		/*5G FEM check*/
		if (rfe_type == 9 || rfe_type == 10 || rfe_type == 11 ||
		    rfe_type == 12 || rfe_type == 15 || rfe_type == 16 ||
		    rfe_type == 17 || rfe_type == 18 || rfe_type == 37 || 
		    rfe_type == 38 || rfe_type == 51 || rfe_type == 52 ||
		    rfe_type == 53 || rfe_type == 54) {
			rf->fem.epa_5g = 1;
			rf->fem.elna_5g = 1;
		}
		/*6G FEM check*/
		if (rfe_type == 13 || rfe_type == 14 || rfe_type == 15 ||
		    rfe_type == 16 || rfe_type == 17 || rfe_type == 18 ||
		    rfe_type == 37 || rfe_type == 38 || rfe_type == 51 ||
		    rfe_type == 52 || rfe_type == 53 ||
		    rfe_type == 54) {
			rf->fem.epa_6g = 1;
			rf->fem.elna_6g = 1;
		}
		break;
#endif
	default:
		break;
	}
}

void halrf_rfe_type_gpio_setting(struct rf_info *rf)
{
	u32 band = rf->hal_com->band[HW_PHY_0].cur_chandef.band;

	RF_DBG(rf, DBG_RF_INIT, "======>%s\n", __func__);

	halrf_set_gpio(rf, HW_PHY_0, (u8)band);
}

enum rtw_hal_status halrf_dm_init(void *rf_void)
{
	struct rf_info *rf = (struct rf_info *)rf_void;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	if (!rf) {
		RF_DBG(rf, DBG_RF_INIT, "[%s] *rf = NULL", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}

	halrf_cmn_info_self_init(rf);
	halrf_dbg_setting_init(rf);
	halrf_cmd_parser_init(rf);
	halrf_set_rfability(rf);
	halrf_rfe_init(rf);
	halrf_rfe_type_gpio_setting(rf);
	halrf_config_nctl_reg(rf);
	halrf_rfk_self_init(rf);

	/*Set Power table ref power*/
	halrf_set_ref_power_to_struct(rf, HW_PHY_0);

	halrf_rck_trigger(rf, HW_PHY_0);
	//halrf_gapk_save_tx_gain_8852a(rf);
	halrf_dack_trigger(rf, false);
	halrf_rx_dck_trigger(rf, HW_PHY_0, true);

	/*RX Gain K Get efuse*/
	/*halrf_get_efuse_rx_gain_k(rf, HW_PHY_0);*/

	/*Thermal Trim, PA Bias k, TSSI Trim get efuse and set reg*/
	halrf_get_efuse_trim(rf, HW_PHY_0);

	/*TSSI Init*/
	halrf_tssi_get_efuse_ex(rf, HW_PHY_0);
	/*halrf_tssi_get_efuse_ex(rf, HW_PHY_1);*/

	/*Set MAC 0xd220[1]=0  r_txagc_BT_en=0 by Bryant*/
	if (rf->phl_com->drv_mode == RTW_DRV_MODE_MP)
		halrf_wl_tx_power_control(rf, 0xffffffff);

	halrf_fcs_init(rf);

	return hal_status;
}

enum rtw_hal_status halrf_init(struct rtw_phl_com_t *phl_com,
			struct rtw_hal_com_t *hal_com, void **rf_out)
{
	//enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct rf_info *rf = NULL;

	rf = hal_mem_alloc(hal_com, sizeof(struct rf_info));

	if (!rf)
		return RTW_HAL_STATUS_RF_INIT_FAILURE;

	*rf_out = rf;
//	PHL_INFO("[PHL] %s - halrf(%p), *rf(%p)\n", __func__, halrf, *rf);
//	PHL_INFO("[PHL] %s\n", __func__);

	rf->phl_com = phl_com;/*shared memory for all components*/
	rf->hal_com = hal_com;/*shared memory for phl and hal*/
	

	return RTW_HAL_STATUS_SUCCESS;
}

void halrf_deinit(struct rtw_phl_com_t *phl_com,
			struct rtw_hal_com_t *hal_com, void *rf)
{
	//enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct rf_info *halrf = (struct rf_info *)rf;

	/*stop FSM of RF or free memory*/
	PHL_INFO("[PHL] %s - halrf(%p)\n", __func__, halrf);

	if(halrf) {
		hal_mem_free(hal_com, halrf, sizeof(struct rf_info));
	}
}
