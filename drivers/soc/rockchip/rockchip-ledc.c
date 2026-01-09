// SPDX-License-Identifier: GPL-2.0-only
/*
 * Rockchip LEDC (LED Controller) driver
 *
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 * Author: Eddy Zhang <eddy.zhang@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/of_device.h>

#include <uapi/misc/rockchip-ledc.h>

#define HIWORD_UPDATE(v, l, h)	(((v) << (l)) | (GENMASK((h), (l)) << 16))

/* VERSION_ID */
#define VERSION_ID			0x00
#define CHANNEL_NUM_SUPPORT_SHIFT	0
#define CHANNEL_NUM_SUPPORT_MASK	(0xf << CHANNEL_NUM_SUPPORT_SHIFT)
#define CHANNLE_INDEX_SHIFT		4
#define CHANNLE_INDEX_MASK		(0xf << CHANNLE_INDEX_SHIFT)
/* ENABLE */
#define ENABLE				0x04
#define PWM_ENABLE_V4			(0x3 << 0)
#define PWM_CLK_EN(v)			HIWORD_UPDATE(v, 0, 0)
#define PWM_EN(v)			HIWORD_UPDATE(v, 1, 1)
#define PWM_CTRL_UPDATE_EN(v)		HIWORD_UPDATE(v, 2, 2)
#define PWM_GLOBAL_JOIN_EN(v)		HIWORD_UPDATE(v, 4, 4)
/* CLK_CTRL */
#define CLK_CTRL			0x08
#define CLK_PRESCALE(v)			HIWORD_UPDATE(v, 0, 2)
#define CLK_SCALE(v)			HIWORD_UPDATE(v, 4, 12)
#define CLK_SRC_SEL(v)			HIWORD_UPDATE(v, 13, 14)
#define SRC_CLK_PWM			0
#define SRC_CLK_PWM_OSC			1
#define SRC_CLK_PWM_RC			2
#define CLK_GLOBAL_SEL(v)		HIWORD_UPDATE(v, 15, 15)
/* INTSTS */
#define INTSTS				0x70
#define LEDC_TX_FRAME_INISTS_SHIFT	10
#define LEDC_TX_DONE_INISTS_SHIFT	11
#define LEDC_TX_FRAME_INT		BIT(LEDC_TX_FRAME_INISTS_SHIFT)
#define LEDC_TX_DONE_INT		BIT(LEDC_TX_DONE_INISTS_SHIFT)
/* INT_EN */
#define PWM_INT_EN			0x74
#define LEDC_TX_FRAME_INT_EN(v)		HIWORD_UPDATE(v, 10, 10)
#define LEDC_TX_DONE_INT_EN(v)		HIWORD_UPDATE(v, 11, 11)
/* FEATURE */
#define FEATURE				0x7c
/* GLOBAL_CTRL */
#define GLOBAL_CTRL			0xc4
#define GLOBAL_PWM_EN(v)		HIWORD_UPDATE(v, 0, 0)
#define GLOBAL_PWM_UPDATE_EN(v)		HIWORD_UPDATE(v, 1, 1)
/* FREQ_ARBITER */
#define FREQ_ARBITER			0x1c0
#define FREQ_GRANT_SHIFT		0
#define FREQ_READ_LOCK_SHIFT		16
/* FREQ_CTRL */
#define FREQ_CTRL			0x1c4
#define FREQ_EN(v)			HIWORD_UPDATE(v, 0, 0)
#define FREQ_CLK_SEL(v)			HIWORD_UPDATE(v, 1, 1)
#define FREQ_CHANNEL_SEL(v)		HIWORD_UPDATE(v, 3, 5)
#define FREQ_CLK_SWITCH_MODE(v)		HIWORD_UPDATE(v, 6, 6)
#define FREQ_TIMIER_CLK_SEL(v)		HIWORD_UPDATE(v, 7, 7)
/* LEDC_CTRL0 (hiword) */
#define LEDC_EN(v)		HIWORD_UPDATE(v, 0, 0)
#define LEDC_TX_MODE(v)		HIWORD_UPDATE(v, 2, 2)
#define LEDC_IDLE_POL(v)	HIWORD_UPDATE(v, 4, 4)
#define LEDC_TH_POL(v)		HIWORD_UPDATE(v, 5, 5)
#define LEDC_TL_POL(v)		HIWORD_UPDATE(v, 6, 6)
#define LEDC_REST_POL(v)	HIWORD_UPDATE(v, 7, 7)
/* DMA_CTRL0 (hiword) */
#define DMA_EN(v)		HIWORD_UPDATE(v, 0, 0)
#define FIFO_EN(v)		HIWORD_UPDATE(v, 1, 1)
#define DMA_REQ_MODE(v)		HIWORD_UPDATE(v, 5, 5)
/* LEDC_CTRL0 */
#define PWM_LEDC_CTRL0					0x60
#define PWM_LEDC_CTRL0_LEDC_EN_SHIFT			0
#define PWM_LEDC_CTRL0_LEDC_EN_MASK			(0x1U << PWM_LEDC_CTRL0_LEDC_EN_SHIFT)
#define PWM_LEDC_CTRL0_LEDC_FORCE_OUTPUT_EN_SHIFT	1
#define PWM_LEDC_CTRL0_LEDC_FORCE_OUTPUT_EN_MASK	(0x1U << PWM_LEDC_CTRL0_LEDC_FORCE_OUTPUT_EN_SHIFT)
#define PWM_LEDC_CTRL0_LEDC_TX_MODE_SHIFT		2
#define PWM_LEDC_CTRL0_LEDC_TX_MODE_MASK		(0x1U << PWM_LEDC_CTRL0_LEDC_TX_MODE_SHIFT)
#define PWM_LEDC_CTRL0_LEDC_STOP_MODE_SHIFT		3
#define PWM_LEDC_CTRL0_LEDC_STOP_MODE_MASK		(0x1U << PWM_LEDC_CTRL0_LEDC_STOP_MODE_SHIFT)
#define PWM_LEDC_CTRL0_LEDC_IDLE_POL_SHIFT		4
#define PWM_LEDC_CTRL0_LEDC_IDLE_POL_MASK		(0x1U << PWM_LEDC_CTRL0_LEDC_IDLE_POL_SHIFT)
#define PWM_LEDC_CTRL0_LEDC_TH_POL_SHIFT		5
#define PWM_LEDC_CTRL0_LEDC_TH_POL_MASK			(0x1U << PWM_LEDC_CTRL0_LEDC_TH_POL_SHIFT)
#define PWM_LEDC_CTRL0_LEDC_TL_POL_SHIFT		6
#define PWM_LEDC_CTRL0_LEDC_TL_POL_MASK			(0x1U << PWM_LEDC_CTRL0_LEDC_TL_POL_SHIFT)
#define PWM_LEDC_CTRL0_LEDC_RESET_POL_SHIFT		7
#define PWM_LEDC_CTRL0_LEDC_RESET_POL_MASK		(0x1U << PWM_LEDC_CTRL0_LEDC_RESET_POL_SHIFT)
#define PWM_LEDC_CTRL0_LEDC_RESET_N_SHIFT		8
#define PWM_LEDC_CTRL0_LEDC_RESET_N_MASK		(0x1U << PWM_LEDC_CTRL0_LEDC_RESET_N_SHIFT)
/* LEDC_CTRL1 */
#define PWM_LEDC_CTRL1					0x64
#define PWM_LEDC_CTRL1_LEDC_CYCLE_T0H_SHIFT		0
#define PWM_LEDC_CTRL1_LEDC_CYCLE_T0H_MASK		(0xFFU << PWM_LEDC_CTRL1_LEDC_CYCLE_T0H_SHIFT)
#define PWM_LEDC_CTRL1_LEDC_CYCLE_T0L_SHIFT		8
#define PWM_LEDC_CTRL1_LEDC_CYCLE_T0L_MASK		(0xFFU << PWM_LEDC_CTRL1_LEDC_CYCLE_T0L_SHIFT)
#define PWM_LEDC_CTRL1_LEDC_CYCLE_T1H_SHIFT		16
#define PWM_LEDC_CTRL1_LEDC_CYCLE_T1H_MASK		(0xFFU << PWM_LEDC_CTRL1_LEDC_CYCLE_T1H_SHIFT)
#define PWM_LEDC_CTRL1_LEDC_CYCLE_T1L_SHIFT		24
#define PWM_LEDC_CTRL1_LEDC_CYCLE_T1L_MASK		(0xFFU << PWM_LEDC_CTRL1_LEDC_CYCLE_T1L_SHIFT)
/* LEDC_CTRL2 */
#define PWM_LEDC_CTRL2					0x68
#define PWM_LEDC_CTRL2_LEDC_CYCLE_RESET_SHIFT		0
#define PWM_LEDC_CTRL2_LEDC_CYCLE_RESET_MASK		(0x7FFFU << PWM_LEDC_CTRL2_LEDC_CYCLE_RESET_SHIFT)
/* LEDC_CTRL3 */
#define PWM_LEDC_CTRL3					0x6c
#define PWM_LEDC_CTRL3_LEDC_BIT_NUM_SHIFT		0
#define PWM_LEDC_CTRL3_LEDC_BIT_NUM_MASK		(0x7FFFU << PWM_LEDC_CTRL3_LEDC_BIT_NUM_SHIFT)
#define PWM_LEDC_CTRL3_LEDC_FRAM_NUM_SHIFT		16
#define PWM_LEDC_CTRL3_LEDC_FRAM_NUM_MASK		(0x3FFU << PWM_LEDC_CTRL3_LEDC_FRAM_NUM_SHIFT)
/* DMA_CTRL0 */
#define PWM_DMA_CTRL0					0x240
#define PWM_DMA_CTRL0_DMA_EN_SHIFT			0
#define PWM_DMA_CTRL0_DMA_EN_MASK			(0x1U << PWM_DMA_CTRL0_DMA_EN_SHIFT)
#define PWM_DMA_CTRL0_FIFO_EN_SHIFT			1
#define PWM_DMA_CTRL0_FIFO_EN_MASK			(0x1U << PWM_DMA_CTRL0_FIFO_EN_SHIFT)
#define PWM_DMA_CTRL0_FIFO_MODE_SHIFT			2
#define PWM_DMA_CTRL0_FIFO_MODE_MASK			(0x1U << PWM_DMA_CTRL0_FIFO_MODE_SHIFT)
#define PWM_DMA_CTRL0_DMA_RX_SINGLE_BYPASS_SHIFT	3
#define PWM_DMA_CTRL0_DMA_RX_SINGLE_BYPASS_MASK		(0x1U << PWM_DMA_CTRL0_DMA_RX_SINGLE_BYPASS_SHIFT)
#define PWM_DMA_CTRL0_DMA_TIMEOUT_EN_SHIFT		4
#define PWM_DMA_CTRL0_DMA_TIMEOUT_EN_MASK		(0x1U << PWM_DMA_CTRL0_DMA_TIMEOUT_EN_SHIFT)
#define PWM_DMA_CTRL0_DMA_REQ_MODE_SHIFT		5
#define PWM_DMA_CTRL0_DMA_REQ_MODE_MASK			(0x1U << PWM_DMA_CTRL0_DMA_REQ_MODE_SHIFT)
/* DMA_CTRL1 */
#define PWM_DMA_CTRL1					0x244
#define PWM_DMA_CTRL1_FIFO_ALMOST_FULL_WATERMARK_SHIFT	0
#define PWM_DMA_CTRL1_FIFO_ALMOST_FULL_WATERMARK_MASK	(0x7U << PWM_DMA_CTRL1_FIFO_ALMOST_FULL_WATERMARK_SHIFT)
#define PWM_DMA_CTRL1_FIFO_ALMOST_EMPTY_WATERMARK_SHIFT	5
#define PWM_DMA_CTRL1_FIFO_ALMOST_EMPTY_WATERMARK_MASK	(0x1U << PWM_DMA_CTRL1_FIFO_ALMOST_EMPTY_WATERMARK_SHIFT)
#define PWM_DMA_CTRL1_DMA_TIMEROUT_THR_SHIFT		12
#define PWM_DMA_CTRL1_DMA_TIMEROUT_THR_MASK		(0xFFFFFU << PWM_DMA_CTRL1_DMA_TIMEROUT_THR_SHIFT)
/* DMA_CTRL2 */
#define PWM_DMA_CTRL2					0x248
#define PWM_DMA_CTRL2_DMA_DATA_NUM_SHIFT		0
#define PWM_DMA_CTRL2_DMA_DATA_NUM_MASK			(0x3FFFFFU << PWM_DMA_CTRL2_DMA_DATA_NUM_SHIFT)
/* DMA_INTSTS */
#define PWM_DMA_INTSTS					0x24c
#define PWM_DMA_INTSTS_FIFO_FULL_INTSTS_SHIFT		0
#define PWM_DMA_INTSTS_FIFO_FULL_INTSTS_MASK		(0x1U << PWM_DMA_INTSTS_FIFO_FULL_INTSTS_SHIFT)
#define PWM_DMA_INTSTS_FIFO_EMPTY_INTSTS_SHIFT		1
#define PWM_DMA_INTSTS_FIFO_EMPTY_INTSTS_MASK		(0x1U << PWM_DMA_INTSTS_FIFO_EMPTY_INTSTS_SHIFT)
#define PWM_DMA_INTSTS_FIFO_OVERFLOW_INTSTS_SHIFT	2
#define PWM_DMA_INTSTS_FIFO_OVERFLOW_INTSTS_MASK	(0x1U << PWM_DMA_INTSTS_FIFO_OVERFLOW_INTSTS_SHIFT)
#define PWM_DMA_INTSTS_FIFO_UNERFLOW_INTSTS_SHIFT	3
#define PWM_DMA_INTSTS_FIFO_UNERFLOW_INTSTS_MASK	(0x1U << PWM_DMA_INTSTS_FIFO_UNERFLOW_INTSTS_SHIFT)
#define PWM_DMA_INTSTS_FIFO_ALMOST_FULL_INTSTS_SHIFT	4
#define PWM_DMA_INTSTS_FIFO_ALMOST_FULL_INTSTS_MASK	(0x1U << PWM_DMA_INTSTS_FIFO_ALMOST_FULL_INTSTS_SHIFT)
#define PWM_DMA_INTSTS_FIFO_ALMOST_EMPTY_INTSTS_SHIFT	5
#define PWM_DMA_INTSTS_FIFO_ALMOST_EMPTY_INTSTS_MASK	(0x1U << PWM_DMA_INTSTS_FIFO_ALMOST_EMPTY_INTSTS_SHIFT)
#define PWM_DMA_INTSTS_DMA_TIMEROUT_INTSTS_SHIFT	6
#define PWM_DMA_INTSTS_DMA_TIMEROUT_INTSTS_MASK		(0x1U << PWM_DMA_INTSTS_DMA_TIMEROUT_INTSTS_SHIFT)
/* DMA_INT_EN */
#define PWM_DMA_INT_EN					0x250
#define PWM_DMA_INT_EN_FIFO_FULL_INT_EN_SHIFT		0
#define PWM_DMA_INT_EN_FIFO_FULL_INT_EN_MASK		(0x1U << PWM_DMA_INT_EN_FIFO_FULL_INT_EN_SHIFT)
#define PWM_DMA_INT_EN_FIFO_EMPTY_INT_EN_SHIFT		1
#define PWM_DMA_INT_EN_FIFO_EMPTY_INT_EN_MASK		(0x1U << PWM_DMA_INT_EN_FIFO_EMPTY_INT_EN_SHIFT)
#define PWM_DMA_INT_EN_FIFO_OVERFLOW_INT_EN_SHIFT	2
#define PWM_DMA_INT_EN_FIFO_OVERFLOW_INT_EN_MASK	(0x1U << PWM_DMA_INT_EN_FIFO_OVERFLOW_INT_EN_SHIFT)
#define PWM_DMA_INT_EN_FIFO_UNVERFLOW_INT_EN_SHIFT	3
#define PWM_DMA_INT_EN_FIFO_UNVERFLOW_INT_EN_MASK	(0x1U << PWM_DMA_INT_EN_FIFO_UNVERFLOW_INT_EN_SHIFT)
#define PWM_DMA_INT_EN_FIFO_ALMOST_FULL_INT_EN_SHIFT	4
#define PWM_DMA_INT_EN_FIFO_ALMOST_FULL_INT_EN_MASK	(0x1U << PWM_DMA_INT_EN_FIFO_ALMOST_FULL_INT_EN_SHIFT)
#define PWM_DMA_INT_EN_FIFO_ALMOST_EMPTY_INT_EN_SHIFT	5
#define PWM_DMA_INT_EN_FIFO_ALMOST_EMPTY_INT_EN_MASK	(0x1U << PWM_DMA_INT_EN_FIFO_ALMOST_EMPTY_INT_EN_SHIFT)
#define PWM_DMA_INT_EN_DMA_TIMEOUT_INT_EN_SHIFT		6
#define PWM_DMA_INT_EN_DMA_TIMEOUT_INT_EN_MASK		(0x1U << PWM_DMA_INT_EN_DMA_TIMEOUT_INT_EN_SHIFT)
/* DMA_INT_MASK */
#define PWM_DMA_INT_MASK				0x254
#define PWM_DMA_INT_MASK_FIFO_FULL_INT_MASK_SHIFT	0
#define PWM_DMA_INT_MASK_FIFO_FULL_INT_MASK_MASK	(0x1U << PWM_DMA_INT_MASK_FIFO_FULL_INT_MASK_SHIFT)
#define PWM_DMA_INT_MASK_FIFO_EMPTY_INT_MASK_SHIFT	1
#define PWM_DMA_INT_MASK_FIFO_EMPTY_INT_MASK_MASK	(0x1U << PWM_DMA_INT_MASK_FIFO_EMPTY_INT_MASK_SHIFT)
#define PWM_DMA_INT_MASK_FIFO_OVERFLOW_INT_MASK_SHIFT	2
#define PWM_DMA_INT_MASK_FIFO_OVERFLOW_INT_MASK_MASK	(0x1U << PWM_DMA_INT_MASK_FIFO_OVERFLOW_INT_MASK_SHIFT)
#define PWM_DMA_INT_MASK_FIFO_UNVERFLOW_INT_MASK_SHIFT	3
#define PWM_DMA_INT_MASK_FIFO_UNVERFLOW_INT_MASK_MASK	(0x1U << PWM_DMA_INT_MASK_FIFO_UNVERFLOW_INT_MASK_SHIFT)
#define PWM_DMA_INT_MASK_FIFO_ALMOST_FULL_INT_MASK_SHIFT	4
#define PWM_DMA_INT_MASK_FIFO_ALMOST_FULL_INT_MASK_MASK	(0x1U << PWM_DMA_INT_MASK_FIFO_ALMOST_FULL_INT_MASK_SHIFT)
#define PWM_DMA_INT_MASK_FIFO_ALMOST_EMPTY_INT_MASK_SHIFT	5
#define PWM_DMA_INT_MASK_FIFO_ALMOST_EMPTY_INT_MASK_MASK	(0x1U << PWM_DMA_INT_MASK_FIFO_ALMOST_EMPTY_INT_MASK_SHIFT)
#define PWM_DMA_INT_MASK_DMA_TIMEOUT_INT_MASK_SHIFT	6
#define PWM_DMA_INT_MASK_DMA_TIMEOUT_INT_MASK_MASK	(0x1U << PWM_DMA_INT_MASK_DMA_TIMEOUT_INT_MASK_SHIFT)
/* DMA_FIFO */
#define PWM_DMA_FIFO					0x258
#define PWM_DMA_FIFO_DMA_FIFO_SHIFT			0
#define PWM_DMA_FIFO_DMA_FIFO_MASK			(0x1U << PWM_DMA_FIFO_DMA_FIFO_SHIFT)
/* DMA_ST0 */
#define PWM_DMA_ST0					0x260
#define PWM_DMA_ST0_FIFO_EMPTY_SHIFT			0
#define PWM_DMA_ST0_FIFO_EMPTY_MASK			(0x1U << PWM_DMA_ST0_FIFO_EMPTY_SHIFT)
#define PWM_DMA_ST0_FIFO_FULL_SHIFT			1
#define PWM_DMA_ST0_FIFO_FULL_MASK			(0x1U << PWM_DMA_ST0_FIFO_FULL_SHIFT)
#define PWM_DMA_ST0_FIFO_ALMOST_EMPTY_SHIFT		2
#define PWM_DMA_ST0_FIFO_ALMOST_EMPTY_MASK		(0x1U << PWM_DMA_ST0_FIFO_ALMOST_EMPTY_SHIFT)
#define PWM_DMA_ST0_FIFO_ALMOST_FULL_SHIFT		3
#define PWM_DMA_ST0_FIFO_ALMOST_FULL_MASK		(0x1U << PWM_DMA_ST0_FIFO_ALMOST_FULL_SHIFT)
/* DMA_ST1 */
#define PWM_DMA_ST1					0x264
#define PWM_DMA_ST1_DMA_DATA_CNT_SHIFT			0
#define PWM_DMA_ST1_DMA_DATA_CNT_MASK			(0x1U << PWM_DMA_ST1_DMA_DATA_CNT_SHIFT)

#define PWM_MAX_CHANNEL_NUM	8

#define LEDC_FIFO_DEPTH 8	/* 8 * 4 bytes */

#define RK_LEDC_DATA_LENGTH_MAX		8192

#define RK_LEDC_T0L_MIN_NS		10
#define RK_LEDC_T0L_MAX_NS		25500
#define RK_LEDC_T0L_DEFAULT_NS		300

#define RK_LEDC_T0H_MIN_NS		10
#define RK_LEDC_T0H_MAX_NS		25500
#define RK_LEDC_T0H_DEFAULT_NS		600

#define RK_LEDC_T1L_MIN_NS		10
#define RK_LEDC_T1L_MAX_NS		25500
#define RK_LEDC_T1L_DEFAULT_NS		600

#define RK_LEDC_T1H_MIN_NS		10
#define RK_LEDC_T1H_MAX_NS		25500
#define RK_LEDC_T1H_DEFAULT_NS		300

#define RK_LEDC_RESET_TIME_MIN_NS	10
#define RK_LEDC_RESET_TIME_MAX_NS	3276700
#define RK_LEDC_RESET_TIME_DEFAULT_NS	80000

enum {
	RK_LEDC_XFER_TYPE_CPU,
	RK_LEDC_XFER_TYPE_DMA,
};

struct rk_ledc {
	struct device *dev;

	int irq;
	int channel_id;

	unsigned long clk_rate;
	struct clk *clk;
	struct clk *pclk;
	u32 scaler;
	u32 cycle_ns;

	void __iomem *base;
	struct resource *res;

	struct pinctrl *pinctrl;
	struct pinctrl_state *active_state;

	dma_addr_t src_dma;
	struct dma_chan *dma_chan;

	u8 *tx_buf;
	u32 tx_len;

	struct rk_ledc_config cfg;
	bool timing_cfg_update;
	bool xfer_cfg_update;

	struct completion completion;
	struct mutex mutex_lock;

	struct dentry *debugfs_dir;
	struct miscdevice miscdev;
};

static int rk_ledc_validate_config(const struct rk_ledc_config *config)
{
	u32 pol;

	if (config->timing.t0h_ns < RK_LEDC_T0H_MIN_NS ||
	    config->timing.t0h_ns > RK_LEDC_T0H_MAX_NS ||
	    config->timing.t0l_ns < RK_LEDC_T0L_MIN_NS ||
	    config->timing.t0l_ns > RK_LEDC_T0L_MAX_NS ||
	    config->timing.t1h_ns < RK_LEDC_T1H_MIN_NS ||
	    config->timing.t1h_ns > RK_LEDC_T1H_MAX_NS ||
	    config->timing.t1l_ns < RK_LEDC_T1L_MIN_NS ||
	    config->timing.t1l_ns > RK_LEDC_T1L_MAX_NS ||
	    config->timing.reset_ns < RK_LEDC_RESET_TIME_MIN_NS ||
	    config->timing.reset_ns > RK_LEDC_RESET_TIME_MAX_NS)
		return -EINVAL;

	if (config->flags & RK_LEDC_CONFIG_UNKNOWN_MASK)
		return -EINVAL;

	pol = RK_LEDC_POL_GET(config->flags);
	if (pol != RK_LEDC_POL_NORMAL && pol != RK_LEDC_POL_REVERSAL)
		return -EINVAL;
	return 0;
}

static irqreturn_t rk_ledc_irq_handler(int irq, void *data)
{
	struct rk_ledc *ledc = (struct rk_ledc *)data;
	int val;
	irqreturn_t ret = IRQ_NONE;

	val = readl(ledc->base + INTSTS);

	if (val & LEDC_TX_FRAME_INT) {
		writel(LEDC_TX_FRAME_INT, ledc->base + INTSTS);
		ret = IRQ_HANDLED;
	}

	if (val & LEDC_TX_DONE_INT) {
		writel(LEDC_TX_DONE_INT, ledc->base + INTSTS);
		complete(&ledc->completion);
		ret = IRQ_HANDLED;
	}

	return ret;
}

static int rk_ledc_timing_config_check(struct rk_ledc *ledc, const struct rk_ledc_config *config)
{
	u64 clk_rate_kHz, val = 1000000000ULL;
	u32 single_cycle_ns;
	u32 scaler;

	ledc->clk_rate = clk_get_rate(ledc->clk);
	clk_rate_kHz = ledc->clk_rate / 1000;

	if (!config->timing.t0h_ns || !config->timing.t0l_ns ||
	    !config->timing.t1h_ns || !config->timing.t1l_ns ||
	    !config->timing.reset_ns) {
		return -EINVAL;
	}

	/*
	 * find the smallest single_cycle_ns that can satisfy the timing requirements
	 * For Ledc application scenarios, a single cycle between 10ns-100ns is typical,
	 * so we start from 10ns and increase by 10ns each time
	 */
	for (single_cycle_ns = 10; single_cycle_ns < 100; single_cycle_ns += 10) {
		if (config->timing.t0h_ns / single_cycle_ns >
		    (PWM_LEDC_CTRL1_LEDC_CYCLE_T0H_MASK >>
		     PWM_LEDC_CTRL1_LEDC_CYCLE_T0H_SHIFT) ||
		    config->timing.t0l_ns / single_cycle_ns >
		    (PWM_LEDC_CTRL1_LEDC_CYCLE_T0L_MASK >>
		     PWM_LEDC_CTRL1_LEDC_CYCLE_T0L_SHIFT) ||
		    config->timing.t1h_ns / single_cycle_ns >
		    (PWM_LEDC_CTRL1_LEDC_CYCLE_T1H_MASK >>
		     PWM_LEDC_CTRL1_LEDC_CYCLE_T1H_SHIFT) ||
		    config->timing.t1l_ns / single_cycle_ns >
		    (PWM_LEDC_CTRL1_LEDC_CYCLE_T1L_MASK >>
		     PWM_LEDC_CTRL1_LEDC_CYCLE_T1L_SHIFT) ||
		    config->timing.reset_ns / single_cycle_ns >
		    PWM_LEDC_CTRL2_LEDC_CYCLE_RESET_MASK) {
			continue;
		}
		/*
		 * When we find a set of single_cycle_ns values that meet the timing
		 * requirements, we also need to check if the current ledc->clk_rate
		 * can generate such a cycle.
		 */
		do_div(val, single_cycle_ns);
		if (val > ledc->clk_rate)
			continue;

		/* Calculate scaler coefficient based on clk_rate_kHz and single_cycle_ns */
		scaler = DIV_ROUND_DOWN_ULL(clk_rate_kHz * (u64)single_cycle_ns, 2000000ULL);
		if (scaler > 256)
			return -EINVAL;
		break;
	}

	if (single_cycle_ns >= 100)
		return -EINVAL;

	ledc->scaler = scaler;
	ledc->cycle_ns = single_cycle_ns;

	return 0;
}

static int rk_ledc_timing_config(struct rk_ledc *ledc, const struct rk_ledc_config *config)
{
	u8 t0h_cycle, t0l_cycle, t1h_cycle, t1l_cycle;
	u16 treset_cycle;
	u32 pol;

	writel(CLK_PRESCALE(0), ledc->base + CLK_CTRL);
	writel(CLK_SCALE(ledc->scaler), ledc->base + CLK_CTRL);

	t0h_cycle = (config->timing.t0h_ns / ledc->cycle_ns) > 0 ?
		    (config->timing.t0h_ns / ledc->cycle_ns) : 1;
	t0l_cycle = (config->timing.t0l_ns / ledc->cycle_ns) > 0 ?
		    (config->timing.t0l_ns / ledc->cycle_ns) : 1;
	t1h_cycle = (config->timing.t1h_ns / ledc->cycle_ns) > 0 ?
		    (config->timing.t1h_ns / ledc->cycle_ns) : 1;
	t1l_cycle = (config->timing.t1l_ns / ledc->cycle_ns) > 0 ?
		    (config->timing.t1l_ns / ledc->cycle_ns) : 1;
	treset_cycle = (config->timing.reset_ns / ledc->cycle_ns) > 0 ?
		       (config->timing.reset_ns / ledc->cycle_ns) : 1;

	writel(((t0h_cycle - 1) << PWM_LEDC_CTRL1_LEDC_CYCLE_T0H_SHIFT) |
			((t0l_cycle - 1) << PWM_LEDC_CTRL1_LEDC_CYCLE_T0L_SHIFT) |
			((t1h_cycle - 1) << PWM_LEDC_CTRL1_LEDC_CYCLE_T1H_SHIFT) |
			((t1l_cycle - 1) << PWM_LEDC_CTRL1_LEDC_CYCLE_T1L_SHIFT),
			ledc->base + PWM_LEDC_CTRL1);

	writel((treset_cycle - 1) << PWM_LEDC_CTRL2_LEDC_CYCLE_RESET_SHIFT,
	       ledc->base + PWM_LEDC_CTRL2);

	if (config->flags & RK_LEDC_CONFIG_UNKNOWN_MASK)
		return -EINVAL;

	pol = RK_LEDC_POL_GET(config->flags);
	if (pol == RK_LEDC_POL_NORMAL) {
		writel(LEDC_IDLE_POL(0) | LEDC_TH_POL(1) |
		       LEDC_TL_POL(0) | LEDC_REST_POL(0),
		       ledc->base + PWM_LEDC_CTRL0);
	} else {
		writel(LEDC_IDLE_POL(1) | LEDC_TH_POL(0) |
			LEDC_TL_POL(1) | LEDC_REST_POL(1), ledc->base + PWM_LEDC_CTRL0);
	}

	return 0;
}

static int rk_ledc_xfer_prepare(struct rk_ledc *ledc, u32 bit_num, u32 frame_num)
{
	u32 tmp, data_total_word_num;

	if (!bit_num || !frame_num)
		return -EINVAL;

	tmp = readl(ledc->base + PWM_LEDC_CTRL3);
	tmp = (tmp & ~PWM_LEDC_CTRL3_LEDC_BIT_NUM_MASK) |
	      ((bit_num - 1) << PWM_LEDC_CTRL3_LEDC_BIT_NUM_SHIFT);
	tmp = (tmp & ~PWM_LEDC_CTRL3_LEDC_FRAM_NUM_MASK) |
	      ((frame_num - 1) << PWM_LEDC_CTRL3_LEDC_FRAM_NUM_SHIFT);
	writel(tmp, ledc->base + PWM_LEDC_CTRL3);

	data_total_word_num = DIV_ROUND_UP(bit_num * frame_num, 32);

	/* Adjust watermark */
	if (!data_total_word_num) {
		writel(0x07 | (0x4 << 5), ledc->base + PWM_DMA_CTRL1);
		data_total_word_num = 1;
	} else {
		writel(0x06 | (0x4 << 5), ledc->base + PWM_DMA_CTRL1);
	}

	tmp = readl(ledc->base + PWM_DMA_CTRL2);
	tmp = (tmp & ~PWM_DMA_CTRL2_DMA_DATA_NUM_MASK) |
		((data_total_word_num - 1) <<
		 PWM_DMA_CTRL2_DMA_DATA_NUM_SHIFT);
	writel(tmp, ledc->base + PWM_DMA_CTRL2);

	return 0;
}

static int rk_ledc_xfer_enable(struct rk_ledc *ledc, int xfer_type)
{
	if (xfer_type == RK_LEDC_XFER_TYPE_DMA)
		writel(DMA_EN(1) | FIFO_EN(1) | DMA_REQ_MODE(0), ledc->base + PWM_DMA_CTRL0);
	else if (xfer_type == RK_LEDC_XFER_TYPE_CPU)
		writel(DMA_EN(0) | FIFO_EN(1) | DMA_REQ_MODE(0), ledc->base + PWM_DMA_CTRL0);
	else
		return -EINVAL;

	writel(LEDC_TX_DONE_INT_EN(1), ledc->base + PWM_INT_EN);
	writel(PWM_CLK_EN(1), ledc->base + ENABLE);
	writel(LEDC_EN(1) | LEDC_TX_MODE(0), ledc->base + PWM_LEDC_CTRL0);

	return 0;
}

static int rk_ledc_xfer_disable(struct rk_ledc *ledc)
{
	writel(LEDC_TX_DONE_INT_EN(0), ledc->base + PWM_INT_EN);
	writel(LEDC_EN(0), ledc->base + PWM_LEDC_CTRL0);
	writel(PWM_CLK_EN(0), ledc->base + ENABLE);

	return 0;
}

static u32 rk_ledc_calculate_xfer_timeout(struct rk_ledc *ledc)
{
	u32 t0h_cycle, t0l_cycle, t1h_cycle, t1l_cycle, treset_cycle;
	u32 bit_num, frame_num, total_bits;
	u32 single_cycle_ns, max_code_time_cycle;
	u32 timeout_ms, tmp;

	tmp = readl(ledc->base + PWM_LEDC_CTRL1);

	t0h_cycle = ((tmp & PWM_LEDC_CTRL1_LEDC_CYCLE_T0H_MASK) >>
		     PWM_LEDC_CTRL1_LEDC_CYCLE_T0H_SHIFT) + 1;
	t0l_cycle = ((tmp & PWM_LEDC_CTRL1_LEDC_CYCLE_T0L_MASK) >>
		     PWM_LEDC_CTRL1_LEDC_CYCLE_T0L_SHIFT) + 1;
	t1h_cycle = ((tmp & PWM_LEDC_CTRL1_LEDC_CYCLE_T1H_MASK) >>
		     PWM_LEDC_CTRL1_LEDC_CYCLE_T1H_SHIFT) + 1;
	t1l_cycle = ((tmp & PWM_LEDC_CTRL1_LEDC_CYCLE_T1L_MASK) >>
		     PWM_LEDC_CTRL1_LEDC_CYCLE_T1L_SHIFT) + 1;

	tmp = readl(ledc->base + PWM_LEDC_CTRL2);
	treset_cycle = ((tmp & PWM_LEDC_CTRL2_LEDC_CYCLE_RESET_MASK) >>
			PWM_LEDC_CTRL2_LEDC_CYCLE_RESET_SHIFT) + 1;

	tmp = readl(ledc->base + PWM_LEDC_CTRL3);
	bit_num = ((tmp & PWM_LEDC_CTRL3_LEDC_BIT_NUM_MASK) >>
		   PWM_LEDC_CTRL3_LEDC_BIT_NUM_SHIFT) + 1;
	frame_num = ((tmp & PWM_LEDC_CTRL3_LEDC_FRAM_NUM_MASK) >>
		     PWM_LEDC_CTRL3_LEDC_FRAM_NUM_SHIFT) + 1;

	total_bits = bit_num * frame_num;

	if (ledc->scaler)
		single_cycle_ns = DIV_ROUND_UP(ledc->scaler * 2 * 1000000,
					       ledc->clk_rate / 1000);
	else
		single_cycle_ns = DIV_ROUND_UP(1000000, ledc->clk_rate / 1000);

	max_code_time_cycle = ((t0h_cycle + t0l_cycle) > (t1h_cycle + t1l_cycle) ?
			(t0h_cycle + t0l_cycle) : (t1h_cycle + t1l_cycle));

	timeout_ms = DIV_ROUND_UP((total_bits * max_code_time_cycle +
				   treset_cycle) * single_cycle_ns,
				  1000000);
	/* some tolerance */
	timeout_ms += 5;

	dev_dbg(ledc->dev, "timeout_ms: %u ms\n", timeout_ms);

	return timeout_ms;
}

static int rk_ledc_dma_request(struct rk_ledc *ledc)
{
	int ret;
	struct dma_slave_config slave_config;

	ledc->dma_chan = dma_request_chan(ledc->dev, "ledc-tx");
	if (IS_ERR(ledc->dma_chan)) {
		ret = PTR_ERR(ledc->dma_chan);
		dev_err(ledc->dev, "failed to get the DMA channel(%d)\n", ret);
		return ret;
	}

	slave_config.dst_addr = (phys_addr_t)(ledc->res->start + PWM_DMA_FIFO);
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	slave_config.dst_maxburst = 4;
	slave_config.direction = DMA_MEM_TO_DEV;
	slave_config.device_fc = false;

	ret = dmaengine_slave_config(ledc->dma_chan, &slave_config);
	if (ret) {
		dev_err(ledc->dev, "dmaengine_slave_config failed(%d)\n", ret);
		dma_release_channel(ledc->dma_chan);
		return ret;
	}

	return 0;
}

static void rk_ledc_dma_cleanup(struct rk_ledc *ledc)
{
	dmaengine_terminate_all(ledc->dma_chan);
	dma_unmap_single(ledc->dev, ledc->src_dma, ledc->tx_len, DMA_TO_DEVICE);
}

static void rk_ledc_dma_callback(void *param)
{
	struct rk_ledc *ledc = (struct rk_ledc *)param;

	dma_unmap_single(ledc->dev, ledc->src_dma, ledc->tx_len, DMA_TO_DEVICE);
}

static int rk_ledc_dma_config(struct rk_ledc *ledc)
{
	struct dma_async_tx_descriptor *dma_desc;

	ledc->src_dma = dma_map_single(ledc->dev, ledc->tx_buf,
			ledc->tx_len, DMA_TO_DEVICE);
	if (dma_mapping_error(ledc->dev, ledc->src_dma)) {
		dev_err(ledc->dev, "DMA mapping failed\n");
		goto err_map;
	}

	dma_desc = dmaengine_prep_slave_single(ledc->dma_chan,
				ledc->src_dma, ledc->tx_len, DMA_MEM_TO_DEV,
				DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		dev_err(ledc->dev, "Not able to get desc for DMA xfer\n");
		goto err_desc;
	}

	dma_desc->callback = rk_ledc_dma_callback;
	dma_desc->callback_param = ledc;
	if (dma_submit_error(dmaengine_submit(dma_desc))) {
		dev_err(ledc->dev, "DMA submit failed\n");
		goto err_submit;
	}

	dma_async_issue_pending(ledc->dma_chan);
	return 0;

err_submit:
	dmaengine_terminate_sync(ledc->dma_chan);
err_desc:
	dma_unmap_single(ledc->dev, ledc->src_dma, ledc->tx_len, DMA_TO_DEVICE);
err_map:
	return -EINVAL;
}

static int rk_ledc_cpu_xfer(struct rk_ledc *ledc)
{
	int i;
	u32 timeout_ms, val = 0;

	timeout_ms = rk_ledc_calculate_xfer_timeout(ledc);

	rk_ledc_xfer_enable(ledc, RK_LEDC_XFER_TYPE_CPU);

	for (i = 0; i < ledc->tx_len; i++) {
		val |= (u32)ledc->tx_buf[i] << ((i & 3) * 8);
		if ((i & 3) == 3) {
			writel(val, ledc->base + PWM_DMA_FIFO);
			val = 0;
		}
	}
	/* Flush remaining bytes (pad with zeros) */
	if (ledc->tx_len & 3)
		writel(val, ledc->base + PWM_DMA_FIFO);

	if (!wait_for_completion_timeout(&ledc->completion, msecs_to_jiffies(timeout_ms))) {
		rk_ledc_xfer_disable(ledc);
		dev_err(ledc->dev, "ledc transfer data timeout\n");
		return -ETIMEDOUT;
	}

	rk_ledc_xfer_disable(ledc);

	return 0;
}

static int rk_ledc_dma_xfer(struct rk_ledc *ledc)
{
	int ret;
	u32 timeout_ms;

	timeout_ms = rk_ledc_calculate_xfer_timeout(ledc);

	rk_ledc_xfer_enable(ledc, RK_LEDC_XFER_TYPE_DMA);

	ret = rk_ledc_dma_config(ledc);
	if (ret)
		return ret;

	if (!wait_for_completion_timeout(&ledc->completion, msecs_to_jiffies(timeout_ms))) {
		rk_ledc_xfer_disable(ledc);
		rk_ledc_dma_cleanup(ledc);
		dev_err(ledc->dev, "ledc transfer data timeout\n");
		return -ETIMEDOUT;
	}

	rk_ledc_xfer_disable(ledc);

	return 0;
}

static int rk_ledc_xfer_common(struct rk_ledc *ledc)
{
	int ret;

	ret = clk_enable(ledc->clk);
	if (ret)
		return ret;
	ret = clk_enable(ledc->pclk);
	if (ret)
		goto err_disable_clk;

	if (ledc->timing_cfg_update) {
		ledc->timing_cfg_update = false;
		ret = rk_ledc_timing_config(ledc, &ledc->cfg);
		if (ret) {
			dev_err(ledc->dev, "ledc timing config update failed!\n");
			goto err_disable_pclk;
		}
	}

	if (ledc->xfer_cfg_update) {
		ledc->xfer_cfg_update = false;
		ret = rk_ledc_xfer_prepare(ledc, ledc->tx_len * 8, 1);
		if (ret) {
			dev_err(ledc->dev, "ledc xfer prepare failed!\n");
			goto err_disable_pclk;
		}
	}

	if (ledc->tx_len <= LEDC_FIFO_DEPTH * 4)
		ret = rk_ledc_cpu_xfer(ledc);
	else
		ret = rk_ledc_dma_xfer(ledc);

err_disable_pclk:
	clk_disable(ledc->pclk);
err_disable_clk:
	clk_disable(ledc->clk);

	return ret;
}

static int rk_ledc_open(struct inode *inode, struct file *file)
{
	struct rk_ledc *ledc = container_of(file->private_data,
			struct rk_ledc, miscdev);

	file->private_data = ledc;
	return 0;
}

static const char *rk_ledc_pol_str(u32 flags)
{
	u32 pol;

	if (flags & RK_LEDC_CONFIG_UNKNOWN_MASK)
		return "unknown";

	pol = RK_LEDC_POL_GET(flags);
	switch (pol) {
	case RK_LEDC_POL_NORMAL:
		return "normal";
	case RK_LEDC_POL_REVERSAL:
		return "reversal";
	default:
		return "unknown";
	}
}

static int rk_ledc_format_config(char *buf, size_t size,
		const struct rk_ledc_config *cfg)
{

	return scnprintf(buf, size,
			 "t0h:%u, t0l:%u, t1h:%u, t1l:%u, reset:%u, pol:%s\n",
			 cfg->timing.t0h_ns,
			 cfg->timing.t0l_ns,
			 cfg->timing.t1h_ns,
			 cfg->timing.t1l_ns,
			 cfg->timing.reset_ns,
			 rk_ledc_pol_str(cfg->flags));
}

static ssize_t rk_ledc_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int r;
	char kbuf[64];
	struct rk_ledc *ledc = file->private_data;

	r = rk_ledc_format_config(kbuf, sizeof(kbuf), &ledc->cfg);

	return simple_read_from_buffer(buf, count, ppos, kbuf, r);
}

static ssize_t rk_ledc_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int ret;
	struct rk_ledc *ledc = file->private_data;

	if (count == 0)
		return 0;
	if (count > RK_LEDC_DATA_LENGTH_MAX)
		return -EINVAL;

	if (!ledc->tx_buf)
		return -ENOMEM;

	mutex_lock(&ledc->mutex_lock);
	if (copy_from_user(ledc->tx_buf, buf, count)) {
		ret = -EFAULT;
		mutex_unlock(&ledc->mutex_lock);
		return ret;
	}

	if (ledc->tx_len != count) {
		ledc->tx_len = count;
		ledc->xfer_cfg_update = true;
	}

	ret = rk_ledc_xfer_common(ledc);
	if (ret) {
		mutex_unlock(&ledc->mutex_lock);
		return ret;
	}

	mutex_unlock(&ledc->mutex_lock);

	return count;
}

static long rk_ledc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	struct rk_ledc_config cfg;
	struct rk_ledc *ledc = file->private_data;

	switch (cmd) {
	case LEDC_IOC_SET_TIMING_CONFIG:
		if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg)))
			return -EFAULT;

		ret = rk_ledc_validate_config(&cfg);
		if (ret) {
			dev_err(ledc->dev, "invalid parameter\n");
			return ret;
		}

		mutex_lock(&ledc->mutex_lock);
		if (!rk_ledc_timing_config_check(ledc, &cfg)) {
			ledc->cfg = cfg;
			ledc->timing_cfg_update = true;
		}
		mutex_unlock(&ledc->mutex_lock);
		break;

	case LEDC_IOC_GET_TIMING_CONFIG:
		mutex_lock(&ledc->mutex_lock);
		cfg = ledc->cfg;
		mutex_unlock(&ledc->mutex_lock);

		if (copy_to_user((void __user *)arg, &cfg, sizeof(cfg)))
			return -EFAULT;
		break;

	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static const struct file_operations rk_ledc_fops = {
	.owner		= THIS_MODULE,
	.open		= rk_ledc_open,
	.read		= rk_ledc_read,
	.write		= rk_ledc_write,
	.unlocked_ioctl	= rk_ledc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = rk_ledc_ioctl,
#endif
};

static int rk_ledc_config_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t rk_ledc_config_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offp)
{
	int i, parsed;
	char kbuf[64];
	struct rk_ledc_config cfg;
	struct rk_ledc *ledc = file->private_data;

	if (count >= sizeof(kbuf))
		goto err_out;

	if (copy_from_user(kbuf, buf, count))
		goto err_out;

	kbuf[count] = '\0';

	/* allow comma-separated input: "a,b,c,d,e,f" */
	for (i = 0; kbuf[i]; i++) {
		if (kbuf[i] == ',')
			kbuf[i] = ' ';
	}

	parsed = sscanf(kbuf, "%hu %hu %hu %hu %u %u",
			&cfg.timing.t0h_ns, &cfg.timing.t0l_ns,
			&cfg.timing.t1h_ns, &cfg.timing.t1l_ns,
			&cfg.timing.reset_ns, &cfg.flags);
	if (parsed != 6)
		goto err_out;

	/* Validate timing parameters */
	i = rk_ledc_validate_config(&cfg);
	if (i) {
		dev_err(ledc->dev, "invalid parameter\n");
		goto err_out;
	}

	mutex_lock(&ledc->mutex_lock);
	if (!rk_ledc_timing_config_check(ledc, &cfg)) {
		ledc->cfg = cfg;
		ledc->timing_cfg_update = true;
	}
	mutex_unlock(&ledc->mutex_lock);

	*offp += count;

	return count;

err_out:

	return -EINVAL;
}

static ssize_t rk_ledc_config_read(struct file *file, char __user *buf,
		size_t count, loff_t *offp)
{
	int r;
	char kbuf[64];
	struct rk_ledc *ledc = file->private_data;

	r = rk_ledc_format_config(kbuf, sizeof(kbuf), &ledc->cfg);

	return simple_read_from_buffer(buf, count, offp, kbuf, r);
}

static const struct file_operations rk_ledc_config_fops = {
	.owner = THIS_MODULE,
	.open = rk_ledc_config_open,
	.write = rk_ledc_config_write,
	.read  = rk_ledc_config_read,
};

static void rk_ledc_create_debugfs(struct rk_ledc *ledc)
{
	struct dentry *debugfs_dir, *debugfs_file;
	char name[16];

	snprintf(name, sizeof(name), "ledc-%d", ledc->channel_id);

	debugfs_dir = debugfs_create_dir(name, NULL);
	if (IS_ERR_OR_NULL(debugfs_dir)) {
		dev_err(ledc->dev, "debugfs_create_dir failed!\n");
		return;
	}

	ledc->debugfs_dir = debugfs_dir;

	debugfs_file = debugfs_create_file("config", 0660,
			debugfs_dir, ledc, &rk_ledc_config_fops);
	if (!debugfs_file)
		dev_err(ledc->dev, "debugfs_create_file for config failed!\n");
}

static void rk_ledc_remove_debugfs(struct rk_ledc *ledc)
{
	debugfs_remove_recursive(ledc->debugfs_dir);
}

static const struct of_device_id rk_ledc_dt_ids[] = {
	{.compatible = "rockchip,rk3538-pwm-ledc"},
	{},
};

MODULE_DEVICE_TABLE(of, rk_ledc_dt_ids);

static void rk_ledc_parse_dt(struct rk_ledc *ledc)
{
	u32 val;

	if (!of_property_read_u32(ledc->dev->of_node, "rockchip,reset-ns", &val))
		ledc->cfg.timing.reset_ns = val;
	else
		ledc->cfg.timing.reset_ns = RK_LEDC_RESET_TIME_DEFAULT_NS;

	if (!of_property_read_u32(ledc->dev->of_node, "rockchip,t0l-ns", &val))
		ledc->cfg.timing.t0l_ns = val;
	else
		ledc->cfg.timing.t0l_ns = RK_LEDC_T0L_DEFAULT_NS;

	if (!of_property_read_u32(ledc->dev->of_node, "rockchip,t0h-ns", &val))
		ledc->cfg.timing.t0h_ns = val;
	else
		ledc->cfg.timing.t0h_ns = RK_LEDC_T0H_DEFAULT_NS;

	if (!of_property_read_u32(ledc->dev->of_node, "rockchip,t1l-ns", &val))
		ledc->cfg.timing.t1l_ns = val;
	else
		ledc->cfg.timing.t1l_ns = RK_LEDC_T1L_DEFAULT_NS;

	if (!of_property_read_u32(ledc->dev->of_node, "rockchip,t1h-ns", &val))
		ledc->cfg.timing.t1h_ns = val;
	else
		ledc->cfg.timing.t1h_ns = RK_LEDC_T1H_DEFAULT_NS;

	if (!of_property_read_u32(ledc->dev->of_node, "rockchip,pol-pattern", &val))
		ledc->cfg.flags = RK_LEDC_POL_SET(val);
	else
		ledc->cfg.flags = RK_LEDC_POL_SET(RK_LEDC_POL_NORMAL);

	dev_info(ledc->dev,
		"dt config: t0h=%u t0l=%u t1h=%u t1l=%u reset=%u pol=%s\n",
		ledc->cfg.timing.t0h_ns, ledc->cfg.timing.t0l_ns, ledc->cfg.timing.t1h_ns,
		ledc->cfg.timing.t1l_ns, ledc->cfg.timing.reset_ns,
		rk_ledc_pol_str(ledc->cfg.flags));
}

static int rk_ledc_probe(struct platform_device *pdev)
{
	struct rk_ledc *ledc;
	int ret;
	u32 feature;

	ledc = devm_kzalloc(&pdev->dev, sizeof(*ledc), GFP_KERNEL);
	if (!ledc)
		return -ENOMEM;

	ledc->dev = &pdev->dev;

	ledc->tx_buf = devm_kzalloc(&pdev->dev, RK_LEDC_DATA_LENGTH_MAX, GFP_KERNEL);
	if (!ledc->tx_buf)
		return -ENOMEM;

	ledc->tx_len = 0;

	ledc->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	ledc->base = devm_ioremap_resource(&pdev->dev, ledc->res);
	if (IS_ERR(ledc->base))
		return PTR_ERR(ledc->base);

	rk_ledc_parse_dt(ledc);

	ledc->clk = devm_clk_get_enabled(&pdev->dev, "pwm");
	if (IS_ERR(ledc->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(ledc->clk),
			"Can't get PWM clk\n");

	ledc->pclk = devm_clk_get_optional_enabled(&pdev->dev, "pclk");
	if (IS_ERR(ledc->pclk))
		return dev_err_probe(&pdev->dev, PTR_ERR(ledc->pclk),
			"Can't get APB clk\n");

	if (!ledc->pclk)
		ledc->pclk = ledc->clk;

	ledc->clk_rate = clk_get_rate(ledc->clk);

	ledc->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ledc->pinctrl)) {
		dev_err(&pdev->dev, "Get pinctrl failed!\n");
		ret = PTR_ERR(ledc->pinctrl);
		goto err_out;
	}

	ledc->active_state = pinctrl_lookup_state(ledc->pinctrl, "active");
	if (IS_ERR(ledc->active_state)) {
		dev_err(&pdev->dev, "No active pinctrl state\n");
		ret = PTR_ERR(ledc->active_state);
		goto err_out;
	}
	ret = pinctrl_select_state(ledc->pinctrl, ledc->active_state);
	if (ret) {
		dev_err(&pdev->dev, "Failed to select active pinctrl state: %d\n", ret);
		goto err_out;
	}

	platform_set_drvdata(pdev, ledc);

	init_completion(&ledc->completion);
	mutex_init(&ledc->mutex_lock);

	ledc->irq = platform_get_irq(pdev, 0);
	if (ledc->irq < 0) {
		ret = ledc->irq;
		goto err_out;
	}

	ret = devm_request_irq(&pdev->dev, ledc->irq, rk_ledc_irq_handler,
			IRQF_NO_SUSPEND, "pwm_ledc", ledc);
	if (ret) {
		dev_err(&pdev->dev, "Claim IRQ failed\n");
		goto err_out;
	}

	ret = rk_ledc_dma_request(ledc);
	if (ret)
		goto err_out;

	feature = readl(ledc->base + FEATURE);
	ledc->channel_id = (feature & CHANNLE_INDEX_MASK) >> CHANNLE_INDEX_SHIFT;
	if (ledc->channel_id < 0 || ledc->channel_id >= PWM_MAX_CHANNEL_NUM) {
		dev_err(&pdev->dev, "Channel id is out of range: %d\n", ledc->channel_id);
		ret = -EINVAL;
		goto err_dma_chan;
	}

	/* Register misc device */
	ledc->miscdev.minor = MISC_DYNAMIC_MINOR;
	ledc->miscdev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "ledc-%d", ledc->channel_id);
	ledc->miscdev.fops = &rk_ledc_fops;
	ledc->miscdev.parent = ledc->dev;
	ret = misc_register(&ledc->miscdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register misc device: %d\n", ret);
		goto err_dma_chan;
	}

	rk_ledc_create_debugfs(ledc);

	if (!rk_ledc_timing_config_check(ledc, &ledc->cfg))
		ledc->timing_cfg_update = true;

	clk_disable(ledc->clk);
	clk_disable(ledc->pclk);

	return 0;

err_dma_chan:
	dma_release_channel(ledc->dma_chan);
err_out:
	return ret;
}

static int rk_ledc_remove(struct platform_device *pdev)
{
	struct rk_ledc *ledc = platform_get_drvdata(pdev);
	int ret;

	if (!ledc)
		return 0;

	misc_deregister(&ledc->miscdev);

	rk_ledc_remove_debugfs(ledc);

	ret = clk_enable(ledc->clk);
	if (ret)
		dev_warn(ledc->dev, "failed to enable clk during remove: %d\n", ret);

	ret = clk_enable(ledc->pclk);
	if (ret)
		dev_warn(ledc->dev, "failed to enable pclk during remove: %d\n", ret);

	rk_ledc_xfer_disable(ledc);

	if (!IS_ERR_OR_NULL(ledc->dma_chan)) {
		dmaengine_terminate_sync(ledc->dma_chan);
		dma_release_channel(ledc->dma_chan);
		ledc->dma_chan = NULL;
	}

	return 0;
}

static struct platform_driver rk_ledc_driver = {
	.probe		= rk_ledc_probe,
	.remove		= rk_ledc_remove,
	.driver		= {
		.name	= "rk-ledc",
		.owner	= THIS_MODULE,
		.of_match_table = rk_ledc_dt_ids,
	},
};

module_platform_driver(rk_ledc_driver);

MODULE_ALIAS("rockchip ledc driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
MODULE_AUTHOR("Eddy Zhang <eddy.zhang@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip ledc driver");
