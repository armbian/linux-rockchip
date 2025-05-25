// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#define pr_fmt(fmt) "rkaiisp: %s:%d " fmt, __func__, __LINE__

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <media/videobuf2-cma-sg.h>
#include <media/videobuf2-dma-sg.h>
#include <soc/rockchip/rockchip_iommu.h>

#include "regs.h"
#include "hw.h"
#include "aiisp.h"

static struct rkaiisp_hw_dev *rkaiisp_hwdev;

/*
 * rkaiisp_hw share hardware resource with rkaiisp virtual device
 * rkaiisp_device rkaiisp_device rkaiisp_device rkaiisp_device
 *      |            |            |            |
 *      \            |            |            /
 *       --------------------------------------
 *                         |
 *                     rkaiisp_hw
 */

static irqreturn_t hw_irq_hdl(int irq, void *ctx)
{
	struct device *dev = ctx;
	struct rkaiisp_hw_dev *hw_dev = dev_get_drvdata(dev);
	struct rkaiisp_device *aidev = hw_dev->aidev[hw_dev->cur_dev_id];
	void __iomem *base = hw_dev->base_addr;
	int i, max = 0, id = 0;
	int len[RKAIISP_DEV_MAX] = { 0 };
	enum rkaiisp_irqhdl_ret irq_hdl_ret;
	u32 mis_val;

	mis_val = readl(base + AIISP_MI_MIS);
	if (mis_val) {
		irq_hdl_ret = rkaiisp_irq_hdl(aidev, mis_val);
		if (irq_hdl_ret == RUN_COMPLETE) {
			spin_lock(&hw_dev->hw_lock);
			for (i = 0; i < RKAIISP_DEV_MAX; i++) {
				aidev = hw_dev->aidev[i];
				if (!aidev)
					continue;

				if (!aidev->streamon)
					continue;

				len[i] = rkaiisp_get_idxbuf_len(aidev);
				if (max < len[i]) {
					max = len[i];
					id = i;
				}
			}

			if (max > 0) {
				hw_dev->is_idle = false;
				hw_dev->cur_dev_id = id;
				aidev = hw_dev->aidev[hw_dev->cur_dev_id];
				spin_unlock(&hw_dev->hw_lock);
				v4l2_dbg(1, rkaiisp_debug, &aidev->v4l2_dev,
					"trigger aidev: %d, idxbuf len: %d\n",
					hw_dev->cur_dev_id, max);
				rkaiisp_trigger(aidev);
			} else {
				hw_dev->is_idle = true;
				spin_unlock(&hw_dev->hw_lock);
			}
		}
	}

	return IRQ_HANDLED;
}

static int rkaiisp_register_irq(struct rkaiisp_hw_dev *hw_dev)
{
	const struct aiisp_match_data *match_data = hw_dev->match_data;
	struct platform_device *pdev = hw_dev->pdev;
	struct device *dev = &pdev->dev;
	int ret, irq;

	irq = platform_get_irq_byname(pdev, "irq");
	if (irq < 0) {
		dev_err(dev, "no irq %s in dts\n",
			match_data->irqs[0].name);
		return irq;
	}

	ret = devm_request_irq(dev, irq,
			       match_data->irqs[0].irq_hdl,
			       0,
			       dev_driver_string(dev),
			       dev);
	if (ret < 0) {
		dev_err(dev, "request %s failed: %d\n",
			match_data->irqs[0].name, ret);
		return ret;
	}

	return 0;
}

int rkaiisp_ispidx_queue(int dev_id, struct rkisp_aiisp_st *idxbuf)
{
	struct rkaiisp_hw_dev *hw_dev = rkaiisp_hwdev;
	struct rkaiisp_device *aidev = NULL;
	union rkaiisp_queue_buf queue_buf;
	int i;

	if (!hw_dev) {
		pr_err("Can not find hwdev!");
		return -EINVAL;
	}

	for (i = 0; i < hw_dev->dev_num; i++) {
		if (hw_dev->aidev[i]) {
			if ((hw_dev->aidev[i]->is_hw_link) && hw_dev->aidev[i]->dev_id == dev_id) {
				aidev = hw_dev->aidev[i];
				break;
			}
		}
	}

	if (!aidev) {
		pr_err("Can not find aidev for dev_id %d!", dev_id);
		return -EINVAL;
	}

	if (aidev->exemode != BOTHEVENT_TO_AIQ) {
		pr_err("aidev %d exemode(%d) is not right!", dev_id, aidev->exemode);
		return -EINVAL;
	}

	queue_buf.aibnr_st = *idxbuf;
	return rkaiisp_queue_ispbuf(aidev, &queue_buf);
}
EXPORT_SYMBOL(rkaiisp_ispidx_queue);

static const char * const rv1126b_clks[] = {
	"clk_aiisp_core",
	"aclk_aiisp",
	"hclk_aiisp",
};

static const struct aiisp_clk_info rv1126b_clk_rate[] = {
	{
		.clk_rate = 400,
		.refer_data = 1920, //width
	}, {
		.clk_rate = 400,
		.refer_data = 2688,
	}, {
		.clk_rate = 500,
		.refer_data = 3072,
	}, {
		.clk_rate = 600,
		.refer_data = 3840,
	}, {
		.clk_rate = 702,
		.refer_data = 4672,
	}
};

static struct aiisp_irqs_data rv1126b_irqs[] = {
	{"irq", hw_irq_hdl},
};

static const struct aiisp_match_data rv1126b_match_data = {
	.clks = rv1126b_clks,
	.num_clks = ARRAY_SIZE(rv1126b_clks),
	.clk_rate_tbl = rv1126b_clk_rate,
	.num_clk_rate_tbl = ARRAY_SIZE(rv1126b_clk_rate),
	.irqs = rv1126b_irqs,
	.num_irqs = ARRAY_SIZE(rv1126b_irqs),
};

static const struct of_device_id rkaiisp_hw_of_match[] = {
	{
		.compatible = "rockchip,rv1126b-rkaiisp",
		.data = &rv1126b_match_data,
	},
	{},
};

static inline bool is_iommu_enable(struct device *dev)
{
	struct device_node *iommu;

	iommu = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!iommu) {
		dev_info(dev, "no iommu attached, using non-iommu buffers\n");
		return false;
	} else if (!of_device_is_available(iommu)) {
		dev_info(dev, "iommu is disabled, using iommu buffers\n");
		of_node_put(iommu);
		return false;
	}
	of_node_put(iommu);

	return true;
}

static void rkaiisp_soft_reset(struct rkaiisp_hw_dev *dev, bool is_secure)
{
	void __iomem *base = dev->base_addr;

	if (is_secure) {
		/* if aidev working, cru reset isn't secure.
		 * aidev soft reset first to protect aidev reset.
		 */
		writel(0x1, base + AIISP_CORE_SOFT_RST);
		udelay(10);
	}

	if (dev->reset) {
		reset_control_assert(dev->reset);
		udelay(10);
		reset_control_deassert(dev->reset);
		udelay(10);
	}

	/* refresh iommu after reset */
	if (dev->is_mmu) {
		rockchip_iommu_disable(dev->dev);
		rockchip_iommu_enable(dev->dev);
	}
}

static void disable_sys_clk(struct rkaiisp_hw_dev *dev)
{
	int i;

	for (i = dev->num_clks - 1; i >= 0; i--)
		if (!IS_ERR(dev->clks[i]))
			clk_disable_unprepare(dev->clks[i]);
}

static int enable_sys_clk(struct rkaiisp_hw_dev *dev)
{
	int i, ret = -EINVAL;

	for (i = 0; i < dev->num_clks; i++) {
		if (!IS_ERR(dev->clks[i])) {
			ret = clk_prepare_enable(dev->clks[i]);
			if (ret < 0)
				goto err;
		}
	}

	rkaiisp_soft_reset(dev, false);
	return 0;
err:
	for (--i; i >= 0; --i)
		if (!IS_ERR(dev->clks[i]))
			clk_disable_unprepare(dev->clks[i]);
	return ret;
}

static int rkaiisp_hw_probe(struct platform_device *pdev)
{
	const struct aiisp_match_data *match_data;
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct rkaiisp_hw_dev *hw_dev;
	struct resource *res;
	int i, ret;
	bool is_mem_reserved = true;

	match_data = of_device_get_match_data(dev);
	hw_dev = devm_kzalloc(dev, sizeof(*hw_dev), GFP_KERNEL);
	if (!hw_dev)
		return -ENOMEM;

	dev_set_drvdata(dev, hw_dev);
	hw_dev->dev = dev;

	hw_dev->grf = syscon_regmap_lookup_by_phandle(node, "rockchip,grf");
	if (IS_ERR(hw_dev->grf))
		dev_warn(dev, "Missing rockchip,grf property\n");

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

	hw_dev->pdev = pdev;
	hw_dev->match_data = match_data;

	for (i = 0; i < match_data->num_clks; i++) {
		struct clk *clk = devm_clk_get(dev, match_data->clks[i]);

		if (IS_ERR(clk)) {
			dev_err(dev, "failed to get %s\n", match_data->clks[i]);
			ret = PTR_ERR(clk);
			goto err;
		}
		hw_dev->clks[i] = clk;
	}
	hw_dev->num_clks = match_data->num_clks;
	hw_dev->clk_rate_tbl = match_data->clk_rate_tbl;
	hw_dev->num_clk_rate_tbl = match_data->num_clk_rate_tbl;

	hw_dev->reset = devm_reset_control_array_get(dev, false, false);
	if (IS_ERR(hw_dev->reset)) {
		dev_dbg(dev, "failed to get reset\n");
		hw_dev->reset = NULL;
	}

	hw_dev->dev_num = 0;
	hw_dev->cur_dev_id = 0;
	mutex_init(&hw_dev->dev_mutex);
	spin_lock_init(&hw_dev->hw_lock);
	atomic_set(&hw_dev->refcnt, 0);

	hw_dev->is_idle = true;
	hw_dev->is_single = true;
	hw_dev->is_dma_contig = true;
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
		hw_dev->is_dma_contig = false;
	hw_dev->mem_ops = &vb2_cma_sg_memops;

	rkaiisp_hwdev = hw_dev;
	rkaiisp_register_irq(hw_dev);
	pm_runtime_enable(dev);

	dev_info(dev, "probe end.\n");
	return 0;
err:
	return ret;
}

static int rkaiisp_hw_remove(struct platform_device *pdev)
{
	struct rkaiisp_hw_dev *hw_dev = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	mutex_destroy(&hw_dev->dev_mutex);
	rkaiisp_hwdev = NULL;
	return 0;
}

static void rkaiisp_hw_shutdown(struct platform_device *pdev)
{
	struct rkaiisp_hw_dev *hw_dev = platform_get_drvdata(pdev);

	hw_dev->is_shutdown = true;
}

static int __maybe_unused rkaiisp_runtime_suspend(struct device *dev)
{
	struct rkaiisp_hw_dev *hw_dev = dev_get_drvdata(dev);

	hw_dev->is_idle = true;
	disable_sys_clk(hw_dev);
	return pinctrl_pm_select_sleep_state(dev);
}

static int __maybe_unused rkaiisp_runtime_resume(struct device *dev)
{
	struct rkaiisp_hw_dev *hw_dev = dev_get_drvdata(dev);
	int ret;

	ret = pinctrl_pm_select_default_state(dev);
	if (ret < 0)
		return ret;

	enable_sys_clk(hw_dev);

	return 0;
}

static const struct dev_pm_ops rkaiisp_hw_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(rkaiisp_runtime_suspend, rkaiisp_runtime_resume, NULL)
};

static struct platform_driver rkaiisp_hw_drv = {
	.driver = {
		.name = "rkaiisp_hw",
		.of_match_table = of_match_ptr(rkaiisp_hw_of_match),
		.pm = &rkaiisp_hw_pm_ops,
	},
	.probe = rkaiisp_hw_probe,
	.remove = rkaiisp_hw_remove,
	.shutdown = rkaiisp_hw_shutdown,
};

static int __init rkaiisp_hw_drv_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&rkaiisp_hw_drv);
	if (!ret)
		ret = platform_driver_register(&rkaiisp_plat_drv);

	return ret;
}

static void __exit rkaiisp_hw_drv_exit(void)
{
	platform_driver_unregister(&rkaiisp_plat_drv);
	platform_driver_unregister(&rkaiisp_hw_drv);
}

module_init(rkaiisp_hw_drv_init);
module_exit(rkaiisp_hw_drv_exit);
