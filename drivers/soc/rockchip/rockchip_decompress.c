// SPDX-License-Identifier:     GPL-2.0+
/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/initramfs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/soc/rockchip/rockchip_decompress.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>

#define DECOM_CTRL		0x0
#define DECOM_ENR		0x4
#define DECOM_RADDR		0x8
#define DECOM_WADDR		0xc
#define DECOM_UDDSL		0x10
#define DECOM_UDDSH		0x14
#define DECOM_TXTHR		0x18
#define DECOM_RXTHR		0x1c
#define DECOM_SLEN		0x20
#define DECOM_STAT		0x24
#define DECOM_ISR		0x28
#define DECOM_IEN		0x2c
#define DECOM_AXI_STAT		0x30
#define DECOM_TSIZEL		0x34
#define DECOM_TSIZEH		0x38
#define DECOM_MGNUM		0x3c
#define DECOM_FRAME		0x40
#define DECOM_DICTID		0x44
#define DECOM_CSL		0x48
#define DECOM_CSH		0x4c
#define DECOM_LMTSL		0x50
#define DECOM_LMTSH		0x54

#define LZ4_HEAD_CSUM_CHECK_EN	BIT(1)
#define LZ4_BLOCK_CSUM_CHECK_EN	BIT(2)
#define LZ4_CONT_CSUM_CHECK_EN	BIT(3)

#define DSOLIEN			BIT(19)
#define ZDICTEIEN		BIT(18)
#define GCMEIEN			BIT(17)
#define GIDEIEN			BIT(16)
#define CCCEIEN			BIT(15)
#define BCCEIEN			BIT(14)
#define HCCEIEN			BIT(13)
#define CSEIEN			BIT(12)
#define DICTEIEN		BIT(11)
#define VNEIEN			BIT(10)
#define WNEIEN			BIT(9)
#define RDCEIEN			BIT(8)
#define WRCEIEN			BIT(7)
#define DISEIEN			BIT(6)
#define LENEIEN			BIT(5)
#define LITEIEN			BIT(4)
#define SQMEIEN			BIT(3)
#define SLCIEN			BIT(2)
#define HDEIEN			BIT(1)
#define DSIEN			BIT(0)

#define DECOM_STOP		BIT(0)
#define DECOM_COMPLETE		BIT(0)
#define DECOM_GZIP_MODE		BIT(4)
#define DECOM_ZLIB_MODE		BIT(5)
#define DECOM_DEFLATE_MODE	BIT(0)

#define DECOM_ENABLE		0x1
#define DECOM_DISABLE		0x0

#define DECOM_INT_MASK \
	(DSOLIEN | ZDICTEIEN | GCMEIEN | GIDEIEN | \
	CCCEIEN | BCCEIEN | HCCEIEN | CSEIEN | \
	DICTEIEN | VNEIEN | WNEIEN | RDCEIEN | WRCEIEN | \
	DISEIEN | LENEIEN | LITEIEN | SQMEIEN | SLCIEN | \
	HDEIEN | DSIEN)

struct rk_decom {
	struct device *dev;
	int irq;
	int num_clocks;
	struct clk_bulk_data *clocks;
	void __iomem *regs;
	phys_addr_t mem_start;
	size_t mem_size;
	struct reset_control *reset;
};

static struct rk_decom *g_decom;

static DECLARE_WAIT_QUEUE_HEAD(g_decom_wait);
static bool g_decom_complete;
static bool g_decom_noblocking;
static u64 g_decom_data_len;

void __init wait_initrd_hw_decom_done(void)
{
	wait_event(g_decom_wait, g_decom_complete);
}

int rk_decom_wait_done(u32 timeout, u64 *decom_len)
{
	int ret;

	if (!decom_len)
		return -EINVAL;

	ret = wait_event_timeout(g_decom_wait, g_decom_complete, timeout * HZ);
	if (!ret) {
		if (g_decom)
			clk_bulk_disable_unprepare(g_decom->num_clocks, g_decom->clocks);

		return -ETIMEDOUT;
	}

	*decom_len = g_decom_data_len;

	return 0;
}
EXPORT_SYMBOL(rk_decom_wait_done);

static DECLARE_WAIT_QUEUE_HEAD(decom_init_done);

int rk_decom_start(u32 mode, phys_addr_t src, phys_addr_t dst, u32 dst_max_size)
{
	int ret;
	u32 irq_status;
	u32 decom_enr;
	u32 decom_mode = rk_get_decom_mode(mode);

	wait_event_timeout(decom_init_done, g_decom, HZ);
	if (!g_decom)
		return -EINVAL;

	if (g_decom->mem_start)
		pr_info("%s: mode %u src %pa dst %pa max_size %u\n",
			__func__, mode, &src, &dst, dst_max_size);

	ret = clk_bulk_prepare_enable(g_decom->num_clocks, g_decom->clocks);
	if (ret)
		return ret;

	g_decom_complete   = false;
	g_decom_data_len   = 0;
	g_decom_noblocking = rk_get_noblocking_flag(mode);

	decom_enr = readl(g_decom->regs + DECOM_ENR);
	if (decom_enr & 0x1) {
		pr_err("decompress busy\n");
		ret = -EBUSY;
		goto error;
	}

	if (g_decom->reset) {
		reset_control_assert(g_decom->reset);
		udelay(10);
		reset_control_deassert(g_decom->reset);
	}

	irq_status = readl(g_decom->regs + DECOM_ISR);
	/* clear interrupts */
	if (irq_status)
		writel(irq_status, g_decom->regs + DECOM_ISR);

	switch (decom_mode) {
	case LZ4_MOD:
		writel(LZ4_CONT_CSUM_CHECK_EN |
		       LZ4_HEAD_CSUM_CHECK_EN |
		       LZ4_BLOCK_CSUM_CHECK_EN |
		       LZ4_MOD, g_decom->regs + DECOM_CTRL);
		break;
	case GZIP_MOD:
		writel(DECOM_DEFLATE_MODE | DECOM_GZIP_MODE,
		       g_decom->regs + DECOM_CTRL);
		break;
	case ZLIB_MOD:
		writel(DECOM_DEFLATE_MODE | DECOM_ZLIB_MODE,
		       g_decom->regs + DECOM_CTRL);
		break;
	default:
		pr_err("undefined mode : %d\n", decom_mode);
		ret = -EINVAL;
		goto error;
	}

	writel(src, g_decom->regs + DECOM_RADDR);
	writel(dst, g_decom->regs + DECOM_WADDR);

	writel(dst_max_size, g_decom->regs + DECOM_LMTSL);
	writel(0x0, g_decom->regs + DECOM_LMTSH);

	writel(DECOM_INT_MASK, g_decom->regs + DECOM_IEN);
	writel(DECOM_ENABLE, g_decom->regs + DECOM_ENR);

	return 0;
error:
	clk_bulk_disable_unprepare(g_decom->num_clocks, g_decom->clocks);

	return ret;
}
EXPORT_SYMBOL(rk_decom_start);

static irqreturn_t rk_decom_irq_handler(int irq, void *priv)
{
	struct rk_decom *rk_dec = priv;
	u32 irq_status;
	u32 decom_status;

	irq_status = readl(rk_dec->regs + DECOM_ISR);
	/* clear interrupts */
	writel(irq_status, rk_dec->regs + DECOM_ISR);
	if (irq_status & DECOM_STOP) {
		decom_status = readl(rk_dec->regs + DECOM_STAT);
		if (decom_status & DECOM_COMPLETE) {
			g_decom_complete = true;
			g_decom_data_len = readl(rk_dec->regs + DECOM_TSIZEH);
			g_decom_data_len = (g_decom_data_len << 32) |
					   readl(rk_dec->regs + DECOM_TSIZEL);
			wake_up(&g_decom_wait);
			if (rk_dec->mem_start)
				dev_info(rk_dec->dev,
					 "decom completed, decom_data_len = %llu\n",
					 g_decom_data_len);
		} else {
			dev_info(rk_dec->dev,
				 "decom failed, irq_status = 0x%x, decom_status = 0x%x, try again !\n",
				 irq_status, decom_status);

			print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET,
				       32, 4, rk_dec->regs, 0x128, false);

			if (g_decom_noblocking) {
				dev_info(rk_dec->dev, "decom failed and exit in noblocking mode.");
				writel(DECOM_DISABLE, rk_dec->regs + DECOM_ENR);
				writel(0, g_decom->regs + DECOM_IEN);

				g_decom_complete  = true;
				g_decom_data_len = 0;
				g_decom_noblocking = false;
				wake_up(&g_decom_wait);
			} else {
#ifndef CONFIG_ROCKCHIP_HW_DECOMPRESS_TEST
				writel(DECOM_ENABLE, rk_dec->regs + DECOM_ENR);
#endif
			}
		}
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t rk_decom_irq_thread(int irq, void *priv)
{
#ifndef CONFIG_ROCKCHIP_HW_DECOMPRESS_TEST
	struct rk_decom *rk_dec = priv;

	if (g_decom_complete) {
		void *start, *end;

		if (rk_dec->mem_start) {
			/*
			 * Now it is safe to free reserve memory that
			 * store the origin ramdisk file
			 */
			start = phys_to_virt(rk_dec->mem_start);
			end = start + rk_dec->mem_size;
			free_reserved_area(start, end, -1, "ramdisk gzip archive");
			rk_dec->mem_start = 0;
		}

		clk_bulk_disable_unprepare(rk_dec->num_clocks, rk_dec->clocks);
	}

#endif
	return IRQ_HANDLED;
}

#ifdef CONFIG_ROCKCHIP_HW_DECOMPRESS_TEST
static ssize_t start_decom_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	u32 mode = 0;
	phys_addr_t src, dst;
	int ret;
	struct rk_decom *rk_dec = dev_get_drvdata(dev);

	src = rk_dec->mem_start;
	dst = rk_dec->mem_start + rk_dec->mem_size / 2;

	if (src == 0x0 || dst == 0x0)
		return -EINVAL;

	ret = kstrtou32(buf, 10, &mode);
	if (ret)
		return ret;

	if (mode != LZ4_MOD && mode != GZIP_MOD && mode != ZLIB_MOD)
		return -EINVAL;

	dev_info(dev, "%s,%d, src = %pa, dst = %pa, mode = %d\n",
		 __func__, __LINE__, &src, &dst, mode);

	ret = rk_decom_start(mode, src, dst, 0x80000000);
	if (ret)
		pr_info("%s, user decompress error\n", __func__);

	return size;
}

static DEVICE_ATTR_WO(start_decom);

static void *uncomp_virt;
static dma_addr_t uncomp_phys;

static void *comp_virt;
static dma_addr_t comp_phys;

static void *decomp_virt;
static dma_addr_t decomp_phys;

static size_t uncomp_size;
static size_t comp_size;

#define RK_DECOME_TIMEOUT	3 /* 3 seconds */

#define FILE_UNCOMPRESSED "/data/data/asyoulik.txt"
#define FILE_COMPRESSED "/data/data/asyoulik.tar.gz"

static int decompress_and_compare(struct rk_decom *rk_decom, int mode)
{
	struct file *file1 = NULL, *file2 = NULL;
	int ret = -EINVAL;
	u64 decom_len;
	loff_t pos;

	pr_info("Starting decompress and compare operation\n");

	file1 = filp_open(FILE_UNCOMPRESSED, O_RDONLY, 0);
	if (IS_ERR(file1))
		goto err_file1;

	file2 = filp_open(FILE_COMPRESSED, O_RDONLY, 0);
	if (IS_ERR(file2))
		goto err_file2;

	uncomp_size = file1->f_inode->i_size;
	comp_size = file2->f_inode->i_size;

	pr_info("Uncompressed file size: %zu bytes\n", uncomp_size);

	uncomp_virt = dma_alloc_coherent(rk_decom->dev, uncomp_size,
					 &uncomp_phys, GFP_KERNEL);
	if (!uncomp_virt)
		goto out_free;

	comp_virt = dma_alloc_coherent(rk_decom->dev, comp_size, &comp_phys,
				       GFP_KERNEL);
	if (!comp_virt)
		goto out_free1;

	decomp_virt = dma_alloc_coherent(rk_decom->dev, uncomp_size * 2,
					 &decomp_phys, GFP_KERNEL);
	if (!decomp_virt)
		goto out_free2;

	pos = 0;
	if (kernel_read(file1, uncomp_virt, uncomp_size, &pos) < 0) {
		pr_err("Failed to read uncompressed file\n");
		goto out_free3;
	}
	filp_close(file1, NULL);
	file1 = NULL;

	pos = 0;
	if (kernel_read(file2, comp_virt, comp_size, &pos) < 0) {
		pr_err("Failed to read compressed file\n");
		goto out_free3;
	}
	filp_close(file2, NULL);
	file2 = NULL;

	/* decompress of RV1126B integrate with new designed iommu which need to
	 * disable bypass iommu by setting the following grf register:
	 * io -4 0x20180014 0xffff0000
	 */

	ret = rk_decom_start(mode, comp_phys, decomp_phys, 0x80000000);
	if (ret) {
		pr_err("rk_decom_start failed[%d].", ret);
		goto out_free3;
	}

	ret = rk_decom_wait_done(RK_DECOME_TIMEOUT, &decom_len);
	pr_info("Decompression %lld completed\n", decom_len);

	pr_info("Comparing files...\n");
	if (memcmp(uncomp_virt, decomp_virt, uncomp_size) == 0) {
		pr_info("Files match exactly!\n");
		ret = 0;
	} else {
		pr_info("Files differ\n");
		ret = -EINVAL;
	}

out_free3:
	dma_free_coherent(rk_decom->dev, uncomp_size * 2, decomp_virt,
			  decomp_phys);
out_free2:
	dma_free_coherent(rk_decom->dev, comp_size, comp_virt, comp_phys);
out_free1:
	dma_free_coherent(rk_decom->dev, uncomp_size, uncomp_virt, uncomp_phys);
out_free:
	if (file2)
		filp_close(file2, NULL);
err_file2:
	if (file1)
		filp_close(file1, NULL);
err_file1:
	return ret;
}

static ssize_t dynamic_buf_decom_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	u32 mode;
	int ret;
	struct rk_decom *rk_dec = dev_get_drvdata(dev);

	ret = kstrtou32(buf, 10, &mode);
	if (ret)
		return ret;

	if (mode != LZ4_MOD && mode != GZIP_MOD && mode != ZLIB_MOD)
		return -EINVAL;

	ret = decompress_and_compare(rk_dec, mode);
	if (ret)
		pr_info("%s, user decompress error\n", __func__);

	return size;
}
static DEVICE_ATTR_WO(dynamic_buf_decom);

static struct attribute *decom_attrs[] = {
	&dev_attr_start_decom.attr,
	&dev_attr_dynamic_buf_decom.attr,
	NULL
};

static const struct attribute_group decom_attr_group = {
	.attrs = decom_attrs,
};
#endif

static int __init rockchip_decom_probe(struct platform_device *pdev)
{
	struct rk_decom *rk_dec;
	struct resource *res = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *mem;
	struct resource reg;
	int ret = 0;

	rk_dec = devm_kzalloc(dev, sizeof(*rk_dec), GFP_KERNEL);
	if (!rk_dec)
		return -ENOMEM;

	rk_dec->dev = dev;
	rk_dec->irq = platform_get_irq(pdev, 0);
	if (rk_dec->irq < 0) {
		dev_err(dev, "failed to get rk_dec irq\n");
		return -ENOENT;
	}

	mem = of_parse_phandle(np, "memory-region", 0);
	if (!mem) {
		dev_err(dev, "missing \"memory-region\" property\n");
#ifndef CONFIG_ROCKCHIP_HW_DECOMPRESS_TEST
		return -ENODEV;
#endif
	}

	ret = of_address_to_resource(mem, 0, &reg);
	of_node_put(mem);
	if (ret) {
		dev_err(dev, "missing \"reg\" property\n");
#ifndef CONFIG_ROCKCHIP_HW_DECOMPRESS_TEST
		return -ENODEV;
#endif
	}

	rk_dec->mem_start = reg.start;
	rk_dec->mem_size = resource_size(&reg);

	rk_dec->num_clocks = devm_clk_bulk_get_all(dev, &rk_dec->clocks);
	if (rk_dec->num_clocks < 0) {
		dev_err(dev, "failed to get decompress clock\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rk_dec->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(rk_dec->regs)) {
		ret = PTR_ERR(rk_dec->regs);
		goto disable_clk;
	}

	dev_set_drvdata(dev, rk_dec);

	rk_dec->reset = devm_reset_control_get_exclusive(dev, "dresetn");
	if (IS_ERR(rk_dec->reset)) {
		ret = PTR_ERR(rk_dec->reset);
		if (ret != -ENOENT)
			return ret;

		dev_dbg(dev, "no reset control found\n");
		rk_dec->reset = NULL;
	}

	ret = devm_request_threaded_irq(dev, rk_dec->irq, rk_decom_irq_handler,
					rk_decom_irq_thread, IRQF_ONESHOT,
					dev_name(dev), rk_dec);
	if (ret < 0) {
		dev_err(dev, "failed to attach decompress irq\n");
		goto disable_clk;
	}

#ifdef CONFIG_ROCKCHIP_HW_DECOMPRESS_TEST
	ret = sysfs_create_group(&pdev->dev.kobj, &decom_attr_group);
	if (ret) {
		dev_err(dev, "SysFS group creation failed\n");
		return ret;
	}
#endif
	g_decom = rk_dec;
	wake_up(&decom_init_done);

#ifndef CONFIG_ROCKCHIP_THUNDER_BOOT
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
#endif
	return 0;

disable_clk:
	clk_bulk_disable_unprepare(rk_dec->num_clocks, rk_dec->clocks);

	return ret;
}

#ifndef CONFIG_ROCKCHIP_THUNDER_BOOT
static void rockchip_decom_shutdown(struct platform_device *pdev)
{
	struct rk_decom *rk_dec = platform_get_drvdata(pdev);

#ifdef CONFIG_ROCKCHIP_HW_DECOMPRESS_TEST
	sysfs_remove_group(&pdev->dev.kobj, &decom_attr_group);
#endif
	devm_free_irq(rk_dec->dev, rk_dec->irq, rk_dec);
	pm_runtime_put_sync(rk_dec->dev);
	pm_runtime_disable(rk_dec->dev);
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id rockchip_decom_dt_match[] = {
	{ .compatible = "rockchip,hw-decompress" },
	{},
};
#endif

static struct platform_driver rk_decom_driver = {
#ifndef CONFIG_ROCKCHIP_THUNDER_BOOT
	.probe		= rockchip_decom_probe,
	.shutdown	= rockchip_decom_shutdown,
#endif
	.driver		= {
		.name	= "rockchip_hw_decompress",
		.of_match_table = rockchip_decom_dt_match,
	},
};

static int __init rockchip_hw_decompress_init(void)
{
	struct device_node *node;

#ifndef CONFIG_ROCKCHIP_THUNDER_BOOT
	return platform_driver_register(&rk_decom_driver);
#endif
	node = of_find_matching_node(NULL, rockchip_decom_dt_match);
	if (node) {
		of_platform_device_create(node, NULL, NULL);
		of_node_put(node);
		return platform_driver_probe(&rk_decom_driver, rockchip_decom_probe);
	}

	return 0;
}

pure_initcall(rockchip_hw_decompress_init);
