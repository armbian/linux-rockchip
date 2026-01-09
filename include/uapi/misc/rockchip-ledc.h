/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Rockchip LEDC (LED Controller) driver
 *
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 * Author: Eddy Zhang <eddy.zhang@rock-chips.com>
 */

#ifndef _UAPI_MISC_ROCKCHIP_LEDC_H_
#define _UAPI_MISC_ROCKCHIP_LEDC_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * User-kernel ABI for Rockchip LEDC misc device.
 *
 * Keep this header stable: changes may break userspace.
 */

enum pwm_ledc_pol_t {
	/*
	 * When the bus is idle, the level is low; when sending data, logic high
	 * is high level; reset code is low level.
	 */
	RK_LEDC_POL_NORMAL = 0,
	/*
	 * When the bus is idle, the level is high; when sending data, logic high
	 * is low level; reset code is high level.
	 */
	RK_LEDC_POL_REVERSAL = 1,
};

/*
 * flags is a packed __u32:
 * - bits[0:0]: polarity flag, values from enum pwm_ledc_pol_t
 * - bits[31:1]: reserved for future flags, must be 0 for now
 *
 * This keeps ABI stable while allowing future extensions without changing
 * struct rk_ledc_config size (ioctl payload).
 */
#define RK_LEDC_POL_SHIFT		0
#define RK_LEDC_POL_MASK		(0x1U << RK_LEDC_POL_SHIFT)
#define RK_LEDC_POL_GET(v)		(((v) & RK_LEDC_POL_MASK) >> RK_LEDC_POL_SHIFT)
#define RK_LEDC_POL_SET(v)		(((v) << RK_LEDC_POL_SHIFT) & RK_LEDC_POL_MASK)
/* No flags defined yet: reserved bits must be 0 */
#define RK_LEDC_FLAGS_MASK		0U
#define RK_LEDC_CONFIG_UNKNOWN_MASK	(~(RK_LEDC_POL_MASK | RK_LEDC_FLAGS_MASK))

/**
 * struct pwm_ledc_timing_config - PWM LEDC timing configuration (ns).
 * @t0h_ns: high time for code 0 (ns)
 * @t0l_ns: low time for code 0 (ns)
 * @t1h_ns: high time for code 1 (ns)
 * @t1l_ns: low time for code 1 (ns)
 * @reset_ns: reset time (ns)
 */
struct pwm_ledc_timing_config {
	__u16 t0h_ns;
	__u16 t0l_ns;
	__u16 t1h_ns;
	__u16 t1l_ns;
	__u32 reset_ns;
};

/**
 * struct rk_ledc_config - PWM LEDC configuration.
 * @timing: timing config of output waveform
 * @flags: packed polarity/flags, see RK_LEDC_POL_* and RK_LEDC_FLAGS_*
 */
struct rk_ledc_config {
	struct pwm_ledc_timing_config timing;
	__u32 flags;
};

/* ioctl commands */
#define LEDC_IOC_MAGIC		'L'
#define LEDC_IOC_SET_TIMING_CONFIG	_IOW(LEDC_IOC_MAGIC, 1, struct rk_ledc_config)
#define LEDC_IOC_GET_TIMING_CONFIG	_IOR(LEDC_IOC_MAGIC, 2, struct rk_ledc_config)

#endif /* _UAPI_MISC_ROCKCHIP_LEDC_H_ */

