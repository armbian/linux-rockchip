// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rockchip AOA Controller Driver
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 */

#include <linux/reset.h>
#include <sound/dmaengine_pcm.h>

#include "aoa_drv.h"
#include "aoa_middleware.h"

#define DRV_NAME		"rockchip-aoa"
#define AOA_AAD_IRQ_ST		0x01a8

struct rk_aoa_dev {
	struct device *dev;
	struct reset_control *rst;
	void __iomem *base;
};

static const struct of_device_id rockchip_aoa_match[] __maybe_unused = {
	{ .compatible = "rockchip,aoa", },
	{},
};

static irqreturn_t rockchip_aoa_isr(int irq, void *devid)
{
	struct rk_aoa_dev *aoa = (struct rk_aoa_dev *)devid;
	u32 st, s;

	st = readl(aoa->base + AOA_AAD_IRQ_ST);
	writel(st, aoa->base + AOA_AAD_IRQ_ST);
	for (s = 1; s < 8; s++) {
		if (st & (1 << s))
			aoa_middleware_aoa_notifier(s);
	}

	return IRQ_HANDLED;
}

int rockchip_aoa_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct rk_aoa_dev *aoa;
	int ret, irq;

	aoa = devm_kzalloc(&pdev->dev, sizeof(*aoa), GFP_KERNEL);
	if (!aoa)
		return -ENOMEM;

	aoa->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, aoa);

	aoa->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(aoa->base))
		return PTR_ERR(aoa->base);

	aoa->rst = devm_reset_control_array_get_optional_exclusive(aoa->dev);
	if (IS_ERR(aoa->rst))
		return PTR_ERR(aoa->rst);

	irq = platform_get_irq_optional(pdev, 0);
	if (irq > 0) {
		ret = devm_request_irq(&pdev->dev, irq, rockchip_aoa_isr,
				       IRQF_SHARED, node->name, aoa);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request irq %d\n", irq);
			return ret;
		}
	}

	return 0;
}

int rockchip_aoa_remove(struct platform_device *pdev)
{
	return 0;
}
