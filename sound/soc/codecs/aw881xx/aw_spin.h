/* SPDX-License-Identifier: GPL-2.0
 * aw_spin.h   aw codec driver
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 *
 *  Author: Bruce zhao <zhaolei@awinic.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __AW_SPIN_H__
#define __AW_SPIN_H__

/*#define AW_MTK_PLATFORM_SPIN*/
/*#define AW_QCOM_PLATFORM_SPIN*/

#define AW_DSP_TRY_TIME        (3)
#define AW_DSP_MSG_HDR_VER     (1)
#define AFE_MSG_ID_MSG_0       (0X10013D2A)
#define AFE_MSG_ID_MSG_1       (0X10013D2B)
#define AW_RX_PORT_ID          (0x1006)
#define AW_RX_TOPO_ID          (0x1000FF01)
#define AW_MSG_ID_SPIN         (0x10013D2E)
#define AW_INLINE_ID_AUDIO_MIX (0x0000000B)

enum {
	AW_REG_SPIN_ST = 0,
	AW_DSP_SPIN_ST,
};

enum {
	AW_DEV_CH_PRI_L = 0,
	AW_DEV_CH_PRI_R = 1,
	AW_DEV_CH_SEC_L = 2,
	AW_DEV_CH_SEC_R = 3,
	AW_DEV_CHAN_MAX,
};

enum aw_dsp_msg_type {
	AW_DSP_MSG_TYPE_DATA = 0,
	AW_DSP_MSG_TYPE_CMD = 1,
};

enum {
	AW_SPIN_OFF = 0,
	AW_SPIN_ON = 1,
};

enum {
	AW_SPIN_0 = 0,
	AW_SPIN_90,
	AW_SPIN_180,
	AW_SPIN_270,
	AW_SPIN_MAX,
};

enum {
	AW_SPIN_OFF_MODE = 0,
	AW_ADSP_SPIN_MODE,
	AW_REG_SPIN_MODE,
	AW_REG_MIXER_SPIN_MODE,
	AW_SPIN_MODE_MAX,
};

enum {
	AW_AUDIO_MIX_DISABLE = 0,
	AW_AUDIO_MIX_ENABLE,
};

struct aw_spin_ch {
	u16 rx_val;
	u16 tx_val;
};

struct aw_spin_desc {
	struct aw_spin_ch spin_table[AW_SPIN_MAX];
	u32 spin_mode;
};

int aw_spin_check_mode(struct aw_spin_desc *spin_desc);
int aw_spin_hold_chan_params(struct aw_spin_desc *spin_desc, u8 type);
void aw_add_spin_controls(void *aw_dev);
void aw_spin_init(struct aw_spin_desc *spin_desc);

#endif
