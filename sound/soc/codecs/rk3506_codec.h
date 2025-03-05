/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * rk3506_codec.h - Rockchip RK3506 SoC Codec Driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef _RK3506_CODEC_H_
#define _RK3506_CODEC_H_

#define AUDIO_ADC_BG_LPF0			0x00
#define AUDIO_ADC_BG_LPF1			0x04
#define AUDIO_ADC_ADC0				0x08
#define AUDIO_ADC_ADC1				0x0c
#define AUDIO_ADC_ADC2				0x10
#define AUDIO_ADC_PGA0				0x14
#define AUDIO_ADC_PGA1				0x18
#define AUDIO_ADC_PGA2				0x1c
#define AUDIO_ADC_LDO				0x20
#define AUDIO_ADC_HK0				0x24
#define AUDIO_ADC_HK1				0x28
#define AUDIO_ADC_DIGEN_CLKE			0x2c
#define AUDIO_ADC_VOLL				0x30
#define AUDIO_ADC_AGC0				0x34
#define AUDIO_ADC_AGC1				0x38
#define AUDIO_ADC_AGC2				0x3c
#define AUDIO_ADC_AGC3				0x40
#define AUDIO_ADC_AGC4				0x44
#define AUDIO_ADC_AGC5				0x48
#define AUDIO_ADC_AGC6				0x4c
#define AUDIO_ADC_AGC7				0x50
#define AUDIO_ADC_AGC8				0x54
#define AUDIO_ADC_READ1				0x58
#define AUDIO_ADC_READ2				0x5c
#define AUDIO_ADC_FILTER			0x60
#define AUDIO_ADC_I2S_CKM			0x64
#define AUDIO_ADC_I2S_TSD			0x68
#define AUDIO_ADC_I2S_TXCR1			0x6c
#define AUDIO_ADC_I2S_TXCR2_TXCMD		0x70

#define ACODEC_REG_MAX				AUDIO_ADC_I2S_TXCR2_TXCMD
#define ACODEC_HIFI				0x0

/* AUDIO_ADC_BG_LPF0 */
#define LPF_PWD_MSK				(0x1 << 0)
#define LPF_PWD_EN				(0x0 << 0)
#define LPF_PWD_DIS				(0x1 << 0)
#define LPF_CHOP_MSK				(0x1 << 1)
#define LPF_CHOP_EN				(0x0 << 1)
#define LPF_CHOP_DIS				(0x1 << 1)
#define LPF_SW_TIME_MSK				(0x1 << 2)
#define LPF_SW_TIME_124US			(0x1 << 2)
#define LPF_SW_TIME_64US			(0x0 << 2)
#define LPF_CLK_MSK				(0x1 << 3)
#define LPF_CLK_EN				(0x1 << 3)
#define LPF_CLK_DIS				(0x0 << 3)
#define LPF_DELAY_TIME_MSK			(0x7 << 4)
#define LPF_DELAY_TIME_2MS			(0x0 << 4)
#define LPF_DELAY_TIME_4MS			(0x1 << 4)
#define LPF_DELAY_TIME_8MS			(0x2 << 4)
#define LPF_DELAY_TIME_16MS			(0x3 << 4)
#define LPF_DELAY_TIME_32MS			(0x4 << 4)
#define LPF_DELAY_TIME_64MS			(0x5 << 4)
#define LPF_DELAY_TIME_128MS			(0x6 << 4)
#define LPF_DELAY_TIME_256MS			(0x7 << 4)

/* AUDIO_ADC_BG_LPF1 */
#define LPF_FORCE_BG_CHARGE_MSK			(0x1 << 0)
#define LPF_FORCE_BG_CHARGE_EN			(0x1 << 0)
#define LPF_FORCE_BG_CHARGE_DIS			(0x1 << 0)
#define LPF_FORCE_RCFILT_MODE_MSK		(0x1 << 1)
#define LPF_FORCE_RCFILT_MODE_EN		(0x1 << 1)
#define LPF_FORCE_RCFILT_MODE_DIS		(0x0 << 1)

/* AUDIO_ADC_ADC0 */
#define ADC_PWD_MSK				(0x1 << 0)
#define ADC_PWD_EN				(0x0 << 0)
#define ADC_PWD_DIS				(0x1 << 0)
#define ADC_ZERO_MSK				(0x1 << 1)
#define ADC_ZERO_EN				(0x1 << 1)
#define ADC_ZERO_DIS				(0x0 << 1)
#define ADC_DEM_CTRL				(0x3 << 2)
#define ADC_DEM_DEFAULT				(0x0 << 2)
#define ADC_DEM_DWA				(0x1 << 2)
#define ADC_DEM_ONE				(0x2 << 2)
#define ADC_DEM_DUAL				(0x3 << 2)
#define ADC_DELAY_SARSEL_MSK			(0x3 << 4)
#define ADC_DELAY_SARSEL_100_PCT		(0x0 << 4)
#define ADC_DELAY_SARSEL_75_PCT			(0x1 << 4)
#define ADC_DELAY_SARSEL_50_PCT			(0x2 << 4)
#define ADC_DELAY_SARSEL_25_PCT			(0x3 << 4)
#define ADC_DELAY_CLKSEL_MSK			(0x3 << 6)
#define ADC_DELAY_CLKSEL_100_PCT		(0x0 << 6)
#define ADC_DELAY_CLKSEL_75_PCT			(0x1 << 6)
#define ADC_DELAY_CLKSEL_50_PCT			(0x2 << 6)
#define ADC_DELAY_CLKSEL_25_PCT			(0x3 << 6)

/* AUDIO_ADC_ADC1 */
#define ADC_IBOP1_CTRL_MSK			(0x3 << 0)
#define ADC_IBOP1_50_PCT			(0x0 << 0)
#define ADC_IBOP1_100_PCT			(0x1 << 0)
#define ADC_IBOP1_150_PCT			(0x2 << 0)
#define ADC_IBOP1_200_PCT			(0x3 << 0)
#define ADC_IBOP2_INC_MSK			(0x1 << 2)
#define ADC_IBOP2_INC_100_PCT			(0x0 << 2)
#define ADC_IBOP2_INC_200_PCT			(0x1 << 2)
#define ADC_IBOP3_INC_MSK			(0x1 << 3)
#define ADC_IBOP3_INC_100_PCT			(0x0 << 3)
#define ADC_IBOP3_INC_150_PCT			(0x1 << 3)
#define ADC_IBCTRL_MSK				(0x7 << 4)
#define ADC_IBCTRL_133_PCT			(0x0 << 4)
#define ADC_IBCTRL_114_PCT			(0x1 << 4)
#define ADC_IBCTRL_100_PCT			(0x2 << 4)
#define ADC_IBCTRL_89_PCT			(0x3 << 4)
#define ADC_IBCTRL_80_PCT			(0x4 << 4)
#define ADC_IBCTRL_73_PCT			(0x5 << 4)
#define ADC_IBCTRL_67_PCT			(0x6 << 4)
#define ADC_IBCTRL_62_PCT			(0x7 << 4)
#define ADC_STOP_RTZ_MSK			(0x1 << 7)
#define ADC_STOP_RTZ_ON				(0x0 << 7)
#define ADC_STOP_RTZ_OFF			(0x1 << 7)

/* AUDIO_ADC_ADC2 */
#define ADC_CAPTRIM_MSK				(0x7 << 0)
#define ADC_CAPTRIM_80_PCT			(0x0 << 0)
#define ADC_CAPTRIM_90_PCT			(0x1 << 0)
#define ADC_CAPTRIM_100_PCT			(0x2 << 0)
#define ADC_CAPTRIM_110_PCT			(0x3 << 0)
#define ADC_CAPTRIM_120_PCT			(0x4 << 0)
#define ADC_CAPTRIM_130_PCT			(0x5 << 0)
#define ADC_CAPTRIM_140_PCT			(0x6 << 0)
#define ADC_CAPTRIM_150_PCT			(0x7 << 0)
#define ADC_ELD_MSK				(0x1 << 3)
#define ADC_ELD_ON				(0x0 << 3)
#define ADC_ELD_OFF				(0x1 << 3)
#define ADC_CHOP_MSK				(0x1 << 4)
#define ADC_CHOP_ON				(0x0 << 4)
#define ADC_CHOP_OFF				(0x1 << 4)
#define ADC_CHOP_SEL_MSK			(0x1 << 5)
#define ADC_CHOP_SEL_FS_50_PCT			(0x0 << 5)
#define ADC_CHOP_SEL_FS_6_25_PCT		(0x1 << 5)
#define ADC_OUT_SEL_MSK				(0x1 << 6)
#define ADC_OUT_SEL_SDM				(0x0 << 6)
#define ADC_OUT_SEL_6K_CLK			(0x1 << 6)

/* AUDIO_ADC_PGA0 */
#define PGA_PWD_MSK				(0x1 << 0)
#define PGA_PWD_EN				(0x0 << 0)
#define PGA_PWD_DIS				(0x1 << 0)
#define PGA_INPUT_DEC_MSK			(0x3 << 1)
#define PGA_INPUT_DEC_N1_34DB			(0x0 << 1)
#define PGA_INPUT_DEC_N4_34DB			(0x1 << 1)
#define PGA_INPUT_DEC_N7_34DB			(0x2 << 1)
#define PGA_INPUT_DEC_N10_34DB			(0x3 << 1)
#define PGA_GAIN_SHIFT				(0x3)
#define PGA_GAIN_MASK				(0x1f << PGA_GAIN_SHIFT)
#define PGA_GAIN_3DB				(0x10 << PGA_GAIN_SHIFT)
#define PGA_GAIN_MIN				(0x0)
#define PGA_GAIN_MAX				(0x1f)

/* AUDIO_ADC_PGA1 */
#define PGA_CHOP_SEL_MSK			(0x3 << 0)
#define PGA_CHOP_SEL_NONE			(0x0 << 0)
#define PGA_CHOP_SEL_200K			(0x1 << 0)
#define PGA_CHOP_SEL_400K			(0x2 << 0)
#define PGA_CHOP_SEL_800K			(0x3 << 0)
#define PGA_IBIAS_CTRL_MSK			(0x3 << 2)
#define PGA_IBIAS_100_PCT			(0x0 << 2)
#define PGA_IBIAS_67_PCT			(0x1 << 1)
#define PGA_IBIAS_133_PCT			(0x2 << 1)
#define PGA_IBIAS_167_PCT			(0x3 << 1)

/* AUDIO_ADC_PGA2 */
#define PGA_BUF_GAIN_MSK			(0x1 << 0)
#define PGA_BUF_GAIN_0DB			(0x0 << 0)
#define PGA_BUF_GAIN_6DB			(0x1 << 0)
#define PGA_BUF_IB_SEL_MSK			(0x3 << 1)
#define PGA_BUF_IB_SEL_100_PCT			(0x0 << 1)
#define PGA_BUF_IB_SEL_67_PCT			(0x1 << 1)
#define PGA_BUF_IB_SEL_133_PCT			(0x2 << 1)
#define PGA_BUF_IB_SEL_167_PCT			(0x3 << 1)
#define PGA_BUF_CHOP_SEL_MSK			(0x3 << 3)
#define PGA_BUF_CHOP_SEL_200K			(0x1 << 3)
#define PGA_BUF_CHOP_SEL_400K			(0x2 << 3)
#define PGA_BUF_CHOP_SEL_800K			(0x3 << 3)

/* AUDIO_ADC_LDO */
#define LDO_MSK					(0x1 << 0)
#define LDO_EN					(0x1 << 0)
#define LDO_DIS					(0x0 << 0)
#define LDO_BYPASS_MSK				(0x1 << 1)
#define LDO_BYPASS_ON				(0x1 << 1)
#define LDO_BYPASS_OFF				(0x0 << 1)
#define LDO_VSEL_MSK				(0x3 << 2)
#define LDO_VSEL_1_5V				(0x0 << 2)
#define LDO_VSEL_1_55V				(0x1 << 2)
#define LDO_VSEL_1_6V				(0x2 << 2)
#define LDO_VSEL_1_65V				(0x3 << 2)
#define ADC_IP_MSK				(0x1 << 7)
#define ADC_IP_EN				(0x1 << 7)
#define ADC_IP_DIS				(0x0 << 7)

/* AUDIO_ADC_HK0 */
#define HK_HALF_VAG_BUF_MSK			(0x1 << 0)
#define HK_HALF_VAG_BUF_ON			(0x1 << 0)
#define HK_HALF_VAG_BUF_OFF			(0x0 << 0)
#define HK_HALF_ADC_BUF_MSK			(0x1 << 1)
#define HK_HALF_ADC_BUF_ON			(0x1 << 1)
#define HK_HALF_ADC_BUF_OFF			(0x0 << 1)
#define HK_VAG_BUF_MSK				(0x1 << 2)
#define HK_VAG_BUF_ON				(0x1 << 2)
#define HK_VAG_BUF_OFF				(0x0 << 2)
#define HK_ADC_BUF_MSK				(0x1 << 3)
#define HK_ADC_BUF_ON				(0x1 << 3)
#define HK_ADC_BUF_OFF				(0x0 << 3)
#define HK_IBIAS_SEL_MSK			(0xf << 4)
#define HK_IBIAS_SEL_200_PCT			(0x8 << 4)
#define HK_IBIAS_SEL_160_PCT			(0x9 << 4)
#define HK_IBIAS_SEL_133_PCT			(0xa << 4)
#define HK_IBIAS_SEL_114_PCT			(0xb << 4)
#define HK_IBIAS_SEL_100_PCT			(0x0 << 4)
#define HK_IBIAS_SEL_80_PCT			(0x1 << 4)
#define HK_IBIAS_SEL_66_PCT			(0x2 << 4)
#define HK_IBIAS_SEL_36_PCT			(0x7 << 4)

/* AUDIO_ADC_HK1 */
#define HK_VREF_1P2V_SEL_MSK			(0x3 << 0)
#define HK_VREF_1P2V_SEL_NORMAL			(0x0 << 0)
#define HK_VREF_1P2V_SEL_P10M			(0x1 << 0)
#define HK_VREF_1P2V_SEL_N10M			(0x2 << 0)
#define HK_VREF_1P2V_SEL_N20M			(0x3 << 0)
#define HL_VAG_CUR_SEL_MSK			(0x3 << 2)
#define HL_VAG_CUR_SEL_6UA			(0x0 << 2)
#define HL_VAG_CUR_SEL_4UA			(0x1 << 2)
#define HL_VAG_CUR_SEL_3UA			(0x2 << 2)
#define HL_VAG_CUR_SEL_1UA			(0x3 << 2)

/* AUDIO_ADC_DIGEN_CLKE */
#define ADCSRT_MSK				(0x7 << 0)
#define ADCSRT(x)				(x)
#define SRST_MSK				(0x1 << 3)
#define SRST_EN					(0x1 << 3)
#define SRST_DIS				(0x0 << 3)
#define I2STX_MSK				(0x1 << 4)
#define I2STX_EN				(0x1 << 4)
#define I2STX_DIS				(0x0 << 4)
#define ADC_MSK					(0x1 << 5)
#define ADC_EN					(0x1 << 5)
#define ADC_DIS					(0x0 << 5)
#define I2STX_CKE_MSK				(0x1 << 6)
#define I2STX_CKE_EN				(0x1 << 6)
#define I2STX_CKE_DIS				(0x0 << 6)
#define ADC_CKE_MSK				(0x1 << 7)
#define ADC_CKE_EN				(0x1 << 7)
#define ADC_CKE_DIS				(0x0 << 7)

/* AUDIO_ADC_VOLL */
#define ADCLV_MSK				(0xff << 0)
#define ADCLV_MIN				(0x0)
#define ADCLV_MAX				(0x7f)
#define ADCLV_SHIFT				(0x1)

/* AUDIO_ADC_AGC0 */
#define ADC_AGC_MSK				(0x1 << 0)
#define ADC_AGC_EN				(0x1 << 0)
#define ADC_AGC_DIS				(0x0 << 0)
#define ADC_NG_MODE_MSK				(0x1 << 1)
#define ADC_NG_MODE_EN				(0x1 << 1)
#define ADC_NG_MODE_DIS				(0x0 << 1)
#define AGC_ZEROCREN_MSK			(0x1 << 2)
#define AGC_ZEROCREN_EN				(0x1 << 2)
#define AGC_ZEROCREN_DIS			(0x0 << 2)
#define ADC_BYPS_MSK				(0x1 << 3)
#define ADC_BYPS_EN				(0x1 << 3)
#define ADC_BYPS_DIS				(0x0 << 3)
#define ADC_AGC_OFFSET_LOW4_MSK			(0xf << 4)

/* AUDIO_ADC_AGC1 */
#define ADC_AGC_OFFSET_HIGH4_MSK		(0xf << 4)

/* AUDIO_ADC_AGC2 */
#define ADC_NG_RSSI_DB_LOW8_MSK			(0xff << 0)

/* AUDIO_ADC_AGC3 */
#define ADC_NG_RSSI_DB_HIGH3_MSK		(0x7 << 0)
#define ADC_NG_PGA_GAIN_MSK			(0x1f << 3)

/* AUDIO_ADC_AGC4 */
#define ADC_TAR_DB_LOW8_MSK			(0xff << 0)

/* AUDIO_ADC_AGC5 */
#define ADC_TAR_DB_HIGH3_MSK			(0x7 << 0)
#define ADC_INI_PGA_GAIN_MSK			(0x1f << 3)

/* AUDIO_ADC_AGC6 */
#define ADC_NG_VOL_CTRL_MSK			(0xff << 0)

/* AUDIO_ADC_AGC7 */
#define ADC_INI_VOL_CTRL_MSK			(0xff << 0)

/* AUDIO_ADC_AGC8 */
#define ADC_POWDET_WIN_MSK			(0xf << 0)
#define ADC_PRATTRATE_WIN_MSK			(0xf << 0)

/* AUDIO_ADC_FILTER */
#define CICCOMP_EN32_MSK			(0x1 << 0)
#define CICCOMP_EN32_EN				(0x1 << 0)
#define CICCOMP_EN32_DIS			(0x0 << 0)
#define CICCOMP_EN64_MSK			(0x1 << 1)
#define CICCOMP_EN64_EN				(0x1 << 1)
#define CICCOMP_EN64_DIS			(0x0 << 1)
#define CICCOMP_CF_MSK				(0x3 << 2)
#define CICCOMP_CF_37_5_PCT			(0x0 << 2)
#define CICCOMP_CF_75_PCT			(0x1 << 2)
#define CICCOMP_CF_100_PCT			(0x2 << 2)
#define HPF_MSK					(0x1 << 4)
#define HPF_EN					(0x1 << 4)
#define HPF_DIS					(0x0 << 4)
#define HPF_CF_MSK				(0x3 << 6)
#define HPF_CF_3_79HZ				(0x0 << 6)
#define HPF_CF_60HZ				(0x1 << 6)
#define HPF_CF_243HZ				(0x2 << 6)
#define HPF_CF_493HZ				(0x3 << 6)
#define AUDIO_ADC_FILTER_MSK			(0xff)
#define AUDIO_ADC_FILTER_MODE1			(HPF_CF_60HZ)
#define AUDIO_ADC_FILTER_MODE2			(HPF_CF_60HZ | CICCOMP_CF_100_PCT | CICCOMP_EN32_EN)
#define AUDIO_ADC_FILTER_MODE3			(HPF_CF_60HZ | CICCOMP_CF_100_PCT | CICCOMP_EN64_EN)

/* AUDIO_ADC_I2S_CKM */
#define I2S_MST_MSK				(0x1 << 0)
#define I2S_MASTER				(0x1 << 0)
#define I2S_SLAVE				(0x0 << 0)
#define SCK_P_MSK				(0x1 << 1)
#define SCK_P					(0x0 << 1)
#define SCK_N					(0x1 << 1)
#define SCK_MSK					(0x1 << 2)
#define SCK_EN					(0x1 << 2)
#define SCK_DIS					(0x0 << 2)
#define SCK_DIV_MSK				(0xf << 4)
#define SCK_DIV(x)				((x - 1) << 4)

/* AUDIO_ADC_I2S_TSD */
#define TXRL_MSK				(0x1 << 0)
#define TXRL_P					(0x0 << 0)
#define TXRL_N					(0x1 << 0)
#define SCKD_TX_MSK				(0x3 << 1)
#define SCKD_TX_64				(0x0 << 1)
#define SCKD_TX_128				(0x1 << 1)
#define SCKD_TX_256				(0x2 << 1)
#define VDW_TX_MSK				(0x1f << 3)
#define VDW_TX(x)				((x - 1) << 3)

/* AUDIO_ADC_I2S_TXCR1 */
#define LSB_TX_MSK				(0x1 << 0)
#define LSB_TX_MSB				(0x0 << 0)
#define LSB_TX_LSB				(0x1 << 0)
#define EXRL_TX_MSK				(0x1 << 1)
#define EXRL_TX_LEFT				(0x0 << 1)
#define EXRL_TX_RIGHT				(0x1 << 1)
#define IBM_TX_MSK				(0x3 << 2)
#define IBM_TX_NORMAL				(0x0 << 2)
#define IBM_TX_LEFT				(0x1 << 2)
#define IBM_TX_RIGHT				(0x2 << 2)
#define PDM_TX_MSK				(0x3 << 4)
#define PDM_TX_NO_DELAY				(0x0 << 4)
#define PDM_TX_1_DELAY				(0x1 << 4)
#define PDM_TX_2_DELAY				(0x2 << 4)
#define PDM_TX_3_DELAY				(0x3 << 4)
#define TFS_TX_MSK				(0x1 << 6)
#define TFS_TX_I2S				(0x0 << 6)
#define TFS_TX_PCM				(0x1 << 6)

/* AUDIO_ADC_I2S_TXCR2_TXCMD */
#define RCNT_TX_MSK				(0x3f << 0)
#define TXC_MSK					(0x1 << 6)
#define TXC_EN					(0x1 << 6)
#define TXC_DIS					(0x0 << 6)
#define TXS_MSK					(0x1 << 7)
#define TXS_START				(0x1 << 7)
#define TXS_STOP				(0x0 << 7)

#endif
