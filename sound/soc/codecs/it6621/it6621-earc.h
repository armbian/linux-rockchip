/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#ifndef _IT6621_EARC_H
#define _IT6621_EARC_H

#include "it6621.h"

#define IT6621_AES3_CON_FS_64000	0x0b
#define IT6621_AES3_CON_FS_128000	0x2b
#define IT6621_AES3_CON_FS_256000	0x1b
#define IT6621_AES3_CON_FS_352000	0x0d
#define IT6621_AES3_CON_FS_384000	0x05
#define IT6621_AES3_CON_FS_512000	0x3b
#define IT6621_AES3_CON_FS_705000	0x2d
#define IT6621_AES3_CON_FS_1024000	0x35
#define IT6621_AES3_CON_FS_1411000	0x1d
#define IT6621_AES3_CON_FS_1536000	0x15

#define IT6621_AUD_TYPE_LPCM		0
#define IT6621_AUD_TYPE_NLPCM		1
#define IT6621_AUD_TYPE_HBR		2
#define IT6621_AUD_TYPE_DSD		3

/* HDMI2.1 spec. Table 9-23: Channel Status Bits 0, 1, 3, 4, and 5 */
#define HDMI_AUDIO_FMT_UN_2CH_LPCM	0x00 /* Unencrypted 2-channel LPCM */
#define HDMI_AUDIO_FMT_UN_MCH_LPCM	0x10 /* Unencrypted multi-channel LPCM */
#define HDMI_AUDIO_FMT_UN_XCH_DSD	0x18 /* Unencrypted One Bit Audio */
#define HDMI_AUDIO_FMT_UN_2CH_NLPCM	0x02 /* Unencrypted IEC 61937 */
#define HDMI_AUDIO_FMT_EN_2CH_NLPCM	0x06 /* Encrypted IEC 61937 */
#define HDMI_AUDIO_FMT_EN_MCH_NLPCM	0x16 /* Encrypted multi-channel LPCM */
#define HDMI_AUDIO_FMT_EN_XCH_DSD	0x1e /* Encrypted One Bit Audio */

/*
 * HDMI2.1 spec.
 * Table 9-25: Channel Status Bits 44 to 47 When Multi-Channel L-PCM is
 * Transmitted.
 * Table 9-26: Channel Status Bits 44 to 47 When One Bit Audio is Transmitted.
 * Table 9-27: Channel Status Bits 44 to 47 When IEC 61937 (Compressed) Audio
 * is Transmitted.
 */
#define HDMI_AUDIO_LAYOUT_LPCM_2CH	0x00
#define HDMI_AUDIO_LAYOUT_LPCM_8CH	0x07
#define HDMI_AUDIO_LAYOUT_LPCM_16CH	0x0b
#define HDMI_AUDIO_LAYOUT_LPCM_32CH	0x03
#define HDMI_AUDIO_LAYOUT_NLPCM_2CH	0x00
#define HDMI_AUDIO_LAYOUT_NLPCM_8CH	0x07
#define HDMI_AUDIO_LAYOUT_DSD_6CH	0x05
#define HDMI_AUDIO_LAYOUT_DSD_12CH	0x09

int it6621_set_arc_enabled(struct it6621_priv *priv, bool enabled);
int it6621_set_earc_enabled(struct it6621_priv *priv, bool enabled);
int it6621_set_enter_arc(struct it6621_priv *priv, bool enabled);
int it6621_get_ddfsm_state(struct it6621_priv *priv, unsigned int *state);
int it6621_set_channel_status(struct it6621_priv *priv);
int it6621_earc_init(struct it6621_priv *priv);
irqreturn_t it6621_irq(int irq, void *data);

#endif /* _IT6621_EARC_H */
