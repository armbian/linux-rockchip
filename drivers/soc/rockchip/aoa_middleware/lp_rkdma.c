// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rockchip Low Power DMA Driver

 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io-64-nonatomic-hi-lo.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "lp_rkdma.h"
#include "aoa_middleware.h"

#define DRIVER_NAME			"lp-rkdma"
#define DMA_MAX_SIZE			(0x1000000)
#define LLI_BLOCK_SIZE			(SZ_4K)
#define RK_DMA_VER(major, minor)	(((major) << 16) | ((minor) << 0))

#define HIWORD_UPDATE(v, h, l)	(((v) << (l)) | (GENMASK((h), (l)) << 16))
#define GENMASK_VAL(v, h, l)	(((v) & GENMASK(h, l)) >> l)

#define RK_DMA_CMN_GROUP_SIZE		0x100
#define RK_DMA_LCH_GROUP_SIZE		0x40

#define RK_DMA_CMN_REG(x)		(d->base + (x))
#define RK_DMA_LCH_REG(x)		(l->base + (x))
#define RK_DMA_LCHn_REG(n, x)		(d->base + RK_DMA_CMN_GROUP_SIZE + \
					 (RK_DMA_LCH_GROUP_SIZE * (n)) + (x))

/* RK_DMA Common Register Define */
#define RK_DMA_CMN_VER			RK_DMA_CMN_REG(0x0000) /* Address Offset: 0x0000 */
#define RK_DMA_CMN_CFG			RK_DMA_CMN_REG(0x0004) /* Address Offset: 0x0004 */
#define RK_DMA_CMN_CTL0			RK_DMA_CMN_REG(0x0008) /* Address Offset: 0x0008 */
#define RK_DMA_CMN_CTL1			RK_DMA_CMN_REG(0x000c) /* Address Offset: 0x000C */
#define RK_DMA_CMN_AXICTL		RK_DMA_CMN_REG(0x0010) /* Address Offset: 0x0010 */
#define RK_DMA_CMN_DYNCTL		RK_DMA_CMN_REG(0x0014) /* Address Offset: 0x0014 */
#define RK_DMA_CMN_IS0			RK_DMA_CMN_REG(0x0018) /* Address Offset: 0x0018 */
#define RK_DMA_CMN_IS1			RK_DMA_CMN_REG(0x001c) /* Address Offset: 0x001C */
#define RK_DMA_CMN_CAP0			RK_DMA_CMN_REG(0x0030) /* Address Offset: 0x0030 */
#define RK_DMA_CMN_CAP1			RK_DMA_CMN_REG(0x0034) /* Address Offset: 0x0034 */
#define RK_DMA_CMN_PCH_EN		RK_DMA_CMN_REG(0x0040) /* Address Offset: 0x0040 */
#define RK_DMA_CMN_PCH_SEN		RK_DMA_CMN_REG(0x0044) /* Address Offset: 0x0044 */

/* RK_DMA_Logic Channel Register Define */
#define RK_DMA_LCH_CTL0			RK_DMA_LCH_REG(0x0000) /* Address Offset: 0x0000 */
#define RK_DMA_LCH_CTL1			RK_DMA_LCH_REG(0x0004) /* Address Offset: 0x0004 */
#define RK_DMA_LCH_CMDBA		RK_DMA_LCH_REG(0x0008) /* Address Offset: 0x0008 */
#define RK_DMA_LCH_TRF_CMD		RK_DMA_LCH_REG(0x000c) /* Address Offset: 0x000C */
#define RK_DMA_LCH_CMDBA_HIGH		RK_DMA_LCH_REG(0x0010) /* Address Offset: 0x0010 */
#define RK_DMA_LCH_IS			RK_DMA_LCH_REG(0x0014) /* Address Offset: 0x0014 */
#define RK_DMA_LCH_IE			RK_DMA_LCH_REG(0x0018) /* Address Offset: 0x0018 */
#define RK_DMA_LCH_DBGS0		RK_DMA_LCH_REG(0x001c) /* Address Offset: 0x001C */
#define RK_DMA_LCH_DBGC0		RK_DMA_LCH_REG(0x0020) /* Address Offset: 0x0020 */
#define RK_DMA_LCH_LLI_CNT		RK_DMA_LCH_REG(0x0030) /* Address Offset: 0x0030 */

/* CMN_VER */
#define CMN_VER_MAJOR(v)		GENMASK_VAL(v, 31, 16)
#define CMN_VER_MINOR(v)		GENMASK_VAL(v, 15, 0)

/* CMN_CFG */
#define CMN_CFG_EN			HIWORD_UPDATE(1, 0, 0)
#define CMN_CFG_DIS			HIWORD_UPDATE(0, 0, 0)
#define CMN_CFG_SRST			HIWORD_UPDATE(1, 1, 1)
#define CMN_CFG_IE_EN			HIWORD_UPDATE(1, 2, 2)
#define CMN_CFG_IE_DIS			HIWORD_UPDATE(0, 2, 2)

/* CMN_CAP0 */
#define CMN_LCH_NUM(v)			(GENMASK_VAL(v, 5, 0) + 1)
#define CMN_PCH_NUM(v)			(GENMASK_VAL(v, 11, 6) + 1)
#define CMN_BUF_DEPTH(v)		(GENMASK_VAL(v, 31, 21) + 1)

/* CMN_CAP1 */
#define CMN_AXI_SIZE(v)			(1 << GENMASK_VAL(v, 2, 0))
#define CMN_AXI_LEN(v)			(GENMASK_VAL(v, 10, 3) + 1)
#define CMN_AXADDR_WIDTH(v)		(32 + GENMASK_VAL(v, 18, 14) - 3)
#define CMN_AXOSR_SUP(v)		(GENMASK_VAL(v, 23, 19) + 1)

/* CMN_PCH_EN */
#define CMN_PCH_EN(n)			HIWORD_UPDATE(1, (n), (n))
#define CMN_PCH_DIS(n)			HIWORD_UPDATE(0, (n), (n))

struct lp_rkdma_lch {
	void __iomem		*base;
	u32			id;
};

struct lp_rkdma_dev {
	struct device		*dev;
	struct lp_rkdma_lch	*lch;
	struct clk_bulk_data	*clks;
	void __iomem		*base;
	int			irq;
	int			num_clks;
	u32			bus_width;
	u32			buf_dep;
	u32			dma_channels;
	u32			dma_requests;
	u32			version;
	void			*data;
};

static int lp_rkdma_init(struct lp_rkdma_dev *d)
{
	int lch, pch, buswidth, maxburst, dep, addrwidth;
	u32 cap0, cap1, ver;

	/* Just get base infos of rkdma */
	ver  = readl(RK_DMA_CMN_VER);
	cap0 = readl(RK_DMA_CMN_CAP0);
	cap1 = readl(RK_DMA_CMN_CAP1);

	lch = CMN_LCH_NUM(cap0);
	pch = CMN_PCH_NUM(cap0);
	dep = CMN_BUF_DEPTH(cap0);

	addrwidth = CMN_AXADDR_WIDTH(cap1);
	buswidth = CMN_AXI_SIZE(cap1);
	maxburst = CMN_AXI_LEN(cap1);

	d->version = ver;
	d->bus_width = buswidth;
	d->buf_dep = dep;
	d->dma_channels = CMN_LCH_NUM(cap0);
	d->dma_requests = CMN_LCH_NUM(cap0);

	dev_info(d->dev, "Lowpower RKDMA: NR_LCH-%d NR_PCH-%d PCH_BUF-%dx%dBytes AXI_LEN-%d ADDR-%dBits V%lu.%lu\n",
		 lch, pch, dep, buswidth, maxburst, addrwidth,
		 CMN_VER_MAJOR(ver), CMN_VER_MINOR(ver));

	return 0;
}

static irqreturn_t lp_rkdma_irq_handler(int irq, void *dev_id)
{
	struct lp_rkdma_dev *d = (struct lp_rkdma_dev *)dev_id;
	struct lp_rkdma_lch *l = &d->lch[0];
	u64 is = 0, is_raw = 0;
	u32 i = 0;

	aoa_middleware_dma_notifier(readl(RK_DMA_LCH_LLI_CNT), d->data);

	is = readq(RK_DMA_CMN_IS0);
	is_raw = is;

	while (is) {
		i = __ffs64(is);
		is &= ~BIT_ULL(i);
		l = &d->lch[i];
		writel(readl(RK_DMA_LCH_IS), RK_DMA_LCH_IS);
	}

	writeq(is_raw, RK_DMA_CMN_IS0);
	return IRQ_HANDLED;
}

int lp_rkdma_probe(struct platform_device *pdev, void *data)
{
	struct lp_rkdma_dev *d;
	struct resource *res;
	int i, ret;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto err_out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto err_free_d;
	}
	if (!request_mem_region(res->start, resource_size(res), dev_name(&pdev->dev))) {
		ret = -EBUSY;
		goto err_free_d;
	}
	d->base = ioremap(res->start, resource_size(res));
	if (!d->base) {
		ret = -ENOMEM;
		goto err_free_region;
	}

	d->num_clks = clk_bulk_get_all(&pdev->dev, &d->clks);
	if (d->num_clks < 1) {
		dev_err(&pdev->dev, "Failed to get clk\n");
		ret = -ENODEV;
		goto err_free_ioremap;
	}

	d->irq = platform_get_irq(pdev, 0);

	platform_set_drvdata(pdev, d);

	/* Enable clock before access registers */
	ret = clk_bulk_prepare_enable(d->num_clks, d->clks);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable clk: %d\n", ret);
		goto err_put_clks;
	}

	lp_rkdma_init(d);

	/* init lch channel */
	d->lch = kcalloc(d->dma_channels, sizeof(struct lp_rkdma_lch), GFP_KERNEL);
	if (!d->lch) {
		ret = -ENOMEM;
		goto err_disable_clk;
	}

	for (i = 0; i < d->dma_channels; i++) {
		struct lp_rkdma_lch *l = &d->lch[i];

		l->id = i;
		l->base = RK_DMA_LCHn_REG(i, 0);
	}
	d->data = data;

	ret = request_irq(d->irq, lp_rkdma_irq_handler, 0, dev_name(&pdev->dev), d);
	if (ret)
		goto err_free_lch;
	return 0;

err_free_lch:
	kfree(d->lch);
err_disable_clk:
	clk_bulk_disable_unprepare(d->num_clks, d->clks);
err_put_clks:
	clk_bulk_put_all(d->num_clks, d->clks);
err_free_ioremap:
	iounmap(d->base);
err_free_region:
	release_mem_region(res->start, resource_size(res));
err_free_d:
	kfree(d);
err_out:
	return ret;
}

int lp_rkdma_remove(struct platform_device *pdev)
{
	struct lp_rkdma_dev *d = platform_get_drvdata(pdev);
	struct lp_rkdma_lch *l = &d->lch[0];
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	/* make sure disable IRQ requests during removing module */
	writel(0x0, RK_DMA_LCH_CTL0);
	writel(0x0, RK_DMA_LCH_IE);

	if (d) {
		if (d->irq > 0) {
			disable_irq(d->irq);
			free_irq(d->irq, d);
		}

		clk_bulk_disable_unprepare(d->num_clks, d->clks);
		clk_bulk_put_all(d->num_clks, d->clks);
		kfree(d->lch);
		iounmap(d->base);
		if (res)
			release_mem_region(res->start, resource_size(res));
		kfree(d);
		platform_set_drvdata(pdev, NULL);
	}
	return 0;
}
