/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#ifndef _IT6621_UAPI_H
#define _IT6621_UAPI_H

/* States */
#define IT6621_EARC_IDLE		0x01
#define IT6621_EARC_PENDING		0x02
#define IT6621_ARC_PENDING		0x03
#define IT6621_EARC_CONNECTED		0x04

/* Events */
#define IT6621_EVENT_STATE_CHANGE	0x01 /* Event of state change, with a
					      * state following up.
					      */

#define IT6621_EVENT_AUDIO_CAP_CHANGE	0x02 /* Event of capabilities change,
					      * with a audio capabilities
					      * following up.
					      */

int it6621_uapi_init(struct it6621_priv *priv);
void it6621_uapi_remove(struct it6621_priv *priv);
int it6621_uapi_msg(struct it6621_priv *priv, u8 event, void *data, size_t len);

#endif /* _IT6621_UAPI_H */
