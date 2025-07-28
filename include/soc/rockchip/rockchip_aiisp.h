/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */
#ifndef __SOC_ROCKCHIP_AIISP_H
#define __SOC_ROCKCHIP_AIISP_H

#include <linux/dma-buf.h>
#include <linux/rk-aiisp-config.h>

struct aiisp_aiynr_ybuf_cfg {
	int dev_id;
	int width;
	int height;
	u32 buf_cnt;
	struct dma_buf *buf[RKAIISP_AIYNR_YBUF_NUM_MAX];
};

#if IS_REACHABLE(CONFIG_VIDEO_ROCKCHIP_AIISP)
int rkaiisp_cfg_aiynr_yuvbuf(struct aiisp_aiynr_ybuf_cfg *buf_cfg);

#else

static inline int rkaiisp_cfg_aiynr_yuvbuf(struct aiisp_aiynr_ybuf_cfg *buf_cfg)
{
	return -EINVAL;
}

#endif
#endif
