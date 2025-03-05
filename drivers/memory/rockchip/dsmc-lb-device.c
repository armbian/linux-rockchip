// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/version_compat_defs.h>

#include "dsmc-host.h"

struct dsmc_cs {
	struct cdev cdev[DSMC_LB_MAX_RGN];
};

static struct dsmc_cs cs_info[DSMC_MAX_SLAVE_NUM];
static struct class *dsmc_class;
static dev_t dsmc_devt;

static inline int get_cs_index(struct inode *inode)
{
	return iminor(inode) / DSMC_LB_MAX_RGN;
}

static inline int get_mem_region_index(struct inode *inode)
{
	return iminor(inode) % DSMC_LB_MAX_RGN;
}

static int dsmc_open(struct inode *inode, struct file *pfile)
{
	struct rockchip_dsmc_device *dsmc_dev = NULL;
	struct rockchip_dsmc *dsmc = NULL;
	struct dsmc_config_cs *cfg;
	struct dsmc_cs_map *map;
	int cs_index, mem_region_index;

	cs_index = get_cs_index(inode);
	mem_region_index = get_mem_region_index(inode);

	dsmc_dev = rockchip_dsmc_find_device_by_compat(rockchip_dsmc_get_compat(0));
	if (dsmc_dev == NULL)
		return -EINVAL;

	dsmc = &dsmc_dev->dsmc;

	if (cs_index < DSMC_MAX_SLAVE_NUM)
		cfg = &dsmc->cfg.cs_cfg[cs_index];
	else
		return -EINVAL;
	if ((cfg->device_type == DSMC_UNKNOWN_DEVICE) ||
	    (!cfg->slv_rgn[mem_region_index].status))
		return -EINVAL;

	map = &dsmc->cs_map[cs_index];

	pfile->private_data = (void *)&map->region_map[mem_region_index];

	return 0;
}

static int dsmc_release(struct inode *inode, struct file *pfile)
{
	return 0;
}

static int dsmc_mmap(struct file *pfile, struct vm_area_struct *vma)
{
	struct dsmc_map *region = (struct dsmc_map *)pfile->private_data;
	unsigned long pfn;
	unsigned long vm_size = 0;

	if (!region)
		return -EINVAL;

	vm_flags_set(vma, VM_PFNMAP | VM_DONTDUMP);
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);

	vm_size = vma->vm_end - vma->vm_start;

	pfn = __phys_to_pfn(region->phys);

	if (remap_pfn_range(vma, vma->vm_start, pfn, vm_size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static const struct file_operations dsmc_fops = {
	.owner = THIS_MODULE,
	.open = dsmc_open,
	.release = dsmc_release,
	.mmap = dsmc_mmap,
};

int rockchip_dsmc_lb_class_create(const char *name)
{
	int ret;

	dsmc_class = class_create(THIS_MODULE, name);
	if (IS_ERR(dsmc_class)) {
		ret = PTR_ERR(dsmc_class);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(rockchip_dsmc_lb_class_create);

int rockchip_dsmc_lb_class_destroy(void)
{
	if (!dsmc_class)
		return 0;

	class_destroy(dsmc_class);
	dsmc_class = NULL;

	return 0;
}
EXPORT_SYMBOL(rockchip_dsmc_lb_class_destroy);

int rockchip_dsmc_register_lb_device(struct device *dev, uint32_t cs)
{
	int ret, j;
	struct device *device_ret;

	if (!dev || (cs >= DSMC_MAX_SLAVE_NUM) || (!dsmc_class))
		return -EINVAL;

	ret = alloc_chrdev_region(&dsmc_devt, 0,
				  DSMC_LB_MAX_RGN, "dsmc");
	if (ret < 0) {
		dev_err(dev, "Failed to alloc dsmc device region\n");
		return -ENODEV;
	}

	for (j = 0; j < DSMC_LB_MAX_RGN; j++) {
		device_ret = device_create(dsmc_class, NULL,
					   MKDEV(MAJOR(dsmc_devt), cs * DSMC_LB_MAX_RGN + j),
					   NULL, "dsmc/cs%d/region%d", cs, j);
		if (IS_ERR(device_ret)) {
			dev_err(dev, "Failed to create device for cs%d region%d\n", cs, j);
			ret = PTR_ERR(device_ret);
			goto err_device_create;
		}
		cdev_init(&cs_info[cs].cdev[j], &dsmc_fops);
		ret = cdev_add(&cs_info[cs].cdev[j],
			       MKDEV(MAJOR(dsmc_devt), cs * DSMC_LB_MAX_RGN + j), 1);
		if (ret) {
			dev_err(dev, "Failed to add cdev for cs%d region%d\n", cs, j);
			goto err_cdev_add;
		}
	}

	return 0;

err_cdev_add:
	device_destroy(dsmc_class, MKDEV(MAJOR(dsmc_devt), cs * DSMC_LB_MAX_RGN + j));

err_device_create:
	while (j-- > 0) {
		device_destroy(dsmc_class, MKDEV(MAJOR(dsmc_devt), cs * DSMC_LB_MAX_RGN + j));
		cdev_del(&cs_info[cs].cdev[j]);
	}
	unregister_chrdev_region(dsmc_devt, DSMC_LB_MAX_RGN);

	return ret;
}
EXPORT_SYMBOL(rockchip_dsmc_register_lb_device);

int rockchip_dsmc_unregister_lb_device(struct device *dev, uint32_t cs)
{
	int j;

	if (!dev || (cs >= DSMC_MAX_SLAVE_NUM))
		return -EINVAL;

	for (j = 0; j < DSMC_LB_MAX_RGN; j++) {
		device_destroy(dsmc_class,
			       MKDEV(MAJOR(dsmc_devt),
			       cs * DSMC_LB_MAX_RGN + j));
		cdev_del(&cs_info->cdev[j]);
	}
	unregister_chrdev_region(dsmc_devt, DSMC_LB_MAX_RGN);

	return 0;
}
EXPORT_SYMBOL(rockchip_dsmc_unregister_lb_device);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhihuan He <huan.he@rock-chips.com>");
MODULE_DESCRIPTION("ROCKCHIP DSMC local bus device");
