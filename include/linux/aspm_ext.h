/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2022-2024 Rockchip Electronics Co., Ltd. */

#ifndef _ASPM_EXT_H
#define _ASPM_EXT_H

#if IS_REACHABLE(CONFIG_PCIEASPM_EXT)
bool pcie_aspm_ext_is_rc_ep_l1ss_capable(struct pci_dev *child, struct pci_dev *parent);
void pcie_aspm_ext_l1ss_enable(struct pci_dev *child, struct pci_dev *parent, bool enable);
bool pcie_aspm_ext_is_in_l1sub_state(struct pci_dev *pdev);
#else
static inline bool pcie_aspm_ext_is_rc_ep_l1ss_capable(struct pci_dev *child, struct pci_dev *parent) { return false; }
static inline void pcie_aspm_ext_l1ss_enable(struct pci_dev *child, struct pci_dev *parent, bool enable) {}
static inline bool pcie_aspm_ext_is_in_l1sub_state(struct pci_dev *pdev) { return false; }
#endif

enum rockchip_pcie_pm_ctrl_flag {
	ROCKCHIP_PCIE_PM_CTRL_RESET = 1,
	ROCKCHIP_PCIE_PM_RETRAIN_LINK = 2,
};

int rockchip_dw_pcie_pm_ctrl_for_user(struct pci_dev *dev, enum rockchip_pcie_pm_ctrl_flag flag);

#endif
