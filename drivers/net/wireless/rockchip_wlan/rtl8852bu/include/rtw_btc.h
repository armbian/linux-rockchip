/******************************************************************************
 *
 * Copyright(c) 2013 - 2017 Realtek Corporation.
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
#ifdef CONFIG_BTC

#ifndef __RTW_BTC_H__
#define __RTW_BTC_H__

#include <drv_types.h>

#define GET_STATUS_CODE_FROM_BT_MP_OPER_RET(RetCode)				(RetCode & 0x0F)
#define CHECK_STATUS_CODE_FROM_BT_MP_OPER_RET(RetCode, StatusCode)	(GET_STATUS_CODE_FROM_BT_MP_OPER_RET(RetCode) == StatusCode)

void rtw_btc_disp_btc_info(_adapter *, void* p_msgprn_hdl, u8 info_type);
void rtw_btc_set_dbg(_adapter *, u32 *pDbgModule);
u32 rtw_btc_get_dbg(_adapter *, u8 *pStrBuf, u32 bufSize);

#if 0
u16 rtw_btc_btreg_read(_adapter *padapter, u8 type, u16 addr, u32 *data);
u16 rtw_btc_btreg_write(_adapter *padapter, u8 type, u16 addr, u16 val);
#endif

#endif /* __RTW_BTC_H__ */
#endif /* CONFIG_BTC */

