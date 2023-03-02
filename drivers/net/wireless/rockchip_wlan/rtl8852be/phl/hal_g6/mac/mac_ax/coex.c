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
#include "coex.h"
#include "../mac_reg.h"
#include "hw.h"
#include "power_saving.h"

#define MAC_AX_RTK_RATE 5

#define MAC_AX_BT_MODE_0_3 0
#define MAC_AX_BT_MODE_2 2

#define MAC_AX_CSR_DELAY 0
#define MAC_AX_CSR_PRI_TO 5
#define MAC_AX_CSR_TRX_TO 4

#define MAC_AX_CSR_RATE 80

#define MAC_AX_SB_DRV_MSK 0xFFFFFF
#define MAC_AX_SB_DRV_SH 0
#define MAC_AX_SB_FW_MSK 0x7F
#define MAC_AX_SB_FW_SH 24

#define R_AX_LTECOEX_STATUS 0x54
#define B_AX_GNT_BT_RFC_S0_STA BIT(3)
#define B_AX_GNT_WL_RFC_S0_STA BIT(2)
#define B_AX_GNT_BT_RFC_S1_STA BIT(5)
#define B_AX_GNT_WL_RFC_S1_STA BIT(4)

#define MAC_AX_BTGS1_NOTIFY BIT(0)

u32 mac_coex_init(struct mac_ax_adapter *adapter,
		  struct mac_ax_coex *coex)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 val;
	u16 val16;
	u32 ret, val32;

	val = MAC_REG_R8(R_AX_GPIO_MUXCFG);
	MAC_REG_W8(R_AX_GPIO_MUXCFG, val | B_AX_ENBT);

	switch (coex->direction) {
	case MAC_AX_COEX_INNER:
		val = MAC_REG_R8(R_AX_GPIO_MUXCFG + 1);
		val = (val & ~BIT(2)) | BIT(1);
		MAC_REG_W8(R_AX_GPIO_MUXCFG + 1, val);
		break;
	case MAC_AX_COEX_OUTPUT:
		val = MAC_REG_R8(R_AX_GPIO_MUXCFG + 1);
		val = val | BIT(1) | BIT(0);
		MAC_REG_W8(R_AX_GPIO_MUXCFG + 1, val);
		break;
	case MAC_AX_COEX_INPUT:
		val = MAC_REG_R8(R_AX_GPIO_MUXCFG + 1);
		val = val & ~(BIT(2) | BIT(1));
		MAC_REG_W8(R_AX_GPIO_MUXCFG + 1, val);
		break;
	default:
		return MACNOITEM;
	}

#if MAC_AX_FW_REG_OFLD
	if (adapter->sm.fwdl == MAC_AX_FWDL_INIT_RDY) {
		ret = MAC_REG_W_OFLD(R_AX_BTC_FUNC_EN, B_AX_PTA_WL_TX_EN,
				     1, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("%s: write offload fail %d",
				      __func__, ret);
			return ret;
		}
		ret = MAC_REG_W_OFLD(R_AX_BT_COEX_CFG_2,
				     B_AX_GNT_BT_BYPASS_PRIORITY,
				     1, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("%s: write offload fail %d",
				      __func__, ret);
			return ret;
		}
		ret = MAC_REG_W_OFLD(R_AX_CSR_MODE,
				     B_AX_WL_ACT_MSK | B_AX_STATIS_BT_EN |
				     B_AX_BT_CNT_REST,
				     0x4003, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("%s: write offload fail %d",
				      __func__, ret);
			return ret;
		}
		ret = MAC_REG_W_OFLD(R_AX_TRXPTCL_RESP_0, B_AX_RSP_CHK_BTCCA,
				     0, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("%s: write offload fail %d",
				      __func__, ret);
			return ret;
		}
		ret = MAC_REG_W_OFLD(R_AX_CCA_CFG_0,
				     B_AX_BTCCA_BRK_TXOP_EN | B_AX_BTCCA_EN,
				     1, 0);
		if (ret != MACSUCCESS) {
			PLTFM_MSG_ERR("%s: write offload fail %d",
				      __func__, ret);
			return ret;
		}
		switch (coex->pta_mode) {
		case MAC_AX_COEX_RTK_MODE:
			val = MAC_REG_R8(R_AX_GPIO_MUXCFG);
			val = SET_CLR_WORD(val, MAC_AX_BT_MODE_0_3,
					   B_AX_BTMODE);
			MAC_REG_W8(R_AX_GPIO_MUXCFG, val);

			ret = MAC_REG_W_OFLD(R_AX_TDMA_MODE,
					     B_AX_RTK_BT_ENABLE,
					     1, 0);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("%s: write offload fail %d",
					      __func__, ret);
				return ret;
			}
			ret = MAC_REG_W_OFLD(R_AX_BT_COEX_CFG_5,
					     B_AX_BT_RPT_SAMPLE_RATE_MSK <<
					     B_AX_BT_RPT_SAMPLE_RATE_SH,
					     MAC_AX_RTK_RATE, 1);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("%s: write offload fail %d",
					      __func__, ret);
				return ret;
			}
			break;
		case MAC_AX_COEX_CSR_MODE:
			val = MAC_REG_R8(R_AX_GPIO_MUXCFG);
			val = SET_CLR_WORD(val, MAC_AX_BT_MODE_2, B_AX_BTMODE);
			MAC_REG_W8(R_AX_GPIO_MUXCFG, val);

			ret = MAC_REG_W_OFLD(R_AX_CSR_MODE,
					     B_AX_BT_PRI_DETECT_TO_MSK <<
					     B_AX_BT_PRI_DETECT_TO_SH,
					     MAC_AX_CSR_PRI_TO, 0);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("%s: write offload fail %d",
					      __func__, ret);
				return ret;
			}
			ret = MAC_REG_W_OFLD(R_AX_CSR_MODE,
					     B_AX_BT_TRX_INIT_DETECT_MSK <<
					     B_AX_BT_TRX_INIT_DETECT_SH,
					     MAC_AX_CSR_TRX_TO, 0);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("%s: write offload fail %d",
					      __func__, ret);
				return ret;
			}
			ret = MAC_REG_W_OFLD(R_AX_CSR_MODE,
					     B_AX_BT_STAT_DELAY_MSK <<
					     B_AX_BT_STAT_DELAY_SH,
					     MAC_AX_CSR_DELAY, 0);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("%s: write offload fail %d",
					      __func__, ret);
				return ret;
			}
			ret = MAC_REG_W8_OFLD(R_AX_BT_COEX_CFG_2,
					      MAC_AX_CSR_RATE, 1);
			if (ret != MACSUCCESS) {
				PLTFM_MSG_ERR("%s: write offload fail %d",
					      __func__, ret);
				return ret;
			}
			break;
		default:
			return MACNOITEM;
		}

		return ret;
	}
#endif

	val = MAC_REG_R8(R_AX_BTC_FUNC_EN);
	MAC_REG_W8(R_AX_BTC_FUNC_EN, val | B_AX_PTA_WL_TX_EN);

	val = MAC_REG_R8(R_AX_BT_COEX_CFG_2 + 1);
	MAC_REG_W8(R_AX_BT_COEX_CFG_2 + 1, val | BIT(0));

	val = MAC_REG_R8(R_AX_CSR_MODE);
	MAC_REG_W8(R_AX_CSR_MODE, val | B_AX_STATIS_BT_EN | B_AX_WL_ACT_MSK);

	val = MAC_REG_R8(R_AX_CSR_MODE + 2);
	MAC_REG_W8(R_AX_CSR_MODE + 2, val | BIT(0));

	val = MAC_REG_R8(R_AX_TRXPTCL_RESP_0 + 3);
	MAC_REG_W8(R_AX_TRXPTCL_RESP_0 + 3, val & ~BIT(1));

	val16 = MAC_REG_R16(R_AX_CCA_CFG_0);
	val16 = (val16 | B_AX_BTCCA_EN) & ~B_AX_BTCCA_BRK_TXOP_EN;
	MAC_REG_W16(R_AX_CCA_CFG_0, val16);

	ret = mac_read_lte(adapter, R_AX_LTE_SW_CFG_2, &val32);
	if (ret) {
		PLTFM_MSG_ERR("%s: Read LTE fail!\n", __func__);
		return ret;
	}
	val32 = val32 & B_AX_WL_RX_CTRL;
	ret = mac_write_lte(adapter, R_AX_LTE_SW_CFG_2, val32);
	if (ret) {
		PLTFM_MSG_ERR("%s: Write LTE fail!\n", __func__);
		return ret;
	}

	switch (coex->pta_mode) {
	case MAC_AX_COEX_RTK_MODE:
		val = MAC_REG_R8(R_AX_GPIO_MUXCFG);
		val = SET_CLR_WORD(val, MAC_AX_BT_MODE_0_3,
				   B_AX_BTMODE);
		MAC_REG_W8(R_AX_GPIO_MUXCFG, val);

		val = MAC_REG_R8(R_AX_TDMA_MODE);
		MAC_REG_W8(R_AX_TDMA_MODE, val | B_AX_RTK_BT_ENABLE);

		val = MAC_REG_R8(R_AX_BT_COEX_CFG_5);
		val = SET_CLR_WORD(val, MAC_AX_RTK_RATE,
				   B_AX_BT_RPT_SAMPLE_RATE);
		MAC_REG_W8(R_AX_BT_COEX_CFG_5, val);
		break;
	case MAC_AX_COEX_CSR_MODE:
		val = MAC_REG_R8(R_AX_GPIO_MUXCFG);
		val = SET_CLR_WORD(val, MAC_AX_BT_MODE_2, B_AX_BTMODE);
		MAC_REG_W8(R_AX_GPIO_MUXCFG, val);

		val16 = MAC_REG_R16(R_AX_CSR_MODE);
		val16 = SET_CLR_WORD(val16, MAC_AX_CSR_PRI_TO,
				     B_AX_BT_PRI_DETECT_TO);
		val16 = SET_CLR_WORD(val16, MAC_AX_CSR_TRX_TO,
				     B_AX_BT_TRX_INIT_DETECT);
		val16 = SET_CLR_WORD(val16, MAC_AX_CSR_DELAY,
				     B_AX_BT_STAT_DELAY);
		val16 = val16 | B_AX_ENHANCED_BT;
		MAC_REG_W16(R_AX_CSR_MODE, val16);

		MAC_REG_W8(R_AX_BT_COEX_CFG_2, MAC_AX_CSR_RATE);
		break;
	default:
		return MACNOITEM;
	}

	return MACSUCCESS;
}

u32 mac_get_gnt(struct mac_ax_adapter *adapter, struct mac_ax_coex_gnt *gnt_cfg)
{
	u32 val, ret, status;
	struct mac_ax_gnt *gnt;

	ret = mac_read_lte(adapter, R_AX_LTE_SW_CFG_1, &val);
	if (ret) {
		PLTFM_MSG_ERR("Read LTE fail!\n");
		return ret;
	}

	ret = mac_read_lte(adapter, R_AX_LTECOEX_STATUS, &status);
	if (ret) {
		PLTFM_MSG_ERR("Read LTE fail!\n");
		return ret;
	}

	gnt = &gnt_cfg->band0;
	gnt->gnt_bt_sw_en = !!(val & B_AX_GNT_BT_RFC_S0_SW_CTRL);
	gnt->gnt_bt = !!(status & B_AX_GNT_BT_RFC_S0_STA);
	gnt->gnt_wl_sw_en = !!(val & B_AX_GNT_WL_RFC_S0_SW_CTRL);
	gnt->gnt_wl = !!(status & B_AX_GNT_WL_RFC_S0_STA);

	gnt = &gnt_cfg->band1;
	gnt->gnt_bt_sw_en = !!(val & B_AX_GNT_BT_RFC_S1_SW_CTRL);
	gnt->gnt_bt = !!(status & B_AX_GNT_BT_RFC_S1_STA);
	gnt->gnt_wl_sw_en = !!(val & B_AX_GNT_WL_RFC_S1_SW_CTRL);
	gnt->gnt_wl = !!(status & B_AX_GNT_WL_RFC_S1_STA);

	return MACSUCCESS;
}

u32 mac_cfg_gnt(struct mac_ax_adapter *adapter, struct mac_ax_coex_gnt *gnt_cfg)
{
	u32 val, ret;

	ret = mac_read_lte(adapter, R_AX_LTE_SW_CFG_1, &val);
	if (ret) {
		PLTFM_MSG_ERR("Read LTE fail!\n");
		return ret;
	}
	val = (gnt_cfg->band0.gnt_bt ? (B_AX_GNT_BT_RFC_S0_SW_VAL |
		 B_AX_GNT_BT_BB_S0_SW_VAL) : 0) |
		(gnt_cfg->band0.gnt_bt_sw_en ?
		 (B_AX_GNT_BT_RFC_S0_SW_CTRL |
		  B_AX_GNT_BT_BB_S0_SW_CTRL) : 0) |
		(gnt_cfg->band0.gnt_wl ? (B_AX_GNT_WL_RFC_S0_SW_VAL |
					  B_AX_GNT_WL_BB_S0_SW_VAL) : 0) |
		(gnt_cfg->band0.gnt_wl_sw_en ?
		 (B_AX_GNT_WL_RFC_S0_SW_CTRL |
		  B_AX_GNT_WL_BB_S0_SW_CTRL) : 0) |
		(gnt_cfg->band1.gnt_bt ? (B_AX_GNT_BT_RFC_S1_SW_VAL |
					  B_AX_GNT_BT_BB_S1_SW_VAL) : 0) |
		(gnt_cfg->band1.gnt_bt_sw_en ?
		 (B_AX_GNT_BT_RFC_S1_SW_CTRL |
		  B_AX_GNT_BT_BB_S1_SW_CTRL) : 0) |
		(gnt_cfg->band1.gnt_wl ? (B_AX_GNT_WL_RFC_S1_SW_VAL |
					  B_AX_GNT_WL_BB_S1_SW_VAL) : 0) |
		(gnt_cfg->band1.gnt_wl_sw_en ?
		 (B_AX_GNT_WL_RFC_S1_SW_CTRL |
		  B_AX_GNT_WL_BB_S1_SW_CTRL) : 0);
	ret = mac_write_lte(adapter, R_AX_LTE_SW_CFG_1, val);
	if (ret) {
		PLTFM_MSG_ERR("Write LTE fail!\n");
		return ret;
	}

	ret = mac_read_lte(adapter, R_AX_LTE_SW_CFG_2, &val);
	if (ret) {
		PLTFM_MSG_ERR("Read LTE fail!\n");
		return ret;
	}
	val = val & B_AX_WL_RX_CTRL ? B_AX_WL_RX_CTRL : 0 |
		((gnt_cfg->band0.gnt_bt_sw_en || gnt_cfg->band1.gnt_bt_sw_en) ?
		 (B_AX_GNT_BT_TX_SW_CTRL | B_AX_GNT_BT_RX_SW_CTRL) : 0) |
		((gnt_cfg->band0.gnt_bt ||  gnt_cfg->band1.gnt_bt) ?
		 (B_AX_GNT_BT_TX_SW_VAL | B_AX_GNT_BT_RX_SW_VAL) : 0) |
		((gnt_cfg->band0.gnt_wl_sw_en || gnt_cfg->band1.gnt_wl_sw_en) ?
		 (B_AX_GNT_WL_TX_SW_CTRL | B_AX_GNT_WL_RX_SW_CTRL) : 0) |
		((gnt_cfg->band0.gnt_wl ||  gnt_cfg->band1.gnt_wl) ?
		 (B_AX_GNT_WL_TX_SW_VAL | B_AX_GNT_WL_RX_SW_VAL) : 0);

	ret = mac_write_lte(adapter, R_AX_LTE_SW_CFG_2, val);
	if (ret) {
		PLTFM_MSG_ERR("Write LTE fail!\n");
		return ret;
	}

	return MACSUCCESS;
}

u32 mac_cfg_plt(struct mac_ax_adapter *adapter, struct mac_ax_plt *plt)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 reg, ret;
	u16 val;

	ret = check_mac_en(adapter, plt->band, MAC_AX_CMAC_SEL);
	if (ret)
		return ret;

	reg = plt->band == 0 ? R_AX_BT_PLT : R_AX_BT_PLT_C1;
	val = (plt->tx & MAC_AX_PLT_LTE_RX ? B_AX_TX_PLT_GNT_LTE_RX : 0) |
		(plt->tx & MAC_AX_PLT_GNT_BT_TX ? B_AX_TX_PLT_GNT_BT_TX : 0) |
		(plt->tx & MAC_AX_PLT_GNT_BT_RX ? B_AX_TX_PLT_GNT_BT_RX : 0) |
		(plt->tx & MAC_AX_PLT_GNT_WL ? B_AX_TX_PLT_GNT_WL : 0) |
		(plt->rx & MAC_AX_PLT_LTE_RX ? B_AX_RX_PLT_GNT_LTE_RX : 0) |
		(plt->rx & MAC_AX_PLT_GNT_BT_TX ? B_AX_RX_PLT_GNT_BT_TX : 0) |
		(plt->rx & MAC_AX_PLT_GNT_BT_RX ? B_AX_RX_PLT_GNT_BT_RX : 0) |
		(plt->rx & MAC_AX_PLT_GNT_WL ? B_AX_RX_PLT_GNT_WL : 0) |
		(plt->rx || plt->tx ? B_AX_PLT_EN : 0);

#if MAC_AX_FW_REG_OFLD
	if (adapter->sm.fwdl == MAC_AX_FWDL_INIT_RDY) {
		ret = MAC_REG_W_OFLD((u16)reg,
				     B_AX_TX_PLT_GNT_LTE_RX |
				     B_AX_TX_PLT_GNT_BT_TX |
				     B_AX_TX_PLT_GNT_BT_RX |
				     B_AX_TX_PLT_GNT_WL |
				     B_AX_RX_PLT_GNT_LTE_RX |
				     B_AX_RX_PLT_GNT_BT_TX |
				     B_AX_RX_PLT_GNT_BT_RX |
				     B_AX_RX_PLT_GNT_WL,
				     val, 1);
		if (ret != MACSUCCESS)
			PLTFM_MSG_ERR("%s: write offload fail %d",
				      __func__, ret);

		return ret;
	}
#endif
	MAC_REG_W16(reg, val);

	return MACSUCCESS;
}

u32 mac_read_coex_reg(struct mac_ax_adapter *adapter,
		      const u32 offset, u32 *val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (offset > 0xFF) {
		PLTFM_MSG_ERR("[ERR]offset exceed coex reg\n");
		return MACBADDR;
	}

	*val = MAC_REG_R32(R_AX_BTC_CFG + offset);

	return MACSUCCESS;
}

u32 mac_write_coex_reg(struct mac_ax_adapter *adapter,
		       const u32 offset, const u32 val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (offset > 0xFF) {
		PLTFM_MSG_ERR("[ERR]offset exceed coex reg\n");
		return MACBADDR;
	}

	MAC_REG_W32(R_AX_BTC_CFG + offset, val);

	return MACSUCCESS;
}

void mac_cfg_sb(struct mac_ax_adapter *adapter, u32 val)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 fw_sb;

	fw_sb = MAC_REG_R32(R_AX_SCOREBOARD);
	fw_sb = GET_FIELD(fw_sb, MAC_AX_SB_FW);
	fw_sb = fw_sb & ~MAC_AX_BTGS1_NOTIFY;
	if (adapter->sm.pwr == MAC_AX_PWR_OFF || _is_in_lps(adapter))
		fw_sb = fw_sb | MAC_AX_NOTIFY_PWR_MAJOR;
	else
		fw_sb = fw_sb | MAC_AX_NOTIFY_TP_MAJOR;
	val = GET_FIELD(val, MAC_AX_SB_DRV);
	val = B_AX_TOGGLE |
		SET_WORD(val, MAC_AX_SB_DRV) |
		SET_WORD(fw_sb, MAC_AX_SB_FW);
	MAC_REG_W32(R_AX_SCOREBOARD, val);
}

u32 mac_cfg_ctrl_path(struct mac_ax_adapter *adapter, u32 wl)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 val = MAC_REG_R8(R_AX_SYS_SDIO_CTRL + 3);

	val = wl ? val | BIT(2) : val & ~BIT(2);
	MAC_REG_W8(R_AX_SYS_SDIO_CTRL + 3, val);

	return MACSUCCESS;
}

u32 mac_get_ctrl_path(struct mac_ax_adapter *adapter, u32 *wl)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u8 val = MAC_REG_R8(R_AX_SYS_SDIO_CTRL + 3);

	*wl = !!(val & BIT(2));

	return MACSUCCESS;
}

u32 mac_get_bt_polt_cnt(struct mac_ax_adapter *adapter,
			struct mac_ax_bt_polt_cnt *cnt)
{
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 offset = cnt->band ? R_AX_BT_PLT_C1 : R_AX_BT_PLT;
	u8 val;

	cnt->cnt = MAC_REG_R16(offset + 2);

	val = MAC_REG_R8(offset + 1);
	MAC_REG_W8(offset + 1, val | BIT(1));

	return MACSUCCESS;
}

u32 mac_write_coex_mask(struct mac_ax_adapter *adapter,
			u32 offset, u32 mask, u32 val)
{
	u32 ret;

	if (offset < R_AX_BTC_CFG || offset > R_AX_LTE_RDATA) {
		PLTFM_MSG_ERR("[ERR]offset exceed coex reg\n");
		return MACBADDR;
	}

	ret = MAC_REG_W_OFLD((u16)offset, mask, val, 1);
	if (ret) {
		PLTFM_MSG_ERR("[ERR]%s fail\n", __func__);
		return ret;
	}

	return MACSUCCESS;
}

