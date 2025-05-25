// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"

#include "stream_v10.h"
#include "stream_v20.h"

void rkvpss_cmsc_config(struct rkvpss_device *dev, bool sync)
{
	if (is_vpss_v10(dev->hw_dev))
		rkvpss_cmsc_config_v10(dev, sync);
	else if (is_vpss_v20(dev->hw_dev))
		rkvpss_cmsc_config_v20(dev, sync);
}

int rkvpss_stream_buf_cnt(struct rkvpss_stream *stream)
{
	struct rkvpss_device *vpss = stream->dev;
	int ret = 0;

	if (is_vpss_v10(vpss->hw_dev))
		ret = rkvpss_stream_buf_cnt_v10(stream);
	else if (is_vpss_v20(vpss->hw_dev))
		ret = rkvpss_stream_buf_cnt_v20(stream);

	return ret;
}

int rkvpss_register_stream_vdevs(struct rkvpss_device *dev)
{
	int ret = -EINVAL;

	if (is_vpss_v10(dev->hw_dev))
		ret = rkvpss_register_stream_vdevs_v10(dev);
	else if (is_vpss_v20(dev->hw_dev))
		ret = rkvpss_register_stream_vdevs_v20(dev);

	return ret;
}

void rkvpss_unregister_stream_vdevs(struct rkvpss_device *dev)
{
	if (is_vpss_v10(dev->hw_dev))
		rkvpss_unregister_stream_vdevs_v10(dev);
	else if (is_vpss_v20(dev->hw_dev))
		rkvpss_unregister_stream_vdevs_v20(dev);
}

void rkvpss_stream_default_fmt(struct rkvpss_device *dev, u32 id,
			       u32 width, u32 height, u32 pixelformat)
{
	if (is_vpss_v10(dev->hw_dev))
		rkvpss_stream_default_fmt_v10(dev, id, width, height, pixelformat);
	else if (is_vpss_v20(dev->hw_dev))
		rkvpss_stream_default_fmt_v20(dev, id, width, height, pixelformat);
}

void rkvpss_isr(struct rkvpss_device *dev, u32 mis_val)
{
	if (is_vpss_v10(dev->hw_dev))
		rkvpss_isr_v10(dev, mis_val);
	else if (is_vpss_v20(dev->hw_dev))
		rkvpss_isr_v20(dev, mis_val);
}

void rkvpss_mi_isr(struct rkvpss_device *dev, u32 mis_val)
{
	if (is_vpss_v10(dev->hw_dev))
		rkvpss_mi_isr_v10(dev, mis_val);
	else if (is_vpss_v20(dev->hw_dev))
		rkvpss_mi_isr_v20(dev, mis_val);
}
