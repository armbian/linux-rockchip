/* SPDX-License-Identifier: GPL-2.0
 * aw_device.h   aw codec driver
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

#ifndef __AW_DEVICE_FILE_H__
#define __AW_DEVICE_FILE_H__

#include "aw_data_type.h"
#include "aw_spin.h"
#include "aw_monitor.h"
#include "aw_calib.h"

#define AW_DEV_DEFAULT_CH    (0)
#define AW_DEV_I2S_CHECK_MAX (5)
#define AW_DEV_DSP_CHECK_MAX (5)
#define AW_REG_NONE          (0xFF)

/********************************************
 *
 * DSP I2C WRITES
 *
 *******************************************/
#define AW_DSP_I2C_WRITES
#define AW_MAX_RAM_WRITE_BYTE_SIZE (128)
#define AW_DSP_ODD_NUM_BIT_TEST    (0x5555)
#define AW_DSP_EVEN_NUM_BIT_TEST   (0xAAAA)
#define AW_DSP_ST_CHECK_MAX        (2)
#define AW_FADE_IN_OUT_DEFAULT     (0)
#define AW_CALI_DELAY_CACL(value)  ((value * 32) / 48)
#define AW_FW_ADDR_LEN             (4)
#define AW_CRC_ADDR_LEN            (1)
#define AW_FW_VER_MASK             (0xFF)

#define AW_GET_MIN_VALUE(value1, value2) \
	((value1) > (value2) ? (value2) : (value1))

#define AW_GET_MAX_VALUE(value1, value2) \
	((value1) > (value2) ? (value1) : (value2))

struct aw_device;

typedef struct aw_msg_hdr aw_dev_msg_t;

enum {
	AW_MSG_TYPE_READ = 0,
	AW_MSG_TYPE_WRITE = 1,
};

enum {
	AW_DEV_VDSEL_DAC = 0,
	AW_DEV_VDSEL_VSENSE = 1,
};

enum {
	AW_DSP_CRC_ON = 0,
	AW_DSP_CRC_BYPASS = 1,
};

enum {
	AW_DSP_FW_UPDATE_OFF = 0,
	AW_DSP_FW_UPDATE_ON = 1,
};

enum {
	AW_FORCE_UPDATE_OFF = 0,
	AW_FORCE_UPDATE_ON = 1,
};

enum {
	AW_500_US = 500,
	AW_1000_US = 1000,
	AW_1500_US = 1500,
	AW_2000_US = 2000,
	AW_3000_US = 3000,
	AW_4000_US = 4000,
	AW_5000_US = 5000,
	AW_10000_US = 10000,
	AW_100000_US = 100000,
};

enum {
	AW_DEV_TYPE_OK = 0,
	AW_DEV_TYPE_NONE = 1,
};

enum {
	AW_DEV_PW_OFF = 0,
	AW_DEV_PW_ON,
};

enum {
	AW_DEV_FW_FAILED = 0,
	AW_DEV_FW_OK,
};

enum {
	AW_DEV_MEMCLK_OSC = 0,
	AW_DEV_MEMCLK_PLL = 1,
};

enum {
	AW_DEV_DSP_WORK = 0,
	AW_DEV_DSP_BYPASS = 1,
};

enum {
	AW_DSP_16_DATA = 0,
	AW_DSP_32_DATA = 1,
};

enum {
	AW_NOT_RCV_MODE = 0,
	AW_RCV_MODE = 1,
};

enum {
	AW_EF_AND_CHECK = 0,
	AW_EF_OR_CHECK,
};

enum {
	AW_SW_CRC_CHECK = 0,
	AW_HW_CRC_CHECK,
};

enum {
	AW_FW_CRC_CHECK = 0,
	AW_CFG_CRC_CHECK,
};

enum {
	AW_REALTIME_CRC_CHECK_NORMAL = 0,
	AW_REALTIME_CRC_CHECK_ABNORMAL,
};

enum {
	AW_HW_TYPE = 0,
	AW_DSP_TYPE,
};

typedef enum {
	AW_FADE_IN_TIME = 0,
	AW_FADE_OUT_TIME = 1,
} fade_time_t;

enum {
	AW_DEV_HMUTE_DISABLE = 0,
	AW_DEV_HMUTE_ENABLE,
};

enum {
	AW_DEV_MASK_INT_VAL = 0,
	AW_DEV_UNMASK_INT_VAL,
};

struct aw_device_ops {
	int (*aw_reg_write)(struct aw_device *aw_dev, u8 reg_addr, u16 reg_data);
	int (*aw_reg_read)(struct aw_device *aw_dev, u8 reg_addr, u16 *reg_data);
	int (*aw_reg_write_bits)(struct aw_device *aw_dev, u8 reg_addr,
				 u16 mask, u16 reg_data);
	int (*aw_dsp_writes)(struct aw_device *aw_dev, u8 *data, u32 len,
			     u16 base);
	int (*aw_dsp_write)(struct aw_device *aw_dev, u16 dsp_addr,
			    u32 reg_data, u8 data_type);
	int (*aw_dsp_read)(struct aw_device *aw_dev, u16 dsp_addr,
			   u32 *dsp_data, u8 data_type);
	int (*aw_dsp_write_bits)(struct aw_device *aw_dev, u16 dsp_addr,
				 u32 dsp_mask, u32 dsp_data, u8 data_type);
	int (*aw_set_hw_volume)(struct aw_device *aw_dev, u16 value);
	int (*aw_get_hw_volume)(struct aw_device *aw_dev, u16 *value);
	unsigned int (*aw_reg_val_to_db)(unsigned int value);
	void (*aw_i2s_tx_enable)(struct aw_device *aw_dev, bool flag);
	int (*aw_get_dev_num)(void);
	bool (*aw_check_wr_access)(int reg);
	bool (*aw_check_rd_access)(int reg);
	int (*aw_get_reg_num)(void);
	int (*aw_get_version)(char *buf, int size);
	int (*aw_read_dsp_pid)(struct aw_device *aw_dev);
	int (*aw_set_cali_re)(struct aw_device *aw_dev);
	int (*aw_get_cali_re)(struct aw_device *aw_dev, u32 *re);
	int (*aw_set_vcalb)(struct aw_device *aw_dev);
	int (*aw_get_r0)(struct aw_device *aw_dev, u32 *re);
	int (*aw_sw_crc_check)(struct aw_device *aw_dev);
	int (*aw_crc_realtime_set)(struct aw_device *aw_dev, bool enable);
	int (*aw_crc_realtime_get)(struct aw_device *aw_dev, bool init_falg);
	int (*aw_crc_realtime_check)(struct aw_device *aw_dev);
	int (*aw_init_vcalb_update)(struct aw_device *aw_dev, backup_sec_t flag);
	int (*aw_init_re_update)(struct aw_device *aw_dev, backup_sec_t flag);
};

struct aw_int_desc {
	unsigned int mask_reg;     /*interrupt mask reg*/
	unsigned int st_reg;       /*interrupt status reg*/
	unsigned int mask_default; /*default mask close all*/
	unsigned int int_mask;     /*set mask*/
	unsigned int intst_mask;   /*interrupt check mask*/
	u16 sysint_st;        /*interrupt reg status*/
};

struct aw_pwd_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_efuse_check_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int or_val;
	unsigned int and_val;
	unsigned int check_val;
};

struct aw_vsense_desc {
	unsigned int vcalb_vsense_reg;
	unsigned int vcalk_vdsel_mask;
	unsigned int vcalb_vsense_default;
};

struct aw_vcalb_desc {
	unsigned int icalkh_reg;
	unsigned int icalkh_reg_mask;
	unsigned int icalkh_shift;

	unsigned int icalkl_reg;
	unsigned int icalkl_reg_mask;
	unsigned int icalkl_shift;

	unsigned int icalk_sign_mask;
	unsigned int icalk_neg_mask;
	int icalk_value_factor;

	unsigned int vcalkh_reg;
	unsigned int vcalkh_reg_mask;
	unsigned int vcalkh_shift;

	unsigned int vcalkl_reg;
	unsigned int vcalkl_reg_mask;
	unsigned int vcalkl_shift;

	unsigned int vcalk_sign_mask;
	unsigned int vcalk_neg_mask;
	int vcalk_value_factor;

	int vscal_factor;
	int iscal_factor;

	unsigned int vcalb_adj_shift;
	int cabl_base_value;
	int vcalb_accuracy;
	unsigned int vcalb_reg;

	/*Vsense data select bit: DAC*/
	unsigned int icalkh_dac_reg;
	unsigned int icalkh_dac_reg_mask;
	unsigned int icalkh_dac_shift;

	unsigned int icalkl_dac_reg;
	unsigned int icalkl_dac_reg_mask;
	unsigned int icalkl_dac_shift;

	unsigned int icalk_dac_sign_mask;
	unsigned int icalk_dac_neg_mask;
	int icalk_dac_value_factor;

	unsigned int vcalkh_dac_reg;
	unsigned int vcalkh_dac_reg_mask;
	unsigned int vcalkh_dac_shift;

	unsigned int vcalkl_dac_reg;
	unsigned int vcalkl_dac_reg_mask;
	unsigned int vcalkl_dac_shift;

	unsigned int vcalk_dac_sign_mask;
	unsigned int vcalk_dac_neg_mask;
	int vcalk_dac_value_factor;

	int vscal_dac_factor;
	int iscal_dac_factor;

	/* vcalb adj */
	u16 init_value;

	/*current vcalb*/
	u16 last_value;
};

struct aw_mute_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_sysst_desc {
	unsigned int reg;
	unsigned int st_check;
	unsigned int st_mask;
	unsigned int pll_check;
	unsigned int dsp_check;
	unsigned int dsp_mask;
	unsigned int st_sws_check;
};

struct aw_noise_gate_en {
	unsigned int reg;
	unsigned int noise_gate_mask;
};

struct aw_profctrl_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int rcv_mode_val;
	unsigned int cur_mode;
};

struct aw_volume_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int shift;
	unsigned int init_volume;
	unsigned int mute_volume;
	unsigned int ctl_volume;
	unsigned int max_volume;
	unsigned int monitor_volume;
};

struct aw_dsp_en_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_memclk_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int mcu_hclk;
	unsigned int osc_clk;
};

struct aw_watch_dog_desc {
	unsigned int reg;
	unsigned int mask;
};

struct aw_dsp_mem_desc {
	unsigned int dsp_madd_reg;
	unsigned int dsp_mdat_reg;
	unsigned int dsp_fw_base_addr;
	unsigned int dsp_cfg_base_addr;
	unsigned int dsp_rom_check_reg;
	unsigned int dsp_rom_check_data;
};

struct aw_soft_rst {
	u8 reg;
	u16 reg_value;
};

struct aw_amppd_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_dsp_crc_desc {
	unsigned int ctl_reg;
	unsigned int ctl_mask;
	unsigned int ctl_enable;
	unsigned int ctl_disable;

	unsigned int dsp_reg;
	unsigned char data_type;

	unsigned int check_reg;
	unsigned char check_reg_data_type;
};

struct aw_cco_mux_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int divider;
	unsigned int bypass;
};

struct aw_re_range_desc {
	u32 re_min;
	u32 re_max;
	u32 re_min_default;
	u32 re_max_default;
};

struct aw_chansel_desc {
	unsigned int rxchan_reg;
	unsigned int rxchan_mask;
	unsigned int rx_left;
	unsigned int rx_right;
};

struct aw_tx_en_desc {
	unsigned int tx_en_reg;
	unsigned int tx_en_mask;
	unsigned int tx_disable;
};

struct aw_dsp_st {
	unsigned int dsp_reg_s1;
	unsigned int dsp_reg_e1;

	unsigned int dsp_reg_s2;
	unsigned int dsp_reg_e2;
};

struct aw_crc_check_desc {
	unsigned int ram_clk_reg;
	unsigned int ram_clk_mask;
	unsigned int ram_clk_on;
	unsigned int ram_clk_off;

	unsigned int crc_ctrl_reg;
	unsigned int crc_end_addr_mask;
	unsigned int crc_fw_check_en_mask;
	unsigned int crc_fwcheck_enable;
	unsigned int crc_fwcheck_disable;
	unsigned int crc_cfg_check_en_mask;
	unsigned int crc_cfgcheck_enable;
	unsigned int crc_cfgcheck_disable;

	unsigned int crc_check_reg;
	unsigned int crc_check_mask;
	unsigned int crc_check_bits_shift;
	unsigned int crc_check_pass;

	unsigned int crc_fw_base_addr;
	unsigned int crc_cfg_base_addr;
	unsigned int crc_init_val;
};

struct aw_crc_check_realtime_desc {
	unsigned int addr;
	unsigned int mask;
	unsigned char data_type;
	unsigned int init_switch;
	unsigned int status;
	unsigned int enable;
	unsigned int disable;

	unsigned int check_addr;
	unsigned int check_mask;
	unsigned char check_data_type;
	unsigned int check_abnormal;
	unsigned int check_normal;
};

struct aw_dsp_vol_desc {
	unsigned int reg;
	unsigned int mute_st;
	unsigned int noise_st;
	unsigned int mask;
};

struct aw_dither_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;
};

struct aw_fw_ver_desc {
	unsigned int version_reg;
	unsigned int data_type;
};

struct aw_dsp_ng_desc {
	unsigned int reg;
	unsigned int mask;
	unsigned int enable;
	unsigned int disable;

	unsigned int dsp_lp_reg; /* dsp low power */
	unsigned int dsp_lp_data_type;
	unsigned int dsp_lp_mask;
	unsigned int dsp_lp_enable;
	unsigned int dsp_lp_disable;
};

struct aw_container {
	int len;
	u8 data[];
};

struct aw_device {
	int status;
	struct mutex *i2c_lock;

	unsigned char cur_prof; /*current profile index*/
	unsigned char set_prof; /*set profile index*/
	unsigned char dsp_crc_st;
	u16 chip_id;
	u32 fw_ver;

	unsigned int channel; /*pa channel select*/
	unsigned int fade_step;
	unsigned int dither_st;
	unsigned int txen_st;

	struct i2c_client *i2c;
	struct device *dev;
	void *private_data;

	u32 fade_en;
	unsigned char dsp_cfg;

	u32 dsp_fw_len;
	u32 dsp_cfg_len;
	u8 fw_status; /*load cfg status*/
	u8 crc_type;

	struct aw_sec_data_desc crc_dsp_cfg;
	struct aw_prof_info prof_info;
	struct aw_int_desc int_desc;
	struct aw_pwd_desc pwd_desc;
	struct aw_mute_desc mute_desc;
	struct aw_amppd_desc amppd_desc;

	struct aw_sysst_desc sysst_desc;
	struct aw_profctrl_desc profctrl_desc;
	struct aw_volume_desc volume_desc;
	struct aw_dsp_en_desc dsp_en_desc;
	struct aw_memclk_desc memclk_desc;
	struct aw_watch_dog_desc watch_dog_desc;
	struct aw_dsp_mem_desc dsp_mem_desc;
	struct aw_soft_rst soft_rst;

	struct aw_dsp_crc_desc dsp_crc_desc;
	struct aw_cco_mux_desc cco_mux_desc;

	struct aw_vsense_desc vsense_desc;
	struct aw_vcalb_desc vcalb_desc;
	struct aw_efuse_check_desc efuse_check_desc;
	struct aw_dsp_vol_desc dsp_vol_desc;

	struct aw_re_range_desc re_range;
	struct aw_tx_en_desc tx_en_desc;
	struct aw_dsp_st dsp_st_desc;
	struct aw_crc_check_desc crc_check_desc;
	struct aw_crc_check_realtime_desc crc_check_realtime_desc;
	struct aw_noise_gate_en noise_gate_en;
	struct aw_fw_ver_desc fw_ver_desc;
	struct aw_dither_desc dither_desc;
	struct aw_dsp_ng_desc dsp_ng_desc;

	struct aw_spin_desc spin_desc;
	struct aw_chansel_desc chansel_desc;
	struct aw_cali_desc cali_desc;
	struct aw_monitor_desc monitor_desc;

	struct aw_device_ops ops;
	struct list_head list_node;
};

void aw_device_deinit(struct aw_device *aw_dev);
int aw_device_init(struct aw_device *aw_dev, struct aw_container *aw_prof);
int aw_device_start(struct aw_device *aw_dev);
int aw_device_stop(struct aw_device *aw_dev);
int aw_device_probe(struct aw_device *aw_dev);
int aw_device_remove(struct aw_device *aw_dev);
int aw_device_params(struct aw_device *aw_dev, dev_params_t params_type,
		     void *data, u32 size, params_option_t params_ops);
bool aw_device_status(struct aw_device *aw_dev, dev_status_t type,
		      status_option_t status_ops);
int aw_device_fw_update(struct aw_device *aw_dev, bool up_dsp_fw_en,
			bool force_up_en);
int aw_dev_get_list_head(struct list_head **head);
void aw_dev_add_dev_list(struct aw_device *aw_dev);
int aw_crc_realtime_check(struct aw_device *aw_dev, u32 *status);

#endif
