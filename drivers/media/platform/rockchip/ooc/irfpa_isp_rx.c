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

static int irfparx_querycap(struct file *file, void *priv,
			    struct v4l2_capability *cap)
{
	struct rkooc_dev *dev = video_drvdata(file);

	strscpy(cap->driver, "rk_irfpa", sizeof(cap->driver));
	strscpy(cap->card, "irfpa_rx", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev->v4l2_dev.name);

	cap->capabilities =
	    V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int irfparx_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
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

static const struct v4l2_ioctl_ops irfpa_rx_ioctl_ops = {
	.vidioc_querycap = irfparx_querycap,
	.vidioc_g_fmt_vid_out = irfparx_g_fmt,

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
static int rkooc_rx_queue_setup(struct vb2_queue *vq,
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

static int rkooc_rx_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkooc_buffer *buf = container_of(vbuf, struct rkooc_buffer, vb);
	struct sg_table *sgt = vb2_dma_sg_plane_desc(vb, 0);

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->vaddr = (uint8_t *) vb2_plane_vaddr(&buf->vb.vb2_buf, 0);
	buf->size = vb2_plane_size(&buf->vb.vb2_buf, 0);

	return 0;
}

static int rkooc_rx_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkooc_buffer *buf = container_of(vbuf, struct rkooc_buffer, vb);

	vb2_set_plane_payload(vb, 0, buf->size);
	vb->planes[0].data_offset = 0;
	return 0;
}

static void rkooc_rx_buf_queue(struct vb2_buffer *vb)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkooc_buffer *buf = container_of(vbuf, struct rkooc_buffer, vb);

	vbuf->sequence = dev->irfpa_rx_seq++;

	spin_lock(&dev->irfpa_lock);
	if (!list_empty(&dev->vid_cap_active)) {
		struct rkooc_buffer *outbuf =
		    list_entry(dev->vid_cap_active.next,
			       struct rkooc_buffer, list);

		list_del(&outbuf->list);
		spin_unlock(&dev->irfpa_lock);

		memcpy(outbuf->vaddr, buf->vaddr, buf->size);
		outbuf->vb.sequence = vbuf->sequence;
		outbuf->vb.vb2_buf.timestamp = ktime_get_boottime_ns();
		vb2_buffer_done(&outbuf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	} else {
		spin_unlock(&dev->irfpa_lock);
	}
	vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);

}

static int rkooc_rx_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vq);

	dev->irfpa_rx_seq = 0;
	return 0;
}

static void rkooc_rx_stop_streaming(struct vb2_queue *vq)
{
	struct rkooc_dev *dev = vb2_get_drv_priv(vq);

	spin_lock(&dev->irfpa_lock);
	while (!list_empty(&dev->irfpa_rx_buffers)) {
		struct rkooc_buffer *buf;

		buf = list_entry(dev->irfpa_rx_buffers.next,
				 struct rkooc_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	INIT_LIST_HEAD(&dev->irfpa_rx_buffers);
	spin_unlock(&dev->irfpa_lock);

	dev->irfpa_rx_seq = 0;
	dev->cur_buf = NULL;
}

static const struct vb2_ops irfpa_rx_qops = {
	.queue_setup = rkooc_rx_queue_setup,
	.buf_init = rkooc_rx_buf_init,
	.buf_prepare = rkooc_rx_buf_prepare,
	.buf_queue = rkooc_rx_buf_queue,
	.start_streaming = rkooc_rx_start_streaming,
	.stop_streaming = rkooc_rx_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

static int rkooc_rx_init_queue(struct rkooc_dev *dev)
{
	struct vb2_queue *q = &dev->irfpa_rx_queue;

	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	q->io_modes = VB2_MMAP | VB2_DMABUF;
	q->drv_priv = dev;
	q->buf_struct_size = sizeof(struct rkooc_buffer);
	q->ops = &irfpa_rx_qops;
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
static int rkooc_rx_fop_release(struct file *file)
{
	struct video_device *vdev = video_devdata(file);

	if (vdev->queue)
		return vb2_fop_release(file);
	return v4l2_fh_release(file);
}

static const struct v4l2_file_operations rkooc_rx_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = rkooc_rx_fop_release,
	.poll = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

// ioctl ops
static int rkooc_rx_create_devnode(struct rkooc_dev *dev)
{
	struct video_device *vfd;
	int ret;

	vfd = &dev->irfpa_rx_dev;
	snprintf(vfd->name, sizeof(vfd->name), "rk-irfpaisp-rx");
	vfd->vfl_dir = VFL_DIR_TX;
	vfd->fops = &rkooc_rx_fops;
	vfd->ioctl_ops = &irfpa_rx_ioctl_ops;
	vfd->device_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING;
	vfd->release = video_device_release_empty;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->queue = &dev->irfpa_rx_queue;

	/*
	 * Provide a mutex to v4l2 core. It will be used to protect
	 * all fops and v4l2 ioctls.
	 */
	vfd->lock = &dev->mutex;
	video_set_drvdata(vfd, dev);

#ifdef CONFIG_MEDIA_CONTROLLER
	dev->vid_rx_pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&vfd->entity, 1, &dev->vid_rx_pad);
	if (ret)
		return ret;
#endif

	ret = video_register_device(vfd, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		media_entity_cleanup(&vfd->entity);
		return ret;
	}
	v4l2_info(&dev->v4l2_dev, "IrfpaIsp rx device registered as %s\n",
		  video_device_node_name(vfd));
	return 0;
}

int rkooc_create_rx_dev(struct rkooc_dev *dev)
{
	int ret = 0;

	INIT_LIST_HEAD(&dev->irfpa_rx_buffers);

	ret = rkooc_rx_init_queue(dev);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, " failed\n");
		goto done;
	}

	ret = rkooc_rx_create_devnode(dev);
	if (ret)
		goto done;

done:
	return ret;
}

void rkooc_remove_rx_dev(struct rkooc_dev *dev)
{
	media_entity_cleanup(&dev->irfpa_rx_dev.entity);
	vb2_video_unregister_device(&dev->irfpa_rx_dev);
}
