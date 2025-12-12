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
			      struct rkooc_dummy_buffer *buf, u32 size)
{
	int ret;
	const struct vb2_mem_ops *g_ops = dev->mem_ops;
	struct sg_table *sg_tbl;
	void *mem_priv;

	buf->size = PAGE_ALIGN(size);

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

static const uint8_t h3812c1sh_regs[] = {
	0x40, 0x10, 0x03, 0xFF, 0xC0, 0x01, 0x8C, 0x2B, 0x0E, 0x20, 0xA2, 0x06,
	0x11, 0x62, 0x84, 0xB6, 0xF5, 0xF6, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x84, 0x60, 0xF2, 0x10, 0xAA, 0x1E, 0x00, 0x89, 0xE0,
	0xA3, 0xF0, 0x80, 0x0E, 0x4F, 0x81, 0x94, 0x54, 0x18, 0x2C, 0x28, 0xF2
};

static long rkooc_sd_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct rkooc_dev *dev = v4l2_get_subdevdata(sd);

	switch (cmd) {
	case RKOOC_CMD_CONFIG_SENSOR:
		{
			dev->sensor = (uintptr_t) arg;
			if (dev->sensor == RKOOC_SENSOR_H3812C1SH) {

				dev->reg_bits = 384;

				dev->win.image_width = 444;
				dev->win.image_height = 336;
				dev->win.ooc_width = 444;
				dev->win.ooc_height = 336;

				u32 size = dev->reg_bits / 8;

				rkooc_alloc_buffer(dev, &dev->reglist, size);
				memset(dev->reglist.vaddr, 0,
				       dev->reglist.size);

				uint8_t *reg = dev->reglist.vaddr;

				for (int i = 0; i < ARRAY_SIZE(h3812c1sh_regs); i++)
					reg[i] = h3812c1sh_regs[i];

			}
			if (dev->sensor == RKOOC_SENSOR_GST412C) {

				dev->win.image_width = 400;
				dev->win.image_height = 300;
				dev->win.ooc_width = 400;
				dev->win.ooc_height = 308;
			}

			break;
		}
	default:
		ret = -ENOIOCTLCMD;
	}

	dev->irfpatx_width = dev->win.image_width;
	dev->irfpatx_height = dev->win.image_height;
	dev->irfpatx_sizeimage = dev->win.image_width * dev->win.image_height * 3 / 2;
	dev->irfpatx_crop.left = dev->irfpatx_compose.left = 0;
	dev->irfpatx_crop.top = dev->irfpatx_compose.top = 0;
	dev->irfpatx_crop.width = dev->irfpatx_compose.width =
	    dev->irfpatx_width;
	dev->irfpatx_crop.height = dev->irfpatx_compose.height =
	    dev->irfpatx_height;

	if (dev->have_dummy) {
		rkooc_free_buffer(dev, &dev->dummy);
		dev->have_dummy = false;
	}

	u16 width = dev->win.ooc_width;
	u16 height = dev->win.ooc_height;
	u32 size = height * width;

	rkooc_alloc_buffer(dev, &dev->dummy, size);
	memset(dev->dummy.vaddr, dev->brightness->val, dev->dummy.size);
	dev->have_dummy = true;

	return ret;
}

static int rkooc_sd_s_stream(struct v4l2_subdev *sd, int on)
{
	struct rkooc_dev *dev = v4l2_get_subdevdata(sd);

	dev->ooctx_num = 0;
	if (on) {
		rkooc_hw_start(dev);
		rkooc_hw_enable_irq(dev);
	} else {
		rkooc_hw_disable_irq(dev);
		rkooc_hw_stop(dev);
	}
	return 0;
}

static int rkooc_sd_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int ret = 0;
	struct rkooc_dev *dev = container_of(sd, struct rkooc_dev, ooc_sd);

	if (flags & 0x1) {
		clk_set_rate(dev->pmclk, freq);
		v4l2_info(sd, "enable clk freq %u, real %lu\n",
			  freq, clk_get_rate(dev->pmclk));

		ret = clk_prepare_enable(dev->pmclk);
		if (ret < 0)
			v4l2_err(&dev->v4l2_dev, "Failed to enable pmclk!\n");

		rkooc_hw_pmclk_enable(dev);
	} else {
		rkooc_hw_pmclk_disable(dev);

		v4l2_info(sd, "disable clk\n");
		clk_disable_unprepare(dev->pmclk);
	}

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
