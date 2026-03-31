/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Rockchip OTP Driver
 *
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 * Author: Hisping <hisping.lin@rock-chips.com>
 */

#ifndef __ROCKCHIP_OTP_H
#define __ROCKCHIP_OTP_H

#if IS_REACHABLE(CONFIG_NVMEM_ROCKCHIP_OTP)
void rockchip_otp_mutex_lock(void);
void rockchip_otp_mutex_unlock(void);
#else
static inline void rockchip_otp_mutex_lock(void) {}
static inline void rockchip_otp_mutex_unlock(void) {}
#endif

#endif /* __ROCKCHIP_OTP_H */
