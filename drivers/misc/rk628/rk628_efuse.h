/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Weixin Zhou <zwx@rock-chips.com>
 */

#ifndef RK628_EFUSE_H
#define RK628_EFUSE_H

#define EFUSE_BASE	0xb0000
#define EFUSE_REVISION	0x50

int rk628_efuse_read(struct rk628 *rk628, unsigned int offset,
				      void *val, size_t bytes);

#endif
