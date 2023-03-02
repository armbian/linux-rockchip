/** @file */
/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
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
 ******************************************************************************/

#ifndef _MAC_AX_PWR_SEQ_FUNC_8852B_H_
#define _MAC_AX_PWR_SEQ_FUNC_8852B_H_

#include "../../mac_def.h"
#if MAC_AX_8852B_SUPPORT

#define PWR_SEQ_VER_8852B  10

u32 mac_pwr_on_sdio_8852b(void *vadapter);
u32 mac_pwr_on_usb_8852b(void *vadapter);
u32 mac_pwr_on_pcie_8852b(void *vadapter);

u32 mac_pwr_off_sdio_8852b(void *vadapter);
u32 mac_pwr_off_usb_8852b(void *vadapter);
u32 mac_pwr_off_pcie_8852b(void *vadapter);
#if MAC_AX_FEATURE_HV
u32 mac_enter_lps_sdio_8852b(void *vadapter);
u32 mac_enter_lps_usb_8852b(void *vadapter);
u32 mac_enter_lps_pcie_8852b(void *vadapter);

u32 mac_leave_lps_sdio_8852b(void *vadapter);
u32 mac_leave_lps_usb_8852b(void *vadapter);
u32 mac_leave_lps_pcie_8852b(void *vadapter);

#endif /* #if MAC_AX_FEATURE_HV */
#endif /* #if MAC_AX_8852B_SUPPORT */
#endif
