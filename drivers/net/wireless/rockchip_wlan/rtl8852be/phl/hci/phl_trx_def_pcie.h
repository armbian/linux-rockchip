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
#ifndef _PHL_TRX_DEF_PCIE_H_
#define _PHL_TRX_DEF_PCIE_H_

#ifdef RTW_WKARD_WD0_SER
#define MAX_WD_PAGE_NUM (256*2)
#else
#define MAX_WD_PAGE_NUM 256
#endif

#define WP_TID_INDIC_RESERVED_BIT BIT(14)
#define WP_RESERVED_SEQ 0xFFF
#define WP_MAX_SEQ_NUMBER WP_RESERVED_SEQ


struct tx_base_desc {
	u8 *vir_addr;
	u32 phy_addr_l;
	u32 phy_addr_h;
	u32 buf_len;
	void *os_rsvd[1];
	u8 cache;
	u16 host_idx;
	u16 avail_num;
	_os_lock txbd_lock;
};

struct rtw_wd_page {
	_os_list list;
	u8 *vir_addr;
	u32 phy_addr_l;
	u32 phy_addr_h;
	u32 buf_len;
	void *os_rsvd[1];
	u8 ls;
	u8 cache;
	u16 wp_seq;
	u16 host_idx;
};

struct rtw_rx_buf {
	_os_list list;
	u8 *vir_addr;
	u32 phy_addr_l;
	u32 phy_addr_h;
	u32 buf_len;
	u16 hw_write_size;
	u8 cache;
	u8 dynamic;
#ifdef CONFIG_DYNAMIC_RX_BUF
	bool reuse;
#endif
	void *os_priv;
};

struct rx_base_desc {
	u8 *vir_addr;
	u32 phy_addr_l;
	u32 phy_addr_h;
	u32 buf_len;
	void *os_rsvd[1];
	u8 cache;
	u16 host_idx;
	u16 hw_idx;
};
#endif	/* _PHL_TRX_DEF_PCIE_H_ */
