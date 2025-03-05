/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Jason Zhang <jason.zhang@rock-chips.com>
 */

#ifndef _IT6621_H
#define _IT6621_H

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <sound/asoundef.h>

#define IT6621_VENDOR_ID		0x4954
#define IT6621_DEVICE_ID		0x6621
#define IT6621_REVISION_VARIANT_B0	0xb0
#define IT6621_REVISION_VARIANT_C0	0xc0
#define IT6621_REVISION_VARIANT_D0	0xd0

#define IT6621_ARC_START		0
#define IT6621_EARC_CAP_CHG		1
#define IT6621_EARC_EDID_OK		2
#define IT6621_EARC_BCLK_OK		3

#define IT6621_AUDIO_START		0
#define IT6621_AUDIO_TO_EN_DMAC		7

#define IT6621_CMD_WAIT_TIME_MS		1
#define IT6621_CMD_WAIT_COUNT		10
#define IT6621_RXCAP_BULK_LEN		16
#define IT6621_RX_CAP_MAX_LEN		256
#define IT6621_ADB_MAX_LEN		32

/**
 * struct it6621_priv - IT6621 device
 * @client: the i2c client used by the driver.
 * @dev: i2c device.
 * @mclk: master clock.
 * @hpdio: Input Hot Plus Detection.
 * @hpdio_lock: hpdio lock to protect hpdio access.
 * @rxcap_lock: rxcap lock to protect rxcap access.
 * @state: the state of IT6621 device.
 * @vid: vendor ID.
 * @devid: device ID.
 * @revid: revision ID.
 * @audio_enc: LPCM 2-ch do not support encryption.
 * @audio_src: I2S, SPDIF, TDM, DSD (DSD: EnAudGen must be TRUE for
 *             Internal AudGen).
 * @audio_ch: channel number.
 * @audio_fs: sample frequency.
 * @audio_type: LPCM, NLPCM.
 * @audio_hbr: HBR.
 * @audio_input_hbr: input audio HBR.
 * @i2s_wl: 0 for 16-bit, 1 for 18-bit, 2 for 20-bit, 3 for 24-bit.
 * @i2s_hbr: I2S HBR.
 * @i2s_fmt: I2S format.
 * @i2s_nlpcm_enabled: auto by input setting.
 * @i2s_hbr_enabled: enable eARC TX I2S hbr.
 * @hbr_layb_enabled: QD980 HFR5-1-40.
 * @mch_lpcm_enabled: SL-870 5-1-30 [Multi-channel 2-ch layout] has audio output
 * @layout_2ch_enabled: use for SL-870 5-1-29 [Multi-channel 2-ch layout] =>
 *                      fixed by SL-870 @ FW Ver1.11 => (always FALSE)
 * @vcm_sel: tx VCM selection.
 * @force_ca: force channel allocation.
 * @force_16ch: force to 16 channels.
 * @toggle_by_edid: indicate whether HPD is toggled by EDID.
 * @rclk: RCLK frequency.
 * @aclk: ACLK frequency.
 * @bclk: BCLK frequency.
 * @rclk_sel: RCLK frequency selection.
 * @update_avg_enabled: enable update average clock detection value.
 * @cmo_opt: eARC TX common mode output enable option.
 * @force_cmo_enabled: force eARC TX common mode enable.
 * @resync_opt: eARC TX common mode resync option.
 * @ubit_opt: U-bit option.
 * @c_ch_opt: eARC TX U-bit C-Ch option.
 * @ecc_opt: eARC TX ECC option.
 * @enc_seed: eARC TX encryption seed.
 * @enc_opt: eARC TX encryption option.
 * @fixed_lcf: fixed LC frequency.
 * @bclk_inv_enabled: enable eARC BCLK inversion.
 * @sck_inv_enabled: enable I2S serial clock inversion.
 * @tck_inv_enabled: enable TDM clock inversion.
 * @mclk_inv_enabled: enable SPDIF master clock inversion.
 * @dclk_inv_enabled: enable DSD clock inversion.
 * @nxt_pkt_to_sel: eARC TX next packet timeout selection.
 * @turn_over_sel: eARC TX turn-over time selection before
 *                    transmitting packet, set 24 us for QD980 HFR5-1-21.
 * @hb_retry_sel: eARC TX HeartBeat retry time selection.
 * @hb_retry_enabled: enable eARC TX auto HeartBeat retry.
 * @cmd_to_enabled: indicate whether command timeout is enabled.
 * @force_arc: force TX ARC mode.
 * @enter_arc_now: enter ARC mode when initiate IT6621 device,
 *                 enable for standalone ARC mode.
 * @arc_enabled: enable ARC mode.
 * @earc_enabled: enable eARC mode, disable for standalone ARC mode.
 * @pkt1_enabled: enable eARC TX packet 1 using U-bit.
 * @pkt2_enabled: enable eARC TX packet 2 using U-bit.
 * @pkt3_enabled: enable eARC TX packet 3 using U-bit.
 * @events: eARC events.
 * @audio_flag: upstream's audio state, if upstream is audio on, set to 1 at
 *              upstream, extern to upstream's function.
 * @hpdio_work: work to toggle hpdio.
 * @config_audio: indicate whether audio is need to be configured.
 * @force_mute: force mute.
 * @uapi_registered: indicate whether uapi is registered.
 * @rxcap: capabilities from the ARC device.
 * @adb: audio data block.
 */
struct it6621_priv {
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;
	struct miscdevice mdev;
	struct clk *mclk;
	struct gpio_desc *hpdio;
	struct list_head fhs;
	struct mutex fhs_lock;
	struct mutex hpdio_lock;
	struct mutex rxcap_lock;
	unsigned int state;
	unsigned int vid;
	unsigned int devid;
	unsigned int revid;

	/* Tx Audio Option */
	unsigned int audio_enc;
	unsigned int audio_src;
	unsigned int audio_ch;
	unsigned int audio_fs;
	unsigned int audio_type;
	unsigned int audio_hbr;
	unsigned int audio_input_hbr;
	unsigned int i2s_wl;
	unsigned int i2s_hbr;
	unsigned int i2s_fmt;
	unsigned int i2s_nlpcm_enabled;
	unsigned int i2s_hbr_enabled;
	unsigned int hbr_layb_enabled;
	unsigned int mch_lpcm_enabled;
	unsigned int layout_2ch_enabled;
	unsigned int vcm_sel;
	unsigned int force_ca;
	unsigned int force_16ch;

	/* FIXME: Need to toggle by edid? */
	unsigned int toggle_by_edid;
	unsigned int rclk;
	unsigned int aclk;
	unsigned int bclk;

	/* Clock configurations */
	unsigned int rclk_sel;
	unsigned int update_avg_enabled;

	unsigned int cmo_opt;
	unsigned int force_cmo_enabled;
	unsigned int resync_opt;
	unsigned int ubit_opt;
	unsigned int c_ch_opt;
	unsigned int ecc_opt;
	unsigned int enc_seed;
	unsigned int enc_opt;
	unsigned int fixed_lcf;

	/* Clock inversion option */
	unsigned int bclk_inv_enabled;
	unsigned int sck_inv_enabled;
	unsigned int tck_inv_enabled;
	unsigned int mclk_inv_enabled;
	unsigned int dclk_inv_enabled;

	/* DMCD option */
	unsigned int nxt_pkt_to_sel;
	unsigned int turn_over_sel;

	/* TX CMDC option */
	unsigned int hb_retry_sel;
	unsigned int hb_retry_enabled;
	unsigned int cmd_to_enabled;

	/* Discovery option */
	unsigned int force_arc;
	unsigned int force_earc;
	unsigned int enter_arc_now;
	unsigned int arc_enabled;
	unsigned int earc_enabled;

	/* Loop Test option */
	unsigned int pkt1_enabled;
	unsigned int pkt2_enabled;
	unsigned int pkt3_enabled;

	unsigned long events;
	unsigned long audio_flag;

	struct work_struct hpdio_work;
	struct class class;
	bool config_audio;
	bool force_mute;
	bool uapi_registered;
	u8 rxcap[IT6621_RX_CAP_MAX_LEN];
	u8 adb[IT6621_ADB_MAX_LEN];
};

#endif /* _IT6621_H */
