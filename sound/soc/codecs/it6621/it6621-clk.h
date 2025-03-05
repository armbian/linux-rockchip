/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#ifndef _IT6621_CLK_H
#define _IT6621_CLK_H

#include "it6621.h"
#include "it6621-earc.h"

int it6621_calc_rclk(struct it6621_priv *priv);
int it6621_force_pdiv(struct it6621_priv *priv);
void it6621_get_aclk(struct it6621_priv *priv);
void it6621_get_bclk(struct it6621_priv *priv);

#endif /* _IT6621_CLK_H */
