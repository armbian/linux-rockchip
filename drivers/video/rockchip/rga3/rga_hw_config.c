// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * Author:
 *	Huang Lee <Putin.li@rock-chips.com>
 */

#include "rga_hw_config.h"

/* RGA 1Word = 4Byte */
#define WORD_TO_BYTE(w) ((w) * 4)

const uint32_t rga3_input_raster_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YVYU_422,
	RGA_FORMAT_VYUY_422,
	RGA_FORMAT_YUYV_422,
	RGA_FORMAT_UYVY_422,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
	RGA_FORMAT_ARGB_8888,
	RGA_FORMAT_ABGR_8888,
	RGA_FORMAT_XRGB_8888,
	RGA_FORMAT_XBGR_8888,
};

const uint32_t rga3_output_raster_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YVYU_422,
	RGA_FORMAT_VYUY_422,
	RGA_FORMAT_YUYV_422,
	RGA_FORMAT_UYVY_422,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
};

const uint32_t rga3_fbcd_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_ARGB_8888,
	RGA_FORMAT_ABGR_8888,
	RGA_FORMAT_XRGB_8888,
	RGA_FORMAT_XBGR_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
};

const uint32_t rga3_fbce_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
};

const uint32_t rga3_tile_format[] = {
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
};

const uint32_t rga2e_input_raster_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_YCbCr_422_P,
	RGA_FORMAT_YCbCr_420_P,
	RGA_FORMAT_YCrCb_422_P,
	RGA_FORMAT_YCrCb_420_P,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YVYU_422,
	RGA_FORMAT_VYUY_422,
	RGA_FORMAT_YUYV_422,
	RGA_FORMAT_UYVY_422,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
	RGA_FORMAT_YCbCr_400,
	RGA_FORMAT_XRGB_8888,
	RGA_FORMAT_XBGR_8888,
	RGA_FORMAT_BPP1,
	RGA_FORMAT_BPP2,
	RGA_FORMAT_BPP4,
	RGA_FORMAT_BPP8,
	RGA_FORMAT_ARGB_8888,
	RGA_FORMAT_ARGB_5551,
	RGA_FORMAT_ARGB_4444,
	RGA_FORMAT_ABGR_8888,
	RGA_FORMAT_ABGR_5551,
	RGA_FORMAT_ABGR_4444,
};

const uint32_t rga2e_output_raster_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_YCbCr_422_P,
	RGA_FORMAT_YCbCr_420_P,
	RGA_FORMAT_YCrCb_422_P,
	RGA_FORMAT_YCrCb_420_P,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YVYU_420,
	RGA_FORMAT_VYUY_420,
	RGA_FORMAT_YUYV_420,
	RGA_FORMAT_UYVY_420,
	RGA_FORMAT_YVYU_422,
	RGA_FORMAT_VYUY_422,
	RGA_FORMAT_YUYV_422,
	RGA_FORMAT_UYVY_422,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
	RGA_FORMAT_Y4,
	RGA_FORMAT_YCbCr_400,
	RGA_FORMAT_XRGB_8888,
	RGA_FORMAT_XBGR_8888,
	RGA_FORMAT_ARGB_8888,
	RGA_FORMAT_ARGB_5551,
	RGA_FORMAT_ARGB_4444,
	RGA_FORMAT_ABGR_8888,
	RGA_FORMAT_ABGR_5551,
	RGA_FORMAT_ABGR_4444,
};

const uint32_t rga2p_input_raster_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_YCbCr_422_P,
	RGA_FORMAT_YCbCr_420_P,
	RGA_FORMAT_YCrCb_422_P,
	RGA_FORMAT_YCrCb_420_P,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YVYU_422,
	RGA_FORMAT_VYUY_422,
	RGA_FORMAT_YUYV_422,
	RGA_FORMAT_UYVY_422,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
	RGA_FORMAT_YCbCr_400,
	RGA_FORMAT_XRGB_8888,
	RGA_FORMAT_XBGR_8888,
	RGA_FORMAT_BPP1,
	RGA_FORMAT_BPP2,
	RGA_FORMAT_BPP4,
	RGA_FORMAT_BPP8,
	RGA_FORMAT_ARGB_8888,
	RGA_FORMAT_ARGB_5551,
	RGA_FORMAT_ARGB_4444,
	RGA_FORMAT_ABGR_8888,
	RGA_FORMAT_ABGR_5551,
	RGA_FORMAT_ABGR_4444,
	RGA_FORMAT_RGBA_2BPP,
	RGA_FORMAT_A8,
	RGA_FORMAT_YCbCr_444_SP,
	RGA_FORMAT_YCrCb_444_SP,
};

const uint32_t rga2p_input1_raster_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_XRGB_8888,
	RGA_FORMAT_XBGR_8888,
	RGA_FORMAT_ARGB_8888,
	RGA_FORMAT_ABGR_8888,
	RGA_FORMAT_ARGB_5551,
	RGA_FORMAT_ABGR_5551,
	RGA_FORMAT_ARGB_4444,
	RGA_FORMAT_ABGR_4444,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_A8,
};

const uint32_t rga2p_output_raster_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
	RGA_FORMAT_RGB_565,
	RGA_FORMAT_BGR_565,
	RGA_FORMAT_YCbCr_422_P,
	RGA_FORMAT_YCbCr_420_P,
	RGA_FORMAT_YCrCb_422_P,
	RGA_FORMAT_YCrCb_420_P,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YVYU_420,
	RGA_FORMAT_VYUY_420,
	RGA_FORMAT_YUYV_420,
	RGA_FORMAT_UYVY_420,
	RGA_FORMAT_YVYU_422,
	RGA_FORMAT_VYUY_422,
	RGA_FORMAT_YUYV_422,
	RGA_FORMAT_UYVY_422,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
	RGA_FORMAT_Y4,
	RGA_FORMAT_Y8,
	RGA_FORMAT_YCbCr_400,
	RGA_FORMAT_RGBA_5551,
	RGA_FORMAT_BGRA_5551,
	RGA_FORMAT_RGBA_4444,
	RGA_FORMAT_BGRA_4444,
	RGA_FORMAT_XRGB_8888,
	RGA_FORMAT_XBGR_8888,
	RGA_FORMAT_ARGB_8888,
	RGA_FORMAT_ARGB_5551,
	RGA_FORMAT_ARGB_4444,
	RGA_FORMAT_ABGR_8888,
	RGA_FORMAT_ABGR_5551,
	RGA_FORMAT_ABGR_4444,
	RGA_FORMAT_YCbCr_444_SP,
	RGA_FORMAT_YCrCb_444_SP,
};

const uint32_t rga2p_tile4x4_format[] = {
	RGA_FORMAT_YCbCr_400,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCbCr_444_SP,
	RGA_FORMAT_YCrCb_444_SP,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
};

const uint32_t rga2p_rkfbc64x4_format[] = {
	RGA_FORMAT_YCbCr_400,
	RGA_FORMAT_YCbCr_420_SP,
	RGA_FORMAT_YCrCb_420_SP,
	RGA_FORMAT_YCbCr_422_SP,
	RGA_FORMAT_YCrCb_422_SP,
	RGA_FORMAT_YCbCr_444_SP,
	RGA_FORMAT_YCrCb_444_SP,
	RGA_FORMAT_YCbCr_420_SP_10B,
	RGA_FORMAT_YCrCb_420_SP_10B,
	RGA_FORMAT_YCbCr_422_SP_10B,
	RGA_FORMAT_YCrCb_422_SP_10B,
};

const uint32_t rga2p_afbc32x8_format[] = {
	RGA_FORMAT_RGBA_8888,
	RGA_FORMAT_BGRA_8888,
	RGA_FORMAT_RGBX_8888,
	RGA_FORMAT_BGRX_8888,
	RGA_FORMAT_XRGB_8888,
	RGA_FORMAT_XBGR_8888,
	RGA_FORMAT_ARGB_8888,
	RGA_FORMAT_ABGR_8888,
	RGA_FORMAT_RGB_888,
	RGA_FORMAT_BGR_888,
};

const struct rga_win_data rga3_win_data[] = {
	{
		.name = "rga3-win0",
		.formats[RGA_RASTER_INDEX] = rga3_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga3_input_raster_format),
		.formats[RGA_AFBC16x16_INDEX] = rga3_fbcd_format,
		.formats_count[RGA_AFBC16x16_INDEX] = ARRAY_SIZE(rga3_fbcd_format),
		.formats[RGA_TILE8x8_INDEX] = rga3_tile_format,
		.formats_count[RGA_TILE8x8_INDEX] = ARRAY_SIZE(rga3_tile_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE | RGA_FBC_MODE | RGA_TILE_MODE,

	},

	{
		.name = "rga3-win1",
		.formats[RGA_RASTER_INDEX] = rga3_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga3_input_raster_format),
		.formats[RGA_AFBC16x16_INDEX] = rga3_fbcd_format,
		.formats_count[RGA_AFBC16x16_INDEX] = ARRAY_SIZE(rga3_fbcd_format),
		.formats[RGA_TILE8x8_INDEX] = rga3_tile_format,
		.formats_count[RGA_TILE8x8_INDEX] = ARRAY_SIZE(rga3_tile_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE | RGA_FBC_MODE | RGA_TILE_MODE,

	},

	{
		.name = "rga3-wr",
		.formats[RGA_RASTER_INDEX] = rga3_output_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga3_output_raster_format),
		.formats[RGA_AFBC16x16_INDEX] = rga3_fbce_format,
		.formats_count[RGA_AFBC16x16_INDEX] = ARRAY_SIZE(rga3_fbce_format),
		.formats[RGA_TILE8x8_INDEX] = rga3_tile_format,
		.formats_count[RGA_TILE8x8_INDEX] = ARRAY_SIZE(rga3_tile_format),
		.supported_rotations = 0,
		.scale_up_mode = RGA_SCALE_UP_NONE,
		.scale_down_mode = RGA_SCALE_DOWN_NONE,
		.rd_mode = RGA_RASTER_MODE | RGA_FBC_MODE | RGA_TILE_MODE,

	},
};

const struct rga_win_data rga2e_win_data[] = {
	{
		.name = "rga2e-src0",
		.formats[RGA_RASTER_INDEX] = rga2e_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2e_input_raster_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE,

	},

	{
		.name = "rga2e-src1",
		.formats[RGA_RASTER_INDEX] = rga2e_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2e_input_raster_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE,

	},

	{
		.name = "rga2-dst",
		.formats[RGA_RASTER_INDEX] = rga2e_output_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2e_output_raster_format),
		.supported_rotations = 0,
		.scale_up_mode = RGA_SCALE_UP_NONE,
		.scale_down_mode = RGA_SCALE_DOWN_NONE,
		.rd_mode = RGA_RASTER_MODE,

	},
};

const struct rga_win_data rga2e_3506_win_data[] = {
	{
		.name = "rga2e-src0",
		.formats[RGA_RASTER_INDEX] = rga2e_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2e_input_raster_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE,

	},

	{
		.name = "rga2e-src1",
		.formats[RGA_RASTER_INDEX] = rga2p_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2p_input_raster_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE,

	},

	{
		.name = "rga2-dst",
		.formats[RGA_RASTER_INDEX] = rga2e_output_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2e_output_raster_format),
		.supported_rotations = 0,
		.scale_up_mode = RGA_SCALE_UP_NONE,
		.scale_down_mode = RGA_SCALE_DOWN_NONE,
		.rd_mode = RGA_RASTER_MODE,

	},
};

const struct rga_win_data rga2p_win_data[] = {
	{
		.name = "rga2p-src0",
		.formats[RGA_RASTER_INDEX] = rga2p_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2p_input_raster_format),
		.formats[RGA_TILE4x4_INDEX] = rga2p_tile4x4_format,
		.formats_count[RGA_TILE4x4_INDEX] = ARRAY_SIZE(rga2p_tile4x4_format),
		.formats[RGA_RKFBC64x4_INDEX] = rga2p_rkfbc64x4_format,
		.formats_count[RGA_RKFBC64x4_INDEX] = ARRAY_SIZE(rga2p_rkfbc64x4_format),
		.formats[RGA_AFBC32x8_INDEX] = rga2p_afbc32x8_format,
		.formats_count[RGA_AFBC32x8_INDEX] = ARRAY_SIZE(rga2p_afbc32x8_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE | RGA_TILE4x4_MODE | RGA_RKFBC_MODE | RGA_AFBC32x8_MODE,

	},

	{
		.name = "rga2p-src1",
		.formats[RGA_RASTER_INDEX] = rga2p_input1_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2p_input1_raster_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE | RGA_TILE4x4_MODE | RGA_RKFBC_MODE | RGA_AFBC32x8_MODE,

	},

	{
		.name = "rga2p-dst",
		.formats[RGA_RASTER_INDEX] = rga2p_output_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2p_output_raster_format),
		.formats[RGA_TILE4x4_INDEX] = rga2p_tile4x4_format,
		.formats_count[RGA_TILE4x4_INDEX] = ARRAY_SIZE(rga2p_tile4x4_format),
		.supported_rotations = 0,
		.scale_up_mode = RGA_SCALE_UP_NONE,
		.scale_down_mode = RGA_SCALE_DOWN_NONE,
		.rd_mode = RGA_RASTER_MODE | RGA_TILE4x4_MODE,

	},
};

const struct rga_win_data rga2p_non_fbc_win_data[] = {
	{
		.name = "rga2p-src0",
		.formats[RGA_RASTER_INDEX] = rga2p_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2p_input_raster_format),
		.formats[RGA_TILE4x4_INDEX] = rga2p_tile4x4_format,
		.formats_count[RGA_TILE4x4_INDEX] = ARRAY_SIZE(rga2p_tile4x4_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE | RGA_TILE4x4_MODE,

	},

	{
		.name = "rga2p-src1",
		.formats[RGA_RASTER_INDEX] = rga2p_input1_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2p_input1_raster_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE,

	},

	{
		.name = "rga2p-dst",
		.formats[RGA_RASTER_INDEX] = rga2p_output_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2p_output_raster_format),
		.supported_rotations = 0,
		.scale_up_mode = RGA_SCALE_UP_NONE,
		.scale_down_mode = RGA_SCALE_DOWN_NONE,
		.rd_mode = RGA_RASTER_MODE,

	},
};

const struct rga_win_data rga2p_lite_win_data[] = {
	{
		.name = "rga2e-src0",
		.formats[RGA_RASTER_INDEX] = rga2e_input_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2e_input_raster_format),
		.supported_rotations = RGA_MODE_ROTATE_MASK,
		.scale_up_mode = RGA_SCALE_UP_BIC,
		.scale_down_mode = RGA_SCALE_DOWN_AVG,
		.rd_mode = RGA_RASTER_MODE,

	},

	{
		.name = "rga2-dst",
		.formats[RGA_RASTER_INDEX] = rga2e_output_raster_format,
		.formats_count[RGA_RASTER_INDEX] = ARRAY_SIZE(rga2e_output_raster_format),
		.supported_rotations = 0,
		.scale_up_mode = RGA_SCALE_UP_NONE,
		.scale_down_mode = RGA_SCALE_DOWN_NONE,
		.rd_mode = RGA_RASTER_MODE,

	},
};

const struct rga_hw_data rga3_data = {
	.version = 0,
	.input_range = {{68, 2}, {8176, 8176}},
	.output_range = {{68, 2}, {8128, 8128}},

	.win = rga3_win_data,
	.win_size = ARRAY_SIZE(rga3_win_data),
	/* 1 << factor mean real factor */
	.max_upscale_factor = 3,
	.max_downscale_factor = 3,

	.byte_stride_align = 16,
	.max_byte_stride = WORD_TO_BYTE(8192),

	.feature = RGA_COLOR_KEY,
	.csc_r2y_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709 | RGA_MODE_CSC_BT2020,
	.csc_y2r_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709 | RGA_MODE_CSC_BT2020,
	.mmu = RGA_IOMMU,
};

const struct rga_hw_data rga2e_data = {
	.version = 0,
	.input_range = {{2, 2}, {8192, 8192}},
	.output_range = {{2, 2}, {4096, 4096}},

	.win = rga2e_win_data,
	.win_size = ARRAY_SIZE(rga2e_win_data),
	/* 1 << factor mean real factor */
	.max_upscale_factor = 4,
	.max_downscale_factor = 4,

	.byte_stride_align = 4,
	.max_byte_stride = WORD_TO_BYTE(8192),

	.feature = RGA_COLOR_FILL | RGA_COLOR_PALETTE |
		   RGA_COLOR_KEY | RGA_ROP_CALCULATE |
		   RGA_NN_QUANTIZE | RGA_DITHER | RGA_FULL_CSC,
	.csc_r2y_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.csc_y2r_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.mmu = RGA_MMU,
};

const struct rga_hw_data rga2e_1106_data = {
	.version = 0,
	.input_range = {{2, 2}, {8192, 8192}},
	.output_range = {{2, 2}, {4096, 4096}},

	.win = rga2e_win_data,
	.win_size = ARRAY_SIZE(rga2e_win_data),
	/* 1 << factor mean real factor */
	.max_upscale_factor = 4,
	.max_downscale_factor = 4,

	.byte_stride_align = 4,
	.max_byte_stride = WORD_TO_BYTE(8192),

	.feature = RGA_COLOR_FILL | RGA_COLOR_PALETTE |
		   RGA_COLOR_KEY | RGA_ROP_CALCULATE |
		   RGA_NN_QUANTIZE | RGA_DITHER | RGA_MOSAIC |
		   RGA_YIN_YOUT | RGA_YUV_HDS | RGA_YUV_VDS |
		   RGA_OSD | RGA_PRE_INTR | RGA_FULL_CSC,
	.csc_r2y_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.csc_y2r_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.mmu = RGA_NONE_MMU,
};

const struct rga_hw_data rga2e_3506_data = {
	.version = 0,
	.input_range = {{2, 2}, {1280, 8192}},
	.output_range = {{2, 2}, {1280, 4096}},

	.win = rga2e_3506_win_data,
	.win_size = ARRAY_SIZE(rga2e_3506_win_data),
	/* 1 << factor mean real factor */
	.max_upscale_factor = 4,
	.max_downscale_factor = 4,

	.byte_stride_align = 4,
	.max_byte_stride = WORD_TO_BYTE(8192),

	.feature = RGA_COLOR_FILL | RGA_COLOR_PALETTE |
		   RGA_COLOR_KEY | RGA_YIN_YOUT | RGA_YUV_HDS | RGA_YUV_VDS |
		   RGA_PRE_INTR | RGA_FULL_CSC | RGA_GAUSS,
	.csc_r2y_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.csc_y2r_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.mmu = RGA_NONE_MMU,
};

const struct rga_hw_data rga2e_iommu_data = {
	.version = 0,
	.input_range = {{2, 2}, {8192, 8192}},
	.output_range = {{2, 2}, {4096, 4096}},

	.win = rga2e_win_data,
	.win_size = ARRAY_SIZE(rga2e_win_data),
	/* 1 << factor mean real factor */
	.max_upscale_factor = 4,
	.max_downscale_factor = 4,

	.byte_stride_align = 4,
	.max_byte_stride = WORD_TO_BYTE(8192),

	.feature = RGA_COLOR_FILL | RGA_COLOR_PALETTE |
		   RGA_COLOR_KEY | RGA_ROP_CALCULATE |
		   RGA_NN_QUANTIZE | RGA_DITHER | RGA_MOSAIC |
		   RGA_YIN_YOUT | RGA_YUV_HDS | RGA_YUV_VDS |
		   RGA_OSD | RGA_PRE_INTR | RGA_FULL_CSC,
	.csc_r2y_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.csc_y2r_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.mmu = RGA_IOMMU,
};

const struct rga_hw_data rga2p_iommu_data = {
	.version = 0,
	.input_range = {{2, 2}, {8192, 8192}},
	.output_range = {{2, 2}, {8192, 8192}},

	.win = rga2p_win_data,
	.win_size = ARRAY_SIZE(rga2p_win_data),
	/* 1 << factor mean real factor */
	.max_upscale_factor = 4,
	.max_downscale_factor = 4,

	.byte_stride_align = 4,
	.max_byte_stride = WORD_TO_BYTE(8192),

	.feature = RGA_COLOR_FILL | RGA_COLOR_PALETTE |
		   RGA_COLOR_KEY | RGA_ROP_CALCULATE |
		   RGA_NN_QUANTIZE | RGA_DITHER | RGA_MOSAIC |
		   RGA_YIN_YOUT | RGA_YUV_HDS | RGA_YUV_VDS |
		   RGA_OSD | RGA_PRE_INTR | RGA_FULL_CSC,
	.csc_r2y_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.csc_y2r_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.mmu = RGA_IOMMU,
};

const struct rga_hw_data rga2p_lite_1103b_data = {
	.version = 0,
	.input_range = {{2, 2}, {2880, 8192}},
	.output_range = {{2, 2}, {2880, 8192}},

	.win = rga2p_lite_win_data,
	.win_size = ARRAY_SIZE(rga2p_lite_win_data),
	/* 1 << factor mean real factor */
	.max_upscale_factor = 4,
	.max_downscale_factor = 4,

	.byte_stride_align = 4,
	.max_byte_stride = WORD_TO_BYTE(8192),

	.feature = RGA_COLOR_FILL | RGA_DITHER | RGA_YIN_YOUT |
		   RGA_YUV_HDS | RGA_YUV_VDS |
		   RGA_PRE_INTR | RGA_FULL_CSC,
	.csc_r2y_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.csc_y2r_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.mmu = RGA_NONE_MMU,
};

const struct rga_hw_data rga2p_iommu_non_fbc_data = {
	/* Register have changed, so the version is increased. */
	.version = 1,
	.input_range = {{2, 2}, {8192, 8192}},
	.output_range = {{2, 2}, {8192, 8192}},

	.win = rga2p_non_fbc_win_data,
	.win_size = ARRAY_SIZE(rga2p_non_fbc_win_data),
	/* 1 << factor mean real factor */
	.max_upscale_factor = 4,
	.max_downscale_factor = 4,

	.byte_stride_align = 4,
	.max_byte_stride = WORD_TO_BYTE(8192),

	.feature = RGA_COLOR_FILL | RGA_COLOR_PALETTE |
		   RGA_COLOR_KEY | RGA_DITHER |
		   RGA_YIN_YOUT | RGA_YUV_HDS | RGA_YUV_VDS |
		   RGA_OSD | RGA_PRE_INTR | RGA_FULL_CSC,
	.csc_r2y_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.csc_y2r_mode = RGA_MODE_CSC_BT601L | RGA_MODE_CSC_BT601F |
			RGA_MODE_CSC_BT709,
	.mmu = RGA_IOMMU,
};
