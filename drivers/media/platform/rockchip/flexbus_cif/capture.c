// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip Flexbus CIF Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/iommu.h>
#include <media/v4l2-common.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-cma-sg.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>

#include "dev.h"
#include "common.h"

#define CIF_REQ_BUFS_MIN	1
#define CIF_MIN_WIDTH		64
#define CIF_MIN_HEIGHT		64
#define CIF_MAX_WIDTH		8192
#define CIF_MAX_HEIGHT		8192

#define OUTPUT_STEP_WISE	8

#define FLEXBUS_CIF_PLANE_Y		0
#define FLEXBUS_CIF_PLANE_CBCR		1
#define FLEXBUS_CIF_MAX_PLANE		3

#define STREAM_PAD_SINK		0
#define STREAM_PAD_SOURCE	1

#define CIF_TIMEOUT_FRAME_NUM	(2)

#define CIF_PCLK_DUAL_EDGE	(V4L2_MBUS_PCLK_SAMPLE_RISING |\
				 V4L2_MBUS_PCLK_SAMPLE_FALLING)

/*
 * Round up height when allocate memory so that Rockchip encoder can
 * use DMA buffer directly, though this may waste a bit of memory.
 */
#define MEMORY_ALIGN_ROUND_UP_HEIGHT		16

/* Get xsubs and ysubs for fourcc formats
 *
 * @xsubs: horizontal color samples in a 4*4 matrix, for yuv
 * @ysubs: vertical color samples in a 4*4 matrix, for yuv
 */
static int fcc_xysubs(u32 fcc, u32 *xsubs, u32 *ysubs)
{
	switch (fcc) {
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
		*xsubs = 2;
		*ysubs = 1;
		break;
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
		*xsubs = 2;
		*ysubs = 2;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct cif_output_fmt out_fmts[] = {
	{
		.fourcc = V4L2_PIX_FMT_YUYV,
		.cplanes = 2,
		.mplanes = 1,
		.bpp = { 8, 16 },
		.cif_yuv_order = CIF_OUTPUT_ORDER_YUYV,
	}, {
		.fourcc = V4L2_PIX_FMT_YVYU,
		.cplanes = 2,
		.mplanes = 1,
		.bpp = { 8, 16 },
		.cif_yuv_order = CIF_OUTPUT_ORDER_YVYU,
	}, {
		.fourcc = V4L2_PIX_FMT_UYVY,
		.cplanes = 2,
		.mplanes = 1,
		.bpp = { 8, 16 },
		.cif_yuv_order = CIF_OUTPUT_ORDER_UYVY,
	}, {
		.fourcc = V4L2_PIX_FMT_VYUY,
		.cplanes = 2,
		.mplanes = 1,
		.bpp = { 8, 16 },
		.cif_yuv_order = CIF_OUTPUT_ORDER_VYUY,
	},
	{
		.fourcc = V4L2_PIX_FMT_RGB24,
		.cplanes = 1,
		.mplanes = 1,
		.bpp = { 24 },
		.cif_yuv_order = CIF_OUTPUT_ORDER_YVYU,
	},
	{
		.fourcc = V4L2_PIX_FMT_BGR24,
		.cplanes = 1,
		.mplanes = 1,
		.bpp = { 24 },
		.cif_yuv_order = CIF_OUTPUT_ORDER_YVYU,
	}
};

static const struct cif_input_fmt in_fmts[] = {
	{
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.cif_yuv_order	= CIF_INPUT_ORDER_YUYV,
		.field		= V4L2_FIELD_NONE,
	}, {
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.cif_yuv_order	= CIF_INPUT_ORDER_YUYV,
		.field		= V4L2_FIELD_INTERLACED,
	}, {
		.mbus_code	= MEDIA_BUS_FMT_YVYU8_2X8,
		.cif_yuv_order	= CIF_INPUT_ORDER_YVYU,
		.field		= V4L2_FIELD_NONE,
	}, {
		.mbus_code	= MEDIA_BUS_FMT_YVYU8_2X8,
		.cif_yuv_order	= CIF_INPUT_ORDER_YVYU,
		.field		= V4L2_FIELD_INTERLACED,
	}, {
		.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
		.cif_yuv_order	= CIF_INPUT_ORDER_UYVY,
		.field		= V4L2_FIELD_NONE,
	}, {
		.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
		.cif_yuv_order	= CIF_INPUT_ORDER_UYVY,
		.field		= V4L2_FIELD_INTERLACED,
	}, {
		.mbus_code	= MEDIA_BUS_FMT_VYUY8_2X8,
		.cif_yuv_order	= CIF_INPUT_ORDER_VYUY,
		.field		= V4L2_FIELD_NONE,
	}, {
		.mbus_code	= MEDIA_BUS_FMT_VYUY8_2X8,
		.cif_yuv_order	= CIF_INPUT_ORDER_VYUY,
		.field		= V4L2_FIELD_INTERLACED,
	}
};

static int flexbus_cif_output_fmt_check(struct flexbus_cif_stream *stream,
					   const struct cif_output_fmt *output_fmt)
{
	const struct cif_input_fmt *input_fmt = stream->cif_fmt_in;
	int ret = -EINVAL;

	switch (input_fmt->mbus_code) {
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
		if (output_fmt->fourcc == V4L2_PIX_FMT_YUYV ||
		    output_fmt->fourcc == V4L2_PIX_FMT_YVYU ||
		    output_fmt->fourcc == V4L2_PIX_FMT_UYVY ||
		    output_fmt->fourcc == V4L2_PIX_FMT_VYUY ||
		    output_fmt->fourcc == V4L2_PIX_FMT_RGB24 ||
		    output_fmt->fourcc == V4L2_PIX_FMT_BGR24)
			ret = 0;
		break;
	default:
		break;
	}
	if (ret)
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "input mbus_code 0x%x, can't transform to %c%c%c%c\n",
			 input_fmt->mbus_code,
			 output_fmt->fourcc & 0xff,
			 (output_fmt->fourcc >> 8) & 0xff,
			 (output_fmt->fourcc >> 16) & 0xff,
			 (output_fmt->fourcc >> 24) & 0xff);
	return ret;
}

static struct v4l2_subdev *get_remote_sensor(struct flexbus_cif_stream *stream, u16 *index)
{
	struct media_pad *local, *remote;
	struct media_entity *sensor_me;
	struct v4l2_subdev *sub = NULL;

	local = &stream->vnode.vdev.entity.pads[0];
	if (!local) {
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "%s: video pad[0] is null\n", __func__);
		return NULL;
	}

	remote = media_pad_remote_pad_first(local);
	if (!remote) {
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "%s: remote pad is null\n", __func__);
		return NULL;
	}

	if (index)
		*index = remote->index;

	sensor_me = remote->entity;

	sub = media_entity_to_v4l2_subdev(sensor_me);

	return sub;

}

static void get_remote_terminal_sensor(struct flexbus_cif_stream *stream,
				       struct v4l2_subdev **sensor_sd)
{
	struct media_graph graph;
	struct media_entity *entity = &stream->vnode.vdev.entity;
	struct media_device *mdev = entity->graph_obj.mdev;
	int ret;

	/* Walk the graph to locate sensor nodes. */
	mutex_lock(&mdev->graph_mutex);
	ret = media_graph_walk_init(&graph, mdev);
	if (ret) {
		mutex_unlock(&mdev->graph_mutex);
		*sensor_sd = NULL;
		return;
	}

	media_graph_walk_start(&graph, entity);
	while ((entity = media_graph_walk_next(&graph))) {
		if (entity->function == MEDIA_ENT_F_CAM_SENSOR)
			break;
	}
	mutex_unlock(&mdev->graph_mutex);
	media_graph_walk_cleanup(&graph);

	if (entity)
		*sensor_sd = media_entity_to_v4l2_subdev(entity);
	else
		*sensor_sd = NULL;
}

static struct flexbus_cif_sensor_info *sd_to_sensor(struct flexbus_cif_device *dev,
						       struct v4l2_subdev *sd)
{
	u32 i;

	for (i = 0; i < dev->num_sensors; ++i)
		if (dev->sensors[i].sd == sd)
			return &dev->sensors[i];

	return NULL;
}

static const struct
cif_input_fmt *get_input_fmt(struct v4l2_subdev *sd, struct v4l2_rect *rect,
			     u32 pad_id)
{
	struct v4l2_subdev_format fmt;
	int ret;
	u32 i;

	fmt.pad = 0;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.reserved[0] = 0;
	fmt.format.field = V4L2_FIELD_NONE;
	ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
	if (ret < 0) {
		v4l2_warn(sd->v4l2_dev,
			  "sensor fmt invalid, set to default size\n");
		goto set_default;
	}

	v4l2_dbg(1, flexbus_cif_debug, sd->v4l2_dev,
		 "remote fmt: mbus code:0x%x, size:%dx%d, field: %d\n",
		 fmt.format.code, fmt.format.width,
		 fmt.format.height, fmt.format.field);
	rect->left = 0;
	rect->top = 0;
	rect->width = fmt.format.width;
	rect->height = fmt.format.height;

	for (i = 0; i < ARRAY_SIZE(in_fmts); i++)
		if (fmt.format.code == in_fmts[i].mbus_code &&
		    fmt.format.field == in_fmts[i].field)
			return &in_fmts[i];

	v4l2_err(sd->v4l2_dev, "remote sensor mbus code not supported\n");

set_default:
	rect->left = 0;
	rect->top = 0;
	rect->width = FLEXBUS_CIF_DEFAULT_WIDTH;
	rect->height = FLEXBUS_CIF_DEFAULT_HEIGHT;

	return NULL;
}

static const struct
cif_output_fmt *flexbus_cif_find_output_fmt(struct flexbus_cif_stream *stream, u32 pixelfmt)
{
	const struct cif_output_fmt *fmt;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(out_fmts); i++) {
		fmt = &out_fmts[i];
		if (fmt->fourcc == pixelfmt)
			return fmt;
	}

	return NULL;
}

static void flexbus_cif_assign_new_buffer_init(struct flexbus_cif_stream *stream,
						  int channel_id)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	u32 frm0_addr;
	u32 frm1_addr;
	unsigned long flags;
	struct flexbus_cif_dummy_buffer *dummy_buf = &dev->dummy_buf;

	frm0_addr = FLEXBUS_DMA_DST_ADDR0;
	frm1_addr = FLEXBUS_DMA_DST_ADDR1;

	spin_lock_irqsave(&stream->vbq_lock, flags);

	if (!stream->curr_buf) {
		if (!list_empty(&stream->buf_head)) {
			stream->curr_buf = list_first_entry(&stream->buf_head,
							    struct flexbus_cif_buffer,
							    queue);
			list_del(&stream->curr_buf->queue);
		}
	}

	if (stream->curr_buf) {
		flexbus_cif_write_register(dev, frm0_addr,
				     stream->curr_buf->buff_addr[FLEXBUS_CIF_PLANE_Y] >> 2);
	} else {
		if (dummy_buf->vaddr) {
			flexbus_cif_write_register(dev, frm0_addr, dummy_buf->dma_addr);
		} else {
			if (stream->lack_buf_cnt < 2)
				stream->lack_buf_cnt++;
		}
	}


	if (!stream->next_buf) {
		if (!list_empty(&stream->buf_head)) {
			stream->next_buf = list_first_entry(&stream->buf_head,
							    struct flexbus_cif_buffer, queue);
			list_del(&stream->next_buf->queue);
		}
	}

	if (stream->next_buf) {
		flexbus_cif_write_register(dev, frm1_addr,
				     stream->next_buf->buff_addr[FLEXBUS_CIF_PLANE_Y] >> 2);
	} else {
		if (dummy_buf->vaddr) {
			flexbus_cif_write_register(dev, frm1_addr, dummy_buf->dma_addr);
		} else {
			if (stream->curr_buf) {
				stream->next_buf = stream->curr_buf;
				flexbus_cif_write_register(dev, frm1_addr,
						     stream->next_buf->buff_addr[FLEXBUS_CIF_PLANE_Y] >> 2);
			}
			if (stream->lack_buf_cnt < 2)
				stream->lack_buf_cnt++;
		}
	}
	spin_unlock_irqrestore(&stream->vbq_lock, flags);

}

static int flexbus_cif_assign_new_buffer_update(struct flexbus_cif_stream *stream,
						   int channel_id)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct flexbus_cif_dummy_buffer *dummy_buf = &dev->dummy_buf;
	struct flexbus_cif_buffer *buffer = NULL;
	u32 frm_addr;
	int ret = 0;
	unsigned long flags;

	frm_addr = stream->frame_phase & CIF_CSI_FRAME0_READY ?
		FLEXBUS_DMA_DST_ADDR0 : FLEXBUS_DMA_DST_ADDR1;

	spin_lock_irqsave(&stream->vbq_lock, flags);
	if (!list_empty(&stream->buf_head)) {
		if (stream->frame_phase == CIF_CSI_FRAME0_READY) {
			if (!stream->curr_buf)
				ret = -EINVAL;
			stream->curr_buf = list_first_entry(&stream->buf_head,
							    struct flexbus_cif_buffer, queue);
			if (stream->curr_buf) {
				list_del(&stream->curr_buf->queue);
				buffer = stream->curr_buf;
				v4l2_dbg(3, flexbus_cif_debug, &dev->v4l2_dev,
					 "stream[%d] update curr_buf 0x%x\n",
					 stream->id, buffer->buff_addr[0]);
			}
		} else if (stream->frame_phase == CIF_CSI_FRAME1_READY) {
			if (!stream->next_buf)
				ret = -EINVAL;
			stream->next_buf = list_first_entry(&stream->buf_head,
							    struct flexbus_cif_buffer, queue);
			if (stream->next_buf) {
				list_del(&stream->next_buf->queue);
				buffer = stream->next_buf;
				v4l2_dbg(3, flexbus_cif_debug, &dev->v4l2_dev,
					 "stream[%d] update next_buf 0x%x\n",
					 stream->id, buffer->buff_addr[0]);
			}
		}
	} else  {
		buffer = NULL;
		if (dummy_buf->vaddr) {
			if (stream->frame_phase == CIF_CSI_FRAME0_READY) {
				stream->curr_buf  = NULL;
			} else if (stream->frame_phase == CIF_CSI_FRAME1_READY) {
				if (stream->cif_fmt_in->field == V4L2_FIELD_INTERLACED) {
					stream->next_buf = stream->curr_buf;
					buffer = stream->next_buf;
				} else {
					stream->next_buf = NULL;
				}
			}
		} else if (stream->curr_buf && stream->next_buf &&
			   stream->curr_buf != stream->next_buf) {
			if (stream->frame_phase == CIF_CSI_FRAME0_READY) {
				stream->curr_buf = stream->next_buf;
				buffer = stream->next_buf;
			} else if (stream->frame_phase == CIF_CSI_FRAME1_READY) {
				stream->next_buf = stream->curr_buf;
				buffer = stream->curr_buf;
			}
			if (stream->lack_buf_cnt < 2)
				stream->lack_buf_cnt++;
		} else {
			stream->curr_buf = NULL;
			stream->next_buf = NULL;
			if (stream->lack_buf_cnt < 2)
				stream->lack_buf_cnt++;
		}
	}
	stream->frame_phase_cache = stream->frame_phase;

	if (buffer) {
		flexbus_cif_write_register(dev, frm_addr,
					      buffer->buff_addr[FLEXBUS_CIF_PLANE_Y] >> 2);
	} else {
		if (dummy_buf->vaddr) {
			flexbus_cif_write_register(dev, frm_addr, dummy_buf->dma_addr >> 2);
			dev->err_state |= (FLEXBUS_CIF_ERR_ID0_NOT_BUF << stream->id);
			dev->irq_stats.not_active_buf_cnt[stream->id]++;
		} else {
			ret = -EINVAL;
			stream->curr_buf = NULL;
			stream->next_buf = NULL;
			dev->err_state |= (FLEXBUS_CIF_ERR_ID0_NOT_BUF << stream->id);
			dev->irq_stats.not_active_buf_cnt[stream->id]++;
		}
	}
	spin_unlock_irqrestore(&stream->vbq_lock, flags);
	return ret;
}

static int flexbus_cif_assign_new_buffer_pingpong(struct flexbus_cif_stream *stream,
						     int init, int channel_id)
{
	int ret = 0;

	if (init)
		flexbus_cif_assign_new_buffer_init(stream, channel_id);
	else
		ret = flexbus_cif_assign_new_buffer_update(stream, channel_id);
	return ret;
}

static void flexbus_cif_stream_stop(struct flexbus_cif_stream *stream)
{
	struct flexbus_cif_device *cif_dev = stream->cif_dev;

	if (atomic_read(&cif_dev->pipe.stream_cnt) == 1)
		flexbus_cif_write_register(cif_dev, FLEXBUS_ENR, (BIT(1) << 16));

	stream->state = FLEXBUS_CIF_STATE_READY;

}

static int flexbus_cif_queue_setup(struct vb2_queue *queue,
				      unsigned int *num_buffers,
				      unsigned int *num_planes,
				      unsigned int sizes[],
				      struct device *alloc_ctxs[])
{
	struct flexbus_cif_stream *stream = queue->drv_priv;
	struct flexbus_cif_device *dev = stream->cif_dev;
	const struct v4l2_pix_format_mplane *pixm = NULL;
	const struct cif_output_fmt *cif_fmt;
	u32 i, height;

	pixm = &stream->pixm;
	cif_fmt = stream->cif_fmt_out;

	*num_planes = cif_fmt->mplanes;

	if (stream->crop_enable)
		height = stream->crop[CROP_SRC_ACT].height;
	else
		height = pixm->height;

	for (i = 0; i < cif_fmt->mplanes; i++) {
		const struct v4l2_plane_pix_format *plane_fmt;
		int h = round_up(height, MEMORY_ALIGN_ROUND_UP_HEIGHT);

		plane_fmt = &pixm->plane_fmt[i];
		sizes[i] = plane_fmt->sizeimage / height * h;
	}

	v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev, "%s count %d, size %d\n",
		 v4l2_type_names[queue->type], *num_buffers, sizes[0]);

	return 0;
}

static void flexbus_cif_check_buffer_update_pingpong(struct flexbus_cif_stream *stream,
							int channel_id)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct flexbus_cif_buffer *buffer = NULL;
	u32 frm_addr = 0;
	u32 frm0_addr = 0;
	u32 frm1_addr = 0;
	unsigned long flags;
	int frame_phase = 0;
	bool is_dual_update_buf = false;

	spin_lock_irqsave(&stream->vbq_lock, flags);
	if (stream->state == FLEXBUS_CIF_STATE_STREAMING &&
	    (stream->curr_buf == NULL ||
	     stream->next_buf == NULL)) {
		frame_phase = stream->frame_phase_cache;
		frm0_addr = FLEXBUS_DMA_DST_ADDR0;
		frm1_addr = FLEXBUS_DMA_DST_ADDR1;
		if (frame_phase & CIF_CSI_FRAME0_READY)
			frm_addr = frm0_addr;
		else
			frm_addr = frm1_addr;
		if (stream->curr_buf == NULL && stream->next_buf == NULL)
			is_dual_update_buf = true;
		if (!list_empty(&stream->buf_head)) {
			if (frame_phase == CIF_CSI_FRAME0_READY) {
				stream->curr_buf = list_first_entry(&stream->buf_head,
								    struct flexbus_cif_buffer, queue);
				if (stream->curr_buf) {
					list_del(&stream->curr_buf->queue);
					buffer = stream->curr_buf;
				}
				if (buffer && is_dual_update_buf)
					stream->next_buf = buffer;
			} else if (frame_phase == CIF_CSI_FRAME1_READY) {
				stream->next_buf = list_first_entry(&stream->buf_head,
							    struct flexbus_cif_buffer, queue);
				if (stream->next_buf) {
					list_del(&stream->next_buf->queue);
					buffer = stream->next_buf;
				}
				if (buffer && is_dual_update_buf)
					stream->curr_buf = buffer;
			}
		} else {
			v4l2_info(&dev->v4l2_dev, "%s %d\n", __func__, __LINE__);
		}
		if (buffer) {
			if (is_dual_update_buf) {
				flexbus_cif_write_register(dev, frm0_addr,
						     buffer->buff_addr[FLEXBUS_CIF_PLANE_Y]);
				flexbus_cif_write_register(dev, frm1_addr,
						     buffer->buff_addr[FLEXBUS_CIF_PLANE_Y]);
			} else {
				flexbus_cif_write_register(dev, frm_addr,
						     buffer->buff_addr[FLEXBUS_CIF_PLANE_Y]);
			}
		}
		v4l2_dbg(3, flexbus_cif_debug, &stream->cif_dev->v4l2_dev,
			 "%s, stream[%d] update buffer, frame_phase %d, lack_buf_cnt %d\n",
			 __func__, stream->id, frame_phase,
			 stream->lack_buf_cnt);
		if (stream->lack_buf_cnt)
			stream->lack_buf_cnt--;
	} else {
		v4l2_info(&dev->v4l2_dev, "%s %d, state %d, curr_buf %p, next_buf %p\n",
			  __func__, __LINE__, stream->state, stream->curr_buf, stream->next_buf);
	}
	spin_unlock_irqrestore(&stream->vbq_lock, flags);
}

/*
 * The vb2_buffer are stored in flexbus_cif_buffer, in order to unify
 * mplane buffer and none-mplane buffer.
 */
static void flexbus_cif_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct flexbus_cif_buffer *cifbuf = to_flexbus_cif_buffer(vbuf);
	struct vb2_queue *queue = vb->vb2_queue;
	struct flexbus_cif_stream *stream = queue->drv_priv;
	const struct cif_output_fmt *fmt = stream->cif_fmt_out;
	unsigned long flags;
	int i;

	memset(cifbuf->buff_addr, 0, sizeof(cifbuf->buff_addr));
	/* If mplanes > 1, every c-plane has its own m-plane,
	 * otherwise, multiple c-planes are in the same m-plane
	 */
	for (i = 0; i < fmt->mplanes; i++) {
		if (stream->cif_dev->is_dma_sg_ops) {
			struct sg_table *sgt = vb2_dma_sg_plane_desc(vb, i);

			cifbuf->buff_addr[i] = sg_dma_address(sgt->sgl);
		} else {
			cifbuf->buff_addr[i] = vb2_dma_contig_plane_dma_addr(vb, i);
		}
	}

	spin_lock_irqsave(&stream->vbq_lock, flags);
	list_add_tail(&cifbuf->queue, &stream->buf_head);
	spin_unlock_irqrestore(&stream->vbq_lock, flags);
	if (stream->lack_buf_cnt)
		flexbus_cif_check_buffer_update_pingpong(stream, stream->id);
	v4l2_dbg(3, flexbus_cif_debug, &stream->cif_dev->v4l2_dev,
		 "stream[%d] buf queue, index: %d, dma_addr 0x%x\n",
		 stream->id, vb->index, cifbuf->buff_addr[0]);
	atomic_inc(&stream->buf_cnt);
}

static int flexbus_cif_create_dummy_buf(struct flexbus_cif_stream *stream)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct flexbus_cif_dummy_buffer *dummy_buf = &dev->dummy_buf;
	int ret = 0;

	if (stream->cif_fmt_out->fourcc == V4L2_PIX_FMT_RGB24 ||
	    stream->cif_fmt_out->fourcc == V4L2_PIX_FMT_BGR24)
		dummy_buf->size = stream->pixm.width * stream->pixm.height * 3;
	else
		dummy_buf->size = stream->pixm.width * stream->pixm.height * 2;

	dummy_buf->is_need_vaddr = true;
	dummy_buf->is_need_dbuf = true;
	ret = flexbus_cif_alloc_buffer(dev, dummy_buf);
	if (ret) {
		v4l2_err(&dev->v4l2_dev,
			 "Failed to allocate the memory for dummy buffer\n");
		return -ENOMEM;
	}

	v4l2_info(&dev->v4l2_dev, "Allocate dummy buffer, size: 0x%08x\n",
		  dummy_buf->size);

	return ret;
}

static void flexbus_cif_destroy_dummy_buf(struct flexbus_cif_stream *stream)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct flexbus_cif_dummy_buffer *dummy_buf = &dev->dummy_buf;

	if (dummy_buf->vaddr)
		flexbus_cif_free_buffer(dev, dummy_buf);
	dummy_buf->dma_addr = 0;
	dummy_buf->vaddr = NULL;
}

static void flexbus_cif_stop_streaming(struct vb2_queue *queue)
{
	struct flexbus_cif_stream *stream = queue->drv_priv;
	struct flexbus_cif_vdev_node *node = &stream->vnode;
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	struct flexbus_cif_buffer *buf = NULL;
	int ret;
	unsigned long flags;

	mutex_lock(&dev->stream_lock);

	v4l2_info(&dev->v4l2_dev, "stream[%d] start stopping\n",
		stream->id);


	stream->stopping = true;
	ret = wait_event_timeout(stream->wq_stopped,
				 stream->state != FLEXBUS_CIF_STATE_STREAMING,
				 msecs_to_jiffies(500));
	if (!ret) {
		flexbus_cif_stream_stop(stream);
		stream->stopping = false;
	}

	video_device_pipeline_stop(&node->vdev);
	ret = dev->pipe.set_stream(&dev->pipe, false);
	if (ret < 0)
		v4l2_err(v4l2_dev, "pipeline stream-off failed error:%d\n",
			 ret);
	/* release buffers */
	spin_lock_irqsave(&stream->vbq_lock, flags);
	if (stream->curr_buf)
		list_add_tail(&stream->curr_buf->queue, &stream->buf_head);
	if (stream->next_buf &&
	    stream->next_buf != stream->curr_buf)
		list_add_tail(&stream->next_buf->queue, &stream->buf_head);
	spin_unlock_irqrestore(&stream->vbq_lock, flags);

	stream->curr_buf = NULL;
	stream->next_buf = NULL;

	list_for_each_entry(buf, &stream->buf_head, queue) {
		v4l2_dbg(3, flexbus_cif_debug, &dev->v4l2_dev,
			 "stream[%d] buf return addr 0x%x\n",
			 stream->id, buf->buff_addr[0]);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	INIT_LIST_HEAD(&stream->buf_head);
	while (!list_empty(&stream->vb_done_list)) {
		buf = list_first_entry(&stream->vb_done_list,
				       struct flexbus_cif_buffer, queue);
		if (buf) {
			list_del(&buf->queue);
			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		}
	}

	atomic_set(&stream->buf_cnt, 0);
	stream->lack_buf_cnt = 0;

	ret = dev->pipe.close(&dev->pipe);
	if (ret < 0)
		v4l2_err(v4l2_dev, "pipeline close failed error:%d\n", ret);
	tasklet_disable(&stream->vb_done_tasklet);
	if (dev->dummy_buf.vaddr)
		flexbus_cif_destroy_dummy_buf(stream);
	INIT_LIST_HEAD(&stream->vb_done_list);
	v4l2_info(&dev->v4l2_dev, "stream[%d] stopping finished, dma_en 0x%x\n", stream->id, stream->dma_en);
	mutex_unlock(&dev->stream_lock);

}

static u32 flexbus_cif_align_bits_per_pixel(struct flexbus_cif_stream *stream,
					       const struct cif_output_fmt *fmt,
					       int plane_index)
{
	u32 bpp = 0, i, cal = 0;

	if (fmt) {
		switch (fmt->fourcc) {
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_GREY:
		case V4L2_PIX_FMT_Y16:
			bpp = fmt->bpp[plane_index];
			break;
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_YVYU:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_VYUY:
				bpp = fmt->bpp[plane_index];
			break;
		case V4L2_PIX_FMT_RGB24:
		case V4L2_PIX_FMT_BGR24:
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_BGR666:
		case V4L2_PIX_FMT_SRGGB8:
		case V4L2_PIX_FMT_SGRBG8:
		case V4L2_PIX_FMT_SGBRG8:
		case V4L2_PIX_FMT_SBGGR8:
		case V4L2_PIX_FMT_SRGGB10:
		case V4L2_PIX_FMT_SGRBG10:
		case V4L2_PIX_FMT_SGBRG10:
		case V4L2_PIX_FMT_SBGGR10:
		case V4L2_PIX_FMT_SRGGB12:
		case V4L2_PIX_FMT_SGRBG12:
		case V4L2_PIX_FMT_SGBRG12:
		case V4L2_PIX_FMT_SBGGR12:
		case V4L2_PIX_FMT_SBGGR16:
		case V4L2_PIX_FMT_SGBRG16:
		case V4L2_PIX_FMT_SGRBG16:
		case V4L2_PIX_FMT_SRGGB16:
		case V4l2_PIX_FMT_SPD16:
		case V4l2_PIX_FMT_EBD8:
		case V4L2_PIX_FMT_Y10:
		case V4L2_PIX_FMT_Y12:
			bpp = max(fmt->bpp[plane_index], (u8)CIF_RAW_STORED_BIT_WIDTH);
			cal = CIF_RAW_STORED_BIT_WIDTH;
			for (i = 1; i < 5; i++) {
				if (i * cal >= bpp) {
					bpp = i * cal;
					break;
				}
			}
			break;
		default:
			v4l2_err(&stream->cif_dev->v4l2_dev, "fourcc: %d is not supported!\n",
				 fmt->fourcc);
			break;
		}
	}

	return bpp;
}

static void flexbus_cif_sync_crop_info(struct flexbus_cif_stream *stream)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_subdev_selection input_sel;
	int ret;

	if (dev->terminal_sensor.sd) {
		input_sel.target = V4L2_SEL_TGT_CROP_BOUNDS;
		input_sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		input_sel.pad = 0;
		ret = v4l2_subdev_call(dev->terminal_sensor.sd,
				       pad, get_selection, NULL,
				       &input_sel);
		if (!ret) {
			stream->crop[CROP_SRC_SENSOR] = input_sel.r;
			stream->crop_enable = true;
			stream->crop_mask |= CROP_SRC_SENSOR_MASK;
			dev->terminal_sensor.selection = input_sel;
		} else {
			dev->terminal_sensor.selection.r = dev->terminal_sensor.raw_rect;
		}
	}

	if ((stream->crop_mask & 0x3) == (CROP_SRC_USR_MASK | CROP_SRC_SENSOR_MASK)) {
		if (stream->crop[CROP_SRC_USR].left + stream->crop[CROP_SRC_USR].width >
		    stream->crop[CROP_SRC_SENSOR].width ||
		    stream->crop[CROP_SRC_USR].top + stream->crop[CROP_SRC_USR].height >
		    stream->crop[CROP_SRC_SENSOR].height)
			stream->crop[CROP_SRC_USR] = stream->crop[CROP_SRC_SENSOR];
	}

	if (stream->crop_mask & CROP_SRC_USR_MASK) {
		stream->crop[CROP_SRC_ACT] = stream->crop[CROP_SRC_USR];
		if (stream->crop_mask & CROP_SRC_SENSOR_MASK) {
			stream->crop[CROP_SRC_ACT].left = stream->crop[CROP_SRC_USR].left +
							  stream->crop[CROP_SRC_SENSOR].left;
			stream->crop[CROP_SRC_ACT].top = stream->crop[CROP_SRC_USR].top +
							  stream->crop[CROP_SRC_SENSOR].top;
		}
	} else {
		stream->crop[CROP_SRC_ACT] = stream->crop[CROP_SRC_SENSOR];
	}
}

/**flexbus_cif_sanity_check_fmt - check fmt for setting
 * @stream - the stream for setting
 * @s_crop - the crop information
 */
static int flexbus_cif_sanity_check_fmt(struct flexbus_cif_stream *stream,
					   const struct v4l2_rect *s_crop)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	struct v4l2_rect input, *crop;

	if (dev->terminal_sensor.sd) {
		stream->cif_fmt_in = get_input_fmt(dev->terminal_sensor.sd,
						   &input, stream->id);
		if (!stream->cif_fmt_in) {
			v4l2_err(v4l2_dev, "Input fmt is invalid\n");
			return -EINVAL;
		}
	} else {
		v4l2_err(v4l2_dev, "terminal_sensor is invalid\n");
		return -EINVAL;
	}

	if (s_crop)
		crop = (struct v4l2_rect *)s_crop;
	else
		crop = &stream->crop[CROP_SRC_ACT];

	if (crop->width + crop->left > input.width ||
	    crop->height + crop->top > input.height) {
		v4l2_err(v4l2_dev, "crop size is bigger than input\n");
		return -EINVAL;
	}

	return 0;
}

static int flexbus_cif_update_sensor_info(struct flexbus_cif_stream *stream)
{
	struct flexbus_cif_sensor_info *sensor, *terminal_sensor;
	struct v4l2_subdev *sensor_sd;
	int ret = 0;

	sensor_sd = get_remote_sensor(stream, NULL);
	if (!sensor_sd) {
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "%s: stream[%d] get remote sensor_sd failed!\n",
			 __func__, stream->id);
		return -ENODEV;
	}

	sensor = sd_to_sensor(stream->cif_dev, sensor_sd);
	if (!sensor) {
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "%s: stream[%d] get remote sensor failed!\n",
			 __func__, stream->id);
		return -ENODEV;
	}
	ret = v4l2_subdev_call(sensor->sd, pad, get_mbus_config,
			       0, &sensor->mbus);
	if (ret && ret != -ENOIOCTLCMD) {
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "%s: get remote %s mbus failed!\n", __func__, sensor->sd->name);
		return ret;
	}

	stream->cif_dev->active_sensor = sensor;

	terminal_sensor = &stream->cif_dev->terminal_sensor;
	get_remote_terminal_sensor(stream, &terminal_sensor->sd);
	if (terminal_sensor->sd) {
		ret = v4l2_subdev_call(terminal_sensor->sd, pad, get_mbus_config,
				       0, &terminal_sensor->mbus);
		if (ret && ret != -ENOIOCTLCMD) {
			v4l2_err(&stream->cif_dev->v4l2_dev,
				 "%s: get terminal %s mbus failed!\n",
				 __func__, terminal_sensor->sd->name);
			return ret;
		}
		ret = v4l2_subdev_call(terminal_sensor->sd, video,
				       g_frame_interval, &terminal_sensor->fi);
		if (ret) {
			v4l2_err(&stream->cif_dev->v4l2_dev,
				 "%s: get terminal %s g_frame_interval failed!\n",
				 __func__, terminal_sensor->sd->name);
			return ret;
		}
	} else {
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "%s: stream[%d] get remote terminal sensor failed!\n",
			 __func__, stream->id);
		return -ENODEV;
	}

	return ret;
}

static void flexbus_cif_config_cif_clk_sampling_edge(struct flexbus_cif_device *dev,
							enum flexbus_cif_clk_edge edge)
{
	u32 val = 0;

	if (edge == FLEXBUS_CIF_CLK_RISING)
		dev->fb_dev->config->grf_config(dev->fb_dev, 1, false, false);
	else
		dev->fb_dev->config->grf_config(dev->fb_dev, 1, false, true);
	v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev,
		 "%s %d, val %x\n", __func__, __LINE__, val);
}

static int flexbus_cif_stream_start(struct flexbus_cif_stream *stream)
{
	u32 val, mbus_flags, href_pol, vsync_pol;
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct flexbus_cif_sensor_info *sensor_info;
	struct v4l2_mbus_config *mbus;
	struct flexbus_cif_cif_sof_subdev *sof_sd = &dev->cif_sof_subdev;
	struct v4l2_rect rect = {0};

	if (stream->state < FLEXBUS_CIF_STATE_STREAMING) {
		stream->frame_idx = 0;
		stream->buf_wake_up_cnt = 0;
		stream->lack_buf_cnt = 0;
		stream->frame_phase = 0;
		stream->is_in_vblank = false;
		stream->is_change_toisp = false;
	}
	sensor_info = dev->active_sensor;
	mbus = &sensor_info->mbus;

	mbus_flags = mbus->bus.parallel.flags;
	if (mbus_flags & V4L2_MBUS_PCLK_SAMPLE_RISING)
		flexbus_cif_config_cif_clk_sampling_edge(dev, FLEXBUS_CIF_CLK_RISING);
	else
		flexbus_cif_config_cif_clk_sampling_edge(dev, FLEXBUS_CIF_CLK_FALLING);
	href_pol = (mbus_flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH) ?
		    HSY_HIGH_ACTIVE : HSY_LOW_ACTIVE;
	vsync_pol = (mbus_flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH) ?
		     VSY_HIGH_ACTIVE : VSY_LOW_ACTIVE;
	flexbus_cif_write_register(dev, FLEXBUS_DVP_POL, href_pol | vsync_pol);
	v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev, "href_pol %d vsync_pol %d\n",
		 href_pol, vsync_pol);
	v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev,
		 "mbus_flags %x V4L2_MBUS_PCLK_SAMPLE_RISING %lx\n",
		 mbus_flags, V4L2_MBUS_PCLK_SAMPLE_RISING);

	if (stream->crop_enable) {
		rect.left = stream->crop[CROP_SRC_ACT].left * 2;
		rect.top = stream->crop[CROP_SRC_ACT].top;
		rect.width = stream->crop[CROP_SRC_ACT].width * 2;
		rect.height = stream->crop[CROP_SRC_ACT].height;
	} else {
		rect.left = 0;
		rect.top = 0;
		rect.width = stream->pixm.width * 2;
		rect.height = stream->pixm.height;
	}
	flexbus_cif_write_register(dev, FLEXBUS_DVP_CROP_SIZE,
			     rect.width |
			     (rect.height << 16));
	flexbus_cif_write_register(dev, FLEXBUS_DVP_CROP_START,
			     rect.top << CIF_CROP_Y_SHIFT |
			     rect.left);
	flexbus_cif_assign_new_buffer_pingpong(stream,
			 FLEXBUS_CIF_YUV_ADDR_STATE_INIT,
			 stream->id);
	flexbus_cif_write_register(dev, FLEXBUS_SLAVE_MODE, BIT(1) | BIT(0));
	val = flexbus_cif_read_register(dev, FLEXBUS_RX_CTL);
	val &= ~FLEXBUS_DST_WAT_LVL_MASK;
	val |= dev->fb_dev->dfs_reg->dfs_8bit;
	val |= FLEXBUS_CONTINUE_MODE;
	val |= FLEXBUS_AUTOPAD;
	flexbus_cif_write_register(dev, FLEXBUS_RX_CTL, val);

	flexbus_cif_write_register(dev, FLEXBUS_COM_CTL, 0x2);
	flexbus_cif_write_register(dev, FLEXBUS_DMA_WR_OUTSTD, 0x1f);

	flexbus_cif_write_register(dev, FLEXBUS_DVP_ORDER, stream->cif_fmt_in->cif_yuv_order |
					stream->cif_fmt_out->cif_yuv_order);

	if (dev->chip_id == RK_FLEXBUS_CIF_RK3506) {
		if (stream->cif_fmt_out->fourcc == V4L2_PIX_FMT_RGB24)
			flexbus_cif_write_register(dev, FLEXBUS_DVP_YUV2RGB,
						   CIF_YUV2RGB_ENABLE | CIF_YUV2RGB_B_LSB | CIF_YUV2RGB_BT601_FULL);
		else if (stream->cif_fmt_out->fourcc == V4L2_PIX_FMT_BGR24)
			flexbus_cif_write_register(dev, FLEXBUS_DVP_YUV2RGB,
						   CIF_YUV2RGB_ENABLE | CIF_YUV2RGB_BT601_FULL);
		else
			flexbus_cif_write_register(dev, FLEXBUS_DVP_YUV2RGB, 0);
	}

	flexbus_cif_write_register_or(dev, FLEXBUS_IMR, CIF_FIFO_OVERFLOW |
					 CIF_BANDWIDTH_LACK |
					 CIF_DMA_END |
					 CIF_SIZE_ERR);
	flexbus_cif_write_register(dev, FLEXBUS_ENR, BIT(1) | ((BIT(1) | BIT(0)) << 16));
	atomic_set(&sof_sd->frm_sync_seq, 0);
	stream->state = FLEXBUS_CIF_STATE_STREAMING;

	return 0;
}

static int flexbus_cif_start_streaming(struct vb2_queue *queue, unsigned int count)
{
	struct flexbus_cif_stream *stream = queue->drv_priv;
	struct flexbus_cif_vdev_node *node = &stream->vnode;
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	struct flexbus_cif_sensor_info *terminal_sensor = &dev->terminal_sensor;
	int ret;

	v4l2_info(&dev->v4l2_dev, "stream[%d] start streaming\n", stream->id);

	mutex_lock(&dev->stream_lock);
	if (stream->state == FLEXBUS_CIF_STATE_STREAMING) {
		ret = -EBUSY;
		v4l2_err(v4l2_dev, "stream in busy state\n");
		goto destroy_buf;
	}

	if (dev->active_sensor) {
		ret = flexbus_cif_update_sensor_info(stream);
		if (ret < 0) {
			v4l2_err(v4l2_dev,
				 "update sensor info failed %d\n",
				 ret);
			goto out;
		}
	}

	if (terminal_sensor->sd) {

		ret = v4l2_subdev_call(terminal_sensor->sd,
				       video, g_frame_interval, &terminal_sensor->fi);
		if (ret)
			terminal_sensor->fi.interval = (struct v4l2_fract) {1, 30};

		flexbus_cif_sync_crop_info(stream);
	}

	ret = flexbus_cif_sanity_check_fmt(stream, NULL);
	if (ret < 0)
		goto destroy_buf;

	if (dev->is_use_dummybuf && (!dev->dummy_buf.vaddr)) {
		ret = flexbus_cif_create_dummy_buf(stream);
		if (ret < 0) {
			v4l2_err(v4l2_dev, "Failed to create dummy_buf, %d\n", ret);
			goto destroy_buf;
		}
	}
	tasklet_enable(&stream->vb_done_tasklet);
	ret = dev->pipe.open(&dev->pipe, &node->vdev.entity, true);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "open cif pipeline failed %d\n",
			 ret);
		goto destroy_buf;
	}
	ret = dev->pipe.set_stream(&dev->pipe, true);
	if (ret < 0)
		goto stop_stream;

	msleep(50);
	ret = flexbus_cif_stream_start(stream);
	if (ret < 0)
		goto destroy_buf;
	ret = video_device_pipeline_start(&node->vdev, &dev->pipe.pipe);
	if (ret < 0) {
		v4l2_err(&dev->v4l2_dev, "start pipeline failed %d\n",
			 ret);
		goto pipe_stream_off;
	}

	goto out;

stop_stream:
	flexbus_cif_stream_stop(stream);
pipe_stream_off:
	dev->pipe.set_stream(&dev->pipe, false);
destroy_buf:
	if (stream->next_buf)
		vb2_buffer_done(&stream->next_buf->vb.vb2_buf,
				VB2_BUF_STATE_QUEUED);
	if (stream->curr_buf)
		vb2_buffer_done(&stream->curr_buf->vb.vb2_buf,
				VB2_BUF_STATE_QUEUED);
	while (!list_empty(&stream->buf_head)) {
		struct flexbus_cif_buffer *buf;

		buf = list_first_entry(&stream->buf_head,
				       struct flexbus_cif_buffer, queue);
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_QUEUED);
		list_del(&buf->queue);
	}

out:
	mutex_unlock(&dev->stream_lock);
	return ret;
}

static const struct vb2_ops flexbus_cif_vb2_ops = {
	.queue_setup = flexbus_cif_queue_setup,
	.buf_queue = flexbus_cif_buf_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.stop_streaming = flexbus_cif_stop_streaming,
	.start_streaming = flexbus_cif_start_streaming,
};

static int flexbus_cif_init_vb2_queue(struct vb2_queue *q,
					 struct flexbus_cif_stream *stream,
					 enum v4l2_buf_type buf_type)
{
	q->type = buf_type;
	q->io_modes = VB2_MMAP | VB2_DMABUF;
	q->drv_priv = stream;
	q->ops = &flexbus_cif_vb2_ops;
	q->mem_ops = &vb2_cma_sg_memops;
	q->buf_struct_size = sizeof(struct flexbus_cif_buffer);
	q->min_buffers_needed = CIF_REQ_BUFS_MIN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &stream->vnode.vlock;
	q->dev = stream->cif_dev->dev;
	q->allow_cache_hints = 1;
	q->bidirectional = 1;
	q->gfp_flags = GFP_DMA32;
	q->dma_attrs = DMA_ATTR_FORCE_CONTIGUOUS;
	return vb2_queue_init(q);
}

static int flexbus_cif_set_fmt(struct flexbus_cif_stream *stream,
				  struct v4l2_pix_format_mplane *pixm,
				  bool try)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	const struct cif_output_fmt *fmt;
	const struct cif_input_fmt *cif_fmt_in = NULL;
	struct v4l2_rect input_rect;
	unsigned int imagesize = 0, planes;
	u32 xsubs = 1, ysubs = 1, i;
	int ret;

	for (i = 0; i < FLEXBUS_CIF_MAX_PLANE; i++)
		memset(&pixm->plane_fmt[i], 0, sizeof(struct v4l2_plane_pix_format));

	fmt = flexbus_cif_find_output_fmt(stream, pixm->pixelformat);
	if (!fmt)
		fmt = &out_fmts[0];

	input_rect.width = FLEXBUS_CIF_DEFAULT_WIDTH;
	input_rect.height = FLEXBUS_CIF_DEFAULT_HEIGHT;

	if (dev->terminal_sensor.sd) {
		cif_fmt_in = get_input_fmt(dev->terminal_sensor.sd,
					   &input_rect, stream->id);
		stream->cif_fmt_in = cif_fmt_in;
	} else {
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "terminal subdev does not exist\n");
		return -EINVAL;
	}

	ret = flexbus_cif_output_fmt_check(stream, fmt);
	if (ret)
		return -EINVAL;

	dev->terminal_sensor.raw_rect = input_rect;

	/* CIF has not scale function,
	 * the size should not be larger than input
	 */
	pixm->width = clamp_t(u32, pixm->width,
			      CIF_MIN_WIDTH, input_rect.width);
	pixm->height = clamp_t(u32, pixm->height,
			       CIF_MIN_HEIGHT, input_rect.height);
	pixm->num_planes = fmt->mplanes;
	pixm->field = V4L2_FIELD_NONE;
	pixm->quantization = V4L2_QUANTIZATION_DEFAULT;

	flexbus_cif_sync_crop_info(stream);
	/* calculate plane size and image size */
	fcc_xysubs(fmt->fourcc, &xsubs, &ysubs);

	planes = fmt->cplanes ? fmt->cplanes : fmt->mplanes;

	for (i = 0; i < planes; i++) {
		struct v4l2_plane_pix_format *plane_fmt;
		int width, height, bpl, size, bpp;

		if (i == 0) {
			if (stream->crop_enable) {
				width = stream->crop[CROP_SRC_ACT].width;
				height = stream->crop[CROP_SRC_ACT].height;
			} else {
				width = pixm->width;
				height = pixm->height;
			}
		} else {
			if (stream->crop_enable) {
				width = stream->crop[CROP_SRC_ACT].width / xsubs;
				height = stream->crop[CROP_SRC_ACT].height / ysubs;
			} else {
				width = pixm->width / xsubs;
				height = pixm->height / ysubs;
			}
		}

		bpp = flexbus_cif_align_bits_per_pixel(stream, fmt, i);
		bpl = width * bpp / CIF_YUV_STORED_BIT_WIDTH;
		size = bpl * height;
		imagesize += size;

		if (fmt->mplanes > i) {
			/* Set bpl and size for each mplane */
			plane_fmt = pixm->plane_fmt + i;
			plane_fmt->bytesperline = bpl;
			plane_fmt->sizeimage = size;
		}
		v4l2_dbg(1, flexbus_cif_debug, &stream->cif_dev->v4l2_dev,
			 "C-Plane %i size: %d, Total imagesize: %d\n",
			 i, size, imagesize);
	}

	/* convert to non-MPLANE format.
	 * It's important since we want to unify non-MPLANE
	 * and MPLANE.
	 */
	if (fmt->mplanes == 1)
		pixm->plane_fmt[0].sizeimage = imagesize;

	if (!try) {
		stream->cif_fmt_out = fmt;
		stream->pixm = *pixm;

		v4l2_dbg(1, flexbus_cif_debug, &stream->cif_dev->v4l2_dev,
			 "%s: req(%d, %d) out(%d, %d)\n", __func__,
			 pixm->width, pixm->height,
			 stream->pixm.width, stream->pixm.height);
	}
	return 0;
}

void flexbus_cif_stream_init(struct flexbus_cif_device *dev, u32 id)
{
	struct flexbus_cif_stream *stream = &dev->stream[id];
	struct v4l2_pix_format_mplane pixm;
	int i;

	memset(stream, 0, sizeof(*stream));
	memset(&pixm, 0, sizeof(pixm));
	stream->id = id;
	stream->cif_dev = dev;

	INIT_LIST_HEAD(&stream->buf_head);
	spin_lock_init(&stream->vbq_lock);
	spin_lock_init(&stream->fps_lock);
	stream->state = FLEXBUS_CIF_STATE_READY;
	init_waitqueue_head(&stream->wq_stopped);

	/* Set default format */
	pixm.pixelformat = V4L2_PIX_FMT_NV12;
	pixm.width = FLEXBUS_CIF_DEFAULT_WIDTH;
	pixm.height = FLEXBUS_CIF_DEFAULT_HEIGHT;
	flexbus_cif_set_fmt(stream, &pixm, false);

	for (i = 0; i < CROP_SRC_MAX; i++) {
		stream->crop[i].left = 0;
		stream->crop[i].top = 0;
		stream->crop[i].width = FLEXBUS_CIF_DEFAULT_WIDTH;
		stream->crop[i].height = FLEXBUS_CIF_DEFAULT_HEIGHT;
	}

	stream->crop_enable = false;
	stream->crop_dyn_en = false;
	stream->crop_mask = 0x0;

	stream->is_compact = false;

	atomic_set(&stream->buf_cnt, 0);
}

static int flexbus_cif_fh_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct flexbus_cif_vdev_node *vnode = vdev_to_node(vdev);
	struct flexbus_cif_stream *stream = to_flexbus_cif_stream(vnode);
	struct flexbus_cif_device *cif_dev = stream->cif_dev;
	int ret;

	/* Make sure active sensor is valid before .set_fmt() */
	ret = flexbus_cif_update_sensor_info(stream);
	if (ret < 0) {
		v4l2_err(vdev,
			 "update sensor info failed %d\n",
			 ret);

		return ret;
	}

	ret = pm_runtime_resume_and_get(cif_dev->dev);
	if (ret < 0) {
		v4l2_err(vdev, "Failed to get runtime pm, %d\n",
			 ret);
		return ret;
	}

	ret = v4l2_fh_open(file);
	if (!ret) {
		mutex_lock(&cif_dev->stream_lock);
		ret = v4l2_pipeline_pm_get(&vnode->vdev.entity);
		v4l2_info(vdev, "open video, entity use_count %d\n",
			  vnode->vdev.entity.use_count);
		mutex_unlock(&cif_dev->stream_lock);
		if (ret < 0)
			vb2_fop_release(file);
	}
	return ret;
}

static int flexbus_cif_fh_release(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct flexbus_cif_vdev_node *vnode = vdev_to_node(vdev);
	struct flexbus_cif_stream *stream = to_flexbus_cif_stream(vnode);
	struct flexbus_cif_device *cif_dev = stream->cif_dev;
	int ret = 0;

	ret = vb2_fop_release(file);
	if (!ret) {
		mutex_lock(&cif_dev->stream_lock);
		v4l2_pipeline_pm_put(&vnode->vdev.entity);
		v4l2_info(vdev, "close video, entity use_count %d\n",
			  vnode->vdev.entity.use_count);
		mutex_unlock(&cif_dev->stream_lock);
	}

	pm_runtime_put_sync(cif_dev->dev);
	return ret;
}

static const struct v4l2_file_operations flexbus_cif_fops = {
	.open = flexbus_cif_fh_open,
	.release = flexbus_cif_fh_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

static int flexbus_cif_enum_input(struct file *file, void *priv,
				     struct v4l2_input *input)
{
	if (input->index > 0)
		return -EINVAL;

	input->type = V4L2_INPUT_TYPE_CAMERA;
	strscpy(input->name, "Camera", sizeof(input->name));

	return 0;
}

static int flexbus_cif_try_fmt_vid_cap_mplane(struct file *file, void *fh,
						 struct v4l2_format *f)
{
	struct flexbus_cif_stream *stream = video_drvdata(file);
	int ret = 0;

	ret = flexbus_cif_set_fmt(stream, &f->fmt.pix_mp, true);

	return ret;
}

static int flexbus_cif_enum_framesizes(struct file *file, void *prov,
					  struct v4l2_frmsizeenum *fsize)
{
	struct v4l2_frmsize_stepwise *s = &fsize->stepwise;
	struct flexbus_cif_stream *stream = video_drvdata(file);
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_rect input_rect;

	if (fsize->index != 0)
		return -EINVAL;

	if (!flexbus_cif_find_output_fmt(stream, fsize->pixel_format))
		return -EINVAL;

	input_rect.width = FLEXBUS_CIF_DEFAULT_WIDTH;
	input_rect.height = FLEXBUS_CIF_DEFAULT_HEIGHT;

	if (dev->terminal_sensor.sd)
		get_input_fmt(dev->terminal_sensor.sd,
			      &input_rect, stream->id);

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	s->min_width = CIF_MIN_WIDTH;
	s->min_height = CIF_MIN_HEIGHT;
	s->max_width = input_rect.width;
	s->max_height = input_rect.height;
	s->step_width = OUTPUT_STEP_WISE;
	s->step_height = OUTPUT_STEP_WISE;

	return 0;
}

static int flexbus_cif_enum_frameintervals(struct file *file, void *fh,
					      struct v4l2_frmivalenum *fival)
{
	struct flexbus_cif_stream *stream = video_drvdata(file);
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct flexbus_cif_sensor_info *sensor = dev->active_sensor;
	struct v4l2_subdev_frame_interval fi;
	int ret;

	if (fival->index != 0)
		return -EINVAL;

	if (!sensor || !sensor->sd) {
		/* TODO: active_sensor is NULL if using DMARX path */
		v4l2_err(&dev->v4l2_dev, "%s Not active sensor\n", __func__);
		return -ENODEV;
	}

	ret = v4l2_subdev_call(sensor->sd, video, g_frame_interval, &fi);
	if (ret && ret != -ENOIOCTLCMD) {
		return ret;
	} else if (ret == -ENOIOCTLCMD) {
		/* Set a default value for sensors not implements ioctl */
		fi.interval.numerator = 1;
		fi.interval.denominator = 30;
	}

	fival->type = V4L2_FRMIVAL_TYPE_CONTINUOUS;
	fival->stepwise.step.numerator = 1;
	fival->stepwise.step.denominator = 1;
	fival->stepwise.max.numerator = 1;
	fival->stepwise.max.denominator = 1;
	fival->stepwise.min.numerator = fi.interval.numerator;
	fival->stepwise.min.denominator = fi.interval.denominator;

	return 0;
}

static int flexbus_cif_enum_fmt_vid_cap_mplane(struct file *file, void *priv,
						  struct v4l2_fmtdesc *f)
{
	const struct cif_output_fmt *fmt = NULL;
	struct flexbus_cif_stream *stream = video_drvdata(file);
	struct flexbus_cif_device *dev = stream->cif_dev;
	const struct cif_input_fmt *cif_fmt_in = NULL;
	struct v4l2_rect input_rect;
	int i = 0;
	int ret = 0;
	int fource_idx = 0;

	if (f->index >= ARRAY_SIZE(out_fmts))
		return -EINVAL;

	if (dev->terminal_sensor.sd) {
		cif_fmt_in = get_input_fmt(dev->terminal_sensor.sd,
					   &input_rect, stream->id);
		stream->cif_fmt_in = cif_fmt_in;
	} else {
		v4l2_err(&stream->cif_dev->v4l2_dev,
			 "terminal subdev does not exist\n");
		return -EINVAL;
	}

	if (f->index != 0)
		fource_idx = stream->new_fource_idx;

	for (i = fource_idx; i < ARRAY_SIZE(out_fmts); i++) {
		fmt = &out_fmts[i];
		ret = flexbus_cif_output_fmt_check(stream, fmt);
		if (!ret) {
			f->pixelformat = fmt->fourcc;
			stream->new_fource_idx = i + 1;
			break;
		}
	}
	if (i == ARRAY_SIZE(out_fmts))
		return -EINVAL;

	switch (f->pixelformat) {
	case V4l2_PIX_FMT_EBD8:
		strscpy(f->description,
			"Embedded data 8-bit",
			sizeof(f->description));
		break;
	case V4l2_PIX_FMT_SPD16:
		strscpy(f->description,
			"Shield pix data 16-bit",
			sizeof(f->description));
		break;
	default:
		break;
	}
	return 0;
}

static int flexbus_cif_s_fmt_vid_cap_mplane(struct file *file,
					       void *priv, struct v4l2_format *f)
{
	struct flexbus_cif_stream *stream = video_drvdata(file);
	struct flexbus_cif_device *dev = stream->cif_dev;
	int ret = 0;

	if (vb2_is_busy(&stream->vnode.buf_queue)) {
		v4l2_err(&dev->v4l2_dev, "%s queue busy\n", __func__);
		return -EBUSY;
	}

	ret = flexbus_cif_set_fmt(stream, &f->fmt.pix_mp, false);

	return ret;
}

static int flexbus_cif_g_fmt_vid_cap_mplane(struct file *file, void *fh,
					       struct v4l2_format *f)
{
	struct flexbus_cif_stream *stream = video_drvdata(file);

	f->fmt.pix_mp = stream->pixm;

	return 0;
}

static int flexbus_cif_querycap(struct file *file, void *priv,
				   struct v4l2_capability *cap)
{
	struct flexbus_cif_stream *stream = video_drvdata(file);
	struct device *dev = stream->cif_dev->dev;

	strscpy(cap->driver, dev->driver->name, sizeof(cap->driver));
	strscpy(cap->card, dev->driver->name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev_name(dev));

	return 0;
}

static __maybe_unused int flexbus_cif_cropcap(struct file *file, void *fh,
						 struct v4l2_cropcap *cap)
{
	struct flexbus_cif_stream *stream = video_drvdata(file);
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_rect *raw_rect = &dev->terminal_sensor.raw_rect;
	int ret = 0;

	if (stream->crop_mask & CROP_SRC_SENSOR) {
		cap->bounds.left = stream->crop[CROP_SRC_SENSOR].left;
		cap->bounds.top = stream->crop[CROP_SRC_SENSOR].top;
		cap->bounds.width = stream->crop[CROP_SRC_SENSOR].width;
		cap->bounds.height = stream->crop[CROP_SRC_SENSOR].height;
	} else {
		cap->bounds.left = raw_rect->left;
		cap->bounds.top = raw_rect->top;
		cap->bounds.width = raw_rect->width;
		cap->bounds.height = raw_rect->height;
	}

	cap->defrect = cap->bounds;
	cap->pixelaspect.numerator = 1;
	cap->pixelaspect.denominator = 1;

	return ret;
}

static int flexbus_cif_s_selection(struct file *file, void *fh,
				      struct v4l2_selection *s)
{
	struct flexbus_cif_stream *stream = video_drvdata(file);
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_subdev *sensor_sd;
	struct v4l2_subdev_selection sd_sel;
	const struct v4l2_rect *rect = &s->r;
	struct v4l2_rect sensor_crop;
	struct v4l2_rect *raw_rect = &dev->terminal_sensor.raw_rect;
	u16 pad = 0;
	int ret = 0;

	if (!s) {
		v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev, "sel is null\n");
		goto err;
	}

	if (s->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sensor_sd = get_remote_sensor(stream, &pad);

		sd_sel.r = s->r;
		sd_sel.pad = pad;
		sd_sel.target = s->target;
		sd_sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;

		ret = v4l2_subdev_call(sensor_sd, pad, set_selection, NULL, &sd_sel);
		if (!ret) {
			s->r = sd_sel.r;
			v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev, "%s: pad:%d, which:%d, target:%d\n",
				 __func__, pad, sd_sel.which, sd_sel.target);
		}
	} else if (s->target == V4L2_SEL_TGT_CROP) {
		ret = flexbus_cif_sanity_check_fmt(stream, rect);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "set crop failed\n");
			return ret;
		}

		if (stream->crop_mask & CROP_SRC_SENSOR) {
			sensor_crop = stream->crop[CROP_SRC_SENSOR];
			if (rect->left + rect->width > sensor_crop.width ||
			    rect->top + rect->height > sensor_crop.height) {
				v4l2_err(&dev->v4l2_dev,
					 "crop size is bigger than sensor input:left:%d, top:%d, width:%d, height:%d\n",
					 sensor_crop.left, sensor_crop.top,
					 sensor_crop.width, sensor_crop.height);
				return -EINVAL;
			}
		} else {
			if (rect->left + rect->width > raw_rect->width ||
			    rect->top + rect->height > raw_rect->height) {
				v4l2_err(&dev->v4l2_dev,
					 "crop size is bigger than sensor raw input:left:%d, top:%d, width:%d, height:%d\n",
					 raw_rect->left, raw_rect->top,
					 raw_rect->width, raw_rect->height);
				return -EINVAL;
			}
		}

		stream->crop[CROP_SRC_USR] = *rect;
		stream->crop_enable = true;
		stream->crop_mask |= CROP_SRC_USR_MASK;
		stream->crop[CROP_SRC_ACT] = stream->crop[CROP_SRC_USR];
		if (stream->crop_mask & CROP_SRC_SENSOR) {
			sensor_crop = stream->crop[CROP_SRC_SENSOR];
			stream->crop[CROP_SRC_ACT].left = sensor_crop.left + stream->crop[CROP_SRC_USR].left;
			stream->crop[CROP_SRC_ACT].top = sensor_crop.top + stream->crop[CROP_SRC_USR].top;
		}

		if (stream->state == FLEXBUS_CIF_STATE_STREAMING) {
			stream->crop_dyn_en = true;

			v4l2_info(&dev->v4l2_dev, "enable dynamic crop, S_SELECTION(%ux%u@%u:%u) target: %d\n",
				  rect->width, rect->height, rect->left, rect->top, s->target);
		} else {
			v4l2_info(&dev->v4l2_dev, "static crop, S_SELECTION(%ux%u@%u:%u) target: %d\n",
				  rect->width, rect->height, rect->left, rect->top, s->target);
		}
	} else {
		goto err;
	}

	return ret;

err:
	return -EINVAL;
}

static int flexbus_cif_g_selection(struct file *file, void *fh,
				      struct v4l2_selection *s)
{
	struct flexbus_cif_stream *stream = video_drvdata(file);
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_subdev *sensor_sd;
	struct v4l2_subdev_selection sd_sel;
	u16 pad = 0;
	int ret = 0;

	if (!s) {
		v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev, "sel is null\n");
		goto err;
	}

	if (s->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sensor_sd = get_remote_sensor(stream, &pad);

		sd_sel.pad = pad;
		sd_sel.target = s->target;
		sd_sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;

		v4l2_dbg(1, flexbus_cif_debug, &dev->v4l2_dev, "%s(line:%d): sd:%s pad:%d, which:%d, target:%d\n",
			 __func__, __LINE__, sensor_sd->name, pad, sd_sel.which, sd_sel.target);

		ret = v4l2_subdev_call(sensor_sd, pad, get_selection, NULL, &sd_sel);
		if (!ret) {
			s->r = sd_sel.r;
		} else {
			s->r.left = 0;
			s->r.top = 0;
			s->r.width = stream->pixm.width;
			s->r.height = stream->pixm.height;
		}
	} else if (s->target == V4L2_SEL_TGT_CROP) {
		if (stream->crop_mask & (CROP_SRC_USR_MASK | CROP_SRC_SENSOR_MASK)) {
			s->r = stream->crop[CROP_SRC_ACT];
		} else {
			s->r.left = 0;
			s->r.top = 0;
			s->r.width = stream->pixm.width;
			s->r.height = stream->pixm.height;
		}
	} else {
		goto err;
	}

	return ret;
err:
	return -EINVAL;
}

static long flexbus_cif_ioctl_default(struct file *file, void *fh,
					 bool valid_prio, unsigned int cmd, void *arg)
{
	return -EINVAL;
}

static const struct v4l2_ioctl_ops flexbus_cif_v4l2_ioctl_ops = {
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_enum_input = flexbus_cif_enum_input,
	.vidioc_try_fmt_vid_cap_mplane = flexbus_cif_try_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_cap = flexbus_cif_enum_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap_mplane = flexbus_cif_s_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_cap_mplane = flexbus_cif_g_fmt_vid_cap_mplane,
	.vidioc_querycap = flexbus_cif_querycap,
	.vidioc_s_selection = flexbus_cif_s_selection,
	.vidioc_g_selection = flexbus_cif_g_selection,
	.vidioc_enum_frameintervals = flexbus_cif_enum_frameintervals,
	.vidioc_enum_framesizes = flexbus_cif_enum_framesizes,
	.vidioc_default = flexbus_cif_ioctl_default,
};

static void flexbus_cif_vb_done_oneframe(struct flexbus_cif_stream *stream,
					    struct vb2_v4l2_buffer *vb_done)
{
	const struct cif_output_fmt *fmt = stream->cif_fmt_out;
	u32 i;

	/* Dequeue a filled buffer */
	for (i = 0; i < fmt->mplanes; i++) {
		vb2_set_plane_payload(&vb_done->vb2_buf, i,
				      stream->pixm.plane_fmt[i].sizeimage);
	}

	vb2_buffer_done(&vb_done->vb2_buf, VB2_BUF_STATE_DONE);
	v4l2_dbg(2, flexbus_cif_debug, &stream->cif_dev->v4l2_dev,
		 "stream[%d] vb done, index: %d, sequence %d\n", stream->id,
		 vb_done->vb2_buf.index, vb_done->sequence);
	atomic_dec(&stream->buf_cnt);
}

static void flexbus_cif_tasklet_handle(unsigned long data)
{
	struct flexbus_cif_stream *stream = (struct flexbus_cif_stream *)data;
	struct flexbus_cif_buffer *buf = NULL;
	unsigned long flags = 0;
	LIST_HEAD(local_list);

	spin_lock_irqsave(&stream->vbq_lock, flags);
	list_replace_init(&stream->vb_done_list, &local_list);
	spin_unlock_irqrestore(&stream->vbq_lock, flags);
	while (!list_empty(&local_list)) {
		buf = list_first_entry(&local_list,
				       struct flexbus_cif_buffer, queue);
		list_del(&buf->queue);
		flexbus_cif_vb_done_oneframe(stream, &buf->vb);
	}
}

static void flexbus_cif_vb_done_tasklet(struct flexbus_cif_stream *stream,
					   struct flexbus_cif_buffer *buf)
{
	unsigned long flags = 0;

	if (!stream || !buf)
		return;
	spin_lock_irqsave(&stream->vbq_lock, flags);
	list_add_tail(&buf->queue, &stream->vb_done_list);
	spin_unlock_irqrestore(&stream->vbq_lock, flags);
	tasklet_schedule(&stream->vb_done_tasklet);
}

static void flexbus_cif_unregister_stream_vdev(struct flexbus_cif_stream *stream)
{
	tasklet_kill(&stream->vb_done_tasklet);
	media_entity_cleanup(&stream->vnode.vdev.entity);
	video_unregister_device(&stream->vnode.vdev);
}

static int flexbus_cif_register_stream_vdev(struct flexbus_cif_stream *stream,
					       bool is_multi_input)
{
	struct flexbus_cif_device *dev = stream->cif_dev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	struct video_device *vdev = &stream->vnode.vdev;
	struct flexbus_cif_vdev_node *node;
	int ret = 0;
	char *vdev_name;

	vdev_name = CIF_CIF_VDEV_NAME;
	strscpy(vdev->name, vdev_name, sizeof(vdev->name));
	node = vdev_to_node(vdev);
	mutex_init(&node->vlock);

	vdev->ioctl_ops = &flexbus_cif_v4l2_ioctl_ops;
	vdev->release = video_device_release_empty;
	vdev->fops = &flexbus_cif_fops;
	vdev->minor = -1;
	vdev->v4l2_dev = v4l2_dev;
	vdev->lock = &node->vlock;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			    V4L2_CAP_STREAMING;
	video_set_drvdata(vdev, stream);
	vdev->vfl_dir = VFL_DIR_RX;
	node->pad.flags = MEDIA_PAD_FL_SINK;

	flexbus_cif_init_vb2_queue(&node->buf_queue, stream,
			     V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	vdev->queue = &node->buf_queue;

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		v4l2_err(v4l2_dev,
			 "video_register_device failed with error %d\n", ret);
		return ret;
	}

	ret = media_entity_pads_init(&vdev->entity, 1, &node->pad);
	if (ret < 0)
		goto unreg;

	INIT_LIST_HEAD(&stream->vb_done_list);
	tasklet_init(&stream->vb_done_tasklet,
		     flexbus_cif_tasklet_handle,
		     (unsigned long)stream);
	tasklet_disable(&stream->vb_done_tasklet);
	return 0;
unreg:
	video_unregister_device(vdev);
	return ret;
}

void flexbus_cif_unregister_stream_vdevs(struct flexbus_cif_device *dev,
					    int stream_num)
{
	struct flexbus_cif_stream *stream;
	int i;

	for (i = 0; i < stream_num; i++) {
		stream = &dev->stream[i];
		flexbus_cif_unregister_stream_vdev(stream);
	}
}

int flexbus_cif_register_stream_vdevs(struct flexbus_cif_device *dev,
					 int stream_num,
					 bool is_multi_input)
{
	struct flexbus_cif_stream *stream;
	int i, j, ret;

	for (i = 0; i < stream_num; i++) {
		stream = &dev->stream[i];
		stream->cif_dev = dev;
		ret = flexbus_cif_register_stream_vdev(stream, is_multi_input);
		if (ret < 0)
			goto err;
	}
	return 0;
err:
	for (j = 0; j < i; j++) {
		stream = &dev->stream[j];
		flexbus_cif_unregister_stream_vdev(stream);
	}

	return ret;
}

static int flexbus_cif_sof_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
					      struct v4l2_event_subscription *sub)
{
	if (sub->type == V4L2_EVENT_FRAME_SYNC)
		return v4l2_event_subscribe(fh, sub, 4, NULL);
	else
		return -EINVAL;
}

static void flexbus_cif_cif_event_inc_sof(struct flexbus_cif_device *dev)
{
	struct flexbus_cif_cif_sof_subdev *subdev = &dev->cif_sof_subdev;

	if (subdev) {
		struct v4l2_event event = {
			.type = V4L2_EVENT_FRAME_SYNC,
			.u.frame_sync.frame_sequence =
				atomic_inc_return(&subdev->frm_sync_seq) - 1,
		};
		v4l2_event_queue(subdev->sd.devnode, &event);
	}
}

static const struct v4l2_subdev_core_ops flexbus_cif_cif_sof_sd_core_ops = {
	.subscribe_event = flexbus_cif_sof_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_ops flexbus_cif_cif_sof_sd_ops = {
	.core = &flexbus_cif_cif_sof_sd_core_ops,
};

int flexbus_cif_register_cif_sof_subdev(struct flexbus_cif_device *dev)
{
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	struct flexbus_cif_cif_sof_subdev *subdev = &dev->cif_sof_subdev;
	struct v4l2_subdev *sd;
	int ret;

	memset(subdev, 0, sizeof(*subdev));
	subdev->cif_dev = dev;
	sd = &subdev->sd;
	v4l2_subdev_init(sd, &flexbus_cif_cif_sof_sd_ops);
	sd->owner = THIS_MODULE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	snprintf(sd->name, sizeof(sd->name), "flexbus_cif-cif-sof");

	v4l2_set_subdevdata(sd, subdev);
	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0)
		goto end;

	ret = v4l2_device_register_subdev_nodes(v4l2_dev);
	if (ret < 0)
		goto free_subdev;

	return ret;

free_subdev:
	v4l2_device_unregister_subdev(sd);

end:
	v4l2_err(sd, "Failed to register subdev, ret:%d\n", ret);
	return ret;
}

void flexbus_cif_unregister_cif_sof_subdev(struct flexbus_cif_device *dev)
{
	struct v4l2_subdev *sd = &dev->cif_sof_subdev.sd;

	v4l2_device_unregister_subdev(sd);
}

static void flexbus_cif_dynamic_crop(struct flexbus_cif_stream *stream)
{
	struct flexbus_cif_device *cif_dev = stream->cif_dev;
	u32 raw_width, crop_width = 64,
	    crop_height = 64, crop_x = 0, crop_y = 0;

	if (!cif_dev->active_sensor)
		return;

	raw_width = stream->crop[CROP_SRC_ACT].width;
	crop_width = raw_width;
	crop_height = stream->crop[CROP_SRC_ACT].height;
	crop_x = stream->crop[CROP_SRC_ACT].left;
	crop_y = stream->crop[CROP_SRC_ACT].top;

	flexbus_cif_write_register(cif_dev, FLEXBUS_DVP_CROP_START,
			     crop_y << CIF_CROP_Y_SHIFT | crop_x);

	flexbus_cif_write_register(cif_dev, FLEXBUS_DVP_CROP_SIZE,
			     crop_height << CIF_CROP_Y_SHIFT | crop_width);

	stream->crop_dyn_en = false;
}

static void flexbus_cif_buf_done_prepare(struct flexbus_cif_stream *stream,
					    struct flexbus_cif_buffer *active_buf,
					    int mipi_id,
					    u32 mode)
{
	struct vb2_v4l2_buffer *vb_done = NULL;

	if (active_buf) {
		vb_done = &active_buf->vb;
		vb_done->vb2_buf.timestamp = ktime_get_ns();
		vb_done->sequence = stream->frame_idx - 1;
		active_buf->fe_timestamp = ktime_get_ns();
	}

	flexbus_cif_vb_done_tasklet(stream, active_buf);
}

static void flexbus_cif_deal_readout_time(struct flexbus_cif_stream *stream)
{
	unsigned long flags;

	spin_lock_irqsave(&stream->fps_lock, flags);
	stream->readout.fe_timestamp = ktime_get_ns();
	spin_unlock_irqrestore(&stream->fps_lock, flags);
}

static void flexbus_cif_update_stream(struct flexbus_cif_device *cif_dev,
				struct flexbus_cif_stream *stream,
				int mipi_id)
{
	struct flexbus_cif_buffer *active_buf = NULL;
	unsigned long flags;
	int ret = 0;

	if (stream->frame_phase == (CIF_CSI_FRAME0_READY | CIF_CSI_FRAME1_READY)) {
		cif_dev->err_state |= (FLEXBUS_CIF_ERR_ID0_TRIG_SIMULT << stream->id);
		cif_dev->irq_stats.trig_simult_cnt[stream->id]++;
		return;
	}
	spin_lock_irqsave(&stream->fps_lock, flags);
	if (stream->frame_phase & CIF_CSI_FRAME0_READY) {
		if (stream->curr_buf)
			active_buf = stream->curr_buf;
		stream->fps_stats.frm0_timestamp = ktime_get_ns();
	} else if (stream->frame_phase & CIF_CSI_FRAME1_READY) {
		if (stream->next_buf)
			active_buf = stream->next_buf;
		stream->fps_stats.frm1_timestamp = ktime_get_ns();
	}
	spin_unlock_irqrestore(&stream->fps_lock, flags);

	flexbus_cif_deal_readout_time(stream);

	ret = flexbus_cif_assign_new_buffer_pingpong(stream,
				 FLEXBUS_CIF_YUV_ADDR_STATE_UPDATE,
				 mipi_id);
	if (ret)
		return;

	flexbus_cif_buf_done_prepare(stream, active_buf, mipi_id, 0);

}

static u32 flexbus_cif_mbus_pixelcode_to_v4l2(u32 pixelcode)
{
	u32 pixelformat;

	switch (pixelcode) {
	case MEDIA_BUS_FMT_Y8_1X8:
		pixelformat = V4L2_PIX_FMT_GREY;
		break;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
		pixelformat = V4L2_PIX_FMT_SBGGR8;
		break;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
		pixelformat = V4L2_PIX_FMT_SGBRG8;
		break;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
		pixelformat = V4L2_PIX_FMT_SGRBG8;
		break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		pixelformat = V4L2_PIX_FMT_SRGGB8;
		break;
	case MEDIA_BUS_FMT_Y10_1X10:
		pixelformat = V4L2_PIX_FMT_Y10;
		break;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
		pixelformat = V4L2_PIX_FMT_SBGGR10;
		break;
	case MEDIA_BUS_FMT_SGBRG10_1X10:
		pixelformat = V4L2_PIX_FMT_SGBRG10;
		break;
	case MEDIA_BUS_FMT_SGRBG10_1X10:
		pixelformat = V4L2_PIX_FMT_SGRBG10;
		break;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		pixelformat = V4L2_PIX_FMT_SRGGB10;
		break;
	case MEDIA_BUS_FMT_Y12_1X12:
		pixelformat = V4L2_PIX_FMT_Y12;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
		pixelformat = V4L2_PIX_FMT_SBGGR12;
		break;
	case MEDIA_BUS_FMT_SGBRG12_1X12:
		pixelformat = V4L2_PIX_FMT_SGBRG12;
		break;
	case MEDIA_BUS_FMT_SGRBG12_1X12:
		pixelformat = V4L2_PIX_FMT_SGRBG12;
		break;
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		pixelformat = V4L2_PIX_FMT_SRGGB12;
		break;
	case MEDIA_BUS_FMT_SPD_2X8:
		pixelformat = V4l2_PIX_FMT_SPD16;
		break;
	case MEDIA_BUS_FMT_EBD_1X8:
		pixelformat = V4l2_PIX_FMT_EBD8;
		break;
	default:
		pixelformat = V4L2_PIX_FMT_SRGGB10;
	}

	return pixelformat;
}

void flexbus_cif_set_default_fmt(struct flexbus_cif_device *cif_dev)
{
	struct v4l2_subdev_selection input_sel;
	struct v4l2_pix_format_mplane pixm;
	struct v4l2_subdev_format fmt;
	int stream_num = 0;
	int ret, i;

	stream_num = 1;

	if (!cif_dev->terminal_sensor.sd)
		flexbus_cif_update_sensor_info(&cif_dev->stream[0]);

	if (cif_dev->terminal_sensor.sd) {
		for (i = 0; i < stream_num; i++) {
			memset(&fmt, 0, sizeof(fmt));
			fmt.pad = i;
			fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			v4l2_subdev_call(cif_dev->terminal_sensor.sd, pad, get_fmt, NULL, &fmt);

			memset(&pixm, 0, sizeof(pixm));
			pixm.pixelformat = flexbus_cif_mbus_pixelcode_to_v4l2(fmt.format.code);
			pixm.width = fmt.format.width;
			pixm.height = fmt.format.height;

			memset(&input_sel, 0, sizeof(input_sel));
			input_sel.pad = i;
			input_sel.target = V4L2_SEL_TGT_CROP_BOUNDS;
			input_sel.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			ret = v4l2_subdev_call(cif_dev->terminal_sensor.sd,
					       pad, get_selection, NULL,
					       &input_sel);
			if (!ret) {
				pixm.width = input_sel.r.width;
				pixm.height = input_sel.r.height;
			}
			flexbus_cif_set_fmt(&cif_dev->stream[i], &pixm, false);
		}
	}
}

static void flexbus_cif_send_sof(struct flexbus_cif_device *cif_dev)
{
	flexbus_cif_cif_event_inc_sof(cif_dev);
}

static void flexbus_cif_deal_sof(struct flexbus_cif_device *cif_dev)
{
	struct flexbus_cif_stream *detect_stream = &cif_dev->stream[0];
	unsigned long flags;

	spin_lock_irqsave(&detect_stream->fps_lock, flags);
	detect_stream->readout.fs_timestamp = ktime_get_ns();
	spin_unlock_irqrestore(&detect_stream->fps_lock, flags);

	flexbus_cif_send_sof(cif_dev);
	detect_stream->frame_idx++;
	v4l2_dbg(3, flexbus_cif_debug, &cif_dev->v4l2_dev,
		  "stream[%d] sof %d %lld\n",
		  detect_stream->id,
		  detect_stream->frame_idx - 1,
		  ktime_get_ns());
}

void flexbus_cif_err_print_work(struct work_struct *work)
{
	struct flexbus_cif_err_state_work *err_state_work = container_of(work,
							      struct flexbus_cif_err_state_work,
							      work);
	struct flexbus_cif_device *dev = container_of(err_state_work,
						struct flexbus_cif_device,
						err_state_work);
	u32 err_state = 0;
	u64 cur_time = 0;
	bool is_print = false;

	cur_time = ktime_get_ns();
	if (err_state_work->last_timestamp == 0) {
		is_print = true;
	} else {
		if (cur_time - err_state_work->last_timestamp > 500000000)
			is_print = true;
	}
	err_state_work->last_timestamp = cur_time;
	err_state = err_state_work->err_state;
	if (err_state & FLEXBUS_CIF_ERR_ID0_NOT_BUF && is_print)
		v4l2_err(&dev->v4l2_dev,
			 "stream[0] not active buffer, frame num %d, cnt %llu\n",
			 dev->stream[0].frame_idx, dev->irq_stats.not_active_buf_cnt[0]);
	if (err_state & FLEXBUS_CIF_ERR_ID1_NOT_BUF && is_print)
		v4l2_err(&dev->v4l2_dev,
			 "stream[1] not active buffer, frame num %d, cnt %llu\n",
			 dev->stream[1].frame_idx, dev->irq_stats.not_active_buf_cnt[1]);
	if (err_state & FLEXBUS_CIF_ERR_ID2_NOT_BUF && is_print)
		v4l2_err(&dev->v4l2_dev,
			 "stream[2] not active buffer, frame num %d, cnt %llu\n",
			 dev->stream[2].frame_idx, dev->irq_stats.not_active_buf_cnt[2]);
	if (err_state & FLEXBUS_CIF_ERR_ID3_NOT_BUF && is_print)
		v4l2_err(&dev->v4l2_dev,
			 "stream[3] not active buffer, frame num %d, cnt %llu\n",
			 dev->stream[3].frame_idx, dev->irq_stats.not_active_buf_cnt[3]);
	if (err_state & FLEXBUS_CIF_ERR_SIZE)
		v4l2_err(&dev->v4l2_dev,
			 "ERROR: size err, cnt %llu\n",
			 dev->irq_stats.cif_size_err_cnt);
	if (err_state & FLEXBUS_CIF_ERR_OVERFLOW)
		v4l2_err(&dev->v4l2_dev,
			 "ERROR: fifo overflow, cnt %llu, dnum %d\n",
			 dev->irq_stats.cif_overflow_cnt, err_state_work->fifo_dnum);
	if (err_state & FLEXBUS_CIF_ERR_BANDWIDTH_LACK)
		v4l2_err(&dev->v4l2_dev,
			 "ERROR: bandwidth lack, cnt %llu\n",
			 dev->irq_stats.cif_bwidth_lack_cnt);
}

void flexbus_cif_irq_pingpong(struct flexbus_cif_device *cif_dev, u32 isr)
{
	struct flexbus_cif_stream *stream;

	if (!cif_dev->active_sensor)
		return;

	stream = &cif_dev->stream[FLEXBUS_CIF_STREAM_CIF];
	if (isr & CIF_FIFO_OVERFLOW) {
		cif_dev->irq_stats.cif_overflow_cnt++;
		cif_dev->err_state |= FLEXBUS_CIF_ERR_OVERFLOW;
		cif_dev->err_state_work.fifo_dnum = flexbus_cif_read_register(cif_dev, FLEXBUS_RXFIFO_DNUM);
	}

	if (isr & CIF_BANDWIDTH_LACK) {
		cif_dev->irq_stats.cif_bwidth_lack_cnt++;
		cif_dev->err_state |= FLEXBUS_CIF_ERR_BANDWIDTH_LACK;
	}

	if (isr & CIF_SIZE_ERR) {
		cif_dev->irq_stats.cif_size_err_cnt++;
		//RESET
		stream->buf_wake_up_cnt = 0;
		cif_dev->err_state |= FLEXBUS_CIF_ERR_SIZE;
	}

	if (isr & CIF_DMA_END) {
		stream->frame_idx++;
		stream->buf_wake_up_cnt++;
		if (stream->stopping) {
			flexbus_cif_stream_stop(stream);
			stream->stopping = false;
			wake_up(&stream->wq_stopped);
			return;
		}
		if (stream->state != FLEXBUS_CIF_STATE_STREAMING)
			return;
		//need to change
		stream->frame_phase = (isr >> FLEXBUS_IMR_DMA_DST0_SHIFT) & 0x3;
		flexbus_cif_update_stream(cif_dev, stream, 0);
		cif_dev->irq_stats.frm_end_cnt[stream->id]++;
		if (stream->crop_dyn_en)
			flexbus_cif_dynamic_crop(stream);
	}
	if (isr & CIF_DMA_START)
		flexbus_cif_deal_sof(cif_dev);
}

