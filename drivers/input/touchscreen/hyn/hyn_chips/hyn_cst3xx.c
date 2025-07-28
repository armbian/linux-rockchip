/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_chips/hyn_cst3xx.c
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
#include "cst3xx_fw.h"

#define BOOT_I2C_ADDR   (0x1A)
#define MAIN_I2C_ADDR   (0x1A)
#define RW_REG_LEN	  (2)
#define CST3xx_BIN_SIZE	(24*1024 + 24)
#define SOFT_RST_ENABLE	 (0)

static struct hyn_ts_data *hyn_3xxdata = NULL;
static const u8 gest_map_tbl[33] = {0xff,4,1,3,2,5,12,6,7,7,9,11,10,13,12,7,7,6,10,6,5,0xff,0xff,
				    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,14};

static int cst3xx_updata_judge(u8 *p_fw, u16 len);
static int cst3xx_read_checksum(u32* check_sum);
static int cst3xx_updata_tpinfo(void);
static int cst3xx_enter_boot(void);
static int cst3xx_set_workmode(enum work_mode mode,u8 enable);
static void cst3xx_rst(void);

static int cst3xx_init(struct hyn_ts_data* ts_data)
{
	int ret = 0;
	HYN_ENTER();
	hyn_3xxdata = ts_data;
	ret = cst3xx_enter_boot();
	if(ret){
		HYN_ERROR("cst3xx_enter_boot failed");
		return -1;
	}
	hyn_3xxdata->fw_updata_addr = (u8*)fw_bin;
	hyn_3xxdata->fw_updata_len = CST3xx_BIN_SIZE;
	if(0 == cst3xx_read_checksum(&hyn_3xxdata->hw_info.ic_fw_checksum)){
		hyn_3xxdata->boot_is_pass = 1;
	}
	hyn_set_i2c_addr(hyn_3xxdata,MAIN_I2C_ADDR);
	cst3xx_rst();
	mdelay(50);

	hyn_3xxdata->need_updata_fw = cst3xx_updata_judge((u8*)fw_bin,CST3xx_BIN_SIZE);
	if(hyn_3xxdata->need_updata_fw){
		HYN_INFO("need updata FW !!!");
	}
	return 0;
}

static int cst3xx_report(void)
{
	u8 buf[80];
	u8 finger_num = 0,len = 0,write_tail_end = 0,key_state=0,key_id = 0,tmp_dat;
	int ret = 0;
	hyn_3xxdata->rp_buf.report_need = REPORT_NONE;
	switch(hyn_3xxdata->work_mode){
		case NOMAL_MODE:
			write_tail_end = 1;
			ret = hyn_wr_reg(hyn_3xxdata,0xD000,2,buf,7);
			len = buf[5] & 0x7F;
			if(ret || buf[6] != 0xAB || buf[0] == 0xAB || len > MAX_POINTS_REPORT){
				ret = -1;
				break;
			}
			if(buf[5]&0x80){ //key
				if(buf[5]==0x80){
					key_id = (buf[1]>>4)-1;
					key_state = buf[0];
				}
				else{
					// finger_num = len;
					len = (len-1)*5+3;
					ret = hyn_wr_reg(hyn_3xxdata,0xD007,2,&buf[5],len);
					len += 5;
					key_id = (buf[len-2]>>4)-1;
					key_state = buf[len-3];
				}
				if(key_state&0x80){
					hyn_3xxdata->rp_buf.report_need |= REPORT_KEY;
					if((key_id == hyn_3xxdata->rp_buf.key_id || 0 == hyn_3xxdata->rp_buf.key_state)&& key_state == 0x83){
						hyn_3xxdata->rp_buf.key_id = key_id;
						hyn_3xxdata->rp_buf.key_state = 1;
					}
					else{
						hyn_3xxdata->rp_buf.key_state = 0;
					}
				}
			}
			else{ //pos
				u16 index = 0;
				u8 i = 0;
				finger_num = len;
				hyn_3xxdata->rp_buf.report_need |= REPORT_POS;
				if(finger_num > 1){
					len = (len-1)*5 + 1;
					ret = hyn_wr_reg(hyn_3xxdata,0xD007,2,&buf[5],len);
				}
				hyn_3xxdata->rp_buf.rep_num = finger_num;
				for(i = 0; i < finger_num; i++){
					index = i*5;
					hyn_3xxdata->rp_buf.pos_info[i].pos_id =  (buf[index]>>4)&0x0F;
					hyn_3xxdata->rp_buf.pos_info[i].event =  (buf[index]&0x0F) == 0x06 ? 1 : 0;
					hyn_3xxdata->rp_buf.pos_info[i].pos_x = ((u16)buf[index + 1]<<4) + ((buf[index + 3] >> 4) & 0x0F);
					hyn_3xxdata->rp_buf.pos_info[i].pos_y = ((u16)buf[index + 2]<<4) + (buf[index + 3] & 0x0F);
					hyn_3xxdata->rp_buf.pos_info[i].pres_z = buf[index + 4];
					// HYN_INFO("report_id = %d, xy = %d,%d",hyn_3xxdata->rp_buf.pos_info[i].pos_id,hyn_3xxdata->rp_buf.pos_info[i].pos_x,hyn_3xxdata->rp_buf.pos_info[i].pos_y);
				}
			}
			break;
		case GESTURE_MODE:
			ret = hyn_wr_reg(hyn_3xxdata,0xD04C,2,&tmp_dat,1);
			if((tmp_dat&0x7F) <= 32){
				tmp_dat = tmp_dat&0x7F;
				hyn_3xxdata->gesture_id = gest_map_tbl[tmp_dat];
				hyn_3xxdata->rp_buf.report_need |= REPORT_GES;
			}
			break;
		default:
			break;
	}
	if(write_tail_end){
		hyn_wr_reg(hyn_3xxdata,0xD000AB,3,buf,0);
	}
	return ret;
}

static int cst3xx_prox_handle(u8 cmd)
{
	int ret = 0;
	switch(cmd){
		case 1: //enable
			hyn_3xxdata->prox_is_enable = 1;
			hyn_3xxdata->prox_state = 0;
			ret = hyn_wr_reg(hyn_3xxdata,0xD004B01,3,NULL,0);
			break;
		case 0: //disable
			hyn_3xxdata->prox_is_enable = 0;
			hyn_3xxdata->prox_state = 0;
			ret = hyn_wr_reg(hyn_3xxdata,0xD004B00,3,NULL,0);
			break;
		case 2: //read
			ret = hyn_wr_reg(hyn_3xxdata,0xD004B,2,&hyn_3xxdata->prox_state,1);
			break;
	}
	return ret;
}

static int cst3xx_set_workmode(enum work_mode mode,u8 enable)
{
	int ret = 0;
	hyn_3xxdata->work_mode = mode;
	if(mode != NOMAL_MODE)
		hyn_esdcheck_switch(hyn_3xxdata,DISABLE);
	switch(mode){
		case NOMAL_MODE:
			hyn_irq_set(hyn_3xxdata,ENABLE);
			hyn_esdcheck_switch(hyn_3xxdata,enable);
			// hyn_wr_reg(hyn_3xxdata,0xD100,2,NULL,0);
			hyn_wr_reg(hyn_3xxdata,0xD109,2,NULL,0);
			break;
		case GESTURE_MODE:
			hyn_wr_reg(hyn_3xxdata,0xD04C80,3,NULL,0);
			break;
		case LP_MODE:
			break;
		case DIFF_MODE:
			hyn_wr_reg(hyn_3xxdata,0xD10D,2,NULL,0);
			break;
		case RAWDATA_MODE:
			hyn_wr_reg(hyn_3xxdata,0xD10A,2,NULL,0);
			break;
		case FAC_TEST_MODE:
			hyn_wr_reg(hyn_3xxdata,0xD119,2,NULL,0);
			break;
		case DEEPSLEEP:
			hyn_irq_set(hyn_3xxdata,DISABLE);
			hyn_wr_reg(hyn_3xxdata,0xD105,2,NULL,0);
			break;
		case ENTER_BOOT_MODE:
			ret |= cst3xx_enter_boot();
			break;
		default :
			hyn_esdcheck_switch(hyn_3xxdata,ENABLE);
			hyn_3xxdata->work_mode = NOMAL_MODE;
			break;
	}
	return ret;
}


static int cst3xx_supend(void)
{
	HYN_ENTER();
	cst3xx_set_workmode(DEEPSLEEP,0);
	return 0;
}

static int cst3xx_resum(void)
{
	HYN_ENTER();
	cst3xx_rst();
	msleep(50);
	cst3xx_set_workmode(NOMAL_MODE,0);
	return 0;
}

static void cst3xx_rst(void)
{
	if(hyn_3xxdata->work_mode==ENTER_BOOT_MODE){
		hyn_set_i2c_addr(hyn_3xxdata,MAIN_I2C_ADDR);
	}
	#if SOFT_RST_ENABLE
	hyn_wr_reg(hyn_3xxdata,0xD10E,2,NULL,0);
	#endif
	gpiod_direction_output(hyn_3xxdata->plat_data.reset_gpio,0);
	msleep(10);
	gpiod_direction_output(hyn_3xxdata->plat_data.reset_gpio,1);
}

static int cst3xx_enter_boot(void)
{
	int retry = 5,ret = 0;
	u8 buf[4] = {0};
	hyn_set_i2c_addr(hyn_3xxdata,BOOT_I2C_ADDR);
	while(++retry < 17){
		cst3xx_rst();
		mdelay(retry);
		ret = hyn_wr_reg(hyn_3xxdata,0xA001AA,3,buf,0);
		if(ret != 0){
			continue;
		}
		ret = hyn_wr_reg(hyn_3xxdata,0xA002,2,buf,1);
		if(ret == 0 &&  (buf[0] == 0xAC || buf[0] == 0x55)){ //ac:3xx 55:1xx
			return 0;
		}
	}
	return -1;
}

static int cst3xx_updata_tpinfo(void)
{
	u8 buf[28];
	struct tp_info *ic = &hyn_3xxdata->hw_info;
	int ret = 0;
	ret = hyn_wr_reg(hyn_3xxdata,0xD101,2,buf,0);
	mdelay(1);
	ret |= hyn_wr_reg(hyn_3xxdata,0xD1F4,2,buf,28);
	if(ret){
		HYN_ERROR("cst3xx_updata_tpinfo failed");
		return -1;
	}

	ic->fw_sensor_txnum = buf[0];
	ic->fw_sensor_rxnum = buf[2];
	ic->fw_key_num = buf[3];
	ic->fw_res_y = (buf[7]<<8)|buf[6];
	ic->fw_res_x = (buf[5]<<8)|buf[4];
	ic->fw_project_id = (buf[17]<<8)|buf[16];
	ic->fw_chip_type = (buf[19]<<8)|buf[18];
	ic->fw_ver = (buf[23]<<24)|(buf[22]<<16)|(buf[21]<<8)|buf[20];

	HYN_INFO("IC_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);
	return 0;
}

static int cst3xx_read_checksum(u32* check_sum)
{
	int ret;
	u8 buf[4],retry = 5;
	while(retry--){
		ret = hyn_wr_reg(hyn_3xxdata,0xA000,2,buf,1);
		if(ret){
			mdelay(2);
			continue;
		}
		if(buf[0]!=0) break;
		mdelay(2);
	}
	mdelay(1);
	if(buf[0] == 0x01){
		memset(buf,0,sizeof(buf));
		ret = hyn_wr_reg(hyn_3xxdata,0xA008,2,buf,4);
		if(0 == ret){
			*check_sum = U8TO32(buf[3],buf[2],buf[1],buf[0]);
			return 0;
		}
	}
	return -1;
}

static int cst3xx_updata_judge(u8 *p_fw, u16 len)
{
	u32 f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
	u8 *p_data = p_fw + len- 16;
	struct tp_info *ic = &hyn_3xxdata->hw_info;
	f_fw_project_id = U8TO16(p_data[1],p_data[0]);
	f_ictype = U8TO16(p_data[3],p_data[2]);
	f_fw_ver = U8TO16(p_data[7],p_data[6]);
	f_fw_ver = (f_fw_ver<<16)|U8TO16(p_data[5],p_data[4]);
	f_checksum = U8TO16(p_data[11],p_data[10]);
	f_checksum = (f_checksum << 16)|U8TO16(p_data[9],p_data[8]);
	HYN_INFO("Bin_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);

	cst3xx_updata_tpinfo();
	cst3xx_set_workmode(NOMAL_MODE,1);

	if(f_ictype != ic->fw_chip_type || f_fw_project_id != ic->fw_project_id){
		return 0; //not updata
	}
	if( hyn_3xxdata->boot_is_pass ==0	//boot failed
		||(f_checksum != ic->ic_fw_checksum && f_fw_ver >= ic->fw_ver)){
		return 1; //need updata
	}
	return 0;
}

static int cst3xx_updata_fw(u8 *bin_addr, u16 len)
{
	#define CHECKSUM_OFFECT  (24*1024+16)
	int i,ret, retry = 4;
	u8 *i2c_buf;
	u16 eep_addr = 0, total_kbyte = 24;
	u32 fw_checksum= 0;
	HYN_ENTER();
	i2c_buf = kmalloc(1024 + 2, GFP_KERNEL);
	if(0 == hyn_3xxdata->fw_file_name[0]){
		fw_checksum = U8TO32(bin_addr[CHECKSUM_OFFECT+3],bin_addr[CHECKSUM_OFFECT+2],bin_addr[CHECKSUM_OFFECT+1],bin_addr[CHECKSUM_OFFECT]);
	}
	else{
		ret = hyn_copy_for_updata(hyn_3xxdata,i2c_buf,CHECKSUM_OFFECT,4);
		if(ret)  goto UPDATA_END;
		fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
	}
	hyn_irq_set(hyn_3xxdata,DISABLE);
	while(--retry){
		ret = cst3xx_enter_boot();
		if(ret){
			HYN_ERROR("cst3xx_enter_boot fail");
			continue;
		}
		ret = hyn_wr_reg(hyn_3xxdata,0xA00200,3,i2c_buf,0);
		if(ret) continue;
		mdelay(5);
		ret = hyn_wr_reg(hyn_3xxdata,0xA003,2,i2c_buf,1);
		if(i2c_buf[0] != 0x55)continue;
		//start trans fw
		for (i=0; i<total_kbyte; i++) {
			i2c_buf[0] = 0xA0;
			i2c_buf[1] = 0x14;
			eep_addr = i << 10;		//i * 1024
			i2c_buf[2] = eep_addr;
			i2c_buf[3] = eep_addr>>8;
			hyn_write_data(hyn_3xxdata, i2c_buf,RW_REG_LEN, 4);

			i2c_buf[0] = 0xA0;
			i2c_buf[1] = 0x18;
			if(0 == hyn_3xxdata->fw_file_name[0]){
				memcpy(i2c_buf + 2, bin_addr + eep_addr, 1024);
			}
			else{
				hyn_copy_for_updata(hyn_3xxdata,i2c_buf + 2,eep_addr,1024);
			}
			hyn_write_data(hyn_3xxdata, i2c_buf,RW_REG_LEN, 1026);

			hyn_wr_reg(hyn_3xxdata, 0xA004EE, 3,i2c_buf,0);
			mdelay(60); //wait finsh

			i2c_buf[0] = 0xA0;
			i2c_buf[1] = 0x05;
			ret = hyn_wr_reg(hyn_3xxdata,0xA005,2,i2c_buf,1);
			if (ret < 0 || i2c_buf[0] != 0x55){
				ret = -1;
				break;
			}
		}
		if(ret) continue;
		ret = hyn_wr_reg(hyn_3xxdata,0xA00300,3,i2c_buf,0);
		if(ret) continue;
		cst3xx_read_checksum(&hyn_3xxdata->hw_info.ic_fw_checksum);
		if(fw_checksum == hyn_3xxdata->hw_info.ic_fw_checksum){
			break;
		}
	}
UPDATA_END:
	hyn_set_i2c_addr(hyn_3xxdata,MAIN_I2C_ADDR);
	cst3xx_rst();
	if(fw_checksum == hyn_3xxdata->hw_info.ic_fw_checksum){
		mdelay(50);
		cst3xx_updata_tpinfo();
		HYN_INFO("updata_fw success");
	}
	else{
		HYN_ERROR("updata_fw failed fw_checksum:%#x ic_checksum:%#x",fw_checksum,hyn_3xxdata->hw_info.ic_fw_checksum);
	}
	hyn_irq_set(hyn_3xxdata,ENABLE);

	kfree(i2c_buf);
	return !retry;
}

static u32 cst3xx_check_esd(void)
{
	int ret = 0;
	u8 buf[6];
	ret = hyn_wr_reg(hyn_3xxdata,0xD040,2,buf,6);
	if(ret ==0){
		u16 checksum = buf[0]+buf[1]+buf[2]+buf[3]+0xA5;
		if(checksum != ( (buf[4]<<8)+ buf[5])){
			ret = -1;
		}
	}
	return ret ? 0:(buf[3]+(buf[2]<<8)+(buf[1]<<16)+(buf[0]<<24));
}

static int cst3xx_get_dbg_data(u8 *buf, u16 len)
{
	return 0;
}

static int cst3xx_get_test_result(u8 *buf, u16 len)
{
	return 0;
}


const struct hyn_ts_fuc hyn_cst3xx_fuc = {
	.tp_rest = cst3xx_rst,
	.tp_report = cst3xx_report,
	.tp_supend = cst3xx_supend,
	.tp_resum = cst3xx_resum,
	.tp_chip_init = cst3xx_init,
	.tp_updata_fw = cst3xx_updata_fw,
	.tp_set_workmode = cst3xx_set_workmode,
	.tp_check_esd = cst3xx_check_esd,
	.tp_prox_handle = cst3xx_prox_handle,
	.tp_get_dbg_data = cst3xx_get_dbg_data,
	.tp_get_test_result = cst3xx_get_test_result
};
