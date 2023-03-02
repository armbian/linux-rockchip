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

#include <drv_types.h>

typedef struct _btcdbginfo {	
	void* p_msgprn_hdl;
	struct submit_ctx btc_dbg_info_sctx;
} BTCDBGINFO, *PBTCDBGINFO;

BTCDBGINFO GLBtcDbgInfo;

#define BTC_INFO_DUMP_TIMEOUT_MS	3000

static void rtw_btc_info_dump_init(_adapter *padapter)
{
	GLBtcDbgInfo.p_msgprn_hdl = NULL;
}

static void rtw_btc_info_dump_set(_adapter *padapter, void* p_msgprn_hdl, u32 timeout_ms)
{
	GLBtcDbgInfo.p_msgprn_hdl = p_msgprn_hdl;
	rtw_sctx_init(&GLBtcDbgInfo.btc_dbg_info_sctx, timeout_ms);
}

static void rtw_btc_info_dump(const char *pmsg)
{
	if (GLBtcDbgInfo.p_msgprn_hdl == NULL)
		return;

	_RTW_PRINT_SEL(GLBtcDbgInfo.p_msgprn_hdl, "%s", pmsg);
}

static int rtw_btc_info_dump_callback(void *priv, enum RTW_PHL_BTC_NOTIFY ntfy, struct rtw_phl_btc_ntfy *info)
{
	struct submit_ctx *sctx = &GLBtcDbgInfo.btc_dbg_info_sctx;

	rtw_sctx_done(&sctx);

	return 0;
}

static int rtw_btc_info_dump_wait(_adapter *padapter)
{
	return rtw_sctx_wait(&GLBtcDbgInfo.btc_dbg_info_sctx, __func__);
}

void rtw_btc_disp_btc_info(_adapter *padapter, void* p_msgprn_hdl, u8 info_type)
{
	struct rtw_phl_btc_ntfy ntfy = {0};
	struct rtw_phl_btc_coex_info_param *pcinfo = &ntfy.u.cinfo;
	struct dvobj_priv *dvobj = adapter_to_dvobj(padapter);
	struct rtw_phl_btc_ops phl_btc_ops = {rtw_btc_info_dump};

	pcinfo->query_type = info_type;
	
	ntfy.notify = PHL_BTC_NTFY_COEX_INFO;
	ntfy.ops = &phl_btc_ops;
	ntfy.priv = padapter;
	ntfy.ntfy_cb = rtw_btc_info_dump_callback;

	rtw_btc_info_dump_set(padapter, p_msgprn_hdl, BTC_INFO_DUMP_TIMEOUT_MS);

	rtw_phl_btc_notify(dvobj->phl, ntfy.notify, &ntfy);

	/* wait here until info dump finishes or timeout expires */
	rtw_btc_info_dump_wait(padapter);

	rtw_btc_info_dump_init(padapter);
}

void rtw_btc_set_dbg(_adapter *padapter, u32 *pDbgModule)
{
}

u32 rtw_btc_get_dbg(_adapter *padapter, u8 *pStrBuf, u32 bufSize)
{
	return 0;
}

#if 0
u16 rtw_btc_btreg_read(_adapter *padapter, u8 type, u16 addr, u32 *data)
{
	return 0;
}

u16 rtw_btc_btreg_write(_adapter *padapter, u8 type, u16 addr, u16 val)
{
	return _SUCCESS;
}
#endif

#endif /* CONFIG_BTC */

