/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Rockchip AOA Controller Driver
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 */

#ifndef __AOA_DRV_H__
#define __AOA_DRV_H__

int rockchip_aoa_probe(struct platform_device *pdev, void *data);
int rockchip_aoa_remove(struct platform_device *pdev);

#endif /* __AOA_DRV_H__ */
