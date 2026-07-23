/* SPDX-License-Identifier: GPL-2.0 */
/*
 * rk2116.h  --  RK2116 ALSA SoC Audio driver
 *
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 * Author: looper <looper.zheng@rock-chips.com>
 *
 */

#ifndef __RK2116_H__
#define __RK2116_H__

#include <linux/regmap.h>

/*
 *   mode0 / LSB : 0x2AC24C92
 *   mode0 / MSB : 0x54433249
 */
#define RK2116_REG_BOOT		(0x54433249)

#define RK2116_REG_ITMD		(0x00DE017B)    /* enter through mode */
#define RK2116_REG_OTMD		(0x0000017B)    /* exit through mode */

extern const struct regmap_config rk2116_regmap_config;
int rk2116_probe(struct device *dev, struct regmap *regmap);
void rk2116_remove(struct device *dev);

#endif /* __RK2116_H__ */
