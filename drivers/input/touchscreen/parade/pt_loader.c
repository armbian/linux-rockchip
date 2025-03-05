/*
 * pt_loader.c
 * Parade TrueTouch(TM) Standard Product FW Loader Module.
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

#define PT_LOADER_NAME "pt_loader"


#define PT_AUTO_LOAD_FOR_CORRUPTED_FW 1
#define PT_LOADER_FW_UPGRADE_RETRY_COUNT 3

/*
 *#define PT_FW_UPGRADE \
 *        (defined(CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE) \
 *        || defined(CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE))
 *
 *#define PT_TTCONFIG_UPGRADE \
 *        (defined(CONFIG_TOUCHSCREEN_PARADE_PLATFORM_TTCONFIG_UPGRADE) \
 *        || defined(CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE))
 *
 */
#define PT_FW_UPGRADE 0
#define PT_TTCONFIG_UPGRADE 0

/* Timeout values in ms. */
#define PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT		3000
#define PT_LDR_SWITCH_TO_APP_MODE_TIMEOUT		300

#define PT_MAX_STATUS_SIZE				32

#define PT_DATA_MAX_ROW_SIZE				256
#define PT_DATA_ROW_SIZE				128

#define PT_ARRAY_ID_OFFSET				0
#define PT_ROW_NUM_OFFSET				1
#define PT_ROW_SIZE_OFFSET				3
#define PT_ROW_DATA_OFFSET				5

#define PT_POST_TT_CFG_CRC_MASK				0x2
static inline struct pt_loader_data *pt_get_loader_data(
		struct device *dev);

static struct pt_core_commands *cmd;

#define PIP2_LAUNCH_APP_DELAY	400

struct pip2_file_read {
	s8 para_num;
	u8 file_handle;
	int file_offset;
	int file_max_size;
	int file_read_size;
	int file_print_size;
	u8 *file_print_buf;
	int file_print_left;
};

#ifdef TTDL_DIAGNOSTICS
struct pip2_file_crc {
	s8 para_num;
	u8 file_handle;
	int file_offset;
	int file_max_size;
	int file_read_size;
};
#endif

struct pip2_loader_data {
	struct device *dev;
	struct completion pip2_fw_upgrade_complete; /* mutex for loader */
	u8 pip2_file_handle;
};

struct pt_loader_data {
	struct device *dev;
	struct pt_sysinfo *si;
	u8 status_buf[PT_MAX_STATUS_SIZE];
	struct completion int_running;
	struct completion calibration_complete;
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	int builtin_bin_fw_status;
	bool is_manual_upgrade_enabled;
#endif
	struct work_struct fw_and_config_upgrade;
	struct work_struct calibration_work;
	struct pt_loader_platform_data *loader_pdata;
#ifdef CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE
	struct mutex config_lock;
	u8 *config_data;
	int config_size;
	bool config_loading;
#endif
	struct pip2_loader_data *pip2_data;
	u8 pip2_load_file_no;
	bool pip2_load_builtin;
	u8 pip2_file_erase_file_no;
	struct pip2_file_read pip2_file_data;
#ifdef TTDL_DIAGNOSTICS
	struct pip2_file_crc pip2_fcrc;
#endif
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	struct work_struct bl_from_file;
	struct work_struct pip2_bl_from_file;
#endif
};

static u8 update_fw_status;
static int pip2_erase_status;
static int pip2_erase_rc;

/* PIP2 update fw status codes. 1-99% are "active" */
enum UPDATE_FW_STATUS {
	UPDATE_FW_IDLE                   = 0,
	UPDATE_FW_ACTIVE_10              = 10,
	UPDATE_FW_ACTIVE_90              = 90,
	UPDATE_FW_ACTIVE_99              = 99,
	UPDATE_FW_COMPLETE               = 100,
	UPDATE_FW_GENERAL_ERROR          = 200,
	UPDATE_FW_PIP_VERSION_ERROR      = 201,
	UPDATE_FW_VERSION_ERROR          = 202,
	UPDATE_FW_ERASE_ERROR            = 203,
	UPDATE_FW_FILE_CLOSE_ERROR       = 204,
	UPDATE_FW_WRITE_ERROR            = 205,
	UPDATE_FW_EXECUTE_ERROR          = 206,
	UPDATE_FW_RESET_ERROR            = 207,
	UPDATE_FW_MODE_ERROR             = 208,
	UPDATE_FW_ENTER_BL_ERROR         = 209,
	UPDATE_FW_FILE_OPEN_ERROR        = 210,
	UPDATE_FW_SENTINEL_NOT_SEEN      = 211,
	UPDATE_FW_EXCLUSIVE_ACCESS_ERROR = 212,
	UPDATE_FW_NO_FW_PROVIDED         = 213,
	UPDATE_FW_INVALID_FW_IMAGE       = 214,
	UPDATE_FW_MISALIGN_FW_IMAGE      = 230,
	UPDATE_FW_SYSTEM_NOMEM           = 231,
	UPDATE_FW_INIT_BL_ERROR          = 232,
	UPDATE_FW_PARSE_ROW_ERROR        = 233,
	UPDATE_FW_PROGRAM_ROW_ERROR      = 234,
	UPDATE_FW_EXIT_BL_ERROR          = 235,
	UPDATE_FW_CHECK_SUM_ERROR        = 236,
	UPDATE_FW_NO_PLATFORM_DATA       = 237,
	UPDATE_FW_UNDEFINED_ERROR        = 255,
};

enum PIP2_FILE_ERASE_STATUS {
	PIP2_FILE_ERASE_STATUS_DUT_BUSY		= 101,
	PIP2_FILE_ERASE_STATUS_ENTER_BL_ERROR	= 102,
};

struct pt_dev_id {
	u32 silicon_id;
	u8 rev_id;
	u32 bl_ver;
};

struct pt_hex_image {
	u8 array_id;
	u16 row_num;
	u16 row_size;
	u8 row_data[PT_DATA_ROW_SIZE];
} __packed;

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
static int pt_pip2_upgrade_firmware_from_builtin(struct device *dev);
#endif

typedef int (*PIP2_SEND_CMD)(struct device *, int,
		u8, u8 *, u16, u8 *, u16 *);
static struct pt_module loader_module;

/*******************************************************************************
 * FUNCTION: pt_get_loader_data
 *
 * SUMMARY: Inline function to get pt_loader_data pointer from loader module.
 *
 * RETURN:
 *  pointer to pt_loader_data structure
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static inline struct pt_loader_data *pt_get_loader_data(
		struct device *dev)
{
	return pt_get_module_data(dev, &loader_module);
}

#if PT_FW_UPGRADE || PT_TTCONFIG_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_calibrate_idacs
 *
 * SUMMARY: Calibrate idac for mutual-cap,buttons,self-cap by called functions.
 * It needs stop panel scan during calibration.
 *
 * PARAMETERS:
 *	*calibration_work - pointer to work_struct structure
 ******************************************************************************/
static void pt_calibrate_idacs(struct work_struct *calibration_work)
{
	struct pt_loader_data *ld = container_of(calibration_work,
			struct pt_loader_data, calibration_work);
	struct device *dev = ld->dev;
	struct pt_cal_ext_data cal_data = {0};
	u8 dut_gen = cmd->request_dut_generation(dev);
	u8 mode;
	u8 status;
	int rc;

	pt_debug(dev, DL_INFO, "Entering %s\n", __func__);
	rc = cmd->request_exclusive(dev, PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		goto exit;

	rc = cmd->nonhid_cmd->suspend_scanning(dev, 0);
	if (rc < 0)
		goto release;

	if (dut_gen == DUT_PIP1_ONLY) {
		for (mode = 0; mode < 3; mode++) {
			rc = cmd->nonhid_cmd->calibrate_idacs(dev, 0, mode,
							      &status);
			if (rc < 0) {
				pt_debug(dev, DL_ERROR,
					 "%s: calibrate idac error, rc = %d\n",
					 __func__, rc);
				break;
			}
		}
	} else {
		/*
		 * Manual calibration is not need after fw 3.4.932110.
		 * Therefore, PT_LOADER_FLAG_NONE should be used to avoid
		 * manual calibration if the FW is newer than 3.4.932110.
		 */
		memset(&cal_data, 0, sizeof(struct pt_cal_ext_data));
		rc = cmd->nonhid_cmd->calibrate_ext(dev, 0, &cal_data,
						    &status);
		if (rc < 0) {
			pt_debug(dev, DL_ERROR,
				 "%s: extended calibrate error, rc = %d\n",
				 __func__, rc);
		}
	}

	rc = cmd->nonhid_cmd->resume_scanning(dev, 0);
	if (rc < 0)
		goto release;

	pt_debug(dev, DL_WARN, "%s: Calibration Done\n", __func__);

release:
	cmd->release_exclusive(dev);
exit:
	complete(&ld->calibration_complete);
}

/*******************************************************************************
 * FUNCTION: pt_calibration_attention
 *
 * SUMMARY: Wrapper function to schedule calibration work used to subscribe into
 * TTDL attention list.Once called will unsubscribe from attention list.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev - pointer to device structure
 ******************************************************************************/
static int pt_calibration_attention(struct device *dev)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	int rc = 0;

	schedule_work(&ld->calibration_work);

	cmd->unsubscribe_attention(dev, PT_ATTEN_STARTUP, PT_LOADER_NAME,
		pt_calibration_attention, 0);

	return rc;
}
#endif /* PT_FW_UPGRADE || PT_TTCONFIG_UPGRADE */

#if PT_FW_UPGRADE \
	|| defined(CONFIG_TOUCHSCREEN_PARADE_PLATFORM_TTCONFIG_UPGRADE)

/*******************************************************************************
 * FUNCTION: pt_get_panel_id
 *
 * SUMMARY: Get panel id from core data.
 *
 * RETURN:
 *	 panel id
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 ******************************************************************************/
static u8 pt_get_panel_id(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);

	return cd->pid_for_loader;
}
#endif


#if (PT_FW_UPGRADE || PT_TTCONFIG_UPGRADE)
/*******************************************************************************
 * FUNCTION: pt_check_firmware_version
 *
 * SUMMARY: Compare fw's version and revision control number with fw image's.
 *
 * RETURN:
 *  -1: Do not upgrade firmware
 *   0: Version info same, let caller decide
 *   1: Do a firmware upgrade
 *
 * PARAMETERS:
 *  *dev             - pointer to device structure
 *   fw_ver_new      - firmware version
 *   fw_revctrl_new  - firmware revision control number
 ******************************************************************************/
static int pt_check_firmware_version(struct device *dev,
		u32 fw_ver_new, u32 fw_revctrl_new)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u32 fw_ver_img;
	u32 fw_revctrl_img;

	fw_ver_img = ld->si->ttdata.fw_ver_major << 8;
	fw_ver_img += ld->si->ttdata.fw_ver_minor;

	pt_debug(dev, DL_WARN,
		"%s: Current FW ver:0x%04X New FW ver:0x%04X\n", __func__,
			fw_ver_img, fw_ver_new);

	if (fw_ver_new > fw_ver_img) {
		pt_debug(dev, DL_WARN,
			"%s: Image is newer, will upgrade\n", __func__);
		return 1;
	}

	if (fw_ver_new < fw_ver_img) {
		pt_debug(dev, DL_WARN,
			"%s: Image is older, will NOT upgrade\n", __func__);
		return -1;
	}

	fw_revctrl_img = ld->si->ttdata.revctrl;

	pt_debug(dev, DL_WARN,
		"%s: Current FW rev:%d New FW rev:%d\n",
		__func__, fw_revctrl_img, fw_revctrl_new);

	if (fw_revctrl_new > fw_revctrl_img) {
		pt_debug(dev, DL_WARN,
			"%s: Image is newer, will upgrade\n", __func__);
		return 1;
	}

	if (fw_revctrl_new < fw_revctrl_img) {
		pt_debug(dev, DL_WARN,
			"%s: Image is older, will NOT upgrade\n", __func__);
		return -1;
	}

	return 0;
}

#endif /* PT_FW_UPGRADE || PT_TTCONFIG_UPGRADE */

#if PT_FW_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_get_row_
 *
 * SUMMARY: Copy the image data to the "row_buf".
 *
 * RETURN:
 *	 pointer to image buffer plus copy size
 *
 * PARAMETERS:
 *  *dev        - pointer to device structure
 *  *row_buf    - pointer to written buffer to program chip
 *  *image_buf  - pointer to image buffer
 *   size       - size of written data
 ******************************************************************************/
static u8 *pt_get_row_(struct device *dev, u8 *row_buf,
		u8 *image_buf, int size)
{
	memcpy(row_buf, image_buf, size);
	return image_buf + size;
}

/*******************************************************************************
 * FUNCTION: pt_ldr_enter_
 *
 * SUMMARY: Enter bootloader state and update device id(silicon id, rev id,
 *  bootloader version).
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev     - pointer to device structure
 *  *dev_id  - pointer to device id
 ******************************************************************************/
static int pt_ldr_enter_(struct device *dev, struct pt_dev_id *dev_id)
{
	int rc;
	u8 return_data[8];
	u8 mode = PT_MODE_OPERATIONAL;

	dev_id->silicon_id = 0;
	dev_id->rev_id = 0;
	dev_id->bl_ver = 0;

	/*
	 * Reset to get to a known state, sleep to allow sentinel processing.
	 *
	 * NOTE: This msleep MUST be greater than the auto launch timeout
	 * that is built into the FW.
	 */
	cmd->request_reset(dev, PT_CORE_CMD_UNPROTECTED);
	msleep(20);

	rc = cmd->request_get_mode(dev, 0, &mode);
	pt_debug(dev, DL_INFO, "%s:request_get_mode rc=%d mode=%d\n",
		__func__, rc, mode);
	if (rc)
		return rc;

	if (mode == PT_MODE_UNKNOWN)
		return -EINVAL;

	if (mode == PT_MODE_OPERATIONAL) {
		rc = cmd->nonhid_cmd->start_bl(dev, PT_CORE_CMD_UNPROTECTED);
		pt_debug(dev, DL_INFO, "%s:start_bl rc=%d\n", __func__, rc);
		if (rc)
			return rc;
		rc = cmd->request_get_mode(dev, 0, &mode);
		pt_debug(dev, DL_INFO, "%s:request_get_mode rc=%d mode=%d\n",
			__func__, rc, mode);
		if (rc)
			return rc;
		if (mode != PT_MODE_BOOTLOADER)
			return -EINVAL;
	}

	rc = cmd->nonhid_cmd->get_bl_info(dev,
		PT_CORE_CMD_UNPROTECTED, return_data);
	pt_debug(dev, DL_INFO, "%s:get_bl_info rc=%d\n", __func__, rc);
	if (rc)
		return rc;

	dev_id->silicon_id = get_unaligned_le32(&return_data[0]);
	dev_id->rev_id = return_data[4];
	dev_id->bl_ver = return_data[5] + (return_data[6] << 8)
		+ (return_data[7] << 16);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_ldr_init_
 *
 * SUMMARY: Erase the entire TrueTouch application, Configuration Data block,
 *  and Design Data block in flash and enables the host to execute the Program
 *  and Verify Row command to bootload the application image and data.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev           - pointer to device structure
 *  *pt_hex_image  - pointer to hex image structure
 ******************************************************************************/
static int pt_ldr_init_(struct device *dev,
		struct pt_hex_image *row_image)
{
	return cmd->nonhid_cmd->initiate_bl(dev, 0, 8,
			(u8 *)pt_data_block_security_key, row_image->row_size,
			row_image->row_data);
}

/*******************************************************************************
 * FUNCTION: pt_ldr_parse_row_
 *
 * SUMMARY: Parse and copy the row buffer data to hex image structure.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev        - pointer to device structure
 *  *row_buf    - pointer to row buffer
 *  *row_image  - pointer to hex image structure
 ******************************************************************************/
static int pt_ldr_parse_row_(struct device *dev, u8 *row_buf,
	struct pt_hex_image *row_image)
{
	int rc = 0;

	row_image->array_id = row_buf[PT_ARRAY_ID_OFFSET];
	row_image->row_num = get_unaligned_be16(&row_buf[PT_ROW_NUM_OFFSET]);
	row_image->row_size = get_unaligned_be16(&row_buf[PT_ROW_SIZE_OFFSET]);

	if (row_image->row_size > ARRAY_SIZE(row_image->row_data)) {
		pt_debug(dev, DL_ERROR,
			"%s: row data buffer overflow\n", __func__);
		rc = -EOVERFLOW;
		goto pt_ldr_parse_row_exit;
	}

	memcpy(row_image->row_data, &row_buf[PT_ROW_DATA_OFFSET],
	       row_image->row_size);
pt_ldr_parse_row_exit:
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_ldr_prog_row_
 *
 * SUMMARY: Program one row that the hex image structure data to the chip.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev        - pointer to device structure
 *  *row_image  - pointer to hex image structure
 ******************************************************************************/
static int pt_ldr_prog_row_(struct device *dev,
				 struct pt_hex_image *row_image)
{
	u16 length = row_image->row_size + 3;
	u8 data[3 + row_image->row_size];
	u8 offset = 0;

	data[offset++] = row_image->array_id;
	data[offset++] = LOW_BYTE(row_image->row_num);
	data[offset++] = HI_BYTE(row_image->row_num);
	memcpy(data + 3, row_image->row_data, row_image->row_size);
	return cmd->nonhid_cmd->prog_and_verify(dev, 0, length, data);
}

/*******************************************************************************
 * FUNCTION: pt_ldr_verify_chksum_
 *
 * SUMMARY: Perform a full verification of the application integrity by
 *  calculating the CRC of the TrueTouch application image in flash and
 *  comparing it to the expected CRC stored in the TrueTouch application CRC
 *  value stored in the Metadata row.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static int pt_ldr_verify_chksum_(struct device *dev)
{
	u8 result;
	int rc;

	rc = cmd->nonhid_cmd->verify_app_integrity(dev, 0, &result);
	if (rc)
		return rc;

	/* fail */
	if (result == 0)
		return -EINVAL;

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_ldr_exit_
 *
 * SUMMARY: Launch the application from bootloader.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static int pt_ldr_exit_(struct device *dev)
{
	return cmd->nonhid_cmd->launch_app(dev, 0);
}

#define PT_NO_INC  0
/*******************************************************************************
 * FUNCTION: _pt_pip2_update_bl_status
 *
 * SUMMARY: Update the value in the bl_status global by either setting to a
 *	specific value or incrementing it ensuring we don't go out of bounds.
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	value - Value to force, if 0 then ignore.
 *	inc   - Value to increment, if 0 then ignore.
 ******************************************************************************/
static u8 _pt_pip2_update_bl_status(struct device *dev, u8 value, u8 inc)
{
	pt_debug(dev, DL_DEBUG,
		"%s: fw_status = %d, request val=%d, request inc=%d\n",
		__func__, update_fw_status, value, inc);

	/* Reset status if both value and inc are 0 */
	if (value == UPDATE_FW_IDLE && inc == 0) {
		update_fw_status = value;
		pt_debug(dev, DL_WARN, "%s: ATM - Reset BL Status to %d\n",
			__func__, value);
		return update_fw_status;
	}

	/* Set to value if valid */
	if (value > UPDATE_FW_IDLE && value < UPDATE_FW_UNDEFINED_ERROR) {
		if (value <= UPDATE_FW_COMPLETE && value > update_fw_status) {
			update_fw_status = value;
		} else if (value >= UPDATE_FW_GENERAL_ERROR) {
			update_fw_status = value;
			pt_debug(dev, DL_WARN,
				"%s: BL Status set to error code %d\n",
				__func__, value);
		}
		return update_fw_status;
	}

	if (inc > 0 && update_fw_status + inc <= UPDATE_FW_COMPLETE)
		update_fw_status += inc;
	else
		pt_debug(dev, DL_ERROR,
			"%s: Inc Request out of bounds: status=%d inc=%d\n",
			__func__, update_fw_status, inc);
	return update_fw_status;
}

/*******************************************************************************
 * FUNCTION: pt_load_app_
 *
 * SUMMARY: Program the firmware image to the chip.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev           - pointer to device structure
 *  *fw            - pointer to the firmware
 *   fw_size       - size of firmware
 *   update_status - store the update status of firmware
 ******************************************************************************/
static int pt_load_app_(struct device *dev, const u8 *fw, int fw_size,
	u8 *update_status)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_dev_id *dev_id;
	struct pt_hex_image *row_image;
	u8 *row_buf;
	size_t image_rec_size;
	size_t row_buf_size = PT_DATA_MAX_ROW_SIZE;
	int row_count = 0;
	u8 *p;
	u8 *last_row;
	int rc;
	int rc_tmp;
	int percent_cmplt;
	int total_row_count;

	if (update_status == NULL) {
		rc = -EINVAL;
		pt_debug(dev, DL_ERROR,
			"%s: update_status is NULL pointer %d\n",
			__func__, rc);
		return rc;
	} else if (*update_status > UPDATE_FW_ACTIVE_10) {
		pt_debug(dev, DL_WARN,
			"%s: update_status is illegal = %d, fixed to %d\n",
			__func__, *update_status, UPDATE_FW_ACTIVE_10);
		*update_status = UPDATE_FW_ACTIVE_10;
	}

	image_rec_size = sizeof(struct pt_hex_image);
	if (fw_size % image_rec_size != 0) {
		pt_debug(dev, DL_ERROR,
			"%s: Firmware image is misaligned\n", __func__);
		*update_status = UPDATE_FW_MISALIGN_FW_IMAGE;
		rc = -EINVAL;
		goto _pt_load_app_error;
	}
	*update_status += 1;
	total_row_count = fw_size / image_rec_size - 1;

	pt_debug(dev, DL_INFO, "%s: start load app\n", __func__);
#ifdef TTHE_TUNER_SUPPORT
	cmd->request_tthe_print(dev, NULL, 0, "start load app");
#endif

	row_buf = kzalloc(row_buf_size, GFP_KERNEL);
	row_image = kzalloc(sizeof(struct pt_hex_image), GFP_KERNEL);
	dev_id = kzalloc(sizeof(struct pt_dev_id), GFP_KERNEL);
	if (!row_buf || !row_image || !dev_id) {
		*update_status = UPDATE_FW_SYSTEM_NOMEM;
		rc = -ENOMEM;
		goto _pt_load_app_exit;
	}
	*update_status += 1;

	cmd->request_stop_wd(dev);

	pt_debug(dev, DL_INFO, "%s: Send BL Loader Enter\n", __func__);
#ifdef TTHE_TUNER_SUPPORT
	cmd->request_tthe_print(dev, NULL, 0, "Send BL Loader Enter");
#endif
	rc = pt_ldr_enter_(dev, dev_id);
	if (rc) {
		*update_status = UPDATE_FW_ENTER_BL_ERROR;
		pt_debug(dev, DL_ERROR, "%s: Error cannot start Loader (ret=%d)\n",
			__func__, rc);
		goto _pt_load_app_exit;
	}
	pt_debug(dev, DL_INFO,
		"%s: dev: silicon id=%08X rev=%02X bl=%08X\n", __func__,
		dev_id->silicon_id, dev_id->rev_id, dev_id->bl_ver);
	*update_status += 1;

	/*
	 * since start loader is successful, firmware mode can be assumed
	 * at bl mode directly. Updating this variable is helpful to detect
	 * firmware entered application mode.
	 */
	cd->mode = PT_MODE_BOOTLOADER;

	/* get last row */
	last_row = (u8 *)fw + fw_size - image_rec_size;
	pt_get_row_(dev, row_buf, last_row, image_rec_size);
	pt_ldr_parse_row_(dev, row_buf, row_image);

	/* initialise bootloader */
	rc = pt_ldr_init_(dev, row_image);
	if (rc) {
		*update_status = UPDATE_FW_ERASE_ERROR;
		pt_debug(dev, DL_ERROR, "%s: Error cannot init Loader (ret=%d)\n",
			__func__, rc);
		goto _pt_load_app_exit;
	}
	*update_status += 5;

	pt_debug(dev, DL_INFO,
		"%s: Send BL Loader Blocks\n", __func__);
#ifdef TTHE_TUNER_SUPPORT
	cmd->request_tthe_print(dev, NULL, 0, "Send BL Loader Blocks");
#endif
	p = (u8 *)fw;
	while (p < last_row) {
		/* Get row */
		row_count += 1;
		pt_debug(dev, DL_INFO, "%s: read row=%d\n",
			__func__, row_count);
		memset(row_buf, 0, row_buf_size);
		p = pt_get_row_(dev, row_buf, p, image_rec_size);

		/* Don't update BL status on every pass */
		if (row_count / 8 * 8 == row_count) {
			/* Calculate % complete for update_status sysfs */
			percent_cmplt = row_count * 100 / total_row_count;
			if (percent_cmplt > UPDATE_FW_ACTIVE_99)
				percent_cmplt = UPDATE_FW_ACTIVE_99;
			*update_status = (percent_cmplt > *update_status) ?
				percent_cmplt : *update_status;
#ifdef TTDL_DIAGNOSTICS
			pt_debug(dev, DL_INFO,
				"Wrote row num %d of total %d\n",
				row_count, total_row_count);
#endif
		}

		/* Parse row */
		pt_debug(dev, DL_INFO, "%s: p=%p buf=%p buf[0]=%02X\n",
			__func__, p, row_buf, row_buf[0]);
		rc = pt_ldr_parse_row_(dev, row_buf, row_image);
		pt_debug(dev, DL_INFO,
			"%s: array_id=%02X row_num=%04X(%d) row_size=%04X(%d)\n",
			__func__, row_image->array_id,
			row_image->row_num, row_image->row_num,
			row_image->row_size, row_image->row_size);
		if (rc) {
			*update_status = UPDATE_FW_PARSE_ROW_ERROR;
			pt_debug(dev, DL_ERROR,
				"%s: Parse Row Error (a=%d r=%d ret=%d\n",
				__func__, row_image->array_id,
				row_image->row_num, rc);
			goto _pt_load_app_exit;
		} else {
			pt_debug(dev, DL_INFO,
				"%s: Parse Row (a=%d r=%d ret=%d\n",
				__func__, row_image->array_id,
				row_image->row_num, rc);
		}

		/* program row */
		rc = pt_ldr_prog_row_(dev, row_image);
		if (rc) {
			*update_status = UPDATE_FW_PROGRAM_ROW_ERROR;
			pt_debug(dev, DL_ERROR,
				"%s: Prog Row Error (array=%d row=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, rc);
			goto _pt_load_app_exit;
		}

		pt_debug(dev, DL_INFO,
			"%s: array=%d row_cnt=%d row_num=%04X\n",
			__func__, row_image->array_id, row_count,
			row_image->row_num);
	}

	/* exit loader */
	pt_debug(dev, DL_INFO, "%s: Send BL Loader Terminate\n", __func__);
#ifdef TTHE_TUNER_SUPPORT
	cmd->request_tthe_print(dev, NULL, 0, "Send BL Loader Terminate");
#endif
	rc = pt_ldr_exit_(dev);
	if (rc) {
		*update_status = UPDATE_FW_EXIT_BL_ERROR;
		pt_debug(dev, DL_ERROR, "%s: Error on exit Loader (ret=%d)\n",
			__func__, rc);

		/* verify app checksum */
		rc_tmp = pt_ldr_verify_chksum_(dev);
		if (rc_tmp) {
			*update_status = UPDATE_FW_CHECK_SUM_ERROR;
			pt_debug(dev, DL_ERROR,
				"%s: ldr_verify_chksum fail r=%d\n",
				__func__, rc_tmp);
		} else
			pt_debug(dev, DL_INFO,
				"%s: APP Checksum Verified\n", __func__);
	} else
		*update_status = UPDATE_FW_ACTIVE_99;

_pt_load_app_exit:
	kfree(row_buf);
	kfree(row_image);
	kfree(dev_id);
_pt_load_app_error:
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_upgrade_firmware
 *
 * SUMMARY: Program the firmware image and set call back for start up.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev           - pointer to device structure
 *  *fw_img        - pointer to the firmware
 *   fw_size       - size of firmware
 *  *update_status - store the update status of firmware
 ******************************************************************************/
static int pt_upgrade_firmware(struct device *dev, const u8 *fw_img,
		int fw_size, u8 *update_status)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int retry = PT_LOADER_FW_UPGRADE_RETRY_COUNT;
	bool wait_for_calibration_complete = false;
	int rc;
	int t;

	_pt_pip2_update_bl_status(dev, UPDATE_FW_IDLE, PT_NO_INC);

	if (update_status == NULL) {
		rc = -EINVAL;
		pt_debug(dev, DL_ERROR,
			"%s: update_status is NULL pointer %d\n",
			__func__, rc);
		return rc;
	} else if (*update_status > UPDATE_FW_ACTIVE_10) {
		pt_debug(dev, DL_WARN,
			"%s: update_status is illegal = %d, fixed to %d\n",
			__func__, *update_status, UPDATE_FW_ACTIVE_10);
		*update_status = UPDATE_FW_ACTIVE_10;
	}

	/* Ensure no enum task is pending */
	pt_debug(dev, DL_WARN, "%s: Cancel enum work thread\n", __func__);
	cancel_work_sync(&cd->enum_work);

	pm_runtime_get_sync(dev);
	*update_status += 1;

	rc = cmd->request_exclusive(dev, PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		*update_status = UPDATE_FW_EXCLUSIVE_ACCESS_ERROR;
		goto exit;
	}
	*update_status += 1;

	t = *update_status;
	while (retry--) {
		/* Restore the update_status */
		*update_status = t;
		rc = pt_load_app_(dev, fw_img, fw_size, update_status);
		if (rc < 0)
			pt_debug(dev, DL_ERROR,
				"%s: Firmware update failed rc=%d, retry:%d\n",
				__func__, rc, retry);
		else
			break;
		msleep(20);
	}
	if (rc < 0) {
		pt_debug(dev, DL_ERROR,
			"%s: Firmware update failed with error code %d\n",
			__func__, rc);
	} else if (ld->loader_pdata &&
			(ld->loader_pdata->flags
			 & PT_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE)) {
#if (KERNEL_VERSION(3, 13, 0) <= LINUX_VERSION_CODE)
		reinit_completion(&ld->calibration_complete);
#else
		INIT_COMPLETION(ld->calibration_complete);
#endif
		/* set up call back for startup */
		pt_debug(dev, DL_INFO,
			"%s: Adding callback for calibration\n", __func__);
		rc = cmd->subscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_LOADER_NAME, pt_calibration_attention, 0);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Failed adding callback for calibration\n",
				__func__);
			pt_debug(dev, DL_ERROR,
				"%s: No calibration will be performed\n",
				__func__);
			rc = 0;
		} else
			wait_for_calibration_complete = true;
	}

	cmd->release_exclusive(dev);

exit:
	if (!rc) {
		cmd->request_enum(dev, true);

		/*
		 * Wait for FW reset sentinel from reset or execute for up
		 * to 500ms
		 */
		t = wait_event_timeout(cd->wait_q,
			(cd->startup_status >=
				STARTUP_STATUS_FW_RESET_SENTINEL) &&
			(cd->mode == PT_MODE_OPERATIONAL),
			msecs_to_jiffies(PT_BL_WAIT_FOR_SENTINEL));
		if (IS_TMO(t)) {
			pt_debug(dev, DL_WARN,
				"%s: 0x%04X Timeout waiting for FW sentinel",
				__func__, cd->startup_status);
		}

		/* Double verify DUT is alive and well in Application mode */
		if (!(cd->startup_status & STARTUP_STATUS_FW_RESET_SENTINEL)) {
			pt_debug(dev, DL_ERROR,
				"%s FW sentinel not seen\n", __func__);
			*update_status = UPDATE_FW_SENTINEL_NOT_SEEN;
		} else if (cd->mode != PT_MODE_OPERATIONAL) {
			*update_status = UPDATE_FW_MODE_ERROR;
			pt_debug(dev, DL_ERROR,
				"%s ERROR: Not in App mode as expected\n",
				__func__);
		} else {
			*update_status = UPDATE_FW_COMPLETE;
			pt_debug(dev, DL_INFO,
				"%s == PIP1 FW upgrade finished ==\n",
				__func__);
		}
	} else if (*update_status < UPDATE_FW_COMPLETE) {
		*update_status = UPDATE_FW_UNDEFINED_ERROR;
		pt_debug(dev, DL_ERROR, "%s undefined error!\n", __func__);
	}

	pm_runtime_put_sync(dev);

	if (wait_for_calibration_complete)
		wait_for_completion(&ld->calibration_complete);

	return rc;
}

#endif /* PT_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_check_firmware_version_platform
 *
 * SUMMARY: The caller of function pt_check_firmware_version() to determine
 *  whether to load firmware from touch_firmware structure.
 *
 * RETURN:
 *   0: Don't upgrade
 *   1: Upgrade
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 *  *fw   - pointer to the touch_firmware structure
 ******************************************************************************/
static int pt_check_firmware_version_platform(struct device *dev,
		struct pt_touch_firmware *fw)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u32 fw_ver_new;
	u32 fw_revctrl_new;
	int upgrade;

	if (!ld->si) {
		pt_debug(dev, DL_WARN,
			"%s: No firmware info found, DUT FW may be corrupted\n",
			__func__);
		return PT_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	fw_ver_new = get_unaligned_be16(fw->ver + 2);
	/* 4 middle bytes are not used */
	fw_revctrl_new = get_unaligned_be32(fw->ver + 8);
	pt_debug(dev, DL_WARN, "%s: Built-in FW version 0x%04x rev %d\n",
		__func__, fw_ver_new, fw_revctrl_new);

	upgrade = pt_check_firmware_version(dev, fw_ver_new,
		fw_revctrl_new);

	if (upgrade > 0)
		return 1;

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_get_platform_firmware
 *
 * SUMMARY: To get the pointer of right touch_firmware structure by panel id.
 *
 * RETURN:
 *   pointer to touch_firmware structure or null pointer if fail
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static struct pt_touch_firmware *pt_get_platform_firmware(
		struct device *dev)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_touch_firmware **fws;
	struct pt_touch_firmware *fw;
	u8 panel_id;

	panel_id = pt_get_panel_id(dev);
	if (panel_id == PANEL_ID_NOT_ENABLED) {
		pt_debug(dev, DL_WARN,
			"%s: Panel ID not enabled, using legacy firmware\n",
			__func__);
		return ld->loader_pdata->fw;
	}

	fws = ld->loader_pdata->fws;
	if (!fws) {
		pt_debug(dev, DL_ERROR,
			"%s: No firmwares provided\n", __func__);
		return NULL;
	}

	/* Find FW according to the Panel ID */
	while ((fw = *fws++)) {
		if (fw->panel_id == panel_id) {
			pt_debug(dev, DL_WARN,
				"%s: Found matching fw:%p with Panel ID: 0x%02X\n",
				__func__, fw, fw->panel_id);
			return fw;
		}
		pt_debug(dev, DL_WARN,
			"%s: Found mismatching fw:%p with Panel ID: 0x%02X\n",
			__func__, fw, fw->panel_id);
	}

	return NULL;
}

/*******************************************************************************
 * FUNCTION: upgrade_firmware_from_platform
 *
 * SUMMARY: Get touch_firmware structure and perform upgrade if pass the
 *  firmware version check.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev     - pointer to device structure
 *   forced  - flag to force upgrade(1:force to upgrade)
 ******************************************************************************/
static int upgrade_firmware_from_platform(struct device *dev,
		bool forced)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_touch_firmware *fw;
	int rc = -ENODEV;
	int upgrade;
	int retry = 3;

retry_bl:
	if (!ld->loader_pdata) {
		_pt_pip2_update_bl_status(dev, UPDATE_FW_NO_PLATFORM_DATA,
			PT_NO_INC);
		pt_debug(dev, DL_ERROR,
			"%s: No loader platform data\n", __func__);
		return rc;
	}

	fw = pt_get_platform_firmware(dev);
	if (!fw || !fw->img || !fw->size) {
		_pt_pip2_update_bl_status(dev, UPDATE_FW_NO_FW_PROVIDED,
			PT_NO_INC);
		pt_debug(dev, DL_ERROR,
			"%s: No platform firmware\n", __func__);
		return rc;
	}

	if (!fw->ver || !fw->vsize) {
		_pt_pip2_update_bl_status(dev, UPDATE_FW_INVALID_FW_IMAGE,
			PT_NO_INC);
		pt_debug(dev, DL_ERROR, "%s: No platform firmware version\n",
			__func__);
		return rc;
	}

	if (forced)
		upgrade = forced;
	else
		upgrade = pt_check_firmware_version_platform(dev, fw);

	if (upgrade) {
		rc = pt_upgrade_firmware(dev,
			fw->img, fw->size, &update_fw_status);

		/* An extra BL may be needed if default PID was wrong choice */
		if ((cd->panel_id_support & PT_PANEL_ID_BY_SYS_INFO) &&
				!rc && !ld->si && retry--) {
			pt_debug(dev, DL_WARN, "%s: ATM - An extra BL may be needed\n",
				__func__);
			/* Panel_ID coming from sysinfo, ensure we have it */
			ld->si = cmd->request_sysinfo(dev);
			goto retry_bl;
		}
	}

	return rc;
}
#endif /* CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
/*******************************************************************************
 * FUNCTION: _pt_pip1_bl_from_file
 *
 * SUMMARY: Wrapper function for _pt_firmware_cont() to perform bl with an
 *  image file from userspace.
 *
 * PARAMETERS:
 *  *fw      - pointer to firmware structure
 *   forced  - flag to force upgrade(1:force to upgrade)
 ******************************************************************************/
static int _pt_pip1_bl_from_file(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int fw_size = 0;
	int rc = 0;
	u8 *fw_img = NULL;
	u8 header_size = 0;

	_pt_pip2_update_bl_status(dev, UPDATE_FW_IDLE, PT_NO_INC);

	fw_img = kzalloc(PT_PIP2_MAX_FILE_SIZE, GFP_KERNEL);
	if (!fw_img) {
		_pt_pip2_update_bl_status(dev, UPDATE_FW_SYSTEM_NOMEM,
					  PT_NO_INC);
		rc = -ENOMEM;
		goto exit;
	}

	rc = cmd->nonhid_cmd->read_us_file(dev, cd->pip2_us_file_path,
					    fw_img, &fw_size);
	if (rc) {
		pt_debug(dev, DL_ERROR, "%s: No firmware provided to load\n",
			 __func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_NO_FW_PROVIDED,
					  PT_NO_INC);
		goto exit;
	}

	if (!fw_img || !fw_size) {
		pt_debug(dev, DL_ERROR,
			"%s: No firmware received\n", __func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_INVALID_FW_IMAGE,
			PT_NO_INC);
		goto exit;
	}

	header_size = fw_img[0];
	if (header_size >= (fw_size + 1)) {
		pt_debug(dev, DL_ERROR,
			"%s: Firmware format is invalid\n", __func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_INVALID_FW_IMAGE,
			PT_NO_INC);
		goto exit;
	}

	pt_upgrade_firmware(dev, &(fw_img[header_size + 1]),
		fw_size - (header_size + 1), &update_fw_status);
exit:
	kfree(fw_img);
	return rc;
}

/*******************************************************************************
 * FUNCTION: _pt_firmware_cont
 *
 * SUMMARY: Firmware upgrade continue function that verifies the firmware size
 *  in the firmware class and then upgrades the firmware.
 *
 * PARAMETERS:
 *  *fw      - pointer to firmware structure
 *   forced  - flag to force upgrade(1:force to upgrade)
 ******************************************************************************/
static void _pt_firmware_cont(const struct firmware *fw, void *context)
{
	struct device *dev = context;
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u8 header_size = 0;

	_pt_pip2_update_bl_status(dev, UPDATE_FW_IDLE, PT_NO_INC);
	if (!fw) {
		pt_debug(dev, DL_ERROR, "%s: No firmware\n", __func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_NO_FW_PROVIDED,
			PT_NO_INC);
		goto pt_firmware_cont_exit;
	}

	if (!fw->data || !fw->size) {
		pt_debug(dev, DL_ERROR,
			"%s: No firmware received\n", __func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_INVALID_FW_IMAGE,
			PT_NO_INC);
		goto pt_firmware_cont_release_exit;
	}

	header_size = fw->data[0];
	if (header_size >= (fw->size + 1)) {
		pt_debug(dev, DL_ERROR,
			"%s: Firmware format is invalid\n", __func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_INVALID_FW_IMAGE,
			PT_NO_INC);
		goto pt_firmware_cont_release_exit;
	}

	pt_upgrade_firmware(dev, &(fw->data[header_size + 1]),
		fw->size - (header_size + 1), &update_fw_status);

pt_firmware_cont_release_exit:
	if (fw)
		release_firmware(fw);

pt_firmware_cont_exit:
	ld->is_manual_upgrade_enabled = 0;
}

/*******************************************************************************
 * FUNCTION: pt_check_firmware_config_version
 *
 * SUMMARY: Compare fw's config version with fw image's. If they are differnt
 *	report a FW upgrade is needed.
 *
 * RETURN:
 *  -1: Do not upgrade firmware
 *   0: Version info same, let caller decide
 *   1: Do a firmware upgrade
 *
 * PARAMETERS:
 *  *dev               - pointer to device structure
 *   image_config_ver  - Image's firmware config version
 ******************************************************************************/
static int pt_check_firmware_config_version(struct device *dev,
		u16 image_config_ver)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u16 fw_config_ver;

	fw_config_ver = ld->si->ttdata.fw_ver_conf;

	pt_debug(dev, DL_WARN,
		 "%s: Current config ver:0x%04X New config ver:0x%04X\n",
		 __func__, fw_config_ver, image_config_ver);

	if (image_config_ver != fw_config_ver) {
		pt_debug(dev, DL_WARN,
			 "%s: Image config ver is different, will upgrade\n",
			 __func__);
		return 1;
	}

	if (image_config_ver == fw_config_ver) {
		pt_debug(dev, DL_WARN,
			 "%s: Image config ver is the same, will NOT upgrade\n",
			 __func__);
		return 0;
	}

	return -1;
}

/*******************************************************************************
 * FUNCTION: pt_check_firmware_version_builtin
 *
 * SUMMARY: The caller of function pt_check_firmware_version() to determine
 *  whether to load built-in firmware.
 *
 * RETURN:
 *   0: Don't upgrade
 *   1: Upgrade
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 *  *fw   - pointer to the firmware structure
 ******************************************************************************/
static int pt_check_firmware_version_builtin(struct device *dev,
		const struct firmware *fw)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u16 fw_config_ver_new;
	u32 fw_ver_new;
	u32 fw_revctrl_new;
	int upgrade;

	if (!ld->si) {
		pt_debug(dev, DL_WARN,
			"%s: No firmware info found, DUT FW may be corrupted\n",
			__func__);
		return PT_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	fw_ver_new = get_unaligned_be16(fw->data + 3);
	/* 4 middle bytes are not used */
	fw_revctrl_new = get_unaligned_be32(fw->data + 9);
	/* Offset 17,18 is the TT Config version*/
	fw_config_ver_new = get_unaligned_be16(fw->data + 17);

	pt_debug(dev, DL_WARN,
		"%s: Built-in FW version=0x%04x rev=%d config=0x%04X\n",
		__func__, fw_ver_new, fw_revctrl_new, fw_config_ver_new);

	upgrade = pt_check_firmware_version(dev, fw_ver_new,
			fw_revctrl_new);

	/* Only check config version if FW version was an exact match */
	if (upgrade == 0)
		upgrade = pt_check_firmware_config_version(dev,
					fw_config_ver_new);

	if (upgrade > 0)
		return 1;

	return 0;
}

/*******************************************************************************
 * FUNCTION: _pt_firmware_cont_builtin
 *
 * SUMMARY: Perform upgrade if pass the firmware version check.
 *
 * PARAMETERS:
 *  *dev     - pointer to device structure
 *   forced  - flag to force upgrade(1:force to upgrade)
 ******************************************************************************/
static void _pt_firmware_cont_builtin(const struct firmware *fw,
		void *context)
{
	struct device *dev = context;
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	int upgrade;

	if (!fw) {
		pt_debug(dev, DL_INFO,
			"%s: No builtin firmware\n", __func__);
		goto _pt_firmware_cont_builtin_exit;
	}

	if (!fw->data || !fw->size) {
		pt_debug(dev, DL_ERROR,
			"%s: Invalid builtin firmware\n", __func__);
		goto _pt_firmware_cont_builtin_exit;
	}

	pt_debug(dev, DL_WARN, "%s: Found firmware\n", __func__);

	upgrade = pt_check_firmware_version_builtin(dev, fw);
	if (upgrade) {
		_pt_firmware_cont(fw, dev);
		ld->builtin_bin_fw_status = 0;
		return;
	}

_pt_firmware_cont_builtin_exit:
	if (fw)
		release_firmware(fw);

	ld->builtin_bin_fw_status = -EINVAL;
}

/*******************************************************************************
 * FUNCTION: upgrade_firmware_from_class
 *
 * SUMMARY: Create the firmware class but don't actually load any FW to the
 *	DUT. This creates all the sysfs nodes needed for a user to bootload
 *	the DUT with their own bin file.
 *
 * RETURN:
 *   0 = success
 *
 * PARAMETERS:
 *  *dev     - pointer to device structure
 ******************************************************************************/
static int upgrade_firmware_from_class(struct device *dev)
{
	int retval;

	pt_debug(dev, DL_INFO,
		"%s: Enabling firmware class loader\n", __func__);

	retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG,
			dev_name(dev), dev, GFP_KERNEL, dev,
			_pt_firmware_cont);
	if (retval < 0) {
		pt_debug(dev, DL_ERROR,
			"%s: Fail request firmware class file load\n",
			__func__);
		return retval;
	}

	return 0;
}

#define FILENAME_LEN_MAX 64
/*******************************************************************************
 * FUNCTION: generate_firmware_filename
 *
 * SUMMARY: Generate firmware file name by panel id. Generates binary FW
 *  filename as following:
 *  - Panel ID not enabled: tt_fw.bin
 *  - Panel ID enabled: tt_fw_pidXX.bin
 *
 * RETURN:
 *   pointer to file name or null pointer if fail
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static char *generate_firmware_filename(struct device *dev)
{
	char *filename;
	u8 panel_id;

	filename = kzalloc(FILENAME_LEN_MAX, GFP_KERNEL);
	if (!filename)
		return NULL;

	panel_id = pt_get_panel_id(dev);
	if (panel_id == PANEL_ID_NOT_ENABLED)
		snprintf(filename, FILENAME_LEN_MAX, "%s", PT_FW_FILE_NAME);
	else
		snprintf(filename, FILENAME_LEN_MAX, "%s_pid%02X%s",
			PT_FW_FILE_PREFIX, panel_id, PT_FW_FILE_SUFFIX);

	pt_debug(dev, DL_INFO, "%s: Filename: %s\n",
		__func__, filename);

	return filename;
}

/*******************************************************************************
 * FUNCTION: generate_silicon_id_firmware_filename
 *
 * SUMMARY: Generate firmware file name with the HW version prefix followed by
 * panel id. Generates binary FW filename as following (where XXXX is the
 * silicon ID):
 *  - Panel ID not enabled: XXXX_tt_fw.bin
 *  - Panel ID enabled: XXXX_tt_fw_pidXX.bin
 *
 * RETURN:
 *   pointer to file name or null pointer if fail
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static char *generate_silicon_id_firmware_filename(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	char *filename;
	char si_id[5] = "DEAD";
	u8 panel_id;

	filename = kzalloc(FILENAME_LEN_MAX, GFP_KERNEL);
	if (!filename)
		return NULL;

	panel_id = pt_get_panel_id(dev);
	memcpy(si_id, cd->hw_version, 4);

	if (panel_id == PANEL_ID_NOT_ENABLED)
		snprintf(filename, FILENAME_LEN_MAX, "%s_%s", si_id,
			PT_FW_FILE_NAME);
	else
		snprintf(filename, FILENAME_LEN_MAX, "%s_%s_pid%02X%s",
			si_id, PT_FW_FILE_PREFIX, panel_id, PT_FW_FILE_SUFFIX);

	pt_debug(dev, DL_INFO, "%s: Filename: %s\n", __func__, filename);

	return filename;
}

#define MAX_FILE_NAMES 2
/*******************************************************************************
 * FUNCTION: upgrade_firmware_from_builtin
 *
 * SUMMARY: Create the firmware class load FW by searching the name of built-in
 *	file. Then perform upgrade after getting the file.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev     - pointer to device structure
 ******************************************************************************/
static int upgrade_firmware_from_builtin(struct device *dev)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	char *filename[MAX_FILE_NAMES];
	int index = 0;
	int retval;
	const struct firmware *fw_entry = NULL;
	int retry = 3;

retry_bl:
	pt_debug(dev, DL_WARN,
		"%s: Enabling firmware class loader built-in\n",
		__func__);

	/* Load the supported file names in the search order */

	/* 0 = Flash file name with optional PID "pt_fw<_PIDX>.bin" */
	filename[0] = generate_firmware_filename(dev);
	if (!filename[0]) {
		pt_debug(dev, DL_ERROR,
			"%s: ERROR - Could not generate filename\n", __func__);
		return -ENOMEM;
	}

	/*
	 * 1 = Flash file name with Silicon ID prefix and optional PID
	 * "XXXX_pt_fw<_PIDX>.bin"
	 */
	filename[1] = generate_silicon_id_firmware_filename(dev);
	if (!filename[1]) {
		pt_debug(dev, DL_ERROR,
			"%s: ERROR - Could not generate filename\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&cd->firmware_class_lock);
	while (index < MAX_FILE_NAMES) {
		pt_debug(dev, DL_WARN, "%s: Request FW file %s\n", __func__,
			filename[index]);
#if (KERNEL_VERSION(3, 13, 0) > LINUX_VERSION_CODE)
		retval = request_firmware(&fw_entry, filename[index], dev);
#else
		retval = request_firmware_direct(&fw_entry,
			filename[index], dev);
#endif
		if (retval < 0) {
			pt_debug(dev, DL_WARN, "%s: Fail request FW %s load\n",
				__func__, filename[index]);
		} else {
			pt_debug(dev, DL_WARN, "%s: FW %s class file loading\n",
				__func__, filename[index]);
			break;
		}
		index++;
	}

	/* Proceed with the BL if a matching file was found */
	if (index != MAX_FILE_NAMES) {
		_pt_firmware_cont_builtin(fw_entry, dev);
		/* An extra BL may be needed if default PID was wrong choice */
		if ((cd->panel_id_support & PT_PANEL_ID_BY_SYS_INFO) &&
				!ld->si && retry--) {
			pt_debug(dev, DL_WARN, "%s: ATM - An extra BL may be needed\n",
				__func__);
			/* Free allocated memory */
			index = 0;
			while (index < MAX_FILE_NAMES)
				kfree(filename[index++]);
			/* Reset index to 0 */
			index = 0;
			mutex_unlock(&cd->firmware_class_lock);

			/* Panel_ID coming from sysinfo, ensure we have it */
			ld->si = cmd->request_sysinfo(dev);
			goto retry_bl;
		}
		retval = ld->builtin_bin_fw_status;
	}

	index = 0;
	while (index < MAX_FILE_NAMES)
		kfree(filename[index++]);

	mutex_unlock(&cd->firmware_class_lock);
	return retval;
}
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */

#if PT_TTCONFIG_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_write_config_row_
 *
 * SUMMARY: Alow to program the data block area that includes configuration
 *  data, manufacturing data, design data.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev         - pointer to device structure
 *   ebid        - block id to determine the block name(configuration,etc)
 *   row_number  - row number if written block
 *   row_size    - row size of written data
 *  *data        - pointer to the data to write
 ******************************************************************************/
static int pt_write_config_row_(struct device *dev, u8 ebid,
		u16 row_number, u16 row_size, u8 *data)
{
	int rc;
	u16 actual_write_len;

	rc = cmd->nonhid_cmd->write_data_block(dev, row_number,
			row_size, ebid, data, (u8 *)pt_data_block_security_key,
			&actual_write_len);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Fail Put EBID=%d row=%d cmd fail r=%d\n",
			__func__, ebid, row_number, rc);
		return rc;
	}

	if (actual_write_len != row_size) {
		pt_debug(dev, DL_ERROR,
			"%s: Fail Put EBID=%d row=%d wrong write size=%d\n",
			__func__, ebid, row_number, actual_write_len);
		rc = -EINVAL;
	}

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_upgrade_ttconfig
 *
 * SUMMARY: Program ttconfig_data with following steps:
 *  1) Suspend scanning
 *  2) Write data to the data block
 *  3) Verify the crc for data block
 *  4) Resume scanning
 *  5) Set up call back for calibration if required
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev            - pointer to device structure
 *  *ttconfig_data  - pointer to the config data to write to data block
 *   ttconfig_size  - size of config data to write
 ******************************************************************************/
static int pt_upgrade_ttconfig(struct device *dev,
		const u8 *ttconfig_data, int ttconfig_size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	bool wait_for_calibration_complete = false;
	u8 ebid = PT_TCH_PARM_EBID;
	u16 row_size = PT_DATA_ROW_SIZE;
	u16 table_size;
	u16 row_count;
	u16 residue;
	u8 *row_buf;
	u8 verify_crc_status;
	u16 calculated_crc;
	u16 stored_crc;
	int rc = 0;
	int i;

	table_size = ttconfig_size;
	row_count = table_size / row_size;
	row_buf = (u8 *)ttconfig_data;
	pt_debug(dev, DL_INFO, "%s: size:%d row_size=%d row_count=%d\n",
		__func__, table_size, row_size, row_count);

	pm_runtime_get_sync(dev);

	rc = cmd->request_exclusive(dev, PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		goto exit;

	rc = cmd->nonhid_cmd->suspend_scanning(dev, 0);
	if (rc < 0)
		goto release;

	for (i = 0; i < row_count; i++) {
		pt_debug(dev, DL_INFO, "%s: row=%d size=%d\n",
			__func__, i, row_size);
		rc = pt_write_config_row_(dev, ebid, i, row_size,
				row_buf);
		if (rc) {
			pt_debug(dev, DL_ERROR, "%s: Fail put row=%d r=%d\n",
				__func__, i, rc);
			break;
		}
		row_buf += row_size;
	}
	if (!rc) {
		residue = table_size % row_size;
		pt_debug(dev, DL_WARN, "%s: row=%d size=%d\n",
			__func__, i, residue);
		rc = pt_write_config_row_(dev, ebid, i, residue,
				row_buf);
		row_count++;
		if (rc)
			pt_debug(dev, DL_ERROR, "%s: Fail put row=%d r=%d\n",
				__func__, i, rc);
	}

	if (!rc)
		pt_debug(dev, DL_WARN,
			"%s: TT_CFG updated: rows:%d bytes:%d\n",
			__func__, row_count, table_size);

	rc = cmd->nonhid_cmd->verify_cfg_block_crc(dev, 0, ebid,
			&verify_crc_status, &calculated_crc, &stored_crc);
	if (rc || verify_crc_status)
		pt_debug(dev, DL_ERROR,
			"%s: CRC Failed, ebid=%d, status=%d, scrc=%X ccrc=%X\n",
			__func__, ebid, verify_crc_status,
			calculated_crc, stored_crc);
	else
		pt_debug(dev, DL_INFO,
			"%s: CRC PASS, ebid=%d, status=%d, scrc=%X ccrc=%X\n",
			__func__, ebid, verify_crc_status,
			calculated_crc, stored_crc);

	rc = cmd->nonhid_cmd->resume_scanning(dev, 0);
	if (rc < 0)
		goto release;

	if (ld->loader_pdata &&
			(ld->loader_pdata->flags
			 & PT_LOADER_FLAG_CALIBRATE_AFTER_TTCONFIG_UPGRADE)) {
#if (KERNEL_VERSION(3, 13, 0) <= LINUX_VERSION_CODE)
		reinit_completion(&ld->calibration_complete);
#else
		INIT_COMPLETION(ld->calibration_complete);
#endif
		/* set up call back for startup */
		pt_debug(dev, DL_INFO, "%s: Adding callback for calibration\n",
			__func__);
		rc = cmd->subscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_LOADER_NAME, pt_calibration_attention, 0);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Failed adding callback for calibration\n",
				__func__);
			pt_debug(dev, DL_ERROR,
				"%s: No calibration will be performed\n",
				__func__);
			rc = 0;
		} else
			wait_for_calibration_complete = true;
	}

release:
	cmd->release_exclusive(dev);

exit:
	if (!rc)
		cmd->request_enum(dev, true);

	pm_runtime_put_sync(dev);

	if (wait_for_calibration_complete)
		wait_for_completion(&ld->calibration_complete);

	return rc;
}
#endif /* PT_TTCONFIG_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_TTCONFIG_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_get_ttconfig_crc
 *
 * SUMMARY: Get crc from ttconfig data.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev            - pointer to device structure
 *  *ttconfig_data  - pointer to the config data
 *   ttconfig_size  - size of config data
 *  *crc            - pointer to the crc of configure to be stored
 ******************************************************************************/
static int pt_get_ttconfig_crc(struct device *dev,
		const u8 *ttconfig_data, int ttconfig_size, u16 *crc)
{
	u16 crc_loc;

	crc_loc = get_unaligned_le16(&ttconfig_data[2]);
	if (ttconfig_size < crc_loc + 2)
		return -EINVAL;

	*crc = get_unaligned_le16(&ttconfig_data[crc_loc]);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_get_ttconfig_version
 *
 * SUMMARY: Get version number from ttconfig data.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev            - pointer to device structure
 *  *ttconfig_data  - pointer to the config data
 *   ttconfig_size  - size of config data
 *  *version        - pointer to the version of configure to be stored
 ******************************************************************************/
static int pt_get_ttconfig_version(struct device *dev,
		const u8 *ttconfig_data, int ttconfig_size, u16 *version)
{
	if (ttconfig_size < PT_TTCONFIG_VERSION_OFFSET
			+ PT_TTCONFIG_VERSION_SIZE)
		return -EINVAL;

	*version = get_unaligned_le16(
		&ttconfig_data[PT_TTCONFIG_VERSION_OFFSET]);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_check_ttconfig_version
 *
 * SUMMARY: Check the configure version and crc value to determine whether to
 *  upgrade,the upgrade conditions as followings:
 *      1) To upgrade if the config version is newer than current config, but
 *         this check is based on the flag in loader plarform data.
 *      2) To upgrade if config CRC is different.
 *      3) Don't upgrade when can't match any of above conditions.
 *
 * RETURN:
 *   0: Don't upgrade
 *   1: Upgrade
 *
 * PARAMETERS:
 *  *dev            - pointer to device structure
 *  *ttconfig_data  - pointer to the config data
 *   ttconfig_size  - size of config data
 ******************************************************************************/
static int pt_check_ttconfig_version(struct device *dev,
		const u8 *ttconfig_data, int ttconfig_size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u16 cfg_crc_new;
	int rc;

	if (!ld->si)
		return 0;

	/* Check for config version */
	if (ld->loader_pdata->flags &
			PT_LOADER_FLAG_CHECK_TTCONFIG_VERSION) {
		u16 cfg_ver_new;

		rc = pt_get_ttconfig_version(dev, ttconfig_data,
				ttconfig_size, &cfg_ver_new);
		if (rc)
			return 0;

		pt_debug(dev, DL_INFO, "%s: img_ver:0x%04X new_ver:0x%04X\n",
			__func__, ld->si->ttdata.fw_ver_conf, cfg_ver_new);

		/* Check if config version is newer */
		if (cfg_ver_new > ld->si->ttdata.fw_ver_conf) {
			pt_debug(dev, DL_WARN,
			"%s: Config version newer, will upgrade\n", __func__);
			return 1;
		}

		pt_debug(dev, DL_WARN,
			"%s: Config version is identical or older, will NOT upgrade\n",
			__func__);
	/* Check for config CRC */
	} else {
		rc = pt_get_ttconfig_crc(dev, ttconfig_data,
				ttconfig_size, &cfg_crc_new);
		if (rc)
			return 0;

		pt_debug(dev, DL_INFO, "%s: img_crc:0x%04X new_crc:0x%04X\n",
			__func__, ld->si->ttconfig.crc, cfg_crc_new);

		if (cfg_crc_new != ld->si->ttconfig.crc) {
			pt_debug(dev, DL_WARN,
				"%s: Config CRC different, will upgrade\n",
				__func__);
			return 1;
		}

		pt_debug(dev, DL_WARN,
			"%s: Config CRC equal, will NOT upgrade\n", __func__);
	}

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_check_ttconfig_version_platform
 *
 * SUMMARY: To call the function pt_check_ttconfig_version() to determine
 *  whether to load config if the firmware version match with current firmware.
 *
 * RETURN:
 *   0: Don't upgrade
 *   1: Upgrade
 *
 * PARAMETERS:
 *  *dev       - pointer to device structure
 *  *ttconfig  - pointer to touch_config structure
 ******************************************************************************/
static int pt_check_ttconfig_version_platform(struct device *dev,
		struct pt_touch_config *ttconfig)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u32 fw_ver_config;
	u32 fw_revctrl_config;

	if (!ld->si) {
		pt_debug(dev, DL_INFO,
			"%s: No firmware info found, DUT FW may be corrupted\n",
			__func__);
		return 0;
	}

	fw_ver_config = get_unaligned_be16(ttconfig->fw_ver + 2);
	/* 4 middle bytes are not used */
	fw_revctrl_config = get_unaligned_be32(ttconfig->fw_ver + 8);

	/* FW versions should match */
	if (pt_check_firmware_version(dev, fw_ver_config,
			fw_revctrl_config)) {
		pt_debug(dev, DL_ERROR,
			"%s: FW versions mismatch\n", __func__);
		return 0;
	}

	/* Check PowerOn Self Test, TT_CFG CRC bit */
	if ((ld->si->ttdata.post_code & PT_POST_TT_CFG_CRC_MASK) == 0) {
		pt_debug(dev, DL_ERROR,
			"%s: POST, TT_CFG failed (%X), will upgrade\n",
			__func__, ld->si->ttdata.post_code);
		return 1;
	}

	return pt_check_ttconfig_version(dev, ttconfig->param_regs->data,
			ttconfig->param_regs->size);
}

/*******************************************************************************
 * FUNCTION: pt_get_platform_ttconfig
 *
 * SUMMARY: To get the pointer of right touch_config structure by panel id.
 *
 * RETURN:
 *   pointer to touch_config structure or null pointer if fail
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static struct pt_touch_config *pt_get_platform_ttconfig(
		struct device *dev)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_touch_config **ttconfigs;
	struct pt_touch_config *ttconfig;
	u8 panel_id;

	panel_id = pt_get_panel_id(dev);
	if (panel_id == PANEL_ID_NOT_ENABLED) {
		/* TODO: Make debug message */
		pt_debug(dev, DL_INFO,
			"%s: Panel ID not enabled, using legacy ttconfig\n",
			__func__);
		return ld->loader_pdata->ttconfig;
	}

	ttconfigs = ld->loader_pdata->ttconfigs;
	if (!ttconfigs)
		return NULL;

	/* Find TT config according to the Panel ID */
	while ((ttconfig = *ttconfigs++)) {
		if (ttconfig->panel_id == panel_id) {
			/* TODO: Make debug message */
			pt_debug(dev, DL_INFO,
				"%s: Found matching ttconfig:%p with Panel ID: 0x%02X\n",
				__func__, ttconfig, ttconfig->panel_id);
			return ttconfig;
		}
		pt_debug(dev, DL_ERROR,
			"%s: Found mismatching ttconfig:%p with Panel ID: 0x%02X\n",
			__func__, ttconfig, ttconfig->panel_id);
	}

	return NULL;
}

/*******************************************************************************
 * FUNCTION: upgrade_ttconfig_from_platform
 *
 * SUMMARY: Get touch_firmware structure and perform upgrade if pass the
 *  firmware version check.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev     - pointer to device structure
 *   forced  - flag to force upgrade(1:force to upgrade)
 ******************************************************************************/
static int upgrade_ttconfig_from_platform(struct device *dev)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_touch_config *ttconfig;
	struct touch_settings *param_regs;

	int rc = -ENODEV;
	int upgrade;

	if (!ld->loader_pdata) {
		pt_debug(dev, DL_ERROR,
			"%s: No loader platform data\n", __func__);
		return rc;
	}

	ttconfig = pt_get_platform_ttconfig(dev);
	if (!ttconfig) {
		pt_debug(dev, DL_ERROR, "%s: No ttconfig data\n", __func__);
		return rc;
	}

	param_regs = ttconfig->param_regs;
	if (!param_regs) {
		pt_debug(dev, DL_ERROR, "%s: No touch parameters\n",
			__func__);
		return rc;
	}

	if (!param_regs->data || !param_regs->size) {
		pt_debug(dev, DL_ERROR,
			"%s: Invalid touch parameters\n", __func__);
		return rc;
	}

	if (!ttconfig->fw_ver || !ttconfig->fw_vsize) {
		pt_debug(dev, DL_ERROR,
			"%s: Invalid FW version for touch parameters\n",
			__func__);
		return rc;
	}

	upgrade = pt_check_ttconfig_version_platform(dev, ttconfig);
	if (upgrade)
		return pt_upgrade_ttconfig(dev, param_regs->data,
				param_regs->size);

	return rc;
}
#endif /* CONFIG_TOUCHSCREEN_PARADE_PLATFORM_TTCONFIG_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_config_data_write
 *
 * SUMMARY: The write method for the config_data sysfs node. The passed
 *  in data (config file) is written to the config_data buffer.
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
static ssize_t pt_config_data_write(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct pt_loader_data *data = pt_get_loader_data(dev);
	u8 *p;

	pt_debug(dev, DL_INFO, "%s: offset:%lld count:%zu\n",
		__func__, offset, count);

	mutex_lock(&data->config_lock);

	if (!data->config_loading) {
		mutex_unlock(&data->config_lock);
		return -ENODEV;
	}

	p = krealloc(data->config_data, offset + count, GFP_KERNEL);
	if (!p) {
		kfree(data->config_data);
		data->config_data = NULL;
		mutex_unlock(&data->config_lock);
		return -ENOMEM;
	}
	data->config_data = p;

	memcpy(&data->config_data[offset], buf, count);
	data->config_size += count;

	mutex_unlock(&data->config_lock);

	return count;
}

static struct bin_attribute bin_attr_config_data = {
	.attr = {
		.name = "config_data",
		.mode = 0200,
	},
	.size = 0,
	.write = pt_config_data_write,
};

/*******************************************************************************
 * FUNCTION: pt_verify_ttconfig_binary
 *
 * SUMMARY: Perform a simple size check if the firmware version match.And
 *  calculate the start pointer of config data to write and the size to write.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *   *dev              - pointer to device structure
 *   *bin_config_data  - pointer to binary config data
 *    bin_config_size  - size of binary config data
 *  **start            - double pointer to config data where to be written
 *   *len              - pointer to the size of config data to store
 ******************************************************************************/
static int pt_verify_ttconfig_binary(struct device *dev,
		u8 *bin_config_data, int bin_config_size, u8 **start, int *len)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	int header_size;
	u16 config_size;
	u32 fw_ver_config;
	u32 fw_revctrl_config;

	if (!ld->si) {
		pt_debug(dev, DL_ERROR,
			"%s: No firmware info found, DUT FW may be corrupted\n",
			__func__);
		return -ENODEV;
	}

	/*
	 * We need 11 bytes for FW version control info and at
	 * least 6 bytes in config (Length + Max Length + CRC)
	 */
	header_size = bin_config_data[0] + 1;
	if (header_size < 11 || header_size >= bin_config_size - 6) {
		pt_debug(dev, DL_ERROR,
			"%s: Invalid header size %d\n", __func__,
			header_size);
		return -EINVAL;
	}

	fw_ver_config = get_unaligned_be16(&bin_config_data[1]);
	/* 4 middle bytes are not used */
	fw_revctrl_config = get_unaligned_be32(&bin_config_data[7]);

	/* FW versions should match */
	if (pt_check_firmware_version(dev, fw_ver_config,
			fw_revctrl_config)) {
		pt_debug(dev, DL_ERROR,
			"%s: FW versions mismatch\n", __func__);
		return -EINVAL;
	}

	config_size = get_unaligned_le16(&bin_config_data[header_size]);
	/* Perform a simple size check (2 bytes for CRC) */
	if (config_size != bin_config_size - header_size - 2) {
		pt_debug(dev, DL_ERROR,
			"%s: Config size invalid\n", __func__);
		return -EINVAL;
	}

	*start = &bin_config_data[header_size];
	*len = bin_config_size - header_size;

	return 0;
}

/*
 * 1: Start loading TT Config
 * 0: End loading TT Config and perform upgrade
 *-1: Exit loading
 */

/*******************************************************************************
 * FUNCTION: pt_config_loading_store
 *
 * SUMMARY: The store method for the config_loading sysfs node. The
 *  passed in value controls if config loading is performed.
 *
 * RETURN: Size of passed in buffer is success
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 *  *attr - pointer to device attributes
 *  *buf  - pointer to buffer that hold the command parameters
 *   size - size of buf
 ******************************************************************************/
static ssize_t pt_config_loading_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	long value;
	u8 *start;
	int length;
	int rc;

	rc = kstrtol(buf, 10, &value);
	if (rc < 0 || value < -1 || value > 1) {
		pt_debug(dev, DL_ERROR, "%s: Invalid value\n", __func__);
		return size;
	}

	mutex_lock(&ld->config_lock);

	if (value == 1)
		ld->config_loading = true;
	else if (value == -1)
		ld->config_loading = false;
	else if (value == 0 && ld->config_loading) {
		ld->config_loading = false;
		if (ld->config_size == 0) {
			pt_debug(dev, DL_ERROR,
				"%s: No config data\n", __func__);
			goto exit_free;
		}

		rc = pt_verify_ttconfig_binary(dev,
				ld->config_data, ld->config_size,
				&start, &length);
		if (rc)
			goto exit_free;

		rc = pt_upgrade_ttconfig(dev, start, length);
	}

exit_free:
	kfree(ld->config_data);
	ld->config_data = NULL;
	ld->config_size = 0;

	mutex_unlock(&ld->config_lock);

	if (rc)
		return rc;

	return size;
}

static DEVICE_ATTR(config_loading, 0200,
	NULL, pt_config_loading_store);
#endif /* CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_forced_upgrade_store
 *
 * SUMMARY: The store method for the forced_upgrade sysfs node. The firmware
 *  loading is forced to performed with platform upgrade strategy.
 *
 * RETURN: Size of passed in buffer is success
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 *  *attr - pointer to device attributes
 *  *buf  - pointer to buffer that hold the command parameters
 *   size - size of buf
 ******************************************************************************/
static ssize_t pt_forced_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int rc = upgrade_firmware_from_platform(dev, true);

	if (rc)
		return rc;
	return size;
}

static DEVICE_ATTR(forced_upgrade, 0200,
	NULL, pt_forced_upgrade_store);
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_manual_upgrade_store
 *
 * SUMMARY: The store method for the forced_upgrade sysfs node that it is
 *  caller for function upgrade_firmware_from_class() to allow upgrade firmware
 *  manually.
 *
 * RETURN: Size of passed in buffer is success
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 *  *attr - pointer to device attributes
 *  *buf  - pointer to buffer that hold the command parameters
 *   size - size of buf
 ******************************************************************************/
static ssize_t pt_manual_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u32 input_data[2] = {0};
	int length;
	int rc = 0;

	length = cmd->parse_sysfs_input(dev, buf, size, input_data,
			ARRAY_SIZE(input_data));

	if (length != 1) {
		pt_debug(dev, DL_WARN, "%s: Invalid number of arguments\n",
			__func__);
		rc = -EINVAL;
		goto exit;
	}

	if (input_data[0] < 0 || input_data[0] > 1) {
		pt_debug(dev, DL_WARN, "%s: Invalid arguments\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	_pt_pip2_update_bl_status(dev, UPDATE_FW_IDLE, PT_NO_INC);
	if (ld->is_manual_upgrade_enabled) {
		rc = -EBUSY;
		goto exit;
	}

	ld->is_manual_upgrade_enabled = 1;

	rc = upgrade_firmware_from_class(ld->dev);
	if (rc < 0)
		ld->is_manual_upgrade_enabled = 0;

exit:
	if (rc)
		return rc;
	return size;
}

static DEVICE_ATTR(manual_upgrade, 0200, NULL, pt_manual_upgrade_store);
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */

/*******************************************************************************
 * FUNCTION: pt_fw_and_config_upgrade
 *
 * SUMMARY: Perform all methods for firmware upgrade and config upgrade
 *  according to the definition of macro.
 *
 * PARAMETERS:
 *  *work_struct  - pointer to work_struct structure
 ******************************************************************************/
static void pt_fw_and_config_upgrade(
		struct work_struct *fw_and_config_upgrade)
{
	struct pt_loader_data *ld = container_of(fw_and_config_upgrade,
			struct pt_loader_data, fw_and_config_upgrade);
	struct device *dev = ld->dev;
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int retry = 200;
#if PT_FW_UPGRADE \
	|| defined(CONFIG_TOUCHSCREEN_PARADE_PLATFORM_TTCONFIG_UPGRADE)
	u8 dut_gen = cmd->request_dut_generation(dev);
#endif

	/*
	 * Since the resume_scan command in pt_probe_complete() has
	 * no protection,it may cause problem for the commands in fw
	 * upgrade process druing probe. Waiting for the probe to
	 * complete before performing fw upgrade can avoid this failure.
	 */
	while (!cd->core_probe_complete && retry--)
		msleep(20);

	ld->si = cmd->request_sysinfo(dev);
	if (!ld->si)
		pt_debug(dev, DL_ERROR,
			"%s: Fail get sysinfo pointer from core\n",
			__func__);
#if !PT_FW_UPGRADE
	pt_debug(dev, DL_INFO,
		"%s: No FW upgrade method selected!\n", __func__);
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE
	if (dut_gen == DUT_PIP1_ONLY) {
		if (!upgrade_firmware_from_platform(dev, false))
			return;
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	if (dut_gen == DUT_PIP2_CAPABLE) {
		if (!pt_pip2_upgrade_firmware_from_builtin(dev))
			return;
		pt_debug(dev, DL_WARN, "%s: Builtin FW upgrade failed\n",
			__func__);
	} else {
		if (!upgrade_firmware_from_builtin(dev))
			return;
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_TTCONFIG_UPGRADE
	if (dut_gen == DUT_PIP1_ONLY) {
		if (!upgrade_ttconfig_from_platform(dev))
			return;
	}
#endif
}

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
#ifdef TTDL_DIAGNOSTICS
/*******************************************************************************
 * FUNCTION: _pt_pip2_get_flash_info
 *
 * SUMMARY: Sends a FLASH_INFO command to the DUT logging the results to kmsg
 *
 * PARAMETERS:
 *	*dev - pointer to device structure
 *	*read_buf - pointer to the read buffer array to store the response
 ******************************************************************************/
static void _pt_pip2_get_flash_info(struct device *dev,
	u8 *read_buf)
{
	u16 actual_read_len;
	int ret;

	/* Get flash info for debugging information */
	ret = cmd->nonhid_cmd->pip2_send_cmd(dev,
		PT_CORE_CMD_UNPROTECTED, PIP2_CMD_ID_FLASH_INFO,
		NULL, 0, read_buf, &actual_read_len);
	if (!ret) {
		pt_debug(dev, DL_DEBUG,
			"%s --- FLASH Information ---\n", __func__);
		pt_debug(dev, DL_DEBUG,
			"%s Manufacturer ID: 0x%02x\n",
			__func__, read_buf[PIP2_RESP_BODY_OFFSET]);
		pt_debug(dev, DL_DEBUG,
			"%s Memory Type    : 0x%02x\n",
			__func__, read_buf[PIP2_RESP_BODY_OFFSET + 1]);
		pt_debug(dev, DL_DEBUG,
			"%s Num Sectors    : 0x%02x%02x%02x%02x\n",
			__func__, read_buf[PIP2_RESP_BODY_OFFSET + 2],
			read_buf[PIP2_RESP_BODY_OFFSET + 3],
			read_buf[PIP2_RESP_BODY_OFFSET + 4],
			read_buf[PIP2_RESP_BODY_OFFSET + 5]);
		pt_debug(dev, DL_DEBUG,
			"%s Sectors Size   : 0x%02x%02x%02x%02x\n",
			__func__, read_buf[PIP2_RESP_BODY_OFFSET + 6],
			read_buf[PIP2_RESP_BODY_OFFSET + 7],
			read_buf[PIP2_RESP_BODY_OFFSET + 8],
			read_buf[PIP2_RESP_BODY_OFFSET + 9]);
		pt_debug(dev, DL_DEBUG,
			"%s Page Size      : 0x%02x%02x%02x%02x\n",
			__func__, read_buf[PIP2_RESP_BODY_OFFSET + 10],
			read_buf[PIP2_RESP_BODY_OFFSET + 11],
			read_buf[PIP2_RESP_BODY_OFFSET + 12],
			read_buf[PIP2_RESP_BODY_OFFSET + 13]);
		if (actual_read_len > 21) {
			pt_debug(dev, DL_DEBUG,
				"%s Status Reg1    : 0x%02x\n",
				__func__,
				read_buf[PIP2_RESP_BODY_OFFSET + 14]);
			pt_debug(dev, DL_DEBUG,
				"%s Status Reg2    : 0x%02x\n",
				__func__,
				read_buf[PIP2_RESP_BODY_OFFSET + 15]);
		}
	}
}
#endif /* TTDL_DIAGNOSTICS */

/*******************************************************************************
 * FUNCTION: _pt_pip2_log_last_error
 *
 * SUMMARY: Sends a STATUS command to the DUT logging the results until all
 *	errors are cleared. Also sends a GET_LAST_ERRNO to get any Boot errors.
 *	This must be sent after the STATUS flush in order not to have this
 *	command cause another error.
 *
 * NOTE: This function support No Interrupt Solution and switch "pip2_send_cmd"
 *  function according to the global variable "bl_with_no_int".
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev - pointer to device structure
 *	*read_buf - pointer to the read buffer array to store the response
 ******************************************************************************/
static int _pt_pip2_log_last_error(struct device *dev,
	u8 *read_buf)
{
	u16 actual_read_len;
	u8 loop = 5;
	u8 info = 0xFF;
	u8 error = 0xFF;
	int ret;
	PIP2_SEND_CMD pip2_send_cmd;
	struct pt_core_data *cd = dev_get_drvdata(dev);

	if (cd->bl_with_no_int)
		pip2_send_cmd = cmd->nonhid_cmd->pip2_send_cmd_no_int;
	else
		pip2_send_cmd = cmd->nonhid_cmd->pip2_send_cmd;
	/*
	 * Send the STATUS command until no errors are found.
	 * The BL will store an error code for each layer of the stack,
	 * and each read will return one error.
	 */
	while (loop > 0 && error) {

		ret = pip2_send_cmd(dev,
			PT_CORE_CMD_UNPROTECTED, PIP2_CMD_ID_STATUS,
			NULL, 0, read_buf, &actual_read_len);

		if (!ret) {
			info  = (u8)read_buf[PIP2_RESP_BODY_OFFSET];
			error = (u8)read_buf[PIP2_RESP_BODY_OFFSET + 1];

			pt_debug(dev, DL_ERROR,
				"%s: STATUS: Status=0x%02X BOOT=%d BUSY=%d INT=%d ERR_PHY=%d ERR_REG=%d ERROR=0x%02X",
				__func__,
				(u8)read_buf[PIP2_RESP_STATUS_OFFSET],
				info & 0x01,
				(info & 0x02) >> 1,
				(info & 0x04) >> 2,
				(info & 0x18) >> 3,
				(info & 0xE0) >> 5,
				error);
		}
		loop--;
	}

	/* Send the GET_LAST_ERROR command to get the last BL startup error */
	ret = pip2_send_cmd(dev,
		PT_CORE_CMD_UNPROTECTED, PIP2_CMD_ID_GET_LAST_ERRNO,
		NULL, 0, read_buf, &actual_read_len);
	if (!ret) {
		pt_debug(dev, DL_ERROR,
			"%s: GET_LAST_ERR: Status=0x%02X ERRNO=0x%02X BOOTMODE=%d\n",
			__func__,
			(u8)read_buf[PIP2_RESP_STATUS_OFFSET],
			(u8)read_buf[PIP2_RESP_BODY_OFFSET],
			(u8)read_buf[PIP2_RESP_BODY_OFFSET + 1]);
	}

	return ret;
}

/*******************************************************************************
 * FUNCTION: _pt_pip2_check_fw_ver
 *
 * SUMMARY: Compare the FW version in the bin file to the current FW. If the
 *	FW version in the bin file is greater an upgrade should be done.
 *
 * RETURN:
 *  -1: Do not upgrade firmware
 *   0: Version info same, let caller decide
 *   1: Do a firmware upgrade
 *
 * PARAMETERS:
 *	*dev - pointer to device structure
 *	*fw  - pointer to the new FW image to load
 *	*hdr - pointer to the PIP2 bin file hdr structure
 ******************************************************************************/
static int _pt_pip2_check_fw_ver(struct device *dev,
	const struct firmware *fw, struct pt_bin_file_hdr *hdr)
{
	u8 img_app_major_ver = fw->data[3];
	u8 img_app_minor_ver = fw->data[4];
	u32 img_app_rev_ctrl  = fw->data[9]<<24 | fw->data[10]<<16 |
			    fw->data[11]<<8 | fw->data[12];

	pt_debug(dev, DL_WARN,
		"%s ATM - BL Image Version:   %02x.%02x.%d\n",
		__func__, img_app_major_ver, img_app_minor_ver,
		img_app_rev_ctrl);

	pt_debug(dev, DL_WARN,
		"%s ATM - Current FW Version: %02X.%02X.%d\n",
		__func__, hdr->fw_major, hdr->fw_minor, hdr->fw_rev_ctrl);

	if ((256 * img_app_major_ver + img_app_minor_ver) >
	    (256 * hdr->fw_major + hdr->fw_minor)) {
		pt_debug(dev, DL_WARN,
			"ATM - bin file version > FW, will upgrade FW");
		return 1;
	}

	if ((256 * img_app_major_ver + img_app_minor_ver) <
	    (256 * hdr->fw_major + hdr->fw_minor)) {
		pt_debug(dev, DL_WARN,
			"ATM - bin file version < FW, will NOT upgrade FW");
		return -1;
	}

	if (img_app_rev_ctrl > hdr->fw_rev_ctrl) {
		pt_debug(dev, DL_WARN,
			"bin file rev ctrl > FW, will upgrade FW");
		return 1;
	}

	if (img_app_rev_ctrl < hdr->fw_rev_ctrl) {
		pt_debug(dev, DL_WARN,
			"bin file rev ctrl > FW, will NOT upgrade FW");
		return -1;
	}

	return 0;
}

/*******************************************************************************
 * FUNCTION: _pt_pip2_check_config_ver
 *
 * SUMMARY: Compare the Config version in the bin file to the current FW. If the
 *	config version in the bin file is greater an upgrade should be done.
 *
 * RETURN:
 *  -1: Do not upgrade firmware
 *   0: Version info same, let caller decide
 *   1: Do a firmware upgrade
 *
 * PARAMETERS:
 *	*dev - pointer to device structure
 *	*fw  - pointer to the new FW image to load
 *	*hdr - pointer to the PIP2 bin file hdr structure
 ******************************************************************************/
static int _pt_pip2_check_config_ver(struct device *dev,
	const struct firmware *fw, struct pt_bin_file_hdr *hdr)
{
	u16 fw_config_ver_new;

	/* Offset 17,18 is the TT Config version*/
	fw_config_ver_new = get_unaligned_be16(fw->data + 17);
	pt_debug(dev, DL_WARN,
		"%s ATM - BL Image Config Version:   %d\n",
		__func__, fw_config_ver_new);

	pt_debug(dev, DL_WARN,
		"%s ATM - Current Config Version:    %d\n",
		__func__, hdr->config_ver);

	if (fw_config_ver_new > hdr->config_ver) {
		pt_debug(dev, DL_WARN,
			"ATM - bin file Config version > FW, will upgrade FW");
		return 1;
	} else
		pt_debug(dev, DL_WARN,
			"ATM - bin file Config version <= FW, will NOT upgrade FW");

	return 0;
}

/*******************************************************************************
 * FUNCTION: _pt_pip2_need_upgrade_due_to_fw_ver
 *
 * SUMMARY: Compare the FW version in the bin file to the current FW. If the
 *	FW version in the bin file is greater an upgrade should be done.
 *
 * PARAMETERS:
 *	*dev - pointer to device structure
 *	*fw  - pointer to the new FW image to load
 ******************************************************************************/
static u8 _pt_pip2_need_upgrade_due_to_fw_ver(struct device *dev,
	const struct firmware *fw)
{
	int ret;
	u8 should_upgrade = 0;
	struct pt_bin_file_hdr hdr = {0};

	ret = cmd->request_pip2_bin_hdr(dev, &hdr);
	if (ret != 0 || hdr.fw_major == 0xFF) {
		pt_debug(dev, DL_WARN,
			"App ver info not available, will upgrade FW");
		should_upgrade = 1;
		goto exit;
	}

	ret = _pt_pip2_check_fw_ver(dev, fw, &hdr);
	if (ret == 0)
		ret = _pt_pip2_check_config_ver(dev, fw, &hdr);

	if (ret > 0)
		should_upgrade = 1;

exit:
	return should_upgrade;
}

/*******************************************************************************
 * FUNCTION: _pt_calibrate_flashless_dut
 *
 * SUMMARY: On a flashless DUT the FW would need to re-calibrate on every power
 *	cycle, so to speed up the BL process the FW does the calibraiton on the
 *	first power up and TTDL will read and store it in RAM to be able to
 *	restore it on subsequent resets of the DUT.
 *
 * RETURN:
 *       0 = success
 *      !0 = failure
 *
 * PARAMETERS:
 *	*dev              - pointer to device structure
 ******************************************************************************/
static int _pt_calibrate_flashless_dut(struct device *dev)
{
	u8 rc = 0;
	u8 cal_status = 1;
	u16 cal_size = 0;
	struct pt_cal_ext_data cal_data = {0};
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_ttdata *ttdata = &cd->sysinfo.ttdata;
	unsigned short cache_chip_id = 0;
	unsigned short active_chip_id = 0;

	cmd->nonhid_cmd->manage_cal_data(dev, PT_CAL_DATA_INFO, &cal_size,
		&cache_chip_id);
	pt_debug(dev, DL_WARN, "%s: Stored CAL   ID=0x%04X size=%d\n",
		__func__, cache_chip_id, cal_size);

	active_chip_id = cmd->nonhid_cmd->calc_crc((u8 *)&ttdata->chip_rev,
			4 + PT_UID_SIZE);
	pt_debug(dev, DL_WARN, "%s: Current Chip ID=0x%04X\n",
		__func__, active_chip_id);

	if (cal_size == 0 || active_chip_id != cache_chip_id) {
		memset(&cal_data, 0, sizeof(struct pt_cal_ext_data));

		/* Calibrate_ext will also save CAL Data in TTDL cache */
		rc = cmd->nonhid_cmd->calibrate_ext(dev, 0, &cal_data,
			&cal_status);
		pt_debug(dev, DL_INFO,
			"%s: Calibration Finished rc=%d\n", __func__, rc);

		/* Afer successful calibration read the stored size */
		if (!rc && cal_status == 0) {
			rc = cmd->nonhid_cmd->manage_cal_data(dev,
				PT_CAL_DATA_INFO, &cal_size, &cache_chip_id);
			if (!rc) {
				pt_debug(dev, DL_WARN,
					"%s: First BL, Read %d Bytes of CAL\n",
					__func__, cal_size);
			} else {
				pt_debug(dev, DL_ERROR,
					"%s: First BL, Failed to read CAL\n",
					__func__);
			}
		} else {
			pt_debug(dev, DL_ERROR,
				"%s: First BL, Calibration failed rc=%d\n",
				__func__, rc);
		}
		rc = cmd->nonhid_cmd->resume_scanning(dev, 0);
		if (rc)
			pt_debug(dev, DL_ERROR,
				"%s: First BL, Resume Scan failed rc=%d\n",
				__func__, rc);
	} else {
		rc = cmd->nonhid_cmd->manage_cal_data(dev, PT_CAL_DATA_RESTORE,
			&cal_size, &cache_chip_id);
		if (!rc)
			pt_debug(dev, DL_WARN, "%s: Restored CAL %d Bytes\n",
				__func__, cal_size);
		else
			pt_debug(dev, DL_ERROR, "%s: Failed to restore CAL\n",
				__func__);

		rc = cmd->nonhid_cmd->resume_scanning(dev, 0);
		if (rc)
			pt_debug(dev, DL_ERROR,
				"%s: Resume Scan failed rc=%d\n",
				__func__, rc);
	}
	return rc;
}

/*******************************************************************************
 * FUNCTION: _pt_pip2_firmware_cont
 *
 * SUMMARY: Bootload the DUT with a FW image using the PIP2 protocol. This
 *	includes getting the DUT into BL mode, writing the file to either SRAM
 *	or FLASH, and launching the application directly in SRAM or by resetting
 *	the DUT without the hostmode pin asserted.
 *
 *	NOTE: Special care must be taken to support a DUT communicating in
 *		PIP2.0 where the length field is defined differently.
 *	NOTE: The write packet len is set so that the overall packet size is
 *		less than 255. The overhead is 9 bytes: 2 byte address (0101),
 *		4 byte header, 1 byte file no. 2 byte CRC
 *
 * PARAMETERS:
 *	*fw      - pointer to the new FW image to load
 *	*context - pointer to the device
 ******************************************************************************/
static void _pt_pip2_firmware_cont(const struct firmware *fw,
		void *context)
{
	u8 read_buf[PT_MAX_PIP2_MSG_SIZE];
	u8 buf[PT_MAX_PIP2_MSG_SIZE];
	u8 *fw_img = NULL;
	u16 write_len;
	u8 mode = PT_MODE_UNKNOWN;
	u8 retry_packet = 0;
	u8 us_fw_used = 0;
	u16 actual_read_len;
	u16 status = 0;
	u16 packet_size;
	int fw_size = 0;
	int remain_bytes;
	int ret = 0;
	int percent_cmplt;
	int t;
	int erase_status;
	u32 max_file_size;
	bool wait_for_calibration_complete = false;
	PIP2_SEND_CMD pip2_send_cmd;
	struct device *dev = context;
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pip2_loader_data *pip2_data = ld->pip2_data;
	struct pt_core_data *cd = dev_get_drvdata(dev);

	pt_debug(dev, DL_WARN, "%s: ATM - Begin BL\n", __func__);
	_pt_pip2_update_bl_status(dev, UPDATE_FW_IDLE, PT_NO_INC);

	if (cd->bus_ops->bustype == BUS_I2C)
		packet_size = PIP2_BL_I2C_FILE_WRITE_LEN_PER_PACKET;
	else
		packet_size = PIP2_BL_SPI_FILE_WRITE_LEN_PER_PACKET;

	ret = cmd->request_exclusive(dev, PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (ret < 0) {
		pt_debug(dev, DL_ERROR,
			"%s: Failed to aquire exclusive access\n", __func__);
		update_fw_status = UPDATE_FW_EXCLUSIVE_ACCESS_ERROR;
		goto exit;
	}
	_pt_pip2_update_bl_status(dev, 0, 1);

	if (!fw) {
		if (ld->pip2_load_builtin) {
			pt_debug(dev, DL_ERROR,
				"%s: No builtin firmware\n", __func__);
			ld->builtin_bin_fw_status = -EINVAL;
			pt_debug(dev, DL_ERROR, "%s: Exit BL\n", __func__);
			_pt_pip2_update_bl_status(dev,
				UPDATE_FW_NO_FW_PROVIDED, PT_NO_INC);
			goto exit;
		} else {
			fw_img = kzalloc(PT_PIP2_MAX_FILE_SIZE, GFP_KERNEL);
			us_fw_used = 1;
			ret = cmd->nonhid_cmd->read_us_file(dev,
				cd->pip2_us_file_path, fw_img, &fw_size);
			if (ret) {
				pt_debug(dev, DL_ERROR,
					"%s: No firmware provided to load\n",
					__func__);
				pt_debug(dev, DL_ERROR, "%s: Exit BL\n",
					__func__);
				_pt_pip2_update_bl_status(dev,
					UPDATE_FW_NO_FW_PROVIDED, PT_NO_INC);
				goto exit;
			}
		}
	} else {
		fw_img = (u8 *)&(fw->data[0]);
		fw_size = fw->size;
	}

	if (!fw_img || !fw_size) {
		pt_debug(dev, DL_ERROR,
			"%s: Invalid fw or file size=%d\n", __func__,
			(int)fw_size);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_INVALID_FW_IMAGE,
			PT_NO_INC);
		goto exit;
	}
	if (ld->pip2_load_file_no == PIP2_FW_FILE) {
		if (fw_img[0] >= (fw_size + 1)) {
			pt_debug(dev, DL_ERROR,
				"%s: Firmware format is invalid\n", __func__);
			_pt_pip2_update_bl_status(dev,
				UPDATE_FW_INVALID_FW_IMAGE, PT_NO_INC);
			goto exit;
		}
	}

	cd->fw_updating = true;
	wake_up(&cd->wait_q);

	_pt_pip2_update_bl_status(dev, 0, 1);
	pt_debug(dev, DL_INFO,
		"%s: Found file of size: %d bytes\n", __func__, (int)fw_size);
	pm_runtime_get_sync(dev);
	_pt_pip2_update_bl_status(dev, 0, 1);

	/* Wait for completion of FW upgrade thread before continuing */
	if (!ld->pip2_load_builtin)
		init_completion(&pip2_data->pip2_fw_upgrade_complete);

	_pt_pip2_update_bl_status(dev, 0, 1);

	cmd->request_stop_wd(dev);

	if (cd->flashless_dut) {
		cd->bl_pip_ver_ready = false;
		cd->app_pip_ver_ready = false;
	}

	/*
	 * 'mode' is used below, if DUT was already in BL before attempting to
	 * enter the BL, there was either no FW to run or the FW was corrupt
	 * so either way force a BL
	 */
	ret = cmd->request_pip2_enter_bl(dev, &mode, NULL);
	if (ret) {
		pt_debug(dev, DL_ERROR, "%s: Failed to enter BL\n",
			__func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_ENTER_BL_ERROR,
			PT_NO_INC);
		goto exit;
	}
	_pt_pip2_update_bl_status(dev, 0, 1);

	/* Only compare FW ver or previous mode when doing a built-in upgrade */
	if (ld->pip2_load_builtin) {
		if (_pt_pip2_need_upgrade_due_to_fw_ver(dev, fw) ||
		    mode == PT_MODE_BOOTLOADER) {
			_pt_pip2_update_bl_status(dev, 0, 1);
		} else {
			_pt_pip2_update_bl_status(dev,
				UPDATE_FW_VERSION_ERROR, PT_NO_INC);
			goto exit;
		}
	}

	pt_debug(dev, DL_INFO, "%s OPEN File %d for write\n",
		__func__, ld->pip2_load_file_no);
	ret = cmd->nonhid_cmd->pip2_file_open(dev, ld->pip2_load_file_no);
	if (ret < 0) {
		pt_debug(dev, DL_ERROR, "%s Open file %d failed\n",
			__func__, ld->pip2_load_file_no);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_FILE_OPEN_ERROR,
			PT_NO_INC);
		goto exit;
	}
	pip2_data->pip2_file_handle = ret;
	_pt_pip2_update_bl_status(dev, 0, 1);

	/* Regarding to TC3315, the size of RAM_FILE is less than fw image */
	if (ld->pip2_load_file_no != PIP2_RAM_FILE) {
		ret = cmd->nonhid_cmd->pip2_file_get_stats(dev,
			ld->pip2_load_file_no, NULL, &max_file_size);
		if (ret) {
			pt_debug(dev, DL_ERROR,
				"%s: Failed to get_file_state ret=%d\n",
				__func__, ret);
			goto exit_close_file;
		}
		if (fw_size > max_file_size) {
			pt_debug(dev, DL_ERROR,
				"%s: Firmware image(%d) is over size(%d)\n",
					__func__, fw_size, max_file_size);
			_pt_pip2_update_bl_status(dev,
				UPDATE_FW_INVALID_FW_IMAGE, PT_NO_INC);
			goto exit_close_file;
		}
#ifdef TTDL_DIAGNOSTICS
		/* Log the Flash part info */
		if (cd->debug_level >= DL_DEBUG)
			_pt_pip2_get_flash_info(dev, read_buf);
#endif
		/* Erase file before loading */
		ret = cmd->nonhid_cmd->pip2_file_erase(dev,
			ld->pip2_load_file_no, &erase_status);
		if (ret < 0) {
			pt_debug(dev, DL_ERROR,
				"%s: File erase failed rc=%d status=%d\n",
				__func__, ret, erase_status);
			_pt_pip2_update_bl_status(dev,
				UPDATE_FW_ERASE_ERROR, PT_NO_INC);
			goto exit_close_file;
		}
		_pt_pip2_update_bl_status(dev, 0, 5);
	}

	remain_bytes = fw_size;
	buf[0] = pip2_data->pip2_file_handle;
	pt_debug(dev, DL_WARN,
		"%s: ATM - Writing %d bytes of firmware data now\n",
		__func__, fw_size);

	/*
	 * No IRQ function is used to BL to reduce BL time due to any IRQ
	 * latency.
	 */
	if (cd->bl_with_no_int) {
		pip2_send_cmd = cmd->nonhid_cmd->pip2_send_cmd_no_int;
		disable_irq_nosync(cd->irq);
	} else
		pip2_send_cmd = cmd->nonhid_cmd->pip2_send_cmd;

	/* Continue writing while data remains */
	while (remain_bytes > packet_size) {
		write_len = packet_size;

		/* Don't update BL status on every pass */
		if (remain_bytes % 2000 < packet_size) {
			/* Calculate % complete for update_fw_status sysfs */
			percent_cmplt = (fw_size - remain_bytes) *
					100 / fw_size;
			if (percent_cmplt > 0 &&
			    percent_cmplt > UPDATE_FW_ACTIVE_90)
				percent_cmplt = UPDATE_FW_ACTIVE_90;
			_pt_pip2_update_bl_status(dev, percent_cmplt,
				PT_NO_INC);

#ifdef TTDL_DIAGNOSTICS
			pt_debug(dev, DL_INFO,
				"Wrote %d bytes with %d bytes remaining\n",
				fw_size - remain_bytes - write_len,
				remain_bytes);
#endif
		}
		if (retry_packet > 0) {
#ifdef TTDL_DIAGNOSTICS
			cd->bl_retry_packet_count++;
			cmd->request_toggle_err_gpio(dev,
				PT_ERR_GPIO_BL_RETRY_PACKET);
			pt_debug(dev, DL_WARN,
				"%s: === Retry Packet #%d ===\n",
				__func__, retry_packet);
#endif
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
			/* Get and log the last error(s) */
			_pt_pip2_log_last_error(dev, read_buf);
#endif
		}

		memcpy(&buf[1], fw_img, write_len);
		ret = pip2_send_cmd(dev,
			PT_CORE_CMD_UNPROTECTED, PIP2_CMD_ID_FILE_WRITE,
			buf, write_len + 1, read_buf, &actual_read_len);
		status = read_buf[PIP2_RESP_STATUS_OFFSET];

		/* Write cmd successful with a fail status */
		if (!ret && status) {
			/*
			 * The last time through the loop when remain_bytes =
			 * write_len, no partial payload will remain, the last
			 * successful write (when writing to RAM) will respond
			 * with EOF status, writing to FLASH will respond with a
			 * standard success status of 0x00
			 */
			if ((ld->pip2_load_file_no == PIP2_RAM_FILE) &&
			    (status == PIP2_RSP_ERR_END_OF_FILE) &&
			    (remain_bytes == write_len)) {
				pt_debug(dev, DL_WARN,
					"%s Last write, ret = 0x%02x\n",
					__func__, status);
				/* Drop out of the while loop */
				break;
			}
			_pt_pip2_update_bl_status(dev,
				UPDATE_FW_WRITE_ERROR, PT_NO_INC);
			pt_debug(dev, DL_ERROR,
				"%s file write failure, status = 0x%02x\n",
				__func__, status);
			if (retry_packet >= 3) {
				/* Tripple retry error - break */
				remain_bytes = 0;
				pt_debug(dev, DL_ERROR,
					"%s %d - Packet status error - Break\n",
					__func__, status);
			} else {
				retry_packet++;
			}
		} else if (ret) {
			/* Packet write failed - retry 3x */
			if (retry_packet >= 3) {
				remain_bytes = 0;
				pt_debug(dev, DL_ERROR,
					"%s %d - Packet cmd error - Break\n",
					__func__, ret);
			}
			retry_packet++;
		} else {
			/* Cmd success and status success */
			retry_packet = 0;
		}

		if (retry_packet == 0) {
			fw_img += write_len;
			remain_bytes -= write_len;
		}
	}
	/* Write the remaining bytes if any remain */
	if (remain_bytes > 0 && retry_packet == 0) {
		pt_debug(dev, DL_INFO,
			"Write last %d bytes to File = 0x%02x\n",
			remain_bytes, pip2_data->pip2_file_handle);
		memcpy(&buf[1], fw_img, remain_bytes);
		while (remain_bytes > 0 && retry_packet <= 3) {
			ret = pip2_send_cmd(dev, PT_CORE_CMD_UNPROTECTED,
				PIP2_CMD_ID_FILE_WRITE, buf, remain_bytes + 1,
				read_buf, &actual_read_len);
			status = read_buf[PIP2_RESP_STATUS_OFFSET];
			if (ret || status) {
				/*
				 * Any non zero status when writing to FLASH is
				 * an error. Only when writing to RAM, the last
				 * packet must respond with an EOF status
				 */
				if ((ld->pip2_load_file_no >= PIP2_FW_FILE) ||
				    (ld->pip2_load_file_no == PIP2_RAM_FILE &&
				     status != PIP2_RSP_ERR_END_OF_FILE)) {
					_pt_pip2_update_bl_status(dev,
						UPDATE_FW_WRITE_ERROR,
						PT_NO_INC);
					pt_debug(dev, DL_ERROR,
						"%s Write Fail-status=0x%02x\n",
						__func__, status);
					retry_packet++;
				} else if (ld->pip2_load_file_no ==
					   PIP2_RAM_FILE &&
					   status == PIP2_RSP_ERR_END_OF_FILE) {
					/* Special case EOF writing to SRAM */
					remain_bytes = 0;
					status = 0;
				}
			} else {
				remain_bytes = 0;
			}
		}
	}

	if (cd->bl_with_no_int)
		enable_irq(cd->irq);

	if (remain_bytes == 0 && retry_packet == 0)
		_pt_pip2_update_bl_status(dev, UPDATE_FW_ACTIVE_99, PT_NO_INC);

	if (retry_packet >= 3) {
		/* A packet write failure occurred 3x */
		pt_debug(dev, DL_ERROR,
			"%s: BL terminated due to consecutive write errors\n",
			__func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_WRITE_ERROR,
			PT_NO_INC);
	} else if (status) {
		ret = status;
		pt_debug(dev, DL_ERROR,
			"%s: File write failed with status=%d\n",
			__func__, status);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_WRITE_ERROR,
			PT_NO_INC);
	} else
		pt_debug(dev, DL_INFO,
			"%s: BIN file write finished successfully\n", __func__);

	ret = cmd->nonhid_cmd->pip2_file_close(dev, ld->pip2_load_file_no);
	if (ret != ld->pip2_load_file_no) {
		pt_debug(dev, DL_ERROR,
			"%s file close failure\n", __func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_FILE_CLOSE_ERROR,
			PT_NO_INC);
		goto exit;
	}

	/* When updating non FW files, stay in BL */
	if (ld->pip2_load_file_no >= PIP2_CONFIG_FILE)
		goto exit;

	if ((ld->pip2_load_file_no == PIP2_RAM_FILE) &&
	    (update_fw_status < UPDATE_FW_COMPLETE)) {
		/* When writing to RAM don't reset, just launch application */
		pt_debug(dev, DL_INFO,
			"%s Sending execute command now...\n", __func__);
		cd->startup_status = STARTUP_STATUS_START;
		ret = cmd->nonhid_cmd->pip2_send_cmd(dev,
			PT_CORE_CMD_UNPROTECTED, PIP2_CMD_ID_EXECUTE,
			NULL, 0, read_buf, &actual_read_len);
		status = read_buf[PIP2_RESP_STATUS_OFFSET];
		if (ret || status) {
			pt_debug(dev, DL_ERROR,
				"%s Execute command failure\n", __func__);
			_pt_pip2_update_bl_status(dev,
				UPDATE_FW_EXECUTE_ERROR, PT_NO_INC);
			goto exit;
		}
	} else if (ld->pip2_load_file_no == PIP2_FW_FILE &&
		   update_fw_status < UPDATE_FW_COMPLETE) {
		pt_debug(dev, DL_INFO,
			"%s Toggle TP_XRES now...\n", __func__);
		cmd->request_reset(dev, PT_CORE_CMD_UNPROTECTED);
	}
	pt_debug(dev, DL_INFO, "%s: APP launched\n", __func__);

	/* If any error occured simply close the file and exit */
	if (update_fw_status > UPDATE_FW_COMPLETE)
		goto exit_close_file;

	/* Wait for FW reset sentinel from reset or execute for up to 500ms */
	t = wait_event_timeout(cd->wait_q,
		(cd->startup_status >= STARTUP_STATUS_FW_RESET_SENTINEL),
		msecs_to_jiffies(PT_BL_WAIT_FOR_SENTINEL));
	if (IS_TMO(t)) {
		pt_debug(dev, DL_WARN,
			"%s: 0x%04X Timeout waiting for FW sentinel",
			__func__, cd->startup_status);
	}

	/* Double verify DUT is alive and well in Application mode */
	if (cd->startup_status & STARTUP_STATUS_FW_RESET_SENTINEL) {
		ret = cmd->request_pip2_get_mode_sysmode(dev,
			PT_CORE_CMD_UNPROTECTED, &mode, NULL);
		pt_debug(dev, DL_WARN, "%s: mode = %d (Expected 2)",
			__func__, mode);
		if (mode != PT_MODE_OPERATIONAL) {
			pt_debug(dev, DL_ERROR,
				"%s ERROR: Not in App mode as expected\n",
				__func__);
			_pt_pip2_update_bl_status(dev, UPDATE_FW_MODE_ERROR,
				PT_NO_INC);
			goto exit;
		}
	} else {
		pt_debug(dev, DL_ERROR, "%s: FW sentinel not seen 0x%04X\n",
			__func__,  cd->startup_status);
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		_pt_pip2_log_last_error(dev, read_buf);
#endif
		_pt_pip2_update_bl_status(dev, UPDATE_FW_SENTINEL_NOT_SEEN,
			PT_NO_INC);
		goto exit;
	}

	/* On a Flashless DUT save or restore the CAL data */
	if (cd->cal_cache_in_host == PT_FEATURE_ENABLE)
		_pt_calibrate_flashless_dut(dev);

	/* Subscribe calibration task if calibration flag is set */
	if (ld->loader_pdata
		&& (ld->loader_pdata->flags
			 & PT_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE)
		&& (cd->cal_cache_in_host == PT_FEATURE_DISABLE)) {
#if (KERNEL_VERSION(3, 13, 0) <= LINUX_VERSION_CODE)
		reinit_completion(&ld->calibration_complete);
#else
		INIT_COMPLETION(ld->calibration_complete);
#endif
		/* set up call back for startup */
		pt_debug(dev, DL_INFO, "%s: Adding callback for calibration\n",
			__func__);
		ret = cmd->subscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_LOADER_NAME, pt_calibration_attention, 0);
		if (ret) {
			pt_debug(dev, DL_ERROR,
				"%s: Failed adding callback for calibration\n",
				__func__);
			ret = 0;
		} else
			wait_for_calibration_complete = true;
	}

	pt_debug(dev, DL_INFO, "%s: == PIP2 FW upgrade finished ==\n",
		__func__);
	goto exit;

exit_close_file:
	ret = cmd->nonhid_cmd->pip2_file_close(dev, ld->pip2_load_file_no);
	if (ret < 0) {
		pt_debug(dev, DL_ERROR,
			"%s file close failure\n", __func__);
		_pt_pip2_update_bl_status(dev, UPDATE_FW_FILE_CLOSE_ERROR,
			PT_NO_INC);
	}

exit:
	cd->fw_updating = false;
	if (us_fw_used)
		kfree(fw_img);
	if (ld->pip2_load_file_no > PIP2_FW_FILE)
		goto exit_staying_in_bl;

	cmd->release_exclusive(dev);

	pm_runtime_put_sync(dev);
	if (fw)
		release_firmware(fw);

	/*
	 * For built-in FW update, it should not warn builtin_bin_fw_status
	 * for each bootup since update_fw_status would be
	 * UPDATE_FW_VERSION_ERROR in most situations because firmware
	 * should have been up to date.
	 */
	if (ld->pip2_load_builtin) {
		if ((update_fw_status == UPDATE_FW_ACTIVE_99) ||
		    (update_fw_status == UPDATE_FW_VERSION_ERROR))
			ld->builtin_bin_fw_status = 0;
		else
			ld->builtin_bin_fw_status = -EINVAL;
	}

	if ((update_fw_status == UPDATE_FW_ACTIVE_99) ||
	    (update_fw_status == UPDATE_FW_VERSION_ERROR)) {
		pt_debug(dev, DL_WARN, "%s: Queue ENUM\n", __func__);
		cmd->request_enum(dev, true);
	}
	if (update_fw_status < UPDATE_FW_COMPLETE)
		_pt_pip2_update_bl_status(dev, UPDATE_FW_COMPLETE, PT_NO_INC);

	if (wait_for_calibration_complete)
		wait_for_completion(&ld->calibration_complete);

	pt_debug(dev, DL_INFO, "%s: Starting watchdog\n", __func__);
	ret = cmd->request_start_wd(dev);

	/*
	 * When in No-Flash mode allow auto BL after any BL.
	 * There is an issue where setting flashless mode via drv_debug
	 * can happen in the middle of pt_pip2_enter_bl() which will revert
	 * the flashless_auto_bl value back to what it was when the function
	 * started.
	 */
	if (cd->flashless_dut)
		cd->flashless_auto_bl = PT_ALLOW_AUTO_BL;

	return;

exit_staying_in_bl:
	if (us_fw_used)
		kfree(fw_img);
	/* When updating a non FW file, a restart is not wanted. Stay in BL */
	_pt_pip2_update_bl_status(dev, UPDATE_FW_COMPLETE, PT_NO_INC);
	cmd->release_exclusive(dev);
	pm_runtime_put_sync(dev);
	if (fw)
		release_firmware(fw);
}

#define PIP2_MAX_FILE_NAMES 3
/*******************************************************************************
 * FUNCTION: pt_pip2_upgrade_firmware_from_builtin
 *
 * SUMMARY: Bootload the DUT with a built in firmware binary image.
 *	Load either a SRAM image "ttdl_fw_RAM.bin" or a FLASH image
 *	"ttdl_fw.bin" with the priority being the SRAM image.
 *
 * PARAMETERS:
 *	*dev - pointer to the device structure
 ******************************************************************************/
static int pt_pip2_upgrade_firmware_from_builtin(struct device *dev)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	const struct firmware *fw_entry = NULL;
	int retval;
	int rc = 0;
	int read_size = 16;
	u8 image[16];
	int index = 0;
	int file_count = 0;
	char *filename[PIP2_MAX_FILE_NAMES];

	/*
	 * Load the supported filenames in the correct search order
	 * 0 - "tt_fw<_PIDX>.bin"
	 * 1 - "XXXX_tt_fw<_PIDX>.bin" where XXXX = Silicon ID
	 * 2 - "tt_fw.bin", default FW name
	 */

	filename[file_count++] = generate_firmware_filename(dev);
	filename[file_count++] = generate_silicon_id_firmware_filename(dev);
	if (pt_get_panel_id(dev) != PANEL_ID_NOT_ENABLED) {
		filename[file_count] =
		    kzalloc(sizeof(PT_FW_FILE_NAME), GFP_KERNEL);
		memcpy(filename[file_count++], PT_FW_FILE_NAME,
		       sizeof(PT_FW_FILE_NAME));
	}

	for (index = 0; index < file_count; index++) {
		if (!filename[index])
			return -ENOMEM;
	}

	if (cd->flashless_dut) {
		pt_debug(dev, DL_INFO,
			"%s: Proceed to BL flashless DUT\n", __func__);
		ld->pip2_load_file_no = PIP2_RAM_FILE;
		if (cd->pip2_us_file_path[0] == '\0') {
			ld->pip2_load_builtin = true;
			pt_debug(dev, DL_WARN,
				"%s: US Path not defined, BL from built-in\n",
				__func__);
		} else {
			/* Read a few bytes to see if file exists */
			rc = cmd->nonhid_cmd->read_us_file(dev,
				cd->pip2_us_file_path, image, &read_size);
			if (!rc) {
				ld->pip2_load_builtin = false;
				pt_debug(dev, DL_WARN,
					"%s: %s Found, BL from US\n",
					__func__, cd->pip2_us_file_path);
				goto ready;
			} else {
				ld->pip2_load_builtin = true;
				pt_debug(dev, DL_WARN,
					"%s: ATM - %s NOT Found, BL from built-in\n",
					__func__, cd->pip2_us_file_path);
			}
		}
	} else {
		ld->pip2_load_file_no = PIP2_FW_FILE;
		ld->pip2_load_builtin = true;
	}

	/* Look for any FW file name match and request the FW */
	mutex_lock(&cd->firmware_class_lock);
	index = 0;
	while (index < file_count) {
		pt_debug(dev, DL_INFO, "%s: Request FW class file: %s\n",
			__func__, filename[index]);
#if (KERNEL_VERSION(3, 13, 0) > LINUX_VERSION_CODE)
		retval = request_firmware(&fw_entry, filename[index], dev);
#else
		retval = request_firmware_direct(&fw_entry,
			filename[index], dev);
#endif
		if (retval < 0) {
			pt_debug(dev, DL_WARN, "%s: ATM - Fail request FW %s load\n",
				__func__, filename[index]);
		} else {
			pt_debug(dev, DL_INFO, "%s: FW %s class file loading\n",
				__func__, filename[index]);
			break;
		}
		index++;
	}

	/* No matching file names found */
	if (index == file_count) {
		pt_debug(dev, DL_WARN, "%s: No FW is found\n", __func__);
		goto exit;
	}

ready:
	_pt_pip2_firmware_cont(fw_entry, dev);
	retval = ld->builtin_bin_fw_status;
exit:
	mutex_unlock(&cd->firmware_class_lock);
	index = 0;
	while (index < file_count)
		kfree(filename[index++]);

	return retval;
}

/*******************************************************************************
 * FUNCTION: pt_pip2_create_fw_class
 *
 * SUMMARY: Create the firmware class but don't actually laod any FW to the
 *	DUT. This creates all the sysfs nodes needed for a user to bootload
 *	the DUT with their own bin file.
 *
 * PARAMETERS:
 *	*pip2_data     - pointer to the PIP2 loader data structure
 ******************************************************************************/
static int pt_pip2_create_fw_class(struct pip2_loader_data *pip2_data)
{

	int ret = 0;
	struct device *dev = pip2_data->dev;
	struct pt_loader_data *ld = pt_get_loader_data(dev);

	/*
	 * The file name dev_name(dev) is tied with bus name and usually
	 * it is "x-0024". This name is wanted to keep consistency
	 * (e.g. /sys/class/firmware/x-0024/) for the path of fw class
	 * nodes with different kernel release. Also it is an invalid bin
	 * file name used intentionally because request_firmware_nowait
	 * will not find the file which is what we want and then simply
	 * create the fw class nodes.
	 */
	ld->pip2_load_builtin = false;
	pt_debug(dev, DL_INFO, "%s: Request FW Class", __func__);
	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG,
			dev_name(dev), dev, GFP_KERNEL, dev,
			_pt_pip2_firmware_cont);
	if (ret) {
		pt_debug(dev, DL_ERROR,
			"%s: ERROR requesting firmware class\n", __func__);
	}

	return ret;
}

/*******************************************************************************
 * FUNCTION: pt_pip2_bl_from_file_work
 *
 * SUMMARY: The work function to schedule the BL work for PIP2 only.
 *
 * PARAMETERS:
 *	*bl_from_file - pointer to work_struct structure
 ******************************************************************************/
static void pt_pip2_bl_from_file_work(struct work_struct *pip2_bl_from_file)
{
	struct pt_loader_data *ld = container_of(pip2_bl_from_file,
			struct pt_loader_data, pip2_bl_from_file);
	struct device *dev = ld->dev;
	const struct firmware *fw_entry = NULL;

	_pt_pip2_firmware_cont(fw_entry, dev);
}

/*******************************************************************************
 * FUNCTION: pt_bl_from_file_work
 *
 * SUMMARY: The work function to schedule the BL work for PIP2 or PIP1
 *  according to the active_dut_generation in core data.
 *
 * PARAMETERS:
 *	*bl_from_file - pointer to work_struct structure
 ******************************************************************************/
static void pt_bl_from_file_work(struct work_struct *bl_from_file)
{
	struct pt_loader_data *ld = container_of(bl_from_file,
			struct pt_loader_data, bl_from_file);
	struct device *dev = ld->dev;
	const struct firmware *fw_entry = NULL;
	u8 dut_gen = cmd->request_dut_generation(dev);

	if (dut_gen == DUT_PIP2_CAPABLE)
		_pt_pip2_firmware_cont(fw_entry, dev);
	else if (dut_gen == DUT_PIP1_ONLY)
		_pt_pip1_bl_from_file(dev);
}

/*******************************************************************************
 * FUNCTION: pt_pip2_bl_from_file_show
 *
 * SUMMARY: The show method for the "pip2_bl_from_file" sysfs node. The
 *  scheduled work will perform PIP2 BL.
 *
 * NOTE: Since this function doesn't set pip2_load_file_no, it will use the
 * value what has been stored there.
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes structure
 *	*buf  - pointer to print output buffer
 ******************************************************************************/
static ssize_t pt_pip2_bl_from_file_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int rc = 0;
	int read_size = 2;
	u8 image[2];

	mutex_lock(&cd->firmware_class_lock);
	ld->pip2_load_builtin = false;
	mutex_unlock(&cd->firmware_class_lock);

	/* Read a few bytes to see if file exists */
	rc = cmd->nonhid_cmd->read_us_file(dev,
		cd->pip2_us_file_path, image, &read_size);

	if (!rc) {
		schedule_work(&ld->pip2_bl_from_file);

		return snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"BL File: %s\n",
			rc, cd->pip2_us_file_path);
	} else {
		return snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"BL File: '%s' - Does not exist\n",
			rc, cd->pip2_us_file_path);
	}
}

/*******************************************************************************
 * FUNCTION: pt_bl_from_file_show
 *
 * SUMMARY: The show method for the "pt_bl_from_file" sysfs node. The scheduled
 *  work can perform either PIP1 BL and PIP2 BL according to the
 *  active_dut_generation of core data.
 *
 * NOTE: Since this function doesn't set pip2_load_file_no, it will use the
 * value what has been stored there.
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes structure
 *	*buf  - pointer to print output buffer
 ******************************************************************************/
static ssize_t pt_bl_from_file_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int rc = 0;
	int read_size = 2;
	u8 dut_gen = cmd->request_dut_generation(dev);
	u8 image[2];

	mutex_lock(&cd->firmware_class_lock);
	ld->pip2_load_builtin = false;
	mutex_unlock(&cd->firmware_class_lock);

	/* Read a few bytes to see if file exists */
	rc = cmd->nonhid_cmd->read_us_file(dev,
		cd->pip2_us_file_path, image, &read_size);

	if (dut_gen == DUT_UNKNOWN) {
		rc = -EINVAL;
		return snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"BL File: '%s' - Failed, DUT Generation could not be determined\n",
			rc, cd->pip2_us_file_path);
	} else if (!rc) {
		schedule_work(&ld->bl_from_file);

		return snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"BL File: %s\n",
			rc, cd->pip2_us_file_path);
	} else {
		return snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"BL File: '%s' - Does not exist\n",
			rc, cd->pip2_us_file_path);
	}
}

/*******************************************************************************
 * FUNCTION: pt_pip2_bl_from_file_store
 *
 * SUMMARY: The store method for the "pip2_bl_from_file" and "pt_bl_from_file"
 *  sysfs node. Used to allow any file path[necessary] and file_no[optional] to
 *  be used to BL, for example: "echo /data/pt_fw 1 > pip2_bl_from_file", and
 *  do the "cat" will perform FW loader process.
 *
 * NOTE: the last char of file path in buf which is a '\n' is not copied.
 * NOTE: the default file_no is PIP2_RAM_FILE.
 *
 * PARAMETERS:
 *      *dev   - pointer to device structure
 *      *attr  - pointer to device attributes
 *      *buf   - pointer to output buffer
 *       size   - size of data in buffer
 ******************************************************************************/
static ssize_t pt_pip2_bl_from_file_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	const char *deli_space = " ",  *deli_comma = ",";
	char name_space[PT_MAX_PATH_SIZE];
	char *ptr_left = NULL, *ptr_right = name_space;
	u8 file_no = PIP2_RAM_FILE;
	u32 input_data[2];
	int length;
	bool file_no_set = false;

	memset(name_space, 0, PT_MAX_PATH_SIZE);
	memset(cd->pip2_us_file_path, 0, PT_MAX_PATH_SIZE);
	if (size <= PT_MAX_PATH_SIZE) {
		memcpy(name_space, buf, size);
		ptr_left = strsep(&ptr_right, deli_space);
		if (ptr_right == NULL) {
			ptr_right = name_space;
			ptr_left = strsep(&ptr_right, deli_comma);
		}

		if (ptr_right != NULL) {
			length = cmd->parse_sysfs_input(
			    dev, ptr_right, strlen(ptr_right), input_data,
			    ARRAY_SIZE(input_data));
			if (length <= 0) {
				pt_debug(dev, DL_ERROR,
					 "%s: Input format error!\n", __func__);
				return -EINVAL;
			}
			file_no_set = true;
			file_no = input_data[0];
		}

		pt_debug(dev, DL_WARN, "%s:Path=%s, File_no=%s(%d)\n", __func__,
			 ptr_left, ptr_right, file_no);

		if ((file_no_set) &&
		    ((file_no < PIP2_RAM_FILE) || (file_no > PIP2_FILE_MAX))) {
			pt_debug(dev, DL_WARN, "%s:Invalid File_no = %d\n",
				__func__, file_no);
			return -EINVAL;
		}

		mutex_lock(&cd->firmware_class_lock);
		ld->pip2_load_file_no = file_no;
		mutex_unlock(&cd->firmware_class_lock);

		if (ptr_left[strlen(ptr_left) - 1] == '\n')
			memcpy(cd->pip2_us_file_path, ptr_left,
			       strlen(ptr_left) - 1);
		else
			memcpy(cd->pip2_us_file_path, ptr_left,
			       strlen(ptr_left));
	}

	return size;
}
static DEVICE_ATTR(pip2_bl_from_file, 0644,
		pt_pip2_bl_from_file_show, pt_pip2_bl_from_file_store);

static DEVICE_ATTR(pt_bl_from_file, 0644,
		pt_bl_from_file_show, pt_pip2_bl_from_file_store);

/*******************************************************************************
 * FUNCTION: pt_pip2_manual_upgrade_store
 *
 * SUMMARY: Store method for the pip2_manual_upgrade sysfs node. Allows
 *	sysfs control of bootloading a new FW image to FLASH.
 *
 * PARAMETERS:
 *      *dev   - pointer to device structure
 *      *attr  - pointer to device attributes
 *      *buf   - pointer to output buffer
 *       size   - size of data in buffer
 ******************************************************************************/
static ssize_t pt_pip2_manual_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u32 input_data[2] = {0};
	int length;
	int rc = 0;

	length = cmd->parse_sysfs_input(dev, buf, size, input_data,
			ARRAY_SIZE(input_data));

	if (length != 1) {
		pt_debug(dev, DL_WARN, "%s: Invalid number of arguments\n",
			__func__);
		rc = -EINVAL;
		goto exit;
	}

	if (input_data[0] < 0 || input_data[0] > 1) {
		pt_debug(dev, DL_WARN, "%s: Invalid arguments\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	_pt_pip2_update_bl_status(dev, UPDATE_FW_IDLE, PT_NO_INC);
	if (ld->is_manual_upgrade_enabled) {
		pt_debug(dev, DL_ERROR,
			"%s: ERROR - Manual upgrade busy\n", __func__);
		rc = -EBUSY;
		goto exit;
	}
	ld->pip2_load_file_no = PIP2_FW_FILE;
	pt_debug(dev, DL_DEBUG, "%s: ATM - File number is %d\n",
		__func__, ld->pip2_load_file_no);

	ld->is_manual_upgrade_enabled = 1;
	rc = pt_pip2_create_fw_class(ld->pip2_data);
	ld->is_manual_upgrade_enabled = 0;
	if (rc < 0)
		pt_debug(dev, DL_ERROR,
			"%s: ERROR - FLASH Upgrade failed\n", __func__);

exit:
	if (rc)
		return rc;
	return size;
}
static DEVICE_ATTR(pip2_manual_upgrade, 0200,
	NULL, pt_pip2_manual_upgrade_store);

/*******************************************************************************
 * FUNCTION: pt_pip2_manual_ram_upgrade_store
 *
 * SUMMARY: Store method for the pip2_manual_ram_upgrade sysfs node. Allows
 *	sysfs control of bootloading a new FW image to SRAM.
 *
 * PARAMETERS:
 *      *dev   - pointer to device structure
 *      *attr  - pointer to device attributes
 *      *buf   - pointer to output buffer
 *       size   - size of data in buffer
 ******************************************************************************/
static ssize_t pt_pip2_manual_ram_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u32 input_data[2] = {0};
	int length;
	int rc = 0;

	length = cmd->parse_sysfs_input(dev, buf, size, input_data,
			ARRAY_SIZE(input_data));

	if (length != 1) {
		pt_debug(dev, DL_WARN, "%s: Invalid number of arguments\n",
			__func__);
		rc = -EINVAL;
		goto exit;
	}

	if (input_data[0] < 0 || input_data[0] > 1) {
		pt_debug(dev, DL_WARN, "%s: Invalid arguments\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	_pt_pip2_update_bl_status(dev, UPDATE_FW_IDLE, PT_NO_INC);
	if (ld->is_manual_upgrade_enabled) {
		pt_debug(dev, DL_ERROR,
			"%s: ERROR - Manual upgrade busy\n", __func__);
		rc = -EBUSY;
		goto exit;
	}

	ld->pip2_load_file_no = PIP2_RAM_FILE;
	pt_debug(dev, DL_DEBUG, "%s: ATM - File number is %d\n",
		__func__, ld->pip2_load_file_no);

	ld->is_manual_upgrade_enabled = 1;
	rc = pt_pip2_create_fw_class(ld->pip2_data);
	ld->is_manual_upgrade_enabled = 0;
	if (rc < 0)
		pt_debug(dev, DL_ERROR,
			"%s: ERROR - RAM Upgrade failed\n", __func__);

exit:
	if (rc)
		return rc;
	return size;
}
static DEVICE_ATTR(pip2_manual_ram_upgrade, 0200,
	NULL, pt_pip2_manual_ram_upgrade_store);

/*******************************************************************************
 * FUNCTION: pt_pip2_file_write_store
 *
 * SUMMARY: Store method for the pip2_file_write sysfs node. Allows
 *	sysfs control to "load" data into any file.
 *
 * PARAMETERS:
 *      *dev   - pointer to device structure
 *      *attr  - pointer to device attributes
 *      *buf   - pointer to output buffer
 *       size   - size of data in buffer
 ******************************************************************************/
static ssize_t pt_pip2_file_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	int rc;
	u32 input_data[3];
	int length;

	if (ld->is_manual_upgrade_enabled) {
		pt_debug(dev, DL_ERROR,
			"%s: ERROR - Manual upgrade busy\n", __func__);
		rc = -EBUSY;
		goto exit;
	}

	length = cmd->parse_sysfs_input(dev, buf, size, input_data,
			ARRAY_SIZE(input_data));
	if (length <= 0 || length > 2) {
		pt_debug(dev, DL_ERROR, "%s: Invalid number of arguments\n",
			__func__);
		ld->pip2_file_data.para_num = 0;
		rc = -EINVAL;
		goto exit;
	}

	if (input_data[0] < PIP2_FW_FILE || input_data[0] > PIP2_FILE_MAX) {
		pt_debug(dev, DL_ERROR, "%s: Invalid file handle\n", __func__);
		ld->pip2_file_data.para_num = 0;
		rc = -EINVAL;
		goto exit;
	}

	ld->pip2_load_file_no = input_data[0];
	if (length == 2)
		ld->pip2_file_data.file_offset = input_data[1];

	ld->is_manual_upgrade_enabled = 1;
	rc = pt_pip2_create_fw_class(ld->pip2_data);
	ld->is_manual_upgrade_enabled = 0;
	if (rc < 0)
		pt_debug(dev, DL_ERROR,
			"%s: ERROR - RAM Upgrade failed\n", __func__);
exit:
	if (rc)
		return rc;
	return size;
}
static DEVICE_ATTR(pip2_file_write, 0200, NULL, pt_pip2_file_write_store);

/*******************************************************************************
 * FUNCTION: pt_update_fw_store
 *
 * SUMMARY: Store method for the update_fw sysfs node. This node is required
 *	by ChromeOS to first determine if loading is available and then perform
 *	the loading if required. This function is simply a wrapper to call:
 *		pt_pip2_manual_upgrade_store - for the TC3XXX or TT7XXX parts
 *		pt_manual_upgrade_store - for the legacy Gen5/6 devices.
 *
 * PARAMETERS:
 *      *dev   - pointer to device structure
 *      *attr  - pointer to device attributes
 *      *buf   - pointer to output buffer
 *       size   - size of data in buffer
 ******************************************************************************/
static ssize_t pt_update_fw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 dut_gen = cmd->request_dut_generation(dev);

	if (dut_gen == DUT_PIP2_CAPABLE)
		size = pt_pip2_manual_upgrade_store(dev, attr, buf, size);
	else if (dut_gen == DUT_PIP1_ONLY)
		size = pt_manual_upgrade_store(dev, attr, buf, size);

	return size;
}
static DEVICE_ATTR(update_fw, 0200, NULL, pt_update_fw_store);
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */

#ifdef TTDL_DIAGNOSTICS
/*******************************************************************************
 * FUNCTION: pt_pip2_file_read_show
 *
 * SUMMARY: The read method for the pip2_file_read sysfs node. Allows to
 *  perform flash read action according to stored value. This function will
 *  re-enter always until it returns:
 *        0: No data to be written to sysfs node
 *       <0: Error happens
 * (For kernel version large than 3.11(not tested), if the function returns
 * non-zero value in previous chunk, to return 0 once can not stop re-enter.
 * It needs one more time to stop read action by returned value <= 0. But for
 * older version, read action will stop when returned value <= 0 once).
 *
 * NOTE: Up to PIP2_FILE_WRITE_LEN_PER_PACKET(245) bytes of data are read for
 * each enter. When last package is read, pip2_file_data.para_num is assigned
 * as negatie value(-1), then next enter can return 0 to indicate the read
 * method can be finished.
 *
 * RETURN: Size of data written to sysfs node
 *
 * PARAMETERS:
 *  *filp     - pointer to file structure
 *  *kobj     - pointer to kobject structure
 *  *bin_attr - pointer to bin_attribute structure
 *   buf      - pointer to cmd input buffer
 *   offset   - offset index to store input buffer
 *   count    - size of data in buffer
 ******************************************************************************/
static ssize_t pt_pip2_file_read_show(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int rc = 0;
	u8 file_handle = ld->pip2_file_data.file_handle;
	u8 *pr_buf = ld->pip2_file_data.file_print_buf;
	int print_idx = 0, read_size = 0, i;
	u8  read_buf[PT_MAX_PIP2_MSG_SIZE];
	u8  read_len;
	u32 address, file_size;

	if (ld->pip2_file_data.file_print_left) {
		pt_debug(dev, DL_INFO, "%s: print left=%d, count=%zu\n",
			__func__, ld->pip2_file_data.file_print_left, count);

		print_idx = ld->pip2_file_data.file_print_left;
		if (count < print_idx) {
			memcpy(buf, pr_buf, count);
			ld->pip2_file_data.file_print_left = print_idx - count;
			for (i = 0; i < ld->pip2_file_data.file_print_left; i++)
				pr_buf[i] = pr_buf[i+count];
			print_idx = count;
		} else {
			memcpy(buf, pr_buf, print_idx);
			ld->pip2_file_data.file_print_left = 0;
		}
		return print_idx;
	}

	if (ld->pip2_file_data.para_num == 0) {
		/*
		 * When offset != 0, it means the extra call for splice out,
		 * don't need a warning.
		 */
		if (offset != 0)
			return 0;

		print_idx += scnprintf(buf, count, "Status: %d\n"
			"No input!\n", -EINVAL);
		pt_debug(dev, DL_ERROR, "%s: Invalid para_num = %d!\n",
			__func__, ld->pip2_file_data.para_num);
		return print_idx;
	} else if (ld->pip2_file_data.para_num == -1) {
		ld->pip2_file_data.para_num = 0;
		pt_debug(dev, DL_INFO, "%s: flash read finish!\n",
				__func__);
		rc = 0;
		goto exit_release;
	} else if (ld->pip2_file_data.para_num < -1) {
		ld->pip2_file_data.para_num = 0;
		pt_debug(dev, DL_ERROR, "%s: Exit directly due to errors!\n",
				__func__);
		return 0;
	} else if (ld->pip2_file_data.para_num > 3) {
		ld->pip2_file_data.para_num = 0;
		pt_debug(dev, DL_ERROR,
			"%s: Exit directly due to invalid parameter!\n",
				__func__);
		return 0;
	}

	if (offset == 0) {
		ld->pip2_file_data.file_print_buf = kzalloc(PIPE_BUF,
			GFP_KERNEL);
		if (!ld->pip2_file_data.file_print_buf) {
			rc = -ENOMEM;
			goto exit;
		}
		/* This functionality is only available in the BL */
		if (cd->mode != PT_MODE_BOOTLOADER) {
			rc = -EPERM;
			goto exit_free;
		}

		pr_buf = ld->pip2_file_data.file_print_buf;

		rc = cmd->request_exclusive(dev,
				PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Failed to request exclusive rc=%d\n",
				__func__, rc);
			goto exit_free;
		}

		rc = cmd->nonhid_cmd->pip2_file_open(dev, file_handle);
		if (rc < 0) {
			pt_debug(dev, DL_ERROR,
				"%s: Failed to file_open rc=%d\n",
				__func__, rc);
			goto exit_release;
		}

		rc = cmd->nonhid_cmd->pip2_file_get_stats(dev, file_handle,
				&address, &file_size);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Failed to get_file_state rc=%d\n",
				__func__, rc);
			goto exit_file_close;
		}

		ld->pip2_file_data.file_print_size = 0;
		ld->pip2_file_data.file_max_size = file_size;
		ld->pip2_file_data.file_print_left = 0;
		print_idx += scnprintf(pr_buf, PIPE_BUF, "ROM_DATA:");

		if (ld->pip2_file_data.para_num == 1)
			ld->pip2_file_data.file_read_size = file_size;
		else if (ld->pip2_file_data.para_num == 2) {
			if (ld->pip2_file_data.file_offset < file_size)
				ld->pip2_file_data.file_read_size = file_size -
					ld->pip2_file_data.file_offset;
			else {
				rc = -EINVAL;
				pt_debug(dev, DL_ERROR,
					"%s: File read out of bounds rc=%d\n",
					__func__, rc);
				goto exit_file_close;
			}
		} else if (ld->pip2_file_data.para_num == 3) {
			if ((ld->pip2_file_data.file_read_size +
				ld->pip2_file_data.file_offset) > file_size) {
				rc = -EINVAL;
				pt_debug(dev, DL_ERROR,
					"%s: File read out of bounds rc=%d\n",
					__func__, rc);
				goto exit_file_close;
			}
		} else {
			pt_debug(dev, DL_ERROR,
				"%s: Invalid number of parameters!\n",
				__func__);
			goto exit_file_close;
		}
	}

	offset = ld->pip2_file_data.file_print_size +
			ld->pip2_file_data.file_offset;
	if ((offset >= ld->pip2_file_data.file_max_size) ||
		(ld->pip2_file_data.file_print_size >=
		ld->pip2_file_data.file_read_size))
		goto exit_file_close;
	else if ((ld->pip2_file_data.file_print_size +
		PIP2_FILE_WRITE_LEN_PER_PACKET) >=
		ld->pip2_file_data.file_read_size)
		read_len = ld->pip2_file_data.file_read_size -
		ld->pip2_file_data.file_print_size;
	else
		read_len = PIP2_FILE_WRITE_LEN_PER_PACKET;

	rc = cmd->nonhid_cmd->pip2_file_seek_offset(dev,
			file_handle, offset, 0);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Failed to seek file offset rc=%d\n",
			__func__, rc);
		goto exit_file_close;
	}

	read_size = cmd->nonhid_cmd->pip2_file_read(dev,
			file_handle, read_len, read_buf);
	if (read_size < 0) {
		pt_debug(dev, DL_ERROR, "%s: Failed to read file rc=%d\n",
				__func__, read_size);
		goto exit_file_close;
	}

	for (i = 0; i < read_size; i++)
		print_idx += scnprintf(pr_buf + print_idx,
			PIPE_BUF - print_idx,
			"%02X ", read_buf[i + PIP2_RESP_BODY_OFFSET]);
	ld->pip2_file_data.file_print_size += read_size;
	if (count < print_idx) {
		memcpy(buf, pr_buf, count);
		ld->pip2_file_data.file_print_left = print_idx - count;
		for (i = 0; i < ld->pip2_file_data.file_print_left; i++)
			pr_buf[i] = pr_buf[i+count];
		print_idx = count;
	} else {
		memcpy(buf, pr_buf, print_idx);
	}
	goto exit_for_next_read;

exit_file_close:
	if (rc)
		print_idx += scnprintf(pr_buf + print_idx,
			PIPE_BUF - print_idx, "(READ ERROR)\n");
	else if (ld->pip2_file_data.file_print_size)
		print_idx += scnprintf(pr_buf + print_idx,
				PIPE_BUF - print_idx,
				":(%d bytes)\n",
				ld->pip2_file_data.file_print_size);
	else
		print_idx += scnprintf(pr_buf + print_idx,
				PIPE_BUF - print_idx, "No Data\n");
	if (count < print_idx) {
		memcpy(buf, pr_buf, count);
		ld->pip2_file_data.file_print_left = print_idx - count;
		for (i = 0; i < ld->pip2_file_data.file_print_left; i++)
			pr_buf[i] = pr_buf[i+count];
		print_idx = count;
	} else {
		memcpy(buf, pr_buf, print_idx);
	}
	rc = cmd->nonhid_cmd->pip2_file_close(dev, file_handle);
	if (file_handle != rc)
		pt_debug(dev, DL_ERROR,
			"%s Failed to close file %d, rc = %d\n", __func__,
			file_handle, rc);
	/*
	 * Mark para_num as negative value to finish read method during
	 * next enter.
	 */
	ld->pip2_file_data.para_num = -1;

exit_for_next_read:
	pt_debug(dev, DL_INFO,
		"%s: %s=%d, %s=%d, %s=%d, %s=%d, %s=%d\n",
		__func__,
		"para_num", ld->pip2_file_data.para_num,
		"handle", ld->pip2_file_data.file_handle,
		"offset", ld->pip2_file_data.file_offset,
		"read", ld->pip2_file_data.file_read_size,
		"print", ld->pip2_file_data.file_print_size);
	return print_idx;
exit_release:
	cmd->release_exclusive(dev);
exit_free:
	kfree(ld->pip2_file_data.file_print_buf);
exit:
	if (rc) {
		ld->pip2_file_data.para_num = -2;
		print_idx = scnprintf(buf, count, "Status: %d\n", rc);
		return print_idx;
	} else
		return 0;
}

/*******************************************************************************
 * FUNCTION: pt_pip2_file_read_store
 *
 * SUMMARY: The write method for the pip2_file_read node. The passed
 *  in data is needed by read method.
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
static ssize_t pt_pip2_file_read_store(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t offset, size_t count)
{
	int rc = 0;
	int length;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	int ic_buffer[4] = {0};

	length = cmd->parse_sysfs_input(dev, buf, count, ic_buffer,
			ARRAY_SIZE(ic_buffer));
	if (length <= 0 || length > 3) {
		pt_debug(dev, DL_ERROR, "%s: Input format error!\n",
				__func__);
		ld->pip2_file_data.para_num = 0;
		rc = -EINVAL;
		goto error;
	}

	if (ic_buffer[0] < PIP2_FW_FILE || ic_buffer[0] > PIP2_FILE_7) {
		pt_debug(dev, DL_ERROR, "%s: Invalid file handle!\n",
				__func__);
		ld->pip2_file_data.para_num = 0;
		rc = -EINVAL;
		goto error;
	}

	switch (length) {
	case 1:
		ld->pip2_file_data.file_handle = ic_buffer[0];
		ld->pip2_file_data.file_offset = 0;
		ld->pip2_file_data.file_read_size = 0;
		break;
	case 2:
		ld->pip2_file_data.file_handle = ic_buffer[0];
		ld->pip2_file_data.file_offset = ic_buffer[1];
		ld->pip2_file_data.file_read_size = 0;
		break;
	case 3:
		ld->pip2_file_data.file_handle = ic_buffer[0];
		ld->pip2_file_data.file_offset = ic_buffer[1];
		ld->pip2_file_data.file_read_size = ic_buffer[2];
		break;
	default:
		break;
	}

	ld->pip2_file_data.para_num = length;
error:
	pt_debug(dev, DL_INFO,
		"%s: %s=%d, %s=%d, %s=%d, %s=%d, %s=%d\n",
		__func__,
		"para_num", ld->pip2_file_data.para_num,
		"handle", ld->pip2_file_data.file_handle,
		"offset", ld->pip2_file_data.file_offset,
		"read", ld->pip2_file_data.file_read_size,
		"print", ld->pip2_file_data.file_print_size);

	if (rc)
		return rc;
	return count;
}

static struct bin_attribute bin_attr_pip2_file_read = {
	.attr = {
		.name = "pip2_file_read",
		.mode = (0644),
	},
	.read = pt_pip2_file_read_show,
	.write = pt_pip2_file_read_store,
};

/******************************************************************************
 * FUNCTION: pt_pip2_file_crc_show
 *
 * SUMMARY: The show method for the "pip2_file_crc" sysfs node.
 *	Shows the CRC of a file or portion of the file.
 *
 *	NOTE: If function is called when DUT is already in BL mode, the DUT
 *	will remain in BL mode when function exits, however if DUT is in
 *	normal mode when function is called, the DUT will be forced into BL
 *	mode and then returned to normal mode when function exits.
 *
 *	NOTE: This sysfs node only can be used for BL version 1.8 or greater.
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes structure
 *	*buf  - pointer to print output buffer
 ******************************************************************************/
static ssize_t pt_pip2_file_crc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int rc = 0;
	u8 file_handle;
	int print_idx = 0;
	u8  read_buf[PT_MAX_PIP2_MSG_SIZE];
	u32 address, file_size;
	u32 length;
	u32 offset;
	u16 file_crc;
	u16 status;

	if (ld->pip2_fcrc.para_num == 0 ||
	    ld->pip2_fcrc.file_handle == 0 ||
	    ld->pip2_fcrc.file_read_size < 0 ||
	    ld->pip2_fcrc.file_offset < 0) {
		pt_debug(dev, DL_ERROR,
			"%s: Invalid parameters!\n",
			__func__);
		print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"Invalid parameters!\n", -EINVAL);
		return print_idx;
	}

	file_handle = ld->pip2_fcrc.file_handle;
	offset = ld->pip2_fcrc.file_offset;
	length = ld->pip2_fcrc.file_read_size;

	/* This functionality is only available in the BL */
	if (cd->mode != PT_MODE_BOOTLOADER) {
		rc = -EPERM;
		print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n", rc);
		goto exit;
	}

	rc = cmd->request_exclusive(dev, PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Failed to request exclusive rc=%d\n",
			__func__, rc);
		print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n", rc);
		goto exit;
	}

	rc = cmd->nonhid_cmd->pip2_file_open(dev, file_handle);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR,
			"%s: Failed to file_open rc=%d\n",
			__func__, rc);
		print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n", rc);
		goto exit_release;
	}

	rc = cmd->nonhid_cmd->pip2_file_get_stats(dev, file_handle,
			&address, &file_size);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Failed to get_file_state rc=%d\n",
			__func__, rc);
		print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n", rc);
		goto exit_file_close;
	}

	ld->pip2_fcrc.file_max_size = file_size;

	if (ld->pip2_fcrc.file_read_size > file_size
		|| ld->pip2_fcrc.file_offset > file_size
		|| ((ld->pip2_fcrc.file_offset +
		    ld->pip2_fcrc.file_read_size) > file_size)) {
		pt_debug(dev, DL_ERROR,
				"%s: Invalid parameters!\n",
				__func__);
		print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n", rc);
		goto exit_file_close;
	}

	rc = cmd->nonhid_cmd->pip2_file_crc(dev,
			file_handle, offset, length, read_buf);
	if (rc) {
		pt_debug(dev, DL_ERROR,
			"%s: Failed to get file crc, rc=%d\n",
			__func__, rc);
		print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n", rc);
	} else {
		status = read_buf[PIP2_RESP_STATUS_OFFSET];
		if (status == PIP2_RSP_ERR_NONE) {
			file_crc = get_unaligned_le16(&read_buf[5]);
			print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
				"Status: %d\n"
				"FILE CRC: %04X\n",
				status, file_crc);
		} else
			print_idx = snprintf(buf, PT_MAX_PRBUF_SIZE,
				"Status: %d\n"
				"FILE CRC: n/a\n",
				status);
	}

exit_file_close:
	rc = cmd->nonhid_cmd->pip2_file_close(dev, file_handle);
	if (file_handle != rc)
		pt_debug(dev, DL_ERROR,
			"%s Failed to close file %d, rc = %d\n", __func__,
			file_handle, rc);

exit_release:
	cmd->release_exclusive(dev);
exit:
	ld->pip2_fcrc.para_num = 0;
	return print_idx;
}

/*******************************************************************************
 * FUNCTION: pt_pip2_file_crc_store
 *
 * SUMMARY: The store method for the "pip2_file_crc" sysfs node. The passed in
 *  data including file_handle, offset and read size are needed by show method.
 *
 *	NOTE: This sysfs node only can be used for BL version 1.8 or greater..
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes structure
 *	*buf  - pointer to print output buffer
 *	 size - size of buffer
 ******************************************************************************/
static ssize_t pt_pip2_file_crc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u32 input_data[4] = {0};
	int length;
	int rc = 0;

	ld->pip2_fcrc.file_handle = PIP2_RAM_FILE;
	ld->pip2_fcrc.file_offset = -1;
	ld->pip2_fcrc.file_read_size = -1;

	length = cmd->parse_sysfs_input(dev, buf, size, input_data,
			ARRAY_SIZE(input_data));

	if (length != 3) {
		pt_debug(dev, DL_WARN, "%s: Invalid number of arguments\n",
			__func__);
		ld->pip2_fcrc.para_num = 0;
		rc = -EINVAL;
		goto exit;
	}

	if ((input_data[0] < PIP2_FW_FILE) || (input_data[0] > PIP2_FILE_MAX)) {
		pt_debug(dev, DL_WARN, "%s: Invalid file_no %d\n",
			__func__, input_data[0]);
		rc = -EINVAL;
		goto exit;
	}

	ld->pip2_fcrc.file_handle = input_data[0];
	ld->pip2_fcrc.file_offset = input_data[1];
	ld->pip2_fcrc.file_read_size = input_data[2];
	ld->pip2_fcrc.para_num = 3;

	pt_debug(dev, DL_INFO,
		"%s: %s=%d, %s=%d, %s=%d, %s=%d\n",
		__func__,
		"para_num", ld->pip2_fcrc.para_num,
		"handle", ld->pip2_fcrc.file_handle,
		"offset", ld->pip2_fcrc.file_offset,
		"length", ld->pip2_fcrc.file_read_size);
exit:
	if (rc)
		return rc;
	return size;
}

static DEVICE_ATTR(pip2_file_crc, 0644,
	pt_pip2_file_crc_show, pt_pip2_file_crc_store);
#endif
/*******************************************************************************
 * FUNCTION: pt_pip2_file_erase_show
 *
 * SUMMARY: The show method for the "pip2_file_erase" sysfs node.
 *	Prints current erase status to output buffer.
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes structure
 *	*buf  - pointer to print output buffer
 ******************************************************************************/
static ssize_t pt_pip2_file_erase_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	struct pt_core_data *cd = dev_get_drvdata(dev);
	u8 file_handle;
	u8 file = ld->pip2_file_erase_file_no;
	int rc;

	pip2_erase_status = -1;
	pip2_erase_rc = 0;

	if (file == PIP2_RAM_FILE) {
		rc = -EINVAL;
		goto exit;
	}

	/* This functionality is only available in the BL */
	if (cd->mode != PT_MODE_BOOTLOADER) {
		rc = -EPERM;
		goto exit;
	}

	rc = cmd->request_exclusive(dev, PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR,
			"%s: Failed to get exclusive access rc=%d\n",
			__func__, rc);
		goto exit;
	}

	file_handle = cmd->nonhid_cmd->pip2_file_open(dev, file);
	if (file_handle != file) {
		rc = -EBADF;
		goto exit_release;
	}

	file_handle = cmd->nonhid_cmd->pip2_file_erase(dev, file,
		&pip2_erase_status);
	if (file_handle < 0) {
		rc = file_handle;
		pt_debug(dev, DL_INFO, "%s: File erase error rc = %d\n",
			__func__, rc);
	} else if (file_handle == file) {
		pt_debug(dev, DL_INFO, "%s: File %d erased\n",
			__func__, file_handle);
	} else {
		rc = -EBADF;
	}

	file_handle = cmd->nonhid_cmd->pip2_file_close(dev, file);
	if (file_handle != file && !rc)
		rc = -EBADF;

exit_release:
	cmd->release_exclusive(dev);
exit:
	pip2_erase_rc = rc;
	if (pip2_erase_status == -1) {
		return snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"Erase Status: n/a\n",
			pip2_erase_rc);
	}
	return snprintf(buf, PT_MAX_PRBUF_SIZE,
		"Status: %d\n"
		"Erase Status: 0x%02X\n",
		pip2_erase_rc, pip2_erase_status);
}

/*******************************************************************************
 * FUNCTION: pt_pip2_file_erase_store
 *
 * SUMMARY: The store method for the "pip2_file_erase" sysfs node. Allows the
 *	caller to provide the file number to erase in FLASH.
 *
 *	NOTE: The DUT must be in BL mode before calling this function.
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes structure
 *	*buf  - pointer to print output buffer
 *	 size - size of buffer
 ******************************************************************************/
static ssize_t pt_pip2_file_erase_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);
	u32 input_data[2] = {0};
	int length;
	int rc = 0;

	length = cmd->parse_sysfs_input(dev, buf, size, input_data,
			ARRAY_SIZE(input_data));

	if (length != 1) {
		pt_debug(dev, DL_WARN, "%s: Invalid number of arguments\n",
			__func__);
		rc = -EINVAL;
		goto exit;
	}

	/* Only allow valid files to be erased */
	if (input_data[0] < PIP2_FW_FILE || input_data[0] > PIP2_FILE_MAX) {
		pip2_erase_status = PIP2_RSP_ERR_BAD_FILE;
		pt_debug(dev, DL_ERROR, "%s: ERROR - Invalid File\n",
			__func__);
		rc = -EINVAL;
		goto exit;
	}

	ld->pip2_file_erase_file_no = input_data[0];

exit:
	if (rc)
		return rc;
	return size;
}

static DEVICE_ATTR(pip2_file_erase, 0644,
	pt_pip2_file_erase_show, pt_pip2_file_erase_store);

/*******************************************************************************
 * FUNCTION: pt_pip2_bl_status_show
 *
 * SUMMARY: The show method for the pip2_bl_status sysfs node.
 *	Shows the percent completion of the current BL or an error message.
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes structure
 *	*buf  - pointer to print output buffer
 ******************************************************************************/
static ssize_t pt_pip2_bl_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	u8 status = update_fw_status;

	if (update_fw_status <= UPDATE_FW_COMPLETE) {
		pt_debug(dev, DL_DEBUG,
			"%s BL_STATUS = %d\n", __func__, update_fw_status);
		return sprintf(buf, "%d\n", update_fw_status);
	}

	switch (status) {
	case UPDATE_FW_GENERAL_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - General programming failure\n", status);
		break;
	case UPDATE_FW_PIP_VERSION_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Wrong PIP version detected\n", status);
		break;
	case UPDATE_FW_VERSION_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - FW vervion newer than bin file\n", status);
		break;
	case UPDATE_FW_ERASE_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - ROM BL failed to erase FW file in FLASH\n",
			status);
		break;
	case UPDATE_FW_FILE_CLOSE_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - ROM BL failed to close FW file in FLASH\n",
			status);
		break;
	case UPDATE_FW_WRITE_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - ROM BL file write failure\n", status);
		break;
	case UPDATE_FW_EXECUTE_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - ROM BL failed to execute RAM image\n",
			status);
		break;
	case UPDATE_FW_RESET_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Reset DUT failure\n",
			status);
		break;
	case UPDATE_FW_MODE_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Program complete, Incorrect BL/APP mode detected after reset sentinel\n",
			status);
		break;
	case UPDATE_FW_ENTER_BL_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Could not enter the BL\n", status);
		break;
	case UPDATE_FW_FILE_OPEN_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - ROM BL failed to open FW file in FLASH\n",
			status);
		break;
	case UPDATE_FW_SENTINEL_NOT_SEEN:
		ret = sprintf(buf,
			"ERROR: %d - FW Reset Sentinel not seen after XRES\n",
			status);
		break;
	case UPDATE_FW_EXCLUSIVE_ACCESS_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Failed to get DUT exclusive access\n",
			status);
		break;
	case UPDATE_FW_NO_FW_PROVIDED:
		ret = sprintf(buf,
			"ERROR: %d - No FW provided to load\n", status);
		break;
	case UPDATE_FW_INVALID_FW_IMAGE:
		ret = sprintf(buf, "ERROR: %d - Invalid FW image\n", status);
		break;
	case UPDATE_FW_MISALIGN_FW_IMAGE:
		ret = sprintf(buf,
			"ERROR: %d - FW image is misaligned\n", status);
		break;
	case UPDATE_FW_SYSTEM_NOMEM:
		ret = sprintf(buf,
			"ERROR: %d - Failed to alloc memory\n", status);
		break;
	case UPDATE_FW_INIT_BL_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Failed to init bootloader\n", status);
		break;
	case UPDATE_FW_PARSE_ROW_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Failed to parse row of FW image\n",
			status);
		break;
	case UPDATE_FW_PROGRAM_ROW_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Failed to progrem FW image\n", status);
		break;
	case UPDATE_FW_EXIT_BL_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Failed to exit bootloader\n", status);
		break;
	case UPDATE_FW_CHECK_SUM_ERROR:
		ret = sprintf(buf,
			"ERROR: %d - Failed to verify app checksum\n", status);
		break;
	case UPDATE_FW_NO_PLATFORM_DATA:
		ret = sprintf(buf,
			"ERROR: %d - No platform data\n", status);
		break;
	case UPDATE_FW_UNDEFINED_ERROR:
	default:
		ret = sprintf(buf, "ERROR: %d - Unknown error\n", status);
		break;
	}
	return ret;
}
static DEVICE_ATTR(pip2_bl_status, 0444, pt_pip2_bl_status_show, NULL);
#if PT_FW_UPGRADE
static DEVICE_ATTR(update_fw_status, 0444, pt_pip2_bl_status_show, NULL);
#endif
/*******************************************************************************
 * FUNCTION: pt_pip2_get_last_error_show
 *
 * SUMMARY: The show method for the pip2_get_last_error_show sysfs node.
 *	Shows the last BL error code.
 *
 *	NOTE: If function is called when DUT is already in BL mode, the DUT
 *	will remain in BL mode when function exits, however if DUT is in
 *	normal mode when function is called, the DUT will be forced into BL
 *	mode and then returned to normal mode when function exits.
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes structure
 *	*buf  - pointer to print output buffer
 ******************************************************************************/
static ssize_t pt_pip2_get_last_error_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	int rc;
	u8 read_buf[256] = {0};

	rc = cmd->request_exclusive(dev, PT_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc)
		goto exit;

	cmd->request_stop_wd(dev);

	/* This functionality is only available in the BL */
	if (cd->mode != PT_MODE_BOOTLOADER) {
		rc = -EPERM;
		goto exit_release;
	}

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	/* Get and log the last error(s) */
	rc = _pt_pip2_log_last_error(dev, read_buf);
#endif

exit_release:
	cmd->release_exclusive(dev);
exit:
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	if (rc)
		return snprintf(buf, PT_MAX_PRBUF_SIZE, "Status: %d\n", rc);
#endif

	if (read_buf[PIP2_RESP_STATUS_OFFSET] == PIP2_RSP_ERR_NONE) {
		return snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"Last Error No: 0x%02X\n",
			PIP2_RSP_ERR_NONE,
			read_buf[PIP2_RESP_BODY_OFFSET]);
	} else {
		return snprintf(buf, PT_MAX_PRBUF_SIZE,
			"Status: %d\n"
			"Last Error No: n/a\n",
			read_buf[PIP2_RESP_STATUS_OFFSET]);
	}
}
static DEVICE_ATTR(pip2_get_last_error, 0444,
	pt_pip2_get_last_error_show, NULL);

#if PT_FW_UPGRADE
/*******************************************************************************
 * FUNCTION: pt_loader_attention
 *
 * SUMMARY: Function to be registered to TTDL attention list to set up
 *  int_running semaphore.
 *
 * RETURN:
 *   0 = success
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static int pt_loader_attention(struct device *dev)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);

	complete(&ld->int_running);
	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_fw_upgrade_cb
 *
 * SUMMARY: Function to be registered to TTDL attention list to allow upgrade
 *  if host cannot get response from firmware with ping command.
 * RETURN:
 *   0 = success
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static int pt_fw_upgrade_cb(struct device *dev)
{
	u8 dut_gen = cmd->request_dut_generation(dev);

#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE
	if (dut_gen == DUT_PIP1_ONLY) {
		pt_debug(dev, DL_WARN, "%s: Upgrade Platform FW", __func__);
		if (!upgrade_firmware_from_platform(dev, false))
			return 0;
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	pt_debug(dev, DL_WARN, "%s: Upgrade Builtin FW", __func__);
	if (dut_gen == DUT_PIP2_CAPABLE) {
		if (!pt_pip2_upgrade_firmware_from_builtin(dev))
			return 0;
		pt_debug(dev, DL_WARN, "%s: Builtin FW upgrade failed",
			__func__);
	} else {
		if (!upgrade_firmware_from_builtin(dev))
			return 0;
	}
#endif
	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_cancel_fw_upgrade_cb
 *
 * SUMMARY: Function to be registered to TTDL attention list to allow an upgrade
 *	to be canceled.
 * RETURN:
 *   0 = success
 *
 * PARAMETERS:
 *  *dev  - pointer to device structure
 ******************************************************************************/
static int pt_cancel_fw_upgrade_cb(struct device *dev)
{
	struct pt_loader_data *ld = pt_get_loader_data(dev);

	pt_debug(dev, DL_WARN,
		"%s: CANCELLING All Loader work\n", __func__);
	cancel_work_sync(&ld->bl_from_file);
	cancel_work_sync(&ld->pip2_bl_from_file);
	cancel_work_sync(&ld->calibration_work);
	cancel_work_sync(&ld->fw_and_config_upgrade);

	return 0;
}
#endif /* PT_FW_UPGRADE */

/*******************************************************************************
 * FUNCTION: pt_loader_probe
 *
 * SUMMARY: The probe function for the FW loader.
 *
 * PARAMETERS:
 *   *dev   - pointer to device structure
 *  **data  - double pointer to the loader data to be created here
 ******************************************************************************/
static int pt_loader_probe(struct device *dev, void **data)
{
	struct pt_loader_data *ld;
	struct pip2_loader_data *pip2_data;
	struct pt_platform_data *pdata = dev_get_platdata(dev);
	int rc;
	u8 dut_gen = cmd->request_dut_generation(dev);

#ifdef TTDL_DIAGNOSTICS
	pt_debug(dev, DL_INFO,
		"%s: entering %s\n", __func__, __func__);
#endif /* TTDL_DIAGNOSTICS */

	if (!pdata || !pdata->loader_pdata) {
		pt_debug(dev, DL_ERROR,
			"%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	ld = kzalloc(sizeof(*ld), GFP_KERNEL);
	if (!ld) {
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

#if PT_FW_UPGRADE
	/* Initialize boot loader status */
	if (update_fw_status != UPDATE_FW_COMPLETE)
		_pt_pip2_update_bl_status(dev, UPDATE_FW_IDLE, PT_NO_INC);
#endif

	if (dut_gen == DUT_PIP2_CAPABLE) {
		pip2_data = kzalloc(sizeof(*pip2_data), GFP_KERNEL);
		if (!pip2_data) {
			rc = -ENOMEM;
			goto error_alloc_pip2_data_failed;
		}
		pip2_data->dev = dev;
		ld->pip2_data = pip2_data;

#if PT_FW_UPGRADE
		rc = device_create_file(dev, &dev_attr_update_fw_status);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating update_fw_status\n",
				__func__);
			goto remove_files;
		}
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		rc = device_create_file(dev, &dev_attr_pt_bl_from_file);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pt_bl_from_file\n",
				__func__);
			goto remove_files;
		}
		rc = device_create_file(dev, &dev_attr_update_fw);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating update_fw\n",
				__func__);
			goto remove_files;
		}
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		rc = device_create_file(dev, &dev_attr_pip2_manual_upgrade);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pip2_manual_upgrade\n",
				__func__);
			goto remove_files;
		}
		rc = device_create_file(dev, &dev_attr_pip2_manual_ram_upgrade);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pip2_manual_ram_upgrade\n",
				__func__);
			goto remove_files;
		}
		rc = device_create_file(dev, &dev_attr_pip2_file_write);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pip2_file_write\n",
				__func__);
			goto remove_files;
		}
		rc = device_create_file(dev, &dev_attr_pip2_bl_from_file);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pip2_bl_from_file\n",
				__func__);
			goto remove_files;
		}

#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */
		rc = device_create_file(dev, &dev_attr_pip2_file_erase);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pip2_file_erase\n",
				__func__);
			goto remove_files;
		}
		rc = device_create_file(dev, &dev_attr_pip2_bl_status);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pip2_bl_status\n",
				__func__);
			goto remove_files;
		}

#ifdef TTDL_DIAGNOSTICS
		rc = device_create_file(dev, &dev_attr_pip2_get_last_error);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pip2_get_last_error\n",
				__func__);
			goto remove_files;
		}
		rc = device_create_bin_file(dev, &bin_attr_pip2_file_read);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating bin_attr_pip2_file_read\n",
				__func__);
			goto remove_files;
		}
		rc = device_create_file(dev, &dev_attr_pip2_file_crc);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pip2_file_crc\n",
				__func__);
			goto remove_files;
		}
#endif
	} else {
#if PT_FW_UPGRADE
		rc = device_create_file(dev, &dev_attr_update_fw_status);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating update_fw_status\n",
				__func__);
			goto remove_files;
		}
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		rc = device_create_file(dev, &dev_attr_pt_bl_from_file);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating pt_bl_from_file\n",
				__func__);
			goto remove_files;
		}
		rc = device_create_file(dev, &dev_attr_update_fw);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating update_fw\n",
				__func__);
			goto remove_files;
		}
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */
#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE
		rc = device_create_file(dev, &dev_attr_forced_upgrade);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating forced_upgrade\n",
				__func__);
			goto remove_files;
		}
#endif /* CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE */
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		rc = device_create_file(dev, &dev_attr_manual_upgrade);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating manual_upgrade\n",
				__func__);
			goto remove_files;
		}
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */
#ifdef CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE
		rc = device_create_file(dev, &dev_attr_config_loading);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating config_loading\n",
				__func__);
			goto remove_files;
		}

		rc = device_create_bin_file(dev, &bin_attr_config_data);
		if (rc) {
			pt_debug(dev, DL_ERROR,
				"%s: Error creating config_data\n",
				__func__);
			goto remove_files;
		}
#endif /* CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE */
	}

	/* Default erase file to an invalid file */
	ld->pip2_file_erase_file_no = PIP2_RAM_FILE;

	ld->loader_pdata = pdata->loader_pdata;
	ld->dev = dev;
	*data = ld;

#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	INIT_WORK(&ld->bl_from_file, pt_bl_from_file_work);
	INIT_WORK(&ld->pip2_bl_from_file, pt_pip2_bl_from_file_work);
#endif /* CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE */

#if PT_FW_UPGRADE
	init_completion(&ld->int_running);

	cmd->subscribe_attention(dev, PT_ATTEN_IRQ, PT_LOADER_NAME,
		pt_loader_attention, PT_MODE_BOOTLOADER);

	cmd->subscribe_attention(dev, PT_ATTEN_LOADER, PT_LOADER_NAME,
		pt_fw_upgrade_cb, PT_MODE_UNKNOWN);

	cmd->subscribe_attention(dev, PT_ATTEN_CANCEL_LOADER, PT_LOADER_NAME,
		pt_cancel_fw_upgrade_cb, PT_MODE_UNKNOWN);
#endif
#if PT_FW_UPGRADE || PT_TTCONFIG_UPGRADE
	pt_debug(dev, DL_INFO, "%s: INIT_WORK pt_calibrate_idacs\n",
		__func__);
	init_completion(&ld->calibration_complete);
	INIT_WORK(&ld->calibration_work, pt_calibrate_idacs);
#endif

#ifdef CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE
	if (dut_gen == DUT_PIP1_ONLY)
		mutex_init(&ld->config_lock);
#endif

	pt_debug(dev, DL_INFO, "%s: Schedule FW upgrade work\n", __func__);
	INIT_WORK(&ld->fw_and_config_upgrade, pt_fw_and_config_upgrade);
	schedule_work(&ld->fw_and_config_upgrade);

	pt_debug(dev, DL_INFO, "%s: Successful probe %s\n",
		__func__, dev_name(dev));
	return 0;


remove_files:
#ifdef CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE
	device_remove_file(dev, &dev_attr_config_loading);
#endif
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	device_remove_file(dev, &dev_attr_manual_upgrade);
#endif
#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE
	device_remove_file(dev, &dev_attr_forced_upgrade);
#endif
#ifdef TTDL_DIAGNOSTICS
	device_remove_file(dev, &dev_attr_pip2_get_last_error);
	device_remove_bin_file(dev, &bin_attr_pip2_file_read);
	device_remove_file(dev, &dev_attr_pip2_file_crc);
#endif
	device_remove_file(dev, &dev_attr_pip2_bl_status);
	device_remove_file(dev, &dev_attr_pip2_file_erase);
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
	device_remove_file(dev, &dev_attr_pip2_bl_from_file);
	device_remove_file(dev, &dev_attr_pip2_file_write);
	device_remove_file(dev, &dev_attr_pt_bl_from_file);
	device_remove_file(dev, &dev_attr_pip2_manual_ram_upgrade);
	device_remove_file(dev, &dev_attr_pip2_manual_upgrade);
	device_remove_file(dev, &dev_attr_update_fw);
	device_remove_file(dev, &dev_attr_update_fw_status);
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */

	kfree(ld->pip2_data);
error_alloc_pip2_data_failed:
	kfree(ld);
error_alloc_data_failed:
error_no_pdata:
	pt_debug(dev, DL_ERROR, "%s failed.\n", __func__);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_loader_release
 *
 * SUMMARY: Remove function for loader module that does following cleanup:
 *  - Unsubscibe all registered attention tasks
 *  - Removes all created sysfs nodes
 *  - Frees all pointers
 *
 * PARAMETERS:
 *  *dev   - pointer to device structure
 *  *data  - pointer to the loader data
 ******************************************************************************/
static void pt_loader_release(struct device *dev, void *data)
{
	struct pt_loader_data *ld = (struct pt_loader_data *)data;
	u8 dut_gen =  cmd->request_dut_generation(dev);

#if PT_FW_UPGRADE
	cmd->unsubscribe_attention(dev, PT_ATTEN_IRQ, PT_LOADER_NAME,
		pt_loader_attention, PT_MODE_BOOTLOADER);

	cmd->unsubscribe_attention(dev, PT_ATTEN_LOADER, PT_LOADER_NAME,
		pt_fw_upgrade_cb, PT_MODE_UNKNOWN);

	cmd->unsubscribe_attention(dev, PT_ATTEN_CANCEL_LOADER, PT_LOADER_NAME,
		pt_cancel_fw_upgrade_cb, PT_MODE_UNKNOWN);
#endif
#if PT_FW_UPGRADE
	device_remove_file(dev, &dev_attr_update_fw_status);
#endif
	if (dut_gen == DUT_PIP2_CAPABLE) {
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		device_remove_file(dev, &dev_attr_update_fw);
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */
#ifdef TTDL_DIAGNOSTICS
		device_remove_bin_file(dev, &bin_attr_pip2_file_read);
		device_remove_file(dev, &dev_attr_pip2_file_crc);
#endif
		device_remove_file(dev, &dev_attr_pip2_get_last_error);
		device_remove_file(dev, &dev_attr_pip2_bl_status);
		device_remove_file(dev, &dev_attr_pip2_file_erase);
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		device_remove_file(dev, &dev_attr_pip2_file_write);
		device_remove_file(dev, &dev_attr_pip2_bl_from_file);
		device_remove_file(dev, &dev_attr_pt_bl_from_file);
		device_remove_file(dev, &dev_attr_pip2_manual_ram_upgrade);
		device_remove_file(dev, &dev_attr_pip2_manual_upgrade);
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */
		kfree(ld->pip2_data);
	} else {
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		device_remove_file(dev, &dev_attr_update_fw);
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */
#ifdef CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE
		device_remove_bin_file(dev, &bin_attr_config_data);
		device_remove_file(dev, &dev_attr_config_loading);
		if (!ld->config_data)
			kfree(ld->config_data);
#endif /* CONFIG_TOUCHSCREEN_PARADE_MANUAL_TTCONFIG_UPGRADE */
#ifdef CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE
		device_remove_file(dev, &dev_attr_pt_bl_from_file);
		device_remove_file(dev, &dev_attr_manual_upgrade);
#endif /* CONFIG_TOUCHSCREEN_PARADE_BINARY_FW_UPGRADE */
#ifdef CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE
		device_remove_file(dev, &dev_attr_forced_upgrade);
#endif /* CONFIG_TOUCHSCREEN_PARADE_PLATFORM_FW_UPGRADE */
	}
	kfree(ld);
}

static struct pt_module loader_module = {
	.name = PT_LOADER_NAME,
	.probe = pt_loader_probe,
	.release = pt_loader_release,
};

/*******************************************************************************
 * FUNCTION: pt_loader_init
 *
 * SUMMARY: Initialize function for loader module which to register
 * loader_module into TTDL module list.
 *
 * RETURN:
 *   0 = success
 *  !0 = failure
 *
 * PARAMETERS:
 *  *dev   - pointer to device structure
 *  *data  - pointer to the loader data
 ******************************************************************************/
static int __init pt_loader_init(void)
{
	int rc;

	cmd = pt_get_commands();
	if (!cmd)
		return -EINVAL;

	rc = pt_register_module(&loader_module);
	if (rc < 0) {
		pr_err("%s: Error, failed registering module\n",
			__func__);
			return rc;
	}

	pr_info("%s: Parade FW Loader Driver (Version %s) rc=%d\n",
		 __func__, PT_DRIVER_VERSION, rc);
	return 0;
}
module_init(pt_loader_init);

/*******************************************************************************
 * FUNCTION: pt_loader_exit
 *
 * SUMMARY: Exit function for loader module which to unregister loader_module
 * from TTDL module list.
 *
 ******************************************************************************/
static void __exit pt_loader_exit(void)
{
	pt_unregister_module(&loader_module);
}
module_exit(pt_loader_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Parade TrueTouch(R) Standard Product FW Loader Driver");
MODULE_AUTHOR("Parade Technologies <ttdrivers@paradetech.com>");
