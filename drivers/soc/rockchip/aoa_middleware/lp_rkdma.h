/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Rockchip Low Power DMA Driver

 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 */

#ifndef __LP_RKDMA_H__
#define __LP_RKDMA_H__

int lp_rkdma_probe(struct platform_device *pdev, void *data);
int lp_rkdma_remove(struct platform_device *pdev);

#endif /* __LP_RKDMA_H__ */
