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
#ifndef __PHL_BTC_FSM_H__
#define __PHL_BTC_FSM_H__

/* Header file for application to invoke btcoex service */

#define RTW_BTC_FWBUF_NUM 4
#define RTW_BTC_NAME_MAX 20

struct fsm_root;
struct fsm_main;
struct btc_obj;

/* function form btcoex service management */
struct fsm_main *phl_btc_new_fsm(struct fsm_root *fsm_m,
	struct phl_info_t *phl_info);
void phl_btc_destory_fsm(struct fsm_main *fsm);
struct btc_obj *phl_btc_new_obj(struct fsm_main *fsm,
	struct phl_info_t *phl_info);
void phl_btc_destory_obj(struct btc_obj *pbtc);

/* function form btcoex service */
enum rtw_phl_status phl_btc_start(struct btc_obj *pbtc);
enum rtw_phl_status phl_btc_cancel(struct btc_obj *pbtc);

#endif /* __PHL_BTC_FSM_H__ */

