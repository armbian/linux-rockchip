/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 * Author: Finley Xiao <finley.xiao@rock-chips.com>
 */

#ifndef _DT_BINDINGS_CLK_ROCKCHIP_RK3506_H
#define _DT_BINDINGS_CLK_ROCKCHIP_RK3506_H

/* cru plls */
#define PLL_GPLL			1
#define PLL_V0PLL			2
#define PLL_V1PLL			3

/* cru-clocks indices */
#define ARMCLK				15
#define CLK_DDR				16
#define XIN24M_GATE			17
#define CLK_GPLL_GATE			18
#define CLK_V0PLL_GATE			19
#define CLK_V1PLL_GATE			20
#define CLK_GPLL_DIV			21
#define CLK_GPLL_DIV_100M		22
#define CLK_V0PLL_DIV			23
#define CLK_V1PLL_DIV			24
#define CLK_INT_VOICE_MATRIX0		25
#define CLK_INT_VOICE_MATRIX1		26
#define CLK_INT_VOICE_MATRIX2		27
#define CLK_FRAC_UART_MATRIX0_MUX	28
#define CLK_FRAC_UART_MATRIX1_MUX	29
#define CLK_FRAC_VOICE_MATRIX0_MUX	30
#define CLK_FRAC_VOICE_MATRIX1_MUX	31
#define CLK_FRAC_COMMON_MATRIX0_MUX	32
#define CLK_FRAC_COMMON_MATRIX1_MUX	33
#define CLK_FRAC_COMMON_MATRIX2_MUX	34
#define CLK_FRAC_UART_MATRIX0		35
#define CLK_FRAC_UART_MATRIX1		36
#define CLK_FRAC_VOICE_MATRIX0		37
#define CLK_FRAC_VOICE_MATRIX1		38
#define CLK_FRAC_COMMON_MATRIX0		39
#define CLK_FRAC_COMMON_MATRIX1		40
#define CLK_FRAC_COMMON_MATRIX2		41
#define CLK_REF_USBPHY_TOP		42
#define CLK_REF_DPHY_TOP		43
#define ACLK_CORE_ROOT			44
#define PCLK_CORE_ROOT			45
#define PCLK_DBG			48
#define PCLK_CORE_GRF			49
#define PCLK_CORE_CRU			50
#define CLK_CORE_EMA_DETECT		51
#define CLK_REF_PVTPLL_CORE		52
#define PCLK_GPIO1			53
#define DBCLK_GPIO1			54
#define ACLK_CORE_PERI_ROOT		55
#define HCLK_CORE_PERI_ROOT		56
#define PCLK_CORE_PERI_ROOT		57
#define CLK_DSMC			58
#define ACLK_DSMC			59
#define PCLK_DSMC			60
#define CLK_FLEXBUS_TX			61
#define CLK_FLEXBUS_RX			62
#define ACLK_FLEXBUS			63
#define HCLK_FLEXBUS			64
#define ACLK_DSMC_SLV			65
#define HCLK_DSMC_SLV			66
#define ACLK_BUS_ROOT			67
#define HCLK_BUS_ROOT			68
#define PCLK_BUS_ROOT			69
#define ACLK_SYSRAM			70
#define HCLK_SYSRAM			71
#define ACLK_DMAC0			72
#define ACLK_DMAC1			73
#define HCLK_M0				74
#define PCLK_BUS_GRF			75
#define PCLK_TIMER			76
#define CLK_TIMER0_CH0			77
#define CLK_TIMER0_CH1			78
#define CLK_TIMER0_CH2			79
#define CLK_TIMER0_CH3			80
#define CLK_TIMER0_CH4			81
#define CLK_TIMER0_CH5			82
#define PCLK_WDT0			83
#define TCLK_WDT0			84
#define PCLK_WDT1			85
#define TCLK_WDT1			86
#define PCLK_MAILBOX			87
#define PCLK_INTMUX			88
#define PCLK_SPINLOCK			89
#define PCLK_DDRC			90
#define HCLK_DDRPHY			91
#define PCLK_DDRMON			92
#define CLK_DDRMON_OSC			93
#define PCLK_STDBY			94
#define HCLK_USBOTG0			95
#define HCLK_USBOTG0_PMU		96
#define CLK_USBOTG0_ADP			97
#define HCLK_USBOTG1			98
#define HCLK_USBOTG1_PMU		99
#define CLK_USBOTG1_ADP			100
#define PCLK_USBPHY			101
#define ACLK_DMA2DDR			102
#define PCLK_DMA2DDR			103
#define STCLK_M0			104
#define CLK_DDRPHY			105
#define CLK_DDRC_SRC			106
#define ACLK_DDRC_0			107
#define ACLK_DDRC_1			108
#define CLK_DDRC			109
#define CLK_DDRMON			110
#define HCLK_LSPERI_ROOT		111
#define PCLK_LSPERI_ROOT		112
#define PCLK_UART0			113
#define PCLK_UART1			114
#define PCLK_UART2			115
#define PCLK_UART3			116
#define PCLK_UART4			117
#define SCLK_UART0			118
#define SCLK_UART1			119
#define SCLK_UART2			120
#define SCLK_UART3			121
#define SCLK_UART4			122
#define PCLK_I2C0			123
#define CLK_I2C0			124
#define PCLK_I2C1			125
#define CLK_I2C1			126
#define PCLK_I2C2			127
#define CLK_I2C2			128
#define PCLK_PWM1			129
#define CLK_PWM1			130
#define CLK_OSC_PWM1			131
#define CLK_RC_PWM1			132
#define CLK_FREQ_PWM1			133
#define CLK_COUNTER_PWM1		134
#define PCLK_SPI0			135
#define CLK_SPI0			136
#define PCLK_SPI1			137
#define CLK_SPI1			138
#define PCLK_GPIO2			139
#define DBCLK_GPIO2			140
#define PCLK_GPIO3			141
#define DBCLK_GPIO3			142
#define PCLK_GPIO4			143
#define DBCLK_GPIO4			144
#define HCLK_CAN0			145
#define CLK_CAN0			146
#define HCLK_CAN1			147
#define CLK_CAN1			148
#define HCLK_PDM			149
#define MCLK_PDM			150
#define CLKOUT_PDM			151
#define MCLK_SPDIFTX			152
#define HCLK_SPDIFTX			153
#define HCLK_SPDIFRX			154
#define MCLK_SPDIFRX			155
#define MCLK_SAI0			156
#define HCLK_SAI0			157
#define MCLK_OUT_SAI0			158
#define MCLK_SAI1			159
#define HCLK_SAI1			160
#define MCLK_OUT_SAI1			161
#define HCLK_ASRC0			162
#define CLK_ASRC0			163
#define HCLK_ASRC1			164
#define CLK_ASRC1			165
#define PCLK_CRU			166
#define PCLK_PMU_ROOT			167
#define MCLK_ASRC0			168
#define MCLK_ASRC1			169
#define MCLK_ASRC2			170
#define MCLK_ASRC3			171
#define LRCK_ASRC0_SRC			172
#define LRCK_ASRC0_DST			173
#define LRCK_ASRC1_SRC			174
#define LRCK_ASRC1_DST			175
#define ACLK_HSPERI_ROOT		176
#define HCLK_HSPERI_ROOT		177
#define PCLK_HSPERI_ROOT		178
#define CCLK_SRC_SDMMC			179
#define HCLK_SDMMC			180
#define HCLK_FSPI			181
#define SCLK_FSPI			182
#define PCLK_SPI2			183
#define ACLK_MAC0			184
#define ACLK_MAC1			185
#define PCLK_MAC0			186
#define PCLK_MAC1			187
#define CLK_MAC_ROOT			188
#define CLK_MAC0			189
#define CLK_MAC1			190
#define MCLK_SAI2			191
#define HCLK_SAI2			192
#define MCLK_OUT_SAI2			193
#define MCLK_SAI3_SRC			194
#define HCLK_SAI3			195
#define MCLK_SAI3			196
#define MCLK_OUT_SAI3			197
#define MCLK_SAI4_SRC			198
#define HCLK_SAI4			199
#define MCLK_SAI4			200
#define HCLK_DSM			201
#define MCLK_DSM			202
#define PCLK_AUDIO_ADC			203
#define MCLK_AUDIO_ADC			204
#define MCLK_AUDIO_ADC_DIV4		205
#define PCLK_SARADC			206
#define CLK_SARADC			207
#define PCLK_OTPC_NS			208
#define CLK_SBPI_OTPC_NS		209
#define CLK_USER_OTPC_NS		210
#define PCLK_UART5			211
#define SCLK_UART5			212
#define PCLK_GPIO234_IOC		213
#define CLK_MAC_PTP_ROOT		214
#define CLK_MAC0_PTP			215
#define CLK_MAC1_PTP			216
#define CLK_SPI2			217
#define ACLK_VIO_ROOT			218
#define HCLK_VIO_ROOT			219
#define PCLK_VIO_ROOT			220
#define HCLK_RGA			221
#define ACLK_RGA			222
#define CLK_CORE_RGA			223
#define ACLK_VOP			224
#define HCLK_VOP			225
#define DCLK_VOP			226
#define PCLK_DPHY			227
#define PCLK_DSI_HOST			228
#define PCLK_TSADC			229
#define CLK_TSADC			230
#define CLK_TSADC_TSEN			231
#define PCLK_GPIO1_IOC			232
#define PCLK_OTPC_S			233
#define CLK_SBPI_OTPC_S			234
#define CLK_USER_OTPC_S			235
#define PCLK_OTP_MASK			236
#define PCLK_KEYREADER			237
#define HCLK_BOOTROM			238
#define PCLK_DDR_SERVICE		239
#define HCLK_CRYPTO_S			240
#define HCLK_KEYLAD			241
#define CLK_CORE_CRYPTO			242
#define CLK_PKA_CRYPTO			243
#define CLK_CORE_CRYPTO_S		244
#define CLK_PKA_CRYPTO_S		245
#define ACLK_CRYPTO_S			246
#define HCLK_RNG_S			247
#define CLK_CORE_CRYPTO_NS		248
#define CLK_PKA_CRYPTO_NS		249
#define ACLK_CRYPTO_NS			250
#define HCLK_CRYPTO_NS			251
#define HCLK_RNG			252
#define CLK_PMU				253
#define PCLK_PMU			254
#define CLK_PMU_32K			255
#define PCLK_PMU_CRU			256
#define PCLK_PMU_GRF			257
#define PCLK_GPIO0_IOC			258
#define PCLK_GPIO0			259
#define DBCLK_GPIO0			260
#define PCLK_GPIO1_SHADOW		261
#define DBCLK_GPIO1_SHADOW		262
#define PCLK_PMU_HP_TIMER		263
#define CLK_PMU_HP_TIMER		264
#define CLK_PMU_HP_TIMER_32K		265
#define PCLK_PWM0			266
#define CLK_PWM0			267
#define CLK_OSC_PWM0			268
#define CLK_RC_PWM0			269
#define CLK_MAC_OUT			270
#define CLK_REF_OUT0			271
#define CLK_REF_OUT1			272
#define CLK_32K_FRAC			273
#define CLK_32K_RC			274
#define CLK_32K				275
#define CLK_32K_PMU			276
#define PCLK_TOUCH_KEY			277
#define CLK_TOUCH_KEY			278
#define CLK_REF_PHY_PLL			279
#define CLK_REF_PHY_PMU_MUX		280
#define CLK_WIFI_OUT			281
#define CLK_V0PLL_REF			282
#define CLK_V1PLL_REF			283
#define CLK_32K_FRAC_MUX		284

#define CLK_NR_CLKS			(CLK_32K_FRAC_MUX + 1)

/* soft-reset indices */

/********Name=SOFTRST_CON00,Offset=0xA00********/
#define SRST_NCOREPORESET0_AC		0
#define SRST_NCOREPORESET1_AC		1
#define SRST_NCOREPORESET2_AC		2
#define SRST_NCORESET0_AC		4
#define SRST_NCORESET1_AC		5
#define SRST_NCORESET2_AC		6
#define SRST_NL2RESET_AC		8
#define SRST_ARESETN_CORE_BIU_AC	9
#define SRST_HRESETN_M0_AC		10

/********Name=SOFTRST_CON02,Offset=0xA08********/
#define SRST_N_DBG			42
#define SRST_P_CORE_BIU			46
#define SRST_PMU			47

/********Name=SOFTRST_CON03,Offset=0xA0C********/
#define SRST_P_DBG			49
#define SRST_POT_DBG			50
#define SRST_P_CORE_GRF			52
#define SRST_CORE_EMA_DETECT		54
#define SRST_REF_PVTPLL_CORE		55
#define SRST_P_GPIO1			56
#define SRST_DB_GPIO1			57

/********Name=SOFTRST_CON04,Offset=0xA10********/
#define SRST_A_CORE_PERI_BIU		67
#define SRST_A_DSMC			69
#define SRST_P_DSMC			70
#define SRST_FLEXBUS			71
#define SRST_A_FLEXBUS			73
#define SRST_H_FLEXBUS			74
#define SRST_A_DSMC_SLV			75
#define SRST_H_DSMC_SLV			76
#define SRST_DSMC_SLV			77

/********Name=SOFTRST_CON05,Offset=0xA14********/
#define SRST_A_BUS_BIU			83
#define SRST_H_BUS_BIU			84
#define SRST_P_BUS_BIU			85
#define SRST_A_SYSTEM			86
#define SRST_H_SySTEM			87
#define SRST_A_DMAC0			88
#define SRST_A_DMAC1			89
#define SRST_H_M0			90
#define SRST_M0_JTAG			91
#define SRST_H_CRYPTO			95

/********Name=SOFTRST_CON06,Offset=0xA18********/
#define SRST_H_RNG			96
#define SRST_P_BUS_GRF			97
#define SRST_P_TIMER0			98
#define SRST_TIMER0_CH0			99
#define SRST_TIMER0_CH1			100
#define SRST_TIMER0_CH2			101
#define SRST_TIMER0_CH3			102
#define SRST_TIMER0_CH4			103
#define SRST_TIMER0_CH5			104
#define SRST_P_WDT0			105
#define SRST_T_WDT0			106
#define SRST_P_WDT1			107
#define SRST_T_WDT1			108
#define SRST_P_MAILBOX			109
#define SRST_P_INTMUX			110
#define SRST_P_SPINLOCK			111

/********Name=SOFTRST_CON07,Offset=0xA1C********/
#define SRST_P_DDRC			112
#define SRST_H_DDRPHY			113
#define SRST_P_DDRMON			114
#define SRST_DDRMON_OSC			115
#define SRST_P_DDR_LPC			116
#define SRST_H_USBOTG0			117
#define SRST_USBOTG0_ADP		119
#define SRST_H_USBOTG1			120
#define SRST_USBOTG1_ADP		122
#define SRST_P_USBPHY			123
#define SRST_USBPHY_POR			124
#define SRST_USBPHY_OTG0		125
#define SRST_USBPHY_OTG1		126

/********Name=SOFTRST_CON08,Offset=0xA20********/
#define SRST_A_DMA2DDR			128
#define SRST_P_DMA2DDR			129

/********Name=SOFTRST_CON09,Offset=0xA24********/
#define SRST_USBOTG0_UTMI		144
#define SRST_USBOTG1_UTMI		145

/********Name=SOFTRST_CON10,Offset=0xA28********/
#define SRST_A_DDRC_0			160
#define SRST_A_DDRC_1			161
#define SRST_A_DDR_BIU			162
#define SRST_DDRC			163
#define SRST_DDRMON			164

/********Name=SOFTRST_CON11,Offset=0xA2C********/
#define SRST_H_LSPERI_BIU		178
#define SRST_P_UART0			180
#define SRST_P_UART1			181
#define SRST_P_UART2			182
#define SRST_P_UART3			183
#define SRST_P_UART4			184
#define SRST_UART0			185
#define SRST_UART1			186
#define SRST_UART2			187
#define SRST_UART3			188
#define SRST_UART4			189
#define SRST_P_I2C0			190
#define SRST_I2C0			191

/********Name=SOFTRST_CON12,Offset=0xA30********/
#define SRST_P_I2C1			192
#define SRST_I2C1			193
#define SRST_P_I2C2			194
#define SRST_I2C2			195
#define SRST_P_PWM1			196
#define SRST_PWM1			197
#define SRST_P_SPI0			202
#define SRST_SPI0			203
#define SRST_P_SPI1			204
#define SRST_SPI1			205
#define SRST_P_GPIO2			206
#define SRST_DB_GPIO2			207

/********Name=SOFTRST_CON13,Offset=0xA34********/
#define SRST_P_GPIO3			208
#define SRST_DB_GPIO3			209
#define SRST_P_GPIO4			210
#define SRST_DB_GPIO4			211
#define SRST_H_CAN0			212
#define SRST_CAN0			213
#define SRST_H_CAN1			214
#define SRST_CAN1			215
#define SRST_H_PDM			216
#define SRST_M_PDM			217
#define SRST_PDM			218
#define SRST_SPDIFTX			219
#define SRST_H_SPDIFTX			220
#define SRST_H_SPDIFRX			221
#define SRST_SPDIFRX			222
#define SRST_M_SAI0			223

/********Name=SOFTRST_CON14,Offset=0xA38********/
#define SRST_H_SAI0			224
#define SRST_M_SAI1			226
#define SRST_H_SAI1			227
#define SRST_H_ASRC0			229
#define SRST_ASRC0			230
#define SRST_H_ASRC1			231
#define SRST_ASRC1			232

/********Name=SOFTRST_CON17,Offset=0xA44********/
#define SRST_H_HSPERI_BIU		276
#define SRST_H_SDMMC			279
#define SRST_H_FSPI			280
#define SRST_S_FSPI			281
#define SRST_P_SPI2			282
#define SRST_A_MAC0			283
#define SRST_A_MAC1			284

/********Name=SOFTRST_CON18,Offset=0xA48********/
#define SRST_M_SAI2			290
#define SRST_H_SAI2			291
#define SRST_H_SAI3			294
#define SRST_M_SAI3			295
#define SRST_H_SAI4			298
#define SRST_M_SAI4			299
#define SRST_H_DSM			300
#define SRST_M_DSM			301
#define SRST_P_AUDIO_ADC		302
#define SRST_M_AUDIO_ADC		303

/********Name=SOFTRST_CON19,Offset=0xA4C********/
#define SRST_P_SARADC			304
#define SRST_SARADC			305
#define SRST_SARADC_PHY			306
#define SRST_P_OTPC_NS			307
#define SRST_SBPI_OTPC_NS		308
#define SRST_USER_OTPC_NS		309
#define SRST_P_UART5			310
#define SRST_UART5			311
#define SRST_P_GPIO234_IOC		312

/********Name=SOFTRST_CON21,Offset=0xA54********/
#define SRST_A_VIO_BIU			339
#define SRST_H_VIO_BIU			340
#define SRST_H_RGA			342
#define SRST_A_RGA			343
#define SRST_CORE_RGA			344
#define SRST_A_VOP			345
#define SRST_H_VOP			346
#define SRST_VOP			347
#define SRST_P_DPHY			348
#define SRST_P_DSI_HOST			349
#define SRST_P_TSADC			350
#define SRST_TSADC			351

/********Name=SOFTRST_CON22,Offset=0xA58********/
#define SRST_P_GPIO1_IOC		353

#endif
