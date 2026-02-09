// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk/rockchip.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define CRU_IODET_CON0				0x00
#define PHASE_SYNC_EN_SHIFT			(0U)
#define PHASE_SYNC_EN_MASK			(0x1U << PHASE_SYNC_EN_SHIFT)
#define CLKEN_SHIFT				(1U)
#define CLKEN_MASK				(0x1U << CLKEN_SHIFT)
#define START_SHIFT				(2U)
#define START_MASK				(0x1U << START_SHIFT)
#define INT_EN_SHIFT				(3U)
#define INT_EN_MASK				(0x1U << INT_EN_SHIFT)
#define INT_CLEAR_SHIFT				(4U)
#define INT_CLEAR_MASK				(0x1U << INT_CLEAR_SHIFT)
#define PLL_NO_STALL_SHIFT			(5U)
#define PLL_NO_STALL_MASK			(0x1U << PLL_NO_STALL_SHIFT)
#define DETECT_BACK_START_SHIFT			(6U)
#define DETECT_BACK_START_MASK			(0x1U << DETECT_BACK_START_SHIFT)
#define CLKDIV_PRE_CYCLE_PLL_STALL_SHIFT	(7U)
#define CLKDIV_PRE_CYCLE_PLL_STALL_MASK		(0x7U << CLKDIV_PRE_CYCLE_PLL_STALL_SHIFT)
#define CLKDIV_PRE_CYCLE_PLL_NOSTALL_SHIFT	(10U)
#define CLKDIV_PRE_CYCLE_PLL_NOSTALL_MASK	(0x7U << CLKDIV_PRE_CYCLE_PLL_NOSTALL_SHIFT)
#define MUX_WAIT_CYCLE_PLL_NOSTALL_SHIFT	(13U)
#define MUX_WAIT_CYCLE_PLL_NOSTALL_MASK		(0x7U << MUX_WAIT_CYCLE_PLL_NOSTALL_SHIFT)

#define CRU_IODET_CON1				0x04
#define REF_MAX_SHIFT				(0U)
#define REF_MAX_MASK				(0x3FFU << REF_MAX_SHIFT)
#define MUX_WAIT_CYCLE_PLL_STALL_SHIFT		(12U)
#define MUX_WAIT_CYCLE_PLL_STALL_MASK		(0xFU << MUX_WAIT_CYCLE_PLL_STALL_SHIFT)

#define HIWORD_UPDATE(val, mask, shift) \
		((val) << (shift) | (mask) << (16))

struct rockchip_iodet_info {
	const char *name;
	u32 con_offset;
	u32 status_offset;
	u32 det_rate;
	u32 live_status;
	u32 lost_status;
};

struct rockchip_iodet {
	int num;
	const struct rockchip_iodet_info *info;
};

#define CLK_IODET(_name, _con_offset, _status_offset,		\
		  _det_rate, _live_status, _lost_status)	\
{								\
	.name = _name,						\
	.con_offset = (_con_offset),				\
	.status_offset = (_status_offset),			\
	.det_rate = (_det_rate),				\
	.live_status = (_live_status),				\
	.lost_status = (_lost_status),				\
}

struct rockchip_iodet_clk {
	struct device *dev;
	void __iomem *base;
	struct clk_hw hw;
	struct clk_hw_onecell_data *clk_data;
	struct clk *ref_clk;
	struct clk *pll_clk;
	struct clk *int_clk;
	struct clk *frac_clk;
	/* protects access to the clock control registers */
	spinlock_t lock;
	const char *name;
	const struct rockchip_iodet *iodet;
	const struct rockchip_iodet_info *info;
	unsigned long last_time;
	int (*lock_cb)(void *data);
	int (*unlock_cb)(void *data);
	void *data_cb;
};

#define to_rockchip_iodet_clk(_hw)	\
	container_of(_hw, struct rockchip_iodet_clk, hw)

static unsigned long rockchip_iodet_clk_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	return parent_rate;
}

int rockchip_iodet_enable(struct clk *clk,
			  struct clk *ref_clk)
{
	struct rockchip_iodet_clk *priv = to_rockchip_iodet_clk(__clk_get_hw(clk));
	int div = 0, ret;

	clk_set_parent(priv->ref_clk, ref_clk);
	ret = clk_set_rate(priv->pll_clk, clk_get_rate(priv->pll_clk));
	if (ret < 0) {
		pr_err("Failed to change aupll rate: %d\n", ret);
		return 0;
	}
	clk_set_rate(priv->int_clk, clk_get_rate(priv->frac_clk));
	clk_prepare_enable(priv->frac_clk);
	clk_prepare_enable(priv->int_clk);
	clk_prepare_enable(ref_clk);

	div = DIV_ROUND_UP(priv->info->det_rate, clk_get_rate(priv->ref_clk));
	writel_relaxed(HIWORD_UPDATE(0x1U, DETECT_BACK_START_MASK, DETECT_BACK_START_SHIFT),
		       priv->base + priv->info->con_offset);
	writel_relaxed(HIWORD_UPDATE(0x5U, MUX_WAIT_CYCLE_PLL_NOSTALL_MASK,
				     MUX_WAIT_CYCLE_PLL_NOSTALL_SHIFT) |
		       HIWORD_UPDATE(0x2U, CLKDIV_PRE_CYCLE_PLL_NOSTALL_MASK,
				     CLKDIV_PRE_CYCLE_PLL_NOSTALL_SHIFT) |
		       HIWORD_UPDATE(0x6U, CLKDIV_PRE_CYCLE_PLL_STALL_MASK,
				     CLKDIV_PRE_CYCLE_PLL_STALL_SHIFT),
		       priv->base + priv->info->con_offset);

	writel_relaxed(HIWORD_UPDATE(0x6U, MUX_WAIT_CYCLE_PLL_STALL_MASK,
				     MUX_WAIT_CYCLE_PLL_STALL_SHIFT),
		       priv->base + priv->info->con_offset + CRU_IODET_CON1);
	writel_relaxed(HIWORD_UPDATE((div + 4), REF_MAX_MASK, REF_MAX_SHIFT),
		       priv->base + priv->info->con_offset + CRU_IODET_CON1);

	writel_relaxed(HIWORD_UPDATE(1, INT_EN_MASK, INT_EN_SHIFT) |
		       HIWORD_UPDATE(1, PHASE_SYNC_EN_MASK, PHASE_SYNC_EN_SHIFT) |
		       HIWORD_UPDATE(1, PLL_NO_STALL_MASK, PLL_NO_STALL_SHIFT),
		       priv->base + priv->info->con_offset);
	writel_relaxed(HIWORD_UPDATE(1, CLKEN_MASK, CLKEN_SHIFT),
		       priv->base + priv->info->con_offset);
	writel_relaxed(HIWORD_UPDATE(1, START_MASK, START_SHIFT),
		       priv->base + priv->info->con_offset);
	return 0;
}

int rockchip_iodet_set_callbacks(struct clk *clk, void *data,
			      int (*lock_cb)(void *data),
			      int (*unlock_cb)(void *data))
{
	struct rockchip_iodet_clk *priv = to_rockchip_iodet_clk(__clk_get_hw(clk));

	priv->lock_cb = lock_cb;
	priv->unlock_cb = unlock_cb;
	priv->data_cb = data;

	return 0;
}

static const struct clk_ops rockchip_iodet_clk_ops = {
	.recalc_rate = rockchip_iodet_clk_recalc_rate,
};

static struct clk_hw *rockchip_iodet_register_clock(struct rockchip_iodet_clk *priv,
						    const char *name, const char *parent_name)
{
	struct clk_hw *hw;
	struct clk_init_data init = {0};
	int ret;

	init.name = name;
	init.ops = &rockchip_iodet_clk_ops;
	init.flags = 0;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	priv->hw.init = &init;

	hw = &priv->hw;
	ret = clk_hw_register(priv->dev, hw);
	if (ret)
		return ERR_PTR(ret);

	ret = of_clk_add_hw_provider(priv->dev->of_node, of_clk_hw_simple_get, hw);
	if (ret) {
		clk_hw_unregister(hw);
		return ERR_PTR(ret);
	}

	return hw;
}

static irqreturn_t rockchip_iodet_interrupt(int irq, void *data)
{
	struct rockchip_iodet_clk *priv = data;
	int status;
	unsigned long now = jiffies;

	status = readl(priv->base + priv->info->status_offset);
	if (status < 0) {
		dev_err(priv->dev, "Failed to read  REG: %d\n", status);
		return status;
	}

	if (time_before(now, priv->last_time + msecs_to_jiffies(10)))
		return IRQ_HANDLED;

	if (status & priv->info->live_status) {
		dev_err(priv->dev, "Io det lived.\n");
		if (priv->lock_cb)
			priv->lock_cb(priv->data_cb);
		writel_relaxed(HIWORD_UPDATE(1, INT_CLEAR_MASK, INT_CLEAR_SHIFT),
			       priv->base + priv->info->con_offset);
		writel_relaxed(HIWORD_UPDATE(0, INT_CLEAR_MASK, INT_CLEAR_SHIFT),
			       priv->base + priv->info->con_offset);
		writel_relaxed(HIWORD_UPDATE(0, START_MASK, START_SHIFT),
			       priv->base + priv->info->con_offset);
		writel_relaxed(HIWORD_UPDATE(1, START_MASK, START_SHIFT),
			       priv->base + priv->info->con_offset);
		return IRQ_HANDLED;
	} else if (status & priv->info->lost_status) {
		dev_err(priv->dev, "Io det lost.\n");
		if (priv->unlock_cb)
			priv->unlock_cb(priv->data_cb);
		writel_relaxed(HIWORD_UPDATE(1, INT_CLEAR_MASK, INT_CLEAR_SHIFT),
			       priv->base + priv->info->con_offset);
		writel_relaxed(HIWORD_UPDATE(0, INT_CLEAR_MASK, INT_CLEAR_SHIFT),
			       priv->base + priv->info->con_offset);
		priv->last_time = now;
		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}

static const struct rockchip_iodet_info rk3572_clk_iodet_info[] = {
	CLK_IODET("clk_iodet_audio_0", 0x7b8, 0x7b4, 1188000000, BIT(3), BIT(0)),
	CLK_IODET("clk_iodet_audio_1", 0x7c0, 0x7b4, 1188000000, BIT(4), BIT(1)),
	CLK_IODET("clk_iodet_audio_2", 0x7c8, 0x7b4, 1188000000, BIT(5), BIT(2)),
};

static const struct rockchip_iodet rk3572_clk_iodet = {
	.num = ARRAY_SIZE(rk3572_clk_iodet_info),
	.info = rk3572_clk_iodet_info,
};

static const struct of_device_id rockchip_clk_iodet_of_match[] = {
	{
		.compatible = "rockchip,rk3572-clock-iodet",
		.data = (void *)&rk3572_clk_iodet,
	},
	{}
};
MODULE_DEVICE_TABLE(of, rockchip_clk_iodet_of_match);

static const struct rockchip_iodet_info *
rockchip_get_iodet_infos(const struct rockchip_iodet *iodet, const char *name)
{
	const struct rockchip_iodet_info *info = iodet->info;
	int i = 0;

	for (i = 0; i < iodet->num; i++) {
		if (strcmp(info->name, name) == 0)
			return info;
		info++;
	}
	return NULL;
}

static int rockchip_iodet_clk_probe(struct platform_device *pdev)
{
	struct rockchip_iodet_clk *priv;
	struct device_node *node = pdev->dev.of_node;
	const char *clk_name;
	struct clk_hw *hw;
	int ret, irq = 0;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct rockchip_iodet_clk),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->iodet = device_get_match_data(&pdev->dev);
	if (!priv->iodet) {
		dev_err(&pdev->dev, "no matching device data found\n");
		return -ENXIO;
	}
	priv->dev = &pdev->dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "could not get a valid irq\n");
		return -ENODEV;
	}

	spin_lock_init(&priv->lock);
	platform_set_drvdata(pdev, priv);

	priv->base = of_iomap(node, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	if (device_property_read_string(&pdev->dev, "clock-output-names", &clk_name))
		priv->name = node->name;
	else
		priv->name = clk_name;

	priv->ref_clk = devm_clk_get(&pdev->dev, "ref_clk");
	if (IS_ERR(priv->ref_clk))
		return PTR_ERR(priv->ref_clk);
	priv->pll_clk = devm_clk_get(&pdev->dev, "pll_clk");
	if (IS_ERR(priv->pll_clk))
		return PTR_ERR(priv->pll_clk);

	priv->int_clk = devm_clk_get(&pdev->dev, "int_clk");
	if (IS_ERR(priv->int_clk))
		return PTR_ERR(priv->int_clk);

	priv->frac_clk = devm_clk_get(&pdev->dev, "frac_clk");
	if (IS_ERR(priv->frac_clk))
		return PTR_ERR(priv->frac_clk);
	priv->info = rockchip_get_iodet_infos(priv->iodet, priv->name);
	if (!priv->info) {
		dev_err(&pdev->dev, "failed to find iodet info for %s\n", priv->name);
		return -EINVAL;
	}

	hw = rockchip_iodet_register_clock(priv,  priv->name, __clk_get_name(priv->int_clk));
	if (IS_ERR(hw))
		return PTR_ERR(hw);

	/* register interrupt handler */
	ret = devm_request_irq(&pdev->dev, irq, rockchip_iodet_interrupt,
			       IRQF_SHARED, dev_name(&pdev->dev), priv);
	if (ret) {
		dev_err(&pdev->dev, "request_irq err: %d\n", ret);
		return ret;
	}

	return 0;
}

static void rockchip_iodet_clk_remove(struct platform_device *pdev)
{
	struct rockchip_iodet_clk *priv = platform_get_drvdata(pdev);

	if (!priv)
		return;

	of_clk_del_provider(pdev->dev.of_node);
	clk_hw_unregister(&priv->hw);
	if (priv->base)
		iounmap(priv->base);
}

static struct platform_driver rockchip_iodet_clk_driver = {
	.probe = rockchip_iodet_clk_probe,
	.remove = rockchip_iodet_clk_remove,
	.driver = {
		.name = "rockchip_iodet_clk",
		.of_match_table = rockchip_clk_iodet_of_match,
	},
};
module_platform_driver(rockchip_iodet_clk_driver);

MODULE_DESCRIPTION("Rockchip Clock Io Det");
MODULE_LICENSE("GPL");
