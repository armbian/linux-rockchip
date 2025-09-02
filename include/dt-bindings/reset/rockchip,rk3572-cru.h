/* SPDX-License-Identifier: (GPL-2.0-only OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Elaine Zhang <zhangqing@rock-chips.com>
 */

#ifndef _DT_BINDINGS_RESET_ROCKCHIP_RK3572_H
#define _DT_BINDINGS_RESET_ROCKCHIP_RK3572_H

/* ==========================list all of reset fields id=========================== */
/* CRU-->SOFTRST_CON01 */
#define SRST_ARESETN_TOP_BIU                     0
#define SRST_PRESETN_TOP_BIU                     1
#define SRST_ARESETN_TOP_MID_BIU                 2
#define SRST_HRESETN_TOP_BIU                     3

/* CRU-->SOFTRST_CON02 */
#define SRST_HRESETN_TOP_FW                      4

/* CRU-->SOFTRST_CON06 */
#define SRST_RESETN_TEST_JUDGE_24M_TOP           5
#define SRST_RESETN_TEST_JUDGE_TOP               6

/* CRU-->SOFTRST_CON07 */
#define SRST_HRESETN_AUDIO_BIU                   7
#define SRST_HRESETN_ASRC_2CH_0                  8
#define SRST_HRESETN_ASRC_2CH_1                  9
#define SRST_RESETN_ASRC_2CH_0                   10
#define SRST_RESETN_ASRC_2CH_1                   11
#define SRST_MRESETN_SAI0                        12
#define SRST_HRESETN_SAI0                        13
#define SRST_HRESETN_SPDIF_RX0                   14
#define SRST_MRESETN_SPDIF_RX0                   15

/* CRU-->SOFTRST_CON08 */
#define SRST_MRESETN_SAI1                        16
#define SRST_HRESETN_SAI1                        17
#define SRST_MRESETN_SAI2                        18
#define SRST_HRESETN_SAI2                        19
#define SRST_MRESETN_SAI3                        20
#define SRST_HRESETN_SAI3                        21
#define SRST_HRESETN_ACDCDIG_DSM                 22

/* CRU-->SOFTRST_CON09 */
#define SRST_MRESETN_ACDCDIG_DSM                 23
#define SRST_PRESETN_AUDIO_BIU                   24
#define SRST_PRESETN_AUDIO_GRF                   25
#define SRST_HRESETN_SPDIF_TX0                   26
#define SRST_MRESETN_SPDIF_TX0                   27
#define SRST_HRESETN_SPDIF_TX1                   28
#define SRST_MRESETN_SPDIF_TX1                   29
#define SRST_RESETN_TEST_JUDGE_24M_AUDIO         30
#define SRST_RESETN_TEST_JUDGE_AUDIO             31

/* CRU-->SOFTRST_CON11 */
#define SRST_ARESETN_BUS_BIU                     32
#define SRST_ARESETN_BUS_FW                      33
#define SRST_PRESETN_BUS_BIU                     34
#define SRST_PRESETN_CRU                         35
#define SRST_HRESETN_CAN0                        36
#define SRST_RESETN_CAN0                         37
#define SRST_HRESETN_CAN1                        38
#define SRST_RESETN_CAN1                         39
#define SRST_HRESETN_CAN2                        40
#define SRST_RESETN_CAN2                         41
#define SRST_HRESETN_CAN3                        42
#define SRST_RESETN_CAN3                         43

/* CRU-->SOFTRST_CON12 */
#define SRST_ARESETN_DECOM_BIU                   44
#define SRST_HRESETN_BUS_BIU                     45
#define SRST_RESETN_KEY_SHIFT                    46
#define SRST_PRESETN_I2C1                        47
#define SRST_PRESETN_I2C2                        48
#define SRST_PRESETN_I2C3                        49
#define SRST_PRESETN_I2C4                        50
#define SRST_PRESETN_I2C5                        51
#define SRST_PRESETN_I2C6                        52
#define SRST_PRESETN_I2C7                        53
#define SRST_PRESETN_I2C8                        54
#define SRST_PRESETN_I2C9                        55
#define SRST_PRESETN_WDT_BUSMCU                  56
#define SRST_TRESETN_WDT_BUSMCU                  57

/* CRU-->SOFTRST_CON13 */
#define SRST_ARESETN_GIC                         58
#define SRST_RESETN_I2C1                         59
#define SRST_RESETN_I2C2                         60
#define SRST_RESETN_I2C3                         61
#define SRST_RESETN_I2C4                         62
#define SRST_RESETN_I2C5                         63
#define SRST_RESETN_I2C6                         64
#define SRST_RESETN_I2C7                         65
#define SRST_RESETN_I2C8                         66
#define SRST_RESETN_I2C9                         67
#define SRST_PRESETN_SARADC                      68

/* CRU-->SOFTRST_CON14 */
#define SRST_RESETN_SARADC                       69
#define SRST_RESETN_SARADC_32K                   70
#define SRST_PRESETN_TSADC                       71
#define SRST_RESETN_TSADC                        72
#define SRST_PRESETN_UART0                       73
#define SRST_PRESETN_UART2                       74
#define SRST_PRESETN_UART3                       75
#define SRST_PRESETN_UART4                       76
#define SRST_PRESETN_UART5                       77
#define SRST_PRESETN_UART6                       78
#define SRST_PRESETN_UART7                       79
#define SRST_PRESETN_UART8                       80
#define SRST_PRESETN_UART9                       81
#define SRST_PRESETN_UART10                      82
#define SRST_PRESETN_UART11                      83
#define SRST_SRESETN_UART0                       84

/* CRU-->SOFTRST_CON15 */
#define SRST_SRESETN_UART2                       85
#define SRST_SRESETN_UART3                       86
#define SRST_SRESETN_UART4                       87
#define SRST_SRESETN_UART5                       88
#define SRST_SRESETN_UART6                       89
#define SRST_SRESETN_UART7                       90

/* CRU-->SOFTRST_CON16 */
#define SRST_SRESETN_UART8                       91
#define SRST_SRESETN_UART9                       92
#define SRST_SRESETN_UART10                      93
#define SRST_SRESETN_UART11                      94
#define SRST_PRESETN_SPI0                        95
#define SRST_PRESETN_SPI1                        96
#define SRST_PRESETN_SPI2                        97
#define SRST_PRESETN_SPI3                        98
#define SRST_PRESETN_SPI4                        99
#define SRST_RESETN_SPI0                         100
#define SRST_RESETN_SPI1                         101
#define SRST_RESETN_SPI2                         102
#define SRST_RESETN_SPI3                         103

/* CRU-->SOFTRST_CON17 */
#define SRST_RESETN_SPI4                         104
#define SRST_PRESETN_WDT0                        105
#define SRST_TRESETN_WDT0                        106
#define SRST_PRESETN_SYS_GRF                     107
#define SRST_PRESETN_PWM1                        108
#define SRST_RESETN_PWM1                         109
#define SRST_PRESETN_PWM2                        110
#define SRST_RESETN_PWM2                         111

/* CRU-->SOFTRST_CON18 */
#define SRST_PRESETN_BUSTIMER0                   112
#define SRST_PRESETN_BUSTIMER1                   113
#define SRST_RESETN_TIMER0                       114
#define SRST_RESETN_TIMER1                       115
#define SRST_RESETN_TIMER2                       116
#define SRST_RESETN_TIMER3                       117
#define SRST_RESETN_TIMER4                       118
#define SRST_RESETN_TIMER5                       119
#define SRST_PRESETN_MAILBOX0                    120

/* CRU-->SOFTRST_CON19 */
#define SRST_PRESETN_GPIO1                       121
#define SRST_DBRESETN_GPIO1                      122
#define SRST_PRESETN_GPIO2                       123
#define SRST_DBRESETN_GPIO2                      124
#define SRST_PRESETN_GPIO3                       125
#define SRST_DBRESETN_GPIO3                      126
#define SRST_PRESETN_GPIO4                       127
#define SRST_DBRESETN_GPIO4                      128
#define SRST_ARESETN_DECOM                       129
#define SRST_PRESETN_DECOM                       130
#define SRST_DRESETN_DECOM                       131
#define SRST_RESETN_TIMER6                       132
#define SRST_RESETN_TIMER7                       133
#define SRST_RESETN_TIMER8                       134
#define SRST_RESETN_TIMER9                       135

/* CRU-->SOFTRST_CON20 */
#define SRST_RESETN_TIMER10                      136
#define SRST_RESETN_TIMER11                      137
#define SRST_ARESETN_RKDMA0                      138
#define SRST_PRESETN_RKDMA0                      139
#define SRST_ARESETN_RKDMA1                      140
#define SRST_PRESETN_RKDMA1                      141
#define SRST_ARESETN_RKDMA2                      142
#define SRST_PRESETN_RKDMA2                      143
#define SRST_ARESETN_RKDMA3                      144
#define SRST_PRESETN_RKDMA3                      145
#define SRST_PRESETN_SPINLOCK                    146
#define SRST_RESETN_REF_PVTPLL_BUS               147
#define SRST_RESETN_I3C                          148
#define SRST_HRESETN_I3C                         149

/* CRU-->SOFTRST_CON21 */
#define SRST_RESETN_BUSMCU_BIU                   150
#define SRST_CORERESETN_N320                     151
#define SRST_PORESETN_N320                       152
#define SRST_RESETN_MCU_AXI_INTEGRITY            153
#define SRST_PRESETN_INTMUX2PMU                  154
#define SRST_PRESETN_INTMUX2DDR                  155
#define SRST_PRESETN_PVTPLL_BUS                  156
#define SRST_RESETN_HDMITXHPD                    157

/* CRU-->SOFTRST_CON22 */
#define SRST_PRESETN_APB2ASB_HDMITX              158
#define SRST_SRESETN_APB2ASB_HDMITX              159
#define SRST_PRESETN_APB2ASB_MIPICSI             160
#define SRST_SRESETN_APB2ASB_MIPICSI             161
#define SRST_PRESETN_APB2ASB_VCCIO0_3            162
#define SRST_SRESETN_APB2ASB_VCCIO0_3            163
#define SRST_PRESETN_APB2ASB_VCCIO1_2_4          164
#define SRST_SRESETN_APB2ASB_VCCIO1_2_4          165
#define SRST_PRESETN_APB2ASB_VCCIO5_6            166
#define SRST_SRESETN_APB2ASB_VCCIO5_6            167
#define SRST_RESETN_TEST_JUDGE_24M_BUS           168
#define SRST_RESETN_TEST_JUDGE_BUS               169

/* CRU-->SOFTRST_CON23 */
#define SRST_PRESETN_DDR_MON                     170
#define SRST_PRESETN_DDR_BIU                     171
#define SRST_PRESETN_DDR_UPCTL                   172
#define SRST_TMRESETN_DDR_MON                    173
#define SRST_ARESETN_DDR_BIU                     174
#define SRST_PRESETN_DDR_BIU_FW                  175
#define SRST_RESETN_DDR_MON                      176
#define SRST_PRESETN_DDR_HWLP                    177

/* CRU-->SOFTRST_CON24 */
#define SRST_ARESETN_DDR_MSCH_BIU                178
#define SRST_ARESETN_DDR_MSCH_FW                 179
#define SRST_RESETN_DDR_SCRAMBLE                 180
#define SRST_LRESETN_DDRSCH_BIU                  181

/* CRU-->SOFTRST_CON25 */
#define SRST_RESETN_DDR_TIMER0                   182
#define SRST_RESETN_DDR_TIMER1                   183
#define SRST_TRESETN_DDR_WDT                     184
#define SRST_PRESETN_WDT                         185
#define SRST_PRESETN_TIMER                       186
#define SRST_PRESETN_DDR_GRF                     187

/* CRU-->SOFTRST_CON26 */
#define SRST_RESETN_DDR_UPCTL                    188
#define SRST_ARESETN_DDR_UPCTL_0                 189
#define SRST_ARESETN_DDR_UPCTL_1                 190
#define SRST_ARESETN_DDR_UPCTL_2                 191
#define SRST_ARESETN_DDR_UPCTL_3                 192
#define SRST_ARESETN_DDR_UPCTL_4                 193
#define SRST_ARESETN_DDR_UPCTL_5                 194

/* CRU-->SOFTRST_CON27 */
#define SRST_RESETN_DDRMCU_BIU                   195
#define SRST_CORERESETN_N320_DDR                 196
#define SRST_PORESETN_N320_DDR                   197
#define SRST_RESETN_TEST_JUDGE_24M_DDR           198
#define SRST_RESETN_TEST_JUDGE_DDR               199

/* CRU-->SOFTRST_CON31 */
#define SRST_ARESETN_RKNN                        200
#define SRST_ARESETN_RKNN_RV                     201
#define SRST_ARESETN_RKNN_BIU                    202
#define SRST_ARESETN_RKNN_FW                     203
#define SRST_PRESETN_RKNN_BIU                    204
#define SRST_PRESETN_PVTPLL_NPU                  205
#define SRST_RESETN_NPU_PVTPLL                   206
#define SRST_RESETN_24M_RKNN                     207
#define SRST_RESETN_NPUTIMER0                    208
#define SRST_RESETN_NPUTIMER1                    209
#define SRST_PRESETN_NPU_TIMER                   210

/* CRU-->SOFTRST_CON32 */
#define SRST_PRESETN_NPU_WDT                     211
#define SRST_TRESETN_NPU_WDT                     212
#define SRST_PRESETN_RKNN_GRF                    213
#define SRST_RESETN_TEST_JUDGE_24M_NPU           214
#define SRST_RESETN_TEST_JUDGE_NPU               215

/* CRU-->SOFTRST_CON33 */
#define SRST_PRESETN_PHP0_GRF                    216
#define SRST_PRESETN_PHP0_BIU                    217
#define SRST_ARESETN_PHP0_BIU                    218
#define SRST_ARESETN_PHP0_FW                     219
#define SRST_PRESETN_PCIE1                       220
#define SRST_RESETN_PCIE1_POWER_UP               221
#define SRST_RESETN_TEST_JUDGE_24M_PHP0          222
#define SRST_RESETN_TEST_JUDGE_PHP0              223

/* CRU-->SOFTRST_CON34 */
#define SRST_PRESETN_PCIE0                       224
#define SRST_RESETN_PCIE0_POWER_UP               225
#define SRST_ARESETN_USB3OTG1                    226
#define SRST_ARESETN_GMAC0                       227
#define SRST_ARESETN_USB3OTG0                    228

/* CRU-->SOFTRST_CON35 */
#define SRST_ARESETN_UFS_BIU                     229
#define SRST_ARESETN_UFS_SYS                     230
#define SRST_ARESETN_UFS                         231
#define SRST_PRESETN_UFS_GRF                     232

/* CRU-->SOFTRST_CON37 */
#define SRST_RESETN_RXOOB0                       233
#define SRST_RESETN_PMALIVE0                     234
#define SRST_ARESETN_SATA0                       235
#define SRST_RESETN_RXOOB1                       236
#define SRST_RESETN_PMALIVE1                     237
#define SRST_ARESETN_SATA1                       238
#define SRST_RESETN_XPCS_PWR_ON                  239
#define SRST_RESETN_ASIC0                        240
#define SRST_RESETN_ASIC1                        241

/* CRU-->SOFTRST_CON42 */
#define SRST_PRESETN_NVM0_GRF                    242
#define SRST_PRESETN_NVM0_BIU                    243
#define SRST_ARESETN_NVM0_BIU                    244
#define SRST_HRESETN_NVM0_BIU                    245
#define SRST_CRESETN_EMMC                        246
#define SRST_HRESETN_EMMC                        247
#define SRST_ARESETN_EMMC                        248
#define SRST_BRESETN_EMMC                        249
#define SRST_TRESETN_EMMC                        250
#define SRST_ARESETN_GMAC1                       251

/* CRU-->SOFTRST_CON43 */
#define SRST_SRESETN_FSPI0                       252
#define SRST_HRESETN_FSPI0                       253
#define SRST_HRESETN_SDMMC1                      254
#define SRST_RESETN_TEST_JUDGE_24M_NVM0          255
#define SRST_RESETN_TEST_JUDGE_NVM0              256

/* CRU-->SOFTRST_CON44 */
#define SRST_ARESETN_DSMC_BIU                    257
#define SRST_ARESETN_DSMC                        258
#define SRST_HRESETN_DSMC_BIU                    259
#define SRST_HRESETN_DSMC                        260
#define SRST_PRESETN_DSMC_BIU                    261
#define SRST_PRESETN_DSMC                        262
#define SRST_RESETN_TEST_JUDGE_24M_DSMC          263
#define SRST_RESETN_TEST_JUDGE_DSMC              264
#define SRST_PRESETN_DSMC_GRF                    265

/* CRU-->SOFTRST_CON45 */
#define SRST_PRESETN_NVM1_GRF                    266
#define SRST_PRESETN_NVM1_BIU                    267
#define SRST_HRESETN_NVM1_BIU                    268
#define SRST_HRESETN_SDMMC0                      269
#define SRST_SRESETN_FSPI1                       270
#define SRST_HRESETN_FSPI1                       271

/* CRU-->SOFTRST_CON46 */
#define SRST_RESETN_TEST_JUDGE_24M_NVM1          272
#define SRST_RESETN_TEST_JUDGE_NVM1              273

/* CRU-->SOFTRST_CON50 */
#define SRST_HRESETN_RKVDEC                      274
#define SRST_HRESETN_RKVDEC_BIU                  275
#define SRST_ARESETN_RKVDEC_BIU                  276
#define SRST_RESETN_RKVDEC_HEVC_CA               277
#define SRST_RESETN_RKVDEC_CORE                  278
#define SRST_RESETN_TEST_JUDGE_24M_VDEC          279
#define SRST_RESETN_TEST_JUDGE_VDEC              280
#define SRST_PRESETN_VDEC_BIU                    281
#define SRST_PRESETN_VDEC_GRF                    282

/* CRU-->SOFTRST_CON52 */
#define SRST_PRESETN_APB2ASB_MIPI_HDMI_SUBVO     283
#define SRST_SRESETN_APB2ASB_MIPI_HDMI_SUBVO     284
#define SRST_PRESETN_APB2ASB_VCCIO0_3_SUBVO      285
#define SRST_SRESETN_APB2ASB_VCCIO0_3_SUBVO      286
#define SRST_PRESETN_HDPTXPHY                    287
#define SRST_PRESETN_HDPTXPHY_GRF                288
#define SRST_PRESETN_MIPI_DPHY0                  289

/* CRU-->SOFTRST_CON53 */
#define SRST_RESETN_HDPTX_INIT                   290
#define SRST_RESETN_HDPTX_CMN                    291
#define SRST_RESETN_HDPTX_LANE                   292
#define SRST_PRESETN_IOC_SUBVO                   293
#define SRST_RESETN_GMAC1_SUBVO_ASYNC            294
#define SRST_RESETN_TEST_JUDGE_24M_SUBSYSVO      295
#define SRST_RESETN_TEST_JUDGE_SUBSYSVO          296

/* CRU-->SOFTRST_CON54 */
#define SRST_PRESETN_APB2ASB_VCCIO1_2_4_SUBVI    297
#define SRST_SRESETN_APB2ASB_VCCIO1_2_4_SUBVI    298
#define SRST_PRESETN_APB2ASB_CSIPHY_SUBVI        299
#define SRST_SRESETN_APB2ASB_CSIPHY_SUBVI        300
#define SRST_PRESETN_CSIPHY0                     301
#define SRST_PRESETN_CSIPHY0_GRF                 302
#define SRST_PRESETN_CSIPHY1                     303
#define SRST_PRESETN_CSIPHY1_GRF                 304
#define SRST_PRESETN_IOC_SUBVI                   305
#define SRST_RESETN_GMAC0_SUBVI_ASYNC            306
#define SRST_SCAN_RESETN_CSIPHY0                 307
#define SRST_SCAN_RESETN_CSIPHY1                 308
#define SRST_RESETN_TEST_JUDGE_24M_SUBSYSVI      309
#define SRST_RESETN_TEST_JUDGE_SUBSYSVI          310

/* CRU-->SOFTRST_CON56 */
#define SRST_PRESETN_APB2ASB_VCCIO5_6_SUBDSMC    311
#define SRST_SRESETN_APB2ASB_VCCIO5_6_SUBDSMC    312
#define SRST_PRESETN_VCCIO5_6                    313
#define SRST_RESETN_GMAC1_ASYNC                  314

/* CRU-->SOFTRST_CON57 */
#define SRST_PRESETN_IMG_BIU                     315
#define SRST_PRESETN_IMG_GRF                     316
#define SRST_HRESETN_IMG_BIU                     317
#define SRST_ARESETN_JPEG_BIU                    318
#define SRST_RESETN_TEST_JUDGE_24M_IMG           319
#define SRST_RESETN_TEST_JUDGE_IMG               320
#define SRST_ARESETN_RGA_BIU                     321
#define SRST_ARESETN_VDPP_BIU                    322
#define SRST_ARESETN_EBC_BIU                     323

/* CRU-->SOFTRST_CON58 */
#define SRST_HRESETN_RGA2E_0                     324
#define SRST_ARESETN_RGA2E_0                     325
#define SRST_RESETN_CORE_RGA2E_0                 326
#define SRST_ARESETN_JPEG                        327
#define SRST_HRESETN_JPEG                        328
#define SRST_HRESETN_VDPP                        329
#define SRST_ARESETN_VDPP                        330
#define SRST_RESETN_CORE_VDPP                    331
#define SRST_HRESETN_EBC                         332
#define SRST_ARESETN_EBC                         333
#define SRST_DRESETN_EBC                         334

/* CRU-->SOFTRST_CON60 */
#define SRST_HRESETN_RKVENC_BIU                  335
#define SRST_ARESETN_RKVENC_BIU                  336
#define SRST_HRESETN_RKVENC                      337
#define SRST_ARESETN_RKVENC                      338
#define SRST_RESETN_RKVENC_CORE                  339
#define SRST_RESETN_TEST_JUDGE_24M_VENC          340
#define SRST_RESETN_TEST_JUDGE_VENC              341

/* CRU-->SOFTRST_CON63 */
#define SRST_ARESETN_VI_BIU                      342
#define SRST_HRESETN_VI_BIU                      343
#define SRST_PRESETN_VI_BIU                      344
#define SRST_DRESETN_VICAP                       345
#define SRST_ARESETN_VICAP                       346
#define SRST_HRESETN_VICAP                       347
#define SRST_RESETN_ISP                          348
#define SRST_RESETN_ISP_VICAP                    349

/* CRU-->SOFTRST_CON64 */
#define SRST_RESETN_CORE_VPSS                    350
#define SRST_PRESETN_VI_GRF                      351
#define SRST_PRESETN_CSI_HOST_0                  352
#define SRST_PRESETN_CSI_HOST_1                  353
#define SRST_PRESETN_CSI_HOST_2                  354
#define SRST_PRESETN_CSI_HOST_3                  355
#define SRST_RESETN_TEST_JUDGE_24M_VI            356
#define SRST_RESETN_TEST_JUDGE_VI                357

/* CRU-->SOFTRST_CON72 */
#define SRST_RESETN_CIFIN                        358

/* CRU-->SOFTRST_CON74 */
#define SRST_ARESETN_VOP_BIU                     359
#define SRST_ARESETN_VOP_FW                      360
#define SRST_HRESETN_VOP_BIU                     361
#define SRST_PRESETN_VOP_BIU                     362
#define SRST_HRESETN_VOP                         363
#define SRST_ARESETN_VOP                         364
#define SRST_DRESETN_VP0                         365

/* CRU-->SOFTRST_CON75 */
#define SRST_DRESETN_VP1                         366
#define SRST_PRESETN_VOPGRF                      367
#define SRST_RESETN_TEST_JUDGE_24M_VOP           368
#define SRST_RESETN_TEST_JUDGE_VOP               369

/* CRU-->SOFTRST_CON78 */
#define SRST_PRESETN_DSIHOST0                    370
#define SRST_HRESETN_VO_BIU                      371
#define SRST_PRESETN_VO_BIU                      372
#define SRST_ARESETN_HDCP0_BIU                   373
#define SRST_PRESETN_VO_GRF                      374

/* CRU-->SOFTRST_CON79 */
#define SRST_PRESETN_HDCP0_TRNG_NS               375
#define SRST_ARESETN_HDCP0                       376
#define SRST_HRESETN_HDCP0                       377
#define SRST_RESETN_HDCP0                        378
#define SRST_PRESETN_HDMITX0                     379
#define SRST_RESETN_HDMITX0_EARC                 380

/* CRU-->SOFTRST_CON80 */
#define SRST_RESETN_HDMITX0_REF                  381
#define SRST_PRESETN_EDP0                        382
#define SRST_RESETN_EDP0_24M                     383
#define SRST_MRESETN_SAI4                        384
#define SRST_HRESETN_SAI4                        385
#define SRST_MRESETN_SAI5                        386
#define SRST_HRESETN_SAI5                        387
#define SRST_HRESETN_SPDIF_TX2                   388
#define SRST_MRESETN_SPDIF_TX2                   389
#define SRST_HRESETN_SPDIF_RX1                   390
#define SRST_MRESETN_SPDIF_RX1                   391

/* CRU-->SOFTRST_CON81 */
#define SRST_RESETN_EBC_VOP_DCLK_VO              392

/* CRU-->SOFTRST_CON82 */
#define SRST_RESETN_TEST_JUDGE_24M_VO            393
#define SRST_RESETN_TEST_JUDGE_VO                394

/* CRU-->SOFTRST_CON84 */
#define SRST_RESETN_GPU                          395
#define SRST_SYSRESETN_GPU                       396
#define SRST_PRESETN_GPU_BIU                     397
#define SRST_PORESETN_GPU_JTAG                   398
#define SRST_PRESETN_GPU_GRF                     399
#define SRST_RESETN_GPU_PVTPLL                   400
#define SRST_PRESETN_PVTPLL_GPU                  401
#define SRST_ARESETN_SLV_GPU_BIU                 402

/* CRU-->SOFTRST_CON85 */
#define SRST_ARESETN_GPU_BIU                     403
#define SRST_RESETN_TEST_JUDGE_24M_GPU           404
#define SRST_RESETN_TEST_JUDGE_GPU               405

/* CRU-->SOFTRST_CON86 */
#define SRST_ARESETN_CENTER_BIU                  406
#define SRST_ARESETN_CENTER_FW_DDR               407
#define SRST_ARESETN_CENTER_FW_GPU               408
#define SRST_ARESETN_DMA2DDR                     409
#define SRST_ARESETN_DDR_SHAREMEM                410
#define SRST_ARESETN_DDR_SHAREMEM_BIU            411
#define SRST_ARESETN_DDR_SHAREMEM_FW             412
#define SRST_HRESETN_CENTER_BIU                  413
#define SRST_HRESETN_CENTER_FW                   414
#define SRST_PRESETN_CENTER_GRF                  415
#define SRST_PRESETN_DMA2DDR                     416

/* CRU-->SOFTRST_CON87 */
#define SRST_PRESETN_CENTER_BIU                  417
#define SRST_RESETN_TEST_JUDGE_24M_CENTER        418
#define SRST_RESETN_TEST_JUDGE_CENTER            419
#define SRST_RESETN_LINKSYM_HDMITXPHY0           420

/* PHPPHY_CRU-->SOFTRST_CON00 */
#define SRST_PRESETN_PHPPHY_CRU                  422
#define SRST_PRESETN_PCIE2_COMBOPHY0             423
#define SRST_PRESETN_PCIE2_COMBOPHY0_GRF         424
#define SRST_PRESETN_PCIE2_COMBOPHY1             425
#define SRST_PRESETN_PCIE2_COMBOPHY1_GRF         426
#define SRST_PRESETN_USBPHY_GRF_0                427
#define SRST_PRESETN_USBPHY_GRF_1                428
#define SRST_RESETN_MPHY_INIT                    429
#define SRST_PRESETN_MPHY_GRF                    430
#define SRST_PRESETN_VCCIO7_IOC                  431
#define SRST_RESETN_USB2DEBUG                    432
#define SRST_PRESETN_PCIEPHY2                    433
#define SRST_PRESETN_PCIEPHY2_GRF                434

/* PHPPHY_CRU-->SOFTRST_CON01 */
#define SRST_SRESETN_APB2ASB_USBMPHY_PHPPHY      435
#define SRST_PRESETN_APB2ASB_USBMPHY_PHPPHY      436
#define SRST_RESETN_OTGPHY_0                     437
#define SRST_RESETN_OTGPHY_1                     438
#define SRST_RESETN_TEST_JUDGE_24M_PHPPHY        439
#define SRST_RESETN_TEST_JUDGE_PHPPHY            440

/* PHPPHY_CRU-->SOFTRST_CON02 */
#define SRST_RESETN_PCIEPHY2                     441
#define SRST_RESETN_PCIE0_PIPE_PHY               442
#define SRST_RESETN_PCIE1_PIPE_PHY               443

/* SECURE_CRU-->SOFTRST_CON00 */
#define SRST_HRESETN_CRYPTO_NS                   444
#define SRST_HRESETN_TRNG_NS                     445
#define SRST_PRESETN_SECURE_NS_BIU               446
#define SRST_ARESETN_SECURE_HIGH_BIU             447
#define SRST_HRESETN_SECURE_NS_BIU               448
#define SRST_PRESETN_OTPC_NS                     449
#define SRST_RESETN_OTPC_NS                      450

/* PMU1_CRU-->SOFTRST_CON03 */
#define SRST_PRESETN_APB2ASB_PMU1                451
#define SRST_SRESETN_APB2ASB_PMU1                452
#define SRST_HRESETN_PMU1_BIU                    453
#define SRST_HRESETN_PMU1_FW                     454
#define SRST_PRESETN_PMU1_BIU                    455
#define SRST_HRESETN_PMU_CM0_BIU                 456
#define SRST_CORERESETN_N100                     457
#define SRST_PORESETN_N100                       458

/* PMU1_CRU-->SOFTRST_CON04 */
#define SRST_PRESETN_CRU_PMU1                    459
#define SRST_PRESETN_PMU1_GRF                    460
#define SRST_PRESETN_PMU1_IOC                    461
#define SRST_PRESETN_PMU1WDT                     462
#define SRST_TRESETN_PMU1WDT                     463
#define SRST_PRESETN_PMUTIMER                    464
#define SRST_RESETN_PMUTIMER0                    465
#define SRST_RESETN_PMUTIMER1                    466
#define SRST_PRESETN_PWM0                        467
#define SRST_RESETN_PWM0                         468

/* PMU1_CRU-->SOFTRST_CON05 */
#define SRST_PRESETN_I2C0                        469
#define SRST_RESETN_I2C0                         470
#define SRST_SRESETN_UART1                       471
#define SRST_PRESETN_UART1                       472
#define SRST_RESETN_TEST_JUDGE_24M_PMU1          473
#define SRST_RESETN_TEST_JUDGE_PMU1              474
#define SRST_RESETN_PDM0                         475

/* PMU1_CRU-->SOFTRST_CON06 */
#define SRST_HRESETN_PDM0                        476
#define SRST_MRESETN_PDM0                        477
#define SRST_HRESETN_VAD                         478
#define SRST_PRESETN_RCOSC                       479
#define SRST_RESETN_RCOSC                        480

/* PMU1_CRU-->SOFTRST_CON07 */
#define SRST_PRESETN_PMU0GRF                     481
#define SRST_PRESETN_PMU0IOC                     482
#define SRST_PRESETN_GPIO0                       483
#define SRST_DBRESETN_GPIO0                      484

/* DDR0_CRU-->SOFTRST_CON00 */
#define SRST_RESETN_DDRPHY1X                     485

/* DDR0_CRU-->SOFTRST_CON01 */
#define SRST_PRESETN_DDRPHY_CRU                  486
#define SRST_PRESETN_DDRPHY                      487

/* BIGCORE_CRU-->SOFTRST_CON00 */
#define SRST_RESETN_REF_PVTPLL_BIGCORE           488
#define SRST_PRESETN_BIGCORE_BIU                 489
#define SRST_PRESETN_BIGCORE_GRF                 490
#define SRST_PRESETN_BIGCORE_CRU                 491
#define SRST_PRESETN_PVTPLL_BIGCORE              492
#define SRST_NBIGCOREPORESET0                    493
#define SRST_NBIGCOREPORESET1                    494
#define SRST_NBIGCORESET0                        495
#define SRST_NBIGCORESET1                        496

/* BIGCORE_CRU-->SOFTRST_CON01 */
#define SRST_NL2RESET_BIGCORE                    497
#define SRST_ARESETN_ADB400_A73_ACE              498
#define SRST_RESETN_TEST_JUDGE_24M_BIGCORE       499
#define SRST_RESETN_TEST_JUDGE_BIGCORE           500
#define SRST_PRESETN_DBG_BIGCORE                 501

/* LITCORE0_CRU-->SOFTRST_CON00 */
#define SRST_RESETN_REF_LITCORE0_PVTPLL          502
#define SRST_NLITCOREPORESET0                    503
#define SRST_NLITCOREPORESET1                    504
#define SRST_NLITCOREPORESET2                    505
#define SRST_NLITCOREPORESET3                    506
#define SRST_NLITCORESET0                        507
#define SRST_NLITCORESET1                        508
#define SRST_NLITCORESET2                        509
#define SRST_NLITCORESET3                        510
#define SRST_NL2RESET_LITCORE0                   511
#define SRST_PRESETN_DBG_LITCORE0                512
#define SRST_ARESETN_ADB400_A53_ACE_LITCORE0     513

/* LITCORE0_CRU-->SOFTRST_CON01 */
#define SRST_PRESETN_LITCORE0_GRF                514
#define SRST_PRESETN_LITCORE0_BIU                515
#define SRST_PRESETN_PVTPLL_LITCORE0             516
#define SRST_PRESETN_LITCORE0_CRU                517
#define SRST_RESETN_TEST_JUDGE_24M_LITCORE0      518
#define SRST_RESETN_TEST_JUDGE_LITCORE0          519

/* LITCORE1_CRU-->SOFTRST_CON00 */
#define SRST_RESETN_REF_LITCORE1_PVTPLL          520
#define SRST_NLITCOREPORESET4                    521
#define SRST_NLITCOREPORESET5                    522
#define SRST_NLITCORESET4                        523
#define SRST_NLITCORESET5                        524
#define SRST_NL2RESET_LITCORE1                   525
#define SRST_PRESETN_DBG_LITCORE1                526
#define SRST_ARESETN_ADB400_A53_ACE_LITCORE1     527
#define SRST_PRESETN_LITCORE1_GRF                528
#define SRST_PRESETN_LITCORE1_BIU                529

/* LITCORE1_CRU-->SOFTRST_CON01 */
#define SRST_PRESETN_PVTPLL_LITCORE1             530
#define SRST_PRESETN_LITCORE1_CRU                531
#define SRST_RESETN_TEST_JUDGE_24M_LITCORE1      532
#define SRST_RESETN_TEST_JUDGE_LITCORE1          533

/* CCI_CRU-->SOFTRST_CON01 */
#define SRST_RESETN_TEST_JUDGE_24M_CCI           534
#define SRST_RESETN_TEST_JUDGE_CCI               535
#define SRST_ARESETN_CPE                         536
#define SRST_PRESETN_CCI_BIU                     537
#define SRST_PRESETN_CCI_FW                      538
#define SRST_ARESETN_CCI_BIU                     539
#define SRST_ARESETN_CCI_FW                      540

/* CCI_CRU-->SOFTRST_CON02 */
#define SRST_PRESETN_DBG_SYS                     541
#define SRST_PRESETN_CCI_GRF                     542
#define SRST_PRESETN_CCI_CRU                     543
#define SRST_ARESETN_CCI                         544
#define SRST_PORESETN_JTAG                       545
#define SRST_PRESETN_DBG_M                       546
#define SRST_ARESETN_ADB400_CCI_A73              547
#define SRST_ARESETN_ADB400_CCI_A53_0            548
#define SRST_ARESETN_ADB400_CCI_A53_1            549
#define SRST_PRESETN_PVTPLL_CCI                  550
#define SRST_PRESETN_CCI_SPINLOCK                551
#define SRST_PRESETN_DBG_M_BIU                   552
#define SRST_RESETN_REF_CCI_PVTPLL               553

#endif // _DT_BINDINGS_RESET_ROCKCHIP_RK3572_H
