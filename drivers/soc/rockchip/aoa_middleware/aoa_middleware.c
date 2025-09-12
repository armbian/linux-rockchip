// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rockchip AOA Middleware Driver
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/nospec.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "aoa_middleware.h"
#include "aoa_mmap.h"
#include "aoa_drv.h"
#include "lp_rkdma.h"

#define NOTIFY_RKDMA_SET_PERIODS        _IOR('N', 1, u32)
#define NOTIFY_RKDMA_GET_TIMESTAMP_NS   _IOR('N', 2, s64)

struct notify_ns {
	s32 ns_id;
	s64 ns;
};

struct notify_rkdma {
	s64 *ns_tbl;
	s32 last_ns_id;
	u32 periods;
};

struct aoa_middleware_devs {
	struct device *dev;
	struct platform_device *pdev_aoa;
	struct platform_device *pdev_dma;
	void *am_d;
	struct notify_rkdma *nty_rkdma;
	struct notify_ns *nty_ns;
	struct miscdevice misc_notifier_aoa;
	struct miscdevice misc_notifier_dma;
	struct fasync_struct *rk_aoa_fasync_queue;
	struct fasync_struct *rk_dma_fasync_queue;
};

int aoa_middleware_aoa_notifier(s32 status, void *data)
{
	struct aoa_middleware_devs *amw_d = data;

	if (!amw_d) {
		pr_err("%s: amw_d pointer is null\n", __func__);
		return -EINVAL;
	}

	/* AOA Notify starting from SIGRTMIN + 1 */
	kill_fasync(&amw_d->rk_aoa_fasync_queue, SIGRTMIN + status, POLL_IN);
	return 0;
}
EXPORT_SYMBOL(aoa_middleware_aoa_notifier);

int aoa_middleware_dma_notifier(s32 dma_count, void *data)
{
	struct aoa_middleware_devs *amw_d = data;
	struct notify_rkdma *n_rkdma;
	struct notify_ns *n_ns;
	struct timespec64 ts;
	s32 delta_id;

	if (!amw_d) {
		pr_err("%s: amw_d pointer is null\n", __func__);
		return -EINVAL;
	}
	if (!amw_d->nty_rkdma) {
		pr_err("%s: nty_rkdma pointer is null\n", __func__);
		return -EINVAL;
	}
	if (!amw_d->nty_ns) {
		pr_err("%s: nty_ns pointer is null\n", __func__);
		return -EINVAL;
	}

	n_rkdma = amw_d->nty_rkdma;
	n_ns = amw_d->nty_ns;

	ktime_get_boottime_ts64(&ts);
	n_ns->ns = timespec64_to_ns(&ts);
	kill_fasync(&amw_d->rk_dma_fasync_queue, SIGRTMIN + 0, POLL_IN);

	/* ns_id: start from 1, range: 0 ~ (periods-1) */
	dma_count = array_index_nospec(dma_count % n_rkdma->periods, n_rkdma->periods);
	n_ns->ns_id = dma_count;
	n_rkdma->ns_tbl[n_ns->ns_id] = n_ns->ns;
	if (n_ns->ns_id < n_rkdma->last_ns_id)
		delta_id = n_ns->ns_id + n_rkdma->periods - n_rkdma->last_ns_id;
	else
		delta_id = n_ns->ns_id - n_rkdma->last_ns_id;
	if (delta_id > 1) {
		s32 missed_id = 0;

		while (missed_id <= delta_id) {
			/**
			 * During sleep, the CPU does not respond to DMA interruptsand
			 * requires manual calibration of PTS.
			 */
			s32 id = (n_ns->ns_id - missed_id);

			if (id < 0)
				id += n_rkdma->periods;
			n_rkdma->ns_tbl[id] = n_ns->ns - missed_id * 16000000;
			missed_id++;
		}
	}
	n_rkdma->last_ns_id = n_ns->ns_id;
	return 0;
}
EXPORT_SYMBOL(aoa_middleware_dma_notifier);

static int rk_aoa_notifier_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct aoa_middleware_devs *amw_d = container_of(misc, struct aoa_middleware_devs, misc_notifier_aoa);

	file->private_data = amw_d;
	return 0;
}

static int rk_aoa_notifier_fasync(int fd, struct file *file, int mode)
{
	struct aoa_middleware_devs *amw_d = file->private_data;

	return fasync_helper(fd, file, mode, &amw_d->rk_aoa_fasync_queue);
}

static const struct file_operations rk_aoa_notifier_fops = {
	.owner   = THIS_MODULE,
	.open    = rk_aoa_notifier_open,
	.fasync  = rk_aoa_notifier_fasync,
};

static int rk_dma_notifier_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct aoa_middleware_devs *amw_d = container_of(misc, struct aoa_middleware_devs, misc_notifier_dma);

	file->private_data = amw_d;
	return 0;
}

static int rk_dma_notifier_fasync(int fd, struct file *file, int mode)
{
	struct aoa_middleware_devs *amw_d = file->private_data;

	return fasync_helper(fd, file, mode, &amw_d->rk_dma_fasync_queue);
}

static long rk_dma_notifier_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct aoa_middleware_devs *amw_d = file->private_data;
	struct notify_rkdma *n_rkdma;
	struct notify_ns n_ns_user;

	if (IS_ERR_OR_NULL(amw_d))
		return -EINVAL;
	if (IS_ERR_OR_NULL(amw_d->nty_rkdma))
		return -EINVAL;
	n_rkdma = amw_d->nty_rkdma;

	switch (cmd) {
	case NOTIFY_RKDMA_SET_PERIODS:
		n_rkdma->periods = arg;
		kfree(n_rkdma->ns_tbl);
		n_rkdma->last_ns_id = 0; /* last_ns_id start from 0, current ns_id start from 1 */
		n_rkdma->ns_tbl = kcalloc(n_rkdma->periods, sizeof(s64), GFP_KERNEL);
		pr_debug("rk_dma_notifier: set and alloc ns table periods: %d\n", n_rkdma->periods);
		break;
	case NOTIFY_RKDMA_GET_TIMESTAMP_NS:
		/* return last timestamp from ns */
		if (copy_from_user(&n_ns_user, (struct notify_ns __user *)arg, sizeof(n_ns_user)))
			return -EFAULT;
		if (n_ns_user.ns_id < 0 || n_ns_user.ns_id >= n_rkdma->periods) {
			pr_err("Invalid ns_id: %d\n", n_ns_user.ns_id);
			return -EINVAL;
		}
		n_ns_user.ns_id = array_index_nospec(n_ns_user.ns_id, n_rkdma->periods);
		n_ns_user.ns = n_rkdma->ns_tbl[n_ns_user.ns_id];
		if (copy_to_user((struct notify_ns __user *)arg, &n_ns_user, sizeof(n_ns_user)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations rk_dma_notifier_fops = {
	.owner   = THIS_MODULE,
	.open    = rk_dma_notifier_open,
	.fasync  = rk_dma_notifier_fasync,
	.compat_ioctl  = rk_dma_notifier_ioctl,
	.unlocked_ioctl = rk_dma_notifier_ioctl,
};

static int aoa_middleware_probe(struct platform_device *pdev)
{
	struct aoa_middleware_devs *amw_d;
	struct platform_device *pdev_slave = NULL;
	struct device_node *np = NULL;
	void *am_map = NULL;
	int ret = 0;

	amw_d = devm_kzalloc(&pdev->dev, sizeof(*amw_d), GFP_KERNEL);
	if (!amw_d)
		return -ENOMEM;
	amw_d->nty_rkdma = devm_kzalloc(&pdev->dev, sizeof(*amw_d->nty_rkdma), GFP_KERNEL);
	if (!amw_d->nty_rkdma)
		return -ENOMEM;
	amw_d->nty_ns = devm_kzalloc(&pdev->dev, sizeof(*amw_d->nty_ns), GFP_KERNEL);
	if (!amw_d->nty_ns)
		return -ENOMEM;
	amw_d->dev = &pdev->dev;
	amw_d->pdev_aoa = NULL;
	amw_d->pdev_dma = NULL;
	amw_d->am_d = NULL;

	/* prepare rockchip aoa control driver */
	np = of_parse_phandle(pdev->dev.of_node, "rockchip,aoa", 0);
	if (!np || !of_device_is_available(np)) {
		dev_err(&pdev->dev, "can't find 'rockchip,aoa' node\n");
		ret = -ENODEV;
		goto err_out;
	}

	pdev_slave = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev_slave) {
		dev_err(&pdev->dev, "get aoa node failed\n");
		ret = -ENODEV;
		goto err_out;
	}

	ret = rockchip_aoa_probe(pdev_slave, (void *)amw_d);
	if (ret) {
		dev_err(&pdev->dev, "probe rockchip aoa failed: %d\n", ret);
		goto err_put_aoa;
	}
	amw_d->pdev_aoa = pdev_slave;
	pdev_slave = NULL;

	/* prepare rockchip low-power dma driver */
	np = of_parse_phandle(pdev->dev.of_node, "rockchip,dma", 0);
	if (!np || !of_device_is_available(np)) {
		dev_err(&pdev->dev, "can't find 'rockchip,dma' node\n");
		ret = -ENODEV;
		goto err_unprobe_aoa;
	}

	pdev_slave = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev_slave) {
		dev_err(&pdev->dev, "get dma node failed\n");
		ret = -ENODEV;
		goto err_unprobe_aoa;
	}

	ret = lp_rkdma_probe(pdev_slave, (void *)amw_d);
	if (ret) {
		dev_err(&pdev->dev, "probe rockchip dma failed: %d\n", ret);
		goto err_put_dma;
	}
	amw_d->pdev_dma = pdev_slave;
	pdev_slave = NULL;

	am_map = aoa_mmap_probe(pdev);
	if (IS_ERR_OR_NULL(am_map)) {
		if (IS_ERR(am_map))
			ret = PTR_ERR(am_map);
		else
			ret = -EINVAL;
		dev_err(&pdev->dev, "%s: aoa mmap probe failed (%d)\n", __func__, ret);
		goto err_unprobe_dma;
	}
	amw_d->am_d = am_map;

	/* prepare aoa/dma notifiers */
	amw_d->misc_notifier_aoa.minor = MISC_DYNAMIC_MINOR;
	amw_d->misc_notifier_aoa.name  = "rk-aoa-notifier";
	amw_d->misc_notifier_aoa.fops  = &rk_aoa_notifier_fops;
	ret = misc_register(&amw_d->misc_notifier_aoa);
	if (ret) {
		dev_err(&pdev->dev, "%s: aoa notifier misc register failed (%d)\n", __func__, ret);
		goto err_mmap_remove;
	}

	amw_d->misc_notifier_dma.minor = MISC_DYNAMIC_MINOR;
	amw_d->misc_notifier_dma.name  = "rk-dma-notifier";
	amw_d->misc_notifier_dma.fops  = &rk_dma_notifier_fops;
	ret = misc_register(&amw_d->misc_notifier_dma);
	if (ret) {
		dev_err(&pdev->dev, "%s: dma notifier misc register failed (%d)\n", __func__, ret);
		goto err_unregister_aoa_misc;
	}

	platform_set_drvdata(pdev, amw_d);
	dev_info(&pdev->dev, "%s: all aoa middlewares are registered\n", __func__);
	return 0;

err_unregister_aoa_misc:
	misc_deregister(&amw_d->misc_notifier_aoa);
err_mmap_remove:
	if (amw_d->am_d) {
		aoa_mmap_remove(pdev, amw_d->am_d);
		amw_d->am_d = NULL;
	}
err_unprobe_dma:
	if (amw_d->pdev_dma)
		lp_rkdma_remove(amw_d->pdev_dma);
err_put_dma:
	if (amw_d->pdev_dma) {
		platform_device_put(amw_d->pdev_dma);
		amw_d->pdev_dma = NULL;
	}
err_unprobe_aoa:
	if (amw_d->pdev_aoa)
		rockchip_aoa_remove(amw_d->pdev_aoa);
err_put_aoa:
	if (amw_d->pdev_aoa) {
		platform_device_put(amw_d->pdev_aoa);
		amw_d->pdev_aoa = NULL;
	}
err_out:
	if (pdev_slave)
		platform_device_put(pdev_slave);
	return ret;
}

static int aoa_middleware_remove(struct platform_device *pdev)
{
	struct aoa_middleware_devs *amw_d = platform_get_drvdata(pdev);

	if (IS_ERR_OR_NULL(amw_d))
		return -EINVAL;

	if (amw_d->pdev_aoa) {
		rockchip_aoa_remove(amw_d->pdev_aoa);
		platform_device_put(amw_d->pdev_aoa);
		amw_d->pdev_aoa = NULL;
	}

	if (amw_d->pdev_dma) {
		lp_rkdma_remove(amw_d->pdev_dma);
		platform_device_put(amw_d->pdev_dma);
		amw_d->pdev_dma = NULL;
	}

	aoa_mmap_remove(pdev, amw_d->am_d);

	misc_deregister(&amw_d->misc_notifier_aoa);
	misc_deregister(&amw_d->misc_notifier_dma);

	dev_info(&pdev->dev, "%s: all aoa middlewares are unregistered\n", __func__);
	return 0;
}

static const struct of_device_id aoa_middleware_of_match[] = {
	{ .compatible = "rockchip,aoa-middleware", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, aoa_middleware_of_match);

static struct platform_driver aoa_middleware_driver = {
	.probe  = aoa_middleware_probe,
	.remove = aoa_middleware_remove,
	.driver = {
		.name           = "aoa-middleware",
		.of_match_table = aoa_middleware_of_match,
	},
};

module_platform_driver(aoa_middleware_driver);

MODULE_DESCRIPTION("Rockchip AOA Middleware Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xing.zheng@rock-chips.com");
