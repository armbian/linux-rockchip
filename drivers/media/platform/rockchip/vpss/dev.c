// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2023 Rockchip Electronics Co., Ltd. */

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs_v1.h"

#include "version.h"

#define RKVPSS_VERNO_LEN 10

int rkvpss_debug;
module_param_named(debug, rkvpss_debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-6)");

static bool rkvpss_clk_dbg;
module_param_named(clk_dbg, rkvpss_clk_dbg, bool, 0644);
MODULE_PARM_DESC(clk_dbg, "rkvpss clk set by user");

static char rkvpss_version[RKVPSS_VERNO_LEN];
module_param_string(version, rkvpss_version, RKVPSS_VERNO_LEN, 0444);
MODULE_PARM_DESC(version, "version number");

static unsigned int rkvpss_wrap_line;
module_param_named(wrap_line, rkvpss_wrap_line, uint, 0644);
MODULE_PARM_DESC(wrap_line, "rkvpss wrap line");

char rkvpss_regfile[RKVPSS_REGFILE_LEN];
module_param_string(reg_file, rkvpss_regfile, RKVPSS_REGFILE_LEN, 0644);
MODULE_PARM_DESC(reg_file, "dump reg file");

int rkvpss_cfginfo_num = 5;

static int rkvpss_get_cfginfo_num(const char *val, const struct kernel_param *kp)
{
	int num, ret;

	ret = kstrtoint(val, 10, &num);
	if (ret)
		return ret;
	if (num < 0 || num > 50) {
		pr_info("rkvpss_cfginfo_num must be range of 0 to 50");
		return -EINVAL;
	}

	return param_set_int(val, kp);
}

static const struct kernel_param_ops cfginfo_num_ops = {
	.set = rkvpss_get_cfginfo_num,
	.get = param_get_int,
};
module_param_cb(cfginfo_num, &cfginfo_num_ops, &rkvpss_cfginfo_num, 0644);
MODULE_PARM_DESC(cfginfo_num, "rkvpss offline cfginfo number");

void rkvpss_set_clk_rate(struct clk *clk, unsigned long rate)
{
	if (rkvpss_clk_dbg)
		return;

	clk_set_rate(clk, rate);
}

void rkvpss_pipeline_default_fmt(struct rkvpss_device *dev)
{
	u32 i, w, h;

	w = dev->vpss_sdev.out_fmt.width;
	h = dev->vpss_sdev.out_fmt.height;
	for (i = 0; i < vpss_outchn_max(dev->hw_dev->vpss_ver); i++)
		rkvpss_stream_default_fmt(dev, i, w, h, V4L2_PIX_FMT_NV12);
}

int rkvpss_pipeline_open(struct rkvpss_device *dev)
{
	if (atomic_inc_return(&dev->pipe_power_cnt) > 1)
		return 0;

	return 0;
}

int rkvpss_pipeline_close(struct rkvpss_device *dev)
{
	if (atomic_dec_return(&dev->pipe_power_cnt))
		return 0;
	return 0;
}

int rkvpss_pipeline_stream(struct rkvpss_device *dev, bool on)
{
	int ret;

	if ((on && atomic_inc_return(&dev->pipe_stream_cnt) > 1) ||
	    (!on && atomic_dec_return(&dev->pipe_stream_cnt) > 0))
		return 0;

	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s on:%d\n", __func__, on);

	if (on) {
		ret = v4l2_subdev_call(&dev->vpss_sdev.sd, video, s_stream, true);
		if (ret < 0)
			goto err;
		ret = v4l2_subdev_call(dev->remote_sd, video, s_stream, true);
		if (ret < 0) {
			v4l2_subdev_call(&dev->vpss_sdev.sd, video, s_stream, false);
			goto err;
		}
		dev->stopping = false;
	} else {
		dev->stopping = true;
		ret = wait_event_timeout(dev->stop_done, !dev->stopping, msecs_to_jiffies(300));
		if (!ret)
			v4l2_warn(&dev->v4l2_dev, "%s timeout\n", __func__);
		v4l2_subdev_call(dev->remote_sd, video, s_stream, false);
		v4l2_subdev_call(&dev->vpss_sdev.sd, video, s_stream, false);
	}

	return 0;
err:
	atomic_dec(&dev->pipe_stream_cnt);
	v4l2_err(&dev->v4l2_dev, "%s on:%d failed:%d\n", __func__, on, ret);
	return ret;
}

static int rkvpss_create_links(struct rkvpss_device *dev)
{
	struct rkvpss_stream_vdev *stream_vdev;
	struct media_entity *source, *sink;
	struct rkvpss_stream *stream;
	unsigned int flags = 0;
	int ret, i;

	if (!dev->remote_sd)
		return -EINVAL;

	/* input links */
	sink = &dev->vpss_sdev.sd.entity;
	ret = media_create_pad_link(&dev->remote_sd->entity, 0,
			sink, RKVPSS_PAD_SINK, MEDIA_LNK_FL_ENABLED);
	if (ret < 0)
		goto end;
	dev->inp = INP_ISP;

	stream_vdev = &dev->stream_vdev;
	/* output stream links */
	flags = MEDIA_LNK_FL_ENABLED;
	source = &dev->vpss_sdev.sd.entity;

	for (i = 0; i < vpss_outchn_max(dev->hw_dev->vpss_ver); i++) {
		stream = &stream_vdev->stream[i];
		stream->linked = flags;
		sink = &stream->vnode.vdev.entity;
		ret = media_create_pad_link(source, RKVPSS_PAD_SOURCE, sink, 0, flags);
		if (ret < 0)
			goto end;
	}

end:
	return ret;
}

static int
rkvpss_subdev_notifier_complete(struct v4l2_async_notifier *notifier)
{
	struct rkvpss_device *dev;
	int ret;

	dev = container_of(notifier, struct rkvpss_device, notifier);

	ret = rkvpss_create_links(dev);
	if (ret < 0)
		goto err;
	ret = v4l2_device_register_subdev_nodes(&dev->v4l2_dev);
	if (ret < 0)
		goto err;
	rkvpss_pipeline_default_fmt(dev);
	v4l2_info(&dev->v4l2_dev, "Async subdev notifier completed\n");
err:
	return ret;
}

static int rkvpss_subdev_notifier_bound(struct v4l2_async_notifier *notifier,
					struct v4l2_subdev *subdev,
					struct v4l2_async_subdev *asd)
{
	struct rkvpss_device *dev;

	dev = container_of(notifier, struct rkvpss_device, notifier);
	dev->remote_sd = subdev;
	v4l2_set_subdev_hostdata(subdev, &dev->vpss_sdev.sd);
	return 0;
}

static void rkvpss_subdev_notifier_unbind(struct v4l2_async_notifier *notifier,
					  struct v4l2_subdev *subdev,
					  struct v4l2_async_subdev *asd)
{
	struct rkvpss_device *dev;

	dev = container_of(notifier, struct rkvpss_device, notifier);
	if (dev->remote_sd) {
		v4l2_set_subdev_hostdata(dev->remote_sd, NULL);
		dev->remote_sd = NULL;
	}
}

static const struct
v4l2_async_notifier_operations rkvpss_subdev_notifier_ops = {
	.bound = rkvpss_subdev_notifier_bound,
	.unbind = rkvpss_subdev_notifier_unbind,
	.complete = rkvpss_subdev_notifier_complete,
};

static int rkvpss_subdev_notifier(struct rkvpss_device *dev)
{
	struct v4l2_async_notifier *ntf = &dev->notifier;
	int ret;

	v4l2_async_nf_init(ntf);
	ret = v4l2_async_nf_parse_fwnode_endpoints(dev->dev,
		ntf, sizeof(struct v4l2_async_subdev), NULL);
	if (ret < 0)
		return ret;
	ntf->ops = &rkvpss_subdev_notifier_ops;
	return v4l2_async_nf_register(&dev->v4l2_dev, ntf);
}

static int rkvpss_register_platform_subdevs(struct rkvpss_device *dev)
{
	int ret;

	ret = rkvpss_register_subdev(dev, &dev->v4l2_dev);
	if (ret < 0)
		return ret;

	ret = rkvpss_register_stream_vdevs(dev);
	if (ret < 0)
		goto err_unreg_vpss_subdev;

	ret = rkvpss_subdev_notifier(dev);
	if (ret < 0)
		goto err_unreg_stream_vdevs;

	return ret;
err_unreg_stream_vdevs:
	rkvpss_unregister_stream_vdevs(dev);
err_unreg_vpss_subdev:
	rkvpss_unregister_subdev(dev);
	return ret;
}

static const struct of_device_id rkvpss_plat_of_match[] = {
	{
		.compatible = "rockchip,rkvpss-vir",
	},
	{},
};

static int rkvpss_plat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct v4l2_device *v4l2_dev;
	struct rkvpss_device *vpss_dev;
	int ret, mult = 2;

	sprintf(rkvpss_version, "v%02x.%02x.%02x",
		RKVPSS_DRIVER_VERSION >> 16,
		(RKVPSS_DRIVER_VERSION & 0xff00) >> 8,
		RKVPSS_DRIVER_VERSION & 0x00ff);

	dev_info(dev, "rkvpss driver version: %s\n", rkvpss_version);

	vpss_dev = devm_kzalloc(dev, sizeof(*vpss_dev), GFP_KERNEL);
	if (!vpss_dev)
		return -ENOMEM;
	vpss_dev->sw_base_addr = devm_kzalloc(dev, RKVPSS_SW_REG_SIZE_MAX * mult, GFP_KERNEL);
	if (!vpss_dev->sw_base_addr)
		return -ENOMEM;

	dev_set_drvdata(dev, vpss_dev);
	vpss_dev->dev = dev;

	ret = rkvpss_attach_hw(vpss_dev);
	if (ret)
		return ret;

	mutex_init(&vpss_dev->apilock);

	strscpy(vpss_dev->name, dev_name(dev), sizeof(vpss_dev->name));
	snprintf(vpss_dev->media_dev.model, sizeof(vpss_dev->media_dev.model),
		 "%s%d", DRIVER_NAME, vpss_dev->dev_id);
	strscpy(vpss_dev->media_dev.driver_name, vpss_dev->name,
		sizeof(vpss_dev->media_dev.driver_name));
	vpss_dev->media_dev.dev = &pdev->dev;
	v4l2_dev = &vpss_dev->v4l2_dev;
	v4l2_dev->mdev = &vpss_dev->media_dev;
	strscpy(v4l2_dev->name, vpss_dev->name, sizeof(v4l2_dev->name));
	v4l2_ctrl_handler_init(&vpss_dev->ctrl_handler, 5);
	v4l2_dev->ctrl_handler = &vpss_dev->ctrl_handler;

	ret = v4l2_device_register(vpss_dev->dev, v4l2_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "register v4l2 device failed:%d\n", ret);
		return ret;
	}
	media_device_init(&vpss_dev->media_dev);
	ret = media_device_register(&vpss_dev->media_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "register media device failed:%d\n", ret);
		goto err_unreg_v4l2_dev;
	}

	ret = rkvpss_register_platform_subdevs(vpss_dev);
	if (ret < 0)
		goto err_unreg_media_dev;

	atomic_set(&vpss_dev->pipe_power_cnt, 0);
	atomic_set(&vpss_dev->pipe_stream_cnt, 0);
	rkvpss_proc_init(vpss_dev);
	pm_runtime_enable(&pdev->dev);
	vpss_dev->is_probe_end = true;
	init_waitqueue_head(&vpss_dev->stop_done);
	vpss_dev->is_suspend = false;
	vpss_dev->is_idle = true;
	return 0;

err_unreg_media_dev:
	media_device_unregister(&vpss_dev->media_dev);
err_unreg_v4l2_dev:
	v4l2_device_unregister(&vpss_dev->v4l2_dev);
	return ret;
}

static int rkvpss_plat_remove(struct platform_device *pdev)
{
	struct rkvpss_device *vpss_dev = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);

	rkvpss_proc_cleanup(vpss_dev);
	rkvpss_unregister_subdev(vpss_dev);
	rkvpss_unregister_stream_vdevs(vpss_dev);

	v4l2_ctrl_handler_free(&vpss_dev->ctrl_handler);
	v4l2_async_nf_unregister(&vpss_dev->notifier);
	v4l2_async_nf_cleanup(&vpss_dev->notifier);
	media_device_unregister(&vpss_dev->media_dev);
	v4l2_device_unregister(&vpss_dev->v4l2_dev);
	mutex_destroy(&vpss_dev->apilock);
	return 0;
}

static int __maybe_unused rkvpss_dev_runtime_suspend(struct device *dev)
{
	struct rkvpss_device *vpss_dev = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&vpss_dev->hw_dev->dev_lock);
	ret = pm_runtime_put_sync(vpss_dev->hw_dev->dev);
	mutex_unlock(&vpss_dev->hw_dev->dev_lock);
	return (ret > 0) ? 0 : ret;
}

static int __maybe_unused rkvpss_dev_runtime_resume(struct device *dev)
{
	struct rkvpss_device *vpss_dev = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&vpss_dev->hw_dev->dev_lock);
	ret = pm_runtime_get_sync(vpss_dev->hw_dev->dev);
	mutex_unlock(&vpss_dev->hw_dev->dev_lock);
	return (ret > 0) ? 0 : ret;
}

static int rkvpss_pm_suspend(struct device *dev)
{
	struct rkvpss_device *vpss_dev = dev_get_drvdata(dev);
	u32 ret;

	v4l2_dbg(4, rkvpss_debug, &vpss_dev->v4l2_dev, "%s enter\n", __func__);
	if (vpss_dev->vpss_sdev.state == VPSS_STOP)
		return 0;

	vpss_dev->is_suspend = true;
	/* wait fe*/
	if (!vpss_dev->is_idle) {
		ret = wait_for_completion_timeout(&vpss_dev->pm_suspend_wait_fe,
						  msecs_to_jiffies(200));
		if (!ret)
			v4l2_info(&vpss_dev->v4l2_dev, "%s wait fe timeout\n", __func__);
	}
	v4l2_dbg(4, rkvpss_debug, &vpss_dev->v4l2_dev, "%s exit\n", __func__);
	return 0;
}

static int rkvpss_pm_resume(struct device *dev)
{
	struct rkvpss_device *vpss_dev = dev_get_drvdata(dev);

	v4l2_dbg(4, rkvpss_debug, &vpss_dev->v4l2_dev, "%s enter\n", __func__);
	if (vpss_dev->vpss_sdev.state == VPSS_STOP)
		return 0;

	vpss_dev->is_suspend = false;
	v4l2_dbg(4, rkvpss_debug, &vpss_dev->v4l2_dev, "%s exit\n", __func__);

	return 0;
}

static const struct dev_pm_ops rkvpss_plat_pm_ops = {
	.suspend = rkvpss_pm_suspend,
	.resume = rkvpss_pm_resume,
	SET_RUNTIME_PM_OPS(rkvpss_dev_runtime_suspend,
			   rkvpss_dev_runtime_resume, NULL)
};

struct platform_driver rkvpss_plat_drv = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(rkvpss_plat_of_match),
		.pm = &rkvpss_plat_pm_ops,
	},
	.probe = rkvpss_plat_probe,
	.remove = rkvpss_plat_remove,
};
EXPORT_SYMBOL(rkvpss_plat_drv);

MODULE_AUTHOR("Rockchip Camera/ISP team");
MODULE_DESCRIPTION("Rockchip VPSS platform driver");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
