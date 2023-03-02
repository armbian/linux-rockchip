/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
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
 ******************************************************************************/

#include "hci_fc.h"

static u32 chcfg_size = sizeof(struct mac_ax_hfc_ch_cfg) * MAC_AX_DMA_CH_NUM;

static u32 chinfo_size = sizeof(struct mac_ax_hfc_ch_info) * MAC_AX_DMA_CH_NUM;

#if MAC_AX_PCIE_SUPPORT
#ifdef PHL_FEATURE_AP
static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_8852a[] = {
	{16, 3792, grp_0}, /* ACH 0 */
	{16, 3792, grp_0}, /* ACH 1 */
	{16, 3792, grp_0}, /* ACH 2 */
	{16, 3792, grp_0}, /* ACH 3 */
	{8, 3792, grp_0}, /* ACH 4 */
	{8, 3792, grp_0}, /* ACH 5 */
	{8, 3792, grp_0}, /* ACH 6 */
	{8, 3792, grp_0}, /* ACH 7 */
	{16, 3792, grp_0}, /* B0MGQ */
	{16, 3792, grp_0}, /* B0HIQ */
	{8, 3792, grp_0}, /* B1MGQ */
	{8, 3792, grp_0}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};
#else // for NiC mode use
static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_8852a[] = {
	{16, 3276, grp_0}, /* ACH 0 */
	{16, 3276, grp_0}, /* ACH 1 */
	{16, 3276, grp_0}, /* ACH 2 */
	{16, 3276, grp_0}, /* ACH 3 */
	{8, 3284, grp_0}, /* ACH 4 */
	{8, 3284, grp_0}, /* ACH 5 */
	{8, 3284, grp_0}, /* ACH 6 */
	{8, 3284, grp_0}, /* ACH 7 */
	{16, 3276, grp_0}, /* B0MGQ */
	{16, 3276, grp_0}, /* B0HIQ */
	{8, 3284, grp_0}, /* B1MGQ */
	{8, 3284, grp_0}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};
#endif

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_8852b[] = {
	{5, 343, grp_0}, /* ACH 0 */
	{5, 343, grp_0}, /* ACH 1 */
	{5, 343, grp_0}, /* ACH 2 */
	{5, 343, grp_0}, /* ACH 3 */
	{0, 0, grp_0}, /* ACH 4 */
	{0, 0, grp_0}, /* ACH 5 */
	{0, 0, grp_0}, /* ACH 6 */
	{0, 0, grp_0}, /* ACH 7 */
	{4, 344, grp_0}, /* B0MGQ */
	{4, 344, grp_0}, /* B0HIQ */
	{0, 0, grp_0}, /* B1MGQ */
	{0, 0, grp_0}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_8852b_turbo[] = {
	{16, 744, grp_0}, /* ACH 0 */
	{16, 744, grp_0}, /* ACH 1 */
	{16, 744, grp_0}, /* ACH 2 */
	{16, 744, grp_0}, /* ACH 3 */
	{0, 0, grp_0}, /* ACH 4 */
	{0, 0, grp_0}, /* ACH 5 */
	{0, 0, grp_0}, /* ACH 6 */
	{0, 0, grp_0}, /* ACH 7 */
	{16, 744, grp_0}, /* B0MGQ */
	{16, 744, grp_0}, /* B0HIQ */
	{0, 0, grp_0}, /* B1MGQ */
	{0, 0, grp_0}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_scc_8852c[] = {
	{13, 1614, grp_0}, /* ACH 0 */
	{13, 1614, grp_0}, /* ACH 1 */
	{13, 1614, grp_0}, /* ACH 2 */
	{13, 1614, grp_0}, /* ACH 3 */
	{13, 1614, grp_1}, /* ACH 4 */
	{13, 1614, grp_1}, /* ACH 5 */
	{13, 1614, grp_1}, /* ACH 6 */
	{13, 1614, grp_1}, /* ACH 7 */
	{13, 1614, grp_0}, /* B0MGQ */
	{13, 1614, grp_0}, /* B0HIQ */
	{13, 1614, grp_1}, /* B1MGQ */
	{13, 1614, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_dbcc_8852c[] = {
	{12, 1609, grp_0}, /* ACH 0 */
	{12, 1609, grp_0}, /* ACH 1 */
	{12, 1609, grp_0}, /* ACH 2 */
	{12, 1609, grp_0}, /* ACH 3 */
	{12, 1609, grp_1}, /* ACH 4 */
	{12, 1609, grp_1}, /* ACH 5 */
	{12, 1609, grp_1}, /* ACH 6 */
	{12, 1609, grp_1}, /* ACH 7 */
	{12, 1609, grp_0}, /* B0MGQ */
	{12, 1609, grp_0}, /* B0HIQ */
	{12, 1609, grp_1}, /* B1MGQ */
	{12, 1609, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_wd128_8852b[] = {
	{256, 1792, grp_0}, /* ACH 0 */
	{256, 1792, grp_0}, /* ACH 1 */
	{256, 1792, grp_0}, /* ACH 2 */
	{256, 1792, grp_0}, /* ACH 3 */
	{0, 0, grp_0}, /* ACH 4 */
	{0, 0, grp_0}, /* ACH 5 */
	{0, 0, grp_0}, /* ACH 6 */
	{0, 0, grp_0}, /* ACH 7 */
	{256, 1792, grp_0}, /* B0MGQ */
	{256, 1792, grp_0}, /* B0HIQ */
	{0, 0, grp_0}, /* B1MGQ */
	{0, 0, grp_0}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_stf_8852a[] = {
	{8, 256, grp_0}, /* ACH 0 */
	{8, 256, grp_0}, /* ACH 1 */
	{8, 256, grp_0}, /* ACH 2 */
	{8, 256, grp_0}, /* ACH 3 */
	{8, 256, grp_1}, /* ACH 4 */
	{8, 256, grp_1}, /* ACH 5 */
	{8, 256, grp_1}, /* ACH 6 */
	{8, 256, grp_1}, /* ACH 7 */
	{8, 256, grp_0}, /* B0MGQ */
	{8, 256, grp_0}, /* B0HIQ */
	{8, 256, grp_1}, /* B1MGQ */
	{8, 256, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_stf_8852b[] = {
	{27, 204, grp_0}, /* ACH 0 */
	{27, 204, grp_0}, /* ACH 1 */
	{27, 204, grp_0}, /* ACH 2 */
	{27, 204, grp_0}, /* ACH 3 */
	{0, 0, grp_0}, /* ACH 4 */
	{0, 0, grp_0}, /* ACH 5 */
	{0, 0, grp_0}, /* ACH 6 */
	{0, 0, grp_0}, /* ACH 7 */
	{11, 204, grp_0}, /* B0MGQ */
	{11, 204, grp_0}, /* B0HIQ */
	{0, 0, grp_0}, /* B1MGQ */
	{0, 0, grp_0}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_scc_stf_8852c[] = {
	{2, 108, grp_0}, /* ACH 0 */
	{2, 108, grp_0}, /* ACH 1 */
	{2, 108, grp_0}, /* ACH 2 */
	{2, 108, grp_0}, /* ACH 3 */
	{2, 108, grp_1}, /* ACH 4 */
	{2, 108, grp_1}, /* ACH 5 */
	{2, 108, grp_1}, /* ACH 6 */
	{2, 108, grp_1}, /* ACH 7 */
	{2, 108, grp_0}, /* B0MGQ */
	{2, 108, grp_0}, /* B0HIQ */
	{2, 108, grp_1}, /* B1MGQ */
	{2, 108, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_dbcc_stf_8852c[] = {
	{2, 76, grp_0}, /* ACH 0 */
	{2, 76, grp_0}, /* ACH 1 */
	{2, 76, grp_0}, /* ACH 2 */
	{2, 76, grp_0}, /* ACH 3 */
	{2, 76, grp_1}, /* ACH 4 */
	{2, 76, grp_1}, /* ACH 5 */
	{2, 76, grp_1}, /* ACH 6 */
	{2, 76, grp_1}, /* ACH 7 */
	{2, 76, grp_0}, /* B0MGQ */
	{2, 76, grp_0}, /* B0HIQ */
	{2, 76, grp_1}, /* B1MGQ */
	{2, 76, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_sutp_8852a[] = {
	{128, 256, grp_0}, /* ACH 0 */
	{0, 0, grp_1}, /* ACH 1 */
	{0, 0, grp_1}, /* ACH 2 */
	{0, 0, grp_1}, /* ACH 3 */
	{0, 0, grp_1}, /* ACH 4 */
	{0, 0, grp_1}, /* ACH 5 */
	{0, 0, grp_1}, /* ACH 6 */
	{0, 0, grp_1}, /* ACH 7 */
	{0, 0, grp_1}, /* B0MGQ */
	{0, 0, grp_1}, /* B0HIQ */
	{0, 0, grp_1}, /* B1MGQ */
	{0, 0, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_bcn_test_8852a[] = {
	{128, 1833, grp_0}, /* ACH 0 */
	{128, 1833, grp_0}, /* ACH 1 */
	{128, 1833, grp_0}, /* ACH 2 */
	{128, 1833, grp_0}, /* ACH 3 */
	{128, 1833, grp_1}, /* ACH 4 */
	{128, 1833, grp_1}, /* ACH 5 */
	{128, 1833, grp_1}, /* ACH 6 */
	{128, 1833, grp_1}, /* ACH 7 */
	{32, 1833, grp_0}, /* B0MGQ */
	{128, 1833, grp_0}, /* B0HIQ */
	{32, 1833, grp_1}, /* B1MGQ */
	{128, 1833, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_la_8852a[] = {
	{64, 586, grp_0}, /* ACH 0 */
	{64, 586, grp_0}, /* ACH 1 */
	{64, 586, grp_0}, /* ACH 2 */
	{64, 586, grp_0}, /* ACH 3 */
	{64, 586, grp_1}, /* ACH 4 */
	{64, 586, grp_1}, /* ACH 5 */
	{64, 586, grp_1}, /* ACH 6 */
	{64, 586, grp_1}, /* ACH 7 */
	{32, 586, grp_0}, /* B0MGQ */
	{64, 586, grp_0}, /* B0HIQ */
	{32, 586, grp_1}, /* B1MGQ */
	{64, 586, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_la_8852b[] = {
	{10, 200, grp_0}, /* ACH 0 */
	{10, 200, grp_0}, /* ACH 1 */
	{10, 200, grp_0}, /* ACH 2 */
	{10, 200, grp_0}, /* ACH 3 */
	{0, 0, grp_0}, /* ACH 4 */
	{0, 0, grp_0}, /* ACH 5 */
	{0, 0, grp_0}, /* ACH 6 */
	{0, 0, grp_0}, /* ACH 7 */
	{4, 200, grp_0}, /* B0MGQ */
	{4, 200, grp_0}, /* B0HIQ */
	{0, 0, grp_0}, /* B1MGQ */
	{0, 0, grp_0}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_pcie_la_8852c[] = {
	{4, 1560, grp_0}, /* ACH 0 */
	{4, 1560, grp_0}, /* ACH 1 */
	{4, 1560, grp_0}, /* ACH 2 */
	{4, 1560, grp_0}, /* ACH 3 */
	{4, 1560, grp_1}, /* ACH 4 */
	{4, 1560, grp_1}, /* ACH 5 */
	{4, 1560, grp_1}, /* ACH 6 */
	{4, 1560, grp_1}, /* ACH 7 */
	{4, 1560, grp_0}, /* B0MGQ */
	{4, 1560, grp_0}, /* B0HIQ */
	{4, 1560, grp_1}, /* B1MGQ */
	{4, 1560, grp_1}, /* B1HIQ */
	{40, 0, 0} /* FWCMDQ */
};

#ifdef PHL_FEATURE_AP
static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_8852a = {
	3792, /* Group 0 */
	0, /* Group 1 */
	3792, /* Public Max */
	0 /* WP threshold */
};
#else //for nic mode use
static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_8852a = {
	3792, /* Group 0 */
	0, /* Group 1 */
	3792, /* Public Max */
	0 /* WP threshold */
};
#endif

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_8852b = {
	448, /* Group 0 */
	0, /* Group 1 */
	448, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_8852b_turbo = {
	960, /* Group 0 */
	0, /* Group 1 */
	960, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_scc_8852c = {
	1614, /* Group 0 */
	1614, /* Group 1 */
	3228, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_dbcc_8852c = {
	1609, /* Group 0 */
	1609, /* Group 1 */
	3218, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_wd128_8852b = {
	1792, /* Group 0 */
	0, /* Group 1 */
	1792, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_stf_8852a = {
	256, /* Group 0 */
	256, /* Group 1 */
	512, /* Public Max */
	104 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_stf_8852b = {
	204, /* Group 0 */
	0, /* Group 1 */
	204, /* Public Max */
	104 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_scc_stf_8852c = {
	108, /* Group 0 */
	108, /* Group 1 */
	216, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_dbcc_stf_8852c = {
	76, /* Group 0 */
	76, /* Group 1 */
	152, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_sutp_8852a = {
	256, /* Group 0 */
	0, /* Group 1 */
	256, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_bcn_test_8852a = {
	1833, /* Group 0 */
	1833, /* Group 1 */
	3666, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_la_8852a = {
	586, /* Group 0 */
	586, /* Group 1 */
	1172, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_la_8852b = {
	200, /* Group 0 */
	0, /* Group 1 */
	200, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_pcie_la_8852c = {
	1560, /* Group 0 */
	1560, /* Group 1 */
	3120, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_pcie = {
	2, /*CH 0-11 pre-cost */
	40, /*H2C pre-cost */
	0, /* WP CH 0-7 pre-cost */
	0, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X2, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X1, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X1, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X1 /* WP CH 8-11 full condition */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_pcie_wd128 = {
	2, /*CH 0-11 pre-cost */
	40, /*H2C pre-cost */
	0, /* WP CH 0-7 pre-cost */
	0, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X2, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X1, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X1, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X1 /* WP CH 8-11 full condition */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_pcie_stf = {
	1, /*CH 0-11 pre-cost */
	40, /*H2C pre-cost */
	64, /* WP CH 0-7 pre-cost */
	64, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X2, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X1, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X2, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X2 /* WP CH 8-11 full condition */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_pcie_stf_8852c = {
	1, /*CH 0-11 pre-cost */
	40, /*H2C pre-cost */
	48, /* WP CH 0-7 pre-cost */
	48, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X2, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X1, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X1, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X1 /* WP CH 8-11 full condition */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_pcie_dlfw_8852c = {
	0, /*CH 0-11 pre-cost */
	256, /*H2C pre-cost */
	0, /* WP CH 0-7 pre-cost */
	0, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X2, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X2, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X1, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X1 /* WP CH 8-11 full condition */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_pcie_la_8852c = {
	2, /*CH 0-11 pre-cost */
	20, /*H2C pre-cost */
	0, /* WP CH 0-7 pre-cost */
	0, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X2, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X2, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X1, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X1 /* WP CH 8-11 full condition */
};
#endif

#if MAC_AX_USB_SUPPORT
static struct mac_ax_hfc_ch_cfg hfc_chcfg_usb_dbcc[] = {
	{22, 212, grp_0}, /* ACH 0 */
	{0, 0, grp_0}, /* ACH 1 */
	{22, 212, grp_0}, /* ACH 2 */
	{0, 0, grp_0}, /* ACH 3 */
	{22, 212, grp_1}, /* ACH 4 */
	{0, 0, grp_1}, /* ACH 5 */
	{22, 212, grp_1}, /* ACH 6 */
	{0, 0, grp_1}, /* ACH 7 */
	{22, 212, grp_0}, /* B0MGQ */
	{0, 0, grp_0}, /* B0HIQ */
	{22, 212, grp_1}, /* B1MGQ */
	{0, 0, grp_1}, /* B1HIQ */
	{0, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_usb_dbcc = {
	256, /* Group 0 */
	256, /* Group 1 */
	512, /* Public Max */
	104 /* WP threshold */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_usb_scc_8852a[] = {
	{22, 402, grp_0}, /* ACH 0 */
	{0, 0, grp_0}, /* ACH 1 */
	{22, 402, grp_0}, /* ACH 2 */
	{0, 0, grp_0}, /* ACH 3 */
	{22, 402, grp_0}, /* ACH 4 */
	{0, 0, grp_0}, /* ACH 5 */
	{22, 402, grp_0}, /* ACH 6 */
	{0, 0, grp_0}, /* ACH 7 */
	{22, 402, grp_0}, /* B0MGQ */
	{0, 0, grp_0}, /* B0HIQ */
	{22, 402, grp_0}, /* B1MGQ */
	{0, 0, grp_0}, /* B1HIQ */
	{0, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_usb_scc_8852b[] = {
	{18, 152, grp_0}, /* ACH 0 */
	{18, 152, grp_0}, /* ACH 1 */
	{18, 152, grp_0}, /* ACH 2 */
	{18, 152, grp_0}, /* ACH 3 */
	{0, 0, grp_0}, /* ACH 4 */
	{0, 0, grp_0}, /* ACH 5 */
	{0, 0, grp_0}, /* ACH 6 */
	{0, 0, grp_0}, /* ACH 7 */
	{18, 152, grp_0}, /* B0MGQ */
	{18, 152, grp_0}, /* B0HIQ */
	{0, 0, grp_0}, /* B1MGQ */
	{0, 0, grp_0}, /* B1HIQ */
	{0, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_usb_scc_turbo_8852b[] = {
	{18, 210, grp_0}, /* ACH 0 */
	{18, 210, grp_0}, /* ACH 1 */
	{18, 210, grp_0}, /* ACH 2 */
	{18, 210, grp_0}, /* ACH 3 */
	{0, 0, grp_0}, /* ACH 4 */
	{0, 0, grp_0}, /* ACH 5 */
	{0, 0, grp_0}, /* ACH 6 */
	{0, 0, grp_0}, /* ACH 7 */
	{18, 210, grp_0}, /* B0MGQ */
	{18, 210, grp_0}, /* B0HIQ */
	{0, 0, grp_0}, /* B1MGQ */
	{0, 0, grp_0}, /* B1HIQ */
	{0, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_usb_scc_8852a = {
	512, /* Group 0 */
	0, /* Group 1 */
	512, /* Public Max */
	104 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_usb_scc_8852b = {
	152, /* Group 0 */
	0, /* Group 1 */
	152, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_usb_scc_turbo_8852b = {
	210, /* Group 0 */
	0, /* Group 1 */
	210, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_usb_8852a = {
	11, /*CH 0-11 pre-cost */
	32, /*H2C pre-cost */
	76, /* WP CH 0-7 pre-cost */
	25, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X2, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X2, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X2, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X2 /* WP CH 8-11 full condition */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_usb_8852b = {
	9, /*CH 0-11 pre-cost */
	32, /*H2C pre-cost */
	64, /* WP CH 0-7 pre-cost */
	24, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X2, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X2, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X2, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X2 /* WP CH 8-11 full condition */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_usb_la_8852a[] = {
	{22, 84, grp_0}, /* ACH 0 */
	{0, 0, grp_0}, /* ACH 1 */
	{22, 84, grp_0}, /* ACH 2 */
	{0, 0, grp_0}, /* ACH 3 */
	{22, 84, grp_1}, /* ACH 4 */
	{0, 0, grp_1}, /* ACH 5 */
	{22, 84, grp_1}, /* ACH 6 */
	{0, 0, grp_1}, /* ACH 7 */
	{22, 84, grp_0}, /* B0MGQ */
	{0, 0, grp_0}, /* B0HIQ */
	{22, 84, grp_1}, /* B1MGQ */
	{0, 0, grp_1}, /* B1HIQ */
	{0, 0, 0} /* FWCMDQ */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_usb_la_8852a = {
	128, /* Group 0 */
	128, /* Group 1 */
	256, /* Public Max */
	104 /* WP threshold */
};
#endif

#if MAC_AX_SDIO_SUPPORT
static struct mac_ax_hfc_ch_cfg hfc_chcfg_sdio_8852a[] = {
	{2, 490, grp_0}, /* ACH 0 */
	{2, 490, grp_0}, /* ACH 1 */
	{2, 490, grp_0}, /* ACH 2 */
	{2, 490, grp_0}, /* ACH 3 */
	{2, 490, grp_0}, /* ACH 4 */
	{2, 490, grp_0}, /* ACH 5 */
	{2, 490, grp_0}, /* ACH 6 */
	{2, 490, grp_0}, /* ACH 7 */
	{2, 490, grp_0}, /* B0MGQ */
	{2, 490, grp_0}, /* B0HIQ */
	{2, 490, grp_0}, /* B1MGQ */
	{2, 490, grp_0}, /* B1HIQ */
	{40, 0, 0} /* H2CQ */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_sdio_8852b[] = {
	{2, 102, grp_0}, /* ACH 0 */
	{2, 102, grp_0}, /* ACH 1 */
	{2, 102, grp_0}, /* ACH 2 */
	{2, 102, grp_0}, /* ACH 3 */
	{0, 0, grp_1}, /* ACH 4 */
	{0, 0, grp_1}, /* ACH 5 */
	{0, 0, grp_1}, /* ACH 6 */
	{0, 0, grp_1}, /* ACH 7 */
	{2, 102, grp_0}, /* B0MGQ */
	{2, 102, grp_0}, /* B0HIQ */
	{0, 0, grp_1}, /* B1MGQ */
	{0, 0, grp_1}, /* B1HIQ */
	{40, 0, 0} /* H2CQ */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_sdio_8852a = {
	512, /* Group 0 */
	0, /* Group 1 */
	512, /* Public Max */
	104 /* WP threshold */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_sdio_8852b = {
	112, /* Group 0 */
	0, /* Group 1 */
	112, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_dbcc_sdio_8852a[] = {
	{2, 246, grp_0}, /* ACH 0 */
	{2, 246, grp_0}, /* ACH 1 */
	{2, 246, grp_0}, /* ACH 2 */
	{2, 246, grp_0}, /* ACH 3 */
	{2, 246, grp_1}, /* ACH 4 */
	{2, 246, grp_1}, /* ACH 5 */
	{2, 246, grp_1}, /* ACH 6 */
	{2, 246, grp_1}, /* ACH 7 */
	{2, 246, grp_0}, /* B0MGQ */
	{2, 246, grp_0}, /* B0HIQ */
	{2, 246, grp_1}, /* B1MGQ */
	{2, 246, grp_1}, /* B1HIQ */
	{40, 0, 0} /* H2CQ */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_dbcc_sdio_8852a = {
	256, /* Group 0 */
	256, /* Group 1 */
	512, /* Public Max */
	104 /* WP threshold */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_la_sdio_8852a[] = {
	{2, 54, grp_0}, /* ACH 0 */
	{2, 54, grp_0}, /* ACH 1 */
	{2, 54, grp_0}, /* ACH 2 */
	{2, 54, grp_0}, /* ACH 3 */
	{2, 54, grp_1}, /* ACH 4 */
	{2, 54, grp_1}, /* ACH 5 */
	{2, 54, grp_1}, /* ACH 6 */
	{2, 54, grp_1}, /* ACH 7 */
	{2, 54, grp_0}, /* B0MGQ */
	{2, 54, grp_0}, /* B0HIQ */
	{2, 54, grp_1}, /* B1MGQ */
	{2, 54, grp_1}, /* B1HIQ */
	{40, 0, 0} /* H2CQ */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_la_sdio_8852a = {
	64, /* Group 0 */
	64, /* Group 1 */
	128, /* Public Max */
	104 /* WP threshold */
};

static struct mac_ax_hfc_ch_cfg hfc_chcfg_la_sdio_8852b[] = {
	{2, 102, grp_0}, /* ACH 0 */
	{2, 102, grp_0}, /* ACH 1 */
	{2, 102, grp_0}, /* ACH 2 */
	{2, 102, grp_0}, /* ACH 3 */
	{0, 0, grp_1}, /* ACH 4 */
	{0, 0, grp_1}, /* ACH 5 */
	{0, 0, grp_1}, /* ACH 6 */
	{0, 0, grp_1}, /* ACH 7 */
	{2, 102, grp_0}, /* B0MGQ */
	{2, 102, grp_0}, /* B0HIQ */
	{0, 0, grp_1}, /* B1MGQ */
	{0, 0, grp_1}, /* B1HIQ */
	{40, 0, 0} /* H2CQ */
};

static struct mac_ax_hfc_pub_cfg hfc_pubcfg_la_sdio_8852b = {
	112, /* Group 0 */
	0, /* Group 1 */
	112, /* Public Max */
	0 /* WP threshold */
};

static struct mac_ax_hfc_prec_cfg hfc_preccfg_sdio = {
	1, /*CH 0-11 pre-cost */
	40, /*H2C pre-cost */
	0, /* WP CH 0-7 pre-cost */
	0, /* WP CH 8-11 pre-cost */
	MAC_AX_HFC_FULL_COND_X1, /* CH 0-11 full condition */
	MAC_AX_HFC_FULL_COND_X1, /* H2C full condition */
	MAC_AX_HFC_FULL_COND_X1, /* WP CH 0-7 full condition */
	MAC_AX_HFC_FULL_COND_X1 /* WP CH 8-11 full condition */
};
#endif

u32 hfc_reset_param(struct mac_ax_adapter *adapter)
{
	struct mac_ax_hfc_param *param;
	struct mac_ax_hfc_ch_cfg *ch_cfg, *ch_cfg_ini;
	struct mac_ax_hfc_ch_info *ch_info;
	struct mac_ax_hfc_pub_cfg *pub_cfg, *pub_cfg_ini;
	struct mac_ax_hfc_pub_info *pub_info;
	struct mac_ax_hfc_prec_cfg *prec_cfg, *prec_cfg_ini;
	u8 ch;

	param = adapter->hfc_param;
	ch_cfg = param->ch_cfg;
	ch_info = param->ch_info;
	pub_cfg = param->pub_cfg;
	pub_info = param->pub_info;
	prec_cfg = param->prec_cfg;

	switch (adapter->hw_info->intf) {
#if MAC_AX_SDIO_SUPPORT
	case MAC_AX_INTF_SDIO:
		param->en = 0;
		param->mode = MAC_AX_HCIFC_SDIO;
		prec_cfg_ini = &hfc_preccfg_sdio;

		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_SCC:
				ch_cfg_ini = hfc_chcfg_sdio_8852a;
				pub_cfg_ini = &hfc_pubcfg_sdio_8852a;
				break;
			case MAC_AX_QTA_DBCC:
				ch_cfg_ini = hfc_chcfg_dbcc_sdio_8852a;
				pub_cfg_ini = &hfc_pubcfg_dbcc_sdio_8852a;
				break;
			case MAC_AX_QTA_DLFW:
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				break;
			case MAC_AX_QTA_LAMODE:
				ch_cfg_ini = hfc_chcfg_la_sdio_8852a;
				pub_cfg_ini = &hfc_pubcfg_la_sdio_8852a;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_SCC:
			case MAC_AX_QTA_SCC_TURBO:
				ch_cfg_ini = hfc_chcfg_sdio_8852b;
				pub_cfg_ini = &hfc_pubcfg_sdio_8852b;
				break;
			case MAC_AX_QTA_DLFW:
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				break;
			case MAC_AX_QTA_LAMODE:
				ch_cfg_ini = hfc_chcfg_la_sdio_8852b;
				pub_cfg_ini = &hfc_pubcfg_la_sdio_8852b;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else {
			return MACCHIPID;
		}
		break;
#endif
#if MAC_AX_USB_SUPPORT
	case MAC_AX_INTF_USB:
		param->en = 0;
		param->mode = MAC_AX_HCIFC_STF;

		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_SCC:
				ch_cfg_ini = hfc_chcfg_usb_scc_8852a;
				pub_cfg_ini = &hfc_pubcfg_usb_scc_8852a;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_DBCC:
				ch_cfg_ini = hfc_chcfg_usb_dbcc;
				pub_cfg_ini = &hfc_pubcfg_usb_dbcc;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_DLFW:
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_LAMODE:
				ch_cfg_ini = hfc_chcfg_usb_la_8852a;
				pub_cfg_ini = &hfc_pubcfg_usb_la_8852a;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_SCC:
				ch_cfg_ini = hfc_chcfg_usb_scc_8852b;
				pub_cfg_ini = &hfc_pubcfg_usb_scc_8852b;
				prec_cfg_ini = &hfc_preccfg_usb_8852b;
				break;
			case MAC_AX_QTA_SCC_TURBO:
				ch_cfg_ini = hfc_chcfg_usb_scc_turbo_8852b;
				pub_cfg_ini = &hfc_pubcfg_usb_scc_turbo_8852b;
				prec_cfg_ini = &hfc_preccfg_usb_8852b;
				break;
			case MAC_AX_QTA_DLFW:
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				prec_cfg_ini = &hfc_preccfg_usb_8852b;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_SCC:
				ch_cfg_ini = hfc_chcfg_usb_scc_8852a;
				pub_cfg_ini = &hfc_pubcfg_usb_scc_8852a;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_DBCC:
				ch_cfg_ini = hfc_chcfg_usb_dbcc;
				pub_cfg_ini = &hfc_pubcfg_usb_dbcc;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_DLFW:
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_LAMODE:
				ch_cfg_ini = hfc_chcfg_usb_la_8852a;
				pub_cfg_ini = &hfc_pubcfg_usb_la_8852a;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_SCC:
				ch_cfg_ini = hfc_chcfg_usb_scc_8852a;
				pub_cfg_ini = &hfc_pubcfg_usb_scc_8852a;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_DBCC:
				ch_cfg_ini = hfc_chcfg_usb_dbcc;
				pub_cfg_ini = &hfc_pubcfg_usb_dbcc;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_DLFW:
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			case MAC_AX_QTA_LAMODE:
				ch_cfg_ini = hfc_chcfg_usb_la_8852a;
				pub_cfg_ini = &hfc_pubcfg_usb_la_8852a;
				prec_cfg_ini = &hfc_preccfg_usb_8852a;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else {
			return MACCHIPID;
		}
		break;
#endif
#if MAC_AX_PCIE_SUPPORT
	case MAC_AX_INTF_PCIE:
		param->en = 0;

		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_DBCC:
			case MAC_AX_QTA_SCC:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_8852a;
				pub_cfg_ini = &hfc_pubcfg_pcie_8852a;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			case MAC_AX_QTA_DBCC_STF:
			case MAC_AX_QTA_SCC_STF:
				param->mode = MAC_AX_HCIFC_STF;
				ch_cfg_ini = hfc_chcfg_pcie_stf_8852a;
				pub_cfg_ini = &hfc_pubcfg_pcie_stf_8852a;
				prec_cfg_ini = &hfc_preccfg_pcie_stf;
				break;
			case MAC_AX_QTA_SU_TP:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_sutp_8852a;
				pub_cfg_ini = &hfc_pubcfg_pcie_sutp_8852a;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			case MAC_AX_QTA_DLFW:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			case MAC_AX_QTA_LAMODE:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_la_8852a;
				pub_cfg_ini = &hfc_pubcfg_pcie_la_8852a;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_SCC:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_8852b;
				pub_cfg_ini = &hfc_pubcfg_pcie_8852b;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			case MAC_AX_QTA_SCC_TURBO:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_8852b_turbo;
				pub_cfg_ini = &hfc_pubcfg_pcie_8852b_turbo;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			case MAC_AX_QTA_SCC_STF:
				param->mode = MAC_AX_HCIFC_STF;
				ch_cfg_ini = hfc_chcfg_pcie_stf_8852b;
				pub_cfg_ini = &hfc_pubcfg_pcie_stf_8852b;
				prec_cfg_ini = &hfc_preccfg_pcie_stf;
				break;
			case MAC_AX_QTA_DLFW:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			case MAC_AX_QTA_LAMODE:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_la_8852b;
				pub_cfg_ini = &hfc_pubcfg_pcie_la_8852b;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
			is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
			switch (adapter->dle_info.qta_mode) {
			case MAC_AX_QTA_DBCC:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_dbcc_8852c;
				pub_cfg_ini = &hfc_pubcfg_pcie_dbcc_8852c;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			case MAC_AX_QTA_SCC:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_scc_8852c;
				pub_cfg_ini = &hfc_pubcfg_pcie_scc_8852c;
				prec_cfg_ini = &hfc_preccfg_pcie;
				break;
			case MAC_AX_QTA_SCC_STF:
				param->mode = MAC_AX_HCIFC_STF;
				ch_cfg_ini = hfc_chcfg_pcie_scc_stf_8852c;
				pub_cfg_ini = &hfc_pubcfg_pcie_scc_stf_8852c;
				prec_cfg_ini = &hfc_preccfg_pcie_stf_8852c;
				break;
			case MAC_AX_QTA_DBCC_STF:
				param->mode = MAC_AX_HCIFC_STF;
				ch_cfg_ini = hfc_chcfg_pcie_dbcc_stf_8852c;
				pub_cfg_ini = &hfc_pubcfg_pcie_dbcc_stf_8852c;
				prec_cfg_ini = &hfc_preccfg_pcie_stf_8852c;
				break;
			case MAC_AX_QTA_DLFW:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = NULL;
				pub_cfg_ini = NULL;
				prec_cfg_ini = &hfc_preccfg_pcie_dlfw_8852c;
				break;
			case MAC_AX_QTA_LAMODE:
				param->mode = MAC_AX_HCIFC_POH;
				ch_cfg_ini = hfc_chcfg_pcie_la_8852c;
				pub_cfg_ini = &hfc_pubcfg_pcie_la_8852c;
				prec_cfg_ini = &hfc_preccfg_pcie_la_8852c;
				break;
			default:
				return MACHFCCH011QTA;
			}
		} else {
			return MACCHIPID;
		}
		break;
#endif
	default:
		return MACINTF;
	}

	if (pub_cfg_ini) {
		pub_cfg->group0 = pub_cfg_ini->group0;
		pub_cfg->group1 = pub_cfg_ini->group1;
		pub_cfg->pub_max = pub_cfg_ini->pub_max;
		pub_cfg->wp_thrd = pub_cfg_ini->wp_thrd;
	}

	pub_info->g0_used = 0;
	pub_info->g1_used = 0;
	pub_info->pub_aval = 0;
	pub_info->wp_aval = 0;

	if (pub_cfg_ini) {
		prec_cfg->ch011_prec = prec_cfg_ini->ch011_prec;
		prec_cfg->h2c_prec = prec_cfg_ini->h2c_prec;
		prec_cfg->wp_ch07_prec = prec_cfg_ini->wp_ch07_prec;
		prec_cfg->wp_ch811_prec = prec_cfg_ini->wp_ch811_prec;
		prec_cfg->ch011_full_cond = prec_cfg_ini->ch011_full_cond;
		prec_cfg->h2c_full_cond = prec_cfg_ini->h2c_full_cond;
		prec_cfg->wp_ch07_full_cond = prec_cfg_ini->wp_ch07_full_cond;
		prec_cfg->wp_ch811_full_cond = prec_cfg_ini->wp_ch811_full_cond;

		adapter->hw_info->sw_amsdu_max_size = prec_cfg->wp_ch07_prec *
						      HFC_PAGE_UNIT;
	}

	if (ch_cfg_ini) {
		for (ch = MAC_AX_DMA_ACH0; ch < MAC_AX_DMA_CH_NUM; ch++) {
			ch_cfg[ch].min = ch_cfg_ini[ch].min;
			ch_cfg[ch].max = ch_cfg_ini[ch].max;
			ch_cfg[ch].grp = ch_cfg_ini[ch].grp;
			ch_info[ch].aval = 0;
			ch_info[ch].used = 0;
		}
	}
	return MACSUCCESS;
}

static inline u32 hfc_ch_cfg_chk(struct mac_ax_adapter *adapter, u8 ch)
{
	struct mac_ax_hfc_ch_cfg *ch_cfg = adapter->hfc_param->ch_cfg;
	struct mac_ax_hfc_pub_cfg *pub_cfg = adapter->hfc_param->pub_cfg;
	struct mac_ax_hfc_prec_cfg *prec_cfg = adapter->hfc_param->prec_cfg;

	if (ch >= MAC_AX_DMA_CH_NUM)
		return MACINTF;

	if ((ch_cfg[ch].min && ch_cfg[ch].min < prec_cfg->ch011_prec) ||
	    ch_cfg[ch].max > pub_cfg->pub_max)
		return MACHFCCH011QTA;
	if (ch_cfg[ch].grp >= grp_num)
		return MACHFCCH011GRP;

	return MACSUCCESS;
}

static inline u32 hfc_pub_info_chk(struct mac_ax_adapter *adapter)
{
	struct mac_ax_hfc_pub_cfg *cfg = adapter->hfc_param->pub_cfg;
	struct mac_ax_hfc_pub_info *info = adapter->hfc_param->pub_info;

	if (info->g0_used + info->g1_used + info->pub_aval != cfg->pub_max) {
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A))
			return MACSUCCESS;
		else
			return MACHFCPUBINFO;
	}

	return MACSUCCESS;
}

static inline u32 hfc_pub_cfg_chk(struct mac_ax_adapter *adapter)
{
	struct mac_ax_hfc_param *param = adapter->hfc_param;
	struct mac_ax_hfc_pub_cfg *pub_cfg = param->pub_cfg;

	if (pub_cfg->group0 + pub_cfg->group1 > pub_cfg->pub_max)
		return MACHFCPUBQTA;

	return MACSUCCESS;
}

u32 hfc_ch_ctrl(struct mac_ax_adapter *adapter, u8 ch)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hfc_ch_cfg *cfg = adapter->hfc_param->ch_cfg;
	u32 val32 = 0;
	u32 ret = 0;

	ret = check_mac_en(adapter, 0, MAC_AX_DMAC_SEL);
	if (ret)
		return ret;

	ret = hfc_ch_cfg_chk(adapter, ch);
	if (ret)
		return ret;

	switch (ch) {
	case MAC_AX_DMA_ACH0:
		val32 = SET_WORD(cfg[MAC_AX_DMA_ACH0].min, B_AX_ACH0_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_ACH0].max, B_AX_ACH0_MAX_PG) |
			(cfg[MAC_AX_DMA_ACH0].grp ? B_AX_ACH0_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_ACH0_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_ACH0_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_ACH1:
		val32 = SET_WORD(cfg[MAC_AX_DMA_ACH1].min, B_AX_ACH1_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_ACH1].max, B_AX_ACH1_MAX_PG) |
			(cfg[MAC_AX_DMA_ACH1].grp ? B_AX_ACH1_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_ACH1_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_ACH1_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_ACH2:
		val32 = SET_WORD(cfg[MAC_AX_DMA_ACH2].min, B_AX_ACH2_MIN_PG) |
			 SET_WORD(cfg[MAC_AX_DMA_ACH2].max, B_AX_ACH2_MAX_PG) |
			(cfg[MAC_AX_DMA_ACH2].grp ? B_AX_ACH2_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_ACH2_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_ACH2_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_ACH3:
		val32 = SET_WORD(cfg[MAC_AX_DMA_ACH3].min, B_AX_ACH3_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_ACH3].max, B_AX_ACH3_MAX_PG) |
			(cfg[MAC_AX_DMA_ACH3].grp ? B_AX_ACH3_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_ACH3_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_ACH3_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_ACH4:
		val32 = SET_WORD(cfg[MAC_AX_DMA_ACH4].min, B_AX_ACH4_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_ACH4].max, B_AX_ACH4_MAX_PG) |
			(cfg[MAC_AX_DMA_ACH4].grp ? B_AX_ACH4_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_ACH4_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_ACH4_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_ACH5:
		val32 = SET_WORD(cfg[MAC_AX_DMA_ACH5].min, B_AX_ACH5_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_ACH5].max, B_AX_ACH5_MAX_PG) |
			(cfg[MAC_AX_DMA_ACH5].grp ? B_AX_ACH5_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_ACH5_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_ACH5_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_ACH6:
		val32 = SET_WORD(cfg[MAC_AX_DMA_ACH6].min, B_AX_ACH6_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_ACH6].max, B_AX_ACH6_MAX_PG) |
			(cfg[MAC_AX_DMA_ACH6].grp ? B_AX_ACH6_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_ACH6_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_ACH6_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_ACH7:
		val32 = SET_WORD(cfg[MAC_AX_DMA_ACH7].min, B_AX_ACH7_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_ACH7].max, B_AX_ACH7_MAX_PG) |
			(cfg[MAC_AX_DMA_ACH7].grp ? B_AX_ACH7_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_ACH7_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_ACH7_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_B0MG:
		val32 = SET_WORD(cfg[MAC_AX_DMA_B0MG].min, B_AX_CH8_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_B0MG].max, B_AX_CH8_MAX_PG) |
			(cfg[MAC_AX_DMA_B0MG].grp ? B_AX_CH8_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_CH8_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_CH8_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_B0HI:
		val32 = SET_WORD(cfg[MAC_AX_DMA_B0HI].min, B_AX_CH9_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_B0HI].max, B_AX_CH9_MAX_PG) |
			(cfg[MAC_AX_DMA_B0HI].grp ? B_AX_CH9_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_CH9_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_CH9_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_B1MG:
		val32 = SET_WORD(cfg[MAC_AX_DMA_B1MG].min, B_AX_CH10_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_B1MG].max, B_AX_CH10_MAX_PG) |
			(cfg[MAC_AX_DMA_B1MG].grp ? B_AX_CH10_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_CH10_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_CH10_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_B1HI:
		val32 = SET_WORD(cfg[MAC_AX_DMA_B1HI].min, B_AX_CH11_MIN_PG) |
			SET_WORD(cfg[MAC_AX_DMA_B1HI].max, B_AX_CH11_MAX_PG) |
			(cfg[MAC_AX_DMA_B1HI].grp ? B_AX_CH11_GRP : 0);
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			MAC_REG_W32(R_AX_CH11_PAGE_CTRL_V1, val32);
		else
			MAC_REG_W32(R_AX_CH11_PAGE_CTRL, val32);
		break;

	case MAC_AX_DMA_H2C:
		break;

	default:
		return MACTXCHDMA;
	}

	return MACSUCCESS;
}

u32 hfc_upd_ch_info(struct mac_ax_adapter *adapter, u8 ch)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hfc_ch_info *info = adapter->hfc_param->ch_info;
	struct mac_ax_hfc_ch_cfg *cfg = adapter->hfc_param->ch_cfg;
	u32 val32;
	u32 ret;

	ret = check_mac_en(adapter, 0, MAC_AX_DMAC_SEL);
	if (ret)
		return ret;

	switch (ch) {
	case MAC_AX_DMA_ACH0:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_ACH0_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_ACH0_PAGE_INFO);
		info[MAC_AX_DMA_ACH0].aval =
			GET_FIELD(val32, B_AX_ACH0_AVAL_PG);
		info[MAC_AX_DMA_ACH0].used =
			GET_FIELD(val32, B_AX_ACH0_USE_PG);
		break;

	case MAC_AX_DMA_ACH1:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_ACH1_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_ACH1_PAGE_INFO);
		info[MAC_AX_DMA_ACH1].aval =
			GET_FIELD(val32, B_AX_ACH1_AVAL_PG);
		info[MAC_AX_DMA_ACH1].used =
			GET_FIELD(val32, B_AX_ACH1_USE_PG);
		break;

	case MAC_AX_DMA_ACH2:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_ACH2_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_ACH2_PAGE_INFO);
		info[MAC_AX_DMA_ACH2].aval =
			GET_FIELD(val32, B_AX_ACH2_AVAL_PG);
		info[MAC_AX_DMA_ACH2].used =
			GET_FIELD(val32, B_AX_ACH2_USE_PG);
		break;

	case MAC_AX_DMA_ACH3:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_ACH3_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_ACH3_PAGE_INFO);
		info[MAC_AX_DMA_ACH3].aval =
			GET_FIELD(val32, B_AX_ACH3_AVAL_PG);
		info[MAC_AX_DMA_ACH3].used =
			GET_FIELD(val32, B_AX_ACH3_USE_PG);
		break;

	case MAC_AX_DMA_ACH4:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_ACH4_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_ACH4_PAGE_INFO);
		info[MAC_AX_DMA_ACH4].aval =
			GET_FIELD(val32, B_AX_ACH4_AVAL_PG);
		info[MAC_AX_DMA_ACH4].used =
			GET_FIELD(val32, B_AX_ACH4_USE_PG);
		break;

	case MAC_AX_DMA_ACH5:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_ACH5_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_ACH5_PAGE_INFO);
		info[MAC_AX_DMA_ACH5].aval =
			GET_FIELD(val32, B_AX_ACH5_AVAL_PG);
		info[MAC_AX_DMA_ACH5].used =
			GET_FIELD(val32, B_AX_ACH5_USE_PG);
		break;

	case MAC_AX_DMA_ACH6:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_ACH6_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_ACH6_PAGE_INFO);
		info[MAC_AX_DMA_ACH6].aval =
			GET_FIELD(val32, B_AX_ACH6_AVAL_PG);
		info[MAC_AX_DMA_ACH6].used =
			GET_FIELD(val32, B_AX_ACH6_USE_PG);
		break;

	case MAC_AX_DMA_ACH7:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_ACH7_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_ACH7_PAGE_INFO);
		info[MAC_AX_DMA_ACH7].aval =
			GET_FIELD(val32, B_AX_ACH7_AVAL_PG);
		info[MAC_AX_DMA_ACH7].used =
			GET_FIELD(val32, B_AX_ACH7_USE_PG);
		break;

	case MAC_AX_DMA_B0MG:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_CH8_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_CH8_PAGE_INFO);
		info[MAC_AX_DMA_B0MG].aval =
			GET_FIELD(val32, B_AX_CH8_AVAL_PG);
		info[MAC_AX_DMA_B0MG].used =
			GET_FIELD(val32, B_AX_CH8_USE_PG);
		break;

	case MAC_AX_DMA_B0HI:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_CH9_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_CH9_PAGE_INFO);
		info[MAC_AX_DMA_B0HI].aval =
			GET_FIELD(val32, B_AX_CH9_AVAL_PG);
		info[MAC_AX_DMA_B0HI].used =
			GET_FIELD(val32, B_AX_CH9_USE_PG);
		break;

	case MAC_AX_DMA_B1MG:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_CH10_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_CH10_PAGE_INFO);
		info[MAC_AX_DMA_B1MG].aval =
			GET_FIELD(val32, B_AX_CH10_AVAL_PG);
		info[MAC_AX_DMA_B1MG].used =
			GET_FIELD(val32, B_AX_CH10_USE_PG);
		break;

	case MAC_AX_DMA_B1HI:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_CH11_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_CH11_PAGE_INFO);
		info[MAC_AX_DMA_B1HI].aval =
			GET_FIELD(val32, B_AX_CH11_AVAL_PG);
		info[MAC_AX_DMA_B1HI].used =
			GET_FIELD(val32, B_AX_CH11_USE_PG);
		break;

	case MAC_AX_DMA_H2C:
		if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
			val32 = MAC_REG_R32(R_AX_CH12_PAGE_INFO_V1);
		else
			val32 = MAC_REG_R32(R_AX_CH12_PAGE_INFO);
		info[MAC_AX_DMA_H2C].aval =
			GET_FIELD(val32, B_AX_CH12_AVAL_PG);
		info[MAC_AX_DMA_H2C].used =
			cfg[MAC_AX_DMA_H2C].min - info[MAC_AX_DMA_H2C].aval;
		break;

	default:
		return MACTXCHDMA;
	}

	return MACSUCCESS;
}

u32 hfc_pub_ctrl(struct mac_ax_adapter *adapter)
{
	u32 val32, ret;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hfc_pub_cfg *cfg = adapter->hfc_param->pub_cfg;

	ret = check_mac_en(adapter, 0, MAC_AX_DMAC_SEL);
	if (ret)
		return ret;

	ret = hfc_pub_cfg_chk(adapter);
	if (ret)
		return ret;

	val32 = SET_WORD(cfg->group0, B_AX_PUBPG_G0) |
		SET_WORD(cfg->group1, B_AX_PUBPG_G1);
	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
		MAC_REG_W32(R_AX_PUB_PAGE_CTRL1_V1, val32);
	else
		MAC_REG_W32(R_AX_PUB_PAGE_CTRL1, val32);

	val32 = SET_WORD(cfg->wp_thrd, B_AX_WP_THRD);
	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
		MAC_REG_W32(R_AX_WP_PAGE_CTRL2_V1, val32);
	else
		MAC_REG_W32(R_AX_WP_PAGE_CTRL2, val32);

	return MACSUCCESS;
}

u32 hfc_upd_mix_info(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hfc_param *param = adapter->hfc_param;
	struct mac_ax_hfc_pub_cfg *pub_cfg = param->pub_cfg;
	struct mac_ax_hfc_prec_cfg *prec_cfg = param->prec_cfg;
	struct mac_ax_hfc_pub_info *info = param->pub_info;
	u32 val32, ret;

	ret = check_mac_en(adapter, 0, MAC_AX_DMAC_SEL);
	if (ret)
		return ret;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		val32 = MAC_REG_R32(R_AX_PUB_PAGE_INFO1_V1);
		info->g0_used = GET_FIELD(val32, B_AX_G0_USE_PG);
		info->g1_used = GET_FIELD(val32, B_AX_G1_USE_PG);

		val32 = MAC_REG_R32(R_AX_PUB_PAGE_INFO3_V1);
		info->g0_aval = GET_FIELD(val32, B_AX_G0_AVAL_PG);
		info->g1_aval = GET_FIELD(val32, B_AX_G1_AVAL_PG);
		info->pub_aval =
			GET_FIELD(MAC_REG_R32(R_AX_PUB_PAGE_INFO2_V1), B_AX_PUB_AVAL_PG);
		info->wp_aval =
			GET_FIELD(MAC_REG_R32(R_AX_WP_PAGE_INFO1_V1), B_AX_WP_AVAL_PG);

		val32 = MAC_REG_R32(R_AX_HCI_FC_CTRL_V1);
		param->en = val32 & B_AX_HCI_FC_EN ? 1 : 0;
		param->h2c_en = val32 & B_AX_HCI_FC_CH12_EN ? 1 : 0;
		param->mode = GET_FIELD(val32, B_AX_HCI_FC_MODE);
		prec_cfg->ch011_full_cond = GET_FIELD(val32, B_AX_HCI_FC_WD_FULL_COND);
		prec_cfg->h2c_full_cond = GET_FIELD(val32, B_AX_HCI_FC_CH12_FULL_COND);
		prec_cfg->wp_ch07_full_cond =
			GET_FIELD(val32, B_AX_HCI_FC_WP_CH07_FULL_COND);
		prec_cfg->wp_ch811_full_cond =
			GET_FIELD(val32, B_AX_HCI_FC_WP_CH811_FULL_COND);

		val32 = MAC_REG_R32(R_AX_CH_PAGE_CTRL_V1);
		prec_cfg->ch011_prec = GET_FIELD(val32, B_AX_PREC_PAGE_CH011);
		prec_cfg->h2c_prec = GET_FIELD(val32, B_AX_PREC_PAGE_CH12);

		val32 = MAC_REG_R32(R_AX_PUB_PAGE_CTRL2_V1);
		pub_cfg->pub_max = GET_FIELD(val32, B_AX_PUBPG_ALL);

		val32 = MAC_REG_R32(R_AX_WP_PAGE_CTRL1_V1);
		prec_cfg->wp_ch07_prec = GET_FIELD(val32, B_AX_PREC_PAGE_WP_CH07);
		prec_cfg->wp_ch811_prec = GET_FIELD(val32, B_AX_PREC_PAGE_WP_CH811);

		val32 = MAC_REG_R32(R_AX_WP_PAGE_CTRL2_V1);
		pub_cfg->wp_thrd = GET_FIELD(val32, B_AX_WP_THRD);

		val32 = MAC_REG_R32(R_AX_PUB_PAGE_CTRL1_V1);
		pub_cfg->group0 = GET_FIELD(val32, B_AX_PUBPG_G0);
		pub_cfg->group1 = GET_FIELD(val32, B_AX_PUBPG_G1);

		ret = hfc_pub_info_chk(adapter);
		if (param->en && ret)
			return ret;
	} else {
		val32 = MAC_REG_R32(R_AX_PUB_PAGE_INFO1);
		info->g0_used = GET_FIELD(val32, B_AX_G0_USE_PG);
		info->g1_used = GET_FIELD(val32, B_AX_G1_USE_PG);

		val32 = MAC_REG_R32(R_AX_PUB_PAGE_INFO3);
		info->g0_aval = GET_FIELD(val32, B_AX_G0_AVAL_PG);
		info->g1_aval = GET_FIELD(val32, B_AX_G1_AVAL_PG);
		info->pub_aval =
			GET_FIELD(MAC_REG_R32(R_AX_PUB_PAGE_INFO2), B_AX_PUB_AVAL_PG);
		info->wp_aval =
			GET_FIELD(MAC_REG_R32(R_AX_WP_PAGE_INFO1), B_AX_WP_AVAL_PG);

		val32 = MAC_REG_R32(R_AX_HCI_FC_CTRL);
		param->en = val32 & B_AX_HCI_FC_EN ? 1 : 0;
		param->h2c_en = val32 & B_AX_HCI_FC_CH12_EN ? 1 : 0;
		param->mode = GET_FIELD(val32, B_AX_HCI_FC_MODE);
		prec_cfg->ch011_full_cond = GET_FIELD(val32, B_AX_HCI_FC_WD_FULL_COND);
		prec_cfg->h2c_full_cond = GET_FIELD(val32, B_AX_HCI_FC_CH12_FULL_COND);
		prec_cfg->wp_ch07_full_cond =
			GET_FIELD(val32, B_AX_HCI_FC_WP_CH07_FULL_COND);
		prec_cfg->wp_ch811_full_cond =
			GET_FIELD(val32, B_AX_HCI_FC_WP_CH811_FULL_COND);

		val32 = MAC_REG_R32(R_AX_CH_PAGE_CTRL);
		prec_cfg->ch011_prec = GET_FIELD(val32, B_AX_PREC_PAGE_CH011);
		prec_cfg->h2c_prec = GET_FIELD(val32, B_AX_PREC_PAGE_CH12);

		val32 = MAC_REG_R32(R_AX_PUB_PAGE_CTRL2);
		pub_cfg->pub_max = GET_FIELD(val32, B_AX_PUBPG_ALL);

		val32 = MAC_REG_R32(R_AX_WP_PAGE_CTRL1);
		prec_cfg->wp_ch07_prec = GET_FIELD(val32, B_AX_PREC_PAGE_WP_CH07);
		prec_cfg->wp_ch811_prec = GET_FIELD(val32, B_AX_PREC_PAGE_WP_CH811);

		val32 = MAC_REG_R32(R_AX_WP_PAGE_CTRL2);
		pub_cfg->wp_thrd = GET_FIELD(val32, B_AX_WP_THRD);

		val32 = MAC_REG_R32(R_AX_PUB_PAGE_CTRL1);
		pub_cfg->group0 = GET_FIELD(val32, B_AX_PUBPG_G0);
		pub_cfg->group1 = GET_FIELD(val32, B_AX_PUBPG_G1);

		ret = hfc_pub_info_chk(adapter);
		if (param->en && ret)
			return ret;
	}
	return MACSUCCESS;
}

static void hfc_h2c_cfg(struct mac_ax_adapter *adapter)
{
	u32 val32;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hfc_param *param = adapter->hfc_param;
	//struct mac_ax_hfc_pub_cfg *pub_cfg = param->pub_cfg;
	struct mac_ax_hfc_prec_cfg *prec_cfg = param->prec_cfg;

	val32 = SET_WORD(prec_cfg->h2c_prec, B_AX_PREC_PAGE_CH12);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
		MAC_REG_W32(R_AX_CH_PAGE_CTRL_V1, val32);
	else
		MAC_REG_W32(R_AX_CH_PAGE_CTRL, val32);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		val32 = SET_CLR_WORD(MAC_REG_R32(R_AX_HCI_FC_CTRL_V1),
				     prec_cfg->h2c_full_cond,
				     B_AX_HCI_FC_CH12_FULL_COND);
		MAC_REG_W32((R_AX_HCI_FC_CTRL_V1), val32);
	} else {
		val32 = SET_CLR_WORD(MAC_REG_R32(R_AX_HCI_FC_CTRL),
				     prec_cfg->h2c_full_cond,
				     B_AX_HCI_FC_CH12_FULL_COND);
		MAC_REG_W32(R_AX_HCI_FC_CTRL, val32);
	}
}

static void hfc_mix_cfg(struct mac_ax_adapter *adapter)
{
	u32 val32;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hfc_param *param = adapter->hfc_param;
	struct mac_ax_hfc_pub_cfg *pub_cfg = param->pub_cfg;
	struct mac_ax_hfc_prec_cfg *prec_cfg = param->prec_cfg;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		val32 = SET_WORD(prec_cfg->ch011_prec, B_AX_PREC_PAGE_CH011) |
			SET_WORD(prec_cfg->h2c_prec, B_AX_PREC_PAGE_CH12);
		MAC_REG_W32(R_AX_CH_PAGE_CTRL_V1, val32);

		val32 = SET_WORD(pub_cfg->pub_max, B_AX_PUBPG_ALL);
		MAC_REG_W32(R_AX_PUB_PAGE_CTRL2_V1, val32);

		val32 = SET_WORD(prec_cfg->wp_ch07_prec, B_AX_PREC_PAGE_WP_CH07) |
			SET_WORD(prec_cfg->wp_ch811_prec, B_AX_PREC_PAGE_WP_CH811);
		MAC_REG_W32(R_AX_WP_PAGE_CTRL1_V1, val32);

		val32 = SET_CLR_WORD(MAC_REG_R32(R_AX_HCI_FC_CTRL_V1),
				     param->mode, B_AX_HCI_FC_MODE);
		val32 = SET_CLR_WORD(val32, prec_cfg->ch011_full_cond,
				     B_AX_HCI_FC_WD_FULL_COND);
		val32 = SET_CLR_WORD(val32, prec_cfg->h2c_full_cond,
				     B_AX_HCI_FC_CH12_FULL_COND);
		val32 = SET_CLR_WORD(val32, prec_cfg->wp_ch07_full_cond,
				     B_AX_HCI_FC_WP_CH07_FULL_COND);
		val32 = SET_CLR_WORD(val32, prec_cfg->wp_ch811_full_cond,
				     B_AX_HCI_FC_WP_CH811_FULL_COND);
		MAC_REG_W32(R_AX_HCI_FC_CTRL_V1, val32);
	} else {
		val32 = SET_WORD(prec_cfg->ch011_prec, B_AX_PREC_PAGE_CH011) |
			SET_WORD(prec_cfg->h2c_prec, B_AX_PREC_PAGE_CH12);
		MAC_REG_W32(R_AX_CH_PAGE_CTRL, val32);

		val32 = SET_WORD(pub_cfg->pub_max, B_AX_PUBPG_ALL);
		MAC_REG_W32(R_AX_PUB_PAGE_CTRL2, val32);

		val32 = SET_WORD(prec_cfg->wp_ch07_prec, B_AX_PREC_PAGE_WP_CH07) |
			SET_WORD(prec_cfg->wp_ch811_prec, B_AX_PREC_PAGE_WP_CH811);
		MAC_REG_W32(R_AX_WP_PAGE_CTRL1, val32);

		val32 = SET_CLR_WORD(MAC_REG_R32(R_AX_HCI_FC_CTRL),
				     param->mode, B_AX_HCI_FC_MODE);
		val32 = SET_CLR_WORD(val32, prec_cfg->ch011_full_cond,
				     B_AX_HCI_FC_WD_FULL_COND);
		val32 = SET_CLR_WORD(val32, prec_cfg->h2c_full_cond,
				     B_AX_HCI_FC_CH12_FULL_COND);
		val32 = SET_CLR_WORD(val32, prec_cfg->wp_ch07_full_cond,
				     B_AX_HCI_FC_WP_CH07_FULL_COND);
		val32 = SET_CLR_WORD(val32, prec_cfg->wp_ch811_full_cond,
				     B_AX_HCI_FC_WP_CH811_FULL_COND);
		MAC_REG_W32(R_AX_HCI_FC_CTRL, val32);
	}
}

static void hfc_func_en(struct mac_ax_adapter *adapter, u8 en, u8 h2c_en)
{
	u32 val32;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hfc_param *param = adapter->hfc_param;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		val32 = MAC_REG_R32(R_AX_HCI_FC_CTRL_V1);
		param->en = en ? 1 : 0;
		param->h2c_en = h2c_en ? 1 : 0;
		val32 = en ? (val32 | B_AX_HCI_FC_EN) : (val32 & ~B_AX_HCI_FC_EN);
		val32 = h2c_en ? (val32 | B_AX_HCI_FC_CH12_EN) :
				 (val32 & ~B_AX_HCI_FC_CH12_EN);
		MAC_REG_W32(R_AX_HCI_FC_CTRL_V1, val32);
	} else {
		val32 = MAC_REG_R32(R_AX_HCI_FC_CTRL);
		param->en = en ? 1 : 0;
		param->h2c_en = h2c_en ? 1 : 0;
		val32 = en ? (val32 | B_AX_HCI_FC_EN) : (val32 & ~B_AX_HCI_FC_EN);
		val32 = h2c_en ? (val32 | B_AX_HCI_FC_CH12_EN) :
				 (val32 & ~B_AX_HCI_FC_CH12_EN);
		MAC_REG_W32(R_AX_HCI_FC_CTRL, val32);
	}
}

u32 hfc_init(struct mac_ax_adapter *adapter, u8 rst, u8 en, u8 h2c_en)
{
	u8 ch;
	u32 ret = 0;

	if (rst)
		ret = hfc_reset_param(adapter);
	if (ret)
		return ret;

	ret = check_mac_en(adapter, 0, MAC_AX_DMAC_SEL);
	if (ret)
		return ret;

	hfc_func_en(adapter, 0, 0);

	if (!en && h2c_en) {
		hfc_h2c_cfg(adapter);
		hfc_func_en(adapter, en, h2c_en);
		return ret;
	}

	for (ch = MAC_AX_DMA_ACH0; ch < MAC_AX_DMA_H2C; ch++) {
		ret = hfc_ch_ctrl(adapter, ch);
		if (ret)
			return ret;
	}

	ret = hfc_pub_ctrl(adapter);
	if (ret)
		return ret;

	hfc_mix_cfg(adapter);
	if (en || h2c_en) {
		hfc_func_en(adapter, en, h2c_en);
		PLTFM_DELAY_US(10);
	}
	for (ch = MAC_AX_DMA_ACH0; ch < MAC_AX_DMA_H2C; ch++) {
		ret = hfc_upd_ch_info(adapter, ch);
		if (ret)
			return ret;
	}
	ret = hfc_upd_mix_info(adapter);
	return ret;
}

u32 hfc_info_init(struct mac_ax_adapter *adapter)
{
	adapter->hfc_param =
		(struct mac_ax_hfc_param *)
		PLTFM_MALLOC(sizeof(struct mac_ax_hfc_param));

	adapter->hfc_param->ch_cfg =
		(struct mac_ax_hfc_ch_cfg *)PLTFM_MALLOC(chcfg_size);

	adapter->hfc_param->ch_info =
		(struct mac_ax_hfc_ch_info *)PLTFM_MALLOC(chinfo_size);

	adapter->hfc_param->pub_cfg =
		(struct mac_ax_hfc_pub_cfg *)
		PLTFM_MALLOC(sizeof(struct mac_ax_hfc_pub_cfg));

	adapter->hfc_param->pub_info =
		(struct mac_ax_hfc_pub_info *)
		PLTFM_MALLOC(sizeof(struct mac_ax_hfc_pub_info));

	adapter->hfc_param->prec_cfg =
		(struct mac_ax_hfc_prec_cfg *)
		PLTFM_MALLOC(sizeof(struct mac_ax_hfc_prec_cfg));

	adapter->hfc_param->en = 0;
	adapter->hfc_param->h2c_en = 0;
	adapter->hfc_param->mode = 0;

	return MACSUCCESS;
}

u32 hfc_info_exit(struct mac_ax_adapter *adapter)
{
	PLTFM_FREE(adapter->hfc_param->prec_cfg,
		   sizeof(struct mac_ax_hfc_prec_cfg));

	PLTFM_FREE(adapter->hfc_param->pub_info,
		   sizeof(struct mac_ax_hfc_pub_info));

	PLTFM_FREE(adapter->hfc_param->pub_cfg,
		   sizeof(struct mac_ax_hfc_pub_cfg));

	PLTFM_FREE(adapter->hfc_param->ch_info, chinfo_size);
	PLTFM_FREE(adapter->hfc_param->ch_cfg, chcfg_size);
	PLTFM_FREE(adapter->hfc_param, sizeof(struct mac_ax_hfc_param));

	return MACSUCCESS;
}

