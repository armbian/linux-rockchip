/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#ifndef _IT6621_REG_CEC_H
#define _IT6621_REG_CEC_H

/* CEC Control Registers */
#define IT6621_REG_CEC_CTRL6		0x06 /* 0: Interrupt Enable, 1: Interrupt Mask */
#define IT6621_TX_INT_MASK		BIT(0)
#define IT6621_RX_INT_MASK		BIT(1)
#define IT6621_RX_FAIL_INT_MASK		BIT(2)
#define IT6621_TX_DONE_INT_MASK		BIT(3)
#define IT6621_RX_DONE_INT_MASK		BIT(4)
#define IT6621_TX_FAIL_INT_MASK		BIT(5)
#define IT6621_CMD_OVERFLOW_INT_MASK	BIT(6)
#define IT6621_DATA_OVERFLOW_INT_MASK	BIT(7)

#define IT6621_REG_CEC_CTRL7		0x07
#define IT6621_DBG_CEC_SEL		GENMASK(4, 2)
#define IT6621_CEC_INT_STS_SEL		BIT(5)

#define IT6621_REG_CEC_CTRL8		0x08
#define IT6621_CEC_INT_SEL		BIT(0) /* CEC interrupt enable,
						* 1: Enable, 0: Disable
						*/
#define IT6621_CEC_INT_DIS		(0x00 << 0)
#define IT6621_CEC_INT_EN		(0x01 << 0)
#define IT6621_CEC_RESET_SEL		BIT(2) /* Reset CEC block,
						* 1: Enable, 0: Disable
						*/
#define IT6621_CEC_RESET_DIS		(0x00 << 2)
#define IT6621_CEC_RESET_EN		(0x01 << 2)
#define IT6621_CEC_SMT			BIT(3) /* Schmitt trigger of CEC IO,
						* 1: Enable, 0: Disable
						*/
#define IT6621_CEC_FORCE		BIT(4) /* Force CEC output regardless of normal function */
#define IT6621_CEC_OE			BIT(5) /* Force CEC output value */
#define IT6621_DBG_CEC_CLR		BIT(6) /* For debug only */
#define IT6621_FIRE_FRAME		BIT(7) /* 1: Fire CEC command out */

#define IT6621_REG_CEC_CTRL9		0x09
#define IT6621_EN_100MS_CNT		BIT(0) /* Used as a reference 100ms
						* time interval for CEC calibration
						*/
#define IT6621_NACK_SEL			BIT(1) /* Acknowledge from follower to initiator */
#define IT6621_ACK_EN			(0x00 << 1) /* ACK */
#define IT6621_NACK_EN			(0x01 << 1) /* NACK */
#define IT6621_PULSE_SEL		BIT(2) /* Select illegal bit as error bit */
#define IT6621_PULSE_DIS		(0x00 << 2) /* Disable */
#define IT6621_PULSE_EN			(0x01 << 2) /* Enable */
#define IT6621_ACK_TRIG_SEL		BIT(3) /* Acknowledge for broadcast mode */
#define IT6621_ACK_TRIG_NACK		(0x00 << 3) /* NACK */
#define IT6621_ACK_TRIG_ACK		(0x01 << 3) /* ACK */
#define IT6621_REFIRE_SEL		BIT(4) /* Retry to fire CEC command */
#define IT6621_REFIRE_DIS		(0x00 << 4) /* Disable */
#define IT6621_REFIRE_EN		(0x01 << 4) /* Enable */
#define IT6621_RX_SELF_SEL		BIT(5) /* Initiator received CEC bus data */
#define IT6621_RX_SELF_EN		(0x00 << 5) /* Enable */
#define IT6621_RX_SELF_DIS		(0x01 << 5) /* Disable */
#define IT6621_REGION_SEL		BIT(6) /* Select region for error bit */
#define IT6621_REGION_START_TO_IDLE	(0x00 << 6) /* Region form state Start to IDLE */
#define IT6621_REGION_WHOLE		(0x01 << 6) /* Whole region */
#define IT6621_DATA_BIT_SEL		BIT(7) /* Select data bit */
#define IT6621_DATA_BIT_NORMAL		(0x00 << 7) /* Normal */
#define IT6621_DATA_BIT_INC_0P1MS	(0x01 << 7) /* Increase 0.1ms */

#define IT6621_REG_CEC_CTRL10		0x0a
#define IT6621_BUS_FREE_SEL		GENMASK(1, 0) /* Select bus free bit time */
#define IT6621_BUS_FREE_AUTO		(0x00 << 0) /* Automatic */
#define IT6621_BUS_FREE_3BIT_TIME	(0x01 << 0) /* 3 bit time */
#define IT6621_BUS_FREE_5BIT_TIME	(0x02 << 0) /* 5 bit time */
#define IT6621_BUS_FREE_7BIT_TIME	(0x03 << 0) /* 7 bit time */
#define IT6621_BIT_END_SEL		GENMASK(3, 2) /* Select logic 0 and
						       * logic 1 output bit time
						       */
#define IT6621_BIT_END_STD		(0x00 << 2) /* Standard */
#define IT6621_BIT_END_MAX0_MIN1	(0x01 << 2) /* Logic 0 maximum and logic 1 minimum */
#define IT6621_BIT_END_MAX1_MIN0	(0x02 << 2) /* Logic 1 maximum and logic 0 minimum */
#define IT6621_AR_BIT_SEL		BIT(4) /* Select bit time for arbitration lose */
#define IT6621_AR_BIT_5BIT_TIME		(0x00 << 4) /* 5 bit time */
#define IT6621_AR_BIT_3BIT_TIME		(0x01 << 4) /* 3 bit time */
#define IT6621_HW_RTY_SEL		BIT(5) /* HW 5 times Retry of TX */
#define IT6621_HW_RTY_EN		(0x00 << 5)
#define IT6621_HW_RTY_DIS		(0x01 << 5)
#define IT6621_CEC_RX_EN_SEL		BIT(6) /* CEC RX function */
#define IT6621_CEC_RX_DIS		(0x00 << 6)
#define IT6621_CEC_RX_EN		(0x01 << 6)

#define IT6621_REG_DATA_MIN		0x0b /* Minimum data bit time */
#define IT6621_REG_TIMER_UNIT		0x0c /* CEC timer unit, nominally
					      * 100us. This value should be
					      * decided from MS_Count.
					      */

#define IT6621_REG_CEC_CTRL13		0x0d
#define IT6621_CEC_IO_PU_SEL		BIT(4)
#define IT6621_CEC_IO_NORMAL		(0x00 << 4) /* Normal */
#define IT6621_CEC_IO_PU		(0x01 << 4) /* Pull-up */
#define IT6621_CEC_IO_SR		BIT(5) /* CEC IO slew rate control */
#define IT6621_CEC_IO_DRV		GENMASK(7, 6)
#define IT6621_CEC_IO_DRV_2P5MA		(0x00 << 6) /* 2.5mA */
#define IT6621_CEC_IO_DRV_5MA		(0x01 << 6) /* 5mA */
#define IT6621_CEC_IO_DRV_7P5MA		(0x02 << 6) /* 7.5mA */
#define IT6621_CEC_IO_DRV_10MA		(0x03 << 6) /* 10mA */

/* CEC Initiator Registers */
#define IT6621_REG_TX_HEADER		0x10 /* CEC initiator command header */
#define IT6621_REG_TX_OPCODE		0x11 /* CEC initiator command Opcode */
#define IT6621_REG_TX_OPERAND1		0x12 /* CEC initiator command Operand1 */
#define IT6621_REG_TX_OPERAND2		0x13 /* CEC initiator command Operand2 */
#define IT6621_REG_TX_OPERAND3		0x14 /* CEC initiator command Operand3 */
#define IT6621_REG_TX_OPERAND4		0x15 /* CEC initiator command Operand4 */
#define IT6621_REG_TX_OPERAND5		0x16 /* CEC initiator command Operand5 */
#define IT6621_REG_TX_OPERAND6		0x17 /* CEC initiator command Operand6 */
#define IT6621_REG_TX_OPERAND7		0x18 /* CEC initiator command Operand7 */
#define IT6621_REG_TX_OPERAND8		0x19 /* CEC initiator command Operand8 */
#define IT6621_REG_TX_OPERAND9		0x1a /* CEC initiator command Operand9 */
#define IT6621_REG_TX_OPERAND10		0x1b /* CEC initiator command Operand10 */
#define IT6621_REG_TX_OPERAND11		0x1c /* CEC initiator command Operand11 */
#define IT6621_REG_TX_OPERAND12		0x1d /* CEC initiator command Operand12 */
#define IT6621_REG_TX_OPERAND13		0x1e /* CEC initiator command Operand13 */
#define IT6621_REG_TX_OPERAND14		0x1f /* CEC initiator command Operand14 */
#define IT6621_REG_TX_OPERAND15		0x20 /* CEC initiator command Operand15 */
#define IT6621_REG_TX_OPERAND16		0x21 /* CEC initiator command Operand16 */

#define IT6621_REG_CEC_INIT22		0x22
#define IT6621_LOG_ADDR			GENMASK(3, 0) /* CEC target logical address */

#define IT6621_REG_CEC_INIT23		0x23
#define IT6621_OUT_NUM			GENMASK(4, 0) /* CEC output byte size in a frame */

/* CEC Follower Registers */
#define IT6621_REG_RX_HEADER		0x30 /* CEC Follower command header */
#define IT6621_REG_RX_OPCODE		0x31 /* CEC Follower command Opcode */
#define IT6621_REG_RX_OPERAND1		0x32 /* CEC Follower command Operand1 */
#define IT6621_REG_RX_OPERAND2		0x33 /* CEC Follower command Operand2 */
#define IT6621_REG_RX_OPERAND3		0x34 /* CEC Follower command Operand3 */
#define IT6621_REG_RX_OPERAND4		0x35 /* CEC Follower command Operand4 */
#define IT6621_REG_RX_OPERAND5		0x36 /* CEC Follower command Operand5 */
#define IT6621_REG_RX_OPERAND6		0x37 /* CEC Follower command Operand6 */
#define IT6621_REG_RX_OPERAND7		0x38 /* CEC Follower command Operand7 */
#define IT6621_REG_RX_OPERAND8		0x39 /* CEC Follower command Operand8 */
#define IT6621_REG_RX_OPERAND9		0x3a /* CEC Follower command Operand9 */
#define IT6621_REG_RX_OPERAND10		0x3b /* CEC Follower command Operand10 */
#define IT6621_REG_RX_OPERAND11		0x3c /* CEC Follower command Operand11 */
#define IT6621_REG_RX_OPERAND12		0x3d /* CEC Follower command Operand12 */
#define IT6621_REG_RX_OPERAND13		0x3e /* CEC Follower command Operand13 */
#define IT6621_REG_RX_OPERAND14		0x3f /* CEC Follower command Operand14 */
#define IT6621_REG_RX_OPERAND15		0x40 /* CEC Follower command Operand15 */
#define IT6621_REG_RX_OPERAND16		0x41 /* CEC Follower command Operand16 */

#define IT6621_REG_CEC_FOL22		0x42
#define IT6621_IN_CNT			GENMASK(4, 0) /* CEC follower received bytes */

/* CEC Misc. Registers */
#define IT6621_REG_CEC_MSIC0		0x43
#define IT6621_OUT_CNT			GENMASK(4, 0) /* CEC initiator output bytes */

#define IT6621_REG_CEC_MSIC1		0x44
#define IT6621_BUS_STATUS		BIT(1)
#define IT6621_BUS_STATUS_BUSY		(0x00 << 1) /* Busy */
#define IT6621_BUS_STATUS_FREE		(0x01 << 1) /* Busy */
#define IT6621_OUT_STATUS		GENMASK(3, 2) /* Output status */
#define IT6621_OUT_STATUS_RCV_ACK	(0x00 << 2) /* Received ACK */
#define IT6621_OUT_STATUS_RCV_NACK	(0x01 << 2) /* Received NACK */
#define IT6621_OUT_STATUS_RTY		(0x02 << 2) /* Retry, if no ACK, NACK
						     * or arbitration lose
						     */
#define IT6621_OUT_STATUS_FAIL		(0x03 << 2) /* Fail */
#define IT6621_ERR_STATUS		GENMASK(5, 4) /* Output status */
#define IT6621_ERR_STATUS_NO_ERR	(0x00 << 4) /* No error occurs */
#define IT6621_ERR_STATUS_MDBP		(0x01 << 4) /* Received data period < minimum
						     * data bit period
						     */
#define IT6621_ERR_STATUS_IHLP		(0x02 << 4) /* Illegal high-low period */
#define IT6621_ERR_STATUS_BOTH		(0x03 << 4) /* Both */
#define IT6621_READY_FIRE		BIT(6) /* Bus ready for firing a CEC command */

#define IT6621_REG_CEC_MS_COUNT_7_0	0x45
#define IT6621_REG_CEC_MS_COUNT_15_8	0x46
#define IT6621_REG_CEC_MS_COUNT_19_16	0x47

#define IT6621_REG_CEC_MSIC5		0x48
#define IT6621_DBG_STATE		GENMASK(3, 0) /* Debug CEC error state */
#define IT6621_DBG_INT			BIT(4) /* CEC interrupt for debug */
#define IT6621_CEC_INT			BIT(5) /* CEC interrupt status */

#define IT6621_REG_CEC_MSIC6		0x49
#define IT6621_DBG_BLOCK		GENMASK(4, 0) /* Debug CEC error block number */

#define IT6621_REG_CEC_MSIC7		0x4a
#define IT6621_DBG_BIT			GENMASK(3, 0) /* Debug CEC error bit number */

#define IT6621_REG_DBG_TIMING		0x4b /* Debug CEC error data bit time */

#define IT6621_REG_CEC_MSIC8		0x4c
#define IT6621_TX_INT			BIT(0) /* CEC initiator output byte interrupt */
#define IT6621_RX_INT			BIT(1) /* CEC follower received byte interrupt */
#define IT6621_RX_FAIL_INT		BIT(2) /* CEC received fail interrupt */
#define IT6621_TX_DONE_INT		BIT(3) /* CEC output finish interrupt */
#define IT6621_RX_DONE_INT		BIT(4) /* CEC received finish interrupt */
#define IT6621_TX_FAIL_INT		BIT(5) /* CEC initiator output fail interrupt */
#define IT6621_CMD_OVERFLOW_INT		BIT(6) /* CEC command FIFO over flow interrupt */
#define IT6621_DATA_OVERFLOW_INT	BIT(7) /* CEC data FIFO over flow interrupt */

#define IT6621_REG_RX_CMD_RX_HEADER	0x4d /* Command FIFO Rx_Header */
#define IT6621_REG_RX_CMD_RX_OPCODE	0x4e /* Command FIFO Rx_Opcode */

#define IT6621_REG_RX_CMD_BYTE2		0x4f
#define IT6621_RX_CMD_IN_CNT		GENMASK(4, 0) /* Command FIFO In_Cnt */
#define IT6621_RX_CMD_ERR_STATUS	GENMASK(6, 5) /* Command FIFO Error_Status */
#define IT6621_RX_CMD_CEC_RX_FAIL	BIT(7) /* Command FIFO CEC Rx_Fail */

#define IT6621_REG_RX_DATA		0x50 /* Data FIFO Rx_Operand */

#define IT6621_REG_CEC_MSIC12		0x51
#define IT6621_RX_CMD_VALID		BIT(0) /* Read command FIFO valid */
#define IT6621_RX_CMD_FULL		BIT(1) /* Command FIFO full */
#define IT6621_RX_CMD_WRITE_OVERFLOW	BIT(2) /* Write command FIFO over flow */
#define IT6621_RX_CMD_READ_OVERFLOW	BIT(3) /* Read command FIFO over flow */
#define IT6621_RX_DATA_VALID		BIT(4) /* Read data FIFO valid */
#define IT6621_RX_DATA_FULL		BIT(5) /* Data FIFO full */
#define IT6621_RX_DATA_WRITE_OVERFLOW	BIT(6) /* Write data FIFO over flow */
#define IT6621_RX_DATA_READ_OVERFLOW	BIT(7) /* Read data FIFO over flow */

#define IT6621_REG_CEC_MSIC13		0x52
#define IT6621_RX_CMD_STG		GENMASK(3, 0) /* Rx command FIFO stage */
#define IT6621_RX_CMD_FIFO_RST		BIT(5) /* CEC command FIFO reset 1:Reset */
#define IT6621_RX_DATA_FIFO_RST		BIT(6) /* CEC data FIFO reset 1:Reset */
#define IT6621_RX_CEC_FIFO_EN_SEL	BIT(7) /* CEC FIFO enable */
#define IT6621_RX_CEC_FIFO_DIS		(0x00 << 7) /* Disable */
#define IT6621_RX_CEC_FIFO_EN		(0x01 << 7) /* Enable */

#define IT6621_REG_CEC_MSIC14		0x53
#define IT6621_RX_DATA_STG		GENMASK(3, 0) /* Rx data FIFO stage */

#define IT6621_REG_CEC_MSIC15		0x54
#define IT6621_TX_FAIL_STATUS		GENMASK(2, 0)
#define IT6621_TX_FAIL_STATUS_RCV_ACK	BIT(0) /* receive ACK */
#define IT6621_TX_FAIL_STATUS_RCV_NACK	BIT(1) /* receive NACK */
#define IT6621_TX_FAIL_STATUS_RTY	BIT(2) /* retry */

#endif /* _IT6621_REG_CEC_H */
