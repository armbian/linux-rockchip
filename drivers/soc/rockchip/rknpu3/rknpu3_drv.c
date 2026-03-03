// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author: Chifred <chifred@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/miscdevice.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>

#ifndef FPGA_PLATFORM
#include <soc/rockchip/rockchip_iommu.h>
#endif

/* IOMMU poll timeout for power off */
#define NPU_MMU_DISABLED_POLL_PERIOD_US		1000
#define NPU_MMU_DISABLED_POLL_TIMEOUT_US	20000

#include "rknpu3_drv.h"
#include "rknpu3_core.h"
#include "rknpu3_reset.h"
#include "rknpu3_task.h"
#include "rknpu3_memory.h"
#include "rknpu3_config.h"
#include "rknpu3_devfreq.h"

/* IOMMU fault handler */
static int rknpu3_iommu_fault_handler(struct iommu_domain *domain,
				     struct device *dev,
				     unsigned long iova,
				     int flags, void *token)
{
	struct rknpu3_device *rknpu3_dev = token;

	LOG_DEV_ERROR(dev, "IOMMU page fault! iova=0x%lx flags=0x%x\n", iova, flags);

	if (rknpu3_dev) {
		/* Print current state info for debugging */
		LOG_DEV_ERROR(dev, "NPU current state: core0_available=%d\n",
			      rknpu3_dev->core_status ?
			      rknpu3_dev->core_status[0].is_available : -1);
	}

	/* Returning non-zero causes system to print more debug info */
	return -EIO;
}

#ifdef FPGA_PLATFORM
/* IRQ configuration */
static const struct rknpu3_irqs_data fpga_npu_irqs[] = {
	{ "npu0_irq", rknpu3_core0_irq_handler },
};
#else
static const struct rknpu3_irqs_data rk3572_npu_irqs[] = {
	{ "npu0_irq", rknpu3_core0_irq_handler },
};
#endif

/* Reset configuration */
static const struct rknpu3_reset_data rknpu3_resets[] = {
	{ "srst_a_npu", "srst_a_nrv" }
};

/* Default configuration used when no platform-specific data is provided */
#ifdef FPGA_PLATFORM
static const struct rknpu3_config rknpu3_default_config = {
	.num_cores = 1,
	.axi_port_num = 4,
	.iommu_en = 0,
	.enable_profiling = false,
	.dma_mask = DMA_BIT_MASK(40),
	.irqs = fpga_npu_irqs,
	.resets = rknpu3_resets,
	.num_irqs = ARRAY_SIZE(fpga_npu_irqs),
	.num_resets = ARRAY_SIZE(rknpu3_resets),
	.bus_bit = 256,
};

static const struct of_device_id rknpu3_of_match[] = {
	{
		.compatible = "rockchip,rk3572-rknpu",
		.data = &rknpu3_default_config,
	},
	{},
};
#else
static const struct rknpu3_config rknpu3_default_config = {
	.num_cores = 1,
	.axi_port_num = 4,
	.iommu_en = 0,
	.enable_profiling = false,
	.dma_mask = DMA_BIT_MASK(40),
	.irqs = rk3572_npu_irqs,
	.resets = rknpu3_resets,
	.num_irqs = ARRAY_SIZE(rk3572_npu_irqs),
	.num_resets = ARRAY_SIZE(rknpu3_resets),
	.bus_bit = 256,
};

static const struct of_device_id rknpu3_of_match[] = {
	{
		.compatible = "rockchip,rk3572-rknpu",
		.data = &rknpu3_default_config,
	},
	{},
};
#endif

static int rknpu3_get_driver_version(uint32_t *version_code)
{
	if (!version_code)
		return -EINVAL;

	*version_code = RKNPU3_GET_DRV_VERSION_CODE(DRIVER_MAJOR, DRIVER_MINOR,
						   DRIVER_PATCHLEVEL);
	return 0;
}

/* Power management functions */
static int rknpu3_power_on(struct rknpu3_device *rknpu3_dev);
static int rknpu3_power_off(struct rknpu3_device *rknpu3_dev);

static void rknpu3_power_off_delay_work(struct work_struct *power_off_work)
{
	int ret = 0;
	struct rknpu3_device *rknpu3_dev =
		container_of(to_delayed_work(power_off_work),
			     struct rknpu3_device, power_off_work);

	mutex_lock(&rknpu3_dev->power_lock);
	if (atomic_dec_if_positive(&rknpu3_dev->power_refcount) == 0) {
		ret = rknpu3_power_off(rknpu3_dev);
		if (ret)
			atomic_inc(&rknpu3_dev->power_refcount);
	}
	mutex_unlock(&rknpu3_dev->power_lock);

	if (ret)
		rknpu3_power_put_delay(rknpu3_dev);
}

int rknpu3_power_get(struct rknpu3_device *rknpu3_dev)
{
	int ret = 0;

	cancel_delayed_work_sync(&rknpu3_dev->power_off_work);

	mutex_lock(&rknpu3_dev->power_lock);
	if (atomic_inc_return(&rknpu3_dev->power_refcount) == 1)
		ret = rknpu3_power_on(rknpu3_dev);
	mutex_unlock(&rknpu3_dev->power_lock);

	return ret;
}

int rknpu3_power_put(struct rknpu3_device *rknpu3_dev)
{
	int ret = 0;

	mutex_lock(&rknpu3_dev->power_lock);
	if (atomic_dec_if_positive(&rknpu3_dev->power_refcount) == 0) {
		ret = rknpu3_power_off(rknpu3_dev);
		if (ret)
			atomic_inc(&rknpu3_dev->power_refcount);
	}
	mutex_unlock(&rknpu3_dev->power_lock);

	if (ret)
		rknpu3_power_put_delay(rknpu3_dev);

	return ret;
}

int rknpu3_power_put_delay(struct rknpu3_device *rknpu3_dev)
{
	if (rknpu3_dev->power_put_delay == 0)
		return rknpu3_power_put(rknpu3_dev);

	mutex_lock(&rknpu3_dev->power_lock);
	if (atomic_read(&rknpu3_dev->power_refcount) == 1)
		mod_delayed_work(
			rknpu3_dev->power_off_wq, &rknpu3_dev->power_off_work,
			msecs_to_jiffies(rknpu3_dev->power_put_delay));
	else
		atomic_dec_if_positive(&rknpu3_dev->power_refcount);
	mutex_unlock(&rknpu3_dev->power_lock);

	return 0;
}

static int rknpu3_power_on(struct rknpu3_device *rknpu3_dev)
{
	struct device *dev = rknpu3_dev->dev;
	int ret = -EINVAL;

#ifndef FPGA_PLATFORM
	LOG_DEV_DEBUG(dev, "rknpu3_power_on\n");

	/* Enable regulators */
	if (rknpu3_dev->vdd) {
		ret = regulator_enable(rknpu3_dev->vdd);
		if (ret) {
			LOG_DEV_ERROR(dev,
				      "failed to enable vdd regulator: %d\n", ret);
			return ret;
		}
	}

	if (rknpu3_dev->mem) {
		ret = regulator_enable(rknpu3_dev->mem);
		if (ret) {
			LOG_DEV_ERROR(dev,
				      "failed to enable mem regulator: %d\n", ret);
			goto err_disable_vdd;
		}
	}
#endif

	/* Enable clocks */
	if (rknpu3_dev->num_clks > 0) {
		ret = clk_bulk_prepare_enable(rknpu3_dev->num_clks, rknpu3_dev->clks);
		if (ret) {
			LOG_DEV_ERROR(dev, "failed to enable clocks: %d\n", ret);
			goto err_disable_mem;
		}
	}

#ifndef FPGA_PLATFORM
	/* Lock devfreq during power state changes */
	rknpu3_devfreq_lock(rknpu3_dev);
#endif

	/* Enable runtime PM */
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		LOG_DEV_ERROR(dev,
			      "failed to get pm runtime for rknpu: %d\n", ret);
		pm_runtime_put_noidle(dev);
		goto err_devfreq_unlock;
	}

	/* Enable IOMMU runtime PM if present */
	if (rknpu3_dev->iommu_dev) {
		ret = pm_runtime_get_sync(rknpu3_dev->iommu_dev);
		if (ret < 0) {
			LOG_DEV_WARN(dev, "failed to power on IOMMU: %d\n", ret);
			pm_runtime_put_noidle(rknpu3_dev->iommu_dev);
			/* Continue without IOMMU - not fatal */
		}
	}

#ifndef FPGA_PLATFORM
	rknpu3_devfreq_unlock(rknpu3_dev);
#endif

	LOG_DEV_DEBUG(dev, "power on success\n");
	return 0;

err_devfreq_unlock:
	rknpu3_devfreq_unlock(rknpu3_dev);
	if (rknpu3_dev->num_clks > 0)
		clk_bulk_disable_unprepare(rknpu3_dev->num_clks, rknpu3_dev->clks);
err_disable_mem:
	if (rknpu3_dev->mem)
		regulator_disable(rknpu3_dev->mem);
#ifndef FPGA_PLATFORM
err_disable_vdd:
	if (rknpu3_dev->vdd)
		regulator_disable(rknpu3_dev->vdd);
#endif
	return ret;
}

static int rknpu3_power_off(struct rknpu3_device *rknpu3_dev)
{
	struct device *dev = rknpu3_dev->dev;
	int ret = 0;

#ifndef FPGA_PLATFORM
	bool val;

	LOG_DEV_DEBUG(dev, "rknpu3_power_off\n");

	/* Lock devfreq during power state changes */
	rknpu3_devfreq_lock(rknpu3_dev);
#endif

	/* Release runtime PM */
	pm_runtime_put_sync(dev);

	/*
	 * Because IOMMU's runtime suspend callback is asynchronous,
	 * So it may be executed after the NPU is turned off after PD/CLK/VD,
	 * and the runtime suspend callback has a register access.
	 * If the PD/VD/CLK is closed, the register access will crash.
	 * As a workaround, it's safe to close pd stuff until iommu disabled.
	 */
#ifndef FPGA_PLATFORM
	if (rknpu3_dev->iommu_en && rknpu3_dev->iommu_dev) {
		pm_runtime_put_sync(rknpu3_dev->iommu_dev);
		ret = readx_poll_timeout(rockchip_iommu_is_enabled, rknpu3_dev->iommu_dev, val,
					 !val, NPU_MMU_DISABLED_POLL_PERIOD_US,
					 NPU_MMU_DISABLED_POLL_TIMEOUT_US);
		if (ret) {
			LOG_DEV_ERROR(dev, "iommu still enabled, power on again\n");
			pm_runtime_get_sync(dev);
			goto out_devfreq_unlock;
		}
	}
out_devfreq_unlock:
	rknpu3_devfreq_unlock(rknpu3_dev);
	if (ret)
		return ret;
#endif

	/* Disable clocks */
	if (rknpu3_dev->num_clks > 0)
		clk_bulk_disable_unprepare(rknpu3_dev->num_clks, rknpu3_dev->clks);

#ifndef FPGA_PLATFORM
	/* Disable regulators */
	if (rknpu3_dev->mem)
		regulator_disable(rknpu3_dev->mem);

	if (rknpu3_dev->vdd)
		regulator_disable(rknpu3_dev->vdd);
#endif

	return ret;
}

static long rknpu3_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct rknpu3_session *session = filp->private_data;
	struct rknpu3_device *rknpu3_dev;
	int ret = 0;
	void __user *argp = (void __user *)arg;

	if (!session || !session->rknpu3_dev)
		return -EINVAL;

	rknpu3_dev = session->rknpu3_dev;

	if (_IOC_TYPE(cmd) != RKNPU3_IOC_MAGIC)
		return -EINVAL;

	/* Power on before ioctl operation */
	ret = rknpu3_power_get(rknpu3_dev);
	if (ret) {
		LOG_DEV_ERROR(rknpu3_dev->dev, "failed to power on: %d\n", ret);
		return ret;
	}

	switch (cmd) {
	case IOCTL_RKNPU3_SUBMIT_TASK: {
		struct rknpu3_task_submit task_submit;

		if (copy_from_user(&task_submit, argp, sizeof(task_submit))) {
			ret = -EFAULT;
			break;
		}
		ret = rknpu3_tasks_submit(rknpu3_dev, &task_submit);
		if (ret == 0 && copy_to_user(argp, &task_submit, sizeof(task_submit)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_WAIT_TASK: {
		struct rknpu3_task_submit task_submit;

		if (copy_from_user(&task_submit, argp, sizeof(task_submit))) {
			ret = -EFAULT;
			break;
		}
		ret = rknpu3_tasks_wait(rknpu3_dev, &task_submit);
		if (ret == 0 && copy_to_user(argp, &task_submit, sizeof(task_submit)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_MEM_CREATE: {
		struct rknpu3_mem_create mem_create;

		if (copy_from_user(&mem_create, argp, sizeof(mem_create))) {
			ret = -EFAULT;
			break;
		}
		ret = rknpu3_memory_alloc(rknpu3_dev, &mem_create);
		if (ret == 0 && copy_to_user(argp, &mem_create, sizeof(mem_create)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_MEM_DESTROY: {
		struct rknpu3_mem_create mem_create;

		if (copy_from_user(&mem_create, argp, sizeof(mem_create))) {
			ret = -EFAULT;
			break;
		}
		ret = rknpu3_memory_free(rknpu3_dev, &mem_create);
		break;
	}
	case IOCTL_RKNPU3_GET_MEM_USAGE: {
		struct rknpu3_mem_usage usage;

		memset(&usage, 0, sizeof(usage));
		ret = rknpu3_memory_get_all_core_usage(rknpu3_dev, &usage);
		if (ret == 0 && copy_to_user(argp, &usage, sizeof(usage)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_RESET_CORE: {
		uint32_t core_id;

		if (copy_from_user(&core_id, argp, sizeof(core_id))) {
			ret = -EFAULT;
			break;
		}
		ret = rknpu3_core_reset(rknpu3_dev, core_id, 0);
		break;
	}
	case IOCTL_RKNPU3_GET_DRV_VERSION: {
		uint32_t version_code;

		ret = rknpu3_get_driver_version(&version_code);
		if (ret == 0 && copy_to_user(argp, &version_code, sizeof(version_code)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_GET_CORE_LOAD: {
		struct rknpu3_load load;

		memset(&load, 0, sizeof(load));
		ret = rknpu3_core_get_load(rknpu3_dev, &load);
		if (ret == 0 && copy_to_user(argp, &load, sizeof(load)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_MEM_SYNC: {
		struct rknpu3_mem_sync mem_sync;

		if (copy_from_user(&mem_sync, argp, sizeof(mem_sync))) {
			ret = -EFAULT;
			break;
		}
		ret = rknpu3_memory_sync(session, &mem_sync);
		break;
	}
	case IOCTL_RKNPU3_MEM_IMPORT: {
		struct rknpu3_mem_import mem_import;

		if (copy_from_user(&mem_import, argp, sizeof(mem_import))) {
			ret = -EFAULT;
			break;
		}
		ret = rknpu3_memory_import_dmabuf(session, &mem_import);
		if (ret == 0 && copy_to_user(argp, &mem_import, sizeof(mem_import)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_MEM_RELEASE: {
		uint64_t handle;

		if (copy_from_user(&handle, argp, sizeof(handle))) {
			ret = -EFAULT;
			break;
		}
		ret = rknpu3_memory_release_imported(session, handle);
		break;
	}
	case IOCTL_RKNPU3_GET_HW_VERSION: {
		uint64_t hw_version;

		ret = rknpu3_core_get_hw_version(rknpu3_dev, &hw_version);
		if (ret == 0 && copy_to_user(argp, &hw_version, sizeof(hw_version)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_GET_BW_INFO: {
		struct rknpu3_bw_info bw_info;

		memset(&bw_info, 0, sizeof(bw_info));
		ret = rknpu3_core_get_bw_info(rknpu3_dev, &bw_info);
		if (ret == 0 && copy_to_user(argp, &bw_info, sizeof(bw_info)))
			ret = -EFAULT;
		break;
	}
	case IOCTL_RKNPU3_GET_CYCLE_INFO: {
		struct rknpu3_cycle_info cycle_info;

		memset(&cycle_info, 0, sizeof(cycle_info));
		ret = rknpu3_core_get_cycle_info(rknpu3_dev, &cycle_info);
		if (ret == 0 && copy_to_user(argp, &cycle_info, sizeof(cycle_info)))
			ret = -EFAULT;
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	/* Power off with delay after ioctl operation */
	rknpu3_power_put_delay(rknpu3_dev);

	return ret;
}

static int rknpu3_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *mdev = filp->private_data;
	struct rknpu3_device *rknpu3_dev;
	struct rknpu3_session *session;

	if (!mdev)
		return -ENODEV;

	rknpu3_dev = container_of(mdev, struct rknpu3_device, miscdev);

	/* Allocate per-file session for resource tracking */
	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session) {
		LOG_DEV_ERROR(rknpu3_dev->dev, "Failed to allocate session\n");
		return -ENOMEM;
	}

	session->rknpu3_dev = rknpu3_dev;
	mutex_init(&session->imported_bufs_lock);
	INIT_LIST_HEAD(&session->imported_bufs);

	filp->private_data = session;
	return 0;
}

static int rknpu3_release(struct inode *inode, struct file *filp)
{
	struct rknpu3_session *session = filp->private_data;

	if (!session)
		return 0;

	/* Release all imported buffers that this session still holds */
	rknpu3_session_release_all_imports(session);

	mutex_destroy(&session->imported_bufs_lock);
	kfree(session);
	filp->private_data = NULL;
	return 0;
}

static const struct file_operations rknpu3_fops = {
	.owner = THIS_MODULE,
	.open = rknpu3_open,
	.release = rknpu3_release,
	.unlocked_ioctl = rknpu3_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = rknpu3_ioctl,
#endif
};

static int rknpu3_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rknpu3_device *rknpu3_dev;
	const struct of_device_id *match;
	const struct rknpu3_config *config;
	int ret = 0;

	LOG_DEV_INFO(dev, "RKNPU3 driver probing\n");

	/* Check device tree node */
	if (!pdev->dev.of_node) {
		LOG_DEV_ERROR(dev, "rknpu device-tree data is missing!\n");
		return -ENODEV;
	}

	/* Match device tree */
	match = of_match_device(rknpu3_of_match, dev);
	if (!match) {
		LOG_DEV_ERROR(dev, "rknpu device-tree entry is missing!\n");
		return -ENODEV;
	}

	/* Allocate device structure */
	rknpu3_dev = devm_kzalloc(dev, sizeof(*rknpu3_dev), GFP_KERNEL);
	if (!rknpu3_dev)
		return -ENOMEM;

	/* Get configuration */
	config = of_device_get_match_data(dev);
	if (!config) {
		LOG_DEV_ERROR(dev, "failed to get config data\n");
		return -EINVAL;
	}

	rknpu3_dev->config = config;
	rknpu3_dev->dev = dev;
	rknpu3_dev->num_cores = config->num_irqs;
	platform_set_drvdata(pdev, rknpu3_dev);

	/* Set DMA mask */
	ret = dma_set_mask_and_coherent(dev, config->dma_mask);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to set DMA mask: %d\n", ret);
		return ret;
	}

	/* Initialize locks */
	spin_lock_init(&rknpu3_dev->dev_lock);
	mutex_init(&rknpu3_dev->power_lock);

	/* Get clocks */
	rknpu3_dev->num_clks = devm_clk_bulk_get_all(dev, &rknpu3_dev->clks);
	if (rknpu3_dev->num_clks < 1) {
		LOG_DEV_WARN(dev, "no clocks found, continue without clocks\n");
		rknpu3_dev->num_clks = 0;
	} else {
		LOG_DEV_INFO(dev, "found %d clocks\n", rknpu3_dev->num_clks);
	}

#ifndef FPGA_PLATFORM
	/* Get power regulators */
	rknpu3_dev->vdd = devm_regulator_get_optional(dev, "rknpu");
	if (IS_ERR(rknpu3_dev->vdd)) {
		if (PTR_ERR(rknpu3_dev->vdd) != -ENODEV) {
			ret = PTR_ERR(rknpu3_dev->vdd);
			LOG_DEV_ERROR(dev, "failed to get vdd regulator: %d\n", ret);
			return ret;
		}
		rknpu3_dev->vdd = NULL;
		LOG_DEV_INFO(dev, "no vdd regulator found\n");
	}

	rknpu3_dev->mem = devm_regulator_get_optional(dev, "mem");
	if (IS_ERR(rknpu3_dev->mem)) {
		if (PTR_ERR(rknpu3_dev->mem) != -ENODEV) {
			ret = PTR_ERR(rknpu3_dev->mem);
			LOG_DEV_ERROR(dev, "failed to get mem regulator: %d\n", ret);
			return ret;
		}
		rknpu3_dev->mem = NULL;
		LOG_DEV_INFO(dev, "no mem regulator found\n");
	}
#endif

	/* Detect IOMMU device (but don't enable PM yet) */
	if (of_property_read_bool(dev->of_node, "iommus")) {
		struct device_node *iommu_np;
		struct platform_device *iommu_pdev;

		/* Check if IOMMU is really ready */
		if (!device_iommu_mapped(dev)) {
			LOG_DEV_INFO(dev, "IOMMU not ready, deferring probe\n");
			return -EPROBE_DEFER;
		}

		rknpu3_dev->iommu_en = true;
		LOG_DEV_INFO(dev, "IOMMU is enabled, using iommu mode\n");

		/* Get IOMMU device */
		iommu_np = of_parse_phandle(dev->of_node, "iommus", 0);
		if (iommu_np) {
			iommu_pdev = of_find_device_by_node(iommu_np);
			if (iommu_pdev)
				rknpu3_dev->iommu_dev = &iommu_pdev->dev;
			of_node_put(iommu_np);
		}
	} else {
		rknpu3_dev->iommu_en = false;
		rknpu3_dev->iommu_domain = NULL;
		LOG_DEV_INFO(dev, "IOMMU is disabled, using reserved memory\n");
	}

	/* Enable runtime PM for device and IOMMU */
	pm_runtime_enable(dev);
	if (rknpu3_dev->iommu_dev)
		pm_runtime_enable(rknpu3_dev->iommu_dev);

	/* Initialize power management workqueue (must be before power_on) */
	rknpu3_dev->power_put_delay = 30000;  /* Default 30 second delay */
	rknpu3_dev->power_off_wq =
		create_freezable_workqueue("rknpu3_power_off_wq");
	if (!rknpu3_dev->power_off_wq) {
		LOG_DEV_ERROR(dev, "failed to create power_off workqueue\n");
		ret = -ENOMEM;
		goto err_pm_disable;
	}
	INIT_DEFERRABLE_WORK(&rknpu3_dev->power_off_work,
			     rknpu3_power_off_delay_work);

	/* Power on using rknpu3_power_on() */
	ret = rknpu3_power_on(rknpu3_dev);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to power on: %d\n", ret);
		goto err_destroy_wq;
	}

	/* Initialize IOMMU domain after power on */
	if (rknpu3_dev->iommu_en) {
		struct iommu_domain *domain;

		/* Get IOMMU domain */
		domain = iommu_get_domain_for_dev(dev);
		if (!domain) {
			LOG_DEV_ERROR(dev,
				      "Failed to get IOMMU domain, deferring probe\n");
			ret = -EPROBE_DEFER;
			goto err_power_off;
		}

		rknpu3_dev->iommu_domain = domain;

		/* Register fault handler (for debugging) */
		iommu_set_fault_handler(domain, rknpu3_iommu_fault_handler, rknpu3_dev);
		LOG_DEV_INFO(dev, "IOMMU fault handler registered, type=%d\n",
			     domain->type);

		/* Print IOMMU domain address range */
		if (domain->geometry.aperture_end > 0) {
			LOG_DEV_INFO(dev, "IOMMU aperture: 0x%llx - 0x%llx\n",
				     domain->geometry.aperture_start,
				     domain->geometry.aperture_end);
		}

		/*
		 * Map NBUF physical address to same IOVA (identity mapping)
		 * NBUF is NPU internal buffer at fixed physical address
		 * Physical: 0x3ff00000 - 0x3ff60000 (384KB)
		 * IOVA:     0x3ff00000 - 0x3ff60000 (same)
		 */
		rknpu3_dev->nbuf_phyaddr = 0x3ff00000;
		rknpu3_dev->nbuf_size = 0x60000;  /* 384KB */
		rknpu3_dev->nbuf_mapped = false;

		ret = iommu_map(domain, rknpu3_dev->nbuf_phyaddr,
				rknpu3_dev->nbuf_phyaddr, rknpu3_dev->nbuf_size,
				IOMMU_READ | IOMMU_WRITE, GFP_KERNEL);
		if (ret) {
			LOG_DEV_ERROR(dev,
				      "Failed to map NBUF: phys=0x%llx size=0x%zx ret=%d\n",
				      (u64)rknpu3_dev->nbuf_phyaddr,
				      rknpu3_dev->nbuf_size, ret);
			/* Continue without NBUF mapping - not fatal */
		} else {
			rknpu3_dev->nbuf_mapped = true;
			LOG_DEV_INFO(dev,
				     "NBUF mapped: phys=0x%llx iova=0x%llx size=0x%zx\n",
				     (u64)rknpu3_dev->nbuf_phyaddr,
				     (u64)rknpu3_dev->nbuf_phyaddr,
				     rknpu3_dev->nbuf_size);
		}

		/* IOMMU mode doesn't use reserved memory to avoid address conflicts */
		rknpu3_dev->reserved_mem_attached = false;
		LOG_DEV_INFO(dev, "IOMMU mode: skip reserved memory binding\n");
	} else {
		/* Only init reserved memory in non-IOMMU mode */
		ret = of_reserved_mem_device_init(dev);
		if (ret) {
			if (ret != -ENODEV) {
				LOG_DEV_ERROR(dev,
					      "failed to init reserved memory: %d\n", ret);
				goto err_power_off;
			}
			LOG_DEV_INFO(dev, "no reserved memory configured\n");
		} else {
			rknpu3_dev->reserved_mem_attached = true;
			LOG_DEV_INFO(dev, "reserved memory attached\n");
		}
	}

	/* Initialize memory management module */
	ret = rknpu3_memory_module_init(rknpu3_dev);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to init memory module: %d\n", ret);
		goto err_reserved_mem_release;
	}

	/* Initialize core management module */
	ret = rknpu3_core_module_init(rknpu3_dev);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to init core module: %d\n", ret);
		goto err_memory_deinit;
	}

	/* Initialize task management module */
	ret = rknpu3_task_module_init(rknpu3_dev, (struct rknpu3_config *)config);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to init task module: %d\n", ret);
		goto err_core_deinit;
	}

	rknpu3_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	rknpu3_dev->miscdev.name = RKNPU3_DEVICE_NAME;
	rknpu3_dev->miscdev.fops = &rknpu3_fops;
	rknpu3_dev->miscdev.parent = dev;
	ret = misc_register(&rknpu3_dev->miscdev);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to register misc device: %d\n", ret);
		goto err_task_deinit;
	}

#ifndef FPGA_PLATFORM
	/* Initialize devfreq */
	ret = rknpu3_devfreq_init(rknpu3_dev);
	if (ret && ret != -EOPNOTSUPP)
		LOG_DEV_WARN(dev, "devfreq init failed: %d, continue without devfreq\n",
			     ret);
#endif

	/*
	 * Set power refcount to 1 (currently powered on from probe),
	 * then use delayed power off to safely turn off power after probe.
	 */
	atomic_set(&rknpu3_dev->power_refcount, 1);
	rknpu3_power_put_delay(rknpu3_dev);

	LOG_DEV_INFO(dev, "%s Driver v%s (%s) loaded successfully\n",
		     DRIVER_NAME,
		     RKNPU3_GET_DRV_VERSION_STRING(DRIVER_MAJOR, DRIVER_MINOR,
						  DRIVER_PATCHLEVEL),
		     DRIVER_DATE);

	return 0;

err_task_deinit:
	rknpu3_task_module_deinit(rknpu3_dev);
err_core_deinit:
	rknpu3_core_module_deinit(rknpu3_dev);
err_memory_deinit:
	rknpu3_memory_module_deinit(rknpu3_dev);
err_reserved_mem_release:
	if (rknpu3_dev->reserved_mem_attached)
		of_reserved_mem_device_release(dev);
	/* Unmap NBUF if it was mapped */
	if (rknpu3_dev->nbuf_mapped && rknpu3_dev->iommu_domain) {
		iommu_unmap(rknpu3_dev->iommu_domain,
			    rknpu3_dev->nbuf_phyaddr, rknpu3_dev->nbuf_size);
		rknpu3_dev->nbuf_mapped = false;
	}
err_power_off:
	rknpu3_power_off(rknpu3_dev);
err_destroy_wq:
	destroy_workqueue(rknpu3_dev->power_off_wq);
err_pm_disable:
	if (rknpu3_dev->iommu_dev) {
		pm_runtime_disable(rknpu3_dev->iommu_dev);
		put_device(rknpu3_dev->iommu_dev);
		rknpu3_dev->iommu_dev = NULL;
	}
	pm_runtime_disable(dev);
	return ret;
}

static void rknpu3_remove(struct platform_device *pdev)
{
	struct rknpu3_device *rknpu3_dev = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	if (!rknpu3_dev)
		return;

	LOG_DEV_INFO(dev, "RKNPU3 driver removing\n");

	/* Cancel pending power off work and destroy workqueue */
	cancel_delayed_work_sync(&rknpu3_dev->power_off_work);
	if (rknpu3_dev->power_off_wq)
		destroy_workqueue(rknpu3_dev->power_off_wq);

#ifndef FPGA_PLATFORM
	/* Remove devfreq */
	rknpu3_devfreq_remove(rknpu3_dev);
#endif

	misc_deregister(&rknpu3_dev->miscdev);

	/* Deinit task management module */
	rknpu3_task_module_deinit(rknpu3_dev);

	/* Deinit core management module */
	rknpu3_core_module_deinit(rknpu3_dev);

	/* Deinit memory management module */
	rknpu3_memory_module_deinit(rknpu3_dev);

	if (rknpu3_dev->reserved_mem_attached)
		of_reserved_mem_device_release(dev);

	/* Unmap NBUF if it was mapped */
	if (rknpu3_dev->nbuf_mapped && rknpu3_dev->iommu_domain) {
		iommu_unmap(rknpu3_dev->iommu_domain,
			    rknpu3_dev->nbuf_phyaddr, rknpu3_dev->nbuf_size);
		rknpu3_dev->nbuf_mapped = false;
		LOG_DEV_INFO(dev, "NBUF unmapped: iova=0x%llx size=0x%zx\n",
			     (u64)rknpu3_dev->nbuf_phyaddr, rknpu3_dev->nbuf_size);
	}

	/* Power off if still powered on */
	mutex_lock(&rknpu3_dev->power_lock);
	if (atomic_read(&rknpu3_dev->power_refcount) > 0) {
		atomic_set(&rknpu3_dev->power_refcount, 0);
		mutex_unlock(&rknpu3_dev->power_lock);
		rknpu3_power_off(rknpu3_dev);
	} else {
		mutex_unlock(&rknpu3_dev->power_lock);
	}

	/* Clean up IOMMU device */
	if (rknpu3_dev->iommu_dev) {
		pm_runtime_disable(rknpu3_dev->iommu_dev);
		put_device(rknpu3_dev->iommu_dev);
		rknpu3_dev->iommu_dev = NULL;
	}

	pm_runtime_disable(dev);

	LOG_DEV_INFO(dev, "RKNPU3 driver removed successfully\n");
}

MODULE_DEVICE_TABLE(of, rknpu3_of_match);

#ifndef FPGA_PLATFORM
#ifdef CONFIG_PM_SLEEP
static int rknpu3_suspend(struct device *dev)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);

	LOG_DEV_DEBUG(dev, "suspend\n");

	/* Ensure power is on before suspend */
	rknpu3_power_get(rknpu3_dev);

	return pm_runtime_force_suspend(dev);
}

static int rknpu3_resume(struct device *dev)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);
	int ret;

	LOG_DEV_DEBUG(dev, "resume\n");

	ret = pm_runtime_force_resume(dev);

	/* Release power with delay after resume */
	rknpu3_power_put_delay(rknpu3_dev);

	return ret;
}
#endif

static int rknpu3_runtime_suspend(struct device *dev)
{
	return rknpu3_devfreq_runtime_suspend(dev);
}

static int rknpu3_runtime_resume(struct device *dev)
{
	return rknpu3_devfreq_runtime_resume(dev);
}

static const struct dev_pm_ops rknpu3_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	SET_SYSTEM_SLEEP_PM_OPS(rknpu3_suspend, rknpu3_resume)
#endif
	SET_RUNTIME_PM_OPS(rknpu3_runtime_suspend, rknpu3_runtime_resume, NULL)
};
#endif

static struct platform_driver rknpu3_driver = {
	.probe = rknpu3_probe,
	.remove = rknpu3_remove,
	.driver = {
		.name = "RKNPU3",
		.of_match_table = rknpu3_of_match,
#ifndef FPGA_PLATFORM
		.pm = &rknpu3_pm_ops,
#endif
	},
};

static int rknpu3_init(void)
{
	return platform_driver_register(&rknpu3_driver);
}

static void rknpu3_exit(void)
{
	platform_driver_unregister(&rknpu3_driver);
}

late_initcall(rknpu3_init);
module_exit(rknpu3_exit);

MODULE_DESCRIPTION("Rockchip RKNPU3 Driver");
MODULE_AUTHOR("Chifred <chifred@rock-chips.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(RKNPU3_GET_DRV_VERSION_STRING(DRIVER_MAJOR, DRIVER_MINOR,
					    DRIVER_PATCHLEVEL));
