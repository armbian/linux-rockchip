/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Shunqing Chen <csq@rock-chips.com>
 */

#ifndef _RK628_MIPI_DPHY_H
#define _RK628_MIPI_DPHY_H

#include "rk628.h"

/* Test Code: 0x44 (HS RX Control of Lane 0) */
#define HSFREQRANGE(x)			UPDATE(x, 6, 1)
#define HSTX(x)				UPDATE(x, 6, 0)
#define HSZERO(x)			UPDATE(x, 5, 0)
#define HSPOST(x)			UPDATE(x, 3, 0)
#define HSEXIT(x)			UPDATE(x, 4, 0)

void rk628_testif_testclr_deassert(struct rk628 *rk628, uint8_t mipi_id);
void rk628_testif_testclr_assert(struct rk628 *rk628, uint8_t mipi_id);
u8 rk628_testif_write(struct rk628 *rk628, u8 test_code, u8 test_data, uint8_t mipi_id);
u8 rk628_testif_read(struct rk628 *rk628, u8 test_code, uint8_t mipi_id);
void rk628_mipi_dphy_init_hsfreqrange(struct rk628 *rk628, int lane_mbps, uint8_t mipi_id);
void rk628_mipi_dphy_init_hsmanual(struct rk628 *rk628, bool manual, uint8_t mipi_id);
int rk628_mipi_dphy_reset_assert(struct rk628 *rk628);
int rk628_mipi_dphy_reset_deassert(struct rk628 *rk628);

#endif
