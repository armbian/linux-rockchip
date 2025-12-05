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
#include <media/videobuf2-dma-sg.h>
#include "dev.h"
#include "regs.h"

static int rkooc_querycap(struct file *file, void *priv,
			  struct v4l2_capability *cap)
{
	struct rkooc_dev *dev = video_drvdata(file);

	strscpy(cap->driver, "rkooc", sizeof(cap->driver));
	strscpy(cap->card, "rkooc", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev->v4l2_dev.name);

	cap->capabilities = dev->vid_out_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int rkooc_enum_fmt(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
	if (f->index > 0)
		return -EINVAL;
	f->pixelformat = V4L2_PIX_FMT_GREY;
	return 0;
}

static int rkooc_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct rkooc_dev *dev = video_drvdata(file);
	struct v4l2_pix_format *pix = &f->fmt.pix;

	pix->width = dev->win.ooc_width;
	pix->height = dev->win.ooc_height;
	pix->pixelformat = V4L2_PIX_FMT_GREY;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = pix->width;
	pix->sizeimage = pix->height * pix->bytesperline;

	return 0;
}

static const struct v4l2_ioctl_ops rkooc_ioctl_ops = {
	.vidioc_querycap = rkooc_querycap,

	.vidioc_enum_fmt_vid_cap = rkooc_enum_fmt,
	.vidioc_g_fmt_vid_cap = rkooc_g_fmt,
	//.vidioc_s_fmt_vid_cap         = rkooc_s_fmt,
	.vidioc_enum_fmt_vid_out = rkooc_enum_fmt,
	.vidioc_g_fmt_vid_out = rkooc_g_fmt,
	//.vidioc_s_fmt_vid_out         = rkooc_s_fmt,

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
static int rkooc_out_queue_setup(struct vb2_queue *vq,
				 unsigned int *nbuffers, unsigned int *nplanes,
				 unsigned int sizes[],
				 struct device *alloc_devs[])
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vq);

	if (*nplanes == 0) {
		*nplanes = 1;
		*nbuffers = 2;
		sizes[0] = dev->win.ooc_width * dev->win.ooc_height;
		return 0;
	}

	return -EINVAL;
}

static int rkooc_out_buf_init(struct vb2_buffer *vb)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkooc_buffer *buf = container_of(vbuf, struct rkooc_buffer, vb);
	struct sg_table *sgt = vb2_dma_sg_plane_desc(vb, 0);

	buf->dma_addr = sg_dma_address(sgt->sgl);
	v4l2_info(&dev->v4l2_dev, "%s: buf %d, dmaaddr %x\n", __func__,
		  vb->index, (u32) buf->dma_addr);

	return 0;
}

static int rkooc_out_buf_prepare(struct vb2_buffer *vb)
{
	unsigned long size;

	size = vb2_plane_size(vb, 0);
	vb2_set_plane_payload(vb, 0, size);
	return 0;
}

static void rkooc_out_buf_queue(struct vb2_buffer *vb)
{
	unsigned long flags;
	struct rkooc_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkooc_buffer *buf = container_of(vbuf, struct rkooc_buffer, vb);

	vbuf->sequence = dev->vid_out_seq_count++;

	spin_lock_irqsave(&dev->slock, flags);
	list_add_tail(&buf->list, &dev->vid_out_active);
	spin_unlock_irqrestore(&dev->slock, flags);
}

static int rkooc_out_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	return 0;
}

static void rkooc_out_stop_streaming(struct vb2_queue *vq)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vq);
	unsigned long flags;

	if (dev->cur_buf)
		vb2_buffer_done(&dev->cur_buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);

	/* Release all active buffers */
	spin_lock_irqsave(&dev->slock, flags);
	while (!list_empty(&dev->vid_out_active)) {
		struct rkooc_buffer *buf;

		buf = list_entry(dev->vid_out_active.next,
				 struct rkooc_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	INIT_LIST_HEAD(&dev->vid_out_active);
	spin_unlock_irqrestore(&dev->slock, flags);

	dev->vid_out_seq_count = 0;
	dev->cur_buf = NULL;
}

static const struct vb2_ops rkooc_out_qops = {
	.queue_setup = rkooc_out_queue_setup,
	.buf_init = rkooc_out_buf_init,
	.buf_prepare = rkooc_out_buf_prepare,
	.buf_queue = rkooc_out_buf_queue,
	.start_streaming = rkooc_out_start_streaming,
	.stop_streaming = rkooc_out_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

static int rkooc_out_init_queue(struct rkooc_dev *dev)
{
	struct vb2_queue *q = &dev->vb_vid_out_q;

	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	q->io_modes = VB2_MMAP | VB2_DMABUF;
	q->drv_priv = dev;
	q->buf_struct_size = sizeof(struct rkooc_buffer);
	q->ops = &rkooc_out_qops;
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
static int rkooc_out_fop_release(struct file *file)
{
	struct video_device *vdev = video_devdata(file);

	if (vdev->queue)
		return vb2_fop_release(file);
	return v4l2_fh_release(file);
}

static const struct v4l2_file_operations rkooc_out_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = rkooc_out_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

// ioctl ops
static int rkooc_out_create_devnode(struct rkooc_dev *dev)
{
	struct video_device *vfd;
	int ret;

	vfd = &dev->vid_out_dev;
	snprintf(vfd->name, sizeof(vfd->name), "rkooc-ooctx");
	vfd->vfl_dir = VFL_DIR_TX;
	vfd->fops = &rkooc_out_fops;
	vfd->ioctl_ops = &rkooc_ioctl_ops;
	vfd->device_caps = dev->vid_out_caps;
	vfd->release = video_device_release_empty;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->queue = &dev->vb_vid_out_q;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	vfd->lock = &dev->mutex;
	video_set_drvdata(vfd, dev);

#ifdef CONFIG_MEDIA_CONTROLLER
	dev->vid_out_pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&vfd->entity, 1, &dev->vid_out_pad);
	if (ret)
		return ret;
#endif

	ret = video_register_device(vfd, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		media_entity_cleanup(&vfd->entity);
		return ret;
	}
	v4l2_info(&dev->v4l2_dev, "OOC tx device registered as %s\n",
		  video_device_node_name(vfd));
	return 0;
}

int rkooc_create_ooc_dev(struct rkooc_dev *dev)
{
	int ret = 0;

	/* set the capabilities of the video capture device */
	dev->vid_out_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING;

	/* set up the queue for the video capture device */
	INIT_LIST_HEAD(&dev->vid_out_active);

	ret = rkooc_out_init_queue(dev);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "rkooc_out_init_queue failed\n");
		goto done;
	}

	ret = rkooc_out_create_devnode(dev);

done:
	return ret;
}

void rkooc_remove_ooc_dev(struct rkooc_dev *dev)
{
	media_entity_cleanup(&dev->vid_out_dev.entity);
	vb2_video_unregister_device(&dev->vid_out_dev);
}
