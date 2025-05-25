/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __REG2_INFO_H__
#define __REG2_INFO_H__

#include "rga_drv.h"

#define RGA2_SYS_REG_BASE			0x000
#define RGA2_CSC_REG_BASE			0x060
#define RGA2_OTHER_REG_BASE			0x090
#define RGA2_CMD_REG_BASE			0x100

/* sys reg */
#define RGA2_SYS_CTRL				0x000
#define RGA2_CMD_CTRL				0x004
#define RGA2_CMD_BASE				0x008
#define RGA2_STATUS1				0x00c
#define RGA2_INT				0x010
#define RGA2_MMU_CTRL0				0x014
#define RGA2_MMU_CMD_BASE			0x018
#define RGA2_STATUS2				0x01c
#define RGA2_WORK_CNT				0x020
#define RGA2_VERSION_NUM			0x028
#define RGA2_READ_LINE_CNT			0x030
#define RGA2_WRITE_LINE_CNT			0x034
#define RGA2_LINE_CNT				0x038
#define RGA2_PERF_CTRL0				0x040

/* full csc reg */
#define RGA2_DST_CSC_00				0x060
#define RGA2_DST_CSC_01				0x064
#define RGA2_DST_CSC_02				0x068
#define RGA2_DST_CSC_OFF0			0x06c
#define RGA2_DST_CSC_10				0x070
#define RGA2_DST_CSC_11				0x074
#define RGA2_DST_CSC_12				0x078
#define RGA2_DST_CSC_OFF1			0x07c
#define RGA2_DST_CSC_20				0x080
#define RGA2_DST_CSC_21				0x084
#define RGA2_DST_CSC_22				0x088
#define RGA2_DST_CSC_OFF2			0x08c

/* back door0 */
#define RGA2_BACKDOOR0				0x090

/* osd read-back reg */
#define RGA2_OSD_CUR_FLAGS0			0x090
#define RGA2_OSD_CUR_FLAGS1			0x09c
#define RGA2_FIXED_OSD_CUR_FLAGS0		0x440
#define RGA2_FIXED_OSD_CUR_FLAGS1		0x444

/* mode ctrl */
#define RGA2_MODE_CTRL_OFFSET			0x000
#define RGA2_SRC_INFO_OFFSET			0x004
#define RGA2_SRC_BASE0_OFFSET			0x008
#define RGA2_FBCIN_HEAD_BASE_OFFSET		0x008 // repeat
#define RGA2_SRC_BASE1_OFFSET			0x00c
#define RGA2_FBCIN_PAYL_BASE_OFFSET		0x00c // repeat
#define RGA2_SRC_BASE2_OFFSET			0x010
#define RGA2_FBCIN_OFF_OFFSET			0x010 // repeat
#define RGA2_SRC_BASE3_OFFSET			0x014
#define RGA2_SRC_VIR_INFO_OFFSET		0x018
#define RGA2_FBCIN_HEAD_VIR_INFO_OFFSET		0x018 // repeat
#define RGA2_SRC_ACT_INFO_OFFSET		0x01c
#define RGA2_SRC_X_FACTOR_OFFSET		0x020
#define RGA2_OSD_CTRL0_OFFSET			0x020 // repeat
#define RGA2_SRC_Y_FACTOR_OFFSET		0x024
#define RGA2_OSD_CTRL1_OFFSET			0x024 // repeat
#define RGA2_SRC_BG_COLOR_OFFSET		0x028
#define RGA2_OSD_COLOR0_OFFSET			0x028 // repeat
#define RGA2_GAUSS_COE_OFFSET			0x028 // repeat
#define RGA2_SRC_FG_COLOR_OFFSET		0x02c
#define RGA2_OSD_COLOR1_OFFSET			0x02c // repeat
#define RGA2_SRC_TR_COLOR0_OFFSET		0x030
#define RGA2_CF_GR_A_OFFSET			0x030 // repeat
#define RGA2_OSD_LAST_FLAGS0_OFFSET		0x030 // repeat
#define RGA2_MOSAIC_MODE_OFFSET			0x030 // repeat
#define RGA2_SRC_TR_COLOR1_OFFSET		0x034
#define RGA2_CF_GR_B_OFFSET			0x034 // repeat
#define RGA2_OSD_LAST_FLAGS1_OFFSET		0x034 // repeat
#define RGA2_DST_INFO_OFFSET			0x038
#define RGA2_DST_BASE0_OFFSET			0x03c
#define RGA2_DST_BASE1_OFFSET			0x040
#define RGA2_DST_BASE2_OFFSET			0x044
#define RGA2_TILE4x4_OUT_BASE_OFFSET		0x044 //repeat
#define RGA2_DST_VIR_INFO_OFFSET		0x048
#define RGA2_DST_ACT_INFO_OFFSET		0x04c
#define RGA2_ALPHA_CTRL0_OFFSET			0x050
#define RGA2_ALPHA_CTRL1_OFFSET			0x054
#define RGA2_FADING_CTRL_OFFSET			0x058
#define RGA2_PAT_CON_OFFSET			0x05c
#define RGA2_ROP_CTRL0_OFFSET			0x060
#define RGA2_CF_GR_G_OFFSET			0x060 // repeat
#define RGA2_DST_Y4MAP_LUT0_OFFSET		0x060 // repeat
#define RGA2_DST_QUANTIZE_SCALE_OFFSET		0x060 // repeat
#define RGA2_OSD_INVERTSION_CAL0_OFFSET		0x060 // repeat
#define RGA2_ROP_CTRL1_OFFSET			0x064
#define RGA2_CF_GR_R_OFFSET			0x064 // repeat
#define RGA2_DST_Y4MAP_LUT1_OFFSET		0x064 // repeat
#define RGA2_DST_QUANTIZE_OFFSET_OFFSET		0x064 // repeat
#define RGA2_OSD_INVERTSION_CAL1_OFFSET		0x064 // repeat
#define RGA2_MASK_BASE_OFFSET			0x068
#define RGA2_MMU_CTRL1_OFFSET			0x06c
#define RGA2_UV_VIR_INFO			0x06c // repeat
#define RGA2_MMU_SRC_BASE_OFFSET		0x070
#define RGA2_PREFETCH_ADDR_TH_OFFSET		0x070 // repeat
#define RGA2_MMU_SRC1_BASE_OFFSET		0x074
#define RGA2_MMU_DST_BASE_OFFSET		0x078
#define RGA2_MMU_ELS_BASE_OFFSET		0x07c

/*RGA_SYS*/
#define m_RGA2_SYS_CTRL_SRC0YUV420SP_RD_OPT_DIS		(0x1 << 12) /* moved to RGA_BACKDOOR0 since RV1126B */
#define m_RGA2_SYS_CTRL_DST_WR_OPT_DIS			(0x1 << 11) /* moved to RGA_BACKDOOR0 since RV1126B */
#define m_RGA2_MASTER_CMD_BYPASS_EN			(0x1 << 11) /* after RV1126B */
#define m_RGA2_SYS_CTRL_CMD_CONTINUE_P			(0x1 << 10)
#define m_RGA2_SYS_CTRL_HOLD_MODE_EN			(0x1 << 9)
#define m_RGA2_SYS_CTRL_RST_HANDSAVE_P			(0x1 << 7)
#define m_RGA2_SYS_CTRL_RST_PROTECT_P			(0x1 << 6)
#define m_RGA2_SYS_CTRL_AUTO_RST			(0x1 << 5)
#define m_RGA2_SYS_CTRL_CCLK_SRESET_P			(0x1 << 4)
#define m_RGA2_SYS_CTRL_ACLK_SRESET_P			(0x1 << 3)
#define m_RGA2_SYS_CTRL_AUTO_CKG			(0x1 << 2)
#define m_RGA2_SYS_CTRL_CMD_MODE			(0x1 << 1)
#define m_RGA2_SYS_CTRL_CMD_OP_ST_P			(0x1 << 0)

#define s_RGA2_SYS_CTRL_CMD_CONTINUE(x)			((x & 0x1) << 10)
#define s_RGA2_SYS_CTRL_HOLD_MODE_EN(x)			((x & 0x1) << 9)
#define s_RGA2_SYS_CTRL_CMD_MODE(x)			((x & 0x1) << 1)

/* RGA_CMD_CTRL */
#define m_RGA2_CMD_CTRL_INCR_NUM			(0x3ff << 3)
#define m_RGA2_CMD_CTRL_STOP				(0x1 << 2)
#define m_RGA2_CMD_CTRL_INCR_VALID_P			(0x1 << 1)
#define m_RGA2_CMD_CTRL_CMD_LINE_ST_P			(0x1 << 0)

#define s_RGA2_CMD_CTRL_INCR_NUM(x)			((x & 0x3ff) << 3)

/* RGA_STATUS1 */
#define m_RGA2_STATUS1_SW_CMD_TOTAL_NUM			(0xfff << 8)
#define m_RGA2_STATUS1_SW_CMD_CUR_NUM			(0xfff << 8)
#define m_RGA2_STATUS1_SW_RGA_STA			(0x1 << 0)

/*RGA_INT*/
#define m_RGA2_INT_FBCIN_DEC_ERROR_CLEAR		(1 << 24)
#define m_RGA2_INT_FBCIN_DEC_ERROR_EN			(1 << 23)
#define m_RGA2_INT_FBCIN_DEC_ERROR			(1 << 22)
#define m_RGA2_INT_PREFETCH_TH_INTR			(1 << 21)
#define m_RGA2_INT_PRE_TH_CLEAR				(1 << 20)
#define m_RGA2_INT_SCL_ERROR_CLEAR			(1 << 19)
#define m_RGA2_INT_SCL_ERROR_EN				(1 << 18)
#define m_RGA2_INT_SCL_ERROR_INTR			(1 << 17)
#define m_RGA2_INT_LINE_WR_CLEAR			(1 << 16)
#define m_RGA2_INT_LINE_RD_CLEAR			(1 << 15)
#define m_RGA2_INT_LINE_WR_EN				(1 << 14)
#define m_RGA2_INT_LINE_RD_EN				(1 << 13)
#define m_RGA2_INT_WRITE_CNT_FLAG			(1 << 12)
#define m_RGA2_INT_READ_CNT_FLAG			(1 << 11)
#define m_RGA2_INT_ALL_CMD_DONE_INT_EN			(1 << 10)
#define m_RGA2_INT_MMU_INT_EN				(1 << 9)
#define m_RGA2_INT_ERROR_INT_EN				(1 << 8)
#define m_RGA2_INT_NOW_CMD_DONE_INT_CLEAR		(1 << 7)
#define m_RGA2_INT_ALL_CMD_DONE_INT_CLEAR		(1 << 6)
#define m_RGA2_INT_MMU_INT_CLEAR			(1 << 5)
#define m_RGA2_INT_ERROR_INT_CLEAR			(1 << 4)
#define m_RGA2_INT_CUR_CMD_DONE_INT_FLAG		(1 << 3)
#define m_RGA2_INT_ALL_CMD_DONE_INT_FLAG		(1 << 2)
#define m_RGA2_INT_MMU_INT_FLAG				(1 << 1)
#define m_RGA2_INT_ERROR_INT_FLAG			(1 << 0)

#define m_RGA2_INT_ERROR_FLAG_MASK \
	( \
		m_RGA2_INT_MMU_INT_FLAG | \
		m_RGA2_INT_ERROR_INT_FLAG | \
		m_RGA2_INT_SCL_ERROR_INTR | \
		m_RGA2_INT_FBCIN_DEC_ERROR \
	)
#define m_RGA2_INT_ERROR_CLEAR_MASK \
	( \
		m_RGA2_INT_MMU_INT_CLEAR | \
		m_RGA2_INT_ERROR_INT_CLEAR | \
		m_RGA2_INT_SCL_ERROR_CLEAR | \
		m_RGA2_INT_FBCIN_DEC_ERROR_CLEAR \
)
#define m_RGA2_INT_ERROR_ENABLE_MASK \
	( \
		m_RGA2_INT_MMU_INT_EN | \
		m_RGA2_INT_ERROR_INT_EN | \
		m_RGA2_INT_SCL_ERROR_EN | \
		m_RGA2_INT_FBCIN_DEC_ERROR_EN \
	)

#define s_RGA2_INT_LINE_WR_CLEAR(x)			((x & 0x1) << 16)
#define s_RGA2_INT_LINE_RD_CLEAR(x)			((x & 0x1) << 15)
#define s_RGA2_INT_LINE_WR_EN(x)			((x & 0x1) << 14)
#define s_RGA2_INT_LINE_RD_EN(x)			((x & 0x1) << 13)
#define s_RGA2_INT_ALL_CMD_DONE_INT_EN(x)		((x & 0x1) << 10)
#define s_RGA2_INT_MMU_INT_EN(x)			((x & 0x1) << 9)
#define s_RGA2_INT_ERROR_INT_EN(x)			((x & 0x1) << 8)
#define s_RGA2_INT_NOW_CMD_DONE_INT_CLEAR(x)		((x & 0x1) << 7)
#define s_RGA2_INT_ALL_CMD_DONE_INT_CLEAR(x)		((x & 0x1) << 6)
#define s_RGA2_INT_MMU_INT_CLEAR(x)			((x & 0x1) << 5)
#define s_RGA2_INT_ERROR_INT_CLEAR(x)			((x & 0x1) << 4)

/* RGA_STATUS2 hardware status */
#define m_RGA2_STATUS2_RPP_MKRAM_RREADY			(0x2 << 11)
#define m_RGA2_STATUS2_DSTRPP_OUTBUF_RREADY		(0x1f << 6)
#define m_RGA2_STATUS2_SRCRPP_OUTBUF_RREADY		(0xf << 2)
#define m_RGA2_STATUS2_BUS_ERROR			(0x1 << 1)
#define m_RGA2_STATUS2_RPP_ERROR			(0x1 << 0)

/* RGA_READ_LINE_CNT_TH */
#define m_RGA2_READ_LINE_SW_INTR_LINE_RD_TH		(0x1fff << 0)

#define s_RGA2_READ_LINE_SW_INTR_LINE_RD_TH(x)		((x & 0x1fff) << 0)

/* RGA_WRITE_LINE_CNT_TN */
#define m_RGA2_WRITE_LINE_SW_INTR_LINE_WR_START		(0x1fff << 0)
#define m_RGA2_WRITE_LINE_SW_INTR_LINE_WR_STEP		(0x1fff << 16)

#define s_RGA2_WRITE_LINE_SW_INTR_LINE_WR_START(x)	((x & 0x1fff) << 0)
#define s_RGA2_WRITE_LINE_SW_INTR_LINE_WR_STEP(x)	((x & 0x1fff) << 16)

/* RGA BACKDOOR0 */
#define m_RGA2_HSDBIL_VSP_FIX_DIS			(0x1 << 0)
#define m_RGA2_HSP_LEFT_COPY_DIS			(0x1 << 1)
#define m_RGA2_AXI_WR128_DIS				(0x1 << 2)
#define m_RGA2_TABLE_PRE_FETCH_DIS			(0X1 << 3)
#define m_RGA2_FBCIN_BSP_DIS				(0X1 << 4)
#define m_RGA2_SRC1_RGB888_FIX_DIS			(0X1 << 5)
#define m_RGA2_SRC0_YUV420SP_RD_OPT_DIS			(0X1 << 6)
#define m_RGA2_DST_WR_OPT_DIS				(0X1 << 7)
#define m_RGA2_OUTSTANDING_CFG_DIS			(0X1 << 8)

/* RGA_MODE_CTRL */
#define m_RGA2_MODE_CTRL_SW_RENDER_MODE			(0x7 << 0)
#define m_RGA2_MODE_CTRL_SW_BITBLT_MODE			(0x1 << 3)
#define m_RGA2_MODE_CTRL_SW_CF_ROP4_PAT			(0x1 << 4)
#define m_RGA2_MODE_CTRL_SW_ALPHA_ZERO_KET		(0x1 << 5)
#define m_RGA2_MODE_CTRL_SW_GRADIENT_SAT		(0x1 << 6)
#define m_RGA2_MODE_CTRL_SW_INTR_CF_E			(0x1 << 7)
#define m_RGA2_MODE_CTRL_SW_OSD_E			(0x1<<8)
#define m_RGA2_MODE_CTRL_SW_MOSAIC_EN			(0x1<<9)
#define m_RGA2_MODE_CTRL_SW_YIN_YOUT_EN			(0x1<<10)
#define m_RGA2_MODE_CTRL_SW_TILE4x4_IN_EN		(0x1 << 12)
#define m_RGA2_MODE_CTRL_SW_TILE4x4_OUT_EN		(0x1 << 13)
#define m_RGA2_MODE_CTRL_SW_TABLE_PRE_FETCH_MODE	(0x3 << 14)
#define m_RGA2_MODE_CTRL_SW_FBC_IN_EN			(0x1 << 16)
#define m_RGA2_MODE_CTRL_SW_SRC_GAUSS_EN		(0x1 << 17)
#define m_RGA2_MODE_CTRL_SW_FBC_BSP_DIS			(0x1 << 18) /* moved to RGA_BACKDOOR0 since RV1126B */
#define m_RGA2_MODE_CTRL_SW_TABLE_PRE_FETCH_DIS		(0x1 << 19) /* moved to RGA_BACKDOOR0 since RV1126B */
#define m_RGA2_MODE_CTRL_SW_AXI_WR128_DIS		(0x1 << 20) /* moved to RGA_BACKDOOR0 since RV1126B */
#define m_RGA2_MODE_CTRL_SW_HSP_LEFT_COPY_DIS		(0x1 << 21) /* moved to RGA_BACKDOOR0 since RV1126B */

#define s_RGA2_MODE_CTRL_SW_RENDER_MODE(x)		((x & 0x7) << 0)
#define s_RGA2_MODE_CTRL_SW_BITBLT_MODE(x)		((x & 0x1) << 3)
#define s_RGA2_MODE_CTRL_SW_CF_ROP4_PAT(x)		((x & 0x1) << 4)
#define s_RGA2_MODE_CTRL_SW_ALPHA_ZERO_KET(x)		((x & 0x1) << 5)
#define s_RGA2_MODE_CTRL_SW_GRADIENT_SAT(x)		((x & 0x1) << 6)
#define s_RGA2_MODE_CTRL_SW_INTR_CF_E(x)		((x & 0x1) << 7)
#define s_RGA2_MODE_CTRL_SW_OSD_E(x)			((x & 0x1) << 8)
#define s_RGA2_MODE_CTRL_SW_MOSAIC_EN(x)		((x & 0x1) << 9)
#define s_RGA2_MODE_CTRL_SW_YIN_YOUT_EN(x)		((x & 0x1) << 10)
#define s_RGA2_MODE_CTRL_SW_TILE4x4_IN_EN(x)		((x & 0x1) << 12)
#define s_RGA2_MODE_CTRL_SW_TILE4x4_OUT_EN(x)		((x & 0x1) << 13)
#define s_RGA2_MODE_CTRL_SW_TABLE_PRE_FETCH_MODE(x)	((x & 0x3) << 14)
#define s_RGA2_MODE_CTRL_SW_FBC_IN_EN(x)		((x & 0x1) << 16)
#define s_RGA2_MODE_CTRL_SW_SRC_GAUSS_EN(x)		((x & 0x1) << 17)
#define s_RGA2_MODE_CTRL_SW_FBC_BSP_DIS(x)		((x & 0x1) << 18)
#define s_RGA2_MODE_CTRL_SW_TABLE_PRE_FETCH_DIS(x)	((x & 0x1) << 19)
#define s_RGA2_MODE_CTRL_SW_AXI_WR128_DIS(x)		((x & 0x1) << 20)
#define s_RGA2_MODE_CTRL_SW_HSP_LEFT_COPY_DIS(x)	((x & 0x1) << 21)

/* RGA_SRC_INFO */
#define m_RGA2_SRC_INFO_SW_SRC_FMT			(0xf << 0)
#define m_RGA2_SRC_INFO_SW_FBCIN_MODE			(0x3 << 0) // repeat
#define m_RGA2_SRC_INFO_SW_FBCIN_FMT			(0x3 << 2) // repeat
#define m_RGA2_SRC_INFO_SW_SW_SRC_RB_SWAP		(0x1 << 4)
#define m_RGA2_SRC_INFO_SW_SW_SRC_ALPHA_SWAP		(0x1 << 5)
#define m_RGA2_SRC_INFO_SW_SW_SRC_UV_SWAP		(0x1 << 6)
#define m_RGA2_SRC_INFO_SW_SW_CP_ENDIAN			(0x1 << 7)
#define m_RGA2_SRC_INFO_SW_SW_SRC_CSC_MODE		(0x3 << 8)
#define m_RGA2_SRC_INFO_SW_SW_SRC_ROT_MODE		(0x3 << 10)
#define m_RGA2_SRC_INFO_SW_SW_SRC_MIR_MODE		(0x3 << 12)
#define m_RGA2_SRC_INFO_SW_SW_SRC_HSCL_MODE		(0x3 << 14)
#define m_RGA2_SRC_INFO_SW_SW_SRC_VSCL_MODE		(0x3 << 16)
#define m_RGA2_SRC_INFO_SW_SW_SRC_TRANS_MODE		(0x1 << 18)
#define m_RGA2_SRC_INFO_SW_SW_SRC_TRANS_E		(0xf << 19)
#define m_RGA2_SRC_INFO_SW_SW_SRC_DITHER_UP_E		(0x1 << 23)
#define m_RGA2_SRC_INFO_SW_SW_SRC_SCL_FILTER		(0x3 << 24)
#define m_RGA2_SRC_INFO_SW_SW_VSP_MODE_SEL		(0x1 << 26)
#define m_RGA2_SRC_INFO_SW_SW_YUV10_E			(0x1 << 27)
#define m_RGA2_SRC_INFO_SW_SW_YUV10_ROUND_E		(0x1 << 28)
#define m_RGA2_SRC_INFO_SW_SW_VSD_MODE_SEL		(0x1 << 29)
#define m_RGA2_SRC_INFO_SW_SW_HSP_MODE_SEL		(0x1 << 30)
#define m_RGA2_SRC_INFO_SW_SW_HSD_MODE_SEL		(0x1 << 31)


#define s_RGA2_SRC_INFO_SW_SRC_FMT(x)			((x & 0xf) << 0)
#define s_RGA2_SRC_INFO_SW_FBCIN_MODE(x)		((x & 0x3) << 0) // repeat
#define s_RGA2_SRC_INFO_SW_FBCIN_FMT(x)			((x & 0x3) << 2) // repeat
#define s_RGA2_SRC_INFO_SW_SW_SRC_RB_SWAP(x)		((x & 0x1) << 4)
#define s_RGA2_SRC_INFO_SW_SW_SRC_ALPHA_SWAP(x)		((x & 0x1) << 5)
#define s_RGA2_SRC_INFO_SW_SW_SRC_UV_SWAP(x)		((x & 0x1) << 6)
#define s_RGA2_SRC_INFO_SW_SW_CP_ENDAIN(x)		((x & 0x1) << 7)
#define s_RGA2_SRC_INFO_SW_SW_SRC_CSC_MODE(x)		((x & 0x3) << 8)
#define s_RGA2_SRC_INFO_SW_SW_SRC_ROT_MODE(x)		((x & 0x3) << 10)
#define s_RGA2_SRC_INFO_SW_SW_SRC_MIR_MODE(x)		((x & 0x3) << 12)
#define s_RGA2_SRC_INFO_SW_SW_SRC_HSCL_MODE(x)		((x & 0x3) << 14)
#define s_RGA2_SRC_INFO_SW_SW_SRC_VSCL_MODE(x)		((x & 0x3) << 16)

#define s_RGA2_SRC_INFO_SW_SW_SRC_TRANS_MODE(x)		((x & 0x1) << 18)
#define s_RGA2_SRC_INFO_SW_SW_SRC_TRANS_E(x)		((x & 0xf) << 19)
#define s_RGA2_SRC_INFO_SW_SW_SRC_DITHER_UP_E(x)	((x & 0x1) << 23)
#define s_RGA2_SRC_INFO_SW_SW_SRC_SCL_FILTER(x)		((x & 0x3) << 24)
#define s_RGA2_SRC_INFO_SW_SW_VSP_MODE_SEL(x)		((x & 0x1) << 26)
#define s_RGA2_SRC_INFO_SW_SW_YUV10_E(x)		((x & 0x1) << 27)
#define s_RGA2_SRC_INFO_SW_SW_YUV10_ROUND_E(x)		((x & 0x1) << 28)
#define s_RGA2_SRC_INFO_SW_SW_VSD_MODE_SEL(x)		((x & 0x1) << 29)
#define s_RGA2_SRC_INFO_SW_SW_HSP_MODE_SEL(x)		((x & 0x1) << 30)
#define s_RGA2_SRC_INFO_SW_SW_HSD_MODE_SEL(x)		((x & 0x1) << 31)

/* RGA_SRC_VIR_INFO */
#define m_RGA2_SRC_VIR_INFO_SW_SRC_VIR_STRIDE		(0x7fff << 0)
#define m_RGA2_SRC_VIR_INFO_SW_MASK_VIR_STRIDE		(0x3ff << 16)

#define s_RGA2_SRC_VIR_INFO_SW_SRC_VIR_STRIDE(x)	((x & 0x7fff) << 0)
#define s_RGA2_SRC_VIR_INFO_SW_MASK_VIR_STRIDE(x)	((x & 0x3ff) << 16)


/* RGA_SRC_ACT_INFO */
#define m_RGA2_SRC_ACT_INFO_SW_TILE4X4_IN_YOFF		(0x3 << 30)
#define m_RGA2_SRC_ACT_INFO_SW_SRC_ACT_HEIGHT		(0x7ff << 16)
#define m_RGA2_SRC_ACT_INFO_SW_TILE4X4_IN_XOFF		(0x3 << 14)
#define m_RGA2_SRC_ACT_INFO_SW_SRC_ACT_WIDTH		(0x7ff << 0)

#define s_RGA2_SRC_ACT_INFO_SW_TILE4X4_IN_YOFF(x)	((x & 0x3) << 30)
#define s_RGA2_SRC_ACT_INFO_SW_SRC_ACT_HEIGHT(x)	((x & 0x7ff) << 16)
#define s_RGA2_SRC_ACT_INFO_SW_TILE4X4_IN_XOFF(x)	((x & 0x3) << 14)
#define s_RGA2_SRC_ACT_INFO_SW_SRC_ACT_WIDTH(x)		((x & 0x7ff) << 0)

/* RGA2_OSD_CTRL0 */
#define m_RGA2_OSD_CTRL0_SW_OSD_MODE			(0x3 << 0)
#define m_RGA2_OSD_CTRL0_SW_OSD_VER_MODE		(0x1 << 2)
#define m_RGA2_OSD_CTRL0_SW_OSD_WIDTH_MODE		(0x1 << 3)
#define m_RGA2_OSD_CTRL0_SW_OSD_BLK_NUM			(0x1f << 4)
#define m_RGA2_OSD_CTRL0_SW_OSD_FLAGS_INDEX		(0x3f << 10)
#define m_RGA2_OSD_CTRL0_SW_OSD_FIX_WIDTH		(0x3f << 20)
#define m_RGA2_OSD_CTRL0_SW_OSD_2BPP_MODE		(0x1 << 30)

#define s_RGA2_OSD_CTRL0_SW_OSD_MODE(x)			((x & 0x3) << 0)
#define s_RGA2_OSD_CTRL0_SW_OSD_VER_MODE(x)		((x & 0x1) << 2)
#define s_RGA2_OSD_CTRL0_SW_OSD_WIDTH_MODE(x)		((x & 0x1) << 3)
#define s_RGA2_OSD_CTRL0_SW_OSD_BLK_NUM(x)		((x & 0x1f) << 4)
#define s_RGA2_OSD_CTRL0_SW_OSD_FLAGS_INDEX(x)		((x & 0x3ff) << 10)
#define s_RGA2_OSD_CTRL0_SW_OSD_FIX_WIDTH(x)		((x & 0x3ff) << 20)
#define s_RGA2_OSD_CTRL0_SW_OSD_2BPP_MODE(x)		((x & 0x1) << 30)

/* RGA2_GAUSS_COE */
#define m_RGA2_GAUSS_COE_SW_COE0			(0x3f << 0)
#define m_RGA2_GAUSS_COE_SW_COE1			(0x3f << 8)
#define m_RGA2_GAUSS_COE_SW_COE2			(0xff << 16)

#define s_RGA2_GAUSS_COE_SW_COE0(x)			((x & 0x3f) << 0)
#define s_RGA2_GAUSS_COE_SW_COE1(x)			((x & 0x3f) << 8)
#define s_RGA2_GAUSS_COE_SW_COE2(x)			((x & 0xff) << 16)

/* RGA2_OSD_CTRL1 */
#define m_RGA2_OSD_CTRL1_SW_OSD_COLOR_SEL		(0x1 << 0)
#define m_RGA2_OSD_CTRL1_SW_OSD_FLAG_SEL		(0x1 << 1)
#define m_RGA2_OSD_CTRL1_SW_OSD_DEFAULT_COLOR		(0x1 << 2)
#define m_RGA2_OSD_CTRL1_SW_OSD_AUTO_INVERST_MODE	(0x1 << 3)
#define m_RGA2_OSD_CTRL1_SW_OSD_THRESH			(0xff << 4)
#define m_RGA2_OSD_CTRL1_SW_OSD_INVERT_A_EN		(0x1 << 12)
#define m_RGA2_OSD_CTRL1_SW_OSD_INVERT_Y_DIS		(0x1 << 13)
#define m_RGA2_OSD_CTRL1_SW_OSD_INVERT_C_DIS		(0x1 << 14)
#define m_RGA2_OSD_CTRL1_SW_OSD_UNFIX_INDEX		(0xf << 16)

#define s_RGA2_OSD_CTRL1_SW_OSD_COLOR_SEL(x)		((x & 0x1) << 0)
#define s_RGA2_OSD_CTRL1_SW_OSD_FLAG_SEL(x)		((x & 0x1) << 1)
#define s_RGA2_OSD_CTRL1_SW_OSD_DEFAULT_COLOR(x)	((x & 0x1) << 2)
#define s_RGA2_OSD_CTRL1_SW_OSD_AUTO_INVERST_MODE(x)	((x & 0x1) << 3)
#define s_RGA2_OSD_CTRL1_SW_OSD_THRESH(x)		((x & 0xff) << 4)
#define s_RGA2_OSD_CTRL1_SW_OSD_INVERT_A_EN(x)		((x & 0x1) << 12)
#define s_RGA2_OSD_CTRL1_SW_OSD_INVERT_Y_DIS(x)		((x & 0x1) << 13)
#define s_RGA2_OSD_CTRL1_SW_OSD_INVERT_C_DIS(x)		((x & 0x1) << 14)
#define s_RGA2_OSD_CTRL1_SW_OSD_UNFIX_INDEX(x)		((x & 0xf) << 16)

/* RGA_DST_INFO */
#define m_RGA2_DST_INFO_SW_DST_FMT			(0xf << 0)
#define m_RGA2_DST_INFO_SW_DST_RB_SWAP			(0x1 << 4)
#define m_RGA2_DST_INFO_SW_ALPHA_SWAP			(0x1 << 5)
#define m_RGA2_DST_INFO_SW_DST_UV_SWAP			(0x1 << 6)
#define m_RGA2_DST_INFO_SW_SRC1_FMT			(0x7 << 7)
#define m_RGA2_DST_INFO_SW_SRC1_RB_SWP			(0x1 << 10)
#define m_RGA2_DST_INFO_SW_SRC1_ALPHA_SWP		(0x1 << 11)
#define m_RGA2_DST_INFO_SW_DITHER_UP_E			(0x1 << 12)
#define m_RGA2_DST_INFO_SW_DITHER_DOWN_E		(0x1 << 13)
#define m_RGA2_DST_INFO_SW_DITHER_MODE			(0x3 << 14)
#define m_RGA2_DST_INFO_SW_DST_CSC_MODE			(0x3 << 16)
#define m_RGA2_DST_INFO_SW_CSC_CLIP_MODE		(0x1 << 18)
#define m_RGA2_DST_INFO_SW_DST_CSC_MODE_2		(0x1 << 19)
#define m_RGA2_DST_INFO_SW_SRC1_CSC_MODE		(0x3 << 20)
#define m_RGA2_DST_INFO_SW_SRC1_CSC_CLIP_MODE		(0x1 << 22)
#define m_RGA2_DST_INFO_SW_DST_UVHDS_MODE		(0x1 << 23)
#define m_RGA2_DST_INFO_SW_DST_FMT_YUV400_EN		(0x1 << 24)
#define m_RGA2_DST_INFO_SW_DST_FMT_Y4_EN		(0x1 << 25)
#define m_RGA2_DST_INFO_SW_DST_NN_QUANTIZE_EN		(0x1 << 26)
#define m_RGA2_DST_INFO_SW_DST_UVVDS_MODE		(0x1 << 27)
#define m_RGA2_DST_INFO_SW_SRC1_A1555_ACONFIG_EN	(0x1 << 28)

#define s_RGA2_DST_INFO_SW_DST_FMT(x)			((x & 0xf) << 0)
#define s_RGA2_DST_INFO_SW_DST_RB_SWAP(x)		((x & 0x1) << 4)
#define s_RGA2_DST_INFO_SW_ALPHA_SWAP(x)		((x & 0x1) << 5)
#define s_RGA2_DST_INFO_SW_DST_UV_SWAP(x)		((x & 0x1) << 6)
#define s_RGA2_DST_INFO_SW_SRC1_FMT(x)			((x & 0x7) << 7)
#define s_RGA2_DST_INFO_SW_SRC1_RB_SWP(x)		((x & 0x1) << 10)
#define s_RGA2_DST_INFO_SW_SRC1_ALPHA_SWP(x)		((x & 0x1) << 11)
#define s_RGA2_DST_INFO_SW_DITHER_UP_E(x)		((x & 0x1) << 12)
#define s_RGA2_DST_INFO_SW_DITHER_DOWN_E(x)		((x & 0x1) << 13)
#define s_RGA2_DST_INFO_SW_DITHER_MODE(x)		((x & 0x3) << 14)
#define s_RGA2_DST_INFO_SW_DST_CSC_MODE(x)		((x & 0x3) << 16)
#define s_RGA2_DST_INFO_SW_CSC_CLIP_MODE(x)		((x & 0x1) << 18)
#define s_RGA2_DST_INFO_SW_DST_CSC_MODE_2(x)		((x & 0x1) << 19)
#define s_RGA2_DST_INFO_SW_SRC1_CSC_MODE(x)		((x & 0x3) << 20)
#define s_RGA2_DST_INFO_SW_SRC1_CSC_CLIP_MODE(x)	((x & 0x1) << 22)
#define s_RGA2_DST_INFO_SW_DST_UVHDS_MODE(x)		((x & 0x1) << 23)
#define s_RGA2_DST_INFO_SW_DST_FMT_YUV400_EN(x)		((x & 0x1) << 24)
#define s_RGA2_DST_INFO_SW_DST_FMT_Y4_EN(x)		((x & 0x1) << 25)
#define s_RGA2_DST_INFO_SW_DST_NN_QUANTIZE_EN(x)	((x & 0x1) << 26)
#define s_RGA2_DST_INFO_SW_DST_UVVDS_MODE(x)		((x & 0x1) << 27)
#define s_RGA2_DST_INFO_SW_SRC1_A1555_ACONFIG_EN(x)	((x & 0x1) << 28)

/* RGA_ALPHA_CTRL0 */
#define m_RGA2_ALPHA_CTRL0_SW_ALPHA_ROP_0		(0x1 << 0)
#define m_RGA2_ALPHA_CTRL0_SW_ALPHA_ROP_SEL		(0x1 << 1)
#define m_RGA2_ALPHA_CTRL0_SW_ROP_MODE			(0x3 << 2)
#define m_RGA2_ALPHA_CTRL0_SW_SRC_GLOBAL_ALPHA		(0xff << 4)
#define m_RGA2_ALPHA_CTRL0_SW_DST_GLOBAL_ALPHA		(0xff << 12)
#define m_RGA2_ALPHA_CTRLO_SW_MASK_ENDIAN		(0x1 << 20)

#define s_RGA2_ALPHA_CTRL0_SW_ALPHA_ROP_0(x)		((x & 0x1) << 0)
#define s_RGA2_ALPHA_CTRL0_SW_ALPHA_ROP_SEL(x)		((x & 0x1) << 1)
#define s_RGA2_ALPHA_CTRL0_SW_ROP_MODE(x)		((x & 0x3) << 2)
#define s_RGA2_ALPHA_CTRL0_SW_SRC_GLOBAL_ALPHA(x)	((x & 0xff) << 4)
#define s_RGA2_ALPHA_CTRL0_SW_DST_GLOBAL_ALPHA(x)	((x & 0xff) << 12)
#define s_RGA2_ALPHA_CTRLO_SW_MASK_ENDIAN(x)		((x & 0x1) << 20)



/* RGA_ALPHA_CTRL1 */
#define m_RGA2_ALPHA_CTRL1_SW_DST_COLOR_M0		(0x1 << 0)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_COLOR_M0		(0x1 << 1)
#define m_RGA2_ALPHA_CTRL1_SW_DST_FACTOR_M0		(0x7 << 2)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_FACTOR_M0		(0x7 << 5)
#define m_RGA2_ALPHA_CTRL1_SW_DST_ALPHA_CAL_M0		(0x1 << 8)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_ALPHA_CAL_M0		(0x1 << 9)
#define m_RGA2_ALPHA_CTRL1_SW_DST_BLEND_M0		(0x3 << 10)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_BLEND_M0		(0x3 << 12)
#define m_RGA2_ALPHA_CTRL1_SW_DST_ALPHA_M0		(0x1 << 14)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_ALPHA_M0		(0x1 << 15)
#define m_RGA2_ALPHA_CTRL1_SW_DST_FACTOR_M1		(0x7 << 16)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_FACTOR_M1		(0x7 << 19)
#define m_RGA2_ALPHA_CTRL1_SW_DST_ALPHA_CAL_M1		(0x1 << 22)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_ALPHA_CAL_M1		(0x1 << 23)
#define m_RGA2_ALPHA_CTRL1_SW_DST_BLEND_M1		(0x3 << 24)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_BLEND_M1		(0x3 << 26)
#define m_RGA2_ALPHA_CTRL1_SW_DST_ALPHA_M1		(0x1 << 28)
#define m_RGA2_ALPHA_CTRL1_SW_SRC_ALPHA_M1		(0x1 << 29)

#define s_RGA2_ALPHA_CTRL1_SW_DST_COLOR_M0(x)		((x & 0x1) << 0)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_COLOR_M0(x)		((x & 0x1) << 1)
#define s_RGA2_ALPHA_CTRL1_SW_DST_FACTOR_M0(x)		((x & 0x7) << 2)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_FACTOR_M0(x)		((x & 0x7) << 5)
#define s_RGA2_ALPHA_CTRL1_SW_DST_ALPHA_CAL_M0(x)	((x & 0x1) << 8)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_ALPHA_CAL_M0(x)	((x & 0x1) << 9)
#define s_RGA2_ALPHA_CTRL1_SW_DST_BLEND_M0(x)		((x & 0x3) << 10)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_BLEND_M0(x)		((x & 0x3) << 12)
#define s_RGA2_ALPHA_CTRL1_SW_DST_ALPHA_M0(x)		((x & 0x1) << 14)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_ALPHA_M0(x)		((x & 0x1) << 15)
#define s_RGA2_ALPHA_CTRL1_SW_DST_FACTOR_M1(x)		((x & 0x7) << 16)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_FACTOR_M1(x)		((x & 0x7) << 19)
#define s_RGA2_ALPHA_CTRL1_SW_DST_ALPHA_CAL_M1(x)	((x & 0x1) << 22)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_ALPHA_CAL_M1(x)	((x & 0x1) << 23)
#define s_RGA2_ALPHA_CTRL1_SW_DST_BLEND_M1(x)		((x & 0x3) << 24)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_BLEND_M1(x)		((x & 0x3) << 26)
#define s_RGA2_ALPHA_CTRL1_SW_DST_ALPHA_M1(x)		((x & 0x1) << 28)
#define s_RGA2_ALPHA_CTRL1_SW_SRC_ALPHA_M1(x)		((x & 0x1) << 29)



/* RGA_MMU_CTRL1 */
#define m_RGA2_MMU_CTRL1_SW_SRC_MMU_EN			(0x1 << 0)
#define m_RGA2_MMU_CTRL1_SW_SRC_MMU_FLUSH		(0x1 << 1)
#define m_RGA2_MMU_CTRL1_SW_SRC_MMU_PREFETCH_EN		(0x1 << 2)
#define m_RGA2_MMU_CTRL1_SW_SRC_MMU_PREFETCH_DIR	(0x1 << 3)
#define m_RGA2_MMU_CTRL1_SW_SRC1_MMU_EN			(0x1 << 4)
#define m_RGA2_MMU_CTRL1_SW_SRC1_MMU_FLUSH		(0x1 << 5)
#define m_RGA2_MMU_CTRL1_SW_SRC1_MMU_PREFETCH_EN	(0x1 << 6)
#define m_RGA2_MMU_CTRL1_SW_SRC1_MMU_PREFETCH_DIR	(0x1 << 7)
#define m_RGA2_MMU_CTRL1_SW_DST_MMU_EN			(0x1 << 8)
#define m_RGA2_MMU_CTRL1_SW_DST_MMU_FLUSH		(0x1 << 9)
#define m_RGA2_MMU_CTRL1_SW_DST_MMU_PREFETCH_EN		(0x1 << 10)
#define m_RGA2_MMU_CTRL1_SW_DST_MMU_PREFETCH_DIR	(0x1 << 11)
#define m_RGA2_MMU_CTRL1_SW_ELS_MMU_EN			(0x1 << 12)
#define m_RGA2_MMU_CTRL1_SW_ELS_MMU_FLUSH		(0x1 << 13)

#define s_RGA2_MMU_CTRL1_SW_SRC_MMU_EN(x)		((x & 0x1) << 0)
#define s_RGA2_MMU_CTRL1_SW_SRC_MMU_FLUSH(x)		((x & 0x1) << 1)
#define s_RGA2_MMU_CTRL1_SW_SRC_MMU_PREFETCH_EN(x)	((x & 0x1) << 2)
#define s_RGA2_MMU_CTRL1_SW_SRC_MMU_PREFETCH_DIR(x)	((x & 0x1) << 3)
#define s_RGA2_MMU_CTRL1_SW_SRC1_MMU_EN(x)				((x & 0x1) << 4)
#define s_RGA2_MMU_CTRL1_SW_SRC1_MMU_FLUSH(x)		((x & 0x1) << 5)
#define s_RGA2_MMU_CTRL1_SW_SRC1_MMU_PREFETCH_EN(x)	((x & 0x1) << 6)
#define s_RGA2_MMU_CTRL1_SW_SRC1_MMU_PREFETCH_DIR(x)	((x & 0x1) << 7)
#define s_RGA2_MMU_CTRL1_SW_DST_MMU_EN(x)		((x & 0x1) << 8)
#define s_RGA2_MMU_CTRL1_SW_DST_MMU_FLUSH(x)		((x & 0x1) << 9)
#define s_RGA2_MMU_CTRL1_SW_DST_MMU_PREFETCH_EN(x)	((x & 0x1) << 10)
#define s_RGA2_MMU_CTRL1_SW_DST_MMU_PREFETCH_DIR(x)	((x & 0x1) << 11)
#define s_RGA2_MMU_CTRL1_SW_ELS_MMU_EN(x)		((x & 0x1) << 12)
#define s_RGA2_MMU_CTRL1_SW_ELS_MMU_FLUSH(x)		((x & 0x1) << 13)

#define RGA2_VSP_BICUBIC_LIMIT				1996
#define RGA2_BILINEAR_PREC				12

#define RGA2_IOMMU_PREFETCH_SHIFT			16
#define RGA2_IOMMU_PREFETCH_MASK			0xffff
#define RGA2_IOMMU_PREFETCH_ALIGN(x) \
	(((x) + RGA2_IOMMU_PREFETCH_MASK) & ~RGA2_IOMMU_PREFETCH_MASK)
#define RGA2_IOMMU_PREFETCH_ALIGN_DOWN(x)		((x) & ~RGA2_IOMMU_PREFETCH_MASK)
#define RGA2_IOMMU_PREFETCH_THRESHOLD_MIN		(0x1U << RGA2_IOMMU_PREFETCH_SHIFT)
#define RGA2_IOMMU_PREFETCH_THRESHOLD_MAX		(0xffffU << RGA2_IOMMU_PREFETCH_SHIFT)

union rga2_color_ctrl {
	uint32_t value;
	struct {
		uint32_t dst_color_mode:1;
		uint32_t src_color_mode:1;

		uint32_t dst_factor_mode:3;
		uint32_t src_factor_mode:3;

		uint32_t dst_alpha_cal_mode:1;
		uint32_t src_alpha_cal_mode:1;

		uint32_t dst_blend_mode:2;
		uint32_t src_blend_mode:2;

		uint32_t dst_alpha_mode:1;
		uint32_t src_alpha_mode:1;
	} bits;
};

union rga2_alpha_ctrl {
	uint32_t value;
	struct {
		uint32_t dst_factor_mode:3;
		uint32_t src_factor_mode:3;

		uint32_t dst_alpha_cal_mode:1;
		uint32_t src_alpha_cal_mode:1;

		uint32_t dst_blend_mode:2;
		uint32_t src_blend_mode:2;

		uint32_t dst_alpha_mode:1;
		uint32_t src_alpha_mode:1;
	} bits;
};

extern const struct rga_backend_ops rga2_ops;

#endif

