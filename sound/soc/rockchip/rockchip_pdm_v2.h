/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Rockchip PDM ALSA SoC Digital Audio Interface(DAI)  driver
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef _ROCKCHIP_PDM_V2_H
#define _ROCKCHIP_PDM_V2_H

#define PDM_V2_SYSCONFIG		0x0000
#define PDM_V2_CTRL			0x0004
#define PDM_V2_FILTER_CTRL		0x0008
#define PDM_V2_FIFO_CTRL		0x000c
#define PDM_V2_DATA_VALID		0x0010
#define PDM_V2_RXFIFO_DATA		0x0014
#define PDM_V2_DATA0R			0x0018
#define PDM_V2_DATA0L			0x001c
#define PDM_V2_DATA1R			0x0020
#define PDM_V2_DATA1L			0x0024
#define PDM_V2_DATA2R			0x0028
#define PDM_V2_DATA2L			0x002c
#define PDM_V2_DATA3R			0x0030
#define PDM_V2_DATA3L			0x0034
#define PDM_V2_VERSION			0x0038
#define PDM_V2_GAIN_CTRL		0x003c
#define PDM_V2_DATA_SHIFT0		0x0040
#define PDM_V2_DATA_SHIFT1		0x0044
#define PDM_V2_FILTER_CTRL1		0x0048
#define PDM_V2_INCR_RXDR		0x0400

/* PDM_V2_SYSCONFIG */
#define PDM_V2_FILTER_GATE_MSK		(0x1 << 4)
#define PDM_V2_FILTER_GATE_EN		(0x1 << 4)
#define PDM_V2_FILTER_GATE_DIS		(0x0 << 4)
#define PDM_V2_NUM_MSK			(0x1 << 3)
#define PDM_V2_NUM_START		(0x1 << 3)
#define PDM_V2_NUM_STOP			(0x0 << 3)
#define PDM_V2_RX_MSK			(0x1 << 2)
#define PDM_V2_RX_START			(0x1 << 2)
#define PDM_V2_RX_STOP			(0x0 << 2)
#define PDM_V2_MEM_GATE_MSK		(0x1 << 1)
#define PDM_V2_MEM_GATE_EN		(0x1 << 1)
#define PDM_V2_MEM_GATE_DIS		(0x0 << 1)
#define PDM_V2_RX_CLR_MSK		(0x1 << 0)
#define PDM_V2_RX_CLR_WR		(0x1 << 0)
#define PDM_V2_RX_CLR_DONE		(0x0 << 0)

/* PDM_V2_CTRL */
#define PDM_V2_PATH0_MODE_SELECT	(0x3 << 22)
#define PDM_V2_PATH_0_1			(0x0 << 22)
#define PDM_V2_PATH_0_2			(0x1 << 22)
#define PDM_V2_PATH_0_3			(0x2 << 22)
#define PDM_V2_SPLIT_MSK		(0x1 << 21)
#define PDM_V2_SPLIT_EN			(0x1 << 21)
#define PDM_V2_SPLIT_DIS		(0x0 << 21)
#define PDM_V2_RX_PATH_SEL_SHIFT(x)	(13 + (x) * 2)
#define PDM_V2_RX_PATH_SEL_MASK(x)	(0x3 << PDM_V2_RX_PATH_SEL_SHIFT(x))
#define PDM_V2_RX_PATH_SEL(x, v)	((v) << PDM_V2_RX_PATH_SEL_SHIFT(x))
#define PDM_V2_CKP_MSK			(0x1 << 12)
#define PDM_V2_CKP_INVERTED		(0x1 << 12)
#define PDM_V2_CKP_NORMAL		(0x0 << 12)
#define PDM_V2_SJM_SEL_MSK		(0x1 << 11)
#define PDM_V2_SJM_SEL_L		(0x1 << 11)
#define PDM_V2_SJM_SEL_R		(0x0 << 11)
#define PDM_V2_PATH_MSK			(0xf << 7)
#define PDM_V2_PATH3_EN			(0x1 << 10)
#define PDM_V2_PATH2_EN			(0x1 << 9)
#define PDM_V2_PATH1_EN			(0x1 << 8)
#define PDM_V2_PATH0_EN			(0x1 << 7)
#define PDM_V2_HWT_MSK			(0x1 << 6)
#define PDM_V2_HWT_EN			(0x1 << 6)
#define PDM_V2_HWT_DIS			(0x0 << 6)
#define PDM_V2_MONO_PATH_MSK		(0x1 << 5)
#define PDM_V2_MONO_PATH_EN		(0x1 << 5)
#define PDM_V2_MONO_PATH_DIS		(0x0 << 5)
#define PDM_V2_VDW_MSK			(0x1f << 0)
#define PDM_V2_VDW(x)			((x - 1) << 0)

/* PDM_V2_FILTER_CTRL */
/* 0.375dB every step. 0: mute, 1: -65.25dB, 255: 30dB */
#define PDM_V2_HPF_V2_R_MSK		(0x1 << 19)
#define PDM_V2_HPF_V2_R_EN		(0x1 << 19)
#define PDM_V2_HPF_V2_R_DIS		(0x0 << 19)
#define PDM_V2_HPF_V2_L_MSK		(0x1 << 20)
#define PDM_V2_HPF_V2_L_EN		(0x1 << 20)
#define PDM_V2_HPF_V2_L_DIS		(0x0 << 20)
#define PDM_V2_HPF_V2_FREQ_MSK		(0xf << 21)
#define PDM_V2_HPF_V2_FREQ_0_234	(0x0 << 21)
#define PDM_V2_HPF_V2_FREQ_0_468	(0x1 << 21)
#define PDM_V2_HPF_V2_FREQ_0_937	(0x2 << 21)
#define PDM_V2_HPF_V2_FREQ_1_875	(0x3 << 21)
#define PDM_V2_HPF_V2_FREQ_3_75		(0x4 << 21)
#define PDM_V2_HPF_V2_FREQ_7_5		(0x5 << 21)
#define PDM_V2_HPF_V2_FREQ_15		(0x6 << 21)
#define PDM_V2_HPF_V2_FREQ_30		(0x7 << 21)
#define PDM_V2_HPF_V2_FREQ_60		(0x8 << 21)
#define PDM_V2_HPF_V2_FREQ_122		(0x9 << 21)
#define PDM_V2_HPF_V2_FREQ_251		(0xa << 21)
#define PDM_V2_HPF_V2_FREQ_528		(0xb << 21)
#define PDM_V2_HPF_V2_FREQ_1183		(0xc << 21)
#define PDM_V2_HPF_V2_FREQ_3152		(0xd << 21)
#define PDM_V2_GAIN_MSK			(0xff << 23)
#define PDM_V2_GAIN_SHIFT		24
#define PDM_V2_GAIN_MIN			0
#define PDM_V2_GAIN_MAX			0x7f
#define PDM_V2_GAIN_0DB			(175 << 23)
#define PDM_V2_HPF_R_MSK		(0x1 << 21)
#define PDM_V2_HPF_R_EN			(0x1 << 21)
#define PDM_V2_HPF_R_DIS		(0x0 << 21)
#define PDM_V2_HPF_L_MSK		(0x1 << 22)
#define PDM_V2_HPF_L_EN			(0x1 << 22)
#define PDM_V2_HPF_L_DIS		(0x0 << 22)
#define PDM_V2_HPF_FREQ_MSK		(0x3 << 19)
#define PDM_V2_HPF_FREQ_3_79		(0x0 << 19)
#define PDM_V2_HPF_FREQ_60		(0x1 << 19)
#define PDM_V2_HPF_FREQ_243		(0x2 << 19)
#define PDM_V2_HPF_FREQ_493		(0x3 << 19)
#define PDM_V2_FIR_COM_BPS_MSK		(0x1 << 18)
#define PDM_V2_SIG_SCALE_MODE_MSK	(0x1 << 17)
#define PDM_V2_SIG_SCALE_HALF		(0x1 << 17)
#define PDM_V2_SIG_SCALE_NORMAL		(0x0 << 17)
#define PDM_V2_CIC_SCALE_MSK		(0x7f << 10)
#define PDM_V2_CIC_SCALE_SHIFT		10
#define PDM_V2_CIC_SCALE(x)		((x) << PDM_V2_CIC_SCALE_SHIFT)
#define PDM_V2_CIC_RATIO_MSK		(0x1ff << 1)
#define PDM_V2_CIC_RATIO_SHIFT		1
#define PDM_V2_CIC_RATIO(x)		((x - 1) << PDM_V2_CIC_RATIO_SHIFT)
#define PDM_V2_FILTER_EN_MSK		(0x1 << 0)
#define PDM_V2_FILTER_EN		(0x1 << 0)
#define PDM_V2_FILTER_DIS		(0x0 << 0)

/* PDM_V2_FIFO_CTRL */
#define PDM_V2_RFL_MSK			(0xff << 20)
#define PDM_V2_RDL_MSK			(0xff << 13)
#define PDM_V2_DMA_RDL(x)		((x - 1) << 13)
#define PDM_V2_DMA_RD_MSK		(0x1 << 12)
#define PDM_V2_DMA_RD_EN		(0x1 << 12)
#define PDM_V2_DMA_RD_DIS		(0x0 << 12)
#define PDM_V2_RXOI_MSK			(0x1 << 11)
#define PDM_V2_RXFI_MSK			(0x1 << 10)
#define PDM_V2_RXOIC_MSK		(0x1 << 9)
#define PDM_V2_RFT_MSK			(0x7f << 2)
#define PDM_V2_RXOIE_MSK		(0x1 << 1)
#define PDM_V2_RXFTIE_MSK		(0x1 << 0)

/* PDM FIFO CTRL */
#define PDM_V2_FIFO_CNT(x)		(((x) >> 20) & 0xff)

/* PDM_V2_GAIN_CTRL */
/* 0.375dB every step. 0: mute, 1: -65.25dB, 255: 30dB */
#define PDM_V2_GAIN_CTRL_MSK		(0xff << 0)
#define PDM_V2_GAIN_CTRL_SHIFT		1
#define PDM_V2_GAIN_CTRL_MIN		0
#define PDM_V2_GAIN_CTRL_MAX		0x7f
#define PDM_V2_GAIN_CTRL_0DB		(175 << 0)

/* PDM_V2_FILTER_CTRL1 */
#define PDM_V2_FILT1_HPF_V2_R_MSK	(0x1 << 0)
#define PDM_V2_FILT1_HPF_V2_R_EN	(0x1 << 0)
#define PDM_V2_FILT1_HPF_V2_R_DIS	(0x0 << 0)
#define PDM_V2_FILT1_HPF_V2_L_MSK	(0x1 << 1)
#define PDM_V2_FILT1_HPF_V2_L_EN	(0x1 << 1)
#define PDM_V2_FILT1_HPF_V2_L_DIS	(0x0 << 1)
#define PDM_V2_FILT1_HPF_V2_FREQ_MSK	(0xf << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_0_234	(0x0 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_0_468	(0x1 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_0_937	(0x2 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_1_875	(0x3 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_3_75	(0x4 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_7_5	(0x5 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_15	(0x6 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_30	(0x7 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_60	(0x8 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_122	(0x9 << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_251	(0xa << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_528	(0xb << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_1183	(0xc << 2)
#define PDM_V2_FILT1_HPF_V2_FREQ_3152	(0xd << 2)

#endif
