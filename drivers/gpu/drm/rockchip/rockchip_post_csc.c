// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 * Author: Zhang yubing <yubing.zhang@rock-chips.com>
 */

#include "rockchip_post_csc.h"

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

#ifndef MAX
#define	MAX(a, b)				((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define	MIN(a, b)				((a) < (b) ? (a) : (b))
#endif
#define	CLIP(x, min_v, max_v)			MIN(MAX(x, min_v), max_v)

enum rk_pq_csc_mode {
	RK_PQ_CSC_RGBL_TO_RGBF = 0,
	RK_PQ_CSC_RGBL_TO_YUV601L,
	RK_PQ_CSC_RGBL_TO_YUV601F,
	RK_PQ_CSC_RGBL_TO_YUV709L,
	RK_PQ_CSC_RGBL_TO_YUV709F,
	RK_PQ_CSC_RGBL_TO_YUV2020L,
	RK_PQ_CSC_RGBL_TO_YUV2020F,
	RK_PQ_CSC_RGBF_TO_RGBL,
	RK_PQ_CSC_RGBF_TO_YUV601L,
	RK_PQ_CSC_RGBF_TO_YUV601F,
	RK_PQ_CSC_RGBF_TO_YUV709L,
	RK_PQ_CSC_RGBF_TO_YUV709F,
	RK_PQ_CSC_RGBF_TO_YUV2020L,
	RK_PQ_CSC_RGBF_TO_YUV2020F,
	RK_PQ_CSC_YUV601L_TO_RGBL,
	RK_PQ_CSC_YUV601L_TO_RGBF,
	RK_PQ_CSC_YUV601L_TO_YUV601F,
	RK_PQ_CSC_YUV601L_TO_YUV709L,
	RK_PQ_CSC_YUV601L_TO_YUV709F,
	RK_PQ_CSC_YUV601F_TO_RGBL,
	RK_PQ_CSC_YUV601F_TO_RGBF,
	RK_PQ_CSC_YUV601F_TO_YUV601L,
	RK_PQ_CSC_YUV601F_TO_YUV709L,
	RK_PQ_CSC_YUV601F_TO_YUV709F,
	RK_PQ_CSC_YUV709L_TO_RGBL,
	RK_PQ_CSC_YUV709L_TO_RGBF,
	RK_PQ_CSC_YUV709L_TO_YUV601L,
	RK_PQ_CSC_YUV709L_TO_YUV601F,
	RK_PQ_CSC_YUV709L_TO_YUV709F,
	RK_PQ_CSC_YUV709F_TO_RGBL,
	RK_PQ_CSC_YUV709F_TO_RGBF,
	RK_PQ_CSC_YUV709F_TO_YUV601L,
	RK_PQ_CSC_YUV709F_TO_YUV601F,
	RK_PQ_CSC_YUV709F_TO_YUV709L,
	RK_PQ_CSC_YUV2020L_TO_RGBL,
	RK_PQ_CSC_YUV2020L_TO_RGBF,
	RK_PQ_CSC_YUV2020L_TO_YUV2020F,
	RK_PQ_CSC_YUV2020F_TO_RGBL,
	RK_PQ_CSC_YUV2020F_TO_RGBF,
	RK_PQ_CSC_YUV2020F_TO_YUV2020L,
	RK_PQ_CSC_RGB2020F_TO_RGB2020L,
	RK_PQ_CSC_RGB2020L_TO_RGB2020F,
	RK_PQ_CSC_IDENTITY_MODE, /* for csc input and output is equal */
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

enum rk_pq_csc_version {
	RK_PQ_CSC_UNKNOWN = 0,
	RK_PQ_CSC_V1,
	RK_PQ_CSC_V2,
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
	u8 pixel_depth;
	u8 coef_precision;
	const struct rk_pq_csc_coef *csc_coef;
};

static const struct rk_csc_colorspace_info g_csc_color_info[] = {
	{ OPTM_CS_E_RGB, OPTM_CS_E_RGB, false, true },                 /* RGBL_TO_RGBF */
	{ OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_601, false, false },         /* RGBL_TO_YUV601L */
	{ OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_601, false, true },          /* RGBL_TO_YUV601F */
	{ OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_709, false, false },         /* RGBL_TO_YUV709L */
	{ OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_709, false, true },          /* RGBL_TO_YUV709F */
	{ OPTM_CS_E_RGB_2020, OPTM_CS_E_XV_YCC_2020, false, false },   /* RGBL_TO_YUV2020L */
	{ OPTM_CS_E_RGB_2020, OPTM_CS_E_XV_YCC_2020, false, true },    /* RGBL_TO_YUV2020F */
	{ OPTM_CS_E_RGB, OPTM_CS_E_RGB, true, false },                 /* RGBF_TO_RGBL */
	{ OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_601, true, false },          /* RGBF_TO_YUV601L */
	{ OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_601, true, true },           /* RGBF_TO_YUV601F */
	{ OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_709, true, false },          /* RGBF_TO_YUV709L */
	{ OPTM_CS_E_RGB, OPTM_CS_E_XV_YCC_709, true, true },           /* RGBF_TO_YUV709F */
	{ OPTM_CS_E_RGB_2020, OPTM_CS_E_XV_YCC_2020, true, false },    /* RGBF_TO_YUV2020L */
	{ OPTM_CS_E_RGB_2020, OPTM_CS_E_XV_YCC_2020, true, true },     /* RGBF_TO_YUV2020F */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_RGB, false, false },         /* YUV601L_TO_RGBL */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_RGB, false, true },          /* YUV601L_TO_RGBF */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_601, false, true },   /* YUV601L_TO_YUV601F */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_709, false, false },  /* YUV601L_TO_YUV709L */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_709, false, true },   /* YUV601L_TO_YUV709F */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_RGB, true, false },          /* YUV601F_TO_RGBL */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_RGB, true, true },           /* YUV601F_TO_RGBF */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_601, true, false },   /* YUV601F_TO_YUV601L */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_709, true, false },   /* YUV601F_TO_YUV709L */
	{ OPTM_CS_E_XV_YCC_601, OPTM_CS_E_XV_YCC_709, true, true },    /* YUV601F_TO_YUV709F */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_RGB, false, false },         /* YUV709L_TO_RGBL */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_RGB, false, true },          /* YUV709L_TO_RGBF */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_XV_YCC_601, false, false },  /* YUV709L_TO_YUV601L */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_XV_YCC_601, false, true },   /* YUV709L_TO_YUV601F */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_XV_YCC_709, false, true },   /* YUV709L_TO_YUV709F */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_RGB, true, false },          /* YUV709F_TO_RGBL */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_RGB, true, true },           /* YUV709F_TO_RGBF */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_XV_YCC_601, true, false },   /* YUV709F_TO_YUV601L */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_XV_YCC_601, true, true },    /* YUV709F_TO_YUV601F */
	{ OPTM_CS_E_XV_YCC_709, OPTM_CS_E_XV_YCC_709, true, false },   /* YUV709F_TO_YUV709L */
	{ OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_RGB_2020, false, false },   /* YUV2020L_TO_RGBL */
	{ OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_RGB_2020, false, true },    /* YUV2020L_TO_RGBF */
	{ OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_XV_YCC_2020, false, true }, /* YUV2020L_TO_YUV2020F */
	{ OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_RGB_2020, true, false },    /* YUV2020F_TO_RGBL */
	{ OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_RGB_2020, true, true },     /* YUV2020F_TO_RGBF */
	{ OPTM_CS_E_XV_YCC_2020, OPTM_CS_E_XV_YCC_2020, true, false }, /* YUV2020F_TO_YUV2020L */
	{ OPTM_CS_E_RGB_2020, OPTM_CS_E_RGB_2020, true, false },       /* RGB2020F_TO_RGB2020L */
	{ OPTM_CS_E_RGB_2020, OPTM_CS_E_RGB_2020, false, true },       /* RGB2020L_TO_RGB2020F */
};

/* for 8bit pixel depth + 8bit coef precision case */
static const struct rk_pq_csc_coef g_mode_csc_coefs_8bit_pix_8bit_precision[] = {
	{ 298,   0,   0,   0,  298,    0,   0,    0, 298 }, /* RGBL_TO_RGBF */
	{  77, 150,  29, -44,  -87,  131, 131, -110, -21 }, /* RGBL_TO_YUV601L */
	{  89, 175,  34, -50,  -99,  149, 149, -125, -24 }, /* RGBL_TO_YUV601F */
	{  54, 183,  19, -30, -101,  131, 131, -119, -12 }, /* RGBL_TO_YUV709L */
	{  63, 213,  22, -34, -115,  149, 149, -135, -14 }, /* RGBL_TO_YUV709F */
	{  67, 174,  15, -37,  -94,  131, 131, -120, -11 }, /* RGBL_TO_YUV2020L */
	{  78, 202,  18, -42, -107,  149, 149, -137, -12 }, /* RGBL_TO_YUV2020F */
	{ 220,   0,   0,   0,  220,    0,   0,    0, 220 }, /* RGBF_TO_RGBL */
	{  66, 129,  25, -38,  -74,  112, 112,  -94, -18 }, /* RGBF_TO_YUV601L */
	{  77, 150,  29, -43,  -85,  128, 128, -107, -21 }, /* RGBF_TO_YUV601F */
	{  47, 157,  16, -26,  -87,  113, 112, -102, -10 }, /* RGBF_TO_YUV709L */
	{  54, 183,  19, -29,  -99,  128, 128, -116, -12 }, /* RGBF_TO_YUV709F */
	{  58, 149,  13, -31,  -81,  112, 112, -103,  -9 }, /* RGBF_TO_YUV2020L */
	{  67, 174,  15, -36,  -92,  128, 128, -118, -10 }, /* RGBF_TO_YUV2020F */
	{ 256,   0, 351, 256,  -86, -179, 256,  444,   0 }, /* YUV601L_TO_RGBL */
	{ 298,   0, 409, 298, -100, -208, 298,  516,   0 }, /* YUV601L_TO_RGBF */
	{ 298,   0,   0,   0,  291,    0,   0,    0, 291 }, /* YUV601L_TO_YUV601F */
	{ 256, -30, -53,   0,  261,   29,   0,   19, 262 }, /* YUV601L_TO_YUV709L */
	{ 298, -34, -62,   0,  297,   33,   0,   22, 299 }, /* YUV601L_TO_YUV709F */
	{ 220,   0, 308, 220,  -76, -157, 220,  390,   0 }, /* YUV601F_TO_RGBL */
	{ 256,   0, 359, 256,  -88, -183, 256,  454,   0 }, /* YUV601F_TO_RGBF */
	{ 220,   0,   0,   0,  225,    0,   0,    0, 225 }, /* YUV601F_TO_YUV601L */
	{ 220, -26, -47,   0,  229,   26,   0,   17, 231 }, /* YUV601F_TO_YUV709L */
	{ 256, -30, -54,   0,  261,   29,   0,   19, 262 }, /* YUV601F_TO_YUV709F */
	{ 256,   0, 394, 256,  -47, -117, 256,  464,   0 }, /* YUV709L_TO_RGBL */
	{ 298,   0, 459, 298,  -55, -136, 298,  541,   0 }, /* YUV709L_TO_RGBF */
	{ 256,  25,  49,   0,  253,  -28,   0,  -19, 252 }, /* YUV709L_TO_YUV601L */
	{ 298,  30,  57,   0,  288,  -32,   0,  -21, 287 }, /* YUV709L_TO_YUV601F */
	{ 298,   0,   0,   0,  291,    0,   0,    0, 291 }, /* YUV709L_TO_YUV709F */
	{ 220,   0, 346, 220,  -41, -103, 220,  408,   0 }, /* YUV709F_TO_RGBL */
	{ 256,   0, 403, 256,  -48, -120, 256,  475,   0 }, /* YUV709F_TO_RGBF */
	{ 220,  22,  43,   0,  223,  -25,   0,  -16, 221 }, /* YUV709F_TO_YUV601L */
	{ 256,  26,  50,   0,  253,  -28,   0,  -19, 252 }, /* YUV709F_TO_YUV601F */
	{ 220,   0,   0,   0,  225,    0,   0,    0, 225 }, /* YUV709F_TO_YUV709L */
	{ 256,   0, 369, 256,  -41, -143, 256,  471,   0 }, /* YUV2020L_TO_RGBL */
	{ 298,   0, 430, 298,  -48, -167, 298,  548,   0 }, /* YUV2020L_TO_RGBF */
	{ 298,   0,   0,   0,  291,    0,   0,    0, 291 }, /* YUV2020L_TO_YUV2020F */
	{ 220,   0, 324, 220,  -36, -126, 220,  414,   0 }, /* YUV2020F_TO_RGBL */
	{ 256,   0, 377, 256,  -42, -146, 256,  482,   0 }, /* YUV2020F_TO_RGBF */
	{ 220,   0,   0,   0,  225,    0,   0,    0, 225 }, /* YUV2020F_TO_YUV2020L */
	{ 220,   0,   0,   0,  220,    0,   0,    0, 220 }, /* RGB2020F_TO_RGB2020L */
	{ 298,   0,   0,   0,  298,    0,   0,    0, 298 }, /* RGB2020L_TO_RGB2020F */
	{ 256,   0,   0,   0,  256,    0,   0,    0, 256 }, /* IDENTITY_MODE */
};

/* for 10bit pixel depth + 10bit coef precision case */
static const struct rk_pq_csc_coef g_mode_csc_coefs_10bit_pix_10bit_precision[] = {
	{ 1196,    0,    0,    0, 1196,    0,    0,    0, 1196 }, /* RGBL_TO_RGBF */
	{  306,  601,  117, -177, -347,  524,  524, -439,  -85 }, /* RGBL_TO_YUV601L */
	{  358,  702,  136, -202, -396,  598,  598, -501,  -97 }, /* RGBL_TO_YUV601F */
	{  218,  732,   74, -120, -404,  524,  524, -476,  -48 }, /* RGBL_TO_YUV709L */
	{  254,  855,   86, -137, -461,  598,  598, -543,  -55 }, /* RGBL_TO_YUV709F */
	{  269,  694,   61, -146, -377,  524,  524, -482,  -42 }, /* RGBL_TO_YUV2020L */
	{  314,  811,   71, -167, -431,  598,  598, -550,  -48 }, /* RGBL_TO_YUV2020F */
	{  877,    0,    0,    0,  877,    0,    0,    0,  877 }, /* RGBF_TO_RGBL */
	{  262,  515,  100, -151, -297,  448,  448, -376,  -73 }, /* RGBF_TO_YUV601L */
	{  306,  601,  117, -173, -339,  512,  512, -429,  -83 }, /* RGBF_TO_YUV601F */
	{  186,  627,   63, -103, -346,  448,  448, -407,  -41 }, /* RGBF_TO_YUV709L */
	{  218,  732,   74, -117, -395,  512,  512, -465,  -47 }, /* RGBF_TO_YUV709F */
	{  230,  595,   52, -125, -323,  448,  448, -412,  -36 }, /* RGBF_TO_YUV2020L */
	{  269,  694,   61, -143, -369,  512,  512, -471,  -41 }, /* RGBF_TO_YUV2020F */
	{ 1024,    0, 1404, 1024, -344, -715, 1024, 1774,    0 }, /* YUV601L_TO_RGBL */
	{ 1196,    0, 1639, 1196, -402, -835, 1196, 2072,    0 }, /* YUV601L_TO_RGBF */
	{ 1196,    0,    0,    0, 1169,    0,    0,    0, 1169 }, /* YUV601L_TO_YUV601F */
	{ 1024, -118, -213,    0, 1043,  117,    0,   77, 1050 }, /* YUV601L_TO_YUV709L */
	{ 1196, -138, -249,    0, 1191,  134,    0,   88, 1199 }, /* YUV601L_TO_YUV709F */
	{  877,    0, 1229,  877, -302, -626,  877, 1554,    0 }, /* YUV601F_TO_RGBL */
	{ 1024,    0, 1436, 1024, -352, -731, 1024, 1815,    0 }, /* YUV601F_TO_RGBF */
	{  877,    0,    0,    0,  897,    0,    0,    0,  897 }, /* YUV601F_TO_YUV601L */
	{  877, -106, -191,    0,  914,  103,    0,   67,  920 }, /* YUV601F_TO_YUV709L */
	{ 1024, -121, -218,    0, 1043,  117,    0,   77, 1050 }, /* YUV601F_TO_YUV709F */
	{ 1024,    0, 1577, 1024, -188, -469, 1024, 1858,    0 }, /* YUV709L_TO_RGBL */
	{ 1196,    0, 1841, 1196, -219, -547, 1196, 2169,    0 }, /* YUV709L_TO_RGBF */
	{ 1024,  104,  201,    0, 1014, -113,    0,  -74, 1007 }, /* YUV709L_TO_YUV601L */
	{ 1196,  119,  229,    0, 1157, -129,    0,  -85, 1150 }, /* YUV709L_TO_YUV601F */
	{ 1196,    0,    0,    0, 1169,    0,    0,    0, 1169 }, /* YUV709L_TO_YUV709F */
	{  877,    0, 1381,  877, -164, -410,  877, 1627,    0 }, /* YUV709F_TO_RGBL */
	{ 1024,    0, 1613, 1024, -192, -479, 1024, 1900,    0 }, /* YUV709F_TO_RGBF */
	{  877,   91,  176,    0,  888,  -99,    0,  -65,  882 }, /* YUV709F_TO_YUV601L */
	{ 1024,  104,  201,    0, 1014, -113,    0,  -74, 1007 }, /* YUV709F_TO_YUV601F */
	{  877,    0,    0,    0,  897,    0,    0,    0,  897 }, /* YUV709F_TO_YUV709L */
	{ 1024,    0, 1476, 1024, -165, -572, 1024, 1884,    0 }, /* YUV2020L_TO_RGBL */
	{ 1196,    0, 1724, 1196, -192, -668, 1196, 2200,    0 }, /* YUV2020L_TO_RGBF */
	{ 1196,    0,    0,    0, 1169,    0,    0,    0, 1169 }, /* YUV2020L_TO_YUV2020F */
	{  877,    0, 1293,  877, -144, -501,  877, 1650,    0 }, /* YUV2020F_TO_RGBL */
	{ 1024,    0, 1510, 1024, -169, -585, 1024, 1927,    0 }, /* YUV2020F_TO_RGBF */
	{  877,    0,    0,    0,  897,    0,    0,    0,  897 }, /* YUV2020F_TO_YUV2020L */
	{  877,    0,    0,    0,  877,    0,    0,    0,  877 }, /* RGB2020F_TO_RGB2020L */
	{ 1196,    0,    0,    0, 1196,    0,    0,    0, 1196 }, /* RGB2020L_TO_RGB2020F */
	{ 1024,    0,    0,    0, 1024,    0,    0,    0, 1024 }, /* IDENTITY_MODE */
};

/* for 10bit pixel depth + 13bit coef precision case */
static const struct rk_pq_csc_coef g_mode_csc_coefs_10bit_pix_13bit_precision[] = {
	{ 9567,     0,     0,     0,  9567,     0,    0,     0, 9567 }, /* RGBL_TO_RGBF */
	{ 2449,  4809,   934, -1414, -2776,  4190, 4189, -3508, -681 }, /* RGBL_TO_YUV601L */
	{ 2860,  5616,  1091, -1614, -3169,  4783, 4783, -4005, -778 }, /* RGBL_TO_YUV601F */
	{ 1742,  5859,   591,  -960, -3230,  4190, 4189, -3805, -384 }, /* RGBL_TO_YUV709L */
	{ 2034,  6842,   691, -1096, -3687,  4783, 4783, -4345, -438 }, /* RGBL_TO_YUV709F */
	{ 2152,  5554,   486, -1170, -3020,  4190, 4190, -3853, -337 }, /* RGBL_TO_YUV2020L */
	{ 2513,  6486,   568, -1336, -3447,  4783, 4783, -4398, -385 }, /* RGBL_TO_YUV2020F */
	{ 7015,     0,     0,     0,  7015,     0,    0,     0, 7015 }, /* RGBF_TO_RGBL */
	{ 2097,  4118,   800, -1211, -2377,  3588, 3587, -3004, -583 }, /* RGBF_TO_YUV601L */
	{ 2449,  4809,   934, -1382, -2714,  4096, 4096, -3430, -666 }, /* RGBF_TO_YUV601F */
	{ 1491,  5017,   507,  -822, -2765,  3587, 3588, -3259, -329 }, /* RGBF_TO_YUV709L */
	{ 1742,  5859,   591,  -939, -3157,  4096, 4096, -3720, -376 }, /* RGBF_TO_YUV709F */
	{ 1843,  4756,   416, -1002, -2586,  3588, 3588, -3299, -289 }, /* RGBF_TO_YUV2020L */
	{ 2152,  5554,   486, -1144, -2952,  4096, 4096, -3767, -329 }, /* RGBF_TO_YUV2020F */
	{ 8192,     0, 11229,  8192, -2756, -5720, 8192, 14192,    0 }, /* YUV601L_TO_RGBL */
	{ 9567,     0, 13113,  9567, -3219, -6679, 9567, 16574,    0 }, /* YUV601L_TO_RGBF */
	{ 9567,     0,     0,     0,  9353,     0,    0,     0, 9353 }, /* YUV601L_TO_YUV601F */
	{ 8192,  -947, -1703,     0,  8345,   939,    0,   615, 8399 }, /* YUV601L_TO_YUV709L */
	{ 9567, -1105, -1989,     0,  9527,  1072,    0,   702, 9590 }, /* YUV601L_TO_YUV709F */
	{ 7015,     0,  9835,  7015, -2414, -5010, 7015, 12430,    0 }, /* YUV601F_TO_RGBL */
	{ 8192,     0, 11485,  8192, -2819, -5850, 8192, 14516,    0 }, /* YUV601F_TO_RGBF */
	{ 7015,     0,     0,     0,  7175,     0,    0,     0, 7175 }, /* YUV601F_TO_YUV601L */
	{ 7015,  -829, -1492,     0,  7309,   822,    0,   538, 7357 }, /* YUV601F_TO_YUV709L */
	{ 8192,  -968, -1742,     0,  8345,   939,    0,   615, 8399 }, /* YUV601F_TO_YUV709F */
	{ 8192,     0, 12613,  8192, -1500, -3749, 8192, 14862,    0 }, /* YUV709L_TO_RGBL */
	{ 9567,     0, 14729,  9567, -1752, -4378, 9567, 17356,    0 }, /* YUV709L_TO_RGBF */
	{ 8192,   814,  1570,     0,  8109,  -906,    0,  -594, 8056 }, /* YUV709L_TO_YUV601L */
	{ 9567,   950,  1834,     0,  9258, -1035,    0,  -678, 9198 }, /* YUV709L_TO_YUV601F */
	{ 9567,     0,     0,     0,  9353,     0,    0,     0, 9353 }, /* YUV709L_TO_YUV709F */
	{ 7015,     0, 11047,  7015, -1314, -3284, 7015, 13017,    0 }, /* YUV709F_TO_RGBL */
	{ 8192,     0, 12901,  8192, -1535, -3835, 8192, 15201,    0 }, /* YUV709F_TO_RGBF */
	{ 7015,   713,  1375,     0,  7102,  -794,    0,  -520, 7056 }, /* YUV709F_TO_YUV601L */
	{ 8192,   832,  1606,     0,  8109,  -906,    0,  -594, 8056 }, /* YUV709F_TO_YUV601F */
	{ 7015,     0,     0,     0,  7175,     0,    0,     0, 7175 }, /* YUV709F_TO_YUV709L */
	{ 8192,     0, 11810,  8192, -1318, -4576, 8192, 15068,    0 }, /* YUV2020L_TO_RGBL */
	{ 9567,     0, 13792,  9567, -1539, -5344, 9567, 17597,    0 }, /* YUV2020L_TO_RGBF */
	{ 9567,     0,     0,     0,  9353,     0,    0,     0, 9353 }, /* YUV2020L_TO_YUV2020F */
	{ 7015,     0, 10344,  7015, -1154, -4008, 7015, 13198,    0 }, /* YUV2020F_TO_RGBL */
	{ 8192,     0, 12080,  8192, -1348, -4681, 8192, 15412,    0 }, /* YUV2020F_TO_RGBF */
	{ 7015,     0,     0,     0,  7175,     0,    0,     0, 7175 }, /* YUV2020F_TO_YUV2020L */
	{ 7015,     0,     0,     0,  7015,     0,    0,     0, 7015 }, /* RGB2020F_TO_RGB2020L */
	{ 9567,     0,     0,     0,  9567,     0,    0,     0, 9567 }, /* RGB2020L_TO_RGB2020F */
	{ 8192,     0,     0,     0,  8192,     0,    0,     0, 8192 }, /* IDENTITY_MODE */
};

static const struct rk_csc_mode_coef g_csc_mode_coefs[] = {
	{8, 8, g_mode_csc_coefs_8bit_pix_8bit_precision},
	{10, 10, g_mode_csc_coefs_10bit_pix_10bit_precision},
	{10, 13, g_mode_csc_coefs_10bit_pix_13bit_precision},
};

/* 10bit Hue Sin Look Up Table -> range[-30, 30] */
static const s32 g_hue_sin_table[PQ_CSC_HUE_TABLE_NUM] = {
	512, 508, 505, 501, 497, 494, 490, 486,
	483, 479, 475, 472, 468, 464, 460, 457,
	453, 449, 445, 442, 438, 434, 430, 426,
	423, 419, 415, 411, 407, 403, 400, 396,
	392, 388, 384, 380, 376, 372, 369, 365,
	361, 357, 353, 349, 345, 341, 337, 333,
	329, 325, 321, 317, 313, 309, 305, 301,
	297, 293, 289, 285, 281, 277, 273, 269,
	265, 261, 257, 253, 249, 245, 241, 237,
	233, 228, 224, 220, 216, 212, 208, 204,
	200, 196, 192, 187, 183, 179, 175, 171,
	167, 163, 159, 154, 150, 146, 142, 138,
	134, 130, 125, 121, 117, 113, 109, 105,
	100, 96, 92, 88, 84, 80, 75, 71,
	67, 63, 59, 54, 50, 46, 42, 38,
	34, 29, 25, 21, 17, 13, 8, 4,
	0, -4, -8, -13, -17, -21, -25, -29,
	-34, -38, -42, -46, -50, -54, -59, -63,
	-67, -71, -75, -80, -84, -88, -92, -96,
	-100, -105, -109, -113, -117, -121, -125, -130,
	-134, -138, -142, -146, -150, -154, -159, -163,
	-167, -171, -175, -179, -183, -187, -192, -196,
	-200, -204, -208, -212, -216, -220, -224, -228,
	-233, -237, -241, -245, -249, -253, -257, -261,
	-265, -269, -273, -277, -281, -285, -289, -293,
	-297, -301, -305, -309, -313, -317, -321, -325,
	-329, -333, -337, -341, -345, -349, -353, -357,
	-361, -365, -369, -372, -376, -380, -384, -388,
	-392, -396, -400, -403, -407, -411, -415, -419,
	-423, -426, -430, -434, -438, -442, -445, -449,
	-453, -457, -460, -464, -468, -472, -475, -479,
	-483, -486, -490, -494, -497, -501, -505, -508,
};

/* 10bit Hue Cos Look Up Table  -> range[-30, 30] */
static const s32 g_hue_cos_table[PQ_CSC_HUE_TABLE_NUM] = {
	887, 889, 891, 893, 895, 897, 899, 901,
	903, 905, 907, 909, 911, 913, 915, 917,
	919, 920, 922, 924, 926, 928, 929, 931,
	933, 935, 936, 938, 940, 941, 943, 945,
	946, 948, 949, 951, 953, 954, 956, 957,
	959, 960, 962, 963, 964, 966, 967, 969,
	970, 971, 973, 974, 975, 976, 978, 979,
	980, 981, 983, 984, 985, 986, 987, 988,
	989, 990, 992, 993, 994, 995, 996, 997,
	998, 998, 999, 1000, 1001, 1002, 1003, 1004,
	1005, 1005, 1006, 1007, 1008, 1008, 1009, 1010,
	1011, 1011, 1012, 1013, 1013, 1014, 1014, 1015,
	1015, 1016, 1016, 1017, 1017, 1018, 1018, 1019,
	1019, 1020, 1020, 1020, 1021, 1021, 1021, 1022,
	1022, 1022, 1022, 1023, 1023, 1023, 1023, 1023,
	1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
	1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
	1023, 1023, 1023, 1023, 1023, 1022, 1022, 1022,
	1022, 1021, 1021, 1021, 1020, 1020, 1020, 1019,
	1019, 1018, 1018, 1017, 1017, 1016, 1016, 1015,
	1015, 1014, 1014, 1013, 1013, 1012, 1011, 1011,
	1010, 1009, 1008, 1008, 1007, 1006, 1005, 1005,
	1004, 1003, 1002, 1001, 1000, 999, 998, 998,
	997, 996, 995, 994, 993, 992, 990, 989,
	988, 987, 986, 985, 984, 983, 981, 980,
	979, 978, 976, 975, 974, 973, 971, 970,
	969, 967, 966, 964, 963, 962, 960, 959,
	957, 956, 954, 953, 951, 949, 948, 946,
	945, 943, 941, 940, 938, 936, 935, 933,
	931, 929, 928, 926, 924, 922, 920, 919,
	917, 915, 913, 911, 909, 907, 905, 903,
	901, 899, 897, 895, 893, 891, 889, 887
};

static const struct rk_pq_csc_coef r2y_for_y2y = {
	306, 601, 117,
	-173, -339, 512,
	512, -429, -83,
};

static const struct rk_pq_csc_coef y2r_for_y2y = {
	1024, -1, 1436,
	1024, -353, -731,
	1024, 1814, 1,
};

static const struct rk_pq_csc_coef r2y_for_r2r = {
	218, 732, 74,
	-117, -395, 512,
	512, -465, -47,
};

static const struct rk_pq_csc_coef y2r_for_r2r = {
	1024, 0, 1612,
	1024, -192, -480,
	1024, 1900, -2,
};

static const struct rk_pq_csc_coef rgb_input_swap_matrix = {
	0, 0, 1,
	1, 0, 0,
	0, 1, 0,
};

static const struct rk_pq_csc_coef yuv_output_swap_matrix = {
	0, 0, 1,
	1, 0, 0,
	0, 1, 0,
};

static enum rk_pq_csc_version get_csc_version(u32 plat)
{
	switch (plat) {
	case VOP_VERSION_RK3528:
	case VOP_VERSION_RK3576:
		return RK_PQ_CSC_V1;
	default:
		return RK_PQ_CSC_UNKNOWN;
	}
}

static
enum color_space_type get_color_space_type(enum drm_color_encoding color_encoding, bool is_yuv)
{
	enum color_space_type color_space_type;

	switch (color_encoding) {
	case DRM_COLOR_YCBCR_BT601:
		if (is_yuv)
			color_space_type = OPTM_CS_E_XV_YCC_601;
		else
			color_space_type = OPTM_CS_E_RGB;
		break;
	case DRM_COLOR_YCBCR_BT709:
		if (is_yuv)
			color_space_type = OPTM_CS_E_XV_YCC_709;
		else
			color_space_type = OPTM_CS_E_RGB;
		break;
	case DRM_COLOR_YCBCR_BT2020:
		if (is_yuv)
			color_space_type = OPTM_CS_E_XV_YCC_2020;
		else
			color_space_type = OPTM_CS_E_RGB_2020;
		break;
	default:
		if (is_yuv)
			color_space_type = OPTM_CS_E_XV_YCC_601;
		else
			color_space_type = OPTM_CS_E_RGB_2020;
	}

	return color_space_type;
}

static const struct rk_pq_csc_coef *csc_get_csc_coef(struct post_csc_convert_mode *convert_mode)
{
	const struct rk_csc_mode_coef  *csc_mode_coef = NULL;
	int i, j;
	enum color_space_type input_color_space, output_color_space;
	bool is_input_full_range = convert_mode->is_input_full_range;
	bool is_output_full_range = convert_mode->is_output_full_range;
	bool is_input_yuv = convert_mode->is_input_yuv;
	bool is_output_yuv = convert_mode->is_output_yuv;
	u8 pixel_depth = convert_mode->pixel_depth;
	u8 coef_precision = convert_mode->coef_precision;

	/* Search for coef table at different csc precision */
	for (i = 0; i < ARRAY_SIZE(g_csc_mode_coefs); i++) {
		if ((g_csc_mode_coefs[i].pixel_depth == pixel_depth) &&
		    (g_csc_mode_coefs[i].coef_precision == coef_precision)) {
			csc_mode_coef = &g_csc_mode_coefs[i];
			break;
		}
	}

	if (!csc_mode_coef)
		return NULL;

	input_color_space = get_color_space_type(convert_mode->intput_color_encoding,
						 is_input_yuv);
	output_color_space = get_color_space_type(convert_mode->output_color_encoding,
						  is_output_yuv);

	for (i = 0; i < 2; i++) {
		/* csc input and output format is equal */
		if ((is_input_full_range == is_output_full_range) &&
		    (is_input_yuv == is_output_yuv) && (input_color_space == output_color_space))
			return &csc_mode_coef->csc_coef[RK_PQ_CSC_IDENTITY_MODE];

		/* Search for csc coef at different csc input/output format */
		for (j = 0; j < ARRAY_SIZE(g_csc_color_info); j++) {
			if (g_csc_color_info[j].input_color_space == input_color_space &&
			    g_csc_color_info[j].output_color_space == output_color_space &&
			    g_csc_color_info[j].in_full_range == is_input_full_range &&
			    g_csc_color_info[j].out_full_range == is_output_full_range)
				return &csc_mode_coef->csc_coef[j];
		}

		/*
		 * If no csc matrix can be found for current input/output
		 * colorspace of post-csc, then csc matrix is found based
		 * on colorspace of post-csc output.
		 */
		convert_mode->intput_color_encoding = convert_mode->output_color_encoding;
		input_color_space = get_color_space_type(convert_mode->intput_color_encoding,
							 is_input_yuv);
		output_color_space = get_color_space_type(convert_mode->output_color_encoding,
							  is_output_yuv);
	}

	return NULL;
}

static void csc_matrix_multiply(struct rk_pq_csc_coef *dst, const struct rk_pq_csc_coef *m0,
				const struct rk_pq_csc_coef *m1)
{
	dst->csc_coef00 = m0->csc_coef00 * m1->csc_coef00 +
			  m0->csc_coef01 * m1->csc_coef10 +
			  m0->csc_coef02 * m1->csc_coef20;

	dst->csc_coef01 = m0->csc_coef00 * m1->csc_coef01 +
			  m0->csc_coef01 * m1->csc_coef11 +
			  m0->csc_coef02 * m1->csc_coef21;

	dst->csc_coef02 = m0->csc_coef00 * m1->csc_coef02 +
			  m0->csc_coef01 * m1->csc_coef12 +
			  m0->csc_coef02 * m1->csc_coef22;

	dst->csc_coef10 = m0->csc_coef10 * m1->csc_coef00 +
			  m0->csc_coef11 * m1->csc_coef10 +
			  m0->csc_coef12 * m1->csc_coef20;

	dst->csc_coef11 = m0->csc_coef10 * m1->csc_coef01 +
			  m0->csc_coef11 * m1->csc_coef11 +
			  m0->csc_coef12 * m1->csc_coef21;

	dst->csc_coef12 = m0->csc_coef10 * m1->csc_coef02 +
			  m0->csc_coef11 * m1->csc_coef12 +
			  m0->csc_coef12 * m1->csc_coef22;

	dst->csc_coef20 = m0->csc_coef20 * m1->csc_coef00 +
			  m0->csc_coef21 * m1->csc_coef10 +
			  m0->csc_coef22 * m1->csc_coef20;

	dst->csc_coef21 = m0->csc_coef20 * m1->csc_coef01 +
			  m0->csc_coef21 * m1->csc_coef11 +
			  m0->csc_coef22 * m1->csc_coef21;

	dst->csc_coef22 = m0->csc_coef20 * m1->csc_coef02 +
			  m0->csc_coef21 * m1->csc_coef12 +
			  m0->csc_coef22 * m1->csc_coef22;
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

static void csc_matrix_element_right_shift(struct rk_pq_csc_coef *m, int n)
{
	m->csc_coef00 = m->csc_coef00 >> n;
	m->csc_coef01 = m->csc_coef01 >> n;
	m->csc_coef02 = m->csc_coef02 >> n;
	m->csc_coef10 = m->csc_coef10 >> n;
	m->csc_coef11 = m->csc_coef11 >> n;
	m->csc_coef12 = m->csc_coef12 >> n;
	m->csc_coef20 = m->csc_coef20 >> n;
	m->csc_coef21 = m->csc_coef21 >> n;
	m->csc_coef22 = m->csc_coef22 >> n;
}

static inline s32 csc_simple_round(s32 x, s32 n)
{
	s32 value = 0;

	if (n == 0)
		return x;

	value = (abs(x) + (1 << (n - 1))) >> (n);
	return (((x) >= 0) ? value : -value);
}

static void csc_matrix_element_right_shift_with_simple_round(struct rk_pq_csc_coef *m, int n)
{
	m->csc_coef00 = csc_simple_round(m->csc_coef00, n);
	m->csc_coef01 = csc_simple_round(m->csc_coef01, n);
	m->csc_coef02 = csc_simple_round(m->csc_coef02, n);
	m->csc_coef10 = csc_simple_round(m->csc_coef10, n);
	m->csc_coef11 = csc_simple_round(m->csc_coef11, n);
	m->csc_coef12 = csc_simple_round(m->csc_coef12, n);
	m->csc_coef20 = csc_simple_round(m->csc_coef20, n);
	m->csc_coef21 = csc_simple_round(m->csc_coef21, n);
	m->csc_coef22 = csc_simple_round(m->csc_coef22, n);
}

static struct rk_pq_csc_coef create_rgb_gain_matrix(s32 r_gain, s32 g_gain, s32 b_gain)
{
	struct rk_pq_csc_coef m;

	m.csc_coef00 = r_gain;
	m.csc_coef01 = 0;
	m.csc_coef02 = 0;

	m.csc_coef10 = 0;
	m.csc_coef11 = g_gain;
	m.csc_coef12 = 0;

	m.csc_coef20 = 0;
	m.csc_coef21 = 0;
	m.csc_coef22 = b_gain;

	return m;
}

static struct rk_pq_csc_coef create_contrast_matrix(s32 contrast)
{
	struct rk_pq_csc_coef m;

	m.csc_coef00 = contrast;
	m.csc_coef01 = 0;
	m.csc_coef02 = 0;

	m.csc_coef10 = 0;
	m.csc_coef11 = contrast;
	m.csc_coef12 = 0;

	m.csc_coef20 = 0;
	m.csc_coef21 = 0;
	m.csc_coef22 = contrast;

	return m;
}

static struct rk_pq_csc_coef create_hue_matrix(s32 hue)
{
	struct rk_pq_csc_coef m;
	s32 hue_idx;
	s32 sin_hue;
	s32 cos_hue;

	hue_idx = CLIP(hue / PQ_CSC_HUE_TABLE_DIV_COEF, 0, PQ_CSC_HUE_TABLE_NUM - 1);
	sin_hue = g_hue_sin_table[hue_idx];
	cos_hue = g_hue_cos_table[hue_idx];

	m.csc_coef00 = 1024;
	m.csc_coef01 = 0;
	m.csc_coef02 = 0;

	m.csc_coef10 = 0;
	m.csc_coef11 = cos_hue;
	m.csc_coef12 = sin_hue;

	m.csc_coef20 = 0;
	m.csc_coef21 = -sin_hue;
	m.csc_coef22 = cos_hue;

	return m;
}

static struct rk_pq_csc_coef create_saturation_matrix(s32 saturation)
{
	struct rk_pq_csc_coef m;

	m.csc_coef00 = 512;
	m.csc_coef01 = 0;
	m.csc_coef02 = 0;

	m.csc_coef10 = 0;
	m.csc_coef11 = saturation;
	m.csc_coef12 = 0;

	m.csc_coef20 = 0;
	m.csc_coef21 = 0;
	m.csc_coef22 = saturation;

	return m;
}

static int csc_calc_adjust_output_coef(struct post_csc_convert_mode *convert_mode,
				       struct post_csc *csc_input_cfg,
				       const struct rk_pq_csc_coef *csc_coef,
				       struct rk_pq_csc_coef *out_matrix,
				       struct rk_pq_csc_ventor *out_dc)
{
	struct rk_pq_csc_coef gain_matrix;
	struct rk_pq_csc_coef contrast_matrix;
	struct rk_pq_csc_coef hue_matrix;
	struct rk_pq_csc_coef saturation_matrix;
	struct rk_pq_csc_coef temp0, temp1;
	const struct rk_pq_csc_coef *r2y_matrix;
	const struct rk_pq_csc_coef *y2r_matrix;
	struct rk_pq_csc_ventor dc_in_ventor;
	struct rk_pq_csc_ventor dc_out_ventor;
	struct rk_pq_csc_ventor v;
	s32 contrast, saturation, brightness;
	s32 r_gain, g_gain, b_gain;
	s32 r_offset, g_offset, b_offset;
	s32 dc_in_offset, dc_out_offset;
	s32 offset_shift_bits;

	contrast = csc_input_cfg->contrast * PQ_CSC_PARAM_FIX_NUM / PQ_CSC_IN_PARAM_NORM_COEF;
	saturation = csc_input_cfg->saturation  * PQ_CSC_PARAM_FIX_NUM / PQ_CSC_IN_PARAM_NORM_COEF;
	r_gain = csc_input_cfg->r_gain * PQ_CSC_PARAM_FIX_NUM / PQ_CSC_IN_PARAM_NORM_COEF;
	g_gain = csc_input_cfg->g_gain * PQ_CSC_PARAM_FIX_NUM / PQ_CSC_IN_PARAM_NORM_COEF;
	b_gain = csc_input_cfg->b_gain * PQ_CSC_PARAM_FIX_NUM / PQ_CSC_IN_PARAM_NORM_COEF;
	r_offset = ((s32)csc_input_cfg->r_offset - PQ_CSC_BRIGHTNESS_OFFSET) /
		   PQ_CSC_TEMP_OFFSET_DIV_COEF;
	g_offset = ((s32)csc_input_cfg->g_offset - PQ_CSC_BRIGHTNESS_OFFSET) /
		   PQ_CSC_TEMP_OFFSET_DIV_COEF;
	b_offset = ((s32)csc_input_cfg->b_offset - PQ_CSC_BRIGHTNESS_OFFSET) /
		   PQ_CSC_TEMP_OFFSET_DIV_COEF;

	gain_matrix = create_rgb_gain_matrix(r_gain, g_gain, b_gain);
	contrast_matrix = create_contrast_matrix(contrast);
	hue_matrix = create_hue_matrix(csc_input_cfg->hue);
	saturation_matrix = create_saturation_matrix(saturation);

	brightness = (s32)csc_input_cfg->brightness - PQ_CSC_BRIGHTNESS_OFFSET;
	dc_in_offset = convert_mode->is_input_full_range ? 0 : -PQ_CSC_DC_IN_OFFSET;
	dc_out_offset = convert_mode->is_output_full_range ? 0 : PQ_CSC_DC_IN_OFFSET;

	/*
	 * M0 = hue_matrix * saturation_matrix,
	 * M1 = gain_matrix * constrast_matrix,
	 */

	if (convert_mode->is_input_yuv && convert_mode->is_output_yuv) {
		/*
		 * yuv2yuv: output = T * M0 * N_r2y * M1 * N_y2r,
		 * so output = T * hue_matrix * saturation_matrix *
		 * N_r2y * gain_matrix * contrast_matrix * N_y2r
		 */
		r2y_matrix = &r2y_for_y2y;
		y2r_matrix = &y2r_for_y2y;
		csc_matrix_multiply(&temp0, csc_coef, &hue_matrix);
		/*
		 * The value bits width is 32 bit, so every time 2 matirx multifly,
		 * right shift is necessary to avoid overflow. For enhancing the
		 * calculator precision, PQ_CALC_ENHANCE_BIT bits is reserved and
		 * right shift before get the final result.
		 */
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_FIX_BIT_WIDTH -
					       PQ_CALC_ENHANCE_BIT);
		csc_matrix_multiply(&temp1, &temp0, &saturation_matrix);
		csc_matrix_element_right_shift(&temp1, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(&temp0, &temp1, r2y_matrix);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_FIX_BIT_WIDTH);
		csc_matrix_multiply(&temp1, &temp0, &gain_matrix);
		csc_matrix_element_right_shift(&temp1, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(&temp0, &temp1, &contrast_matrix);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(out_matrix, &temp0, y2r_matrix);
		csc_matrix_element_right_shift_with_simple_round(out_matrix,
			PQ_CSC_PARAM_FIX_BIT_WIDTH + PQ_CALC_ENHANCE_BIT);

		dc_in_ventor.csc_offset0 = dc_in_offset;
		dc_in_ventor.csc_offset1 = -PQ_CSC_DC_IN_OUT_DEFAULT;
		dc_in_ventor.csc_offset2 = -PQ_CSC_DC_IN_OUT_DEFAULT;
		dc_out_ventor.csc_offset0 = brightness + dc_out_offset;
		dc_out_ventor.csc_offset1 = PQ_CSC_DC_IN_OUT_DEFAULT;
		dc_out_ventor.csc_offset2 = PQ_CSC_DC_IN_OUT_DEFAULT;
	} else if (convert_mode->is_input_yuv && !convert_mode->is_output_yuv) {
		/*
		 * yuv2rgb: output = M1 * T * M0,
		 * so output = gain_matrix * contrast_matrix * T *
		 * hue_matrix * saturation_matrix
		 */
		csc_matrix_multiply(&temp0, csc_coef, &hue_matrix);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_FIX_BIT_WIDTH -
					       PQ_CALC_ENHANCE_BIT);
		csc_matrix_multiply(&temp1, &temp0, &saturation_matrix);
		csc_matrix_element_right_shift(&temp1, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(&temp0, &contrast_matrix, &temp1);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(out_matrix, &gain_matrix, &temp0);
		csc_matrix_element_right_shift(out_matrix, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH +
					       PQ_CALC_ENHANCE_BIT);

		dc_in_ventor.csc_offset0 = dc_in_offset;
		dc_in_ventor.csc_offset1 = -PQ_CSC_DC_IN_OUT_DEFAULT;
		dc_in_ventor.csc_offset2 = -PQ_CSC_DC_IN_OUT_DEFAULT;
		dc_out_ventor.csc_offset0 = brightness + dc_out_offset + r_offset;
		dc_out_ventor.csc_offset1 = brightness + dc_out_offset + g_offset;
		dc_out_ventor.csc_offset2 = brightness + dc_out_offset + b_offset;
	} else if (!convert_mode->is_input_yuv && convert_mode->is_output_yuv) {
		/*
		 * rgb2yuv: output = M0 * T * M1,
		 * so output = hue_matrix * saturation_matrix * T *
		 * gain_matrix * contrast_matrix
		 */
		csc_matrix_multiply(&temp0, csc_coef, &gain_matrix);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH -
					       PQ_CALC_ENHANCE_BIT);
		csc_matrix_multiply(&temp1, &temp0, &contrast_matrix);
		csc_matrix_element_right_shift(&temp1, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(&temp0, &saturation_matrix, &temp1);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(out_matrix, &hue_matrix, &temp0);
		csc_matrix_element_right_shift(out_matrix, PQ_CSC_PARAM_FIX_BIT_WIDTH +
					       PQ_CALC_ENHANCE_BIT);

		dc_in_ventor.csc_offset0 = dc_in_offset;
		dc_in_ventor.csc_offset1 = dc_in_offset;
		dc_in_ventor.csc_offset2 = dc_in_offset;
		dc_out_ventor.csc_offset0 = brightness + dc_out_offset;
		dc_out_ventor.csc_offset1 = PQ_CSC_DC_IN_OUT_DEFAULT;
		dc_out_ventor.csc_offset2 = PQ_CSC_DC_IN_OUT_DEFAULT;
	} else {
		/*
		 * rgb2rgb: output = T * M1 * N_y2r * M0 * N_r2y,
		 * so output = T * gain_matrix * contrast_matrix *
		 * N_y2r * hue_matrix * saturation_matrix * N_r2y
		 */
		r2y_matrix = &r2y_for_r2r;
		y2r_matrix = &y2r_for_r2r;

		csc_matrix_multiply(&temp0, &contrast_matrix, y2r_matrix);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH -
					       PQ_CALC_ENHANCE_BIT);
		csc_matrix_multiply(&temp1, &gain_matrix, &temp0);
		csc_matrix_element_right_shift(&temp1, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(&temp0, &temp1, &hue_matrix);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_FIX_BIT_WIDTH);
		csc_matrix_multiply(&temp1, &temp0, &saturation_matrix);
		csc_matrix_element_right_shift(&temp1, PQ_CSC_PARAM_HALF_FIX_BIT_WIDTH);
		csc_matrix_multiply(&temp0, &temp1, r2y_matrix);
		csc_matrix_element_right_shift(&temp0, PQ_CSC_PARAM_FIX_BIT_WIDTH);
		csc_matrix_multiply(out_matrix, csc_coef, &temp0);
		csc_matrix_element_right_shift_with_simple_round(out_matrix,
								 PQ_CSC_PARAM_FIX_BIT_WIDTH +
								 PQ_CALC_ENHANCE_BIT);

		dc_in_ventor.csc_offset0 = dc_in_offset;
		dc_in_ventor.csc_offset1 = dc_in_offset;
		dc_in_ventor.csc_offset2 = dc_in_offset;
		dc_out_ventor.csc_offset0 = brightness + dc_out_offset + r_offset;
		dc_out_ventor.csc_offset1 = brightness + dc_out_offset + g_offset;
		dc_out_ventor.csc_offset2 = brightness + dc_out_offset + b_offset;
	}

	if (convert_mode->pixel_depth < 10) {
		offset_shift_bits = 10 - convert_mode->pixel_depth; // [1, 2]
		dc_in_ventor.csc_offset0 >>= offset_shift_bits;
		dc_in_ventor.csc_offset1 >>= offset_shift_bits;
		dc_in_ventor.csc_offset2 >>= offset_shift_bits;
		dc_out_ventor.csc_offset0 >>= offset_shift_bits;
		dc_out_ventor.csc_offset1 >>= offset_shift_bits;
		dc_out_ventor.csc_offset2 >>= offset_shift_bits;
	} else {
		offset_shift_bits = convert_mode->pixel_depth - 10; // [0, 3]
		dc_in_ventor.csc_offset0 <<= offset_shift_bits;
		dc_in_ventor.csc_offset1 <<= offset_shift_bits;
		dc_in_ventor.csc_offset2 <<= offset_shift_bits;
		dc_out_ventor.csc_offset0 <<= offset_shift_bits;
		dc_out_ventor.csc_offset1 <<= offset_shift_bits;
		dc_out_ventor.csc_offset2 <<= offset_shift_bits;
	}

	csc_matrix_ventor_multiply(&v, out_matrix, &dc_in_ventor);
	out_dc->csc_offset0 = v.csc_offset0 +
		(dc_out_ventor.csc_offset0 << convert_mode->coef_precision);
	out_dc->csc_offset1 = v.csc_offset1 +
		(dc_out_ventor.csc_offset1 << convert_mode->coef_precision);
	out_dc->csc_offset2 = v.csc_offset2 +
		(dc_out_ventor.csc_offset2 << convert_mode->coef_precision);

	return 0;
}

static void csc_get_range_offset(const struct post_csc_convert_mode *convert_mode,
				 struct rk_pq_csc_dc_coef *csc_dc_coef)
{
	int offset_y = convert_mode->is_input_full_range ? 0 : 16;
	int offset_c = convert_mode->is_input_yuv ? 128 : offset_y;
	int offset_shift_bits = convert_mode->pixel_depth - 8;

	csc_dc_coef->csc_in_dc0 = -offset_y << offset_shift_bits;
	csc_dc_coef->csc_in_dc1 = -offset_c << offset_shift_bits;
	csc_dc_coef->csc_in_dc2 = -offset_c << offset_shift_bits;

	offset_y = convert_mode->is_output_full_range ? 0 : 16;
	offset_c = convert_mode->is_output_yuv ? 128 : offset_y;
	csc_dc_coef->csc_out_dc0 = offset_y << offset_shift_bits;
	csc_dc_coef->csc_out_dc1 = offset_c << offset_shift_bits;
	csc_dc_coef->csc_out_dc2 = offset_c << offset_shift_bits;
}

static int csc_calc_default_output_coef(const struct post_csc_convert_mode *convert_mode,
					const struct rk_pq_csc_coef *csc_coef,
					struct rk_pq_csc_coef *out_matrix,
					struct rk_pq_csc_ventor *out_dc)
{
	struct rk_pq_csc_ventor dc_in_ventor;
	struct rk_pq_csc_ventor dc_out_ventor;
	struct rk_pq_csc_ventor v;
	struct rk_pq_csc_dc_coef csc_dc_coef = {0};

	csc_get_range_offset(convert_mode, &csc_dc_coef);

	out_matrix->csc_coef00 = csc_coef->csc_coef00;
	out_matrix->csc_coef01 = csc_coef->csc_coef01;
	out_matrix->csc_coef02 = csc_coef->csc_coef02;
	out_matrix->csc_coef10 = csc_coef->csc_coef10;
	out_matrix->csc_coef11 = csc_coef->csc_coef11;
	out_matrix->csc_coef12 = csc_coef->csc_coef12;
	out_matrix->csc_coef20 = csc_coef->csc_coef20;
	out_matrix->csc_coef21 = csc_coef->csc_coef21;
	out_matrix->csc_coef22 = csc_coef->csc_coef22;

	dc_in_ventor.csc_offset0 = csc_dc_coef.csc_in_dc0;
	dc_in_ventor.csc_offset1 = csc_dc_coef.csc_in_dc1;
	dc_in_ventor.csc_offset2 = csc_dc_coef.csc_in_dc2;
	dc_out_ventor.csc_offset0 = csc_dc_coef.csc_out_dc0;
	dc_out_ventor.csc_offset1 = csc_dc_coef.csc_out_dc1;
	dc_out_ventor.csc_offset2 = csc_dc_coef.csc_out_dc2;

	csc_matrix_ventor_multiply(&v, csc_coef, &dc_in_ventor);
	out_dc->csc_offset0 = v.csc_offset0 +
		(dc_out_ventor.csc_offset0 << convert_mode->coef_precision);
	out_dc->csc_offset1 = v.csc_offset1 +
		(dc_out_ventor.csc_offset1 << convert_mode->coef_precision);
	out_dc->csc_offset2 = v.csc_offset2 +
		(dc_out_ventor.csc_offset2 << convert_mode->coef_precision);

	return 0;
}

static inline s32 pq_csc_simple_round(s32 x, s32 n)
{
	s32 value = 0;

	if (n == 0)
		return x;

	value = (abs(x) + (1 << (n - 1))) >> (n);
	return (((x) >= 0) ? value : -value);
}

static void rockchip_swap_color_channel(const struct post_csc_convert_mode *mode,
					struct post_csc_coef *csc_simple_coef,
					struct rk_pq_csc_coef *out_matrix,
					struct rk_pq_csc_ventor *out_dc)
{
	struct rk_pq_csc_coef tmp_matrix;
	struct rk_pq_csc_ventor tmp_v;

	if (mode->swap_channels) {
		if (!mode->is_input_yuv) {
			memcpy(&tmp_matrix, out_matrix, sizeof(struct rk_pq_csc_coef));
			csc_matrix_multiply(out_matrix, &tmp_matrix, &rgb_input_swap_matrix);
		}

		if (mode->is_output_yuv) {
			memcpy(&tmp_matrix, out_matrix, sizeof(struct rk_pq_csc_coef));
			memcpy(&tmp_v, out_dc, sizeof(struct rk_pq_csc_ventor));
			csc_matrix_multiply(out_matrix, &yuv_output_swap_matrix, &tmp_matrix);
			csc_matrix_ventor_multiply(out_dc, &yuv_output_swap_matrix, &tmp_v);
		}
	}
}

int rockchip_calc_post_csc(struct post_csc *csc_cfg, struct post_csc_coef *csc_simple_coef,
			   struct post_csc_convert_mode *convert_mode)
{
	int ret = 0;
	struct rk_pq_csc_coef out_matrix;
	struct rk_pq_csc_ventor out_dc;
	const struct rk_pq_csc_coef *csc_coef;
	int bit_num = PQ_CSC_SIMPLE_MAT_PARAM_FIX_BIT_WIDTH;

	csc_coef = csc_get_csc_coef(convert_mode);
	if (!csc_coef) {
		DRM_ERROR("get csc index err:\n");
		DRM_ERROR("pixel_depth=%d, coef_precision=%d\n",
			  convert_mode->pixel_depth, convert_mode->coef_precision);
		DRM_ERROR("input: colorspace=%d, yuv=%d, full_range=%d\n",
			  convert_mode->intput_color_encoding, convert_mode->is_input_yuv,
			  convert_mode->is_input_full_range);
		DRM_ERROR("output: colorspace=%d, yuv=%d, full_range=%d\n",
			  convert_mode->output_color_encoding, convert_mode->is_output_yuv,
			  convert_mode->is_output_full_range);
		return -EINVAL;
	}

	if (csc_cfg)
		ret = csc_calc_adjust_output_coef(convert_mode, csc_cfg,
						  csc_coef, &out_matrix, &out_dc);
	else
		ret = csc_calc_default_output_coef(convert_mode, csc_coef, &out_matrix,
					   &out_dc);

	rockchip_swap_color_channel(convert_mode, csc_simple_coef, &out_matrix, &out_dc);

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

	if (get_csc_version(convert_mode->plat) == RK_PQ_CSC_V1) {
		csc_simple_coef->csc_dc0 = csc_simple_round(csc_simple_coef->csc_dc0, bit_num);
		csc_simple_coef->csc_dc1 = csc_simple_round(csc_simple_coef->csc_dc1, bit_num);
		csc_simple_coef->csc_dc2 = csc_simple_round(csc_simple_coef->csc_dc2, bit_num);
	}
	csc_simple_coef->range_type = convert_mode->is_output_full_range;

	return ret;
}
