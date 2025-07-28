/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKVPSS_HW_H
#define _RKVPSS_HW_H

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

#define VPSS_MAX_BUS_CLK 4
#define VPSS_MAX_DEV	 8

struct vpss_clk_info {
	u32 clk_rate;
	u32 refer_data;
};

struct vpss_match_data {
	int clks_num;
	const char * const *clks;
	enum rkvpss_ver vpss_ver;
	struct irqs_data *irqs;
	int num_irqs;
};

struct rkvpss_hw_dev {
	struct device *dev;
	void __iomem *base_addr;
	const struct vpss_match_data *match_data;
	const struct vpss_clk_info *clk_rate_tbl;
	struct reset_control *reset;
	struct clk *clks[VPSS_MAX_BUS_CLK];
	struct rkvpss_device *vpss[VPSS_MAX_DEV];
	struct rkvpss_offline_dev ofl_dev;
	struct list_head list;
	void *sw_reg;
	int clk_rate_tbl_num;
	int clks_num;
	int dev_num;
	int cur_dev_id;
	int pre_dev_id;
	unsigned long core_clk_min;
	unsigned long core_clk_max;
	enum rkvpss_ver	vpss_ver;
	/* lock for multi dev */
	struct mutex dev_lock;
	spinlock_t reg_lock;
	atomic_t refcnt;
	const struct vb2_mem_ops *mem_ops;
	bool is_ofl_ch[RKVPSS_OUTPUT_MAX];
	bool is_ofl_cmsc;
	bool is_mmu;
	bool is_single;
	bool is_dma_contig;
	bool is_shutdown;
	bool is_suspend;
	bool is_first;
	bool is_probe_end;
	int dvbm_refcnt;
	int dvbm_flag;
};

#ifdef CONFIG_VIDEO_ROCKCHIP_VPSS_V10
static inline bool is_vpss_v10(const struct rkvpss_hw_dev *hw_dev) { return hw_dev->vpss_ver == VPSS_V10; }
#else
static inline bool is_vpss_v10(const struct rkvpss_hw_dev *hw_dev) { return false; }
#endif

#ifdef CONFIG_VIDEO_ROCKCHIP_VPSS_V20
static inline bool is_vpss_v20(const struct rkvpss_hw_dev *hw_dev) { return hw_dev->vpss_ver == VPSS_V20; }
#else
static inline bool is_vpss_v20(const struct rkvpss_hw_dev *hw_dev) { return false; }
#endif

#define RKVPSS_ZME_TAP_COE(x, y) (((x) & 0x3ff) | (((y) & 0x3ff) << 16))
extern const s16 rkvpss_zme_tap8_coe[11][17][8];
extern const s16 rkvpss_zme_tap6_coe[11][17][8];
int rkvpss_get_zme_tap_coe_index(int ratio);
void rkvpss_cmsc_slop(struct rkvpss_cmsc_point *p0,
		      struct rkvpss_cmsc_point *p1,
		      int *k, int *hor);

void rkvpss_soft_reset(struct rkvpss_hw_dev *hw_dev);
void rkvpss_hw_write(struct rkvpss_hw_dev *hw_dev, u32 reg, u32 val);
u32 rkvpss_hw_read(struct rkvpss_hw_dev *hw_dev, u32 reg);
void rkvpss_hw_set_bits(struct rkvpss_hw_dev *hw, u32 reg, u32 mask, u32 val);
void rkvpss_hw_clear_bits(struct rkvpss_hw_dev *hw, u32 reg, u32 mask);
void rkvpss_hw_reg_save(struct rkvpss_hw_dev *dev);
void rkvpss_hw_reg_restore(struct rkvpss_hw_dev *dev);
#endif
