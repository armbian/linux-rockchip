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
	int irq;
	void *data;
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
			aoa_middleware_aoa_notifier(s, aoa->data);
	}

	return IRQ_HANDLED;
}

int rockchip_aoa_probe(struct platform_device *pdev, void *data)
{
	struct device_node *node = pdev->dev.of_node;
	struct rk_aoa_dev *aoa;
	int ret, irq;
	struct resource *res;

	aoa = kzalloc(sizeof(*aoa), GFP_KERNEL);
	if (!aoa) {
		ret = -ENOMEM;
		goto err_out;
	}

	aoa->dev = &pdev->dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto err_free_aoa;
	}

	aoa->base = ioremap(res->start, resource_size(res));
	if (!aoa->base) {
		ret = -ENOMEM;
		goto err_free_aoa;
	}

	aoa->rst = of_reset_control_array_get_optional_exclusive(node);
	if (IS_ERR(aoa->rst)) {
		ret = PTR_ERR(aoa->rst);
		goto err_unmap;
	}

	irq = platform_get_irq_optional(pdev, 0);
	if (irq > 0) {
		ret = request_irq(irq, rockchip_aoa_isr, 0, node->name, aoa);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request irq %d\n", irq);
			goto err_put_rst;
		}
	}
	aoa->irq = irq;
	aoa->data = data;
	dev_set_drvdata(&pdev->dev, aoa);
	return 0;

err_put_rst:
	reset_control_put(aoa->rst);
err_unmap:
	iounmap(aoa->base);
err_free_aoa:
	kfree(aoa);
err_out:
	return ret;
}

int rockchip_aoa_remove(struct platform_device *pdev)
{
	struct rk_aoa_dev *aoa = platform_get_drvdata(pdev);

	if (aoa) {
		if (aoa->irq > 0) {
			disable_irq(aoa->irq);
			free_irq(aoa->irq, aoa);
		}

		reset_control_put(aoa->rst);
		iounmap(aoa->base);
		kfree(aoa);
		dev_set_drvdata(&pdev->dev, NULL);
	}
	return 0;
}
