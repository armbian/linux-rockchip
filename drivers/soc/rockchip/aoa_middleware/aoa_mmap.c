// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rockchip AOA Memory Mapping Driver
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 */

#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>

#include "aoa_mmap.h"

#define DEVICE_NAME		"aoa-mmap"
#define AOA_MMAP_IOC_MAGIC	'a'
#define AOA_MMAP_IOC_GET_INFO	_IOR(AOA_MMAP_IOC_MAGIC, 1, struct aoa_mmap_info)

struct aoa_mmap_info {
	__u32 phys_addr;
	__u32 size;
};

struct aoa_mmap_dev {
	struct device    *dev;		/* dev structure of platform device */
	void __iomem     *kvirt;	/* kernel virtual address, obtained by memremap */
	phys_addr_t       phys;		/* fixed physical start (0x3ff20000) */
	u32               size;		/* size 64KB */
	struct miscdevice misc;
};

static int aoa_mmap_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct aoa_mmap_dev *am_d = container_of(misc, struct aoa_mmap_dev, misc);

	file->private_data = am_d;
	return 0;
}

static int aoa_mmap_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int aoa_mmap_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct aoa_mmap_dev *am_d = file->private_data;
	unsigned long length = vma->vm_end - vma->vm_start;

	/* The length cannot exceed the reserved size */
	if (length > am_d->size)
		return -EINVAL;

	/* Force Non-Cacheable */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	/* Mapping to user space by physical address */
	return remap_pfn_range(vma, vma->vm_start, am_d->phys >> PAGE_SHIFT,
			       length, vma->vm_page_prot);
}

static long aoa_mmap_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct aoa_mmap_dev *am_d = file->private_data;
	struct aoa_mmap_info info;

	if (unlikely(!am_d))
		return -ENODEV;
	if (IS_ERR(am_d))
		return PTR_ERR(am_d);

	switch (cmd) {
	case AOA_MMAP_IOC_GET_INFO:
		info.phys_addr = am_d->phys;
		info.size = am_d->size;
		if (copy_to_user((struct aoa_mmap_info __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct file_operations aoa_mmap_fops = {
	.owner          = THIS_MODULE,
	.open           = aoa_mmap_open,
	.release        = aoa_mmap_release,
	.mmap           = aoa_mmap_mmap,
	.unlocked_ioctl = aoa_mmap_ioctl,
};

void *aoa_mmap_probe(struct platform_device *pdev)
{
	struct aoa_mmap_dev *am_d;
	struct resource res;
	struct device_node *res_node;
	int ret;

	am_d = devm_kzalloc(&pdev->dev, sizeof(*am_d), GFP_KERNEL);
	if (!am_d)
		return ERR_PTR(-ENOMEM);
	am_d->dev = &pdev->dev;

	res_node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!res_node) {
		dev_err(&pdev->dev, "failed to get memory region node\n");
		return NULL;
	}

	ret = of_address_to_resource(res_node, 0, &res);
	of_node_put(res_node);
	if (ret) {
		dev_err(&pdev->dev, "failed to get reserved region address\n");
		return NULL;
	}

	am_d->phys = res.start;
	am_d->size = resource_size(&res);

	/* ioremap the SRAM area (Non-Cacheable Device memory) */
	am_d->kvirt = devm_ioremap(am_d->dev, am_d->phys, am_d->size);
	if (!am_d->kvirt) {
		dev_err(am_d->dev, "ioremap failed\n");
		return NULL;
	}

	am_d->misc.minor = MISC_DYNAMIC_MINOR;
	am_d->misc.name  = DEVICE_NAME;
	am_d->misc.fops  = &aoa_mmap_fops;

	ret = misc_register(&am_d->misc);
	if (ret) {
		dev_err(am_d->dev, "misc_register failed: %d\n", ret);
		return NULL;
	}

	dev_info(am_d->dev, "aoa_mmap_mem: mapped phys=%pa size=%u\n",
		 &am_d->phys, am_d->size);
	return am_d;
}

int aoa_mmap_remove(struct platform_device *pdev, void *am_d)
{
	if (!am_d)
		return -ENOMEM;

	misc_deregister(&((struct aoa_mmap_dev *)am_d)->misc);
	return 0;
}
