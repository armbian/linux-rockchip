// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2025 Rockchip Electronics Co., Ltd. */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include "dev.h"
#include "regs.h"
#include "rkooc-externel.h"

// ctrl ops
static int rkooc_user_vid_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct rkooc_dev *dev =
	    container_of(ctrl->handler, struct rkooc_dev, ctrl_hdl);
	struct v4l2_subdev *sd = &dev->ooc_sd;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		v4l2_info(sd, "OCC Set Brightness %d\n", ctrl->val);
		break;
	}
	return 0;
}

static const struct v4l2_ctrl_ops rkooc_out_ctrl_ops = {
	.s_ctrl = rkooc_user_vid_s_ctrl,
};

static void rkooc_init_dummy_vb2(struct rkooc_dev *dev,
				 struct rkooc_dummy_buffer *buf)
{
	unsigned long attrs = DMA_ATTR_FORCE_CONTIGUOUS;

	memset(&buf->vb2_queue, 0, sizeof(buf->vb2_queue));
	memset(&buf->vb, 0, sizeof(buf->vb));
	buf->vb2_queue.gfp_flags = GFP_KERNEL | GFP_DMA32;
	buf->vb2_queue.dma_dir = DMA_BIDIRECTIONAL;
	buf->vb2_queue.dma_attrs = attrs;
	buf->vb.vb2_buf.vb2_queue = &buf->vb2_queue;
}

static int rkooc_alloc_buffer(struct rkooc_dev *dev,
			      struct rkooc_dummy_buffer *buf)
{
	int ret;
	const struct vb2_mem_ops *g_ops = dev->mem_ops;
	struct sg_table *sg_tbl;
	void *mem_priv;

	u16 width = dev->ooc_width;
	u16 height = dev->ooc_height;

	buf->size = PAGE_ALIGN(width * height);

	rkooc_init_dummy_vb2(dev, buf);
	mem_priv = g_ops->alloc(&buf->vb.vb2_buf, dev->v4l2_dev.dev, buf->size);
	if (IS_ERR_OR_NULL(mem_priv)) {
		ret = -ENOMEM;
		return ret;
	}

	buf->mem_priv = mem_priv;
	sg_tbl = (struct sg_table *)g_ops->cookie(&buf->vb.vb2_buf, mem_priv);
	buf->dma_addr = sg_dma_address(sg_tbl->sgl);
	g_ops->prepare(mem_priv);
	buf->vaddr = g_ops->vaddr(&buf->vb.vb2_buf, mem_priv);

	memset(buf->vaddr, dev->brightness->val, buf->size);

	v4l2_info(&dev->v4l2_dev,
		  "rkooc alloc dummy buffer size %d, vaddr %p, dmaaddr %x, val %d\n",
		  buf->size, buf->vaddr, (u32) buf->dma_addr,
		  dev->brightness->val);
	return 0;
}

static void rkooc_free_buffer(struct rkooc_dev *dev,
			      struct rkooc_dummy_buffer *buf)
{
	const struct vb2_mem_ops *g_ops = dev->mem_ops;

	if (buf && buf->mem_priv) {
		v4l2_info(&dev->v4l2_dev, "%s buf:0x%x\n", __func__,
			  (u32) buf->dma_addr);
		g_ops->put(buf->mem_priv);
		buf->size = 0;
		buf->vaddr = NULL;
		buf->mem_priv = NULL;
	}
}

static long rkooc_sd_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct rkooc_dev *dev = v4l2_get_subdevdata(sd);

	switch (cmd) {
	case RKOOC_CMD_CONFIG_FULLMODE:
		{
			v4l2_info(&dev->v4l2_dev, "CONIFG_FULLMODE\n");
			break;
		}
	case RKOOC_CMD_CONFIG_DATAMODE:
		{
			struct rkooc_config_datamode *cfg =
			    (struct rkooc_config_datamode *)arg;

			dev->ooc_width = cfg->ooc_width;
			dev->ooc_height = cfg->ooc_height;
			dev->image_width = cfg->image_width;
			dev->image_height = cfg->image_height;

			dev->irfpatx_width = dev->image_width;
			dev->irfpatx_height = dev->image_height;
			dev->irfpatx_sizeimage =
			    dev->image_width * dev->image_height;
			dev->irfpatx_crop.left = dev->irfpatx_compose.left = 0;
			dev->irfpatx_crop.top = dev->irfpatx_compose.top = 0;
			dev->irfpatx_crop.width = dev->irfpatx_compose.width =
			    dev->irfpatx_width;
			dev->irfpatx_crop.height = dev->irfpatx_compose.height =
			    dev->irfpatx_height;
			break;
		}
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static int rkooc_sd_s_stream(struct v4l2_subdev *sd, int on)
{
	struct rkooc_dev *dev = v4l2_get_subdevdata(sd);

	if (on) {
		rkooc_alloc_buffer(dev, &dev->dummy);
		rkooc_hw_win1_config(dev);
		rkooc_hw_sys_config(dev);
		rkooc_hw_enable_irq(dev);
	} else {
		rkooc_hw_disable_irq(dev);
		rkooc_hw_win1_disable(dev);
		rkooc_free_buffer(dev, &dev->dummy);
	}
	return 0;
}

static int rkooc_sd_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int ret = 0;
	struct rkooc_dev *dev = container_of(sd, struct rkooc_dev, ooc_sd);

	if (flags)
		ret = rkooc_hw_pmclk_enable(dev);
	else
		rkooc_hw_pmclk_disable(dev);
	return ret;
}

static const struct v4l2_subdev_core_ops rkooc_core_ops = {
	.ioctl = rkooc_sd_ioctl,
};

static const struct v4l2_subdev_video_ops rkooc_video_ops = {
	.s_stream = rkooc_sd_s_stream,
	.s_crystal_freq = rkooc_sd_s_crystal_freq,
};

static const struct v4l2_subdev_ops rkooc_sd_ops = {
	.core = &rkooc_core_ops,
	.video = &rkooc_video_ops,
};

int rkooc_register_ooc_subdev(struct rkooc_dev *dev)
{
	int ret;
	struct v4l2_subdev *sd = &dev->ooc_sd;
	struct v4l2_ctrl_handler *ctrl_hdl = &dev->ctrl_hdl;

	/* set up the ctrl_handler of the video capture device */
	v4l2_ctrl_handler_init(ctrl_hdl, 2);
	dev->brightness = v4l2_ctrl_new_std(ctrl_hdl, &rkooc_out_ctrl_ops,
					    V4L2_CID_BRIGHTNESS, 0, 0x3e, 1,
					    0x20);
	v4l2_ctrl_handler_setup(ctrl_hdl);

	v4l2_subdev_init(sd, &rkooc_sd_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, sizeof(sd->name), "rkooc-subdev");

#ifdef CONFIG_MEDIA_CONTROLLER
	sd->entity.ops = NULL;
	sd->entity.function = MEDIA_ENT_F_V4L2_SUBDEV_UNKNOWN;
	dev->subdev_pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sd->entity, 1, &dev->subdev_pad);
	if (ret)
		goto err_free_handler;
#endif

	sd->owner = THIS_MODULE;
	sd->ctrl_handler = ctrl_hdl;
	v4l2_set_subdevdata(sd, dev);

	ret = v4l2_device_register_subdev(&dev->v4l2_dev, sd);
	if (ret < 0) {
		v4l2_err(sd, "Failed to register isp subdev\n");
		goto err_cleanup_media_entity;
	}

	return 0;
err_cleanup_media_entity:
#ifdef CONFIG_MEDIA_CONTROLLER
	media_entity_cleanup(&sd->entity);
#endif
err_free_handler:
	v4l2_ctrl_handler_free(&dev->ctrl_hdl);
	return ret;
}

void rkooc_unregister_ooc_subdev(struct rkooc_dev *dev)
{
	struct v4l2_subdev *sd = &dev->ooc_sd;

	v4l2_ctrl_handler_free(&dev->ctrl_hdl);
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
}
