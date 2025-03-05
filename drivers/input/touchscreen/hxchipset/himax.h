/* Himax Android Driver Sample Code for common functions
 *
 * Copyright (C) 2021 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef HIMAX_H
#define HIMAX_H

#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/extcon.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/async.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/firmware.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/pm_wakeup.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>

#if defined(CONFIG_OF)
#include <linux/of_gpio.h>
#endif

#define TRC_EOL "\n"
#define TRC_EOL_LENGTH sizeof(TRC_EOL)
#define TRC_LINE_LENGTH 68
#define TRC_TEXT_LENGTH (TRC_LINE_LENGTH - TRC_EOL_LENGTH) /* reserve space */
#define TRC_NUM_LINES 5000

struct himax_ts_data;

struct himax_ic_data {
	int vendor_arch_ver;
	int vendor_config_ver;
	int vendor_touch_cfg_ver;
	int vendor_display_cfg_ver;
	int vendor_cid_maj_ver;
	int vendor_cid_min_ver;
	int vendor_panel_ver;
	int vendor_sensor_id;
	int ic_adc_num;
	uint8_t vendor_config_date[12];
	uint8_t vendor_cus_info[12];
	uint8_t vendor_proj_info[12];
	uint8_t vendor_ic_id[13];
	int HX_RX_NUM;
	int HX_TX_NUM;
	int HX_BT_NUM;
	int HX_X_RES;
	int HX_Y_RES;
	int HX_MAX_PT;
	bool HX_XY_REVERSE;
	bool HX_INT_IS_EDGE;
	bool HX_IS_ID_EN;
	int HX_RX_NUM_2;
	int HX_TX_NUM_2;
};

struct himax_report_data {
	int touch_all_size;
	int raw_cnt_max;
	int raw_cnt_rmd;
	int touch_info_size;
	uint8_t finger_num;
	uint8_t finger_on;
	uint8_t *hx_coord_buf;
	uint8_t hx_state_info[2];
	int rawdata_size;
	uint8_t diag_cmd;
	uint8_t *hx_rawdata_buf;
	uint8_t rawdata_frame_size;
};

struct himax_core_fp {
	void (*fp_burst_enable)(struct himax_ts_data *ts, uint8_t auto_add_4_byte);
	void (*fp_interface_on)(struct himax_ts_data *ts);
	void (*fp_sense_on)(struct himax_ts_data *ts, uint8_t FlashMode);
	bool (*fp_sense_off)(struct himax_ts_data *ts, bool check_en);
	bool (*fp_wait_wip)(struct himax_ts_data *ts, int Timing);
	void (*fp_init_psl)(struct himax_ts_data *ts);
	void (*fp_resume_ic_action)(struct himax_ts_data *ts);
	void (*fp_suspend_ic_action)(struct himax_ts_data *ts);
	void (*fp_power_on_init)(struct himax_ts_data *ts);
	bool (*fp_dd_clk_set)(struct himax_ts_data *ts, bool enable);
	void (*fp_dd_reg_en)(struct himax_ts_data *ts, bool enable);
	bool (*fp_dd_reg_write)(struct himax_ts_data *ts, uint8_t addr, uint8_t pa_num, int len,
				uint8_t *data, uint8_t bank);
	bool (*fp_dd_reg_read)(struct himax_ts_data *ts, uint8_t addr, uint8_t pa_num, int len,
			       uint8_t *data, uint8_t bank);
	bool (*fp_ic_id_read)(struct himax_ts_data *ts);
	/* CORE_FW */
	void (*fp_parse_raw_data)(struct himax_ts_data *ts, struct himax_report_data *hx_touch_data,
				  int mul_num, int self_num, uint8_t diag_cmd, int16_t *mutual_data,
				  int16_t *self_data);
	void (*fp_system_reset)(struct himax_ts_data *ts);
	int (*fp_Calculate_CRC_with_AP)(struct himax_ts_data *ts, unsigned char *FW_content,
					int CRC_from_FW, int len);
	uint32_t (*fp_check_CRC)(struct himax_ts_data *ts, uint8_t *start_addr, int reload_length);
	void (*fp_diag_register_set)(struct himax_ts_data *ts, uint8_t diag_command,
				     uint8_t storage_type, bool is_dirly);
	void (*fp_control_reK)(struct himax_ts_data *ts, bool enable);
	int (*fp_chip_self_test)(struct himax_ts_data *ts, struct seq_file *s, void *v);
	void (*fp_reload_disable)(struct himax_ts_data *ts, int disable);
	int (*fp_read_ic_trigger_type)(struct himax_ts_data *ts);
	void (*fp_read_FW_ver)(struct himax_ts_data *ts);
	bool (*fp_read_event_stack)(struct himax_ts_data *ts, uint8_t *buf, uint8_t length);
	void (*fp_return_event_stack)(struct himax_ts_data *ts);
	bool (*fp_calculateChecksum)(struct himax_ts_data *ts, bool change_iref, uint32_t size);
	void (*fp_read_FW_status)(struct himax_ts_data *ts);
	void (*fp_irq_switch)(struct himax_ts_data *ts, int switch_on);
	int (*fp_assign_sorting_mode)(struct himax_ts_data *ts, uint8_t *tmp_data);
	int (*fp_check_sorting_mode)(struct himax_ts_data *ts, uint8_t *tmp_data);
	int (*fp_get_max_dc)(struct himax_ts_data *ts);
	/* CORE_FW */

	/* CORE_FLASH */
	void (*fp_chip_erase)(struct himax_ts_data *ts);
	bool (*fp_block_erase)(struct himax_ts_data *ts, int start_addr, int length);
	bool (*fp_sector_erase)(struct himax_ts_data *ts, int start_addr, int length);
	void (*fp_flash_programming)(struct himax_ts_data *ts, uint8_t *FW_content, int start_addr,
				     int FW_Size);
	void (*fp_flash_page_write)(struct himax_ts_data *ts, uint8_t *write_addr, int length,
				    uint8_t *write_data);
	int (*fp_fts_ctpm_fw_upgrade_with_sys_fs_64k)(struct himax_ts_data *ts, unsigned char *fw,
						      int len, bool change_iref);
	int (*fp_fts_ctpm_fw_upgrade_with_sys_fs_128k)(struct himax_ts_data *ts, unsigned char *fw,
						       int len, bool change_iref);
	void (*fp_flash_dump_func)(struct himax_ts_data *ts, uint8_t local_flash_command,
				   int Flash_Size, uint8_t *flash_buffer);
	bool (*fp_flash_lastdata_check)(struct himax_ts_data *ts, uint32_t size);
	bool (*fp_bin_desc_get)(struct himax_ts_data *ts, unsigned char *fw, uint32_t max_sz);
	void (*fp_flash_read)(struct himax_ts_data *ts, uint8_t *r_data, int start_addr,
			      int length);
	/* CORE_FLASH */

	/* CORE_SRAM */
	bool (*fp_get_DSRAM_data)(struct himax_ts_data *ts, uint8_t *info_data, bool DSRAM_Flag);
	/* CORE_SRAM */

	/* CORE_DRIVER */
	void (*fp_chip_init)(struct himax_ts_data *ts);
	int (*fp_fw_ver_bin)(struct himax_ts_data *ts);
	void (*fp_pin_reset)(struct himax_ts_data *ts);
	void (*fp_touch_information)(struct himax_ts_data *ts);
	void (*fp_reload_config)(struct himax_ts_data *ts);
	int (*fp_get_touch_data_size)(struct himax_ts_data *ts);
	int (*fp_cal_data_len)(struct himax_ts_data *ts, int raw_cnt_rmd, int HX_MAX_PT,
			       int raw_cnt_max);
	bool (*fp_diag_check_sum)(struct himax_ts_data *ts,
				  struct himax_report_data *hx_touch_data);
	void (*fp_diag_parse_raw_data)(struct himax_ts_data *ts,
				       struct himax_report_data *hx_touch_data, int mul_num,
				       int self_num, uint8_t diag_cmd, int32_t *mutual_data,
				       int32_t *self_data);
	void (*fp_ic_reset)(struct himax_ts_data *ts, uint8_t loadconfig, uint8_t int_off);
};

struct himax_virtual_key {
	int index;
	int keycode;
	int x_range_min;
	int x_range_max;
	int y_range_min;
	int y_range_max;
};

struct trc_line {
	u8 read_offset;
	char text[TRC_LINE_LENGTH];
};

struct trc_lines {
	bool enabled;
	struct mutex mutex;
	int fill_status;
	int read_idx;
	int write_idx;
	struct trc_line lines[TRC_NUM_LINES];
};

struct himax_debug_ops {
	void (*fp_ts_dbg_func)(struct himax_ts_data *ts, int start);
	int (*fp_set_diag_cmd)(struct himax_ts_data *ts, struct himax_ic_data *ic_data,
			       struct himax_report_data *hx_touch_data);
};

struct himax_debug_procfs {
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *self_test;
	struct proc_dir_entry *wpbplock_node;
	struct proc_dir_entry *fail_det;
	struct proc_dir_entry *inspect_mode;

	struct proc_dir_entry *diag_dir;
	struct proc_dir_entry *stack;
	struct proc_dir_entry *delta;
	struct proc_dir_entry *dc;
	struct proc_dir_entry *baseline;

	struct proc_dir_entry *vendor;
	struct proc_dir_entry *debug;
	struct proc_dir_entry *flash_dump;
};

#define BUF_SIZE 1024

struct himax_debug {
	struct himax_debug_ops ops;
	bool flash_dump_going;
	uint8_t diag_arr_num;
	int max_mutual;
	int min_mutual;
	int max_self;
	int min_self;
	uint8_t byte_length;
	uint8_t reg_cmd[4];
	struct himax_debug_procfs procfs;
	uint8_t cfg_flag;
	bool dsram_flag;
	bool is_2t2r;
	int hx_rx_num_2;
	int hx_tx_num_2;
	uint32_t *diag_mutual_2;
	int32_t *diag_mutual;
	int32_t *diag_mutual_new;
	int32_t *diag_mutual_old;
	uint8_t diag_max_cnt;
	uint8_t hx_state_info[2];
	uint8_t diag_coor[128];
	int32_t *diag_self;
	int32_t *diag_self_new;
	int32_t *diag_self_old;
	uint8_t *gma_buf;
	bool fw_update_complete;
	bool fw_update_going;
	int handshaking_result;
	unsigned char debug_level_cmd;
	int flash_size;
	uint8_t *flash_buffer;
	uint8_t flash_cmd;
	uint8_t flash_progress;
	bool flash_dump_rst; /*Fail = 0, Pass = 1*/
	char buf_tmp[BUF_SIZE];
	uint8_t *reg_read_data;
	struct timespec64 time_start;
	struct timespec64 time_end;
	struct timespec64 time_delta;
	uint8_t process_type;
	uint8_t mode_flag;
	uint8_t h_overflow;
};

struct himax_ic_incell {
	struct fw_operation *pfw_op;
	struct flash_operation *pflash_op;
};

struct himax_target_report_data {
	int *x;
	int *y;
	int *w;
	int *finger_id;
	int finger_on;
	int finger_num;
	int ig_count;
};

struct himax_ts_data {
	bool initialized;
	bool suspended;
	atomic_t suspend_mode;
	uint8_t x_channel;
	uint8_t y_channel;
	uint8_t useScreenRes;
	uint8_t diag_cmd;
	char chip_name[30];
	uint8_t chip_cell_type;
	uint8_t protocol_type;
	uint8_t first_pressed;
	uint8_t coord_data_size;
	uint8_t area_data_size;
	uint8_t coordInfoSize;
	uint8_t raw_data_frame_size;
	uint8_t nFinger_support;
	uint8_t irq_enabled;
	uint8_t diag_self[50];
	uint16_t finger_pressed;
	uint16_t last_slot;
	uint16_t pre_finger_mask;
	uint16_t old_finger;
	int hx_point_num;
	uint32_t debug_log_level;
	uint32_t widthFactor;
	uint32_t heightFactor;
	int lcm_gpio;
	int rst_gpio;
	int use_irq;
	int (*power)(int on);
	int pre_finger_data[10][2];
	struct device *dev;
	struct extcon_dev *edev;
	struct workqueue_struct *himax_wq;
	struct work_struct work;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct i2c_client *client;
	struct himax_i2c_platform_data *pdata;
	struct himax_virtual_key *button;
	struct mutex rw_lock;
	atomic_t irq_state;
	spinlock_t irq_lock;
	/******* SPI-start *******/
	struct spi_device *spi;
	int hx_irq;
	uint8_t *xfer_buff;
	/******* SPI-end *******/
	int in_self_test;
	int suspend_resume_done;
	int bus_speed;
	int touch_num;

	struct notifier_block fb_notif;
	struct workqueue_struct *himax_att_wq;
	struct delayed_work work_att;

	struct workqueue_struct *flash_wq;
	struct work_struct flash_work;

	struct workqueue_struct *himax_boot_upgrade_wq;
	struct delayed_work work_boot_upgrade;

	struct workqueue_struct *ts_int_workqueue;
	struct delayed_work ts_int_work;

	struct workqueue_struct *himax_diag_wq;
	struct delayed_work himax_diag_delay_wrok;
	struct trc_lines trc_lines;
	struct notifier_block cable_hotplug_nb;
	bool probe_flag;
	struct work_struct probe_work;
	struct work_struct connected_work;
	struct work_struct disconnected_work;

	struct himax_debug debug;
	struct himax_ic_incell ic_incell;
	struct himax_core_fp core_fp;
	struct himax_core_command_operation *core_cmd_op;
	struct sram_operation *psram_op;
	uint8_t *internal_buffer;

	/* inspection */
	int gap_vertical_partial;
	int *gap_vertical_part;
	int gap_horizontal_partial;
	int *gap_horizontal_part;
	int dc_max;
	int one_kind_raw_size;
	uint32_t rslt_data_len;
	int **inspection_criteria;
	int *inspt_crtra_flag;
	int *test_item_flag;
	int do_lpwg_test;
	int hx_criteria_item;
	int hx_criteria_size;
	char *rslt_data;
	bool file_w_flag;
	char file_path[256];
	char rslt_log[256];
	char start_log[512];

	struct himax_ic_data *ic_data;
	unsigned char ic_checksum;
	unsigned long fw_ver_maj_flash_addr;
	unsigned long fw_ver_min_flash_addr;
	unsigned long cfg_ver_maj_flash_addr;
	unsigned long cfg_ver_min_flash_addr;
	unsigned long cid_ver_maj_flash_addr;
	unsigned long cid_ver_min_flash_addr;
	uint32_t cfg_table_flash_addr;
	int fw_ver;
	int cfg_ver;
	int cid_maj; /*GUEST ID*/
	int cid_min; /*VER for GUEST*/
	const struct firmware *hxfw;
	bool boot_upgrade_flag;
	uint32_t hx_chip_inited;
	struct himax_report_data *hx_touch_data;
	int hx_ic_amount;

	/*ts_work about start*/
	struct himax_target_report_data *target_report_data;
	/*ts_work about end*/
	int hx_touch_info_point_cnt;
	bool chip_test_r_flag;
	u8 hx_hw_reset_activate;
	uint8_t aa_press;
	uint8_t en_noisefilter;
	uint8_t last_en_noisefilter;
	int p_point_num;
	int ts_dbg;
	uint8_t hx_proc_send_flag;
	uint8_t inspect_mode_flag;

	u8 *rw_buf;
	int hx_fail_det;

	const char *location;
	struct list_head chips;
};

#endif
