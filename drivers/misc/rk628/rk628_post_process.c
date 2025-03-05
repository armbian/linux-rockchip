// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 *
 * Author: Wyon Bi <bivvy.bi@rock-chips.com>
 */

#include <linux/debugfs.h>
#include "rk628.h"
#include "rk628_config.h"
#include "rk628_cru.h"
#include "rk628_post_process.h"
#include "rk628_rgb.h"

#define PQ_CSC_HUE_TABLE_NUM			256
#define PQ_CSC_MODE_COEF_COMMENT_LEN		32
#define PQ_CSC_SIMPLE_MAT_PARAM_FIX_BIT_WIDTH	10
#define PQ_CSC_SIMPLE_MAT_PARAM_FIX_NUM		(1 << PQ_CSC_SIMPLE_MAT_PARAM_FIX_BIT_WIDTH)

#define PQ_CALC_ENHANCE_BIT			6
/* csc convert coef fixed-point num bit width */
#define PQ_CSC_PARAM_FIX_BIT_WIDTH		10
/* csc convert coef half fixed-point num bit width */
#define PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH		(PQ_CSC_PARAM_FIX_BIT_WIDTH - 1)
/* csc convert coef fixed-point num */
#define PQ_CSC_PARAM_FIX_NUM			(1 << PQ_CSC_PARAM_FIX_BIT_WIDTH)
#define PQ_CSC_PARAM_HALF_FIX_NUM		(1 << PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH)
/* csc input param bit width */
#define PQ_CSC_IN_PARAM_NORM_BIT_WIDTH		9
/* csc input param normalization coef */
#define PQ_CSC_IN_PARAM_NORM_COEF		(1 << PQ_CSC_IN_PARAM_NORM_BIT_WIDTH)

/* csc hue table range [0,255] */
#define PQ_CSC_HUE_TABLE_DIV_COEF		2
/* csc brightness offset */
#define PQ_CSC_BRIGHTNESS_OFFSET		256

/* dc coef base bit width */
#define PQ_CSC_DC_COEF_BASE_BIT_WIDTH		10
/* input dc coef offset for 10bit data */
#define PQ_CSC_DC_IN_OFFSET			64
/* input and output dc coef offset for 10bit data u,v */
#define PQ_CSC_DC_IN_OUT_DEFAULT		512
/* r,g,b color temp div coef, range [-128,128] for 10bit data */
#define PQ_CSC_TEMP_OFFSET_DIV_COEF		2

#define	MAX(a, b)				((a) > (b) ? (a) : (b))
#define	MIN(a, b)				((a) < (b) ? (a) : (b))
#define	CLIP(x, min_v, max_v)			MIN(MAX(x, min_v), max_v)

#define V4L2_COLORSPACE_BT709F	0xfe
#define V4L2_COLORSPACE_BT2020F	0xff

enum vop_csc_bit_depth {
	CSC_10BIT_DEPTH,
	CSC_13BIT_DEPTH,
};

enum rk_pq_csc_mode {
	RK_PQ_CSC_YUV2RGB_601 = 0,             /* YCbCr_601 LIMIT-> RGB FULL */
	RK_PQ_CSC_YUV2RGB_709,                 /* YCbCr_709 LIMIT-> RGB FULL */
	RK_PQ_CSC_RGB2YUV_601,                 /* RGB FULL->YCbCr_601 LIMIT */
	RK_PQ_CSC_RGB2YUV_709,                 /* RGB FULL->YCbCr_709 LIMIT */
	RK_PQ_CSC_YUV2YUV_709_601,             /* YCbCr_709 LIMIT->YCbCr_601 LIMIT */
	RK_PQ_CSC_YUV2YUV_601_709,             /* YCbCr_601 LIMIT->YCbCr_709 LIMIT */
	RK_PQ_CSC_YUV2YUV,                     /* YCbCr LIMIT->YCbCr LIMIT */
	RK_PQ_CSC_YUV2RGB_601_FULL,            /* YCbCr_601 FULL-> RGB FULL */
	RK_PQ_CSC_YUV2RGB_709_FULL,            /* YCbCr_709 FULL-> RGB FULL */
	RK_PQ_CSC_RGB2YUV_601_FULL,            /* RGB FULL->YCbCr_601 FULL */
	RK_PQ_CSC_RGB2YUV_709_FULL,            /* RGB FULL->YCbCr_709 FULL */
	RK_PQ_CSC_YUV2YUV_709_601_FULL,        /* YCbCr_709 FULL->YCbCr_601 FULL */
	RK_PQ_CSC_YUV2YUV_601_709_FULL,        /* YCbCr_601 FULL->YCbCr_709 FULL */
	RK_PQ_CSC_YUV2YUV_FULL,                /* YCbCr FULL->YCbCr FULL */
	RK_PQ_CSC_YUV2YUV_LIMIT2FULL,          /* YCbCr  LIMIT->YCbCr  FULL */
	RK_PQ_CSC_YUV2YUV_601_709_LIMIT2FULL,  /* YCbCr 601 LIMIT->YCbCr 709 FULL */
	RK_PQ_CSC_YUV2YUV_709_601_LIMIT2FULL,  /* YCbCr 709 LIMIT->YCbCr 601 FULL */
	RK_PQ_CSC_YUV2YUV_FULL2LIMIT,          /* YCbCr  FULL->YCbCr  LIMIT */
	RK_PQ_CSC_YUV2YUV_601_709_FULL2LIMIT,  /* YCbCr 601 FULL->YCbCr 709 LIMIT */
	RK_PQ_CSC_YUV2YUV_709_601_FULL2LIMIT,  /* YCbCr 709 FULL->YCbCr 601 LIMIT */
	RK_PQ_CSC_YUV2RGBL_601,                /* YCbCr_601 LIMIT-> RGB LIMIT */
	RK_PQ_CSC_YUV2RGBL_709,                /* YCbCr_709 LIMIT-> RGB LIMIT */
	RK_PQ_CSC_RGBL2YUV_601,                /* RGB LIMIT->YCbCr_601 LIMIT */
	RK_PQ_CSC_RGBL2YUV_709,                /* RGB LIMIT->YCbCr_709 LIMIT */
	RK_PQ_CSC_YUV2RGBL_601_FULL,           /* YCbCr_601 FULL-> RGB LIMIT */
	RK_PQ_CSC_YUV2RGBL_709_FULL,           /* YCbCr_709 FULL-> RGB LIMIT */
	RK_PQ_CSC_RGBL2YUV_601_FULL,           /* RGB LIMIT->YCbCr_601 FULL */
	RK_PQ_CSC_RGBL2YUV_709_FULL,           /* RGB LIMIT->YCbCr_709 FULL */
	RK_PQ_CSC_RGB2RGBL,                    /* RGB FULL->RGB LIMIT */
	RK_PQ_CSC_RGBL2RGB,                    /* RGB LIMIT->RGB FULL */
	RK_PQ_CSC_RGBL2RGBL,                   /* RGB LIMIT->RGB LIMIT */
	RK_PQ_CSC_RGB2RGB,                     /* RGB FULL->RGB FULL */
	RK_PQ_CSC_YUV2RGB_2020,                /* YUV 2020 FULL->RGB  2020 FULL */
	RK_PQ_CSC_RGB2YUV2020_LIMIT2FULL,      /* BT2020RGBLIMIT -> BT2020YUVFULL */
	RK_PQ_CSC_RGB2YUV2020_LIMIT,           /* BT2020RGBLIMIT -> BT2020YUVLIMIT */
	RK_PQ_CSC_RGB2YUV2020_FULL2LIMIT,      /* BT2020RGBFULL -> BT2020YUVLIMIT */
	RK_PQ_CSC_RGB2YUV2020_FULL,            /* BT2020RGBFULL -> BT2020YUVFULL */
};

enum color_space_type {
	OPTM_CS_E_UNKNOWN = 0,
	OPTM_CS_E_ITU_R_BT_709 = 1,
	OPTM_CS_E_FCC = 4,
	OPTM_CS_E_ITU_R_BT_470_2_BG = 5,
	OPTM_CS_E_SMPTE_170_M = 6,
	OPTM_CS_E_SMPTE_240_M = 7,
	OPTM_CS_E_XV_YCC_709 = OPTM_CS_E_ITU_R_BT_709,
	OPTM_CS_E_XV_YCC_601 = 8,
	OPTM_CS_E_RGB = 9,
	OPTM_CS_E_XV_YCC_2020 = 10,
	OPTM_CS_E_RGB_2020 = 11,
};

struct rk_pq_csc_coef {
	s32 csc_coef00;
	s32 csc_coef01;
	s32 csc_coef02;
	s32 csc_coef10;
	s32 csc_coef11;
	s32 csc_coef12;
	s32 csc_coef20;
	s32 csc_coef21;
	s32 csc_coef22;
};

struct rk_pq_csc_ventor {
	s32 csc_offset0;
	s32 csc_offset1;
	s32 csc_offset2;
};

struct rk_pq_csc_dc_coef {
	s32 csc_in_dc0;
	s32 csc_in_dc1;
	s32 csc_in_dc2;
	s32 csc_out_dc0;
	s32 csc_out_dc1;
	s32 csc_out_dc2;
};

/* color space param */
struct rk_csc_colorspace_info {
	enum color_space_type input_color_space;
	enum color_space_type output_color_space;
	bool in_full_range;
	bool out_full_range;
};

struct rk_csc_mode_coef {
	enum rk_pq_csc_mode csc_mode;
	char c_csc_comment[PQ_CSC_MODE_COEF_COMMENT_LEN];
	const struct rk_pq_csc_coef *pst_csc_coef;
	const struct rk_pq_csc_dc_coef *pst_csc_dc_coef;
	struct rk_csc_colorspace_info st_csc_color_info;
};

/*
 *CSC matrix
 */
/* xv_ycc BT.601 limit(i.e. SD) -> RGB full */
static const struct rk_pq_csc_coef rk_csc_table_xv_yccsdy_cb_cr_limit_to_rgb_full = {
	1196, 0, 1639,
	1196, -402, -835,
	1196, 2072, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_xv_yccsdy_cb_cr_limit_to_rgb_full = {
	-64, -512, -512,
	0, 0, 0
};

/* BT.709 limit(i.e. HD) -> RGB full */
static const struct rk_pq_csc_coef rk_csc_table_hdy_cb_cr_limit_to_rgb_full = {
	1196, 0, 1841,
	1196, -219, -547,
	1196, 2169, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_hdy_cb_cr_limit_to_rgb_full = {
	-64, -512, -512,
	0, 0, 0
};

/* RGB full-> YUV601 (i.e. SD) limit */
static const struct rk_pq_csc_coef rk_csc_table_rgb_to_xv_yccsdy_cb_cr = {
	262, 515, 100,
	-151, -297, 448,
	448, -376, -73
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_rgb_to_xv_yccsdy_cb_cr = {
	0, 0, 0,
	64, 512, 512
};

/* RGB full-> YUV709 (i.e. SD) limit */
static const struct rk_pq_csc_coef rk_csc_table_rgb_to_hdy_cb_cr = {
	186, 627, 63,
	-103, -346, 448,
	448, -407, -41
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_rgb_to_hdy_cb_cr = {
	0, 0, 0,
	64, 512, 512
};

/* BT.709 (i.e. HD) -> to xv_ycc BT.601 (i.e. SD) */
static const struct rk_pq_csc_coef rk_csc_table_hdy_cb_cr_to_xv_yccsdy_cb_cr = {
	1024, 104, 201,
	0, 1014, -113,
	0, -74, 1007
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_hdy_cb_cr_to_xv_yccsdy_cb_cr = {
	-64, -512, -512,
	64, 512, 512
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_hdy_cb_cr_full_to_xv_yccsdy_cb_cr_full = {
	0, -512, -512,
	0, 512, 512
};

/* xv_ycc BT.601 (i.e. SD) -> to BT.709 (i.e. HD) */
static const struct rk_pq_csc_coef rk_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr = {
	1024, -121, -218,
	0, 1043, 117,
	0, 77, 1050
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr = {
	-64, -512, -512,
	64, 512, 512
};

/* xv_ycc BT.601 full(i.e. SD) -> RGB full */
static const struct rk_pq_csc_coef rk_csc_table_xv_yccsdy_cb_cr_to_rgb_full = {
	1024, 0, 1436,
	1024, -352, -731,
	1024, 1815, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_xv_yccsdy_cb_cr_to_rgb_full = {
	0, -512, -512,
	0, 0, 0
};

/* BT.709 full(i.e. HD) -> RGB full */
static const struct rk_pq_csc_coef rk_csc_table_hdy_cb_cr_to_rgb_full = {
	1024, 0, 1613,
	1024, -192, -479,
	1024, 1900, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_hdy_cb_cr_to_rgb_full = {
	0, -512, -512,
	0, 0, 0
};

/* RGB full-> YUV601 full(i.e. SD) */
static const struct rk_pq_csc_coef rk_csc_table_rgb_to_xv_yccsdy_cb_cr_full = {
	306, 601, 117,
	-173, -339, 512,
	512, -429, -83
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_rgb_to_xv_yccsdy_cb_cr_full = {
	0, 0, 0,
	0, 512, 512
};

/* RGB full-> YUV709 full (i.e. SD) */
static const struct rk_pq_csc_coef rk_csc_table_rgb_to_hdy_cb_cr_full = {
	218, 732, 74,
	-117, -395, 512,
	512, -465, -47
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_rgb_to_hdy_cb_cr_full = {
	0, 0, 0,
	0, 512, 512
};

/* limit -> full */
static const struct rk_pq_csc_coef rk_csc_table_identity_y_cb_cr_limit_to_y_cb_cr_full = {
	1196, 0, 0,
	0, 1169, 0,
	0, 0, 1169
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_y_cb_cr_limit_to_y_cb_cr_full = {
	-64, -512, -512,
	0, 512, 512
};

/* 601 limit -> 709 full */
static const struct rk_pq_csc_coef rk_csc_table_identity_601_limit_to_709_full = {
	1196, -138, -249,
	0, 1191, 134,
	0, 88, 1199
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_601_limit_to_709_full = {
	-64, -512, -512,
	0, 512, 512
};

/* 709 limit -> 601 full */
static const struct rk_pq_csc_coef rk_csc_table_identity_709_limit_to_601_full = {
	1196, 119, 229,
	0, 1157, -129,
	0, -85, 1150
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_709_limit_to_601_full = {
	-64, -512, -512,
	0, 512, 512
};

/* full ->   limit */
static const struct rk_pq_csc_coef rk_csc_table_identity_y_cb_cr_full_to_y_cb_cr_limit = {
	877, 0, 0,
	0, 897, 0,
	0, 0, 897
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_y_cb_cr_full_to_y_cb_cr_limit = {
	0, -512, -512,
	64, 512, 512
};

/* 601 full ->  709 limit */
static const struct rk_pq_csc_coef rk_csc_table_identity_y_cb_cr_601_full_to_y_cb_cr_709_limit = {
	877, -106, -191,
	0, 914, 103,
	0, 67, 920
};

static const struct rk_pq_csc_dc_coef
rk_dc_csc_table_identity_y_cb_cr_601_full_to_y_cb_cr_709_limit = {
	0, -512, -512,
	64, 512, 512
};

/* 709 full ->  601 limit */
static const struct rk_pq_csc_coef rk_csc_table_identity_y_cb_cr_709_full_to_y_cb_cr_601_limit = {
	877, 91, 176,
	0, 888, -99,
	0, -65, 882
};

static const struct rk_pq_csc_dc_coef
rk_dc_csc_table_identity_y_cb_cr_709_full_to_y_cb_cr_601_limit = {
	0, -512, -512,
	64, 512, 512
};

/* xv_ycc BT.601 limit(i.e. SD) -> RGB limit */
static const struct rk_pq_csc_coef rk_csc_table_xv_yccsdy_cb_cr_limit_to_rgb_limit = {
	1024, 0, 1404,
	1024, -344, -715,
	1024, 1774, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_xv_yccsdy_cb_cr_limit_to_rgb_limit = {
	-64, -512, -512,
	64, 64, 64
};

/* BT.709 limit(i.e. HD) -> RGB limit */
static const struct rk_pq_csc_coef rk_csc_table_hdy_cb_cr_limit_to_rgb_limit = {
	1024, 0, 1577,
	1024, -188, -469,
	1024, 1858, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_hdy_cb_cr_limit_to_rgb_limit = {
	-64, -512, -512,
	64, 64, 64
};

/* RGB limit-> YUV601 (i.e. SD) limit */
static const struct rk_pq_csc_coef rk_csc_table_rgb_limit_to_xv_yccsdy_cb_cr = {
	306, 601, 117,
	-177, -347, 524,
	524, -439, -85
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_rgb_limit_to_xv_yccsdy_cb_cr = {
	-64, -64, -64,
	64, 512, 512
};

/* RGB limit -> YUV709 (i.e. SD) limit */
static const struct rk_pq_csc_coef rk_csc_table_rgb_limit_to_hdy_cb_cr = {
	218, 732, 74,
	-120, -404, 524,
	524, -476, -48
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_rgb_limit_to_hdy_cb_cr = {
	-64, -64, -64,
	64, 512, 512
};

/* xv_ycc BT.601 full(i.e. SD) -> RGB limit */
static const struct rk_pq_csc_coef rk_csc_table_xv_yccsdy_cb_cr_to_rgb_limit = {
	877, 0, 1229,
	877, -302, -626,
	877, 1554, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_xv_yccsdy_cb_cr_to_rgb_limit = {
	0, -512, -512,
	64, 64, 64
};

/* BT.709 full(i.e. HD) -> RGB limit */
static const struct rk_pq_csc_coef rk_csc_table_hdy_cb_cr_to_rgb_limit = {
	877, 0, 1381,
	877, -164, -410,
	877, 1627, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_hdy_cb_cr_to_rgb_limit = {
	0, -512, -512,
	64, 64, 64
};

/* RGB limit-> YUV601 full(i.e. SD) */
static const struct rk_pq_csc_coef rk_csc_table_rgb_limit_to_xv_yccsdy_cb_cr_full = {
	358, 702, 136,
	-202, -396, 598,
	598, -501, -97
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_rgb_limit_to_xv_yccsdy_cb_cr_full = {
	-64, -64, -64,
	0, 512, 512
};

/* RGB limit-> YUV709 full (i.e. SD) */
static const struct rk_pq_csc_coef rk_csc_table_rgb_limit_to_hdy_cb_cr_full = {
	254, 855, 86,
	-137, -461, 598,
	598, -543, -55
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_rgb_limit_to_hdy_cb_cr_full = {
	-64, -64, -64,
	0, 512, 512
};

/* RGB full -> RGB limit */
static const struct rk_pq_csc_coef rk_csc_table_identity_rgb_to_rgb_limit = {
	877, 0, 0,
	0, 877, 0,
	0, 0, 877
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_rgb_to_rgb_limit = {
	0, 0, 0,
	64, 64, 64
};

/* RGB limit -> RGB full */
static const struct rk_pq_csc_coef rk_csc_table_identity_rgb_limit_to_rgb = {
	1196, 0, 0,
	0, 1196, 0,
	0, 0, 1196
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_rgb_limit_to_rgb = {
	-64, -64, -64,
	0, 0, 0
};

/* RGB limit/full -> RGB limit/full */
static const struct rk_pq_csc_coef rk_csc_table_identity_rgb_to_rgb = {
	1024, 0, 0,
	0, 1024, 0,
	0, 0, 1024
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_rgb_to_rgb1 = {
	-64, -64, -64,
	64, 64, 64
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_rgb_to_rgb2 = {
	0, 0, 0,
	0, 0, 0
};

static const struct rk_pq_csc_coef rk_csc_table_identity_yuv_to_rgb_2020 = {
	1024, 0, 1510,
	1024, -169, -585,
	1024, 1927, 0
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_yuv_to_rgb_2020 = {
	0, -512, -512,
	0, 0, 0
};

/* 2020 RGB LIMIT ->YUV LIMIT */
static const struct rk_pq_csc_coef rk_csc_table_identity_rgb_limit_to_yuv_limit_2020 = {
	269, 694, 61,
	-146, -377, 524,
	524, -482, -42
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_rgb_limit_to_yuv_limit_2020 = {
	-64, -64, -64,
	64, 512, 512
};

/* 2020 RGB LIMIT ->YUV FULL */
static const struct rk_pq_csc_coef rk_csc_table_identity_rgb_limit_to_yuv_full_2020 = {
	314, 811, 71,
	-167, -431, 598,
	598, -550, -48
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_rgb_limit_to_yuv_full_2020 = {
	-64, -64, -64,
	0, 512, 512
};

/* 2020 RGB FULL ->YUV LIMIT */
static const struct rk_pq_csc_coef rk_csc_table_identity_rgb_full_to_yuv_limit_2020 = {
	230, 595, 52,
	-125, -323, 448,
	448, -412, -36
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_rgb_full_to_yuv_limit_2020 = {
	0, 0, 0,
	64, 512, 512
};

/* 2020 RGB FULL ->YUV FULL */
static const struct rk_pq_csc_coef rk_csc_table_identity_rgb_full_to_yuv_full_2020 = {
	269, 694, 61,
	-143, -369, 512,
	512, -471, -41
};

static const struct rk_pq_csc_dc_coef rk_dc_csc_table_identity_rgb_full_to_yuv_full_2020 = {
	0, 0, 0,
	0, 512, 512
};

/* identity matrix */
static const struct rk_pq_csc_coef rk_csc_table_identity_y_cb_cr_to_y_cb_cr = {
	1024, 0, 0,
	0, 1024, 0,
	0, 0, 1024
};

/*
 *CSC Param Struct
 */
static const struct rk_csc_mode_coef g_mode_csc_coef[] = {
	{
		RK_PQ_CSC_YUV2RGB_601, "YUV601 L->RGB F",
		&rk_csc_table_xv_yccsdy_cb_cr_limit_to_rgb_full,
		&rk_dc_csc_table_xv_yccsdy_cb_cr_limit_to_rgb_full,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_RGB, false, true
		}
	},
	{
		RK_PQ_CSC_YUV2RGB_709, "YUV709 L->RGB F",
		&rk_csc_table_hdy_cb_cr_limit_to_rgb_full,
		&rk_dc_csc_table_hdy_cb_cr_limit_to_rgb_full,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_RGB, false, true
		}
	},
	{
		RK_PQ_CSC_RGB2YUV_601, "RGB F->YUV601 L",
		&rk_csc_table_rgb_to_xv_yccsdy_cb_cr,
		&rk_dc_csc_table_rgb_to_xv_yccsdy_cb_cr,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_601, true, false
		}
	},
	{
		RK_PQ_CSC_RGB2YUV_709, "RGB F->YUV709 L",
		&rk_csc_table_rgb_to_hdy_cb_cr,
		&rk_dc_csc_table_rgb_to_hdy_cb_cr,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_ITU_R_BT_709, true, false
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_709_601, "YUV709 L->YUV601 L",
		&rk_csc_table_hdy_cb_cr_to_xv_yccsdy_cb_cr,
		&rk_dc_csc_table_hdy_cb_cr_to_xv_yccsdy_cb_cr,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_XV_YCC_601, false, false
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_601_709, "YUV601 L->YUV709 L",
		&rk_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		&rk_dc_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_ITU_R_BT_709, false, false
		}
	},
	{
		RK_PQ_CSC_YUV2YUV, "YUV L->YUV L",
		&rk_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		&rk_dc_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_ITU_R_BT_709, false, false
		}
	},
	{
		RK_PQ_CSC_YUV2RGB_601_FULL, "YUV601 F->RGB F",
		&rk_csc_table_xv_yccsdy_cb_cr_to_rgb_full,
		&rk_dc_csc_table_xv_yccsdy_cb_cr_to_rgb_full,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_RGB, true, true
		}
	},
		{
		RK_PQ_CSC_YUV2RGB_709_FULL, "YUV709 F->RGB F",
		&rk_csc_table_hdy_cb_cr_to_rgb_full,
		&rk_dc_csc_table_hdy_cb_cr_to_rgb_full,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_RGB, true, true
		}
	},
	{
		RK_PQ_CSC_RGB2YUV_601_FULL, "RGB F->YUV601 F",
		&rk_csc_table_rgb_to_xv_yccsdy_cb_cr_full,
		&rk_dc_csc_table_rgb_to_xv_yccsdy_cb_cr_full,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_601, true, true
		}
	},
	{
		RK_PQ_CSC_RGB2YUV_709_FULL, "RGB F->YUV709 F",
		&rk_csc_table_rgb_to_hdy_cb_cr_full,
		&rk_dc_csc_table_rgb_to_hdy_cb_cr_full,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_ITU_R_BT_709, true, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_709_601_FULL, "YUV709 F->YUV601 F",
		&rk_csc_table_hdy_cb_cr_to_xv_yccsdy_cb_cr,
		&rk_dc_csc_table_hdy_cb_cr_full_to_xv_yccsdy_cb_cr_full,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_XV_YCC_601, true, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_601_709_FULL, "YUV601 F->YUV709 F",
		&rk_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		&rk_dc_csc_table_hdy_cb_cr_full_to_xv_yccsdy_cb_cr_full,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_ITU_R_BT_709, true, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_FULL, "YUV F->YUV F",
		&rk_csc_table_identity_y_cb_cr_to_y_cb_cr,
		&rk_dc_csc_table_hdy_cb_cr_full_to_xv_yccsdy_cb_cr_full,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_ITU_R_BT_709, true, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_LIMIT2FULL, "YUV L->YUV F",
		&rk_csc_table_identity_y_cb_cr_limit_to_y_cb_cr_full,
		&rk_dc_csc_table_identity_y_cb_cr_limit_to_y_cb_cr_full,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_ITU_R_BT_709, false, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_601_709_LIMIT2FULL, "YUV601 L->YUV709 F",
		&rk_csc_table_identity_601_limit_to_709_full,
		&rk_dc_csc_table_identity_601_limit_to_709_full,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_ITU_R_BT_709, false, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_709_601_LIMIT2FULL, "YUV709 L->YUV601 F",
		&rk_csc_table_identity_709_limit_to_601_full,
		&rk_dc_csc_table_identity_709_limit_to_601_full,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_XV_YCC_601, false, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_FULL2LIMIT, "YUV F->YUV L",
		&rk_csc_table_identity_y_cb_cr_full_to_y_cb_cr_limit,
		&rk_dc_csc_table_identity_y_cb_cr_full_to_y_cb_cr_limit,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_ITU_R_BT_709, true, false
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_601_709_FULL2LIMIT, "YUV601 F->YUV709 L",
		&rk_csc_table_identity_y_cb_cr_601_full_to_y_cb_cr_709_limit,
		&rk_dc_csc_table_identity_y_cb_cr_601_full_to_y_cb_cr_709_limit,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_ITU_R_BT_709, true, false
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_709_601_FULL2LIMIT, "YUV709 F->YUV601 L",
		&rk_csc_table_identity_y_cb_cr_709_full_to_y_cb_cr_601_limit,
		&rk_dc_csc_table_identity_y_cb_cr_709_full_to_y_cb_cr_601_limit,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_XV_YCC_601, true, false
		}
	},
	{
		RK_PQ_CSC_YUV2RGBL_601, "YUV601 L->RGB L",
		&rk_csc_table_xv_yccsdy_cb_cr_limit_to_rgb_limit,
		&rk_dc_csc_table_xv_yccsdy_cb_cr_limit_to_rgb_limit,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_RGB, false, false
		}
	},
	{
		RK_PQ_CSC_YUV2RGBL_709, "YUV709 L->RGB L",
		&rk_csc_table_hdy_cb_cr_limit_to_rgb_limit,
		&rk_dc_csc_table_hdy_cb_cr_limit_to_rgb_limit,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_RGB, false, false
		}
	},
	{
		RK_PQ_CSC_RGBL2YUV_601, "RGB L->YUV601 L",
		&rk_csc_table_rgb_limit_to_xv_yccsdy_cb_cr,
		&rk_dc_csc_table_rgb_limit_to_xv_yccsdy_cb_cr,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_601, false, false
		}
	},
	{
		RK_PQ_CSC_RGBL2YUV_709, "RGB L->YUV709 L",
		&rk_csc_table_rgb_limit_to_hdy_cb_cr,
		&rk_dc_csc_table_rgb_limit_to_hdy_cb_cr,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_ITU_R_BT_709, false, false
		}
	},
	{
		RK_PQ_CSC_YUV2RGBL_601_FULL, "YUV601 F->RGB L",
		&rk_csc_table_xv_yccsdy_cb_cr_to_rgb_limit,
		&rk_dc_csc_table_xv_yccsdy_cb_cr_to_rgb_limit,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_RGB, true, false
		}
	},
	{
		RK_PQ_CSC_YUV2RGBL_709_FULL, "YUV709 F->RGB L",
		&rk_csc_table_hdy_cb_cr_to_rgb_limit,
		&rk_dc_csc_table_hdy_cb_cr_to_rgb_limit,
		{
			OPTM_CS_E_ITU_R_BT_709, OPTM_CS_E_RGB, true, false
		}
	},
	{
		RK_PQ_CSC_RGBL2YUV_601_FULL, "RGB L->YUV601 F",
		&rk_csc_table_rgb_limit_to_xv_yccsdy_cb_cr_full,
		&rk_dc_csc_table_rgb_limit_to_xv_yccsdy_cb_cr_full,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_601, false, true
		}
	},
	{
		RK_PQ_CSC_RGBL2YUV_709_FULL, "RGB L->YUV709 F",
		&rk_csc_table_rgb_limit_to_hdy_cb_cr_full,
		&rk_dc_csc_table_rgb_limit_to_hdy_cb_cr_full,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_ITU_R_BT_709, false, true
		}
	},
	{
		RK_PQ_CSC_RGB2RGBL, "RGB F->RGB L",
		&rk_csc_table_identity_rgb_to_rgb_limit,
		&rk_dc_csc_table_identity_rgb_to_rgb_limit,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_RGB, true, false
		}
	},
	{
		RK_PQ_CSC_RGBL2RGB, "RGB L->RGB F",
		&rk_csc_table_identity_rgb_limit_to_rgb,
		&rk_dc_csc_table_identity_rgb_limit_to_rgb,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_RGB, false, true
		}
	},
	{
		RK_PQ_CSC_RGBL2RGBL, "RGB L->RGB L",
		&rk_csc_table_identity_rgb_to_rgb,
		&rk_dc_csc_table_identity_rgb_to_rgb1,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_RGB, false, false
		}
	},
	{
		RK_PQ_CSC_RGB2RGB, "RGB F->RGB F",
		&rk_csc_table_identity_rgb_to_rgb,
		&rk_dc_csc_table_identity_rgb_to_rgb2,
		{
			OPTM_CS_E_RGB, OPTM_CS_E_RGB, true, true
		}
	},
	{
		RK_PQ_CSC_YUV2RGB_2020, "YUV2020 F->RGB2020 F",
		&rk_csc_table_identity_yuv_to_rgb_2020,
		&rk_dc_csc_table_identity_yuv_to_rgb_2020,
		{
			OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_RGB_2020, true, true
		}
	},
	{
		RK_PQ_CSC_RGB2YUV2020_LIMIT2FULL, "RGB2020 L->YUV2020 F",
		&rk_csc_table_identity_rgb_limit_to_yuv_full_2020,
		&rk_dc_csc_table_identity_rgb_limit_to_yuv_full_2020,
		{
			OPTM_CS_E_RGB_2020, OPTM_CS_E_XV_YCC_2020, false, true
		}
	},
	{
		RK_PQ_CSC_RGB2YUV2020_LIMIT, "RGB2020 L->YUV2020 L",
		&rk_csc_table_identity_rgb_limit_to_yuv_limit_2020,
		&rk_dc_csc_table_identity_rgb_limit_to_yuv_limit_2020,
		{
			OPTM_CS_E_RGB_2020, OPTM_CS_E_XV_YCC_2020, false, false
		}
	},
	{
		RK_PQ_CSC_RGB2YUV2020_FULL2LIMIT, "RGB2020 F->YUV2020 L",
		&rk_csc_table_identity_rgb_full_to_yuv_limit_2020,
		&rk_dc_csc_table_identity_rgb_full_to_yuv_limit_2020,
		{
			OPTM_CS_E_RGB_2020, OPTM_CS_E_XV_YCC_2020, true, false
		}
	},
	{
		RK_PQ_CSC_RGB2YUV2020_FULL, "RGB2020 F->YUV2020 F",
		&rk_csc_table_identity_rgb_full_to_yuv_full_2020,
		&rk_dc_csc_table_identity_rgb_full_to_yuv_full_2020,
		{
			OPTM_CS_E_RGB_2020, OPTM_CS_E_XV_YCC_2020, true, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV, "YUV 601 L->YUV 601 L",
		&rk_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		&rk_dc_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_601, false, false
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_FULL, "YUV 601 F->YUV 601 F",
		&rk_csc_table_identity_y_cb_cr_to_y_cb_cr,
		&rk_dc_csc_table_hdy_cb_cr_full_to_xv_yccsdy_cb_cr_full,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_601, true, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_LIMIT2FULL, "YUV 601 L->YUV 601 F",
		&rk_csc_table_identity_y_cb_cr_limit_to_y_cb_cr_full,
		&rk_dc_csc_table_identity_y_cb_cr_limit_to_y_cb_cr_full,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_601,  false, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_FULL2LIMIT, "YUV 601 F->YUV 601 L",
		&rk_csc_table_identity_y_cb_cr_full_to_y_cb_cr_limit,
		&rk_dc_csc_table_identity_y_cb_cr_full_to_y_cb_cr_limit,
		{
			OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_601, true, false
		}
	},
	{
		RK_PQ_CSC_YUV2YUV, "YUV 2020 L->YUV 2020 L",
		&rk_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		&rk_dc_csc_table_xv_yccsdy_cb_cr_to_hdy_cb_cr,
		{
			OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_XV_YCC_2020, false, false
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_FULL, "YUV 2020 F->YUV 2020 F",
		&rk_csc_table_identity_y_cb_cr_to_y_cb_cr,
		&rk_dc_csc_table_hdy_cb_cr_full_to_xv_yccsdy_cb_cr_full,
		{
			OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_XV_YCC_2020, true, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_LIMIT2FULL, "YUV 2020 L->YUV 2020 F",
		&rk_csc_table_identity_y_cb_cr_limit_to_y_cb_cr_full,
		&rk_dc_csc_table_identity_y_cb_cr_limit_to_y_cb_cr_full,
		{
			OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_XV_YCC_2020, false, true
		}
	},
	{
		RK_PQ_CSC_YUV2YUV_FULL2LIMIT, "YUV 2020 F->YUV 2020 L",
		&rk_csc_table_identity_y_cb_cr_full_to_y_cb_cr_limit,
		&rk_dc_csc_table_identity_y_cb_cr_full_to_y_cb_cr_limit,
		{
			OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_XV_YCC_2020, true, false
		}
	},
	{
		RK_PQ_CSC_RGB2RGBL, "RGB 2020 F->RGB 2020 L",
		&rk_csc_table_identity_rgb_to_rgb_limit,
		&rk_dc_csc_table_identity_rgb_to_rgb_limit,
		{
			OPTM_CS_E_RGB_2020, OPTM_CS_E_RGB_2020, true, false
		}
	},
	{
		RK_PQ_CSC_RGBL2RGB, "RGB 2020 L->RGB 2020 F",
		&rk_csc_table_identity_rgb_limit_to_rgb,
		&rk_dc_csc_table_identity_rgb_limit_to_rgb,
		{
			OPTM_CS_E_RGB_2020, OPTM_CS_E_RGB_2020, false, true
		}
	},
	{
		RK_PQ_CSC_RGBL2RGBL, "RGB 2020 L->RGB 2020 L",
		&rk_csc_table_identity_rgb_to_rgb,
		&rk_dc_csc_table_identity_rgb_to_rgb1,
		{
			OPTM_CS_E_RGB_2020, OPTM_CS_E_RGB_2020, false, false
		}
	},
	{
		RK_PQ_CSC_RGB2RGB, "RGB 2020 F->RGB 2020 F",
		&rk_csc_table_identity_rgb_to_rgb,
		&rk_dc_csc_table_identity_rgb_to_rgb2,
		{
			OPTM_CS_E_RGB_2020, OPTM_CS_E_RGB_2020, true, true
		}
	},
};

enum vop_csc_format {
	CSC_BT601L,
	CSC_BT709L,
	CSC_BT601F,
	CSC_BT2020,
	CSC_BT709L_13BIT,
	CSC_BT709F_13BIT,
	CSC_BT2020L_13BIT,
	CSC_BT2020F_13BIT,
};

static const char * const vop_csc_format_str[] = {
	"BT601L",
	"BT709L",
	"BT601F",
	"BT2020",
	"BT709L_13BIT",
	"BT709F_13BIT",
	"BT2020L_13BIT",
	"BT2020F_13BIT",
};

enum vop_csc_mode {
	CSC_RGB,
	CSC_YUV,
};

struct csc_mapping {
	enum vop_csc_format csc_format;
	enum color_space_type rgb_color_space;
	enum color_space_type yuv_color_space;
	bool rgb_full_range;
	bool yuv_full_range;
};

static const struct csc_mapping csc_mapping_table[] = {
	{
		CSC_BT601L,
		OPTM_CS_E_RGB,
		OPTM_CS_E_XV_YCC_601,
		true,
		false,
	},
	{
		CSC_BT709L,
		OPTM_CS_E_RGB,
		OPTM_CS_E_XV_YCC_709,
		true,
		false,
	},
	{
		CSC_BT601F,
		OPTM_CS_E_RGB,
		OPTM_CS_E_XV_YCC_601,
		true,
		true,
	},
	{
		CSC_BT2020,
		OPTM_CS_E_RGB_2020,
		OPTM_CS_E_XV_YCC_2020,
		true,
		true,
	},
	{
		CSC_BT709L_13BIT,
		OPTM_CS_E_RGB,
		OPTM_CS_E_XV_YCC_709,
		true,
		false,
	},
	{
		CSC_BT709F_13BIT,
		OPTM_CS_E_RGB,
		OPTM_CS_E_XV_YCC_709,
		true,
		true,
	},
	{
		CSC_BT2020L_13BIT,
		OPTM_CS_E_RGB_2020,
		OPTM_CS_E_XV_YCC_2020,
		true,
		false,
	},
	{
		CSC_BT2020F_13BIT,
		OPTM_CS_E_RGB_2020,
		OPTM_CS_E_XV_YCC_2020,
		true,
		true,
	},
};

static bool is_rgb_format(u64 format)
{
	switch (format) {
	case BUS_FMT_YUV420:
	case BUS_FMT_YUV422:
	case BUS_FMT_YUV444:
		return false;
	case BUS_FMT_RGB:
	default:
		return true;
	}
}

struct post_csc_coef {
	s32 csc_coef00;
	s32 csc_coef01;
	s32 csc_coef02;
	s32 csc_coef10;
	s32 csc_coef11;
	s32 csc_coef12;
	s32 csc_coef20;
	s32 csc_coef21;
	s32 csc_coef22;

	s32 csc_dc0;
	s32 csc_dc1;
	s32 csc_dc2;

	u32 range_type;
};

static int csc_get_mode_index(int post_csc_mode, bool is_input_yuv, bool is_output_yuv)
{
	const struct rk_csc_colorspace_info *colorspace_info;
	enum color_space_type input_color_space;
	enum color_space_type output_color_space;
	bool is_input_full_range;
	bool is_output_full_range;
	int i;

	for (i = 0; i < ARRAY_SIZE(csc_mapping_table); i++) {
		if (post_csc_mode == csc_mapping_table[i].csc_format) {
			input_color_space = is_input_yuv ? csc_mapping_table[i].yuv_color_space :
					    csc_mapping_table[i].rgb_color_space;
			is_input_full_range = is_input_yuv ? csc_mapping_table[i].yuv_full_range :
					      csc_mapping_table[i].rgb_full_range;
			output_color_space = is_output_yuv ? csc_mapping_table[i].yuv_color_space :
					     csc_mapping_table[i].rgb_color_space;
			is_output_full_range = is_output_yuv ? csc_mapping_table[i].yuv_full_range :
					       csc_mapping_table[i].rgb_full_range;
			break;
		}
	}
	if (i >= ARRAY_SIZE(csc_mapping_table))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(g_mode_csc_coef); i++) {
		colorspace_info = &g_mode_csc_coef[i].st_csc_color_info;
		if (colorspace_info->input_color_space == input_color_space &&
		    colorspace_info->output_color_space == output_color_space &&
		    colorspace_info->in_full_range == is_input_full_range &&
		    colorspace_info->out_full_range == is_output_full_range)
			return i;
	}

	return -EINVAL;
}

static void csc_matrix_ventor_multiply(struct rk_pq_csc_ventor *dst,
				       const struct rk_pq_csc_coef *m0,
				       const struct rk_pq_csc_ventor *v0)
{
	dst->csc_offset0 = m0->csc_coef00 * v0->csc_offset0 +
			   m0->csc_coef01 * v0->csc_offset1 +
			   m0->csc_coef02 * v0->csc_offset2;

	dst->csc_offset1 = m0->csc_coef10 * v0->csc_offset0 +
			   m0->csc_coef11 * v0->csc_offset1 +
			   m0->csc_coef12 * v0->csc_offset2;

	dst->csc_offset2 = m0->csc_coef20 * v0->csc_offset0 +
			   m0->csc_coef21 * v0->csc_offset1 +
			   m0->csc_coef22 * v0->csc_offset2;
}

static inline s32 pq_csc_simple_round(s32 x, s32 n)
{
	s32 value = 0;

	if (n == 0)
		return x;

	value = (abs(x) + (1 << (n - 1))) >> (n);
	return (((x) >= 0) ? value : -value);
}

static int csc_calc_default_output_coef(const struct rk_csc_mode_coef *csc_mode_cfg,
					struct rk_pq_csc_coef *out_matrix,
					struct rk_pq_csc_ventor *out_dc)
{
	const struct rk_pq_csc_coef *csc_coef;
	const struct rk_pq_csc_dc_coef *csc_dc_coef;
	struct rk_pq_csc_ventor dc_in_ventor;
	struct rk_pq_csc_ventor dc_out_ventor;
	struct rk_pq_csc_ventor v;

	csc_coef = csc_mode_cfg->pst_csc_coef;
	csc_dc_coef = csc_mode_cfg->pst_csc_dc_coef;

	out_matrix->csc_coef00 = csc_coef->csc_coef00;
	out_matrix->csc_coef01 = csc_coef->csc_coef01;
	out_matrix->csc_coef02 = csc_coef->csc_coef02;
	out_matrix->csc_coef10 = csc_coef->csc_coef10;
	out_matrix->csc_coef11 = csc_coef->csc_coef11;
	out_matrix->csc_coef12 = csc_coef->csc_coef12;
	out_matrix->csc_coef20 = csc_coef->csc_coef20;
	out_matrix->csc_coef21 = csc_coef->csc_coef21;
	out_matrix->csc_coef22 = csc_coef->csc_coef22;

	dc_in_ventor.csc_offset0 = csc_dc_coef->csc_in_dc0;
	dc_in_ventor.csc_offset1 = csc_dc_coef->csc_in_dc1;
	dc_in_ventor.csc_offset2 = csc_dc_coef->csc_in_dc2;
	dc_out_ventor.csc_offset0 = csc_dc_coef->csc_out_dc0;
	dc_out_ventor.csc_offset1 = csc_dc_coef->csc_out_dc1;
	dc_out_ventor.csc_offset2 = csc_dc_coef->csc_out_dc2;

	csc_matrix_ventor_multiply(&v, csc_coef, &dc_in_ventor);
	out_dc->csc_offset0 = v.csc_offset0 + dc_out_ventor.csc_offset0 *
			      PQ_CSC_SIMPLE_MAT_PARAM_FIX_NUM;
	out_dc->csc_offset1 = v.csc_offset1 + dc_out_ventor.csc_offset1 *
			      PQ_CSC_SIMPLE_MAT_PARAM_FIX_NUM;
	out_dc->csc_offset2 = v.csc_offset2 + dc_out_ventor.csc_offset2 *
			      PQ_CSC_SIMPLE_MAT_PARAM_FIX_NUM;

	return 0;
}

static int vop2_convert_csc_mode(int csc_mode, int bit_depth)
{
	switch (csc_mode) {
	case V4L2_COLORSPACE_SMPTE170M:
	case V4L2_COLORSPACE_470_SYSTEM_M:
	case V4L2_COLORSPACE_470_SYSTEM_BG:
		return CSC_BT601L;
	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_SMPTE240M:
	case V4L2_COLORSPACE_DEFAULT:
		if (bit_depth == CSC_13BIT_DEPTH)
			return CSC_BT709L_13BIT;
		else
			return CSC_BT709L;
	case V4L2_COLORSPACE_JPEG:
		return CSC_BT601F;
	case V4L2_COLORSPACE_BT2020:
		if (bit_depth == CSC_13BIT_DEPTH)
			return CSC_BT2020L_13BIT;
		else
			return CSC_BT2020;
	case V4L2_COLORSPACE_BT709F:
		if (bit_depth == CSC_10BIT_DEPTH)
			return CSC_BT601F;
		else
			return CSC_BT709F_13BIT;
	case V4L2_COLORSPACE_BT2020F:
		if (bit_depth == CSC_10BIT_DEPTH)
			return CSC_BT601F;
		else
			return CSC_BT2020F_13BIT;
	default:
		return CSC_BT709L;
	}
}

static int rockchip_calc_post_csc(struct post_csc_coef *csc_simple_coef,
				  int csc_mode, bool is_input_yuv, bool is_output_yuv)
{
	int ret = 0;
	struct rk_pq_csc_coef out_matrix;
	struct rk_pq_csc_ventor out_dc;
	const struct rk_csc_mode_coef *csc_mode_cfg;
	int bit_num = PQ_CSC_SIMPLE_MAT_PARAM_FIX_BIT_WIDTH;

	ret = csc_get_mode_index(csc_mode, is_input_yuv, is_output_yuv);
	if (ret < 0)
		return ret;

	csc_mode_cfg = &g_mode_csc_coef[ret];

	ret = csc_calc_default_output_coef(csc_mode_cfg, &out_matrix, &out_dc);

	csc_simple_coef->csc_coef00 = out_matrix.csc_coef00;
	csc_simple_coef->csc_coef01 = out_matrix.csc_coef01;
	csc_simple_coef->csc_coef02 = out_matrix.csc_coef02;
	csc_simple_coef->csc_coef10 = out_matrix.csc_coef10;
	csc_simple_coef->csc_coef11 = out_matrix.csc_coef11;
	csc_simple_coef->csc_coef12 = out_matrix.csc_coef12;
	csc_simple_coef->csc_coef20 = out_matrix.csc_coef20;
	csc_simple_coef->csc_coef21 = out_matrix.csc_coef21;
	csc_simple_coef->csc_coef22 = out_matrix.csc_coef22;
	csc_simple_coef->csc_dc0 = out_dc.csc_offset0;
	csc_simple_coef->csc_dc1 = out_dc.csc_offset1;
	csc_simple_coef->csc_dc2 = out_dc.csc_offset2;

	csc_simple_coef->csc_dc0 = pq_csc_simple_round(csc_simple_coef->csc_dc0, bit_num);
	csc_simple_coef->csc_dc1 = pq_csc_simple_round(csc_simple_coef->csc_dc1, bit_num);
	csc_simple_coef->csc_dc2 = pq_csc_simple_round(csc_simple_coef->csc_dc2, bit_num);
	csc_simple_coef->range_type = csc_mode_cfg->st_csc_color_info.out_full_range;

	return ret;
}

static void calc_dsp_frm_hst_vst(const struct rk628_display_mode *src,
				 const struct rk628_display_mode *dst,
				 u32 *dsp_frame_hst,
				 u32 *dsp_frame_vst)
{
	u32 bp_in, bp_out;
	u32 v_scale_ratio;
	u64 t_frm_st;
	u64 t_bp_in, t_bp_out, t_delta, tin;
	u32 src_pixclock, dst_pixclock;
	u32 dst_htotal, dst_hsync_len, dst_hback_porch;
	u32 dst_vsync_len, dst_vback_porch, dst_vactive;
	u32 src_htotal, src_hsync_len, src_hback_porch;
	u32 src_vtotal, src_vsync_len, src_vback_porch, src_vactive;
	u32 rem;
	u32 x;

	src_pixclock = div_u64(1000000000llu, src->clock);
	dst_pixclock = div_u64(1000000000llu, dst->clock);

	src_hsync_len = src->hsync_end - src->hsync_start;
	src_hback_porch = src->htotal - src->hsync_end;
	src_htotal = src->htotal;
	src_vsync_len = src->vsync_end - src->vsync_start;
	src_vback_porch = src->vtotal - src->vsync_end;
	src_vactive = src->vdisplay;
	src_vtotal = src->vtotal;

	dst_hsync_len = dst->hsync_end - dst->hsync_start;
	dst_hback_porch = dst->htotal - dst->hsync_end;
	dst_htotal = dst->htotal;
	dst_vsync_len = dst->vsync_end - dst->vsync_start;
	dst_vback_porch = dst->vtotal - dst->vsync_end;
	dst_vactive = dst->vdisplay;

	bp_in = (src_vback_porch + src_vsync_len) * src_htotal +
		src_hsync_len + src_hback_porch;
	bp_out = (dst_vback_porch + dst_vsync_len) * dst_htotal +
		 dst_hsync_len + dst_hback_porch;

	t_bp_in = bp_in * src_pixclock;
	t_bp_out = bp_out * dst_pixclock;
	tin = src_vtotal * src_htotal * src_pixclock;

	v_scale_ratio = src_vactive / dst_vactive;
	x = 5;
__retry:
	if (v_scale_ratio <= 2)
		t_delta = x * src_htotal * src_pixclock;
	else
		t_delta = 12 * src_htotal * src_pixclock;

	if (t_bp_in + t_delta > t_bp_out)
		t_frm_st = (t_bp_in + t_delta - t_bp_out);
	else
		t_frm_st = tin - (t_bp_out - (t_bp_in + t_delta));

	do_div(t_frm_st, src_pixclock);
	rem = do_div(t_frm_st, src_htotal);
	if ((t_frm_st < 2 || t_frm_st > 14) && x < 12) {
		x++;
		goto __retry;
	}
	if (t_frm_st < 2 || t_frm_st > 14)
		t_frm_st = 4;

	*dsp_frame_hst = rem;
	*dsp_frame_vst = t_frm_st;
}

static void rk628_post_process_scaler_init(struct rk628 *rk628,
					   struct rk628_display_mode *src,
					   const struct rk628_display_mode *dst)
{
	u32 dsp_frame_hst, dsp_frame_vst;
	u32 scl_hor_mode, scl_ver_mode;
	u32 scl_v_factor, scl_h_factor;
	u32 dsp_htotal, dsp_hs_end, dsp_hact_st, dsp_hact_end;
	u32 dsp_vtotal, dsp_vs_end, dsp_vact_st, dsp_vact_end;
	u32 dsp_hbor_end, dsp_hbor_st, dsp_vbor_end, dsp_vbor_st;
	u16 bor_right = 0, bor_left = 0, bor_up = 0, bor_down = 0;
	u8 hor_down_mode = 0, ver_down_mode = 0;
	u32 dst_hsync_len, dst_hback_porch, dst_hfront_porch, dst_hactive;
	u32 dst_vsync_len, dst_vback_porch, dst_vfront_porch, dst_vactive;
	u32 src_hactive;
	u32 src_vactive;
	int gvi_offset = 0;

	if (rk628->version == RK628F_VERSION && rk628->gvi.division_mode)
		gvi_offset = 4;

	src_hactive = src->hdisplay;
	src_vactive = src->vdisplay;

	dst_hactive = dst->hdisplay;
	dst_hsync_len = dst->hsync_end - dst->hsync_start;
	dst_hback_porch = dst->htotal - dst->hsync_end;
	dst_hfront_porch = dst->hsync_start - dst->hdisplay;
	dst_vsync_len = dst->vsync_end - dst->vsync_start;
	dst_vback_porch = dst->vtotal - dst->vsync_end;
	dst_vfront_porch = dst->vsync_start - dst->vdisplay;
	dst_vactive = dst->vdisplay;

	dsp_htotal = dst_hsync_len + dst_hback_porch +
		     dst_hactive + dst_hfront_porch;
	dsp_vtotal = dst_vsync_len + dst_vback_porch +
		     dst_vactive + dst_vfront_porch;
	dsp_hs_end = dst_hsync_len;
	dsp_vs_end = dst_vsync_len;
	dsp_hbor_end = dst_hsync_len + dst_hback_porch + dst_hactive - gvi_offset;
	dsp_hbor_st = dst_hsync_len + dst_hback_porch - gvi_offset;
	dsp_vbor_end = dst_vsync_len + dst_vback_porch + dst_vactive;
	dsp_vbor_st = dst_vsync_len + dst_vback_porch;
	dsp_hact_st = dsp_hbor_st + bor_left;
	dsp_hact_end = dsp_hbor_end - bor_right;
	dsp_vact_st = dsp_vbor_st + bor_up;
	dsp_vact_end = dsp_vbor_end - bor_down;

	calc_dsp_frm_hst_vst(src, dst, &dsp_frame_hst, &dsp_frame_vst);
	dev_info(rk628->dev, "dsp_frame_vst:%d  dsp_frame_hst:%d\n",
		 dsp_frame_vst, dsp_frame_hst);

	if (src_hactive > dst_hactive) {
		scl_hor_mode = 2;

		if (hor_down_mode == 0) {
			if ((src_hactive - 1) / (dst_hactive - 1) > 2)
				scl_h_factor = ((src_hactive - 1) << 14) /
						(dst_hactive - 1);
			else
				scl_h_factor = ((src_hactive - 2) << 14) /
						(dst_hactive - 1);
		} else {
			scl_h_factor = (dst_hactive << 16) / (src_hactive - 1);
		}

	} else if (src_hactive == dst_hactive) {
		scl_hor_mode = 0;
		scl_h_factor = 0;
	} else {
		scl_hor_mode = 1;
		scl_h_factor = ((src_hactive - 1) << 16) / (dst_hactive - 1);
	}

	if (src_vactive > dst_vactive) {
		scl_ver_mode = 2;

		if (ver_down_mode == 0) {
			if ((src_vactive - 1) / (dst_vactive - 1) > 2)
				scl_v_factor = ((src_vactive - 1) << 14) /
						(dst_vactive - 1);
			else
				scl_v_factor = ((src_vactive - 2) << 14) /
						(dst_vactive - 1);
		} else {
			scl_v_factor = (dst_vactive << 16) / (src_vactive - 1);
		}

	} else if (src_vactive == dst_vactive) {
		scl_ver_mode = 0;
		scl_v_factor = 0;
	} else {
		scl_ver_mode = 1;
		scl_v_factor = ((src_vactive - 1) << 16) / (dst_vactive - 1);
	}

	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON0, SW_HRES_MASK,
			      SW_HRES(src_hactive));
	rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_VER_DOWN_MODE(ver_down_mode) |
			SCL_HOR_DOWN_MODE(hor_down_mode) |
			SCL_VER_MODE(scl_ver_mode) |
			SCL_HOR_MODE(scl_hor_mode));
	rk628_i2c_write(rk628, GRF_SCALER_CON1, SCL_V_FACTOR(scl_v_factor) |
			SCL_H_FACTOR(scl_h_factor));
	rk628_i2c_write(rk628, GRF_SCALER_CON2, DSP_FRAME_VST(dsp_frame_vst) |
			DSP_FRAME_HST(dsp_frame_hst));
	rk628_i2c_write(rk628, GRF_SCALER_CON3, DSP_HS_END(dsp_hs_end) |
			DSP_HTOTAL(dsp_htotal));
	rk628_i2c_write(rk628, GRF_SCALER_CON4, DSP_HACT_END(dsp_hact_end) |
			DSP_HACT_ST(dsp_hact_st));
	rk628_i2c_write(rk628, GRF_SCALER_CON5, DSP_VS_END(dsp_vs_end) |
			DSP_VTOTAL(dsp_vtotal));
	rk628_i2c_write(rk628, GRF_SCALER_CON6, DSP_VACT_END(dsp_vact_end) |
			DSP_VACT_ST(dsp_vact_st));
	rk628_i2c_write(rk628, GRF_SCALER_CON7, DSP_HBOR_END(dsp_hbor_end) |
			DSP_HBOR_ST(dsp_hbor_st));
	rk628_i2c_write(rk628, GRF_SCALER_CON8, DSP_VBOR_END(dsp_vbor_end) |
			DSP_VBOR_ST(dsp_vbor_st));
}

static int rk628_scaler_color_bar_show(struct seq_file *s, void *data)
{
	seq_puts(s, "  Enable horizontal color bar:\n");
	seq_puts(s, "      example: echo 1 > /sys/kernel/debug/rk628/2-0050/scaler_color_bar\n");
	seq_puts(s, "  Enable vertical color bar:\n");
	seq_puts(s, "      example: echo 2 > /sys/kernel/debug/rk628/2-0050/scaler_color_bar\n");
	seq_puts(s, "  Disable color bar:\n");
	seq_puts(s, "      example: echo 0 > /sys/kernel/debug/rk628/2-0050/scaler_color_bar\n");

	return 0;
}

static int rk628_scaler_color_bar_open(struct inode *inode, struct file *file)
{
	return single_open(file, rk628_scaler_color_bar_show, inode->i_private);
}

static ssize_t rk628_scaler_color_bar_write(struct file *file, const char __user *ubuf,
					    size_t len, loff_t *offp)
{
	struct rk628 *rk628 = ((struct seq_file *)file->private_data)->private;
	u8 mode;

	if (kstrtou8_from_user(ubuf, len, 0, &mode))
		return -EFAULT;

	switch (mode) {
	case 0:
		rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_COLOR_BAR_EN(0));
		break;
	case 1:
		rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_COLOR_BAR_EN(1));
		rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_COLOR_VER_EN(0));
		break;
	case 2:
	default:
		rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_COLOR_BAR_EN(1));
		rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_COLOR_VER_EN(1));
	}

	return len;
}

static const struct file_operations rk628_scaler_color_bar_fops = {
	.owner = THIS_MODULE,
	.open = rk628_scaler_color_bar_open,
	.read = seq_read,
	.write = rk628_scaler_color_bar_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void rk628_post_process_create_debugfs_file(struct rk628 *rk628)
{
	debugfs_create_file("scaler_color_bar", 0600, rk628->debug_dir,
			    rk628, &rk628_scaler_color_bar_fops);
}

void rk628_post_process_init(struct rk628 *rk628)
{
	struct rk628_display_mode *src = &rk628->src_mode;
	const struct rk628_display_mode *dst = &rk628->dst_mode;

	if (src->flags & DRM_MODE_FLAG_PVSYNC)
		rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
					SW_VSYNC_POL_MASK, SW_VSYNC_POL(1));
	if (src->flags & DRM_MODE_FLAG_PHSYNC)
		rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
					SW_HSYNC_POL_MASK, SW_HSYNC_POL(1));

	rk628_post_process_scaler_init(rk628, src, dst);
}

static void rk628_post_process_csc(struct rk628 *rk628)
{
	enum bus_format in_fmt, out_fmt;
	struct post_csc_coef csc_coef = {};
	bool is_input_yuv, is_output_yuv;
	u32 color_space = V4L2_COLORSPACE_DEFAULT;
	u32 val;
	int range_type;

	in_fmt = rk628_get_input_bus_format(rk628);
	out_fmt = rk628_get_output_bus_format(rk628);

	if (in_fmt == out_fmt) {
		if (out_fmt == BUS_FMT_YUV422) {
			rk628_i2c_write(rk628, GRF_CSC_CTRL_CON,
					SW_YUV2VYU_SWP(1) |
					SW_R2Y_EN(0));
			return;
		}
		rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, SW_R2Y_EN(0));
		rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, SW_Y2R_EN(0));
		rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, SW_FROM_CSC_MATRIX_EN(0));
		return;
	}

	if (rk628->version == RK628D_VERSION) {
		if (in_fmt == BUS_FMT_RGB)
			rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, SW_R2Y_EN(1));
		else if (out_fmt == BUS_FMT_RGB)
			rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, SW_Y2R_EN(1));
	} else {
		rk628->csc_mode = vop2_convert_csc_mode(color_space, CSC_13BIT_DEPTH);

		is_input_yuv = !is_rgb_format(in_fmt);
		is_output_yuv = !is_rgb_format(out_fmt);
		rockchip_calc_post_csc(&csc_coef, rk628->csc_mode, is_input_yuv, is_output_yuv);

		val = ((csc_coef.csc_coef01 & 0xffff) << 16) | (csc_coef.csc_coef00 & 0xffff);
		rk628_i2c_write(rk628, GRF_CSC_MATRIX_COE01_COE00, val);

		val = ((csc_coef.csc_coef10 & 0xffff) << 16) | (csc_coef.csc_coef02 & 0xffff);
		rk628_i2c_write(rk628, GRF_CSC_MATRIX_COE10_COE02, val);

		val = ((csc_coef.csc_coef12 & 0xffff) << 16) | (csc_coef.csc_coef11 & 0xffff);
		rk628_i2c_write(rk628, GRF_CSC_MATRIX_COE12_COE11, val);

		val = ((csc_coef.csc_coef21 & 0xffff) << 16) | (csc_coef.csc_coef20 & 0xffff);
		rk628_i2c_write(rk628, GRF_CSC_MATRIX_COE21_COE20, val);

		rk628_i2c_write(rk628, GRF_CSC_MATRIX_COE22, csc_coef.csc_coef22);

		rk628_i2c_write(rk628, GRF_CSC_MATRIX_OFFSET0, csc_coef.csc_dc0);
		rk628_i2c_write(rk628, GRF_CSC_MATRIX_OFFSET1, csc_coef.csc_dc1);
		rk628_i2c_write(rk628, GRF_CSC_MATRIX_OFFSET2, csc_coef.csc_dc2);

		range_type = csc_coef.range_type ? 0 : 1;
		range_type <<= is_input_yuv ? 0 : 1;
		val = SW_Y2R_MODE(range_type) | SW_FROM_CSC_MATRIX_EN(1);
		rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, val);

		if (rk628_output_is_bt1120(rk628))
			rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, SW_YUV2VYU_SWP(1));
	}
}

const char *rk628_post_process_get_csc_mode_name(struct rk628 *rk628)
{
	return vop_csc_format_str[rk628->csc_mode];
}

void rk628_post_process_enable(struct rk628 *rk628)
{
	rk628_cru_clk_adjust(rk628);
	/*
	 * bt1120 needs to configure the timing register, but hdmitx will modify
	 * the timing as needed, so the bt1120 enable process is moved here.
	 */
	if (rk628_input_is_bt1120(rk628))
		rk628_bt1120_rx_enable(rk628);
	rk628_post_process_csc(rk628);
	rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_EN(1));
}

void rk628_post_process_disable(struct rk628 *rk628)
{
	rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_EN(0));
}
