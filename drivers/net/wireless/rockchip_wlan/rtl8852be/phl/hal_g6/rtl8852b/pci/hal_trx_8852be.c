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
#define _HAL_TRX_8852BE_C_
#include "../../hal_headers.h"
#include "../rtl8852b_hal.h"
#include "hal_trx_8852be.h"

/**
 * this function will query total hw tx dma channels number
 *
 * returns the number of hw tx dma channel
 */
static u8 hal_query_txch_num_8852be(void)
{
	u8 ch_num = 0;

	ch_num = TX_DMA_CHANNEL_ENTRY_8852BE;

	return ch_num;
}

/**
 * this function will query total hw rx dma channels number
 *
 * returns the number of hw rx dma channel
 */
static u8 hal_query_rxch_num_8852be(void)
{
	u8 ch_num = 0;

	ch_num = RX_DMA_CHANNEL_ENTRY_8852BE;

	return ch_num;
}
static u8 hal_qsel_to_tid_8852be(struct hal_info_t *hal, u8 qsel_id, u8 tid_indic)
{
	u8 tid = 0;

	switch (qsel_id) {
		case RTW_TXDESC_QSEL_BE_0:
		case RTW_TXDESC_QSEL_BE_1:
		case RTW_TXDESC_QSEL_BE_2:
		case RTW_TXDESC_QSEL_BE_3:
			tid = (1 == tid_indic) ? RTW_PHL_RING_CAT_TID3 : RTW_PHL_RING_CAT_TID0;
			break;
		case RTW_TXDESC_QSEL_BK_0:
		case RTW_TXDESC_QSEL_BK_1:
		case RTW_TXDESC_QSEL_BK_2:
		case RTW_TXDESC_QSEL_BK_3:
			tid = (1 == tid_indic) ? RTW_PHL_RING_CAT_TID2 : RTW_PHL_RING_CAT_TID1;
			break;
		case RTW_TXDESC_QSEL_VI_0:
		case RTW_TXDESC_QSEL_VI_1:
		case RTW_TXDESC_QSEL_VI_2:
		case RTW_TXDESC_QSEL_VI_3:
			tid = (1 == tid_indic) ? RTW_PHL_RING_CAT_TID5 : RTW_PHL_RING_CAT_TID4;
			break;
		case RTW_TXDESC_QSEL_VO_0:
		case RTW_TXDESC_QSEL_VO_1:
		case RTW_TXDESC_QSEL_VO_2:
		case RTW_TXDESC_QSEL_VO_3:
			tid = (1 == tid_indic) ? RTW_PHL_RING_CAT_TID7 : RTW_PHL_RING_CAT_TID6;
			break;
		case RTW_TXDESC_QSEL_MGT_0:
		case RTW_TXDESC_QSEL_MGT_1:
			tid = RTW_PHL_RING_CAT_MGNT;
			break;
		case RTW_TXDESC_QSEL_HIGH_0:
		case RTW_TXDESC_QSEL_HIGH_1:
			tid = RTW_PHL_RING_CAT_HIQ;
 			break;
		default :
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]unknown qsel_id (%d)\n",
					qsel_id);
			tid = 0;
			break;
	}

	return tid;
}

/**
 * Get target DMA channel's BD ram hw register of rtl8852b
 * @dma_ch: input target dma channel with the hw definition of rtl8852BE
 * return the BD ram hw register
 */
static u32 _hal_get_bd_ram_reg_8852be(u8 dma_ch)
{
	u32 reg = 0;

	switch (dma_ch) {
	case ACH0_QUEUE_IDX_8852BE:
		reg = R_AX_ACH0_BDRAM_CTRL;
		break;
	case ACH1_QUEUE_IDX_8852BE:
		reg = R_AX_ACH1_BDRAM_CTRL;
		break;
	case ACH2_QUEUE_IDX_8852BE:
		reg = R_AX_ACH2_BDRAM_CTRL;
		break;
	case ACH3_QUEUE_IDX_8852BE:
		reg = R_AX_ACH3_BDRAM_CTRL;
		break;
	case ACH4_QUEUE_IDX_8852BE:
		reg = R_AX_ACH4_BDRAM_CTRL;
		break;
	case ACH5_QUEUE_IDX_8852BE:
		reg = R_AX_ACH5_BDRAM_CTRL;
		break;
	case ACH6_QUEUE_IDX_8852BE:
		reg = R_AX_ACH6_BDRAM_CTRL;
		break;
	case ACH7_QUEUE_IDX_8852BE:
		reg = R_AX_ACH7_BDRAM_CTRL;
		break;
	case MGQ_B0_QUEUE_IDX_8852BE:
		reg = R_AX_CH8_BDRAM_CTRL;
		break;
	case HIQ_B0_QUEUE_IDX_8852BE:
		reg = R_AX_CH9_BDRAM_CTRL;
		break;
	case MGQ_B1_QUEUE_IDX_8852BE:
		reg = R_AX_CH10_BDRAM_CTRL;
		break;
	case HIQ_B1_QUEUE_IDX_8852BE:
		reg = R_AX_CH11_BDRAM_CTRL;
		break;
	case FWCMD_QUEUE_IDX_8852BE:
		reg = R_AX_CH12_BDRAM_CTRL;
		break;
	default :
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]unknown channel (%d)\n",
				dma_ch);
		reg = 0xFFFF;
		break;
	}

	return reg;
}



/**
 * Get target DMA channel's BD num hw register of rtl8852b
 * @dma_ch: input target dma channel with the hw definition of rtl8852BE
 * return the BD num hw register
 */
static u32 _hal_get_bd_num_reg_8852be(u8 dma_ch)
{
	u32 reg = 0;

	switch (dma_ch) {
	case ACH0_QUEUE_IDX_8852BE:
		reg = R_AX_ACH0_TXBD_NUM;
		break;
	case ACH1_QUEUE_IDX_8852BE:
		reg = R_AX_ACH1_TXBD_NUM;
		break;
	case ACH2_QUEUE_IDX_8852BE:
		reg = R_AX_ACH2_TXBD_NUM;
		break;
	case ACH3_QUEUE_IDX_8852BE:
		reg = R_AX_ACH3_TXBD_NUM;
		break;
	case ACH4_QUEUE_IDX_8852BE:
		reg = R_AX_ACH4_TXBD_NUM;
		break;
	case ACH5_QUEUE_IDX_8852BE:
		reg = R_AX_ACH5_TXBD_NUM;
		break;
	case ACH6_QUEUE_IDX_8852BE:
		reg = R_AX_ACH6_TXBD_NUM;
		break;
	case ACH7_QUEUE_IDX_8852BE:
		reg = R_AX_ACH7_TXBD_NUM;
		break;
	case MGQ_B0_QUEUE_IDX_8852BE:
		reg = R_AX_CH8_TXBD_NUM;
		break;
	case HIQ_B0_QUEUE_IDX_8852BE:
		reg = R_AX_CH9_TXBD_NUM;
		break;
	case MGQ_B1_QUEUE_IDX_8852BE:
		reg = R_AX_CH10_TXBD_NUM;
		break;
	case HIQ_B1_QUEUE_IDX_8852BE:
		reg = R_AX_CH11_TXBD_NUM;
		break;
	case FWCMD_QUEUE_IDX_8852BE:
		reg = R_AX_CH12_TXBD_NUM;
		break;
	case RX_QUEUE_IDX_8852BE:
		reg = R_AX_RXQ_RXBD_NUM;
		break;
	case RP_QUEUE_IDX_8852BE:
		reg = R_AX_RPQ_RXBD_NUM;
		break;
	default :
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]unknown channel (%d)\n",
				dma_ch);
		reg = 0xFFFF;
		break;
	}

	return reg;
}


/**
 * Get target DMA channel's BD Desc hw register of rtl8852b
 * @dma_ch: input target dma channel with the hw definition of rtl8852BE
 * return the BD Desc hw register
 */
static void _hal_get_bd_desc_reg_8852be(u8 dma_ch, u32 *addr_l, u32 *addr_h)
{
	u32 reg = 0;

	switch (dma_ch) {
	case ACH0_QUEUE_IDX_8852BE:
		*addr_l = R_AX_ACH0_TXBD_DESA_L;
		*addr_h = R_AX_ACH0_TXBD_DESA_H;
		break;
	case ACH1_QUEUE_IDX_8852BE:
		*addr_l = R_AX_ACH1_TXBD_DESA_L;
		*addr_h = R_AX_ACH1_TXBD_DESA_H;
		break;
	case ACH2_QUEUE_IDX_8852BE:
		*addr_l = R_AX_ACH2_TXBD_DESA_L;
		*addr_h = R_AX_ACH2_TXBD_DESA_H;
		break;
	case ACH3_QUEUE_IDX_8852BE:
		*addr_l = R_AX_ACH3_TXBD_DESA_L;
		*addr_h = R_AX_ACH3_TXBD_DESA_H;
		break;
	case ACH4_QUEUE_IDX_8852BE:
		*addr_l = R_AX_ACH4_TXBD_DESA_L;
		*addr_h = R_AX_ACH4_TXBD_DESA_H;
		break;
	case ACH5_QUEUE_IDX_8852BE:
		*addr_l = R_AX_ACH5_TXBD_DESA_L;
		*addr_h = R_AX_ACH5_TXBD_DESA_H;
		break;
	case ACH6_QUEUE_IDX_8852BE:
		*addr_l = R_AX_ACH6_TXBD_DESA_L;
		*addr_h = R_AX_ACH6_TXBD_DESA_H;
		break;
	case ACH7_QUEUE_IDX_8852BE:
		*addr_l = R_AX_ACH7_TXBD_DESA_L;
		*addr_h = R_AX_ACH7_TXBD_DESA_H;
		break;
	case MGQ_B0_QUEUE_IDX_8852BE:
		*addr_l = R_AX_CH8_TXBD_DESA_L;
		*addr_h = R_AX_CH8_TXBD_DESA_H;
		break;
	case HIQ_B0_QUEUE_IDX_8852BE:
		*addr_l = R_AX_CH9_TXBD_DESA_L;
		*addr_h = R_AX_CH9_TXBD_DESA_H;
		break;
	case MGQ_B1_QUEUE_IDX_8852BE:
		*addr_l = R_AX_CH10_TXBD_DESA_L;
		*addr_h = R_AX_CH10_TXBD_DESA_H;
		break;
	case HIQ_B1_QUEUE_IDX_8852BE:
		*addr_l = R_AX_CH11_TXBD_DESA_L;
		*addr_h = R_AX_CH11_TXBD_DESA_H;
		break;
	case FWCMD_QUEUE_IDX_8852BE:
		*addr_l = R_AX_CH12_TXBD_DESA_L;
		*addr_h = R_AX_CH12_TXBD_DESA_H;
		break;
	case RX_QUEUE_IDX_8852BE:
		*addr_l = R_AX_RXQ_RXBD_DESA_L;
		*addr_h = R_AX_RXQ_RXBD_DESA_H;
		break;
	case RP_QUEUE_IDX_8852BE:
		*addr_l = R_AX_RPQ_RXBD_DESA_L;
		*addr_h = R_AX_RPQ_RXBD_DESA_H;
		break;
	default :
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]unknown channel (%d)\n",
				dma_ch);
		reg = 0xFFFF;
		break;
	}

}



/**
 * Get target DMA channel's BD Index hw register of rtl8852b
 * @dma_ch: input target dma channel with the hw definition of rtl8852BE
 * return the BD Index hw register
 */
static u32 _hal_get_bd_idx_reg_8852be(u8 dma_ch)
{
	u32 reg = 0;

	switch (dma_ch) {
	case ACH0_QUEUE_IDX_8852BE:
		reg = R_AX_ACH0_TXBD_IDX;
		break;
	case ACH1_QUEUE_IDX_8852BE:
		reg = R_AX_ACH1_TXBD_IDX;
		break;
	case ACH2_QUEUE_IDX_8852BE:
		reg = R_AX_ACH2_TXBD_IDX;
		break;
	case ACH3_QUEUE_IDX_8852BE:
		reg = R_AX_ACH3_TXBD_IDX;
		break;
	case ACH4_QUEUE_IDX_8852BE:
		reg = R_AX_ACH4_TXBD_IDX;
		break;
	case ACH5_QUEUE_IDX_8852BE:
		reg = R_AX_ACH5_TXBD_IDX;
		break;
	case ACH6_QUEUE_IDX_8852BE:
		reg = R_AX_ACH6_TXBD_IDX;
		break;
	case ACH7_QUEUE_IDX_8852BE:
		reg = R_AX_ACH7_TXBD_IDX;
		break;
	case MGQ_B0_QUEUE_IDX_8852BE:
		reg = R_AX_CH8_TXBD_IDX;
		break;
	case HIQ_B0_QUEUE_IDX_8852BE:
		reg = R_AX_CH9_TXBD_IDX;
		break;
	case MGQ_B1_QUEUE_IDX_8852BE:
		reg = R_AX_CH10_TXBD_IDX;
		break;
	case HIQ_B1_QUEUE_IDX_8852BE:
		reg = R_AX_CH11_TXBD_IDX;
		break;
	case FWCMD_QUEUE_IDX_8852BE:
		reg = R_AX_CH12_TXBD_IDX;
		break;
	case RX_QUEUE_IDX_8852BE:
		reg = R_AX_RXQ_RXBD_IDX;
		break;
	case RP_QUEUE_IDX_8852BE:
		reg = R_AX_RPQ_RXBD_IDX;
		break;
	default :
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]unknown channel (%d)\n",
				dma_ch);
		reg = 0xFFFF;
		break;
	}

	return reg;
}


/**
 * this function maps the sw xmit ring identified by macid, tid and band
 * to rtl8852BE hw tx dma channel
 * @macid: input target macid range is 0 ~ 127
 * @cat: input target packet category, see enum rtw_phl_ring_cat
 * @band: input target band, 0 for band 0 / 1 for band 1
 *
 * returns the mapping hw dma channel defined by XXX_QUEUE_IDX_8852BE
 * if the input parameter is unknown value, returns ACH0_QUEUE_IDX_8852BE
 */
static u8 hal_mapping_hw_tx_chnl_8852be(u16 macid, enum rtw_phl_ring_cat cat,
					u8 band)
{
	u8 dma_ch = 0;

	/* hana_todo, decided by tid only currently,
		we should consider more situation later */

	if (0 == band) {
		switch (cat) {
		case RTW_PHL_RING_CAT_TID0:
		case RTW_PHL_RING_CAT_TID3:
			dma_ch = ACH0_QUEUE_IDX_8852BE;
			break;
		case RTW_PHL_RING_CAT_TID1:
		case RTW_PHL_RING_CAT_TID2:
			dma_ch = ACH1_QUEUE_IDX_8852BE;
			break;
		case RTW_PHL_RING_CAT_TID4:
		case RTW_PHL_RING_CAT_TID5:
			dma_ch = ACH2_QUEUE_IDX_8852BE;
			break;
		case RTW_PHL_RING_CAT_TID6:
		case RTW_PHL_RING_CAT_TID7:
			dma_ch = ACH3_QUEUE_IDX_8852BE;
			break;
		case RTW_PHL_RING_CAT_MGNT:
			dma_ch = MGQ_B0_QUEUE_IDX_8852BE;
			break;
		case RTW_PHL_RING_CAT_HIQ:
			dma_ch = HIQ_B0_QUEUE_IDX_8852BE;
			break;
		default:
			dma_ch = ACH0_QUEUE_IDX_8852BE;
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]unknown category (%d)\n",
					cat);
			break;
		}
	} else {
		dma_ch = ACH0_QUEUE_IDX_8852BE;
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]unknown band (%d)\n",
				band);
	}

	return dma_ch;
}


/**
 * this function will return available txbd number of target dma channel
 * @ch_idx: input, target dma channel index
 * @host_idx: the ptr of current host index of this channel
 * @hw_idx: the ptr of current hw index of this channel
 *
 * NOTE, input host_idx and hw_idx ptr shall NOT be NULL
 */
static u16 hal_get_avail_txbd_8852be(struct rtw_hal_com_t *hal_com, u8 ch_idx,
				     u16 *host_idx, u16 *hw_idx)
{
	struct bus_cap_t *bus_cap = &hal_com->bus_cap;
	u16 avail_txbd = 0;
	u32 tmp32 = 0, reg = 0;
	u8 tx_dma_ch = 0;

	tx_dma_ch = ACH0_QUEUE_IDX_8852BE + ch_idx;

	reg = _hal_get_bd_idx_reg_8852be(tx_dma_ch);
	if (0xFFFF == reg) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
			"[WARNING]get dma channel register fail\n");
		avail_txbd = 0;
	} else {
		tmp32 = hal_read32(hal_com, reg);

		*host_idx = (u16)(tmp32 & 0x0FFF);
		*hw_idx = (u16)((tmp32 >> 16) & 0x0FFF);

		avail_txbd = hal_calc_avail_wptr(*hw_idx, *host_idx,
						 (u16)bus_cap->txbd_num);
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "hal_get_avail_txbd_8852be => dma_ch %d, host_idx %d, "
			  "hw_idx %d, avail_txbd %d\n",
			  ch_idx, *host_idx, *hw_idx, avail_txbd);
	}

	return avail_txbd;
}

/**
 * this function will return available txbd number of target dma channel
 * @ch_idx: input, target dma channel index
 * @host_idx: the ptr of current host index of this channel
 * @hw_idx: the ptr of current hw index of this channel
 *
 * NOTE, input host_idx and hw_idx ptr shall NOT be NULL
 */
static u16 hal_get_avail_rxbd_8852be(struct rtw_hal_com_t *hal_com, u8 ch_idx,
				     u16 *host_idx, u16 *hw_idx)
{
 	struct bus_cap_t *bus_cap = &hal_com->bus_cap;
	u16 avail_rxbd = 0;
	u32 tmp32 = 0, reg = 0;
	u8 rx_dma_ch = 0;

	rx_dma_ch = RX_QUEUE_IDX_8852BE + ch_idx;

	reg = _hal_get_bd_idx_reg_8852be(rx_dma_ch);
	if (0xFFFF == reg) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING]get dma channel register fail\n");
		avail_rxbd = 0;
	} else {
		tmp32 = hal_read32(hal_com, reg);

		*host_idx = (u16)(tmp32 & 0x0FFF);
		*hw_idx = (u16)((tmp32 >> 16) & 0x0FFF);

		avail_rxbd = hal_calc_avail_rptr(*host_idx, *hw_idx,
						 (u16)bus_cap->rxbd_num);
	}

	return avail_rxbd;
}


void _hal_fill_wp_seq_field_8852be(u8 *seq_info, u16 wp_seq)
{
	/* update sw wp seq */
	SET_PCIE_SEQ_INFO_0(seq_info, wp_seq);
	SET_PCIE_SEQ_INFO_0_VALID(seq_info, 1);
}

void _hal_fill_wp_addr_info_8852be(struct rtw_hal_com_t *hal_com,
				   u8 *addr_info, struct rtw_pkt_buf_list *pkt,
					u8 num, u8 mpdu_ls, u8 msdu_ls)
{
	SET_ADDR_INFO_LEN(addr_info, pkt->length);
	SET_ADDR_INFO_NUM(addr_info, num);
	SET_ADDR_INFO_MSDU_LS(addr_info, msdu_ls);
	SET_ADDR_INFO_ADDR_LOW(addr_info, pkt->phy_addr_l);
	SET_ADDR_INFO_ADDR_HIGH(addr_info, pkt->phy_addr_h);
}

u8 _hal_get_tid_indic_8852be(u8 tid)
{
	u8 tid_indic = 0;
	switch (tid) {
		case RTW_PHL_RING_CAT_TID0:
		case RTW_PHL_RING_CAT_TID1:
		case RTW_PHL_RING_CAT_TID4:
		case RTW_PHL_RING_CAT_TID6:
		case RTW_PHL_RING_CAT_MGNT:
		case RTW_PHL_RING_CAT_HIQ:
			tid_indic = 0;
			break;
		case RTW_PHL_RING_CAT_TID3:
		case RTW_PHL_RING_CAT_TID2:
		case RTW_PHL_RING_CAT_TID5:
		case RTW_PHL_RING_CAT_TID7:
			tid_indic = 1;
			break;
		default:
			PHL_ERR("unknown tid %d\n", tid);
			break;
	}

	return tid_indic;
}

#ifdef CONFIG_PHL_TXSC
static u8 qsel_tbl[] = {
	TID_0_QSEL/*0*/, TID_1_QSEL/*1*/, TID_2_QSEL/*1*/, TID_3_QSEL/*0*/,
	TID_4_QSEL/*2*/, TID_5_QSEL/*2*/, TID_6_QSEL/*3*/, TID_7_QSEL/*2*/
};

static u8 tid_ind[] = {
	TID_0_IND, TID_1_IND, TID_2_IND, TID_3_IND,
	TID_4_IND, TID_5_IND, TID_6_IND, TID_7_IND
};

static enum rtw_hal_status
_hal_txsc_update_wd(struct hal_info_t *hal,
						struct rtw_phl_pkt_req *req, u32 *wd_len)
{
	struct rtw_xmit_req *tx_req = req ? req->tx_req : NULL;
	struct rtw_t_meta_data	*mdata;
	u32 *wd_words;
	u32 w0, w2, w3;

	if (NULL == tx_req)
		return RTW_HAL_STATUS_FAILURE;

	if (req->wd_len == 0) {
		rtw_hal_mac_ax_fill_txdesc(hal->mac, tx_req, req->wd_page,
								   wd_len);
		req->wd_len = (u8)*wd_len;
	} else {
		mdata = &tx_req->mdata;
		mdata->dma_ch = hal_mapping_hw_tx_chnl_8852be(mdata->macid, mdata->tid, mdata->band);

		wd_words = (u32 *)req->wd_page;
		w0 = le32_to_cpu(wd_words[0])
				 & ~((AX_TXD_HW_SSN_SEL_MSK << AX_TXD_HW_SSN_SEL_SH)
					 | (AX_TXD_CH_DMA_MSK << AX_TXD_CH_DMA_SH));
		w2 = le32_to_cpu(wd_words[2])
				 & ~(AX_TXD_TID_IND
					 | (AX_TXD_QSEL_MSK << AX_TXD_QSEL_SH)
					 | (AX_TXD_TXPKTSIZE_MSK << AX_TXD_TXPKTSIZE_SH));

		/* Update SSN SEL, DMA CH, QSEL, and TID indicator in WD cache */
		w0 |= (((mdata->hw_ssn_sel & AX_TXD_HW_SSN_SEL_MSK) << AX_TXD_HW_SSN_SEL_SH)
			   | ((mdata->dma_ch & AX_TXD_CH_DMA_MSK) << AX_TXD_CH_DMA_SH));
		wd_words[0] = cpu_to_le32(w0);

		if (mdata->hw_seq_mode == 0) {
			w3 = cpu_to_le32((mdata->sw_seq & 0xFFF) |
				(mdata->ampdu_en ? BIT(12) : 0) |
				((mdata->bk || mdata->ack_ch_info) ? BIT(13) : 0));
			wd_words[3] = w3;
		}

		if (tid_ind[mdata->tid])
			w2 |= AX_TXD_TID_IND;

		w2 |= (qsel_tbl[mdata->tid] & AX_TXD_QSEL_MSK) << AX_TXD_QSEL_SH;
		w2 |= (mdata->pktlen & AX_TXD_TXPKTSIZE_MSK) << AX_TXD_TXPKTSIZE_SH;
		wd_words[2] = cpu_to_le32(w2);

		*wd_len = req->wd_len;
	}

	return RTW_HAL_STATUS_SUCCESS;
}
#endif

/**
 * the function update wd page, including wd info, wd body, seq info, addr info
 * @hal: see struct hal_info_t
 * @phl_pkt_req: see struct rtw_phl_pkt_req
 */
static enum rtw_hal_status
hal_update_wd_8852be(struct hal_info_t *hal,
                     struct rtw_phl_pkt_req *req)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;
	struct rtw_hal_com_t *hal_com = hal->hal_com;
	struct bus_hw_cap_t *bus_hw_cap = &hal_com->bus_hw_cap;
	struct rtw_xmit_req *tx_req = NULL;
	struct rtw_pkt_buf_list *pkt_list = NULL;
	u32 wd_len = 0, seq_ofst = 0, addr_info_ofst = 0;
	u16 wp_seq = 0;
	u8 i = 0, wp_num = 0, mpdu_ls = 0, msdu_ls = 0, tid_indic = 0;
	FUNCIN_WSTS(hstatus);
	do {
		if (NULL == req)
			break;

		tx_req = req->tx_req;
		pkt_list = (struct rtw_pkt_buf_list *)tx_req->pkt_list;

#ifdef CONFIG_PHL_TXSC
		_hal_txsc_update_wd(hal, req, &wd_len);
#else
		/* connect with halmac */
		rtw_hal_mac_ax_fill_txdesc(hal->mac, tx_req, req->wd_page,
						&wd_len);
#endif
		tid_indic = _hal_get_tid_indic_8852be(tx_req->mdata.tid);

		seq_ofst = wd_len;
		wp_seq = (1 == tid_indic) ?
				(req->wp_seq | WP_TID_INDIC_RESERVED_BIT) : req->wp_seq;
		_hal_fill_wp_seq_field_8852be(req->wd_page + seq_ofst, wp_seq);

		addr_info_ofst = seq_ofst + bus_hw_cap->seq_info_size;
		for (i = 0; i < tx_req->mdata.addr_info_num; i++) {
			if (0 == i)
				wp_num = tx_req->mdata.addr_info_num;
			else
				wp_num = 0;

			if ((tx_req->mdata.addr_info_num - 1) == i)
				msdu_ls = 1;
			else
				msdu_ls = 0;

			_hal_fill_wp_addr_info_8852be(hal_com,
				req->wd_page + addr_info_ofst,
				&pkt_list[i], wp_num, mpdu_ls, msdu_ls);

			addr_info_ofst += bus_hw_cap->addr_info_size;

			//debug_dump_data(pkt_list[i].vir_addr, pkt_list[i].length, "dump wp");
		}

		/* 8852BE length in txbd(wd_len) should be fix to 128 */
		if (addr_info_ofst != bus_hw_cap->max_wd_page_size)
    			req->wd_len = bus_hw_cap->max_wd_page_size;

	} while (false);

	#if 0 /* remove this for saving cpu cycle */
	if (RTW_HAL_STATUS_SUCCESS == hstatus) {
		debug_dump_data(req->wd_page, (u16)addr_info_ofst, "dump wd page");
	}
	#endif
	FUNCOUT_WSTS(hstatus);
	return hstatus;
}

/**
 * the function update txbd
 * @hal: see struct hal_info_t
 * @txbd_ring: the target txbd ring buffer going to update, see struct tx_base_desc
 * @wd: the wd page going to be filled in txbd, see struct rtw_wd_page
 */
static enum rtw_hal_status
hal_update_txbd_8852be(struct hal_info_t *hal,
			struct tx_base_desc *txbd_ring,
			struct rtw_wd_page *wd_page,
		       u8 ch_idx, u16 wd_num)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;
	struct rtw_hal_com_t *hal_com = hal->hal_com;
	struct bus_hw_cap_t *bus_hw_cap = &hal_com->bus_hw_cap;
	u8 *ring_head = 0;
	u8 *target_txbd = 0;
	u16 host_idx = 0;
	u16 txbd_num = (u16)hal_com->bus_cap.txbd_num;

	do {
		if (NULL == wd_page)
			break;
		if (NULL == txbd_ring)
			break;

		/* connect with halmac */
		host_idx = txbd_ring[ch_idx].host_idx;

		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
		          "hal_update_txbd_8852be => ch_idx %d, wd_num %d\n",
			  ch_idx, wd_num);

		while (wd_num > 0) {

			ring_head = txbd_ring[ch_idx].vir_addr;
			target_txbd = ring_head + (host_idx *
						   bus_hw_cap->txbd_len);

			SET_TXBUFFER_DESC_LEN(target_txbd, wd_page->buf_len);
			SET_TXBUFFER_DESC_LS(target_txbd, wd_page->ls);
			SET_TXBUFFER_DESC_ADD_LOW(target_txbd,
							wd_page->phy_addr_l);
			SET_TXBUFFER_DESC_ADD_HIGH(target_txbd,
							wd_page->phy_addr_h);
			host_idx = (host_idx + 1) % txbd_num;
			wd_page->host_idx = host_idx;
			wd_num--;

			//multi wd page in one update txbd
			#if 0//S_TODO
			if(wd_num > 0){
				wd_page = list_first_entry(wd_page->list,
								struct rtw_wd_page,
								wd_page->list);
				if(NULL == wd_page)
					break;
			}
			#endif
		}

		txbd_ring[ch_idx].host_idx = host_idx;
	} while (false);

	return hstatus;
}

/**
 * the function trigger tx start
 * @hal: see struct hal_info_t
 * @txbd_ring: the target txbd ring buffer going to update, see struct tx_base_desc
 * @ch_idx: the dma channel index of this txbd_ring
 */
static enum rtw_hal_status
hal_trigger_txdma_8852be(struct hal_info_t *hal,
			struct tx_base_desc *txbd_ring, u8 ch_idx)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	u8 tx_dma_ch;
	u32 txbd_reg;

	do {
		/* connect with halmac */
		tx_dma_ch = ACH0_QUEUE_IDX_8852BE + ch_idx;
		txbd_reg = _hal_get_bd_idx_reg_8852be(tx_dma_ch);
		if (0xFFFF == txbd_reg)
			break;
		PHL_TRACE(COMP_PHL_XMIT, _PHL_DEBUG_,
			  "hal_trigger_txdma_8852be => dma_ch %d, host_idx %d.\n",
			  ch_idx, txbd_ring[ch_idx].host_idx);
		hal_write16(hal->hal_com, txbd_reg, txbd_ring[ch_idx].host_idx);
		#ifdef CONFIG_DBG_H2C_TX
		if (tx_dma_ch == FWCMD_QUEUE_IDX_8852BE)
			PHL_INFO("%s: 0x%x -> 0x%x\n", __func__,
					_hal_get_bd_idx_reg_8852be(FWCMD_QUEUE_IDX_8852BE),
					hal_read32(hal->hal_com, _hal_get_bd_idx_reg_8852be(FWCMD_QUEUE_IDX_8852BE)));
		#endif /* CONFIG_DBG_H2C_TX */
		hstatus = RTW_HAL_STATUS_SUCCESS;
	} while (false);

	return hstatus;
}


static enum rtw_hal_status hal_pltfm_tx_8852be(void *hal, struct rtw_h2c_pkt *h2c_pkt)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;
#if 0

	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_wd_page wd_page;
	struct tx_base_desc *txbd_ring = NULL;

	_os_mem_set(hal_to_drvpriv(hal_info), &wd_page, 0, sizeof(wd_page));

	txbd_ring = (struct tx_base_desc *)hal_info->hal_com->fw_txbd;
	wd_page.vir_addr = h2c_pkt->vir_addr;
	wd_page.phy_addr_l = h2c_pkt->phy_addr_l;
	wd_page.phy_addr_h= h2c_pkt->phy_addr_h;
	wd_page.buf_len = h2c_pkt->buf_len;
	wd_page.cache = 1;

	_os_spinlock(hal_to_drvpriv(hal_info), &txbd_ring[FWCMD_QUEUE_IDX_8852BE].txbd_lock, _ps, NULL);
	hstatus = hal_update_txbd_8852BE(hal_info, txbd_ring, &wd_page,
							FWCMD_QUEUE_IDX_8852BE, 1);
	_os_spinunlock(hal_to_drvpriv(hal_info), &txbd_ring[FWCMD_QUEUE_IDX_8852BE].txbd_lock, _ps, NULL);

	/* enqueue busy queue */

	hstatus = hal_trigger_txdma_8852BE(hal_info, txbd_ring, FWCMD_QUEUE_IDX_8852BE);
#endif
	return hstatus;
}

u8 hal_get_fwcmd_queue_idx_8852be(void)
{
	return FWCMD_QUEUE_IDX_8852BE;
}

static u8 hal_check_rxrdy_8852be(struct rtw_phl_com_t *phl_com, u8 *rxbd_info,
				 u8 ch_idx)
{
	struct hal_spec_t *hal_spec = &phl_com->hal_spec;
	u8 res = false;
	u16 tag = 0, target_tag = 0;
	u16 read_cnt = 0;

	do {
		if (rxbd_info == NULL) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_,
					"[WARNING] input rx bd info is NULL!\n");
			res = false;
			break;
		}

		tag = (u16)GET_RX_BD_INFO_TAG(rxbd_info);

		if (hal_spec->rx_tag[ch_idx] == 0x1fff ||
			hal_spec->rx_tag[ch_idx] == 0)
			target_tag = 1;
		else
			target_tag = hal_spec->rx_tag[ch_idx] + 1;

		while (read_cnt < 10000) {

			read_cnt++;

			tag = (u16)GET_RX_BD_INFO_TAG(rxbd_info);

			if (tag == target_tag) {
				res = true;
				break;
			}
		}

		if (true == res) {
			hal_spec->rx_tag[ch_idx] = tag;
		} else {
			PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING] polling Rx Tag fail, tag = %d, target_tag = %d\n",
					tag, target_tag);
#ifdef RTW_WKARD_98D_RXTAG
			if (tag) {
				hal_spec->rx_tag[ch_idx] = tag;
				res = true;
			}
#endif
		}

	} while (false);

	return res;
}



u16 hal_handle_rx_report_8852be(struct hal_info_t *hal, u8 *rp, u16 len,
				u8 *sw_retry, u8 *dma_ch, u16 *wp_seq,
				u8 *macid, u8 *ac_queue, u8 *txsts)
{
	u8 polluted = false;
	u16 rsize = 0;
	u8 tid = 0, qsel_value = 0, band = 0, tid_indic = 0;

	do {
		if (len < RX_RP_PACKET_SIZE)
			break;

		*macid = (u8)GET_RX_RP_PKT_MAC_ID(rp);
		qsel_value = (u8)GET_RX_RP_PKT_QSEL(rp);
		*ac_queue = qsel_value % RTW_MAX_AC_QUEUE_NUM;
		*txsts = (u8)GET_RX_RP_PKT_TX_STS(rp);
		*wp_seq = (u16)GET_RX_RP_PKT_PCIE_SEQ(rp);
		polluted = (u8)GET_RX_RP_PKT_POLLUTED(rp);


		band = (qsel_value & BIT3) ? 1 : 0;
		tid_indic = (*wp_seq & WP_TID_INDIC_RESERVED_BIT) ? 1 : 0;
		*wp_seq &= (WP_RESERVED_SEQ);
		tid = hal_qsel_to_tid_8852be(hal, qsel_value, tid_indic);
		*dma_ch = hal_mapping_hw_tx_chnl_8852be(*macid, tid, band);

		PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "Get recycle report: qsel = %d, macid = %d, wp_seq = 0x%x, tid_indic = %d,"
			" tid = %d, band = %d, dma_ch = %d\n",
			qsel_value, *macid, *wp_seq, tid_indic, tid, band, *dma_ch);

		if (TX_STATUS_TX_DONE != *txsts) {
			*sw_retry = true;
			/* hana_todo handle sw retry */
		} else if (true == polluted) {
			PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "this wp is polluted\n");
			*sw_retry = true;
			/* hana_todo handle sw retry */
		} else {
			*sw_retry = false;
		}

		rsize = RX_RP_PACKET_SIZE;
	} while (false);

	return rsize;
}


/**
 * Process Rx PPDU Status with HALMAC API and PHYDM API
 */
static enum rtw_hal_status
hal_handle_ppdusts_8852be(void *hal, u8 *psbuf, u16 sz,
			struct rtw_r_meta_data *mdata, struct rx_ppdu_status *rxps)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;

	if (mdata->mac_info_vld) {
		/*Call HALMAC API HALMAC API*/
		rxps->mac_info_length = 4; //To DO
	}

	rxps->phy_info_length = sz - rxps->mac_info_length;
	if (rxps->phy_info_length > 0) {
		/* the remaining length > 4  the phy info is valid */
		/* CALL PHYDM API Here*/
		//rx_desc->mac_id

	}
	hstatus = RTW_HAL_STATUS_SUCCESS;
	return hstatus;
}

/**
 * SW Parsing Rx Desc
 **/
static enum rtw_hal_status
_hal_parsing_rx_wd_8852be(struct hal_info_t *hal, u8 *desc,
						struct rtw_r_meta_data *mdata)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;

	mdata->pktlen = GET_RX_AX_DESC_PKT_LEN_8852B(desc);
	mdata->shift = GET_RX_AX_DESC_SHIFT_8852B(desc);
	mdata->wl_hd_iv_len = GET_RX_AX_DESC_HDR_IV_L_8852B(desc);
	mdata->bb_sel = GET_RX_AX_DESC_BB_SEL_8852B(desc);
	mdata->mac_info_vld = GET_RX_AX_DESC_MAC_INFO_VLD_8852B(desc);
	mdata->rpkt_type = GET_RX_AX_DESC_RPKT_TYPE_8852B(desc);
	mdata->drv_info_size = GET_RX_AX_DESC_DRV_INFO_SIZE_8852B(desc);
	mdata->long_rxd = GET_RX_AX_DESC_LONG_RXD_8852B(desc);

	mdata->ppdu_type = GET_RX_AX_DESC_PPDU_TYPE_8852B(desc);
	mdata->ppdu_cnt = GET_RX_AX_DESC_PPDU_CNT_8852B(desc);
	mdata->sr_en = GET_RX_AX_DESC_SR_EN_8852B(desc);
	mdata->user_id = GET_RX_AX_DESC_USER_ID_8852B(desc);
	mdata->rx_rate = GET_RX_AX_DESC_RX_DATARATE_8852B(desc);
	mdata->rx_gi_ltf = GET_RX_AX_DESC_RX_GI_LTF_8852B(desc);
	mdata->non_srg_ppdu = GET_RX_AX_DESC_NON_SRG_PPDU_8852B(desc);
	mdata->inter_ppdu = GET_RX_AX_DESC_INTER_PPDU_8852B(desc);
	mdata->bw = GET_RX_AX_DESC_BW_8852B(desc);

	mdata->freerun_cnt = GET_RX_AX_DESC_FREERUN_CNT_8852B(desc);

	mdata->a1_match = GET_RX_AX_DESC_A1_MATCH_8852B(desc);
	mdata->sw_dec = GET_RX_AX_DESC_SW_DEC_8852B(desc);
	mdata->hw_dec = GET_RX_AX_DESC_HW_DEC_8852B(desc);
	mdata->ampdu = GET_RX_AX_DESC_AMPDU_8852B(desc);
	mdata->ampdu_end_pkt = GET_RX_AX_DESC_AMPDU_EDN_PKT_8852B(desc);
	mdata->amsdu = GET_RX_AX_DESC_AMSDU_8852B(desc);
	mdata->amsdu_cut = GET_RX_AX_DESC_AMSDU_CUT_8852B(desc);
	mdata->last_msdu = GET_RX_AX_DESC_LAST_MSDU_8852B(desc);
	mdata->bypass = GET_RX_AX_DESC_BYPASS_8852B(desc);
	mdata->crc32 = GET_RX_AX_DESC_CRC32_8852B(desc);
	mdata->icverr = GET_RX_AX_DESC_ICVERR_8852B(desc);
	mdata->magic_wake = GET_RX_AX_DESC_MAGIC_WAKE_8852B(desc);
	mdata->unicast_wake = GET_RX_AX_DESC_UNICAST_WAKE_8852B(desc);
	mdata->pattern_wake = GET_RX_AX_DESC_PATTERN_WAKE_8852B(desc);

	if (mdata->long_rxd==1) {
		mdata->macid = GET_RX_AX_DESC_MACID_8852B(desc);
	}

	hstatus = RTW_HAL_STATUS_SUCCESS;
	return hstatus;
}

static u8 hal_handle_rxbd_info_8852be(struct hal_info_t *hal,
						u8 *rxbd_info, u16 *size)
{
	u8 res = false;
	u16 pld_size = 0;
	u8 fs = 0, ls = 0;
	u8 pkt_rdy = false;

	do {
		if (NULL == rxbd_info)
			break;
		if (NULL == size)
			break;

	fs = (u8)GET_RX_BD_INFO_FS(rxbd_info);
	ls = (u8)GET_RX_BD_INFO_LS(rxbd_info);
	pld_size = (u16)GET_RX_BD_INFO_HW_W_SIZE(rxbd_info);

	if (fs == 1) {
		if (ls == 1)
			pkt_rdy = true;
		else
			pkt_rdy = false;

	} else if (fs == 0) {
		if (ls == 1)
			pkt_rdy = false;
		else
			pkt_rdy = false;
	}

	if (pkt_rdy) {
		*size = pld_size;
		res = true;
	} else {
		*size = 0;
		PHL_TRACE(COMP_PHL_DBG, _PHL_WARNING_, "[WARNING] need to handle RX FS/LS\n");
		res = false;
	}
	}while(false);
	return res;
}



/**
 * the function update rxbd
 */
static enum rtw_hal_status
hal_update_rxbd_8852be(struct hal_info_t *hal, struct rx_base_desc *rxbd,
		       struct rtw_rx_buf *rx_buf)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;
	struct rtw_hal_com_t *hal_com = hal->hal_com;
	struct bus_hw_cap_t *bus_hw_cap = &hal_com->bus_hw_cap;
	u8 *ring_head = NULL;
	u8 *target_rxbd = NULL;
	u16 rxbd_num = (u16)hal_com->bus_cap.rxbd_num;

	do {
		if (NULL == rxbd)
			break;
		if (NULL == rx_buf)
			break;

		ring_head = rxbd->vir_addr;
		target_rxbd = ring_head + (rxbd->host_idx *
					   bus_hw_cap->rxbd_len);
		/* connect with halmac */
		SET_RX_BD_RXBUFFSIZE(target_rxbd, rx_buf->buf_len);
		SET_RX_BD_PHYSICAL_ADDR_LOW(target_rxbd,
					    (u32)rx_buf->phy_addr_l);
		SET_RX_BD_PHYSICAL_ADDR_HIGH(target_rxbd,
					     (u32)rx_buf->phy_addr_h);
		rxbd->host_idx = (rxbd->host_idx + 1) % rxbd_num;
	} while (false);

	return hstatus;
}


/**
 * the function notify rx done
 */
static enum rtw_hal_status
hal_notify_rxdone_8852be(struct hal_info_t *hal,
		struct rx_base_desc *rxbd, u8 ch, u16 rxcnt)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	u32 reg = 0;
	u8 rx_dma_ch = 0;

	do {
		rx_dma_ch = RX_QUEUE_IDX_8852BE + ch;
		reg = _hal_get_bd_idx_reg_8852be(rx_dma_ch);
		/* connect with halmac */
		if (0xFFFF == reg)
			break;
		hal_write16(hal->hal_com, reg, rxbd->host_idx);
		hstatus = RTW_HAL_STATUS_SUCCESS;
	} while (false);

	return hstatus;
}


struct bd_ram bdram_table_8852b[] = {
		/* ACH0_QUEUE_IDX_8852BE */ {0, 25, 4},
		/* ACH1_QUEUE_IDX_8852BE */ {25, 25, 4},
		/* ACH2_QUEUE_IDX_8852BE */ {50, 25, 4},
		/* ACH3_QUEUE_IDX_8852BE */ {75, 25, 4},
		/* ACH4_QUEUE_IDX_8852BE */ {100, 25, 4},
		/* ACH5_QUEUE_IDX_8852BE */ {125, 25, 4},
		/* ACH6_QUEUE_IDX_8852BE */ {150, 25, 4},
		/* ACH7_QUEUE_IDX_8852BE */ {175, 25, 4},
		/* MGQ_B0_QUEUE_IDX_8852BE */ {185, 10, 4},
		/* HIQ_B0_QUEUE_IDX_8852BE */ {195, 10, 4},
		/* MGQ_B1_QUEUE_IDX_8852BE */ {205, 10, 4},
		/* HIQ_B1_QUEUE_IDX_8852BE */ {215, 10, 4},
		/* FWCMD_QUEUE_IDX_8852BE */ {225, 10, 4}
};

static void _hal_tx_init_bd_ram_8852be(struct hal_info_t *hal)
{
	u32 reg = 0;
	u32 value = 0;
	u8 i = 0;

	for (i = 0; i < TX_DMA_CHANNEL_ENTRY_8852BE; i++) {
		/*if (FWCMD_QUEUE_IDX_8852BE == i)
			continue;*/
		value = 0;
		value = bdram_table_8852b[i].sidx +
			((bdram_table_8852b[i].max << 8) & 0xFF00) +
			((bdram_table_8852b[i].min << 16) & 0xFF0000);

		reg = _hal_get_bd_ram_reg_8852be(i);
		if (0 != reg)
			hal_write32(hal->hal_com, reg, value);
		else
			PHL_ERR("query txbd num reg fail (ch_idx = %d)\n", i);
	}

	value = hal_read32(hal->hal_com, R_AX_PCIE_INIT_CFG1);
	value |= (B_AX_RST_BDRAM);
	hal_write32(hal->hal_com, R_AX_PCIE_INIT_CFG1, value);

}
static void _hal_trx_init_bd_num_8852be(struct hal_info_t *hal)
{
	struct rtw_hal_com_t *hal_com = hal->hal_com;
	u16 txbd_num = (u16)hal_com->bus_cap.txbd_num;
	u16 rxbd_num = (u16)hal_com->bus_cap.rxbd_num;
	u32 reg = 0;
	u16 value = 0;
	u8 i = 0;

	for (i = 0; i < TX_DMA_CHANNEL_ENTRY_8852BE; i++) {
		/*if (FWCMD_QUEUE_IDX_8852BE == i)
			continue;*/

		value = txbd_num & B_AX_DESC_NUM_MSK;

		reg = _hal_get_bd_num_reg_8852be(i);
		if (0 != reg)
			hal_write16(hal->hal_com, reg, value);
		else
			PHL_ERR("query txbd num reg fail (ch_idx = %d)\n", i);
	}

	for (i = RX_QUEUE_IDX_8852BE;
		 i < RX_QUEUE_IDX_8852BE + RX_DMA_CHANNEL_ENTRY_8852BE; i++) {
		 /*if (FWCMD_QUEUE_IDX_8852BE == i)
			 continue;*/

		 value = rxbd_num & B_AX_DESC_NUM_MSK;

		 reg = _hal_get_bd_num_reg_8852be(i);
		 if (0 != reg)
			 hal_write16(hal->hal_com, reg, value);
		 else
			 PHL_ERR("query rxbd num reg fail (ch_idx = %d)\n", i);
	}
}


static void _hal_trx_init_bd_8852be(struct hal_info_t *hal, u8 *txbd_buf, u8 *rxbd_buf)
{
	struct tx_base_desc *txbd = NULL;
	struct rx_base_desc *rxbd = NULL;
	u32 reg_addr_l = 0, reg_addr_h = 0;
	u8 i = 0, rxch_idx = 0;

	if (NULL != txbd_buf && NULL != rxbd_buf) {
		txbd = (struct tx_base_desc *)txbd_buf;
		rxbd = (struct rx_base_desc *)rxbd_buf;

		for (i = 0; i < TX_DMA_CHANNEL_ENTRY_8852BE; i++) {
			/*if (FWCMD_QUEUE_IDX_8852BE == i)
				continue;*/

			_hal_get_bd_desc_reg_8852be(i, &reg_addr_l, &reg_addr_h);
			if (0 != reg_addr_l)
				hal_write32(hal->hal_com, reg_addr_l, txbd[i].phy_addr_l);
			else
				PHL_ERR("query txbd desc reg_addr_l fail (ch_idx = %d)\n", i);

			if (0 != reg_addr_h)
				hal_write32(hal->hal_com, reg_addr_h, txbd[i].phy_addr_h);
			else
				PHL_ERR("query txbd desc reg_addr_h fail (ch_idx = %d)\n", i);
		}

		for (i = 0; i < RX_DMA_CHANNEL_ENTRY_8852BE; i++) {
			 rxch_idx = i + RX_QUEUE_IDX_8852BE;
			_hal_get_bd_desc_reg_8852be(rxch_idx, &reg_addr_l, &reg_addr_h);
			if (0 != reg_addr_l)
				hal_write32(hal->hal_com, reg_addr_l, rxbd[i].phy_addr_l);
			else
				PHL_ERR("query rxbd desc reg_addr_l fail (ch_idx = %d)\n", i);
			if (0 != reg_addr_h)
				hal_write32(hal->hal_com, reg_addr_h, rxbd[i].phy_addr_h);
			else
				PHL_ERR("query rxbd desc reg_addr_h fail (ch_idx = %d)\n", i);
		}

	}
}

static void _hal_tx_enable_truncate_mode(struct hal_info_t *hal, u8 *txbd_buf,
					 u8 *rxbd_buf)
{
	struct tx_base_desc *txbd = NULL;
	struct rx_base_desc *rxbd = NULL;
	u32 value = 0;

	txbd = (struct tx_base_desc *)txbd_buf;
	rxbd = (struct rx_base_desc *)rxbd_buf;

	value = hal_read32(hal->hal_com, R_AX_PCIE_INIT_CFG1);
	value |= (B_AX_TX_TRUNC_MODE | B_AX_RX_TRUNC_MODE);
	hal_write32(hal->hal_com, R_AX_PCIE_INIT_CFG1, value);

	hal_write32(hal->hal_com, R_AX_TXDMA_ADDR_H, txbd->phy_addr_h);
	hal_write32(hal->hal_com, R_AX_RXDMA_ADDR_H, rxbd->phy_addr_h);
}

static void _hal_tx_disable_truncate_mode(struct hal_info_t *hal)
{
	u32 value = 0;

	value = hal_read32(hal->hal_com, R_AX_TX_ADDRESS_INFO_MODE_SETTING);
	value &= ~(B_AX_HOST_ADDR_INFO_8B_SEL);
	hal_write32(hal->hal_com, R_AX_TX_ADDRESS_INFO_MODE_SETTING, value);

	value = hal_read32(hal->hal_com, R_AX_PKTIN_SETTING);
	value |= (B_AX_WD_ADDR_INFO_LENGTH);
	hal_write32(hal->hal_com, R_AX_PKTIN_SETTING, value);
}


static void _hal_select_rxbd_mode(struct hal_info_t *hal,
					enum rxbd_mode_8852BE mode)
{
	u32 value = 0;

	if (RXBD_MODE_PACKET == mode) {
		value = hal_read32(hal->hal_com, R_AX_PCIE_INIT_CFG1);
		value &= ~(B_AX_RXBD_MODE);
		hal_write32(hal->hal_com, R_AX_PCIE_INIT_CFG1, value);
	} else if (RXBD_MODE_SEPARATION == mode) {
/*
		value = hal_read32(hal->hal_com, R_AX_PCIE_INIT_CFG1);
		value |= (B_AX_RXBD_MODE);
		hal_write32(hal->hal_com, R_AX_PCIE_INIT_CFG1, value);
*/
		/* hana_todo, append length setting */
	} else {
		PHL_WARN("Unknown Rx BD mode (%d)\n", mode);
	}
}

static void hal_cfg_wow_txdma_8852be(struct hal_info_t *hal, u8 en)
{
	struct mac_ax_txdma_ch_map ch_map;

	ch_map.ch0 = en ? MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE;
	ch_map.ch1 = en ? MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE;
	ch_map.ch2 = en ? MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE;
	ch_map.ch3 = en ? MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE;
	ch_map.ch4 = MAC_AX_PCIE_IGNORE;
	ch_map.ch5 = MAC_AX_PCIE_IGNORE;
	ch_map.ch6 = MAC_AX_PCIE_IGNORE;
	ch_map.ch7 = MAC_AX_PCIE_IGNORE;
	ch_map.ch8 = en ? MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE;
	ch_map.ch9 = en ? MAC_AX_PCIE_ENABLE : MAC_AX_PCIE_DISABLE;
	ch_map.ch10 = MAC_AX_PCIE_IGNORE;
	ch_map.ch11 = MAC_AX_PCIE_IGNORE;
	ch_map.ch12 = MAC_AX_PCIE_IGNORE;

	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_mac_cfg_txdma(hal, &ch_map))
		PHL_ERR("%s failure \n", __func__);

}

static u8 hal_poll_txdma_idle_8852be(struct hal_info_t *hal)
{
	struct mac_ax_txdma_ch_map ch_map;

	ch_map.ch0 = MAC_AX_PCIE_ENABLE;
	ch_map.ch1 = MAC_AX_PCIE_ENABLE;
	ch_map.ch2 = MAC_AX_PCIE_ENABLE;
	ch_map.ch3 = MAC_AX_PCIE_ENABLE;
	ch_map.ch4 = MAC_AX_PCIE_IGNORE;
	ch_map.ch5 = MAC_AX_PCIE_IGNORE;
	ch_map.ch6 = MAC_AX_PCIE_IGNORE;
	ch_map.ch7 = MAC_AX_PCIE_IGNORE;
	ch_map.ch8 = MAC_AX_PCIE_ENABLE;
	ch_map.ch9 = MAC_AX_PCIE_ENABLE;
	ch_map.ch10 = MAC_AX_PCIE_IGNORE;
	ch_map.ch11 = MAC_AX_PCIE_IGNORE;
	ch_map.ch12 = MAC_AX_PCIE_ENABLE;


	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_mac_poll_txdma_idle(hal, &ch_map)) {

		PHL_ERR("%s failure \n", __func__);

		return false;
	}
	return true;
}
static void _hal_clear_trx_state(struct hal_info_t *hal)
{
	u32 value = 0;

	/* disable hci */
	value = hal_read32(hal->hal_com, R_AX_PCIE_INIT_CFG1);
	value &= ~(B_AX_RXHCI_EN | B_AX_TXHCI_EN);
	hal_write32(hal->hal_com, R_AX_PCIE_INIT_CFG1, value);

	/* check mac power on status */

	/* clear hci idx */
	value = hal_read32(hal->hal_com, R_AX_TXBD_RWPTR_CLR1);
	value |= (B_AX_CLR_ACH0_IDX | B_AX_CLR_ACH1_IDX | B_AX_CLR_ACH2_IDX |
				B_AX_CLR_ACH3_IDX | B_AX_CLR_ACH4_IDX | B_AX_CLR_ACH5_IDX |
				B_AX_CLR_ACH6_IDX | B_AX_CLR_ACH7_IDX | B_AX_CLR_CH8_IDX |
				B_AX_CLR_CH9_IDX | B_AX_CLR_CH12_IDX);
	hal_write32(hal->hal_com, R_AX_TXBD_RWPTR_CLR1, value);

	value = hal_read32(hal->hal_com, R_AX_TXBD_RWPTR_CLR2);
	value |= (B_AX_CLR_CH10_IDX | B_AX_CLR_CH11_IDX);
	hal_write32(hal->hal_com, R_AX_TXBD_RWPTR_CLR2, value);

	value = hal_read32(hal->hal_com, R_AX_RXBD_RWPTR_CLR);
	value |= (B_AX_CLR_RXQ_IDX | B_AX_CLR_RPQ_IDX);
	hal_write32(hal->hal_com, R_AX_RXBD_RWPTR_CLR, value);
}

/**
 * the function will deinitializing 8852BE specific data and hw configuration
 */
static void hal_trx_deinit_8852be(struct hal_info_t *hal)
{
	/*struct rtw_hal_com_t *hal_com = hal->hal_com;*/
}

/**
 * the function will initializing 8852BE specific data and hw configuration
 */
static enum rtw_hal_status hal_trx_init_8852be(struct hal_info_t *hal, u8 *txbd_buf, u8 *rxbd_buf)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;
	/* Set AMPDU max agg number to 128 */
	/* CR setting*/
	rtw_hal_mac_set_hw_ampdu_cfg(hal, 0, 0x7F, 0xAB);
	return hstatus;
}

static struct hal_trx_ops ops = {0};
void hal_trx_ops_init_8852be(void)
{
	ops.init = hal_trx_init_8852be;
	ops.deinit = hal_trx_deinit_8852be;
	ops.query_tx_res = hal_get_avail_txbd_8852be;
	ops.query_rx_res = hal_get_avail_rxbd_8852be;
	ops.cfg_wow_txdma = hal_cfg_wow_txdma_8852be;
	ops.poll_txdma_idle = hal_poll_txdma_idle_8852be;
	ops.map_hw_tx_chnl = hal_mapping_hw_tx_chnl_8852be;
	ops.qsel_to_tid = hal_qsel_to_tid_8852be;
	ops.query_txch_num = hal_query_txch_num_8852be;
	ops.query_rxch_num = hal_query_rxch_num_8852be;
	ops.update_wd = hal_update_wd_8852be;
	ops.update_txbd = hal_update_txbd_8852be;
	ops.tx_start = hal_trigger_txdma_8852be;
	ops.check_rxrdy = hal_check_rxrdy_8852be;
	ops.handle_rxbd_info = hal_handle_rxbd_info_8852be;
	ops.handle_rx_buffer = hal_handle_rx_buffer_8852b;
	ops.update_rxbd = hal_update_rxbd_8852be;
	ops.notify_rxdone = hal_notify_rxdone_8852be;
	ops.handle_wp_rpt = hal_handle_rx_report_8852be;
	ops.get_fwcmd_queue_idx = hal_get_fwcmd_queue_idx_8852be;
}



u32 hal_hook_trx_ops_8852be(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal_info)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;

	if (NULL != hal_info) {
		hal_trx_ops_init_8852be();
		hal_info->trx_ops = &ops;
		hstatus = RTW_HAL_STATUS_SUCCESS;
	}

	return hstatus;
}
