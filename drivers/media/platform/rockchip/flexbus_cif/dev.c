// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip Flexbus CIF Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#include <linux/pm_runtime.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-fwnode.h>
#include <linux/iommu.h>
#include <dt-bindings/soc/rockchip-system-status.h>
#include <soc/rockchip/rockchip-system-status.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include "dev.h"
#include "procfs.h"
#include <linux/kthread.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>
#include <dt-bindings/mfd/rockchip-flexbus.h>

int flexbus_cif_debug;
module_param_named(debug, flexbus_cif_debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

static DEFINE_MUTEX(flexbus_cif_mutex);
static LIST_HEAD(flexbus_cif_device_list);

void flexbus_cif_write_register(struct flexbus_cif_device *dev,
				   u32 offset, u32 val)
{
	rockchip_flexbus_writel(dev->fb_dev, offset, val);
	v4l2_dbg(4, flexbus_cif_debug, &dev->v4l2_dev,
		 "write reg[0x%x]:0x%x!!!\n",
		 offset, val);

}

void flexbus_cif_write_register_or(struct flexbus_cif_device *dev,
				      u32 offset, u32 val)
{
	unsigned int reg_val = 0x0;

	reg_val = rockchip_flexbus_readl(dev->fb_dev, offset);
	reg_val |= val;
	rockchip_flexbus_writel(dev->fb_dev, offset, reg_val);
	v4l2_dbg(4, flexbus_cif_debug, &dev->v4l2_dev,
		 "write or reg[0x%x]:0x%x!!!\n",
		 offset, val);
}

void flexbus_cif_write_register_and(struct flexbus_cif_device *dev,
				       u32 offset, u32 val)
{
	unsigned int reg_val = 0x0;

	reg_val = rockchip_flexbus_readl(dev->fb_dev, offset);
	reg_val &= val;
	rockchip_flexbus_writel(dev->fb_dev, offset, reg_val);
	v4l2_dbg(4, flexbus_cif_debug, &dev->v4l2_dev,
		 "write and reg[0x%x]:0x%x!!!\n",
		 offset, val);
}

unsigned int flexbus_cif_read_register(struct flexbus_cif_device *dev, u32 offset)
{
	unsigned int val = 0x0;

	val = rockchip_flexbus_readl(dev->fb_dev, offset);

	return val;
}

/**************************** pipeline operations *****************************/
static int __cif_pipeline_prepare(struct flexbus_cif_pipeline *p,
				  struct media_entity *me)
{
	struct v4l2_subdev *sd;
	int i;

	p->num_subdevs = 0;
	memset(p->subdevs, 0, sizeof(p->subdevs));

	while (1) {
		struct media_pad *pad = NULL;

		/* Find remote source pad */
		for (i = 0; i < me->num_pads; i++) {
			struct media_pad *spad = &me->pads[i];

			if (!(spad->flags & MEDIA_PAD_FL_SINK))
				continue;
			pad = media_pad_remote_pad_first(spad);
			if (pad)
				break;
		}

		if (!pad)
			break;

		sd = media_entity_to_v4l2_subdev(pad->entity);
		p->subdevs[p->num_subdevs++] = sd;
		me = &sd->entity;
		if (me->num_pads == 1)
			break;
	}

	return 0;
}

static int __cif_pipeline_s_cif_clk(struct flexbus_cif_pipeline *p)
{
	return 0;
}

static int flexbus_cif_pipeline_open(struct flexbus_cif_pipeline *p,
					struct media_entity *me,
					bool prepare)
{
	int ret;

	if (WARN_ON(!p || !me))
		return -EINVAL;
	if (atomic_inc_return(&p->power_cnt) > 1)
		return 0;

	/* go through media graphic and get subdevs */
	if (prepare)
		__cif_pipeline_prepare(p, me);

	if (!p->num_subdevs)
		return -EINVAL;

	ret = __cif_pipeline_s_cif_clk(p);
	if (ret < 0)
		return ret;

	return 0;
}

static int flexbus_cif_pipeline_close(struct flexbus_cif_pipeline *p)
{
	atomic_dec_return(&p->power_cnt);

	return 0;
}

/*
 * stream-on order: isp_subdev, mipi dphy, sensor
 * stream-off order: mipi dphy, sensor, isp_subdev
 */
static int flexbus_cif_pipeline_set_stream(struct flexbus_cif_pipeline *p, bool on)
{
	int i, ret = 0;

	if ((on && atomic_inc_return(&p->stream_cnt) > 1) ||
	    (!on && atomic_dec_return(&p->stream_cnt) > 0))
		return 0;

	/* phy -> sensor */
	for (i = 0; i < p->num_subdevs; i++) {
		ret = v4l2_subdev_call(p->subdevs[i], video, s_stream, on);
		if (on && ret < 0 && ret != -ENOIOCTLCMD && ret != -ENODEV)
			goto err_stream_off;
	}

	return 0;

err_stream_off:
	for (--i; i >= 0; --i)
		v4l2_subdev_call(p->subdevs[i], video, s_stream, false);
	return ret;
}

static int flexbus_cif_create_link(struct flexbus_cif_device *dev,
				      struct flexbus_cif_sensor_info *sensor)
{
	struct flexbus_cif_sensor_info linked_sensor;
	struct media_entity *source_entity, *sink_entity;
	int ret = 0;
	u32 pad;

	linked_sensor.sd = sensor->sd;

	memcpy(&linked_sensor.mbus, &sensor->mbus,
	       sizeof(struct v4l2_mbus_config));

	for (pad = 0; pad < linked_sensor.sd->entity.num_pads; pad++) {
		if (linked_sensor.sd->entity.pads[pad].flags &
		    MEDIA_PAD_FL_SOURCE) {
			if (pad == linked_sensor.sd->entity.num_pads) {
				dev_err(dev->dev,
					"failed to find src pad for %s\n",
					linked_sensor.sd->name);

				break;
			}

			if (linked_sensor.mbus.type == V4L2_MBUS_BT656 ||
			     linked_sensor.mbus.type == V4L2_MBUS_PARALLEL) {
				source_entity = &linked_sensor.sd->entity;
				sink_entity = &dev->stream[FLEXBUS_CIF_STREAM_CIF].vnode.vdev.entity;

				ret = media_create_pad_link(source_entity,
							    pad,
							    sink_entity,
							    0,
							    MEDIA_LNK_FL_ENABLED);
				if (ret)
					dev_err(dev->dev, "failed to create link for %s\n",
						linked_sensor.sd->name);
				break;
			}
		}
	}

	return ret;
}

/***************************** media controller *******************************/
static int flexbus_cif_create_links(struct flexbus_cif_device *dev)
{
	u32 s = 0;

	/* sensor links(or mipi-phy) */
	for (s = 0; s < dev->num_sensors; ++s) {
		struct flexbus_cif_sensor_info *sensor = &dev->sensors[s];

		flexbus_cif_create_link(dev, sensor);
	}

	return 0;
}

static int _set_pipeline_default_fmt(struct flexbus_cif_device *dev)
{
	flexbus_cif_set_default_fmt(dev);
	return 0;
}


static int subdev_notifier_complete(struct v4l2_async_notifier *notifier)
{
	struct flexbus_cif_device *dev;
	struct flexbus_cif_sensor_info *sensor;
	struct v4l2_subdev *sd;
	struct v4l2_device *v4l2_dev = NULL;
	int ret, index;

	dev = container_of(notifier, struct flexbus_cif_device, notifier);

	v4l2_dev = &dev->v4l2_dev;

	for (index = 0; index < dev->num_sensors; index++) {
		sensor = &dev->sensors[index];

		list_for_each_entry(sd, &v4l2_dev->subdevs, list) {
			if (sd->ops) {
				if (sd == sensor->sd) {
					ret = v4l2_subdev_call(sd,
							       pad,
							       get_mbus_config,
							       0,
							       &sensor->mbus);
					if (ret)
						v4l2_err(v4l2_dev,
							 "get mbus config failed for linking\n");
				}
			}
		}

		if (sensor->mbus.type == V4L2_MBUS_PARALLEL ||
		    sensor->mbus.type == V4L2_MBUS_BT656) {
			ret = flexbus_cif_register_cif_sof_subdev(dev);
			if (ret < 0) {
				v4l2_err(&dev->v4l2_dev,
					 "Err: register cif sof subdev failed!!!\n");
				goto notifier_end;
			}
			break;
		}
	}

	ret = flexbus_cif_create_links(dev);
	if (ret < 0)
		goto unregister_cif;

	ret = v4l2_device_register_subdev_nodes(&dev->v4l2_dev);
	if (ret < 0)
		goto unregister_cif;

	ret = _set_pipeline_default_fmt(dev);
	if (ret < 0)
		goto unregister_cif;

	v4l2_info(&dev->v4l2_dev, "Async subdev notifier completed\n");

	return ret;

unregister_cif:
	flexbus_cif_unregister_cif_sof_subdev(dev);
notifier_end:
	return ret;
}

struct flexbus_cif_async_subdev {
	struct v4l2_async_subdev asd;
	struct v4l2_mbus_config mbus;
	int lanes;
};

static int subdev_notifier_bound(struct v4l2_async_notifier *notifier,
				 struct v4l2_subdev *subdev,
				 struct v4l2_async_subdev *asd)
{
	struct flexbus_cif_device *cif_dev = container_of(notifier,
					struct flexbus_cif_device, notifier);
	struct flexbus_cif_async_subdev *s_asd = container_of(asd,
					struct flexbus_cif_async_subdev, asd);

	if (cif_dev->num_sensors == ARRAY_SIZE(cif_dev->sensors)) {
		v4l2_err(&cif_dev->v4l2_dev,
			 "%s: the num of subdev is beyond %d\n",
			 __func__, cif_dev->num_sensors);
		return -EBUSY;
	}

	cif_dev->sensors[cif_dev->num_sensors].lanes = s_asd->lanes;
	cif_dev->sensors[cif_dev->num_sensors].mbus = s_asd->mbus;
	cif_dev->sensors[cif_dev->num_sensors].sd = subdev;
	++cif_dev->num_sensors;

	v4l2_err(subdev, "Async registered subdev\n");

	return 0;
}

static int flexbus_cif_fwnode_parse(struct device *dev,
				       struct v4l2_fwnode_endpoint *vep,
				       struct v4l2_async_subdev *asd)
{
	struct flexbus_cif_async_subdev *rk_asd =
			container_of(asd, struct flexbus_cif_async_subdev, asd);

	if (vep->bus_type != V4L2_MBUS_BT656 &&
	    vep->bus_type != V4L2_MBUS_PARALLEL)
		return 0;

	rk_asd->mbus.type = vep->bus_type;

	return 0;
}

static const struct v4l2_async_notifier_operations subdev_notifier_ops = {
	.bound = subdev_notifier_bound,
	.complete = subdev_notifier_complete,
};

static int cif_subdev_notifier(struct flexbus_cif_device *cif_dev)
{
	struct v4l2_async_notifier *ntf = &cif_dev->notifier;
	struct device *dev = cif_dev->dev;
	int ret;

	v4l2_async_nf_init(ntf);

	ret = v4l2_async_nf_parse_fwnode_endpoints(
		dev, ntf, sizeof(struct flexbus_cif_async_subdev), flexbus_cif_fwnode_parse);

	if (ret < 0) {
		v4l2_err(&cif_dev->v4l2_dev,
			 "%s: parse fwnode failed\n", __func__);
		return ret;
	}

	ntf->ops = &subdev_notifier_ops;

	ret = v4l2_async_nf_register(&cif_dev->v4l2_dev, ntf);

	return ret;
}

/***************************** platform deive *******************************/

static int flexbus_cif_register_platform_subdevs(struct flexbus_cif_device *cif_dev)
{
	int stream_num = 0, ret;

	stream_num = FLEXBUS_CIF_SINGLE_STREAM;
	ret = flexbus_cif_register_stream_vdevs(cif_dev, stream_num,
					  false);

	if (ret < 0) {
		dev_err(cif_dev->dev, "cif register stream[%d] failed!\n", stream_num);
		return -EINVAL;
	}

	ret = cif_subdev_notifier(cif_dev);
	if (ret < 0) {
		v4l2_err(&cif_dev->v4l2_dev,
			 "Failed to register subdev notifier(%d)\n", ret);
		goto err_unreg_stream_vdev;
	}

	return 0;
err_unreg_stream_vdev:
	flexbus_cif_unregister_stream_vdevs(cif_dev, stream_num);
	return ret;
}

static void flexbus_cif_irq_handler(struct rockchip_flexbus *fb_dev, u32 isr)
{
	struct flexbus_cif_device *cif_dev = (struct flexbus_cif_device *)fb_dev->fb1_data;

	flexbus_cif_irq_pingpong(cif_dev, isr);
	flexbus_cif_write_register(cif_dev, FLEXBUS_ICR, isr);
	if (cif_dev->err_state &&
	    (!work_busy(&cif_dev->err_state_work.work))) {
		cif_dev->err_state_work.err_state = cif_dev->err_state;
		cif_dev->err_state = 0;
		schedule_work(&cif_dev->err_state_work.work);
	}
}

static int flexbus_cif_plat_init(struct flexbus_cif_device *cif_dev, struct device_node *node)
{
	struct device *dev = cif_dev->dev;
	struct v4l2_device *v4l2_dev;
	int ret;

	mutex_init(&cif_dev->stream_lock);
	atomic_set(&cif_dev->pipe.power_cnt, 0);
	atomic_set(&cif_dev->pipe.stream_cnt, 0);
	atomic_set(&cif_dev->power_cnt, 0);
	cif_dev->pipe.open = flexbus_cif_pipeline_open;
	cif_dev->pipe.close = flexbus_cif_pipeline_close;
	cif_dev->pipe.set_stream = flexbus_cif_pipeline_set_stream;
	cif_dev->id_use_cnt = 0;
	INIT_WORK(&cif_dev->err_state_work.work, flexbus_cif_err_print_work);

	flexbus_cif_stream_init(cif_dev, FLEXBUS_CIF_STREAM_CIF);
	cif_dev->is_dma_sg_ops = true;

#if defined(CONFIG_ROCKCHIP_FLEXBUS_CIF_USE_DUMMY_BUF)
	cif_dev->is_use_dummybuf = true;
#else
	cif_dev->is_use_dummybuf = false;
#endif

	strscpy(cif_dev->media_dev.model, "flexbus-cif",
		sizeof(cif_dev->media_dev.model));
	cif_dev->media_dev.dev = dev;
	v4l2_dev = &cif_dev->v4l2_dev;
	v4l2_dev->mdev = &cif_dev->media_dev;
	strscpy(v4l2_dev->name, "flexbus-cif", sizeof(v4l2_dev->name));

	ret = v4l2_device_register(cif_dev->dev, &cif_dev->v4l2_dev);
	if (ret < 0)
		return ret;

	media_device_init(&cif_dev->media_dev);
	ret = media_device_register(&cif_dev->media_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "Failed to register media device: %d\n",
			 ret);
		goto err_unreg_v4l2_dev;
	}

	/* create & register platefom subdev (from of_node) */
	ret = flexbus_cif_register_platform_subdevs(cif_dev);
	if (ret < 0)
		goto err_unreg_media_dev;
	mutex_lock(&flexbus_cif_mutex);
	list_add_tail(&cif_dev->list, &flexbus_cif_device_list);
	mutex_unlock(&flexbus_cif_mutex);

	return 0;

err_unreg_media_dev:
	media_device_unregister(&cif_dev->media_dev);
err_unreg_v4l2_dev:
	v4l2_device_unregister(&cif_dev->v4l2_dev);
	return ret;
}

static int flexbus_cif_plat_uninit(struct flexbus_cif_device *cif_dev)
{
	int stream_num = 0;

	if (cif_dev->active_sensor->mbus.type == V4L2_MBUS_BT656 ||
	    cif_dev->active_sensor->mbus.type == V4L2_MBUS_PARALLEL)
		flexbus_cif_unregister_cif_sof_subdev(cif_dev);

	media_device_unregister(&cif_dev->media_dev);
	v4l2_device_unregister(&cif_dev->v4l2_dev);

	stream_num = FLEXBUS_CIF_SINGLE_STREAM;

	flexbus_cif_unregister_stream_vdevs(cif_dev, stream_num);

	return 0;
}

static const struct flexbus_cif_match_data rk3576_cif_match_data = {
	.chip_id = RK_FLEXBUS_CIF_RK3576,
};

static const struct flexbus_cif_match_data rk3506_cif_match_data = {
	.chip_id = RK_FLEXBUS_CIF_RK3506,
};

static const struct of_device_id flexbus_cif_plat_of_match[] = {
	{
		.compatible = "rockchip,flexbus-cif-rk3576",
		.data = &rk3576_cif_match_data,
	},
	{
		.compatible = "rockchip,flexbus-cif-rk3506",
		.data = &rk3506_cif_match_data,
	},
	{},
};

static int flexbus_cif_plat_probe(struct platform_device *pdev)
{
	struct rockchip_flexbus *rkfb = dev_get_drvdata(pdev->dev.parent);
	const struct of_device_id *match;
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct flexbus_cif_device *cif_dev;
	const struct flexbus_cif_match_data *data = NULL;
	int ret;

	dev_info(dev, "czf flexbus_cif driver probe\n");

	if (rkfb->opmode1 != ROCKCHIP_FLEXBUS1_OPMODE_CIF) {
		dev_err(&pdev->dev, "flexbus1 opmode mismatch!\n");
		return -ENODEV;
	}

	match = of_match_node(flexbus_cif_plat_of_match, node);
	if (IS_ERR(match))
		return PTR_ERR(match);

	cif_dev = devm_kzalloc(dev, sizeof(*cif_dev), GFP_KERNEL);
	if (!cif_dev)
		return -ENOMEM;

	dev_set_drvdata(dev, cif_dev);
	cif_dev->dev = dev;
	data = match->data;
	cif_dev->chip_id = data->chip_id;

	ret = flexbus_cif_plat_init(cif_dev, node);
	if (ret)
		return ret;

	cif_dev->fb_dev = rkfb;
	rockchip_flexbus_set_fb1(rkfb, cif_dev, flexbus_cif_irq_handler);

	if (flexbus_cif_proc_init(cif_dev))
		dev_warn(dev, "dev:%s create proc failed\n", dev_name(dev));

	pm_runtime_enable(&pdev->dev);

	return 0;
}

static int flexbus_cif_plat_remove(struct platform_device *pdev)
{
	struct flexbus_cif_device *cif_dev = platform_get_drvdata(pdev);

	flexbus_cif_plat_uninit(cif_dev);
	flexbus_cif_proc_cleanup(cif_dev);

	return 0;
}

static int __maybe_unused flexbus_cif_runtime_suspend(struct device *dev)
{
	struct flexbus_cif_device *cif_dev = dev_get_drvdata(dev);
	int ret = 0;

	if (atomic_dec_return(&cif_dev->power_cnt))
		return 0;

	return (ret > 0) ? 0 : ret;
}

static int __maybe_unused flexbus_cif_runtime_resume(struct device *dev)
{
	struct flexbus_cif_device *cif_dev = dev_get_drvdata(dev);
	int ret = 0;

	if (atomic_inc_return(&cif_dev->power_cnt) > 1)
		return 0;

	return (ret > 0) ? 0 : ret;
}

static int __maybe_unused __flexbus_cif_clr_unready_dev(void)
{
	struct flexbus_cif_device *cif_dev;

	mutex_lock(&flexbus_cif_mutex);

	list_for_each_entry(cif_dev, &flexbus_cif_device_list, list) {
		v4l2_async_notifier_clr_unready_dev(&cif_dev->notifier);
	}

	mutex_unlock(&flexbus_cif_mutex);

	return 0;
}

static int flexbus_cif_clr_unready_dev_param_set(const char *val, const struct kernel_param *kp)
{
#ifdef MODULE
	__flexbus_cif_clr_unready_dev();
#endif

	return 0;
}

module_param_call(flexbus_cif_clr_unready_dev, flexbus_cif_clr_unready_dev_param_set, NULL, NULL, 0200);
MODULE_PARM_DESC(flexbus_cif_clr_unready_dev, "clear unready devices");

#ifndef MODULE
int flexbus_cif_clr_unready_dev(void)
{
	__flexbus_cif_clr_unready_dev();

	return 0;
}
late_initcall(flexbus_cif_clr_unready_dev);
#endif

static const struct dev_pm_ops flexbus_cif_plat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(flexbus_cif_runtime_suspend, flexbus_cif_runtime_resume, NULL)
};

struct platform_driver flexbus_cif_plat_drv = {
	.driver = {
		.name = CIF_DRIVER_NAME,
		.of_match_table = of_match_ptr(flexbus_cif_plat_of_match),
		.pm = &flexbus_cif_plat_pm_ops,
	},
	.probe = flexbus_cif_plat_probe,
	.remove = flexbus_cif_plat_remove,
};

static int flexbus_cif_plat_drv_init(void)
{
	return platform_driver_register(&flexbus_cif_plat_drv);
}

static void __exit flexbus_cif_plat_drv_exit(void)
{
	platform_driver_unregister(&flexbus_cif_plat_drv);
}

module_init(flexbus_cif_plat_drv_init);
module_exit(flexbus_cif_plat_drv_exit);

MODULE_AUTHOR("Rockchip Camera/ISP team");
MODULE_DESCRIPTION("Rockchip flexbus cif platform driver");
MODULE_LICENSE("GPL");
