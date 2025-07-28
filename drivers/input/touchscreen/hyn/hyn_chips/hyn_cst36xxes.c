/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_chips/hyn_cst36xxes.c
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


#include "cst36xxes_fw.h"

#define BOOT_I2C_ADDR   (0x5A)
#define MAIN_I2C_ADDR   (0x58) //use 2 slave addr

#define PART_NO_EN	  (1)
#define cst36xxes_BIN_SIZE	(10*1024)
#define MODULE_ID_ADDR	  (0x6400)
#define PARTNUM_ADDR		(0x7F10)

static struct hyn_ts_data *hyn_36xxesdata = NULL;
static const u8 gest_map_tbl[] = {
	IDX_POWER,   //GESTURE_LABEL_CLICK,
	IDX_POWER,  //GESTURE_LABEL_CLICK2,
	IDX_UP,	 //GESTURE_LABEL_TOP,
	IDX_DOWN,   //GESTURE_LABEL_BOTTOM,
	IDX_LEFT,   //GESTURE_LABEL_LEFT,
	IDX_RIGHT,  //GESTURE_LABEL_RIGHT,
	IDX_C,	  //GESTURE_LABEL_C,
	IDX_e,	  //GESTURE_LABEL_E,
	IDX_V,	  //GESTURE_LABEL_v,
	IDX_NULL,   //GESTURE_LABEL_^,
	IDX_NULL,   //GESTURE_LABEL_>,
	IDX_NULL,   //GESTURE_LABEL_<,
	IDX_M,	  //GESTURE_LABEL_M,
	IDX_W,	  //GESTURE_LABEL_W,
	IDX_O,	  //GESTURE_LABEL_O,
	IDX_S,	  //GESTURE_LABEL_S,
	IDX_Z,	  //GESTURE_LABEL_Z
};

static const struct hyn_chip_series hyn_36xxes_fw[] = {
	{0xcaca2305,0xffffffff,"cst36xxes",(u8*)fw_bin},
	{0,0,"",NULL}
};

static int cst36xxes_updata_judge(u8 *p_fw, u16 len);
static u32 cst36xxes_read_checksum(void);
static int cst36xxes_enter_boot(void);
static u32 cst36xxes_fread_word(u32 addr);
static void cst36xxes_rst(void);

static int cst36xxes_init(struct hyn_ts_data* ts_data)
{
	int ret = 0,i;
	u32 read_part_no,module_id;
	HYN_ENTER();
	hyn_36xxesdata = ts_data;

	ret = cst36xxes_enter_boot();
	if(ret){
		HYN_ERROR("cst36xxes_enter_boot failed");
		return -1;
	}
	hyn_36xxesdata->fw_updata_addr = hyn_36xxes_fw[0].fw_bin;
	hyn_36xxesdata->fw_updata_len = cst36xxes_BIN_SIZE;
	read_part_no = cst36xxes_fread_word(PARTNUM_ADDR);
	module_id =  cst36xxes_fread_word(MODULE_ID_ADDR);
	HYN_INFO("read_part_no:0x%08x module_id:0x%08x",read_part_no,module_id);
	if(module_id > 10) module_id = 0xffffffff;

	for(i = 0; ;i++){
#if PART_NO_EN
		if(hyn_36xxes_fw[i].part_no == read_part_no && hyn_36xxes_fw[i].moudle_id == module_id)
#else
		if( hyn_36xxes_fw[i].moudle_id == module_id)
#endif
		{   hyn_36xxesdata->fw_updata_addr = hyn_36xxes_fw[i].fw_bin;
			HYN_INFO("chip %s match fw success ,partNo check is [%s]",hyn_36xxes_fw[i].chip_name,PART_NO_EN ? "enable":"disable");
			break;
		}

		if(hyn_36xxes_fw[i].part_no == 0 && hyn_36xxes_fw[i].moudle_id == 0){
			HYN_INFO("unknown chip or unknown moudle_id use hyn_36xxes_fw[0]");
			break;
		}
	}

	hyn_36xxesdata->hw_info.ic_fw_checksum = cst36xxes_read_checksum();
	hyn_set_i2c_addr(hyn_36xxesdata,MAIN_I2C_ADDR);
	cst36xxes_rst(); //exit boot
	mdelay(50);

	hyn_36xxesdata->need_updata_fw = cst36xxes_updata_judge(hyn_36xxesdata->fw_updata_addr,cst36xxes_BIN_SIZE);
	if(hyn_36xxesdata->need_updata_fw){
		HYN_INFO("need updata FW !!!");
	}


	return 0;
}


static int cst36xxes_report(void)
{
	u8 buf[80],i=0;
	u8 finger_num = 0,key_num=0,report_typ= 0,key_state=0,key_id = 0,tmp_dat=0;
	int ret = 0,retry = 2;

	while(retry--){ //read point
		ret = hyn_wr_reg(hyn_36xxesdata,0xD0050000,0x80|4,buf,11);
		report_typ = buf[4];//FF:pos F0:ges E0:prox
		finger_num = buf[5]&0x0f;
		key_num	= (buf[5]&0xf0)>>4;
		if(finger_num+key_num <= MAX_POINTS_REPORT){
			if(key_num + finger_num > 1){
				ret |= hyn_read_data(hyn_36xxesdata,&buf[11],(key_num + finger_num -1)*5);
			}
			// if(hyn_sum16(0x55,&buf[6],(key_num + finger_num)*5) != (buf[0] | buf[1]<<8)){
			//	 ret = -1;
			// }
		}
		else{
			ret = -2;
		}
		if(ret && retry) continue;
		ret |= hyn_wr_reg(hyn_36xxesdata,0xD00002AB,4,buf,0);
		if(ret == 0){
			break;
		}
	}
	if(ret) return ret;

	if((report_typ==0xff)&&((finger_num+key_num)>0)){
		if(key_num){
			key_id	= buf[10]&0x0f;
			key_state = buf[10]>>4;
			if(hyn_36xxesdata->rp_buf.report_need == REPORT_NONE){ //Mutually exclusive reporting of coordinates and keys
				hyn_36xxesdata->rp_buf.report_need |= REPORT_KEY;
			}
			hyn_36xxesdata->rp_buf.key_id = key_id;
			hyn_36xxesdata->rp_buf.key_state = key_state !=0 ? 1:0;
		}

		if(finger_num){ //pos
			u16 index = 0;
			u8 touch_down = 0;
			if(hyn_36xxesdata->rp_buf.report_need == REPORT_NONE){ //Mutually exclusive reporting of coordinates and keys
				hyn_36xxesdata->rp_buf.report_need |= REPORT_POS;
			}
			hyn_36xxesdata->rp_buf.rep_num = finger_num;
			for(i = 0; i < finger_num; i++){
				index = (key_num+i)*5;
				hyn_36xxesdata->rp_buf.pos_info[i].pos_id = buf[index+10]&0x0F;
				hyn_36xxesdata->rp_buf.pos_info[i].event =  buf[index+10]>>4;
				hyn_36xxesdata->rp_buf.pos_info[i].pos_x =  buf[index+6] + ((u16)(buf[index+9]&0x0F) <<8); //x is rx direction
				hyn_36xxesdata->rp_buf.pos_info[i].pos_y =  buf[index+7] + ((u16)(buf[index+9]&0xf0) <<4);
				hyn_36xxesdata->rp_buf.pos_info[i].pres_z = buf[index+8];
				if(hyn_36xxesdata->rp_buf.pos_info[i].event){
					touch_down++;
				}
			}
			if(0== touch_down){
				hyn_36xxesdata->rp_buf.rep_num = 0;
			}
		}
	}else if(report_typ == 0xF0){
		 tmp_dat = buf[10]&0xff;
		if((tmp_dat&0x7F) < sizeof(gest_map_tbl) && gest_map_tbl[tmp_dat] != IDX_NULL){
			hyn_36xxesdata->gesture_id = gest_map_tbl[tmp_dat];
			hyn_36xxesdata->rp_buf.report_need |= REPORT_GES;
			HYN_INFO("gesture_id:%d",tmp_dat);
		}
	}else if(report_typ == 0xE0){//proximity
		u8 state = buf[6] ? PS_FAR_AWAY : PS_NEAR;
		if(hyn_36xxesdata->prox_is_enable && hyn_36xxesdata->prox_state != state){
			hyn_36xxesdata->prox_state = state;
			hyn_36xxesdata->rp_buf.report_need |= REPORT_PROX;
		}
	}
	return ret;
}

static int cst36xxes_prox_handle(u8 cmd)
{
	int ret = 0;
	switch(cmd){
		case 1: //enable
			hyn_36xxesdata->prox_is_enable = 1;
			hyn_36xxesdata->prox_state = 0;
			ret = hyn_wr_reg(hyn_36xxesdata,0xD004B001,4,NULL,0);
			break;
		case 0: //disable
			hyn_36xxesdata->prox_is_enable = 0;
			hyn_36xxesdata->prox_state = 0;
			ret = hyn_wr_reg(hyn_36xxesdata,0xD004B000,4,NULL,0);
			break;
		default:
			break;
	}
	return ret;
}

static int cst36xxes_set_workmode(enum work_mode mode,u8 enable)
{
	int ret = 0;
	uint8_t i = 0;
	for(i=0;i<5;i++)
	{
		hyn_wr_reg(hyn_36xxesdata,0xD0000400,4,0,0); //disable lp i2c plu
		mdelay(1);
		ret = hyn_wr_reg(hyn_36xxesdata,0xD0000400,4,0,0);
		if(ret == 0)
			break;
	}
	switch(mode){
		case NOMAL_MODE:
			hyn_irq_set(hyn_36xxesdata,ENABLE);
			hyn_esdcheck_switch(hyn_36xxesdata,enable);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD0000000,4,0,0);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD0000C00,4,0,0);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD0000100,4,0,0);
			break;
		case GESTURE_MODE:
			hyn_esdcheck_switch(hyn_36xxesdata,ENABLE);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD0000C01,4,0,0);
			break;
		case LP_MODE:
			hyn_esdcheck_switch(hyn_36xxesdata,DISABLE);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD00004AB,4,0,0);
			break;
		case DIFF_MODE:
		case RAWDATA_MODE:
		case BASELINE_MODE:
		case CALIBRATE_MODE:
			hyn_esdcheck_switch(hyn_36xxesdata,DISABLE);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD00002AB,4,0,0);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD00001AB,4,0,0); //enter debug mode
			break;
		case FAC_TEST_MODE:
			hyn_esdcheck_switch(hyn_36xxesdata,DISABLE);
			cst36xxes_rst();
			msleep(50);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD00002AB,4,0,0);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD00000AB,4,0,0); //enter fac test
			msleep(50); //wait  switch to fac mode
			break;
		case DEEPSLEEP:
			hyn_esdcheck_switch(hyn_36xxesdata,DISABLE);
			hyn_irq_set(hyn_36xxesdata,DISABLE);
			ret |= hyn_wr_reg(hyn_36xxesdata,0xD00022AB,4,0,0);
			break;
		case ENTER_BOOT_MODE:
			hyn_esdcheck_switch(hyn_36xxesdata,DISABLE);
			ret |= cst36xxes_enter_boot();
			break;
		case GLOVE_EXIT:
		case GLOVE_ENTER:
			hyn_36xxesdata->glove_is_enable = mode&0x01;
			ret = hyn_wr_reg(hyn_36xxesdata,(mode&0x01)? 0xD0000AAB:0xD0000A00,4,0,0); //glove mode
			mode = hyn_36xxesdata->work_mode; //not switch work mode
			HYN_INFO("set_glove:%d",hyn_36xxesdata->glove_is_enable);
			break;
		case CHARGE_EXIT:
		case CHARGE_ENTER:
			hyn_36xxesdata->charge_is_enable = mode&0x01;
			ret = hyn_wr_reg(hyn_36xxesdata,(mode&0x01)? 0xD0000BAB:0xD0000B00,4,0,0); //charg mode
			mode = hyn_36xxesdata->work_mode; //not switch work mode
			HYN_INFO("set_charge:%d",hyn_36xxesdata->charge_is_enable);
			break;
		default :
			hyn_esdcheck_switch(hyn_36xxesdata,enable);
			ret = -2;
			break;
	}
	if(ret != -2){
		hyn_36xxesdata->work_mode = mode;
	}
	HYN_INFO("set_workmode:%d ret:%d",mode,ret);
	return ret;
}


static int cst36xxes_supend(void)
{
	HYN_ENTER();
	cst36xxes_set_workmode(DEEPSLEEP,0);
	return 0;
}

static int cst36xxes_resum(void)
{
	HYN_ENTER();
	cst36xxes_rst();
	msleep(50);
	cst36xxes_set_workmode(NOMAL_MODE,0);
	return 0;
}

static void cst36xxes_rst(void)
{
	HYN_ENTER();
	if(hyn_36xxesdata->work_mode==ENTER_BOOT_MODE){
		hyn_set_i2c_addr(hyn_36xxesdata,MAIN_I2C_ADDR);
	}
	gpiod_direction_output(hyn_36xxesdata->plat_data.reset_gpio,0);
	msleep(2);
	gpiod_direction_output(hyn_36xxesdata->plat_data.reset_gpio,1);
}

static int cst36xxes_wait_ready(u16 times,u8 ms,u16 reg,u16 check_vlue)
{
	int ret = 0;
	u8 buf[4];
	while(times--){
		ret = hyn_wr_reg(hyn_36xxesdata,reg,2,buf,2);
		if(0==ret && U8TO16(buf[0],buf[1])==check_vlue){
			return 0;
		}
		mdelay(ms);
	}
	return -1;
}

static int erase_all_mem(void)
{
	int ret = FALSE;
	u8 i2c_buf[8];
	u32 retry = 3;
	u32 Flash_addr;
	int i = 0;

	for (i=0;i<3;i++){
		Flash_addr = 0x2000 + i*0x1000;
		ret  |= hyn_wr_reg(hyn_36xxesdata,U8TO32(0xA0,0x06,Flash_addr&0xFF,Flash_addr>>8),4,0,0);
		ret  |= hyn_wr_reg(hyn_36xxesdata,0xA0080010,4,0,0); //set len
		ret  |= hyn_wr_reg(hyn_36xxesdata,0xA00A0100,4,0,0);
		ret  |= hyn_wr_reg(hyn_36xxesdata,0xA018CACA,4,0,0);
		ret  |= hyn_wr_reg(hyn_36xxesdata,0xA004E0,3,NULL,0);	//trig read

		mdelay(30);

		retry = 20;
		while(retry--){
			ret = hyn_wr_reg(hyn_36xxesdata,0xA020,2,i2c_buf,2);
			if(ret==0 && i2c_buf[0]==0xE0 && i2c_buf[1]==0x88){
				break;
			}
			mdelay(1);
		}

	}

	return TRUE;
}

static int write_code(u8 *bin_addr,uint8_t pak_num)
{
	#define PKG_SIZE	(512)
	u16 eep_addr = 0, eep_len;
	u8 i2c_buf[PKG_SIZE+10];
	int i = 0;

	//start trans fw
	// eep_addr = 0;
	eep_len = 0;
	for (i=0; i<pak_num; i++){
		eep_addr = 0x2000+(i<<9);
		hyn_wr_reg(hyn_36xxesdata,U8TO32(0xA0,0x06,eep_addr,eep_addr>>8),4,0,0);
		hyn_wr_reg(hyn_36xxesdata,0xA0080002,4,0,0);
		hyn_wr_reg(hyn_36xxesdata,0xA00A0000,4,0,0);
		hyn_wr_reg(hyn_36xxesdata,0xA018CACA,4,0,0);

		i2c_buf[0] = 0xA0;
		i2c_buf[1] = 0x30;
		if(0 == hyn_36xxesdata->fw_file_name[0]){
			memcpy(i2c_buf + 2, bin_addr+eep_len, PKG_SIZE);
		}else{
			hyn_copy_for_updata(hyn_36xxesdata,i2c_buf + 2,eep_len,PKG_SIZE);
		}
		hyn_write_data(hyn_36xxesdata, i2c_buf,2, PKG_SIZE+2);
		hyn_wr_reg(hyn_36xxesdata, 0xA004E1, 3,0,0);

		eep_len += PKG_SIZE;
		mdelay(20); //wait finsh
		cst36xxes_wait_ready(100,1,0xA020,0xE188);
		hyn_36xxesdata->fw_updata_process = i*100/pak_num;
	}
	return TRUE;
}


static int cst36xxes_enter_boot(void)
{
	int retry = 0,ret = 0;
	hyn_set_i2c_addr(hyn_36xxesdata,BOOT_I2C_ADDR);
	while(++retry<20){
		cst36xxes_rst();
		mdelay(12+retry);
		ret = hyn_wr_reg(hyn_36xxesdata,0xA001A7,3,0,0);
		if(ret < 0){
			continue;
		}
		if(0==cst36xxes_wait_ready(10,2,0xA002,0x33DD)){
			return 0;
		}
	}
	return -1;
}

static int cst36xxes_updata_tpinfo(void)
{
	u8 buf[60];
	struct tp_info *ic = &hyn_36xxesdata->hw_info;
	int ret = 0;
	int retry = 5;
	while(--retry){
		//get all config info
		ret |= cst36xxes_set_workmode(NOMAL_MODE,ENABLE);
		ret |= hyn_wr_reg(hyn_36xxesdata,0xD0010000,0x80|4,buf,50);
		if(ret == 0 && buf[3]==0xCA && buf[2]==0xCA) break;
		mdelay(1);
		ret |= hyn_wr_reg(hyn_36xxesdata,0xD0000400,4,buf,0);
	}

	if(ret || retry==0){
		HYN_ERROR("cst36xxes_updata_tpinfo failed");
		return -1;
	}

	ic->fw_sensor_txnum = buf[48];
	ic->fw_sensor_rxnum = buf[49];
	ic->fw_key_num = buf[27];
	ic->fw_res_y = (buf[31]<<8)|buf[30];
	ic->fw_res_x = (buf[29]<<8)|buf[28];
	HYN_INFO("IC_info tx:%d rx:%d key:%d res-x:%d res-y:%d",ic->fw_sensor_txnum,ic->fw_sensor_rxnum,ic->fw_key_num,ic->fw_res_x,ic->fw_res_y);

	ic->fw_project_id = U8TO32(buf[39],buf[38],buf[37],buf[36]);
	ic->fw_chip_type = U8TO32(buf[3],buf[2],buf[1],buf[0]);
	ic->fw_ver = U8TO32(buf[35],buf[34],buf[33],buf[32]);
	HYN_INFO("IC_info fw_project_id:%04x ictype:%04x fw_ver:%04x checksum:%04x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);
	return 0;
}

static u32 cst36xxes_fread_word(u32 addr)
{
	int ret;
	u8 rec_buf[4],retry;
	u32 read_word = 0;

	retry = 3;
	while(retry--){
		ret = hyn_wr_reg(hyn_36xxesdata,U8TO32(0xA0,0x06,(addr&0xFF),((addr>>8)&0xFF)),4,NULL,0); //set addr
		ret  |= hyn_wr_reg(hyn_36xxesdata,0xA0080400,4,0,0); //set len
		ret  |= hyn_wr_reg(hyn_36xxesdata,0xA00A0000,4,0,0);
		ret  |= hyn_wr_reg(hyn_36xxesdata,0xA004D2,3,NULL,0);	//trig read
		if(ret ==0) break;
	}
	if(ret) return 0;

	retry = 20;
	while(retry--){
		ret = hyn_wr_reg(hyn_36xxesdata,0xA020,2,rec_buf,2);
		if(ret==0 && rec_buf[0]==0xD2 && rec_buf[1]==0x88){
			ret  |= hyn_wr_reg(hyn_36xxesdata,0xA030,2,rec_buf,4);
			if(ret ==0){
				read_word = U8TO32(rec_buf[3],rec_buf[2],rec_buf[1],rec_buf[0]);
				break;
			}
		}
		mdelay(1);
	}
	return read_word;
}

static u32 cst36xxes_read_checksum(void)
{
	int ret = -1;
	u8 buf[10] = {0};
	u32 chipSum = 0;
	hyn_36xxesdata->boot_is_pass = 0;

	hyn_wr_reg(hyn_36xxesdata,0xA004D6,3,0,0);
	mdelay(30);

	ret = cst36xxes_wait_ready(20,2,0xA020,0xD688);

	ret |= hyn_wr_reg(hyn_36xxesdata,0xA030,2,buf,10);
		if(ret == 0 && buf[5] == 0xCA){
			chipSum = U8TO32(buf[9],buf[8],buf[7],buf[6]);
			hyn_36xxesdata->boot_is_pass = 1;
		}
	return chipSum;
}

static int cst36xxes_updata_judge(u8 *p_fw, u16 len)
{
	u32 f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
	// u32 f_check_all;
	u8 *p_data = p_fw + 192; //35k
	int ret;
	struct tp_info *ic = &hyn_36xxesdata->hw_info;

	f_fw_project_id = U8TO32(p_data[39],p_data[38],p_data[37],p_data[36]);
	f_ictype		= U8TO32(p_data[3],p_data[2],p_data[1],p_data[0]);
	f_fw_ver		= U8TO32(p_data[35],p_data[34],p_data[33],p_data[32]);

	p_data = p_fw + cst36xxes_BIN_SIZE - 4;
	f_checksum = U8TO32(p_data[3],p_data[2],p_data[1],p_data[0]);
	HYN_INFO("Bin_info fw_project_id:%04x ictype:%04x fw_ver:%04x checksum:%04x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);

	ret = cst36xxes_updata_tpinfo(); //boot checksum pass, communicate failed not updata
	if(ret) HYN_ERROR("get tpinfo failed");

	if(f_ictype != ic->fw_chip_type || f_fw_project_id != ic->fw_project_id){
		HYN_ERROR("not update,please confirm:f_ictype 0x%04x,f_fw_project_id 0x%04x",f_ictype,f_fw_project_id);
		return 0; //not updata
	}

	if(hyn_36xxesdata->boot_is_pass ==0	//boot failed
	|| ( ret == 0 && f_ictype == ic->fw_chip_type && f_checksum != ic->ic_fw_checksum && f_fw_ver > ic->fw_ver ) // match new ver .h file
	){
		return 1; //need updata
	}
	return 0;
}

static int cst36xxes_updata_fw(u8 *bin_addr, u16 len)
{
	#define PKG_SIZE	(512)
	int ret = -1, retry_fw= 4,pak_num;
	u8 i2c_buf[PKG_SIZE+10] , *p_bin_addr;
	u32 fw_checksum = 0;
	HYN_ENTER();
	HYN_INFO("len = %d",len);
	hyn_36xxesdata->fw_updata_process = 0;
	if(len < cst36xxes_BIN_SIZE){
		HYN_ERROR("bin len erro");
		goto UPDATA_END;
	}
	len = cst36xxes_BIN_SIZE-4;
	if(0 == hyn_36xxesdata->fw_file_name[0]){
		p_bin_addr = bin_addr + len;
		fw_checksum = U8TO32(p_bin_addr[3],p_bin_addr[2],p_bin_addr[1],p_bin_addr[0]);
	}
	else{
		ret = hyn_copy_for_updata(hyn_36xxesdata,i2c_buf,cst36xxes_BIN_SIZE,4);
		if(ret)  goto UPDATA_END;
		fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
	}
	HYN_INFO("fw_checksum_all:%04x",fw_checksum);
	hyn_irq_set(hyn_36xxesdata,DISABLE);
	hyn_esdcheck_switch(hyn_36xxesdata,DISABLE);
	len = cst36xxes_BIN_SIZE;
	pak_num = len/PKG_SIZE;
	while(--retry_fw){
		ret = cst36xxes_enter_boot();
		if(ret){
			HYN_INFO("cst36xxes_enter_boot fail");
			continue;
		}

		ret = erase_all_mem();
		if(ret){
			HYN_INFO("erase_all_mem fail");
			continue;
		}

		ret = write_code(bin_addr,pak_num);
		if (ret){
			HYN_INFO("write_code fail");
			continue;
		}

		hyn_36xxesdata->hw_info.ic_fw_checksum = cst36xxes_read_checksum();
		HYN_INFO("ic_fw_checksum:%04x",hyn_36xxesdata->hw_info.ic_fw_checksum);
		if(fw_checksum == hyn_36xxesdata->hw_info.ic_fw_checksum && 0 != hyn_36xxesdata->boot_is_pass){
			break;
		}
	}
UPDATA_END:
	hyn_set_i2c_addr(hyn_36xxesdata,MAIN_I2C_ADDR);
	cst36xxes_rst();
	if(ret == 0){
		msleep(50);
		cst36xxes_updata_tpinfo();
		hyn_36xxesdata->fw_updata_process = 100;
		HYN_INFO("updata_fw success");
	}
	else{
		hyn_36xxesdata->fw_updata_process |= 0x80;
		HYN_ERROR("updata_fw failed fw_checksum:%#x ic_checksum:%#x",fw_checksum,hyn_36xxesdata->hw_info.ic_fw_checksum);
	}
	hyn_irq_set(hyn_36xxesdata,ENABLE);
	return ret;
}

static u32 cst36xxes_check_esd(void)
{
	int16_t ok = FALSE;
	uint8_t i2c_buf[6], retry;
	uint8_t flag = 0;
	uint32_t esd_value = 0;

	retry = 4;
	flag = 0;
	while (retry--)
	{
		hyn_wr_reg(hyn_36xxesdata,0xD0190000,4,i2c_buf,0);
		udelay(200);
		ok = hyn_wr_reg(hyn_36xxesdata,0xD0190000,4,i2c_buf,1);
		if (ok == FALSE){
			msleep(1);
			continue;
		} else {
			if ((i2c_buf[0] == 0x20) || (i2c_buf[0]==0xA0)){
				flag = 1;
				esd_value = i2c_buf[0];
				break;
			} else {
				HYN_INFO("ESD data NA: 0x%x ",i2c_buf[0]);
				msleep(2);
				continue;
			}
		}
	}
	HYN_INFO("ESD data:%d,0x%04x,0x%04x", flag, esd_value, hyn_36xxesdata->esd_last_value);
	if (flag == 0)
	{
		hyn_power_source_ctrl(hyn_36xxesdata, 0);
		mdelay(2);
		hyn_power_source_ctrl(hyn_36xxesdata, 1);
		mdelay(2);
		cst36xxes_rst();
		msleep(40);
		esd_value = 0;
		hyn_36xxesdata->esd_last_value = 0;
		HYN_INFO("esd_check power reset ic");
	}
	if ((hyn_36xxesdata->work_mode == GESTURE_MODE)&&(esd_value != 0xA0))
	{
		ok = hyn_wr_reg(hyn_36xxesdata,0xD0000C01,4,0,0);
		if (ok == FALSE){
			HYN_ERROR("enter_sleep failed");
		}
	}

	hyn_36xxesdata->esd_last_value = esd_value;
	return esd_value;
}

static int red_dbg_data(u8 *buf, u16 len ,u32 *cmd_list,u8 type)
{
	struct tp_info *ic = &hyn_36xxesdata->hw_info;
	int ret = 0;
	u16 read_len = (ic->fw_sensor_rxnum*ic->fw_sensor_txnum)*type;
	u16 total_len = (ic->fw_sensor_rxnum*ic->fw_sensor_txnum + ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*type;
	if(total_len > len || read_len == 0){
		HYN_ERROR("buf too small or fw_sensor_rxnum fw_sensor_txnum erro");
		return -1;
	}
	ret |= hyn_wr_reg(hyn_36xxesdata,*cmd_list++,0x80|4,buf,read_len); //m cap
	buf += read_len;
	read_len = ic->fw_sensor_rxnum*type;
	ret |= hyn_wr_reg(hyn_36xxesdata,*cmd_list++,0x80|4,buf,read_len); //s rx cap
	buf += read_len;
	read_len = ic->fw_sensor_txnum*type;
	ret |= hyn_wr_reg(hyn_36xxesdata,*cmd_list++,0x80|4,buf,read_len); //s tx cap
	// ret |= hyn_wr_reg(hyn_36xxesdata,0xD00002AB,4,0,0); //end
	return ret < 0 ? -1:total_len;
}

static int cst36xxes_get_dbg_data(u8 *buf, u16 len)
{
	int read_len = -1,ret = 0;
	HYN_ENTER();
	switch(hyn_36xxesdata->work_mode){
		case DIFF_MODE:
			read_len = red_dbg_data(buf,len,(u32[]){0xD0090000,0xD0110000,0xD00D0000},2);
			break;
		case RAWDATA_MODE:
			read_len = red_dbg_data(buf,len,(u32[]){0xD0080000,0xD0100000,0xD00C0000},2);
			break;
		case BASELINE_MODE:
			read_len = red_dbg_data(buf,len,(u32[]){0xD00A0000,0xD0120000,0xD00E0000},2);
			break;
		case CALIBRATE_MODE:{
				u16 tmp_len = len/2;
				len /= 2;
				read_len = red_dbg_data(buf+tmp_len,tmp_len,(u32[]){0xD00B0000,0xD0130000,0xD00F0000},1);
				if(read_len > 0){
					u8 *src_ptr = buf+tmp_len,tmp;
					s16 *des_ptr = (s16*)buf;
					tmp_len = read_len;
					while(tmp_len--){
						tmp = (*src_ptr++)&0x7F;
						*des_ptr++ = (tmp & 0x40) ? -(tmp&0x3F):tmp;
					}
					read_len *= 2;
				}
				break;
			}
		default:
			HYN_ERROR("work_mode:%d",hyn_36xxesdata->work_mode);
			break;
	}
	// HYN_INFO("read_len:%d len:%d",(int)(sizeof(struct ts_frame)+read_len),len);
	if(read_len > 0 && len > (sizeof(struct ts_frame)+read_len)){
		ret = cst36xxes_report();
		if(ret ==0){
			memcpy(buf+read_len+4,(void*)&hyn_36xxesdata->rp_buf,sizeof(struct ts_frame));
			read_len += sizeof(struct ts_frame);
		}
	}
	else{
		hyn_wr_reg(hyn_36xxesdata,0xD00002AB,4,0,0); //end
	}
	return  read_len;
}

static int get_fac_test_data(u32 cmd ,u8 *buf, u16 len ,u8 rev)
{
	int ret = 0;
	ret  = hyn_wr_reg(hyn_36xxesdata,cmd,4,0,0);
	ret |= hyn_wait_irq_timeout(hyn_36xxesdata,100);
	ret |= hyn_wr_reg(hyn_36xxesdata,0xD0090000,0x80|4,buf+rev,len);
	ret  |= hyn_wr_reg(hyn_36xxesdata,0xD00002AB,4,0,0);
	if(ret==0 && rev){
		len /= 2;
		while(len--){
			*buf = *(buf+2);
			buf += 2;
		}
	}
	return ret;
}

#define FACTEST_PATH	"/sdcard/hyn_fac_test_cfg.ini"
#define FACTEST_LOG_PATH "/sdcard/hyn_fac_test.log"
#define FACTEST_ITEM	  (MULTI_OPEN_TEST|MULTI_SHORT_TEST|MULTI_SCAP_TEST)

static int cst36xxes_get_test_result(u8 *buf, u16 len)
{
	struct tp_info *ic = &hyn_36xxesdata->hw_info;
	u16 mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum*2;
	u16 st_len = (ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*2;
	u8 *rbuf = buf;
	u16 *raw_s;
	uint8_t i2c_buf[6];
	int i = 0;

	int ret = FAC_GET_DATA_FAIL;
	HYN_ENTER();
	if((mt_len*3 + st_len*2 + 4) > len || mt_len==0){
		HYN_ERROR("%s", mt_len ? "buf too small":" ic->fw_sensor_rxnum*ic->fw_sensor_txnum=0");
		return FAC_GET_DATA_FAIL;
	}
	if(get_fac_test_data(0xD0002B77,rbuf,mt_len,0)){ //read open high data
		HYN_ERROR("read open high failed");
		goto TEST_ERRO;
	}
	rbuf += mt_len;
	if(get_fac_test_data(0xD0002B07,rbuf,mt_len,0)){ //read open low data
		HYN_ERROR("read open low failed");
		goto TEST_ERRO;
	}
	rbuf += mt_len;
	if(get_fac_test_data(0xD0002C00,rbuf,st_len,0)){ //read short test data
		HYN_ERROR("read fac short failed");
		goto TEST_ERRO;
	}
	else
	{
		raw_s = (u16*)(rbuf);
		HYN_INFO("raw_s start data =  %d",*(raw_s));
		for(i = 0; i< ic->fw_sensor_rxnum+ic->fw_sensor_txnum; i++){
			HYN_INFO("short raw data = %d %d",i, *(raw_s+i));
			if((*raw_s) != 0)  *raw_s = 2000 / (*raw_s);
			else  *raw_s =0;
			HYN_INFO(" data = %d",*(raw_s));
			raw_s++;
		}
	}
	//must rest
	rbuf += st_len;
	cst36xxes_rst();
	msleep(20);
	ret = hyn_wr_reg(hyn_36xxesdata,0xD00000AB,4,0,0);
	if(ret){
			HYN_ERROR("exit lowpower scan failed");
			return FAC_GET_CFG_FAIL;
		}
	for (i=0;i<10;i++)
	{
		ret = hyn_wr_reg(hyn_36xxesdata,0xD0020100,4,i2c_buf,1);
		if(ret){
			msleep(1);
			continue;
		}
		HYN_INFO("i2c_buf[0] 0x%x", i2c_buf[0]);
		if (i2c_buf[0] == 0x30)
			break;

		msleep(1);
	}

	if(get_fac_test_data(0xD0002D00,rbuf,st_len,0)){ ///read scap test data
		HYN_ERROR("read scap failed");
		goto TEST_ERRO;
	}
	////read data finlish start test
	ret = hyn_factory_multitest(hyn_36xxesdata ,FACTEST_PATH, buf,(s16*)(rbuf+st_len),FACTEST_ITEM);

TEST_ERRO:
	if(0 == hyn_fac_test_log_save(FACTEST_LOG_PATH,hyn_36xxesdata,(s16*)buf,ret,FACTEST_ITEM)){
		HYN_INFO("fac_test log save success");
	}
	cst36xxes_resum();
	return ret;
}


const struct hyn_ts_fuc hyn_cst36xxes_fuc = {
	.tp_rest = cst36xxes_rst,
	.tp_report = cst36xxes_report,
	.tp_supend = cst36xxes_supend,
	.tp_resum = cst36xxes_resum,
	.tp_chip_init = cst36xxes_init,
	.tp_updata_fw = cst36xxes_updata_fw,
	.tp_set_workmode = cst36xxes_set_workmode,
	.tp_check_esd = cst36xxes_check_esd,
	.tp_prox_handle = cst36xxes_prox_handle,
	.tp_get_dbg_data = cst36xxes_get_dbg_data,
	.tp_get_test_result = cst36xxes_get_test_result
};
