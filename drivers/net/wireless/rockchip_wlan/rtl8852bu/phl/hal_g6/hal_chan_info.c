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
#define _HAL_CHAN_INFO_C_
#include "hal_headers.h"

#ifdef CONFIG_PHL_CHANNEL_INFO

enum rtw_hal_status
rtw_hal_cfg_chinfo(void *hal, struct rtw_phl_stainfo_t *sta, u8 enable)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct chinfo_bbcr_cfg bbcr = {0};
	u8 mode = 0, filter = 0, sg_size = 0;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	u8 macid = (u8)sta->macid;
	/*
	 *TODO: Discuss how to config accroding to different mode.
	 *	Currently, macid, mode, filter, sg_size use default value,
	 *	which is hard code in rtw_hal_mac_chan_info_cfg.
	 */
	hal_status = rtw_hal_mac_chan_info_cfg(hal_info, enable, macid, mode, filter, sg_size);
	if (hal_status == RTW_HAL_STATUS_SUCCESS) {
		if (enable) {
			/*
			 *Below is Example code for BBCR set to Legacy channel 80MHz
			 *TODO 1: need to calculate channel raw data size to see if
			 *	it is acceptable.
			 *TODO 2: there should be 10 sets of pre-defined configuration for BBCR.
			 *	need to decide which CR to use.
			 */
			bbcr.ch_i_phy0_en =1;
			bbcr.ch_i_phy1_en = 0;
			bbcr.ch_i_grp_num_he = 1;
			bbcr.ch_i_grp_num_non_he = 1;
			bbcr.ch_i_data_src = 0; /*LS, channel estimation*/
			bbcr.ch_i_cmprs = 0; /*8 bits*/
			bbcr.ch_i_ele_bitmap = 0x101;
			bbcr.ch_i_blk_start_idx = 1;
			bbcr.ch_i_blk_end_idx = 10;
			bbcr.ch_i_type = 0; /* legacy channel*/
			bbcr.ch_i_seg_len = 0; /*12*/
			rtw_hal_bb_chan_info_cfg(hal_info, &bbcr);
		}
	}

	return hal_status;
}

void
_hal_fill_csi_header_remain(void* hal, struct csi_header_t *csi_header
	, struct rtw_r_meta_data *mdata)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *h = hal_info->hal_com;

	/* from mdata */
	csi_header->hw_assigned_timestamp = mdata->freerun_cnt;
	csi_header->rx_data_rate = mdata->rx_rate;
	csi_header->bandwidth = mdata->bw;
	csi_header->ch_matrix_report = mdata->get_ch_info;
	/* TODO: from drv define, get value from other side.*/
	csi_header->channel = 0;
	csi_header->num_sub_carrier = 0;
	csi_header->num_bit_per_tone = 0;
	/* Others: mac addres not from TA ? */
	/* hal_mem_cpy(h, &(csi_header->mac_addr[0]), &(mdata->ta[0]), MAC_ALEN); */
}

void
_hal_fill_csi_header_phy_info(void* hal, struct csi_header_t *csi_header
	, struct ch_rpt_hdr_info *ch_hdr_rpt, struct phy_info_rpt *phy_info)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_hal_com_t *h = hal_info->hal_com;

	/* Initialize csi header */
	hal_mem_set(h, csi_header, 0, sizeof(struct csi_header_t));
	/* from phy_info_rpt */
	hal_mem_cpy(h, &(csi_header->rssi[0]), &(phy_info->rssi[0]), 2);
	csi_header->rxsc = phy_info->rxsc;
	/* from ch_rpt_hdr_info */
	csi_header->nc = ch_hdr_rpt->n_rx;
	csi_header->nr = ch_hdr_rpt->n_sts;
	csi_header->avg_idle_noise_pwr = ch_hdr_rpt->avg_noise_pow;
	csi_header->csi_data_length = (ch_hdr_rpt->total_len_l <<16 | ch_hdr_rpt->total_len_m)
		- 16; /* header(8byte) + Phy_info_in_seg_0(8byte) */
	csi_header->csi_valid = ch_hdr_rpt->set_valid;
	hal_mem_cpy(h, &(csi_header->evm[0]), &(ch_hdr_rpt->evm[0]), 2);
}

#endif /* CONFIG_PHL_CHANNEL_INFO */