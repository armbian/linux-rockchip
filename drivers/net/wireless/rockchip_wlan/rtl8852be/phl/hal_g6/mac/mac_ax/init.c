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

#include "init.h"
#include "security_cam.h"
#include "hw.h"
#if MAC_AX_PCIE_SUPPORT
#include "_pcie.h"
#endif

static void _mp_core_swr_volt(struct mac_ax_adapter *adapter, u8 init)
{
	struct mac_ax_ops *mac_ops = adapter_to_mac_ops(adapter);
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	enum mac_ax_core_swr_volt v;
	u8 val8;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB))
		return;

	if (init) {
		val8 = MAC_REG_R8(R_AX_SPSLDO_ON_CTRL0);
		adapter->hw_info->core_swr_volt = GET_FIELD(val8, B_AX_VOL_L1);
	} else {
		v = MAC_AX_SWR_NORM;
		mac_ops->set_hw_value(adapter, MAC_AX_HW_SET_CORE_SWR_VOLT, &v);
	}
}

#if MAC_AX_PCIE_SUPPORT
static u32 _patch_pcie_pldr_polling_fail(struct mac_ax_adapter *adapter)
{
	u32 ret = MACSUCCESS;

	if (!(is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
	      is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)))
		return MACSUCCESS;

	ctrl_dma_all_pcie(adapter, MAC_AX_FUNC_DIS);

	ret = clr_idx_all_pcie(adapter);
	if (ret)
		return ret;

	ret = poll_dma_all_idle_pcie(adapter);
	if (ret)
		return ret;

	return ret;
}
#endif

#ifdef CONFIG_NEW_HALMAC_INTERFACE
struct mac_ax_adapter *get_mac_ax_adapter(enum mac_ax_intf intf,
					  u8 chip_id, u8 cv,
					  void *phl_adapter, void *drv_adapter,
					  struct mac_ax_pltfm_cb *pltfm_cb)
{
	struct mac_ax_adapter *adapter = NULL;

	switch (chip_id) {
#if MAC_AX_8852A_SUPPORT
	case MAC_AX_CHIP_ID_8852A:
		adapter = get_mac_8852a_adapter(intf, cv, phl_adapter,
						drv_adapter, pltfm_cb);
		break;
#endif
	default:
		return NULL;
	}

	return adapter;
}
#else
struct mac_ax_adapter *get_mac_ax_adapter(enum mac_ax_intf intf,
					  u8 chip_id, u8 cv,
					  void *drv_adapter,
					  struct mac_ax_pltfm_cb *pltfm_cb)
{
	struct mac_ax_adapter *adapter = NULL;

	switch (chip_id) {
#if MAC_AX_8852A_SUPPORT
	case MAC_AX_CHIP_ID_8852A:
		adapter = get_mac_8852a_adapter(intf, cv, drv_adapter,
						pltfm_cb);
		break;
#endif
#if MAC_AX_8852B_SUPPORT
	case MAC_AX_CHIP_ID_8852B:
		adapter = get_mac_8852b_adapter(intf, cv, drv_adapter,
						pltfm_cb);
		break;
#endif
#if MAC_AX_8852C_SUPPORT
	case MAC_AX_CHIP_ID_8852C:
		adapter = get_mac_8852c_adapter(intf, cv, drv_adapter,
						pltfm_cb);
		break;
#endif
#if MAC_AX_8192XB_SUPPORT
	case MAC_AX_CHIP_ID_8192XB:
		adapter = get_mac_8192xb_adapter(intf, cv, drv_adapter,
						 pltfm_cb);
		break;
#endif

	default:
		return NULL;
	}

	return adapter;
}
#endif

u32 hci_func_en(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret = MACSUCCESS;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		val32 = MAC_REG_R32(R_AX_HCI_FUNC_EN) |
			B_AX_HCI_TXDMA_EN | B_AX_HCI_RXDMA_EN;
		MAC_REG_W32(R_AX_HCI_FUNC_EN, val32);
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		val32 = MAC_REG_R32(R_AX_HCI_FUNC_EN) |
			B_AX_HCI_TXDMA_EN | B_AX_HCI_RXDMA_EN;
		MAC_REG_W32(R_AX_HCI_FUNC_EN, val32);
	} else {
		val32 = MAC_REG_R32(R_AX_HCI_FUNC_EN_V1) |
			B_AX_HCI_TXDMA_EN | B_AX_HCI_RXDMA_EN;
		MAC_REG_W32(R_AX_HCI_FUNC_EN_V1, val32);
	}
	return ret;
}

u32 dmac_pre_init(struct mac_ax_adapter *adapter, enum mac_ax_qta_mode mode, u8 fwdl)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		val32 = (B_AX_MAC_FUNC_EN | B_AX_DMAC_FUNC_EN |
		 B_AX_DISPATCHER_EN | B_AX_PKT_BUF_EN | B_AX_H_AXIDMA_EN);
		MAC_REG_W32(R_AX_DMAC_FUNC_EN, val32);

		adapter->sm.dmac_func = MAC_AX_FUNC_ON;

		val32 = MAC_REG_R32(R_AX_HAXI_INIT_CFG1);
		switch (adapter->hw_info->intf) {
		case MAC_AX_INTF_USB:
			val32 = SET_CLR_WORD(val32, DMA_MOD_USB,
					     B_AX_DMA_MODE);
			break;
		case MAC_AX_INTF_PCIE:
			val32 = SET_CLR_WORD(val32, DMA_MOD_PCIE_1B,
					     B_AX_DMA_MODE);
			break;
		case MAC_AX_INTF_SDIO:
			val32 = SET_CLR_WORD(val32, DMA_MOD_SDIO,
					     B_AX_DMA_MODE);
			break;
		default:
			PLTFM_MSG_ERR("[ERR]DMAC init with not support intf: %X\n",
				      adapter->hw_info->intf);
			return MACINTF;
		}
		val32 = (val32 & ~B_AX_STOP_AXI_MST) | B_AX_TXHCI_EN_V1 |
			B_AX_RXHCI_EN_V1;
		MAC_REG_W32(R_AX_HAXI_INIT_CFG1, val32);

		val32 = MAC_REG_R32(R_AX_HAXI_DMA_STOP1) &
			~(B_AX_STOP_ACH0 | B_AX_STOP_ACH1 |
			  B_AX_STOP_ACH3 | B_AX_STOP_ACH4 |
			  B_AX_STOP_ACH5 | B_AX_STOP_ACH6 |
			  B_AX_STOP_ACH7 | B_AX_STOP_CH8 |
			  B_AX_STOP_CH9 | B_AX_STOP_CH12);
		MAC_REG_W32(R_AX_HAXI_DMA_STOP1, val32);

		val32 = MAC_REG_R32(R_AX_HAXI_DMA_STOP2) &
			~(B_AX_STOP_CH10 | B_AX_STOP_CH11);
		MAC_REG_W32(R_AX_HAXI_DMA_STOP2, val32);

		val32 = MAC_REG_R32(R_AX_PLATFORM_ENABLE) | B_AX_AXIDMA_EN;
		MAC_REG_W32(R_AX_PLATFORM_ENABLE, val32);
	} else {
		val32 = (B_AX_MAC_FUNC_EN | B_AX_DMAC_FUNC_EN |
		 B_AX_DISPATCHER_EN | B_AX_PKT_BUF_EN);
		MAC_REG_W32(R_AX_DMAC_FUNC_EN, val32);

		adapter->sm.dmac_func = MAC_AX_FUNC_ON;
	}

	val32 = (B_AX_DISPATCHER_CLK_EN);
	MAC_REG_W32(R_AX_DMAC_CLK_EN, val32);

	if (!fwdl)
		return MACSUCCESS;

	ret = dle_init(adapter, MAC_AX_QTA_DLFW, mode);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]DLE pre init %d\n", ret);
		return ret;
	}

	ret = hfc_init(adapter, 1, 0, 1);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]HCI FC pre init %d\n", ret);
		return ret;
	}

	return ret;
}

u32 dmac_func_en(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret = 0;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		val32 = (B_AX_MAC_FUNC_EN | B_AX_DMAC_FUNC_EN | B_AX_MAC_SEC_EN |
		 B_AX_DISPATCHER_EN | B_AX_DLE_CPUIO_EN | B_AX_PKT_IN_EN |
		 B_AX_DMAC_TBL_EN | B_AX_PKT_BUF_EN | B_AX_STA_SCH_EN |
		 B_AX_TXPKT_CTRL_EN | B_AX_WD_RLS_EN | B_AX_MPDU_PROC_EN |
		 B_AX_DMAC_CRPRT | B_AX_H_AXIDMA_EN);
		MAC_REG_W32(R_AX_DMAC_FUNC_EN, val32);
	} else {
		val32 = (B_AX_MAC_FUNC_EN | B_AX_DMAC_FUNC_EN | B_AX_MAC_SEC_EN |
		 B_AX_DISPATCHER_EN | B_AX_DLE_CPUIO_EN | B_AX_PKT_IN_EN |
		 B_AX_DMAC_TBL_EN | B_AX_PKT_BUF_EN | B_AX_STA_SCH_EN |
		 B_AX_TXPKT_CTRL_EN | B_AX_WD_RLS_EN | B_AX_MPDU_PROC_EN |
		 B_AX_DMAC_CRPRT);
		MAC_REG_W32(R_AX_DMAC_FUNC_EN, val32);
	}

	val32 = (B_AX_MAC_SEC_CLK_EN | B_AX_DISPATCHER_CLK_EN |
		 B_AX_DLE_CPUIO_CLK_EN | B_AX_PKT_IN_CLK_EN |
		 B_AX_STA_SCH_CLK_EN | B_AX_TXPKT_CTRL_CLK_EN |
		 B_AX_WD_RLS_CLK_EN | B_AX_BBRPT_CLK_EN);
	MAC_REG_W32(R_AX_DMAC_CLK_EN, val32);

	adapter->sm.dmac_func = MAC_AX_FUNC_ON;

	return ret;
}

u32 cmac_func_en(struct mac_ax_adapter *adapter, u8 band, u8 en)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32_func_en = 0;
	u32 val32_ck_en = 0;
	u32 val32_c1pc_en = 0;
	u32 addrl_func_en[] = {R_AX_CMAC_FUNC_EN, R_AX_CMAC_FUNC_EN_C1};
	u32 addrl_ck_en[] = {R_AX_CK_EN, R_AX_CK_EN_C1};

	val32_func_en = B_AX_CMAC_EN | B_AX_CMAC_TXEN | B_AX_CMAC_RXEN |
			B_AX_PHYINTF_EN | B_AX_CMAC_DMA_EN | B_AX_PTCLTOP_EN |
			B_AX_SCHEDULER_EN | B_AX_TMAC_EN | B_AX_RMAC_EN |
			B_AX_CMAC_CRPRT;
	val32_ck_en = B_AX_CMAC_CKEN | B_AX_PHYINTF_CKEN | B_AX_CMAC_DMA_CKEN |
		      B_AX_PTCLTOP_CKEN | B_AX_SCHEDULER_CKEN | B_AX_TMAC_CKEN |
		      B_AX_RMAC_CKEN;
	val32_c1pc_en = B_AX_R_SYM_WLCMAC1_PC_EN |
			B_AX_R_SYM_WLCMAC1_P1_PC_EN |
			B_AX_R_SYM_WLCMAC1_P2_PC_EN |
			B_AX_R_SYM_WLCMAC1_P3_PC_EN |
			B_AX_R_SYM_WLCMAC1_P4_PC_EN;

	if (band >= MAC_AX_BAND_NUM) {
		PLTFM_MSG_ERR("band %d invalid\n", band);
		return MACFUNCINPUT;
	}

	if (en) {
		if (band == MAC_AX_BAND_1) {
			MAC_REG_W32(R_AX_AFE_CTRL1,
				    MAC_REG_R32(R_AX_AFE_CTRL1) |
				    val32_c1pc_en);
			MAC_REG_W32(R_AX_SYS_ISO_CTRL_EXTEND,
				    MAC_REG_R32(R_AX_SYS_ISO_CTRL_EXTEND) &
				    ~B_AX_R_SYM_ISO_CMAC12PP);
			MAC_REG_W32(R_AX_SYS_ISO_CTRL_EXTEND,
				    MAC_REG_R32(R_AX_SYS_ISO_CTRL_EXTEND) |
				    B_AX_CMAC1_FEN);
		}
		MAC_REG_W32(addrl_ck_en[band],
			    MAC_REG_R32(addrl_ck_en[band]) | val32_ck_en);
		MAC_REG_W32(addrl_func_en[band],
			    MAC_REG_R32(addrl_func_en[band]) | val32_func_en);
	} else {
		MAC_REG_W32(addrl_func_en[band],
			    MAC_REG_R32(addrl_func_en[band]) & ~val32_func_en);
		MAC_REG_W32(addrl_ck_en[band],
			    MAC_REG_R32(addrl_ck_en[band]) & ~val32_ck_en);
		if (band == MAC_AX_BAND_1) {
			MAC_REG_W32(R_AX_SYS_ISO_CTRL_EXTEND,
				    MAC_REG_R32(R_AX_SYS_ISO_CTRL_EXTEND) &
				    ~B_AX_CMAC1_FEN);
			MAC_REG_W32(R_AX_SYS_ISO_CTRL_EXTEND,
				    MAC_REG_R32(R_AX_SYS_ISO_CTRL_EXTEND) |
				    B_AX_R_SYM_ISO_CMAC12PP);
			MAC_REG_W32(R_AX_AFE_CTRL1,
				    MAC_REG_R32(R_AX_AFE_CTRL1) &
				    ~val32_c1pc_en);
		}
	}

	if (band == MAC_AX_BAND_0)
		adapter->sm.cmac0_func = en ? MAC_AX_FUNC_ON : MAC_AX_FUNC_OFF;
	else
		adapter->sm.cmac1_func = en ? MAC_AX_FUNC_ON : MAC_AX_FUNC_OFF;

	return MACSUCCESS;
}

u32 chip_func_en(struct mac_ax_adapter *adapter)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	u32 val32;
	u32 ret = MACSUCCESS;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A) ||
	    is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		/* patch for OCP */
		val32 = MAC_REG_R32(R_AX_SPSLDO_ON_CTRL0);
		val32 |= SET_WOR2(B_AX_OCP_L1_MSK, B_AX_OCP_L1_SH,
				  B_AX_OCP_L1_MSK);
		MAC_REG_W32(R_AX_SPSLDO_ON_CTRL0, val32);
	}

	return ret;
}

u32 mac_sys_init(struct mac_ax_adapter *adapter)
{
	u32 ret;

	ret = dmac_func_en(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]DMAC en %d\n", ret);
		return ret;
	}

	ret = cmac_func_en(adapter, MAC_AX_BAND_0, MAC_AX_FUNC_EN);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]CMAC %d en %d %d\n",
			      MAC_AX_BAND_0, MAC_AX_FUNC_EN, ret);
		return ret;
	}

	ret = chip_func_en(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]chip en %d\n", ret);
		return ret;
	}

	return ret;
}

u32 mac_hal_init(struct mac_ax_adapter *adapter,
		 struct mac_ax_trx_info *trx_info,
		 struct mac_ax_fwdl_info *fwdl_info,
		 struct mac_ax_intf_info *intf_info)
{
	struct mac_ax_ops *mac_ops = adapter_to_mac_ops(adapter);
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hw_info *hw_info = adapter->hw_info;
#if MAC_AX_FEATURE_DBGPKG
	struct mac_ax_dbgpkg dbg_val = {0};
	struct mac_ax_dbgpkg_en dbg_en = {0};
#endif
	u32 ret;
	u32 rom_addr;
	u8 fwdl_en;

	_mp_core_swr_volt(adapter, 1);

	ret = mac_ops->pwr_switch(adapter, 1);
	if (ret == MACALRDYON) {
		ret = mac_ops->pwr_switch(adapter, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]pwr_switch 0 fail %d\n", ret);
			goto end;
		}
		ret = mac_ops->pwr_switch(adapter, 1);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]pwr_switch 0->1 fail %d\n", ret);
			goto end;
		}
	}
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pwr_switch 1 fail %d\n", ret);
		goto end;
	}
#if MAC_AX_PCIE_SUPPORT
	if (adapter->hw_info->intf == MAC_AX_INTF_PCIE) {
		ret = _patch_pcie_pldr_polling_fail(adapter);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]_patch_pcie_pldr_polling_fail %d\n", ret);
			goto end;
		}
	}
#endif
	ret = hci_func_en(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]hci_func_en %d\n", ret);
		goto end;
	}

	fwdl_en = fwdl_info->fw_en &&
		  (fwdl_info->dlrom_en || fwdl_info->dlram_en) ? 1 : 0;
	ret = dmac_pre_init(adapter, trx_info->qta_mode, fwdl_en);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]fwdl_pre_init %d\n", ret);
		goto end;
	}

	ret = ops->intf_pre_init(adapter, intf_info);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]intf_pre_init %d\n", ret);
		goto end;
	}

	if (fwdl_info->fw_en) {
		if (fwdl_info->dlrom_en) {
			switch (hw_info->chip_id) {
			case MAC_AX_CHIP_ID_8852A:
				rom_addr = RTL8852A_ROM_ADDR;
				break;
			case MAC_AX_CHIP_ID_8852B:
				rom_addr = RTL8852B_ROM_ADDR;
				break;
			case MAC_AX_CHIP_ID_8852C:
				rom_addr = RTL8852C_ROM_ADDR;
				break;
			case MAC_AX_CHIP_ID_8192XB:
				rom_addr = RTL8192XB_ROM_ADDR;
				break;
			default:
				PLTFM_MSG_ERR("[ERR]chip id\n");
				return MACNOITEM;
			}
			ret = mac_ops->romdl(adapter,
					     fwdl_info->rom_buff,
					     rom_addr,
					     fwdl_info->rom_size);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]romdl %d\n", ret);
				goto end;
			}
		}

		if (fwdl_info->dlram_en) {
			if (fwdl_info->fw_from_hdr) {
				ret = mac_ops->enable_fw(adapter,
							 fwdl_info->fw_cat);
				if (ret != MACSUCCESS) {
					PLTFM_MSG_ERR("[ERR]enable_fw %d\n",
						      ret);
					goto end;
				}
			} else {
				ret = mac_ops->enable_cpu(adapter, 0,
							  fwdl_info->dlram_en);
				if (ret != MACSUCCESS) {
					PLTFM_MSG_ERR("[ERR]enable_cpu %d\n",
						      ret);
					goto end;
				}

				ret = mac_ops->fwdl(adapter,
						    fwdl_info->ram_buff,
						    fwdl_info->ram_size);
				if (ret != MACSUCCESS) {
					PLTFM_MSG_ERR("[ERR]fwdl %d\n", ret);
					goto end;
				}
			}
		}
	}

	ret = set_enable_bb_rf(adapter, MAC_AX_FUNC_EN);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]set_enable_bb_rf %d\n", ret);
		goto end;
	}

	ret = mac_ops->sys_init(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]sys_init %d\n", ret);
		goto end;
	}

	ret = mac_ops->trx_init(adapter, trx_info);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]trx_init %d\n", ret);
		goto end;
	}

	ret = ops->intf_init(adapter, intf_info);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]intf_init %d\n", ret);
		goto end;
	}

end:
	if (ret != MACSUCCESS) {
		adapter->sm.mac_rdy = MAC_AX_MAC_INIT_ERR;
		PLTFM_MSG_ERR("[ERR]hal_init fail %d\n", ret);
#if MAC_AX_FEATURE_DBGPKG
		dbg_en.ss_dbg = 1;
		dbg_en.dle_dbg = 1;
		dbg_en.dmac_dbg = 1;
		dbg_en.cmac_dbg = 1;
		dbg_en.mac_dbg_port = 1;
		dbg_en.plersvd_dbg = 1;
		mac_ops->dbg_status_dump(adapter, &dbg_val, &dbg_en);
#endif
	} else {
		adapter->sm.mac_rdy = MAC_AX_MAC_RDY;
	}

	return ret;
}

u32 mac_hal_deinit(struct mac_ax_adapter *adapter)
{
	struct mac_ax_ops *ops = adapter_to_mac_ops(adapter);
	struct mac_ax_intf_ops *intf_ops = adapter_to_intf_ops(adapter);
#if MAC_AX_FEATURE_DBGPKG
	struct mac_ax_dbgpkg dbg_val = {0};
	struct mac_ax_dbgpkg_en dbg_en = {0};
#endif
	u32 ret;

	adapter->sm.mac_rdy = MAC_AX_MAC_NOT_RDY;

	_mp_core_swr_volt(adapter, 1);

	ret = rst_port_info(adapter, MAC_AX_BAND_0);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]reset port info %d\n", ret);
		return ret;
	}

	if (!is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		ret = rst_port_info(adapter, MAC_AX_BAND_1);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]reset port info %d\n", ret);
			return ret;
		}
	}

	ret = rst_p2p_info(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]reset p2p info %d\n", ret);
		return ret;
	}

	ret = rst_dbcc_info(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]reset dbcc info %d\n", ret);
		return ret;
	}

	ret = free_sec_info_tbl(adapter, SEC_CAM_NORMAL);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]remove security info tbl\n");
		return ret;
	}

	ret = mac_remove_role_by_band(adapter, MAC_AX_BAND_0, 1);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]remove band0 role fail\n");
		return ret;
	}

	ret = mac_remove_role_by_band(adapter, MAC_AX_BAND_1, 1);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]remove band0 role fail\n");
		return ret;
	}

	ret = intf_ops->intf_deinit(adapter, NULL);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]intf deinit\n");
		goto end;
	}

	ret = ops->pwr_switch(adapter, 0);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pwr switch off\n");
		goto end;
	}

end:
	if (ret != MACSUCCESS) {
		adapter->sm.mac_rdy = MAC_AX_MAC_DEINIT_ERR;
		PLTFM_MSG_ERR("[ERR]hal_deinit fail %d\n", ret);
#if MAC_AX_FEATURE_DBGPKG
		dbg_en.ss_dbg = 1;
		dbg_en.dle_dbg = 1;
		dbg_en.dmac_dbg = 1;
		dbg_en.cmac_dbg = 1;
		dbg_en.mac_dbg_port = 1;
		dbg_en.plersvd_dbg = 1;
		ops->dbg_status_dump(adapter, &dbg_val, &dbg_en);
#endif
	}

	return ret;
}

u32 mac_hal_fast_init(struct mac_ax_adapter *adapter,
		      struct mac_ax_trx_info *trx_info,
		      struct mac_ax_fwdl_info *fwdl_info,
		      struct mac_ax_intf_info *intf_info)
{
	struct mac_ax_ops *mac_ops = adapter_to_mac_ops(adapter);
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_hw_info *hw_info = adapter->hw_info;
#if MAC_AX_FEATURE_DBGPKG
	struct mac_ax_dbgpkg dbg_val = {0};
	struct mac_ax_dbgpkg_en dbg_en = {0};
#endif
	u32 rom_addr;
	u32 ret;
	u8 fwdl_en;

	ret = mac_ops->pwr_switch(adapter, 1);
	if (ret == MACALRDYON) {
		ret = mac_ops->pwr_switch(adapter, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]pwr_switch 0 fail %d\n", ret);
			goto end;
		}
		ret = mac_ops->pwr_switch(adapter, 1);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]pwr_switch 0->1 fail %d\n", ret);
			goto end;
		}
	}
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]pwr_switch 1 fail %d\n", ret);
		goto end;
	}
#if MAC_AX_PCIE_SUPPORT
	if (adapter->hw_info->intf == MAC_AX_INTF_PCIE) {
		ret = _patch_pcie_pldr_polling_fail(adapter);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("[ERR]_patch_pcie_pldr_polling_fail %d\n", ret);
			goto end;
		}
	}
#endif
	ret = hci_func_en(adapter);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]hci_func_en %d\n", ret);
		goto end;
	}

	fwdl_en = fwdl_info->fw_en &&
		  (fwdl_info->dlrom_en || fwdl_info->dlram_en) ? 1 : 0;
	ret = dmac_pre_init(adapter, trx_info->qta_mode, fwdl_en);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]fwdl_pre_init %d\n", ret);
		goto end;
	}

	ret = ops->intf_pre_init(adapter, intf_info);
	if (ret != MACSUCCESS) {
		PLTFM_MSG_ERR("[ERR]intf_pre_init %d\n", ret);
		goto end;
	}

	if (fwdl_info->fw_en) {
		if (fwdl_info->dlrom_en) {
			switch (hw_info->chip_id) {
			case MAC_AX_CHIP_ID_8852A:
				rom_addr = RTL8852A_ROM_ADDR;
				break;
			case MAC_AX_CHIP_ID_8852B:
				rom_addr = RTL8852B_ROM_ADDR;
				break;
			case MAC_AX_CHIP_ID_8852C:
				rom_addr = RTL8852C_ROM_ADDR;
				break;
			case MAC_AX_CHIP_ID_8192XB:
				rom_addr = RTL8192XB_ROM_ADDR;
				break;
			default:
				PLTFM_MSG_ERR("[ERR]chip id\n");
				return MACNOITEM;
			}
			ret = mac_ops->romdl(adapter,
					     fwdl_info->rom_buff,
					     rom_addr,
					     fwdl_info->rom_size);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("[ERR]romdl %d\n", ret);
				goto end;
			}
		}

		if (fwdl_info->dlram_en) {
			if (fwdl_info->fw_from_hdr) {
				ret = mac_ops->enable_fw(adapter,
							 fwdl_info->fw_cat);
				if (ret != MACSUCCESS) {
					PLTFM_MSG_ERR("[ERR]enable_fw %d\n",
						      ret);
					goto end;
				}
			} else {
				ret = mac_ops->enable_cpu(adapter, 0,
							  fwdl_info->dlram_en);
				if (ret != MACSUCCESS) {
					PLTFM_MSG_ERR("[ERR]enable_cpu %d\n",
						      ret);
					goto end;
				}

				ret = mac_ops->fwdl(adapter,
						    fwdl_info->ram_buff,
						    fwdl_info->ram_size);
				if (ret != MACSUCCESS) {
					PLTFM_MSG_ERR("[ERR]fwdl %d\n", ret);
					goto end;
				}
			}
		}
	}

end:
	if (ret != MACSUCCESS) {
		adapter->sm.mac_rdy = MAC_AX_MAC_FINIT_ERR;
		PLTFM_MSG_ERR("[ERR]hal_fast_init fail %d\n", ret);
#if MAC_AX_FEATURE_DBGPKG
		dbg_en.ss_dbg = 1;
		dbg_en.dle_dbg = 1;
		dbg_en.dmac_dbg = 1;
		dbg_en.cmac_dbg = 1;
		dbg_en.mac_dbg_port = 1;
		dbg_en.plersvd_dbg = 1;
		mac_ops->dbg_status_dump(adapter, &dbg_val, &dbg_en);
#endif
	} else {
		adapter->sm.mac_rdy = MAC_AX_MAC_RDY;
	}

	return ret;
}

u32 mac_hal_fast_deinit(struct mac_ax_adapter *adapter)
{
	struct mac_ax_ops *ops = adapter_to_mac_ops(adapter);
	struct mac_ax_intf_ops *intf_ops = adapter_to_intf_ops(adapter);
#if MAC_AX_FEATURE_DBGPKG
	struct mac_ax_dbgpkg dbg_val = {0};
	struct mac_ax_dbgpkg_en dbg_en = {0};
#endif
	u32 ret;

	adapter->sm.mac_rdy = MAC_AX_MAC_NOT_RDY;

	ret = intf_ops->intf_deinit(adapter, NULL);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]intf deinit\n");
		goto end;
	}

	ret = ops->pwr_switch(adapter, 0);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]pwr switch off\n");
		goto end;
	}
end:
	if (ret != MACSUCCESS) {
		adapter->sm.mac_rdy = MAC_AX_MAC_FDEINIT_ERR;
		PLTFM_MSG_ERR("[ERR]hal_fast_deinit fail %d\n", ret);
#if MAC_AX_FEATURE_DBGPKG
		dbg_en.ss_dbg = 1;
		dbg_en.dle_dbg = 1;
		dbg_en.dmac_dbg = 1;
		dbg_en.cmac_dbg = 1;
		dbg_en.mac_dbg_port = 1;
		dbg_en.plersvd_dbg = 1;
		ops->dbg_status_dump(adapter, &dbg_val, &dbg_en);
#endif
	}

	return ret;
}

u32 mac_ax_init_state(struct mac_ax_adapter *adapter)
{
	struct mac_ax_state_mach sm = MAC_AX_DFLT_SM;

	adapter->sm = sm;
	adapter->fw_info.h2c_seq = 0;
	adapter->fw_info.rec_seq = 0;

	return MACSUCCESS;
}

u32 mix_info_init(struct mac_ax_adapter *adapter)
{
	PLTFM_MUTEX_INIT(&adapter->fw_info.seq_lock);
	PLTFM_MUTEX_INIT(&adapter->fw_info.msg_reg);
	PLTFM_MUTEX_INIT(&adapter->flash_info.lock);
	PLTFM_MUTEX_INIT(&adapter->hw_info->ind_access_lock);
	PLTFM_MUTEX_INIT(&adapter->hw_info->lte_rlock);
	PLTFM_MUTEX_INIT(&adapter->hw_info->lte_wlock);
	PLTFM_MUTEX_INIT(&adapter->hw_info->dbg_port_lock);
	PLTFM_MUTEX_INIT(&adapter->cmd_ofld_info.cmd_ofld_lock);
	PLTFM_MUTEX_INIT(&adapter->hw_info->err_set_lock);
	PLTFM_MUTEX_INIT(&adapter->hw_info->err_get_lock);
#if MAC_AX_PCIE_SUPPORT
	PLTFM_MUTEX_INIT(&adapter->hw_info->dbi_lock);
	PLTFM_MUTEX_INIT(&adapter->hw_info->mdio_lock);
#endif
	PLTFM_MUTEX_INIT(&adapter->h2c_agg_info.h2c_agg_lock);
	PLTFM_MUTEX_INIT(&adapter->scanofld_info.drv_chlist_state_lock);
	PLTFM_MUTEX_INIT(&adapter->scanofld_info.fw_chlist_state_lock);
	adapter->hw_info->ind_aces_cnt = 0;
	adapter->hw_info->dbg_port_cnt = 0;

	return MACSUCCESS;
}

u32 mix_info_exit(struct mac_ax_adapter *adapter)
{
	PLTFM_MUTEX_DEINIT(&adapter->fw_info.seq_lock);
	PLTFM_MUTEX_DEINIT(&adapter->fw_info.msg_reg);
	PLTFM_MUTEX_DEINIT(&adapter->flash_info.lock);
	PLTFM_MUTEX_DEINIT(&adapter->hw_info->ind_access_lock);
	PLTFM_MUTEX_DEINIT(&adapter->hw_info->lte_rlock);
	PLTFM_MUTEX_DEINIT(&adapter->hw_info->lte_wlock);
	PLTFM_MUTEX_DEINIT(&adapter->hw_info->dbg_port_lock);
	PLTFM_MUTEX_DEINIT(&adapter->cmd_ofld_info.cmd_ofld_lock);
	PLTFM_MUTEX_DEINIT(&adapter->hw_info->err_set_lock);
	PLTFM_MUTEX_DEINIT(&adapter->hw_info->err_get_lock);
#if MAC_AX_PCIE_SUPPORT
	PLTFM_MUTEX_DEINIT(&adapter->hw_info->dbi_lock);
	PLTFM_MUTEX_DEINIT(&adapter->hw_info->mdio_lock);
#endif
	PLTFM_MUTEX_DEINIT(&adapter->h2c_agg_info.h2c_agg_lock);
	PLTFM_MUTEX_DEINIT(&adapter->scanofld_info.drv_chlist_state_lock);
	PLTFM_MUTEX_DEINIT(&adapter->scanofld_info.fw_chlist_state_lock);
	adapter->hw_info->ind_aces_cnt = 0;
	adapter->hw_info->dbg_port_cnt = 0;

	return MACSUCCESS;
}
