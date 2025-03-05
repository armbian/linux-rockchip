/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Lingsong Ding <damon.ding@rock-chips.com>
 */

#ifndef RK628_PWM_H
#define RK628_PWM_H
#include "rk628.h"

int rk628_pwm_probe(struct rk628 *rk628, struct device_node *pwm_np);
void rk628_pwm_init(struct rk628 *rk628);
void rk628_pwm_create_debugfs_file(struct rk628 *rk628);

#endif
