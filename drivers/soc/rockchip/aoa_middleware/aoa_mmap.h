/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Rockchip AOA Memory Mapping Driver
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 */

#ifndef __AOA_MMAP_H__
#define __AOA_MMAP_H__

void *aoa_mmap_probe(struct platform_device *pdev);
int aoa_mmap_remove(struct platform_device *pdev, void *am_d);

#endif /* __AOA_MMAP_H__ */
