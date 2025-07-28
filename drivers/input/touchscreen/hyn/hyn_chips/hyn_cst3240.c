/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_chips/hyn_cst3240.c
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
#include "cst3240_fw.h"

#define BOOT_I2C_ADDR   (0x5A)
#define MAIN_I2C_ADDR   (0x5A)
#define RW_REG_LEN   (2)
#define MODULE_ID_ADDR	  (0x3FFC)
#define CST3240_BIN_SIZE   (31*512 + 480)

static struct hyn_ts_data *hyn_3240data = NULL;
static const u8 gest_map_tbl[] = {
	IDX_NULL,	IDX_RIGHT,	IDX_UP,	  IDX_LEFT,
	IDX_DOWN,	IDX_O,		IDX_C,	   IDX_e,
	IDX_M,	   IDX_M,		IDX_W,	   IDX_V,
	IDX_S,	   IDX_Z,		IDX_C,	   IDX_M,
	IDX_M,	   IDX_e,		IDX_S,	   IDX_e,
	IDX_O,	   IDX_NULL,	 IDX_NULL,	IDX_NULL,
	IDX_NULL,	IDX_NULL,	 IDX_NULL,	IDX_NULL,
	IDX_NULL,	IDX_NULL,	 IDX_NULL,	IDX_NULL,
	IDX_POWER
};

static int cst3240_updata_judge(u8 *p_fw, u16 len);
static u32 cst3240_read_checksum(void);
static int cst3240_updata_tpinfo(void);
static int cst3240_enter_boot(void);
static u32 cst3240_fread_word(u32 addr);
static void cst3240_rst(void);


static const struct hyn_chip_series hyn_3240_fw[] = {
	{0x3240,0x00,"cst32401",(u8*)fw_bin},//default fw
	{0x3240,0x01,"cst32402",(u8*)fw_bin},
	{0x3240,0x02,"cst32403",(u8*)fw_bin},
	{0,0,"",NULL}
};

static int cst3240_init(struct hyn_ts_data* ts_data)
{
	int ret = 0,i;
	u32 module_id;
	HYN_ENTER();
	hyn_3240data = ts_data;
	ret = cst3240_enter_boot();
	if(ret){
		HYN_INFO("cst3240_enter_boot failed");
		return -1;
	}
	module_id = cst3240_fread_word(MODULE_ID_ADDR);
	hyn_3240data->fw_updata_addr = hyn_3240_fw[0].fw_bin;

	for(i = 0; ;i++){
		if( hyn_3240_fw[i].moudle_id == module_id){
			hyn_3240data->fw_updata_addr = hyn_3240_fw[i].fw_bin;
			HYN_INFO("chip %s match fw success",hyn_3240_fw[i].chip_name);
			break;
		}
		if(hyn_3240_fw[i].part_no == 0 && hyn_3240_fw[i].moudle_id == 0){
			HYN_INFO("unknown chip or unknown moudle_id use hyn_3240_fw[0]");
			break;
		}
	}
	hyn_3240data->hw_info.fw_module_id = module_id;
	hyn_3240data->fw_updata_len = CST3240_BIN_SIZE;
	hyn_3240data->hw_info.ic_fw_checksum = cst3240_read_checksum();
	HYN_INFO("curt read checksum:0x%x  module_id:0x%x",hyn_3240data->hw_info.ic_fw_checksum,module_id);
	//hyn_wr_reg(hyn_3240data,0xA006EE,3,buf,0); //exit boot
	hyn_set_i2c_addr(hyn_3240data,MAIN_I2C_ADDR);
	cst3240_rst();
	msleep(50);
	hyn_3240data->need_updata_fw = cst3240_updata_judge((u8*)fw_bin,CST3240_BIN_SIZE);
	return 0;
}

static int cst3240_report(void)
{
	u8 buf[80]={0};
	u8 finger_num = 0,key_flg = 0,tmp_dat;
	int len = 0;
	int ret = 0,retry = 2;
	switch(hyn_3240data->work_mode){
		case NOMAL_MODE:
			retry = 2;
			while(retry--){
				ret = hyn_wr_reg(hyn_3240data,0xD000,2,buf,7);
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
					ret = hyn_wr_reg(hyn_3240data,0xD007,2,&buf[5],len);
				}
				if(ret && retry) continue;
				ret |= hyn_wr_reg(hyn_3240data,0xD000AB,3,buf,0);
				if(ret == 0){
					break;
				}
			}
			if(ret){
				HYN_ERROR("read frame failed");
				break;
			}
			if(key_flg){ //key
				if(hyn_3240data->rp_buf.report_need == REPORT_NONE){
					hyn_3240data->rp_buf.report_need |= REPORT_KEY;
				}
				len = finger_num ? (len+5):3;
				hyn_3240data->rp_buf.key_id = (buf[len-2]>>4)-1;
				hyn_3240data->rp_buf.key_state = (buf[len-3]&0x0F)==0x03 ? 1:0;
				HYN_INFO("key_id:%x state:%x",hyn_3240data->rp_buf.key_id ,hyn_3240data->rp_buf.key_state);
			}
			if(finger_num){
				u16 index = 0,i = 0;
				u8 touch_down = 0;
				if(hyn_3240data->rp_buf.report_need == REPORT_NONE){
					hyn_3240data->rp_buf.report_need |= REPORT_POS;
				}
				hyn_3240data->rp_buf.rep_num = finger_num;
				for(i = 0; i < finger_num; i++){
					index = i*5;
					hyn_3240data->rp_buf.pos_info[i].pos_id =  (buf[index]>>4)&0x0F;
					hyn_3240data->rp_buf.pos_info[i].event =  (buf[index]&0x0F) == 0x06 ? 1 : 0;
					hyn_3240data->rp_buf.pos_info[i].pos_x = ((u16)buf[index + 1]<<4) + ((buf[index + 3] >> 4) & 0x0F);
					hyn_3240data->rp_buf.pos_info[i].pos_y = ((u16)buf[index + 2]<<4) + (buf[index + 3] & 0x0F);
					hyn_3240data->rp_buf.pos_info[i].pres_z = buf[index + 4];
					if(hyn_3240data->rp_buf.pos_info[i].event) touch_down++;
					// HYN_INFO("report_id = %d, xy = %d,%d",hyn_3240data->rp_buf.pos_info[i].pos_id,hyn_3240data->rp_buf.pos_info[i].pos_x,hyn_3240data->rp_buf.pos_info[i].pos_y);
				}
				if(0== touch_down){
					hyn_3240data->rp_buf.rep_num = 0;
				}
			}
			break;
		case GESTURE_MODE:
			ret = hyn_wr_reg(hyn_3240data,0xD04C,2,&tmp_dat,1);
			if((tmp_dat&0x7F) <= 32){
				tmp_dat = tmp_dat&0x7F;
				hyn_3240data->gesture_id = gest_map_tbl[tmp_dat];
				hyn_3240data->rp_buf.report_need |= REPORT_GES;
			}
			break;
		default:
			break;
	}
	return ret;
}

static int cst3240_prox_handle(u8 cmd)
{
	/*int ret = 0;
	switch(cmd){
		case 1: //enable
			hyn_3240data->prox_is_enable = 1;
			hyn_3240data->prox_state = 0;
			ret = hyn_wr_reg(hyn_3240data,0xD004B01,3,NULL,0);
			break;
		case 0: //disable
			hyn_3240data->prox_is_enable = 0;
			hyn_3240data->prox_state = 0;
			ret = hyn_wr_reg(hyn_3240data,0xD004B00,3,NULL,0);
			break;
		case 2: //read
			ret = hyn_wr_reg(hyn_3240data,0xD004B,2,&hyn_3240data->prox_state,1);
			break;
	}
	return ret;*/
	return 0;
}

static int cst3240_set_workmode(enum work_mode mode,u8 enable)
{
	int ret = 0;
	HYN_ENTER();
	hyn_3240data->work_mode = mode;
	if(mode != NOMAL_MODE)
		hyn_esdcheck_switch(hyn_3240data,DISABLE);
	switch(mode){
		case NOMAL_MODE:
			hyn_irq_set(hyn_3240data,ENABLE);
			hyn_esdcheck_switch(hyn_3240data,enable);
			ret |= hyn_wr_reg(hyn_3240data,0xD109,2,0,0);
			break;
		case GESTURE_MODE:
			ret |= hyn_wr_reg(hyn_3240data,0xD104,2,0,0);
			break;
		case LP_MODE:
			break;
		case DIFF_MODE:
			ret |= hyn_wr_reg(hyn_3240data,0xD10D,2,0,0);
			ret |= hyn_wr_reg(hyn_3240data,0x000500,3,0,0);
			break;
		case RAWDATA_MODE:
			ret |= hyn_wr_reg(hyn_3240data,0xD10A,2,0,0);
			ret |= hyn_wr_reg(hyn_3240data,0x000500,3,0,0);
			break;
		case FAC_TEST_MODE:
			cst3240_rst();
			msleep(50);
			ret |= hyn_wr_reg(hyn_3240data,0xD119,2,0,0);
			msleep(50); //wait  switch to fac mode
			break;
		case DEEPSLEEP:
			hyn_irq_set(hyn_3240data,DISABLE);
			ret |= hyn_wr_reg(hyn_3240data,0xD105,2,0,0);
			break;
		case ENTER_BOOT_MODE:
			ret |= cst3240_enter_boot();
			break;
		default :
			ret = -2;
			break;
	}
	return ret;
}


static int cst3240_supend(void)
{
	HYN_ENTER();
	cst3240_set_workmode(DEEPSLEEP,0);
	return 0;
}

static int cst3240_resum(void)
{
	HYN_ENTER();
	cst3240_rst();
	msleep(50);
	cst3240_set_workmode(NOMAL_MODE,1);
	return 0;
}

static void cst3240_rst(void)
{
	if(hyn_3240data->work_mode==ENTER_BOOT_MODE){
		hyn_set_i2c_addr(hyn_3240data,MAIN_I2C_ADDR);
	}
	gpiod_direction_output(hyn_3240data->plat_data.reset_gpio,0);
	msleep(8);
	gpiod_direction_output(hyn_3240data->plat_data.reset_gpio,1);
}

static int cst3240_enter_boot(void)
{
	int retry = 0,ret = 0,times;
	u8 buf[4];
	hyn_set_i2c_addr(hyn_3240data,BOOT_I2C_ADDR);
	while(++retry<20){
		cst3240_rst();
		mdelay(5+retry);
		ret = hyn_wr_reg(hyn_3240data,0xA001AA,3,0,0);
		if(ret) continue;
		times = 5;
		while(--times){
			ret = hyn_wr_reg(hyn_3240data,0xA002,2,buf,2);
			if(ret ==0 && buf[0] == 0x55 && buf[1] == 0xA8) break;
			mdelay(1);
		}
		if(times == 0) continue;
		ret = hyn_wr_reg(hyn_3240data,0xA00100,3,0,0);
		if(ret) continue;
		break;
	}
	return retry < 20 ? 0:-1;
}

static int cst3240_updata_tpinfo(void)
{
	u8 buf[28];
	struct tp_info *ic = &hyn_3240data->hw_info;
	int ret = 0,retry = 5;
	HYN_ENTER();
	while(--retry){
		ret = hyn_wr_reg(hyn_3240data,0xD101,2,0,0); //get module info mode
		ret |= hyn_wr_reg(hyn_3240data,0xD204,2,buf,8);
		ret |= hyn_wr_reg(hyn_3240data,0xD1F4,2,&buf[8],8);
		ret |= cst3240_set_workmode(NOMAL_MODE,ENABLE);
		if(ret==0 && U8TO16(buf[3],buf[2])==0x3240)break;
		mdelay(1);
	}

	if(ret || retry==0){
		HYN_ERROR("cst3240_updata_tpinfo failed");
		return -1;
	}

	ic->fw_project_id = U8TO16(buf[1],buf[0]);
	ic->fw_chip_type = U8TO16(buf[3],buf[2]);
	ic->fw_ver = U8TO32(buf[7],buf[6],buf[5],buf[4]);

	ic->fw_sensor_txnum = buf[8];
	ic->fw_sensor_rxnum = buf[10];
	ic->fw_key_num = buf[11];
	ic->fw_res_y = U8TO16(buf[15],buf[14]);
	ic->fw_res_x = U8TO16(buf[13],buf[12]);

	HYN_INFO("IC_info tx:%d rx:%d key:%d res-x:%d res-y:%d",ic->fw_sensor_txnum,ic->fw_sensor_rxnum,ic->fw_key_num,ic->fw_res_x,ic->fw_res_y);
	HYN_INFO("IC_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);
	return 0;
}

static u32 cst3240_fread_word(u32 addr)
{
	int ret;
	u8 rec_buf[4],retry;
	u32 read_word = 0;
	retry = 3;
	while(retry--){
		ret = hyn_write_data(hyn_3240data,(u8[]){0xA0,0x18,0x00,0x00,0x00,0x00},2,6);
		ret  |= hyn_wr_reg(hyn_3240data,0xA01000,3,0,0); //set section
		ret |= hyn_wr_reg(hyn_3240data,U8TO32(0xA0,0x0c,(addr&0xFF),((addr>>8)&0xFF)),4,NULL,0); //set addr
		ret  |= hyn_wr_reg(hyn_3240data,0xA004E4,3,NULL,0);	//trig read
		if(ret ==0) break;
	}
	if(ret) return 0;

	retry = 10;
	while(retry--){
		mdelay(2);
		ret = hyn_wr_reg(hyn_3240data,0xA018,2,rec_buf,4);
		if(ret==0){
			read_word = U8TO32(rec_buf[3],rec_buf[2],rec_buf[1],rec_buf[0]);
			ret = hyn_wr_reg(hyn_3240data,0xA018,2,rec_buf,4);
			if(ret == 0 && read_word==U8TO32(rec_buf[3],rec_buf[2],rec_buf[1],rec_buf[0])){
				break;
			}
		}
	}
	return read_word;
}

static u32 cst3240_read_checksum(void)
{
	int ret;
	u32 check_sum = 0;
	u8 buf[4],retry = 6;
	hyn_3240data->boot_is_pass = 0;
	while(--retry){
		ret = hyn_wr_reg(hyn_3240data,0xA000,2,buf,1);
		if(ret ==0 && buf[0] == 0x01) break;
		mdelay(2);
	}
	if(retry){
		retry = 3;
		while(retry){
			ret = hyn_wr_reg(hyn_3240data,0xA008,2,buf,4);
			if(ret==0){
				check_sum = U8TO32(buf[3],buf[2],buf[1],buf[0]);
				hyn_3240data->boot_is_pass = 1;
				break;
			}
			mdelay(1);
		}
	}
	return check_sum;
}

static int cst3240_updata_judge(u8 *p_fw, u16 len)
{
	int ret;
	u32 f_check_all,f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
	u8 *p_data; // = p_fw + len- 16;
	struct tp_info *ic = &hyn_3240data->hw_info;
	HYN_ENTER();
	p_data = &p_fw[CST3240_BIN_SIZE-16+4];
	f_fw_project_id = U8TO16(p_data[1],p_data[0]);
	f_ictype = U8TO16(p_data[3],p_data[2]);
	f_fw_ver = U8TO32(p_data[7],p_data[6],p_data[5],p_data[4]);
	f_checksum = U8TO32(p_data[11],p_data[10],p_data[9],p_data[8]);

	HYN_INFO("Bin_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);
	ret = cst3240_updata_tpinfo(); //boot checksum pass, communicate failed not updata
	if(ret) HYN_ERROR("get tpinfo failed");

	f_check_all = hyn_sum32(0x55,(u32*)p_fw,len/4-1);
	if(f_check_all != f_checksum){
		HYN_INFO(".h file checksum erro:%04x",f_check_all);
		return 0; //no need updata
	}

	if(hyn_3240data->boot_is_pass ==0	//checksum failed
	|| ( ret == 0 && f_ictype == ic->fw_chip_type && f_fw_project_id == ic->fw_project_id
		&& f_checksum != ic->ic_fw_checksum && f_fw_ver >= ic->fw_ver ) // match new ver .h file
	){
		return 1; //need updata
	}
	return 0;
}

static int cst3240_updata_fw(u8 *bin_addr, u16 len)
{
	int i,ret=-1, times = 0,retry;
	u8 i2c_buf[512+2];
	u16 eep_addr = 0;
	u32 fw_checksum = 0;
	HYN_ENTER();
	if(len < CST3240_BIN_SIZE){
		HYN_ERROR("len = %d",len);
		goto UPDATA_END;
	}
	// if(len > CST3240_BIN_SIZE) len = CST3240_BIN_SIZE;

	if(0 == hyn_3240data->fw_file_name[0]){
		fw_checksum = *(u32*)(bin_addr+CST3240_BIN_SIZE-4);
	}
	else{
		ret = hyn_copy_for_updata(hyn_3240data,i2c_buf,CST3240_BIN_SIZE-4,4);
		if(ret)  goto UPDATA_END;
		fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
	}
	hyn_irq_set(hyn_3240data,DISABLE);
	hyn_esdcheck_switch(hyn_3240data,DISABLE);
	retry = 4;
	while(--retry){
		hyn_3240data->fw_updata_process = 0;
		ret = cst3240_enter_boot();
		if(ret) continue;
		ret = hyn_wr_reg(hyn_3240data,0xA01000,3,0,0);
		if(ret) continue;
		//start trans fw
		for (i=0; i<32;i++) {
			if(31 == i){
				hyn_wr_reg(hyn_3240data, 0xA01002, 3,0,0);
				hyn_wr_reg(hyn_3240data, 0xA00CE001, 4,0,0);
			}
			eep_addr = i << 9; //i * 512
			ret = hyn_wr_reg(hyn_3240data,0xA0140000+U16REV(eep_addr),4,0,0);
			i2c_buf[0] = 0xA0;
			i2c_buf[1] = 0x18;
			if(0 == hyn_3240data->fw_file_name[0]){
				memcpy(i2c_buf + 2, bin_addr + eep_addr, 512);
			}else{
				ret |= hyn_copy_for_updata(hyn_3240data,i2c_buf + 2,eep_addr,512);
			}
			ret |= hyn_write_data(hyn_3240data, i2c_buf,RW_REG_LEN, i<31 ? 514:482);
			if(ret){ //com erro
				continue;
			}
			ret = hyn_wr_reg(hyn_3240data, 0xA004EE, 3,0,0);
			if(ret) ret = hyn_wr_reg(hyn_3240data, 0xA004EE, 3,0,0);
			mdelay((4-retry)*100); //wait finsh
			times = 100;
			while(--times){
				ret = hyn_wr_reg(hyn_3240data,0xA005,2,i2c_buf,1);
				if(ret==0 && i2c_buf[0] == 0x55) break;
				ret = -1;
				mdelay(10);
			}
			if(ret) break;
			hyn_3240data->fw_updata_process = i*100/32;
		}
		// HYN_INFO("ret = %d",ret);
		if(ret) continue;

		ret = hyn_wr_reg(hyn_3240data,0xA00300,3,i2c_buf,0);
		if(ret) continue;
		hyn_3240data->hw_info.ic_fw_checksum = cst3240_read_checksum();
		HYN_INFO("retry:%d fw_checksum:%x ic_checksum:%x\r\n",4-retry,fw_checksum,hyn_3240data->hw_info.ic_fw_checksum);
		if(fw_checksum != hyn_3240data->hw_info.ic_fw_checksum){
			ret = -2;
			continue;
		}
		break;
	}
UPDATA_END:
	hyn_set_i2c_addr(hyn_3240data,MAIN_I2C_ADDR);
	cst3240_rst();
	if(ret == 0){
		msleep(50);
		cst3240_updata_tpinfo();
		hyn_3240data->fw_updata_process = 100;
		HYN_INFO("updata_fw success");
	}
	else{
		hyn_3240data->fw_updata_process |= 0x80;
		HYN_ERROR("updata_fw failed");
	}
	hyn_irq_set(hyn_3240data,ENABLE);
	return ret;
}

static u32 cst3240_check_esd(void)
{
	int ret = 0;
	u8 buf[6];
	HYN_ENTER();
	ret = hyn_wr_reg(hyn_3240data,0xD040,2,buf,6);
	if(ret ==0 && hyn_sum16(0xA5,buf,4)==U8TO16(buf[4],buf[5])){
		ret = U8TO32(buf[0],buf[1],buf[2],buf[3]);
	}
	return ret;
}


static int cst3240_get_dbg_data(u8 *buf, u16 len)
{
	int ret = -1,timeout;
	u16 read_len = (hyn_3240data->hw_info.fw_sensor_txnum * hyn_3240data->hw_info.fw_sensor_rxnum)*2;
	u16 total_len = read_len + (hyn_3240data->hw_info.fw_sensor_txnum + hyn_3240data->hw_info.fw_sensor_rxnum)*2;
	HYN_ENTER();
	if(total_len > len){
		HYN_ERROR("buf too small");
		return -1;
	}
	switch(hyn_3240data->work_mode){
		case DIFF_MODE:
		case RAWDATA_MODE:
			timeout = 60;
			while(--timeout){ //wait rise edge
				if(gpiod_get_value(hyn_3240data->plat_data.irq_gpio)==1) break;
				msleep(1);
			}
			ret = hyn_wr_reg(hyn_3240data,0x1000,2,buf,read_len); //mt

			buf += read_len;
			read_len = hyn_3240data->hw_info.fw_sensor_rxnum*2;
			ret |= hyn_wr_reg(hyn_3240data,0x7000,2,buf,read_len); //rx

			buf += read_len;
			read_len = hyn_3240data->hw_info.fw_sensor_txnum*2;
			ret |= hyn_wr_reg(hyn_3240data,0x7200,2,buf,read_len); //tx

			ret |= hyn_wr_reg(hyn_3240data,0x000500,3,0,0); //end
			break;
		default:
			HYN_ERROR("work_mode:%d",hyn_3240data->work_mode);
			break;
	}
	return ret==0 ? total_len:-1;
}

#define FACTEST_PATH	"/sdcard/hyn_fac_test_cfg.ini"
#define FACTEST_LOG_PATH "/sdcard/hyn_fac_test.log"
#define FACTEST_ITEM	  (MULTI_OPEN_TEST|MULTI_SHORT_TEST)
static int cst3240_get_test_result(u8 *buf, u16 len)
{
	int ret = 0,timeout;
	struct tp_info *ic = &hyn_3240data->hw_info;
	u16 scap_len = (ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*2;
	u16 mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum*2,i = 0;
	u16 *raw_s;

	HYN_ENTER();
	if((mt_len*3 + scap_len) > len || mt_len==0){
		HYN_ERROR("buf too small");
		return FAC_GET_DATA_FAIL;
	}
	msleep(1);
	timeout = 500;
	while(--timeout){ //wait rise edge
		if(gpiod_get_value(hyn_3240data->plat_data.irq_gpio)==1) break;
		msleep(10);
	}
	if(hyn_wr_reg(hyn_3240data,0x3000,2,buf,mt_len)){ //open high
		ret = FAC_GET_DATA_FAIL;
		HYN_ERROR("read open high failed");
		goto selftest_end;
	}
	if(hyn_wr_reg(hyn_3240data,0x1000,2,buf + mt_len,mt_len)){ //open low
		ret = FAC_GET_DATA_FAIL;
		HYN_ERROR("read open low failed");
		goto selftest_end;
	}
	//short test
	if(hyn_wr_reg(hyn_3240data,0x5000,2,buf+(mt_len*2),scap_len)){
		ret = FAC_GET_DATA_FAIL;
		HYN_ERROR("read fac short failed");
		goto selftest_end;
	}
	else{
		raw_s = (u16*)(buf + mt_len*2);
		for(i = 0; i< ic->fw_sensor_rxnum+ic->fw_sensor_txnum; i++){
			*raw_s = U16REV((u16)*raw_s);
			raw_s++;
		}
	}

	//read data finlish start test
	ret = hyn_factory_multitest(hyn_3240data ,FACTEST_PATH, buf,(s16*)(buf+scap_len+mt_len*2),FACTEST_ITEM);

selftest_end:
	if(0 == hyn_fac_test_log_save(FACTEST_LOG_PATH,hyn_3240data,(s16*)buf,ret,FACTEST_ITEM)){
		HYN_INFO("fac_test log save success");
	}
	cst3240_resum();
	return ret;
}


const struct hyn_ts_fuc hyn_cst3240_fuc = {
	.tp_rest = cst3240_rst,
	.tp_report = cst3240_report,
	.tp_supend = cst3240_supend,
	.tp_resum = cst3240_resum,
	.tp_chip_init = cst3240_init,
	.tp_updata_fw = cst3240_updata_fw,
	.tp_set_workmode = cst3240_set_workmode,
	.tp_check_esd = cst3240_check_esd,
	.tp_prox_handle = cst3240_prox_handle,
	.tp_get_dbg_data = cst3240_get_dbg_data,
	.tp_get_test_result = cst3240_get_test_result
};
