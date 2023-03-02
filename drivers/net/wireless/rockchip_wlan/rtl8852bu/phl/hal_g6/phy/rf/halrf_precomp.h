/******************************************************************************
 *
 * Copyright(c) 2007 - 2020  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/
#ifndef __HALRF_PRECOMP_H__
#define __HALRF_PRECOMP_H__

/*@--------------------------[Define] ---------------------------------------*/

#ifdef CONFIG_RTL8852A
    #define RF_8852A_SUPPORT
#endif

#ifdef CONFIG_RTL8852B
    #define RF_8852B_SUPPORT
#endif

#ifdef CONFIG_RTL8852C
    #define RF_8852C_SUPPORT
#endif

#define	MASKBYTE0		0xff
#define	MASKBYTE1		0xff00
#define	MASKBYTE2		0xff0000
#define	MASKBYTE3		0xff000000
#define	MASKHWORD		0xffff0000
#define	MASKLWORD		0x0000ffff
#define	MASKDWORD		0xffffffff
#define	MASKRF		0xfffff
#define	MASKRFMODE		0xf0000
#define	MASKRFRXBB		0x003e0
#define	MASKTXPWR		0x0003f
#define	INVALID_RF_DATA 0xffffffff


/*---[Define Only] ----------------------------------------------------------*/
#include "../../hal_headers_le.h"
#include "halrf_ic_hw_info.h"
#include "halrf_ic_sw_info.h"

/*---[Include structure & prototype] ----------------------------------------*/

#include "halrf_hw_cfg.h"
#include "halrf_hw_cfg_ex.h"
#include "halrf_interface.h"
#include "halrf_dbg_cmd.h"
#include "halrf_dbg.h"
#include "halrf_txgapk.h"
#include "halrf_pwr_track.h"
#include "halrf_iqk.h"
#include "halrf_dpk.h"
#include "halrf_dack.h"
#include "halrf_pmac.h"
//#include "halrf_dbg_cmd.h"
#include "halrf_dbg_cmd_ex.h"
#include "halrf_init.h"
#include "halrf_init_ex.h"
#include "halrf_pwr_table.h"
#include "halrf_api.h"
#include "halrf_psd.h"
#include "halrf_kfree.h"
#include "halrf_hwimg.h"
#ifdef RF_8852A_SUPPORT
#include "halrf_8852a/halrf_dack_8852a.h"
#include "halrf_8852a/halrf_hwimg_8852a.h"
#include "halrf_8852a/halrf_kfree_8852a.h"
#endif

#ifdef RF_8852B_SUPPORT
#include "halrf_8852b/halrf_dack_8852b.h"
#include "halrf_8852b/halrf_hwimg_8852b.h"
#include "halrf_8852b/halrf_kfree_8852b.h"
#endif

#ifdef RF_8852C_SUPPORT
//#include "halrf_8852c/halrf_dack_8852c.h"
//#include "halrf_8852c/halrf_hwimg_8852c.h"
//#include "halrf_8852c/halrf_kfree_8852c.h"
#endif

#include "halrf.h"
#include "halrf_ex.h"

#ifdef RF_8852A_SUPPORT
	#include "halrf_8852a/halrf_efuse_8852a.h"
	#include "halrf_8852a/halrf_reg_cfg_8852a.h"
	#include "halrf_8852a/halrf_8852a.h"
	#include "halrf_8852a/halrf_8852a_api.h"
	#include "halrf_8852a/halrf_8852a_api_ex.h"
	#include "halrf_8852a/halrf_iqk_8852a.h"
	#include "halrf_8852a/halrf_dpk_8852a.h"
	#include "halrf_8852a/halrf_txgapk_8852a.h"
	#include "halrf_8852a/halrf_version_rtl8852a.h"
	#include "halrf_8852a/halrf_set_pwr_table_8852a.h"
	#include "halrf_8852a/halrf_tssi_8852a.h"
	#include "halrf_8852a/halrf_psd_8852a.h"
#endif

#ifdef RF_8852B_SUPPORT
	#include "halrf_8852b/halrf_efuse_8852b.h"
	#include "halrf_8852b/halrf_reg_cfg_8852b.h"
	#include "halrf_8852b/halrf_8852b.h"
	#include "halrf_8852b/halrf_8852b_api.h"
	#include "halrf_8852b/halrf_8852b_api_ex.h"
	#include "halrf_8852b/halrf_iqk_8852b.h"
	#include "halrf_8852b/halrf_dpk_8852b.h"
	#include "halrf_8852b/halrf_txgapk_8852b.h"
	#include "halrf_8852b/halrf_version_rtl8852b.h"
	#include "halrf_8852b/halrf_set_pwr_table_8852b.h"
	#include "halrf_8852b/halrf_tssi_8852b.h"
	#include "halrf_8852b/halrf_psd_8852b.h"
#endif

#ifdef RF_8852C_SUPPORT
//	#include "halrf_8852c/halrf_efuse_8852c.h"
//	#include "halrf_8852c/halrf_reg_cfg_8852c.h"
	#include "halrf_8852c/halrf_8852c.h"
//	#include "halrf_8852c/halrf_8852c_api.h"
//	#include "halrf_8852c/halrf_8852c_api_ex.h"
//	#include "halrf_8852c/halrf_iqk_8852c.h"
//	#include "halrf_8852c/halrf_dpk_8852c.h"
//	#include "halrf_8852c/halrf_txgapk_8852c.h"
//	#include "halrf_8852c/halrf_version_rtl8852c.h"
//	#include "halrf_8852c/halrf_set_pwr_table_8852c.h"
//	#include "halrf_8852c/halrf_tssi_8852c.h"
#endif



#endif
