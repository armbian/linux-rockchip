/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 */
#ifndef __MAXIM2C_COMPACT_H__
#define __MAXIM2C_COMPACT_H__

#include <linux/version.h>

#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
enum rkmodule_pad_type {
	PAD0,
	PAD1,
	PAD2,
	PAD3,
	PAD_MAX,
};

#ifndef fallthrough
#define fallthrough
#endif

#ifndef read_poll_timeout
#define read_poll_timeout(op, val, cond, sleep_us, timeout_us,			\
			sleep_before_read, args...)				\
{										\
	u64 __timeout_us = (timeout_us);					\
	unsigned long __sleep_us = (sleep_us);					\
	ktime_t __timeout = ktime_add_us(ktime_get(), __timeout_us);		\
	might_sleep_if((__sleep_us) != 0);					\
	if (sleep_before_read && __sleep_us)					\
		usleep_range((__sleep_us >> 2) + 1, __sleep_us);		\
	for (;;) {								\
		(val) = op(args);						\
		if (cond)							\
			break;							\
		if (__timeout_us &&						\
				ktime_compare(ktime_get(), __timeout) > 0) {	\
			(val) = op(args);					\
			break;							\
		}								\
		if (__sleep_us)							\
			usleep_range((__sleep_us >> 2) + 1, __sleep_us);	\
		cpu_relax();							\
	}									\
	(cond) ? 0 : -ETIMEDOUT;						\
}
#endif /* read_poll_timeout */
#endif /* LINUX_VERSION_CODE */

#endif /* __MAXIM2C_COMPACT_H__ */
