/******************************************************************************
 *
 * Copyright(c) 2019 - 2021 Realtek Corporation.
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
#define _HAL_API_MAC_C_
#include "mac/mac_ax.h"
#include "hal_headers.h"
#ifdef RTW_PHL_BCN
#include "mac/mac_ax/fwcmd.h"
#endif

#define RTL8852A_FPGA_VERIFICATION 1

u16 hal_mac_get_macid_num(struct hal_info_t *hal)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);

	return mac->hw_info->macid_num;
}
void hal_mac_get_hwinfo(struct hal_info_t *hal, struct hal_spec_t *hal_spec)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_hw_info *mac_info = mac->hw_info;
	struct mac_ax_ops *ops = mac->ops;
	/*struct mac_ax_hw_info* (*get_hw_info)(struct mac_ax_adapter *adapter);*/

	mac_info = ops->get_hw_info(mac);

	hal_spec->macid_num = mac_info->macid_num;

	hal->hal_com->cv = mac_info->cv;
	PHL_INFO("[MAC-INFO]- CV : %d\n", mac_info->cv);
	PHL_INFO("[MAC-INFO]- tx_ch_num: %d\n", mac_info->tx_ch_num);

	PHL_INFO("[MAC-INFO]- tx_data_ch_num: %d\n", mac_info->tx_data_ch_num);
	PHL_INFO("[MAC-INFO]- wd_body_len: %d\n", mac_info->wd_body_len);
	PHL_INFO("[MAC-INFO]- wd_info_len: %d\n", mac_info->wd_info_len);

	PHL_INFO("[MAC-INFO]- pwr_seq_ver: %d\n", mac_info->pwr_seq_ver);
	PHL_INFO("[MAC-INFO]- fifo_size: %d\n", mac_info->fifo_size);
	PHL_INFO("[MAC-INFO]- macid_num: %d\n", mac_info->macid_num);
	PHL_INFO("[MAC-INFO]- wl_efuse_size: %d\n", mac_info->wl_efuse_size);

	PHL_INFO("[MAC-INFO]- efuse_size: %d\n", mac_info->efuse_size);
	PHL_INFO("[MAC-INFO]- log_efuse_size: %d\n", mac_info->log_efuse_size);

	PHL_INFO("[MAC-INFO]- bt_efuse_size: %d\n", mac_info->bt_efuse_size);
	PHL_INFO("[MAC-INFO]- sec_ctrl_efuse_size: %d\n", mac_info->sec_ctrl_efuse_size);
	PHL_INFO("[MAC-INFO]- sec_data_efuse_size: %d\n", mac_info->sec_data_efuse_size);

}

enum rtw_hal_status rtw_hal_mac_watchdog(struct hal_info_t *hal_info)
{
#ifdef CONFIG_HAL_MAC_WATCHDOG
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 ret = 0;

	ret = mac->ops->watchdog(mac);

	return (ret == MACSUCCESS) ? (RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
#else
	return RTW_HAL_STATUS_SUCCESS;
#endif
}

#ifdef CONFIG_PCI_HCI
enum rtw_hal_status rtw_hal_mac_set_pcicfg(struct hal_info_t *hal_info,
					struct mac_ax_pcie_cfgspc_param *pci_cfgspc)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 ret = 0;

	ret = mac->ops->set_hw_value(mac, MAC_AX_HW_PCIE_CFGSPC_SET, pci_cfgspc);
	return (ret == 0) ? (RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
}

enum rtw_hal_status rtw_hal_mac_ltr_sw_trigger(struct hal_info_t *hal_info, enum rtw_pcie_ltr_state state)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	enum mac_ax_pcie_ltr_sw_ctrl ctrl;
	enum rtw_hal_status hstats = RTW_HAL_STATUS_FAILURE;
	u32 ret = 0;

	switch (state) {

	case RTW_PCIE_LTR_SW_ACT:
		ctrl = MAC_AX_PCIE_LTR_SW_ACT;
		break;
	case RTW_PCIE_LTR_SW_IDLE:
		ctrl = MAC_AX_PCIE_LTR_SW_IDLE;
		break;
	default:
		PHL_INFO("%s: state = %u, fail to trigger \n", __func__, state);
		return RTW_HAL_STATUS_FAILURE;

	}

	ret = mac->ops->set_hw_value(mac, MAX_AX_HW_PCIE_LTR_SW_TRIGGER, &ctrl);

	if (ret == 0)
		hstats = RTW_HAL_STATUS_SUCCESS;
 	else
		hstats = RTW_HAL_STATUS_FAILURE;

	return hstats;
}

enum rtw_hal_status rtw_hal_mac_ltr_set_pcie(struct hal_info_t *hal_info,
	enum rtw_pcie_bus_func_cap_t hw_ctrl,
	u8 idle_ctrl, u32 idle_val, u8 act_ctrl, u32 act_val)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_pcie_ltr_param param;
	u32 ret = 0;
	enum mac_ax_pcie_func_ctrl ctrl;

	switch (hw_ctrl) {

	case RTW_PCIE_BUS_FUNC_ENABLE:
		ctrl = MAC_AX_PCIE_ENABLE;
		break;
	case RTW_PCIE_BUS_FUNC_DISABLE:
		ctrl = MAC_AX_PCIE_DISABLE;
		break;
	case RTW_PCIE_BUS_FUNC_DEFAULT:
		ctrl = MAC_AX_PCIE_DEFAULT;
		break;
	default:
		ctrl = MAC_AX_PCIE_IGNORE;
	}

	param.write = 1;
	param.read = 0;
	param.ltr_ctrl = MAC_AX_PCIE_IGNORE;
	param.ltr_hw_ctrl = ctrl;
	param.ltr_spc_ctrl = MAC_AX_PCIE_LTR_SPC_IGNORE;
	param.ltr_idle_timer_ctrl = MAC_AX_PCIE_LTR_IDLE_TIMER_IGNORE;
	param.ltr_rx0_th_ctrl.ctrl = MAC_AX_PCIE_IGNORE;
	param.ltr_rx0_th_ctrl.val = 0;
	param.ltr_rx1_th_ctrl.ctrl = MAC_AX_PCIE_IGNORE;
	param.ltr_rx1_th_ctrl.val = 0;

	/* when ctrl is not equal to 0, setting ignore to make hw control the value */
	if (idle_ctrl) {
		param.ltr_idle_lat_ctrl.ctrl = MAC_AX_PCIE_ENABLE;
		param.ltr_idle_lat_ctrl.val = idle_val;
	} else {
		param.ltr_idle_lat_ctrl.ctrl = MAC_AX_PCIE_IGNORE;
		param.ltr_idle_lat_ctrl.val = 0;
	}

	if (act_ctrl) {
		param.ltr_act_lat_ctrl.ctrl = MAC_AX_PCIE_ENABLE;
		param.ltr_act_lat_ctrl.val = act_val;
	} else {
		param.ltr_act_lat_ctrl.ctrl = MAC_AX_PCIE_IGNORE;
		param.ltr_act_lat_ctrl.val = 0;
	}

	PHL_INFO("%s: ltr_hw_ctrl %u \n", __func__, param.ltr_hw_ctrl);
	PHL_INFO("%s: ltr_idle_lat_ctrl, ctrl %u, val %u\n", __func__,
		param.ltr_idle_lat_ctrl.ctrl, param.ltr_idle_lat_ctrl.val);
	PHL_INFO("%s: ltr_act_lat_ctrl, ctrl %u, val %u\n", __func__,
		param.ltr_act_lat_ctrl.ctrl, param.ltr_act_lat_ctrl.val);


	ret = mac->ops->intf_ops->ltr_set_pcie(mac, &param);
	return (ret == 0) ? (RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
}

enum rtw_hal_status hal_mac_set_l2_leave(struct hal_info_t *hal_info)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 ret = 0;
	u8 set = true;

	ret = mac->ops->set_hw_value(mac, MAX_AX_HW_PCIE_L2_LEAVE, &set);
	return (ret == MACSUCCESS) ? (RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
}

enum rtw_hal_status rtw_hal_mac_poll_txdma_idle(struct hal_info_t *hal,
					struct mac_ax_txdma_ch_map *ch_map)
{

	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	u32 ret = 0;

	ret = hal_mac_ops->intf_ops->poll_txdma_ch_idle(mac, ch_map);

	return (ret == MACSUCCESS) ?
			(RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);

}

enum rtw_hal_status rtw_hal_mac_poll_rxdma_idle(struct hal_info_t *hal,
					struct mac_ax_rxdma_ch_map *ch_map)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	u32 ret = 0;

	ret = hal_mac_ops->intf_ops->poll_rxdma_ch_idle(mac, ch_map);

	return (ret == MACSUCCESS) ?
			(RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
}
enum rtw_hal_status rtw_hal_mac_cfg_txdma(struct hal_info_t *hal,
					struct mac_ax_txdma_ch_map *ch_map)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	u32 ret = 0;

	ret = hal_mac_ops->intf_ops->ctrl_txdma_ch(mac, ch_map);

	return (ret == MACSUCCESS) ?
			(RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);

}


enum rtw_hal_status rtw_hal_mac_clr_bdidx(struct hal_info_t *hal)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	u32 ret = 0;

	ret = hal_mac_ops->intf_ops->clr_idx_all(mac);

	return (ret == MACSUCCESS) ?
			(RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
}

enum rtw_hal_status rtw_hal_mac_rst_bdram(struct hal_info_t *hal)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	u32 ret = 0;
	u8 val;

	ret = hal_mac_ops->set_hw_value(mac, MAC_AX_HW_PCIE_RST_BDRAM, &val);

	return (ret == MACSUCCESS) ?
			(RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
}

enum rtw_hal_status rtw_hal_mac_cfg_dma_io(struct hal_info_t *hal, u8 en)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	enum mac_ax_func_sw ctrl;
	u32 ret = 0;

	if (en)
		ctrl = MAC_AX_FUNC_EN;
	else
		ctrl = MAC_AX_FUNC_DIS;

	ret = hal_mac_ops->intf_ops->ctrl_dma_io(mac, ctrl);

	return (ret == MACSUCCESS) ?
			(RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
}


#endif

#ifdef CONFIG_USB_HCI
u8 hal_mac_get_bulkout_id(struct hal_info_t *hal, u8 dma_ch, u8 mode)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);

	return mac->ops->intf_ops->get_bulkout_id(mac, dma_ch, mode);
}

u32 hal_mac_usb_tx_agg_cfg(struct hal_info_t *hal, u8* wd_buf, u8 agg_num)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_usb_tx_agg_cfg agg;

	agg.pkt = wd_buf;
	agg.agg_num = agg_num;

	return mac->ops->intf_ops->usb_tx_agg_cfg(mac, &agg);
}

u32 hal_mac_usb_rx_agg_cfg(struct hal_info_t *hal, u8 agg_mode,
	u8 drv_define, u8 timeout, u8 size, u8 pkt_num)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_rx_agg_cfg cfg;

	cfg.mode = agg_mode;
	cfg.thold.drv_define = drv_define;
	cfg.thold.timeout = timeout;
	cfg.thold.size = size;
	cfg.thold.pkt_num = pkt_num;

	return mac->ops->intf_ops->usb_rx_agg_cfg(mac, &cfg);
}

enum rtw_hal_status hal_mac_force_usb_switch(struct hal_info_t *hal)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);

	if(mac->ops->intf_ops->u2u3_switch(mac) == MACSUCCESS) {
		PHL_INFO("%s,success!\n", __func__);
		return RTW_HAL_STATUS_SUCCESS;
	}
	else {
		PHL_INFO("%s,fail!\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}
}

u32 hal_mac_get_cur_usb_mode(struct hal_info_t *hal)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	return mac->ops->intf_ops->get_usb_mode(mac);
}
u32 hal_mac_get_usb_support_ability(struct hal_info_t *hal)

{

	struct mac_ax_adapter *mac = hal_to_mac(hal);

	return mac->ops->intf_ops->get_usb_support_ability(mac);

}
u8 hal_mac_usb_get_max_bulkout_wd_num(struct hal_info_t *hal)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);

	return mac->usb_info.max_bulkout_wd_num;
}
#endif

#ifdef CONFIG_SDIO_HCI
u8 hal_mac_sdio_read8(struct rtw_hal_com_t *hal, u32 addr)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	return mac_intf_ops->reg_read8(mac, addr);
}

u16 hal_mac_sdio_read16(struct rtw_hal_com_t *hal, u32 addr)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	return mac_intf_ops->reg_read16(mac, addr);
}

u32 hal_mac_sdio_read32(struct rtw_hal_com_t *hal, u32 addr)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	return mac_intf_ops->reg_read32(mac, addr);
}

int hal_mac_sdio_write8(struct rtw_hal_com_t *hal, u32 addr, u8 value)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	mac_intf_ops->reg_write8(mac, addr, value);
	return 0;
}

int hal_mac_sdio_write16(struct rtw_hal_com_t *hal, u32 addr, u16 value)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	mac_intf_ops->reg_write16(mac, addr, value);
	return 0;
}

int hal_mac_sdio_write32(struct rtw_hal_com_t *hal, u32 addr, u32 value)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	mac_intf_ops->reg_write32(mac, addr, value);
	return 0;
}

static void _read_register(struct rtw_hal_com_t *hal, u32 addr, u32 cnt, u8 *buf)
{
	u32 i, n;
	u16 val16;
	u32 val32;

	i = addr & 0x3;
	/* Handle address not start from 4 bytes alignment case */
	if (i) {
		val32 = cpu_to_le32(hal_read32(hal, addr & ~0x3));
		n = 4 - i;
		_os_mem_cpy(hal->drv_priv, buf, ((u8 *)&val32) + i, n);
		i = n;
		cnt -= n;
	}

	while (cnt) {
		if (cnt >= 4)
			n = 4;
		else if (cnt >= 2)
			n = 2;
		else
			n = 1;
		cnt -= n;

		switch (n) {
		case 1:
			buf[i] = hal_read8(hal, addr+i);
			i++;
			break;
		case 2:
			val16 = cpu_to_le16(hal_read16(hal, addr+i));
			_os_mem_cpy(hal->drv_priv, &buf[i], &val16, 2);
			i += 2;
			break;
		case 4:
			val32 = cpu_to_le32(hal_read32(hal, addr+i));
			_os_mem_cpy(hal->drv_priv, &buf[i], &val32, 4);
			i += 4;
			break;
		}
	}
}

static int _sdio_read_local(struct rtw_hal_com_t *hal, u32 addr, u32 cnt, u8 *buf)
{
	/*struct hal_info_t *hal_info = hal->hal_priv;*/
	/*struct mac_ax_adapter *mac = hal_to_mac(hal_info);*/
	/*struct mac_ax_ops *mac_api = mac->ops;*/
	/*struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;*/

	if (buf == NULL)
		return -1;
	/*GEORGIA_TODO_FIXIT_ASKFORMAC*/
	/*mac_intf_ops->reg_readrn(mac, addr, cnt, buf);*/
	return 0;
}

void hal_mac_sdio_read_mem(struct rtw_hal_com_t *hal, u32 addr, u32 cnt, u8 *pmem)
{

	if (pmem == NULL) {
		PHL_ERR("pmem is NULL\n");
		return;
	}

	if (addr & 0xFFFF0000) {
		int err = 0;

		err = _sdio_read_local(hal, addr, cnt, pmem);
		if (!err)
			return;
	}
	_read_register(hal, addr, cnt, pmem);
}

#ifdef CONFIG_SDIO_INDIRECT_ACCESS
u8 hal_mac_sdio_iread8(struct rtw_hal_com_t *hal, u32 addr)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	return mac_intf_ops->reg_read8(mac, addr);
}

u16 hal_mac_sdio_iread16(struct rtw_hal_com_t *hal, u32 addr)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	return mac_intf_ops->reg_read16(mac, addr);
}

u32 hal_mac_sdio_iread32(struct rtw_hal_com_t *hal, u32 addr)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *mac_api = mac->ops;
	struct mac_ax_intf_ops *mac_intf_ops = mac_api->intf_ops;

	return 0;
}
#endif /* CONFIG_SDIO_INDIRECT_ACCESS */
#endif /* CONFIG_SDIO_HCI */


#ifndef CONFIG_NEW_HALMAC_INTERFACE
#ifdef CONFIG_SDIO_HCI
static u8 hal_mac_sdio_cmd52_r8(void *h, u32 addr)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	return _os_sdio_cmd52_r8(hal->drv_priv, addr);
}
static u8 hal_mac_sdio_cmd53_r8(void *h, u32 addr)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	return _os_sdio_cmd53_r8(hal->drv_priv, addr);
}
static u16 hal_mac_sdio_cmd53_r16(void *h, u32 addr)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	return _os_sdio_cmd53_r16(hal->drv_priv, addr);
}
static u32 hal_mac_sdio_cmd53_r32(void *h, u32 addr)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	return _os_sdio_cmd53_r32(hal->drv_priv, addr);
}
static u8 hal_mac_sdio_cmd53_rn(void *h, u32 addr, u32 size, u8 *val)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	return _os_sdio_cmd53_rn(hal->drv_priv, addr, size,val);
}
static void hal_mac_sdio_cmd52_w8(void *h, u32 addr, u8 val)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_sdio_cmd52_w8(hal->drv_priv, addr, val);
}
static void hal_mac_sdio_cmd53_w8(void *h, u32 addr, u8 val)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_sdio_cmd53_w8(hal->drv_priv, addr, val);
}
static void hal_mac_sdio_cmd53_w16(void *h, u32 addr, u16 val)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_sdio_cmd53_w16(hal->drv_priv, addr, val);
}
static void hal_mac_sdio_cmd53_w32(void *h, u32 addr, u32 val)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_sdio_cmd53_w32(hal->drv_priv, addr, val);
}
static u8 hal_mac_sdio_cmd53_wn(void *h, u32 addr, u32 size, u8 *val)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_sdio_cmd53_wn(hal->drv_priv, addr, size, val);
	return true;
}
static u8 hal_mac_sdio_cmd52_cia_r8(void *h, u32 addr)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	return _os_sdio_read_cia_r8(hal->drv_priv, addr);
}
#endif /*CONFIG_SDIO_HCI*/

static u8 hal_mac_reg_r8(void *h, u32 addr)
{
	return hal_read8((struct rtw_hal_com_t *)h, addr);
}
static u16 hal_mac_reg_r16(void *h, u32 addr)
{
	return hal_read16((struct rtw_hal_com_t *)h, addr);
}
static u32 hal_mac_reg_r32(void *h, u32 addr)
{
	return hal_read32((struct rtw_hal_com_t *)h, addr);
}
static void hal_mac_reg_w8(void *h, u32 addr, u8 val)
{
	hal_write8((struct rtw_hal_com_t *)h, addr, val);
}
static void hal_mac_reg_w16(void *h, u32 addr, u16 val)
{
	hal_write16((struct rtw_hal_com_t *)h, addr, val);
}
static void hal_mac_reg_w32(void *h, u32 addr, u32 val)
{
	hal_write32((struct rtw_hal_com_t *)h, addr, val);
}


#ifdef DBG_HAL_MAC_MEM_MOINTOR
static void hal_mac_mem_free(void *h, void *buf, u32 size)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	/*PHL_INFO("MAC free mem - :%d\n", size);*/
	_os_atomic_sub(hal->drv_priv, &(hal->hal_mac_mem), size);
	_os_mem_free(hal->drv_priv, buf, size);
}
static void* hal_mac_mem_alloc(void *h, u32 size)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	/*PHL_INFO("MAC Alloc mem - :%d\n", size);*/
	_os_atomic_add_return(hal->drv_priv, &(hal->hal_mac_mem), size);

	return _os_mem_alloc(hal->drv_priv, size);
}
#else
static void hal_mac_mem_free(void *h, void *buf, u32 size)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_mem_free(hal->drv_priv, buf, size);
}
static void* hal_mac_mem_alloc(void *h, u32 size)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	return _os_mem_alloc(hal->drv_priv, size);
}
#endif /*DBG_HAL_MAC_MEM_MOINTOR*/
static void hal_mac_mem_cpy(void *h, void *dest, void *src, u32 size)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_mem_cpy(hal->drv_priv, dest, src, size);
}
static void hal_mac_mem_set(void *h, void *addr, u8 val, u32 size)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_mem_set(hal->drv_priv, addr, val, size);

}
static s32 hal_mac_mem_cmp(void *h, void *ptr1, void *ptr2, u32 num)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	return _os_mem_cmp(hal->drv_priv, ptr1, ptr2, num);
}
static void hal_mac_udelay(void *h, u32 us)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_delay_us(hal->drv_priv, us);
}
static void hal_mac_mdelay(void *h, u32 ms)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_delay_ms(hal->drv_priv, ms);
}

static void hal_mac_mutex_init(void *h, mac_ax_mutex *mutex)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_mutex_init(hal->drv_priv, mutex);
}
static void hal_mac_mutex_deinit(void *h, mac_ax_mutex *mutex)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_mutex_deinit(hal->drv_priv, mutex);
}
static void hal_mac_mutex_lock(void *h, mac_ax_mutex *mutex)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_mutex_lock(hal->drv_priv, mutex);
}
static void hal_mac_mutex_unlock(void *h, mac_ax_mutex *mutex)
{
	struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

	_os_mutex_unlock(hal->drv_priv, mutex);
}

static void hal_mac_event_notify(void *h,
			enum mac_ax_feature mac_ft,
			enum mac_ax_status stat, u8 *buf, u32 size)
{
	//struct rtw_hal_com_t *hal = (struct rtw_hal_com_t *)h;

}

struct rtw_h2c_pkt *hal_query_h2c_pkt(struct rtw_phl_com_t *phl_com,
									  struct rtw_hal_com_t *hal_com,
									  enum h2c_buf_class type)
{
	struct rtw_h2c_pkt *h2c_pkt = NULL;
	enum rtw_h2c_pkt_type pkt_type = H2CB_TYPE_MAX;

	switch (type) {
	case H2CB_CLASS_CMD:
		pkt_type = H2CB_TYPE_CMD;
		break;
	case H2CB_CLASS_DATA:
		pkt_type = H2CB_TYPE_DATA;
		break;
	case H2CB_CLASS_LONG_DATA:
		pkt_type = H2CB_TYPE_LONG_DATA;
		break;
	case H2CB_CLASS_LAST:
		pkt_type = H2CB_TYPE_MAX;
		break;
	default:
		PHL_ERR("%s: Unknown type(%d)\n", __func__, type);
		break;
	}

	h2c_pkt = rtw_phl_query_h2c_pkt(phl_com, pkt_type);

	if (!h2c_pkt)
		PHL_ERR("hal_query_h2c_pkt fail (type %d).\n", type);

	return h2c_pkt;
}

enum rtw_hal_status hal_pltfm_tx(struct rtw_phl_com_t *phl_com,
								 struct rtw_hal_com_t *hal_com,
								 struct rtw_h2c_pkt *pkt)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;

	if (rtw_phl_pltfm_tx(phl_com, pkt) == RTW_PHL_STATUS_SUCCESS)
		hstatus = RTW_HAL_STATUS_SUCCESS;
	else
		PHL_ERR("hal_pltfm_tx fail.\n");

	return hstatus;
}

void hal_ser_l2_notify(void *phl, void *hal)
{
	struct rtw_phl_com_t *phl_com = (struct rtw_phl_com_t *)phl;

	rtw_phl_ser_l2_notify(phl_com);
}

static s32 hal_mac_sprintf(void *drv_adapter, char *buf, size_t size, const char *fmt, ...)
{
#if defined(PHL_PLATFORM_LINUX) || defined(PHL_PLATFORM_WINDOWS)
	s32 ret;
	va_list args;

	va_start(args, fmt);
	ret = _os_vsnprintf(buf, size, fmt, args);
	if (ret >= 0)
		ret = _os_strlen((u8 *)buf);

	/* PHL_PRINT("[HAL_MMAC]%s", buf); */
	va_end(args);
	return ret;
#else
	return 0;
#endif
}

static s32 hal_mac_strcmp(void *drv_adapter, const char *s1, const char *s2)
{
	return _os_strcmp(s1, s2);
}

static char* hal_mac_strsep(void *drv_adapter, char **s, const char *ct)
{
	return _os_strsep(s, ct);
}

static u32 hal_mac_strlen(void *drv_adapter, char *buf)
{
	return _os_strlen((u8 *)buf);
}

static char* hal_mac_strcpy(void *drv_adapter, char *dest, const char *src)
{
	return _os_strcpy(dest, src);
}

static char* hal_mac_strpbrk(void *drv_adapter, const char *cs, const char *ct)
{
	return _os_strpbrk(cs, ct);
}

static u32 hal_mac_strtoul(void *drv_adapter, const char *buf, u32 base)
{
#if 1
	u32 ret = 0, val = 0;

	if (base == 10)
		ret = _os_sscanf(buf, "%d", &val);
	else if (base == 16)
		ret = _os_sscanf(buf, "%x", &val);
	else
		ret = _os_sscanf(buf, "%d", &val);

	return val;
#else
	//stdlib strtoul
#endif

}

static u8
hal_mac_ld_fw_symbol(void *phl_com, void *hal_com, const char *name,
	u8 **buf, u32 *buf_size)
{
	return rtw_hal_ld_fw_symbol((struct rtw_phl_com_t *)phl_com,
		(struct rtw_hal_com_t *)hal_com, name, buf, buf_size);
}

#define RTW_HALMAC_FAIL		0
#define RTW_HALMAC_SUCCESS		1
#define MSG_PREFIX			"[MAC]"

void hal_mac_msg_print(void *p, u8 dbg_level, s8 *fmt, ...)
{
#if defined(PHL_PLATFORM_LINUX) || defined(PHL_PLATFORM_WINDOWS)
#define MSG_LEN		100
		va_list args;
		char str[MSG_LEN] = {0};
		int err;
		u8 ret = RTW_HALMAC_SUCCESS;

		str[0] = '\n';
		va_start(args, fmt);
		err = _os_vsnprintf(str, MSG_LEN, fmt, args);
		va_end(args);

		/* An output error is encountered */
		if (err < 0)
			return;
		/* Output may be truncated due to size limit */
		if ((err == (MSG_LEN - 1)) && (str[MSG_LEN - 2] != '\n'))
			ret = RTW_HALMAC_FAIL;

		PHL_TRACE(COMP_PHL_MAC, dbg_level, MSG_PREFIX " %s", str);
#endif
}

struct mac_ax_pltfm_cb rtw_plt_cb = {0};
void rtw_plt_cb_init(void)
{
	/* R/W register */
#ifdef CONFIG_SDIO_HCI
	rtw_plt_cb.sdio_cmd52_r8 = hal_mac_sdio_cmd52_r8;
	rtw_plt_cb.sdio_cmd53_r8 = hal_mac_sdio_cmd53_r8;
	rtw_plt_cb.sdio_cmd53_r16 = hal_mac_sdio_cmd53_r16;
	rtw_plt_cb.sdio_cmd53_r32 = hal_mac_sdio_cmd53_r32;
	rtw_plt_cb.sdio_cmd53_rn = hal_mac_sdio_cmd53_rn;
	rtw_plt_cb.sdio_cmd52_w8 = hal_mac_sdio_cmd52_w8;
	rtw_plt_cb.sdio_cmd53_w8 = hal_mac_sdio_cmd53_w8;
	rtw_plt_cb.sdio_cmd53_w16 = hal_mac_sdio_cmd53_w16;
	rtw_plt_cb.sdio_cmd53_w32 = hal_mac_sdio_cmd53_w32;
	rtw_plt_cb.sdio_cmd53_wn = hal_mac_sdio_cmd53_wn;
	rtw_plt_cb.sdio_cmd52_cia_r8 = hal_mac_sdio_cmd52_cia_r8;
#endif /* CONFIG_SDIO_HCI */

#if defined(CONFIG_USB_HCI) || defined(CONFIG_PCI_HCI)
	rtw_plt_cb.reg_r8 = hal_mac_reg_r8;
	rtw_plt_cb.reg_r16 = hal_mac_reg_r16;
	rtw_plt_cb.reg_r32 = hal_mac_reg_r32;
	rtw_plt_cb.reg_w8 = hal_mac_reg_w8;
	rtw_plt_cb.reg_w16 = hal_mac_reg_w16;
	rtw_plt_cb.reg_w32 = hal_mac_reg_w32;
#endif /* CONFIG_USB_HCI || CONFIG_PCI_HCI */

	/* Memory allocate */
	rtw_plt_cb.rtl_free = hal_mac_mem_free;
	rtw_plt_cb.rtl_malloc = hal_mac_mem_alloc;
	rtw_plt_cb.rtl_memcpy = hal_mac_mem_cpy;
	rtw_plt_cb.rtl_memset = hal_mac_mem_set;
	rtw_plt_cb.rtl_memcmp = hal_mac_mem_cmp;
	/* Sleep */
	rtw_plt_cb.rtl_delay_us = hal_mac_udelay;
	rtw_plt_cb.rtl_delay_ms = hal_mac_mdelay;

	/* Process Synchronization */
	rtw_plt_cb.rtl_mutex_init = hal_mac_mutex_init;
	rtw_plt_cb.rtl_mutex_deinit = hal_mac_mutex_deinit;
	rtw_plt_cb.rtl_mutex_lock = hal_mac_mutex_lock;
	rtw_plt_cb.rtl_mutex_unlock = hal_mac_mutex_unlock;

	rtw_plt_cb.msg_print = hal_mac_msg_print;
	rtw_plt_cb.event_notify = hal_mac_event_notify;
	rtw_plt_cb.ser_l2_notify = hal_ser_l2_notify;
	rtw_plt_cb.ld_fw_symbol = hal_mac_ld_fw_symbol;
	/*.tx = ;	*/
#if MAC_AX_PHL_H2C
	rtw_plt_cb.tx = hal_pltfm_tx;
	rtw_plt_cb.rtl_query_h2c = hal_query_h2c_pkt;
#endif
#if MAC_AX_FEATURE_DBGCMD
	rtw_plt_cb.rtl_sprintf = hal_mac_sprintf;
	rtw_plt_cb.rtl_strcmp = hal_mac_strcmp;
	rtw_plt_cb.rtl_strsep = hal_mac_strsep;
	rtw_plt_cb.rtl_strlen = hal_mac_strlen;
	rtw_plt_cb.rtl_strcpy = hal_mac_strcpy;
	rtw_plt_cb.rtl_strpbrk = hal_mac_strpbrk;
	rtw_plt_cb.rtl_strtoul = hal_mac_strtoul;
#endif
}


#endif /*CONFIG_NEW_HALMAC_INTERFACE*/

/* halmac wrapper API for hal and proto type is at hal_api_mac.h */
#define MAC_STATUS_MAX	MACSDIOSEQERR+1 /* Wrong interface */
const char *const ma_status[] = {
	"MAC Success",
	"Callback of platform is null",
	"Endian of platform error",
	"Invalid base address",
	"Leave suspend error",
	"Pointer is null",
	"Chip ID is undefined",
	"Can not get MAC adapter",
	"Unexpected structure alignment",
	"Buffer space is not enough",
	"Buffer size error",
	"Invalid item",
	"Polling timeout",
	"Power switch fail",
	"Work queue is busy",
	"Failed compare result",
	"Wrong interface",
	"Incorrect FW bin file",
	"Wrong FIFO configuration",
	"Same MACID",
	"MACID full",
	"There is no FW",
	"Process is busy",
	"state machine error",
	"switch efuse bank fail",
	"read efuse fail",
	"write efuse fail",
	"efuse size error",
	"eeprom parsing fail",
	"compare efuse fail",
	"secure on, no host indirect access",
	"invalid tx dma channel",
	"address cam update error",
	"Power state error",
	"SDIO Tx mix mode",
	"SDIO Tx sequence error",
};

#define mac_sstr(status) (((status) >= MAC_STATUS_MAX) ? "unknown" : ma_status[(status)])

/**
 * rtw_hal_mac_get_version() - Get HALMAC version
 * @ver_str:	string buffer for storing version string
 * @len:	string buffer length
 *
 * HALMAC version format is "V[major].[prototype].[sub ver].[sub index]",
 * ex: V0.16.1.0
 *
 */
void rtw_hal_mac_get_version(char *ver_str, u16 len)
{
	_os_snprintf(ver_str, len, "V%u.%u.%u.%u",
		     MAC_AX_MAJOR_VER, MAC_AX_PROTOTYPE_VER,
		     MAC_AX_SUB_VER, MAC_AX_SUB_INDEX);
}

/**
 * rtw_hal_mac_get_fw_ver() - Get Firmware version
 * @hal_info:	struct hal_info_t *
 * @ver_str:	string buffer for storing version string
 * @len:	string buffer length
 *
 * Firmware version format is "V[major].[ver].[sub ver].[sub index]",
 * ex: V0.5.0
 *
 */
void rtw_hal_mac_get_fw_ver(struct hal_info_t *hal_info, char *ver_str, u16 len)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	_os_snprintf(ver_str, len, "V%u.%u.%u.%u",
		     mac->fw_info.major_ver,mac->fw_info.minor_ver,
		     mac->fw_info.sub_ver, mac->fw_info.sub_idx);
}

#define _IS_FW_READY(hal_info) \
		(hal_to_mac(hal_info)->sm.fwdl == MAC_AX_FWDL_INIT_RDY)

void hal_mac_print_fw_version(struct hal_info_t *hal_info)
{
	char ver[20] = {0};
	rtw_hal_mac_get_fw_ver(hal_info, ver, 20);
	PHL_PRINT("%s: FW version %s, %sReady\n", __func__, ver,
		  _IS_FW_READY(hal_info) ? "" : "Not ");
}

u32 rtw_hal_mac_init(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal_info)
{
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct mac_ax_adapter *mac = NULL;
	struct mac_ax_ops *mac_ops;
	u32 status;

#ifdef DBG_HAL_MAC_MEM_MOINTOR
	_os_atomic_set(hal_com->drv_priv, &(hal_com->hal_mac_mem), 0);
#endif
	hal_info->mac = NULL;
#ifdef CONFIG_NEW_HALMAC_INTERFACE
	status = new_mac_ax_ops_init(phl_com, hal_com, &mac, &mac_ops);
#else
	{
		enum mac_ax_intf intf = MAC_AX_INTF_INVALID;
		if (phl_com->hci_type == RTW_HCI_PCIE)
			intf = MAC_AX_INTF_PCIE;
		else if (phl_com->hci_type ==  RTW_HCI_USB)
			intf = MAC_AX_INTF_USB;
		else if ((phl_com->hci_type ==  RTW_HCI_SDIO) ||
			(phl_com->hci_type ==  RTW_HCI_GSPI))
			intf = MAC_AX_INTF_SDIO;

		rtw_plt_cb_init();
		status = mac_ax_ops_init(hal_com,
				&rtw_plt_cb, intf, &mac, &mac_ops);
		#if MAC_AX_PHL_H2C
		if (status == MACSUCCESS && mac != NULL)
			mac_ax_phl_init(phl_com, mac);
		#endif
	}
#endif

	if (status != MACSUCCESS) {
		PHL_ERR("%s: halmac_init_adapter fail!(status=%d-%s)\n",
				__func__, status, mac_sstr(status));
		hal_status = RTW_HAL_STATUS_MAC_INIT_FAILURE;
		goto error_mac_init;
	}

	if (!mac) {
		PHL_ERR("halmac alloc failed\n");
		hal_status = RTW_HAL_STATUS_MAC_INIT_FAILURE;
		/*_os_warn_on(1);*/
		goto error_mac_init;
	}
	hal_com->cv = mac->hw_info->cv;
	hal_info->mac = mac;

	return hal_status;

error_mac_init:
	if (mac) {
		mac_ax_ops_exit(mac);
		hal_info->mac = NULL;
	}
	return hal_status;
}

u32 rtw_hal_mac_deinit(struct rtw_phl_com_t *phl_com,
						struct hal_info_t *hal_info)
{
	#ifdef DBG_HAL_MAC_MEM_MOINTOR
	struct rtw_hal_com_t *hal = hal_info->hal_com;
	#endif
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	if (mac) {
		if(mac_ax_ops_exit(mac) == MACSUCCESS) {
			hal_status = RTW_HAL_STATUS_FAILURE;

			hal_info->mac = NULL;
		} else {
			PHL_ERR("%s failed\n", __func__);
		}
	}
	#ifdef DBG_HAL_MAC_MEM_MOINTOR
	PHL_INFO("[PHL-MEM] %s HALMAC memory :%d\n", __func__,
		_os_atomic_read(hal_to_drvpriv(hal_info), &(hal->hal_mac_mem)));
	#endif

	return hal_status;
}

u8 _hal_mac_ax_dmach2datach(u8 dmach)
{
	u8 data_ch = MAC_AX_DATA_CH0;


	if ((MAC_AX_DMA_B0HI == dmach) ||
		(MAC_AX_DMA_B1HI == dmach))
		data_ch = MAC_AX_DATA_HIQ;
	else if ((MAC_AX_DMA_B0MG == dmach) ||
			(MAC_AX_DMA_B1MG == dmach))
		data_ch = 0;
	else {
		data_ch = dmach;
	}
	return data_ch;
}

#ifdef CONFIG_SDIO_HCI
void rtw_hal_mac_sdio_cfg(struct rtw_phl_com_t *phl_com,
			  struct hal_info_t *hal_info,
			  struct rtw_ic_info *ic_info)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_sdio_info info;

	_os_mem_set(hal_to_drvpriv(hal_info), &info, 0, sizeof(info));

	info.sdio_4byte = (ic_info->sdio_info.io_align_sz == 4)
		? MAC_AX_SDIO_4BYTE_MODE_RW : MAC_AX_SDIO_4BYTE_MODE_DISABLE;

	if (ic_info->sdio_info.sd3_bus_mode)
		info.spec_ver = MAC_AX_SDIO_SPEC_VER_3_00;
	else
		info.spec_ver = MAC_AX_SDIO_SPEC_VER_2_00;

	info.block_size = ic_info->sdio_info.block_sz;

	if (ic_info->sdio_info.tx_512_by_byte_mode)
		info.opn_mode = MAC_AX_SDIO_OPN_MODE_BYTE;
	else
		info.opn_mode = MAC_AX_SDIO_OPN_MODE_BLOCK;

	/*
	 * MAC_AX_HW_SDIO_INFO always return MACSUCCESS,
	 * so don't check return value here.
	 */
	mac->ops->set_hw_value(mac, MAC_AX_HW_SDIO_INFO, &info);
}

/**
 * rtw_hal_mac_sdio_tx_cfg - SDIO TX related setting
 * @hal:	pointer of struct rtw_hal_com_t
 *
 * Configure setting for SDIO TX.
 * All tx related setting which need to be config after mac init(power on)
 * should be included here.
 *
 * Config tx mode to DUMMY_AUTO to release (32K-4) bytes TX size limitation,
 * but this mode only work on SDIO host supporting block mode.
 *
 * No return value for this function.
 */
void rtw_hal_mac_sdio_tx_cfg(struct rtw_hal_com_t *hal)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum mac_ax_sdio_tx_mode mode = MAC_AX_SDIO_TX_MODE_DUMMY_AUTO;
	u32 err;


	err = mac->ops->set_hw_value(mac, MAC_AX_HW_SDIO_TX_MODE, &mode);
	if (err != MACSUCCESS)
		PHL_ERR("%s: set TX mode(%u) FAIL!(%u)\n", __func__, mode, err);
}

/**
 * rtw_hal_mac_sdio_rx_agg_cfg - SDIO RX aggregation setting
 * @hal:	pointer of struct rtw_hal_com_t
 * @enable:	enable function or not
 * @drv_define:	use driver's parameters or not
 * @timeout:	timeout threshold, unit 1us
 * @size:	size threshold, unit 1KB
 * @pkt_num:	packet number threshold
 *
 * Configure setting for SDIO RX aggregation.
 *
 * No return value for this function.
 */
void rtw_hal_mac_sdio_rx_agg_cfg(struct rtw_hal_com_t *hal, bool enable,
				 u8 drv_define, u8 timeout, u8 size, u8 pkt_num)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_rx_agg_cfg cfg = {MAC_AX_RX_AGG_MODE_NONE};


	if (enable) {
		cfg.mode = MAC_AX_RX_AGG_MODE_DMA;
		cfg.thold.drv_define = drv_define;
		cfg.thold.timeout = timeout;
		cfg.thold.size = size;
		cfg.thold.pkt_num = pkt_num;
	}

	mac->ops->set_hw_value(mac, MAC_AX_HW_SDIO_RX_AGG, &cfg);
}

/* AX TX DESC */
/* DWORD 0 ; Offset 00h */
#define GET_TX_AX_DESC_CHANNEL_DMA(_wd)		LE_BITS_TO_4BYTE(_wd, 16, 4)
#define GET_TX_AX_DESC_WP_OFFSET(_wd)		LE_BITS_TO_4BYTE(_wd, 24, 8)

/* DWORD 2 ; Offset 08h */
#define GET_TX_AX_DESC_PKT_LEN(_wd)		LE_BITS_TO_2BYTE(_wd+8, 0, 14)

/**
 * rtw_hal_mac_sdio_check_tx_allow - Check hardware resource enough to xmit
 * @hal:	pointer of struct rtw_hal_com_t
 * @dma_ch:	dma channel to xmit, should be the same as field "CHANNEL_DMA "in WD
 * @buf:	data buffer to xmit
 * @len:	data buffer length
 * @agg_count:	how many packets aggregated in this buffer
 * @pkt_len:	array to store each packet's lenght in this buffer,
 *		pkt_len should be the same as field "PKT_LEN" in WD
 * @wp_offset:	array to store each packet's wp_offset in this buffer
 *		wp_offset should be the same as field "WP_OFFSET" in WD
 * @txaddr:	return SDIO TX address for this tx buffer, and only valid when
 *		function's return value is true.
 * @txlen:	return SDIO TX length for this tx buffer, and only valid when
 *		function's return value is true. txlen would >= len and align
 *		to particular size by IC.
 *
 * Check if hardware resource is enough to send this tx buffer and return
 * SDIO TX address.
 *
 * Return true if hardware resource is enough, otherwise false.
 */
bool rtw_hal_mac_sdio_check_tx_allow(struct rtw_hal_com_t *hal, u8 dma_ch,
				     u8 *buf, u32 len, u8 agg_count,
				     u16 *pkt_len, u8 *wp_offset, u32 *txaddr,
				     u32 *txlen)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_sdio_tx_info info = {0};
	int i;
	u32 err;
	u16 pkt_len_tmp;
	u8 wp_offset_tmp;
	u8 wd_ch;


	/* For H2C, pkt_len & wp_offset may be invalid */
	if (!pkt_len && (agg_count == 1)) {
		pkt_len_tmp = GET_TX_AX_DESC_PKT_LEN(buf);
		pkt_len = &pkt_len_tmp;
	}
	if (!wp_offset && (agg_count == 1)) {
		wp_offset_tmp = (u8)GET_TX_AX_DESC_WP_OFFSET(buf);
		wp_offset = &wp_offset_tmp;
	}

	/* check tx wd */
	wd_ch = (u8)GET_TX_AX_DESC_CHANNEL_DMA(buf);
	if (wd_ch != dma_ch) {
		PHL_ERR("%s: tx channel abnormal, %u(wd) != %u(input)\n",
			__func__, wd_ch, dma_ch);
		debug_dump_data(buf, len&0xFFFF0000?0xFFFF:(u16)len, "wd:");
	}

	info.total_size = len;
	info.dma_txagg_num = agg_count;
	info.ch_dma = dma_ch;
	/* halmac request pkt_size should be little endian */
	for (i =  0; i < agg_count; i++)
		pkt_len[i] = cpu_to_le16(pkt_len[i]);
	info.pkt_size = (u8*)pkt_len;
	info.wp_offset = wp_offset;
	info.chk_cnt = 5;

	err = mac->ops->intf_ops->tx_allow_sdio(mac, &info);
	if (err != MACSUCCESS) {
		PHL_DBG("%s: tx allow fail! (%d)\n", __func__, err);
		goto err_exit;
	}

	err = mac->ops->intf_ops->tx_cmd_addr_sdio(mac, &info, txaddr);
	if (err != MACSUCCESS) {
		PHL_ERR("%s: get tx addr fail! (%d)\n", __func__, err);
		goto err_exit;
	}

	/*
	 * TX I/O size should align to 8 bytes on SDIO byte count mode,
	 * or hardware would go wrong.
	 *
	 * TODO: alignment size would be get from HALMAC return.
	 */
	*txlen = _ALIGN(len, 8);

	return true;

err_exit:
	/* Revert pkt_len to CPU endian */
	for (i =  0; i < agg_count; i++)
		pkt_len[i] = le16_to_cpu(pkt_len[i]);

	return false;
}

static enum rtw_rx_type _pkt_type_mac2phl(enum mac_ax_pkt_t mac_type)
{
	enum rtw_rx_type t = RTW_RX_TYPE_MAX;


	switch (mac_type) {
	case MAC_AX_PKT_DATA:
		t = RTW_RX_TYPE_WIFI;
		break;
	case MAC_AX_PKT_MGNT:
	case MAC_AX_PKT_CTRL:
	case MAC_AX_PKT_8023:
	case MAC_AX_PKT_H2C:
	case MAC_AX_PKT_FWDL:
		/* TODO: unknown */
		PHL_WARN("%s: unhandled RX type(%d)\n", __func__, mac_type);
		break;
	case MAC_AX_PKT_C2H:
		t = RTW_RX_TYPE_C2H;
		break;
	case MAC_AX_PKT_PPDU:
		t = RTW_RX_TYPE_PPDU_STATUS;
		break;
	case MAC_AX_PKT_CH_INFO:
		t = RTW_RX_TYPE_CHANNEL_INFO;
		break;
	case MAC_AX_PKT_DFS:
		t = RTW_RX_TYPE_DFS_RPT;
		break;
	default:
		PHL_WARN("%s: unknon RX type(%d)\n", __func__, mac_type);
		break;
	}

	return t;
}

static void hal_mac_parse_rxpkt_info(struct mac_ax_rxpkt_info *info,
			      struct rtw_r_meta_data *meta)
{
	meta->pktlen = (u16)info->pktsize;	/* DW0 [0:13] */
	meta->shift = info->shift;		/* DW0 [14:15] */
	meta->rpkt_type = _pkt_type_mac2phl(info->type); /* DW0 [24:27] */
	meta->drv_info_size = info->drvsize;	/* DW0 [28:30] */
	meta->long_rxd = (info->rxdlen == RXD_LONG_LEN) ? 1 : 0; /* DW0 [31:31] */

	if (info->type == MAC_AX_PKT_DATA) {
#if 0
		meta->dma_ch;
		meta->hal_port;
		meta->ta[6]; /* Transmitter Address */
#endif
#if 0
		meta->wl_hd_iv_len;	/* DW0 [16:21] */
		meta->bb_sel;		/* DW0 [22:22] */
		meta->mac_info_vld;	/* DW0 [23:23] */
#endif
#if 0
		meta->long_rxd;		/* DW0 [31:31] */

		meta->ppdu_type;		/* DW1 [0:3] */
		meta->ppdu_cnt;		/* DW1 [4:6] */
		meta->sr_en;		/* DW1 [7:7] */
		meta->user_id;		/* DW1 [8:15] */
		meta->rx_rate;		/* DW1 [16:24] */
		meta->rx_gi_ltf;		/* DW1 [25:27] */
		meta->non_srg_ppdu;	/* DW1 [28:28] */
		meta->inter_ppdu;		/* DW1 [29:29] */
		meta->bw;			/* DW1 [30:31] */

		meta->freerun_cnt;	/* DW2 [0:31] */

		meta->a1_match;		/* DW3 [0:0] */
		meta->sw_dec;		/* DW3 [1:1] */
		meta->hw_dec;		/* DW3 [2:2] */
		meta->ampdu;		/* DW3 [3:3] */
		meta->ampdu_end_pkt;	/* DW3 [4:4] */
		meta->amsdu;		/* DW3 [5:5] */
		meta->amsdu_cut;		/* DW3 [6:6] */
		meta->last_msdu;		/* DW3 [7:7] */
		meta->bypass;		/* DW3 [8:8] */
#endif
		meta->crc32 = info->u.data.crc_err;	/* DW3 [9:9] */
		meta->icverr = info->u.data.icv_err;	/* DW3 [10:10] */
#if 0
		meta->magic_wake;		/* DW3 [11:11] */
		meta->unicast_wake;	/* DW3 [12:12] */
		meta->pattern_wake;	/* DW3 [13:13] */
		meta->get_ch_info;		/* DW3 [14:15] */
		meta->pattern_idx;		/* DW3 [16:20] */
		meta->target_idc;		/* DW3 [21:23] */
		meta->chksum_ofld_en;	/* DW3 [24:24] */
		meta->with_llc;		/* DW3 [25:25] */
		meta->rx_statistics;	/* DW3 [26:26] */

		meta->frame_type;		/* DW4 [0:1] */
		meta->mc;			/* DW4 [2:2] */
		meta->bc;			/* DW4 [3:3] */
		meta->more_data;		/* DW4 [4:4] */
		meta->more_frag;		/* DW4 [5:5] */
		meta->pwr_bit;		/* DW4 [6:6] */
		meta->qos;			/* DW4 [7:7] */
		meta->tid;			/* DW4 [8:11] */
		meta->eosp;		/* DW4 [12:12] */
		meta->htc;			/* DW4 [13:13] */
		meta->q_null;		/* DW4 [14:14] */
		meta->seq;		/* DW4 [16:27] */
		meta->frag_num;		/* DW4 [28:31] */

		meta->sec_cam_idx;		/* DW5 [0:7] */
		meta->addr_cam;		/* DW5 [8:15] */
		meta->macid;		/* DW5 [16:23] */
		meta->rx_pl_id;		/* DW5 [24:27] */
		meta->addr_cam_vld;	/* DW5 [28:28] */
		meta->addr_fwd_en;		/* DW5 [29:29] */
		meta->rx_pl_match;		/* DW5 [30:30] */

		meta->mac_addr[6];		/* DW6 [0:31] DW7 [0:15] */
		meta->smart_ant;		/* DW7 [16:16] */
		meta->sec_type;		/* DW7 [17:20] */
#endif
	} else if (info->type == MAC_AX_PKT_PPDU) {
		meta->mac_info_vld = info->u.ppdu.mac_info; /* DW0 [23:23] */
	}
}

/* AX RX DESC */
/* DWORD 0 ; Offset 00h */
#define GET_RX_AX_DESC_PKT_LEN(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc, 0, 14)
#define GET_RX_AX_DESC_SHIFT(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc, 14, 2)
#define GET_RX_AX_DESC_HDR_IV_L(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc, 16, 6)
#define GET_RX_AX_DESC_BB_SEL(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc, 22, 1)
#define GET_RX_AX_DESC_MAC_INFO_VLD(__pRxStatusDesc) LE_BITS_TO_4BYTE( __pRxStatusDesc, 23, 1)
#define GET_RX_AX_DESC_RPKT_TYPE(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc, 24, 4)
#define GET_RX_AX_DESC_DRV_INFO_SIZE(__pRxStatusDesc) LE_BITS_TO_4BYTE( __pRxStatusDesc, 28, 3)
#define GET_RX_AX_DESC_LONG_RXD(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc, 31, 1)

/* DWORD 1 ; Offset 04h */
#define GET_RX_AX_DESC_PPDU_TYPE(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 0, 4)
#define GET_RX_AX_DESC_PPDU_CNT(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 4, 3)
#define GET_RX_AX_DESC_SR_EN(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 7, 1)
#define GET_RX_AX_DESC_USER_ID(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 8, 8)
#define GET_RX_AX_DESC_RX_DATARATE(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 16, 9)
#define GET_RX_AX_DESC_RX_GI_LTF(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 25, 3)
#define GET_RX_AX_DESC_NON_SRG_PPDU(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 28, 1)
#define GET_RX_AX_DESC_INTER_PPDU(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 29, 1)
#define GET_RX_AX_DESC_BW(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+4, 30, 2)

/* DWORD 2 ; Offset 08h */
#define GET_RX_AX_DESC_FREERUN_CNT(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+8, 0, 32)

/* DWORD 3 ; Offset 0ch */
#define GET_RX_AX_DESC_A1_MATCH(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 0, 1)
#define GET_RX_AX_DESC_SW_DEC(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 1, 1)
#define GET_RX_AX_DESC_HW_DEC(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 2, 1)
#define GET_RX_AX_DESC_AMPDU(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 3, 1)
#define GET_RX_AX_DESC_AMPDU_EDN_PKT(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 4, 1)
#define GET_RX_AX_DESC_AMSDU(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 5, 1)
#define GET_RX_AX_DESC_AMSDU_CUT(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 6, 1)
#define GET_RX_AX_DESC_LAST_MSDU(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 7, 1)
#define GET_RX_AX_DESC_BYPASS(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 8, 1)
#define GET_RX_AX_DESC_CRC32(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 9, 1)
#define GET_RX_AX_DESC_ICVERR(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 10, 1)
#define GET_RX_AX_DESC_MAGIC_WAKE(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 11, 1)
#define GET_RX_AX_DESC_UNICAST_WAKE(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 12, 1)
#define GET_RX_AX_DESC_PATTERN_WAKE(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 13, 1)

#define GET_RX_AX_DESC_CH_INFO(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 14, 1)
#define GET_RX_AX_DESC_STATISTICS(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 15, 1)
#define GET_RX_AX_DESC_PATTERN_IDX(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 16, 5)
#define GET_RX_AX_DESC_TARGET_IDC(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 21, 3)
#define GET_RX_AX_DESC_CHKSUM_OFFLOAD(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 24, 1)
#define GET_RX_AX_DESC_WITH_LLC(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+12, 25, 1)

/* DWORD 4 ; Offset 10h */
#define GET_RX_AX_DESC_TYPE(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 0, 2)
#define GET_RX_AX_DESC_MC(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 2, 1)
#define GET_RX_AX_DESC_BC(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 3, 1)
#define GET_RX_AX_DESC_MD(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 4, 1)
#define GET_RX_AX_DESC_MF(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 5, 1)
#define GET_RX_AX_DESC_PWR(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 6, 1)
#define GET_RX_AX_DESC_QOS(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 7, 1)
#define GET_RX_AX_DESC_TID(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 8, 4)
#define GET_RX_AX_DESC_EOSP(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 12, 1)
#define GET_RX_AX_DESC_HTC(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 13, 1)
#define GET_RX_AX_DESC_QNULL(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 14, 1)

#define GET_RX_AX_DESC_SEQ(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 16, 12)
#define GET_RX_AX_DESC_FRAG(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+16, 28, 4)

/* DWORD 5 ; Offset 14h */
#define GET_RX_AX_DESC_CAM_IDX(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+20, 0, 8)
#define GET_RX_AX_DESC_ADDR_CAM(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+20, 8, 8)
#define GET_RX_AX_DESC_MACID(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+20, 16, 8)
#define GET_RX_AX_DESC_PL_ID(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+20, 24, 4)
#define GET_RX_AX_DESC_CAM_VLD(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+20, 28, 1)
#define GET_RX_AX_DESC_FWD_EN(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+20, 29, 1)
#define GET_RX_AX_DESC_PL_MATCH(__pRxStatusDesc) LE_BITS_TO_4BYTE(__pRxStatusDesc+20, 30, 1)

#ifdef CONFIG_PHL_CSUM_OFFLOAD_RX
static void hal_mac_parse_rxd_checksume(struct rtw_hal_com_t *hal,
					struct rtw_r_meta_data *meta, u8 status)
{
	if ((status == MAC_AX_CHKSUM_OFD_IPV4_TCP_OK) ||
		(status == MAC_AX_CHKSUM_OFD_IPV6_TCP_OK) ||
		(status == MAC_AX_CHKSUM_OFD_IPV4_UDP_OK) ||
		(status == MAC_AX_CHKSUM_OFD_IPV6_UDP_OK))
		meta->chksum_status = 0;
	else
		meta->chksum_status = 1;
}
#endif

static void hal_mac_parse_rxd(struct rtw_hal_com_t *hal,
			      u8 *rxd, struct rtw_r_meta_data *meta)
{
#if 0
	meta->pktlen = GET_RX_AX_DESC_PKT_LEN(rxd);
	meta->shift = GET_RX_AX_DESC_SHIFT(rxd);
	meta->wl_hd_iv_len = GET_RX_AX_DESC_HDR_IV_L(rxd);
	meta->bb_sel = GET_RX_AX_DESC_BB_SEL(rxd);
	meta->mac_info_vld = GET_RX_AX_DESC_MAC_INFO_VLD(rxd);
	meta->rpkt_type = GET_RX_AX_DESC_RPKT_TYPE(rxd);
	meta->drv_info_size = GET_RX_AX_DESC_DRV_INFO_SIZE(rxd);
	meta->long_rxd = GET_RX_AX_DESC_LONG_RXD(rxd);

	meta->ppdu_type = GET_RX_AX_DESC_PPDU_TYPE(rxd);
	meta->ppdu_cnt = GET_RX_AX_DESC_PPDU_CNT(rxd);
	meta->sr_en = GET_RX_AX_DESC_SR_EN(rxd);
	meta->user_id = GET_RX_AX_DESC_USER_ID(rxd);
	meta->rx_rate = GET_RX_AX_DESC_RX_DATARATE(rxd);
	meta->rx_gi_ltf = GET_RX_AX_DESC_RX_GI_LTF(rxd);
	meta->non_srg_ppdu = GET_RX_AX_DESC_NON_SRG_PPDU(rxd);
	meta->inter_ppdu = GET_RX_AX_DESC_INTER_PPDU(rxd);
	meta->bw = GET_RX_AX_DESC_BW(rxd);

	meta->freerun_cnt = GET_RX_AX_DESC_FREERUN_CNT(rxd);

	meta->a1_match = GET_RX_AX_DESC_A1_MATCH(rxd);
#endif
	meta->sw_dec = GET_RX_AX_DESC_SW_DEC(rxd);
	meta->hw_dec = GET_RX_AX_DESC_HW_DEC(rxd);
#if 0
	meta->ampdu = GET_RX_AX_DESC_AMPDU(rxd);
	meta->ampdu_end_pkt = GET_RX_AX_DESC_AMPDU_EDN_PKT(rxd);
	meta->amsdu = GET_RX_AX_DESC_AMSDU(rxd);
#endif
	meta->amsdu_cut = GET_RX_AX_DESC_AMSDU_CUT(rxd);
	meta->last_msdu = GET_RX_AX_DESC_LAST_MSDU(rxd);
#if 0
	meta->bypass = GET_RX_AX_DESC_BYPASS(rxd);
	meta->crc32 = GET_RX_AX_DESC_CRC32(rxd);
	meta->icverr = GET_RX_AX_DESC_ICVERR(rxd);
	meta->magic_wake = GET_RX_AX_DESC_MAGIC_WAKE(rxd);
	meta->unicast_wake = GET_RX_AX_DESC_UNICAST_WAKE(rxd);
	meta->pattern_wake = GET_RX_AX_DESC_PATTERN_WAKE(rxd);
	meta->get_ch_info = GET_RX_AX_DESC_CH_INFO(rxd);
	meta->rx_statistics = GET_RX_AX_DESC_STATISTICS(rxd);

	meta->pattern_idx = GET_RX_AX_DESC_PATTERN_IDX(rxd);
	meta->target_idc = GET_RX_AX_DESC_TARGET_IDC(rxd);
	meta->chksum_ofld_en = GET_RX_AX_DESC_CHKSUM_OFFLOAD(rxd);
	meta->with_llc = GET_RX_AX_DESC_WITH_LLC(rxd);
#endif
	meta->chksum_ofld_en = GET_RX_AX_DESC_CHKSUM_OFFLOAD(rxd);

	if (meta->long_rxd == 1) {
#if 0
		meta->frame_type = GET_RX_AX_DESC_TYPE(rxd);
#endif
		meta->mc = GET_RX_AX_DESC_MC(rxd);
		meta->bc = GET_RX_AX_DESC_BC(rxd);
#if 0
		meta->more_data = GET_RX_AX_DESC_MD(rxd);
#endif
		meta->more_frag = GET_RX_AX_DESC_MF(rxd);
#if 0
		meta->pwr_bit = GET_RX_AX_DESC_PWR(rxd);
#endif
		meta->qos = GET_RX_AX_DESC_QOS(rxd);

		meta->tid = GET_RX_AX_DESC_TID(rxd);
#if 0
		meta->eosp = GET_RX_AX_DESC_EOSP(rxd);
		meta->htc = GET_RX_AX_DESC_HTC(rxd);
#endif
		meta->q_null = GET_RX_AX_DESC_QNULL(rxd);
		meta->seq = GET_RX_AX_DESC_SEQ(rxd);
		meta->frag_num = GET_RX_AX_DESC_FRAG(rxd);

#if 0
		meta->sec_cam_idx = GET_RX_AX_DESC_CAM_IDX(rxd);
		meta->addr_cam = GET_RX_AX_DESC_ADDR_CAM(rxd);

		meta->macid = GET_RX_AX_DESC_MACID(rxd);
		meta->rx_pl_id = GET_RX_AX_DESC_PL_ID(rxd);
		meta->addr_cam_vld = GET_RX_AX_DESC_CAM_VLD(rxd);
		meta->addr_fwd_en = GET_RX_AX_DESC_FWD_EN(rxd);
		meta->rx_pl_match = GET_RX_AX_DESC_PL_MATCH(rxd);
#endif

		_os_mem_cpy(hal->drv_priv,
			(void*)&meta->mac_addr, (void*)(rxd + 24), MAC_ALEN);
	}
}

int rtw_hal_mac_sdio_parse_rx(struct rtw_hal_com_t *hal,
			      struct rtw_rx_buf *rxbuf)
{
	struct hal_info_t *hal_info = hal->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_rxpkt_info info;
	struct sdio_rx_pkt *pkt;
	u8 *ptr;
	u32 len;
	int i;
	u32 offset;
	u32 err = MACSUCCESS;


	rxbuf->agg_cnt = 0;
	rxbuf->agg_start = 0;
	ptr = rxbuf->ptr;
	len = rxbuf->len;

	for (i = 0; i < MAX_BUS_RX_AGG; i++) {
		_os_mem_set(hal->drv_priv, &info, 0, sizeof(info));
		err = mac->ops->parse_rxdesc(mac, &info, ptr, len);
		if (err != MACSUCCESS) {
			PHL_ERR("%s: agg_idx=%d, len=%u(%u), parse_rxdesc FAIL!(%u)\n",
				__func__, i, len, rxbuf->len, err);
			rxbuf->len -= len;
			len = 0;
			break;
		}

		pkt = &rxbuf->pkt[i];
		_os_mem_set(hal->drv_priv, pkt, 0, sizeof(*pkt));

		pkt->wd = ptr;
		pkt->wd_len = (u8)info.rxdlen;
		offset = info.rxdlen + (info.drvsize * 8) + (info.shift * 2);
		pkt->pkt = ptr + offset;
		pkt->pkt_len = (u16)info.pktsize;
		hal_mac_parse_rxpkt_info(&info, &pkt->meta);
		hal_mac_parse_rxd(hal, ptr, &pkt->meta);
		_os_mem_cpy(hal->drv_priv, pkt->meta.ta, pkt->pkt + 10, 6);

		offset += info.pktsize;

		offset = _ALIGN(offset, 8);
#ifdef CONFIG_PHL_CSUM_OFFLOAD_RX
		if (pkt->meta.chksum_ofld_en) {
			u8 status;
			u32 result;

			status = *(ptr + offset);
			result = mac->ops->chk_rx_tcpip_chksum_ofd(mac, status);
			hal_mac_parse_rxd_checksume(hal, &pkt->meta, result);
			offset += 8;
		}
#endif /* CONFIG_PHL_CSUM_OFFLOAD_RX */
		ptr += offset;
		if (offset >= len) {
			len = 0;
			i++;
			break;
		}
		len -= offset;
	}

	rxbuf->agg_cnt = (u8)i;
	if (len) {
		/* not finish yet */
		rxbuf->next_ptr = ptr;
		offset = (u32)(ptr - rxbuf->ptr);
	} else {
		rxbuf->next_ptr = NULL;
		offset = rxbuf->len;
	}

	return offset;
}

int rtw_hal_mac_sdio_rx(struct rtw_hal_com_t *hal, struct rtw_rx_buf *rxbuf)
{
	u32 len;


	len = hal_read32(hal, R_AX_SDIO_RX_REQ_LEN) & B_AX_RX_REQ_LEN_MSK;
	if (!len)
		return 0;
	if (len > rxbuf->buf_len) {
		/* TODO: read and drop */
		return 0;
	}

	if (_FAIL == hal_sdio_cmd53_r(hal, SDIO_CMD_ADDR_RXFF, len, rxbuf->buffer))
		return 0;

	rxbuf->used_len = len;
	rxbuf->ptr = rxbuf->buffer;
	rxbuf->len = len;

#ifndef CONFIG_PHL_SDIO_READ_RXFF_IN_INT
	len = rtw_hal_mac_sdio_parse_rx(hal, rxbuf);
#endif

	return len;
}
#endif /*CONFIG_SDIO_HCI*/

enum rtw_hal_status rtw_hal_mac_get_pwr_state(struct hal_info_t *hal_info,
				enum rtw_mac_pwr_st *pwr_state)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	enum mac_ax_mac_pwr_st st;

	if (hal_mac_ops->get_hw_value(mac, MAC_AX_HW_GET_PWR_STATE, &st) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s : retrieve WL MAC state %u\n", __func__, st);

	switch (st) {
	case MAC_AX_MAC_OFF:
		*pwr_state = RTW_MAC_PWR_OFF;
		break;
	case MAC_AX_MAC_ON:
		*pwr_state = RTW_MAC_PWR_ON;
		break;
	case MAC_AX_MAC_LPS:
		*pwr_state = RTW_MAC_PWR_LPS;
		break;
	default:
		*pwr_state = RTW_MAC_PWR_NONE;
		break;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

/* halmac wrapper API for hal and proto type is at hal_api_mac.h */
enum rtw_hal_status
rtw_hal_mac_power_switch(struct rtw_phl_com_t *phl_com,
			 struct hal_info_t *hal_info,
			 u8 on_off)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	//u8 pwr_state = 0;

	/*pwr_state = hal_mac_get_pwr_state(mac);
	if(pwr_state != on_off)*/

	if (mac->ops->pwr_switch(mac, on_off) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

#ifdef DBG_PHL_MAC_REG_RW
#define HALPHY_BASE_OFFSET 0x10000

bool rtw_hal_mac_reg_chk(struct rtw_hal_com_t *hal_com, u32 addr)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	bool rst = true;
	u32 mac_rst;

	if (addr & HALPHY_BASE_OFFSET)
		goto _exit;

	if (mac != NULL && mac->ops->io_chk_access) {
		mac_rst = mac->ops->io_chk_access(mac, addr);
		if (mac_rst != MACSUCCESS) {
			rst = false;
			PHL_ERR("%s failed - addr(0x%08x) is err code(%d)\n",
				__func__, addr, mac_rst);
			_os_warn_on(1);
		}
	}
_exit:
	return rst;
}
#endif

/* halmac wrapper API for hal and proto type is at hal_api_mac.h */
enum rtw_hal_status rtw_hal_mac_dbcc_pre_cfg(struct rtw_phl_com_t *phl_com,
					     struct hal_info_t *hal_info,
					     u8 dbcc_en)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_trx_info trx_info = { 0 };
	struct mac_ax_pkt_drop_info drop_info = { 0 };

	if (dbcc_en) {
		if (true == phl_com->dev_cap.tx_mu_ru)
			trx_info.trx_mode = MAC_AX_TRX_SW_MODE;
		else
			trx_info.trx_mode = MAC_AX_TRX_HW_MODE;
		trx_info.qta_mode = MAC_AX_QTA_DBCC;

		if (mac->ops->dbcc_enable(mac, &trx_info, dbcc_en) != MACSUCCESS)
			return RTW_HAL_STATUS_FAILURE;
	} else {
		/* disable FW band 1 feature (H2C) */

		drop_info.sel = MAC_AX_PKT_DROP_SEL_BAND;
		drop_info.band = MAC_AX_BAND_1;

		if (mac->ops->pkt_drop(mac, &drop_info) != MACSUCCESS)
			return RTW_HAL_STATUS_FAILURE;
	}
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_dbcc_cfg(struct rtw_phl_com_t *phl_com,
					 struct hal_info_t *hal_info,
					 u8 dbcc_en)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_trx_info trx_info = { 0 };

	if (dbcc_en) {
		/* Pass Throught, mac already enable at pre_cfg */
	} else {
		if (true == phl_com->dev_cap.tx_mu_ru)
			trx_info.trx_mode = MAC_AX_TRX_SW_MODE;
		else
			trx_info.trx_mode = MAC_AX_TRX_HW_MODE;
		trx_info.qta_mode = MAC_AX_QTA_SCC;

		if (mac->ops->dbcc_enable(mac, &trx_info, dbcc_en) != MACSUCCESS)
			return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

u32 rtw_hal_mac_coex_init(struct rtw_hal_com_t *hal_com, u8 pta_mode, u8 direction)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_coex pta_para;

	pta_para.pta_mode = pta_mode;
	pta_para.direction = direction;

	return (ops->coex_init(mac, &pta_para));
}

u32 rtw_hal_mac_coex_reg_read(struct rtw_hal_com_t *hal_com, u32 offset, u32 *value)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;

	/* valid offset -> 0xda00~0xdaff  */
	offset = offset & 0xff;

	return (ops->coex_read(mac, offset, value));
}

u32 rtw_hal_mac_set_scoreboard(struct rtw_hal_com_t *hal_com, u32 *value)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;

	return (ops->set_hw_value(mac, MAC_AX_HW_SET_SCOREBOARD, value));
}

u32 rtw_hal_mac_get_scoreboard(struct rtw_hal_com_t *hal_com, u32 *value)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;

	return (ops->get_hw_value(mac, MAC_AX_HW_GET_SCOREBOARD, value));
}

u32 rtw_hal_mac_set_grant(struct rtw_hal_com_t *hal_com, u8 *value)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_coex_gnt gnt_val;

	gnt_val.band0.gnt_bt_sw_en = value[0];
	gnt_val.band0.gnt_bt = value[1];
	gnt_val.band0.gnt_wl_sw_en = value[2];
	gnt_val.band0.gnt_wl = value[3];

	gnt_val.band1.gnt_bt_sw_en = value[4];
	gnt_val.band1.gnt_bt = value[5];
	gnt_val.band1.gnt_wl_sw_en = value[6];
	gnt_val.band1.gnt_wl = value[7];

	return (ops->set_hw_value(mac, MAC_AX_HW_SET_COEX_GNT, &gnt_val));
}

u32 rtw_hal_mac_get_grant(struct rtw_hal_com_t *hal_com, u8 *value)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;

	return (ops->get_hw_value(mac, MAC_AX_HW_GET_COEX_GNT, value));
}

u32 rtw_hal_mac_set_polluted(struct rtw_hal_com_t *hal_com, u8 band, u8 tx_val, u8 rx_val)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_plt plt_val;

	plt_val.band = band;
	plt_val.tx = tx_val;
	plt_val.rx = rx_val;

	return (ops->set_hw_value(mac, MAC_AX_HW_SET_POLLUTED, &plt_val));
}

u32 rtw_hal_mac_set_tx_time(struct rtw_hal_com_t *hal_com, u8 is_btc, u8 is_resume, u8 macid, u32 tx_time)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_max_tx_time max_tx_time;
	u32 result = 0xffffffff;

	max_tx_time.macid = macid;
	max_tx_time.is_cctrl = 1;
	max_tx_time.max_tx_time = tx_time;

	if (is_btc && !is_resume) { /* for btc control tx time case */
		hal_com->btc_ctrl.tx_time = true;
		result = ops->set_hw_value(mac, MAC_AX_HW_SET_MAX_TX_TIME, &max_tx_time);
	} else if (is_btc && is_resume) { /* for btc release tx time case */
		result = ops->set_hw_value(mac, MAC_AX_HW_SET_MAX_TX_TIME, &max_tx_time);
		hal_com->btc_ctrl.tx_time = false;
	} else { /* not btc control case */
		if (hal_com->btc_ctrl.tx_time)
			return result;
		else
			result = ops->set_hw_value(mac, MAC_AX_HW_SET_MAX_TX_TIME, &max_tx_time);
	}

	return result;
}

u32 rtw_hal_mac_get_tx_time(struct rtw_hal_com_t *hal_com, u8 macid, u32 *tx_time)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_max_tx_time max_tx_time;
	u32 result;

	max_tx_time.macid = macid;
	max_tx_time.is_cctrl = 1;
	max_tx_time.max_tx_time = *tx_time;

	result = ops->get_hw_value(mac, MAC_AX_HW_GET_MAX_TX_TIME, &max_tx_time);
	*tx_time = max_tx_time.max_tx_time;

	return (result);
}

u32 rtw_hal_mac_set_tx_retry_limit(struct rtw_hal_com_t *hal_com, u8 is_btc, u8 is_resume, u8 macid, u8 tx_retry)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_cctl_rty_lmt_cfg tx_retry_limit;
	u32 result = 0xffffffff;

	tx_retry_limit.macid = macid;
	tx_retry_limit.data_lmt_sel = 1;
	tx_retry_limit.data_lmt_val = tx_retry;
	tx_retry_limit.rts_lmt_sel = 0;
	tx_retry_limit.rts_lmt_val = 0;

	if (is_btc && !is_resume) { /* for btc control tx time case */
		hal_com->btc_ctrl.tx_retry = true;
		result = ops->set_hw_value(mac, MAC_AX_HW_SET_CCTL_RTY_LMT, &tx_retry_limit);
	} else if (is_btc && is_resume) { /* for btc release tx time case */
		result = ops->set_hw_value(mac, MAC_AX_HW_SET_CCTL_RTY_LMT, &tx_retry_limit);
		hal_com->btc_ctrl.tx_retry = false;
	} else { /* not btc control case */
		if (hal_com->btc_ctrl.tx_retry)
			return result;
		else
			result = ops->set_hw_value(mac, MAC_AX_HW_SET_CCTL_RTY_LMT, &tx_retry_limit);
	}

	return result;
}

u32 rtw_hal_mac_get_tx_retry_limit(struct rtw_hal_com_t *hal_com, u8 macid, u8 *tx_retry)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_rty_lmt tx_retry_limit;
	u32 result;

	tx_retry_limit.macid = macid;
	tx_retry_limit.tx_cnt = *tx_retry;
	result = ops->get_hw_value(mac, MAC_AX_HW_GET_DATA_RTY_LMT, &tx_retry_limit);
	*tx_retry = (u8)tx_retry_limit.tx_cnt;

	return (result);
}

u32 rtw_hal_mac_get_bt_polt_cnt(struct rtw_hal_com_t *hal_com, u8 band, u16 *cnt)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_bt_polt_cnt polt;
	u32 result;

	polt.band = band;
	polt.cnt = *cnt;
	result = ops->get_hw_value(mac, MAC_AX_HW_GET_POLLUTED_CNT, &polt);
	*cnt = polt.cnt;

	return (result);
}

u32 rtw_hal_mac_set_coex_ctrl(struct rtw_hal_com_t *hal_com, u32 val)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;

	return (ops->set_hw_value(mac, MAC_AX_HW_SET_COEX_CTRL, &val));
}

u32 rtw_hal_mac_get_coex_ctrl(struct rtw_hal_com_t *hal_com, u32* val)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;

	return (ops->get_hw_value(mac, MAC_AX_HW_GET_COEX_CTRL, val));
}

u32 rtw_hal_mac_coex_reg_write(struct rtw_hal_com_t *hal_com, u32 offset, u32 value)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;

	/* valid offset -> 0xda00~0xdaff  */
	offset = offset & 0xff;

	return (ops->coex_write(mac, offset, value));
}

/* halmac wrapper API for hal and proto type is at hal_api_drv.h */
u32 rtw_hal_mac_send_h2c(struct rtw_hal_com_t *hal_com,
	struct rtw_g6_h2c_hdr *hdr, u32 *pvalue)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	u32 mac_err = 0;

	mac_err = ops->outsrc_h2c_common(mac, hdr, pvalue);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

static enum mac_ax_net_type
_rtype_to_mac_nettype(struct rtw_wifi_role_t *wifi_role)
{
	enum mac_ax_net_type net_type = MAC_AX_NET_TYPE_NO_LINK;

	switch (wifi_role->type) {
	case PHL_RTYPE_STATION:
	case PHL_RTYPE_TDLS:
		net_type = (wifi_role->mstate == MLME_NO_LINK)
			? MAC_AX_NET_TYPE_NO_LINK
			: MAC_AX_NET_TYPE_INFRA;
		break;
	case PHL_RTYPE_MONITOR:
	case PHL_RTYPE_P2P_DEVICE:
		net_type = MAC_AX_NET_TYPE_NO_LINK;
		break;
	case PHL_RTYPE_P2P_GC:
		net_type = MAC_AX_NET_TYPE_INFRA;
		break;
	case PHL_RTYPE_AP:
	case PHL_RTYPE_P2P_GO:
	case PHL_RTYPE_MESH:
		net_type = MAC_AX_NET_TYPE_AP;
		break;
	case PHL_RTYPE_ADHOC:
	case PHL_RTYPE_ADHOC_MASTER:
		net_type = MAC_AX_NET_TYPE_ADHOC;
		break;
	case PHL_RTYPE_NAN: /*TODO*/
	default:
		net_type = MAC_AX_NET_TYPE_NO_LINK;
		break;
	}

	return net_type;
}


enum rtw_hal_status
rtw_hal_mac_port_init(struct hal_info_t *hal_info,
					struct rtw_wifi_role_t *wifi_role)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_port_init_para ppara = {0};
	struct rtw_phl_stainfo_t *phl_sta;

	ppara.net_type = _rtype_to_mac_nettype(wifi_role);
	ppara.band_idx = wifi_role->hw_band;
	ppara.port_idx = wifi_role->hw_port;
	/*#ifdef CONFIG_AP*/
	ppara.hiq_win = wifi_role->hiq_win;
	ppara.dtim_period  = wifi_role->dtim_period;
	ppara.mbid_num = wifi_role->mbid_num;/*max mbid number*/
	/*#endif - CONFIG_AP*/

	if (ppara.net_type == MAC_AX_NET_TYPE_INFRA) {
		phl_sta = rtw_phl_get_stainfo_self(wifi_role->phl_com->phl_priv,
						   wifi_role);
		if (phl_sta->asoc_cap.bcn_interval)
			ppara.bcn_interval = phl_sta->asoc_cap.bcn_interval;
		else
			ppara.bcn_interval = 100;
		ppara.bss_color = phl_sta->asoc_cap.bsscolor;
	} else if (ppara.net_type == MAC_AX_NET_TYPE_AP) {
		#ifdef RTW_PHL_BCN
		ppara.bcn_interval = (u16)wifi_role->bcn_cmn.bcn_interval;
		#else
		ppara.bcn_interval = 100;
		#endif
		ppara.bss_color = wifi_role->proto_role_cap.bsscolor;
	} else if (ppara.net_type == MAC_AX_NET_TYPE_ADHOC) {
		/* TODO */
		ppara.bcn_interval = 100;
	} else {
		/* other net_type, i.e. MAC_AX_NO_LINK */
		ppara.bcn_interval = 100;
	}

	if (mac->ops->port_init(mac, &ppara) == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
	return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_port_cfg(struct hal_info_t *hal_info,
			struct rtw_wifi_role_t *wifi_role,
			enum pcfg_type type, void *param)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum mac_ax_port_cfg_type ptype = MAC_AX_PCFG_FUNC_SW;
	struct mac_ax_port_cfg_para ppara = {0};

	switch (type){
	case PCFG_FUNC_SW :
		ptype = MAC_AX_PCFG_FUNC_SW;
		break;
	case PCFG_TBTT_AGG :
		ptype = MAC_AX_PCFG_TBTT_AGG;
		break;
	case PCFG_TBTT_SHIFT :
		ptype = MAC_AX_PCFG_TBTT_SHIFT;
		break;
	case PCFG_HIQ_WIN :
		ptype = MAC_AX_PCFG_HIQ_WIN;
		break;
	case PCFG_HIQ_DTIM :
		ptype = MAC_AX_PCFG_HIQ_DTIM;
		break;
	case PCFG_BCN_INTERVAL :
		ptype = MAC_AX_PCFG_BCN_INTV;
		break;
	case PCFG_BSS_CLR :
		ptype = MAC_AX_PCFG_BSS_CLR;
		break;
	case PCFG_BCN_EN :
		ptype = MAC_AX_PCFG_TX_SW;
		break;
	default :
		PHL_ERR("Unknown port cfg type %d\n", type);
		goto _error;
	}

	ppara.band = wifi_role->hw_band;
	ppara.port = wifi_role->hw_port;
	#ifdef RTW_PHL_BCN/*#ifdef CONFIG_AP*/
	ppara.mbssid_idx = wifi_role->hw_mbssid;
	#endif
	ppara.val = *(u32 *)param;

	if (mac->ops->port_cfg(mac, ptype, &ppara) == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
_error:
	return RTW_HAL_STATUS_FAILURE;
}

static enum mac_ax_wifi_role
_rtype_to_mac_wifirole(struct rtw_wifi_role_t *wifi_role)
{
	enum mac_ax_wifi_role mac_wifi_role = MAC_AX_WIFI_ROLE_NONE;

	switch (wifi_role->type) {
	case PHL_RTYPE_NONE:
		mac_wifi_role = MAC_AX_WIFI_ROLE_NONE;
		break;
	case PHL_RTYPE_STATION:
	case PHL_RTYPE_TDLS:
		mac_wifi_role = MAC_AX_WIFI_ROLE_STATION;
		break;
	case PHL_RTYPE_AP:
		mac_wifi_role = MAC_AX_WIFI_ROLE_AP;
		break;
	case PHL_RTYPE_VAP:
		mac_wifi_role = MAC_AX_WIFI_ROLE_VAP;
		break;
	case PHL_RTYPE_ADHOC:
		mac_wifi_role = MAC_AX_WIFI_ROLE_ADHOC;
		break;
	case PHL_RTYPE_ADHOC_MASTER:
		mac_wifi_role = MAC_AX_WIFI_ROLE_ADHOC_MASTER;
		break;
	case PHL_RTYPE_MESH:
		mac_wifi_role = MAC_AX_WIFI_ROLE_MESH;
		break;
	case PHL_RTYPE_MONITOR:
		mac_wifi_role = MAC_AX_WIFI_ROLE_MONITOR;
		break;
	case PHL_RTYPE_P2P_DEVICE:
		mac_wifi_role = MAC_AX_WIFI_ROLE_P2P_DEVICE;
		break;
	case PHL_RTYPE_P2P_GC:
		mac_wifi_role = MAC_AX_WIFI_ROLE_P2P_GC;
		break;
	case PHL_RTYPE_P2P_GO:
		mac_wifi_role = MAC_AX_WIFI_ROLE_P2P_GO;
		break;
	case PHL_RTYPE_NAN:
		mac_wifi_role = MAC_AX_WIFI_ROLE_NAN;
		break;
	default:
		mac_wifi_role = MAC_AX_WIFI_ROLE_STATION;
		break;
	}

	return mac_wifi_role;
}

static enum mac_ax_upd_mode
	_hal_updmode_to_mac_upt_mode(enum phl_upd_mode mode)
{
	enum mac_ax_upd_mode upd_mode = MAC_AX_ROLE_INFO_CHANGE;

	switch (mode) {
	case PHL_UPD_ROLE_CREATE:
		upd_mode = MAC_AX_ROLE_CREATE;
		break;
	case PHL_UPD_ROLE_REMOVE:
		upd_mode = MAC_AX_ROLE_REMOVE;
		break;
	case PHL_UPD_ROLE_TYPE_CHANGE:
		upd_mode = MAC_AX_ROLE_TYPE_CHANGE;
		break;
	case PHL_UPD_ROLE_INFO_CHANGE:
	case PHL_UPD_STA_INFO_CHANGE:
		upd_mode = MAC_AX_ROLE_INFO_CHANGE;
		break;
	case PHL_UPD_STA_CON_DISCONN:
		upd_mode = MAC_AX_ROLE_CON_DISCONN;
		break;
	case PHL_UPD_ROLE_MAX:
	/* fallthrough */
	default:
		PHL_ERR("error upt mode %d\n", mode);
		break;
	}

	return upd_mode;
}

static void _hal_stainfo_to_macrinfo(struct hal_info_t *hal_info,
			struct rtw_phl_stainfo_t *sta,
			struct mac_ax_role_info *rinfo,
			enum phl_upd_mode mode,
			bool is_connect)
{
	struct rtw_wifi_role_t *wifi_role = sta->wrole;
	void *drv = hal_to_drvpriv(hal_info);
	bool is_self = false;

	rinfo->macid = (u8)sta->macid;
	rinfo->band = wifi_role->hw_band;
	rinfo->port = wifi_role->hw_port;
	rinfo->wmm = wifi_role->hw_wmm;

	rinfo->net_type = _rtype_to_mac_nettype(wifi_role);
	rinfo->wifi_role = _rtype_to_mac_wifirole(wifi_role);
	rinfo->bcn_hit_cond = sta->bcn_hit_cond;
	rinfo->hit_rule = sta->hit_rule;
	rinfo->tsf_sync = wifi_role->hw_port;
	rinfo->aid = sta->aid;
	rinfo->wapi = sta->wapi;
	rinfo->sec_ent_mode = sta->sec_mode;
	rinfo->upd_mode = _hal_updmode_to_mac_upt_mode(mode);
	rinfo->opmode = (is_connect == true) ? MAC_AX_ROLE_CONNECT : MAC_AX_ROLE_DISCONN;

	if (rinfo->net_type == MAC_AX_NET_TYPE_AP) {
		if (_os_mem_cmp(drv, wifi_role->mac_addr, sta->mac_addr, MAC_ALEN) == 0)
			is_self = true;

		if(is_self == true) {
			rinfo->self_role = MAC_AX_SELF_ROLE_AP;
		} else {
			rinfo->self_role = MAC_AX_SELF_ROLE_AP_CLIENT;
			/* for ap client disconnect case,
			need to set no-link only for MAC_AX_ROLE_CON_DISCONN mode */
			if (is_connect == false && rinfo->upd_mode == MAC_AX_ROLE_CON_DISCONN)
				rinfo->net_type = MAC_AX_NET_TYPE_NO_LINK;
			/* only for client under AX SoftAP mode  */
			if (sta->wmode & WLAN_MD_11AX)
				rinfo->tf_mac_padding = sta->asoc_cap.trig_padding;
		}
	} else if (rinfo->net_type == MAC_AX_NET_TYPE_INFRA || rinfo->net_type == MAC_AX_NET_TYPE_NO_LINK) {
		rinfo->self_role = MAC_AX_SELF_ROLE_CLIENT;
	}

	if ((sta->wmode & WLAN_MD_11AX) && (wifi_role->mstate == MLME_LINKED)) {
		rinfo->trigger = sta->tf_trs;
		rinfo->bss_color = sta->asoc_cap.bsscolor;
		rinfo->addr_mask = (sta->addr_msk > 0)?MAC_AX_BYTE5:MAC_AX_MSK_NONE;
		rinfo->mask_sel = (sta->addr_sel > 0)?MAC_AX_BSSID_MSK:MAC_AX_NO_MSK;
	}

	//TODO
	switch (rinfo->net_type) {
	case MAC_AX_NET_TYPE_NO_LINK :
		_os_mem_cpy(drv, rinfo->self_mac, wifi_role->mac_addr, MAC_ALEN);
		break;
	case MAC_AX_NET_TYPE_ADHOC :
		_os_mem_cpy(drv, rinfo->self_mac, wifi_role->mac_addr, MAC_ALEN);
		break;
	case MAC_AX_NET_TYPE_INFRA :
		rinfo->aid = sta->aid;
		_os_mem_cpy(drv, rinfo->self_mac, wifi_role->mac_addr, MAC_ALEN);
		_os_mem_cpy(drv, rinfo->target_mac, sta->mac_addr, MAC_ALEN);
		_os_mem_cpy(drv, rinfo->bssid, sta->mac_addr, MAC_ALEN);
		break;
	case MAC_AX_NET_TYPE_AP :
		_os_mem_cpy(drv, rinfo->self_mac, wifi_role->mac_addr, MAC_ALEN);
		_os_mem_cpy(drv, rinfo->target_mac, sta->mac_addr, MAC_ALEN);
		_os_mem_cpy(drv, rinfo->bssid, wifi_role->mac_addr, MAC_ALEN);
		break;
	default :
		break;
	}

}


enum rtw_hal_status
rtw_hal_mac_role_sync(struct hal_info_t *hal_info,
	struct rtw_phl_stainfo_t *sta)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_role_info mac_rinfo = {0};
	u32 rst = 0;

	_hal_stainfo_to_macrinfo(hal_info, sta, &mac_rinfo, PHL_UPD_ROLE_CREATE,
		false);

	rst = mac->ops->role_sync(mac, &mac_rinfo);

	if ((rst == MACSUCCESS) || (rst == MACSAMACID))
		return RTW_HAL_STATUS_SUCCESS;

	return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_addr_cam_add_entry(struct hal_info_t *hal_info,
					struct rtw_phl_stainfo_t *sta)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_role_info mac_rinfo = {0};
	u32 rst = 0;

	_hal_stainfo_to_macrinfo(hal_info, sta, &mac_rinfo, PHL_UPD_ROLE_CREATE, false);

	rst = mac->ops->add_role(mac, &mac_rinfo);

	if ((rst == MACSUCCESS) || (rst == MACSAMACID))
		return RTW_HAL_STATUS_SUCCESS;

	return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_addr_cam_change_entry(struct hal_info_t *hal_info,
					struct rtw_phl_stainfo_t *sta,
					enum phl_upd_mode mode,
					bool is_connect)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_role_info mac_rinfo = {0};

	_hal_stainfo_to_macrinfo(hal_info, sta, &mac_rinfo, mode, is_connect);

	if (mac->ops->change_role(mac, &mac_rinfo) == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;

	return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_addr_cam_set_aid(struct hal_info_t *hal_info,
			     struct rtw_phl_stainfo_t *sta,
			     u16 aid)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_role_info mac_rinfo = {0};

	_hal_stainfo_to_macrinfo(hal_info, sta, &mac_rinfo, PHL_UPD_STA_INFO_CHANGE, true);

	mac_rinfo.aid = aid;

	if (mac->ops->change_role(mac, &mac_rinfo) == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;

	return RTW_HAL_STATUS_FAILURE;
}


enum rtw_hal_status
rtw_hal_mac_addr_cam_del_entry(struct hal_info_t *hal_info,
					struct rtw_phl_stainfo_t *sta)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->remove_role(mac, (u8)sta->macid) == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
	return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_add_key(struct hal_info_t *hal_info, u8 macid, u8 type, u8 ext_key,
					u8 spp,	u8 keyid, u8 keytype, u8 *keybuf)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	void *drv = hal_to_drvpriv(hal_info);
	struct mac_ax_sec_cam_info sec_cam;
	u32 mac_err;

	sec_cam.type = type;
	sec_cam.ext_key = ext_key;
	sec_cam.spp_mode = spp;
	sec_cam.len = 20;
	sec_cam.offset = 0;
	_os_mem_set(drv, &sec_cam.key, 0, sizeof(sec_cam.key));

	switch (type)
	{
		case RTW_ENC_WEP40:
			_os_mem_cpy(drv, &sec_cam.key, keybuf, 5);
			break;
		case RTW_ENC_WEP104:
			_os_mem_cpy(drv, &sec_cam.key, keybuf, 13);
			break;
		case RTW_ENC_TKIP:
		case RTW_ENC_CCMP:
		case RTW_ENC_CCMP256:
		case RTW_ENC_GCMP:
		case RTW_ENC_GCMP256:
		case RTW_ENC_BIP_CCMP128:
		case RTW_ENC_WAPI:
		case RTW_ENC_GCMSMS4:
			_os_mem_cpy(drv, &sec_cam.key, keybuf, 16);
			break;
		default:
			break;
	}

	mac_err = mac->ops->sta_add_key(mac, &sec_cam, macid, keyid, keytype);

	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_delete_key(struct hal_info_t *hal_info, u8 macid, u8 type,
						u8 ext_key, u8 spp, u8 keyid, u8 keytype)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 mac_err;

	mac_err = mac->ops->sta_del_key(mac, macid, keyid, keytype);

	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

u32
rtw_hal_mac_search_key_idx(struct hal_info_t *hal_info, u8 macid,
						u8 keyid, u8 keytype)
{
	u32 sec_cam_idx;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	sec_cam_idx = mac->ops->sta_search_key_idx(mac, macid, keyid, keytype);

	return sec_cam_idx;
}

u32
rtw_hal_mac_ser_reset_wdt_intr(struct hal_info_t *hal_info)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *ops = mac->ops;
	u8 dummyval = 0;
	u32 result = 0xffffffff;

	result = ops->set_hw_value(mac, MAC_AX_HW_SET_WDT_ISR_RST, &dummyval);

	return result;
}

enum rtw_hal_status
rtw_hal_mac_ser_get_error_status(struct hal_info_t *hal_info, u32 *err)
{
	u32 mac_err = 0;
	enum mac_ax_err_info err_info = 0;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	mac_err = mac->ops->get_err_status(mac, &err_info);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	*err = err_info;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_ser_set_error_status(struct hal_info_t *hal_info, enum RTW_PHL_SER_RCVY_STEP err)
{
	u32 mac_err = 0;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 err_info = 0;

	if (err == RTW_PHL_SER_L1_DISABLE_EN) {
		err_info = MAC_AX_ERR_L1_DISABLE_EN;
	} else if (err == RTW_PHL_SER_L1_RCVY_EN) {
		err_info = MAC_AX_ERR_L1_RCVY_EN;
	} else if (err == RTW_PHL_SER_L0_CFG_NOTIFY) {
		err_info = MAC_AX_ERR_L0_CFG_NOTIFY;
	} else if (err == RTW_PHL_SER_L0_CFG_DIS_NOTIFY) {
		err_info = MAC_AX_ERR_L0_CFG_DIS_NOTIFY;
	} else if (err == RTW_PHL_SER_L0_CFG_HANDSHAKE) {
		err_info = MAC_AX_ERR_L0_CFG_HANDSHAKE;
	} else if (err == RTW_PHL_SER_L0_RCVY_EN) {
		err_info = MAC_AX_ERR_L0_RCVY_EN;
	}
	PHL_INFO("%s : error info to mac 0x%x.\n", __func__, err_info);

	mac_err = mac->ops->set_err_status(mac, err_info);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_trigger_cmac_err(struct hal_info_t *hal_info)
{
	u32 mac_err = 0;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	mac_err = mac->ops->trigger_cmac_err(mac);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_trigger_dmac_err(struct hal_info_t *hal_info)
{
	u32 mac_err = 0;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	mac_err = mac->ops->trigger_dmac_err(mac);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_lv1_rcvy(struct hal_info_t *hal_info, enum rtw_phl_ser_lv1_recv_step step)
{
	u32 mac_err = 0;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	mac_err = mac->ops->lv1_rcvy(mac, step);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_ser_ctrl(struct hal_info_t *hal_info, bool en)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 mac_err = 0;
	enum mac_ax_func_sw cfg = (en == true) ? MAC_AX_FUNC_EN : MAC_AX_FUNC_DIS;
	u32 start_t = 0;

	start_t = _os_get_cur_time_us();

	PHL_INFO("%s\n", __func__);

	mac_err = mac->ops->ser_ctrl(mac, cfg);

	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	PHL_INFO("%s : %s ser success with %d us.\n", __func__,
			(en ? "start" : "stop"), phl_get_passing_time_us(start_t));

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_poll_hw_tx_done(struct hal_info_t *hal_info)
{
	PHL_TRACE(COMP_PHL_MAC, _PHL_WARNING_, "poll hw tx done is not supported now!\n");
	return RTW_HAL_STATUS_MAC_API_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_hw_tx_resume(struct hal_info_t *hal_info)
{
	PHL_TRACE(COMP_PHL_MAC, _PHL_WARNING_, "resume hw tx is not supported now!\n");
	return RTW_HAL_STATUS_MAC_API_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_poll_hw_rx_done(struct hal_info_t *hal_info)
{
	PHL_TRACE(COMP_PHL_MAC, _PHL_WARNING_, "poll hw rx done is not supported now!\n");
	return RTW_HAL_STATUS_MAC_API_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_hw_rx_resume(struct hal_info_t *hal_info)
{
	PHL_TRACE(COMP_PHL_MAC, _PHL_WARNING_, "resume hw rx is not supported now!\n");
	return RTW_HAL_STATUS_MAC_API_FAILURE;
}


#define FW_PLE_SIZE 0x800
#define FW_PLE_SEGMENT_SIZE 512 /*Because PHL_PRINT have prefix 5 bytes "PHL: "*/

void
_hal_fw_dbg_dump(struct hal_info_t *hal_info, u8 *buffer, u16 bufsize)
{
	char str[FW_PLE_SEGMENT_SIZE + 1] = {0};
	u16 i = 0, ofst = 0;

	for (ofst = 0; ofst < bufsize; ofst += FW_PLE_SEGMENT_SIZE) {
 		for (i = 0; i < FW_PLE_SEGMENT_SIZE; i++)
			_os_snprintf(str + i, 2, "%c", buffer[i + ofst]);
		PHL_PRINT("%s\n", str);
		_os_mem_set(hal_to_drvpriv(hal_info), str, 0, sizeof(str));
	}
}

enum rtw_hal_status
rtw_hal_mac_dump_fw_rsvd_ple(struct hal_info_t *hal_info)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u8 *buffer = NULL;
	u16 bufSize = FW_PLE_SIZE;

	if(mac->ops->dump_fw_rsvd_ple(mac, &buffer) != 0) {
		PHL_ERR("%s fail!\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}

	if (buffer != NULL) {
		PHL_PRINT("=======================\n");
		PHL_PRINT("Start to dump fw rsvd ple :\n\n");
		_hal_fw_dbg_dump(hal_info, buffer, bufSize);
		PHL_PRINT("\n=======================\n");
		_os_mem_free(hal_info->hal_com->drv_priv, buffer, bufSize);
	} else {
		PHL_ERR("dump_fw_rsvd_ple return invalid buffer!\n");
	}

	return RTW_HAL_STATUS_SUCCESS;
}


/*
 * halmac wrapper API for hal and proto type is at hal_api_mac.h
 * init HW scope or start HW scope?
 */
enum rtw_hal_status
rtw_hal_mac_init_mac(void *mac,struct hal_init_info_t *init_info)
{
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	struct mac_ax_ops *hal_mac_ops = mac_info->ops;
	struct mac_ax_trx_info trx_info;

	trx_info.trx_mode = init_info->trx_info.trx_mode;
	trx_info.qta_mode = init_info->trx_info.qta_mode;

	if (hal_mac_ops->sys_init(mac_info))
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	if (hal_mac_ops->trx_init(mac_info, &trx_info))
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_trx_init(void *mac, struct hal_init_info_t *init_info)
{
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	struct mac_ax_ops *hal_mac_ops = mac_info->ops;
	struct mac_ax_trx_info trx_info;

	trx_info.trx_mode = init_info->trx_info.trx_mode;
	trx_info.qta_mode = init_info->trx_info.qta_mode;

	if (hal_mac_ops->trx_init(mac_info, &trx_info))
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

void _hal_mac_get_ofld_cap(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal_info)
{
#ifdef CONFIG_FW_IO_OFLD_SUPPORT
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 mac_status = 0;

	mac_status = mac->ops->get_hw_value(mac, MAC_AX_HW_GET_FW_CAP,
			&(hal_info->hal_com->dev_hw_cap.fw_cap.offload_cap));
	if (mac_status == MACSUCCESS) {
		phl_com->dev_cap.fw_cap.offload_cap = phl_com->dev_sw_cap.fw_cap.offload_cap &
						      hal_info->hal_com->dev_hw_cap.fw_cap.offload_cap;
		PHL_INFO("%s: sw ofld cap: 0x%x, fw ofld cap 0x%x, final ofld cap: 0x%x!\n", __func__,
				phl_com->dev_sw_cap.fw_cap.offload_cap,
				hal_info->hal_com->dev_hw_cap.fw_cap.offload_cap,
				phl_com->dev_cap.fw_cap.offload_cap);
	} else {
		hal_info->hal_com->dev_hw_cap.fw_cap.offload_cap = 0;
		phl_com->dev_cap.fw_cap.offload_cap = 0;
		PHL_WARN("%s: fw ofld cap not enabled.\n", __func__);
	}
#else
	hal_info->hal_com->dev_hw_cap.fw_cap.offload_cap = 0;
	phl_com->dev_cap.fw_cap.offload_cap = 0;
	PHL_INFO("%s: fw ofld cap not enabled.\n", __func__);
#endif
}

enum rtw_hal_status
rtw_hal_mac_hal_init(struct rtw_phl_com_t *phl_com,
		     struct hal_info_t *hal_info,
		     struct hal_init_info_t *init_info)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_fw_info_t *fw_info = &phl_com->fw_info;
	struct hal_ops_t *hal_ops = hal_get_ops(hal_info);
	struct mac_ax_fwdl_info fwdl_info;
	u32 mac_status = 0;
	enum rtw_fw_type fw_type = RTW_FW_MAX;
#ifdef CONFIG_PHL_CSUM_OFFLOAD_RX
	u8 tx_chksum_offload = 0;
	u8 rx_chksum_offload = 0;
#endif
	FUNCIN_WSTS(hstatus);

#ifdef PHL_FEATURE_NIC
	fw_type = RTW_FW_NIC;
#elif defined(PHL_FEATURE_AP)
	fw_type = RTW_FW_AP;
#else
	fw_type  = RTW_FW_MAX;
#endif

	hstatus = hal_ops->hal_cfg_fw(phl_com, hal_info, init_info->ic_name, fw_type);
	if(RTW_HAL_STATUS_SUCCESS != hstatus) {
		PHL_ERR("%s : Cfg FW Failed: %d!\n", __func__, hstatus);
		return hstatus;
	}
	/* fwdl_info */
	fwdl_info.fw_en = fw_info->fw_en;
	fwdl_info.dlram_en = fw_info->dlram_en;
	fwdl_info.dlrom_en = fw_info->dlrom_en;
	fwdl_info.ram_buff = fw_info->ram_buff;
	fwdl_info.ram_size = fw_info->ram_size;
	fwdl_info.rom_buff = fw_info->rom_buff;
	fwdl_info.rom_size = fw_info->rom_size;
	fwdl_info.fw_cat = fw_info->fw_type;

	if(fw_info->fw_src == RTW_FW_SRC_EXTNAL)
		fwdl_info.fw_from_hdr = 0;
	else
		fwdl_info.fw_from_hdr = 1;

	mac_status = mac->ops->hal_init(mac, &init_info->trx_info, &fwdl_info, &init_info->intf_info);

	if (mac_status == MACSUCCESS) {
		hstatus = RTW_HAL_STATUS_SUCCESS;
		hal_mac_print_fw_version(hal_info);
	} else {
		hstatus = RTW_HAL_STATUS_MAC_INIT_FAILURE;
		PHL_ERR("%s : mac_status %d!\n", __func__, mac_status);
	}
	_hal_mac_get_ofld_cap(phl_com, hal_info);
#ifdef CONFIG_PHL_CSUM_OFFLOAD_RX
	rx_chksum_offload = 1;
	mac_status = mac->ops->tcpip_chksum_ofd(mac, tx_chksum_offload, rx_chksum_offload);
	if (mac_status != MACSUCCESS)
		PHL_ERR("%s : tcpip_chksum_ofd mac_status %d!!!!!!!\n", __func__, mac_status);
#endif

	FUNCOUT_WSTS(hstatus);

	return hstatus;
}

enum rtw_hal_status
rtw_hal_mac_hal_fast_init(struct rtw_phl_com_t *phl_com,
			  struct hal_info_t *hal_info,
			  struct hal_init_info_t *init_info)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_fw_info_t *fw_info = &phl_com->fw_info;
	struct hal_ops_t *hal_ops = hal_get_ops(hal_info);
	struct mac_ax_fwdl_info fwdl_info;
	u32 mac_status = 0;
	enum rtw_fw_type fw_type = RTW_FW_MAX;

	FUNCIN_WSTS(hstatus);

#ifdef PHL_FEATURE_NIC
	fw_type = RTW_FW_NIC;
#elif defined(PHL_FEATURE_AP)
	fw_type = RTW_FW_AP;
#else
	fw_type  = RTW_FW_MAX;
#endif
	hstatus = hal_ops->hal_cfg_fw(phl_com, hal_info, init_info->ic_name, fw_type);
	if(RTW_HAL_STATUS_SUCCESS != hstatus) {
		PHL_ERR("%s : Cfg FW Failed: %d!\n", __func__, hstatus);
		return hstatus;
	}

	/* fwdl_info */
	fwdl_info.fw_en = fw_info->fw_en;
	fwdl_info.dlram_en = fw_info->dlram_en;
	fwdl_info.dlrom_en = fw_info->dlrom_en;
	fwdl_info.ram_buff = fw_info->ram_buff;
	fwdl_info.ram_size = fw_info->ram_size;
	fwdl_info.rom_buff = fw_info->rom_buff;
	fwdl_info.rom_size = fw_info->rom_size;
	fwdl_info.fw_cat = fw_info->fw_type;

	if(fw_info->fw_src == RTW_FW_SRC_EXTNAL)
		fwdl_info.fw_from_hdr = 0;
	else
		fwdl_info.fw_from_hdr = 1;

	mac_status = mac->ops->hal_fast_init(mac, &init_info->trx_info, &fwdl_info, &init_info->intf_info);
	if (mac_status == MACSUCCESS)
		hstatus = RTW_HAL_STATUS_SUCCESS;
	else {

		hstatus = RTW_HAL_STATUS_HAL_INIT_FAILURE;
		PHL_ERR("%s : mac_status %d!\n", __func__, mac_status);
	}

	FUNCOUT_WSTS(hstatus);

	return hstatus;
}

enum rtw_hal_status
rtw_hal_mac_hal_deinit(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal_info)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 mac_status = 0;

	mac_status = mac->ops->hal_deinit(mac);

	if (mac_status == MACSUCCESS)
		hstatus = RTW_HAL_STATUS_SUCCESS;
	else
		PHL_ERR("%s : mac_status %d!\n", __func__, mac_status);

	FUNCOUT_WSTS(hstatus);

	return hstatus;
}

enum rtw_hal_status
rtw_hal_mac_chk_allq_empty(struct hal_info_t *hal_info, u8 *empty)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;

	if (hal_mac_ops->chk_allq_empty(mac, empty))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

#ifdef CONFIG_WOWLAN
enum rtw_hal_status
rtw_hal_mac_cfg_wow_sleep(struct hal_info_t *hal_info, u8 sleep)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;

	if (hal_mac_ops->cfg_wow_sleep(mac,sleep))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_wow_fw_status(struct hal_info_t *hal_info, u8 *status, u8 func_en)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;

	if (hal_mac_ops->get_wow_fw_status(mac,status, func_en))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_cfg_keep_alive(struct hal_info_t *hal_info, u16 macid, u8 en,
								struct rtw_keep_alive_info *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_keep_alive_info info = {0};

	if (en && cfg == NULL)
		return RTW_HAL_STATUS_FAILURE;

	_os_mem_set(hal_to_drvpriv(hal_info), &info, 0, sizeof(info));

	if (en) {
		info.keepalive_en = cfg->keep_alive_en;
		info.period = cfg->keep_alive_period;
		info.packet_id = cfg->null_pkt_id;
	}

	if (hal_mac_ops->cfg_keepalive(mac, (u8)macid, &info))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_cfg_disc_dec(struct hal_info_t *hal_info, u16 macid, u8 en, struct rtw_disc_det_info *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_disconnect_det_info info = {0};

	if (en && cfg == NULL)
		return RTW_HAL_STATUS_FAILURE;

	_os_mem_set(hal_to_drvpriv(hal_info), &info, 0, sizeof(info));

	if (en) {
		info.disconnect_detect_en = cfg->disc_det_en;
		info.disconnect_en = (cfg->disc_wake_en == 1) ? 0 : 1;
		info.check_period = cfg->check_period;
		info.try_pkt_count = cfg->try_pkt_count;
		info.tryok_bcnfail_count_en = cfg->cnt_bcn_lost_en;
		info.tryok_bcnfail_count_limit = cfg->cnt_bcn_lost_limit;
	}

	if (hal_mac_ops->cfg_disconnect_det(mac, (u8)macid, &info))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_cfg_dev2hst_gpio(struct hal_info_t *hal_info, u8 en, struct rtw_wow_gpio_info *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_dev2hst_gpio_info info = {0};

	if (en && cfg == NULL)
		return RTW_HAL_STATUS_FAILURE;

	_os_mem_set(hal_to_drvpriv(hal_info), &info, 0, sizeof(info));

	if (en) {
		info.dev2hst_gpio_en = cfg->dev2hst_gpio_en;
		info.disable_inband = cfg->disable_inband;
		info.gpio_output_input = cfg->gpio_output_input;
		info.gpio_active = cfg->gpio_active;
		info.toggle_pulse = cfg->toggle_pulse;
		info.data_pin_wakeup = cfg->data_pin_wakeup;
		info.gpio_pulse_nonstop = cfg->gpio_pulse_nonstop;
		info.gpio_time_unit = cfg->gpio_time_unit;
		info.gpio_num = cfg->dev2hst_gpio;
		info.gpio_pulse_dura = cfg->gpio_pulse_dura;
		info.gpio_pulse_period = cfg->gpio_pulse_period;
		info.gpio_pulse_count = cfg->gpio_pulse_count;
		info.customer_id = cfg->customer_id;
		info.gpio_pulse_en_a = cfg->gpio_pulse_en_a;
		info.gpio_duration_unit_a = cfg->gpio_duration_unit_a;
		info.gpio_pulse_nonstop_a = cfg->gpio_pulse_nonstop_a;
		info.special_reason_a = cfg->special_reason_a;
		info.gpio_duration_a = cfg->gpio_duration_a;
		info.gpio_pulse_count_a = cfg->gpio_pulse_count_a;
	}

	if (hal_mac_ops->cfg_dev2hst_gpio(mac, &info))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_cfg_wow_wake(struct hal_info_t *hal_info, u16 macid, u8 en, struct rtw_wow_wake_info *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_wow_wake_info info = {0};
	struct mac_ax_remotectrl_info_parm_ *content = NULL;
	struct mac_ax_remotectrl_info_parm_ param;

	if (en && cfg == NULL)
		return RTW_HAL_STATUS_FAILURE;

	_os_mem_set(hal_to_drvpriv(hal_info), &info, 0, sizeof(info));

	if (en) {
		info.wow_en = cfg->wow_en;
		info.drop_all_pkt = cfg->drop_all_pkt;
		info.rx_parse_after_wake = cfg->rx_parse_after_wake;
		info.pairwise_sec_algo = cfg->pairwise_sec_algo;
		info.group_sec_algo = cfg->group_sec_algo;
		/*
		info.bip_sec_algo = cfg->bip_sec_algo;
		*/
		info.pattern_match_en = cfg->pattern_match_en;
		info.magic_en = cfg->magic_pkt_en;
		info.hw_unicast_en = cfg->hw_unicast_en;
		info.fw_unicast_en = cfg->fw_unicast_en;
		info.deauth_wakeup = cfg->deauth_wakeup;
		info.rekey_wakeup = cfg->rekey_wakeup;
		info.eap_wakeup = cfg->eap_wakeup;
		info.all_data_wakeup = cfg->all_data_wakeup;

		if (cfg->pairwise_sec_algo) {
			param.validcheck = cfg->remote_wake_ctrl_info.valid_check;
			param.symbolchecken = cfg->remote_wake_ctrl_info.symbol_check_en;
			param.lastkeyid = cfg->remote_wake_ctrl_info.gtk_key_idx;
			_os_mem_cpy(hal_to_drvpriv(hal_info), param.ptktxiv, cfg->remote_wake_ctrl_info.ptk_tx_iv, IV_LENGTH);
			_os_mem_cpy(hal_to_drvpriv(hal_info), param.rxptkiv, cfg->remote_wake_ctrl_info.ptk_rx_iv, IV_LENGTH);
			_os_mem_cpy(hal_to_drvpriv(hal_info), param.rxgtkiv_0, cfg->remote_wake_ctrl_info.gtk_rx_iv_idx0, IV_LENGTH);
			_os_mem_cpy(hal_to_drvpriv(hal_info), param.rxgtkiv_1, cfg->remote_wake_ctrl_info.gtk_rx_iv_idx1, IV_LENGTH);
			_os_mem_cpy(hal_to_drvpriv(hal_info), param.rxgtkiv_2, cfg->remote_wake_ctrl_info.gtk_rx_iv_idx2, IV_LENGTH);
			_os_mem_cpy(hal_to_drvpriv(hal_info), param.rxgtkiv_3, cfg->remote_wake_ctrl_info.gtk_rx_iv_idx3, IV_LENGTH);
			content = &param;
		}
	}

	/* should pass cfg->remote_wake_ctrl_info to halmac */
	if (hal_mac_ops->cfg_wow_wake(mac, (u8)macid, &info, content))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_wake_rsn(struct hal_info_t *hal_info, u8 *wake_rsn, u8 *reset)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;

	if (hal_mac_ops->get_wow_wake_rsn(mac, wake_rsn, reset) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_cfg_ndp_ofld(struct hal_info_t *hal_info, u16 macid, u8 en, struct rtw_ndp_ofld_info *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_ndp_ofld_info info = {0};
	struct mac_ax_ndp_info_parm_ content[2];
	void* drv_priv = hal_to_drvpriv(hal_info);
	u8 idx = 0;

	if (en && cfg == NULL)
		return RTW_HAL_STATUS_FAILURE;

	_os_mem_set(drv_priv, &info, 0, sizeof(struct mac_ax_ndp_ofld_info));
	_os_mem_set(drv_priv, content, 0, sizeof(struct mac_ax_ndp_info_parm_)*2);

	if (en) {
		info.ndp_en = cfg->ndp_en;
		info.na_id = cfg->ndp_id;
		for (idx = 0; idx < 2; idx++) {
			content[idx].enable = cfg->ndp_ofld_content[idx].ndp_en;
			_os_mem_cpy(drv_priv,content[idx].targetlinkaddress,
				cfg->ndp_ofld_content[idx].mac_addr, MAC_ADDRESS_LENGTH);
			content[idx].checkremoveip = cfg->ndp_ofld_content[idx].chk_remote_ip;
			_os_mem_cpy(drv_priv,content[idx].remoteipv6address,
				cfg->ndp_ofld_content[idx].remote_ipv6_addr, IPV6_ADDRESS_LENGTH);
			content[idx].numberoftargetip = cfg->ndp_ofld_content[0].num_target_ip;
			_os_mem_cpy(drv_priv,&(content[idx].targetip[0][0]),
				&(cfg->ndp_ofld_content[idx].target_ipv6_addr[0][0]), IPV6_ADDRESS_LENGTH*2);
		}
	}

	if (hal_mac_ops->cfg_ndp_ofld(mac, (u8)macid, &info, content))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_cfg_arp_ofld(struct hal_info_t *hal_info, u16 macid, u8 en,
								struct rtw_arp_ofld_info *cfg)
{

	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_arp_ofld_info info = {0};

	if (en && cfg == NULL)
		return RTW_HAL_STATUS_FAILURE;

	if (en) {
		info.arp_en = cfg->arp_en;
		info.arp_rsp_id = cfg->arp_rsp_id;
		info.arp_action = cfg->arp_action;
	}

	if (hal_mac_ops->cfg_arp_ofld(mac, (u8)macid, &info, NULL))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_cfg_wow_cam(struct hal_info_t *hal_info, u16 macid, u8 en,
							struct rtw_pattern_match_info *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct rtw_wowcam_upd_info *wowcam_info = NULL;
	struct mac_ax_wowcam_upd_info info;
	void* drv_priv = hal_to_drvpriv(hal_info);
	u8 i = 0;
	u8 j = 0;

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "%s (en: %u) ==>\n", __func__, en);

	if (en && cfg == NULL) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s() <== RTW_HAL_STATUS_FAILURE for input cfg == NULL\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}

	/* config each wow cam, no matter valid or not */
	for (i = 0; i < MAX_WOW_CAM_NUM; ++i) {
		_os_mem_set(drv_priv, &info, 0, sizeof(struct mac_ax_wowcam_upd_info));

		if (en) {
			wowcam_info = &(cfg->wowcam_info[i]);

			info.idx = wowcam_info->wow_cam_idx;
			info.r_w = wowcam_info->rw;
			info.valid = wowcam_info->valid;

			if (wowcam_info->valid != 0) {
				info.wkfm1 = wowcam_info->wake_mask[0];
				info.wkfm2 = wowcam_info->wake_mask[1];
				info.wkfm3 = wowcam_info->wake_mask[2];
				info.wkfm4 = wowcam_info->wake_mask[3];
				info.crc = wowcam_info->match_crc;
				info.negative_pattern_match = wowcam_info->is_negative_pattern_match;
				info.skip_mac_hdr = wowcam_info->skip_mac_hdr;
				info.uc = wowcam_info->uc;
				info.mc = wowcam_info->mc;
				info.bc = wowcam_info->bc;

				PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "wow_cam [%u]:\n", info.idx);

				for (j = 0; j < 4; ++j)
					PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "- wake_mask[%u] = 0x%08x\n", j, wowcam_info->wake_mask[j]);
				PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-      rw, crc = (%u, 0x%08x)\n", info.r_w, info.crc);
				PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "- Neg_ptn_mtch = %u\n", info.negative_pattern_match);
				PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-   SkipMacHdr = %u\n", info.skip_mac_hdr);
				PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "- (UC, MC, BC) = (%u, %u, %u)\n", info.uc, info.mc, info.bc);
			} else {
				continue;
			}
		} else {
			info.idx = i;
			info.r_w = 1;
			info.valid = 0;
		}

		if (hal_mac_ops->cfg_wowcam_upd(mac, &info)) {
			PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s() <== RTW_HAL_STATUS_FAILURE for cfg wowcam(%u)\n", __func__, info.idx);
			return RTW_HAL_STATUS_FAILURE;
		}
	}

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "%s() <==\n", __func__);

	return RTW_HAL_STATUS_SUCCESS;
}

static u32 _hal_mac_recv_aoac_report(struct mac_ax_adapter *mac, struct mac_ax_aoac_report *buf, u8 rx_rdy)
{
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	u32 mac_status = 0;

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "%s(): request aoac report.\n", __func__);

	mac_status = hal_mac_ops->request_aoac_report(mac, rx_rdy);

	if (mac_status) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s() <== mac_status(%u) from request_aoac_report()\n", __func__, mac_status);
		return mac_status;
	}

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "%s(): read aoac report\n", __func__);

	mac_status = hal_mac_ops->read_aoac_report(mac, buf, rx_rdy);

	if (mac_status) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s() <== get mac_status(%u) from read_aoac_report()\n", __func__, mac_status);
		return mac_status;
	}

	return mac_status;
}

#define KEY_ID_MASK 0x3
#define KEY_ID_OFFSET 6

enum rtw_hal_status _hal_mac_aoac_rpt_chk(struct rtw_aoac_report *aoac_info)
{
	u8 key_id_from_iv = 0;
	u32 rx_iv = *((u32 *)aoac_info->ptk_rx_iv);

	/* Case I. Aoac report is all zero in phase 0 */
	if (rx_iv == 0) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s(): ptk_rx_iv is Zero, treating this case as error.\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}

	/* Case II. KEY_IDX does not match with GTK_RX_IV */
	key_id_from_iv = (aoac_info->gtk_rx_iv[aoac_info->key_idx][3] >> KEY_ID_OFFSET) & KEY_ID_MASK;

	if (key_id_from_iv != aoac_info->key_idx) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s(): Key_idx(%u) not match with the one(%u) parsed from GTK_RX_IV[%u]\n",
						__func__, aoac_info->key_idx, key_id_from_iv, aoac_info->key_idx);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

/*
AOAC_RPT - PHASE 0:
	Halmac will get aoac report through c2h reg.

	(Cuz at this moment, the rx is still blocked by host.
	The rx cannot be resume only if those IV be updated by aoac_rpt)

	In PHASE 0, Fw will transfer some necessary info,
	such as RX_IV, GTK_KEY_ID, GTK_RX_IV, Rekey_OK and iGTK_ipn.
*/

enum rtw_hal_status
_hal_mac_read_aoac_rpt_phase_0(void* drv_priv, struct mac_ax_aoac_report *aoac_rpt_buf, struct rtw_aoac_report *aoac_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	aoac_info->rekey_ok = aoac_rpt_buf->rekey_ok;
	aoac_info->key_idx = aoac_rpt_buf->key_idx;
	_os_mem_cpy(drv_priv, aoac_info->ptk_rx_iv, aoac_rpt_buf->ptk_rx_iv, IV_LENGTH);

	switch (aoac_info->key_idx) {
	case 0:
		_os_mem_cpy(drv_priv, aoac_info->gtk_rx_iv[0], aoac_rpt_buf->gtk_rx_iv_0, IV_LENGTH);
		break;
	case 1:
		_os_mem_cpy(drv_priv, aoac_info->gtk_rx_iv[1], aoac_rpt_buf->gtk_rx_iv_1, IV_LENGTH);
		break;
	case 2:
		_os_mem_cpy(drv_priv, aoac_info->gtk_rx_iv[2], aoac_rpt_buf->gtk_rx_iv_2, IV_LENGTH);
		break;
	case 3:
		_os_mem_cpy(drv_priv, aoac_info->gtk_rx_iv[3], aoac_rpt_buf->gtk_rx_iv_3, IV_LENGTH);
		break;
	default:
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s(): Unknown gtk_key_idx(%u)\n", __func__, aoac_info->key_idx);
		break;
	}

	if (aoac_info->rekey_ok)
		_os_mem_cpy(drv_priv, aoac_info->igtk_ipn, aoac_rpt_buf->igtk_ipn, sizeof(aoac_rpt_buf->igtk_ipn));

	hal_status = _hal_mac_aoac_rpt_chk(aoac_info);

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "PHASE 0:\n");

	debug_dump_data((u8 *)aoac_rpt_buf, sizeof(struct mac_ax_aoac_report), "aoac_report");

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-     rekey_ok = %u\n", aoac_info->rekey_ok);
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-      key_idx = %u\n", aoac_info->key_idx);
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-    ptk_rx_iv = 0x%08x%08x\n", *((u32*)(aoac_info->ptk_rx_iv)+1), *((u32*)(aoac_info->ptk_rx_iv)));
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "- gtk_rx_iv[%u] = 0x%08x%08x\n",
					aoac_info->key_idx,
					*((u32*)(aoac_info->gtk_rx_iv[aoac_info->key_idx])+1),
					*((u32*)(aoac_info->gtk_rx_iv[aoac_info->key_idx])));

	if (aoac_info->rekey_ok)
		PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-     igtk_ipn = 0x%08x%08x\n", *((u32*)(aoac_info->igtk_ipn)+1), *((u32*)(aoac_info->igtk_ipn)));

	return hal_status;
}

/*
AOAC_RPT - PHASE 1:
	Halmac will get aoac report through c2h pkt.

	(Cuz at this moment,
	the host has resumed the rx, c2h pkt can be used here.)

	In PHASE 1, Fw will transfer all info in aoac report.
	Those entries got in phase 0 should remain the same value in phase 1.
*/

enum rtw_hal_status
_hal_mac_read_aoac_rpt_phase_1(void* drv_priv, struct mac_ax_aoac_report *aoac_rpt_buf, struct rtw_aoac_report *aoac_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	u8 i = 0;

	aoac_info->rpt_ver = aoac_rpt_buf->rpt_ver;
	aoac_info->sec_type = aoac_rpt_buf->sec_type;
	aoac_info->pattern_idx = aoac_rpt_buf->pattern_idx;

	_os_mem_cpy(drv_priv, aoac_info->ptk_tx_iv, aoac_rpt_buf->ptk_tx_iv, IV_LENGTH);

	for (i = 0; i < 4; ++i) {
		switch (i) {
		case 0:
			_os_mem_cpy(drv_priv, aoac_info->gtk_rx_iv[0], aoac_rpt_buf->gtk_rx_iv_0, IV_LENGTH);
			break;
		case 1:
			_os_mem_cpy(drv_priv, aoac_info->gtk_rx_iv[1], aoac_rpt_buf->gtk_rx_iv_1, IV_LENGTH);
			break;
		case 2:
			_os_mem_cpy(drv_priv, aoac_info->gtk_rx_iv[2], aoac_rpt_buf->gtk_rx_iv_2, IV_LENGTH);
			break;
		case 3:
			_os_mem_cpy(drv_priv, aoac_info->gtk_rx_iv[3], aoac_rpt_buf->gtk_rx_iv_3, IV_LENGTH);
			break;
		default:
			PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s(): Unknown gtk_key_idx(%u)\n", __func__, aoac_info->key_idx);
			break;
		}
	}

	_os_mem_cpy(drv_priv, aoac_info->gtk, aoac_rpt_buf->gtk, sizeof(aoac_rpt_buf->gtk));
	_os_mem_cpy(drv_priv, aoac_info->eapol_key_replay_count, aoac_rpt_buf->eapol_key_replay_count, sizeof(aoac_rpt_buf->eapol_key_replay_count));
	_os_mem_cpy(drv_priv, aoac_info->igtk_key_id, aoac_rpt_buf->igtk_key_id, sizeof(aoac_rpt_buf->igtk_key_id));
	_os_mem_cpy(drv_priv, aoac_info->igtk, aoac_rpt_buf->igtk, sizeof(aoac_rpt_buf->igtk));

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "PHASE 1:\n");

	debug_dump_data((u8 *)aoac_rpt_buf, sizeof(struct mac_ax_aoac_report), "aoac_report");

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-             rekey_ok = %u\n", aoac_info->rekey_ok);
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-              key_idx = %u\n", aoac_info->key_idx);
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-            ptk_rx_iv = 0x%08x%08x\n", *((u32*)(aoac_info->ptk_rx_iv)+1), *((u32*)(aoac_info->ptk_rx_iv)));
	for(i = 0; i < 4; ++i) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-         gtk_rx_iv[%u] = 0x%08x%08x\n",
					i,
					*((u32*)(aoac_info->gtk_rx_iv[i])+1),
					*((u32*)(aoac_info->gtk_rx_iv[i])));
	}
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-            ptk_tx_iv = 0x%08x%08x\n", *((u32*)(aoac_info->ptk_tx_iv)+1), *((u32*)(aoac_info->ptk_tx_iv)));
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-              rpt_ver = %u\n", aoac_info->rpt_ver);
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-             sec_type = %u\n", aoac_info->sec_type);
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-          pattern_idx = %u\n", aoac_info->pattern_idx);
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "- eapol_key_replay_cnt = 0x%08x%08x\n", *((u32*)(aoac_info->eapol_key_replay_count)+1), *((u32*)(aoac_info->eapol_key_replay_count)));
	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "-          igtk_key_id = 0x%08x%08x\n", *((u32*)(aoac_info->igtk_key_id)+1), *((u32*)(aoac_info->igtk_key_id)));
	debug_dump_data(aoac_info->gtk, sizeof(aoac_rpt_buf->gtk), "GTK:");
	debug_dump_data(aoac_info->igtk, sizeof(aoac_rpt_buf->igtk), "iGTK:");

	return hal_status;
}

enum rtw_hal_status
rtw_hal_mac_get_aoac_rpt(struct hal_info_t *hal_info, struct rtw_aoac_report *aoac_info, u8 rx_ready)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
#ifndef RTW_WKARD_WOW_SKIP_AOAC_RPT
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
#endif
	struct mac_ax_aoac_report aoac_rpt_buf;
	void* drv_priv = hal_to_drvpriv(hal_info);
	u32 mac_status = 0;

	if (aoac_info == NULL) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s() <== RTW_HAL_STATUS_FAILURE for input info == NULL\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}

#ifdef RTW_WKARD_WOW_SKIP_AOAC_RPT
	mac_status = MACPROCERR;
#else
	mac_status = _hal_mac_recv_aoac_report(mac, &aoac_rpt_buf, rx_ready);
#endif

	if (mac_status) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s() <== RTW_HAL_STATUS_FAILURE for _hal_mac_recv_aoac_report fail with mac_status(%u).\n",
					__func__, mac_status);
		hal_status = RTW_HAL_STATUS_FAILURE;
	} else {
		if (rx_ready == 0)
			hal_status = _hal_mac_read_aoac_rpt_phase_0(drv_priv, &aoac_rpt_buf, aoac_info);
		else
			hal_status = _hal_mac_read_aoac_rpt_phase_1(drv_priv, &aoac_rpt_buf, aoac_info);
	}

	return hal_status;
}

enum rtw_hal_status
rtw_hal_mac_cfg_gtk_ofld(struct hal_info_t *hal_info, u16 macid, u8 en,
								struct rtw_gtk_ofld_info *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_gtk_ofld_info  info;
	struct mac_ax_gtk_info_parm_ param;
	void* drv_priv = hal_to_drvpriv(hal_info);

	_os_mem_set(drv_priv, &info, 0, sizeof(struct mac_ax_gtk_ofld_info));
	_os_mem_set(drv_priv, &param, 0, sizeof(struct mac_ax_gtk_info_parm_));

	if (en && cfg == NULL) {
		PHL_TRACE(COMP_PHL_WOW, _PHL_WARNING_, "%s() <== RTW_HAL_STATUS_FAILURE for input cfg == NULL\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}

	if (en) {
		info.gtk_en = cfg->gtk_en;
		info.tkip_en = cfg->tkip_en;
		info.ieee80211w_en = cfg->ieee80211w_en;
		info.pairwise_wakeup = cfg->pairwise_wakeup;
		info.gtk_rsp_id = cfg->gtk_rsp_id;

		info.algo_akm_suit = cfg->akmtype_byte3;

		if (info.gtk_en) {
			_os_mem_cpy(drv_priv, param.kck, cfg->gtk_ofld_content.kck, cfg->gtk_ofld_content.kck_len);
			_os_mem_cpy(drv_priv, param.kek, cfg->gtk_ofld_content.kek, cfg->gtk_ofld_content.kek_len);

			if (info.tkip_en)
				_os_mem_cpy(drv_priv, param.rxmickey, cfg->gtk_ofld_content.rxmickey, TKIP_MIC_KEY_LENGTH);
		}

		if (info.ieee80211w_en) {
			info.bip_sec_algo = cfg->bip_sec_algo;
			info.pmf_sa_query_id = cfg->sa_query_id;

			_os_mem_cpy(drv_priv, param.igtk_keyid, cfg->gtk_ofld_content.igtk_keyid, cfg->gtk_ofld_content.igtk_len);
			_os_mem_cpy(drv_priv, param.ipn, cfg->gtk_ofld_content.ipn, IGTK_PKT_NUM_LENGTH);
			_os_mem_cpy(drv_priv, param.igtk, &(cfg->gtk_ofld_content.igtk[0]), cfg->gtk_ofld_content.igtk_len);

			if (cfg->hw_11w_en == 0)
				_os_mem_cpy(drv_priv, param.sk, cfg->gtk_ofld_content.psk, cfg->gtk_ofld_content.psk_len);
		}
	} else {
		info.gtk_en = false;
		info.ieee80211w_en = false;
	}

	if (hal_mac_ops->cfg_gtk_ofld(mac, (u8)macid, &info, &param))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;

}

enum rtw_hal_status
rtw_hal_mac_cfg_realwow(struct hal_info_t *hal_info, u16 macid, u8 en,
								struct rtw_realwow_info *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_realwow_info info = {0};
	struct mac_ax_realwowv2_info_parm_ param = {0};
	void *drv_priv = hal_to_drvpriv(hal_info);

	_os_mem_set(drv_priv, &info, 0, sizeof(struct mac_ax_realwow_info));
	_os_mem_set(drv_priv, &param, 0, sizeof(struct mac_ax_realwowv2_info_parm_));

	if (en) {
		info.realwow_en = cfg->realwow_en;
		info.auto_wakeup = cfg->auto_wakeup;
		info.keepalive_id = cfg->keepalive_id;
		info.wakeup_pattern_id = cfg->wakeup_pattern_id;
		info.ack_pattern_id = cfg->ack_pattern_id;
		if (info.realwow_en) {
			param.interval = cfg->realwow_ofld_content.interval;
			param.kapktsize = cfg->realwow_ofld_content.keep_alive_pkt_size;
			param.acklostlimit = cfg->realwow_ofld_content.ack_lost_limit;
			param.ackpatternsize = cfg->realwow_ofld_content.ack_ptrn_size;
			param.wakeuppatternsize = cfg->realwow_ofld_content.wakeup_ptrn_size;
			param.wakeupsecnum = cfg->realwow_ofld_content.wakeup_sec_num;
		}
	} else {
		info.realwow_en = false;
	}

	if (hal_mac_ops->cfg_realwow(mac, (u8)macid, &info, &param))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_set_wowlan(struct hal_info_t *hal, u8 enter)
{
	u32 mac_err = 0;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	enum mac_ax_wow_ctrl ctrl = (enter == 1) ? MAC_AX_WOW_ENTER : MAC_AX_WOW_LEAVE;

	mac_err = mac->ops->intf_ops->set_wowlan(mac, ctrl);
	if (mac_err != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

static bool _wow_is_txq_empty(struct mac_ax_tx_queue_empty *val)
{
	u8 i = 0;
	u8 size = ARRAY_SIZE(val->macid_txq_empty);

	if (!val->others_empty)
		return false;

	if (!val->band0_mgnt_empty)
		return false;

	if (!val->band1_mgnt_empty)
		return false;

	for (i = 0; i < size; i++) {
		if (val->macid_txq_empty[i] != 0xFF)
			return false;
	}

	PHL_INFO("%s : others_empty %d.\n", __func__, val->others_empty);
	PHL_INFO("%s : band0_mgnt_empty %d.\n", __func__, val->band0_mgnt_empty);
	PHL_INFO("%s : band1_mgnt_empty %d.\n", __func__, val->band1_mgnt_empty);
	for (i = 0; i < size; i++)
		PHL_INFO("%s : macid_txq_empty[%d] %d.\n", __func__, i, val->macid_txq_empty[i]);

	return true;
}

#define MAX_WOW_POLLNG_TXQ_EMPTY_TIME 50000 /* us */
#define MAX_WOW_CHK_TXQ_EMPTY_CNT 2 /* continously check ok should satisfied this value */
enum rtw_hal_status rtw_hal_mac_wow_chk_txq_empty(struct hal_info_t *hal, u8 *empty)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_tx_queue_empty val = {0};
	u32 start_t = _os_get_cur_time_us();
	u8 chk_cnt = 0;

	while (1) {

		if (phl_get_passing_time_us(start_t) >= MAX_WOW_POLLNG_TXQ_EMPTY_TIME) {
			PHL_ERR("%s : reach maximum polling time.\n", __func__);
			break;
		}

		for (chk_cnt = 0; chk_cnt < MAX_WOW_CHK_TXQ_EMPTY_CNT; chk_cnt++) {
			if (mac->ops->is_txq_empty(mac, &val) != MACSUCCESS)
				break;

			if (!_wow_is_txq_empty(&val)) {
				break;
			} else {
				*empty = 1;
			}
		}

		if (*empty)
			break;

		_os_delay_us(hal_to_drvpriv(hal), 50);
	}

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "%s : polling empty %d with duration %d.\n",
			  __func__, *empty, phl_get_passing_time_us(start_t));

	return hstatus;
}

enum rtw_hal_status rtw_hal_mac_wow_wde_drop(struct hal_info_t *hal, u8 band)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_pkt_drop_info info = {0};
	u32 mac_err = 0;

	PHL_TRACE(COMP_PHL_WOW, _PHL_INFO_, "%s with band %d.\n", __func__, band);

	info.sel = MAC_AX_PKT_DROP_SEL_BAND_ONCE;
	info.band = band;

	mac_err = mac->ops->pkt_drop(mac, &info);
	if (mac_err != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

#endif /* CONFIG_WOWLAN */

static enum rtw_hal_status
hal_mac_read_efuse(struct mac_ax_adapter *mac, u32 addr, u32 size,
						u8 *val, enum mac_ax_efuse_bank bank)
{
	if (mac->ops->read_efuse(mac, addr, size, val, bank) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

static enum rtw_hal_status
hal_mac_write_efuse(struct mac_ax_adapter *mac, u32 addr, u8 val,
						enum mac_ax_efuse_bank bank)
{
	if (mac->ops->write_efuse(mac, addr, val, bank) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_enable_cpu(struct hal_info_t *hal_info, u8 reason, u8 dlfw)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->enable_cpu(mac, reason, dlfw) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_disable_cpu(struct hal_info_t *hal_info)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->disable_cpu(mac) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_romdl(struct hal_info_t *hal_info, u8 *rom_buf, u32 rom_size)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 rom_addr = 0x18900000;

	if (mac->ops->romdl(mac, rom_buf, rom_addr, rom_size) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_fwdl(struct hal_info_t *hal_info, u8 *fw_buf, u32 fw_size)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 mac_err;

	mac_err = mac->ops->fwdl(mac, fw_buf, fw_size);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	hal_mac_print_fw_version(hal_info);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_enable_fw(struct hal_info_t *hal_info, u8 fw_type)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 mac_err;

	mac_err = mac->ops->enable_fw(mac, fw_type);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	hal_mac_print_fw_version(hal_info);
	return RTW_HAL_STATUS_SUCCESS;
}


/*   */
/**
 * rtw_hal_mac_ax_fill_txdesc
 * @mac: see struct mac_ax_adapter
 * @treq: the xmit request for this tx descriptor
 * @wd_buf: the wd buffer to fill
 * @wd_len: output, return the total length of filled wd
 *
 * Note,halmac API for hal and proto type is at hal_api_mac.h
 */
enum rtw_hal_status
rtw_hal_mac_ax_fill_txdesc(void *mac, struct rtw_xmit_req *treq,
				u8 *wd_buf, u32 *wd_len)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;

	*wd_len = mac_info->ops->txdesc_len(
		mac_info,
		&treq->mdata);

	hal_status = mac_info->ops->build_txdesc(
		mac_info,
		&treq->mdata, wd_buf, *wd_len);


	return hal_status;
}

/**
 * rtw_hal_mac_set_hw_ampdu_cfg
 * @hal_info: see struct hal_info_t
 * @band: target band this AMPDU going to send
 * @max_agg_num: AMPDU maximum aggregation number
 * @max_agg_time: AMPDU maximum aggregation time, in unit of 32 us
 *
 * Note,
 * (1) halmac API for hal and proto type is at hal_api_mac.h
 */
enum rtw_hal_status
rtw_hal_mac_set_hw_ampdu_cfg(struct hal_info_t *hal_info,
                             u8 band,
                             u16 max_agg_num, u8 max_agg_time)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_ampdu_cfg info;
	u32 mac_err;

	_os_mem_set(hal_to_drvpriv(hal_info), &info, 0, sizeof(info));

	info.band = band;
	info.wdbk_mode = MAC_AX_WDBK_MODE_SINGLE_BK;
	info.rty_bk_mode = MAC_AX_RTY_BK_MODE_AGG;
	info.max_agg_num = max_agg_num;
	info.max_agg_time_32us = max_agg_time;

	mac_err = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_AMPDU_CFG, &info);

	if (mac_err != MACSUCCESS)
		goto fail;

	return RTW_HAL_STATUS_SUCCESS;
fail:
	return RTW_HAL_STATUS_MAC_API_FAILURE;
}


/**
 * the function to update DMAC control info by halmac api
 * @hal_info: see struct hal_info_t
 * @dctl_info: structure of dmac control information, define by halmac
 * @macid: the macid corresponding to this cmac control info
 *
 * return RTW_HAL_STATUS_MAC_API_FAILURE if update fail
 */
enum rtw_hal_status rtw_hal_dmc_tbl_cfg(struct hal_info_t *hal_info,
                                        struct mac_ax_dctl_info *dctl_info,
                                        struct mac_ax_dctl_info *dctl_info_mask,
                                        u16 macid)
{
	enum rtw_hal_status sts = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 ret = 0;

	ret = mac->ops->upd_dctl_info(mac, dctl_info, dctl_info_mask, (u8)macid, 1);

	if (0 == ret) {
		sts = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MAC, _PHL_WARNING_, "mac_upd_dctl_info fail (0x%08X)\n",
			  ret);
		sts = RTW_HAL_STATUS_MAC_API_FAILURE;
	}

	return sts;
}



/**
 * the function to update CMAC control info by halmac api
 * @hal_info: see struct hal_info_t
 * @cctl_info: structure of cmac control information, define by halmac
 * @macid: the macid corresponding to this cmac control info
 *
 * return RTW_HAL_STATUS_MAC_API_FAILURE if update fail
 */
enum rtw_hal_status rtw_hal_cmc_tbl_cfg(struct hal_info_t *hal_info,
					struct mac_ax_cctl_info *cctl_info,
					struct mac_ax_cctl_info *cctl_info_mask,
					u16 macid)
{
	enum rtw_hal_status sts = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 ret = 0;
	cctl_info_mask->addr_cam_index = 0;

	ret = mac->ops->upd_cctl_info(mac, cctl_info, cctl_info_mask, (u8)macid, 1);

	if (0 == ret) {
		sts = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MAC, _PHL_WARNING_, "mac_upd_cctl_info fail (0x%08X)\n",
			  ret);
		sts = RTW_HAL_STATUS_MAC_API_FAILURE;
	}

	return sts;
}


/**
 * the function to update BA CAM entry by halmac api
 * @hal_info: see struct hal_info_t
 * @ba_cam: structure of ba cam entry, define by halmac
 *
 * return RTW_HAL_STATUS_MAC_API_FAILURE if update fail
 */
enum rtw_hal_status rtw_hal_bacam_cfg(struct hal_info_t *hal_info,
				      struct mac_ax_bacam_info *ba_cam)
{
	enum rtw_hal_status sts = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 ret = 0;

	ret = mac->ops->bacam_info(mac, ba_cam);

	if (0 == ret) {
		sts = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MAC, _PHL_WARNING_, "mac_bacam_info fail (0x%08X)\n",
			  ret);
		sts = RTW_HAL_STATUS_MAC_API_FAILURE;
	}

	return sts;
}

/**
 * rtw_hal_mac_set_bw() - Update channel and bandwdith related setting
 * @hal_info:	struct hal_info_t*
 * @band_idx:	0x0: band0, 0x1: band1
 * @ch:		center channel
 * @band:	band
 * @bw:		bandwidth
 *
 * All channel and bandwidth related MAC setting would be done in
 * this function.
 * Following setting may be done in this functions:
 *	a. Enable changing CCK data rate to OFDM 6M function
 *	   to avoid BB/RF abnormal when channel is not 2.4G.
 *
 * Return RTW_HAL_STATUS_SUCCESS when operation success.
 */
enum rtw_hal_status rtw_hal_mac_set_bw(struct hal_info_t *hal_info,
			u8 band_idx, u8 pri_ch,	u8 central_ch_seg0,
			u8 central_ch_seg1,	enum band_type band,
			enum channel_width bw)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_cfg_bw mac_bw = {0};
	u32 ret = 0;

	mac_bw.cbw = bw;
	mac_bw.band = band_idx;
	mac_bw.pri_ch = pri_ch;
	mac_bw.central_ch = central_ch_seg0;

	ret = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_BW_CFG, &mac_bw);
	return (ret == 0) ? (RTW_HAL_STATUS_SUCCESS): (RTW_HAL_STATUS_FAILURE);
}

/**
 * rtw_hal_mac_ax_init_bf_role
 * @bf_role: 0 = BFEE, 1 = BFER
 * @band: 0 = BAND0, 1 = BAND1
 */
enum rtw_hal_status
rtw_hal_mac_ax_init_bf_role(struct rtw_hal_com_t *hal_com, u8 bf_role, u8 band)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac_info = hal_to_mac(hal_info);

	if (bf_role == HAL_BF_ROLE_BFEE) {
		hal_status = mac_info->ops->init_snd_mee(
				mac_info, band);
	} else {
		hal_status = mac_info->ops->init_snd_mer(
				mac_info, band);
	};
	return hal_status;
}


/**
 * rtw_hal_mac_ax_disable_bfee
 * @band: 0 = BAND0, 1 = BAND1
 */
enum rtw_hal_status
rtw_hal_mac_ax_deinit_bfee(struct rtw_hal_com_t *hal_com, u8 band)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "--> %s : Warning BFee is going to deinit\n", __func__);

	hal_status = mac_info->ops->deinit_mee(mac_info, band);

	return hal_status;
}


/**
 * rtw_hal_mac_ax_bfee_para_reg
 * 	Set BFee capability with STA info by method : Control Register
 * input:
 * @sta: (struct rtw_phl_stainfo_t *)
 */
enum rtw_hal_status
rtw_hal_mac_ax_bfee_para_reg(void *mac, struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	struct rtw_hal_com_t *hal_com = (struct rtw_hal_com_t *)mac_info->drv_adapter;
	struct mac_reg_csi_para csi_para;

	_os_mem_set(hal_com->drv_priv, &csi_para, 0, sizeof(csi_para));

	csi_para.band = sta->wrole->hw_band;
	csi_para.portsel = (sta->wrole->hw_port == 0) ? 0 : 1;
	csi_para.nc = (sta->wrole->proto_role_cap.max_nc > sta->asoc_cap.num_snd_dim) ?
		       sta->asoc_cap.num_snd_dim :
		       sta->wrole->proto_role_cap.max_nc;
	csi_para.nr = (sta->wrole->proto_role_cap.bfme_sts >
		       sta->asoc_cap.num_snd_dim) ?
		       sta->asoc_cap.num_snd_dim :
		       sta->wrole->proto_role_cap.bfme_sts;
	/**
	 * For HE/VHT, Ng = 0 can provide the most detail information.
	 * Ng do not care bfer cap.
	 **/
	csi_para.ng = 0;
	/**
	 * for HE/VHT, Cb = 1 {6,4}/{9,7} can provide the most detail information
	 * Cb do not care bfer cap, only care bfee self capabiltiy.
	 **/
	if (sta->wmode & WLAN_MD_11AX)
		csi_para.cb = sta->wrole->proto_role_cap.cb_sz_su_fb;
	else if (sta->wmode & WLAN_MD_11AC)
		csi_para.cb = sta->wrole->proto_role_cap.ht_vht_cb;
	else
		csi_para.cb = 0;

	csi_para.cs = 1; /* Carrier Sense */
	if (sta->asoc_cap.ht_ldpc &&
	    sta->asoc_cap.vht_ldpc &&
	    sta->asoc_cap.he_ldpc)
		csi_para.ldpc_en = 1;

	if (sta->asoc_cap.stbc_ht_rx &&
	    sta->asoc_cap.stbc_vht_rx &&
	    sta->asoc_cap.stbc_he_rx)
		csi_para.stbc_en = 1;

	csi_para.bf_en = 0;

	hal_status = mac_info->ops->set_csi_para_reg(mac_info, &csi_para);

	return hal_status;
}

/**
 * rtw_hal_mac_ax_bfee_para_cctl
 * 	Set BFee capability with STA info by method : CMAC Control Table
 * input:
 * @sta: (struct rtw_phl_stainfo_t *)
 */
enum rtw_hal_status
rtw_hal_mac_ax_bfee_para_cctl(void *mac, struct rtw_phl_stainfo_t *sta)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	struct rtw_hal_com_t *hal_com =
		(struct rtw_hal_com_t *)mac_info->drv_adapter;
	struct mac_cctl_csi_para csi_para;

	_os_mem_set(hal_com->drv_priv, &csi_para, 0, sizeof(csi_para));

	csi_para.macid = (u8)sta->macid;
	csi_para.band = sta->wrole->hw_band;
	csi_para.nc = (sta->wrole->proto_role_cap.max_nc > sta->asoc_cap.num_snd_dim) ?
		       sta->asoc_cap.num_snd_dim :
		       sta->wrole->proto_role_cap.max_nc;
	csi_para.nr = (sta->wrole->proto_role_cap.bfme_sts >
		       sta->asoc_cap.num_snd_dim) ?
		       sta->asoc_cap.num_snd_dim :
		       sta->wrole->proto_role_cap.bfme_sts;
	/**
	 * For HE/VHT, Ng = 0 can provide the most detail information.
	 * Ng do not care bfer cap.
	 **/
	csi_para.ng = 0;
	/**
	 * for HE/VHT, Cb = 1 {6,4}/{9,7} can provide the most detail information
	 * Cb do not care bfer cap.
	 **/
	if (sta->wmode & WLAN_MD_11AX)
		csi_para.cb = sta->wrole->proto_role_cap.cb_sz_su_fb;
	else if (sta->wmode & WLAN_MD_11AC)
		csi_para.cb = sta->wrole->proto_role_cap.ht_vht_cb;
	else
		csi_para.cb = 0;
	csi_para.cs = 1;
	csi_para.bf_en = 0;

	if (sta->asoc_cap.stbc_ht_rx &&
	    sta->asoc_cap.stbc_vht_rx &&
	    sta->asoc_cap.stbc_he_rx)
		csi_para.stbc_en = 1;

	if (sta->asoc_cap.ht_ldpc &&
	    sta->asoc_cap.vht_ldpc &&
	    sta->asoc_cap.he_ldpc)
		csi_para.ldpc_en = 1;
	csi_para.rate = MAC_AX_OFDM54;
	csi_para.gi_ltf = MAC_AX_SGI_4XHE08;
	csi_para.gid_sel = 1;
	csi_para.bw = MAC_AX_BW_20M;

	hal_status = mac_info->ops->set_csi_para_cctl(mac_info, &csi_para);
	return hal_status;
}

enum rtw_hal_status
rtw_hal_mac_ax_bfee_set_csi_rrsc(void *mac, u8 band, u32 rrsc)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;

	hal_status = mac_info->ops->csi_rrsc(mac_info, band, rrsc);

	return hal_status;
}

/**
 * rtw_hal_mac_ax_bfee_forced_csi_rate
 * 	set bf report frame rate
 * @mac:(struct mac_ax_adapter *)
 * @ht_rate:
 * @vht_rate:
 * @he_rate:
 */

enum rtw_hal_status
rtw_hal_mac_ax_bfee_forced_csi_rate(void *mac, struct rtw_phl_stainfo_t *sta,
	u8 ht_rate, u8 vht_rate, u8 he_rate)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;

	hal_status = mac_info->ops->csi_force_rate(mac_info,
		sta->wrole->hw_band, ht_rate, vht_rate, he_rate);

	return hal_status;
}

/**
 * rtw_hal_mac_ax_set_bf_entry
 * 	set HW BF entry for sounding and TxBF
 * input :
 * @band: BF Entry is band0 or band1;
 * @macid: BF Entry's macid
 * @bfee_idx: SU/MU HW Entry Index
 * @txbf_idx: Tx BF CSI Entry Index
 * @buffer_idx: CSI Buffer idx used by TxBF entry
 */
enum rtw_hal_status
rtw_hal_mac_ax_set_bf_entry(void *mac, u8 band,
		u8 macid, u8 bfee_idx, u16 txbf_idx, u16 buffer_idx)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	/* 1. CSI Buffer Idx */
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "set_csi_buffer_index : band 0x%x macid 0x%x txbf_idx 0x%x buffer_idx 0x%x\n",
				       band, macid, txbf_idx, buffer_idx);
	hal_status = mac_info->ops->set_csi_buffer_index(mac_info, band, macid,
							 txbf_idx, buffer_idx);


	/*TODO: this api might revised in the future */
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "set_snd_sts_index: band 0x%x macid 0x%x bf_idx 0x%x\n",
				band, macid, bfee_idx);
	hal_status = mac_info->ops->set_snd_sts_index(
					mac_info, band, macid, bfee_idx);


	return hal_status;
}


/**
 * rtw_hal_mac_ax_get_snd_sts
 * 	Get HW BF entry sounding status
 * input :
 * @band: BF Entry is band0 or band1;
 * @bfee_idx: SU/MU HW Entry Index
 * return
 * @hal_status: enum rtw_hal_status
 * 	RTW_HAL_STATUS_SUCCESS = Sounding Success
 * 	RTW_HAL_STATUS_FAILURE = Sounding Fail
 */
enum rtw_hal_status
rtw_hal_mac_ax_get_snd_sts(void *mac, u8 band, u8 bfee_idx)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	u32 sts = 0;

	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "get_snd_sts_index: band 0x%x bf_idx 0x%x\n", band, bfee_idx);
	/*TODO: This API shall modify to return sounding status instead of CR value*/
	/*MAC Define : #define B_AX_MER_SU_BFMEE0_SND_STS BIT(9)*/
	sts = mac_info->ops->get_snd_sts_index(mac_info, band, bfee_idx);
	if (sts & B_AX_MER_SU_BFMEE0_SND_STS)
		hal_status = RTW_HAL_STATUS_SUCCESS;

	return hal_status;
}

/**
 * rtw_hal_mac_ax_hw_snd_control
 * @band: 0 = BAND0, 1 = BAND1
 * @hw_snd_ctrl: 0 = HW_SND_PAUSE 1 = HW_SND_RELEASE
 */
enum rtw_hal_status
rtw_hal_mac_ax_hw_snd_control(
	void *mac,
	u8 band,
	u8 hw_snd_ctrl)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_,
		  "mac_hw_snd_pause_release: band 0x%x hw_snd_ctrl 0x%x\n",
		  band, hw_snd_ctrl);

	hal_status = mac_info->ops->hw_snd_pause_release(
				mac_info,
				band,
				hw_snd_ctrl);

	return hal_status;
}

/* Tx Frame Exchange Related : MU */
/**
 * rtw_hal_mac_ax_mu_sta_upd
 * @mac:  (struct mac_ax_adapter *)
 * @macid: sta macid for configuration
 * @bfmu_idx: 0~5, MU STA Index
 * @prot_type: RTS/CTS type for the group : enum rtw_hal_protection_type
 * @resp_type: Ack Policy for the group : enum rtw_hal_ack_resp_type
 * @grp_bitmap: group bitmap for STA,
 **/
enum rtw_hal_status
rtw_hal_mac_ax_mu_sta_upd(void *mac, u8 macid, u8 bfmu_idx,
			enum rtw_hal_protection_type prot_type,
			enum rtw_hal_ack_resp_type resp_type, u8 mugrp_bm)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	struct rtw_hal_com_t *hal_com =
		(struct rtw_hal_com_t *)mac_info->drv_adapter;
	struct mac_ax_mu_sta_upd sta_info;
	u8 i = 0;

	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "==> rtw_hal_mac_ax_mu_sta_upd \n");
	_os_mem_set(hal_com->drv_priv, &sta_info, 0, sizeof(sta_info));

	sta_info.macid = macid;
	sta_info.mu_idx = bfmu_idx;
	for (i = 0; i < 5; i++) {
		if (mugrp_bm & BIT(i)) {
			sta_info.prot_rsp_type[i].u.feld_type.protect =
				prot_type & 0xF;
			sta_info.prot_rsp_type[i].u.feld_type.rsp =
				resp_type & 0xF;
		} else {
			sta_info.prot_rsp_type[i].u.feld_type.protect = 0;
			sta_info.prot_rsp_type[i].u.feld_type.rsp = 0;
		}
	}
	sta_info.mugrp_bitmap = mugrp_bm & 0x1F;

	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "sta_info.macid = 0x%x \n", sta_info.macid);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "sta_info.mu_idx = 0x%x \n", sta_info.mu_idx);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "sta_info.mugrp_bitmap = 0x%x \n", sta_info.mugrp_bitmap);

	hal_status = mac_info->ops->mu_sta_upd(mac_info, &sta_info);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "<== rtw_hal_mac_ax_mu_sta_upd \n");
	return hal_status;
}

/**
 * rtw_hal_mac_ax_mu_decision_para
 * @mac:  (struct mac_ax_adapter *)
 * @mu_thold:  MU MIMO pkt Threshold
 * @bypass_thold: by pass mu_thold
 * @bypass_tp: by pass MU TP > SU TP check.
 **/
enum rtw_hal_status
rtw_hal_mac_ax_mu_decision_para(void *mac, u32 mu_thold,
				bool bypass_thold, bool bypass_tp)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	struct rtw_hal_com_t *hal_com =
		(struct rtw_hal_com_t *)mac_info->drv_adapter;
	struct mac_ax_mudecision_para mu_d_para;

	_os_mem_set(hal_com->drv_priv, &mu_d_para, 0, sizeof(mu_d_para));

	mu_d_para.mu_thold = mu_thold;
	mu_d_para.bypass_thold = bypass_thold ? 1 : 0;
	mu_d_para.bypass_tp = bypass_tp ? 1 : 0;

	hal_status = mac_info->ops->upd_mudecision_para(mac_info, &mu_d_para);

	return hal_status;
}

/**
 * rtw_hal_mac_ax_set_mu_fix_mode
 * @mac:  (struct mac_ax_adapter *)
 * @gid:  GID for STA X + STAY
 * @prot_type: RTS/CTS type for the group : enum rtw_hal_protection_type
 * @resp_type: Ack Policy for the group : enum rtw_hal_ack_resp_type
 * @fix_mu: true = Fix FW decision = MU
 * @he: true = Fix TX HE MU, false = Fix TX VHT MU;
 * @fix_resp: fix frame exchange ack policy
 * @fix_prot: fix frame exchange protection type
 **/
enum rtw_hal_status
rtw_hal_mac_ax_set_mu_fix_mode(
	void *mac, u8 gid, enum rtw_hal_protection_type prot_type,
	enum rtw_hal_ack_resp_type resp_type,
	bool fix_mu, bool he, bool fix_resp, bool fix_prot)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	struct rtw_hal_com_t *hal_com =
		(struct rtw_hal_com_t *)mac_info->drv_adapter;
	struct mac_ax_fixmode_para fix_info;
	PHL_INFO("===>rtw_hal_mac_ax_set_mu_fix_mode\n");

	_os_mem_set(hal_com->drv_priv, &fix_info, 0, sizeof(fix_info));

	fix_info.force_sumuru_en = fix_mu ? 1:0 ;
	fix_info.forcemu = fix_mu ? 1:0 ;

	if (fix_mu) {
		if (!he){
			fix_info.fix_fe_vhtmu_en = fix_prot ? 1 : 0;
			fix_info.fix_frame_seq_vhtmu = fix_resp ? 1 : 0;
			fix_info.prot_type_vhtmu = fix_prot ? prot_type : 5;/*default hw setting*/
			fix_info.resp_type_vhtmu = fix_resp ? resp_type : 4;/*default hw setting*/
			PHL_INFO("fix_info.prot_type_vhtmu = 0x%x\n", fix_info.prot_type_vhtmu);
			PHL_INFO("fix_info.resp_type_vhtmu = 0x%x\n", fix_info.resp_type_vhtmu);

		} else {
			fix_info.fix_fe_hemu_en = fix_prot ? 1 : 0;;
			fix_info.fix_frame_seq_hemu = fix_resp ? 1 : 0;
			fix_info.prot_type_hemu = fix_prot ? prot_type : 5;/*default hw setting*/
			fix_info.resp_type_hemu = fix_resp ? resp_type : 4;/*default hw setting*/
			PHL_INFO("fix_info.prot_type_hemu = 0x%x\n", fix_info.prot_type_hemu);
			PHL_INFO("fix_info.resp_type_hemu = 0x%x\n", fix_info.resp_type_hemu);
		}

		fix_info.mugrpid = gid;
		PHL_INFO("fix_info.mugrpid = 0x%x\n", fix_info.mugrpid);
	}

	hal_status = mac_info->ops->set_fw_fixmode(mac_info, &fix_info);
	PHL_INFO("<===rtw_hal_mac_ax_set_mu_fix_mode\n");
	return hal_status;
}


void
_hal_mac_fill_mu_sc_tbl_row(u32 *mac_score, void *hal_score)
{
	struct hal_mu_score_tbl_score *h_score =
		(struct hal_mu_score_tbl_score *)hal_score;

	*mac_score = (u32)h_score->score[0] |
		     ((u32)h_score->score[1] << 8) | ((u32)h_score->valid << 10) |
		     ((u32)h_score->macid << 11);

}
/**
 * rtw_hal_mac_ax_set_mu_table_whole
* @mac:  (struct mac_ax_adapter *)
 *@hal_score_tbl:  struct hal_mu_score_tbl *
 */
enum rtw_hal_status
rtw_hal_mac_ax_set_mu_table_whole(void *mac, void *hal_score_tbl)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac_info = (struct mac_ax_adapter *)mac;
	struct rtw_hal_com_t *hal_com =
		(struct rtw_hal_com_t *)mac_info->drv_adapter;
	struct mac_mu_table mu_table;
	struct hal_mu_score_tbl *score_tbl = (struct hal_mu_score_tbl *)hal_score_tbl;
	_os_mem_set(hal_com->drv_priv, &mu_table, 0, sizeof(mu_table));

	/*TODO: halmac api shall refine!!!*/
	mu_table.mu_score_tbl_ctrl = (score_tbl->mu_ctrl.mu_sc_thr) |
				     (score_tbl->mu_ctrl.mu_opt << 2);

	/*TODO: if next IC has more than 6 MU STAs!!! */
	_hal_mac_fill_mu_sc_tbl_row(&mu_table.mu_score_tbl_0, &score_tbl->mu_score[0]);
	_hal_mac_fill_mu_sc_tbl_row(&mu_table.mu_score_tbl_1, &score_tbl->mu_score[1]);
	_hal_mac_fill_mu_sc_tbl_row(&mu_table.mu_score_tbl_2, &score_tbl->mu_score[2]);
	_hal_mac_fill_mu_sc_tbl_row(&mu_table.mu_score_tbl_3, &score_tbl->mu_score[3]);
	_hal_mac_fill_mu_sc_tbl_row(&mu_table.mu_score_tbl_4, &score_tbl->mu_score[4]);
	_hal_mac_fill_mu_sc_tbl_row(&mu_table.mu_score_tbl_5, &score_tbl->mu_score[5]);

	hal_status = mac_info->ops->set_mu_table(mac_info, &mu_table);

	return hal_status;
}


enum rtw_hal_status
rtw_hal_mac_parse_c2h(void *hal, u8 *buf, u32 buf_len, void *c2h)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;

	hal_status = mac->ops->process_c2h(mac, buf, buf_len, (u8 *)c2h);

	return hal_status;
}

/**
 * Required information in (hal_handle_rx_buffer_XXXXX case RX_DESC_PKT_TYPE_PPDU_STATUS),
 * it cannot be used by core/phl/other hal module
 **/
void
_hal_mac_ax_ppdu_sts_to_hal_ppdu_sts(
	struct mac_ax_ppdu_rpt *mac_ppdu, void *hal_ppdu_sts)
{
	struct hal_ppdu_sts *hal_ppdu =
			(struct hal_ppdu_sts *)hal_ppdu_sts;
	u8 i = 0;
	u32 j = 0;

	hal_ppdu->rx_cnt_ptr = mac_ppdu->rx_cnt_ptr;
	hal_ppdu->phy_st_ptr = mac_ppdu->phy_st_ptr;
	hal_ppdu->plcp_ptr = mac_ppdu->plcp_ptr;
	hal_ppdu->plcp_size = mac_ppdu->plcp_size;
	hal_ppdu->phy_st_size = mac_ppdu->phy_st_size;
	hal_ppdu->rx_cnt_size = mac_ppdu->rx_cnt_size;
	hal_ppdu->usr_num = mac_ppdu->usr_num;
	for(i = 0; i < mac_ppdu->usr_num; i++) {
		if(1 == mac_ppdu->usr[i].vld) {
			hal_ppdu->usr[i].vld = 1;
			hal_ppdu->usr[i].macid = mac_ppdu->usr[i].macid;
			hal_ppdu->usr[i].has_data = mac_ppdu->usr[i].has_data;
			hal_ppdu->usr[i].has_ctrl = mac_ppdu->usr[i].has_ctrl;
			hal_ppdu->usr[i].has_mgnt = mac_ppdu->usr[i].has_mgnt;
			hal_ppdu->usr[i].has_bcn = mac_ppdu->usr[i].has_bcn;
		}
	}
	/* process / decode rx cnt report */
	/* TODO: Halmac api shall provid decoder */
	if ((0 != hal_ppdu->rx_cnt_size) && (NULL != hal_ppdu->rx_cnt_ptr)) {
		for(j = 0; (j < (hal_ppdu->rx_cnt_size/2)) &&
		    (j < HAL_RXCNT_MAX); j++) {
			hal_ppdu->rx_cnt.ppdu_cnt[j] =
				((u16)*(hal_ppdu->rx_cnt_ptr + 2 * j));
		}
	}
}

/**
 * if any information is required for other core/phl module,
 * copy to rx meta data or hal_info from halmac ax ppdu status.
 **/
void
_hal_mac_ax_ppdu_sts_to_hal_info(struct hal_info_t *hal_info,
	struct mac_ax_ppdu_rpt *mac_ppdu, void *rx_mdata)
{
	/* struct rtw_r_meta_data *mdata =
			(struct rtw_r_meta_data *)rx_mdata; */
	return;
}
/**
 * rtw_hal_mac_ax_parse_ppdu_sts
 * @hal:(struct hal_info_t *)
 * @mac_valid:if mac information invalid (from rx desc)
 * @buf: pointer of ppdu status, point to header of mac_info
 * @buf_l:ppdu status payload size
 * @ppdu_sts: (struct hal_ppdu_sts *) for return value to hal
 * @rx_mdata: (struct rtw_r_meta_data *) for saving ppdu status
 */
enum rtw_hal_status
rtw_hal_mac_ax_parse_ppdu_sts(void *hal, u8 mac_valid, u8 *buf, u16 buf_l,
			      void *ppdu_sts, void *rx_mdata)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_ppdu_rpt ppdu_rpt = {0};

	PHL_DBG("%s\n", __FUNCTION__);

	hal_status = mac->ops->parse_ppdu(hal_info->mac, buf, buf_l,
					  mac_valid, &ppdu_rpt);

	if (hal_status == RTW_HAL_STATUS_SUCCESS) {
		_hal_mac_ax_ppdu_sts_to_hal_ppdu_sts(&ppdu_rpt, ppdu_sts);
		_hal_mac_ax_ppdu_sts_to_hal_info(hal_info ,&ppdu_rpt,
						 rx_mdata);
	}

	return hal_status;
}


/**
 * the function to enable HW header conversion function
 * @hal_info: see struct hal_info_t
 * @en_hdr_conv: true to enable, false to disable
 *
 * return RTW_HAL_STATUS_MAC_API_FAILURE if update fail
 */
enum rtw_hal_status rtw_hal_hdr_conv_cfg(struct hal_info_t *hal_info,
				      u8 en_hdr_conv)
{
	enum rtw_hal_status sts = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 ret = 0;

	ret = mac->ops->hdr_conv(mac, en_hdr_conv);

	if (MACSUCCESS == ret) {
		sts = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MAC, _PHL_WARNING_, "hdr_conv fail (0x%08X)\n",
			  ret);
		sts = RTW_HAL_STATUS_MAC_API_FAILURE;
	}

	return sts;
}


#ifdef RTW_PHL_BCN //fill hal mac ops
enum rtw_hal_status
hal_mac_ax_config_beacon(struct hal_info_t *hal, struct rtw_bcn_entry *bcn_entry)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct rtw_bcn_info_cmn *bcn_cmn = bcn_entry->bcn_cmn;
	struct rtw_bcn_info_hw *bcn_hw = &bcn_entry->bcn_hw;
	enum mac_ax_port_cfg_type ptype;
	struct mac_ax_port_cfg_para ppara = {0};

	ppara.band = bcn_hw->band;
	ppara.port = bcn_hw->port;
	ppara.mbssid_idx = bcn_hw->mbssid;

	ptype = MAC_AX_PCFG_BCN_INTV;
	ppara.val = bcn_cmn->bcn_interval;
	if (mac->ops->port_cfg(mac, ptype, &ppara) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	ptype = MAC_AX_PCFG_HIQ_DTIM;
	ppara.val = bcn_cmn->bcn_dtim;
	if (mac->ops->port_cfg(mac, ptype, &ppara) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
hal_mac_ax_send_beacon(struct hal_info_t *hal, struct rtw_bcn_entry *bcn_entry)
{

	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct rtw_bcn_info_cmn *bcn_cmn = bcn_entry->bcn_cmn;
	struct rtw_bcn_info_hw *bcn_hw = &bcn_entry->bcn_hw;
	struct mac_ax_bcn_info info = {0};

	if(!mac->ops->send_bcn_h2c)
		return RTW_HAL_STATUS_FAILURE;

	info.pld_buf = bcn_cmn->bcn_buf;
	info.pld_len = (u16)bcn_cmn->bcn_length;

	info.band = bcn_hw->band;
	info.port = bcn_hw->port;
	info.mbssid = bcn_hw->mbssid;
	info.grp_ie_ofst = (u8)bcn_cmn->ie_offset_tim;
	info.macid = bcn_hw->mac_id;

	if(bcn_cmn->bcn_offload & BIT(BCN_HW_TIM))
		info.grp_ie_ofst |= BIT(7);

	if(bcn_cmn->bcn_offload & BIT(BCN_HW_SEQ)){
		info.ssn_sel = 1;
		info.ssn_mode = 1;
	}
	else {
		info.ssn_sel = 0;
		info.ssn_mode = 0;
	}

	info.rate_sel = (u16)bcn_cmn->bcn_rate;

	mac->ops->send_bcn_h2c(mac, &info);

	return RTW_HAL_STATUS_SUCCESS;

}
#endif //RTW_PHL_BCN

enum rtw_hal_status
rtw_hal_mac_ppdu_stat_cfg(struct hal_info_t *hal_info,
					u8 band_idx,
					bool ppdu_stat_en,
					u8 appen_info,
					u8 filter)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_phy_rpt_cfg cfg = {0};

	cfg.en = ppdu_stat_en;
	cfg.type = MAC_AX_PPDU_STATUS;
	/*cfg.dest = MAC_AX_PRPT_DEST_HOST;*/

	cfg.u.ppdu.band = band_idx;
	if (ppdu_stat_en) {
		if (appen_info&HAL_PPDU_MAC_INFO)
			cfg.u.ppdu.bmp_append_info |= MAC_AX_PPDU_MAC_INFO;
		if (appen_info&HAL_PPDU_PLCP)
			cfg.u.ppdu.bmp_append_info |= MAC_AX_PPDU_PLCP;
		if (appen_info&HAL_PPDU_RX_CNT)
			cfg.u.ppdu.bmp_append_info |= MAC_AX_PPDU_RX_CNT;

		if (filter&HAL_PPDU_HAS_A1M)
			cfg.u.ppdu.bmp_filter |= MAC_AX_PPDU_HAS_A1M;
		if (filter&HAL_PPDU_HAS_CRC_OK)
			cfg.u.ppdu.bmp_filter |= MAC_AX_PPDU_HAS_CRC_OK;

		cfg.u.ppdu.dup2fw_en = false;
		cfg.u.ppdu.dup2fw_len = 0;
	}

	if (mac->ops->cfg_phy_rpt(mac, &cfg) != MACSUCCESS) {
		PHL_ERR("%s fault\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_config_hw_mgnt_sec(struct hal_info_t *hal_info, u8 en)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *hal_mac_ops = mac->ops;

	hal_mac_ops->sta_hw_security_support(mac, SEC_UC_MGNT_ENC, en);
	hal_mac_ops->sta_hw_security_support(mac, SEC_BMC_MGNT_ENC, en);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_get_append_fcs(struct hal_info_t *hal_info, u8 *val)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *ops = mac->ops;

	if (ops->get_hw_value(mac, MAC_AX_HW_GET_APP_FCS, val) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	else
		return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_get_acpt_icv_err(struct hal_info_t *hal_info, u8 *val)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *ops = mac->ops;

	if (ops->get_hw_value(mac, MAC_AX_HW_GET_RX_ICVERR, val) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	else
		return RTW_HAL_STATUS_SUCCESS;
}

#ifdef CONFIG_PHL_CHANNEL_INFO
enum rtw_hal_status
rtw_hal_mac_chan_info_cfg(struct hal_info_t *hal_info,
				bool chinfo_en, u8 macid,
				u8 mode, u8 filter, u8 sg_size)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_phy_rpt_cfg cfg = {0};

	cfg.en = chinfo_en;
	cfg.type = MAC_AX_CH_INFO;
	/*cfg.dest = MAC_AX_PRPT_DEST_HOST;*/
	cfg.u.chif.macid = macid;

	if (chinfo_en) {
		/*ToDo - mode*/
		cfg.u.chif.trigger = MAC_AX_CH_INFO_MACID;
		/*ToDo - filter*/
		cfg.u.chif.bmp_filter = MAC_AX_CH_INFO_DATA_FRM;
		cfg.u.chif.dis_to = 0;
		/*ToDo - sg_size*/
		cfg.u.chif.seg_size = MAC_AX_CH_IFNO_SEG_512;
	}
	if (mac->ops->cfg_phy_rpt(mac, &cfg) != MACSUCCESS) {
		PHL_ERR("%s fault\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}
	return RTW_HAL_STATUS_SUCCESS;
}
#endif /* CONFIG_PHL_CHANNEL_INFO */

void rtw_hal_mac_dbg_status_dump(struct hal_info_t *hal, struct hal_mac_dbg_dump_cfg *cfg)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_dbgpkg pkg = {0};
	struct mac_ax_dbgpkg_en pkg_en = {0};

	pkg.ss_dbg_0 = cfg->ss_dbg_0;
	pkg.ss_dbg_1 = cfg->ss_dbg_1;

	pkg_en.ss_dbg = cfg->ss_dbg;
	pkg_en.dle_dbg = cfg->dle_dbg;
	pkg_en.dmac_dbg = cfg->dmac_dbg;
	pkg_en.cmac_dbg = cfg->cmac_dbg;
	pkg_en.mac_dbg_port = cfg->mac_dbg_port;
	pkg_en.plersvd_dbg = cfg->plersvd_dbg;
	pkg_en.tx_flow_dbg = cfg->tx_flow_dbg;

	PHL_INFO("%s: ss_dbg_0 %d, ss_dbg_1 %d, ss_dbg %d\n", __func__, pkg.ss_dbg_0, pkg.ss_dbg_1, pkg_en.ss_dbg);
	PHL_INFO("%s: dle_dbg %d, dmac_dbg %d, cmac_dbg %d\n", __func__, pkg_en.dle_dbg, pkg_en.dmac_dbg, pkg_en.cmac_dbg);
	PHL_INFO("%s: mac_dbg_port %d, plersvd_dbg %d, tx_flow_dbg %d\n", __func__, pkg_en.mac_dbg_port, pkg_en.plersvd_dbg, pkg_en.tx_flow_dbg);

	mac->ops->dbg_status_dump(mac, &pkg, &pkg_en);
}

#ifdef CONFIG_PHL_DFS
enum rtw_hal_status
rtw_hal_mac_dfs_rpt_cfg(struct hal_info_t *hal_info,
				bool rpt_en, u8 rpt_num, u8 rpt_to)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_phy_rpt_cfg cfg = {0};

	cfg.en = rpt_en;
	cfg.type = MAC_AX_DFS;
	/*cfg.dest = MAC_AX_PRPT_DEST_HOST;*/

	if (rpt_en) {
		cfg.u.dfs.num_th = rpt_num;
		cfg.u.dfs.en_timeout = rpt_to;
	}

	if (mac->ops->cfg_phy_rpt(mac, &cfg) != MACSUCCESS) {
		PHL_ERR("%s fault\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_parse_dfs(struct hal_info_t *hal_info,
			u8 *buf, u32 buf_len, struct mac_ax_dfs_rpt *dfs_rpt)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;

	if (mac->ops->parse_dfs(mac, buf, buf_len, dfs_rpt) != MACSUCCESS) {
		PHL_ERR("%s fault\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}
#endif /*CONFIG_PHL_DFS*/

enum rtw_hal_status
_hal_mac_get_pkt_ofld(struct hal_info_t *hal_info, u8 *id)
{
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u8 *pkt_buf = NULL;
	u16 pkt_len;

	if(mac == NULL)
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	if (mac->ops->pkt_ofld_packet(mac, &pkt_buf, &pkt_len, id) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	debug_dump_data((u8 *)pkt_buf, pkt_len, "pkt ofld");

	_os_mem_free(hal_com->drv_priv, pkt_buf, pkt_len);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
_hal_mac_chk_pkt_ofld(struct hal_info_t *hal_info)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	void *d = hal_to_drvpriv(hal_info);
	u16 loop_cnt = 0;

	if(mac == NULL)
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	do
	{
		if (mac->ops->check_fwofld_done(mac, 1) == MACSUCCESS)
			break;

		_os_sleep_ms(d, POLLING_HALMAC_TIME);

		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if ( loop_cnt < POLLING_HALMAC_CNT) {
		PHL_PRINT("%s, check count = %d.\n", __func__, loop_cnt);
		return RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_ERR("%s, polling timeout!!!\n", __func__);
		return RTW_HAL_STATUS_FAILURE;
	}
}

enum rtw_hal_status
_hal_mac_add_pkt_ofld(struct hal_info_t *hal_info, u8 *pkt, u16 len, u8 *id)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 status;

	if(mac == NULL)
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	PHL_PRINT("%s: len %d.\n", __func__, len);

	status = mac->ops->add_pkt_ofld(mac, pkt, len, id);
	if (status != MACSUCCESS) {
		PHL_ERR("%s fault, status = %d.\n", __func__, status);
		return RTW_HAL_STATUS_FAILURE;
	}
	PHL_PRINT("%s: id %d.\n", __func__, *id);

	status = _hal_mac_chk_pkt_ofld(hal_info);
	return status;
}

enum rtw_hal_status
_hal_mac_del_pkt_ofld(struct hal_info_t *hal_info, u8 *id)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 status;

	if(mac == NULL)
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	PHL_PRINT("%s: id %d.\n", __func__, *id);

	status = mac->ops->del_pkt_ofld(mac, *id);
	if (status != MACSUCCESS) {
		PHL_ERR("%s fault, status = %d.\n", __func__, status);
		return RTW_HAL_STATUS_FAILURE;
	}

	status = _hal_mac_chk_pkt_ofld(hal_info);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
_hal_mac_read_pkt_ofld(struct hal_info_t *hal_info, u8 *id)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 status;

	if(mac == NULL)
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	PHL_PRINT("%s: id %d.\n", __func__, *id);

	status = mac->ops->read_pkt_ofld(mac, *id);
	if (status != MACSUCCESS) {
		PHL_ERR("%s fault, status = %d.\n", __func__, status);
		return RTW_HAL_STATUS_FAILURE;
	}

	status = _hal_mac_chk_pkt_ofld(hal_info);
	if (status != MACSUCCESS) {
		return RTW_HAL_STATUS_FAILURE;
	}

	status = _hal_mac_get_pkt_ofld(hal_info, id);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_pkt_ofld(struct hal_info_t *hal, u8 *id, u8 op,
						u8 *pkt, u16 *len)
{
	enum rtw_hal_status status = RTW_HAL_STATUS_FAILURE;

	switch(op) {
	case PKT_OFLD_ADD:
		status = _hal_mac_add_pkt_ofld(hal, pkt, *len, id);
		break;

	case PKT_OFLD_DEL:
		status = _hal_mac_del_pkt_ofld(hal, id);
		break;

	case PKT_OFLD_READ:
		status = _hal_mac_read_pkt_ofld(hal, id);
		break;

	default:
		PHL_ERR("%s op(%d) not define.\n", __func__, op);
		break;
	}

	return status;
}

enum rtw_hal_status rtw_hal_mac_pkt_update_ids(struct hal_info_t *hal,
						struct pkt_ofld_entry *entry)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal->mac;
	struct mac_ax_general_pkt_ids mac_ids = {0};
	u32 status;

	mac_ids.macid = (u8)entry->macid;
	mac_ids.probersp = entry->pkt_info[PKT_TYPE_PROBE_RSP].id;
	mac_ids.pspoll = entry->pkt_info[PKT_TYPE_PS_POLL].id;
	mac_ids.nulldata = entry->pkt_info[PKT_TYPE_NULL_DATA].id;
	mac_ids.qosnull = entry->pkt_info[PKT_TYPE_QOS_NULL].id;
	mac_ids.cts2self = entry->pkt_info[PKT_TYPE_CTS2SELF].id;

	PHL_PRINT("macid %d, probersp %d, pspoll %d, nulldata %d, qosnull %d, cts2self %d.\n",
		mac_ids.macid,
		mac_ids.probersp,
		mac_ids.pspoll,
		mac_ids.nulldata,
		mac_ids.qosnull,
		mac_ids.cts2self);

	status = mac->ops->general_pkt_ids(mac, &mac_ids);
	if (status != MACSUCCESS) {
		PHL_ERR("%s fault, status = %d.\n", __func__, status);
		return RTW_HAL_STATUS_FAILURE;
	}
	return status;
}
enum rtw_hal_status
rtw_hal_mac_reset_pkt_ofld_state(struct hal_info_t *hal_info)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;

	if (mac == NULL)
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	if (mac->ops->reset_fwofld_state(mac, 1) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	else
		return RTW_HAL_STATUS_SUCCESS;

}

/* comment temporary and review it later */
#if 0
u4Byte hal_mac_ax_dbg_h2cpkt_lb(RT_HAL_MAC_INFO *hm_info, u32 size)
{
	PADAPTER adapter = hm_info->adapter;
	u32 ret = 0;

	u8 *h2cbuf = NULL;
	FunctionIn(COMP_HAL_MAC_API);

	//ret = hm_info->halmac_ax_ops->fwcmd_lb(hm_info->halmac_ax_apter,
	//	100, 0);

	PlatformAllocateMemory(adapter, (PVOID *)&h2cbuf, size);
	PlatformZeroMemory(h2cbuf,size);

	for (u4Byte tmpc = 0; tmpc < size - 32; tmpc++) {
		h2cbuf[32 + tmpc] = (u8)tmpc & 0xFF;
	}

	hal_mac_ax_send_h2c_pkt(adapter, h2cbuf, size);

	PlatformFreeMemory(h2cbuf, size);

	FunctionOut(COMP_HAL_MAC_API);
	return ret;
}


u4Byte hal_mac_ax_update_table_dl_swru(RT_HAL_MAC_INFO *hm_info,
	struct ofdma_dl_group *dl_group)
{
	PADAPTER adapter = hm_info->adapter;
	u32 ret=0;
	FunctionIn(COMP_HAL_MAC_API);

	ret = hm_info->halmac_ax_ops->upd_dlru_grptbl(
		hm_info->halmac_ax_apter,
		&dl_group->dl_grp_table);


	FunctionOut(COMP_HAL_MAC_API);
	return ret;
}

u4Byte hal_mac_ax_update_table_dl_swfix(RT_HAL_MAC_INFO *hm_info,
	struct ofdma_dl_group *dl_group)
{
	PADAPTER adapter = hm_info->adapter;
	u32 ret=0;
	FunctionIn(COMP_HAL_MAC_API);

	ret = hm_info->halmac_ax_ops->upd_dlru_fixtbl(
		hm_info->halmac_ax_apter,
		&dl_group->fixed_mode_group);


	FunctionOut(COMP_HAL_MAC_API);
	return ret;
}


u4Byte hal_mac_ax_update_table_ul_rufix(RT_HAL_MAC_INFO *hm_info,
	struct ofdma_ul_group *ul_group)
{
	PADAPTER adapter = hm_info->adapter;
	u32 ret=0;
	FunctionIn(COMP_HAL_MAC_API);

	ret = hm_info->halmac_ax_ops->upd_ulru_fixtbl(
		hm_info->halmac_ax_apter,
		&ul_group->fixed_mode_group);

	FunctionOut(COMP_HAL_MAC_API);
	return ret;
}

u4Byte hal_mac_ax_update_table_ul_ru_table(RT_HAL_MAC_INFO *hm_info,
	struct ofdma_ul_group *ul_group)
{
	PADAPTER adapter = hm_info->adapter;
	u32 ret=0;
	FunctionIn(COMP_HAL_MAC_API);

	ret = hm_info->halmac_ax_ops->upd_ulru_grptbl(
		hm_info->halmac_ax_apter,
		&ul_group->ul_grp_table);

	FunctionOut(COMP_HAL_MAC_API);
	return ret;
}


u4Byte hal_mac_ax_update_table_ul_drvfix(RT_HAL_MAC_INFO *hm_info,
	struct ofdma_ul_group *ul_group)
{
	PADAPTER adapter = hm_info->adapter;
	u32 ret=0;
	FunctionIn(COMP_HAL_MAC_API);

	ret = hm_info->halmac_ax_ops->upd_ul_fixinfo(
		hm_info->halmac_ax_apter,
		&ul_group->drv_fixed_info);

	FunctionOut(COMP_HAL_MAC_API);
	return ret;
}


u4Byte hal_mac_ax_issue_bsrp(RT_HAL_MAC_INFO *hm_info,
	struct ofdma_ul_group *ul_group)
{
	PADAPTER adapter = hm_info->adapter;
	u32 ret=0;
	FunctionIn(COMP_HAL_MAC_API);

	ret = hm_info->halmac_ax_ops->upd_ul_fixinfo(
		hm_info->halmac_ax_apter,
		&ul_group->drv_fixed_info
		);

	FunctionOut(COMP_HAL_MAC_API);
	return ret;
}


u4Byte hal_mac_ax_update_ru_sta(RT_HAL_MAC_INFO *hm_info,
	u8 *ru_sta_info)
{
	/* upd_rusta_info */
	struct mac_ax_bb_stainfo *bb_stainfo = (struct mac_ax_bb_stainfo *)ru_sta_info;

	hm_info->halmac_ax_ops->upd_rusta_info(
			hm_info->halmac_ax_apter,
			bb_stainfo
			);
	return 0;
}

u4Byte hal_mac_ax_update_ba_info_table(RT_HAL_MAC_INFO *hm_info,
	u8 ba_info)
{
	/* upd_ba_infotbl */
	struct mac_ax_ba_infotbl *ba_info_tbl = (struct mac_ax_ba_infotbl *)ba_info;

	hm_info->halmac_ax_ops->upd_ba_infotbl(
				hm_info->halmac_ax_apter,
				ba_info_tbl
				);

	return 0;
}

u4Byte hal_mac_ax_Test_H2C(RT_HAL_MAC_INFO *hm_info,
	u32 length, u8 Burst)
{

	u32 ret =0;
	FunctionIn(COMP_HAL_MAC_API);
	ret = hm_info->halmac_ax_ops->fwcmd_lb(
				hm_info->halmac_ax_apter,
				length,
				Burst
				);


	FunctionOut(COMP_HAL_MAC_API);

	return ret;
}

u4Byte hal_mac_ax_compare_h2c_c2h(RT_HAL_MAC_INFO *hm_info,
	u8 *buf, u32 len)
{
	u32 ret=0;
	FunctionIn(COMP_HAL_MAC_API);
	ret = hm_info->halmac_ax_ops->process_c2h(
				hm_info->halmac_ax_apter,
				buf,
				len
				);
	FunctionOut(COMP_HAL_MAC_API);
	return ret;
}
#endif
//====================================================================

/*
 * halmac wrapper API for hal and proto type is at hal_api_mac.h
 * Efuse part.
 */
enum rtw_hal_status
rtw_hal_mac_get_log_efuse_size(struct rtw_hal_com_t *hal_com, u32 *val,
							   bool is_limited)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	if(is_limited == true) {
		if(mac->ops->get_hw_value(mac,
				MAC_AX_HW_GET_LIMIT_LOG_EFUSE_SIZE, val) != MACSUCCESS){
				PHL_ERR("%s: Get limited logical efuse size fail!\n",
						__FUNCTION__);
				return RTW_HAL_STATUS_FAILURE;
		}
	}
	else {
		if(mac->ops->get_hw_value(mac,
				MAC_AX_HW_GET_LOGICAL_EFUSE_SIZE, val) != MACSUCCESS){
				PHL_ERR("%s: Get logical efuse size fail!\n", __FUNCTION__);
				return RTW_HAL_STATUS_FAILURE;
		}
	}
	PHL_INFO("%s: Logical efuse size = %d!\n", __FUNCTION__, *val);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_read_log_efuse_map(struct rtw_hal_com_t *hal_com, u8 *map,
							   bool is_limited)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->dump_log_efuse(mac,
			MAC_AX_EFUSE_PARSER_MAP,
			#ifdef RTW_WKARD_EFUSE_OPERATION
			MAC_AX_EFUSE_R_DRV,
			#else
			MAC_AX_EFUSE_R_AUTO,
			#endif
			map,
			is_limited
			) != MACSUCCESS) {
		PHL_INFO("%s: Dump logical efuse fail!\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}

	PHL_INFO("%s: Dump logical efuse ok!\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

/*
 * HALMAC PG EFUSE API put version length at the tail of map/mask buffer
 */

enum rtw_hal_status
rtw_hal_mac_write_log_efuse_map(struct rtw_hal_com_t *hal_com,
								u8 *map,
								u32 map_size,
								u8 *mask,
								u32 mask_size,
								u8 *map_version,
								u8 *mask_version,
								u8 version_length,
								u8 part,
								bool is_limited)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_pg_efuse_info info;
	enum rtw_hal_status status = RTW_HAL_STATUS_EFUSE_PG_FAIL;
	u8 *tmp_map = NULL;
	u8 *tmp_mask = NULL;

	tmp_map = _os_mem_alloc(hal_com->drv_priv, (map_size + version_length));
	if(tmp_map == NULL) {
		PHL_WARN("%s: Allocate pg map buffer fail!\n", __FUNCTION__);
		status = RTW_HAL_STATUS_RESOURCE;
		goto err_mem_tmp_map;
	}

	tmp_mask = _os_mem_alloc(hal_com->drv_priv, (mask_size + version_length));
	if(tmp_mask == NULL) {
		PHL_WARN("%s: Allocate pg mask buffer fail!\n", __FUNCTION__);
		status = RTW_HAL_STATUS_RESOURCE;
		goto err_mem_tmp_mask;
	}

	/* Copy efuse map and map version to tmp_map buffer */
	_os_mem_cpy(hal_com->drv_priv, tmp_map, map, map_size);
	_os_mem_cpy(hal_com->drv_priv, tmp_map+map_size, map_version,
				version_length);
	/* Copy efuse mask and mask version to tmp_mask buffer */
	_os_mem_cpy(hal_com->drv_priv, tmp_mask, mask, mask_size);
	_os_mem_cpy(hal_com->drv_priv, tmp_mask+mask_size, mask_version,
				version_length);
#if 0 /* For debug usage */
	debug_dump_data(map, (u16)map_size, "logical map:");
	debug_dump_data(map_version, version_length, "logical map version:");
	debug_dump_data(mask, (u16)mask_size, "mask:");
	debug_dump_data(mask_version, version_length, "mask version:");
	debug_dump_data(tmp_map, (u16)(map_size + version_length), "tmp_map:");
	debug_dump_data(tmp_mask, (u16)(mask_size + version_length), "tmp_mask:");
#endif
	info.efuse_map = tmp_map;
	info.efuse_map_size = map_size;
	info.efuse_mask = tmp_mask;
	info.efuse_mask_size= mask_size;

	if (mac->ops->pg_efuse_by_map(mac,
								  &info,
								  MAC_AX_EFUSE_R_DRV,
								  part,
								  is_limited) != MACSUCCESS) {
		PHL_INFO("%s: PG Fail!\n", __FUNCTION__);
		status = RTW_HAL_STATUS_EFUSE_PG_FAIL;
	}
	else {
		PHL_INFO("%s: PG ok!\n", __FUNCTION__);
		status = RTW_HAL_STATUS_SUCCESS;
	}
	_os_mem_free(hal_com->drv_priv, tmp_map, (map_size + version_length));
	_os_mem_free(hal_com->drv_priv, tmp_mask, (mask_size + version_length));

	return status;

err_mem_tmp_mask:
	_os_mem_free(hal_com->drv_priv, tmp_map, (map_size + version_length));

err_mem_tmp_map:

	return status;
}


enum rtw_hal_status
rtw_hal_mac_read_hidden_rpt(struct rtw_hal_com_t *hal_com)
{

	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_defeature_value rpt;
	u32 err;

	err = mac->ops->read_hidden_rpt(mac, &rpt);
	if (err != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_TRIG, _PHL_INFO_, "err=0x%x\n", err);
		return RTW_HAL_STATUS_FAILURE;
	}

	if (rpt.tx_spatial_stream != 7 && rpt.tx_spatial_stream > 0) {
		hal_com->phy_hw_cap[0].tx_num = rpt.tx_spatial_stream;
		hal_com->phy_hw_cap[1].tx_num = rpt.tx_spatial_stream;
		hal_com->rfpath_tx_num = rpt.tx_spatial_stream;
	}
	if (rpt.rx_spatial_stream != 7 && rpt.rx_spatial_stream > 0) {
		hal_com->phy_hw_cap[0].rx_num = rpt.rx_spatial_stream;
		hal_com->phy_hw_cap[1].rx_num = rpt.rx_spatial_stream;
		hal_com->rfpath_rx_num = rpt.rx_spatial_stream;
	}
	if (rpt.hw_special_type > EFUSE_HW_STYPE_NONE &&
	    rpt.hw_special_type < EFUSE_HW_STYPE_GENERAL)
		hal_com->dev_hw_cap.hw_stype_cap = rpt.hw_special_type;

	if (rpt.wl_func_support > EFUSE_WL_FUNC_NONE &&
	    rpt.wl_func_support < EFUSE_WL_FUNC_GENERAL)
		hal_com->dev_hw_cap.wl_func_cap = rpt.wl_func_support;

	PHL_TRACE(COMP_PHL_MAC, _PHL_INFO_, "hidden tx=%d hidden rx=%d\n",
		rpt.tx_spatial_stream, rpt.rx_spatial_stream);
	PHL_TRACE(COMP_PHL_MAC, _PHL_INFO_, "hidden bw=%d\n", rpt.bandwidth);
	PHL_TRACE(COMP_PHL_MAC, _PHL_INFO_, "hidden protocol = %d\n",
		rpt.protocol_80211);
	PHL_TRACE(COMP_PHL_MAC, _PHL_INFO_, "hidden nic=%d\n", rpt.NIC_router);
	PHL_TRACE(COMP_PHL_MAC, _PHL_INFO_, "hidden hw special type=%d\n",
		rpt.hw_special_type);
	PHL_TRACE(COMP_PHL_MAC, _PHL_INFO_, "hidden wl func=%d\n",
		rpt.wl_func_support);
	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status
rtw_hal_mac_check_efuse_autoload(struct rtw_hal_com_t *hal_com, u8 *autoload)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->check_efuse_autoload(mac, autoload) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	if(*autoload == true)
		PHL_INFO("%s: efuse auto load SUCCESS!\n", __FUNCTION__);
	else
		PHL_INFO("%s: efuse auto load FAIL!\n", __FUNCTION__);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_efuse_avl(struct rtw_hal_com_t *hal_com, u32 *val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->get_efuse_avl_size(mac, val) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_efuse_size(struct rtw_hal_com_t *hal_com, u32 *val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->get_hw_value(mac,
		MAC_AX_HW_GET_EFUSE_SIZE, val) != MACSUCCESS){
		PHL_ERR("%s: Get efuse size fail!\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}
	PHL_INFO("%s: Efuse size = %d!\n", __FUNCTION__, *val);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_efuse_mask_size(struct rtw_hal_com_t *hal_com, u32 *val,
								bool is_limited)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if(is_limited == true) {
		if(mac->ops->get_hw_value(mac,
			MAC_AX_HW_GET_LIMIT_EFUSE_MASK_SIZE, val) != MACSUCCESS)
			return RTW_HAL_STATUS_FAILURE;
	}
	else {
		if(mac->ops->get_hw_value(mac,
			MAC_AX_HW_GET_EFUSE_MASK_SIZE, val) != MACSUCCESS)
			return RTW_HAL_STATUS_FAILURE;
	}

	PHL_INFO("%s: efuse mask size = %d\n", __FUNCTION__, *val);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_efuse_info(struct rtw_hal_com_t *hal_com,
	u8 *efuse_map, enum rtw_efuse_info info_type, void *value,
	u8 size, u8 map_valid)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	PHL_INFO("%s\n", __FUNCTION__);

	if (mac->ops->get_efuse_info(mac, efuse_map, info_type, value, size,
								 &map_valid) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_read_phy_efuse(struct rtw_hal_com_t *hal_com,
	u32 addr, u32 size, u8 *value)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	PHL_INFO("%s\n", __FUNCTION__);

	if (mac->ops->read_efuse(mac, addr, size, value,
		MAC_AX_EFUSE_BANK_WIFI) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_read_bt_phy_efuse(struct rtw_hal_com_t *hal_com,
	u32 addr, u32 size, u8 *value)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	PHL_INFO("%s\n", __FUNCTION__);

	if (mac->ops->read_efuse(mac, addr, size, value,
		MAC_AX_EFUSE_BANK_BT) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

u32 rtw_hal_mac_write_msk_pwr_reg(
	struct rtw_hal_com_t *hal_com, u8 band, u32 offset, u32 mask, u32 val)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	u32 result = 0;

	result = ops->write_msk_pwr_reg(mac, band, offset, mask, val);

	if (result != MACSUCCESS)
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_,
			"Write power register failure, status = %d\n", result);

	return result;
}

u32 rtw_hal_mac_set_pwr_reg(struct rtw_hal_com_t *hal_com, u8 band, u32 offset, u32 val){

	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	u32 result = 0;

	result = ops->write_pwr_reg(mac, band, offset, val);

	if (result != MACSUCCESS)
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "Set power register failure, status = %d\n", result);

	return result;
}

u32 rtw_hal_mac_get_pwr_reg(struct rtw_hal_com_t *hal_com, u8 band, u32 offset, u32 *val){
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	u32 result = 0;

	result = ops->read_pwr_reg(mac, band, offset, val);

	if (result != MACSUCCESS)
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "Get power register failure, status = %d\n", result);

	return result;
}

enum rtw_hal_status
rtw_hal_mac_get_xcap(struct rtw_hal_com_t *hal_com, u8 sc_xo, u32 *value){
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *ops = mac->ops;
	enum rtw_hal_status ret = RTW_HAL_STATUS_SUCCESS;

	ret = ops->read_xcap_reg(mac, sc_xo, value);

	if (ret != MACSUCCESS)
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "Get xacp failure, status = %d\n", ret);

	return ret;
}

enum rtw_hal_status
rtw_hal_mac_set_xcap(struct rtw_hal_com_t *hal_com, u8 sc_xo, u32 value){
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *ops = mac->ops;
	enum rtw_hal_status ret = RTW_HAL_STATUS_SUCCESS;

	ret = ops->write_xcap_reg(mac, sc_xo, value);

	if (ret != MACSUCCESS)
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "Set xacp failure, status = %d\n", ret);

	return ret;
}

enum rtw_hal_status
rtw_hal_mac_get_xsi(struct rtw_hal_com_t *hal_com, u8 offset, u8* val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	/* Because mac si function is not support mac ops*/
	/* we call mac si function temporarily until mac team feedback.*/
	if (mac_read_xtal_si(mac, offset, val) != MACSUCCESS) {
		PHL_INFO("Get xsi failure, status = %s\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}

	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_set_xsi(struct rtw_hal_com_t *hal_com, u8 offset, u8 val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	/* Because mac si function is not support mac ops*/
	/* we call mac si function temporarily until mac team feedback.*/
	if (mac_write_xtal_si(mac, offset, val, 0xff) != MACSUCCESS) {
		PHL_INFO("Set xsi failure, status = %s\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}

	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status
rtw_hal_mac_fw_dbg_dump(struct hal_info_t *hal_info, u8 is_low_power)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u8 *buffer = NULL;
	u16 bufSize = FW_PLE_SIZE;
	struct mac_ax_fwdbg_en en;
	u32 mac_err = 0;

	en.status_dbg = 1;
	en.ps_dbg = 1;
	en.rsv_ple_dbg = 0;

	mac_err = mac->ops->fw_dbg_dump(mac, &buffer, &en);
	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	if (en.rsv_ple_dbg && buffer) {
		/*fw buffer is 2048, but Windows DbgPrint only 512 Bytes, so we split buffer to 4 segments*/
		if (buffer != NULL) {
			PHL_PRINT("=======================\n");
			PHL_PRINT("Start to dump fw rsvd ple:\n\n");
			_hal_fw_dbg_dump(hal_info, buffer, bufSize);
			PHL_PRINT("\n=======================\n");
		}
	}
	if (buffer != NULL)
		_os_mem_free(hal_info->hal_com->drv_priv, buffer, bufSize);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_req_pwr_state(struct hal_info_t *hal_info, u8 pwr_state)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if(mac->ops->ps_pwr_state(mac, MAC_AX_PWR_STATE_ACT_REQ, pwr_state)
			== MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_chk_pwr_state(struct hal_info_t *hal_info, u8 pwr_state, u32 *mac_sts)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	*mac_sts = mac->ops->ps_pwr_state(mac, MAC_AX_PWR_STATE_ACT_CHK, pwr_state);
	if(*mac_sts == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_lps_cfg(struct hal_info_t *hal_info,
			struct rtw_hal_lps_info *lps_info)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum mac_ax_ps_mode ax_ps_mode;
	struct mac_ax_lps_info ax_lps_info;

	if (lps_info->lps_en) {
		ax_ps_mode = MAC_AX_PS_MODE_LEGACY;
	} else {
		ax_ps_mode = MAC_AX_PS_MODE_ACTIVE;
	}

	ax_lps_info.listen_bcn_mode = lps_info->listen_bcn_mode;
	ax_lps_info.awake_interval = lps_info->awake_interval;
	ax_lps_info.smart_ps_mode = lps_info->smart_ps_mode;

	if (mac->ops->cfg_lps(mac, (u8)lps_info->macid, ax_ps_mode,
		&ax_lps_info) == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_lps_chk_leave(struct hal_info_t *hal_info, u16 macid, u32 *mac_sts)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	*mac_sts = mac->ops->chk_leave_lps(mac, (u8)macid);
	if(*mac_sts  == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_lps_chk_access(struct hal_info_t *hal_info, u32 offset)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if(mac->ops->io_chk_access(mac, offset) == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

enum rtw_hal_status
rtw_hal_mac_get_rx_cnt(struct hal_info_t *hal_info, u8 cur_phy_idx, u8 type_idx, u32 *ret_value){
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *ops = mac->ops;
	enum rtw_hal_status ret = RTW_HAL_STATUS_SUCCESS;

	struct mac_ax_rx_cnt rx_cnt;
	u16 rx_cnt_buff[MAC_AX_RX_PPDU_MAX];
	u32 rx_cnt_total=0;
	u16 ppdu_idx;

	rx_cnt.type = type_idx;
	rx_cnt.op = MAC_AX_RXCNT_R;
	rx_cnt.buf = rx_cnt_buff;
	rx_cnt.band = cur_phy_idx;

	ret = ops->rx_cnt(mac, &rx_cnt);

	for(ppdu_idx=0;ppdu_idx<MAC_AX_RX_PPDU_MAX;ppdu_idx++)
	{
		rx_cnt_total+=rx_cnt_buff[ppdu_idx];
	}

	*ret_value = rx_cnt_total;

	if (ret != MACSUCCESS){
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "mac get rx counter fail, status=%d\n",ret);
	}

	return ret;
}

enum rtw_hal_status
rtw_hal_mac_set_reset_rx_cnt(struct hal_info_t *hal_info, u8 cur_phy_idx)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *ops = mac->ops;
	enum rtw_hal_status ret = RTW_HAL_STATUS_SUCCESS;

	struct mac_ax_rx_cnt rx_cnt;
	u16 rx_cnt_buff[MAC_AX_RX_PPDU_MAX];

	rx_cnt.type = MAC_AX_RX_IDX;
	rx_cnt.op = MAC_AX_RXCNT_RST_ALL;
	rx_cnt.buf = rx_cnt_buff;
	rx_cnt.band = cur_phy_idx;

	ret = ops->rx_cnt(mac, &rx_cnt);

	if (ret != MACSUCCESS){
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "mac reset rx cnt fail, status=%d\n",ret);
	}

	return ret;
}

enum rtw_hal_status
rtw_hal_mac_tx_idle_poll(struct rtw_hal_com_t *hal_com, u8 band_idx)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_tx_idle_poll_cfg cfg = {0};
	u32 err = 0;


	cfg.sel = MAC_AX_TX_IDLE_POLL_SEL_BAND;
	cfg.band = band_idx;
	err = mac->ops->tx_idle_poll(mac, &cfg);
	if (err != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_,
			  "Polling tx idle failure(%d)!\n", err);
		return RTW_HAL_STATUS_TIMEOUT;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_sch_txen_cfg {
	RTW_TXEN_BE0 = 1 << 0,
	RTW_TXEN_BK0 = 1 << 1,
	RTW_TXEN_VI0 = 1 << 2,
	RTW_TXEN_VO0 = 1 << 3,
	RTW_TXEN_BE1 = 1 << 4,
	RTW_TXEN_BK1 = 1 << 5,
	RTW_TXEN_VI1 = 1 << 6,
	RTW_TXEN_VO1 = 1 << 7,
	RTW_TXEN_MG0 = 1 << 8,
	RTW_TXEN_MG1 = 1 << 9,
	RTW_TXEN_MG2 = 1 << 10,
	RTW_TXEN_HI = 1 << 11,
	RTW_TXEN_BCN = 1 << 12,
	RTW_TXEN_UL = 1 << 13,
	RTW_TXEN_TWT0 = 1 << 14,
	RTW_TXEN_TWT1 = 1 << 15,
	RTW_TXEN_DRV_MASK = 0x19FF,
	RTW_TXEN_ALL = 0xFFFF,
};

enum rtw_hal_status
rtw_hal_mac_set_sch_tx_en(struct rtw_hal_com_t *hal_com, u8 band_idx,
						u16 tx_en, u16 tx_en_mask)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_sch_tx_en_cfg cfg;
	enum rtw_hal_status ret;
	u32 result = 0;

	ret = RTW_HAL_STATUS_SUCCESS;
	cfg.band = band_idx;

	cfg.tx_en.be0 = ((tx_en & RTW_TXEN_BE0) ? 1 : 0);
	cfg.tx_en.bk0 = ((tx_en & RTW_TXEN_BK0) ? 1 : 0);
	cfg.tx_en.vi0 = ((tx_en & RTW_TXEN_VI0) ? 1 : 0);
	cfg.tx_en.vo0 = ((tx_en & RTW_TXEN_VO0) ? 1 : 0);
	cfg.tx_en.be1 = ((tx_en & RTW_TXEN_BE1) ? 1 : 0);
	cfg.tx_en.bk1 = ((tx_en & RTW_TXEN_BK1) ? 1 : 0);
	cfg.tx_en.vi1 = ((tx_en & RTW_TXEN_VI1) ? 1 : 0);
	cfg.tx_en.vo1 = ((tx_en & RTW_TXEN_VO1) ? 1 : 0);
	cfg.tx_en.mg0 = ((tx_en & RTW_TXEN_MG0) ? 1 : 0);
	cfg.tx_en.mg1 = ((tx_en & RTW_TXEN_MG1) ? 1 : 0);
	cfg.tx_en.mg2 = ((tx_en & RTW_TXEN_MG2) ? 1 : 0);
	cfg.tx_en.hi = ((tx_en & RTW_TXEN_HI) ? 1 : 0);
	cfg.tx_en.bcn = ((tx_en & RTW_TXEN_BCN) ? 1 : 0);
	cfg.tx_en.ul = ((tx_en & RTW_TXEN_UL) ? 1 : 0);
	cfg.tx_en.twt0 = ((tx_en & RTW_TXEN_TWT0) ? 1 : 0);
	cfg.tx_en.twt1 = ((tx_en & RTW_TXEN_TWT1) ? 1 : 0);

	cfg.tx_en_mask.be0 = ((tx_en_mask & RTW_TXEN_BE0) ? 1 : 0);
	cfg.tx_en_mask.bk0 = ((tx_en_mask & RTW_TXEN_BK0) ? 1 : 0);
	cfg.tx_en_mask.vi0 = ((tx_en_mask & RTW_TXEN_VI0) ? 1 : 0);
	cfg.tx_en_mask.vo0 = ((tx_en_mask & RTW_TXEN_VO0) ? 1 : 0);
	cfg.tx_en_mask.be1 = ((tx_en_mask & RTW_TXEN_BE1) ? 1 : 0);
	cfg.tx_en_mask.bk1 = ((tx_en_mask & RTW_TXEN_BK1) ? 1 : 0);
	cfg.tx_en_mask.vi1 = ((tx_en_mask & RTW_TXEN_VI1) ? 1 : 0);
	cfg.tx_en_mask.vo1 = ((tx_en_mask & RTW_TXEN_VO1) ? 1 : 0);
	cfg.tx_en_mask.mg0 = ((tx_en_mask & RTW_TXEN_MG0) ? 1 : 0);
	cfg.tx_en_mask.mg1 = ((tx_en_mask & RTW_TXEN_MG1) ? 1 : 0);
	cfg.tx_en_mask.mg2 = ((tx_en_mask & RTW_TXEN_MG2) ? 1 : 0);
	cfg.tx_en_mask.hi = ((tx_en_mask & RTW_TXEN_HI) ? 1 : 0);
	cfg.tx_en_mask.bcn = ((tx_en_mask & RTW_TXEN_BCN) ? 1 : 0);
	cfg.tx_en_mask.ul = ((tx_en_mask & RTW_TXEN_UL) ? 1 : 0);
	cfg.tx_en_mask.twt0 = ((tx_en_mask & RTW_TXEN_TWT0) ? 1 : 0);
	cfg.tx_en_mask.twt1 = ((tx_en_mask & RTW_TXEN_TWT1) ? 1 : 0);

	result = ops->set_hw_value(mac, MAC_AX_HW_SET_SCH_TXEN_CFG, &cfg);

	if (result != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "Set Tx pause failure, status = %d\n", result);
		ret = RTW_HAL_STATUS_FAILURE;
	}

	return ret;
}

enum rtw_hal_status
rtw_hal_tx_pause(struct rtw_hal_com_t *hal_com,
		 u8 band_idx, bool tx_pause, enum tx_pause_rson rson)
{
	u16 *tx_off;
	enum tx_pause_rson i;
	u16 tx_cfg = 0;
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;


	tx_off = &hal_com->band[band_idx].tx_pause[rson];
	if (tx_pause == true) {
		switch (rson) {
		case PAUSE_RSON_NOR_SCAN:
			*tx_off = (u16)~RTW_TXEN_MG0;
			break;
		case PAUSE_RSON_RFK:
		case PAUSE_RSON_PSD:
		case PAUSE_RSON_DFS:
		case PAUSE_RSON_DBCC:
		case PAUSE_RSON_RESET:
			*tx_off = (u16)RTW_TXEN_ALL;
			break;
		default:
			PHL_ERR("Unknow pause reason:%d\n", rson);
			goto _error;
		}
	} else {
		*tx_off = 0;
	}

	tx_off = hal_com->band[band_idx].tx_pause;
	for (i = 0; (i < PAUSE_RSON_MAX) && (tx_cfg != RTW_TXEN_ALL); i++)
		if (tx_off[i])
			tx_cfg |= tx_off[i];
	/* tx_cfg is white list, but tx_pause of struct rtw_hw_band is black list */
	tx_cfg = ~tx_cfg;
	PHL_INFO("TX %sPause - Reason(%d) for band-%u, final tx_cfg(0x%04x)\n",
		 tx_pause?"":"Un-", rson, band_idx, tx_cfg);

	hstatus = rtw_hal_mac_set_sch_tx_en(hal_com, band_idx,
					    tx_cfg, RTW_TXEN_ALL);
	if (hstatus != RTW_HAL_STATUS_SUCCESS)
		goto _error;

	if ((rson == PAUSE_RSON_RFK) && tx_pause) {
		hstatus = rtw_hal_mac_tx_idle_poll(hal_com, band_idx);
		if (hstatus != RTW_HAL_STATUS_SUCCESS)
			goto _error;
	}

_error:
	return hstatus;
}

enum rtw_hal_status
rtw_hal_mac_set_macid_pause(struct rtw_hal_com_t *hal_com,
                            u16 macid, bool pause)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_macid_pause_cfg cfg = {0};
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;

	cfg.macid = (u8)macid;
	cfg.pause = pause;

	PHL_INFO("%s macid:%d(%s) \n", __func__, macid, pause ? "pause":"unpause");
	/*TODO - MAC_AX_HW_SET_MULTI_ID_PAUSE*/
	if (ops->set_hw_value(mac, MAC_AX_HW_SET_ID_PAUSE, &cfg) != MACSUCCESS) {
		PHL_ERR("%s failed\n", __func__);
		hstatus = RTW_HAL_STATUS_FAILURE;
	}

	return hstatus;
}

enum rtw_hal_status
rtw_hal_mac_set_macid_grp_pause(struct rtw_hal_com_t *hal_com,
                             u32 *macid_arr, u8 macid_arr_sz, bool pause)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *ops = mac->ops;
	struct mac_ax_macid_pause_grp cfg = {0};
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_SUCCESS;
	u8 pause_grp_sz = 0, mac_grp_sz = 0, arr_idx = 0, bit_idx = 0;
	u32 mac_ret = MACSUCCESS;

	/* size check */
	mac_grp_sz = sizeof(cfg.pause_grp) / sizeof(cfg.pause_grp[0]);
	pause_grp_sz = MIN(mac_grp_sz, macid_arr_sz);

	for (arr_idx = 0; arr_idx < pause_grp_sz; arr_idx++) {
		for (bit_idx = 0; bit_idx < 32; bit_idx++) {
			if (macid_arr[arr_idx] & BIT(bit_idx)) {
				cfg.mask_grp[arr_idx] |= BIT(bit_idx);
				if (pause)
					cfg.pause_grp[arr_idx] |= BIT(bit_idx);
				else
					cfg.pause_grp[arr_idx] &= ~BIT(bit_idx);
			}
		}
	}

	PHL_INFO("%s cfg pause_grp:0x%x,0x%x,0x%x,0x%x, mask_grp:0x%x,0x%x,0x%x,0x%x(%s)\n", __func__,
			cfg.pause_grp[0], cfg.pause_grp[1], cfg.pause_grp[2], cfg.pause_grp[3],
			cfg.mask_grp[0], cfg.mask_grp[1], cfg.mask_grp[2], cfg.mask_grp[3],
			pause ? "pause":"unpause");

	mac_ret = ops->set_hw_value(mac, MAC_AX_HW_SET_MULTI_ID_PAUSE, &cfg);
	if (mac_ret != MACSUCCESS) {
		PHL_ERR("%s failed(mac ret:%d)\n", __func__, mac_ret);
		hstatus = RTW_HAL_STATUS_FAILURE;
	}

	return hstatus;
}

enum rtw_hal_status
rtw_hal_mac_fw_log_cfg(struct rtw_hal_com_t *hal_com,
			struct rtw_hal_fw_log_cfg *fl_cfg)
{
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_fw_log log_cfg = {0};
	u32 status;

	if(mac == NULL)
		return RTW_HAL_STATUS_MAC_INIT_FAILURE;

	log_cfg.level = fl_cfg->level;
	log_cfg.output = fl_cfg->output;
	log_cfg.comp = fl_cfg->comp;
	log_cfg.comp_ext = fl_cfg->comp_ext;

	PHL_PRINT("%s: level %d, output 0x%08x, comp 0x%08x, comp ext 0x%08x.\n",
			__func__,
			log_cfg.level,
			log_cfg.output,
			log_cfg.comp,
			log_cfg.comp_ext);

	if(log_cfg.output == MAC_AX_FL_LV_UART)
	{
		mac->ops->pinmux_set_func(mac, MAC_AX_GPIO_UART_TX_GPIO5);
		mac->ops->sel_uart_tx_pin(mac, MAC_AX_UART_TX_GPIO5);
		mac->ops->pinmux_set_func(mac, MAC_AX_GPIO_UART_RX_GPIO6);
		mac->ops->sel_uart_rx_pin(mac, MAC_AX_UART_RX_GPIO6);
	}

	status = mac->ops->fw_log_cfg(mac, &log_cfg);
	if (status != MACSUCCESS) {
		PHL_ERR("%s fault, status = %d.\n", __func__, status);
		return RTW_HAL_STATUS_FAILURE;
	}
	return RTW_HAL_STATUS_SUCCESS;
}

u32
rtw_hal_mac_lamode_trig(struct rtw_hal_com_t *hal_com, u8 trig)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 rpt = 0;

	rpt = mac->ops->lamode_trigger(mac, trig);

	return rpt;
}

enum rtw_hal_status
rtw_hal_mac_lamode_cfg_buf(struct rtw_hal_com_t *hal_com, u8 buf_sel,
			   u32 *addr_start, u32 *addr_end)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_la_buf_param param;

	param.la_buf_sel = buf_sel;

	mac->ops->lamode_buf_cfg(mac, &param);

	*addr_start = param.start_addr;
	*addr_end = param.end_addr;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_lamode_cfg(struct rtw_hal_com_t *hal_com, u8 func_en,
		       u8 restart_en, u8 timeout_en, u8 timeout_val,
		       u8 data_loss_imr, u8 la_tgr_tu_sel, u8 tgr_time_val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_la_cfg cfg;

	cfg.la_func_en = func_en;
	cfg.la_restart_en = restart_en;
	cfg.la_timeout_en = timeout_en;
	cfg.la_timeout_val = timeout_val;
	cfg.la_data_loss_imr = data_loss_imr;
	cfg.la_tgr_tu_sel = la_tgr_tu_sel;
	cfg.la_tgr_time_val = tgr_time_val;

	mac->ops->lamode_cfg(mac, &cfg);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_lamode_st(struct rtw_hal_com_t *hal_com, u8 *la_state,
			  u16 *la_finish_addr, bool *la_round_up,
			  bool *la_loss_data)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_la_status info;

	info = mac->ops->get_lamode_st(mac);

	*la_state = info.la_sw_fsmst;
	*la_finish_addr = info.la_buf_wptr;
	*la_round_up = info.la_buf_rndup_ind;
	*la_loss_data = info.la_data_loss;

	return RTW_HAL_STATUS_SUCCESS;
}

static inline void _hal_set_dft_rxfltr(struct mac_ax_rx_fltr_ctrl_t *ctrl,
					struct mac_ax_rx_fltr_ctrl_t *mask)
{
	/* filter packets by address */
	ctrl->sniffer_mode = 0;
	mask->sniffer_mode = 1;

	/* check unicast */
	ctrl->acpt_a1_match_pkt = 1;
	mask->acpt_a1_match_pkt = 1;
	ctrl->uc_pkt_chk_cam_match = 1;
	mask->uc_pkt_chk_cam_match = 1;

	/* check broadcast */
	ctrl->acpt_bc_pkt = 1;
	mask->acpt_bc_pkt = 1;
	ctrl->bc_pkt_chk_cam_match = 1;
	mask->bc_pkt_chk_cam_match = 1;

	/* check multicast */
	ctrl->acpt_mc_pkt = 1;
	mask->acpt_mc_pkt = 1;
	/* black list filter */
	ctrl->mc_pkt_white_lst_mode = 0;
	mask->mc_pkt_white_lst_mode = 1;

	/* check beacon */
	ctrl->bcn_chk_en = 1;
	mask->bcn_chk_en = 1;
	ctrl->bcn_chk_rule = 0; /* 2: A2&A3 match */
	mask->bcn_chk_rule = 0x3;

	/* misc */
	ctrl->acpt_pwr_mngt_pkt = 1;
	mask->acpt_pwr_mngt_pkt = 1;

	ctrl->acpt_ftm_req_pkt = 1;
	mask->acpt_ftm_req_pkt = 1;
}
/**
 * rtw_hal_mac_set_rxfltr_by_mode - Set rx filter option by scenario
 * @hal_com:	pointer of struct rtw_hal_com_t
 * @band:	0x0: band0, 0x1: band1
 * @mode:	scenario mode
 *
 * Set RX filter setting by scenario.
 *
 * Return RTW_HAL_STATUS_SUCCESS when setting is ok.
 */
enum rtw_hal_status
rtw_hal_mac_set_rxfltr_by_mode(struct rtw_hal_com_t *hal_com,
			       u8 band, enum rtw_rx_fltr_mode mode)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_rx_fltr_ctrl_t ctrl = {0};
	struct mac_ax_rx_fltr_ctrl_t mask = {0};
	u32 err;


	switch (mode) {
	case RX_FLTR_MODE_RESTORE:
		break;
	case RX_FLTR_MODE_SNIFFER:
		ctrl.sniffer_mode = 1;
		mask.sniffer_mode = 1;
		break;
	case RX_FLTR_MODE_SCAN:
		ctrl.acpt_a1_match_pkt = 1;
		mask.acpt_a1_match_pkt = 1;
		ctrl.acpt_bc_pkt = 1;
		mask.acpt_bc_pkt = 1;
		ctrl.acpt_mc_pkt = 1;
		mask.acpt_mc_pkt = 1;
		ctrl.uc_pkt_chk_cam_match = 0;
		mask.uc_pkt_chk_cam_match = 1;
		ctrl.bc_pkt_chk_cam_match = 0;
		mask.bc_pkt_chk_cam_match = 1;

		/* Do NOT check B_AX_A_BCN_CHK_RULE
		 * when receiving beacon and probe_response
		 */
		ctrl.bcn_chk_en = 0;
		mask.bcn_chk_en = 1;
		break;
	case RX_FLTR_MODE_STA_NORMAL:
		#if 1
		_hal_set_dft_rxfltr(&ctrl, &mask);

		#else
		/* filter packets by address */
		ctrl.sniffer_mode = 0;
		mask.sniffer_mode = 1;
		/* check unicast */
		ctrl.acpt_a1_match_pkt = 1;
		mask.acpt_a1_match_pkt = 1;
		ctrl.uc_pkt_chk_cam_match = 1;
		mask.uc_pkt_chk_cam_match = 1;
		/* check broadcast */
		ctrl.acpt_bc_pkt = 1;
		mask.acpt_bc_pkt = 1;
		ctrl.bc_pkt_chk_cam_match = 1;
		mask.bc_pkt_chk_cam_match = 1;

		if ((hal_com->chip_id == CHIP_WIFI6_8852A)
		    && (hal_com->cv == CAV)) {
			/* don't check address cam for multicast, accept all */
			ctrl.acpt_mc_pkt = 0;
			mask.acpt_mc_pkt = 1;
		} else {
			/* check multicast */
			ctrl.acpt_mc_pkt = 1;
			mask.acpt_mc_pkt = 1;
			/* black list filter */
			ctrl.mc_pkt_white_lst_mode = 0;
			mask.mc_pkt_white_lst_mode = 1;
		}
		/* check beacon */
		ctrl.bcn_chk_en = 1;
		mask.bcn_chk_en = 1;
		ctrl.bcn_chk_rule = 2; /* 2: A2&A3 match */
		mask.bcn_chk_rule = 0x3;
		/* misc */
		ctrl.acpt_pwr_mngt_pkt = 1;
		mask.acpt_pwr_mngt_pkt = 1;
		ctrl.acpt_ftm_req_pkt = 1;
		mask.acpt_ftm_req_pkt = 1;
		#endif
		break;
	case RX_FLTR_MODE_STA_LINKING:
		#if 1
		_hal_set_dft_rxfltr(&ctrl, &mask);
		/* check broadcast */
		ctrl.acpt_bc_pkt = 1;
		mask.acpt_bc_pkt = 1;
		ctrl.bc_pkt_chk_cam_match = 0;
		mask.bc_pkt_chk_cam_match = 1;

		/* check beacon */
		ctrl.bcn_chk_en = 0;
		mask.bcn_chk_en = 1;

		#else
		/* filter packets by address */
		ctrl.sniffer_mode = 0;
		mask.sniffer_mode = 1;
		/* check unicast */
		ctrl.acpt_a1_match_pkt = 1;
		mask.acpt_a1_match_pkt = 1;
		ctrl.uc_pkt_chk_cam_match = 1;
		mask.uc_pkt_chk_cam_match = 1;
		/* check broadcast */
		ctrl.acpt_bc_pkt = 1;
		mask.acpt_bc_pkt = 1;
		ctrl.bc_pkt_chk_cam_match = 0;
		mask.bc_pkt_chk_cam_match = 1;

		if ((hal_com->chip_id == CHIP_WIFI6_8852A)
		    && (hal_com->cv == CAV)) {
			/* don't check address cam for multicast, accept all */
			ctrl.acpt_mc_pkt = 0;
			mask.acpt_mc_pkt = 1;
		} else {
			/* check multicast */
			ctrl.acpt_mc_pkt = 1;
			mask.acpt_mc_pkt = 1;
			/* black list filter */
			ctrl.mc_pkt_white_lst_mode = 0;
			mask.mc_pkt_white_lst_mode = 1;
		}
		/* check beacon */
		ctrl.bcn_chk_en = 0;
		mask.bcn_chk_en = 1;
		ctrl.bcn_chk_rule = 2; /* 2: A2&A3 match */
		mask.bcn_chk_rule = 0x3;
		/* misc */
		ctrl.acpt_pwr_mngt_pkt = 1;
		mask.acpt_pwr_mngt_pkt = 1;
		ctrl.acpt_ftm_req_pkt = 1;
		mask.acpt_ftm_req_pkt = 1;
		#endif
		break;

	case RX_FLTR_MODE_AP_NORMAL:
		#if 1
		_hal_set_dft_rxfltr(&ctrl, &mask);

		/* check unicast */
		ctrl.acpt_a1_match_pkt = 1;
		mask.acpt_a1_match_pkt = 1;
		ctrl.uc_pkt_chk_cam_match = 0;
		mask.uc_pkt_chk_cam_match = 1;

		/* check broadcast (for probe req) */
		ctrl.acpt_bc_pkt = 1;
		mask.acpt_bc_pkt = 1;
		ctrl.bc_pkt_chk_cam_match = 0;
		mask.bc_pkt_chk_cam_match = 1;

		#else
		/*
		 * SNIFFER:	   	OFF
		 * UC address CAM A1:	ON
		 * UC addr CAM match:	OFF
		 * BC accept:	     	ON
		 * BC address CAM:    	OFF
		 * Beacon check:    	OFF
		 */
		/* filter packets by address */
		ctrl.sniffer_mode = 0;
		mask.sniffer_mode = 1;
		/* Unicast
		 * Do not enable address CAM matching filtering but all A1
		 * matched ones. AP should accept all UC requests from
		 * unknown STAs.
		 */
		ctrl.acpt_a1_match_pkt = 1;
		mask.acpt_a1_match_pkt = 1;
		ctrl.uc_pkt_chk_cam_match = 0;
		mask.uc_pkt_chk_cam_match = 1;

		/* check broadcast (Probe req) */
		ctrl.acpt_bc_pkt = 1;
		mask.acpt_bc_pkt = 1;
		ctrl.bc_pkt_chk_cam_match = 0;
		mask.bc_pkt_chk_cam_match = 1;

		if ((hal_com->chip_id == CHIP_WIFI6_8852A)
		    && (hal_com->cv == CAV)) {
		    /* don't check address cam for multicast, accept all */
		    ctrl.acpt_mc_pkt = 1;
		    mask.acpt_mc_pkt = 1;
		} else {
		    /* check multicast */
		    ctrl.acpt_mc_pkt = 1;
		    mask.acpt_mc_pkt = 1;
		    /* black list filter */
		    ctrl.mc_pkt_white_lst_mode = 0;
		    mask.mc_pkt_white_lst_mode = 1;
		}

		/* check beacon */
		ctrl.bcn_chk_en = 1;
		mask.bcn_chk_en = 1;
		ctrl.bcn_chk_rule = 2; /* 2: A2&A3 match */
		mask.bcn_chk_rule = 0x3;

		/* bcn_chk_rule
		0: A3 hit
		1: A2 hit
		2: A2 & A3 hit
		3: A2 | A3 hit
		*/
		/* accept power management frame */
		ctrl.acpt_pwr_mngt_pkt = 1;
		mask.acpt_pwr_mngt_pkt = 1;
		ctrl.acpt_ftm_req_pkt = 1;
		mask.acpt_ftm_req_pkt = 1;
		#endif
		break;

	}

	err = mac->ops->set_rx_fltr_opt(mac, &ctrl, &mask, band);
	if (err)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

/**
 * rtw_hal_mac_set_rxfltr_acpt_crc_err - Accept CRC error packets or not
 * @hal_com:	pointer of struct rtw_hal_com_t
 * @band:	0x0: band0, 0x1: band1
 * @enable:	0: deny, 1: accept
 *
 * Control accepting CRC error packets or not.
 *
 * Return RTW_HAL_STATUS_SUCCESS when setting is ok.
 */
enum rtw_hal_status rtw_hal_mac_set_rxfltr_acpt_crc_err(
		struct rtw_hal_com_t *hal_com, u8 band, u8 enable)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_rx_fltr_ctrl_t ctrl = {0};
	struct mac_ax_rx_fltr_ctrl_t mask = {0};
	u32 err;


	if (enable)
		ctrl.acpt_crc32_err_pkt = 1;
	else
		ctrl.acpt_crc32_err_pkt = 0;
	mask.acpt_crc32_err_pkt = 1;

	err = mac->ops->set_rx_fltr_opt(mac, &ctrl, &mask, band);
	if (err)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

/**
 * rtw_hal_mac_set_rxfltr_mpdu_size - Set max MPDU size
 * @hal_com:	pointer of struct rtw_hal_com_t
 * @band:	0x0: band0, 0x1: band1
 * @size:	MPDU max size, unit: byte. 0 for no limit.
 *
 * MPDU size exceed Max size would be dropped.
 *
 * Return RTW_HAL_STATUS_SUCCESS when setting is ok.
 */
enum rtw_hal_status rtw_hal_mac_set_rxfltr_mpdu_size(
		struct rtw_hal_com_t *hal_com, u8 band, u16 size)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_rx_fltr_ctrl_t ctrl = {0};
	struct mac_ax_rx_fltr_ctrl_t mask = {0};
	u32 err;


	/* unit of pkt_len_fltr is 512 bytes */
	ctrl.pkt_len_fltr = size >> 9;
	ctrl.pkt_len_fltr += (size & 0x7F) ? 1 : 0;
	mask.pkt_len_fltr = 0x3F;

	err = mac->ops->set_rx_fltr_opt(mac, &ctrl, &mask, band);
	if (err)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

/**
 * rtw_hal_mac_set_rxfltr_by_type - Filter RX frame by frame type
 * @hal_com:	pointer of struct rtw_hal_com_t
 * @band:	0x0: band0, 0x1: band1
 * @type:	802.11 frame type, b'00: mgmt, b'01: ctrl, b'10: data
 * @target:	0: drop, 1: accept(driver), 2: firmware
 *
 * Set RX filter setting by 802.11 frame type and frame would be dropped or
 * forward to specific target.
 *
 * Return RTW_HAL_STATUS_SUCCESS when setting is ok, otherwise fail.
 */
enum rtw_hal_status rtw_hal_mac_set_rxfltr_by_type(
		struct rtw_hal_com_t *hal_com, u8 band, u8 type, u8 target)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum mac_ax_pkt_t ftype;
	enum mac_ax_fwd_target fwd;
	u32 err;


	switch (type) {
	case RTW_PHL_PKT_TYPE_MGNT:
		ftype = MAC_AX_PKT_MGNT;
		break;
	case RTW_PHL_PKT_TYPE_CTRL:
		ftype = MAC_AX_PKT_CTRL;
		break;
	case RTW_PHL_PKT_TYPE_DATA:
		ftype = MAC_AX_PKT_DATA;
		break;
	default:
		return RTW_HAL_STATUS_FAILURE;
	}

	if (target > MAC_AX_FWD_TO_WLAN_CPU)
		return RTW_HAL_STATUS_FAILURE;
	fwd = (enum mac_ax_fwd_target)target;

	err = mac->ops->set_rx_fltr_typ_opt(mac, ftype, fwd, band);
	if (err)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

/**
 * rtw_hal_mac_set_rxfltr_by_subtype - Filter RX frame by frame type & subtype
 * @hal_com:	pointer of struct rtw_hal_com_t
 * @band:	0x0: band0, 0x1: band1
 * @type:	802.11 frame type, b'00: mgmt, b'01: ctrl, b'10: data
 * @subtype:	802.11 frame subtype, value range is 0x00~0xFF.
 * @target:	0: drop, 1: accept(driver), 2: firmware
 *
 * Set RX filter setting by 802.11 frame type and subtype, then frame would be
 * dropped or forward to specific target.
 *
 * Return RTW_HAL_STATUS_SUCCESS when setting is ok, otherwise fail.
 */
enum rtw_hal_status rtw_hal_mac_set_rxfltr_by_subtype(
				struct rtw_hal_com_t *hal_com, u8 band,
				u8 type, u8 subtype, u8 target)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum mac_ax_pkt_t ftype;
	enum mac_ax_fwd_target fwd;
	u32 err;


	switch (type) {
	case 0:
		ftype = MAC_AX_PKT_MGNT;
		break;
	case 1:
		ftype = MAC_AX_PKT_CTRL;
		break;
	case 2:
		ftype = MAC_AX_PKT_DATA;
		break;
	default:
		return RTW_HAL_STATUS_FAILURE;
	}

	if (subtype > 0xFF)
		return RTW_HAL_STATUS_FAILURE;

	if (target > MAC_AX_FWD_TO_WLAN_CPU)
		return RTW_HAL_STATUS_FAILURE;
	fwd = (enum mac_ax_fwd_target)target;

	err = mac->ops->set_rx_fltr_typstyp_opt(mac, ftype, subtype, fwd, band);
	if (err)
		return RTW_HAL_STATUS_FAILURE;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_enable_bb_rf(struct hal_info_t *hal_info, u8 enable)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;

	mac->ops->set_hw_value(mac, MAC_AX_HW_EN_BB_RF, &enable);
	return RTW_HAL_STATUS_SUCCESS;
}

void
rtw_hal_mac_get_buffer_data(struct rtw_hal_com_t *hal_com, u32 strt_addr,
			    u8 *buf, u32 len, u32 dbg_path)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	mac_mem_dump(mac, MAC_AX_MEM_SHARED_BUF, strt_addr, buf, len, dbg_path);
}

void
rtl_hal_dump_sec_cam_tbl(struct rtw_hal_com_t *hal_com)
{
	u32 i = 0;
	u32 sec_cam_tbl_sz = 128;
	struct hal_info_t *hal = hal_com->hal_priv;
	struct mac_ax_adapter *mac =  hal_to_mac(hal);
	struct sec_cam_table_t *sec_cam_table = mac->hw_info->sec_cam_table;
	struct sec_cam_entry_t *entry = NULL;

	PHL_PRINT("===== HW Info Security CAM Table =====\n");
	PHL_PRINT("entry  valid  mac_id  key_id  key_type\n");
	for (i = 0; i < sec_cam_tbl_sz; i++) {
		entry = sec_cam_table->sec_cam_entry[i];
		if (entry == NULL)
			continue;

		PHL_PRINT("  %3d    %3d     %3d     %3d       %3d\n",
			i, entry->valid, entry->mac_id, entry->key_id, entry->key_type);
	}
}

void halmac_cmd_parser(struct hal_info_t *hal_info, char input[][MAX_ARGV],
		      u32 input_num, char *output, u32 out_len)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	mac->ops->halmac_cmd_parser(mac, input, input_num, output, out_len);
}


s32 halmac_cmd(struct hal_info_t *hal_info, char *input, char *output, u32 out_len)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	return mac->ops->halmac_cmd(mac, input, output, out_len);
}

bool rtw_hal_mac_proc_cmd(struct hal_info_t *hal_info, struct rtw_proc_cmd *incmd,
						char *output, u32 out_len)
{
	if(incmd->in_type == RTW_ARG_TYPE_BUF)
		halmac_cmd(hal_info, incmd->in.buf, output, out_len);
	else if(incmd->in_type == RTW_ARG_TYPE_ARRAY){
		halmac_cmd_parser(hal_info, incmd->in.vector,
					incmd->in_cnt_len, output, out_len);
	}

	return true;
}

static enum mac_ax_cmac_path_sel _ac_drv2mac(u8 ac, u8 wmm)
{
	enum mac_ax_cmac_path_sel sel = MAC_AX_CMAC_PATH_SEL_INVALID;


	switch (ac) {
	case 0:
		/* BE */
		sel = MAC_AX_CMAC_PATH_SEL_BE0;
		break;
	case 1:
		/* BK */
		sel = MAC_AX_CMAC_PATH_SEL_BK0;
		break;
	case 2:
		/* VI */
		sel = MAC_AX_CMAC_PATH_SEL_VI0;
		break;
	case 3:
		/* VO */
		sel = MAC_AX_CMAC_PATH_SEL_VO0;
		break;
	}

	if (sel != MAC_AX_CMAC_PATH_SEL_INVALID && wmm)
		/* wmm == 1 */
		sel += 4;

	return sel;
}

/**
 * rtw_hal_mac_set_edca() - setup WMM EDCA parameter
 * @hal_com:	struct rtw_hal_com_t *
 * @band:	0x0: band0, 0x1: band1
 * @wmm:	hardware wmm index
 * @ac:		Access Category, 0:BE, 1:BK, 2:VI, 3:VO
 * @param:	AIFS:BIT[7:0], CWMIN:BIT[11:8], CWMAX:BIT[15:12],
 *		TXOP:BIT[31:16]
 *
 * Setup WMM EDCA parameter set for specific AC.
 *
 * Return RTW_HAL_STATUS_SUCCESS when setting is ok.
 */
enum rtw_hal_status
rtw_hal_mac_set_edca(struct rtw_hal_com_t *hal_com, u8 band, u8 wmm, u8 ac,
		     u32 param)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_edca_param edca = {0};
	u32 err = 0;


	edca.band = band;
	edca.path = _ac_drv2mac(ac, wmm);
	edca.txop_32us = param  >> 16;
	edca.ecw_max = (param >> 12) & 0xF;
	edca.ecw_min = (param >> 8) & 0xF;
	edca.aifs_us = param & 0xFF;

	err = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_EDCA_PARAM, &edca);
	if (err)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

#ifdef CONFIG_PHL_TWT
enum rtw_hal_status
rtw_hal_mac_twt_info_update(void *hal, struct rtw_phl_twt_info twt_info, struct rtw_wifi_role_t *role, u8 action)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_twt_para mac_twt_para = {0};
	u32 mac_status = MACSUCCESS;

	switch (twt_info.nego_type)
	{
		case RTW_PHL_INDIV_TWT:
			mac_twt_para.nego_tp = MAC_AX_TWT_NEGO_TP_IND;
			mac_twt_para.flow_id = 0;
			break;
		case RTW_PHL_WAKE_TBTT_INR:
			mac_twt_para.nego_tp = MAC_AX_TWT_NEGO_TP_WAKE;
			mac_twt_para.flow_id = 0;
			break;
		case RTW_PHL_BCAST_TWT:
			mac_twt_para.nego_tp = MAC_AX_TWT_NEGO_TP_BRC;
			mac_twt_para.flow_id = twt_info.bcast_twt_id;
			break;
		default:
			PHL_ERR("%s : Error TWT nego type %d\n", __func__, twt_info.nego_type);
			return RTW_HAL_STATUS_FAILURE;
	}
	switch (action) {
		case TWT_CFG_ADD:
			mac_twt_para.act = MAC_AX_TWT_ACT_TP_ADD;
			break;
		case TWT_CFG_DELETE:
			mac_twt_para.act = MAC_AX_TWT_ACT_TP_DEL;
			break;
		case TWT_CFG_MODIFY:
			mac_twt_para.act = MAC_AX_TWT_ACT_TP_MOD;
			break;
		default:
			PHL_ERR("%s : Error TWT action %d\n", __func__, action);
			return RTW_HAL_STATUS_FAILURE;
	}
	mac_twt_para.trig = twt_info.trigger;
	mac_twt_para.flow_tp = twt_info.flow_type;
	mac_twt_para.proct = twt_info.twt_protection;
	mac_twt_para.id = twt_info.twt_id;
	mac_twt_para.wake_exp = twt_info.twt_wake_int_exp;
	mac_twt_para.band = role->hw_band;
	mac_twt_para.port = role->hw_port;
	mac_twt_para.rsp_pm = twt_info.responder_pm_mode;
	mac_twt_para.wake_unit = twt_info.wake_dur_unit;
	mac_twt_para.impt = twt_info.implicit_lastbcast;
	mac_twt_para.wake_man = twt_info.twt_wake_int_mantissa;
	mac_twt_para.dur = twt_info.nom_min_twt_wake_dur;
	mac_twt_para.trgt_l = twt_info.target_wake_time_l;
	mac_twt_para.trgt_h = twt_info.target_wake_time_h;
	/* HalMac API to setup/delete TWT config*/
	mac_status = mac->ops->twt_info_upd_h2c(mac, &mac_twt_para);
	if (MACSUCCESS != mac_status){
		PHL_TRACE(COMP_PHL_TWT, _PHL_DEBUG_, "rtw_hal_mac_twt_info_update(): mac_twt_info_upd_h2c fail(%d)\n",
			mac_status);
		return RTW_HAL_STATUS_FAILURE;
	}
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_twt_sta_update(void *hal, u8 macid, u8 twt_id, u8 action)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_twtact_para mac_twtact_para = {0};
	u32 mac_status = MACSUCCESS;

	switch (action) {
		case TWT_STA_ADD_MACID:
			mac_twtact_para.act = MAC_AX_TWTACT_ACT_TP_ADD;
			break;
		case TWT_STA_DEL_MACID:
			mac_twtact_para.act = MAC_AX_TWTACT_ACT_TP_DEL;
			break;
		case TWT_STA_TETMINATW_SP:
			mac_twtact_para.act = MAC_AX_TWTACT_ACT_TP_TRMNT;
			break;
		case TWT_STA_SUSPEND_TWT:
			mac_twtact_para.act = MAC_AX_TWTACT_ACT_TP_SUS;
			break;
		case TWT_STA_RESUME_TWT:
			mac_twtact_para.act = MAC_AX_TWTACT_ACT_TP_RSUM;
			break;
		default:
			PHL_ERR("%s : Error TWT action %d\n", __func__, action);
			return RTW_HAL_STATUS_FAILURE;
	}
	mac_twtact_para.macid = macid;
	mac_twtact_para.id = twt_id;
	/* Call HalMac API to setup/delete TWT STA config*/
	mac_status = mac->ops->twt_act_h2c(mac, &mac_twtact_para);
	if (MACSUCCESS != mac_status) {
		PHL_TRACE(COMP_PHL_TWT, _PHL_DEBUG_, "rtw_hal_mac_twt_sta_update(): mac_twt_act_h2c fail(%d)\n",
			mac_status);
		return RTW_HAL_STATUS_FAILURE;
	}
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_twt_sta_announce(void *hal, u8 macid)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_twtanno_para mac_twtanno_para = {0};
	u32 mac_status = MACSUCCESS;

	mac_twtanno_para.macid = macid;
	mac_status = mac->ops->twt_anno_h2c(mac, &mac_twtanno_para);
	if (MACSUCCESS != mac_status) {
		PHL_TRACE(COMP_PHL_TWT, _PHL_DEBUG_, "rtw_hal_mac_twt_sta_announce(): mac_twt_anno_h2c fail(%d)\n",
			mac_status);
		return RTW_HAL_STATUS_FAILURE;
	}
	return RTW_HAL_STATUS_SUCCESS;
}
#endif /* CONFIG_PHL_TWT */

/**
 * rtw_hal_mac_get_ampdu_cfg() - get ampdu config
 * @hal_com:    struct rtw_hal_com_t *
 * @band:       0x0: band0, 0x1: band1
 * @cfg:        struct hal_ax_ampdu_cfg *
 * Get ampdu config.
 * Return RTW_HAL_STATUS_SUCCESS when query is done.
 */
enum rtw_hal_status
rtw_hal_mac_get_ampdu_cfg(struct rtw_hal_com_t *hal_com,
                          u8 band,
                          struct mac_ax_ampdu_cfg *cfg)
{
	/* To Do: need to refine after Mac api updating*/
#if 0
     struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
     struct mac_ax_adapter *mac = hal_to_mac(hal_info);

     u32 err = 0;
     cfg->band = band;
     err = mac->ops->get_hw_value(mac, MAC_AX_HW_GET_AMPDU_CFG, cfg);
     if (err)
	     return RTW_HAL_STATUS_FAILURE;
#endif
     return RTW_HAL_STATUS_SUCCESS;
}

/*
 * rtw_hal_mac_set_rty_lmt() - setup retry limit parameter
 * @hal_com:	struct rtw_hal_com_t *
 * @macid:
 *
 * Return RTW_HAL_STATUS_SUCCESS when setting is ok.
 */
enum rtw_hal_status
rtw_hal_mac_set_rty_lmt(struct rtw_hal_com_t *hal_com, u8 macid,
	u8 rts_lmt_sel, u8 rts_lmt_val, u8 data_lmt_sel, u8 data_lmt_val)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_cctl_rty_lmt_cfg cfg = {0};
	u32 err = 0;


	cfg.macid = macid;
	cfg.data_lmt_sel = data_lmt_sel;
	cfg.data_lmt_val = data_lmt_val;
	cfg.rts_lmt_sel = rts_lmt_sel;
	cfg.rts_lmt_val = rts_lmt_val;

	err = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_CCTL_RTY_LMT, &cfg);
	if (err)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

/**
 * rtw_hal_mac_is_tx_mgnt_empty() - Get tx mgnt queue status
 * @hal_info:	struct rtw_hal_info_t *
 * @band:	0x0: band0, 0x1: band1
 * @st:		queue status, 1 for empty and 0 for not empty.
 *
 * Return RTW_HAL_STATUS_SUCCESS when setting is ok.
 */
enum rtw_hal_status
rtw_hal_mac_is_tx_mgnt_empty(struct hal_info_t *hal_info, u8 band, u8 *st)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_tx_queue_empty q_st = {{0}};
	u32 err;


	err = mac->ops->is_txq_empty(mac, &q_st);
	if (err != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	if ((band == 0) && (q_st.band0_mgnt_empty == DLE_QUEUE_EMPTY))
		*st = 1;
	else if ((band == 1) && (q_st.band1_mgnt_empty == DLE_QUEUE_EMPTY))
		*st = 1;
	else
		*st = 0;

	return RTW_HAL_STATUS_SUCCESS;
}


/* FW Sounding Command */
/* 1. NDPA Content */
void _hal_max_ax_snd_cmd_ndpa(struct mac_ax_ndpa_para *mac_ndpa,
			      struct hal_ndpa_para *hal_ndpa,
			      bool he, u8 sta_nr)
{
	u8 i = 0;
	struct hal_he_ndpa_sta_info *hal_he_sta = NULL;
	struct hal_vht_ndpa_sta_info *hal_vht_sta = NULL;

	mac_ndpa->common.frame_ctl = hal_ndpa->common.frame_ctl;
	mac_ndpa->common.duration = hal_ndpa->common.duration;
	for (i = 0; i < MAC_ALEN; i++) {
		mac_ndpa->common.addr1[i] = hal_ndpa->common.addr1[i];
		mac_ndpa->common.addr2[i] = hal_ndpa->common.addr2[i];
	}
	mac_ndpa->snd_dialog.dialog = hal_ndpa->snd_dialog.token;
	mac_ndpa->snd_dialog.he = hal_ndpa->snd_dialog.he;
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "halmac he 0x%x sta_nr %d \n", he, sta_nr);
	if (he) {
		for (i = 0; (i < sta_nr)&&(i < HAL_MAX_HE_SND_STA_NUM); i++) {
			hal_he_sta = (struct hal_he_ndpa_sta_info *)
				      &hal_ndpa->ndpa_sta_info[i];
			mac_ndpa->he_para.sta_info[i].aid = hal_he_sta->aid;
			mac_ndpa->he_para.sta_info[i].bw = hal_he_sta->bw;
			mac_ndpa->he_para.sta_info[i].fb_ng = hal_he_sta->fb_ng;
			mac_ndpa->he_para.sta_info[i].disambiguation = 1;
			mac_ndpa->he_para.sta_info[i].cb = hal_he_sta->cb;//cb nc
			mac_ndpa->he_para.sta_info[i].nc = hal_he_sta->nc;//cb nc
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "halmac cmd buf HE (%d) : aid 0x%x bw 0x%x fbng 0x%x cb 0x%x nc 0x%x\n",
				i, mac_ndpa->he_para.sta_info[i].aid,
				mac_ndpa->he_para.sta_info[i].bw,
				mac_ndpa->he_para.sta_info[i].fb_ng,
				mac_ndpa->he_para.sta_info[i].cb,
				mac_ndpa->he_para.sta_info[i].nc);
		}
	} else {
		for (i = 0; (i < sta_nr)&&(i < HAL_MAX_VHT_SND_STA_NUM); i++) {
			hal_vht_sta = (struct hal_vht_ndpa_sta_info *)
				      &hal_ndpa->ndpa_sta_info[i];
			mac_ndpa->vht_para.sta_info[i].aid =
					(u16)hal_vht_sta->aid12;
			mac_ndpa->vht_para.sta_info[i].fb_type =
					(u16)hal_vht_sta->feedback_type;
			mac_ndpa->vht_para.sta_info[i].nc =
					(u16)hal_vht_sta->nc;
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "halmac cmd buf VHT(%d) : aid 0x%x fb 0x%x nc 0x%x\n",
				i,
				mac_ndpa->vht_para.sta_info[i].aid,
				mac_ndpa->vht_para.sta_info[i].fb_type,
				mac_ndpa->vht_para.sta_info[i].nc);
		}
	}
}
/* 2-1. BFRP Content - VHT */
void _hal_max_ax_snd_cmd_bfrp_vht(struct mac_ax_bfrp_para *mac_bfrp,
				 struct hal_bfrp_para *hal_bfrp, u8 bfrp_num)
{
	struct mac_ax_vht_bfrp_para *mac_vht_bfrp = NULL;
	struct hal_bfrp_vht *hal_vht_bfrp = NULL;
	u8 i = 0, j = 0;
	for (i = 0; (i <= bfrp_num)&&(i < HAL_MAX_VHT_BFRP_NUM); i++){
		mac_bfrp->hdr[i].frame_ctl = hal_bfrp->hdr[i].frame_ctl;
		mac_bfrp->hdr[i].duration = hal_bfrp->hdr[i].duration;
		for (j = 0; j < MAC_ALEN; j++) {
			mac_bfrp->hdr[i].addr1[j] = hal_bfrp->hdr[i].addr1[j];
			mac_bfrp->hdr[i].addr2[j] = hal_bfrp->hdr[i].addr2[j];
		}
		mac_vht_bfrp = &mac_bfrp->vht_para[i];
		hal_vht_bfrp = &hal_bfrp->vht_para[i];
		mac_vht_bfrp->retransmission_bitmap = hal_vht_bfrp->rexmit_bmp;
	}
}
/* 2-2. BFRP Content - HE */
void _hal_max_ax_snd_cmd_bfrp_he(struct mac_ax_bfrp_para *mac_bfrp,
			      struct hal_bfrp_para *hal_bfrp,
			      u8 num_1, u8 num_2)
{
	u8 i = 0;
	struct mac_ax_he_bfrp_para *mac_he_bfrp = NULL;
	struct hal_bfrp_he *hal_he_bfrp = NULL;

	/* BFRP-0 */
	if (num_1) {
		mac_bfrp->hdr[0].frame_ctl = hal_bfrp->hdr[0].frame_ctl;
		mac_bfrp->hdr[0].duration = hal_bfrp->hdr[0].duration;
		for (i = 0; i < MAC_ALEN; i++) {
			mac_bfrp->hdr[0].addr1[i] = hal_bfrp->hdr[0].addr1[i];
			mac_bfrp->hdr[0].addr2[i] = hal_bfrp->hdr[0].addr2[i];
		}
		mac_he_bfrp = &mac_bfrp->he_para[0];
		hal_he_bfrp = &hal_bfrp->he_para[0];
		mac_he_bfrp->common.tgr_info = hal_he_bfrp->common.tgr_info;
		mac_he_bfrp->common.ul_len = hal_he_bfrp->common.ul_len;
		mac_he_bfrp->common.more_tf = hal_he_bfrp->common.more_tf;
		mac_he_bfrp->common.cs_rqd = hal_he_bfrp->common.cs_rqd;
		mac_he_bfrp->common.ul_bw = hal_he_bfrp->common.ul_bw;
		mac_he_bfrp->common.gi_ltf = hal_he_bfrp->common.gi_ltf;
		mac_he_bfrp->common.mimo_ltfmode =
					hal_he_bfrp->common.mimo_ltfmode;
		mac_he_bfrp->common.num_heltf = hal_he_bfrp->common.num_heltf;
		mac_he_bfrp->common.ul_pktext = hal_he_bfrp->common.ul_pktext;
		mac_he_bfrp->common.ul_stbc = hal_he_bfrp->common.ul_stbc;
		mac_he_bfrp->common.ldpc_extra_sym =
					hal_he_bfrp->common.ldpc_extra_sym;
		mac_he_bfrp->common.dplr = hal_he_bfrp->common.dplr;
		mac_he_bfrp->common.ap_tx_pwr = hal_he_bfrp->common.ap_tx_pwr;
		mac_he_bfrp->common.ul_sr  = hal_he_bfrp->common.ul_sr;
		mac_he_bfrp->common.ul_siga2_rsvd =
					hal_he_bfrp->common.ul_siga2_rsvd;
		for( i = 0; (i < num_1)&&(i < HAL_MAX_HE_BFRP_STA_NUM); i++) {
			mac_he_bfrp->user[i].aid12 =
					hal_he_bfrp->user[i].aid12;
			mac_he_bfrp->user[i].fbseg_rexmit_bmp =
					hal_he_bfrp->fbseg_rexmit_bmp[i];
			mac_he_bfrp->user[i].ru_pos =
					hal_he_bfrp->user[i].ru_pos;
			mac_he_bfrp->user[i].ss_alloc =
					hal_he_bfrp->user[i].ss_alloc;
			mac_he_bfrp->user[i].ul_dcm =
					hal_he_bfrp->user[i].ul_dcm;
			mac_he_bfrp->user[i].ul_fec_code =
					hal_he_bfrp->user[i].ul_fec_code;
			mac_he_bfrp->user[i].ul_mcs =
					hal_he_bfrp->user[i].ul_mcs;
			mac_he_bfrp->user[i].ul_tgt_rssi =
					hal_he_bfrp->user[i].ul_tgt_rssi;
		}
	}
	/* BFRP - 1 */
	if (num_2) {
		mac_bfrp->hdr[1].frame_ctl = hal_bfrp->hdr[1].frame_ctl;
		mac_bfrp->hdr[1].duration = hal_bfrp->hdr[1].duration;
		for (i = 0; i < MAC_ALEN; i++) {
			mac_bfrp->hdr[1].addr1[i] = hal_bfrp->hdr[1].addr1[i];
			mac_bfrp->hdr[1].addr2[i] = hal_bfrp->hdr[1].addr2[i];
		}
		mac_he_bfrp = &mac_bfrp->he_para[1];
		hal_he_bfrp = &hal_bfrp->he_para[1];
		mac_he_bfrp->common.tgr_info = hal_he_bfrp->common.tgr_info;
		mac_he_bfrp->common.ul_len = hal_he_bfrp->common.ul_len;
		mac_he_bfrp->common.more_tf = hal_he_bfrp->common.more_tf;
		mac_he_bfrp->common.cs_rqd = hal_he_bfrp->common.cs_rqd;
		mac_he_bfrp->common.ul_bw = hal_he_bfrp->common.ul_bw;
		mac_he_bfrp->common.gi_ltf = hal_he_bfrp->common.gi_ltf;
		mac_he_bfrp->common.mimo_ltfmode =
					hal_he_bfrp->common.mimo_ltfmode;
		mac_he_bfrp->common.num_heltf = hal_he_bfrp->common.num_heltf;
		mac_he_bfrp->common.ul_pktext = hal_he_bfrp->common.ul_pktext;
		mac_he_bfrp->common.ul_stbc = hal_he_bfrp->common.ul_stbc;
		mac_he_bfrp->common.ldpc_extra_sym =
					hal_he_bfrp->common.ldpc_extra_sym;
		mac_he_bfrp->common.dplr = hal_he_bfrp->common.dplr;
		mac_he_bfrp->common.ap_tx_pwr = hal_he_bfrp->common.ap_tx_pwr;
		mac_he_bfrp->common.ul_sr  = hal_he_bfrp->common.ul_sr;
		mac_he_bfrp->common.ul_siga2_rsvd =
					hal_he_bfrp->common.ul_siga2_rsvd;
		for( i = 0; (i < num_1)&&(i < HAL_MAX_HE_BFRP_STA_NUM); i++) {
			mac_he_bfrp->user[i].aid12 =
					hal_he_bfrp->user[i].aid12;
			mac_he_bfrp->user[i].fbseg_rexmit_bmp =
					hal_he_bfrp->fbseg_rexmit_bmp[i];
			mac_he_bfrp->user[i].ru_pos =
					hal_he_bfrp->user[i].ru_pos;
			mac_he_bfrp->user[i].ss_alloc =
					hal_he_bfrp->user[i].ss_alloc;
			mac_he_bfrp->user[i].ul_dcm =
					hal_he_bfrp->user[i].ul_dcm;
			mac_he_bfrp->user[i].ul_fec_code =
					hal_he_bfrp->user[i].ul_fec_code;
			mac_he_bfrp->user[i].ul_mcs =
					hal_he_bfrp->user[i].ul_mcs;
			mac_he_bfrp->user[i].ul_tgt_rssi =
					hal_he_bfrp->user[i].ul_tgt_rssi;
		}
	}
}

/* 2-3 he bfrp f2p cmd */
void _hal_max_ax_snd_cmd_bfrp_he_f2p(struct mac_ax_snd_f2P *mac_bfrp_f2p,
				     struct hal_bfrp_para *hal_bfrp,
				     u8 num_1, u8 num_2)
{
	struct hal_bfrp_he *hal_he_bfrp = NULL;
	if (num_1) {
		hal_he_bfrp = &hal_bfrp->he_para[0];
		mac_bfrp_f2p[0].csi_len_bfrp =
			hal_he_bfrp->f2p_info.csi_len_bfrp;
		mac_bfrp_f2p[0].tb_t_pe_bfrp =
			hal_he_bfrp->f2p_info.tb_t_pe_bfrp;
		mac_bfrp_f2p[0].tri_pad_bfrp =
			hal_he_bfrp->f2p_info.tri_pad_bfrp;
		mac_bfrp_f2p[0].ul_cqi_rpt_tri_bfrp =
			hal_he_bfrp->f2p_info.ul_cqi_rpt_tri_bfrp;
		mac_bfrp_f2p[0].rf_gain_idx_bfrp =
			hal_he_bfrp->f2p_info.rf_gain_idx_bfrp;
		mac_bfrp_f2p[0].fix_gain_en_bfrp =
			hal_he_bfrp->f2p_info.fix_gain_en_bfrp;
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "==> _hal_max_ax_snd_cmd_bfrp_he_f2p[0] \n");
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[0].csi_len_bfrp = 0x%x \n",
			  mac_bfrp_f2p[0].csi_len_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[0].tb_t_pe_bfrp = 0x%x \n",
			  mac_bfrp_f2p[0].tb_t_pe_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[0].tri_pad_bfrp = 0x%x \n",
			  mac_bfrp_f2p[0].tri_pad_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[0].ul_cqi_rpt_tri_bfrp = 0x%x \n",
			  mac_bfrp_f2p[0].ul_cqi_rpt_tri_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[0].rf_gain_idx_bfrp = 0x%x \n",
			  mac_bfrp_f2p[0].rf_gain_idx_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[0].fix_gain_en_bfrp = 0x%x \n",
			  mac_bfrp_f2p[0].fix_gain_en_bfrp);
	}
	if (num_2) {
		hal_he_bfrp = &hal_bfrp->he_para[1];
		mac_bfrp_f2p[1].csi_len_bfrp =
			hal_he_bfrp->f2p_info.csi_len_bfrp;
		mac_bfrp_f2p[1].tb_t_pe_bfrp =
			hal_he_bfrp->f2p_info.tb_t_pe_bfrp;
		mac_bfrp_f2p[1].tri_pad_bfrp =
			hal_he_bfrp->f2p_info.tri_pad_bfrp;
		mac_bfrp_f2p[1].ul_cqi_rpt_tri_bfrp =
			hal_he_bfrp->f2p_info.ul_cqi_rpt_tri_bfrp;
		mac_bfrp_f2p[1].rf_gain_idx_bfrp =
			hal_he_bfrp->f2p_info.rf_gain_idx_bfrp;
		mac_bfrp_f2p[1].fix_gain_en_bfrp =
			hal_he_bfrp->f2p_info.fix_gain_en_bfrp;
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "==> _hal_max_ax_snd_cmd_bfrp_he_f2p[1] \n");
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[1].csi_len_bfrp = 0x%x \n",
			  mac_bfrp_f2p[1].csi_len_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[1].tb_t_pe_bfrp = 0x%x \n",
			  mac_bfrp_f2p[1].tb_t_pe_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[1].tri_pad_bfrp = 0x%x \n",
			  mac_bfrp_f2p[1].tri_pad_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[1].ul_cqi_rpt_tri_bfrp = 0x%x \n",
			  mac_bfrp_f2p[1].ul_cqi_rpt_tri_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[1].rf_gain_idx_bfrp = 0x%x \n",
			  mac_bfrp_f2p[1].rf_gain_idx_bfrp);
		PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_,
			  "mac_bfrp_f2p[1].fix_gain_en_bfrp = 0x%x \n",
			  mac_bfrp_f2p[1].fix_gain_en_bfrp);
	}
}


/* 3. WD Content */
void _hal_max_ax_snd_cmd_wd(struct mac_ax_snd_wd_para *mac_wd,
			    struct hal_snd_wd_para *hal_wd)
{
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "==> _hal_max_ax_snd_cmd_wd \n");
	mac_wd->txpktsize = hal_wd->txpktsize;
	mac_wd->ndpa_duration = hal_wd->ndpa_duration;
	mac_wd->datarate = hal_wd->datarate;
	mac_wd->macid = hal_wd->macid;
	mac_wd->force_txop = hal_wd->force_txop;
	mac_wd->data_bw = hal_wd->data_bw;
	mac_wd->gi_ltf = hal_wd->gi_ltf;
	mac_wd->data_er = hal_wd->data_er;
	mac_wd->data_dcm = hal_wd->data_dcm;
	mac_wd->data_stbc = hal_wd->data_stbc;
	mac_wd->data_ldpc = hal_wd->data_ldpc;
	mac_wd->data_bw_er = hal_wd->data_bw_er;
	mac_wd->multiport_id = hal_wd->multiport_id;
	mac_wd->mbssid = hal_wd->mbssid;

	mac_wd->signaling_ta_pkt_sc = hal_wd->signaling_ta_pkt_sc;
	mac_wd->sw_define = hal_wd->sw_define;
	mac_wd->txpwr_ofset_type = hal_wd->txpwr_ofset_type;
	mac_wd->lifetime_sel = hal_wd->lifetime_sel;
	mac_wd->stf_mode = hal_wd->stf_mode;
	mac_wd->disdatafb = hal_wd->disdatafb;
	mac_wd->data_txcnt_lmt_sel = hal_wd->data_txcnt_lmt_sel;
	mac_wd->data_txcnt_lmt = hal_wd->data_txcnt_lmt;
	mac_wd->sifs_tx = hal_wd->sifs_tx;
	mac_wd->snd_pkt_sel = hal_wd->snd_pkt_sel;
	mac_wd->ndpa = hal_wd->ndpa;

	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->txpktsize = 0x%x \n", mac_wd->txpktsize);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->ndpa_duration = 0x%x \n", mac_wd->ndpa_duration);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->datarate = 0x%x \n", mac_wd->datarate);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "mac_wd->macid = 0x%x \n", mac_wd->macid);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->force_txop = 0x%x \n", mac_wd->force_txop);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->data_bw = 0x%x \n", mac_wd->data_bw);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->gi_ltf = 0x%x \n", mac_wd->gi_ltf);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->data_er = 0x%x \n", mac_wd->data_er);

	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->data_dcm = 0x%x \n", mac_wd->data_dcm);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->data_stbc = 0x%x \n", mac_wd->data_stbc);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->data_ldpc = 0x%x \n", mac_wd->data_ldpc);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->data_bw_er = 0x%x \n", mac_wd->data_bw_er);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->multiport_id = 0x%x \n", mac_wd->multiport_id);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->mbssid = 0x%x \n", mac_wd->mbssid);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->signaling_ta_pkt_sc = 0x%x \n", mac_wd->signaling_ta_pkt_sc);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->sw_define = 0x%x \n", mac_wd->sw_define);

	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->txpwr_ofset_type = 0x%x \n", mac_wd->txpwr_ofset_type);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->lifetime_sel = 0x%x \n", mac_wd->lifetime_sel);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->stf_mode = 0x%x \n", mac_wd->stf_mode);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->disdatafb = 0x%x \n", mac_wd->disdatafb);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->data_txcnt_lmt_sel = 0x%x \n", mac_wd->data_txcnt_lmt_sel);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->data_txcnt_lmt = 0x%x \n", mac_wd->data_txcnt_lmt);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_DEBUG_, "mac_wd->sifs_tx = 0x%x \n", mac_wd->sifs_tx);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "mac_wd->snd_pkt_sel = 0x%x \n", mac_wd->snd_pkt_sel);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "mac_wd->ndpa = 0x%x \n", mac_wd->ndpa);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "<== _hal_max_ax_snd_cmd_wd \n");
}


enum rtw_hal_status
hal_mac_ax_send_fw_snd(struct hal_info_t *hal_info,
		       struct hal_ax_fwcmd_snd *hal_cmd)
{
	enum rtw_hal_status hstatus = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_fwcmd_snd snd_cmd = {0};
	u8 i = 0, he = 0, sta_nr = 0;

	/* 1. COMMAND Common */
	snd_cmd.frexgtype = hal_cmd->frame_ex_type;
	snd_cmd.bfrp0_user_num = hal_cmd->bfrp0_sta_nr;
	snd_cmd.bfrp1_user_num = hal_cmd->bfrp1_sta_nr;
	for (i = 0; i < HAL_MAX_HE_SND_STA_NUM; i++) {
		snd_cmd.macid[i] = (u8)hal_cmd->macid[i];
	}

	he = (hal_cmd->frame_ex_type >= HAL_FEXG_TYPE_AX_SU) ? 1 : 0;

	if (hal_cmd->frame_ex_type == HAL_FEXG_TYPE_AX_SU) {
		sta_nr = 1;
	} else {
		sta_nr = he ? (hal_cmd->bfrp0_sta_nr + hal_cmd->bfrp1_sta_nr) :
			 (hal_cmd->frame_ex_type - HAL_FEXG_TYPE_AC_SU + 1);
	}

	/* 2. NDPA Content */
	_hal_max_ax_snd_cmd_ndpa(&snd_cmd.pndpa, &hal_cmd->ndpa,
			         he, sta_nr);
	/* 3. BFRP Content */
	if (he) {
		_hal_max_ax_snd_cmd_bfrp_he(&snd_cmd.pbfrp, &hal_cmd->bfrp,
				hal_cmd->bfrp0_sta_nr, hal_cmd->bfrp1_sta_nr);
		_hal_max_ax_snd_cmd_bfrp_he_f2p(snd_cmd.f2p, &hal_cmd->bfrp,
				hal_cmd->bfrp0_sta_nr, hal_cmd->bfrp1_sta_nr);
	} else {
		_hal_max_ax_snd_cmd_bfrp_vht(&snd_cmd.pbfrp, &hal_cmd->bfrp,
					     (sta_nr - 1));
	}
	/* 4. WD Content */
	/* 4-1 NDPA */
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "NDPA WD: \n");
	_hal_max_ax_snd_cmd_wd(&snd_cmd.wd[0], &hal_cmd->wd[0]);
	/* 4-2 NDP */
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "NDP WD: \n");
	_hal_max_ax_snd_cmd_wd(&snd_cmd.wd[1], &hal_cmd->wd[1]);
	/* 4-3 BFRP*/
	if (he) {
		if (hal_cmd->bfrp0_sta_nr) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "HE BFRP-1 WD: \n");
			_hal_max_ax_snd_cmd_wd(&snd_cmd.wd[2],
					       &hal_cmd->wd[2]);
		}
		if (hal_cmd->bfrp1_sta_nr) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "HE BFRP-2 WD: \n");
			_hal_max_ax_snd_cmd_wd(&snd_cmd.wd[3],
					       &hal_cmd->wd[3]);
		}
	} else {
		for (i = 0; i < (sta_nr - 1)&&(i < HAL_MAX_VHT_BFRP_NUM);
		     i++) {
			PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "VHT BFRP %d WD: \n", i);
			_hal_max_ax_snd_cmd_wd(&snd_cmd.wd[2 + i],
					       &hal_cmd->wd[2 + i]);
		}
	}

	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "[fw_snd] snd_cmd.macid[0] = 0x%x \n", snd_cmd.macid[0]);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "[fw_snd] snd_cmd.bfrp0_user_num = 0x%x \n", snd_cmd.bfrp0_user_num);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "[fw_snd] snd_cmd.bfrp1_user_num = 0x%x \n", snd_cmd.bfrp1_user_num);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "[fw_snd] snd_cmd.mode = 0x%x \n", snd_cmd.mode);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "[fw_snd] snd_cmd.frexgtype = 0x%x \n", snd_cmd.frexgtype);
	hstatus = mac->ops->set_snd_para(mac, &snd_cmd);
	PHL_TRACE(COMP_PHL_SOUND, _PHL_INFO_, "[fw_snd] hal_mac_ax_send_fw_snd hstatus = 0x%x\n", hstatus);

	return hstatus;
}



enum rtw_hal_status
rtw_hal_mac_tx_mode_sel(struct hal_info_t *hal_info, u8 fw_tx, u8 txop_wmm_en_bm)
{
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;

	struct mac_ax_mac_tx_mode_sel mode_sel = {0};

	if (fw_tx)
		mode_sel.sw_mode_band0_en = 1;
	if (txop_wmm_en_bm & BIT(0))
		mode_sel.txop_rot_wmm0_en = 1;
	if (txop_wmm_en_bm & BIT(1))
		mode_sel.txop_rot_wmm1_en = 1;
	if (txop_wmm_en_bm & BIT(2))
		mode_sel.txop_rot_wmm2_en = 1;
	if (txop_wmm_en_bm & BIT(3))
		mode_sel.txop_rot_wmm3_en = 1;

	mac->ops->tx_mode_sel(mac, &mode_sel);
	return RTW_HAL_STATUS_SUCCESS;
}

u32 rtw_hal_mac_process_c2h(void *hal, struct rtw_c2h_info *c2h)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	struct mac_ax_ccxrpt mac_ccx = {0};
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
#ifdef RTW_WKARD_CCX_RPT_LIMIT_CTRL
	u32 retry_limit = 32;
#endif
	u32 retid = 0;

	hal_status = mac->ops->get_c2h_event(mac, c2h, (enum phl_msg_evt_id *)&retid, (u8 *)&mac_ccx);
	/*PHL_INFO("%s, hal_status=%d, retid=%d\n", __func__, hal_status, retid);*/

	if (hal_status == RTW_HAL_STATUS_SUCCESS && (retid == MSG_EVT_CCX_REPORT_TX_OK || retid == MSG_EVT_CCX_REPORT_TX_FAIL)) {
		#if 0
		PHL_INFO("%s, mac_ccx.tx_state=%d\n", __func__, mac_ccx.tx_state);
		PHL_INFO("%s, mac_ccx.sw_define=%d\n", __func__, mac_ccx.sw_define);
		PHL_INFO("%s, mac_ccx.macid=%d\n", __func__,  mac_ccx.macid);
		PHL_INFO("%s, mac_ccx.pkt_ok_num=%d\n", __func__, mac_ccx.pkt_ok_num);
		PHL_INFO("%s, mac_ccx.data_txcnt=%d\n", __func__, mac_ccx.data_txcnt);
		#endif

#ifdef RTW_WKARD_CCX_RPT_LIMIT_CTRL
		if (hal_info->hal_com->spe_pkt_cnt_lmt)
			retry_limit = hal_info->hal_com->spe_pkt_cnt_lmt;

		if (retid == MSG_EVT_CCX_REPORT_TX_FAIL && mac_ccx.data_txcnt != retry_limit)
			retid = 0;
#endif
	}
	return retid;
}

enum rtw_hal_status
rtw_hal_mac_f2p_test_cmd(struct hal_info_t *hal_info,
						struct mp_mac_ax_f2p_test_para *info,
						struct mp_mac_ax_f2p_wd *f2pwd,
						struct mp_mac_ax_f2p_tx_cmd *ptxcmd,
						u8 *psigb_addr)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 ret = RTW_HAL_STATUS_FAILURE;

	if (mac->ops->f2p_test_cmd(mac, (void*)info, (void*)f2pwd, (void*)ptxcmd, psigb_addr) == MACSUCCESS)
		ret = RTW_HAL_STATUS_SUCCESS;

	return ret;
}

enum rtw_hal_status
rtw_hal_mac_write_pwr_ofst_mode(struct rtw_hal_com_t *hal_com, u8 band)
{

	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;

	if (tpu->normal_mode_lock_en)
		return RTW_HAL_STATUS_FAILURE;

	mac->ops->write_pwr_ofst_mode(mac, band, tpu);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_write_pwr_ofst_bw(struct rtw_hal_com_t *hal_com, u8 band)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;

	if (tpu->normal_mode_lock_en)
		return RTW_HAL_STATUS_FAILURE;

	mac->ops->write_pwr_ofst_bw(mac, band, tpu);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_write_pwr_ref_reg(struct rtw_hal_com_t *hal_com, u8 band)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;

	if (tpu->normal_mode_lock_en)
		return RTW_HAL_STATUS_FAILURE;

	mac->ops->write_pwr_ref_reg(mac, band, tpu);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_write_pwr_limit_en(struct rtw_hal_com_t *hal_com, u8 band)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;

	if (tpu->normal_mode_lock_en)
		return RTW_HAL_STATUS_FAILURE;

	mac->ops->write_pwr_limit_en(mac, band, tpu);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_set_pwr_lmt_en_val(struct rtw_hal_com_t *hal_com, u8 band, bool en_val)
{
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;

	if (tpu->normal_mode_lock_en)
		return RTW_HAL_STATUS_FAILURE;

	tpu->pwr_lmt_en = en_val;

	return RTW_HAL_STATUS_SUCCESS;
}

bool
rtw_hal_mac_get_pwr_lmt_en_val(struct rtw_hal_com_t *hal_com, u8 band)
{

	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;

	return tpu->pwr_lmt_en;
}

enum rtw_hal_status
rtw_hal_mac_set_tpu_mode(struct rtw_hal_com_t *hal_com,
			 enum rtw_tpu_op_mode op_mode_new, u8 band)
{
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;

	if (op_mode_new == TPU_DBG_MODE) {
		tpu->op_mode = TPU_DBG_MODE;
		tpu->normal_mode_lock_en = true;
	} else {
		tpu->op_mode = TPU_NORMAL_MODE;
		tpu->normal_mode_lock_en = false;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_write_pwr_limit_rua_reg(struct rtw_hal_com_t *hal_com, u8 band)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;

	if (tpu->normal_mode_lock_en)
		return RTW_HAL_STATUS_FAILURE;

	mac->ops->write_pwr_limit_rua_reg(mac, band, tpu);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_write_pwr_limit_reg(struct rtw_hal_com_t *hal_com, u8 band)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;
	struct rtw_tpu_pwr_imt_info *lmt = &tpu->rtw_tpu_pwr_imt_i;

	if (tpu->normal_mode_lock_en)
		return RTW_HAL_STATUS_FAILURE;

	mac->ops->write_pwr_limit_reg(mac, band, lmt);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_write_pwr_by_rate_reg(struct rtw_hal_com_t *hal_com, u8 band)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct rtw_tpu_info *tpu = &hal_com->band[band].rtw_tpu_i;
	struct rtw_tpu_pwr_by_rate_info *by_rate = &tpu->rtw_tpu_pwr_by_rate_i;

	if (tpu->normal_mode_lock_en)
		return RTW_HAL_STATUS_FAILURE;

	mac->ops->write_pwr_by_rate_reg(mac, band, by_rate);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_log_efuse_bt_size(struct rtw_hal_com_t *hal_com, u32 *val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->get_hw_value(mac,
		MAC_AX_HW_GET_BT_LOGICAL_EFUSE_SIZE, val) != MACSUCCESS){
		PHL_ERR("%s: Get bt efuse size fail!\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}
	PHL_INFO("%s: BT Efuse log size = %d!\n", __FUNCTION__, *val);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_efuse_bt_mask_size(struct rtw_hal_com_t *hal_com, u32 *val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if(mac->ops->get_hw_value(mac,
	MAC_AX_HW_GET_BT_EFUSE_MASK_SIZE, val) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s: bt efuse mask size = %d\n", __FUNCTION__, *val);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_write_log_efuse_bt_map(struct rtw_hal_com_t *hal_com,
							u8 *map,
							u32 map_size,
							u8 *mask,
							u32 mask_size)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_pg_efuse_info info;
	enum rtw_hal_status status = RTW_HAL_STATUS_EFUSE_PG_FAIL;
	u8 *tmp_map = NULL;
	u8 *tmp_mask = NULL;

	tmp_map = _os_mem_alloc(hal_com->drv_priv, map_size);
	if(tmp_map == NULL) {
		PHL_WARN("%s: Allocate pg map buffer fail!\n", __FUNCTION__);
		status = RTW_HAL_STATUS_RESOURCE;
		goto err_mem_tmp_map;
	}

	tmp_mask = _os_mem_alloc(hal_com->drv_priv, mask_size);
	if(tmp_mask == NULL) {
		PHL_WARN("%s: Allocate pg mask buffer fail!\n", __FUNCTION__);
		status = RTW_HAL_STATUS_RESOURCE;
		goto err_mem_tmp_mask;
	}

	/* Copy efuse map to tmp_map buffer */
	_os_mem_cpy(hal_com->drv_priv, tmp_map, map, map_size);

	/* Copy efuse mask to tmp_mask buffer */
	_os_mem_cpy(hal_com->drv_priv, tmp_mask, mask, mask_size);


	info.efuse_map = tmp_map;
	info.efuse_map_size = map_size;
	info.efuse_mask = tmp_mask;
	info.efuse_mask_size= mask_size;

	if (mac->ops->pg_efuse_by_map_bt(mac,
					&info,
					MAC_AX_EFUSE_R_DRV) != MACSUCCESS) {
		PHL_INFO("%s: BT PG Fail!\n", __FUNCTION__);
		status = RTW_HAL_STATUS_EFUSE_PG_FAIL;
	}
	else {
		PHL_INFO("%s: BT PG ok!\n", __FUNCTION__);
		status = RTW_HAL_STATUS_SUCCESS;
	}
	_os_mem_free(hal_com->drv_priv, tmp_map, map_size);
	_os_mem_free(hal_com->drv_priv, tmp_mask, mask_size);

	return status;

err_mem_tmp_mask:
	_os_mem_free(hal_com->drv_priv, tmp_mask, mask_size);

err_mem_tmp_map:
	_os_mem_free(hal_com->drv_priv, tmp_map, map_size);

	return status;
}

enum rtw_hal_status
rtw_hal_mac_read_log_efuse_bt_map(struct rtw_hal_com_t *hal_com, u8 *map)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->dump_log_efuse_bt(mac,
			MAC_AX_EFUSE_PARSER_MAP,
			#ifdef RTW_WKARD_EFUSE_OPERATION
			MAC_AX_EFUSE_R_DRV,
			#else
			MAC_AX_EFUSE_R_AUTO,
			#endif
			map
			) != MACSUCCESS) {
		PHL_INFO("%s: Dump bt logical efuse fail!\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}

	PHL_INFO("%s: Dump bt logical efuse ok!\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_efuse_bt_avl(struct rtw_hal_com_t *hal_com, u32 *val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->get_efuse_avl_size_bt(mac, val) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_get_efuse_bt_size(struct rtw_hal_com_t *hal_com, u32 *val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->get_hw_value(mac,
		MAC_AX_HW_GET_BT_EFUSE_SIZE, val) != MACSUCCESS){
		PHL_ERR("%s: Get efuse bt size fail!\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}
	PHL_INFO("%s: Efuse size = %d!\n", __FUNCTION__, *val);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_set_mu_edca(struct rtw_hal_com_t *hal_com, u8 band, u8 ac,
	u16 timer, u8 cw_min, u8 cw_max, u8 aifs)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_muedca_param edca = {0};
	u32 err = 0;

	edca.band = band;
	edca.ac = ac;
	edca.aifs_us = aifs;
	edca.ecw_min = cw_min;
	edca.ecw_max = cw_max;
	edca.muedca_timer_32us = timer;
	err = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_MUEDCA_PARAM, &edca);
	if (err)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_set_mu_edca_ctrl(struct rtw_hal_com_t *hal_com,
	u8 band, u8 wmm, u8 set)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_muedca_cfg cfg = {0};
	u32 err = 0;

	cfg.band = band;
	cfg.wmm_sel = wmm;
	cfg.countdown_en = set;
	cfg.tb_update_en = set;
	err = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_MUEDCA_CTRL, &cfg);
	if (err)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_led_set_ctrl_mode(struct hal_info_t *hal_info,
						  enum mac_ax_led_mode mode,
						  u8 led_id)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->set_led_mode(mac, mode, led_id) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_led_ctrl(struct hal_info_t *hal_info, u8 high,
					 u8 led_id)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->led_ctrl(mac, high, led_id) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_sw_gpio_ctrl(struct hal_info_t *hal_info, u8 high,
					 u8 gpio)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->sw_gpio_ctrl(mac, high, gpio) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_set_sw_gpio_mode(struct hal_info_t *hal_info, enum rtw_gpio_mode mode,
					 u8 gpio)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->set_sw_gpio_mode(mac, mode, gpio) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_get_wl_dis_val(struct hal_info_t *hal_info, u8 *val)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->get_wl_dis_val(mac, val) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_pcie_trx_mit(struct hal_info_t *hal_info,
			 struct mac_ax_pcie_trx_mitigation *mit_info)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (MACSUCCESS !=
	    mac->ops->set_hw_value(mac, MAX_AX_HW_PCIE_MIT, mit_info))
		return RTW_HAL_STATUS_FAILURE;

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_read_efuse_bt_hidden(struct rtw_hal_com_t *hal_com, u32 addr, u32 size, u8 *val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	if (mac->ops->read_efuse(mac, addr, size, val, MAC_AX_EFUSE_BANK_BT) != MACSUCCESS)
		return RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_write_efuse_bt_hidden(struct rtw_hal_com_t *hal_com, u32 addr, u8 val)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	u8 tmp_value;

	if (mac->ops->read_efuse(mac, addr, 1, &tmp_value, MAC_AX_EFUSE_BANK_BT) != MACSUCCESS) {
		PHL_INFO("%s read bt efuse hideen block fail.\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}

	if(tmp_value != 0xFF) {
		PHL_INFO("%s bt efuse hidden offset = 0x%x has value = 0x%x.\n", __FUNCTION__, addr, tmp_value);
		return RTW_HAL_STATUS_FAILURE;
	}

	if (mac->ops->write_efuse(mac, addr, val, MAC_AX_EFUSE_BANK_BT) != MACSUCCESS) {
		PHL_INFO("%s write bt efuse hideen block fail.\n", __FUNCTION__);
		return RTW_HAL_STATUS_FAILURE;
	}

	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_tsf_sync(struct hal_info_t *hal_info,
						u8 from_port, u8 to_port, enum phl_band_idx band,
						s32 sync_offset_tu, enum hal_tsf_sync_act action)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum mac_ax_tsf_sync_act mac_action = MAC_AX_TSF_SYNC_NOW_ONCE;
	s32 sync_offset_unit = 0;/* for halmac API use, unit is 32us  */

	switch (action){
	case HAL_TSF_SYNC_NOW_ONCE :
		mac_action = MAC_AX_TSF_SYNC_NOW_ONCE;
		break;
	case HAL_TSF_EN_SYNC_AUTO :
		mac_action = MAC_AX_TSF_EN_SYNC_AUTO;
		break;
	case HAL_TSF_DIS_SYNC_AUTO :
		mac_action = MAC_AX_TSF_DIS_SYNC_AUTO;
		break;
	default :
		PHL_ERR("Unknown tsf sync action %d\n", action);
		goto _error;
	}

	/* covert TU to unit(unit is 32us, 1TU=1024us=32*32us) */
	sync_offset_unit = sync_offset_tu * 32;

	if (mac->ops->tsf_sync(mac, from_port, to_port,
		sync_offset_unit, mac_action) == MACSUCCESS)
		return RTW_HAL_STATUS_SUCCESS;
_error:
	return RTW_HAL_STATUS_FAILURE;
}

/**
 * rtw_hal_mac_get_sec_cam() - get the security cam raw data from HW
 * @hal_info:		struct hal_info_t *
 * @num:		How many cam you wnat to dump from the first one.
 * @buf:		ptr to buffer which store the content from HW.
 *			If buf is NULL, use console as debug path.
 * @size		Size of allocated memroy for @buf.
 *			The size should be @num * size of security cam offset(0x20).
 *
 * Return RTW_HAL_STATUS_SUCCESS when function successfully works,
 * otherwise, return RTW_HAL_STATUS_FAILURE.
 */
enum rtw_hal_status
rtw_hal_mac_get_sec_cam(struct hal_info_t *hal_info, u16 num, u8 *buf, u16 size)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	int i = 0;
	/* ToDO: fix the magic number later */
	u8 sec_cam_offset_sz = 0x20;

	if (buf == NULL) {
		/* to console */
		for (i = 0; i < num; i++) {
			PHL_INFO("======= SEC CAM (%d)DUMP  =======\n", i);
			mac_mem_dump(mac, MAC_AX_MEM_SECURITY_CAM, i*sec_cam_offset_sz\
				, NULL, sec_cam_offset_sz, 0);
			PHL_INFO("\n");
		}
	} else {
		/* to buffer */
		if (size < sec_cam_offset_sz*num) {
			PHL_ERR("%s buf size is not enough to dump security cam\n", __func__);
			return RTW_HAL_STATUS_FAILURE;
		}

		for (i = 0; i < num; i++)
			mac_mem_dump(mac, MAC_AX_MEM_SECURITY_CAM, i*sec_cam_offset_sz\
				, buf + (i*sec_cam_offset_sz), sec_cam_offset_sz, 1);
	}
	return RTW_HAL_STATUS_SUCCESS;
}

/**
 * rtw_hal_mac_get_addr_cam() - get the address cam raw data from HW
 * @hal_info:		struct hal_info_t *
 * @num:		How many cam you wnat to dump from the first one.
 * @buf:		ptr to buffer which store the content from HW.
 *			If buf is NULL, use console as debug path.
 * @size		Size of allocated memroy for @buf.
 *			The size should be @num * size of Addr cam offset(0x40).
 *
 * Return RTW_HAL_STATUS_SUCCESS when function successfully works,
 * otherwise, return RTW_HAL_STATUS_FAILURE.
 */
enum rtw_hal_status
rtw_hal_mac_get_addr_cam(struct hal_info_t *hal_info, u16 num, u8 *buf, u16 size)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	int i = 0;
	/* ToDO: fix the magic number later */
	u8 addr_cam_offset_sz = 0x40;

	if (buf == NULL) {
		/* to console */
		for (i = 0; i < num; i++) {
			PHL_INFO("======= ADDR CAM (%d)DUMP  =======\n", i);
			mac_mem_dump(mac, MAC_AX_MEM_ADDR_CAM, i*addr_cam_offset_sz\
				, NULL, addr_cam_offset_sz, 0);
			PHL_INFO("\n");
		}
	} else {
		/* to buffer */
		if (size < addr_cam_offset_sz*num) {
			PHL_ERR("%s buf size is not enough to dump addr cam\n", __func__);
			return RTW_HAL_STATUS_FAILURE;
		}

		for (i = 0; i < num; i++)
			mac_mem_dump(mac, MAC_AX_MEM_ADDR_CAM, i*addr_cam_offset_sz\
				, buf + (i*addr_cam_offset_sz), addr_cam_offset_sz, 1);

	}
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_get_tsf(struct hal_info_t *hal, u8 *port,
					u32 *tsf_h, u32 *tsf_l)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	struct mac_ax_port_tsf val = {0};

	val.port = *port;
	if (hal_mac_ops->get_hw_value(mac, MAC_AX_HW_GET_TSF, &val))
		return RTW_HAL_STATUS_FAILURE;
	*tsf_h = val.tsf_h;
	*tsf_l = val.tsf_l;
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mac_cfg_txhci(struct hal_info_t *hal, u8 en)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	enum mac_ax_func_sw mac_en;
	u32 ret = 0;

	if (en)
		mac_en = MAC_AX_FUNC_EN;
	else
		mac_en = MAC_AX_FUNC_DIS;

	ret = hal_mac_ops->intf_ops->ctrl_txhci(mac, mac_en);

	return (ret == MACSUCCESS) ?
			(RTW_HAL_STATUS_SUCCESS):(RTW_HAL_STATUS_FAILURE);
}

enum rtw_hal_status rtw_hal_mac_cfg_rxhci(struct hal_info_t *hal, u8 en)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_ops *hal_mac_ops = mac->ops;
	enum mac_ax_func_sw mac_en;
	u32 ret = 0;

	if (en)
		mac_en = MAC_AX_FUNC_EN;
	else
		mac_en = MAC_AX_FUNC_DIS;

	ret = hal_mac_ops->intf_ops->ctrl_rxhci(mac, mac_en);

	return (ret == MACSUCCESS) ?
			(RTW_HAL_STATUS_SUCCESS):(RTW_HAL_STATUS_FAILURE);
}

#ifdef CONFIG_MCC_SUPPORT
void _hal_mac_mcc_fill_role_info(struct rtw_phl_mcc_role *mcc_role,
				struct mac_ax_mcc_role *info)
{
	struct rtw_phl_mcc_policy_info *policy = &mcc_role->policy;

	if (mcc_role->bt_role) {
		info->group = mcc_role->group;
		info->btc_in_2g = true;
		info->duration = policy->dur_info.dur;
	} else {
		info->macid = mcc_role->macid;
		info->central_ch_seg0 = mcc_role->chandef->center_ch;
		info->central_ch_seg1 = (u8)mcc_role->chandef->center_freq2;
		info->primary_ch = mcc_role->chandef->chan;
		info->bandwidth = mcc_role->chandef->bw;
		info->group = mcc_role->group;
		info->c2h_rpt = policy->c2h_rpt;
		info->dis_tx_null = policy->dis_tx_null;
		info->dis_sw_retry = policy->dis_sw_retry;
		info->in_curr_ch = policy->in_curr_ch;
		info->sw_retry_count = policy->sw_retry_count;
		info->tx_null_early= policy->tx_null_early;
		info->duration = policy->dur_info.dur;
		info->courtesy_en = policy->courtesy_en;
		info->courtesy_num = policy->courtesy_num;
		info->courtesy_target = policy->courtesy_target;
	}
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_role_info(): macid(%d), central_ch_seg0(%d), central_ch_seg1(%d)\n",
		info->macid, info->central_ch_seg0, info->central_ch_seg1);
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_role_info(): primary_ch(%d), bandwidth(%d), group(%d), c2h_rpt(%d)\n",
		info->primary_ch, info->bandwidth, info->group, info->c2h_rpt);
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_role_info(): dis_tx_null(%d), dis_sw_retry(%d), in_curr_ch(%d)\n",
		info->dis_tx_null, info->dis_sw_retry, info->in_curr_ch);
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_role_info(): sw_retry_count(%d), tx_null_early(%d), btc_in_2g(%d)\n",
		info->sw_retry_count, info->tx_null_early, info->btc_in_2g);
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_role_info(): duration(%d)\n",
		info->duration);
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_role_info(): courtesy_en(%d), courtesy_num(%d), courtesy_target(0x%x)\n",
		info->courtesy_en, info->courtesy_num, info->courtesy_target);
}

void _hal_mac_mcc_fill_duration_info(struct rtw_phl_mcc_en_info *en_info,
					struct rtw_phl_mcc_bt_info *bt_info,
					struct mac_ax_mcc_duration_info *info)
{
	info->group = en_info->mcc_role[0].group;
	info->btc_in_group = (bt_info->bt_dur > 0) ? 1 : 0;
	info->start_macid = en_info->mcc_role[en_info->ref_role_idx].macid;
	info->macid_x = en_info->mcc_role[0].macid;
	info->macid_y = en_info->mcc_role[1].macid;
	info->duration_x = en_info->mcc_role[0].policy.dur_info.dur;
	info->duration_y = en_info->mcc_role[1].policy.dur_info.dur;
	info->start_tsf_low = en_info->tsf_low;
	info->start_tsf_high = en_info->tsf_high;
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_duration_info(): info->group(%d), btc_in_group(%d), start_macid(%d), macid_x(%d), macid_y(%d)\n",
		info->group, info->btc_in_group, info->start_macid,
		info->macid_x, info->macid_y);
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_duration_info(): duration_x(%d), duration_y(%d), start_tsf(0x%08x %08x)\n",
		info->duration_x, info->duration_y, info->start_tsf_high,
		info->start_tsf_low);
}

void _hal_mac_mcc_fill_start_info(u8 group, u8 macid, u32 tsf_high, u32 tsf_low,
			u8 btc_in_group, u8 old_group_action, u8 old_group,
			struct mac_ax_mcc_start *info)
{
	info->group = group;
	info->btc_in_group = btc_in_group;
	info->old_group_action = old_group_action;
	info->old_group = old_group;
	info->macid = macid;
	info->tsf_low = tsf_low;
	info->tsf_high = tsf_high;
	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "_hal_mac_mcc_fill_start_info(): info->group(%d), btc_in_group(%d), old_group_action(%d), old_group(%d), macid(%d), Tsf(0x%08x %08x)\n",
		info->group, info->btc_in_group, info->old_group_action,
		info->old_group, info->macid, info->tsf_high, info->tsf_low);
}

enum rtw_hal_status rtw_hal_mac_add_mcc(struct hal_info_t *hal,
					struct rtw_phl_mcc_role *mcc_role)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;
	u16 loop_cnt = 0;
	struct mac_ax_mcc_role info = {0};

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, ">>> rtw_hal_mac_add_mcc()\n");
	if (mac == NULL)
		goto exit;
	_hal_mac_mcc_fill_role_info(mcc_role, &info);
	mac_status = mac->ops->add_mcc(mac, &info);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_add_mcc(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}
	do {
		if (mac->ops->check_add_mcc_done(mac, (u8)info.group) ==
								MACSUCCESS)
			break;
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if (loop_cnt < POLLING_HALMAC_CNT) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_mac_add_mcc(): polling ok, count(%d)\n",
			  loop_cnt);
		hal_status = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_add_mcc(): polling timeout\n");
	}
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_start_mcc(struct hal_info_t *hal,
				u8 group, u8 macid, u32 tsf_high, u32 tsf_low,
				u8 btc_in_group, u8 old_group_action, u8 old_group)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_mcc_start info = {0};
	u32 mac_status = MACSUCCESS;
	u16 loop_cnt = 0;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, ">>> rtw_hal_mac_start_mcc(): group(%d), macid(%d) Start tsf(0x%08X %08X), btc_in_group(%d)\n",
		group, macid, tsf_high, tsf_low, btc_in_group);
	if (mac == NULL)
		goto exit;
	_hal_mac_mcc_fill_start_info(group, macid, tsf_high, tsf_low,
					btc_in_group, old_group_action,
					old_group, &info);
	mac_status = mac->ops->start_mcc(mac, &info);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_start_mcc(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}
	do {
		if (mac->ops->check_start_mcc_done(mac, group) ==
								MACSUCCESS)
			break;
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if (loop_cnt < POLLING_HALMAC_CNT) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_mac_start_mcc(): polling ok, count(%d)\n",
			  loop_cnt);
		hal_status = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_start_mcc(): polling timeout\n");
	}
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_stop_mcc(struct hal_info_t *hal, u8 group,
					u8 macid)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;
	u16 loop_cnt = 0;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, ">>> rtw_hal_mac_stop_mcc(): group(%d), macid(%d)\n",
		group, macid);
	if (mac == NULL)
		goto exit;
	/*prev_groups always set to 1, driver stop all group pattern in the same hw band.*/
	mac_status = mac->ops->stop_mcc(mac, group, macid, 1);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_stop_mcc(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}
	do {
		if (mac->ops->check_stop_mcc_done(mac, group) ==
								MACSUCCESS)
			break;
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if (loop_cnt < POLLING_HALMAC_CNT) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_mac_stop_mcc(): polling ok, count(%d)\n",
			  loop_cnt);
		hal_status = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_stop_mcc(): polling timeout\n");
	}
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_del_mcc_group(struct hal_info_t *hal, u8 group)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;
	u16 loop_cnt = 0;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, ">>> rtw_hal_mac_del_mcc_group(): group(%d)\n",
		group);
	if (mac == NULL)
		goto exit;
	/*prev_groups always set to 1, driver stop all group pattern in the same hw band.*/
	mac_status = mac->ops->del_mcc_group(mac, group, 1);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_del_mcc_group(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}

	do {
		if (mac->ops->check_del_mcc_group_done(mac, group) ==
								MACSUCCESS)
			break;
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if (loop_cnt < POLLING_HALMAC_CNT) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_mac_del_mcc_group(): polling ok, count(%d)\n",
			  loop_cnt);
		hal_status = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_del_mcc_group(): polling timeout\n");
	}
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_mcc_request_tsf(struct hal_info_t *hal,
					u8 group, u8 macid_x, u8 macid_y)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;
	u16 loop_cnt = 0;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, ">>> rtw_hal_mac_mcc_request_tsf(): group(%d), macid_x(%d), macid_y(%d)\n",
		group, macid_x, macid_y);
	if (mac == NULL)
		goto exit;
	mac_status = mac->ops->mcc_request_tsf(mac, group, macid_x, macid_y);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_mcc_request_tsf(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}

	do {
		if (mac->ops->check_mcc_request_tsf_done(mac, group) ==
								MACSUCCESS)
			break;
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if (loop_cnt < POLLING_HALMAC_CNT) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_mac_mcc_request_tsf(): polling ok, count(%d)\n",
			  loop_cnt);
		hal_status = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_mcc_request_tsf(): polling timeout\n");
	}
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_mcc_macid_bitmap(struct hal_info_t *hal,
					u8 group, u8 macid, u8 *bitmap, u8 len)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;
	u16 loop_cnt = 0;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, ">>> rtw_hal_mac_mcc_macid_bitmap(): group(%d), macid(%d)\n",
		group, macid);
	if (mac == NULL)
		goto exit;
	mac_status = mac->ops->mcc_macid_bitmap(mac, group, macid, bitmap, len);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_mcc_macid_bitmap(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}

	do {
		if (mac->ops->check_mcc_macid_bitmap_done(mac, group) ==
								MACSUCCESS)
			break;
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if (loop_cnt < POLLING_HALMAC_CNT) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_mac_mcc_macid_bitmap(): polling ok, count(%d)\n",
			  loop_cnt);
		hal_status = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_mcc_macid_bitmap(): polling timeout\n");
	}
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_mcc_sync_enable(struct hal_info_t *hal,
				u8 group, u8 source, u8 target, u8 offset)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;
	u16 loop_cnt = 0;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, ">>> rtw_hal_mac_mcc_sync_enable(): group(%d), source macid(%d), target macid(%d), offset(%d)\n",
		group, source, target, offset);
	if (mac == NULL)
		goto exit;
	mac_status = mac->ops->mcc_sync_enable(mac, group, source, target, offset);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "mac_mcc_sync_enable(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}
	do {
		if (mac->ops->check_mcc_sync_enable_done(mac, group) ==
								MACSUCCESS)
			break;
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if (loop_cnt < POLLING_HALMAC_CNT) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "mac_mcc_sync_enable(): polling ok, count(%d)\n",
			  loop_cnt);
		hal_status = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "mac_mcc_sync_enable(): polling timeout\n");
	}
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_set_duration(struct hal_info_t *hal,
					struct rtw_phl_mcc_en_info *en_info,
					struct rtw_phl_mcc_bt_info *bt_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_mcc_duration_info info = {0};
	u32 mac_status;
	u16 loop_cnt = 0;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, ">>> rtw_hal_mac_set_duration()\n");
	_hal_mac_mcc_fill_duration_info(en_info, bt_info, &info);
	mac_status = mac->ops->mcc_set_duration(mac, &info);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_set_duration(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}
	do {
		if (mac->ops->check_mcc_set_duration_done(mac, (u8)info.group)
								== MACSUCCESS)
			break;
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
		loop_cnt++;
	} while (loop_cnt < POLLING_HALMAC_CNT);

	if (loop_cnt < POLLING_HALMAC_CNT) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_mac_set_duration(): polling ok, count(%d)\n",
			  loop_cnt);
		hal_status = RTW_HAL_STATUS_SUCCESS;
	} else {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_set_duration(): polling timeout\n");
	}
exit:
	return hal_status;

}

enum rtw_hal_status rtw_hal_mac_get_mcc_tsf_rpt(struct hal_info_t *hal,
					u8 group, u32 *tsf_x_h, u32 *tsf_x_l,
					u32 *tsf_y_h, u32 *tsf_y_l)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;

	if (mac == NULL)
		goto exit;
	mac_status = mac->ops->get_mcc_tsf_rpt(mac, group, tsf_x_h, tsf_x_l,
						tsf_y_h, tsf_y_l);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_get_mcc_tsf_rpt(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}
	hal_status = RTW_HAL_STATUS_SUCCESS;
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_get_mcc_status_rpt(struct hal_info_t *hal,
				u8 group, u8 *status, u32 *tsf_h, u32 *tsf_l)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;

	if (mac == NULL)
		goto exit;
	mac_status = mac->ops->get_mcc_status_rpt(mac, group, status, tsf_h,
						tsf_l);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "mac_mcc_sync_enable(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}
	hal_status = RTW_HAL_STATUS_SUCCESS;
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mac_get_mcc_group(struct hal_info_t *hal, u8 *group)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status;

	if (mac == NULL)
		goto exit;
	mac_status = mac->ops->get_mcc_group(mac, group);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MCC, _PHL_ERR_, "rtw_hal_mac_get_mcc_group(): fault, status = %d.\n",
			  mac_status);
		goto exit;
	}
	hal_status = RTW_HAL_STATUS_SUCCESS;
exit:
	return hal_status;
}
#endif /* CONFIG_MCC_SUPPORT */

#ifdef CONFIG_PHL_P2PPS
#define P2P_ACT_INIT 0
#define P2P_ACT_UPDATE 1
#define P2P_ACT_REMOVE 2
#define P2P_ACT_TERMINATE 3

#define P2P_TYPE_GO 0
#define P2P_TYPE_GC 1

void _hal_mac_dump_p2p_act_struct(struct mac_ax_p2p_act_info *mac_p2p_info)
{
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): macid = %d\n",
		mac_p2p_info->macid);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): noaid = %d\n",
		mac_p2p_info->noaid);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): act = %d\n",
		mac_p2p_info->act);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): type = %d\n",
		mac_p2p_info->type);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): all_slep = %d\n",
		mac_p2p_info->all_slep);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): srt = %x\n",
		mac_p2p_info->srt);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): itvl = %d\n",
		mac_p2p_info->itvl);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): dur = %d\n",
		mac_p2p_info->dur);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): cnt = %d\n",
		mac_p2p_info->cnt);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_dump_p2p_act_struct(): ctw = %d\n",
		mac_p2p_info->ctw);
}

void _hal_mac_noa_fill_info(u8 action,
	struct rtw_phl_noa_desc *desc,
	u16 macid,
	struct mac_ax_p2p_act_info *mac_p2p_info)
{
	struct rtw_wifi_role_t *wrole = desc->w_role;

	mac_p2p_info->macid = (u8)macid;
	mac_p2p_info->noaid = desc->noa_id;
	mac_p2p_info->act = action;
	if (wrole->type == PHL_RTYPE_AP)
		mac_p2p_info->type = P2P_TYPE_GO;
	else if (wrole->type == PHL_RTYPE_STATION)
		mac_p2p_info->type = P2P_TYPE_GC;
	mac_p2p_info->srt = desc->start_t_l;
	mac_p2p_info->itvl = desc->interval;
	mac_p2p_info->dur = desc->duration;
	mac_p2p_info->cnt = desc->count;
}

enum rtw_hal_status _hal_mac_get_p2p_stat(struct hal_info_t *hal)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u16 loop_cnt;
	u32 m_stat;

	for (loop_cnt = 0; loop_cnt < POLLING_HALMAC_CNT; loop_cnt++) {
		m_stat = mac->ops->get_p2p_stat(mac) ;
		if (m_stat == MACSUCCESS) {
			PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]_hal_mac_get_p2p_stat(): polling ok, count(%d)\n",
				loop_cnt);
			return RTW_HAL_STATUS_SUCCESS;
		} else if (m_stat == MACP2PSTFAIL) {
			PHL_TRACE(COMP_PHL_P2PPS, _PHL_ERR_, "[NoA]_hal_mac_get_p2p_stat(): polling error, count(%d)\n",
				loop_cnt);
			return RTW_HAL_STATUS_FAILURE;
		}
		_os_sleep_ms(hal_to_drvpriv(hal), POLLING_HALMAC_TIME);
	}
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_ERR_, "[NoA]_hal_mac_get_p2p_stat(): polling timeout!\n");
	return RTW_HAL_STATUS_FAILURE;
}


enum rtw_hal_status _hal_mac_set_p2p_act(struct hal_info_t *hal,
	struct mac_ax_p2p_act_info *mac_p2p_info)
{
	enum rtw_hal_status h_stat = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 m_stat;

	m_stat = mac->ops->p2p_act_h2c(mac, mac_p2p_info);
	if (m_stat != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_P2PPS, _PHL_ERR_, "[NoA]_hal_mac_set_noa_act(): Fault, status = %d, Action = %d\n",
				m_stat, mac_p2p_info->act);
		return RTW_HAL_STATUS_MAC_API_FAILURE;
	}
	h_stat = _hal_mac_get_p2p_stat(hal);
	return h_stat;
}

enum rtw_hal_status rtw_hal_mac_noa_init(struct hal_info_t *hal,
	struct rtw_phl_noa_info *noa_info,
	struct rtw_phl_noa_desc *in_desc,
	u16 macid)
{
	enum rtw_hal_status h_stat = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_p2p_act_info mac_p2p_info = {0};

	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]rtw_hal_mac_noa_init()\n");
	_hal_mac_noa_fill_info(P2P_ACT_INIT, in_desc, macid, &mac_p2p_info);
	_hal_mac_dump_p2p_act_struct(&mac_p2p_info);
	h_stat = _hal_mac_set_p2p_act(hal, &mac_p2p_info);
	return h_stat;
}

enum rtw_hal_status rtw_hal_mac_noa_update(struct hal_info_t *hal,
	struct rtw_phl_noa_info *noa_info,
	struct rtw_phl_noa_desc *in_desc,
	u16 macid)
{
	enum rtw_hal_status h_stat = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_p2p_act_info mac_p2p_info = {0};

	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]rtw_hal_mac_noa_update()\n");
	_hal_mac_noa_fill_info(P2P_ACT_UPDATE, in_desc, macid, &mac_p2p_info);
	_hal_mac_dump_p2p_act_struct(&mac_p2p_info);
	h_stat = _hal_mac_set_p2p_act(hal, &mac_p2p_info);
	return h_stat;
}

enum rtw_hal_status rtw_hal_mac_noa_remove(struct hal_info_t *hal,
	struct rtw_phl_noa_info *noa_info,
	struct rtw_phl_noa_desc *in_desc,
	u16 macid)
{
	enum rtw_hal_status h_stat = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_p2p_act_info mac_p2p_info = {0};

	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]rtw_hal_mac_noa_remove()\n");
	_hal_mac_noa_fill_info(P2P_ACT_REMOVE, in_desc, macid, &mac_p2p_info);
	_hal_mac_dump_p2p_act_struct(&mac_p2p_info);
	h_stat = _hal_mac_set_p2p_act(hal, &mac_p2p_info);
	return h_stat;

}

enum rtw_hal_status rtw_hal_mac_noa_terminate(struct hal_info_t *hal,
	struct rtw_phl_noa_info *noa_info,
	struct rtw_phl_noa_desc *in_desc,
	u16 macid)
{
	enum rtw_hal_status h_stat = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_p2p_act_info mac_p2p_info = {0};

	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]rtw_hal_mac_noa_terminate()\n");
	_hal_mac_noa_fill_info(P2P_ACT_TERMINATE, in_desc, macid, &mac_p2p_info);
	_hal_mac_dump_p2p_act_struct(&mac_p2p_info);
	h_stat = _hal_mac_set_p2p_act(hal, &mac_p2p_info);
	return h_stat;

}

enum rtw_hal_status rtw_hal_mac_tsf32_tog_enable(struct hal_info_t *hal,
	u8 hw_band,
	u8 port,
	u16 early)
{
	enum rtw_hal_status h_stat = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_t32_togl_info mac_t32_tog_info = {0};
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 m_stat;

	mac_t32_tog_info.band = hw_band;
	mac_t32_tog_info.port = port;
	mac_t32_tog_info.en = true;
	mac_t32_tog_info.early = early;
	m_stat = mac->ops->tsf32_togl_h2c(mac, &mac_t32_tog_info);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]rtw_hal_mac_tsf32_tog_enable() m_stat = %d\n",
		m_stat);
	if(m_stat == MACSUCCESS)
		h_stat = RTW_HAL_STATUS_SUCCESS;
	return h_stat;
}

enum rtw_hal_status rtw_hal_mac_tsf32_tog_disable(struct hal_info_t *hal,
	u8 hw_band,
	u8 port)
{
	enum rtw_hal_status h_stat = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_t32_togl_info mac_t32_tog_info = {0};
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 m_stat;

	mac_t32_tog_info.band = hw_band;
	mac_t32_tog_info.port = port;
	mac_t32_tog_info.en = false;
	m_stat = mac->ops->tsf32_togl_h2c(mac, &mac_t32_tog_info);
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]rtw_hal_mac_tsf32_tog_disable() m_stat = %d\n",
		m_stat);
	if(m_stat == MACSUCCESS)
		h_stat = RTW_HAL_STATUS_SUCCESS;
	return h_stat;
}

enum rtw_hal_status rtw_hal_mac_get_tsf32_tog_rpt(struct hal_info_t *hal,
	struct rtw_phl_tsf32_tog_rpt *rpt)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_t32_togl_rpt m_rpt = {0};
	u32 m_stat = 0;
	PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]rtw_hal_mac_get_tsf32_tog_rpt()\n");
	m_stat = mac->ops->get_t32_togl_rpt(mac, &m_rpt);
	if (m_stat != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_P2PPS, _PHL_INFO_, "[NoA]rtw_hal_mac_get_tsf32_tog_rpt() MAC FAIL(%d)!\n",
			m_stat);
		return RTW_HAL_STATUS_FAILURE;
	}
	rpt->band = m_rpt.band;
	rpt->port = m_rpt.port;
	rpt->valid = m_rpt.valid;
	rpt->early = m_rpt.early;
	rpt->status = m_rpt.status;
	rpt->tsf_l = m_rpt.tsf_l;
	rpt->tsf_h = m_rpt.tsf_h;
	return RTW_HAL_STATUS_SUCCESS;
}
#endif
void rtw_hal_mac_notification(struct hal_info_t *hal_info,
                              enum phl_msg_evt_id event,
                              u8 band)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	mac->ops->event_notify(mac, event, band);
}

void rtw_hal_mac_cmd_notification(struct hal_info_t *hal_info,
                              void *hal_cmd,
                              u8 band)
{
	/*todo*/
}

enum rtw_hal_status
rtw_hal_mac_trigger_fw_conflict(struct hal_info_t *hal_info, u32 addr, u8 vol)
{
	struct hal_ops_t *hal_ops = hal_get_ops(hal_info);
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 i = 0;
	u32 convert_mask = 0xffffffff;
	u32 val;

	/* Switch voltage */
	mac->ops->set_hw_value(mac, MAC_AX_HW_SET_CORE_SWR_VOLT, &vol);

	/* Trigger method: H2C Halt */
	hal_ops->write_macreg(hal_info, 0x168, convert_mask, 0x5dc0007);
	hal_ops->write_macreg(hal_info, 0x160, convert_mask, 0x1);

	/* loop read reg */
	for(i = 0; i<1000; i++){
		val = hal_ops->read_macreg(hal_info,
					    addr,
					    convert_mask);
		PHL_INFO("%s: voltag %d count %d io_value = %x\n", __FUNCTION__,vol, i, val);
	}

	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status
rtw_hal_mac_set_gpio_func(struct rtw_hal_com_t *hal_com, u8 func, s8 gpio_cfg){
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	struct mac_ax_ops *ops = mac->ops;
	enum rtw_hal_status ret = RTW_HAL_STATUS_SUCCESS;

	ret = ops->set_gpio_func(mac, func, gpio_cfg);

	if (ret != MACSUCCESS)
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "Set GPIO failure, status = %d\n", ret);

	return ret;
}

enum rtw_hal_status
rtw_hal_mac_bfee_set_vht_gid(struct hal_info_t *hal,
			     u8 band, struct rtw_phl_gid_pos_tbl *tbl)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct mac_gid_pos gid_pos = {0};
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	u32 mac_status = 0;
	u8 i = 0;

	gid_pos.band = band;
	/* For GID 0~31 */
	for (i = 0; i < 4; i++) {
		gid_pos.gid_tab[0] |= (tbl->gid_vld[i] << (i << 3));
	}

	for (i = 0; i < 8; i++) {
		if (i < 4)
			gid_pos.user_pos[0] |= (tbl->pos[i] << (i << 3));
		else
			gid_pos.user_pos[1] |= (tbl->pos[i] << ((i - 4) << 3));
	}
	/* For GID 32~64 */
	for (i = 4; i < 8; i++) {
		gid_pos.gid_tab[1] |= (tbl->gid_vld[i] << ((i - 4) << 3));
	}
	for (i = 8; i < 16; i++) {
		if (i < 12)
			gid_pos.user_pos[2] |= (tbl->pos[i] << ((i - 8) << 3));
		else
			gid_pos.user_pos[3] |= (tbl->pos[i] << ((i - 12) << 3));
	}
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s() : Set VHT GID for band %d;\n",
		  __func__, band);
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s() : band %d ; gid tbl 0x%x 0x%x;\n",
		  __func__, band,
		  gid_pos.gid_tab[0], gid_pos.gid_tab[1]);
	PHL_TRACE(COMP_PHL_DBG, _PHL_INFO_, "%s() : user position 0x%x 0x%x 0x%x 0x%x;\n",
		  __func__,
		  gid_pos.user_pos[0], gid_pos.user_pos[1],
		  gid_pos.user_pos[2], gid_pos.user_pos[3]);

	mac_status = mac->ops->gidpos(mac, &gid_pos);
	if (mac_status != MACSUCCESS) {
		hal_status = RTW_HAL_STATUS_FAILURE;
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s() : fail, status = %d.\n",
			  __func__, mac_status);
	}

	return hal_status;
}

/* acq_val/mgq_val , input unit : us */
enum rtw_hal_status
rtw_hal_mac_set_tx_lifetime(struct hal_info_t *hal, enum phl_band_idx band,
			    bool acq_en, bool mgq_en, u16 acq_val, u16 mgq_val)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_lifetime_cfg cfg = {0};
	u32 mac_status = 0;
	u16 tmp = 0;
#define HAL_MAC_TX_LIFETIME_UNIT_US_SHT 8 /* 256 us */

	if (HW_BAND_1 == band)
		cfg.band = 1;
	else
		cfg.band = 0;

	if(true == acq_en) {
		cfg.en.acq_en = 1;
		tmp = acq_val >> HAL_MAC_TX_LIFETIME_UNIT_US_SHT;
		cfg.val.acq_val_1 = tmp;
		cfg.val.acq_val_2 = tmp;
		cfg.val.acq_val_3 = tmp;
		cfg.val.acq_val_4 = tmp;
	}

	if (true == mgq_en) {
		cfg.en.mgq_en = 1;
		tmp = mgq_val >> HAL_MAC_TX_LIFETIME_UNIT_US_SHT;
		cfg.val.mgq_val = tmp;
	}

	mac_status = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_LIFETIME_CFG, (void *)&cfg);

	if (mac_status != MACSUCCESS) {
		hal_status = RTW_HAL_STATUS_FAILURE;
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s() : fail, status = %d.\n",
			  __func__, mac_status);
	}

	return hal_status;
}

#ifdef CONFIG_FW_IO_OFLD_SUPPORT
enum rtw_hal_status rtw_hal_mac_add_cmd_ofld(struct rtw_hal_com_t *hal_com, struct rtw_mac_cmd *cmd)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u32 mac_status;

	if (mac == NULL)
		goto exit;
	mac_status = mac->ops->add_cmd_ofld(mac, cmd);
	if (mac_status != MACSUCCESS) {
		PHL_TRACE(COMP_PHL_MAC, _PHL_ERR_, "%s(): fault, status = %d.\n",
			 __func__, mac_status);
		goto exit;
	}
	hal_status = RTW_HAL_STATUS_SUCCESS;
exit:
	return hal_status;
}
#endif


enum rtw_hal_status rtw_hal_mac_set_hw_rts_th(struct hal_info_t *hal, u8 band,
					      u16 time_th, u16 len_th)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal);
	struct mac_ax_hw_rts_th hw_rts_th = {0};
	u32 mac_err;

	PHL_INFO("%s\n", __func__);
	_os_mem_set(hal_to_drvpriv(hal), &hw_rts_th, 0, sizeof(hw_rts_th));

	hw_rts_th.band = band;
	hw_rts_th.time_th = time_th;
	hw_rts_th.len_th = len_th;

	mac_err = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_HW_RTS_TH,
					 &hw_rts_th);

	if (mac_err != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, mac_err);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

u8 rtw_hal_mac_get_efuse_ver_len(struct rtw_hal_com_t *hal_com)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);

	return mac->hw_info->efuse_version_size;
}


enum rtw_hal_status
rtw_hal_mac_set_dfs_tb_ctrl(struct hal_info_t *hal_info, u8 set)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	u8 dis_ru_26 = 0;
	enum rtw_hal_status ret = RTW_HAL_STATUS_SUCCESS;

	if (set)
		dis_ru_26 = 1;
	ret = mac->ops->set_hw_value(mac, MAC_AX_HW_SET_TX_RU26_TB, &dis_ru_26);
	if (ret != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, ret);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_mac_patch_rx_rate(struct hal_info_t *hal_info, struct rtw_r_meta_data *mdata)
{
	struct mac_ax_adapter *mac = hal_to_mac(hal_info);
	enum rtw_hal_status ret = RTW_HAL_STATUS_SUCCESS;

	ret = mac->ops->patch_rx_rate(mac, mdata);
	if (ret != MACSUCCESS) {
		PHL_ERR("%s : mac status %d.\n", __func__, ret);
		return RTW_HAL_STATUS_FAILURE;
	}

	return RTW_HAL_STATUS_SUCCESS;
}

/**
 * rtw_hal_mac_set_tx_duty() - Set tx pause/un-pause interval
 * @hal_info: struct hal_info_t*
 * @pause_interval: tx pause interval (ms)
 * @tx_interval: tx interval (ms)
 *
 * Return RTW_HAL_STATUS_SUCCESS when operation success.
 */
enum rtw_hal_status
rtw_hal_mac_set_tx_duty(struct hal_info_t *hal_info,
			u16 pause_interval,
			u16 tx_interval)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 mac_status = 0;

	mac_status = mac->ops->tx_duty(mac, pause_interval, tx_interval);
	if (mac_status != MACSUCCESS) {
		hal_status = RTW_HAL_STATUS_FAILURE;
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s() : fail, status = %d.\n",
			  __func__, mac_status);
	}

	return hal_status;
}

/**
 * rtw_hal_mac_stop_tx_duty() - Stop previous tx duty config
 * @hal_info: struct hal_info_t*
 *
 * Return RTW_HAL_STATUS_SUCCESS when operation success.
 */
enum rtw_hal_status
rtw_hal_mac_stop_tx_duty(struct hal_info_t *hal_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;
	struct mac_ax_adapter *mac = (struct mac_ax_adapter *)hal_info->mac;
	u32 mac_status = 0;

	mac_status = mac->ops->tx_duty_stop(mac);
	if (mac_status != MACSUCCESS) {
		hal_status = RTW_HAL_STATUS_FAILURE;
		PHL_TRACE(COMP_PHL_DBG, _PHL_ERR_, "%s() : fail, status = %d.\n",
			  __func__, mac_status);
	}

	return hal_status;
}