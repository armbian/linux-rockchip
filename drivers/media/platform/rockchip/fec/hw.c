// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include "fec_offline.h"
#include "hw.h"
#include "regs.h"
#include "version.h"

#define RKFEC_VERNO_LEN 10

static bool rkfec_clk_dbg;
module_param_named(clk_dbg, rkfec_clk_dbg, bool, 0644);
MODULE_PARM_DESC(clk_dbg, "rkfec clk set by user");

static char rkfec_version[RKFEC_VERNO_LEN];
module_param_string(version, rkfec_version, RKFEC_VERNO_LEN, 0444);
MODULE_PARM_DESC(version, "version number");

static const char * const rv1126b_fec_clks[] = {
	"aclk_fec",
	"hclk_fec",
	"clk_fec",
};

static int rkfec_set_clk_rate(struct clk *clk, unsigned long rate)
{
	if (rkfec_clk_dbg)
		return 0;

	return clk_set_rate(clk, rate);
}

static void rkfec_soft_reset(struct rkfec_hw_dev *hw)
{
	u32 val;

	/* reset */
	if (hw->reset) {
		reset_control_assert(hw->reset);
		udelay(20);
		reset_control_deassert(hw->reset);
		udelay(20);
	}

	/* refresh iommu after reset */
	if (hw->is_mmu) {
		rockchip_iommu_disable(hw->dev);
		rockchip_iommu_enable(hw->dev);
	}

	/* clk_dis */
	writel(0, hw->base_addr + RKFEC_CLK_DIS);

	/* int en */
	val = FRM_END_P_FEC;
	writel(val, hw->base_addr + RKFEC_INT_EN);
}

static inline bool is_iommu_enable(struct device *dev)
{
	struct device_node *iommu;

	iommu = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!iommu) {
		dev_info(dev, "no iommu attached, using non-iommu buffers\n");
		return false;
	} else if (!of_device_is_available(iommu)) {
		dev_info(dev, "iommu is disable, using non-iommu buffers\n");
		of_node_put(iommu);
		return false;
	}
	of_node_put(iommu);

	return true;
}

static void disable_sys_clk(struct rkfec_hw_dev *dev)
{
	int i;

	for (i = 0; i < dev->clks_num; i++)
		clk_disable_unprepare(dev->clks[i]);
}

static int enable_sys_clk(struct rkfec_hw_dev *dev)
{
	int i, ret;

	for (i = 0; i < dev->clks_num; i++) {
		ret = clk_prepare_enable(dev->clks[i]);
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	for (--i; i >= 0; --i)
		clk_disable_unprepare(dev->clks[i]);
	return ret;
}

static irqreturn_t rkfec_irq_hdl(int irq, void *ctx)
{
	struct device *dev = ctx;

	struct rkfec_hw_dev *hw_dev = dev_get_drvdata(dev);
	void __iomem *base = hw_dev->base_addr;
	unsigned int mis_val;

	spin_lock(&hw_dev->irq_lock);
	mis_val = readl(base + RKFEC_INT_MSK);
	writel(mis_val, base + RKFEC_INT_CLR);
	spin_unlock(&hw_dev->irq_lock);

	v4l2_dbg(3, rkfec_debug, &hw_dev->ofl_dev.v4l2_dev,
		 "fec isr:0x%x\n", mis_val);

	hw_dev->ofl_dev.isr_cnt++;

	if (mis_val & FRM_END_P_FEC) {
		mis_val &= -FRM_END_P_FEC;
		rkfec_offline_irq(hw_dev, mis_val);
	}

	if (mis_val & (BIT(2) | BIT(3) | BIT(4) | BIT(5)))
		hw_dev->ofl_dev.err_cnt++;

	return IRQ_HANDLED;
}

static const struct fec_clk_info rv1126b_fec_clk_rate[] = {
	{
		.clk_rate = 300,
		.refer_data = 1920,
	}, {
		.clk_rate = 400,
		.refer_data = 2688,
	}, {
		.clk_rate = 500,
		.refer_data = 3072,
	}, {
		.clk_rate = 500,
		.refer_data = 3840,
	}, {
		.clk_rate = 702,
		.refer_data = 4672,
	}
};

static struct irqs_data rv1126b_fec_irqs[] = {
	{"fec_irq", rkfec_irq_hdl},
};

static const struct fec_match_data rv1126b_fec_match_data = {
	.fec_ver = RKFEC_V20,
	.clks = rv1126b_fec_clks,
	.clks_num = ARRAY_SIZE(rv1126b_fec_clks),
	.clk_rate_tbl = rv1126b_fec_clk_rate,
	.clk_rate_tbl_num = ARRAY_SIZE(rv1126b_fec_clk_rate),
	.irqs = rv1126b_fec_irqs,
	.num_irqs = ARRAY_SIZE(rv1126b_fec_irqs),
};

static const struct of_device_id rkfec_hw_of_match[] = {
	{
		.compatible = "rockchip,rv1126b-rkfec",
		.data = &rv1126b_fec_match_data,
	},
	{},
};

static int rkfec_hw_probe(struct platform_device *pdev)
{
	const struct fec_match_data *match_data;
	struct device *dev = &pdev->dev;
	struct rkfec_hw_dev *hw_dev;
	struct resource *res;
	int i, ret, irq;
	bool is_mem_reserved = true;

	snprintf(rkfec_version, sizeof(rkfec_version), "v%02x.%02x.%02x",
		RKFEC_DRIVER_VERSION >> 16,
		(RKFEC_DRIVER_VERSION & 0xff00) >> 8,
		RKFEC_DRIVER_VERSION & 0xff);

	dev_info(dev, "rkfec driver version: %s\n", rkfec_version);

	match_data = device_get_match_data(&pdev->dev);
	if (!match_data) {
		dev_err(dev, "no of match data provided\n");
		return -EINVAL;
	}

	hw_dev = devm_kzalloc(dev, sizeof(*hw_dev), GFP_KERNEL);
	if (!hw_dev)
		return -ENOMEM;

	dev_set_drvdata(dev, hw_dev);
	hw_dev->dev = dev;
	hw_dev->match_data = match_data;
	hw_dev->fec_ver = match_data->fec_ver;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "get resource failed\n");
		ret = -EINVAL;
		goto err;
	}

	hw_dev->base_addr = devm_ioremap_resource(dev, res);
	if (PTR_ERR(hw_dev->base_addr) == -EBUSY) {
		resource_size_t offset = res->start;
		resource_size_t size = resource_size(res);

		hw_dev->base_addr = devm_ioremap(dev, offset, size);
	}
	if (IS_ERR(hw_dev->base_addr)) {
		dev_err(dev, "ioremap failed\n");
		ret = PTR_ERR(hw_dev->base_addr);
		goto err;
	}

	/* three are irq names in dts */
	spin_lock_init(&hw_dev->irq_lock);
	for (i = 0; i < match_data->num_irqs; i++) {
		irq = platform_get_irq_byname(pdev,
				match_data->irqs[i].name);
		if (irq < 0) {
			dev_err(dev, "no irq %s in dts\n",
					match_data->irqs[i].name);
			ret = irq;
			goto err;
		}
		ret = devm_request_irq(dev, irq,
				match_data->irqs[i].irq_hdl,
				IRQF_SHARED,
				dev_driver_string(dev),
				dev);
		if (ret < 0) {
			dev_err(dev, "request %s failed: %d\n",
					match_data->irqs[i].name, ret);
			goto err;
		}

		dev_err(dev, "request %s : %d\n",
			match_data->irqs[i].name, irq);
	}

	for (i = 0; i < match_data->clks_num; i++) {
		struct clk *clk = devm_clk_get(dev, match_data->clks[i]);

		if (IS_ERR(clk)) {
			dev_err(dev, "failed to get %s\n",
					match_data->clks[i]);
			ret = PTR_ERR(clk);
			goto err;
		}
		hw_dev->clks[i] = clk;
	}
	hw_dev->clks_num = match_data->clks_num;
	hw_dev->clk_rate_tbl = match_data->clk_rate_tbl;
	hw_dev->clk_rate_tbl_num = match_data->clk_rate_tbl_num;

	hw_dev->reset = devm_reset_control_array_get(dev, false, false);
	if (IS_ERR(hw_dev->reset)) {
		dev_info(dev, "failed to get cru reset, error = %ld\n", PTR_ERR(hw_dev->reset));
		hw_dev->reset = NULL;
	}

	mutex_init(&hw_dev->dev_lock);
	hw_dev->is_idle = true;
	hw_dev->is_dma_config = true;
	hw_dev->is_dma_sg_ops = true;
	hw_dev->is_shutdown = false;
	hw_dev->is_mmu = is_iommu_enable(dev);
	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		is_mem_reserved = false;
		if (!hw_dev->is_mmu)
			dev_info(dev, "No reserved memory region. default cma area!\n");
	}
	if (hw_dev->is_mmu && !is_mem_reserved)
		hw_dev->is_dma_config = false;
	hw_dev->mem_ops = &vb2_cma_sg_memops;
	hw_dev->soft_reset = rkfec_soft_reset;
	hw_dev->set_clk = rkfec_set_clk_rate;

	rkfec_register_offline(hw_dev);

	dev_info(dev, "%s success\n", __func__);

	pm_runtime_enable(&pdev->dev);

	return 0;
err:
	return ret;
}

static int rkfec_hw_remove(struct platform_device *pdev)
{
	struct rkfec_hw_dev *hw_dev = platform_get_drvdata(pdev);

	rkfec_unregister_offline(hw_dev);
	pm_runtime_disable(&pdev->dev);
	mutex_destroy(&hw_dev->dev_lock);
	return 0;
}

static void rkfec_hw_shutdown(struct platform_device *pdev)
{
	struct rkfec_hw_dev *hw_dev = platform_get_drvdata(pdev);
	u32 val;

	hw_dev->is_shutdown = true;
	if (pm_runtime_active(&pdev->dev)) {
		writel(0, hw_dev->base_addr + RKFEC_INT_EN);

		val = SYS_SOFT_RST_FBCE | SYS_SOFT_RST_ACLK;
		writel(val, hw_dev->base_addr + RKFEC_CLK_DIS);
		udelay(10);
		writel(~val, hw_dev->base_addr + RKFEC_CLK_DIS);
	}
	dev_info(&pdev->dev, "%s\n", __func__);
}

static int __maybe_unused rkfec_hw_runtime_suspend(struct device *dev)
{
	struct rkfec_hw_dev *hw_dev = dev_get_drvdata(dev);

	if (rkfec_debug >= 4)
		dev_info(dev, "%s enter\n", __func__);

	if (dev->power.runtime_status)
		writel(0, hw_dev->base_addr + RKFEC_INT_EN);

	disable_sys_clk(hw_dev);

	if (rkfec_debug >= 4)
		dev_info(dev, "%s exit\n", __func__);

	return 0;
}

static int __maybe_unused rkfec_hw_runtime_resume(struct device *dev)
{
	struct rkfec_hw_dev *hw_dev = dev_get_drvdata(dev);

	if (rkfec_debug >= 4)
		dev_info(dev, "%s enter\n", __func__);

	enable_sys_clk(hw_dev);
	rkfec_soft_reset(hw_dev);

	if (dev->power.runtime_status) {
		//toto
	} else {
		//toto
	}

	hw_dev->is_idle = true;

	if (rkfec_debug >= 4)
		dev_info(dev, "%s exit\n", __func__);

	return 0;
}

static const struct dev_pm_ops rkfec_hw_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
			   pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(rkfec_hw_runtime_suspend,
			   rkfec_hw_runtime_resume, NULL)
};

static struct platform_driver rkfec_hw_drv = {
	.driver = {
		.name = "rkfec_hw",
		.of_match_table = of_match_ptr(rkfec_hw_of_match),
		.pm = &rkfec_hw_pm_ops,
	},
	.probe = rkfec_hw_probe,
	.remove = rkfec_hw_remove,
	.shutdown = rkfec_hw_shutdown,
};

static int __init rkfec_hw_drv_init(void)
{
	int ret;

	ret = platform_driver_register(&rkfec_hw_drv);

	return ret;
}

static void __exit rkfec_hw_drv_exit(void)
{
	platform_driver_unregister(&rkfec_hw_drv);
}

module_init(rkfec_hw_drv_init);
module_exit(rkfec_hw_drv_exit);
