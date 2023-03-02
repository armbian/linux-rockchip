/******************************************************************************
 *
 * Copyright(c) 2019 - 2021 Realtek Corporation.
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
#ifndef __RTW_CSA_H_
#define __RTW_CSA_H_

#if CONFIG_DFS
#define CSA_IE_LEN 3 /* Length of Channel Switch element */
#define CSA_SWITCH_MODE 0
#define CSA_NEW_CH 1
#define CSA_SWITCH_COUNT 2
#define MAX_CSA_CNT 10

#define CS_WR_DATA_LEN 5 /* Length of Channel Switch Wrapper element */

void reset_csa_param(struct rf_ctl_t *rfctl);
bool rtw_get_csa_setting(struct dvobj_priv *d, s16 *req_ch, u8 *req_bw, u8 *req_offset);

#ifdef CONFIG_ECSA_PHL
#define MCC_ECSA_DELAY_START_TIME 30000 /* ms */

enum ecsa_state_t {
	ECSA_ST_NONE,
	ECSA_ST_SW_START, /* ready to switch */
	ECSA_ST_SW_DONE /* switch done */
};

struct core_ecsa_info {
	enum ecsa_state_t state;
	u32 ecsa_allow_case;
	u32 ecsa_delay_time;

	/* @channel_width defined in 802.11-2016, Table 9-252 VHT operation information subfields
	* 0 for 20 MHz or 40 MHz
	* 1 for 80 MHz, 160 MHz or 80+80 MHz
	* 2 for 160 MHz (deprecated)
	* 3 for non-contiguous 80+80 MHz (deprecated)
	*/
	u8 channel_width;
	struct createbss_parm *bss_param;
	struct rtw_phl_ecsa_param phl_ecsa_param;
};

__inline static void set_ecsa_state(struct core_ecsa_info *ecsa_info, enum ecsa_state_t state)
{
	ecsa_info->state = state;
}

__inline static bool check_ecsa_state(struct core_ecsa_info *ecsa_info, enum ecsa_state_t state)
{
	if (ecsa_info->state == state)
		return _TRUE;
	return _FALSE;
}

#define SET_ECSA_STATE(adapter, state) set_ecsa_state(&((adapter)->ecsa_info), (state))
#define CHK_ECSA_STATE(adapter, state) check_ecsa_state(&((adapter)->ecsa_info), (state))

bool rtw_mr_is_ecsa_running(struct _ADAPTER *a);
void rtw_ecsa_update_probe_resp(struct xmit_frame *xframe);
void rtw_ecsa_update_beacon(void *priv, struct rtw_wifi_role_t *role);
bool rtw_ap_check_ecsa_allow(
	void *priv,
	struct rtw_wifi_role_t *role,
	struct rtw_chan_def chan_def,
	enum phl_ecsa_start_reason reason,
	u32 *delay_start_ms
);
void rtw_ecsa_mr_update_chan_info_by_role(
	void *priv,
	struct rtw_wifi_role_t *role,
	struct rtw_chan_def new_chan_def
);
bool rtw_ecsa_check_tx_resume_allow(void *priv, struct rtw_wifi_role_t *role);
void rtw_ecsa_complete(void *priv, struct rtw_wifi_role_t *role);
void rtw_trigger_phl_ecsa_start(struct _ADAPTER *a);
#endif /* CONFIG_ECSA_PHL */
#endif /* CONFIG_DFS */

#endif
