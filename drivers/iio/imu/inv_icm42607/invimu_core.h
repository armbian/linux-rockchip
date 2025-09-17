/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Rockchip Electronics Co., Ltd.
 */
#ifndef __INVIMU_CORE_H
#define __INVIMU_CORE_H

int invimu_chip_init(struct imu_ctrb *ctrb, bool use_spi);
int invimu_core_probe(struct device *dev, struct regmap *regmap, int irq, bool use_spi);
#endif
