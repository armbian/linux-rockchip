// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * rk730.c -- RK730 ALSA SoC Audio driver
 *
 * Copyright (C) 2022 Rockchip Electronics Co., Ltd.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include "rk730.h"

/* Output diagram
 * FOR current evb, LOUT2 to speaker and LOUT2 to headphone
 * DAC0(DAC_L_N/DAC_R_N)->HP_0 -> LOUT2
 * DAC1(DAC_L_P/DAC_R_P)->SP_0 -> LOUT1
 *
 * FOR HP differential mode
 *- DAC_L_N(DAC0) -> HP Driver -> LOUT2 ----------
 *- DAC_R_N(DAC0) -> HP Driver -> ROUT2 ----      |
 *                                          |     |
 *                                          |     R
 *                                          R     |
 *                                          |     |
 *- DAC_L_P(DAC1) -> SPK Driver -> LOUT1 ---|-----
 *- DAC_R_P(DAC1) -> SPK Driver -> ROUT1 ---
 */

enum rk730_mix_mode {
	RK730_MIX_MODE_1_PATH,
	RK730_MIX_MODE_2_PATHS,
	RK730_MIX_MODE_3_PATHS,
};

enum rk730_chop_freq {
	RK730_CHOP_FREQ_NONE,
	RK730_CHOP_FREQ_200KHZ,
	RK730_CHOP_FREQ_400KHZ,
	RK730_CHOP_FREQ_800KHZ,
};

struct rk730_priv {
	struct regmap *regmap;
	struct clk *mclk;
	unsigned int sysclk;
	atomic_t mix_mode;
	bool fixed_mclk_fs;
};

/* ADC Digital Volume */
static const DECLARE_TLV_DB_MINMAX(adc_dig_tlv, -9600, 0);
/* DAC Digital Volume */
static const DECLARE_TLV_DB_MINMAX(dac_dig_tlv, -9600, 0);
/* ADC Volume */
static const DECLARE_TLV_DB_MINMAX(adc_tlv, -1050, 0);
/* MIC Boost Volume */
static const DECLARE_TLV_DB_RANGE(micboost_tlv,
	0, 2, TLV_DB_SCALE_ITEM(0, 600, 0),
	3, 4, TLV_DB_SCALE_ITEM(2400, 1200, 0),
	5, 7, TLV_DB_SCALE_ITEM(4200, 600, 0),
);

static const DECLARE_TLV_DB_SCALE(micdecrease_tlv, 0, -300, 0);

static const char * const mux_input_l_text[] = { "DIFF", "VINR2", "VINL2" };
static const char * const mux_input_r_text[] = { "DIFF", "VINR1", "VINL1" };
static SOC_ENUM_SINGLE_DECL(mux_input_l_enum, RK730_ADC_PGA_BLOCK_0,
			    4, mux_input_l_text);
static SOC_ENUM_SINGLE_DECL(mux_input_r_enum, RK730_ADC_PGA_BLOCK_1,
			    4, mux_input_r_text);
static const struct snd_kcontrol_new mux_input_l =
	SOC_DAPM_ENUM("Left Input Mux", mux_input_l_enum);
static const struct snd_kcontrol_new mux_input_r =
	SOC_DAPM_ENUM("Right Input Mux", mux_input_r_enum);

static const char * const adc_data_sel_text[] = { "normal", "left", "right", "swap" };

static SOC_ENUM_SINGLE_DECL(adc_data_sel_enum, RK730_DADC_SEL,
			    0, adc_data_sel_text);
static const struct snd_kcontrol_new adc_data_sel =
	SOC_DAPM_ENUM("ADCSel Mux", adc_data_sel_enum);

static const char * const chop_freq_text[] = {
	"Disabled", "200kHz", "400kHz", "800kHz",
};

static const char * const hfp_center_freq_text[] = {
	"80HZ", "100HZ", "120HZ", "140HZ",
};

static const char * const dac_hfp_center_freq_text[] = {
	"C*0.8", "C*0.9", "C*1.0(fs=6.144M)", "C*1.1(fs=5.6448M)",
	"C*1.2", "C*1.3", "C*1.4", "C*1.5(fs=4.096M)",
};

static SOC_ENUM_SINGLE_DECL(mic_chop_freq_enum, RK730_MIC_BOOST_3,
			    6, chop_freq_text);
static SOC_ENUM_SINGLE_DECL(adc_pga_chop_freq_enum, RK730_ADC_PGA_BLOCK_1,
			    6, chop_freq_text);
static SOC_ENUM_SINGLE_DECL(hp_lo_chop_freq_enum, RK730_HP_1,
			    5, chop_freq_text);
static SOC_ENUM_SINGLE_DECL(dac_hfp_center_freq_enum, RK730_DDAC_FILTER,
			    2, hfp_center_freq_text);

static SOC_ENUM_SINGLE_DECL(adc_capacity_trim_enum, RK730_ADC_2,
			    0, dac_hfp_center_freq_text);

static const char * const micbias_volt_text[] = {
	"2.0v", "2.2v", "2.5v", "2.8v",
};

static SOC_ENUM_SINGLE_DECL(micbias_volt_enum, RK730_MIC_BIAS,
			    2, micbias_volt_text);

static const char * const dig_ldo_volt_text[] = {
	"1.4v", "1.5v", "1.6v", "1.7v",
};

static SOC_ENUM_SINGLE_DECL(dig_ldo_volt_enum, RK730_LDO,
			    0, dig_ldo_volt_text);

static const char * const ana_ldo_volt_text[] = {
	"1.4v", "1.5v", "1.6v", "1.7v",
};

static const struct snd_kcontrol_new rk730_out1_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new rk730_out2_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static SOC_ENUM_SINGLE_DECL(ana_ldo_volt_enum, RK730_LDO,
			    4, ana_ldo_volt_text);

static const char * const adc_sdo_sel_tx_text[] = {
	"ADCL ADCR", "ADCL ADCR DACL DACR", "ADCL DACL", "ADCL DACR",
	"ADCR DACL", "ADCR DACR",           "DACL DACR",
};

static SOC_ENUM_SINGLE_DECL(adc_sdo_sel_tx_enum, RK730_DI2S_TXCR2,
			    5, adc_sdo_sel_tx_text);

static int rk730_pll_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		dev_dbg(component->dev, "%s on\n", __func__);
		snd_soc_component_write(component, RK730_SYSPLL_0, 0x00);
	} else {
		dev_dbg(component->dev, "%s off\n", __func__);
		snd_soc_component_write(component, RK730_SYSPLL_0, 0xff);
	}
	return 0;
}

static int rk730_dacl_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event))
		dev_dbg(component->dev, "%s on\n", __func__);
	else
		dev_dbg(component->dev, "%s off\n", __func__);
	return 0;
}

static int rk730_out1_drv_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event))
		dev_dbg(component->dev, "%s on\n", __func__);
	else
		dev_dbg(component->dev, "%s off\n", __func__);
	return 0;
}

static int rk730_out2_drv_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event))
		dev_dbg(component->dev, "%s on\n", __func__);
	else
		dev_dbg(component->dev, "%s off\n", __func__);
	return 0;
}

static int rk730_dacr_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event))
		dev_dbg(component->dev, "%s on\n", __func__);
	else
		dev_dbg(component->dev, "%s off\n", __func__);
	return 0;
}

static int rk730_sdin_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(component->dev, "%s on\n", __func__);
		snd_soc_component_update_bits(component, RK730_DI2S_RXCMD_TSD,
					      RK730_DI2S_RXCMD_TSD_RXS_MASK,
					      RK730_DI2S_RXCMD_TSD_RXS_EN);
		snd_soc_component_update_bits(component, RK730_DTOP_DIGEN_CLKE,
					      RK730_DTOP_DIGEN_CLKE_I2SRX_CKE_MASK |
					      RK730_DTOP_DIGEN_CLKE_I2SRX_EN_MASK,
					      RK730_DTOP_DIGEN_CLKE_I2SRX_CKE_EN |
					      RK730_DTOP_DIGEN_CLKE_I2SRX_EN);
		snd_soc_component_update_bits(component, RK730_DTOP_DIGEN_CLKE,
					      RK730_DTOP_DIGEN_CLKE_DAC_CKE_MASK |
					      RK730_DTOP_DIGEN_CLKE_DAC_EN_MASK,
					      RK730_DTOP_DIGEN_CLKE_DAC_CKE_EN |
					      RK730_DTOP_DIGEN_CLKE_DAC_EN);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(component->dev, "%s off\n", __func__);
		snd_soc_component_update_bits(component, RK730_DTOP_DIGEN_CLKE,
					      RK730_DTOP_DIGEN_CLKE_DAC_CKE_MASK |
					      RK730_DTOP_DIGEN_CLKE_DAC_EN_MASK,
					      RK730_DTOP_DIGEN_CLKE_DAC_CKE_DIS |
					      RK730_DTOP_DIGEN_CLKE_DAC_DIS);
		snd_soc_component_update_bits(component, RK730_DTOP_DIGEN_CLKE,
					      RK730_DTOP_DIGEN_CLKE_I2SRX_CKE_MASK |
					      RK730_DTOP_DIGEN_CLKE_I2SRX_EN_MASK,
					      RK730_DTOP_DIGEN_CLKE_I2SRX_CKE_DIS |
					      RK730_DTOP_DIGEN_CLKE_I2SRX_DIS);
		snd_soc_component_update_bits(component, RK730_DI2S_RXCMD_TSD,
					      RK730_DI2S_RXCMD_TSD_RXS_MASK,
					      RK730_DI2S_RXCMD_TSD_RXS_DIS);
		break;
	default:
		dev_err(component->dev, "%s Invalid event = 0x%x\n", __func__, event);
	}
	return 0;
}

static int rk730_sdout_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		dev_dbg(component->dev, "%s on\n", __func__);
		snd_soc_component_update_bits(component, RK730_DI2S_TXCR3_TXCMD,
					      RK730_DI2S_TXCR_3_TXCMD_TXS_MASK,
					      RK730_DI2S_TXCR_3_TXCMD_TXS_EN);
		snd_soc_component_update_bits(component, RK730_DTOP_DIGEN_CLKE,
					      RK730_DTOP_DIGEN_CLKE_ADC_CKE_MASK |
					      RK730_DTOP_DIGEN_CLKE_I2STX_CKE_MASK,
					      RK730_DTOP_DIGEN_CLKE_ADC_CKE_EN |
					      RK730_DTOP_DIGEN_CLKE_I2STX_CKE_EN);
		usleep_range(20000, 21000);
		snd_soc_component_update_bits(component, RK730_DTOP_DIGEN_CLKE,
					      RK730_DTOP_DIGEN_CLKE_ADC_EN_MASK |
					      RK730_DTOP_DIGEN_CLKE_I2STX_EN_MASK,
					      RK730_DTOP_DIGEN_CLKE_ADC_EN |
					      RK730_DTOP_DIGEN_CLKE_I2STX_EN);
	} else {
		dev_dbg(component->dev, "%s off\n", __func__);

		snd_soc_component_update_bits(component, RK730_DTOP_DIGEN_CLKE,
					      RK730_DTOP_DIGEN_CLKE_ADC_EN_MASK |
					      RK730_DTOP_DIGEN_CLKE_I2STX_EN_MASK,
					      RK730_DTOP_DIGEN_CLKE_ADC_DIS |
					      RK730_DTOP_DIGEN_CLKE_I2STX_DIS);
		usleep_range(50, 60);
		snd_soc_component_update_bits(component, RK730_DTOP_DIGEN_CLKE,
					      RK730_DTOP_DIGEN_CLKE_ADC_CKE_MASK |
					      RK730_DTOP_DIGEN_CLKE_I2STX_CKE_MASK,
					      RK730_DTOP_DIGEN_CLKE_ADC_CKE_DIS |
					      RK730_DTOP_DIGEN_CLKE_I2STX_CKE_DIS);
		snd_soc_component_update_bits(component, RK730_DI2S_TXCR3_TXCMD,
					      RK730_DI2S_TXCR_3_TXCMD_TXS_MASK,
					      RK730_DI2S_TXCR_3_TXCMD_TXS_DIS);
	}
	return 0;
}

static int rk730_adcr_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(component->dev, "%s on\n", __func__);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(component->dev, "%s off\n", __func__);
		break;
	default:
		dev_err(component->dev, "%s Invalid event = 0x%x\n", __func__, event);
	}
	return 0;
}

static int rk730_adcl_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(component->dev, "%s on\n", __func__);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(component->dev, "%s off\n", __func__);
		break;
	default:
		dev_err(component->dev, "%s Invalid event = 0x%x\n", __func__, event);
	}
	return 0;
}

static int rk730_dac0_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		dev_dbg(component->dev, "%s on\n", __func__);

		/* analog  config */
		snd_soc_component_update_bits(component, RK730_DAC_0,
					      RK730_DAC_0_DAC_R_HP_PWD_MASK |
					      RK730_DAC_0_DAC_L_HP_PWD_MASK |
					      RK730_DAC_0_DAC_HP_IBIAS_MASK,
					      RK730_DAC_0_DAC_L_HP_PWU |
					      RK730_DAC_0_DAC_R_HP_PWU |
					      RK730_DAC_0_DAC_HP_IBIAS_ON);
		snd_soc_component_update_bits(component, RK730_HP_0,
					      RK730_HP_0_ANTIPOP_PWR_MASK |
					      RK730_HP_0_HP_IBIAS_MASK |
					      RK730_HP_0_HP_TWOTAGE_EN_MASK |
					      RK730_HP_0_HP_OSTG_PWR_MASK |
					      RK730_HP_0_HP_BLOCK_PWR_MASK,
					      RK730_HP_0_ANTIPOP_PWR_OFF |
					      RK730_HP_0_HP_IBIAS_ON |
					      RK730_HP_0_HP_TWOTAGE_EN |
					      RK730_HP_0_HP_OSTG_PWR_ON |
					      RK730_HP_0_HP_BLOCK_PWR_ON);
	} else {
		dev_dbg(component->dev, "%s off\n", __func__);

		/* analog  config */
		snd_soc_component_update_bits(component, RK730_HP_0,
					      RK730_HP_0_ANTIPOP_PWR_MASK |
					      RK730_HP_0_HP_IBIAS_MASK |
					      RK730_HP_0_HP_TWOTAGE_EN_MASK |
					      RK730_HP_0_HP_OSTG_PWR_MASK |
					      RK730_HP_0_HP_BLOCK_PWR_MASK,
					      RK730_HP_0_ANTIPOP_PWR_ON |
					      RK730_HP_0_HP_IBIAS_OFF |
					      RK730_HP_0_HP_TWOTAGE_DIS |
					      RK730_HP_0_HP_OSTG_PWR_OFF |
					      RK730_HP_0_HP_BLOCK_PWR_OFF);
		snd_soc_component_update_bits(component, RK730_DAC_0,
					      RK730_DAC_0_DAC_R_HP_PWD_MASK |
					      RK730_DAC_0_DAC_L_HP_PWD_MASK |
					      RK730_DAC_0_DAC_HP_IBIAS_MASK,
					      RK730_DAC_0_DAC_L_HP_PWD |
					      RK730_DAC_0_DAC_R_HP_PWD|
					      RK730_DAC_0_DAC_HP_IBIAS_OFF);
	}
	return 0;
}

static int rk730_dac1_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		dev_dbg(component->dev, "%s on\n", __func__);

		/* analog  config */
		snd_soc_component_update_bits(component, RK730_DAC_1,
					      RK730_DAC_1_DAC_SPK_IBIAS_MASK |
					      RK730_DAC_1_DAC_R_SPK_PWD_MASK |
					      RK730_DAC_1_DAC_L_SPK_PWD_MASK,
					      RK730_DAC_1_DAC_SPK_IBIAS_ON |
					      RK730_DAC_1_DAC_R_SPK_PWU |
					      RK730_DAC_1_DAC_L_SPK_PWU);
		snd_soc_component_update_bits(component, RK730_SPK_0,
					      RK730_SPK_0_ANTIPOP_PWR_MASK |
					      RK730_SPK_0_SPK_IBIAS_MASK |
					      RK730_SPK_0_SPK_OSTAGE_PWR_MASK |
					      RK730_SPK_0_SPK_BLOCK_PWR_MASK,
					      RK730_SPK_0_ANTIPOP_PWR_OFF |
					      RK730_SPK_0_SPK_IBIAS_ON |
					      RK730_SPK_0_SPK_OSTAGE_PWR_ON |
					      RK730_SPK_0_SPK_BLOCK_PWR_ON);
	} else {
		dev_dbg(component->dev, "%s off\n", __func__);

		/* analog  config */
		snd_soc_component_update_bits(component, RK730_SPK_0,
					      RK730_SPK_0_ANTIPOP_PWR_MASK |
					      RK730_SPK_0_SPK_IBIAS_MASK |
					      RK730_SPK_0_SPK_OSTAGE_PWR_MASK |
					      RK730_SPK_0_SPK_BLOCK_PWR_MASK,
					      RK730_SPK_0_ANTIPOP_PWR_ON |
					      RK730_SPK_0_SPK_IBIAS_OFF |
					      RK730_SPK_0_SPK_OSTAGE_PWR_OFF |
					      RK730_SPK_0_SPK_BLOCK_PWR_OFF);
		snd_soc_component_update_bits(component, RK730_DAC_1,
					      RK730_DAC_1_DAC_SPK_IBIAS_MASK |
					      RK730_DAC_1_DAC_R_SPK_PWD_MASK |
					      RK730_DAC_1_DAC_L_SPK_PWD_MASK,
					      RK730_DAC_1_DAC_SPK_IBIAS_OFF |
					      RK730_DAC_1_DAC_R_SPK_PWD |
					      RK730_DAC_1_DAC_L_SPK_PWD);
	}
	return 0;
}

static int rk730_out1_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event))
		dev_dbg(component->dev, "%s on\n", __func__);
	else
		dev_dbg(component->dev, "%s off\n", __func__);
	return 0;
}

static int rk730_out2_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (SND_SOC_DAPM_EVENT_ON(event))
		dev_dbg(component->dev, "%s on\n", __func__);
	else
		dev_dbg(component->dev, "%s off\n", __func__);
	return 0;
}

static const struct snd_kcontrol_new rk730_snd_controls[] = {
	SOC_DOUBLE_R_TLV("ADC PGA Volume", RK730_ADC_PGA_BLOCK_0, RK730_ADC_PGA_BLOCK_1,
			 1, 0x7, 1, adc_tlv),
	SOC_DOUBLE_R_TLV("MIC2 Boost Volume", RK730_MIC_BOOST_0, RK730_MIC_BOOST_1,
			 1, 0x7, 0, micboost_tlv),
	SOC_DOUBLE_R_TLV("MIC1 Boost Volume", RK730_MIC_BOOST_2, RK730_MIC_BOOST_3,
			 1, 0x7, 0, micboost_tlv),
	SOC_DOUBLE_R_TLV("MIC2 decrease Volume", RK730_MIC_BOOST_0, RK730_MIC_BOOST_1,
			 4, 0x3, 0, micdecrease_tlv),
	SOC_DOUBLE_R_TLV("MIC1 decrease Volume", RK730_MIC_BOOST_2, RK730_MIC_BOOST_3,
			 4, 0x3, 0, micdecrease_tlv),
	SOC_DOUBLE_R_TLV("ADC Digital Volume", RK730_DADC_VOLL, RK730_DADC_VOLR,
			 0, 0xff, 1, adc_dig_tlv),
	SOC_DOUBLE_R_TLV("DAC Digital Volume", RK730_DDAC_VOLL, RK730_DDAC_VOLR,
			 0, 0xff, 1, dac_dig_tlv),
	SOC_ENUM("DIG DLO VOLTAGE", dig_ldo_volt_enum),
	SOC_ENUM("ANA DLO VOLTAGE", ana_ldo_volt_enum),
	SOC_ENUM("MIC Chop Freq", mic_chop_freq_enum),
	SOC_ENUM("ADC PGA Chop Freq", adc_pga_chop_freq_enum),
	SOC_ENUM("HP / Lineout Chop Freq", hp_lo_chop_freq_enum),
	SOC_ENUM("Mic Bias Volt", micbias_volt_enum),
	SOC_ENUM("DAC HPF Center Freq", dac_hfp_center_freq_enum),
	SOC_ENUM("ADC CAPACITY TRIM", adc_capacity_trim_enum),
	SOC_ENUM("ADC SDO SEL TX", adc_sdo_sel_tx_enum),
	SOC_SINGLE("ADC Volume Bypass Switch", RK730_DTOP_VUCTL, 7, 1, 0),
	SOC_SINGLE("DAC Volume Bypass Switch", RK730_DTOP_VUCTL, 6, 1, 0),
	SOC_SINGLE("ADC Fade Switch", RK730_DTOP_VUCTL, 5, 1, 0),
	SOC_SINGLE("DAC Fade Switch", RK730_DTOP_VUCTL, 4, 1, 0),
	SOC_SINGLE("DAC Left gain polarity", RK730_DTOP_VUCTL, 3, 1, 0),
	SOC_SINGLE("DAC Right gain polarity", RK730_DTOP_VUCTL, 2, 1, 0),
	SOC_SINGLE("ADC Zero Crossing Switch", RK730_DTOP_VUCTL, 1, 1, 0),
	SOC_SINGLE("DAC Zero Crossing Switch", RK730_DTOP_VUCTL, 0, 1, 0),
	SOC_SINGLE("MIC1N / MIC2P Exchanged Switch", RK730_MIC_BOOST_2, 7, 1, 0),
	SOC_SINGLE("ADC CHOP EN", RK730_ADC_2, 4, 1, 0),
};

static const struct snd_soc_dapm_widget rk730_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("ANA LDO", 1, RK730_LDO, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("PLL", 1, SND_SOC_NOPM, 0, 0,
			      rk730_pll_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DAC BUF", 2, RK730_HK_TOP_2, 0, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("HK VAG BUF", 0, RK730_HK_TOP_1, 7, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("HK DAC BUF", 0, RK730_HK_TOP_1, 6, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("MICBIAS", 1, RK730_MIC_BIAS, 0, 1, NULL, 0),
	SND_SOC_DAPM_DAC_E("DAC0", NULL, SND_SOC_NOPM, 0, 0,
			   rk730_dac0_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC1", NULL, SND_SOC_NOPM, 0, 0,
			   rk730_dac1_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DACL", "Left Playback", SND_SOC_NOPM, 0, 0,
			   rk730_dacl_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DACR", "Right Playback", SND_SOC_NOPM, 0, 0,
			   rk730_dacr_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("OUT1 Power", SND_SOC_NOPM, 0, 0,
			    rk730_out1_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("OUT2 Power", SND_SOC_NOPM, 0, 0,
			    rk730_out2_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("OUT1", SND_SOC_NOPM, 0, 0, &rk730_out1_switch),
	SND_SOC_DAPM_SWITCH("OUT2", SND_SOC_NOPM, 0, 0, &rk730_out2_switch),
	SND_SOC_DAPM_OUT_DRV_E("OUT1 DRV", SND_SOC_NOPM, 0, 0, NULL, 0, rk730_out1_drv_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("OUT2 DRV", SND_SOC_NOPM, 0, 0, NULL, 0, rk730_out2_drv_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),
	SND_SOC_DAPM_AIF_IN_E("I2S IN", "HiFi Playback", 0, SND_SOC_NOPM, 0, 0,
			      rk730_sdin_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("I2S OUT", "HiFi Capture", 0, SND_SOC_NOPM, 0, 0,
			       rk730_sdout_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADCL", "Left Capture", RK730_ADC_0, 0, 1,
			   rk730_adcl_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_ADC_E("ADCR", "Right Capture", RK730_ADC_0, 1, 1,
			   rk730_adcr_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MUX("ADCSel Mux", SND_SOC_NOPM, 0, 1, &adc_data_sel),
	SND_SOC_DAPM_MUX("Left PGA Mux", RK730_ADC_PGA_BLOCK_0, 0, 1, &mux_input_l),
	SND_SOC_DAPM_MUX("Right PGA Mux", RK730_ADC_PGA_BLOCK_1, 0, 1, &mux_input_r),
	/* 0:mic_r2_pwd 3:1 mic_r2_boost */
	SND_SOC_DAPM_PGA("MIC2R", RK730_MIC_BOOST_0, 0, 1, NULL, 0),
	SND_SOC_DAPM_PGA("MIC2L", RK730_MIC_BOOST_1, 0, 1, NULL, 0),
	SND_SOC_DAPM_PGA("MIC1R", RK730_MIC_BOOST_2, 0, 1, NULL, 0),
	SND_SOC_DAPM_PGA("MIC1L", RK730_MIC_BOOST_3, 0, 1, NULL, 0),
	SND_SOC_DAPM_PGA("DIFFL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DIFFR", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MIC2"),
};

static const struct snd_soc_dapm_route rk730_dapm_routes[] = {
	{"OUT1", NULL, "OUT1 Power"},
	{"OUT2", NULL, "OUT2 Power"},

	{"LOUT1", NULL, "OUT1 DRV"},
	{"ROUT1", NULL, "OUT1 DRV"},
	{"LOUT2", NULL, "OUT2 DRV"},
	{"ROUT2", NULL, "OUT2 DRV"},

	{"OUT1 DRV", NULL, "OUT1"},
	{"OUT2 DRV", NULL, "OUT2"},

	{"OUT1", "Switch", "DAC1"},
	{"OUT2", "Switch", "DAC0"},

	{"DAC1", NULL, "I2S IN"},
	{"DAC0", NULL, "I2S IN"},

	{"HK DAC BUF", NULL, "HK VAG BUF"},
	{"I2S IN", NULL, "HK DAC BUF"},
	{"I2S OUT", NULL, "HK VAG BUF"},
	{"I2S IN", NULL, "PLL"},
	{"I2S OUT", NULL, "PLL"},
	{"I2S IN", NULL, "ANA LDO"},
	{"I2S OUT", NULL, "ANA LDO"},

	{"I2S OUT", NULL, "ADCR"},
	{"I2S OUT", NULL, "ADCL"},
	{"ADCR", NULL, "ADCSel Mux"},
	{"ADCL", NULL, "ADCSel Mux"},

	{"ADCSel Mux", "normal", "Left PGA Mux"},
	{"ADCSel Mux", "normal", "Right PGA Mux"},
	{"ADCSel Mux", "swap", "Left PGA Mux"},
	{"ADCSel Mux", "swap", "Right PGA Mux"},
	{"ADCSel Mux", "left", "Left PGA Mux"},
	{"ADCSel Mux", "right", "Right PGA Mux"},

	{"Left PGA Mux", "DIFF", "DIFFL"},
	{"Left PGA Mux", "VINR2", "MIC2R"},
	{"Left PGA Mux", "VINL2", "MIC2L"},

	{"Right PGA Mux", "DIFF", "DIFFR"},
	{"Right PGA Mux", "VINR1", "MIC1R"},
	{"Right PGA Mux", "VINL1", "MIC1L"},

	{"DIFFL", NULL, "MIC2R"},
	{"DIFFL", NULL, "MIC2L"},
	{"DIFFR", NULL, "MIC1R"},
	{"DIFFR", NULL, "MIC1L"},

	{"MIC1R", NULL, "MIC1"},
	{"MIC1L", NULL, "MIC1"},
	{"MIC2R", NULL, "MIC2"},
	{"MIC2L", NULL, "MIC2"},
	{"MIC1", NULL, "MICBIAS"},
	{"MIC2", NULL, "MICBIAS"},
};

struct _coeff_div {
	int mclk;
	int rate;
	char syspll_channel;
	char fsclk_channel;
	char refclk_channel;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* mclk */
	{12288000, 48000, 0x0, 0x1, 0x0},
	{12288000, 96000, 0x0, 0x1, 0x0},
	{12288000, 192000, 0x0, 0x1, 0x0},
	{12288000, 44100, 0x1, 0x1, 0x0},
	{12288000, 88200, 0x1, 0x1, 0x0},
	{12288000, 176000, 0x1, 0x1, 0x0},
	{12288000, 8000, 0x2, 0x3, 0x0},
	{12288000, 16000, 0x2, 0x3, 0x0},
	{12288000, 32000, 0x2, 0x3, 0x0},
	{12288000, 64000, 0x2, 0x3, 0x0},
	{12288000, 128000, 0x2, 0x3, 0x0},

	{12000000, 48000, 0x3, 0x1, 0x0},
	{12000000, 96000, 0x3, 0x1, 0x0},
	{12000000, 192000, 0x3, 0x1, 0x0},
	{12000000, 44100, 0x4, 0x1, 0x0},
	{12000000, 88200, 0x4, 0x1, 0x0},
	{12000000, 176000, 0x4, 0x1, 0x0},
	{12000000, 8000, 0x5, 0x3, 0x0},
	{12000000, 16000, 0x5, 0x3, 0x0},
	{12000000, 32000, 0x5, 0x3, 0x0},
	{12000000, 64000, 0x5, 0x3, 0x0},
	{12000000, 128000, 0x5, 0x3, 0x0},

	{24000000, 48000, 0x9, 0x1, 0x1},
	{24000000, 96000, 0x9, 0x1, 0x1},
	{24000000, 192000, 0x9, 0x1, 0x1},
	{24000000, 44100, 0xa, 0x1, 0x1},
	{24000000, 88200, 0xa, 0x1, 0x1},
	{24000000, 176000, 0xa, 0x1, 0x1},
	{24000000, 8000, 0xb, 0x3, 0x1},
	{24000000, 16000, 0xb, 0x3, 0x1},
	{24000000, 32000, 0xb, 0x3, 0x1},
	{24000000, 64000, 0xb, 0x3, 0x1},
	{24000000, 128000, 0xb, 0x3, 0x1},

	{6144000, 48000, 0xc, 0x1, 0x0},
	{6144000, 96000, 0xc, 0x1, 0x0},
	{6144000, 192000, 0xc, 0x1, 0x0},
	{11289600, 44100, 0xd, 0x1, 0x0},
	{11289600, 88200, 0xd, 0x1, 0x0},
	{11289600, 176000, 0xd, 0x1, 0x0},
	{8192000, 8000, 0xe, 0x3, 0x0},
	{8192000, 16000, 0xe, 0x3, 0x0},
	{8192000, 32000, 0xe, 0x3, 0x0},
	{8192000, 64000, 0xe, 0x3, 0x0},
	{8192000, 128000, 0xe, 0x3, 0x0},

	/* uncommon sample rate groups */
	{12288000, 11025, 0x1, 0x1, 0x0},
	{12288000, 22050, 0x1, 0x1, 0x0},
	{12000000, 11025, 0x4, 0x1, 0x0},
	{12000000, 22050, 0x4, 0x1, 0x0},
	{24000000, 11025, 0xa, 0x1, 0x0},
	{24000000, 22050, 0xa, 0x1, 0x0},
	{11289600, 11025, 0xd, 0x1, 0x0},
	{11289600, 22050, 0xd, 0x1, 0x0},
};

static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}
	return -EINVAL;
}

struct _coeff_clk {
	int mclk;
	int rate;
};

/* codec selects the required mclk and sets it by itself  */
static const struct _coeff_clk coeff_clk[] = {
	/* mclks */
	{12288000, 48000},
	{12288000, 96000},
	{12288000, 192000},
	{11289600, 44100},
	{11289600, 88200},
	{11289600, 176000},
	{8192000, 8000},
	{8192000, 16000},
	{8192000, 32000},
	{8192000, 64000},
	{8192000, 128000},
	/* uncommon sample rate groups */
	{11289600, 11025},
	{11289600, 22050},
};

static inline int get_coeff_clk(int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_clk); i++) {
		if (coeff_clk[i].rate == rate)
			return coeff_clk[i].mclk;
	}
	return -EINVAL;
}

static unsigned int samplerate_to_bit(unsigned int samplerate)
{
	switch (samplerate) {
	case 8000:
	case 11025:
		return 0;
	case 16000:
	case 22050:
		return 1;
	case 32000:
	case 44100:
	case 48000:
		return 2;
	case 64000:
	case 88200:
	case 96000:
		return 3;
	case 128000:
	case 176400:
	case 192000:
		return 4;
	default:
		return 2;
	}
}

static int rk730_dai_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct rk730_priv *rk730 = snd_soc_component_get_drvdata(component);
	unsigned int rate;
	int coeff;

	if (!rk730->fixed_mclk_fs) {
		rk730->sysclk = get_coeff_clk(params_rate(params));
		if ((int)rk730->sysclk < 0) {
			dev_err(component->dev,
				"Unable to lookup coeff clk with sample rate %dHz\n",
				params_rate(params));
			return -EINVAL;
		}
		dev_info(component->dev, "%s: Lookup mclk:%d for rate:%d\n",
			 __func__, rk730->sysclk, params_rate(params));

		if (clk_set_rate(rk730->mclk, rk730->sysclk) < 0) {
			dev_err(component->dev,
				"Unable to set mclk %dHz\n", rk730->sysclk);
			return -EINVAL;
		}
	}

	coeff = get_coeff(rk730->sysclk, params_rate(params));
	if (coeff < 0)
		coeff = get_coeff(rk730->sysclk / 2, params_rate(params));
	if (coeff < 0)
		coeff = get_coeff(rk730->sysclk * 2, params_rate(params));
	if (coeff < 0) {
		dev_err(component->dev,
			"Unable to configure sample rate %dHz with %dHz MCLK\n",
			params_rate(params), rk730->sysclk);
		return coeff;
	}
	dev_info(component->dev, "%s:index %d  mclk=%d rate=%d\n",
		 __func__, coeff, coeff_div[coeff].mclk, coeff_div[coeff].rate);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			snd_soc_component_update_bits(component, RK730_DI2S_RXCR2,
						      RK730_DI2S_RXCR2_VDW_MASK,
						      RK730_DI2S_RXCR2_VDW(16));
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			snd_soc_component_update_bits(component, RK730_DI2S_RXCR2,
						      RK730_DI2S_RXCR2_VDW_MASK,
						      RK730_DI2S_RXCR2_VDW(24));
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			snd_soc_component_update_bits(component, RK730_DI2S_RXCR2,
						      RK730_DI2S_RXCR2_VDW_MASK,
						      RK730_DI2S_RXCR2_VDW(32));
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			snd_soc_component_update_bits(component, RK730_DI2S_TXCR2,
						      RK730_DI2S_TXCR2_VDW_MASK,
						      RK730_DI2S_TXCR2_VDW(16));
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			snd_soc_component_update_bits(component, RK730_DI2S_TXCR2,
						      RK730_DI2S_TXCR2_VDW_MASK,
						      RK730_DI2S_TXCR2_VDW(24));
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			snd_soc_component_update_bits(component, RK730_DI2S_TXCR2,
						      RK730_DI2S_TXCR2_VDW_MASK,
						      RK730_DI2S_TXCR2_VDW(32));
			break;
		default:
			return -EINVAL;
		}
	}

	rate = samplerate_to_bit(params_rate(params));
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_soc_component_update_bits(component, RK730_DTOP_SRT,
					      RK730_DTOP_DACSRT_MASK,
					      RK730_DTOP_DACSRT(rate));
	} else {
		snd_soc_component_update_bits(component, RK730_DTOP_SRT,
					      RK730_DTOP_ADCSRT_MASK,
					      RK730_DTOP_ADCSRT(rate));
	}
	snd_soc_component_write(component, RK730_SYSPLL_3,
				coeff_div[coeff].syspll_channel << 4 |
				coeff_div[coeff].fsclk_channel << 2 |
				coeff_div[coeff].refclk_channel << 0);
	return 0;
}

static int rk730_dai_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	u8 val = 0, format = 0;
	u8 mask = RK730_DI2S_TXCR1_TFS_TX_MASK;
	int ret = 0;

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		mask |= RK730_DI2S_TXCR1_IBM_TX_MASK;
		format = RK730_DI2S_TXCR1_TFS_TX_I2S | RK730_DI2S_TXCR1_IBM_TX_NORMAL;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		mask |= RK730_DI2S_TXCR1_IBM_TX_MASK;
		format = RK730_DI2S_TXCR1_TFS_TX_I2S | RK730_DI2S_TXCR1_IBM_TX_RIGHT;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		mask |= RK730_DI2S_TXCR1_PBM_TX_MASK;
		format = RK730_DI2S_TXCR1_TFS_TX_I2S | RK730_DI2S_TXCR1_IBM_TX_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		mask |= RK730_DI2S_TXCR1_PBM_TX_MASK;
		format = RK730_DI2S_TXCR1_TFS_TX_PCM | RK730_DI2S_TXCR1_PBM_TX_NODELAY;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		mask |= RK730_DI2S_TXCR1_PBM_TX_MASK;
		format = RK730_DI2S_TXCR1_TFS_TX_PCM | RK730_DI2S_TXCR1_PBM_TX_DELAY1;
		break;
	default:
		return -EINVAL;
	}
	snd_soc_component_update_bits(component, RK730_DI2S_RXCR1, mask, format);
	snd_soc_component_update_bits(component, RK730_DI2S_TXCR1, mask, format);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_NF:
		val |= RK730_DI2S_CKM_SCLK_INVERTED;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		val |= RK730_DI2S_CKM_SCLK_NORMAL;
		break;
	default:
		ret = -EINVAL;
	}
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		val |= RK730_DI2S_CKM_MST_SLAVE | RK730_DI2S_CKM_SCLK_DIS;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		val |= RK730_DI2S_CKM_MST_MASTER | RK730_DI2S_CKM_SCLK_EN;
		break;
	default:
		return -EINVAL;
	}
	snd_soc_component_update_bits(component, RK730_DI2S_CKM,
				      RK730_DI2S_CKM_MST_MASK |
				      RK730_DI2S_CKM_SCLK_POL_MASK |
				      RK730_DI2S_CKM_SCLK_EN_MASK,
				      val);
	return ret;
}

static int rk730_dai_mute(struct snd_soc_dai *codec_dai, int mute, int stream)
{
	struct snd_soc_component *component = codec_dai->component;

	dev_dbg(component->dev, "%s %d stream %d\n", __func__, mute, stream);
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mute) {
			snd_soc_component_update_bits(component, RK730_DADC_SEL,
						      RK730_DADC_DAC_MUTE_MASK,
						      RK730_DADC_DAC_MUTE);
		} else {
			snd_soc_component_update_bits(component, RK730_DADC_SEL,
						      RK730_DADC_DAC_MUTE_MASK,
						      RK730_DADC_DAC_UNMUTE);
		}
	} else {
		if (mute) {
			snd_soc_component_update_bits(component, RK730_DADC_SEL,
						      RK730_DADC_ADC_MUTE_MASK,
						      RK730_DADC_ADC_L_MUTE |
						      RK730_DADC_ADC_R_MUTE);
		} else {
			snd_soc_component_update_bits(component, RK730_DADC_SEL,
						      RK730_DADC_ADC_MUTE_MASK,
						      RK730_DADC_ADC_L_UNMUTE |
						      RK730_DADC_ADC_R_UNMUTE);
		}
	}

	return 0;
}

static int rk730_set_bias_level(struct snd_soc_component *component,
				enum snd_soc_bias_level level)
{
	struct rk730_priv *rk730 = snd_soc_component_get_drvdata(component);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/*
		 * SND_SOC_BIAS_PREPARE is called while preparing for a
		 * transition to ON or away from ON. If current bias_level
		 * is SND_SOC_BIAS_ON, then it is preparing for a transition
		 * away from ON. Disable the clock in that case, otherwise
		 * enable it.
		 */
		if (!IS_ERR(rk730->mclk)) {
			if (snd_soc_component_get_bias_level(component) ==
			    SND_SOC_BIAS_ON)
				clk_disable_unprepare(rk730->mclk);
			else
				clk_prepare_enable(rk730->mclk);
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		if (snd_soc_component_get_bias_level(component) == SND_SOC_BIAS_OFF)
			regcache_sync(rk730->regmap);
		break;

	case SND_SOC_BIAS_OFF:
		regcache_mark_dirty(rk730->regmap);
		break;
	}
	return 0;
}

/*
 * Note that this should be called from init rather than from hw_params.
 */
static int rk730_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				 int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = codec_dai->component;
	struct rk730_priv *rk730 = snd_soc_component_get_drvdata(component);

	rk730->sysclk = freq;
	return 0;
}

#define RK730_RATES	SNDRV_PCM_RATE_8000_192000
#define RK730_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops rk730_dai_ops = {
	.set_fmt = rk730_dai_set_fmt,
	.hw_params = rk730_dai_hw_params,
	.mute_stream = rk730_dai_mute,
	.set_sysclk = rk730_set_dai_sysclk,
};

static struct snd_soc_dai_driver rk730_dai = {
	.name = "HiFi",
	.playback = {
		 .stream_name = "HiFi Playback",
		 .channels_min = 1,
		 .channels_max = 2,
		 .rates = RK730_RATES,
		 .formats = RK730_FORMATS,
	},
	.capture = {
		 .stream_name = "HiFi Capture",
		 .channels_min = 1,
		 .channels_max = 2,
		 .rates = RK730_RATES,
		 .formats = RK730_FORMATS,
	},
	.ops = &rk730_dai_ops,
};

static int rk730_reset(struct snd_soc_component *component)
{
	struct rk730_priv *rk730 = snd_soc_component_get_drvdata(component);

	clk_prepare_enable(rk730->mclk);
	udelay(10);
	snd_soc_component_update_bits(component, RK730_DTOP_SRT,
				      RK730_DTOP_SRST_MASK,
				      RK730_DTOP_SRST_EN);
	snd_soc_component_update_bits(component, RK730_DTOP_SRT,
				      RK730_DTOP_SRST_MASK,
				      RK730_DTOP_SRST_DIS);
	/* WA: Initial micbias default, ADC stopped with micbias(>2.5v) */
	snd_soc_component_update_bits(component, RK730_MIC_BIAS,
				      RK730_MIC_BIAS_VOLT_MASK,
				      RK730_MIC_BIAS_VOLT_2_2V);
	/* PF: Use the chop 400kHz for better ADC noise performance */
	snd_soc_component_update_bits(component, RK730_MIC_BOOST_3,
				      RK730_MIC_BOOST_3_MIC_CHOP_MASK,
				      RK730_MIC_BOOST_3_MIC_CHOP(RK730_CHOP_FREQ_400KHZ));
	snd_soc_component_update_bits(component, RK730_ADC_PGA_BLOCK_1,
				      RK730_ADC_PGA_BLOCK_1_PGA_CHOP_MASK,
				      RK730_ADC_PGA_BLOCK_1_PGA_CHOP(RK730_CHOP_FREQ_400KHZ));
	clk_disable_unprepare(rk730->mclk);

	return 0;
}

static int rk730_probe(struct snd_soc_component *component)
{
	struct rk730_priv *rk730 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	regcache_mark_dirty(rk730->regmap);

	/* initialize private data */
	atomic_set(&rk730->mix_mode, RK730_MIX_MODE_1_PATH);

	ret = snd_soc_component_read(component, RK730_HK_TOP_0);
	if (ret < 0) {
		dev_err(component->dev, "Failed to read register: %d\n", ret);
		return ret;
	}

	rk730_reset(component);

	return ret;
}

static const struct snd_soc_component_driver rk730_component_driver = {
	.probe			= rk730_probe,
	.set_bias_level		= rk730_set_bias_level,
	.controls		= rk730_snd_controls,
	.num_controls		= ARRAY_SIZE(rk730_snd_controls),
	.dapm_widgets		= rk730_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(rk730_dapm_widgets),
	.dapm_routes		= rk730_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(rk730_dapm_routes),
	.suspend_bias_off	= 1,
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static const struct reg_default rk730_reg_defaults[] = {
	{0x00, 0x08},
	{0x01, 0xc0},
	{0x02, 0x00},
	{0x03, 0x00},
	{0x04, 0x03},
	{0x05, 0x42},
	{0x06, 0x02},
	{0x07, 0x05},
	{0x08, 0x07},
	{0x09, 0x70},
	{0x0a, 0x01},
	{0x0b, 0x01},
	{0x0c, 0x01},
	{0x0d, 0x01},
	{0x0e, 0x01},
	{0x0f, 0x01},
	{0x10, 0xff},
	{0x11, 0x07},
	{0x12, 0x54},
	{0x13, 0x04},
	{0x14, 0x23},
	{0x15, 0x35},
	{0x16, 0x67},
	{0x17, 0x1e},
	{0x18, 0xc0},
	{0x19, 0x13},
	{0x1a, 0x04},
	{0x1b, 0x20},
	{0x1c, 0x00},
	{0x1d, 0x00},
	{0x1e, 0x90},
	{0x1f, 0x11},
	{0x20, 0x09},
	{0x21, 0xac},
	{0x22, 0x80},
	{0x23, 0x00},
	{0x24, 0xac},
	{0x25, 0x80},
	{0x26, 0x00},
	{0x27, 0x00},
	{0x28, 0x20},
	{0x29, 0x00},
	{0x40, 0x00},
	{0x41, 0x00},
	{0x42, 0x00},
	{0x43, 0x88},
	{0x44, 0x01},
	{0x45, 0xff},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x48, 0x00},
	{0x49, 0x00},
	{0x4a, 0x00},
	{0x4b, 0x00},
	{0x4c, 0xff},
	{0x4d, 0xff},
	{0x4e, 0x1e},
	{0x4f, 0xf4},
	{0x50, 0x00},
	{0x51, 0x00},
	{0x52, 0x1f},
	{0x53, 0x1f},
	{0x54, 0x02},
	{0x55, 0x00},
	{0x56, 0xc0},
	{0x57, 0x06},
	{0x58, 0x60},
	{0x59, 0x07},
	{0x5a, 0x03},
	{0x5b, 0x00},
	{0x5c, 0x03},
	{0x5d, 0x66},
	{0x5e, 0x9a},
	{0x5f, 0xcc},
	{0x60, 0x1f},
	{0x61, 0xcc},
	{0x62, 0x0e},
	{0x63, 0x08},
	{0x64, 0x06},
	{0x65, 0x02},
	{0x66, 0x05},
	{0x67, 0x01},
	{0x68, 0x00},
	{0x69, 0x01},
	{0x6a, 0x00},
	{0x6b, 0x00},
	{0x6c, 0x00},
	{0x6d, 0x17},
	{0x6e, 0x00},
	{0x6f, 0x00},
	{0x70, 0x17},
	{0x71, 0x00},
};

static const struct regmap_config rk730_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RK730_DI2S_TXCR3_TXCMD,
	.reg_defaults = rk730_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(rk730_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

static int rk730_i2c_probe(struct i2c_client *i2c,
			   const struct i2c_device_id *id)
{
	struct rk730_priv *rk730;
	int ret;

	rk730 = devm_kzalloc(&i2c->dev, sizeof(struct rk730_priv), GFP_KERNEL);
	if (!rk730)
		return -ENOMEM;

	rk730->regmap = devm_regmap_init_i2c(i2c, &rk730_regmap);
	if (IS_ERR(rk730->regmap))
		return PTR_ERR(rk730->regmap);

	rk730->mclk = devm_clk_get(&i2c->dev, "mclk");
	if (IS_ERR(rk730->mclk))
		return PTR_ERR(rk730->mclk);

	rk730->fixed_mclk_fs =
		device_property_read_bool(&i2c->dev, "rockchip,mclk-fs-fixed");

	i2c_set_clientdata(i2c, rk730);

	ret = devm_snd_soc_register_component(&i2c->dev,
			&rk730_component_driver, &rk730_dai, 1);
	return ret;
}

static const struct i2c_device_id rk730_i2c_id[] = {
	{ "rk730", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rk730_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id rk730_of_match[] = {
	{ .compatible = "rockchip,rk730" },
	{ }
};
MODULE_DEVICE_TABLE(of, rk730_of_match);
#endif

static struct i2c_driver rk730_i2c_driver = {
	.driver = {
		.name = "rk730",
		.of_match_table = of_match_ptr(rk730_of_match),
	},
	.probe  = rk730_i2c_probe,
	.id_table = rk730_i2c_id,
};

module_i2c_driver(rk730_i2c_driver);

MODULE_DESCRIPTION("ASoC RK730 driver");
MODULE_AUTHOR("Sugar Zhang <sugar.zhang@rock-chips.com>");
MODULE_LICENSE("GPL");
