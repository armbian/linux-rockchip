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
#define _RTW_TRX_USB_C_
#include <drv_types.h>		/* struct dvobj_priv and etc. */


/********************************xmit section*******************************/
#ifdef CONFIG_USB_TX_AGGREGATION
#define IDEA_CONDITION 1	/* check all packets before enqueue */
static s32 usb_xmitframe_process(_adapter *padapter,
		struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf)
{	
	return _TRUE;
}

#else /* CONFIG_USB_TX_AGGREGATION */

static s32 usb_xmitframe_process(_adapter *padapter,
		struct xmit_priv *pxmitpriv, struct xmit_buf *pxmitbuf)
{
	return _TRUE;

}
#endif


static void usb_xmit_tasklet(_adapter *padapter)
{
#ifdef CONFIG_TX_AMSDU_SW_MODE
	core_tx_amsdu_tasklet(padapter);
#endif
}

s32 usb_init_xmit_priv(_adapter *adapter)
{
	s32 ret = _SUCCESS;
	struct xmit_priv *pxmitpriv = &adapter->xmitpriv;
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);

	rtw_tasklet_init(&pxmitpriv->xmit_tasklet,
		     (void(*)(unsigned long))usb_xmit_tasklet,
		     (unsigned long)adapter);

	return _SUCCESS;
}

void usb_free_xmit_priv(_adapter *adapter)
{
}

#if 0 /*def CONFIG_XMIT_THREAD_MODE*/
/*
 * Description
 *	Transmit xmitbuf to hardware tx fifo
 *
 * Return
 *	_SUCCESS	ok
 *	_FAIL		something error
 */
s32 usb_xmit_buf_handler(_adapter *adapter)
{
	return _SUCCESS;
}
#endif /* CONFIG_XMIT_THREAD_MODE */

s32 usb_xmitframe_enqueue(_adapter *adapter, struct xmit_frame *pxmitframe)
{
	return _SUCCESS;
}

/******************************** recv section*******************************/
int usb_init_recv_priv(struct dvobj_priv *dvobj)
{
	int res = _SUCCESS;

	return res;
}

void usb_free_recv_priv(struct dvobj_priv *dvobj)
{

}

struct rtw_intf_ops usb_ops = {
	.init_xmit_priv = usb_init_xmit_priv,
	.free_xmit_priv = usb_free_xmit_priv,
	.xmitframe_enqueue = usb_xmitframe_enqueue,
	#if 0 /*def CONFIG_XMIT_THREAD_MODE*/
	.xmit_buf_handler = usb_xmit_buf_handler,
	#endif

	.init_recv_priv = usb_init_recv_priv,
	.free_recv_priv = usb_free_recv_priv,

};

