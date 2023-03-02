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
#define _HAL_API_BB_C_
#include "hal_headers.h"
#include "phy/bb/halbb_export_fun.h"

//kevin-cmd
#include "phy/bb/halbb_hw_cfg_ex.h"

#ifdef USE_TRUE_PHY
enum phl_phy_idx rtw_hal_bb_band_to_phy_idx(struct rtw_hal_com_t *hal_com, u8 band_idx)
{
	enum phl_phy_idx p_idx = HW_PHY_0;

	if (band_idx == 1)
		p_idx = HW_PHY_1;

	return p_idx;
}

void rtw_hal_bb_dfs_en(struct hal_info_t *hal_info, bool en)
{
	halbb_dfs_en(hal_info->bb, en);
}

void rtw_hal_bb_tssi_cont_en(struct hal_info_t *hal_info, bool en, enum rf_path path)
{
	halbb_tssi_cont_en(hal_info->bb, en, path);
}

void rtw_hal_bb_adc_en(struct hal_info_t *hal_info, bool en)
{
	halbb_adc_en(hal_info->bb, en);
}

void rtw_hal_bb_reset_en(struct hal_info_t *hal_info, bool en, enum phl_phy_idx phy_idx)
{
	halbb_bb_reset_en(hal_info->bb, en, phy_idx);
}

bool rtw_hal_bb_proc_cmd(struct hal_info_t *hal_info, struct rtw_proc_cmd *incmd,
						char *output, u32 out_len)
{
	if(incmd->in_type == RTW_ARG_TYPE_BUF)
		halbb_cmd(hal_info->bb, incmd->in.buf, output, out_len);
	else if(incmd->in_type == RTW_ARG_TYPE_ARRAY){
		halbb_cmd_parser(hal_info->bb, incmd->in.vector,
					incmd->in_cnt_len, output, out_len);
	}

	return true;
}

enum rtw_hal_status rtw_hal_bb_watchdog(struct hal_info_t *hal_info, u8 is_lps)
{
	enum bb_watchdog_mode_t mode = BB_WATCHDOG_NORMAL;

	if (is_lps)
		mode = BB_WATCHDOG_LOW_IO;

	halbb_watchdog(hal_info->bb, mode, HW_PHY_0);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_simple_watchdog(struct hal_info_t *hal_info, u8 io_en)
{
	enum bb_watchdog_mode_t mode = BB_WATCHDOG_NON_IO;

	if (io_en)
		mode = BB_WATCHDOG_LOW_IO;

	halbb_watchdog(hal_info->bb, mode, HW_PHY_0);

	return RTW_HAL_STATUS_SUCCESS;
}

void rtw_hal_bb_reset(struct hal_info_t *hal_info)
{
	halbb_watchdog_reset(hal_info->bb);
}

void rtw_hal_bb_fw_edcca(struct hal_info_t *hal_info)
{
	PHL_INFO("[Cert], %s() ==> !! \n", __func__);

	halbb_fw_edcca(hal_info->bb);
}

void rtw_hal_bb_dm_init(struct hal_info_t *hal_info)
{
	halbb_dm_init(hal_info->bb, HW_PHY_0);
}

void rtw_hal_bb_dm_deinit(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal_info)
{
	halbb_dm_deinit(phl_com, hal_info->bb);
}

enum rtw_hal_status rtw_hal_bb_ctrl_rx_cca(struct rtw_hal_com_t *hal_com,
	bool cca_en, enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal = (struct hal_info_t *)hal_com->hal_priv;

	halbb_ctrl_rx_cca(hal->bb, cca_en, phy_idx);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_ctrl_dbcc(struct hal_info_t *hal_info, bool dbcc_en)
{
	halbb_ctrl_dbcc(hal_info->bb, dbcc_en);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_cfg_dbcc(struct hal_info_t *hal_info, bool dbcc_en)
{
	halbb_cfg_dbcc(hal_info->bb, dbcc_en);

	return RTW_HAL_STATUS_SUCCESS;
}

u32 rtw_hal_bb_init(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal_info)
{
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	enum rtw_hal_status hal_status;

	hal_status = halbb_buffer_init(phl_com, hal_com, &(hal_info->bb));

	if ((hal_status != RTW_HAL_STATUS_SUCCESS) || (hal_info->bb == NULL))
		PHL_ERR("[PHL] rtw_hal_bb_init failed status(%d), hal_info->bb(%p)\n",
			hal_status, hal_info->bb);

	return hal_status;
}

void rtw_hal_bb_deinit(struct rtw_phl_com_t *phl_com,
						struct hal_info_t *hal_info)
{
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;

	halbb_buffer_deinit(phl_com, hal_com, hal_info->bb);
}

void rtw_hal_init_bb_reg(struct hal_info_t *hal_info)
{
	halbb_init_reg(hal_info->bb);
	halbb_reset_bb(hal_info->bb);
}

u32 rtw_hal_read_rf_reg(struct rtw_hal_com_t *hal_com,
			enum rf_path path, u32 addr, u32 mask)
{
	struct hal_info_t *hal = (struct hal_info_t *)hal_com->hal_priv;

	return halbb_read_rf_reg(hal->bb, path, addr, mask);
}

bool rtw_hal_write_rf_reg(struct rtw_hal_com_t *hal_com,
			  enum rf_path path, u32 addr, u32 mask, u32 data)
{
	struct hal_info_t *hal = (struct hal_info_t *)hal_com->hal_priv;

	return halbb_write_rf_reg(hal->bb, path, addr, mask, data);
}

u32 rtw_hal_read_bb_reg(struct rtw_hal_com_t *hal_com, u32 addr, u32 mask)
{
	struct hal_info_t *hal = (struct hal_info_t *)hal_com->hal_priv;

	return halbb_get_reg(hal->bb, addr, mask);
}

bool rtw_hal_write_bb_reg(struct rtw_hal_com_t *hal_com,
			  u32 addr, u32 mask, u32 data)
{
	struct hal_info_t *hal = (struct hal_info_t *)hal_com->hal_priv;

	halbb_set_reg(hal->bb, addr, mask, data);
	return true;
}

u32 rtw_hal_bb_read_cr(struct rtw_hal_com_t *hal_com, u32 addr, u32 mask)
{
	struct hal_info_t *hal = (struct hal_info_t *)hal_com->hal_priv;

	return halbb_rf_get_bb_reg(hal->bb, addr, mask);
}

bool rtw_hal_bb_write_cr(struct rtw_hal_com_t *hal_com,
			     u32 addr, u32 mask, u32 data)
{
	struct hal_info_t *hal = (struct hal_info_t *)hal_com->hal_priv;

	return halbb_rf_set_bb_reg(hal->bb, addr, mask, data);
}

enum rtw_hal_status
rtw_hal_bb_stainfo_init(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	if (halbb_sta_info_init(hal_info->bb, sta))
		hal_status = RTW_HAL_STATUS_SUCCESS;
	else
		PHL_ERR("[HAL] halbb_stainfo_init failed status(%d)\n",
			hal_status);

	return hal_status;
}

enum rtw_hal_status
rtw_hal_bb_stainfo_deinit(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	if (halbb_sta_info_deinit(hal_info->bb, sta))
		hal_status = RTW_HAL_STATUS_SUCCESS;
	else
		PHL_ERR("[HAL] rtw_hal_bb_stainfo_deinit failed status(%d)\n",
			hal_status);

	return hal_status;
}

enum rtw_hal_status
rtw_hal_bb_stainfo_add(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	if (halbb_sta_info_add_entry(hal_info->bb, sta))
		hal_status = RTW_HAL_STATUS_SUCCESS;
	else
		PHL_ERR("[HAL] halbb_stainfo_init failed status(%d)\n",
			hal_status);

	return hal_status;
}

enum rtw_hal_status
rtw_hal_bb_stainfo_delete(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	if (halbb_sta_info_delete_entry(hal_info->bb, sta))
		hal_status = RTW_HAL_STATUS_SUCCESS;
	else
		PHL_ERR("[HAL] rtw_hal_bb_stainfo_deinit failed status(%d)\n",
			hal_status);

	return hal_status;
}

void rtw_hal_bb_media_status_update(struct hal_info_t *hal_info,
			struct rtw_phl_stainfo_t *sta, bool is_connected)
{
	halbb_media_status_update(hal_info->bb, sta, is_connected);
}

enum rtw_hal_status
rtw_hal_bb_upt_ramask(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	if (!rtw_halbb_dft_mask(hal_info->bb, sta)) {
		PHL_ERR("rtw_halbb_set_dft_mask failed\n");
		return RTW_HAL_STATUS_FAILURE;
	}

	PHL_INFO("sta's cur_ra_mask : 0x%016llx\n", sta->hal_sta->ra_info.cur_ra_mask);

	if (sta->hal_sta->ra_info.ra_mask) {
		sta->hal_sta->ra_info.cur_ra_mask &= sta->hal_sta->ra_info.ra_mask;
		PHL_INFO("ra_mask : 0x%016llx, cur_ra_mask : 0x%016llx\n",
			sta->hal_sta->ra_info.ra_mask, sta->hal_sta->ra_info.cur_ra_mask);
	}
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ra_register(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	if (!rtw_halbb_raregistered(hal_info->bb, sta))
		return RTW_HAL_STATUS_FAILURE;

	sta->hal_sta->ra_info.ra_registered = true;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ra_deregister(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	if (!rtw_halbb_ra_deregistered(hal_info->bb, sta))
		PHL_ERR("rtw_halbb_ra_deregistered failed\n");

	sta->hal_sta->ra_info.ra_registered = false;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ra_update(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	if (!sta->hal_sta->ra_info.ra_registered) {
		PHL_WARN("%s mac-id:%d not register RA\n", __func__, sta->macid);
		return RTW_HAL_STATUS_SUCCESS;
	}

	if (sta->hal_sta->ra_info.ra_mask) {
		sta->hal_sta->ra_info.cur_ra_mask &= sta->hal_sta->ra_info.ra_mask;
		PHL_INFO("ra_mask : 0x%016llx, cur_ra_mask : 0x%016llx\n",
			sta->hal_sta->ra_info.ra_mask, sta->hal_sta->ra_info.cur_ra_mask);
	}

	if (rtw_halbb_raupdate(hal_info->bb, sta) == true)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

#ifdef CONFIG_BTCOEX
enum rtw_hal_status rtw_hal_btc_cfg_tx_1ss(struct rtw_hal_com_t *hal_com,
			struct rtw_phl_com_t *phl_com, u8 rid, bool enable)
{
	enum rtw_hal_status hstats = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_i = (struct hal_info_t *)hal_com->hal_priv;
	void *drv = halcom_to_drvpriv(hal_com);
	struct rtw_wifi_role_t *wrole = rtw_phl_get_wrole_by_ridx(phl_com, rid);
	struct phl_queue *sta_queue = NULL;
	struct rtw_phl_stainfo_t *sta = NULL;
	u8 ra_nss_limit = enable ? 1 : 0;

	PHL_PRINT("%s: rid(%d), enable(%d)\n", __FUNCTION__, rid, enable);
	if (wrole == NULL) {
		PHL_ERR("%s: Get role failed\n", __FUNCTION__);
		goto exit;
	}
	sta_queue = &wrole->assoc_sta_queue;
	_os_spinlock(drv, &sta_queue->lock, _bh, NULL);
	phl_list_for_loop(sta, struct rtw_phl_stainfo_t,
				&sta_queue->queue, list) {
		if (!sta)
			continue;
		sta->hal_sta->ra_info.ra_nss_limit = ra_nss_limit;
		hstats = rtw_hal_bb_ra_update(hal_i, sta);
		if (RTW_HAL_STATUS_SUCCESS != hstats) {
			PHL_ERR("%s: macid(%d), Fail to cfg ra_nss_limit(%d)\n",
				__FUNCTION__, sta->macid, ra_nss_limit);
			break;
		} else {
			PHL_PRINT("%s: macid(%d), succee to cfg ra_nss_limit(%d)\n",
				__FUNCTION__, sta->macid, ra_nss_limit);
		}
	}
	_os_spinunlock(drv, &sta_queue->lock, _bh, NULL);
exit:
	return hstats;
}
#endif /* CONFIG_BTCOEX */

enum rtw_hal_status
rtw_hal_bb_query_txsts_rpt(struct hal_info_t *hal_info,
				u16 macid0, u16 macid1)
{
	if (!rtw_halbb_query_txsts(hal_info->bb, macid0, macid1))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

#define INVALID_ARFR_IDX 0xFF
#define rtw_halbb_get_arfr_idx(_bb, _sta)	INVALID_ARFR_IDX
u8 rtw_hal_bb_get_arfr_idx(struct hal_info_t *hal_info,
				struct rtw_phl_stainfo_t *sta)
{
	return rtw_halbb_get_arfr_idx(hal_info->bb, sta);
}

enum rtw_hal_status
rtw_hal_bb_get_efuse_info(struct rtw_hal_com_t *hal_com,
	u8 *efuse_map, enum rtw_efuse_info info_type, void *value,
	u8 size, u8 map_valid)
{
	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

u8 hal_get_primary_channel_idx(u8 pri_ch,
				u8 central_ch, enum channel_width bw,
				enum chan_offset bw_offset)
{
	u8 idx = 255;

	if(bw == CHANNEL_WIDTH_80) {
		if(bw_offset == CHAN_OFFSET_NO_EXT ||
		   bw_offset == CHAN_OFFSET_NO_DEF) {
		   PHL_ERR("%s invalid bw offset\n",__FUNCTION__);
		   return idx;
		}
		if (central_ch > pri_ch)
			idx = (bw_offset == CHAN_OFFSET_UPPER) ? (4) : (2);
		else
			idx = (bw_offset == CHAN_OFFSET_UPPER) ? (1) : (3);
	}
	else if(bw == CHANNEL_WIDTH_40) {
		if(bw_offset == CHAN_OFFSET_NO_EXT ||
		   bw_offset == CHAN_OFFSET_NO_DEF) {
		   PHL_ERR("%s invalid bw offset\n",__FUNCTION__);
		   return idx;
		}
		idx = (bw_offset == CHAN_OFFSET_UPPER) ? (2) : (1);
	}
	else {
		idx = 0;
	}
	PHL_INFO("Using SC index %u for P%u C%u B%u O%u\n",
		 idx, pri_ch, central_ch, bw, bw_offset);
	return idx;
}

enum rtw_hal_status rtw_hal_bb_set_ch_bw(struct hal_info_t *hal_info,
					enum phl_phy_idx phy_idx,
					u8 pri_ch,
					u8 central_ch_seg0,
					u8 central_ch_seg1,
					enum band_type band,
					enum channel_width bw)
{
	if(halbb_ctrl_bw_ch(hal_info->bb, pri_ch, central_ch_seg0,
				central_ch_seg1, band, bw, phy_idx) == false)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

#ifdef CONFIG_PHL_CUSTOM_FEATURE
/**
 * Early Weak Interference Rejection (EWIR)
 * rtw_hal_bb_set_pop_en: configure bb feature about packet on packet.
 * @hal_info: see struct hal_info_t
 * @en: enable: true, disable: false
 * @phy_idx: corresponding to the hw_band in wifi_role.
 * returns enum rtw_hal_status
 */
enum rtw_hal_status rtw_hal_bb_set_pop_en(struct hal_info_t *hal_info,
                                          bool en,
                                          enum phl_phy_idx phy_idx)
{
	PHL_INFO(" %s, pop_enable(%d)\n", __FUNCTION__, en);
	halbb_pop_en(hal_info->bb, en, phy_idx);
	return RTW_HAL_STATUS_SUCCESS;
}

/**
 * Early Weak Interference Rejection (EWIR)
 * rtw_hal_bb_query_pop_en: query the status of packet on packet operation.
 * @hal_info: see struct hal_info_t
 * @phy_idx: corresponding to the hw_band in wifi_role.
 * returns true: running; false: not running.
 */
bool rtw_hal_bb_query_pop_en(struct hal_info_t *hal_info,
                             enum phl_phy_idx phy_idx)
{
	return halbb_querry_pop_en(hal_info->bb, phy_idx);
}

/**
 * rtw_hal_bb_set_pkt_detect_thold: configure packet threshold(dbm) detection
 * It would pause DIG while pd threshold enabling(value > 0)
 * @hal_info: see struct hal_info_t
 * @bound: pd threshold value
 * returns enum rtw_hal_status
 */
enum rtw_hal_status rtw_hal_bb_set_pkt_detect_thold(struct hal_info_t *hal_info,
                                                    u32 bound)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	enum halbb_pause_type pause_type = HALBB_PAUSE;
	u8 pause_result = 0;
	u32 val[5] = {0};
	val[0] = bound;
	val[1] = 0;
	if (bound == 0) {
		pause_type = HALBB_RESUME;
	} else if ((bound > 102) || (bound < 40)) {
		PHL_INFO(" %s, invalid boundary!\n", __FUNCTION__);
		return hal_status;
	}
	pause_result = halbb_pause_func(hal_info->bb,
					F_DIG, pause_type,
					HALBB_PAUSE_LV_2,
					2,
					val);
	if (pause_result == PAUSE_SUCCESS)
		hal_status = RTW_HAL_STATUS_SUCCESS;
	return hal_status;
}

/**
 * rtw_hal_bb_query_pkt_detect_thold: query packet threshold(dbm) value.
 * @hal_info: see struct hal_info_t
 * @get_en_info: Caller can confirm the feature running or not with true value.
 * 1. true: for query pd threshold detection enabling or not.
 * 2. false:for query pd threshold value.
 * @phy_idx: corresponding to the hw_band in wifi_role.
 * returns pd threshold value
 */
u8 rtw_hal_bb_query_pkt_detect_thold(struct hal_info_t *hal_info,
                                     bool get_en_info,
                                     enum phl_phy_idx phy_idx)
{
	return halbb_querry_pd_lower_bound(hal_info->bb, get_en_info, phy_idx);
}
#endif

#ifdef CONFIG_PHL_DFS
bool rtw_hal_in_radar_domain(void *hal,
			u8 ch, enum channel_width bw)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return halbb_is_dfs_band(hal_info->bb, ch, bw);
}

enum rtw_hal_status
rtw_hal_bb_dfs_rpt_cfg(struct hal_info_t *hal_info, bool dfs_en)
{
	if (dfs_en)
		halbb_radar_detect_enable(hal_info->bb);
	else
		halbb_radar_detect_disable(hal_info->bb);
	return RTW_HAL_STATUS_SUCCESS;
}

bool
rtw_hal_bb_radar_detect(struct hal_info_t *hal_info,
				struct hal_dfs_rpt *hal_dfs)
{

	return halbb_radar_detect(hal_info->bb, hal_dfs);
}
#endif /*CONFIG_PHL_DFS*/

#ifdef CONFIG_PHL_CHANNEL_INFO
enum rtw_hal_status
rtw_hal_bb_ch_info_parsing(struct hal_info_t *hal_info,
	u8 *addr, u32 len, u8 *rpt_buf, struct ch_rpt_hdr_info *ch_hdr_rpt,
	struct phy_info_rpt *phy_rpt, struct ch_info_drv_rpt *drv)
{
	enum bb_ch_info_t chinfo_status = BB_CH_INFO_SUCCESS;

	chinfo_status = halbb_ch_info_parsing(hal_info->bb, addr, len, rpt_buf
		, (void*)ch_hdr_rpt, (void *)phy_rpt, (void *)drv);

	if (chinfo_status == BB_CH_INFO_SUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
	else if (chinfo_status == BB_CH_INFO_LAST_SEG)
		return RTW_HAL_STATUS_BB_CH_INFO_LAST_SEG;
	else
		return RTW_HAL_STATUS_FAILURE;
}

void
rtw_hal_bb_chan_info_cfg(struct hal_info_t *hal_info, struct chinfo_bbcr_cfg *bbcr)
{
	halbb_cfg_ch_info_cr(hal_info->bb, (void*)bbcr);
}
#endif /* CONFIG_PHL_CHANNEL_INFO */

enum rtw_hal_status
rtw_hal_bb_ctrl_btg(struct rtw_hal_com_t *hal_com, bool btg)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("[HAL] call rtw_hal_bb_ctrl_btg !!!\n");

	halbb_ctrl_btg(hal_info->bb, btg);

	return status;
}

enum rtw_hal_status
rtw_hal_bb_ctrl_btc_preagc(struct rtw_hal_com_t *hal_com, bool bt_en)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("[HAL] call rtw_hal_bb_ctrl_btc_preagc !!!\n");

	halbb_ctrl_btc_preagc(hal_info->bb, bt_en);

	return status;
}

enum rtw_hal_status
rtw_hal_bb_cfg_rx_path(struct rtw_hal_com_t *hal_com, u8 rx_path)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("[HAL] call halbb_ctrl_rx_path !!!\n");

	if(halbb_ctrl_rx_path(hal_info->bb, rx_path))
		status = RTW_HAL_STATUS_SUCCESS;
	else
		status = RTW_HAL_STATUS_FAILURE;

	return status;

}

enum rtw_hal_status
rtw_hal_bb_cfg_tx_path(struct rtw_hal_com_t *hal_com, u8 tx_path)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("[HAL] call halbb_ctrl_tx_path !!!\n");

	if(halbb_ctrl_tx_path(hal_info->bb, tx_path))
		status = RTW_HAL_STATUS_SUCCESS;
	else
		status = RTW_HAL_STATUS_FAILURE;
	return status;
}

enum rtw_hal_status  rtw_hal_bb_get_rx_ok(struct hal_info_t *hal_info, u8 cur_phy_idx, u32 *rx_ok)
{
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL BB API]%s\n", __FUNCTION__);

	*rx_ok = halbb_mp_get_rx_crc_ok(hal_info->bb, cur_phy_idx);

	return ret;
}

enum rtw_hal_status  rtw_hal_bb_get_rx_crc(struct hal_info_t *hal_info, u8 cur_phy_idx, u32 *rx_crc_err)
{
	enum rtw_hal_status ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL BB API]%s\n", __FUNCTION__);

	*rx_crc_err = halbb_mp_get_rx_crc_err(hal_info->bb, cur_phy_idx);

	return ret;
}

enum rtw_hal_status  rtw_hal_bb_set_reset_cnt(void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	PHL_INFO("[HAL] call halbb_mp_seset_cnt !!!\n");
	halbb_mp_reset_cnt(hal_info->bb);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_power(struct rtw_hal_com_t *hal_com, s16 power_dbm,
					 enum phl_phy_idx phy_idx)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	PHL_INFO("[MP HAL API] %s: call halbb_set_txpwr_dbm \n", __FUNCTION__);
	PHL_INFO("[MP HAL API] %s: power_dbm = %d\n", __FUNCTION__, power_dbm);

	if(halbb_set_txpwr_dbm(hal_info->bb, power_dbm, phy_idx))
		hal_status = RTW_HAL_STATUS_SUCCESS;
	else
		hal_status = RTW_HAL_STATUS_FAILURE;

	return hal_status;
}

enum rtw_hal_status
rtw_hal_bb_get_power(struct rtw_hal_com_t *hal_com, s16 *power_dbm,
					 enum phl_phy_idx phy_idx)
{
	int ret = RTW_HAL_STATUS_SUCCESS;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	PHL_INFO("[MP HAL API] %s: call halbb_get_txpwr_dbm \n", __FUNCTION__);
	*power_dbm = halbb_get_txpwr_dbm(hal_info->bb, phy_idx);
	PHL_INFO("[MP HAL API] %s: power_dbm = %d\n", __FUNCTION__, *power_dbm);
	return ret;
}

enum rtw_hal_status
rtw_hal_bb_set_pwr_index(void *hal, u16 pwr_idx, enum rf_path tx_path, bool is_cck)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	PHL_INFO("[MP HAL API] %s: call halbb_set_txpwr_idx\n", __FUNCTION__);

	if (is_cck) {
		if(halbb_set_cck_txpwr_idx(hal_info->bb, pwr_idx, tx_path))
			hal_status = RTW_HAL_STATUS_SUCCESS;
		else
			hal_status = RTW_HAL_STATUS_FAILURE;

	} else {
		if(halbb_set_ofdm_txpwr_idx(hal_info->bb, pwr_idx, tx_path))
			hal_status = RTW_HAL_STATUS_SUCCESS;
		else
			hal_status = RTW_HAL_STATUS_FAILURE;
	}
	return hal_status;
}

enum rtw_hal_status rtw_hal_bb_get_pwr_index(void *hal, u16 *pwr_idx, enum rf_path tx_path, bool is_cck){
	int ret = RTW_HAL_STATUS_SUCCESS;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	PHL_INFO("[MP HAL API] %s: call halbb_get_txpwr_idx\n", __FUNCTION__);

	if (is_cck) {
		PHL_INFO("[MP HAL API] call halbb_get_cck_txpwr_idx\n");
		*pwr_idx = halbb_get_cck_txpwr_idx(hal_info->bb, tx_path);

	} else {
		PHL_INFO("[MP HAL API] call halbb_get_ofdm_txpwr_idx\n");
		*pwr_idx = halbb_get_ofdm_txpwr_idx(hal_info->bb, tx_path);
	}


	return ret;
}

enum rtw_hal_status rtw_hal_bb_set_plcp_tx(struct rtw_hal_com_t *hal_com,
						struct mp_plcp_param_t *plcp_tx_struct,
						struct mp_usr_plcp_gen_in *plcp_usr_info,
						enum phl_phy_idx plcp_phy_idx,
						u8 *plcp_sts)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	PHL_INFO("%s\n", __FUNCTION__);

	*plcp_sts = halbb_plcp_gen(hal_info->bb, (void*)plcp_tx_struct,(void*)plcp_usr_info,plcp_phy_idx);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_pmac_cont_tx(struct rtw_hal_com_t *hal_com, u8 enable, u8 is_cck,
							enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	struct halbb_pmac_info tx_info = {0};

	PHL_INFO("%s: enable = %d is_cck = %d phy_idx = %d\n",
			 __FUNCTION__, enable, is_cck, phy_idx);

	tx_info.en_pmac_tx = enable;
	tx_info.is_cck = is_cck;
	tx_info.mode = CONT_TX;

	halbb_set_pmac_tx(hal_info->bb, &tx_info, phy_idx);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_pmac_packet_tx(struct rtw_hal_com_t *hal_com, u8 enable,
							  u8 is_cck, u16 tx_cnt ,u16 period, u16 tx_time,
							  enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	struct halbb_pmac_info tx_info = {0};

	PHL_INFO("%s: enable = %d is_cck = %d phy_idx = %d\n",
			 __FUNCTION__, enable, is_cck, phy_idx);
	PHL_INFO("%s: tx_cnt = %d period = %d tx_time = %d\n",
			 __FUNCTION__, tx_cnt, period, tx_time);

	tx_info.en_pmac_tx = enable;
	tx_info.is_cck = is_cck;
	tx_info.mode = PKTS_TX;
	tx_info.tx_cnt = tx_cnt;
	tx_info.period = period;
	tx_info.tx_time = tx_time;

	halbb_set_pmac_tx(hal_info->bb, &tx_info, phy_idx);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_pmac_fw_trigger_tx(struct rtw_hal_com_t *hal_com, u8 enable,
							  u8 is_cck, u16 tx_cnt, u8 tx_duty,
							  enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	struct halbb_pmac_info tx_info = {0};

	PHL_INFO("%s: enable = %d is_cck = %d phy_idx = %d\n",
			 __FUNCTION__, enable, is_cck, phy_idx);
	PHL_INFO("%s: tx_cnt = %d\n", __FUNCTION__, tx_cnt);
	PHL_INFO("%s: tx_duty = %d\n", __FUNCTION__, tx_duty);

	tx_info.en_pmac_tx = enable;
	tx_info.is_cck = is_cck;
	tx_info.mode = FW_TRIG_TX;
	tx_info.tx_cnt = tx_cnt;
	tx_info.duty_cycle = tx_duty;

	halbb_set_pmac_tx(hal_info->bb, &tx_info, phy_idx);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_parse_phy_sts(void *hal, void *ppdu_sts,
			 struct rtw_phl_rx_pkt *phl_rx, u8 is_su)
{
	enum rtw_hal_status hstutus = RTW_HAL_STATUS_SUCCESS;

	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct hal_ppdu_sts *hal_ppdu = (struct hal_ppdu_sts *)ppdu_sts;
	struct rtw_r_meta_data *mdata = &(phl_rx->r.mdata);
	struct rtw_phl_ppdu_phy_info *phy_info = &(phl_rx->r.phy_info);
	struct physts_rxd rxdesc = {0};
	struct physts_result bb_rpt = {0};
	u8 i = 0;


	rxdesc.data_rate = mdata->rx_rate;
	rxdesc.gi_ltf = mdata->rx_gi_ltf;
	rxdesc.is_su = is_su;
	rxdesc.macid_su = (u8)mdata->macid;
	rxdesc.user_num = hal_ppdu->usr_num;
	rxdesc.is_to_self = mdata->a1_match;
	rxdesc.phy_idx = (mdata->bb_sel == 0) ? HW_PHY_0 : HW_PHY_1;

	for (i = 0; i < rxdesc.user_num; i++) {
		rxdesc.user_i[i].macid = (u8)hal_ppdu->usr[i].macid;
		rxdesc.user_i[i].is_data = hal_ppdu->usr[i].has_data;
		rxdesc.user_i[i].is_ctrl = hal_ppdu->usr[i].has_ctrl;
		rxdesc.user_i[i].is_mgnt = hal_ppdu->usr[i].has_ctrl;
		rxdesc.user_i[i].is_bcn = hal_ppdu->usr[i].has_bcn;
	}

	halbb_physts_parsing(hal_info->bb, hal_ppdu->phy_st_ptr,
			     (u16)hal_ppdu->phy_st_size,
			     &rxdesc, &bb_rpt);

	if ((bb_rpt.rssi_avg != 0) || (bb_rpt.physts_rpt_valid == 1)) {
		phy_info->is_valid = true;
		/* rssi from bb rpt, bit 0 is 0.0 ~ 0.9, removed it */
		phy_info->rssi = (bb_rpt.rssi_avg >> 1);
		phy_info->ch_idx = bb_rpt.ch_idx;
		phy_info->tx_bf = bb_rpt.is_bf;
		for (i = 0; i < RTW_PHL_MAX_RF_PATH; i++) {
			/* rssi from bb rpt, bit 0 is 0.0 ~ 0.9, removed it */
			phy_info->rssi_path[i] = (bb_rpt.rssi[i] >> 1);
			PHL_TRACE(COMP_PHL_PSTS, _PHL_INFO_,
					"phy_info->rssi_path[%d] = %d\n",
					i, phy_info->rssi_path[i]);
		}
#ifdef RTW_WKARD_SINGLE_PATH_RSSI
		if (RF_PATH_A == hal_info->hal_com->cur_rx_rfpath) {
			phy_info->rssi = phy_info->rssi_path[0];
		} else if (RF_PATH_B == hal_info->hal_com->cur_rx_rfpath) {
			phy_info->rssi = phy_info->rssi_path[1];
		} else if (RF_PATH_C == hal_info->hal_com->cur_rx_rfpath) {
			phy_info->rssi = phy_info->rssi_path[2];
		} else if (RF_PATH_D == hal_info->hal_com->cur_rx_rfpath) {
			phy_info->rssi = phy_info->rssi_path[3];
		}
#endif
		PHL_TRACE(COMP_PHL_PSTS, _PHL_INFO_,
				"avg_rssi %d ; ch_idx %d\n",
				phy_info->rssi, phy_info->ch_idx);
	}

	return hstutus;
}

enum rtw_hal_status rtw_hal_bb_get_tx_ok(void *hal, u8 cur_phy_idx, u32 *tx_ok)
{
	/* struct hal_info_t *hal_info = (struct hal_info_t *)hal; */

	PHL_INFO("[HAL] call get halbb_mp_get_tx_ok !!!\n");
	/*tx_ok = halbb_mp_get_tx_ok(hal_info->bb, cur_phy_idx);*/
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_get_txpwr_ref(struct hal_info_t *hal_info, u8 is_cck, u8 tx_path, s16 *txpwr_ref)
{
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API]%s\n", __FUNCTION__);

	if (is_cck)
		*txpwr_ref = halbb_get_cck_ref_dbm(hal_info->bb, tx_path);
	else
		*txpwr_ref = halbb_get_ofdm_ref_dbm(hal_info->bb, tx_path);

	PHL_INFO("[MP HAL API] %s: txpwr_ref = %x\n", __FUNCTION__, *txpwr_ref);

	return ret;
}

enum rtw_hal_status rtw_hal_bb_get_rssi(struct hal_info_t *hal_info, enum rf_path rx_path, u8 *rssi)
{
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API]%s\n", __FUNCTION__);

	*rssi = halbb_mp_get_rssi(hal_info->bb, rx_path);

	return ret;
}

enum rtw_hal_status rtw_hal_bb_get_rssi_ex(struct hal_info_t *hal_info, enum rf_path rx_path, s16 *rssi, u8 cur_phy_idx)
{
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API]%s\n", __FUNCTION__);

	/*
	ToDo: Should follow halbb master
	[HALBB][8852A][B-Cut] Refine rssi structure from s32 to s8
	sha-1:f301ebb0e7cd0c1521d3c67e94c2dd4434d87f44

	struct rssi_physts  rssi_t;
	*rssi_t = halbb_get_mp_rssi_physts(hal_info->bb, RF_PATH_AB, cur_phy_idx);
	*rssi = rssi_t.rssi_seg[cur_phy_idx].rssi[0];
	*rssi = (*rssi<<8)|rssi_t.rssi_seg[cur_phy_idx].rssi[1];
	*/
	return ret;
}

enum rtw_hal_status rtw_hal_bb_get_rxevm(struct hal_info_t *hal_info, u8 user, u8 strm, u8 rxevm_table, u8 *rx_evm)
{
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API]%s\n", __FUNCTION__);

	*rx_evm = halbb_mp_get_rxevm(hal_info->bb,  user, strm, rxevm_table);

	return ret;
}

enum rtw_hal_status rtw_hal_bb_trigger_rxevm(struct hal_info_t *hal_info, u8 cur_phy_idx,
	u32 *phy0_user0_rxevm, u32 *phy0_user1_rxevm, u32 *phy0_user2_rxevm, u32 *phy0_user3_rxevm,
	u32 *phy1_user0_rxevm, u32 *phy1_user1_rxevm, u32 *phy1_user2_rxevm, u32 *phy1_user3_rxevm)
{

	struct rxevm_physts evm;
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API]%s\n", __FUNCTION__);

	evm = halbb_mp_get_rxevm_physts(hal_info->bb, cur_phy_idx);
	*phy0_user0_rxevm = evm.rxevm_seg[0].rxevm_user[0].rxevm_ss_3;
	*phy0_user0_rxevm = (*phy0_user0_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[0].rxevm_ss_2;
	*phy0_user0_rxevm = (*phy0_user0_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[0].rxevm_ss_1;
	*phy0_user0_rxevm = (*phy0_user0_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[0].rxevm_ss_0;

	*phy0_user1_rxevm = evm.rxevm_seg[0].rxevm_user[1].rxevm_ss_3;
	*phy0_user1_rxevm = (*phy0_user1_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[1].rxevm_ss_2;
	*phy0_user1_rxevm = (*phy0_user1_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[1].rxevm_ss_1;
	*phy0_user1_rxevm = (*phy0_user1_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[1].rxevm_ss_0;

	*phy0_user2_rxevm = evm.rxevm_seg[0].rxevm_user[2].rxevm_ss_3;
	*phy0_user2_rxevm = (*phy0_user2_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[2].rxevm_ss_2;
	*phy0_user2_rxevm = (*phy0_user2_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[2].rxevm_ss_1;
	*phy0_user2_rxevm = (*phy0_user2_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[2].rxevm_ss_0;

	*phy0_user3_rxevm = evm.rxevm_seg[0].rxevm_user[3].rxevm_ss_3;
	*phy0_user3_rxevm = (*phy0_user3_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[3].rxevm_ss_2;
	*phy0_user3_rxevm = (*phy0_user3_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[3].rxevm_ss_1;
	*phy0_user3_rxevm = (*phy0_user3_rxevm<<8)|evm.rxevm_seg[0].rxevm_user[3].rxevm_ss_0;

	*phy1_user0_rxevm = evm.rxevm_seg[1].rxevm_user[0].rxevm_ss_3;
	*phy1_user0_rxevm = (*phy1_user0_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[0].rxevm_ss_2;
	*phy1_user0_rxevm = (*phy1_user0_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[0].rxevm_ss_1;
	*phy1_user0_rxevm = (*phy1_user0_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[0].rxevm_ss_0;

	*phy1_user1_rxevm = evm.rxevm_seg[1].rxevm_user[1].rxevm_ss_3;
	*phy1_user1_rxevm = (*phy1_user1_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[1].rxevm_ss_2;
	*phy1_user1_rxevm = (*phy1_user1_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[1].rxevm_ss_1;
	*phy1_user1_rxevm = (*phy1_user1_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[1].rxevm_ss_0;

	*phy1_user2_rxevm = evm.rxevm_seg[1].rxevm_user[2].rxevm_ss_3;
	*phy1_user2_rxevm = (*phy1_user2_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[2].rxevm_ss_2;
	*phy1_user2_rxevm = (*phy1_user2_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[2].rxevm_ss_1;
	*phy1_user2_rxevm = (*phy1_user2_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[2].rxevm_ss_0;

	*phy1_user3_rxevm = evm.rxevm_seg[1].rxevm_user[3].rxevm_ss_3;
	*phy1_user3_rxevm = (*phy1_user3_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[3].rxevm_ss_2;
	*phy1_user3_rxevm = (*phy1_user3_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[3].rxevm_ss_1;
	*phy1_user3_rxevm = (*phy1_user3_rxevm<<8)|evm.rxevm_seg[1].rxevm_user[3].rxevm_ss_0;

	return ret;
}

/* mode: 0 = tmac, 1 = pmac */
enum rtw_hal_status
rtw_hal_bb_tx_mode_switch(struct rtw_hal_com_t *hal_com,
						  enum phl_phy_idx phy_idx,
						  u8 mode)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	if(mode == 0) {
		PHL_INFO("%s: tmac mode\n", __FUNCTION__);
		halbb_set_tmac_tx(hal_info->bb, phy_idx);
	}
	else {
		PHL_INFO("%s: pmac mode\n", __FUNCTION__);
	}

	return ret;
}

enum rtw_hal_status rtw_hal_bb_set_txsc(struct hal_info_t *hal_info, u8 txsc,
										enum phl_phy_idx phy_idx)
{
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;
	PHL_INFO("[HAL] call halbb_set_txsc !!!\n");

	if(halbb_set_txsc(hal_info->bb, txsc, phy_idx))
		status = RTW_HAL_STATUS_SUCCESS;
	else
		status = RTW_HAL_STATUS_FAILURE;

	return status;
}


u8 rtw_hal_bb_get_txsc(struct rtw_hal_com_t *hal_com, u8 pri_ch,
		u8 central_ch, enum channel_width cbw, enum channel_width dbw)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)(hal_com->hal_priv);

	return halbb_get_txsc(hal_info->bb, pri_ch, central_ch, cbw, dbw);
}

u32 rtw_hal_bb_process_c2h(void *hal, struct rtw_c2h_info *c2h)
{
#ifdef RTW_WKARD_BB_C2H
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	u8 cls = c2h->c2h_class;
	u8 func = c2h->c2h_func;
	u16 len = c2h->content_len;
	u8 *buf = c2h->content;

	return rtw_halbb_c2h_parsing(hal_info->bb, cls, func, (u8)len, buf);
#else
	return 0;
#endif
}

u16 rtw_hal_bb_get_su_rx_rate(struct rtw_hal_com_t *hal_com)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)(hal_com->hal_priv);
	struct bb_pkt_cnt_su_info rpt;
	u16 max_num_tmp = 0;
	u16 rx_rate = 0;
	u16 i = 0;
	u16 *pkt_cnt_tmp;
	u8 rate_num_tmp;
	u16 ofst_mode = 0;
	u16 ofst_ss = 0;
	u16 idx = 0;
	bool is_ht_mode = false;
	bool is_legacy_rate = true;

	halbb_get_rx_pkt_cnt_rpt_su(hal_info->bb, &rpt);

	/*Legacy rate*/
	if (rpt.pkt_cnt_cck || rpt.pkt_cnt_ofdm) {
		for (i = 0; i < 12; i++) {
			if (rpt.pkt_cnt_legacy[i] >= max_num_tmp) {
				max_num_tmp = rpt.pkt_cnt_legacy[i];
				rx_rate = i;
			}
		}
	}
	if (rpt.pkt_cnt_t == 0) {
		return rx_rate;
	}
	/*HT, VHT, HE*/
	if (rpt.he_pkt_not_zero) {
		pkt_cnt_tmp = rpt.pkt_cnt_he;
		rate_num_tmp = 24;
		ofst_mode = 0x180;
	} else if (rpt.vht_pkt_not_zero) {
		pkt_cnt_tmp = rpt.pkt_cnt_vht;
		rate_num_tmp = 24;
		ofst_mode = 0x100;
	} else if (rpt.ht_pkt_not_zero) {
		pkt_cnt_tmp = rpt.pkt_cnt_ht;
		rate_num_tmp = 16;
		ofst_mode = 0x80;
		is_ht_mode = true;
	} else {
		return rx_rate;
	}
	for (i = 0; i < rate_num_tmp; i++) {
		if (pkt_cnt_tmp[i] >= max_num_tmp) {
			max_num_tmp = pkt_cnt_tmp[i];
			idx = i;
			is_legacy_rate = false;
		}
	}
	if (is_legacy_rate)
		return rx_rate;
	if (!is_ht_mode) {
		ofst_ss = idx / HE_VHT_NUM_MCS;

		if (ofst_ss >= 0) /*>=2SS*/
			idx -= (ofst_ss * HE_VHT_NUM_MCS);
	}
	rx_rate = ofst_mode + (ofst_ss << 4) + idx;
	return rx_rate;
}

#ifdef CONFIG_DBCC_SUPPORT
enum rtw_hal_status
rtw_hal_phy_dbcc_pre_cfg(struct hal_info_t *hal_info,
			struct rtw_phl_com_t *phl_com, bool dbcc_en)
{

	rtw_hal_init_bb_reg(hal_info);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_phy_dbcc_cfg(struct hal_info_t *hal_info,
	struct rtw_phl_com_t *phl_com, bool dbcc_en)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	/* BB DBCC Settings */
	rtw_hal_bb_cfg_dbcc(hal_info, dbcc_en);
	/* enable/disable rf */
	hal_status = rtw_hal_rf_ctrl_dbcc(hal_info->hal_com, dbcc_en);
	if (RTW_HAL_STATUS_SUCCESS != hal_status) {
		hal_status = RTW_HAL_STATUS_FAILURE;
	}

	return hal_status;
}
#endif

enum rtw_hal_status
rtw_hal_bb_get_txinfo_power(struct hal_info_t *hal_info,
					s16 *txinfo_power_dbm)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s: call halbb_get_txinfo_txpwr_dbm \n",
		__FUNCTION__);

	*txinfo_power_dbm = halbb_get_txinfo_txpwr_dbm(hal_info->bb);

	PHL_INFO("[MP HAL API] %s: txinfo_power_dbm = %d\n",
		__FUNCTION__, *txinfo_power_dbm);
	return ret;
}

enum rtw_hal_status
rtw_hal_bb_ctrl_rf_mode(struct hal_info_t *hal_info,
					enum phl_rf_mode rf_mode){
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s: call rtw_hal_bb_ctrl_rf_mode \n",
		__FUNCTION__);

	halbb_ctrl_rf_mode(hal_info->bb, rf_mode);

	return ret;
}

enum rtw_hal_status
rtw_hal_bb_set_sta_id(struct hal_info_t *hal_info,
	u16 staid, enum phl_phy_idx phy_idx)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	halbb_set_sta_id(hal_info->bb, staid, phy_idx);

	return ret;
}

enum rtw_hal_status
rtw_hal_bb_set_bss_color(struct hal_info_t *hal_info,
	u8 bsscolor, enum phl_phy_idx phy_idx)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	halbb_set_bss_color(hal_info->bb, bsscolor, phy_idx);

	return ret;
}

#ifdef RTW_WKARD_DEF_CMACTBL_CFG
enum rtw_hal_status
rtw_hal_bb_trx_path_cfg(struct hal_info_t *hal_info,
		enum rf_path tx, u8 tx_nss, enum rf_path rx, u8 rx_nss)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	halbb_ctrl_trx_path(hal_info->bb, tx, tx_nss, rx, rx_nss);
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_,
		"tx_ant(%d) tx_nss(%d) rx_ant(%d) rx_nss(%d)\n",
		tx, tx_nss, rx, rx_nss);
	return ret;
}

u16 rtw_hal_bb_cfg_cmac_tx_ant(struct hal_info_t *hal_info,
	enum rf_path tx_path)
{
	u16 ret;

	ret = (BIT(RF_PATH_A) | BIT(RF_PATH_B))|(RF_PATH_A<<4)|(RF_PATH_B<<6);
	ret = halbb_cfg_cmac_tx_ant(hal_info->bb, tx_path);

	return ret;
}
#endif

enum rtw_hal_status
rtw_hal_bb_backup_info(struct rtw_hal_com_t *hal_com, u8 cur_phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s ==>start\n", __FUNCTION__);

	halbb_backup_info(hal_info->bb, cur_phy_idx);

	PHL_INFO("[MP HAL API] %s ==>end\n", __FUNCTION__);

	return ret;
}

enum rtw_hal_status
rtw_hal_bb_restore_info(struct rtw_hal_com_t *hal_com, u8 cur_phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s ==>start\n", __FUNCTION__);

	halbb_restore_info(hal_info->bb, cur_phy_idx);

	PHL_INFO("[MP HAL API] %s ==>end\n", __FUNCTION__);

	return ret;
}

void rtw_hal_bb_set_tx_pow_ref(struct rtw_hal_com_t *hal_com,
			       enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	halbb_set_tx_pow_ref(hal_info->bb, phy_idx);
}

#ifdef CONFIG_RTW_ACS
void rtw_hal_bb_acs_mntr_trigger(struct hal_info_t *hal_info, u16 monitor_time)
{
	struct ccx_para_info para = {0};
	struct env_trig_rpt trig_rpt = {0};

	para.rac_lv = RAC_LV_3;
	para.mntr_time = monitor_time;
	/*clm para*/
	para.clm_app = CLM_ACS;
	para.clm_input_opt = CLM_CCA_P20;

	/*nhm para*/
	para.nhm_app = NHM_ACS;
	para.nhm_incld_cca = NHM_INCLUDE_CCA;

	halbb_env_mntr_trigger(hal_info->bb, &para, &trig_rpt);
}

enum rtw_hal_status rtw_hal_bb_acs_mntr_result(struct hal_info_t *hal_info, void *rpt)
{
	u8 result = 0;
	struct env_mntr_rpt mntr_rpt = {0};
	struct auto_chan_sel_report *acs_rpt = (struct auto_chan_sel_report *)rpt;

	result = halbb_env_mntr_result(hal_info->bb, &mntr_rpt);

	if ((result & (CLM_SUCCESS | NHM_SUCCESS)) != (CLM_SUCCESS | NHM_SUCCESS)) {
		return RTW_HAL_STATUS_FAILURE;
	} else {
		acs_rpt->clm_ratio = mntr_rpt.clm_ratio;
		acs_rpt->nhm_pwr = mntr_rpt.nhm_pwr;
		return RTW_HAL_STATUS_SUCCESS;
	}
}
#endif /* CONFIG_RTW_ACS */

enum rtw_hal_status
rtw_hal_bb_tssi_bb_reset(struct rtw_hal_com_t *hal_com)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	halbb_tssi_bb_reset(hal_info->bb);

	return RTW_HAL_STATUS_SUCCESS;
}
#ifdef RTW_WKARD_DYNAMIC_BFEE_CAP
void rtw_hal_bb_dcr_en(struct hal_info_t *hal_info, bool en)
{
	halbb_dcr_en(hal_info->bb, en);
	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_,
		  "rtw_hal_bb_dcr_en : %d \n", (en ? 1 : 0));
	 return;
}

bool rtw_hal_bb_csi_rsp(struct hal_info_t *hal_info)
{
	bool ret = true;
	if (0 == halbb_dyn_csi_rsp_rlt_get(hal_info->bb)) {
		ret = false;
	}
	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_,
		  "rtw_hal_bb_csi_rsp : %d \n", (ret ? 1 : 0));
	return ret;
}
#endif

void rtw_hal_bb_notification(struct hal_info_t *hal_info,
                             enum phl_msg_evt_id event,
                             enum phl_phy_idx phy_idx)
{
	halbb_wifi_event_notify(hal_info->bb, event, phy_idx);
}

void rtw_hal_bb_cmd_notification(struct hal_info_t *hal_info,
                             void *hal_cmd,
                             enum phl_phy_idx phy_idx)
{
	halbb_bb_cmd_notify(hal_info->bb, hal_cmd, phy_idx);
}

enum rtw_hal_status
rtw_hal_bb_set_gain_offset(struct hal_info_t *hal_info, s8 rx_gain_offset,
				enum rf_path rx_path, enum phl_phy_idx phy_idx, u8 iscck)
{
	if(iscck)
		halbb_normal_efuse_verify_cck(hal_info->bb, rx_gain_offset, rx_path, phy_idx);
	else
		halbb_normal_efuse_verify(hal_info->bb, rx_gain_offset, rx_path, phy_idx);

	return RTW_HAL_STATUS_SUCCESS;
}

void rtw_hal_bb_get_efuse_init(struct rtw_hal_com_t *hal_com)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	halbb_get_efuse_init(hal_info->bb);
	PHL_INFO("[HAL API] %s ==>end\n", __FUNCTION__);
}
enum rtw_hal_status
rtw_hal_bb_set_dpd_bypass(struct rtw_hal_com_t *hal_com, bool pdp_bypass,
						enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	PHL_INFO("%s: pdp_bypass = %d phy_idx = %d\n",
			 __FUNCTION__, pdp_bypass, phy_idx);

	halbb_dpd_bypass(hal_info->bb, pdp_bypass, phy_idx);
	return RTW_HAL_STATUS_SUCCESS;
}


void rtw_hal_bb_gpio_setting(struct rtw_hal_com_t *hal_com, u8 gpio_idx, u8 path,
			bool inv,  u8 src)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)(hal_com->hal_priv);

	halbb_gpio_setting(hal_info->bb, gpio_idx, path, inv, src);
}

void rtw_hal_bb_gpio_setting_all(struct rtw_hal_com_t *hal_com, u8 rfe_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)(hal_com->hal_priv);

	halbb_gpio_setting_all(hal_info->bb, rfe_idx);
}

bool rtw_hal_bb_check_tx_idle(struct hal_info_t *hal_info, enum phl_phy_idx phy_idx)
{
	return halbb_chk_tx_idle(hal_info->bb, phy_idx);
}

static enum rtw_hal_status
_cnvrt_rainfo_to_rate(enum hal_rate_mode rate_mode, u8 mcs_ss_idx,
		      enum rtw_data_rate *data_rate)
{
	enum rtw_hal_status hal_sts = RTW_HAL_STATUS_FAILURE;
	u16 rate_idx = 0, ss = 0;

	switch(rate_mode) {
	case HAL_LEGACY_MODE:
		rate_idx = (u16)(mcs_ss_idx & 0xf);
		ss = 0;
		hal_sts = RTW_HAL_STATUS_SUCCESS;
		break;
	case HAL_HT_MODE:
		rate_idx = (u16)(mcs_ss_idx & 0x1f);
		ss = 0;
		hal_sts = RTW_HAL_STATUS_SUCCESS;
		break;
	case HAL_VHT_MODE:
	case HAL_HE_MODE:
		rate_idx = (u16)(mcs_ss_idx & 0xf);
		ss = (mcs_ss_idx & 0x30) >> 4;
		hal_sts = RTW_HAL_STATUS_SUCCESS;
		break;
	default:
		PHL_TRACE(COMP_PHL_XMIT, _PHL_ERR_,
			  "%s : incorrect rate mode(0x%x), fail to covert rate\n",
			  __func__, rate_mode);
		hal_sts = RTW_HAL_STATUS_FAILURE;
		break;
	}

	if (RTW_HAL_STATUS_SUCCESS == hal_sts)
		*data_rate = rate_idx | (ss << 4) | (rate_mode << 7);

	return hal_sts;
}

enum rtw_hal_status
rtw_hal_bb_ic_hw_setting_init(struct hal_info_t *hal_info)
{
	halbb_dm_init(hal_info->bb, HW_PHY_0);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_query_rainfo(void *hal, struct rtw_hal_stainfo_t *hal_sta,
			struct rtw_phl_rainfo *phl_rainfo)
{
	enum rtw_hal_status hal_sts = RTW_HAL_STATUS_FAILURE;
	struct rtw_rate_info *rainfo = NULL;

	do {
		if (NULL == hal_sta) {
			PHL_TRACE(COMP_PHL_XMIT, _PHL_ERR_,
				  "%s : hal_sta is NULL\n",
				  __func__);
			break;
		}

		if (NULL == phl_rainfo) {
			PHL_TRACE(COMP_PHL_XMIT, _PHL_ERR_,
				  "%s : Input parameter is NULL\n",
				  __func__);
			break;
		}

		rainfo = &hal_sta->ra_info.rpt_rt_i;

		phl_rainfo->gi_ltf = rainfo->gi_ltf;
		if (HAL_RATE_BW_20 == rainfo->bw) {
			phl_rainfo->bw = CHANNEL_WIDTH_20;
		} else if (HAL_RATE_BW_40 == rainfo->bw) {
			phl_rainfo->bw = CHANNEL_WIDTH_40;
		} else if (HAL_RATE_BW_80 == rainfo->bw) {
			phl_rainfo->bw = CHANNEL_WIDTH_80;
		} else if (HAL_RATE_BW_160 == rainfo->bw) {
			phl_rainfo->bw = CHANNEL_WIDTH_160;
		} else {
			PHL_TRACE(COMP_PHL_XMIT, _PHL_ERR_,
				  "%s : incorrect bw(0x%x), fail to covert rate\n",
				  __func__, rainfo->bw);
			break;
		}

		if (RTW_HAL_STATUS_SUCCESS ==
		    _cnvrt_rainfo_to_rate(rainfo->mode,
					  rainfo->mcs_ss_idx,
					  &phl_rainfo->rate)) {
			hal_sts = RTW_HAL_STATUS_SUCCESS;
			break;
		} else {
			break;
		}
	} while (false);

	return hal_sts;
}

void rtw_hal_bb_nhm_mntr_result(struct rtw_hal_com_t *hal_com, void *rpt, enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)(hal_com->hal_priv);
	struct env_mntr_rpt mntr_rpt = {0};
	struct watchdog_nhm_report *nhm_rpt = (struct watchdog_nhm_report *)rpt;

	halbb_env_mntr_get_bg_result(hal_info->bb, &mntr_rpt, phy_idx);

	nhm_rpt->ccx_rpt_stamp = mntr_rpt.ccx_rpt_stamp;
	nhm_rpt->ccx_rpt_result = mntr_rpt.ccx_rpt_result;
	nhm_rpt->nhm_pwr_dbm = (s8) ((s16) mntr_rpt.nhm_pwr - 110);
	nhm_rpt->nhm_ratio = mntr_rpt.nhm_ratio;
}

void rtw_hal_bb_set_pow_patten_sharp(struct rtw_hal_com_t *hal_com, u8 channel, u8 is_cck, u8 sharp_id, enum phl_phy_idx phy_idx)
{
	u8 is_ofdm = (is_cck == 0)?1:0;
	struct hal_info_t *hal_info = (struct hal_info_t *)(hal_com->hal_priv);
	struct rtw_tpu_info *tpu = &hal_info->hal_com->band[phy_idx].rtw_tpu_i;
	tpu->tx_ptrn_shap_idx = sharp_id;

	halbb_set_tx_pow_pattern_shap(hal_info->bb, channel, is_ofdm, phy_idx);
}

void rtw_hal_bb_env_rpt(struct rtw_hal_com_t *hal_com, struct rtw_env_report *env_rpt,
		     enum phl_phy_idx phy_indx)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)(hal_com->hal_priv);
	struct env_mntr_rpt bg_rpt = {0};
	u8 env_rpt_status = 0;

	halbb_env_mntr_get_bg_result(hal_info->bb, &bg_rpt, phy_indx);
	env_rpt_status = bg_rpt.ccx_rpt_result;
	env_rpt_status = env_rpt_status & CCX_SUCCESS;

	/*if ok update env rpt */
	if (env_rpt_status == CCX_SUCCESS) {
		env_rpt->clm_ratio = bg_rpt.clm_ratio;
		env_rpt->nhm_pwr = bg_rpt.nhm_pwr;
		env_rpt->nhm_ratio = bg_rpt.nhm_ratio;
		env_rpt->nhm_cca_ratio = bg_rpt.nhm_cca_ratio;
		env_rpt->rpt_status = 1;
	} else {
		env_rpt->rpt_status = 0;
	}
}

enum rtw_hal_status
rtw_hal_bb_set_tb_pwr_ofst(struct hal_info_t *hal_info,
			s16 ofst, enum phl_phy_idx phy_idx)
{
	bool ret = false;

	ret = halbb_set_pwr_ul_tb_ofst(hal_info->bb, ofst, phy_idx);
	if (ret == true)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

#ifdef CONFIG_MCC_SUPPORT
enum rtw_hal_status
rtw_hal_bb_upd_mcc_macid(struct hal_info_t *hal_info,
                         struct rtw_phl_mcc_role *mrole)
{
	bool ret = false;
	struct bb_mcc_i mi = {0};

	mi.type = mrole->wrole->type;
	mi.self_macid = (u8)mrole->macid;
	mi.chandef = mrole->chandef;
	mi.macid_bitmap = mrole->used_macid.bitmap;
	mi.macid_map_len = mrole->used_macid.len;

	ret = halbb_upd_mcc_macid(hal_info->bb, &mi);

	if (ret == true)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

void
rtw_hal_bb_mcc_stop(struct hal_info_t *hal_info)
{
	halbb_mcc_stop(hal_info->bb);
}

enum rtw_hal_status
rtw_hal_bb_mcc_start(struct hal_info_t *hal_info,
                     struct rtw_phl_mcc_role *m_role1,
                     struct rtw_phl_mcc_role *m_role2)
{
	bool ret = false;
	struct bb_mcc_i mi_1 = {0}, mi_2 = {0};

	mi_1.type = m_role1->wrole->type;
	mi_1.self_macid = (u8)m_role1->macid;
	mi_1.chandef = m_role1->chandef;
	mi_1.macid_bitmap = m_role1->used_macid.bitmap;
	mi_1.macid_map_len = m_role1->used_macid.len;

	mi_2.type = m_role2->wrole->type;
	mi_2.self_macid = (u8)m_role2->macid;
	mi_2.chandef = m_role2->chandef;
	mi_2.macid_bitmap = m_role2->used_macid.bitmap;
	mi_2.macid_map_len = m_role2->used_macid.len;

	ret = halbb_mcc_start(hal_info->bb, &mi_1, &mi_2);

	if (ret == true)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}
#endif


#else /*ifndef USE_TRUE_PHY*/
enum phl_phy_idx rtw_hal_bb_band_to_phy_idx(struct rtw_hal_com_t *hal_com, u8 band_idx)
{
	return HW_PHY_0;
}

void rtw_hal_bb_dfs_en(struct hal_info_t *hal_info, bool en)
{
}

void rtw_hal_bb_tssi_cont_en(struct hal_info_t *hal_info, bool en, enum rf_path path)
{
}

void rtw_hal_bb_adc_en(struct hal_info_t *hal_info, bool en)
{
}

void rtw_hal_bb_reset_en(struct hal_info_t *hal_info, bool en, enum phl_phy_idx phy_idx)
{
}
bool rtw_hal_bb_proc_cmd(struct hal_info_t *hal_info, struct rtw_proc_cmd *incmd,
			 char *output, u32 out_len)
{
	return true;
}

enum rtw_hal_status rtw_hal_bb_watchdog(struct hal_info_t *hal_info, u8 is_lps)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_simple_watchdog(struct hal_info_t *hal_info, u8 io_en)
{
	return RTW_HAL_STATUS_SUCCESS;
}

void rtw_hal_bb_reset(struct hal_info_t *hal_info)
{
}

void rtw_hal_bb_fw_edcca(struct hal_info_t *hal_info)
{
}

void rtw_hal_bb_dm_init(struct hal_info_t *hal_info)
{
}

void rtw_hal_bb_dm_deinit(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal_info)
{
}

enum rtw_hal_status rtw_hal_bb_ctrl_rx_cca(struct rtw_hal_com_t *hal_com,
	bool cca_en, enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_ctrl_dbcc(struct hal_info_t *hal_info, bool dbcc_en)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_cfg_dbcc(struct hal_info_t *hal_info, bool dbcc_en)
{
	return RTW_HAL_STATUS_SUCCESS;
}

u32 rtw_hal_bb_init(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}

void rtw_hal_bb_deinit(struct rtw_phl_com_t *phl_com,
		       struct hal_info_t *hal_info)
{
}

void rtw_hal_init_bb_reg(struct hal_info_t *hal_info)
{
}

void rtw_hal_bb_init_reg_by_hdr(struct hal_info_t *hal_info, u32 *folder_array,
				u32 folder_len, u8 is_form_folder, enum phl_phy_idx phy_idx)

{
}

u32 rtw_hal_read_rf_reg(struct rtw_hal_com_t *hal_com,
			enum rf_path path, u32 addr, u32 mask)
{
	return RTW_HAL_STATUS_SUCCESS;
}

bool rtw_hal_write_rf_reg(struct rtw_hal_com_t *hal_com,
			  enum rf_path path, u32 addr, u32 mask, u32 data)
{
	return RTW_HAL_STATUS_SUCCESS;
}

u32 rtw_hal_read_bb_reg(struct rtw_hal_com_t *hal_com,
			u32 addr, u32 mask)
{
	return RTW_HAL_STATUS_SUCCESS;
}

bool rtw_hal_write_bb_reg(struct rtw_hal_com_t *hal_com,
			  u32 addr, u32 mask, u32 data)
{
	return true;
}

u32 rtw_hal_bb_read_cr(struct rtw_hal_com_t *hal_com, u32 addr, u32 mask)
{
	return RTW_HAL_STATUS_SUCCESS;
}

bool rtw_hal_bb_write_cr(struct rtw_hal_com_t *hal_com, u32 addr, u32 mask,
			 u32 data)
{
	return true;
}

enum rtw_hal_status
rtw_hal_bb_stainfo_init(struct hal_info_t *hal_info,
			struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_stainfo_deinit(struct hal_info_t *hal_info,
			  struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_stainfo_add(struct hal_info_t *hal_info,
		       struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_stainfo_delete(struct hal_info_t *hal_info,
			  struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

void rtw_hal_bb_media_status_update(struct hal_info_t *hal_info,
			struct rtw_phl_stainfo_t *sta, bool is_connected)
{
}

enum rtw_hal_status
rtw_hal_bb_upt_ramask(struct hal_info_t *hal_info,
		      struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ra_register(struct hal_info_t *hal_info,
		       struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ra_deregister(struct hal_info_t *hal_info,
			 struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ra_update(struct hal_info_t *hal_info,
		     struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

u8 rtw_hal_bb_get_arfr_idx(struct hal_info_t *hal_info,
			   struct rtw_phl_stainfo_t *sta)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_get_efuse_info(struct rtw_hal_com_t *hal_com, u8 *efuse_map,
			  enum rtw_efuse_info info_type, void *value,
			  u8 size, u8 map_valid)
{
	return RTW_HAL_STATUS_SUCCESS;
}

u8 hal_get_primary_channel_idx(u8 pri_ch, u8 central_ch, enum channel_width bw,
			       enum chan_offset bw_offset)
{
	return 0;
}

enum rtw_hal_status rtw_hal_bb_set_ch_bw(struct hal_info_t *hal_info,
					 enum phl_phy_idx phy_idx,
					 u8 pri_ch,
					 u8 central_ch_seg0,
					 u8 central_ch_seg1,
					 enum band_type band,
					 enum channel_width bw)
{
	return RTW_HAL_STATUS_SUCCESS;
}

bool rtw_hal_in_radar_domain(void *hal, u8 ch, enum channel_width bw)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_dfs_rpt_cfg(struct hal_info_t *hal_info, bool dfs_en)
{
	return RTW_HAL_STATUS_SUCCESS;
}

bool rtw_hal_bb_radar_detect(struct hal_info_t *hal_info,
			     struct hal_dfs_rpt *hal_dfs)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ctrl_btg(struct rtw_hal_com_t *hal_com, bool btg)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ctrl_btc_preagc(struct rtw_hal_com_t *hal_com, bool bt_en)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_cfg_rx_path(struct rtw_hal_com_t *hal_com, u8 rx_path)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_cfg_tx_path(struct rtw_hal_com_t *hal_com, u8 tx_path)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status  rtw_hal_bb_get_rx_ok(struct hal_info_t *hal_info,
					  u8 cur_phy_idx, u32 *rx_ok)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status  rtw_hal_bb_get_rx_crc(struct hal_info_t *hal_info,
					   u8 cur_phy_idx, u32 *rx_crc_err)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status  rtw_hal_bb_set_reset_cnt(void *hal)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_power(struct rtw_hal_com_t *hal_com, s16 power_dbm,
		     enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_get_power(struct rtw_hal_com_t *hal_com, s16 *power_dbm,
		     enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_pwr_index(void *hal, u16 pwr_idx, enum rf_path tx_path, bool is_cck)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_get_pwr_index(void *hal, u16 *pwr_idx,
					     enum rf_path tx_path, bool is_cck)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_plcp_tx(struct rtw_hal_com_t *hal_com,
						struct mp_plcp_param_t *plcp_tx_struct,
						struct mp_usr_plcp_gen_in *plcp_usr_info,
						enum phl_phy_idx plcp_phy_idx,
						u8 *plcp_sts)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_pmac_cont_tx(struct rtw_hal_com_t *hal_com, u8 enable,
			    u8 is_cck, enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_pmac_packet_tx(struct rtw_hal_com_t *hal_com, u8 enable,
			      u8 is_cck, u16 tx_cnt ,u16 period, u16 tx_time,
			      enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_pmac_fw_trigger_tx(struct rtw_hal_com_t *hal_com, u8 enable,
				  u8 is_cck, u16 tx_cnt, u8 tx_duty,
				  enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_parse_phy_sts(void *hal, void *ppdu_sts,
			 struct rtw_phl_rx_pkt *phl_rx, u8 is_su)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_get_tx_ok(void *hal, u8 cur_phy_idx, u32 *tx_ok)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_get_txpwr_ref(struct hal_info_t *hal_info, u8 is_cck, u8 tx_path,
			 s16 *txpwr_ref)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_get_rssi(struct hal_info_t *hal_info,
					enum rf_path rx_path, u8 *rssi)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_get_rssi_ex(struct hal_info_t *hal_info,
					enum rf_path rx_path, s16 *rssi, u8 cur_phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_get_rxevm(struct hal_info_t *hal_info, u8 user,
					 u8 strm, u8 rxevm_table, u8 *rx_evm)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_trigger_rxevm(struct hal_info_t *hal_info, u8 cur_phy_idx,
	u32 *phy0_user0_rxevm, u32 *phy0_user1_rxevm, u32 *phy0_user2_rxevm, u32 *phy0_user3_rxevm,
	u32 *phy1_user0_rxevm, u32 *phy1_user1_rxevm, u32 *phy1_user2_rxevm, u32 *phy1_user3_rxevm)
{
	return RTW_HAL_STATUS_SUCCESS;
}

/* mode: 0 = tmac, 1 = pmac */
enum rtw_hal_status
rtw_hal_bb_tx_mode_switch(struct rtw_hal_com_t *hal_com,
			  enum phl_phy_idx phy_idx, u8 mode)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_bb_set_txsc(struct hal_info_t *hal_info, u8 txsc,
					enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_FAILURE;
}

u8 rtw_hal_bb_get_txsc(struct rtw_hal_com_t *hal_com, u8 pri_ch,
		       u8 central_ch, enum channel_width cbw, enum channel_width dbw)
{
	return 0;
}

u32 rtw_hal_bb_process_c2h(void *hal, struct rtw_c2h_info *c2h)
{
	return 0;
}

#ifdef CONFIG_DBCC_SUPPORT
enum rtw_hal_status
rtw_hal_phy_dbcc_pre_cfg(struct hal_info_t *hal_info,
			struct rtw_phl_com_t *phl_com, bool dbcc_en)
{
	return RTW_HAL_STATUS_SUCCESS;
}
enum rtw_hal_status
rtw_hal_phy_dbcc_cfg(struct hal_info_t *hal_info,
			struct rtw_phl_com_t *phl_com, bool dbcc_en)
{
	return RTW_HAL_STATUS_SUCCESS;
}
#endif

enum rtw_hal_status
rtw_hal_bb_get_txinfo_power(struct hal_info_t *hal_info,
					s16 *txinfo_power_dbm)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_ctrl_rf_mode(struct hal_info_t *hal_info,
					enum phl_rf_mode rf_mode){
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_sta_id(struct hal_info_t *hal_info,
	u16 staid, enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_set_bss_color(struct hal_info_t *hal_info,
	u8 bsscolor, enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

#ifdef RTW_WKARD_DEF_CMACTBL_CFG
enum rtw_hal_status
rtw_hal_bb_trx_path_cfg(struct hal_info_t *hal_info,
		enum rf_path tx, u8 tx_nss, enum rf_path rx, u8 rx_nss)
{
	return RTW_HAL_STATUS_SUCCESS;
}

u16 rtw_hal_bb_cfg_cmac_tx_ant(struct hal_info_t *hal_info,
	enum rf_path tx_path)
{
	return 0;
}
#endif

enum rtw_hal_status
rtw_hal_bb_backup_info(struct rtw_hal_com_t *hal_com, u8 cur_phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_bb_restore_info(struct rtw_hal_com_t *hal_com, u8 cur_phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

void rtw_hal_bb_set_tx_pow_ref(struct rtw_hal_com_t *hal_com,
			       enum phl_phy_idx phy_idx)
{
}

enum rtw_hal_status
rtw_hal_bb_tssi_bb_reset(struct rtw_hal_com_t *hal_com)
{
	return RTW_HAL_STATUS_SUCCESS;
}

#ifdef CONFIG_RTW_ACS
void rtw_hal_bb_acs_mntr_trigger(struct hal_info_t *hal_info, u16 monitor_time)
{

}

enum rtw_hal_status rtw_hal_bb_acs_mntr_result(struct hal_info_t *hal_info, void *rpt)
{
	return RTW_HAL_STATUS_FAILURE;
}
#endif /* CONFIG_RTW_ACS */
#ifdef RTW_WKARD_DYNAMIC_BFEE_CAP
void rtw_hal_bb_dcr_en(struct hal_info_t *hal_info, bool en)
{
	return;
}

bool rtw_hal_bb_csi_rsp(struct hal_info_t *hal_info)
{
	return true;
}


#endif

void rtw_hal_bb_get_efuse_init(struct rtw_hal_com_t *hal_com)
{

}

void rtw_hal_bb_notification(struct hal_info_t *hal_info,
                             enum phl_msg_evt_id event,
                             enum phl_phy_idx phy_idx)
{

}

void rtw_hal_bb_cmd_notification(struct hal_info_t *hal_info,
                             void *hal_cmd,
                             enum phl_phy_idx phy_idx)
{

}

enum rtw_hal_status
rtw_hal_bb_set_dpd_bypass(struct rtw_hal_com_t *hal_com, bool pdp_bypass,
						enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status
rtw_hal_bb_set_gain_offset(struct hal_info_t *hal_info, s8 rx_gain_offset,
				enum rf_path rx_path, enum phl_phy_idx phy_idx, u8 iscck)
{
	return RTW_HAL_STATUS_SUCCESS;
}


bool rtw_hal_bb_check_tx_idle(struct hal_info_t *hal_info, enum phl_phy_idx phy_idx)
{
	return false;
}

enum rtw_hal_status
rtw_hal_bb_ic_hw_setting_init(struct hal_info_t *hal_info)
{

	return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_bb_query_rainfo(void *hal, struct rtw_hal_stainfo_t *hal_sta,
			struct rtw_phl_rainfo *phl_rainfo)
{
	return RTW_HAL_STATUS_FAILURE;
}


void rtw_hal_bb_nhm_mntr_result(struct rtw_hal_com_t *hal_com, void *rpt, enum phl_phy_idx phy_idx)
{

}


void rtw_hal_bb_set_pow_patten_sharp(struct rtw_hal_com_t *hal_com, u8 channel, u8 is_cck, u8 sharp_id, enum phl_phy_idx phy_idx)
{

}

void rtw_hal_bb_env_rpt(struct rtw_hal_com_t *hal_com, struct rtw_env_report *env_rpt,
					 enum phl_phy_idx phy_indx)
{

}


enum rtw_hal_status
rtw_hal_bb_set_tb_pwr_ofst(struct hal_info_t *hal_info,
			s16 ofst, enum phl_phy_idx phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

#ifdef CONFIG_MCC_SUPPORT
enum rtw_hal_status
rtw_hal_bb_upd_mcc_macid(struct hal_info_t *hal_info,
                         struct rtw_phl_mcc_role *mrole)
{
	return RTW_HAL_STATUS_SUCCESS;
}

void
rtw_hal_bb_mcc_stop(struct hal_info_t *hal_info)
{

}

enum rtw_hal_status
rtw_hal_bb_mcc_start(struct hal_info_t *hal_info,
                     struct rtw_phl_mcc_role *m_role1,
                     struct rtw_phl_mcc_role *m_role2)
{
	return RTW_HAL_STATUS_SUCCESS;
}
#endif
#endif /*ifdef USE_TRUE_PHY*/
