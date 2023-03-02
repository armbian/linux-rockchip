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
#ifndef _PHL_TRX_PCIE_H_
#define _PHL_TRX_PCIE_H_

#define WP_DELAY_THRES_MS 1000
#define WD_PAGE_SIZE 128
#ifdef CONFIG_PHL_REDUCE_MEM
#define RX_BUF_SIZE 8192 /* Rx buffer size (without RXBD info length) to 8 byte alignment  by DD suggestion  */
#else
#define RX_BUF_SIZE 11460 /* Rx buffer size (without RXBD info length) to 8 byte alignment  by DD suggestion  */
#endif

enum dump_list_type {
	TYPE_WD_PAGE = 0,
	TYPE_PHL_RING = 1,
	TYPE_RING_STS = 2,
	TYPE_H2C_PKT = 3,
	TYPE_MAX = 0xFF
};



struct rtw_rx_buf_ring {
	struct rtw_rx_buf *rx_buf;
	_os_list idle_rxbuf_list;
	_os_list busy_rxbuf_list;
	u16 idle_rxbuf_cnt;
	u16 busy_rxbuf_cnt;
	_os_lock idle_rxbuf_lock;
	_os_lock busy_rxbuf_lock;
};

struct rtw_wp_tag {
	u8 *ptr;
};

struct rtw_h2c_work {
	struct rtw_h2c_pkt *cmd;
	struct rtw_h2c_pkt *data;
	struct rtw_h2c_pkt *ldata;
	struct rtw_h2c_pkt **cmd_ring;
	struct rtw_h2c_pkt **data_ring;
	struct rtw_h2c_pkt **ldata_ring;
	_os_lock       	lock;
	u16		cmd_cnt;
	u16		cmd_idx;
	u16		data_cnt;
	u16		data_idx;
	u16		ldata_cnt;
	u16		ldata_idx;
};

struct rtw_wd_page_ring {
	struct rtw_wd_page *wd_page;
	struct rtw_wd_page *wd_work;
	struct rtw_wd_page **wd_work_ring;
 	_os_list	idle_wd_page_list;
  	_os_list	busy_wd_page_list;
	_os_list	pending_wd_page_list;
	_os_lock	idle_lock;
 	_os_lock	busy_lock;
	_os_lock	pending_lock;
	_os_lock	work_lock;
	_os_lock	wp_tag_lock;
	u16 		idle_wd_page_cnt;
	u16 		busy_wd_page_cnt;
	u16 		pending_wd_page_cnt;
	u16		wd_work_cnt;
	u16		wd_work_idx;
	struct rtw_h2c_work h2c_work;
	struct rtw_wp_tag wp_tag[WP_MAX_SEQ_NUMBER];
	u16 wp_seq;
	u16 cur_hw_res;
};

struct phl_buf {
	u8 *vir_addr;
	_dma phy_addr;
	u8 cache;
	u16 buf_len;
};




/* struct hana_temp{ */
/* 	struct wp_tag wp_tag[DMA_CHANNEL_ENTRY][4096]; */
/* 	struct wp_tag wp_tag_hq_b0[4096]; */
/* 	struct wp_tag wp_tag_mq_b0[4096]; */
/* 	struct wp_tag wp_tag_mq_no_ps_b0[4096]; */
/* 	struct wp_tag wp_tag_hq_b1[4096]; */
/* 	struct wp_tag wp_tag_mq_b1[4096]; */
/* 	struct wp_tag wp_tag_mq_no_ps_b1[4096]; */
/* }; */

enum rtw_phl_status phl_hook_trx_ops_pci(struct phl_info_t *phl_info);

enum rtw_phl_status phl_cmd_set_l2_leave(struct phl_info_t *phl_info);
#endif	/* _PHL_TRX_PCIE_H_ */
