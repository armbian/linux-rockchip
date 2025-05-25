/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKAIISP_HW_H
#define _RKAIISP_HW_H

#include <linux/interrupt.h>
#include <linux/rk-aiisp-config.h>

#define RKAIISP_MAX_BUS_CLK		10

enum rkaiisp_dev {
	RKAIISP_DEV_ID0 = 0,
	RKAIISP_DEV_ID1,
	RKAIISP_DEV_ID2,
	RKAIISP_DEV_ID3,
	RKAIISP_DEV_ID4,
	RKAIISP_DEV_ID5,
	RKAIISP_DEV_ID6,
	RKAIISP_DEV_ID7,
	RKAIISP_DEV_MAX,
};

enum rkaiisp_sw_reg {
	SW_REG_CACHE = 0xffffffff,
	SW_REG_CACHE_SYNC = 0xeeeeeeee,
};

struct aiisp_irqs_data {
	const char *name;
	irqreturn_t (*irq_hdl)(int irq, void *ctx);
};

struct aiisp_clk_info {
	u32 clk_rate;
	u32 refer_data;
};

struct aiisp_match_data {
	const char * const *clks;
	int num_clks;
	const struct aiisp_clk_info *clk_rate_tbl;
	int num_clk_rate_tbl;
	struct aiisp_irqs_data *irqs;
	int num_irqs;
};

struct rkaiisp_hw_dev {
	const struct aiisp_match_data *match_data;
	struct platform_device *pdev;
	struct device *dev;
	struct regmap *grf;
	void __iomem *base_addr;
	struct clk *clks[RKAIISP_MAX_BUS_CLK];
	int num_clks;
	const struct aiisp_clk_info *clk_rate_tbl;
	int num_clk_rate_tbl;
	struct reset_control *reset;

	struct rkaiisp_device *aidev[RKAIISP_DEV_MAX];
	int dev_num;
	int cur_dev_id;

	/* lock for multi dev */
	struct mutex dev_mutex;
	spinlock_t hw_lock;
	atomic_t refcnt;
	const struct vb2_mem_ops *mem_ops;

	bool is_dma_contig;
	bool is_dma_sg_ops;
	bool is_mmu;
	bool is_idle;
	bool is_single;
	bool is_shutdown;
};

int rkaiisp_ispidx_queue(int dev_id, struct rkisp_aiisp_st *idxbuf);

#endif
