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
#define _HAL_SER_C_
#include "hal_headers.h"

enum rtw_hal_status rtw_hal_ser_ctrl(void *hal, bool en)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	hstatus = rtw_hal_mac_ser_ctrl(hal_info, en);

	return hstatus;
}

u32
rtw_hal_ser_get_error_status(void *hal, u32 *err)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum RTW_PHL_SER_NOTIFY_EVENT notify = RTW_PHL_SER_L2_RESET;

	rtw_hal_mac_ser_get_error_status(hal_info, err);

	if ((*err == MAC_AX_ERR_L1_ERR_DMAC) || (*err == MAC_AX_ERR_L0_PROMOTE_TO_L1)) {
		notify = RTW_PHL_SER_PAUSE_TRX;
	} else if (*err == MAC_AX_ERR_L1_RESET_DISABLE_DMAC_DONE) {
		notify = RTW_PHL_SER_DO_RECOVERY;
	} else if (*err == MAC_AX_ERR_L1_RESET_RECOVERY_DONE) {
		notify = RTW_PHL_SER_READY;
	} else if (*err < MAC_AX_ERR_L0_PROMOTE_TO_L1) {
		notify = RTW_PHL_SER_L0_RESET;
	} else if (*err == MAC_AX_DUMP_SHAREBUFF_INDICATOR) {
		notify = RTW_PHL_SER_DUMP_FW_LOG;
	} else if (*err == MAC_AX_ERR_L2_ERR_APB_SA_TO_HCI_WCMAC) {
		notify = RTW_PHL_SER_LOG_ONLY;
	} else if ((*err == MAC_AX_ERR_L1_PROMOTE_TO_L2) ||
			  ((*err >= MAC_AX_ERR_L2_ERR_AH_DMA) && (*err <= MAC_AX_GET_ERR_MAX))) {
		notify = RTW_PHL_SER_L2_RESET;
	}

	return notify;
}

enum rtw_hal_status rtw_hal_ser_set_error_status(void *hal, u32 err)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_ser_set_error_status(hal_info, err);
}

enum rtw_hal_status rtw_hal_trigger_cmac_err(void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_trigger_cmac_err(hal_info);
}

enum rtw_hal_status rtw_hal_trigger_dmac_err(void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return rtw_hal_mac_trigger_dmac_err(hal_info);
}

enum rtw_hal_status rtw_hal_lv1_rcvy(void *hal, u32 step)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;

	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "===> rtw_hal_lv1_rcvy step %d\n", step);
	status = rtw_hal_mac_lv1_rcvy(hal_info, step);
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "<=== rtw_hal_lv1_rcvy step %d, status 0x%x\n", step, status);

	return status;
}

void rtw_hal_dump_fw_rsvd_ple(void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	u32 mac_err;
	mac_err = rtw_hal_mac_dump_fw_rsvd_ple(hal_info);
}

void
rtw_hal_ser_reset_wdt_intr(void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	u32 mac_err;
	mac_err = rtw_hal_mac_ser_reset_wdt_intr(hal_info);
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "rtw_hal_ser_reset_wdt_intr status 0x%x\n",mac_err);
}
