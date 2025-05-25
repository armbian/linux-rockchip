/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_BUF_H__
#define __RKCE_BUF_H__

#include <linux/types.h>

int rkce_cma_init(void *device);
void rkce_cma_deinit(void *device);
void *rkce_cma_alloc(uint32_t size);
void rkce_cma_free(void *buf);
uint32_t rkce_cma_virt2phys(void *buf);
void *rkce_cma_phys2virt(uint32_t phys);

#endif
