/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKFEC_HW_H
#define _RKFEC_HW_H

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <media/videobuf2-cma-sg.h>
#include <media/videobuf2-dma-sg.h>
#include <soc/rockchip/rockchip_iommu.h>


#define FEC_MAX_BUS_CLK 4

enum rkfec_fec_ver {
	RKFEC_V10 = 0x00, /* Version 1.0 of the FEC */
	RKFEC_V20 = 0x20, /* Version 2.0 of the FEC */
};

struct fec_clk_info {
	u32 clk_rate;
	u32 refer_data;
};

struct irqs_data {
	const char *name;
	irqreturn_t (*irq_hdl)(int irq, void *ctx);
};

struct fec_match_data {
	enum rkfec_fec_ver fec_ver;
	int clks_num;
	const char * const *clks;
	int clk_rate_tbl_num;
	const struct fec_clk_info *clk_rate_tbl;
	struct irqs_data *irqs;
	int num_irqs;
};

struct rkfec_hw_dev {
	struct device *dev;
	void __iomem *base_addr;
	const struct fec_match_data *match_data;
	const struct fec_clk_info *clk_rate_tbl;
	struct reset_control *reset;
	struct clk *clks[FEC_MAX_BUS_CLK];
	struct rkfec_offline_dev ofl_dev;
	int clk_rate_tbl_num;
	int clks_num;
	/* lock for hw */
	struct mutex dev_lock;
	/* lock for irq */
	spinlock_t irq_lock;
	const struct vb2_mem_ops *mem_ops;
	bool is_mmu;
	bool is_idle;
	bool is_dma_config;
	bool is_dma_sg_ops;
	bool is_shutdown;
	bool is_suspend;
	enum rkfec_fec_ver fec_ver;

	void (*soft_reset)(struct rkfec_hw_dev *hw);
	int (*set_clk)(struct clk *clk, unsigned long rate);
};

#ifndef IS_LINUX_VERSION_AT_LEAST_6_1
#define IS_LINUX_VERSION_AT_LEAST_6_1 (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
#endif

#endif
