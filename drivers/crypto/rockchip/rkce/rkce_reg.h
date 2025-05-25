/* SPDX-License-Identifier: GPL-2.0 */

/* Copyright (c) 2024 Rockchip Electronics Co., Ltd. */

#ifndef __RKCE_REG_H
#define __RKCE_REG_H

#include <linux/types.h>

/****************************************************************************************/
/*                                                                                      */
/*                               Module Structure Section                               */
/*                                                                                      */
/****************************************************************************************/
/* RKCE Register Structure Define */
struct RKCE_REG {
	uint32_t CLK_CTL;                            /* Address Offset: 0x0000 */
	uint32_t RST_CTL;                            /* Address Offset: 0x0004 */
	uint32_t RESERVED0008[126];                  /* Address Offset: 0x0008 */
	uint32_t TD_ADDR;                            /* Address Offset: 0x0200 */
	uint32_t TD_LOAD_CTRL;                       /* Address Offset: 0x0204 */
	uint32_t FIFO_ST;                            /* Address Offset: 0x0208 */
	uint32_t RESERVED020C;                       /* Address Offset: 0x020C */
	uint32_t SYMM_INT_EN;                        /* Address Offset: 0x0210 */
	uint32_t SYMM_INT_ST;                        /* Address Offset: 0x0214 */
	uint32_t SYMM_TD_ID;                         /* Address Offset: 0x0218 */
	uint32_t SYMM_TD_ST;                         /* Address Offset: 0x021C */
	uint32_t SYMM_ST_DBG;                        /* Address Offset: 0x0220 */
	uint32_t SYMM_CONTEXT_SIZE;                  /* Address Offset: 0x0224 */
	uint32_t SYMM_TD_ADDR_DBG;                   /* Address Offset: 0x0228 */
	uint32_t SYMM_TD_GRANT_DBG;                  /* Address Offset: 0x022C */
	uint32_t HASH_INT_EN;                        /* Address Offset: 0x0230 */
	uint32_t HASH_INT_ST;                        /* Address Offset: 0x0234 */
	uint32_t HASH_TD_ID;                         /* Address Offset: 0x0238 */
	uint32_t HASH_TD_ST;                         /* Address Offset: 0x023C */
	uint32_t HASH_ST_DBG;                        /* Address Offset: 0x0240 */
	uint32_t HASH_CONTEXT_SIZE;                  /* Address Offset: 0x0244 */
	uint32_t HASH_TD_ADDR_DBG;                   /* Address Offset: 0x0248 */
	uint32_t HASH_TD_GRANT_DBG;                  /* Address Offset: 0x024C */
	uint32_t SYMM_TD_POP_ADDR;                   /* Address Offset: 0x0250 */
	uint32_t HASH_TD_POP_ADDR;                   /* Address Offset: 0x0254 */
	uint32_t TD_POP_CTRL;                        /* Address Offset: 0x0258 */
	uint32_t RESERVED025C[5];                    /* Address Offset: 0x025C */
	uint32_t KL_TO_CE_PADDR;                     /* Address Offset: 0x0270 */
	uint32_t KL_KD_ADDR;                         /* Address Offset: 0x0274 */
	uint32_t RESERVED0278[94];                   /* Address Offset: 0x0278 */
	uint32_t ECC_CTL;                            /* Address Offset: 0x03F0 */
	uint32_t ECC_INT_EN;                         /* Address Offset: 0x03F4 */
	uint32_t ECC_INT_ST;                         /* Address Offset: 0x03F8 */
	uint32_t ECC_ABN_ST;                         /* Address Offset: 0x03FC */
	uint32_t ECC_CURVE_WIDE;                     /* Address Offset: 0x0400 */
	uint32_t ECC_MAX_CURVE_WIDE;                 /* Address Offset: 0x0404 */
	uint32_t ECC_DATA_ENDIAN;                    /* Address Offset: 0x0408 */
	uint32_t RESERVED040C[17];                   /* Address Offset: 0x040C */
	uint32_t KL_APB_CMD;                         /* Address Offset: 0x0450 */
	uint32_t KL_APB_PADDR;                       /* Address Offset: 0x0454 */
	uint32_t KL_APB_PWDATA;                      /* Address Offset: 0x0458 */
	uint32_t RESERVED045C[2];                    /* Address Offset: 0x045C */
	uint32_t KL_KD_VID;                          /* Address Offset: 0x0464 */
	uint32_t KL_KD_MODE;                         /* Address Offset: 0x0468 */
	uint32_t RESERVED046C[5];                    /* Address Offset: 0x046C */
	uint32_t PKA_RAM_CTL;                        /* Address Offset: 0x0480 */
	uint32_t PKA_RAM_ST;                         /* Address Offset: 0x0484 */
	uint32_t RESERVED0488[6];                    /* Address Offset: 0x0488 */
	uint32_t PKA_DEBUG_CTL;                      /* Address Offset: 0x04A0 */
	uint32_t PKA_DEBUG_ST;                       /* Address Offset: 0x04A4 */
	uint32_t PKA_DEBUG_MONITOR;                  /* Address Offset: 0x04A8 */
	uint32_t RESERVED04AC[85];                   /* Address Offset: 0x04AC */
	uint32_t KT_ST;                              /* Address Offset: 0x0600 */
	uint32_t RESERVED0604;                       /* Address Offset: 0x0604 */
	uint32_t KL_INTER_COPY;                      /* Address Offset: 0x0608 */
	uint32_t RESERVED060C[4];                    /* Address Offset: 0x060C */
	uint32_t LOCKSTEP_EN;                        /* Address Offset: 0x061C */
	uint32_t RESERVED0620[2];                    /* Address Offset: 0x0620 */
	uint32_t LOCKSTEP_IJERR;                     /* Address Offset: 0x0628 */
	uint32_t RESERVED062C[5];                    /* Address Offset: 0x062C */
	uint32_t KL_OTP_KEY_REQ;                     /* Address Offset: 0x0640 */
	uint32_t KL_KEY_CLEAR;                       /* Address Offset: 0x0644 */
	uint32_t KL_OTP_KEY_LEN;                     /* Address Offset: 0x0648 */
	uint32_t KL_HW_DRNG_REQ;                     /* Address Offset: 0x064C */
	uint32_t RESERVED0650[12];                   /* Address Offset: 0x0650 */
	uint32_t AES_VER;                            /* Address Offset: 0x0680 */
	uint32_t DES_VER;                            /* Address Offset: 0x0684 */
	uint32_t SM4_VER;                            /* Address Offset: 0x0688 */
	uint32_t HASH_VER;                           /* Address Offset: 0x068C */
	uint32_t HMAC_VER;                           /* Address Offset: 0x0690 */
	uint32_t RESERVED0694;                       /* Address Offset: 0x0694 */
	uint32_t PKA_VER;                            /* Address Offset: 0x0698 */
	uint32_t EXTRA_FEATURE;                      /* Address Offset: 0x069C */
	uint32_t RESERVED06A0[20];                   /* Address Offset: 0x06A0 */
	uint32_t CE_VER;                             /* Address Offset: 0x06F0 */
	uint32_t RESERVED06F4[67];                   /* Address Offset: 0x06F4 */
	uint32_t PKA_MEM_MAP0;                       /* Address Offset: 0x0800 */
	uint32_t PKA_MEM_MAP1;                       /* Address Offset: 0x0804 */
	uint32_t PKA_MEM_MAP2;                       /* Address Offset: 0x0808 */
	uint32_t PKA_MEM_MAP3;                       /* Address Offset: 0x080C */
	uint32_t PKA_MEM_MAP4;                       /* Address Offset: 0x0810 */
	uint32_t PKA_MEM_MAP5;                       /* Address Offset: 0x0814 */
	uint32_t PKA_MEM_MAP6;                       /* Address Offset: 0x0818 */
	uint32_t PKA_MEM_MAP7;                       /* Address Offset: 0x081C */
	uint32_t PKA_MEM_MAP8;                       /* Address Offset: 0x0820 */
	uint32_t PKA_MEM_MAP9;                       /* Address Offset: 0x0824 */
	uint32_t PKA_MEM_MAP10;                      /* Address Offset: 0x0828 */
	uint32_t PKA_MEM_MAP11;                      /* Address Offset: 0x082C */
	uint32_t PKA_MEM_MAP12;                      /* Address Offset: 0x0830 */
	uint32_t PKA_MEM_MAP13;                      /* Address Offset: 0x0834 */
	uint32_t PKA_MEM_MAP14;                      /* Address Offset: 0x0838 */
	uint32_t PKA_MEM_MAP15;                      /* Address Offset: 0x083C */
	uint32_t PKA_MEM_MAP16;                      /* Address Offset: 0x0840 */
	uint32_t PKA_MEM_MAP17;                      /* Address Offset: 0x0844 */
	uint32_t PKA_MEM_MAP18;                      /* Address Offset: 0x0848 */
	uint32_t PKA_MEM_MAP19;                      /* Address Offset: 0x084C */
	uint32_t PKA_MEM_MAP20;                      /* Address Offset: 0x0850 */
	uint32_t PKA_MEM_MAP21;                      /* Address Offset: 0x0854 */
	uint32_t PKA_MEM_MAP22;                      /* Address Offset: 0x0858 */
	uint32_t PKA_MEM_MAP23;                      /* Address Offset: 0x085C */
	uint32_t PKA_MEM_MAP24;                      /* Address Offset: 0x0860 */
	uint32_t PKA_MEM_MAP25;                      /* Address Offset: 0x0864 */
	uint32_t PKA_MEM_MAP26;                      /* Address Offset: 0x0868 */
	uint32_t PKA_MEM_MAP27;                      /* Address Offset: 0x086C */
	uint32_t PKA_MEM_MAP28;                      /* Address Offset: 0x0870 */
	uint32_t PKA_MEM_MAP29;                      /* Address Offset: 0x0874 */
	uint32_t PKA_MEM_MAP30;                      /* Address Offset: 0x0878 */
	uint32_t PKA_MEM_MAP31;                      /* Address Offset: 0x087C */
	uint32_t PKA_OPCODE;                         /* Address Offset: 0x0880 */
	uint32_t N_NP_T0_T1_ADDR;                    /* Address Offset: 0x0884 */
	uint32_t PKA_STATUS;                         /* Address Offset: 0x0888 */
	uint32_t RESERVED088C;                       /* Address Offset: 0x088C */
	uint32_t PKA_L0;                             /* Address Offset: 0x0890 */
	uint32_t PKA_L1;                             /* Address Offset: 0x0894 */
	uint32_t PKA_L2;                             /* Address Offset: 0x0898 */
	uint32_t PKA_L3;                             /* Address Offset: 0x089C */
	uint32_t PKA_L4;                             /* Address Offset: 0x08A0 */
	uint32_t PKA_L5;                             /* Address Offset: 0x08A4 */
	uint32_t PKA_L6;                             /* Address Offset: 0x08A8 */
	uint32_t PKA_L7;                             /* Address Offset: 0x08AC */
	uint32_t PKA_PIPE_RDY;                       /* Address Offset: 0x08B0 */
	uint32_t PKA_DONE;                           /* Address Offset: 0x08B4 */
	uint32_t PKA_MON_SELECT;                     /* Address Offset: 0x08B8 */
	uint32_t PKA_DEBUG_REG_EN;                   /* Address Offset: 0x08BC */
	uint32_t DEBUG_CNT_ADDR;                     /* Address Offset: 0x08C0 */
	uint32_t DEBUG_EXT_ADDR;                     /* Address Offset: 0x08C4 */
	uint32_t PKA_DEBUG_HALT;                     /* Address Offset: 0x08C8 */
	uint32_t RESERVED08CC;                       /* Address Offset: 0x08CC */
	uint32_t PKA_MON_READ;                       /* Address Offset: 0x08D0 */
	uint32_t PKA_INT_ENA;                        /* Address Offset: 0x08D4 */
	uint32_t PKA_INT_ST;                         /* Address Offset: 0x08D8 */
	uint32_t TD0_TD1_TX_ADDR;                    /* Address Offset: 0x08DC */
	uint32_t RESERVED08E0[456];                  /* Address Offset: 0x08E0 */
	uint32_t SRAM_ADDR;                          /* Address Offset: 0x1000 */
};

/****************************************************************************************/
/*                                                                                      */
/*                               Register Bitmap Section                                */
/*                                                                                      */
/****************************************************************************************/
/******************************************RKCE******************************************/
/* CLK_CTL */
#define RKCE_CLK_CTL_OFFSET                        (0x0U)
#define RKCE_CLK_CTL_AUTO_CLKGATE_EN_SHIFT         (0U)
#define RKCE_CLK_CTL_AUTO_CLKGATE_EN_MASK          (0x1U << RKCE_CLK_CTL_AUTO_CLKGATE_EN_SHIFT)
/* RST_CTL */
#define RKCE_RST_CTL_OFFSET                        (0x4U)
#define RKCE_RST_CTL_SW_SYMM_RESET_SHIFT           (0U)
#define RKCE_RST_CTL_SW_SYMM_RESET_MASK            (0x1U << RKCE_RST_CTL_SW_SYMM_RESET_SHIFT)
#define RKCE_RST_CTL_SW_HASH_RESET_SHIFT           (1U)
#define RKCE_RST_CTL_SW_HASH_RESET_MASK            (0x1U << RKCE_RST_CTL_SW_HASH_RESET_SHIFT)
#define RKCE_RST_CTL_SW_PKA_RESET_SHIFT            (2U)
#define RKCE_RST_CTL_SW_PKA_RESET_MASK             (0x1U << RKCE_RST_CTL_SW_PKA_RESET_SHIFT)
/* TD_ADDR */
#define RKCE_TD_ADDR_OFFSET                        (0x200U)
#define RKCE_TD_ADDR_TD_ADDR_SHIFT                 (0U)
#define RKCE_TD_ADDR_TD_ADDR_MASK                  (0xFFFFFFFFU << RKCE_TD_ADDR_TD_ADDR_SHIFT)
/* TD_LOAD_CTRL */
#define RKCE_TD_LOAD_CTRL_OFFSET                   (0x204U)
#define RKCE_TD_LOAD_CTRL_SYMM_TLR_SHIFT           (0U)
#define RKCE_TD_LOAD_CTRL_SYMM_TLR_MASK            (0x1U << RKCE_TD_LOAD_CTRL_SYMM_TLR_SHIFT)
#define RKCE_TD_LOAD_CTRL_HASH_TLR_SHIFT           (1U)
#define RKCE_TD_LOAD_CTRL_HASH_TLR_MASK            (0x1U << RKCE_TD_LOAD_CTRL_HASH_TLR_SHIFT)
/* SYMM_INT_ST */
#define RKCE_SYMM_INT_ST_OFFSET                    (0x214U)
#define RKCE_SYMM_INT_ST_TD_DONE_SHIFT             (0U)
#define RKCE_SYMM_INT_ST_TD_DONE_MASK              (0x1U << RKCE_SYMM_INT_ST_TD_DONE_SHIFT)
#define RKCE_SYMM_INT_ST_DST_ERROR_SHIFT           (1U)
#define RKCE_SYMM_INT_ST_DST_ERROR_MASK            (0x1U << RKCE_SYMM_INT_ST_DST_ERROR_SHIFT)
#define RKCE_SYMM_INT_ST_SRC_ERROR_SHIFT           (2U)
#define RKCE_SYMM_INT_ST_SRC_ERROR_MASK            (0x1U << RKCE_SYMM_INT_ST_SRC_ERROR_SHIFT)
#define RKCE_SYMM_INT_ST_TD_ERROR_SHIFT            (3U)
#define RKCE_SYMM_INT_ST_TD_ERROR_MASK             (0x1U << RKCE_SYMM_INT_ST_TD_ERROR_SHIFT)
#define RKCE_SYMM_INT_ST_NE_LEN_SHIFT              (4U)
#define RKCE_SYMM_INT_ST_NE_LEN_MASK               (0x1U << RKCE_SYMM_INT_ST_NE_LEN_SHIFT)
#define RKCE_SYMM_INT_ST_LOCKSTEP_ERROR_SHIFT      (5U)
#define RKCE_SYMM_INT_ST_LOCKSTEP_ERROR_MASK       (0x1U << RKCE_SYMM_INT_ST_LOCKSTEP_ERROR_SHIFT)
/* SYMM_TD_ID */
#define RKCE_SYMM_TD_ID_OFFSET                     (0x218U)
#define RKCE_SYMM_TD_ID                            (0x0U)
#define RKCE_SYMM_TD_ID_STDID_SHIFT                (0U)
#define RKCE_SYMM_TD_ID_STDID_MASK                 (0xFFFFFFFFU << RKCE_SYMM_TD_ID_STDID_SHIFT)
/* SYMM_TD_ST */
#define RKCE_SYMM_TD_ST_OFFSET                     (0x21CU)
#define RKCE_SYMM_TD_ST                            (0x0U)
#define RKCE_SYMM_TD_ST_FIRST_PKG_FLAG_SHIFT       (0U)
#define RKCE_SYMM_TD_ST_FIRST_PKG_FLAG_MASK        (0x1U << RKCE_SYMM_TD_ST_FIRST_PKG_FLAG_SHIFT)
#define RKCE_SYMM_TD_ST_LAST_PKG_FLAG_SHIFT        (1U)
#define RKCE_SYMM_TD_ST_LAST_PKG_FLAG_MASK         (0x1U << RKCE_SYMM_TD_ST_LAST_PKG_FLAG_SHIFT)
#define RKCE_SYMM_TD_ST_DMA_START_FLAG_SHIFT       (2U)
#define RKCE_SYMM_TD_ST_DMA_START_FLAG_MASK        (0x1U << RKCE_SYMM_TD_ST_DMA_START_FLAG_SHIFT)
#define RKCE_SYMM_TD_ST_PRMPT_PKG_FLGA_SHIFT       (3U)
#define RKCE_SYMM_TD_ST_PRMPT_PKG_FLGA_MASK        (0x1U << RKCE_SYMM_TD_ST_PRMPT_PKG_FLGA_SHIFT)
/* SYMM_CONTEXT_SIZE */
#define RKCE_SYMM_CONTEXT_SIZE_OFFSET              (0x224U)
#define RKCE_SYMM_CONTEXT_SIZE                     (0x20U)
/* HASH_INT_ST */
#define RKCE_HASH_INT_ST_OFFSET                    (0x234U)
#define RKCE_HASH_INT_ST_TD_DONE_SHIFT             (0U)
#define RKCE_HASH_INT_ST_TD_DONE_MASK              (0x1U << RKCE_HASH_INT_ST_TD_DONE_SHIFT)
#define RKCE_HASH_INT_ST_DST_ERROR_SHIFT           (1U)
#define RKCE_HASH_INT_ST_DST_ERROR_MASK            (0x1U << RKCE_HASH_INT_ST_DST_ERROR_SHIFT)
#define RKCE_HASH_INT_ST_SRC_ERROR_SHIFT           (2U)
#define RKCE_HASH_INT_ST_SRC_ERROR_MASK            (0x1U << RKCE_HASH_INT_ST_SRC_ERROR_SHIFT)
#define RKCE_HASH_INT_ST_TD_ERROR_SHIFT            (3U)
#define RKCE_HASH_INT_ST_TD_ERROR_MASK             (0x1U << RKCE_HASH_INT_ST_TD_ERROR_SHIFT)
#define RKCE_HASH_INT_ST_NE_LEN_SHIFT              (4U)
#define RKCE_HASH_INT_ST_NE_LEN_MASK               (0x1U << RKCE_HASH_INT_ST_NE_LEN_SHIFT)
#define RKCE_HASH_INT_ST_LOCKSTEP_ERROR_SHIFT      (5U)
#define RKCE_HASH_INT_ST_LOCKSTEP_ERROR_MASK       (0x1U << RKCE_HASH_INT_ST_LOCKSTEP_ERROR_SHIFT)
/* HASH_TD_ID */
#define RKCE_HASH_TD_ID_OFFSET                     (0x238U)
#define RKCE_HASH_TD_ID                            (0x0U)
#define RKCE_HASH_TD_ID_HTDID_SHIFT                (0U)
#define RKCE_HASH_TD_ID_HTDID_MASK                 (0xFFFFFFFFU << RKCE_HASH_TD_ID_HTDID_SHIFT)
/* HASH_TD_ST */
#define RKCE_HASH_TD_ST_OFFSET                     (0x23CU)
#define RKCE_HASH_TD_ST                            (0x0U)
#define RKCE_HASH_TD_ST_FIRST_PKG_FLAG_SHIFT       (0U)
#define RKCE_HASH_TD_ST_FIRST_PKG_FLAG_MASK        (0x1U << RKCE_HASH_TD_ST_FIRST_PKG_FLAG_SHIFT)
#define RKCE_HASH_TD_ST_LAST_PKG_FLAG_SHIFT        (1U)
#define RKCE_HASH_TD_ST_LAST_PKG_FLAG_MASK         (0x1U << RKCE_HASH_TD_ST_LAST_PKG_FLAG_SHIFT)
#define RKCE_HASH_TD_ST_DMA_START_FLAG_SHIFT       (2U)
#define RKCE_HASH_TD_ST_DMA_START_FLAG_MASK        (0x1U << RKCE_HASH_TD_ST_DMA_START_FLAG_SHIFT)
#define RKCE_HASH_TD_ST_PRMPT_PKG_FLGA_SHIFT       (3U)
#define RKCE_HASH_TD_ST_PRMPT_PKG_FLGA_MASK        (0x1U << RKCE_HASH_TD_ST_PRMPT_PKG_FLGA_SHIFT)
/* HASH_CONTEXT_SIZE */
#define RKCE_HASH_CONTEXT_SIZE_OFFSET              (0x244U)
#define RKCE_HASH_CONTEXT_SIZE                     (0xD0U)
/* TD_POP_CTRL */
#define RKCE_TD_POP_CTRL_OFFSET                    (0x258U)
#define RKCE_TD_POP_CTRL                           (0x0U)
#define RKCE_TD_POP_CTRL_SYMM_TPR_SHIFT            (0U)
#define RKCE_TD_POP_CTRL_SYMM_TPR_MASK             (0x1U << RKCE_TD_POP_CTRL_SYMM_TPR_SHIFT)
#define RKCE_TD_POP_CTRL_HASH_TPR_SHIFT            (1U)
#define RKCE_TD_POP_CTRL_HASH_TPR_MASK             (0x1U << RKCE_TD_POP_CTRL_HASH_TPR_SHIFT)
/* ECC_CTL */
#define RKCE_ECC_CTL_OFFSET                        (0x3F0U)
#define RKCE_ECC_CTL_ECC_REQ_SHIFT                 (0U)
#define RKCE_ECC_CTL_ECC_REQ_MASK                  (0x1U << RKCE_ECC_CTL_ECC_REQ_SHIFT)
#define RKCE_ECC_CTL_FUNC_SEL_SHIFT                (4U)
#define RKCE_ECC_CTL_FUNC_SEL_MASK                 (0xFU << RKCE_ECC_CTL_FUNC_SEL_SHIFT)
#define RKCE_ECC_CTL_CURVE_MODE_SEL_SHIFT          (8U)
#define RKCE_ECC_CTL_CURVE_MODE_SEL_MASK           (0x1U << RKCE_ECC_CTL_CURVE_MODE_SEL_SHIFT)
#define RKCE_ECC_CTL_RAND_K_SRC_SHIFT              (12U)
#define RKCE_ECC_CTL_RAND_K_SRC_MASK               (0x1U << RKCE_ECC_CTL_RAND_K_SRC_SHIFT)
/* ECC_INT_EN */
#define RKCE_ECC_INT_EN_OFFSET                     (0x3F4U)
#define RKCE_ECC_INT_EN_DONE_INT_EN_SHIFT          (0U)
#define RKCE_ECC_INT_EN_DONE_INT_EN_MASK           (0x1U << RKCE_ECC_INT_EN_DONE_INT_EN_SHIFT)
/* ECC_INT_ST */
#define RKCE_ECC_INT_ST_OFFSET                     (0x3F8U)
#define RKCE_ECC_INT_ST_DONE_INT_ST_SHIFT          (0U)
#define RKCE_ECC_INT_ST_DONE_INT_ST_MASK           (0x1U << RKCE_ECC_INT_ST_DONE_INT_ST_SHIFT)
/* ECC_ABN_ST */
#define RKCE_ECC_ABN_ST_OFFSET                     (0x3FCU)
#define RKCE_ECC_ABN_ST                            (0x0U)
#define RKCE_ECC_ABN_ST_BAD_POINT_OUT_SHIFT        (0U)
#define RKCE_ECC_ABN_ST_BAD_POINT_OUT_MASK         (0x1U << RKCE_ECC_ABN_ST_BAD_POINT_OUT_SHIFT)
#define RKCE_ECC_ABN_ST_BAD_T_OUT_SHIFT            (1U)
#define RKCE_ECC_ABN_ST_BAD_T_OUT_MASK             (0x1U << RKCE_ECC_ABN_ST_BAD_T_OUT_SHIFT)
#define RKCE_ECC_ABN_ST_BAD_S_OUT_SHIFT            (2U)
#define RKCE_ECC_ABN_ST_BAD_S_OUT_MASK             (0x1U << RKCE_ECC_ABN_ST_BAD_S_OUT_SHIFT)
#define RKCE_ECC_ABN_ST_BAD_R_OUT_SHIFT            (3U)
#define RKCE_ECC_ABN_ST_BAD_R_OUT_MASK             (0x1U << RKCE_ECC_ABN_ST_BAD_R_OUT_SHIFT)
#define RKCE_ECC_ABN_ST_BAD_R_K_MID_SHIFT          (4U)
#define RKCE_ECC_ABN_ST_BAD_R_K_MID_MASK           (0x1U << RKCE_ECC_ABN_ST_BAD_R_K_MID_SHIFT)
#define RKCE_ECC_ABN_ST_BAD_S_IN_SHIFT             (5U)
#define RKCE_ECC_ABN_ST_BAD_S_IN_MASK              (0x1U << RKCE_ECC_ABN_ST_BAD_S_IN_SHIFT)
#define RKCE_ECC_ABN_ST_BAD_R_IN_SHIFT             (6U)
#define RKCE_ECC_ABN_ST_BAD_R_IN_MASK              (0x1U << RKCE_ECC_ABN_ST_BAD_R_IN_SHIFT)
#define RKCE_ECC_ABN_ST_BAD_K_IN_SHIFT             (7U)
#define RKCE_ECC_ABN_ST_BAD_K_IN_MASK              (0x1U << RKCE_ECC_ABN_ST_BAD_K_IN_SHIFT)
#define RKCE_ECC_ABN_ST_BAD_INV_OT_SHIFT           (8U)
#define RKCE_ECC_ABN_ST_BAD_INV_OT_MASK            (0x1U << RKCE_ECC_ABN_ST_BAD_INV_OT_SHIFT)
/* ECC_CURVE_WIDE */
#define RKCE_ECC_CURVE_WIDE_OFFSET                 (0x400U)
#define RKCE_ECC_CURVE_WIDE_CURVE_WIDE_SHIFT       (0U)
#define RKCE_ECC_CURVE_WIDE_CURVE_WIDE_MASK        (0x3FFU << RKCE_ECC_CURVE_WIDE_CURVE_WIDE_SHIFT)
/* ECC_MAX_CURVE_WIDE */
#define RKCE_ECC_MAX_CURVE_WIDE_OFFSET             (0x404U)
#define RKCE_ECC_MAX_CURVE_WIDE                    (0x100U)
/* ECC_DATA_ENDIAN */
#define RKCE_ECC_DATA_ENDIAN_OFFSET                (0x408U)
/* PKA_RAM_CTL */
#define RKCE_PKA_RAM_CTL_OFFSET                    (0x480U)
#define RKCE_PKA_RAM_CTL_PKA_RDY                   BIT(0)
#define RKCE_PKA_RAM_CTL_RAM_PKA_RDY_SHIFT         (0U)
#define RKCE_PKA_RAM_CTL_RAM_PKA_RDY_MASK          (0x3U << RKCE_PKA_RAM_CTL_RAM_PKA_RDY_SHIFT)
/* PKA_RAM_ST */
#define RKCE_PKA_RAM_ST_OFFSET                     (0x484U)
#define RKCE_PKA_RAM_ST                            (0x1U)
#define RKCE_PKA_RAM_ST_CLK_RAM_RDY_SHIFT          (0U)
#define RKCE_PKA_RAM_ST_CLK_RAM_RDY_MASK           (0x1U << RKCE_PKA_RAM_ST_CLK_RAM_RDY_SHIFT)
/* AES_VER */
#define RKCE_AES_VER_OFFSET                        (0x680U)
#define RKCE_AES_VER_ECB_FLAG_SHIFT                (0U)
#define RKCE_AES_VER_ECB_FLAG_MASK                 (0x1U << RKCE_AES_VER_ECB_FLAG_SHIFT)
#define RKCE_AES_VER_CBC_FLAG_SHIFT                (1U)
#define RKCE_AES_VER_CBC_FLAG_MASK                 (0x1U << RKCE_AES_VER_CBC_FLAG_SHIFT)
#define RKCE_AES_VER_CTS_FLAG_SHIFT                (2U)
#define RKCE_AES_VER_CTS_FLAG_MASK                 (0x1U << RKCE_AES_VER_CTS_FLAG_SHIFT)
#define RKCE_AES_VER_CTR_FLAG_SHIFT                (3U)
#define RKCE_AES_VER_CTR_FLAG_MASK                 (0x1U << RKCE_AES_VER_CTR_FLAG_SHIFT)
#define RKCE_AES_VER_CFB_FLAG_SHIFT                (4U)
#define RKCE_AES_VER_CFB_FLAG_MASK                 (0x1U << RKCE_AES_VER_CFB_FLAG_SHIFT)
#define RKCE_AES_VER_OFB_FLAG_SHIFT                (5U)
#define RKCE_AES_VER_OFB_FLAG_MASK                 (0x1U << RKCE_AES_VER_OFB_FLAG_SHIFT)
#define RKCE_AES_VER_XTS_FLAG_SHIFT                (6U)
#define RKCE_AES_VER_XTS_FLAG_MASK                 (0x1U << RKCE_AES_VER_XTS_FLAG_SHIFT)
#define RKCE_AES_VER_CCM_FLAG_SHIFT                (7U)
#define RKCE_AES_VER_CCM_FLAG_MASK                 (0x1U << RKCE_AES_VER_CCM_FLAG_SHIFT)
#define RKCE_AES_VER_GCM_FLAG_SHIFT                (8U)
#define RKCE_AES_VER_GCM_FLAG_MASK                 (0x1U << RKCE_AES_VER_GCM_FLAG_SHIFT)
#define RKCE_AES_VER_CMAC_FLAG_SHIFT               (9U)
#define RKCE_AES_VER_CMAC_FLAG_MASK                (0x1U << RKCE_AES_VER_CMAC_FLAG_SHIFT)
#define RKCE_AES_VER_CBC_MAC_FLAG_SHIFT            (10U)
#define RKCE_AES_VER_CBC_MAC_FLAG_MASK             (0x1U << RKCE_AES_VER_CBC_MAC_FLAG_SHIFT)
#define RKCE_AES_VER_BYPASS_SHIFT                  (12U)
#define RKCE_AES_VER_BYPASS_MASK                   (0x1U << RKCE_AES_VER_BYPASS_SHIFT)
#define RKCE_AES_VER_AES128_FLAG_SHIFT             (16U)
#define RKCE_AES_VER_AES128_FLAG_MASK              (0x1U << RKCE_AES_VER_AES128_FLAG_SHIFT)
#define RKCE_AES_VER_AES192_FLAG_SHIFT             (17U)
#define RKCE_AES_VER_AES192_FLAG_MASK              (0x1U << RKCE_AES_VER_AES192_FLAG_SHIFT)
#define RKCE_AES_VER_AES256_FLAG_SHIFT             (18U)
#define RKCE_AES_VER_AES256_FLAG_MASK              (0x1U << RKCE_AES_VER_AES256_FLAG_SHIFT)
#define RKCE_AES_VER_LOCKSTEP_FLAG_SHIFT           (20U)
#define RKCE_AES_VER_LOCKSTEP_FLAG_MASK            (0x1U << RKCE_AES_VER_LOCKSTEP_FLAG_SHIFT)
#define RKCE_AES_VER_SECURE_FLAG_SHIFT             (21U)
#define RKCE_AES_VER_SECURE_FLAG_MASK              (0x1U << RKCE_AES_VER_SECURE_FLAG_SHIFT)
/* DES_VER */
#define RKCE_DES_VER_OFFSET                        (0x684U)
#define RKCE_DES_VER_ECB_FLAG_SHIFT                (0U)
#define RKCE_DES_VER_ECB_FLAG_MASK                 (0x1U << RKCE_DES_VER_ECB_FLAG_SHIFT)
#define RKCE_DES_VER_CBC_FLAG_SHIFT                (1U)
#define RKCE_DES_VER_CBC_FLAG_MASK                 (0x1U << RKCE_DES_VER_CBC_FLAG_SHIFT)
#define RKCE_DES_VER_CFB_FLAG_SHIFT                (4U)
#define RKCE_DES_VER_CFB_FLAG_MASK                 (0x1U << RKCE_DES_VER_CFB_FLAG_SHIFT)
#define RKCE_DES_VER_OFB_FLAG_SHIFT                (5U)
#define RKCE_DES_VER_OFB_FLAG_MASK                 (0x1U << RKCE_DES_VER_OFB_FLAG_SHIFT)
#define RKCE_DES_VER_TDES_FLAG_SHIFT               (16U)
#define RKCE_DES_VER_TDES_FLAG_MASK                (0x1U << RKCE_DES_VER_TDES_FLAG_SHIFT)
#define RKCE_DES_VER_EEE_FLAG_SHIFT                (17U)
#define RKCE_DES_VER_EEE_FLAG_MASK                 (0x1U << RKCE_DES_VER_EEE_FLAG_SHIFT)
#define RKCE_DES_VER_EDE_FLAG_SHIFT                (18U)
#define RKCE_DES_VER_EDE_FLAG_MASK                 (0x1U << RKCE_DES_VER_EDE_FLAG_SHIFT)
#define RKCE_DES_VER_LOCKSTEP_FLAG_SHIFT           (20U)
#define RKCE_DES_VER_LOCKSTEP_FLAG_MASK            (0x1U << RKCE_DES_VER_LOCKSTEP_FLAG_SHIFT)
#define RKCE_DES_VER_SECURE_FLAG_SHIFT             (21U)
#define RKCE_DES_VER_SECURE_FLAG_MASK              (0x1U << RKCE_DES_VER_SECURE_FLAG_SHIFT)
/* SM4_VER */
#define RKCE_SM4_VER_OFFSET                        (0x688U)
#define RKCE_SM4_VER_ECB_FLAG_SHIFT                (0U)
#define RKCE_SM4_VER_ECB_FLAG_MASK                 (0x1U << RKCE_SM4_VER_ECB_FLAG_SHIFT)
#define RKCE_SM4_VER_CBC_FLAG_SHIFT                (1U)
#define RKCE_SM4_VER_CBC_FLAG_MASK                 (0x1U << RKCE_SM4_VER_CBC_FLAG_SHIFT)
#define RKCE_SM4_VER_CTS_FLAG_SHIFT                (2U)
#define RKCE_SM4_VER_CTS_FLAG_MASK                 (0x1U << RKCE_SM4_VER_CTS_FLAG_SHIFT)
#define RKCE_SM4_VER_CTR_FLAG_SHIFT                (3U)
#define RKCE_SM4_VER_CTR_FLAG_MASK                 (0x1U << RKCE_SM4_VER_CTR_FLAG_SHIFT)
#define RKCE_SM4_VER_CFB_FLAG_SHIFT                (4U)
#define RKCE_SM4_VER_CFB_FLAG_MASK                 (0x1U << RKCE_SM4_VER_CFB_FLAG_SHIFT)
#define RKCE_SM4_VER_OFB_FLAG_SHIFT                (5U)
#define RKCE_SM4_VER_OFB_FLAG_MASK                 (0x1U << RKCE_SM4_VER_OFB_FLAG_SHIFT)
#define RKCE_SM4_VER_XTS_FLAG_SHIFT                (6U)
#define RKCE_SM4_VER_XTS_FLAG_MASK                 (0x1U << RKCE_SM4_VER_XTS_FLAG_SHIFT)
#define RKCE_SM4_VER_CCM_FLAG_SHIFT                (7U)
#define RKCE_SM4_VER_CCM_FLAG_MASK                 (0x1U << RKCE_SM4_VER_CCM_FLAG_SHIFT)
#define RKCE_SM4_VER_GCM_FLAG_SHIFT                (8U)
#define RKCE_SM4_VER_GCM_FLAG_MASK                 (0x1U << RKCE_SM4_VER_GCM_FLAG_SHIFT)
#define RKCE_SM4_VER_CMAC_FLAG_SHIFT               (9U)
#define RKCE_SM4_VER_CMAC_FLAG_MASK                (0x1U << RKCE_SM4_VER_CMAC_FLAG_SHIFT)
#define RKCE_SM4_VER_CBC_MAC_FLAG_SHIFT            (10U)
#define RKCE_SM4_VER_CBC_MAC_FLAG_MASK             (0x1U << RKCE_SM4_VER_CBC_MAC_FLAG_SHIFT)
#define RKCE_SM4_VER_LOCKSTEP_FLAG_SHIFT           (20U)
#define RKCE_SM4_VER_LOCKSTEP_FLAG_MASK            (0x1U << RKCE_SM4_VER_LOCKSTEP_FLAG_SHIFT)
#define RKCE_SM4_VER_SECURE_FLAG_SHIFT             (21U)
#define RKCE_SM4_VER_SECURE_FLAG_MASK              (0x1U << RKCE_SM4_VER_SECURE_FLAG_SHIFT)
/* HASH_VER */
#define RKCE_HASH_VER_OFFSET                       (0x68CU)
#define RKCE_HASH_VER_SHA1_FLAG_SHIFT              (0U)
#define RKCE_HASH_VER_SHA1_FLAG_MASK               (0x1U << RKCE_HASH_VER_SHA1_FLAG_SHIFT)
#define RKCE_HASH_VER_SHA224_FLAG_SHIFT            (1U)
#define RKCE_HASH_VER_SHA224_FLAG_MASK             (0x1U << RKCE_HASH_VER_SHA224_FLAG_SHIFT)
#define RKCE_HASH_VER_SHA256_FLAG_SHIFT            (2U)
#define RKCE_HASH_VER_SHA256_FLAG_MASK             (0x1U << RKCE_HASH_VER_SHA256_FLAG_SHIFT)
#define RKCE_HASH_VER_SHA384_FLAG_SHIFT            (3U)
#define RKCE_HASH_VER_SHA384_FLAG_MASK             (0x1U << RKCE_HASH_VER_SHA384_FLAG_SHIFT)
#define RKCE_HASH_VER_SHA512_FLAG_SHIFT            (4U)
#define RKCE_HASH_VER_SHA512_FLAG_MASK             (0x1U << RKCE_HASH_VER_SHA512_FLAG_SHIFT)
#define RKCE_HASH_VER_SHA512_224_FLAG_SHIFT        (5U)
#define RKCE_HASH_VER_SHA512_224_FLAG_MASK         (0x1U << RKCE_HASH_VER_SHA512_224_FLAG_SHIFT)
#define RKCE_HASH_VER_SHA512_256_FLAG_SHIFT        (6U)
#define RKCE_HASH_VER_SHA512_256_FLAG_MASK         (0x1U << RKCE_HASH_VER_SHA512_256_FLAG_SHIFT)
#define RKCE_HASH_VER_MD5_FLAG_SHIFT               (7U)
#define RKCE_HASH_VER_MD5_FLAG_MASK                (0x1U << RKCE_HASH_VER_MD5_FLAG_SHIFT)
#define RKCE_HASH_VER_SM3_FLAG_SHIFT               (8U)
#define RKCE_HASH_VER_SM3_FLAG_MASK                (0x1U << RKCE_HASH_VER_SM3_FLAG_SHIFT)
#define RKCE_HASH_VER_LOCKSTEP_FLAG_SHIFT          (20U)
#define RKCE_HASH_VER_LOCKSTEP_FLAG_MASK           (0x1U << RKCE_HASH_VER_LOCKSTEP_FLAG_SHIFT)
/* HMAC_VER */
#define RKCE_HMAC_VER_OFFSET                       (0x690U)
#define RKCE_HMAC_VER_SHA1_FLAG_SHIFT              (0U)
#define RKCE_HMAC_VER_SHA1_FLAG_MASK               (0x1U << RKCE_HMAC_VER_SHA1_FLAG_SHIFT)
#define RKCE_HMAC_VER_SHA256_FLAG_SHIFT            (1U)
#define RKCE_HMAC_VER_SHA256_FLAG_MASK             (0x1U << RKCE_HMAC_VER_SHA256_FLAG_SHIFT)
#define RKCE_HMAC_VER_SHA512_FLAG_SHIFT            (2U)
#define RKCE_HMAC_VER_SHA512_FLAG_MASK             (0x1U << RKCE_HMAC_VER_SHA512_FLAG_SHIFT)
#define RKCE_HMAC_VER_MD5_FLAG_SHIFT               (3U)
#define RKCE_HMAC_VER_MD5_FLAG_MASK                (0x1U << RKCE_HMAC_VER_MD5_FLAG_SHIFT)
#define RKCE_HMAC_VER_SM3_FLAG_SHIFT               (4U)
#define RKCE_HMAC_VER_SM3_FLAG_MASK                (0x1U << RKCE_HMAC_VER_SM3_FLAG_SHIFT)
#define RKCE_HMAC_VER_LOCKSTEP_FLAG_SHIFT          (20U)
#define RKCE_HMAC_VER_LOCKSTEP_FLAG_MASK           (0x1U << RKCE_HMAC_VER_LOCKSTEP_FLAG_SHIFT)
/* PKA_VER */
#define RKCE_PKA_VER_OFFSET                        (0x698U)
/* EXTRA_FEATURE */
#define RKCE_EXTRA_FEATURE_OFFSET                  (0x69CU)
#define RKCE_EXTRA_FEATURE_AXI_EXPAND_BIT_SHIFT    (0U)
#define RKCE_EXTRA_FEATURE_AXI_EXPAND_BIT_MASK     (0xFU << RKCE_EXTRA_FEATURE_AXI_EXPAND_BIT_SHIFT)
/* CE_VER */
#define RKCE_CE_VER_OFFSET                         (0x6F0U)
/* PKA_MEM_MAP0 */
#define RKCE_PKA_MEM_MAP0_OFFSET                   (0x800U)
#define RKCE_MAP_REG_NUM                           (32)
/* PKA_OPCODE */
#define RKCE_PKA_OPCODE_OFFSET                     (0x880U)
#define RKCE_PKA_OPCODE_TAG_SHIFT                  (0U)
#define RKCE_PKA_OPCODE_TAG_MASK                   (0x3FU << RKCE_PKA_OPCODE_TAG_SHIFT)
#define RKCE_PKA_OPCODE_REG_R_SHIFT                (6U)
#define RKCE_PKA_OPCODE_REG_R_MASK                 (0x3FU << RKCE_PKA_OPCODE_REG_R_SHIFT)
#define RKCE_PKA_OPCODE_R_DIS_SHIFT                (11U)
#define RKCE_PKA_OPCODE_REG_B_SHIFT                (12U)
#define RKCE_PKA_OPCODE_REG_B_MASK                 (0x3FU << RKCE_PKA_OPCODE_REG_B_SHIFT)
#define RKCE_PKA_OPCODE_B_IMMED_SHIFT              (17U)
#define RKCE_PKA_OPCODE_REG_A_SHIFT                (18U)
#define RKCE_PKA_OPCODE_REG_A_MASK                 (0x3FU << RKCE_PKA_OPCODE_REG_A_SHIFT)
#define RKCE_PKA_OPCODE_A_IMMED_SHIFT	           (23U)
#define RKCE_PKA_OPCODE_LEN_SHIFT                  (24U)
#define RKCE_PKA_OPCODE_LEN_MASK                   (0x7U << RKCE_PKA_OPCODE_LEN_SHIFT)
#define RKCE_PKA_OPCODE_OPCODE_SHIFT               (27U)
#define RKCE_PKA_OPCODE_OPCODE_MASK                (0x1FU << RKCE_PKA_OPCODE_OPCODE_SHIFT)
/* N_NP_T0_T1_ADDR */
#define RKCE_N_NP_T0_T1_ADDR_OFFSET                (0x884U)
#define RKCE_N_NP_T0_T1_ADDR_REG_N_SHIFT           (0U)
#define RKCE_N_NP_T0_T1_ADDR_REG_N_MASK            (0x1FU << RKCE_N_NP_T0_T1_ADDR_REG_N_SHIFT)
#define RKCE_N_NP_T0_T1_ADDR_REG_NP_SHIFT          (5U)
#define RKCE_N_NP_T0_T1_ADDR_REG_NP_MASK           (0x1FU << RKCE_N_NP_T0_T1_ADDR_REG_NP_SHIFT)
#define RKCE_N_NP_T0_T1_ADDR_REG_T0_SHIFT          (10U)
#define RKCE_N_NP_T0_T1_ADDR_REG_T0_MASK           (0x1FU << RKCE_N_NP_T0_T1_ADDR_REG_T0_SHIFT)
#define RKCE_N_NP_T0_T1_ADDR_REG_T1_SHIFT          (15U)
#define RKCE_N_NP_T0_T1_ADDR_REG_T1_MASK           (0x1FU << RKCE_N_NP_T0_T1_ADDR_REG_T1_SHIFT)
/* PKA_STATUS */
#define RKCE_PKA_STATUS_OFFSET                     (0x888U)
#define RKCE_PKA_STATUS                            (0x1U)
#define RKCE_PKA_STATUS_PIPE_IS_BUSY_SHIFT         (0U)
#define RKCE_PKA_STATUS_PIPE_IS_BUSY_MASK          (0x1U << RKCE_PKA_STATUS_PIPE_IS_BUSY_SHIFT)
#define RKCE_PKA_STATUS_PKA_BUSY_SHIFT             (1U)
#define RKCE_PKA_STATUS_PKA_BUSY_MASK              (0x1U << RKCE_PKA_STATUS_PKA_BUSY_SHIFT)
#define RKCE_PKA_STATUS_ALU_OUT_ZERO_SHIFT         (2U)
#define RKCE_PKA_STATUS_ALU_OUT_ZERO_MASK          (0x1U << RKCE_PKA_STATUS_ALU_OUT_ZERO_SHIFT)
#define RKCE_PKA_STATUS_ALU_MOD_OVFLW_SHIFT        (3U)
#define RKCE_PKA_STATUS_ALU_MOD_OVFLW_MASK         (0x1U << RKCE_PKA_STATUS_ALU_MOD_OVFLW_SHIFT)
#define RKCE_PKA_STATUS_DIV_BY_ZERO_SHIFT          (4U)
#define RKCE_PKA_STATUS_DIV_BY_ZERO_MASK           (0x1U << RKCE_PKA_STATUS_DIV_BY_ZERO_SHIFT)
#define RKCE_PKA_STATUS_ALU_CARRY_SHIFT            (5U)
#define RKCE_PKA_STATUS_ALU_CARRY_MASK             (0x1U << RKCE_PKA_STATUS_ALU_CARRY_SHIFT)
#define RKCE_PKA_STATUS_ALU_SIGN_OUT_SHIFT         (6U)
#define RKCE_PKA_STATUS_ALU_SIGN_OUT_MASK          (0x1U << RKCE_PKA_STATUS_ALU_SIGN_OUT_SHIFT)
#define RKCE_PKA_STATUS_MODINV_OF_ZERO_SHIFT       (7U)
#define RKCE_PKA_STATUS_MODINV_OF_ZERO_MASK        (0x1U << RKCE_PKA_STATUS_MODINV_OF_ZERO_SHIFT)
#define RKCE_PKA_STATUS_PKA_CPU_BUSY_SHIFT         (8U)
#define RKCE_PKA_STATUS_PKA_CPU_BUSY_MASK          (0x1U << RKCE_PKA_STATUS_PKA_CPU_BUSY_SHIFT)
#define RKCE_PKA_STATUS_OPCODE_SHIFT               (9U)
#define RKCE_PKA_STATUS_OPCODE_MASK                (0x1FU << RKCE_PKA_STATUS_OPCODE_SHIFT)
#define RKCE_PKA_STATUS_TAG_SHIFT                  (14U)
#define RKCE_PKA_STATUS_TAG_MASK                   (0x3FU << RKCE_PKA_STATUS_TAG_SHIFT)
/* PKA_L0 */
#define RKCE_PKA_L0_OFFSET                         (0x890U)
#define RKCE_LEN_REG_NUM                           (8)
/* PKA_PIPE_RDY */
#define RKCE_PKA_PIPE_RDY_OFFSET                   (0x8B0U)
#define RKCE_PKA_PIPE_RDY                          (0x1U)
#define RKCE_PKA_PIPE_RDY_PKA_PIPE_RDY_SHIFT       (0U)
#define RKCE_PKA_PIPE_RDY_PKA_PIPE_RDY_MASK        (0x1U << RKCE_PKA_PIPE_RDY_PKA_PIPE_RDY_SHIFT)
/* PKA_DONE */
#define RKCE_PKA_DONE_OFFSET                       (0x8B4U)
#define RKCE_PKA_DONE                              (0x1U)
#define RKCE_PKA_DONE_PKA_DONE_SHIFT               (0U)
#define RKCE_PKA_DONE_PKA_DONE_MASK                (0x1U << RKCE_PKA_DONE_PKA_DONE_SHIFT)
/* PKA_INT_ENA */
#define RKCE_PKA_INT_ENA_OFFSET                    (0x8D4U)
#define RKCE_PKA_INT_ENA_PKA_INT_ENA_SHIFT         (0U)
#define RKCE_PKA_INT_ENA_PKA_INT_ENA_MASK          (0x1U << RKCE_PKA_INT_ENA_PKA_INT_ENA_SHIFT)
/* PKA_INT_ST */
#define RKCE_PKA_INT_ST_OFFSET                     (0x8D8U)
#define RKCE_PKA_INT_ST_PKA_INT_ST_SHIFT           (0U)
#define RKCE_PKA_INT_ST_PKA_INT_ST_MASK            (0x1U << RKCE_PKA_INT_ST_PKA_INT_ST_SHIFT)
/* TD0_TD1_TX_ADDR */
#define RKCE_TD0_TD1_TX_ADDR_OFFSET                (0x8DCU)
#define RKCE_TD0_TD1_TX_ADDR_REG_TD0_SHIFT         (0U)
#define RKCE_TD0_TD1_TX_ADDR_REG_TD0_MASK          (0x1FU << RKCE_TD0_TD1_TX_ADDR_REG_TD0_SHIFT)
#define RKCE_TD0_TD1_TX_ADDR_REG_TD1_SHIFT         (5U)
#define RKCE_TD0_TD1_TX_ADDR_REG_TD1_MASK          (0x1FU << RKCE_TD0_TD1_TX_ADDR_REG_TD1_SHIFT)
#define RKCE_TD0_TD1_TX_ADDR_REG_TX_SHIFT          (10U)
#define RKCE_TD0_TD1_TX_ADDR_REG_TX_MASK           (0x1FU << RKCE_TD0_TD1_TX_ADDR_REG_TX_SHIFT)
#define RKCE_TD0_TD1_TX_ADDR_PKA_ASCA_EN_SHIFT     (31U)
#define RKCE_TD0_TD1_TX_ADDR_PKA_ASCA_EN_MASK      (0x1U << RKCE_TD0_TD1_TX_ADDR_PKA_ASCA_EN_SHIFT)
/* SRAM_ADDR */
#define RKCE_SRAM_ADDR_OFFSET                      (0x1000U)
#define RKCE_SRAM_ADDR_SRAM_ADDR_SHIFT             (0U)
#define RKCE_SRAM_ADDR_SRAM_ADDR_MASK              (0xFFFFFFFFU << RKCE_SRAM_ADDR_SRAM_ADDR_SHIFT)

#define	RKCE_SRAM_SIZE                             (0x1000U)

#endif /* __RKCE_REG_H */
