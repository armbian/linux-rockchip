// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Rockchip Flexbus
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/mfd/syscon.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/interrupt.h>

#include <dt-bindings/mfd/rockchip-flexbus.h>
#include <linux/mfd/rockchip-flexbus.h>

unsigned int rockchip_flexbus_readl(struct rockchip_flexbus *rkfb, unsigned int reg)
{
	return readl_relaxed(rkfb->base + reg);
}
EXPORT_SYMBOL_GPL(rockchip_flexbus_readl);

void rockchip_flexbus_writel(struct rockchip_flexbus *rkfb, unsigned int reg, unsigned int val)
{
	writel_relaxed(val, rkfb->base + reg);
}
EXPORT_SYMBOL_GPL(rockchip_flexbus_writel);

void rockchip_flexbus_clrbits(struct rockchip_flexbus *rkfb, unsigned int reg, unsigned int clr_val)
{
	unsigned int reg_val;

	reg_val = rockchip_flexbus_readl(rkfb, reg);
	reg_val &= ~(clr_val);
	rockchip_flexbus_writel(rkfb, reg, reg_val);
}
EXPORT_SYMBOL_GPL(rockchip_flexbus_clrbits);

void rockchip_flexbus_setbits(struct rockchip_flexbus *rkfb, unsigned int reg, unsigned int set_val)
{
	unsigned int reg_val;

	reg_val = rockchip_flexbus_readl(rkfb, reg);
	reg_val |= set_val;
	rockchip_flexbus_writel(rkfb, reg, reg_val);
}
EXPORT_SYMBOL_GPL(rockchip_flexbus_setbits);

void rockchip_flexbus_clrsetbits(struct rockchip_flexbus *rkfb, unsigned int reg,
				 unsigned int clr_val, unsigned int set_val)
{
	unsigned int reg_val;

	reg_val = rockchip_flexbus_readl(rkfb, reg);
	reg_val &= ~(clr_val);
	reg_val |= set_val;
	rockchip_flexbus_writel(rkfb, reg, reg_val);
}
EXPORT_SYMBOL_GPL(rockchip_flexbus_clrsetbits);

static struct rockchip_flexbus_dfs_reg rockchip_flexbus_dfs_reg_v0 = {
	.dfs_2bit	= 0x0,
	.dfs_4bit	= 0x1,
	.dfs_8bit	= 0x2,
	.dfs_16bit	= 0x3,
	.dfs_mask	= 0x3,
};

static struct rockchip_flexbus_dfs_reg rockchip_flexbus_dfs_reg_v1 = {
	.dfs_1bit	= (0x0 << 29),
	.dfs_2bit	= (0x1 << 29),
	.dfs_4bit	= (0x2 << 29),
	.dfs_8bit	= (0x3 << 29),
	.dfs_16bit	= (0x4 << 29),
	.dfs_mask	= (0x7 << 29),
};

#define RK3506_GRF_SOC_CON1	0x0004
static void rk3506_flexbus_init_config(struct rockchip_flexbus *rkfb)
{
	regmap_write(rkfb->regmap_grf, RK3506_GRF_SOC_CON1, BIT(4 + 16));
}

static void rk3506_flexbus_grf_config(struct rockchip_flexbus *rkfb, bool slave_mode, bool cpol,
				      bool cpha)
{
	u32 val = 0x3 << 16;

	if (slave_mode) {
		if ((!cpol && cpha) || (cpol && !cpha))
			val |= BIT(0);
	} else {
		val |= BIT(1);
	}
	regmap_write(rkfb->regmap_grf, RK3506_GRF_SOC_CON1, val);
}

#define RK3576_VCCIO_IOC_MISC_CON0	0x6400
static void rk3576_flexbus_grf_config(struct rockchip_flexbus *rkfb, bool slave_mode, bool cpol,
				      bool cpha)
{
	u32 val = 0x3 << (16 + 7);

	if (slave_mode) {
		val |= BIT(8);
		if ((!cpol && cpha) || (cpol && !cpha))
			val |= BIT(7);
	}
	regmap_write(rkfb->regmap_grf, RK3576_VCCIO_IOC_MISC_CON0, val);
}

static irqreturn_t rockchip_flexbus_isr(int irq, void *dev_id)
{
	struct rockchip_flexbus *rkfb = dev_id;
	u32 isr;

	isr = rockchip_flexbus_readl(rkfb, FLEXBUS_ISR);

	if (rkfb->opmode0 != ROCKCHIP_FLEXBUS0_OPMODE_NULL && rkfb->fb0_isr)
		rkfb->fb0_isr(rkfb, isr);
	if (rkfb->opmode1 != ROCKCHIP_FLEXBUS1_OPMODE_NULL && rkfb->fb1_isr)
		rkfb->fb1_isr(rkfb, isr);

	return IRQ_HANDLED;
}

static void rockchip_flexbus_clk_bulk_disable(void *data)
{
	struct rockchip_flexbus *rkfb = data;

	clk_bulk_disable_unprepare(rkfb->num_clks, rkfb->clks);
}

static void rockchip_flexbus_init(struct rockchip_flexbus *rkfb)
{
	if (rkfb->config->init_config)
		rkfb->config->init_config(rkfb);

	if (rkfb->opmode0 != ROCKCHIP_FLEXBUS0_OPMODE_NULL &&
	    rkfb->opmode1 != ROCKCHIP_FLEXBUS1_OPMODE_NULL)
		rockchip_flexbus_writel(rkfb, FLEXBUS_COM_CTL, FLEXBUS_TX_AND_RX);
	else if (rkfb->opmode0 != ROCKCHIP_FLEXBUS0_OPMODE_NULL)
		rockchip_flexbus_writel(rkfb, FLEXBUS_COM_CTL, FLEXBUS_TX_ONLY);
	else
		rockchip_flexbus_writel(rkfb, FLEXBUS_COM_CTL, FLEXBUS_RX_ONLY);
}

static int rockchip_flexbus_probe(struct platform_device *pdev)
{
	struct rockchip_flexbus *rkfb;
	int ret;

	rkfb = devm_kzalloc(&pdev->dev, sizeof(*rkfb), GFP_KERNEL);
	if (!rkfb)
		return -ENOMEM;

	platform_set_drvdata(pdev, rkfb);

	ret = device_property_read_u32(&pdev->dev, "rockchip,flexbus0-opmode", &rkfb->opmode0);
	if (ret)
		rkfb->opmode0 = ROCKCHIP_FLEXBUS0_OPMODE_NULL;
	if (rkfb->opmode0 < ROCKCHIP_FLEXBUS0_OPMODE_NULL ||
	    rkfb->opmode0 > ROCKCHIP_FLEXBUS0_OPMODE_SPI)
		return -EINVAL;

	ret = device_property_read_u32(&pdev->dev, "rockchip,flexbus1-opmode", &rkfb->opmode1);
	if (ret)
		rkfb->opmode1 = ROCKCHIP_FLEXBUS1_OPMODE_NULL;
	if (rkfb->opmode1 < ROCKCHIP_FLEXBUS1_OPMODE_NULL ||
	    rkfb->opmode1 > ROCKCHIP_FLEXBUS1_OPMODE_CIF)
		return -EINVAL;

	rkfb->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(rkfb->base))
		return PTR_ERR(rkfb->base);
	rkfb->dev = &pdev->dev;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		return dev_err_probe(&pdev->dev, ret, "failed to get irq\n");

	ret = devm_request_irq(&pdev->dev, ret, rockchip_flexbus_isr, 0, dev_name(&pdev->dev),
			       rkfb);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request irq %d\n", ret);
		return ret;
	}

	rkfb->config = device_get_match_data(&pdev->dev);

	rkfb->regmap_grf = syscon_regmap_lookup_by_phandle_optional(pdev->dev.of_node,
								    "rockchip,grf");
	if (!rkfb->regmap_grf) {
		dev_err(&pdev->dev, "failed to get rockchip,grf node.\n");
		return -ENODEV;
	}

	rkfb->num_clks = devm_clk_bulk_get_all(&pdev->dev, &rkfb->clks);
	if (rkfb->num_clks <= 0) {
		dev_err(&pdev->dev, "bus clock not found\n");
		return -ENODEV;
	}
	ret = clk_bulk_prepare_enable(rkfb->num_clks, rkfb->clks);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable clocks.\n");
		return ret;
	}

	ret = devm_add_action_or_reset(&pdev->dev, rockchip_flexbus_clk_bulk_disable, rkfb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register devm action, %d\n", ret);
		return ret;
	}

	rockchip_flexbus_init(rkfb);

	switch (rockchip_flexbus_readl(rkfb, FLEXBUS_REVISION) >> 24 & 0xff) {
	case 0x0:
		rkfb->dfs_reg = &rockchip_flexbus_dfs_reg_v0;
		break;
	case 0x1:
		rkfb->dfs_reg = &rockchip_flexbus_dfs_reg_v1;
		break;
	default:
		dev_err(&pdev->dev, "failed to get large version.\n");
		return -EINVAL;
	}

	return devm_of_platform_populate(&pdev->dev);
}

static const struct rockchip_flexbus_config rk3506_flexbus_config = {
	.init_config =		rk3506_flexbus_init_config,
	.grf_config =		rk3506_flexbus_grf_config,
	.txwat_start_max =	255,
};

static const struct rockchip_flexbus_config rk3576_flexbus_config = {
	.init_config =		NULL,
	.grf_config =		rk3576_flexbus_grf_config,
	.txwat_start_max =	511,
};

static const struct of_device_id rockchip_flexbus_of_match[] = {
	{ .compatible = "rockchip,rk3506-flexbus", .data = &rk3506_flexbus_config},
	{ .compatible = "rockchip,rk3576-flexbus", .data = &rk3576_flexbus_config},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rockchip_flexbus_of_match);

static int rockchip_flexbus_resume(struct device *dev)
{
	struct rockchip_flexbus *rkfb = dev_get_drvdata(dev);

	rockchip_flexbus_init(rkfb);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(rockchip_flexbus_pm_ops, NULL, rockchip_flexbus_resume);

static struct platform_driver rockchip_flexbus_driver = {
	.probe	= rockchip_flexbus_probe,
	.driver	= {
		.name		= "rockchip_flexbus",
		.of_match_table	= rockchip_flexbus_of_match,
		.pm		= pm_sleep_ptr(&rockchip_flexbus_pm_ops),
	},
};

module_platform_driver(rockchip_flexbus_driver);

MODULE_DESCRIPTION("Rockchip Flexbus driver");
MODULE_LICENSE("GPL");
