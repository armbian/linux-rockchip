/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#ifndef _IT6621_REG_BANK0_H
#define _IT6621_REG_BANK0_H

/* Bank 0 */

/* General Registers */
#define IT6621_REG_VENDOR_ID_LOW	0x00 /* Vendor ID low */
#define IT6621_REG_VENDOR_ID_HIGH	0x01 /* Vendor ID high */
#define IT6621_REG_DEV_ID_LOW		0x02 /* Device ID low */
#define IT6621_REG_DEV_ID_HIGH		0x03 /* Device ID high */
#define IT6621_REG_REV_ID		0x04 /* Revision ID */

/* System Reset */
#define IT6621_REG_SYS_RESET		0x05
#define IT6621_SW_ACLK_RESET_SEL	BIT(0) /* Software ACLK clock domain reset */
#define IT6621_SW_ACLK_RESET_DIS	(0x00 << 0)
#define IT6621_SW_ACLK_RESET_EN		(0x01 << 0)
#define IT6621_SW_BCLK_RESET_SEL	BIT(1) /* Software BCLK clock domain reset */
#define IT6621_SW_BCLK_RESET_DIS	(0x00 << 1)
#define IT6621_SW_BCLK_RESET_EN		(0x01 << 1)
#define IT6621_SW_TCCLK_RESET_SEL	BIT(2) /* Software TCCLK clock domain reset */
#define IT6621_SW_TCCLK_RESET_DIS	(0x00 << 2)
#define IT6621_SW_TCCLK_RESET_EN	(0x01 << 2)
#define IT6621_SW_RCCLK_RESET_SEL	BIT(3) /* Software RCCLK clock domain reset */
#define IT6621_SW_RCCLK_RESET_DIS	(0x00 << 3)
#define IT6621_SW_RCCLK_RESET_EN	(0x01 << 3)
#define IT6621_SW_RCLK_RESET_SEL	BIT(7) /* Software RCLK clock domain reset */
#define IT6621_SW_RCLK_RESET_DIS	(0x00 << 7)
#define IT6621_SW_RCLK_RESET_EN		(0x01 << 7)

/* System Status */
#define IT6621_REG_SYS_STATUS		0x07
#define IT6621_HPDIO_STATUS		BIT(0) /* eARC HPDIO status */
#define IT6621_HPDB_STATUS		BIT(1) /* eARC HPDB status */
#define IT6621_TX_XP_LOCK		BIT(2) /* eARC TX XP lock indicator of PLL, 1: locked */
#define IT6621_TX_NO_AUDIO		BIT(4) /* 1: eARC TX no input audio */
#define IT6621_TX_MUTE			BIT(6) /* eARC TX input mute status */

#define IT6621_REG_EARC_TX_AUD_FREQ_NUM	0x0a

/* System Control Registers */
#define IT6621_REG_SYS_CTRL0		0x0c
#define IT6621_EXT_INT_EN		BIT(1) /* External INT pin, 0: Disable, 1: Enable */
#define IT6621_INT_IO_MODE		BIT(2) /* External INT pin mode selection,
						* 0: Push-Pull Mode,
						* 1: Open-Drain mode
						*/
#define IT6621_INT_IO_POL		BIT(3) /* External INT pin active selection,
						* 0: INT active low,
						* 1: INT active high
						*/
#define IT6621_FORCE_WRITE_UPDATE	BIT(4) /* 1: force write update in non-idle state */

#define IT6621_REG_SYS_CTRL1		0x0d
#define IT6621_TX_AUD_FREQ_OPT		BIT(0) /* eARC TX audio sampling frequency option */
#define IT6621_TX_AUD_FREQ_DIV1		(0x00 << 0) /* div 1 */
#define IT6621_TX_AUD_FREQ_DIV4		(0x01 << 0) /* div 4 */
#define IT6621_TX_FORCE_HPDB_LOW_SEL	BIT(4) /* eARC TX force HPDB low */
#define IT6621_TX_FORCE_HPDB_LOW_DIS	(0x00 << 4)
#define IT6621_TX_FORCE_HPDB_LOW_EN	(0x01 << 4)

#define IT6621_REG_SYS_CTRL3		0x0f
#define IT6621_BANK_SEL			GENMASK(1, 0) /* Bank selection */
#define IT6621_BANK0			(0x00 << 0) /* Reg00h~ Regffh */
#define IT6621_BANK1			(0x01 << 0) /* Reg110h ~ Reg1ffh */
#define IT6621_BANK2			(0x02 << 0) /* Reg210h ~ Reg2ffh */
#define IT6621_TX_INT_STATUS		BIT(2) /* eARC TX interrupt status */
#define IT6621_CEC_INT_STATUS		BIT(4) /* eARC CEC interrupt status */

/* Interrupt Status Registers */
#define IT6621_REG_INT_STATUS0		0x10
#define IT6621_TX_HPDIO_ON		BIT(0) /* eARC TX HPDIO on interrupt */
#define IT6621_TX_HPDIO_OFF		BIT(1) /* eARC TX HPDIO off interrupt */
#define IT6621_TX_STATE3_CHANGE		BIT(2) /* eARC TX state3 change interrupt */
#define IT6621_TX_HB_LOST		BIT(3) /* eARC TX HeartBeat lost interrupt */
#define IT6621_TX_DISV_TIMEOUT		BIT(4) /* eARC TX discovery timeout interrupt */
#define IT6621_TX_NO_AUD_CHANGE		BIT(5) /* eARC TX no audio change interrupt */
#define IT6621_TX_XP_LOCK_CHANGE	BIT(6) /* eARC TX XP_LOCK change interrupt */
#define IT6621_TX_RESYNC_ERR		BIT(7) /* eARC TX ReSync error interrupt */

#define IT6621_REG_INT_STATUS1		0x11
#define IT6621_TX_CMD_DONE		BIT(0) /* eARC TX command done interrupt */
#define IT6621_TX_CMD_FAIL		BIT(1) /* eARC TX command fail interrupt */
#define IT6621_TX_HB_DONE		BIT(2) /* eARC TX HeartBeat done interrupt */
#define IT6621_TX_READ_STATE_CHANGE	BIT(3) /* eARC TX read EARC_RX_STAT change interrupt */
#define IT6621_TX_CMDC_BP_ERR		BIT(4) /* eARC TX CMDC bi-phase error interrupt */
#define IT6621_TX_WRITE_STATE_CHANGE	BIT(5) /* eARC TX auto write STAT_CHNG
						* from '1' to '0' done
						*/
#define IT6621_TX_WRITE_CAP_CHANGE	BIT(6) /* eARC TX auto write CAP_CHNG
						* from '1' to '0' done
						*/
#define IT6621_TX_HB_FAIL		BIT(7) /* eARC TX HeartBeat fail interrupt */

#define IT6621_REG_INT_STATUS2		0x12
#define IT6621_TX_FIFO_ERR		BIT(0) /* eARC TX FIFO error interrupt */
#define IT6621_TX_DEC_ERR		BIT(1) /* eARC TX input audio decode error interrupt */
#define IT6621_TX_SPDIF_READY		BIT(2) /* eARC TX input SPDIF channel
						* status ready interrupt
						*/
#define IT6621_TX_SPDIF_CHANGE		BIT(3) /* eARC TX input SPDIF channel
						* status change interrupt
						*/
#define IT6621_TX_MUTE_CHANGE		BIT(4) /* eARC TX input MUTE change interrupt */

#define IT6621_REG_INT_STATUS3		0x13
#define IT6621_TX_HPDB_ON		BIT(0) /* eARC TX HPDB on interrupt */
#define IT6621_TX_HPDB_OFF		BIT(1) /* eARC TX HPDB off interrupt */

#define IT6621_REG_INT_STATUS4		0x19
#define IT6621_DET_ACLK_STABLE		BIT(0) /* Detect ACLK stable interrupt */
#define IT6621_DET_ACLK_VALID		BIT(1) /* Detect ACLK valid interrupt */
#define IT6621_DET_BCLK_STABLE		BIT(2) /* Detect BCLK stable interrupt */
#define IT6621_DET_BCLK_VALID		BIT(3) /* Detect BCLK valid interrupt */
#define IT6621_DET_NO_BCLK		BIT(4) /* Detect no BCLK interrupt */
#define IT6621_DET_NO_DMAC		BIT(5) /* Detect no DMAC interrupt */

/* Interrupt Enable Registers */
#define IT6621_REG_INT_EN0		0x1c
#define IT6621_TX_HPDIO_ON_EN		BIT(0) /* Enable eARC TX HPDIO on interrupt */
#define IT6621_TX_HPDIO_OFF_EN		BIT(1) /* Enable eARC TX HPDIO off interrupt */
#define IT6621_TX_STATE3_CHANGE_EN	BIT(2) /* Enable eARC TX state3 change interrupt */
#define IT6621_TX_HB_LOST_EN		BIT(3) /* Enable eARC TX HeartBeat lost interrupt */
#define IT6621_TX_DISV_TIMEOUT_EN	BIT(4) /* Enable eARC TX discovery timeout interrupt */
#define IT6621_TX_NO_AUD_CHANGE_EN	BIT(5) /* Enable eARC TX no audio change interrupt */
#define IT6621_TX_XP_LOCK_CHANGE_EN	BIT(6) /* Enable eARC TX XP_LOCK change interrupt */
#define IT6621_TX_RESYNC_ERR_EN		BIT(7) /* Enable eARC TX ReSync error interrupt */

#define IT6621_REG_INT_EN1		0x1d
#define IT6621_TX_CMD_DONE_SEL		BIT(0) /* Enable eARC TX command done interrupt */
#define IT6621_TX_CMD_DONE_DIS		(0x00 << 0)
#define IT6621_TX_CMD_DONE_EN		(0x01 << 0)
#define IT6621_TX_CMD_FAIL_SEL		BIT(1) /* Enable eARC TX command fail interrupt */
#define IT6621_TX_CMD_FAIL_DIS		(0x00 << 1)
#define IT6621_TX_CMD_FAIL_EN		(0x01 << 1)
#define IT6621_TX_HB_DONE_SEL		BIT(2) /* Enable eARC TX HeartBeat done interrupt */
#define IT6621_TX_HB_DONE_DIS		(0x00 << 2)
#define IT6621_TX_HB_DONE_EN		(0x01 << 2)
#define IT6621_TX_READ_STATE_CHG_SEL	BIT(3) /* Enable eARC TX read
						* EARC_RX_STAT change interrupt
						*/
#define IT6621_TX_READ_STATE_CHG_DIS	(0x00 << 3)
#define IT6621_TX_READ_STATE_CHG_EN	(0x01 << 3)
#define IT6621_TX_CMDC_BP_ERR_SEL	BIT(4) /* Enable eARC TX CMDC bi-phase error interrupt */
#define IT6621_TX_CMDC_BP_ERR_DIS	(0x00 << 4)
#define IT6621_TX_CMDC_BP_ERR_EN	(0x01 << 4)
#define IT6621_TX_WRITE_STATE_CHG_SEL	BIT(5) /* Enable eARC TX auto write
						* state change interrupt
						*/
#define IT6621_TX_WRITE_STATE_CHG_DIS	(0x00 << 5)
#define IT6621_TX_WRITE_STATE_CHG_EN	(0x01 << 5)
#define IT6621_TX_WRITE_CAP_CHG_SEL	BIT(6) /* Enable eARC TX auto write cap change interrupt */
#define IT6621_TX_WRITE_CAP_CHG_DIS	(0x00 << 6)
#define IT6621_TX_WRITE_CAP_CHG_EN	(0x01 << 6)
#define IT6621_TX_HB_FAIL_SEL		BIT(7) /* Enable eARC TX HeartBeat fail interrupt */
#define IT6621_TX_HB_FAIL_DIS		(0x00 << 7)
#define IT6621_TX_HB_FAIL_EN		(0x01 << 7)

#define IT6621_REG_INT_EN2		0x1e
#define IT6621_TX_FIFO_ERR_EN		BIT(0) /* Enable eARC TX FIFO error interrupt */
#define IT6621_TX_DEC_ERR_EN		BIT(1) /* Enable eARC TX input audio
						* decode error interrupt
						*/
#define IT6621_TX_SPDIF_READY_EN	BIT(2) /* Enable eARC TX input SPDIF
						* channel status ready interrupt
						*/
#define IT6621_TX_SPDIF_CHANGE_EN	BIT(3) /* Enable eARC TX input SPDIF
						* channel status change interrupt
						*/
#define IT6621_TX_MUTE_CHANGE_EN	BIT(4) /* Enable eARC TX input MUTE change interrupt */

#define IT6621_REG_INT_EN3		0x1f
#define IT6621_TX_HPDB_ON_EN		BIT(0) /* Enable eARC TX HPDB on interrupt */
#define IT6621_TX_HPDB_OFF_EN		BIT(1) /* Enable eARC TX HPDB off interrupt */

#define IT6621_REG_INT_EN4		0x25
#define IT6621_DET_ACLK_STABLE_EN	BIT(0) /* Enable detect ACLK stable interrupt */
#define IT6621_DET_ACLK_VALID_EN	BIT(1) /* Enable detect ACLK valid interrupt */
#define IT6621_DET_BCLK_STABLE_EN	BIT(2) /* Enable detect BCLK stable interrupt */
#define IT6621_DET_BCLK_VALID_EN	BIT(3) /* Enable detect BCLK valid interrupt */
#define IT6621_DET_NO_BCLK_EN		BIT(4) /* Enable detect no BCLK interrupt */
#define IT6621_DET_NO_DMAC_EN		BIT(5) /* Enable detect no DMAC interrupt */

/* Clock Control Registers */
#define IT6621_REG_CLK_CTRL0		0x28
#define IT6621_RCLK_FREQ_SEL		GENMASK(1, 0) /* RCLK frequency selection */
#define IT6621_RCLK_FREQ_REFCLK		(0x00 << 0) /* RCLK = REFCLK, 20MHz */
#define IT6621_RCLK_FREQ_REFCLK_DIV_2	(0x01 << 0) /* RCLK = REFCLK/2, 10Mhz */
#define IT6621_RCLK_FREQ_REFCLK_DIV_4	(0x02 << 0) /* RCLK = REFCLK/4, 5Mhz */
#define IT6621_RCLK_FREQ_REFCLK_DIV_8	(0x03 << 0) /* RCLK = REFCLK/8, 2.5Mhz */
#define IT6621_RCLK_PWD_SEL		BIT(2) /* Enable RCLK power-down when IDDQ mode */
#define IT6621_RCLK_PWD_DIS		(0x00 << 2) /* disable */
#define IT6621_RCLK_PWD_EN		(0x01 << 2) /* enable */
#define IT6621_GATE_RCLK_SEL		BIT(4) /* Gating GRCLK clock domain */
#define IT6621_GATE_RCLK_DIS		(0x00 << 4) /* disable */
#define IT6621_GATE_RCLK_EN		(0x01 << 4) /* enable */
#define IT6621_DMAC_PWD_SEL		BIT(6) /* Enable DMAC power-down */
#define IT6621_DMAC_PWD_DIS		(0x00 << 6) /* disable */
#define IT6621_DMAC_PWD_EN		(0x01 << 6) /* enable */
#define IT6621_CMDC_PWD_SEL		BIT(7) /* Enable CMDC power-down */
#define IT6621_CMDC_PWD_DIS		(0x00 << 7) /* disable */
#define IT6621_CMDC_PWD_EN		(0x01 << 7) /* enable */

#define IT6621_REG_CLK_CTRL1		0x29
#define IT6621_TBCLK_SEL		GENMASK(2, 0) /* TBCLK selection */
#define IT6621_TBCLK_AICLK		(0x00 << 0) /* TBCLK = AICLK x 1 */
#define IT6621_TBCLK_AICLK_MUL_2	(0x01 << 0) /* TBCLK = AICLK x 2 */
#define IT6621_TBCLK_AICLK_MUL_4	(0x02 << 0) /* TBCLK = AICLK x 4 */
#define IT6621_TBCLK_AICLK_MUL_8	(0x03 << 0) /* TBCLK = AICLK x 8 */
#define IT6621_TBCLK_AICLK_MUL_16	(0x04 << 0) /* TBCLK = AICLK x 16 */
#define IT6621_TBCLK_AICLK_MUL_32	(0x05 << 0) /* TBCLK = AICLK x 32 */
#define IT6621_TBCLK_AUTO		BIT(3) /* 1: TBCLK auto mode */
#define IT6621_AOCLK_SEL		GENMASK(6, 4) /* AOCLK selection */
#define IT6621_AOCLK_RBCLK		(0x00 << 4) /* AOCLK = RBCLK / 1 */
#define IT6621_AOCLK_RBCLK_DIV_2	(0x01 << 4) /* AOCLK = RBCLK / 2 */
#define IT6621_AOCLK_RBCLK_DIV_4	(0x02 << 4) /* AOCLK = RBCLK / 4 */
#define IT6621_AOCLK_RBCLK_DIV_8	(0x03 << 4) /* AOCLK = RBCLK / 8 */
#define IT6621_AOCLK_RBCLK_DIV_16	(0x04 << 4) /* AOCLK = RBCLK / 16 */
#define IT6621_AOCLK_RBCLK_DIV_32	(0x05 << 4) /* AOCLK = RBCLK / 32 */
#define IT6621_AOCLK_LCCLK		(0x06 << 4) /* AOCLK = LCCLK */
#define IT6621_AOCLK_AUTO		BIT(7) /* 1: AOCLK auto mode */

#define IT6621_REG_CLK_CTRL2		0x2a
#define IT6621_ACLK_INV_SEL		BIT(0) /* eARC ACLK inversion */
#define IT6621_ACLK_INV_DIS		(0x00 << 0) /* disable */
#define IT6621_ACLK_INV_EN		(0x01 << 0) /* enable */
#define IT6621_BCLK_INV_SEL		BIT(1) /* eARC BCLK inversion */
#define IT6621_BCLK_INV_DIS		(0x00 << 1) /* disable */
#define IT6621_BCLK_INV_EN		(0x01 << 1) /* enable */
#define IT6621_RCCLK_OPT		BIT(2) /* RCCLK option */
#define IT6621_MCLKX2_SEL		GENMASK(6, 4) /* MCLKX2 selection */
#define IT6621_MCLKX2_RBCLKX2		(0x00 << 4) /* MCLKX2 = RBCLKX2 / 1 */
#define IT6621_MCLKX2_RBCLKX2_DIV_2	(0x01 << 4) /* MCLKX2 = RBCLKX2 / 2 */
#define IT6621_MCLKX2_RBCLKX2_DIV_4	(0x02 << 4) /* MCLKX2 = RBCLKX2 / 4 */
#define IT6621_MCLKX2_RBCLKX2_DIV_8	(0x03 << 4) /* MCLKX2 = RBCLKX2 / 8 */
#define IT6621_MCLKX2_RBCLKX2_DIV_16	(0x04 << 4) /* MCLKX2 = RBCLKX2 / 16 */
#define IT6621_MCLKX2_RBCLKX2_DIV_32	(0x05 << 4) /* MCLKX2 = RBCLKX2 / 32 */
#define IT6621_MCLKX2_LCCLK		(0x06 << 4) /* MCLKX2 = LCCLK */
#define IT6621_MCLKX2_AUTO		BIT(7) /* 1: MCLKX2 auto mode */

/* eARC TX Discovery and Disconnect Registers */
#define IT6621_REG_TX_DD0		0x30
#define IT6621_TX_DD_FSM_SEL		BIT(0) /* 1: eARC TX enable discovery and disconnect FSM */
#define IT6621_TX_DD_FSM_DIS		(0x00 << 0)
#define IT6621_TX_DD_FSM_EN		(0x01 << 0)
#define IT6621_TX_EARC_SEL		BIT(1) /* 1: enable eARC TX function */
#define IT6621_TX_EARC_DIS		(0x00 << 1)
#define IT6621_TX_EARC_EN		(0x01 << 1)
#define IT6621_TX_ARC_SEL		BIT(2) /* 1: enable ARC TX function */
#define IT6621_TX_ARC_DIS		(0x00 << 2)
#define IT6621_TX_ARC_EN		(0x01 << 2)
#define IT6621_TX_ARC_NOW_SEL		BIT(3) /* 1: ARC TX function is enabled now */
#define IT6621_TX_ARC_NOW_DIS		(0x00 << 3)
#define IT6621_TX_ARC_NOW_EN		(0x01 << 3)
#define IT6621_TX_DISV_TIMEOUT_SEL	GENMASK(5, 4) /* eARC TX discovery
						       * timeout selection
						       * (No COMMA)
						       */
#define IT6621_TX_DISV_TIMEOUT_450MS	(0x00 << 4) /* 450ms */
#define IT6621_TX_DISV_TIMEOUT_475MS	(0x01 << 4) /* 475ms */
#define IT6621_TX_DISV_TIMEOUT_500MS	(0x02 << 4) /* 500ms */
#define IT6621_TX_DISV_TIMEOUT_600MS	(0x03 << 4) /* 600ms */
#define IT6621_TX_HB_POS_SEL		GENMASK(7, 6) /* eARC TX HeartBeat position selection */
#define IT6621_TX_HB_POS_1MS		(0x00 << 6) /* 1ms */
#define IT6621_TX_HB_POS_3MS		(0x01 << 6) /* 3ms */
#define IT6621_TX_HB_POS_5MS		(0x02 << 6) /* 5ms */
#define IT6621_TX_HB_POS_7MS		(0x03 << 6) /* 7ms */

#define IT6621_REG_TX_DD1		0x31
#define IT6621_TX_COMMA_SEL		GENMASK(1, 0) /* eARC TX valid COMMA selection */
#define IT6621_TX_COMMA_8_12MS		(0x00 << 0) /* 8~12ms */
#define IT6621_TX_COMMA_7_13MS		(0x01 << 0) /* 7~13ms */
#define IT6621_TX_COMMA_6_14MS		(0x02 << 0) /* 6~14ms */
#define IT6621_TX_COMMA_5_15MS		(0x03 << 0) /* 5~15ms */
#define IT6621_TX_COMMA_OPT		BIT(2) /* eARC TX COMMA bit error
						* tolerance, 0: no tolerance,
						* 1: 1-bit tolerance
						*/

#define IT6621_REG_TX_DD2		0x32
#define IT6621_TX_DD_FSM_STATE		GENMASK(5, 0) /* eARC TX discovery and
						       * disconnect FSM state
						       */
#define IT6621_TX_EARC_MODE		(0x08 << 0)
#define IT6621_TX_ARC_MODE		(0x20 << 0)

#define IT6621_REG_TX_DD3		0x33
#define IT6621_TX_FORCE_ARC_MODE_SEL	BIT(0) /* force TX ARC mode */
#define IT6621_TX_FORCE_ARC_MODE_DIS	(0x00 << 0)
#define IT6621_TX_FORCE_ARC_MODE_EN	(0x01 << 0)

/* eARC TX CMDC Registers */
#define IT6621_REG_CMDC0		0x40
#define IT6621_TX_CMD_TIMEOUT_EN	BIT(0) /* 1: eARC TX enable NACK/RETRY timeout (256-time) */
#define IT6621_TX_NACK_DELAY_EN		BIT(1) /* 1: eARC TX enable NACK delay */
#define IT6621_TX_DEBUG_FIFO_EN		BIT(2) /* 1: eARC TX enable debug FIFO */
#define IT6621_TX_NEXT_PKT_TIMEOUT_SEL	BIT(3) /* eARC TX next packet timeout selection */
#define IT6621_TX_NEXT_PKT_TIMEOUT_30US	(0x00 << 3) /* 30us */
#define IT6621_TX_NEXT_PKT_TIMEOUT_50US	(0x01 << 3) /* 50us */
#define IT6621_TX_NACK_DELAY_SEL	GENMASK(5, 4) /* eARC TX NACK packet delay time selection */
#define IT6621_TX_NACK_DELAY_500US	(0x00 << 4) /* 500us */
#define IT6621_TX_NACK_DELAY_1MS	(0x01 << 4) /* 1ms */
#define IT6621_TX_NACK_DELAY_2MS	(0x02 << 4) /* 2ms */
#define IT6621_TX_NACK_DELAY_4MS	(0x03 << 4) /* 4ms */
#define IT6621_TX_TURN_OVER_SEL		GENMASK(7, 6) /* eARC TX turn-over time
						       * selection before
						       * transmitting packet
						       */
#define IT6621_TX_TURN_OVER_8US		(0x00 << 6) /* 8us */
#define IT6621_TX_TURN_OVER_16US	(0x01 << 6) /* 16us */
#define IT6621_TX_TURN_OVER_24US	(0x02 << 6) /* 24us */

#define IT6621_REG_CMDC1		0x41
#define IT6621_TX_CMDC_STATE		GENMASK(3, 0) /* eARC TX CMDC state */
#define IT6621_TX_CMDC_STATE_IDLE	(0x00 << 0) /* IDLE */
#define IT6621_TX_CMDC_STATE_CMD	(0x01 << 0) /* TX Cmd */
#define IT6621_TX_CMDC_STATE_DEVID	(0x02 << 0) /* TX DevID */
#define IT6621_TX_CMDC_STATE_OFFSET	(0x03 << 0) /* TX Offset */
#define IT6621_TX_CMDC_STATE_CONT1	(0x04 << 0) /* TX Cont1 */
#define IT6621_TX_CMDC_STATE_CONT2	(0x05 << 0) /* TX Cont2 */
#define IT6621_TX_CMDC_STATE_RETRY	(0x06 << 0) /* TX Retry */
#define IT6621_TX_CMDC_STATE_DATA1	(0x07 << 0) /* TX Data1 */
#define IT6621_TX_CMDC_STATE_DATA2	(0x08 << 0) /* TX Data2 */
#define IT6621_TX_CMDC_STATE_STOP	(0x09 << 0) /* TX Stop */
#define IT6621_TX_FAIL_STATE		GENMASK(7, 4)
#define IT6621_TX_FAIL_STATE_NO_RESP	BIT(4) /* No response */
#define IT6621_TX_FAIL_STATE_UNEXP_RESP	BIT(5) /* Unexpected response */
#define IT6621_TX_FAIL_STATE_ECC_ERR	BIT(6) /* Uncorrectable ECC error */
#define IT6621_TX_FAIL_STATE_TIMEOUT	BIT(7) /* NACK/RETRY 256 times timeout */

#define IT6621_REG_TX_DEV_ID		0x42 /* eARC TX device ID */
#define IT6621_REG_TX_OFFSET		0x43 /* eARC TX offset value */

#define IT6621_REG_CMDC2		0x44
#define IT6621_TX_BYTE_NO		GENMASK(4, 0) /* eARC TX byte number (1~32 bytes) */
#define IT6621_TX_CMD_BUSY		BIT(5) /* 1: eARC TX command busy */
#define IT6621_TX_WRITE_TRIGGER		BIT(6) /* eARC TX write trigger */
#define IT6621_TX_READ_TRIGGER		BIT(7) /* eARC TX read trigger */

#define IT6621_REG_TX_DATA_FIFO		0x45 /* eARC TX data FIFO of read/write
					      * data (32-stage FIFO)
					      */

#define IT6621_REG_CMDC3		0x46
#define IT6621_TX_DATA_FIFO_STAGE	GENMASK(5, 0) /* eARC TX data FIFO stage */
#define IT6621_TX_DATA_FIFO_EMPTY	(0x00 << 0) /* FIFO empty */
#define IT6621_TX_DATA_FIFO_FULL	(0x20 << 0) /* FIFO full */
#define IT6621_TX_DATA_FIFO_ERR		BIT(6) /* eARC TX data FIFO error */
#define IT6621_TX_DATA_FIFO_CLEAR	BIT(7) /* eARC TX data FIFO clear */

#define IT6621_REG_CMDC4		0x47
#define IT6621_TX_AUTO_HB_EN		BIT(0) /* eARC TX auto HeartBeat, 0: disable, 1: enable */
#define IT6621_TX_AUTO_HB_BUSY		BIT(1) /* 1: eARC TX auto HeartBeat busy */
#define IT6621_TX_HB_TIME_SEL		GENMASK(3, 2) /* eARC TX auto HeartBeat time selection */
#define IT6621_TX_HB_TIME_35MS		(0x00 << 2) /* 35ms */
#define IT6621_TX_HB_TIME_40MS		(0x01 << 2) /* 40ms */
#define IT6621_TX_HB_TIME_45MS		(0x02 << 2) /* 45ms */
#define IT6621_TX_HB_TIME_50MS		(0x03 << 2) /* 50ms */
#define IT6621_TX_HB_RETRY_SEL		BIT(4) /* eARC TX auto HeartBeat retry,
						* 0: disable, 1: enable
						*/
#define IT6621_TX_HB_RETRY_DIS		(0x00 << 4)
#define IT6621_TX_HB_RETRY_EN		(0x01 << 4)
#define IT6621_TX_HB_TRIGGER		BIT(5) /* eARC TX HeartBeat trigger */
#define IT6621_TX_HB_RETRY_TIME_SEL	GENMASK(7, 6) /* eARC TX HeartBeat retry time selection */
#define IT6621_TX_HB_RETRY_0MS		(0x00 << 6) /* 0ms */
#define IT6621_TX_HB_RETRY_4MS		(0x01 << 6) /* 4ms */
#define IT6621_TX_HB_RETRY_8MS		(0x02 << 6) /* 8ms */
#define IT6621_TX_HB_RETRY_16MS		(0x03 << 6) /* 16ms */

#define IT6621_REG_TX_READ_STATE	0x48 /* eARC TX read back value of EARC_RX_STAT */
#define IT6621_EARC_RX_STAT_EARC_HPD	BIT(0)
#define IT6621_EARC_RX_STAT_CAP_CHNG	BIT(3)
#define IT6621_EARC_RX_STAT_STAT_CHNG	BIT(4)

#define IT6621_REG_CMDC5		0x49
#define IT6621_TX_AUTO_WRITE_STATE_SEL	BIT(0) /* 1: eARC TX auto write EARC_TX_STAT[7:1] */
#define IT6621_TX_AUTO_WRITE_STATE_DIS	(0x00 << 0) /* to pass SL-870 HFR5-1-35/36/37 */
#define IT6621_TX_AUTO_WRITE_STATE_EN	(0x01 << 0)
#define IT6621_TX_WRITE_STATE		GENMASK(7, 1) /* eARC TX write value of EARC_TX_STAT[7:1] */

#define IT6621_REG_CMDC6		0x4a
#define IT6621_TX_CMO_OPT		BIT(0) /* eARC TX common mode output enable option */
#define IT6621_TX_CMO_NORMAL		(0x00 << 0) /* normal */
#define IT6621_TX_CMO_PRE_1BIT		(0x01 << 0) /* 1-bit before normal */
#define IT6621_TX_FORCE_CMO_OPT		BIT(1) /* eARC TX common mode output enable option */
#define IT6621_TX_FORCE_CMO_NORMAL	(0x00 << 1) /* normal */
#define IT6621_TX_FORCE_CMO_EN		(0x01 << 1) /* force output enable */
#define IT6621_TX_RESYNC_OPT		BIT(2) /* eARC TX common mode resync option */
#define IT6621_TX_RESYNC_OLD		(0x00 << 2) /* old */
#define IT6621_TX_RESYNC_NEW		(0x01 << 2) /* new */

#define IT6621_REG_TX_DEBUG_FIFO0	0x4d /* eARC TX debug FIFO (32-stage
					      * FIFO), Cmd[7:0]/Data[7:0]
					      */

#define IT6621_REG_TX_DEBUG_FIFO1	0x4e
#define IT6621_TX_DEBUG_FIFO_CD		BIT(0) /* C/D */
#define IT6621_TX_DEBUG_FIFO_DATA	(0x00 << 0) /* Data */
#define IT6621_TX_DEBUG_FIFO_CMD	(0x01 << 0) /* Cmd */
#define IT6621_TX_DEBUG_FIFO_IO		BIT(1) /* I/O */
#define IT6621_TX_DEBUG_FIFO_INPUT_PKT	(0x00 << 1) /* input packet */
#define IT6621_TX_DEBUG_FIFO_OUTPUT_PKT	(0x01 << 1) /* output packet */
#define IT6621_TX_DEBUG_FIFO_ECC_ERR	BIT(2) /* 1: ECC error */

#define IT6621_REG_TX_DEBUG_FIFO2	0x4f
#define IT6621_TX_DEBUG_FIFO_STAGE	GENMASK(5, 0) /* eARC TX debug FIFO stage */
#define IT6621_TX_DEBUG_FIFO_EMPTY	(0x00 << 0) /* FIFO empty */
#define IT6621_TX_DEBUG_FIFO_FULL	(0x20 << 0) /* FIFO full */
#define IT6621_TX_DEBUG_FIFO_ERR	BIT(6) /* eARC TX debug FIFO error */
#define IT6621_TX_DEBUG_FIFO_CLEAR_SEL	BIT(7) /* eARC TX debug FIFO clear */
#define IT6621_TX_DEBUG_FIFO_CLEAR_DIS	(0x00 << 7) /* FIFO empty */
#define IT6621_TX_DEBUG_FIFO_CLEAR_EN	(0x01 << 7)

/* eARC TX DMAC Control Registers */
#define IT6621_REG_DMAC_CTRL0		0x60
#define IT6621_TX_ECC_EN		BIT(0) /* eARC TX ECC, 0: disable, 1: enable */
#define IT6621_TX_AUTO_ECC		BIT(1) /* 1: eARC TX auto ECC */
#define IT6621_TX_ECC_OPT		BIT(2) /* eARC TX ECC option */
#define IT6621_TX_ECC_VC_SWAP		(0x00 << 2) /* vc swap */
#define IT6621_TX_ECC_U_SWAP		(0x01 << 2) /* u swap */
#define IT6621_TX_ENC_SEL		BIT(3) /* eARC TX encryption, 0: disable, 1: enable */
#define IT6621_TX_ENC_DIS		(0x00 << 3) /* disable */
#define IT6621_TX_ENC_EN		(0x01 << 3) /* enable */
#define IT6621_TX_ENC_OPT		GENMASK(5, 4) /* eARC TX encryption option */
#define IT6621_TX_ENC_OPT0		(0x00 << 4)
#define IT6621_TX_ENC_OPT1		(0x01 << 4)
#define IT6621_TX_ENC_OPT2		(0x02 << 4)
#define IT6621_TX_ENC_OPT3		(0x03 << 4)

#define IT6621_REG_TX_ENC_SEED_LOW	0x61 /* eARC TX encryption seed */
#define IT6621_REG_TX_ENC_SEED_HIGH	0x62 /* eARC TX encryption seed */

#define IT6621_REG_DMAC_CTRL1		0x63
#define IT6621_TX_MANUAL_RESET_EN_SEL	BIT(0) /* 1: enable eARC TX audio FIFO manual reset */
#define IT6621_TX_MANUAL_RESET_DIS	(0x00 << 0)
#define IT6621_TX_MANUAL_RESET_EN	(0x01 << 0)
#define IT6621_TX_AUTO_RESET_EN		BIT(1) /* 1: enable eARC TX audio FIFO auto reset */
#define IT6621_TX_PKT1_EN_SEL		BIT(2) /* 1: enable eARC TX packet 1 using U-bit */
#define IT6621_TX_PKT1_DIS		(0x00 << 2)
#define IT6621_TX_PKT1_EN		(0x01 << 2)
#define IT6621_TX_PKT2_EN_SEL		BIT(3) /* 1: enable eARC TX packet 2 using U-bit */
#define IT6621_TX_PKT2_DIS		(0x00 << 3)
#define IT6621_TX_PKT2_EN		(0x01 << 3)
#define IT6621_TX_PKT3_EN_SEL		BIT(4) /* 1: enable eARC TX packet 3 using U-bit */
#define IT6621_TX_PKT3_DIS		(0x00 << 4)
#define IT6621_TX_PKT3_EN		(0x01 << 4)
#define IT6621_TX_C_CH_OPT		BIT(5) /* 1: eARC TX U-bit C-Ch option */
#define IT6621_TX_C_CH_OPT1		(0x00 << 5) /* use RegTxChSt[14:8] for
						     * C-Ch[14:8] and
						     * RegTxPktCH[7:0] is for
						     * CH[7:0] which should be
						     * set 0x00
						     */
#define IT6621_TX_C_CH_OPT2		(0x01 << 5) /* use RegTxPktCH[6:0] for
						     * C-Ch[14:8] and CH[7:0]
						     * is set 0x00
						     */
#define IT6621_TX_U_BIT_OPT		BIT(6) /* U-bit option */
#define IT6621_TX_U_BIT_1BIT_FRAME	(0x00 << 6) /* 1-bit/frame */
#define IT6621_TX_U_BIT_1BIT_SUB_FRAME	(0x01 << 6) /* 1-bit/sub-frame */
#define IT6621_TX_V_BIT_VAL		BIT(7) /* V-bit value */
#define IT6621_TX_V_BIT_LPCM		(0x00 << 7) /* LPCM */
#define IT6621_TX_V_BIT_NLPCM		(0x01 << 7) /* NLPCM */

/* Input Audio Control Registers */
#define IT6621_REG_TX_AUD_SRC_EN	0x80 /* Enable input audio source */
#define IT6621_AUD_SRC0_EN		BIT(0) /* for audio source 0 */
#define IT6621_AUD_SRC1_EN		BIT(1) /* for audio source 1 */
#define IT6621_AUD_SRC2_EN		BIT(2) /* for audio source 2 */
#define IT6621_AUD_SRC3_EN		BIT(3) /* for audio source 3 */
#define IT6621_AUD_SRC4_EN		BIT(4) /* for audio source 4 */
#define IT6621_AUD_SRC5_EN		BIT(5) /* for audio source 5 */
#define IT6621_AUD_SRC6_EN		BIT(6) /* for audio source 6 */
#define IT6621_AUD_SRC7_EN		BIT(7) /* for audio source 7 */
#define IT6621_AUD_SRC7_SRC0_EN		(IT6621_AUD_SRC7_EN | \
					 IT6621_AUD_SRC6_EN | \
					 IT6621_AUD_SRC5_EN | \
					 IT6621_AUD_SRC4_EN | \
					 IT6621_AUD_SRC3_EN | \
					 IT6621_AUD_SRC2_EN | \
					 IT6621_AUD_SRC1_EN | \
					 IT6621_AUD_SRC0_EN)
#define IT6621_AUD_SRC6_SRC0_EN		(IT6621_AUD_SRC6_EN | \
					 IT6621_AUD_SRC5_EN | \
					 IT6621_AUD_SRC4_EN | \
					 IT6621_AUD_SRC3_EN | \
					 IT6621_AUD_SRC2_EN | \
					 IT6621_AUD_SRC1_EN | \
					 IT6621_AUD_SRC0_EN)
#define IT6621_AUD_SRC5_SRC0_EN		(IT6621_AUD_SRC5_EN | \
					 IT6621_AUD_SRC4_EN | \
					 IT6621_AUD_SRC3_EN | \
					 IT6621_AUD_SRC2_EN | \
					 IT6621_AUD_SRC1_EN | \
					 IT6621_AUD_SRC0_EN)
#define IT6621_AUD_SRC4_SRC0_EN		(IT6621_AUD_SRC4_EN | \
					 IT6621_AUD_SRC3_EN | \
					 IT6621_AUD_SRC2_EN | \
					 IT6621_AUD_SRC1_EN | \
					 IT6621_AUD_SRC0_EN)
#define IT6621_AUD_SRC3_SRC0_EN		(IT6621_AUD_SRC3_EN | \
					 IT6621_AUD_SRC2_EN | \
					 IT6621_AUD_SRC1_EN | \
					 IT6621_AUD_SRC0_EN)
#define IT6621_AUD_SRC2_SRC0_EN		(IT6621_AUD_SRC2_EN | \
					 IT6621_AUD_SRC1_EN | \
					 IT6621_AUD_SRC0_EN)
#define IT6621_AUD_SRC1_SRC0_EN		(IT6621_AUD_SRC1_EN | \
					 IT6621_AUD_SRC0_EN)

#define IT6621_REG_TX_AUD_SRC_SEL	0x81 /* Input audio source selection */
#define IT6621_TX_AUD_SRC		GENMASK(1, 0) /* eARC TX encryption option */
#define IT6621_AUD_SRC_I2S		(0x00 << 0) /* I2S */
#define IT6621_AUD_SRC_SPDIF		(0x01 << 0) /* SPDIF */
#define IT6621_AUD_SRC_TDM		(0x02 << 0) /* TDM */
#define IT6621_AUD_SRC_DSD		(0x03 << 0) /* DSD */

#define IT6621_REG_TX_I2S		0x82
#define IT6621_I2S_FMT			GENMASK(4, 0) /* TX I2S/TDM format */
#define IT6621_I2S_FMT_STANDARD		(0x00 << 0) /* Standard I2S */
#define IT6621_I2S_FMT_32BIT		(0x01 << 0) /* 32-bit I2S */
#define IT6621_I2S_FMT_LEFT_JUSTIFIED	(0x00 << 1) /* Left-justified */
#define IT6621_I2S_FMT_RIGHT_JUSTIFIED	(0x01 << 1) /* Right-justified */
#define IT6621_I2S_FMT_DATA_DELAY	(0x00 << 2) /* Data delay 1T correspond to WS */
#define IT6621_I2S_FMT_NO_DATA_DELAY	(0x01 << 2) /* No data delay correspond to WS */
#define IT6621_I2S_FMT_WS_0_LEFT_CH	(0x00 << 3) /* WS=0 is left channel */
#define IT6621_I2S_FMT_WS_0_RIGHT_CH	(0x01 << 3) /* WS=0 is right channel */
#define IT6621_I2S_FMT_MSB_FIRST	(0x00 << 4) /* MSB shift first */
#define IT6621_I2S_FMT_LSB_FIRST	(0x01 << 4) /* LSB shift first */
#define IT6621_I2S_TDM_WORD_LEN		GENMASK(6, 5) /* TX I2S/TDM word length */
#define IT6621_I2S_TDM_16BIT		(0x00 << 5) /* 16 bits */
#define IT6621_I2S_TDM_18BIT		(0x01 << 5) /* 18 bits */
#define IT6621_I2S_TDM_20BIT		(0x02 << 5) /* 20 bits */
#define IT6621_I2S_TDM_24BIT		(0x03 << 5) /* 24 bits */

#define IT6621_REG_TX_AUD_CTRL3		0x83
#define IT6621_TX_TDM_CH_NUM		GENMASK(2, 0) /* TX TDM interface channel number */
#define IT6621_TX_TDM_2CH		(0x00 << 0) /* 2-ch */
#define IT6621_TX_TDM_4CH		(0x01 << 0) /* 4-ch */
#define IT6621_TX_TDM_8CH		(0x03 << 0) /* 8-ch */
#define IT6621_TX_TDM_16CH		(0x07 << 0) /* 16-ch */
#define IT6621_TX_I2S_HBR_EN_SEL	BIT(3) /* enable TX I2S HBR input */
#define IT6621_TX_I2S_HBR_DIS		(0x00 << 3)
#define IT6621_TX_I2S_HBR_EN		(0x01 << 3)
#define IT6621_TX_DSD_MODE		BIT(4) /* TX DSD mode */
#define IT6621_TX_DSD_MODE_NORMAL	(0x00 << 4) /* normal mode */
#define IT6621_TX_DSD_MODE_PHASE	(0x01 << 4) /* phase mode */
#define IT6621_TX_DSD_BIT_INV_SEL	BIT(5) /* TX DSD data inversion */
#define IT6621_TX_DSD_BIT_NO_INV	(0x00 << 5) /* not inverse */
#define IT6621_TX_DSD_BIT_INV		(0x01 << 5) /* inverse */
#define IT6621_TX_DSD_LM_SWAP_SEL	BIT(6) /* TX DSD LSB/MSB swap */
#define IT6621_TX_DSD_LM_NO_SWAP	(0x00 << 6) /* no swap */
#define IT6621_TX_DSD_LM_SWAP		(0x01 << 6) /* swap */
#define IT6621_TX_DSD_LR_SWAP_SEL	BIT(7) /* TX DSD L/R swap */
#define IT6621_TX_DSD_LR_NO_SWAP	(0x00 << 7) /* no swap */
#define IT6621_TX_DSD_LR_SWAP		(0x01 << 7) /* swap */

#define IT6621_REG_TX_AUD_SEL1		0x84
#define IT6621_TX_AUD0_SEL		GENMASK(2, 0) /* TX audio 0 input selection */
#define IT6621_TX_AUD0_FROM_SRC0	(0x00 << 0) /* from audio source 0 */
#define IT6621_TX_AUD0_FROM_SRC1	(0x01 << 0) /* from audio source 1 */
#define IT6621_TX_AUD0_FROM_SRC2	(0x02 << 0) /* from audio source 2 */
#define IT6621_TX_AUD0_FROM_SRC3	(0x03 << 0) /* from audio source 3 */
#define IT6621_TX_AUD0_FROM_SRC4	(0x04 << 0) /* from audio source 4 */
#define IT6621_TX_AUD0_FROM_SRC5	(0x05 << 0) /* from audio source 5 */
#define IT6621_TX_AUD0_FROM_SRC6	(0x06 << 0) /* from audio source 6 */
#define IT6621_TX_AUD0_FROM_SRC7	(0x07 << 0) /* from audio source 7 */
#define IT6621_TX_AUD1_SEL		GENMASK(6, 4) /* TX audio 1 input selection */
#define IT6621_TX_AUD1_FROM_SRC0	(0x00 << 4) /* from audio source 0 */
#define IT6621_TX_AUD1_FROM_SRC1	(0x01 << 4) /* from audio source 1 */
#define IT6621_TX_AUD1_FROM_SRC2	(0x02 << 4) /* from audio source 2 */
#define IT6621_TX_AUD1_FROM_SRC3	(0x03 << 4) /* from audio source 3 */
#define IT6621_TX_AUD1_FROM_SRC4	(0x04 << 4) /* from audio source 4 */
#define IT6621_TX_AUD1_FROM_SRC5	(0x05 << 4) /* from audio source 5 */
#define IT6621_TX_AUD1_FROM_SRC6	(0x06 << 4) /* from audio source 6 */
#define IT6621_TX_AUD1_FROM_SRC7	(0x07 << 4) /* from audio source 7 */

#define IT6621_REG_TX_AUD_SEL2		0x85
#define IT6621_TX_AUD2_SEL		GENMASK(2, 0) /* TX audio 2 input selection */
#define IT6621_TX_AUD2_FROM_SRC0	(0x00 << 0) /* from audio source 0 */
#define IT6621_TX_AUD2_FROM_SRC1	(0x01 << 0) /* from audio source 1 */
#define IT6621_TX_AUD2_FROM_SRC2	(0x02 << 0) /* from audio source 2 */
#define IT6621_TX_AUD2_FROM_SRC3	(0x03 << 0) /* from audio source 3 */
#define IT6621_TX_AUD2_FROM_SRC4	(0x04 << 0) /* from audio source 4 */
#define IT6621_TX_AUD2_FROM_SRC5	(0x05 << 0) /* from audio source 5 */
#define IT6621_TX_AUD2_FROM_SRC6	(0x06 << 0) /* from audio source 6 */
#define IT6621_TX_AUD2_FROM_SRC7	(0x07 << 0) /* from audio source 7 */
#define IT6621_TX_AUD3_SEL		GENMASK(6, 4) /* TX audio 3 input selection */
#define IT6621_TX_AUD3_FROM_SRC0	(0x00 << 4) /* from audio source 0 */
#define IT6621_TX_AUD3_FROM_SRC1	(0x01 << 4) /* from audio source 1 */
#define IT6621_TX_AUD3_FROM_SRC2	(0x02 << 4) /* from audio source 2 */
#define IT6621_TX_AUD3_FROM_SRC3	(0x03 << 4) /* from audio source 3 */
#define IT6621_TX_AUD3_FROM_SRC4	(0x04 << 4) /* from audio source 4 */
#define IT6621_TX_AUD3_FROM_SRC5	(0x05 << 4) /* from audio source 5 */
#define IT6621_TX_AUD3_FROM_SRC6	(0x06 << 4) /* from audio source 6 */
#define IT6621_TX_AUD3_FROM_SRC7	(0x07 << 4) /* from audio source 7 */

#define IT6621_REG_TX_AUD_SEL3		0x86
#define IT6621_TX_AUD4_SEL		GENMASK(2, 0) /* TX audio 4 input selection */
#define IT6621_TX_AUD4_FROM_SRC0	(0x00 << 0) /* from audio source 0 */
#define IT6621_TX_AUD4_FROM_SRC1	(0x01 << 0) /* from audio source 1 */
#define IT6621_TX_AUD4_FROM_SRC2	(0x02 << 0) /* from audio source 2 */
#define IT6621_TX_AUD4_FROM_SRC3	(0x03 << 0) /* from audio source 3 */
#define IT6621_TX_AUD4_FROM_SRC4	(0x04 << 0) /* from audio source 4 */
#define IT6621_TX_AUD4_FROM_SRC5	(0x05 << 0) /* from audio source 5 */
#define IT6621_TX_AUD4_FROM_SRC6	(0x06 << 0) /* from audio source 6 */
#define IT6621_TX_AUD4_FROM_SRC7	(0x07 << 0) /* from audio source 7 */
#define IT6621_TX_AUD5_SEL		GENMASK(6, 4) /* TX audio 5 input selection */
#define IT6621_TX_AUD5_FROM_SRC0	(0x00 << 4) /* from audio source 0 */
#define IT6621_TX_AUD5_FROM_SRC1	(0x01 << 4) /* from audio source 1 */
#define IT6621_TX_AUD5_FROM_SRC2	(0x02 << 4) /* from audio source 2 */
#define IT6621_TX_AUD5_FROM_SRC3	(0x03 << 4) /* from audio source 3 */
#define IT6621_TX_AUD5_FROM_SRC4	(0x04 << 4) /* from audio source 4 */
#define IT6621_TX_AUD5_FROM_SRC5	(0x05 << 4) /* from audio source 5 */
#define IT6621_TX_AUD5_FROM_SRC6	(0x06 << 4) /* from audio source 6 */
#define IT6621_TX_AUD5_FROM_SRC7	(0x07 << 4) /* from audio source 7 */

#define IT6621_REG_TX_AUD_SEL4		0x87
#define IT6621_TX_AUD6_SEL		GENMASK(2, 0) /* TX audio 6 input selection */
#define IT6621_TX_AUD6_FROM_SRC0	(0x00 << 0) /* from audio source 0 */
#define IT6621_TX_AUD6_FROM_SRC1	(0x01 << 0) /* from audio source 1 */
#define IT6621_TX_AUD6_FROM_SRC2	(0x02 << 0) /* from audio source 2 */
#define IT6621_TX_AUD6_FROM_SRC3	(0x03 << 0) /* from audio source 3 */
#define IT6621_TX_AUD6_FROM_SRC4	(0x04 << 0) /* from audio source 4 */
#define IT6621_TX_AUD6_FROM_SRC5	(0x05 << 0) /* from audio source 5 */
#define IT6621_TX_AUD6_FROM_SRC6	(0x06 << 0) /* from audio source 6 */
#define IT6621_TX_AUD6_FROM_SRC7	(0x07 << 0) /* from audio source 7 */
#define IT6621_TX_AUD7_SEL		GENMASK(6, 4) /* TX audio 7 input selection */
#define IT6621_TX_AUD7_FROM_SRC0	(0x00 << 4) /* from audio source 0 */
#define IT6621_TX_AUD7_FROM_SRC1	(0x01 << 4) /* from audio source 1 */
#define IT6621_TX_AUD7_FROM_SRC2	(0x02 << 4) /* from audio source 2 */
#define IT6621_TX_AUD7_FROM_SRC3	(0x03 << 4) /* from audio source 3 */
#define IT6621_TX_AUD7_FROM_SRC4	(0x04 << 4) /* from audio source 4 */
#define IT6621_TX_AUD7_FROM_SRC5	(0x05 << 4) /* from audio source 5 */
#define IT6621_TX_AUD7_FROM_SRC6	(0x06 << 4) /* from audio source 6 */
#define IT6621_TX_AUD7_FROM_SRC7	(0x07 << 4) /* from audio source 7 */

#define IT6621_REG_TX_AUD_FORCE_OUTPUT	0x88 /* 1: force output data to 24-bit '0' */

#define IT6621_REG_TX_AUD_CTRL9		0x89
#define IT6621_TX_SPDIF_REC_EN		BIT(0) /* 1: enable SPDIF input record channel status */
#define IT6621_TX_SPDIF_ERR_DET_EN	BIT(1) /* 1: enable SPDIF input channel
						* status error detection
						*/
#define IT6621_TX_MCLK_FREQ		GENMASK(3, 2) /* Input MCLK frequency selection */
#define IT6621_TX_MCLK_FREQ_128FS	(0x00 << 2) /* 128FS */
#define IT6621_TX_MCLK_FREQ_256FS	(0x01 << 2) /* 256FS */
#define IT6621_TX_MCLK_FREQ_512FS	(0x02 << 2) /* 512FS */
#define IT6621_TX_MCLK_FREQ_1024FS	(0x03 << 2) /* 1024FS */
#define IT6621_TX_MUTE_TO_OFF_EN	BIT(4) /* 1: enable TXMUTE to audio off */
#define IT6621_TX_FIFO_ERR_TO_MUTE_EN	BIT(5) /* 1: enable FIFO error to audio mute */
#define IT6621_TX_NO_CLK_TO_MUTE_EN	BIT(6) /* 1: enable no TBCLK to audio mute */
#define IT6621_TX_UNSTB_CLK_TO_MUTE_EN	BIT(7) /* 1: enable TBCLK unstable to audio mute */

#define IT6621_REG_TX_AUD_CTRL10	0x8a
#define IT6621_TX_EXT_MUTE_SEL		BIT(0) /* external MUTE input
						* RegEnTxMute=RegF0[1] at FPGA
						* register (SlvAddr=0x88)
						*/
#define IT6621_TX_EXT_MUTE_EN		(0x00 << 0) /* enable */
#define IT6621_TX_EXT_MUTE_DIS		(0x01 << 0) /* disable */
#define IT6621_TX_AUTO_CH_STAT_MUTE_SEL	BIT(1) /* eARC TX channel status MUTE auto mode */
#define IT6621_TX_AUTO_CH_STAT_MUTE_DIS	(0x00 << 1) /* disable */
#define IT6621_TX_AUTO_CH_STAT_MUTE_EN	(0x01 << 1) /* enable */
#define IT6621_TX_AUTO_AUD_FMT_SEL	BIT(2) /* eARC TX channel status audio format auto mode */
#define IT6621_TX_AUTO_AUD_FMT_DIS	(0x00 << 2) /* disable */
#define IT6621_TX_AUTO_AUD_FMT_EN	(0x01 << 2) /* enable */
#define IT6621_TX_AUTO_LAYOUT_SEL	BIT(3) /* eARC TX channel status layout auto mode */
#define IT6621_TX_AUTO_LAYOUT_DIS	(0x00 << 3) /* disable */
#define IT6621_TX_AUTO_LAYOUT_EN	(0x01 << 3) /* enable */
#define IT6621_TX_MCH_LPCM_SEL		BIT(4) /* eARC TX force multi-channel LPCM */
#define IT6621_TX_MCH_LPCM_DIS		(0x00 << 4) /* disable */
#define IT6621_TX_MCH_LPCM_EN		(0x01 << 4) /* enable */
#define IT6621_TX_NLPCM_I2S_SEL		BIT(5) /* eARC TX force NLPCM from I2S interface */
#define IT6621_TX_NLPCM_I2S_DIS		(0x00 << 5) /* disable */
#define IT6621_TX_NLPCM_I2S_EN		(0x01 << 5) /* enable */
#define IT6621_TX_2CH_LAYOUT_SEL	BIT(6) /* eARC TX force 2-channel layout */
#define IT6621_TX_2CH_LAYOUT_DIS	(0x00 << 6) /* disable */
#define IT6621_TX_2CH_LAYOUT_EN		(0x01 << 6) /* enable */
#define IT6621_TX_FORCE_MUTE_SEL	BIT(7) /* force TXMUTE */
#define IT6621_TX_FORCE_MUTE_DIS	(0x00 << 7) /* disable */
#define IT6621_TX_FORCE_MUTE_EN		(0x01 << 7) /* enable */

/* eARC TX AFE Registers */
#define IT6621_REG_TX_AFE0		0xa0
#define IT6621_TX_XP_RESET_SEL		BIT(0) /* Reset PLL, low active */
#define IT6621_TX_XP_RESET_ACTIVE	(0x00 << 0) /* active */
#define IT6621_TX_XP_RESET_INACTIVE	(0x01 << 0) /* inactive */
#define IT6621_TX_XP_PWD		BIT(1) /* Power down PLL, low active */
#define IT6621_TX_XP_IPWD		BIT(2) /* Power down the bias for PLL and LC, low active */

#define IT6621_REG_TX_AFE1		0xa1
#define IT6621_TX_XP_PDIV		GENMASK(1, 0) /* Set PDIV[1:0] by
						       * TBCLKSEL[2:0] and the
						       * frequency of AICLK.
						       * When TBCLKSEL=101, set 00
						       * When TBCLKSEL=100
						       * 2.048MHz~3.072MHz: set 00
						       * 4.096MHz~6.144MHz: set 01
						       * When TBCLKSEL=011
						       * 2.048MHz~3.072MHz: set 00
						       * 4.096MHz~6.144MHz: set 01
						       * 8.192MHz~12.288MHz: set 10 or 11
						       * When TBCLKSEL=010
						       * 4.096MHz~6.144MHz: set 00
						       * 8.192MHz~12.288MHz: set 01
						       * 16.384MHz~24.576MHz: set 10 or 11
						       * When TBCLKSEL=001
						       * 2.048MHz~12.288MHz: set 00
						       * 16.384MHz~24.576MHz: set 01
						       * 32.768MHz~49.152MHz: set 10 or 11
						       * When TBCLKSEL=000
						       * 4.096MHz~24.576MHz: set 00
						       * 32.768MHz~49.152MHz: set 01
						       * 65.536MHz~98.304MHz: set 10 or 11
						       */
#define IT6621_TX_XP_GAIN		BIT(2) /* Set GAIN by TBCLKSEL[2:0] and
						* the frequency of AICLK
						* When TBCLK=000, 4.096MHz~6.144MHz: set 0
						* When TBCLK=001, 2.048MHz~3.072MHz: set 0
						* When others set1
						*/
#define IT6621_TX_XP_DEI_SEL		BIT(3)
#define IT6621_TX_XP_DEI_DIS		(0x00 << 3)
#define IT6621_TX_XP_DEI_EN		(0x01 << 3)
#define IT6621_TX_XP_ER0_SEL		BIT(4)
#define IT6621_TX_XP_ER0_DIS		(0x00 << 4)
#define IT6621_TX_XP_ER0_EN		(0x01 << 4)
#define IT6621_TX_AUTO_XP_GAIN_SEL	BIT(6) /* 1: XP_GAIN auto mode */
#define IT6621_TX_AUTO_XP_GAIN_DIS	(0x00 << 6)
#define IT6621_TX_AUTO_XP_GAIN_EN	(0x01 << 6)
#define IT6621_TX_AUTO_XP_PDIV_SEL	BIT(7) /* 1: XP_PDIV[1:0] auto mode */
#define IT6621_TX_AUTO_XP_PDIV_DIS	(0x00 << 7)
#define IT6621_TX_AUTO_XP_PDIV_EN	(0x01 << 7)

#define IT6621_REG_TX_AFE2		0xa2
#define IT6621_TX_DRV_DPWD		BIT(0) /* Power down the differential
						* mode driver, high active.
						* NOTE: Set 1 in ARC mode.
						*/
#define IT6621_TX_DRV_RESET_SEL		BIT(1) /* Reset the differential mode
						* driver, high active.
						* NOTE: Set 1 in ARC mode.
						*/
#define IT6621_TX_DRV_RESET_DIS		(0x00 << 1)
#define IT6621_TX_DRV_RESET_EN		(0x01 << 1)
#define IT6621_TX_DRV_DC		GENMASK(3, 2)
#define IT6621_TX_DRV_DC_DIS		(0x00 << 2)
#define IT6621_TX_DRV_DC_EN		(0x01 << 2)
#define IT6621_TX_DRV_DC_SEL_DIS	(0x00 << 3)
#define IT6621_TX_DRV_DC_SEL_EN		(0x01 << 3)
#define IT6621_TX_DRV_DSW_SEL		GENMASK(6, 4)
#define IT6621_TX_DRV_DSW_4		(0x04 << 4)
#define IT6621_TX_DRV_DSW_7		(0x07 << 4)
#define IT6621_TX_DRV_OT_SEL		BIT(7)
#define IT6621_TX_DRV_OT_DIS		(0x00 << 7)
#define IT6621_TX_DRV_OT_EN		(0x01 << 7)

#define IT6621_REG_TX_AFE3		0xa3
#define IT6621_TX_DRV_CPWD		BIT(0) /* Power down the common mode driver, high active */
#define IT6621_TX_DRV_CSR_SEL		GENMASK(3, 1) /* 0: fastest, 7: slowest */
#define IT6621_TX_DRV_CSR_2		(0x02 << 1)
#define IT6621_TX_DRV_CSR_4		(0x04 << 1)
#define IT6621_TX_DRV_CSW_SEL		GENMASK(6, 4) /* 0: minimum, 7: maximum */
#define IT6621_TX_DRV_CSW_4		(0x04 << 4)
#define IT6621_TX_DRV_CSW_7		(0x07 << 4)
#define IT6621_TX_IN_ARC_MODE_SEL	BIT(7) /* In ARC mode */
#define IT6621_TX_IN_ARC_SIGNAL_MODE	(0x00 << 7) /* signal mode */
#define IT6621_TX_IN_ARC_COMMON_MODE	(0x01 << 7) /* common mode */

#define IT6621_REG_TX_AFE4		0xa4
#define IT6621_TX_RC_PWD		BIT(0) /* Power down common receiver,
						* high active.
						* NOTE: Set 1 in ARC mode.
						*/
#define IT6621_TX_RC_CK_SEL		BIT(1)
#define IT6621_TX_RC_CK_DIS		(0x00 << 1)
#define IT6621_TX_RC_CK_EN		(0x01 << 1)
#define IT6621_TX_VCM_SEL		GENMASK(3, 2)
#define IT6621_TX_VCM0			(0x00 << 2)
#define IT6621_TX_VCM1			(0x01 << 2)
#define IT6621_TX_VCM2			(0x02 << 2)
#define IT6621_TX_VCM3			(0x03 << 2)
#define IT6621_TX_HYS_SEL		GENMASK(6, 4)
#define IT6621_TX_HYS5			(0x05 << 4)
#define IT6621_TX_HYS6			(0x06 << 4)

#define IT6621_REG_TX_AFE5		0xa5
#define IT6621_TX_LC_PWD		BIT(4) /* PWD=0, LCVCO is at power down */
#define IT6621_TX_LC_ICTP_PWD		BIT(5) /* ICTP_PWD=0, ICTP block is at power down */
#define IT6621_TX_LC_RESET		BIT(6) /* RESET=0, LCVCO is at reset */

#define IT6621_REG_TX_AFE15		0xaf
#define IT6621_TX_AUTO_DPWD_SEL		BIT(0) /* 1: enable auto power down
						* differential mode TX AFE
						*/
#define IT6621_TX_AUTO_DPWD_DIS		(0x00 << 0)
#define IT6621_TX_AUTO_DPWD_EN		(0x01 << 0)
#define IT6621_TX_AUTO_DRST_SEL		BIT(1) /* 1: enable auto reset differential mode TX AFE */
#define IT6621_TX_AUTO_DRST_DIS		(0x00 << 1)
#define IT6621_TX_AUTO_DRST_EN		(0x01 << 1)
#define IT6621_TX_AUTO_CPWD_SEL		BIT(2) /* 1: enable auto power down common mode TX AFE */
#define IT6621_TX_AUTO_CPWD_DIS		(0x00 << 2)
#define IT6621_TX_AUTO_CPWD_EN		(0x01 << 2)

/* eARC Clock Detection Registers */
#define IT6621_REG_CLK_DET0		0xc0
#define IT6621_ACLK_BND_NUM_LOW		GENMASK(7, 0) /* ACLK frequency boundary
						       * for XP_PDIV auto setting.
						       * 128T@3.584MHz count by 10MHz RCLK.
						       */

#define IT6621_REG_CLK_DET1		0xc1
#define IT6621_ACLK_BND_NUM_HIGH	BIT(0)

#define IT6621_REG_CLK_DET2		0xc2
#define IT6621_ACLK_VALID_NUM_LOW	GENMASK(7, 0) /* ACLK frequency valid for
						       * XP_PDIV auto setting.
						       * 128T@2MHz count by 10MHz RCLK.
						       */

#define IT6621_REG_CLK_DET3		0xc3
#define IT6621_ACLK_VALID_NUM_HIGH	GENMASK(1, 0)

#define IT6621_REG_CLK_DET4		0xc4
#define IT6621_BCLK_BND_NUM		GENMASK(7, 0) /* BCLK frequency boundary
						       * for FSEL auto setting.
						       * 128T@6.25MHz count by 10MHz RCLK.
						       */

#define IT6621_REG_CLK_DET5		0xc5
#define IT6621_BCLK_VALID_NUM_LOW	GENMASK(7, 0) /* BCLK frequency valid for
						       * FSEL auto setting.
						       * 128T@4MHz count by 10MHz RCLK.
						       */

#define IT6621_REG_CLK_DET6		0xc6
#define IT6621_BCLK_VALID_NUM_HIGH	BIT(0)

#define IT6621_REG_CLK_DET7		0xc7
#define IT6621_ACLK_DET_EN_SEL		BIT(0) /* enable ACLK clock detection */
#define IT6621_ACLK_DET_DIS		(0x00 << 0)
#define IT6621_ACLK_DET_EN		(0x01 << 0)
#define IT6621_BCLK_DET_EN_SEL		BIT(1) /* 1: enable BCLK clock detection */
#define IT6621_UPDATE_AVG_EN_SEL	BIT(4) /* enable update average clock detection value */
#define IT6621_UPDATE_AVG_DIS		(0x00 << 4)
#define IT6621_UPDATE_AVG_EN		(0x01 << 4)
#define IT6621_CLK_DIFF_MIN_EN		BIT(5)
#define IT6621_CLK_ORG_DELTA_EN		(0x00 << 5) /* 0: Original delta */
#define IT6621_CLK_FIX_MIN_DELTA_EN	(0x01 << 5) /* 1: Fix minimum delta */
#define IT6621_RCLK_DELTA_SEL		GENMASK(7, 6) /* RCLK deviation selection */
#define IT6621_RCLK_DELTA1		(0x00 << 6) /* +/-1% */
#define IT6621_RCLK_DELTA2		(0x01 << 6) /* +/-2% */
#define IT6621_RCLK_DELTA3		(0x02 << 6) /* +/-3% */
#define IT6621_RCLK_DELTA5		(0x03 << 6) /* +/-5% */

#define IT6621_REG_CLK_DET8		0xc8
#define IT6621_DET_ACLK_AVG_LOW		GENMASK(7, 0) /* ACLK frequency detection */

#define IT6621_REG_CLK_DET9		0xc9
#define IT6621_DET_ACLK_AVG_HIGH	GENMASK(3, 0)
#define IT6621_DET_ACLK_PRE_DIV_2	BIT(4) /* 1: ACLK is pre-divided by 2 */
#define IT6621_DET_ACLK_PRE_DIV_4	BIT(5) /* 1: ACLK is pre-divided by 4 */
#define IT6621_DET_ACLK_FREQ_VALID	BIT(6) /* 1: ACLK frequency is valid */
#define IT6621_DET_ACLK_FREQ_STB	BIT(7) /* 1: ACLK frequency detection */

#define IT6621_REG_CLK_DET10		0xca
#define IT6621_DET_BCLK_AVG_LOW		GENMASK(7, 0) /* BCLK frequency detection */

#define IT6621_REG_CLK_DET11		0xcb
#define IT6621_DET_BCLK_AVG_HIGH	GENMASK(2, 0)
#define IT6621_DET_BCLK_PRE_DIV_2	BIT(4) /* 1: BCLK is pre-divided by 2 */
#define IT6621_DET_BCLK_PRE_DIV_4	BIT(5) /* 1: BCLK is pre-divided by 4 */
#define IT6621_DET_BCLK_FREQ_VALID	BIT(6) /* 1: BCLK frequency is valid */
#define IT6621_DET_BCLK_FREQ_STB	BIT(7) /* 1: BCLK frequency detection */

/* GPIO Control Registers */
#define IT6621_REG_GPIO_CTRL0		0xe8
#define IT6621_I2S0_GPIO_MODE_EN	BIT(0) /* 1: enable I2S0 GPIO mode */
#define IT6621_I2S0_GPIO_OUTPUT_EN	BIT(1) /* 1: enable I2S0 output enable */
#define IT6621_I2S0_GPIO_OUTPUT_VAL	BIT(2) /* I2S0 GPIO output value */
#define IT6621_I2S0_GPIO_INPUT_VAL	BIT(3) /* I2S0 GPIO input value */
#define IT6621_I2S1_GPIO_MODE_EN	BIT(4) /* 1: enable I2S1 GPIO mode */
#define IT6621_I2S1_GPIO_OUTPUT_EN	BIT(5) /* 1: enable I2S1 output enable */
#define IT6621_I2S1_GPIO_OUTPUT_VAL	BIT(6) /* I2S1 GPIO output value */
#define IT6621_I2S1_GPIO_INPUT_VAL	BIT(7) /* I2S1 GPIO input value */

#define IT6621_REG_GPIO_CTRL1		0xe9
#define IT6621_I2S2_GPIO_MODE_EN	BIT(0) /* 1: enable I2S2 GPIO mode */
#define IT6621_I2S2_GPIO_OUTPUT_EN	BIT(1) /* 1: enable I2S2 output enable */
#define IT6621_I2S2_GPIO_OUTPUT_VAL	BIT(2) /* I2S2 GPIO output value */
#define IT6621_I2S2_GPIO_INPUT_VAL	BIT(3) /* I2S2 GPIO input value */
#define IT6621_I2S3_GPIO_MODE_EN	BIT(4) /* 1: enable I2S3 GPIO mode */
#define IT6621_I2S3_GPIO_OUTPUT_EN	BIT(5) /* 1: enable I2S3 output enable */
#define IT6621_I2S3_GPIO_OUTPUT_VAL	BIT(6) /* I2S3 GPIO output value */
#define IT6621_I2S3_GPIO_INPUT_VAL	BIT(7) /* I2S3 GPIO input value */

#define IT6621_REG_GPIO_CTRL2		0xea
#define IT6621_I2S4_GPIO_MODE_EN	BIT(0) /* 1: enable I2S4 GPIO mode */
#define IT6621_I2S4_GPIO_OUTPUT_EN	BIT(1) /* 1: enable I2S4 output enable */
#define IT6621_I2S4_GPIO_OUTPUT_VAL	BIT(2) /* I2S4 GPIO output value */
#define IT6621_I2S4_GPIO_INPUT_VAL	BIT(3) /* I2S4 GPIO input value */
#define IT6621_I2S5_GPIO_MODE_EN	BIT(4) /* 1: enable I2S5 GPIO mode */
#define IT6621_I2S5_GPIO_OUTPUT_EN	BIT(5) /* 1: enable I2S5 output enable */
#define IT6621_I2S5_GPIO_OUTPUT_VAL	BIT(6) /* I2S5 GPIO output value */
#define IT6621_I2S5_GPIO_INPUT_VAL	BIT(7) /* I2S5 GPIO input value */

#define IT6621_REG_GPIO_CTRL3		0xeb
#define IT6621_I2S6_GPIO_MODE_EN	BIT(0) /* 1: enable I2S6 GPIO mode */
#define IT6621_I2S6_GPIO_OUTPUT_EN	BIT(1) /* 1: enable I2S6 output enable */
#define IT6621_I2S6_GPIO_OUTPUT_VAL	BIT(2) /* I2S6 GPIO output value */
#define IT6621_I2S6_GPIO_INPUT_VAL	BIT(3) /* I2S6 GPIO input value */
#define IT6621_I2S7_GPIO_MODE_EN	BIT(4) /* 1: enable I2S7 GPIO mode */
#define IT6621_I2S7_GPIO_OUTPUT_EN	BIT(5) /* 1: enable I2S7 output enable */
#define IT6621_I2S7_GPIO_OUTPUT_VAL	BIT(6) /* I2S7 GPIO output value */
#define IT6621_I2S7_GPIO_INPUT_VAL	BIT(7) /* I2S7 GPIO input value */

#define IT6621_REG_GPIO_CTRL4		0xec
#define IT6621_MCLK_GPIO_MODE_EN	BIT(0) /* 1: enable MCLK GPIO mode */
#define IT6621_MCLK_GPIO_OUTPUT_EN	BIT(1) /* 1: enable MCLK output enable */
#define IT6621_MCLK_GPIO_OUTPUT_VAL	BIT(2) /* MCLK GPIO output value */
#define IT6621_MCLK_GPIO_INPUT_VAL	BIT(3) /* MCLK GPIO input value */
#define IT6621_SPDIF_GPIO_MODE_EN	BIT(4) /* 1: enable SPDIF GPIO mode */
#define IT6621_SPDIF_GPIO_OUTPUT_EN	BIT(5) /* 1: enable SPDIF output enable */
#define IT6621_SPDIF_GPIO_OUTPUT_VAL	BIT(6) /* SPDIF GPIO output value */
#define IT6621_SPDIF_GPIO_INPUT_VAL	BIT(7) /* SPDIF GPIO input value */

#define IT6621_REG_GPIO_CTRL5		0xed
#define IT6621_MUTE_GPIO_MODE_EN	BIT(0) /* 1: enable MUTE GPIO mode */
#define IT6621_MUTE_GPIO_OUTPUT_EN	BIT(1) /* 1: enable MUTE output enable */
#define IT6621_MUTE_GPIO_OUTPUT_VAL	BIT(2) /* MUTE GPIO output value */
#define IT6621_MUTE_GPIO_INPUT_VAL	BIT(3) /* MUTE GPIO input value */
#define IT6621_HPDB_GPIO_MODE_EN	BIT(4) /* 1: enable HPDB GPIO mode */
#define IT6621_HPDB_GPIO_OUTPUT_EN	BIT(5) /* 1: enable HPDB output enable */
#define IT6621_HPDB_GPIO_OUTPUT_VAL	BIT(6) /* HPDB GPIO output value */
#define IT6621_HPDB_GPIO_INPUT_VAL	BIT(7) /* HPDB GPIO input value */

/* Misc Registers */
#define IT6621_REG_MISC0		0xf0
#define IT6621_DEBUG_SEL		GENMASK(2, 0) /* Select one of the 8 debug groups */
#define IT6621_DEBUG_EN			BIT(3) /* 1: enable debug output */
#define IT6621_AUD_DEBUG_EN		BIT(5) /* 1: Enable audio signal debug output */

#define IT6621_REG_MISC1		0xf1
#define IT6621_HPD_DG_TIME_SEL		GENMASK(1, 0) /* HPD de-glitch time selection */
#define IT6621_HPD_DG_TIME_10US		(0x00 << 0) /* 10us */
#define IT6621_HPD_DG_TIME_100US	(0x01 << 0) /* 100us */
#define IT6621_HPD_DG_TIME_1MS		(0x02 << 0) /* 1ms */
#define IT6621_HPD_DG_TIME_10MS		(0x03 << 0) /* 10ms */
#define IT6621_CMD_HOLD_TIME		GENMASK(7, 4) /* PCI2C hold time */

#define IT6621_REG_MISC2		0xf2
#define IT6621_CMD_DRV			GENMASK(1, 0) /* Driving setting for PCSCL and PCSDA */
#define IT6621_HPD_DRV			GENMASK(3, 2) /* Driving setting for HPD signals */
#define IT6621_I2S_DRV			GENMASK(5, 4) /* Driving setting for I2S signals */
#define IT6621_SPDIF_DRV		GENMASK(7, 6) /* Driving setting for SPDIF signals */

#define IT6621_REG_MISC3		0xf3
#define IT6621_CMD_SMT			BIT(0) /* PCSCL/PCSDA Schmitt trigger option */
#define IT6621_SCK_SMT			BIT(2) /* SCK/DCLK Schmitt trigger option */
#define IT6621_MCLK_SMT			BIT(3) /* MCLK Schmitt trigger option */
#define IT6621_SPDIFSMT			BIT(4) /* SPDIF Schmitt trigger option */

#define IT6621_REG_MISC12		0xfc
#define IT6621_CRCLK_SEL_EN		BIT(0)
#define IT6621_PWD_CRCLK		(0x00 << 0) /* power-down CRCLK and disable CEC function */
#define IT6621_CRCLK_EN			(0x01 << 0) /* enable CRCLK for CEC function */
#define IT6621_CEC_SLAVE_ADDR		GENMASK(7, 1) /* CEC slave address */

#define IT6621_REG_PASS_WORD		0xff /* 0xC3/0xA5 on, 0xFF off */
#define IT6621_PASS_WORD_ON1		0xc3
#define IT6621_PASS_WORD_ON2		0xa5
#define IT6621_PASS_WORD_OFF		0xff

#endif /* _IT6621_REG_BANK0_H */
