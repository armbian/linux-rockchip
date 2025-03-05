// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Adjustable fractional divider clock implementation.
 * Uses rational best approximation algorithm.
 *
 * Output is calculated as
 *
 *	rate = (m / n) * parent_rate				(1)
 *
 * This is useful when we have a prescaler block which asks for
 * m (numerator) and n (denominator) values to be provided to satisfy
 * the (1) as much as possible.
 *
 * Since m and n have the limitation by a range, e.g.
 *
 *	n >= 1, n < N_width, where N_width = 2^nwidth		(2)
 *
 * for some cases the output may be saturated. Hence, from (1) and (2),
 * assuming the worst case when m = 1, the inequality
 *
 *	floor(log2(parent_rate / rate)) <= nwidth		(3)
 *
 * may be derived. Thus, in cases when
 *
 *	(parent_rate / rate) >> N_width				(4)
 *
 * we might scale up the rate by 2^scale (see the description of
 * CLK_FRAC_DIVIDER_POWER_OF_TWO_PS for additional information), where
 *
 *	scale = floor(log2(parent_rate / rate)) - nwidth	(5)
 *
 * and assume that the IP, that needs m and n, has also its own
 * prescaler, which is capable to divide by 2^scale. In this way
 * we get the denominator to satisfy the desired range (2) and
 * at the same time a much better result of m and n than simple
 * saturated values.
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/rational.h>
#include "clk.h"

struct clk_fractional_divider_v2 {
	struct clk_hw	hw;
	void __iomem	*reg;
	void __iomem	*high_reg;
	u8		mshift;
	u8		mwidth;
	u32		mmask;
	u8		nshift;
	u8		nwidth;
	u32		nmask;
	u8		high_mshift;
	u8		high_mwidth;
	u32		high_mmask;
	u8		high_nshift;
	u8		high_nwidth;
	u32		high_nmask;
	u8		flags;
	void		(*approximation)(struct clk_hw *hw,
					 unsigned long rate,
					 unsigned long *parent_rate,
					 unsigned long *m, unsigned long *n);
	spinlock_t	*lock;
};

#define to_clk_fd_v2(_hw) container_of(_hw, struct clk_fractional_divider_v2, hw)

static inline u32 clk_fd_v2_readl(struct clk_fractional_divider_v2 *fd,
				  void __iomem *reg)
{
	return readl(reg);
}

static inline void clk_fd_v2_writel(struct clk_fractional_divider_v2 *fd,
				    void __iomem *reg, u32 val)
{
	writel(val, reg);
}

static unsigned long clk_fd_v2_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct clk_fractional_divider_v2 *fd = to_clk_fd_v2(hw);
	unsigned long flags = 0;
	unsigned long m_low, n_low, m_high, n_high, m, n;
	u32 val_low, val_high;
	u64 ret;

	if (fd->lock)
		spin_lock_irqsave(fd->lock, flags);
	else
		__acquire(fd->lock);

	val_low = clk_fd_v2_readl(fd, fd->reg);
	val_high = clk_fd_v2_readl(fd, fd->high_reg);

	if (fd->lock)
		spin_unlock_irqrestore(fd->lock, flags);
	else
		__release(fd->lock);

	m_low = (val_low & fd->mmask) >> fd->mshift;
	n_low = (val_low & fd->nmask) >> fd->nshift;
	m_high = (val_high & fd->high_mmask) >> fd->high_mshift;
	n_high = (val_high & fd->high_nmask) >> fd->high_nshift;
	m = (m_high << 16) | m_low;
	n = (n_high << 16) | n_low;

	if (fd->flags & CLK_FRAC_DIVIDER_ZERO_BASED) {
		m++;
		n++;
	}

	if (!n || !m)
		return parent_rate;

	ret = (u64)parent_rate * m;
	do_div(ret, n);

	return ret;
}

static void clk_fractional_divider_general_approximation_v2(struct clk_hw *hw,
							    unsigned long rate,
							    unsigned long *parent_rate,
							    unsigned long *m, unsigned long *n)
{
	struct clk_fractional_divider_v2 *fd = to_clk_fd_v2(hw);
	unsigned long scale;

	/*
	 * Get rate closer to *parent_rate to guarantee there is no overflow
	 * for m and n. In the result it will be the nearest rate left shifted
	 * by (scale - fd->nwidth) bits.
	 *
	 * For the detailed explanation see the top comment in this file.
	 */
	scale = fls_long(*parent_rate / rate - 1);
	if (scale > fd->nwidth)
		rate <<= scale - fd->nwidth;

	rational_best_approximation(rate, *parent_rate,
				    GENMASK(fd->mwidth - 1, 0),
				    GENMASK(fd->nwidth - 1, 0),
				    m, n);
}

static long clk_fd_v2_round_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long *parent_rate)
{
	struct clk_fractional_divider_v2 *fd = to_clk_fd_v2(hw);
	unsigned long m, n;
	u64 ret;

	if (!rate || rate >= *parent_rate)
		return *parent_rate;

	if (fd->approximation)
		fd->approximation(hw, rate, parent_rate, &m, &n);
	else
		clk_fractional_divider_general_approximation_v2(hw, rate, parent_rate, &m, &n);

	ret = (u64)*parent_rate * m;
	do_div(ret, n);

	return ret;
}

static int clk_fd_v2_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct clk_fractional_divider_v2 *fd = to_clk_fd_v2(hw);
	unsigned long flags = 0;
	unsigned long m, n, m_low, n_low, m_high, n_high;
	u32 val, val_high;

	rational_best_approximation(rate, parent_rate,
				    GENMASK(fd->mwidth - 1, 0),
				    GENMASK(fd->nwidth - 1, 0),
				    &m, &n);

	if (fd->flags & CLK_FRAC_DIVIDER_ZERO_BASED) {
		m--;
		n--;
	}
	/*
	 * When compensation the fractional divider,
	 * the [1:0] bits of the numerator register are omitted,
	 * which will lead to a large deviation in the result.
	 * Therefore, it is required that the numerator must
	 * be greater than 4.
	 *
	 * Note that there are some exceptions here:
	 * If there is an even frac div, we need to keep the original
	 * numerator(<4) and denominator. Otherwise, it may cause the
	 * issue that the duty ratio is not 50%.
	 */
	if (m < 4 && m != 0) {
		if (n % 2 == 0)
			val = 1;
		else
			val = DIV_ROUND_UP(4, m);

		n *= val;
		m *= val;
		if (n > GENMASK(fd->nwidth + fd->high_nwidth - 1, 0)) {
			pr_debug("%s n(%ld) is overflow, use mask value\n",
				 __func__, n);
			n = GENMASK(fd->nwidth + fd->high_nwidth - 1, 0);
		}
	}

	if (fd->lock)
		spin_lock_irqsave(fd->lock, flags);
	else
		__acquire(fd->lock);

	m_low = m & fd->nmask;
	n_low = n & fd->nmask;
	m_high = (m & GENMASK(fd->mwidth - 1, (fd->mwidth - fd->high_mwidth))) >>
		 (fd->mwidth - fd->high_mwidth);
	n_high = (n & GENMASK(fd->nwidth - 1, (fd->nwidth - fd->high_nwidth))) >>
		 (fd->nwidth - fd->high_nwidth);

	val_high = clk_fd_v2_readl(fd, fd->high_reg);
	val_high &= ~(fd->high_mmask | fd->high_nmask);
	val_high |= (m_high << fd->high_mshift) | (n_high << fd->high_nshift);

	clk_fd_v2_writel(fd, fd->high_reg, val_high);

	val = clk_fd_v2_readl(fd, fd->reg);
	val &= ~(fd->mmask | fd->nmask);
	val |= (m_low << fd->mshift) | (n_low << fd->nshift);
	clk_fd_v2_writel(fd, fd->reg, val);

	if (fd->lock)
		spin_unlock_irqrestore(fd->lock, flags);
	else
		__release(fd->lock);

	return 0;
}

static const struct clk_ops clk_fractional_divider_ops_v2 = {
	.recalc_rate = clk_fd_v2_recalc_rate,
	.round_rate = clk_fd_v2_round_rate,
	.set_rate = clk_fd_v2_set_rate,
};

struct clk_hw *clk_hw_register_fractional_divider_v2(struct device *dev,
						     const char *name,
						     const char *parent_name,
						     unsigned long flags,
						     void __iomem *reg,
						     u8 mshift, u8 mwidth,
						     u8 nshift, u8 nwidth,
						     void __iomem *high_reg,
						     u8 high_mshift, u8 high_mwidth,
						     u8 high_nshift, u8 high_nwidth,
						     u8 clk_divider_flags,
						     spinlock_t *lock)
{
	struct clk_fractional_divider_v2 *fd;
	struct clk_init_data init;
	struct clk_hw *hw;
	int ret;

	fd = kzalloc(sizeof(*fd), GFP_KERNEL);
	if (!fd)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clk_fractional_divider_ops_v2;
	init.flags = flags;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;

	fd->reg = reg;
	fd->mshift = mshift;
	fd->mwidth = mwidth + high_mwidth;
	fd->mmask = GENMASK(mwidth - 1, 0) << mshift;
	fd->nshift = nshift;
	fd->nwidth = nwidth + high_nwidth;
	fd->nmask = GENMASK(nwidth - 1, 0) << nshift;
	fd->high_reg = high_reg;
	fd->high_mshift = high_mshift;
	fd->high_mwidth = high_mwidth;
	fd->high_mmask = GENMASK(high_mwidth - 1, 0) << high_mshift;
	fd->high_nshift = high_nshift;
	fd->high_nwidth = high_nwidth;
	fd->high_nmask = GENMASK(high_nwidth - 1, 0) << high_nshift;
	fd->flags = clk_divider_flags;
	fd->lock = lock;
	fd->hw.init = &init;

	hw = &fd->hw;
	ret = clk_hw_register(dev, hw);
	if (ret) {
		kfree(fd);
		hw = ERR_PTR(ret);
	}

	return hw;
}
EXPORT_SYMBOL_GPL(clk_hw_register_fractional_divider_v2);

struct clk *clk_register_fractional_divider_v2(struct device *dev,
					       const char *name,
					       const char *parent_name,
					       unsigned long flags,
					       void __iomem *reg,
					       u8 mshift, u8 width,
					       void __iomem *high_reg,
					       u8 high_mshift, u8 high_width,
					       u8 clk_divider_flags,
					       spinlock_t *lock)
{
	struct clk_hw *hw;

	hw = clk_hw_register_fractional_divider_v2(dev, name, parent_name, flags,
						   reg, mshift, width, 0, width,
						   high_reg, high_mshift, high_width,
						   0, high_width,
						   clk_divider_flags,
						   lock);
	if (IS_ERR(hw))
		return ERR_CAST(hw);
	return hw->clk;
}
EXPORT_SYMBOL_GPL(clk_register_fractional_divider_v2);

void clk_hw_unregister_fractional_divider_v2(struct clk_hw *hw)
{
	struct clk_fractional_divider_v2 *fd;

	fd = to_clk_fd_v2(hw);

	clk_hw_unregister(hw);
	kfree(fd);
}
