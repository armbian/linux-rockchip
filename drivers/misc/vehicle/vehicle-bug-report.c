// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rockchip vehicle bug report driver
 *
 * Copyright (c) 2025, Rockchip Electronics Co., Ltd.
 *
 * Author: Luo Wei <lw@rock-chips.com>
 */

#include "core.h"

static ssize_t _vehicle_bug_report_read_m0bus(struct vehicle_bug_report *report,
						char *buf, loff_t offset, size_t count)
{
	unsigned long flags;

	if (report->m0bus_ioaddr) {
		spin_lock_irqsave(&report->lock_m0bus, flags);
		memcpy(buf, report->m0bus_ioaddr + offset, count);
		spin_unlock_irqrestore(&report->lock_m0bus, flags);
		//pr_info("%s\n", (char *)buf);
		return count;
	} else {
		return sysfs_emit(buf, "error\n");
	}
}

static ssize_t _vehicle_bug_report_write_m0bus(struct vehicle_bug_report *report,
						 const char *buf, loff_t offset, size_t size)
{
	unsigned long flags;

	if (report->m0bus_ioaddr) {
		spin_lock_irqsave(&report->lock_m0bus, flags);
		memcpy(report->m0bus_ioaddr + offset, buf, size);
		spin_unlock_irqrestore(&report->lock_m0bus, flags);
		pr_debug("%s: offset=%d, size=%d\n", __func__, (int)offset, (int)size);
	} else {
		return -EINVAL;
	}

	return size;
}

static ssize_t _vehicle_bug_report_read_m0pmu(struct vehicle_bug_report *report,
						char *buf, loff_t offset, size_t count)
{
	unsigned long flags;

	if (report->m0pmu_ioaddr) {
		spin_lock_irqsave(&report->lock_m0pmu, flags);
		memcpy(buf, report->m0pmu_ioaddr + offset, count);
		spin_unlock_irqrestore(&report->lock_m0pmu, flags);
		//pr_info("%s\n", (char *)buf);
		return count;
	} else {
		return sysfs_emit(buf, "error\n");
	}
}

static ssize_t _vehicle_bug_report_write_m0pmu(struct vehicle_bug_report *report,
						 const char *buf, loff_t offset, size_t size)
{
	unsigned long flags;

	if (report->m0pmu_ioaddr) {
		spin_lock_irqsave(&report->lock_m0pmu, flags);
		memcpy(report->m0pmu_ioaddr + offset, buf, size);
		spin_unlock_irqrestore(&report->lock_m0pmu, flags);
		pr_debug("%s: offset=%d, size=%d\n", __func__, (int)offset, (int)size);
	} else
		return -EINVAL;

	return size;
}

static ssize_t _vehicle_bug_report_read_cluster(struct vehicle_bug_report *report,
						char *buf, loff_t offset, size_t count)
{
	unsigned long flags;

	if (report->cluster_ioaddr) {
		spin_lock_irqsave(&report->lock_cluster, flags);
		memcpy(buf, report->cluster_ioaddr + offset, count);
		spin_unlock_irqrestore(&report->lock_cluster, flags);
		//pr_info("%s\n", (char *)buf);
		return count;
	} else {
		return sysfs_emit(buf, "error\n");
	}
}

static ssize_t _vehicle_bug_report_write_cluster(struct vehicle_bug_report *report,
						 const char *buf, loff_t offset, size_t size)
{
	unsigned long flags;

	if (report->cluster_ioaddr) {
		spin_lock_irqsave(&report->lock_cluster, flags);
		memcpy(report->cluster_ioaddr + offset, buf, size);
		spin_unlock_irqrestore(&report->lock_cluster, flags);
		pr_debug("%s: offset=%d, size=%d\n", __func__, (int)offset, (int)size);
	} else
		return -EINVAL;

	return size;
}

static ssize_t _vehicle_bug_report_read_mcu(struct vehicle_bug_report *report,
						char *buf, loff_t offset, size_t count)
{
	unsigned long flags;

	if (report->mcu_ioaddr) {
		spin_lock_irqsave(&report->lock_mcu, flags);
		memcpy(buf, report->mcu_ioaddr + offset, count);
		spin_unlock_irqrestore(&report->lock_mcu, flags);
		//pr_info("%s\n", (char *)buf);
		return count;
	} else {
		return sysfs_emit(buf, "error\n");
	}
}

static ssize_t _vehicle_bug_report_write_mcu(struct vehicle_bug_report *report,
						 const char *buf, loff_t offset, size_t size)
{
	unsigned long flags;

	if (report->mcu_ioaddr) {
		spin_lock_irqsave(&report->lock_mcu, flags);
		memcpy(report->mcu_ioaddr + offset, buf, size);
		spin_unlock_irqrestore(&report->lock_mcu, flags);
		pr_debug("%s: offset=%d, size=%d\n", __func__, (int)offset, (int)size);
	} else
		return -EINVAL;

	return size;
}

static ssize_t _vehicle_bug_report_read_dsp(struct vehicle_bug_report *report,
						char *buf, loff_t offset, size_t count)
{
	unsigned long flags;

	if (report->dsp_ioaddr) {
		spin_lock_irqsave(&report->lock_dsp, flags);
		memcpy(buf, report->dsp_ioaddr + offset, count);
		spin_unlock_irqrestore(&report->lock_dsp, flags);
		//pr_info("%s\n", (char *)buf);
		return count;
	} else {
		return sysfs_emit(buf, "error\n");
	}
}

static ssize_t _vehicle_bug_report_write_dsp(struct vehicle_bug_report *report,
						 const char *buf, loff_t offset, size_t size)
{
	unsigned long flags;

	if (report->dsp_ioaddr) {
		spin_lock_irqsave(&report->lock_dsp, flags);
		memcpy(report->dsp_ioaddr + offset, buf, size);
		spin_unlock_irqrestore(&report->lock_dsp, flags);
		pr_debug("%s: offset=%d, size=%d\n", __func__, (int)offset, (int)size);
	} else
		return -EINVAL;

	return size;
}

static ssize_t vehicle_bug_report_read_m0bus(struct file *filp, struct kobject *kobj,
					     struct bin_attribute *attr,
					     char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_read_m0bus(report, buffer, offset, count);

	return ret;
}

static ssize_t vehicle_bug_report_write_m0bus(struct file *filp, struct kobject *kobj,
					      struct bin_attribute *attr,
					      char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_write_m0bus(report, buffer, offset, count);

	return ret;
}

static ssize_t vehicle_bug_report_read_m0pmu(struct file *filp, struct kobject *kobj,
					     struct bin_attribute *attr,
					     char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_read_m0pmu(report, buffer, offset, count);

	return ret;
}

static ssize_t vehicle_bug_report_write_m0pmu(struct file *filp, struct kobject *kobj,
					      struct bin_attribute *attr,
					      char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_write_m0pmu(report, buffer, offset, count);

	return ret;
}


static ssize_t vehicle_bug_report_read_cluster(struct file *filp, struct kobject *kobj,
					struct bin_attribute *attr,
					char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_read_cluster(report, buffer, offset, count);

	return ret;
}

static ssize_t vehicle_bug_report_write_cluster(struct file *filp, struct kobject *kobj,
						struct bin_attribute *attr,
						char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_write_cluster(report, buffer, offset, count);

	return ret;
}

static ssize_t vehicle_bug_report_read_mcu(struct file *filp, struct kobject *kobj,
					   struct bin_attribute *attr,
					   char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_read_mcu(report, buffer, offset, count);

	return ret;
}

static ssize_t vehicle_bug_report_write_mcu(struct file *filp, struct kobject *kobj,
					    struct bin_attribute *attr,
					    char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_write_mcu(report, buffer, offset, count);

	return ret;
}

static ssize_t vehicle_bug_report_read_dsp(struct file *filp, struct kobject *kobj,
					   struct bin_attribute *attr,
					   char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_read_dsp(report, buffer, offset, count);

	return ret;
}

static ssize_t vehicle_bug_report_write_dsp(struct file *filp, struct kobject *kobj,
					    struct bin_attribute *attr,
					    char *buffer, loff_t offset, size_t count)
{
	struct vehicle_bug_report *report;
	int ret;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	ret = _vehicle_bug_report_write_dsp(report, buffer, offset, count);

	return ret;
}

static BIN_ATTR(m0bus, 0644, vehicle_bug_report_read_m0bus,
		vehicle_bug_report_write_m0bus, 0x040000);
static BIN_ATTR(m0pmu, 0644, vehicle_bug_report_read_m0pmu,
		vehicle_bug_report_write_m0pmu, 0x020000);
static BIN_ATTR(cluster, 0644, vehicle_bug_report_read_cluster,
		vehicle_bug_report_write_cluster, 0x080000);
static BIN_ATTR(mcu, 0644, vehicle_bug_report_read_mcu,
		vehicle_bug_report_write_mcu, 0x020000);
static BIN_ATTR(dsp, 0644, vehicle_bug_report_read_dsp,
		vehicle_bug_report_write_dsp, 0x020000);

/*
 * Class attributes
 */
static struct bin_attribute *vehicle_bug_report_sysfs_attrs[] = {
	&bin_attr_m0bus,
	&bin_attr_m0pmu,
	&bin_attr_cluster,
	&bin_attr_mcu,
	&bin_attr_dsp,
	NULL,
};

BIN_ATTRIBUTE_GROUPS(vehicle_bug_report_sysfs);

static ssize_t vehicle_bug_report_attr_show(struct kobject *kobj,
					    struct attribute *attr, char *buf)
{
	struct vehicle_bug_report *report;
	struct vehicle_bug_report_attribute *vehicle_bug_report_attr;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	vehicle_bug_report_attr = container_of(attr, struct vehicle_bug_report_attribute, attr);

	if (!vehicle_bug_report_attr->show)
		return -ENOENT;

	return vehicle_bug_report_attr->show(report, buf);
}

static ssize_t vehicle_bug_report_attr_store(struct kobject *kobj, struct attribute *attr,
					     const char *buf, size_t size)
{
	struct vehicle_bug_report *report;
	struct vehicle_bug_report_attribute *vehicle_bug_report_attr;

	report = container_of(kobj, struct vehicle_bug_report, kobj);
	vehicle_bug_report_attr = container_of(attr, struct vehicle_bug_report_attribute, attr);

	if (!vehicle_bug_report_attr->store)
		return -ENOENT;

	return vehicle_bug_report_attr->store(report, buf, size);
}

static const struct sysfs_ops vehicle_bug_report_sysfs_ops = {
	.show = vehicle_bug_report_attr_show,
	.store = vehicle_bug_report_attr_store,
};

static const struct kobj_type vehicle_bug_report_ktype = {
	.sysfs_ops = &vehicle_bug_report_sysfs_ops,
	.default_groups = vehicle_bug_report_sysfs_groups,
};

static void vehicle_bug_report_uninit_sysfs(struct vehicle_bug_report *report)
{
	if (!report) {
		pr_err("report or amp_pdev is null\n");
		return;
	}

	if (kobject_name(&report->kobj) == NULL)
		return;

	kobject_del(&report->kobj);
	kobject_put(&report->kobj);

	memset(&report->kobj, 0, sizeof(report->kobj));
}

static int vehicle_bug_report_init_sysfs(struct vehicle_bug_report *report)
{
	struct platform_device *pdev = report->pdev;
	int r;

	snprintf(report->alias, sizeof(report->alias), "dmesg");

	r = kobject_init_and_add(&report->kobj, &vehicle_bug_report_ktype,
		&pdev->dev.kobj, "%s", report->alias);
	if (r) {
		pr_info("failed to create sysfs files\n");
		kobject_put(&report->kobj);
		goto err;
	}

	return 0;

err:
	vehicle_bug_report_uninit_sysfs(report);

	return r;
}

static void *vehicle_bug_report_map_kernel(phys_addr_t start, size_t len)
{
	int i;
	void *vaddr;
	pgprot_t pgprot;
	phys_addr_t phys;
	int npages = PAGE_ALIGN(len) / PAGE_SIZE;
	struct page **p = vmalloc(sizeof(struct page *) * npages);

	if (!p)
		return NULL;

	pgprot = pgprot_noncached(PAGE_KERNEL);

	phys = start;
	for (i = 0; i < npages; i++) {
		p[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	if (pfn_valid(start >> PAGE_SHIFT))
		vaddr = vmap(p, npages, VM_MAP, pgprot);
	else
		vaddr = ioremap(start, len);

	vfree(p);

	return vaddr;
}

static int vehicle_bug_report_probe(struct platform_device *pdev)
{
	struct vehicle_bug_report *report = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct device_node *node_m0bus;
	struct resource res_m0bus;
	struct device_node *node_m0pmu;
	struct resource res_m0pmu;
	struct device_node *node_cluster;
	struct resource res_cluster;
	struct device_node *node_mcu;
	struct resource res_mcu;
	struct device_node *node_dsp;
	struct resource res_dsp;
	int ret;

	if (g_vehicle_hw == NULL)
		return -ENOMEM;

	report = devm_kzalloc(&pdev->dev, sizeof(struct vehicle_bug_report), GFP_KERNEL);
	if (!report)
		return -ENOMEM;

	node_m0bus = of_parse_phandle(np, "memory-region-m0bus", 0);
	if (node_m0bus) {
		ret = of_address_to_resource(node_m0bus, 0, &res_m0bus);
		of_node_put(node_m0bus);
		if (!ret) {
			report->m0bus_size = (unsigned int)resource_size(&res_m0bus);
			report->m0bus_addr = res_m0bus.start;
			report->m0bus_ioaddr = vehicle_bug_report_map_kernel(report->m0bus_addr,
									     report->m0bus_size);
		} else {
			pr_info("%s: failed to get resource for m0bus\n", __func__);
			return ret;
		}
	}

	node_m0pmu = of_parse_phandle(np, "memory-region-m0pmu", 0);
	if (node_m0pmu) {
		ret = of_address_to_resource(node_m0pmu, 0, &res_m0pmu);
		of_node_put(node_m0pmu);
		if (!ret) {
			report->m0pmu_size = (unsigned int)resource_size(&res_m0pmu);
			report->m0pmu_addr = res_m0pmu.start;
			report->m0pmu_ioaddr = vehicle_bug_report_map_kernel(report->m0pmu_addr,
									     report->m0pmu_size);
		} else {
			pr_info("%s: failed to get resource for m0pmu\n", __func__);
			return ret;
		}
	}

	node_cluster = of_parse_phandle(np, "memory-region-cluster", 0);
	if (node_cluster) {
		ret = of_address_to_resource(node_cluster, 0, &res_cluster);
		of_node_put(node_cluster);
		if (!ret) {
			report->cluster_size = (unsigned int)resource_size(&res_cluster);
			report->cluster_addr = res_cluster.start;
			report->cluster_ioaddr = vehicle_bug_report_map_kernel(report->cluster_addr,
									     report->cluster_size);
		} else {
			pr_info("%s: failed to get resource for cluster\n", __func__);
			return ret;
		}
	}

	node_mcu = of_parse_phandle(np, "memory-region-mcu", 0);
	if (node_mcu) {
		ret = of_address_to_resource(node_mcu, 0, &res_mcu);
		of_node_put(node_mcu);
		if (!ret) {
			report->mcu_size = (unsigned int)resource_size(&res_mcu);
			report->mcu_addr = res_mcu.start;
			report->mcu_ioaddr = vehicle_bug_report_map_kernel(report->mcu_addr,
									     report->mcu_size);
		} else {
			pr_info("%s: failed to get resource for mcu\n", __func__);
			return ret;
		}
	}

	node_dsp = of_parse_phandle(np, "memory-region-dsp", 0);
	if (node_dsp) {
		ret = of_address_to_resource(node_dsp, 0, &res_dsp);
		of_node_put(node_dsp);
		if (!ret) {
			report->dsp_size = (unsigned int)resource_size(&res_dsp);
			report->dsp_addr = res_dsp.start;
			report->dsp_ioaddr = vehicle_bug_report_map_kernel(report->dsp_addr,
									     report->dsp_size);
		} else {
			pr_info("%s: failed to get resource for dsp\n", __func__);
			return ret;
		}
	}

	report->dev = dev;

	pr_info("%s: m0bus_ioaddr=0x%p, m0bus_addr=0x%x,size=0x%x\n", __func__,
		report->m0bus_ioaddr, (int)report->m0bus_addr, report->m0bus_size);

	pr_info("%s: m0pmu_ioaddr=0x%p, m0pmu_addr=0x%x,size=0x%x\n", __func__,
		report->m0pmu_ioaddr, (int)report->m0pmu_addr, report->m0pmu_size);

	pr_info("%s: cluster_ioaddr=0x%p, cluster_addr=0x%x,size=0x%x\n", __func__,
		report->cluster_ioaddr, (int)report->cluster_addr, report->cluster_size);

	pr_info("%s: mcu_ioaddr=0x%p, mcu_addr=0x%x,size=0x%x\n", __func__,
		report->mcu_ioaddr, (int)report->mcu_addr, report->mcu_size);

	pr_info("%s: dsp_ioaddr=0x%p, dsp_addr=0x%x,size=0x%x\n", __func__,
		report->dsp_ioaddr, (int)report->dsp_addr, report->dsp_size);

	report->pdev = pdev;

	spin_lock_init(&report->lock_m0bus);
	spin_lock_init(&report->lock_m0pmu);
	spin_lock_init(&report->lock_cluster);
	spin_lock_init(&report->lock_mcu);
	spin_lock_init(&report->lock_dsp);
	mutex_init(&report->ops_mutex);

	vehicle_bug_report_init_sysfs(report);

	platform_set_drvdata(pdev, report);
	g_vehicle_hw->vehicle_bug_report = report;

	return 0;
}

static int vehicle_bug_report_remove(struct platform_device *pdev)
{
	struct vehicle_bug_report *report = platform_get_drvdata(pdev);

	vehicle_bug_report_uninit_sysfs(report);

	vunmap(report->m0bus_ioaddr);
	vunmap(report->m0pmu_ioaddr);
	vunmap(report->cluster_ioaddr);
	vunmap(report->mcu_ioaddr);
	vunmap(report->dsp_ioaddr);

	return 0;
}

static const struct of_device_id vehicle_bug_report_of_match[] = {
	{ .compatible = "rockchip,vehicle-bug-report", },
	{},
};
MODULE_DEVICE_TABLE(of, vehicle_bug_report_of_match);

static struct platform_driver vehicle_bug_report_driver = {
	.probe = vehicle_bug_report_probe,
	.remove = vehicle_bug_report_remove,
	.driver = {
		.name = "vehicle-bug-report",
		.of_match_table = vehicle_bug_report_of_match,
	},
};
module_platform_driver(vehicle_bug_report_driver);

MODULE_ALIAS("platform:vehicle-bug-report");
MODULE_AUTHOR("Luo Wei <lw@rock-chips.com>");
MODULE_DESCRIPTION("rockchip vehicle bug report driver for cluster, mcu, dsp etc");
MODULE_LICENSE("GPL");
