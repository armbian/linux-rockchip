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
#ifndef _RTW_TRX_OPS_H_
#define _RTW_TRX_OPS_H_
#include <drv_types.h>

static inline s32 rtw_intf_init_xmit_priv(_adapter *adapter)
{
	return adapter_to_dvobj(adapter)->intf_ops->init_xmit_priv(adapter);
}

static inline void rtw_intf_free_xmit_priv(_adapter *adapter)
{

	adapter_to_dvobj(adapter)->intf_ops->free_xmit_priv(adapter);
}
static inline s32 rtw_intf_data_xmit(_adapter *adapter,
					struct xmit_frame *pxmitframe)
{
	return adapter_to_dvobj(adapter)->intf_ops->data_xmit(adapter, pxmitframe);
}

static inline s32 rtw_intf_xmitframe_enqueue(_adapter *adapter,
						struct xmit_frame *pxmitframe)
{
	u32 rtn;

	/* enqueue is not necessary, casuse phl use sw queue to save xmitframe */
	rtn = core_tx_call_phl(adapter, pxmitframe, NULL);

	if (rtn == FAIL)
		core_tx_free_xmitframe(adapter, pxmitframe);

	return rtn;
}

static inline u8 rtw_intf_start_xmit_frame_thread(_adapter *adapter)
{
	u8 rst = _SUCCESS;
#if defined(CONFIG_SDIO_HCI) || defined(CONFIG_GSPI_HCI)
#ifndef CONFIG_SDIO_TX_TASKLET
	if (adapter_to_dvobj(adapter)->intf_ops->start_xmit_frame_thread)
		rst = adapter_to_dvobj(adapter)->intf_ops->start_xmit_frame_thread(adapter);
#endif
#endif
	return rst;
}
static inline void rtw_intf_cancel_xmit_frame_thread(_adapter *adapter)
{
#if defined(CONFIG_SDIO_HCI) || defined(CONFIG_GSPI_HCI)
#ifndef CONFIG_SDIO_TX_TASKLET
	if (adapter_to_dvobj(adapter)->intf_ops->cancel_xmit_frame_thread)
		adapter_to_dvobj(adapter)->intf_ops->cancel_xmit_frame_thread(adapter);
#endif
#endif
}

#if 0 /*def CONFIG_XMIT_THREAD_MODE*/
static inline s32 rtw_intf_xmit_buf_handler(_adapter *adapter)
{
	return adapter_to_dvobj(adapter)->intf_ops->xmit_buf_handler(adapter);
}
#endif


/************************ recv *******************/
static inline s32 rtw_intf_init_recv_priv(struct dvobj_priv *dvobj)
{
	return dvobj->intf_ops->init_recv_priv(dvobj);
}
static inline void rtw_intf_free_recv_priv(struct dvobj_priv *dvobj)
{
	return dvobj->intf_ops->free_recv_priv(dvobj);
}

#ifdef CONFIG_RECV_THREAD_MODE
static inline s32 rtw_intf_recv_hdl(_adapter *adapter)
{
	return adapter_to_dvobj(adapter)->intf_ops->recv_hdl(adapter);
}
#endif

struct lite_data_buf *rtw_alloc_litedatabuf(struct trx_data_buf_q *data_buf_q);
s32 rtw_free_litedatabuf(struct trx_data_buf_q *data_buf_q,
		struct lite_data_buf *lite_data_buf);

#endif /* _RTW_TRX_OPS_H_ */
