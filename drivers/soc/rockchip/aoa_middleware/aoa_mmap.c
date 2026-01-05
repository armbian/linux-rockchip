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

#define DEVICE_MAP_SRAM		"aoa-mmap"
#define DEVICE_MAP_EXTRAM	"aoa-mmap-extram"
#define DEVICE_MAP_NUM		2
#define AOA_MMAP_IOC_MAGIC	'a'
#define AOA_MMAP_IOC_GET_INFO	_IOR(AOA_MMAP_IOC_MAGIC, 1, struct aoa_mmap_info)

/**
 * Use generic types that are as compatible as possible with user-space information.
 */
struct aoa_mmap_info {
	u32 phys_addr;
	u32 size;
};

struct aoa_mmap_dev {
	void __iomem     *kvirt;	/* kernel virtual address, obtained by memremap */
	phys_addr_t       phys;		/* physical start */
	resource_size_t   size;		/* the size of ram */
	struct miscdevice misc;
};

struct aoa_mmap_devs {
	struct aoa_mmap_dev *am_d[DEVICE_MAP_NUM];	/* am_d[0]: sram, am_d[1]: ext-ram */
	struct device    *dev;				/* dev structure of platform device */
};

static char *device_map_name[] = { DEVICE_MAP_SRAM, DEVICE_MAP_EXTRAM };

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
		info.phys_addr = (u32)am_d->phys;
		info.size = (u32)am_d->size;
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
	struct aoa_mmap_devs *am_ds;
	struct aoa_mmap_dev *am_d;
	struct resource res;
	struct device_node *res_node;
	int ret = 0, n;

	am_ds = devm_kzalloc(&pdev->dev, sizeof(*am_ds), GFP_KERNEL);
	if (!am_ds)
		return ERR_PTR(-ENOMEM);
	am_ds->dev = &pdev->dev;

	for (n = 0; n < DEVICE_MAP_NUM; n++) {
		/**
		 * Try to read the memory-region phandle first.
		 * If there is no phandle for this index, treat it as "no more regions".
		 */
		res_node = of_parse_phandle(pdev->dev.of_node, "memory-region", n);
		if (!res_node)
			break;

		/* Convert the phandle to a resource */
		ret = of_address_to_resource(res_node, 0, &res);
		of_node_put(res_node);
		if (ret) {
			dev_err(&pdev->dev, "failed to parse reserved region address: %d\n", ret);
			goto err_unregister;
		}

		/* Only allocate am_d after confirming the region exists */
		am_d = devm_kzalloc(&pdev->dev, sizeof(*am_d), GFP_KERNEL);
		if (!am_d) {
			ret = -ENOMEM;
			goto err_unregister;
		}

		am_d->phys = res.start;
		am_d->size = resource_size(&res);

		/* Map the reserved memory */
		am_d->kvirt = devm_ioremap(am_ds->dev, am_d->phys, am_d->size);
		if (!am_d->kvirt) {
			dev_err(&pdev->dev, "ioremap failed\n");
			ret = -ENOMEM;
			goto err_unregister;
		}

		/* Initialize miscdevice */
		am_d->misc.minor = MISC_DYNAMIC_MINOR;
		am_d->misc.name  = device_map_name[n];
		am_d->misc.fops  = &aoa_mmap_fops;

		ret = misc_register(&am_d->misc);
		if (ret) {
			dev_err(&pdev->dev, "misc_register failed: %d\n", ret);
			goto err_unregister;
		}

		am_ds->am_d[n] = am_d;

		dev_info(&pdev->dev, "am_d[%d] mapped phys=%pa size=%u\n",
			 n, &am_d->phys, (unsigned int)am_d->size);
	}

	/* If no entry was registered, return an error */
	if (n == 0)
		return ERR_PTR(-ENODEV);

	return am_ds;

err_unregister:
	/**
	 * Cleanup: deregister all successfully registered misc devices.
	 * 'n' equals the number of successful registrations.
	 */
	for (int i = 0; i < n; i++)
		misc_deregister(&am_ds->am_d[i]->misc);

	return ERR_PTR(ret);
}

int aoa_mmap_remove(struct platform_device *pdev, void *am_map)
{
	struct aoa_mmap_devs *am_ds = am_map;
	int n;

	if (!am_ds)
		return -ENOMEM;

	for (n = 0; n < DEVICE_MAP_NUM; n++) {
		struct aoa_mmap_dev *am_d = am_ds->am_d[n];

		if (am_d)
			misc_deregister(&am_d->misc);
	}

	return 0;
}
