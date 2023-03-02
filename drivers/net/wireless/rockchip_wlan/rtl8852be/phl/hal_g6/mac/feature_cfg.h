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

#ifndef _MAC_AX_FEATURE_CFG_H_
#define _MAC_AX_FEATURE_CFG_H_

#ifndef __cplusplus /* for win/linux driver */

/* [Note] Modify MakeFile to reduce code size */

/* Debug package for debugging */
#define MAC_AX_FEATURE_DBGPKG	1

/* Hadrware verification module */
#define MAC_AX_FEATURE_HV	0

/* FW offload reg read/write  */
#define MAC_AX_FW_REG_OFLD	0

/* Temp code for FPGA verification*/
#define MAC_AX_FPGA_TEST	0

/* Temp code for lack of BTC driver*/
#define MAC_AX_ASIC_TEMP	1

/* temp for h2c alloc move to phl layer*/
#define MAC_AX_PHL_H2C		1

/* Debug command */
#define MAC_AX_FEATURE_DBGCMD		1

/* Debug Log Decode */
#define MAC_AX_FEATURE_DBGDEC		1

#ifdef CONFIG_BTCOEX
#define MAC_AX_COEX_INIT_EN	0
#else
#define MAC_AX_COEX_INIT_EN	1
#endif

#else /* for WD1 test program */

/* [Note] Modify MakeFile to reduce code size */

/* Debug package for debugging */
#define MAC_AX_FEATURE_DBGPKG	1

/* Debug package for debugging */
#define MAC_AX_FW_REG_OFLD	0

/* Hadrware verification module */
#define MAC_AX_FEATURE_HV	1

#define MAC_AX_FPGA_TEST	1

#define MAC_AX_ASIC_TEMP	1

/* temp for h2c alloc move to phl layer */
#define MAC_AX_PHL_H2C		0

/* [HV] for SIC command generagor */
#define MAC_AX_HV_SIC_GEN	0

#define MAC_AX_COEX_INIT_EN	1

/* Debug command */
#define MAC_AX_FEATURE_DBGCMD		1

/* Debug Log Decode */
#define MAC_AX_FEATURE_DBGDEC		1
#endif // #else /* for WD1 test program */

#endif

