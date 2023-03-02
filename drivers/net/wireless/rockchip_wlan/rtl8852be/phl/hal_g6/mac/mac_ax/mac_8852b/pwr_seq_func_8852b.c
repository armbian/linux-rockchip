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

#include "../pwr.h"
#include "../pwr_seq_func.h"

#if MAC_AX_8852B_SUPPORT

#define SW_LPS_OPTION 0x0001A0B2
#define PWR_K_CHK_OFFSET 0x5E9
#define PWR_K_CHK_VALUE 0xAA

u32 mac_pwr_on_sdio_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	//u8 xtal_si_val;
	u8 pwr_k_chk_value = 0;
	u8 val8;

	/* 0x1086[0] = 0 == 0x1084[16] = 0 */
	val32 = MAC_REG_R32(R_AX_SDIO_BUS_CTRL);
	MAC_REG_W32(R_AX_SDIO_BUS_CTRL, val32 & ~B_AX_HCI_SUS_REQ);

	/* polling 0x1086[1] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_SDIO_BUS_CTRL, B_AX_HCI_RESUME_RDY,
			   B_AX_HCI_RESUME_RDY);
	if (ret)
		return ret;

	/* 0x04[12:11] = 2'b00 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~(B_AX_AFSM_WLSUS_EN |
			B_AX_AFSM_PCIE_SUS_EN));

	/* 0x04[18] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_DIS_WLBT_PDNSUSEN_SOPC);

	/* 0x90[1] = 1 */
	val32 = MAC_REG_R32(R_AX_WLLPS_CTRL);
	MAC_REG_W32(R_AX_WLLPS_CTRL, val32 | B_AX_DIS_WLBT_LPSEN_LOPC);

	/* 0x04[15] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~B_AX_APDM_HPDN);

	/* 0x04[10] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~B_AX_APFM_SWLPS);

	/* polling 0x04[17] = 1*/
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_RDY_SYSPWR,
			   B_AX_RDY_SYSPWR);
	if (ret)
		return ret;

	/* 0x20[23] = 1*/
	val32 = MAC_REG_R32(R_AX_AFE_LDO_CTRL);
	MAC_REG_W32(R_AX_AFE_LDO_CTRL, val32 | B_AX_AON_OFF_PC_EN);

	/* polling 0x20[23] = 1*/
	ret = pwr_poll_u32(adapter, R_AX_AFE_LDO_CTRL, B_AX_AON_OFF_PC_EN,
			   B_AX_AON_OFF_PC_EN);
	if (ret)
		return ret;

	/* 0x400[1:0] = 2'b01 and 0x400[5:4] = 2'b11*/
	val32 = MAC_REG_R32(R_AX_SPS_DIG_OFF_CTRL0);
	val32 = SET_CLR_WORD(val32, 0x1, B_AX_C1_L1);
	val32 = SET_CLR_WORD(val32, 0x3, B_AX_C3_L1);
	MAC_REG_W32(R_AX_SPS_DIG_OFF_CTRL0, val32);

	/* 0x04[16] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_EN_WLON);

	/* 0x04[8] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_APFN_ONMAC);

	/* polling 0x04[8] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_APFN_ONMAC, 0);
	if (ret)
		return ret;

	/* reset platform twice : 0x88[0] = 1->0->1->0 */
	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 & ~B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 & ~B_AX_PLATFORM_EN);

	/* 0x88[0] = 1 */
	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	/* 0x18[6] = 1 */
	/* XTAL_SI 0x90[6] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x40, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 1 */
	/* XTAL_SI 0x90[5] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x20, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[2] = 0 then 0x90[3] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[2] = 1 then 0x90[3] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[0] = 0 then 0x90[1] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[0] = 1 then 0x90[1] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	#if defined(CONFIG_RTL8852B)
		/* XTAL_SI 0x24[6:4] = 3'b000 */
		ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_2, 0, 0x70);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	#endif

	#if defined(CONFIG_RTL8852BP)
		/* XTAL_SI 0x24[6:4] = 3'b001 */
		ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_2, 0x10, 0x70);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	#endif

	/* XTAL_SI 0x26[3:0] = 4'b0000 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_4, 0, 0x0F);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0xCC[2] = 1 */
	val32 = MAC_REG_R32(R_AX_PMC_DBG_CTRL2);
	MAC_REG_W32(R_AX_PMC_DBG_CTRL2, val32 | B_AX_SYSON_DIS_PMCR_AX_WRMSK);

	/* 0x00[8] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 | B_AX_ISO_EB2CORE);

	/* 0x00[15] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 & ~(BIT(15)));

	PLTFM_DELAY_MS(1);

	/* 0x00[14] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 & ~(BIT(14)));

	/* 0xCC[2] = 0 */
	val32 = MAC_REG_R32(R_AX_PMC_DBG_CTRL2);
	MAC_REG_W32(R_AX_PMC_DBG_CTRL2, val32 & ~B_AX_SYSON_DIS_PMCR_AX_WRMSK);

	/* check power calibration is done or not */
	ret = mac_read_efuse_plus(adapter, PWR_K_CHK_OFFSET, 0x1, &pwr_k_chk_value,
				  MAC_AX_EFUSE_BANK_WIFI);
	if (ret)
		PLTFM_MSG_ERR("%s: read efuse fail\n", __func__);

	if (pwr_k_chk_value != PWR_K_CHK_VALUE) {
		/* 0x200[3:0]=0x9, 0x200 [25:22]=0xA */
		val32 = MAC_REG_R32(R_AX_SPS_DIG_ON_CTRL0);
		val32 = SET_CLR_WORD(val32, 0x9, B_AX_VOL_L1);
		val32 = SET_CLR_WORD(val32, 0xA, B_AX_VREFPFM_L);
		MAC_REG_W32(R_AX_SPS_DIG_ON_CTRL0, val32);
	}

	return MACSUCCESS;
}

u32 mac_pwr_on_usb_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	//u8 xtal_si_val;
	u8 pwr_k_chk_value = 0;
	u8 val8;

	/* 0x04[12:11] = 2'b00 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~(B_AX_AFSM_WLSUS_EN |
			B_AX_AFSM_PCIE_SUS_EN));

	/* 0x04[18] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_DIS_WLBT_PDNSUSEN_SOPC);

	/* 0x90[1] = 1 */
	val32 = MAC_REG_R32(R_AX_WLLPS_CTRL);
	MAC_REG_W32(R_AX_WLLPS_CTRL, val32 | B_AX_DIS_WLBT_LPSEN_LOPC);

	/* 0x04[15] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~B_AX_APDM_HPDN);

	/* 0x04[10] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~B_AX_APFM_SWLPS);

	/* polling 0x04[17] = 1*/
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_RDY_SYSPWR,
			   B_AX_RDY_SYSPWR);
	if (ret)
		return ret;

	/* 0x20[23] = 1*/
	val32 = MAC_REG_R32(R_AX_AFE_LDO_CTRL);
	MAC_REG_W32(R_AX_AFE_LDO_CTRL, val32 | B_AX_AON_OFF_PC_EN);

	/* polling 0x20[23] = 1*/
	ret = pwr_poll_u32(adapter, R_AX_AFE_LDO_CTRL, B_AX_AON_OFF_PC_EN,
			   B_AX_AON_OFF_PC_EN);
	if (ret)
		return ret;

	/* 0x400[1:0] = 2'b01 and 0x400[5:4] = 2'b11*/
	val32 = MAC_REG_R32(R_AX_SPS_DIG_OFF_CTRL0);
	val32 = SET_CLR_WORD(val32, 0x1, B_AX_C1_L1);
	val32 = SET_CLR_WORD(val32, 0x3, B_AX_C3_L1);
	MAC_REG_W32(R_AX_SPS_DIG_OFF_CTRL0, val32);

	/* 0x04[16] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_EN_WLON);

	/* 0x04[8] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_APFN_ONMAC);

	/* polling 0x04[8] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_APFN_ONMAC, 0);
	if (ret)
		return ret;

	/* reset platform twice : 0x88[0] = 1->0->1->0 */
	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 & ~B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 & ~B_AX_PLATFORM_EN);

	/* 0x88[0] = 1 */
	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	/* 0x18[6] = 1 */
	/* XTAL_SI 0x90[6] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x40, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 1 */
	/* XTAL_SI 0x90[5] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x20, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[2] = 0 then 0x90[3] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[2] = 1 then 0x90[3] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[0] = 0 then 0x90[1] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[0] = 1 then 0x90[1] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	#if defined(CONFIG_RTL8852B)
		/* XTAL_SI 0x24[6:4] = 3'b000 */
		ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_2, 0, 0x70);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	#endif

	#if defined(CONFIG_RTL8852BP)
		/* XTAL_SI 0x24[6:4] = 3'b001 */
		ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_2, 0x10, 0x70);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	#endif

	/* XTAL_SI 0x26[3:0] = 4'b0000 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_4, 0, 0x0F);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0xCC[2] = 1 */
	val32 = MAC_REG_R32(R_AX_PMC_DBG_CTRL2);
	MAC_REG_W32(R_AX_PMC_DBG_CTRL2, val32 | B_AX_SYSON_DIS_PMCR_AX_WRMSK);

	/* 0x00[8] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 | B_AX_ISO_EB2CORE);

	/* 0x00[15] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 & ~(BIT(15)));

	PLTFM_DELAY_MS(1);

	/* 0x00[14] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 & ~(BIT(14)));

	/* 0xCC[2] = 0 */
	val32 = MAC_REG_R32(R_AX_PMC_DBG_CTRL2);
	MAC_REG_W32(R_AX_PMC_DBG_CTRL2, val32 & ~B_AX_SYSON_DIS_PMCR_AX_WRMSK);

	/* check power calibration is done or not */
	ret = mac_read_efuse_plus(adapter, PWR_K_CHK_OFFSET, 0x1, &pwr_k_chk_value,
				  MAC_AX_EFUSE_BANK_WIFI);
	if (ret)
		PLTFM_MSG_ERR("%s: read efuse fail\n", __func__);

	if (pwr_k_chk_value != PWR_K_CHK_VALUE) {
		/* 0x200[3:0]=0x9, 0x200 [25:22]=0xA */
		val32 = MAC_REG_R32(R_AX_SPS_DIG_ON_CTRL0);
		val32 = SET_CLR_WORD(val32, 0x9, B_AX_VOL_L1);
		val32 = SET_CLR_WORD(val32, 0xA, B_AX_VREFPFM_L);
		MAC_REG_W32(R_AX_SPS_DIG_ON_CTRL0, val32);
	}

	return MACSUCCESS;
}

u32 mac_pwr_on_pcie_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	u16 val16;
	//u8 xtal_si_val;
	u8 pwr_k_chk_value = 0;
	u8 val8;

	/* 0x04[12:11] = 2'b00 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~(B_AX_AFSM_WLSUS_EN |
			B_AX_AFSM_PCIE_SUS_EN));

	/* 0x04[18] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_DIS_WLBT_PDNSUSEN_SOPC);

	/* 0x90[1] = 1 */
	val32 = MAC_REG_R32(R_AX_WLLPS_CTRL);
	MAC_REG_W32(R_AX_WLLPS_CTRL, val32 | B_AX_DIS_WLBT_LPSEN_LOPC);

	/* 0x04[15] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~B_AX_APDM_HPDN);

	/* 0x04[10] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~B_AX_APFM_SWLPS);

	/* polling 0x04[17] = 1*/
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_RDY_SYSPWR,
			   B_AX_RDY_SYSPWR);
	if (ret)
		return ret;

	/* 0x20[23] = 1*/
	val32 = MAC_REG_R32(R_AX_AFE_LDO_CTRL);
	MAC_REG_W32(R_AX_AFE_LDO_CTRL, val32 | B_AX_AON_OFF_PC_EN);

	/* polling 0x20[23] = 1*/
	ret = pwr_poll_u32(adapter, R_AX_AFE_LDO_CTRL, B_AX_AON_OFF_PC_EN,
			   B_AX_AON_OFF_PC_EN);
	if (ret)
		return ret;

	/* 0x400[1:0] = 2'b01 and 0x400[5:4] = 2'b11*/
	val32 = MAC_REG_R32(R_AX_SPS_DIG_OFF_CTRL0);
	val32 = SET_CLR_WORD(val32, 0x1, B_AX_C1_L1);
	val32 = SET_CLR_WORD(val32, 0x3, B_AX_C3_L1);
	MAC_REG_W32(R_AX_SPS_DIG_OFF_CTRL0, val32);

	/* 0x04[16] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_EN_WLON);

	/* 0x04[8] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_APFN_ONMAC);

	/* polling 0x04[8] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_APFN_ONMAC, 0);
	if (ret)
		return ret;

	/* reset platform twice : 0x88[0] = 1->0->1->0 */
	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 & ~B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 & ~B_AX_PLATFORM_EN);

	/* 0x88[0] = 1 */
	val8 = MAC_REG_R8(R_AX_PLATFORM_ENABLE);
	MAC_REG_W8(R_AX_PLATFORM_ENABLE, val8 | B_AX_PLATFORM_EN);

	/* 0x70[12] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_SDIO_CTRL);
	MAC_REG_W32(R_AX_SYS_SDIO_CTRL, val32 & ~B_AX_PCIE_CALIB_EN_V1);

	/* 0x18[6] = 1 */
	/* XTAL_SI 0x90[6] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x40, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 1 */
	/* XTAL_SI 0x90[5] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x20, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[2] = 0 then 0x90[3] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[2] = 1 then 0x90[3] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[0] = 0 then 0x90[1] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[0] = 1 then 0x90[1] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	#if defined(CONFIG_RTL8852B)
		/* XTAL_SI 0x24[6:4] = 3'b000 */
		ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_2, 0, 0x70);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	#endif

	#if defined(CONFIG_RTL8852BP)
		/* XTAL_SI 0x24[6:4] = 3'b001 */
		ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_2, 0x10, 0x70);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	#endif

	/* XTAL_SI 0x26[3:0] = 4'b0000 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_XTAL_XMD_4, 0, 0x0F);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0xCC[2] = 1 */
	val32 = MAC_REG_R32(R_AX_PMC_DBG_CTRL2);
	MAC_REG_W32(R_AX_PMC_DBG_CTRL2, val32 | B_AX_SYSON_DIS_PMCR_AX_WRMSK);

	/* 0x00[8] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 | B_AX_ISO_EB2CORE);

	/* 0x00[15] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 & ~(BIT(15)));

	PLTFM_DELAY_MS(1);

	/* 0x00[14] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ISO_CTRL);
	MAC_REG_W32(R_AX_SYS_ISO_CTRL, val32 & ~(BIT(14)));

	/* 0xCC[2] = 0 */
	val32 = MAC_REG_R32(R_AX_PMC_DBG_CTRL2);
	MAC_REG_W32(R_AX_PMC_DBG_CTRL2, val32 & ~B_AX_SYSON_DIS_PMCR_AX_WRMSK);

	/* check power calibration is done or not */
	ret = mac_read_efuse_plus(adapter, PWR_K_CHK_OFFSET, 0x1, &pwr_k_chk_value,
				  MAC_AX_EFUSE_BANK_WIFI);
	if (ret)
		PLTFM_MSG_ERR("%s: read efuse fail\n", __func__);

	if (pwr_k_chk_value != PWR_K_CHK_VALUE) {
		/* 0x200[3:0]=0x9, 0x200 [25:22]=0xA */
		val32 = MAC_REG_R32(R_AX_SPS_DIG_ON_CTRL0);
		val32 = SET_CLR_WORD(val32, 0x9, B_AX_VOL_L1);
		val32 = SET_CLR_WORD(val32, 0xA, B_AX_VREFPFM_L);
		MAC_REG_W32(R_AX_SPS_DIG_ON_CTRL0, val32);

		if (is_cv(adapter, CBV)) {
			/* 0x7A[3:0] = 0xA and 0xCC[2]=1 */
			val32 = MAC_REG_R32(R_AX_PMC_DBG_CTRL2);
			MAC_REG_W32(R_AX_PMC_DBG_CTRL2, val32 |
					B_AX_SYSON_DIS_PMCR_AX_WRMSK);

			val16 = MAC_REG_R16(R_AX_HCI_LDO_CTRL);
			val16 = SET_CLR_WORD(val16, 0xA, B_AX_R_AX_VADJ);
			MAC_REG_W16(R_AX_HCI_LDO_CTRL, val16);

			val32 = MAC_REG_R32(R_AX_PMC_DBG_CTRL2);
			MAC_REG_W32(R_AX_PMC_DBG_CTRL2, val32 &
					~B_AX_SYSON_DIS_PMCR_AX_WRMSK);
		}
	}

	return MACSUCCESS;
}

u32 mac_pwr_off_sdio_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u8 val8;
	u32 ret;
	//u8 xtal_si_val;

	/* XTAL_SI 0x90[4] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x10, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[3] = 1 then 0x90[2] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[3] = 0 then 0x90[2] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 0 and 0x81[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x80, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[1] = 1 then 0x90[0] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[1] = 0 then 0x90[0] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x04[16] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_EN_WLON);

	/* 0x02[1:0] = 0 */
	val8 = MAC_REG_R8(R_AX_SYS_FUNC_EN);
	MAC_REG_W8(R_AX_SYS_FUNC_EN, val8 & ~B_AX_FEN_BB_GLB_RSTN & ~B_AX_FEN_BBRSTB);

	/* 0x18[5] = 0 */
	/* XTAL_SI 0x90[5] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[6] = 0 */
	/* XTAL_SI 0x90[6] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x04[9] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_APFM_OFFMAC);

	/* polling 0x04[9] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_APFM_OFFMAC, 0);
	if (ret)
		return ret;

	/* 0x04[28][30] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~B_AX_SOP_EDSWR &
			~B_AX_SOP_EASWR);

	/* 0x10[10]=1,  0x200[18:17] = 2'b11 */
	val32 = MAC_REG_R32(R_AX_SYS_SWR_CTRL1);
	MAC_REG_W32(R_AX_SYS_SWR_CTRL1, val32 | B_AX_SYM_CTRL_SPS_PWMFREQ);

	val32 = MAC_REG_R32(R_AX_SPS_DIG_ON_CTRL0);
	val32 = SET_CLR_WORD(val32, 0x3, B_AX_REG_ZCDC_H);
	MAC_REG_W32(R_AX_SPS_DIG_ON_CTRL0, val32);

	/* 0x04[12:11] = 2'b01 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, (val32 | B_AX_AFSM_WLSUS_EN) &
			~B_AX_AFSM_PCIE_SUS_EN);

	/* 0x1086[0] = 1 */
	val32 = MAC_REG_R32(R_AX_SDIO_BUS_CTRL);
	MAC_REG_W32(R_AX_SDIO_BUS_CTRL, val32 | B_AX_HCI_SUS_REQ);

	#if 0
	/* polling 0x1086[1] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_SDIO_BUS_CTRL, B_AX_HCI_RESUME_RDY, 0);
	if (ret)
		return ret;
	#endif

	return MACSUCCESS;
}

u32 mac_pwr_off_usb_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u8 val8;
	u32 ret;
	//u8 xtal_si_val;

	/* XTAL_SI 0x90[4] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x10, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[3] = 1 then 0x90[2] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[3] = 0 then 0x90[2] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 0 and 0x81[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x80, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[1] = 1 then 0x90[0] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[1] = 0 then 0x90[0] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x04[16] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_EN_WLON);

	/* 0x02[1:0] = 0 */
	val8 = MAC_REG_R8(R_AX_SYS_FUNC_EN);
	MAC_REG_W8(R_AX_SYS_FUNC_EN, val8 & ~B_AX_FEN_BB_GLB_RSTN & ~B_AX_FEN_BBRSTB);

	/* 0x18[5] = 0 */
	/* XTAL_SI 0x90[5] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[6] = 0 */
	/* XTAL_SI 0x90[6] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x04[9] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_APFM_OFFMAC);

	/* polling 0x04[9] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_APFM_OFFMAC, 0);
	if (ret)
		return ret;

	/* 0x04[28] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 & ~B_AX_SOP_EDSWR);

	/* 0x10[10]=1,  0x200[18:17] = 2'b11 */
	val32 = MAC_REG_R32(R_AX_SYS_SWR_CTRL1);
	MAC_REG_W32(R_AX_SYS_SWR_CTRL1, val32 | B_AX_SYM_CTRL_SPS_PWMFREQ);

	val32 = MAC_REG_R32(R_AX_SPS_DIG_ON_CTRL0);
	val32 = SET_CLR_WORD(val32, 0x3, B_AX_REG_ZCDC_H);
	MAC_REG_W32(R_AX_SPS_DIG_ON_CTRL0, val32);

	/* 0x04[12:11] = 2'b01 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, (val32 | B_AX_AFSM_WLSUS_EN) &
			~B_AX_AFSM_PCIE_SUS_EN);

	return MACSUCCESS;
}

u32 mac_pwr_off_pcie_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u8 val8;
	u32 ret;
	//u8 xtal_si_val;

	/* XTAL_SI 0x90[4] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x10, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[3] = 1 then 0x90[2] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[3] = 0 then 0x90[2] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 0 and 0x81[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x80, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[1] = 1 then 0x90[0] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[1] = 0 then 0x90[0] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x04[16] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_EN_WLON);

	/* 0x02[1:0] = 0 */
	val8 = MAC_REG_R8(R_AX_SYS_FUNC_EN);
	MAC_REG_W8(R_AX_SYS_FUNC_EN, val8 & ~B_AX_FEN_BB_GLB_RSTN & ~B_AX_FEN_BBRSTB);

	/* 0x18[5] = 0 */
	/* XTAL_SI 0x90[5] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[6] = 0 */
	/* XTAL_SI 0x90[6] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x04[9] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_APFM_OFFMAC);

	/* polling 0x04[9] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_SYS_PW_CTRL, B_AX_APFM_OFFMAC, 0);
	if (ret)
		return ret;

	/* 0x91[0] = 0 == 0x90[8]=0 */
	//val32 = MAC_REG_R32(R_AX_WLLPS_CTRL);
	//MAC_REG_W32(R_AX_WLLPS_CTRL, val32 & ~B_AX_LPSOP_DSWR);

	/* 0x90[31:0] = 0x00_01_A0_B2 */
	MAC_REG_W32(R_AX_WLLPS_CTRL, SW_LPS_OPTION);

	/* 0x10[10]=1,  0x200[18:17] = 2'b11 */
	val32 = MAC_REG_R32(R_AX_SYS_SWR_CTRL1);
	MAC_REG_W32(R_AX_SYS_SWR_CTRL1, val32 | B_AX_SYM_CTRL_SPS_PWMFREQ);

	val32 = MAC_REG_R32(R_AX_SPS_DIG_ON_CTRL0);
	val32 = SET_CLR_WORD(val32, 0x3, B_AX_REG_ZCDC_H);
	MAC_REG_W32(R_AX_SPS_DIG_ON_CTRL0, val32);

	/* 0x04[10] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_PW_CTRL);
	MAC_REG_W32(R_AX_SYS_PW_CTRL, val32 | B_AX_APFM_SWLPS);

	return MACSUCCESS;
}

#if MAC_AX_FEATURE_HV
u32 mac_enter_lps_sdio_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	//u8 xtal_si_val;

	/* 0x280[6] = 1  */
	val32 = MAC_REG_R32(R_AX_XTAL_ON_CTRL0);
	MAC_REG_W32(R_AX_XTAL_ON_CTRL0, val32 | B_AX_EN_XBUF_DRV_LPS);

	/* 0x68[6] = 0	*/
	val32 = MAC_REG_R32(R_AX_WL_BT_PWR_CTRL);
	MAC_REG_W32(R_AX_WL_BT_PWR_CTRL, val32 & ~B_AX_WLAN_32K_SEL);

	/* 0x8394[2] = 1 */
	val32 = MAC_REG_R32(R_AX_OSC_32K_CTRL);
	MAC_REG_W32(R_AX_OSC_32K_CTRL, val32 | B_AX_CAL32K_XTAL_EN);

	/* 0x8398[31:0] = 0x00_40_04_C4 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x004004C4);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x804004C4);

	/* 0x8398[31:0] = 0x00_41_B4_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x0041B400);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x8041B400);

	/* 0x8398[31:0] = 0x00_42_00_04 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00420004);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80420004);

	/* 0x8398[31:0] = 0x00_43_00_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00430000);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80430000);

	/* 0x8398[31:0] = 0x00_44_7D_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00447D00);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80447D00);

	/* 0x8398[31:0] = 0x00_45_00_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00450000);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80450000);

	/* 0x8398[31:0] = 0x00_48_00_01 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00480001);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80480001);

	/* polling 0x8398[2] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_32K_CAL_REG0, BIT(2), BIT(2));
	if (ret)
		return ret;

	/* 0x8398[1] = 1 */
	val32 = MAC_REG_R32(R_AX_32K_CAL_REG0);
	MAC_REG_W32(R_AX_32K_CAL_REG0, val32 | BIT(1));

	/* 0x8399[7:0] = 0x00 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 1, 0x00);

	/* 0x839A[7:0] = 0x48 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 2, 0x48);

	/* 0x839B[7:0] = 0x80 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 3, 0x80);

	/* 0x8300[0] = 1 */
	val32 = MAC_REG_R32(R_AX_FWD1IMR);
	MAC_REG_W32(R_AX_FWD1IMR, val32 | B_AX_FS_RPWM_INT_EN);

	/* 0x8322[0] = 1 == 0x8320[16] =1 */
	val32 = MAC_REG_R32(R_AX_TSF_32K_SEL);
	MAC_REG_W32(R_AX_TSF_32K_SEL, val32 | B_AX_CKSL_WLTSF);

	/* polling 0x8322[1] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_TSF_32K_SEL, B_AX_TSF_CLK_STABLE,
			   B_AX_TSF_CLK_STABLE);
	if (ret)
		return ret;

	/* DAV CAV */
	/* XTAL_SI 0x90[0] = 0 then 0x90[1] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[0] = 1 then 0x90[1] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0xA1[1] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_SRAM_CTRL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x10, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[3] = 1 then 0x90[2] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[3] = 0 then 0x90[2] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x81[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 0 */
	/* XTAL_SI 0x90[5] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[6] = 0 */
	/* XTAL_SI 0x90[6] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x90[31:0] = 0x04_80_A1_B0 */
	MAC_REG_W32(R_AX_WLLPS_CTRL, 0x0480A1B0);

	/* 0x90[0] = 1 */
	val32 = MAC_REG_R32(R_AX_WLLPS_CTRL);
	MAC_REG_W32(R_AX_WLLPS_CTRL, val32 | B_AX_WL_LPS_EN);

	/* polling 0x90[0] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_WLLPS_CTRL, B_AX_WL_LPS_EN, 0);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 mac_enter_lps_usb_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	//u8 xtal_si_val;

	/* 0x280[6] = 1  */
	val32 = MAC_REG_R32(R_AX_XTAL_ON_CTRL0);
	MAC_REG_W32(R_AX_XTAL_ON_CTRL0, val32 | B_AX_EN_XBUF_DRV_LPS);

	/* 0x68[6] = 0	*/
	val32 = MAC_REG_R32(R_AX_WL_BT_PWR_CTRL);
	MAC_REG_W32(R_AX_WL_BT_PWR_CTRL, val32 & ~B_AX_WLAN_32K_SEL);

	/* 0x8394[2] = 1 */
	val32 = MAC_REG_R32(R_AX_OSC_32K_CTRL);
	MAC_REG_W32(R_AX_OSC_32K_CTRL, val32 | B_AX_CAL32K_XTAL_EN);

	/* 0x8398[31:0] = 0x00_40_04_C4 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x004004C4);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x804004C4);

	/* 0x8398[31:0] = 0x00_41_B4_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x0041B400);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x8041B400);

	/* 0x8398[31:0] = 0x00_42_00_04 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00420004);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80420004);

	/* 0x8398[31:0] = 0x00_43_00_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00430000);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80430000);

	/* 0x8398[31:0] = 0x00_44_7D_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00447D00);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80447D00);

	/* 0x8398[31:0] = 0x00_45_00_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00450000);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80450000);

	/* 0x8398[31:0] = 0x00_48_00_01 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00480001);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80480001);

	/* polling 0x8398[2] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_32K_CAL_REG0, BIT(2), BIT(2));
	if (ret)
		return ret;

	/* 0x8398[1] = 1 */
	val32 = MAC_REG_R32(R_AX_32K_CAL_REG0);
	MAC_REG_W32(R_AX_32K_CAL_REG0, val32 | BIT(1));

	/* 0x8399[7:0] = 0x00 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 1, 0x00);

	/* 0x839A[7:0] = 0x48 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 2, 0x48);

	/* 0x839B[7:0] = 0x80 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 3, 0x80);

	/* 0x8300[0] = 1 */
	val32 = MAC_REG_R32(R_AX_FWD1IMR);
	MAC_REG_W32(R_AX_FWD1IMR, val32 | B_AX_FS_RPWM_INT_EN);

	/* 0x8322[0] = 1 == 0x8320[16] =1 */
	val32 = MAC_REG_R32(R_AX_TSF_32K_SEL);
	MAC_REG_W32(R_AX_TSF_32K_SEL, val32 | B_AX_CKSL_WLTSF);

	/* polling 0x8322[1] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_TSF_32K_SEL, B_AX_TSF_CLK_STABLE,
			   B_AX_TSF_CLK_STABLE);
	if (ret)
		return ret;

	/* DAV CAV */
	/* XTAL_SI 0x90[0] = 0 then 0x90[1] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[0] = 1 then 0x90[1] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0xA1[1] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_SRAM_CTRL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x10, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[3] = 1 then 0x90[2] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[3] = 0 then 0x90[2] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x81[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 0 */
	/* XTAL_SI 0x90[5] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[6] = 0 */
	/* XTAL_SI 0x90[6] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x90[31:0] = 0x04_81_A1_B0 */
	MAC_REG_W32(R_AX_WLLPS_CTRL, 0x0481A1B0);

	/* 0x90[0] = 1 */
	val32 = MAC_REG_R32(R_AX_WLLPS_CTRL);
	MAC_REG_W32(R_AX_WLLPS_CTRL, val32 | B_AX_WL_LPS_EN);

	/* polling 0x90[0] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_WLLPS_CTRL, B_AX_WL_LPS_EN, 0);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 mac_enter_lps_pcie_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	//u8 xtal_si_val;

	/* 0x280[6] = 1  */
	val32 = MAC_REG_R32(R_AX_XTAL_ON_CTRL0);
	MAC_REG_W32(R_AX_XTAL_ON_CTRL0, val32 | B_AX_EN_XBUF_DRV_LPS);

	/* 0x68[6] = 0  */
	val32 = MAC_REG_R32(R_AX_WL_BT_PWR_CTRL);
	MAC_REG_W32(R_AX_WL_BT_PWR_CTRL, val32 & ~B_AX_WLAN_32K_SEL);

	/* 0x8394[2] = 1 */
	val32 = MAC_REG_R32(R_AX_OSC_32K_CTRL);
	MAC_REG_W32(R_AX_OSC_32K_CTRL, val32 | B_AX_CAL32K_XTAL_EN);

	/* 0x8398[31:0] = 0x00_40_04_C4 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x004004C4);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x804004C4);

	/* 0x8398[31:0] = 0x00_41_B4_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x0041B400);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x8041B400);

	/* 0x8398[31:0] = 0x00_42_00_04 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00420004);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80420004);

	/* 0x8398[31:0] = 0x00_43_00_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00430000);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80430000);

	/* 0x8398[31:0] = 0x00_44_7D_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00447D00);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80447D00);

	/* 0x8398[31:0] = 0x00_45_00_00 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00450000);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80450000);

	/* 0x8398[31:0] = 0x00_48_00_01 then 0x8398[31] = 1 */
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x00480001);
	MAC_REG_W32(R_AX_32K_CAL_REG0, 0x80480001);

	/* polling 0x8398[2] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_32K_CAL_REG0, BIT(2), BIT(2));
	if (ret)
		return ret;

	/* 0x8398[1] = 1 */
	val32 = MAC_REG_R32(R_AX_32K_CAL_REG0);
	MAC_REG_W32(R_AX_32K_CAL_REG0, val32 | BIT(1));

	/* 0x8399[7:0] = 0x00 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 1, 0x00);

	/* 0x839A[7:0] = 0x48 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 2, 0x48);

	/* 0x839B[7:0] = 0x80 */
	MAC_REG_W8(R_AX_32K_CAL_REG0 + 3, 0x80);

	/* 0x8300[0] = 1 */
	val32 = MAC_REG_R32(R_AX_FWD1IMR);
	MAC_REG_W32(R_AX_FWD1IMR, val32 | B_AX_FS_RPWM_INT_EN);

	/* 0x8322[0] = 1 == 0x8320[16] =1 */
	val32 = MAC_REG_R32(R_AX_TSF_32K_SEL);
	MAC_REG_W32(R_AX_TSF_32K_SEL, val32 | B_AX_CKSL_WLTSF);

	/* polling 0x8322[1] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_TSF_32K_SEL, B_AX_TSF_CLK_STABLE,
			   B_AX_TSF_CLK_STABLE);
	if (ret)
		return ret;

	/* DAV CAV */
	/* XTAL_SI 0x90[0] = 0 then 0x90[1] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[0] = 1 then 0x90[1] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0xA1[1] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_SRAM_CTRL, 0x02, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x10, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[3] = 1 then 0x90[2] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[3] = 0 then 0x90[2] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x81[0] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 0 */
	/* XTAL_SI 0x90[5] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[6] = 0 */
	/* XTAL_SI 0x90[6] = 0 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 & ~B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x90[31:0] = 0x04_81_A1_B0 */
	MAC_REG_W32(R_AX_WLLPS_CTRL, 0x0481A1B0);

	/* 0x90[0] = 1 */
	val32 = MAC_REG_R32(R_AX_WLLPS_CTRL);
	MAC_REG_W32(R_AX_WLLPS_CTRL, val32 | B_AX_WL_LPS_EN);

	/* polling 0x90[0] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_WLLPS_CTRL, B_AX_WL_LPS_EN, 0);
	if (ret)
		return ret;

	return MACSUCCESS;
}

u32 mac_leave_lps_sdio_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	//u8 xtal_si_val;
	u32 cnt = 320;

	/* 0x1083[7] = 1 == 0x1080[31] = 1 */
	val32 = MAC_REG_R32(R_AX_SDIO_HRPWM1);
	MAC_REG_W32(R_AX_SDIO_HRPWM1, val32 | BIT(31));

	/* polling 0x1E5[7] = 1 == 0x1E4[15] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_RPWM,
			   B_AX_RPWM_TOGGLE, B_AX_RPWM_TOGGLE);
	if (ret)
		return ret;

	/* delay 0x10 ms */
	while (--cnt)
		PLTFM_DELAY_US(50);

	/* 0x8322[0] = 0 == 0x8320[16] = 0 */
	val32 = MAC_REG_R32(R_AX_TSF_32K_SEL);
	MAC_REG_W32(R_AX_TSF_32K_SEL, val32 & ~B_AX_CKSL_WLTSF);

	/* polling 0x8322[1] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_TSF_32K_SEL, B_AX_TSF_CLK_STABLE, 0);
	if (ret)
		return ret;

	/* 0x1083[7] = 0 == 0x1080[31] = 0 */
	val32 = MAC_REG_R32(R_AX_SDIO_HRPWM1);
	MAC_REG_W32(R_AX_SDIO_HRPWM1, val32 & ~(BIT(31)));

	/* 0x8304[0] = 1 */
	val32 = MAC_REG_R32(R_AX_FWD1ISR);
	MAC_REG_W32(R_AX_FWD1ISR, val32 | B_AX_FS_RPWM_INT);

	/* 0x18[6] = 1 */
	/* XTAL_SI 0x90[6] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x40, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 1 */
	/* XTAL_SI 0x90[5] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x20, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[2] = 0 then 0x90[3] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[2] = 1 then 0x90[3] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x81[0] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0xA1[1] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_SRAM_CTRL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x80, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[1] = 1 then 0x90[0] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[1] = 0 then 0x90[0] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	return MACSUCCESS;
}

u32 mac_leave_lps_usb_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	//u8 xtal_si_val;
	u32 cnt = 320;

	/* 0x1203[7] = 1 == 0x1200[31] = 1 */
	val32 = MAC_REG_R32(R_AX_USB_D2F_F2D_INFO);
	MAC_REG_W32(R_AX_USB_D2F_F2D_INFO, val32 | BIT(31));

	/* polling 0x1E5[7] = 1 == 0x1E4[15] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_RPWM,
			   B_AX_RPWM_TOGGLE, B_AX_RPWM_TOGGLE);
	if (ret)
		return ret;

	/* delay 0x10 ms */
	while (--cnt)
		PLTFM_DELAY_US(50);

	/* 0x8322[0] = 0 == 0x8320[16] = 0 */
	val32 = MAC_REG_R32(R_AX_TSF_32K_SEL);
	MAC_REG_W32(R_AX_TSF_32K_SEL, val32 & ~B_AX_CKSL_WLTSF);

	/* polling 0x8322[1] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_TSF_32K_SEL, B_AX_TSF_CLK_STABLE, 0);
	if (ret)
		return ret;

	/* 0x1203[7] = 0 == 0x1200[31] = 0 */
	val32 = MAC_REG_R32(R_AX_USB_D2F_F2D_INFO);
	MAC_REG_W32(R_AX_USB_D2F_F2D_INFO, val32 & ~(BIT(31)));

	/* 0x8304[0] = 1 */
	val32 = MAC_REG_R32(R_AX_FWD1ISR);
	MAC_REG_W32(R_AX_FWD1ISR, val32 | B_AX_FS_RPWM_INT);

	/* 0x18[6] = 1 */
	/* XTAL_SI 0x90[6] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x40, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 1 */
	/* XTAL_SI 0x90[5] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x20, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[2] = 0 then 0x90[3] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[2] = 1 then 0x90[3] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x81[0] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0xA1[1] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_SRAM_CTRL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x80, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[1] = 1 then 0x90[0] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[1] = 0 then 0x90[0] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	return MACSUCCESS;
}

u32 mac_leave_lps_pcie_8852b(void *vadapter)
{
	struct mac_ax_adapter *adapter = (struct mac_ax_adapter *)vadapter;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	u32 val32;
	u32 ret;
	//u8 xtal_si_val;
	u32 cnt = 320;

	/* 0x10C1[7] = 1 == 0x10C0[15] = 1 */
	val32 = MAC_REG_R32(R_AX_PCIE_HRPWM);
	MAC_REG_W32(R_AX_PCIE_HRPWM, val32 | BIT(15));

	/* polling 0x1E5[7] = 1 == 0x1E4[15] = 1 */
	ret = pwr_poll_u32(adapter, R_AX_RPWM,
			   B_AX_RPWM_TOGGLE, B_AX_RPWM_TOGGLE);
	if (ret)
		return ret;

	/* delay 0x10 ms */
	while (--cnt)
		PLTFM_DELAY_US(50);

	/* 0x8322[0] = 0 == 0x8320[16] = 0 */
	val32 = MAC_REG_R32(R_AX_TSF_32K_SEL);
	MAC_REG_W32(R_AX_TSF_32K_SEL, val32 & ~B_AX_CKSL_WLTSF);

	/* polling 0x8322[1] = 0 */
	ret = pwr_poll_u32(adapter, R_AX_TSF_32K_SEL, B_AX_TSF_CLK_STABLE, 0);
	if (ret)
		return ret;

	/* 0x10C1[7] = 0 */
	val32 = MAC_REG_R32(R_AX_PCIE_HRPWM);
	MAC_REG_W32(R_AX_PCIE_HRPWM, val32 & ~(BIT(15)));

	/* 0x8304[0] = 1 */
	val32 = MAC_REG_R32(R_AX_FWD1ISR);
	MAC_REG_W32(R_AX_FWD1ISR, val32 | B_AX_FS_RPWM_INT);

	/* 0x18[6] = 1 */
	/* XTAL_SI 0x90[6] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_PTA_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x40, 0x40);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* 0x18[5] = 1 */
	/* XTAL_SI 0x90[5] = 1 */
	val32 = MAC_REG_R32(R_AX_SYS_ADIE_PAD_PWR_CTRL);
	MAC_REG_W32(R_AX_SYS_ADIE_PAD_PWR_CTRL, val32 | B_AX_SYM_PADPDN_WL_RFC_1P3);

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x20, 0x20);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[2] = 0 then 0x90[3] = 0 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[2] = 1 then 0x90[3] = 1 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x04, 0x04);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x08, 0x08);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[4] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x10);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x80[0] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S0, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x81[0] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_WL_RFC_S1, 0x01, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0xA1[1] = 0 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_SRAM_CTRL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* XTAL_SI 0x90[7] = 1 */
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x80, 0x80);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	/* DAV CAV */
	/* XTAL_SI 0x90[1] = 1 then 0x90[0] = 1 */
	/* DAV after CBV */
	/* XTAL_SI 0x90[1] = 0 then 0x90[0] = 0 */
	#if 0
	ret = mac_read_xtal_si(adapter, XTAL_SI_CV, &xtal_si_val);
	if (ret) {
		PLTFM_MSG_ERR("Read XTAL_SI fail!\n");
		return ret;
	}

	xtal_si_val = (xtal_si_val & 0x0F);

	if (xtal_si_val == CAV) {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x02, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0x01, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	} else {
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
		ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
		if (ret) {
			PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
			return ret;
		}
	}
	#endif

	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x02);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}
	ret = mac_write_xtal_si(adapter, XTAL_SI_ANAPAR_WL, 0, 0x01);
	if (ret) {
		PLTFM_MSG_ERR("Write XTAL_SI fail!\n");
		return ret;
	}

	return MACSUCCESS;
}

#endif

#endif /* #if MAC_AX_8852B_SUPPORT */
