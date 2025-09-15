/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Rockchip AOA Middleware Driver
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 */

#ifndef __AOA_MIDDLEWARE_H__
#define __AOA_MIDDLEWARE_H__

int aoa_middleware_aoa_notifier(s32 status, void *data);
int aoa_middleware_dma_notifier(s32 dma_count, void *data);

#endif /* __AOA_MIDDLEWARE_H__ */
