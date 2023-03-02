/** @file */
/******************************************************************************
 *
 * Copyright(c) 2016 - 2019 Realtek Corporation. All rights reserved.
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

#ifndef __HALMAC_PCIE_REG_H__
#define __HALMAC_PCIE_REG_H__

/* PCIE PHY register */
#define RAC_CTRL_PPR			0x00
#define RAC_ANA10			0x10
#define RAC_ANA19			0x19

#define RAC_REG_REV2			0x1B
#define BAC_CMU_EN_DLY_SH		12
#define BAC_CMU_EN_DLY_MSK		0xF

#define RAC_REG_FLD_0			0x1D
#define BAC_AUTOK_N_SH			2
#define BAC_AUTOK_N_MSK			0x3

#define RAC_ANA1F			0x1F
#define RAC_SET_PPR			0x20
#define RAC_TRG_PPR			0x21
#define RAC_ANA24			0x24
#define RAC_ANA26			0x26

#define RAC_CTRL_PPR_V1			0x30
#define BAC_AUTOK_DIV_SH		14
#define BAC_AUTOK_DIV_MSK		0x3
#define BAC_AUTOK_EN			BIT(13)
#define BAC_AUTOK_ONCE_EN		BIT(12)
#define BAC_AUTOK_HW_TAR_SH		0
#define BAC_AUTOK_HW_TAR_MSK		0xFFF

#define RAC_SET_PPR_V1			0x31
#define BAC_AUTOK_MGN_SH		12
#define BAC_AUTOK_MGN_MSK		0xF
#define BAC_AUTOK_TAR_SH		0
#define BAC_AUTOK_TAR_MSK		0xFFF

/* PCIE CFG register */
#define PCIE_L1_STS			0x80
#define PCIE_PHY_RATE			0x82
#define PCIE_L1SS_CTRL			0x718
#define PCIE_L1_CTRL			0x719
#define PCIE_ACK_NFTS			0x70D
#define PCIE_COM_CLK_NFTS		0x70E
#define PCIE_FTS			0x80C
#define PCIE_ASPM_CTRL			0x70F
#define PCIE_CLK_CTRL			0x725
#define CFG_RST_MSTATE			0xB48
#define PCIE_L1SS_CAP			0x160
#define PCIE_L1SS_SUP			0x164
#define PCIE_L1SS_STS			0x168

/* PCIE CFG bit */
#define PCIE_BIT_STS_L0S		BIT(0)
#define PCIE_BIT_STS_L1			BIT(1)
#define PCIE_BIT_WAKE			BIT(2)
#define PCIE_BIT_L1			BIT(3)
#define PCIE_BIT_CLK			BIT(4)
#define PCIE_BIT_L0S			BIT(7)
#define PCIE_BIT_L1SS			BIT(5)
#define PCIE_BIT_L1SSSUP		BIT(4)

/* PCIE ASPM mask*/
#define SHFT_L1DLY			3
#define SHFT_L0SDLY			0
#define PCIE_ASPMDLY_MASK		0x07
#define PCIE_L1SS_MASK			0x0F

/* PCIE Capability */
#define PCIE_L1SS_ID			0x001E

/* PCIE MAC register */
#define LINK_CTRL2_REG_OFFSET		0xA0
#define GEN2_CTRL_OFFSET		0x80C
#define LINK_STATUS_REG_OFFSET		0x82

#define PCIE_GEN1_SPEED			0x01
#define PCIE_GEN2_SPEED			0x02

#endif/* __HALMAC_PCIE_REG_H__ */
