// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#define pr_fmt(fmt) "vpss_rockit: %s:%d " fmt, __func__, __LINE__


#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"

#include "stream_v20.h"
#include "vpss_rockit.h"
#include "vpss_dvbm.h"


static struct rockit_rkvpss_cfg *rockit_vpss_cfg;

struct rkvpss_rockit_buffer {
	struct rkvpss_buffer vpss_buffer;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *dba;
	struct sg_table *sgt;
	void *mpi_buf;
	struct list_head queue;
	int buf_id;
	u32 buff_addr;
	void *vaddr;
};

static struct rkvpss_stream *rkvpss_rockit_get_stream(struct rockit_rkvpss_cfg *input_cfg)
{
	struct rkvpss_device *vpss_dev = NULL;
	struct rkvpss_stream *stream = NULL;
	u8 i;

	if (!rockit_vpss_cfg) {
		pr_err("rockit_vpss_cfg is null, get stream failed\n");
		return NULL;
	}
	if (!input_cfg) {
		pr_err("input_cfg is null, get stream failed\n");
		return NULL;
	}

	for (i = 0; i < rockit_vpss_cfg->vpss_num; i++) {
		if (!strcmp(rockit_vpss_cfg->rkvpss_dev_cfg[i].vpss_name,
			    input_cfg->current_name)) {
			vpss_dev = rockit_vpss_cfg->rkvpss_dev_cfg[i].vpss_dev;
			break;
		}
	}

	if (vpss_dev == NULL) {
		pr_err("can not find vpss_dev!\n");
		return NULL;
	}

	switch (input_cfg->nick_id) {
	case 0:
		stream = &vpss_dev->stream_vdev.stream[RKVPSS_OUTPUT_CH0];
		break;
	case 1:
		stream = &vpss_dev->stream_vdev.stream[RKVPSS_OUTPUT_CH1];
		break;
	case 2:
		stream = &vpss_dev->stream_vdev.stream[RKVPSS_OUTPUT_CH2];
		break;
	case 3:
		stream = &vpss_dev->stream_vdev.stream[RKVPSS_OUTPUT_CH3];
		break;
	case 4:
		stream = &vpss_dev->stream_vdev.stream[RKVPSS_OUTPUT_CH4];
		break;
	case 5:
		stream = &vpss_dev->stream_vdev.stream[RKVPSS_OUTPUT_CH5];
		break;
	default:
		stream = NULL;
		break;
	}

	return stream;
}

static void rkvpss_rockit_cfg_stream_buffer(struct rkvpss_stream *stream,
			struct rkvpss_rockit_buffer *vpssrk_buf,
			struct rockit_rkvpss_cfg *input_cfg)
{
	u32 y_offs = 0, uv_offs = 0;
	u32 dma_addr = vpssrk_buf->buff_addr;
	struct capture_fmt *fmt;

	fmt = &stream->out_cap_fmt;

	if (input_cfg->vir_width) {
		stream->out_fmt.plane_fmt[0].bytesperline = input_cfg->vir_width *
							    DIV_ROUND_UP(fmt->bpp[0], 8);
		if (fmt->fmt_type == FMT_FBC) {
			y_offs = input_cfg->y_offset;	// FBC header is at buffer start
			uv_offs = input_cfg->uv_offset + stream->fbc_head_size;	// payload
		} else {
			y_offs = input_cfg->y_offset;
			uv_offs = input_cfg->uv_offset;
		}

		stream->out_fmt.plane_fmt[1].bytesperline = input_cfg->vir_width *
							    DIV_ROUND_UP(fmt->bpp[0], 8);
		stream->out_fmt.plane_fmt[1].sizeimage = stream->out_fmt.plane_fmt[1].bytesperline *
							 stream->out_fmt.height;
	} else {
		y_offs = 0;
		if (fmt->fmt_type == FMT_FBC) {
			y_offs = 0;  // FBC header is at buffer start
			uv_offs = stream->fbc_head_size;  // Y payload after header
		} else if (stream->dev->stream_vdev.wrap_line && stream->id == RKVPSS_OUTPUT_CH0) {
			uv_offs = stream->out_fmt.plane_fmt[0].bytesperline *
				  stream->dev->stream_vdev.wrap_line;
			stream->dev->wrap_buf.dbuf = vpssrk_buf->dmabuf;
			stream->dev->wrap_buf.dma_addr = dma_addr;
		} else {
			uv_offs = stream->out_fmt.plane_fmt[0].bytesperline * stream->out_fmt.height;
		}
	}

	vpssrk_buf->vpss_buffer.dma[0] = dma_addr + y_offs;
	vpssrk_buf->vpss_buffer.dma[1] = dma_addr + uv_offs;
	vpssrk_buf->vpss_buffer.vaddr[0] = NULL;
	vpssrk_buf->vpss_buffer.vaddr[1] = NULL;
	vpssrk_buf->vpss_buffer.vb.vb2_buf.planes[0].mem_priv = NULL;
	if (vpssrk_buf->vaddr) {
		vpssrk_buf->vpss_buffer.vaddr[0] = vpssrk_buf->vaddr + y_offs;
		vpssrk_buf->vpss_buffer.vaddr[1] = vpssrk_buf->vaddr + uv_offs;
		vpssrk_buf->vpss_buffer.vb.vb2_buf.planes[0].mem_priv = vpssrk_buf->sgt;
	}
}

int rkvpss_rockit_buf_queue(struct rockit_rkvpss_cfg *input_cfg)
{
	struct rkvpss_stream *stream = NULL;
	struct rkvpss_rockit_buffer *vpssrk_buf = NULL;
	struct rkvpss_device *vpss_dev = NULL;
	struct rkvpss_stream_cfg *stream_cfg = NULL;
	unsigned long lock_flags = 0;
	int dev_id, i;

	if (!input_cfg) {
		pr_err("input_cfg is null\n");
		return -EINVAL;
	}

	stream = rkvpss_rockit_get_stream(input_cfg);
	if (!stream || stream->id >= ROCKIT_STREAM_NUM_MAX) {
		pr_err("inval stream");
		return -EINVAL;
	}

	dev_id = stream->dev->dev_id;
	vpss_dev = stream->dev;

	if (!rockit_vpss_cfg) {
		pr_err("rockit_vpss_cfg is null\n");
		return -EINVAL;
	}
	stream_cfg = &rockit_vpss_cfg->rkvpss_dev_cfg[dev_id].rkvpss_stream_cfg[stream->id];
	stream_cfg->node = input_cfg->node;

	if (!input_cfg->buf)
		return -EINVAL;

	for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++) {
		if (stream_cfg->buff_id[i] == input_cfg->mpi_id) {
			input_cfg->is_alloc = 0;
			break;
		}
	}

	if (input_cfg->is_alloc) {
		struct dma_buf_attachment *dba;
		struct sg_table *sgt;
		struct iosys_map map;

		for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++) {
			if (!stream_cfg->buff_id[i] && !stream_cfg->rkvpss_buff[i]) {
				stream_cfg->buff_id[i] = input_cfg->mpi_id;
				vpssrk_buf = kzalloc(sizeof(struct rkvpss_rockit_buffer),
					     GFP_KERNEL);
				if (!vpssrk_buf) {
					stream_cfg->buff_id[i] = 0;
					pr_err("vpssrk_buf alloc failed\n");
					return -ENOMEM;
				}
				break;
			}
		}
		if (i == ROCKIT_BUF_NUM_MAX)
			return -EINVAL;

		dba = dma_buf_attach(input_cfg->buf, vpss_dev->hw_dev->dev);
		if (IS_ERR(dba)) {
			kfree(vpssrk_buf);
			stream_cfg->buff_id[i] = 0;
			return PTR_ERR(dba);
		}

		sgt = dma_buf_map_attachment(dba, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt)) {
			dma_buf_detach(input_cfg->buf, dba);
			kfree(vpssrk_buf);
			stream_cfg->buff_id[i] = 0;
			return PTR_ERR(sgt);
		}

		vpssrk_buf->vaddr = NULL;
		/* default vmap two to get image, rkvpss_buf_dbg > 0 to vmap all */
		if (i < 2 || rkvpss_buf_dbg > 0) {
			v4l2_dbg(3, rkvpss_debug, &vpss_dev->v4l2_dev,
				 "stream:%d rockit vmap buf:%p\n", stream->id, input_cfg->buf);
			if (dma_buf_vmap(input_cfg->buf, &map) == 0)
				vpssrk_buf->vaddr = map.vaddr;
		}

		vpssrk_buf->buff_addr = sg_dma_address(sgt->sgl);
		get_dma_buf(input_cfg->buf);
		vpssrk_buf->mpi_buf = input_cfg->mpibuf;
		vpssrk_buf->dmabuf = input_cfg->buf;
		vpssrk_buf->dba = dba;
		vpssrk_buf->sgt = sgt;
		stream_cfg->rkvpss_buff[i] = vpssrk_buf;
	}

	/*no todo, vpss use update config mi addr */

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	vpssrk_buf = NULL;
	for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++) {
		if (stream_cfg->buff_id[i] == input_cfg->mpi_id) {
			vpssrk_buf = stream_cfg->rkvpss_buff[i];
			break;
		}
	}
	if (!vpssrk_buf) {
		spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
		return -EINVAL;
	}

	if (stream->out_cap_fmt.mplanes == 1)
		rkvpss_rockit_cfg_stream_buffer(stream, vpssrk_buf, input_cfg);

	v4l2_dbg(2, rkvpss_debug, &vpss_dev->v4l2_dev,
		 "stream:%d rockit_queue buf:%p y:0x%x uv:0x%x\n",
		 stream->id, vpssrk_buf,
		 vpssrk_buf->vpss_buffer.dma[0], vpssrk_buf->vpss_buffer.dma[1]);

	list_add_tail(&vpssrk_buf->vpss_buffer.queue, &stream->buf_queue);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	return 0;
}

int rkvpss_rockit_buf_done(struct rkvpss_stream *stream, int cmd, struct rkvpss_buffer *curr_buf)
{
	struct rkvpss_device *vpss_dev = stream->dev;
	struct rkvpss_rockit_buffer *vpssrk_buf = NULL;
	struct rkvpss_stream_cfg *stream_cfg = NULL;
	unsigned long lock_flags = 0;
	u32 dev_id = vpss_dev->dev_id;

	if (!rockit_vpss_cfg ||
	    !rockit_vpss_cfg->rkvpss_rockit_mpibuf_done ||
	    stream->id >= ROCKIT_STREAM_NUM_MAX)
		return -EINVAL;

	v4l2_dbg(4, rkvpss_debug, &vpss_dev->v4l2_dev, "%s enter cmd:%d\n", __func__, cmd);

	if (!stream->rockit_on) {
		if (!vpss_dev->stream_vdev.wrap_line) {
			spin_lock_irqsave(&stream->vbq_lock, lock_flags);
			list_add_tail(&curr_buf->queue, &stream->buf_queue);
			spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
		}
		v4l2_dbg(2, rkvpss_debug, &vpss_dev->v4l2_dev, "%s already stop\n", __func__);
		return 0;
	}

	stream_cfg = &rockit_vpss_cfg->rkvpss_dev_cfg[dev_id].rkvpss_stream_cfg[stream->id];

	if (cmd == ROCKIT_DVBM_END) {
		vpssrk_buf = container_of(curr_buf, struct rkvpss_rockit_buffer, vpss_buffer);

		rockit_vpss_cfg->mpibuf = vpssrk_buf->mpi_buf;
		rockit_vpss_cfg->frame.u64PTS = curr_buf->vb.vb2_buf.timestamp;
		rockit_vpss_cfg->frame.u32TimeRef = curr_buf->vb.sequence;

		v4l2_dbg(2, rkvpss_debug, &vpss_dev->v4l2_dev,
			 "stream:%d seq:%d rockit buf done:0x%x\n",
			 stream->id,
			 curr_buf->vb.sequence,
			 curr_buf->dma[0]);
	} else {
		if (!(stream->dev->stream_vdev.wrap_line && stream->id == RKVPSS_OUTPUT_CH0))
			return 0;

		rockit_vpss_cfg->frame.u64PTS = vpss_dev->vpss_sdev.frame_timestamp;
		rockit_vpss_cfg->frame.u32TimeRef = vpss_dev->vpss_sdev.frame_seq;
		rockit_vpss_cfg->frame.ispEncCnt =
			RKVPSS2X_RO_VPSS2ENC_FRM_CNT(rkvpss_hw_read(vpss_dev->hw_dev, RKVPSS2X_VPSS2ENC_DEBUG));
		v4l2_dbg(2, rkvpss_debug, &vpss_dev->v4l2_dev,
			 "stream:%d seq:%d enc_frm_cnt:%d rockit buf done:0x%x\n",
			 stream->id, curr_buf->vb.sequence,
			 rockit_vpss_cfg->frame.ispEncCnt, curr_buf->dma[0]);
	}

	rockit_vpss_cfg->frame.u32Height = stream->out_fmt.height;
	rockit_vpss_cfg->frame.u32Width = stream->out_fmt.width;
	rockit_vpss_cfg->frame.enPixelFormat = stream->out_fmt.pixelformat;
	rockit_vpss_cfg->frame.u32VirWidth = stream->out_fmt.width;
	rockit_vpss_cfg->frame.u32VirHeight = stream->out_fmt.height;
	rockit_vpss_cfg->current_name = vpss_dev->name;
	rockit_vpss_cfg->node = stream_cfg->node;
	rockit_vpss_cfg->event = cmd;

	if (stream->is_attach_info) {
		struct rkisp_vpss_frame_info *src_info = &vpss_dev->frame_info;

		rockit_vpss_cfg->frame.u64PTS = src_info->timestamp;
		rockit_vpss_cfg->frame.hdr = src_info->hdr;
		rockit_vpss_cfg->frame.rolling_shutter_skew = src_info->rolling_shutter_skew;

		rockit_vpss_cfg->frame.sensor_exposure_time = src_info->sensor_exposure_time;
		rockit_vpss_cfg->frame.sensor_analog_gain = src_info->sensor_analog_gain;
		rockit_vpss_cfg->frame.sensor_digital_gain = src_info->sensor_digital_gain;
		rockit_vpss_cfg->frame.isp_digital_gain = src_info->isp_digital_gain;

		rockit_vpss_cfg->frame.sensor_exposure_time_m = src_info->sensor_exposure_time_m;
		rockit_vpss_cfg->frame.sensor_analog_gain_m = src_info->sensor_analog_gain_m;
		rockit_vpss_cfg->frame.sensor_digital_gain_m = src_info->sensor_digital_gain_m;
		rockit_vpss_cfg->frame.isp_digital_gain_m = src_info->isp_digital_gain_m;

		rockit_vpss_cfg->frame.sensor_exposure_time_l = src_info->sensor_exposure_time_l;
		rockit_vpss_cfg->frame.sensor_analog_gain_l = src_info->sensor_analog_gain_l;
		rockit_vpss_cfg->frame.sensor_digital_gain_l = src_info->sensor_digital_gain_l;
		rockit_vpss_cfg->frame.isp_digital_gain_l = src_info->isp_digital_gain_l;
	}

	if (list_empty(&stream->buf_queue))
		rockit_vpss_cfg->is_empty = true;
	else
		rockit_vpss_cfg->is_empty = false;

	rockit_vpss_cfg->rkvpss_rockit_mpibuf_done(rockit_vpss_cfg);

	return 0;
}

/* dynamic refmt */
int rkvpss_rockit_pause_stream(struct rockit_rkvpss_cfg *input_cfg)
{
	struct rkvpss_stream *stream = NULL;

	stream = rkvpss_rockit_get_stream(input_cfg);

	if (stream == NULL) {
		pr_err("the stream is null\n");
		return -EINVAL;
	}

	v4l2_dbg(1, rkvpss_debug, &stream->dev->v4l2_dev,
		 "%s stream:%d\n", __func__, stream->id);

	rockit_vpss_ops.rkvpss_stream_stop(stream);

	if (stream->dev->stream_vdev.wrap_line && stream->id == RKVPSS_OUTPUT_CH0)
		rkvpss_dvbm_deinit(stream->dev);
	return 0;
}
EXPORT_SYMBOL(rkvpss_rockit_pause_stream);

/* dynamic refmt */
int rkvpss_rockit_config_stream(struct rockit_rkvpss_cfg *input_cfg,
					int width, int height, int wrap_line)
{
	struct rkvpss_stream *stream = NULL;
	struct rkvpss_buffer *vpss_buf, *buf_temp;
	int ret;
	unsigned long lock_flags = 0;


	stream = rkvpss_rockit_get_stream(input_cfg);
	if (stream == NULL) {
		pr_err("the stream is null\n");
		return -EINVAL;
	}

	v4l2_dbg(1, rkvpss_debug, &stream->dev->v4l2_dev,
		 "%s stream:%d %dx%d wrap_line:%d\n",
		 __func__, stream->id, width, height, wrap_line);

	stream->dev->stream_vdev.wrap_line = wrap_line;
	stream->out_fmt.width = width;
	stream->out_fmt.height = height;
	stream->out_fmt.plane_fmt[0].bytesperline = 0;
	ret = rockit_vpss_ops.rkvpss_set_fmt(stream, &stream->out_fmt, false);
	if (ret < 0) {
		pr_err("stream id %d config failed\n", stream->id);
		return -EINVAL;
	}

	/* wrap mode */
	if (stream->dev->stream_vdev.wrap_line && stream->id == RKVPSS_OUTPUT_CH0)
		rkvpss_dvbm_init(stream);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);

	if (stream->curr_buf) {
		list_add_tail(&stream->curr_buf->queue, &stream->buf_queue);
		if (stream->curr_buf == stream->next_buf)
			stream->next_buf = NULL;
		stream->curr_buf = NULL;
	}
	if (stream->next_buf) {
		list_add_tail(&stream->next_buf->queue, &stream->buf_queue);
		stream->next_buf = NULL;
	}

	list_for_each_entry_safe(vpss_buf, buf_temp, &stream->buf_queue, queue) {
		struct rkvpss_rockit_buffer *vpssrk_buf = container_of(vpss_buf,
							  struct rkvpss_rockit_buffer, vpss_buffer);

		if (stream->out_cap_fmt.mplanes == 1)
			rkvpss_rockit_cfg_stream_buffer(stream, vpssrk_buf, input_cfg);
	}

	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	return 0;
}
EXPORT_SYMBOL(rkvpss_rockit_config_stream);

/* dynamic refmt */
int rkvpss_rockit_resume_stream(struct rockit_rkvpss_cfg *input_cfg)
{
	struct rkvpss_stream *stream = NULL;
	int ret = 0;

	stream = rkvpss_rockit_get_stream(input_cfg);
	if (stream == NULL) {
		pr_err("the stream is NULL");
		return -EINVAL;
	}

	v4l2_dbg(1, rkvpss_debug, &stream->dev->v4l2_dev,
		 "%s stream:%d\n", __func__, stream->id);

	ret = rockit_vpss_ops.rkvpss_stream_start(stream);
	if (ret < 0) {
		pr_err("stream id:%d start failed\n", stream->id);
		return -EINVAL;
	}

	//tosee

	return 0;
}
EXPORT_SYMBOL(rkvpss_rockit_resume_stream);

void rkvpss_rockit_buf_state_clear(struct rkvpss_stream *stream)
{
	struct rkvpss_stream_cfg *stream_cfg;
	u32 i = 0, dev_id = stream->dev->dev_id;

	if (!rockit_vpss_cfg || stream->id >= ROCKIT_STREAM_NUM_MAX)
		return;

	stream_cfg = &rockit_vpss_cfg->rkvpss_dev_cfg[dev_id].rkvpss_stream_cfg[stream->id];

	for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++)
		stream_cfg->buff_id[i] = 0;
}

int rkvpss_rockit_buf_free(struct rkvpss_stream *stream)
{
	struct rkvpss_rockit_buffer *vpssrk_buf;
	struct rkvpss_stream_cfg *stream_cfg;
	u32 i = 0, dev_id = stream->dev->dev_id;

	if (!rockit_vpss_cfg || stream->id >= ROCKIT_STREAM_NUM_MAX)
		return -EINVAL;

	stream_cfg = &rockit_vpss_cfg->rkvpss_dev_cfg[dev_id].rkvpss_stream_cfg[stream->id];

	mutex_lock(&stream_cfg->freebuf_lock);
	for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++) {
		if (!stream_cfg->rkvpss_buff[i])
			continue;

		vpssrk_buf = (struct rkvpss_rockit_buffer *)stream_cfg->rkvpss_buff[i];
		if (vpssrk_buf->dba) {
			if (vpssrk_buf->vaddr) {
				struct iosys_map map = IOSYS_MAP_INIT_VADDR(vpssrk_buf->vaddr);

				dma_buf_vunmap(vpssrk_buf->dmabuf, &map);
				vpssrk_buf->vaddr = NULL;
			}
			if (vpssrk_buf->sgt) {
				dma_buf_unmap_attachment(vpssrk_buf->dba,
							 vpssrk_buf->sgt, DMA_BIDIRECTIONAL);
				vpssrk_buf->sgt = NULL;
			}
			dma_buf_detach(vpssrk_buf->dmabuf, vpssrk_buf->dba);
			dma_buf_put(vpssrk_buf->dmabuf);
			vpssrk_buf->dba = NULL;
		}
		kfree(stream_cfg->rkvpss_buff[i]);
		stream_cfg->rkvpss_buff[i] = NULL;
	}
	mutex_unlock(&stream_cfg->freebuf_lock);

	return 0;
}

/* when rockit stream off call */
int rkvpss_rockit_free_stream_buf(struct rockit_rkvpss_cfg *input_cfg)
{
	struct rkvpss_stream *stream;
	struct rkvpss_buffer *buf;
	unsigned long lock_flags = 0;

	if (!input_cfg)
		return -EINVAL;
	stream = rkvpss_rockit_get_stream(input_cfg);
	if (!stream)
		return -EINVAL;
	if (stream->streaming)
		return 0;

	v4l2_dbg(1, rkvpss_debug, &stream->dev->v4l2_dev,
		 "%s stream:%d\n", __func__, stream->id);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);

	if (stream->curr_buf) {
		list_add_tail(&stream->curr_buf->queue, &stream->buf_queue);
		if (stream->curr_buf == stream->next_buf)
			stream->next_buf = NULL;
		stream->curr_buf = NULL;
	}

	if (stream->next_buf) {
		list_add_tail(&stream->next_buf->queue, &stream->buf_queue);
		stream->next_buf = NULL;
	}

	while (!list_empty(&stream->buf_queue)) {
		buf = list_first_entry(&stream->buf_queue, struct rkvpss_buffer, queue);
		list_del(&buf->queue);
	}
	rkvpss_rockit_buf_state_clear(stream);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	rkvpss_rockit_buf_free(stream);

	return 0;
}
EXPORT_SYMBOL(rkvpss_rockit_free_stream_buf);

void rkvpss_rockit_dev_init(struct rkvpss_device *dev)
{
	struct rkvpss_stream_cfg *stream_cfg;
	int i, j;

	if (rockit_vpss_cfg == NULL) {
		rockit_vpss_cfg = kzalloc(sizeof(struct rockit_rkvpss_cfg), GFP_KERNEL);
		if (rockit_vpss_cfg == NULL)
			return;
	}
	rockit_vpss_cfg->vpss_num = dev->hw_dev->dev_num;
	for (i = 0; i < rockit_vpss_cfg->vpss_num; i++) {
		if (dev->hw_dev->vpss[i]) {
			rockit_vpss_cfg->rkvpss_dev_cfg[i].vpss_name = dev->hw_dev->vpss[i]->name;
			rockit_vpss_cfg->rkvpss_dev_cfg[i].vpss_dev = dev->hw_dev->vpss[i];

			for (j = 0; j < vpss_outchn_max(dev->hw_dev->vpss_ver); j++) {
				stream_cfg = &rockit_vpss_cfg->rkvpss_dev_cfg[i].rkvpss_stream_cfg[j];
				mutex_init(&stream_cfg->freebuf_lock);
			}
		}
	}
}

void rkvpss_rockit_dev_deinit(void)
{
	kfree(rockit_vpss_cfg);
	rockit_vpss_cfg = NULL;
}

void rkvpss_rockit_frame_start(struct rkvpss_device *dev)
{
	struct rkvpss_stream *stream;
	int i;

	if (rockit_vpss_cfg == NULL)
		return;

	for (i = 0; i <= RKVPSS_OUTPUT_CH1; i++) {
		stream = &dev->stream_vdev.stream[i];
		if (!stream->streaming)
			continue;
		if (stream->curr_buf && !stream->curr_buf->vb.vb2_buf.memory)
			rkvpss_rockit_buf_done(stream, ROCKIT_DVBM_START, stream->curr_buf);
	}
}

void *rkvpss_rockit_function_register(void *function, int cmd)
{
	if (rockit_vpss_cfg == NULL) {
		pr_err("rockit_vpss_cfg is null, function register failed\n");
		return NULL;
	}

	switch (cmd) {
	case ROCKIT_BUF_QUE:
		function = rkvpss_rockit_buf_queue;
		break;
	case ROCKIT_MPIBUF_DONE:
		rockit_vpss_cfg->rkvpss_rockit_mpibuf_done = function;
		if (!rockit_vpss_cfg->rkvpss_rockit_mpibuf_done)
			pr_err("get rkvpss_rockit_mpibuf_done failed\n");
		break;
	default:
		break;
	}
	return function;
}
EXPORT_SYMBOL(rkvpss_rockit_function_register);

int rkvpss_rockit_get_vpssdev(char **name)
{
	int i = 0;

	if (rockit_vpss_cfg == NULL) {
		pr_err("rockit_vpss_cfg is null\n");
		return -EINVAL;
	}

	if (name == NULL) {
		pr_err("the name is null\n");
		return -EINVAL;
	}

	for (i = 0; i < rockit_vpss_cfg->vpss_num; i++)
		name[i] = (char *)rockit_vpss_cfg->rkvpss_dev_cfg[i].vpss_name;
	if (name[0] == NULL)
		return -EINVAL;
	else
		return 0;
}
EXPORT_SYMBOL(rkvpss_rockit_get_vpssdev);

