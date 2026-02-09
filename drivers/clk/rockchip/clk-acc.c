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

#define CRU_ACC_CON0			0x00
#define CRU_ACC_TARGET_SHIFT		(0U)
#define CRU_ACC_TARGET_MASK		(0xFFFFFFFFU << CRU_ACC_TARGET_SHIFT)

#define CRU_ACC_CON1			0x04
#define CRU_ACC_REF_CNT_SHIFT		(0U)
#define CRU_ACC_REF_CNT_MASK		(0xFFFFU << CRU_ACC_REF_CNT_SHIFT)
#define CRU_ACC_COARSE_STEP_SHIFT	(16U)
#define CRU_ACC_COARSE_STEP_MASK	(0xFFU << CRU_ACC_COARSE_STEP_SHIFT)
#define CRU_ACC_FINE_STEP_SHIFT		(24U)
#define CRU_ACC_FINE_STEP_MASK		(0xFFU << CRU_ACC_FINE_STEP_SHIFT)

#define CRU_ACC_CON2			0x08
#define CRU_ACC_UNLOCK_THRESH_SHIFT	(0U)
#define CRU_ACC_UNLOCK_THRESH_MASK	(0x1FU << CRU_ACC_UNLOCK_THRESH_SHIFT)
#define CRU_ACC_ADAPTIVE_EN_SHIFT	(31U)
#define CRU_ACC_ADAPTIVE_EN_MASK	(0x1U << CRU_ACC_ADAPTIVE_EN_SHIFT)

#define CRU_FRAC_DIV_SHIFT	(16U)
#define CRU_FRAC_DIV_MASK	(0xFFFFU)
#define CRU_FRAC_HIGHDIV_SHIFT	(8U)
#define CRU_FRAC_HIGHDIV_MASK	(0xFFU)

#define HIWORD_UPDATE(val, mask, shift) \
		((val) << (shift) | (mask) << (16))

struct rockchip_acc_info {
	const char *name;
	u32 con_offset;
	u32 intsts_offset;
	u32 inten_offset;
	u32 intclr_offset;
	u32 div_offset;
	u32 high_div_offset;
	u32 en_shift;
	u32 lock_shift;
	u32 unlock_shift;
};

struct rockchip_acc {
	int num;
	const struct rockchip_acc_info *info;
};

#define CLK_ACC(_name, _con_offset, _intsts_offset,	\
		_inten_offset, _intclr_offset,		\
		_div_offset, _high_div_offset,		\
		_en_shift, _lock_shift, _unlock_shift)	\
{							\
	.name = _name,					\
	.con_offset = (_con_offset),			\
	.intsts_offset = (_intsts_offset),		\
	.inten_offset = (_inten_offset),		\
	.intclr_offset = (_intclr_offset),		\
	.div_offset = (_div_offset),			\
	.high_div_offset = (_high_div_offset),		\
	.en_shift = (_en_shift),			\
	.lock_shift = (_lock_shift),			\
	.unlock_shift = (_unlock_shift),		\
}

struct rockchip_acc_clk {
	struct device *dev;
	void __iomem *base;
	struct clk_hw hw;
	struct clk_hw_onecell_data *clk_data;
	struct clk *ref_clk;
	struct clk *adjust_clk;
	/* protects access to the clock control registers */
	spinlock_t lock;
	const char *name;
	const struct rockchip_acc *acc;
	const struct rockchip_acc_info *info;
	int (*lock_cb)(void *data);
	int (*unlock_cb)(void *data);
	void *data_cb;
};

#define to_rockchip_acc_clk(_hw) \
	container_of(_hw, struct rockchip_acc_clk, hw)

static unsigned long rockchip_acc_clk_recalc_rate(struct clk_hw *hw,
						  unsigned long parent_rate)
{
	return parent_rate;
}

static void rockchip_acc_frac_get_div(struct rockchip_acc_clk *priv,
				      u32 *n, u32 *m)
{
	u32 reg_val, reg_val_h;

	reg_val = readl_relaxed(priv->base + priv->info->div_offset);
	*n = (reg_val >> CRU_FRAC_DIV_SHIFT) & CRU_FRAC_DIV_MASK;
	*m = reg_val & CRU_FRAC_DIV_MASK;

	if (priv->info->high_div_offset) {
		reg_val_h = readl_relaxed(priv->base + priv->info->high_div_offset);
		*n |= ((reg_val_h >> CRU_FRAC_HIGHDIV_SHIFT) & CRU_FRAC_HIGHDIV_MASK) <<
		      CRU_FRAC_DIV_SHIFT;
		*m |= (reg_val_h & CRU_FRAC_HIGHDIV_MASK) << CRU_FRAC_DIV_SHIFT;
	}
}

static void rockchip_acc_frac_set_div(struct rockchip_acc_clk *priv,
				      u32 n, u32 m)
{
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	if (priv->info->high_div_offset)
		writel_relaxed((((n >> CRU_FRAC_DIV_SHIFT) & CRU_FRAC_HIGHDIV_MASK) <<
				CRU_FRAC_HIGHDIV_SHIFT) |
				((m >> CRU_FRAC_DIV_SHIFT) & CRU_FRAC_HIGHDIV_MASK),
			       priv->base + priv->info->high_div_offset);

	writel_relaxed(((n & CRU_FRAC_DIV_MASK) << CRU_FRAC_DIV_SHIFT) |
			(m & CRU_FRAC_DIV_MASK), priv->base + priv->info->div_offset);

	spin_unlock_irqrestore(&priv->lock, flags);
}

int rockchip_acc_adaptive_adjust(struct clk *clk,
				 struct clk *ref_clk,
				 bool enable)
{
	struct rockchip_acc_clk *priv = to_rockchip_acc_clk(__clk_get_hw(clk));
	unsigned long parent_rate, target_cnt, ref_rate;
	u32 n, m, div, ref_div;
	u32 fine_step = 1, coarse_step = 8;

	if (!enable) {
		writel_relaxed(0, priv->base + priv->info->con_offset + CRU_ACC_CON2);
		clk_disable_unprepare(priv->adjust_clk);
		clk_disable_unprepare(priv->ref_clk);
		return 0;
	}

	clk_set_parent(priv->ref_clk, ref_clk);
	clk_prepare_enable(priv->adjust_clk);
	clk_prepare_enable(priv->ref_clk);

	parent_rate = clk_get_rate(priv->adjust_clk);
	if (!parent_rate) {
		dev_err(priv->dev, "Failed to get parent rate\n");
		return -EINVAL;
	}

	ref_rate = clk_get_rate(ref_clk);
	if (!ref_rate) {
		dev_err(priv->dev, "Failed to get refclk rate\n");
		return -EINVAL;
	}

	rockchip_acc_frac_get_div(priv, &n, &m);

	div = 0xfffff / m;
	if (div % 2 && div > 1)
		div = div - 1;

	n *= div;
	m *= div;

	rockchip_acc_frac_set_div(priv, n, m);

	if (parent_rate % ref_rate)
		ref_div = 10;
	else
		ref_div = 1;

	target_cnt = (parent_rate * ref_div) / ref_rate;

	if (priv->info->inten_offset)
		writel_relaxed(HIWORD_UPDATE(1, 0x1 << priv->info->en_shift,
					     priv->info->en_shift) |
			       HIWORD_UPDATE(1, 0x1 << (priv->info->en_shift + 5),
					     priv->info->en_shift + 5),
			       priv->base + priv->info->inten_offset);
	writel_relaxed(0, priv->base + priv->info->con_offset + CRU_ACC_CON2);
	writel_relaxed(target_cnt, priv->base + priv->info->con_offset + CRU_ACC_CON0);
	writel_relaxed((fine_step << CRU_ACC_FINE_STEP_SHIFT) |
		       (coarse_step << CRU_ACC_COARSE_STEP_SHIFT) |
		       ((ref_div - 1) << CRU_ACC_REF_CNT_SHIFT),
	priv->base + priv->info->con_offset + CRU_ACC_CON1);

	writel_relaxed(1 << CRU_ACC_ADAPTIVE_EN_SHIFT,
		       priv->base + priv->info->con_offset + CRU_ACC_CON2);

	return 0;
}

int rockchip_acc_set_callbacks(struct clk *acc_clk, void *data,
			      int (*lock_cb)(void *data),
			      int (*unlock_cb)(void *data))
{
	struct rockchip_acc_clk *priv = to_rockchip_acc_clk(__clk_get_hw(acc_clk));

	priv->lock_cb = lock_cb;
	priv->unlock_cb = unlock_cb;
	priv->data_cb = data;

	return 0;
}

static const struct clk_ops rockchip_acc_clk_ops = {
	.recalc_rate = rockchip_acc_clk_recalc_rate,
};

static struct clk_hw *rockchip_acc_register_clock(struct rockchip_acc_clk *priv,
						  const char *name,
						  const char *parent_name)
{
	struct clk_hw *hw;
	struct clk_init_data init = {0};
	int ret;

	init.name = name;
	init.ops = &rockchip_acc_clk_ops;
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

static irqreturn_t rockchip_acc_interrupt(int irq, void *data)
{
	struct rockchip_acc_clk *priv = data;
	int status;

	status = readl(priv->base + priv->info->intsts_offset);
	if (status & priv->info->lock_shift) {
		if (priv->lock_cb)
			priv->lock_cb(priv->data_cb);
		if (priv->info->intclr_offset) {
			writel_relaxed(HIWORD_UPDATE(status, 0xffff, 0),
				       priv->base + priv->info->intclr_offset);
			writel_relaxed(HIWORD_UPDATE(0, 0xffff, 0),
				       priv->base + priv->info->intclr_offset);
		}
		return IRQ_HANDLED;
	} else if (status & priv->info->unlock_shift) {
		if (priv->unlock_cb)
			priv->unlock_cb(priv->data_cb);
		if (priv->info->intclr_offset) {
			writel_relaxed(HIWORD_UPDATE(status, 0xffff, 0),
				       priv->base + priv->info->intclr_offset);
			writel_relaxed(HIWORD_UPDATE(0, 0xffff, 0),
				       priv->base + priv->info->intclr_offset);
		}
		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}

static const struct rockchip_acc_info rk3538_clk_acc_info[] = {
	CLK_ACC("clk_acc_audio_frac_0", 0x3b0, 0x3c8, 0, 0,
		0x344, 0xcc8, 0, BIT(3), BIT(3)),
	CLK_ACC("clk_acc_audio_frac_1", 0x3bc, 0x3d4, 0, 0,
		0x348, 0xccc, 0, BIT(3), BIT(3)),
};

static const struct rockchip_acc rk3538_clk_acc = {
	.num = ARRAY_SIZE(rk3538_clk_acc_info),
	.info = rk3538_clk_acc_info,
};

static const struct rockchip_acc_info rk3572_clk_acc_info[] = {
	CLK_ACC("clk_acc_audio_frac_0", 0x700, 0x7b0, 0x7a4, 0x7a0,
		0x330, 0xcd4, 0, BIT(0), BIT(5)),
	CLK_ACC("clk_acc_audio_frac_1", 0x718, 0x7b0, 0x7a4, 0x7a0,
		0x338, 0xcd8, 1, BIT(1), BIT(6)),
	CLK_ACC("clk_acc_audio_frac_2", 0x730, 0x7b0, 0x7a4, 0x7a0,
		0x340, 0xcdc, 2, BIT(2), BIT(7)),
};

static const struct rockchip_acc rk3572_clk_acc = {
	.num = ARRAY_SIZE(rk3572_clk_acc_info),
	.info = rk3572_clk_acc_info,
};

static const struct of_device_id rockchip_clk_acc_of_match[] = {
	{
		.compatible = "rockchip,rk3538-clock-acc",
		.data = (void *)&rk3538_clk_acc,
	},
	{
		.compatible = "rockchip,rk3572-clock-acc",
		.data = (void *)&rk3572_clk_acc,
	},
	{}
};
MODULE_DEVICE_TABLE(of, rockchip_clk_acc_of_match);

static const struct rockchip_acc_info *
rockchip_get_acc_infos(const struct rockchip_acc *acc, const char *name)
{
	const struct rockchip_acc_info *info = acc->info;
	int i = 0;

	for (i = 0; i < acc->num; i++) {
		if (strcmp(info->name, name) == 0)
			return info;
		info++;
	}
	return NULL;
}

static int rockchip_acc_clk_probe(struct platform_device *pdev)
{
	struct rockchip_acc_clk *priv;
	struct device_node *node = pdev->dev.of_node;
	const char *clk_name;
	struct clk_hw *hw;
	int ret, irq = 0;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct rockchip_acc_clk),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->acc = device_get_match_data(&pdev->dev);
	if (!priv->acc) {
		dev_err(&pdev->dev, "no matching device data found\n");
		return -ENXIO;
	}
	priv->dev = &pdev->dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		dev_err(&pdev->dev, "could not get a valid irq\n");

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
	priv->adjust_clk = devm_clk_get(&pdev->dev, "adjust_clk");
	if (IS_ERR(priv->adjust_clk))
		return PTR_ERR(priv->adjust_clk);

	priv->info = rockchip_get_acc_infos(priv->acc, priv->name);
	if (!priv->info) {
		dev_err(&pdev->dev, "failed to find acc info for %s\n", priv->name);
		return -EINVAL;
	}

	hw = rockchip_acc_register_clock(priv,  priv->name, __clk_get_name(priv->adjust_clk));
	if (IS_ERR(hw))
		return PTR_ERR(hw);

	/* register interrupt handler */
	if (irq > 0) {
		ret = devm_request_irq(&pdev->dev, irq, rockchip_acc_interrupt,
				       IRQF_SHARED, dev_name(&pdev->dev), priv);
		if (ret) {
			dev_err(&pdev->dev, "request_irq err: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static void rockchip_acc_clk_remove(struct platform_device *pdev)
{
	struct rockchip_acc_clk *priv = platform_get_drvdata(pdev);

	if (!priv)
		return;

	of_clk_del_provider(pdev->dev.of_node);
	clk_hw_unregister(&priv->hw);
	if (priv->base)
		iounmap(priv->base);
}

static struct platform_driver rockchip_acc_clk_driver = {
	.probe = rockchip_acc_clk_probe,
	.remove = rockchip_acc_clk_remove,
	.driver = {
		.name = "rockchip_acc_clk",
		.of_match_table = rockchip_clk_acc_of_match,
	},
};
module_platform_driver(rockchip_acc_clk_driver);

MODULE_DESCRIPTION("Rockchip Clock Acc");
MODULE_LICENSE("GPL");
