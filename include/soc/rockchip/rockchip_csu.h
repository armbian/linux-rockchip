/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023, Rockchip Electronics Co., Ltd.
 */

#ifndef __SOC_ROCKCHIP_CSU_H
#define __SOC_ROCKCHIP_CSU_H

#include <dt-bindings/soc/rockchip-csu.h>

#define CSU_MAX_DIV		8
#define CSU_DIV_MASK		0x7
#define CSU_EN_MASK		0xefff

#define RK3572_CSU_EN_SHIFT		12
#define RK3572_CSU_EN_MASK		(0x1U << RK3572_CSU_EN_SHIFT)
#define RK3572_CSU_DIV_SHIFT		14
#define RK3572_CSU_DIV_MASK		(0x3U << RK3572_CSU_DIV_SHIFT)
#define RK3572_CSU_WAIT_TH_SHIFT	16

struct csu_clk;

#if IS_REACHABLE(CONFIG_ROCKCHIP_CSU)
struct csu_clk *rockchip_csu_get(struct device *dev, const char *name);
int rockchip_csu_enable(struct csu_clk *clk);
int rockchip_csu_disable(struct csu_clk *clk);
int rockchip_csu_set_div(struct csu_clk *clk, unsigned int div);
int rockchip_csu_set_threshold(struct csu_clk *clk, u32 wait_th, u32 idle_th);
#else
static inline struct csu_clk *
rockchip_csu_get(struct device *dev, const char *name)
{
	return ERR_PTR(-EOPNOTSUPP);
}

static inline int rockchip_csu_enable(struct csu_clk *clk)
{
	return -EOPNOTSUPP;
}

static inline int rockchip_csu_disable(struct csu_clk *clk)
{
	return -EOPNOTSUPP;
}

static inline int
rockchip_csu_set_div(struct csu_clk *clk, unsigned int div)
{
	return -EOPNOTSUPP;
}

static inline int
rockchip_csu_set_threshold(struct csu_clk *clk, u32 wait_th, u32 idle_th)
{
	return -EOPNOTSUPP;
}
#endif

#endif
