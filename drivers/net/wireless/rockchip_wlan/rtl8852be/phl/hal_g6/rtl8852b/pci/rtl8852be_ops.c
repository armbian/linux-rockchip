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
#define _RTL8852BE_OPS_C_
#include "../rtl8852b_hal.h"
#include "rtl8852be.h"

void hal_set_ops_8852be(struct rtw_phl_com_t *phl_com,
				struct hal_info_t *hal)
{
	struct hal_ops_t *ops = hal_get_ops(hal);

	hal_set_ops_8852b(phl_com, hal);

	ops->init_hal_spec = init_hal_spec_8852be;
	ops->hal_get_efuse = hal_get_efuse_8852be;
	ops->hal_init = hal_init_8852be;
	ops->hal_deinit = hal_deinit_8852be;
	ops->hal_start = hal_start_8852be;
	ops->hal_stop = hal_stop_8852be;
#ifdef CONFIG_WOWLAN
	ops->hal_wow_init = hal_wow_init_8852be;
	ops->hal_wow_deinit = hal_wow_deinit_8852be;
#endif /* CONFIG_WOWLAN */
	ops->hal_mp_init = hal_mp_init_8852be;
	ops->hal_mp_deinit = hal_mp_deinit_8852be;

	ops->hal_hci_configure = hal_hci_cfg_8852be;
	ops->init_default_value = hal_init_default_value_8852be;
	ops->enable_interrupt = hal_enable_int_8852be;
	ops->disable_interrupt = hal_disable_int_8852be;
	ops->recognize_interrupt = hal_recognize_int_8852be;
	ops->clear_interrupt = hal_clear_int_8852be;
	ops->interrupt_handler = hal_int_hdler_8852be;
	ops->restore_interrupt = hal_restore_int_8852be;
	ops->restore_rx_interrupt = hal_rx_int_restore_8852be;
}

