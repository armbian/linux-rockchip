/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Shunqing Chen <csq@rock-chips.com>
 */

#ifndef RK628_GPIO_H
#define RK628_GPIO_H

#define GPIO0_BASE		0x000D0000
#define GPIO1_BASE		0x000E0000
#define GPIO2_BASE		0x000F0000
#define GPIO3_BASE		0x00100000

/* GPIO control registers */
#define GPIO_SWPORT_DR_L	0x00
#define GPIO_SWPORT_DR_H	0x04
#define GPIO_SWPORT_DDR_L	0x08
#define GPIO_SWPORT_DDR_H	0x0c
#define GPIO_INTEN_L		0x10
#define GPIO_INTEN_H		0x14
#define GPIO_INTMASK_L		0x18
#define GPIO_INTMASK_H		0x1c
#define GPIO_INTTYPE_L		0x20
#define GPIO_INTTYPE_H		0x24
#define GPIO_INT_POLARITY_L	0x28
#define GPIO_INT_POLARITY_H	0x2c
#define GPIO_INT_BOTHEDGE_L	0x30
#define GPIO_INT_BOTHEDGE_H	0x34
#define GPIO_DEBOUNCE_L		0x38
#define GPIO_DEBOUNCE_H		0x3c
#define GPIO_DBCLK_DIV_EN_L	0x40
#define GPIO_DBCLK_DIV_EN_H	0x44
#define GPIO_INT_STATUS		0x50
#define GPIO_INT_RAWSTATUS	0x58
#define GPIO_PORTS_EOI_L	0x60
#define GPIO_PORTS_EOI_H	0x64
#define GPIO_EXT_PORT		0x70
#define GPIO_VER_ID		0x78

#endif

