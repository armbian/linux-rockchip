/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_lib/hyn_spi.c
 *
 * hynitron TouchScreen driver.
 *
 * Copyright (c) 2025  hynitron
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "../hyn_core.h"

#ifndef I2C_PORT

int hyn_write_data(struct hyn_ts_data *ts_data, u8 *buf, u8 reg_len, u16 len)
{
	struct spi_message msg;
	struct spi_transfer transfer[1] = { {0} };
	int ret;
	mutex_lock(&ts_data->mutex_bus);

	spi_message_init(&msg);
	transfer[0].len = len;
	transfer[0].delay_usecs = SPI_DELAY_CS;
	transfer[0].tx_buf = buf;
	transfer[0].rx_buf = NULL;
	spi_message_add_tail(&transfer[0], &msg);
	ret = spi_sync(ts_data->client, &msg);

	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? ret:0;
}

int hyn_read_data(struct hyn_ts_data *ts_data,u8 *buf, u16 len)
{
	struct spi_message msg;
	struct spi_transfer transfer[1] = { {0} };
	int ret;
	mutex_lock(&ts_data->mutex_bus);

	spi_message_init(&msg);
	transfer[0].len = len;
	transfer[0].delay_usecs = SPI_DELAY_CS;
	transfer[0].tx_buf = NULL;
	transfer[0].rx_buf = buf;
	spi_message_add_tail(&transfer[0], &msg);
	ret = spi_sync(ts_data->client, &msg);

	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? ret:0;
}

int hyn_wr_reg(struct hyn_ts_data *ts_data, u32 reg_addr, u8 reg_len, u8 *rbuf, u16 rlen)
{
	int ret = 0,i=0;
	struct spi_message msg;
	struct spi_transfer transfer[2] = { {0}, {0} };
	u8 cmd[4];
	mutex_lock(&ts_data->mutex_bus);
	reg_len = reg_len&0x0F;
	memset(cmd,0,sizeof(cmd));
	i = reg_len;
	while(i){
		i--;
		cmd[i] = reg_addr;
		reg_addr >>= 8;
	}
	spi_message_init(&msg);
	transfer[0].len = reg_len;
	transfer[0].tx_buf = cmd;
	transfer[0].rx_buf = NULL;
	spi_message_add_tail(&transfer[0], &msg);
	ret = spi_sync(ts_data->client, &msg);
	if (ret < 0) {
		mutex_unlock(&ts_data->mutex_bus);
		return ret;
	}
	udelay(20);
	if (rlen > 0) {
		spi_message_init(&msg);
		transfer[1].len = rlen;
		transfer[1].delay_usecs = SPI_DELAY_CS;
		transfer[1].tx_buf = NULL;
		transfer[1].rx_buf = rbuf;
		spi_message_add_tail(&transfer[1], &msg);
		ret = spi_sync(ts_data->client, &msg);
	}

	mutex_unlock(&ts_data->mutex_bus);

	return ret < 0 ? ret:0;
}

#endif
