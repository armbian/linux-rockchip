/*
 * es8389.c  --  ES8389/ES8312 ALSA SoC Audio Codec
 *
 * Copyright (C) 2018 Everest Semiconductor Co., Ltd
 *
 * Authors:  David Yang(yangxiaohua@everest-semi.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>

#include "es8389.h"


/* codec private data */

struct	es8389_private {
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct clk *mclk;
	unsigned int sysclk;
	struct delayed_work state_work;
	int mastermode;
	struct mutex lock;

	int dmic;
	unsigned int mclk_src;
	enum snd_soc_bias_level bias_level;
};

static struct es8389_private *es8389_data;
static struct snd_soc_component *es8389_codec;

static bool es8389_volatile_register(struct device *dev,
			unsigned int reg)
{
	if ((reg  <= 0xff))
		return true;
	else
		return false;
}

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -9550, 50, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -9550, 50, 0);
static const DECLARE_TLV_DB_SCALE(pga_vol_tlv, 0, 300, 0);
static const DECLARE_TLV_DB_SCALE(mix_vol_tlv, -9500, 100, 0);
static const DECLARE_TLV_DB_SCALE(alc_target_tlv, -3200, 200, 0);
static const DECLARE_TLV_DB_SCALE(alc_max_level, -3200, 200, 0);

static const char *const alc[] = {
	"ALC OFF",
	"ADCR ALC ON",
	"ADCL ALC ON",
	"ADCL & ADCL ALC ON",
};

static const char *const ramprate[] = {
	"0.125db/1  LRCK",
	"0.125db/4  LRCK",
	"0.125db/8  LRCK",
	"0.125db/16  LRCK",
	"0.125db/32  LRCK",
	"0.125db/64  LRCK",
	"0.125db/128  LRCK",
	"0.125db/256  LRCK",
	"0.125db/512  LRCK",
	"0.125db/1024  LRCK",
	"0.125db/2048  LRCK",
	"0.125db/4096  LRCK",
	"0.125db/8192  LRCK",
	"0.125db/16384  LRCK",
	"0.125db/32768  LRCK",
	"0.125db/65536  LRCK",
};

static const char *const winsize[] = {
	"2 LRCK",
	"4  LRCK",
	"8  LRCK",
	"16  LRCK",
	"32  LRCK",
	"64  LRCK",
	"128  LRCK",
	"256  LRCK",
	"512  LRCK",
	"1024  LRCK",
	"2048  LRCK",
	"4096  LRCK",
	"8192  LRCK",
	"16384  LRCK",
	"32768  LRCK",
	"65536  LRCK",
};

static const struct soc_enum alc_enable =
	SOC_ENUM_SINGLE(ES8389_ALC_ON_REG2B, 5, 4, alc);
static const struct soc_enum alc_ramprate =
	SOC_ENUM_SINGLE(ES8389_ALC_CTL_REG29, 4, 16, ramprate);
static const struct soc_enum alc_winsize =
	SOC_ENUM_SINGLE(ES8389_ALC_CTL_REG29, 0, 16, winsize);

static const char *es8389_outl_mux_txt[] = {
	"normal",
	"DAC2 channel to DAC1 channel",
};

static const char *es8389_outr_mux_txt[] = {
	"normal",
	"DAC1 channel to DAC2 channel",
};

static const char *es8389_dmic_mux_txt[] = {
	"AMIC",
	"DMIC",
};

static const unsigned int es8326_outl_mux_values[] = {
	0, 1
};

static const unsigned int es8326_outr_mux_values[] = {
	0, 1
};

static const unsigned int es8326_dmic_mux_values[] = {
	0, 3
};

static const struct soc_enum es8389_outl_mux_enum =
	SOC_VALUE_ENUM_SINGLE(ES8389_DAC_MIX_REG44, 5, 1,
			ARRAY_SIZE(es8389_outl_mux_txt), es8389_outl_mux_txt,
			es8326_outl_mux_values);

static const struct snd_kcontrol_new es8389_outl_mux_controls =
	SOC_DAPM_ENUM("OUTL MUX", es8389_outl_mux_enum);

static const struct soc_enum es8389_outr_mux_enum =
	SOC_VALUE_ENUM_SINGLE(ES8389_DAC_MIX_REG44, 4, 1,
			ARRAY_SIZE(es8389_outr_mux_txt), es8389_outr_mux_txt,
			es8326_outr_mux_values);

static const struct snd_kcontrol_new es8389_outr_mux_controls =
	SOC_DAPM_ENUM("OUTR MUX", es8389_outr_mux_enum);

static const struct soc_enum es8389_dmic_mux_enum =
	SOC_VALUE_ENUM_SINGLE(ES8389_DMIC_EN_REG6D, 6, 3,
			ARRAY_SIZE(es8389_dmic_mux_txt), es8389_dmic_mux_txt,
			es8326_dmic_mux_values);

static const struct snd_kcontrol_new es8389_dmic_mux_controls =
	SOC_DAPM_ENUM("ADC MUX", es8389_dmic_mux_enum);


static const struct snd_kcontrol_new es8389_left_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACR DACL Mixer", ES8389_DAC_MIX_REG44, 3, 1, 0),
};

static const struct snd_kcontrol_new es8389_right_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACL DACR Mixer", ES8389_DAC_MIX_REG44, 2, 1, 0),
};


static const struct snd_kcontrol_new es8389_adc_mixer_controls[] = {
	SOC_DAPM_SINGLE("DACL ADCL Mixer", ES8389_ADC_RESET_REG31, 7, 1, 0),
	SOC_DAPM_SINGLE("DACR ADCR Mixer", ES8389_ADC_RESET_REG31, 6, 1, 0),
};

static const struct snd_kcontrol_new es8389_snd_controls[] = {
	SOC_SINGLE_TLV("ADCL Capture Volume", ES8389_ADCL_VOL_REG27, 0, 0xFF, 0, adc_vol_tlv),
	SOC_SINGLE_TLV("ADCR Capture Volume", ES8389_ADCR_VOL_REG28, 0, 0xFF, 0, adc_vol_tlv),
	SOC_SINGLE_TLV("ADCL PGA Volume", ES8389_MIC1_GAIN_REG72, 0, 0x0E, 0, pga_vol_tlv),
	SOC_SINGLE_TLV("ADCR PGA Volume", ES8389_MIC2_GAIN_REG73, 0, 0x0E, 0, pga_vol_tlv),

	SOC_ENUM("ALC Capture Switch", alc_enable),
	SOC_SINGLE_TLV("ALC Capture Target Level", ES8389_ALC_TARGET_REG2C,
			0, 0x0f, 0, alc_target_tlv),
	SOC_SINGLE_TLV("ALC Capture Max Gain", ES8389_ALC_GAIN_REG2D,
			0, 0x0f, 0, alc_max_level),
	SOC_ENUM("ADC Ramp Rate", alc_ramprate),
	SOC_ENUM("ALC Capture Winsize", alc_winsize),
	SOC_DOUBLE("ADC OSR Volume ON", ES8389_ADC_MUTE_REG2F, 6, 7, 1, 0),
	SOC_SINGLE_TLV("ADC OSR Volume", ES8389_OSR_VOL_REG26, 0, 0xFF, 0, adc_vol_tlv),

	SOC_SINGLE_TLV("DACL Playback Volume", ES8389_DACL_VOL_REG46, 0, 0xFF, 0, dac_vol_tlv),
	SOC_SINGLE_TLV("DACR Playback Volume", ES8389_DACR_VOL_REG47, 0, 0xFF, 0, dac_vol_tlv),
	SOC_SINGLE_TLV("ADC2DAC Mixer Volume", ES8389_MIX_VOL_REG48, 0, 0x7F, 0, mix_vol_tlv),

	SOC_SINGLE("Mix ADCR And DACR to DACR", ES8389_DAC_MIX_REG44, 0, 1, 0),
	SOC_SINGLE("Mix ADCL And DACL to DACL", ES8389_DAC_MIX_REG44, 1, 1, 0),
};

static const struct snd_soc_dapm_widget es8389_dapm_widgets[] = {
	/*Input Side*/
	SND_SOC_DAPM_INPUT("INPUT1"),
	SND_SOC_DAPM_INPUT("INPUT2"),
	SND_SOC_DAPM_INPUT("DMIC"),
	SND_SOC_DAPM_PGA("PGAL", SND_SOC_NOPM, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGAR", SND_SOC_NOPM, 4, 0, NULL, 0),

	/*ADCs*/
	SND_SOC_DAPM_ADC("ADCL", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADCR", NULL, SND_SOC_NOPM, 0, 0),

	/* Audio Interface */
	SND_SOC_DAPM_AIF_OUT("I2S OUT", "I2S Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("I2S IN", "I2S Playback", 0, SND_SOC_NOPM, 0, 0),

	/*DACs*/
	SND_SOC_DAPM_DAC("DACL", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DACR", NULL, SND_SOC_NOPM, 0, 0),

	/*Output Side*/
	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),

	/* Digital Interface */
	SND_SOC_DAPM_PGA("IF DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF DACL1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF DACR1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF DACL2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF DACR2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF DACL3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF DACR3", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Digital Interface Select */
	SND_SOC_DAPM_MIXER("IF DACL Mixer", SND_SOC_NOPM, 0, 0,
			   &es8389_left_mixer_controls[0],
			   ARRAY_SIZE(es8389_left_mixer_controls)),
	SND_SOC_DAPM_MIXER("IF DACR Mixer", SND_SOC_NOPM, 0, 0,
			   &es8389_right_mixer_controls[0],
			   ARRAY_SIZE(es8389_right_mixer_controls)),

	SND_SOC_DAPM_MIXER("ADC Mixer", SND_SOC_NOPM, 0, 0,
			   &es8389_adc_mixer_controls[0],
			   ARRAY_SIZE(es8389_adc_mixer_controls)),
	SND_SOC_DAPM_MUX("ADC MUX", SND_SOC_NOPM, 0, 0, &es8389_dmic_mux_controls),

	SND_SOC_DAPM_MUX("OUTL MUX", SND_SOC_NOPM, 0, 0, &es8389_outl_mux_controls),
	SND_SOC_DAPM_MUX("OUTR MUX", SND_SOC_NOPM, 0, 0, &es8389_outr_mux_controls),
};


static const struct snd_soc_dapm_route es8389_dapm_routes[] = {
	{"PGAL", NULL, "INPUT1"},
	{"PGAR", NULL, "INPUT2"},

	{"ADCL", NULL, "PGAL"},
	{"ADCR", NULL, "PGAR"},

	{"ADC Mixer", "DACL ADCL Mixer", "DACL"},
	{"ADC Mixer", "DACR ADCR Mixer", "DACR"},
	{"ADC Mixer", NULL, "ADCL"},
	{"ADC Mixer", NULL, "ADCR"},

	{"ADC MUX", "AMIC", "ADC Mixer"},
	{"ADC MUX", "DMIC", "DMIC"},

	{"I2S OUT", NULL, "ADC MUX"},

	{"DACL", NULL, "I2S IN"},
	{"DACR", NULL, "I2S IN"},

	{"IF DACL1", NULL, "DACL"},
	{"IF DACR1", NULL, "DACR"},
	{"IF DACL2", NULL, "DACL"},
	{"IF DACR2", NULL, "DACR"},
	{"IF DACL3", NULL, "DACL"},
	{"IF DACR3", NULL, "DACR"},


	{"IF DACL Mixer", NULL, "IF DACL2"},
	{"IF DACL Mixer", "DACR DACL Mixer", "IF DACR1"},
	{"IF DACR Mixer", NULL, "IF DACR2"},
	{"IF DACR Mixer", "DACL DACR Mixer", "IF DACL1"},

	{"OUTL MUX", "normal", "IF DACL Mixer"},
	{"OUTL MUX", "DAC2 channel to DAC1 channel", "IF DACR Mixer"},
	{"OUTR MUX", "normal", "IF DACR Mixer"},
	{"OUTR MUX", "DAC1 channel to DAC2 channel", "IF DACL Mixer"},

	{"HPOL", NULL, "OUTL MUX"},
	{"HPOR", NULL, "OUTR MUX"},

};

struct _coeff_div {
	u16 fs;
	u32 mclk;
	u32 rate;
	u8 Reg0x04;
	u8 Reg0x05;
	u8 Reg0x06;
	u8 Reg0x07;
	u8 Reg0x08;
	u8 Reg0x09;
	u8 Reg0x0A;
	u8 Reg0x11;
	u8 Reg0x21;
	u8 Reg0x22;
	u8 Reg0x41;
	u8 Reg0x42;
	u8 Reg0x43;
	u8 Reg0x44;
};


/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div  coeff_div[] = {
	{32 ,256000 ,8000 , 0x00, 0x57, 0x84, 0xD0, 0x03, 0xC1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{48 ,384000 ,8000 , 0x02, 0x5F, 0x04, 0xC0, 0x03, 0xC1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{64 ,512000 ,8000 , 0x00, 0x4D, 0x24, 0xC0, 0x03, 0xD1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{128 ,1024000 ,8000 , 0x00, 0x45, 0x04, 0xD0, 0x03, 0xC1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{192 ,1536000 ,8000 , 0x02, 0x4D, 0x24, 0xC0, 0x03, 0xD1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{256 ,2048000 ,8000 , 0x01, 0x45, 0x04, 0xD0, 0x03, 0xC1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{384 ,3072000 ,8000 , 0x02, 0x45, 0x04, 0xD0, 0x03, 0xC1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{512 ,4096000 ,8000 , 0x00, 0x41, 0x04, 0xE0, 0x00, 0xD1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{600 ,4800000 ,8000 , 0x05, 0x65, 0x25, 0xF9, 0x00, 0xD1, 0x90, 0x00, 0x18, 0xC7, 0x8F, 0xC7, 0x01, 0x80},
	{768 ,6144000 ,8000 , 0x05, 0x45, 0x04, 0xD0, 0x03, 0xC1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{1024 ,8192000 ,8000 , 0x01, 0x41, 0x06, 0xE0, 0x00, 0xD1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{1500 ,12000000 ,8000 , 0x0E, 0x65, 0x25, 0xF9, 0x00, 0xD1, 0x90, 0x00, 0x18, 0xC7, 0x8F, 0xC7, 0x01, 0x80},
	{1536 ,12288000 ,8000 , 0x02, 0x41, 0x04, 0xE0, 0x00, 0xD1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{1625 ,13000000 ,8000 , 0x40, 0x6E, 0x05, 0xC8, 0x01, 0xC2, 0x90, 0x00, 0x18, 0x95, 0x63, 0x95, 0x00, 0x80},
	{2048 ,16384000 ,8000 , 0x03, 0x44, 0x01, 0xC0, 0x00, 0xD2, 0x80, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{2304 ,18432000 ,8000 , 0x11, 0x45, 0x25, 0xF0, 0x00, 0xD1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{2400 ,19200000 ,8000 , 0x4A, 0x57, 0x04, 0xC0, 0x03, 0xC1, 0xB0, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{3072 ,24576000 ,8000 , 0x05, 0x44, 0x01, 0xC0, 0x00, 0xD2, 0x80, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x01, 0x80},
	{3250 ,26000000 ,8000 , 0x40, 0x7E, 0x01, 0xC9, 0x00, 0xC2, 0x80, 0x00, 0x18, 0x95, 0xC7, 0x95, 0x00, 0x80},
	{32 ,512000 ,16000 , 0x00, 0x55, 0x84, 0xD0, 0x01, 0xC1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{48 ,768000 ,16000 , 0x02, 0x57, 0x04, 0xC0, 0x01, 0xC1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{64 ,1024000 ,16000 , 0x00, 0x45, 0x24, 0xC0, 0x01, 0xD1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{128 ,2048000 ,16000 , 0x00, 0x51, 0x04, 0xD0, 0x01, 0xC1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{192 ,3072000 ,16000 , 0x02, 0x65, 0x25, 0xE0, 0x00, 0xE1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{256 ,4096000 ,16000 , 0x00, 0x41, 0x04, 0xC0, 0x01, 0xD1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{300 ,4800000 ,16000 , 0x02, 0x66, 0x01, 0xD9, 0x00, 0xC2, 0x80, 0x00, 0x18, 0x95, 0xC7, 0x95, 0x00, 0x80},
	{384 ,6144000 ,16000 , 0x02, 0x51, 0x04, 0xD0, 0x01, 0xC1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{512 ,8192000 ,16000 , 0x01, 0x41, 0x04, 0xC0, 0x01, 0xD1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{750 ,12000000 ,16000 , 0x0E, 0x7E, 0x01, 0xC9, 0x00, 0xC2, 0x80, 0x00, 0x18, 0x95, 0xC7, 0x95, 0x00, 0x80},
	{768 ,12288000 ,16000 , 0x02, 0x41, 0x04, 0xC0, 0x01, 0xD1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{1024 ,16384000 ,16000 , 0x03, 0x41, 0x04, 0xC0, 0x01, 0xD1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{1152 ,18432000 ,16000 , 0x08, 0x51, 0x04, 0xD0, 0x01, 0xC1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{1200 ,19200000 ,16000 , 0x4A, 0x57, 0x04, 0xC0, 0x01, 0xC1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{1536 ,24576000 ,16000 , 0x05, 0x41, 0x04, 0xC0, 0x01, 0xD1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{1625 ,26000000 ,16000 , 0x40, 0x6E, 0x05, 0xC8, 0x01, 0xC2, 0x90, 0x00, 0x18, 0x95, 0x63, 0x95, 0x00, 0x80},
	{800 ,19200000 ,24000 , 0x18, 0x55, 0x04, 0xC0, 0x01, 0xC1, 0x90, 0x00, 0x1F, 0x7F, 0xFF, 0x7F, 0x00, 0x80},
	{600 ,19200000 ,32000 , 0x4A, 0x57, 0x04, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{32 ,1411200 ,44100 , 0x00, 0x45, 0xA4, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{64 ,2822400 ,44100 , 0x00, 0x51, 0x00, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{128 ,5644800 ,44100 , 0x00, 0x41, 0x04, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{256 ,11289600 ,44100 , 0x01, 0x41, 0x04, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{512 ,22579200 ,44100 , 0x03, 0x41, 0x04, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{32 ,1536000 ,48000 , 0x00, 0x45, 0xA4, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{48 ,2304000 ,48000 , 0x02, 0x55, 0x04, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{64 ,3072000 ,48000 , 0x00, 0x51, 0x04, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{100 ,4800000 ,48000 , 0x18, 0x5F, 0x04, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{125 ,6000000 ,48000 , 0x04, 0x6E, 0x05, 0xC8, 0x10, 0xC2, 0x80, 0x01, 0x18, 0x95, 0x63, 0x95, 0x00, 0x00},
	{128 ,6144000 ,48000 , 0x00, 0x41, 0x04, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{200 ,9600000 ,48000 , 0x18, 0x57, 0x04, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{250 ,12000000 ,48000 , 0x04, 0x76, 0x01, 0xD9, 0x01, 0xC1, 0x80, 0x00, 0x98, 0x95, 0x63, 0x95, 0x00, 0x00},
	{256 ,12288000 ,48000 ,	0x01, 0x01, 0x04, 0xD0, 0x00, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{384 ,18432000 ,48000 , 0x02, 0x41, 0x04, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{400 ,19200000 ,48000 , 0x18, 0x55, 0x04, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{500 ,24000000 ,48000 , 0x04, 0x46, 0x01, 0xD8, 0x10, 0xD2, 0x80, 0x00, 0x18, 0x95, 0x63, 0x95, 0x00, 0x00},
	{512 ,24576000 ,48000 , 0x03, 0x41, 0x04, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{800 ,38400000 ,48000 , 0x18, 0x45, 0x04, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x1F, 0x7F, 0x7F, 0x7F, 0x00, 0x00},
	{128 ,11289600 ,88200 , 0x00, 0x50, 0x00, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x9F, 0x7F, 0x7F, 0x7F, 0x80, 0x00},
	{64 ,6144000 ,96000 , 0x00, 0x41, 0x00, 0xD0, 0x10, 0xD1, 0x80, 0x00, 0x9F, 0x7F, 0x7F, 0x7F, 0x80, 0x00},
	{256 ,24576000 ,96000 , 0x00, 0x40, 0x00, 0xC0, 0x10, 0xC1, 0x80, 0x00, 0x9F, 0x7F, 0x7F, 0x7F, 0x80, 0x00},
	{128 ,24576000 ,192000 , 0x00, 0x50, 0x00, 0xC0, 0x18, 0xC1, 0x81, 0x00, 0x8F, 0x7F, 0x3F, 0x7F, 0x80, 0x00},
};
static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if ( coeff_div[i].rate == rate &&  coeff_div[i].mclk == mclk)
			return i;
	}
	return -EINVAL;
}

/*
 * if PLL not be used, use internal clk1 for mclk,otherwise, use internal clk2 for PLL source.
 */
static int es8389_set_dai_sysclk(struct snd_soc_dai *dai,
			int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *codec = dai->component;
	struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);

	printk("Enter into %s()\n", __func__);
	switch (freq) {
	case 11289600:
	case 22579200:
	case 5644800:
	case 2822400:
	case 1411200:
	case 12288000:
	case 16384000:
	case 18432000:
	case 24576000:
	case 8192000:
	case 6144000:
	case 4096000:
	case 2048000:
	case 3072000:
	case 1536000:
	case 1024000:
	case 12000000:
		es8389->sysclk = freq;
		return 0;
	}

	return -EINVAL;
}

static int es8389_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *codec = codec_dai->component;
	struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);
	u8 state = 0;

	dev_dbg(codec->dev, "Enter into %s()\n", __func__);

	switch(fmt & SND_SOC_DAIFMT_MASTER_MASK)
	{
		case SND_SOC_DAIFMT_CBM_CFM:
			regmap_update_bits(es8389->regmap, ES8389_MASTER_MODE_REG01,
					ES8389_MASTER_MODE_EN, ES8389_MASTER_MODE_EN);
			break;
		case SND_SOC_DAIFMT_CBS_CFS:
			break;
		default:
			return -EINVAL;
	}

	switch(fmt & SND_SOC_DAIFMT_FORMAT_MASK)
	{
		case SND_SOC_DAIFMT_I2S:
			state |= ES8389_DAIFMT_I2S;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			dev_err(codec->dev, "Codec driver does not support right justified\n");
			return -EINVAL;
		case SND_SOC_DAIFMT_LEFT_J:
			state |= ES8389_DAIFMT_LEFT_J;
			break;
		case SND_SOC_DAIFMT_DSP_A:
			state |= ES8389_DAIFMT_DSP_A;
			break;
		case SND_SOC_DAIFMT_DSP_B:
			state |= ES8389_DAIFMT_DSP_B;
			break;
		default:
			break;
	}
		regmap_update_bits(es8389->regmap, ES8389_ADC_REG20, ES8389_DAIFMT_MASK, state);
		regmap_update_bits(es8389->regmap, ES8389_DAC_REG40, ES8389_DAIFMT_MASK, state);

		/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_IF:
			break;
		case SND_SOC_DAIFMT_IB_NF:
			break;
		case SND_SOC_DAIFMT_NB_IF:
			break;
		default:
			break;
		}

	return 0;
}

static int es8389_pcm_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);
	int coeff;
	u8 state = 0;

	printk("Enter into %s()\n", __func__);

	switch (params_format(params)){
		case SNDRV_PCM_FORMAT_S16_LE:
			state |= ES8389_S16_LE;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			state |= ES8389_S20_3_LE;
			break;
		case SNDRV_PCM_FORMAT_S18_3LE:
			state |= ES8389_S18_LE;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			state |= ES8389_S24_LE;
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			state |= ES8389_S32_LE;
			break;
		default:
			return -EINVAL;
	}

	regmap_update_bits(es8389->regmap, ES8389_ADC_REG20, ES8389_DATA_LEN_MASK, state);
	regmap_update_bits(es8389->regmap, ES8389_DAC_REG40, ES8389_DATA_LEN_MASK, state);

	coeff = get_coeff(es8389->sysclk, params_rate(params));
	if(coeff >= 0)
	{
		regmap_write(es8389->regmap, ES8389_CLK_DIV1_REG04, coeff_div[coeff].Reg0x04);
		regmap_write(es8389->regmap, ES8389_CLK_MUL_REG05, coeff_div[coeff].Reg0x05);
		regmap_write(es8389->regmap, ES8389_CLK_MUX1_REG06, coeff_div[coeff].Reg0x06);
		regmap_write(es8389->regmap, ES8389_CLK_MUX2_REG07, coeff_div[coeff].Reg0x07);
		regmap_write(es8389->regmap, ES8389_CLK_CTL1_REG08, coeff_div[coeff].Reg0x08);
		regmap_write(es8389->regmap, ES8389_CLK_CTL2_REG09, coeff_div[coeff].Reg0x09);
		regmap_write(es8389->regmap, ES8389_CLK_CTL3_REG0A, coeff_div[coeff].Reg0x0A);
		regmap_write(es8389->regmap, ES8389_CLK_DIV2_REG11, coeff_div[coeff].Reg0x11);
		regmap_write(es8389->regmap, ES8389_ADC_REG21, coeff_div[coeff].Reg0x21);
		regmap_write(es8389->regmap, ES8389_ADC_REG22, coeff_div[coeff].Reg0x22);
		regmap_write(es8389->regmap, ES8389_DAC_REG41, coeff_div[coeff].Reg0x41);
		regmap_write(es8389->regmap, ES8389_DAC_REG42, coeff_div[coeff].Reg0x42);
		regmap_write(es8389->regmap, ES8389_DAC_REG43, coeff_div[coeff].Reg0x43);
		regmap_update_bits(es8389->regmap, ES8389_DAC_MIX_REG44,
						0xC0, coeff_div[coeff].Reg0x44);
	}

	return 0;
}

static int es8389_set_bias_level(struct snd_soc_component *codec,
			enum snd_soc_bias_level level)
{
	int ret;
	struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);
	printk("Enter into %s(), level = %d\n", __func__, level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		printk("%s on\n",__func__);
		ret = clk_prepare_enable(es8389->mclk);
		if (ret)
			return ret;
		regmap_write(es8389->regmap, ES8389_HPSW_REG69, 0x23);
		regmap_write(es8389->regmap, ES8389_ANA_CTL1_REG61, 0xF9);
		regmap_write(es8389->regmap, ES8389_ADC_EN_REG64, 0x8F);
		regmap_write(es8389->regmap, ES8389_CSM_JUMP_REG10, 0xE4);
		regmap_write(es8389->regmap, ES8389_RESET_REG00, 0x01);
		regmap_write(es8389->regmap, ES8389_CLK_OFF1_REG03, 0xC3);
		usleep_range(45000,50000);
		regmap_write(es8389->regmap, ES8389_DAC_RESET_REG4D, 0X00);
		break;
	case SND_SOC_BIAS_PREPARE:
		printk("%s prepare\n",__func__);
		break;
	case SND_SOC_BIAS_STANDBY:
		printk("%s standby\n",__func__);
		regmap_write(es8389->regmap, ES8389_CSM_JUMP_REG10, 0xD4);
		usleep_range(70000,72000); //20MS
		regmap_write(es8389->regmap, ES8389_ANA_CTL1_REG61, 0x59);
		regmap_write(es8389->regmap, ES8389_ADC_EN_REG64, 0x00);
		regmap_write(es8389->regmap, ES8389_CLK_OFF1_REG03, 0x00);
		regmap_write(es8389->regmap, ES8389_RESET_REG00, 0x7E);
		break;
	case SND_SOC_BIAS_OFF:
		printk("%s off\n",__func__);
		clk_disable_unprepare(es8389->mclk);
		break;
	}
	return 0;
}



static int es8389_mute(struct snd_soc_dai *dai, int mute, int direction)
{
	struct snd_soc_component *codec = dai->component;
	struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);
	printk("Enter into %s(), mute = %d\n", __func__, mute);

	if(mute) {
		if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(es8389->regmap, ES8389_DAC_REG40,
						0x03, 0x03);
		} else {
			regmap_update_bits(es8389->regmap, ES8389_ADC_REG20,
						0x03, 0x03);
		}
	} else {
		queue_delayed_work(system_wq, &es8389->state_work,
				   msecs_to_jiffies(10));
		if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(es8389->regmap, ES8389_DAC_REG40,
						0x03, 0x00);
		} else {
			regmap_update_bits(es8389->regmap, ES8389_ADC_REG20,
						0x03, 0x00);
		}
	}

	return 0;
}

#define es8389_RATES SNDRV_PCM_RATE_8000_96000

#define es8389_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops es8389_ops = {
	.hw_params = es8389_pcm_hw_params,
	.set_fmt = es8389_set_dai_fmt,
	.set_sysclk = es8389_set_dai_sysclk,
	.mute_stream = es8389_mute,
};

static struct snd_soc_dai_driver es8389_dai = {
	.name = "ES8389 HiFi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es8389_RATES,
		.formats = es8389_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = es8389_RATES,
		.formats = es8389_FORMATS,
	},
	.ops = &es8389_ops,
	.symmetric_rate = 1
};


static void es8389_state_delay_work(struct work_struct *work)
{
	struct es8389_private *es8389 =
		container_of(work, struct es8389_private, state_work.work);
	unsigned int state;

	mutex_lock(&es8389->lock);
	regmap_read(es8389->regmap, ES8389_CSM_STATE1_REGF1, &state);
	state &= 0x1F;
	if (state == ES8389_STATE_STANDBY){
		printk("Enter into %s()\n", __func__);
		regmap_write(es8389->regmap, ES8389_HPSW_REG69, 0x23);
		regmap_write(es8389->regmap, ES8389_ANA_CTL1_REG61, 0xF9);
		regmap_write(es8389->regmap, ES8389_ADC_EN_REG64, 0x8F);
		regmap_write(es8389->regmap, ES8389_CSM_JUMP_REG10, 0xE4);
		regmap_write(es8389->regmap, ES8389_RESET_REG00, 0x01);
		regmap_write(es8389->regmap, ES8389_CLK_OFF1_REG03, 0xC3);
		//usleep_range(45000,50000);
		regmap_write(es8389->regmap, ES8389_DAC_RESET_REG4D, 0X00);
	}
	mutex_unlock(&es8389->lock);
}

static int es8389_suspend(struct snd_soc_component *codec)
{
	//struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);
	printk("Enter into %s()\n", __func__);

	es8389_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int es8389_resume(struct snd_soc_component *codec)
{
	//struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);
	printk("Enter into %s()\n", __func__);

	es8389_set_bias_level(codec, SND_SOC_BIAS_ON);

	return 0;
}

static int es8389_probe(struct snd_soc_component *codec)
{
	int ret = 0;
	struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);
	printk("Enter into %s()\n", __func__);

	es8389_codec = codec;
	#if 1
	es8389->mclk = devm_clk_get(codec->dev, "mclk");
	if (IS_ERR(es8389->mclk)) {
		dev_err(codec->dev, "%s,unable to get mclk\n", __func__);
		return PTR_ERR(es8389->mclk);
	}
	if (!es8389->mclk)
		dev_err(codec->dev, "%s, assuming static mclk\n", __func__);

	ret = clk_prepare_enable(es8389->mclk);
	if (ret) {
		dev_err(codec->dev, "%s, unable to enable mclk\n", __func__);
		return ret;
	}
#endif

	regmap_write(es8389->regmap, ES8389_ISO_CTL_REGF3, 0x00);
	regmap_write(es8389->regmap, ES8389_RESET_REG00, 0x7E);
	regmap_write(es8389->regmap, ES8389_ISO_CTL_REGF3, 0x38);
	regmap_write(es8389->regmap, ES8389_DAC_INV_REG45, 0x03);
	regmap_write(es8389->regmap, ES8389_VMID_REG60, 0x2A);
	regmap_write(es8389->regmap, ES8389_ANA_CTL1_REG61, 0xC9);
	regmap_write(es8389->regmap, ES8389_ANA_VSEL_REG62, 0x7F);
	regmap_write(es8389->regmap, ES8389_ANA_CTL2_REG63, 0x06);
	regmap_write(es8389->regmap, ES8389_LOW_POWER1_REG6B, 0x00);
	if (es8389->dmic)
		regmap_write(es8389->regmap, ES8389_DMIC_EN_REG6D, 0xE8);
	else
		regmap_write(es8389->regmap, ES8389_DMIC_EN_REG6D, 0x28);
	regmap_write(es8389->regmap, ES8389_MIC1_GAIN_REG72, 0x10);
	regmap_write(es8389->regmap, ES8389_MIC2_GAIN_REG73, 0x10);
	regmap_write(es8389->regmap, ES8389_CSM_JUMP_REG10, 0xC4);
	regmap_write(es8389->regmap, ES8389_MASTER_MODE_REG01, 0x08);
	regmap_write(es8389->regmap, ES8389_CSM_STATE1_REGF1, 0xC0);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG12, 0x01);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG13, 0x01);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG14, 0x01);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG15, 0x01);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG16, 0x3F);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG17, 0xF9);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG18, 0x09);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG19, 0x01);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG1A, 0x01);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG1B, 0x3F);
	regmap_write(es8389->regmap, ES8389_SYSTEM_REG1C, 0x11);
	regmap_write(es8389->regmap, ES8389_CHIP_MISC_REGF0, 0x11);
	regmap_write(es8389->regmap, ES8389_MASTER_CLK_REG02, 0x00);
	regmap_write(es8389->regmap, ES8389_CLK_DIV1_REG04, 0x00);
	regmap_write(es8389->regmap, ES8389_CLK_MUL_REG05, 0x10);
	regmap_write(es8389->regmap, ES8389_CLK_MUX1_REG06, 0x00);
	regmap_write(es8389->regmap, ES8389_CLK_MUX2_REG07, 0xC0);
	regmap_write(es8389->regmap, ES8389_CLK_CTL1_REG08, 0x00);
	regmap_write(es8389->regmap, ES8389_CLK_CTL2_REG09, 0xC0);
	regmap_write(es8389->regmap, ES8389_CLK_CTL3_REG0A, 0x80);
	regmap_write(es8389->regmap, ES8389_SCLK_DIV_REG0B, 0x04);
	regmap_write(es8389->regmap, ES8389_LRCK_DIV1_REG0C, 0x01);
	regmap_write(es8389->regmap, ES8389_LRCK_DIV2_REG0D, 0x00);
	regmap_write(es8389->regmap, ES8389_OSC_CLK_REG0F, 0x00);
	regmap_write(es8389->regmap, ES8389_ADC_REG21, 0x1F);
	regmap_write(es8389->regmap, ES8389_ADC_REG22, 0x7F);
	regmap_write(es8389->regmap, ES8389_DAC_REG41, 0x7F);
	regmap_write(es8389->regmap, ES8389_DAC_REG42, 0x7F);
	regmap_write(es8389->regmap, ES8389_RESET_REG00, 0x00);
	regmap_write(es8389->regmap, ES8389_CLK_OFF1_REG03, 0xC1);
	regmap_write(es8389->regmap, ES8389_RESET_REG00, 0x01);
	regmap_write(es8389->regmap, ES8389_DAC_REG43, 0x10);
	regmap_write(es8389->regmap, ES8389_DAC_RAMP_REG49, 0x0F);
	regmap_update_bits(es8389->regmap, ES8389_ADC_REG20, 0x03, 0x03);
	regmap_update_bits(es8389->regmap, ES8389_DAC_REG40, 0x03, 0x03);
	//regmap_write(es8389->regmap, ES8389_DAC_RESET_REG4D, 0x02);

	return ret;
}

static void es8389_remove(struct snd_soc_component *codec)
{
	struct es8389_private *es8389 = snd_soc_component_get_drvdata(codec);

	printk("Enter into %s()\n", __func__);
	regmap_write(es8389->regmap, ES8389_MASTER_MODE_REG01, 0x28);
	regmap_write(es8389->regmap, ES8389_HPSW_REG69, 0x00);
	regmap_write(es8389->regmap, ES8389_VMID_REG60, 0x00);
	regmap_write(es8389->regmap, ES8389_RESET_REG00, 0x00);
	regmap_write(es8389->regmap, ES8389_CSM_JUMP_REG10, 0xCC);
	usleep_range(500000,550000); //500MS
	regmap_write(es8389->regmap, ES8389_CSM_JUMP_REG10, 0x00);
	regmap_write(es8389->regmap, ES8389_ANA_CTL1_REG61, 0x08);
	regmap_write(es8389->regmap, ES8389_ISO_CTL_REGF3, 0xC1);
	regmap_write(es8389->regmap, ES8389_PULL_DOWN_REGF2, 0x00);

}

static struct snd_soc_component_driver soc_codec_dev_es8389 = {
	.probe = es8389_probe,
	.remove = es8389_remove,
	.suspend = es8389_suspend,
	.resume = es8389_resume,
	.set_bias_level = es8389_set_bias_level,
	//.idle_bias_off = 0,

	.controls = es8389_snd_controls,
	.num_controls = ARRAY_SIZE(es8389_snd_controls),
	.dapm_widgets = es8389_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(es8389_dapm_widgets),
	.dapm_routes = es8389_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(es8389_dapm_routes),
};

static struct regmap_config es8389_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = ES8389_MAX_REGISTER,

	.volatile_reg = es8389_volatile_register,
	.cache_type = REGCACHE_RBTREE,
};

#ifdef CONFIG_OF
static struct of_device_id es8389_if_dt_ids[] = {
	{ .compatible = "everest,es8389", },
	{ }
};
MODULE_DEVICE_TABLE(of, es8389_if_dt_ids);
#endif

static void es8389_i2c_shutdown(struct i2c_client *i2c)
{
	struct snd_soc_component *component;
	struct es8389_private *es8389;

	es8389 = i2c_get_clientdata(i2c);
	component = es8389->component;
	dev_dbg(component->dev, "Enter into %s\n", __func__);

	cancel_delayed_work_sync(&es8389->state_work);

	regmap_write(es8389->regmap, ES8389_MASTER_MODE_REG01, 0x28);
	regmap_write(es8389->regmap, ES8389_HPSW_REG69, 0x00);
	regmap_write(es8389->regmap, ES8389_VMID_REG60, 0x00);
	regmap_write(es8389->regmap, ES8389_RESET_REG00, 0x00);
	regmap_write(es8389->regmap, ES8389_CSM_JUMP_REG10, 0xCC);
	usleep_range(500000,550000); //500MS
	regmap_write(es8389->regmap, ES8389_CSM_JUMP_REG10, 0x00);
	regmap_write(es8389->regmap, ES8389_ANA_CTL1_REG61, 0x08);
	regmap_write(es8389->regmap, ES8389_ISO_CTL_REGF3, 0xC1);
	regmap_write(es8389->regmap, ES8389_PULL_DOWN_REGF2, 0x00);
}
static u32 cur_reg=0;

static ssize_t es8389_show(struct device *dev,
			struct device_attribute *attr, char *_buf)
{
	int ret;
	ret = sprintf(_buf, "%s(): get 0x%04x=0x%04x\n",
			__FUNCTION__, cur_reg,
			snd_soc_component_read(es8389_codec, cur_reg));
	return ret;
}

static ssize_t es8389_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int val = 0, flag = 0;
	u8 i = 0, reg, num, value_w, value_r;

	val = simple_strtol(buf, NULL, 16);
	flag = (val >> 16) & 0xFF;

	if (flag) {
		reg = (val >> 8) & 0xFF;
		value_w = val & 0xFF;
		printk("\nWrite: start REG:0x%02x,val:0x%02x,count:0x%02x\n", reg, value_w, flag);
		while(flag--) {
			snd_soc_component_write(es8389_codec, reg, value_w);
			printk("Write 0x%02x to REG:0x%02x\n", value_w, reg);
			reg++;
		}
	}
	else {
		reg = (val >> 8) & 0xFF;
		num = val & 0xff;
		printk("\nRead: start REG:0x%02x,count:0x%02x\n",
			reg, num);
		do {
			value_r = snd_soc_component_read(es8389_codec, reg);
			printk("REG[0x%02x]: 0x%02x;  \n", reg, value_r);
			reg++;
			i++;
		} while (i<num);
	}

	return count;
}

static DEVICE_ATTR(es8389, 0664, es8389_show, es8389_store);

static struct attribute *es8389_debug_attrs[] = {
	&dev_attr_es8389.attr,
	NULL,
};

static struct attribute_group es8389_debug_attr_group = {
	.name   = "es8389_debug",
	.attrs  = es8389_debug_attrs,
};

static int es8389_read(struct i2c_client *client, u8 reg, u8 *value, int count)
{
        int ret;
        u8 read_cmd[3] = {0};
        u8 cmd_len = 0;

        read_cmd[0] = reg;
        cmd_len = 1;
        ret = i2c_master_send(client, read_cmd, cmd_len);
        if (ret != cmd_len) {
                pr_err("es8389_read error1 ret = %d.\n", ret);
                return -1;
        }
        ret = i2c_master_recv(client, value, count);
        if (ret != 1) {
                pr_err("es8389_read error2, ret = %d.\n", ret);
                return -1;
        }
        return 0;
}

static int es8389_i2c_probe(struct i2c_client *i2c_client,
			    const struct i2c_device_id *id)
{
	struct es8389_private *es8389;
	int ret = -1;
	//unsigned int val;
	u8 value[2];

	printk("Enter into %s\n", __func__);
	es8389 = devm_kzalloc(&i2c_client->dev,
			sizeof(*es8389), GFP_KERNEL);
	if (es8389 == NULL)
		return -ENOMEM;

	ret = es8389_read(i2c_client, ES8389_CHIP_ID0_REGFD, &value[0], 1);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "i2c recv Failed\n");
		return ret;
	}
	ret = es8389_read(i2c_client, ES8389_CHIP_ID1_REGFE, &value[1], 1);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "i2c recv Failed\n");
		return ret;
	}
	dev_info(&i2c_client->dev, "es8389_i2c_probe() chip id %#x %x\n", value[0], value[1]);

	es8389->dmic = ES8389_DMIC;     // dmic interface disabled
	mutex_init(&es8389->lock);
	INIT_DELAYED_WORK(&es8389->state_work,
			  es8389_state_delay_work);

	i2c_set_clientdata(i2c_client, es8389);
	es8389->regmap = devm_regmap_init_i2c(i2c_client, &es8389_regmap);
	if (IS_ERR(es8389->regmap)) {
		ret = PTR_ERR(es8389->regmap);
		dev_err(&i2c_client->dev, "regmap_init() failed: %d\n", ret);
		return ret;
	}
	es8389_data = es8389;

	ret =  devm_snd_soc_register_component(&i2c_client->dev,
			&soc_codec_dev_es8389,
			&es8389_dai,
			1);
	if (ret < 0) {
		return ret;
	}

	printk("Enter into %s-----4\n", __func__);
	ret = sysfs_create_group(&i2c_client->dev.kobj,
				&es8389_debug_attr_group);
	if (ret) {
		pr_err("failed to create attr group\n");
	}

	printk("Exit %s\n", __func__);
	return ret;
}

static void es8389_i2c_remove(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &es8389_debug_attr_group);
}

static const struct i2c_device_id es8389_i2c_id[] = {
	{"everest,es8390", 0},
	{"everest,es8389", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, es8389_i2c_id);

static struct i2c_driver es8389_i2c_driver = {
	.driver = {
		.name	= "es8389",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(es8389_if_dt_ids),
	},
	.shutdown = es8389_i2c_shutdown,
	.probe = es8389_i2c_probe,
	.remove   = es8389_i2c_remove,
	.id_table = es8389_i2c_id,
};
module_i2c_driver(es8389_i2c_driver);

MODULE_DESCRIPTION("ASoC es8389 driver");
MODULE_AUTHOR("David Yang <yangxiaohua@everest-semi.com>");
MODULE_LICENSE("GPL");


