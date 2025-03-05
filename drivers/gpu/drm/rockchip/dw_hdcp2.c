// SPDX-License-Identifier: GPL-2.0
/*
 * Synopsys DesignWare Cores HDCP Controller
 *
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 *
 * Author: Zhang Yubing <yubing.zhang@rock-chips.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/rockchip/rockchip_sip.h>
#include <uapi/misc/dw_hdcp2.h>

#define HDCP_MAX_PORT			6

/**
 * struct hl_device - hdcp host library device structure
 * each hdcp controller attach to a hl_device, it include
 * code memory info, data memory info and hpi(apb) interface
 * info
 */
struct hl_device {
	/** @allocated: is the memory allocated */
	bool allocated;
	/** @initialized: is the device initialized */
	bool initialized;
	/** @code_loaded: is the code loaded */
	bool code_loaded;

	/** @code_is_phys_mem: is physical address used for code */
	bool code_is_phys_mem;
	/** @code_base: dma address for code */
	dma_addr_t code_base;
	/** @code_size: code length */
	uint32_t code_size;
	/** @code: virtual address for code */
	uint8_t *code;
	/** @data_is_phys_mem: is physical address used for data */
	bool data_is_phys_mem;
	/** @data_base: dma address for data */
	dma_addr_t data_base;
	/** @data_size: data length */
	uint32_t data_size;
	/** @data: virtual address for data */
	uint8_t *data;

	/** @hpi_resource: resource of HPI interface */
	struct resource *hpi_resource;
	/** @hpi: base address of HPI registers */
	uint8_t __iomem *hpi;
};

struct dw_hdcp_grf_reg {
	uint32_t offset;
	uint32_t shift;
};

struct dw_hdcp_port_cfg {
	struct dw_hdcp_grf_reg connect_reg;
	int port_id;
};

struct dw_hdcp_cfg {
	int port_num;
	struct dw_hdcp_grf_reg boot_reg;
	struct dw_hdcp_port_cfg port_cfg[HDCP_MAX_PORT];
};

struct dw_hdcp {
	struct device *dev;
	struct miscdevice misc_dev;
	struct hl_device hl_dev;

	struct regmap *vo_grf;
	struct reset_control *rsts_bulk;
	struct clk_bulk_data *clks;
	const struct dw_hdcp_cfg *cfgs;
	int num_clks;
	int id;
	bool is_suspend;
};

enum {
	HDCP_PORT0 = 0,
	HDCP_PORT1,
	HDCP_PORT2,
};

static void dw_hdcp_free_hl_dev_slot(struct hl_device *hl_dev);

static void dw_hdcp_free_hl(struct dw_hdcp *hdcp)
{
	dw_hdcp_free_hl_dev_slot(&hdcp->hl_dev);
	hdcp->hl_dev.code_loaded = false;
}

static void dw_hdcp_reset(struct dw_hdcp *hdcp)
{
	int ret;

	reset_control_assert(hdcp->rsts_bulk);
	udelay(20);
	reset_control_deassert(hdcp->rsts_bulk);

	ret = sip_hdcpkey_init(hdcp->id);
	if (ret)
		dev_err(hdcp->dev, "load hdcp key failed\n");
}

static int dw_hdcp_set_reset(struct dw_hdcp *hdcp, void __user *arg)
{
	u32 reset;

	if (!arg)
		return -EFAULT;

	if (copy_from_user(&reset, arg, sizeof(reset)))
		return -EFAULT;

	if (reset) {
		dev_info(hdcp->dev, "hdcp reset\n");
		dw_hdcp_free_hl(hdcp);
		dw_hdcp_reset(hdcp);
	}

	return 0;
}

static int dw_hdcp_get_status(struct dw_hdcp *hdcp, void __user *arg)
{
	const struct dw_hdcp_cfg *cfg = hdcp->cfgs;
	const struct dw_hdcp_port_cfg *port_cfg;
	struct hl_drv_ioc_status status;
	int i;
	u32 val = 0;
	u32 connected_status = 0;
	u32 booted_status = 0;

	if (!arg)
		return -EFAULT;

	if (!hdcp->is_suspend) {
		for (i = 0; i < cfg->port_num; i++) {
			port_cfg = &cfg->port_cfg[i];
			regmap_read(hdcp->vo_grf, port_cfg->connect_reg.offset, &val);
			if (val & BIT(port_cfg->connect_reg.shift))
				connected_status |= 1 << port_cfg->port_id;
		}

		regmap_read(hdcp->vo_grf, cfg->boot_reg.offset, &val);
		if (val & BIT(cfg->boot_reg.shift))
			booted_status = 1;
	}

	status.connected_status = connected_status;
	status.booted_status = booted_status;

	if (copy_to_user(arg, &status, sizeof(status)))
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_MEMINFO implementation */
static long dw_hdcp_get_meminfo(struct hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_meminfo info;

	if (!arg)
		return -EFAULT;

	info.hpi_base  = hl_dev->hpi_resource->start;
	info.code_base = hl_dev->code_base;
	info.code_size = hl_dev->code_size;
	info.data_base = hl_dev->data_base;
	info.data_size = hl_dev->data_size;

	if (copy_to_user(arg, &info, sizeof(info)))
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_LOAD_CODE implementation */
static long dw_hdcp_load_code(struct hl_device *hl_dev, struct hl_drv_ioc_code __user *arg)
{
	struct hl_drv_ioc_code head;

	if (!arg || !hl_dev->code)
		return -EFAULT;

	if (copy_from_user(&head, arg, sizeof(head)))
		return -EFAULT;

	if (head.len > hl_dev->code_size)
		return -ENOSPC;

	if (hl_dev->code_loaded)
		return -EBUSY;

	if (copy_from_user(hl_dev->code, &arg->data, head.len))
		return -EFAULT;

	hl_dev->code_loaded = true;
	return 0;
}

/* HL_DRV_IOC_WRITE_DATA implementation */
static long dw_hdcp_write_data(struct hl_device *hl_dev, struct hl_drv_ioc_data __user *arg)
{
	struct hl_drv_ioc_data head;

	if (!arg || !hl_dev->data)
		return -EFAULT;

	if (copy_from_user(&head, arg, sizeof(head)))
		return -EFAULT;

	if (hl_dev->data_size < head.len)
		return -ENOSPC;
	if (hl_dev->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_from_user(hl_dev->data + head.offset, &arg->data, head.len))
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_READ_DATA implementation */
static long dw_hdcp_read_data(struct hl_device *hl_dev, struct hl_drv_ioc_data __user *arg)
{
	struct hl_drv_ioc_data head;

	if (!arg || !hl_dev->data)
		return -EFAULT;

	if (copy_from_user(&head, arg, sizeof(head)))
		return -EFAULT;

	if (hl_dev->data_size < head.len)
		return -ENOSPC;
	if (hl_dev->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_to_user(&arg->data, hl_dev->data + head.offset, head.len))
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_MEMSET_DATA implementation */
static long dw_hdcp_set_data(struct hl_device *hl_dev, void __user *arg)
{
	union {
		struct hl_drv_ioc_data data;
		unsigned char buf[sizeof(struct hl_drv_ioc_data) + 1];
	} u;

	if (!arg || !hl_dev->data)
		return -EFAULT;

	if (copy_from_user(&u.data, arg, sizeof(u.buf)))
		return -EFAULT;

	if (hl_dev->data_size < u.data.len)
		return -ENOSPC;
	if (hl_dev->data_size - u.data.len < u.data.offset)
		return -ENOSPC;

	memset(hl_dev->data + u.data.offset, u.data.data[0], u.data.len);
	return 0;
}

/* HL_DRV_IOC_READ_HPI implementation */
static long dw_hdcp_hpi_read(struct hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_hpi_reg reg;

	if (!arg)
		return -EFAULT;

	if (copy_from_user(&reg, arg, sizeof(reg)))
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(hl_dev->hpi_resource))
		return -EINVAL;

	reg.value = ioread32(hl_dev->hpi + reg.offset);
	if (copy_to_user(arg, &reg, sizeof(reg)))
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_WRITE_HPI implementation */
static long dw_hdcp_hpi_write(struct hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_hpi_reg reg;

	if (!arg)
		return -EFAULT;

	if (copy_from_user(&reg, arg, sizeof(reg)))
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(hl_dev->hpi_resource))
		return -EINVAL;

	iowrite32(reg.value, hl_dev->hpi + reg.offset);
#ifdef TROOT_GRIFFIN
	if ((reg.offset == 0x38) && ((reg.value & 0x000000ff) == 0x08))
		hl_dev->code_loaded = false;
#endif
	return 0;
}

static int dw_hdcp_check_hl_dev_slot(const struct hl_drv_ioc_meminfo *info,
				     struct hl_device *hl_dev)
{
	if (info->hpi_base == hl_dev->hpi_resource->start)
		return 0;

	return -EBUSY;
}

static void dw_hdcp_free_dma_areas(struct hl_device *hl_dev)
{
	struct dw_hdcp *hdcp = container_of(hl_dev, struct dw_hdcp, hl_dev);

	if (!hl_dev->code_is_phys_mem && hl_dev->code) {
		dma_free_coherent(hdcp->dev, hl_dev->code_size, hl_dev->code, hl_dev->code_base);
		hl_dev->code = NULL;
	}

	if (!hl_dev->data_is_phys_mem && hl_dev->data) {
		dma_free_coherent(hdcp->dev, hl_dev->data_size, hl_dev->data, hl_dev->data_base);
		hl_dev->data = NULL;
	}
}

static int dw_hdcp_alloc_dma_areas(struct hl_device *hl_dev, const struct hl_drv_ioc_meminfo *info)
{
	struct dw_hdcp *hdcp = container_of(hl_dev, struct dw_hdcp, hl_dev);

	hl_dev->code_size = info->code_size;
	hl_dev->code_is_phys_mem = (info->code_base != HL_DRIVER_ALLOCATE_DYNAMIC_MEM);
	hl_dev->data_size = info->data_size;
	hl_dev->data_is_phys_mem = (info->data_base != HL_DRIVER_ALLOCATE_DYNAMIC_MEM);

	if ((hl_dev->code_is_phys_mem && !hl_dev->code) ||
	    (hl_dev->data_is_phys_mem && !hl_dev->data)) {
		dev_err(hdcp->dev, "hdcp don't support phys mem\n");
		return -ENOMEM;
	}

	hl_dev->code = dma_alloc_coherent(hdcp->dev, hl_dev->code_size,
					  &hl_dev->code_base, GFP_KERNEL);
	if (!hl_dev->code)
		return -ENOMEM;

	hl_dev->data = dma_alloc_coherent(hdcp->dev, hl_dev->data_size,
					  &hl_dev->data_base, GFP_KERNEL);
	if (!hl_dev->data) {
		dw_hdcp_free_dma_areas(hl_dev);
		return -ENOMEM;
	}

	return 0;
}

/* HL_DRV_IOC_INIT implementation */
static long dw_hdcp_init(struct hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_meminfo info;
	int rc;

	if (!arg)
		return -EFAULT;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	rc = dw_hdcp_check_hl_dev_slot(&info, hl_dev);
	if (rc)
		return -EMFILE;

	if (!hl_dev->initialized) {
		rc = dw_hdcp_alloc_dma_areas(hl_dev, &info);
		if (rc < 0)
			goto err_free;

		hl_dev->initialized = true;
	}

	return 0;

err_free:
	dw_hdcp_free_dma_areas(hl_dev);
	hl_dev->initialized = false;

	return rc;
}

static void dw_hdcp_free_hl_dev_slot(struct hl_device *hl_dev)
{
	if (hl_dev->initialized)
		dw_hdcp_free_dma_areas(hl_dev);

	hl_dev->initialized  = false;
}

static long dw_hdcp_hld_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct hl_device *hl_dev;
	struct dw_hdcp *hdcp;
	struct miscdevice *misc_dev;
	void __user *data;

	if (!f)
		return -EFAULT;

	misc_dev = f->private_data;
	hdcp = container_of(misc_dev, struct dw_hdcp, misc_dev);
	hl_dev = &hdcp->hl_dev;

	data = (void __user *)arg;

	switch (cmd) {
	case HL_DRV_IOC_INIT:
		return dw_hdcp_init(hl_dev, data);
	case HL_DRV_IOC_MEMINFO:
		return dw_hdcp_get_meminfo(hl_dev, data);
	case HL_DRV_IOC_READ_HPI:
		return dw_hdcp_hpi_read(hl_dev, data);
	case HL_DRV_IOC_WRITE_HPI:
		return dw_hdcp_hpi_write(hl_dev, data);
	case HL_DRV_IOC_LOAD_CODE:
		return dw_hdcp_load_code(hl_dev, data);
	case HL_DRV_IOC_WRITE_DATA:
		return dw_hdcp_write_data(hl_dev, data);
	case HL_DRV_IOC_READ_DATA:
		return dw_hdcp_read_data(hl_dev, data);
	case HL_DRV_IOC_MEMSET_DATA:
		return dw_hdcp_set_data(hl_dev, data);

	case RK_DRV_IOC_GET_STATUS:
		return dw_hdcp_get_status(hdcp, data);
	case RK_DRV_IOC_RESET:
		return dw_hdcp_set_reset(hdcp, data);
	default:
		return -EINVAL;
	}

	return -ENOTTY;
}

static int dw_hdcp_hld_open(struct inode *inode, struct file *f)
{
	struct dw_hdcp *hdcp;
	struct miscdevice *misc_dev;

	misc_dev = f->private_data;
	hdcp = container_of(misc_dev, struct dw_hdcp, misc_dev);
	pm_runtime_get_sync(hdcp->dev);

	return 0;
}

static int dw_hdcp_hld_release(struct inode *inode, struct file *f)
{
	struct dw_hdcp *hdcp;
	struct miscdevice *misc_dev;

	misc_dev = f->private_data;
	hdcp = container_of(misc_dev, struct dw_hdcp, misc_dev);
	pm_runtime_put(hdcp->dev);

	return 0;
}

static const struct file_operations dw_hdcp_hld_file_operations = {
#ifdef CONFIG_COMPAT
	.compat_ioctl = dw_hdcp_hld_ioctl,
#endif
	.unlocked_ioctl = dw_hdcp_hld_ioctl,
	.open = dw_hdcp_hld_open,
	.release = dw_hdcp_hld_release,
	.owner = THIS_MODULE,
};

static int dw_hdcp_hld_init(struct dw_hdcp *hdcp, struct resource *res, void __iomem *base)
{
	hdcp->hl_dev.allocated = false;
	hdcp->hl_dev.initialized = false;
	hdcp->hl_dev.code_loaded = false;
	hdcp->hl_dev.code = NULL;
	hdcp->hl_dev.data = NULL;
	hdcp->hl_dev.hpi_resource = res;
	hdcp->hl_dev.hpi = base;

	hdcp->misc_dev.name = devm_kasprintf(hdcp->dev, GFP_KERNEL, "hl_dev%d", hdcp->id);
	if (!hdcp->misc_dev.name)
		return -ENOMEM;
	hdcp->misc_dev.minor = MISC_DYNAMIC_MINOR;
	hdcp->misc_dev.fops = &dw_hdcp_hld_file_operations;

	return misc_register(&hdcp->misc_dev);
}

static void dw_hdcp_hld_exit(struct dw_hdcp *hdcp)
{
	dw_hdcp_free_hl_dev_slot(&hdcp->hl_dev);

	misc_deregister(&hdcp->misc_dev);
}

static int dw_hdcp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_hdcp *hdcp;
	const struct dw_hdcp_cfg *cfgs;
	struct resource *res;
	void __iomem *base;
	int id, ret;

	hdcp = devm_kzalloc(dev, sizeof(*hdcp), GFP_KERNEL);
	if (!hdcp)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	id = of_alias_get_id(dev->of_node, "hdcp");
	if (id < 0)
		id = 0;

	hdcp->id = id;
	hdcp->dev = dev;

	cfgs = device_get_match_data(dev);
	if (!cfgs) {
		dev_err(dev, "no OF data can be matched with %p node\n", dev->of_node);
		return -EINVAL;
	}

	hdcp->cfgs = &cfgs[hdcp->id];

	hdcp->vo_grf = syscon_regmap_lookup_by_phandle(dev->of_node, "rockchip,vo-grf");
	if (IS_ERR(hdcp->vo_grf)) {
		dev_err(hdcp->dev, "Get vo-grf failed\n");
		return -ENODEV;
	}

	hdcp->rsts_bulk = devm_reset_control_array_get_exclusive(dev);
	if (IS_ERR(hdcp->rsts_bulk)) {
		dev_err(dev, "Get resets failed\n");
		return -ENODEV;
	}

	hdcp->num_clks = devm_clk_bulk_get_all(dev, &hdcp->clks);
	if (hdcp->num_clks < 1) {
		dev_err(dev, "Get clks failed\n");
		return -ENODEV;
	}

	ret = dw_hdcp_hld_init(hdcp, res, base);
	if (ret) {
		dev_err(dev, "hld init failed\n");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, hdcp);

	pm_runtime_enable(hdcp->dev);

	return 0;
}

static int dw_hdcp_remove(struct platform_device *pdev)
{
	struct dw_hdcp *hdcp = platform_get_drvdata(pdev);

	dw_hdcp_hld_exit(hdcp);

	pm_runtime_disable(hdcp->dev);

	return 0;
}

static int dw_hdcp_runtime_suspend(struct device *dev)
{
	struct dw_hdcp *hdcp = dev_get_drvdata(dev);

	hdcp->is_suspend = true;
	clk_bulk_disable_unprepare(hdcp->num_clks, hdcp->clks);

	dw_hdcp_free_hl(hdcp);

	return 0;
}

static int dw_hdcp_runtime_resume(struct device *dev)
{
	struct dw_hdcp *hdcp = dev_get_drvdata(dev);
	int ret;

	ret = clk_bulk_prepare_enable(hdcp->num_clks, hdcp->clks);
	if (ret)
		dev_err(dev, "prepare enable clk bulk failed\n");

	dw_hdcp_reset(hdcp);

	hdcp->is_suspend = false;
	return 0;
}

static const struct dev_pm_ops dw_hdcp_pm_ops = {
	SET_RUNTIME_PM_OPS(dw_hdcp_runtime_suspend, dw_hdcp_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
};

/*
 * rk3576 hdcp connect as follow:
 * HDMITX --> HDCP0 PORT1
 * DPTX --> HDCP1 PORT0
 */
static const struct dw_hdcp_cfg rk3576_hdcp_cfgs[] = {
	{
		.port_num = 1,
		.boot_reg = {0xd4, 20},
		.port_cfg = {
			{
				.connect_reg = {0xd4, 16},
				.port_id = 1,
			},
		},
	},
	{
		.port_num = 1,
		.boot_reg = {0xc8, 26},
		.port_cfg = {
			{
				.connect_reg = {0xc0, 6},
				.port_id = 0,
			},
		},
	},
};

/*
 * rk3588 hdcp connect as follow:
 * DPTX0 --> HDCP0 PORT0
 * DPTX1 --> HDCP0 PORT1
 * HDMIRX --> HDCP1 PORT0
 * HDMITX0 --> HDCP1 PORT1
 * HDMITX1 --> HDCP1 PORT2
 */
static const struct dw_hdcp_cfg rk3588_hdcp_cfgs[] = {
	{
		.port_num = 2,
		.boot_reg = {0x2c, 8},
		.port_cfg = {
			{
				.connect_reg = {0x20, 8},
				.port_id = 0,
			},
			{
				.connect_reg = {0x20, 24},
				.port_id = 1,
			},
		},
	},
	{
		.port_num = 3,
		.boot_reg = {0x3c, 16},
		.port_cfg = {
			{
				.connect_reg = {0x40, 8},
				.port_id = 0,
			},
			{
				.connect_reg = {0x3c, 24},
				.port_id = 1,
			},
			{
				.connect_reg = {0x40, 4},
				.port_id = 2,
			},
		},
	},
};

static const struct of_device_id dw_hdcp_of_match[] = {
	{
		.compatible = "rockchip,rk3576-hdcp",
		.data = &rk3576_hdcp_cfgs,
	},
	{
		.compatible = "rockchip,rk3588-hdcp",
		.data = &rk3588_hdcp_cfgs,
	},
	{}
};

MODULE_DEVICE_TABLE(of, dw_hdcp_of_match);

static struct platform_driver dw_hdcp_driver = {
	.probe = dw_hdcp_probe,
	.remove = dw_hdcp_remove,
	.driver = {
		.name = "dw-hdcp",
		.of_match_table = dw_hdcp_of_match,
		.pm = &dw_hdcp_pm_ops,
	},
};

module_platform_driver(dw_hdcp_driver);

MODULE_AUTHOR("Zhang Yubing <yubing.zhang@rock-chips.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip HDCP Host Library Driver");
