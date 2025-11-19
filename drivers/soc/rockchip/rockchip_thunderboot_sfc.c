// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 */
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/rockchip/rockchip_thunderboot.h>

#define SFC_ICLR	0x08
#define SFC_SR		0x24
#define SFC_RAWISR	0x28

/* SFC_SR Register */
#define SFC_BUSY	BIT(0)

/* SFC_RAWISR Register */
#define DMA_INT		BIT(7)

static int rk_tb_sfc_thread(void *p)
{
	int ret = 0;
	struct platform_device *pdev = p;
	void __iomem *regs;
	struct resource *res;
	struct device *dev = &pdev->dev;
	u32 status;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = ioremap(res->start, resource_size(res));
	if (!regs) {
		dev_err(dev, "ioremap failed for resource %pR\n", res);
		return -ENOMEM;
	}

#ifdef SFC_DEBUG
	print_hex_dump(KERN_WARNING, "tb_sfc", DUMP_PREFIX_OFFSET, 4, 4, regs, 0x60, 0);
#endif

	ret = readl_poll_timeout(regs + SFC_SR, status,
				 !(status & SFC_BUSY), 100,
				 5000 * USEC_PER_MSEC);
	if (ret) {
		dev_err(dev, "Wait for SFC idle timeout!\n");
		goto out;
	} else {
		if (likely(readl(regs + SFC_RAWISR) & DMA_INT))
			dev_err(dev, "DMA finished!\n");
		else
			dev_err(dev, "Last transfer non DMA!\n");
	}

	rk_tb_ramdisk_compress_done();
	rk_tb_prepare_ramdisk_decompress(dev);

out:
	iounmap(regs);

	return 0;
}

static int __init rk_tb_sfc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct task_struct *tsk;

	tsk = kthread_run(rk_tb_sfc_thread, pdev, "tb_sfc");
	if (IS_ERR(tsk)) {
		ret = PTR_ERR(tsk);
		dev_err(&pdev->dev, "start thread failed (%d)\n", ret);
	}

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id rk_tb_sfc_dt_match[] = {
	{ .compatible = "rockchip,thunder-boot-sfc" },
	{},
};
#endif

static struct platform_driver rk_tb_sfc_driver = {
	.driver		= {
		.name	= "rockchip_thunder_boot_sfc",
		.of_match_table = rk_tb_sfc_dt_match,
	},
};

static int __init rk_tb_sfc_init(void)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, rk_tb_sfc_dt_match);
	if (node) {
		of_platform_device_create(node, NULL, NULL);
		of_node_put(node);
		return platform_driver_probe(&rk_tb_sfc_driver, rk_tb_sfc_probe);
	}

	return 0;
}

pure_initcall(rk_tb_sfc_init);
