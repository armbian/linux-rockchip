// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2023 Rockchip Electronics Co., Ltd. */

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"


void rkvpss_idx_write(struct rkvpss_device *dev, u32 reg, u32 val, int idx)
{
	u32 *mem, *flag, offset = idx * RKVPSS_SW_REG_SIZE_MAX;

	if (!dev->unite_mode)
		offset = 0;

	mem = dev->sw_base_addr + reg + offset;
	flag = dev->sw_base_addr + reg + RKVPSS_SW_REG_SIZE + offset;
	*mem = val;
	*flag = RKVPSS_REG_CACHE;

	if (dev->hw_dev->is_single && !dev->unite_mode)
		rkvpss_hw_write(dev->hw_dev, reg, val);
}

void rkvpss_unite_write(struct rkvpss_device *dev, u32 reg, u32 val)
{
	rkvpss_idx_write(dev, reg, val, VPSS_UNITE_LEFT);
	if (dev->unite_mode)
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);
}

u32 rkvpss_idx_read(struct rkvpss_device *dev, u32 reg, int idx)
{
	u32 val, offset = idx * RKVPSS_SW_REG_SIZE_MAX;

	if (!dev->unite_mode)
		offset = 0;

	if (dev->hw_dev->is_single && !dev->unite_mode)
		val = rkvpss_hw_read(dev->hw_dev, reg);
	else
		val = *(u32 *)(dev->sw_base_addr + reg + offset);
	return val;
}

void rkvpss_idx_set_bits(struct rkvpss_device *dev, u32 reg, u32 mask, u32 val, int idx)
{
	u32 tmp = rkvpss_idx_read(dev, reg, idx) & ~mask;

	rkvpss_idx_write(dev, reg, val | tmp, idx);
}

void rkvpss_unite_set_bits(struct rkvpss_device *dev, u32 reg, u32 mask, u32 val)
{
	rkvpss_idx_set_bits(dev, reg, mask, val, VPSS_UNITE_LEFT);
	if (dev->unite_mode)
		rkvpss_idx_set_bits(dev, reg, mask, val, VPSS_UNITE_RIGHT);
}

void rkvpss_idx_clear_bits(struct rkvpss_device *dev, u32 reg, u32 mask, int idx)
{
	u32 tmp = rkvpss_idx_read(dev, reg, idx);

	rkvpss_idx_write(dev, reg, tmp & ~mask, idx);
}

void rkvpss_unite_clear_bits(struct rkvpss_device *dev, u32 reg, u32 mask)
{
	rkvpss_idx_clear_bits(dev, reg, mask, VPSS_UNITE_LEFT);
	if (dev->unite_mode)
		rkvpss_idx_clear_bits(dev, reg, mask, VPSS_UNITE_RIGHT);
}

// 1126b unite or multi sensor todo
void rkvpss_update_regs(struct rkvpss_device *dev, u32 start, u32 end)
{
	struct rkvpss_hw_dev *hw = dev->hw_dev;
	void __iomem *base = hw->base_addr;
	unsigned long lock_flags = 0;
	u32 i, j;

	if (end > RKVPSS_SW_REG_SIZE - 4) {
		dev_err(dev->dev, "%s out of range\n", __func__);
		return;
	}
	for (i = start; i <= end; i += 4) {
		u32 *val = dev->sw_base_addr + i;
		u32 *flag = dev->sw_base_addr + i + RKVPSS_SW_REG_SIZE;

		if (dev->unite_mode &&
		    ((!dev->mir_en && dev->unite_index == VPSS_UNITE_RIGHT) ||
		     (dev->mir_en && dev->unite_index == VPSS_UNITE_LEFT))) {
			val += (RKVPSS_SW_REG_SIZE_MAX / 4);
			flag += (RKVPSS_SW_REG_SIZE_MAX / 4);
		}

		if (*flag != RKVPSS_REG_CACHE)
			continue;

		if (IS_SYNC_REG(i)) {
			spin_lock_irqsave(&hw->reg_lock, lock_flags);
			if (i == RKVPSS_VPSS_ONLINE) {
				u32 mask = 0;

				for (j = 0; j < vpss_outchn_max(hw->vpss_ver); j++) {
					if (!hw->is_ofl_ch[j])
						continue;
					mask |= (RKVPSS_ISP2VPSS_CHN0_SEL(3) << j * 2);
				}
				*val |= (readl(base + i) & mask);
			} else {
				*val |= readl(base + i);
			}
		}
		writel(*val, base + i);
		if (i == RKVPSS_MI_WR_INIT)
			writel(*val, base + i);
		if (IS_SYNC_REG(i))
			spin_unlock_irqrestore(&hw->reg_lock, lock_flags);
	}
}

int rkvpss_attach_hw(struct rkvpss_device *vpss)
{
	struct device_node *np;
	struct platform_device *pdev;
	struct rkvpss_hw_dev *hw;

	np = of_parse_phandle(vpss->dev->of_node, "rockchip,hw", 0);
	if (!np || !of_device_is_available(np)) {
		dev_err(vpss->dev, "failed to get vpss hw node\n");
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		dev_err(vpss->dev, "failed to get vpss hw from node\n");
		return -ENODEV;
	}

	hw = platform_get_drvdata(pdev);
	if (!hw) {
		dev_err(vpss->dev, "failed attach vpss hw\n");
		return -EINVAL;
	}

	if (hw->dev_num >= VPSS_MAX_DEV) {
		dev_err(vpss->dev, "failed attach vpss hw, max dev:%d\n", VPSS_MAX_DEV);
		return -EINVAL;
	}

	if (hw->dev_num)
		hw->is_single = false;
	vpss->dev_id = hw->dev_num;
	hw->vpss[hw->dev_num] = vpss;
	hw->dev_num++;
	vpss->hw_dev = hw;
	vpss->vpss_ver = hw->vpss_ver;

	return 0;
}

void rkvpss_detach_hw(struct rkvpss_device *vpss)
{
	struct rkvpss_hw_dev *hw = vpss->hw_dev;

	if (hw) {
		hw->vpss[vpss->dev_id] = NULL;
		vpss->hw_dev = NULL;
	}
}
