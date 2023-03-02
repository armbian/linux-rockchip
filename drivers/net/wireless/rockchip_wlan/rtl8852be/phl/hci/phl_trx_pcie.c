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
#define _PHL_TRX_PCIE_C_
#include "../phl_headers.h"
#include "phl_trx_pcie.h"

#define target_in_area(target, start, end) \
	((target < start || target > end) ? false : true)
void phl_recycle_payload(struct phl_info_t *phl_info, u8 dma_ch, u16 wp_seq,
			 u8 txsts);

void phl_dump_link_list(void *phl, _os_list *list_head, u8 type)
{
	struct rtw_wd_page *wd_page = NULL, *t = NULL;
	struct rtw_h2c_pkt *h2c_pkt = NULL, *h2c_t = NULL;
	struct rtw_phl_tring_list *phl_tring_list = NULL, *phl_t = NULL;
	struct phl_ring_status *ring_sts = NULL, *rsts_t = NULL;
	struct phl_info_t *phl_info = (struct phl_info_t *)phl;
	void *drv_priv = phl_to_drvpriv(phl_info);
	u8 *vir_addr = NULL;
	u32 i = 0, j = 0;
	u16 phl_idx = 0, phl_next_idx = 0;

	switch (type) {
	case TYPE_WD_PAGE:
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "===Dump WD Page===\n");
		phl_list_for_loop_safe(wd_page, t, struct rtw_wd_page,
						list_head, list) {
			vir_addr = (u8 *)wd_page->vir_addr;
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "vir_addr = %p, %x; phy_addr_l = %x; phy_addr_h = %x\n",
					vir_addr, *vir_addr,
					wd_page->phy_addr_l,
					wd_page->phy_addr_h);
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "cache = %d; buf_len = %d, wp_seq = %d\n",
					wd_page->cache, wd_page->buf_len,
					wd_page->wp_seq);
		}
		break;
	case TYPE_PHL_RING:
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "===Dump PHL Ring===\n");
		phl_list_for_loop_safe(phl_tring_list, phl_t,
					struct rtw_phl_tring_list,
					list_head, list) {

			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
				"-- macid = %d, band = %d, wmm = %d --\n",
					phl_tring_list->macid,
					phl_tring_list->band,
					phl_tring_list->wmm);

			for (i = 0; i < MAX_PHL_RING_CAT_NUM; i++) {
				phl_idx = (u16)_os_atomic_read(drv_priv,
						&phl_tring_list->phl_ring[i].phl_idx);
				phl_next_idx = (u16)_os_atomic_read(drv_priv,
						&phl_tring_list->phl_ring[i].phl_next_idx);

				PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
						"tid = %d\n"
						"dma_ch = %d\n"
						"tx_thres = %d\n"
						"core_idx = %d\n"
						"phl_idx = %d\n"
						"phl_next_idx = %d\n",
						phl_tring_list->phl_ring[i].tid,
						phl_tring_list->phl_ring[i].dma_ch,
						phl_tring_list->phl_ring[i].tx_thres,
						phl_tring_list->phl_ring[i].core_idx,
						phl_idx,
						phl_next_idx);

				for (j = 0; j < 5; j++) {
					PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
							"entry[%d] = %p\n",
							j,
					phl_tring_list->phl_ring[i].entry[j]);
				}
			}
		}
		break;
	case TYPE_RING_STS:
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "===Dump PHL Ring status===\n");
		phl_list_for_loop_safe(ring_sts, rsts_t, struct phl_ring_status,
					list_head, list) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
					"macid = %d\n"
					"req_busy = %d\n"
					"ring_ptr = %p\n",
					ring_sts->macid,
					ring_sts->req_busy,
					ring_sts->ring_ptr);
		}
		break;
	case TYPE_H2C_PKT:
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "===Dump H2C PKT===\n");
		phl_list_for_loop_safe(h2c_pkt, h2c_t, struct rtw_h2c_pkt,
					list_head, list) {
			vir_addr = (u8 *)h2c_pkt->vir_head;
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "vir_addr = %p, %x; phy_addr_l = %x; phy_addr_h = %x\n",
					vir_addr, *vir_addr,
					h2c_pkt->phy_addr_l,
					h2c_pkt->phy_addr_h);
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "type = %d; cache = %d; buf_len = %d\n",
					h2c_pkt->type, h2c_pkt->cache, h2c_pkt->buf_len);
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "vir_head = %p; vir_data = %p; vir_tail = %p; vir_end = %p\n",
					(u8 *)h2c_pkt->vir_head,
					(u8 *)h2c_pkt->vir_data,
					(u8 *)h2c_pkt->vir_tail,
					(u8 *)h2c_pkt->vir_end);
		}
		break;
	default :
		break;
	}
}

void _phl_dump_wp_stats(struct phl_info_t *phl_info)
{
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_wp_rpt_stats *rpt_stats = NULL;
	u8 ch = 0;

	rpt_stats = (struct rtw_wp_rpt_stats *)hal_com->trx_stat.wp_rpt_stats;

	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		  "\n== wp report statistics == \n");
	for (ch = 0; ch < hci_info->total_txch_num; ch++) {
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "ch			: %u\n", (int)ch);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "busy count		: %u\n",
			  (int)rpt_stats[ch].busy_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "ok count		: %u\n",
			  (int)rpt_stats[ch].tx_ok_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "retry fail count	: %u\n",
			  (int)rpt_stats[ch].rty_fail_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "lifetime drop count	: %u\n",
			  (int)rpt_stats[ch].lifetime_drop_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "macid drop count	: %u\n",
			  (int)rpt_stats[ch].macid_drop_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "sw drop count		: %u\n",
			  (int)rpt_stats[ch].sw_drop_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "recycle fail count	: %u\n",
			  (int)rpt_stats[ch].recycle_fail_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "delay ok count			: %u\n",
			  (int)rpt_stats[ch].delay_tx_ok_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "delay retry fail count		: %u\n",
			  (int)rpt_stats[ch].delay_rty_fail_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "delay lifetime drop count	: %u\n",
			  (int)rpt_stats[ch].delay_lifetime_drop_cnt);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "delay macid drop count		: %u\n",
			  (int)rpt_stats[ch].delay_macid_drop_cnt);

	}
}

void _phl_dump_busy_wp(struct phl_info_t *phl_info)
{
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_wd_page_ring *wd_ring = NULL;
	struct rtw_xmit_req *treq = NULL;
	void *ptr = NULL;
	u16 wp_seq = 0;
	u8 ch = 0;

	wd_ring = (struct rtw_wd_page_ring *)hci_info->wd_ring;
	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		  "\n== dump busy wp == \n");
	for (ch = 0; ch < hci_info->total_txch_num; ch++) {

		for (wp_seq = 0; wp_seq < WP_MAX_SEQ_NUMBER; wp_seq++) {
			if (NULL != wd_ring[ch].wp_tag[wp_seq].ptr) {
				ptr = wd_ring[ch].wp_tag[wp_seq].ptr;
				treq = (struct rtw_xmit_req *)ptr;
				PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
					  "dma_ch = %d, wp_seq = 0x%x, ptr = %p!\n",
					  ch, wp_seq, ptr);
				PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
					  "wifi seq = %d\n",
					  treq->mdata.sw_seq);
			}
		}
	}


}

u8 _phl_check_recycle(u16 target, u16 rptr, u16 wptr, u16 bndy)
{
	u8 recycle = false;
	u8 init = 0;	/* starting point */

	if (wptr > rptr) {
		if (true == target_in_area(target, wptr, (bndy-1)))
			recycle = true;
		else if (true == target_in_area(target, init, rptr))
			recycle = true;
		else
			recycle = false;

	} else if (rptr > wptr) {
		if (true == target_in_area(target, wptr, rptr))
			recycle = true;
		else
			recycle = false;
	} else {
		recycle = true;
	}

	return recycle;
}

void phl_tx_start_pcie(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	_os_atomic_set(drv, &phl_info->phl_sw_tx_sts, PHL_TX_STATUS_RUNNING);
}

void phl_tx_resume_pcie(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	_os_atomic_set(drv, &phl_info->phl_sw_tx_sts, PHL_TX_STATUS_RUNNING);
}

void phl_req_tx_stop_pcie(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	_os_atomic_set(drv, &phl_info->phl_sw_tx_sts,
				PHL_TX_STATUS_STOP_INPROGRESS);
}

void phl_tx_stop_pcie(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	_os_atomic_set(drv, &phl_info->phl_sw_tx_sts, PHL_TX_STATUS_SW_PAUSE);
}

bool phl_is_tx_sw_pause_pcie(struct phl_info_t *phl_info)
{
	void *drvpriv = phl_to_drvpriv(phl_info);

	if (PHL_TX_STATUS_SW_PAUSE == _os_atomic_read(drvpriv,
								&phl_info->phl_sw_tx_sts))
		return true;
	else
		return false;

}

void phl_rx_start_pcie(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	_os_atomic_set(drv, &phl_info->phl_sw_rx_sts, PHL_RX_STATUS_RUNNING);
}

void phl_rx_resume_pcie(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	_os_atomic_set(drv, &phl_info->phl_sw_rx_sts, PHL_RX_STATUS_RUNNING);
}

void phl_req_rx_stop_pcie(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	_os_atomic_set(drv, &phl_info->phl_sw_rx_sts,
				PHL_RX_STATUS_STOP_INPROGRESS);
}

void phl_rx_stop_pcie(struct phl_info_t *phl_info)
{
	void *drv = phl_to_drvpriv(phl_info);
	_os_atomic_set(drv, &phl_info->phl_sw_rx_sts, PHL_RX_STATUS_SW_PAUSE);
}

bool phl_is_rx_sw_pause_pcie(struct phl_info_t *phl_info)
{
	void *drvpriv = phl_to_drvpriv(phl_info);

	if (PHL_RX_STATUS_SW_PAUSE == _os_atomic_read(drvpriv,
								&phl_info->phl_sw_rx_sts)) {
		if (true == rtw_phl_is_phl_rx_idle(phl_info))
			return true;
		else
			return false;
	} else {
		return false;
	}
}

#ifdef RTW_WKARD_DYNAMIC_LTR
static bool _phl_judge_idle_ltr_switching_conditions(
	struct phl_info_t *phl_info, u16 macid)
{
	struct rtw_phl_stainfo_t *sta_info = NULL;
	struct rtw_stats *stats = &phl_info->phl_com->phl_stats;
	u16 ltr_thre = phl_info->phl_com->bus_sw_cap.ltr_sw_ctrl_thre;
	u8 tx_thre = 0, rx_thre = 0;
	u32 last_time = phl_ltr_get_last_trigger_time(phl_info->phl_com);

	tx_thre = ltr_thre >> 8;
	rx_thre = (u8)(ltr_thre & 0xFF);

	sta_info = rtw_phl_get_stainfo_by_macid(phl_info, macid);

	if (!rtw_hal_ltr_is_sw_ctrl(phl_info->phl_com, phl_info->hal))
		return false;

	if (sta_info == NULL)
		return false;

	if (sta_info->wrole == NULL)
		return false;

	if (stats->tx_traffic.lvl > tx_thre)
		return false;

	if (stats->rx_traffic.lvl > rx_thre)
		return false;

	if (RTW_PCIE_LTR_SW_IDLE == phl_ltr_get_cur_state(phl_info->phl_com))
		return false;

	if (phl_get_passing_time_us(last_time) < 500)
		return false;

	return true;

}
static bool _phl_judge_act_ltr_switching_conditions(
	struct phl_info_t *phl_info, u8 ch)
{
	u32 last_time = phl_ltr_get_last_trigger_time(phl_info->phl_com);
	u8 fwcmd_queue_idx = 0;

	fwcmd_queue_idx = rtw_hal_get_fwcmd_queue_idx(phl_info->hal);

	if (!rtw_hal_ltr_is_sw_ctrl(phl_info->phl_com, phl_info->hal))
		return true;

	if (ch == fwcmd_queue_idx)
		return true;

	if (RTW_PCIE_LTR_SW_ACT == phl_ltr_get_cur_state(phl_info->phl_com))
		return true;

	if (phl_get_passing_time_us(last_time) < 500)
		return false;

	return true;
}

static void _phl_act_ltr_update_stats(struct phl_info_t *phl_info,
		bool success, u8 ch, u16 pending_wd_page_cnt)
{
	static bool bdly = false;
	static u32 dly_start_time = 0;
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};

	if (!rtw_hal_ltr_is_sw_ctrl(phl_info->phl_com, phl_info->hal))
		return;

	if (success) {
		/* only those have been delayed last time*/
		if (bdly) {
			PHL_INFO("%s() ch(%u), %u packets be transmitted after defering %uus\n"
				, __func__, ch,	pending_wd_page_cnt,
				phl_get_passing_time_us(dly_start_time));
			rtw_hal_ltr_update_stats(phl_info->hal, true);
		}
		bdly = false;
	} else {

		/* the first packet that is going to defer */
		if (false == bdly) {
			dly_start_time = _os_get_cur_time_us();
			/* send messages to trigger tx */
			msg.band_idx = HW_BAND_0;
			SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_MDL_TX);
			SET_MSG_EVT_ID_FIELD(msg.msg_id, MSG_EVT_LTR_TX_DLY);
			phl_msg_hub_send(phl_info, &attr, &msg);
		}

		PHL_DBG("%s() ch(%u), %u packets be delayed\n", __func__,
							ch,	pending_wd_page_cnt);

		rtw_hal_ltr_update_stats(phl_info->hal, false);
		bdly = true;
	}
}

static void _phl_switch_act_ltr(struct phl_info_t *phl_info, u8 tx_dma_ch)
{
	u8 fwcmd_queue_idx = 0;

	if (!rtw_hal_ltr_is_sw_ctrl(phl_info->phl_com, phl_info->hal))
		return;

	if (RTW_PCIE_LTR_SW_ACT == phl_ltr_get_cur_state(phl_info->phl_com))
		return;

	fwcmd_queue_idx = rtw_hal_get_fwcmd_queue_idx(phl_info->hal);

	if (tx_dma_ch != fwcmd_queue_idx)
		phl_ltr_sw_trigger(phl_info->phl_com, phl_info->hal,
			RTW_PCIE_LTR_SW_ACT);

}

static void _phl_switch_idle_ltr(struct phl_info_t *phl_info,
					struct rtw_wp_rpt_stats *rpt_stats)
{
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	u8 ch = 0;
	bool bempty = 1;
	u8 fwcmd_queue_idx = 0;

	fwcmd_queue_idx = rtw_hal_get_fwcmd_queue_idx(phl_info->hal);

	for (ch = 0; ch < hci_info->total_txch_num; ch++) {
		if (ch == fwcmd_queue_idx)
			continue;
		if (rpt_stats[ch].busy_cnt != 0)
			bempty = 0;
	}

	if (bempty)
		phl_ltr_sw_trigger(phl_info->phl_com, phl_info->hal,
			RTW_PCIE_LTR_SW_IDLE);

}
#endif

#ifdef RTW_WKARD_TXBD_UPD_LMT
static void
_phl_free_h2c_work_ring(struct phl_info_t *phl_info,
			struct rtw_wd_page_ring *wd_page_ring)
{
	void *drv_priv = phl_to_drvpriv(phl_info);
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_info->phl_com);
	struct rtw_h2c_work *h2c_work = &wd_page_ring->h2c_work;
	struct rtw_h2c_pkt *cmd = h2c_work->cmd;
	struct rtw_h2c_pkt *data = h2c_work->data;
	struct rtw_h2c_pkt *ldata = h2c_work->ldata;
	struct phl_hci_trx_ops *hci_trx_ops = phl_info->hci_trx_ops;
	u16 i = 0, buf_num = 0;

	buf_num = hal_spec->txbd_multi_tag;

	if (NULL != cmd) {
		for (i = 0; i < buf_num; i++) {
			if (NULL == cmd->vir_head)
				continue;
			hci_trx_ops->free_h2c_pkt_buf(phl_info, cmd);
			cmd->vir_head = NULL;
			cmd->cache = false;
			cmd++;
		}
		_os_mem_free(drv_priv, h2c_work->cmd,
			     buf_num * sizeof(*h2c_work->cmd));
	}
	if (NULL != data) {
		for (i = 0; i < buf_num; i++) {
			if (NULL == data->vir_head)
				continue;
			hci_trx_ops->free_h2c_pkt_buf(phl_info, data);
			data->vir_head = NULL;
			data->cache = false;
			data++;
		}
		_os_mem_free(drv_priv, h2c_work->data,
			     buf_num * sizeof(*h2c_work->data));
	}
	if (NULL != ldata) {
		for (i = 0; i < buf_num; i++) {
			if (NULL == ldata->vir_head)
				continue;
			hci_trx_ops->free_h2c_pkt_buf(phl_info, ldata);
			ldata->vir_head = NULL;
			ldata->cache = false;
			ldata++;
		}
		_os_mem_free(drv_priv, h2c_work->ldata,
			     buf_num * sizeof(*h2c_work->ldata));
	}

	if (NULL != h2c_work->cmd_ring) {
		_os_mem_free(drv_priv, h2c_work->cmd_ring,
			     buf_num * sizeof(struct rtw_h2c_pkt *));
        }
	if (NULL != h2c_work->data_ring) {
		_os_mem_free(drv_priv, h2c_work->data_ring,
			     buf_num * sizeof(struct rtw_h2c_pkt *));
        }
	if (NULL != h2c_work->ldata_ring) {
		_os_mem_free(drv_priv, h2c_work->ldata_ring,
			     buf_num * sizeof(struct rtw_h2c_pkt *));
        }
	h2c_work->cmd_cnt = 0;
	h2c_work->cmd_idx = 0;
	h2c_work->data_cnt = 0;
	h2c_work->data_idx = 0;
	h2c_work->ldata_cnt = 0;
	h2c_work->ldata_idx = 0;
	_os_spinlock_free(drv_priv,	&h2c_work->lock);
}


static enum rtw_phl_status
_phl_alloc_h2c_work_ring(struct phl_info_t *phl_info,
			 struct rtw_wd_page_ring *wd_page_ring)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	void *drv_priv = phl_to_drvpriv(phl_info);
	struct phl_hci_trx_ops *hci_trx_ops = phl_info->hci_trx_ops;
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_info->phl_com);
	struct rtw_h2c_work *h2c_work = &wd_page_ring->h2c_work;
	struct rtw_h2c_pkt *cmd = NULL, *data =  NULL, *ldata = NULL;
	u16 buf_num = 0, i = 0;

	buf_num = hal_spec->txbd_multi_tag;
	_os_spinlock_init(drv_priv, &h2c_work->lock);

	h2c_work->cmd = _os_mem_alloc(drv_priv, buf_num * sizeof(*cmd));
	h2c_work->data = _os_mem_alloc(drv_priv, buf_num * sizeof(*data));
	h2c_work->ldata = _os_mem_alloc(drv_priv, buf_num * sizeof(*ldata));

	if (!h2c_work->cmd || !h2c_work->data || !h2c_work->ldata) {
		psts = RTW_PHL_STATUS_RESOURCE;
		goto out;
	}
	cmd = h2c_work->cmd;
	data = h2c_work->data;
	ldata = h2c_work->ldata;

	_os_mem_set(drv_priv, cmd, 0, buf_num * sizeof(*cmd));
	_os_mem_set(drv_priv, data, 0, buf_num * sizeof(*data));
	_os_mem_set(drv_priv, ldata, 0, buf_num * sizeof(*ldata));

	h2c_work->cmd_ring =
		_os_mem_alloc(drv_priv,
			       buf_num * sizeof(struct rtw_h2c_pkt *));
	h2c_work->data_ring =
		_os_mem_alloc(drv_priv,
			       buf_num * sizeof(struct rtw_h2c_pkt *));
	h2c_work->ldata_ring =
		_os_mem_alloc(drv_priv,
			       buf_num * sizeof(struct rtw_h2c_pkt *));

	if (!h2c_work->cmd_ring || !h2c_work->data_ring ||
	    !h2c_work->ldata_ring) {
		psts = RTW_PHL_STATUS_RESOURCE;
		goto out;
	}
	_os_mem_set(drv_priv, h2c_work->cmd_ring, 0,
		    buf_num * sizeof(struct rtw_h2c_pkt *));
	_os_mem_set(drv_priv, h2c_work->data_ring, 0,
		    buf_num * sizeof(struct rtw_h2c_pkt *));
	_os_mem_set(drv_priv, h2c_work->ldata_ring, 0,
		    buf_num * sizeof(struct rtw_h2c_pkt *));

	for (i = 0; i < buf_num; i++) {
		cmd->type = H2CB_TYPE_CMD;
		cmd->cache = false;
		cmd->buf_len = FWCMD_HDR_LEN + _WD_BODY_LEN + H2C_CMD_LEN;
		hci_trx_ops->alloc_h2c_pkt_buf(phl_info, cmd, cmd->buf_len);
		if (NULL == cmd->vir_head) {
			psts = RTW_PHL_STATUS_RESOURCE;
			goto out;
		}
		cmd->vir_data = cmd->vir_head + FWCMD_HDR_LEN + _WD_BODY_LEN;
		cmd->vir_tail = cmd->vir_data;
		cmd->vir_end = cmd->vir_data + H2C_CMD_LEN;
		INIT_LIST_HEAD(&cmd->list);
		h2c_work->cmd_ring[i] = cmd;
		h2c_work->cmd_cnt++;
		cmd++;
	}
	for (i = 0; i < buf_num; i++) {
		data->type = H2CB_TYPE_DATA;
		data->cache = false;
		data->buf_len = FWCMD_HDR_LEN + _WD_BODY_LEN + H2C_DATA_LEN;
		hci_trx_ops->alloc_h2c_pkt_buf(phl_info, data, data->buf_len);
		if (NULL == data->vir_head) {
			psts = RTW_PHL_STATUS_RESOURCE;
			goto out;
		}
		data->vir_data = data->vir_head + FWCMD_HDR_LEN + _WD_BODY_LEN;
		data->vir_tail = data->vir_data;
		data->vir_end = data->vir_data + H2C_DATA_LEN;
		INIT_LIST_HEAD(&data->list);
		h2c_work->data_ring[i] = data;
		h2c_work->data_cnt++;
		data++;
	}
	for (i = 0; i < buf_num; i++) {
		ldata->type = H2CB_TYPE_LONG_DATA;
		ldata->cache = false;
		ldata->buf_len = FWCMD_HDR_LEN + _WD_BODY_LEN +
				 H2C_LONG_DATA_LEN;
		hci_trx_ops->alloc_h2c_pkt_buf(phl_info, ldata, ldata->buf_len);
		if (NULL == ldata->vir_head) {
			psts = RTW_PHL_STATUS_RESOURCE;
			goto out;
		}
		ldata->vir_data = ldata->vir_head + FWCMD_HDR_LEN +
				 _WD_BODY_LEN;
		ldata->vir_tail = ldata->vir_data;
		ldata->vir_end = ldata->vir_data + H2C_LONG_DATA_LEN;
		INIT_LIST_HEAD(&ldata->list);
		h2c_work->ldata_ring[i] = ldata;
		h2c_work->ldata_cnt++;
		ldata++;
	}

	h2c_work->cmd_idx = 0;
	h2c_work->data_idx = 0;
	h2c_work->ldata_idx = 0;
	psts = RTW_PHL_STATUS_SUCCESS;

out:
	if (RTW_PHL_STATUS_SUCCESS != psts) {
		_phl_free_h2c_work_ring(phl_info, wd_page_ring);
		h2c_work->cmd = NULL;
		h2c_work->data = NULL;
		h2c_work->ldata = NULL;
		h2c_work->cmd_ring = NULL;
		h2c_work->data_ring = NULL;
		h2c_work->ldata_ring = NULL;
	}

	return psts;
}


static void
_phl_free_wd_work_ring(struct phl_info_t *phl_info,
		       struct rtw_wd_page_ring *wd_page_ring)
{
	void *drv_priv = phl_to_drvpriv(phl_info);
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_info->phl_com);
	u16 i = 0, buf_num = 0;

	buf_num = hal_spec->txbd_multi_tag;

	if (NULL != wd_page_ring->wd_work) {
		for (i = 0; i < buf_num; i++) {

			if (NULL == wd_page_ring->wd_work[i].vir_addr)
				continue;

			wd_page_ring->wd_work[i].wp_seq = WP_RESERVED_SEQ;
			_os_shmem_free(drv_priv, phl_info->hci->wd_dma_pool,
			       	wd_page_ring->wd_work[i].vir_addr,
			       	(_dma *)&wd_page_ring->wd_work[i].phy_addr_l,
			       	(_dma *)&wd_page_ring->wd_work[i].phy_addr_h,
			       	wd_page_ring->wd_work[i].buf_len,
				wd_page_ring->wd_work[i].cache,
				PCI_DMA_FROMDEVICE,
				wd_page_ring->wd_work[i].os_rsvd[0]);
			wd_page_ring->wd_work[i].vir_addr = NULL;
			wd_page_ring->wd_work[i].cache = 0;
		}

		_os_mem_free(drv_priv, wd_page_ring->wd_work,
			      buf_num * sizeof(*wd_page_ring->wd_work));
		wd_page_ring->wd_work = NULL;
	}

	if (NULL != wd_page_ring->wd_work_ring) {
		_os_mem_free(drv_priv, wd_page_ring->wd_work_ring,
			      buf_num * sizeof(struct rtw_wd_page *));
		wd_page_ring->wd_work_ring = NULL;
        }
	wd_page_ring->wd_work_cnt = 0;
	wd_page_ring->wd_work_idx = 0;

}

static enum rtw_phl_status
_phl_alloc_wd_work_ring(struct phl_info_t *phl_info,
			struct rtw_wd_page_ring *wd_page_ring)
{
	enum rtw_phl_status psts = RTW_PHL_STATUS_FAILURE;
	void *drv_priv = phl_to_drvpriv(phl_info);
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_info->phl_com);
	struct rtw_wd_page *wd_work = NULL;
	void *dma_pool = NULL;
	u32 buf_len = 0;
	u16 buf_num = 0, i = 0;

	buf_num = hal_spec->txbd_multi_tag;

	wd_page_ring->wd_work = _os_mem_alloc(drv_priv,
					       buf_num * sizeof(*wd_work));
	if (!wd_page_ring->wd_work) {
		psts = RTW_PHL_STATUS_RESOURCE;
		goto out;
	}
	wd_work = wd_page_ring->wd_work;
	_os_mem_set(drv_priv, wd_work, 0, buf_num * sizeof(*wd_work));

	wd_page_ring->wd_work_ring =
		_os_mem_alloc(drv_priv,
			       buf_num * sizeof(struct rtw_wd_page *));
	if (!wd_page_ring->wd_work_ring) {
		psts = RTW_PHL_STATUS_RESOURCE;
		goto out;
	}
	_os_mem_set(drv_priv, wd_page_ring->wd_work_ring, 0,
		    buf_num * sizeof(struct rtw_wd_page *));

	for (i = 0; i < buf_num; i++) {
#ifdef CONFIG_DMA_TX_USE_COHERENT_MEM
		wd_work[i].cache = POOL_ADDR;
		dma_pool = phl_info->hci->wd_dma_pool;
#else
		wd_work[i].cache = VIRTUAL_ADDR;
#endif
		buf_len = WD_PAGE_SIZE;
		wd_work[i].vir_addr = _os_shmem_alloc(drv_priv, dma_pool,
					(_dma *)&wd_work[i].phy_addr_l,
					(_dma *)&wd_work[i].phy_addr_h,
					buf_len,
					wd_work[i].cache,
					PCI_DMA_TODEVICE,
					&wd_work[i].os_rsvd[0]);
 		if (NULL == wd_work[i].vir_addr) {
			psts = RTW_PHL_STATUS_RESOURCE;
			goto out;
		}
		wd_work[i].buf_len = buf_len;
		wd_work[i].wp_seq = WP_RESERVED_SEQ;
		INIT_LIST_HEAD(&wd_work[i].list);

		wd_page_ring->wd_work_ring[i] = &wd_work[i];
		wd_page_ring->wd_work_cnt++;
		/* hana_todo now check 4 byte align only */
		/* if ((unsigned long)wd_page_buf & 0xF) { */
		/* 	res = _FAIL; */
		/* 	break; */
		/* } */
	}

	wd_page_ring->wd_work_idx = 0;
	psts = RTW_PHL_STATUS_SUCCESS;

out:
	if (RTW_PHL_STATUS_SUCCESS != psts) {
		_phl_free_wd_work_ring(phl_info, wd_page_ring);
		wd_page_ring->wd_work = NULL;
		wd_page_ring->wd_work_ring = NULL;
	}

	return psts;
}
#else
#define _phl_free_h2c_work_ring(_phl, _ring)
#define _phl_alloc_h2c_work_ring(_phl, _ring) RTW_PHL_STATUS_SUCCESS
#define _phl_free_wd_work_ring(_phl, _ring)
#define _phl_alloc_wd_work_ring(_phl, _ring) RTW_PHL_STATUS_SUCCESS
#endif

static enum rtw_phl_status enqueue_pending_wd_page(struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_page_ring,
				struct rtw_wd_page *wd_page, u8 pos)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	void *drv_priv = phl_to_drvpriv(phl_info);
	_os_list *list = &wd_page_ring->pending_wd_page_list;

	if (wd_page != NULL) {
		_os_spinlock(drv_priv, &wd_page_ring->pending_lock, _bh, NULL);

		if (_tail == pos)
			list_add_tail(&wd_page->list, list);
		else if (_first == pos)
			list_add(&wd_page->list, list);

		wd_page_ring->pending_wd_page_cnt++;

		_os_spinunlock(drv_priv, &wd_page_ring->pending_lock, _bh, NULL);

		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}

static enum rtw_phl_status enqueue_busy_wd_page(struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_page_ring,
				struct rtw_wd_page *wd_page, u8 pos)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	void *drv_priv = phl_to_drvpriv(phl_info);
	_os_list *list = &wd_page_ring->busy_wd_page_list;

	if (wd_page != NULL) {
		_os_spinlock(drv_priv, &wd_page_ring->busy_lock, _bh, NULL);

		if (_tail == pos)
			list_add_tail(&wd_page->list, list);
		else if (_first == pos)
			list_add(&wd_page->list, list);

		wd_page_ring->busy_wd_page_cnt++;

		_os_spinunlock(drv_priv, &wd_page_ring->busy_lock, _bh, NULL);

		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}


static enum rtw_phl_status enqueue_idle_wd_page(
				struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_page_ring,
				struct rtw_wd_page *wd_page)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	void *drv_priv = phl_to_drvpriv(phl_info);
	_os_list *list = &wd_page_ring->idle_wd_page_list;

	if (wd_page != NULL) {
		_os_mem_set(phl_to_drvpriv(phl_info), wd_page->vir_addr, 0,
					WD_PAGE_SIZE);
		wd_page->buf_len = WD_PAGE_SIZE;
		wd_page->wp_seq = WP_RESERVED_SEQ;
		wd_page->host_idx = 0;
		INIT_LIST_HEAD(&wd_page->list);

		_os_spinlock(drv_priv, &wd_page_ring->idle_lock, _bh, NULL);

		list_add_tail(&wd_page->list, list);
		wd_page_ring->idle_wd_page_cnt++;

		_os_spinunlock(drv_priv, &wd_page_ring->idle_lock, _bh, NULL);

		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}

#ifdef RTW_WKARD_TXBD_UPD_LMT
static enum rtw_phl_status enqueue_h2c_work_ring(
				struct phl_info_t *phl_info,
				struct rtw_h2c_pkt *h2c)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	void *drv_priv = phl_to_drvpriv(phl_info);
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_wd_page_ring *wd_ring = NULL;
	struct rtw_h2c_work *h2c_work = NULL;
	struct rtw_h2c_pkt *work_done_h2c = NULL;
	struct rtw_h2c_pkt **ring = NULL;
	u16 *idx = 0, *cnt = 0;
	u8 fwcmd_qidx = 0;

	fwcmd_qidx = rtw_hal_get_fwcmd_queue_idx(phl_info->hal);
	wd_ring = (struct rtw_wd_page_ring *)hci_info->wd_ring;
	h2c_work = &wd_ring[fwcmd_qidx].h2c_work;

	if (h2c == NULL)
		goto out;

	_os_spinlock(drv_priv, &h2c_work->lock, _bh, NULL);

	if (H2CB_TYPE_CMD == h2c->type) {
		ring = h2c_work->cmd_ring;
		idx = &h2c_work->cmd_idx;
		cnt = &h2c_work->cmd_cnt;
	} else if (H2CB_TYPE_DATA == h2c->type) {
		ring = h2c_work->data_ring;
		idx = &h2c_work->data_idx;
		cnt = &h2c_work->data_cnt;
	} else if (H2CB_TYPE_LONG_DATA == h2c->type) {
		ring = h2c_work->ldata_ring;
		idx = &h2c_work->ldata_idx;
		cnt = &h2c_work->ldata_cnt;
	} else {
		_os_spinunlock(drv_priv, &h2c_work->lock, _bh, NULL);
		goto out;
	}

	work_done_h2c = ring[*idx];
	ring[*idx] = h2c;
	*idx = (*idx + 1) % *cnt;

	_os_spinunlock(drv_priv, &h2c_work->lock, _bh, NULL);

	pstatus = phl_enqueue_idle_h2c_pkt(phl_info, work_done_h2c);

out:
	return pstatus;
}

static enum rtw_phl_status enqueue_wd_work_ring(
				struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_page_ring,
				struct rtw_wd_page *wd_page)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	void *drv_priv = phl_to_drvpriv(phl_info);
	struct rtw_wd_page *work_done_wd = NULL;
	struct rtw_wd_page **ring = wd_page_ring->wd_work_ring;

	if (wd_page != NULL) {

		_os_spinlock(drv_priv, &wd_page_ring->work_lock, _bh, NULL);

		work_done_wd = ring[wd_page_ring->wd_work_idx];
		ring[wd_page_ring->wd_work_idx] = wd_page;
		wd_page_ring->wd_work_idx =
		    (wd_page_ring->wd_work_idx + 1) % wd_page_ring->wd_work_cnt;

		_os_spinunlock(drv_priv, &wd_page_ring->work_lock, _bh, NULL);

		enqueue_idle_wd_page(phl_info, wd_page_ring, work_done_wd);

		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}
#else
#define enqueue_h2c_work_ring(_phl, _h2c) RTW_PHL_STATUS_FAILURE
#define enqueue_wd_work_ring(_phl, _ring, _wd) RTW_PHL_STATUS_FAILURE
#endif


static struct rtw_wd_page *query_pending_wd_page(struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_page_ring)
{
	void *drv_priv = phl_to_drvpriv(phl_info);
	_os_list *pending_list = &wd_page_ring->pending_wd_page_list;
	struct rtw_wd_page *wd_page = NULL;

	_os_spinlock(drv_priv, &wd_page_ring->pending_lock, _bh, NULL);

	if (true == list_empty(pending_list)) {
		wd_page = NULL;
	} else {
		wd_page = list_first_entry(pending_list, struct rtw_wd_page,
						list);
		wd_page_ring->pending_wd_page_cnt--;
		list_del(&wd_page->list);
	}

	_os_spinunlock(drv_priv, &wd_page_ring->pending_lock, _bh, NULL);

	return wd_page;
}


static struct rtw_wd_page *query_idle_wd_page(struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_page_ring)
{
	void *drv_priv = phl_to_drvpriv(phl_info);
	_os_list *idle_list = &wd_page_ring->idle_wd_page_list;
	struct rtw_wd_page *wd_page = NULL;

	_os_spinlock(drv_priv, &wd_page_ring->idle_lock, _bh, NULL);

	if (true == list_empty(idle_list)) {
		wd_page = NULL;
	} else {
		wd_page = list_first_entry(idle_list, struct rtw_wd_page, list);
		wd_page_ring->idle_wd_page_cnt--;
		list_del(&wd_page->list);
	}

	_os_spinunlock(drv_priv, &wd_page_ring->idle_lock, _bh, NULL);

	return wd_page;
}

static enum rtw_phl_status rtw_release_target_wd_page(
					struct phl_info_t *phl_info,
					struct rtw_wd_page_ring *wd_page_ring,
					struct rtw_wd_page *wd_page)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	if (wd_page_ring != NULL && wd_page != NULL) {
		enqueue_idle_wd_page(phl_info, wd_page_ring, wd_page);
		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}

static enum rtw_phl_status rtw_release_pending_wd_page(
				struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_page_ring,
				u16 release_num)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	_os_list *list = &wd_page_ring->pending_wd_page_list;
	struct rtw_wd_page *wd_page = NULL;

	if (wd_page_ring != NULL) {
		while (release_num > 0 && true != list_empty(list)) {

			wd_page = query_pending_wd_page(phl_info, wd_page_ring);

			enqueue_idle_wd_page(phl_info, wd_page_ring, wd_page);

			release_num--;
		}
		pstatus = RTW_PHL_STATUS_SUCCESS;
	}
	return pstatus;
}

static enum rtw_phl_status rtw_release_busy_wd_page(
				struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_page_ring,
				u16 release_num)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	_os_list *list = &wd_page_ring->busy_wd_page_list;
	struct rtw_wd_page *wd_page = NULL;
	void *drv_priv = phl_to_drvpriv(phl_info);
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_info->phl_com);

	if (wd_page_ring != NULL) {
		_os_spinlock(drv_priv, &wd_page_ring->busy_lock, _bh, NULL);

		while (release_num > 0 && true != list_empty(list)) {

			wd_page = list_first_entry(list, struct rtw_wd_page,
							list);
			wd_page_ring->busy_wd_page_cnt--;
			list_del(&wd_page->list);

			_os_spinunlock(drv_priv, &wd_page_ring->busy_lock, _bh, NULL);
			if (true == hal_spec->txbd_upd_lmt) {
				pstatus = enqueue_wd_work_ring(phl_info,
							       wd_page_ring,
							       wd_page);
			} else {
				pstatus = enqueue_idle_wd_page(phl_info,
							       wd_page_ring,
							       wd_page);
			}
			_os_spinlock(drv_priv, &wd_page_ring->busy_lock, _bh, NULL);

			if (RTW_PHL_STATUS_SUCCESS != pstatus)
				break;
			release_num--;
		}
		_os_spinunlock(drv_priv, &wd_page_ring->busy_lock, _bh, NULL);

	}
	return pstatus;
}

static void _phl_reset_txbd(struct phl_info_t *phl_info,
				struct tx_base_desc *txbd)
{
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	_os_mem_set(phl_to_drvpriv(phl_info), txbd->vir_addr, 0, txbd->buf_len);
	txbd->host_idx = 0;
	txbd->avail_num = (u16)hal_com->bus_cap.txbd_num;
}
static void _phl_reset_wp_tag(struct phl_info_t *phl_info,
			struct rtw_wd_page_ring *wd_page_ring, u8 dma_ch)
{
	u16 wp_seq = 0;

	for (wp_seq = 0; wp_seq < WP_MAX_SEQ_NUMBER; wp_seq++) {
		if (NULL != wd_page_ring->wp_tag[wp_seq].ptr)
			phl_recycle_payload(phl_info, dma_ch, wp_seq,
					    TX_STATUS_TX_FAIL_SW_DROP);
	}
}


static enum rtw_phl_status enqueue_pending_h2c_pkt(struct phl_info_t *phl_info,
				struct phl_h2c_pkt_pool *h2c_pkt_pool,
				struct rtw_h2c_pkt *h2c_pkt, u8 pos)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
#if 0
	void *drv_priv = phl_to_drvpriv(phl_info);
	_os_list *list = &wd_page_ring->pending_wd_page_list;

	if (wd_page != NULL) {
		_os_spinlock(drv_priv, &wd_page_ring->pending_lock, _bh, NULL);

		if (_tail == pos)
			list_add_tail(&wd_page->list, list);
		else if (_first == pos)
			list_add(&wd_page->list, list);

		wd_page_ring->pending_wd_page_cnt++;

		_os_spinunlock(drv_priv, &wd_page_ring->pending_lock, _bh, NULL);

		pstatus = RTW_PHL_STATUS_SUCCESS;
	}
#endif
	return pstatus;
}

static struct rtw_h2c_pkt *query_pending_h2c_pkt(struct phl_info_t *phl_info,
				struct phl_h2c_pkt_pool *h2c_pkt_pool)
{
	//void *drv_priv = phl_to_drvpriv(phl_info);
	//_os_list *pending_list = &wd_page_ring->pending_wd_page_list;
	struct rtw_h2c_pkt *h2c_pkt = NULL;
#if 0
	_os_spinlock(drv_priv, &wd_page_ring->pending_lock, _bh, NULL);

	if (true == list_empty(pending_list)) {
		wd_page = NULL;
	} else {
		wd_page = list_first_entry(pending_list, struct rtw_wd_page,
						list);
		wd_page_ring->pending_wd_page_cnt--;
		list_del(&wd_page->list);
	}

	_os_spinunlock(drv_priv, &wd_page_ring->pending_lock, _bh, NULL);
#endif
	return h2c_pkt;
}

static enum rtw_phl_status phl_release_busy_h2c_pkt(
				struct phl_info_t *phl_info,
				struct phl_h2c_pkt_pool *h2c_pkt_pool,
				u16 release_num)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	_os_list *list = &h2c_pkt_pool->busy_h2c_pkt_list.queue;
	struct rtw_h2c_pkt *h2c_pkt = NULL;
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_info->phl_com);

	if (h2c_pkt_pool != NULL) {

		while (release_num > 0 && true != list_empty(list)) {
			h2c_pkt = phl_query_busy_h2c_pkt(phl_info);

			if (!h2c_pkt)
				break;

			if (true == hal_spec->txbd_upd_lmt) {
				pstatus = enqueue_h2c_work_ring(phl_info,
								h2c_pkt);
			} else {
				pstatus = phl_enqueue_idle_h2c_pkt(phl_info,
								   h2c_pkt);
			}

			if (RTW_PHL_STATUS_SUCCESS != pstatus)
				break;
			release_num--;
		}
	}
	return pstatus;
}

static void phl_tx_reset_pcie(struct phl_info_t *phl_info)
{
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_wd_page_ring *wd_ring = NULL;
	struct tx_base_desc *txbd = NULL;
	struct phl_h2c_pkt_pool *h2c_pool = NULL;
	u8 ch = 0;

	txbd = (struct tx_base_desc *)hci_info->txbd_buf;
	wd_ring = (struct rtw_wd_page_ring *)hci_info->wd_ring;
	h2c_pool = (struct phl_h2c_pkt_pool *)phl_info->h2c_pool;

	for (ch = 0; ch < hci_info->total_txch_num; ch++) {
		_phl_reset_txbd(phl_info, &txbd[ch]);
		rtw_release_busy_wd_page(phl_info, &wd_ring[ch],
					 wd_ring[ch].busy_wd_page_cnt);
		rtw_release_pending_wd_page(phl_info, &wd_ring[ch],
					 wd_ring[ch].pending_wd_page_cnt);
		wd_ring[ch].cur_hw_res = 0;
		_phl_reset_wp_tag(phl_info, &wd_ring[ch], ch);
	}

	phl_release_busy_h2c_pkt(phl_info, h2c_pool,
				 (u16)h2c_pool->busy_h2c_pkt_list.cnt);

	phl_dump_h2c_pool_stats(phl_info->h2c_pool);
}



#ifdef CONFIG_DYNAMIC_RX_BUF
void _phl_alloc_dynamic_rxbuf_pcie(struct rtw_rx_buf *rx_buf,
					struct phl_info_t *phl_info)
{
	enum rtw_phl_status sts = RTW_PHL_STATUS_FAILURE;
	void *drv_priv = phl_to_drvpriv(phl_info);
	u32 buf_len = RX_BUF_SIZE;

	if (rx_buf->reuse) {
		rx_buf->reuse = false;
		_os_pkt_buf_map_rx(drv_priv,
				(_dma *)&rx_buf->phy_addr_l,
				(_dma *)&rx_buf->phy_addr_h,
				RX_BUF_SIZE + PHL_RX_HEADROOM,
				rx_buf->os_priv);
		return;
	}

	if (rx_buf != NULL) {
#ifdef CONFIG_DMA_RX_USE_COHERENT_MEM
		rx_buf->cache = false;
#else
		rx_buf->cache = VIRTUAL_ADDR;
#endif
		rx_buf->vir_addr = _os_pkt_buf_alloc_rx(
				drv_priv,
				(_dma *)&rx_buf->phy_addr_l,
				(_dma *)&rx_buf->phy_addr_h,
				buf_len,
				rx_buf->cache,
				&rx_buf->os_priv);
		if (NULL == rx_buf->vir_addr) {
			sts = RTW_PHL_STATUS_RESOURCE;
		} else {
			rx_buf->buf_len = buf_len;
			rx_buf->dynamic = 1;
			rx_buf->reuse = false;
			/* enqueue_idle_rx_buf(phl_info, rx_buf_ring, rx_buf); */
			sts = RTW_PHL_STATUS_SUCCESS;
		}
	}
}
#endif


static enum rtw_phl_status enqueue_busy_rx_buf(
				struct phl_info_t *phl_info,
				struct rtw_rx_buf_ring *rx_buf_ring,
				struct rtw_rx_buf *rx_buf, u8 pos)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	_os_list *list = &rx_buf_ring->busy_rxbuf_list;

	if (rx_buf != NULL) {
		_os_spinlock(phl_to_drvpriv(phl_info),
				&rx_buf_ring->busy_rxbuf_lock, _bh, NULL);
		if (_tail == pos)
			list_add_tail(&rx_buf->list, list);
		else if (_first == pos)
			list_add(&rx_buf->list, list);

		rx_buf_ring->busy_rxbuf_cnt++;
		_os_spinunlock(phl_to_drvpriv(phl_info),
				&rx_buf_ring->busy_rxbuf_lock, _bh, NULL);
		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}


static enum rtw_phl_status enqueue_idle_rx_buf(
				struct phl_info_t *phl_info,
				struct rtw_rx_buf_ring *rx_buf_ring,
				struct rtw_rx_buf *rx_buf)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	_os_list *list = &rx_buf_ring->idle_rxbuf_list;

	if (rx_buf != NULL) {
		_os_mem_set(phl_to_drvpriv(phl_info), rx_buf->vir_addr, 0,
					RX_BUF_SIZE);
		rx_buf->buf_len = RX_BUF_SIZE;
		INIT_LIST_HEAD(&rx_buf->list);

		_os_spinlock(phl_to_drvpriv(phl_info),
					&rx_buf_ring->idle_rxbuf_lock, _bh, NULL);
		list_add_tail(&rx_buf->list, list);
		rx_buf_ring->idle_rxbuf_cnt++;
		_os_spinunlock(phl_to_drvpriv(phl_info),
					&rx_buf_ring->idle_rxbuf_lock, _bh, NULL);

		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}

static struct rtw_rx_buf *query_busy_rx_buf(struct phl_info_t *phl_info,
					struct rtw_rx_buf_ring *rx_buf_ring)
{
	_os_list *busy_list = &rx_buf_ring->busy_rxbuf_list;
	struct rtw_rx_buf *rx_buf = NULL;

	_os_spinlock(phl_to_drvpriv(phl_info),
			&rx_buf_ring->busy_rxbuf_lock, _bh, NULL);
	if (true == list_empty(busy_list)) {
		rx_buf = NULL;
	} else {
		rx_buf = list_first_entry(busy_list, struct rtw_rx_buf, list);
		rx_buf_ring->busy_rxbuf_cnt--;
		list_del(&rx_buf->list);
	}
	_os_spinunlock(phl_to_drvpriv(phl_info),
			&rx_buf_ring->busy_rxbuf_lock, _bh, NULL);
	return rx_buf;
}

static struct rtw_rx_buf *query_idle_rx_buf(struct phl_info_t *phl_info,
					struct rtw_rx_buf_ring *rx_buf_ring)
{
	_os_list *idle_list = &rx_buf_ring->idle_rxbuf_list;
	struct rtw_rx_buf *rx_buf = NULL;

	_os_spinlock(phl_to_drvpriv(phl_info),
			&rx_buf_ring->idle_rxbuf_lock, _bh, NULL);
	if (true == list_empty(idle_list)) {
		rx_buf = NULL;
	} else {
		rx_buf = list_first_entry(idle_list, struct rtw_rx_buf, list);
		rx_buf_ring->idle_rxbuf_cnt--;
		list_del(&rx_buf->list);
	}
	_os_spinunlock(phl_to_drvpriv(phl_info),
			&rx_buf_ring->idle_rxbuf_lock, _bh, NULL);

	return rx_buf;
}

enum rtw_phl_status
phl_release_target_rx_buf(struct phl_info_t *phl_info, void *r, u8 ch,
				enum rtw_rx_type type)
{
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_rx_buf_ring *rx_buf_ring = NULL;
	struct rtw_rx_buf *rx_buf = (struct rtw_rx_buf *)r;
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	rx_buf_ring = (struct rtw_rx_buf_ring *)hci_info->rxbuf_pool;

#ifdef CONFIG_DYNAMIC_RX_BUF
	if(rx_buf && (type == RTW_RX_TYPE_WIFI))
		_phl_alloc_dynamic_rxbuf_pcie(rx_buf, phl_info);
#endif
	if (&rx_buf_ring[ch] != NULL && rx_buf != NULL) {
		enqueue_idle_rx_buf(phl_info, &rx_buf_ring[ch], rx_buf);
		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}

static enum rtw_phl_status phl_release_busy_rx_buf(
				struct phl_info_t *phl_info,
				struct rtw_rx_buf_ring *rx_buf_ring,
				u16 release_num)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_rx_buf *rx_buf = NULL;

	if (rx_buf_ring != NULL) {

		while (release_num > 0) {
			rx_buf = query_busy_rx_buf(phl_info, rx_buf_ring);
			if (NULL == rx_buf)
				break;
			enqueue_idle_rx_buf(phl_info, rx_buf_ring, rx_buf);
			release_num--;
		}
		pstatus = RTW_PHL_STATUS_SUCCESS;
	}
	return pstatus;
}



/* static void rtl8852ae_free_wd_page_buf(_adapter *adapter, void *vir_addr, */
/* 				dma_addr_t *bus_addr, size_t size) */
/* { */
/* 	struct platform_ops *ops = &adapter->platform_func; */
/* 	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter); */
/* 	struct pci_dev *pdev = dvobj->ppcidev; */

/* 	FUNCIN(); */
/* 	ops->free_cache_mem(pdev,vir_addr, bus_addr, size, PCI_DMA_TODEVICE); */

/* 	/\* NONCACHE hana_todo */
/* 	 * ops->alloc_noncache_mem(pdev, vir_addr, bus_addr, size); */
/* 	 *\/ */
/* 	FUNCOUT(); */
/* } */

static void _phl_free_rxbuf_pcie(struct phl_info_t *phl_info,
				 struct rtw_rx_buf *rx_buf)
{
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	u16 rxbuf_num = (u16)hal_com->bus_cap.rxbuf_num;
	u16 i = 0;

	if (NULL != rx_buf) {
		for (i = 0; i < rxbuf_num; i++) {

			if (NULL == rx_buf[i].vir_addr)
				continue;
			_os_pkt_buf_free_rx(phl_to_drvpriv(phl_info),
								rx_buf[i].vir_addr,
								(_dma)rx_buf[i].phy_addr_l,
								(_dma)rx_buf[i].phy_addr_h,
								rx_buf[i].buf_len,
								rx_buf[i].cache,
								rx_buf[i].os_priv);
			rx_buf[i].vir_addr = NULL;
			rx_buf[i].cache = 0;
		}

		_os_mem_free(phl_to_drvpriv(phl_info), rx_buf,
					sizeof(struct rtw_rx_buf) * rxbuf_num);
	}
}

static void _phl_free_rxbuf_pool_pcie(struct phl_info_t *phl_info,
						u8 *rxbuf_pool, u8 ch_num)
{
	struct rtw_rx_buf_ring *ring = NULL;
	u8 i = 0;

	FUNCIN();
		ring = (struct rtw_rx_buf_ring *)rxbuf_pool;
	if (NULL != ring) {
		for (i = 0; i < ch_num; i++) {

			ring[i].idle_rxbuf_cnt = 0;

			if (NULL == ring[i].rx_buf)
				continue;

			_phl_free_rxbuf_pcie(phl_info, ring[i].rx_buf);
			ring[i].rx_buf = NULL;
			_os_spinlock_free(phl_to_drvpriv(phl_info),
					&ring->idle_rxbuf_lock);
			_os_spinlock_free(phl_to_drvpriv(phl_info),
					&ring->busy_rxbuf_lock);
		}
		_os_mem_free(phl_to_drvpriv(phl_info), ring,
					sizeof(struct rtw_rx_buf_ring) * ch_num);
	}

	FUNCOUT();
}

static void _phl_destory_dma_pool_pcie(struct phl_info_t *phl_info, struct dma_pool *pool)
{
#ifdef CONFIG_DMA_TX_USE_COHERENT_MEM
	_os_dma_pool_destory(phl_to_drvpriv(phl_info), pool);
#endif
}

/* static void *rtl8852ae_alloc_wd_page_buf(_adapter *adapter, */
/* 					 dma_addr_t *bus_addr, size_t size) */
/* { */
/* 	struct platform_ops *ops = &adapter->platform_func; */
/* 	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter); */
/* 	struct pci_dev *pdev = dvobj->ppcidev; */
/* 	void *vir_addr = NULL; */

/* 	FUNCIN(); */
/* 	vir_addr = ops->alloc_cache_mem(pdev, bus_addr, size, PCI_DMA_TODEVICE); */

/* 	/\* NONCACHE hana_todo */
/* 	 * vir_addr = ops->alloc_noncache_mem(pdev, bus_addr, size); */
/* 	 *\/ */

/* 	FUNCOUT(); */
/* 	return vir_addr; */
/* } */

static 	struct rtw_rx_buf *
_phl_alloc_rxbuf_pcie(struct phl_info_t *phl_info,
				struct rtw_rx_buf_ring *rx_buf_ring)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_rx_buf *rx_buf = NULL;
	u32 buf_len = 0;
	u16 rxbuf_num = (u16)hal_com->bus_cap.rxbuf_num;
	void *drv_priv = phl_to_drvpriv(phl_info);
	int i;

	buf_len = sizeof(*rx_buf) * rxbuf_num;
	rx_buf = _os_mem_alloc(drv_priv, buf_len);
	if (rx_buf != NULL) {
		for (i = 0; i < rxbuf_num; i++) {
#ifdef CONFIG_DMA_RX_USE_COHERENT_MEM
			rx_buf[i].cache = false;
#else
			rx_buf[i].cache = VIRTUAL_ADDR;
#endif
			buf_len = RX_BUF_SIZE;
			rx_buf[i].vir_addr = _os_pkt_buf_alloc_rx(
					phl_to_drvpriv(phl_info),
					(_dma *)&rx_buf[i].phy_addr_l,
					(_dma *)&rx_buf[i].phy_addr_h,
					buf_len,
					rx_buf[i].cache,
					&rx_buf[i].os_priv);
			if (NULL == rx_buf[i].vir_addr) {
				pstatus = RTW_PHL_STATUS_RESOURCE;
				break;
			}
			rx_buf[i].buf_len = buf_len;
			rx_buf[i].dynamic = 0;
#ifdef CONFIG_DYNAMIC_RX_BUF
			rx_buf[i].reuse = false;
#endif

			INIT_LIST_HEAD(&rx_buf[i].list);
			enqueue_idle_rx_buf(phl_info, rx_buf_ring, &rx_buf[i]);
			pstatus = RTW_PHL_STATUS_SUCCESS;
				/* hana_todo now check 4 byte align only */
			/* if ((unsigned long)wd_page_buf & 0xF) { */
			/* 	res = _FAIL; */
			/* 	break; */
			/* } */
		}
	}

	if (RTW_PHL_STATUS_SUCCESS != pstatus) {
		_phl_free_rxbuf_pcie(phl_info, rx_buf);
		rx_buf = NULL;
	}
	return rx_buf;
}



static enum rtw_phl_status
_phl_alloc_rxbuf_pool_pcie(struct phl_info_t *phl_info, u8 ch_num)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_rx_buf_ring *rx_buf_ring = NULL;
	struct rtw_rx_buf *rx_buf = NULL;
	u32 buf_len = 0;
	u16 rxbuf_num = (u16)hal_com->bus_cap.rxbuf_num;
	int i;
	FUNCIN_WSTS(pstatus);

	buf_len = sizeof(*rx_buf_ring) * ch_num;
	rx_buf_ring = _os_mem_alloc(phl_to_drvpriv(phl_info), buf_len);

	if (NULL != rx_buf_ring) {
		for (i = 0; i < ch_num; i++) {
			_os_spinlock_init(phl_to_drvpriv(phl_info),
					&rx_buf_ring[i].idle_rxbuf_lock);
			_os_spinlock_init(phl_to_drvpriv(phl_info),
					&rx_buf_ring[i].busy_rxbuf_lock);
			INIT_LIST_HEAD(&rx_buf_ring[i].idle_rxbuf_list);
			INIT_LIST_HEAD(&rx_buf_ring[i].busy_rxbuf_list);
			rx_buf = _phl_alloc_rxbuf_pcie(phl_info,
							&rx_buf_ring[i]);
			if (NULL == rx_buf) {
				pstatus = RTW_PHL_STATUS_RESOURCE;
				break;
			}
			rx_buf_ring[i].rx_buf = rx_buf;
			rx_buf_ring[i].idle_rxbuf_cnt = rxbuf_num;
			rx_buf_ring[i].busy_rxbuf_cnt = 0;
			pstatus = RTW_PHL_STATUS_SUCCESS;
		}
	}

	if (RTW_PHL_STATUS_SUCCESS == pstatus) {
		phl_info->hci->rxbuf_pool = (u8 *)rx_buf_ring;
	} else
		_phl_free_rxbuf_pool_pcie(phl_info, (u8 *)rx_buf_ring, ch_num);
	FUNCOUT_WSTS(pstatus);

	return pstatus;
}




/* static void rtl8852ae_free_wd_page_buf(_adapter *adapter, void *vir_addr, */
/* 				dma_addr_t *bus_addr, size_t size) */
/* { */
/* 	struct platform_ops *ops = &adapter->platform_func; */
/* 	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter); */
/* 	struct pci_dev *pdev = dvobj->ppcidev; */

/* 	FUNCIN(); */
/* 	ops->free_cache_mem(pdev,vir_addr, bus_addr, size, PCI_DMA_TODEVICE); */

/* 	/\* NONCACHE hana_todo */
/* 	 * ops->alloc_noncache_mem(pdev, vir_addr, bus_addr, size); */
/* 	 *\/ */
/* 	FUNCOUT(); */
/* } */

static void _phl_free_wd_page_pcie(struct phl_info_t *phl_info,
					struct rtw_wd_page *wd_page)
{
	u16 i = 0;

	if (NULL != wd_page) {
		for (i = 0; i < MAX_WD_PAGE_NUM; i++) {

			if (NULL == wd_page[i].vir_addr)
				continue;

			wd_page[i].wp_seq = WP_RESERVED_SEQ;
			_os_shmem_free(phl_to_drvpriv(phl_info),
						phl_info->hci->wd_dma_pool,
						wd_page[i].vir_addr,
						(_dma *)&wd_page[i].phy_addr_l,
						(_dma *)&wd_page[i].phy_addr_h,
						wd_page[i].buf_len,
						wd_page[i].cache,
						PCI_DMA_FROMDEVICE,
						wd_page[i].os_rsvd[0]);
			wd_page[i].vir_addr = NULL;
			wd_page[i].cache = 0;
		}

		_os_mem_free(phl_to_drvpriv(phl_info), wd_page,
					sizeof(struct rtw_wd_page) * MAX_WD_PAGE_NUM);
	}
}

static void _phl_free_wd_ring_pcie(struct phl_info_t *phl_info, u8 *wd_page_buf,
					u8 ch_num)
{
	struct rtw_wd_page_ring *wd_page_ring = NULL;
	void *drv_priv = phl_to_drvpriv(phl_info);
	u8 i = 0;
	FUNCIN();

	wd_page_ring = (struct rtw_wd_page_ring *)wd_page_buf;
	if (NULL != wd_page_ring) {
		for (i = 0; i < ch_num; i++) {

			wd_page_ring[i].idle_wd_page_cnt = 0;

			if (NULL == wd_page_ring[i].wd_page)
				continue;

			if (i == rtw_hal_get_fwcmd_queue_idx(phl_info->hal)) {
				_phl_free_h2c_work_ring(phl_info,
							&wd_page_ring[i]);
			}
			_phl_free_wd_work_ring(phl_info, &wd_page_ring[i]);
			_phl_free_wd_page_pcie(phl_info,
						wd_page_ring[i].wd_page);
			wd_page_ring[i].wd_page = NULL;
			_os_spinlock_free(drv_priv,
						&wd_page_ring[i].idle_lock);
			_os_spinlock_free(drv_priv,
						&wd_page_ring[i].busy_lock);
			_os_spinlock_free(drv_priv,
						&wd_page_ring[i].pending_lock);
			_os_spinlock_free(drv_priv,
						&wd_page_ring[i].work_lock);
			_os_spinlock_free(drv_priv,
						&wd_page_ring[i].wp_tag_lock);
		}
		_os_mem_free(phl_to_drvpriv(phl_info), wd_page_ring,
						sizeof(struct rtw_wd_page_ring) * ch_num);
	}
	FUNCOUT();
}

/* static void *rtl8852ae_alloc_wd_page_buf(_adapter *adapter, */
/* 					 dma_addr_t *bus_addr, size_t size) */
/* { */
/* 	struct platform_ops *ops = &adapter->platform_func; */
/* 	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter); */
/* 	struct pci_dev *pdev = dvobj->ppcidev; */
/* 	void *vir_addr = NULL; */

/* 	FUNCIN(); */
/* 	vir_addr = ops->alloc_cache_mem(pdev, bus_addr, size, PCI_DMA_TODEVICE); */

/* 	/\* NONCACHE hana_todo */
/* 	 * vir_addr = ops->alloc_noncache_mem(pdev, bus_addr, size); */
/* 	 *\/ */

/* 	FUNCOUT(); */
/* 	return vir_addr; */
/* } */

static struct rtw_wd_page *_phl_alloc_wd_page_pcie(
			struct phl_info_t *phl_info, _os_list *list)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_wd_page *wd_page = NULL;
	void *dma_pool = NULL;
	u32 buf_len = 0;
	int i;

	buf_len = sizeof(*wd_page) * MAX_WD_PAGE_NUM;
	wd_page = _os_mem_alloc(phl_to_drvpriv(phl_info), buf_len);
	if (wd_page != NULL) {
		for (i = 0; i < MAX_WD_PAGE_NUM; i++) {
#ifdef CONFIG_DMA_TX_USE_COHERENT_MEM
			wd_page[i].cache = POOL_ADDR;
			dma_pool = phl_info->hci->wd_dma_pool;
#else
			wd_page[i].cache = VIRTUAL_ADDR;
#endif
			buf_len = WD_PAGE_SIZE;
			wd_page[i].vir_addr = _os_shmem_alloc(
						phl_to_drvpriv(phl_info), dma_pool,
						(_dma *)&wd_page[i].phy_addr_l,
						(_dma *)&wd_page[i].phy_addr_h,
						buf_len,
						wd_page[i].cache,
						PCI_DMA_TODEVICE,
						&wd_page[i].os_rsvd[0]);
			if (NULL == wd_page[i].vir_addr) {
				pstatus = RTW_PHL_STATUS_RESOURCE;
				break;
			}
			wd_page[i].buf_len = buf_len;
			wd_page[i].wp_seq = WP_RESERVED_SEQ;
			INIT_LIST_HEAD(&wd_page[i].list);

			list_add_tail(&wd_page[i].list, list);

			pstatus = RTW_PHL_STATUS_SUCCESS;
				/* hana_todo now check 4 byte align only */
			/* if ((unsigned long)wd_page_buf & 0xF) { */
			/* 	res = _FAIL; */
			/* 	break; */
			/* } */
		}
	}

	if (RTW_PHL_STATUS_SUCCESS != pstatus) {
		_phl_free_wd_page_pcie(phl_info, wd_page);
		wd_page = NULL;
	}

	return wd_page;
}



static enum rtw_phl_status
_phl_alloc_wd_ring_pcie(struct phl_info_t *phl_info, u8 ch_num)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_wd_page_ring *wd_page_ring = NULL;
	struct rtw_wd_page *wd_page = NULL;
	void *drv_priv = NULL;
	u32 buf_len = 0;
	int i;

	FUNCIN_WSTS(pstatus);
	drv_priv = phl_to_drvpriv(phl_info);
	buf_len = sizeof(struct rtw_wd_page_ring) * ch_num;
	wd_page_ring = _os_mem_alloc(phl_to_drvpriv(phl_info), buf_len);
	if (NULL != wd_page_ring) {
		for (i = 0; i < ch_num; i++) {
			INIT_LIST_HEAD(&wd_page_ring[i].idle_wd_page_list);
			INIT_LIST_HEAD(&wd_page_ring[i].busy_wd_page_list);
			INIT_LIST_HEAD(&wd_page_ring[i].pending_wd_page_list);
			_os_spinlock_init(drv_priv,
						&wd_page_ring[i].idle_lock);
			_os_spinlock_init(drv_priv,
						&wd_page_ring[i].busy_lock);
			_os_spinlock_init(drv_priv,
						&wd_page_ring[i].pending_lock);
			_os_spinlock_init(drv_priv,
						&wd_page_ring[i].work_lock);
			_os_spinlock_init(drv_priv,
						&wd_page_ring[i].wp_tag_lock);

			wd_page = _phl_alloc_wd_page_pcie(phl_info,
					&wd_page_ring[i].idle_wd_page_list);
			if (NULL == wd_page) {
				pstatus = RTW_PHL_STATUS_RESOURCE;
				break;
			}

			pstatus = _phl_alloc_wd_work_ring(phl_info,
							  &wd_page_ring[i]);
			if (RTW_PHL_STATUS_SUCCESS != pstatus)
				break;

			if (i == rtw_hal_get_fwcmd_queue_idx(phl_info->hal)) {
				pstatus = _phl_alloc_h2c_work_ring(phl_info,
							     &wd_page_ring[i]);
				if (RTW_PHL_STATUS_SUCCESS != pstatus)
					break;
			}
			wd_page_ring[i].wd_page = wd_page;
			wd_page_ring[i].idle_wd_page_cnt = MAX_WD_PAGE_NUM;
			wd_page_ring[i].busy_wd_page_cnt = 0;
			wd_page_ring[i].pending_wd_page_cnt = 0;
			wd_page_ring[i].wp_seq = 1;
			pstatus = RTW_PHL_STATUS_SUCCESS;
		}
	}

	if (RTW_PHL_STATUS_SUCCESS == pstatus) {
		phl_info->hci->wd_ring = (u8 *)wd_page_ring;
	} else
		_phl_free_wd_ring_pcie(phl_info, (u8 *)wd_page_ring, ch_num);
	FUNCOUT_WSTS(pstatus);

	return pstatus;
}

static void _phl_free_h2c_pkt_buf_pcie(struct phl_info_t *phl_info,
				struct rtw_h2c_pkt *_h2c_pkt)
{
	struct rtw_h2c_pkt *h2c_pkt = _h2c_pkt;

	_os_shmem_free(phl_to_drvpriv(phl_info), NULL,
				h2c_pkt->vir_head,
				(_dma *)&h2c_pkt->phy_addr_l,
				(_dma *)&h2c_pkt->phy_addr_h,
				h2c_pkt->buf_len,
				h2c_pkt->cache,
				PCI_DMA_FROMDEVICE,
				h2c_pkt->os_rsvd[0]);
}

enum rtw_phl_status _phl_alloc_h2c_pkt_buf_pcie(struct phl_info_t *phl_info,
	struct rtw_h2c_pkt *_h2c_pkt, u32 buf_len)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_h2c_pkt *h2c_pkt = _h2c_pkt;

	h2c_pkt->vir_head = _os_shmem_alloc(
				phl_to_drvpriv(phl_info), NULL,
				(_dma *)&h2c_pkt->phy_addr_l,
				(_dma *)&h2c_pkt->phy_addr_h,
				buf_len,
				h2c_pkt->cache,
				PCI_DMA_TODEVICE,
				&h2c_pkt->os_rsvd[0]);

	if (h2c_pkt->vir_head)
		pstatus = RTW_PHL_STATUS_SUCCESS;

	return pstatus;
}

static void _phl_free_rxbd_pcie(struct phl_info_t *phl_info,
						u8 *rxbd_buf, u8 ch_num)
{
	struct rx_base_desc *rxbd = (struct rx_base_desc *)rxbd_buf;
	u8 i = 0;

	FUNCIN();

	if (NULL != rxbd) {
		for (i = 0; i < ch_num; i++) {

			if (NULL == rxbd[i].vir_addr)
				continue;
			_os_shmem_free(phl_to_drvpriv(phl_info), NULL,
						rxbd[i].vir_addr,
						(_dma *)&rxbd[i].phy_addr_l,
						(_dma *)&rxbd[i].phy_addr_h,
						rxbd[i].buf_len,
						rxbd[i].cache,
						PCI_DMA_FROMDEVICE,
						rxbd[i].os_rsvd[0]);
			rxbd[i].vir_addr = NULL;
			rxbd[i].cache = 0;
		}

		_os_mem_free(phl_to_drvpriv(phl_info), rxbd,
					sizeof(struct rx_base_desc) * ch_num);
	}
	FUNCOUT();
}


static enum rtw_phl_status
_phl_alloc_rxbd_pcie(struct phl_info_t *phl_info, u8 ch_num)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rx_base_desc *rxbd = NULL;
	u32 buf_len = 0;
	u16 rxbd_num = (u16)hal_com->bus_cap.rxbd_num;
	u8 addr_info_size = hal_com->bus_hw_cap.rxbd_len;
	u8 i = 0;
	FUNCIN_WSTS(pstatus);

	buf_len = sizeof(struct rx_base_desc) * ch_num;
	rxbd = _os_mem_alloc(phl_to_drvpriv(phl_info), buf_len);
	if (NULL != rxbd) {
		for (i = 0; i < ch_num; i++) {
			rxbd[i].cache = DMA_ADDR;
			buf_len = addr_info_size * rxbd_num;
			rxbd[i].vir_addr = _os_shmem_alloc(
						phl_to_drvpriv(phl_info), NULL,
						(_dma *)&rxbd[i].phy_addr_l,
						(_dma *)&rxbd[i].phy_addr_h,
						buf_len,
						rxbd[i].cache,
						PCI_DMA_TODEVICE,
						&rxbd[i].os_rsvd[0]);
			if (NULL == rxbd[i].vir_addr) {
				pstatus = RTW_PHL_STATUS_RESOURCE;
				break;
			}
			rxbd[i].buf_len = buf_len;
			rxbd[i].host_idx = 0;
			rxbd[i].hw_idx = 0;
			pstatus = RTW_PHL_STATUS_SUCCESS;
		}
	}

	if (RTW_PHL_STATUS_SUCCESS == pstatus)
		phl_info->hci->rxbd_buf = (u8 *)rxbd;
	else
		_phl_free_rxbd_pcie(phl_info, (u8 *)rxbd, ch_num);
	FUNCOUT_WSTS(pstatus);

	return pstatus;
}


static void _phl_free_txbd_pcie(struct phl_info_t *phl_info, u8 *txbd_buf,
				u8 ch_num)
{
	struct tx_base_desc *txbd = (struct tx_base_desc *)txbd_buf;
	u8 i = 0;
	FUNCIN();

	if (NULL != txbd) {
		for (i = 0; i < ch_num; i++) {

			if (NULL == txbd[i].vir_addr)
				continue;
			_os_shmem_free(phl_to_drvpriv(phl_info), NULL,
						txbd[i].vir_addr,
						(_dma *)&txbd[i].phy_addr_l,
						(_dma *)&txbd[i].phy_addr_h,
						txbd[i].buf_len,
						txbd[i].cache,
						PCI_DMA_FROMDEVICE,
						txbd[i].os_rsvd[0]);
			txbd[i].vir_addr = NULL;
			txbd[i].cache = 0;
			_os_spinlock_free(phl_to_drvpriv(phl_info),
						&txbd[i].txbd_lock);
		}

		_os_mem_free(phl_to_drvpriv(phl_info), txbd,
						sizeof(struct tx_base_desc) * ch_num);
	}

	FUNCOUT();
}



static enum rtw_phl_status
_phl_alloc_txbd_pcie(struct phl_info_t *phl_info, u8 ch_num)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct tx_base_desc *txbd = NULL;
	u32 buf_len = 0;
	u16 txbd_num = (u16)hal_com->bus_cap.txbd_num;
	u8 addr_info_size = hal_com->bus_hw_cap.txbd_len;
	u8 i = 0;
	FUNCIN_WSTS(pstatus);

	buf_len = sizeof(struct tx_base_desc) * ch_num;
	txbd = _os_mem_alloc(phl_to_drvpriv(phl_info), buf_len);
	if (NULL != txbd) {
		for (i = 0; i < ch_num; i++) {
			txbd[i].cache = DMA_ADDR;
			buf_len = addr_info_size * txbd_num;
			txbd[i].vir_addr = _os_shmem_alloc(
						phl_to_drvpriv(phl_info), NULL,
						(_dma *)&txbd[i].phy_addr_l,
						(_dma *)&txbd[i].phy_addr_h,
						buf_len,
						txbd[i].cache,
						PCI_DMA_TODEVICE,
						&txbd[i].os_rsvd[0]);
			if (NULL == txbd[i].vir_addr) {
				pstatus = RTW_PHL_STATUS_RESOURCE;
				break;
			}
			txbd[i].buf_len = buf_len;
			txbd[i].avail_num = txbd_num;
			_os_spinlock_init(phl_to_drvpriv(phl_info),
						&txbd[i].txbd_lock);
			pstatus = RTW_PHL_STATUS_SUCCESS;
		}
	}

	if (RTW_PHL_STATUS_SUCCESS == pstatus)
		phl_info->hci->txbd_buf = (u8 *)txbd;
	else
		_phl_free_txbd_pcie(phl_info, (u8 *)txbd, ch_num);
	FUNCOUT_WSTS(pstatus);

	return pstatus;
}

enum rtw_phl_status _phl_update_default_rx_bd(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rx_base_desc *rxbd = NULL;
	struct rtw_rx_buf_ring *ring = NULL;
	struct rtw_rx_buf *rxbuf = NULL;
	u32 i = 0, j = 0;

	rxbd = (struct rx_base_desc *)hci_info->rxbd_buf;
	ring = (struct rtw_rx_buf_ring *)hci_info->rxbuf_pool;
	for (i = 0; i < hci_info->total_rxch_num; i++) {
		for (j = 0; j < hal_com->bus_cap.rxbd_num; j++) {
			rxbuf = query_idle_rx_buf(phl_info, &ring[i]);
			if (NULL == rxbuf) {
				PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
					"[WARNING] there is no resource for rx bd default setting\n");
				pstatus = RTW_PHL_STATUS_RESOURCE;
				break;
			}

			hstatus = rtw_hal_update_rxbd(phl_info->hal, &rxbd[i],
								rxbuf);
			if (RTW_HAL_STATUS_SUCCESS == hstatus) {
				enqueue_busy_rx_buf(phl_info, &ring[i], rxbuf, _tail);
				pstatus = RTW_PHL_STATUS_SUCCESS;
			} else {
				enqueue_idle_rx_buf(phl_info, &ring[i], rxbuf);
				PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING] update rx bd fail\n");
				pstatus = RTW_PHL_STATUS_FAILURE;
				break;
			}
		}

	}

	return pstatus;
}

static void _phl_reset_rxbd(struct phl_info_t *phl_info,
					struct rx_base_desc *rxbd)
{
	_os_mem_set(phl_to_drvpriv(phl_info), rxbd->vir_addr, 0, rxbd->buf_len);
	rxbd->host_idx = 0;
	rxbd->hw_idx = 0;
}


static void phl_rx_reset_pcie(struct phl_info_t *phl_info)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct hal_spec_t *hal_spec = &phl_com->hal_spec;
	struct rx_base_desc *rxbd = NULL;
	struct rtw_rx_buf_ring *ring = NULL;
	u8 ch = 0;

	rxbd = (struct rx_base_desc *)hci_info->rxbd_buf;
	ring = (struct rtw_rx_buf_ring *)hci_info->rxbuf_pool;

	for (ch = 0; ch < hci_info->total_rxch_num; ch++) {
		_phl_reset_rxbd(phl_info, &rxbd[ch]);
		phl_release_busy_rx_buf(phl_info, &ring[ch],
					ring[ch].busy_rxbuf_cnt);
	}
	hal_spec->rx_tag[0] = 0;
	hal_spec->rx_tag[1] = 0;
	_phl_update_default_rx_bd(phl_info);

}


void _phl_sort_ring_by_hw_res(struct phl_info_t *phl_info)
{
	_os_list *t_fctrl_result = &phl_info->t_fctrl_result;
	struct phl_ring_status *ring_sts, *t;
	u16 hw_res = 0, host_idx = 0, hw_idx = 0;
	u32 avail = 0, no_res = 0;
	_os_list *no_res_first = NULL;

	phl_list_for_loop_safe(ring_sts, t, struct phl_ring_status,
					t_fctrl_result, list) {

		if (ring_sts->ring_ptr->dma_ch > 32)
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			"[WARNING] dma channel number larger than record map\n");

		if (no_res & (BIT0 << ring_sts->ring_ptr->dma_ch)) {
			if (&ring_sts->list == no_res_first)
				break;
			list_del(&ring_sts->list);
			list_add_tail(&ring_sts->list, t_fctrl_result);
			continue;
		} else if (avail & (BIT0 << ring_sts->ring_ptr->dma_ch)) {
			continue;
		}

		hw_res = rtw_hal_tx_res_query(phl_info->hal,
						ring_sts->ring_ptr->dma_ch,
						&host_idx, &hw_idx);
		if (0 == hw_res) {
			if (no_res_first == NULL)
				no_res_first = &ring_sts->list;
			list_del(&ring_sts->list);
			list_add_tail(&ring_sts->list, t_fctrl_result);
			no_res = no_res | (BIT0 << ring_sts->ring_ptr->dma_ch);
		} else {
			avail = avail | (BIT0 << ring_sts->ring_ptr->dma_ch);
		}
	}
}

void _phl_tx_flow_ctrl_pcie(struct phl_info_t *phl_info, _os_list *sta_list)
{
	/* _phl_sort_ring_by_hw_res(phl_info); */
	phl_tx_flow_ctrl(phl_info, sta_list);
}

static enum rtw_phl_status _phl_handle_xmit_ring_pcie
						(struct phl_info_t *phl_info,
						struct phl_ring_status *ring_sts)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct phl_hci_trx_ops *hci_trx_ops = phl_info->hci_trx_ops;
	struct rtw_phl_tx_ring *tring = ring_sts->ring_ptr;
	struct rtw_xmit_req *tx_req = NULL;
	u16 rptr = 0, next_idx = 0;
	void *drv_priv = phl_to_drvpriv(phl_info);

	while (0 != ring_sts->req_busy) {
		rptr = (u16)_os_atomic_read(drv_priv, &tring->phl_next_idx);

		tx_req = (struct rtw_xmit_req *)tring->entry[rptr];
		if (NULL == tx_req)  {
			PHL_ERR("tx_req is NULL!\n");
			break;
		}
		tx_req->mdata.macid = ring_sts->macid;
		tx_req->mdata.band = ring_sts->band;
		tx_req->mdata.wmm = ring_sts->wmm;
		tx_req->mdata.hal_port = ring_sts->port;
		/*tx_req->mdata.mbssid = ring_sts->mbssid;*/
		tx_req->mdata.tid = tring->tid;
		tx_req->mdata.dma_ch = tring->dma_ch;
		pstatus = hci_trx_ops->prepare_tx(phl_info, tx_req);

		if (RTW_PHL_STATUS_SUCCESS == pstatus) {
			ring_sts->req_busy--;

			/* hana_todo, workaround here to update phl_index */
			_os_atomic_set(drv_priv, &tring->phl_idx, rptr);

			if (0 != ring_sts->req_busy) {
				next_idx = rptr + 1;

				if (next_idx >= MAX_PHL_RING_ENTRY_NUM) {
					_os_atomic_set(drv_priv,
						       &tring->phl_next_idx, 0);

				} else {
					_os_atomic_inc(drv_priv,
						       &tring->phl_next_idx);
				}
			}
		} else {
			PHL_INFO("HCI prepare tx fail\n");
			break;
		}
	}

	return pstatus;
}

static void _phl_tx_callback_pcie(void *context)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_phl_handler *phl_handler
		= (struct rtw_phl_handler *)phl_container_of(context,
							struct rtw_phl_handler,
							os_handler);
	struct phl_info_t *phl_info = (struct phl_info_t *)phl_handler->context;
	struct phl_hci_trx_ops *hci_trx_ops = phl_info->hci_trx_ops;
	struct phl_ring_status *ring_sts = NULL, *t;
	void *drvpriv = phl_to_drvpriv(phl_info);
	_os_list sta_list;
	bool tx_pause = false;

	FUNCIN_WSTS(pstatus);
	INIT_LIST_HEAD(&sta_list);

	/* check datapath sw state */
	tx_pause = phl_datapath_chk_trx_pause(phl_info, PHL_CTRL_TX);
	if (true == tx_pause)
		goto end;

#ifdef CONFIG_POWER_SAVE
	/* check ps state when tx is not paused */
	if (false == phl_ps_is_datapath_allowed(phl_info)) {
		PHL_WARN("%s(): datapath is not allowed now... may in low power.\n", __func__);
		goto chk_stop;
	}
#endif

	if (true == phl_check_xmit_ring_resource(phl_info, &sta_list)) {
		_phl_tx_flow_ctrl_pcie(phl_info, &sta_list);

		phl_list_for_loop_safe(ring_sts, t, struct phl_ring_status,
		                       &phl_info->t_fctrl_result, list) {
			list_del(&ring_sts->list);
			_phl_handle_xmit_ring_pcie(phl_info, ring_sts);
			phl_release_ring_sts(phl_info, ring_sts);
		}
	}

	pstatus = hci_trx_ops->tx(phl_info);
	if (RTW_PHL_STATUS_FAILURE == pstatus) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING] phl_tx fail!\n");
	}

#ifdef CONFIG_POWER_SAVE
chk_stop:
#endif
	if (PHL_TX_STATUS_STOP_INPROGRESS ==
	    _os_atomic_read(drvpriv, &phl_info->phl_sw_tx_sts)) {
		PHL_WARN("PHL_TX_STATUS_STOP_INPROGRESS, going to stop sw tx.\n");
		phl_tx_stop_pcie(phl_info);
	}

end:
	phl_free_deferred_tx_ring(phl_info);

	FUNCOUT_WSTS(pstatus);
}


static u8 _phl_check_rx_hw_resource(struct phl_info_t *phl_info)
{
	struct hci_info_t *hci_info = phl_info->hci;
	u16 hw_res = 0, host_idx = 0, hw_idx = 0;
	u8 i = 0;
	u8 avail = 0;

	for (i = 0; i < hci_info->total_rxch_num; i++) {
		hw_res = rtw_hal_rx_res_query(phl_info->hal,
							i,
							&host_idx, &hw_idx);

		if (0 != hw_res) {
			avail = true;
			break;
		} else {
			avail = false;
		}
	}

	return avail;
}

static void _phl_rx_callback_pcie(void *context)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_phl_handler *phl_handler
		= (struct rtw_phl_handler *)phl_container_of(context,
							struct rtw_phl_handler,
							os_handler);
	struct phl_info_t *phl_info = (struct phl_info_t *)phl_handler->context;
	struct phl_hci_trx_ops *hci_trx_ops = phl_info->hci_trx_ops;
	void *drvpriv = phl_to_drvpriv(phl_info);
	bool rx_pause = false;
#ifdef CONFIG_SYNC_INTERRUPT
	struct rtw_phl_evt_ops *ops = &phl_info->phl_com->evt_ops;
#endif /* CONFIG_SYNC_INTERRUPT */

	FUNCIN_WSTS(pstatus);

	/* check datapath sw state */
	rx_pause = phl_datapath_chk_trx_pause(phl_info, PHL_CTRL_RX);
	if (true == rx_pause)
		goto end;

	do {
		if (false == phl_check_recv_ring_resource(phl_info))
			break;

		pstatus = hci_trx_ops->rx(phl_info);

		if (RTW_PHL_STATUS_FAILURE == pstatus) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING] phl_rx fail!\n");
		}
	} while (false);

	if (PHL_RX_STATUS_STOP_INPROGRESS ==
	    _os_atomic_read(drvpriv, &phl_info->phl_sw_rx_sts)) {
		phl_rx_stop_pcie(phl_info);
	}

end:
	/* restore int mask of rx */
	rtw_hal_restore_rx_interrupt(phl_info->hal);
#ifdef CONFIG_SYNC_INTERRUPT
	ops->interrupt_restore(phl_to_drvpriv(phl_info), true);
#endif /* CONFIG_SYNC_INTERRUPT */

	FUNCOUT_WSTS(pstatus);

}
void _phl_fill_tx_meta_data(struct rtw_xmit_req *tx_req,
                            u16 packet_len)
{
	tx_req->mdata.wp_offset = 56;
	tx_req->mdata.wd_page_size = 1;
	tx_req->mdata.addr_info_num = tx_req->pkt_cnt;
	tx_req->mdata.pktlen = packet_len;
}



void phl_trx_resume_pcie(struct phl_info_t *phl_info, u8 type)
{
	if (PHL_CTRL_TX & type)
		phl_tx_resume_pcie(phl_info);

	if (PHL_CTRL_RX & type)
		phl_rx_resume_pcie(phl_info);
}

void phl_trx_reset_pcie(struct phl_info_t *phl_info, u8 type)
{
	struct rtw_phl_com_t *phl_com = phl_info->phl_com;
	struct rtw_stats *phl_stats = &phl_com->phl_stats;

	PHL_INFO("%s\n", __func__);

	if (PHL_CTRL_TX & type) {
		phl_tx_reset_pcie(phl_info);
		phl_reset_tx_stats(phl_stats);
	}

	if (PHL_CTRL_RX & type) {
		phl_rx_reset_pcie(phl_info);
		phl_reset_rx_stats(phl_stats);
	}
}

void phl_trx_stop_pcie(struct phl_info_t *phl_info)
{
	phl_trx_reset_pcie(phl_info, PHL_CTRL_TX|PHL_CTRL_RX);
}

void phl_trx_deinit_pcie(struct phl_info_t *phl_info)
{
	struct hci_info_t *hci_info = phl_info->hci;
	FUNCIN();
	_phl_free_rxbuf_pool_pcie(phl_info, hci_info->rxbuf_pool,
					hci_info->total_rxch_num);
	hci_info->rxbuf_pool = NULL;

	_phl_free_rxbd_pcie(phl_info, hci_info->rxbd_buf,
					hci_info->total_rxch_num);
	hci_info->rxbd_buf = NULL;

	_phl_free_wd_ring_pcie(phl_info, hci_info->wd_ring,
					hci_info->total_txch_num);
	hci_info->wd_ring = NULL;

	_phl_free_txbd_pcie(phl_info, hci_info->txbd_buf,
					hci_info->total_txch_num);
	hci_info->txbd_buf = NULL;

	_phl_destory_dma_pool_pcie(phl_info, hci_info->wd_dma_pool);

	FUNCOUT();
}

enum rtw_phl_status phl_trx_init_pcie(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct hci_info_t *hci_info = phl_info->hci;
	struct rtw_phl_handler *tx_handler = &phl_info->phl_tx_handler;
	struct rtw_phl_handler *rx_handler = &phl_info->phl_rx_handler;
	struct rtw_phl_handler *ser_handler = &phl_info->phl_ser_handler;
	void *drv_priv = phl_to_drvpriv(phl_info);
	u8 txch_num = 0, rxch_num = 0;
	u16 i = 0;

	FUNCIN_WSTS(pstatus);

#ifdef CONFIG_DMA_TX_USE_COHERENT_MEM
	/* init DMA pool */
	hci_info->wd_dma_pool = _os_dma_pool_create(drv_priv, "wd_page_pool", WD_PAGE_SIZE);

	if (hci_info->wd_dma_pool == NULL)
		return pstatus;
#endif
	do {
#ifdef CONFIG_PHL_CPU_BALANCE_TX
		_os_workitem *workitem = &tx_handler->os_handler.u.workitem;
		_os_workitem_config_cpu(drv_priv, workitem, "TX_PHL_0", CPU_ID_TX_PHL_0);
		tx_handler->type = RTW_PHL_HANDLER_PRIO_LOW;
#else
		tx_handler->type = RTW_PHL_HANDLER_PRIO_HIGH; /* tasklet */
#endif
		tx_handler->callback = _phl_tx_callback_pcie;
		tx_handler->context = phl_info;
		tx_handler->drv_priv = drv_priv;
		pstatus = phl_register_handler(phl_info->phl_com, tx_handler);
		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;

		ser_handler->type = RTW_PHL_HANDLER_PRIO_HIGH; /* tasklet */
		ser_handler->callback = phl_ser_send_check;
		ser_handler->context = phl_info;
		ser_handler->drv_priv = drv_priv;
		pstatus = phl_register_handler(phl_info->phl_com, ser_handler);
		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;

#ifdef CONFIG_DMA_TX_USE_COHERENT_MEM
		/* avoid dma_free_coherent() being called in atomic context */
		rx_handler->type = RTW_PHL_HANDLER_PRIO_LOW;
#else
		rx_handler->type = RTW_PHL_HANDLER_PRIO_HIGH;
#endif
		rx_handler->callback = _phl_rx_callback_pcie;
		rx_handler->context = phl_info;
		rx_handler->drv_priv = drv_priv;
		pstatus = phl_register_handler(phl_info->phl_com, rx_handler);
		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;
		/* pcie tx sw resource */
		txch_num = rtw_hal_query_txch_num(phl_info->hal);
		hci_info->total_txch_num = txch_num;
		/* allocate tx bd */
		pstatus = _phl_alloc_txbd_pcie(phl_info, txch_num);
		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;
		/* allocate wd page */
		pstatus = _phl_alloc_wd_ring_pcie(phl_info, txch_num);
		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;

		for (i = 0; i < PHL_MACID_MAX_NUM; i++)
			hci_info->wp_seq[i] = WP_RESERVED_SEQ;

		/* pcie rx sw resource */
		rxch_num = rtw_hal_query_rxch_num(phl_info->hal);
		hci_info->total_rxch_num = rxch_num;

		pstatus = _phl_alloc_rxbd_pcie(phl_info, rxch_num);
		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;
		/* allocate wd page */
		pstatus = _phl_alloc_rxbuf_pool_pcie(phl_info, rxch_num);
		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;

	} while (false);

	if (RTW_PHL_STATUS_SUCCESS != pstatus)
		phl_trx_deinit_pcie(phl_info);
	else
		pstatus = _phl_update_default_rx_bd(phl_info);

	FUNCOUT_WSTS(pstatus);
	return pstatus;
}


enum rtw_phl_status phl_trx_config_pcie(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct hci_info_t *hci_info = phl_info->hci;

	do {
		hstatus = rtw_hal_trx_init(phl_info->hal, hci_info->txbd_buf,
						hci_info->rxbd_buf);
		if (RTW_HAL_STATUS_SUCCESS != hstatus) {
			PHL_ERR("rtw_hal_trx_init fail with status 0x%08X\n",
				hstatus);
			pstatus = RTW_PHL_STATUS_FAILURE;
			break;
		}
		else {
			pstatus = RTW_PHL_STATUS_SUCCESS;
		}

		phl_tx_start_pcie(phl_info);
		phl_rx_start_pcie(phl_info);

	} while (false);

	return pstatus;
}

#ifdef CONFIG_PHL_TXSC
u8 *_phl_txsc_apply_shortcut(struct phl_info_t *phl_info, struct rtw_xmit_req *tx_req,
							struct rtw_phl_stainfo_t *phl_sta, struct rtw_phl_pkt_req *phl_pkt_req)
{
	struct phl_txsc_entry *ptxsc = NULL;

	if (phl_sta == NULL)
		return (u8 *)ptxsc;

	if (tx_req->shortcut_id >= PHL_TXSC_ENTRY_NUM) {
		PHL_ERR("[PHL][TXSC] wrong shortcut_id:%d, plz check !!!\n", tx_req->shortcut_id);
		return (u8 *)ptxsc;
	}

	ptxsc = &phl_sta->phl_txsc[tx_req->shortcut_id];

	if ((tx_req->treq_type == RTW_PHL_TREQ_TYPE_CORE_TXSC)) {

		if (ptxsc == NULL) {
			PHL_ERR("[txsc][phl] fetal err: ptxsc = NULL, plz check.\n");
			return (u8 *)ptxsc;
		}

		if (!ptxsc->txsc_wd_cached) {
			PHL_ERR("[txsc][phl] fetal err: txsc_wd_cached = 0, plz check.\n");
			return (u8 *)ptxsc;
		}

		_os_mem_cpy(phl_info, phl_pkt_req->wd_page, ptxsc->txsc_wd_cache, ptxsc->txsc_wd_len);
		phl_pkt_req->wd_len = ptxsc->txsc_wd_len;

		/* update pktlen in wd_page, wd_body[8:15] = pktsize */
		#if 0
		packet_len = cpu_to_le16(tx_req->mdata.pktlen);
		_os_mem_cpy(phl_info, phl_pkt_req.wd_page+8, &packet_len, sizeof(u16));
		#endif

		ptxsc->txsc_cache_hit++;
	}

	return (u8 *)ptxsc;
}

enum rtw_phl_status
_phl_txsc_add_shortcut(struct phl_info_t *phl_info, struct rtw_xmit_req *tx_req,
								struct rtw_phl_pkt_req *phl_pkt_req, struct phl_txsc_entry *ptxsc)
{

	if (tx_req->shortcut_id >= PHL_TXSC_ENTRY_NUM) {
		PHL_ERR("[PHL][TXSC] wrong shortcut_id:%d, plz check.\n", tx_req->shortcut_id);
		return RTW_PHL_STATUS_FAILURE;
	}

	if (ptxsc == NULL) {
		PHL_ERR("[txsc][phl] fetal err: ptxsc = NULL, shortcut_id = %d, plz check.\n", tx_req->shortcut_id);
		return RTW_PHL_STATUS_FAILURE;
	}

	if (tx_req->treq_type & RTW_PHL_TREQ_TYPE_PHL_UPDATE_TXSC) {

		_os_mem_set(phl_info, ptxsc, 0x0, sizeof(struct phl_txsc_entry));
		_os_mem_cpy(phl_info, ptxsc->txsc_wd_cache, phl_pkt_req->wd_page, phl_pkt_req->wd_len);

		ptxsc->txsc_wd_len = phl_pkt_req->wd_len;
		ptxsc->txsc_wd_cached = true;

		#if 0
		PHL_PRINT("\n[txsc][phl] shortcut_id:%d, wd_page cached, len:%d. SMH: %u (%u)\n\n",
			tx_req->shortcut_id, ptxsc->txsc_wd_len, tx_req->mdata.smh_en,
			tx_req->treq_type);
		#endif

		tx_req->treq_type &= ~RTW_PHL_TREQ_TYPE_PHL_UPDATE_TXSC;
		if (tx_req->treq_type != RTW_PHL_TREQ_TYPE_PHL_ADD_TXSC)
			PHL_PRINT("Updated WD for request type %u\n", tx_req->treq_type);
	}

	return RTW_PHL_STATUS_SUCCESS;
}
#endif

enum rtw_phl_status
phl_prepare_tx_pcie(struct phl_info_t *phl_info, struct rtw_xmit_req *tx_req)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
 	struct rtw_wp_rpt_stats *rpt_stats =
		(struct rtw_wp_rpt_stats *)hal_com->trx_stat.wp_rpt_stats;
	struct hci_info_t *hci_info = NULL;
	struct rtw_pkt_buf_list *pkt_buf = NULL;
	struct rtw_wd_page_ring *wd_ring = NULL;
	struct rtw_wd_page *wd_page = NULL;
	struct rtw_phl_pkt_req phl_pkt_req;
	void *ptr = NULL;
	u16 packet_len = 0, wp_seq = 0;
	u8 dma_ch = 0, i = 0;
	u16 mid = 0;
#ifdef CONFIG_PHL_TXSC
	struct phl_txsc_entry *ptxsc = NULL;
	struct rtw_phl_stainfo_t *phl_sta = rtw_phl_get_stainfo_by_macid(phl_info, tx_req->mdata.macid);
#endif
	FUNCIN_WSTS(pstatus);
	do {
		if (NULL == phl_info->hci) {
			PHL_ERR("phl_info->hci is NULL!\n");
			break;
		}
		hci_info = (struct hci_info_t *)phl_info->hci;
		wd_ring = (struct rtw_wd_page_ring *)hci_info->wd_ring;

		if (NULL == tx_req)  {
			PHL_ERR("tx_req is NULL!\n");
			break;
		}
		mid = tx_req->mdata.macid;
		dma_ch = tx_req->mdata.dma_ch;
		wp_seq = wd_ring[dma_ch].wp_seq;

		if (NULL != wd_ring[dma_ch].wp_tag[wp_seq].ptr) {
			ptr = wd_ring[dma_ch].wp_tag[wp_seq].ptr;
			PHL_ERR("wp_tag out of resource!\n");
			PHL_ERR("stuck wp info:\n");
			PHL_ERR("dma_ch = %d, wp_seq = 0x%x, ptr = %p!\n",
				dma_ch, wp_seq, ptr);
			PHL_ERR("wifi seq = %d\n",
				((struct rtw_xmit_req *)ptr)->mdata.sw_seq);
			_phl_dump_busy_wp(phl_info);
			break;
		}

		pkt_buf = (struct rtw_pkt_buf_list *)&tx_req->pkt_list[0];
		for (i = 0; i < tx_req->pkt_cnt; i++) {
			packet_len += pkt_buf->length;
			pkt_buf++;
		}

		tx_req->total_len = packet_len;

		wd_page = query_idle_wd_page(phl_info, &wd_ring[dma_ch]);
		if (NULL == wd_page) {
			PHL_ERR("query idle wd page fail!\n");
			PHL_ERR("dma_ch = %d, idle wd num = %d, "
				"busy wd num = %d, pending wd num = %d\n",
				dma_ch,
				wd_ring[dma_ch].idle_wd_page_cnt,
				wd_ring[dma_ch].busy_wd_page_cnt,
				wd_ring[dma_ch].pending_wd_page_cnt);
			if (wd_ring[dma_ch].busy_wd_page_cnt > MAX_WD_PAGE_NUM * 4 / 5)
				rtw_hal_tx_dbg_status_dump(phl_info->hal);
			break;
		}
		/* hana_todo */
		_phl_fill_tx_meta_data(tx_req, packet_len);

		phl_pkt_req.wd_page = wd_page->vir_addr;

		phl_pkt_req.wp_seq = wp_seq;
		phl_pkt_req.tx_req = tx_req;

#ifdef CONFIG_PHL_TXSC
		phl_pkt_req.wd_len = 0;
		ptxsc = (struct phl_txsc_entry *)_phl_txsc_apply_shortcut(phl_info, tx_req, phl_sta, &phl_pkt_req);
#endif

		hstatus = rtw_hal_update_wd_page(phl_info->hal, &phl_pkt_req);
		wd_page->buf_len = phl_pkt_req.wd_len;

		if (RTW_HAL_STATUS_SUCCESS == hstatus) {
			hci_info->wp_seq[mid] = phl_pkt_req.wp_seq;
			enqueue_pending_wd_page(phl_info, &wd_ring[dma_ch],
						wd_page, _tail);
			tx_req->tx_time = _os_get_cur_time_ms();
#ifdef CONFIG_PHL_TX_DBG
			if (tx_req->tx_dbg.en_dbg) {
				tx_req->tx_dbg.enq_pending_wd_t =
						_os_get_cur_time_us();
			}
#endif /* CONFIG_PHL_TX_DBG */
			_os_spinlock(phl_to_drvpriv(phl_info),
				     &wd_ring[dma_ch].wp_tag_lock,
				     _bh, NULL);
			PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
				  "update tx req(%p) in ch(%d) with wp_seq(0x%x) and wifi seq(%d)!\n",
				  tx_req, dma_ch, wp_seq, tx_req->mdata.sw_seq);
			wd_ring[dma_ch].wp_tag[wp_seq].ptr = (u8 *)tx_req;
			rpt_stats[dma_ch].busy_cnt++;
			_os_spinunlock(phl_to_drvpriv(phl_info),
				       &wd_ring[dma_ch].wp_tag_lock,
				       _bh, NULL);

			wp_seq = (wp_seq + 1) % WP_MAX_SEQ_NUMBER;
			if (0 == wp_seq)
				wp_seq = 1;

			wd_ring[dma_ch].wp_seq = wp_seq;

			pstatus = RTW_PHL_STATUS_SUCCESS;

			//wb wd page
			if(wd_page->cache == VIRTUAL_ADDR) {
				PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "[%s] wd page cache wback \n",
					__FUNCTION__);
				_os_cache_wback(phl_to_drvpriv(phl_info),
					(_dma *)&wd_page->phy_addr_l,
					(_dma *)&wd_page->phy_addr_h,
					wd_page->buf_len, PCI_DMA_TODEVICE);
			}

#ifdef CONFIG_PHL_TXSC
			_phl_txsc_add_shortcut(phl_info, tx_req, &phl_pkt_req, ptxsc);
#endif

			break;
		} else {
			rtw_release_target_wd_page(phl_info, &wd_ring[dma_ch],
						wd_page);
			pstatus = RTW_PHL_STATUS_FAILURE;
			break;
		}
	} while(false);
	FUNCOUT_WSTS(pstatus);
	return pstatus;
}

static enum rtw_phl_status
phl_handle_pending_wd(struct phl_info_t *phl_info,
				struct rtw_wd_page_ring *wd_ring,
				u16 txcnt, u8 ch)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct tx_base_desc *txbd = NULL;
	struct rtw_wd_page *wd = NULL;
	u16 cnt = 0;

#ifdef RTW_WKARD_DYNAMIC_LTR
	if (true != _phl_judge_act_ltr_switching_conditions(phl_info, ch)) {
		_phl_act_ltr_update_stats(phl_info, false, ch,
		                          wd_ring->pending_wd_page_cnt);
		return RTW_PHL_STATUS_FAILURE;
	} else {
		_phl_act_ltr_update_stats(phl_info, true, ch,
		                          wd_ring->pending_wd_page_cnt);
	}
#endif

	txbd = (struct tx_base_desc *)hci_info->txbd_buf;
	while (txcnt > cnt) {
		wd = query_pending_wd_page(phl_info, wd_ring);

		if (NULL == wd) {
			pstatus = RTW_PHL_STATUS_RESOURCE;
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "query Tx pending WD fail!\n");
			break;
		}

		wd->ls = 1;//tmp set LS=1
		hstatus = rtw_hal_update_txbd(phl_info->hal, txbd, wd, ch, 1);
		if (RTW_HAL_STATUS_SUCCESS == hstatus) {
			enqueue_busy_wd_page(phl_info, wd_ring, wd, _tail);
			pstatus = RTW_PHL_STATUS_SUCCESS;
		} else {
			enqueue_pending_wd_page(phl_info, wd_ring, wd, _first);
			pstatus = RTW_PHL_STATUS_RESOURCE;
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "update Tx BD fail!\n");
			break;
		}

		cnt++;
	}

	if (RTW_PHL_STATUS_SUCCESS == pstatus) {
#ifdef RTW_WKARD_DYNAMIC_LTR
		_phl_switch_act_ltr(phl_info, ch);
#endif
		hstatus = rtw_hal_trigger_txstart(phl_info->hal, txbd, ch);
		if (RTW_HAL_STATUS_SUCCESS != hstatus) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "update txbd idx fail!\n");
			pstatus = RTW_PHL_STATUS_FAILURE;
		} else {
			#ifdef CONFIG_POWER_SAVE
			phl_ps_tx_pkt_ntfy(phl_info);
			#endif
			if (wd_ring->cur_hw_res > cnt)
				wd_ring->cur_hw_res -= cnt;
			else
				wd_ring->cur_hw_res = 0;
		}
	}

	return pstatus;
}


static enum rtw_phl_status
phl_handle_busy_wd(struct phl_info_t *phl_info,
                   struct rtw_wd_page_ring *wd_ring, u16 hw_idx)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	void *drv_priv = phl_to_drvpriv(phl_info);
	_os_list *list = &wd_ring->busy_wd_page_list;
	struct rtw_wd_page *wd = NULL;
	u16 bndy = (u16)hal_com->bus_cap.txbd_num;
	u16 target = 0;
	u16 release_num = 0;

	do {
		_os_spinlock(drv_priv, &wd_ring->busy_lock, _bh, NULL);

		if (list_empty(list)) {
			pstatus = RTW_PHL_STATUS_SUCCESS;
			_os_spinunlock(drv_priv, &wd_ring->busy_lock, _bh, NULL);
			break;
		}

		if (wd_ring->busy_wd_page_cnt > (bndy - 1)) {
			release_num = wd_ring->busy_wd_page_cnt - (bndy - 1);
			_os_spinunlock(drv_priv, &wd_ring->busy_lock, _bh, NULL);
			pstatus = rtw_release_busy_wd_page(phl_info, wd_ring,
								release_num);

			if (RTW_PHL_STATUS_SUCCESS != pstatus)
				break;
			else
				_os_spinlock(drv_priv, &wd_ring->busy_lock, _bh, NULL);
		}

		wd = list_first_entry(list, struct rtw_wd_page, list);
		target = wd->host_idx;

		if (hw_idx >= target)
			release_num = ((hw_idx - target) + 1) % bndy;
		else
			release_num = ((bndy - target) + (hw_idx + 1)) % bndy;

		_os_spinunlock(drv_priv, &wd_ring->busy_lock, _bh, NULL);

		pstatus = rtw_release_busy_wd_page(phl_info, wd_ring,
							release_num);

		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;
	} while (false);

	return pstatus;
}

enum rtw_phl_status phl_recycle_busy_wd(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_wd_page_ring *wd_ring = NULL;
	u16 hw_res = 0, host_idx = 0, hw_idx = 0;
	u8 ch = 0;
	FUNCIN_WSTS(pstatus);
	wd_ring = (struct rtw_wd_page_ring *)hci_info->wd_ring;

	for (ch = 0; ch < hci_info->total_txch_num; ch++) {
		hw_res = rtw_hal_tx_res_query(phl_info->hal, ch, &host_idx,
							&hw_idx);
		pstatus = phl_handle_busy_wd(phl_info, &wd_ring[ch], hw_idx);
	}

	FUNCOUT_WSTS(pstatus);
	return pstatus;
}

static enum rtw_phl_status
phl_handle_busy_h2c(struct phl_info_t *phl_info,
			struct phl_h2c_pkt_pool *h2c_pool, u16 hw_idx)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	void *drv_priv = phl_to_drvpriv(phl_info);
	struct phl_queue *queue = &h2c_pool->busy_h2c_pkt_list;
	_os_list *list = &h2c_pool->busy_h2c_pkt_list.queue;
	struct rtw_h2c_pkt *h2c = NULL;
	u16 bndy = (u16)hal_com->bus_cap.txbd_num;
	u16 target = 0;
	u16 release_num = 0;
	u16 tmp_cnt = 0;

	do {
		_os_spinlock(drv_priv, &queue->lock, _bh, NULL);

		if (list_empty(list)) {
			pstatus = RTW_PHL_STATUS_SUCCESS;

			_os_spinunlock(drv_priv, &queue->lock, _bh, NULL);
			break;
		}

		tmp_cnt = (u16)queue->cnt;
		if (tmp_cnt > (bndy - 1)) {
			release_num = tmp_cnt - (bndy - 1);
			_os_spinunlock(drv_priv, &queue->lock, _bh, NULL);
			pstatus = phl_release_busy_h2c_pkt(phl_info, h2c_pool,
							release_num);

			if (RTW_PHL_STATUS_SUCCESS != pstatus)
				break;
			else
				_os_spinlock(drv_priv, &queue->lock, _bh, NULL);
		}

		h2c = list_first_entry(list, struct rtw_h2c_pkt, list);
		target = h2c->host_idx;

		if (hw_idx >= target)
			release_num = ((hw_idx - target) + 1) % bndy;
		else
			release_num = ((bndy - target) + (hw_idx + 1)) % bndy;

		_os_spinunlock(drv_priv, &queue->lock, _bh, NULL);

		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "%s : release_num %d.\n", __func__, release_num);

		pstatus = phl_release_busy_h2c_pkt(phl_info, h2c_pool,
							release_num);

		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			break;
	} while (false);

	return pstatus;
}

enum rtw_phl_status phl_recycle_busy_h2c(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct phl_h2c_pkt_pool *h2c_pool = NULL;
	u16 hw_res = 0, host_idx = 0, hw_idx = 0;
	u8 fwcmd_queue_idx = 0;

	FUNCIN_WSTS(pstatus);
	h2c_pool = (struct phl_h2c_pkt_pool *)phl_info->h2c_pool;
	_os_spinlock(phl_to_drvpriv(phl_info), &h2c_pool->recycle_lock, _bh, NULL);
	fwcmd_queue_idx = rtw_hal_get_fwcmd_queue_idx(phl_info->hal);

	hw_res = rtw_hal_tx_res_query(phl_info->hal, fwcmd_queue_idx, &host_idx,
						&hw_idx);
	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "%s : host idx %d, hw_idx %d.\n",
			  __func__, host_idx, hw_idx);
	pstatus = phl_handle_busy_h2c(phl_info, h2c_pool, hw_idx);
	_os_spinunlock(phl_to_drvpriv(phl_info), &h2c_pool->recycle_lock, _bh, NULL);
	FUNCOUT_WSTS(pstatus);
	return pstatus;
}

static enum rtw_phl_status phl_tx_pcie(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_wd_page_ring *wd_ring = NULL;
	u16 hw_res = 0, host_idx = 0, hw_idx = 0, txcnt = 0;
	u8 ch = 0;
	FUNCIN_WSTS(pstatus);
	wd_ring = (struct rtw_wd_page_ring *)hci_info->wd_ring;

	for (ch = 0; ch < hci_info->total_txch_num; ch++) {
#ifndef RTW_WKARD_WIN_TRX_BALANCE
		/* if wd_ring is empty, do not read hw_idx for saving cpu cycle */
		if (wd_ring[ch].pending_wd_page_cnt == 0 && wd_ring[ch].busy_wd_page_cnt == 0){
			pstatus = RTW_PHL_STATUS_SUCCESS;
			continue;
		}
#endif
		/* hana_todo skip fwcmd queue */
		if (wd_ring[ch].cur_hw_res < hal_com->bus_cap.read_txbd_th ||
		    wd_ring[ch].pending_wd_page_cnt > wd_ring[ch].cur_hw_res) {
			hw_res = rtw_hal_tx_res_query(phl_info->hal, ch, &host_idx,
			                              &hw_idx);
			wd_ring[ch].cur_hw_res = hw_res;
			pstatus = phl_handle_busy_wd(phl_info, &wd_ring[ch], hw_idx);

			if (RTW_PHL_STATUS_FAILURE == pstatus)
				continue;
		} else {
			hw_res = wd_ring[ch].cur_hw_res;
		}

		if (list_empty(&wd_ring[ch].pending_wd_page_list))
			continue;

		if (0 == hw_res) {
			continue;

		} else {
			txcnt = (hw_res < wd_ring[ch].pending_wd_page_cnt) ?
				hw_res : wd_ring[ch].pending_wd_page_cnt;

			pstatus = phl_handle_pending_wd(phl_info, &wd_ring[ch],
							txcnt, ch);

			if (RTW_PHL_STATUS_SUCCESS != pstatus)
				continue;
		}
	}
	FUNCOUT_WSTS(pstatus);
	return pstatus;
}


enum rtw_phl_status _phl_refill_rxbd(struct phl_info_t *phl_info,
					void* rx_buf_ring,
					struct rx_base_desc *rxbd,
					u8 ch, u16 refill_cnt)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct rtw_rx_buf *rxbuf = NULL;
	u16 cnt = 0;

	for (cnt = 0; cnt < refill_cnt; cnt++) {
		rxbuf = query_idle_rx_buf(phl_info, rx_buf_ring);
		if (NULL == rxbuf) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"[WARNING] there is no resource for rx bd refill setting\n");
			pstatus = RTW_PHL_STATUS_RESOURCE;
			break;
		}
		hstatus = rtw_hal_update_rxbd(phl_info->hal, rxbd, rxbuf);
		if (RTW_HAL_STATUS_SUCCESS != hstatus) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"[WARNING] update rxbd fail\n");
			pstatus = RTW_PHL_STATUS_FAILURE;
			break;
		}
		enqueue_busy_rx_buf(phl_info, rx_buf_ring, rxbuf, _tail);
		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	/* hana_todo */
	/* wmb(); */

	if (cnt) {
		hstatus = rtw_hal_notify_rxdone(phl_info->hal, rxbd, ch, cnt);
		if (RTW_HAL_STATUS_SUCCESS != hstatus) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"[WARNING] notify rxdone fail\n");
			pstatus = RTW_PHL_STATUS_FAILURE;
		}
	}
	return pstatus;
}

enum rtw_phl_status phl_get_single_rx(struct phl_info_t *phl_info,
					 struct rtw_rx_buf_ring *rx_buf_ring,
					 u8 ch,
					 struct rtw_phl_rx_pkt **pphl_rx)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct rtw_phl_rx_pkt *phl_rx = NULL;
	struct rtw_rx_buf *rxbuf = NULL;
	struct hal_spec_t *hal_spec = phl_get_ic_spec(phl_info->phl_com);
	u16 buf_size = 0;

	phl_rx = rtw_phl_query_phl_rx(phl_info);
	rxbuf = query_busy_rx_buf(phl_info, rx_buf_ring);

	do {
		if (NULL == phl_rx) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s(%d) phl_rx out of resource\n",
				__func__, __LINE__);
				pstatus = RTW_PHL_STATUS_FAILURE;
			break;
		}
		if (NULL == rxbuf) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "%s(%d) [WARNING] queried NULL rxbuf\n",
				__func__, __LINE__);
				pstatus = RTW_PHL_STATUS_FAILURE;
			break;
		}

		phl_rx->rxbuf_ptr = (u8 *)rxbuf;

		if(rxbuf->cache == VIRTUAL_ADDR){
		_os_cache_inv(phl_to_drvpriv(phl_info),
			(_dma *)&rxbuf->phy_addr_l,
			(_dma *)&rxbuf->phy_addr_h,
			hal_spec->rx_bd_info_sz,
			PCI_DMA_FROMDEVICE);
		}
		if (true != rtw_hal_check_rxrdy(phl_info->phl_com,
						phl_info->hal,
						rxbuf->vir_addr,
						ch)) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "RX:%s(%d) packet not ready\n",
					__func__, __LINE__);
				pstatus = RTW_PHL_STATUS_FAILURE;
#ifdef RTW_WKARD_98D_RXTAG
			phl_rx->rxbuf_ptr = (u8 *)rxbuf;
			goto drop;
#endif
			break;
		}

		if (true != rtw_hal_handle_rxbd_info(phl_info->hal,
								rxbuf->vir_addr,
								&buf_size))
			goto drop;

#ifdef CONFIG_DYNAMIC_RX_BUF
		phl_rx->r.os_priv = rxbuf->os_priv;
#endif

		/* hana_todo handle rx amsdu */
		if(rxbuf->cache == VIRTUAL_ADDR){
		_os_cache_inv(phl_to_drvpriv(phl_info),
			(_dma *)&rxbuf->phy_addr_l,
			(_dma *)&rxbuf->phy_addr_h,
				buf_size,
			PCI_DMA_FROMDEVICE);
		}

		rxbuf->hw_write_size = buf_size;
		hstatus = rtw_hal_handle_rx_buffer(phl_info->phl_com,
							phl_info->hal,
							rxbuf->vir_addr,
							buf_size, phl_rx);

		if (RTW_HAL_STATUS_SUCCESS != hstatus)
			goto drop;

		pstatus = RTW_PHL_STATUS_SUCCESS;

	} while (false);

	if (RTW_PHL_STATUS_SUCCESS != pstatus) {
		/* hana_todo cache validate api */
		if (NULL != rxbuf) {
			enqueue_busy_rx_buf(phl_info, rx_buf_ring, rxbuf, _first);
		}

		if (NULL != phl_rx) {
			phl_release_phl_rx(phl_info, phl_rx);
			phl_rx = NULL;
		}
	}
	*pphl_rx = phl_rx;

	return pstatus;

drop:
#ifdef CONFIG_DYNAMIC_RX_BUF
	/* avoid re-allocating buffer carried on rxbuf */
	phl_rx->type = RTW_RX_TYPE_MAX;
#endif
	phl_rx->r.mdata.dma_ch = ch;
	phl_recycle_rx_buf(phl_info, phl_rx);

	return RTW_PHL_STATUS_FRAME_DROP;
}

#define PHL_RX_HEADROOM 0
void phl_rx_handle_normal(struct phl_info_t *phl_info,
						 struct rtw_phl_rx_pkt *phl_rx)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	_os_list frames;
#ifdef CONFIG_DYNAMIC_RX_BUF
	struct rtw_rx_buf *rxbuf = (struct rtw_rx_buf *)phl_rx->rxbuf_ptr;
#endif
	FUNCIN_WSTS(pstatus);
	INIT_LIST_HEAD(&frames);

	/* unmap rx buffer */
#ifdef CONFIG_DYNAMIC_RX_BUF
	_os_pkt_buf_unmap_rx(phl_to_drvpriv(phl_info), rxbuf->phy_addr_l,
		rxbuf->phy_addr_h, RX_BUF_SIZE+PHL_RX_HEADROOM);
#endif

	/* stat : rx rate counter */
	if (phl_rx->r.mdata.rx_rate <= RTW_DATA_RATE_HE_NSS4_MCS11)
		phl_info->phl_com->phl_stats.rx_rate_nmr[phl_rx->r.mdata.rx_rate]++;

	pstatus = phl_rx_reorder(phl_info, phl_rx, &frames);
	if (pstatus == RTW_PHL_STATUS_SUCCESS)
		phl_handle_rx_frame_list(phl_info, &frames);
	else
		PHL_TRACE(COMP_PHL_RECV, _PHL_WARNING_, "[WARNING]handle normal rx error (0x%08X)!\n", pstatus);

	FUNCOUT_WSTS(pstatus);
}

void _phl_wp_rpt_statistics(struct phl_info_t *phl_info, u8 ch, u16 wp_seq,
			    u8 txsts, struct rtw_xmit_req *treq)
{
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_wp_rpt_stats *rpt_stats = NULL;
	u32 diff_t = 0, cur_time = _os_get_cur_time_ms();

	rpt_stats = (struct rtw_wp_rpt_stats *)hal_com->trx_stat.wp_rpt_stats;

	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		  "recycle tx req(%p) in ch(%d) with wp_seq(0x%x) and wifi seq(%d)!\n",
		  treq, ch, wp_seq, treq->mdata.sw_seq);

#ifdef CONFIG_PHL_TX_DBG
	if (treq->tx_dbg.en_dbg) {
		treq->tx_dbg.recycle_wd_t = _os_get_cur_time_us();
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "tx dbg rpt: macid(%02d), tx_dbg_pkt_type(%02d), txsts(%d), sw_seq(%04d), total tx time(%08d) us\n",
			treq->mdata.macid, treq->tx_dbg.tx_dbg_pkt_type, txsts,
			treq->mdata.sw_seq, phl_get_passing_time_us(
			treq->tx_dbg.core_add_tx_t));
		PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "tx dbg rpt: core_add_tx_t(0x%08x), enq_pending_wd_t(0x%08x), recycle_wd_t(0x%08x)\n",
			treq->tx_dbg.core_add_tx_t,
			treq->tx_dbg.enq_pending_wd_t,
			treq->tx_dbg.recycle_wd_t);

		if(TX_STATUS_TX_DONE != txsts) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "tx dbg rpt: tx fail(%d)\n", txsts);
			if(NULL != treq->tx_dbg.statecb) {
				treq->tx_dbg.statecb(phl_to_drvpriv(phl_info), treq->tx_dbg.pctx, false);
			}
		} else {
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "tx dbg rpt: tx done(%d)\n", txsts);
			if(NULL != treq->tx_dbg.statecb) {
				treq->tx_dbg.statecb(phl_to_drvpriv(phl_info), treq->tx_dbg.pctx, true);
			}
		}
	}
#endif /* CONFIG_PHL_TX_DBG */

	if (cur_time >= treq->tx_time)
		diff_t = cur_time - treq->tx_time;
	else
		diff_t = RTW_U32_MAX - treq->tx_time + cur_time;

	if (diff_t > WP_DELAY_THRES_MS) {
		if (TX_STATUS_TX_DONE == txsts)
			rpt_stats[ch].delay_tx_ok_cnt++;
		else if (TX_STATUS_TX_FAIL_REACH_RTY_LMT == txsts)
			rpt_stats[ch].delay_rty_fail_cnt++;
		else if (TX_STATUS_TX_FAIL_LIFETIME_DROP == txsts)
			rpt_stats[ch].delay_lifetime_drop_cnt++;
		else if (TX_STATUS_TX_FAIL_MACID_DROP == txsts)
			rpt_stats[ch].delay_macid_drop_cnt++;
	} else {
		if (TX_STATUS_TX_DONE == txsts)
			rpt_stats[ch].tx_ok_cnt++;
		else if (TX_STATUS_TX_FAIL_REACH_RTY_LMT == txsts)
			rpt_stats[ch].rty_fail_cnt++;
		else if (TX_STATUS_TX_FAIL_LIFETIME_DROP == txsts)
			rpt_stats[ch].lifetime_drop_cnt++;
		else if (TX_STATUS_TX_FAIL_MACID_DROP == txsts)
			rpt_stats[ch].macid_drop_cnt++;
	}
}


void _phl_wp_rpt_chk_txsts(struct phl_info_t *phl_info, u8 ch, u16 wp_seq,
			    u8 txsts, struct rtw_xmit_req *treq)
{
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_wp_rpt_stats *rpt_stats = NULL;
	struct rtw_pkt_buf_list *pkt_buf = (struct rtw_pkt_buf_list *)treq->pkt_list;
	int i;

	rpt_stats = (struct rtw_wp_rpt_stats *)hal_com->trx_stat.wp_rpt_stats;

	if(TX_STATUS_TX_DONE != txsts) {
		if (TX_STATUS_TX_FAIL_REACH_RTY_LMT == txsts) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"this wp is tx fail (REACH_RTY_LMT): wp(%p), ch(%d), wp_seq(0x%x), macid(%d), Sw SN(%d), tid(%d), Rty_lmt_en/cnt(%d/%d)!\n",
				treq, ch, wp_seq, treq->mdata.macid,
				treq->mdata.sw_seq, treq->mdata.tid,
				treq->mdata.data_tx_cnt_lmt_en,
				treq->mdata.data_tx_cnt_lmt);
		} else if (TX_STATUS_TX_FAIL_LIFETIME_DROP == txsts) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"this wp is tx fail (LIFETIME_DROP): wp(%p), ch(%d), wp_seq(0x%x), macid(%d), Sw SN(%d), tid(%d), Rty_lmt_en/cnt(%d/%d)!\n",
				treq, ch, wp_seq, treq->mdata.macid,
				treq->mdata.sw_seq, treq->mdata.tid,
				treq->mdata.data_tx_cnt_lmt_en,
				treq->mdata.data_tx_cnt_lmt);
		} else if (TX_STATUS_TX_FAIL_MACID_DROP == txsts) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"this wp is tx fail (MACID_DROP): wp(%p), ch(%d), wp_seq(0x%x), macid(%d), Sw SN(%d), tid(%d), Rty_lmt_en/cnt(%d/%d)!\n",
				treq, ch, wp_seq, treq->mdata.macid,
				treq->mdata.sw_seq, treq->mdata.tid,
				treq->mdata.data_tx_cnt_lmt_en,
				treq->mdata.data_tx_cnt_lmt);
		} else if(TX_STATUS_TX_FAIL_SW_DROP == txsts) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"this wp is tx fail (SW_DROP): wp(%p), ch(%d), wp_seq(0x%x), macid(%d), Sw SN(%d), tid(%d), Rty_lmt_en/cnt(%d/%d)!\n",
				treq, ch, wp_seq, treq->mdata.macid,
				treq->mdata.sw_seq, treq->mdata.tid,
				treq->mdata.data_tx_cnt_lmt_en,
				treq->mdata.data_tx_cnt_lmt);
		} else {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
				"this wp is tx fail (UNKNOWN(%d)): wp(%p), ch(%d), wp_seq(0x%x), macid(%d), Sw SN(%d), tid(%d), Rty_lmt_en/cnt(%d/%d)!\n",
				txsts, treq, ch, wp_seq, treq->mdata.macid,
				treq->mdata.sw_seq, treq->mdata.tid,
				treq->mdata.data_tx_cnt_lmt_en,
				treq->mdata.data_tx_cnt_lmt);
		}

		/* dump tx fail mac hdr */
		if(MAC_HDR_LEN <= pkt_buf[0].length) {
			PHL_DATA(COMP_PHL_XMIT, _PHL_INFO_, "=== Dump Tx MAC HDR ===");
			for (i = 0; i < MAC_HDR_LEN; i++) {
				if (!(i % 8))
					PHL_DATA(COMP_PHL_XMIT, _PHL_INFO_, "\n");
				PHL_DATA(COMP_PHL_XMIT, _PHL_INFO_, "%02X ", pkt_buf[0].vir_addr[i]);
			}
			PHL_DATA(COMP_PHL_XMIT, _PHL_INFO_, "\n");
		}
	}

	if (treq->txfb) {
		treq->txfb->txsts = txsts;
		if (treq->txfb->txfb_cb)
			treq->txfb->txfb_cb(treq->txfb);
	}
}

void phl_recycle_payload(struct phl_info_t *phl_info, u8 dma_ch, u16 wp_seq,
			 u8 txsts)
{
	enum rtw_phl_status sts = RTW_PHL_STATUS_FAILURE;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_wp_rpt_stats *rpt_stats =
		(struct rtw_wp_rpt_stats *)hal_com->trx_stat.wp_rpt_stats;
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_phl_evt_ops *ops = &phl_info->phl_com->evt_ops;
	struct rtw_wd_page_ring *wd_ring = NULL;
	struct rtw_xmit_req *treq = NULL;
	u16 macid = 0;

	wd_ring = (struct rtw_wd_page_ring *)hci_info->wd_ring;
	treq = (struct rtw_xmit_req *)wd_ring[dma_ch].wp_tag[wp_seq].ptr;

	if (NULL == treq)
		goto end;

	macid = treq->mdata.macid;

	_phl_wp_rpt_statistics(phl_info, dma_ch, wp_seq, txsts, treq);
	_phl_wp_rpt_chk_txsts(phl_info, dma_ch, wp_seq, txsts, treq);

	if (RTW_PHL_TREQ_TYPE_TEST_PATTERN == treq->treq_type) {
		if (NULL == ops->tx_test_recycle)
			goto end;
		PHL_INFO("call tx_test_recycle\n");
		sts = ops->tx_test_recycle(phl_info, treq);
	} else if (RTW_PHL_TREQ_TYPE_NORMAL == treq->treq_type
#if defined(CONFIG_CORE_TXSC) || defined(CONFIG_PHL_TXSC)
		   || RTW_PHL_TREQ_TYPE_CORE_TXSC == treq->treq_type
		   || RTW_PHL_TREQ_TYPE_PHL_ADD_TXSC == treq->treq_type
#endif
	) {
		if (NULL == ops->tx_recycle)
			goto end;
		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "call tx_recycle\n");
		sts = ops->tx_recycle(phl_to_drvpriv(phl_info), treq);
	}

end:
	if (RTW_PHL_STATUS_SUCCESS != sts) {
		PHL_WARN("tx recycle fail\n");
		rpt_stats[dma_ch].recycle_fail_cnt++;
	} else {
		_os_spinlock(phl_to_drvpriv(phl_info),
			     &wd_ring[dma_ch].wp_tag_lock,
			     _bh, NULL);
		wd_ring[dma_ch].wp_tag[wp_seq].ptr = NULL;
		rpt_stats[dma_ch].busy_cnt--;

#ifdef RTW_WKARD_DYNAMIC_LTR
		if (true ==
		    _phl_judge_idle_ltr_switching_conditions(phl_info, macid))
			_phl_switch_idle_ltr(phl_info, rpt_stats);
#endif
		_os_spinunlock(phl_to_drvpriv(phl_info),
			       &wd_ring[dma_ch].wp_tag_lock,
			       _bh, NULL);
	}
	/* phl_indic_pkt_complete(phl_info); */
}

/**
 * Use this function to check if this report is valid
 * @wp_seq: the wp sequence parsing from target report
 * @phl_rx: rx structure including the starting vir_addr of whole rxbuf
 * return true if this recycle report is valid
 * otherwise, execute rx buffer debug dump and return false
 */
bool _phl_chk_wp_report(u16 wp_seq, struct rtw_phl_rx_pkt *phl_rx)
{
	bool ret = false;
	u32 i = 0, len = 0;
	struct rtw_rx_buf *rxbuf = (struct rtw_rx_buf *)phl_rx->rxbuf_ptr;

	if (wp_seq < WP_MAX_SEQ_NUMBER) {
		ret = true;
	} else {
		ret = false;

		PHL_WARN("ERROR Recycle Report!! get recycle report with invalid wp_seq(%d), skip this offest and get next offset\n",
		         wp_seq);
		PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "=== Dump ERROR Recycle Report ===");
		len = (rxbuf->hw_write_size > 0x200) ?
			0x200 : rxbuf->hw_write_size;
		for (i = 0; i < len; i++) {
			if (!(i % 8))
				PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "\n");
			PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "%02X ",
			         rxbuf->vir_addr[i]);
		}
		PHL_DATA(COMP_PHL_DBG, _PHL_INFO_, "\n");
	}

	return ret;
}

void _phl_rx_handle_wp_report(struct phl_info_t *phl_info,
                              struct rtw_phl_rx_pkt *phl_rx)
{
	struct rtw_recv_pkt *r = &phl_rx->r;
	u8 *pkt = NULL;
	u16 pkt_len = 0;
	u16 wp_seq = 0, rsize = 0;
	u8 sw_retry = 0, dma_ch = 0, txsts = 0;
	u8 macid = 0, ac_queue = 0;

	pkt = r->pkt_list[0].vir_addr;
	pkt_len = r->pkt_list[0].length;

	while (pkt_len > 0) {
		rsize = rtw_hal_handle_wp_rpt(phl_info->hal, pkt, pkt_len,
					      &sw_retry, &dma_ch, &wp_seq,
					      &macid, &ac_queue, &txsts);

		if (0 == rsize)
			break;

		if (false == _phl_chk_wp_report(wp_seq, phl_rx)) {
			pkt += rsize;
			pkt_len -= rsize;
			continue;
		}

		#ifdef CONFIG_PHL_RELEASE_RPT_ENABLE
		phl_rx_wp_report_record_sts(phl_info, macid, ac_queue, txsts);
		#endif

		if (false == sw_retry) {
			phl_recycle_payload(phl_info, dma_ch, wp_seq, txsts);
		} else {
			/* hana_todo handle sw retry */
			phl_recycle_payload(phl_info, dma_ch, wp_seq, txsts);
		}
		pkt += rsize;
		pkt_len -= rsize;
	}
}


static void phl_rx_process_pcie(struct phl_info_t *phl_info,
							struct rtw_phl_rx_pkt *phl_rx)
{
	switch (phl_rx->type) {
	case RTW_RX_TYPE_WIFI:
#ifdef CONFIG_PHL_RX_PSTS_PER_PKT
		if (false == phl_rx_proc_wait_phy_sts(phl_info, phl_rx)) {
			PHL_TRACE(COMP_PHL_PSTS, _PHL_DEBUG_,
				  "phl_rx_proc_wait_phy_sts() return false \n");
			phl_rx_handle_normal(phl_info, phl_rx);
		}
#else
		phl_rx_handle_normal(phl_info, phl_rx);
#endif
		break;
	case RTW_RX_TYPE_TX_WP_RELEASE_HOST:
		_phl_rx_handle_wp_report(phl_info, phl_rx);
		phl_recycle_rx_buf(phl_info, phl_rx);
		break;
	case RTW_RX_TYPE_PPDU_STATUS:
		phl_rx_proc_ppdu_sts(phl_info, phl_rx);
#ifdef CONFIG_PHL_RX_PSTS_PER_PKT
		phl_rx_proc_phy_sts(phl_info, phl_rx);
#endif
		phl_recycle_rx_buf(phl_info, phl_rx);
		break;
	case RTW_RX_TYPE_C2H:
		phl_recycle_rx_buf(phl_info, phl_rx);
		break;
	case RTW_RX_TYPE_CHANNEL_INFO:
	case RTW_RX_TYPE_TX_RPT:
	case RTW_RX_TYPE_DFS_RPT:
	case RTW_RX_TYPE_MAX:
		PHL_TRACE(COMP_PHL_RECV, _PHL_WARNING_, "phl_rx_process_pcie(): Unsupported case:%d, please check it\n",
				phl_rx->type);
		phl_recycle_rx_buf(phl_info, phl_rx);
		break;
	default :
		PHL_TRACE(COMP_PHL_RECV, _PHL_WARNING_, "[WARNING] unrecognize rx type\n");
		phl_recycle_rx_buf(phl_info, phl_rx);
		break;
	}
}

static u16 _phl_get_idle_rxbuf_cnt(struct phl_info_t *phl_info,
					struct rtw_rx_buf_ring *rx_buf_ring)
{
	return rx_buf_ring->idle_rxbuf_cnt;
}

static enum rtw_phl_status phl_rx_pcie(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rtw_hal_com_t *hal_com = rtw_hal_get_halcom(phl_info->hal);
	struct rtw_rx_buf_ring *rx_buf_ring = NULL;
	struct rx_base_desc *rxbd = NULL;
	struct rtw_phl_rx_pkt *phl_rx = NULL;
	u16 i = 0, rxcnt = 0, idle_rxbuf_cnt = 0;
	u8 ch = 0;

	FUNCIN_WSTS(pstatus);

	rx_buf_ring = (struct rtw_rx_buf_ring *)hci_info->rxbuf_pool;
	rxbd = (struct rx_base_desc *)hci_info->rxbd_buf;

	for (ch = 0; ch < hci_info->total_rxch_num; ch++) {
		rxcnt = phl_calc_avail_rptr(rxbd[ch].host_idx, rxbd[ch].hw_idx,
		                            (u16)hal_com->bus_cap.rxbd_num);
		if (rxcnt == 0) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_,
				"no avail hw rx\n");
			pstatus = RTW_PHL_STATUS_SUCCESS;
			continue;
		}

		idle_rxbuf_cnt = _phl_get_idle_rxbuf_cnt(phl_info,
							 &rx_buf_ring[ch]);

		if (idle_rxbuf_cnt == 0) {
			PHL_WARN("%s, idle rxbuf is empty. (ch = %d)\n",
				 __func__, ch);
			phl_dump_all_sta_rx_info(phl_info);
			PHL_INFO("phl_rx_ring stored rx number = %d\n",
				 rtw_phl_query_new_rx_num(phl_info));
#ifdef PHL_RX_BATCH_IND
			if (ch == 0)
				_phl_indic_new_rxpkt(phl_info);
#endif
			pstatus = RTW_PHL_STATUS_SUCCESS;
			continue;
		}

		/* only handle affordable amount of rxpkt */
		if (rxcnt > idle_rxbuf_cnt) {
			PHL_WARN("rxcnt %d is lager than idle rxbuf cnt %d.\n", rxcnt, idle_rxbuf_cnt);
			rxcnt = idle_rxbuf_cnt;
		}

		for (i = 0; i < rxcnt; i++) {
			pstatus = phl_get_single_rx(phl_info, &rx_buf_ring[ch],
							ch, &phl_rx);
			if (RTW_PHL_STATUS_FRAME_DROP == pstatus)
				continue;
			if (NULL == phl_rx) {
				rxcnt = i;
				break;
			}

			/* hana_todo */
			phl_rx->r.mdata.dma_ch = ch;
			phl_rx_process_pcie(phl_info, phl_rx);
		}

#ifdef PHL_RX_BATCH_IND
		if (ch == 0 && phl_info->rx_new_pending)
			_phl_indic_new_rxpkt(phl_info);
#endif

		pstatus = _phl_refill_rxbd(phl_info, &rx_buf_ring[ch],
							&rxbd[ch], ch, rxcnt);

		if (RTW_PHL_STATUS_RESOURCE == pstatus)
			PHL_WARN("%s, rxcnt is not refilled %d.\n", __func__ , rxcnt);

		if (RTW_PHL_STATUS_SUCCESS != pstatus)
			continue;
	}

	FUNCOUT_WSTS(pstatus);

	return pstatus;
}

enum rtw_phl_status phl_pltfm_tx_pcie(struct phl_info_t *phl_info, void *pkt)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct rtw_h2c_pkt *h2c_pkt = (struct rtw_h2c_pkt *)pkt;
	struct tx_base_desc *txbd = NULL;
	struct phl_h2c_pkt_pool *h2c_pool = NULL;
	struct rtw_wd_page wd;
	u8 fwcmd_queue_idx = 0;

	txbd = (struct tx_base_desc *)phl_info->hci->txbd_buf;
	h2c_pool = (struct phl_h2c_pkt_pool *)phl_info->h2c_pool;

	_os_mem_set(phl_to_drvpriv(phl_info), &wd, 0, sizeof(wd));
	/* fowart h2c pkt information into the format of wd page */
	wd.phy_addr_l = h2c_pkt->phy_addr_l;
	wd.phy_addr_h= h2c_pkt->phy_addr_h;
	wd.buf_len = h2c_pkt->data_len;
	wd.ls = 1;

	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "%s : wd.phy_addr_l %x, wd.phy_addr_h %x\n", __func__ , wd.phy_addr_l, wd.phy_addr_h);
	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "%s : buf_len %x.\n", __func__ , wd.buf_len);

	fwcmd_queue_idx = rtw_hal_get_fwcmd_queue_idx(phl_info->hal);

	_os_spinlock(phl_to_drvpriv(phl_info), &txbd[fwcmd_queue_idx].txbd_lock, _bh, NULL);
	hstatus = rtw_hal_update_txbd(phl_info->hal, txbd, &wd, fwcmd_queue_idx, 1);
	_os_spinunlock(phl_to_drvpriv(phl_info), &txbd[fwcmd_queue_idx].txbd_lock, _bh, NULL);

	h2c_pkt->host_idx = wd.host_idx;

	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "%s : h2c_pkt->host_idx %d.\n", __func__, h2c_pkt->host_idx);

	if (RTW_HAL_STATUS_SUCCESS == hstatus)
			pstatus = phl_enqueue_busy_h2c_pkt(phl_info, h2c_pkt, _tail);

	if (RTW_PHL_STATUS_SUCCESS == pstatus) {
		_os_spinlock(phl_to_drvpriv(phl_info), &txbd[fwcmd_queue_idx].txbd_lock, _bh, NULL);
		hstatus = rtw_hal_trigger_txstart(phl_info->hal, txbd, fwcmd_queue_idx);
		_os_spinunlock(phl_to_drvpriv(phl_info), &txbd[fwcmd_queue_idx].txbd_lock, _bh, NULL);
		if (RTW_HAL_STATUS_SUCCESS != hstatus) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]update Tx RW ptr fail!\n");
			pstatus = RTW_PHL_STATUS_FAILURE;
		}
	}

	return pstatus;
}

void *phl_get_txbd_buf_pcie(struct phl_info_t *phl_info)
{
	struct hci_info_t *hci_info = phl_info->hci;

	return hci_info->txbd_buf;
}

void *phl_get_rxbd_buf_pcie(struct phl_info_t *phl_info)
{
	struct hci_info_t *hci_info = phl_info->hci;

	return hci_info->rxbd_buf;
}

void phl_recycle_rx_pkt_pcie(struct phl_info_t *phl_info,
				struct rtw_phl_rx_pkt *phl_rx)
{
#ifdef CONFIG_DYNAMIC_RX_BUF
	struct rtw_rx_buf *rx_buf = (struct rtw_rx_buf *)phl_rx->rxbuf_ptr;

	rx_buf->reuse = true;
#endif

	phl_recycle_rx_buf(phl_info, phl_rx);
}

enum rtw_phl_status phl_register_trx_hdlr_pcie(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;
	struct rtw_phl_handler *tx_handler = &phl_info->phl_tx_handler;
	struct rtw_phl_handler *rx_handler = &phl_info->phl_rx_handler;
	void *drv_priv = phl_to_drvpriv(phl_info);

	tx_handler->type = RTW_PHL_HANDLER_PRIO_HIGH; /* tasklet */
	tx_handler->callback = _phl_tx_callback_pcie;
	tx_handler->context = phl_info;
	tx_handler->drv_priv = drv_priv;
	pstatus = phl_register_handler(phl_info->phl_com, tx_handler);
	if (RTW_PHL_STATUS_SUCCESS != pstatus)
		PHL_ERR("%s : register tx_handler fail.\n", __FUNCTION__);

	rx_handler->type = RTW_PHL_HANDLER_PRIO_HIGH;
	rx_handler->callback = _phl_rx_callback_pcie;
	rx_handler->context = phl_info;
	rx_handler->drv_priv = drv_priv;
	pstatus = phl_register_handler(phl_info->phl_com, rx_handler);
	if (RTW_PHL_STATUS_SUCCESS != pstatus)
		PHL_ERR("%s : register rx_handler fail.\n", __FUNCTION__);

	return pstatus;
}

void
phl_tx_watchdog_pcie(struct phl_info_t *phl_info)
{
	struct rtw_stats *phl_stats = NULL;

	phl_stats = &phl_info->phl_com->phl_stats;

	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		  "\n=== Tx statistics === \n");
	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		"\nunicast tx bytes	: %llu\n", phl_stats->tx_byte_uni);
	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		"total tx bytes		: %llu\n", phl_stats->tx_byte_total);
	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		 "tx throughput		: %d(kbps)\n",
			 (int)phl_stats->tx_tp_kbits);
	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		"last tx time		: %d(ms)\n",
			 (int)phl_stats->last_tx_time_ms);
	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		"tx request num to phl	: %d\n",
			 (int)phl_stats->txreq_num);

	#ifdef RTW_WKARD_DYNAMIC_LTR
	if (rtw_hal_ltr_is_sw_ctrl(phl_info->phl_com, phl_info->hal)) {
		PHL_INFO(
			"ltr sw ctrl 			: %u\n",
			rtw_hal_ltr_is_sw_ctrl(phl_info->phl_com, phl_info->hal) ? 1 : 0);
		PHL_INFO(
			"ltr current state 		: %u\n",
			phl_ltr_get_cur_state(phl_info->phl_com));
		PHL_INFO(
			"ltr active trigger cnt : %lu\n",
			phl_ltr_get_tri_cnt(phl_info->phl_com, RTW_PCIE_LTR_SW_ACT));
		PHL_INFO(
			"ltr idle trigger cnt   : %lu\n",
			phl_ltr_get_tri_cnt(phl_info->phl_com, RTW_PCIE_LTR_SW_IDLE));
		PHL_INFO(
			"ltr last trigger time  : %lu\n",
			phl_ltr_get_last_trigger_time(phl_info->phl_com));
	}
	#endif

	_phl_dump_wp_stats(phl_info);

	_phl_dump_busy_wp(phl_info);

	PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		  "\n===================== \n");

}

void
phl_read_hw_rx(struct phl_info_t *phl_info, enum rx_channel_type rx_ch)
{
	struct hci_info_t *hci_info = (struct hci_info_t *)phl_info->hci;
	struct rx_base_desc *rxbd = NULL;
	u16 host_idx = 0;

	if (rx_ch >= hci_info->total_rxch_num)
		return;

	rxbd = (struct rx_base_desc *)hci_info->rxbd_buf;
	rtw_hal_rx_res_query(phl_info->hal, rx_ch, &host_idx,
	                     &rxbd[rx_ch].hw_idx);
}

static struct phl_hci_trx_ops ops= {0};
void phl_hci_trx_ops_init(void)
{
	ops.hci_trx_init = phl_trx_init_pcie;
	ops.hci_trx_deinit = phl_trx_deinit_pcie;
	ops.prepare_tx = phl_prepare_tx_pcie;
	ops.recycle_rx_buf = phl_release_target_rx_buf;
	ops.tx = phl_tx_pcie;
	ops.rx = phl_rx_pcie;
	ops.trx_cfg = phl_trx_config_pcie;
	ops.trx_stop = phl_trx_stop_pcie;
	ops.recycle_busy_wd = phl_recycle_busy_wd;
	ops.recycle_busy_h2c = phl_recycle_busy_h2c;
	ops.read_hw_rx = phl_read_hw_rx;
	ops.pltfm_tx = phl_pltfm_tx_pcie;
	ops.alloc_h2c_pkt_buf = _phl_alloc_h2c_pkt_buf_pcie;
	ops.free_h2c_pkt_buf = _phl_free_h2c_pkt_buf_pcie;
	ops.trx_reset = phl_trx_reset_pcie;
	ops.trx_resume = phl_trx_resume_pcie;
	ops.req_tx_stop = phl_req_tx_stop_pcie;
	ops.req_rx_stop = phl_req_rx_stop_pcie;
	ops.is_tx_pause = phl_is_tx_sw_pause_pcie;
	ops.is_rx_pause = phl_is_rx_sw_pause_pcie;
	ops.get_txbd_buf = phl_get_txbd_buf_pcie;
	ops.get_rxbd_buf = phl_get_rxbd_buf_pcie;
	ops.recycle_rx_pkt = phl_recycle_rx_pkt_pcie;
	ops.register_trx_hdlr = phl_register_trx_hdlr_pcie;
	ops.rx_handle_normal = phl_rx_handle_normal;
	ops.tx_watchdog = phl_tx_watchdog_pcie;
}


enum rtw_phl_status phl_hook_trx_ops_pci(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

	if (NULL != phl_info) {
		phl_hci_trx_ops_init();
		phl_info->hci_trx_ops = &ops;
		pstatus = RTW_PHL_STATUS_SUCCESS;
	}

	return pstatus;
}

enum rtw_phl_status phl_cmd_set_l2_leave(struct phl_info_t *phl_info)
{
	enum rtw_phl_status pstatus = RTW_PHL_STATUS_FAILURE;

#ifdef CONFIG_CMD_DISP
	pstatus = phl_cmd_enqueue(phl_info, HW_BAND_0, MSG_EVT_HAL_SET_L2_LEAVE, NULL, 0, NULL, PHL_CMD_WAIT, 0);

	if (is_cmd_failure(pstatus)) {
		/* Send cmd success, but wait cmd fail*/
		pstatus = RTW_PHL_STATUS_FAILURE;
	} else if (pstatus != RTW_PHL_STATUS_SUCCESS) {
		/* Send cmd fail */
		pstatus = RTW_PHL_STATUS_FAILURE;
	}
#else
	if (rtw_hal_set_l2_leave(phl_info->hal) == RTW_HAL_STATUS_SUCCESS)
		pstatus = RTW_PHL_STATUS_SUCCESS;
#endif
	return pstatus;
}
