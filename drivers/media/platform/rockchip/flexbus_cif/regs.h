/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Rockchip Flexbus CIF Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef _FLEXBUS_CIF_REGS_H
#define _FLEXBUS_CIF_REGS_H

#define GRF_FLEXBUS1_CON		(0x6400)

#define CIF_INPUT_ORDER_YVYU		(0x0 << 0)
#define CIF_INPUT_ORDER_VYUY		(0x1 << 0)
#define CIF_INPUT_ORDER_YUYV		(0x2 << 0)
#define CIF_INPUT_ORDER_UYVY		(0x3 << 0)

#define CIF_OUTPUT_ORDER_YVYU		(0x0 << 2)
#define CIF_OUTPUT_ORDER_VYUY		(0x1 << 2)
#define CIF_OUTPUT_ORDER_YUYV		(0x2 << 2)
#define CIF_OUTPUT_ORDER_UYVY		(0x3 << 2)


#define YUV_INPUT_422			(0x00 << 7)
#define CIF_YUV_STORED_BIT_WIDTH	(8U)
#define CIF_CROP_Y_SHIFT		16

#define CIF_FIFO_OVERFLOW		FLEXBUS_RX_OVF_ISR
#define CIF_BANDWIDTH_LACK		FLEXBUS_RX_UDF_ISR
#define CIF_SIZE_ERR			FLEXBUS_DVP_FRAME_AB_ISR
#define CIF_DMA_START			FLEXBUS_DVP_FRAME_START_ISR
#define CIF_DMA_END			(FLEXBUS_DMA_DST0_ISR | FLEXBUS_DMA_DST1_ISR)

#define HSY_HIGH_ACTIVE			(BIT(1))
#define HSY_LOW_ACTIVE			(0)
#define VSY_HIGH_ACTIVE			(BIT(0))
#define VSY_LOW_ACTIVE			(0)
#define CIF_RAW_STORED_BIT_WIDTH	(8U)
#define FLEXBUS_IMR_DMA_DST0_SHIFT	(10U)

#define CIF_YUV2RGB_ENABLE		(0x1 << 0)
#define CIF_YUV2RGB_B_LSB		(0x1 << 3)

#define CIF_YUV2RGB_BT601_LIMIT		(0 << 1)
#define CIF_YUV2RGB_BT601_FULL		(2 << 1)
#define CIF_YUV2RGB_BT709_LIMIT		(3 << 1)

#endif

