/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2025 Rockchip Electronics Co., Ltd. */

#ifndef _RKFEC_REGS_H
#define _RKFEC_REGS_H

#define RKFEC_BASE                      0x0000
#define RKFEC_SYS_AR_QOS                (RKFEC_BASE + 0x0014)
#define RKFEC_STRT                      (RKFEC_BASE + 0x0200)
#define RKFEC_UPD                       (RKFEC_BASE + 0x0204)
#define RKFEC_CLK_DIS                   (RKFEC_BASE + 0x0208)
#define RKFEC_CTRL                      (RKFEC_BASE + 0x0210)
#define RKFEC_RD_VIR_STRIDE             (RKFEC_BASE + 0x0214)
#define RKFEC_RD_Y_BASE                 (RKFEC_BASE + 0x0218)
#define RKFEC_RD_C_BASE                 (RKFEC_BASE + 0x021c)
#define RKFEC_LUT_BASE                  (RKFEC_BASE + 0x0220)
#define RKFEC_WR_Y_BASE                 (RKFEC_BASE + 0x0234)
#define RKFEC_WR_C_BASE                 (RKFEC_BASE + 0x0238)
#define RKFEC_WR_FBCE_HEAD_OFFSET       (RKFEC_BASE + 0x023c)
#define RKFEC_RD_Y_BASE_SHD             (RKFEC_BASE + 0x0240)
#define RKFEC_RD_C_BASE_SHD             (RKFEC_BASE + 0x0244)
#define RKFEC_LUT_BASE_SHD              (RKFEC_BASE + 0x0248)
#define RKFEC_WR_FBCE_HEAD_OFFSET_SHD   (RKFEC_BASE + 0x024c)
#define RKFEC_CORE_CTRL                 (RKFEC_BASE + 0x0280)
#define RKFEC_BG_VALUE                  (RKFEC_BASE + 0x0284)
#define RKFEC_DST_SIZE                  (RKFEC_BASE + 0x0288)
#define RKFEC_LUT_SIZE                  (RKFEC_BASE + 0x028c)
#define RKFEC_STATUS0                   (RKFEC_BASE + 0x0290)
#define RKFEC_STATUS1                   (RKFEC_BASE + 0x0294)
#define RKFEC_SRC_SIZE                  (RKFEC_BASE + 0x0298)
#define RKFEC_STOP_REG                  (RKFEC_BASE + 0x029c)
#define RKFEC_WR_VIR_STRIDE             (RKFEC_BASE + 0x02a0)
#define RKFEC_INT_EN                    (RKFEC_BASE + 0x02b0)
#define RKFEC_INT_RAW                   (RKFEC_BASE + 0x02b4)
#define RKFEC_INT_MSK                   (RKFEC_BASE + 0x02b8)
#define RKFEC_INT_CLR                   (RKFEC_BASE + 0x02bc)

#define RKFEC_CACHE_STATUS              (RKFEC_BASE + 0x0e08)
#define RKFEC_CACHE_COMMAND             (RKFEC_BASE + 0x0e10)
#define RKFEC_CACHE_CLEAR_PAGE          (RKFEC_BASE + 0x0e14)
#define RKFEC_CACHE_MAX_READS           (RKFEC_BASE + 0x0e18)
#define RKFEC_CACHE_CTRL                (RKFEC_BASE + 0x0e1c)
#define RKFEC_CACHE_PERFCNT_SRC0        (RKFEC_BASE + 0x0e20)
#define RKFEC_CACHE_PERFCNT_VAL0        (RKFEC_BASE + 0x0e24)
#define RKFEC_CACHE_PERFCNT_SRC1        (RKFEC_BASE + 0x0e28)
#define RKFEC_CACHE_RERFCNT_VAL1        (RKFEC_BASE + 0x0e2c)

#define RKFEC_MMU_DTE_ADDR              (RKFEC_BASE + 0x0f00)
#define RKFEC_MMU_STATUS                (RKFEC_BASE + 0x0f04)
#define RKFEC_MMU_COMMAND               (RKFEC_BASE + 0x0f08)
#define RKFEC_MMU_PAGE_FAULT_ADDR       (RKFEC_BASE + 0x0f0c)
#define RKFEC_MMU_ZAP_ONE_LINE          (RKFEC_BASE + 0x0f10)
#define RKFEC_MMU_INT_RAWSTAT           (RKFEC_BASE + 0x0f14)
#define RKFEC_MMU_INT_CLEAR             (RKFEC_BASE + 0x0f18)
#define RKFEC_MMU_INT_MASK              (RKFEC_BASE + 0x0f1c)
#define RKFEC_MMU_INT_STATUS            (RKFEC_BASE + 0x0f20)
#define RKFEC_MMU_AUTO_GATING           (RKFEC_BASE + 0x0f24)
#define RKFEC_MMU_REG_LOAD_EN           (RKFEC_BASE + 0x0f28)

/****************** BIT *******************/

/* FEC_STRT */
#define SYS_FEC_ST                      BIT(0)

/* FEC_UPD */
#define SYS_FEC_FORCE_UPD               BIT(0)

/* FEC_CLK_DIS */
#define SYS_FEC_LGC_CKG_DIS             BIT(0)
#define SYS_FEC_RAM_CKG_DIS             BIT(1)
#define SYS_RST_PROTECT_DIS             BIT(12)
#define SYS_SOFT_RST_FBCE               BIT(13)
#define SYS_SOFT_RST_ACLK               BIT(14)
#define ARST_SAFETY_FEC_DONE            BIT(15)
#define SW_FEC_HURRY_EN                 BIT(16)
#define SW_FEC_HURRY_NUM(n)             ((n & 0x7fff) << 17)

/* FEC_CTRL */
#define SW_FEC_RD_PIC_FORMAT            BIT(1)
#define SW_FEC_RD_FORMAT                BIT(2)
#define SW_FEC_RD_MODE(x)               ((x & 0x3) << 4)
#define SW_FEC_WR_PIC_FORMAT            BIT(7)
#define SW_FEC_WR_FORMAT                BIT(8)
#define SW_FEC_WR_MODE(x)               ((x & 0x3) << 9)
#define SW_FEC_WR_FBCE_FORCE_UNC_EN     BIT(13)

#define SW_FEC_RD_FMT(x)                ((x & 0xf) << 2)
#define SW_FEC_WR_FMT(x)                ((x & 0xf) << 8)

/* FEC_CORE_CTRL */
#define SW_FEC_EN                       BIT(0)
#define SW_FEC_BIC_MODE(x)              ((x & 0x3) << 3)
#define SW_LUT_DENSITY(x)               ((x & 0x3) << 5)
#define SW_FEC_BORDER_MODE(x)           ((x & 0x1) << 7)
#define SW_FEC_PBUF_CRS_DIS(x)          ((x & 0x1) << 8)
#define SW_FEC_CRS_BUF_MODE(x)          ((x & 0x1) << 10)
#define SW_FEC_EN_SHD                   BIT(31)

/* FEC_RD_VIR_STRIDE */
#define FEC_RD_VIR_STRIDE_Y(x)          ((x) & 0x3fff)
#define FEC_RD_VIR_STRIDE_C(x)          (((x) & 0x3fff) << 16)

/* FEC_BG_VALUE */
#define SW_BG_Y_VALUE(y)                ((y & 0xff))
#define SW_BG_U_VALUE(u)                ((u & 0xff) << 10)
#define SW_BG_V_VALUE(v)                ((v & 0xff) << 20)

/* FEC_DST_SIZE */
#define SW_FEC_DST_WIDTH(x)             ((x) & 0x1fff)
#define Sw_FEC_DST_HEIGHT(x)            (((x) & 0x1fff) << 16)

/* FEC_SRC_SIZE */
#define SW_FEC_SRC_WIDTH(x)             ((x) & 0x1fff)
#define Sw_FEC_SRC_HEIGHT(x)            (((x) & 0x1fff) << 16)

/* FEC_WR_VIR_STRIDE */
#define FEC_WR_VIR_STRIDE_Y(x)          ((x) & 0x3fff)
#define FEC_WR_VIR_STRIDE_C(x)          (((x) & 0x3fff) << 16)

/* FEC_WR_FBCE_HEAD_OFFSET */
#define SW_FEC_WR_FBCE_HEAD_OFFSET(x)   ((x) << 4)

/* LUT SIZE */
#define SW_LUT_SIZE(x)                  ((x) & 0x3fffff)

/* FEC_CACHE_CTRL */
#define SW_CACHE_LINESIZE(x)		((x & 0x3) << 4)
#define SW_CACHE_FORCE_BSP(x)		((x & 0x1) << 12)
#define SW_REPLACE_STRATEGY(x)		((x & 0x1) << 9)
#define SW_CACHELINE_EN(x)		((x & 0x1) << 13)
#define SW_CACHE_BYPASS_EN(x)		((x & 0x1) << 6)

/* FEC_INT_EN */
#define PBUF_BD_CRS_P                   BIT(0)
#define FEC_STOP_IRQ                    BIT(1)
#define MBUF_PP_FAIL                    BIT(2)
#define PBUF_PP_FAIL                    BIT(3)
#define AXI_MS_BRESP_ERR                BIT(4)
#define AXI_MS_RRESP_ERR                BIT(5)
#define FRM_END_P_FEC                   BIT(7)

#endif /* _RKISPP_REGS_H */

