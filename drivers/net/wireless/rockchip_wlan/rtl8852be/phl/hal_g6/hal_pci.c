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
#define _HAL_PCI_C_
#include "hal_headers.h"

#ifdef CONFIG_PCI_HCI
static u8 pci_read8(struct rtw_hal_com_t *hal, u32 addr)
{
	return _os_read8_pcie(hal->drv_priv, addr);
}
static u16 pci_read16(struct rtw_hal_com_t *hal, u32 addr)
{
	return _os_read16_pcie(hal->drv_priv, addr);
}
static u32 pci_read32(struct rtw_hal_com_t *hal, u32 addr)
{
	return _os_read32_pcie(hal->drv_priv, addr);
}

static int pci_write8(struct rtw_hal_com_t *hal, u32 addr, u8 val)
{
	return _os_write8_pcie(hal->drv_priv, addr, val);
}
static int pci_write16(struct rtw_hal_com_t *hal, u32 addr, u16 val)
{
	return _os_write16_pcie(hal->drv_priv, addr, val);
}
static int pci_write32(struct rtw_hal_com_t *hal, u32 addr, u32 val)
{
	return _os_write32_pcie(hal->drv_priv, addr, val);
}
#ifdef RTW_WKARD_BUS_WRITE
static int pci_write_post_cfg(struct rtw_hal_com_t *hal, u32 addr, u32 val)
{
	struct hal_info_t	*hal_info = hal->hal_priv;
	struct hal_ops_t	*hal_ops = hal_get_ops(hal_info);

	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	if(NULL != hal_ops->write_reg_post_cfg) {
		hal_status = hal_ops->write_reg_post_cfg(hal_info, addr, val);
	}

	return hal_status;
}
#endif
void hal_pci_set_io_ops(struct rtw_hal_com_t *hal, struct hal_io_ops *pops)
{
	/*_rtw_memset((u8 *)pops, 0, sizeof(struct hal_io_ops));*/
	_os_mem_set(hal->drv_priv, (u8 *)pops, 0, sizeof(struct hal_io_ops));

	pops->_read8 = &pci_read8;
	pops->_read16 = &pci_read16;
	pops->_read32 = &pci_read32;

	pops->_write8 = &pci_write8;
	pops->_write16 = &pci_write16;
	pops->_write32 = &pci_write32;
	#ifdef RTW_WKARD_BUS_WRITE
	pops->_write_post_cfg = &pci_write_post_cfg;
	#endif
}

enum rtw_hal_status rtw_hal_set_l2_leave(void *hal)
{
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	/* Only AON registers are accessed */
	status = hal_mac_set_l2_leave(hal_info);

	if (status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("%s : fail\n", __func__);
	}
	return status;
}
void rtw_hal_clear_bdidx(void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	FUNCIN();

	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_mac_clr_bdidx(hal_info))
		PHL_ERR("%s failure \n", __func__);
}

void rtw_hal_rst_bdram(void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	FUNCIN();

	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_mac_rst_bdram(hal_info))
		PHL_ERR("%s failure \n", __func__);

}
void rtw_hal_cfg_dma_io(void *hal, u8 en)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	PHL_TRACE(COMP_PHL_DBG, _PHL_DEBUG_, "%s : enable %u.\n", __func__, en);

	if (RTW_HAL_STATUS_SUCCESS != rtw_hal_mac_cfg_dma_io(hal_info, en))
		PHL_ERR("%s failure \n", __func__);
}

#ifdef RTW_WKARD_DYNAMIC_LTR
enum rtw_hal_status
rtw_hal_ltr_en_hw_mode(void *hal, bool hw_mode)
{
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	status = rtw_hal_mac_ltr_set_pcie(hal_info,
			hw_mode ? RTW_PCIE_BUS_FUNC_ENABLE : RTW_PCIE_BUS_FUNC_DISABLE,
			0, 0, 0, 0);

	if (status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("%s : fail\n", __func__);
		return status;
	}

	return status;
}

enum rtw_hal_status
rtw_hal_ltr_sw_trigger(void *hal, enum rtw_pcie_ltr_state state)
{
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	status = rtw_hal_mac_ltr_sw_trigger(hal_info, state);

	if (status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("%s : fail\n", __func__);

	return status;
}

void rtw_hal_ltr_update_stats(void *hal, bool clear)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_trx_stat *trx_stat = &hal_info->hal_com->trx_stat;
	static u32 dly_start_time = 0;

	if(clear) {
		dly_start_time = 0;
	} else {
		/* first in */
		if (dly_start_time == 0) {
			dly_start_time = _os_get_cur_time_us();
			trx_stat->ltr_last_tx_dly_time = 0;
		}
		trx_stat->ltr_tx_dly_count++;
		trx_stat->ltr_last_tx_dly_time =
				phl_get_passing_time_us(dly_start_time);
	}
}

bool rtw_hal_ltr_is_sw_ctrl(struct rtw_phl_com_t *phl_com, void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct bus_cap_t *bus_cap = &hal_info->hal_com->bus_cap;

	return bus_cap->ltr_sw_ctrl ?
		(phl_com->bus_sw_cap.ltr_sw_ctrl ? true : false) : false;
}

bool rtw_hal_ltr_is_hw_ctrl(struct rtw_phl_com_t *phl_com, void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct bus_cap_t *bus_cap = &hal_info->hal_com->bus_cap;

	return bus_cap->ltr_hw_ctrl ?
		(phl_com->bus_sw_cap.ltr_hw_ctrl ? true : false) : false;
}

#endif
#endif /*CONFIG_PCI_HCI*/
