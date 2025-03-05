/* Himax Android Driver Sample Code for ic core functions
 *
 * Copyright (C) 2021 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed,  and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __HIMAX_IC_CORE_H__
#define __HIMAX_IC_CORE_H__

#include "himax_platform.h"
#include "himax_common.h"
#include <linux/slab.h>

#define DATA_LEN_8			8
#define DATA_LEN_4			4
#define ADDR_LEN_4			4
#define FLASH_RW_MAX_LEN		256
#define FLASH_WRITE_BURST_SZ		8
#define MAX_I2C_TRANS_SZ		128
#define HIMAX_TOUCH_DATA_SIZE		128

#define FW_SECTOR_PER_BLOCK		8
#define FW_PAGE_PER_SECTOR		64
#define FW_PAGE_SZ			128
#define HX1K				0x400
#define HX64K				0x10000

#define HX_RW_REG_FAIL (-1)
#define HX_DRIVER_MAX_IC_NUM 5

#if defined(__HIMAX_HX83191_MOD__)
#define HX_MOD_KSYM_HX83191 HX_MOD_KSYM_HX83191
#endif

#if defined(__HIMAX_HX83192_MOD__)
#define HX_MOD_KSYM_HX83192 HX_MOD_KSYM_HX83192
#endif

#if defined(__HIMAX_HX83193_MOD__)
#define HX_MOD_KSYM_HX83193 HX_MOD_KSYM_HX83193
#endif

/* CORE_INIT */
/* CORE_FW */
/* CORE_FLASH */
/* CORE_SRAM */

#if defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
void himax_mcu_in_cmd_struct_free(struct himax_ts_data *ts);
#endif

#if defined(HX_RST_PIN_FUNC)
void himax_gpio_set(int pinnum, uint8_t value);
#endif

int himax_report_data_init(struct himax_ts_data *ts);

/* CORE_INIT */
int himax_mcu_in_cmd_struct_init(struct himax_ts_data *ts);
void himax_mcu_in_cmd_init(struct himax_ts_data *ts);
void himax_parse_assign_cmd(uint32_t addr, uint8_t *cmd, int len);
int himax_mcu_WP_BP_disable(struct himax_ts_data *ts);
int himax_mcu_WP_BP_enable(struct himax_ts_data *ts);
int himax_mcu_WP_BP_status(struct himax_ts_data *ts);
int himax_mcu_flash_id_check(struct himax_ts_data *ts);
int himax_mcu_register_write(struct himax_ts_data *ts, uint8_t *write_addr, uint32_t write_length,
			     uint8_t *write_data, uint8_t cfg_flag);
int himax_mcu_register_read(struct himax_ts_data *ts, uint8_t *read_addr, uint32_t read_length,
			    uint8_t *read_data, uint8_t cfg_flag);
void himax_mcu_tp_lcm_pin_reset(struct himax_ts_data *ts);
/* CORE_INIT */

enum AHB_Interface_Command_Table {
	addr_AHB_address_byte_0 = 0x00,
	addr_AHB_rdata_byte_0 = 0x08,
	addr_AHB_access_direction = 0x0C,
	addr_AHB_continous = 0x13,
	addr_AHB_INC4 = 0x0D,
	addr_sense_on_off_0 = 0x31,
	addr_sense_on_off_1 = 0x32,
	addr_read_event_stack = 0x30,
	para_AHB_access_direction_read = 0x00,
	para_AHB_continous = 0x31,
	para_AHB_INC4 = 0x10,
	para_sense_off_0 = 0x27,
	para_sense_off_1 = 0x95,
};

/* CORE_FW */
#define addr_fw_state				0x800204DC
#define addr_psl				0x900000A0
#define addr_cs_central_state			0x900000A8
#define addr_flag_reset_event			0x900000E4
#define addr_chk_dd_status			0x900000E8
#define fw_addr_osc_en				0x9000009C
#define fw_addr_osc_pw				0x90000280
#define addr_system_reset			0x90000018
#define addr_ctrl_fw				0x9000005C
#define addr_icid_addr				0x900000D0
#define fw_addr_program_reload_from		0x00000000
#define fw_addr_reload_status			0x80050000
#define fw_addr_reload_crc32_result		0x80050018
#define fw_addr_reload_addr_from		0x80050020
#define fw_addr_reload_addr_cmd_beat		0x80050028
#define data_system_reset			0x00000055
#define data_clear				0x00000000
#define addr_raw_out_sel			0x100072EC
#define addr_set_frame_addr			0x10007294
#define addr_sorting_mode_en			0x10007F04
#define addr_fw_mode_status			0x10007088
#define addr_fw_architecture_version		0x10007004
#define addr_fw_config_date			0x10007038
#define addr_fw_config_version			0x10007084
#define addr_fw_CID				0x10007000
#define addr_fw_customer			0x10007008
#define addr_fw_project_name			0x10007014
#define addr_fw_dbg_msg_addr			0x10007F40
#define addr_HX_ID_EN				0x10007134
#define addr_mkey				0x100070E8
#define addr_fw_define_flash_reload		0x10007f00
#define addr_fw_define_2nd_flash_reload		0x100072c0
#define data_fw_define_flash_reload_dis		0x0000a55a
#define data_fw_define_flash_reload_en		0x00000000
#define addr_fw_define_int_is_edge		0x10007088
#define addr_fw_define_rxnum_txnum_maxpt	0x100070f4
#define addr_fw_define_xy_res_enable		0x100070f8

#define fw_data_rawdata_ready_hb 0xa3
#define fw_data_rawdata_ready_lb 0x3a
/* CORE_FW */

/* CORE_FLASH */
#define flash_addr_ctrl_base		0x80000000
#define flash_addr_spi200_trans_fmt	(flash_addr_ctrl_base + 0x10)
#define flash_addr_spi200_trans_ctrl	(flash_addr_ctrl_base + 0x20)
#define flash_addr_spi200_cmd		(flash_addr_ctrl_base + 0x24)
#define flash_addr_spi200_addr		(flash_addr_ctrl_base + 0x28)
#define flash_addr_spi200_data		(flash_addr_ctrl_base + 0x2c)
#define flash_addr_spi200_fifo_rst	(flash_addr_ctrl_base + 0x30)
#define flash_addr_spi200_rst_status	(flash_addr_ctrl_base + 0x34)
#define flash_addr_spi200_flash_speed	(flash_addr_ctrl_base + 0x40)
#define flash_addr_spi200_bt_num	(flash_addr_ctrl_base + 0xe8)
#define flash_data_spi200_txfifo_rst	0x00000004
#define flash_data_spi200_rxfifo_rst	0x00000002
#define flash_data_spi200_trans_fmt	0x00020780
#define flash_data_spi200_trans_ctrl_1	0x42000003
#define flash_data_spi200_trans_ctrl_2	0x47000000
#define flash_data_spi200_trans_ctrl_3	0x67000000
#define flash_data_spi200_trans_ctrl_4	0x610ff000
#define flash_data_spi200_trans_ctrl_6	0x42000000
#define flash_data_spi200_trans_ctrl_7	0x6940020f
#define flash_data_spi200_cmd_1		0x00000005
#define flash_data_spi200_cmd_2		0x00000006
#define flash_data_spi200_cmd_3		0x000000C7
#define flash_data_spi200_cmd_4		0x000000D8
#define flash_data_spi200_cmd_6		0x00000002
#define flash_data_spi200_cmd_7		0x0000003b
#define flash_data_spi200_cmd_8		0x00000003
/* CORE_FLASH */

/* CORE_SRAM */
#define sram_adr_rawdata_addr 0x10000000
#define sram_adr_rawdata_end 0x00000000
#define sram_passwrd_start 0x5AA5
#define sram_passwrd_end 0xA55A
/* CORE_SRAM */

enum bin_desc_map_table {
	TP_CONFIG_TABLE = 0x0000000A,
	FW_CID = 0x10000000,
	FW_VER = 0x10000100,
	CFG_VER = 0x30000000, //0x10000005,
};

extern uint32_t himax_dbg_reg_ary[4];

struct fw_operation {
	uint8_t addr_osc_en[4];
	uint8_t addr_osc_pw[4];
	uint8_t flash_lock_type[1];
	uint8_t addr_program_reload_from[4];
	uint8_t addr_reload_status[4];
	uint8_t addr_reload_crc32_result[4];
	uint8_t addr_reload_addr_from[4];
	uint8_t addr_reload_addr_cmd_beat[4];
	uint8_t data_rawdata_ready_hb[1];
	uint8_t data_rawdata_ready_lb[1];
};

struct flash_operation {
	uint8_t addr_spi200_trans_fmt[4];
	uint8_t addr_spi200_trans_ctrl[4];
	uint8_t addr_spi200_fifo_rst[4];
	uint8_t addr_spi200_rst_status[4];
	uint8_t addr_spi200_flash_speed[4];
	uint8_t addr_spi200_cmd[4];
	uint8_t addr_spi200_addr[4];
	uint8_t addr_spi200_data[4];
	uint8_t addr_spi200_bt_num[4];
	uint8_t data_spi200_txfifo_rst[4];
	uint8_t data_spi200_rxfifo_rst[4];
	uint8_t data_spi200_trans_fmt[4];
	uint8_t data_spi200_trans_ctrl_1[4];
	uint8_t data_spi200_trans_ctrl_2[4];
	uint8_t data_spi200_trans_ctrl_3[4];
	uint8_t data_spi200_trans_ctrl_4[4];
	uint8_t data_spi200_trans_ctrl_5[4];
	uint8_t data_spi200_trans_ctrl_6[4];
	uint8_t data_spi200_trans_ctrl_7[4];
	uint8_t data_spi200_cmd_1[4];
	uint8_t data_spi200_cmd_2[4];
	uint8_t data_spi200_cmd_3[4];
	uint8_t data_spi200_cmd_4[4];
	uint8_t data_spi200_cmd_5[4];
	uint8_t data_spi200_cmd_6[4];
	uint8_t data_spi200_cmd_7[4];
	uint8_t data_spi200_cmd_8[4];
};

struct sram_operation {
	uint8_t addr_rawdata_addr[4];
	uint8_t addr_rawdata_end[4];
	uint8_t passwrd_start[2];
	uint8_t passwrd_end[2];
};

struct himax_core_command_operation {
	struct fw_operation *fw_op;
	struct flash_operation *flash_op;
	struct sram_operation *sram_op;
};

struct himax_chip_ops {
	bool (*detect)(struct himax_ts_data *ts);
};

struct himax_chip_entry {
	struct himax_chip_ops ops;
	struct list_head list;
};

#define DECLARE_HIMAX_CHIP(_name, _detect)                                                         \
	struct himax_chip_entry _name = { \
		.ops = { \
			.detect = _detect, \
		}, \
		.list = LIST_HEAD_INIT((_name).list), \
	}

#endif

#define Flash_list                                                                                 \
	{                                                                                          \
		{ 0xEF, 0x30, 0x12 }, { 0xEF, 0x60, 0x12 }, { 0xC8, 0x60, 0x13 },                  \
		{ 0xC8, 0x60, 0x12 }, { 0xC2, 0x28, 0x11 }, { 0xC2, 0x28, 0x12 },                  \
		{ 0xC2, 0x25, 0x32 }, { 0x85, 0x60, 0x13 }, { 0x85, 0x60, 0x12 },                  \
		{ 0x85, 0x40, 0x12 }, { 0x7F, 0x11, 0x52 }, { 0x5E, 0x60, 0x13 },                  \
		{ 0x1C, 0x38, 0x13 }, { 0x1C, 0x38, 0x12 }, { 0x9D, 0x40, 0x12 }                   \
	}
