/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Rockchip Secure OTP Driver
 *
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 * Author: Hisping <hisping.lin@rock-chips.com>
 */

#ifndef __ROCKCHIP_SEC_OTP_H
#define __ROCKCHIP_SEC_OTP_H

#include <linux/errno.h>

#if IS_REACHABLE(CONFIG_NVMEM_ROCKCHIP_SEC_OTP)
int rockchip_read_oem_non_protected_otp(unsigned int byte_off,
				void *byte_buf, size_t byte_len);
int rockchip_write_oem_non_protected_otp(unsigned int byte_off,
				void *byte_buf, size_t byte_len);
#else
static inline int rockchip_read_oem_non_protected_otp(unsigned int byte_off,
				void *byte_buf, size_t byte_len)
{
	return -ENODEV;
}

static inline int rockchip_write_oem_non_protected_otp(unsigned int byte_off,
				void *byte_buf, size_t byte_len)
{
	return -ENODEV;
}
#endif

#endif /* __ROCKCHIP_SEC_OTP_H */
