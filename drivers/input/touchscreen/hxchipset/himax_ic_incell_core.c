/* Himax Android Driver Sample Code for incell ic core functions
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
#include "himax_ic_core.h"

uint32_t himax_dbg_reg_ary[4] = { addr_fw_dbg_msg_addr, addr_cs_central_state, addr_chk_dd_status,
			    addr_flag_reset_event };

static void himax_mcu_burst_enable(struct himax_ts_data *ts, uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[DATA_LEN_4];
	int ret;

	/*I("%s,Entering\n", __func__);*/

	tmp_data[0] = (para_AHB_INC4 | auto_add_4_byte);

	ret = himax_bus_write(ts, addr_AHB_INC4, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

int himax_mcu_register_read(struct himax_ts_data *ts, uint8_t *read_addr, uint32_t read_length,
			    uint8_t *read_data, uint8_t cfg_flag)
{
	uint8_t tmp_data[DATA_LEN_4];
	int i = 0;
	int address = 0;
	int ret = 0;

	/*I("%s,Entering\n",__func__);*/

	if (cfg_flag == false) {
		if (read_length > FLASH_RW_MAX_LEN) {
			E("%s: read len over %d!\n", __func__, FLASH_RW_MAX_LEN);
			return LENGTH_FAIL;
		}

		if (read_length > DATA_LEN_4)
			ts->core_fp.fp_burst_enable(ts, 1);
		else
			ts->core_fp.fp_burst_enable(ts, 0);

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

		if (read_length > DATA_LEN_4)
			ts->core_fp.fp_burst_enable(ts, 0);

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
EXPORT_SYMBOL_GPL(himax_mcu_register_read);

static int himax_mcu_flash_write_burst_lenth(struct himax_ts_data *ts, uint8_t *reg_byte,
					     uint8_t *write_data, uint32_t length)
{
	uint8_t *data_byte;
	int ret = 0;

	if (!ts->internal_buffer) {
		E("%s: internal buffer not initialized!\n", __func__);
		return MEM_ALLOC_FAIL;
	}
	data_byte = ts->internal_buffer;

	/* assign addr 4bytes */
	memcpy(data_byte, reg_byte, ADDR_LEN_4);
	/* assign data n bytes */
	memcpy(data_byte + ADDR_LEN_4, write_data, length);

	ret = himax_bus_write(ts, addr_AHB_address_byte_0, data_byte, length + ADDR_LEN_4,
			      HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: xfer fail!\n", __func__);
		return I2C_FAIL;
	}

	return NO_ERR;
}

int himax_mcu_register_write(struct himax_ts_data *ts, uint8_t *write_addr, uint32_t write_length,
			     uint8_t *write_data, uint8_t cfg_flag)
{
	int address;
	uint8_t tmp_addr[4];
	uint8_t *tmp_data;
	int total_read_times = 0;
	uint32_t max_bus_size = MAX_I2C_TRANS_SZ;
	uint32_t total_size_temp = 0;
	unsigned int i = 0;
	int ret = 0;

	/*I("%s,Entering\n", __func__);*/
	if (cfg_flag == 0) {
		total_size_temp = write_length;

		tmp_addr[3] = write_addr[3];
		tmp_addr[2] = write_addr[2];
		tmp_addr[1] = write_addr[1];
		tmp_addr[0] = write_addr[0];

		if (total_size_temp % max_bus_size == 0)
			total_read_times = total_size_temp / max_bus_size;
		else
			total_read_times = total_size_temp / max_bus_size + 1;

		if (write_length > DATA_LEN_4)
			ts->core_fp.fp_burst_enable(ts, 1);
		else
			ts->core_fp.fp_burst_enable(ts, 0);

		for (i = 0; i < (total_read_times); i++) {
			/* I("[log]write %d time start!\n", i);
			 * I("[log]addr[3]=0x%02X, addr[2]=0x%02X,
				addr[1]=0x%02X,	addr[0]=0x%02X!\n",
				tmp_addr[3], tmp_addr[2],
				tmp_addr[1], tmp_addr[0]);
			 * I("%s, write addr = 0x%02X%02X%02X%02X\n",
				__func__, tmp_addr[3], tmp_addr[2],
				tmp_addr[1], tmp_addr[0]);
			 */

			if (total_size_temp >= max_bus_size) {
				tmp_data = write_data + (i * max_bus_size);

				ret = himax_mcu_flash_write_burst_lenth(ts, tmp_addr, tmp_data,
									max_bus_size);
				if (ret < 0) {
					I("%s: i2c access fail!\n", __func__);
					return I2C_FAIL;
				}
				total_size_temp = total_size_temp - max_bus_size;
			} else {
				tmp_data = write_data + (i * max_bus_size);
				/* I("last total_size_temp=%d\n",
				 *	total_size_temp % max_bus_size);
				 */
				ret = himax_mcu_flash_write_burst_lenth(ts, tmp_addr, tmp_data,
									total_size_temp);
				if (ret < 0) {
					I("%s: i2c access fail!\n", __func__);
					return I2C_FAIL;
				}
			}

			/*I("[log]write %d time end!\n", i);*/
			address = ((i + 1) * max_bus_size);
			tmp_addr[0] = write_addr[0] + (uint8_t)((address)&0x00FF);

			if (tmp_addr[0] < write_addr[0])
				tmp_addr[1] =
					write_addr[1] + (uint8_t)((address >> 8) & 0x00FF) + 1;
			else
				tmp_addr[1] = write_addr[1] + (uint8_t)((address >> 8) & 0x00FF);

			udelay(100);
		}
	} else if (cfg_flag == 1) {
		ret = himax_bus_write(ts, write_addr[0], write_data, write_length,
				      HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}
	} else
		E("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);

	return NO_ERR;
}
EXPORT_SYMBOL_GPL(himax_mcu_register_write);

static int himax_write_read_reg(struct himax_ts_data *ts, uint8_t *tmp_addr, uint8_t *tmp_data,
				uint8_t hb, uint8_t lb)
{
	int cnt = 0;

	do {
		himax_mcu_register_write(ts, tmp_addr, DATA_LEN_4, tmp_data, 0);
		usleep_range(10000, 11000);
		himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, 0);
		/* I("%s:Now tmp_data[0]=0x%02X,[1]=0x%02X,
		 *	[2]=0x%02X,[3]=0x%02X\n",
		 *	__func__, tmp_data[0],
		 *	tmp_data[1], tmp_data[2], tmp_data[3]);
		 */
	} while ((tmp_data[1] != hb && tmp_data[0] != lb) && cnt++ < 100);

	if (cnt >= 100)
		return HX_RW_REG_FAIL;

	I("Now register 0x%08X : high byte=0x%02X,low byte=0x%02X\n", tmp_addr[3], tmp_data[1],
	  tmp_data[0]);
	return NO_ERR;
}

static void himax_mcu_interface_on(struct himax_ts_data *ts)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_data2[DATA_LEN_4];
	int cnt = 0;
	int ret = 0;

	/* Read a dummy register to wake up I2C.*/
	ret = himax_bus_read(ts, addr_AHB_rdata_byte_0, tmp_data, DATA_LEN_4,
			     HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) { /* to knock I2C*/
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	do {
		tmp_data[0] = para_AHB_continous;

		ret = himax_bus_write(ts, addr_AHB_continous, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		tmp_data[0] = para_AHB_INC4;

		ret = himax_bus_write(ts, addr_AHB_INC4, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/*Check cmd*/
		himax_bus_read(ts, addr_AHB_continous, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		himax_bus_read(ts, addr_AHB_INC4, tmp_data2, 1, HIMAX_I2C_RETRY_TIMES);

		if (tmp_data[0] == para_AHB_continous && tmp_data2[0] == para_AHB_INC4)
			break;

		usleep_range(1000, 1100);
	} while (++cnt < 10);

	if (cnt > 0)
		I("%s:Polling burst mode: %d times\n", __func__, cnt);
}

#define WIP_PRT_LOG "%s: retry:%d, bf[0]=%d, bf[1]=%d,bf[2]=%d, bf[3]=%d\n"
static bool himax_mcu_wait_wip(struct himax_ts_data *ts, int Timing)
{
	uint8_t tmp_data[DATA_LEN_4];
	int retry_cnt = 0;

	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_fmt, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_trans_fmt, 0);
	tmp_data[0] = 0x01;

	do {
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_ctrl,
					 DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_trans_ctrl_1, 0);

		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_cmd_1, 0);
		tmp_data[0] = tmp_data[1] = tmp_data[2] = tmp_data[3] = 0xFF;
		himax_mcu_register_read(ts, ts->ic_incell.pflash_op->addr_spi200_data, DATA_LEN_4,
					tmp_data, 0);

		if ((tmp_data[0] & 0x01) == 0x00)
			return true;

		retry_cnt++;

		if (tmp_data[0] != 0x00 || tmp_data[1] != 0x00 || tmp_data[2] != 0x00 ||
		    tmp_data[3] != 0x00)
			I(WIP_PRT_LOG, __func__, retry_cnt, tmp_data[0], tmp_data[1], tmp_data[2],
			  tmp_data[3]);

		if (retry_cnt > 100) {
			E("%s: Wait wip error!\n", __func__);
			return false;
		}

		msleep(Timing);
	} while ((tmp_data[0] & 0x01) == 0x01);

	return true;
}

static void himax_mcu_sense_on(struct himax_ts_data *ts, uint8_t FlashMode)
{
	/*Overwrite*/
	return;
}

static bool himax_mcu_sense_off(struct himax_ts_data *ts, bool check_en)
{
	/*Overwrite*/
	return true;
}

/*power saving level*/
static void himax_mcu_init_psl(struct himax_ts_data *ts)
{
	uint8_t addr[DATA_LEN_4] = { 0 };
	uint8_t data[DATA_LEN_4] = { 0 };
	himax_parse_assign_cmd(addr_psl, addr, sizeof(addr));
	himax_parse_assign_cmd(data_clear, data, sizeof(data));
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
	I("%s: power saving level reset OK!\n", __func__);
}

static void himax_mcu_resume_ic_action(struct himax_ts_data *ts)
{
	/* Nothing to do */
}

static void himax_mcu_suspend_ic_action(struct himax_ts_data *ts)
{
	/* Nothing to do */
}

static void himax_mcu_power_on_init(struct himax_ts_data *ts)
{
	uint8_t addr[DATA_LEN_4] = { 0 };
	uint8_t data[DATA_LEN_4] = { 0 };
	himax_parse_assign_cmd(data_clear, data, sizeof(data));

	ts->core_fp.fp_touch_information(ts);
	/*RawOut select initial*/
	himax_parse_assign_cmd(addr_raw_out_sel, addr, sizeof(addr));
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
	/*DSRAM func initial*/
	ts->core_fp.fp_assign_sorting_mode(ts, data);
	/* N frame initial :　reset N frame back to default value 1 for normal mode */
	himax_parse_assign_cmd(addr_set_frame_addr, addr, sizeof(addr));
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
	/*FW reload done initial*/
	himax_parse_assign_cmd(addr_fw_define_2nd_flash_reload, addr, sizeof(addr));
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
	ts->core_fp.fp_sense_on(ts, 0x00);

	I("%s: waiting for FW reload data\n", __func__);
}

static bool himax_mcu_dd_clk_set(struct himax_ts_data *ts, bool enable)
{
	/*Overwrite*/
	return true;
}

static void himax_mcu_dd_reg_en(struct himax_ts_data *ts, bool enable)
{
	/*Overwrite*/
	return;
}

static bool himax_mcu_dd_reg_write(struct himax_ts_data *ts, uint8_t addr, uint8_t pa_num, int len,
				   uint8_t *data, uint8_t bank)
{
	/*Calculate total write length*/
	uint32_t data_len = (((len + pa_num - 1) / 4 - pa_num / 4) + 1) * 4;
	uint8_t *w_data;
	uint8_t tmp_addr[4] = { 0 };
	uint8_t tmp_data[4] = { 0 };
	bool *chk_data;
	uint32_t chk_idx = 0;
	int i = 0;
	int ret = 0;

	w_data = kmalloc(sizeof(uint8_t) * data_len, GFP_KERNEL);
	if (w_data == NULL) {
		return false;
	}

	chk_data = kcalloc(data_len, sizeof(bool), GFP_KERNEL);
	if (chk_data == NULL) {
		E("%s Allocate chk buf failed\n", __func__);
		kfree(w_data);
		return false;
	}

	memset(w_data, 0, data_len * sizeof(uint8_t));

	/*put input data*/
	chk_idx = pa_num % 4;
	for (i = 0; i < len; i++) {
		w_data[chk_idx] = data[i];
		chk_data[chk_idx++] = true;
	}

	/*get original data*/
	chk_idx = (pa_num / 4) * 4;
	for (i = 0; i < data_len; i++) {
		if (!chk_data[i]) {
			ts->core_fp.fp_dd_reg_read(ts, addr, (uint8_t)(chk_idx + i), 1, tmp_data,
						   bank);

			w_data[i] = tmp_data[0];
			chk_data[i] = true;
		}
		D("%s w_data[%d] = %2X\n", __func__, i, w_data[i]);
	}

	tmp_addr[3] = 0x30;
	tmp_addr[2] = addr >> 4;
	tmp_addr[1] = (addr << 4) | (bank * 4);
	tmp_addr[0] = chk_idx;
	D("%s Addr = %02X%02X%02X%02X.\n", __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1],
	  tmp_addr[0]);
	kfree(chk_data);

	ret = (himax_mcu_register_write(ts, tmp_addr, data_len, w_data, 0) == NO_ERR);
	kfree(w_data);
	return ret ? false : true;
}

static bool himax_mcu_dd_reg_read(struct himax_ts_data *ts, uint8_t addr, uint8_t pa_num, int len,
				  uint8_t *data, uint8_t bank)
{
	uint8_t tmp_addr[4] = { 0 };
	uint8_t tmp_data[4] = { 0 };
	int i = 0;

	for (i = 0; i < len; i++) {
		tmp_addr[3] = 0x30;
		tmp_addr[2] = addr >> 4;
		tmp_addr[1] = (addr << 4) | (bank * 4);
		tmp_addr[0] = pa_num + i;

		if (himax_mcu_register_read(ts, tmp_addr, DATA_LEN_4, tmp_data, 0))
			goto READ_FAIL;

		data[i] = tmp_data[0];

		D("%s Addr = %02X%02X%02X%02X.data = %2X\n", __func__, tmp_addr[3], tmp_addr[2],
		  tmp_addr[1], tmp_addr[0], data[i]);
	}
	return true;

READ_FAIL:
	E("%s Read DD reg Failed.\n", __func__);
	return false;
}

/* CORE_FW */
/* FW side start*/
static void diag_mcu_parse_raw_data(struct himax_ts_data *ts,
				    struct himax_report_data *hx_touch_data, int mul_num,
				    int self_num, uint8_t diag_cmd, int32_t *mutual_data,
				    int32_t *self_data)
{
	int RawDataLen_word;
	int index = 0;
	int temp1, temp2, i;

	if (hx_touch_data->hx_rawdata_buf[0] == ts->ic_incell.pfw_op->data_rawdata_ready_lb[0] &&
	    hx_touch_data->hx_rawdata_buf[1] == ts->ic_incell.pfw_op->data_rawdata_ready_hb[0] &&
	    hx_touch_data->hx_rawdata_buf[2] > 0 && hx_touch_data->hx_rawdata_buf[3] == diag_cmd) {
		RawDataLen_word = hx_touch_data->rawdata_size / 2;
		index = (hx_touch_data->hx_rawdata_buf[2] - 1) * RawDataLen_word;

		/* I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n",index,
		 *	buf[56], buf[57], buf[58], buf[59], mul_num, self_num);
		 * I("RawDataLen=%d , RawDataLen_word=%d ,
		 *	hx_touch_info_size=%d\n",
		 *	RawDataLen, RawDataLen_word, hx_touch_info_size);
		 */
		for (i = 0; i < RawDataLen_word; i++) {
			temp1 = index + i;

			if (temp1 < mul_num) { /*mutual*/
				mutual_data[index + i] =
					((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 + 4 + 1]) *
						256 +
					hx_touch_data->hx_rawdata_buf[i * 2 + 4];
			} else { /*self*/
				temp1 = i + index;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2)
					break;

				self_data[i + index - mul_num] =
					(((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 + 4 + 1])
					 << 8) +
					hx_touch_data->hx_rawdata_buf[i * 2 + 4];
			}
		}
	}
}

/*-------------------------------------------------------------------------
*
*	Create: Unknown
*
*	Using: Do software reset by setting addr 0x90000018 register with value
*		   0x55, then read dummy 4 bytes.
*
*	param: None
*
*	Dependency function: None
*
*/
static void himax_mcu_system_reset(struct himax_ts_data *ts)
{
	uint8_t addr[DATA_LEN_4] = { 0 };
	uint8_t data[DATA_LEN_4] = { 0 };
#if defined(HIMAX_I2C_PLATFORM)
	int ret = 0;
#endif
	himax_parse_assign_cmd(addr_system_reset, addr, sizeof(addr));
	himax_parse_assign_cmd(data_system_reset, data, sizeof(data));
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	msleep(280);

#if defined(HIMAX_I2C_PLATFORM)
	ret = himax_bus_read(ts, addr_AHB_rdata_byte_0, data, DATA_LEN_4, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
	}
#endif
}

static int himax_mcu_Calculate_CRC_with_AP(struct himax_ts_data *ts, unsigned char *FW_content,
					   int CRC_from_FW, int len)
{
	int i, j, length = 0;
	int fw_data;
	int fw_data_2;
	int CRC = 0xFFFFFFFF;
	int PolyNomial = 0x82F63B78;

	length = len / 4;

	for (i = 0; i < length; i++) {
		fw_data = FW_content[i * 4];

		for (j = 1; j < 4; j++) {
			fw_data_2 = FW_content[i * 4 + j];
			fw_data += (fw_data_2) << (8 * j);
		}
		CRC = fw_data ^ CRC;
		for (j = 0; j < 32; j++) {
			if ((CRC % 2) != 0)
				CRC = ((CRC >> 1) & 0x7FFFFFFF) ^ PolyNomial;
			else
				CRC = (((CRC >> 1) & 0x7FFFFFFF));
		}
	}

	return CRC;
}

static uint32_t himax_mcu_check_CRC(struct himax_ts_data *ts, uint8_t *start_addr,
				    int reload_length)
{
	uint32_t result = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int cnt = 0, ret = 0;
	int length = reload_length / DATA_LEN_4;

	ret = himax_mcu_register_write(ts, ts->ic_incell.pfw_op->addr_reload_addr_from, DATA_LEN_4,
				       start_addr, 0);
	if (ret < NO_ERR) {
		E("%s: i2c access fail!\n", __func__);
		return HW_CRC_FAIL;
	}

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x99;
	tmp_data[1] = (length >> 8);
	tmp_data[0] = length;
	ret = himax_mcu_register_write(ts, ts->ic_incell.pfw_op->addr_reload_addr_cmd_beat,
				       DATA_LEN_4, tmp_data, 0);
	if (ret < NO_ERR) {
		E("%s: i2c access fail!\n", __func__);
		return HW_CRC_FAIL;
	}
	cnt = 0;

	do {
		ret = himax_mcu_register_read(ts, ts->ic_incell.pfw_op->addr_reload_status,
					      DATA_LEN_4, tmp_data, 0);
		if (ret < NO_ERR) {
			E("%s: i2c access fail!\n", __func__);
			return HW_CRC_FAIL;
		}

		if ((tmp_data[0] & 0x01) != 0x01) {
			ret = himax_mcu_register_read(
				ts, ts->ic_incell.pfw_op->addr_reload_crc32_result, DATA_LEN_4,
				tmp_data, 0);
			if (ret < NO_ERR) {
				E("%s: i2c access fail!\n", __func__);
				return HW_CRC_FAIL;
			}
			I("%s:data[3]=%X,data[2]=%X,data[1]=%X,data[0]=%X\n", __func__, tmp_data[3],
			  tmp_data[2], tmp_data[1], tmp_data[0]);
			result = ((tmp_data[3] << 24) + (tmp_data[2] << 16) + (tmp_data[1] << 8) +
				  tmp_data[0]);
			goto END;
		} else {
			I("Waiting for HW ready!\n");
			usleep_range(1000, 1100);
			if (cnt >= 100)
				ts->core_fp.fp_read_FW_status(ts);
		}

	} while (cnt++ < 100);
END:
	return result;
}

#define PRT_DATA "%s:[3]=0x%2X, [2]=0x%2X, [1]=0x%2X, [0]=0x%2X\n"
static void himax_mcu_diag_register_set(struct himax_ts_data *ts, uint8_t diag_command,
					uint8_t storage_type, bool is_dirly)
{
	uint8_t addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t cnt = 50;

	if (diag_command > 0 && storage_type % 8 > 0 && !is_dirly)
		tmp_data[0] = diag_command + 0x08;
	else
		tmp_data[0] = diag_command;
	I("diag_command = %d, tmp_data[0] = %X\n", diag_command, tmp_data[0]);
	ts->core_fp.fp_interface_on(ts);
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	himax_parse_assign_cmd(addr_raw_out_sel, addr, sizeof(addr));
	do {
		himax_mcu_register_write(ts, addr, DATA_LEN_4, tmp_data, 0);
		himax_mcu_register_read(ts, addr, DATA_LEN_4, back_data, 0);
		I(PRT_DATA, __func__, back_data[3], back_data[2], back_data[1], back_data[0]);
		cnt--;
	} while (tmp_data[0] != back_data[0] && cnt > 0);
}

static int himax_mcu_chip_self_test(struct himax_ts_data *ts, struct seq_file *s, void *v)
{
	/*Overwrite*/
	return 0x00;
}

#define PRT_TMP_DATA "%s:[0]=0x%2X,[1]=0x%2X,	[2]=0x%2X,[3]=0x%2X\n"
static void himax_mcu_reload_disable(struct himax_ts_data *ts, int disable)
{
	uint8_t addr[DATA_LEN_4] = { 0 };
	uint8_t data[DATA_LEN_4] = { 0 };

	I("%s:entering\n", __func__);
	himax_parse_assign_cmd(addr_fw_define_flash_reload, addr, sizeof(addr));

	if (disable) { /*reload disable*/
		himax_parse_assign_cmd(data_fw_define_flash_reload_dis, data, sizeof(data));
		himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
	} else { /*reload enable*/
		himax_parse_assign_cmd(data_fw_define_flash_reload_en, data, sizeof(data));
		himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
	}

	I("%s: setting OK!\n", __func__);
}

static int himax_mcu_read_ic_trigger_type(struct himax_ts_data *ts)
{
	uint8_t addr[DATA_LEN_4] = { 0 };
	uint8_t data[DATA_LEN_4] = { 0 };
	int trigger_type = false;
	himax_parse_assign_cmd(addr_fw_define_int_is_edge, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);

	if ((data[1] & 0x01) == 1)
		trigger_type = true;

	return trigger_type;
}

static void himax_mcu_read_FW_ver(struct himax_ts_data *ts)
{
	uint8_t addr[DATA_LEN_4] = { 0 };
	uint8_t data[12] = { 0 };
	uint8_t data_2[DATA_LEN_4] = { 0 };
	uint8_t retry = 0;
	uint8_t reload_status = 0;

	I("%s: waiting for FW reload done\n", __func__);

	while (reload_status == 0) {
		himax_parse_assign_cmd(addr_fw_define_flash_reload, addr, sizeof(addr));
		himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);
		himax_parse_assign_cmd(addr_fw_define_2nd_flash_reload, addr, sizeof(addr));
		himax_mcu_register_read(ts, addr, DATA_LEN_4, data_2, 0);

		if ((data[2] == 0x9A && data[3] == 0xA9) ||
		    (data_2[1] == 0x72 && data_2[0] == 0xC0)) {
			I("%s: FW finish reload done\n", __func__);
			reload_status = 1;
			break;
		} else if (retry == 200) {
			E("%s: FW fail reload done !!!!!\n", __func__);
			ts->core_fp.fp_read_FW_status(ts);
			ts->ic_data->vendor_panel_ver = 0;
			ts->ic_data->vendor_arch_ver = 0;
			ts->ic_data->vendor_config_ver = 0;
			ts->ic_data->vendor_touch_cfg_ver = 0;
			ts->ic_data->vendor_display_cfg_ver = 0;
			ts->ic_data->vendor_cid_maj_ver = 0;
			ts->ic_data->vendor_cid_min_ver = 0;
			goto END;
		} else {
			retry++;
			usleep_range(10000, 11000);
			I("%s: wait reload done %d times\n", __func__, retry);
		}
	}

	I("%s:data[2]=0x%2.2X,data[3]=0x%2.2X\n", __func__, data[2], data[3]);
	I("%s:data_2[0]=0x%2.2X,data_2[1]=0x%2.2X,reload_status=%d\n", __func__, data_2[0],
	  data_2[1], reload_status);
	/**
	 * Read FW version
	 */
	himax_parse_assign_cmd(addr_fw_architecture_version, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);
	ts->ic_data->vendor_panel_ver = data[0];
	ts->ic_data->vendor_arch_ver = data[1] << 8 | data[2];

	himax_parse_assign_cmd(addr_fw_config_version, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);
	ts->ic_data->vendor_config_ver = data[2] << 8 | data[3];
	/*I("CFG_VER : %X\n",ts->ic_data->vendor_config_ver);*/
	ts->ic_data->vendor_touch_cfg_ver = data[2];
	ts->ic_data->vendor_display_cfg_ver = data[3];

	himax_parse_assign_cmd(addr_fw_CID, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);
	ts->ic_data->vendor_cid_maj_ver = data[2];
	ts->ic_data->vendor_cid_min_ver = data[3];

	himax_parse_assign_cmd(addr_fw_customer, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, 12, data, 0);
	memcpy(ts->ic_data->vendor_cus_info, data, 12);

	himax_parse_assign_cmd(addr_fw_project_name, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, 12, data, 0);
	memcpy(ts->ic_data->vendor_proj_info, data, 12);

	himax_parse_assign_cmd(addr_fw_config_date, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, 12, data, 0);
	memcpy(ts->ic_data->vendor_config_date, data, 12);

	I("Architecture Version : %X\n", ts->ic_data->vendor_arch_ver);
	I("CID : %04X\n", (ts->ic_data->vendor_cid_maj_ver << 8 | ts->ic_data->vendor_cid_min_ver));
	I("FW Display Config. Version : %X\n", ts->ic_data->vendor_display_cfg_ver);
	I("FW Touch Config. Version : %X\n", ts->ic_data->vendor_touch_cfg_ver);
	I("Panel Version : %X\n", ts->ic_data->vendor_panel_ver);
	I("Config. Date = %s\n", ts->ic_data->vendor_config_date);
	I("Project Name = %s\n", ts->ic_data->vendor_proj_info);
	I("Cusomer = %s\n", ts->ic_data->vendor_cus_info);

END:
	return;
}

static bool himax_mcu_read_event_stack(struct himax_ts_data *ts, uint8_t *buf, uint8_t length)
{
	struct timespec64 t_start, t_end, t_delta;
	int len = length;
	int i2c_speed = 0;

	if (ts->debug_log_level & BIT(2)) {
		ktime_get_real_ts64(&t_start);

		himax_bus_read(ts, addr_read_event_stack, buf, length, HIMAX_I2C_RETRY_TIMES);

		ktime_get_real_ts64(&t_end);
		t_delta.tv_nsec = (t_end.tv_sec * 1000000000 + t_end.tv_nsec) -
				  (t_start.tv_sec * 1000000000 + t_start.tv_nsec);

		i2c_speed = (len * 9 * 1000000 / (int)t_delta.tv_nsec) * 13 / 10;
		ts->bus_speed = (int)i2c_speed;
	} else {
		himax_bus_read(ts, addr_read_event_stack, buf, length, HIMAX_I2C_RETRY_TIMES);
	}

	return 1;
}

static void himax_mcu_return_event_stack(struct himax_ts_data *ts)
{
	int retry = 20, i;
	uint8_t tmp_data[DATA_LEN_4];

	I("%s:entering\n", __func__);

	do {
		I("now %d times!\n", retry);

		for (i = 0; i < DATA_LEN_4; i++)
			tmp_data[i] = ts->psram_op->addr_rawdata_end[i];

		himax_mcu_register_write(ts, ts->psram_op->addr_rawdata_addr, DATA_LEN_4, tmp_data,
					 0);
		himax_mcu_register_read(ts, ts->psram_op->addr_rawdata_addr, DATA_LEN_4, tmp_data,
					0);
		retry--;
		usleep_range(10000, 11000);
	} while ((tmp_data[1] != ts->psram_op->addr_rawdata_end[1] &&
		  tmp_data[0] != ts->psram_op->addr_rawdata_end[0]) &&
		 retry > 0);

	I("%s: End of setting!\n", __func__);
}

static bool himax_mcu_calculateChecksum(struct himax_ts_data *ts, bool change_iref, uint32_t size)
{
	uint8_t CRC_result = 0, i;
	uint8_t tmp_data[DATA_LEN_4];

	I("%s:Now size=%d\n", __func__, size);
	for (i = 0; i < DATA_LEN_4; i++)
		tmp_data[i] = ts->psram_op->addr_rawdata_end[i];

	CRC_result = ts->core_fp.fp_check_CRC(ts, tmp_data, size);
	msleep(50);

	if (CRC_result != 0)
		I("%s: CRC Fail=%d\n", __func__, CRC_result);

	return (CRC_result == 0) ? true : false;
}

static void himax_mcu_read_FW_status(struct himax_ts_data *ts)
{
	uint8_t len = 0;
	uint8_t i = 0;
	uint8_t addr[4] = { 0 };
	uint8_t data[4] = { 0 };

	len = (uint8_t)(sizeof(himax_dbg_reg_ary) / sizeof(uint32_t));

	for (i = 0; i < len; i++) {
		himax_parse_assign_cmd(himax_dbg_reg_ary[i], addr, 4);
		himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);

		I("reg[0-3] : 0x%08X = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", himax_dbg_reg_ary[i], data[0],
		  data[1], data[2], data[3]);
	}
}

static void himax_mcu_irq_switch(struct himax_ts_data *ts, int switch_on)
{
	if (switch_on) {
		if (ts->use_irq)
			himax_int_enable(ts, switch_on);
		else
			hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	} else {
		if (ts->use_irq)
			himax_int_enable(ts, switch_on);
		else {
			hrtimer_cancel(&ts->timer);
			cancel_work_sync(&ts->work);
		}
	}
}

static int himax_mcu_assign_sorting_mode(struct himax_ts_data *ts, uint8_t *tmp_data)
{
	uint8_t addr[DATA_LEN_4] = { 0 };
	himax_parse_assign_cmd(addr_sorting_mode_en, addr, sizeof(addr));
	I("%s:Now data[3]=0x%2X,data[2]=0x%2X,data[1]=0x%2X,data[0]=0x%2X\n", __func__, tmp_data[3],
	  tmp_data[2], tmp_data[1], tmp_data[0]);

	himax_mcu_register_write(ts, addr, DATA_LEN_4, tmp_data, 0);

	return NO_ERR;
}

static int himax_mcu_check_sorting_mode(struct himax_ts_data *ts, uint8_t *tmp_data)
{
	uint8_t addr[DATA_LEN_4] = { 0 };
	himax_parse_assign_cmd(addr_sorting_mode_en, addr, sizeof(addr));

	himax_mcu_register_read(ts, addr, DATA_LEN_4, tmp_data, 0);
	//I("%s: tmp_data[0]=%x,tmp_data[1]=%x\n", __func__,
	//		tmp_data[0], tmp_data[1]);

	return NO_ERR;
}

/* FW side end*/
/* CORE_FW */

/* CORE_FLASH */
/* FLASH side start*/
static void himax_mcu_chip_erase(struct himax_ts_data *ts)
{
	ts->core_fp.fp_interface_on(ts);

	/* Reset power saving level */
	if (ts->core_fp.fp_init_psl != NULL)
		ts->core_fp.fp_init_psl(ts);

	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_fmt, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_trans_fmt, 0);

	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_ctrl, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_trans_ctrl_2, 0);
	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_cmd_2, 0);

	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_cmd_3, 0);
	msleep(2000);

	if (!ts->core_fp.fp_wait_wip(ts, 100))
		E("%s: Chip_Erase Fail\n", __func__);
}

static bool himax_mcu_block_erase(struct himax_ts_data *ts, int start_addr, int length)
{
	uint32_t page_prog_start = 0;
	uint32_t block_size = 0x10000;

	uint8_t tmp_data[4] = { 0 };

	ts->core_fp.fp_interface_on(ts);

	ts->core_fp.fp_init_psl(ts);

	himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_fmt, DATA_LEN_4,
				 ts->ic_incell.pflash_op->data_spi200_trans_fmt, 0);

	for (page_prog_start = start_addr; page_prog_start < start_addr + length;
	     page_prog_start = page_prog_start + block_size) {
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_ctrl,
					 DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_trans_ctrl_2, 0);
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_cmd_2, 0);

		tmp_data[3] = (page_prog_start >> 24) & 0xFF;
		tmp_data[2] = (page_prog_start >> 16) & 0xFF;
		tmp_data[1] = (page_prog_start >> 8) & 0xFF;
		tmp_data[0] = page_prog_start & 0xFF;
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_addr, DATA_LEN_4,
					 tmp_data, 0);

		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_trans_ctrl,
					 DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_trans_ctrl_3, 0);
		himax_mcu_register_write(ts, ts->ic_incell.pflash_op->addr_spi200_cmd, DATA_LEN_4,
					 ts->ic_incell.pflash_op->data_spi200_cmd_4, 0);
		msleep(1000);

		if (!ts->core_fp.fp_wait_wip(ts, 100)) {
			E("%s:Erase Fail\n", __func__);
			return false;
		}
	}

	I("%s:END\n", __func__);
	return true;
}

static bool himax_mcu_sector_erase(struct himax_ts_data *ts, int start_addr, int length)
{
	uint8_t addr[4];
	uint8_t data[4];
	uint32_t page_prog_start = 0;
	uint32_t sector_size = 0x1000;

	ts->core_fp.fp_init_psl(ts);

	/*=====================================
	 SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	=====================================*/
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x10;
	data[3] = 0x00;
	data[2] = 0x02;
	data[1] = 0x07;
	data[0] = 0x80;

	himax_mcu_register_write(ts, addr, 4, data, 0);

	for (page_prog_start = start_addr; page_prog_start < start_addr + length;
	     page_prog_start = page_prog_start + sector_size) {
		/*=====================================
	Write Enable : 1. 0x8000_0020 ==> 0x4700_0000 //control
				   2. 0x8000_0024 ==> 0x0000_0006 //WREN
	=====================================*/
		addr[3] = 0x80;
		addr[2] = 0x00;
		addr[1] = 0x00;
		addr[0] = 0x20;
		data[3] = 0x47;
		data[2] = 0x00;
		data[1] = 0x00;
		data[0] = 0x00;
		himax_mcu_register_write(ts, addr, 4, data, 0);

		addr[3] = 0x80;
		addr[2] = 0x00;
		addr[1] = 0x00;
		addr[0] = 0x24;
		data[3] = 0x00;
		data[2] = 0x00;
		data[1] = 0x00;
		data[0] = 0x06;
		himax_mcu_register_write(ts, addr, 4, data, 0);

		/*=====================================
	Sector Erase
	Erase Command : 0x8000_0028 ==> 0x0000_0000 //SPI addr
	                0x8000_0020 ==> 0x6700_0000 //control
	                0x8000_0024 ==> 0x0000_0020 //SE
	=====================================*/
		addr[3] = 0x80;
		addr[2] = 0x00;
		addr[1] = 0x00;
		addr[0] = 0x28;
		data[3] = (uint8_t)(page_prog_start >> 24);
		data[2] = (uint8_t)(page_prog_start >> 16);
		data[1] = (uint8_t)(page_prog_start >> 8);
		data[0] = (uint8_t)page_prog_start;
		himax_mcu_register_write(ts, addr, 4, data, 0);

		addr[3] = 0x80;
		addr[2] = 0x00;
		addr[1] = 0x00;
		addr[0] = 0x20;
		data[3] = 0x67;
		data[2] = 0x00;
		data[1] = 0x00;
		data[0] = 0x00;
		himax_mcu_register_write(ts, addr, 4, data, 0);

		addr[3] = 0x80;
		addr[2] = 0x00;
		addr[1] = 0x00;
		addr[0] = 0x24;
		data[3] = 0x00;
		data[2] = 0x00;
		data[1] = 0x00;
		data[0] = 0x20;
		himax_mcu_register_write(ts, addr, 4, data, 0);

		if (!ts->core_fp.fp_wait_wip(ts, 100)) {
			return false;
		}
	}

	I("%s:END\n", __func__);
	return true;
}

static void himax_mcu_flash_programming(struct himax_ts_data *ts, uint8_t *FW_content,
					int start_addr, int FW_Size)
{
	/*Overwrite*/
	return;
}

static void himax_mcu_flash_page_write(struct himax_ts_data *ts, uint8_t *write_addr, int length,
				       uint8_t *write_data)
{
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k(struct himax_ts_data *ts,
							 unsigned char *fw, int len,
							 bool change_iref)
{
	int burnFW_success = 0;

	if (len != FW_SIZE_64k) {
		E("%s: The file size is not 64K bytes\n", __func__);
		return false;
	}

#if defined(HX_RST_PIN_FUNC)
	if (HX_SYSTEM_RESET == 0)
		ts->core_fp.fp_ic_reset(ts, false, false);
	else
		ts->core_fp.fp_system_reset(ts);
#endif
	ts->core_fp.fp_sense_off(ts, true);
	ts->core_fp.fp_block_erase(ts, 0x00, FW_SIZE_64k);
	ts->core_fp.fp_flash_programming(ts, fw, 0, FW_SIZE_64k);

	if (ts->core_fp.fp_check_CRC(ts, ts->ic_incell.pfw_op->addr_program_reload_from,
				     FW_SIZE_64k) == 0)
		burnFW_success = 1;

#if defined(HX_RST_PIN_FUNC)
	if (HX_SYSTEM_RESET == 0)
		ts->core_fp.fp_ic_reset(ts, false, false);
	else
		ts->core_fp.fp_system_reset(ts);
#endif
	return burnFW_success;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k(struct himax_ts_data *ts,
							  unsigned char *fw, int len,
							  bool change_iref)
{
	int burnFW_success = 0;
	int result = 0;

	if (len != FW_SIZE_128k) {
		E("%s: The file size is not 128K bytes\n", __func__);
		return false;
	}

#if defined(HX_RST_PIN_FUNC)
	if (HX_SYSTEM_RESET == 0)
		ts->core_fp.fp_ic_reset(ts, false, false);
	else
		ts->core_fp.fp_system_reset(ts);
#endif
	ts->core_fp.fp_sense_off(ts, true);

	result = himax_mcu_WP_BP_disable(ts);

	if (result != 0) {
		/* WP BP disable fail */
		burnFW_success = -1;

		return burnFW_success;
	}

	ts->core_fp.fp_block_erase(ts, 0x00, FW_SIZE_128k);
	ts->core_fp.fp_flash_programming(ts, fw, 0, FW_SIZE_128k);

	if (ts->core_fp.fp_check_CRC(ts, ts->ic_incell.pfw_op->addr_program_reload_from,
				     FW_SIZE_128k) == 0)
		burnFW_success = 1;

	return burnFW_success;
}

static void himax_mcu_flash_dump_func(struct himax_ts_data *ts, uint8_t local_flash_command,
				      int Flash_Size, uint8_t *flash_buffer)
{
	/*Overwrite*/
	return;
}

static bool himax_mcu_flash_lastdata_check(struct himax_ts_data *ts, uint32_t size)
{
	uint8_t tmp_addr[4];
	/* 64K - 0x80, which is the address of
	 * the last 128bytes in 64K, default value
	 */
	uint32_t start_addr = 0xFFFFFFFF;
	uint32_t temp_addr = 0;
	uint32_t flash_page_len = 0x80;
	uint8_t flash_tmp_buffer[128] = { 0 };

	if (size < flash_page_len) {
		E("%s: flash size is wrong, terminated\n", __func__);
		E("%s: flash size = %08X; flash page len = %08X\n", __func__, size, flash_page_len);
		goto FAIL;
	}

	/* In order to match other size of fw */
	start_addr = size - flash_page_len;
	I("%s: Now size is %d, the start_addr is 0x%08X\n", __func__, size, start_addr);
	for (temp_addr = start_addr; temp_addr < (start_addr + flash_page_len);
	     temp_addr = temp_addr + flash_page_len) {
		/*I("temp_addr=%d,tmp_addr[0]=0x%2X, tmp_addr[1]=0x%2X,
		 *	tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n",
		 *	temp_addr,tmp_addr[0], tmp_addr[1],
		 *	tmp_addr[2],tmp_addr[3]);
		 */
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		himax_mcu_register_read(ts, tmp_addr, flash_page_len, &flash_tmp_buffer[0], 0);
	}

	I("FLASH[%08X] ~ FLASH[%08X] = %02X%02X%02X%02X\n", size - 4, size - 1,
	  flash_tmp_buffer[flash_page_len - 4], flash_tmp_buffer[flash_page_len - 3],
	  flash_tmp_buffer[flash_page_len - 2], flash_tmp_buffer[flash_page_len - 1]);

	if ((!flash_tmp_buffer[flash_page_len - 4]) && (!flash_tmp_buffer[flash_page_len - 3]) &&
	    (!flash_tmp_buffer[flash_page_len - 2]) && (!flash_tmp_buffer[flash_page_len - 1])) {
		I("Fail, Last four Bytes are all 0x00:\n");
		goto FAIL;
	} else if ((flash_tmp_buffer[flash_page_len - 4] == 0xFF) &&
		   (flash_tmp_buffer[flash_page_len - 3] == 0xFF) &&
		   (flash_tmp_buffer[flash_page_len - 2] == 0xFF) &&
		   (flash_tmp_buffer[flash_page_len - 1] == 0xFF)) {
		I("Fail, Last four Bytes are all 0xFF:\n");
		goto FAIL;
	} else {
		return 0;
	}

FAIL:
	return 1;
}

static bool hx_bin_desc_data_get(struct himax_ts_data *ts, uint32_t addr, uint8_t *flash_buf)
{
	uint8_t data_sz = 0x10;
	uint32_t i = 0, j = 0;
	uint16_t chk_end = 0;
	uint16_t chk_sum = 0;
	uint32_t map_code = 0;
	unsigned long flash_addr = 0;

	for (i = 0; i < FW_PAGE_SZ; i = i + data_sz) {
		for (j = i; j < (i + data_sz); j++) {
			chk_end |= flash_buf[j];
			chk_sum += flash_buf[j];
		}
		if (!chk_end) { /*1. Check all zero*/
			I("%s: End in %X\n", __func__, i + addr);
			return false;
		} else if (chk_sum % 0x100) /*2. Check sum*/
			I("%s: chk sum failed in %X\n", __func__, i + addr);
		else { /*3. get data*/
			map_code = flash_buf[i] + (flash_buf[i + 1] << 8) +
				   (flash_buf[i + 2] << 16) + (flash_buf[i + 3] << 24);
			flash_addr = flash_buf[i + 4] + (flash_buf[i + 5] << 8) +
				     (flash_buf[i + 6] << 16) + (flash_buf[i + 7] << 24);
			switch (map_code) {
			case FW_CID:
				ts->cid_ver_maj_flash_addr = flash_addr;
				ts->cid_ver_min_flash_addr = flash_addr + 1;
				I("%s: CID in %lX\n", __func__, ts->cid_ver_maj_flash_addr);
				break;
			case FW_VER:
				ts->fw_ver_maj_flash_addr = flash_addr;
				ts->fw_ver_min_flash_addr = flash_addr + 1;
				I("%s: FW_VER in %lX\n", __func__, ts->fw_ver_maj_flash_addr);
				break;
			case CFG_VER:
				ts->cfg_ver_maj_flash_addr = flash_addr;
				ts->cfg_ver_min_flash_addr = flash_addr + 1;
				I("%s: CFG_VER in = %lX\n", __func__, ts->cfg_ver_maj_flash_addr);
				break;
			case TP_CONFIG_TABLE:
				ts->cfg_table_flash_addr = flash_addr;
				I("%s: CONFIG_TABLE in %X\n", __func__, ts->cfg_table_flash_addr);
				break;
			}
		}
		chk_end = 0;
		chk_sum = 0;
	}

	return true;
}

static bool hx_mcu_bin_desc_get(struct himax_ts_data *ts, unsigned char *fw, uint32_t max_sz)
{
	uint32_t addr_t = 0;
	unsigned char *fw_buf = NULL;
	bool keep_on_flag = false;
	bool g_bin_desc_flag = false;

	do {
		fw_buf = &fw[addr_t];

		/*Check bin is with description table or not*/
		if (!g_bin_desc_flag) {
			if (fw_buf[0x00] == 0x00 && fw_buf[0x01] == 0x00 && fw_buf[0x02] == 0x00 &&
			    fw_buf[0x03] == 0x00 && fw_buf[0x04] == 0x00 && fw_buf[0x05] == 0x00 &&
			    fw_buf[0x06] == 0x00 && fw_buf[0x07] == 0x00 && fw_buf[0x0E] == 0x87)
				g_bin_desc_flag = true;
		}
		if (!g_bin_desc_flag) {
			I("%s: fw_buf[0x00] = %2X, fw_buf[0x0E] = %2X\n", __func__, fw_buf[0x00],
			  fw_buf[0x0E]);
			I("%s: No description table\n", __func__);
			break;
		}

		/*Get related data*/
		keep_on_flag = hx_bin_desc_data_get(ts, addr_t, fw_buf);

		addr_t = addr_t + FW_PAGE_SZ;
	} while (max_sz > addr_t && keep_on_flag);

	return g_bin_desc_flag;
}

/* FLASH side end*/
/* CORE_FLASH */

/* CORE_SRAM */
/* SRAM side start*/
static bool himax_mcu_get_DSRAM_data(struct himax_ts_data *ts, uint8_t *info_data, bool DSRAM_Flag)
{
	unsigned int i = 0;
	unsigned char tmp_addr[ADDR_LEN_4];
	unsigned char tmp_data[DATA_LEN_4];
	uint8_t max_i2c_size = MAX_I2C_TRANS_SZ;
	uint8_t x_num = ts->ic_data->HX_RX_NUM;
	uint8_t y_num = ts->ic_data->HX_TX_NUM;
	/*int m_key_num = 0;*/
	int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
	int total_data_size = (x_num * y_num + x_num + y_num) * 2;
	int total_size_temp;
	/*int mutual_data_size = x_num * y_num * 2;*/
	int total_read_times = 0;
	int address = 0;
	uint8_t *temp_info_data = NULL; /*max mkey size = 8*/
	uint16_t check_sum_cal = 0;
	int fw_run_flag = -1;

	temp_info_data = kcalloc((total_size + 8), sizeof(uint8_t), GFP_KERNEL);
	if (temp_info_data == NULL) {
		E("%s, Failed to allocate memory\n", __func__);
		return false;
	}
	/* 1. Read number of MKey R100070E8H to determin data size */
	/* m_key_num = ts->ic_data->HX_BT_NUM; */
	/* I("%s,m_key_num=%d\n",__func__ ,m_key_num); */
	/* total_size += m_key_num * 2; */
	/* 2. Start DSRAM Rawdata and Wait Data Ready */
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = ts->psram_op->passwrd_start[1];
	tmp_data[0] = ts->psram_op->passwrd_start[0];
	fw_run_flag =
		himax_write_read_reg(ts, ts->psram_op->addr_rawdata_addr, tmp_data,
				     ts->psram_op->passwrd_end[1], ts->psram_op->passwrd_end[0]);

	if (fw_run_flag < 0) {
		I("%s Data NOT ready => bypass\n", __func__);
		ts->core_fp.fp_read_FW_status(ts);
		goto FAIL;
	}

	/* 3. Read RawData */
	total_size_temp = total_size;
	I("%s:data[0]=0x%2X,data[1]=0x%2X,data[2]=0x%2X,data[3]=0x%2X\n", __func__,
	  ts->psram_op->addr_rawdata_addr[0], ts->psram_op->addr_rawdata_addr[1],
	  ts->psram_op->addr_rawdata_addr[2], ts->psram_op->addr_rawdata_addr[3]);

	tmp_addr[0] = ts->psram_op->addr_rawdata_addr[0];
	tmp_addr[1] = ts->psram_op->addr_rawdata_addr[1];
	tmp_addr[2] = ts->psram_op->addr_rawdata_addr[2];
	tmp_addr[3] = ts->psram_op->addr_rawdata_addr[3];

	if (total_size % max_i2c_size == 0)
		total_read_times = total_size / max_i2c_size;
	else
		total_read_times = total_size / max_i2c_size + 1;

	for (i = 0; i < total_read_times; i++) {
		address = (ts->psram_op->addr_rawdata_addr[3] << 24) +
			  (ts->psram_op->addr_rawdata_addr[2] << 16) +
			  (ts->psram_op->addr_rawdata_addr[1] << 8) +
			  ts->psram_op->addr_rawdata_addr[0] + i * max_i2c_size;
		/*I("%s address = %08X\n", __func__, address);*/

		tmp_addr[3] = (uint8_t)((address >> 24) & 0x00FF);
		tmp_addr[2] = (uint8_t)((address >> 16) & 0x00FF);
		tmp_addr[1] = (uint8_t)((address >> 8) & 0x00FF);
		tmp_addr[0] = (uint8_t)((address)&0x00FF);

		if (total_size_temp >= max_i2c_size) {
			himax_mcu_register_read(ts, tmp_addr, max_i2c_size,
						&temp_info_data[i * max_i2c_size], 0);
			total_size_temp = total_size_temp - max_i2c_size;
		} else {
			/*I("last total_size_temp=%d\n",total_size_temp);*/
			himax_mcu_register_read(ts, tmp_addr, total_size_temp % max_i2c_size,
						&temp_info_data[i * max_i2c_size], 0);
		}
	}

	/* 4. FW stop outputing */
	tmp_data[3] = temp_info_data[3];
	tmp_data[2] = temp_info_data[2];
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_mcu_register_write(ts, ts->psram_op->addr_rawdata_addr, DATA_LEN_4, tmp_data, 0);

	/* 5. Data Checksum Check */
	for (i = 2; i < total_size; i += 2) /* 2:PASSWORD NOT included */
		check_sum_cal += (temp_info_data[i + 1] * 256 + temp_info_data[i]);

	if (check_sum_cal % 0x10000 != 0) {
		I("%s check_sum_cal fail=%2X\n", __func__, check_sum_cal);
		goto FAIL;
	} else {
		memcpy(info_data, &temp_info_data[4], total_data_size * sizeof(uint8_t));
		/*I("%s checksum PASS\n", __func__);*/
	}
	kfree(temp_info_data);
	return true;
FAIL:
	kfree(temp_info_data);
	return false;
}
/* SRAM side end*/
/* CORE_SRAM */

static void himax_mcu_init_ic(struct himax_ts_data *ts)
{
	I("%s: use default incell init.\n", __func__);
}

#if defined(HX_BOOT_UPGRADE)
/*-------------------------------------------------------------------------
*
*	Create: Unknown
*
*	Using: Read FW_VER and CFG_VER value from FW file
*
*	param: None
*
*	Dependency function: himax_auto_update_check
*
*/
static int himax_mcu_fw_ver_bin(struct himax_ts_data *ts)
{
	I("%s: use default incell address.\n", __func__);
	if (ts->hxfw != NULL) {
		I("Catch fw version in bin file!\n");
		ts->fw_ver = (ts->hxfw->data[ts->fw_ver_maj_flash_addr] << 8) |
			     ts->hxfw->data[ts->fw_ver_min_flash_addr];
		ts->cfg_ver = (ts->hxfw->data[ts->cfg_ver_maj_flash_addr] << 8) |
			      ts->hxfw->data[ts->cfg_ver_min_flash_addr];
		ts->cid_maj = ts->hxfw->data[ts->cid_ver_maj_flash_addr];
		ts->cid_min = ts->hxfw->data[ts->cid_ver_min_flash_addr];
	} else {
		I("FW data is null!\n");
		return 1;
	}
	return NO_ERR;
}
#endif

void himax_mcu_tp_lcm_pin_reset(struct himax_ts_data *ts)
{
	I("%s: Brian debug \n", __func__);
	I("%s: Now reset the Touch chip and LCM.\n", __func__);
	himax_gpio_set(ts->rst_gpio, 0);
	himax_gpio_set(ts->lcm_gpio, 0);
	usleep_range(500, 600);
	himax_gpio_set(ts->rst_gpio, 1);
	msleep(100);
	himax_gpio_set(ts->lcm_gpio, 1);
}

#if defined(HX_RST_PIN_FUNC)
static void himax_mcu_pin_reset(struct himax_ts_data *ts)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	himax_gpio_set(ts->rst_gpio, 0);
	msleep(20);
	himax_gpio_set(ts->rst_gpio, 1);
	msleep(50);
}

static void himax_mcu_ic_reset(struct himax_ts_data *ts, uint8_t loadconfig, uint8_t int_off)
{
	ts->hx_hw_reset_activate = 0;
	I("%s,status: loadconfig=%d,int_off=%d\n", __func__, loadconfig, int_off);

	if (ts->rst_gpio >= 0) {
		if (int_off)
			ts->core_fp.fp_irq_switch(ts, 0);

		ts->core_fp.fp_pin_reset(ts);

		if (loadconfig)
			ts->core_fp.fp_reload_config(ts);

		if (int_off)
			ts->core_fp.fp_irq_switch(ts, 1);
	}
}
#endif

/*-------------------------------------------------------------------------
*
*	Create: Unknown
*
*	Using: Read related touch information from mcu or assign fixed values
*		   to ic_data value.
*
*	param: None
*
*	Dependency function: moduleID_mcu_ic_id_read
*-
*/
static void himax_mcu_touch_information(struct himax_ts_data *ts)
{
#if !defined(HX_FIX_TOUCH_INFO)
	uint8_t addr[DATA_LEN_4] = { 0 };
	uint8_t data[DATA_LEN_8] = { 0 };
	himax_parse_assign_cmd(addr_fw_define_rxnum_txnum_maxpt, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_8, data, 0);
	ts->ic_data->HX_RX_NUM = data[2];
	ts->ic_data->HX_TX_NUM = data[3];
	ts->ic_data->HX_MAX_PT = data[4];
	/*I("%s : HX_RX_NUM=%d,ic_data->HX_TX_NUM=%d,ic_data->HX_MAX_PT=%d\n",
	 *	__func__,ts->ic_data->HX_RX_NUM,
	 *	ts->ic_data->HX_TX_NUM,ts->ic_data->HX_MAX_PT);
	 */
	himax_parse_assign_cmd(addr_fw_define_xy_res_enable, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);

	/*I("%s : c_data->HX_XY_REVERSE=0x%2.2X\n",__func__,data[1]);*/
	if ((data[1] & 0x04) == 0x04)
		ts->ic_data->HX_XY_REVERSE = true;
	else
		ts->ic_data->HX_XY_REVERSE = false;
	himax_parse_assign_cmd(addr_fw_define_int_is_edge, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);
	/*I("%s : data[0]=0x%2.2X,data[1]=0x%2.2X,data[2]=0x%2.2X,
	 *	data[3]=0x%2.2X\n",__func__,data[0],data[1],data[2],data[3]);
	 */
	/*I("data[0] & 0x01 = %d\n",(data[0] & 0x01));*/

	if ((data[1] & 0x01) == 1)
		ts->ic_data->HX_INT_IS_EDGE = true;
	else
		ts->ic_data->HX_INT_IS_EDGE = false;

	/* add HX_ID_EN check */
	himax_parse_assign_cmd(addr_HX_ID_EN, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);

	if ((data[1] & 0x02) == 0x02) {
		ts->ic_data->HX_IS_ID_EN = true;
	} else {
		ts->ic_data->HX_IS_ID_EN = false;
	}

	/*Read number of MKey R100070E8H to determin data size*/
	himax_parse_assign_cmd(addr_mkey, addr, sizeof(addr));
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);

	ts->ic_data->HX_BT_NUM = data[0] & 0x03;

#else
	ts->ic_data->HX_RX_NUM = FIX_HX_RX_NUM;
	ts->ic_data->HX_TX_NUM = FIX_HX_TX_NUM;
	ts->ic_data->HX_BT_NUM = FIX_HX_BT_NUM;
	ts->ic_data->HX_MAX_PT = FIX_HX_MAX_PT;
	ts->ic_data->HX_XY_REVERSE = FIX_HX_XY_REVERSE;
	ts->ic_data->HX_INT_IS_EDGE = FIX_HX_INT_IS_EDGE;
#endif
	ts->ic_data->HX_Y_RES = ts->pdata->screenHeight;
	ts->ic_data->HX_X_RES = ts->pdata->screenWidth;

	ts->core_fp.fp_ic_id_read(ts);
	I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d\n", __func__, ts->ic_data->HX_RX_NUM,
	  ts->ic_data->HX_TX_NUM);
	I("%s:HX_MAX_PT=%d,HX_XY_REVERSE =%d\n", __func__, ts->ic_data->HX_MAX_PT,
	  ts->ic_data->HX_XY_REVERSE);
	I("%s:HX_Y_RES=%d,HX_X_RES =%d\n", __func__, ts->ic_data->HX_Y_RES, ts->ic_data->HX_X_RES);
}

static void himax_mcu_reload_config(struct himax_ts_data *ts)
{
	if (himax_report_data_init(ts))
		E("%s: allocate data fail\n", __func__);

	ts->core_fp.fp_sense_on(ts, 0x00);
}

static int himax_mcu_get_touch_data_size(struct himax_ts_data *ts)
{
	return HIMAX_TOUCH_DATA_SIZE;
}

static int himax_mcu_cal_data_len(struct himax_ts_data *ts, int raw_cnt_rmd, int HX_MAX_PT,
				  int raw_cnt_max)
{
	int RawDataLen;

	if (raw_cnt_rmd != 0x00)
		RawDataLen = MAX_I2C_TRANS_SZ - ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 1;
	else
		RawDataLen = MAX_I2C_TRANS_SZ - ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 1;

	return RawDataLen;
}

static bool himax_mcu_diag_check_sum(struct himax_ts_data *ts,
				     struct himax_report_data *hx_touch_data)
{
	uint16_t check_sum_cal = 0;
	int i;

	/* Check 128th byte CRC */
	for (i = 0, check_sum_cal = 0;
	     i < (hx_touch_data->touch_all_size - hx_touch_data->touch_info_size); i += 2) {
		check_sum_cal += (hx_touch_data->hx_rawdata_buf[i + 1] * FLASH_RW_MAX_LEN +
				  hx_touch_data->hx_rawdata_buf[i]);
	}

	if (check_sum_cal % HX64K != 0) {
		I("%s fail=%2X\n", __func__, check_sum_cal);
		return 0;
	}

	return 1;
}

static void himax_mcu_diag_parse_raw_data(struct himax_ts_data *ts,
					  struct himax_report_data *hx_touch_data, int mul_num,
					  int self_num, uint8_t diag_cmd, int32_t *mutual_data,
					  int32_t *self_data)
{
	diag_mcu_parse_raw_data(ts, hx_touch_data, mul_num, self_num, diag_cmd, mutual_data,
				self_data);
}

/* CORE_INIT */
/* init start */
static void himax_mcu_fp_init(struct himax_ts_data *ts)
{
	/* CORE_FW */
	ts->core_fp.fp_burst_enable = himax_mcu_burst_enable;
	ts->core_fp.fp_interface_on = himax_mcu_interface_on;
	ts->core_fp.fp_sense_on = himax_mcu_sense_on;
	ts->core_fp.fp_sense_off = himax_mcu_sense_off;
	ts->core_fp.fp_wait_wip = himax_mcu_wait_wip;
	ts->core_fp.fp_init_psl = himax_mcu_init_psl;
	ts->core_fp.fp_resume_ic_action = himax_mcu_resume_ic_action;
	ts->core_fp.fp_suspend_ic_action = himax_mcu_suspend_ic_action;
	ts->core_fp.fp_power_on_init = himax_mcu_power_on_init;
	ts->core_fp.fp_dd_clk_set = himax_mcu_dd_clk_set;
	ts->core_fp.fp_dd_reg_en = himax_mcu_dd_reg_en;
	ts->core_fp.fp_dd_reg_write = himax_mcu_dd_reg_write;
	ts->core_fp.fp_dd_reg_read = himax_mcu_dd_reg_read;
	ts->core_fp.fp_system_reset = himax_mcu_system_reset;
	ts->core_fp.fp_Calculate_CRC_with_AP = himax_mcu_Calculate_CRC_with_AP;
	ts->core_fp.fp_check_CRC = himax_mcu_check_CRC;
	ts->core_fp.fp_diag_register_set = himax_mcu_diag_register_set;
	ts->core_fp.fp_chip_self_test = himax_mcu_chip_self_test;
	ts->core_fp.fp_reload_disable = himax_mcu_reload_disable;
	ts->core_fp.fp_read_ic_trigger_type = himax_mcu_read_ic_trigger_type;
	ts->core_fp.fp_read_FW_ver = himax_mcu_read_FW_ver;
	ts->core_fp.fp_read_event_stack = himax_mcu_read_event_stack;
	ts->core_fp.fp_return_event_stack = himax_mcu_return_event_stack;
	ts->core_fp.fp_calculateChecksum = himax_mcu_calculateChecksum;
	ts->core_fp.fp_read_FW_status = himax_mcu_read_FW_status;
	ts->core_fp.fp_irq_switch = himax_mcu_irq_switch;
	ts->core_fp.fp_assign_sorting_mode = himax_mcu_assign_sorting_mode;
	ts->core_fp.fp_check_sorting_mode = himax_mcu_check_sorting_mode;
	/* CORE_FW */
	/* CORE_FLASH */
	ts->core_fp.fp_chip_erase = himax_mcu_chip_erase;
	ts->core_fp.fp_block_erase = himax_mcu_block_erase;
	ts->core_fp.fp_sector_erase = himax_mcu_sector_erase;
	ts->core_fp.fp_flash_programming = himax_mcu_flash_programming;
	ts->core_fp.fp_flash_page_write = himax_mcu_flash_page_write;
	ts->core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k =
		himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k;
	ts->core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k =
		himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k;
	ts->core_fp.fp_flash_dump_func = himax_mcu_flash_dump_func;
	ts->core_fp.fp_flash_lastdata_check = himax_mcu_flash_lastdata_check;
	ts->core_fp.fp_bin_desc_get = hx_mcu_bin_desc_get;

	/* CORE_FLASH */
	/* CORE_SRAM */
	ts->core_fp.fp_get_DSRAM_data = himax_mcu_get_DSRAM_data;
	/* CORE_SRAM */
	/* CORE_DRIVER */
	ts->core_fp.fp_chip_init = himax_mcu_init_ic;
#if defined(HX_BOOT_UPGRADE)
	ts->core_fp.fp_fw_ver_bin = himax_mcu_fw_ver_bin;
#endif
#if defined(HX_RST_PIN_FUNC)
	ts->core_fp.fp_pin_reset = himax_mcu_pin_reset;
	ts->core_fp.fp_ic_reset = himax_mcu_ic_reset;
#endif
	ts->core_fp.fp_touch_information = himax_mcu_touch_information;
	ts->core_fp.fp_reload_config = himax_mcu_reload_config;
	ts->core_fp.fp_get_touch_data_size = himax_mcu_get_touch_data_size;
	ts->core_fp.fp_cal_data_len = himax_mcu_cal_data_len;
	ts->core_fp.fp_diag_check_sum = himax_mcu_diag_check_sum;
	ts->core_fp.fp_diag_parse_raw_data = himax_mcu_diag_parse_raw_data;
}

int himax_mcu_in_cmd_struct_init(struct himax_ts_data *ts)
{
	int err = 0;

	I("%s: Entering!\n", __func__);

	ts->core_cmd_op = kzalloc(sizeof(struct himax_core_command_operation), GFP_KERNEL);
	if (ts->core_cmd_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_fail;
	}

	ts->core_cmd_op->fw_op = kzalloc(sizeof(struct fw_operation), GFP_KERNEL);
	if (ts->core_cmd_op->fw_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_fw_op_fail;
	}

	ts->core_cmd_op->flash_op = kzalloc(sizeof(struct flash_operation), GFP_KERNEL);
	if (ts->core_cmd_op->flash_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_flash_op_fail;
	}

	ts->core_cmd_op->sram_op = kzalloc(sizeof(struct sram_operation), GFP_KERNEL);
	if (ts->core_cmd_op->sram_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_sram_op_fail;
	}

	ts->ic_incell.pfw_op = ts->core_cmd_op->fw_op;
	ts->ic_incell.pflash_op = ts->core_cmd_op->flash_op;
	ts->psram_op = ts->core_cmd_op->sram_op;

	ts->internal_buffer = kzalloc(sizeof(uint8_t) * HX_MAX_WRITE_SZ, GFP_KERNEL);

	if (ts->internal_buffer == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_g_internal_buffer_fail;
	}
	himax_mcu_fp_init(ts);

	return NO_ERR;

err_g_core_cmd_op_g_internal_buffer_fail:
	kfree(ts->core_cmd_op->sram_op);
	ts->core_cmd_op->sram_op = NULL;
err_g_core_cmd_op_sram_op_fail:
	kfree(ts->core_cmd_op->flash_op);
	ts->core_cmd_op->flash_op = NULL;
err_g_core_cmd_op_flash_op_fail:
	kfree(ts->core_cmd_op->fw_op);
	ts->core_cmd_op->fw_op = NULL;
err_g_core_cmd_op_fw_op_fail:
	kfree(ts->core_cmd_op);
	ts->core_cmd_op = NULL;
err_g_core_cmd_op_fail:

	return err;
}
EXPORT_SYMBOL(himax_mcu_in_cmd_struct_init);

void himax_mcu_in_cmd_struct_free(struct himax_ts_data *ts)
{
	ts->ic_incell.pfw_op = NULL;
	ts->ic_incell.pflash_op = NULL;
	ts->psram_op = NULL;
	kfree(ts->internal_buffer);
	ts->internal_buffer = NULL;
	kfree(ts->core_cmd_op->sram_op);
	ts->core_cmd_op->sram_op = NULL;
	kfree(ts->core_cmd_op->flash_op);
	ts->core_cmd_op->flash_op = NULL;
	kfree(ts->core_cmd_op->fw_op);
	ts->core_cmd_op->fw_op = NULL;
	ts->core_cmd_op = NULL;

	I("%s: release completed\n", __func__);
}

void himax_mcu_in_cmd_init(struct himax_ts_data *ts)
{
	I("%s: Entering!\n", __func__);
	/* CORE_FW */
	himax_parse_assign_cmd(fw_addr_osc_en, ts->ic_incell.pfw_op->addr_osc_en,
			       sizeof(ts->ic_incell.pfw_op->addr_osc_en));
	himax_parse_assign_cmd(fw_addr_osc_pw, ts->ic_incell.pfw_op->addr_osc_pw,
			       sizeof(ts->ic_incell.pfw_op->addr_osc_pw));
	himax_parse_assign_cmd(fw_addr_program_reload_from,
			       ts->ic_incell.pfw_op->addr_program_reload_from,
			       sizeof(ts->ic_incell.pfw_op->addr_program_reload_from));
	himax_parse_assign_cmd(fw_addr_reload_status, ts->ic_incell.pfw_op->addr_reload_status,
			       sizeof(ts->ic_incell.pfw_op->addr_reload_status));
	himax_parse_assign_cmd(fw_addr_reload_crc32_result,
			       ts->ic_incell.pfw_op->addr_reload_crc32_result,
			       sizeof(ts->ic_incell.pfw_op->addr_reload_crc32_result));
	himax_parse_assign_cmd(fw_addr_reload_addr_from,
			       ts->ic_incell.pfw_op->addr_reload_addr_from,
			       sizeof(ts->ic_incell.pfw_op->addr_reload_addr_from));
	himax_parse_assign_cmd(fw_addr_reload_addr_cmd_beat,
			       ts->ic_incell.pfw_op->addr_reload_addr_cmd_beat,
			       sizeof(ts->ic_incell.pfw_op->addr_reload_addr_cmd_beat));
	himax_parse_assign_cmd(fw_data_rawdata_ready_hb,
			       ts->ic_incell.pfw_op->data_rawdata_ready_hb,
			       sizeof(ts->ic_incell.pfw_op->data_rawdata_ready_hb));
	himax_parse_assign_cmd(fw_data_rawdata_ready_lb,
			       ts->ic_incell.pfw_op->data_rawdata_ready_lb,
			       sizeof(ts->ic_incell.pfw_op->data_rawdata_ready_lb));
	/* CORE_FW */
	/* CORE_FLASH */
	himax_parse_assign_cmd(flash_addr_spi200_trans_fmt,
			       ts->ic_incell.pflash_op->addr_spi200_trans_fmt,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_trans_fmt));
	himax_parse_assign_cmd(flash_addr_spi200_trans_ctrl,
			       ts->ic_incell.pflash_op->addr_spi200_trans_ctrl,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_trans_ctrl));
	himax_parse_assign_cmd(flash_addr_spi200_fifo_rst,
			       ts->ic_incell.pflash_op->addr_spi200_fifo_rst,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_fifo_rst));
	himax_parse_assign_cmd(flash_addr_spi200_flash_speed,
			       ts->ic_incell.pflash_op->addr_spi200_flash_speed,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_flash_speed));
	himax_parse_assign_cmd(flash_addr_spi200_rst_status,
			       ts->ic_incell.pflash_op->addr_spi200_rst_status,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_rst_status));
	himax_parse_assign_cmd(flash_addr_spi200_cmd, ts->ic_incell.pflash_op->addr_spi200_cmd,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_cmd));
	himax_parse_assign_cmd(flash_addr_spi200_addr, ts->ic_incell.pflash_op->addr_spi200_addr,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_addr));
	himax_parse_assign_cmd(flash_addr_spi200_data, ts->ic_incell.pflash_op->addr_spi200_data,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_data));
	himax_parse_assign_cmd(flash_addr_spi200_bt_num,
			       ts->ic_incell.pflash_op->addr_spi200_bt_num,
			       sizeof(ts->ic_incell.pflash_op->addr_spi200_bt_num));
	himax_parse_assign_cmd(flash_data_spi200_trans_fmt,
			       ts->ic_incell.pflash_op->data_spi200_trans_fmt,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_trans_fmt));
	himax_parse_assign_cmd(flash_data_spi200_txfifo_rst,
			       ts->ic_incell.pflash_op->data_spi200_txfifo_rst,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_txfifo_rst));
	himax_parse_assign_cmd(flash_data_spi200_rxfifo_rst,
			       ts->ic_incell.pflash_op->data_spi200_rxfifo_rst,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_rxfifo_rst));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_1,
			       ts->ic_incell.pflash_op->data_spi200_trans_ctrl_1,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_trans_ctrl_1));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_2,
			       ts->ic_incell.pflash_op->data_spi200_trans_ctrl_2,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_trans_ctrl_2));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_3,
			       ts->ic_incell.pflash_op->data_spi200_trans_ctrl_3,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_trans_ctrl_3));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_4,
			       ts->ic_incell.pflash_op->data_spi200_trans_ctrl_4,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_trans_ctrl_4));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_6,
			       ts->ic_incell.pflash_op->data_spi200_trans_ctrl_6,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_trans_ctrl_6));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_7,
			       ts->ic_incell.pflash_op->data_spi200_trans_ctrl_7,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_trans_ctrl_7));
	himax_parse_assign_cmd(flash_data_spi200_cmd_1, ts->ic_incell.pflash_op->data_spi200_cmd_1,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_cmd_1));
	himax_parse_assign_cmd(flash_data_spi200_cmd_2, ts->ic_incell.pflash_op->data_spi200_cmd_2,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_cmd_2));
	himax_parse_assign_cmd(flash_data_spi200_cmd_3, ts->ic_incell.pflash_op->data_spi200_cmd_3,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_cmd_3));
	himax_parse_assign_cmd(flash_data_spi200_cmd_4, ts->ic_incell.pflash_op->data_spi200_cmd_4,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_cmd_4));
	himax_parse_assign_cmd(flash_data_spi200_cmd_6, ts->ic_incell.pflash_op->data_spi200_cmd_6,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_cmd_6));
	himax_parse_assign_cmd(flash_data_spi200_cmd_7, ts->ic_incell.pflash_op->data_spi200_cmd_7,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_cmd_7));
	himax_parse_assign_cmd(flash_data_spi200_cmd_8, ts->ic_incell.pflash_op->data_spi200_cmd_8,
			       sizeof(ts->ic_incell.pflash_op->data_spi200_cmd_8));
	/* CORE_FLASH */
	/* CORE_SRAM */
	/* sram start*/
	himax_parse_assign_cmd(sram_adr_rawdata_addr, ts->psram_op->addr_rawdata_addr,
			       sizeof(ts->psram_op->addr_rawdata_addr));
	himax_parse_assign_cmd(sram_adr_rawdata_end, ts->psram_op->addr_rawdata_end,
			       sizeof(ts->psram_op->addr_rawdata_end));
	himax_parse_assign_cmd(sram_passwrd_start, ts->psram_op->passwrd_start,
			       sizeof(ts->psram_op->passwrd_start));
	himax_parse_assign_cmd(sram_passwrd_end, ts->psram_op->passwrd_end,
			       sizeof(ts->psram_op->passwrd_end));
	/* sram end*/
	/* CORE_SRAM */
}
EXPORT_SYMBOL(himax_mcu_in_cmd_init);

/*-------------------------------------------------------------------------
*
*	Create: 2021/07
*
*	Using: Disable WP BP protection, or FW cannot be updated succesfully.
*
*	param: None
*
*	Dependency function: himax_mcu_WP_BP_enable, himax_mcu_flash_id_check
*
*  +-----------------+-----------+----+------+-----------+
*  |       ID        |    BP     | WP | type | Lock code |
*  +-----------------+-----------+----+------+-----------+
*  | 0               | 2_3       |  7 |    1 | 0x8C      |
*  | 1/6/11          | 2_3_4     |  7 |    2 | 0x9C      |
*  | 4/5/10/12/13/14 | 2_3_4_5   |  7 |    3 | 0xBC      |
*  | 2/3/7/8/9       | 2_3_4_5_6 |  7 |    4 | 0xFC      |
*  +-----------------+-----------+----+------+-----------+
*
*/
int himax_mcu_WP_BP_disable(struct himax_ts_data *ts)
{
	uint8_t addr[4];
	uint8_t data[4];
	uint8_t lock_code = 0;

	himax_mcu_flash_id_check(ts);

	/*high priority unlock*/
	lock_code = 0xFC;

#if defined(WP_GPIO4)
	/*WP addr */
	addr[3] = 0x90;
	addr[2] = 0x02;
	addr[1] = 0x80;
	addr[0] = 0x00;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x04;
	data[0] = 0x10;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x1C;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
#endif

#if defined(WP_GPIO0)
	/*WP addr */
	addr[3] = 0x90;
	addr[2] = 0x02;
	addr[1] = 0x80;
	addr[0] = 0x00;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x04;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x0C;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
#endif

	/*BP addr */
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x10;
	data[3] = 0x00;
	data[2] = 0x02;
	data[1] = 0x07;
	data[0] = 0x80;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x20;
	data[3] = 0x47;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x06;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x20;
	data[3] = 0x41;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x2C;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	msleep(40); //msleep(10);

	/*Check BP */
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x20;
	data[3] = 0x42;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x05;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	/*Read Addr*/
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x2C;
	himax_mcu_register_read(ts, addr, 1, data, 0);

	/*Addr 0x8000_002C value should be 0x00*/
	I("%s: WP BP disable check, Addr 0x8000_002C, read value is %02X\n", __func__, data[0]);

	data[0] = (~data[0]) & lock_code;

	if (data[0] != lock_code) {
		E("%s: Fail to disable WP BP lock with wrong reversed data :%x, lock code %x.\n",
		  __func__, data[0], lock_code);
		return -1;
	}

	I("%s: Disable WP BP lock finish.\n", __func__);
	return 0;
}

/*-------------------------------------------------------------------------
*
*	Create: 2021/07
*
*	Using: Enable WP BP protection after FW update.
*
*	param: None
*
*	Dependency function: himax_mcu_WP_BP_disable, himax_mcu_flash_id_check
*
*  +-----------------+-----------+----+------+-----------+
*  |       ID        |    BP     | WP | type | Lock code |
*  +-----------------+-----------+----+------+-----------+
*  | 0               | 2_3       |  7 |    1 | 0x8C      |
*  | 1/6/11          | 2_3_4     |  7 |    2 | 0x9C      |
*  | 4/5/10/12/13/14 | 2_3_4_5   |  7 |    3 | 0xBC      |
*  | 2/3/7/8/9       | 2_3_4_5_6 |  7 |    4 | 0xFC      |
*  +-----------------+-----------+----+------+-----------+
*
*/
int himax_mcu_WP_BP_enable(struct himax_ts_data *ts)
{
	uint8_t addr[4];
	uint8_t data[4];
	int ret = 0;
	uint8_t lock_code = 0;

	ret = himax_mcu_flash_id_check(ts);

	if (ret == -1) {
		E("%s: Cannot recognize flash id type \n", __func__);
		return -1;
	}

	switch (ts->ic_incell.pfw_op->flash_lock_type[0]) {
	case 1:
		lock_code = 0x8C;
		break;
	case 2:
		lock_code = 0x9C;
		break;
	case 3:
		lock_code = 0xBC;
		break;
	case 4:
		lock_code = 0xFC;
		break;
	default:
		E("%s: Unknown lock type with value : %x \n", __func__,
		  ts->ic_incell.pfw_op->flash_lock_type[0]);
		break;
	}

	/*Check Addr 0x8000_002C value, if the same as lock_code BP is lock*/
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x10;
	data[3] = 0x00;
	data[2] = 0x02;
	data[1] = 0x07;
	data[0] = 0x80;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x20;
	data[3] = 0x42;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x05;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x2C;
	himax_mcu_register_read(ts, addr, 1, data, 0);

	/* bit 2_3_4 and bit 7 should be 1 */
	I("%s: BP lock check before enable WP BP, Addr 0x8000_002C, read value is %02X\n", __func__,
	  data[0]);

	data[0] = data[0] & lock_code;

	/* fast check need to judge according to flash type */
	if (data[0] == lock_code) {
		I("%s: Enable fast check pass with value %02X\n", __func__, data[0]);
		return 0;
	}

	/*BP addr */
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x10;
	data[3] = 0x00;
	data[2] = 0x02;
	data[1] = 0x07;
	data[0] = 0x80;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x20;
	data[3] = 0x47;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x06;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x20;
	data[3] = 0x41;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x2C;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = lock_code;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	msleep(10);

	/*Check BP */
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x20;
	data[3] = 0x42;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x05;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	/*Read Addr*/
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x2C;
	himax_mcu_register_read(ts, addr, 1, data, 0);

	/*Addr 0x8000_002C value should be the same as lock_code*/
	I("%s: WP BP enable check, Addr 0x8000_002C, read value is %02X\n", __func__, data[0]);

	if (data[0] != lock_code) {
		E("%s: Fail to enable WP BP lock with wrong data :%x, lock code %x.\n", __func__,
		  data[0], lock_code);
		return -1;
	}

#if defined(WP_GPIO0)
	/*WP addr */
	addr[3] = 0x90;
	addr[2] = 0x02;
	addr[1] = 0x80;
	addr[0] = 0x00;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x04;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x0C;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
#endif

#if defined(WP_GPIO4)
	/*WP addr */
	addr[3] = 0x90;
	addr[2] = 0x02;
	addr[1] = 0x80;
	addr[0] = 0x00;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x01;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x04;
	data[0] = 0x10;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x1C;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);
#endif

	I("%s: Enable WP BP lock finish.\n", __func__);
	return 0;
}

/*-------------------------------------------------------------------------
*
*	Create: 2021/08
*
*	Using: Check WP BP protection status.
*
*	param: None
*
*	Dependency function: himax_mcu_flash_id_check
*
*  +-----------------+-----------+----+------+-----------+
*  |       ID        |    BP     | WP | type | Lock code |
*  +-----------------+-----------+----+------+-----------+
*  | 0               | 2_3       |  7 |    1 | 0x8C      |
*  | 1/6/11          | 2_3_4     |  7 |    2 | 0x9C      |
*  | 4/5/10/12/13/14 | 2_3_4_5   |  7 |    3 | 0xBC      |
*  | 2/3/7/8/9       | 2_3_4_5_6 |  7 |    4 | 0xFC      |
*  +-----------------+-----------+----+------+-----------+
*
*/
int himax_mcu_WP_BP_status(struct himax_ts_data *ts)
{
	uint8_t addr[4];
	uint8_t data[4];
	int ret = 0;
	uint8_t lock_code = 0;

	ret = himax_mcu_flash_id_check(ts);

	if (ret == -1) {
		E("%s: Cannot recognize flash id type \n", __func__);
		return -1;
	}

	switch (ts->ic_incell.pfw_op->flash_lock_type[0]) {
	case 1:
		lock_code = 0x8C;
		break;
	case 2:
		lock_code = 0x9C;
		break;
	case 3:
		lock_code = 0xBC;
		break;
	case 4:
		lock_code = 0xFC;
		break;
	default:
		E("%s: Unknown lock type with value : %x \n", __func__,
		  ts->ic_incell.pfw_op->flash_lock_type[0]);
		break;
	}

	/*Check Addr 0x8000_002C value, if 0x9C BP is lock*/
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x10;
	data[3] = 0x00;
	data[2] = 0x02;
	data[1] = 0x07;
	data[0] = 0x80;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x20;
	data[3] = 0x42;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x05;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x2C;
	himax_mcu_register_read(ts, addr, 1, data, 0);

	I("%s: WP BP lock check, Addr 0x8000_002C, read value is %02X\n", __func__, data[0]);

	if (data[0] == lock_code) {
		I("%s: WP BP lock status is lock with value : %x.\n", __func__, data[0]);
	} else {
		I("%s: WP BP lock status is unlock with value : %x.\n", __func__, data[0]);
	}
	return data[0];
}

/*-------------------------------------------------------------------------
*
*	Create: 2021/08
*
*	Using: Check Flash ID and sort ID to Lock type.
*
*	param: None
*
*	Dependency function: himax_mcu_WP_BP_disable, himax_mcu_WP_BP_enable
*
*  +-----------------+-----------+----+------+-----------+
*  |       ID        |    BP     | WP | type | Lock code |
*  +-----------------+-----------+----+------+-----------+
*  | 0               | 2_3       |  7 |    1 | 0x8C      |
*  | 1/6/11          | 2_3_4     |  7 |    2 | 0x9C      |
*  | 4/5/10/12/13/14 | 2_3_4_5   |  7 |    3 | 0xBC      |
*  | 2/3/7/8/9       | 2_3_4_5_6 |  7 |    4 | 0xFC      |
*  +-----------------+-----------+----+------+-----------+
*
*/
int himax_mcu_flash_id_check(struct himax_ts_data *ts)
{
	uint8_t addr[4];
	uint8_t data[4];
	uint8_t i;
	uint8_t Flash_list_tmp[][3] = Flash_list;
	size_t len = sizeof(Flash_list_tmp) / (sizeof(uint8_t) * 3);
	uint8_t flash_idx;

	flash_idx = 0xff;

	/*Check Addr 0x8000_002C value, if 0x9C BP is lock*/
	addr[3] = 0x80;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x00 + 0x20;
	data[3] = 0x42;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x02;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x24;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x9F;
	himax_mcu_register_write(ts, addr, DATA_LEN_4, data, 0);

	addr[0] = 0x00 + 0x2C;
	himax_mcu_register_read(ts, addr, DATA_LEN_4, data, 0);

	I("%s: FlashList len : %zd, Flash type ID data = %X,%X,%X\n", __func__, len, data[0],
	  data[1], data[2]);

	for (i = 0; i < len; i++) {
		if ((Flash_list_tmp[i][0] == data[0]) && (Flash_list_tmp[i][1] == data[1]) &&
		    (Flash_list_tmp[i][2] == data[2])) {
			flash_idx = i;
			break;
		}
	}

	I("%s: Flash id mapping to index : %d.\n", __func__, flash_idx);

	if (flash_idx == 0) {
		ts->ic_incell.pfw_op->flash_lock_type[0] = 1;
	} else if ((flash_idx == 1) || (flash_idx == 6) || (flash_idx == 11)) {
		ts->ic_incell.pfw_op->flash_lock_type[0] = 2;
	} else if ((flash_idx == 4) || (flash_idx == 5) || (flash_idx == 10) || (flash_idx == 12) ||
		   (flash_idx == 13) || (flash_idx == 14)) {
		ts->ic_incell.pfw_op->flash_lock_type[0] = 3;
	} else if ((flash_idx == 2) || (flash_idx == 3) || (flash_idx == 7) || (flash_idx == 8) ||
		   (flash_idx == 9)) {
		ts->ic_incell.pfw_op->flash_lock_type[0] = 4;
	} else {
		ts->ic_incell.pfw_op->flash_lock_type[0] = 0xff;
	}

	if (ts->ic_incell.pfw_op->flash_lock_type[0] == 0xff) {
		E("%s: Flash_lock_type Unknown cause FlashList compare fail \n", __func__);
		return -1;
	}

	I("%s: Flash id check and sort finished.\n", __func__);
	return 0;
}

/* init end*/
/* CORE_INIT */
