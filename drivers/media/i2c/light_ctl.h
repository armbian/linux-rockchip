/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef LIGHT_CTL_HEAD_H
#define LIGHT_CTL_HEAD_H

#include <linux/pwm.h>
#include <linux/rk-camera-module.h>
#include <linux/types.h>
#include <linux/kref.h>
#include <linux/completion.h>

struct light_ctl_info {
	struct pwm_device	*pwm;
	struct pwm_state	pwm_state;
	struct gpio_desc	*light_gpio;
	struct rk_light_param	light_param;
	u32			light_index;
};

struct lightctl_device {
	struct platform_device *pdev;
	struct light_ctl_info *light_info;
	struct proc_dir_entry *procfs;
	char name[128];
	struct mutex mutex;
	struct kref kref;
	struct completion removed;
};

#if IS_REACHABLE(CONFIG_VIDEO_LIGHT_CTL)

int light_ctl_write(int light_id, const struct rk_light_param *light_param);

#else

static inline int light_ctl_write(int light_id, const struct rk_light_param *light_param)
{
	return 0;
}

#endif

#endif
