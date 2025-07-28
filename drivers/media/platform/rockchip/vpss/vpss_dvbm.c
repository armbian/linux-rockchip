// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */


#define pr_fmt(fmt) "vpss_dvbm: %s:%d " fmt, __func__, __LINE__

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"

#include "vpss_dvbm.h"
#include "stream_v20.h"

static struct dvbm_port *g_dvbm;

int rkvpss_dvbm_get(struct rkvpss_device *vpss_dev)
{
	struct device_node *np = vpss_dev->hw_dev->dev->of_node;
	struct device_node *np_dvbm = of_parse_phandle(np, "dvbm", 0);
	int ret = 0;

	g_dvbm = NULL;
	if (!np_dvbm || !of_device_is_available(np_dvbm)) {
		dev_warn(vpss_dev->dev, "failed to get dvbm node\n");
	} else {
		struct platform_device *p_dvbm = of_find_device_by_node(np_dvbm);

		g_dvbm = rk_dvbm_get_port(p_dvbm, DVBM_VPSS_PORT);
		put_device(&p_dvbm->dev);
	}

	of_node_put(np_dvbm);
	if (IS_ERR_OR_NULL(g_dvbm)) {
		g_dvbm = NULL;
		ret = -EINVAL;
	}
	return ret;
}

int rkvpss_dvbm_init(struct rkvpss_stream *stream)
{
	struct rkvpss_device *vpss_dev = stream->dev;
	struct dvbm_isp_cfg_t dvbm_cfg;
	u32 width, height, wrap_line;

	if (!g_dvbm)
		return -EINVAL;
	if (vpss_dev->hw_dev->dvbm_flag == DVBM_OFFLINE) {
		v4l2_err(&vpss_dev->v4l2_dev,
			"offline dvbm already set, online dvbm set fail.\n");
		return -EINVAL;
	}

	vpss_dev->hw_dev->dvbm_refcnt++;
	vpss_dev->hw_dev->dvbm_flag = DVBM_ONLINE;

	width = stream->out_fmt.plane_fmt[0].bytesperline;
	height = stream->out_fmt.height;
	wrap_line = vpss_dev->stream_vdev.wrap_line;
	dvbm_cfg.dma_addr = vpss_dev->wrap_buf.dma_addr;
	dvbm_cfg.buf = vpss_dev->wrap_buf.dbuf;
	dvbm_cfg.ybuf_bot = 0;
	dvbm_cfg.ybuf_top = width * wrap_line;
	dvbm_cfg.ybuf_lstd = width;
	dvbm_cfg.ybuf_fstd = width * height;
	dvbm_cfg.cbuf_bot = dvbm_cfg.ybuf_top;
	dvbm_cfg.cbuf_top = dvbm_cfg.cbuf_bot + (width * wrap_line / 2);
	dvbm_cfg.cbuf_lstd = width;
	dvbm_cfg.cbuf_fstd = dvbm_cfg.ybuf_fstd / 2;

	rk_dvbm_ctrl(g_dvbm, DVBM_VPSS_SET_CFG, &dvbm_cfg);
	rk_dvbm_link(g_dvbm, vpss_dev->dev_id);
	return 0;
}

void rkvpss_dvbm_deinit(struct rkvpss_device *vpss_dev)
{
	if (!g_dvbm || !vpss_dev) {
		pr_err("g_dvbm %p or vpss_dev %p is NULL\n", g_dvbm, vpss_dev);
		return;
	}

	vpss_dev->hw_dev->dvbm_refcnt--;
	if (vpss_dev->hw_dev->dvbm_refcnt <= 0)
		vpss_dev->hw_dev->dvbm_flag = DVBM_DEINIT;

	rk_dvbm_unlink(g_dvbm, vpss_dev->dev_id);
}

int rkvpss_dvbm_event(struct rkvpss_device *vpss_dev, u32 event)
{
	enum dvbm_cmd cmd;
	u32 seq;

	if (!g_dvbm || !vpss_dev->stream_vdev.wrap_line)
		return -EINVAL;

	seq = vpss_dev->vpss_sdev.frame_seq;

	switch (event) {
	case ROCKIT_DVBM_START:
		cmd = DVBM_VPSS_FRM_START;
		break;
	case ROCKIT_DVBM_END:
		cmd = DVBM_VPSS_FRM_END;
		break;
	default:
		return -EINVAL;
	}

	return rk_dvbm_ctrl(g_dvbm, cmd, &seq);
}

