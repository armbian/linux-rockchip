/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_chips/hyn_cst66xx.c
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


#include "cst66xx_fw1.h"
#include "cst66xx_fw2.h"

#define BOOT_I2C_ADDR   (0x5A)
#define MAIN_I2C_ADDR   (0x58) //use 2 slave addr

#define PART_NO_EN		  (0)
#define cst66xx_BIN_SIZE	(40*1024) //(44*1024) (36*1024)
#define MODULE_ID_ADDR	  (0xA400)
#define PARTNUM_ADDR		(0xFF10)

static struct hyn_ts_data *hyn_66xxdata = NULL;
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

static const struct hyn_chip_series hyn_6xx_fw[] = {
	{0xCACA220E,0xffffffff,"cst3530",(u8*)fw_module1},//if PART_NO_EN==0 use default chip
	{0xCACA2202,0xffffffff,"cst3640",(u8*)fw_module1},
	{0xCACA2201,0xffffffff,"cst6656",(u8*)fw_module1},
	{0xCACA220C,0xffffffff,"cst3548",(u8*)fw_module1},
	{0xCACA2209,0xffffffff,"cst3556",(u8*)fw_module1},
	{0xCACA2203,0xffffffff,"cst6644",(u8*)fw_module1},
	{0xCACA2204,0xffffffff,"cst6856",(u8*)fw_module1},
	{0xCACA2204,0x00000002,"cst6856",(u8*)fw_module2},
	//0xCACA2205 //154
	//0xCACA2206 //148E
	{0,0,"",NULL}
};

static int cst66xx_updata_judge(u8 *p_fw, u16 len);
static u32 cst66xx_read_checksum(void);
static int cst66xx_enter_boot(void);
static u32 cst66xx_fread_word(u32 addr);
static void cst66xx_rst(void);

static int cst66xx_init(struct hyn_ts_data* ts_data)
{
	int ret = 0,i;
	u32 read_part_no,module_id;
	HYN_ENTER();
	hyn_66xxdata = ts_data;

	ret = cst66xx_enter_boot();
	if(ret){
		HYN_ERROR("cst66xx_enter_boot failed");
		return -1;
	}
	hyn_66xxdata->fw_updata_addr = hyn_6xx_fw[0].fw_bin;
	hyn_66xxdata->fw_updata_len = cst66xx_BIN_SIZE;
	read_part_no = cst66xx_fread_word(PARTNUM_ADDR);
	module_id =  cst66xx_fread_word(MODULE_ID_ADDR);
	HYN_INFO("read_part_no:0x%08x module_id:0x%08x",read_part_no,module_id);
	if(module_id > 10) module_id = 0xffffffff;

	for(i = 0; ;i++){
#if PART_NO_EN
		if(hyn_6xx_fw[i].part_no == read_part_no && hyn_6xx_fw[i].moudle_id == module_id)
#else
		if( hyn_6xx_fw[i].moudle_id == module_id)
#endif
		{   hyn_66xxdata->fw_updata_addr = hyn_6xx_fw[i].fw_bin;
			HYN_INFO("chip %s match fw success ,partNo check is [%s]",hyn_6xx_fw[i].chip_name,PART_NO_EN ? "enable":"disable");
			break;
		}

		if(hyn_6xx_fw[i].part_no == 0 && hyn_6xx_fw[i].moudle_id == 0){
			HYN_INFO("unknown chip or unknown moudle_id use hyn_6xx_fw[0]");
			break;
		}
	}
	hyn_66xxdata->hw_info.fw_module_id = module_id;
	hyn_66xxdata->hw_info.ic_fw_checksum = cst66xx_read_checksum();
	hyn_set_i2c_addr(hyn_66xxdata,MAIN_I2C_ADDR);
	cst66xx_rst(); //exit boot
	mdelay(50);

	hyn_66xxdata->need_updata_fw = cst66xx_updata_judge(hyn_66xxdata->fw_updata_addr,cst66xx_BIN_SIZE);
	if(hyn_66xxdata->need_updata_fw){
		HYN_INFO("need updata FW !!!");
	}
	return 0;
}


static int cst66xx_report(void)
{
	u8 buf[80],i=0;
	u8 finger_num = 0,key_num=0,report_typ= 0,key_state=0,key_id = 0,tmp_dat=0;
	int ret = 0,retry = 2;

	while(retry--){ //read point
		ret = hyn_wr_reg(hyn_66xxdata,0xD0070000,0x80|4,buf,9);
		report_typ = buf[2];//FF:pos F0:ges E0:prox
		finger_num = buf[3]&0x0f;
		key_num	= ((buf[3]&0xf0)>>4);
		if(finger_num+key_num <= MAX_POINTS_REPORT){
			if(key_num + finger_num > 1){
				ret |= hyn_read_data(hyn_66xxdata,&buf[9],(key_num + finger_num -1)*5);
			}
			if(hyn_sum16(0x55,&buf[4],(key_num + finger_num)*5) != (buf[0] | buf[1]<<8)){
				ret = -1;
			}
		}
		else{
			ret = -2;
		}
		if(ret && retry) continue;
		ret |= hyn_wr_reg(hyn_66xxdata,0xD00002AB,4,buf,0);
		if(ret == 0){
			break;
		}
	}
	if(ret) return ret;
	if((report_typ==0xff)&&((finger_num+key_num)>0)){
		if(key_num){
			key_id	= buf[8]&0x0f;
			key_state = buf[8]>>4;
			if(hyn_66xxdata->rp_buf.report_need == REPORT_NONE){ //Mutually exclusive reporting of coordinates and keys
				hyn_66xxdata->rp_buf.report_need |= REPORT_KEY;
			}
			hyn_66xxdata->rp_buf.key_id = key_id;
			hyn_66xxdata->rp_buf.key_state = key_state !=0 ? 1:0;
		}

		if(finger_num){ //pos
			u16 index = 0;
			u8 touch_down = 0;
			if(hyn_66xxdata->rp_buf.report_need == REPORT_NONE){ //Mutually exclusive reporting of coordinates and keys
				hyn_66xxdata->rp_buf.report_need |= REPORT_POS;
			}
			hyn_66xxdata->rp_buf.rep_num = finger_num;
			for(i = 0; i < finger_num; i++){
				index = (key_num+i)*5;
				hyn_66xxdata->rp_buf.pos_info[i].pos_id = buf[index+8]&0x0F;
				hyn_66xxdata->rp_buf.pos_info[i].event =  buf[index+8]>>4;
				hyn_66xxdata->rp_buf.pos_info[i].pos_x =  buf[index+4] + ((u16)(buf[index+7]&0x0F) <<8); //x is rx direction
				hyn_66xxdata->rp_buf.pos_info[i].pos_y =  buf[index+5] + ((u16)(buf[index+7]&0xf0) <<4);
				hyn_66xxdata->rp_buf.pos_info[i].pres_z = buf[index+6];
				if(hyn_66xxdata->rp_buf.pos_info[i].event){
					touch_down++;
				}
			}
			if(0== touch_down){
				hyn_66xxdata->rp_buf.rep_num = 0;
			}
		}
	}else if(report_typ == 0xF0){ //gesture
		 tmp_dat = buf[8]&0xff;
		if((tmp_dat&0x7F) < sizeof(gest_map_tbl) && gest_map_tbl[tmp_dat] != IDX_NULL){
			hyn_66xxdata->gesture_id = gest_map_tbl[tmp_dat];
			hyn_66xxdata->rp_buf.report_need |= REPORT_GES;
			HYN_INFO("gesture_id:%d",tmp_dat);
		}
	}else if(report_typ == 0xE0){//proximity
		u8 state = buf[4] ? PS_FAR_AWAY : PS_NEAR;
		if(hyn_66xxdata->prox_is_enable && hyn_66xxdata->prox_state != state){
			hyn_66xxdata->prox_state =state;
			hyn_66xxdata->rp_buf.report_need |= REPORT_PROX;
		}
	}
	return ret;
}

static int cst66xx_prox_handle(u8 cmd)
{
	int ret = 0;
	switch(cmd){
		case 1: //enable
			hyn_66xxdata->prox_is_enable = 1;
			hyn_66xxdata->prox_state = 0xAA;
			ret = hyn_wr_reg(hyn_66xxdata,0xD004B001,4,NULL,0);
			break;
		case 0: //disable
			hyn_66xxdata->prox_is_enable = 0;
			hyn_66xxdata->prox_state = 0xAA;
			ret = hyn_wr_reg(hyn_66xxdata,0xD004B000,4,NULL,0);
			break;
		default:
			break;
	}
	return ret;
}

static int cst66xx_set_workmode(enum work_mode mode,u8 enable)
{
	int ret = 0,i;
	for(i=0;i<5;i++){
		ret = hyn_wr_reg(hyn_66xxdata,0xD0000400,4,0,0); //disable lp i2c plu
		mdelay(1);
		ret |= hyn_wr_reg(hyn_66xxdata,0xD0000400,4,0,0);
		if(ret == 0)
			break;
	}
	switch(mode){
		case NOMAL_MODE:
			hyn_irq_set(hyn_66xxdata,ENABLE);
			hyn_esdcheck_switch(hyn_66xxdata,ENABLE);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD0000000,4,0,0);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD0000C00,4,0,0);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD0000100,4,0,0);
			break;
		case GESTURE_MODE:
			hyn_esdcheck_switch(hyn_66xxdata,ENABLE);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD0000C01,4,0,0);
			break;
		case LP_MODE:
			hyn_esdcheck_switch(hyn_66xxdata,DISABLE);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD00004AB,4,0,0);
			break;
		case DIFF_MODE:
		case RAWDATA_MODE:
		case BASELINE_MODE:
		case CALIBRATE_MODE:
			hyn_esdcheck_switch(hyn_66xxdata,DISABLE);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD00002AB,4,0,0);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD00001AB,4,0,0); //enter debug mode
			break;
		case FAC_TEST_MODE:
			hyn_esdcheck_switch(hyn_66xxdata,DISABLE);
			cst66xx_rst();
			msleep(50);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD00002AB,4,0,0);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD00000AB,4,0,0); //enter fac test
			msleep(50); //wait  switch to fac mode
			break;
		case DEEPSLEEP:
			hyn_irq_set(hyn_66xxdata,DISABLE);
			hyn_esdcheck_switch(hyn_66xxdata,DISABLE);
			ret |= hyn_wr_reg(hyn_66xxdata,0xD00022AB,4,0,0);
			break;
		case ENTER_BOOT_MODE:
			hyn_esdcheck_switch(hyn_66xxdata,DISABLE);
			ret |= cst66xx_enter_boot();
			break;
		case GLOVE_EXIT:
		case GLOVE_ENTER:
			hyn_66xxdata->glove_is_enable = mode&0x01;
			ret = hyn_wr_reg(hyn_66xxdata,(mode&0x01)? 0xD0000AAB:0xD0000A00,4,0,0); //glove mode
			mode = hyn_66xxdata->work_mode; //not switch work mode
			HYN_INFO("set_glove:%d",hyn_66xxdata->glove_is_enable);
			break;
		case CHARGE_EXIT:
		case CHARGE_ENTER:
			hyn_66xxdata->charge_is_enable = mode&0x01;
			ret = hyn_wr_reg(hyn_66xxdata,(mode&0x01)? 0xD0000BAB:0xD0000B00,4,0,0); //charg mode
			mode = hyn_66xxdata->work_mode; //not switch work mode
			HYN_INFO("set_charge:%d",hyn_66xxdata->charge_is_enable);
			break;
		default :
			hyn_esdcheck_switch(hyn_66xxdata,enable);
			ret = -2;
			break;
	}
	if(ret != -2){
		hyn_66xxdata->work_mode = mode;
	}
	HYN_INFO("set_workmode:%d ret:%d",mode,ret);
	return ret;
}


static int cst66xx_supend(void)
{
	HYN_ENTER();
	cst66xx_set_workmode(DEEPSLEEP,0);
	return 0;
}

static int cst66xx_resum(void)
{
	HYN_ENTER();
	cst66xx_rst();
	msleep(50);
	cst66xx_set_workmode(NOMAL_MODE,1);
	return 0;
}

static void cst66xx_rst(void)
{
	HYN_ENTER();
	if(hyn_66xxdata->work_mode==ENTER_BOOT_MODE){
		hyn_set_i2c_addr(hyn_66xxdata,MAIN_I2C_ADDR);
	}
	gpiod_direction_output(hyn_66xxdata->plat_data.reset_gpio,0);
	msleep(8);
	gpiod_direction_output(hyn_66xxdata->plat_data.reset_gpio,1);
}

static int cst66xx_wait_ready(u16 times,u8 ms,u16 reg,u16 check_vlue)
{
	int ret = 0;
	u8 buf[4];
	while(times--){
		ret = hyn_wr_reg(hyn_66xxdata,reg,2,buf,2);
		if(0==ret && U8TO16(buf[0],buf[1])==check_vlue){
			return 0;
		}
		mdelay(ms);
	}
	return -1;
}

static int cst66xx_enter_boot(void)
{
	int retry = 0,ret = 0;
	hyn_set_i2c_addr(hyn_66xxdata,BOOT_I2C_ADDR);
	while(++retry<20){
		cst66xx_rst();
		mdelay(12+retry);
		ret = hyn_wr_reg(hyn_66xxdata,0xA001A8,3,0,0);
		if(ret < 0){
			continue;
		}
		if(0==cst66xx_wait_ready(10,2,0xA002,0x22DD)){
			return 0;
		}
	}
	return -1;
}

static int cst66xx_updata_tpinfo(void)
{
	u8 buf[60];
	struct tp_info *ic = &hyn_66xxdata->hw_info;
	int ret = 0;
	int retry = 5;
	while(--retry){
		//get all config info
		ret |= cst66xx_set_workmode(NOMAL_MODE,ENABLE);
		ret |= hyn_wr_reg(hyn_66xxdata,0xD0030000,0x80|4,buf,50);
		if(ret == 0 && buf[3]==0xCA && buf[2]==0xCA) break;
		mdelay(1);
		ret |= hyn_wr_reg(hyn_66xxdata,0xD0000400,4,buf,0);
	}

	if(ret || retry==0){
		HYN_ERROR("cst66xx_updata_tpinfo failed");
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

static u32 cst66xx_fread_word(u32 addr)
{
	int ret;
	u8 rec_buf[4],retry;
	u32 read_word = 0;

	retry = 3;
	while(retry--){
		ret = hyn_wr_reg(hyn_66xxdata,U8TO32(0xA0,0x06,(addr&0xFF),((addr>>8)&0xFF)),4,NULL,0); //set addr
		ret  |= hyn_wr_reg(hyn_66xxdata,0xA0080400,4,0,0); //set len
		ret  |= hyn_wr_reg(hyn_66xxdata,0xA00A0300,4,0,0); //?
		ret  |= hyn_wr_reg(hyn_66xxdata,0xA004D2,3,NULL,0);	//trig read
		if(ret ==0) break;
	}
	if(ret) return 0;

	retry = 20;
	while(retry--){
		ret = hyn_wr_reg(hyn_66xxdata,0xA020,2,rec_buf,2);
		if(ret==0 && rec_buf[0]==0xD2 && rec_buf[1]==0x88){
			ret  |= hyn_wr_reg(hyn_66xxdata,0xA040,2,rec_buf,4);
			if(ret ==0){
				read_word = U8TO32(rec_buf[3],rec_buf[2],rec_buf[1],rec_buf[0]);
				break;
			}
		}
		mdelay(1);
	}
	return read_word;
}

static int  read_checksum(u16 start_val,u16 start_addr,u16 len ,u32 *check_sum)
{
	int ret,retry = 3;
	u8 buf[8] = {0};
	while(retry--){
		ret = hyn_wr_reg(hyn_66xxdata,U8TO32(0xA0,0x06,start_addr&0xFF,start_addr>>8),4,0,0);
		ret |= hyn_wr_reg(hyn_66xxdata,U8TO32(0xA0,0x08,len&0xFF,len>>8),4,0,0);
		ret |= hyn_wr_reg(hyn_66xxdata,U8TO32(0xA0,0x0A,start_val&0xFF,start_val>>8),4,0,0);
		ret |= hyn_wr_reg(hyn_66xxdata,0xA004D6,3,0,0);
		if(ret) continue;
		mdelay(len/0xc00 + 1);
		ret = cst66xx_wait_ready(20,2,0xA020,0xD688);
		ret |= hyn_wr_reg(hyn_66xxdata,0xA040,2,buf,5);
		if(ret == 0 && buf[0] == 0xCA){
			*check_sum = U8TO32(buf[4],buf[3],buf[2],buf[1]);
			break;
		}
		else{
			ret = -1;
			continue;
		}
	}
	return ret ? -1:0;
}

static u32 cst66xx_read_checksum(void)
{
	int ret = -1;
	u32 chipSum1 = 0,chipSum2 = 0,chipSum3=0,totalSum = 0;
	hyn_66xxdata->boot_is_pass = 0;
	if(0==read_checksum(0,0,0x9000,&chipSum1)){
		if (0==read_checksum(1,0xb000,0x0ffc,&chipSum2)) {
			ret = 0;
			if (hyn_66xxdata->fw_updata_len >= 44*1024)
				ret =read_checksum(1,0xc000,0x0ffc,&chipSum3);
		}
	}
	if(ret ==0){
		if (hyn_66xxdata->fw_updata_len >= 44*1024)
			totalSum = chipSum1 + chipSum2*2 - 0x55 + chipSum3*2 - 0x55;
		else
			totalSum = chipSum1 + chipSum2*2 - 0x55;
		hyn_66xxdata->boot_is_pass = 1;
	}
	HYN_INFO("chipSum1:%04x chipSum2:%04x chipSum3:%04x totalSum:%04x",chipSum1,chipSum2,chipSum3,totalSum);
	return totalSum;
}

static int cst66xx_updata_judge(u8 *p_fw, u16 len)
{
	u32 f_check_all,f_checksum,f_fw_ver,f_ictype,f_fw_project_id;
	u8 *p_data;
	int ret;
	struct tp_info *ic = &hyn_66xxdata->hw_info;

	p_data  = p_fw + (len >= 44*1024 ? 40*1024:35*1024 );
	f_fw_project_id = U8TO32(p_data[39],p_data[38],p_data[37],p_data[36]);
	f_ictype		= U8TO32(p_data[3],p_data[2],p_data[1],p_data[0]);
	f_fw_ver		= U8TO32(p_data[35],p_data[34],p_data[33],p_data[32]);

	p_data = p_fw + (len >= 44*1024 ? 44*1024:40*1024 );
	f_checksum = U8TO32(p_data[3],p_data[2],p_data[1],p_data[0]);
	HYN_INFO("Bin_info fw_project_id:%04x ictype:%04x fw_ver:%04x checksum:%04x",f_fw_project_id,f_ictype,f_fw_ver,f_checksum);

	ret = cst66xx_updata_tpinfo(); //boot checksum pass, communicate failed not updata
	if(ret) HYN_ERROR("get tpinfo failed");

	//check h file
	f_check_all = hyn_sum32(0x55,(u32*)p_fw,len/4);
	if(f_check_all != f_checksum){
		HYN_INFO(".h file checksum erro:%04x len:%d",f_check_all,len);
		return 0; //no need updata
	}

	if(hyn_66xxdata->boot_is_pass ==0	//boot failed
	//|| (ret && hyn_66xxdata->boot_is_pass)//erro fw needupdata
	|| ( ret == 0 && f_ictype == ic->fw_chip_type && f_checksum != ic->ic_fw_checksum && f_fw_ver >= ic->fw_ver ) // match new ver .h file
	){
		return 1; //need updata
	}
	return 0;
}

static int cst66xx_erase_flash(u16 start_addr, u16 len, u16 type)
{
	int ret = 0;
	// HYN_INFO("addr:%04x len:%04x",0xA0060000+U16REV(start_addr),0xA0080000+U16REV(len));
	ret = hyn_wr_reg(hyn_66xxdata,0xA0060000+U16REV(start_addr),4,0,0);  //addr
	ret |= hyn_wr_reg(hyn_66xxdata,0xA0080000+U16REV(len),4,0,0); //len
	ret |= hyn_wr_reg(hyn_66xxdata,0xA00A0000+U16REV(type),4,0,0); //type
	ret |= hyn_wr_reg(hyn_66xxdata,0xA018CACA,4,0,0); //key
	ret |= hyn_wr_reg(hyn_66xxdata,0xA004E0,3,0,0); //trig
	if(ret) return -1;

	mdelay(20); //wait finsh earse flash
	ret = cst66xx_wait_ready(100,1,0xA020,0xE088);
	return ret;
}

static int cst66xx_updata_fw(u8 *bin_addr, u16 len)
{
	#define PKG_SIZE	(1024)
	int i,ret = -1, retry_fw= 4,pak_num;
	u8 *i2c_buf , *p_bin_addr;
	u16 eep_addr = 0, eep_len;
	u32 fw_checksum = 0;
	HYN_ENTER();
	i2c_buf = kmalloc(PKG_SIZE + 10, GFP_KERNEL);
	HYN_INFO("len = %d",len);
	hyn_66xxdata->fw_updata_process = 0;
	if(len < cst66xx_BIN_SIZE){
		HYN_ERROR("bin len erro");
		goto UPDATA_END;
	}
	len = cst66xx_BIN_SIZE;
	if(0 == hyn_66xxdata->fw_file_name[0]){
		p_bin_addr = bin_addr + len;
		fw_checksum = U8TO32(p_bin_addr[3],p_bin_addr[2],p_bin_addr[1],p_bin_addr[0]);
	}
	else{
		ret = hyn_copy_for_updata(hyn_66xxdata,i2c_buf,cst66xx_BIN_SIZE,4);
		if(ret)  goto UPDATA_END;
		fw_checksum = U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]);
	}
	HYN_INFO("fw_checksum_all:%04x",fw_checksum);
	hyn_irq_set(hyn_66xxdata,DISABLE);
	hyn_esdcheck_switch(hyn_66xxdata,DISABLE);
	pak_num = len/PKG_SIZE;
	while(--retry_fw){
		ret = cst66xx_enter_boot();
		if(ret){
			HYN_INFO("cst66xx_enter_boot fail");
			continue;
		}
		if(cst66xx_erase_flash(0x0000,0x8000,0x02)) continue; //erase 32k 0x0000~0x8000
		if(cst66xx_erase_flash(0x8000,0x1000,0x01)) continue; //erase 4k 0x8000~0x9000
		if(cst66xx_erase_flash(0xB000,0x3000,0x04)) continue; //erase 12k 0xB000~0xE000
		//start trans fw
		eep_addr = 0;
		eep_len = 0;
		// p_bin_addr = bin_addr;
		for (i=0; i<pak_num; i++){
			ret = hyn_wr_reg(hyn_66xxdata,U8TO32(0xA0,0x06,eep_addr,eep_addr>>8),4,0,0);
			ret |= hyn_wr_reg(hyn_66xxdata,0xA0080004,4,0,0);
			ret |= hyn_wr_reg(hyn_66xxdata,i>=36 ? 0xA00A0300:0xA00A0000,4,0,0);
			ret |= hyn_wr_reg(hyn_66xxdata,0xA018CACA,4,0,0);
			if(ret) continue;

			i2c_buf[0] = 0xA0;
			i2c_buf[1] = 0x40;
			if(0 == hyn_66xxdata->fw_file_name[0]){
				memcpy(i2c_buf + 2, bin_addr+eep_len, PKG_SIZE);
			}else{
				ret |= hyn_copy_for_updata(hyn_66xxdata,i2c_buf + 2,eep_len,PKG_SIZE);
			}
			ret |= hyn_write_data(hyn_66xxdata, i2c_buf,2, PKG_SIZE+2);
			msleep(5);
			ret |= hyn_wr_reg(hyn_66xxdata, 0xA004E1, 3,0,0);

			eep_len += PKG_SIZE;
			eep_addr += PKG_SIZE;
			if(0x9000 == eep_addr){
				eep_addr = 0xB000;
			}
			mdelay(20); //wait finsh
			cst66xx_wait_ready(100,1,0xA020,0xE188);
			hyn_66xxdata->fw_updata_process = i*100/pak_num;
			// HYN_INFO("FB_%d",hyn_66xxdata->fw_updata_process);
		}
		if(ret) continue;
		hyn_66xxdata->hw_info.ic_fw_checksum = cst66xx_read_checksum();
		if(fw_checksum == hyn_66xxdata->hw_info.ic_fw_checksum && 0 != hyn_66xxdata->boot_is_pass){
			break;
		}
		else{
			ret = -2;
		}
	}
UPDATA_END:
	hyn_set_i2c_addr(hyn_66xxdata,MAIN_I2C_ADDR);
	cst66xx_rst();
	if(ret == 0){
		msleep(50);
		cst66xx_updata_tpinfo();
		hyn_66xxdata->fw_updata_process = 100;
		HYN_INFO("updata_fw success");
	}
	else{
		hyn_66xxdata->fw_updata_process |= 0x80;
		HYN_ERROR("updata_fw failed fw_checksum:%#x ic_checksum:%#x",fw_checksum,hyn_66xxdata->hw_info.ic_fw_checksum);
	}
	hyn_irq_set(hyn_66xxdata,ENABLE);

	kfree(i2c_buf);
	return ret;
}

static u32 cst66xx_check_esd(void)
{
	int ok = 0;
	u8 i2c_buf[6], retry;
	u32 esd_value = 0;

	retry = 4;
	while (retry--){
		hyn_wr_reg(hyn_66xxdata,0xD0000D00,4,i2c_buf,0);
		udelay(200);
		ok = hyn_wr_reg(hyn_66xxdata,0xD0000D00,4,i2c_buf,1);
		if((i2c_buf[0] == 0x20 || i2c_buf[0]==0xA0 || i2c_buf[0]==0xA5)&& ok==0){
			break;
		}
		else{
			ok = -2;
		}
		msleep(1);
	}
	if(-2==ok){   //esdcheck failed rest system
		hyn_66xxdata->esd_fail_cnt = 3;
		esd_value = hyn_66xxdata->esd_last_value;
	}
	else if(i2c_buf[0]==0xA5){ //old sdk (V4.5)
		ok = hyn_wr_reg(hyn_66xxdata,0xD0040400,4,i2c_buf,4);
		if(ok){
			mdelay(1);
			ok = hyn_wr_reg(hyn_66xxdata,0xD0040400,4,i2c_buf,4);
		}
		esd_value = ok==0 ? U8TO32(i2c_buf[3],i2c_buf[2],i2c_buf[1],i2c_buf[0]):hyn_66xxdata->esd_last_value;
	}
	else{
		esd_value = hyn_66xxdata->esd_last_value+1;
	}
	return esd_value;
}

static int red_dbg_data(u8 *buf, u16 len ,u32 *cmd_list,u8 type)
{
	struct tp_info *ic = &hyn_66xxdata->hw_info;
	int ret = 0;
	u16 read_len = (ic->fw_sensor_rxnum*ic->fw_sensor_txnum)*type;
	u16 total_len = (ic->fw_sensor_rxnum*ic->fw_sensor_txnum + ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*type;
	if(total_len > len || read_len == 0){
		HYN_ERROR("buf too small or fw_sensor_rxnum fw_sensor_txnum erro");
		return -1;
	}
	ret |= hyn_wr_reg(hyn_66xxdata,*cmd_list++,0x80|4,buf,read_len); //m cap
	buf += read_len;
	read_len = ic->fw_sensor_rxnum*type;
	ret |= hyn_wr_reg(hyn_66xxdata,*cmd_list++,0x80|4,buf,read_len); //s rx cap
	buf += read_len;
	read_len = ic->fw_sensor_txnum*type;
	ret |= hyn_wr_reg(hyn_66xxdata,*cmd_list++,0x80|4,buf,read_len); //s tx cap
	// ret |= hyn_wr_reg(hyn_66xxdata,0xD00002AB,4,0,0); //end
	return ret < 0 ? -1:total_len;
}

static int cst66xx_get_dbg_data(u8 *buf, u16 len)
{
	int read_len = -1,ret = 0;
	HYN_ENTER();
	switch(hyn_66xxdata->work_mode){
		case DIFF_MODE:
			read_len = red_dbg_data(buf,len,(u32[]){0xD0120000,0xD01A0000,0xD0160000},2);
			break;
		case RAWDATA_MODE:
			read_len = red_dbg_data(buf,len,(u32[]){0xD0110000,0xD0190000,0xD0150000},2);
			break;
		case BASELINE_MODE:
			read_len = red_dbg_data(buf,len,(u32[]){0xD0130000,0xD01B0000,0xD0170000},2);
			break;
		case CALIBRATE_MODE:{
				u16 tmp_len = len/2;
				len /= 2;
				read_len = red_dbg_data(buf+tmp_len,tmp_len,(u32[]){0xD0140000,0xD0180000,0xD01c0000},1);
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
			HYN_ERROR("work_mode:%d",hyn_66xxdata->work_mode);
			break;
	}
	// HYN_INFO("read_len:%d len:%d",(int)(sizeof(struct ts_frame)+read_len),len);
	if(read_len > 0 && len > (sizeof(struct ts_frame)+read_len)){
		ret = cst66xx_report();
		if(ret ==0){
			memcpy(buf+read_len+4,(void*)&hyn_66xxdata->rp_buf,sizeof(struct ts_frame));
			read_len += sizeof(struct ts_frame);
		}
	}
	else{
		hyn_wr_reg(hyn_66xxdata,0xD00002AB,4,0,0); //end
	}
	return  read_len;
}

static int get_fac_test_data(u32 cmd ,u8 *buf, u16 len ,u8 rev)
{
	int ret = 0;
	ret  = hyn_wr_reg(hyn_66xxdata,cmd,4,0,0);
	ret |= hyn_wait_irq_timeout(hyn_66xxdata,100);
	ret  |= hyn_wr_reg(hyn_66xxdata,0xD0120000,0x80|4,buf+rev,len);
	ret  |= hyn_wr_reg(hyn_66xxdata,0xD00002AB,4,0,0);
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

static int cst66xx_get_test_result(u8 *buf, u16 len)
{
	struct tp_info *ic = &hyn_66xxdata->hw_info;
	u16 mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum*2;
	u16 st_len = (ic->fw_sensor_txnum + ic->fw_sensor_rxnum)*2;
	u8 *rbuf = buf;
	int ret = FAC_GET_DATA_FAIL;
	HYN_ENTER();
	if((mt_len*3 + st_len*2 + 4) > len || mt_len==0){
		HYN_ERROR("%s", mt_len ? "buf too small":" ic->fw_sensor_rxnum*ic->fw_sensor_txnum=0");
		return FAC_GET_DATA_FAIL;
	}
	if(get_fac_test_data(0xD0002000,rbuf,mt_len,0)){ //read open high data
		HYN_ERROR("read open high failed");
		goto TEST_ERRO;
	}
	rbuf += mt_len;
	if(get_fac_test_data(0xD0002100,rbuf,mt_len,0)){ //read open low data
		HYN_ERROR("read open low failed");
		goto TEST_ERRO;
	}
	rbuf += mt_len;
	if(get_fac_test_data(0xD0002300,rbuf,st_len,1)){ //read short test data
		HYN_ERROR("read fac short failed");
		goto TEST_ERRO;
	}
	//must rest
	rbuf += st_len;
	if(get_fac_test_data(0xD0002500,rbuf,st_len,0)){ ///read scap test data
		HYN_ERROR("read scap failed");
		goto TEST_ERRO;
	}
	////read data finlish start test
	ret = hyn_factory_multitest(hyn_66xxdata ,FACTEST_PATH, buf,(s16*)(rbuf+st_len),FACTEST_ITEM);

TEST_ERRO:
	if(0 == hyn_fac_test_log_save(FACTEST_LOG_PATH,hyn_66xxdata,(s16*)buf,ret,FACTEST_ITEM)){
		HYN_INFO("fac_test log save success");
	}
	cst66xx_resum();
	return ret;
}


const struct hyn_ts_fuc hyn_cst66xx_fuc = {
	.tp_rest = cst66xx_rst,
	.tp_report = cst66xx_report,
	.tp_supend = cst66xx_supend,
	.tp_resum = cst66xx_resum,
	.tp_chip_init = cst66xx_init,
	.tp_updata_fw = cst66xx_updata_fw,
	.tp_set_workmode = cst66xx_set_workmode,
	.tp_check_esd = cst66xx_check_esd,
	.tp_prox_handle = cst66xx_prox_handle,
	.tp_get_dbg_data = cst66xx_get_dbg_data,
	.tp_get_test_result = cst66xx_get_test_result
};
