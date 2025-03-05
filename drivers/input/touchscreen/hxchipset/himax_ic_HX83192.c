/* Himax Android Driver Sample Code for HX83192 chipset
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

#define hx83192_data_adc_num 120

#include "himax.h"
#include "himax_ic_core.h"

static void hx83192_chip_init(struct himax_ts_data *ts)
{
	ts->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, ts->chip_cell_type);
	ts->ic_checksum = HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	ts->fw_ver_maj_flash_addr = 49157; /*0x00C005*/
	ts->fw_ver_min_flash_addr = 49158; /*0x00C006*/
	ts->cfg_ver_maj_flash_addr = 49408; /*0x00C100*/
	ts->cfg_ver_min_flash_addr = 49409; /*0x00C101*/
	ts->cid_ver_maj_flash_addr = 49154; /*0x00C002*/
	ts->cid_ver_min_flash_addr = 49155; /*0x00C003*/
	ts->cfg_table_flash_addr = 0x10000;
}

static bool himax_get_ic_Amount(struct himax_ts_data *ts)
{
	bool result = false;
	uint8_t tmp_addr[DATA_LEN_4] = { 0 };
	uint8_t tmp_data[DATA_LEN_4] = { 0 };
	int cascadeenb;

	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0xEC;
	himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, false);
	if (result) {
		cascadeenb = (tmp_data[1] >> 2);
		switch (cascadeenb) {
		case 0:
			ts->hx_ic_amount = 3;
			break;
		case 2:
			ts->hx_ic_amount = 2;
			break;
		case 3:
			ts->hx_ic_amount = 1;
			break;
		default:
			result = false;
			break;
		}
	}
	I("hx_ic_Amount : %d\n", ts->hx_ic_amount);

	return result;
}

static void hx83192_sense_on(struct himax_ts_data *ts, uint8_t FlashMode)
{
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	int retry = 0;
	int ret = 0;

	I("Enter %s\n", __func__);
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
			himax_parse_assign_cmd(addr_ctrl_fw, tmp_addr, sizeof(tmp_addr));
			himax_parse_assign_cmd(data_clear, tmp_data, sizeof(tmp_data));
			himax_mcu_register_write(ts, tmp_addr, DATA_LEN_4, tmp_data, 0);

			msleep(20);

			himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, 0);

			I("%s:Read status from IC = %X,%X\n", __func__, tmp_data[0], tmp_data[1]);
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
			I("%s:OK and Read status from IC = %X,%X\n", __func__, tmp_data[0],
			  tmp_data[1]);
			/* reset code*/
			tmp_data[0] = 0x00;
			tmp_data[1] = 0x00;

			ret = himax_bus_write(ts, addr_sense_on_off_0, tmp_data, 2,
					      HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);

			/*ret = himax_bus_write(ts, ts->ic_incell.pfw_op
				->adr_i2c_psw_ub[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);*/
		}
		msleep(280);

#if defined(HIMAX_I2C_PLATFORM)
		ret = himax_bus_read(ts, addr_AHB_rdata_byte_0, tmp_data, DATA_LEN_4,
				     HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
		}
#endif
	}
}

static bool hx83192_sense_off(struct himax_ts_data *ts, bool check_en)
{
	bool result = true;
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4] = { 0 };
	uint8_t tmp_data[DATA_LEN_4] = { 0 };
	uint8_t cMax = 7;
	uint8_t check = 0x87;
	int ret = 0;

	msleep(280);

	himax_parse_assign_cmd(addr_cs_central_state, tmp_addr, sizeof(tmp_addr));
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
			himax_mcu_register_write(ts, tmp_addr, DATA_LEN_4, tmp_data, 0);
			msleep(20);
			himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, 0);
			I("%s: Check 9000005C data[0]=%X\n", __func__, tmp_data[0]);
			if (cnt++ >= cMax)
				break;
		} while (tmp_data[0] != check);
	}

	do {
		tmp_data[0] = para_sense_off_0;

		ret = himax_bus_write(ts, addr_sense_on_off_0, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		tmp_data[0] = para_sense_off_1;

		ret = himax_bus_write(ts, addr_sense_on_off_1, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		himax_parse_assign_cmd(addr_cs_central_state, tmp_addr, sizeof(tmp_addr));
		himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, false);
		I("%s: Check enter_save_mode data[0]=%X\n", __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			return true;
		} else if (cnt == 6) {
			usleep_range(10000, 11000);
#if defined(HX_RST_PIN_FUNC)
			if (HX_SYSTEM_RESET == 0)
				ts->core_fp.fp_ic_reset(ts, false, false);
			else
				ts->core_fp.fp_system_reset(ts);
#endif
		}

	} while (cnt++ < 15);

	return result;
}

static int hx83192_mcu_register_read(struct himax_ts_data *ts, uint8_t *read_addr,
				     uint32_t read_length, uint8_t *read_data, uint8_t cfg_flag)
{
	uint8_t tmp_data[DATA_LEN_4];
	int i = 0;
	int address = 0;
	int ret = 0;

	if (cfg_flag == false) {
		if (read_length > FLASH_RW_MAX_LEN) {
			E("%s: read len over %d!\n", __func__, FLASH_RW_MAX_LEN);
			return LENGTH_FAIL;
		}

		address = (read_addr[3] << 24) + (read_addr[2] << 16) + (read_addr[1] << 8) +
			  read_addr[0];
		i = address;
		tmp_data[0] = (uint8_t)i;
		tmp_data[1] = (uint8_t)(i >> 8);
		tmp_data[2] = (uint8_t)(i >> 16);
		tmp_data[3] = (uint8_t)(i >> 24);

		ret = himax_bus_write(ts, addr_AHB_address_byte_0, tmp_data, DATA_LEN_4,
				      HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

		tmp_data[0] = para_AHB_access_direction_read;

		ret = himax_bus_write(ts, addr_AHB_access_direction, tmp_data, 1,
				      HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

		ret = himax_bus_read(ts, addr_AHB_rdata_byte_0, read_data, read_length,
				     HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

	} else {
		ret = himax_bus_read(ts, read_addr[0], read_data, read_length,
				     HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}
	}
	return NO_ERR;
}

static void hx83192_mcu_flash_dump_func(struct himax_ts_data *ts, uint8_t local_flash_command,
					int Flash_Size, uint8_t *flash_buffer)
{
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	int page_prog_start = 0, retry_cnt = 0;

	I("%s,Entering\n", __func__);

	ts->core_fp.fp_sense_off(ts, true);
	ts->core_fp.fp_burst_enable(ts, 0);

	/* ===SPI RX-FIFO Reset===*/
	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_fifo_rst, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_rxfifo_rst, 0);

	do {
		himax_mcu_register_read(ts, ts->ic_incell.pflash_op->addr_spi200_fifo_rst,
					DATA_LEN_4, tmp_data, 0);

		if (retry_cnt > 50) {
			E("%s: Polling SPI Status FAIL", __func__);
			return;
		}
		retry_cnt++;
	} while ((tmp_data[0] & 0x02) != 0);

	/* ===SPI Transfer Control===*/
	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_ctrl, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_trans_ctrl_7, 0);

	for (page_prog_start = 0; page_prog_start < Flash_Size; page_prog_start += 16) {
		tmp_addr[0] = page_prog_start % 0x100;
		tmp_addr[1] = (page_prog_start >> 8) % 0x100;
		tmp_addr[2] = (page_prog_start >> 16) % 0x100;
		tmp_addr[3] = page_prog_start / 0x1000000;

		/* ===Set SPI Address ===*/
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_addr, DATA_LEN_4,
					 tmp_addr, 0);
		/* ===SPI Transfer Control===*/
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_cmd_7, 0);
		retry_cnt = 0;
		do {
			himax_mcu_register_read(ts, ts->ic_incell.pflash_op->addr_spi200_rst_status,
						DATA_LEN_4, tmp_data, 0);

			if (retry_cnt > 50) {
				E("%s: Polling SPI Status FAIL", __func__);
				return;
			}
			retry_cnt++;
		} while ((tmp_data[1] & 0x80) == 0);

		hx83192_mcu_register_read(ts, ts->ic_incell.pflash_op->addr_spi200_data, 16,
					  &flash_buffer[page_prog_start], false);
	}

	ts->core_fp.fp_sense_on(ts, 0x01);
}

static void hx83192_mcu_flash_programming(struct himax_ts_data *ts, uint8_t *FW_content,
					  int start_addr, int length)
{
	int page_prog_start = 0, i = 0, j = 0, k = 0, retry_cnt = 0;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t buring_data[FLASH_RW_MAX_LEN]; /* Read for flash data, 128K*/

	I("%s", __func__);
	/* 4 bytes for padding*/
	ts->core_fp.fp_interface_on(ts);

	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_flash_speed, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_cmd_8, 0);

	/* ===SPI TX-FIFO Reset===*/
	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_fifo_rst, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_txfifo_rst, 0);

	/* ===Polling Reset Status ===*/
	retry_cnt = 0;

	do {
		himax_mcu_register_read(ts, ts->ic_incell.pflash_op->addr_spi200_fifo_rst,
					DATA_LEN_4, tmp_data, 0);

		if (retry_cnt > 50) {
			E("%s: Polling SPI Status Active FAIL", __func__);
			return;
		}
		retry_cnt++;
	} while (((tmp_data[0] & 0x04) >> 2) == 1);

	/* ===SPI Transfer Format===*/
	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_fmt, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_trans_fmt, 0);

	for (page_prog_start = start_addr; page_prog_start < start_addr + length;
	     page_prog_start += FLASH_RW_MAX_LEN) {
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_ctrl,
					 DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_trans_ctrl_2, 0);
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_cmd_2, 0);

		/* ===Polling SPI Status Active ===*/
		retry_cnt = 0;
		do {
			himax_mcu_register_read(ts, ts->ic_incell.pflash_op->addr_spi200_rst_status,
						4, tmp_data, 0);

			if (retry_cnt > 50) {
				E("%s: Polling FAIL", __func__);
				return;
			}
			retry_cnt++;
		} while ((tmp_data[0] & 0x01) == 1);

		/* ===WEL Write Enable ===*/
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_ctrl,
					 DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_trans_ctrl_6, 0);

		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_cmd_1, 0);

		/* ===Polling SPI Status Active ===*/
		retry_cnt = 0;
		do {
			himax_mcu_register_read(ts, ts->ic_incell.pflash_op->addr_spi200_rst_status,
						DATA_LEN_4, tmp_data, 0);
			if (retry_cnt > 50) {
				E("%s: Polling FAIL", __func__);
				return;
			}
			retry_cnt++;
		} while ((tmp_data[0] & 0x01) == 1);

		himax_mcu_register_read(ts, ts->ic_incell.pflash_op->addr_spi200_data, DATA_LEN_4,
					tmp_data, 0);
		//WEL Fail
		if (((tmp_data[0] & 0x02) >> 1) == 0) {
			I("%s:SPI 0x8000002c = %d\n", __func__, tmp_data[0]);
			break;
		}

		/*Programmable size = 256 bytes, word_number = 256/4 = 64*/
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_ctrl,
					 DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_trans_ctrl_4, 0);

		/* Flash start address 1st : 0x0000_0000*/
		if (page_prog_start < 0x100) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = (uint8_t)page_prog_start;
		} else if (page_prog_start >= 0x100 && page_prog_start < 0x10000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		} else if (page_prog_start >= 0x10000 && page_prog_start < 0x1000000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = (uint8_t)(page_prog_start >> 16);
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_addr, DATA_LEN_4,
					 tmp_data, 0);

		for (i = 0; i < ADDR_LEN_4; i++)
			buring_data[i] = ts->ic_incell.pflash_op->addr_spi200_data[i];

		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_cmd_6, 0);

		for (j = 0; j < 16; j++) {
			for (i = (page_prog_start + (j * 16)), k = 0;
			     i < (page_prog_start + (j * 16)) + 16; i++, k++)

				buring_data[k + ADDR_LEN_4] = FW_content[i - start_addr];
			/*I("FW_content[%d] = 0x%02X", i - start_addr, FW_content[i - start_addr]);*/

			if (himax_bus_write(ts, addr_AHB_address_byte_0, buring_data,
					    ADDR_LEN_4 + 16, HIMAX_I2C_RETRY_TIMES) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return;
			}

			/* ===Polling SPI Status Active ===*/
			retry_cnt = 0;
			do {
				himax_mcu_register_read(
					ts, ts->ic_incell.pflash_op->addr_spi200_rst_status,
					DATA_LEN_4, tmp_data, 0);

				if (retry_cnt > 50) {
					E("%s: Polling FAIL", __func__);
					return;
				}
				retry_cnt++;
			} while ((tmp_data[2] & 0x40) == 0);
		}

		if (!ts->core_fp.fp_wait_wip(ts, 1))
			E("%s:Flash_Programming Fail\n", __func__);
	}
}

static bool hx83192_mcu_ic_id_read(struct himax_ts_data *ts)
{
	I("%s: [HX83192-A]", __func__);
	return true;
}

static bool hx83192_mcu_dd_clk_set(struct himax_ts_data *ts, bool enable)
{
	uint8_t data[4] = { 0 };

	data[0] = (enable) ? 0xDD : 0x00;
	return (himax_mcu_register_write(ts, ts->ic_incell.pfw_op->addr_osc_en,
					 sizeof(ts->ic_incell.pfw_op->addr_osc_en), data,
					 0) == NO_ERR);
}

static void hx83192_mcu_dd_reg_en(struct himax_ts_data *ts, bool enable)
{
	uint8_t data[4] = { 0 };

	data[0] = 0xA5;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	himax_mcu_register_write(ts, ts->ic_incell.pfw_op->addr_osc_pw,
				 sizeof(ts->ic_incell.pfw_op->addr_osc_pw), data, 0);
	data[0] = 0x00;
	data[1] = 0x55;
	data[2] = 0x66;
	data[3] = 0xCC;

	ts->core_fp.fp_dd_reg_write(ts, 0xEB, 0, 4, data, 0);
	data[0] = 0x00;
	data[1] = 0x83;
	data[2] = 0x19;
	data[3] = 0x2A;
	ts->core_fp.fp_dd_reg_write(ts, 0xB9, 0, 4, data, 0);
}

static void hx83192_func_re_init(struct himax_ts_data *ts)
{
	ts->core_fp.fp_sense_on = hx83192_sense_on;
	ts->core_fp.fp_sense_off = hx83192_sense_off;
	ts->core_fp.fp_chip_init = hx83192_chip_init;
	ts->core_fp.fp_ic_id_read = hx83192_mcu_ic_id_read;
	ts->core_fp.fp_dd_clk_set = hx83192_mcu_dd_clk_set;
	ts->core_fp.fp_dd_reg_en = hx83192_mcu_dd_reg_en;

	ts->core_fp.fp_flash_dump_func = hx83192_mcu_flash_dump_func;
	ts->core_fp.fp_flash_programming = hx83192_mcu_flash_programming;
}

bool hx83192_chip_detect(struct himax_ts_data *ts)
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

	hx83192_func_re_init(ts);

	ts->core_fp.fp_sense_off(ts, true);

	for (i = 0; i < 5; i++) {
		himax_parse_assign_cmd(addr_icid_addr, tmp_addr, sizeof(tmp_addr));
		himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, false);
		I("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3], tmp_data[2],
		  tmp_data[1]);

		if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x19) && (tmp_data[1] == 0x2a)) {
			strscpy(ts->chip_name, HX_83192A_SERIES_PWON, 30);
			I("%s:IC name = %s\n", __func__, ts->chip_name);
			I("Himax IC package %x%x%x in\n", tmp_data[3], tmp_data[2], tmp_data[1]);

			ret_data = true;
			ts->ic_data->ic_adc_num = hx83192_data_adc_num;
			himax_get_ic_Amount(ts);
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
EXPORT_SYMBOL(hx83192_chip_detect);

MODULE_LICENSE("GPL");
