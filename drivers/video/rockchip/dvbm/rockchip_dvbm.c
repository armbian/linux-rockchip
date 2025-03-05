// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 *
 * author:
 *	Yandong Lin, yandong.lin@rock-chips.com
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <soc/rockchip/rockchip_dvbm.h>

#define RK_DVBM		"rk_dvbm"

unsigned int dvbm_debug;
module_param(dvbm_debug, uint, 0644);
MODULE_PARM_DESC(dvbm_debug, "bit switch for dvbm debug information");

static struct dvbm_ctx *g_ctx;

#define DVBM_DEBUG	0x00000001
#define DVBM_DEBUG_FRM	0x00000010

#define dvbm_debug(fmt, args...)				\
	do {							\
		if (unlikely(dvbm_debug & (DVBM_DEBUG)))	\
			pr_info(fmt, ##args);			\
	} while (0)

#define dvbm_debug_frm(fmt, args...)				\
	do {							\
		if (unlikely(dvbm_debug & (DVBM_DEBUG_FRM)))	\
			pr_info(fmt, ##args);			\
	} while (0)

#define dvbm_err(fmt, args...)					\
	pr_err("%s:%d: " fmt, __func__, __LINE__, ##args)

#define UPDATE_LINE_CNT 0

#define DVBM_CHANNEL_NUM (3)

struct dvbm_ctx {
	struct device *dev;

	u32 isp_connet;
	u32 vepu_connet;

	/* vepu infos */
	struct dvbm_port port_vepu;
	atomic_t vepu_link;
	struct dvbm_cb vepu_cb;
	struct dvbm_addr_cfg vepu_cfg;

	/* isp infos */
	struct dvbm_port port_isp;
	struct dvbm_cb isp_cb;
	struct dvbm_isp_cfg_t isp_cfg[DVBM_CHANNEL_NUM];
	struct dvbm_addr_cfg dvbm_addr[DVBM_CHANNEL_NUM];
	u32 chan_id;
	struct dvbm_isp_frm_info isp_frm_info;
	atomic_t isp_link;
	u32 isp_max_lcnt;
	u32 isp_frm_start;
	u32 isp_frm_end;
	ktime_t isp_frm_time;
	u32 wrap_line;
};

static struct dvbm_ctx *port_to_ctx(struct dvbm_port *port)
{
	struct dvbm_ctx *ctx = NULL;

	if (IS_ERR_OR_NULL(port))
		return g_ctx;
	if (port->dir == DVBM_ISP_PORT)
		ctx = container_of(port, struct dvbm_ctx, port_isp);
	else if (port->dir == DVBM_VEPU_PORT)
		ctx = container_of(port, struct dvbm_ctx, port_vepu);

	return ctx;
}

static void dvbm2enc_callback(struct dvbm_ctx *ctx, enum dvbm_cb_event event, void *arg)
{
	struct dvbm_cb *callback = &ctx->vepu_cb;
	dvbm_callback cb = callback->cb;

	if (!cb) {
		dvbm_err("vepu callback is null\n");
		return;
	}

	cb(callback->ctx, event, arg);
}

static void init_isp_infos(struct dvbm_ctx *ctx)
{
	ctx->isp_frm_start = 0;
	ctx->isp_frm_end = 0;
	ctx->isp_frm_time = 0;
}

static void rk_dvbm_show_time(struct dvbm_ctx *ctx)
{
	ktime_t time = ktime_get();

	if (ctx->isp_frm_time)
		dvbm_debug("isp frame start[%d : %d] times %lld us\n",
			   ctx->isp_frm_start, ctx->isp_frm_end,
			   ktime_us_delta(time, ctx->isp_frm_time));
	ctx->isp_frm_time = time;
}

static void rk_dvbm_update_isp_frm_info(struct dvbm_ctx *ctx, u32 line_cnt)
{
#if UPDATE_LINE_CNT
	struct dvbm_isp_frm_info *frm_info = &ctx->isp_frm_info;

	frm_info->line_cnt = ALIGN(line_cnt, 32);
	dvbm_debug_frm("dvbm frame %d line %d\n", frm_info->frame_cnt, frm_info->line_cnt);
	dvbm2enc_callback(ctx, DVBM_VEPU_NOTIFY_FRM_INFO, frm_info);
#endif
}

struct dvbm_port *rk_dvbm_get_port(struct platform_device *pdev,
				   enum dvbm_port_dir dir)
{
	struct dvbm_ctx *ctx = NULL;
	struct dvbm_port *port = NULL;

	if (WARN_ON(!pdev))
		return NULL;

	ctx = (struct dvbm_ctx *)platform_get_drvdata(pdev);
	WARN_ON(!ctx);
	dvbm_debug("%s dir %d\n", __func__, dir);
	if (dir == DVBM_ISP_PORT)
		port = &ctx->port_isp;
	else if (dir == DVBM_VEPU_PORT)
		port = &ctx->port_vepu;

	return port;
}
EXPORT_SYMBOL(rk_dvbm_get_port);

int rk_dvbm_put(struct dvbm_port *port)
{
	struct dvbm_ctx *ctx = NULL;

	if (WARN_ON(!port))
		return -EINVAL;

	ctx = port_to_ctx(port);
	if (!ctx)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(rk_dvbm_put);

int rk_dvbm_link(struct dvbm_port *port, int id)
{
	struct dvbm_ctx *ctx;
	enum dvbm_port_dir dir;
	int ret = 0;

	if (WARN_ON(!port))
		return -EINVAL;

	ctx = port_to_ctx(port);
	dir = port->dir;

	if (dir == DVBM_ISP_PORT) {
		if (id >= DVBM_CHANNEL_NUM)
			dvbm_err("id %d is invalid\n", id);

		dvbm2enc_callback(ctx, DVBM_ISP_REQ_CONNECT, &id);
	} else if (dir == DVBM_VEPU_PORT) {
	}

	dvbm_debug("%s %d connect frm_cnt[%d : %d]\n",
		   dir == DVBM_ISP_PORT ? "isp" : "vepu", id,
		   ctx->isp_frm_start, ctx->isp_frm_end);

	return ret;
}
EXPORT_SYMBOL(rk_dvbm_link);

int rk_dvbm_unlink(struct dvbm_port *port, int id)
{
	struct dvbm_ctx *ctx;
	enum dvbm_port_dir dir;

	if (WARN_ON(!port))
		return -EINVAL;

	ctx = port_to_ctx(port);
	dir = port->dir;

	if (dir == DVBM_ISP_PORT) {
		if (id >= DVBM_CHANNEL_NUM)
			dvbm_err("id %d is invalid\n", id);

		dvbm2enc_callback(ctx, DVBM_ISP_REQ_DISCONNECT, &id);
	} else if (dir == DVBM_VEPU_PORT) {
	}
	dvbm_debug("%s disconnect\n", dir == DVBM_ISP_PORT ? "isp" : "vepu");

	return 0;
}
EXPORT_SYMBOL(rk_dvbm_unlink);

int rk_dvbm_set_cb(struct dvbm_port *port, struct dvbm_cb *cb)
{
	struct dvbm_ctx *ctx;
	enum dvbm_port_dir dir;

	if (WARN_ON(!port) || WARN_ON(!cb))
		return -EINVAL;

	ctx = port_to_ctx(port);
	dir = port->dir;

	if (dir == DVBM_ISP_PORT) {

	} else if (dir == DVBM_VEPU_PORT) {
		ctx->vepu_cb.cb = cb->cb;
		ctx->vepu_cb.ctx = cb->ctx;
	}

	return 0;
}
EXPORT_SYMBOL(rk_dvbm_set_cb);

int rk_dvbm_ctrl(struct dvbm_port *port, enum dvbm_cmd cmd, void *arg)
{
	struct dvbm_ctx *ctx;

	if ((cmd < DVBM_ISP_CMD_BASE) || (cmd > DVBM_VEPU_CMD_BUTT)) {
		dvbm_err("%s input cmd invalid\n", __func__);
		return -EINVAL;
	}

	ctx = port_to_ctx(port);

	switch (cmd) {
	case DVBM_ISP_SET_CFG: {
		struct dvbm_isp_cfg_t *cfg = (struct dvbm_isp_cfg_t *)arg;
		struct dvbm_addr_cfg *dvbm_adr;
		u32 chan_id = cfg->chan_id;

		if (chan_id >= DVBM_CHANNEL_NUM) {
			dvbm_err("%s cmd %d chan id %d is invalid\n", __func__, cmd, chan_id);
			return -EINVAL;
		}

		memcpy(&ctx->isp_cfg[chan_id], cfg, sizeof(struct dvbm_isp_cfg_t));
		init_isp_infos(ctx);

		dvbm_adr = &ctx->dvbm_addr[chan_id];
		dvbm_adr->chan_id   = chan_id;
		dvbm_adr->ybuf_bot  = cfg->dma_addr + cfg->ybuf_bot;
		dvbm_adr->ybuf_top  = cfg->dma_addr + cfg->ybuf_top;
		dvbm_adr->ybuf_sadr = cfg->dma_addr + cfg->ybuf_bot;
		dvbm_adr->cbuf_bot  = cfg->dma_addr + cfg->cbuf_bot;
		dvbm_adr->cbuf_top  = cfg->dma_addr + cfg->cbuf_top;
		dvbm_adr->cbuf_sadr = cfg->dma_addr + cfg->cbuf_bot;
		dvbm2enc_callback(ctx, DVBM_ISP_SET_DVBM_CFG, cfg);
	} break;
	case DVBM_ISP_FRM_START: {
		dvbm2enc_callback(ctx, DVBM_VEPU_NOTIFY_FRM_STR, arg);
		rk_dvbm_update_isp_frm_info(ctx, 0);
		rk_dvbm_show_time(ctx);
	} break;
	case DVBM_ISP_FRM_END: {
		u32 line_cnt = ctx->isp_max_lcnt;

		dvbm2enc_callback(ctx, DVBM_VEPU_NOTIFY_FRM_END, arg);
		ctx->isp_frm_end = *(u32 *)arg;
		/* wrap frame_cnt 0 - 255 */
		ctx->isp_frm_info.frame_cnt = (ctx->isp_frm_start + 1) % 256;
		rk_dvbm_update_isp_frm_info(ctx, line_cnt);
		ctx->isp_frm_start++;
		dvbm_debug("isp frame end[%d : %d]\n", ctx->isp_frm_start, ctx->isp_frm_end);
	} break;
	case DVBM_ISP_FRM_QUARTER: {
		u32 line_cnt;

		line_cnt = ctx->isp_max_lcnt >> 2;
		rk_dvbm_update_isp_frm_info(ctx, line_cnt);
	} break;
	case DVBM_ISP_FRM_HALF: {
		u32 line_cnt;

		line_cnt = ctx->isp_max_lcnt >> 1;
		rk_dvbm_update_isp_frm_info(ctx, line_cnt);
	} break;
	case DVBM_ISP_FRM_THREE_QUARTERS: {
		u32 line_cnt;

		line_cnt = (ctx->isp_max_lcnt >> 2) * 3;
		rk_dvbm_update_isp_frm_info(ctx, line_cnt);
	} break;
	case DVBM_VEPU_GET_ADR: {
		struct dvbm_addr_cfg *dvbm_adr = (struct dvbm_addr_cfg *)arg;
		u32 chan_id = dvbm_adr->chan_id;

		if (chan_id >= DVBM_CHANNEL_NUM) {
			dvbm_err("%s cmd %d chan id %d is invalid\n", __func__, cmd, chan_id);
			return -EINVAL;
		}

		*dvbm_adr = ctx->dvbm_addr[chan_id];
	} break;
	default: {
	} break;
	}

	return 0;
}
EXPORT_SYMBOL(rk_dvbm_ctrl);

static int rk_dvbm_probe(struct platform_device *pdev)
{
	struct dvbm_ctx *ctx = NULL;
	struct device *dev = &pdev->dev;

	dev_info(dev, "probe start\n");
	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	ctx->port_isp.dir = DVBM_ISP_PORT;
	ctx->port_vepu.dir = DVBM_VEPU_PORT;

	platform_set_drvdata(pdev, ctx);

	g_ctx = ctx;

	dev_info(dev, "probe success\n");

	return 0;
}

static int rk_dvbm_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "remove device\n");

	return 0;
}

static const struct of_device_id rk_dvbm_dt_ids[] = {
	{
		.compatible = "rockchip,rk-dvbm",
	},
	{ },
};

static struct platform_driver rk_dvbm_driver = {
	.probe = rk_dvbm_probe,
	.remove = rk_dvbm_remove,
	.driver = {
		.name = "rk_dvbm",
		.of_match_table = of_match_ptr(rk_dvbm_dt_ids),
	},
};

static int __init rk_dvbm_init(void)
{
	return platform_driver_register(&rk_dvbm_driver);
}

static __exit void rk_dvbm_exit(void)
{
	platform_driver_unregister(&rk_dvbm_driver);
}

subsys_initcall(rk_dvbm_init);
module_exit(rk_dvbm_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Yandong Lin yandong.lin@rock-chips.com");
MODULE_DESCRIPTION("Rockchip dvbm driver");
