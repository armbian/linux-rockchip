/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKAVSP_REGS_H
#define _RKAVSP_REGS_H

#define ALIGN_RASTER(w)                 (((w + 15) / 16) * 16)
#define ALIGN_QUAD(w)                   (((w * 3 + 15) / 16) * 16)

#define DCP_STD_MSK                     0x3FFF
#define AVSP_FORCE_UPD                  0x1
#define AVSP_ST                         0x1
#define AVSP_BYPASS_OPEN                0x1
#define AVSP_BYPASS_OFF                 0x0

/* AVSP FMT DEFINE */
#define AVSP_MODE_RASTER                0x0
#define AVSP_MODE_TILE                  0x1
#define AVSP_MODE_FBCE                  0x2
#define AVSP_MODE_QUAD                  0x3

/* AVSP_DCP_CORE_CTRL */
#define SW_DCP_BYPASS(x)                ((x & 0x1) << 7)
#define SW_DCP_WR_MODE(x)               ((x & 0x3) << 5)
#define SW_DCP_RD_MODE(x)               ((x & 0x3) << 3)
#define SW_DCP_BAND_NUM(x)              (x & 0x7)

/* AVSP_SRC_SIZE */
#define SW_AVSP_SRC_WIDTH(x)            ((x) & 0x07ff)
#define Sw_AVSP_SRC_HEIGHT(x)           (((x) & 0x1fff) << 16)

/* AVSP_RD_VIR_STRIDE */
#define AVSP_RD_VIR_STRIDE_Y(x)         ((x) & 0x3fff)
#define AVSP_RD_VIR_STRIDE_C(x)         (((x) & 0x3fff) << 16)

/* AVSP_WR_VIR_STRIDE */
#define AVSP_WR_VIR_STRIDE_Y(x)         ((x) & 0x3fff)
#define AVSP_WR_VIR_STRIDE_C(x)         (((x) & 0x3fff) << 16)

/* AVSP_RCS_CORE_CTRL */
#define SW_RCS_WR_MODE(x)               ((x & 0x3) << 5)
#define SW_RCS_RD_MODE(x)               ((x & 0x3) << 3)
#define SW_RCS_BAND_NUM(x)              (x & 0x7)
#define SW_RCS_FBCE_CTL                 (0x6 << 9)

// AVSP INT
#define DCP_INT                         (0x1 << 25)
#define RCS_INT                         0x00010000
#define SYS_SOFT_RST_DCP                0x00004000
#define SYS_SOFT_RST_VAL                0x00000000
#define SYS_DCP_LGC_CKG_DIS             0x00000001
#define SYS_DCP_RAM_CKG_DIS             0x00000002

/* AVSP_REGS */
#define AVSP_BASE                       0x00000000

#define AVSP_DCP_STRT                   (AVSP_BASE + 0x00300)
#define AVSP_DCP_UPDATE                 (AVSP_BASE + 0x00304)
#define AVSP_DCP_CLK_DIS                (AVSP_BASE + 0x00308)
#define AVSP_DCP_CTRL                   (AVSP_BASE + 0x00310)
#define AVSP_DCP_SIZE                   (AVSP_BASE + 0x00314)
#define AVSP_DCP_RD_VIR_SIZE            (AVSP_BASE + 0x00318)
#define AVSP_DCP_WR_LV0_VIR_SIZE        (AVSP_BASE + 0x0031c)
#define AVSP_DCP_WR_LV1_VIR_SIZE        (AVSP_BASE + 0x00320)
#define AVSP_DCP_WR_LV2_VIR_SIZE        (AVSP_BASE + 0x00324)
#define AVSP_DCP_WR_LV3_VIR_SIZE        (AVSP_BASE + 0x00328)
#define AVSP_DCP_WR_LV4_VIR_SIZE        (AVSP_BASE + 0x0032c)
#define AVSP_DCP_WR_LV5_VIR_SIZE        (AVSP_BASE + 0x00330)
#define AVSP_DCP_RD_Y_BASE              (AVSP_BASE + 0x00334)
#define AVSP_DCP_RD_C_BASE              (AVSP_BASE + 0x00338)
#define AVSP_DCP_LV0_BASE_Y             (AVSP_BASE + 0x0033c)
#define AVSP_DCP_LV1_BASE_Y             (AVSP_BASE + 0x00340)
#define AVSP_DCP_LV2_BASE_Y             (AVSP_BASE + 0x00344)
#define AVSP_DCP_LV3_BASE_Y             (AVSP_BASE + 0x00348)
#define AVSP_DCP_LV4_BASE_Y             (AVSP_BASE + 0x0034c)
#define AVSP_DCP_LV5_BASE_Y             (AVSP_BASE + 0x00350)
#define AVSP_DCP_LV0_BASE_C             (AVSP_BASE + 0x00354)
#define AVSP_DCP_LV1_BASE_C             (AVSP_BASE + 0x00358)
#define AVSP_DCP_LV2_BASE_C             (AVSP_BASE + 0x0035c)
#define AVSP_DCP_LV3_BASE_C             (AVSP_BASE + 0x00360)
#define AVSP_DCP_LV4_BASE_C             (AVSP_BASE + 0x00364)
#define AVSP_DCP_LV5_BASE_C             (AVSP_BASE + 0x00368)
#define AVSP_DCP_INT_EN                 (AVSP_BASE + 0x00390)
#define AVSP_DCP_INT_CLR                (AVSP_BASE + 0x00394)
#define AVSP_DCP_INT_RAW                (AVSP_BASE + 0x00398)
#define AVSP_DCP_INT_MSK                (AVSP_BASE + 0x0039c)
#define AVSP_DCP_STATUS0                (AVSP_BASE + 0x003a0)
#define AVSP_DCP_STATUS1                (AVSP_BASE + 0x003a4)
#define AVSP_DCP_STATUS2                (AVSP_BASE + 0x003a8)
#define AVSP_DCP_STATUS3                (AVSP_BASE + 0x003ac)
#define AVSP_DCP_STATUS4                (AVSP_BASE + 0x003b0)
#define AVSP_RCS_STRT                   (AVSP_BASE + 0x00400)
#define AVSP_RCS_UPDATE                 (AVSP_BASE + 0x00404)
#define AVSP_RCS_CLK_DIS                (AVSP_BASE + 0x00408)
#define AVSP_RCS_CTRL                   (AVSP_BASE + 0x00410)
#define AVSP_RCS_SIZE                   (AVSP_BASE + 0x00414)
#define AVSP_RCS_WR_STRIDE              (AVSP_BASE + 0x00430)
#define AVSP_RCS_C0LV0_BASE             (AVSP_BASE + 0x00434)
#define AVSP_RCS_C0LV1_BASE             (AVSP_BASE + 0x00438)
#define AVSP_RCS_C0LV2_BASE             (AVSP_BASE + 0x0043c)
#define AVSP_RCS_C0LV3_BASE             (AVSP_BASE + 0x00440)
#define AVSP_RCS_C0LV4_BASE             (AVSP_BASE + 0x00444)
#define AVSP_RCS_C0LV5_BASE             (AVSP_BASE + 0x00448)
#define AVSP_RCS_C1LV0_BASE             (AVSP_BASE + 0x0044c)
#define AVSP_RCS_C1LV2_BASE             (AVSP_BASE + 0x00450)
#define AVSP_RCS_C1LV1_BASE             (AVSP_BASE + 0x00454)
#define AVSP_RCS_C1LV3_BASE             (AVSP_BASE + 0x00458)
#define AVSP_RCS_C1LV4_BASE             (AVSP_BASE + 0x0045c)
#define AVSP_RCS_C1LV5_BASE             (AVSP_BASE + 0x00460)
#define AVSP_RCS_DTLV0_BASE             (AVSP_BASE + 0x00464)
#define AVSP_RCS_DTLV1_BASE             (AVSP_BASE + 0x00468)
#define AVSP_RCS_DTLV2_BASE             (AVSP_BASE + 0x0046c)
#define AVSP_RCS_DTLV3_BASE             (AVSP_BASE + 0x00470)
#define AVSP_RCS_DTLV4_BASE             (AVSP_BASE + 0x00474)
#define AVSP_RCS_DTLV5_BASE             (AVSP_BASE + 0x00478)
#define AVSP_RCS_WR_Y_BASE              (AVSP_BASE + 0x0047c)
#define AVSP_RCS_WR_C_BASE              (AVSP_BASE + 0x00480)
#define AVSP_RCS_WR_FBCE_HEAD_OFFSET    (AVSP_BASE + 0x00484)
#define AVSP_RCS_INT_EN0                (AVSP_BASE + 0x00490)
#define AVSP_RCS_INT_CLR0               (AVSP_BASE + 0x00494)
#define AVSP_RCS_INT_RAW0               (AVSP_BASE + 0x00498)
#define AVSP_RCS_INT_MSK0               (AVSP_BASE + 0x0049c)
#define AVSP_RCS_INT_EN1                (AVSP_BASE + 0x004a0)
#define AVSP_RCS_INT_CLR1               (AVSP_BASE + 0x004a4)
#define AVSP_RCS_INT_RAW1               (AVSP_BASE + 0x004a8)
#define AVSP_RCS_INT_MSK1               (AVSP_BASE + 0x004ac)
#define AVSP_RCS_STATUS0                (AVSP_BASE + 0x004b0)
#define AVSP_RCS_STATUS1                (AVSP_BASE + 0x004b4)
#define AVSP_RCS_STATUS2                (AVSP_BASE + 0x004b8)
#define AVSP_RCS_STATUS3                (AVSP_BASE + 0x004bc)
#define MMU_DTE_ADDR                    (AVSP_BASE + 0x00f00)
#define MMU_STATUS                      (AVSP_BASE + 0x00f04)
#define MMU_COMMAND                     (AVSP_BASE + 0x00f08)
#define MMU_PAGE_FAULT_ADDR             (AVSP_BASE + 0x00f0c)
#define MMU_ZAP_ONE_LINE                (AVSP_BASE + 0x00f10)
#define MMU_INT_RAWSTAT                 (AVSP_BASE + 0x00f14)
#define MMU_INT_CLEAR                   (AVSP_BASE + 0x00f18)
#define MMU_INT_MASK                    (AVSP_BASE + 0x00f1c)
#define MMU_INT_STATUS                  (AVSP_BASE + 0x00f20)
#define MMU_AUTO_GATING                 (AVSP_BASE + 0x00f24)
#define MMU_REG_LOAD_EN                 (AVSP_BASE + 0x00f28)

#endif
