// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>
#include "dev.h"
#include "regs.h"
#include "isp_pdaf.h"

static int rkisp_pdaf_enum_fmt(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
	if (f->index > 0)
		return -EINVAL;

	f->pixelformat = V4l2_PIX_FMT_SPD16;
	strscpy(f->description, "Shield pix data 16-bit",
		sizeof(f->description));
	return 0;
}

static int rkisp_pdaf_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct video_device *vdev = video_devdata(file);
	struct rkisp_pdaf_vdev *pdaf_vdev = video_get_drvdata(vdev);

	f->fmt.pix_mp = pdaf_vdev->fmt;
	return 0;
}

static int rkisp_pdaf_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct video_device *vdev = video_devdata(file);
	struct rkisp_pdaf_vdev *pdaf_vdev = video_get_drvdata(vdev);
	struct v4l2_pix_format_mplane *pixm;
	u32 bytesperline;

	if (!f)
		return -EINVAL;

	if (vb2_is_streaming(&pdaf_vdev->vnode.buf_queue)) {
		v4l2_err(vdev->v4l2_dev, "%s queue busy\n", __func__);
		return -EBUSY;
	}
	pixm = &f->fmt.pix_mp;
	pixm->num_planes = 1;
	pixm->field = V4L2_FIELD_NONE;
	pixm->pixelformat = V4l2_PIX_FMT_SPD16;
	bytesperline = ALIGN(pixm->width, 8) * 2;
	if (pixm->plane_fmt[0].bytesperline > bytesperline)
		bytesperline = ALIGN(pixm->plane_fmt[0].bytesperline, 16);
	pixm->plane_fmt[0].bytesperline = bytesperline;
	pixm->plane_fmt[0].sizeimage = bytesperline * pixm->height;
	pdaf_vdev->fmt = *pixm;
	return 0;
}

static int rkisp_pdaf_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
	struct video_device *vdev = video_devdata(file);
	struct rkisp_pdaf_vdev *pdaf_vdev = video_get_drvdata(vdev);
	struct device *dev = pdaf_vdev->dev->dev;

	strscpy(cap->card, vdev->name, sizeof(cap->card));
	snprintf(cap->driver, sizeof(cap->driver),
		 "%s_v%d", dev->driver->name,
		 pdaf_vdev->dev->isp_ver >> 4);
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev_name(dev));
	cap->version = RKISP_DRIVER_VERSION;
	return 0;
}

/* ISP video device IOCTLs */
static const struct v4l2_ioctl_ops rkisp_pdaf_ioctl = {
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_enum_fmt_vid_cap = rkisp_pdaf_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = rkisp_pdaf_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = rkisp_pdaf_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = rkisp_pdaf_s_fmt,
	.vidioc_querycap = rkisp_pdaf_querycap
};

static int rkisp_pdaf_fh_open(struct file *file)
{
	struct rkisp_pdaf_vdev *pdaf_vdev = video_drvdata(file);
	int ret;

	if (!pdaf_vdev->dev->is_probe_end)
		return -EINVAL;

	ret = v4l2_fh_open(file);
	if (!ret) {
		ret = v4l2_pipeline_pm_get(&pdaf_vdev->vnode.vdev.entity);
		if (ret < 0)
			vb2_fop_release(file);
	}

	return ret;
}

static int rkisp_pdaf_fop_release(struct file *file)
{
	struct rkisp_pdaf_vdev *pdaf_vdev = video_drvdata(file);
	int ret;

	ret = vb2_fop_release(file);
	if (!ret)
		v4l2_pipeline_pm_put(&pdaf_vdev->vnode.vdev.entity);
	return ret;
}

struct v4l2_file_operations rkisp_pdaf_fops = {
	.mmap = vb2_fop_mmap,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.open = rkisp_pdaf_fh_open,
	.release = rkisp_pdaf_fop_release
};

static int rkisp_pdaf_queue_setup(struct vb2_queue *vq,
				  unsigned int *num_buffers,
				  unsigned int *num_planes,
				  unsigned int sizes[],
				  struct device *alloc_ctxs[])
{
	struct rkisp_pdaf_vdev *pdaf_vdev = vq->drv_priv;
	struct rkisp_device *dev = pdaf_vdev->dev;

	if (!pdaf_vdev->fmt.plane_fmt[0].sizeimage)
		return -EINVAL;
	*num_planes = 1;
	sizes[0] = pdaf_vdev->fmt.plane_fmt[0].sizeimage;
	v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev, "%s count %d, size %d\n",
		 pdaf_vdev->vnode.vdev.name, *num_buffers, sizes[0]);
	return 0;
}

static void rkisp_pdaf_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkisp_buffer *buf = to_rkisp_buffer(vbuf);
	struct vb2_queue *vq = vb->vb2_queue;
	struct rkisp_pdaf_vdev *pdaf_vdev = vq->drv_priv;
	struct rkisp_device *dev = pdaf_vdev->dev;
	unsigned long lock_flags = 0;
	struct sg_table *sgt;

	if (dev->hw_dev->is_dma_sg_ops) {
		sgt = vb2_dma_sg_plane_desc(vb, 0);
		buf->buff_addr[0] = sg_dma_address(sgt->sgl);
	} else {
		buf->buff_addr[0] = vb2_dma_contig_plane_dma_addr(vb, 0);
	}
	v4l2_dbg(2, rkisp_debug, &dev->v4l2_dev,
		 "pdaf queue buf:0x%x\n", buf->buff_addr[0]);
	spin_lock_irqsave(&pdaf_vdev->vbq_lock, lock_flags);
	list_add_tail(&buf->queue, &pdaf_vdev->buf_queue);
	spin_unlock_irqrestore(&pdaf_vdev->vbq_lock, lock_flags);
}

static void rkisp_pdaf_stop_streaming(struct vb2_queue *vq)
{
	struct rkisp_pdaf_vdev *pdaf_vdev = vq->drv_priv;
	struct rkisp_device *dev = pdaf_vdev->dev;
	struct rkisp_buffer *buf;
	unsigned long flags = 0;
	bool is_wait = dev->hw_dev->is_shutdown ? false : true;

	if (!pdaf_vdev->streaming)
		return;
	v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev,
		 "%s state:0x%x\n", __func__, dev->isp_state);
	pdaf_vdev->stopping = false;
	if (dev->hw_dev->is_single)
		rkisp_clear_bits(dev, ISP39_W3A_CTRL0, ISP39_W3A_PDAF_EN, false);
	if (IS_HDR_RDBK(dev->rd_mode)) {
		spin_lock_irqsave(&dev->hw_dev->rdbk_lock, flags);
		if (dev->hw_dev->cur_dev_id != dev->dev_id || dev->hw_dev->is_idle) {
			is_wait = false;
			rkisp_clear_bits(dev, ISP39_W3A_CTRL0, ISP39_W3A_PDAF_EN, false);
		}
		spin_unlock_irqrestore(&dev->hw_dev->rdbk_lock, flags);
	}
	if (is_wait && (rkisp_read(dev, ISP39_W3A_CTRL0, false) & ISP39_W3A_PDAF_EN)) {
		int ret = wait_event_timeout(pdaf_vdev->done,
					     !pdaf_vdev->streaming, msecs_to_jiffies(200));

		if (!ret) {
			rkisp_clear_bits(dev, ISP39_W3A_CTRL0, ISP39_W3A_PDAF_EN, false);
			v4l2_warn(&dev->v4l2_dev, "%s timeout\n", __func__);
		}
	}
	pdaf_vdev->streaming = false;
	pdaf_vdev->stopping = false;
	spin_lock_irqsave(&pdaf_vdev->vbq_lock, flags);
	if (pdaf_vdev->curr_buf) {
		list_add_tail(&pdaf_vdev->curr_buf->queue, &pdaf_vdev->buf_queue);
		if (pdaf_vdev->curr_buf == pdaf_vdev->next_buf)
			pdaf_vdev->next_buf = NULL;
		pdaf_vdev->curr_buf = NULL;
	}
	if (pdaf_vdev->next_buf) {
		list_add_tail(&pdaf_vdev->next_buf->queue, &pdaf_vdev->buf_queue);
		pdaf_vdev->next_buf = NULL;
	}
	while (!list_empty(&pdaf_vdev->buf_queue)) {
		buf = list_first_entry(&pdaf_vdev->buf_queue,
				       struct rkisp_buffer, queue);
		list_del(&buf->queue);
		buf->vb.vb2_buf.synced = false;
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	while (!list_empty(&pdaf_vdev->buf_done_list)) {
		buf = list_first_entry(&pdaf_vdev->buf_done_list,
				       struct rkisp_buffer, queue);
		list_del(&buf->queue);
		buf->vb.vb2_buf.synced = false;
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	spin_unlock_irqrestore(&pdaf_vdev->vbq_lock, flags);
	tasklet_disable(&pdaf_vdev->buf_done_tasklet);
}

static int rkisp_pdaf_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct rkisp_pdaf_vdev *pdaf_vdev = vq->drv_priv;
	struct rkisp_device *dev = pdaf_vdev->dev;
	u32 val;

	if (pdaf_vdev->streaming)
		return -EBUSY;
	v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev,
		 "%s cnt:%d\n", __func__, count);
	val = pdaf_vdev->fmt.plane_fmt[0].bytesperline;
	if (dev->isp_ver == ISP_V35)
		val = 512;
	rkisp_write(dev, ISP39_W3A_CTRL1, val, false);
	pdaf_vdev->streaming = true;
	tasklet_enable(&pdaf_vdev->buf_done_tasklet);
	return 0;
}

static const struct vb2_ops rkisp_pdaf_vb2_ops = {
	.queue_setup = rkisp_pdaf_queue_setup,
	.buf_queue = rkisp_pdaf_buf_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.stop_streaming = rkisp_pdaf_stop_streaming,
	.start_streaming = rkisp_pdaf_start_streaming,
};

static int rkisp_pdaf_init_vb2_queue(struct vb2_queue *q, struct rkisp_pdaf_vdev *pdaf_vdev)
{
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->drv_priv = pdaf_vdev;
	q->ops = &rkisp_pdaf_vb2_ops;
	q->mem_ops = pdaf_vdev->dev->hw_dev->mem_ops;
	q->buf_struct_size = sizeof(struct rkisp_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &pdaf_vdev->api_lock;
	q->dev = pdaf_vdev->dev->hw_dev->dev;
	q->min_buffers_needed = 1;
	q->allow_cache_hints = 1;
	q->bidirectional = 1;
	if (pdaf_vdev->dev->hw_dev->is_dma_contig)
		q->dma_attrs = DMA_ATTR_FORCE_CONTIGUOUS;
	q->gfp_flags = GFP_DMA32;
	return vb2_queue_init(q);
}

static void rkisp_pdaf_buf_done_task(unsigned long arg)
{
	struct rkisp_pdaf_vdev *pdaf_vdev = (struct rkisp_pdaf_vdev *)arg;
	struct rkisp_buffer *buf = NULL;
	unsigned long flags = 0;
	LIST_HEAD(local_list);

	spin_lock_irqsave(&pdaf_vdev->vbq_lock, flags);
	list_replace_init(&pdaf_vdev->buf_done_list, &local_list);
	spin_unlock_irqrestore(&pdaf_vdev->vbq_lock, flags);

	while (!list_empty(&local_list)) {
		buf = list_first_entry(&local_list, struct rkisp_buffer, queue);
		list_del(&buf->queue);

		v4l2_dbg(4, rkisp_debug, &pdaf_vdev->dev->v4l2_dev,
			 "pdaf seq:%d buf:0x%x done\n",
			 buf->vb.sequence, buf->buff_addr[0]);
		vb2_buffer_done(&buf->vb.vb2_buf,
				pdaf_vdev->streaming ? VB2_BUF_STATE_DONE : VB2_BUF_STATE_ERROR);
	}
}

void rkisp_pdaf_update_buf(struct rkisp_device *dev)
{
	struct rkisp_pdaf_vdev *pdaf_vdev = dev->pdaf_vdev;
	struct rkisp_buffer *buf = NULL;
	unsigned long flags = 0;
	u32 val;

	if (!pdaf_vdev)
		return;

	spin_lock_irqsave(&pdaf_vdev->vbq_lock, flags);
	if (!pdaf_vdev->next_buf && !list_empty(&pdaf_vdev->buf_queue)) {
		buf = list_first_entry(&pdaf_vdev->buf_queue,
				       struct rkisp_buffer, queue);
		list_del(&buf->queue);
		pdaf_vdev->next_buf = buf;
	}
	spin_unlock_irqrestore(&pdaf_vdev->vbq_lock, flags);

	if (pdaf_vdev->next_buf) {
		val = pdaf_vdev->next_buf->buff_addr[0];
		rkisp_write(dev, ISP39_W3A_PDAF_ADDR, val, false);
		if (!dev->hw_dev->is_single) {
			pdaf_vdev->curr_buf = pdaf_vdev->next_buf;
			pdaf_vdev->next_buf = NULL;
		}
	}
	v4l2_dbg(4, rkisp_debug, &dev->v4l2_dev,
		 "%s BASE:0x%x SHD:0x%x\n", __func__,
		 rkisp_read(dev, ISP39_W3A_PDAF_ADDR, false),
		 rkisp_read(dev, ISP39_W3A_PDAF_ADDR_SHD, true));
}

void rkisp_pdaf_isr(struct rkisp_device *dev)
{
	struct rkisp_pdaf_vdev *pdaf_vdev = dev->pdaf_vdev;
	struct rkisp_buffer *buf = NULL;
	unsigned long flags = 0;
	u32 w3a_ris;

	if (!pdaf_vdev)
		return;

	w3a_ris = rkisp_read(dev, ISP39_W3A_INT_STAT, true);
	if (w3a_ris & ISP39_W3A_INT_PDAF_OVF) {
		v4l2_err(&dev->v4l2_dev, "pdaf overflow 0x%x\n", w3a_ris);
		rkisp_write(dev, ISP39_W3A_INT_STAT, ISP39_W3A_INT_PDAF_OVF, true);
	}

	if (!(w3a_ris & ISP39_W3A_INT_PDAF))
		return;
	rkisp_write(dev, ISP39_W3A_INT_STAT, ISP39_W3A_INT_PDAF, true);

	if (pdaf_vdev->stopping) {
		pdaf_vdev->stopping = false;
		pdaf_vdev->streaming = false;
		wake_up(&pdaf_vdev->done);
		return;
	}

	spin_lock_irqsave(&pdaf_vdev->vbq_lock, flags);
	if (pdaf_vdev->curr_buf) {
		buf = pdaf_vdev->curr_buf;
		pdaf_vdev->curr_buf = NULL;
	}
	if (pdaf_vdev->next_buf) {
		pdaf_vdev->curr_buf = pdaf_vdev->next_buf;
		pdaf_vdev->next_buf = NULL;
	}
	spin_unlock_irqrestore(&pdaf_vdev->vbq_lock, flags);
	rkisp_pdaf_update_buf(dev);

	if (buf) {
		struct vb2_buffer *vb2_buf = &buf->vb.vb2_buf;
		u32 size = pdaf_vdev->fmt.plane_fmt[0].sizeimage;
		u64 ns = 0;
		u32 seq = 0;

		vb2_set_plane_payload(vb2_buf, 0, size);
		rkisp_dmarx_get_frame(dev, &seq, NULL, &ns, true);
		if (!ns)
			ns = ktime_get_ns();
		buf->vb.sequence = seq;
		buf->vb.vb2_buf.timestamp = ns;
		spin_lock_irqsave(&pdaf_vdev->vbq_lock, flags);
		list_add_tail(&buf->queue, &pdaf_vdev->buf_done_list);
		spin_unlock_irqrestore(&pdaf_vdev->vbq_lock, flags);
		tasklet_schedule(&pdaf_vdev->buf_done_tasklet);
	}
}

int rkisp_register_pdaf_vdev(struct rkisp_device *dev)
{
	struct rkisp_pdaf_vdev *pdaf_vdev;
	struct rkisp_vdev_node *node;
	struct video_device *vdev;
	struct media_entity *source, *sink;
	int ret;

	if (dev->isp_ver != ISP_V39 && dev->isp_ver != ISP_V35)
		return 0;
	pdaf_vdev = kzalloc(sizeof(struct rkisp_pdaf_vdev), GFP_KERNEL);
	if (!pdaf_vdev)
		return -ENOMEM;
	pdaf_vdev->dev = dev;
	dev->pdaf_vdev = pdaf_vdev;

	node = &pdaf_vdev->vnode;
	vdev = &node->vdev;
	INIT_LIST_HEAD(&pdaf_vdev->buf_queue);
	spin_lock_init(&pdaf_vdev->vbq_lock);
	mutex_init(&pdaf_vdev->api_lock);
	init_waitqueue_head(&pdaf_vdev->done);
	strscpy(vdev->name, "rkisp-pdaf", sizeof(vdev->name));

	vdev->ioctl_ops = &rkisp_pdaf_ioctl;
	vdev->fops = &rkisp_pdaf_fops;
	vdev->release = video_device_release_empty;
	vdev->lock = &pdaf_vdev->api_lock;
	vdev->v4l2_dev = &dev->v4l2_dev;
	vdev->queue = &node->buf_queue;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
	vdev->vfl_dir =  VFL_DIR_RX;
	rkisp_pdaf_init_vb2_queue(vdev->queue, pdaf_vdev);
	video_set_drvdata(vdev, pdaf_vdev);
	node->pad.flags = MEDIA_PAD_FL_SINK;

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		v4l2_err(vdev->v4l2_dev,
			"could not register Video for Linux device\n");
		kfree(pdaf_vdev);
		dev->pdaf_vdev = NULL;
		return ret;
	}

	ret = media_entity_pads_init(&vdev->entity, 1, &node->pad);
	if (ret < 0)
		goto unreg;

	source = &dev->isp_sdev.sd.entity;
	sink = &vdev->entity;
	ret = media_create_pad_link(source, RKISP_ISP_PAD_SOURCE_STATS,
				    sink, 0, MEDIA_LNK_FL_ENABLED);
	if (ret < 0)
		goto unreg;
	INIT_LIST_HEAD(&pdaf_vdev->buf_done_list);
	tasklet_init(&pdaf_vdev->buf_done_tasklet,
		     rkisp_pdaf_buf_done_task,
		     (unsigned long)pdaf_vdev);
	tasklet_disable(&pdaf_vdev->buf_done_tasklet);
	return 0;
unreg:
	video_unregister_device(vdev);
	kfree(pdaf_vdev);
	dev->pdaf_vdev = NULL;
	return ret;
}

void rkisp_unregister_pdaf_vdev(struct rkisp_device *dev)
{
	struct rkisp_pdaf_vdev *pdaf_vdev;
	struct rkisp_vdev_node *node;
	struct video_device *vdev;

	if (!dev->pdaf_vdev)
		return;
	pdaf_vdev = dev->pdaf_vdev;
	node = &pdaf_vdev->vnode;
	vdev = &node->vdev;
	tasklet_kill(&pdaf_vdev->buf_done_tasklet);
	media_entity_cleanup(&vdev->entity);
	video_unregister_device(vdev);
	kfree(pdaf_vdev);
	dev->pdaf_vdev = NULL;
}
