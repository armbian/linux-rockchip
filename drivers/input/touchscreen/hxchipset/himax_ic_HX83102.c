// SPDX-License-Identifier: GPL-2.0
/*  Himax Android Driver Sample Code for HX83102 chipset (incell, new core)
 *
 * Copyright (C) 2026 Himax Corporation.
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
#include "himax_ic_core.h"
#include "himax_ic_HX83102.h"
#include <linux/types.h>
#include <linux/string.h>

#define HX_83102D_SERIES_PWON "HX83102D"
#define HX_83102E_SERIES_PWON "HX83102E"
#define HX_83102J_SERIES_PWON "HX83102J"

static void hx83102_chip_init(struct himax_ts_data *ts)
{
	ts->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, ts->chip_cell_type);
	ts->ic_checksum = HX_TP_BIN_CHECKSUM_CRC;

	/* Default (D) flash layout */
	ts->fw_ver_maj_flash_addr = 49157; /*0x00C005*/
	ts->fw_ver_min_flash_addr = 49158; /*0x00C006*/
	ts->cfg_ver_maj_flash_addr = 49408; /*0x00C100*/
	ts->cfg_ver_min_flash_addr = 49409; /*0x00C101*/
	ts->cid_ver_maj_flash_addr = 49154; /*0x00C002*/
	ts->cid_ver_min_flash_addr = 49155; /*0x00C003*/
	ts->cfg_table_flash_addr = 0x10000;

	/* Override for E/J if chip_name was already set */
	if (!strcmp(ts->chip_name, HX_83102E_SERIES_PWON) ||
	    !strcmp(ts->chip_name, HX_83102J_SERIES_PWON)) {
		ts->fw_ver_maj_flash_addr = 59397; /*0x00E805*/
		ts->fw_ver_min_flash_addr = 59398; /*0x00E806*/
		ts->cfg_ver_maj_flash_addr = 59648; /*0x00E900*/
		ts->cfg_ver_min_flash_addr = 59649; /*0x00E901*/
		ts->cid_ver_maj_flash_addr = 59394; /*0x00E802*/
		ts->cid_ver_min_flash_addr = 59395; /*0x00E803*/
		ts->cfg_table_flash_addr = 0x10000;
	}
}

static void hx83102_sense_on(struct himax_ts_data *ts, uint8_t FlashMode)
{
	uint8_t tmp_addr[DATA_LEN_4] = { 0 };
	uint8_t tmp_data[DATA_LEN_4] = { 0 };
	int retry = 0;
	int ret = 0;

	ts->core_fp.fp_interface_on(ts);

	if (!FlashMode) {
#if defined(HX_RST_PIN_FUNC)
		if (HX_SYSTEM_RESET == 0)
			ts->core_fp.fp_ic_reset(ts, false, false);
		else
			ts->core_fp.fp_system_reset(ts);
#endif
	} else {
		do {
			himax_parse_assign_cmd(addr_ctrl_fw, tmp_addr,
					       sizeof(tmp_addr));
			himax_parse_assign_cmd(data_clear, tmp_data,
					       sizeof(tmp_data));
			himax_mcu_register_write(ts, tmp_addr, DATA_LEN_4,
						 tmp_data, 0);

			msleep(20);

			himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4,
						tmp_data, 0);

			I("%s:Read status from IC = %X,%X\n", __func__,
			  tmp_data[0], tmp_data[1]);
		} while (tmp_data[0] != 0x00 && retry++ < 5);

		if (retry >= 5) {
			E("%s: Fail:\n", __func__);
#if defined(HX_RST_PIN_FUNC)
			if (HX_SYSTEM_RESET == 0)
				ts->core_fp.fp_ic_reset(ts, false, false);
			else
				ts->core_fp.fp_system_reset(ts);
#endif
		} else {
			I("%s:OK and Read status from IC = %X,%X\n", __func__,
			  tmp_data[0], tmp_data[1]);
			/* reset code*/
			tmp_data[0] = 0x00;
			tmp_data[1] = 0x00;

			ret = himax_bus_write(ts, addr_sense_on_off_0,
					      tmp_data, 2,
					      HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);
		}
		msleep(280);
	}
}

static bool hx83102_sense_off(struct himax_ts_data *ts, bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4] = { 0 };
	uint8_t tmp_data[DATA_LEN_4] = { 0 };
	uint8_t cMax = 7;
	uint8_t check = 0x87;
	int ret = 0;

	msleep(280);

	himax_parse_assign_cmd(addr_cs_central_state, tmp_addr,
			       sizeof(tmp_addr));
	himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, false);

	if (tmp_data[0] != 0x0C) {
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x5C;
		cnt = 0;
		do {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0xA5;
			himax_mcu_register_write(ts, tmp_addr, DATA_LEN_4,
						 tmp_data, 0);
			msleep(20);
			himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4,
						tmp_data, 0);
			I("%s: Check 9000005C data[0]=%X\n", __func__,
			  tmp_data[0]);
			if (cnt++ >= cMax)
				break;
		} while (tmp_data[0] != check);
	}

	do {
		tmp_data[0] = para_sense_off_0;

		ret = himax_bus_write(ts, addr_sense_on_off_0, tmp_data, 1,
				      HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		tmp_data[0] = para_sense_off_1;

		ret = himax_bus_write(ts, addr_sense_on_off_1, tmp_data, 1,
				      HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		himax_parse_assign_cmd(addr_cs_central_state, tmp_addr,
				       sizeof(tmp_addr));
		himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data,
					false);
		I("%s: Check enter_save_mode data[0]=%X\n", __func__,
		  tmp_data[0]);

		if (tmp_data[0] == 0x0C)
			return true;
		else if (cnt == 6) {
			usleep_range(10000, 11000);
#if defined(HX_RST_PIN_FUNC)
			if (HX_SYSTEM_RESET == 0)
				ts->core_fp.fp_ic_reset(ts, false, false);
			else
				ts->core_fp.fp_system_reset(ts);
#endif
		}

	} while (cnt++ < 15);

	return false;
}

static bool hx83102_mcu_ic_id_read(struct himax_ts_data *ts)
{
	I("%s: [HX83102]\n", __func__);
	return true;
}

static bool hx83102_mcu_dd_clk_set(struct himax_ts_data *ts, bool enable)
{
	uint8_t data[4] = { 0 };

	data[0] = (enable) ? 0xDD : 0x00;
	return (himax_mcu_register_write(ts, ts->ic_incell.pfw_op->addr_osc_en,
					 sizeof(ts->ic_incell.pfw_op->addr_osc_en),
					 data, 0) == NO_ERR);
}

static void hx83102_mcu_dd_reg_en(struct himax_ts_data *ts, bool enable)
{
	uint8_t data[4] = { 0 };

	data[0] = 0xA5;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	himax_mcu_register_write(ts, ts->ic_incell.pfw_op->addr_osc_pw,
				 sizeof(ts->ic_incell.pfw_op->addr_osc_pw),
				 data, 0);
	data[0] = 0x00;
	data[1] = 0x55;
	data[2] = 0x66;
	data[3] = 0xCC;

	ts->core_fp.fp_dd_reg_write(ts, 0xEB, 0, 4, data, 0);
	data[0] = 0x00;
	data[1] = 0x83;
	data[2] = 0x10;
	data[3] = 0x2D;
	ts->core_fp.fp_dd_reg_write(ts, 0xB9, 0, 4, data, 0);
}

static void hx83102_func_re_init(struct himax_ts_data *ts)
{
	ts->core_fp.fp_sense_on = hx83102_sense_on;
	ts->core_fp.fp_sense_off = hx83102_sense_off;
	ts->core_fp.fp_chip_init = hx83102_chip_init;
	ts->core_fp.fp_ic_id_read = hx83102_mcu_ic_id_read;
	ts->core_fp.fp_dd_clk_set = hx83102_mcu_dd_clk_set;
	ts->core_fp.fp_dd_reg_en = hx83102_mcu_dd_reg_en;
}

bool hx83102_chip_detect(struct himax_ts_data *ts)
{
	uint8_t tmp_data[DATA_LEN_4] = { 0 };
	uint8_t tmp_addr[DATA_LEN_4] = { 0 };
	bool ret_data = false;
	int ret = 0;
	int i = 0;

	ret = himax_mcu_in_cmd_struct_init(ts);
	if (ret < 0) {
		ret_data = false;
		E("%s:cmd_struct_init Fail:\n", __func__);
		return ret_data;
	}

	himax_mcu_in_cmd_init(ts);

	hx83102_func_re_init(ts);

	ts->core_fp.fp_sense_off(ts, true);

	for (i = 0; i < 5; i++) {
		himax_parse_assign_cmd(addr_icid_addr, tmp_addr,
				       sizeof(tmp_addr));
		himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data,
					false);
		I("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3],
		  tmp_data[2], tmp_data[1]);

		if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x10) &&
		    (tmp_data[1] == 0x2d || tmp_data[1] == 0x2e ||
		     tmp_data[1] == 0x29)) {
			if (tmp_data[1] == 0x2d) {
				strscpy(ts->chip_name, HX_83102D_SERIES_PWON,
					sizeof(ts->chip_name));
				ts->ic_data->ic_adc_num = hx83102d_data_adc_num;
				I("%s:detect IC HX83102D successfully\n",
				  __func__);
			} else if (tmp_data[1] == 0x29) {
				strscpy(ts->chip_name, HX_83102J_SERIES_PWON,
					sizeof(ts->chip_name));
				ts->ic_data->ic_adc_num = hx83102j_data_adc_num;
				ts->debug.flash_size = HX83102JA_FLASH_SIZE;
				I("%s:detect IC HX83102J successfully\n",
				  __func__);
			} else {
				strscpy(ts->chip_name, HX_83102E_SERIES_PWON,
					sizeof(ts->chip_name));
				ts->ic_data->ic_adc_num = hx83102e_data_adc_num;
				I("%s:detect IC HX83102E successfully\n",
				  __func__);
			}

			ret_data = true;
			return ret_data;
		}
	}
	ret_data = false;
	E("%s:Read driver ID register Fail:\n", __func__);
	E("Could NOT find Himax Chipset\n");
	E("Please check 1.VCCD,VCCA,VSP,VSN\n");
	E("2.LCM_RST,TP_RST\n");
	E("3.Power On Sequence\n");

	return ret_data;
}
EXPORT_SYMBOL(hx83102_chip_detect);
