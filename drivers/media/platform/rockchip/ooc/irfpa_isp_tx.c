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
#include <media/v4l2-rect.h>
#include "dev.h"

static int irfpatx_querycap(struct file *file, void *priv,
			    struct v4l2_capability *cap)
{
	struct rkooc_dev *dev = video_drvdata(file);

	strscpy(cap->driver, "rk_irfpa", sizeof(cap->driver));
	strscpy(cap->card, "irfpa_tx", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev->v4l2_dev.name);

	cap->capabilities = dev->vid_cap_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int irfpatx_enum_fmt(struct file *file, void *priv,
			    struct v4l2_fmtdesc *f)
{
	switch (f->index) {
	case 0:
		f->pixelformat = V4L2_PIX_FMT_NV12;
		break;
	case 1:
		f->pixelformat = V4L2_PIX_FMT_GREY;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int irfpatx_g_selection(struct file *file, void *priv,
			       struct v4l2_selection *sel)
{
	struct rkooc_dev *dev = video_drvdata(file);

	sel->r.left = sel->r.top = 0;
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = dev->irfpatx_crop;
		break;
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.width = dev->win.image_width;
		sel->r.height = dev->win.image_height;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int irfpatx_s_selection(struct file *file, void *fh,
			       struct v4l2_selection *sel)
{
	struct rkooc_dev *dev = video_drvdata(file);

	if (sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		dev->irfpatx_crop = sel->r;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int irfpatx_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct rkooc_dev *dev = video_drvdata(file);
	struct v4l2_pix_format *pix = &f->fmt.pix;

	pix->width = dev->irfpatx_width;
	pix->height = dev->irfpatx_height;
	pix->pixelformat = dev->irfpatx_fourcc;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = dev->irfpatx_width;
	pix->sizeimage = dev->irfpatx_sizeimage;
	return 0;
}

static int irfpatx_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct rkooc_dev *dev = video_drvdata(file);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_rect compose = { 0, 0, pix->width, pix->height };
	struct v4l2_rect crop = {
	    0, 0, dev->win.image_width, dev->win.image_height };

	dev->irfpatx_width = pix->width;
	dev->irfpatx_height = pix->height;

	if (pix->pixelformat == V4L2_PIX_FMT_GREY) {
		dev->irfpatx_fourcc = V4L2_PIX_FMT_GREY;
		dev->irfpatx_sizeimage = pix->height * pix->width;
	} else {
		dev->irfpatx_fourcc = V4L2_PIX_FMT_NV12;
		dev->irfpatx_sizeimage = pix->height * pix->width * 3 / 2;
	}

	// reset crop
	dev->irfpatx_crop = crop;

	dev->irfpatx_compose = compose;
	return 0;
}

static const struct v4l2_ioctl_ops irfpa_ioctl_ops = {
	.vidioc_querycap = irfpatx_querycap,

	.vidioc_enum_fmt_vid_cap = irfpatx_enum_fmt,
	.vidioc_g_fmt_vid_cap = irfpatx_g_fmt,
	.vidioc_s_fmt_vid_cap = irfpatx_s_fmt,

	.vidioc_g_selection = irfpatx_g_selection,
	.vidioc_s_selection = irfpatx_s_selection,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
};

// queue ops
static int rkooc_cap_queue_setup(struct vb2_queue *vq,
				 unsigned int *nbuffers, unsigned int *nplanes,
				 unsigned int sizes[],
				 struct device *alloc_devs[])
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vq);

	if (*nplanes == 0) {
		*nplanes = 1;
		sizes[0] = dev->irfpatx_sizeimage;
		return 0;
	}
	return -EINVAL;
}

static int rkooc_cap_buf_init(struct vb2_buffer *vb)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkooc_buffer *buf = container_of(vbuf, struct rkooc_buffer, vb);

	buf->vaddr = (uint8_t *) vb2_plane_vaddr(&buf->vb.vb2_buf, 0);
	buf->size = vb2_plane_size(&buf->vb.vb2_buf, 0);

	v4l2_info(&dev->v4l2_dev, "%s: buf %d, vaddr %p, size %d\n", __func__,
		  vb->index, buf->vaddr, buf->size);
	return 0;
}

static int rkooc_cap_buf_prepare(struct vb2_buffer *vb)
{
	//struct rkooc_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkooc_buffer *buf = container_of(vbuf, struct rkooc_buffer, vb);

	vb2_set_plane_payload(vb, 0, buf->size);
	vb->planes[0].data_offset = 0;
	return 0;
}

static void rkooc_cap_buf_queue(struct vb2_buffer *vb)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkooc_buffer *buf = container_of(vbuf, struct rkooc_buffer, vb);

	spin_lock(&dev->irfpa_lock);
	list_add_tail(&buf->list, &dev->vid_cap_active);
	spin_unlock(&dev->irfpa_lock);
}

static int rkooc_cap_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vq);

	v4l2_info(&dev->v4l2_dev, "enter %s, count %d\n", __func__, count);
	return 0;
}

static void rkooc_cap_stop_streaming(struct vb2_queue *vq)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vq);

	/* Release all active buffers */
	while (!list_empty(&dev->vid_cap_active)) {
		struct rkooc_buffer *buf;

		buf = list_entry(dev->vid_cap_active.next,
				 struct rkooc_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		v4l2_info(&dev->v4l2_dev, "vid_cap buffer %d done\n",
			  buf->vb.vb2_buf.index);
	}

}

static const struct vb2_ops rkooc_cap_qops = {
	.queue_setup = rkooc_cap_queue_setup,
	.buf_init = rkooc_cap_buf_init,
	.buf_prepare = rkooc_cap_buf_prepare,
	.buf_queue = rkooc_cap_buf_queue,
	.start_streaming = rkooc_cap_start_streaming,
	.stop_streaming = rkooc_cap_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

static int rkooc_cap_init_queue(struct rkooc_dev *dev)
{
	struct vb2_queue *q = &dev->irfpa_tx_queue;

	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP;
	q->drv_priv = dev;
	q->buf_struct_size = sizeof(struct rkooc_buffer);
	q->ops = &rkooc_cap_qops;
	q->mem_ops = dev->mem_ops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = 1;
	q->lock = &dev->mutex;
	q->dev = dev->v4l2_dev.dev;
	q->allow_cache_hints = true;
	q->bidirectional = true;
	q->gfp_flags = GFP_DMA32;
	q->dma_attrs = DMA_ATTR_FORCE_CONTIGUOUS;

	return vb2_queue_init(q);
}

// file ops
static int rkooc_cap_fop_release(struct file *file)
{
	struct video_device *vdev = video_devdata(file);

	if (vdev->queue)
		return vb2_fop_release(file);
	return v4l2_fh_release(file);
}

static const struct v4l2_file_operations rkooc_cap_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = rkooc_cap_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

// ioctl ops

static int rkooc_cap_create_devnode(struct rkooc_dev *dev)
{
	struct video_device *vfd;
	int ret;

	vfd = &dev->vid_cap_dev;
	snprintf(vfd->name, sizeof(vfd->name), "rk-irfpaisp-result");
	vfd->fops = &rkooc_cap_fops;
	vfd->ioctl_ops = &irfpa_ioctl_ops;
	vfd->device_caps = dev->vid_cap_caps;
	vfd->release = video_device_release_empty;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->queue = &dev->irfpa_tx_queue;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	vfd->lock = &dev->mutex;
	video_set_drvdata(vfd, dev);

#ifdef CONFIG_MEDIA_CONTROLLER
	dev->vid_cap_pad.flags = MEDIA_PAD_FL_SINK;
	ret = media_entity_pads_init(&vfd->entity, 1, &dev->vid_cap_pad);
	if (ret)
		return ret;
#endif

	ret = video_register_device(vfd, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		media_entity_cleanup(&vfd->entity);
		return ret;
	}
	v4l2_info(&dev->v4l2_dev, "IrfpaIsp out device registered as %s\n",
		  video_device_node_name(vfd));
	return 0;
}

int rkooc_create_cap_dev(struct rkooc_dev *dev)
{
	int ret = 0;

	/* set the capabilities of the video capture device */
	dev->vid_cap_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	/* set up the queue for the video capture device */
	INIT_LIST_HEAD(&dev->vid_cap_active);

	ret = rkooc_cap_init_queue(dev);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "rkooc_cap_init_queue failed\n");
		goto done;
	}

	ret = rkooc_cap_create_devnode(dev);
	if (ret)
		goto done;

done:
	return ret;
}

void rkooc_remove_cap_dev(struct rkooc_dev *dev)
{
	media_entity_cleanup(&dev->vid_cap_dev.entity);
	vb2_video_unregister_device(&dev->vid_cap_dev);
}
