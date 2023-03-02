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
#include "phl_headers.h"

static void
_phl_sw_cap_para_init(
	struct rtw_phl_com_t* phl_com, struct rtw_para_info_t *para_info)
{
	para_info->para_src = RTW_PARA_SRC_INTNAL;
	para_info->para_data = NULL;
	para_info->para_data_len = 0;
	para_info->hal_phy_folder = NULL;
}

static void
_phl_sw_cap_para_free(
	struct rtw_phl_com_t* phl_com, struct rtw_para_info_t *para_info)
{
	u32 buf_sz = MAX_HWCONFIG_FILE_CONTENT;
	void *drv = phl_com->drv_priv;

	if(para_info->para_data)
		_os_mem_free(drv, para_info->para_data, buf_sz * sizeof(u32));

	para_info->para_data = NULL;
	para_info->para_data_len = 0;
}

static void
_phl_pwrlmt_para_init(
	struct rtw_phl_com_t* phl_com, struct rtw_para_pwrlmt_info_t *para_info)
{
	para_info->para_src = RTW_PARA_SRC_INTNAL;
	para_info->para_data = NULL;
	para_info->para_data_len = 0;
	para_info->ext_regd_arridx = 0;
	para_info->ext_reg_map_num = 0;
	para_info->hal_phy_folder = NULL;
}

static void
_phl_pwrlmt_para_free(
	struct rtw_phl_com_t* phl_com, struct rtw_para_pwrlmt_info_t *para_info)
{
	u32 file_buf_sz = MAX_HWCONFIG_FILE_CONTENT;
	u32 buf_sz = MAX_LINES_HWCONFIG_TXT;
	void *drv = phl_com->drv_priv;

	if(para_info->para_data)
		_os_mem_free(drv, para_info->para_data, file_buf_sz * sizeof(u32));
	para_info->para_data = NULL;
	para_info->para_data_len = 0;

	if(para_info->ext_reg_codemap)
		_os_mem_free(drv, para_info->ext_reg_codemap, buf_sz * sizeof(u8));
	para_info->ext_reg_codemap = NULL;
	para_info->ext_reg_map_num = 0;
}

enum channel_width _phl_sw_cap_get_hi_bw(struct phy_cap_t *phy_cap)
{
	enum channel_width bw = CHANNEL_WIDTH_20;
	do {
		if (phy_cap->bw_sup & BW_CAP_80_80M) {
			bw = CHANNEL_WIDTH_80_80;
			break;
		} else if (phy_cap->bw_sup & BW_CAP_160M) {
			bw = CHANNEL_WIDTH_160;
			break;
		} else if (phy_cap->bw_sup & BW_CAP_80M) {
			bw = CHANNEL_WIDTH_80;
			break;
		} else if (phy_cap->bw_sup & BW_CAP_40M) {
			bw = CHANNEL_WIDTH_40;
			break;
		} else if (phy_cap->bw_sup & BW_CAP_20M) {
			bw = CHANNEL_WIDTH_20;
			break;
		}
	} while (0);

	return bw;
}

enum rtw_phl_status
phl_sw_cap_init(struct rtw_phl_com_t* phl_com)
{
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	struct phy_sw_cap_t *phy_sw_cap = NULL;
	u8	idx=0;

	for(idx=0; idx < 2 ; idx++)
	{
		phy_sw_cap = &phl_com->phy_sw_cap[idx];

		_phl_sw_cap_para_init(phl_com, &phy_sw_cap->mac_reg_info);
		_phl_sw_cap_para_init(phl_com, &phy_sw_cap->bb_phy_reg_info);
		_phl_sw_cap_para_init(phl_com, &phy_sw_cap->bb_phy_reg_mp_info);
		_phl_sw_cap_para_init(phl_com, &phy_sw_cap->bb_phy_reg_gain_info);
		_phl_sw_cap_para_init(phl_com, &phy_sw_cap->rf_radio_a_info);
		_phl_sw_cap_para_init(phl_com, &phy_sw_cap->rf_radio_b_info);
		_phl_sw_cap_para_init(phl_com, &phy_sw_cap->rf_txpwr_byrate_info);
		_phl_sw_cap_para_init(phl_com, &phy_sw_cap->rf_txpwrtrack_info);

		_phl_pwrlmt_para_init(phl_com, &phy_sw_cap->rf_txpwrlmt_info);
		_phl_pwrlmt_para_init(phl_com, &phy_sw_cap->rf_txpwrlmt_ru_info);
		phy_sw_cap->bfreed_para = false;
	}
	phl_com->dev_sw_cap.bfree_para_info = false; /* Default keep Phy file param info*/
#endif
	phl_com->dev_sw_cap.fw_cap.fw_src = RTW_FW_SRC_INTNAL;
	phl_com->dev_sw_cap.btc_mode = BTC_MODE_NORMAL;
	phl_com->dev_sw_cap.bypass_rfe_chk = false;
	phl_com->dev_sw_cap.rf_board_opt = PHL_UNDEFINED_SW_CAP;

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_sw_cap_deinit(struct rtw_phl_com_t* phl_com)
{
#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	struct phy_sw_cap_t *phy_sw_cap = NULL;
	u8	idx=0;

	for (idx = 0; idx < 2; idx++) {
		phy_sw_cap = &phl_com->phy_sw_cap[idx];
		if (phy_sw_cap->bfreed_para == true) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "already bfreed para_info->para_data\n");
			return RTW_PHL_STATUS_SUCCESS;
		}
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "To free para_info->para_data phy %d\n", idx);

		_phl_sw_cap_para_free(phl_com, &phy_sw_cap->mac_reg_info);
		_phl_sw_cap_para_free(phl_com, &phy_sw_cap->bb_phy_reg_info);
		_phl_sw_cap_para_free(phl_com, &phy_sw_cap->bb_phy_reg_mp_info);
		_phl_sw_cap_para_free(phl_com, &phy_sw_cap->bb_phy_reg_gain_info);

		_phl_sw_cap_para_free(phl_com, &phy_sw_cap->rf_radio_a_info);
		_phl_sw_cap_para_free(phl_com, &phy_sw_cap->rf_radio_b_info);
		_phl_sw_cap_para_free(phl_com, &phy_sw_cap->rf_txpwr_byrate_info);
		_phl_sw_cap_para_free(phl_com, &phy_sw_cap->rf_txpwrtrack_info);

		_phl_pwrlmt_para_free(phl_com, &phy_sw_cap->rf_txpwrlmt_info);
		_phl_pwrlmt_para_free(phl_com, &phy_sw_cap->rf_txpwrlmt_ru_info);

		phy_sw_cap->bfreed_para = true;
	}
#endif

	return RTW_PHL_STATUS_SUCCESS;
}

void rtw_phl_init_free_para_buf(struct rtw_phl_com_t *phl_com)
{

#ifdef CONFIG_LOAD_PHY_PARA_FROM_FILE
	if (phl_com->dev_sw_cap.bfree_para_info == true)
		phl_sw_cap_deinit(phl_com);

#endif
}


u16 _phl_sw_role_cap_bf(enum role_type rtype)
{
	u16 def_bf_cap = 0;

	if (PHL_RTYPE_AP == rtype) {
		/* AP mode : no MU BFee */
		def_bf_cap = (HW_CAP_BFEE_HT_SU | HW_CAP_BFER_HT_SU |
			      HW_CAP_BFEE_VHT_SU | HW_CAP_BFER_VHT_SU |
			      HW_CAP_BFER_VHT_MU |
			      HW_CAP_BFEE_HE_SU | HW_CAP_BFER_HE_SU |
			      HW_CAP_BFER_HE_MU |
			      HW_CAP_HE_NON_TB_CQI | HW_CAP_HE_TB_CQI);
	} else if (PHL_RTYPE_STATION == rtype) {
		/* STA mode : no MU BFer */
		def_bf_cap = (HW_CAP_BFEE_HT_SU | HW_CAP_BFER_HT_SU |
			      HW_CAP_BFEE_VHT_SU | HW_CAP_BFER_VHT_SU |
			      HW_CAP_BFEE_VHT_MU |
			      HW_CAP_BFEE_HE_SU | HW_CAP_BFER_HE_SU |
			      HW_CAP_BFEE_HE_MU |
			      HW_CAP_HE_NON_TB_CQI | HW_CAP_HE_TB_CQI);
	} else {
		def_bf_cap = (HW_CAP_BFEE_HT_SU | HW_CAP_BFER_HT_SU |
			      HW_CAP_BFEE_VHT_SU | HW_CAP_BFER_VHT_SU |
			      HW_CAP_BFEE_VHT_MU | HW_CAP_BFER_VHT_MU |
			      HW_CAP_BFEE_HE_SU | HW_CAP_BFER_HE_SU |
			      HW_CAP_BFEE_HE_MU | HW_CAP_BFER_HE_MU |
			      HW_CAP_HE_NON_TB_CQI | HW_CAP_HE_TB_CQI);
	}

	return def_bf_cap;
}

static void _phl_init_proto_bf_cap(struct phl_info_t *phl_info,
		u8 hw_band, enum role_type rtype, struct protocol_cap_t *role_cap)
{
#ifdef RTW_WKARD_PHY_CAP
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct role_sw_cap_t *sw_role_cap = &phl_com->role_sw_cap;
	struct protocol_cap_t proto_cap = {0};
	u16 bfcap = sw_role_cap->bf_cap;

	/* First : compare and get the bf sw_proto_cap and hw_proto_cap .*/
	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_get_bf_proto_cap(
						phl_com,
						phl_info->hal,
			 			hw_band,
						&proto_cap)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_,
			  "%s : Get SW/HW BF Cap FAIL, disable all of the BF functions.\n", __func__);
	}

	/* Second : filter bf cap with 802.11 spec */
	bfcap &= _phl_sw_role_cap_bf(rtype);

	/* Final : Compare with sw_role_cap->bf_cap to judge the final wrole's BF CAP. */
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s : sw_role_cap->bf_cap = 0x%x \n",
		  __func__, sw_role_cap->bf_cap);
	if (!(bfcap & HW_CAP_BFEE_HT_SU) &&
	    (proto_cap.ht_su_bfme)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HT SU BFEE by sw_role_cap.\n");
		role_cap->ht_su_bfme = 0;
	} else {
		role_cap->ht_su_bfme = proto_cap.ht_su_bfme;
	}

	if (!(bfcap & HW_CAP_BFER_HT_SU) &&
	    (proto_cap.ht_su_bfmr)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HT SU BFER by sw_role_cap.\n");
		role_cap->ht_su_bfmr = 0;
	} else {
		role_cap->ht_su_bfmr = proto_cap.ht_su_bfmr;
	}

	if (!(bfcap & HW_CAP_BFEE_VHT_SU) &&
	    (proto_cap.vht_su_bfme)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable VHT SU BFEE by sw_role_cap.\n");
		role_cap->vht_su_bfme = 0;
	} else {
		role_cap->vht_su_bfme = proto_cap.vht_su_bfme;
	}

	if (!(bfcap & HW_CAP_BFER_VHT_SU) &&
	    (proto_cap.vht_su_bfmr)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable VHT SU BFER by sw_role_cap.\n");
		role_cap->vht_su_bfmr = 0;
	} else {
		role_cap->vht_su_bfmr = proto_cap.vht_su_bfmr;
	}

	if (!(bfcap & HW_CAP_BFEE_VHT_MU) &&
	    (proto_cap.vht_mu_bfme)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable VHT MU BFEE by sw_role_cap.\n");
		role_cap->vht_mu_bfme = 0;
	} else {
		role_cap->vht_mu_bfme = proto_cap.vht_mu_bfme;
	}

	if (!(bfcap & HW_CAP_BFER_VHT_MU) &&
	    (proto_cap.vht_mu_bfmr)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable VHT MU BFER by sw_role_cap.\n");
		role_cap->vht_mu_bfmr = 0;
	} else {
		role_cap->vht_mu_bfmr = proto_cap.vht_mu_bfmr;
	}

	if (!(bfcap & HW_CAP_BFEE_HE_SU) &&
	    (proto_cap.he_su_bfme)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE SU BFEE by sw_role_cap.\n");
		role_cap->he_su_bfme = 0;
	} else {
		role_cap->he_su_bfme = proto_cap.he_su_bfme;
	}

	if (!(bfcap & HW_CAP_BFER_HE_SU) &&
	    (proto_cap.he_su_bfmr)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE SU BFER by sw_role_cap.\n");
		role_cap->he_su_bfmr = 0;
	} else {
		role_cap->he_su_bfmr = proto_cap.he_su_bfmr;
	}

	if (!(bfcap & HW_CAP_BFEE_HE_MU) &&
	    (proto_cap.he_mu_bfme)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE MU BFEE by sw_role_cap.\n");
		role_cap->he_mu_bfme = 0;
	} else {
		role_cap->he_mu_bfme = proto_cap.he_mu_bfme;
	}

	if (!(bfcap & HW_CAP_BFER_HE_MU) &&
	    (proto_cap.he_mu_bfmr)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE MU BFER by sw_role_cap.\n");
		role_cap->he_mu_bfmr = 0;
	} else {
		role_cap->he_mu_bfmr = proto_cap.he_mu_bfmr;
	}

	if (!(bfcap & HW_CAP_HE_NON_TB_CQI) &&
	    (proto_cap.non_trig_cqi_fb)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE NON-TB CQI_FB by sw_role_cap.\n");
		role_cap->non_trig_cqi_fb = 0;
	} else {
		role_cap->non_trig_cqi_fb = proto_cap.non_trig_cqi_fb;
	}

	if (!(bfcap & HW_CAP_HE_TB_CQI) &&
	    (proto_cap.trig_cqi_fb)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE TB CQI_FB by sw_role_cap.\n");
		role_cap->trig_cqi_fb = 0;
	} else {
		role_cap->trig_cqi_fb = proto_cap.trig_cqi_fb;
	}
#endif

}

static void _phl_external_cap_limit(struct phl_info_t *phl_info,
	struct protocol_cap_t *proto_role_cap)
{
#ifdef RTW_WKARD_BTC_STBC_CAP
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);

	if ((proto_role_cap->cap_option & EXT_CAP_LIMIT_2G_RX_STBC) &&
		hal_com->btc_ctrl.disable_rx_stbc) {
		proto_role_cap->stbc_he_rx = 0;
		proto_role_cap->stbc_vht_rx = 0;
		proto_role_cap->stbc_ht_rx = 0;
		PHL_INFO("%s Disable STBC RX cap for BTC request\n", __func__);
	}
#endif
}

static void _phl_init_proto_stbc_cap(struct phl_info_t *phl_info,
		u8 hw_band, struct protocol_cap_t *proto_role_cap)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct role_sw_cap_t *sw_role_cap = &phl_com->role_sw_cap;
	struct protocol_cap_t proto_cap = {0};

	/* First : compare and get the stbc sw_proto_cap and hw_proto_cap .*/
	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_get_stbc_proto_cap(phl_com,
								 phl_info->hal,
			 					 hw_band,
								 &proto_cap)) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_,
			  "%s : Get SW/HW STBC proto_cap FAIL, disable all of the STBC functions.\n", __func__);
	}

	/* Final : Compare with sw_role_cap->stbc_cap to judge the final wrole's STBC CAP. */
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s : sw_role_cap->stbc_cap = 0x%x \n",
		__func__, sw_role_cap->stbc_cap);

#ifdef RTW_WKARD_PHY_CAP

	proto_role_cap->stbc_tx = 0; /* Removed later */

	/* Check sw role cap, if it is not support, set proto_role_cap->xxx to 0 */
	if (!(sw_role_cap->stbc_cap & HW_CAP_STBC_HT_TX) &&
	    (proto_cap.stbc_ht_tx)) {
		proto_role_cap->stbc_ht_tx = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HT STBC Tx by sw_role_cap.\n");
	} else {
		proto_role_cap->stbc_ht_tx = proto_cap.stbc_ht_tx;
	}

	if (!(sw_role_cap->stbc_cap & HW_CAP_STBC_VHT_TX) &&
	    (proto_cap.stbc_vht_tx)) {
		proto_role_cap->stbc_vht_tx = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable VHT STBC Tx by sw_role_cap.\n");
	} else {
		proto_role_cap->stbc_vht_tx = proto_cap.stbc_vht_tx;
	}

	if (!(sw_role_cap->stbc_cap & HW_CAP_STBC_HE_TX) &&
	    (proto_cap.stbc_he_tx)) {
		proto_role_cap->stbc_he_tx = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE STBC Tx by sw_role_cap.\n");
	} else {
		proto_role_cap->stbc_he_tx = proto_cap.stbc_he_tx;
	}

	if (!(sw_role_cap->stbc_cap & HW_CAP_STBC_HE_TX_GT_80M) &&
	    (proto_cap.stbc_tx_greater_80mhz)) {
		proto_role_cap->stbc_tx_greater_80mhz = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable STBC Tx (greater than 80M) by sw_role_cap.\n");
	} else {
		proto_role_cap->stbc_tx_greater_80mhz = proto_cap.stbc_tx_greater_80mhz;
	}

	if (!(sw_role_cap->stbc_cap & HW_CAP_STBC_HT_RX) &&
	    (proto_cap.stbc_ht_rx)) {
		proto_role_cap->stbc_ht_rx = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HT STBC Rx by sw_role_cap.\n");
	} else {
		proto_role_cap->stbc_ht_rx = proto_cap.stbc_ht_rx;
	}

	if (!(sw_role_cap->stbc_cap & HW_CAP_STBC_VHT_RX) &&
	    (proto_cap.stbc_vht_rx)) {
		proto_role_cap->stbc_vht_rx = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable VHT STBC Rx by sw_role_cap.\n");
	} else {
		proto_role_cap->stbc_vht_rx = proto_cap.stbc_vht_rx;
	}

	if (!(sw_role_cap->stbc_cap & HW_CAP_STBC_HE_RX) &&
	    (proto_cap.stbc_he_rx)) {
		proto_role_cap->stbc_he_rx = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE STBC Rx by sw_role_cap.\n");
	} else {
		proto_role_cap->stbc_he_rx = proto_cap.stbc_he_rx;
	}

	if (!(sw_role_cap->stbc_cap & HW_CAP_STBC_HE_RX_GT_80M) &&
	    (proto_cap.stbc_rx_greater_80mhz)) {
		proto_role_cap->stbc_rx_greater_80mhz = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "Disable HE STBC Rx (greater than 80M) by sw_role_cap.\n");
	} else {
		proto_role_cap->stbc_rx_greater_80mhz = proto_cap.stbc_rx_greater_80mhz;
	}
#endif

	_phl_external_cap_limit(phl_info, proto_role_cap);
}

static enum rtw_phl_status
_phl_init_protocol_cap(struct phl_info_t *phl_info,
				u8 hw_band, enum role_type rtype,
				struct protocol_cap_t *proto_role_cap)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;

	/* TODO: Get protocol cap from sw and hw cap*/
	if (rtype == PHL_RTYPE_AP) {
		proto_role_cap->num_ampdu = 128;
		proto_role_cap->ampdu_density = 0;
		proto_role_cap->ampdu_len_exp = 0xff;
		proto_role_cap->amsdu_in_ampdu = 1;
		proto_role_cap->max_amsdu_len =
			phl_com->proto_sw_cap[hw_band].max_amsdu_len;
		proto_role_cap->htc_rx = 1;
		proto_role_cap->sm_ps = 0;
		proto_role_cap->trig_padding = 0;
#ifdef CONFIG_PHL_TWT
		proto_role_cap->twt =
				phl_com->dev_cap.twt_sup & RTW_PHL_TWT_RSP_SUP;
#else
		proto_role_cap->twt = 0;
#endif /* CONFIG_PHL_TWT */
		proto_role_cap->all_ack = 1;
		proto_role_cap->a_ctrl = 0xe;
		proto_role_cap->ops = 1;
		proto_role_cap->ht_vht_trig_rx = 0;
		proto_role_cap->bsscolor = 0x0E; /* Default BSS Color */
		proto_role_cap->edca[RTW_AC_BE].ac = RTW_AC_BE;
		proto_role_cap->edca[RTW_AC_BE].param = 0xA42B;
		proto_role_cap->edca[RTW_AC_BK].ac = RTW_AC_BK;
		proto_role_cap->edca[RTW_AC_BK].param = 0xA549;
		proto_role_cap->edca[RTW_AC_VI].ac = RTW_AC_VI;
		proto_role_cap->edca[RTW_AC_VI].param = 0x5E4326;
		proto_role_cap->edca[RTW_AC_VO].ac = RTW_AC_VO;
		proto_role_cap->edca[RTW_AC_VO].param = 0x2F3224;
		proto_role_cap->ht_ldpc = 1;
		proto_role_cap->vht_ldpc = 1;
		proto_role_cap->he_ldpc = 1;
		proto_role_cap->sgi_20 = 1;
		proto_role_cap->sgi_40 = 1;
		proto_role_cap->sgi_80 = 1;
		proto_role_cap->sgi_160 = 0;
		switch (phl_com->phy_cap[hw_band].rxss) {
			default:
				break;
			case 1:
				proto_role_cap->ht_rx_mcs[0] = 0xff;
				proto_role_cap->vht_rx_mcs[0] = 0xfe;
				proto_role_cap->vht_rx_mcs[1] = 0xff;
				proto_role_cap->he_rx_mcs[0] = 0xfe;
				proto_role_cap->he_rx_mcs[1] = 0xff;
				break;
			case 2:
				proto_role_cap->ht_rx_mcs[0] = 0xff;
				proto_role_cap->ht_rx_mcs[1] = 0xff;
				proto_role_cap->vht_rx_mcs[0] = 0xfa;
				proto_role_cap->vht_rx_mcs[1] = 0xff;
				proto_role_cap->he_rx_mcs[0] = 0xfa;
				proto_role_cap->he_rx_mcs[1] = 0xff;
				break;
		}
		switch (phl_com->phy_cap[hw_band].txss) {
			default:
				break;
			case 1:
				proto_role_cap->ht_tx_mcs[0] = 0xff;
				proto_role_cap->vht_tx_mcs[0] = 0xfe;
				proto_role_cap->vht_tx_mcs[1] = 0xff;
				proto_role_cap->he_tx_mcs[0] = 0xfe;
				proto_role_cap->he_tx_mcs[1] = 0xff;
				break;
			case 2:
				proto_role_cap->ht_tx_mcs[0] = 0xff;
				proto_role_cap->ht_tx_mcs[1] = 0xff;
				proto_role_cap->vht_tx_mcs[0] = 0xfa;
				proto_role_cap->vht_tx_mcs[1] = 0xff;
				proto_role_cap->he_tx_mcs[0] = 0xfa;
				proto_role_cap->he_tx_mcs[1] = 0xff;
				break;
		}

		proto_role_cap->ltf_gi = 0x3f;	// bit-x
		proto_role_cap->doppler_tx = 1;
		proto_role_cap->doppler_rx = 0;
		proto_role_cap->dcm_max_const_tx = 0;
		proto_role_cap->dcm_max_nss_tx = 0;
		proto_role_cap->dcm_max_const_rx = 3;
		proto_role_cap->dcm_max_nss_rx = 0;
		proto_role_cap->partial_bw_su_in_mu = 1;
		_phl_init_proto_stbc_cap(phl_info, hw_band, proto_role_cap);
		_phl_init_proto_bf_cap(phl_info, hw_band, rtype, proto_role_cap);

		/* All of the HT/VHT/HE BFee */
		if ((1 == proto_role_cap->ht_su_bfme) ||
		    (1 == proto_role_cap->vht_su_bfme) ||
		    (1 == proto_role_cap->vht_mu_bfme) ||
		    (1 == proto_role_cap->he_su_bfme) ||
		    (1 == proto_role_cap->he_mu_bfme) ||
		    (1 == proto_role_cap->non_trig_cqi_fb)||
		    (1 == proto_role_cap->trig_cqi_fb)) {
			proto_role_cap->bfme_sts = 3;
			proto_role_cap->bfme_sts_greater_80mhz = 0;
			proto_role_cap->max_nc = 1;
		} else {
			proto_role_cap->bfme_sts = 0;
			proto_role_cap->bfme_sts_greater_80mhz = 0;
			proto_role_cap->max_nc = 0;
		}
		/* HE BFer */
		if ((1 == proto_role_cap->he_su_bfmr) ||
		    (1 == proto_role_cap->he_mu_bfmr)) {
			proto_role_cap->num_snd_dim = 1;
			proto_role_cap->num_snd_dim_greater_80mhz = 0;
		} else {
			proto_role_cap->num_snd_dim = 0;
			proto_role_cap->num_snd_dim_greater_80mhz = 0;
		}
		/* HE BFee */
		if ((1 == proto_role_cap->he_su_bfme) ||
		    (1 == proto_role_cap->he_mu_bfme)) {
			proto_role_cap->ng_16_su_fb = 1;
			proto_role_cap->ng_16_mu_fb = 1;
			proto_role_cap->cb_sz_su_fb = 1;
			proto_role_cap->cb_sz_mu_fb = 1;
			proto_role_cap->he_rx_ndp_4x32 = 1;
		} else {
			proto_role_cap->ng_16_su_fb = 0;
			proto_role_cap->ng_16_mu_fb = 0;
			proto_role_cap->cb_sz_su_fb = 0;
			proto_role_cap->cb_sz_mu_fb = 0;
			proto_role_cap->he_rx_ndp_4x32 = 0;
		}

		/*HE SU BFer or BFer*/
		if ((1 == proto_role_cap->he_su_bfme) ||
		    (1 == proto_role_cap->he_su_bfmr)) {
			proto_role_cap->trig_su_bfm_fb = 1;
		} else {
			proto_role_cap->trig_su_bfm_fb = 0;
		}
		/*HE MU BFer or BFer*/
		if ((1 == proto_role_cap->he_mu_bfme) ||
		    (1 == proto_role_cap->he_mu_bfmr)) {
			proto_role_cap->trig_mu_bfm_fb = 1;
		} else {
			proto_role_cap->trig_mu_bfm_fb = 0;
		}
		/* HT/VHT BFee */
		if ((1 == proto_role_cap->vht_mu_bfme) ||
		    (1 == proto_role_cap->vht_su_bfme) ||
		    (1 == proto_role_cap->ht_su_bfme)) {
			proto_role_cap->ht_vht_ng = 0; /* vht ng = 1 */
			proto_role_cap->ht_vht_cb = 1; /* vht_mu{9,7}/vht_su{6,4}/ht{4,2} */
		}

		proto_role_cap->partial_bw_su_er = 1;
		proto_role_cap->pkt_padding = 2;
		proto_role_cap->pwr_bst_factor = 1;
		proto_role_cap->dcm_max_ru = 2;
		proto_role_cap->long_sigb_symbol = 1;
		proto_role_cap->tx_1024q_ru = 0;
		proto_role_cap->rx_1024q_ru = 1;
		proto_role_cap->fbw_su_using_mu_cmprs_sigb = 1;
		proto_role_cap->fbw_su_using_mu_non_cmprs_sigb = 1;
		proto_role_cap->nss_tx =
			phl_com->phy_cap[hw_band].txss;
		proto_role_cap->nss_rx =
			phl_com->phy_cap[hw_band].rxss;
	} else if (rtype == PHL_RTYPE_STATION) {
		proto_role_cap->num_ampdu = 128;
		proto_role_cap->ampdu_density = 0;
		proto_role_cap->ampdu_len_exp = 0xff;
		proto_role_cap->amsdu_in_ampdu = 1;
		proto_role_cap->max_amsdu_len =
			phl_com->proto_sw_cap[hw_band].max_amsdu_len;
		proto_role_cap->htc_rx = 1;
		proto_role_cap->sm_ps = 3;
		proto_role_cap->trig_padding = 2;
#ifdef CONFIG_PHL_TWT
		proto_role_cap->twt =
				phl_com->dev_cap.twt_sup & RTW_PHL_TWT_REQ_SUP;
#else
		proto_role_cap->twt = 0;
#endif /* CONFIG_PHL_TWT */
		proto_role_cap->all_ack = 1;
		proto_role_cap->a_ctrl = 0x6;
		proto_role_cap->ops = 1;
		proto_role_cap->ht_vht_trig_rx = 1;
		proto_role_cap->edca[RTW_AC_BE].ac = RTW_AC_BE;
		proto_role_cap->edca[RTW_AC_BE].param = 0xA42B;
		proto_role_cap->edca[RTW_AC_BK].ac = RTW_AC_BK;
		proto_role_cap->edca[RTW_AC_BK].param = 0xA549;
		proto_role_cap->edca[RTW_AC_VI].ac = RTW_AC_VI;
		proto_role_cap->edca[RTW_AC_VI].param = 0x5E4326;
		proto_role_cap->edca[RTW_AC_VO].ac = RTW_AC_VO;
		proto_role_cap->edca[RTW_AC_VO].param = 0x2F3224;
		proto_role_cap->ht_ldpc = 1;
		proto_role_cap->vht_ldpc = 1;
		proto_role_cap->he_ldpc = 1;
		proto_role_cap->sgi_20 = 1;
		proto_role_cap->sgi_40 = 1;
		proto_role_cap->sgi_80 = 1;
		proto_role_cap->sgi_160 = 0;

		switch (phl_com->phy_cap[hw_band].rxss) {
			default:
				break;
			case 1:
				proto_role_cap->ht_rx_mcs[0] = 0xff;
				proto_role_cap->vht_rx_mcs[0] = 0xfe;
				proto_role_cap->vht_rx_mcs[1] = 0xff;
				proto_role_cap->he_rx_mcs[0] = 0xfe;
				proto_role_cap->he_rx_mcs[1] = 0xff;
				break;
			case 2:
				proto_role_cap->ht_rx_mcs[0] = 0xff;
				proto_role_cap->ht_rx_mcs[1] = 0xff;
				proto_role_cap->vht_rx_mcs[0] = 0xfa;
				proto_role_cap->vht_rx_mcs[1] = 0xff;
				proto_role_cap->he_rx_mcs[0] = 0xfa;
				proto_role_cap->he_rx_mcs[1] = 0xff;
				break;
		}
		switch (phl_com->phy_cap[hw_band].txss) {
			default:
				break;
			case 1:
				proto_role_cap->ht_tx_mcs[0] = 0xff;
				proto_role_cap->vht_tx_mcs[0] = 0xfe;
				proto_role_cap->vht_tx_mcs[1] = 0xff;
				proto_role_cap->he_tx_mcs[0] = 0xfe;
				proto_role_cap->he_tx_mcs[1] = 0xff;
				break;
			case 2:
				proto_role_cap->ht_tx_mcs[0] = 0xff;
				proto_role_cap->ht_tx_mcs[1] = 0xff;
				proto_role_cap->vht_tx_mcs[0] = 0xfa;
				proto_role_cap->vht_tx_mcs[1] = 0xff;
				proto_role_cap->he_tx_mcs[0] = 0xfa;
				proto_role_cap->he_tx_mcs[1] = 0xff;
				break;
		}

		proto_role_cap->ltf_gi = 0x3f;	// bit-x
		proto_role_cap->doppler_tx = 1;
		proto_role_cap->doppler_rx = 0;
		proto_role_cap->dcm_max_const_tx = 3;
		proto_role_cap->dcm_max_nss_tx = 1;
		proto_role_cap->dcm_max_const_rx = 3;
		proto_role_cap->dcm_max_nss_rx = 0;

		_phl_init_proto_stbc_cap(phl_info, hw_band, proto_role_cap);
		_phl_init_proto_bf_cap(phl_info, hw_band, rtype, proto_role_cap);

		/* All of the HT/VHT/HE BFee */
		if ((1 == proto_role_cap->ht_su_bfme) ||
		    (1 == proto_role_cap->vht_su_bfme) ||
		    (1 == proto_role_cap->vht_mu_bfme) ||
		    (1 == proto_role_cap->he_su_bfme) ||
		    (1 == proto_role_cap->he_mu_bfme) ||
		    (1 == proto_role_cap->non_trig_cqi_fb) ||
		    (1 == proto_role_cap->trig_cqi_fb)) {
			proto_role_cap->bfme_sts = 3;
			proto_role_cap->bfme_sts_greater_80mhz = 0;
			proto_role_cap->max_nc = 1;
		} else {
			proto_role_cap->bfme_sts = 0;
			proto_role_cap->bfme_sts_greater_80mhz = 0;
			proto_role_cap->max_nc = 0;
		}

		/* HE BFer */
		if ((1 == proto_role_cap->he_su_bfmr) ||
		    (1 == proto_role_cap->he_mu_bfmr)) {
			proto_role_cap->num_snd_dim = 1;
			proto_role_cap->num_snd_dim_greater_80mhz = 0;
		} else {
			proto_role_cap->num_snd_dim = 0;
			proto_role_cap->num_snd_dim_greater_80mhz = 0;
		}
		/* HE BFee */
		if ((1 == proto_role_cap->he_su_bfme) ||
		    (1 == proto_role_cap->he_mu_bfme)) {
#ifdef RTW_WKARD_BFEE_DISABLE_NG16
			proto_role_cap->ng_16_su_fb = 0;
			proto_role_cap->ng_16_mu_fb = 0;
#else
			proto_role_cap->ng_16_su_fb = 1;
			proto_role_cap->ng_16_mu_fb = 1;
#endif
			proto_role_cap->cb_sz_su_fb = 1;
			proto_role_cap->cb_sz_mu_fb = 1;
			proto_role_cap->he_rx_ndp_4x32 = 1;
		} else {
			proto_role_cap->ng_16_su_fb = 0;
			proto_role_cap->ng_16_mu_fb = 0;
			proto_role_cap->cb_sz_su_fb = 0;
			proto_role_cap->cb_sz_mu_fb = 0;
			proto_role_cap->he_rx_ndp_4x32 = 0;
		}
		/*HE SU BFer or BFer*/
		if ((1 == proto_role_cap->he_su_bfme) ||
		    (1 == proto_role_cap->he_su_bfmr)) {
			proto_role_cap->trig_su_bfm_fb = 1;
		} else {
			proto_role_cap->trig_su_bfm_fb = 0;
		}
		/*HE MU BFer or BFer*/
		if ((1 == proto_role_cap->he_mu_bfme) ||
		    (1 == proto_role_cap->he_mu_bfmr)) {
			proto_role_cap->trig_mu_bfm_fb = 1;
		} else {
			proto_role_cap->trig_mu_bfm_fb = 0;
		}
		/* HT/VHT BFee */
		if ((1 == proto_role_cap->vht_mu_bfme) ||
		    (1 == proto_role_cap->vht_su_bfme) ||
		    (1 == proto_role_cap->ht_su_bfme)) {
			proto_role_cap->ht_vht_ng = 0; /* vht ng = 1 */
			proto_role_cap->ht_vht_cb = 1; /* vht_mu{9,7}/vht_su{6,4}/ht{4,2} */
		}
		proto_role_cap->partial_bw_su_in_mu = 0;
		proto_role_cap->partial_bw_su_er = 1;
		proto_role_cap->pkt_padding = 2;
		proto_role_cap->pwr_bst_factor = 1;
		proto_role_cap->dcm_max_ru = 2;
		proto_role_cap->long_sigb_symbol = 1;
		proto_role_cap->tx_1024q_ru = 1;
		proto_role_cap->rx_1024q_ru = 1;
		proto_role_cap->fbw_su_using_mu_cmprs_sigb = 1;
		proto_role_cap->fbw_su_using_mu_non_cmprs_sigb = 1;
		proto_role_cap->nss_tx =
			phl_com->phy_cap[hw_band].txss;
		proto_role_cap->nss_rx =
			phl_com->phy_cap[hw_band].rxss;
	}
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_init_protocol_cap(struct phl_info_t *phl_info,
			    struct rtw_wifi_role_t *wifi_role)
{

	enum rtw_phl_status ret = RTW_PHL_STATUS_SUCCESS;
	struct protocol_cap_t *role_proto_cap = &wifi_role->proto_role_cap;

	_os_mem_set(phl_to_drvpriv(phl_info),
		role_proto_cap, 0, sizeof(struct protocol_cap_t));

	ret = _phl_init_protocol_cap(phl_info, wifi_role->hw_band, wifi_role->type,
		role_proto_cap);

	if (ret == RTW_PHL_STATUS_FAILURE)
		PHL_ERR("wrole:%d - %s failed\n", wifi_role->id, __func__);

	return ret;
}

static enum rtw_phl_status
_phl_init_role_cap(struct phl_info_t *phl_info,
			u8 hw_band, struct role_cap_t *role_cap)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;

#ifdef RTW_WKARD_PHY_CAP
	role_cap->wmode = phl_com->phy_cap[hw_band].proto_sup;
	role_cap->bw = _phl_sw_cap_get_hi_bw(&phl_com->phy_cap[hw_band]);
	role_cap->rty_lmt = 0xFF; /* default follow CR */
	role_cap->rty_lmt_rts = 0xFF; /* default follow CR */

	role_cap->tx_htc = 1;
	role_cap->tx_sgi = 1;
	role_cap->tx_ht_ldpc = 1;
	role_cap->tx_vht_ldpc = 1;
	role_cap->tx_he_ldpc = 1;
	role_cap->tx_ht_stbc = 1;
	role_cap->tx_vht_stbc = 1;
	role_cap->tx_he_stbc = 1;
#endif
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_init_role_cap(struct phl_info_t *phl_info,
		  struct rtw_wifi_role_t *wifi_role)
{
	struct role_cap_t *role_cap = &wifi_role->cap;
	enum rtw_phl_status ret = RTW_PHL_STATUS_SUCCESS;

	_os_mem_set(phl_to_drvpriv(phl_info),
		role_cap, 0, sizeof(struct role_cap_t));


	ret = _phl_init_role_cap(phl_info, wifi_role->hw_band, role_cap);

	ret = phl_custom_init_role_cap(phl_info, wifi_role->hw_band, role_cap);

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
rtw_phl_get_dft_proto_cap(void *phl, u8 hw_band, enum role_type rtype,
				struct protocol_cap_t *role_proto_cap)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	_os_mem_set(phl_to_drvpriv(phl_info),
		role_proto_cap, 0, sizeof(struct protocol_cap_t));

	return _phl_init_protocol_cap(phl_info, hw_band, rtype,
		role_proto_cap);
}

enum rtw_phl_status
rtw_phl_get_dft_cap(void *phl, u8 hw_band, struct role_cap_t *role_cap)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;

	_os_mem_set(phl_to_drvpriv(phl_info),
		role_cap, 0, sizeof(struct role_cap_t));

	return _phl_init_role_cap(phl_info, hw_band, role_cap);
}


void rtw_phl_final_cap_decision(void * phl)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;

#ifdef CONFIG_PHL_DFS
	phl_com->dfs_info.region_domain = DFS_REGD_ETSI;
#endif

	rtw_hal_final_cap_decision(phl_com, phl_info->hal);
}

void phl_init_proto_stbc_cap(struct rtw_wifi_role_t *role,
		struct phl_info_t *phl_info,
		struct protocol_cap_t *proto_role_cap)
{
	if (role->chandef.band == BAND_ON_24G)
		proto_role_cap->cap_option |= EXT_CAP_LIMIT_2G_RX_STBC;
	else
		proto_role_cap->cap_option &= ~(EXT_CAP_LIMIT_2G_RX_STBC);

	_phl_init_proto_stbc_cap(phl_info, role->hw_band, proto_role_cap);
}
