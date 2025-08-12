/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Header providing constants for Rockchip hwspinlock.
 *
 * Copyright (C) 2025, Rockchip Electronics Co., Ltd.
 */

#ifndef __DT_BINDINGS_RV1126B_HWSPINLOCK_H__
#define __DT_BINDINGS_RV1126B_HWSPINLOCK_H__

/* User Id: 1 ~ 15 */
#define HWSPINLOCK_USER_PRIMARY_SPL               1
#define HWSPINLOCK_USER_PRIMARY_BL31              2
#define HWSPINLOCK_USER_PRIMARY_BL32              3
#define HWSPINLOCK_USER_PRIMARY_UBOOT             4
#define HWSPINLOCK_USER_PRIMARY_OS                5
#define HWSPINLOCK_USER_SECONDARY_SPL             6
#define HWSPINLOCK_USER_SECONDARY_BL31            7
#define HWSPINLOCK_USER_SECONDARY_BL32            8
#define HWSPINLOCK_USER_SECONDARY_UBOOT           9
#define HWSPINLOCK_USER_SECONDARY_OS              10

/* Lock Id: 0 ~ 63  */
#define HWSPINLOCK_LOCK_OTP                       0
#define HWSPINLOCK_LOCK_CRYPTO                    1
#define HWSPINLOCK_LOCK_RNG                       2
#define HWSPINLOCK_LOCK_KEYLADDER                 3
#define HWSPINLOCK_LOCK_RKVDEC                    4
#define HWSPINLOCK_LOCK_JPEG                      5
#define HWSPINLOCK_LOCK_IEP                       6
#define HWSPINLOCK_LOCK_RKVENC0                   7
#define HWSPINLOCK_LOCK_RKVENC1                   8
#define HWSPINLOCK_LOCK_NPU                       9
#define HWSPINLOCK_LOCK_GPU                       10

#endif
