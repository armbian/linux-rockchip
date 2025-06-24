/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef RK_SPI_SLAVE_H
#define RK_SPI_SLAVE_H

#include <linux/types.h>

#define SPI_SLAVE_BASE		's'
#define SPI_SLAVE_INIT_CYCLIC	_IOW(SPI_SLAVE_BASE, 0, int)
#define SPI_SLAVE_DEINIT_CYCLIC	_IOW(SPI_SLAVE_BASE, 1, int)

#endif
