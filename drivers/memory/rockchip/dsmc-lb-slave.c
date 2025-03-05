// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include "dsmc-host.h"
#include "dsmc-lb-slave.h"

struct dsmc_soc_info {
	int (*dsmc_soc_init)(struct platform_device *pdev);
};

struct dsmc_lb_slv_map {
	phys_addr_t phys;
	size_t size;
};

struct rockchip_dsmc_lb_slave {
	const struct dsmc_soc_info *soc_info;

	struct device *dev;
	/* Hardware resources */
	void __iomem *regs;
	struct regmap *grf;
	struct clk *aclk;
	struct clk *hclk;
	struct reset_control *reset;
	struct reset_control *areset;
	struct reset_control *hreset;

	struct dsmc_lb_slv_map lb_slv_mem;
};

static int rk3506_dsmc_lb_slv_init(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_active;
	struct rockchip_dsmc_lb_slave *priv;

	priv = platform_get_drvdata(pdev);

	if (IS_ERR_OR_NULL(priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return -ENODEV;
	}

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "Failed to get pinctrl\n");
		return PTR_ERR(pinctrl);
	}

	pinctrl_active = pinctrl_lookup_state(pinctrl, "active");
	if (IS_ERR(pinctrl_active)) {
		dev_err(dev, "Failed to lookup active pinctrl state\n");
		return PTR_ERR(pinctrl_active);
	}

	/* enable dsmc slave and rdyn to normal mode */
	regmap_write(priv->grf, RK3506_GRF_SOC_CON(1),
		     DSMC_SLAVE_ENABLE(1) | DSMC_SLAVE_RDYN_MODE(1));

	/* The active iomux setting should be enabled after the pull-up week. */
	ret = pinctrl_select_state(pinctrl, pinctrl_active);
	if (ret) {
		dev_err(dev, "Failed to select active pinctrl state\n");
		return ret;
	}

	return ret;
}

static const struct dsmc_soc_info rk3506_soc_info = {
	.dsmc_soc_init = rk3506_dsmc_lb_slv_init,
};

static const struct of_device_id dsmc_lb_slave_of_match[] = {
	{ .compatible = "rockchip,rk3506-dsmc-lb-slave", .data = &rk3506_soc_info },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, dsmc_lb_slave_of_match);

static int rockchip_dsmc_platform_init(struct platform_device *pdev)
{
	struct rockchip_dsmc_lb_slave *priv;
	struct device *dev = &pdev->dev;
	int ret = 0;

	priv = platform_get_drvdata(pdev);

	priv->soc_info = device_get_match_data(dev);
	if (!priv->soc_info) {
		dev_err(dev, "Error: No device match found\n");
		return -ENODEV;
	}

	ret = priv->soc_info->dsmc_soc_init(pdev);
	if (ret) {
		dev_err(dev, "Error: Soc init fail!\n");
		return ret;
	}

	return ret;
}

static int dsmc_lb_slave_init(struct rockchip_dsmc_lb_slave *priv)
{
	struct device *dev = priv->dev;

	if (priv->lb_slv_mem.phys < AXI_ADDR_4GB_RANGE) {
		writel(priv->lb_slv_mem.phys, priv->regs + AXI_WR_ADDR_BASE);
		writel(priv->lb_slv_mem.phys, priv->regs + AXI_RD_ADDR_BASE);
	} else {
		dev_err(dev, "Error: Invalid address for slave memory!\n");
		return -ENODEV;
	}

	/* clear all h2s interrupt */
	writel(APP_H2S_INT_STA_MASK << APP_H2S_INT_STA_SHIFT,
	       priv->regs + APP_H2S_INT_STA);

	/* enable all h2s interrupt */
	writel(0xffffffff, priv->regs + APP_H2S_INT_STA_EN);
	writel(0xffffffff, priv->regs + APP_H2S_INT_STA_SIG_EN);

	return 0;
}

static int dsmc_lb_slave_dma_trigger(struct rockchip_dsmc_lb_slave *priv)
{
	int timeout = 1000;

	/* wait interrupt register empty */
	while (timeout-- > 0) {
		if (!(readl(priv->regs + LBC_S2H_INT_STA) &
		      (0x1 << S2H_INT_FOR_DMA_NUM)))
			break;
		udelay(1);
		if (timeout == 0) {
			dev_err(priv->dev, "Timeout waiting for s2h interrupt empty!\n");
			return -ETIMEDOUT;
		}
	}

	/* trigger a slave to host interrupt which will start dma hardware mode copy */
	writel(0x1, priv->regs + APP_CON(S2H_INT_FOR_DMA_NUM));

	return 0;
}

static irqreturn_t rockchip_dsmc_lb_slave_irq(int irq, void *data)
{
	struct rockchip_dsmc_lb_slave *priv = data;

	if (readl(priv->regs + LBC_CON(15)))
		dsmc_lb_slave_dma_trigger(priv);

	/* clear all h2s interrupt */
	writel(APP_H2S_INT_STA_MASK << APP_H2S_INT_STA_SHIFT,
	       priv->regs + APP_H2S_INT_STA);

	return IRQ_HANDLED;
}

static int rk_dsmc_lb_slave_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *slv_map = NULL;
	struct rockchip_dsmc_lb_slave *priv;
	struct resource *mem;
	int irq, ret = 0;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	priv->grf = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
	if (IS_ERR(priv->grf))
		dev_warn(dev, "Missing rockchip,grf property\n");

	ret = rockchip_dsmc_platform_init(pdev);
	if (ret)
		return ret;

	priv->reset = devm_reset_control_get(dev, "dsmc_slv");
	if (IS_ERR(priv->reset)) {
		ret = PTR_ERR(priv->reset);
		dev_warn(dev, "failed to get dsmc slave reset: %d\n", ret);
	}
	priv->areset = devm_reset_control_get(dev, "a_dsmc_slv");
	if (IS_ERR(priv->areset)) {
		ret = PTR_ERR(priv->areset);
		dev_warn(dev, "failed to get dsmc slave areset: %d\n", ret);
	}
	priv->hreset = devm_reset_control_get(dev, "h_dsmc_slv");
	if (IS_ERR(priv->hreset)) {
		ret = PTR_ERR(priv->hreset);
		dev_warn(dev, "failed to get dsmc slave hreset: %d\n", ret);
	}

	priv->aclk = devm_clk_get(dev, "aclk_dsmc_slv");
	if (IS_ERR(priv->aclk)) {
		dev_err(dev, "Can't get dsmc_slv aclk\n");
		return PTR_ERR(priv->aclk);
	}
	priv->hclk = devm_clk_get(dev, "hclk_dsmc_slv");
	if (IS_ERR(priv->hclk)) {
		dev_err(dev, "Can't get dsmc_slv hclk\n");
		return PTR_ERR(priv->hclk);
	}
	ret = clk_prepare_enable(priv->aclk);
	if (ret) {
		dev_err(dev, "Can't prepare enable dsmc aclk: %d\n", ret);
		goto err_dis_aclk;
	}
	ret = clk_prepare_enable(priv->hclk);
	if (ret) {
		dev_err(dev, "Can't prepare enable dsmc hclk: %d\n", ret);
		goto err_dis_hclk;
	}

	slv_map = of_parse_phandle(np, "memory-region", 0);
	if (!slv_map) {
		ret = -EINVAL;
		dev_err(dev, "missing memory-region property\n");
		goto err_dis_hclk;
	}

	ret = of_address_to_resource(slv_map, 0, mem);
	of_node_put(slv_map);
	if (ret < 0) {
		dev_err(dev, "memory-region missing reg property\n");
		goto err_dis_hclk;
	}
	if (resource_size(mem) <= 0) {
		ret = -EINVAL;
		dev_err(dev, "memory-region size error\n");
		goto err_dis_hclk;
	}

	priv->lb_slv_mem.phys = mem->start;
	priv->lb_slv_mem.size = resource_size(mem);

	priv->dev = dev;
	if (dsmc_lb_slave_init(priv)) {
		ret = -ENODEV;
		dev_err(dev, "dsmc local bus slave init fail!\n");
		goto err_dis_hclk;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = -ENODEV;
		dev_err(dev, "cannot find dsmc lb slave IRQ\n");
		goto err_dis_hclk;
	}
	ret = devm_request_irq(dev, irq, rockchip_dsmc_lb_slave_irq,
			       0, dev_name(dev), priv);
	if (ret < 0) {
		dev_err(dev, "cannot request IRQ\n");
		goto err_dis_hclk;
	}

	dev_info(dev, "rockchip dsmc local bus slave driver initialized\n");

	return 0;

err_dis_hclk:
	clk_disable_unprepare(priv->hclk);
err_dis_aclk:
	clk_disable_unprepare(priv->aclk);
	return ret;
}

static int rk_dsmc_lb_slave_remove(struct platform_device *pdev)
{
	struct rockchip_dsmc_lb_slave *priv;

	priv = platform_get_drvdata(pdev);

	if (priv->aclk) {
		clk_disable_unprepare(priv->aclk);
		priv->aclk = NULL;
	}
	if (priv->hclk) {
		clk_disable_unprepare(priv->hclk);
		priv->hclk = NULL;
	}

	return 0;
}

struct platform_driver rk_dsmc_lb_slave_driver = {
	.probe = rk_dsmc_lb_slave_probe,
	.remove = rk_dsmc_lb_slave_remove,
	.driver = {
		.name = "dsmc_lb_slave",
		.of_match_table = dsmc_lb_slave_of_match,
	},
};

module_platform_driver(rk_dsmc_lb_slave_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhihuan He <huan.he@rock-chips.com>");
MODULE_DESCRIPTION("ROCKCHIP DSMC SLAVE driver");
