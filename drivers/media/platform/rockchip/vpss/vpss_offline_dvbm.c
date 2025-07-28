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

#include "vpss_offline_dvbm.h"

static struct dvbm_port *g_ofl_dvbm;

int rkvpss_ofl_dvbm_get(struct rkvpss_offline_dev *ofl)
{
	struct device_node *np = ofl->hw->dev->of_node;
	struct device_node *np_dvbm = of_parse_phandle(np, "dvbm", 0);
	int ret = 0;

	g_ofl_dvbm = NULL;
	if (!np_dvbm || !of_device_is_available(np_dvbm)) {
		dev_warn(ofl->hw->dev, "failed to get dvbm node\n");
	} else {
		struct platform_device *p_dvbm = of_find_device_by_node(np_dvbm);

		g_ofl_dvbm = rk_dvbm_get_port(p_dvbm, DVBM_VPSS_PORT);
		put_device(&p_dvbm->dev);
	}

	of_node_put(np_dvbm);

	return ret;
}

int rkvpss_ofl_dvbm_init(struct rkvpss_offline_dev *ofl, struct dma_buf *dbuf, u32 dma_addr,
	u32 wrap_line, int width, int height, int id)
{
	struct dvbm_isp_cfg_t dvbm_cfg;

	if (!g_ofl_dvbm)
		return -EINVAL;
	if (ofl->hw->dvbm_flag == DVBM_ONLINE) {
		v4l2_err(&ofl->v4l2_dev,
			"online dvbm already set, offline dvbm set fail.\n");
		return -EINVAL;
	}

	ofl->hw->dvbm_flag = DVBM_OFFLINE;

	dvbm_cfg.dma_addr = dma_addr;
	dvbm_cfg.buf = dbuf;
	dvbm_cfg.ybuf_bot = 0;
	dvbm_cfg.ybuf_top = width * wrap_line;
	dvbm_cfg.ybuf_lstd = width;
	dvbm_cfg.ybuf_fstd = width * height;
	dvbm_cfg.cbuf_bot = dvbm_cfg.ybuf_top;
	dvbm_cfg.cbuf_top = dvbm_cfg.cbuf_bot + (width * wrap_line / 2);
	dvbm_cfg.cbuf_lstd = width;
	dvbm_cfg.cbuf_fstd = dvbm_cfg.ybuf_fstd / 2;
	dvbm_cfg.chan_id = id;

	rk_dvbm_ctrl(g_ofl_dvbm, DVBM_VPSS_SET_CFG, &dvbm_cfg);
	rk_dvbm_link(g_ofl_dvbm, id);

	return 0;
}

void rkvpss_ofl_dvbm_deinit(struct rkvpss_offline_dev *ofl, int id)
{
	if (!g_ofl_dvbm || !ofl) {
		pr_err("g_dvbm %p or vpss_dev %p is NULL\n", g_ofl_dvbm, ofl);
		return;
	}
	ofl->hw->dvbm_flag = DVBM_DEINIT;
	rk_dvbm_unlink(g_ofl_dvbm, id);
	v4l2_dbg(2, rkvpss_debug, &ofl->v4l2_dev, "%s: clear vpss2enc_sel\n", __func__);
	rkvpss_hw_clear_bits(ofl->hw, RKVPSS_VPSS_CTRL, RKVPSS_VPSS2ENC_SEL);
}

int rkvpss_ofl_dvbm_event(u32 event, u32 seq)
{
	enum dvbm_cmd cmd;

	if (!g_ofl_dvbm)
		return -EINVAL;

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

	return rk_dvbm_ctrl(g_ofl_dvbm, cmd, &seq);
}

