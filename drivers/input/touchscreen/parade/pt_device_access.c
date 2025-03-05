/*
 * pt_device_access.c
 * Parade TrueTouch(TM) Standard Product Device Access Module.
 * Configuration and Test command/status user interface.
 * For use with Parade touchscreen controllers.
 * Supported parts include:
 * TMA5XX
 * TMA448
 * TMA445A
 * TT21XXX
 * TT31XXX
 * TT4XXXX
 * TT7XXX
 * TC3XXX
 *
 * Copyright (C) 2015-2020 Parade Technologies
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Parade Technologies at www.paradetech.com <ttdrivers@paradetech.com>
 */

#include "pt_regs.h"
#include <linux/firmware.h>

#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#define PT_CMCP_THRESHOLD_FILE_NAME "pt_thresholdfile.csv"
#define CMCP_THRESHOLD_FILE_NAME "ttdl_cmcp_thresholdfile.csv"

/* Max test case number */
#define MAX_CASE_NUM            (22)

/* ASCII */
#define ASCII_LF                (0x0A)
#define ASCII_CR                (0x0D)
#define ASCII_COMMA             (0x2C)
#define ASCII_ZERO              (0x30)
#define ASCII_NINE              (0x39)

/* Max characters of test case name */
#define NAME_SIZE_MAX           (50)

/* Max sensor and button number */
#define MAX_BUTTONS             (PIP1_SYSINFO_MAX_BTN)
#define MAX_SENSORS             (5120)
#define MAX_TX_SENSORS          (128)
#define MAX_RX_SENSORS          (128)

/* Multiply by 2 for double (min, max) values */
#define TABLE_BUTTON_MAX_SIZE   (MAX_BUTTONS * 2)
#define TABLE_SENSOR_MAX_SIZE   (MAX_SENSORS * 2)
#define TABLE_TX_MAX_SIZE       (MAX_TX_SENSORS*2)
#define TABLE_RX_MAX_SIZE       (MAX_RX_SENSORS*2)

#define CM_PANEL_DATA_OFFSET    (6)
#define CM_BTN_DATA_OFFSET      (6)
#define CP_PANEL_DATA_OFFSET    (6)
#define CP_BTN_DATA_OFFSET      (6)
#define MAX_BUF_LEN             (100000)
#define RETRIEVE_PANEL_SCAN_HDR (10)

enum print_buffer_format {
	PT_PR_FORMAT_DEFAULT        = 0,
	PT_PR_FORMAT_U8_SPACE       = 1,
	PT_PR_FORMAT_U16_SPACE      = 2,
	PT_PR_FORMAT_U8_NO_SPACE    = 3,
	PT_PR_FORMAT_U32_SPACE      = 4,
	PT_PR_FORMAT_UNDEFINE
};

/* cmcp csv file information */
struct configuration {
	u32 cm_range_limit_row;
	u32 cm_range_limit_col;
	u32 cm_min_limit_cal;
	u32 cm_max_limit_cal;
	u32 cm_max_delta_sensor_percent;
	u32 cm_max_delta_button_percent;
	u32 min_button;
	u32 max_button;
	u32 cp_max_delta_sensor_rx_percent;
	u32 cp_max_delta_sensor_tx_percent;
	u32 cm_min_max_table_btn[TABLE_BUTTON_MAX_SIZE];
	u32 cp_min_max_table_btn[TABLE_BUTTON_MAX_SIZE];
	u32 cm_min_max_table_sensor[TABLE_SENSOR_MAX_SIZE];
	u32 cp_min_max_table_rx[TABLE_RX_MAX_SIZE];
	u32 cp_min_max_table_tx[TABLE_TX_MAX_SIZE];
	u32 cm_min_max_table_btn_size;
	u32 cp_min_max_table_btn_size;
	u32 cm_min_max_table_sensor_size;
	u32 cp_min_max_table_rx_size;
	u32 cp_min_max_table_tx_size;
	u32 cp_max_delta_button_percent;
	u32 cm_max_table_gradient_cols_percent[TABLE_TX_MAX_SIZE];
	u32 cm_max_table_gradient_cols_percent_size;
	u32 cm_max_table_gradient_rows_percent[TABLE_RX_MAX_SIZE];
	u32 cm_max_table_gradient_rows_percent_size;
	u32 cm_excluding_row_edge;
	u32 cm_excluding_col_edge;
	u32 rx_num;
	u32 tx_num;
	u32 btn_num;
	u32 cm_enabled;
	u32 cp_enabled;
	u32 is_valid_or_not;
};

/* Test case search definition */
struct test_case_search {
	char name[NAME_SIZE_MAX]; /* Test case name */
	u32 name_size;            /* Test case name size */
	u32 offset;               /* Test case offset */
};

/* Test case field definition */
struct test_case_field {
	char *name;     /* Test case name */
	u32 name_size;  /* Test case name size */
	u32 type;       /* Test case type */
	u32 *bufptr;    /* Buffer to store value information */
	u32 exist_or_not;/* Test case exist or not */
	u32 data_num;   /* Buffer data number */
	u32 line_num;   /* Buffer line number */
};

/* Test case type */
enum test_case_type {
	TEST_CASE_TYPE_NO,
	TEST_CASE_TYPE_ONE,
	TEST_CASE_TYPE_MUL,
	TEST_CASE_TYPE_MUL_LINES,
};

/* Test case order in test_case_field_array */
enum case_order {
	CM_TEST_INPUTS,
	CM_EXCLUDING_COL_EDGE,
	CM_EXCLUDING_ROW_EDGE,
	CM_GRADIENT_CHECK_COL,
	CM_GRADIENT_CHECK_ROW,
	CM_RANGE_LIMIT_ROW,
	CM_RANGE_LIMIT_COL,
	CM_MIN_LIMIT_CAL,
	CM_MAX_LIMIT_CAL,
	CM_MAX_DELTA_SENSOR_PERCENT,
	CM_MAX_DELTA_BUTTON_PERCENT,
	PER_ELEMENT_MIN_MAX_TABLE_BUTTON,
	PER_ELEMENT_MIN_MAX_TABLE_SENSOR,
	CP_TEST_INPUTS,
	CP_MAX_DELTA_SENSOR_RX_PERCENT,
	CP_MAX_DELTA_SENSOR_TX_PERCENT,
	CP_MAX_DELTA_BUTTON_PERCENT,
	CP_PER_ELEMENT_MIN_MAX_BUTTON,
	MIN_BUTTON,
	MAX_BUTTON,
	PER_ELEMENT_MIN_MAX_RX,
	PER_ELEMENT_MIN_MAX_TX,
	CASE_ORDER_MAX,
};

enum cmcp_test_item {
	CMCP_FULL = 0,
	CMCP_CM_PANEL,
	CMCP_CP_PANEL,
	CMCP_CM_BTN,
	CMCP_CP_BTN,
};

#define CM_ENABLED 0x10
#define CP_ENABLED 0x20
#define CM_PANEL (0x01 | CM_ENABLED)
#define CP_PANEL (0x02 | CP_ENABLED)
#define CM_BTN (0x04 | CM_ENABLED)
#define CP_BTN (0x08 | CP_ENABLED)
#define CMCP_FULL_CASE\
	(CM_PANEL | CP_PANEL | CM_BTN | CP_BTN | CM_ENABLED | CP_ENABLED)

#define PT_DEVICE_ACCESS_NAME "pt_device_access"
#define PT_INPUT_ELEM_SZ (sizeof("0xHH") + 1)

#define PIP_CMD_MAX_LENGTH ((1 << 16) - 1)

#ifdef TTHE_TUNER_SUPPORT
struct heatmap_param {
	bool scan_start;
	enum scan_data_type_list data_type; /* raw, base, diff */
	int num_element;
};
#endif
#define ABS(x)			(((x) < 0) ? -(x) : (x))

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#define PT_MAX_CONFIG_BYTES    256
#define PT_TTHE_TUNER_GET_PANEL_DATA_FILE_NAME "get_panel_data"
#define TTHE_TUNER_MAX_BUF	(PT_MAX_PRBUF_SIZE * 8)

struct pt_device_access_data {
	struct device *dev;
	struct pt_sysinfo *si;
	struct mutex sysfs_lock;
	bool sysfs_nodes_created;
	struct kobject mfg_test;
	u8 panel_scan_retrieve_id;
	u8 panel_scan_type_id;
	u8 get_idac_data_id;
	u8 calibrate_sensing_mode;
	u8 calibrate_initialize_baselines;
	u8 baseline_sensing_mode;
	u8 fw_self_test_id;
	u8 fw_self_test_format;
	u16 fw_self_test_param_len;
	u8 fw_self_test_param[PT_FW_SELF_TEST_MAX_PARM];
	struct pt_cal_ext_data cal_ext_data;
	struct dentry *panel_scan_debugfs;
	int panel_scan_size;
	u8 panel_scan_data_buf[TTHE_TUNER_MAX_BUF];
	struct mutex debugfs_lock;
#ifdef TTHE_TUNER_SUPPORT
	struct heatmap_param heatmap;
	struct dentry *tthe_get_panel_data_debugfs;
	u8 tthe_get_panel_data_is_open;
#endif
	struct dentry *cmcp_results_debugfs;
	struct dentry *base_dentry;
	struct dentry *mfg_test_dentry;
	u8 ic_buf[PT_MAX_PRBUF_SIZE];
	u8 response_buf[PT_MAX_PRBUF_SIZE];
	struct mutex cmcp_threshold_lock;
	u8 *cmcp_threshold_data;
	int cmcp_threshold_size;
	bool cmcp_threshold_loading;
	struct work_struct cmcp_threshold_update;
	int builtin_cmcp_threshold_status;
	bool is_manual_upgrade_enabled;
	struct configuration *configs;
	struct cmcp_data *cmcp_info;
	struct result *result;
	struct test_case_search *test_search_array;
	struct test_case_field *test_field_array;
	int cmcp_test_items;
	int test_executed;
	int cmcp_range_check;
	int cmcp_force_calibrate;
	int cmcp_test_in_progress;
};

struct cmcp_data {
	struct gd_sensor *gd_sensor_col;
	struct gd_sensor *gd_sensor_row;
	int32_t *cm_data_panel;
	int32_t *cp_tx_data_panel;
	int32_t *cp_rx_data_panel;
	int32_t *cp_tx_cal_data_panel;
	int32_t *cp_rx_cal_data_panel;
	int32_t cp_sensor_rx_delta;
	int32_t cp_sensor_tx_delta;
	int32_t cp_button_delta;
	int32_t *cm_btn_data;
	int32_t *cp_btn_data;
	int32_t *cm_sensor_column_delta;
	int32_t *cm_sensor_row_delta;
	int32_t cp_btn_cal;
	int32_t cm_btn_cal;
	int32_t cp_button_ave;
	int32_t cm_ave_data_panel;
	int32_t cp_tx_ave_data_panel;
	int32_t cp_rx_ave_data_panel;
	int32_t cm_cal_data_panel;
	int32_t cm_ave_data_btn;
	int32_t cm_cal_data_btn;
	int32_t cm_delta_data_btn;
	int32_t cm_sensor_delta;

	int32_t tx_num;
	int32_t rx_num;
	int32_t btn_num;
};

struct result {
	int32_t config_ver;
	int32_t revision_ctrl;
	int32_t device_id_high;
	int32_t device_id_low;
	/* Sensor Cm validation */
	bool cm_test_pass;
	bool cm_sensor_validation_pass;
	bool cm_sensor_row_delta_pass;
	bool cm_sensor_col_delta_pass;
	bool cm_sensor_gd_row_pass;
	bool cm_sensor_gd_col_pass;
	bool cm_sensor_calibration_pass;
	bool cm_sensor_delta_pass;
	bool cm_button_validation_pass;
	bool cm_button_delta_pass;

	int32_t *cm_sensor_raw_data;
	int32_t cm_sensor_calibration;
	int32_t cm_sensor_delta;
	int32_t *cm_button_raw_data;
	int32_t cm_button_delta;

	/* Sensor Cp validation */
	bool cp_test_pass;
	bool cp_sensor_delta_pass;
	bool cp_sensor_rx_delta_pass;
	bool cp_sensor_tx_delta_pass;
	bool cp_sensor_average_pass;
	bool cp_button_delta_pass;
	bool cp_button_average_pass;
	bool cp_rx_validation_pass;
	bool cp_tx_validation_pass;
	bool cp_button_validation_pass;

	int32_t *cp_sensor_rx_raw_data;
	int32_t *cp_sensor_tx_raw_data;
	int32_t cp_sensor_rx_delta;
	int32_t cp_sensor_tx_delta;
	int32_t cp_sensor_rx_calibration;
	int32_t cp_sensor_tx_calibration;
	int32_t *cp_button_raw_data;
	int32_t cp_button_delta;

	/*other validation*/
	bool short_test_pass;
	bool test_summary;
};

static struct pt_core_commands *cmd;

static struct pt_module device_access_module;

static ssize_t pt_run_and_get_selftest_result(struct device *dev,
		int protect, char *buf, size_t buf_len, u8 test_id,
		u16 read_length, bool get_result_on_pass,
		bool print_results, u8 print_format);

static int _pt_calibrate_idacs_cmd(struct device *dev,
		u8 sensing_mode, u8 *status);

static int pt_perform_calibration(struct device *dev);

/*******************************************************************************
 * FUNCTION: pt_get_device_access_data
 *
 * SUMMARY: Inline function to get pt_device_access_data.
 *
 * RETURN:
 *	 pointer to pt_device_access_data structure
 *
 * PARAMETERS:
 *	*dev          - pointer to device structure
 ******************************************************************************/
static inline struct pt_device_access_data *pt_get_device_access_data(
		struct device *dev)
{
	return pt_get_module_data(dev, &device_access_module);
}

/*******************************************************************************
 * FUNCTION: cmcp_check_config_fw_match
 *
 * SUMMARY: Checks if tx,rx and btn num of firmware match with configuration.
 *
 * RETURN:
 *	 0 = match
 *	!0 = doesn't match
 *
 * PARAMETERS:
 *	*dev           - pointer to device structure
 *	*configuration - pointer to configuration structure
 ******************************************************************************/
static int cmcp_check_config_fw_match(struct device *dev,
	struct configuration *configuration)
{
	struct pt_device_access_data *dad
		= pt_get_device_access_data(dev);
	int32_t tx_num = dad->configs->tx_num;
	int32_t rx_num = dad->configs->rx_num;
	int32_t button_num = dad->configs->btn_num;
	int ret = 0;

	if (tx_num != dad->si->sensing_conf_data.tx_num) {
		pt_debug(dev, DL_ERROR,
			"%s: TX number mismatch! CSV=%d DUT=%d\n",
			__func__, tx_num, dad->si->sensing_conf_data.tx_num);
		ret = -EINVAL;
	}

	if (rx_num != dad->si->sensing_conf_data.rx_num) {
		pt_debug(dev, DL_ERROR,
			"%s: RX number mismatch! CSV=%d DUT=%d\n",
			__func__, rx_num, dad->si->sensing_conf_data.rx_num);
		ret = -EINVAL;
	}

	if (button_num != dad->si->num_btns) {
		pt_debug(dev, DL_ERROR,
			"%s: Button number mismatch! CSV=%d DUT=%d\n",
			__func__, button_num, dad->si->num_btns);
		ret = -EINVAL;
	}

	return ret;
}

/*******************************************************************************
 * FUNCTION: validate_cm_test_results
 *
 * SUMMARY: Checks cm test results and outputs each test and a summary result
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev           - pointer to device structure
 *  *configuration - pointer to configuration structure
 *  *cmcp_info     - pointer to cmcp_data structure to store cmcp data from fw
 *  *result        - pointer to result structure
 *  *pass          - pointer to bool value
 *   test_item     - flag to store all test item are requested
 ******************************************************************************/
static int validate_cm_test_results(struct device *dev,
	struct configuration *configuration, struct cmcp_data *cmcp_info,
	struct result *result, bool *pass, int test_item)
{
	int32_t tx_num = cmcp_info->tx_num;
	int32_t rx_num = cmcp_info->rx_num;
	int32_t button_num =  cmcp_info->btn_num;
	uint32_t sensor_num = tx_num * rx_num;
	int32_t *cm_sensor_data = cmcp_info->cm_data_panel;
	int32_t cm_button_delta;
	int32_t cm_sensor_calibration;
	int32_t *cm_button_data = cmcp_info->cm_btn_data;
	struct gd_sensor *gd_sensor_col = cmcp_info->gd_sensor_col;
	struct gd_sensor *gd_sensor_row = cmcp_info->gd_sensor_row;
	int32_t *cm_sensor_column_delta = cmcp_info->cm_sensor_column_delta;
	int32_t *cm_sensor_row_delta = cmcp_info->cm_sensor_row_delta;
	int ret = 0;
	int i, j;

	pt_debug(dev, DL_INFO, "%s: start\n", __func__);

	if ((test_item & CM_PANEL) == CM_PANEL) {
		pt_debug(dev, DL_INFO,
			"Check each sensor Cm data for min max value\n ");

		/* Check each sensor Cm data for min/max values */
		result->cm_sensor_validation_pass = true;

	for (i = 0; i < sensor_num; i++) {
		int row = i % rx_num;
		int col = i / rx_num;
		int32_t cm_sensor_min =
		  configuration->cm_min_max_table_sensor[(row*tx_num+col)*2];
		int32_t cm_sensor_max =
		  configuration->cm_min_max_table_sensor[(row*tx_num+col)*2+1];
		if ((cm_sensor_data[i] < cm_sensor_min) ||
		    (cm_sensor_data[i] > cm_sensor_max)) {
			pt_debug(dev, DL_WARN,
					"%s: Sensor[%d,%d]:%d (%d,%d)\n",
					"Cm sensor min/max test",
					row, col,
					cm_sensor_data[i],
					cm_sensor_min, cm_sensor_max);
			result->cm_sensor_validation_pass = false;
		}
	}

	/*check cm gradient column data*/
	result->cm_sensor_gd_col_pass = true;
	for (i = 0; i < configuration->cm_max_table_gradient_cols_percent_size;
	     i++) {
		if ((gd_sensor_col + i)->gradient_val >
		    10 * configuration->cm_max_table_gradient_cols_percent[i]) {
			pt_debug(dev, DL_WARN,
			"%s: cm_max_table_gradient_cols_percent[%d]:%d, gradient_val:%d\n",
			__func__, i,
			configuration->cm_max_table_gradient_cols_percent[i],
			(gd_sensor_col + i)->gradient_val);
			result->cm_sensor_gd_col_pass = false;
		}
	}

	/*check cm gradient row data*/
	result->cm_sensor_gd_row_pass = true;
	for (j = 0; j < configuration->cm_max_table_gradient_rows_percent_size;
	     j++) {
		if ((gd_sensor_row + j)->gradient_val >
		    10 * configuration->cm_max_table_gradient_rows_percent[j]) {
			pt_debug(dev, DL_WARN,
			"%s: cm_max_table_gradient_rows_percent[%d]:%d, gradient_val:%d\n",
			__func__, j,
			configuration->cm_max_table_gradient_rows_percent[j],
			(gd_sensor_row + j)->gradient_val);
			result->cm_sensor_gd_row_pass = false;
		}
	}

	result->cm_sensor_row_delta_pass = true;
	result->cm_sensor_col_delta_pass = true;
	result->cm_sensor_calibration_pass = true;
	result->cm_sensor_delta_pass = true;

	/* Check each row Cm data with neighbor for difference */
	for (i = 0; i < tx_num; i++) {
		for (j = 1; j < rx_num; j++) {
			int32_t cm_sensor_row_diff =
				ABS(cm_sensor_data[i * rx_num + j] -
				cm_sensor_data[i * rx_num + j - 1]);
			cm_sensor_row_delta[i * rx_num + j - 1] =
				cm_sensor_row_diff;
			if (cm_sensor_row_diff >
			    configuration->cm_range_limit_row) {
				pt_debug(dev, DL_DEBUG,
					"%s: Sensor[%d,%d]:%d (%d)\n",
					"Cm sensor row range limit test",
					j, i, cm_sensor_row_diff,
					configuration->cm_range_limit_row);
				result->cm_sensor_row_delta_pass = false;
			}
		}
	}

	/* Check each column Cm data with neighbor for difference */
	for (i = 1; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			int32_t cm_sensor_col_diff =
				ABS((int)cm_sensor_data[i * rx_num + j] -
				(int)cm_sensor_data[(i - 1) * rx_num + j]);
			cm_sensor_column_delta[(i - 1) * rx_num + j] =
				cm_sensor_col_diff;
			if (cm_sensor_col_diff >
			    configuration->cm_range_limit_col) {
				pt_debug(dev, DL_DEBUG,
					"%s: Sensor[%d,%d]:%d (%d)\n",
					"Cm sensor column range limit test",
					j, i, cm_sensor_col_diff,
					configuration->cm_range_limit_col);
				result->cm_sensor_col_delta_pass = false;
			}
		}
	}

	/* Check sensor calculated Cm for min/max values */
	cm_sensor_calibration = cmcp_info->cm_cal_data_panel;
	if (cm_sensor_calibration <
	    configuration->cm_min_limit_cal ||
	    cm_sensor_calibration > configuration->cm_max_limit_cal) {
		pt_debug(dev, DL_DEBUG, "%s: Cm_cal:%d (%d,%d)\n",
			"Cm sensor Cm_cal min/max test",
			cm_sensor_calibration,
			configuration->cm_min_limit_cal,
			configuration->cm_max_limit_cal);
		result->cm_sensor_calibration_pass = false;
	}

	/* Check sensor Cm delta for range limit */
	if (cmcp_info->cm_sensor_delta >
	    (10 * configuration->cm_max_delta_sensor_percent)) {
		pt_debug(dev, DL_DEBUG,
			"%s: Cm_sensor_delta:%d (%d)\n",
			"Cm sensor delta range limit test",
			cmcp_info->cm_sensor_delta,
			configuration->cm_max_delta_sensor_percent);
		result->cm_sensor_delta_pass = false;
	}

	result->cm_test_pass = result->cm_sensor_gd_col_pass
			&& result->cm_sensor_gd_row_pass
			&& result->cm_sensor_validation_pass
			&& result->cm_sensor_row_delta_pass
			&& result->cm_sensor_col_delta_pass
			&& result->cm_sensor_calibration_pass
			&& result->cm_sensor_delta_pass;
	}

	if (((test_item & CM_BTN) == CM_BTN) && (cmcp_info->btn_num)) {
		/* Check each button Cm data for min/max values */
		result->cm_button_validation_pass = true;
		for (i = 0; i < button_num; i++) {
			int32_t  cm_button_min =
				configuration->cm_min_max_table_btn[i * 2];
			int32_t  cm_button_max =
				configuration->cm_min_max_table_btn[i * 2 + 1];
			if ((cm_button_data[i] <= cm_button_min) ||
			    (cm_button_data[i] >= cm_button_max)) {
				pt_debug(dev, DL_DEBUG,
					"%s: Button[%d]:%d (%d,%d)\n",
					"Cm button min/max test",
					i, cm_button_data[i],
					cm_button_min, cm_button_max);
				result->cm_button_validation_pass = false;
			}
		}

		/* Check button Cm delta for range limit */
		result->cm_button_delta_pass = true;

		cm_button_delta = ABS((cmcp_info->cm_ave_data_btn -
			cmcp_info->cm_cal_data_btn) * 100 /
			cmcp_info->cm_ave_data_btn);
		if (cm_button_delta >
		    configuration->cm_max_delta_button_percent) {
			pt_debug(dev, DL_INFO,
				"%s: Cm_button_delta:%d (%d)\n",
				"Cm button delta range limit test",
				cm_button_delta,
				configuration->cm_max_delta_button_percent);
			result->cm_button_delta_pass = false;
		}

		result->cm_test_pass = result->cm_test_pass &&
				       result->cm_button_validation_pass &&
				       result->cm_button_delta_pass;
	}

	if (pass)
		*pass = result->cm_test_pass;

	return ret;
}

/*******************************************************************************
 * FUNCTION: validate_cp_test_results
 *
 * SUMMARY: Checks cp test results and outputs each test and a summary result.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev           - pointer to device structure
 *  *configuration - pointer to configuration structure
 *  *cmcp_info     - pointer to cmcp_data structure to store cmcp data from fw
 *  *result        - pointer to result structure
 *  *pass          - pointer to bool value
 *   test_item     - flag to store all test item are requested
 ******************************************************************************/
static int validate_cp_test_results(struct device *dev,
	struct configuration *configuration, struct cmcp_data *cmcp_info,
	struct result *result, bool *pass, int test_item)
{
	int i = 0;
	uint32_t configuration_rx_num;
	uint32_t configuration_tx_num;
	int32_t *cp_sensor_tx_data = cmcp_info->cp_tx_data_panel;
	int32_t *cp_sensor_rx_data = cmcp_info->cp_rx_data_panel;
	int32_t cp_button_delta;
	int32_t cp_button_average;

	result->cp_test_pass = true;
	configuration_rx_num = configuration->cp_min_max_table_rx_size/2;
	configuration_tx_num = configuration->cp_min_max_table_tx_size/2;

	pt_debug(dev, DL_INFO, "%s start\n", __func__);

	if ((test_item & CP_PANEL) == CP_PANEL) {
		int32_t cp_sensor_tx_delta;
		int32_t cp_sensor_rx_delta;

		/* Check Sensor Cp delta for range limit */
		result->cp_sensor_delta_pass = true;
		/*check cp_sensor_tx_delta */
		for (i = 0; i < configuration_tx_num; i++) {
			cp_sensor_tx_delta =
				ABS((cmcp_info->cp_tx_cal_data_panel[i]-
				cmcp_info->cp_tx_data_panel[i]) * 100 /
				cmcp_info->cp_tx_data_panel[i]);

			if (cp_sensor_tx_delta >
			    configuration->cp_max_delta_sensor_tx_percent) {
				pt_debug(dev, DL_DEBUG,
				"%s: Cp_sensor_tx_delta:%d (%d)\n",
				"Cp sensor delta range limit test",
				cp_sensor_tx_delta,
				configuration->cp_max_delta_sensor_tx_percent);
				result->cp_sensor_delta_pass = false;
			}
		}

		/*check cp_sensor_rx_delta */
		for (i = 0; i < configuration_rx_num; i++) {
			cp_sensor_rx_delta =
				ABS((cmcp_info->cp_rx_cal_data_panel[i] -
				cmcp_info->cp_rx_data_panel[i]) * 100 /
				cmcp_info->cp_rx_data_panel[i]);
			if (cp_sensor_rx_delta >
			    configuration->cp_max_delta_sensor_rx_percent) {
				pt_debug(dev, DL_DEBUG,
				"%s: Cp_sensor_rx_delta:%d(%d)\n",
				"Cp sensor delta range limit test",
				cp_sensor_rx_delta,
				configuration->cp_max_delta_sensor_rx_percent);
				result->cp_sensor_delta_pass = false;
			}
		}

		/* Check sensor Cp rx for min/max values */
		result->cp_rx_validation_pass = true;
		for (i = 0; i < configuration_rx_num; i++) {
			int32_t cp_rx_min =
				configuration->cp_min_max_table_rx[i * 2];
			int32_t cp_rx_max =
				configuration->cp_min_max_table_rx[i * 2 + 1];
			if ((cp_sensor_rx_data[i] <= cp_rx_min) ||
			    (cp_sensor_rx_data[i] >= cp_rx_max)) {
				pt_debug(dev, DL_DEBUG,
					"%s: Cp Rx[%d]:%d (%d,%d)\n",
					"Cp Rx min/max test",
					i, (int)cp_sensor_rx_data[i],
					cp_rx_min, cp_rx_max);
				result->cp_rx_validation_pass = false;
			}
		}

		/* Check sensor Cp tx for min/max values */
		result->cp_tx_validation_pass = true;
		for (i = 0; i < configuration_tx_num; i++) {
			int32_t cp_tx_min =
				configuration->cp_min_max_table_tx[i * 2];
			int32_t cp_tx_max =
				configuration->cp_min_max_table_tx[i * 2 + 1];
			if ((cp_sensor_tx_data[i] <= cp_tx_min) ||
			    (cp_sensor_tx_data[i] >= cp_tx_max)) {
				pt_debug(dev, DL_DEBUG,
					"%s: Cp Tx[%d]:%d(%d,%d)\n",
					"Cp Tx min/max test",
					i, cp_sensor_tx_data[i],
					cp_tx_min, cp_tx_max);
				result->cp_tx_validation_pass = false;
			}
		}

		result->cp_test_pass = result->cp_test_pass
				&& result->cp_sensor_delta_pass
				&& result->cp_rx_validation_pass
				&& result->cp_tx_validation_pass;
	}

	if (((test_item & CP_BTN) == CP_BTN) && (cmcp_info->btn_num)) {
		result->cp_button_delta_pass = true;

		/* Check button Cp delta for range limit */
		cp_button_delta = ABS((cmcp_info->cp_btn_cal
		- cmcp_info->cp_button_ave) * 100 /
		cmcp_info->cp_button_ave);
		if (cp_button_delta >
		    configuration->cp_max_delta_button_percent) {
			pt_debug(dev, DL_INFO,
				"%s: Cp_button_delta:%d (%d)\n",
				"Cp button delta range limit test",
				cp_button_delta,
				configuration->cp_max_delta_button_percent);
			result->cp_button_delta_pass = false;
		}

		/* Check button Cp average for min/max values */
		result->cp_button_average_pass = true;
		cp_button_average = cmcp_info->cp_button_ave;
		if (cp_button_average < configuration->min_button ||
		    cp_button_average > configuration->max_button) {
			pt_debug(dev, DL_INFO,
				"%s: Button Cp average fails min/max test\n",
				__func__);
			pt_debug(dev, DL_INFO,
				"%s: Cp_button_average:%d (%d,%d)\n",
				"Cp button average min/max test",
				cp_button_average,
				configuration->min_button,
				configuration->max_button);
			result->cp_button_average_pass = false;
		}

		/* Check each button Cp data for min/max values */
		result->cp_button_validation_pass = true;
		for (i = 0; i < cmcp_info->btn_num; i++) {
			int32_t  cp_button_min =
				configuration->cp_min_max_table_btn[i * 2];
			int32_t  cp_button_max =
				configuration->cp_min_max_table_btn[i * 2 + 1];
			if ((cmcp_info->cp_btn_data[i] <= cp_button_min) ||
			    (cmcp_info->cp_btn_data[i] >= cp_button_max)) {
				pt_debug(dev, DL_DEBUG,
					"%s: Button[%d]:%d (%d,%d)\n",
					"Cp button min/max test",
					i, cmcp_info->cp_btn_data[i],
					cp_button_min, cp_button_max);
				result->cp_button_validation_pass = false;
			}
		}

		result->cp_test_pass = result->cp_test_pass
				&& result->cp_button_delta_pass
				&& result->cp_button_average_pass
				&& result->cp_button_validation_pass;
	}

	if (pass)
		*pass = result->cp_test_pass;

	return 0;
}

/*******************************************************************************
 * FUNCTION: calculate_gradient_row
 *
 * SUMMARY: Calculates gradient value for rows.
 *
 * PARAMETERS:
 *  *gd_sensor_row_head - pointer to gd_sensor structure
 *   row_num            - number of row
 *   exclude_row_edge   - flag to exclude row edge(1:exclude; 0:include)
 *   exclude_col_edge   - flag to exclude column edge(1:exclude; 0:include)
 ******************************************************************************/
static void calculate_gradient_row(struct gd_sensor *gd_sensor_row_head,
		 uint16_t row_num, int exclude_row_edge, int exclude_col_edge)
{
	int i = 0;
	uint16_t cm_min_cur = 0;
	uint16_t cm_max_cur = 0;
	uint16_t cm_ave_cur = 0;
	uint16_t cm_ave_next = 0;
	uint16_t cm_ave_prev = 0;
	struct gd_sensor *p = gd_sensor_row_head;

	if (exclude_row_edge) {
		for (i = 0; i < row_num; i++) {
			if (!exclude_col_edge) {
				cm_ave_cur = (p + i)->cm_ave;
				cm_min_cur = (p + i)->cm_min;
				cm_max_cur = (p + i)->cm_max;
				if (i < (row_num-1))
					cm_ave_next = (p + i+1)->cm_ave;
				if (i > 0)
					cm_ave_prev = (p + i-1)->cm_ave;
			} else {
				cm_ave_cur = (p + i)->cm_ave_exclude_edge;
				cm_min_cur = (p + i)->cm_min_exclude_edge;
				cm_max_cur = (p + i)->cm_max_exclude_edge;
				if (i < (row_num-1))
					cm_ave_next =
					(p + i+1)->cm_ave_exclude_edge;
				if (i > 0)
					cm_ave_prev =
					(p + i-1)->cm_ave_exclude_edge;
			}

			if (cm_ave_cur == 0)
				cm_ave_cur = 1;

			/*multiple 1000 to increate accuracy*/
			if ((i == 0) || (i == (row_num-1))) {
				(p + i)->gradient_val =
				(cm_max_cur - cm_min_cur) * 1000 /
				cm_ave_cur;
			} else if (i == 1) {
				(p + i)->gradient_val = (cm_max_cur - cm_min_cur
				+ ABS(cm_ave_cur - cm_ave_next)) * 1000 /
				cm_ave_cur;
			} else {
				(p + i)->gradient_val = (cm_max_cur - cm_min_cur
				+ ABS(cm_ave_cur - cm_ave_prev)) * 1000 /
				cm_ave_cur;
			}
		}
	} else if (!exclude_row_edge) {
		for (i = 0; i < row_num; i++) {
			if (!exclude_col_edge) {
				cm_ave_cur = (p + i)->cm_ave;
				cm_min_cur = (p + i)->cm_min;
				cm_max_cur = (p + i)->cm_max;
				if (i < (row_num-1))
					cm_ave_next = (p + i+1)->cm_ave;
				if (i > 0)
					cm_ave_prev = (p + i-1)->cm_ave;
			} else {
				cm_ave_cur = (p + i)->cm_ave_exclude_edge;
				cm_min_cur = (p + i)->cm_min_exclude_edge;
				cm_max_cur = (p + i)->cm_max_exclude_edge;
				if (i < (row_num-1))
					cm_ave_next =
					(p + i+1)->cm_ave_exclude_edge;
				if (i > 0)
					cm_ave_prev =
					(p + i-1)->cm_ave_exclude_edge;
			}

			if (cm_ave_cur == 0)
				cm_ave_cur = 1;
			/*multiple 1000 to increate accuracy*/
			if (i <= 1)
				(p + i)->gradient_val = (cm_max_cur - cm_min_cur
				+ ABS(cm_ave_cur - cm_ave_next)) * 1000 /
				cm_ave_cur;
			else
				(p + i)->gradient_val = (cm_max_cur - cm_min_cur
				+ ABS(cm_ave_cur - cm_ave_prev)) * 1000 /
				cm_ave_cur;
		}
	}
}

/*******************************************************************************
 * FUNCTION: calculate_gradient_col
 *
 * SUMMARY: Calculates gradient value for columns.
 *
 * PARAMETERS:
 *  *gd_sensor_row_head - pointer to gd_sensor structure
 *   col_num            - number of column
 *   exclude_row_edge   - flag to exclude row edge(1:exclude; 0:include)
 *   exclude_col_edge   - flag to exclude column edge(1:exclude; 0:include)
 ******************************************************************************/
static void calculate_gradient_col(struct gd_sensor *gd_sensor_row_head,
	uint16_t col_num, int exclude_row_edge, int exclude_col_edge)
{
	int i = 0;
	int32_t cm_min_cur = 0;
	int32_t cm_max_cur = 0;
	int32_t cm_ave_cur = 0;
	int32_t cm_ave_next = 0;
	int32_t cm_ave_prev = 0;
	struct gd_sensor *p = gd_sensor_row_head;

	if (!exclude_col_edge) {
		for (i = 0; i < col_num; i++) {
			if (!exclude_row_edge) {
				cm_ave_cur = (p + i)->cm_ave;
				cm_min_cur = (p + i)->cm_min;
				cm_max_cur = (p + i)->cm_max;
				if (i < (col_num-1))
					cm_ave_next = (p + i+1)->cm_ave;
				if (i > 0)
					cm_ave_prev = (p + i-1)->cm_ave;
			} else {
				cm_ave_cur = (p + i)->cm_ave_exclude_edge;
				cm_min_cur = (p + i)->cm_min_exclude_edge;
				cm_max_cur = (p + i)->cm_max_exclude_edge;
				if (i < (col_num-1))
					cm_ave_next =
					(p + i+1)->cm_ave_exclude_edge;
				if (i > 0)
					cm_ave_prev =
					(p + i-1)->cm_ave_exclude_edge;
			}
			if (cm_ave_cur == 0)
				cm_ave_cur = 1;
			/*multiple 1000 to increate accuracy*/
			if (i <= 1)
				(p + i)->gradient_val = (cm_max_cur - cm_min_cur
				+ ABS(cm_ave_cur - cm_ave_next)) * 1000 /
				cm_ave_cur;
			else
				(p + i)->gradient_val = (cm_max_cur - cm_min_cur
				+ ABS(cm_ave_cur - cm_ave_prev)) * 1000 /
				cm_ave_cur;
		}
	} else if (exclude_col_edge) {
		for (i = 0; i < col_num; i++) {
			if (!exclude_row_edge) {
				cm_ave_cur = (p + i)->cm_ave;
				cm_min_cur = (p + i)->cm_min;
				cm_max_cur = (p + i)->cm_max;
				if (i < (col_num-1))
					cm_ave_next = (p + i+1)->cm_ave;
				if (i > 0)
					cm_ave_prev = (p + i-1)->cm_ave;
			} else {
				cm_ave_cur = (p + i)->cm_ave_exclude_edge;
				cm_min_cur = (p + i)->cm_min_exclude_edge;
				cm_max_cur = (p + i)->cm_max_exclude_edge;
				if (i < (col_num-1))
					cm_ave_next =
					(p + i+1)->cm_ave_exclude_edge;
				if (i > 0)
					cm_ave_prev =
					(p + i-1)->cm_ave_exclude_edge;
			}

			if (cm_ave_cur == 0)
				cm_ave_cur = 1;
			/*multiple 1000 to increate accuracy*/
			if ((i == 0) || (i == (col_num - 1)))
				(p + i)->gradient_val =
					 (cm_max_cur - cm_min_cur) * 1000 /
					 cm_ave_cur;
			else if (i == 1)
				(p + i)->gradient_val =
					(cm_max_cur - cm_min_cur +
					ABS(cm_ave_cur - cm_ave_next))
					 * 1000 / cm_ave_cur;
			else
				(p + i)->gradient_val =
					(cm_max_cur - cm_min_cur +
					ABS(cm_ave_cur - cm_ave_prev))
					* 1000 / cm_ave_cur;
			}
	}
}

/*******************************************************************************
 * FUNCTION: fill_gd_sensor_table
 *
 * SUMMARY: Fills cm calculation result and exclude parameter to gd_sensor
 *  structure.
 *
 * PARAMETERS:
 *  *head                - pointer to gd_sensor structure
 *   index               - index of row or column
 *   cm_max              - maximum of cm
 *   cm_min              - minmum of cm
 *   cm_ave              - average of cm
 *   cm_max_exclude_edge - maximum of cm without edge data
 *   cm_min_exclude_edge - minmum of cm without edge data
 *   cm_ave_exclude_edge - average of cm without edge data
 ******************************************************************************/
static void fill_gd_sensor_table(struct gd_sensor *head, int32_t index,
	int32_t cm_max, int32_t cm_min,	int32_t cm_ave,
	int32_t cm_max_exclude_edge, int32_t cm_min_exclude_edge,
	int32_t cm_ave_exclude_edge)
{
	(head + index)->cm_max = cm_max;
	(head + index)->cm_min = cm_min;
	(head + index)->cm_ave = cm_ave;
	(head + index)->cm_ave_exclude_edge = cm_ave_exclude_edge;
	(head + index)->cm_max_exclude_edge = cm_max_exclude_edge;
	(head + index)->cm_min_exclude_edge = cm_min_exclude_edge;
}

/*******************************************************************************
 * FUNCTION: calculate_gd_info
 *
 * SUMMARY: Calculates gradient panel sensor column and row by calling
 *  function calculate_gradient_col() & calculate_gradient_row().
 *
 * PARAMETERS:
 *  *head                - pointer to gd_sensor structure
 *   index               - index of row or column
 *   cm_max              - maximum of cm
 *   cm_min              - minmum of cm
 *   cm_ave              - average of cm
 *   cm_max_exclude_edge - maximum of cm without edge data
 *   cm_min_exclude_edge - minmum of cm without edge data
 *   cm_ave_exclude_edge - average of cm without edge data
 ******************************************************************************/
static void calculate_gd_info(struct gd_sensor *gd_sensor_col,
	struct gd_sensor *gd_sensor_row, int tx_num, int rx_num,
	int32_t *cm_sensor_data, int cm_excluding_row_edge,
	int cm_excluding_col_edge)
{
	int32_t cm_max;
	int32_t cm_min;
	int32_t cm_ave;
	int32_t cm_max_exclude_edge;
	int32_t cm_min_exclude_edge;
	int32_t cm_ave_exclude_edge;
	int32_t cm_data;
	int i;
	int j;

	/*calculate all the gradient related info for column*/
	for (i = 0; i < tx_num; i++) {
		/*re-initialize for a new col*/
		cm_max = cm_sensor_data[i * rx_num];
		cm_min = cm_max;
		cm_ave = 0;
		cm_max_exclude_edge = cm_sensor_data[i * rx_num + 1];
		cm_min_exclude_edge = cm_max_exclude_edge;
		cm_ave_exclude_edge = 0;

		for (j = 0; j < rx_num; j++) {
			cm_data = cm_sensor_data[i * rx_num + j];
			if (cm_data > cm_max)
				cm_max = cm_data;
			if (cm_data < cm_min)
				cm_min = cm_data;
			cm_ave += cm_data;
			/*calculate exclude edge data*/
			if ((j > 0) && (j < (rx_num-1))) {
				if (cm_data > cm_max_exclude_edge)
					cm_max_exclude_edge = cm_data;
				if (cm_data < cm_min_exclude_edge)
					cm_min_exclude_edge = cm_data;
				cm_ave_exclude_edge += cm_data;
			}
		}
		cm_ave /= rx_num;
		cm_ave_exclude_edge /= (rx_num-2);
		fill_gd_sensor_table(gd_sensor_col, i, cm_max, cm_min, cm_ave,
		cm_max_exclude_edge, cm_min_exclude_edge, cm_ave_exclude_edge);
	}

	calculate_gradient_col(gd_sensor_col, tx_num, cm_excluding_row_edge,
		 cm_excluding_col_edge);

	/*calculate all the gradient related info for row*/
	for (j = 0; j < rx_num; j++) {
		/*re-initialize for a new row*/
		cm_max = cm_sensor_data[j];
		cm_min = cm_max;
		cm_ave = 0;
		cm_max_exclude_edge = cm_sensor_data[rx_num + j];
		cm_min_exclude_edge = cm_max_exclude_edge;
		cm_ave_exclude_edge = 0;
		for (i = 0; i < tx_num; i++) {
			cm_data = cm_sensor_data[i * rx_num + j];
			if (cm_data > cm_max)
				cm_max = cm_data;
			if (cm_data < cm_min)
				cm_min = cm_data;
			cm_ave += cm_data;
			/*calculate exclude edge data*/
			if ((i >  0) && (i < (tx_num-1))) {
				if (cm_data > cm_max_exclude_edge)
					cm_max_exclude_edge = cm_data;
				if (cm_data < cm_min_exclude_edge)
					cm_min_exclude_edge = cm_data;
				cm_ave_exclude_edge += cm_data;
			}
		}
		cm_ave /= tx_num;
		cm_ave_exclude_edge /= (tx_num-2);
		fill_gd_sensor_table(gd_sensor_row, j, cm_max, cm_min, cm_ave,
		cm_max_exclude_edge, cm_min_exclude_edge, cm_ave_exclude_edge);
	}
	calculate_gradient_row(gd_sensor_row, rx_num, cm_excluding_row_edge,
		 cm_excluding_col_edge);
}

/*******************************************************************************
 * FUNCTION: pt_get_cmcp_info
 *
 * SUMMARY: Function to include following work:
 *  1) run short test and get result
 *  2) run selftest to get cm_panel data, cm_cal_data_panel data, calculate
 *     cm_ave_data_panel, cm_sensor_delta and gradient by column and row.
 *  3) run selftest to get cp_panel data, cp_cal_data_panel data, cacluate
 *     cp_ave_data_panel, cp_sensor_delta for tx and rx.
 *  4) run selftest to get cm_btn data, cm_cal_data_btn data, cacluate
 *     cm_delta_data_btn data, cm_ave_data_btn data.
 *  5) run selftest to get cp_btn data, cp_btn_cal data, cacluate
 *     cp_button_delta data, cp_button_ave data.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *   *dad       - pointer to pt_device_access_data structure
 *   *cmcp_info - pointer to cmcp_data structure
 ******************************************************************************/
static int  pt_get_cmcp_info(struct pt_device_access_data *dad,
	struct cmcp_data *cmcp_info)
{
	struct device *dev;
	int32_t *cm_data_panel = cmcp_info->cm_data_panel;
	int32_t *cp_tx_data_panel = cmcp_info->cp_tx_data_panel;
	int32_t *cp_rx_data_panel = cmcp_info->cp_rx_data_panel;
	int32_t *cp_tx_cal_data_panel = cmcp_info->cp_tx_cal_data_panel;
	int32_t *cp_rx_cal_data_panel = cmcp_info->cp_rx_cal_data_panel;
	int32_t *cm_btn_data = cmcp_info->cm_btn_data;
	int32_t *cp_btn_data = cmcp_info->cp_btn_data;
	struct gd_sensor *gd_sensor_col = cmcp_info->gd_sensor_col;
	struct gd_sensor *gd_sensor_row = cmcp_info->gd_sensor_row;
	struct result *result = dad->result;
	int32_t cp_btn_cal = 0;
	int32_t cp_btn_ave = 0;
	int32_t cm_ave_data_panel = 0;
	int32_t cm_ave_data_btn = 0;
	int32_t cp_tx_ave_data_panel = 0;
	int32_t cp_rx_ave_data_panel = 0;
	u8 tmp_buf[3];
	int tx_num;
	int rx_num;
	int btn_num;
	int rc = 0;
	int i;

	dev = dad->dev;
	cmcp_info->tx_num = dad->si->sensing_conf_data.tx_num;
	cmcp_info->rx_num = dad->si->sensing_conf_data.rx_num;
	cmcp_info->btn_num = dad->si->num_btns;

	tx_num = cmcp_info->tx_num;
	rx_num = cmcp_info->rx_num;
	btn_num = cmcp_info->btn_num;
	pt_debug(dev, DL_INFO, "%s tx_num=%d", __func__, tx_num);
	pt_debug(dev, DL_INFO, "%s rx_num=%d", __func__, rx_num);
	pt_debug(dev, DL_INFO, "%s btn_num=%d", __func__, btn_num);

	/*short test*/
	result->short_test_pass = true;
	rc = pt_run_and_get_selftest_result(dev, PT_CORE_CMD_UNPROTECTED,
		tmp_buf, sizeof(tmp_buf),
		PT_ST_ID_AUTOSHORTS, PIP_CMD_MAX_LENGTH,
		PT_ST_DONT_GET_RESULTS, PT_ST_NOPRINT, PT_PR_FORMAT_DEFAULT);
	if (rc) {
		pt_debug(dev, DL_ERROR, "short test not supported");
		goto exit;
	}
	if (dad->ic_buf[1] != 0)
		result->short_test_pass = false;

	/*Get cm_panel data*/
	rc = pt_run_and_get_selftest_result(dev, PT_CORE_CMD_UNPROTECTED,
		tmp_buf, sizeof(tmp_buf),
		PT_ST_ID_CM_PANEL, PIP_CMD_MAX_LENGTH,
		PT_ST_GET_RESULTS, PT_ST_NOPRINT, PT_PR_FORMAT_DEFAULT);
	if (rc) {
		pt_debug(dev, DL_ERROR, "Get CM Panel not supported");
		goto exit;
	}
	if (cm_data_panel != NULL) {
		for (i = 0; i < tx_num * rx_num;  i++) {
			cm_data_panel[i] =
			10*(dad->ic_buf[CM_PANEL_DATA_OFFSET+i*2] + 256
			* dad->ic_buf[CM_PANEL_DATA_OFFSET+i*2+1]);
			pt_debug(dev, DL_DEBUG,
				"cm_data_panel[%d]=%d\n",
				i, cm_data_panel[i]);
			cm_ave_data_panel += cm_data_panel[i];
		}
		cm_ave_data_panel /= (tx_num * rx_num);
		cmcp_info->cm_ave_data_panel = cm_ave_data_panel;
	}

	/* Calculate gradient panel sensor column/row here */
	calculate_gd_info(gd_sensor_col, gd_sensor_row, tx_num, rx_num,
		cm_data_panel, 1, 1);
	for (i = 0; i < tx_num; i++) {
		pt_debug(dev, DL_DEBUG,
			"i=%d max=%d,min=%d,ave=%d, gradient=%d", i,
			gd_sensor_col[i].cm_max,
			gd_sensor_col[i].cm_min,
			gd_sensor_col[i].cm_ave,
			gd_sensor_col[i].gradient_val);
	}

	for (i = 0; i < rx_num; i++) {
		pt_debug(dev, DL_DEBUG,
			"i=%d max=%d,min=%d,ave=%d, gradient=%d", i,
			gd_sensor_row[i].cm_max,
			gd_sensor_row[i].cm_min,
			gd_sensor_row[i].cm_ave,
			gd_sensor_row[i].gradient_val);
	}

	/*Get cp data*/
	rc = pt_run_and_get_selftest_result(dev, PT_CORE_CMD_UNPROTECTED,
		tmp_buf, sizeof(tmp_buf),
		PT_ST_ID_CP_PANEL, PIP_CMD_MAX_LENGTH,
		PT_ST_GET_RESULTS, PT_ST_NOPRINT, PT_PR_FORMAT_DEFAULT);
	if (rc) {
		pt_debug(dev, DL_ERROR, "Get CP Panel not supported");
		goto exit;
	}
	/*Get cp_tx_data_panel*/
	if (cp_tx_data_panel != NULL) {
		for (i = 0; i < tx_num; i++) {
			cp_tx_data_panel[i] =
			10*(dad->ic_buf[CP_PANEL_DATA_OFFSET+i*2]
			+ 256 * dad->ic_buf[CP_PANEL_DATA_OFFSET+i*2+1]);
			pt_debug(dev, DL_DEBUG,
				"cp_tx_data_panel[%d]=%d\n",
				i, cp_tx_data_panel[i]);
			cp_tx_ave_data_panel += cp_tx_data_panel[i];
		}
		cp_tx_ave_data_panel /= tx_num;
		cmcp_info->cp_tx_ave_data_panel = cp_tx_ave_data_panel;
	}

	/*Get cp_tx_cal_data_panel*/
	if (cp_tx_cal_data_panel != NULL) {
		for (i = 0; i < tx_num; i++) {
			cp_tx_cal_data_panel[i] =
			10*(dad->ic_buf[CP_PANEL_DATA_OFFSET+tx_num*2+i*2]
		+ 256 * dad->ic_buf[CP_PANEL_DATA_OFFSET+tx_num*2+i*2+1]);
			pt_debug(dev, DL_DEBUG, "cp_tx_cal_data_panel[%d]=%d\n",
				i, cp_tx_cal_data_panel[i]);
		}
	}

	/*get cp_sensor_tx_delta,using the first sensor cal value for temp */
	/*multiple 1000 to increase accuracy*/
	cmcp_info->cp_sensor_tx_delta = ABS((cp_tx_cal_data_panel[0]
		- cp_tx_ave_data_panel) * 1000 / cp_tx_ave_data_panel);

	/*Get cp_rx_data_panel*/
	if (cp_rx_data_panel != NULL) {
		for (i = 0; i < rx_num;  i++) {
			cp_rx_data_panel[i] =
			10*(dad->ic_buf[CP_PANEL_DATA_OFFSET+tx_num*4+i*2] +
			256 * dad->ic_buf[CP_PANEL_DATA_OFFSET+tx_num*4+i*2+1]);
			pt_debug(dev, DL_DEBUG,
				"cp_rx_data_panel[%d]=%d\n", i,
				cp_rx_data_panel[i]);
			cp_rx_ave_data_panel += cp_rx_data_panel[i];
		}
		cp_rx_ave_data_panel /= rx_num;
		cmcp_info->cp_rx_ave_data_panel = cp_rx_ave_data_panel;
	}

	/*Get cp_rx_cal_data_panel*/
	if (cp_rx_cal_data_panel != NULL) {
		for (i = 0; i < rx_num; i++) {
			cp_rx_cal_data_panel[i] =
		10 * (dad->ic_buf[CP_PANEL_DATA_OFFSET+tx_num*4+rx_num*2+i*2] +
		256 *
		dad->ic_buf[CP_PANEL_DATA_OFFSET+tx_num*4+rx_num*2+i*2+1]);
			pt_debug(dev, DL_DEBUG,
				"cp_rx_cal_data_panel[%d]=%d\n", i,
				cp_rx_cal_data_panel[i]);
		}
	}

	/*get cp_sensor_rx_delta,using the first sensor cal value for temp */
	/*multiple 1000 to increase accuracy*/
	cmcp_info->cp_sensor_rx_delta = ABS((cp_rx_cal_data_panel[0]
		- cp_rx_ave_data_panel) * 1000 / cp_rx_ave_data_panel);

	if (btn_num == 0) {
		pt_debug(dev, DL_INFO, "%s: Skip Button Test\n", __func__);
		goto skip_button_test;
	}

	/*get cm btn data*/
	rc = pt_run_and_get_selftest_result(dev, PT_CORE_CMD_UNPROTECTED,
		tmp_buf, sizeof(tmp_buf),
		PT_ST_ID_CM_BUTTON, PIP_CMD_MAX_LENGTH,
		PT_ST_GET_RESULTS, PT_ST_NOPRINT, PT_PR_FORMAT_DEFAULT);
	if (rc) {
		pt_debug(dev, DL_ERROR, "Get CM BTN not supported");
		goto exit;
	}
	if (cm_btn_data != NULL) {
		for (i = 0; i < btn_num; i++) {
			cm_btn_data[i] =
			10 * (dad->ic_buf[CM_BTN_DATA_OFFSET+i*2] +
			256 * dad->ic_buf[CM_BTN_DATA_OFFSET+i*2+1]);
			pt_debug(dev, DL_DEBUG,
				" cm_btn_data[%d]=%d\n",
				i, cm_btn_data[i]);
			cm_ave_data_btn += cm_btn_data[i];
		}
		cm_ave_data_btn /= btn_num;
		cmcp_info->cm_ave_data_btn = cm_ave_data_btn;
	}

	/*get cp btn data*/
	rc = pt_run_and_get_selftest_result(dev, PT_CORE_CMD_UNPROTECTED,
		tmp_buf, sizeof(tmp_buf),
		PT_ST_ID_CP_BUTTON, PIP_CMD_MAX_LENGTH,
		PT_ST_GET_RESULTS, PT_ST_NOPRINT, PT_PR_FORMAT_DEFAULT);
	if (rc) {
		pt_debug(dev, DL_ERROR, "Get CP BTN not supported");
		goto exit;
	}
	if (cp_btn_data != NULL) {
		for (i = 0; i < btn_num; i++) {
			cp_btn_data[i] =
			10 * (dad->ic_buf[CP_BTN_DATA_OFFSET+i*2] +
			256 * dad->ic_buf[CP_BTN_DATA_OFFSET+i*2+1]);
			cp_btn_ave += cp_btn_data[i];
			pt_debug(dev, DL_DEBUG,
				"cp_btn_data[%d]=%d\n",
				i, cp_btn_data[i]);
		}
		cp_btn_ave /= btn_num;
		cp_btn_cal = 10*(dad->ic_buf[CP_BTN_DATA_OFFSET+i*2]
			 + 256 * dad->ic_buf[CP_BTN_DATA_OFFSET+i*2+1]);
		cmcp_info->cp_button_ave = cp_btn_ave;
		cmcp_info->cp_btn_cal = cp_btn_cal;
		/*multiple 1000 to increase accuracy*/
		cmcp_info->cp_button_delta = ABS((cp_btn_cal
		- cp_btn_ave) * 1000 / cp_btn_ave);
		pt_debug(dev, DL_INFO, " cp_btn_cal=%d\n",
			cp_btn_cal);
		pt_debug(dev, DL_INFO, " cp_btn_ave=%d\n",
			cp_btn_ave);
	}

skip_button_test:
exit:
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_get_cm_cal
 *
 * SUMMARY: Function to include following work:
 *  1) run selftest to get cm_cal_data_panel, cm_sensor_delta
 *  2) run selftest to get cm_cal_data_btn, cm_delta_data_btn
 *
 * NOTE:
 *  This function depends on the calculation result of pt_get_cmcp_info()
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *   *dad       - pointer to pt_device_access_data structure
 *   *cmcp_info - pointer to cmcp_data structure
 ******************************************************************************/
static int pt_get_cm_cal(struct pt_device_access_data *dad,
	struct cmcp_data *cmcp_info)
{
	struct device *dev;
	int32_t *cm_data_panel = cmcp_info->cm_data_panel;
	int32_t *cm_btn_data = cmcp_info->cm_btn_data;
	u8 tmp_buf[3];
	int rc = 0;
	int i;

	dev = dad->dev;

	/*Get cm_cal data*/
	rc = pt_run_and_get_selftest_result(dev, PT_CORE_CMD_UNPROTECTED,
			tmp_buf, sizeof(tmp_buf),
			PT_ST_ID_CM_PANEL, PIP_CMD_MAX_LENGTH,
			PT_ST_GET_RESULTS, PT_ST_NOPRINT, PT_PR_FORMAT_DEFAULT);
	if (rc) {
		pt_debug(dev, DL_ERROR, "Get CM Panel not supported");
		goto exit;
	}
	if (cm_data_panel != NULL) {
		i = cmcp_info->tx_num * cmcp_info->rx_num;
		cmcp_info->cm_cal_data_panel =
		    10 * (dad->ic_buf[CM_PANEL_DATA_OFFSET + i * 2] +
			  256 * dad->ic_buf[CM_PANEL_DATA_OFFSET + i * 2 + 1]);
		/*multiple 1000 to increase accuracy*/
		cmcp_info->cm_sensor_delta =
		    ABS((cmcp_info->cm_ave_data_panel -
			 cmcp_info->cm_cal_data_panel) *
			1000 / cmcp_info->cm_ave_data_panel);
	}

	if (cmcp_info->btn_num == 0) {
		pt_debug(dev, DL_INFO, "%s: Skip Button Test\n", __func__);
		goto skip_button_test;
	}

	/*get cm_btn_cal data*/
	rc = pt_run_and_get_selftest_result(dev, PT_CORE_CMD_UNPROTECTED,
			tmp_buf, sizeof(tmp_buf),
			PT_ST_ID_CM_BUTTON, PIP_CMD_MAX_LENGTH,
			PT_ST_GET_RESULTS, PT_ST_NOPRINT, PT_PR_FORMAT_DEFAULT);
	if (rc) {
		pt_debug(dev, DL_ERROR, "Get CM BTN not supported");
		goto exit;
	}
	if (cm_btn_data != NULL) {
		i = cmcp_info->btn_num;
		cmcp_info->cm_cal_data_btn =
		    10 * (dad->ic_buf[CM_BTN_DATA_OFFSET + i * 2] +
			  256 * dad->ic_buf[CM_BTN_DATA_OFFSET + i * 2 + 1]);
		/*multiple 1000 to increase accuracy*/
		cmcp_info->cm_delta_data_btn = ABS(
		    (cmcp_info->cm_ave_data_btn - cmcp_info->cm_cal_data_btn) *
		    1000 / cmcp_info->cm_ave_data_btn);
		pt_debug(dev, DL_INFO, " cm_btn_cal=%d\n",
			 cmcp_info->cm_cal_data_btn);
	}

skip_button_test:
exit:
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_free_cmcp_buf
 *
 * SUMMARY: Free pointers in cmcp_data structure
 *
 * PARAMETERS:
 *  *cmcp_info  - pointer to cmcp_data structure
 ******************************************************************************/
static void pt_free_cmcp_buf(struct cmcp_data *cmcp_info)
{
	if (cmcp_info->gd_sensor_col != NULL)
		kfree(cmcp_info->gd_sensor_col);
	if (cmcp_info->gd_sensor_row != NULL)
		kfree(cmcp_info->gd_sensor_row);
	if (cmcp_info->cm_data_panel != NULL)
		kfree(cmcp_info->cm_data_panel);
	if (cmcp_info->cp_tx_data_panel != NULL)
		kfree(cmcp_info->cp_tx_data_panel);
	if (cmcp_info->cp_rx_data_panel != NULL)
		kfree(cmcp_info->cp_rx_data_panel);
	if (cmcp_info->cp_tx_cal_data_panel != NULL)
		kfree(cmcp_info->cp_tx_cal_data_panel);
	if (cmcp_info->cp_rx_cal_data_panel != NULL)
		kfree(cmcp_info->cp_rx_cal_data_panel);
	if (cmcp_info->cm_btn_data != NULL)
		kfree(cmcp_info->cm_btn_data);
	if (cmcp_info->cp_btn_data != NULL)
		kfree(cmcp_info->cp_btn_data);
	if (cmcp_info->cm_sensor_column_delta != NULL)
		kfree(cmcp_info->cm_sensor_column_delta);
	if (cmcp_info->cm_sensor_row_delta != NULL)
		kfree(cmcp_info->cm_sensor_row_delta);
}

/*******************************************************************************
 * FUNCTION: pt_cmcp_get_test_item
 *
 * SUMMARY: Parses enum cmcp_test_item to integer value test_item as bitwise
 *  type.
 *
 * RETURN: integer value to indidate available test item with bitwise type
 *
 * PARAMETERS:
 *	 item_input  - enum cmcp_test_item
 ******************************************************************************/
static int pt_cmcp_get_test_item(int item_input)
{
	int test_item = 0;

	switch (item_input) {
	case CMCP_FULL:
		test_item = CMCP_FULL_CASE;
		break;
	case CMCP_CM_PANEL:
		test_item = CM_PANEL;
		break;
	case CMCP_CP_PANEL:
		test_item = CP_PANEL;
		break;
	case CMCP_CM_BTN:
		test_item = CM_BTN;
		break;
	case CMCP_CP_BTN:
		test_item = CP_BTN;
		break;
	}
	return test_item;
}

/*******************************************************************************
 * FUNCTION: pt_cmcp_test_show
 *
 * SUMMARY: Show method for cmcp_test sysfs node. Allows to perform cmcp test
 *  with following steps:
 *   1) Get cmcp test items which from threhold file
 *   2) Check whether cmcp test items match with firmware
 *   3) Set parameter to force single TX
 *   4) Do calibration if requested
 *   5) Get all cmcp data from FW and do calculation
 *   6) Set parameter to restore to multi tx
 *   7) Do calibration if requested
 *   8) Check scan state,try to fix if it is not right
 *   9) Start watchdog
 *  10) Validate cm and cp test results if requested
 *  11) Fill the test result
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes
 *	*buf  - pointer to output buffer
 ******************************************************************************/
static ssize_t pt_cmcp_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_device_access_data *dad
		= pt_get_device_access_data(dev);
	struct cmcp_data *cmcp_info = dad->cmcp_info;
	struct result *result = dad->result;
	struct configuration *configuration = dad->configs;
	bool final_pass = true;
	static const char * const cmcp_test_case_array[] = {"Full Cm/Cp test",
		"Cm panel test", "Cp panel test",
		"Cm button test", "Cp button test"};
	int index = 0;
	int test_item = 0;
	int no_builtin_file = 0;
	int rc = 0;
	int self_test_result_1 = 0;
	int self_test_result_2 = 0;
	u8 sys_mode = FW_SYS_MODE_UNDEFINED;
	u8 retry = 3;

	dev = dad->dev;
	if ((configuration == NULL) || (cmcp_info == NULL))
		goto exit;

	mutex_lock(&dad->sysfs_lock);

	if (dad->cmcp_test_in_progress) {
		mutex_unlock(&dad->sysfs_lock);
		goto cmcp_not_ready;
	}
	dad->cmcp_test_in_progress = 1;

	dad->test_executed = 0;
	test_item = pt_cmcp_get_test_item(dad->cmcp_test_items);

	if (dad->builtin_cmcp_threshold_status < 0) {
		pt_debug(dev, DL_WARN, "%s: No cmcp threshold file.\n",
			__func__);
		no_builtin_file = 1;
		mutex_unlock(&dad->sysfs_lock);
		goto start_testing;
	}

	if (dad->cmcp_test_items < 0) {
		pt_debug(dev, DL_ERROR,
			"%s: Invalid test item! Should be 0~4!\n", __func__);
		mutex_unlock(&dad->sysfs_lock);
		goto invalid_item;
	}

	pt_debug(dev, DL_INFO, "%s: Test item is %s, %d\n",
		__func__, cmcp_test_case_array[dad->cmcp_test_items],
		test_item);

	if ((dad->si->num_btns == 0)
		&& ((dad->cmcp_test_items == CMCP_CM_BTN)
			|| (dad->cmcp_test_items == CMCP_CP_BTN))) {
		pt_debug(dev, DL_WARN,
			"%s: FW doesn't support button!\n", __func__);
		mutex_unlock(&dad->sysfs_lock);
		goto invalid_item_btn;
	}

	mutex_unlock(&dad->sysfs_lock);

	if (cmcp_check_config_fw_match(dev, configuration))
		goto mismatch;

start_testing:
	pt_debug(dev, DL_INFO, "%s: Start Cm/Cp test!\n", __func__);
	result->cm_test_pass = true;
	result->cp_test_pass = true;

	/*stop watchdog*/
	rc = cmd->request_stop_wd(dev);
	if (rc)
		pt_debug(dev, DL_ERROR, "stop watchdog failed");

	/* Make sure the device is awake */
	pm_runtime_get_sync(dev);
	/* Resource protect */
	rc = cmd->request_exclusive(dev, PT_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Error on request exclusive rc = %d\n",
			__func__, rc);
	}

	/*force single tx*/
	rc = cmd->nonhid_cmd->set_param(dev,
			PT_CORE_CMD_UNPROTECTED, 0x1F, 1, 1);
	if (rc)
		pt_debug(dev, DL_ERROR, "force single tx failed");

	/*suspend_scanning */
	rc = cmd->nonhid_cmd->suspend_scanning(dev, PT_CORE_CMD_UNPROTECTED);
	if (rc)
		pt_debug(dev, DL_ERROR, "suspend_scanning failed");

	/* Do calibration if requested */
	if (!dad->cmcp_force_calibrate) {
		pt_debug(dev, DL_INFO, "do calibration in single tx mode");
		rc = pt_perform_calibration(dev);
		if (rc)
			pt_debug(dev, DL_ERROR, "calibration failed");
	}
	/*resume_scanning */
	rc = cmd->nonhid_cmd->resume_scanning(dev, PT_CORE_CMD_UNPROTECTED);
	if (rc)
		pt_debug(dev, DL_ERROR, "resume_scanning failed");

	/*get all cmcp data from FW*/
	self_test_result_1 = pt_get_cmcp_info(dad, cmcp_info);
	if (self_test_result_1)
		pt_debug(dev, DL_ERROR, "pt_get_cmcp_info failed");

	/*restore to multi tx*/
	rc = cmd->nonhid_cmd->set_param(dev,
			PT_CORE_CMD_UNPROTECTED, 0x1F, 0, 1);
	if (rc)
		pt_debug(dev, DL_ERROR, "restore multi tx failed");

	/*suspend_scanning */
	rc = cmd->nonhid_cmd->suspend_scanning(dev, 0);
	if (rc)
		pt_debug(dev, DL_ERROR, "suspend_scanning failed");

	/* Do calibration if requested */
	if (!dad->cmcp_force_calibrate) {
		pt_debug(dev, DL_INFO, "do calibration in multi tx mode");
		rc = pt_perform_calibration(dev);
		if (rc)
			pt_debug(dev, DL_ERROR, "calibration failed");
	}
	/*resume_scanning */
	rc = cmd->nonhid_cmd->resume_scanning(dev, PT_CORE_CMD_UNPROTECTED);
	if (rc)
		pt_debug(dev, DL_ERROR, "resume_scanning failed");

	/*get cm cal data from FW*/
	self_test_result_2 = pt_get_cm_cal(dad, cmcp_info);
	if (self_test_result_2)
		pt_debug(dev, DL_ERROR, "pt_get_cm_cal failed");

	/* check scan state,try to fix if it is not right*/
	while (retry--) {
		rc = cmd->request_get_fw_mode(dev, PT_CORE_CMD_UNPROTECTED,
			&sys_mode, NULL);

		if (sys_mode != FW_SYS_MODE_SCANNING) {
			pt_debug(dev, DL_ERROR,
				"%s: fw mode: %d, retry: %d, rc = %d\n",
				__func__, sys_mode, retry, rc);
			rc = cmd->nonhid_cmd->resume_scanning(dev,
						PT_CORE_CMD_UNPROTECTED);
		}
	}

	rc = cmd->release_exclusive(dev);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Error on release exclusive rc = %d\n",
			__func__, rc);
	}
	pm_runtime_put(dev);

	/*start  watchdog*/
	rc = cmd->request_start_wd(dev);
	if (rc)
		pt_debug(dev, DL_ERROR, "start watchdog failed");

	if (self_test_result_1 || self_test_result_2)
		goto self_test_failed;

	/* The tests are finished without failure */
	mutex_lock(&dad->sysfs_lock);
	dad->test_executed = 1;
	mutex_unlock(&dad->sysfs_lock);

	if (no_builtin_file)
		goto no_builtin;

	if ((test_item) & (CM_ENABLED))
		validate_cm_test_results(dev, configuration, cmcp_info,
		result, &final_pass, test_item);

	if ((test_item) & (CP_ENABLED))
		validate_cp_test_results(dev, configuration, cmcp_info,
		result, &final_pass, test_item);

	if ((dad->cmcp_test_items == CMCP_FULL)
	&& (dad->cmcp_range_check == 0)) {
		/*full test and full check*/
		result->test_summary = result->cm_test_pass
			&& result->cp_test_pass
			&& result->short_test_pass;
	} else if ((dad->cmcp_test_items == CMCP_FULL)
		&& (dad->cmcp_range_check == 1)) {
		/*full test and basic check*/
		result->test_summary = result->cm_sensor_gd_col_pass
			&& result->cm_sensor_gd_row_pass
			&& result->cm_sensor_validation_pass
			&& result->cp_rx_validation_pass
			&& result->cp_tx_validation_pass
			&& result->short_test_pass;
	} else if (dad->cmcp_test_items == CMCP_CM_PANEL) {
		/*cm panel test result only*/
		result->test_summary = result->cm_sensor_gd_col_pass
			&& result->cm_sensor_gd_row_pass
			&& result->cm_sensor_validation_pass
			&& result->cm_sensor_row_delta_pass
			&& result->cm_sensor_col_delta_pass
			&& result->cm_sensor_calibration_pass
			&& result->cm_sensor_delta_pass;
	} else if (dad->cmcp_test_items == CMCP_CP_PANEL) {
		/*cp panel test result only*/
		result->test_summary = result->cp_sensor_delta_pass
			&& result->cp_rx_validation_pass
			&& result->cp_tx_validation_pass;
	} else if (dad->cmcp_test_items == CMCP_CM_BTN) {
		/*cm button test result only*/
		result->test_summary = result->cm_button_validation_pass
			&& result->cm_button_delta_pass;
	} else if (dad->cmcp_test_items == CMCP_CP_BTN) {
		/*cp button test result only*/
		result->test_summary = result->cp_button_delta_pass
			&& result->cp_button_average_pass
			&& result->cp_button_validation_pass;
	}

	if (result->test_summary) {
		pt_debug(dev, DL_INFO,
			"%s: Finish Cm/Cp test! All Test Passed\n", __func__);
		index = snprintf(buf, PT_MAX_PRBUF_SIZE, "Status: 1\n");
	} else {
		pt_debug(dev, DL_INFO,
			"%s: Finish Cm/Cp test! Range Check Failure\n",
			__func__);
		index = snprintf(buf, PT_MAX_PRBUF_SIZE, "Status: 6\n");
	}
	goto cmcp_ready;

mismatch:
	index = snprintf(buf, PT_MAX_PRBUF_SIZE,
		 "Status: 2\nInput cmcp threshold file mismatches with FW\n");
	goto cmcp_ready;
invalid_item_btn:
	index = snprintf(buf, PT_MAX_PRBUF_SIZE,
		"Status: 3\nFW doesn't support button!\n");
	goto cmcp_ready;
invalid_item:
	index = snprintf(buf, PT_MAX_PRBUF_SIZE,
		"Status: 4\nWrong test item or range check input!\nOnly support below items:\n0 - Cm/Cp Panel & Button with Gradient (Typical)\n1 - Cm Panel with Gradient\n2 - Cp Panel\n3 - Cm Button\n4 - Cp Button\nOnly support below range check:\n0 - Full Range Checking (default)\n1 - Basic Range Checking(TSG5 style)\n");
	goto cmcp_ready;
self_test_failed:
	index = snprintf(buf, PT_MAX_PRBUF_SIZE,
		"Status: 5\nget self test ID not supported!\n");
	goto cmcp_ready;
cmcp_not_ready:
	index = snprintf(buf, PT_MAX_PRBUF_SIZE, "Status: 0\n");
	goto cmcp_ready;
no_builtin:
	index = snprintf(buf, PT_MAX_PRBUF_SIZE,
		"Status: 7\nNo cmcp threshold file!\n");
cmcp_ready:
	mutex_lock(&dad->sysfs_lock);
	dad->cmcp_test_in_progress = 0;
	mutex_unlock(&dad->sysfs_lock);
exit:
	return index;
}

/*******************************************************************************
 * FUNCTION: pt_cmcp_test_store
 *
 * SUMMARY: The store method for cmcp_test sysfs node.Allows the user to
 *  configure which cm/cp tests will be executed on the "cat" of this node.
 *
 * RETURN: Size of passed in buffer
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 *  *attr - pointer to device attributes
 *  *buf  - pointer to buffer that hold the command parameters
 *   size - size of buf
 ******************************************************************************/
static ssize_t pt_cmcp_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_device_access_data *dad
		= pt_get_device_access_data(dev);
	u8 test_item = 0;
	u8 range_check = 0;
	u8 force_calibrate = 0;
	u32 input_data[4];
	int ret = 0;
	static const char * const cmcp_test_case_array[] = {"Full Cm/Cp test",
		"Cm panel test", "Cp panel test",
		"Cm button test", "Cp button test"};
	static const char * const cmcp_test_range_check_array[] = {
		"Full (default)", "Basic"};
	static const char * const cmcp_test_force_cal_array[] =	{
		"Calibrate When Testing (default)", "No Calibration"};
	ssize_t length = 0;

	pm_runtime_get_sync(dev);
	mutex_lock(&dad->sysfs_lock);

	length = cmd->parse_sysfs_input(dev, buf, size, input_data,
			ARRAY_SIZE(input_data));
	if (length <= 0 || length > 3) {
		pt_debug(dev, DL_ERROR, "%s: Input format error!\n",
					__func__);
		dad->cmcp_test_items = -EINVAL;
		ret = -EINVAL;
		goto error;
	}

	/* Get test item  */
	test_item = input_data[0];
	/* Get range check */
	if (length >= 2)
		range_check = input_data[1];
	/* Get force calibration */
	if (length == 3)
		force_calibrate = input_data[2];

	/*
	 * Test item limitation:
	 *	 0: Perform all Tests
	 *	 1: CM Panel with Gradient
	 *	 2: CP Panel
	 *	 3: CM Button
	 *	 4: CP Button
	 * Ranage check limitation:
	 *	 0: full check
	 *	 1: basic check
	 * Force calibrate limitation:
	 *	 0: do calibration
	 *	 1: don't do calibration
	 */
	if ((test_item < 0) || (test_item > 4) || (range_check > 1)
		|| (force_calibrate > 1)) {
		pt_debug(dev, DL_ERROR,
		"%s: Test item should be 0~4; Range check should be 0~1; Force calibrate should be 0~1\n",
		__func__);
		dad->cmcp_test_items = -EINVAL;
		ret = -EINVAL;
		goto error;
	}
	/*
	 * If it is not all Test, then range_check should be 0
	 * because other test does not has concept of basic check
	 */
	if (test_item > 0 && test_item < 5)
		range_check = 0;

	dad->cmcp_test_items = test_item;
	dad->cmcp_range_check = range_check;
	dad->cmcp_force_calibrate = force_calibrate;
	pt_debug(dev, DL_INFO,
		"%s: Test item=%s; Range check=%s; Force cal=%s.\n",
		__func__,
		cmcp_test_case_array[test_item],
		cmcp_test_range_check_array[range_check],
		cmcp_test_force_cal_array[force_calibrate]);

error:
	mutex_unlock(&dad->sysfs_lock);
	pm_runtime_put(dev);

	if (ret)
		return ret;

	return size;
}

static DEVICE_ATTR(cmcp_test, 0600,
	pt_cmcp_test_show, pt_cmcp_test_store);

/*******************************************************************************
 * FUNCTION: prepare_print_string
 *
 * SUMMARY: Formats input buffer to out buffer with string type,and increases
 *  the index by size of formated data.
 *
 * RETURN:
 *	 index plus with size of formated data
 *
 * PARAMETERS:
 *  *out_buf  - output buffer to store formated data
 *  *in_buf   - input buffer to be formated
 *   index    - index in output buffer for appending content
 ******************************************************************************/
int prepare_print_string(char *out_buf, char *in_buf, int index)
{
	if ((out_buf == NULL) || (in_buf == NULL))
		return index;
	index += scnprintf(&out_buf[index], MAX_BUF_LEN - index,
			"%s", in_buf);
	return index;
}

/*******************************************************************************
 * FUNCTION: prepare_print_string
 *
 * SUMMARY: Formats input buffer to out buffer with decimal base,and increases
 *  the index by size of formated data.
 *
 * RETURN:
 *	 index plus with size of formated data
 *
 * PARAMETERS:
 *  *out_buf  - output buffer to store formated data
 *  *in_buf   - input buffer to be formated
 *   index    - index in output buffer for appending content
 *   data_num - data number in input buffer
 ******************************************************************************/
int prepare_print_data(char *out_buf, int32_t *in_buf, int index, int data_num)
{
	int i;

	if ((out_buf == NULL) || (in_buf == NULL))
		return index;
	for (i = 0; i < data_num; i++)
		index += scnprintf(&out_buf[index], MAX_BUF_LEN - index,
				"%d,", in_buf[i]);
	return index;
}

/*******************************************************************************
 * FUNCTION: save_header
 *
 * SUMMARY: Appends "header" for cmcp test result to output buffer.
 *
 * RETURN:
 *	 index plus with size of formated data
 *
 * PARAMETERS:
 *  *out_buf  - output buffer to store formated data
 *   index    - index in output buffer for appending content
 *  *result   - pointer to result structure
 ******************************************************************************/
int save_header(char *out_buf, int index, struct result *result)
{
	struct rtc_time tm;
	char time_buf[100] = {0};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
	struct timespec ts;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
#else
	struct timex txc;

	do_gettimeofday(&(txc.time));
	rtc_time_to_tm(txc.time.tv_sec, &tm);
#endif

	scnprintf(time_buf, 100, "%d/%d/%d,TIME,%d:%d:%d,", tm.tm_year+1900,
		 tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	index = prepare_print_string(out_buf, ",.header,\n", index);
	index = prepare_print_string(out_buf, ",DATE,", index);
	index = prepare_print_string(out_buf, &time_buf[0], index);
	index = prepare_print_string(out_buf, ",\n", index);
	index = prepare_print_string(out_buf, ",SW_VERSION,", index);
	index = prepare_print_string(out_buf, PT_DRIVER_VERSION, index);
	index = prepare_print_string(out_buf, ",\n", index);
	index = prepare_print_string(out_buf, ",.end,\n", index);
	index = prepare_print_string(out_buf, ",.engineering data,\n", index);

	return index;
}

/*******************************************************************************
 * FUNCTION: print_silicon_id
 *
 * SUMMARY: Formats input buffer(silicon id) to out buffer with
 *  string type,and increases the index by size of formated data.
 *
 * RETURN:
 *	 index plus with size of formated data
 *
 * PARAMETERS:
 *  *out_buf  - output buffer to store formated data
 *  *in_buf   - input buffer to be formated
 *   index    - index in output buffer for appending content
 ******************************************************************************/
static int print_silicon_id(char *out_buf, char *in_buf, int index)
{
	index = prepare_print_string(out_buf, ",1,", index);
	index = prepare_print_string(out_buf, &in_buf[0], index);
	return index;
}

/*******************************************************************************
 * FUNCTION: save_engineering_data
 *
 * SUMMARY: Generates cmcp test result with *.csv format to output buffer, but
 *  it doesn't include the header.
 *
 * RETURN:
 *	 index plus with size of formated data
 *
 * PARAMETERS:
 *  *dev             - pointer to device structure
 *  *out_buf         - output buffer to store formated data
 *   index           - index in output buffer for appending content
 *  *cmcp_info       - pointer to cmcp_data structure
 *  *configuration   - pointer to configuration structure
 *  *result          - pointer to result structure
 *   test_item       - test control in bitwise
 *   no_builtin_file - flag to determin if builtin-file exist
 ******************************************************************************/
int save_engineering_data(struct device *dev, char *out_buf, int index,
	struct cmcp_data *cmcp_info, struct configuration *configuration,
	struct result *result, int test_item, int no_builtin_file)
{
	int i;
	int j;
	int tx_num = cmcp_info->tx_num;
	int rx_num = cmcp_info->rx_num;
	int btn_num = cmcp_info->btn_num;
	int tmp = 0;
	uint32_t fw_revision_control;
	uint32_t fw_config_ver;
	char device_id[20] = {0};
	struct pt_device_access_data *dad
		= pt_get_device_access_data(dev);

	fw_revision_control = dad->si->ttdata.revctrl;
	fw_config_ver = dad->si->ttdata.fw_ver_conf;
	/*calculate silicon id*/
	result->device_id_low = 0;
	result->device_id_high = 0;

	for (i = 0; i < 4; i++)
		result->device_id_low =
		(result->device_id_low << 8) + dad->si->ttdata.mfg_id[i];

	for (i = 4; i < 8; i++)
		result->device_id_high =
		(result->device_id_high << 8) + dad->si->ttdata.mfg_id[i];

	scnprintf(device_id, 20, "%x%x",
		result->device_id_high, result->device_id_low);

	/*print test summary*/
	index = print_silicon_id(out_buf, &device_id[0], index);
	if (result->test_summary)
		index = prepare_print_string(out_buf, ",PASS,\n", index);
	else
		index = prepare_print_string(out_buf, ",FAIL,\n", index);

	/*revision ctrl number*/
	index = print_silicon_id(out_buf, &device_id[0], index);
	index = prepare_print_string(out_buf, ",FW revision Control,", index);
	index = prepare_print_data(out_buf, &fw_revision_control, index, 1);
	index = prepare_print_string(out_buf, "\n", index);

	/*config version*/
	index = print_silicon_id(out_buf, &device_id[0], index);
	index = prepare_print_string(out_buf, ",CONFIG_VER,", index);
	index = prepare_print_data(out_buf, &fw_config_ver, index, 1);
	index = prepare_print_string(out_buf, "\n", index);

	/* Shorts test */
	index = print_silicon_id(out_buf, &device_id[0], index);
	if (result->short_test_pass)
		index = prepare_print_string(out_buf, ",Shorts,PASS,\n", index);
	else
		index = prepare_print_string(out_buf, ",Shorts,FAIL,\n", index);

	if ((test_item & CM_ENABLED) == CM_ENABLED) {
		/*print BUTNS_CM_DATA_ROW00*/
		if (((test_item & CM_BTN) == CM_BTN) && (btn_num > 0)) {
			index = print_silicon_id(out_buf, &device_id[0], index);
			index = prepare_print_string(out_buf,
				",Sensor Cm Validation,BUTNS_CM_DATA_ROW00,",
				index);
			index = prepare_print_data(out_buf,
				&cmcp_info->cm_btn_data[0],
				index,
				btn_num);
			index = prepare_print_string(out_buf, "\n", index);
		}

		if ((test_item & CM_PANEL) == CM_PANEL) {
			/*print CM_DATA_ROW*/
			for (i = 0; i < rx_num; i++) {
				index = print_silicon_id(out_buf, &device_id[0],
							index);
				index = prepare_print_string(out_buf,
							",Sensor Cm Validation,CM_DATA_ROW",
							index);
				index = prepare_print_data(out_buf, &i,
							index, 1);
				for (j = 0; j < tx_num; j++)
					index = prepare_print_data(out_buf,
					&cmcp_info->cm_data_panel[j*rx_num+i],
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);
			}

			if (!no_builtin_file) {
				/*print CM_MAX_GRADIENT_COLS_PERCENT*/
				index = print_silicon_id(out_buf,
							&device_id[0], index);
				index = prepare_print_string(out_buf,
					 ",Sensor Cm Validation,CM_MAX_GRADIENT_COLS_PERCENT,",
					index);
				for (i = 0; i < tx_num; i++) {
					char tmp_buf[10] = {0};

					scnprintf(tmp_buf, 10, "%d.%d,",
				cmcp_info->gd_sensor_col[i].gradient_val / 10,
				cmcp_info->gd_sensor_col[i].gradient_val % 10);
					index = prepare_print_string(out_buf,
						&tmp_buf[0], index);
				}
				index = prepare_print_string(out_buf,
								"\n", index);

				/*print CM_MAX_GRADIENT_ROWS_PERCENT*/
				index = print_silicon_id(out_buf,
							&device_id[0], index);
				index = prepare_print_string(out_buf,
			",Sensor Cm Validation,CM_MAX_GRADIENT_ROWS_PERCENT,",
					index);
				for (i = 0; i < rx_num; i++) {
					char tmp_buf[10] = {0};

					scnprintf(tmp_buf, 10, "%d.%d,",
				cmcp_info->gd_sensor_row[i].gradient_val / 10,
				cmcp_info->gd_sensor_row[i].gradient_val % 10);
					index = prepare_print_string(out_buf,
						&tmp_buf[0], index);
				}
				index = prepare_print_string(out_buf,
								"\n", index);

				if (!dad->cmcp_range_check) {
					/*print CM_DELTA_COLUMN*/
					for (i = 0; i < rx_num; i++) {
						index = print_silicon_id(
							out_buf,
							&device_id[0], index);
						index = prepare_print_string(
							out_buf,
							",Sensor Cm Validation,DELTA_COLUMNS_ROW",
							index);
						index = prepare_print_data(
							out_buf,
							&i, index, 1);
						index = prepare_print_data(
							out_buf,
							&tmp, index, 1);
					for (j = 1; j < tx_num; j++)
						index = prepare_print_data(
						out_buf,
			&cmcp_info->cm_sensor_column_delta[(j-1)*rx_num+i],
						index, 1);
						index = prepare_print_string(
								out_buf,
								"\n", index);
					}

					/*print CM_DELTA_ROW*/
					index = print_silicon_id(out_buf,
								&device_id[0],
								index);
					index = prepare_print_string(out_buf,
						 ",Sensor Cm Validation,DELTA_ROWS_ROW",
								index);
					index = prepare_print_data(out_buf,
								&tmp, index, 1);
					for (j = 0; j < tx_num; j++)
						index = prepare_print_data(
								out_buf,
								&tmp, index, 1);
					index = prepare_print_string(out_buf,
								"\n", index);

					for (i = 1; i < rx_num; i++) {
						index = print_silicon_id(
								out_buf,
								&device_id[0],
								index);
						index = prepare_print_string(
								out_buf,
						",Sensor Cm Validation,DELTA_ROWS_ROW",
								index);
						index = prepare_print_data(
								out_buf, &i,
								index, 1);
					for (j = 0; j < tx_num; j++)
						index = prepare_print_data(
							out_buf,
				&cmcp_info->cm_sensor_row_delta[j*rx_num+i-1],
							index, 1);
						index = prepare_print_string(
							out_buf,
							"\n", index);
					}

				/*print pass/fail Sensor Cm Validation*/
				index = print_silicon_id(out_buf, &device_id[0],
							index);
				if (result->cm_test_pass)
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation,PASS,\n",
						index);
				else
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation,FAIL,\n",
						index);
			}
			}
		}

		if (!no_builtin_file) {
			if (((test_item & CM_BTN) == CM_BTN) && (btn_num > 0)
				&& (!dad->cmcp_range_check)) {
				char tmp_buf[10] = {0};
				/*print Button Element by Element */
				index = print_silicon_id(out_buf, &device_id[0],
					index);
				if (result->cm_button_validation_pass)
					index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Button Element by Element,PASS\n",
					index);
				else
					index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Button Element by Element,FAIL\n",
					index);

				/*
				 *print  Sensor Cm Validation
				 *- Buttons Range Buttons Range
				 */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
					 ",Sensor Cm Validation - Buttons Range,Buttons Range,",
					 index);
				scnprintf(tmp_buf, 10, "%d.%d,",
					 cmcp_info->cm_delta_data_btn / 10,
					 cmcp_info->cm_delta_data_btn % 10);
				index = prepare_print_string(out_buf,
					&tmp_buf[0], index);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print  Sensor Cm Validation
				 *-Buttons Range Cm_button_avg
				 */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
					 ",Sensor Cm Validation - Buttons Range,Cm_button_avg,",
					 index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cm_ave_data_btn,
					 index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print  Sensor Cm Validation
				 * -Buttons Range Cm_button_avg
				 */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Buttons Range,Cm_button_cal,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cm_cal_data_btn,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print  Sensor Cm Validation
				 *-Buttons Range pass/fail
				 */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				if (result->cm_button_delta_pass)
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Buttons Range,PASS,LIMITS,",
						index);
				else
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Buttons Range,FAIL,LIMITS,",
						index);
				index = prepare_print_data(out_buf,
				&configuration->cm_max_delta_button_percent,
				index, 1);
				index = prepare_print_string(out_buf,
				"\n", index);
			}

			if ((test_item & CM_PANEL) == CM_PANEL &&
				!dad->cmcp_range_check) {
				char tmp_buf[10] = {0};
				/*print Cm_sensor_cal */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Calibration,Cm_sensor_cal,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cm_cal_data_panel,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Cm_sensor_cal limit*/
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				if (result->cm_sensor_calibration_pass)
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Calibration,PASS,LIMITS,",
						index);
				else
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Calibration,FAIL,LIMITS,",
						index);
				index = prepare_print_data(out_buf,
					&configuration->cm_min_limit_cal,
					index, 1);
				index = prepare_print_data(out_buf,
					&configuration->cm_max_limit_cal,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Columns Delta Matrix*/
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				if (result->cm_sensor_col_delta_pass)
					index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Columns Delta Matrix,PASS,LIMITS,",
					index);
				else
					index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Columns Delta Matrix,FAIL,LIMITS,",
					index);
				index = prepare_print_data(out_buf,
					&configuration->cm_range_limit_col,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Cm Validation - Element by Element*/
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				if (result->cm_sensor_validation_pass)
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Element by Element,PASS,",
						index);
				else
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Element by Element,FAIL,",
						index);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Cm Validation -Gradient Cols*/
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				if (result->cm_sensor_gd_col_pass)
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Gradient Cols,PASS,",
						index);
				else
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Gradient Cols,FAIL,",
						index);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Cm Validation -Gradient Rows*/
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				if (result->cm_sensor_gd_row_pass)
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Gradient Rows,PASS,",
						index);
				else
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Gradient Rows,FAIL,",
						index);
				index = prepare_print_string(out_buf,
					"\n", index);


				/*
				 * Print Sensor Cm Validation
				 * -Rows Delta Matrix
				 */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				if (result->cm_sensor_row_delta_pass)
					index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Rows Delta Matrix,PASS,LIMITS,",
					index);
				else
					index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Rows Delta Matrix,FAIL,LIMITS,",
					index);
				index = prepare_print_data(out_buf,
					&configuration->cm_range_limit_row,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Cm_sensor_avg */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Sensor Range,Cm_sensor_avg,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cm_ave_data_panel,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*printSensor Cm Validation -
				 * Sensor Range,   Sensor Range
				 */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
					",Sensor Cm Validation - Sensor Range,Sensor Range,",
					index);
				scnprintf(tmp_buf, 10, "%d.%d,",
					cmcp_info->cm_sensor_delta / 10,
					cmcp_info->cm_sensor_delta % 10);
				index = prepare_print_string(out_buf,
					&tmp_buf[0], index);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Sensor Cm Validation - Sensor Range*/
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				if (result->cm_sensor_delta_pass)
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Sensor Range,PASS,LIMITS,",
						index);
				else
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation - Sensor Range,FAIL,LIMITS,",
						index);
				index = prepare_print_data(out_buf,
				&configuration->cm_max_delta_sensor_percent,
						index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);
			}
		}
	}

	if ((test_item & CP_ENABLED) == CP_ENABLED) {
		if (((test_item & CP_BTN) == CP_BTN) && (btn_num > 0)) {
			/*print   BUTNS_CP_DATA_ROW00  */
			index = print_silicon_id(out_buf, &device_id[0], index);
			index = prepare_print_string(out_buf,
			",Self-cap Calibration Check,BUTNS_CP_DATA_ROW00,",
				index);
			index = prepare_print_data(out_buf,
				&cmcp_info->cp_btn_data[0],
				index, btn_num);
			index = prepare_print_string(out_buf,
				"\n", index);

			if (!no_builtin_file && !dad->cmcp_range_check) {
				/*print Cp Button Element by Element */
				index = print_silicon_id(out_buf, &device_id[0],
					index);
				if (result->cp_button_validation_pass)
					index = prepare_print_string(out_buf,
					",Self-cap Calibration Check - Button Element by Element,PASS\n",
					index);
				else
					index = prepare_print_string(out_buf,
					",Self-cap Calibration Check - Button Element by Element,FAIL\n",
					index);

				/*print   cp_button_ave  */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_button_avg,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cp_button_ave,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print   Cp_button_cal  */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_button_cal,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cp_btn_cal,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);
			}
		}

		if ((test_item & CP_PANEL) == CP_PANEL) {
			/*print CP_DATA_RX */
			index = print_silicon_id(out_buf, &device_id[0], index);
			index = prepare_print_string(out_buf,
			",Self-cap Calibration Check,CP_DATA_RX,", index);
			index = prepare_print_data(out_buf,
				&cmcp_info->cp_rx_data_panel[0], index, rx_num);
			index = prepare_print_string(out_buf, "\n", index);

			/*print CP_DATA_TX */
			index = print_silicon_id(out_buf, &device_id[0], index);
			index = prepare_print_string(out_buf,
			",Self-cap Calibration Check,CP_DATA_TX,", index);
			index = prepare_print_data(out_buf,
				&cmcp_info->cp_tx_data_panel[0], index, tx_num);
			index = prepare_print_string(out_buf, "\n", index);
		}
		if (((test_item & CP_BTN) == CP_BTN) && (btn_num > 0)
			&& !dad->cmcp_range_check) {
			if (!no_builtin_file) {
				char tmp_buf[10] = {0};
				/*print  Cp_delta_button */
				index = print_silicon_id(out_buf, &device_id[0],
					index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_delta_button,",
				index);
				scnprintf(tmp_buf, 10, "%d.%d,",
				cmcp_info->cp_button_delta / 10,
				cmcp_info->cp_button_delta % 10);
				index = prepare_print_string(out_buf,
						&tmp_buf[0], index);
				index = prepare_print_string(out_buf, "\n",
					index);
			}
		}
		if ((test_item & CP_PANEL) == CP_PANEL &&
			!dad->cmcp_range_check) {
			if (!no_builtin_file) {
				char tmp_buf[10] = {0};
				/*print Cp_delta_rx */
				index = print_silicon_id(out_buf, &device_id[0],
					index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_delta_rx,",
				index);
				scnprintf(tmp_buf, 10, "%d.%d,",
				cmcp_info->cp_sensor_rx_delta / 10,
				cmcp_info->cp_sensor_rx_delta % 10);
				index = prepare_print_string(out_buf,
						&tmp_buf[0], index);
				index = prepare_print_string(out_buf, "\n",
					index);

				/*print Cp_delta_tx */
				index = print_silicon_id(out_buf, &device_id[0],
					index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_delta_tx,",
					index);
				scnprintf(tmp_buf, 10, "%d.%d,",
				cmcp_info->cp_sensor_tx_delta / 10,
				cmcp_info->cp_sensor_tx_delta % 10);
				index = prepare_print_string(out_buf,
						&tmp_buf[0], index);
				index = prepare_print_string(out_buf, "\n",
					index);

				/*print Cp_sensor_avg_rx */
				index = print_silicon_id(out_buf, &device_id[0],
					index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_sensor_avg_rx,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cp_rx_ave_data_panel,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Cp_sensor_avg_tx */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_sensor_avg_tx,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cp_tx_ave_data_panel,
					index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Cp_sensor_cal_rx */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_sensor_cal_rx,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cp_rx_cal_data_panel[0],
					index, rx_num);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print Cp_sensor_cal_tx */
				index = print_silicon_id(out_buf,
					&device_id[0], index);
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,Cp_sensor_cal_tx,",
					index);
				index = prepare_print_data(out_buf,
					&cmcp_info->cp_tx_cal_data_panel[0],
					index, tx_num);
				index = prepare_print_string(out_buf,
					"\n", index);
			}
		}

		if (!no_builtin_file && !dad->cmcp_range_check) {
			/*print  cp test limits  */
			index = print_silicon_id(out_buf, &device_id[0], index);
			if (result->cp_test_pass)
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,PASS, LIMITS,",
				index);
			else
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,FAIL, LIMITS,",
				index);

			index = prepare_print_string(out_buf,
				"CP_MAX_DELTA_SENSOR_RX_PERCENT,", index);
			index = prepare_print_data(out_buf,
				&configuration->cp_max_delta_sensor_rx_percent,
				index, 1);
			index = prepare_print_string(out_buf,
				"CP_MAX_DELTA_SENSOR_TX_PERCENT,", index);
			index = prepare_print_data(out_buf,
				&configuration->cp_max_delta_sensor_tx_percent,
				index, 1);
			index = prepare_print_string(out_buf,
				"CP_MAX_DELTA_BUTTON_PERCENT,", index);
			index = prepare_print_data(out_buf,
				&configuration->cp_max_delta_button_percent,
				index, 1);
			index = prepare_print_string(out_buf, "\n", index);
		}
	}

	if (!no_builtin_file) {
		if ((test_item & CM_ENABLED) == CM_ENABLED) {
			if ((test_item & CM_PANEL) == CM_PANEL) {
				/*print columns gradient limit*/
				index = prepare_print_string(out_buf,
				",Sensor Cm Validation,MAX_LIMITS,CM_MAX_GRADIENT_COLS_PERCENT,",
				index);
				index = prepare_print_data(out_buf,
			&configuration->cm_max_table_gradient_cols_percent[0],
					index,
			configuration->cm_max_table_gradient_cols_percent_size);
				index = prepare_print_string(out_buf,
					"\n", index);
				/*print rows gradient limit*/
				index = prepare_print_string(out_buf,
				",Sensor Cm Validation,MAX_LIMITS,CM_MAX_GRADIENT_ROWS_PERCENT,",
				index);
				index = prepare_print_data(out_buf,
			&configuration->cm_max_table_gradient_rows_percent[0],
					index,
			configuration->cm_max_table_gradient_rows_percent_size);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print cm max limit*/
				for (i = 0; i < rx_num; i++) {
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation,MAX_LIMITS,CM_DATA_ROW",
						index);
					index = prepare_print_data(out_buf,
						&i, index, 1);
					for (j = 0; j < tx_num; j++)
						index = prepare_print_data(
							out_buf,
		&configuration->cm_min_max_table_sensor[i*tx_num*2+j*2+1],
							index, 1);
					index = prepare_print_string(out_buf,
						"\n", index);
				}
			}

			if (((test_item & CM_BTN) == CM_BTN) && (btn_num > 0)) {
				index = prepare_print_string(out_buf,
					",Sensor Cm Validation,MAX LIMITS,M_BUTNS,",
					index);
				for (j = 0; j < btn_num; j++) {
					index = prepare_print_data(out_buf,
				&configuration->cm_min_max_table_btn[2*j+1],
						index, 1);
				}
				index = prepare_print_string(out_buf,
					"\n", index);
			}

			index = prepare_print_string(out_buf,
				 ",Sensor Cm Validation MAX LIMITS\n", index);

			if ((test_item & CM_PANEL) == CM_PANEL) {
				/*print cm min limit*/
				for (i = 0; i < rx_num; i++) {
					index = prepare_print_string(out_buf,
						",Sensor Cm Validation,MIN_LIMITS,CM_DATA_ROW",
						index);
					index = prepare_print_data(out_buf, &i,
						index, 1);
					for (j = 0; j < tx_num; j++)
						index = prepare_print_data(
							out_buf,
		&configuration->cm_min_max_table_sensor[i*tx_num*2 + j*2],
							index, 1);
					index = prepare_print_string(out_buf,
						"\n", index);
				}
			}

			if (((test_item & CM_BTN) == CM_BTN) && (btn_num > 0)) {
				index = prepare_print_string(out_buf,
					",Sensor Cm Validation,MIN LIMITS,M_BUTNS,",
					index);
				for (j = 0; j < btn_num; j++) {
					index = prepare_print_data(out_buf,
				&configuration->cm_min_max_table_btn[2*j],
						 index, 1);
				}
				index = prepare_print_string(out_buf,
					"\n", index);
			}
			index = prepare_print_string(out_buf,
				",Sensor Cm Validation MIN LIMITS\n", index);
		}

		if ((test_item & CP_ENABLED) == CP_ENABLED) {
			if ((test_item & CP_PANEL) == CP_PANEL) {
				/*print cp tx max limit*/
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,MAX_LIMITS,TX,",
					index);
				for (i = 0; i < tx_num; i++)
					index = prepare_print_data(out_buf,
				&configuration->cp_min_max_table_tx[i*2+1],
						index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print cp rx max limit*/
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,MAX_LIMITS,RX,",
					index);
				for (i = 0; i < rx_num; i++)
					index = prepare_print_data(out_buf,
				&configuration->cp_min_max_table_rx[i*2+1],
						index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);
			}

			/*print cp btn max limit*/
			if (((test_item & CP_BTN) == CP_BTN) && (btn_num > 0)) {
				index = prepare_print_string(out_buf,
			",Self-cap Calibration Check,MAX_LIMITS,S_BUTNS,",
					index);
				for (i = 0; i < btn_num; i++)
					index = prepare_print_data(out_buf,
				&configuration->cp_min_max_table_btn[i*2+1],
						index, 1);
				index = prepare_print_string(out_buf,
						"\n", index);
			}

			if ((test_item & CP_PANEL) == CP_PANEL) {
				/*print cp tx min limit*/
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,MIN_LIMITS,TX,",
					index);
				for (i = 0; i < tx_num; i++)
					index = prepare_print_data(out_buf,
				&configuration->cp_min_max_table_tx[i*2],
						index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);

				/*print cp rx min limit*/
				index = prepare_print_string(out_buf,
				",Self-cap Calibration Check,MIN_LIMITS,RX,",
					index);
				for (i = 0; i < rx_num; i++)
					index = prepare_print_data(out_buf,
				&configuration->cp_min_max_table_rx[i*2],
						index, 1);
				index = prepare_print_string(out_buf,
					"\n", index);
			}

			/*print cp btn min limit*/
			if (((test_item & CP_BTN) == CP_BTN) && (btn_num > 0)) {
				index = prepare_print_string(out_buf,
			",Self-cap Calibration Check,MIN_LIMITS,S_BUTNS,",
					index);
				for (i = 0; i < btn_num; i++)
					index = prepare_print_data(out_buf,
				&configuration->cp_min_max_table_btn[i*2],
						index, 1);
				index = prepare_print_string(out_buf,
						"\n", index);
			}
		}
	}
	return index;
}

/*******************************************************************************
 * FUNCTION: result_save
 *
 * SUMMARY: Malloc memory for output buffer and populate with the cmcp test
 *  header and results in the csv file format.
 *
 * NOTE: It supports simple_read_from_buffer() to read data multiple times to
 *  the buffer.
 *
 * RETURN:
 *	 Size of data printed to "buf"
 *
 * PARAMETERS:
 *  *dev             - pointer to device structure
 *  *buf             - the user space buffer to read to
 *  *configuration   - pointer to configuration structure
 *  *result          - pointer to result structure
 *  *cmcp_info       - pointer to cmcp_data structure
 *  *ppos            - the current position in the buffer
 *   count           - the maximum number of bytes to read
 *   test_item       - test control in bitwise
 *   no_builtin_file - flag to determine if builtin-file exist
 ******************************************************************************/
int result_save(struct device *dev, char *buf,
	struct configuration *configuration, struct result *result,
	struct cmcp_data *cmcp_info, loff_t *ppos, size_t count, int test_item,
	int no_builtin_file)
{
	u8 *out_buf = NULL;
	int index = 0;
	int byte_left;

	out_buf = kzalloc(MAX_BUF_LEN, GFP_KERNEL);
	if (configuration == NULL)
		pt_debug(dev, DL_WARN, "config is NULL");
	if (result == NULL)
		pt_debug(dev, DL_WARN, "result is NULL");
	if (cmcp_info == NULL)
		pt_debug(dev, DL_WARN, "cmcp_info is NULL");

	index = save_header(out_buf, index, result);
	index = save_engineering_data(dev, out_buf, index,
		cmcp_info, configuration, result,
		test_item, no_builtin_file);
	byte_left = simple_read_from_buffer(buf, count, ppos, out_buf, index);

	kfree(out_buf);
	return byte_left;
}

/*******************************************************************************
 * FUNCTION: cmcp_results_debugfs_open
 *
 * SUMMARY: Open method for cmcp_results debugfs node.
 *
 * RETURN: 0 = success
 *
 * PARAMETERS:
 *      *inode - file inode number
 *      *filp  - file pointer to debugfs file
 ******************************************************************************/
static int cmcp_results_debugfs_open(struct inode *inode,
		struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

/*******************************************************************************
 * FUNCTION: cmcp_results_debugfs_close
 *
 * SUMMARY: Close method for cmcp_results debugfs node.
 *
 * RETURN: 0 = success
 *
 * PARAMETERS:
 *      *inode - file inode number
 *      *filp  - file pointer to debugfs file
 ******************************************************************************/
static int cmcp_results_debugfs_close(struct inode *inode,
		struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

/*******************************************************************************
 * FUNCTION: cmcp_results_debugfs_read
 *
 * SUMMARY: Read method for cmcp_results debugfs node. This function prints
 *	cmcp test results to user buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *      *filp   - file pointer to debugfs file
 *      *buf    - the user space buffer to read to
 *       count  - the maximum number of bytes to read
 *      *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t cmcp_results_debugfs_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_data *dad = filp->private_data;
	struct device *dev;
	struct cmcp_data *cmcp_info = dad->cmcp_info;
	struct result *result = dad->result;
	struct configuration *configuration = dad->configs;
	int ret = 0;
	int test_item;
	int no_builtin_file = 0;
	int test_executed = 0;

	dev = dad->dev;

	mutex_lock(&dad->sysfs_lock);
	test_executed = dad->test_executed;
	test_item = pt_cmcp_get_test_item(dad->cmcp_test_items);
	if (dad->builtin_cmcp_threshold_status < 0) {
		pt_debug(dev, DL_WARN,
				"%s: No cmcp threshold file.\n", __func__);
		no_builtin_file = 1;
	}
	mutex_unlock(&dad->sysfs_lock);

	if (test_executed)
		/*save result to buf*/
		ret = result_save(dev, buf, configuration, result, cmcp_info,
			ppos, count, test_item, no_builtin_file);
	else {
		char warning_info[] =
		"No test result available!\n";
		pt_debug(dev, DL_ERROR,
				"%s: No test result available!\n", __func__);

		return simple_read_from_buffer(buf, count, ppos, warning_info,
			strlen(warning_info));
	}

	return ret;
}

static const struct file_operations cmcp_results_debugfs_fops = {
	.open = cmcp_results_debugfs_open,
	.release = cmcp_results_debugfs_close,
	.read = cmcp_results_debugfs_read,
	.write = NULL,
};

/*******************************************************************************
 * FUNCTION: cmcp_return_offset_of_new_case
 *
 * SUMMARY: Returns the buffer offset of new test case
 *
 * NOTE: There are two static variable inside this function.
 *
 * RETURN: offset index for new case
 *
 * PARAMETERS:
 *   *bufPtr      - pointer to input buffer
 *    first_time  - flag to initialize some static variable
 *                  (0:init; 1:don't init)
 *   *pFileEnd    - pointer to the end of file for safe check
 ******************************************************************************/
u32 cmcp_return_offset_of_new_case(const char *bufPtr, u32 first_time,
		const char *pFileEnd)
{
	static u32 offset, first_search;

	if (first_time == 0) {
		first_search = 0;
		offset = 0;
	}

	if (first_search != 0) {
		/* Search one case */
		for (;;) {
			/* Search ASCII_LF */
			while (bufPtr < pFileEnd) {
				if (*bufPtr++ != ASCII_LF)
					offset++;
				else
					break;
			}
			if (bufPtr >= pFileEnd)
				break;
			offset++;
			/*
			 * Single line: end loop
			 * Multiple lines: continue loop
			 */
			if (*bufPtr != ASCII_COMMA)
				break;
		}
	} else
		first_search = 1;

	return offset;
}

/*******************************************************************************
 * FUNCTION: cmcp_get_case_info_from_threshold_file
 *
 * SUMMARY: Gets test case information from cmcp threshold file
 *
 * RETURN:
 *   Number of test cases
 *
 * PARAMETERS:
 *  *dev           - pointer to Device structure
 *  *buf           - pointer to input file
 *  *search_array  - pointer to test_case_search structure
 *   file_size     - size of input file for safe check
 ******************************************************************************/
u32 cmcp_get_case_info_from_threshold_file(struct device *dev, const char *buf,
		struct test_case_search *search_array, u32 file_size)
{
	u32 case_num = 0, buffer_offset = 0, name_count = 0, first_search = 0;
	const char *pFileEnd = buf + file_size;

	pt_debug(dev, DL_INFO, "%s: Search cmcp threshold file\n",
		__func__);

	/* Get all the test cases */
	for (case_num = 0; case_num < MAX_CASE_NUM; case_num++) {
		buffer_offset =
			cmcp_return_offset_of_new_case(&buf[buffer_offset],
			first_search, pFileEnd);
		first_search = 1;

		if (buf[buffer_offset] == 0)
			break;

		for (name_count = 0; name_count < NAME_SIZE_MAX; name_count++) {
			/* File end */
			if (buf[buffer_offset + name_count] == ASCII_COMMA)
				break;

			search_array[case_num].name[name_count] =
					buf[buffer_offset + name_count];
		}

		/* Exit when buffer offset is larger than file size */
		if (buffer_offset >= file_size)
			break;

		search_array[case_num].name_size = name_count;
		search_array[case_num].offset = buffer_offset;
		/*
		 *  pt_debug(dev, DL_INFO, "Find case %d: Name is %s;
		 *  Name size is %d; Case offset is %d\n",
		 *	case_num,
		 *	search_array[case_num].name,
		 *	search_array[case_num].name_size,
		 *	search_array[case_num].offset);
		 */
	}

	return case_num;
}

/*******************************************************************************
 * FUNCTION: cmcp_compose_data
 *
 * SUMMARY: Composes one value based on data of each bit
 *
 * RETURN:
 *   Value that composed from buffer
 *
 * PARAMETERS:
 *  *buf     - pointer to input file
 *   count   - number of data elements in *buf in decimal
 ******************************************************************************/
int cmcp_compose_data(char *buf, u32 count)
{
	u32 base_array[] = {1, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9};
	int value = 0;
	u32 index = 0;

	for (index = 0; index < count; index++)
		value += buf[index] * base_array[count - 1 - index];

	return value;
}

/*******************************************************************************
 * FUNCTION: cmcp_return_one_value
 *
 * SUMMARY: Parses csv file at a given row and offset and combines multiple
 *  "bits' as a single value. Handles data over multiple lines and various
 *  end-of-line characters.
 *
 * NOTE: There is a static value to calculate line count inside this function.
 *
 * RETURN:
 *   Value that parsed from buffer
 *
 * PARAMETERS:
 *  *dev       - pointer to devices structure
 *  *buf       - pointer to input buffer
 *  *offset    - offset index of input buffer
 *  *line_num  - store line count
 *   pFileEnd -  pointer to the end of threshold file
 ******************************************************************************/
int cmcp_return_one_value(struct device *dev, const char *buf, u32 *offset,
			  u32 *line_num, const char *pFileEnd)
{
	int value = -1;
	char tmp_buffer[10];
	u32 count = 0;
	u32 tmp_offset = *offset;
	static u32 line_count = 1;

	/* Bypass extra commas */
	while (((buf + tmp_offset + 1) < pFileEnd)
			&& buf[tmp_offset] == ASCII_COMMA
			&& buf[tmp_offset + 1] == ASCII_COMMA)
		tmp_offset++;

	if ((buf + tmp_offset + 1) >= pFileEnd)
		goto exit;

	/* Windows and Linux difference at the end of one line */
	if (buf[tmp_offset] == ASCII_COMMA && buf[tmp_offset + 1] == ASCII_CR) {
		if ((buf + tmp_offset + 2) < pFileEnd) {
			if (buf[tmp_offset + 2] == ASCII_LF)
				tmp_offset += 2;
		} else
			goto exit;
	} else if (buf[tmp_offset] == ASCII_COMMA &&
		 buf[tmp_offset + 1] == ASCII_LF)
		tmp_offset += 1;
	else if (buf[tmp_offset] == ASCII_COMMA
			&& buf[tmp_offset + 1] == ASCII_CR)
		tmp_offset += 1;

	if ((buf + tmp_offset + 1) >= pFileEnd)
		goto exit;

	/* New line for multiple lines */
	if ((buf[tmp_offset] == ASCII_LF || buf[tmp_offset] == ASCII_CR) &&
	     buf[tmp_offset + 1] == ASCII_COMMA) {
		tmp_offset++;
		line_count++;
		pt_debug(dev, DL_DEBUG, "%s: Line Count = %d\n",
			__func__, line_count);
	}

	/* Beginning */
	if (buf[tmp_offset] == ASCII_COMMA) {
		tmp_offset++;
		for (;;) {
			if ((buf + tmp_offset) >= pFileEnd)
				break;

			if ((buf[tmp_offset] >= ASCII_ZERO)
			&& (buf[tmp_offset] <= ASCII_NINE)) {
				tmp_buffer[count++] =
				buf[tmp_offset] - ASCII_ZERO;
				tmp_offset++;
			} else {
				if (count != 0) {
					value = cmcp_compose_data(tmp_buffer,
					count);
					/*pt_debug(dev, DL_DEBUG, */
					/* ",%d", value);*/
				} else {
					/* 0 indicates no data available */
					value = -1;
				}
				break;
			}
		}
	} else {
		/* Multiple line: line count */
		*line_num = line_count;
		/* Reset for next case */
		line_count = 1;
	}

exit:
	*offset = tmp_offset;

	return value;
}

/*******************************************************************************
 * FUNCTION: cmcp_get_configuration_info
 *
 * SUMMARY: Gets cmcp configuration information.
 *
 * PARAMETERS:
 *  *dev          - pointer to devices structure
 *  *buf          - pointer to input buffer
 *  *search_array - pointer to test_case_search structure
 *   case_count   - number of test cases
 *  *field_array  - pointer to test_case_field structure
 *  *config       - pointer to configuration structure
 *   file_size    - file size of threshold file
 ******************************************************************************/
void cmcp_get_configuration_info(struct device *dev,
		const char *buf, struct test_case_search *search_array,
		u32 case_count, struct test_case_field *field_array,
		struct configuration *config, u32 file_size)
{
	u32 count = 0, sub_count = 0;
	u32 exist_or_not = 0;
	u32 value_offset = 0;
	int retval = 0;
	u32 data_num = 0;
	u32 line_num = 1;
	const char *pFileEnd = buf + file_size;

	pt_debug(dev, DL_INFO,
		"%s: Fill configuration struct per cmcp threshold file\n",
		__func__);

	/* Search cases */
	for (count = 0; count < MAX_CASE_NUM; count++) {
		exist_or_not = 0;
		for (sub_count = 0; sub_count < case_count; sub_count++) {
			if (!strncmp(field_array[count].name,
					search_array[sub_count].name,
					field_array[count].name_size)) {
				exist_or_not = 1;
				break;
			}
		}

		field_array[count].exist_or_not = exist_or_not;

		pt_debug(dev, DL_DEBUG,
			"%s: Field Array[%d] exists: %d, type: %d\n",
			__func__, count, exist_or_not, field_array[count].type);

		/* Clear data number */
		data_num = 0;

		if (exist_or_not == 1) {
			switch (field_array[count].type) {
			case TEST_CASE_TYPE_NO:
				field_array[count].data_num = 0;
				field_array[count].line_num = 1;
				break;
			case TEST_CASE_TYPE_ONE:
				value_offset = search_array[sub_count].offset
					+ search_array[sub_count].name_size;
				*field_array[count].bufptr =
						cmcp_return_one_value(dev, buf,
						&value_offset, 0, pFileEnd);
				field_array[count].data_num = 1;
				field_array[count].line_num = 1;
				break;
			case TEST_CASE_TYPE_MUL:
			case TEST_CASE_TYPE_MUL_LINES:
				line_num = 1;
				value_offset = search_array[sub_count].offset
					+ search_array[sub_count].name_size;
				for (;;) {
					retval = cmcp_return_one_value(
					    dev, buf, &value_offset, &line_num,
					    pFileEnd);
					if (retval >= 0) {
						*field_array[count].bufptr++ =
						retval;
						data_num++;
					} else
						break;
				}

				field_array[count].data_num = data_num;
				field_array[count].line_num = line_num;
				break;
			default:
				break;
			}
			pt_debug(dev, DL_DEBUG,
				"%s: %s: Data count is %d, line number is %d\n",
				__func__,
				field_array[count].name,
				field_array[count].data_num,
				field_array[count].line_num);
		} else
			pt_debug(dev, DL_ERROR, "%s: !!! %s doesn't exist\n",
				__func__, field_array[count].name);
	}
}

/*******************************************************************************
 * FUNCTION: cmcp_get_basic_info
 *
 * SUMMARY: Gets basic information for cmcp test, such as available test item,
 *  number of tx, rx, button.
 *
 * PARAMETERS:
 *  *dev          - pointer to devices structure
 *  *field_array  - pointer to test_case_field structure
 *  *config       - pointer to configuration structure
 ******************************************************************************/
void cmcp_get_basic_info(struct device *dev,
	struct test_case_field *field_array, struct configuration *config)
{
	u32 tx_num = 0;
	u32 index = 0;

	config->is_valid_or_not = 1; /* Set to valid by default */
	config->cm_enabled = 0;
	config->cp_enabled = 0;

	if (field_array[CM_TEST_INPUTS].exist_or_not)
		config->cm_enabled = 1;
	if (field_array[CP_TEST_INPUTS].exist_or_not)
		config->cp_enabled = 1;

	/* Get basic information only when CM and CP are enabled */
	if (config->cm_enabled && config->cp_enabled) {
		pt_debug(dev, DL_INFO,
			"%s: Find CM and CP thresholds\n", __func__);

		config->rx_num =
			field_array[PER_ELEMENT_MIN_MAX_TABLE_SENSOR].line_num;
		tx_num =
		(field_array[PER_ELEMENT_MIN_MAX_TABLE_SENSOR].data_num >> 1)
			/field_array[PER_ELEMENT_MIN_MAX_TABLE_SENSOR].line_num;
		config->tx_num = tx_num;

		config->btn_num =
		field_array[PER_ELEMENT_MIN_MAX_TABLE_BUTTON].data_num >> 1;

		config->cm_min_max_table_btn_size =
			field_array[PER_ELEMENT_MIN_MAX_TABLE_BUTTON].data_num;
		config->cm_min_max_table_sensor_size =
			field_array[PER_ELEMENT_MIN_MAX_TABLE_SENSOR].data_num;
		config->cp_min_max_table_rx_size =
			field_array[PER_ELEMENT_MIN_MAX_RX].data_num;
		config->cp_min_max_table_tx_size =
			field_array[PER_ELEMENT_MIN_MAX_TX].data_num;
		config->cm_max_table_gradient_cols_percent_size =
			field_array[CM_GRADIENT_CHECK_COL].data_num;
		config->cm_max_table_gradient_rows_percent_size =
			field_array[CM_GRADIENT_CHECK_ROW].data_num;
		config->cp_min_max_table_btn_size =
			field_array[CP_PER_ELEMENT_MIN_MAX_BUTTON].data_num;

		/* *** Detailed Debug Information *** */
		pt_debug(dev, DL_DEBUG, "%d\n",
					config->cm_excluding_col_edge);
		pt_debug(dev, DL_DEBUG, "%d\n",
					config->cm_excluding_row_edge);
		for (index = 0;
		index < config->cm_max_table_gradient_cols_percent_size;
		index++)
			pt_debug(dev, DL_DEBUG, "%d\n",
			config->cm_max_table_gradient_cols_percent[index]);
		for (index = 0;
		index < config->cm_max_table_gradient_rows_percent_size;
		index++)
			pt_debug(dev, DL_DEBUG, "%d\n",
			config->cm_max_table_gradient_rows_percent[index]);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cm_range_limit_row);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cm_range_limit_col);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cm_min_limit_cal);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cm_max_limit_cal);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cm_max_delta_sensor_percent);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cm_max_delta_button_percent);
		for (index = 0;
		index < config->cm_min_max_table_btn_size; index++)
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cm_min_max_table_btn[index]);
		for (index = 0;
			index < config->cm_min_max_table_sensor_size; index++)
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cm_min_max_table_sensor[index]);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cp_max_delta_sensor_rx_percent);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cp_max_delta_sensor_tx_percent);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cp_max_delta_button_percent);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->min_button);
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->max_button);

		for (index = 0;
		index < config->cp_min_max_table_btn_size; index++)
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cp_min_max_table_btn[index]);
		for (index = 0;
		index < config->cp_min_max_table_rx_size; index++)
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cp_min_max_table_rx[index]);
		for (index = 0;
		index < config->cp_min_max_table_tx_size; index++)
			pt_debug(dev, DL_DEBUG, "%d\n",
				config->cp_min_max_table_tx[index]);
		/* *** End of Detailed Debug Information *** */

		/* Invalid mutual data length */
		if ((field_array[PER_ELEMENT_MIN_MAX_TABLE_SENSOR].data_num >>
		1) % field_array[PER_ELEMENT_MIN_MAX_TABLE_SENSOR].line_num) {
			config->is_valid_or_not = 0;
			pt_debug(dev, DL_ERROR, "Invalid mutual data length\n");
		}
	} else {
		if (!config->cm_enabled)
			pt_debug(dev, DL_ERROR,
				"%s: Miss CM thresholds or CM data format is wrong!\n",
				__func__);

		if (!config->cp_enabled)
			pt_debug(dev, DL_ERROR,
				"%s: Miss CP thresholds or CP data format is wrong!\n",
				__func__);

		config->rx_num = 0;
		config->tx_num = 0;
		config->btn_num = 0;
		config->is_valid_or_not = 0;
	}

	pt_debug(dev, DL_DEBUG,
		"%s:\n"
		"Input file is %s!\n"
		"CM test: %s\n"
		"CP test: %s\n"
		"rx_num is %d\n"
		"tx_num is %d\n"
		"btn_num is %d\n",
		__func__,
		config->is_valid_or_not == 1 ? "VALID" : "!!! INVALID !!!",
		config->cm_enabled == 1 ? "Found" : "Not found",
		config->cp_enabled == 1 ? "Found" : "Not found",
		config->rx_num,
		config->tx_num,
		config->btn_num);
}

/*******************************************************************************
 * FUNCTION: cmcp_test_case_field_init
 *
 * SUMMARY: Initialize the structure test_field_array.
 *
 * PARAMETERS:
 *  *test_field_array  - pointer to test_case_field structure
 *  *configuration     - pointer to configuration structure
 ******************************************************************************/
void cmcp_test_case_field_init(struct test_case_field *test_field_array,
	struct configuration *configs)
{
	struct test_case_field test_case_field_array[MAX_CASE_NUM] = {
		{"CM TEST INPUTS", 14, TEST_CASE_TYPE_NO,
				NULL, 0, 0, 0},
		{"CM_EXCLUDING_COL_EDGE", 21, TEST_CASE_TYPE_ONE,
				&configs->cm_excluding_col_edge, 0, 0, 0},
		{"CM_EXCLUDING_ROW_EDGE", 21, TEST_CASE_TYPE_ONE,
				&configs->cm_excluding_row_edge, 0, 0, 0},
		{"CM_GRADIENT_CHECK_COL", 21, TEST_CASE_TYPE_MUL,
				&configs->cm_max_table_gradient_cols_percent[0],
				0, 0, 0},
		{"CM_GRADIENT_CHECK_ROW", 21, TEST_CASE_TYPE_MUL,
				&configs->cm_max_table_gradient_rows_percent[0],
				0, 0, 0},
		{"CM_RANGE_LIMIT_ROW", 18, TEST_CASE_TYPE_ONE,
				&configs->cm_range_limit_row, 0, 0, 0},
		{"CM_RANGE_LIMIT_COL", 18, TEST_CASE_TYPE_ONE,
				&configs->cm_range_limit_col, 0, 0, 0},
		{"CM_MIN_LIMIT_CAL", 16, TEST_CASE_TYPE_ONE,
				&configs->cm_min_limit_cal, 0, 0, 0},
		{"CM_MAX_LIMIT_CAL", 16, TEST_CASE_TYPE_ONE,
				&configs->cm_max_limit_cal, 0, 0, 0},
		{"CM_MAX_DELTA_SENSOR_PERCENT", 27, TEST_CASE_TYPE_ONE,
				&configs->cm_max_delta_sensor_percent, 0, 0, 0},
		{"CM_MAX_DELTA_BUTTON_PERCENT", 27, TEST_CASE_TYPE_ONE,
				&configs->cm_max_delta_button_percent, 0, 0, 0},
		{"PER_ELEMENT_MIN_MAX_TABLE_BUTTON", 32, TEST_CASE_TYPE_MUL,
				&configs->cm_min_max_table_btn[0], 0, 0, 0},
		{"PER_ELEMENT_MIN_MAX_TABLE_SENSOR", 32,
				TEST_CASE_TYPE_MUL_LINES,
				&configs->cm_min_max_table_sensor[0], 0, 0, 0},
		{"CP TEST INPUTS", 14, TEST_CASE_TYPE_NO,
				NULL, 0, 0, 0},
		{"CP_PER_ELEMENT_MIN_MAX_BUTTON", 29, TEST_CASE_TYPE_MUL,
				&configs->cp_min_max_table_btn[0], 0, 0, 0},
		{"CP_MAX_DELTA_SENSOR_RX_PERCENT", 30, TEST_CASE_TYPE_ONE,
				&configs->cp_max_delta_sensor_rx_percent,
				0, 0, 0},
		{"CP_MAX_DELTA_SENSOR_TX_PERCENT", 30, TEST_CASE_TYPE_ONE,
				&configs->cp_max_delta_sensor_tx_percent,
				0, 0, 0},
		{"CP_MAX_DELTA_BUTTON_PERCENT", 27, TEST_CASE_TYPE_ONE,
				&configs->cp_max_delta_button_percent, 0, 0, 0},
		{"MIN_BUTTON", 10, TEST_CASE_TYPE_ONE,
				&configs->min_button, 0, 0, 0},
		{"MAX_BUTTON", 10, TEST_CASE_TYPE_ONE,
				&configs->max_button, 0, 0, 0},
		{"PER_ELEMENT_MIN_MAX_RX", 22, TEST_CASE_TYPE_MUL,
				&configs->cp_min_max_table_rx[0], 0, 0, 0},
		{"PER_ELEMENT_MIN_MAX_TX", 22, TEST_CASE_TYPE_MUL,
				&configs->cp_min_max_table_tx[0], 0, 0, 0},
	};

	memcpy(test_field_array, test_case_field_array,
		sizeof(struct test_case_field) * MAX_CASE_NUM);
}

/*******************************************************************************
 * FUNCTION: pt_parse_cmcp_threshold_file_common
 *
 * SUMMARY: Parses cmcp threshold file and stores to the data structure.
 *
 * PARAMETERS:
 *  *dev       - pointer to devices structure
 *  *buf       - pointer to input buffer
 *   file_size - file size
 ******************************************************************************/
static ssize_t pt_parse_cmcp_threshold_file_common(
	struct device *dev, const char *buf, u32 file_size)
{
	struct pt_device_access_data *dad
		= pt_get_device_access_data(dev);
	ssize_t rc = 0;
	u32 case_count = 0;

	pt_debug(dev, DL_INFO,
		"%s: Start parsing cmcp threshold file. File size is %d\n",
		__func__, file_size);

	cmcp_test_case_field_init(dad->test_field_array, dad->configs);

	/* Get all the cases from .csv file */
	case_count = cmcp_get_case_info_from_threshold_file(dev,
		buf, dad->test_search_array, file_size);

	pt_debug(dev, DL_INFO,
		"%s: Number of cases found in CSV file: %d\n",
		__func__, case_count);

	/* Search cases */
	cmcp_get_configuration_info(dev,
		buf,
		dad->test_search_array, case_count, dad->test_field_array,
		dad->configs, file_size);

	/* Get basic information */
	cmcp_get_basic_info(dev, dad->test_field_array, dad->configs);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_cmcp_threshold_loading_store
 *
 * SUMMARY: The store method for the cmcp_threshold_loading sysfs node. The
 *  passed in value controls if threshold loading is performed.
 *
 * RETURN: Size of passed in buffer is success
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes
 *	*buf  - pointer to buffer that hold the command parameters
 *	 size - size of buf
 ******************************************************************************/
static ssize_t pt_cmcp_threshold_loading_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_device_access_data *dad = pt_get_device_access_data(dev);
	ssize_t length;
	u32 input_data[3];
	int rc = 0;

	length = cmd->parse_sysfs_input(dev, buf, size, input_data,
			ARRAY_SIZE(input_data));

	if (length != 1) {
		pt_debug(dev, DL_WARN, "%s: Invalid number of arguments\n",
			__func__);
		rc = -EINVAL;
		goto exit;
	}

	mutex_lock(&dad->cmcp_threshold_lock);

	if (input_data[0] == 1)
		dad->cmcp_threshold_loading = true;
	else if (input_data[0] == -1)
		dad->cmcp_threshold_loading = false;
	else if (input_data[0] == 0 && dad->cmcp_threshold_loading) {
		dad->cmcp_threshold_loading = false;

		if (dad->cmcp_threshold_size == 0) {
			pt_debug(dev, DL_ERROR, "%s: No cmcp threshold data\n",
					__func__);
			goto exit_free;
		}

		/* Clear test executed flag */
		dad->test_executed = 0;

		pt_parse_cmcp_threshold_file_common(dev,
			&dad->cmcp_threshold_data[0], dad->cmcp_threshold_size);

		/* Mark valid */
		dad->builtin_cmcp_threshold_status = 0;
		/* Restore test item to default value when new file input */
		dad->cmcp_test_items = 0;
	} else {
		pt_debug(dev, DL_WARN, "%s: Invalid value\n", __func__);
		rc = -EINVAL;
		mutex_unlock(&dad->cmcp_threshold_lock);
		goto exit;
	}

exit_free:
	kfree(dad->cmcp_threshold_data);
	dad->cmcp_threshold_data = NULL;
	dad->cmcp_threshold_size = 0;
	mutex_unlock(&dad->cmcp_threshold_lock);

exit:
	if (rc)
		return rc;
	return size;
}

static DEVICE_ATTR(cmcp_threshold_loading, 0200,
	NULL, pt_cmcp_threshold_loading_store);

/*******************************************************************************
 * FUNCTION: pt_cmcp_threshold_data_write
 *
 * SUMMARY: The write method for the cmcp_threshold_data_sysfs node. The passed
 *  in data (threshold file) is written to the threshold buffer.
 *
 * RETURN: Size of passed in buffer is success
 *
 * PARAMETERS:
 *  *filp     - pointer to file structure
 *  *kobj     - pointer to kobject structure
 *  *bin_attr - pointer to bin_attribute structure
 *   buf      - pointer to cmd input buffer
 *   offset   - offset index to store input buffer
 *   count    - size of data in buffer
 ******************************************************************************/
static ssize_t pt_cmcp_threshold_data_write(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct pt_device_access_data *dad
		= pt_get_device_access_data(dev);
	u8 *p;

	pt_debug(dev, DL_INFO, "%s: offset:%lld count:%zu\n",
		__func__, offset, count);

	mutex_lock(&dad->cmcp_threshold_lock);

	if (!dad->cmcp_threshold_loading) {
		mutex_unlock(&dad->cmcp_threshold_lock);
		return -ENODEV;
	}

	p = krealloc(dad->cmcp_threshold_data, offset + count, GFP_KERNEL);
	if (!p) {
		kfree(dad->cmcp_threshold_data);
		dad->cmcp_threshold_data = NULL;
		mutex_unlock(&dad->cmcp_threshold_lock);
		return -ENOMEM;
	}
	dad->cmcp_threshold_data = p;

	memcpy(&dad->cmcp_threshold_data[offset], buf, count);
	dad->cmcp_threshold_size += count;

	mutex_unlock(&dad->cmcp_threshold_lock);

	return count;
}

static struct bin_attribute bin_attr_cmcp_threshold_data = {
	.attr = {
		.name = "cmcp_threshold_data",
		.mode = 0200,
	},
	.size = 0,
	.write = pt_cmcp_threshold_data_write,
};


/*******************************************************************************
 * FUNCTION: pt_suspend_scan_cmd_
 *
 * SUMMARY: Non-protected wrapper function for suspend scan command
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev - pointer to devices structure
 ******************************************************************************/
static int pt_suspend_scan_cmd_(struct device *dev)
{
	int rc;

	rc = cmd->nonhid_cmd->suspend_scanning(dev, 0);
	if (rc)
		pt_debug(dev, DL_ERROR, "%s: Suspend scan failed rc = %d\n",
			__func__, rc);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_resume_scan_cmd_
 *
 * SUMMARY: Non-protected wrapper function for resume scan command
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev - pointer to devices structure
 ******************************************************************************/
static int pt_resume_scan_cmd_(struct device *dev)
{
	int rc;

	rc = cmd->nonhid_cmd->resume_scanning(dev, 0);
	if (rc)
		pt_debug(dev, DL_ERROR, "%s: Resume scan failed rc = %d\n",
			__func__, rc);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_exec_scan_cmd_
 *
 * SUMMARY: Non-protected wrapper function for execute scan command
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev       - pointer to devices structure
 *	 scan_type - type of panel scan to perform (PIP2 only)
 ******************************************************************************/
static int pt_exec_scan_cmd_(struct device *dev, u8 scan_type)
{
	int rc;

	rc = cmd->nonhid_cmd->exec_panel_scan(dev, PT_CORE_CMD_UNPROTECTED,
		scan_type);
	if (rc)
		pt_debug(dev, DL_ERROR,
			"%s: Heatmap start scan failed rc = %d\n",
			__func__, rc);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_ret_scan_data_cmd_
 *
 * SUMMARY: Non-protected wrapper function for retrieve panel data command
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev             - pointer to device structure
 *	 read_offset     - read pointer offset
 *	 read_count      - length of data to read
 *	 data_id         - enumerated test ID to read selftest results from
 *	*response        - pointer to store the read response status
 *	*config          - pointer to store config data
 *	*actual_read_len - pointer to store data length actually read
 *	*return_buf      - pointer to the read buffer
 ******************************************************************************/
static int pt_ret_scan_data_cmd_(struct device *dev, u16 read_offset,
		u16 read_count, u8 data_id, u8 *response, u8 *config,
		u16 *actual_read_len, u8 *return_buf)
{
	int rc;

	rc = cmd->nonhid_cmd->retrieve_panel_scan(dev, 0, read_offset,
			read_count, data_id, response, config, actual_read_len,
			return_buf);
	if (rc)
		pt_debug(dev, DL_ERROR,
				"%s: Retrieve scan data failed rc = %d\n",
				__func__, rc);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_get_data_structure_cmd_
 *
 * SUMMARY: Non-protected wrapper function for get data structure command
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev             - pointer to device structure
 *	 read_offset     - read pointer offset
 *	 read_length     - length of data to read
 *	 data_id         - data ID to read
 *	*status          - pointer to store the read response status
 *	*data_format     - pointer to store format of data read
 *	*actual_read_len - pointer to store data length actually read
 *	*data            - pointer to store data read
 ******************************************************************************/
static int pt_get_data_structure_cmd_(struct device *dev, u16 read_offset,
		u16 read_length, u8 data_id, u8 *status, u8 *data_format,
		u16 *actual_read_len, u8 *data)
{
	int rc;

	rc = cmd->nonhid_cmd->get_data_structure(dev, 0, read_offset,
			read_length, data_id, status, data_format,
			actual_read_len, data);
	if (rc)
		pt_debug(dev, DL_ERROR,
				"%s: Get data structure failed rc = %d\n",
				__func__, rc);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_run_selftest_cmd_
 *
 * SUMMARY: Non-protected wrapper function for run self test command
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev                  - pointer to device structure
 *	 test_id              - enumerated test ID to run
 *	 write_idacs_to_flash - flag whether to write new IDACS to flash
 *	*status               - pointer to store the read response status
 *	*summary_results      - pointer to store the results summary
 *	*results_available    - pointer to store if results are available
 ******************************************************************************/
static int pt_run_selftest_cmd_(struct device *dev, u8 test_id,
		u8 write_idacs_to_flash, u8 *status, u8 *summary_result,
		u8 *results_available)
{
	int rc;

	rc = cmd->nonhid_cmd->run_selftest(dev, 0, test_id,
			write_idacs_to_flash, status, summary_result,
			results_available);
	if (rc)
		pt_debug(dev, DL_ERROR, "%s: Run self test failed rc = %d\n",
				__func__, rc);
	return rc;
}


/*******************************************************************************
 * FUNCTION: pt_get_selftest_result_cmd_
 *
 * SUMMARY: Non-protected wrapper function for get self test result command
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev             - pointer to device structure
 *	 read_offset     - read pointer offset
 *	 read_length     - length of data to read
 *	 test_id         - enumerated test ID to read selftest results from
 *	*status          - pointer to store the read response status
 *	*actual_read_len - pointer to store data length actually read
 *	*data            - pointer to where the data read is stored
 ******************************************************************************/
static int pt_get_selftest_result_cmd_(struct device *dev,
		u16 read_offset, u16 read_length, u8 test_id, u8 *status,
		u16 *actual_read_len, u8 *data)
{
	int rc;

	rc = cmd->nonhid_cmd->get_selftest_result(dev, 0, read_offset,
			read_length, test_id, status, actual_read_len, data);
	if (rc)
		pt_debug(dev, DL_ERROR,
			"%s: Get self test result failed rc = %d\n",
			__func__, rc);
	return rc;
}

/*******************************************************************************
 * FUNCTION: _pt_calibrate_ext_cmd
 *
 * SUMMARY: Wrapper function to function calibrate_ext() in pt_core_commands
 *  structure
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *  *dev          - pointer to device structure
 *  *cal_data     - pointer to extended calibration data structure
 *  *status       - pointer to where the command response status is stored
 ******************************************************************************/
static int _pt_calibrate_ext_cmd(struct device *dev,
		struct pt_cal_ext_data *cal_data, u8 *status)
{
	int rc;

	rc = cmd->nonhid_cmd->calibrate_ext(dev,
			PT_CORE_CMD_UNPROTECTED, cal_data, status);
	return rc;
}

/*******************************************************************************
 * FUNCTION: _pt_calibrate_idacs_cmd
 *
 * SUMMARY: Wrapper function to function calibrate_idacs() in pt_core_commands
 *  structure
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *  *dev          - pointer to device structure
 *   sensing_mode - sense mode to calibrate (0-5)
 *  *status       - pointer to where the command response status is stored
 ******************************************************************************/
static int _pt_calibrate_idacs_cmd(struct device *dev,
		u8 sensing_mode, u8 *status)
{
	int rc;

	rc = cmd->nonhid_cmd->calibrate_idacs(dev, 0, sensing_mode, status);
	return rc;
}

/*******************************************************************************
 * FUNCTION: _pt_initialize_baselines_cmd
 *
 * SUMMARY: Wrapper function to call initialize_baselines() in pt_core_commands
 *  structure
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *  *dev          - pointer to device structure
 *   sensing_mode - enumerated ID against which to initialize the baseline
 *  *status       - pointer to where the command response statas is stored
 ******************************************************************************/
static int _pt_initialize_baselines_cmd(struct device *dev,
		u8 sensing_mode, u8 *status)
{
	int rc;

	rc = cmd->nonhid_cmd->initialize_baselines(dev, 0, sensing_mode,
			status);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_perform_calibration
 *
 * SUMMARY: For Gen5/6, Send the PIP1 Calibrate IDACs command (0x28). For TT/TC,
 *  send PIP1 Extended Calibrate command (0x30).
 *
 * NOTE: Panel scan must be suspended prior to calling this function.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev - pointer to device structure
 ******************************************************************************/
static int pt_perform_calibration(struct device *dev)
{
	struct pt_cal_ext_data cal_data = {0};
	u8 dut_gen = cmd->request_dut_generation(dev);
	u8 mode;
	u8 status;
	int rc;

	if (dut_gen == DUT_PIP1_ONLY) {
		for (mode = 0; mode < 3; mode++) {
			rc = _pt_calibrate_idacs_cmd(dev, mode, &status);
			if (rc < 0) {
				pt_debug(dev, DL_ERROR,
					"%s: calibrate idac error, mode= %d, rc = %d\n",
					__func__, mode, rc);
				break;
			}
		}
	} else {
		memset(&cal_data, 0, sizeof(struct pt_cal_ext_data));
		rc = _pt_calibrate_ext_cmd(dev, &cal_data, &status);
		if (rc < 0)
			pt_debug(dev, DL_ERROR,
				"%s: extended calibrate error, rc = %d\n",
				__func__, rc);
	}

	return rc;
}

/*******************************************************************************
 * FUNCTION: prepare_print_buffer
 *
 * SUMMARY: Format input buffer to out buffer with Hex base,and format "status"
 *  to decimal base.
 *
 * RETURN:
 *	 size of formated data in output buffer
 *
 * PARAMETERS:
 *   status       - Indicate test result:0(STATUS_SUCCESS),-1(STATUS_FAIL)
 *  *in_buf       - input buffer to be formated
 *   length       - length of input buffer
 *  *out_buf      - output buffer to store formated data
 *   out_buf_size - length of output buffer
 *   out_format   - format of ouput data (5 supported formats):
 *    PT_PR_FORMAT_DEFAULT      : format all data as a column
 *    PT_PR_FORMAT_U8_SPACE     : sort status bytes and self test results,
 *      and format the results as a row, each element include 1 byte
 *    PT_PR_FORMAT_U16_SPACE    : sort status bytes and self test results,
 *      and format the results as a row, each element include 2 byte
 *    PT_PR_FORMAT_U8_NO_SPACE  : sort status bytes and self test results,
 *      and format the results as a row, no space between the elements
 *    PT_PR_FORMAT_U32_SPACE    : sort status bytes and self test results,
 *      and format the results as a row, each element include 4 byte
 ******************************************************************************/
static int prepare_print_buffer(int status, u8 *in_buf, int length,
		u8 *out_buf, size_t out_buf_size, u8 out_format)
{
	int index = 0;
	int data_length;
	int i;

	index += scnprintf(out_buf, out_buf_size, "Status: %d\n", status);

	if (out_format == PT_PR_FORMAT_DEFAULT) {
		for (i = 0; i < length; i++)
			index += scnprintf(&out_buf[index],
				out_buf_size - index,
				"%02X\n", in_buf[i]);
	} else {
		index += scnprintf(&out_buf[index],
			out_buf_size - index,
			"Response Status[1-%d]: ", MIN(length, 3));
		for (i = 0; i < MIN(length, 3); i++)
			index += scnprintf(&out_buf[index],
				out_buf_size - index,
				"%02X ", in_buf[i]);
		index += scnprintf(&out_buf[index], out_buf_size - index, "\n");
		if (length <= 6) {
			goto exit;
		} else {
			data_length = get_unaligned_le16(&in_buf[4]);
			index += scnprintf(&out_buf[index],
				out_buf_size - index, "RAW_DATA: ");
		}

		if (out_format == PT_PR_FORMAT_U8_SPACE) {
			for (i = 6; i < length; i++)
				index += scnprintf(&out_buf[index],
					out_buf_size - index,
					"%02X ", in_buf[i]);
			index += scnprintf(&out_buf[index],
				out_buf_size - index,
				":(%d bytes)\n", data_length);
		} else if (out_format == PT_PR_FORMAT_U16_SPACE) {
			for (i = 6; (i + 1) < length; i += 2)
				index += scnprintf(&out_buf[index],
					out_buf_size - index, "%04X ",
					get_unaligned_le16(&in_buf[i]));
			index += scnprintf(&out_buf[index],
				out_buf_size - index,
				":(%d words)\n", (length-6)/2);
		} else if (out_format == PT_PR_FORMAT_U8_NO_SPACE) {
			for (i = 6; i < length; i++)
				index += scnprintf(&out_buf[index],
					out_buf_size - index,
					"%02X", in_buf[i]);
			index += scnprintf(&out_buf[index],
				out_buf_size - index,
				":(%d bytes)\n", data_length);
		} else if (out_format == PT_PR_FORMAT_U32_SPACE) {
			for (i = 6; (i + 1) < length; i += 4)
				index += scnprintf(&out_buf[index],
					out_buf_size - index, "%08X ",
					get_unaligned_le32(&in_buf[i]));
			index += scnprintf(&out_buf[index],
				out_buf_size - index,
				":(%d 32bit values)\n", (length-6)/4);
		}
	}

exit:
	return index;
}

/*******************************************************************************
 * FUNCTION: pt_run_and_get_selftest_result
 *
 * SUMMARY: Run the selftest and store the test result in the
 *  pt_device_access_data struct.
 *
 * RETURN:
 *	 >0 : Size of debugfs data to print
 *	 <0 : failure
 *	  0 : success
 *
 * NOTE: "Status: x" - x will contain the first error code if any
 *
 * PARAMETERS:
 *  *dev                - pointer to device structure
 *   protect            - flag to call protected or non-protected
 *  *buf                - pointer to print buf of return data
 *   buf_len            - length of print buf of return data
 *   test_id            - selftest id
 *   read_length        - max length to stor return data
 *   get_result_on_pass - indicate whether to get result when finish test
 *   print_results      - print results to log
 * (true:get result;false:don't get result )
 *   print_format       - format of print results
 ******************************************************************************/
static ssize_t pt_run_and_get_selftest_result(struct device *dev,
		int protect, char *buf, size_t buf_len, u8 test_id,
		u16 read_length, bool get_result_on_pass, bool print_results,
		u8 print_format)
{
	struct pt_device_access_data *dad = pt_get_device_access_data(dev);
	int status = STATUS_SUCCESS;
	u8 cmd_status = STATUS_SUCCESS;
	u8 summary_result = 0;
	u8 sys_mode = FW_SYS_MODE_UNDEFINED;
	u16 act_length = 0;
	int length = 0;
	int size = 0;
	int rc;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	if (protect == PT_CORE_CMD_PROTECTED) {
		rc = cmd->request_exclusive(dev, PT_REQUEST_EXCLUSIVE_TIMEOUT);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error on request exclusive rc = %d\n",
				__func__, rc);
			status = -EPERM;
			goto put_pm_runtime;
		}
	}

	/* Get the current scan state so we restore to the same at the end */
	rc = cmd->request_get_fw_mode(dev, PT_CORE_CMD_UNPROTECTED, &sys_mode,
		NULL);
	if (rc) {
		status = rc;
		goto release_exclusive;
	}

	if (sys_mode != FW_SYS_MODE_TEST) {
		rc = pt_suspend_scan_cmd_(dev);
		if (rc) {
			pt_debug(dev, DL_ERROR,
					"%s: Error on suspend scan rc = %d\n",
					__func__, rc);
			status = -EPERM;
			goto release_exclusive;
		}
	}

	/* Sleep for 20ms to allow the last scan to be available in FW */
	msleep(20);

	rc = pt_run_selftest_cmd_(dev, test_id, 0,
		&cmd_status, &summary_result, NULL);
	if (rc) {
		/* Error sending self test */
		pt_debug(dev, DL_ERROR,
			"%s: Error on run self test for test_id:%d rc = %d\n",
			__func__, test_id, rc);
		status = rc;
		goto resume_scan;
	}
	if (cmd_status) {
		/* Self test response status failure */
		pt_debug(dev, DL_WARN,
			"%s: Test ID: 0x%02X resulted in status: 0x%02X\n",
			__func__, test_id, cmd_status);
		status = cmd_status;
	}

	dad->si = cmd->request_sysinfo(dad->dev);
	if (!dad->si) {
		pt_debug(dad->dev, DL_ERROR,
			"%s: Fail get sysinfo pointer from core\n", __func__);
		if (status == STATUS_SUCCESS)
			status = -EINVAL;
		goto resume_scan;
	}
	if (IS_PIP_VER_GE(dad->si, 1, 11)) {
		/* PIP1.11+ does not report the summary_result in byte 6 */
		summary_result = cmd_status;
	}

	/* Form response buffer */
	dad->ic_buf[0] = cmd_status;
	dad->ic_buf[1] = summary_result;

	pt_debug(dev, DL_INFO, "%s: Run Self Test cmd status = %d\n",
		__func__, cmd_status);
	pt_debug(dev, DL_INFO, "%s: Run Self Test result summary = %d\n",
		__func__, summary_result);

	length = 2;

	/*
	 * Get data if requested and the cmd status indicates that the test
	 * completed with either a pass or a fail. All other status codes
	 * indicate the test itself was not run so there is no data to retrieve
	 */
	if ((cmd_status == PT_ST_RESULT_PASS ||
	     cmd_status == PT_ST_RESULT_FAIL)  && get_result_on_pass) {
		rc = pt_get_selftest_result_cmd_(dev, 0, read_length,
			test_id, &cmd_status, &act_length, &dad->ic_buf[6]);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error on get self test result rc = %d\n",
				__func__, rc);
			if (status == STATUS_SUCCESS)
				status = rc;
			goto resume_scan;
		}

		pt_debug(dev, DL_INFO, "%s: Get Self Test result status = %d\n",
			__func__, cmd_status);

		/* Only store new status if no error on running self test */
		if (status == STATUS_SUCCESS)
			status = cmd_status;

		dad->ic_buf[2] = cmd_status;
		dad->ic_buf[3] = test_id;
		dad->ic_buf[4] = LOW_BYTE(act_length);
		dad->ic_buf[5] = HI_BYTE(act_length);

		length = 6 + act_length;
	}

resume_scan:
	/* Only resume scanning if we suspended it */
	if (sys_mode == FW_SYS_MODE_SCANNING)
		pt_resume_scan_cmd_(dev);

release_exclusive:
	if (protect == PT_CORE_CMD_PROTECTED)
		cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	/* Communication error */
	if (status < 0)
		length = 0;

	if (print_results) {
		size = prepare_print_buffer(status, dad->ic_buf, length,
			buf, buf_len, print_format);
		rc = size;
	}

	mutex_unlock(&dad->sysfs_lock);

	return rc;
}

struct pt_device_access_debugfs_data {
	struct pt_device_access_data *dad;
	ssize_t pr_buf_len;
	u8 pr_buf[10 * PT_MAX_PRBUF_SIZE];
};

/*******************************************************************************
 * FUNCTION: pt_device_access_debugfs_open
 *
 * SUMMARY: Open the device_access debugfs node to initialize.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *  *inode - pointer to inode structure
 *  *filp  - pointer to file structure
 ******************************************************************************/
static int pt_device_access_debugfs_open(struct inode *inode,
		struct file *filp)
{
	struct pt_device_access_data *dad = inode->i_private;
	struct pt_device_access_debugfs_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dad = dad;

	filp->private_data = data;

	return nonseekable_open(inode, filp);
}

/*******************************************************************************
 * FUNCTION: pt_device_access_debugfs_release
 *
 * SUMMARY: Close the device_access debugfs node to free pointer.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *  *inode - pointer to inode structure
 *  *filp  - pointer to file structure
 ******************************************************************************/
static int pt_device_access_debugfs_release(struct inode *inode,
		struct file *filp)
{
	kfree(filp->private_data);

	return 0;
}

#define PT_DEBUGFS_FOPS(_name, _read, _write) \
static const struct file_operations _name##_debugfs_fops = { \
	.open = pt_device_access_debugfs_open, \
	.release = pt_device_access_debugfs_release, \
	.read = _read, \
	.write = _write, \
}

/*******************************************************************************
 * FUNCTION: panel_scan_debugfs_read
 *
 * SUMMARY: This function retrieves a full panel scan by sending the following
 *	PIP commands:
 *		1) Suspend Scanning
 *		2) Execute Panel Scan
 *		3) Retrieve Panel Scan (n times to retrieve full scan)
 *		4) Resume Scanning
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *      *filp   - file pointer to debugfs file
 *      *buf    - the user space buffer to read to
 *       count  - the maximum number of bytes to read
 *      *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t panel_scan_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	struct device *dev = dad->dev;
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int status = STATUS_FAIL;
	u8 config;
	u16 num_elem_read;
	int length = 0;
	u8 element_size = 0;
	u8 *buf_out;
	u8 *buf_offset;
	u8 sys_mode = FW_SYS_MODE_UNDEFINED;
	int elem_offset = 0;
	int rc;
	int print_idx = 0;
	int i;

	mutex_lock(&dad->debugfs_lock);
	buf_out = dad->panel_scan_data_buf;
	if (!buf_out)
		goto release_mutex;

	pm_runtime_get_sync(dev);

	/*
	 * This function will re-enter if the panel_scan_size is greater than
	 * count (count is the kernel page size which is typically 4096), on
	 * re-entry, *ppos will retain how far the last copy to user space
	 * completed
	 */
	if (*ppos) {
		if (*ppos >= dad->panel_scan_size)
			goto release_mutex;

		print_idx = simple_read_from_buffer(buf, count, ppos,
			buf_out, dad->panel_scan_size);

		pt_debug(dev, DL_DEBUG, "%s: Sent %d bytes to user space\n",
			__func__, print_idx);

		goto release_mutex;
	}

	rc = cmd->request_exclusive(dev, PT_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Error on request exclusive rc = %d\n",
			__func__, rc);
		goto put_pm_runtime;
	}

	/* Get the current scan state so we restore to the same at the end */
	rc = cmd->request_get_fw_mode(dev, PT_CORE_CMD_UNPROTECTED, &sys_mode,
		NULL);
	if (rc) {
		status = rc;
		goto release_exclusive;
	}

	if (sys_mode != FW_SYS_MODE_TEST) {
		rc = pt_suspend_scan_cmd_(dev);
		if (rc) {
			pt_debug(dev, DL_ERROR,
					"%s: Error on suspend scan rc = %d\n",
					__func__, rc);
			goto release_exclusive;
		}
	}

	rc = pt_exec_scan_cmd_(dev, dad->panel_scan_type_id);
	if (rc) {
		pt_debug(dev, DL_ERROR,
				"%s: Error on execute panel scan rc = %d\n",
				__func__, rc);
		goto resume_scan;
	}

	/* Set length to max to read all */
	rc = pt_ret_scan_data_cmd_(dev, 0, 0xFFFF,
			dad->panel_scan_retrieve_id, dad->ic_buf, &config,
			&num_elem_read, NULL);
	if (rc) {
		pt_debug(dev, DL_ERROR,
				"%s: Error on retrieve panel scan rc = %d\n",
				__func__, rc);
		goto resume_scan;
	}

	length = get_unaligned_le16(&dad->ic_buf[0]);
	buf_offset = dad->ic_buf + length;
	element_size = config & 0x07;
	elem_offset = num_elem_read;
	while (num_elem_read > 0) {
		rc = pt_ret_scan_data_cmd_(dev, elem_offset, 0xFFFF,
				dad->panel_scan_retrieve_id, NULL, &config,
				&num_elem_read, buf_offset);
		if (rc)
			goto resume_scan;

		length += num_elem_read * element_size;
		buf_offset = dad->ic_buf + length;
		elem_offset += num_elem_read;
		if (num_elem_read < 0x7A)
			break;
	}
	/* Reconstruct cmd header */
	put_unaligned_le16(length, &dad->ic_buf[0]);
	put_unaligned_le16(elem_offset, &dad->ic_buf[7]);

	status = STATUS_SUCCESS;

resume_scan:
	/* Only resume scanning if we suspended it */
	if (sys_mode == FW_SYS_MODE_SCANNING)
		pt_resume_scan_cmd_(dev);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;
	if (cd->show_timestamp)
		print_idx += scnprintf(buf_out + print_idx, TTHE_TUNER_MAX_BUF,
			"[%u] SCAN_DATA:", pt_get_time_stamp());
	else
		print_idx += scnprintf(buf_out + print_idx, TTHE_TUNER_MAX_BUF,
			"SCAN_DATA:");

	for (i = 0; i < length; i++)
		print_idx += scnprintf(buf_out + print_idx,
			TTHE_TUNER_MAX_BUF - print_idx,
			"%02X ", dad->ic_buf[i]);
	print_idx += scnprintf(buf_out + print_idx,
			TTHE_TUNER_MAX_BUF - print_idx,
			":(%d bytes)\n", length);

	/*
	 * Save the size of the full scan which this function uses on re-entry
	 * to send the data back to user space in 'count' size chuncks
	 */
	dad->panel_scan_size = print_idx;
	print_idx = simple_read_from_buffer(buf, count, ppos, buf_out,
		print_idx);
	pt_debug(dev, DL_DEBUG, "%s: Sent %d bytes to user space\n",
		__func__, print_idx);

release_mutex:
	mutex_unlock(&dad->debugfs_lock);
	return print_idx;
}

/*******************************************************************************
 * FUNCTION: panel_scan_debugfs_write
 *
 * SUMMARY: Store the type of panel scan the read method will perform.
 *
 * RETURN: Size of debugfs data write
 *
 * PARAMETERS:
 *      *filp   - file pointer to debugfs file
 *      *buf    - the user space buffer to write to
 *       count  - the maximum number of bytes to write
 *      *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t panel_scan_debugfs_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	ssize_t length;
	u32 input_data[3];
	int rc = 0;

	rc = simple_write_to_buffer(data->pr_buf, sizeof(data->pr_buf), ppos,
			buf, count);
	if (rc < 0)
		return rc;

	count = rc;

	mutex_lock(&dad->debugfs_lock);
	length = cmd->parse_sysfs_input(dad->dev, data->pr_buf, count,
			input_data, ARRAY_SIZE(input_data));
	switch (length) {
	case 1:
		dad->panel_scan_retrieve_id = input_data[0];
		dad->panel_scan_type_id = 0;
		break;
	case 2:
		dad->panel_scan_retrieve_id = input_data[0];
		dad->panel_scan_type_id = input_data[1];
		break;
	default:
		pt_debug(dad->dev, DL_ERROR,
				"%s: Malformed input\n", __func__);
		rc = -EINVAL;
	}
	mutex_unlock(&dad->debugfs_lock);

	if (rc)
		return rc;
	return count;
}

/*******************************************************************************
 * FUNCTION: panel_scan_debugfs_open
 *
 * SUMMARY: Open the panel_scan debugfs node to initialize.
 *
 * RETURN: 0 = success
 *        !0 = failure
 *
 * PARAMETERS:
 *      *inode - file inode number
 *      *filp  - file pointer to debugfs file
 ******************************************************************************/
static int panel_scan_debugfs_open(struct inode *inode,
		struct file *filp)
{
	struct pt_device_access_data *dad = inode->i_private;
	struct pt_device_access_debugfs_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dad = dad;
	data->pr_buf_len = 4 * PT_MAX_PRBUF_SIZE;

	filp->private_data = data;

	return nonseekable_open(inode, filp);
}

/*******************************************************************************
 * FUNCTION: panel_scan_debugfs_close
 *
 * SUMMARY: Close the panel_scan debugfs node to free pointer.
 *
 * RETURN: 0 = success
 *
 * PARAMETERS:
 *      *inode - file inode number
 *      *filp  - file pointer to debugfs file
 ******************************************************************************/
static int panel_scan_debugfs_close(struct inode *inode,
		struct file *filp)
{
	kfree(filp->private_data);
	filp->private_data = NULL;
	return 0;
}

static const struct file_operations panel_scan_fops = {
	.open = panel_scan_debugfs_open,
	.release = panel_scan_debugfs_close,
	.read = panel_scan_debugfs_read,
	.write = panel_scan_debugfs_write,
};

/*******************************************************************************
 * FUNCTION: get_idac_debugfs_read
 *
 * SUMMARY: Retrieve data structure with idac data id by sending the following
 *	PIP commands:
 *		1) Suspend Scanning
 *		2) Retrieve data structure
 *		3) Resume Scanning
 *	The "Status: n" this node prints, 'n' will be:
 *		- zero for a full pass
 *		- negative for TTDL communication errors
 *		- positive for any FW status errors
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *      *filp   - file pointer to debugfs file
 *      *buf    - the user space buffer to read to
 *       count  - the maximum number of bytes to read
 *      *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t get_idac_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	struct device *dev = dad->dev;
	int status = STATUS_FAIL;
	u8 cmd_status = 0;
	u8 data_format = 0;
	u16 act_length = 0;
	int length = 0;
	int rc;

	if (*ppos)
		goto exit;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, PT_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR,
			"%s: Error on request exclusive rc = %d\n",
			__func__, rc);
		goto put_pm_runtime;
	}

	rc = pt_suspend_scan_cmd_(dev);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR, "%s: Error on suspend scan rc = %d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = pt_get_data_structure_cmd_(dev, 0, PIP_CMD_MAX_LENGTH,
		dad->get_idac_data_id, &cmd_status, &data_format,
		&act_length, &dad->ic_buf[5]);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR,
			"%s: Error on get data structure rc = %d\n",
			__func__, rc);
		goto resume_scan;
	}

	dad->ic_buf[0] = cmd_status;
	dad->ic_buf[1] = dad->get_idac_data_id;
	dad->ic_buf[2] = LOW_BYTE(act_length);
	dad->ic_buf[3] = HI_BYTE(act_length);
	dad->ic_buf[4] = data_format;

	length = 5 + act_length;

	status = cmd_status;

resume_scan:
	pt_resume_scan_cmd_(dev);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	data->pr_buf_len = prepare_print_buffer(status, dad->ic_buf, length,
		data->pr_buf, sizeof(data->pr_buf), PT_PR_FORMAT_DEFAULT);

	mutex_unlock(&dad->sysfs_lock);

exit:
	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

/*******************************************************************************
 * FUNCTION: get_idac_debugfs_write
 *
 * SUMMARY: Store the data id of idac,the read method will perform.
 *
 * RETURN: Size of debugfs data write
 *
 * PARAMETERS:
 *      *filp   - file pointer to debugfs file
 *      *buf    - the user space buffer to write to
 *       count  - the maximum number of bytes to write
 *      *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t get_idac_debugfs_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	ssize_t length;
	u32 input_data[2];
	int rc = 0;

	rc = simple_write_to_buffer(data->pr_buf, sizeof(data->pr_buf), ppos,
			buf, count);
	if (rc < 0)
		return rc;

	count = rc;

	mutex_lock(&dad->sysfs_lock);

	length = cmd->parse_sysfs_input(dad->dev, data->pr_buf, count,
			input_data, ARRAY_SIZE(input_data));
	if (length != 1) {
		pt_debug(dad->dev, DL_ERROR,
				"%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	dad->get_idac_data_id = input_data[0];

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return count;
}

PT_DEBUGFS_FOPS(get_idac, get_idac_debugfs_read, get_idac_debugfs_write);

/*******************************************************************************
 * FUNCTION: calibrate_ext_debugfs_read
 *
 * SUMMARY: Perform extended calibration command(0x30) which is flexible to
 *  calibrate each individual feature by adding extra parameter for calibration
 *  mode.
 *
 * NOTE:
 *	- This calibrate command requires the DUT to support PIP version >= 1.10
 *	- The "Status:" included in the printout will be one of the following:
 *		<0 - Linux error code (PIP transmission error)
 *		 0 - Full pass
 *		>0 - PIP error status
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	*filp   - file pointer to debugfs file
 *	*buf    - the user space buffer to read to
 *	 count  - the maximum number of bytes to read
 *	*ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t calibrate_ext_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	struct device *dev = dad->dev;
	int status = STATUS_FAIL;
	int length = 0;
	int rc;

	if (*ppos)
		goto exit;

	dad->si = cmd->request_sysinfo(dad->dev);
	if (!dad->si) {
		pt_debug(dad->dev, DL_ERROR,
			"%s: Fail get sysinfo pointer from core\n",
			__func__);
		status = -EIO;
		data->pr_buf_len = prepare_print_buffer(status, dad->ic_buf, 0,
			data->pr_buf, sizeof(data->pr_buf),
			PT_PR_FORMAT_DEFAULT);
		goto exit;
	}

	if (!IS_PIP_VER_GE(dad->si, 1, 10)) {
		pt_debug(dad->dev, DL_ERROR,
			"%s: extended calibration command is not supported\n",
			__func__);
		status = -EPROTONOSUPPORT;
		data->pr_buf_len = prepare_print_buffer(status, dad->ic_buf, 0,
			data->pr_buf, sizeof(data->pr_buf),
			PT_PR_FORMAT_DEFAULT);
		goto exit;
	}

	if (dad->cal_ext_data.mode == PT_CAL_EXT_MODE_UNDEFINED) {
		pt_debug(dad->dev, DL_ERROR,
			"%s: No parameters provided for calibration command\n",
			__func__);
		status = -EINVAL;
		data->pr_buf_len = prepare_print_buffer(status, dad->ic_buf, 0,
			data->pr_buf, sizeof(data->pr_buf),
			PT_PR_FORMAT_DEFAULT);
		goto exit;
	}

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, PT_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR,
				"%s: Error on request exclusive rc = %d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = pt_suspend_scan_cmd_(dev);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR, "%s: Error on suspend scan rc = %d\n",
				__func__, rc);
		goto release_exclusive;
	}

	rc = _pt_calibrate_ext_cmd(dev, &dad->cal_ext_data, &dad->ic_buf[0]);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR,
				"%s: Error on calibrate_ext rc = %d\n",
				__func__, rc);
		goto resume_scan;
	}

	/*
	 * Include PIP errors as positive status codes and report the data.
	 * No PIP error "0x00" in the response indicates full success
	 */
	length = 1;
	status = dad->ic_buf[0];

resume_scan:
	pt_resume_scan_cmd_(dev);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	/* Negative status codes are bus transmission errors and have no data */
	if (status < 0)
		length = 0;

	data->pr_buf_len = prepare_print_buffer(status, dad->ic_buf, length,
		data->pr_buf, sizeof(data->pr_buf), PT_PR_FORMAT_DEFAULT);

	mutex_unlock(&dad->sysfs_lock);

exit:
	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

/*******************************************************************************
 * FUNCTION: calibrate_ext_debugfs_write
 *
 * SUMMARY: Stores the calibration mode and up to three parameters to perform
 * individual features.
 *
 * RETURN: Size of debugfs data write
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to write to
 *		count  - the maximum number of bytes to write
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t calibrate_ext_debugfs_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	ssize_t length;
	u32 input_data[5];
	int rc = 0;
	int i = 0;

	rc = simple_write_to_buffer(data->pr_buf, sizeof(data->pr_buf), ppos,
			buf, count);
	if (rc < 0)
		return rc;

	count = rc;

	mutex_lock(&dad->sysfs_lock);

	length = cmd->parse_sysfs_input(dad->dev, data->pr_buf, count,
			input_data, ARRAY_SIZE(input_data));
	if ((length <= 4) && (length > 0)) {
		for (i = length; i < 4; i++)
			input_data[i] = 0;

		dad->cal_ext_data.mode = (u8)input_data[0];
		dad->cal_ext_data.data0 = (u8)input_data[1];
		dad->cal_ext_data.data1 = (u8)input_data[2];
		dad->cal_ext_data.data2 = (u8)input_data[3];
#ifdef TTDL_DIAGNOSTICS
		pt_debug(dad->dev, DL_INFO,
			"%s: calibration mode=%d, data[0..2]=0x%02X %02X %02X\n",
			__func__,
			dad->cal_ext_data.mode, dad->cal_ext_data.data0,
			dad->cal_ext_data.data1, dad->cal_ext_data.data2);
#endif
	} else {
		pt_debug(dad->dev, DL_ERROR,
				"%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return count;
}

PT_DEBUGFS_FOPS(calibrate_ext,
	calibrate_ext_debugfs_read, calibrate_ext_debugfs_write);

/*******************************************************************************
 * FUNCTION: calibrate_debugfs_read
 *
 * SUMMARY: Perform calibration by sending the following PIP commands:
 *     1) Suspend Scanning
 *     2) Execute calibrate
 *     3) Initialize baseline conditionally
 *     4) Resume Scanning
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to read to
 *		count  - the maximum number of bytes to read
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t calibrate_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	struct device *dev = dad->dev;
	int status = STATUS_FAIL;
	int length = 0;
	int rc;

	if (*ppos)
		goto exit;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, PT_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR,
				"%s: Error on request exclusive rc = %d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = pt_suspend_scan_cmd_(dev);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR, "%s: Error on suspend scan rc = %d\n",
				__func__, rc);
		goto release_exclusive;
	}

	rc = _pt_calibrate_idacs_cmd(dev, dad->calibrate_sensing_mode,
			&dad->ic_buf[0]);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR,
				"%s: Error on calibrate idacs rc = %d\n",
				__func__, rc);
		goto resume_scan;
	}

	length = 1;

	/* Check if baseline initialization is requested */
	if (dad->calibrate_initialize_baselines) {
		/* Perform baseline initialization for all modes */
		rc = _pt_initialize_baselines_cmd(dev, PT_IB_SM_MUTCAP |
				PT_IB_SM_SELFCAP | PT_IB_SM_BUTTON,
				&dad->ic_buf[length]);
		if (rc) {
			status = rc;
			pt_debug(dev, DL_ERROR,
				"%s: Error on initialize baselines rc = %d\n",
				__func__, rc);
			goto resume_scan;
		}

		length++;
	}

	status = STATUS_SUCCESS;

resume_scan:
	pt_resume_scan_cmd_(dev);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	data->pr_buf_len = prepare_print_buffer(status, dad->ic_buf, length,
		data->pr_buf, sizeof(data->pr_buf), PT_PR_FORMAT_DEFAULT);

	mutex_unlock(&dad->sysfs_lock);

exit:
	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

/*******************************************************************************
 * FUNCTION: calibrate_debugfs_write
 *
 * SUMMARY: Stores the calibration sense mode and a flag to control if the
 *  baseline will be initialized for the read method of this node.
 *
 * RETURN: Size of debugfs data write
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to write to
 *		count  - the maximum number of bytes to write
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t calibrate_debugfs_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	ssize_t length;
	u32 input_data[3];
	int rc = 0;

	rc = simple_write_to_buffer(data->pr_buf, sizeof(data->pr_buf), ppos,
			buf, count);
	if (rc < 0)
		return rc;

	count = rc;

	mutex_lock(&dad->sysfs_lock);

	length = cmd->parse_sysfs_input(dad->dev, data->pr_buf, count,
			input_data, ARRAY_SIZE(input_data));
	if (length != 2) {
		pt_debug(dad->dev, DL_ERROR,
				"%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	dad->calibrate_sensing_mode = input_data[0];
	dad->calibrate_initialize_baselines = input_data[1];

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return count;
}

PT_DEBUGFS_FOPS(calibrate, calibrate_debugfs_read, calibrate_debugfs_write);

/*******************************************************************************
 * FUNCTION: baseline_debugfs_read
 *
 * SUMMARY: Perform baseline initialization by sending the following PIP
 *  commands:
 *	   1) Suspend Scanning
 *	   2) Execute initialize baseline
 *	   3) Resume Scanning
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to read to
 *		count  - the maximum number of bytes to read
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t baseline_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	struct device *dev = dad->dev;
	int status = STATUS_FAIL;
	int length = 0;
	int rc;

	if (*ppos)
		goto exit;

	mutex_lock(&dad->sysfs_lock);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, PT_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR,
				"%s: Error on request exclusive rc = %d\n",
				__func__, rc);
		goto put_pm_runtime;
	}

	rc = pt_suspend_scan_cmd_(dev);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR, "%s: Error on suspend scan rc = %d\n",
				__func__, rc);
		goto release_exclusive;
	}

	rc = _pt_initialize_baselines_cmd(dev, dad->baseline_sensing_mode,
			&dad->ic_buf[0]);
	if (rc) {
		status = rc;
		pt_debug(dev, DL_ERROR,
				"%s: Error on initialize baselines rc = %d\n",
				__func__, rc);
		goto resume_scan;
	}

	length = 1;

	status = STATUS_SUCCESS;

resume_scan:
	pt_resume_scan_cmd_(dev);

release_exclusive:
	cmd->release_exclusive(dev);

put_pm_runtime:
	pm_runtime_put(dev);

	if (status == STATUS_FAIL)
		length = 0;

	data->pr_buf_len = prepare_print_buffer(status, dad->ic_buf, length,
		data->pr_buf, sizeof(data->pr_buf), PT_PR_FORMAT_DEFAULT);

	mutex_unlock(&dad->sysfs_lock);

exit:
	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

/*******************************************************************************
 * FUNCTION: baseline_debugfs_write
 *
 * SUMMARY: Store the sense mode of base initialization, the read method will
 *  perform.
 *
 * RETURN: Size of debugfs data write
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to write to
 *		count  - the maximum number of bytes to write
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t baseline_debugfs_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	ssize_t length;
	u32 input_data[2];
	int rc = 0;

	rc = simple_write_to_buffer(data->pr_buf, sizeof(data->pr_buf), ppos,
			buf, count);
	if (rc < 0)
		return rc;

	count = rc;

	mutex_lock(&dad->sysfs_lock);

	length = cmd->parse_sysfs_input(dad->dev, data->pr_buf, count,
		input_data, ARRAY_SIZE(input_data));
	if (length != 1) {
		pt_debug(dad->dev, DL_ERROR,
				"%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

	dad->baseline_sensing_mode = input_data[0];

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return count;
}

PT_DEBUGFS_FOPS(baseline, baseline_debugfs_read, baseline_debugfs_write);

/*******************************************************************************
 * FUNCTION: auto_shorts_debugfs_read
 *
 * SUMMARY: Performs the "auto shorts" test and prints the result to the ouput
 *  buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	*filp   - file pointer to debugfs file
 *	*buf    - the user space buffer to read to
 *	 count  - the maximum number of bytes to read
 *	*ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t auto_shorts_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;

	if (!*ppos)
		/* Set length to PIP_CMD_MAX_LENGTH to read all */
		data->pr_buf_len = pt_run_and_get_selftest_result(
			data->dad->dev, PT_CORE_CMD_PROTECTED,
			data->pr_buf, sizeof(data->pr_buf),
			PT_ST_ID_AUTOSHORTS, PIP_CMD_MAX_LENGTH,
			PT_ST_DONT_GET_RESULTS, PT_ST_PRINT_RESULTS,
			PT_PR_FORMAT_DEFAULT);

	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

PT_DEBUGFS_FOPS(auto_shorts, auto_shorts_debugfs_read, NULL);

/*******************************************************************************
 * FUNCTION: opens_debugfs_read
 *
 * SUMMARY: Performs the "opens" test and prints the results to the output
 *  buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to read to
 *		count  - the maximum number of bytes to read
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t opens_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;

	if (!*ppos)
		/* Set length to PIP_CMD_MAX_LENGTH to read all */
		data->pr_buf_len = pt_run_and_get_selftest_result(
			data->dad->dev, PT_CORE_CMD_PROTECTED,
			data->pr_buf, sizeof(data->pr_buf),
			PT_ST_ID_OPENS, PIP_CMD_MAX_LENGTH,
			PT_ST_DONT_GET_RESULTS, PT_ST_PRINT_RESULTS,
			PT_PR_FORMAT_DEFAULT);

	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

PT_DEBUGFS_FOPS(opens, opens_debugfs_read, NULL);

/*******************************************************************************
 * FUNCTION: cm_panel_debugfs_read
 *
 * SUMMARY: Performs the "CM panel" test and prints the result to the ouput
 *  buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to read to
 *		count  - the maximum number of bytes to read
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t cm_panel_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;

	if (!*ppos)
		/* Set length to PIP_CMD_MAX_LENGTH to read all */
		data->pr_buf_len = pt_run_and_get_selftest_result(
			data->dad->dev, PT_CORE_CMD_PROTECTED,
			data->pr_buf, sizeof(data->pr_buf),
			PT_ST_ID_CM_PANEL, PIP_CMD_MAX_LENGTH,
			PT_ST_GET_RESULTS, PT_ST_PRINT_RESULTS,
			PT_PR_FORMAT_DEFAULT);

	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

PT_DEBUGFS_FOPS(cm_panel, cm_panel_debugfs_read, NULL);

/*******************************************************************************
 * FUNCTION: cp_panel_debugfs_read
 *
 * SUMMARY: Performs the "CP panel" test and prints the result to the output
 *  buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to read to
 *		count  - the maximum number of bytes to read
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t cp_panel_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;

	if (!*ppos)
		/* Set length to PIP_CMD_MAX_LENGTH to read all */
		data->pr_buf_len = pt_run_and_get_selftest_result(
			data->dad->dev, PT_CORE_CMD_PROTECTED,
			data->pr_buf, sizeof(data->pr_buf),
			PT_ST_ID_CP_PANEL, PIP_CMD_MAX_LENGTH,
			PT_ST_GET_RESULTS, PT_ST_PRINT_RESULTS,
			PT_PR_FORMAT_DEFAULT);

	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

PT_DEBUGFS_FOPS(cp_panel, cp_panel_debugfs_read, NULL);

/*******************************************************************************
 * FUNCTION: cm_button_debugfs_read
 *
 * SUMMARY: Performs the "CM buttons" test and prints the result to the output
 *  buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to read to
 *		count  - the maximum number of bytes to read
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t cm_button_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;

	if (!*ppos)
		/* Set length to PIP_CMD_MAX_LENGTH to read all */
		data->pr_buf_len = pt_run_and_get_selftest_result(
			data->dad->dev, PT_CORE_CMD_PROTECTED,
			data->pr_buf, sizeof(data->pr_buf),
			PT_ST_ID_CM_BUTTON, PIP_CMD_MAX_LENGTH,
			PT_ST_GET_RESULTS, PT_ST_PRINT_RESULTS,
			PT_PR_FORMAT_DEFAULT);

	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

PT_DEBUGFS_FOPS(cm_button, cm_button_debugfs_read, NULL);

/*******************************************************************************
 * FUNCTION: cp_button_debugfs_read
 *
 * SUMMARY: Performs the "CP buttons" test and prints the result to the output
 *  buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to read to
 *		count  - the maximum number of bytes to read
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t cp_button_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;

	if (!*ppos)
		/* Set length to PIP_CMD_MAX_LENGTH to read all */
		data->pr_buf_len = pt_run_and_get_selftest_result(
			data->dad->dev, PT_CORE_CMD_PROTECTED,
			data->pr_buf, sizeof(data->pr_buf),
			PT_ST_ID_CP_BUTTON, PIP_CMD_MAX_LENGTH,
			PT_ST_GET_RESULTS, PT_ST_PRINT_RESULTS,
			PT_PR_FORMAT_DEFAULT);

	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

PT_DEBUGFS_FOPS(cp_button, cp_button_debugfs_read, NULL);

/*******************************************************************************
 * FUNCTION: fw_self_test_debugfs_read
 *
 * SUMMARY: Performs the self test by firmware and prints the results to the
 *  output buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	*filp   - file pointer to debugfs file
 *	*buf    - the user space buffer to read to
 *	 count  - the maximum number of bytes to read
 *	*ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t fw_self_test_debugfs_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	u8 ret_status;
	u8 ret_self_test_id;
	u8 sys_mode = FW_SYS_MODE_UNDEFINED;
	u16 ret_act_load_len;
	int rc;
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;

	if (!*ppos) {
		if (dad->fw_self_test_id == PT_ST_ID_INVALID ||
			dad->fw_self_test_format >= PT_PR_FORMAT_UNDEFINE) {
			data->pr_buf_len = scnprintf(data->pr_buf,
				sizeof(data->pr_buf), "Status: %d\n", -EINVAL);
			pt_debug(dad->dev, DL_ERROR,
				"%s: Malformed input\n", __func__);
			goto exit;
		}

		/* only send the load parameters cmd if param data exists */
		if (dad->fw_self_test_param_len > 0) {
			rc = cmd->request_get_fw_mode(dad->dev,
				PT_CORE_CMD_UNPROTECTED, &sys_mode, NULL);
			if (rc) {
				pt_debug(dad->dev, DL_ERROR,
					"%s: ERR on request mode rc=%d\n",
					__func__, rc);
				data->pr_buf_len = scnprintf(
					data->pr_buf,
					sizeof(data->pr_buf),
					"Status: %d\n", rc);
				goto exit;
			}

			if (sys_mode != FW_SYS_MODE_TEST) {
				rc = pt_suspend_scan_cmd_(dad->dev);
				if (rc) {
					pt_debug(dad->dev, DL_ERROR,
						"%s: ERR on sus scan rc=%d\n",
						__func__, rc);
					data->pr_buf_len = scnprintf(
						data->pr_buf,
						sizeof(data->pr_buf),
						"Status: %d\n", rc);
					goto exit;
				}
			}
			cmd->nonhid_cmd->load_self_test_param(dad->dev,
				PT_CORE_CMD_PROTECTED,
				dad->fw_self_test_id, 0,
				dad->fw_self_test_param_len,
				dad->fw_self_test_param, &ret_status,
				&ret_self_test_id, &ret_act_load_len);
			if (ret_status) {
				data->pr_buf_len = scnprintf(data->pr_buf,
					sizeof(data->pr_buf),
					"Status: %d\n", -EINVAL);
				pt_debug(dad->dev, DL_ERROR,
					"%s: Load Param Malformed input\n",
					__func__);
				goto resume_scan;
			}
		}

		/* Set length to PIP_CMD_MAX_LENGTH to read all */
		data->pr_buf_len = pt_run_and_get_selftest_result(
			dad->dev, PT_CORE_CMD_PROTECTED,
			data->pr_buf, sizeof(data->pr_buf),
			dad->fw_self_test_id, PIP_CMD_MAX_LENGTH,
			PT_ST_GET_RESULTS, PT_ST_PRINT_RESULTS,
			dad->fw_self_test_format);

		/* Clear the parameters so next test won't use them */
		if (dad->fw_self_test_param_len > 0) {
			cmd->nonhid_cmd->load_self_test_param(dad->dev,
				PT_CORE_CMD_PROTECTED,
				dad->fw_self_test_id, 0, 0, NULL,
				&ret_status, &ret_self_test_id,
				&ret_act_load_len);
		}

		dad->fw_self_test_id = PT_ST_ID_INVALID;
		dad->fw_self_test_format = PT_PR_FORMAT_UNDEFINE;
		dad->fw_self_test_param_len = 0;

resume_scan:
		/* Only resume scanning if we suspended it */
		if (sys_mode == FW_SYS_MODE_SCANNING)
			pt_resume_scan_cmd_(dad->dev);
	}

exit:
	return simple_read_from_buffer(buf, count, ppos, data->pr_buf,
			data->pr_buf_len);
}

/*******************************************************************************
 * FUNCTION: fw_self_test_debugfs_write
 *
 * SUMMARY: Store the self test ID and ouput format, the read method will
 *  perform.
 *
 * RETURN: Size of debugfs data write
 *
 * PARAMETERS:
 *	*filp   - file pointer to debugfs file
 *	*buf    - the user space buffer to write to
 *	 count  - the maximum number of bytes to write
 *	*ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t fw_self_test_debugfs_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_debugfs_data *data = filp->private_data;
	struct pt_device_access_data *dad = data->dad;
	ssize_t length;
	u32 input_data[PT_FW_SELF_TEST_MAX_PARM + 1];
	int rc = 0;
	int i;

	rc = simple_write_to_buffer(data->pr_buf, sizeof(data->pr_buf), ppos,
			buf, count);
	if (rc < 0)
		return rc;

	count = rc;

	mutex_lock(&dad->sysfs_lock);

	length = cmd->parse_sysfs_input(dad->dev, data->pr_buf, count,
		input_data, PT_FW_SELF_TEST_MAX_PARM + 1);
	if (length == 1) {
		dad->fw_self_test_id = input_data[0];
		dad->fw_self_test_format = PT_PR_FORMAT_DEFAULT;
		dad->fw_self_test_param_len = 0;
	} else if (length == 2) {
		dad->fw_self_test_id = input_data[0];
		dad->fw_self_test_format = input_data[1];
		dad->fw_self_test_param_len = 0;
	} else if (length > 2 && (length <= PT_FW_SELF_TEST_MAX_PARM)) {
		dad->fw_self_test_id = input_data[0];
		dad->fw_self_test_format = input_data[1];
		dad->fw_self_test_param_len = length - 2;
		pt_debug(dad->dev, DL_INFO,
			"%s: test_id=%d, format=%d, param_len=%d",
			__func__, dad->fw_self_test_id,
			dad->fw_self_test_format, dad->fw_self_test_param_len);
		for (i = 0; i < dad->fw_self_test_param_len; i++)
			dad->fw_self_test_param[i] = input_data[i + 2];
	} else {
		pt_debug(dad->dev, DL_ERROR,
				"%s: Malformed input\n", __func__);
		rc = -EINVAL;
		goto exit_unlock;
	}

exit_unlock:
	mutex_unlock(&dad->sysfs_lock);

	if (rc)
		return rc;

	return count;
}

PT_DEBUGFS_FOPS(fw_self_test,
	fw_self_test_debugfs_read, fw_self_test_debugfs_write);

#ifdef TTHE_TUNER_SUPPORT
/*******************************************************************************
 * FUNCTION: tthe_get_panel_data_debugfs_read
 *
 * SUMMARY: Performs a panel scan and prints the panel data into the output
 *  buffer.
 *
 * RETURN: Size of debugfs data print
 *
 * PARAMETERS:
 *	   *filp   - file pointer to debugfs file
 *	   *buf    - the user space buffer to read to
 *		count  - the maximum number of bytes to read
 *	   *ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t tthe_get_panel_data_debugfs_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_data *dad = filp->private_data;
	struct device *dev;
	struct pt_core_data *cd;
	u8 config;
	u16 actual_read_len;
	u16 length = 0;
	u8 element_size = 0;
	u8 *buf_offset;
	u8 *buf_out;
	int elem;
	int elem_offset = 0;
	int print_idx = 0;
	int rc;
	int rc1;
	int i;

	mutex_lock(&dad->debugfs_lock);
	dev = dad->dev;
	cd = dev_get_drvdata(dev);
	buf_out = dad->panel_scan_data_buf;
	if (!buf_out)
		goto release_mutex;

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, PT_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc)
		goto put_runtime;

	if (dad->heatmap.scan_start) {
		/*
		 * To fix CDT206291: avoid multiple scans when
		 * return data is larger than 4096 bytes in one cycle
		 */
		dad->heatmap.scan_start = 0;

		/* Start scan */
		rc = pt_exec_scan_cmd_(dev, 0);
		if (rc)
			goto release_exclusive;
	}

	elem = dad->heatmap.num_element;

#if defined(PT_ENABLE_MAX_ELEN)
	if (elem > PT_MAX_ELEN) {
		rc = pt_ret_scan_data_cmd_(dev, elem_offset,
		PT_MAX_ELEN, dad->heatmap.data_type, dad->ic_buf,
		&config, &actual_read_len, NULL);
	} else{
		rc = pt_ret_scan_data_cmd_(dev, elem_offset, elem,
			dad->heatmap.data_type, dad->ic_buf, &config,
			&actual_read_len, NULL);
	}
#else
	rc = pt_ret_scan_data_cmd_(dev, elem_offset, elem,
			dad->heatmap.data_type, dad->ic_buf, &config,
			&actual_read_len, NULL);
#endif
	if (rc)
		goto release_exclusive;

	length = get_unaligned_le16(&dad->ic_buf[0]);
	buf_offset = dad->ic_buf + length;

	element_size = config & PT_CMD_RET_PANEL_ELMNT_SZ_MASK;

	elem -= actual_read_len;
	elem_offset = actual_read_len;
	while (elem > 0) {
#ifdef PT_ENABLE_MAX_ELEN
		if (elem > PT_MAX_ELEN) {
			rc = pt_ret_scan_data_cmd_(dev, elem_offset,
			PT_MAX_ELEN, dad->heatmap.data_type, NULL, &config,
			&actual_read_len, buf_offset);
		} else{
			rc = pt_ret_scan_data_cmd_(dev, elem_offset, elem,
				dad->heatmap.data_type, NULL, &config,
				&actual_read_len, buf_offset);
		}
#else

		rc = pt_ret_scan_data_cmd_(dev, elem_offset, elem,
				dad->heatmap.data_type, NULL, &config,
				&actual_read_len, buf_offset);
#endif
		if (rc)
			goto release_exclusive;

		if (!actual_read_len)
			break;

		length += actual_read_len * element_size;
		buf_offset = dad->ic_buf + length;
		elem -= actual_read_len;
		elem_offset += actual_read_len;
	}

	/* Reconstruct cmd header */
	put_unaligned_le16(length, &dad->ic_buf[0]);
	put_unaligned_le16(elem_offset, &dad->ic_buf[7]);

release_exclusive:
	rc1 = cmd->release_exclusive(dev);
put_runtime:
	pm_runtime_put(dev);

	if (rc)
		goto release_mutex;
	if (cd->show_timestamp)
		print_idx += scnprintf(buf_out, TTHE_TUNER_MAX_BUF,
			"[%u] PT_DATA:", pt_get_time_stamp());
	else
		print_idx += scnprintf(buf_out, TTHE_TUNER_MAX_BUF,
			"PT_DATA:");
	for (i = 0; i < length; i++)
		print_idx += scnprintf(buf_out + print_idx,
				TTHE_TUNER_MAX_BUF - print_idx,
				"%02X ", dad->ic_buf[i]);
	print_idx += scnprintf(buf_out + print_idx,
			TTHE_TUNER_MAX_BUF - print_idx,
			":(%d bytes)\n", length);
	rc = simple_read_from_buffer(buf, count, ppos, buf_out, print_idx);
	print_idx = rc;

release_mutex:
	mutex_unlock(&dad->debugfs_lock);
	return print_idx;
}

/*******************************************************************************
 * FUNCTION: tthe_get_panel_data_debugfs_write
 *
 * SUMMARY: Store the panel data type to retrieve and size of panel data, the
 *  read method will perform.
 *
 * RETURN: Size of debugfs data write
 *
 * PARAMETERS:
 *	*filp   - file pointer to debugfs file
 *	*buf    - the user space buffer to write to
 *	 count  - the maximum number of bytes to write
 *	*ppos   - the current position in the buffer
 ******************************************************************************/
static ssize_t tthe_get_panel_data_debugfs_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct pt_device_access_data *dad = filp->private_data;
	struct device *dev = dad->dev;
	ssize_t length;
	int max_read;
	u32 input_data[8];
	u8 *buf_in = dad->panel_scan_data_buf;
	int ret;

	mutex_lock(&dad->debugfs_lock);
	ret = copy_from_user(buf_in + (*ppos), buf, count);
	if (ret)
		goto exit;
	buf_in[count] = 0;

	length = cmd->parse_sysfs_input(dev, buf_in, count, input_data,
			ARRAY_SIZE(input_data));
	if (length <= 0) {
		pt_debug(dev, DL_ERROR,
				"%s: %s Group Data store\n",
				__func__, "Malformed input for");
		goto exit;
	}

	/* update parameter value */
	dad->heatmap.num_element = input_data[3] + (input_data[4] << 8);
	dad->heatmap.data_type = input_data[5];

	if (input_data[6] > 0)
		dad->heatmap.scan_start = true;
	else
		dad->heatmap.scan_start = false;

	/* elem can not be bigger then buffer size */
	max_read = PT_CMD_RET_PANEL_HDR;
	max_read += dad->heatmap.num_element * PT_CMD_RET_PANEL_ELMNT_SZ_MAX;

	if (max_read >= PT_MAX_PRBUF_SIZE) {
		dad->heatmap.num_element =
			(PT_MAX_PRBUF_SIZE - PT_CMD_RET_PANEL_HDR)
			/ PT_CMD_RET_PANEL_ELMNT_SZ_MAX;
		pt_debug(dev, DL_INFO, "%s: Will get %d element\n",
			__func__, dad->heatmap.num_element);
	}

exit:
	mutex_unlock(&dad->debugfs_lock);
	pt_debug(dev, DL_INFO, "%s: return count=%zu\n",
		__func__, count);
	return count;
}

/*******************************************************************************
 * FUNCTION: tthe_get_panel_data_debugfs_open
 *
 * SUMMARY: Open the get_panel_data debugfs node to initialize.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *  *inode - pointer to inode structure
 *  *filp  - pointer to file structure
 ******************************************************************************/
static int tthe_get_panel_data_debugfs_open(struct inode *inode,
		struct file *filp)
{
	struct pt_device_access_data *dad = inode->i_private;

	mutex_lock(&dad->debugfs_lock);

	if (dad->tthe_get_panel_data_is_open) {
		mutex_unlock(&dad->debugfs_lock);
		return -EBUSY;
	}

	filp->private_data = inode->i_private;

	dad->tthe_get_panel_data_is_open = 1;
	mutex_unlock(&dad->debugfs_lock);
	return 0;
}

/*******************************************************************************
 * FUNCTION: tthe_get_panel_data_debugfs_close
 *
 * SUMMARY: Close the get_panel_data debugfs node to free pointer.
 *
 * RETURN:
 *  0 = success
 * !0 = failure
 *
 * PARAMETERS:
 *	*inode - pointer to inode structure
 *	*filp  - pointer to file structure
 ******************************************************************************/
static int tthe_get_panel_data_debugfs_close(struct inode *inode,
		struct file *filp)
{
	struct pt_device_access_data *dad = filp->private_data;

	mutex_lock(&dad->debugfs_lock);
	filp->private_data = NULL;
	dad->tthe_get_panel_data_is_open = 0;
	mutex_unlock(&dad->debugfs_lock);

	return 0;
}

static const struct file_operations tthe_get_panel_data_fops = {
	.open = tthe_get_panel_data_debugfs_open,
	.release = tthe_get_panel_data_debugfs_close,
	.read = tthe_get_panel_data_debugfs_read,
	.write = tthe_get_panel_data_debugfs_write,
};
#endif

/*******************************************************************************
 * FUNCTION: pt_setup_sysfs
 *
 * SUMMARY: Creates all device test dependent sysfs nodes owned by the
 *  device access file.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 ******************************************************************************/
static int pt_setup_sysfs(struct device *dev)
{
	struct pt_device_access_data *dad
		= pt_get_device_access_data(dev);
	int rc = 0;

	pt_debug(dev, DL_INFO, "Entering %s\n", __func__);

	dad->base_dentry = debugfs_create_dir(dev_name(dev), NULL);
	if (IS_ERR_OR_NULL(dad->base_dentry)) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create base directory\n",
			__func__);
		goto exit;
	}

	dad->mfg_test_dentry = debugfs_create_dir("mfg_test",
			dad->base_dentry);
	if (IS_ERR_OR_NULL(dad->mfg_test_dentry)) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create mfg_test directory\n",
			__func__);
		goto unregister_base_dir;
	}

	dad->panel_scan_debugfs = debugfs_create_file(
		"panel_scan", 0644, dad->mfg_test_dentry, dad,
		&panel_scan_fops);
	if (IS_ERR_OR_NULL(dad->panel_scan_debugfs)) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create panel_scan\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("get_idac", 0600,
			dad->mfg_test_dentry, dad, &get_idac_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create get_idac\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("auto_shorts", 0400,
			dad->mfg_test_dentry, dad,
			&auto_shorts_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create auto_shorts\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("opens", 0400,
			dad->mfg_test_dentry, dad, &opens_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create opens\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("calibrate_ext",
			0600, dad->mfg_test_dentry,
			dad, &calibrate_ext_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create calibrate_ext\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("calibrate", 0600,
			dad->mfg_test_dentry, dad, &calibrate_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create calibrate\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("baseline", 0600,
			dad->mfg_test_dentry, dad, &baseline_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create baseline\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("cm_panel", 0400,
			dad->mfg_test_dentry, dad, &cm_panel_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create cm_panel\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("cp_panel", 0400,
			dad->mfg_test_dentry, dad, &cp_panel_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create cp_panel\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("cm_button", 0400,
			dad->mfg_test_dentry, dad, &cm_button_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create cm_button\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("cp_button", 0400,
			dad->mfg_test_dentry, dad, &cp_button_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create cp_button\n",
			__func__);
		goto unregister_base_dir;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("fw_self_test", 0600,
		dad->mfg_test_dentry, dad, &fw_self_test_debugfs_fops))) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create fw_self_test\n",
			__func__);
		goto unregister_base_dir;
	}

	dad->cmcp_results_debugfs = debugfs_create_file("cmcp_results", 0644,
		dad->mfg_test_dentry, dad, &cmcp_results_debugfs_fops);
	if (IS_ERR_OR_NULL(dad->cmcp_results_debugfs)) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create cmcp_results\n",
			__func__);
		dad->cmcp_results_debugfs = NULL;
		goto unregister_base_dir;
	}

#ifdef TTHE_TUNER_SUPPORT
	dad->tthe_get_panel_data_debugfs = debugfs_create_file(
			PT_TTHE_TUNER_GET_PANEL_DATA_FILE_NAME,
			0644, NULL, dad, &tthe_get_panel_data_fops);
	if (IS_ERR_OR_NULL(dad->tthe_get_panel_data_debugfs)) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create get_panel_data\n",
			__func__);
		dad->tthe_get_panel_data_debugfs = NULL;
		goto unregister_base_dir;
	}
#endif

	rc = device_create_file(dev, &dev_attr_cmcp_test);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create cmcp_test\n",
			__func__);
		goto unregister_base_dir;
	}

	rc = device_create_file(dev, &dev_attr_cmcp_threshold_loading);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create cmcp_thresold_loading\n",
			__func__);
		goto unregister_cmcp_test;
	}

	rc = device_create_bin_file(dev, &bin_attr_cmcp_threshold_data);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, could not create cmcp_thresold_data\n",
			__func__);
		goto unregister_cmcp_thresold_loading;
	}

	dad->sysfs_nodes_created = true;
	return rc;

unregister_cmcp_thresold_loading:
	device_remove_file(dev, &dev_attr_cmcp_threshold_loading);
unregister_cmcp_test:
	device_remove_file(dev, &dev_attr_cmcp_test);
unregister_base_dir:
	debugfs_remove_recursive(dad->base_dentry);
exit:
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_setup_cmcp_attention
 *
 * SUMMARY: Funtion to be registered to TTDL attention list to setup sysfs and
 *  parse threshold file for device test.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 ******************************************************************************/
static int pt_setup_cmcp_attention(struct device *dev)
{
	struct pt_device_access_data *dad
		= pt_get_device_access_data(dev);
	int rc = 0;

	dad->si = cmd->request_sysinfo(dev);
	if (!dad->si)
		return -EINVAL;

	rc = pt_setup_sysfs(dev);
	schedule_work(&dad->cmcp_threshold_update);

	cmd->unsubscribe_attention(dev, PT_ATTEN_STARTUP,
		PT_DEVICE_ACCESS_NAME, pt_setup_cmcp_attention,
		0);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_cmcp_parse_threshold_file
 *
 * SUMMARY: Parse cmcp threshold file and store it to the structure.
 *
 * PARAMETERS:
 *	*fw       - pointer to firmware structure
 *	*context  - expected read length of the response
 ******************************************************************************/
static void pt_cmcp_parse_threshold_file(const struct firmware *fw,
		void *context)
{
	struct device *dev = context;
	struct pt_device_access_data *dad =
		pt_get_device_access_data(dev);

	if (!fw) {
		pt_debug(dev, DL_WARN,
				"%s: No builtin cmcp threshold file\n",
				__func__);
		goto exit;
	}

	if (!fw->data || !fw->size) {
		pt_debug(dev, DL_ERROR,
				"%s: Invalid builtin cmcp threshold file\n",
		__func__);
		goto exit;
	}

	pt_debug(dev, DL_WARN, "%s: Found cmcp threshold file.\n",
		__func__);

	pt_parse_cmcp_threshold_file_common(dev, &fw->data[0], fw->size);

	dad->builtin_cmcp_threshold_status = 0;
	release_firmware(fw);
	return;

exit:
	if (fw)
		release_firmware(fw);

	dad->builtin_cmcp_threshold_status = -EINVAL;
}

/*******************************************************************************
 * FUNCTION: pt_device_access_user_command
 *
 * SUMMARY: Wrapper function to call pt_cmcp_parse_threshold_file() by firmware
 *  class function.
 *
 * PARAMETERS:
 *	*cmcp_threshold_update - pointer to work_struct structure
 ******************************************************************************/
static void pt_parse_cmcp_threshold_builtin(
	struct work_struct *cmcp_threshold_update)
{
	struct pt_device_access_data *dad =
		container_of(cmcp_threshold_update,
		struct pt_device_access_data,
		cmcp_threshold_update);
	struct device *dev = dad->dev;
	struct pt_core_data *cd = dev_get_drvdata(dev);
	const struct firmware *fw_entry = NULL;
	int retval;

	dad->si = cmd->request_sysinfo(dev);
	if (!dad->si) {
		pt_debug(dev, DL_ERROR,
			"%s: Fail get sysinfo pointer from core\n",
			__func__);
		return;
	}

	pt_debug(dev, DL_INFO,
		"%s: Enabling cmcp threshold class loader built-in\n",
		__func__);

	/* Open threshold file */
	mutex_lock(&cd->firmware_class_lock);
#if (KERNEL_VERSION(3, 13, 0) > LINUX_VERSION_CODE)
	retval = request_firmware(&fw_entry, CMCP_THRESHOLD_FILE_NAME, dev);
#else
	retval = request_firmware_direct(&fw_entry,
				CMCP_THRESHOLD_FILE_NAME, dev);
#endif
	if (retval < 0) {
		pt_debug(dev, DL_WARN,
			"%s: Failed loading cmcp threshold file, attempting legacy file\n",
			__func__);
		/* Try legacy file name */
#if (KERNEL_VERSION(3, 13, 0) > LINUX_VERSION_CODE)
	retval = request_firmware(&fw_entry,
				PT_CMCP_THRESHOLD_FILE_NAME, dev);
#else
	retval = request_firmware_direct(&fw_entry,
				PT_CMCP_THRESHOLD_FILE_NAME, dev);
#endif
		if (retval < 0) {
			mutex_unlock(&cd->firmware_class_lock);
			dad->builtin_cmcp_threshold_status = -EINVAL;
			pt_debug(dev, DL_WARN,
				"%s: Fail request cmcp threshold class file load\n",
				__func__);
			goto exit;
		}
	}
	pt_cmcp_parse_threshold_file(fw_entry, dev);

	mutex_unlock(&cd->firmware_class_lock);

exit:
	return;
}

/*******************************************************************************
 * FUNCTION: pt_device_access_probe
 *
 * SUMMARY: The probe function for the device access
 *
 * PARAMETERS:
 *	*dev   - pointer to device structure
 *	**data - double pointer to pt_device_access_data data to be created here
 ******************************************************************************/
static int pt_device_access_probe(struct device *dev, void **data)
{
	struct pt_device_access_data *dad;
	struct configuration *configurations;
	struct cmcp_data *cmcp_info;
	struct result *result;

	int tx_num = MAX_TX_SENSORS;
	int rx_num = MAX_RX_SENSORS;
	int btn_num = MAX_BUTTONS;

	struct test_case_field *test_case_field_array;
	struct test_case_search *test_case_search_array;
	int rc = 0;

	dad = kzalloc(sizeof(*dad), GFP_KERNEL);
	if (!dad) {
		rc = -ENOMEM;
		goto pt_device_access_probe_data_failed;
	}

	configurations =
		kzalloc(sizeof(*configurations), GFP_KERNEL);
	if (!configurations) {
		rc = -ENOMEM;
		goto pt_device_access_probe_configs_failed;
	}
	dad->configs = configurations;

	cmcp_info = kzalloc(sizeof(*cmcp_info), GFP_KERNEL);
	if (!cmcp_info) {
		rc = -ENOMEM;
		goto pt_device_access_probe_cmcp_info_failed;
	}
	dad->cmcp_info = cmcp_info;

	cmcp_info->tx_num = tx_num;
	cmcp_info->rx_num = rx_num;
	cmcp_info->btn_num = btn_num;

	result = kzalloc(sizeof(*result), GFP_KERNEL);
	if (!result) {
		rc = -ENOMEM;
		goto pt_device_access_probe_result_failed;
	}
	dad->result = result;

	test_case_field_array =
		kzalloc(sizeof(*test_case_field_array) * MAX_CASE_NUM,
		GFP_KERNEL);
	if (!test_case_field_array) {
		rc = -ENOMEM;
		goto pt_device_access_probe_field_array_failed;
	}

	test_case_search_array =
		kzalloc(sizeof(*test_case_search_array) * MAX_CASE_NUM,
		GFP_KERNEL);
	if (!test_case_search_array) {
		rc = -ENOMEM;
		goto pt_device_access_probe_search_array_failed;
	}

	cmcp_info->gd_sensor_col = (struct gd_sensor *)
		kzalloc(tx_num * sizeof(struct gd_sensor), GFP_KERNEL);
	if (cmcp_info->gd_sensor_col == NULL)
		goto pt_device_access_probe_gd_sensor_col_failed;

	cmcp_info->gd_sensor_row = (struct gd_sensor *)
		kzalloc(rx_num * sizeof(struct gd_sensor), GFP_KERNEL);
	if (cmcp_info->gd_sensor_row == NULL)
		goto pt_device_access_probe_gd_sensor_row_failed;

	cmcp_info->cm_data_panel =
		kzalloc((tx_num * rx_num + 1) * sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cm_data_panel == NULL)
		goto pt_device_access_probe_cm_data_panel_failed;

	cmcp_info->cp_tx_data_panel =
		kzalloc(tx_num * sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cp_tx_data_panel == NULL)
		goto pt_device_access_probe_cp_tx_data_panel_failed;

	cmcp_info->cp_tx_cal_data_panel =
		kzalloc(tx_num * sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cp_tx_cal_data_panel == NULL)
		goto pt_device_access_probe_cp_tx_cal_data_panel_failed;

	cmcp_info->cp_rx_data_panel =
		kzalloc(rx_num * sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cp_rx_data_panel == NULL)
		goto pt_device_access_probe_cp_rx_data_panel_failed;

	cmcp_info->cp_rx_cal_data_panel =
		kzalloc(rx_num * sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cp_rx_cal_data_panel == NULL)
		goto pt_device_access_probe_cp_rx_cal_data_panel_failed;

	cmcp_info->cm_btn_data = kcalloc(btn_num, sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cm_btn_data == NULL)
		goto pt_device_access_probe_cm_btn_data_failed;

	cmcp_info->cp_btn_data = kcalloc(btn_num, sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cp_btn_data == NULL)
		goto pt_device_access_probe_cp_btn_data_failed;

	cmcp_info->cm_sensor_column_delta =
		 kzalloc(rx_num * tx_num * sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cm_sensor_column_delta == NULL)
		goto pt_device_access_probe_cm_sensor_column_delta_failed;

	cmcp_info->cm_sensor_row_delta =
		 kzalloc(tx_num * rx_num  * sizeof(int32_t), GFP_KERNEL);
	if (cmcp_info->cm_sensor_row_delta == NULL)
		goto pt_device_access_probe_cm_sensor_row_delta_failed;

	mutex_init(&dad->sysfs_lock);
	mutex_init(&dad->cmcp_threshold_lock);
	dad->dev = dev;
#ifdef TTHE_TUNER_SUPPORT
	mutex_init(&dad->debugfs_lock);
	dad->heatmap.num_element = 200;
#endif
	*data = dad;

	dad->test_field_array = test_case_field_array;
	dad->test_search_array = test_case_search_array;
	dad->test_executed = 0;

	dad->cal_ext_data.mode = PT_CAL_EXT_MODE_UNDEFINED;
	dad->panel_scan_retrieve_id = 0;
	dad->panel_scan_type_id = 0;

	INIT_WORK(&dad->cmcp_threshold_update, pt_parse_cmcp_threshold_builtin);

	/* get sysinfo */
	dad->si = cmd->request_sysinfo(dev);
	if (dad->si) {
		rc = pt_setup_sysfs(dev);
		if (rc)
			goto pt_device_access_setup_sysfs_failed;
	} else {
		pt_debug(dev, DL_ERROR,
				"%s: Fail get sysinfo pointer from core p=%p\n",
				__func__, dad->si);
		cmd->subscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_DEVICE_ACCESS_NAME,
			pt_setup_cmcp_attention, 0);
	}

	schedule_work(&dad->cmcp_threshold_update);

	return 0;

pt_device_access_setup_sysfs_failed:
	kfree(cmcp_info->cm_sensor_row_delta);
pt_device_access_probe_cm_sensor_row_delta_failed:
	kfree(cmcp_info->cm_sensor_column_delta);
pt_device_access_probe_cm_sensor_column_delta_failed:
	kfree(cmcp_info->cp_btn_data);
pt_device_access_probe_cp_btn_data_failed:
	kfree(cmcp_info->cm_btn_data);
pt_device_access_probe_cm_btn_data_failed:
	kfree(cmcp_info->cp_rx_cal_data_panel);
pt_device_access_probe_cp_rx_cal_data_panel_failed:
	kfree(cmcp_info->cp_rx_data_panel);
pt_device_access_probe_cp_rx_data_panel_failed:
	kfree(cmcp_info->cp_tx_cal_data_panel);
pt_device_access_probe_cp_tx_cal_data_panel_failed:
	kfree(cmcp_info->cp_tx_data_panel);
pt_device_access_probe_cp_tx_data_panel_failed:
	kfree(cmcp_info->cm_data_panel);
pt_device_access_probe_cm_data_panel_failed:
	kfree(cmcp_info->gd_sensor_row);
pt_device_access_probe_gd_sensor_row_failed:
	kfree(cmcp_info->gd_sensor_col);
pt_device_access_probe_gd_sensor_col_failed:
	kfree(test_case_search_array);
pt_device_access_probe_search_array_failed:
	kfree(test_case_field_array);
pt_device_access_probe_field_array_failed:
	kfree(result);
pt_device_access_probe_result_failed:
	kfree(cmcp_info);
pt_device_access_probe_cmcp_info_failed:
	kfree(configurations);
pt_device_access_probe_configs_failed:
	kfree(dad);
pt_device_access_probe_data_failed:
	pt_debug(dev, DL_ERROR, "%s failed.\n", __func__);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_device_access_release
 *
 * SUMMARY: Remove function for device_access module that does following
 *  cleanup:
 *	- Unsubscibe all registered attention tasks
 *	- Removes all created sysfs nodes
 *	- Frees all pointers
 *
 * PARAMETERS:
 *	*dev   - pointer to device structure
 *	*data  - pointer to the device_access data
 ******************************************************************************/
static void pt_device_access_release(struct device *dev, void *data)
{
	struct pt_device_access_data *dad = data;

	if (dad->sysfs_nodes_created) {
		debugfs_remove(dad->cmcp_results_debugfs);
		debugfs_remove_recursive(dad->base_dentry);
#ifdef TTHE_TUNER_SUPPORT
		debugfs_remove(dad->tthe_get_panel_data_debugfs);
#endif
		device_remove_file(dev, &dev_attr_cmcp_test);
		device_remove_file(dev, &dev_attr_cmcp_threshold_loading);
		device_remove_bin_file(dev, &bin_attr_cmcp_threshold_data);
		kfree(dad->cmcp_threshold_data);
	} else {
		cmd->unsubscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_DEVICE_ACCESS_NAME,
			pt_setup_cmcp_attention, 0);
	}

	kfree(dad->test_search_array);
	kfree(dad->test_field_array);
	kfree(dad->configs);
	pt_free_cmcp_buf(dad->cmcp_info);
	kfree(dad->cmcp_info);
	kfree(dad->result);
	kfree(dad);
}

static struct pt_module device_access_module = {
	.name = PT_DEVICE_ACCESS_NAME,
	.probe = pt_device_access_probe,
	.release = pt_device_access_release,
};

/*******************************************************************************
 * FUNCTION: pt_device_access_init
 *
 * SUMMARY: Initialize function for device access module which to register
 * device_access_module into TTDL module list.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 ******************************************************************************/
static int __init pt_device_access_init(void)
{
	int rc;

	cmd = pt_get_commands();
	if (!cmd)
		return -EINVAL;

	rc = pt_register_module(&device_access_module);
	if (rc) {
		pr_err("%s: Error, failed registering module\n",
			__func__);
			return rc;
	}

	pr_info("%s: Parade TTSP Device Access Driver (Built %s) rc = %d\n",
		 __func__, PT_DRIVER_VERSION, rc);
	return 0;
}
module_init(pt_device_access_init);

/*******************************************************************************
 * FUNCTION: pt_device_access_exit
 *
 * SUMMARY: Exit function for device access module which to unregister
 * device_access_module from TTDL module list.
 *
 ******************************************************************************/
static void __exit pt_device_access_exit(void)
{
	pt_unregister_module(&device_access_module);
}
module_exit(pt_device_access_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Parade TrueTouch(R) Standard Product Device Access Driver");
MODULE_AUTHOR("Parade Technologies <ttdrivers@paradetech.com>");
