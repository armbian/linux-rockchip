// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * it6621-codec.c -- IT6621 ALSA SoC audio codec driver
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/asoundef.h>

#include "it6621.h"
#include "it6621-clk.h"
#include "it6621-earc.h"
#include "it6621-uapi.h"
#include "it6621-reg-bank0.h"
#include "it6621-reg-bank1.h"
#include "it6621-reg-cec.h"

#define IT6621_I2C_NAME "it6621"

static const struct regmap_range_cfg it6621_range = {
	.range_min = IT6621_REG_VENDOR_ID_LOW,
	.range_max = IT6621_REG_TX_PKT3_PB15,
	.selector_reg = IT6621_REG_SYS_CTRL3,
	.selector_mask = IT6621_BANK_SEL,
	.selector_shift = 0,
	.window_start = 0,
	.window_len = 256,
};

static bool it6621_is_accessible_reg(struct device *dev, unsigned int reg)
{
	if (reg >= IT6621_REG_VENDOR_ID_LOW && reg <= IT6621_REG_TX_PKT3_PB15)
		return true;

	return false;
}

static const struct regmap_config it6621_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = IT6621_REG_TX_PKT3_PB15,
	.readable_reg = it6621_is_accessible_reg,
	.writeable_reg = it6621_is_accessible_reg,
	.ranges = &it6621_range,
	.num_ranges = 1,
};

static int it6621_set_audio_src(struct it6621_priv *priv)
{
	unsigned int src;

	if (priv->audio_ch >= 15)
		src = IT6621_AUD_SRC7_SRC0_EN;
	else if (priv->audio_ch >= 13)
		src = IT6621_AUD_SRC6_SRC0_EN;
	else if (priv->audio_ch >= 11)
		src = IT6621_AUD_SRC5_SRC0_EN;
	else if (priv->audio_ch >= 9)
		src = IT6621_AUD_SRC4_SRC0_EN;
	else if (priv->audio_ch >= 7 || priv->i2s_hbr)
		src = IT6621_AUD_SRC3_SRC0_EN;
	else if (priv->audio_ch >= 5)
		src = IT6621_AUD_SRC2_SRC0_EN;
	else if (priv->audio_ch >= 3)
		src = IT6621_AUD_SRC1_SRC0_EN;
	else
		src = IT6621_AUD_SRC0_EN;

	/* Set input audio source */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_AUD_SRC_SEL,
			   IT6621_TX_AUD_SRC, priv->audio_src);

	/* Update source number */
	regmap_write(priv->regmap, IT6621_REG_TX_AUD_SRC_EN, src);

	/* Update ACLK detect after changing source number */
	regmap_update_bits(priv->regmap, IT6621_REG_CLK_DET7,
			   IT6621_ACLK_DET_EN_SEL, IT6621_ACLK_DET_DIS);
	regmap_update_bits(priv->regmap, IT6621_REG_CLK_DET7,
			   IT6621_ACLK_DET_EN_SEL, IT6621_ACLK_DET_EN);

	return 0;
}

static int it6621_set_i2s_tdm_fmt(struct it6621_priv *priv)
{
	unsigned int wl;

	if ((priv->i2s_wl == IEC958_AES4_CON_WORDLEN_24_20) ||
	    (priv->i2s_wl == (IEC958_AES4_CON_WORDLEN_20_16 |
			      IEC958_AES4_CON_MAX_WORDLEN_24)))
		wl = IT6621_I2S_TDM_20BIT;
	else if (priv->i2s_wl == (IEC958_AES4_CON_WORDLEN_24_20 |
				  IEC958_AES4_CON_MAX_WORDLEN_24))
		wl = IT6621_I2S_TDM_24BIT;
	else
		wl = IT6621_I2S_TDM_16BIT;

	regmap_update_bits(priv->regmap, IT6621_REG_TX_I2S,
			   IT6621_I2S_TDM_WORD_LEN | IT6621_I2S_FMT,
			   wl | priv->i2s_fmt);

	regmap_update_bits(priv->regmap, IT6621_REG_TX_AUD_CTRL3,
			   IT6621_TX_I2S_HBR_EN_SEL | IT6621_TX_TDM_CH_NUM,
			   priv->i2s_hbr | ((priv->audio_ch / 2) - 1));

	return 0;
}

static int it6621_set_spdif_fmt(struct it6621_priv *priv)
{
	if (priv->audio_type == IT6621_AUD_TYPE_NLPCM)
		regmap_update_bits(priv->regmap, IT6621_REG_DMAC_CTRL1,
				   IT6621_TX_V_BIT_VAL, IT6621_TX_V_BIT_NLPCM);
	else
		regmap_update_bits(priv->regmap, IT6621_REG_DMAC_CTRL1,
				   IT6621_TX_V_BIT_VAL, IT6621_TX_V_BIT_LPCM);

	return 0;
}

static int it6621_set_auto_audio_fmt(struct it6621_priv *priv)
{
	unsigned int auto_fmt;

	if ((priv->audio_type == IT6621_AUD_TYPE_NLPCM) &&
	    (priv->audio_ch == 8)) {
		auto_fmt = IT6621_TX_AUTO_AUD_FMT_DIS;
	} else {
		/* NOTE: Disable auto audio format in SPDIF */
		if (priv->audio_src == IT6621_AUD_SRC_SPDIF)
			auto_fmt = IT6621_TX_AUTO_AUD_FMT_DIS;
		else
			auto_fmt = IT6621_TX_AUTO_AUD_FMT_EN;
	}

	regmap_update_bits(priv->regmap, IT6621_REG_TX_AUD_CTRL10,
			   IT6621_TX_AUTO_AUD_FMT_SEL, auto_fmt);

	return 0;
}

static int it6621_set_audio_fmt(struct it6621_priv *priv)
{
	int ret;

	/* Set audio source */
	ret = it6621_set_audio_src(priv);
	if (ret < 0)
		return ret;

	/* RX I2S/TDM word length */
	ret = it6621_set_i2s_tdm_fmt(priv);
	if (ret < 0)
		return ret;

	/* SPDIF V bit */
	ret = it6621_set_spdif_fmt(priv);
	if (ret < 0)
		return ret;

	/* For Compress audio LayoutB */
	ret = it6621_set_auto_audio_fmt(priv);
	if (ret < 0)
		return ret;

	/* eARC TX Channel Status */
	ret = it6621_set_channel_status(priv);
	if (ret < 0)
		return ret;

	return 0;
}

static int it6621_update_config(struct it6621_priv *priv)
{
	if (priv->force_16ch)
		priv->audio_ch = 16;

	if ((priv->audio_fs == IT6621_AES3_CON_FS_256000) ||
	    (priv->audio_fs == IT6621_AES3_CON_FS_352000) ||
	    (priv->audio_fs == IT6621_AES3_CON_FS_384000) ||
	    (priv->audio_fs == IT6621_AES3_CON_FS_512000) ||
	    (priv->audio_fs == IT6621_AES3_CON_FS_705000) ||
	    (priv->audio_fs == IEC958_AES3_CON_FS_768000) ||
	    (priv->audio_fs == IT6621_AES3_CON_FS_1024000) ||
	    (priv->audio_fs == IT6621_AES3_CON_FS_1411000) ||
	    (priv->audio_fs == IT6621_AES3_CON_FS_1536000))
		priv->audio_hbr = 1;
	else
		priv->audio_hbr = 0;

	if (priv->audio_input_hbr == 1)
		priv->audio_hbr = 1;

	if (priv->audio_hbr)
		priv->audio_type = IT6621_AUD_TYPE_NLPCM;

	if ((priv->audio_src == IT6621_AUD_SRC_I2S) &&
	    priv->audio_hbr && priv->i2s_hbr_enabled)
		priv->i2s_hbr = IT6621_TX_I2S_HBR_EN;
	else
		priv->i2s_hbr = IT6621_TX_I2S_HBR_DIS;

	if ((priv->audio_src == IT6621_AUD_SRC_I2S) &&
	    (priv->audio_type == IT6621_AUD_TYPE_NLPCM))
		priv->i2s_nlpcm_enabled = IT6621_TX_NLPCM_I2S_EN;
	else
		priv->i2s_nlpcm_enabled = IT6621_TX_NLPCM_I2S_DIS;

	/* NOTE: For Allion AC3 and Allion Astro VG849C test. */
	if ((priv->audio_ch > 2) &&
	    (priv->audio_type == IT6621_AUD_TYPE_NLPCM))
		priv->audio_ch = 2;

	regmap_update_bits(priv->regmap, IT6621_REG_TX_AUD_CTRL10,
			   IT6621_TX_2CH_LAYOUT_SEL | IT6621_TX_NLPCM_I2S_SEL |
			   IT6621_TX_MCH_LPCM_SEL | IT6621_TX_EXT_MUTE_SEL,
			   IT6621_TX_2CH_LAYOUT_DIS | priv->i2s_nlpcm_enabled |
			   IT6621_TX_MCH_LPCM_DIS | IT6621_TX_EXT_MUTE_EN);
	return 0;
}

static int it6621_reconfig_aclk(struct it6621_priv *priv)
{
	if (((priv->audio_src == IT6621_AUD_SRC_I2S) && priv->sck_inv_enabled) ||
	    ((priv->audio_src == IT6621_AUD_SRC_TDM) && priv->tck_inv_enabled) ||
	    ((priv->audio_src == IT6621_AUD_SRC_SPDIF) && priv->mclk_inv_enabled) ||
	    ((priv->audio_src == IT6621_AUD_SRC_DSD) && priv->dclk_inv_enabled))
		regmap_update_bits(priv->regmap, IT6621_REG_CLK_CTRL2,
				   IT6621_ACLK_INV_SEL, IT6621_ACLK_INV_EN);
	else
		regmap_update_bits(priv->regmap, IT6621_REG_CLK_CTRL2,
				   IT6621_ACLK_INV_SEL, IT6621_ACLK_INV_DIS);

	return 0;
}

static int it6621_config_audio(struct it6621_priv *priv)
{
	int ret;

	ret = it6621_update_config(priv);
	if (ret < 0)
		return ret;

	ret = it6621_reconfig_aclk(priv);
	if (ret < 0)
		return ret;

	ret = it6621_set_audio_fmt(priv);
	if (ret < 0)
		return ret;

	return 0;
}

static int it6621_startup(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	struct it6621_priv *priv = snd_soc_component_get_drvdata(dai->component);
	unsigned int state;

	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK)
		return 0;

	set_bit(IT6621_AUDIO_START, &priv->audio_flag);
	clear_bit(IT6621_AUDIO_TO_EN_DMAC, &priv->audio_flag);

	it6621_get_ddfsm_state(priv, &state);

	if ((state != IT6621_TX_EARC_MODE) && (state != IT6621_TX_ARC_MODE)) {
		cancel_work_sync(&priv->hpdio_work);
		schedule_work(&priv->hpdio_work);
	}

	priv->config_audio = true;

	return 0;
}

static int it6621_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct it6621_priv *priv = snd_soc_component_get_drvdata(dai->component);

	if (stream != SNDRV_PCM_STREAM_PLAYBACK)
		return 0;

	if (!priv->force_mute)
		return 0;

	/* FIXME: Sometimes unmuting stream does not work */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_AUD_CTRL10,
			   IT6621_TX_FORCE_MUTE_SEL,
			   mute ? IT6621_TX_FORCE_MUTE_EN :
				  IT6621_TX_FORCE_MUTE_DIS);

	return 0;
}

static void it6621_shutdown(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct it6621_priv *priv = snd_soc_component_get_drvdata(dai->component);

	clear_bit(IT6621_AUDIO_START, &priv->audio_flag);
	clear_bit(IT6621_AUDIO_TO_EN_DMAC, &priv->audio_flag);
}

static int it6621_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				 unsigned int freq, int dir)
{
	struct it6621_priv *priv = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	if (!freq)
		return 0;

	ret = clk_set_rate(priv->mclk, freq);
	if (ret)
		dev_err(priv->dev, "failed to set mclk\n");

	return ret;
}

static int it6621_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct it6621_priv *priv = snd_soc_dai_get_drvdata(dai);
	unsigned int fs;
	unsigned int wl;
	unsigned int ch;
	int ret;

	switch (snd_pcm_rate_to_rate_bit(params_rate(params))) {
	case SNDRV_PCM_RATE_32000:
		fs = IEC958_AES3_CON_FS_32000;
		break;
	case SNDRV_PCM_RATE_44100:
		fs = IEC958_AES3_CON_FS_44100;
		break;
	case SNDRV_PCM_RATE_48000:
		fs = IEC958_AES3_CON_FS_48000;
		break;
	case SNDRV_PCM_RATE_64000:
		fs = IT6621_AES3_CON_FS_64000;
		break;
	case SNDRV_PCM_RATE_88200:
		fs = IEC958_AES3_CON_FS_88200;
		break;
	case SNDRV_PCM_RATE_96000:
		fs = IEC958_AES3_CON_FS_96000;
		break;
	case SNDRV_PCM_RATE_176400:
		fs = IEC958_AES3_CON_FS_176400;
		break;
	case SNDRV_PCM_RATE_192000:
		fs = IEC958_AES3_CON_FS_192000;
		break;
	case SNDRV_PCM_RATE_352800:
		fs = IT6621_AES3_CON_FS_352000;
		break;
	case SNDRV_PCM_RATE_384000:
		fs = IT6621_AES3_CON_FS_384000;
		break;
	default:
		dev_err(priv->dev, "invalid rate: %d\n", params_rate(params));
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		wl = IEC958_AES4_CON_WORDLEN_20_16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		wl = IEC958_AES4_CON_WORDLEN_24_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		wl = IEC958_AES4_CON_WORDLEN_24_20 |
		     IEC958_AES4_CON_MAX_WORDLEN_24;
		break;
	default:
		dev_err(priv->dev, "invalid format: %d\n",
			params_format(params));
		return -EINVAL;
	}

	switch (params_channels(params)) {
	case 32:
	case 16:
	case 8:
	case 6:
	case 2:
		ch = params_channels(params);
		break;
	default:
		dev_err(priv->dev, "invalid channel: %d\n",
			params_channels(params));
		return -EINVAL;
	}

	if (priv->config_audio || (priv->audio_fs != fs) ||
	    (priv->i2s_wl != wl) || (priv->audio_ch != ch)) {
		priv->audio_fs = fs;
		priv->i2s_wl = wl;
		priv->audio_ch = ch;
		priv->config_audio = false;

		ret = it6621_config_audio(priv);
		if (ret)
			return ret;
	}

	return 0;
}

static int it6621_probe(struct snd_soc_component *component)
{
	struct it6621_priv *priv;

	priv = snd_soc_component_get_drvdata(component);
	priv->mclk = devm_clk_get_optional(component->dev, "mclk");

	if (IS_ERR(priv->mclk))
		return PTR_ERR(priv->mclk);

	clk_prepare_enable(priv->mclk);

	return 0;
}

static int it6621_suspend(struct snd_soc_component *component)
{
	struct it6621_priv *priv = snd_soc_component_get_drvdata(component);

	clear_bit(IT6621_ARC_START, &priv->events);
	clear_bit(IT6621_EARC_CAP_CHG, &priv->events);
	clear_bit(IT6621_EARC_EDID_OK, &priv->events);
	clear_bit(IT6621_EARC_BCLK_OK, &priv->events);

	return 0;
}

static int it6621_resume(struct snd_soc_component *component)
{
	struct it6621_priv *priv = snd_soc_component_get_drvdata(component);

	return it6621_earc_init(priv);
}

static int it6621_audio_cap_ctl_info(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof_field(struct it6621_priv, rxcap);

	return 0;
}

static int it6621_audio_cap_ctl_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct it6621_priv *priv = snd_soc_component_get_drvdata(component);

	mutex_lock(&priv->rxcap_lock);
	memcpy(ucontrol->value.bytes.data, priv->rxcap, sizeof(priv->rxcap));
	mutex_unlock(&priv->rxcap_lock);

	return 0;
}

static const char *const it6621_ddfsm_enum[] = { "ARC", "eARC", "unknown" };

static int it6621_ddfsm_ctl_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = ARRAY_SIZE(it6621_ddfsm_enum);

	if (uinfo->value.enumerated.item >= ARRAY_SIZE(it6621_ddfsm_enum))
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;

	strscpy(uinfo->value.enumerated.name,
		it6621_ddfsm_enum[uinfo->value.enumerated.item],
		sizeof(uinfo->value.enumerated.name));

	return 0;
}

static int it6621_ddfsm_ctl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct it6621_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int state;

	it6621_get_ddfsm_state(priv, &state);

	if (state == IT6621_TX_ARC_MODE)
		ucontrol->value.enumerated.item[0] = 0;
	else if (state == IT6621_TX_EARC_MODE)
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 2;

	return 0;
}

static int it6621_earc_switch_ctl_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int it6621_earc_switch_ctl_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct it6621_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (priv->earc_enabled ==
					    IT6621_TX_EARC_EN) ? 1 : 0;

	return 0;
}

static int it6621_earc_switch_ctl_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct it6621_priv *priv = snd_soc_component_get_drvdata(component);

	return it6621_set_earc_enabled(priv, !!ucontrol->value.integer.value[0]);
}

static int it6621_arc_switch_ctl_info(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int it6621_arc_switch_ctl_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct it6621_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (priv->arc_enabled ==
					    IT6621_TX_ARC_EN) ? 1 : 0;

	return 0;
}

static int it6621_arc_switch_ctl_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct it6621_priv *priv = snd_soc_component_get_drvdata(component);

	return it6621_set_arc_enabled(priv, !!ucontrol->value.integer.value[0]);
}

static const struct snd_kcontrol_new it6621_controls[] = {
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READ |
			   SNDRV_CTL_ELEM_ACCESS_VOLATILE),
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "eARC Audio Capabilities",
		.info = it6621_audio_cap_ctl_info,
		.get = it6621_audio_cap_ctl_get,
	},
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READ |
			   SNDRV_CTL_ELEM_ACCESS_VOLATILE),
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "eARC DD FSM",
		.info = it6621_ddfsm_ctl_info,
		.get = it6621_ddfsm_ctl_get,
	},
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
			   SNDRV_CTL_ELEM_ACCESS_VOLATILE),
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "eARC Switch",
		.info = it6621_earc_switch_ctl_info,
		.get = it6621_earc_switch_ctl_get,
		.put = it6621_earc_switch_ctl_put,
	},
	{
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE |
			   SNDRV_CTL_ELEM_ACCESS_VOLATILE),
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "ARC Switch",
		.info = it6621_arc_switch_ctl_info,
		.get = it6621_arc_switch_ctl_get,
		.put = it6621_arc_switch_ctl_put,
	},
};

static const struct snd_soc_component_driver it6621_component = {
	.probe = it6621_probe,
	.suspend = it6621_suspend,
	.resume = it6621_resume,
	.controls = it6621_controls,
	.num_controls = ARRAY_SIZE(it6621_controls),
};

static const struct snd_soc_dai_ops it6621_dai_ops = {
	.startup = it6621_startup,
	.set_sysclk = it6621_set_dai_sysclk,
	.hw_params = it6621_hw_params,
	.mute_stream = it6621_mute,
	.shutdown = it6621_shutdown,
};

#define IT6621_RATES (SNDRV_PCM_RATE_32000 | \
		      SNDRV_PCM_RATE_44100 | \
		      SNDRV_PCM_RATE_48000 | \
		      SNDRV_PCM_RATE_64000 | \
		      SNDRV_PCM_RATE_88200 | \
		      SNDRV_PCM_RATE_96000 | \
		      SNDRV_PCM_RATE_176400 | \
		      SNDRV_PCM_RATE_192000 | \
		      SNDRV_PCM_RATE_352800 | \
		      SNDRV_PCM_RATE_384000)

#define IT6621_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_driver it6621_dais[] = {
	{
		.name = "it6621-i2s",
		.playback = {
			.stream_name = "IT6621 I2S Playback",
			.channels_min = 2,
			.channels_max = 16,
			.rates = IT6621_RATES,
			.formats = IT6621_FORMATS,
		},
		.ops = &it6621_dai_ops,
	}, {
		.name = "it6621-spdif",
		.playback = {
			.stream_name = "IT6621 SPDIF Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = IT6621_RATES,
			.formats = IT6621_FORMATS,
		},
		.ops = &it6621_dai_ops,
	}, {
		.name = "it6621-dsd",
		.playback = {
			.stream_name = "IT6621 DSD Playback",
			.channels_min = 2,
			.channels_max = 12,
			.rates = IT6621_RATES,
			.formats = IT6621_FORMATS,
		},
		.ops = &it6621_dai_ops,
	},
};

static int it6621_check_dev(struct it6621_priv *priv)
{
	unsigned int val;

	regmap_bulk_read(priv->regmap, IT6621_REG_VENDOR_ID_LOW, &val, 2);
	priv->vid = le16_to_cpu(val);
	if (priv->vid != IT6621_VENDOR_ID) {
		dev_err(priv->dev, "invalid vendor id %x\n", priv->vid);
		return -ENODEV;
	}

	regmap_bulk_read(priv->regmap, IT6621_REG_DEV_ID_LOW, &val, 2);
	priv->devid = le16_to_cpu(val);
	if (priv->devid != IT6621_DEVICE_ID) {
		dev_err(priv->dev, "invalid device id %x\n", priv->devid);
		return -ENODEV;
	}

	regmap_read(priv->regmap, IT6621_REG_REV_ID, &val);
	priv->revid = val;

	if ((priv->revid != IT6621_REVISION_VARIANT_B0) &&
	    (priv->revid != IT6621_REVISION_VARIANT_C0) &&
	    (priv->revid != IT6621_REVISION_VARIANT_D0)) {
		dev_err(priv->dev, "invalid revision id %x\n", priv->revid);
		return -ENODEV;
	}

	return 0;
}

static void it6621_toggle_hpdio(struct it6621_priv *priv)
{
	mutex_lock(&priv->hpdio_lock);

	dev_dbg(priv->dev, "toggle hpdio\n");
	gpiod_set_value_cansleep(priv->hpdio, 0);
	/*
	 * NOTE: Toggle HPD signal at least 100ms (suggestion time is about
	 * 1 ~ 1.5 second).
	 */
	msleep(100);
	gpiod_set_value_cansleep(priv->hpdio, 1);

	mutex_unlock(&priv->hpdio_lock);
}

static void it6621_hpdio_work(struct work_struct *work)
{
	struct it6621_priv *priv = container_of(work, struct it6621_priv,
						hpdio_work);

	it6621_toggle_hpdio(priv);
}

static int it6621_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct it6621_priv *priv;
	const char *source;
	int ret;

	priv = devm_kzalloc(&client->dev, sizeof(struct it6621_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&client->dev, priv);

	priv->client = client;
	priv->dev = &client->dev;
	priv->regmap = devm_regmap_init_i2c(client, &it6621_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(priv->dev, "failed to initialize regmap\n");
		return PTR_ERR(priv->regmap);
	}

	ret = it6621_check_dev(priv);
	if (ret < 0)
		return ret;

	priv->audio_type = IT6621_AUD_TYPE_LPCM;
	priv->audio_ch = 0;
	priv->audio_fs = IEC958_AES3_CON_FS_NOTID;
	priv->i2s_wl = IEC958_AES4_CON_WORDLEN_NOTID;
	priv->mch_lpcm_enabled = IT6621_TX_MCH_LPCM_DIS;
	priv->layout_2ch_enabled = IT6621_TX_2CH_LAYOUT_DIS;
	priv->vcm_sel = IT6621_TX_VCM0;
	priv->i2s_fmt = IT6621_I2S_FMT_32BIT;
	priv->rclk_sel = IT6621_RCLK_FREQ_REFCLK_DIV_2;
	priv->update_avg_enabled = IT6621_UPDATE_AVG_EN;
	priv->sck_inv_enabled = 0;
	priv->tck_inv_enabled = 1;
	priv->mclk_inv_enabled = 1;
	priv->dclk_inv_enabled = 0;
	priv->cmo_opt = IT6621_TX_CMO_NORMAL;
	priv->force_cmo_enabled = IT6621_TX_FORCE_CMO_EN;
	priv->resync_opt = IT6621_TX_RESYNC_NEW;
	priv->pkt1_enabled = IT6621_TX_PKT1_DIS;
	priv->pkt2_enabled = IT6621_TX_PKT2_DIS;
	priv->pkt3_enabled = IT6621_TX_PKT3_DIS;
	priv->i2s_hbr_enabled = 1;
	priv->ubit_opt = IT6621_TX_U_BIT_1BIT_FRAME;
	priv->c_ch_opt = IT6621_TX_C_CH_OPT1;
	priv->ecc_opt = IT6621_TX_ECC_U_SWAP;
	priv->enc_seed = 0xa5c3;
	priv->enc_opt = IT6621_TX_ENC_OPT3;
	priv->bclk_inv_enabled = IT6621_BCLK_INV_DIS;
	priv->nxt_pkt_to_sel = IT6621_TX_NEXT_PKT_TIMEOUT_50US;
	priv->turn_over_sel = IT6621_TX_TURN_OVER_16US;
	priv->hb_retry_sel = IT6621_TX_HB_RETRY_0MS;
	priv->hb_retry_enabled = IT6621_TX_HB_RETRY_EN;
	priv->cmd_to_enabled = 0;
	priv->enter_arc_now = IT6621_TX_ARC_NOW_DIS;
	priv->arc_enabled = IT6621_TX_ARC_EN;
	priv->earc_enabled = IT6621_TX_EARC_EN;
	INIT_LIST_HEAD(&priv->fhs);
	mutex_init(&priv->fhs_lock);
	mutex_init(&priv->hpdio_lock);
	mutex_init(&priv->rxcap_lock);
	INIT_WORK(&priv->hpdio_work, it6621_hpdio_work);

	ret = it6621_uapi_init(priv);
	if (ret) {
		dev_err(priv->dev, "failed to initialize uapi\n");
		return ret;
	}

	ret = devm_add_action_or_reset(priv->dev,
				       (void(*)(void *))it6621_uapi_remove,
				       priv);
	if (ret)
		return ret;

	ret = device_property_read_string(priv->dev, "ite,audio-source",
					  &source);
	if (ret) {
		dev_err(priv->dev, "no input audio source\n");
		return ret;
	}

	if (!strcmp(source, "i2s"))
		priv->audio_src = IT6621_AUD_SRC_I2S;
	else if (!strcmp(source, "spdif"))
		priv->audio_src = IT6621_AUD_SRC_SPDIF;
	else if (!strcmp(source, "tdm"))
		priv->audio_src = IT6621_AUD_SRC_TDM;
	else if (!strcmp(source, "dsd"))
		priv->audio_src = IT6621_AUD_SRC_DSD;
	else
		return -EINVAL;

	if (device_property_read_bool(priv->dev, "ite,force-arc"))
		priv->force_arc = 1;

	if (device_property_read_bool(priv->dev, "ite,force-earc"))
		priv->force_earc = 1;

	if (device_property_read_u32(priv->dev, "ite,fixed-lcf",
				     &priv->fixed_lcf))
		priv->fixed_lcf = 0x00;

	priv->hpdio = devm_gpiod_get_optional(&client->dev, "hpdio",
					      GPIOD_OUT_LOW);

	ret = it6621_earc_init(priv);
	if (ret)
		return ret;

	if (client->irq) {
		ret = devm_request_threaded_irq(priv->dev, client->irq, NULL,
						it6621_irq,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						"it6621", priv);
		if (ret)
			return ret;
	}

	return devm_snd_soc_register_component(priv->dev, &it6621_component,
					       it6621_dais,
					       ARRAY_SIZE(it6621_dais));
}

static const struct i2c_device_id it6621_i2c_id[] = {
	{ IT6621_I2C_NAME, 0 },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(i2c, it6621_i2c_id);

static const struct of_device_id it6621_dt_match[] = {
	{ .compatible = "ite,it6621" },
	{ /* sentinel */ },
};

static struct i2c_driver it6621_i2c_driver = {
	.driver = {
		.name = IT6621_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(it6621_dt_match),
	},
	.probe = it6621_i2c_probe,
	.id_table = it6621_i2c_id,
};

module_i2c_driver(it6621_i2c_driver);

MODULE_AUTHOR("Jason Zhang <jason.zhang@rock-chips.com>");
MODULE_DESCRIPTION("ASoC IT6621 eARC Driver");
MODULE_LICENSE("GPL");
