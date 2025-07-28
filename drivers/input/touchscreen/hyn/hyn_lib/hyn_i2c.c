/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_lib/hyn_i2c.c
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

#ifdef I2C_PORT

#if (I2C_USE_DMA==1)

int hyn_read_data(struct hyn_ts_data *ts_data,u8 *buf, u16 len)
{
	int ret = -1;
	struct i2c_msg i2c_msg;
	mutex_lock(&ts_data->mutex_bus);
	i2c_msg.addr = ts_data->client->addr;
	i2c_msg.flags = I2C_M_RD;
	i2c_msg.len = len;
	i2c_msg.buf = buf;
	ret = i2c_transfer(ts_data->client->adapter, &i2c_msg, 1);
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}

int hyn_write_data(struct hyn_ts_data *ts_data, u8 *buf, u8 reg_len, u16 len)
{
	int ret = 0;
	struct i2c_msg i2c_msg;
	u8 cmd_len = reg_len&0x0F;
	if(cmd_len > 4) return -1;
	mutex_lock(&ts_data->mutex_bus);
	i2c_msg.addr = ts_data->client->addr;
	i2c_msg.flags = 0;
	i2c_msg.len = len;
	i2c_msg.buf = buf;
	ret = i2c_transfer(ts_data->client->adapter, &i2c_msg, 1);
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}

int hyn_wr_reg(struct hyn_ts_data *ts_data, u32 reg_addr, u8 reg_len, u8 *rbuf, u16 rlen)
{
	int ret = 0,i=0;
	struct i2c_msg i2c_msg[2];
	u8 wbuf[4];
	u8 cmd_len = reg_len&0x0F;
	if(cmd_len==0 || cmd_len > 4) return -1;
	mutex_lock(&ts_data->mutex_bus);
	memset(wbuf,0,sizeof(wbuf));
	i = cmd_len;
	while(i){
		i--;
		wbuf[i] = reg_addr;
		reg_addr >>= 8;
	}
	i2c_msg[0].addr = ts_data->client->addr;
	i2c_msg[0].flags = 0;
	i2c_msg[0].len = cmd_len;
	i2c_msg[0].buf = wbuf;
	ret = i2c_transfer(ts_data->client->adapter, &i2c_msg[0], 1);
	if(rlen){
		i2c_msg[1].addr = ts_data->client->addr;
		i2c_msg[1].flags = I2C_M_RD;
		i2c_msg[1].buf = rbuf;
		i2c_msg[1].len = rlen;
		ret |= i2c_transfer(ts_data->client->adapter, &i2c_msg[1], 1);
	}
	// ret = i2c_transfer(ts_data->client->adapter, i2c_msg, rlen ? 2:1);
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}

#elif (I2C_USE_DMA==2)

int hyn_write_data(struct hyn_ts_data *ts_data, u8 *buf, u8 reg_len, u16 len)
{
	int ret = 0;
	mutex_lock(&ts_data->mutex_bus);
	ts_data->client->addr = ((ts_data->client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
	memcpy(ts_data->dma_buff_va,buf, len);
	ret = i2c_master_send(ts_data->client, (u8 *)ts_data->dma_buff_pa, len);
	ts_data->client->addr = ts_data->client->addr& I2C_MASK_FLAG & (~I2C_DMA_FLAG);
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}

int hyn_read_data(struct hyn_ts_data *ts_data,u8 *buf, u16 len)
{
	int ret = 0;
	mutex_lock(&ts_data->mutex_bus);
	ts_data->client->addr = ((ts_data->client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
	ret = i2c_master_recv(ts_data->client, (u8 *)ts_data->dma_buff_pa, len);
	memcpy(buf, ts_data->dma_buff_va, len);
	ts_data->client->addr = ts_data->client->addr& I2C_MASK_FLAG & (~I2C_DMA_FLAG);
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}

int hyn_wr_reg(struct hyn_ts_data *ts_data, u32 reg_addr, u8 reg_len, u8 *rbuf, u16 rlen)
{
	int ret = 0,i=0;
	u8 wbuf[4];
	mutex_lock(&ts_data->mutex_bus);
	reg_len = reg_len&0x0F;
	memset(wbuf,0,sizeof(wbuf));
	i = reg_len;
	while(i){
		i--;
		wbuf[i] = reg_addr;
		reg_addr >>= 8;
	}
	ts_data->client->addr = ((ts_data->client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG);
	memcpy(ts_data->dma_buff_va,wbuf, reg_len);
	ret = i2c_master_send(ts_data->client, (u8 *)ts_data->dma_buff_pa, reg_len);
	if(rlen){
		ret |= i2c_master_recv(ts_data->client, (u8 *)ts_data->dma_buff_pa, rlen);
		memcpy(rbuf, ts_data->dma_buff_va, rlen);
	}
	ts_data->client->addr = ts_data->client->addr& I2C_MASK_FLAG & (~I2C_DMA_FLAG);
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}

#else //0

int hyn_read_data(struct hyn_ts_data *ts_data,u8 *buf, u16 len)
{
	int ret = -1;
	mutex_lock(&ts_data->mutex_bus);
	while(len){
		if(len > HYN_TRANSFER_LIMIT_LEN){
			ret = i2c_master_recv(ts_data->client, buf, HYN_TRANSFER_LIMIT_LEN);
			len -= HYN_TRANSFER_LIMIT_LEN;
			buf += HYN_TRANSFER_LIMIT_LEN;
		}
		else{
			ret = i2c_master_recv(ts_data->client, buf, len);
			len = 0;
		}
		if (ret < 0) {
			break;
		}
	}
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}
int hyn_write_data(struct hyn_ts_data *ts_data, u8 *buf, u8 reg_len, u16 len)
{
	int ret = 0;
	u8 cmd_len = reg_len&0x0F;
	if(cmd_len > 4) return -1;
	mutex_lock(&ts_data->mutex_bus);
#if HYN_TRANSFER_LIMIT_LEN < 1048
	if(len > HYN_TRANSFER_LIMIT_LEN){
		u32 reg = U8TO32(buf[0],buf[1],buf[2],buf[3])>>((4-cmd_len)*8);
		u8 w_buf[HYN_TRANSFER_LIMIT_LEN],i;
		u16 step = HYN_TRANSFER_LIMIT_LEN-cmd_len;
		u16 w_len = 0;
		buf += cmd_len;
		len -= cmd_len;
		while(len){
			i = cmd_len;
			while(i--){
				w_buf[cmd_len-1-i] = reg>>(i*8);
			}
			memcpy(&w_buf[cmd_len],buf,step);
			if(0==(reg_len&0x80)){
				reg += step;
			}
			else{
				reg = (reg&0xffff0000)|((reg>>8)&0xff)|((reg<<8)&0xff00);
				reg += step;
				reg = (reg&0xffff0000)|((reg>>8)&0xff)|((reg<<8)&0xff00);
			}
			buf += step;
			if(len > step){
				w_len = HYN_TRANSFER_LIMIT_LEN;
				len -= step;
			}
			else{
				w_len = len+cmd_len;
				len = 0;
			}
			ret = i2c_master_send(ts_data->client, w_buf, w_len);
			if(ret < 0) break;
		}
	}else
#endif
	{
		ret = i2c_master_send(ts_data->client, buf, len);
	}
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}

int hyn_wr_reg(struct hyn_ts_data *ts_data, u32 reg_addr, u8 reg_len, u8 *rbuf, u16 rlen)
{
	int ret = 0,i=0;
	u8 wbuf[4];
	u8 cmd_len = reg_len&0x0F;
	if(cmd_len==0 || cmd_len > 4) return -1;
	mutex_lock(&ts_data->mutex_bus);
	do{
		i = cmd_len-1;
		do{
			wbuf[cmd_len-1-i] = reg_addr>>(i*8);
		}while(i--);
		ret = i2c_master_send(ts_data->client, wbuf, cmd_len);
		if(rlen){
			ret |= i2c_master_recv(ts_data->client, rbuf, rlen < HYN_TRANSFER_LIMIT_LEN ? rlen : HYN_TRANSFER_LIMIT_LEN);
		}
		if(ret < 0 || rlen <= HYN_TRANSFER_LIMIT_LEN) break;
		rbuf += HYN_TRANSFER_LIMIT_LEN;
		rlen -= HYN_TRANSFER_LIMIT_LEN;
		if(0==(reg_len&0x80)){
			reg_addr += HYN_TRANSFER_LIMIT_LEN;
		}
		else{
			reg_addr = (reg_addr&0xffff0000)|((reg_addr>>8)&0xff)|((reg_addr<<8)&0xff00);
			reg_addr += HYN_TRANSFER_LIMIT_LEN;
			reg_addr = (reg_addr&0xffff0000)|((reg_addr>>8)&0xff)|((reg_addr<<8)&0xff00);
		}
	}while(1);
	mutex_unlock(&ts_data->mutex_bus);
	return ret < 0 ? -1:0;
}
#endif
#endif
