// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#include <linux/notifier.h>
#include <sound/asoundef.h>

#include "it6621.h"
#include "it6621-clk.h"
#include "it6621-earc.h"
#include "it6621-reg-bank0.h"
#include "it6621-reg-bank1.h"
#include "it6621-reg-cec.h"
#include "it6621-uapi.h"

int it6621_set_channel_status(struct it6621_priv *priv)
{
	unsigned int layout = 0;
	unsigned int ca = 0;
	unsigned int fmt = 0;
	unsigned int wl = 0;
	unsigned int fs = 0;

	/* NOTE: Add for Category code */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_CH_ST1, 0xff,
			   IEC958_AES1_CON_PCM_CODER);

	/* Refer to HDMI2.1 spec table 9-23 and ICE60958-3 p9 */
	if ((priv->audio_type == IT6621_AUD_TYPE_LPCM) &&
	    (priv->audio_ch == 2) && (priv->audio_enc == 0) &&
	    (priv->mch_lpcm_enabled == IT6621_TX_MCH_LPCM_DIS) &&
	    (priv->i2s_nlpcm_enabled == IT6621_TX_NLPCM_I2S_DIS))
		fmt = HDMI_AUDIO_FMT_UN_2CH_LPCM;
	else if ((priv->audio_type == IT6621_AUD_TYPE_LPCM) &&
		 (priv->audio_ch == 2) && (priv->audio_enc == 0) &&
		 (priv->mch_lpcm_enabled == IT6621_TX_MCH_LPCM_EN))
		fmt = HDMI_AUDIO_FMT_UN_MCH_LPCM;
	else if ((priv->audio_type == IT6621_AUD_TYPE_LPCM) &&
		 (priv->audio_ch > 2) && (priv->audio_enc == 0))
		fmt = HDMI_AUDIO_FMT_UN_MCH_LPCM;
	else if ((priv->audio_type == IT6621_AUD_TYPE_DSD) &&
		 (priv->audio_enc == 0))
		fmt = HDMI_AUDIO_FMT_UN_XCH_DSD;
	else if ((priv->audio_type == IT6621_AUD_TYPE_NLPCM) &&
		 (priv->audio_enc == 0))
		fmt = HDMI_AUDIO_FMT_UN_2CH_NLPCM;
	else if ((priv->audio_type == IT6621_AUD_TYPE_NLPCM) &&
		 (priv->audio_enc == 1))
		fmt = HDMI_AUDIO_FMT_EN_2CH_NLPCM;
	else if ((priv->audio_type == IT6621_AUD_TYPE_LPCM) &&
		 (priv->audio_enc == 1))
		fmt = HDMI_AUDIO_FMT_EN_MCH_NLPCM;
	else if ((priv->audio_type == IT6621_AUD_TYPE_DSD) &&
		 (priv->audio_enc == 1))
		fmt = HDMI_AUDIO_FMT_EN_XCH_DSD;
	else
		dev_err(priv->dev, "invalid audio format\n");

	/*
	 * NOTE: Software for which copyright is asserted, Channel status mode
	 * is set to mode 0, and audio format is set with fmt.
	 */
	regmap_write(priv->regmap, IT6621_REG_TX_CH_ST0,
		     ((fmt & 0x1c) << 1) | (fmt & 0x03));

	/* Refer to IEC60958-3 p12 */
	if (priv->audio_src == IT6621_AUD_SRC_DSD)
		wl = IEC958_AES4_CON_WORDLEN_NOTID;
	else if (priv->audio_type == IT6621_AUD_TYPE_NLPCM)
		wl = IEC958_AES4_CON_WORDLEN_20_16;
	else
		wl = priv->i2s_wl;

	/* NOTE: Set sample word length */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_CH_ST4, 0x0f, wl);

	/* Refer to HDMI2.1 spec table 9-25 and IEC60958-3 p12 */
	if ((priv->audio_type == IT6621_AUD_TYPE_LPCM) &&
	    (priv->audio_ch <= 2))
		layout = HDMI_AUDIO_LAYOUT_LPCM_2CH;
	else if ((priv->audio_type == IT6621_AUD_TYPE_LPCM) &&
		 (priv->audio_ch <= 8))
		layout = HDMI_AUDIO_LAYOUT_LPCM_8CH;
	else if ((priv->audio_type == IT6621_AUD_TYPE_LPCM) &&
		 (priv->audio_ch <= 16))
		layout = HDMI_AUDIO_LAYOUT_LPCM_16CH;
	else if ((priv->audio_type == IT6621_AUD_TYPE_NLPCM) &&
		 (priv->audio_ch <= 2))
		layout = HDMI_AUDIO_LAYOUT_NLPCM_2CH;
	else if ((priv->audio_type == IT6621_AUD_TYPE_NLPCM) &&
		 (priv->audio_ch <= 8))
		layout = HDMI_AUDIO_LAYOUT_NLPCM_8CH;
	else if ((priv->audio_type == IT6621_AUD_TYPE_DSD) &&
		 (priv->audio_ch <= 6))
		layout = HDMI_AUDIO_LAYOUT_DSD_6CH;
	else if ((priv->audio_type == IT6621_AUD_TYPE_DSD) &&
		 (priv->audio_ch <= 12))
		layout = HDMI_AUDIO_LAYOUT_DSD_12CH;
	else
		dev_err(priv->dev, "invalid layout\n");

	regmap_write(priv->regmap, IT6621_REG_TX_CH_ST5, layout << 4);

	/* Refer to HDMI2.0 spec, IEC60958-3 p11 and p12 */
	if ((layout == HDMI_AUDIO_LAYOUT_LPCM_2CH) ||
	    (layout == HDMI_AUDIO_LAYOUT_NLPCM_2CH)) {
		fs = priv->audio_fs;
	} else if ((layout == HDMI_AUDIO_LAYOUT_LPCM_8CH) ||
		   (layout == HDMI_AUDIO_LAYOUT_NLPCM_8CH)) {
		if (priv->audio_fs == IEC958_AES3_CON_FS_32000)
			fs = IT6621_AES3_CON_FS_128000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_44100)
			fs = IEC958_AES3_CON_FS_176400;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_48000)
			fs = IEC958_AES3_CON_FS_192000;
		else if (priv->audio_fs == IT6621_AES3_CON_FS_64000)
			fs = IT6621_AES3_CON_FS_256000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_88200)
			fs = IT6621_AES3_CON_FS_352000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_96000)
			fs = IT6621_AES3_CON_FS_384000;
		else if (priv->audio_fs == IT6621_AES3_CON_FS_128000)
			fs = IT6621_AES3_CON_FS_512000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_176400)
			fs = IT6621_AES3_CON_FS_705000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_192000)
			fs = IEC958_AES3_CON_FS_768000;
		else
			dev_err(priv->dev, "invalid fs and channel\n");
	} else if ((layout == HDMI_AUDIO_LAYOUT_LPCM_16CH) ||
		   (layout == HDMI_AUDIO_LAYOUT_DSD_6CH)) {
		if (priv->audio_fs == IEC958_AES3_CON_FS_32000)
			fs = IT6621_AES3_CON_FS_256000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_44100)
			fs = IT6621_AES3_CON_FS_352000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_48000)
			fs = IT6621_AES3_CON_FS_384000;
		else if (priv->audio_fs == IT6621_AES3_CON_FS_64000)
			fs = IT6621_AES3_CON_FS_512000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_88200)
			fs = IT6621_AES3_CON_FS_705000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_96000)
			fs = IEC958_AES3_CON_FS_768000;
		else
			dev_err(priv->dev, "invalid fs and channel\n");
	} else if (layout == HDMI_AUDIO_LAYOUT_DSD_12CH) {
		if (priv->audio_fs == IEC958_AES3_CON_FS_32000)
			fs = IT6621_AES3_CON_FS_512000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_44100)
			fs = IT6621_AES3_CON_FS_705000;
		else if (priv->audio_fs == IEC958_AES3_CON_FS_48000)
			fs = IEC958_AES3_CON_FS_768000;
		else
			dev_err(priv->dev, "invalid fs and channel\n");
	}

	regmap_write(priv->regmap, IT6621_REG_TX_CH_ST3,
		     ((fs & 0x30) << 2) + (fs & 0x0f));

	/* Refer to HDMI2.1 spec table 9-28 and CTA861-G table 28~30 */
	/* Audio InfoFrame */
	if (priv->audio_ch > 8) {
#ifdef _1QD980ATC_ // for 5-1-60
		ca = 0x00;
#else
		/*
		 * CTA-861-G spec. Section 6.6.3: Delivery According to the
		 * Speaker Mask (Byte 4 = 0xFE).
		 */
		ca = 0xfe;
#endif
	} else if ((priv->audio_type == IT6621_AUD_TYPE_NLPCM) ||
		   (priv->mch_lpcm_enabled == IT6621_TX_MCH_LPCM_EN) ||
		   (priv->layout_2ch_enabled == IT6621_TX_2CH_LAYOUT_EN)) {
		ca = 0x00;
	} else {
		/*
		 * CTA-861-G spec. Table 35 Audio InfoFrame Data Byte 4.
		 * 0x00: FR/FL
		 * 0x01: FR/FL/LFE1
		 * 0x03: FR/FL/LFE1/FC
		 * 0x07: FR/FL/LFE1/FC/BC
		 * 0x0B: FR/FL/LFE1/FC/LS/RS
		 * 0x0F: FR/FL/LFE1/FC/LS/RS/BC
		 * 0x13: FR/FL/LFE1/FC/LS/RS/RLC/RRC
		 * 0xff: Channels delivered according to Channel Index
		 */

		switch (priv->audio_ch) {
		case 0:
			ca = 0xff;
			break; // no audio
		case 2:
			ca = 0x00;
			break;
		case 3:
			ca = 0x01;
			break; // 0x01,0x02,0x04
		case 4:
			ca = 0x03;
			break; // 0x03,0x05,0x06,0x08,0x14
		case 5:
			ca = 0x07;
			break; // 0x07,0x09,0x0A,0x0C,0x15,0x16,0x18
		case 6:
			ca = 0x0B;
			break; // 0x0B,0x0D,0x0E,0x10,0x17,0x19,0x1A,0x1C
		case 7:
			ca = 0x0f;
			break; // 0x0F,0x11,0x12,0x1B,0x1D,0x1E
		case 8:
			ca = 0x13;
			break; // 0x13,0x1F
		default:
			break;
		}
	}

	/* For allocation test 5-1-34, can't use on normal LPCM test */
	if (priv->force_ca & 0x80) {
		switch (priv->force_ca) {
		case 0x80:
			ca = 0x00;
			break; // 2ch
		case 0x81:
			ca = 0x01;
			break; // 4ch
		case 0x82:
			ca = 0x0B;
			break; // 6ch
		case 0x83:
			ca = 0x13;
			break; // 8ch
		default:
			break;
		}

		/* Clear for test */
		priv->force_ca = 0;
	}

	/* NOTE: Set channel allocation. */
	regmap_write(priv->regmap, IT6621_REG_TX_CH_ST9, ca);

	/*
	 * NOTE: DM_INH = 0, permitted or no information about any assertion
	 * of the down mixed stereo output.
	 *
	 * LSV3 = 0, LSV2 = 0, LSV1 = 0, LSV1 = 0, Level Shift Value is 0dB.
	 *
	 * LFEPBL1 = 0, LFEPBL0 = 0, unknown or refer to other information for
	 * LFE playback level comparing with other channel signal.
	 */
	regmap_write(priv->regmap, IT6621_REG_TX_CH_ST10, 0x00);

	if (priv->audio_ch > 8) {
		/*
		 * CTA-861-G spec. Table 29: Audio InfoFrame Format When Byte 4
		 * is 0xFE.
		 *
		 * Byte 6: FLW/   RLC/  FLC/  BC    BL/   FC    LFE1 FL/
		 *         FRW    RRC   FRC         BR               FR
		 * Byte 7: TpSiL/ SiL/  TpBC  LFE2  LS/   TpFC  TpC  TpFL/
		 *         TpSiR  SiR               RS               TpFR
		 * Byte 8: F87=0  F86=0 F85=0 F84=0 TpLS/ BtFL/ BtFC TpBL/
		 *                                  TpRS  BtFR       TpBR
		 */
		/* Set HDMI2.1 channel status */
		regmap_write(priv->regmap, IT6621_REG_TX_CH_ST11, 0x6f);
		regmap_write(priv->regmap, IT6621_REG_TX_CH_ST12, 0x0f);
		regmap_write(priv->regmap, IT6621_REG_TX_CH_ST13, 0x0c);
	} else {
		/* Reserved in CTA-861-G spec when Byte 4 is less than 0x32. */
		regmap_write(priv->regmap, IT6621_REG_TX_CH_ST11, 0x00);
		regmap_write(priv->regmap, IT6621_REG_TX_CH_ST12, 0x00);
		regmap_write(priv->regmap, IT6621_REG_TX_CH_ST13, 0x00);
	}

	/* Reserved in CTA-861-G spec. */
	regmap_write(priv->regmap, IT6621_REG_TX_CH_ST14, 0x00);
	regmap_write(priv->regmap, IT6621_REG_TX_CH_ST15, 0x00);

	return 0;
}

int it6621_set_arc_enabled(struct it6621_priv *priv, bool enabled)
{
	priv->arc_enabled = enabled ? IT6621_TX_ARC_EN : IT6621_TX_ARC_DIS;

	regmap_update_bits(priv->regmap, IT6621_REG_TX_DD0, IT6621_TX_ARC_SEL,
			   priv->arc_enabled);

	return 0;
}

int it6621_set_earc_enabled(struct it6621_priv *priv, bool enabled)
{
	priv->earc_enabled = enabled ? IT6621_TX_EARC_EN : IT6621_TX_EARC_DIS;

	regmap_update_bits(priv->regmap, IT6621_REG_TX_DD0, IT6621_TX_EARC_SEL,
			   priv->earc_enabled);

	return 0;
}

int it6621_set_enter_arc(struct it6621_priv *priv, bool enabled)
{
	unsigned int state;

	if (enabled) {
		regmap_update_bits(priv->regmap, IT6621_REG_TX_DD0,
				   IT6621_TX_ARC_NOW_SEL,
				   IT6621_TX_ARC_NOW_EN);
		it6621_get_ddfsm_state(priv, &state);
		if (state != IT6621_TX_ARC_MODE) {
			dev_err(priv->dev, "not in ARC mode\n");
			return -EAGAIN;
		}

		/* NOTE: ARC only support SPDIF */
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AUD_SRC_SEL,
				   IT6621_TX_AUD_SRC, IT6621_AUD_SRC_SPDIF);
		/* Common mode ARC */
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE3,
				   IT6621_TX_IN_ARC_MODE_SEL,
				   IT6621_TX_IN_ARC_COMMON_MODE);
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE3,
				   IT6621_TX_DRV_CSW_SEL, IT6621_TX_DRV_CSW_7);
		dev_dbg(priv->dev, "ARC now enabled\n");
	} else {
		regmap_update_bits(priv->regmap, IT6621_REG_TX_DD0,
				   IT6621_TX_ARC_NOW_SEL,
				   IT6621_TX_ARC_NOW_DIS);
		/* Signal mode ARC */
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE3,
				   IT6621_TX_IN_ARC_MODE_SEL,
				   IT6621_TX_IN_ARC_SIGNAL_MODE);
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE3,
				   IT6621_TX_DRV_CSW_SEL, IT6621_TX_DRV_CSW_4);
		dev_dbg(priv->dev, "ARC now disabled\n");
	}

	return 0;
}

static int it6621_toggle_real_hpd(struct it6621_priv *priv)
{
	/* toggle real HPD at least 100ms */
	/* Set HPDB low */
	regmap_update_bits(priv->regmap, IT6621_REG_SYS_CTRL1,
			   IT6621_TX_FORCE_HPDB_LOW_SEL,
			   IT6621_TX_FORCE_HPDB_LOW_EN);
	msleep(120);
	/* Set HPDB high */
	regmap_update_bits(priv->regmap, IT6621_REG_SYS_CTRL1,
			   IT6621_TX_FORCE_HPDB_LOW_SEL,
			   IT6621_TX_FORCE_HPDB_LOW_DIS);

	return 0;
}

static int it6621_set_auto_mode(struct it6621_priv *priv)
{
	/* auto mode , eARC auto detect */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_DD3,
			   IT6621_TX_FORCE_ARC_MODE_SEL,
			   IT6621_TX_FORCE_ARC_MODE_DIS);

	it6621_set_enter_arc(priv, false);

	return it6621_toggle_real_hpd(priv);
}

static int it6621_force_arc_mode(struct it6621_priv *priv)
{
	/*
	 * Force ARC mode , close eARC mode.
	 * Please recover to this HDMI RX's original EDID.
	 */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_DD3,
			   IT6621_TX_FORCE_ARC_MODE_SEL,
			   IT6621_TX_FORCE_ARC_MODE_EN);

	return it6621_toggle_real_hpd(priv);
}

int it6621_get_ddfsm_state(struct it6621_priv *priv, unsigned int *state)
{
	int ret;

	ret = regmap_read(priv->regmap, IT6621_REG_TX_DD2, state);
	*state &= IT6621_TX_DD_FSM_STATE;

	return ret;
}

static int it6621_wait_cmdc_bus(struct it6621_priv *priv)
{
	unsigned int status;
	unsigned int cmdc_state;
	unsigned int dd_state;
	unsigned int val;
	int ret;

	ret = regmap_read_poll_timeout(priv->regmap, IT6621_REG_INT_STATUS1,
				       status, status & (IT6621_TX_CMD_FAIL |
							 IT6621_TX_CMD_DONE),
				       1000, 10000);
	if (ret) {
		it6621_get_ddfsm_state(priv, &dd_state);
		regmap_read(priv->regmap, IT6621_REG_CMDC1, &val);
		cmdc_state = val & IT6621_TX_CMDC_STATE;
		dev_err(priv->dev, "timeout! TXDDFSM: %d, TXCMDFSM: %d\n",
			dd_state, cmdc_state);
	}

	if (status & IT6621_TX_CMD_DONE) {
		/* TX init done interrupt */
		regmap_write(priv->regmap, IT6621_REG_INT_STATUS1,
			     IT6621_TX_CMD_DONE);
		return 0;
	}

	if (status & IT6621_TX_CMD_FAIL) {
		regmap_write(priv->regmap, IT6621_REG_INT_STATUS1,
			     IT6621_TX_CMD_FAIL);
		dev_err(priv->dev, "TX CMD failed\n");

		regmap_read(priv->regmap, IT6621_REG_CMDC1, &val);

		if (val & IT6621_TX_FAIL_STATE_NO_RESP)
			dev_err(priv->dev, "no response\n");

		if (val & IT6621_TX_FAIL_STATE_UNEXP_RESP)
			dev_err(priv->dev, "unexpected response\n");

		if (val & IT6621_TX_FAIL_STATE_ECC_ERR)
			dev_err(priv->dev, "uncorrectable ECC error\n");

		if (val & IT6621_TX_FAIL_STATE_TIMEOUT) {
			if (priv->cmd_to_enabled)
				dev_err(priv->dev,
					"NACK/RETRY 256 times timeout\n");
			else
				dev_err(priv->dev, "NACK command state\n");
		}
	}

	return -EAGAIN;
}

static int it6621_read_cmdc_cmd(struct it6621_priv *priv, unsigned int devid,
				unsigned int offset, unsigned int *cmd)
{
	unsigned int state;

	it6621_get_ddfsm_state(priv, &state);
	if (state != IT6621_TX_EARC_MODE) {
		dev_err(priv->dev, "abort CMDC read\n");
		return -EAGAIN;
	}

	/* Data FIFO Clear */
	regmap_update_bits(priv->regmap, IT6621_REG_CMDC3,
			   IT6621_TX_DATA_FIFO_CLEAR,
			   IT6621_TX_DATA_FIFO_CLEAR);
	/* TxCmd DevID */
	regmap_write(priv->regmap, IT6621_REG_TX_DEV_ID, devid);
	/* TxCmd Offset */
	regmap_write(priv->regmap, IT6621_REG_TX_OFFSET, offset);
	/* Read Trigger (1-byte) */
	regmap_write(priv->regmap, IT6621_REG_CMDC2, IT6621_TX_READ_TRIGGER);

	it6621_wait_cmdc_bus(priv);

	return regmap_read(priv->regmap, IT6621_REG_TX_DATA_FIFO, cmd);
}

static void it6621_write_cmdc_cmd(struct it6621_priv *priv, unsigned int devid,
				  unsigned int offset, unsigned int data)
{
	unsigned int state;

	it6621_get_ddfsm_state(priv, &state);
	if (state != IT6621_TX_EARC_MODE) {
		dev_err(priv->dev, "abort CMDC write\n");
		return;
	}

	/* Data FIFO Clear */
	regmap_update_bits(priv->regmap, IT6621_REG_CMDC3,
			   IT6621_TX_DATA_FIFO_CLEAR,
			   IT6621_TX_DATA_FIFO_CLEAR);
	/* TxCmd DevID */
	regmap_write(priv->regmap, IT6621_REG_TX_DEV_ID, devid);
	/* TxCmd Offset */
	regmap_write(priv->regmap, IT6621_REG_TX_OFFSET, offset);
	/* TxCmd WrData */
	regmap_write(priv->regmap, IT6621_REG_TX_DATA_FIFO, data);
	/* Write Trigger (1-byte) */
	regmap_write(priv->regmap, IT6621_REG_CMDC2, IT6621_TX_WRITE_TRIGGER);

	it6621_wait_cmdc_bus(priv);
}

static int it6621_read_cmdc_bulk(struct it6621_priv *priv, unsigned int devid,
				 unsigned int offset, size_t len, void *data)
{
	unsigned int state;
	int ret;

	it6621_get_ddfsm_state(priv, &state);
	if (state != IT6621_TX_EARC_MODE) {
		dev_err(priv->dev, "abort CMDC bulk read\n");
		return -EAGAIN;
	}

	/* Data FIFO Clear */
	regmap_update_bits(priv->regmap, IT6621_REG_CMDC3,
			   IT6621_TX_DATA_FIFO_CLEAR,
			   IT6621_TX_DATA_FIFO_CLEAR);
	/* TxCmd DevID */
	regmap_write(priv->regmap, IT6621_REG_TX_DEV_ID, devid);
	/* TxCmd Offset */
	regmap_write(priv->regmap, IT6621_REG_TX_OFFSET, offset);
	/* Read Trigger & Read ByteNum[4:0] */
	regmap_write(priv->regmap, IT6621_REG_CMDC2,
		     IT6621_TX_READ_TRIGGER | (len - 1));

	ret = it6621_wait_cmdc_bus(priv);
	if (ret)
		return ret;

	regmap_noinc_read(priv->regmap, IT6621_REG_TX_DATA_FIFO, data, len);

	return 0;
}

/**
 * it6621_set_latency - eARC TX Set latency request
 *
 * @latency: latency in marco second, range from 0 to 250 ms.
 */
static void it6621_set_latency(struct it6621_priv *priv, unsigned int latency)
{
	it6621_write_cmdc_cmd(priv, 0x74, 0xd3, latency);
}

/**
 * it6621_get_latency - Read eARC RX's Latency
 *
 * @latency: latency in marco second, range from 0 to 250 ms.
 */
static int it6621_get_latency(struct it6621_priv *priv, unsigned int *latency)
{
	return it6621_read_cmdc_cmd(priv, 0x74, 0xd2, latency);
}

/**
 * it6621_reset_driver - should be set power down in ARC mode
 *
 * @on: power on/down differential mode signal.
 */
static void it6621_reset_driver(struct it6621_priv *priv, bool enabled)
{
	dev_dbg(priv->dev, "set TX driver %s\n", enabled ? "on" : "off");

	if (enabled)
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE2,
				   IT6621_TX_DRV_RESET_SEL,
				   IT6621_TX_DRV_RESET_DIS);
	else
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE2,
				   IT6621_TX_DRV_RESET_SEL,
				   IT6621_TX_DRV_RESET_EN);
}

static int it6621_check_audio(struct it6621_priv *priv)
{
	unsigned int val;

	if (test_bit(IT6621_AUDIO_TO_EN_DMAC, &priv->audio_flag) &&
	    test_bit(IT6621_AUDIO_START, &priv->audio_flag)) {
		if (test_bit(IT6621_EARC_BCLK_OK, &priv->events)) {
			/* Need to be finetune at diff platform */
			msleep(50);
			it6621_reset_driver(priv, true);
			clear_bit(IT6621_EARC_BCLK_OK, &priv->events);
			clear_bit(IT6621_AUDIO_TO_EN_DMAC, &priv->audio_flag);
			regmap_read(priv->regmap, IT6621_REG_TX_AFE2, &val);
			dev_dbg(priv->dev, "A2: 0x%02x\n", val);
		}

		if (priv->force_arc)
			it6621_set_enter_arc(priv, true);
	}

	return 0;
}

int it6621_earc_init(struct it6621_priv *priv)
{
	priv->state = IT6621_EARC_IDLE;
	priv->rclk = 0;
	priv->aclk = 0;
	priv->bclk = 0;

	/* Enable GRCLK */
	regmap_update_bits(priv->regmap, IT6621_REG_CLK_CTRL0,
			   IT6621_GATE_RCLK_SEL, IT6621_GATE_RCLK_DIS);

	/* TXAFE PLL Reset */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE0,
			   IT6621_TX_XP_RESET_SEL, IT6621_TX_XP_RESET_ACTIVE);

	/* eARCTX Reset */
	regmap_update_bits(priv->regmap, IT6621_REG_SYS_RESET,
			   IT6621_SW_RCLK_RESET_SEL, IT6621_SW_RCLK_RESET_EN);

	regmap_update_bits(priv->regmap, IT6621_REG_CLK_CTRL0,
			   IT6621_RCLK_FREQ_SEL, priv->rclk_sel);

	regmap_update_bits(priv->regmap, IT6621_REG_CLK_DET7,
			   IT6621_ACLK_DET_EN_SEL, IT6621_ACLK_DET_DIS);
	regmap_update_bits(priv->regmap, IT6621_REG_CLK_DET7,
			   IT6621_UPDATE_AVG_EN_SEL, priv->update_avg_enabled);
	regmap_update_bits(priv->regmap, IT6621_REG_CMDC4,
			   IT6621_TX_HB_RETRY_TIME_SEL |
			   IT6621_TX_HB_RETRY_SEL,
			   priv->hb_retry_sel | priv->hb_retry_enabled);
	/* NOTE: Disable to pass SL-870 HFR5-1-35/36/37 */
	regmap_update_bits(priv->regmap, IT6621_REG_CMDC5,
			   IT6621_TX_AUTO_WRITE_STATE_SEL,
			   IT6621_TX_AUTO_WRITE_STATE_DIS);
	regmap_update_bits(priv->regmap, IT6621_REG_DMAC_CTRL0,
			   IT6621_TX_ENC_OPT | IT6621_TX_ENC_SEL |
			   IT6621_TX_ECC_OPT,
			   priv->enc_opt | (priv->audio_enc << 3) |
			   priv->ecc_opt);
	regmap_write(priv->regmap, IT6621_REG_TX_ENC_SEED_LOW,
		     priv->enc_seed & 0xff);
	regmap_write(priv->regmap, IT6621_REG_TX_ENC_SEED_HIGH,
		     (priv->enc_seed & 0xff00) >> 8);
	regmap_update_bits(priv->regmap, IT6621_REG_DMAC_CTRL1,
			   IT6621_TX_U_BIT_OPT | IT6621_TX_C_CH_OPT |
			   IT6621_TX_PKT3_EN_SEL | IT6621_TX_PKT2_EN_SEL |
			   IT6621_TX_PKT1_EN_SEL,
			   priv->ubit_opt | priv->c_ch_opt |
			   priv->pkt3_enabled | priv->pkt2_enabled |
			   priv->pkt1_enabled);
	regmap_update_bits(priv->regmap, IT6621_REG_CMDC6,
			   IT6621_TX_RESYNC_OPT | IT6621_TX_FORCE_CMO_OPT |
			   IT6621_TX_CMO_OPT,
			   priv->resync_opt | priv->force_cmo_enabled |
			   priv->cmo_opt);
	regmap_update_bits(priv->regmap, IT6621_REG_CMDC0,
			   IT6621_TX_TURN_OVER_SEL |
			   IT6621_TX_NEXT_PKT_TIMEOUT_SEL,
			   priv->turn_over_sel |
			   priv->nxt_pkt_to_sel);
	regmap_update_bits(priv->regmap, IT6621_REG_TX_AUD_CTRL10,
			   IT6621_TX_2CH_LAYOUT_SEL | IT6621_TX_NLPCM_I2S_SEL |
			   IT6621_TX_MCH_LPCM_SEL | IT6621_TX_EXT_MUTE_SEL,
			   priv->layout_2ch_enabled | priv->i2s_nlpcm_enabled |
			   priv->mch_lpcm_enabled | IT6621_TX_EXT_MUTE_DIS);
	regmap_update_bits(priv->regmap, IT6621_REG_CLK_CTRL2,
			   IT6621_BCLK_INV_SEL | IT6621_ACLK_INV_SEL,
			   priv->bclk_inv_enabled | IT6621_ACLK_INV_DIS);
	regmap_update_bits(priv->regmap, IT6621_REG_TX_DD0,
			   IT6621_TX_ARC_NOW_SEL | IT6621_TX_ARC_SEL |
			   IT6621_TX_EARC_SEL,
			   priv->enter_arc_now | priv->arc_enabled |
			   priv->earc_enabled);

	if (priv->force_arc)
		regmap_update_bits(priv->regmap, IT6621_REG_TX_DD3,
				   IT6621_TX_FORCE_ARC_MODE_SEL,
				   IT6621_TX_FORCE_ARC_MODE_EN);
	else
		regmap_update_bits(priv->regmap, IT6621_REG_TX_DD3,
				   IT6621_TX_FORCE_ARC_MODE_SEL,
				   IT6621_TX_FORCE_ARC_MODE_DIS);

	/* test to disable TxXPLOCKChgInt */
	regmap_write(priv->regmap, IT6621_REG_INT_EN0,
		     IT6621_TX_RESYNC_ERR_EN | IT6621_TX_NO_AUD_CHANGE_EN |
		     IT6621_TX_DISV_TIMEOUT_EN | IT6621_TX_HB_LOST_EN |
		     IT6621_TX_STATE3_CHANGE_EN | IT6621_TX_HPDIO_OFF_EN |
		     IT6621_TX_HPDIO_ON_EN);
	regmap_write(priv->regmap, IT6621_REG_INT_EN1,
		     IT6621_TX_CMD_DONE_EN | IT6621_TX_CMD_FAIL_EN |
		     IT6621_TX_READ_STATE_CHG_EN | IT6621_TX_CMDC_BP_ERR_EN |
		     IT6621_TX_WRITE_STATE_CHG_EN | IT6621_TX_WRITE_CAP_CHG_EN |
		     IT6621_TX_HB_FAIL_EN);
	regmap_write(priv->regmap, IT6621_REG_INT_EN2,
		     IT6621_TX_MUTE_CHANGE_EN | IT6621_TX_SPDIF_CHANGE_EN |
		     IT6621_TX_SPDIF_READY_EN | IT6621_TX_DEC_ERR_EN |
		     IT6621_TX_FIFO_ERR_EN);
	/* enable RX HPD interrupt for HPDB interrupt */
	regmap_write(priv->regmap, IT6621_REG_INT_EN3,
		     IT6621_TX_HPDB_OFF_EN | IT6621_TX_HPDB_ON_EN);
	regmap_write(priv->regmap, IT6621_REG_INT_EN4,
		     IT6621_DET_NO_BCLK_EN | IT6621_DET_BCLK_VALID_EN |
		     IT6621_DET_BCLK_STABLE_EN | IT6621_DET_ACLK_VALID_EN |
		     IT6621_DET_ACLK_STABLE_EN);
	regmap_update_bits(priv->regmap, 0xa9, 0xFF, 0xFD);
	it6621_calc_rclk(priv);

	/* enable ACLK detection */
	regmap_update_bits(priv->regmap, IT6621_REG_CLK_DET7,
			   IT6621_ACLK_DET_EN_SEL, IT6621_ACLK_DET_EN);

	/* RTxDbgFifoClr */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_DEBUG_FIFO2,
			   IT6621_TX_DEBUG_FIFO_CLEAR_SEL,
			   IT6621_TX_DEBUG_FIFO_CLEAR_EN);
	regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE1,
			   IT6621_TX_XP_ER0_SEL | IT6621_TX_XP_DEI_SEL,
			   IT6621_TX_XP_ER0_DIS | IT6621_TX_XP_DEI_DIS);
	if (priv->revid == IT6621_REVISION_VARIANT_C0) {
		/*
		 * NOTE: In eARC mode, change DMAC back to B0 version,
		 * DRV_OT_SEL set to 1 for C0,TX_HYS_SEL = 5 for C0
		 */
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE2,
				   IT6621_TX_DRV_OT_SEL |
				   IT6621_TX_DRV_DSW_SEL |
				   IT6621_TX_DRV_DC,
				   IT6621_TX_DRV_OT_EN |
				   IT6621_TX_DRV_DSW_4 |
				   IT6621_TX_DRV_DC_SEL_DIS |
				   IT6621_TX_DRV_DC_DIS);
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE4,
				   IT6621_TX_HYS_SEL |
				   IT6621_TX_VCM_SEL |
				   IT6621_TX_RC_CK_SEL,
				   IT6621_TX_HYS5 |
				   priv->vcm_sel |
				   IT6621_TX_RC_CK_DIS);
	} else if (priv->revid == IT6621_REVISION_VARIANT_D0) {
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE2,
				   IT6621_TX_DRV_OT_SEL |
				   IT6621_TX_DRV_DSW_SEL |
				   IT6621_TX_DRV_DC,
				   IT6621_TX_DRV_OT_EN |
				   IT6621_TX_DRV_DSW_7 |
				   IT6621_TX_DRV_DC_SEL_DIS |
				   IT6621_TX_DRV_DC_DIS);
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE4,
				   IT6621_TX_HYS_SEL |
				   IT6621_TX_VCM_SEL |
				   IT6621_TX_RC_CK_SEL,
				   IT6621_TX_HYS5 |
				   priv->vcm_sel |
				   IT6621_TX_RC_CK_DIS);
	} else {
		/*
		 * NOTE: DRV_OT_SEL set to 0 force TM tri-state in
		 * ARC single mode (B0).
		 */
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE2,
				   IT6621_TX_DRV_OT_SEL |
				   IT6621_TX_DRV_DSW_SEL,
				   IT6621_TX_DRV_OT_DIS |
				   IT6621_TX_DRV_DSW_4);
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE4,
				   IT6621_TX_HYS_SEL |
				   IT6621_TX_VCM_SEL |
				   IT6621_TX_RC_CK_SEL,
				   IT6621_TX_HYS6 |
				   priv->vcm_sel |
				   IT6621_TX_RC_CK_DIS);
	}

	/* For D0 TX_DRV_CSR */
	if (priv->revid == IT6621_REVISION_VARIANT_D0)
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE3,
				   IT6621_TX_DRV_CSW_SEL |
				   IT6621_TX_DRV_CSR_SEL,
				   IT6621_TX_DRV_CSW_4 |
				   IT6621_TX_DRV_CSR_2);
	else
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE3,
				   IT6621_TX_DRV_CSW_SEL |
				   IT6621_TX_DRV_CSR_SEL,
				   IT6621_TX_DRV_CSW_4 |
				   IT6621_TX_DRV_CSR_4);

	regmap_update_bits(priv->regmap, IT6621_REG_SYS_RESET,
			   IT6621_SW_RCCLK_RESET_SEL |
			   IT6621_SW_TCCLK_RESET_SEL |
			   IT6621_SW_BCLK_RESET_SEL |
			   IT6621_SW_ACLK_RESET_SEL,
			   IT6621_SW_RCCLK_RESET_DIS |
			   IT6621_SW_TCCLK_RESET_DIS |
			   IT6621_SW_BCLK_RESET_DIS |
			   IT6621_SW_ACLK_RESET_DIS);

	/* Enable eARC TX Discovery and Disconnect FSM */
	regmap_update_bits(priv->regmap, IT6621_REG_TX_DD0,
			   IT6621_TX_DD_FSM_SEL, IT6621_TX_DD_FSM_EN);

	if (priv->force_arc) {
		dev_info(priv->dev, "force ARC mode\n");
		it6621_force_arc_mode(priv);
	}

	if (priv->force_earc) {
		dev_info(priv->dev, "force eARC mode\n");
		it6621_set_auto_mode(priv);
	}

	return 0;
}

static int it6621_read_rxcap(struct it6621_priv *priv)
{
	u8 rxcap[IT6621_RX_CAP_MAX_LEN];
	int i;
	int ret;

	/* Enable eARC TX Command Done/Fail Interrupt */
	regmap_update_bits(priv->regmap, IT6621_REG_INT_EN1,
			   IT6621_TX_CMD_FAIL_SEL | IT6621_TX_CMD_DONE_SEL,
			   IT6621_TX_CMD_FAIL_EN | IT6621_TX_CMD_DONE_EN);

	for (i = 0; i < 255; i += IT6621_RXCAP_BULK_LEN) {
		ret = it6621_read_cmdc_bulk(priv, 0xa0, i,
					    IT6621_RXCAP_BULK_LEN, &rxcap[i]);
		if (ret) {
			dev_err(priv->dev, "failed to read eARC RX Cap\n");
			return ret;
		}
	}

	mutex_lock(&priv->rxcap_lock);

	if (memcmp(rxcap, priv->rxcap, sizeof(priv->rxcap))) {
		memcpy(priv->rxcap, rxcap, sizeof(priv->rxcap));
		set_bit(IT6621_EARC_CAP_CHG, &priv->events);
	}

	mutex_unlock(&priv->rxcap_lock);

	return 0;
}

static void it6621_set_state(struct it6621_priv *priv, unsigned int state)
{
	if (priv->state != state) {
		priv->state = state;
		it6621_uapi_msg(priv, IT6621_EVENT_STATE_CHANGE, &priv->state,
				sizeof(priv->state));
	}
}

static int it6621_check_rxcap(struct it6621_priv *priv)
{
	if (test_bit(IT6621_EARC_CAP_CHG, &priv->events)) {
		clear_bit(IT6621_EARC_CAP_CHG, &priv->events);
		it6621_uapi_msg(priv, IT6621_EVENT_AUDIO_CAP_CHANGE,
				priv->rxcap, sizeof(priv->rxcap));
		cancel_work_sync(&priv->hpdio_work);
		schedule_work(&priv->hpdio_work);
	}

	return 0;
}

static int it6621_earc_irq(struct it6621_priv *priv)
{
	unsigned int status0;
	unsigned int status1;
	unsigned int status2;
	unsigned int status3;
	unsigned int status4;
	unsigned int state;
	unsigned int val;
	unsigned int latency = 0;

	regmap_read(priv->regmap, IT6621_REG_INT_STATUS0, &status0);
	regmap_read(priv->regmap, IT6621_REG_INT_STATUS1, &status1);
	regmap_read(priv->regmap, IT6621_REG_INT_STATUS2, &status2);
	regmap_read(priv->regmap, IT6621_REG_INT_STATUS3, &status3);
	regmap_read(priv->regmap, IT6621_REG_INT_STATUS4, &status4);
	regmap_write(priv->regmap, IT6621_REG_INT_STATUS0, status0);
	regmap_write(priv->regmap, IT6621_REG_INT_STATUS1, status1);
	regmap_write(priv->regmap, IT6621_REG_INT_STATUS2, status2);
	status3 &= IT6621_TX_HPDB_OFF | IT6621_TX_HPDB_ON;
	regmap_write(priv->regmap, IT6621_REG_INT_STATUS3, status3);
	regmap_write(priv->regmap, IT6621_REG_INT_STATUS4, status4);

	/* Interrupt status0 */
	if (status0 & IT6621_TX_HPDIO_ON) {
		dev_dbg(priv->dev, "HPD IO on\n");
		/* Set HPDB low clear */
		regmap_update_bits(priv->regmap, IT6621_REG_SYS_CTRL1,
				   IT6621_TX_FORCE_HPDB_LOW_SEL,
				   IT6621_TX_FORCE_HPDB_LOW_DIS);
	}

	if (status0 & IT6621_TX_HPDIO_OFF) {
		dev_dbg(priv->dev, "HPD IO off\n");

		if (test_bit(IT6621_ARC_START, &priv->events))
			it6621_set_enter_arc(priv, false);

		/* Set HPDB low */
		if (!priv->toggle_by_edid)
			regmap_update_bits(priv->regmap,
					   IT6621_REG_SYS_CTRL1,
					   IT6621_TX_FORCE_HPDB_LOW_SEL,
					   IT6621_TX_FORCE_HPDB_LOW_EN);
		else
			priv->toggle_by_edid = 0;

		clear_bit(IT6621_ARC_START, &priv->events);
	}

	if (status0 & IT6621_TX_STATE3_CHANGE) {
		dev_dbg(priv->dev, "eARC enable\n");

		it6621_get_ddfsm_state(priv, &state);
		dev_dbg(priv->dev, "eARC stat3 change to %d\n",
			state & IT6621_TX_EARC_MODE ? 1 : 0);
		clear_bit(IT6621_ARC_START, &priv->events);

		if (state & IT6621_TX_EARC_MODE)
			it6621_set_state(priv, IT6621_EARC_CONNECTED);
		else
			regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE2,
					   IT6621_TX_DRV_RESET_SEL,
					   IT6621_TX_DRV_RESET_EN);
	}

	if (status0 & IT6621_TX_HB_LOST) {
		dev_dbg(priv->dev, "heartbeat lost\n");
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE2,
				   IT6621_TX_DRV_RESET_SEL,
				   IT6621_TX_DRV_RESET_EN);
	}

	if (status0 & IT6621_TX_DISV_TIMEOUT) {
		dev_dbg(priv->dev, "discovery timeout\n");

		priv->events = 0;
		set_bit(IT6621_ARC_START, &priv->events);
		it6621_set_state(priv, IT6621_ARC_PENDING);
	}

	if (status0 & IT6621_TX_NO_AUD_CHANGE) {
		regmap_read(priv->regmap, IT6621_REG_SYS_STATUS, &val);
		val &= IT6621_TX_NO_AUDIO;
		dev_dbg(priv->dev, "TX has %s audio stream\n", val ? "no" : "");
	}

	if (status0 & IT6621_TX_XP_LOCK_CHANGE) {
		regmap_read(priv->regmap, IT6621_REG_SYS_STATUS, &val);
		val &= IT6621_TX_XP_LOCK;
		dev_dbg(priv->dev, "XP lock: %d\n", val >> 2);
	}

	if (status0 & IT6621_TX_RESYNC_ERR)
		dev_err(priv->dev, "CMDC resync error\n");

	/* Interrupt status1 */
	if (status1 & IT6621_TX_CMD_DONE)
		dev_dbg(priv->dev, "Cmd Done\n");

	if (status1 & IT6621_TX_HB_DONE)
		dev_dbg(priv->dev, "heartbeat done\n");

	if (status1 & IT6621_TX_READ_STATE_CHANGE) {
		regmap_read(priv->regmap, IT6621_REG_TX_READ_STATE, &val);
		dev_dbg(priv->dev, "RX state change to 0x%02X\n", val);
		val &= 0x18;
		regmap_update_bits(priv->regmap, IT6621_REG_CMDC5, 0x18, val);
	}

	if (status1 & IT6621_TX_CMDC_BP_ERR)
		dev_err(priv->dev, "CMDC bi-phase error\n");

	if (status1 & IT6621_TX_WRITE_STATE_CHANGE) {
		it6621_get_latency(priv, &latency);
		dev_dbg(priv->dev, "auto write STAT_CHNG 1->0 done\n");
		dev_dbg(priv->dev, "latency: 0x%02X ms\n", latency);
	}

	if (status1 & IT6621_TX_WRITE_CAP_CHANGE) {
		dev_dbg(priv->dev, "auto write CAP_CHNG  1->0 done\n");
		it6621_read_rxcap(priv);

		if (priv->audio_enc) {
			it6621_write_cmdc_cmd(priv, 0x9c, 0x10,
					      priv->enc_opt >> 4);
			it6621_write_cmdc_cmd(priv, 0x9c, 0x11,
					      priv->enc_seed & 0xff);
			it6621_write_cmdc_cmd(priv, 0x9c, 0x12,
					      (priv->enc_seed & 0xff00) >> 8);
		}

		/* Write 140ms to ERX_LATENCY_REQ */
		it6621_set_latency(priv, 140);
		regmap_read(priv->regmap, IT6621_REG_TX_AFE2, &val);
		val &= IT6621_TX_DRV_RESET_SEL;
		dev_dbg(priv->dev, "TX driver reset: %d\n", val >> 1);
	}

	if (status1 & IT6621_TX_HB_FAIL)
		dev_dbg(priv->dev, "heartbeat fail\n");

	/* Interrupt status2 */
	if (status2 & IT6621_TX_FIFO_ERR)
		dev_dbg(priv->dev, "audio FIFO error\n");

	if (status2 & IT6621_TX_DEC_ERR)
		dev_dbg(priv->dev, "audio decode error\n");

	if (status2 & IT6621_TX_SPDIF_READY)
		dev_dbg(priv->dev, "SPDIF channel status is ready\n");

	if (status2 & IT6621_TX_SPDIF_CHANGE)
		dev_dbg(priv->dev, "SPDIF channel status is changed\n");

	if (status2 & IT6621_TX_MUTE_CHANGE) {
		regmap_read(priv->regmap, IT6621_REG_SYS_STATUS, &val);
		val &= IT6621_TX_MUTE;
		dev_dbg(priv->dev, "input is %s\n", val ? "muted" : "unmuted");
	}

	/* Interrupt status3 */
	if (status3 & IT6621_TX_HPDB_ON) {
		dev_dbg(priv->dev, "HPD bus on\n");
		it6621_set_state(priv, IT6621_EARC_PENDING);
	}

	if (status3 & IT6621_TX_HPDB_OFF) {
		dev_dbg(priv->dev, "HPD bus off\n");
		regmap_update_bits(priv->regmap, IT6621_REG_TX_AFE2,
				   IT6621_TX_DRV_RESET_SEL,
				   IT6621_TX_DRV_RESET_EN);

		if (test_bit(IT6621_ARC_START, &priv->events))
			it6621_set_enter_arc(priv, false);

		clear_bit(IT6621_ARC_START, &priv->events);
		it6621_set_state(priv, IT6621_EARC_IDLE);
	}

	/* Interrupt status4 */
	if (status4 & IT6621_DET_ACLK_STABLE) {
		dev_dbg(priv->dev, "ACLK is stable\n");
		it6621_get_aclk(priv);
		it6621_force_pdiv(priv);
	}

	if (status4 & IT6621_DET_ACLK_VALID)
		dev_dbg(priv->dev, "ACLK is valid\n");

	if (status4 & IT6621_DET_NO_BCLK) {
		dev_dbg(priv->dev, "BCLK is not present\n");
		clear_bit(IT6621_EARC_BCLK_OK, &priv->events);
		it6621_reset_driver(priv, false);
	}

	if (status4 & IT6621_DET_BCLK_STABLE) {
		dev_dbg(priv->dev, "BCLK is stable\n");
		it6621_get_bclk(priv);
		set_bit(IT6621_EARC_BCLK_OK, &priv->events);
		if (test_bit(IT6621_AUDIO_START, &priv->audio_flag))
			set_bit(IT6621_AUDIO_TO_EN_DMAC, &priv->audio_flag);
	}

	if (status4 & IT6621_DET_BCLK_VALID)
		dev_dbg(priv->dev, "BCLK is valid\n");

	return 0;
}

irqreturn_t it6621_irq(int irq, void *data)
{
	struct it6621_priv *priv = data;

	it6621_earc_irq(priv);

	it6621_check_audio(priv);
	it6621_check_rxcap(priv);

	return IRQ_HANDLED;
}
