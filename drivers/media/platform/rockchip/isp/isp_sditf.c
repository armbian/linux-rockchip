// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-common.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include "dev.h"
#include "isp_vpss.h"
#include "regs.h"

static int rkisp_sditf_get_set_fmt(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_format *fmt)
{
	struct rkisp_sditf_device *sditf = v4l2_get_subdevdata(sd);
	struct rkisp_device *dev = sditf->isp;

	if (!fmt)
		return -EINVAL;

	/* get isp out format */
	fmt->pad = RKISP_ISP_PAD_SOURCE_PATH;
	fmt->which = V4L2_SUBDEV_FORMAT_ACTIVE;
	return v4l2_subdev_call(&dev->isp_sdev.sd, pad, get_fmt, NULL, fmt);
}

static int rkisp_sditf_s_stream(struct v4l2_subdev *sd, int on)
{
	struct rkisp_sditf_device *sditf = v4l2_get_subdevdata(sd);
	struct rkisp_device *dev = sditf->isp;
	struct rkisp_hw_dev *hw = dev->hw_dev;
	struct rkisp_isp_subdev *isp_sdev = &dev->isp_sdev;
	struct rkisp_stream *stream;
	int ret = 0;

	if (dev->isp_ver == ISP_V39) {
		stream = &dev->cap_dev.stream[RKISP_STREAM_LDC];
		if (stream->linked) {
			v4l2_err(sd, "isp to vpss online no support for ldcpath link\n");
			return -EINVAL;
		}
	}

	v4l2_dbg(1, rkisp_debug, sd, "%s %d\n", __func__, on);

	mutex_lock(&hw->dev_lock);
	if (on) {
		if (dev->is_pre_on &&
		    !dev->hw_dev->is_single &&
		    !atomic_read(&dev->hw_dev->refcnt) &&
		    !atomic_read(&dev->cap_dev.refcnt))
			rkisp_hw_enum_isp_size(dev->hw_dev);

		atomic_inc(&dev->cap_dev.refcnt);
		ret = dev->pipe.open(&dev->pipe, &isp_sdev->sd.entity, true);
		if (ret < 0)
			goto refcnt_dec;
		ret = dev->pipe.set_stream(&dev->pipe, true);
		if (ret < 0)
			goto pipe_close;
		sditf->is_on = true;
		dev->irq_ends_mask |= ISP_FRAME_VPSS;
		goto unlock;
	}
	sditf->is_on = false;
	dev->irq_ends_mask &= ~ISP_FRAME_VPSS;
	dev->pipe.set_stream(&dev->pipe, false);
pipe_close:
	dev->pipe.close(&dev->pipe);
refcnt_dec:
	atomic_dec(&dev->cap_dev.refcnt);
unlock:
	if (dev->is_pre_on && !atomic_read(&dev->cap_dev.refcnt)) {
		dev->is_pre_on = false;
		dev->params_vdev.first_cfg_params = false;
		stream = &dev->cap_dev.stream[0];
		v4l2_pipeline_pm_put(&stream->vnode.vdev.entity);
	}
	mutex_unlock(&hw->dev_lock);
	return ret;
}

static int rkisp_sditf_s_power(struct v4l2_subdev *sd, int on)
{
	struct rkisp_sditf_device *sditf = v4l2_get_subdevdata(sd);
	struct rkisp_device *dev = sditf->isp;
	struct rkisp_stream *stream = &dev->cap_dev.stream[0];
	int ret = 0;

	v4l2_dbg(1, rkisp_debug, sd, "%s %d\n", __func__, on);

	if (on) {
		sditf->remote_sd = v4l2_get_subdev_hostdata(sd);
		ret = v4l2_pipeline_pm_get(&stream->vnode.vdev.entity);
	} else {
		v4l2_pipeline_pm_put(&stream->vnode.vdev.entity);
	}
	return ret;
}

void rkisp_sditf_reset_notify_vpss(struct rkisp_device *dev)
{
	struct rkisp_sditf_device *sditf = dev->sditf_dev;

	if (!sditf || !sditf->is_on || !sditf->remote_sd)
		return;
	v4l2_info(&dev->v4l2_dev, "%s\n", __func__);
	v4l2_subdev_call(sditf->remote_sd, core, ioctl, RKISP_VPSS_RESET_NOTIFY_VPSS, NULL);
}

static void rkisp_config_frame_info(struct rkisp_device *dev, struct rkisp_vpss_frame_info *info)
{
	struct sensor_exposure_cfg *exp = &dev->params_vdev.exposure;
	u32 seq = 0;
	u64 ns = 0;

	rkisp_dmarx_get_frame(dev, &seq, NULL, &ns, true);

	if (!ns)
		ns = ktime_get_ns();

	info->seq = seq;
	info->hdr = 0;
	info->timestamp = IS_HDR_RDBK(dev->rd_mode) ? ns : dev->vicap_sof.timestamp;
	info->rolling_shutter_skew = exp->linear_exp.rolling_shutter_skew;
	info->sensor_exposure_time = exp->linear_exp.coarse_integration_time;
	info->sensor_analog_gain = exp->linear_exp.analog_gain_code_global;
	info->sensor_digital_gain = exp->linear_exp.digital_gain_global;
	info->isp_digital_gain = exp->linear_exp.isp_digital_gain;
	if (dev->rd_mode == HDR_RDBK_FRAME2 ||
		dev->rd_mode == HDR_FRAMEX2_DDR ||
		dev->rd_mode == HDR_LINEX2_DDR) {
		info->hdr = 1;
		info->rolling_shutter_skew = exp->hdr_exp[0].rolling_shutter_skew;

		info->sensor_exposure_time = exp->hdr_exp[0].coarse_integration_time;
		info->sensor_analog_gain = exp->hdr_exp[0].analog_gain_code_global;
		info->sensor_digital_gain = exp->hdr_exp[0].digital_gain_global;
		info->isp_digital_gain = exp->hdr_exp[0].isp_digital_gain;

		info->sensor_exposure_time_l = exp->hdr_exp[1].coarse_integration_time;
		info->sensor_analog_gain_l = exp->hdr_exp[1].analog_gain_code_global;
		info->sensor_digital_gain_l = exp->hdr_exp[1].digital_gain_global;
		info->isp_digital_gain_l = exp->hdr_exp[1].isp_digital_gain;
	}
}

void rkisp_sditf_sof(struct rkisp_device *dev, u32 irq)
{
	struct rkisp_sditf_device *sditf = dev->sditf_dev;
	struct rkisp_vpss_sof info;
	struct rkisp_vpss_frame_info frame_info;

	if (!sditf || !sditf->is_on || !sditf->remote_sd)
		return;
	info.irq = irq;
	rkisp_dmarx_get_frame(dev, &info.seq, NULL, &info.timestamp, !dev->is_aiisp_en);
	info.unite_index = dev->unite_index;
	if (dev->isp_ver == ISP_V35)
		info.grey = !!(rkisp_read(dev, ISP3X_CNR_CTRL, false) & ISP35_CNR_UV_DIS);
	v4l2_subdev_call(sditf->remote_sd, core, ioctl, RKISP_VPSS_CMD_SOF, &info);

	rkisp_config_frame_info(dev, &frame_info);

	v4l2_subdev_call(sditf->remote_sd, core, ioctl, RKISP_VPSS_CMD_FRAME_INFO, &frame_info);
}

static long rkisp_sditf_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct rkisp_sditf_device *sditf = v4l2_get_subdevdata(sd);
	long ret = 0;

	switch (cmd) {
	case RKISP_VPSS_CMD_EOF:
		if (*(int *)arg) {
			sditf->is_on = false;
			sditf->isp->irq_ends_mask &= ~ISP_FRAME_VPSS;
		}
		rkisp_check_idle(sditf->isp, ISP_FRAME_VPSS);
		break;
	case RKISP_VPSS_GET_UNITE_MODE:
		*(unsigned int *)arg = sditf->isp->unite_div - 1;
		break;
	case RKISP_VPSS_GET_ISP_WORKING:
		*(int *)arg = sditf->isp->hw_dev->is_runing;
		break;
	case RKISP_VPSS_GET_UNITE_EXTEND_PIXEL:
		*(int *)arg = sditf->isp->hw_dev->unite_extend_pixel;
		break;
	default:
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

static const struct v4l2_subdev_pad_ops sditf_pad_ops = {
	.set_fmt = rkisp_sditf_get_set_fmt,
	.get_fmt = rkisp_sditf_get_set_fmt,
};

static const struct v4l2_subdev_video_ops sditf_video_ops = {
	.s_stream = rkisp_sditf_s_stream,
};

static const struct v4l2_subdev_core_ops sditf_core_ops = {
	.s_power = rkisp_sditf_s_power,
	.ioctl = rkisp_sditf_ioctl,
};

static const struct v4l2_subdev_ops sditf_subdev_ops = {
	.core = &sditf_core_ops,
	.video = &sditf_video_ops,
	.pad = &sditf_pad_ops,
};

static int rkisp_sditf_notifier(struct rkisp_sditf_device *sditf)
{
	struct v4l2_async_notifier *ntf = &sditf->notifier;
	int ret;

	v4l2_async_nf_init(ntf);
	ret = v4l2_async_subdev_nf_register(&sditf->sd, ntf);
	if (ret) {
		v4l2_async_nf_cleanup(ntf);
		dev_err(sditf->dev, "failed to register async notifier:%d\n", ret);
		return ret;
	}

	return v4l2_async_register_subdev(&sditf->sd);
}

static int rkisp_sditf_attach(struct rkisp_sditf_device *sditf)
{
	struct rkisp_device *isp;
	struct platform_device *pdev;
	struct device_node *np;

	np = of_parse_phandle(sditf->dev->of_node, "rockchip,isp", 0);
	if (!np || !of_device_is_available(np)) {
		dev_err(sditf->dev, "failed to get isp node\n");
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		dev_err(sditf->dev, "failed to get isp from node\n");
		return -ENODEV;
	}
	isp = platform_get_drvdata(pdev);
	if (!isp) {
		dev_err(sditf->dev, "failed to attach isp\n");
		return -EINVAL;
	}
	isp->sditf_dev = sditf;
	sditf->isp = isp;

	return 0;
}

static int rkisp_sditf_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rkisp_sditf_device *sditf;
	struct v4l2_subdev *sd;
	int ret;

	sditf = devm_kzalloc(dev, sizeof(*sditf), GFP_KERNEL);
	if (!sditf)
		return -ENOMEM;
	dev_set_drvdata(dev, sditf);
	sditf->dev = dev;
	sd = &sditf->sd;
	v4l2_subdev_init(sd, &sditf_subdev_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_COMPOSER;
	strscpy(sd->name, dev_name(dev), sizeof(sd->name));

	strscpy(sd->name, dev_name(dev), sizeof(sd->name));
	v4l2_set_subdevdata(sd, sditf);
	sd->dev = dev;

	ret = rkisp_sditf_attach(sditf);
	if (ret < 0)
		return ret;

	sditf->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sd->entity, 1, &sditf->pad);
	if (ret < 0)
		return ret;

	return rkisp_sditf_notifier(sditf);
}

static int rkisp_sditf_remove(struct platform_device *pdev)
{
	struct rkisp_sditf_device *sditf = platform_get_drvdata(pdev);
	struct v4l2_subdev *sd = &sditf->sd;

	v4l2_async_nf_unregister(&sditf->notifier);
	v4l2_async_nf_cleanup(&sditf->notifier);

	media_entity_cleanup(&sd->entity);
	v4l2_async_unregister_subdev(sd);
	return 0;
}

static const struct of_device_id rkisp_sditf_of_match[] = {
	{
		.compatible = "rockchip,rkisp-sditf",
	},
	{},
};

struct platform_driver rkisp_sditf_drv = {
	.probe = rkisp_sditf_probe,
	.remove = rkisp_sditf_remove,
	.driver = {
		.name = "rkisp_sditf",
		.of_match_table = of_match_ptr(rkisp_sditf_of_match),
	},
};
EXPORT_SYMBOL(rkisp_sditf_drv);
