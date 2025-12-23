// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2023 Rockchip Electronics Co., Ltd. */

#include "vpss.h"
#include "common.h"
#include "stream.h"
#include "dev.h"
#include "vpss_offline.h"
#include "hw.h"
#include "procfs.h"
#include "regs.h"

struct irqs_data {
	const char *name;
	irqreturn_t (*irq_hdl)(int irq, void *ctx);
};

const s16 rkvpss_zme_tap8_coe[11][17][8] = {
	{//>=2.667
		{4, -12, 20, 488, 20, -12, 4, 0},
		{4, -8, 8, 484, 36, -16, 4, 0},
		{4, -4, -4, 476, 52, -20, 8, 0},
		{0, 0, -16, 480, 68, -28, 8, 0},
		{0, 4, -24, 472, 84, -32, 8, 0},
		{0, 4, -36, 468, 100, -36, 12, 0},
		{0, 8, -44, 456, 120, -40, 12, 0},
		{0, 12, -52, 448, 136, -44, 12, 0},
		{0, 12, -56, 436, 156, -48, 16, -4},
		{-4, 16, -60, 424, 176, -52, 16, -4},
		{-4, 16, -64, 412, 196, -56, 16, -4},
		{-4, 16, -68, 400, 216, -60, 16, -4},
		{-4, 20, -72, 380, 236, -64, 20, -4},
		{-4, 20, -72, 364, 256, -68, 20, -4},
		{-4, 20, -72, 348, 272, -68, 20, -4},
		{-4, 20, -72, 332, 292, -72, 20, -4},
		{-4, 20, -72, 312, 312, -72, 20, -4},
	},
	{//>=2
		{8, -24, 44, 456, 44, -24, 8, 0},
		{8, -20, 28, 460, 56, -28, 8, 0},
		{8, -16, 16, 452, 72, -32, 12, 0},
		{4, -12, 8, 448, 88, -36, 12, 0},
		{4, -8, -4, 444, 104, -40, 12, 0},
		{4, -8, -16, 444, 120, -44, 12, 0},
		{4, -4, -24, 432, 136, -48, 16, 0},
		{4, 0, -32, 428, 152, -52, 16, -4},
		{0, 4, -40, 424, 168, -56, 16, -4},
		{0, 4, -44, 412, 188, -60, 16, -4},
		{0, 8, -52, 400, 204, -60, 16, -4},
		{0, 8, -56, 388, 224, -64, 16, -4},
		{0, 12, -60, 372, 240, -64, 16, -4},
		{0, 12, -64, 356, 264, -68, 16, -4},
		{0, 12, -64, 340, 280, -68, 16, -4},
		{0, 16, -68, 324, 296, -68, 16, -4},
		{0, 16, -68, 308, 308, -68, 16, 0},
	},
	{//>=1.5
		{12, -32, 64, 424, 64, -32, 12, 0},
		{8, -32, 52, 432, 76, -36, 12, 0},
		{8, -28, 40, 432, 88, -40, 12, 0},
		{8, -24, 28, 428, 104, -44, 12, 0},
		{8, -20, 16, 424, 120, -48, 12, 0},
		{8, -16, 8, 416, 132, -48, 12, 0},
		{4, -16, -4, 420, 148, -52, 12, 0},
		{4, -12, -12, 412, 164, -56, 12, 0},
		{4, -8, -20, 400, 180, -56, 12, 0},
		{4, -4, -28, 388, 196, -56, 12, 0},
		{4, -4, -32, 380, 212, -60, 12, 0},
		{4, 0, -40, 368, 228, -60, 12, 0},
		{4, 0, -44, 356, 244, -60, 12, 0},
		{0, 4, -48, 344, 260, -60, 12, 0},
		{0, 4, -52, 332, 276, -60, 12, 0},
		{0, 8, -56, 320, 292, -60, 8, 0},
		{0, 8, -56, 304, 304, -56, 8, 0},
	},
	{//>1
		{12, -40, 84, 400, 84, -40, 12, 0},
		{12, -40, 72, 404, 96, -44, 12, 0},
		{12, -36, 60, 404, 108, -48, 12, 0},
		{8, -32, 48, 404, 120, -48, 12, 0},
		{8, -32, 36, 404, 136, -52, 12, 0},
		{8, -28, 28, 396, 148, -52, 12, 0},
		{8, -24, 16, 392, 160, -52, 12, 0},
		{8, -20, 8, 384, 176, -56, 12, 0},
		{8, -20, 0, 384, 188, -56, 8, 0},
		{8, -16, -8, 372, 204, -56, 8, 0},
		{8, -12, -16, 364, 216, -56, 8, 0},
		{4, -12, -20, 356, 232, -56, 8, 0},
		{4, -8, -28, 348, 244, -56, 8, 0},
		{4, -8, -32, 332, 264, -52, 4, 0},
		{4, -4, -36, 324, 272, -52, 4, 0},
		{4, 0, -40, 312, 280, -48, 0, 4},
		{4, 0, -44, 296, 296, -44, 0, 4},
	},
	{//==1
		{0, 0, 0, 511, 0, 0, 0, 0},
		{-1, 3, -12, 511, 14, -4, 1, 0},
		{-2, 6, -23, 509, 28, -8, 2, 0},
		{-2, 9, -33, 503, 44, -12, 3, 0},
		{-3, 11, -41, 496, 61, -16, 4, 0},
		{-3, 13, -48, 488, 79, -21, 5, -1},
		{-3, 14, -54, 477, 98, -25, 7, -2},
		{-4, 16, -59, 465, 118, -30, 8, -2},
		{-4, 17, -63, 451, 138, -35, 9, -1},
		{-4, 18, -66, 437, 158, -39, 10, -2},
		{-4, 18, -68, 421, 180, -44, 11, -2},
		{-4, 18, -69, 404, 201, -48, 13, -3},
		{-4, 18, -70, 386, 222, -52, 14, -2},
		{-4, 18, -70, 368, 244, -56, 15, -3},
		{-4, 18, -69, 348, 265, -59, 16, -3},
		{-4, 18, -67, 329, 286, -63, 16, -3},
		{-3, 17, -65, 307, 307, -65, 17, -3},
	},
	{//>=0.833
		{-16, 0, 145, 254, 145, 0, -16, 0},
		{-16, -2, 140, 253, 151, 3, -17, 0},
		{-15, -5, 135, 253, 157, 5, -18, 0},
		{-14, -7, 129, 252, 162, 8, -18, 0},
		{-13, -9, 123, 252, 167, 11, -19, 0},
		{-13, -11, 118, 250, 172, 15, -19, 0},
		{-12, -12, 112, 250, 177, 18, -20, -1},
		{-11, -14, 107, 247, 183, 21, -20, -1},
		{-10, -15, 101, 245, 188, 25, -21, -1},
		{-9, -16, 96, 243, 192, 29, -21, -2},
		{-8, -18, 90, 242, 197, 33, -22, -2},
		{-8, -19, 85, 239, 202, 37, -22, -2},
		{-7, -19, 80, 236, 206, 41, -22, -3},
		{-7, -20, 75, 233, 210, 46, -22, -3},
		{-6, -21, 69, 230, 215, 50, -22, -3},
		{-5, -21, 65, 226, 219, 55, -22, -5},
		{-5, -21, 60, 222, 222, 60, -21, -5},
	},
	{//>=0.7
		{-16, 0, 145, 254, 145, 0, -16, 0},
		{-16, -2, 140, 253, 151, 3, -17, 0},
		{-15, -5, 135, 253, 157, 5, -18, 0},
		{-14, -7, 129, 252, 162, 8, -18, 0},
		{-13, -9, 123, 252, 167, 11, -19, 0},
		{-13, -11, 118, 250, 172, 15, -19, 0},
		{-12, -12, 112, 250, 177, 18, -20, -1},
		{-11, -14, 107, 247, 183, 21, -20, -1},
		{-10, -15, 101, 245, 188, 25, -21, -1},
		{-9, -16, 96, 243, 192, 29, -21, -2},
		{-8, -18, 90, 242, 197, 33, -22, -2},
		{-8, -19, 85, 239, 202, 37, -22, -2},
		{-7, -19, 80, 236, 206, 41, -22, -3},
		{-7, -20, 75, 233, 210, 46, -22, -3},
		{-6, -21, 69, 230, 215, 50, -22, -3},
		{-5, -21, 65, 226, 219, 55, -22, -5},
		{-5, -21, 60, 222, 222, 60, -21, -5},
	},
	{//>=0.5
		{-16, 0, 145, 254, 145, 0, -16, 0},
		{-16, -2, 140, 253, 151, 3, -17, 0},
		{-15, -5, 135, 253, 157, 5, -18, 0},
		{-14, -7, 129, 252, 162, 8, -18, 0},
		{-13, -9, 123, 252, 167, 11, -19, 0},
		{-13, -11, 118, 250, 172, 15, -19, 0},
		{-12, -12, 112, 250, 177, 18, -20, -1},
		{-11, -14, 107, 247, 183, 21, -20, -1},
		{-10, -15, 101, 245, 188, 25, -21, -1},
		{-9, -16, 96, 243, 192, 29, -21, -2},
		{-8, -18, 90, 242, 197, 33, -22, -2},
		{-8, -19, 85, 239, 202, 37, -22, -2},
		{-7, -19, 80, 236, 206, 41, -22, -3},
		{-7, -20, 75, 233, 210, 46, -22, -3},
		{-6, -21, 69, 230, 215, 50, -22, -3},
		{-5, -21, 65, 226, 219, 55, -22, -5},
		{-5, -21, 60, 222, 222, 60, -21, -5},
	},
	{//>=0.33
		{-18, 18, 144, 226, 144, 19, -17, -4},
		{-17, 16, 139, 226, 148, 21, -17, -4},
		{-17, 13, 135, 227, 153, 24, -18, -5},
		{-17, 11, 131, 226, 157, 27, -18, -5},
		{-17, 9, 126, 225, 161, 30, -17, -5},
		{-16, 6, 122, 225, 165, 33, -17, -6},
		{-16, 4, 118, 224, 169, 37, -17, -7},
		{-16, 2, 113, 224, 173, 40, -17, -7},
		{-15, 0, 109, 222, 177, 43, -17, -7},
		{-15, -1, 104, 220, 181, 47, -16, -8},
		{-14, -3, 100, 218, 185, 51, -16, -9},
		{-14, -5, 96, 217, 188, 54, -15, -9},
		{-14, -6, 91, 214, 192, 58, -14, -9},
		{-13, -7, 87, 212, 195, 62, -14, -10},
		{-13, -9, 83, 210, 198, 66, -13, -10},
		{-12, -10, 79, 207, 201, 70, -12, -11},
		{-12, -11, 74, 205, 205, 74, -11, -12},
	},
	{//>=0.25
		{14, 66, 113, 133, 113, 66, 14, -7},
		{12, 65, 112, 133, 114, 68, 15, -7},
		{11, 63, 111, 132, 115, 70, 17, -7},
		{10, 62, 110, 132, 116, 71, 18, -7},
		{8, 60, 108, 132, 118, 73, 20, -7},
		{7, 58, 107, 132, 119, 75, 21, -7},
		{6, 56, 106, 132, 120, 76, 23, -7},
		{5, 55, 105, 131, 121, 78, 24, -7},
		{4, 53, 103, 131, 122, 80, 26, -7},
		{3, 51, 102, 131, 122, 81, 28, -6},
		{2, 50, 101, 130, 123, 83, 29, -6},
		{1, 48, 99, 131, 124, 84, 31, -6},
		{0, 46, 98, 129, 125, 86, 33, -5},
		{-1, 45, 97, 128, 126, 88, 34, -5},
		{-2, 43, 95, 130, 126, 89, 36, -5},
		{-3, 41, 94, 128, 127, 91, 38, -4},
		{-3, 39, 92, 128, 128, 92, 39, -3},
	},
	{//others
		{39, 69, 93, 102, 93, 69, 39, 8},
		{38, 68, 92, 102, 93, 70, 40, 9},
		{37, 67, 91, 102, 93, 71, 41, 10},
		{36, 66, 91, 101, 94, 71, 42, 11},
		{35, 65, 90, 102, 94, 72, 43, 11},
		{34, 64, 89, 102, 94, 73, 44, 12},
		{33, 63, 88, 101, 95, 74, 45, 13},
		{32, 62, 88, 100, 95, 75, 46, 14},
		{31, 62, 87, 100, 95, 75, 47, 15},
		{30, 61, 86, 99, 96, 76, 48, 16},
		{29, 60, 86, 98, 96, 77, 49, 17},
		{28, 59, 85, 98, 96, 78, 50, 18},
		{27, 58, 84, 99, 97, 78, 50, 19},
		{26, 57, 83, 99, 97, 79, 51, 20},
		{25, 56, 83, 98, 97, 80, 52, 21},
		{24, 55, 82, 97, 98, 81, 53, 22},
		{23, 54, 81, 98, 98, 81, 54, 23},
	}
};

const s16 rkvpss_zme_tap6_coe[11][17][8] = {
	{//>=2.667
		{-12, 20, 492, 20, -12, 4, 0, 0},
		{-8, 8, 488, 36, -16, 4, 0, 0},
		{-4, -4, 488, 48, -20, 4, 0, 0},
		{0, -16, 484, 64, -24, 4, 0, 0},
		{0, -24, 476, 80, -28, 8, 0, 0},
		{4, -32, 464, 100, -32, 8, 0, 0},
		{8, -40, 456, 116, -36, 8, 0, 0},
		{8, -48, 448, 136, -40, 8, 0, 0},
		{12, -52, 436, 152, -44, 8, 0, 0},
		{12, -60, 424, 172, -48, 12, 0, 0},
		{12, -64, 412, 192, -52, 12, 0, 0},
		{16, -64, 392, 212, -56, 12, 0, 0},
		{16, -68, 380, 232, -60, 12, 0, 0},
		{16, -68, 360, 248, -60, 16, 0, 0},
		{16, -68, 344, 268, -64, 16, 0, 0},
		{16, -68, 328, 288, -68, 16, 0, 0},
		{16, -68, 308, 308, -68, 16, 0, 0},
	},
	{//>=2
		{-20, 40, 468, 40, -20, 4, 0, 0},
		{-16, 28, 464, 56, -24, 4, 0, 0},
		{-16, 16, 464, 68, -28, 8, 0, 0},
		{-12, 4, 460, 84, -32, 8, 0, 0},
		{-8, -4, 452, 100, -36, 8, 0, 0},
		{-4, -12, 444, 116, -40, 8, 0, 0},
		{-4, -24, 440, 136, -44, 8, 0, 0},
		{0, -32, 432, 152, -48, 8, 0, 0},
		{0, -36, 416, 168, -48, 12, 0, 0},
		{4, -44, 408, 184, -52, 12, 0, 0},
		{4, -48, 400, 200, -56, 12, 0, 0},
		{8, -52, 380, 220, -56, 12, 0, 0},
		{8, -56, 372, 236, -60, 12, 0, 0},
		{8, -60, 356, 256, -60, 12, 0, 0},
		{12, -60, 340, 268, -60, 12, 0, 0},
		{12, -60, 324, 288, -64, 12, 0, 0},
		{12, -64, 308, 308, -64, 12, 0, 0},
	},
	{//>=1.5
		{-28, 60, 440, 60, -28, 8, 0, 0},
		{-28, 48, 440, 76, -32, 8, 0, 0},
		{-24, 36, 440, 88, -36, 8, 0, 0},
		{-20, 28, 432, 104, -40, 8, 0, 0},
		{-16, 16, 428, 116, -40, 8, 0, 0},
		{-16, 4, 428, 132, -44, 8, 0, 0},
		{-12, -4, 420, 148, -48, 8, 0, 0},
		{-8, -12, 408, 164, -48, 8, 0, 0},
		{-8, -20, 404, 180, -52, 8, 0, 0},
		{-4, -24, 388, 196, -52, 8, 0, 0},
		{-4, -32, 384, 212, -56, 8, 0, 0},
		{0, -36, 372, 224, -56, 8, 0, 0},
		{0, -40, 360, 240, -56, 8, 0, 0},
		{4, -44, 344, 256, -56, 8, 0, 0},
		{4, -48, 332, 272, -56, 8, 0, 0},
		{4, -52, 316, 292, -56, 8, 0, 0},
		{8, -52, 300, 300, -52, 8, 0, 0},
	},
	{//>1
		{-36, 80, 420, 80, -36, 4, 0, 0},
		{-32, 68, 412, 92, -36, 8, 0, 0},
		{-28, 56, 412, 104, -40, 8, 0, 0},
		{-28, 44, 412, 116, -40, 8, 0, 0},
		{-24, 36, 404, 132, -44, 8, 0, 0},
		{-24, 24, 404, 144, -44, 8, 0, 0},
		{-20, 16, 396, 160, -48, 8, 0, 0},
		{-16, 8, 388, 172, -48, 8, 0, 0},
		{-16, 0, 380, 188, -48, 8, 0, 0},
		{-12, -8, 376, 200, -48, 4, 0, 0},
		{-12, -12, 364, 216, -48, 4, 0, 0},
		{-8, -20, 356, 228, -48, 4, 0, 0},
		{-8, -24, 344, 244, -48, 4, 0, 0},
		{-4, -32, 332, 260, -48, 4, 0, 0},
		{-4, -36, 320, 272, -44, 4, 0, 0},
		{0, -40, 308, 288, -44, 0, 0, 0},
		{0, -40, 296, 296, -40, 0, 0, 0},
	},
	{//==1
		{0, 0, 511, 0, 0, 0, 0, 0},
		{3, -12, 511, 13, -3, 0, 0, 0},
		{6, -22, 507, 28, -7, 0, 0, 0},
		{8, -32, 502, 44, -11, 1, 0, 0},
		{10, -40, 495, 61, -15, 1, 0, 0},
		{11, -47, 486, 79, -19, 2, 0, 0},
		{12, -53, 476, 98, -24, 3, 0, 0},
		{13, -58, 464, 117, -28, 4, 0, 0},
		{14, -62, 451, 137, -33, 5, 0, 0},
		{15, -65, 437, 157, -38, 6, 0, 0},
		{15, -67, 420, 179, -42, 7, 0, 0},
		{15, -68, 404, 200, -46, 7, 0, 0},
		{14, -68, 386, 221, -50, 9, 0, 0},
		{14, -68, 367, 243, -54, 10, 0, 0},
		{14, -67, 348, 264, -58, 11, 0, 0},
		{13, -66, 328, 286, -61, 12, 0, 0},
		{13, -63, 306, 306, -63, 13, 0, 0},
	},
	{//>=0.833
		{-31, 104, 362, 104, -31, 4, 0, 0},
		{-30, 94, 362, 114, -32, 4, 0, 0},
		{-29, 84, 361, 125, -32, 3, 0, 0},
		{-28, 75, 359, 136, -33, 3, 0, 0},
		{-27, 66, 356, 147, -33, 3, 0, 0},
		{-25, 57, 353, 158, -33, 2, 0, 0},
		{-24, 49, 349, 169, -33, 2, 0, 0},
		{-22, 41, 344, 180, -32, 1, 0, 0},
		{-20, 33, 339, 191, -31, 0, 0, 0},
		{-19, 26, 333, 203, -30, -1, 0, 0},
		{-17, 19, 327, 214, -29, -2, 0, 0},
		{-16, 13, 320, 225, -27, -3, 0, 0},
		{-14, 7, 312, 236, -25, -4, 0, 0},
		{-13, 1, 305, 246, -22, -5, 0, 0},
		{-11, -4, 295, 257, -19, -6, 0, 0},
		{-10, -8, 286, 267, -16, -7, 0, 0},
		{-9, -12, 277, 277, -12, -9, 0, 0},
	},
	{//>=0.7
		{-31, 104, 362, 104, -31, 4, 0, 0},
		{-30, 94, 362, 114, -32, 4, 0, 0},
		{-29, 84, 361, 125, -32, 3, 0, 0},
		{-28, 75, 359, 136, -33, 3, 0, 0},
		{-27, 66, 356, 147, -33, 3, 0, 0},
		{-25, 57, 353, 158, -33, 2, 0, 0},
		{-24, 49, 349, 169, -33, 2, 0, 0},
		{-22, 41, 344, 180, -32, 1, 0, 0},
		{-20, 33, 339, 191, -31, 0, 0, 0},
		{-19, 26, 333, 203, -30, -1, 0, 0},
		{-17, 19, 327, 214, -29, -2, 0, 0},
		{-16, 13, 320, 225, -27, -3, 0, 0},
		{-14, 7, 312, 236, -25, -4, 0, 0},
		{-13, 1, 305, 246, -22, -5, 0, 0},
		{-11, -4, 295, 257, -19, -6, 0, 0},
		{-10, -8, 286, 267, -16, -7, 0, 0},
		{-9, -12, 277, 277, -12, -9, 0, 0},
	},
	{//>=0.5
		{ -20, 130, 297, 130, -20, -5, 0, 0},
		{ -21, 122, 298, 138, -19, -6, 0, 0},
		{ -22, 115, 297, 146, -17, -7, 0, 0},
		{ -22, 108, 296, 153, -16, -7, 0, 0},
		{ -23, 101, 295, 161, -14, -8, 0, 0},
		{ -23, 93, 294, 169, -12, -9, 0, 0},
		{ -24, 87, 292, 177, -10, -10, 0, 0},
		{ -24, 80, 289, 185, -7, -11, 0, 0},
		{ -24, 73, 286, 193, -4, -12, 0, 0},
		{ -23, 66, 283, 200, -1, -13, 0, 0},
		{ -23, 60, 279, 208, 2, -14, 0, 0},
		{ -23, 54, 276, 215, 5, -15, 0, 0},
		{ -22, 48, 271, 222, 9, -16, 0, 0},
		{ -21, 42, 266, 229, 13, -17, 0, 0},
		{ -21, 37, 261, 236, 17, -18, 0, 0},
		{ -21, 32, 255, 242, 22, -18, 0, 0},
		{ -20, 27, 249, 249, 27, -20, 0, 0},
	},
	{//>=0.33
		{16, 136, 217, 136, 16, -9, 0, 0},
		{13, 132, 217, 141, 18, -9, 0, 0},
		{11, 128, 217, 145, 21, -10, 0, 0},
		{9, 124, 216, 149, 24, -10, 0, 0},
		{7, 119, 216, 153, 27, -10, 0, 0},
		{5, 115, 216, 157, 30, -11, 0, 0},
		{3, 111, 215, 161, 33, -11, 0, 0},
		{1, 107, 214, 165, 36, -11, 0, 0},
		{0, 102, 213, 169, 39, -11, 0, 0},
		{-2, 98, 211, 173, 43, -11, 0, 0},
		{-3, 94, 209, 177, 46, -11, 0, 0},
		{-4, 90, 207, 180, 50, -11, 0, 0},
		{-5, 85, 206, 184, 53, -11, 0, 0},
		{-6, 81, 203, 187, 57, -10, 0, 0},
		{-7, 77, 201, 190, 61, -10, 0, 0},
		{-8, 73, 198, 193, 65, -9, 0, 0},
		{-9, 69, 196, 196, 69, -9, 0, 0},
	},
	{//>=0.25
		{66, 115, 138, 115, 66, 12, 0, 0},
		{64, 114, 136, 116, 68, 14, 0, 0},
		{63, 113, 134, 117, 70, 15, 0, 0},
		{61, 111, 135, 118, 71, 16, 0, 0},
		{59, 110, 133, 119, 73, 18, 0, 0},
		{57, 108, 134, 120, 74, 19, 0, 0},
		{55, 107, 133, 121, 76, 20, 0, 0},
		{53, 105, 133, 121, 78, 22, 0, 0},
		{51, 104, 133, 122, 79, 23, 0, 0},
		{49, 102, 132, 123, 81, 25, 0, 0},
		{47, 101, 132, 124, 82, 26, 0, 0},
		{45, 99, 131, 125, 84, 28, 0, 0},
		{44, 98, 130, 125, 85, 30, 0, 0},
		{42, 96, 130, 126, 87, 31, 0, 0},
		{40, 95, 128, 127, 89, 33, 0, 0},
		{38, 93, 129, 127, 90, 35, 0, 0},
		{36, 92, 128, 128, 92, 36, 0, 0},
	},
	{//others
		{80, 105, 116, 105, 80, 26, 0, 0},
		{79, 104, 115, 105, 81, 28, 0, 0},
		{77, 103, 116, 106, 81, 29, 0, 0},
		{76, 102, 115, 106, 82, 31, 0, 0},
		{74, 101, 115, 106, 83, 33, 0, 0},
		{73, 100, 114, 106, 84, 35, 0, 0},
		{71, 99, 114, 107, 84, 37, 0, 0},
		{70, 98, 113, 107, 85, 39, 0, 0},
		{68, 98, 113, 107, 86, 40, 0, 0},
		{67, 97, 112, 108, 86, 42, 0, 0},
		{65, 96, 112, 108, 87, 44, 0, 0},
		{63, 95, 112, 108, 88, 46, 0, 0},
		{62, 94, 112, 108, 88, 48, 0, 0},
		{60, 93, 111, 109, 89, 50, 0, 0},
		{58, 93, 111, 109, 90, 51, 0, 0},
		{57, 92, 110, 110, 90, 53, 0, 0},
		{55, 91, 110, 110, 91, 55, 0, 0},
	}
};

void rkvpss_hw_write(struct rkvpss_hw_dev *hw, u32 reg, u32 val)
{
	unsigned long lock_flags = 0;

	if (IS_SYNC_REG(reg))
		spin_lock_irqsave(&hw->reg_lock, lock_flags);
	writel(val, hw->base_addr + reg);
	if (IS_SYNC_REG(reg))
		spin_unlock_irqrestore(&hw->reg_lock, lock_flags);
}

u32 rkvpss_hw_read(struct rkvpss_hw_dev *hw, u32 reg)
{
	unsigned long lock_flags = 0;
	u32 val;

	if (IS_SYNC_REG(reg))
		spin_lock_irqsave(&hw->reg_lock, lock_flags);
	val = readl(hw->base_addr + reg);
	if (IS_SYNC_REG(reg))
		spin_unlock_irqrestore(&hw->reg_lock, lock_flags);

	return val;
}

void rkvpss_hw_set_bits(struct rkvpss_hw_dev *hw, u32 reg, u32 mask, u32 val)
{
	unsigned long lock_flags = 0;
	u32 tmp;

	if (IS_SYNC_REG(reg))
		spin_lock_irqsave(&hw->reg_lock, lock_flags);
	tmp = readl(hw->base_addr + reg) & ~mask;
	writel(val | tmp, hw->base_addr + reg);
	if (IS_SYNC_REG(reg))
		spin_unlock_irqrestore(&hw->reg_lock, lock_flags);
}

void rkvpss_hw_clear_bits(struct rkvpss_hw_dev *hw, u32 reg, u32 mask)
{
	unsigned long lock_flags = 0;
	u32 val;

	if (IS_SYNC_REG(reg))
		spin_lock_irqsave(&hw->reg_lock, lock_flags);
	val = readl(hw->base_addr + reg) & ~mask;
	writel(val, hw->base_addr + reg);
	if (IS_SYNC_REG(reg))
		spin_unlock_irqrestore(&hw->reg_lock, lock_flags);
}

void rkvpss_soft_reset(struct rkvpss_hw_dev *hw)
{
	writel(RKVPSS_SOFT_RST, hw->base_addr + RKVPSS_VPSS_RESET);
	if (hw->reset) {
		reset_control_assert(hw->reset);
		udelay(20);
		reset_control_deassert(hw->reset);
		udelay(20);
	}

	/* refresh iommu after reset */
	if (hw->is_mmu) {
		rockchip_iommu_disable(hw->dev);
		rockchip_iommu_enable(hw->dev);
	}

	rkvpss_hw_set_bits(hw, RKVPSS_VPSS_CTRL, RKVPSS_ACK_FRM_PRO_DIS,
			   RKVPSS_ACK_FRM_PRO_DIS);
	rkvpss_hw_write(hw, RKVPSS_VPSS_IRQ_CFG, 0x3fff);
	rkvpss_hw_write(hw, RKVPSS_MI_IMSC, 0xd0000000);
	rkvpss_hw_set_bits(hw, RKVPSS_VPSS_ONLINE, RKVPSS_ONLINE_MODE_MASK,
					   RKVPSS_ISP2VPSS_ONLINE2);
	rkvpss_hw_write(hw, RKVPSS_VPSS_UPDATE, RKVPSS_CFG_FORCE_UPD);
}

int rkvpss_get_zme_tap_coe_index(int ratio)
{
	int idx;

	if (ratio >= 26670)
		idx = 0;
	else if (ratio >= 20000)
		idx = 1;
	else if (ratio >= 15000)
		idx = 2;
	else if (ratio > 10000)
		idx = 3;
	else if (ratio == 10000)
		idx = 4;
	else if (ratio >= 8333)
		idx = 5;
	else if (ratio >= 7000)
		idx = 6;
	else if (ratio >= 5000)
		idx = 7;
	else if (ratio >= 3300)
		idx = 8;
	else if (ratio >= 2500)
		idx = 9;
	else
		idx = 10;

	return idx;
}

void rkvpss_cmsc_slop(struct rkvpss_cmsc_point *p0,
		      struct rkvpss_cmsc_point *p1,
		      int *k, int *hor)
{
	int x0 = p0->x, y0 = p0->y;
	int x1 = p1->x, y1 = p1->y;
	int i, slp, sign_flag = 0, slp_bit = 13;
	int highest1 = 0, k_fix, pow;

	if (y0 == y1) {
		/* horizontal line */
		*hor = 1;
		slp = 0;
	} else {
		*hor = 0;
		slp = (int)((x1 - x0) * (1 << slp_bit) / (y1 - y0));
		if (slp < 0) {
			sign_flag = 1;
			slp = -slp;
		}
		slp = min_t(int, slp, (1 << slp_bit * 2) - 1);
	}

	for (i = slp_bit * 2 - 1; i >= 0; i--) {
		if (slp >> i) {
			highest1 = i;
			break;
		}
	}
	if (highest1 >= slp_bit) {
		pow = highest1 - slp_bit + 1;
		k_fix = slp >> pow;
	} else {
		pow = 0;
		k_fix = slp;
	}
	*k = (sign_flag << (slp_bit + 4)) + (pow << slp_bit) + k_fix;
}

static inline bool is_iommu_enable(struct device *dev)
{
	struct device_node *iommu;

	iommu = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!iommu) {
		dev_info(dev, "no iommu attached, using non-iommu buffers\n");
		return false;
	} else if (!of_device_is_available(iommu)) {
		dev_info(dev, "iommu is disabled, using non-iommu buffers\n");
		of_node_put(iommu);
		return false;
	}
	of_node_put(iommu);

	return true;
}

static void disable_sys_clk(struct rkvpss_hw_dev *dev)
{
	int i;

	for (i = 0; i < dev->clks_num; i++)
		clk_disable_unprepare(dev->clks[i]);
}

static int enable_sys_clk(struct rkvpss_hw_dev *dev)
{
	int i, ret = -EINVAL;

	for (i = 0; i < dev->clks_num; i++) {
		ret = clk_prepare_enable(dev->clks[i]);
		if (ret < 0)
			goto err;
	}
	return 0;
err:
	for (--i; i >= 0; --i)
		clk_disable_unprepare(dev->clks[i]);
	return ret;
}

static irqreturn_t vpss_irq_hdl(int irq, void *ctx)
{
	struct device *dev = ctx;
	struct rkvpss_hw_dev *hw_dev = dev_get_drvdata(dev);
	struct rkvpss_device *vpss = hw_dev->vpss[hw_dev->cur_dev_id];
	unsigned int mis_val;

	mis_val = rkvpss_hw_read(hw_dev, RKVPSS_VPSS_MIS);
	if (mis_val) {
		rkvpss_hw_write(hw_dev, RKVPSS_VPSS_ICR, mis_val);
		if (mis_val & RKVPSS_ISP_ALL_FRM_END)
			rkvpss_isr(vpss, mis_val);
		if (mis_val & RKVPSS_ALL_FRM_END)
			rkvpss_offline_irq(hw_dev, mis_val);
	}
	return IRQ_HANDLED;
}

static irqreturn_t mi_irq_hdl(int irq, void *ctx)
{
	struct device *dev = ctx;
	struct rkvpss_hw_dev *hw_dev = dev_get_drvdata(dev);
	struct rkvpss_device *vpss = hw_dev->vpss[hw_dev->cur_dev_id];
	unsigned int mis_val;

	mis_val = rkvpss_hw_read(hw_dev, RKVPSS_MI_MIS);
	if (mis_val) {
		if (mis_val & RKVPSS_MI_BUS_ERR)
			dev_err(dev, "axi bus error\n");
		if (is_vpss_v20(hw_dev) &&
		    mis_val & RKVPSS2X_MI_RD_FBCD_ERR)
			dev_err(dev, "rd_fbcd_err\n");

		rkvpss_hw_write(hw_dev, RKVPSS_MI_ICR, mis_val);
		if (vpss)
			rkvpss_mi_isr(vpss, mis_val);
	}
	return IRQ_HANDLED;
}

static struct irqs_data vpss_irqs[] = {
	{"vpss_irq", vpss_irq_hdl},
	{"mi_irq", mi_irq_hdl},
};

static const char * const vpss_clks[] = {
	"clk_vpss",
	"aclk_vpss",
	"hclk_vpss",
};

static const struct vpss_match_data rk3576_vpss_match_data = {
	.irqs = vpss_irqs,
	.num_irqs = ARRAY_SIZE(vpss_irqs),
	.clks = vpss_clks,
	.clks_num = ARRAY_SIZE(vpss_clks),
	.vpss_ver = VPSS_V10,
};

static const struct vpss_match_data rv1126b_vpss_match_data = {
	.irqs = vpss_irqs,
	.num_irqs = ARRAY_SIZE(vpss_irqs),
	.clks = vpss_clks,
	.clks_num = ARRAY_SIZE(vpss_clks),
	.vpss_ver = VPSS_V20,
};

static const struct of_device_id rkvpss_hw_of_match[] = {
#ifdef CONFIG_VIDEO_ROCKCHIP_VPSS_V10
	{
		.compatible = "rockchip,rk3576-rkvpss",
		.data = &rk3576_vpss_match_data,
	},
#endif
#ifdef CONFIG_VIDEO_ROCKCHIP_VPSS_V20
	{
		.compatible = "rockchip,rv1126b-rkvpss",
		.data = &rv1126b_vpss_match_data,
	},
#endif
	{},
};

void rkvpss_hw_reg_save(struct rkvpss_hw_dev *dev)
{
	void *buf = dev->sw_reg;

	dev_info(dev->dev, "%s\n", __func__);
	memcpy_fromio(buf, dev->base_addr, RKVPSS_SW_REG_SIZE);
}

void rkvpss_hw_reg_restore(struct rkvpss_hw_dev *dev)
{
	void __iomem *base = dev->base_addr;
	void *reg_buf = dev->sw_reg;
	u32 val, *reg, i, range;

	dev_info(dev->dev, "%s\n", __func__);

	/* cmsc */
	for (i = RKVPSS_CMSC_BASE; i <= RKVPSS_CMSC_WIN7_L3_SLP; i += 4) {
		if (i == RKVPSS_CMSC_UPDATE)
			continue;
		reg = reg_buf + i;
		writel(*reg, base + i);
	}
	val = RKVPSS_CMSC_GEN_UPD | RKVPSS_CMSC_UPDATE;
	writel(val, base + RKVPSS_CMSC_UPDATE);

	/* crop1 */
	range = is_vpss_v20(dev) ? RKVPSS2X_CROP1_5_SIZE : RKVPSS_CROP1_3_V_SIZE;
	for (i = RKVPSS_CROP1_CTRL; i <= range; i += 4) {
		if (i == RKVPSS_CROP1_UPDATE)
			continue;
		reg = reg_buf + i;
		writel(*reg, base + i);
	}
	val = RKVPSS_CROP_GEN_UPD | RKVPSS_CROP_FORCE_UPD;
	writel(val, base + RKVPSS_CROP1_UPDATE);
	writel(val, base + RKVPSS_CROP1_UPDATE);

	/* scale */
	if (is_vpss_v10(dev)) {
		for (i = RKVPSS_ZME_BASE; i <= RKVPSS_ZME_UV_YSCL_FACTOR; i += 4) {
			if (i == RKVPSS_ZME_UPDATE)
				continue;
			reg = reg_buf + i;
			writel(*reg, base + i);
		}
		val = RKVPSS_ZME_GEN_UPD | RKVPSS_ZME_FORCE_UPD;
		writel(val, base + RKVPSS_ZME_UPDATE);
	}
	for (i = RKVPSS_SCALE1_BASE; i <= RKVPSS_SCALE1_IN_CROP_OFFSET; i += 4) {
		if (i == RKVPSS_SCALE1_UPDATE)
			continue;
		reg = reg_buf + i;
		writel(*reg, base + i);
	}
	val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
	writel(val, base + RKVPSS_SCALE1_UPDATE);

	for (i = RKVPSS_SCALE2_BASE; i <= RKVPSS_SCALE2_IN_CROP_OFFSET; i += 4) {
		if (i == RKVPSS_SCALE2_UPDATE)
			continue;
		reg = reg_buf + i;
		writel(*reg, base + i);
	}
	val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
	writel(val, base + RKVPSS_SCALE2_UPDATE);

	for (i = RKVPSS_SCALE3_BASE; i <= RKVPSS_SCALE3_IN_CROP_OFFSET; i += 4) {
		if (i == RKVPSS_SCALE3_UPDATE)
			continue;
		reg = reg_buf + i;
		writel(*reg, base + i);
	}
	val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
	writel(val, base + RKVPSS_SCALE3_UPDATE);

	if (is_vpss_v20(dev)) {
		for (i = RKVPSS2X_SCALE0_BASE; i <= RKVPSS2X_SCALE0_IN_CROP_OFFSET; i += 4) {
			if (i == RKVPSS2X_SCALE0_UPDATE)
				continue;
			reg = reg_buf + i;
			writel(*reg, base + i);
		}
		val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
		writel(val, base + RKVPSS2X_SCALE0_UPDATE);

		for (i = RKVPSS2X_SCALE4_BASE; i <= RKVPSS2X_SCALE4_IN_CROP_OFFSET; i += 4) {
			if (i == RKVPSS2X_SCALE4_UPDATE)
				continue;
			reg = reg_buf + i;
			writel(*reg, base + i);
		}
		val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
		writel(val, base + RKVPSS2X_SCALE4_UPDATE);

		for (i = RKVPSS2X_SCALE5_BASE; i <= RKVPSS2X_SCALE5_IN_CROP_OFFSET; i += 4) {
			if (i == RKVPSS2X_SCALE4_UPDATE)
				continue;
			reg = reg_buf + i;
			writel(*reg, base + i);
		}
		val = RKVPSS_SCL_GEN_UPD | RKVPSS_SCL_FORCE_UPD;
		writel(val, base + RKVPSS2X_SCALE5_UPDATE);
	}

	/* mi */
	range = is_vpss_v20(dev) ? RKVPSS2X_MI_CHN5_WR_LINE_CNT : RKVPSS_MI_CHN3_WR_LINE_CNT;
	for (i = RKVPSS_MI_BASE; i <= range; i += 4) {
		if (i >= RKVPSS_MI_RD_CTRL && i <= RKVPSS_MI_RD_Y_HEIGHT_SHD)
			continue;
		if (i == RKVPSS_MI_WR_INIT)
			continue;
		reg = reg_buf + i;
		writel(*reg, base + i);
	}
	val = RKVPSS_MI_FORCE_UPD;
	writel(val, base + RKVPSS_MI_WR_INIT);
	writel(val, base + RKVPSS_MI_WR_INIT);

	/* vpss ctrl */
	for (i = RKVPSS_VPSS_BASE; i <= RKVPSS_VPSS_Y2R_OFF2; i += 4) {
		if (i == RKVPSS_VPSS_UPDATE || i == RKVPSS_VPSS_RESET)
			continue;
		reg = reg_buf + i;
		writel(*reg, base + i);
	}
	val = RKVPSS_CFG_FORCE_UPD | RKVPSS_CFG_GEN_UPD | RKVPSS_MIR_GEN_UPD |
	      RKVPSS_ONLINE2_CHN_FORCE_UPD;
	if (is_vpss_v20(dev))
		val |= RKVPSS_MIR_FORCE_UPD;
	if (!dev->is_ofl_cmsc)
		val |= RKVPSS_MIR_FORCE_UPD;
	writel(val, base + RKVPSS_VPSS_UPDATE);
}

static int rkvpss_hw_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct vpss_match_data *match_data;
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct rkvpss_hw_dev *hw_dev;
	struct resource *res;
	int i, ret, irq;
	bool is_mem_reserved = true;

	match = of_match_node(rkvpss_hw_of_match, node);
	if (IS_ERR(match))
		return PTR_ERR(match);

	hw_dev = devm_kzalloc(dev, sizeof(*hw_dev), GFP_KERNEL);
	if (!hw_dev)
		return -ENOMEM;
	hw_dev->sw_reg = devm_kzalloc(dev, RKVPSS_SW_REG_SIZE, GFP_KERNEL);
	if (!hw_dev->sw_reg)
		return -ENOMEM;
	dev_set_drvdata(dev, hw_dev);
	hw_dev->dev = dev;
	match_data = match->data;
	hw_dev->match_data = match->data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "get resource failed\n");
		ret = -EINVAL;
		goto err;
	}
	hw_dev->base_addr = devm_ioremap_resource(dev, res);
	if (PTR_ERR(hw_dev->base_addr) == -EBUSY) {
		resource_size_t offset = res->start;
		resource_size_t size = resource_size(res);

		hw_dev->base_addr = devm_ioremap(dev, offset, size);
	}
	if (IS_ERR(hw_dev->base_addr)) {
		dev_err(dev, "ioremap failed\n");
		ret = PTR_ERR(hw_dev->base_addr);
		goto err;
	}

	for (i = 0; i < match_data->num_irqs; i++) {
		irq = platform_get_irq_byname(pdev, match_data->irqs[i].name);
		if (irq < 0) {
			dev_err(dev, "no irq %s in dts\n", match_data->irqs[i].name);
			ret = irq;
			goto err;
		}
		ret = devm_request_irq(dev, irq,
				       match_data->irqs[i].irq_hdl,
				       IRQF_SHARED,
				       dev_driver_string(dev),
				       dev);
		if (ret < 0) {
			dev_err(dev, "request %s failed: %d\n",
				match_data->irqs[i].name, ret);
			goto err;
		}
	}

	for (i = 0; i < match_data->clks_num; i++) {
		struct clk *clk = devm_clk_get(dev, match_data->clks[i]);

		if (IS_ERR(clk)) {
			dev_err(dev, "failed to get %s\n",
				match_data->clks[i]);
			ret = PTR_ERR(clk);
			goto err;
		}
		hw_dev->clks[i] = clk;
	}
	hw_dev->clks_num = match_data->clks_num;

	hw_dev->reset = devm_reset_control_array_get(dev, false, false);
	if (IS_ERR(hw_dev->reset)) {
		dev_info(dev, "failed to get cru reset\n");
		hw_dev->reset = NULL;
	}

	hw_dev->dev_num = 0;
	hw_dev->cur_dev_id = 0;
	hw_dev->pre_dev_id = -1;
	hw_dev->vpss_ver = match_data->vpss_ver;
	mutex_init(&hw_dev->dev_lock);
	spin_lock_init(&hw_dev->reg_lock);
	atomic_set(&hw_dev->refcnt, 0);
	INIT_LIST_HEAD(&hw_dev->list);
	for (i = 0; i < vpss_outchn_max(hw_dev->vpss_ver); i++)
		hw_dev->is_ofl_ch[i] = false;
	hw_dev->is_ofl_cmsc = false;
	hw_dev->is_single = true;
	hw_dev->is_dma_contig = true;
	hw_dev->is_shutdown = false;
	hw_dev->is_mmu = is_iommu_enable(dev);
	hw_dev->is_suspend = false;
	hw_dev->is_first = false;
	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		is_mem_reserved = false;
		if (!hw_dev->is_mmu)
			dev_info(dev, "No reserved memory region. default cma area!\n");
	}
	if (hw_dev->is_mmu && !is_mem_reserved)
		hw_dev->is_dma_contig = false;
	hw_dev->mem_ops = &vb2_cma_sg_memops;

	rkvpss_register_offline(hw_dev);

	pm_runtime_enable(&pdev->dev);
	hw_dev->is_probe_end = true;
	return 0;
err:
	return ret;
}

static int rkvpss_hw_remove(struct platform_device *pdev)
{
	struct rkvpss_hw_dev *hw_dev = platform_get_drvdata(pdev);

	rkvpss_unregister_offline(hw_dev);
	pm_runtime_disable(&pdev->dev);
	mutex_destroy(&hw_dev->dev_lock);
	return 0;
}

static void rkvpss_hw_shutdown(struct platform_device *pdev)
{
	struct rkvpss_hw_dev *hw_dev = platform_get_drvdata(pdev);

	hw_dev->is_shutdown = true;
	if (pm_runtime_active(&pdev->dev)) {
		rkvpss_hw_write(hw_dev, RKVPSS_VPSS_IMSC, 0);
		rkvpss_hw_write(hw_dev, RKVPSS_MI_IMSC, 0);
		rkvpss_hw_write(hw_dev, RKVPSS_VPSS_RESET, RKVPSS_SOFT_RST);
	}
}

static int __maybe_unused rkvpss_hw_runtime_suspend(struct device *dev)
{
	struct rkvpss_hw_dev *hw_dev = dev_get_drvdata(dev);
	u32 ret;

	if (rkvpss_debug >= 4)
		dev_info(dev, "%s enter\n", __func__);

	if (dev->power.runtime_status) {
		rkvpss_hw_write(hw_dev, RKVPSS_MI_IMSC, 0);
		rkvpss_hw_write(hw_dev, RKVPSS_VPSS_IMSC, 0);
	} else {
		rkvpss_hw_reg_save(hw_dev);
		hw_dev->is_suspend = true;
		if (hw_dev->ofl_dev.pm_need_wait) {
			ret = wait_for_completion_timeout(&hw_dev->ofl_dev.pm_cmpl,
							  msecs_to_jiffies(200));
			if (!ret)
				dev_info(dev, "%s pm wait offline timeout\n", __func__);
		}
	}

	disable_sys_clk(hw_dev);

	if (rkvpss_debug >= 4)
		dev_info(dev, "%s exit\n", __func__);

	return 0;
}

static int __maybe_unused rkvpss_hw_runtime_resume(struct device *dev)
{
	struct rkvpss_hw_dev *hw_dev = dev_get_drvdata(dev);
	void __iomem *base = hw_dev->base_addr;
	int i;

	if (rkvpss_debug >= 4)
		dev_info(dev, "%s enter\n", __func__);

	enable_sys_clk(hw_dev);
	rkvpss_soft_reset(hw_dev);

	if (dev->power.runtime_status) {
		for (i = 0; i < hw_dev->dev_num; i++) {
			struct rkvpss_device *vpss = hw_dev->vpss[i];

			if (!vpss || !vpss->is_probe_end)
				continue;
			if (vpss->sw_base_addr) {
				void *buf = vpss->sw_base_addr;

				memset(buf, 0, RKVPSS_SW_REG_SIZE_MAX);
				memcpy_fromio(buf, base, RKVPSS_SW_REG_SIZE);
			}
		}
		hw_dev->is_first = true;
	} else {
		rkvpss_hw_reg_restore(hw_dev);
		hw_dev->is_suspend = false;
	}

	if (rkvpss_debug >= 4)
		dev_info(dev, "%s exit\n", __func__);

	return 0;
}

static const struct dev_pm_ops rkvpss_hw_pm_ops = {
	LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(rkvpss_hw_runtime_suspend,
			   rkvpss_hw_runtime_resume, NULL)
};

static struct platform_driver rkvpss_hw_drv = {
	.driver = {
		.name = "rkvpss_hw",
		.of_match_table = of_match_ptr(rkvpss_hw_of_match),
		.pm = &rkvpss_hw_pm_ops,
	},
	.probe = rkvpss_hw_probe,
	.remove = rkvpss_hw_remove,
	.shutdown = rkvpss_hw_shutdown,
};

static int __init rkvpss_hw_drv_init(void)
{
	int ret;

	ret = platform_driver_register(&rkvpss_hw_drv);
	if (!ret)
		ret = platform_driver_register(&rkvpss_plat_drv);
	return ret;
}

static void __exit rkvpss_hw_drv_exit(void)
{
	platform_driver_unregister(&rkvpss_plat_drv);
	platform_driver_unregister(&rkvpss_hw_drv);
}

module_init(rkvpss_hw_drv_init);
module_exit(rkvpss_hw_drv_exit);
