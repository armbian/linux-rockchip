/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Rockchip PCIe Apis For WIFI
 *
 * Copyright (c) 2022, Rockchip Electronics Co., Ltd.
 */

#ifndef __RK_DHD_PCIE_LINUX_H__
#define __RK_DHD_PCIE_LINUX_H__

#include <typedefs.h>
#include <sbchipc.h>
#include <pcie_core.h>
#include <dhd_pcie.h>
#include <linux/aspm_ext.h>

static inline void
rk_dhd_bus_l1ss_enable_rc_ep(dhd_bus_t *bus, bool enable)
{
	if (!bus->rc_ep_aspm_cap || !bus->rc_ep_l1ss_cap) {
		pr_err("%s: NOT L1SS CAPABLE rc_ep_aspm_cap: %d rc_ep_l1ss_cap: %d\n",
		       __func__, bus->rc_ep_aspm_cap, bus->rc_ep_l1ss_cap);
		return;
	}

	/* Disable ASPM of RC and EP */
	pr_err("%s: %s L1ss\n", __FUNCTION__, enable ? "enable" : "disable");
	pcie_aspm_ext_l1ss_enable(bus->dev, bus->rc_dev, enable);
}

static inline bool
rk_dhd_bus_is_rc_ep_l1ss_capable(dhd_bus_t *bus)
{
	return pcie_aspm_ext_is_rc_ep_l1ss_capable(bus->dev, bus->rc_dev);
}

static inline int
rk_dhd_bus_pcie_wait_for_l1ss(dhd_bus_t *bus)
{
	u32 val;
	int i;

	if (!bus->rc_ep_aspm_cap || !bus->rc_ep_l1ss_cap) {
		return -1;
	}

	pci_read_config_dword(bus->dev, PCIECFGREG_STATUS_CMD, &val);
	if (val == (uint32)-1)
		return -1;

	for (i = 0; i < 5; i++) {
		if (pcie_aspm_ext_is_in_l1sub_state(bus->rc_dev))
			return 0;
		msleep(20);
	}
	pr_err("%s failed\n", __FUNCTION__);

	return -1;
}

#endif /* __RK_DHD_PCIE_LINUX_H__ */
