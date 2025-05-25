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


#define STREAM_OUT_REQ_BUFS_MIN 0

static void rkvpss_frame_end(struct rkvpss_stream *stream);
static int rkvpss_stream_crop(struct rkvpss_stream *stream, bool on, bool sync);
static int rkvpss_stream_scale(struct rkvpss_stream *stream, bool on, bool sync);
static void rkvpss_stream_mf(struct rkvpss_stream *stream);

static const struct capture_fmt scl0_fmts[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV16,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_GREY,
		.fmt_type = FMT_YUV,
		.bpp = { 8 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV400,
	}, {
		.fourcc = V4L2_PIX_FMT_UYVY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_422P,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV61,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV21,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_VYUY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_422P,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_TILE420,
		.fmt_type = FMT_YUV,
		.bpp = { 24 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = 0,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_TILE422,
		.fmt_type = FMT_YUV,
		.bpp = { 32 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = 0,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}
};

static const struct capture_fmt scl1_fmts[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV16,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_GREY,
		.fmt_type = FMT_YUV,
		.bpp = { 8 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV400,
	}, {
		.fourcc = V4L2_PIX_FMT_UYVY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_422P,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB565,
		.fmt_type = FMT_RGB,
		.bpp = { 16 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = RKVPSS_MI_CHN_WR_RB_SWAP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_RGB565,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB24,
		.fmt_type = FMT_RGB,
		.bpp = { 24 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = RKVPSS_MI_CHN_WR_RB_SWAP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_RGB888,
	}, {
		.fourcc = V4L2_PIX_FMT_XBGR32,
		.fmt_type = FMT_RGB,
		.bpp = { 32 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = 0,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_ARGB888,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB565X,
		.fmt_type = FMT_RGB,
		.bpp = { 16 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = 0,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_RGB565,
	}, {
		.fourcc = V4L2_PIX_FMT_BGR24,
		.fmt_type = FMT_RGB,
		.bpp = { 24 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = 0,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_RGB888,
	}, {
		.fourcc = V4L2_PIX_FMT_XRGB32,
		.fmt_type = FMT_RGB,
		.bpp = { 32 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = RKVPSS_MI_CHN_WR_RB_SWAP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_ARGB888,
	}, {
		.fourcc = V4L2_PIX_FMT_NV61,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV21,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_VYUY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_422P,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		   .fourcc = V4L2_PIX_FMT_TILE420,
		   .fmt_type = FMT_YUV,
		   .bpp = { 24 },
		   .cplanes = 1,
		   .mplanes = 1,
		   .swap = 0,
		   .wr_fmt = 0,
		   .output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		   .fourcc = V4L2_PIX_FMT_TILE422,
		   .fmt_type = FMT_YUV,
		   .bpp = { 32 },
		   .cplanes = 1,
		   .mplanes = 1,
		   .swap = 0,
		   .wr_fmt = 0,
		   .output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}
};

static const struct capture_fmt scl2_3_fmts[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV16,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_GREY,
		.fmt_type = FMT_YUV,
		.bpp = { 8 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV400,
	}, {
		.fourcc = V4L2_PIX_FMT_UYVY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_422P,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV61,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV21,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_VYUY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_422P,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}
};


static struct stream_config scl0_config = {
	.fmts = scl0_fmts,
	.fmt_size = ARRAY_SIZE(scl0_fmts),
	.frame_end_id = RKVPSS_MI_CHN0_FRM_END,
	.crop = {
		.ctrl = RKVPSS_CROP1_CTRL,
		.update = RKVPSS_CROP1_UPDATE,
		.h_offs = RKVPSS_CROP1_0_H_OFFS,
		.v_offs = RKVPSS_CROP1_0_V_OFFS,
		.h_size = RKVPSS_CROP1_0_H_SIZE,
		.v_size = RKVPSS_CROP1_0_V_SIZE,
		.ctrl_shd = RKVPSS_CROP1_CTRL_SHD,
		.h_offs_shd = RKVPSS_CROP1_0_H_OFFS_SHD,
		.v_offs_shd = RKVPSS_CROP1_0_V_OFFS_SHD,
		.h_size_shd = RKVPSS_CROP1_0_H_SIZE_SHD,
		.v_size_shd = RKVPSS_CROP1_0_V_SIZE_SHD,
	},
	.mi = {
		.ctrl = RKVPSS_MI_CHN0_WR_CTRL,
		.stride = RKVPSS_MI_CHN0_WR_Y_STRIDE,
		.y_base = RKVPSS_MI_CHN0_WR_Y_BASE,
		.uv_base = RKVPSS_MI_CHN0_WR_CB_BASE,
		.y_size = RKVPSS_MI_CHN0_WR_Y_SIZE,
		.uv_size = RKVPSS_MI_CHN0_WR_CB_SIZE,
		.y_offs_cnt = RKVPSS_MI_CHN0_WR_Y_OFFS_CNT,
		.uv_offs_cnt = RKVPSS_MI_CHN0_WR_CB_OFFS_CNT,
		.y_pic_width = RKVPSS_MI_CHN0_WR_Y_PIC_WIDTH,
		.y_pic_size = RKVPSS_MI_CHN0_WR_Y_PIC_SIZE,

		.ctrl_shd = RKVPSS_MI_CHN0_WR_CTRL_SHD,
		.y_shd = RKVPSS_MI_CHN0_WR_Y_BASE_SHD,
		.uv_shd = RKVPSS_MI_CHN0_WR_CB_BASE_SHD,
	},
};

static struct stream_config scl1_config = {
	.fmts = scl1_fmts,
	.fmt_size = ARRAY_SIZE(scl1_fmts),
	.frame_end_id = RKVPSS_MI_CHN1_FRM_END,
	.crop = {
		.ctrl = RKVPSS_CROP1_CTRL,
		.update = RKVPSS_CROP1_UPDATE,
		.h_offs = RKVPSS_CROP1_1_H_OFFS,
		.v_offs = RKVPSS_CROP1_1_V_OFFS,
		.h_size = RKVPSS_CROP1_1_H_SIZE,
		.v_size = RKVPSS_CROP1_1_V_SIZE,
		.ctrl_shd = RKVPSS_CROP1_CTRL_SHD,
		.h_offs_shd = RKVPSS_CROP1_1_H_OFFS_SHD,
		.v_offs_shd = RKVPSS_CROP1_1_V_OFFS_SHD,
		.h_size_shd = RKVPSS_CROP1_1_H_SIZE_SHD,
		.v_size_shd = RKVPSS_CROP1_1_V_SIZE_SHD,
	},
	.scale = {
		.ctrl = RKVPSS_SCALE1_CTRL,
		.update = RKVPSS_SCALE1_UPDATE,
		.src_size = RKVPSS_SCALE1_SRC_SIZE,
		.dst_size = RKVPSS_SCALE1_DST_SIZE,
		.hy_fac = RKVPSS_SCALE1_HY_FAC,
		.hc_fac = RKVPSS_SCALE1_HC_FAC,
		.vy_fac = RKVPSS_SCALE1_VY_FAC,
		.vc_fac = RKVPSS_SCALE1_VC_FAC,
		.hy_offs = RKVPSS_SCALE1_HY_OFFS,
		.hc_offs = RKVPSS_SCALE1_HC_OFFS,
		.vy_offs = RKVPSS_SCALE1_VY_OFFS,
		.vc_offs = RKVPSS_SCALE1_VC_OFFS,
		.hy_size = RKVPSS_SCALE1_HY_SIZE,
		.hc_size = RKVPSS_SCALE1_HC_SIZE,
		.hy_offs_mi = RKVPSS_SCALE1_HY_OFFS_MI,
		.hc_offs_mi = RKVPSS_SCALE1_HC_OFFS_MI,
		.in_crop_offs = RKVPSS_SCALE1_IN_CROP_OFFSET,
		.ctrl_shd = RKVPSS_SCALE1_CTRL_SHD,
		.src_size_shd = RKVPSS_SCALE1_SRC_SIZE_SHD,
		.dst_size_shd = RKVPSS_SCALE1_DST_SIZE_SHD,
		.hy_fac_shd = RKVPSS_SCALE1_HY_FAC_SHD,
		.hc_fac_shd = RKVPSS_SCALE1_HC_FAC_SHD,
		.vy_fac_shd = RKVPSS_SCALE1_VY_FAC_SHD,
		.vc_fac_shd = RKVPSS_SCALE1_VC_FAC_SHD,
		.hy_offs_shd = RKVPSS_SCALE1_HY_OFFS_SHD,
		.hc_offs_shd = RKVPSS_SCALE1_HC_OFFS_SHD,
		.vy_offs_shd = RKVPSS_SCALE1_VY_OFFS_SHD,
		.vc_offs_shd = RKVPSS_SCALE1_VC_OFFS_SHD,
		.hy_size_shd = RKVPSS_SCALE1_HY_SIZE_SHD,
		.hc_size_shd = RKVPSS_SCALE1_HC_SIZE_SHD,
		.hy_offs_mi_shd = RKVPSS_SCALE1_HY_OFFS_MI_SHD,
		.hc_offs_mi_shd = RKVPSS_SCALE1_HC_OFFS_MI_SHD,
		.in_crop_offs_shd = RKVPSS_SCALE1_IN_CROP_OFFSET_SHD,
	},
	.mi = {
		.ctrl = RKVPSS_MI_CHN1_WR_CTRL,
		.stride = RKVPSS_MI_CHN1_WR_Y_STRIDE,
		.y_base = RKVPSS_MI_CHN1_WR_Y_BASE,
		.uv_base = RKVPSS_MI_CHN1_WR_CB_BASE,
		.y_size = RKVPSS_MI_CHN1_WR_Y_SIZE,
		.uv_size = RKVPSS_MI_CHN1_WR_CB_SIZE,
		.y_offs_cnt = RKVPSS_MI_CHN1_WR_Y_OFFS_CNT,
		.uv_offs_cnt = RKVPSS_MI_CHN1_WR_CB_OFFS_CNT,
		.y_pic_width = RKVPSS_MI_CHN1_WR_Y_PIC_WIDTH,
		.y_pic_size = RKVPSS_MI_CHN1_WR_Y_PIC_SIZE,

		.ctrl_shd = RKVPSS_MI_CHN1_WR_CTRL_SHD,
		.y_shd = RKVPSS_MI_CHN1_WR_Y_BASE_SHD,
		.uv_shd = RKVPSS_MI_CHN1_WR_CB_BASE_SHD,
	},
};

static struct stream_config scl2_config = {
	.fmts = scl2_3_fmts,
	.fmt_size = ARRAY_SIZE(scl2_3_fmts),
	.frame_end_id = RKVPSS_MI_CHN2_FRM_END,
	.crop = {
		.ctrl = RKVPSS_CROP1_CTRL,
		.update = RKVPSS_CROP1_UPDATE,
		.h_offs = RKVPSS_CROP1_2_H_OFFS,
		.v_offs = RKVPSS_CROP1_2_V_OFFS,
		.h_size = RKVPSS_CROP1_2_H_SIZE,
		.v_size = RKVPSS_CROP1_2_V_SIZE,
		.ctrl_shd = RKVPSS_CROP1_CTRL_SHD,
		.h_offs_shd = RKVPSS_CROP1_2_H_OFFS_SHD,
		.v_offs_shd = RKVPSS_CROP1_2_V_OFFS_SHD,
		.h_size_shd = RKVPSS_CROP1_2_H_SIZE_SHD,
		.v_size_shd = RKVPSS_CROP1_2_V_SIZE_SHD,
	},
	.scale = {
		.ctrl = RKVPSS_SCALE2_CTRL,
		.update = RKVPSS_SCALE2_UPDATE,
		.src_size = RKVPSS_SCALE2_SRC_SIZE,
		.dst_size = RKVPSS_SCALE2_DST_SIZE,
		.hy_fac = RKVPSS_SCALE2_HY_FAC,
		.hc_fac = RKVPSS_SCALE2_HC_FAC,
		.vy_fac = RKVPSS_SCALE2_VY_FAC,
		.vc_fac = RKVPSS_SCALE2_VC_FAC,
		.hy_offs = RKVPSS_SCALE2_HY_OFFS,
		.hc_offs = RKVPSS_SCALE2_HC_OFFS,
		.vy_offs = RKVPSS_SCALE2_VY_OFFS,
		.vc_offs = RKVPSS_SCALE2_VC_OFFS,
		.hy_size = RKVPSS_SCALE2_HY_SIZE,
		.hc_size = RKVPSS_SCALE2_HC_SIZE,
		.hy_offs_mi = RKVPSS_SCALE2_HY_OFFS_MI,
		.hc_offs_mi = RKVPSS_SCALE2_HC_OFFS_MI,
		.in_crop_offs = RKVPSS_SCALE2_IN_CROP_OFFSET,
		.ctrl_shd = RKVPSS_SCALE2_CTRL_SHD,
		.src_size_shd = RKVPSS_SCALE2_SRC_SIZE_SHD,
		.dst_size_shd = RKVPSS_SCALE2_DST_SIZE_SHD,
		.hy_fac_shd = RKVPSS_SCALE2_HY_FAC_SHD,
		.hc_fac_shd = RKVPSS_SCALE2_HC_FAC_SHD,
		.vy_fac_shd = RKVPSS_SCALE2_VY_FAC_SHD,
		.vc_fac_shd = RKVPSS_SCALE2_VC_FAC_SHD,
		.hy_offs_shd = RKVPSS_SCALE2_HY_OFFS_SHD,
		.hc_offs_shd = RKVPSS_SCALE2_HC_OFFS_SHD,
		.vy_offs_shd = RKVPSS_SCALE2_VY_OFFS_SHD,
		.vc_offs_shd = RKVPSS_SCALE2_VC_OFFS_SHD,
		.hy_size_shd = RKVPSS_SCALE2_HY_SIZE_SHD,
		.hc_size_shd = RKVPSS_SCALE2_HC_SIZE_SHD,
		.hy_offs_mi_shd = RKVPSS_SCALE2_HY_OFFS_MI_SHD,
		.hc_offs_mi_shd = RKVPSS_SCALE2_HC_OFFS_MI_SHD,
		.in_crop_offs_shd = RKVPSS_SCALE2_IN_CROP_OFFSET_SHD,
	},
	.mi = {
		.ctrl = RKVPSS_MI_CHN2_WR_CTRL,
		.stride = RKVPSS_MI_CHN2_WR_Y_STRIDE,
		.y_base = RKVPSS_MI_CHN2_WR_Y_BASE,
		.uv_base = RKVPSS_MI_CHN2_WR_CB_BASE,
		.y_size = RKVPSS_MI_CHN2_WR_Y_SIZE,
		.uv_size = RKVPSS_MI_CHN2_WR_CB_SIZE,
		.y_offs_cnt = RKVPSS_MI_CHN2_WR_Y_OFFS_CNT,
		.uv_offs_cnt = RKVPSS_MI_CHN2_WR_CB_OFFS_CNT,
		.y_pic_width = RKVPSS_MI_CHN2_WR_Y_PIC_WIDTH,
		.y_pic_size = RKVPSS_MI_CHN2_WR_Y_PIC_SIZE,

		.ctrl_shd = RKVPSS_MI_CHN2_WR_CTRL_SHD,
		.y_shd = RKVPSS_MI_CHN2_WR_Y_BASE_SHD,
		.uv_shd = RKVPSS_MI_CHN2_WR_CB_BASE_SHD,
	},
};

static struct stream_config scl3_config = {
	.fmts = scl2_3_fmts,
	.fmt_size = ARRAY_SIZE(scl2_3_fmts),
	.frame_end_id = RKVPSS_MI_CHN3_FRM_END,
	.crop = {
		.ctrl = RKVPSS_CROP1_CTRL,
		.update = RKVPSS_CROP1_UPDATE,
		.h_offs = RKVPSS_CROP1_3_H_OFFS,
		.v_offs = RKVPSS_CROP1_3_V_OFFS,
		.h_size = RKVPSS_CROP1_3_H_SIZE,
		.v_size = RKVPSS_CROP1_3_V_SIZE,
		.ctrl_shd = RKVPSS_CROP1_CTRL_SHD,
		.h_offs_shd = RKVPSS_CROP1_3_H_OFFS_SHD,
		.v_offs_shd = RKVPSS_CROP1_3_V_OFFS_SHD,
		.h_size_shd = RKVPSS_CROP1_3_H_SIZE_SHD,
		.v_size_shd = RKVPSS_CROP1_3_V_SIZE_SHD,
	},
	.scale = {
		.ctrl = RKVPSS_SCALE3_CTRL,
		.update = RKVPSS_SCALE3_UPDATE,
		.src_size = RKVPSS_SCALE3_SRC_SIZE,
		.dst_size = RKVPSS_SCALE3_DST_SIZE,
		.hy_fac = RKVPSS_SCALE3_HY_FAC,
		.hc_fac = RKVPSS_SCALE3_HC_FAC,
		.vy_fac = RKVPSS_SCALE3_VY_FAC,
		.vc_fac = RKVPSS_SCALE3_VC_FAC,
		.hy_offs = RKVPSS_SCALE3_HY_OFFS,
		.hc_offs = RKVPSS_SCALE3_HC_OFFS,
		.vy_offs = RKVPSS_SCALE3_VY_OFFS,
		.vc_offs = RKVPSS_SCALE3_VC_OFFS,
		.hy_size = RKVPSS_SCALE3_HY_SIZE,
		.hc_size = RKVPSS_SCALE3_HC_SIZE,
		.hy_offs_mi = RKVPSS_SCALE3_HY_OFFS_MI,
		.hc_offs_mi = RKVPSS_SCALE3_HC_OFFS_MI,
		.in_crop_offs = RKVPSS_SCALE3_IN_CROP_OFFSET,
		.ctrl_shd = RKVPSS_SCALE3_CTRL_SHD,
		.src_size_shd = RKVPSS_SCALE3_SRC_SIZE_SHD,
		.dst_size_shd = RKVPSS_SCALE3_DST_SIZE_SHD,
		.hy_fac_shd = RKVPSS_SCALE3_HY_FAC_SHD,
		.hc_fac_shd = RKVPSS_SCALE3_HC_FAC_SHD,
		.vy_fac_shd = RKVPSS_SCALE3_VY_FAC_SHD,
		.vc_fac_shd = RKVPSS_SCALE3_VC_FAC_SHD,
		.hy_offs_shd = RKVPSS_SCALE3_HY_OFFS_SHD,
		.hc_offs_shd = RKVPSS_SCALE3_HC_OFFS_SHD,
		.vy_offs_shd = RKVPSS_SCALE3_VY_OFFS_SHD,
		.vc_offs_shd = RKVPSS_SCALE3_VC_OFFS_SHD,
		.hy_size_shd = RKVPSS_SCALE3_HY_SIZE_SHD,
		.hc_size_shd = RKVPSS_SCALE3_HC_SIZE_SHD,
		.hy_offs_mi_shd = RKVPSS_SCALE3_HY_OFFS_MI_SHD,
		.hc_offs_mi_shd = RKVPSS_SCALE3_HC_OFFS_MI_SHD,
		.in_crop_offs_shd = RKVPSS_SCALE3_IN_CROP_OFFSET_SHD,
	},
	.mi = {
		.ctrl = RKVPSS_MI_CHN3_WR_CTRL,
		.stride = RKVPSS_MI_CHN3_WR_Y_STRIDE,
		.y_base = RKVPSS_MI_CHN3_WR_Y_BASE,
		.uv_base = RKVPSS_MI_CHN3_WR_CB_BASE,
		.y_size = RKVPSS_MI_CHN3_WR_Y_SIZE,
		.uv_size = RKVPSS_MI_CHN3_WR_CB_SIZE,
		.y_offs_cnt = RKVPSS_MI_CHN3_WR_Y_OFFS_CNT,
		.uv_offs_cnt = RKVPSS_MI_CHN3_WR_CB_OFFS_CNT,
		.y_pic_width = RKVPSS_MI_CHN3_WR_Y_PIC_WIDTH,
		.y_pic_size = RKVPSS_MI_CHN3_WR_Y_PIC_SIZE,

		.ctrl_shd = RKVPSS_MI_CHN3_WR_CTRL_SHD,
		.y_shd = RKVPSS_MI_CHN3_WR_Y_BASE_SHD,
		.uv_shd = RKVPSS_MI_CHN3_WR_CB_BASE_SHD,
	},
};

static void calc_unite_scl_params(struct rkvpss_stream *stream)
{
	struct rkvpss_online_unite_params *params = &stream->unite_params;
	u32 right_scl_need_size_y, right_scl_need_size_c;
	u32 left_in_used_size_y, left_in_used_size_c;
	u32 right_fst_position_y, right_fst_position_c;
	u32 right_y_crop_total;
	u32 right_c_crop_total;

	params->y_w_fac = (stream->crop.width - 1) * 4096 /
			  (stream->out_fmt.width  - 1);
	params->c_w_fac = (stream->crop.width / 2 - 1) * 4096 /
			  (stream->out_fmt.width / 2 - 1);
	params->y_h_fac = (stream->crop.height - 1) * 4096 /
			  (stream->out_fmt.height - 1);
	params->c_h_fac = (stream->crop.height - 1) * 4096 /
			  (stream->out_fmt.height - 1);

	right_fst_position_y = stream->out_fmt.width / 2 *
			       params->y_w_fac;
	right_fst_position_c = stream->out_fmt.width / 2 / 2 *
			       params->c_w_fac;

	left_in_used_size_y = right_fst_position_y >> 12;
	left_in_used_size_c = (right_fst_position_c >> 12) * 2;

	params->y_w_phase = right_fst_position_y & 0xfff;
	params->c_w_phase = right_fst_position_c & 0xfff;

	right_scl_need_size_y = stream->crop.width -
				left_in_used_size_y;
	params->right_scl_need_size_y = right_scl_need_size_y;
	right_scl_need_size_c = stream->crop.width -
				left_in_used_size_c;
	params->right_scl_need_size_c = right_scl_need_size_c;

	if (stream->id == 0 && stream->crop.width != stream->out_fmt.width) {
		right_y_crop_total = stream->crop.width / 2 +
				     RKMOUDLE_UNITE_EXTEND_PIXEL -
				     right_scl_need_size_y - 3;
		right_c_crop_total = stream->crop.width / 2 +
				     RKMOUDLE_UNITE_EXTEND_PIXEL -
				     right_scl_need_size_c - 6;
	} else {
		right_y_crop_total = stream->crop.width / 2 +
				     RKMOUDLE_UNITE_EXTEND_PIXEL -
				     right_scl_need_size_y;
		right_c_crop_total = stream->crop.width / 2 +
				     RKMOUDLE_UNITE_EXTEND_PIXEL -
				     right_scl_need_size_c;
	}

	params->quad_crop_w = ALIGN_DOWN(min(right_y_crop_total, right_c_crop_total), 2);

	params->scl_in_crop_w_y = right_y_crop_total - params->quad_crop_w;
	params->scl_in_crop_w_c = right_c_crop_total - params->quad_crop_w;

	if (rkvpss_debug >= 2) {
		v4l2_info(&stream->dev->v4l2_dev,
			  "%s ch:%d y_w_fac:%u c_w_fac:%u y_h_fac:%u c_h_fac:%u\n",
			  __func__, stream->id, params->y_w_fac,
			  params->c_w_fac, params->y_h_fac, params->c_h_fac);
		v4l2_info(&stream->dev->v4l2_dev,
			  "\t%s y_w_phase:%u c_w_phase:%u quad_crop_w:%u scl_in_crop_w_y:%u scl_in_crop_w_c:%u\n",
			  __func__, params->y_w_phase, params->c_w_phase,
			  params->quad_crop_w,
			  params->scl_in_crop_w_y, params->scl_in_crop_w_c);
		v4l2_info(&stream->dev->v4l2_dev,
			  "\t%s right_scl_need_size_y:%u right_scl_need_size_c:%u\n",
			  __func__, params->right_scl_need_size_y,
			  params->right_scl_need_size_c);
	}
}

int rkvpss_stream_buf_cnt_v10(struct rkvpss_stream *stream)
{
	unsigned long lock_flags = 0;
	struct rkvpss_buffer *buf, *tmp;
	int cnt = 0;

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	list_for_each_entry_safe(buf, tmp, &stream->buf_queue, queue)
		cnt++;
	if (stream->curr_buf)
		cnt++;
	if (stream->next_buf && stream->next_buf != stream->curr_buf)
		cnt++;
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	return cnt;
}

static void stream_frame_start(struct rkvpss_stream *stream, u32 irq)
{
	struct rkvpss_device *dev = stream->dev;

	if (stream->is_crop_upd) {
		if (dev->unite_mode)
			calc_unite_scl_params(stream);
		rkvpss_stream_scale(stream, true, !irq);
		rkvpss_stream_crop(stream, true, !irq);
	}
	if (!irq && !stream->curr_buf &&
	    !stream->dev->hw_dev->is_single)
		stream->ops->update_mi(stream);
}

static void scl_force_update(struct rkvpss_stream *stream)
{
	u32 val;

	switch (stream->id) {
	case RKVPSS_OUTPUT_CH0:
		val = RKVPSS_MI_CHN0_FORCE_UPD;
		break;
	case RKVPSS_OUTPUT_CH1:
		val = RKVPSS_MI_CHN1_FORCE_UPD;
		break;
	case RKVPSS_OUTPUT_CH2:
		val = RKVPSS_MI_CHN2_FORCE_UPD;
		break;
	case RKVPSS_OUTPUT_CH3:
		val = RKVPSS_MI_CHN3_FORCE_UPD;
		break;
	default:
		return;
	}
	/* need update two for online2 mode */
	rkvpss_unite_write(stream->dev, RKVPSS_MI_WR_INIT, val);
	rkvpss_unite_write(stream->dev, RKVPSS_MI_WR_INIT, val);
}

static void scl_update_mi(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_buffer *buf = NULL;
	unsigned long lock_flags = 0;
	u32 y_base, uv_base;

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (!list_empty(&stream->buf_queue) && !stream->curr_buf) {
		buf = list_first_entry(&stream->buf_queue, struct rkvpss_buffer, queue);
		list_del(&buf->queue);
		stream->curr_buf = buf;
	}
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	if (buf) {
		y_base = buf->dma[0];
		uv_base = buf->dma[1];
		rkvpss_idx_write(dev, stream->config->mi.y_base, y_base, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, stream->config->mi.uv_base, uv_base, VPSS_UNITE_LEFT);
		if (dev->unite_mode) {
			y_base = buf->dma[0] + stream->out_fmt.width / 2;
			uv_base = buf->dma[1] + stream->out_fmt.width / 2;
			rkvpss_idx_write(dev, stream->config->mi.y_base, y_base,
					 VPSS_UNITE_RIGHT);
			rkvpss_idx_write(dev, stream->config->mi.uv_base, uv_base,
					 VPSS_UNITE_RIGHT);
		}

		scl_force_update(stream);
		if (stream->is_pause) {
			stream->is_pause = false;
			stream->ops->enable_mi(stream);
		}
	} else if (!stream->is_pause) {
		stream->is_pause = true;
		stream->ops->disable_mi(stream);
	}

	v4l2_dbg(2, rkvpss_debug, &dev->v4l2_dev,
		 "%s id:%d unite_index:%d Y:0x%x UV:0x%x | Y_SHD:0x%x\n",
		__func__, stream->id, dev->unite_index,
		rkvpss_idx_read(dev, stream->config->mi.y_base, dev->unite_index),
		rkvpss_idx_read(dev, stream->config->mi.uv_base, dev->unite_index),
		rkvpss_hw_read(dev->hw_dev, stream->config->mi.y_shd));
}

static void scl_config_mi(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct capture_fmt *fmt = &stream->out_cap_fmt;
	struct v4l2_pix_format_mplane *out_fmt = &stream->out_fmt;
	u32 reg, val, mask;

	v4l2_dbg(4, rkvpss_debug, &dev->v4l2_dev,
		 "%s stream:%d\n", __func__, stream->id);

	val = out_fmt->plane_fmt[0].bytesperline;
	reg = stream->config->mi.stride;
	rkvpss_unite_write(dev, reg, val);
	rkvpss_unite_write(dev, reg, val);

	switch (fmt->fourcc) {
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
		val = out_fmt->plane_fmt[0].bytesperline / 2;
		break;
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_XRGB32:
		val = out_fmt->plane_fmt[0].bytesperline / 4;
		break;
	default:
		val = out_fmt->plane_fmt[0].bytesperline;
		break;
	}

	val = val * out_fmt->height;
	reg = stream->config->mi.y_pic_size;
	rkvpss_unite_write(dev, reg, val);

	val = out_fmt->plane_fmt[0].bytesperline * out_fmt->height;
	reg = stream->config->mi.y_size;
	rkvpss_unite_write(dev, reg, val);

	val = out_fmt->plane_fmt[1].sizeimage;
	reg = stream->config->mi.uv_size;
	rkvpss_unite_write(dev, reg, val);

	reg = stream->config->mi.y_offs_cnt;
	rkvpss_unite_write(dev, reg, 0);
	reg = stream->config->mi.uv_offs_cnt;
	rkvpss_unite_write(dev, reg, 0);

	val = fmt->wr_fmt | fmt->output_fmt | fmt->swap |
	      RKVPSS_MI_CHN_WR_EN | RKVPSS_MI_CHN_WR_AUTO_UPD;
	reg = stream->config->mi.ctrl;
	rkvpss_unite_write(dev, reg, val);

	switch (fmt->fourcc) {
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_VYUY:
		mask = RKVPSS_MI_WR_UV_SWAP;
		val = RKVPSS_MI_WR_UV_SWAP;
		rkvpss_hw_set_bits(dev->hw_dev, RKVPSS_MI_WR_CTRL, mask, val);
		break;
	case V4L2_PIX_FMT_TILE420:
	case V4L2_PIX_FMT_TILE422:
		mask = RKVPSS_MI_WR_TILE_SEL(3);
		val = RKVPSS_MI_WR_TILE_SEL(stream->id + 1);
		rkvpss_hw_set_bits(dev->hw_dev, RKVPSS_MI_WR_CTRL, mask, val);
		break;
	default:
		break;
	}

	stream->is_mf_upd = true;
	rkvpss_frame_end(stream);
}

static void scl_enable_mi(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	u32 val, mask;

	switch (stream->id) {
	case RKVPSS_OUTPUT_CH0:
		val = RKVPSS_ISP2VPSS_CHN0_SEL(2);
		mask = RKVPSS_ISP2VPSS_CHN0_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH1:
		val = RKVPSS_ISP2VPSS_CHN1_SEL(2);
		mask = RKVPSS_ISP2VPSS_CHN1_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH2:
		val = RKVPSS_ISP2VPSS_CHN2_SEL(2);
		mask = RKVPSS_ISP2VPSS_CHN2_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH3:
		val = RKVPSS_ISP2VPSS_CHN3_SEL(2);
		mask = RKVPSS_ISP2VPSS_CHN3_SEL(3);
		break;
	default:
		return;
	}
	val |= RKVPSS_ISP2VPSS_ONLINE2;
	if (!dev->hw_dev->is_ofl_cmsc)
		val |= RKVPSS_ISP2VPSS_ONLINE2_CMSC_EN;
	rkvpss_unite_set_bits(dev, RKVPSS_VPSS_ONLINE, mask, val);
	val = RKVPSS_ONLINE2_CHN_FORCE_UPD | RKVPSS_CFG_GEN_UPD;
	rkvpss_unite_write(dev, RKVPSS_VPSS_UPDATE, val);
	v4l2_dbg(2, rkvpss_debug, &dev->v4l2_dev,
		 "%s id:%d\n", __func__,
		 stream->id);
}

static void scl_disable_mi(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	u32 val;

	switch (stream->id) {
	case RKVPSS_OUTPUT_CH0:
		val = RKVPSS_ISP2VPSS_CHN0_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH1:
		val = RKVPSS_ISP2VPSS_CHN1_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH2:
		val = RKVPSS_ISP2VPSS_CHN2_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH3:
		val = RKVPSS_ISP2VPSS_CHN3_SEL(3);
		break;
	default:
		return;
	}

	rkvpss_unite_clear_bits(dev, RKVPSS_VPSS_ONLINE, val);
	val = RKVPSS_ONLINE2_CHN_FORCE_UPD | RKVPSS_CFG_GEN_UPD;
	rkvpss_unite_write(dev, RKVPSS_VPSS_UPDATE, val);
	v4l2_dbg(2, rkvpss_debug, &dev->v4l2_dev,
		 "%s id:%d\n", __func__,
		 stream->id);
}

static struct streams_ops scl_stream_ops = {
	.config_mi = scl_config_mi,
	.enable_mi = scl_enable_mi,
	.disable_mi = scl_disable_mi,
	.update_mi = scl_update_mi,
	.frame_start = stream_frame_start,
};

static void rkvpss_buf_done_task(unsigned long arg)
{
	struct rkvpss_stream *stream = (struct rkvpss_stream *)arg;
	struct rkvpss_vdev_node *node = &stream->vnode;
	struct rkvpss_buffer *buf = NULL;
	unsigned long lock_flags = 0;
	LIST_HEAD(local_list);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	list_replace_init(&stream->buf_done_list, &local_list);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	while (!list_empty(&local_list)) {
		buf = list_first_entry(&local_list, struct rkvpss_buffer, queue);
		list_del(&buf->queue);
		v4l2_dbg(2, rkvpss_debug, &stream->dev->v4l2_dev,
			 "%s stream:%d index:%d seq:%d buf:0x%x done\n",
			 node->vdev.name, stream->id, buf->vb.vb2_buf.index,
			 buf->vb.sequence, buf->dma[0]);
		vb2_buffer_done(&buf->vb.vb2_buf,
				stream->streaming ? VB2_BUF_STATE_DONE : VB2_BUF_STATE_ERROR);
	}
}

static void rkvpss_stream_buf_done(struct rkvpss_stream *stream,
				   struct rkvpss_buffer *buf)
{
	unsigned long lock_flags = 0;

	if (!stream || !buf)
		return;

	v4l2_dbg(3, rkvpss_debug, &stream->dev->v4l2_dev,
		 "stream:%d\n", stream->id);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	list_add_tail(&buf->queue, &stream->buf_done_list);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	tasklet_schedule(&stream->buf_done_tasklet);
}

static void rkvpss_fill_frame_info(struct rkvpss_frame_info *dst_info,
				   struct rkisp_vpss_frame_info *src_info)
{
	dst_info->timestamp = src_info->timestamp;

	dst_info->seq = src_info->seq;
	dst_info->hdr = src_info->hdr;
	dst_info->rolling_shutter_skew = src_info->rolling_shutter_skew;

	dst_info->sensor_exposure_time = src_info->sensor_exposure_time;
	dst_info->sensor_analog_gain = src_info->sensor_analog_gain;
	dst_info->sensor_digital_gain = src_info->sensor_digital_gain;
	dst_info->isp_digital_gain = src_info->isp_digital_gain;

	dst_info->sensor_exposure_time_m = src_info->sensor_exposure_time_m;
	dst_info->sensor_analog_gain_m = src_info->sensor_analog_gain_m;
	dst_info->sensor_digital_gain_m = src_info->sensor_digital_gain_m;
	dst_info->isp_digital_gain_m = src_info->isp_digital_gain_m;

	dst_info->sensor_exposure_time_l = src_info->sensor_exposure_time_l;
	dst_info->sensor_analog_gain_l = src_info->sensor_analog_gain_l;
	dst_info->sensor_digital_gain_l = src_info->sensor_digital_gain_l;
	dst_info->isp_digital_gain_l = src_info->isp_digital_gain_l;
}

static void rkvpss_frame_end(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_subdev *sdev = &dev->vpss_sdev;
	struct rkvpss_buffer *buf = NULL;
	unsigned long lock_flags = 0;

	v4l2_dbg(3, rkvpss_debug, &dev->v4l2_dev,
		 "%s stream:%d\n", __func__, stream->id);
	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (stream->curr_buf) {
		buf = stream->curr_buf;
		stream->curr_buf = NULL;
	}
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	if (buf) {
		struct vb2_buffer *vb2_buf = &buf->vb.vb2_buf;
		struct capture_fmt *fmt = &stream->out_cap_fmt;
		u64 ns = sdev->frame_timestamp;
		int i;

		for (i = 0; i < fmt->mplanes; i++) {
			u32 payload_size = stream->out_fmt.plane_fmt[i].sizeimage;

			vb2_set_plane_payload(vb2_buf, i, payload_size);

			if (stream->is_attach_info && i == fmt->mplanes - 1) {
				struct rkvpss_frame_info *dst_info = buf->vaddr[i] + payload_size;
				struct rkisp_vpss_frame_info *src_info = &dev->frame_info;

				rkvpss_fill_frame_info(dst_info, src_info);
			}
		}
		if (!ns)
			ns = ktime_get_ns();
		buf->vb.vb2_buf.timestamp = ns;
		buf->vb.sequence = sdev->frame_seq;

		ns = ktime_get_ns();
		stream->dbg.frameloss += buf->vb.sequence - stream->dbg.id - 1;
		stream->dbg.id = buf->vb.sequence;
		stream->dbg.delay = ns - buf->vb.vb2_buf.timestamp;
		stream->dbg.interval = ns - stream->dbg.timestamp;
		stream->dbg.timestamp = ns;
		rkvpss_stream_buf_done(stream, buf);
	}

	rkvpss_stream_mf(stream);
	stream->ops->update_mi(stream);
}

static int rkvpss_queue_setup(struct vb2_queue *queue,
			      unsigned int *num_buffers,
			      unsigned int *num_planes,
			      unsigned int sizes[],
			      struct device *alloc_ctxs[])
{
	struct rkvpss_stream *stream = queue->drv_priv;
	struct rkvpss_device *dev = stream->dev;
	const struct v4l2_pix_format_mplane *pixm = NULL;
	const struct capture_fmt *cap_fmt = NULL;
	u32 i;

	pixm = &stream->out_fmt;
	if (!pixm->width || !pixm->height)
		return -EINVAL;
	cap_fmt = &stream->out_cap_fmt;
	*num_planes = cap_fmt->mplanes;

	for (i = 0; i < cap_fmt->mplanes; i++) {
		const struct v4l2_plane_pix_format *plane_fmt;

		plane_fmt = &pixm->plane_fmt[i];
		sizes[i] = plane_fmt->sizeimage / pixm->height * ALIGN(pixm->height, 16);

		if (stream->is_attach_info && i == cap_fmt->mplanes - 1)
			sizes[i] += sizeof(struct rkvpss_frame_info);
	}

	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s stream:%d count %d, size %d\n",
		 v4l2_type_names[queue->type],
		 stream->id, *num_buffers, sizes[0]);

	return 0;
}

static void rkvpss_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkvpss_buffer *vpssbuf = to_rkvpss_buffer(vbuf);
	struct vb2_queue *queue = vb->vb2_queue;
	struct rkvpss_stream *stream = queue->drv_priv;
	struct rkvpss_device *dev = stream->dev;
	struct v4l2_pix_format_mplane *pixm = &stream->out_fmt;
	struct capture_fmt *cap_fmt = &stream->out_cap_fmt;
	unsigned long lock_flags = 0;
	struct sg_table *sgt;
	u32 height, size;
	int i;

	for (i = 0; i < cap_fmt->mplanes; i++) {
		sgt = vb2_dma_sg_plane_desc(vb, i);
		vpssbuf->dma[i] = sg_dma_address(sgt->sgl);

		vpssbuf->vaddr[i] = vb2_plane_vaddr(vb, i);
	}
	/*
	 * NOTE: plane_fmt[0].sizeimage is total size of all planes for single
	 * memory plane formats, so calculate the size explicitly.
	 */
	if (cap_fmt->mplanes == 1) {
		for (i = 0; i < cap_fmt->cplanes - 1; i++) {
			height = pixm->height;
			size = (i == 0) ?
				pixm->plane_fmt[i].bytesperline * height :
				pixm->plane_fmt[i].sizeimage;
			vpssbuf->dma[i + 1] = vpssbuf->dma[i] + size;
		}
	}

	v4l2_dbg(2, rkvpss_debug, &dev->v4l2_dev,
		 "%s stream:%d index:%d buf:0x%x\n", __func__,
		 stream->id, vb->index, vpssbuf->dma[0]);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	list_add_tail(&vpssbuf->queue, &stream->buf_queue);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	if (dev->hw_dev->is_single &&
	    stream->streaming && !stream->curr_buf)
		stream->ops->update_mi(stream);
}

static void destroy_buf_queue(struct rkvpss_stream *stream,
			      enum vb2_buffer_state state)
{
	unsigned long lock_flags = 0;
	struct rkvpss_buffer *buf;
	LIST_HEAD(queue_local_list);
	LIST_HEAD(done_local_list);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (stream->curr_buf) {
		list_add_tail(&stream->curr_buf->queue, &stream->buf_queue);
		stream->curr_buf = NULL;
	}
	list_replace_init(&stream->buf_queue, &queue_local_list);
	list_replace_init(&stream->buf_done_list, &done_local_list);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	while (!list_empty(&queue_local_list)) {
		buf = list_first_entry(&queue_local_list, struct rkvpss_buffer, queue);
		list_del(&buf->queue);
		buf->vb.vb2_buf.synced = false;
		vb2_buffer_done(&buf->vb.vb2_buf, state);
	}

	while (!list_empty(&done_local_list)) {
		buf = list_first_entry(&done_local_list, struct rkvpss_buffer, queue);
		list_del(&buf->queue);
		buf->vb.vb2_buf.synced = false;
		vb2_buffer_done(&buf->vb.vb2_buf, state);
	}
}

static int rkvpss_stream_crop(struct rkvpss_stream *stream, bool on, bool sync)
{
	struct rkvpss_device *dev = stream->dev;
	struct v4l2_rect *crop = &stream->crop;
	u32 reg_ctrl = stream->config->crop.ctrl;
	u32 reg_upd = stream->config->crop.update;
	u32 reg_h_offs = stream->config->crop.h_offs;
	u32 reg_v_offs = stream->config->crop.v_offs;
	u32 reg_h_size = stream->config->crop.h_size;
	u32 reg_v_size = stream->config->crop.v_size;
	u32 val;

	val = RKVPSS_CROP_CHN_EN(stream->id);
	if (!dev->unite_mode) {
		if (on) {
			rkvpss_unite_set_bits(dev, reg_ctrl, 0, val);
			rkvpss_unite_write(dev, reg_h_offs, crop->left);
			rkvpss_unite_write(dev, reg_v_offs, crop->top);
			rkvpss_unite_write(dev, reg_h_size, crop->width);
			rkvpss_unite_write(dev, reg_v_size, crop->height);

			v4l2_dbg(4, rkvpss_debug, &dev->v4l2_dev,
				 "crop left:%d top:%d w:%d h:%d\n",
				 crop->left, crop->top, crop->width, crop->height);
		} else {
			rkvpss_unite_clear_bits(dev, reg_ctrl, val);
		}
		if (sync) {
			val = RKVPSS_CROP_CHN_FORCE_UPD(stream->id);
			val |= RKVPSS_CROP_GEN_UPD;
			rkvpss_unite_write(dev, reg_upd, val);
			rkvpss_unite_write(dev, reg_upd, val);
		}
	} else {
		if (on) {
			if (crop->left + crop->width / 2 != dev->vpss_sdev.in_fmt.width / 2) {
				v4l2_err(&dev->v4l2_dev,
					 "unite need centered crop left:%d top:%d w:%d h:%d\n",
					 crop->left, crop->top, crop->width, crop->height);
				return -EINVAL;
			}
			/* unite left */
			rkvpss_idx_set_bits(dev, reg_ctrl, 0, val, VPSS_UNITE_LEFT);
			rkvpss_idx_write(dev, reg_h_offs, crop->left, VPSS_UNITE_LEFT);
			rkvpss_idx_write(dev, reg_v_offs, crop->top, VPSS_UNITE_LEFT);
			/* if no scale left don't enlarge */
			if (crop->width == stream->out_fmt.width)
				rkvpss_idx_write(dev, reg_h_size, crop->width / 2,
						 VPSS_UNITE_LEFT);
			else
				rkvpss_idx_write(dev, reg_h_size, crop->width / 2 +
						 RKMOUDLE_UNITE_EXTEND_PIXEL, VPSS_UNITE_LEFT);
			rkvpss_idx_write(dev, reg_v_size, crop->height, VPSS_UNITE_LEFT);
			v4l2_dbg(4, rkvpss_debug, &dev->v4l2_dev,
				 "left crop left:%d top:%d w:%d h:%d\n",
				 rkvpss_idx_read(dev, reg_h_offs, VPSS_UNITE_LEFT),
				 rkvpss_idx_read(dev, reg_v_offs, VPSS_UNITE_LEFT),
				 rkvpss_idx_read(dev, reg_h_size, VPSS_UNITE_LEFT),
				 rkvpss_idx_read(dev, reg_v_size, VPSS_UNITE_LEFT));

			/* unite right */
			rkvpss_idx_set_bits(dev, reg_ctrl, 0, val, VPSS_UNITE_RIGHT);
			rkvpss_idx_write(dev, reg_h_offs, stream->unite_params.quad_crop_w,
					 VPSS_UNITE_RIGHT);
			rkvpss_idx_write(dev, reg_v_offs, crop->top, VPSS_UNITE_RIGHT);
			rkvpss_idx_write(dev, reg_h_size, crop->width / 2 +
				     RKMOUDLE_UNITE_EXTEND_PIXEL -
				     stream->unite_params.quad_crop_w, VPSS_UNITE_RIGHT);
			rkvpss_idx_write(dev, reg_v_size, crop->height, VPSS_UNITE_RIGHT);
			v4l2_dbg(4, rkvpss_debug, &dev->v4l2_dev,
				 "right crop left:%d top:%d w:%d h:%d\n",
				 rkvpss_idx_read(dev, reg_h_offs, VPSS_UNITE_RIGHT),
				 rkvpss_idx_read(dev, reg_v_offs, VPSS_UNITE_RIGHT),
				 rkvpss_idx_read(dev, reg_h_size, VPSS_UNITE_RIGHT),
				 rkvpss_idx_read(dev, reg_v_size, VPSS_UNITE_RIGHT));
		} else {
			rkvpss_idx_clear_bits(dev, reg_ctrl, val, VPSS_UNITE_LEFT);
			rkvpss_idx_clear_bits(dev, reg_ctrl, val, VPSS_UNITE_RIGHT);
		}
		if (sync) {
			val = RKVPSS_CROP_CHN_FORCE_UPD(stream->id);
			val |= RKVPSS_CROP_GEN_UPD;
			rkvpss_idx_write(dev, reg_upd, val, VPSS_UNITE_LEFT);
			rkvpss_idx_write(dev, reg_upd, val, VPSS_UNITE_RIGHT);
		}
	}
	stream->is_crop_upd = false;
	return 0;
}

static void poly_phase_scale(struct rkvpss_stream *stream, bool on, bool sync)
{
	struct rkvpss_device *dev = stream->dev;
	u32 out_w = stream->out_fmt.width;
	u32 out_h = stream->out_fmt.height;
	u32 in_w = stream->crop.width;
	u32 in_h = stream->crop.height;
	u32 ctrl, y_xscl_fac, y_yscl_fac, uv_xscl_fac, uv_yscl_fac;
	u32 i, j, idx, ratio, val, in_div, out_div, factor;
	bool dering_en = false, yuv420_in = false, yuv422_to_420 = false;

	if (!on) {
		rkvpss_unite_write(dev, RKVPSS_ZME_Y_SCL_CTRL, 0);
		rkvpss_unite_write(dev, RKVPSS_ZME_UV_SCL_CTRL, 0);
		if (dev->unite_mode) {
			rkvpss_idx_write(dev, RKVPSS_ZME_Y_SCL_CTRL, 0, VPSS_UNITE_LEFT);
			rkvpss_idx_write(dev, RKVPSS_ZME_UV_SCL_CTRL, 0, VPSS_UNITE_LEFT);
			rkvpss_idx_write(dev, RKVPSS_ZME_Y_SCL_CTRL, 0, VPSS_UNITE_RIGHT);
			rkvpss_idx_write(dev, RKVPSS_ZME_UV_SCL_CTRL, 0, VPSS_UNITE_RIGHT);
		}
		return;
	}

	v4l2_dbg(4, rkvpss_debug, &dev->v4l2_dev,
		 "%s id:%d in_w:%d in_h:%d out_w:%d out_h:%d\n", __func__,
		 stream->id, in_w, in_h, out_w, out_h);

	/* constraints */
	if (((out_w >= in_w) && (in_w * 8 < out_w)) ||
	    ((in_w > out_w) && (out_w * 8 < in_w)) ||
	    ((out_h >= in_h) && (in_h * 6 < out_h)) ||
	    ((in_h > out_h) && (out_h * 6 < in_h))) {
		v4l2_err(&dev->v4l2_dev, "stream:%d scale size error\n", stream->id);
		return;
	}

	/*config scl clk gate*/
	if (in_w == out_w && in_h == out_h)
		rkvpss_unite_clear_bits(dev, RKVPSS_VPSS_CLK_GATE, RKVPSS_SCL0_CKG_DIS);
	else
		rkvpss_unite_set_bits(dev, RKVPSS_VPSS_CLK_GATE, RKVPSS_SCL0_CKG_DIS,
				      RKVPSS_SCL0_CKG_DIS);

	/* TODO diff for input and output format */
	if (yuv420_in) {
		in_div = 2;
		out_div = 2;
	} else if (yuv422_to_420) {
		in_div = 1;
		out_div = 2;
	} else {
		in_div = 1;
		out_div = 1;
	}

	if (dev->unite_mode) {
		/* unite left */
		if (in_w == out_w)
			val = (in_w / 2 - 1) | ((in_h - 1) << 16);
		else
			val = (in_w / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL - 1) |
			      ((in_h - 1) << 16);
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_SRC_SIZE, val, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_SRC_SIZE, val, VPSS_UNITE_LEFT);

		/* unite right */
		val = (ALIGN(stream->unite_params.right_scl_need_size_y + 3, 2) - 1) |
			  ((in_h - 1) << 16);
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_SRC_SIZE, val, VPSS_UNITE_RIGHT);
		val = (ALIGN(stream->unite_params.right_scl_need_size_c + 6, 2) - 1) |
			  ((in_h - 1) << 16);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_SRC_SIZE, val, VPSS_UNITE_RIGHT);

		val = (out_w / 2 - 1) | ((out_h - 1) << 16);
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_DST_SIZE, val, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_DST_SIZE, val, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_DST_SIZE, val, VPSS_UNITE_RIGHT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_DST_SIZE, val, VPSS_UNITE_RIGHT);
	} else {
		val = (in_w - 1) | ((in_h - 1) << 16);
		rkvpss_unite_write(dev, RKVPSS_ZME_Y_SRC_SIZE, val);
		rkvpss_unite_write(dev, RKVPSS_ZME_UV_SRC_SIZE, val);
		val = (out_w - 1) | ((out_h - 1) << 16);
		rkvpss_unite_write(dev, RKVPSS_ZME_Y_DST_SIZE, val);
		rkvpss_unite_write(dev, RKVPSS_ZME_UV_DST_SIZE, val);
	}

	ctrl = RKVPSS_ZME_XSCL_MODE | RKVPSS_ZME_YSCL_MODE;
	if (dering_en) {
		ctrl |= RKVPSS_ZME_DERING_EN;
		rkvpss_unite_write(dev, RKVPSS_ZME_Y_DERING_PARA, 0xd10410);
		rkvpss_unite_write(dev, RKVPSS_ZME_UV_DERING_PARA, 0xd10410);

		if (dev->unite_mode) {
			rkvpss_idx_write(dev, RKVPSS_ZME_Y_DERING_PARA, 0xd10410,
					 VPSS_UNITE_LEFT);
			rkvpss_idx_write(dev, RKVPSS_ZME_UV_DERING_PARA, 0xd10410,
					 VPSS_UNITE_LEFT);
			rkvpss_idx_write(dev, RKVPSS_ZME_Y_DERING_PARA, 0xd10410,
					 VPSS_UNITE_RIGHT);
			rkvpss_idx_write(dev, RKVPSS_ZME_UV_DERING_PARA, 0xd10410,
					 VPSS_UNITE_RIGHT);
		}
	}

	if (in_w != out_w) {
		if (in_w > out_w) {
			factor = 4096;
			ctrl |= RKVPSS_ZME_XSD_EN;
		} else {
			factor = 65536;
			ctrl |= RKVPSS_ZME_XSU_EN;
		}
		y_xscl_fac = (in_w - 1) * factor / (out_w - 1);
		uv_xscl_fac = (in_w / 2 - 1) * factor / (out_w / 2 - 1);

		ratio = y_xscl_fac * 10000 / factor;
		idx = rkvpss_get_zme_tap_coe_index(ratio);
		for (i = 0; i < 17; i++) {
			for (j = 0; j < 8; j += 2) {
				val = RKVPSS_ZME_TAP_COE(rkvpss_zme_tap8_coe[idx][i][j],
							 rkvpss_zme_tap8_coe[idx][i][j + 1]);
				rkvpss_unite_write(dev,
						   RKVPSS_ZME_Y_HOR_COE0_10 + i * 16 + j * 2,
						   val);
				rkvpss_unite_write(dev,
						   RKVPSS_ZME_UV_HOR_COE0_10 + i * 16 + j * 2,
						   val);

				if (dev->unite_mode) {
					rkvpss_idx_write(dev,
							 RKVPSS_ZME_Y_HOR_COE0_10 + i * 16 + j * 2,
							 val, VPSS_UNITE_LEFT);
					rkvpss_idx_write(dev,
							 RKVPSS_ZME_UV_HOR_COE0_10 + i * 16 + j * 2,
							 val, VPSS_UNITE_LEFT);
					rkvpss_idx_write(dev,
							 RKVPSS_ZME_Y_HOR_COE0_10 + i * 16 + j * 2,
							 val, VPSS_UNITE_RIGHT);
					rkvpss_idx_write(dev,
							 RKVPSS_ZME_UV_HOR_COE0_10 + i * 16 + j * 2,
							 val, VPSS_UNITE_RIGHT);
				}
			}
		}
	} else {
		y_xscl_fac = 0;
		uv_xscl_fac = 0;
	}

	if (dev->unite_mode) {
		/* unite left */
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_XSCL_FACTOR, y_xscl_fac, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_XSCL_FACTOR, uv_xscl_fac, VPSS_UNITE_LEFT);

		/* unite right */
		val = y_xscl_fac | (stream->unite_params.y_w_phase << 16);
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_XSCL_FACTOR, val, VPSS_UNITE_RIGHT);
		val = uv_xscl_fac | (stream->unite_params.c_w_phase << 16);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_XSCL_FACTOR, val, VPSS_UNITE_RIGHT);
	} else {
		rkvpss_unite_write(dev, RKVPSS_ZME_Y_XSCL_FACTOR, y_xscl_fac);
		rkvpss_unite_write(dev, RKVPSS_ZME_UV_XSCL_FACTOR, uv_xscl_fac);
	}

	if (in_h != out_h) {
		if (in_h > out_h) {
			factor = 4096;
			ctrl |= RKVPSS_ZME_YSD_EN;
		} else {
			factor = 65536;
			ctrl |= RKVPSS_ZME_YSU_EN;
		}
		y_yscl_fac = (in_h - 1) * factor / (out_h - 1);
		uv_yscl_fac = (in_h / in_div - 1) * factor / (out_h / out_div - 1);

		ratio = y_yscl_fac * 10000 / factor;
		idx = rkvpss_get_zme_tap_coe_index(ratio);
		for (i = 0; i < 17; i++) {
			for (j = 0; j < 8; j += 2) {
				val = RKVPSS_ZME_TAP_COE(rkvpss_zme_tap6_coe[idx][i][j],
							 rkvpss_zme_tap6_coe[idx][i][j + 1]);
				rkvpss_unite_write(dev,
						   RKVPSS_ZME_Y_VER_COE0_10 + i * 16 + j * 2,
						   val);
				rkvpss_unite_write(dev,
						   RKVPSS_ZME_UV_VER_COE0_10 + i * 16 + j * 2,
						   val);

				if (dev->unite_mode) {
					rkvpss_idx_write(dev,
							 RKVPSS_ZME_Y_VER_COE0_10 + i * 16 + j * 2,
							 val, VPSS_UNITE_LEFT);
					rkvpss_idx_write(dev,
							 RKVPSS_ZME_UV_VER_COE0_10 + i * 16 + j * 2,
							 val, VPSS_UNITE_LEFT);
					rkvpss_idx_write(dev,
							 RKVPSS_ZME_Y_VER_COE0_10 + i * 16 + j * 2,
							 val, VPSS_UNITE_RIGHT);
					rkvpss_idx_write(dev,
							 RKVPSS_ZME_UV_VER_COE0_10 + i * 16 + j * 2,
							 val, VPSS_UNITE_RIGHT);
				}
			}
		}
	} else {
		y_yscl_fac = 0;
		uv_yscl_fac = 0;
	}

	if (dev->unite_mode) {
		/* unite left */
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_YSCL_FACTOR, y_yscl_fac, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_YSCL_FACTOR, uv_yscl_fac, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_SCL_CTRL, ctrl, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_SCL_CTRL, ctrl, VPSS_UNITE_LEFT);

		/* unite right */
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_YSCL_FACTOR, y_yscl_fac, VPSS_UNITE_RIGHT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_YSCL_FACTOR, uv_yscl_fac, VPSS_UNITE_RIGHT);
		rkvpss_idx_write(dev, RKVPSS_ZME_Y_SCL_CTRL, ctrl, VPSS_UNITE_RIGHT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UV_SCL_CTRL, ctrl, VPSS_UNITE_RIGHT);
	} else {
		rkvpss_unite_write(dev, RKVPSS_ZME_Y_YSCL_FACTOR, y_yscl_fac);
		rkvpss_unite_write(dev, RKVPSS_ZME_UV_YSCL_FACTOR, uv_yscl_fac);
		rkvpss_unite_write(dev, RKVPSS_ZME_Y_SCL_CTRL, ctrl);
		rkvpss_unite_write(dev, RKVPSS_ZME_UV_SCL_CTRL, ctrl);
	}

	if (dev->unite_mode) {
		/* unite left */
		val = out_w / 2;
		rkvpss_idx_write(dev, RKVPSS_ZME_H_SIZE, (val << 16) | val, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_H_OFFS, 0, VPSS_UNITE_LEFT);

		/* unite right */
		rkvpss_idx_write(dev, RKVPSS_ZME_H_SIZE, (val << 16) | val, VPSS_UNITE_RIGHT);
		rkvpss_idx_write(dev, RKVPSS_ZME_H_OFFS, 0, VPSS_UNITE_RIGHT);
		val = out_w / 2 - ALIGN_DOWN(out_w / 2, 16);
		rkvpss_idx_write(dev, RKVPSS_ZME_H_OFFS, (3 << 20) | (3 << 16) |
				(stream->unite_params.scl_in_crop_w_y << 8) |
				(stream->unite_params.scl_in_crop_w_c << 12) |
				(val << 4) | val, VPSS_UNITE_RIGHT);
	}

	ctrl = RKVPSS_ZME_GATING_EN;
	if (yuv420_in)
		ctrl |= RKVPSS_ZME_SCL_YUV420_REAL_EN;
	if (yuv422_to_420)
		ctrl |= RKVPSS_ZME_422TO420_EN;
	if (dev->unite_mode) {
		/* unite left */
		ctrl |= RKVPSS_ZME_CLIP_EN | RKVPSS_ZME_8K_EN;
		rkvpss_idx_write(dev, RKVPSS_ZME_CTRL, ctrl, VPSS_UNITE_LEFT);

		/* unite left */
		ctrl |= RKVPSS_ZME_IN_CLIP_EN;
		rkvpss_idx_write(dev, RKVPSS_ZME_CTRL, ctrl, VPSS_UNITE_RIGHT);
	} else {
		rkvpss_unite_write(dev, RKVPSS_ZME_CTRL, ctrl);
	}

	val = RKVPSS_ZME_GEN_UPD;
	if (sync)
		val |= RKVPSS_ZME_FORCE_UPD;
	rkvpss_unite_write(dev, RKVPSS_ZME_UPDATE, val);
	if (dev->unite_mode) {
		rkvpss_idx_write(dev, RKVPSS_ZME_UPDATE, val, VPSS_UNITE_LEFT);
		rkvpss_idx_write(dev, RKVPSS_ZME_UPDATE, val, VPSS_UNITE_RIGHT);
	}
}

static void bilinear_scale(struct rkvpss_stream *stream, bool on, bool sync)
{
	struct rkvpss_device *dev = stream->dev;
	u32 out_w = stream->out_fmt.width;
	u32 out_h = stream->out_fmt.height;
	u32 in_w = stream->crop.width;
	u32 in_h = stream->crop.height;
	u32 in_div, out_div;
	u32 reg, val, ctrl = 0, clk_mask = 0;
	bool yuv420_in = false, yuv422_to_420 = false;

	if (!on) {
		reg = stream->config->scale.ctrl;
		rkvpss_unite_write(dev, reg, 0);
		return;
	}

	/*config scl clk gate*/
	switch (stream->id) {
	case RKVPSS_OUTPUT_CH1:
		clk_mask = RKVPSS_SCL1_CKG_DIS;
		break;
	case RKVPSS_OUTPUT_CH2:
		clk_mask = RKVPSS_SCL2_CKG_DIS;
		break;
	case RKVPSS_OUTPUT_CH3:
		clk_mask = RKVPSS_SCL3_CKG_DIS;
		break;
	default:
		return;
	}
	if (in_w == out_w && in_h == out_h)
		rkvpss_unite_clear_bits(dev, RKVPSS_VPSS_CLK_GATE, clk_mask);
	else
		rkvpss_unite_set_bits(dev, RKVPSS_VPSS_CLK_GATE, clk_mask, clk_mask);

	if (!dev->unite_mode) {
		/* TODO diff for input and output format */
		if (yuv420_in) {
			in_div = 2;
			out_div = 2;
		} else if (yuv422_to_420) {
			in_div = 1;
			out_div = 2;
		} else {
			in_div = 1;
			out_div = 1;
		}

		reg = stream->config->scale.hy_offs;
		rkvpss_unite_write(dev, reg, 0);
		reg = stream->config->scale.hc_offs;
		rkvpss_unite_write(dev, reg, 0);
		reg = stream->config->scale.vy_offs;
		rkvpss_unite_write(dev, reg, 0);
		reg = stream->config->scale.vc_offs;
		rkvpss_unite_write(dev, reg, 0);

		val = in_w | (in_h << 16);
		reg = stream->config->scale.src_size;
		rkvpss_unite_write(dev, reg, val);

		/* ch2 ch3 scale out width exceed 1920 */
		if ((stream->id >= RKVPSS_OUTPUT_CH2) &&
		    (in_w != out_w) && (out_w > 1920))
			v4l2_err(&dev->v4l2_dev,
				 "stream:%d scale out width exceed 1920\n",
				 stream->id);

		val = out_w | (out_h << 16);
		reg = stream->config->scale.dst_size;
		rkvpss_unite_write(dev, reg, val);

		if (in_w != out_w || !sync) {
			val = (in_w - 1) * 4096 / (out_w - 1);
			reg = stream->config->scale.hy_fac;
			rkvpss_unite_write(dev, reg, val);

			val = (in_w / 2 - 1) * 4096 / (out_w / 2 - 1);
			reg = stream->config->scale.hc_fac;
			rkvpss_unite_write(dev, reg, val);

			ctrl |= RKVPSS_SCL_HY_EN | RKVPSS_SCL_HC_EN;
		}
		if (in_h != out_h || !sync) {
			val = (in_h - 1) * 4096 / (out_h - 1);
			reg = stream->config->scale.vy_fac;
			rkvpss_unite_write(dev, reg, val);

			val = (in_h / in_div - 1) * 4096 / (out_h / out_div - 1);
			reg = stream->config->scale.vc_fac;
			rkvpss_unite_write(dev, reg, val);

			ctrl |= RKVPSS_SCL_VY_EN | RKVPSS_SCL_VC_EN;
		}

		reg = stream->config->scale.ctrl;
		rkvpss_unite_write(dev, reg, ctrl);

		val = RKVPSS_SCL_GEN_UPD;
		if (sync)
			val |= RKVPSS_SCL_FORCE_UPD;
		reg = stream->config->scale.update;
		rkvpss_unite_write(dev, reg, val);
	} else {
		/* unite left */
		reg = stream->config->scale.in_crop_offs;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_LEFT);

		reg = stream->config->scale.hy_offs;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_LEFT);
		reg = stream->config->scale.hc_offs;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_LEFT);
		reg = stream->config->scale.vy_offs;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_LEFT);
		reg = stream->config->scale.vc_offs;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_LEFT);

		reg = stream->config->scale.hy_offs_mi;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_LEFT);
		reg = stream->config->scale.hc_offs_mi;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_LEFT);

		if (in_w == out_w)
			val = (in_w / 2) | (in_h << 16);
		else
			val = (in_w / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL) | (in_h << 16);
		reg = stream->config->scale.src_size;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_LEFT);

		/* ch2 ch3 scale out width exceed 1920 */
		if ((stream->id >= RKVPSS_OUTPUT_CH2) &&
		    (in_w != out_w) && (out_w / 2 > 1920))
			v4l2_err(&dev->v4l2_dev,
				 "stream:%d scale out width exceed 1920\n",
				 stream->id);

		val = (out_w / 2) | (out_h << 16);
		reg = stream->config->scale.dst_size;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_LEFT);

		ctrl |= RKVPSS_SCL_CLIP_EN;

		if (in_w != out_w || !sync) {
			reg = stream->config->scale.hy_fac;
			rkvpss_idx_write(dev, reg, stream->unite_params.y_w_fac, VPSS_UNITE_LEFT);
			reg = stream->config->scale.hc_fac;
			rkvpss_idx_write(dev, reg, stream->unite_params.c_w_fac, VPSS_UNITE_LEFT);
			ctrl |= RKVPSS_SCL_HY_EN | RKVPSS_SCL_HC_EN;
		}

		if (in_h != out_h || !sync) {
			reg = stream->config->scale.vy_fac;
			rkvpss_idx_write(dev, reg, stream->unite_params.y_h_fac, VPSS_UNITE_LEFT);
			reg = stream->config->scale.vc_fac;
			rkvpss_idx_write(dev, reg, stream->unite_params.c_h_fac, VPSS_UNITE_LEFT);
			ctrl |= RKVPSS_SCL_VY_EN | RKVPSS_SCL_VC_EN;
		}

		reg = stream->config->scale.ctrl;
		rkvpss_idx_write(dev, reg, ctrl, VPSS_UNITE_LEFT);

		val = RKVPSS_SCL_GEN_UPD;
		if (sync)
			val |= RKVPSS_SCL_FORCE_UPD;
		reg = stream->config->scale.update;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_LEFT);

		/* unite right */
		ctrl = 0;
		val = stream->unite_params.scl_in_crop_w_y |
		      (stream->unite_params.scl_in_crop_w_c << 4);
		reg = stream->config->scale.in_crop_offs;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);

		val = stream->unite_params.y_w_phase;
		reg = stream->config->scale.hy_offs;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);
		val = stream->unite_params.c_w_phase;
		reg = stream->config->scale.hc_offs;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);
		reg = stream->config->scale.vy_offs;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_RIGHT);
		reg = stream->config->scale.vc_offs;
		rkvpss_idx_write(dev, reg, 0, VPSS_UNITE_RIGHT);

		val = out_w / 2 - ALIGN_DOWN(out_w / 2, 16);
		reg = stream->config->scale.hy_offs_mi;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);
		reg = stream->config->scale.hc_offs_mi;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);

		val = (in_w / 2 + RKMOUDLE_UNITE_EXTEND_PIXEL) | (in_h << 16);
		reg = stream->config->scale.src_size;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);

		/* ch2 ch3 scale out width exceed 1920 */
		if ((stream->id >= RKVPSS_OUTPUT_CH2) &&
		    (in_w != out_w) && (out_w / 2 > 1920))
			v4l2_err(&dev->v4l2_dev,
				 "stream:%d scale out width exceed 1920\n",
				 stream->id);

		val = (out_w / 2) | (out_h << 16);
		reg = stream->config->scale.dst_size;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);

		ctrl |= RKVPSS_SCL_CLIP_EN | RKVPSS_SCL_IN_CLIP_EN;

		if (in_w != out_w || !sync) {
			reg = stream->config->scale.hy_fac;
			rkvpss_idx_write(dev, reg, stream->unite_params.y_w_fac, VPSS_UNITE_RIGHT);
			reg = stream->config->scale.hc_fac;
			rkvpss_idx_write(dev, reg, stream->unite_params.c_w_fac, VPSS_UNITE_RIGHT);
			ctrl |= RKVPSS_SCL_HY_EN | RKVPSS_SCL_HC_EN;
		}

		if (in_h != out_h || !sync) {
			reg = stream->config->scale.vy_fac;
			rkvpss_idx_write(dev, reg, stream->unite_params.y_h_fac, VPSS_UNITE_RIGHT);
			reg = stream->config->scale.vc_fac;
			rkvpss_idx_write(dev, reg, stream->unite_params.c_h_fac, VPSS_UNITE_RIGHT);
			ctrl |= RKVPSS_SCL_VY_EN | RKVPSS_SCL_VC_EN;
		}

		reg = stream->config->scale.ctrl;
		rkvpss_idx_write(dev, reg, ctrl, VPSS_UNITE_RIGHT);

		val = RKVPSS_SCL_GEN_UPD;
		if (sync)
			val |= RKVPSS_SCL_FORCE_UPD;
		reg = stream->config->scale.update;
		rkvpss_idx_write(dev, reg, val, VPSS_UNITE_RIGHT);
	}
}

static int rkvpss_stream_scale(struct rkvpss_stream *stream, bool on, bool sync)
{
	if (stream->id == RKVPSS_OUTPUT_CH0)
		poly_phase_scale(stream, on, sync);
	else
		bilinear_scale(stream, on, sync);
	return 0;
}

static void rkvpss_stream_stop(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	int ret;

	stream->stopping = true;
	if (atomic_read(&dev->pipe_stream_cnt) > 0) {
		ret = wait_event_timeout(stream->done, !stream->streaming,
					 msecs_to_jiffies(300));
		if (!ret)
			v4l2_warn(&dev->v4l2_dev, "%s id:%d timeout\n", __func__, stream->id);
	}
	stream->stopping = false;
	stream->streaming = false;
	if (stream->ops->disable_mi)
		stream->ops->disable_mi(stream);
	rkvpss_stream_crop(stream, false, true);
	rkvpss_stream_scale(stream, false, true);
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s id:%d\n", __func__,
		 stream->id);
}

static void rkvpss_stop_streaming(struct vb2_queue *queue)
{
	struct rkvpss_stream *stream = queue->drv_priv;
	struct rkvpss_vdev_node *node = &stream->vnode;
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_hw_dev *hw = dev->hw_dev;

	if (!stream->streaming)
		return;

	mutex_lock(&hw->dev_lock);
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s %s id:%d enter\n", __func__,
		 node->vdev.name, stream->id);

	if (atomic_read(&dev->pipe_stream_cnt) == 1) {
		rkvpss_pipeline_stream(dev, false);
		rkvpss_stream_stop(stream);
	} else {
		rkvpss_stream_stop(stream);
		rkvpss_pipeline_stream(dev, false);
	}
	destroy_buf_queue(stream, VB2_BUF_STATE_ERROR);
	rkvpss_pipeline_close(dev);
	tasklet_disable(&stream->buf_done_tasklet);
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s %s id:%d exit\n", __func__,
		 node->vdev.name, stream->id);
	mutex_unlock(&hw->dev_lock);
}

static int check_wr_uvswap(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_stream *check_stream;
	struct capture_fmt *fmt;
	bool wr_uv_swap = false;
	int i, ret = 0;

	for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
		check_stream = &dev->stream_vdev.stream[i];
		if (check_stream->streaming) {
			fmt = &check_stream->out_cap_fmt;
			switch (fmt->fourcc) {
			case V4L2_PIX_FMT_NV21:
			case V4L2_PIX_FMT_NV61:
			case V4L2_PIX_FMT_VYUY:
				wr_uv_swap = true;
				break;
			default:
				break;
			}
		}
	}
	if (wr_uv_swap) {
		switch (stream->out_cap_fmt.fourcc) {
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_UYVY:
			v4l2_err(&dev->v4l2_dev, "wr_uv_swap need to be consistent\n");
			ret = -EINVAL;
			break;
		default:
			break;
		}
	}

	return ret;
}

static int rkvpss_stream_start(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	int ret = 0;

	if (dev->unite_mode)
		calc_unite_scl_params(stream);

	stream->is_crop_upd = true;
	ret = rkvpss_stream_scale(stream, true, true);
	if (ret < 0)
		return ret;
	ret = rkvpss_stream_crop(stream, true, true);
	if (ret < 0)
		return ret;
	ret = check_wr_uvswap(stream);
	if (ret < 0)
		return ret;

	stream->is_pause = true;

	if (stream->ops->config_mi)
		stream->ops->config_mi(stream);

	stream->streaming = true;
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s id:%d enter\n", __func__,
		 stream->id);
	return ret;
}

static int rkvpss_start_streaming(struct vb2_queue *queue, unsigned int count)
{
	struct rkvpss_stream *stream = queue->drv_priv;
	struct rkvpss_vdev_node *node = &stream->vnode;
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_hw_dev *hw = dev->hw_dev;
	int ret = -EINVAL;

	if (stream->streaming)
		return -EBUSY;

	mutex_lock(&hw->dev_lock);
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s %s id:%d enter\n", __func__,
		 node->vdev.name, stream->id);

	if (!dev->inp || !stream->linked) {
		v4l2_err(&dev->v4l2_dev, "no link or invalid input source\n");
		goto free_buf_queue;
	}

	if (hw->is_ofl_ch[stream->id]) {
		v4l2_err(&dev->v4l2_dev, "channel[%d] already assigned to offline", stream->id);
		goto free_buf_queue;
	}

	if (rkvpss_pipeline_open(dev) < 0)
		goto free_buf_queue;

	ret = rkvpss_stream_start(stream);
	if (ret < 0) {
		v4l2_err(&dev->v4l2_dev, "start %s failed\n", node->vdev.name);
		goto pipe_close;
	}

	ret = rkvpss_pipeline_stream(dev, true);
	if (ret < 0)
		goto stop_stream;

	tasklet_enable(&stream->buf_done_tasklet);
	stream->dbg.id = -1;
	stream->dbg.frameloss = 0;
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s %s id:%d exit\n", __func__,
		 node->vdev.name, stream->id);
	mutex_unlock(&hw->dev_lock);
	return 0;
stop_stream:
	stream->streaming = false;
	rkvpss_stream_stop(stream);
pipe_close:
	rkvpss_pipeline_close(dev);
free_buf_queue:
	destroy_buf_queue(stream, VB2_BUF_STATE_QUEUED);
	mutex_unlock(&hw->dev_lock);
	return ret;
}

static const struct vb2_ops stream_vb2_ops = {
	.queue_setup = rkvpss_queue_setup,
	.buf_queue = rkvpss_buf_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.stop_streaming = rkvpss_stop_streaming,
	.start_streaming = rkvpss_start_streaming,
};

static int rkvpss_init_vb2_queue(struct vb2_queue *q,
				 struct rkvpss_stream *stream,
				 enum v4l2_buf_type buf_type)
{
	q->type = buf_type;
	q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_USERPTR;
	q->drv_priv = stream;
	q->ops = &stream_vb2_ops;
	q->mem_ops = stream->dev->hw_dev->mem_ops;
	q->buf_struct_size = sizeof(struct rkvpss_buffer);
	q->min_buffers_needed = STREAM_OUT_REQ_BUFS_MIN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	q->lock = &stream->dev->apilock;
	q->dev = stream->dev->hw_dev->dev;
	q->allow_cache_hints = 1;
	q->bidirectional = 1;
	if (stream->dev->hw_dev->is_dma_contig)
		q->dma_attrs = DMA_ATTR_FORCE_CONTIGUOUS;
	q->gfp_flags = GFP_DMA32;
	return vb2_queue_init(q);
}

static const
struct capture_fmt *find_fmt(struct rkvpss_stream *stream, u32 pixelfmt)
{
	const struct capture_fmt *fmt;
	u32 i;

	for (i = 0; i < stream->config->fmt_size; i++) {
		fmt = &stream->config->fmts[i];
		if (fmt->fourcc == pixelfmt)
			return fmt;
	}
	return NULL;
}

static int fcc_xysubs(u32 fcc, u32 *xsubs, u32 *ysubs)
{
	switch (fcc) {
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_YUV444M:
		*xsubs = 1;
		*ysubs = 1;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_FBC2:
		*xsubs = 2;
		*ysubs = 1;
		break;
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_FBC0:
		*xsubs = 2;
		*ysubs = 2;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rkvpss_set_fmt(struct rkvpss_stream *stream,
			  struct v4l2_pix_format_mplane *pixm,
			  bool try)
{
	struct rkvpss_device *dev = stream->dev;
	u32 i, planes, xsubs = 1, ysubs = 1, imagsize = 0;
	const struct capture_fmt *fmt;

	fmt = find_fmt(stream, pixm->pixelformat);
	if (!fmt) {
		v4l2_err(&dev->v4l2_dev,
			 "nonsupport pixelformat:%c%c%c%c\n",
			 pixm->pixelformat,
			 pixm->pixelformat >> 8,
			 pixm->pixelformat >> 16,
			 pixm->pixelformat >> 24);
		return -EINVAL;
	}

	/* Tile4x4 writing of Channel0 and Channel1 only supports either one.*/
	if (fmt->fourcc == V4L2_PIX_FMT_TILE420 || fmt->fourcc == V4L2_PIX_FMT_TILE422) {
		if ((stream->id == 0) &&
		    dev->stream_vdev.stream[1].streaming &&
		    (dev->stream_vdev.stream[1].out_cap_fmt.fourcc == V4L2_PIX_FMT_TILE420 ||
		     dev->stream_vdev.stream[1].out_cap_fmt.fourcc == V4L2_PIX_FMT_TILE422)) {
			v4l2_err(&dev->v4l2_dev,
				 "Tile4x4 writing of Ch0 and Cl1 only supports either one\n");
			return -EINVAL;
		}
		if ((stream->id == 1) &&
		    dev->stream_vdev.stream[0].streaming &&
		    (dev->stream_vdev.stream[0].out_cap_fmt.fourcc == V4L2_PIX_FMT_TILE420 ||
		     dev->stream_vdev.stream[0].out_cap_fmt.fourcc == V4L2_PIX_FMT_TILE422)) {
			v4l2_err(&dev->v4l2_dev,
				 "Tile4x4 writing of Ch0 and Cl1 only supports either one\n");
			return -EINVAL;
		}
	}

	pixm->num_planes = fmt->mplanes;
	pixm->field = V4L2_FIELD_NONE;
	if (!pixm->quantization)
		pixm->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fcc_xysubs(fmt->fourcc, &xsubs, &ysubs);
	planes = fmt->cplanes ? fmt->cplanes : fmt->mplanes;
	for (i = 0; i < planes; i++) {
		struct v4l2_plane_pix_format *plane_fmt;
		u32 width, height, bytesperline, w, h;

		plane_fmt = pixm->plane_fmt + i;
		w = pixm->width;
		h = pixm->height;
		width = i ? w / xsubs : w;
		height = i ? h / ysubs : h;

		if (fmt->fourcc == V4L2_PIX_FMT_TILE420 || fmt->fourcc == V4L2_PIX_FMT_TILE422)
			bytesperline = ALIGN(((width / 4) * fmt->bpp[i]), 16);
		else
			bytesperline = width * DIV_ROUND_UP(fmt->bpp[i], 8);

		if (i != 0 || plane_fmt->bytesperline < bytesperline)
			plane_fmt->bytesperline = bytesperline;

		if (fmt->fourcc == V4L2_PIX_FMT_TILE420 || fmt->fourcc == V4L2_PIX_FMT_TILE422)
			plane_fmt->sizeimage = plane_fmt->bytesperline * (height / 4);
		else
			plane_fmt->sizeimage = plane_fmt->bytesperline * height;

		imagsize += plane_fmt->sizeimage;
	}
	if (fmt->mplanes == 1)
		pixm->plane_fmt[0].sizeimage = imagsize;
	if (!try) {
		stream->out_cap_fmt = *fmt;
		stream->out_fmt = *pixm;
		v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
			 "%s: stream:%d req(%d, %d) out(%d, %d)\n",
			 __func__, stream->id, pixm->width, pixm->height,
			 stream->out_fmt.width, stream->out_fmt.height);
	}
	return 0;
}

/************************* v4l2_file_operations***************************/

static int rkvpss_fh_open(struct file *file)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	struct rkvpss_device *dev = stream->dev;
	int ret;

	if (!dev || !dev->is_probe_end ||
	    !dev->hw_dev || !dev->hw_dev->is_probe_end)
		return -EINVAL;

	ret = v4l2_fh_open(file);
	if (!ret) {
		ret = v4l2_pipeline_pm_get(&stream->vnode.vdev.entity);
		if (ret < 0) {
			v4l2_err(&dev->v4l2_dev,
				 "pipeline power on failed %d\n", ret);
			vb2_fop_release(file);
		}
	}
	return ret;
}

static int rkvpss_fh_release(struct file *file)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	int ret;

	ret = vb2_fop_release(file);
	if (!ret)
		v4l2_pipeline_pm_put(&stream->vnode.vdev.entity);
	return ret;
}

static const struct v4l2_file_operations rkvpss_fops = {
	.open = rkvpss_fh_open,
	.release = rkvpss_fh_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

static int rkvpss_enum_input(struct file *file, void *priv,
			     struct v4l2_input *input)
{
	if (input->index > 0)
		return -EINVAL;

	input->type = V4L2_INPUT_TYPE_CAMERA;
	strscpy(input->name, "Camera", sizeof(input->name));

	return 0;
}

static int rkvpss_try_fmt_vid_mplane(struct file *file, void *fh,
				     struct v4l2_format *f)
{
	struct rkvpss_stream *stream = video_drvdata(file);

	return rkvpss_set_fmt(stream, &f->fmt.pix_mp, true);
}

static int rkvpss_enum_fmt_vid_mplane(struct file *file, void *priv,
				      struct v4l2_fmtdesc *f)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	const struct capture_fmt *fmt = NULL;

	if (f->index >= stream->config->fmt_size)
		return -EINVAL;

	fmt = &stream->config->fmts[f->index];
	f->pixelformat = fmt->fourcc;

	switch (f->pixelformat) {
	case V4L2_PIX_FMT_TILE420:
		strscpy(f->description,
			"Rockchip yuv420 tile",
			sizeof(f->description));
		break;
	case V4L2_PIX_FMT_TILE422:
		strscpy(f->description,
			"Rockchip yuv422 tile",
			sizeof(f->description));
		break;
	default:
		break;
	}

	return 0;
}

static int rkvpss_s_fmt_vid_mplane(struct file *file,
				   void *priv, struct v4l2_format *f)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	struct video_device *vdev = &stream->vnode.vdev;
	struct rkvpss_vdev_node *node = vdev_to_node(vdev);
	struct rkvpss_device *dev = stream->dev;

	/* Change not allowed if queue is streaming. */
	if (vb2_is_streaming(&node->buf_queue)) {
		v4l2_err(&dev->v4l2_dev, "%s queue busy\n", __func__);
		return -EBUSY;
	}

	return rkvpss_set_fmt(stream, &f->fmt.pix_mp, false);
}

static int rkvpss_g_fmt_vid_mplane(struct file *file, void *fh,
				       struct v4l2_format *f)
{
	struct rkvpss_stream *stream = video_drvdata(file);

	f->fmt.pix_mp = stream->out_fmt;

	return 0;
}

static int rkvpss_g_selection(struct file *file, void *prv,
			      struct v4l2_selection *sel)
{
	struct rkvpss_stream *stream = video_drvdata(file);

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = stream->crop;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rkvpss_s_selection(struct file *file, void *prv,
			      struct v4l2_selection *sel)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	struct rkvpss_device *dev = stream->dev;
	struct v4l2_rect *crop = &stream->crop;
	u32 max_w = dev->vpss_sdev.in_fmt.width;
	u32 max_h = dev->vpss_sdev.in_fmt.height;

	if (sel->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;
	if (sel->flags != 0)
		return -EINVAL;

	sel->r.left = ALIGN(sel->r.left, 2);
	sel->r.top = ALIGN(sel->r.top, 2);
	sel->r.width = ALIGN(sel->r.width, 2);
	sel->r.height = ALIGN(sel->r.height, 2);
	if (!sel->r.width || !sel->r.height ||
	    sel->r.width + sel->r.left > max_w ||
	    sel->r.height + sel->r.top > max_h) {
		v4l2_err(&dev->v4l2_dev,
			 "invalid crop left:%d top:%d w:%d h:%d for %dx%d\n",
			 sel->r.left, sel->r.top, sel->r.width, sel->r.height, max_w, max_h);
		return -EINVAL;
	}

	*crop = sel->r;
	stream->is_crop_upd = true;
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "stream %d crop(%d,%d)/%dx%d\n", stream->id,
		 crop->left, crop->top, crop->width, crop->height);
	return 0;
}

static int rkvpss_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	struct device *dev = stream->dev->dev;
	struct video_device *vdev = video_devdata(file);

	strscpy(cap->card, vdev->name, sizeof(cap->card));
	snprintf(cap->driver, sizeof(cap->driver),
		 "%s_v%d", dev->driver->name,
		 stream->dev->vpss_ver >> 4);
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev_name(dev));

	return 0;
}

static void rkvpss_stream_mf(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	u32 val, mask;

	if (!stream->is_mf_upd)
		return;
	stream->is_mf_upd = false;

	if (!dev->hw_dev->is_ofl_cmsc) {
		mask = RKVPSS_MIR_EN;
		val = dev->mir_en ? RKVPSS_MIR_EN : 0;
		rkvpss_unite_set_bits(dev, RKVPSS_VPSS_CTRL, mask, val);
	}
	mask = RKVPSS_MI_CHN_V_FLIP(stream->id);
	/* Tile4x4 writing can't flip*/
	if (stream->out_cap_fmt.fourcc == V4L2_PIX_FMT_TILE420 ||
	    stream->out_cap_fmt.fourcc == V4L2_PIX_FMT_TILE422)
		stream->flip_en = false;
	val = stream->flip_en ? mask : 0;
	rkvpss_unite_set_bits(dev, RKVPSS_MI_WR_VFLIP_CTRL, mask, val);
}

static int rkvpss_get_mirror_flip(struct rkvpss_stream *stream,
				  struct rkvpss_mirror_flip *cfg)
{
	cfg->mirror = stream->dev->mir_en;
	cfg->flip = stream->flip_en;
	return 0;
}

static int rkvpss_set_mirror_flip(struct rkvpss_stream *stream,
				  struct rkvpss_mirror_flip *cfg)
{
	struct rkvpss_device *dev = stream->dev;

	if (dev->hw_dev->is_ofl_cmsc && cfg->mirror) {
		cfg->mirror = 0;
		v4l2_warn(&stream->dev->v4l2_dev,
			  "mirror mux to vpss offline mode\n");
	}
	if (dev->mir_en != !!cfg->mirror ||
	    stream->flip_en != !!cfg->flip)
		stream->is_mf_upd = true;
	dev->mir_en = !!cfg->mirror;
	stream->flip_en = !!cfg->flip;
	return 0;
}

static void cmsc_config_hw(struct rkvpss_device *dev, struct rkvpss_cmsc_cfg *cfg, bool sync,
			       int unite_index)
{
	int i, j, k, slope, hor;
	u32 win_en, mode, val, ctrl = 0;

	for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
		win_en = cfg->win[i].win_en;
		v4l2_dbg(4, rkvpss_debug, &dev->v4l2_dev,
		 "%s unite:%d, unite_index:%d ch:%d win_en:0x%x\n", __func__,
		 dev->unite_mode, unite_index, i, win_en);
		if (win_en)
			ctrl |= RKVPSS_CMSC_CHN_EN(i);
		rkvpss_idx_write(dev, RKVPSS_CMSC_CHN0_WIN + i * 4, win_en, unite_index);

		mode = cfg->win[i].mode;
		rkvpss_idx_write(dev, RKVPSS_CMSC_CHN0_MODE + i * 4, mode, unite_index);

		for (j = 0; j < RKVPSS_CMSC_WIN_MAX && win_en; j++) {
			if (!(win_en & BIT(j)))
				continue;
			for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
				val = RKVPSS_CMSC_WIN_VTX(cfg->win[j].point[k].x,
							  cfg->win[j].point[k].y);
				rkvpss_idx_write(dev, RKVPSS_CMSC_WIN0_L0_VTX + k * 8 + j * 32, val,
						 unite_index);

				rkvpss_cmsc_slop(&cfg->win[j].point[k],
						 (k + 1 == RKVPSS_CMSC_POINT_MAX) ?
						 &cfg->win[j].point[0] : &cfg->win[j].point[k + 1],
						 &slope, &hor);
				val = RKVPSS_CMSC_WIN_SLP(slope, hor);
				rkvpss_idx_write(dev, RKVPSS_CMSC_WIN0_L0_SLP + k * 8 + j * 32, val,
						 unite_index);
				v4l2_dbg(4, rkvpss_debug, &dev->v4l2_dev,
					 "%s unite:%d unite_index:%d ch:%d win:%d point:%d x:%u y:%u",
					 __func__, dev->unite_mode, unite_index, i, j, k,
					 cfg->win[j].point[k].x,
					 cfg->win[j].point[k].y);
			}

			if ((mode & BIT(j)))
				continue;
			val = RKVPSS_CMSK_WIN_YUV(cfg->win[j].cover_color_y,
						  cfg->win[j].cover_color_u,
						  cfg->win[j].cover_color_v);
			val |= RKVPSS_CMSC_WIN_ALPHA(cfg->win[j].cover_color_a);
			rkvpss_idx_write(dev, RKVPSS_CMSC_WIN0_PARA + j * 4, val, unite_index);
		}
	}

	if (ctrl) {
		ctrl |= RKVPSS_CMSC_EN;
		ctrl |= RKVPSS_CMSC_BLK_SZIE(cfg->mosaic_block);
	}
	rkvpss_idx_write(dev, RKVPSS_CMSC_CTRL, ctrl, unite_index);
	val = RKVPSS_CMSC_GEN_UPD;
	if (sync)
		val |= RKVPSS_CMSC_FORCE_UPD;
	rkvpss_idx_write(dev, RKVPSS_CMSC_UPDATE, val, unite_index);
}

void rkvpss_cmsc_config_v10(struct rkvpss_device *dev, bool sync)
{
	unsigned long lock_flags = 0;
	struct rkvpss_cmsc_cfg left_cfg = {0}, right_cfg = {0};
	struct rkvpss_cmsc_win *win;
	struct rkvpss_cmsc_point *point;
	int i, j, k;
	u32  mask;

	spin_lock_irqsave(&dev->cmsc_lock, lock_flags);
	if (dev->hw_dev->is_ofl_cmsc || !dev->cmsc_upd) {
		spin_unlock_irqrestore(&dev->cmsc_lock, lock_flags);
		return;
	}
	dev->cmsc_upd = false;
	spin_unlock_irqrestore(&dev->cmsc_lock, lock_flags);

	if (dev->unite_mode) {
		/* unite left */
		for (i = 0; i < RKVPSS_CMSC_WIN_MAX; i++) {
			left_cfg.win[i].cover_color_y = dev->cmsc_cfg.win[i].cover_color_y;
			left_cfg.win[i].cover_color_u = dev->cmsc_cfg.win[i].cover_color_u;
			left_cfg.win[i].cover_color_v = dev->cmsc_cfg.win[i].cover_color_v;
			left_cfg.win[i].cover_color_a = dev->cmsc_cfg.win[i].cover_color_a;
			for (j = 0; j < RKVPSS_CMSC_POINT_MAX; j++)
				left_cfg.win[i].point[j] = dev->cmsc_cfg.win[i].point[j];
		}
		left_cfg.mosaic_block = dev->cmsc_cfg.mosaic_block;
		for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
			left_cfg.win[i].win_en = dev->cmsc_cfg.win[i].win_en;
			left_cfg.win[i].mode = dev->cmsc_cfg.win[i].mode;
		}

		for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
			for (j = 0; j < RKVPSS_CMSC_WIN_MAX; j++) {
				win = &left_cfg.win[j];
				if (!(left_cfg.win[i].win_en & BIT(j)))
					continue;
				mask = 0;
				for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
					point = &win->point[k];
					if (point->x >= dev->vpss_sdev.in_fmt.width / 2)
						mask |= BIT(k);
					else
						mask &= ~BIT(k);
				}
				if (mask == 0xf) {
					/** all right **/
					left_cfg.win[i].win_en &= ~BIT(j);
				} else if (mask != 0) {
					/** middle  need avoid pentagon **/
					if (win->point[0].x != win->point[3].x ||
					    win->point[1].x != win->point[2].x) {
						left_cfg.win[i].win_en &= ~BIT(j);
					} else {
						win->point[1].x = dev->vpss_sdev.in_fmt.width / 2;
						win->point[2].x = dev->vpss_sdev.in_fmt.width / 2;
					}
				}
			}
		}

		/* unite right */
		for (i = 0; i < RKVPSS_CMSC_WIN_MAX; i++) {
			right_cfg.win[i].cover_color_y = dev->cmsc_cfg.win[i].cover_color_y;
			right_cfg.win[i].cover_color_u = dev->cmsc_cfg.win[i].cover_color_u;
			right_cfg.win[i].cover_color_v = dev->cmsc_cfg.win[i].cover_color_v;
			right_cfg.win[i].cover_color_a = dev->cmsc_cfg.win[i].cover_color_a;

			for (j = 0; j < RKVPSS_CMSC_POINT_MAX; j++)
				right_cfg.win[i].point[j] = dev->cmsc_cfg.win[i].point[j];
		}
		right_cfg.mosaic_block = dev->cmsc_cfg.mosaic_block;
		for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
			right_cfg.win[i].win_en = dev->cmsc_cfg.win[i].win_en;
			right_cfg.win[i].mode = dev->cmsc_cfg.win[i].mode;
		}

		for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
			for (j = 0; j < RKVPSS_CMSC_WIN_MAX; j++) {
				win = &right_cfg.win[j];
				if (!(right_cfg.win[i].win_en & BIT(j)))
					continue;
				mask = 0;
				for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
					point = &win->point[k];
					if (point->x <= dev->vpss_sdev.in_fmt.width / 2)
						mask |= BIT(k);
					else
						mask &= ~BIT(k);
				}
				if (mask == 0xf) {
					/** all left **/
					right_cfg.win[i].win_en &= ~BIT(j);
				} else if (mask != 0) {
					/** middle  need avoid pentagon **/
					if (win->point[0].x != win->point[3].x ||
					    win->point[1].x != win->point[2].x) {
						right_cfg.win[i].win_en &= ~BIT(j);
					} else {
						win->point[0].x = RKMOUDLE_UNITE_EXTEND_PIXEL;
						win->point[3].x = RKMOUDLE_UNITE_EXTEND_PIXEL;
						win->point[1].x = win->point[1].x -
								  (dev->vpss_sdev.in_fmt.width / 2)
								  + RKMOUDLE_UNITE_EXTEND_PIXEL;
						win->point[2].x = win->point[2].x -
								  (dev->vpss_sdev.in_fmt.width / 2)
								  + RKMOUDLE_UNITE_EXTEND_PIXEL;
					}
				} else {
					/** all right **/
					win->point[0].x = win->point[0].x -
							  (dev->vpss_sdev.in_fmt.width / 2) +
							  RKMOUDLE_UNITE_EXTEND_PIXEL;
					win->point[1].x = win->point[1].x -
							  (dev->vpss_sdev.in_fmt.width / 2) +
							  RKMOUDLE_UNITE_EXTEND_PIXEL;
					win->point[2].x = win->point[2].x -
							  (dev->vpss_sdev.in_fmt.width / 2) +
							  RKMOUDLE_UNITE_EXTEND_PIXEL;
					win->point[3].x = win->point[3].x -
							  (dev->vpss_sdev.in_fmt.width / 2) +
							  RKMOUDLE_UNITE_EXTEND_PIXEL;
				}
			}
		}

		cmsc_config_hw(dev, &left_cfg, sync, VPSS_UNITE_LEFT);
		cmsc_config_hw(dev, &right_cfg, sync, VPSS_UNITE_RIGHT);
	} else {
		cmsc_config_hw(dev, &dev->cmsc_cfg, sync, VPSS_UNITE_LEFT);
	}
}

static int rkvpss_get_cmsc(struct rkvpss_stream *stream, struct rkvpss_cmsc_cfg *cfg)
{
	struct rkvpss_device *dev = stream->dev;
	unsigned long lock_flags = 0;
	u32 i, win_en, mode;

	spin_lock_irqsave(&dev->cmsc_lock, lock_flags);
	*cfg = dev->cmsc_cfg;
	spin_unlock_irqrestore(&dev->cmsc_lock, lock_flags);

	win_en = cfg->win[stream->id].win_en;
	mode = cfg->win[stream->id].mode;
	for (i = 0; i < RKVPSS_CMSC_WIN_MAX; i++) {
		cfg->win[i].win_en = !!(win_en & BIT(i));
		cfg->win[i].mode = !!(mode & BIT(i));
	}
	cfg->width_ro = dev->vpss_sdev.in_fmt.width;
	cfg->height_ro = dev->vpss_sdev.in_fmt.height;
	return 0;
}

static int rkvpss_set_cmsc(struct rkvpss_stream *stream, struct rkvpss_cmsc_cfg *cfg)
{
	struct rkvpss_device *dev = stream->dev;
	unsigned long lock_flags = 0;
	u16 i, j, k, win_en = 0, mode = 0;
	int ret = 0;

	if (dev->hw_dev->is_ofl_cmsc)
		return -EINVAL;

	spin_lock_irqsave(&dev->cmsc_lock, lock_flags);
	for (i = 0; i < RKVPSS_CMSC_WIN_MAX; i++) {
		if (!cfg->win[i].win_en)
			continue;

		win_en |= BIT(i);
		mode |= cfg->win[i].mode ? BIT(i) : 0;

		for (j = 0; j < RKVPSS_CMSC_POINT_MAX; j++) {
			for (k = j + 1; k < RKVPSS_CMSC_POINT_MAX; k++) {
				if (cfg->win[i].point[j].x == cfg->win[i].point[k].x &&
				    cfg->win[i].point[j].y == cfg->win[i].point[k].y) {
					v4l2_warn(&dev->v4l2_dev,
						  "points should different, P%d(%d,%d) P%d(%d,%d)\n",
						  j,
						  cfg->win[i].point[j].x,
						  cfg->win[i].point[j].y,
						  k,
						  cfg->win[i].point[k].x,
						  cfg->win[i].point[k].y);
					ret = -EINVAL;
					goto unlock;
				}
			}
		}
		if (!cfg->win[i].mode) {
			dev->cmsc_cfg.win[i].cover_color_y = cfg->win[i].cover_color_y;
			dev->cmsc_cfg.win[i].cover_color_u = cfg->win[i].cover_color_u;
			dev->cmsc_cfg.win[i].cover_color_v = cfg->win[i].cover_color_v;
			dev->cmsc_cfg.win[i].cover_color_a = cfg->win[i].cover_color_a;
			if (dev->cmsc_cfg.win[i].cover_color_a > 15)
				dev->cmsc_cfg.win[i].cover_color_a = 15;
		}
		for (j = 0; j < RKVPSS_CMSC_POINT_MAX; j++)
			dev->cmsc_cfg.win[i].point[j] = cfg->win[i].point[j];
	}
	dev->cmsc_cfg.win[stream->id].win_en = win_en;
	dev->cmsc_cfg.win[stream->id].mode = mode;
	dev->cmsc_cfg.mosaic_block = cfg->mosaic_block;
	dev->cmsc_upd = true;
	v4l2_dbg(3, rkvpss_debug, &dev->v4l2_dev,
		 "%s ch:%d win_en:0x%x",
		 __func__, stream->id,
		 dev->cmsc_cfg.win[stream->id].win_en);

unlock:
	spin_unlock_irqrestore(&dev->cmsc_lock, lock_flags);
	return ret;
}

static long rkvpss_ioctl_default(struct file *file, void *fh,
				 bool valid_prio, unsigned int cmd, void *arg)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	long ret = 0;

	if (!arg)
		return -EINVAL;

	switch (cmd) {
	case RKVPSS_CMD_GET_MIRROR_FLIP:
		ret = rkvpss_get_mirror_flip(stream, arg);
		break;
	case RKVPSS_CMD_SET_MIRROR_FLIP:
		ret = rkvpss_set_mirror_flip(stream, arg);
		break;
	case RKVPSS_CMD_GET_CMSC:
		ret = rkvpss_get_cmsc(stream, arg);
		break;
	case RKVPSS_CMD_SET_CMSC:
		ret = rkvpss_set_cmsc(stream, arg);
		break;
	case RKVPSS_CMD_STREAM_ATTACH_INFO:
		stream->is_attach_info = *(int *)arg;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ioctl_ops rkvpss_v4l2_ioctl_ops = {
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_enum_input = rkvpss_enum_input,
	.vidioc_try_fmt_vid_cap_mplane = rkvpss_try_fmt_vid_mplane,
	.vidioc_enum_fmt_vid_cap = rkvpss_enum_fmt_vid_mplane,
	.vidioc_s_fmt_vid_cap_mplane = rkvpss_s_fmt_vid_mplane,
	.vidioc_g_fmt_vid_cap_mplane = rkvpss_g_fmt_vid_mplane,
	.vidioc_s_selection = rkvpss_s_selection,
	.vidioc_g_selection = rkvpss_g_selection,
	.vidioc_querycap = rkvpss_querycap,
	.vidioc_default = rkvpss_ioctl_default,
};

static void rkvpss_unregister_stream_video(struct rkvpss_stream *stream)
{
	tasklet_kill(&stream->buf_done_tasklet);
	media_entity_cleanup(&stream->vnode.vdev.entity);
	video_unregister_device(&stream->vnode.vdev);
}

static int rkvpss_register_stream_video(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	struct video_device *vdev = &stream->vnode.vdev;
	struct rkvpss_vdev_node *node;
	enum v4l2_buf_type buf_type;
	int ret = 0;

	node = vdev_to_node(vdev);
	vdev->release = video_device_release_empty;
	vdev->fops = &rkvpss_fops;
	vdev->minor = -1;
	vdev->v4l2_dev = v4l2_dev;
	vdev->lock = &dev->apilock;
	video_set_drvdata(vdev, stream);

	vdev->ioctl_ops = &rkvpss_v4l2_ioctl_ops;
	vdev->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE;
	vdev->vfl_dir = VFL_DIR_RX;
	node->pad.flags = MEDIA_PAD_FL_SINK;
	buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	rkvpss_init_vb2_queue(&node->buf_queue, stream, buf_type);
	vdev->queue = &node->buf_queue;

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		v4l2_err(v4l2_dev,
			 "video register failed with error %d\n", ret);
		return ret;
	}

	ret = media_entity_pads_init(&vdev->entity, 1, &node->pad);
	if (ret < 0)
		goto unreg;

	INIT_LIST_HEAD(&stream->buf_done_list);
	tasklet_init(&stream->buf_done_tasklet,
		     rkvpss_buf_done_task, (unsigned long)stream);
	tasklet_disable(&stream->buf_done_tasklet);
	return 0;
unreg:
	video_unregister_device(vdev);
	return ret;
}

void rkvpss_stream_default_fmt_v10(struct rkvpss_device *dev, u32 id,
			       u32 width, u32 height, u32 pixelformat)
{
	struct rkvpss_stream *stream = &dev->stream_vdev.stream[id];
	struct v4l2_pix_format_mplane pixm = { 0 };

	if (pixelformat)
		pixm.pixelformat = pixelformat;
	else
		pixm.pixelformat = stream->out_cap_fmt.fourcc;
	if (!pixm.pixelformat)
		return;

	stream->crop.left = 0;
	stream->crop.top = 0;
	stream->crop.width = width;
	stream->crop.height = height;

	pixm.width = width;
	pixm.height = height;
	rkvpss_set_fmt(stream, &pixm, false);
}

int rkvpss_register_stream_vdevs_v10(struct rkvpss_device *dev)
{
	struct rkvpss_stream_vdev *stream_vdev;
	struct rkvpss_stream *stream;
	struct video_device *vdev;
	char *vdev_name;
	int i, j, ret = 0;

	stream_vdev = &dev->stream_vdev;
	memset(stream_vdev, 0, sizeof(*stream_vdev));

	for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
		stream = &stream_vdev->stream[i];
		stream->id = i;
		stream->dev = dev;
		vdev = &stream->vnode.vdev;
		INIT_LIST_HEAD(&stream->buf_queue);
		init_waitqueue_head(&stream->done);
		spin_lock_init(&stream->vbq_lock);
		switch (i) {
		case RKVPSS_OUTPUT_CH0:
			vdev_name = S0_VDEV_NAME;
			stream->ops = &scl_stream_ops;
			stream->config = &scl0_config;
			break;
		case RKVPSS_OUTPUT_CH1:
			vdev_name = S1_VDEV_NAME;
			stream->ops = &scl_stream_ops;
			stream->config = &scl1_config;
			break;
		case RKVPSS_OUTPUT_CH2:
			vdev_name = S2_VDEV_NAME;
			stream->ops = &scl_stream_ops;
			stream->config = &scl2_config;
			break;
		case RKVPSS_OUTPUT_CH3:
			vdev_name = S3_VDEV_NAME;
			stream->ops = &scl_stream_ops;
			stream->config = &scl3_config;
			break;
		default:
			v4l2_err(&dev->v4l2_dev, "Invalid stream:%d\n", i);
			return -EINVAL;
		}
		strscpy(vdev->name, vdev_name, sizeof(vdev->name));
		ret = rkvpss_register_stream_video(stream);
		if (ret < 0)
			goto err;
	}
	return 0;
err:
	for (j = 0; j < i; j++) {
		stream = &stream_vdev->stream[j];
		rkvpss_unregister_stream_video(stream);
	}
	return ret;
}

void rkvpss_unregister_stream_vdevs_v10(struct rkvpss_device *dev)
{
	struct rkvpss_stream_vdev *stream_vdev;
	struct rkvpss_stream *stream;
	int i;

	stream_vdev = &dev->stream_vdev;
	for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
		stream = &stream_vdev->stream[i];
		rkvpss_unregister_stream_video(stream);
	}
}

void rkvpss_isr_v10(struct rkvpss_device *dev, u32 mis_val)
{
	v4l2_dbg(3, rkvpss_debug, &dev->v4l2_dev,
		 "isr:0x%x\n", mis_val);
	dev->isr_cnt++;
	if (mis_val & RKVPSS_ISP_ALL_FRM_END && dev->remote_sd)
		rkvpss_check_idle(dev, VPSS_FRAME_END);
}

void rkvpss_mi_isr_v10(struct rkvpss_device *dev, u32 mis_val)
{
	struct rkvpss_stream *stream;
	int i, ris = readl(dev->hw_dev->base_addr + RKVPSS_MI_RIS);

	v4l2_dbg(3, rkvpss_debug, &dev->v4l2_dev,
		 "mi isr:0x%x, ris:0x%x\n", mis_val, ris);

	for (i = 0; i < RKVPSS_OUT_V10_MAX; i++) {
		stream = &dev->stream_vdev.stream[i];

		if (!stream->streaming ||
		    !(ris & stream->config->frame_end_id))
			continue;
		writel(stream->config->frame_end_id, dev->hw_dev->base_addr + RKVPSS_MI_ICR);

		if (!dev->unite_mode || dev->unite_index == VPSS_UNITE_RIGHT) {
			if (stream->stopping) {
				stream->stopping = false;
				stream->streaming = false;
				stream->ops->disable_mi(stream);
				wake_up(&stream->done);
			} else {
				rkvpss_frame_end(stream);
			}
		}
	}
	rkvpss_check_idle(dev, (ris & 0xf) << 3);
}
