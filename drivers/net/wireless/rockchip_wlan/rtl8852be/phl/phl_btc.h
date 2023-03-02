/******************************************************************************
 *
 * Copyright(c) 2019 - 2020 Realtek Corporation.
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
#ifndef __PHL_BTC_H__
#define __PHL_BTC_H__

/* For EXTERNAL application to notify btc (expose) */
/* @phl: refer to phl_infi_t
 * @notify: notification event
 * @ntfy: notify information (optional)
 * return value:
 * 	0: no wait
 * 	1: have to wait call back info->ntfy_cb()
 *	-1: Failure
 */
int rtw_phl_btc_notify(void *phl, enum RTW_PHL_BTC_NOTIFY notify,
	struct rtw_phl_btc_ntfy *info);
void rtw_phl_btc_role_notify(void *phl, u8 role_id, enum role_state rstate);
void rtw_phl_btc_hub_msg_hdl(void *phl, struct phl_msg *msg);
void rtw_phl_btc_packet_event_notify(void *phl, u8 role_id, u8 pkt_evt_type);
u8 rtw_phl_btc_pkt_2_evt_type(u8 packet_type);

#endif /* __PHL_BTC_H__ */
