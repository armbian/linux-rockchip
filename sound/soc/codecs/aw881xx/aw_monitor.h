/* SPDX-License-Identifier: GPL-2.0
 * aw_monitor.h   aw codec driver
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

#ifndef __AW_MONITOR_H__
#define __AW_MONITOR_H__

/*#define AW_DEBUG*/
/*#define AW_SYS_BATTERY_ST*/
#include "aw_data_type.h"

struct aw_table;

#define AW_TABLE_SIZE           sizeof(struct aw_table)
#define AW_MONITOR_DEFAULT_FLAG (0)

#define IPEAK_NONE (0xFF)
#define GAIN_NONE  (0xFF)
#define VMAX_NONE  (0xFFFFFFFF)

#define AW_GET_32_DATA(w, x, y, z) \
	((u32)((((u8)w) << 24) | (((u8)x) << 16) | \
	(((u8)y) << 8) | ((u8)z)))
#define AW_GET_16_DATA(x, y) \
	((u16)((((u8)x) << 8) | (u8)y))

typedef enum {
	AW_GET_INIT_VMAX = 0,
	AW_SET_INIT_VMAX,
	AW_HW_MONITOR_ST,
	AW_DSP_MONITOR_ST,
} monitor_update_t;

enum {
	AW_MONITOR_DISABLE = 0,
	AW_MONITOR_ENABLE,
};

enum {
	AW_MON_LOGIC_OR = 0,
	AW_MON_LOGIC_AND = 1,
};

enum {
	AW_FIRST_ENTRY = 0,
	AW_NOT_FIRST_ENTRY = 1,
};

enum aw_monitor_hdr_ver {
	AW_MONITOR_HDR_VER_0_1_1 = 0x00010100,
};

enum aw_monitor_init {
	AW_MON_CFG_ST = 0,
	AW_MON_CFG_OK = 1,
};

#define MONITOR_EN_MASK 0x01

enum {
	MONITOR_EN_BIT = 0,
	MONITOR_LOGIC_BIT = 1,
	MONITOR_IPEAK_EN_BIT = 2,
	MONITOR_GAIN_EN_BIT = 3,
	MONITOR_VMAX_EN_BIT = 4,
	MONITOR_TEMP_EN_BIT = 5,
	MONITOR_VOL_EN_BIT = 6,
};

enum {
	AW_INTERNAL_TEMP = 0,
	AW_EXTERNAL_TEMP = 1,
};

struct aw_monitor_hdr_v_0_1_1 {
	u32 check_sum;
	u32 monitor_ver;
	char chip_type[16];
	u32 ui_ver;
	u32 monitor_time;
	u32 monitor_count;
	u32 enable_flag;
	/* [bit 31:7]*/
	/* [bit 6: vol en]*/
	/* [bit 5: temp en]*/
	/* [bit 4: vmax en]*/
	/* [bit 3: gain en]*/
	/* [bit 2: ipeak en]*/
	/* [bit 1: & or | flag]*/
	/* [bit 0: monitor en]*/
	u32 temp_aplha;
	u32 temp_num;
	u32 single_temp_size;
	u32 temp_offset;
	u32 vol_aplha;
	u32 vol_num;
	u32 single_vol_size;
	u32 vol_offset;
	u32 reserver[3];
};

struct aw_table {
	int16_t min_val;
	int16_t max_val;
	u16 ipeak;
	u16 gain;
	u32 vmax;
};

struct aw_table_info {
	u8 table_num;
	struct aw_table *aw_table;
};

struct aw_monitor_cfg {
	u8 monitor_status;
	u32 monitor_switch;
	u32 monitor_time;
	u32 monitor_count;
	u32 logic_switch;
	u32 temp_switch;
	u32 temp_aplha;
	u32 vol_switch;
	u32 vol_aplha;
	u32 ipeak_switch;
	u32 gain_switch;
	u32 vmax_switch;
	struct aw_table_info temp_info;
	struct aw_table_info vol_info;
};

struct aw_monitor_trace {
	int32_t pre_val;
	int32_t sum_val;
	struct aw_table aw_table;
};

struct aw_voltage_desc {
	unsigned int reg;
	unsigned int vbat_range;
	unsigned int int_bit;
};

struct aw_temperature_desc {
	unsigned int reg;
	unsigned int sign_mask;
	unsigned int neg_mask;
};

struct aw_ipeak_desc {
	unsigned int reg;
	unsigned int mask;
};

struct aw_vmax_desc {
	unsigned int dsp_reg;
	unsigned char data_type;
	unsigned int init_vmax;
};

struct aw_dsp_monitor_desc {
	unsigned int dsp_temp_reg;
	unsigned int dsp_temp_reg_type;
	unsigned int dsp_temp_en_mask;

	unsigned int dsp_volt_reg;
	unsigned int dsp_volt_reg_type;
	unsigned int dsp_volt_en_mask;

	unsigned int extn_temp_reg;
	unsigned char extn_temp_reg_type;

	unsigned int extn_temp_en_reg;
	unsigned int extn_temp_en_reg_type;
	unsigned int extn_temp_en_mask;
};

struct aw_hw_monitor_desc {
	unsigned int reg;
	unsigned int bop_mask;
	unsigned int vol_mask;
	unsigned int ipeak_mask;
};

/******************************************************************
 * struct monitor
 *******************************************************************/
struct aw_monitor_desc {
	struct delayed_work delay_work;
	struct delayed_work dsp_monitor_work;
	struct aw_monitor_cfg monitor_cfg;

	u8 hw_monitor_en;
	u8 dsp_monitor_en;
	u8 dsp_temp_flag;
	u32 dsp_monitor_delay;

	u8 first_entry;
	u8 samp_count;
	u32 pre_vmax;

	struct aw_monitor_trace temp_trace;
	struct aw_monitor_trace vol_trace;

#ifdef AW_DEBUG
	u16 test_vol;
	int16_t test_temp;
#endif

	struct aw_hw_monitor_desc hw_monitor_desc;
	struct aw_dsp_monitor_desc dsp_monitor_desc;

	struct aw_voltage_desc voltage_desc;
	struct aw_temperature_desc temp_desc;
	struct aw_vmax_desc vmax_desc;
	struct aw_ipeak_desc ipeak_desc;
};

/******************************************************************
 * aw882xx monitor functions
 *******************************************************************/
void aw_monitor_start(struct aw_monitor_desc *monitor_desc);
int aw_monitor_stop(struct aw_monitor_desc *monitor_desc);
void aw_monitor_init(struct aw_monitor_desc *monitor_desc);
void aw_monitor_deinit(struct aw_monitor_desc *monitor_desc);
int aw_monitor_parse_fw(struct aw_monitor_desc *monitor_desc, u8 *data,
			u32 data_len);
void aw_monitor_update_st(struct aw_monitor_desc *monitor_desc,
			  monitor_update_t type);

#endif
