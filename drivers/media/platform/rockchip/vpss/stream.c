// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs_v1.h"

#include "stream_v1.h"

void rkvpss_cmsc_config(struct rkvpss_device *dev, bool sync)
{
	if (dev->vpss_ver == VPSS_V10)
		rkvpss_cmsc_config_v1(dev, sync);
}

int rkvpss_stream_buf_cnt(struct rkvpss_stream *stream)
{
	struct rkvpss_device *vpss = stream->dev;
	int ret = 0;

	if (vpss->vpss_ver == VPSS_V10)
		ret = rkvpss_stream_buf_cnt_v1(stream);

	return ret;
}

int rkvpss_register_stream_vdevs(struct rkvpss_device *dev)
{
	int ret = -EINVAL;

	if (dev->vpss_ver == VPSS_V10)
		ret = rkvpss_register_stream_vdevs_v1(dev);

	return ret;
}

void rkvpss_unregister_stream_vdevs(struct rkvpss_device *dev)
{
	if (dev->vpss_ver == VPSS_V10)
		rkvpss_unregister_stream_vdevs_v1(dev);
}

void rkvpss_stream_default_fmt(struct rkvpss_device *dev, u32 id,
			       u32 width, u32 height, u32 pixelformat)
{
	if (dev->vpss_ver == VPSS_V10)
		rkvpss_stream_default_fmt_v1(dev, id, width, height, pixelformat);
}

void rkvpss_isr(struct rkvpss_device *dev, u32 mis_val)
{
	if (dev->vpss_ver == VPSS_V10)
		rkvpss_isr_v1(dev, mis_val);
}

void rkvpss_mi_isr(struct rkvpss_device *dev, u32 mis_val)
{
	if (dev->vpss_ver == VPSS_V10)
		rkvpss_mi_isr_v1(dev, mis_val);
}
