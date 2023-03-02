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
#define _HAL_FW_C_
#include "hal_headers.h"

static void _hal_send_fwdl_hub_msg(struct rtw_phl_com_t *phl_com, u8 dl_ok)
{
	struct phl_msg msg = {0};
	u16 evt_id = (dl_ok) ? MSG_EVT_FWDL_OK : MSG_EVT_FWDL_FAIL;

	msg.inbuf = NULL;
	msg.inlen = 0;
	SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_MDL_PHY_MGNT);
	SET_MSG_EVT_ID_FIELD(msg.msg_id, evt_id);
	msg.band_idx = HW_BAND_0;
	rtw_phl_msg_hub_hal_send(phl_com, NULL, &msg);
}

static void _hal_fw_log_set(struct rtw_hal_fw_log_cfg *fl_cfg, u8 type, u32 value)
{
	switch(type) {
	case FL_CFG_TYPE_LEVEL:
		fl_cfg->level = value;
		break;

	case FL_CFG_TYPE_OUTPUT:
		fl_cfg->output |= value;
		break;

	case FL_CFG_TYPE_COMP:
		fl_cfg->comp |= value;
		break;

	case FL_CFG_TYPE_COMP_EXT:
		fl_cfg->comp_ext |= value;
		break;

	default:
		break;
	}
}

static void _hal_fw_log_clr(struct rtw_hal_fw_log_cfg *fl_cfg, u8 type, u32 value)
{
	switch(type) {
	case FL_CFG_TYPE_LEVEL:
		break;

	case FL_CFG_TYPE_OUTPUT:
		fl_cfg->output &= (~value);
		break;

	case FL_CFG_TYPE_COMP:
		fl_cfg->comp &= (~value);
		break;

	case FL_CFG_TYPE_COMP_EXT:
		fl_cfg->comp_ext &= (~value);
		break;

	default:
		break;
	}
}

static void _hal_fw_log_info(struct rtw_hal_fw_log_cfg *fl_cfg)
{
	PHL_PRINT("%s: level %d, output 0x%08x, comp 0x%08x, comp ext 0x%08x.\n",
			__func__,
			fl_cfg->level,
			fl_cfg->output,
			fl_cfg->comp,
			fl_cfg->comp_ext);
}

enum rtw_hal_status rtw_hal_fw_log_cfg(void *hal, u8 op, u8 type, u32 value)
{
	struct rtw_hal_com_t *hal_com = (struct rtw_hal_com_t *)hal;
	static struct rtw_hal_fw_log_cfg fl_cfg = {0};

	switch(op) {
	case FL_CFG_OP_SET:
		_hal_fw_log_set(&fl_cfg, type, value);
		break;
	case FL_CFG_OP_CLR:
		_hal_fw_log_clr(&fl_cfg, type, value);
		break;
	case FL_CFG_OP_INFO:
		_hal_fw_log_info(&fl_cfg);
		break;
	default:
		break;
	}

	return rtw_hal_mac_fw_log_cfg(hal_com, &fl_cfg);
}

void hal_fw_en_basic_log(struct rtw_hal_com_t *hal_com)
{
	rtw_hal_fw_log_cfg(hal_com, FL_CFG_OP_SET, FL_CFG_TYPE_LEVEL,
				FL_LV_LOUD);
	rtw_hal_fw_log_cfg(hal_com, FL_CFG_OP_SET, FL_CFG_TYPE_OUTPUT,
				FL_OP_C2H);
	rtw_hal_fw_log_cfg(hal_com, FL_CFG_OP_SET, FL_CFG_TYPE_COMP,
				FL_COMP_TASK);
}

enum rtw_hal_status rtw_hal_cfg_fw_ps_log(void *hal, u8 en)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	if(en)
		return rtw_hal_fw_log_cfg(hal_info->hal_com, FL_CFG_OP_SET,
						FL_CFG_TYPE_COMP, FL_COMP_PS);
	else
		return rtw_hal_fw_log_cfg(hal_info->hal_com, FL_CFG_OP_CLR,
						FL_CFG_TYPE_COMP, FL_COMP_PS);
}

enum rtw_hal_status rtw_hal_cfg_fw_mcc_log(void *hal, u8 en)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;

	if(en)
		status = rtw_hal_fw_log_cfg(hal_info->hal_com, FL_CFG_OP_SET,
					FL_CFG_TYPE_COMP, MAC_AX_FL_COMP_MCC);
	else
		status = rtw_hal_fw_log_cfg(hal_info->hal_com, FL_CFG_OP_CLR,
					FL_CFG_TYPE_COMP, MAC_AX_FL_COMP_MCC);

	PHL_INFO("rtw_hal_cfg_fw_mcc_log(): status(%d), en(%d)\n", status, en);
	return status;
}

enum rtw_hal_status
rtw_hal_download_fw(struct rtw_phl_com_t *phl_com, void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct rtw_fw_info_t *fw_info = &phl_com->fw_info;

	FUNCIN_WSTS(hal_status);

	if (!fw_info->fw_en)
		return hal_status;

	if (fw_info->dlrom_en) {
		hal_status = rtw_hal_mac_romdl(hal_info, fw_info->rom_buff,
									   fw_info->rom_size);

		if (hal_status != RTW_HAL_STATUS_SUCCESS)
			return hal_status;
	}

	hal_status = rtw_hal_mac_disable_cpu(hal_info);
	if (hal_status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("disable cpu fail!\n");
		return hal_status;
	}

	hal_status = rtw_hal_mac_enable_cpu(hal_info, 0, true);
	if (hal_status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("enable cpu fail!\n");
		return hal_status;
	}

	if (fw_info->dlram_en) {
		hal_status = rtw_hal_mac_fwdl(hal_info, fw_info->ram_buff,
									  fw_info->ram_size);
	}

	_hal_send_fwdl_hub_msg(phl_com, (!hal_status) ? true : false);

	FUNCOUT_WSTS(hal_status);

	return hal_status;
}

enum rtw_hal_status
rtw_hal_redownload_fw(struct rtw_phl_com_t *phl_com, void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct rtw_fw_info_t *fw_info = &phl_com->fw_info;

	FUNCIN_WSTS(hal_status);

	/* Disable/Enable CPU is necessary first when FWDL from files */
	if(fw_info->dlram_en && fw_info->fw_src == RTW_FW_SRC_EXTNAL) {

		hal_status = rtw_hal_mac_disable_cpu(hal_info);
		if (hal_status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("disable cpu fail!\n");
			return hal_status;
		}

		hal_status = rtw_hal_mac_enable_cpu(hal_info, 0, true);
		if (hal_status != RTW_HAL_STATUS_SUCCESS) {
			PHL_ERR("enable cpu fail!\n");
			return hal_status;
		}
	}

	if (fw_info->dlram_en) {
		if(fw_info->fw_src == RTW_FW_SRC_EXTNAL) {
			hal_status = rtw_hal_mac_fwdl(hal_info, fw_info->ram_buff,
						 				  fw_info->ram_size);
		} else {
			hal_status = rtw_hal_mac_enable_fw(hal_info, fw_info->fw_type);
		}
	}

	rtw_phl_pkt_ofld_reset_all_entry(phl_com);
	rtw_hal_rf_config_radio_to_fw(hal_info);

	_hal_send_fwdl_hub_msg(phl_com, (!hal_status) ? true : false);

	FUNCOUT_WSTS(hal_status);

	return hal_status;
}

void rtw_hal_fw_dbg_dump(void *hal, u8 is_low_power)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	rtw_hal_mac_fw_dbg_dump(hal_info, is_low_power);
}

enum rtw_fw_status rtw_hal_get_fw_status(void *h)
{
	struct hal_info_t *hal = (struct hal_info_t *)h;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	u32 mac_fw_sts;

	mac_fw_sts = hal_mac_ops->get_fw_status(mac);

	switch (mac_fw_sts) {
	case MACSUCCESS:
		return RTW_FW_STATUS_OK;
	case MACNOFW:
		return RTW_FW_STATUS_NOFW;
	case MACFWASSERT:
		return RTW_FW_STATUS_ASSERT;
	case MACFWEXCEP:
		return RTW_FW_STATUS_EXCEP;
	case MACFWRXI300:
		return RTW_FW_STATUS_RXI300;
	case MACFWPCHANG:
		return RTW_FW_STATUS_HANG;
	default:
		return RTW_FW_STATUS_OK;
	}
}