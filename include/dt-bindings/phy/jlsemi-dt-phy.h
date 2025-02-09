/* SPDX-License-Identifier: GPL-2.0
 *
 * Device Tree constants for JLSemi PHY
 *
 * Author: Gangqiao Kuang
 *
 * Copyright (c) 2021 JLSemi Corporation
 */

#ifndef _DT_BINDINGS_JLSEMI_PHY_H
#define _DT_BINDINGS_JLSEMI_PHY_H

/**************************** Linux Version Compatible ********************/
#define JLSEMI_DEV_COMPATIBLE	(KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE)
#define JL2XXX_GET_STRING	(KERNEL_VERSION(4, 5, 0) <= LINUX_VERSION_CODE)
#define JL2XXX_GET_STAT		(KERNEL_VERSION(4, 5, 0) <= LINUX_VERSION_CODE)
#define JL2XXX_PHY_TUNABLE	(KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE)
#define JLSEMI_PHY_WOL		(KERNEL_VERSION(3, 10, 0) < LINUX_VERSION_CODE)
/*************************************************************************/

/**************************** JLSemi Debug *******************************/
#define JLSEMI_DEBUG_INFO	0
/*************************************************************************/

/************************* JLSemi Phy Init Reentrant *********************/
#define JLSEMI_PHY_NOT_REENTRANT	false
/*************************************************************************/

/**************************** JL1XXX-LED *********************************/
/* PHY LED Modes Select */
#define JL1XXX_LED0_STRAP		(1 << 0)
#define JL1XXX_LED0_EEE			(1 << 1)
#define JL1XXX_LED0_100_ACTIVITY	(1 << 2)
#define JL1XXX_LED0_10_ACTIVITY		(1 << 3)
#define JL1XXX_LED0_100_LINK		(1 << 4)
#define JL1XXX_LED0_10_LINK		(1 << 5)
#define JL1XXX_LED1_STRAP		(1 << 8)
#define JL1XXX_LED1_EEE			(1 << 9)
#define JL1XXX_LED1_100_ACTIVITY	(1 << 10)
#define JL1XXX_LED1_10_ACTIVITY		(1 << 11)
#define JL1XXX_LED1_100_LINK		(1 << 12)
#define JL1XXX_LED1_10_LINK		(1 << 13)

/* PHY LED As Gpio Output Select */
#define JL1XXX_GPIO_LED0_OUT		(1 << 2)
#define JL1XXX_GPIO_LED1_OUT		(1 << 3)
#define JL1XXX_GPIO_LED0_EN		(1 << 14)
#define JL1XXX_GPIO_LED1_EN		(1 << 15)

/* PHY LED Control Enable Mask Select */
#define JL1XXX_LED_STATIC_OP_EN		(1 << 0)
#define JL1XXX_LED_MODE_EN		(1 << 1)
#define JL1XXX_LED_GLOABL_PERIOD_EN	(1 << 2)
#define JL1XXX_LED_GLOBAL_ON_EN		(1 << 3)
#define JL1XXX_LED_GPIO_OUT_EN		(1 << 4)
//-----------------------------------------------------------------------//
/* PHY LED Control Enable Mask Config */
#define JL1XXX_LED_CTRL_EN	(0)

/* PHY LED Modes Config */
#define JL1XXX_CFG_LED_MODE	(JL1XXX_LED0_100_LINK | \
				 JL1XXX_LED0_10_LINK | \
				 JL1XXX_LED1_100_ACTIVITY | \
				 JL1XXX_LED1_10_ACTIVITY)

/* PHY LED As Gpio Output Config */
#define JL1XXX_CFG_GPIO		(JL1XXX_GPIO_LED0_EN | \
				 JL1XXX_GPIO_LED0_OUT | \
				 JL1XXX_GPIO_LED1_EN | \
				 JL1XXX_GPIO_LED1_OUT)

/* PHY LED Global Period Config */
#define JL1XXX_GLOBAL_PERIOD_MS		0x10

/* PHY LED Global Hold On Config */
#define JL1XXX_GLOBAL_ON_MS		0x8
/**************************************************************************/

/****************************** JL1XXX-WOL ********************************/
/* PHY WOL Control Enable Mask Select */
#define JL1XXX_WOL_STATIC_OP_EN		(1 << 0)
//-----------------------------------------------------------------------//
/* PHY WOL Control Enable Mask Config */
#define JL1XXX_WOL_CTRL_EN	(0)

/*************************************************************************/

/***************************** JL1XXX-INTR *******************************/
/* PHY Interrupt Control Enable Mask Select */
#define JL1XXX_INTR_STATIC_OP_EN	(1 << 0)
#define JL1XXX_INTR_LINK_CHANGE_EN	(1 << 1)
#define JL1XXX_INTR_AN_ERR_EN		(1 << 2)
//-----------------------------------------------------------------------//
/* PHY Interrupt Irq Number Config */
#define JL1XXX_INTR_IRQ			-1

/* PHY Interrupt Control Enable Mask Config */
#define JL1XXX_INTR_CTRL_EN	(0)
/*************************************************************************/

/**************************** JL1XXX-MDI *********************************/
/* PHY MDI Control Mode Enable Mask Select */
#define JL1XXX_MDI_STATIC_OP_EN		(1 << 0)
#define JL1XXX_MDI_RATE_EN		(1 << 1)
#define JL1XXX_MDI_AMPLITUDE_EN		(1 << 2)

/* PHY MDI Rate Select */
#define JL1XXX_MDI_RATE_STANDARD	0
#define JL1XXX_MDI_RATE_ACCELERATE	1

/* PHY MDI Amplitude Select */
#define JL1XXX_MDI_AMPLITUDE0		0
#define JL1XXX_MDI_AMPLITUDE1		1
#define JL1XXX_MDI_AMPLITUDE2		2
#define JL1XXX_MDI_AMPLITUDE3		3
#define JL1XXX_MDI_AMPLITUDE4		4
#define JL1XXX_MDI_AMPLITUDE5		5
#define JL1XXX_MDI_AMPLITUDE6		6
#define JL1XXX_MDI_AMPLITUDE7		7
//-----------------------------------------------------------------------//
/* PHY MDI Control Mode Enable Mask Config */
#define JL1XXX_MDI_CTRL_EN		(0)

/* PHY MDI Rate Config */
#define JL1XXX_MDI_RATE			JL1XXX_MDI_RATE_ACCELERATE

/* PHY MDI Amplitude Config */
#define JL1XXX_MDI_AMPLITUDE		JL1XXX_MDI_AMPLITUDE4

/*************************************************************************/

/**************************** JL1XXX-RMII ********************************/
/* PHY RMII Control Mode Enable Mask Select */
#define JL1XXX_RMII_STATIC_OP_EN	(1 << 0)
#define JL1XXX_RMII_MODE_EN		(1 << 1)
#define JL1XXX_RMII_CLK_50M_INPUT_EN	(1 << 2)
#define JL1XXX_RMII_TX_SKEW_EN		(1 << 3)
#define JL1XXX_RMII_RX_SKEW_EN		(1 << 4)
#define JL1XXX_RMII_CRS_DV_EN		(1 << 5)
//-----------------------------------------------------------------------//
/* PHY RMII Control Mode Enable Mask Config */
#define JL1XXX_RMII_CTRL_EN		(0)

/* PHY RMII Timing Config */
#define JL1XXX_RMII_TX_TIMING		0xf
#define JL1XXX_RMII_RX_TIMING		0xf

/*************************************************************************/

/**************************** JL2XXX-LED *********************************/
/* PHY LED Modes Select*/
#define JL2XXX_LED0_LINK10		(1 << 0)
#define JL2XXX_LED0_LINK100		(1 << 1)
#define JL2XXX_LED0_LINK1000		(1 << 3)
#define JL2XXX_LED0_ACTIVITY		(1 << 4)
#define JL2XXX_LED1_LINK10		(1 << 5)
#define JL2XXX_LED1_LINK100		(1 << 6)
#define JL2XXX_LED1_LINK1000		(1 << 8)
#define JL2XXX_LED1_ACTIVITY		(1 << 9)
#define JL2XXX_LED2_LINK10		(1 << 10)
#define JL2XXX_LED2_LINK100		(1 << 11)
#define JL2XXX_LED2_LINK1000		(1 << 13)
#define JL2XXX_LED2_ACTIVITY		(1 << 14)
/* mode_A = 0 and mode_B = 1 default mode_A */
#define JL2XXX_LED_GLB_MODE_B		(1 << 15)

/* PHY LED Polarity Select */
#define JL2XXX_LED0_POLARITY		(1 << 12)
#define JL2XXX_LED1_POLARITY		(1 << 11)
#define JL2XXX_LED2_POLARITY		(1 << 10)

/* PHY LED Control Enable Mask Select */
#define JL2XXX_LED_STATIC_OP_EN		(1 << 0)
#define JL2XXX_LED_MODE_EN		(1 << 1)
#define JL2XXX_LED_GLOABL_PERIOD_EN	(1 << 2)
#define JL2XXX_LED_GLOBAL_ON_EN		(1 << 3)
#define JL2XXX_LED_POLARITY_EN		(1 << 4)

//-----------------------------------------------------------------------//
/* PHY LED Control Enable Mask Config */
#define JL2XXX_LED_CTRL_EN	(0)

/* PHY LED Modes Config */
#define JL2XXX_CFG_LED_MODE	(JL2XXX_LED0_LINK10 | \
				 JL2XXX_LED0_ACTIVITY | \
				 JL2XXX_LED1_LINK100 | \
				 JL2XXX_LED1_ACTIVITY | \
				 JL2XXX_LED2_LINK1000 | \
				 JL2XXX_LED2_ACTIVITY)

/* PHY LED Polarity Config */
#define JL2XXX_LED_POLARITY	(JL2XXX_LED0_POLARITY | \
				 JL2XXX_LED1_POLARITY | \
				 JL2XXX_LED2_POLARITY)

/* PHY LED Global Period Config */
#define JL2XXX_GLOBAL_PERIOD_MS		0x3

/* PHY LED Global Hold On Config */
#define JL2XXX_GLOBAL_ON_MS		0x2
/*************************************************************************/

/**************************** JL2XXX-FLD *********************************/
/* PHY Fast Link Down Control Enable Mask Select */
#define JL2XXX_FLD_STATIC_OP_EN		(1 << 0)
//-----------------------------------------------------------------------//
/* PHY Fast Link Down Control Enable Mask Config */
#define JL2XXX_FLD_CTRL_EN		(0)

/* PHY Fast Link Down Delay Config */
#define JL2XXX_FLD_DELAY		0
/*************************************************************************/

/**************************** JL2XXX-WOL *********************************/
/* PHY WOL Control Enable Mask Select */
#define JL2XXX_WOL_STATIC_OP_EN		(1 << 0)

//-----------------------------------------------------------------------//
/* PHY WOL Control Enable Mask Config */
#define JL2XXX_WOL_CTRL_EN	(0)
/*************************************************************************/

/**************************** JL2XXX-INTR ********************************/
/* PHY Interrupt Control Enable Mask Select */
#define JL2XXX_INTR_STATIC_OP_EN	(1 << 0)
#define JL2XXX_INTR_LINK_CHANGE_EN	(1 << 1)
#define JL2XXX_INTR_AN_ERR_EN		(1 << 2)
#define JL2XXX_INTR_AN_COMPLETE_EN	(1 << 3)
#define JL2XXX_INTR_AN_PAGE_RECE	(1 << 4)
//-----------------------------------------------------------------------//
/* PHY Interrupt Irq Number Config */
#define JL2XXX_INTR_IRQ			-1

/* PHY Interrupt Control Enable Mask Config */
#define JL2XXX_INTR_CTRL_EN	(0)
/*************************************************************************/

/**************************** JL2XXX-DSFT ********************************/
/* PHY Downshift Control Enable Mask */
#define JL2XXX_DSFT_STATIC_OP_EN	(1 << 0)
//-----------------------------------------------------------------------//
/* PHY Downshift Control Enable Config */
#define JL2XXX_DSFT_CTRL_EN	(0)

/* PHY Downshift Count Config */
#define JL2XXX_DSFT_AN_CNT		4
/*************************************************************************/

/**************************** JL2XXX-RGMII *******************************/
/* PHY RGMII Control Mode Enable Mask Select */
#define JL2XXX_RGMII_STATIC_OP_EN	(1 << 0)
#define JL2XXX_RGMII_TX_DLY_EN		(1 << 1)
#define JL2XXX_RGMII_RX_DLY_EN		(1 << 2)
/* PHY RGMII DELAY BIT */
#define JL2XXX_RGMII_TX_DLY_2NS		(1 << 8)
#define JL2XXX_RGMII_RX_DLY_2NS		(1 << 9)
//-----------------------------------------------------------------------//
/* PHY RGMII Control Mode Enable Mask Config */
#define JL2XXX_RGMII_CTRL_EN	(0)

/*************************************************************************/

/**************************** JL2XXX-PATCH *******************************/
/* PHY Patch Control Mode Enable Mask Select */
#define JL2XXX_PATCH_STATIC_OP_EN	(1 << 0)
//-----------------------------------------------------------------------//
/* PHY Patch Control Mode Enable Mask Config */
#define JL2XXX_PATCH_CTRL_EN	(JL2XXX_PATCH_STATIC_OP_EN)

/*************************************************************************/

/**************************** JL2XXX-CLOCK *******************************/
/* PHY Clock Control Mode Enable Mask Select */
#define JL2XXX_CLK_STATIC_OP_EN		(1 << 0)
#define JL2XXX_25M_CLK_OUT_EN		(1 << 1)
#define JL2XXX_125M_CLK_OUT_EN		(1 << 2)
#define JL2XXX_CLK_OUT_DIS		(1 << 3)
//-----------------------------------------------------------------------//
/* PHY Clock Control Mode Enable Mask Config */
#define JL2XXX_CLK_CTRL_EN	(0)

/*************************************************************************/

/**************************** JL2XXX-WORK_MODE ***************************/
/* PHY Work Mode Control Mode Enable Mask Select */
#define JL2XXX_WORK_MODE_STATIC_OP_EN	(1 << 0)

/* PHY Work Mode Select */
#define JL2XXX_UTP_RGMII_MODE		0
#define JL2XXX_FIBER_RGMII_MODE		1
#define JL2XXX_UTP_FIBER_RGMII_MODE	2
#define JL2XXX_UTP_SGMII_MODE		3
#define JL2XXX_PHY_SGMII_RGMII_MODE	4
#define JL2XXX_MAC_SGMII_RGMII_MODE	5
#define JL2XXX_UTP_FIBER_FORCE_MODE1	6
#define JL2XXX_UTP_FIBER_FORCE_MODE2	7
//-----------------------------------------------------------------------//
/* PHY Work Mode Control Mode Enable Mask Config */
#define JL2XXX_WORK_MODE_CTRL_EN	(0)

/* PHY Work Mode Config */
#define JL2XXX_WORK_MODE_MODE		JL2XXX_UTP_RGMII_MODE

/*************************************************************************/

/**************************** JL2XXX-LOOPBACK ****************************/
/* PHY Loopback Control Mode Enable Mask Select */
#define JL2XXX_LPBK_STATIC_OP_EN	(1 << 0)

/* PHY Loopback Mode Select */
#define JL2XXX_LPBK_PCS_10M		0
#define JL2XXX_LPBK_PCS_100M		1
#define JL2XXX_LPBK_PCS_1000M		2
#define JL2XXX_LPBK_PMD_1000M		3
#define JL2XXX_LPBK_EXT_STUB_1000M	4
//-----------------------------------------------------------------------//
/* PHY Loopback Control Mode Enable Mask Config */
#define JL2XXX_LPBK_CTRL_EN		(0)

/* PHY Loopback Mode Config */
#define JL2XXX_LPBK_MODE		JL2XXX_LPBK_PCS_1000M
/*************************************************************************/

/**************************** JL2XXX-SLEW_RATE ****************************/
/* PHY Slew Rate Control Mode Enable Mask Select */
#define JL2XXX_SLEW_RATE_STATIC_OP_EN	(1 << 0)
//-----------------------------------------------------------------------//
/* PHY Slew Rate Control Mode Enable Mask Config */
#define JL2XXX_SLEW_RATE_CTRL_EN	(0)

/*************************************************************************/

/**************************** JL2XXX-RXC_OUT *****************************/
/* PHY Rx Clock Out Control Mode Enable Mask Select */
#define JL2XXX_RXC_OUT_STATIC_OP_EN	(1 << 0)
//-----------------------------------------------------------------------//
/* PHY Rx Clock Out Control Mode Enable Mask Config */
#define JL2XXX_RXC_OUT_CTRL_EN		(0)

/*************************************************************************/
#endif
