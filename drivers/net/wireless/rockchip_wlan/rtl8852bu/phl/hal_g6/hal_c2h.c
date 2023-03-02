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
#define _HAL_C2H_C_
#include "hal_headers.h"


struct hal_c2h_hdl c2h_list[] =
{
	{C2H_CAT_OUTSRC, C2H_CLS_PHYDM_MIN, C2H_CLS_PHYDM_MAX,
		rtw_hal_bb_process_c2h},
	{C2H_CAT_OUTSRC, C2H_CLS_BTC_MIN, C2H_CLS_BTC_MAX,
		rtw_hal_btc_process_c2h},
	{C2H_CAT_MAC, C2H_CLS_MAC_MIN, C2H_CLS_MAC_MAX,
		rtw_hal_mac_process_c2h},
	{C2H_CAT_MAX, C2H_CLS_MAX, C2H_CLS_MAX, NULL}
};

void hal_c2h_post_process(void *phl, void *hal, void *c2h)
{
	struct rtw_phl_com_t *phl_com = (struct rtw_phl_com_t *)phl;
	struct rtw_c2h_info *c = (struct rtw_c2h_info *)c2h;
	u8 i = 0;
	u32 ret = 0;

	for (i = 0; C2H_CAT_MAX != c2h_list[i].cat; i++) {
		if (c->c2h_cat == c2h_list[i].cat) {
			if (c->c2h_class >= c2h_list[i].cls_min &&
				c->c2h_class <= c2h_list[i].cls_max) {
				ret = c2h_list[i].c2h_hdl(hal, c2h);
				break;
			}
		}
	}

	if (ret > 0) {
		/* submit phl msg hub */
		struct phl_msg msg = {0};
		struct phl_msg_attribute attr = {0};

		SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_MDL_RX);
		SET_MSG_EVT_ID_FIELD(msg.msg_id, (u16)ret);
		if (rtw_phl_msg_hub_hal_send(phl_com, &attr, &msg) !=
			RTW_PHL_STATUS_SUCCESS) {
			PHL_ERR("%s rtw_phl_msg_hub_hal_send failed\n", __func__);
		}
	}
}

