// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>
#include <linux/nospec.h>
#include "regs.h"
#include "hw.h"
#include "aiisp.h"
#include "version.h"
#include "procfs.h"

#define RKAIISP_VERNO_LEN		10

int rkaiisp_debug;
module_param_named(debug, rkaiisp_debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

int rkaiisp_showreg;
module_param_named(showreg, rkaiisp_showreg, int, 0644);
MODULE_PARM_DESC(showreg, "show register (0-1)");

int rkaiisp_stdfps = 30;
module_param_named(standardfps, rkaiisp_stdfps, int, 0644);
MODULE_PARM_DESC(standardfps, "standard fps");

static char rkaiisp_version[RKAIISP_VERNO_LEN];
module_param_string(version, rkaiisp_version, RKAIISP_VERNO_LEN, 0444);
MODULE_PARM_DESC(version, "version number");

/***************************** platform deive *******************************/
static int rkaiisp_attach_hw(struct rkaiisp_device *aidev)
{
	struct device_node *np;
	struct platform_device *pdev;
	struct rkaiisp_hw_dev *hw_dev;
	const char *name;
	int ret, dev_id = -1;

	of_property_read_string(aidev->dev->of_node, "name", &name);
	if (name) {
		ret = sscanf(name, "rkaiisp-vir%d", &dev_id);
		if ((dev_id >= RKAIISP_DEV_MAX) || ret != 1) {
			dev_err(aidev->dev, "dev_id %d, failed attach aidev hw, max dev:%d\n",
				dev_id, RKAIISP_DEV_MAX);
			return -EINVAL;
		}
	}

	np = of_parse_phandle(aidev->dev->of_node, "rockchip,hw", 0);
	if (!np || !of_device_is_available(np)) {
		dev_err(aidev->dev, "failed to get isp hw node\n");
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		dev_err(aidev->dev, "failed to get aidev hw from node\n");
		return -ENODEV;
	}

	hw_dev = platform_get_drvdata(pdev);
	if (!hw_dev) {
		dev_err(aidev->dev, "failed attach aidev hw\n");
		return -EINVAL;
	}

	mutex_lock(&hw_dev->dev_mutex);
	if (hw_dev->dev_num >= RKAIISP_DEV_MAX) {
		dev_err(aidev->dev, "failed attach aidev hw, max dev:%d\n", RKAIISP_DEV_MAX);
		mutex_unlock(&hw_dev->dev_mutex);
		return -EINVAL;
	}

	if (dev_id == -1)
		dev_id = hw_dev->dev_num;

	if (dev_id >= 0 && dev_id < RKAIISP_DEV_MAX) {
		dev_id = array_index_nospec(dev_id, RKAIISP_DEV_MAX);
		dev_info(aidev->dev, "dev_id %d\n", dev_id);
		aidev->dev_id = dev_id;
		hw_dev->aidev[dev_id] = aidev;
		hw_dev->dev_num++;
		aidev->hw_dev = hw_dev;
		aidev->is_hw_link = true;
	}

	hw_dev->is_single = (hw_dev->dev_num > 1) ? false : true;
	mutex_unlock(&hw_dev->dev_mutex);

	return 0;
}

static int rkaiisp_plat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct v4l2_device *v4l2_dev;
	struct rkaiisp_device *aidev;
	int ret;

	snprintf(rkaiisp_version, sizeof(rkaiisp_version),
		 "v%02x.%02x.%02x",
		 RKAIISP_DRIVER_VERSION >> 16,
		 (RKAIISP_DRIVER_VERSION & 0xff00) >> 8,
		 RKAIISP_DRIVER_VERSION & 0x00ff);

	dev_info(dev, "rkaiisp driver version: %s\n", rkaiisp_version);

	aidev = devm_kzalloc(dev, sizeof(*aidev), GFP_KERNEL);
	if (!aidev)
		return -ENOMEM;

	dev_set_drvdata(dev, aidev);
	aidev->dev = dev;
	ret = rkaiisp_attach_hw(aidev);
	if (ret)
		return ret;

	aidev->sw_base_addr = devm_kzalloc(dev, RKAIISP_SW_MAX_SIZE, GFP_KERNEL);
	if (!aidev->sw_base_addr)
		return -ENOMEM;

	snprintf(aidev->media_dev.model, sizeof(aidev->media_dev.model),
		 "%s%d", DRIVER_NAME, aidev->dev_id);
	strscpy(aidev->name, dev_name(dev), sizeof(aidev->name));
	strscpy(aidev->media_dev.driver_name, aidev->name,
		sizeof(aidev->media_dev.driver_name));

	mutex_init(&aidev->apilock);
	aidev->media_dev.dev = dev;

	v4l2_dev = &aidev->v4l2_dev;
	v4l2_dev->mdev = &aidev->media_dev;
	strscpy(v4l2_dev->name, aidev->name, sizeof(v4l2_dev->name));

	ret = v4l2_device_register(aidev->dev, &aidev->v4l2_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "Failed to register v4l2 device:%d\n", ret);
		return ret;
	}

	media_device_init(&aidev->media_dev);
	ret = media_device_register(&aidev->media_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "Failed to register media device:%d\n", ret);
		goto err_unreg_v4l2_dev;
	}

	pm_runtime_enable(dev);
	ret = rkaiisp_register_vdev(aidev, &aidev->v4l2_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "Failed to register platform subdevs:%d\n", ret);
		goto err_unreg_media_dev;
	}

	rkaiisp_proc_init(aidev);

	v4l2_info(v4l2_dev, "probe end.\n");
	return 0;

err_unreg_media_dev:
	media_device_unregister(&aidev->media_dev);
err_unreg_v4l2_dev:
	v4l2_device_unregister(&aidev->v4l2_dev);
	return ret;
}

static int rkaiisp_plat_remove(struct platform_device *pdev)
{
	struct rkaiisp_device *aidev = platform_get_drvdata(pdev);

	aidev->is_hw_link = false;
	aidev->hw_dev->aidev[aidev->dev_id] = NULL;

	pm_runtime_disable(&pdev->dev);

	rkaiisp_proc_cleanup(aidev);
	media_device_unregister(&aidev->media_dev);
	v4l2_device_unregister(&aidev->v4l2_dev);
	rkaiisp_unregister_vdev(aidev);
	media_device_cleanup(&aidev->media_dev);
	return 0;
}

static int __maybe_unused rkaiisp_runtime_suspend(struct device *dev)
{
	struct rkaiisp_device *aidev = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&aidev->hw_dev->dev_mutex);
	ret = pm_runtime_put_sync(aidev->hw_dev->dev);
	mutex_unlock(&aidev->hw_dev->dev_mutex);
	return (ret > 0) ? 0 : ret;
}

static int __maybe_unused rkaiisp_runtime_resume(struct device *dev)
{
	struct rkaiisp_device *aidev = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&aidev->hw_dev->dev_mutex);
	ret = pm_runtime_get_sync(aidev->hw_dev->dev);
	mutex_unlock(&aidev->hw_dev->dev_mutex);
	return (ret > 0) ? 0 : ret;
}

static const struct dev_pm_ops rkaiisp_plat_pm_ops = {
	SET_RUNTIME_PM_OPS(rkaiisp_runtime_suspend, rkaiisp_runtime_resume, NULL)
};

static const struct of_device_id rkaiisp_plat_of_match[] = {
	{
		.compatible = "rockchip,rkaiisp-vir",
	},
	{},
};

struct platform_driver rkaiisp_plat_drv = {
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = of_match_ptr(rkaiisp_plat_of_match),
		   .pm = &rkaiisp_plat_pm_ops,
	},
	.probe = rkaiisp_plat_probe,
	.remove = rkaiisp_plat_remove,
};

MODULE_AUTHOR("Rockchip Camera/ISP team");
MODULE_DESCRIPTION("Rockchip ISP platform driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
