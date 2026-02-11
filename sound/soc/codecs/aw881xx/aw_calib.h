/* SPDX-License-Identifier: GPL-2.0
 * aw_calib.h   aw codec driver
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

#ifndef __AW_CALIBRATION_H__
#define __AW_CALIBRATION_H__

/*#define AW_CALI_STORE_EXAMPLE*/
#include "aw_data_type.h"

#define AW_CALI_STORE_EXAMPLE
#define AW_ERRO_CALI_RE_VALUE (0)
#define AW_ERRO_CALI_F0_VALUE (2600)

#define AW_CALI_RE_DEFAULT_TIMER (3000)
#define MSGS_SIZE                (512)
#define RESERVED_SIZE            (252)

#define AW_CALI_ALL_DEV (0xFFFFFFFF)

#define AW_CALI_RE_MAX                         (15000)
#define AW_CALI_RE_MIN                         (4000)
#define AW_CALI_CFG_NUM                         (4)
#define AW_CALI_F0_DATA_NUM                      (4)
#define AW_CALI_READ_CNT_MAX                     (8)
#define AW_CALI_DATA_SUM_RM                       (2)
#define AW_DSP_RE_TO_SHOW_RE(re, shift)        (((re) * (1000)) >> (shift))
#define AW_SHOW_RE_TO_DSP_RE(re, shift)        (((re) << shift) / (1000))
#define AW_CALI_F0_TIME                           (5 * 1000)
#define F0_READ_CNT_MAX                             (5)
#define AW_FS_CFG_MAX                               (11)
#define AW_DEV_CH_MAX                               (16)
#define AW_DEV_RE_RANGE                         (RE_RANGE_NUM * AW_DEV_CH_MAX)
#define AW_TE_CACL_VALUE(te, coil_alpha) \
	((int32_t)(((int32_t)te << 18) / (coil_alpha)))
#define AW_RE_REALTIME_VALUE(re_cacl, te_cacl) \
	((re_cacl) + (int32_t)((int64_t)((te_cacl) * (re_cacl)) >> 14))

enum {
	CALI_CHECK_DISABLE = 0,
	CALI_CHECK_ENABLE = 1,
};

enum {
	CALI_RESULT_NONE = 0,
	CALI_RESULT_NORMAL = 1,
	CALI_RESULT_ERROR = -1,
};

enum {
	CALI_OPS_HMUTE = 0X0001,
	CALI_OPS_NOISE = 0X0002,
};

enum {
	CALI_TYPE_RE = 0,
	CALI_TYPE_F0,
};

enum {
	AW_GET_RE_FROM_BIN = 0,
	AW_GET_RE_FROM_PA,
};

enum {
	GET_RE_TYPE = 0,
	GET_F0_TYPE,
	GET_Q_TYPE,
};

enum {
	AW_CALI_CMD_RE = 0,
	AW_CALI_CMD_F0,
	AW_CALI_CMD_RE_F0,
	AW_CALI_CMD_F0_Q,
	AW_CALI_CMD_RE_F0_Q,
};

enum {
	CALI_STR_NONE = 0,
	CALI_STR_CALI_RE_F0,
	CALI_STR_CALI_RE,
	CALI_STR_CALI_F0,
	CALI_STR_SET_RE,
	CALI_STR_SHOW_RE,      /*show cali_re*/
	CALI_STR_SHOW_R0,      /*show real r0*/
	CALI_STR_SHOW_CALI_F0, /*GET DEV CALI_F0*/
	CALI_STR_SHOW_F0,      /*SHOW REAL F0*/
	CALI_STR_SHOW_TE,
	CALI_STR_DEV_SEL,      /*switch device*/
	CALI_STR_VER,
	CALI_STR_SHOW_RE_RANGE,
	CALI_STR_MAX,
};

enum {
	RE_MIN_FLAG = 0,
	RE_MAX_FLAG = 1,
	RE_RANGE_NUM = 2,
};

enum {
	AW_CALI_MODE_NONE = 0,
	AW_CALI_MODE_ALL,
	AW_CALI_MODE_MAX,
};

struct re_data {
	u32 re_range[2];
};

#define AW_IOCTL_MAGIC        'a'
#define AW_IOCTL_GET_F0       _IOWR(AW_IOCTL_MAGIC, 5, int32_t)
#define AW_IOCTL_SET_CALI_RE  _IOWR(AW_IOCTL_MAGIC, 6, int32_t)
#define AW_IOCTL_GET_RE       _IOWR(AW_IOCTL_MAGIC, 17, int32_t)
#define AW_IOCTL_GET_CALI_F0  _IOWR(AW_IOCTL_MAGIC, 18, int32_t)
#define AW_IOCTL_GET_REAL_R0  _IOWR(AW_IOCTL_MAGIC, 19, int32_t)
#define AW_IOCTL_GET_TE       _IOWR(AW_IOCTL_MAGIC, 20, int32_t)
#define AW_IOCTL_GET_RE_RANGE _IOWR(AW_IOCTL_MAGIC, 21, struct re_data)

struct cali_cfg {
	u32 data[AW_CALI_CFG_NUM];
};

struct aw_cali_cfg_desc {
	unsigned int actampth_reg;
	unsigned char actampth_data_type;

	unsigned int noiseampth_reg;
	unsigned char noiseampth_data_type;

	unsigned int ustepn_reg;
	unsigned char ustepn_data_type;

	unsigned int alphan_reg;
	unsigned int alphan_data_type;
};

struct aw_noise_desc {
	unsigned int dsp_reg;
	unsigned char data_type;
	unsigned int mask;
};

struct aw_f0_desc {
	unsigned int dsp_reg;
	unsigned char data_type;
	unsigned int shift;
};

struct aw_q_desc {
	unsigned int dsp_reg;
	unsigned char data_type;
	unsigned int shift;
};

struct aw_dsp_cali_re_desc {
	unsigned int dsp_reg;
	unsigned char data_type;
	unsigned int shift;
};

struct aw_r0_desc {
	unsigned int dsp_reg;
	unsigned char data_type;
	unsigned int shift;
	unsigned int init_value;
};

struct aw_hw_cali_re_desc {
	unsigned int hbits_reg;
	unsigned int lbits_reg;
	unsigned int hbits_mask;
	unsigned int hbits_shift;
	unsigned int lbits_mask;
	unsigned int lbits_shift;
	unsigned int cali_re_shift;
	u16 re_hbits;
	u16 re_lbits;
};

struct aw_ra_desc {
	unsigned int dsp_reg;
	unsigned char data_type;
	unsigned int shift;
};

struct aw_spkr_temp_desc {
	unsigned int reg;
};

struct aw_cali_delay_desc {
	unsigned int dsp_reg;
	unsigned char data_type;
	unsigned int delay;
};

struct aw_cali_iv_desc {
	unsigned int reg;
	unsigned int reabs_mask;
};

struct aw_cali_backup_desc {
	/* dsp ng info */
	unsigned int dsp_ng_cfg;
	unsigned int dsp_lp_cfg; /* low power */
};

struct aw_cali_desc {
	bool status;
	struct cali_cfg cali_cfg;
	u16 store_vol;
	u32 cali_re; /*cali value*/
	u32 f0;
	u32 q;
	u32 ra;
	int8_t cali_result;
	u8 cali_check_st;
	int8_t mode; /*0:NONE 1:ATTR/CLASS/MISC */

	struct aw_cali_cfg_desc cali_cfg_desc;
	struct aw_ra_desc ra_desc;
	struct aw_noise_desc noise_desc;
	struct aw_f0_desc f0_desc;
	struct aw_q_desc q_desc;
	struct aw_dsp_cali_re_desc dsp_re_desc;
	struct aw_r0_desc r0_desc;
	struct aw_spkr_temp_desc spkr_temp_desc;
	struct aw_hw_cali_re_desc hw_cali_re_desc;
	struct aw_cali_delay_desc cali_delay_desc;
	struct aw_cali_iv_desc iv_desc;
	struct aw_cali_backup_desc backup_info;
};

void aw_cali_init(struct aw_cali_desc *cali_desc);
void aw_cali_deinit(struct aw_cali_desc *cali_desc);
bool aw_cali_get_cali_status(struct aw_cali_desc *cali_desc);
int aw_cali_set_cali_re(struct aw_cali_desc *cali_desc);
int aw_cali_get_cali_re(struct aw_cali_desc *cali_desc, u32 *re, u32 type);
int aw_cali_get_ra(struct aw_cali_desc *cali_desc);
int aw_cali_get_te(struct aw_cali_desc *cali_desc, int32_t *te);
bool aw_cali_check_result(struct aw_cali_desc *cali_desc);
int aw_dev_init_re_update(struct aw_cali_desc *cali_desc, backup_sec_t flag);

#endif
