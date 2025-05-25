/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ALSA SoC Audio Layer - Rockchip ASRC Controller driver
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef _ROCKCHIP_ASRC_H_
#define _ROCKCHIP_ASRC_H_

#define ASRC_VERSION			0x0000
#define ASRC_CON			0x0004
#define ASRC_CLKDIV_CON			0x0008
#define ASRC_DATA_FMT			0x000c
#define ASRC_LOOP_CON0			0x0010
#define ASRC_LOOP_CON1			0x0014
#define ASRC_LOOP_CON2			0x0018
#define ASRC_MANUAL_RATIO		0x0020
#define ASRC_SAMPLE_RATE		0x0024
#define ASRC_RESAMPLE_RATE		0x0028
#define ASRC_TRACK_PERIOD		0x002c
#define ASRC_RATIO_MARGIN		0x0030
#define ASRC_LRCK_MARGIN		0x0034
#define ASRC_FETCH_LEN			0x0040
#define ASRC_DMA_THRESH			0x0050
#define ASRC_LRCK_FILT			0x0058
#define ASRC_INT_CON			0x0060
#define ASRC_INT_ST			0x0064
#define ASRC_ST				0x0070
#define ASRC_RATIO_ST			0x0080
#define ASRC_RESAMPLE_RATE_ST		0x0084
#define ASRC_THETA_CNT_ST		0x0090
#define ASRC_DECI_THETA_ACC_ST		0x0094
#define ASRC_FIFO_IN_WRCNT		0x00a0
#define ASRC_FIFO_IN_RDCNT		0x00a4
#define ASRC_FIFO_OUT_WRCNT		0x00b0
#define ASRC_FIFO_OUT_RDCNT		0x00b4
#define ASRC_RXDR			0x00c0
#define ASRC_TXDR			0x00c4
#define ASRC_FIFO_IN_DATA		0x1000
#define ASRC_FIFO_OUT_DATA		0x5000

/**********************ASRC_CON************************/
#define ASRC_RATIO_FILT_WIN			(0x3 << 20)
#define ASRC_RATIO_TRACK_MODE			(0x1 << 19)
#define ASRC_RATIO_TRACK_MODE_DIS		(0x0 << 19)
#define ASRC_RATIO_EXC_MSK			(0x1 << 18)
#define ASRC_RATIO_EXC_EN			(0x1 << 18)
#define ASRC_RATIO_EXC_DIS			(0x0 << 18)
#define ASRC_RATIO_FILT_MSK			(0x1 << 17)
#define ASRC_RATIO_FILT_EN			(0x1 << 17)
#define ASRC_RATIO_FILT_DIS			(0x0 << 17)
#define ASRC_RATIO_TRACK_MSK			(0x1 << 16)
#define ASRC_RATIO_TRACK_EN			(0x1 << 16)
#define ASRC_RATIO_TRACK_DIS			(0x0 << 16)
#define ASRC_OUT_MSK				(0x1 << 9)
#define ASRC_OUT_STOP				(0x1 << 9)
#define ASRC_OUT_START				(0x0 << 9)
#define ASRC_IN_MSK				(0x1 << 8)
#define ASRC_IN_STOP				(0x1 << 8)
#define ASRC_IN_START				(0x0 << 8)
#define ASRC_CHAN_NUM_MSK			(0x3 << 4)
#define ASRC_CHAN_NUM(x)			(((x - 2) / 2) << 4)
#define ASRC_REAL_TIME_MODE_MSK			(0x3 << 2)
#define ASRC_M2M				(0x0 << 2)
#define ASRC_S2M				(0x1 << 2)
#define ASRC_M2D				(0x2 << 2)
#define ASRC_S2D				(0x3 << 2)
#define ASRC_MODE_MSK				(0x1 << 1)
#define ASRC_REAL_TIME				(0x0 << 1)
#define ASRC_MEMORY_FETCH			(0x1 << 1)
#define ASRC_MSK				(0x1 << 0)
#define ASRC_EN					(0x1 << 0)
#define ASRC_DIS				(0x0 << 0)

/**********************ASRC_CLKDIV_CON************************/
#define ASRC_DST_LRCK_DIV_MSK			(0x1 << 31)
#define ASRC_DST_LRCK_DIV_EN			(0x1 << 31)
#define ASRC_DST_LRCK_DIV_DIS			(0x0 << 31)
#define ASRC_DST_LRCK_DIV_CON_MSK		(0x7fff << 16)
#define ASRC_DST_LRCK_DIV(x)			((x - 1) << 16)
#define ASRC_SRC_LRCK_DIV_MSK			(0x1 << 15)
#define ASRC_SRC_LRCK_DIV_EN			(0x1 << 15)
#define ASRC_SRC_LRCK_DIV_DIS			(0x0 << 15)
#define ASRC_SRC_LRCK_DIV_CON_MSK		(0x7fff << 0)
#define ASRC_SRC_LRCK_DIV(x)			((x - 1) << 0)

/**********************ASRC_DATA_FMT************************/
#define ASRC_OSJM_MSK				(0x1f << 24)
#define ASRC_OSJM(x)				(x << 24)
#define ASRC_ISJM_MSK				(0x1f << 16)
#define ASRC_ISJM(x)				(x << 16)
#define ASRC_OFMT_MSK				(0x1 << 12)
#define ASRC_OFMT_16				(0x1 << 12)
#define ASRC_OFMT_32				(0x0 << 12)
#define ASRC_IFMT_MSK				(0x1 << 8)
#define ASRC_IFMT_16				(0x1 << 8)
#define ASRC_IFMT_32				(0x0 << 8)
#define ASRC_OWL_MSK				(0x3 << 4)
#define ASRC_OWL_16BIT				(0x2 << 4)
#define ASRC_OWL_20BIT				(0x1 << 4)
#define ASRC_OWL_24BIT				(0x0 << 4)
#define ASRC_IWL_MSK				(0x3 << 0)
#define ASRC_IWL_16BIT				(0x2 << 0)
#define ASRC_IWL_20BIT				(0x1 << 0)
#define ASRC_IWL_24BIT				(0x0 << 0)

/**********************ASRC_TRACK_PERIOD************************/
#define ASRC_RATIO_TRACK_DIV_MSK		(0xfff << 16)
#define ASRC_RATIO_TRACK_DIV(x)			(x << 16)
#define ASRC_RATIO_TRACK_PERIOD_MSK		(0xfff << 0)
#define ASRC_RATIO_TRACK_PERIOD(x)		(x << 0)

/**********************ASRC_DMA_THRESH************************/
#define ASRC_POS_THRESH_MSK			(0x1f << 26)
#define ASRC_POS_THRESH(x)			(x << 26)
#define ASRC_NEG_THRESH_MSK			(0x1f << 20)
#define ASRC_NEG_THRESH(x)			(x << 20)
#define ASRC_OUT_THRESH_MSK			(0xf << 12)
#define ASRC_OUT_THRESH(x)			(x << 12)
#define ASRC_IN_THRESH_MSK			(0xf << 8)
#define ASRC_IN_THRESH(x)			(x << 8)
#define ASRC_DMA_RX_THRESH_MSK			(0xf << 4)
#define ASRC_DMA_RX_THRESH(x)			(x << 4)
#define ASRC_DMA_TX_THRESH_MSK			(0xf << 0)
#define ASRC_DMA_TX_THRESH(x)			(x << 0)

/**********************ASRC_LRCK_FILT************************/
#define ASRC_LRCK_DRIFT_MARGIN_MSK		(0x7ff << 0)
#define ASRC_LRCK_DRIFT_MARGIN(x)		(x << 0)
#define ASRC_LRCK_SUPER_MARGIN_MSK		(0x7ff << 16)
#define ASRC_LRCK_SUPER_MARGIN(x)		(x << 16)
#define ASRC_LRCK_FILT_MSK			(0x1 << 31)
#define ASRC_LRCK_FILT_EN			(0x1 << 31)
#define ASRC_LRCK_FILT_DIS			(0x0 << 31)

/**********************ASRC_INT_CON************************/
#define ASRC_DST_LRCK_UNLOCK_MSK		(0x1 << 12)
#define ASRC_DST_LRCK_UNLOCK_EN			(0x1 << 12)
#define ASRC_DST_LRCK_UNLOCK_DIS		(0x0 << 12)
#define ASRC_SRC_LRCK_UNLOCK_MSK		(0x1 << 11)
#define ASRC_SRC_LRCK_UNLOCK_EN			(0x1 << 11)
#define ASRC_SRC_LRCK_UNLOCK_DIS		(0x0 << 11)
#define ASRC_RATIO_UNLOCK_MSK			(0x1 << 10)
#define ASRC_RATIO_UNLOCK_EN			(0x1 << 10)
#define ASRC_RATIO_UNLOCK_DIS			(0x0 << 10)
#define ASRC_FIFO_OUT_EMPTY_MSK			(0x1 << 9)
#define ASRC_FIFO_OUT_EMPTY_EN			(0x1 << 9)
#define ASRC_FIFO_OUT_EMPTY_DIS			(0x0 << 9)
#define ASRC_FIFO_OUT_FULL_MSK			(0x1 << 8)
#define ASRC_FIFO_OUT_FULL_EN			(0x1 << 8)
#define ASRC_FIFO_OUT_FULL_DIS			(0x0 << 8)
#define ASRC_FIFO_IN_EMPTY_MSK			(0x1 << 7)
#define ASRC_FIFO_IN_EMPTY_EN			(0x1 << 7)
#define ASRC_FIFO_IN_EMPTY_DIS			(0x0 << 7)
#define ASRC_FIFO_IN_FULL_MSK			(0x1 << 6)
#define ASRC_FIFO_IN_FULL_EN			(0x1 << 6)
#define ASRC_FIFO_IN_FULL_DIS			(0x1 << 6)
#define ASRC_RATIO_UPDATE_DONE_MSK		(0x1 << 5)
#define ASRC_RATIO_UPDATE_DONE_EN		(0x1 << 5)
#define ASRC_RATIO_UPDATE_DONE_DIS		(0x0 << 5)
#define ASRC_RATIO_CHANGE_DONE_MSK		(0x1 << 4)
#define ASRC_RATIO_CHANGE_DONE_EN		(0x1 << 4)
#define ASRC_RATIO_CHANGE_DONE_DIS		(0x0 << 4)
#define ASRC_RATIO_INIT_DONE_MSK		(0x1 << 3)
#define ASRC_RATIO_INIT_DONE_EN			(0x1 << 3)
#define ASRC_RATIO_INIT_DONE_DIS		(0x0 << 3)
#define ASRC_CONV_ERROR_MSK			(0x1 << 2)
#define ASRC_CONV_ERROR_EN			(0x1 << 2)
#define ASRC_CONV_ERROR_DIS			(0x0 << 2)
#define ASRC_CONV_DONE_MSK			(0x1 << 1)
#define ASRC_CONV_DONE_EN			(0x1 << 1)
#define ASRC_CONV_DONE_DIS			(0x0 << 0)
#define ASRC_OUT_START_MSK			(0x1 << 0)
#define ASRC_OUT_START_EN			(0x1 << 0)
#define ASRC_OUT_START_DIS			(0x0 << 0)

/**********************ASRC_INT_ST************************/
#define ASRC_DST_LRCK_UNLOCK_ST			(0x1 << 12)
#define ASRC_SRC_LRCK_UNLOCK_ST			(0x1 << 11)
#define ASRC_RATIO_UNLOCK_ST			(0x1 << 10)
#define ASRC_FIFO_OUT_EMPTY_ST			(0x1 << 9)
#define ASRC_FIFO_OUT_FULL_ST			(0x1 << 8)
#define ASRC_FIFO_IN_EMPTY_ST			(0x1 << 7)
#define ASRC_FIFO_IN_FULL_ST			(0x1 << 6)
#define ASRC_RATIO_UPDATE_DONE_ST		(0x1 << 5)
#define ASRC_RATIO_CHANGE_DONE_ST		(0x1 << 4)
#define ASRC_RATIO_INIT_DONE_ST			(0x1 << 3)
#define ASRC_CONV_ERROR_ST			(0x1 << 2)
#define ASRC_CONV_DONE_ST			(0x1 << 1)
#define ASRC_OUT_START_ST			(0x1 << 0)

/**********************ASRC_ST************************/
#define ASRC_RATIO_ST_MSK			(0x3 << 29)
#define ASRC_RATIO_ST_INIT			(0x0 << 29)
#define ASRC_RATIO_ST_TRACK			(0x1 << 29)
#define ASRC_RATIO_ST_STOP			(0x2 << 29)
#define ASRC_EXCEED_POS				(0x1 << 28)
#define ASRC_EXCEED_NEG				(0x1 << 27)

#endif
