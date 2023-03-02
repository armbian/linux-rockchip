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
#ifndef _PHL_ACS_H_
#define _PHL_ACS_H_

/* avoid clm/nhm result not ready when scan done */
#define MONITOR_TIME_TOLERANCE 15

void phl_acs_mntr_trigger(struct phl_info_t *phl_info, u8 ch_idx, u16 channel, u16 monitor_time);
void phl_acs_mntr_result(struct phl_info_t *phl_info);

#endif /*_PHL_ACS_H_*/
