/* Himax Android Driver Sample Code for debug nodes
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

#include "himax.h"
#include "himax_debug.h"
#include "himax_ic_core.h"

static ssize_t himax_crc_test_read(struct himax_ts_data *ts, char *buf, size_t len)
{
	ssize_t ret = 0;
	uint8_t result = 0;
	uint32_t size = 0;

	ts->core_fp.fp_sense_off(ts, true);
	msleep(20);

	size = FW_SIZE_128k;

	result = ts->core_fp.fp_calculateChecksum(ts, false, size);
	ts->core_fp.fp_sense_on(ts, 0x01);

	if (result)
		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
				"CRC test is Pass!\n");
	else
		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
				"CRC test is Fail!\n");

	return ret;
}

static ssize_t himax_vendor_read(struct file *file, char *buf, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = pde_data(file_inode(file));
	ssize_t ret = 0;

	ts->core_fp.fp_reload_disable(ts, 0);
	ts->core_fp.fp_power_on_init(ts);
	ts->core_fp.fp_read_FW_ver(ts);
	ts->core_fp.fp_touch_information(ts);

	if (!ts->hx_proc_send_flag) {
		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
				"IC = %s\n", ts->chip_name);

		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
				"Architecture Version = 0x%2.2X\n", ts->ic_data->vendor_arch_ver);

		if (ts->chip_cell_type == CHIP_IS_ON_CELL) {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"CONFIG_VER = 0x%2.2X\n", ts->ic_data->vendor_config_ver);
		} else {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"FW Touch Config. Version = 0x%2.2X\n",
					ts->ic_data->vendor_touch_cfg_ver);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"FW Display Config. Version = 0x%2.2X\n",
					ts->ic_data->vendor_display_cfg_ver);
		}

		if (ts->ic_data->vendor_cid_maj_ver < 0 && ts->ic_data->vendor_cid_min_ver < 0) {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"CID = NULL\n");
		} else {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"CID = 0x%2.2X\n",
					(ts->ic_data->vendor_cid_maj_ver << 8 |
					 ts->ic_data->vendor_cid_min_ver));
		}

		if (ts->ic_data->vendor_panel_ver < 0) {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Panel Version = NULL\n");
		} else {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Panel Version = 0x%2.2X\n", ts->ic_data->vendor_panel_ver);
		}
		if (ts->chip_cell_type == CHIP_IS_IN_CELL) {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Cusomer = %s\n", ts->ic_data->vendor_cus_info);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Project Name = %s\n", ts->ic_data->vendor_proj_info);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Config. Date  = %s\n", ts->ic_data->vendor_config_date);
		}
		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "\n");
		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
				"Himax Touch Driver Version:\n");
		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "%s\n",
				HIMAX_DRIVER_VER);
		ts->hx_proc_send_flag = 1;

		if (copy_to_user(buf, ts->debug.buf_tmp, (len > BUF_SIZE) ? BUF_SIZE : len))
			I("%s,here:%d\n", __func__, __LINE__);

	} else {
		ts->hx_proc_send_flag = 0;
	}

	return ret;
}

static const struct proc_ops himax_proc_vendor_ops = {
	.proc_read = himax_vendor_read,
};

static ssize_t himax_attn_read(struct himax_ts_data *ts, char *buf, size_t len)
{
	ssize_t ret = 0;

	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "attn = %x\n",
			himax_int_gpio_read(ts->pdata->gpio_irq));

	return ret;
}

static ssize_t himax_int_en_read(struct himax_ts_data *ts, char *buf, size_t len)
{
	size_t ret = 0;

	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "%d\n",
			ts->irq_enabled);

	return ret;
}

static ssize_t himax_int_en_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	int ret = 0;

	if (len >= 12) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	if (buf[0] == '0') {
		himax_int_enable(ts, 0);
	} else if (buf[0] == '1') {
		himax_int_enable(ts, 1);
	} else if (buf[0] == '2') {
		himax_int_enable(ts, 0);
		free_irq(ts->hx_irq, ts);
		ts->irq_enabled = 0;
	} else if (buf[0] == '3') {
		ret = himax_int_en_set(ts);

		if (ret == 0) {
			ts->irq_enabled = 1;
			atomic_set(&ts->irq_state, 1);
		}
	} else
		return -EINVAL;

	return len;
}

static ssize_t himax_layout_read(struct himax_ts_data *ts, char *buf, size_t len)
{
	size_t ret = 0;

	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "%d ",
			ts->pdata->abs_x_min);
	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "%d ",
			ts->pdata->abs_x_max);
	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "%d ",
			ts->pdata->abs_y_min);
	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "%d ",
			ts->pdata->abs_y_max);
	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "\n");

	return ret;
}

static ssize_t himax_layout_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	char buf_tmp[5] = { 0 };
	int i = 0, j = 0, k = 0, ret;
	unsigned long value;
	int layout[4] = { 0 };

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	for (i = 0; i < 20; i++) {
		if (buf[i] == ',' || buf[i] == '\n') {
			memset(buf_tmp, 0x0, sizeof(buf_tmp));

			if (i - j <= 5) {
				memcpy(buf_tmp, buf + j, i - j);
			} else {
				I("buffer size is over 5 char\n");
				return len;
			}

			j = i + 1;

			if (k < 4) {
				ret = kstrtoul(buf_tmp, 10, &value);
				I("%s failed to get value, ret:%d\n", __func__, ret);
				layout[k++] = value;
			}
		}
	}

	if (k == 4) {
		ts->pdata->abs_x_min = layout[0];
		ts->pdata->abs_x_max = (layout[1] - 1);
		ts->pdata->abs_y_min = layout[2];
		ts->pdata->abs_y_max = (layout[3] - 1);
		I("%d, %d, %d, %d\n", ts->pdata->abs_x_min, ts->pdata->abs_x_max,
		  ts->pdata->abs_y_min, ts->pdata->abs_y_max);
		input_unregister_device(ts->input_dev);
		himax_input_register(ts);
	} else {
		I("ERR@%d, %d, %d, %d\n", ts->pdata->abs_x_min, ts->pdata->abs_x_max,
		  ts->pdata->abs_y_min, ts->pdata->abs_y_max);
	}

	return len;
}

static ssize_t himax_debug_level_read(struct himax_ts_data *ts, char *buf, size_t len)
{
	size_t ret = 0;

	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "%u\n",
			ts->debug_log_level);

	if (copy_to_user(buf, ts->debug.buf_tmp, (len > BUF_SIZE) ? BUF_SIZE : len))
		I("%s,here:%d\n", __func__, __LINE__);

	return ret;
}

static ssize_t himax_debug_level_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	int i;

	if (len >= 12) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	ts->debug_log_level = 0;

	for (i = 0; i < len; i++) {
		if (buf[i] >= '0' && buf[i] <= '9')
			ts->debug_log_level |= (buf[i] - '0');
		else if (buf[i] >= 'A' && buf[i] <= 'F')
			ts->debug_log_level |= (buf[i] - 'A' + 10);
		else if (buf[i] >= 'a' && buf[i] <= 'f')
			ts->debug_log_level |= (buf[i] - 'a' + 10);

		if (i != len - 1)
			ts->debug_log_level <<= 4;
	}
	I("Now debug level value=%d\n", ts->debug_log_level);

	if (ts->debug_log_level & BIT(4)) {
		I("Turn on/Enable Debug Mode for Inspection!\n");
		goto END_FUNC;
	}

	if (ts->debug_log_level & BIT(3)) {
		if (ts->pdata->screenWidth > 0 && ts->pdata->screenHeight > 0 &&
		    (ts->pdata->abs_x_max - ts->pdata->abs_x_min) > 0 &&
		    (ts->pdata->abs_y_max - ts->pdata->abs_y_min) > 0) {
			ts->widthFactor = (ts->pdata->screenWidth << SHIFTBITS) /
					  (ts->pdata->abs_x_max - ts->pdata->abs_x_min);
			ts->heightFactor = (ts->pdata->screenHeight << SHIFTBITS) /
					   (ts->pdata->abs_y_max - ts->pdata->abs_y_min);

			if (ts->widthFactor > 0 && ts->heightFactor > 0) {
				ts->useScreenRes = 1;
			} else {
				ts->heightFactor = 0;
				ts->widthFactor = 0;
				ts->useScreenRes = 0;
			}
		} else {
			I("Enable finger debug with raw position mode!\n");
		}
	} else {
		ts->useScreenRes = 0;
		ts->widthFactor = 0;
		ts->heightFactor = 0;
	}
END_FUNC:
	return len;
}

static ssize_t himax_proc_register_read(struct himax_ts_data *ts, char *buf, size_t len)
{
	int ret = 0;
	uint16_t loop_i;

	memset(ts->debug.reg_read_data, 0x00, 128 * sizeof(uint8_t));

	I("himax_register_show: %02X,%02X,%02X,%02X\n", ts->debug.reg_cmd[3], ts->debug.reg_cmd[2],
	  ts->debug.reg_cmd[1], ts->debug.reg_cmd[0]);
	himax_mcu_register_read(ts, ts->debug.reg_cmd, 128, ts->debug.reg_read_data,
				ts->debug.cfg_flag);

	ret += snprintf(ts->debug.buf_tmp + ret, len - ret, "command:  %02X,%02X,%02X,%02X\n",
			ts->debug.reg_cmd[3], ts->debug.reg_cmd[2], ts->debug.reg_cmd[1],
			ts->debug.reg_cmd[0]);

	for (loop_i = 0; loop_i < 128; loop_i++) {
		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
				"0x%2.2X ", ts->debug.reg_read_data[loop_i]);
		if ((loop_i % 16) == 15)
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"\n");
	}

	ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret, "\n");

	return ret;
}

static ssize_t himax_proc_register_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	char buff_tmp[16] = { 0 };
	uint8_t length = 0;
	unsigned long result = 0;
	uint8_t loop_i = 0;
	uint16_t base = 2;
	char *data_str = NULL;
	uint8_t w_data[20] = { 0 };
	uint8_t x_pos[20] = { 0 };
	uint8_t count = 0;

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	memset(ts->debug.reg_cmd, 0x0, sizeof(ts->debug.reg_cmd));

	I("himax %s\n", buf);

	if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':' && buf[2] == 'x') {
		length = strlen(buf);

		/* I("%s: length = %d.\n", __func__,length); */
		for (loop_i = 0; loop_i < length; loop_i++) {
			/* find postion of 'x' */
			if (buf[loop_i] == 'x') {
				x_pos[count] = loop_i;
				count++;
			}
		}

		data_str = strrchr(buf, 'x');
		I("%s: %s.\n", __func__, data_str);
		length = strlen(data_str + 1);

		switch (buf[0]) {
		case 'r':
			if (buf[3] == 'F' && buf[4] == 'E' && length == 4) {
				length = length - base;
				ts->debug.cfg_flag = 1;
				memcpy(buff_tmp, data_str + base + 1, length);
			} else {
				ts->debug.cfg_flag = 0;
				memcpy(buff_tmp, data_str + 1, length);
			}
			ts->debug.byte_length = length / 2;
			if (!kstrtoul(buff_tmp, 16, &result)) {
				for (loop_i = 0; loop_i < ts->debug.byte_length; loop_i++)
					ts->debug.reg_cmd[loop_i] = (uint8_t)(result >> loop_i * 8);
			}

			break;
		case 'w':
			if (buf[3] == 'F' && buf[4] == 'E') {
				ts->debug.cfg_flag = 1;
				memcpy(buff_tmp, buf + base + 3, length);
			} else {
				ts->debug.cfg_flag = 0;
				memcpy(buff_tmp, buf + 3, length);
			}

			if (count < 3) {
				ts->debug.byte_length = length / 2;

				if (!kstrtoul(buff_tmp, 16, &result)) {
					/* command */
					for (loop_i = 0; loop_i < ts->debug.byte_length; loop_i++) {
						ts->debug.reg_cmd[loop_i] =
							(uint8_t)(result >> loop_i * 8);
					}
				}

				if (!kstrtoul(data_str + 1, 16, &result)) {
					/* data */
					for (loop_i = 0; loop_i < ts->debug.byte_length; loop_i++) {
						w_data[loop_i] = (uint8_t)(result >> loop_i * 8);
					}
				}

				himax_mcu_register_write(ts, ts->debug.reg_cmd,
							 ts->debug.byte_length, w_data,
							 ts->debug.cfg_flag);
			} else {
				for (loop_i = 0; loop_i < count; loop_i++) {
					/* parsing addr after 'x' */
					memset(buff_tmp, 0x0, sizeof(buff_tmp));
					if (ts->debug.cfg_flag != 0 && loop_i != 0)
						ts->debug.byte_length = 2;
					else
						ts->debug.byte_length =
							x_pos[1] - x_pos[0] - 2; /* original */

					memcpy(buff_tmp, buf + x_pos[loop_i] + 1,
					       ts->debug.byte_length);

					/* I("%s: buff_tmp = %s\n",*/
					/*	__func__, buff_tmp);*/
					if (!kstrtoul(buff_tmp, 16, &result)) {
						if (loop_i == 0)
							ts->debug.reg_cmd[loop_i] =
								(uint8_t)(result);
						/* I("%s:
						 * reg_cmd
						 * = %X\n", __func__,
						 * reg_cmd[0]);
						 */
						else
							w_data[loop_i - 1] = (uint8_t)(result);
						/* I("%s: w_data[%d] =
						 * %2X\n", __func__,
						 * loop_i - 1,
						 * w_data[loop_i - 1]);
						 */
					}
				}

				ts->debug.byte_length = count - 1;
				himax_mcu_register_write(ts, ts->debug.reg_cmd,
							 ts->debug.byte_length, &w_data[0],
							 ts->debug.cfg_flag);
			}
			break;
		};
	}
	return len;
}

static void himax_burn_GMA_to_flash(struct himax_ts_data *ts, uint8_t *gma_buf, uint8_t type)
{
	uint16_t CFG_4k_SIZE = 0x1000;
	uint32_t start_addr;
	uint8_t tmp_addr[4];

	switch (type) {
	case 'V': /*Start to flash VCOM*/
		start_addr = 0x28000;
		I("Item : VCOM\n");
		break;
	case 'A': /*Start to flash Analog GMA*/
		start_addr = 0x29000;
		I("Item : AGMA\n");
		break;
	case 'D': /*Start to flash Digital GMA*/
		start_addr = 0x2A000;
		I("Item : DGMA\n");
		break;
	default:
		I("Input cmd is incorrect.\n");
		return;
	}

	if (start_addr < 0x100) {
		tmp_addr[3] = 0x00;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = (uint8_t)start_addr;
	} else if (start_addr >= 0x100 && start_addr < 0x10000) {
		tmp_addr[3] = 0x00;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = (uint8_t)(start_addr >> 8);
		tmp_addr[0] = (uint8_t)start_addr;
	} else if (start_addr >= 0x10000 && start_addr < 0x1000000) {
		tmp_addr[3] = 0x00;
		tmp_addr[2] = (uint8_t)(start_addr >> 16);
		tmp_addr[1] = (uint8_t)(start_addr >> 8);
		tmp_addr[0] = (uint8_t)start_addr;
	}

	ts->core_fp.fp_sector_erase(ts, start_addr, CFG_4k_SIZE);
	ts->core_fp.fp_flash_programming(ts, gma_buf, start_addr, CFG_4k_SIZE);

	if (ts->core_fp.fp_check_CRC(ts, tmp_addr, CFG_4k_SIZE) == 0)
		I("Burn GMA Success!\n");
	else
		E("Burn GMA FAIL!\n");
}

static uint8_t himax_convert_char2int(uint8_t src)
{
	uint8_t result;
	if ((src <= '9') && (src >= '0')) {
		result = src - '0';
	} else if ((src <= 'F') && (src >= 'A')) {
		result = src - 'A' + 0x0a;
	} else if ((src <= 'f') && (src >= 'a')) {
		result = src - 'a' + 0x0a;
	} else {
		result = 0xFF;
	}
	return result;
}

static uint8_t *himax_convert_GMA_data(struct himax_ts_data *ts, uint8_t *data, size_t data_size)
{
	uint8_t HEX_check[3];
	uint8_t GMA_content = 0;
	uint16_t CFG_4k_SIZE = 0x1000;
	uint32_t i = 0;
	uint32_t GMA_length = 0;
	uint32_t crc_value = 0;

	ts->debug.gma_buf = kcalloc(CFG_4k_SIZE, sizeof(uint8_t), GFP_KERNEL);
	memset(HEX_check, 0xFF, 3);
	memset(ts->debug.gma_buf, 0x00, CFG_4k_SIZE * sizeof(uint8_t));

	while (i < data_size) {
		if (data[i] == ',') {
			memset(HEX_check, 0xFF, 3);
			goto next_interation;
		} else if (data[i] == 'x') {
			if (HEX_check[1] == 0xFF) {
				HEX_check[1] = data[i];
				goto next_interation;
			}
		} else if (data[i] == '0') {
			if (HEX_check[0] == 0xFF) {
				HEX_check[0] = data[i];
				goto next_interation;
			}
		} else if (himax_convert_char2int(data[i]) == 0xFF) {
			goto next_interation;
		}

		if ((HEX_check[1] == 'x') && (HEX_check[0] == '0')) {
			if (HEX_check[2] == 0xFF) {
				HEX_check[2] = data[i];
				GMA_content = himax_convert_char2int(data[i]);
			} else {
				GMA_content = (GMA_content << 4) + himax_convert_char2int(data[i]);
				ts->debug.gma_buf[8 + GMA_length] = GMA_content;
				++GMA_length;
			}
		}
	next_interation:
		i++;
	}

	ts->debug.gma_buf[0] = (GMA_length) % 0x100;
	ts->debug.gma_buf[1] = (GMA_length >> 8) % 0x100;
	ts->debug.gma_buf[2] = (GMA_length >> 16) % 0x100;
	ts->debug.gma_buf[3] = (GMA_length >> 24) % 0x100;

	crc_value = ts->core_fp.fp_Calculate_CRC_with_AP(ts, ts->debug.gma_buf, 0, CFG_4k_SIZE - 4);

	ts->debug.gma_buf[CFG_4k_SIZE - 4] = (crc_value) % 0x100;
	ts->debug.gma_buf[CFG_4k_SIZE - 3] = (crc_value >> 8) % 0x100;
	ts->debug.gma_buf[CFG_4k_SIZE - 2] = (crc_value >> 16) % 0x100;
	ts->debug.gma_buf[CFG_4k_SIZE - 1] = (crc_value >> 24) % 0x100;

	return ts->debug.gma_buf;
}

static ssize_t himax_GMA_cmd_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	char *fileName = NULL;
	const struct firmware *fw = NULL;

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	I("himax %s\n", buf);

	if ((buf[0] == 'A' && buf[1] == 'G' && buf[2] == 'M' && buf[3] == 'A') ||
	    (buf[0] == 'V' && buf[1] == 'C' && buf[2] == 'O' && buf[3] == 'M') ||
	    (buf[0] == 'D' && buf[1] == 'G' && buf[2] == 'M' && buf[3] == 'A')) {
		switch (buf[0]) {
		case 'A':
			fileName = "AGMA.txt";
			break;
		case 'V':
			fileName = "VCOM.txt";
			break;
		case 'D':
			fileName = "DGMA.txt";
			break;
		};
	}
	request_firmware(&fw, fileName, ts->dev);

	if (fw)
		himax_convert_GMA_data(ts, (uint8_t *)fw->data, fw->size);
	himax_burn_GMA_to_flash(ts, ts->debug.gma_buf, buf[0]);

	kfree(ts->debug.gma_buf);
	if (fw)
		kfree(fw);
	return len;
}

static int32_t *getMutualBuffer(struct himax_ts_data *ts)
{
	return ts->debug.diag_mutual;
}
static int32_t *getMutualNewBuffer(struct himax_ts_data *ts)
{
	return ts->debug.diag_mutual_new;
}
static int32_t *getMutualOldBuffer(struct himax_ts_data *ts)
{
	return ts->debug.diag_mutual_old;
}
static int32_t *getSelfBuffer(struct himax_ts_data *ts)
{
	return ts->debug.diag_self;
}
static int32_t *getSelfNewBuffer(struct himax_ts_data *ts)
{
	return ts->debug.diag_self_new;
}
static int32_t *getSelfOldBuffer(struct himax_ts_data *ts)
{
	return ts->debug.diag_self_old;
}
static void setMutualBuffer(struct himax_ts_data *ts, uint8_t x_num, uint8_t y_num)
{
	ts->debug.diag_mutual = kzalloc(x_num * y_num * sizeof(int32_t), GFP_KERNEL);
}
static void setMutualNewBuffer(struct himax_ts_data *ts, uint8_t x_num, uint8_t y_num)
{
	ts->debug.diag_mutual_new = kzalloc(x_num * y_num * sizeof(int32_t), GFP_KERNEL);
}
static void setMutualOldBuffer(struct himax_ts_data *ts, uint8_t x_num, uint8_t y_num)
{
	ts->debug.diag_mutual_old = kzalloc(x_num * y_num * sizeof(int32_t), GFP_KERNEL);
}
static void setSelfBuffer(struct himax_ts_data *ts, uint8_t x_num, uint8_t y_num)
{
	ts->debug.diag_self = kzalloc((x_num + y_num) * sizeof(int32_t), GFP_KERNEL);
}
static void setSelfNewBuffer(struct himax_ts_data *ts, uint8_t x_num, uint8_t y_num)
{
	ts->debug.diag_self_new = kzalloc((x_num + y_num) * sizeof(int32_t), GFP_KERNEL);
}
static void setSelfOldBuffer(struct himax_ts_data *ts, uint8_t x_num, uint8_t y_num)
{
	ts->debug.diag_self_old = kzalloc((x_num + y_num) * sizeof(int32_t), GFP_KERNEL);
}

#if defined(HX_TP_PROC_2T2R)
static int32_t *getMutualBuffer_2(struct himax_ts_data *ts)
{
	return ts->debug.diag_mutual_2;
}
static void setMutualBuffer_2(struct himax_ts_data *ts, uint8_t x_num_2, uint8_t y_num_2)
{
	ts->debug.diag_mutual_2 = kzalloc(x_num_2 * y_num_2 * sizeof(int32_t), GFP_KERNEL);
}
#endif

static int himax_set_diag_cmd(struct himax_ts_data *ts, struct himax_ic_data *ic_data,
			      struct himax_report_data *hx_touch_data)
{
	int32_t *mutual_data;
	int32_t *self_data;
	int mul_num;
	int self_num;
	/* int RawDataLen = 0; */
	hx_touch_data->diag_cmd = ts->diag_cmd;

	if (hx_touch_data->diag_cmd >= 1 && hx_touch_data->diag_cmd <= 7) {
		/* Check event stack CRC */
		if (!ts->core_fp.fp_diag_check_sum(ts, hx_touch_data))
			goto bypass_checksum_failed_packet;

#if defined(HX_TP_PROC_2T2R)
		if (ts->debug.is_2t2r &&
		    (hx_touch_data->diag_cmd >= 4 && hx_touch_data->diag_cmd <= 6)) {
			mutual_data = getMutualBuffer_2(ts);
			self_data = getSelfBuffer(ts);
			/* initiallize the block number of mutual and self */
			mul_num = ic_data->HX_RX_NUM_2 * ic_data->HX_TX_NUM_2;
			self_num = ic_data->HX_RX_NUM_2 + ic_data->HX_TX_NUM_2;
		} else
#endif
		{
			mutual_data = getMutualBuffer(ts);
			self_data = getSelfBuffer(ts);
			/* initiallize the block number of mutual and self */
			mul_num = ic_data->HX_RX_NUM * ic_data->HX_TX_NUM;
			self_num = ic_data->HX_RX_NUM + ic_data->HX_TX_NUM;
		}
		ts->core_fp.fp_diag_parse_raw_data(ts, hx_touch_data, mul_num, self_num,
						   hx_touch_data->diag_cmd, mutual_data, self_data);
	} else if (hx_touch_data->diag_cmd == 8) {
		memset(ts->debug.diag_coor, 0x00, sizeof(ts->debug.diag_coor));
		memcpy(&(ts->debug.diag_coor[0]), &hx_touch_data->hx_coord_buf[0],
		       hx_touch_data->touch_info_size);
	}

	/* assign state info data */
	memcpy(&(ts->debug.hx_state_info[0]), &hx_touch_data->hx_state_info[0], 2);
	return NO_ERR;
bypass_checksum_failed_packet:
	return 1;
}

/* #if defined(HX_DEBUG_LEVEL) */
static void himax_log_touch_data(struct himax_ts_data *ts, int start)
{
	int loop_i = 0;
	int print_size = 0;
	uint8_t *buf = NULL;

	if (start == 1)
		return; /* report data when end of ts_work*/

	if (ts->hx_touch_data->diag_cmd > 0) {
		print_size = ts->hx_touch_data->touch_all_size;
		buf = kcalloc(print_size, sizeof(uint8_t), GFP_KERNEL);
		if (buf == NULL) {
			E("%s, Failed to allocate memory\n", __func__);
			return;
		}

		memcpy(buf, ts->hx_touch_data->hx_coord_buf, ts->hx_touch_data->touch_info_size);
		memcpy(&buf[ts->hx_touch_data->touch_info_size], ts->hx_touch_data->hx_rawdata_buf,
		       print_size - ts->hx_touch_data->touch_info_size);
	}

	else if (ts->hx_touch_data->diag_cmd == 0) {
		print_size = ts->hx_touch_data->touch_info_size;
		buf = kcalloc(print_size, sizeof(uint8_t), GFP_KERNEL);
		if (buf == NULL) {
			E("%s, Failed to allocate memory\n", __func__);
			return;
		}

		memcpy(buf, ts->hx_touch_data->hx_coord_buf, print_size);
	} else {
		E("%s:cmd fault\n", __func__);
		return;
	}

	for (loop_i = 0; loop_i < print_size; loop_i += 8) {
		if ((loop_i + 7) >= print_size) {
			I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i, buf[loop_i], loop_i + 1,
			  buf[loop_i + 1]);
			I("P %2d = 0x%2.2X P %2d = 0x%2.2X\n", loop_i + 2, buf[loop_i + 2],
			  loop_i + 3, buf[loop_i + 3]);
			break;
		}

		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i, buf[loop_i], loop_i + 1,
		  buf[loop_i + 1]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 2, buf[loop_i + 2], loop_i + 3,
		  buf[loop_i + 3]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 4, buf[loop_i + 4], loop_i + 5,
		  buf[loop_i + 5]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 6, buf[loop_i + 6], loop_i + 7,
		  buf[loop_i + 7]);
		I("\n");
	}
	kfree(buf);
}

#define PRT_LOG "Finger %d=> X:%d, Y:%d W:%d, Z:%d, F:%d, Int_Delay_Cnt:%d\n"
static void himax_log_touch_event(struct himax_ts_data *ts, int start)
{
	int loop_i = 0;

	if (start == 1)
		return; /*report data when end of ts_work*/

	if (ts->target_report_data->finger_on > 0 && ts->target_report_data->finger_num > 0) {
		for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
			if (ts->target_report_data->x[loop_i] >= 0 &&
			    ts->target_report_data->x[loop_i] <= ts->pdata->abs_x_max &&
			    ts->target_report_data->y[loop_i] >= 0 &&
			    ts->target_report_data->y[loop_i] <= ts->pdata->abs_y_max) {
				I(PRT_LOG, loop_i + 1, ts->target_report_data->x[loop_i],
				  ts->target_report_data->y[loop_i],
				  ts->target_report_data->w[loop_i],
				  ts->target_report_data->w[loop_i], loop_i + 1,
				  ts->target_report_data->ig_count);
			}
		}
	} else if (ts->target_report_data->finger_on == 0 &&
		   ts->target_report_data->finger_num == 0) {
		I("All Finger leave\n");
	} else {
		I("%s : wrong input!\n", __func__);
	}
}
static void himax_log_touch_int_devation(struct himax_ts_data *ts, int touched)
{
	if (touched == HX_FINGER_ON) {
		ktime_get_real_ts64(&ts->debug.time_start);
		/* I(" Irq start time = %ld.%06ld s\n",
		 * time_start.tv_sec, time_start.tv_nsec/1000);
		 */
	} else if (touched == HX_FINGER_LEAVE) {
		ktime_get_real_ts64(&ts->debug.time_end);
		ts->debug.time_delta.tv_nsec =
			(ts->debug.time_end.tv_sec * 1000000000 + ts->debug.time_end.tv_nsec) -
			(ts->debug.time_start.tv_sec * 1000000000 + ts->debug.time_start.tv_nsec);
		/*  I("Irq finish time = %ld.%06ld s\n",
		 *	time_end.tv_sec, time_end.tv_nsec/1000);
		 */
		I("Touch latency = %ld us\n", ts->debug.time_delta.tv_nsec / 1000);
		I("bus_speed = %d kHz\n", ts->bus_speed);
		if (ts->target_report_data->finger_on == 0 &&
		    ts->target_report_data->finger_num == 0)
			I("All Finger leave\n");
	} else {
		I("%s : wrong input!\n", __func__);
	}
}

#define RAW_DOWN_STATUS "status: Raw:F:%02d Down, X:%d, Y:%d, W:%d\n"
#define RAW_UP_STATUS "status: Raw:F:%02d Up, X:%d, Y:%d\n"

static void himax_log_touch_event_detail(struct himax_ts_data *ts, int start)
{
	int loop_i = 0;

	if (start == HX_FINGER_LEAVE) {
		for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
			if (((ts->old_finger >> loop_i & 1) == 0) &&
			    ((ts->pre_finger_mask >> loop_i & 1) == 1)) {
				if (ts->target_report_data->x[loop_i] >= 0 &&
				    ts->target_report_data->x[loop_i] <= ts->pdata->abs_x_max &&
				    ts->target_report_data->y[loop_i] >= 0 &&
				    ts->target_report_data->y[loop_i] <= ts->pdata->abs_y_max) {
					I(RAW_DOWN_STATUS, loop_i + 1,
					  ts->target_report_data->x[loop_i],
					  ts->target_report_data->y[loop_i],
					  ts->target_report_data->w[loop_i]);
				}
			} else if ((((ts->old_finger >> loop_i & 1) == 1) &&
				    ((ts->pre_finger_mask >> loop_i & 1) == 0))) {
				I(RAW_UP_STATUS, loop_i + 1, ts->pre_finger_data[loop_i][0],
				  ts->pre_finger_data[loop_i][1]);
			} else {
				/* I("dbg hx_point_num=%d, old_finger=0x%02X,"
				 * " pre_finger_mask=0x%02X\n",
				 * ts->hx_point_num, ts->old_finger,
				 * ts->pre_finger_mask);
				 */
			}
		}
	}
}

static void himax_ts_dbg_func(struct himax_ts_data *ts, int start)
{
	if (ts->debug_log_level & BIT(0)) {
		/* I("debug level 1\n"); */
		himax_log_touch_data(ts, start);
	}
	if (ts->debug_log_level & BIT(1)) {
		/* I("debug level 2\n"); */
		himax_log_touch_event(ts, start);
	}
	if (ts->debug_log_level & BIT(2)) {
		/* I("debug level 4\n"); */
		himax_log_touch_int_devation(ts, start);
	}
	if (ts->debug_log_level & BIT(3)) {
		/* I("debug level 8\n"); */
		himax_log_touch_event_detail(ts, start);
	}
}

static int himax_change_mode(struct himax_ts_data *ts, uint8_t str_pw, uint8_t end_pw)
{
	uint8_t data[4] = { 0 };
	int count = 0;

	/*sense off*/
	ts->core_fp.fp_sense_off(ts, true);
	/*mode change*/
	data[1] = str_pw;
	data[0] = str_pw;
	if (ts->core_fp.fp_assign_sorting_mode != NULL)
		ts->core_fp.fp_assign_sorting_mode(ts, data);

	/*sense on*/
	ts->core_fp.fp_sense_on(ts, 0x01);
	/*wait mode change*/
	do {
		if (ts->core_fp.fp_check_sorting_mode != NULL)
			ts->core_fp.fp_check_sorting_mode(ts, data);
		if ((data[0] == end_pw) && (data[1] == end_pw))
			return 0;

		I("Now retry %d times!\n", count);
		count++;
		msleep(50);
	} while (count < 50);

	return ERR_WORK_OUT;
}

#define PRT_OK_LOG "%s: change mode 0x%4X. str_pw = %2X, end_pw = %2X\n"
#define PRT_FAIL_LOG "%s: change mode failed. str_pw = %2X, end_pw = %2X\n"
static ssize_t himax_diag_cmd_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	char *dbg_map_str = "mode:";
	char *str_ptr = NULL;
	int str_len = 0;
	int rst = 0;
	uint8_t str_pw = 0;
	uint8_t end_pw = 0;

	switch (len) {
	case 1: /*raw out select - diag,X*/
		if (!kstrtoint(buf, 16, &rst)) {
			ts->diag_cmd = rst;
			I("%s: dsram_flag = %d\n", __func__, ts->debug.dsram_flag);
			if (ts->debug.dsram_flag) {
				/*Cancal work queue and return to stack*/
				ts->debug.process_type = 0;
				ts->debug.dsram_flag = false;
				cancel_delayed_work(&ts->himax_diag_delay_wrok);
				himax_int_enable(ts, 1);
				ts->core_fp.fp_return_event_stack(ts);
			}
			ts->core_fp.fp_diag_register_set(ts, ts->diag_cmd, 0, false);
			I("%s: Set raw out select 0x%X.\n", __func__, ts->diag_cmd);
		}
		if (!ts->diag_cmd) {
			if (ts->debug.mode_flag) /*back to normal mode*/
				himax_change_mode(ts, 0x00, 0x99);
		}
		break;
	case 2: /*data processing + rawout select - diag,XY*/
		if (!kstrtoint(buf, 16, &rst)) {
			ts->debug.process_type = (rst >> 4) & 0xF;
			ts->diag_cmd = rst & 0xF;
		}
		if (ts->diag_cmd == 0)
			break;
		else if (ts->debug.process_type > 0 && ts->debug.process_type <= 3) {
			if (!ts->debug.dsram_flag) {
				/*Start wrok queue*/
				himax_int_enable(ts, 0);
				ts->core_fp.fp_diag_register_set(ts, ts->diag_cmd,
								 ts->debug.process_type, false);

				queue_delayed_work(ts->himax_diag_wq, &ts->himax_diag_delay_wrok,
						   2 * HZ / 100);
				ts->debug.dsram_flag = true;

				I("%s: Start get raw data in DSRAM\n", __func__);
			} else {
				ts->core_fp.fp_diag_register_set(ts, ts->diag_cmd,
								 ts->debug.process_type, false);
			}
		}
		break;
	case 4: /*data processing + rawout select - diag,XXYY*/
		/*ex:XXYY=010A=dsram rawdata*/
		I("%s, now case 4\n", __func__);
		if (!kstrtoint(buf, 16, &rst)) {
			ts->debug.process_type = (rst >> 8) & 0xFF;
			ts->diag_cmd = rst & 0xFF;
			I("%s:process_type=0x%02X, diag_cmd=0x%02X\n", __func__,
			  ts->debug.process_type, ts->diag_cmd);
		}
		if (ts->debug.process_type <= 0 || ts->diag_cmd <= 0)
			break;
		else if (ts->debug.process_type > 0 && ts->debug.process_type <= 3) {
			if (!ts->debug.dsram_flag) {
				/*Start wrok queue*/
				himax_int_enable(ts, 0);
				ts->core_fp.fp_diag_register_set(ts, ts->diag_cmd,
								 ts->debug.process_type, true);

				queue_delayed_work(ts->himax_diag_wq, &ts->himax_diag_delay_wrok,
						   2 * HZ / 100);
				ts->debug.dsram_flag = true;

				I("%s: Start get raw data in DSRAM\n", __func__);
			} else {
				ts->core_fp.fp_diag_register_set(ts, ts->diag_cmd,
								 ts->debug.process_type, true);
			}
		}
		break;
	case 9: /*change mode - mode:XXYY(start PW,end PW)*/
		str_ptr = strnstr(buf, dbg_map_str, len);
		if (str_ptr) {
			str_len = strlen(dbg_map_str);
			if (!kstrtoint(buf + str_len, 16, &rst)) {
				str_pw = (rst >> 8) & 0xFF;
				end_pw = rst & 0xFF;
				if (!himax_change_mode(ts, str_pw, end_pw)) {
					ts->debug.mode_flag = 1;
					I(PRT_OK_LOG, __func__, rst, str_pw, end_pw);
				} else
					I(PRT_FAIL_LOG, __func__, str_pw, end_pw);
			}
		} else {
			I("%s: Can't find string [%s].\n", __func__, dbg_map_str);
		}
		break;
	default:
		I("%s: Length is not correct.\n", __func__);
	}
	return len;
}

static ssize_t himax_diag_arrange_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	ts->debug.diag_arr_num = buf[0] - '0';
	I("%s: diag_arr_num = %d\n", __func__, ts->debug.diag_arr_num);
	return len;
}

static void himax_get_mutual_edge(struct himax_ts_data *ts)
{
	int i = 0;

	for (i = 0; i < (ts->ic_data->HX_RX_NUM * ts->ic_data->HX_TX_NUM); i++) {
		if (ts->debug.diag_mutual[i] > ts->debug.max_mutual)
			ts->debug.max_mutual = ts->debug.diag_mutual[i];

		if (ts->debug.diag_mutual[i] < ts->debug.min_mutual)
			ts->debug.min_mutual = ts->debug.diag_mutual[i];
	}
}

static void himax_get_self_edge(struct himax_ts_data *ts)
{
	int i = 0;

	for (i = 0; i < (ts->ic_data->HX_RX_NUM + ts->ic_data->HX_TX_NUM); i++) {
		if (ts->debug.diag_self[i] > ts->debug.max_self)
			ts->debug.max_self = ts->debug.diag_self[i];

		if (ts->debug.diag_self[i] < ts->debug.min_self)
			ts->debug.min_self = ts->debug.diag_self[i];
	}
}

static void print_state_info(struct seq_file *s)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));

	/* seq_printf(s, "State_info_2bytes:%3d, %3d\n",
	 * _state_info[0],hx_state_info[1]);
	 */
	seq_printf(s, "ReCal = %d\t", ts->debug.hx_state_info[0] & 0x01);
	seq_printf(s, "Palm = %d\t", ts->debug.hx_state_info[0] >> 1 & 0x01);
	seq_printf(s, "AC mode = %d\t", ts->debug.hx_state_info[0] >> 2 & 0x01);
	seq_printf(s, "Water = %d\n", ts->debug.hx_state_info[0] >> 3 & 0x01);
	seq_printf(s, "Glove = %d\t", ts->debug.hx_state_info[0] >> 4 & 0x01);
	seq_printf(s, "TX Hop = %d\t", ts->debug.hx_state_info[0] >> 5 & 0x01);
	seq_printf(s, "Base Line = %d\t", ts->debug.hx_state_info[0] >> 6 & 0x01);
	seq_printf(s, "OSR Hop = %d\t", ts->debug.hx_state_info[1] >> 3 & 0x01);
	seq_printf(s, "KEY = %d\n", ts->debug.hx_state_info[1] >> 4 & 0x0F);
}

static void himax_diag_arrange_print(struct seq_file *s, int i, int j, int transpose)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));

	if (transpose)
		seq_printf(s, "%6d", ts->debug.diag_mutual[j + i * ts->ic_data->HX_RX_NUM]);
	else
		seq_printf(s, "%6d", ts->debug.diag_mutual[i + j * ts->ic_data->HX_RX_NUM]);
}

/* ready to print second step which is column*/
static void himax_diag_arrange_inloop(struct seq_file *s, int in_init, int out_init, bool transpose,
				      int j)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	int x_channel = ts->ic_data->HX_RX_NUM;
	int y_channel = ts->ic_data->HX_TX_NUM;
	int i;
	int in_max = 0;

	if (transpose)
		in_max = y_channel;
	else
		in_max = x_channel;

	if (in_init > 0) { /* bit0 = 1 */
		for (i = in_init - 1; i >= 0; i--)
			himax_diag_arrange_print(s, i, j, transpose);

		if (transpose) {
			if (out_init > 0)
				seq_printf(s, " %5d\n", ts->debug.diag_self[j]);
			else
				seq_printf(s, " %5d\n", ts->debug.diag_self[x_channel - j - 1]);
		}
	} else { /* bit0 = 0 */
		for (i = 0; i < in_max; i++)
			himax_diag_arrange_print(s, i, j, transpose);

		if (transpose) {
			if (out_init > 0)
				seq_printf(s, " %5d\n", ts->debug.diag_self[x_channel - j - 1]);
			else
				seq_printf(s, " %5d\n", ts->debug.diag_self[j]);
		}
	}
}

/* print first step which is row */
static void himax_diag_arrange_outloop(struct seq_file *s, int transpose, int out_init, int in_init)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	int j;
	int x_channel = ts->ic_data->HX_RX_NUM;
	int y_channel = ts->ic_data->HX_TX_NUM;
	int out_max = 0;
	int self_cnt = 0;

	if (transpose)
		out_max = x_channel;
	else
		out_max = y_channel;

	if (out_init > 0) { /* bit1 = 1 */
		self_cnt = 1;

		for (j = out_init - 1; j >= 0; j--) {
			seq_printf(s, "%3c%02d%c", '[', j + 1, ']');
			himax_diag_arrange_inloop(s, in_init, out_init, transpose, j);

			if (!transpose) {
				seq_printf(s, " %5d\n",
					   ts->debug.diag_self[y_channel + x_channel - self_cnt]);
				self_cnt++;
			}
		}
	} else { /* bit1 = 0 */
		/* self_cnt = x_channel; */
		for (j = 0; j < out_max; j++) {
			seq_printf(s, "%3c%02d%c", '[', j + 1, ']');
			himax_diag_arrange_inloop(s, in_init, out_init, transpose, j);

			if (!transpose) {
				seq_printf(s, " %5d\n", ts->debug.diag_self[j + x_channel]);
			}
		}
	}
}

/* determin the output format of diag */
static void himax_diag_arrange(struct seq_file *s)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	int x_channel = ts->ic_data->HX_RX_NUM;
	int y_channel = ts->ic_data->HX_TX_NUM;
	int bit2, bit1, bit0;
	int i;
	/* rotate bit */
	bit2 = ts->debug.diag_arr_num >> 2;
	/* reverse Y */
	bit1 = ts->debug.diag_arr_num >> 1 & 0x1;
	/* reverse X */
	bit0 = ts->debug.diag_arr_num & 0x1;

	if (ts->debug.diag_arr_num < 4) {
		for (i = 0; i <= x_channel; i++)
			seq_printf(s, "%3c%02d%c", '[', i, ']');

		seq_puts(s, "\n");
		himax_diag_arrange_outloop(s, bit2, bit1 * y_channel, bit0 * x_channel);
		seq_printf(s, "%6c", ' ');

		if (bit0 == 1) {
			for (i = x_channel - 1; i >= 0; i--)
				seq_printf(s, "%6d", ts->debug.diag_self[i]);
		} else {
			for (i = 0; i < x_channel; i++)
				seq_printf(s, "%6d", ts->debug.diag_self[i]);
		}
	} else {
		for (i = 0; i <= y_channel; i++)
			seq_printf(s, "%3c%02d%c", '[', i, ']');

		seq_puts(s, "\n");
		himax_diag_arrange_outloop(s, bit2, bit1 * x_channel, bit0 * y_channel);
		seq_printf(s, "%6c", ' ');

		if (bit1 == 1) {
			for (i = x_channel + y_channel - 1; i >= x_channel; i--)
				seq_printf(s, "%6d", ts->debug.diag_self[i]);
		} else {
			for (i = x_channel; i < x_channel + y_channel; i++)
				seq_printf(s, "%6d", ts->debug.diag_self[i]);
		}
	}
}

static void *himax_diag_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;

	return (void *)((unsigned long)*pos + 1);
}

static void *himax_diag_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_diag_seq_stop(struct seq_file *s, void *v)
{
	kfree(v);
}

/* DSRAM thread */
static bool himax_ts_diag_func(struct himax_ts_data *ts)
{
	int retry = 3;
	int i = 0, j = 0;
	unsigned int index = 0;
	int x_channel = ts->ic_data->HX_RX_NUM;
	int y_channel = ts->ic_data->HX_TX_NUM;
	int total_size = (y_channel * x_channel + y_channel + x_channel) * 2;
	uint8_t *info_data = NULL;
	int32_t *mutual_data = NULL;
	int32_t *mutual_data_new = NULL;
	int32_t *mutual_data_old = NULL;
	int32_t *self_data = NULL;
	int32_t *self_data_new = NULL;
	int32_t *self_data_old = NULL;
	int32_t new_data;
	/* 1:common dsram,2:100 frame Max,3:N-(N-1)frame */
	int dsram_type = ts->debug.process_type;

	info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (info_data == NULL) {
		E("%s: Failed to allocate memory\n", __func__);
		return false;
	}

	memset(info_data, 0, total_size * sizeof(uint8_t));

	I("%s: process type=%d!\n", __func__, ts->debug.process_type);

	ts->core_fp.fp_burst_enable(ts, 1);

	if (dsram_type <= 2) {
		mutual_data = getMutualBuffer(ts);
		self_data = getSelfBuffer(ts);
	} else if (dsram_type == 3) {
		mutual_data = getMutualBuffer(ts);
		mutual_data_new = getMutualNewBuffer(ts);
		mutual_data_old = getMutualOldBuffer(ts);
		self_data = getSelfBuffer(ts);
		self_data_new = getSelfNewBuffer(ts);
		self_data_old = getSelfOldBuffer(ts);
	}

	do {
		if (ts->core_fp.fp_get_DSRAM_data(ts, info_data, ts->debug.dsram_flag))
			break;
	} while (retry-- > 0);

	if (retry <= 0) {
		E("%s: Get DSRAM data failed\n", __func__);
		kfree(info_data);
		return false;
	}

	index = 0;

	for (i = 0; i < y_channel; i++) { /*mutual data*/
		for (j = 0; j < x_channel; j++) {
			new_data = (((int8_t)info_data[index + 1] << 8) | info_data[index]);

			if (dsram_type <= 1) {
				mutual_data[i * x_channel + j] = new_data;
			} else if (dsram_type == 2) { /* Keep max data */
				if (mutual_data[i * x_channel + j] < new_data)
					mutual_data[i * x_channel + j] = new_data;
			} else if (dsram_type == 3) {
				/* Cal data for [N]-[N-1] frame */
				mutual_data_new[i * x_channel + j] = new_data;
				mutual_data[i * x_channel + j] =
					mutual_data_new[i * x_channel + j] -
					mutual_data_old[i * x_channel + j];
			}
			index += 2;
		}
	}

	for (i = 0; i < x_channel + y_channel; i++) { /*self data*/
		new_data = (((int8_t)info_data[index + 1] << 8) | info_data[index]);
		if (dsram_type <= 1) {
			self_data[i] = new_data;
		} else if (dsram_type == 2) { /* Keep max data */
			if (self_data[i] < new_data)
				self_data[i] = new_data;
		} else if (dsram_type == 3) { /* Cal data for [N]-[N-1] frame */
			self_data_new[i] = new_data;
			self_data[i] = self_data_new[i] - self_data_old[i];
		}
		index += 2;
	}

	kfree(info_data);

	if (dsram_type == 3) {
		memcpy(mutual_data_old, mutual_data_new, x_channel * y_channel * sizeof(int32_t));
		/* copy N data to N-1 array */
		memcpy(self_data_old, self_data_new, (x_channel + y_channel) * sizeof(int32_t));
		/* copy N data to N-1 array */
	}

	ts->debug.diag_max_cnt++;

	if (dsram_type >= 1 && dsram_type <= 3) {
		queue_delayed_work(ts->himax_diag_wq, &ts->himax_diag_delay_wrok, 1 / 10 * HZ);
	}
	return true;
}

static int himax_diag_print(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	int x_num = ts->ic_data->HX_RX_NUM;
	int y_num = ts->ic_data->HX_TX_NUM;
	size_t ret = 0;

	/* don't add KEY_COUNT */
	seq_printf(s, "ChannelStart: %4d, %4d\n\n", x_num, y_num);

	/* start to show out the raw data in adb shell */
	himax_diag_arrange(s);
	seq_puts(s, "\n");
	seq_puts(s, "ChannelEnd");
	seq_puts(s, "\n");

	/* print Mutual/Slef Maximum and Minimum */
	himax_get_mutual_edge(ts);
	himax_get_self_edge(ts);
	seq_printf(s, "Mutual Max:%3d, Min:%3d\n", ts->debug.max_mutual, ts->debug.min_mutual);
	seq_printf(s, "Self Max:%3d, Min:%3d\n", ts->debug.max_self, ts->debug.min_self);
	/* recovery status after print*/
	ts->debug.max_mutual = 0;
	ts->debug.min_mutual = 0xFFFF;
	ts->debug.max_self = 0;
	ts->debug.min_self = 0xFFFF;

	/*pring state info*/
	print_state_info(s);

	if (s->count >= s->size)
		ts->debug.h_overflow++;

	return ret;
}

static int himax_diag_stack_read(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));

	if (ts->diag_cmd)
		himax_diag_print(s, v);
	else
		seq_puts(s, "Please set raw out select 'echo diag,X > debug'\n\n");

	return 0;
}

static const struct seq_operations himax_diag_stack_ops = {
	.start = himax_diag_seq_start,
	.next = himax_diag_seq_next,
	.stop = himax_diag_seq_stop,
	.show = himax_diag_stack_read,
};

static int himax_diag_stack_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_diag_stack_ops);
};

static const struct proc_ops himax_proc_stack_ops = {
	//.owner = THIS_MODULE,
	.proc_open = himax_diag_stack_open,
	.proc_read = seq_read,
	.proc_release = seq_release,
};

static int himax_sram_read(struct seq_file *s, void *v, uint8_t rs)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	int d_type = 0;

	d_type = (!ts->diag_cmd) ? rs : ts->diag_cmd;

	if (!ts->debug.h_overflow) {
		if (!ts->debug.process_type) {
			himax_int_enable(ts, 0);
			ts->core_fp.fp_diag_register_set(ts, d_type, 0, false);

			if (!himax_ts_diag_func(ts))
				seq_puts(s, "Get sram data failed.");
			else
				himax_diag_print(s, v);

			ts->diag_cmd = 0;
			ts->core_fp.fp_diag_register_set(ts, 0, 0, false);
			himax_int_enable(ts, 1);
		}
	}

	if ((ts->debug.process_type <= 3 && ts->diag_cmd && ts->debug.dsram_flag) ||
	    ts->debug.h_overflow) {
		himax_diag_print(s, v);
		ts->debug.h_overflow = 0;
	}

	return 0;
}

static int himax_diag_delta_read(struct seq_file *s, void *v)
{
	return himax_sram_read(s, v, 0x09);
}

static const struct seq_operations himax_diag_delta_ops = {
	.start = himax_diag_seq_start,
	.next = himax_diag_seq_next,
	.stop = himax_diag_seq_stop,
	.show = himax_diag_delta_read,
};

static int himax_diag_delta_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_diag_delta_ops);
};

static const struct proc_ops himax_proc_delta_ops = {
	//.owner = THIS_MODULE,
	.proc_open = himax_diag_delta_open,
	.proc_read = seq_read,
	.proc_release = seq_release,
};

static int himax_diag_dc_read(struct seq_file *s, void *v)
{
	return himax_sram_read(s, v, 0x0A);
}

static const struct seq_operations himax_diag_dc_ops = {
	.start = himax_diag_seq_start,
	.next = himax_diag_seq_next,
	.stop = himax_diag_seq_stop,
	.show = himax_diag_dc_read,
};
static int himax_diag_dc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_diag_dc_ops);
};

static const struct proc_ops himax_proc_dc_ops = {
	//.owner = THIS_MODULE,
	.proc_open = himax_diag_dc_open,
	.proc_read = seq_read,
	.proc_release = seq_release,
};

static int himax_diag_baseline_read(struct seq_file *s, void *v)
{
	return himax_sram_read(s, v, 0x0B);
}

static const struct seq_operations himax_diag_baseline_ops = {
	.start = himax_diag_seq_start,
	.next = himax_diag_seq_next,
	.stop = himax_diag_seq_stop,
	.show = himax_diag_baseline_read,
};
static int himax_diag_baseline_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_diag_baseline_ops);
};

static const struct proc_ops himax_proc_baseline_ops = {
	//.owner = THIS_MODULE,
	.proc_open = himax_diag_baseline_open,
	.proc_read = seq_read,
	.proc_release = seq_release,
};

static ssize_t himax_reset_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	if (len >= 12) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

#if defined(HX_RST_PIN_FUNC)
	if (buf[0] == '1')
		ts->core_fp.fp_ic_reset(ts, false, false);
	else if (buf[0] == '2')
		ts->core_fp.fp_ic_reset(ts, false, true);
	else if (buf[0] == '3')
		ts->core_fp.fp_ic_reset(ts, true, false);
	else if (buf[0] == '4')
		ts->core_fp.fp_ic_reset(ts, true, true);
		/* else if (buf[0] == '5') */
		/*	ESD_HW_REST(); */
#endif
	return len;
}

static ssize_t himax_proc_FW_debug_read(struct himax_ts_data *ts, char *buf, size_t len)
{
	ssize_t ret = 0;
	uint8_t i = 0;
	uint8_t addr[4] = { 0 };
	uint8_t data[4] = { 0 };

	len = (size_t)(sizeof(himax_dbg_reg_ary) / sizeof(uint32_t));

	for (i = 0; i < len; i++) {
		himax_parse_assign_cmd(himax_dbg_reg_ary[i], addr, 4);
		himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);

		ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
				"reg[0-3] : 0x%08X = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				himax_dbg_reg_ary[i], data[0], data[1], data[2], data[3]);
		I("reg[0-3] : 0x%08X = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", himax_dbg_reg_ary[i], data[0],
		  data[1], data[2], data[3]);
	}

	return ret;
}

static void setFlashBuffer(struct himax_ts_data *ts)
{
	ts->debug.flash_buffer = kcalloc(ts->debug.flash_size, sizeof(uint8_t), GFP_KERNEL);
}

static void flash_dump_prog_set(struct himax_ts_data *ts, uint8_t prog)
{
	ts->debug.flash_progress = prog;
	if (prog == ONGOING)
		ts->debug.flash_dump_going = ONGOING;
	else
		ts->debug.flash_dump_going = START;
}

static int himax_proc_flash_read(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = pde_data(file_inode(s->file));
	ssize_t ret = 0;
	int i;
	uint8_t flash_progress = ts->debug.flash_progress;
	uint8_t flash_cmd = ts->debug.flash_cmd;
	bool flash_rst = ts->debug.flash_dump_rst;

	I("flash_progress = %d\n", flash_progress);

	if (!flash_rst) {
		seq_puts(s, "FlashStart:Fail\n");
		seq_puts(s, "FlashEnd\n");
		return ret;
	}

	if (flash_progress == START)
		seq_puts(s, "Flash dump - Start\n");
	else if (flash_progress == ONGOING)
		seq_puts(s, "Flash dump - On-going\n");
	else if (flash_progress == FINISHED)
		seq_puts(s, "Flash dump - Finished\n");

	/*print flash dump data*/
	if (flash_cmd == 1 && flash_progress == FINISHED) {
		seq_puts(s, "Start to print flash dump data\n");
		for (i = 0; i < ts->debug.flash_size; i++) {
			seq_printf(s, "0x%02X,", ts->debug.flash_buffer[i]);
			if (i % 16 == 15)
				seq_puts(s, "\n");
		}
	}

	seq_puts(s, "FlashEnd\n");

	return ret;
}

static ssize_t himax_proc_flash_write(struct file *filp, const char __user *buff, size_t len,
				      loff_t *data)
{
	struct himax_ts_data *ts = pde_data(file_inode(filp));
	char buf[80] = { 0 };

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	I("%s: buf = %s\n", __func__, buf);

	if (ts->debug.flash_progress == ONGOING) {
		E("%s: process is busy , return!\n", __func__);
		return len;
	}

	if ((buf[1] == '_') && (buf[2] == '6')) {
		if (buf[3] == '4')
			ts->debug.flash_size = FW_SIZE_64k;

	} else if ((buf[1] == '_') && (buf[2] == '2')) {
		if (buf[3] == '8')
			ts->debug.flash_size = FW_SIZE_128k;
	}

	/*1 : print flash to window, 2 : dump to sdcard*/
	if (buf[0] == '1') {
		/* 1_64,1_28 for flash size:
		 * 64k,128k
		 */
		ts->debug.flash_cmd = 1;
		flash_dump_prog_set(ts, START);
		ts->debug.flash_dump_rst = true;
		queue_work(ts->flash_wq, &ts->flash_work);
	} else if (buf[0] == '2') {
		/* 2_64,2_28 for flash size:
		 * 64k,128k
		 */
		ts->debug.flash_cmd = 2;
		flash_dump_prog_set(ts, START);
		ts->debug.flash_dump_rst = true;
		queue_work(ts->flash_wq, &ts->flash_work);
	}

	return len;
}

static void *himax_flash_dump_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;

	return (void *)((unsigned long)*pos + 1);
}

static void *himax_flash_dump_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_flash_dump_seq_stop(struct seq_file *s, void *v)
{
}

static const struct seq_operations himax_flash_dump_seq_ops = {
	.start = himax_flash_dump_seq_start,
	.next = himax_flash_dump_seq_next,
	.stop = himax_flash_dump_seq_stop,
	.show = himax_proc_flash_read,
};
static int himax_flash_dump_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_flash_dump_seq_ops);
};

static const struct proc_ops himax_proc_flash_ops = {
	//.owner = THIS_MODULE,
	.proc_open = himax_flash_dump_proc_open,
	.proc_read = seq_read,
	.proc_write = himax_proc_flash_write,
};

static void himax_ts_flash_func(struct himax_ts_data *ts)
{
	uint8_t flash_command = ts->debug.flash_cmd;

	himax_int_enable(ts, 0);
	flash_dump_prog_set(ts, ONGOING);

	/*msleep(100);*/
	I("%s: flash_command = %d enter.\n", __func__, flash_command);

	if (flash_command == 1 || flash_command == 2) {
		ts->core_fp.fp_flash_dump_func(ts, flash_command, ts->debug.flash_size,
					       ts->debug.flash_buffer);
		ts->debug.flash_dump_rst = true;
	}

	I("Complete~~~~~~~~~~~~~~~~~~~~~~~\n");

	/*	if (flash_command == 2) {
 *		struct file *fn;
 *		struct filename *vts_name;
 *
 *		vts_name = getname_kernel(FLASH_DUMP_FILE);
 *		fn = file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
 *
 *		if (!IS_ERR(fn)) {
 *			I("%s create file and ready to write\n", __func__);
 *			fn->f_op->write(fn, flash_buffer,
 *			  Flash_Size * sizeof(uint8_t), &fn->f_pos);
 *			filp_close(fn, NULL);
 *		} else {
 *			E("%s Open file failed!\n", __func__);
 *			g_flash_dump_rst = false;
 *		}
 *	}
 */
	himax_int_enable(ts, 1);
	flash_dump_prog_set(ts, FINISHED);
}

static ssize_t himax_sense_on_off_write(struct himax_ts_data *ts, char *buf, size_t len)
{
	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (buf[0] == '0') {
		ts->core_fp.fp_sense_off(ts, true);
		I("Sense off\n");
	} else if (buf[0] == '1') {
		if (buf[1] == 's') {
			ts->core_fp.fp_sense_on(ts, 0x00);
			I("Sense on re-map on, run sram\n");
		} else {
			ts->core_fp.fp_sense_on(ts, 0x01);
			I("Sense on re-map off, run flash\n");
		}
	} else {
		I("Do nothing\n");
	}

	return len;
}

static ssize_t himax_debug_read(struct file *file, char *buf, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = pde_data(file_inode(file));
	ssize_t ret = 0;
	int i = 0;

	if (!ts->hx_proc_send_flag) {
		I("%s, Enter\n", __func__);

		memset(ts->debug.buf_tmp, 0, sizeof(ts->debug.buf_tmp));
		if (dbg_cmd_flag) {
			if (dbg_func_ptr_r[dbg_cmd_flag])
				ret += dbg_func_ptr_r[dbg_cmd_flag](ts, buf, len);
			else
				goto END_FUNC_R;
		}

		if (ts->debug.debug_level_cmd == 't') {
			if (!ts->debug.fw_update_going) {
				if (ts->debug.fw_update_complete)
					ret += snprintf(ts->debug.buf_tmp + ret,
							sizeof(ts->debug.buf_tmp) - ret,
							"FW Update Complete ");
				else
					ret += snprintf(ts->debug.buf_tmp + ret,
							sizeof(ts->debug.buf_tmp) - ret,
							"FW Update Fail ");
			} else {
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"FW Update Ongoing ");
			}
		} else if (ts->debug.debug_level_cmd == 'h') {
			if (ts->debug.handshaking_result == 0)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Handshaking Result = %d (MCU Running)\n",
						ts->debug.handshaking_result);
			else if (ts->debug.handshaking_result == 1)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Handshaking Result = %d (MCU Stop)\n",
						ts->debug.handshaking_result);
			else if (ts->debug.handshaking_result == 2)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Handshaking Result = %d (I2C Error)\n",
						ts->debug.handshaking_result);
			else
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Handshaking Result = error\n");

		} else if (ts->debug.debug_level_cmd == 'v') {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Architecture Version = 0x%2.2X\n",
					ts->ic_data->vendor_arch_ver);

			if (ts->chip_cell_type == CHIP_IS_ON_CELL)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"CONFIG_VER = 0x%2.2X\n",
						ts->ic_data->vendor_config_ver);
			else {
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"FW Touch Config. Version = 0x%2.2X\n",
						ts->ic_data->vendor_touch_cfg_ver);
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"FW Display Config. Version = 0x%2.2X\n",
						ts->ic_data->vendor_display_cfg_ver);
			}
			if (ts->ic_data->vendor_cid_maj_ver < 0 &&
			    ts->ic_data->vendor_cid_min_ver < 0)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret, "CID = NULL\n");
			else
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret, "CID = 0x%2.2X\n",
						(ts->ic_data->vendor_cid_maj_ver << 8 |
						 ts->ic_data->vendor_cid_min_ver));

			if (ts->ic_data->vendor_panel_ver < 0)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Panel Version = NULL\n");
			else
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Panel Version = 0x%2.2X\n",
						ts->ic_data->vendor_panel_ver);
			if (ts->chip_cell_type == CHIP_IS_IN_CELL) {
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret, "Cusomer = %s\n",
						ts->ic_data->vendor_cus_info);

				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Project Name = %s\n",
						ts->ic_data->vendor_proj_info);
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Config. Date = %s\n",
						ts->ic_data->vendor_config_date);
			}
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"\n");
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Himax Touch Driver Version:\n");
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"%s\n", HIMAX_DRIVER_VER);
		} else if (ts->debug.debug_level_cmd == 'd') {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Himax Touch IC Information :\n");
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"%s\n", ts->chip_name);

			switch (ts->ic_checksum) {
			case HX_TP_BIN_CHECKSUM_SW:
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"IC Checksum : SW\n");
				break;

			case HX_TP_BIN_CHECKSUM_HW:
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"IC Checksum : HW\n");
				break;

			case HX_TP_BIN_CHECKSUM_CRC:
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"IC Checksum : CRC\n");
				break;

			default:
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"IC Checksum error.\n");
			}

			if (ts->ic_data->HX_INT_IS_EDGE)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Driver register Interrupt : EDGE TIRGGER\n");
			else
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Driver register Interrupt : LEVEL TRIGGER\n");

			if (ts->protocol_type == PROTOCOL_TYPE_A)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Protocol : TYPE_A\n");
			else
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Protocol : TYPE_B\n");

			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"RX Num : %d\n", ts->ic_data->HX_RX_NUM);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"TX Num : %d\n", ts->ic_data->HX_TX_NUM);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"BT Num : %d\n", ts->ic_data->HX_BT_NUM);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"X Resolution : %d\n", ts->ic_data->HX_X_RES);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Y Resolution : %d\n", ts->ic_data->HX_Y_RES);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"Max Point : %d\n", ts->ic_data->HX_MAX_PT);
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"XY reverse : %d\n", ts->ic_data->HX_XY_REVERSE);
#if defined(HX_TP_PROC_2T2R)
			if (ts->debug.is_2t2r) {
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret, "2T2R panel\n");
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret, "RX Num_2 : %d\n",
						ts->debug.hx_rx_num_2);
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret, "TX Num_2 : %d\n",
						ts->debug.hx_tx_num_2);
			}
#endif
		} else if (ts->debug.debug_level_cmd == 'n') {
			/* Edgd = 1, Level = 0 */
			if (ts->core_fp.fp_read_ic_trigger_type(ts) == 1)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"IC Interrupt type is edge trigger.\n");
			else if (ts->core_fp.fp_read_ic_trigger_type(ts) == 0)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"IC Interrupt type is level trigger.\n");
			else
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Unkown IC trigger type.\n");

			if (ts->ic_data->HX_INT_IS_EDGE)
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Driver register Interrupt : EDGE TIRGGER\n");
			else
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret,
						"Driver register Interrupt : LEVEL TRIGGER\n");
		} else if (ts->debug.debug_level_cmd == 'l') {
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"LotID : ");
			for (i = 0; i < 13; i++) {
				ret += snprintf(ts->debug.buf_tmp + ret,
						sizeof(ts->debug.buf_tmp) - ret, "%02X",
						ts->ic_data->vendor_ic_id[i]);
			}
			ret += snprintf(ts->debug.buf_tmp + ret, sizeof(ts->debug.buf_tmp) - ret,
					"\n");
		}

	END_FUNC_R:
		if (copy_to_user(buf, ts->debug.buf_tmp, (len > BUF_SIZE) ? BUF_SIZE : len))
			I("%s,here:%d\n", __func__, __LINE__);

		ts->hx_proc_send_flag = 1;
	} else {
		ts->hx_proc_send_flag = 0;
	}

	return ret;
}

static ssize_t himax_debug_write(struct file *file, const char *buff, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = pde_data(file_inode(file));
	char fileName[128];
	char buf[80] = "\0";
	int result = 0;
	int fw_type = 0;
	const struct firmware *fw = NULL;

	char *str_ptr = NULL;
	int str_len = 0;
	int i = 0;

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	str_len = len;
	buf[str_len - 1] = 0; /*remove \n*/

	while (dbg_cmd_str[i]) {
		str_ptr = strnstr(buf, dbg_cmd_str[i], len);
		if (str_ptr) {
			str_len = strlen(dbg_cmd_str[i]);
			dbg_cmd_flag = i + 1;
			ts->debug.debug_level_cmd = 0;
			I("Cmd is correct :%s, dbg_cmd = %d\n", str_ptr, dbg_cmd_flag);
			break;
		}
		i++;
	}
	if (!str_ptr) {
		I("Cmd is not correct\n");
		dbg_cmd_flag = 0;
	}

	if (buf[str_len] == ',') {
		dbg_cmd_par = buf + str_len + 1;
		if (dbg_func_ptr_w[dbg_cmd_flag])
			/* 2 => '/n' + ','*/
			dbg_func_ptr_w[dbg_cmd_flag](ts, dbg_cmd_par, len - str_len - 2);

		I("string of paremeter is %s, dbg_cmd_par = %s\n", buf + str_len + 1, dbg_cmd_par);
	} else {
		I("No paremeter of this cmd\n");
	}

	if (dbg_cmd_flag)
		return len;

	if (buf[0] == 'v') { /* firmware version */
		himax_int_enable(ts, 0);
		ts->debug.debug_level_cmd = buf[0];
		ts->core_fp.fp_reload_disable(ts, 0);
		ts->core_fp.fp_power_on_init(ts);
		ts->core_fp.fp_read_FW_ver(ts);
		himax_int_enable(ts, 1);
		return len;
	} else if (buf[0] == 'd') { /* ic information */
		ts->debug.debug_level_cmd = buf[0];
		return len;
	} else if (buf[0] == 't') {
		if (buf[1] == 's' && buf[2] == 'd' && buf[3] == 'b' && buf[4] == 'g') {
			if (buf[5] == '1') {
				I("Open Ts Debug!\n");
				ts->ts_dbg = 1;
			} else if (buf[5] == '0') {
				I("Close Ts Debug!\n");
				ts->ts_dbg = 0;
			} else {
				E("Parameter fault for ts debug\n");
			}
			goto ENDFUCTION;
		}
		himax_int_enable(ts, 0);
		ts->debug.debug_level_cmd = buf[0];
		ts->debug.fw_update_complete = false;
		ts->debug.fw_update_going = true;
		memset(fileName, 0, 128);
		/* parse the file name */
		snprintf(fileName, len - 2, "%s", &buf[2]);

		I("NOW Running common flow update!\n");
		I("%s: upgrade from file(%s) start!\n", __func__, fileName);
		result = request_firmware(&fw, fileName, ts->dev);

		if (result < 0) {
			I("fail to request_firmware fwpath: %s (ret:%d)\n", fileName, result);
			return result;
		}

		I("%s: FW image: %02X, %02X, %02X, %02X\n", __func__, fw->data[0], fw->data[1],
		  fw->data[2], fw->data[3]);
		fw_type = (fw->size) / 1024;
		/*	start to upgrade */
		himax_int_enable(ts, 0);
		I("Now FW size is : %dk\n", fw_type);

		switch (fw_type) {
		case 64:
			if (ts->core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k(
				    ts, (unsigned char *)fw->data, fw->size, false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
				ts->debug.fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
				ts->debug.fw_update_complete = true;
			}
			break;
		case 128:
			if (ts->core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k(
				    ts, (unsigned char *)fw->data, fw->size, false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
				ts->debug.fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
				ts->debug.fw_update_complete = true;
			}
			break;

		default:
			E("%s: Flash command fail: %d\n", __func__, __LINE__);
			ts->debug.fw_update_complete = false;
			break;
		}
		release_firmware(fw);
		goto firmware_upgrade_done;

	} else if (buf[0] == 'i' && buf[1] == 'n' && buf[2] == 't') {
		/* INT trigger */
		ts->debug.debug_level_cmd = 'n';
		return len;
	} else if (buf[0] == 'l' && buf[1] == 'o' && buf[2] == 't') {
		ts->debug.debug_level_cmd = buf[0];
		ts->core_fp.fp_sense_off(ts, true);
		ts->core_fp.fp_ic_id_read(ts);
		ts->core_fp.fp_sense_on(ts, 0x01);
		return len;
	}
	/* others,do nothing */
	ts->debug.debug_level_cmd = 0;
	return len;

firmware_upgrade_done:
	ts->debug.fw_update_going = false;
	ts->core_fp.fp_reload_disable(ts, 0);

	ts->core_fp.fp_reload_disable(ts, 0);
	ts->core_fp.fp_power_on_init(ts);

	/* need to be review with FW update flow */
	result = himax_mcu_WP_BP_enable(ts);

	if (result < 0) {
		I("%s: WP BP enable fail\n", __func__);
	}
	ts->core_fp.fp_read_FW_ver(ts);
	ts->core_fp.fp_touch_information(ts);

	himax_int_enable(ts, 1);
	/* todo himax_chip->tp_firmware_upgrade_proceed = 0;
	 * todo himax_chip->suspend_state = 0;
	 * todo enable_irq(himax_chip->irq);
	 */
ENDFUCTION:
	return len;
}

static const struct proc_ops himax_proc_debug_ops = {
	.proc_read = himax_debug_read,
	.proc_write = himax_debug_write,
};

static void himax_himax_data_init(struct himax_ts_data *ts)
{
	ts->debug.ops.fp_ts_dbg_func = himax_ts_dbg_func;
	ts->debug.ops.fp_set_diag_cmd = himax_set_diag_cmd;
	ts->debug.flash_dump_going = false;
}

static void himax_ts_flash_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, flash_work);

	himax_ts_flash_func(ts);
}

static void himax_ts_diag_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts =
		container_of(work, struct himax_ts_data, himax_diag_delay_wrok.work);
	himax_ts_diag_func(ts);
}

static void dbg_func_ptr_init(void)
{
	/*debug function ptr init*/
	dbg_func_ptr_r[1] = himax_crc_test_read;
	dbg_func_ptr_r[2] = himax_proc_FW_debug_read;
	dbg_func_ptr_r[3] = himax_attn_read;
	dbg_func_ptr_r[4] = himax_layout_read;
	dbg_func_ptr_w[4] = himax_layout_write;
	dbg_func_ptr_w[5] = himax_sense_on_off_write;
	dbg_func_ptr_r[6] = himax_debug_level_read;
	dbg_func_ptr_w[6] = himax_debug_level_write;
	dbg_func_ptr_r[7] = himax_int_en_read;
	dbg_func_ptr_w[7] = himax_int_en_write;
	dbg_func_ptr_w[8] = himax_proc_register_write;
	dbg_func_ptr_r[8] = himax_proc_register_read;
	dbg_func_ptr_w[9] = himax_reset_write;
	dbg_func_ptr_w[10] = himax_diag_arrange_write;
	dbg_func_ptr_w[11] = himax_diag_cmd_write;
	dbg_func_ptr_w[12] = himax_GMA_cmd_write;
}

int himax_touch_proc_init(struct himax_ts_data *ts)
{
	ts->debug.procfs.diag_dir = proc_mkdir(HIMAX_PROC_DIAG_FOLDER, ts->debug.procfs.proc_dir);

	if (ts->debug.procfs.diag_dir == NULL) {
		E(" %s: diag_dir file create failed!\n", __func__);
		return -ENOMEM;
	}

	ts->debug.procfs.vendor =
		proc_create_data(HIMAX_PROC_VENDOR_FILE, 0444, ts->debug.procfs.proc_dir,
				 &himax_proc_vendor_ops, ts);
	if (ts->debug.procfs.vendor == NULL) {
		E(" %s: proc vendor file create failed!\n", __func__);
		goto fail_1;
	}

	ts->debug.procfs.stack = proc_create_data(
		HIMAX_PROC_STACK_FILE, 0444, ts->debug.procfs.diag_dir, &himax_proc_stack_ops, ts);
	if (ts->debug.procfs.stack == NULL) {
		E(" %s: proc stack file create failed!\n", __func__);
		goto fail_2_1;
	}

	ts->debug.procfs.delta = proc_create_data(
		HIMAX_PROC_DELTA_FILE, 0444, ts->debug.procfs.diag_dir, &himax_proc_delta_ops, ts);
	if (ts->debug.procfs.delta == NULL) {
		E(" %s: proc delta file create failed!\n", __func__);
		goto fail_2_2;
	}

	ts->debug.procfs.dc = proc_create_data(HIMAX_PROC_DC_FILE, 0444, ts->debug.procfs.diag_dir,
					       &himax_proc_dc_ops, ts);
	if (ts->debug.procfs.dc == NULL) {
		E(" %s: proc dc file create failed!\n", __func__);
		goto fail_2_3;
	}

	ts->debug.procfs.baseline =
		proc_create_data(HIMAX_PROC_BASELINE_FILE, 0444, ts->debug.procfs.diag_dir,
				 &himax_proc_baseline_ops, ts);
	if (ts->debug.procfs.baseline == NULL) {
		E(" %s: proc baseline file create failed!\n", __func__);
		goto fail_2_4;
	}

	ts->debug.procfs.debug = proc_create_data(
		HIMAX_PROC_DEBUG_FILE, 0644, ts->debug.procfs.proc_dir, &himax_proc_debug_ops, ts);
	if (ts->debug.procfs.debug == NULL) {
		E(" %s: proc debug file create failed!\n", __func__);
		goto fail_3;
	}
	dbg_func_ptr_init();

	ts->debug.procfs.flash_dump =
		proc_create_data(HIMAX_PROC_FLASH_DUMP_FILE, 0644, ts->debug.procfs.proc_dir,
				 &himax_proc_flash_ops, ts);
	if (ts->debug.procfs.flash_dump == NULL) {
		E(" %s: proc flash dump file create failed!\n", __func__);
		goto fail_4;
	}

	return 0;

	/* remove_proc_entry(HIMAX_PROC_PEN_POS_FILE, ts->debug.procfs.proc_dir); */
fail_4:
	remove_proc_entry(HIMAX_PROC_DEBUG_FILE, ts->debug.procfs.proc_dir);
fail_3:
	remove_proc_entry(HIMAX_PROC_BASELINE_FILE, ts->debug.procfs.diag_dir);
fail_2_4:
	remove_proc_entry(HIMAX_PROC_DC_FILE, ts->debug.procfs.diag_dir);
fail_2_3:
	remove_proc_entry(HIMAX_PROC_DELTA_FILE, ts->debug.procfs.diag_dir);
fail_2_2:
	remove_proc_entry(HIMAX_PROC_STACK_FILE, ts->debug.procfs.diag_dir);
fail_2_1:
	remove_proc_entry(HIMAX_PROC_VENDOR_FILE, ts->debug.procfs.proc_dir);
fail_1:
	return -ENOMEM;
}

void himax_touch_proc_deinit(struct himax_ts_data *ts)
{
	remove_proc_entry(HIMAX_PROC_FLASH_DUMP_FILE, ts->debug.procfs.proc_dir);
	remove_proc_entry(HIMAX_PROC_DEBUG_FILE, ts->debug.procfs.proc_dir);
	remove_proc_entry(HIMAX_PROC_BASELINE_FILE, ts->debug.procfs.diag_dir);
	remove_proc_entry(HIMAX_PROC_DC_FILE, ts->debug.procfs.diag_dir);
	remove_proc_entry(HIMAX_PROC_DELTA_FILE, ts->debug.procfs.diag_dir);
	remove_proc_entry(HIMAX_PROC_STACK_FILE, ts->debug.procfs.diag_dir);
	remove_proc_entry(HIMAX_PROC_VENDOR_FILE, ts->debug.procfs.proc_dir);
	remove_proc_entry(HIMAX_PROC_DIAG_FOLDER, ts->debug.procfs.proc_dir);
}

int himax_debug_init(struct himax_ts_data *ts)
{
	I("%s:Enter\n", __func__);

	if (ts == NULL) {
		E("%s: ts struct is NULL\n", __func__);
		return -EPROBE_DEFER;
	}

	ts->debug.min_mutual = 0xFFFF;
	ts->debug.min_self = 0xFFFF;
	ts->debug.flash_size = 0x2B000; /*0x20000;*/

	ts->debug.reg_read_data = kzalloc(128 * sizeof(uint8_t), GFP_KERNEL);
	if (ts->debug.reg_read_data == NULL) {
		E("%s: reg_read_data allocate failed\n", __func__);
		goto err_alloc_reg_read_data_fail;
	}

	himax_himax_data_init(ts);

	ts->flash_wq = create_singlethread_workqueue("himax_flash_wq");

	if (!ts->flash_wq) {
		E("%s: create flash workqueue failed\n", __func__);
		goto err_create_flash_dump_wq_failed;
	}

	INIT_WORK(&ts->flash_work, himax_ts_flash_work_func);

	ts->debug.flash_progress = START;
	setFlashBuffer(ts);
	if (ts->debug.flash_buffer == NULL) {
		E("%s: flash buffer allocate fail failed\n", __func__);
		goto err_flash_buf_alloc_failed;
	}

	ts->himax_diag_wq = create_singlethread_workqueue("himax_diag");

	if (!ts->himax_diag_wq) {
		E("%s: create diag workqueue failed\n", __func__);
		goto err_create_diag_wq_failed;
	}

	INIT_DELAYED_WORK(&ts->himax_diag_delay_wrok, himax_ts_diag_work_func);

	setSelfBuffer(ts, ts->ic_data->HX_RX_NUM, ts->ic_data->HX_TX_NUM);
	if (getSelfBuffer(ts) == NULL) {
		E("%s: self buffer allocate failed\n", __func__);
		goto err_self_buf_alloc_failed;
	}

	setSelfNewBuffer(ts, ts->ic_data->HX_RX_NUM, ts->ic_data->HX_TX_NUM);
	if (getSelfNewBuffer(ts) == NULL) {
		E("%s: self new buffer allocate failed\n", __func__);
		goto err_self_new_alloc_failed;
	}

	setSelfOldBuffer(ts, ts->ic_data->HX_RX_NUM, ts->ic_data->HX_TX_NUM);
	if (getSelfOldBuffer(ts) == NULL) {
		E("%s: self old buffer allocate failed\n", __func__);
		goto err_self_old_alloc_failed;
	}

	setMutualBuffer(ts, ts->ic_data->HX_RX_NUM, ts->ic_data->HX_TX_NUM);
	if (getMutualBuffer(ts) == NULL) {
		E("%s: mutual buffer allocate failed\n", __func__);
		goto err_mut_buf_alloc_failed;
	}

	setMutualNewBuffer(ts, ts->ic_data->HX_RX_NUM, ts->ic_data->HX_TX_NUM);
	if (getMutualNewBuffer(ts) == NULL) {
		E("%s: mutual new buffer allocate failed\n", __func__);
		goto err_mut_new_alloc_failed;
	}

	setMutualOldBuffer(ts, ts->ic_data->HX_RX_NUM, ts->ic_data->HX_TX_NUM);
	if (getMutualOldBuffer(ts) == NULL) {
		E("%s: mutual old buffer allocate failed\n", __func__);
		goto err_mut_old_alloc_failed;
	}

#if defined(HX_TP_PROC_2T2R)

	if (ts->debug.is_2t2r) {
		setMutualBuffer_2(ts, ts->ic_data->HX_RX_NUM_2, ts->ic_data->HX_TX_NUM_2);

		if (getMutualBuffer_2(ts) == NULL) {
			E("%s: mutual buffer 2 allocate failed\n", __func__);
			goto err_mut_buf2_alloc_failed;
		}
	}
#endif

	if (himax_touch_proc_init(ts))
		goto err_proc_init_failed;

	return 0;

err_proc_init_failed:
#if defined(HX_TP_PROC_2T2R)
	kfree(ts->debug.diag_mutual_2);
	ts->debug.diag_mutual_2 = NULL;
err_mut_buf2_alloc_failed:
#endif
	kfree(ts->debug.diag_mutual_old);
	ts->debug.diag_mutual_old = NULL;
err_mut_old_alloc_failed:
	kfree(ts->debug.diag_mutual_new);
	ts->debug.diag_mutual_new = NULL;
err_mut_new_alloc_failed:
	kfree(ts->debug.diag_mutual);
	ts->debug.diag_mutual = NULL;
err_mut_buf_alloc_failed:
	kfree(ts->debug.diag_self_old);
	ts->debug.diag_self_old = NULL;
err_self_old_alloc_failed:
	kfree(ts->debug.diag_self_new);
	ts->debug.diag_self_new = NULL;
err_self_new_alloc_failed:
	kfree(ts->debug.diag_self);
	ts->debug.diag_self = NULL;
err_self_buf_alloc_failed:
	cancel_delayed_work_sync(&ts->himax_diag_delay_wrok);
	destroy_workqueue(ts->himax_diag_wq);
err_create_diag_wq_failed:

	kfree(ts->debug.flash_buffer);
	ts->debug.flash_buffer = NULL;
err_flash_buf_alloc_failed:
	destroy_workqueue(ts->flash_wq);
err_create_flash_dump_wq_failed:
	kfree(ts->debug.reg_read_data);
	ts->debug.reg_read_data = NULL;
err_alloc_reg_read_data_fail:

	return -ENOMEM;
}
EXPORT_SYMBOL(himax_debug_init);

int himax_debug_remove(struct himax_ts_data *ts)
{
	himax_touch_proc_deinit(ts);
	cancel_delayed_work_sync(&ts->himax_diag_delay_wrok);

	destroy_workqueue(ts->himax_diag_wq);
	destroy_workqueue(ts->flash_wq);

	if (ts->debug.reg_read_data != NULL)
		kfree(ts->debug.reg_read_data);

	return 0;
}
EXPORT_SYMBOL(himax_debug_remove);
