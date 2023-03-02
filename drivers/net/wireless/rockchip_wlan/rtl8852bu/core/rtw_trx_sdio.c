/******************************************************************************
 *
 * Copyright(c) 2015 - 2019 Realtek Corporation.
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
#define _RTW_TRX_SDIO_C_

#include <drv_types.h>		/* struct dvobj_priv and etc. */
#include <drv_types_sdio.h>	/* RTW_SDIO_ADDR_CMD52_GEN */
#include <rtw_sdio.h>

#define CONFIG_NEW_SDIO_WP_FUNC
#ifdef CONFIG_NEW_SDIO_WP_FUNC
/*
 * Description:
 *	Write to TX FIFO
 *	Align write size to block size,
 *	and check enough FIFO size to write.
 *
 * Parameters:
 *	addr		not use
 *	cnt		size to write
 *	mem		struct xmit_buf*
 *
 * Return:
 *	_SUCCESS(1)	Success
 *	_FAIL(0)	Fail
 */
static u32 rtw_sdio_xmit(struct dvobj_priv *d, u32 txaddr, u32 cnt, u8 *mem)
{
	u32 txsize;
	u32 ret = _FAIL;

	cnt = _RND4(cnt);

	/* align size to guarantee I/O would be done in one command */
	txsize = rtw_sdio_cmd53_align_size(d, cnt);

	ret = rtw_sdio_write_cmd53(d, txaddr, mem, txsize);

	/*GEORGIA_TODO_FIXIT_MOVE_TO_CALLER*/
	/*rtw_sctx_done_err(&xmitbuf->sctx,
		(_FAIL == ret) ? RTW_SCTX_DONE_WRITE_PORT_ERR : RTW_SCTX_DONE_SUCCESS);*/
exit :
	return ret;
}
#else
static u32 rtw_sdio_xmit(struct dvobj_priv *d, u32 txaddr, u32 cnt, u8 *mem)
{
	s32 err;
	u32 txaddr;

	cnt = _RND4(cnt);
	cnt = rtw_sdio_cmd53_align_size(d, cnt);

	err = sd_write(d, txaddr, cnt, mem);

	/*GEORGIA_TODO_FIXIT_MOVE_TO_CALLER*/
	/*rtw_sctx_done_err(&xmitbuf->sctx,
		 err ? RTW_SCTX_DONE_WRITE_PORT_ERR : RTW_SCTX_DONE_SUCCESS);*/

	if (err) {
		RTW_ERR("%s, error=%d\n", __func__, err);
		return _FAIL;
	}
	return _SUCCESS;
}
#endif


/********************************xmit section*******************************/
#ifdef CONFIG_TX_AMSDU_SW_MODE
static void sdio_xmit_tasklet(_adapter *padapter)
{
	core_tx_amsdu_tasklet(padapter);
}
#endif

s32 sdio_init_xmit_priv(_adapter *adapter)
{
#ifdef CONFIG_TX_AMSDU_SW_MODE
	struct xmit_priv *xmitpriv = &adapter->xmitpriv;

	rtw_tasklet_init(&xmitpriv->xmit_tasklet,
			(void(*)(unsigned long))sdio_xmit_tasklet,
			(unsigned long)adapter);
#endif
	return _SUCCESS;
}

void sdio_free_xmit_priv(_adapter *adapter)
{
#if 0 /*CONFIG_CORE_XMITBUF*/
	struct xmit_priv *pxmitpriv;
	struct xmit_buf *pxmitbuf;
	_queue *pqueue;
	_list *plist, *phead;
	_list tmplist;

	pxmitpriv = &adapter->xmitpriv;
	pqueue = &pxmitpriv->pending_xmitbuf_queue;
	phead = get_list_head(pqueue);
	_rtw_init_listhead(&tmplist);

	_rtw_spinlock_bh(&pqueue->lock);
	if (_rtw_queue_empty(pqueue) == _FALSE) {
		/*
		 * Insert tmplist to end of queue, and delete phead
		 * then tmplist become head of queue.
		 */
		rtw_list_insert_tail(&tmplist, phead);
		rtw_list_delete(phead);
	}
	_rtw_spinunlock_bh(&pqueue->lock);

	phead = &tmplist;
	while (rtw_is_list_empty(phead) == _FALSE) {
		plist = get_next(phead);
		rtw_list_delete(plist);

		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);
		rtw_free_xmitframe(pxmitpriv, (struct xmit_frame *)pxmitbuf->priv_data);
		pxmitbuf->priv_data = NULL;
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
	}
#endif
}

/*
 * Description:
 *	Handle xmitframe(packet) come from rtw_xmit()
 *
 * Return:
 *	_TRUE	handle packet directly, maybe ok or drop
 *	_FALSE	enqueue, temporary can't transmit packets to hardware
 */
s32 sdio_data_xmit(_adapter *adapter, struct xmit_frame *pxmitframe)
{
	struct xmit_priv *pxmitpriv;
	s32 err;

	pxmitframe->attrib.qsel = pxmitframe->attrib.priority;
	pxmitpriv = &adapter->xmitpriv;

#ifdef CONFIG_80211N_HT
	if ((pxmitframe->frame_tag == DATA_FRAMETAG)
		&& (pxmitframe->attrib.ether_type != 0x0806)
		&& (pxmitframe->attrib.ether_type != 0x888e)
		&& (pxmitframe->attrib.dhcp_pkt != 1)) {
		rtw_issue_addbareq_cmd(adapter, pxmitframe, _TRUE);
	}
#endif /* CONFIG_80211N_HT */

	_rtw_spinlock_bh(&pxmitpriv->lock);
	err = rtw_xmitframe_enqueue(adapter, pxmitframe);
	_rtw_spinunlock_bh(&pxmitpriv->lock);
	if (err != _SUCCESS) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);

		pxmitpriv->tx_drop++;
		return _TRUE;
	}

#ifdef CONFIG_SDIO_TX_TASKLET
	rtw_tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#else
	_rtw_up_sema(&pxmitpriv->SdioXmitSema);
#endif

	return _FALSE;
}

/*
 * Description:
 *	Aggregation packets and send to hardware
 *
 * Return:
 *	0	Success
 *	-1	Hardware resource(TX FIFO) not ready
 *	-2	Software resource(xmitbuf) not ready
 */
 /*SDIO bus-agg*/
static s32 _sdio_xmit_xmitframes(_adapter *adapter, struct xmit_priv *pxmitpriv)
{
	s32 err = 0;

	return err;
}

/*
 * Description
 *	Transmit xmitframe from queue
 *
 * Return
 *	_SUCCESS	ok
 *	_FAIL		something error
 */
static s32 _sdio_xmit_frame_handler(_adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct xmit_priv *pxmitpriv = &adapter->xmitpriv;
	s32 ret;

	ret = _rtw_down_sema(&pxmitpriv->SdioXmitSema);
	if (_FAIL == ret) {
		RTW_ERR("%s: down sema fail!\n", __FUNCTION__);
		return _FAIL;
	}

next:
	if (RTW_CANNOT_RUN(dvobj)) {
		RTW_DBG(FUNC_ADPT_FMT "- bDriverStopped(%s) bSurpriseRemoved(%s)\n",
			 FUNC_ADPT_ARG(adapter),
			 dev_is_drv_stopped(dvobj) ? "True" : "False",
			 dev_is_surprise_removed(dvobj) ? "True" : "False");
		return _FAIL;
	}

	_rtw_spinlock_bh(&pxmitpriv->lock);
	ret = rtw_txframes_pending(adapter);
	_rtw_spinunlock_bh(&pxmitpriv->lock);
	/* All queues are empty! */
	if (ret == 0)
		return _SUCCESS;

	/* Dequeue frame and agg-tx then enqueue pending xmitbuf-queue */
	ret = _sdio_xmit_xmitframes(adapter, pxmitpriv);
	if (ret == -2) {
		/* here sleep 1ms will cause big TP loss of TX */
		/* from 50+ to 40+ */
		if (adapter->registrypriv.wifi_spec)
		rtw_msleep_os(1);
		else
			#ifdef CONFIG_REDUCE_TX_CPU_LOADING
			rtw_msleep_os(1);
			#else
			rtw_yield_os();
			#endif

		goto next;
	}
	_rtw_spinlock_bh(&pxmitpriv->lock);
	ret = rtw_txframes_pending(adapter);
	_rtw_spinunlock_bh(&pxmitpriv->lock);
	if (ret == 1)
		goto next;

	return _SUCCESS;
}

thread_return _sdio_xmit_frame_thread(thread_context context)
{
	s32 ret;
	_adapter *adapter;
	struct xmit_priv *pxmitpriv;
	u8 thread_name[20] = {0};


	ret = _SUCCESS;
	adapter = (_adapter *)context;
	pxmitpriv = &adapter->xmitpriv;

	rtw_sprintf(thread_name, 20, "RTWHALXT-"ADPT_FMT, ADPT_ARG(adapter));
	rtw_thread_enter(thread_name);

	RTW_INFO("start "FUNC_ADPT_FMT"\n", FUNC_ADPT_ARG(adapter));

	do {
		ret = _sdio_xmit_frame_handler(adapter);
		flush_signals_thread();
	} while (_SUCCESS == ret);

	RTW_INFO(FUNC_ADPT_FMT " Exit\n", FUNC_ADPT_ARG(adapter));

	rtw_thread_wait_stop();

	return 0;
}
u8 sdio_start_xmit_frame_thread(_adapter *adapter)
{
	u8 _status = _SUCCESS;

#ifndef CONFIG_SDIO_TX_TASKLET
	struct xmit_priv *xmitpriv = &adapter->xmitpriv;

	if (xmitpriv->SdioXmitThread == NULL) {
		RTW_INFO(FUNC_ADPT_FMT " start RTWHALXT\n", FUNC_ADPT_ARG(adapter));
		xmitpriv->SdioXmitThread = rtw_thread_start(_sdio_xmit_frame_thread, adapter, "RTWHALXT");
		if (xmitpriv->SdioXmitThread == NULL) {
			RTW_ERR("%s: start _sdio_xmit_frame_thread FAIL!!\n", __FUNCTION__);
			_status = _FAIL;
		}
	}
#endif /* !CONFIG_SDIO_TX_TASKLET */
	return _status;
}

void sdio_cancel_xmit_frame_thread(_adapter *adapter)
{
#ifndef CONFIG_SDIO_TX_TASKLET
	struct xmit_priv *xmitpriv = &adapter->xmitpriv;

	/* stop xmit_buf_thread */
	if (xmitpriv->SdioXmitThread) {
		_rtw_up_sema(&xmitpriv->SdioXmitSema);
		rtw_thread_stop(xmitpriv->SdioXmitThread);
		xmitpriv->SdioXmitThread = NULL;
	}
#endif /* !CONFIG_SDIO_TX_TASKLET */
}

s32 sdio_dequeue_xmit(_adapter *adapter)
{
#if 0
	struct mlme_priv *pmlmepriv = &adapter->mlmepriv;
	struct xmit_priv *pxmitpriv = &adapter->xmitpriv;
	struct dvobj_priv *pdvobjpriv = adapter_to_dvobj(adapter);
	struct xmit_buf *pxmitbuf;
	u32 polling_num = 0;
	u32 txaddr = 0;

#ifdef CONFIG_SDIO_TX_ENABLE_AVAL_INT

#endif
	pxmitbuf = select_and_dequeue_pending_xmitbuf(adapter);

	if (pxmitbuf == NULL)
		return _TRUE;

#ifdef CONFIG_SDIO_TX_ENABLE_AVAL_INT
query_free_page:

	/* Total number of page is NOT available, so update current FIFO status*/
	u8	bUpdatePageNum = _FALSE;

	if (!bUpdatePageNum) {
		HalQueryTxBufferStatus8821CSdio(adapter);
		bUpdatePageNum = _TRUE;
		goto query_free_page;
	} else {
		bUpdatePageNum = _FALSE;
		enqueue_pending_xmitbuf_to_head(pxmitpriv, pxmitbuf);
		return _TRUE;
	}
#endif
	if (_TRUE == rtw_is_xmit_blocked(adapter)) {
		enqueue_pending_xmitbuf_to_head(pxmitpriv, pxmitbuf);
		/*rtw_msleep_os(1);*/
		return _FALSE;
	}

	/* check if hardware tx fifo page is enough */
	while (rtw_halmac_sdio_tx_allowed(pdvobjpriv, pxmitbuf->pdata, pxmitbuf->len)) {
		if (RTW_CANNOT_RUN(adapter_to_dvobj(adapter))) {
			RTW_INFO("%s: bSurpriseRemoved(write port)\n", __func__);
			goto free_xmitbuf;
		}

		polling_num++;
		/* Only polling (0x7F / 10) times here, since rtw_halmac_sdio_tx_allowed() has polled 10 times within */
		if ((polling_num % 6) == 0) {
			enqueue_pending_xmitbuf_to_head(pxmitpriv, pxmitbuf);
			rtw_msleep_os(1);
			return _FALSE;
		}
	}

#ifdef CONFIG_CHECK_LEAVE_LPS
	#ifdef CONFIG_LPS_CHK_BY_TP
	if (!adapter_to_pwrctl(adapter)->lps_chk_by_tp)
	#endif
		traffic_check_for_leave_lps(adapter, _TRUE, pxmitbuf->agg_num);
#endif

	if (_TRUE == rtw_is_xmit_blocked(adapter)) {
		enqueue_pending_xmitbuf_to_head(pxmitpriv, pxmitbuf);
		/*rtw_msleep_os(1);*/
		return _FALSE;
	}
	/*sdio_write_port(adapter, pxmitbuf->len, (u8 *)pxmitbuf);*/
	txaddr = rtw_hal_sdio_get_tx_addr(GET_PHL_COM(pdvobjpriv),
					pxmitbuf->pdata, pxmitbuf->len);
	rtw_sdio_xmit(adapter, txaddr, pxmitbuf->len, (u8 *)pxmitbuf);

free_xmitbuf:
	rtw_free_xmitbuf(pxmitpriv, pxmitbuf);

#ifdef CONFIG_SDIO_TX_TASKLET
	rtw_tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#endif

#endif
	return _FALSE;
}

/*
 * Description
 *	Transmit xmitbuf to hardware tx fifo
 *
 * Return
 *	_SUCCESS	ok
 *	_FAIL		something error
 */
#if 0 /*def CONFIG_XMIT_THREAD_MODE*/
s32 sdio_xmit_buf_handler(_adapter *adapter)
{
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	struct xmit_priv *pxmitpriv;
	u8 queue_empty, queue_pending;
	s32 ret;


	pxmitpriv = &adapter->xmitpriv;

	ret = _rtw_down_sema(&pxmitpriv->xmit_sema);
	if (_FAIL == ret) {
		RTW_ERR("%s: down SdioXmitBufSema fail!\n", __FUNCTION__);
		return _FAIL;
	}

	if (RTW_CANNOT_RUN(dvobj)) {
		RTW_DBG(FUNC_ADPT_FMT "- bDriverStopped(%s) bSurpriseRemoved(%s)\n",
			 FUNC_ADPT_ARG(adapter),
			 dev_is_drv_stopped(dvobj) ? "True" : "False",
			 dev_is_surprise_removed(dvobj) ? "True" : "False");
		return _FAIL;
	}

	if (rtw_mi_check_pending_xmitbuf(adapter) == 0)
		return _SUCCESS;
#ifdef CONFIG_LPS_LCLK
	ret = rtw_register_tx_alive(adapter);
	if (ret != _SUCCESS)
		return _SUCCESS;
#endif

	do {
		queue_empty = rtw_mi_sdio_dequeue_xmit(adapter);

	} while (!queue_empty);

#ifdef CONFIG_LPS_LCLK
	rtw_unregister_tx_alive(adapter);
#endif

	return _SUCCESS;
}
#endif
/*
 * Description:
 *	Enqueue xmitframe
 *
 * Return:
 *	_TRUE	enqueue ok
 *	_FALSE	fail
 */
s32 sdio_xmitframe_enqueue(_adapter *adapter, struct xmit_frame *pxmitframe)
{
	struct xmit_priv *pxmitpriv;
	s32 ret;


	pxmitpriv = &adapter->xmitpriv;

	ret = rtw_xmitframe_enqueue(adapter, pxmitframe);
	if (ret != _SUCCESS) {
		rtw_free_xmitframe(pxmitpriv, pxmitframe);
		pxmitpriv->tx_drop++;
		return _FALSE;
	}

#ifdef CONFIG_SDIO_TX_TASKLET
	rtw_tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
#else /* !CONFIG_SDIO_TX_TASKLET */
	_rtw_up_sema(&pxmitpriv->SdioXmitSema);
#endif /* !CONFIG_SDIO_TX_TASKLET */

	return _TRUE;
}
/******************************** recv section******************************/
static s32 sdio_recv_hdl(_adapter *adapter)
{
	return _SUCCESS;

}

s32 sdio_init_recv_priv(struct dvobj_priv *dvobj)
{
	s32 res = _SUCCESS;
	return res;
}

/*
 * Free recv private variable of hardware dependent
 * 1. recv buf
 * 2. recv tasklet
 */
void sdio_free_recv_priv(struct dvobj_priv *dvobj)
{

}


struct rtw_intf_ops sdio_ops = {
	.read		= rtw_sdio_raw_read,
	.write		= rtw_sdio_raw_write,

	/****************** data path *****************/

	/****************** xmit *********************/
	.init_xmit_priv = sdio_init_xmit_priv,
	.free_xmit_priv = sdio_free_xmit_priv,
	.data_xmit	= sdio_data_xmit,
	.xmitframe_enqueue = sdio_xmitframe_enqueue,
	.start_xmit_frame_thread	= sdio_start_xmit_frame_thread,
	.cancel_xmit_frame_thread	= sdio_cancel_xmit_frame_thread,
	#if 0 /*def CONFIG_XMIT_THREAD_MODE*/
	.xmit_buf_handler	= sdio_xmit_buf_handler,
	#endif

	/******************  recv *********************/
	.init_recv_priv = sdio_init_recv_priv,
	.free_recv_priv = sdio_free_recv_priv,
	#ifdef CONFIG_RECV_THREAD_MODE
	.recv_hdl = sdio_recv_hdl,
	#endif
};

