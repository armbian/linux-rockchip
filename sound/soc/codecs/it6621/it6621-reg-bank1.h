/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#ifndef _IT6621_REG_BANK1_H
#define _IT6621_REG_BANK1_H

/* Bank 1 */

/* General Registers */
#define IT6621_REG_GEN0			0x110
#define IT6621_100MS_CNT_SEL		BIT(7) /* Enable 100ms calibration counter */
#define IT6621_100MS_CNT_DIS		(0x00 << 7)
#define IT6621_100MS_CNT_EN		(0x01 << 7)

#define IT6621_REG_100MS_CNT_7_0	0x111 /* 100ms counter value */
#define IT6621_REG_100MS_CNT_15_8	0x112
#define IT6621_REG_100MS_CNT_23_16	0x113

#define IT6621_REG_GEN4			0x114
#define IT6621_1US_TIME_INT		GENMASK(5, 0) /* 1us time base integer number.
						       * Note: default value is
						       * calculated by 10MHz RCLK.
						       */

#define IT6621_REG_1US_TIME_FLT		0x115 /* 1us time base floating number */

#define IT6621_REG_GEN6			0x116
#define IT6621_TIMER_INT_NUM		GENMASK(6, 0) /* User defined timer
						       * interrupt count number
						       */
#define IT6621_TIMER_INT_UNIT		BIT(7)
#define IT6621_TIMER_INT_UNIT_10MS	(0x00 << 7) /* unit=10ms */
#define IT6621_TIMER_INT_UNIT_100MS	(0x01 << 7) /* unit=100ms */

#define IT6621_REG_GEN7			0x117
#define IT6621_TIME_STAMP_EN_SEL	GENMASK(6, 0) /* Time stamp for firmware */
#define IT6621_TIME_STAMP_EN		BIT(7) /* 1: Enable time stamp */

/* Input SPDIF Channel Status Registers */
#define IT6621_REG_AI_TX_CH_ST_7_0	0x120
#define IT6621_REG_AI_TX_CH_ST_15_8	0x121
#define IT6621_REG_AI_TX_CH_ST_23_16	0x122
#define IT6621_REG_AI_TX_CH_ST_31_24	0x123
#define IT6621_REG_AI_TX_CH_ST_39_32	0x124

/* eARC TX Channel Status Registers */
#define IT6621_REG_TX_CH_ST0		0x130
#define IT6621_REG_TX_CH_ST1		0x131
#define IT6621_REG_TX_CH_ST2		0x132
#define IT6621_REG_TX_CH_ST3		0x133
#define IT6621_REG_TX_CH_ST4		0x134
#define IT6621_REG_TX_CH_ST5		0x135
#define IT6621_REG_TX_CH_ST9		0x139
#define IT6621_REG_TX_CH_ST10		0x13a
#define IT6621_REG_TX_CH_ST11		0x13b
#define IT6621_REG_TX_CH_ST12		0x13c
#define IT6621_REG_TX_CH_ST13		0x13d
#define IT6621_REG_TX_CH_ST14		0x13e
#define IT6621_REG_TX_CH_ST15		0x13f

/* eARC TX U-bit Packet Registers */

/* Packet Type 1 */
#define IT6621_REG_TX_PKT1_CRC		0x150 /* eARC TX packet CRC */
#define IT6621_REG_TX_PKT1_CH		0x151 /* eARC TX packet CH */
#define IT6621_REG_TX_PKT1_HB0		0x152 /* eARC TX packet HB0 */
#define IT6621_REG_TX_PKT1_HB1		0x153 /* eARC TX packet HB1 */
#define IT6621_REG_TX_PKT1_HB2		0x154 /* eARC TX packet HB2 */
#define IT6621_REG_TX_PKT1_PB0		0x155 /* eARC TX packet PB0 */
#define IT6621_REG_TX_PKT1_PB1		0x156 /* eARC TX packet PB1 */
#define IT6621_REG_TX_PKT1_PB2		0x157 /* eARC TX packet PB2 */
#define IT6621_REG_TX_PKT1_PB3		0x158 /* eARC TX packet PB3 */
#define IT6621_REG_TX_PKT1_PB4		0x159 /* eARC TX packet PB4 */
#define IT6621_REG_TX_PKT1_PB5		0x15a /* eARC TX packet PB5 */
#define IT6621_REG_TX_PKT1_PB6		0x15b /* eARC TX packet PB6 */
#define IT6621_REG_TX_PKT1_PB7		0x15c /* eARC TX packet PB7 */
#define IT6621_REG_TX_PKT1_PB8		0x15d /* eARC TX packet PB8 */
#define IT6621_REG_TX_PKT1_PB9		0x15e /* eARC TX packet PB9 */
#define IT6621_REG_TX_PKT1_PB10		0x15f /* eARC TX packet PB10 */
#define IT6621_REG_TX_PKT1_PB11		0x160 /* eARC TX packet PB11 */
#define IT6621_REG_TX_PKT1_PB12		0x161 /* eARC TX packet PB12 */
#define IT6621_REG_TX_PKT1_PB13		0x162 /* eARC TX packet PB13 */
#define IT6621_REG_TX_PKT1_PB14		0x163 /* eARC TX packet PB14 */
#define IT6621_REG_TX_PKT1_PB15		0x164 /* eARC TX packet PB15 */
#define IT6621_REG_TX_PKT1_PB16		0x165 /* eARC TX packet PB16 */
#define IT6621_REG_TX_PKT1_PB17		0x166 /* eARC TX packet PB17 */
#define IT6621_REG_TX_PKT1_PB18		0x167 /* eARC TX packet PB18 */
#define IT6621_REG_TX_PKT1_PB19		0x168 /* eARC TX packet PB19 */
#define IT6621_REG_TX_PKT1_PB20		0x169 /* eARC TX packet PB20 */
#define IT6621_REG_TX_PKT1_PB21		0x16a /* eARC TX packet PB21 */
#define IT6621_REG_TX_PKT1_PB22		0x16b /* eARC TX packet PB22 */
#define IT6621_REG_TX_PKT1_PB23		0x16c /* eARC TX packet PB23 */
#define IT6621_REG_TX_PKT1_PB24		0x16d /* eARC TX packet PB24 */
#define IT6621_REG_TX_PKT1_PB25		0x16e /* eARC TX packet PB25 */
#define IT6621_REG_TX_PKT1_PB26		0x16f /* eARC TX packet PB26 */
#define IT6621_REG_TX_PKT1_PB27		0x170 /* eARC TX packet PB27 */

/* Packet Type 2 */
#define IT6621_REG_TX_PKT2_CRC		0x171 /* eARC TX packet CRC */
#define IT6621_REG_TX_PKT2_CH		0x172 /* eARC TX packet CH */
#define IT6621_REG_TX_PKT2_HB0		0x173 /* eARC TX packet HB0 */
#define IT6621_REG_TX_PKT2_HB1		0x174 /* eARC TX packet HB1 */
#define IT6621_REG_TX_PKT2_HB2		0x175 /* eARC TX packet HB2 */
#define IT6621_REG_TX_PKT2_PB0		0x176 /* eARC TX packet PB0 */
#define IT6621_REG_TX_PKT2_PB1		0x177 /* eARC TX packet PB1 */
#define IT6621_REG_TX_PKT2_PB2		0x178 /* eARC TX packet PB2 */
#define IT6621_REG_TX_PKT2_PB3		0x179 /* eARC TX packet PB3 */
#define IT6621_REG_TX_PKT2_PB4		0x17a /* eARC TX packet PB4 */
#define IT6621_REG_TX_PKT2_PB5		0x17b /* eARC TX packet PB5 */
#define IT6621_REG_TX_PKT2_PB6		0x17c /* eARC TX packet PB6 */
#define IT6621_REG_TX_PKT2_PB7		0x17d /* eARC TX packet PB7 */
#define IT6621_REG_TX_PKT2_PB8		0x17e /* eARC TX packet PB8 */
#define IT6621_REG_TX_PKT2_PB9		0x17f /* eARC TX packet PB9 */
#define IT6621_REG_TX_PKT2_PB10		0x180 /* eARC TX packet PB10 */
#define IT6621_REG_TX_PKT2_PB11		0x181 /* eARC TX packet PB11 */
#define IT6621_REG_TX_PKT2_PB12		0x182 /* eARC TX packet PB12 */
#define IT6621_REG_TX_PKT2_PB13		0x183 /* eARC TX packet PB13 */
#define IT6621_REG_TX_PKT2_PB14		0x184 /* eARC TX packet PB14 */
#define IT6621_REG_TX_PKT2_PB15		0x185 /* eARC TX packet PB15 */

/* Packet Type 3 */
#define IT6621_REG_TX_PKT3_CRC		0x186 /* eARC TX packet CRC */
#define IT6621_REG_TX_PKT3_CH		0x187 /* eARC TX packet CH */
#define IT6621_REG_TX_PKT3_HB0		0x188 /* eARC TX packet HB0 */
#define IT6621_REG_TX_PKT3_HB1		0x189 /* eARC TX packet HB1 */
#define IT6621_REG_TX_PKT3_HB2		0x18a /* eARC TX packet HB2 */
#define IT6621_REG_TX_PKT3_PB0		0x18b /* eARC TX packet PB0 */
#define IT6621_REG_TX_PKT3_PB1		0x18c /* eARC TX packet PB1 */
#define IT6621_REG_TX_PKT3_PB2		0x18d /* eARC TX packet PB2 */
#define IT6621_REG_TX_PKT3_PB3		0x18e /* eARC TX packet PB3 */
#define IT6621_REG_TX_PKT3_PB4		0x18f /* eARC TX packet PB4 */
#define IT6621_REG_TX_PKT3_PB5		0x190 /* eARC TX packet PB5 */
#define IT6621_REG_TX_PKT3_PB6		0x191 /* eARC TX packet PB6 */
#define IT6621_REG_TX_PKT3_PB7		0x192 /* eARC TX packet PB7 */
#define IT6621_REG_TX_PKT3_PB8		0x193 /* eARC TX packet PB8 */
#define IT6621_REG_TX_PKT3_PB9		0x194 /* eARC TX packet PB9 */
#define IT6621_REG_TX_PKT3_PB10		0x195 /* eARC TX packet PB10 */
#define IT6621_REG_TX_PKT3_PB11		0x196 /* eARC TX packet PB11 */
#define IT6621_REG_TX_PKT3_PB12		0x197 /* eARC TX packet PB12 */
#define IT6621_REG_TX_PKT3_PB13		0x198 /* eARC TX packet PB13 */
#define IT6621_REG_TX_PKT3_PB14		0x199 /* eARC TX packet PB14 */
#define IT6621_REG_TX_PKT3_PB15		0x19a /* eARC TX packet PB15 */

#endif /* _IT6621_REG_BANK1_H */
