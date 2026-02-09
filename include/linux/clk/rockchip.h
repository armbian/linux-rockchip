/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 */

#ifndef __LINUX_CLK_ROCKCHIP_H_
#define __LINUX_CLK_ROCKCHIP_H_

#ifdef CONFIG_ROCKCHIP_CLK_COMPENSATION
int rockchip_pll_clk_compensation(struct clk *clk, int ppm);
#else
static inline int rockchip_pll_clk_compensation(struct clk *clk, int ppm)
{
	return -ENOSYS;
}
#endif

#if IS_REACHABLE(CONFIG_ROCKCHIP_CLK_ACC)
int rockchip_acc_adaptive_adjust(struct clk *clk, struct clk *ref_clk, bool enable);
int rockchip_acc_set_callbacks(struct clk *acc_clk, void *data, int (*lock_cb)(void *data),
			       int (*unlock_cb)(void *data));
#else
static inline int rockchip_acc_adaptive_adjust(struct clk *clk, struct clk *ref_clk, bool enable)
{
	return -EOPNOTSUPP;
}

static inline int rockchip_acc_set_callbacks(struct clk *acc_clk, void *data, int (*lock_cb)(void *data),
					     int (*unlock_cb)(void *data))
{
	return -EOPNOTSUPP;
}
#endif

#endif /* __LINUX_CLK_ROCKCHIP_H_ */
