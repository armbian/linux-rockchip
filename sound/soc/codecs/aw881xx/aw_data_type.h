/* SPDX-License-Identifier: GPL-2.0
 * aw_data_type.h   aw codec driver
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

#ifndef __AW_DATA_TYPE_H__
#define __AW_DATA_TYPE_H__

#define AW_NAME_BUF_MAX (50)

/******************************************************************
 * aw profile
 *******************************************************************/
#define PROJECT_NAME_MAX  (24)
#define CUSTOMER_NAME_MAX (16)
#define CFG_VERSION_MAX   (4)
#define DEV_NAME_MAX      (16)
#define PROFILE_STR_MAX   (32)

#define ACF_FILE_ID (0xa15f908)

typedef enum {
	AW_RECORD_SEC_DATA = 0,
	AW_RECOVERY_SEC_DATA = 1,
} backup_sec_t;

typedef enum {
	AW_GET_DEV_PARAMS = 0,
	AW_SET_DEV_PARAMS = 1,
} params_option_t;

typedef enum {
	AW_GET_DEV_STATUS = 0,
	AW_SET_DEV_STATUS = 1,
} status_option_t;

typedef enum {
	AW_DEV_NONE_MSG = 0,
	AW_DEV_DSP_PARAMS,
	AW_DEV_HMUTE_PARAMS,
	AW_DEV_INT_PARAMS,
	AW_DEV_VOLUME_PARAMS,
	AW_DEV_FADE_STEP_PARAMS,
	AW_DEV_FADE_IN_TIME_PARAMS,
	AW_DEV_FADE_OUT_TIME_PARAMS,
	AW_DEV_CHANNEL_MODE_PARAMS,
	AW_DEV_CRC_FLAG_PARAMS,
	AW_DEV_SYSST_PARAMS,
	AW_DEV_CALI_RE_PARAMS,
	AW_DEV_BIN_RE_PARAMS,
	AW_DEV_TE_PARAMS,
	AW_DEV_HOLD_SPIN_PARAMS,
	AW_DEV_REALTIME_CRC_GET_PARAMS,
	AW_DEV_REALTIME_CRC_SET_PARAMS,
	AW_DEV_DSP_NOISE_GATE_PARAMS,
	AW_DEV_DSP_LOW_POWER_PARAMS,
} dev_params_t;

typedef enum {
	AW_DEV_NONE_STATUS = 0,
	AW_DEV_PLL_WDT_STATUS,
	AW_DEV_CLEAR_INT_STATUS,
	AW_DEV_CHECK_SPIN_MODE_STATUS,
} dev_status_t;

struct aw_msg_hdr {
	int32_t type;
	int32_t opcode_id;
	int32_t version;
	int32_t reseriver[3];
};

enum aw_cfg_hdr_version {
	AW_CFG_HDR_VER_0_0_0_1 = 0x00000001,
	AW_CFG_HDR_VER_1_0_0_0 = 0x01000000,
};

enum aw_cfg_dde_type {
	AW_DEV_NONE_TYPE_ID = 0xFFFFFFFF,
	AW_DEV_TYPE_ID = 0x00000000,
	AW_SKT_TYPE_ID = 0x00000001,
	AW_DEV_DEFAULT_TYPE_ID = 0x00000002,
};

enum aw_sec_type {
	ACF_SEC_TYPE_REG = 0,
	ACF_SEC_TYPE_DSP,
	ACF_SEC_TYPE_DSP_CFG,
	ACF_SEC_TYPE_DSP_FW,
	ACF_SEC_TYPE_HDR_REG,
	ACF_SEC_TYPE_HDR_DSP_CFG,
	ACF_SEC_TYPE_HDR_DSP_FW,
	ACF_SEC_TYPE_MUTLBIN,
	ACF_SEC_TYPE_SKT_PROJECT,
	ACF_SEC_TYPE_DSP_PROJECT,
	ACF_SEC_TYPE_MONITOR,
	ACF_SEC_TYPE_MAX,
};

enum profile_data_type {
	AW_DATA_TYPE_REG = 0,
	AW_DATA_TYPE_DSP_CFG,
	AW_DATA_TYPE_DSP_FW,
	AW_DATA_TYPE_MAX,
};

enum aw_prof_type {
	AW_PROFILE_MUSIC = 0,
	AW_PROFILE_VOICE,
	AW_PROFILE_VOIP,
	AW_PROFILE_RINGTONE,
	AW_PROFILE_RINGTONE_HS,
	AW_PROFILE_LOWPOWER,
	AW_PROFILE_BYPASS,
	AW_PROFILE_MMI,
	AW_PROFILE_FM,
	AW_PROFILE_NOTIFICATION,
	AW_PROFILE_RECEIVER,
	AW_PROFILE_MAX,
};

enum aw_profile_status {
	AW_PROFILE_WAIT = 0,
	AW_PROFILE_OK,
};

struct aw_cfg_hdr {
	u32 a_id;                    /*acf file ID 0xa15f908*/
	char a_project[PROJECT_NAME_MAX]; /*project name*/
	char a_custom[CUSTOMER_NAME_MAX]; /*custom name :huawei xiaomi vivo oppo*/
	char a_version[CFG_VERSION_MAX];  /*author update version*/
	u32 a_author_id;             /*author id*/
	u32 a_ddt_size;              /*sub section table entry size*/
	u32 a_ddt_num;               /*sub section table entry num*/
	u32 a_hdr_offset;            /*sub section table offset in file*/
	u32 a_hdr_version;           /*sub section table version*/
	u32 reserve[3];
};

struct aw_cfg_dde {
	u32 type;        /*DDE type id*/
	char dev_name[DEV_NAME_MAX];
	u16 dev_index;   /*dev id*/
	u16 dev_bus;     /*dev bus id*/
	u16 dev_addr;    /*dev addr id*/
	u16 dev_profile; /*dev profile id*/
	u32 data_type;   /*data type id*/
	u32 data_size;
	u32 data_offset;
	u32 data_crc;
	u32 reserve[5];
};

struct aw_cfg_dde_v_1_0_0_0 {
	u32 type;        /*DDE type id*/
	char dev_name[DEV_NAME_MAX];
	u16 dev_index;   /*dev id*/
	u16 dev_bus;     /*dev bus id*/
	u16 dev_addr;    /*dev addr id*/
	u16 dev_profile; /*dev profile id*/
	u32 data_type;   /*data type id*/
	u32 data_size;
	u32 data_offset;
	u32 data_crc;
	char dev_profile_str[PROFILE_STR_MAX];
	u32 chip_id;
	u32 reserve[4];
};

struct aw_sec_data_desc {
	u32 len;
	unsigned char *data;
};

struct aw_prof_desc {
	u32 id;
	u32 prof_st;
	char *prf_str;
	u32 fw_ver;
	struct aw_sec_data_desc sec_desc[AW_DATA_TYPE_MAX];
};

struct aw_all_prof_info {
	struct aw_prof_desc prof_desc[AW_PROFILE_MAX];
};

struct aw_prof_info {
	int count;
	int prof_type;
	char **prof_name_list;
	struct aw_prof_desc *prof_desc;
};

#endif
