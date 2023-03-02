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
#ifndef __PHL_SCAN_FSM_H__
#define __PHL_SCAN_FSM_H__

/* fsm private struct */
struct fsm_root;
struct fsm_main;

/* scan fsm init api */
struct fsm_main *phl_scan_new_fsm(struct fsm_root *fsm_m,
	struct phl_info_t *phl_info);
void phl_scan_destory_fsm(struct fsm_main *fsm);
struct scan_obj *phl_scan_new_obj(struct fsm_main *fsm,
	struct phl_info_t *phl_info);
void phl_scan_destory_obj(struct scan_obj *pscan);
bool phl_fsm_should_stop(struct fsm_main *fsm);

#endif /* __PHL_SCAN_FSM_H__ */
