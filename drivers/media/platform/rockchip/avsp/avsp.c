// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#include <linux/version.h>

#include "avsp.h"
#include "regs.h"

int rkavsp_log_level;
module_param_named(debug, rkavsp_log_level, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-2)");

#if IS_LINUX_VERSION_AT_LEAST_6_1
	#define GET_SG_TABLE(mem_ops, off_buf) mem_ops->cookie(&(off_buf)->vb, (off_buf)->mem)
#else
	#define GET_SG_TABLE(mem_ops, off_buf) mem_ops->cookie((off_buf)->mem)
#endif

static void rkavsp_soft_reset(struct rkavsp_dev *hw);

#if IS_LINUX_VERSION_AT_LEAST_6_1
static void init_vb2(struct rkavsp_dev *avsp, struct rkavsp_buf *buf)
{
	unsigned long attrs = DMA_ATTR_NO_KERNEL_MAPPING;

	if (!buf)
		return;
	memset(&buf->vb, 0, sizeof(buf->vb));
	memset(&buf->vb2_queue, 0, sizeof(buf->vb2_queue));
	buf->vb2_queue.gfp_flags = GFP_KERNEL | GFP_DMA32;
	buf->vb2_queue.dma_dir = DMA_BIDIRECTIONAL;
	if (avsp->is_dma_config)
		attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
	buf->vb2_queue.dma_attrs = attrs;
	buf->vb.vb2_queue = &buf->vb2_queue;
}
#endif

static struct rkavsp_buf *avsp_buf_add(struct file *file, int fd)
{
	struct rkavsp_dev *avsp = container_of(file->private_data, struct rkavsp_dev, mdev);
	struct rkavsp_buf *buf = NULL, *next = NULL;
	const struct vb2_mem_ops *ops = avsp->mem_ops;
	struct dma_buf *dbuf = NULL;
	void *mem = NULL;
	bool need_add = true;

	dbuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dbuf)) {
		RKAVSP_ERR("dma buf get err.\n");
		return buf;
	}

	mutex_lock(&avsp->dev_lock);
	list_for_each_entry_safe(buf, next, &avsp->list, list) {
		if (buf->file == file && buf->fd == fd && buf->dbuf == dbuf) {
			need_add = false;
			break;
		}
	}

	if (need_add) {
		buf = kzalloc(sizeof(struct rkavsp_buf), GFP_KERNEL);
		if (!buf)
			goto err;
#if IS_LINUX_VERSION_AT_LEAST_6_1
		init_vb2(avsp, buf);

		mem = ops->attach_dmabuf(&buf->vb, avsp->dev, dbuf, dbuf->size);
#else
		mem = ops->attach_dmabuf(avsp->dev, dbuf, dbuf->size,
					 DMA_BIDIRECTIONAL);
#endif
		if (IS_ERR(mem)) {
			RKAVSP_ERR("failed to attach dmabuf.\n");
			goto err;
		}

		if (ops->map_dmabuf(mem)) {
			RKAVSP_ERR("failed to map.\n");
			ops->detach_dmabuf(mem);
			mem = NULL;
			goto err;
		}

		buf->fd = fd;
		buf->file = file;
		buf->dbuf = dbuf;
		buf->mem = mem;
		/* internal_alloc already add */
		buf->alloc = false;
		list_add_tail(&buf->list, &avsp->list);
		RKAVSP_DBG("file:%p fd:%d dbuf:%p\n", file, fd, dbuf);
	} else {
		dma_buf_put(dbuf);
	}
	mutex_unlock(&avsp->dev_lock);
	return buf;
err:
	dma_buf_put(dbuf);
	kfree(buf);
	buf = NULL;
	mutex_unlock(&avsp->dev_lock);
	return buf;
}

static void avsp_buf_del(struct file *file, int fd, bool is_all)
{
	struct rkavsp_dev *avsp = container_of(file->private_data, struct rkavsp_dev, mdev);
	struct rkavsp_buf *buf, *next;
	const struct vb2_mem_ops *ops = avsp->mem_ops;

	mutex_lock(&avsp->dev_lock);
	list_for_each_entry_safe(buf, next, &avsp->list, list) {
		if (buf->file == file && (is_all || buf->fd == fd)) {
			RKAVSP_DBG("file:%p fd:%d dbuf:%p\n", file, buf->fd, buf->dbuf);
			if (!buf->alloc) {
				ops->unmap_dmabuf(buf->mem);
				ops->detach_dmabuf(buf->mem);
			} else {
				ops->put(buf->mem);
			}
			dma_buf_put(buf->dbuf);
			buf->file = NULL;
			buf->mem = NULL;
			buf->dbuf = NULL;
			buf->fd = -1;
			list_del(&buf->list);
			kfree(buf);
			if (!is_all)
				break;
		}
	}
	mutex_unlock(&avsp->dev_lock);
}

static int avsp_dcp_run(struct file *file, struct rkavsp_dcp_in_out *buf)
{
	struct rkavsp_dev *avsp = container_of(file->private_data, struct rkavsp_dev, mdev);
	void __iomem *base = avsp->base;
	const struct vb2_mem_ops *mem_ops = avsp->mem_ops;
	struct sg_table *sgt;
	struct rkavsp_buf *off_buf;
	int ret = -EINVAL;
	int pry_h[6], i;
	u32 in_offs, out_offs, val;
	u32 in_w = buf->in_width, in_h = buf->in_height;
	u32 dcp_bypass = AVSP_BYPASS_OFF;
	u32 bandnum = buf->bandnum;
	u32 wr_mode = buf->dcp_wr_mode, rd_mode = buf->dcp_rd_mode;
	u32 dcp_rd_stride_y = buf->dcp_rd_stride_y, dcp_rd_stride_c = buf->dcp_rd_stride_c;

	mutex_lock(&avsp->dcp_lock);
	if (rd_mode != AVSP_MODE_QUAD && rd_mode != AVSP_MODE_RASTER) {
		RKAVSP_ERR("dcp rd_mode err.\n");
		mutex_unlock(&avsp->dcp_lock);
		return -EINVAL;
	}

	switch (buf->dcp_wr_mode) {
	case AVSP_MODE_RASTER:
		dcp_bypass = AVSP_BYPASS_OPEN;
		break;
	case AVSP_MODE_QUAD:
		break;
	default:
		RKAVSP_ERR("no support dcp_wr_mode.\n");
		mutex_unlock(&avsp->dcp_lock);
		return -EINVAL;
	}

	// DCP CTL SET
	val = SW_DCP_BYPASS(dcp_bypass) | SW_DCP_WR_MODE(wr_mode) |
	      SW_DCP_RD_MODE(rd_mode) | SW_DCP_BAND_NUM(bandnum);
	writel(val, base + AVSP_DCP_CTRL);
	val = SW_AVSP_SRC_WIDTH(in_w) | Sw_AVSP_SRC_HEIGHT(in_h);
	writel(val, base + AVSP_DCP_SIZE);
	val = AVSP_RD_VIR_STRIDE_Y(dcp_rd_stride_y) | AVSP_RD_VIR_STRIDE_C(dcp_rd_stride_c);
	writel(val, base + AVSP_DCP_RD_VIR_SIZE);

	// wr stride set
	for (i = 0; i < bandnum; i++) {
		val = AVSP_WR_VIR_STRIDE_Y(buf->dcp_wr_stride_y[i]) |
		AVSP_WR_VIR_STRIDE_C(buf->dcp_wr_stride_c[i]);
		writel(val, base + AVSP_DCP_WR_LV0_VIR_SIZE + i * 4);
	}

	/* input picture buf */
	in_offs = dcp_rd_stride_y * in_h * 4;
	off_buf = avsp_buf_add(file, buf->in_pic_fd);
	if (!off_buf) {
		mutex_unlock(&avsp->dcp_lock);
		return -ENOMEM;
	}

	sgt = GET_SG_TABLE(mem_ops, off_buf);
	if (!sgt)
		goto free_buf;
	val = sg_dma_address(sgt->sgl);
	writel(val, base + AVSP_DCP_RD_Y_BASE);
	if (rd_mode == AVSP_MODE_RASTER) {
		val += in_offs;
		writel(val, base + AVSP_DCP_RD_C_BASE);
	}

	/* output pyramid buf */
	for (i = 0; i < bandnum; i++) {
		pry_h[i] = in_h / (1 << i);
		out_offs = (buf->dcp_wr_stride_y[i]) * pry_h[i] * 4;
		off_buf = avsp_buf_add(file, buf->out_pry_fd[i]);
		if (!off_buf) {
			ret = -ENOMEM;
			goto free_buf;
		}

		sgt = GET_SG_TABLE(mem_ops, off_buf);
		if (!sgt)
			goto free_buf;
		val = sg_dma_address(sgt->sgl);
		writel(val, base + AVSP_DCP_LV0_BASE_Y + i * 4);

		if (wr_mode == AVSP_MODE_RASTER) {
			val += out_offs;
			writel(val, base + AVSP_DCP_LV0_BASE_C + i * 4);
		}
	}

	writel(AVSP_FORCE_UPD, base + AVSP_DCP_UPDATE);
	writel(AVSP_ST, base + AVSP_DCP_STRT);
	RKAVSP_DBG("DCP: write start success.\n");

	ret = wait_for_completion_timeout(&avsp->dcp_cmpl, msecs_to_jiffies(300));
	if (!ret) {
		RKAVSP_ERR("IOCTL AVSP_DCP work out time.\n");
		ret = -EAGAIN;
		rkavsp_soft_reset(avsp);
		goto free_buf;
	} else {
		ret = 0;
	}
	mutex_unlock(&avsp->dcp_lock);
	return ret;

free_buf:
	RKAVSP_DBG("avsp_dcp free buf.\n");
	avsp_buf_del(file, 0, true);
	mutex_unlock(&avsp->dcp_lock);
	return ret;
}

static int avsp_rcs_run(struct file *file, struct rkavsp_rcs_in_out *buf)
{
	struct rkavsp_dev *avsp = container_of(file->private_data, struct rkavsp_dev, mdev);
	void __iomem *base = avsp->base;
	const struct vb2_mem_ops *mem_ops = avsp->mem_ops;
	struct rkavsp_buf *off_buf;
	struct sg_table *sgt;
	int ret = -EINVAL;
	int i;

	u32 rd_mode = AVSP_MODE_QUAD;
	u32 out_offs, val, c_addr;
	u32 in_w = buf->in_width, in_h = buf->in_height;
	u32 bandnum = buf->bandnum;
	u32 wr_mode = buf->rcs_wr_mode;
	u32 rcs_wr_stride_y = buf->rcs_wr_stride_y, rcs_wr_stride_c = buf->rcs_wr_stride_c;
	u32 rcs_out_start_offset = buf->rcs_out_start_offset;

	mutex_lock(&avsp->rcs_lock);
	val = SW_RCS_BAND_NUM(bandnum) | SW_RCS_RD_MODE(rd_mode) | SW_RCS_WR_MODE(wr_mode);
	if (wr_mode == AVSP_MODE_FBCE)
		val |= SW_RCS_FBCE_CTL;

	writel(val, base + AVSP_RCS_CTRL);
	val = SW_AVSP_SRC_WIDTH(in_w) | Sw_AVSP_SRC_HEIGHT(in_h);
	writel(val, base + AVSP_RCS_SIZE);
	val = AVSP_WR_VIR_STRIDE_Y(rcs_wr_stride_y) | AVSP_WR_VIR_STRIDE_C(rcs_wr_stride_c);
	writel(val, base + AVSP_RCS_WR_STRIDE);

	// pry input0 buf add
	for (i = 0; i < bandnum; i++) {
		off_buf = avsp_buf_add(file, buf->in_pry0_fd[i]);
		if (!off_buf) {
			ret = -ENOMEM;
			goto free_buf;
		}

		sgt = GET_SG_TABLE(mem_ops, off_buf);
		if (!sgt)
			goto free_buf;
		val = sg_dma_address(sgt->sgl);
		writel(val, base + AVSP_RCS_C0LV0_BASE + i * 4);
	}

	// pry input1 buf add
	for (i = 0; i < bandnum; i++) {
		off_buf = avsp_buf_add(file, buf->in_pry1_fd[i]);
		if (!off_buf) {
			ret = -ENOMEM;
			goto free_buf;
		}

		sgt = GET_SG_TABLE(mem_ops, off_buf);
		if (!sgt)
			goto free_buf;
		val = sg_dma_address(sgt->sgl);
		writel(val, base + AVSP_RCS_C1LV0_BASE + i * 4);
	}

	// RCS DT_LVX
	for (i = 0; i < bandnum; i++) {
		off_buf = avsp_buf_add(file, buf->dt_pry_fd[i]);
		if (!off_buf) {
			ret = -ENOMEM;
			goto free_buf;
		}

		sgt = GET_SG_TABLE(mem_ops, off_buf);
		if (!sgt)
			goto free_buf;
		val = sg_dma_address(sgt->sgl);
		writel(val, base + AVSP_RCS_DTLV0_BASE + i * 4);
	}

	off_buf = avsp_buf_add(file, buf->out_pic_fd);
	if (!off_buf) {
		ret = -ENOMEM;
		goto free_buf;
	}

	sgt = GET_SG_TABLE(mem_ops, off_buf);
	if (!sgt)
		goto free_buf;
	val = sg_dma_address(sgt->sgl);

	switch (wr_mode) {
	case AVSP_MODE_RASTER:
		out_offs = rcs_wr_stride_y * in_h * 4;
		val += rcs_out_start_offset;
		writel(val, base + AVSP_RCS_WR_Y_BASE);
		val += out_offs;
		writel(val, base + AVSP_RCS_WR_C_BASE);
		break;
	case AVSP_MODE_FBCE:
		c_addr = val + (rcs_out_start_offset / 64) * 16;
		writel(c_addr, base + AVSP_RCS_WR_C_BASE);
		out_offs = rcs_wr_stride_c * in_h + ((rcs_out_start_offset / 64) * 384);
		val += out_offs;
		writel(val, base + AVSP_RCS_WR_Y_BASE);

		val = out_offs;
		writel(val << 4, base + AVSP_RCS_WR_FBCE_HEAD_OFFSET);
		break;
	default:
		val += (rcs_out_start_offset * 6);
		writel(val, base + AVSP_RCS_WR_Y_BASE);
		break;
	}

	writel(AVSP_FORCE_UPD, base + AVSP_RCS_UPDATE);
	writel(AVSP_ST, base + AVSP_RCS_STRT);
	ret = wait_for_completion_timeout(&avsp->rcs_cmpl, msecs_to_jiffies(300));
	if (!ret) {
		RKAVSP_ERR("IOCTL AVSP_RCS work out time.\n");
		ret = -EAGAIN;
		rkavsp_soft_reset(avsp);
		goto free_buf;
	} else {
		ret = 0;
	}
	mutex_unlock(&avsp->rcs_lock);
	return ret;

free_buf:
	RKAVSP_DBG("avsp_rcs free buf.\n");
	avsp_buf_del(file, 0, true);
	mutex_unlock(&avsp->rcs_lock);
	return ret;
}

static int avsp_open(struct inode *inode, struct file *file)
{
	int ret;
	struct rkavsp_dev *avsp = container_of(file->private_data, struct rkavsp_dev, mdev);

	mutex_lock(&avsp->dev_lock);
	ret = pm_runtime_get_sync(avsp->dev);
	mutex_unlock(&avsp->dev_lock);

	RKAVSP_INFO("avsp: device opened.\n");
	return (ret > 0) ? 0 : ret;
}

static int avsp_release(struct inode *inode, struct file *file)
{
	struct rkavsp_dev *avsp = container_of(file->private_data, struct rkavsp_dev, mdev);

	avsp_buf_del(file, 0, true);
	mutex_lock(&avsp->dev_lock);
	pm_runtime_put_sync(avsp->dev);
	mutex_unlock(&avsp->dev_lock);
	RKAVSP_INFO("avsp: device released.\n");
	return 0;
}

static long avsp_ioctl_default(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct rkavsp_dcp_in_out dcp_data;
	struct rkavsp_rcs_in_out rcs_data;

	if (!arg) {
		ret = -EINVAL;
		goto out;
	}

	switch (cmd) {
	case RKAVSP_CMD_DCP:
		if (copy_from_user(&dcp_data, (struct rkavsp_dcp_in_out __user *)arg,
				   sizeof(struct rkavsp_dcp_in_out))) {
			ret = -EFAULT;
			goto out;
		}
		ret = avsp_dcp_run(file, &dcp_data);
		break;
	case RKAVSP_CMD_RCS:
		if (copy_from_user(&rcs_data, (struct rkavsp_rcs_in_out __user *)arg,
				   sizeof(struct rkavsp_rcs_in_out))) {
			ret = -EFAULT;
			goto out;
		}
		ret = avsp_rcs_run(file, &rcs_data);
		break;
	default:
		ret = -EFAULT;
	}
out:
	return ret;
}

static const struct file_operations avsp_fops = {
	.owner = THIS_MODULE,
	.open = avsp_open,
	.release = avsp_release,
	.unlocked_ioctl = avsp_ioctl_default,
};

static irqreturn_t avsp_dcp_irq_hdl(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct rkavsp_dev *avsp = dev_get_drvdata(dev);
	void __iomem *base = avsp->base;
	unsigned int mis_val;

	mis_val = readl(base + AVSP_DCP_INT_MSK);
	writel(mis_val, base + AVSP_DCP_INT_CLR);

	if (mis_val & DCP_INT) {
		mis_val &= (~DCP_INT);
		if (!completion_done(&avsp->dcp_cmpl)) {
			complete(&avsp->dcp_cmpl);
			RKAVSP_DBG("misval: 0x%x\n", mis_val);
		}
	}
	return IRQ_HANDLED;
}

static irqreturn_t avsp_rcs_irq_hdl(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct rkavsp_dev *avsp = dev_get_drvdata(dev);
	void __iomem *base = avsp->base;
	unsigned int mis_val;

	mis_val = readl(base + AVSP_RCS_INT_MSK1);
	writel(mis_val, base + AVSP_RCS_INT_CLR0);
	writel(mis_val, base + AVSP_RCS_INT_CLR1);

	if (mis_val & RCS_INT) {
		mis_val &= (~RCS_INT);
		if (!completion_done(&avsp->rcs_cmpl)) {
			complete(&avsp->rcs_cmpl);
			RKAVSP_DBG("misval: 0x%x\n", mis_val);
		}
	}
	return IRQ_HANDLED;
}

static const char * const rv1126b_avsp_clks[] = {
	"aclk_avsp",
	"hclk_avsp",
};

static void rkavsp_set_clk_rate(struct clk *clk, unsigned long rate)
{
	clk_set_rate(clk, rate);
}

static void disable_sys_clk(struct rkavsp_dev *dev)
{
	int i;

	for (i = 0; i < dev->clks_num; i++)
		clk_disable_unprepare(dev->clks[i]);
}

static int enable_sys_clk(struct rkavsp_dev *dev)
{
	int i, ret;

	for (i = 0; i < dev->clks_num; i++) {
		ret = clk_prepare_enable(dev->clks[i]);
		if (ret < 0)
			goto err;
	}

	//tosee
	rkavsp_set_clk_rate(dev->clks[0],
			   dev->clk_rate_tbl[dev->clk_rate_tbl_num - 1].clk_rate * 1000000);

	return 0;

err:
	for (--i; i >= 0; --i)
		clk_disable_unprepare(dev->clks[i]);
	return ret;
}

static int avsp_probe(struct platform_device *pdev)
{
	const struct avsp_match_data *match_data;
	struct rkavsp_dev *avsp;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int i, irq, ret = 0;
	//bool is_mmu;

	match_data = device_get_match_data(&pdev->dev);
	if (!match_data)
		return -ENODEV;

	avsp = devm_kzalloc(&pdev->dev, sizeof(*avsp), GFP_KERNEL);
	if (!avsp)
		return -ENOMEM;

	dev_set_drvdata(dev, avsp);
	avsp->dev = &pdev->dev;
	avsp->match_data = match_data;

	/* map the registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		RKAVSP_ERR("get memory resource failed.\n");
		ret = -EINVAL;
		goto err;
	}

	avsp->base = devm_ioremap_resource(avsp->dev, res);
	if (IS_ERR(avsp->base)) {
		RKAVSP_ERR("ioremap failed\n");
		ret = PTR_ERR(avsp->base);
		goto err;
	}

	INIT_LIST_HEAD(&avsp->list);
	init_completion(&avsp->dcp_cmpl);
	init_completion(&avsp->rcs_cmpl);
	avsp->mem_ops = &vb2_cma_sg_memops;

	/* get the irq */
	for (i = 0; i < match_data->num_irqs; i++) {
		irq = platform_get_irq_byname(pdev, match_data->irqs[i].name);
		if (irq < 0) {
			RKAVSP_ERR("no irq %s in dts.\n", match_data->irqs[i].name);
			ret = irq;
			goto err;
		}
		ret = devm_request_irq(dev, irq, match_data->irqs[i].irq_hdl,
				       IRQF_SHARED, dev_driver_string(dev), dev);
		if (ret < 0) {
			RKAVSP_ERR("request %s failed: %d\n", match_data->irqs[i].name, ret);
			goto err;
		}
	}
	/* get the clk */
	for (i = 0; i < match_data->clks_num; i++) {
		struct clk *clk = devm_clk_get(dev, match_data->clks[i]);

		if (IS_ERR(clk)) {
			RKAVSP_ERR("failed to get %s\n", match_data->clks[i]);
			ret = PTR_ERR(clk);
			goto err;
		}
		avsp->clks[i] = clk;
	}
	avsp->clks_num = match_data->clks_num;
	avsp->clk_rate_tbl = match_data->clk_rate_tbl;
	avsp->clk_rate_tbl_num = match_data->clk_rate_tbl_num;

	avsp->reset = devm_reset_control_array_get(dev, false, false);
	if (IS_ERR(avsp->reset)) {
		RKAVSP_INFO("failed to get cru reset\n");
		avsp->reset = NULL;
	}

	mutex_init(&avsp->dev_lock);
	mutex_init(&avsp->dcp_lock);
	mutex_init(&avsp->rcs_lock);
	avsp->is_dma_config = true;

	// register misc device
	avsp->mdev.minor = MISC_DYNAMIC_MINOR;
	avsp->mdev.name = AVSP_NAME;
	avsp->mdev.fops = &avsp_fops;

	ret = misc_register(&avsp->mdev);
	if (ret < 0) {
		RKAVSP_ERR("avsp misc register failed.\n");
		goto err;
	}

	pm_runtime_enable(&pdev->dev);
	RKAVSP_INFO("avsp misc device probe success.\n");
	return 0;
err:
	return ret;
}

static int avsp_remove(struct platform_device *pdev)
{
	/* misc device remove */
	struct rkavsp_dev *avsp = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	misc_deregister(&avsp->mdev);
	mutex_destroy(&avsp->rcs_lock);
	mutex_destroy(&avsp->dcp_lock);
	mutex_destroy(&avsp->dev_lock);

	return 0;
}

static const struct avsp_clk_info rv1126b_avsp_clk_rate[] = {
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
		.clk_rate = 600,
		.refer_data = 3840,
	}, {
		.clk_rate = 702,
		.refer_data = 4672,
	}
};

static struct irqs_data rv1126b_avsp_irqs[] = {
	{"dcp_irq", avsp_dcp_irq_hdl},
	{"rcs_irq", avsp_rcs_irq_hdl},
};

static const struct avsp_match_data rv1126b_avsp_match_data = {
	.clks = rv1126b_avsp_clks,
	.clks_num = ARRAY_SIZE(rv1126b_avsp_clks),
	.clk_rate_tbl = rv1126b_avsp_clk_rate,
	.clk_rate_tbl_num = ARRAY_SIZE(rv1126b_avsp_clk_rate),
	.irqs = rv1126b_avsp_irqs,
	.num_irqs = ARRAY_SIZE(rv1126b_avsp_irqs),
};

static const struct of_device_id rockchip_avsp_match[] = {
	{
		.compatible = "rockchip,rv1126b-rkavsp",
		.data = &rv1126b_avsp_match_data,
	},
	{},
};

MODULE_DEVICE_TABLE(of, rockchip_avsp_match);

static void rkavsp_soft_reset(struct rkavsp_dev *hw)
{
	u32 val;

	/* reset */
	val = SYS_SOFT_RST_DCP;
	writel(val, hw->base + AVSP_DCP_CLK_DIS);
	udelay(10);
	writel(SYS_SOFT_RST_VAL, hw->base + AVSP_DCP_CLK_DIS);

	if (hw->reset) {
		reset_control_assert(hw->reset);
		udelay(20);
		reset_control_deassert(hw->reset);
		udelay(20);
	}

	/* refresh iommu after reset */
	rockchip_iommu_disable(hw->dev);
	rockchip_iommu_enable(hw->dev);

	/* clk_dis */
	val = SYS_DCP_LGC_CKG_DIS | SYS_DCP_RAM_CKG_DIS;
	writel(val, hw->base + AVSP_DCP_CLK_DIS);

	/* int en */
	val = DCP_INT;
	writel(val, hw->base + AVSP_DCP_INT_EN);
	val = RCS_INT;
	writel(val, hw->base + AVSP_RCS_INT_EN1);
}

static int __maybe_unused rkavsp_runtime_suspend(struct device *dev)
{
	struct rkavsp_dev *avsp = dev_get_drvdata(dev);

	if (dev->power.runtime_status) {
		writel(0, avsp->base + AVSP_DCP_INT_EN);
		writel(0, avsp->base + AVSP_RCS_INT_EN1);
	}

	disable_sys_clk(avsp);
	return 0;
}

static int __maybe_unused rkavsp_runtime_resume(struct device *dev)
{
	struct rkavsp_dev *avsp = dev_get_drvdata(dev);

	enable_sys_clk(avsp);
	rkavsp_soft_reset(avsp);

	//if (dev->power.runtime_status)
	return 0;
}

static const struct dev_pm_ops rkavsp_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				     pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(rkavsp_runtime_suspend,
			   rkavsp_runtime_resume, NULL)
};

static void rkavsp_shutdown(struct platform_device *pdev)
{
	struct rkavsp_dev *avsp = platform_get_drvdata(pdev);
	u32 val;

	//hw_dev->is_shutdown = true;
	if (pm_runtime_active(&pdev->dev)) {
		writel(0, avsp->base + AVSP_DCP_INT_EN);
		writel(0, avsp->base + AVSP_RCS_INT_EN1);

		val = SYS_SOFT_RST_DCP;
		writel(val, avsp->base + AVSP_DCP_CLK_DIS);
		udelay(10);
		writel(SYS_SOFT_RST_VAL, avsp->base + AVSP_DCP_CLK_DIS);
	}
	RKAVSP_INFO("shutdown.\n");
}

static struct platform_driver avsp_pdrv = {
	.probe = avsp_probe,
	.remove = avsp_remove,
	.shutdown = rkavsp_shutdown,
	.driver = {
		.name = AVSP_NAME,
		.pm = &rkavsp_pm_ops,
		.of_match_table = of_match_ptr(rockchip_avsp_match),
	},
};

module_platform_driver(avsp_pdrv);

MODULE_AUTHOR("Zhizhen Zheng <zhizhen.zheng@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip AVSP Module");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
