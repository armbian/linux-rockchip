/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_chips/hyn_cst226se.c
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
#include "cst226se_fw.h"


#define BOOT_I2C_ADDR   (0x5A)
#define MAIN_I2C_ADDR   (0x5A)

#define RW_REG_LEN   (2)

#define CST226SE_BIN_SIZE	(7*1024+512)

static struct hyn_ts_data *hyn_226data = NULL;
static const u8 gest_map_tbl[33] = {0xff,4,1,3,2,5,12,6,7,7,9,11,10,13,12,7,7,6,10,6,5,0xff,0xff,
				    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,14};

static int cst226se_updata_judge(u8 *p_fw, u16 len);
static u32 cst226se_read_checksum(void);
static int cst226se_updata_tpinfo(void);
static int cst226se_enter_boot(void);
static int cst226se_set_workmode(enum work_mode mode,u8 enable);
static void cst226se_rst(void);

static int cst226se_init(struct hyn_ts_data* ts_data)
{
	int ret = 0;
	u8 buf[4];
	HYN_ENTER();
	hyn_226data = ts_data;
	ret = cst226se_enter_boot();
	if(ret){
		HYN_ERROR("cst226se_enter_boot failed");
		return -1;
	}
	hyn_226data->fw_updata_addr = (u8*)fw_bin;
	hyn_226data->fw_updata_len = CST226SE_BIN_SIZE;
	hyn_226data->hw_info.ic_fw_checksum = cst226se_read_checksum();
	hyn_wr_reg(hyn_226data,0xA006EE,3,buf,0); //exit boot
	cst226se_rst();
	mdelay(50);
	hyn_set_i2c_addr(hyn_226data,MAIN_I2C_ADDR);
	hyn_226data->need_updata_fw = cst226se_updata_judge((u8*)fw_bin,CST226SE_BIN_SIZE);
	if(hyn_226data->need_updata_fw){
		HYN_INFO("need updata FW !!!");
	}
	return 0;
}

static int cst226se_report(void)
{
	u8 buf[80]={0};
	u8 finger_num = 0,key_flg = 0,tmp_dat;
	int len = 0;
	int ret = 0,retry = 2;
	switch(hyn_226data->work_mode){
		case NOMAL_MODE:
			retry = 2;
			while(retry--){
				ret = hyn_wr_reg(hyn_226data,0xD000,2,buf,7);
				finger_num = buf[5] & 0x7F;
				if(ret || buf[6] != 0xAB || buf[0] == 0xAB || finger_num > MAX_POINTS_REPORT){
					ret = -2;
					continue;
				}
				key_flg = (buf[5]&0x80) ? 1:0;
				len = 0;
				if(finger_num > 1){
					len += (finger_num-1)*5;
				}
				if(key_flg && finger_num){
					len += 3;
				}
				if(len > 0){
					ret = hyn_wr_reg(hyn_226data,0xD007,2,&buf[5],len);
				}
				ret |= hyn_wr_reg(hyn_226data,0xD000AB,3,buf,0);
				if(ret){
					ret = -3;
					continue;
				}
				ret = 0;
				break;
			}
			if(ret){
				hyn_wr_reg(hyn_226data,0xD000AB,3,buf,0);
				HYN_ERROR("read frame failed");
				break;
			}
			if(key_flg){ //key
				if(hyn_226data->rp_buf.report_need == REPORT_NONE){
					hyn_226data->rp_buf.report_need |= REPORT_KEY;
				}
				len = finger_num ? (len+5):3;
				hyn_226data->rp_buf.key_id = (buf[len-2]>>4)-1;
				hyn_226data->rp_buf.key_state = (buf[len-3]&0x0F)==0x03 ? 1:0;
				HYN_INFO("key_id:%x state:%x",hyn_226data->rp_buf.key_id ,hyn_226data->rp_buf.key_state);
			}
			if(finger_num){
				u16 index = 0,i = 0;
				u8 touch_down = 0;
				if(hyn_226data->rp_buf.report_need == REPORT_NONE){
					hyn_226data->rp_buf.report_need |= REPORT_POS;
				}
				hyn_226data->rp_buf.rep_num = finger_num;
				for(i = 0; i < finger_num; i++){
					index = i*5;
					hyn_226data->rp_buf.pos_info[i].pos_id =  (buf[index]>>4)&0x0F;
					hyn_226data->rp_buf.pos_info[i].event =  (buf[index]&0x0F) == 0x06 ? 1 : 0;
					hyn_226data->rp_buf.pos_info[i].pos_x = ((u16)buf[index + 1]<<4) + ((buf[index + 3] >> 4) & 0x0F);
					hyn_226data->rp_buf.pos_info[i].pos_y = ((u16)buf[index + 2]<<4) + (buf[index + 3] & 0x0F);
					hyn_226data->rp_buf.pos_info[i].pres_z = buf[index + 4];
					if(hyn_226data->rp_buf.pos_info[i].event) touch_down++;
					// HYN_INFO("report_id = %d, xy = %d,%d",hyn_226data->rp_buf.pos_info[i].pos_id,hyn_226data->rp_buf.pos_info[i].pos_x,hyn_226data->rp_buf.pos_info[i].pos_y);
				}
				if(0== touch_down){
					hyn_226data->rp_buf.rep_num = 0;
				}
			}
			break;
		case GESTURE_MODE:
			ret = hyn_wr_reg(hyn_226data,0xD04C,2,&tmp_dat,1);
			if((tmp_dat&0x7F) <= 32){
				tmp_dat = tmp_dat&0x7F;
				hyn_226data->gesture_id = gest_map_tbl[tmp_dat];
				hyn_226data->rp_buf.report_need |= REPORT_GES;
			}
			break;
		default:
			break;
	}
	return ret;
}


static int cst226se_prox_handle(u8 cmd)
{
	int ret = 0;
	HYN_ENTER();
	switch(cmd){
		case 1: //enable
			hyn_226data->prox_is_enable = 1;
			hyn_226data->prox_state = 0;
			ret = hyn_wr_reg(hyn_226data,0xD004B01,3,NULL,0);
			break;
		case 0: //disable
			hyn_226data->prox_is_enable = 0;
			hyn_226data->prox_state = 0;
			ret = hyn_wr_reg(hyn_226data,0xD004B00,3,NULL,0);
			break;
		case 2: //read
			ret = hyn_wr_reg(hyn_226data,0xD004B,2,&hyn_226data->prox_state,1);
			break;
	}
	return ret;
}

static int cst226se_set_workmode(enum work_mode mode,u8 enable)
{
	int ret = 0;
	HYN_ENTER();
	hyn_226data->work_mode = mode;
	if(mode != NOMAL_MODE)
		hyn_esdcheck_switch(hyn_226data,DISABLE);
	switch(mode){
		case NOMAL_MODE:
			hyn_irq_set(hyn_226data,ENABLE);
			hyn_esdcheck_switch(hyn_226data,enable);
			hyn_wr_reg(hyn_226data,0xD10B,2,NULL,0); //soft rst
			hyn_wr_reg(hyn_226data,0xD109,2,NULL,0);
			break;
		case GESTURE_MODE:
			hyn_wr_reg(hyn_226data,0xD04C80,3,NULL,0);
			break;
		case LP_MODE:
			break;
		case DIFF_MODE:
			hyn_wr_reg(hyn_226data,0xD10B,2,NULL,0);
			hyn_wr_reg(hyn_226data,0xD10D,2,NULL,0);
			break;
		case RAWDATA_MODE:
			hyn_wr_reg(hyn_226data,0xD10B,2,NULL,0);
			hyn_wr_reg(hyn_226data,0xD10A,2,NULL,0);
			break;
		case FAC_TEST_MODE:
			hyn_wr_reg(hyn_226data,0xD10B,2,NULL,0);
			hyn_wr_reg(hyn_226data,0xD119,2,NULL,0);
			msleep(50); //wait  switch to fac mode
			break;
		case DEEPSLEEP:
			hyn_irq_set(hyn_226data,DISABLE);
			hyn_wr_reg(hyn_226data,0xD105,2,NULL,0);
			break;
		case ENTER_BOOT_MODE:
			ret |= cst226se_enter_boot();
			break;
		default :
			hyn_esdcheck_switch(hyn_226data,ENABLE);
			hyn_226data->work_mode = NOMAL_MODE;
			break;
	}
	return ret;
}


static int cst226se_supend(void)
{
	HYN_ENTER();
	cst226se_set_workmode(DEEPSLEEP,0);
	return 0;
}

static int cst226se_resum(void)
{
	HYN_ENTER();
	cst226se_rst();
	msleep(50);
	cst226se_set_workmode(NOMAL_MODE,0);
	return 0;
}

static void cst226se_rst(void)
{
	if(hyn_226data->work_mode==ENTER_BOOT_MODE){
		hyn_set_i2c_addr(hyn_226data,MAIN_I2C_ADDR);
	}
	gpiod_direction_output(hyn_226data->plat_data.reset_gpio,0);
	msleep(10);
	gpiod_direction_output(hyn_226data->plat_data.reset_gpio,1);
}

static int cst226se_enter_boot(void)
{
	int retry = 5,ret = 0;
	u8 buf[4] = {0};
	hyn_set_i2c_addr(hyn_226data,BOOT_I2C_ADDR);
	while(++retry<17){
		cst226se_rst();
		mdelay(retry);
		ret = hyn_wr_reg(hyn_226data,0xA001AA,3,buf,0);
		if(ret != 0){
			continue;
		}
		ret = hyn_wr_reg(hyn_226data,0xA003,2,buf,1);
		if(ret == 0 &&  buf[0] == 0x55){
			return 0;
		}
	}
	return -1;
}

static int cst226se_updata_tpinfo(void)
{
	u8 buf[28];
	struct tp_info *ic = &hyn_226data->hw_info;
	int ret = 0,retry = 5;
	while(--retry){
		ret = hyn_wr_reg(hyn_226data,0xD101,2,buf,0);
		mdelay(1);
		ret |= hyn_wr_reg(hyn_226data,0xD1F4,2,buf,28);
		cst226se_set_workmode(NOMAL_MODE,0);
		if(ret ==0 &&  U8TO16(buf[19],buf[18])==0x00a8){
			break;
		}
		msleep(1);
	}

	if(ret || retry==0){
		HYN_ERROR("cst226se_updata_tpinfo failed");
		return -1;
	}
	ic->fw_sensor_txnum = buf[0];
	ic->fw_sensor_rxnum = buf[2];
	ic->fw_key_num = buf[3];
	ic->fw_res_y = (buf[7]<<8)|buf[6];
	ic->fw_res_x = (buf[5]<<8)|buf[4];
	ic->fw_project_id = (buf[17]<<8)|buf[16];
	ic->fw_chip_type = U8TO16(buf[19],buf[18]);
	ic->fw_ver = (buf[23]<<24)|(buf[22]<<16)|(buf[21]<<8)|buf[20];

	HYN_INFO("IC_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);
	return 0;
}

static u32 cst226se_read_checksum(void)
{
	int ret;
	u8 buf[4],retry = 5;
	hyn_226data->boot_is_pass = 0;
	while(retry--){
		ret = hyn_wr_reg(hyn_226data,0xA000,2,buf,1);
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
		hyn_wr_reg(hyn_226data,0xA008,2,buf,4);
		hyn_226data->boot_is_pass = 1;
	}
	return U8TO32(buf[3],buf[2],buf[1],buf[0]);
}

static int cst226se_updata_judge(u8 *p_fw, u16 len)
{
	u32 f_check_all,f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
	int ret = 0;
	u8 *p_data = p_fw +7680-12;
	struct tp_info *ic = &hyn_226data->hw_info;

	f_fw_project_id = U8TO16(p_data[1],p_data[0]);
	f_ictype = U8TO16(p_data[3],p_data[2]);

	f_fw_ver = U8TO32(p_data[7],p_data[6],p_data[5],p_data[4]);
	f_checksum = U8TO32(p_data[11],p_data[10],p_data[9],p_data[8]);

	HYN_INFO("Bin_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);

	ret = cst226se_updata_tpinfo();
	if(ret)HYN_ERROR("get tpinfo failed");

	//check h file
	f_check_all = hyn_sum32(0x55,(u32*)p_fw,(len-4)/4);
	if(f_check_all != f_checksum){
		HYN_INFO(".h file checksum erro:%04x len:%d",f_check_all,len);
		return 0; //not updata
	}
	if(hyn_226data->boot_is_pass ==0	//emty chip
	||(hyn_226data->boot_is_pass && ret) //erro code
	|| ( ret == 0 && f_checksum != ic->ic_fw_checksum  && f_fw_ver >= ic->fw_ver ) // match new ver .h file
	){
		return 1; //need updata
	}
	return 0;
}

static int cst226se_updata_fw(u8 *bin_addr, u16 len)
{
	#define CHECKSUM_OFFECT  (7680-4)
	int i,ret, retry = 4;
	u8 i2c_buf[512+2];
	u16 eep_addr = 0, total_kbyte = len/512;
	u32 fw_checksum = 0;
	HYN_ENTER();
	if(0 == hyn_226data->fw_file_name[0]){
		fw_checksum = U8TO32(bin_addr[CHECKSUM_OFFECT+3],bin_addr[CHECKSUM_OFFECT+2],bin_addr[CHECKSUM_OFFECT+1],bin_addr[CHECKSUM_OFFECT]);
	}
	else{
		ret = hyn_copy_for_updata(hyn_226data,i2c_buf,CHECKSUM_OFFECT,4);
		if(ret)  goto UPDATA_END;
		fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
	}

	hyn_irq_set(hyn_226data,DISABLE);
	while(--retry){
		ret = cst226se_enter_boot();
		if(ret){
			HYN_ERROR("cst226se_enter_boot fail");
			continue;
		}
		//start trans fw
		for (i=0; i<total_kbyte; i++) {
			i2c_buf[0] = 0xA0;
			i2c_buf[1] = 0x14;
			eep_addr = i << 9;		//i * 512
			i2c_buf[2] = eep_addr;
			i2c_buf[3] = eep_addr>>8;
			ret = hyn_write_data(hyn_226data, i2c_buf,RW_REG_LEN, 4);
			i2c_buf[0] = 0xA0;
			i2c_buf[1] = 0x18;
			if(0 == hyn_226data->fw_file_name[0]){
				memcpy(i2c_buf + 2, bin_addr + eep_addr, 512);
			}
			else{
				ret |= hyn_copy_for_updata(hyn_226data,i2c_buf + 2,eep_addr,512);
			}
			ret |= hyn_write_data(hyn_226data, i2c_buf,RW_REG_LEN, 514);
			ret |= hyn_wr_reg(hyn_226data, 0xA004EE, 3,i2c_buf,0);
			msleep(300); //wait finsh
			{
				uint8_t time_out = 10;
				while(--time_out){
					ret = hyn_wr_reg(hyn_226data,0xA005,2,i2c_buf,1);
					if (ret == 0 && i2c_buf[0] == 0x55){
						break;
					}
					msleep(100);
				}
				if(time_out==0){
					ret = -1;
					break;
				}
			}
		}
		if(ret) continue;
		ret = hyn_wr_reg(hyn_226data,0xA00100,3,i2c_buf,0);
		ret |= hyn_wr_reg(hyn_226data,0xA00300,3,i2c_buf,0);
		if(ret) continue;
		hyn_226data->hw_info.ic_fw_checksum = cst226se_read_checksum();
		if(fw_checksum == hyn_226data->hw_info.ic_fw_checksum){
			break;
		}
	}
	UPDATA_END:
	hyn_set_i2c_addr(hyn_226data,MAIN_I2C_ADDR);
	cst226se_rst();
	if(fw_checksum == hyn_226data->hw_info.ic_fw_checksum){
		mdelay(50);
		cst226se_updata_tpinfo();
		ret = 0;
		HYN_INFO("updata_fw success");
	}
	else{
		ret = -1;
		HYN_ERROR("updata_fw failed fw_checksum:%#x ic_checksum:%#x",fw_checksum,hyn_226data->hw_info.ic_fw_checksum);
	}
	hyn_irq_set(hyn_226data,ENABLE);
	return ret;
}

static u32 cst226se_check_esd(void)
{
	int ret = 0;
	u8 buf[6];
	ret = hyn_wr_reg(hyn_226data,0xD040,2,buf,6);
	if(ret ==0){
		u16 checksum = buf[0]+buf[1]+buf[2]+buf[3]+0xA5;
		if(checksum != ( (buf[4]<<8)+ buf[5])){
			ret = -1;
		}
	}
	return ret ? 0:(buf[3]+(buf[2]<<8)+(buf[1]<<16)+(buf[0]<<24));
}

static int cst226se_get_dbg_data(u8 *buf, u16 len)
{
	int ret = -1,timeout=0;
	u16 read_len = (hyn_226data->hw_info.fw_sensor_txnum * hyn_226data->hw_info.fw_sensor_rxnum)*2;
	u16 total_len = read_len + (hyn_226data->hw_info.fw_sensor_txnum + hyn_226data->hw_info.fw_sensor_rxnum*2 + hyn_226data->hw_info.fw_key_num)*2;
	HYN_ENTER();
	if(total_len > len){
		HYN_ERROR("buf too small");
		return -1;
	}
	timeout = 500;
	while(--timeout){ //wait rise edge
		if(gpiod_get_value(hyn_226data->plat_data.irq_gpio)==1) break;
		mdelay(1);
	}
	switch(hyn_226data->work_mode){
		case DIFF_MODE:
		case RAWDATA_MODE:
			{
				u8 temp[10*16],*ptr;
				u16 index = 0,*ptr_dif = (u16*)buf;
				ret = hyn_wr_reg(hyn_226data,0x8001,2,buf,total_len); //mt  //rx scap  //tx scap
				memcpy(temp,buf,read_len/2);
				ptr = &buf[read_len/2];
				for(index = 0;index < read_len/2; index++){
					*ptr_dif = ((*ptr<<8)+temp[index]);
					ptr++;
					ptr_dif++;
				}
			}
			break;
		default:
			HYN_ERROR("work_mode:%d",hyn_226data->work_mode);
			break;
	}
	return ret==0 ? total_len:-1;
}


#define FACTEST_PATH	"/sdcard/hyn_fac_test_cfg.ini"
#define FACTEST_LOG_PATH "/sdcard/hyn_fac_test.log"
#define FACTEST_ITEM	  (MULTI_OPEN_TEST|MULTI_SHORT_TEST)
static int cst226se_get_test_result(u8 *buf, u16 len)
{
	int ret = 0,timeout;
	struct tp_info *ic = &hyn_226data->hw_info;
	u16 scap_len = (ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*2;
	u16 mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum*2;

	HYN_ENTER();
	if((mt_len*3 + scap_len) > len || mt_len==0){
		HYN_ERROR("buf too small");
		return FAC_GET_DATA_FAIL;
	}
	msleep(1);
	timeout = 500;
	while(--timeout){ //wait rise edge
		if(gpiod_get_value(hyn_226data->plat_data.irq_gpio)==1) break;
		msleep(10);
	}
	if(hyn_wr_reg(hyn_226data,0x1215,2,buf,mt_len*2+scap_len+4)){ //open high
		ret = FAC_GET_DATA_FAIL;
		HYN_ERROR("read open high failed");
		goto selftest_end;
	}
	{
		u16 *test_high = (u16*)&buf[0],*test_low = (u16*)&buf[mt_len],*test_short = (u16*)&buf[mt_len*2],w_tmp = 0,i=0;
		for(i=0; i< mt_len/2; i++){
			w_tmp = *test_low;
			*test_low = *test_high;
			*test_high = w_tmp;
			test_high++;
			test_low++;
		}

		for(i = 0; i< ic->fw_sensor_rxnum+ic->fw_sensor_txnum; i++){
			*test_short = U16REV((u16)*test_short);
			test_short++;
		}
	}

	//read data finlish start test
	ret = hyn_factory_multitest(hyn_226data ,FACTEST_PATH, buf,(s16*)(buf+scap_len+mt_len*2),FACTEST_ITEM);

selftest_end:
	if(0 == hyn_fac_test_log_save(FACTEST_LOG_PATH,hyn_226data,(s16*)buf,ret,FACTEST_ITEM)){
		HYN_INFO("fac_test log save success");
	}
	cst226se_resum();
	return ret;
}


const struct hyn_ts_fuc hyn_cst226se_fuc = {
	.tp_rest = cst226se_rst,
	.tp_report = cst226se_report,
	.tp_supend = cst226se_supend,
	.tp_resum = cst226se_resum,
	.tp_chip_init = cst226se_init,
	.tp_updata_fw = cst226se_updata_fw,
	.tp_set_workmode = cst226se_set_workmode,
	.tp_check_esd = cst226se_check_esd,
	.tp_prox_handle = cst226se_prox_handle,
	.tp_get_dbg_data = cst226se_get_dbg_data,
	.tp_get_test_result = cst226se_get_test_result
};
