/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_chips/hyn_cst7xx.c
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
#include "cst7xx_fw.h"

#define CUSTOM_SENSOR_NUM  	(20)

#define BOOT_I2C_ADDR   (0x6A)
#define MAIN_I2C_ADDR   (0x15)
#define RW_REG_LEN   (2)
#define CST7XX_BIN_SIZE	(15*1024)
static struct hyn_ts_data *hyn_7xxdata = NULL;

static int cst7xx_updata_judge(u8 *p_fw, u16 len);
static u32 cst7xx_read_checksum(void);
static int cst7xx_updata_tpinfo(void);
static int cst7xx_enter_boot(void);
static int cst7xx_set_workmode(enum work_mode mode,u8 enable);
static void cst7xx_rst(void);

static int cst7xx_init(struct hyn_ts_data* ts_data)
{
	int ret = 0;
	u8 buf[4];
	HYN_ENTER();
	hyn_7xxdata = ts_data;
	ret = cst7xx_enter_boot();
	if(ret == FALSE){
		HYN_ERROR("cst7xx_enter_boot failed");
		return FALSE;
	}
	hyn_7xxdata->fw_updata_addr = (u8*)fw_bin;
	hyn_7xxdata->fw_updata_len = CST7XX_BIN_SIZE;

	hyn_7xxdata->hw_info.ic_fw_checksum = cst7xx_read_checksum();

	hyn_wr_reg(hyn_7xxdata,0xA006EE,3,buf,0); //exit boot
	cst7xx_rst();
	mdelay(50);
	hyn_set_i2c_addr(hyn_7xxdata,MAIN_I2C_ADDR);
	cst7xx_updata_tpinfo();
	cst7xx_set_workmode(NOMAL_MODE,0);
	hyn_7xxdata->need_updata_fw = cst7xx_updata_judge((u8*)fw_bin,CST7XX_BIN_SIZE);

	if(hyn_7xxdata->need_updata_fw){
		HYN_INFO("need updata FW !!!");
	}
	return TRUE;
}


static int  cst7xx_enter_boot(void)
{
	uint8_t t;
	hyn_set_i2c_addr(hyn_7xxdata,BOOT_I2C_ADDR);
	for (t = 5;; t += 2)
	{
		int ok = FALSE;
		uint8_t i2c_buf[4] = {0};

		if (t >= 15){
			return FALSE;
		}
		cst7xx_rst();
		mdelay(t);
		ok = hyn_wr_reg(hyn_7xxdata, 0xA001AA, 3, i2c_buf, 0);
		if(ok == FALSE){
			continue;
		}
		ok = hyn_wr_reg(hyn_7xxdata, 0xA003,  2, i2c_buf, 1);
		if(ok == FALSE){
			continue;
		}
		if (i2c_buf[0] != 0x55){
			continue;
		}
		break;
	}
	return TRUE;
}


static int write_code(u8 *bin_addr,uint8_t retry)
{
	uint16_t i,t;//,j;
	int ok = FALSE;
	u8 i2c_buf[512+2];

	bin_addr+=6;
	hyn_7xxdata->fw_updata_process = 0;
	for ( i = 0;i < CST7XX_BIN_SIZE; i += 512)
	{
		i2c_buf[0] = 0xA0;
		i2c_buf[1] = 0x14;

		i2c_buf[2] = i;
		i2c_buf[3] = i >> 8;
		ok = hyn_write_data(hyn_7xxdata, i2c_buf,RW_REG_LEN, 4);
		if (ok == FALSE){
			break;
		}

		i2c_buf[0] = 0xA0;
		i2c_buf[1] = 0x18;
		if(0 == hyn_7xxdata->fw_file_name[0]){
			memcpy(i2c_buf + 2, bin_addr + i, 512);
		}
		else{
			ok = hyn_copy_for_updata(hyn_7xxdata,i2c_buf + 2,i+6,512);
			if(ok)break;
		}
		ok = hyn_write_data(hyn_7xxdata, i2c_buf,RW_REG_LEN, 514);
		if (ok == FALSE){
			break;
		}

		ok = hyn_wr_reg(hyn_7xxdata, 0xA004EE, 3,i2c_buf,0);
		if (ok == FALSE){
			break;
		}

		mdelay(100 * retry);
		hyn_7xxdata->fw_updata_process = i*100/CST7XX_BIN_SIZE;
		for (t = 0;; t ++)
		{
			if (t >= 50){
				return FALSE;
			}
			mdelay(5);

			ok = hyn_wr_reg(hyn_7xxdata,0xA005,2,i2c_buf,1);
			if (ok == FALSE){
				continue;
			}
			if (i2c_buf[0] != 0x55){
				continue;
			}
			break;
		}
	}
	return ok;
}


static uint32_t cst7xx_read_checksum(void)
{
	int ret = -1,time_out,retry = 3;
	uint8_t i2c_buf[4] = {0};
	uint32_t value = 0;
	hyn_7xxdata->boot_is_pass = 0;
	while(retry--){
		ret = hyn_wr_reg(hyn_7xxdata, 0xA00300,  3, i2c_buf, 0);
		if(ret) continue;
		mdelay(100);
		time_out = 100;
		while(time_out--){
			mdelay(10);
			ret = hyn_wr_reg(hyn_7xxdata, 0xA000,  2, i2c_buf, 1);
			if(0==ret && i2c_buf[0] == 1){
				hyn_7xxdata->boot_is_pass = 1;
				break;
			}
			ret = -2;
		}
		if(ret) continue;
		ret = hyn_wr_reg(hyn_7xxdata, 0xA008,  2, i2c_buf, 2);
		if(ret == 0){
			value = (i2c_buf[1]<<8)|i2c_buf[0];
			break;
		}
	}
	return value;
}



static int cst7xx_updata_fw(u8 *bin_addr, u16 len)
{
	int retry = 0;
	int ok = FALSE,ret = 0;
	u8 i2c_buf[4];
	u32 fw_checksum = 0;
	// len = len;
	HYN_ENTER();
	if(0 == hyn_7xxdata->fw_file_name[0]){
		fw_checksum =U8TO16(bin_addr[5],bin_addr[4]);
	}
	else{
		ok = hyn_copy_for_updata(hyn_7xxdata,i2c_buf,4,2);
		if(ok)  goto UPDATA_END;
		fw_checksum = U8TO16(i2c_buf[1],i2c_buf[0]);
	}

	hyn_irq_set(hyn_7xxdata,DISABLE);
	hyn_esdcheck_switch(hyn_7xxdata,DISABLE);

	for(retry = 1; retry<10; retry++){
		ret = -1;
		ok = cst7xx_enter_boot();
		if (ok == FALSE){
			continue;
		}
		ok = write_code(bin_addr,retry);
		if (ok == FALSE){
			continue;
		}
		hyn_7xxdata->hw_info.ic_fw_checksum = cst7xx_read_checksum();
		if(fw_checksum != hyn_7xxdata->hw_info.ic_fw_checksum && hyn_7xxdata->boot_is_pass){
			hyn_7xxdata->fw_updata_process |= 0x80;
			continue;
		}
		hyn_7xxdata->fw_updata_process = 100;
		ret = 0;
		break;
	}
UPDATA_END:
	hyn_wr_reg(hyn_7xxdata,0xA006EE,3,i2c_buf,0); //exit boot
	mdelay(2);
	cst7xx_rst();
	mdelay(50);

	hyn_set_i2c_addr(hyn_7xxdata,MAIN_I2C_ADDR);
	HYN_INFO("updata_fw %s",ret == 0 ? "success" : "failed");
	if(ret == 0){
		cst7xx_updata_tpinfo();
	}
	hyn_irq_set(hyn_7xxdata,ENABLE);
	hyn_esdcheck_switch(hyn_7xxdata,ENABLE);
	return ret;
}


static int cst7xx_updata_tpinfo(void)
{
	u8 buf[8];
	struct tp_info *ic = &hyn_7xxdata->hw_info;
	int ret = 0;

	ret = hyn_wr_reg(hyn_7xxdata,0xA6,1,buf,6);
	if(ret == FALSE){
		HYN_ERROR("cst7xx_updata_tpinfo failed");
		return FALSE;
	}

	ic->fw_sensor_txnum = 2;
	ic->fw_sensor_rxnum = CUSTOM_SENSOR_NUM;
	ic->fw_key_num = hyn_7xxdata->plat_data.key_num;
	ic->fw_res_y = hyn_7xxdata->plat_data.y_resolution;
	ic->fw_res_x = hyn_7xxdata->plat_data.x_resolution;
	ic->fw_project_id = buf[3];
	ic->fw_chip_type = buf[4];
	ic->fw_ver = buf[0];

	HYN_INFO("IC_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",ic->fw_project_id,ic->fw_chip_type,ic->fw_ver,ic->ic_fw_checksum);
	return TRUE;
}

static int cst7xx_updata_judge(u8 *p_fw, u16 len)
{
	u32 f_checksum,f_fw_ver,f_ictype,f_project_id;//f_module_id;
	u8 *p_data = p_fw ;
	u16 i,check_h =0x55;
	struct tp_info *ic = &hyn_7xxdata->hw_info;

	f_checksum = U8TO16(p_data[5],p_data[4]);
	p_data += (0x3BF8+6);
	f_project_id = p_data[2];
	f_ictype = (p_data[1]<<8)|p_data[0];
	f_fw_ver = (p_data[5]<<8)|p_data[4];
	// f_module_id = p_data[3];

	HYN_INFO("Bin_info fw_project_id:%04x ictype:%04x fw_ver:%x checksum:%#x",f_project_id,f_ictype,f_fw_ver,f_checksum);

	p_data = p_fw+6;
	for(i=0;i<len-2;i++){
		u16 tmp;
		check_h += p_data[i];
		tmp = check_h>>15;
		check_h <<= 1;
		check_h |= tmp;
	}
	if(check_h != f_checksum){
		HYN_ERROR(".h file is damaged !! check_h:0x%04x f_checksum:0x%04x",check_h,f_checksum);
		return 0;
	}

	if(hyn_7xxdata->boot_is_pass==0  //emty
	  || (ic->fw_ver <= f_fw_ver && ic->fw_project_id ==f_project_id && f_checksum != ic->ic_fw_checksum)
	){
		return 1; //need updata
	}
	return 0;
}

//------------------------------------------------------------------------------//
static int cst7xx_set_workmode(enum work_mode mode,u8 enable)
{
	int ret = -1;
	switch(mode){
		case NOMAL_MODE:
			hyn_esdcheck_switch(hyn_7xxdata,ENABLE);
			hyn_irq_set(hyn_7xxdata,ENABLE);
			ret = hyn_wr_reg(hyn_7xxdata,0xFE00,2,NULL,0);
			break;
		case GESTURE_MODE:
			hyn_esdcheck_switch(hyn_7xxdata,DISABLE);
			ret = hyn_wr_reg(hyn_7xxdata,0xD001,2,NULL,0);
			break;
		case LP_MODE:
			hyn_esdcheck_switch(hyn_7xxdata,DISABLE);
			break;
		case DIFF_MODE:
		case RAWDATA_MODE:
			hyn_esdcheck_switch(hyn_7xxdata,DISABLE);
			ret = hyn_wr_reg(hyn_7xxdata,mode==DIFF_MODE ? 0xBF07:0xBF06,2,NULL,0);
			break;
		case FAC_TEST_MODE:
			hyn_esdcheck_switch(hyn_7xxdata,DISABLE);
			hyn_wr_reg(hyn_7xxdata,0xF001,2,NULL,0);
			msleep(50);
			break;
		case DEEPSLEEP:
			hyn_esdcheck_switch(hyn_7xxdata,DISABLE);
			hyn_irq_set(hyn_7xxdata,DISABLE);
			ret = hyn_wr_reg(hyn_7xxdata,0xA503,2,NULL,0);
			break;
		case ENTER_BOOT_MODE:
			hyn_esdcheck_switch(hyn_7xxdata,DISABLE);
			ret |= cst7xx_enter_boot();
			break;
		case CHARGE_EXIT:
		case CHARGE_ENTER:
			hyn_7xxdata->charge_is_enable = mode&0x01;
			ret = hyn_wr_reg(hyn_7xxdata,(mode&0x01)? 0xE601:0xE600,2,0,0); //charg mode
			mode = hyn_7xxdata->work_mode; //not switch work mode
			break;
		default :
			hyn_esdcheck_switch(hyn_7xxdata,enable);
			hyn_7xxdata->work_mode = NOMAL_MODE;
			ret = -2;
			break;
	}
	if(ret != -2){
		hyn_7xxdata->work_mode = mode;
	}
	return ret;
}

static int cst7xx_prox_handle(u8 cmd)
{
	int ret = 0;
	switch(cmd){
		case 1: //enable
			hyn_7xxdata->prox_is_enable = 1;
			hyn_7xxdata->prox_state = 0;
			ret = hyn_wr_reg(hyn_7xxdata,0xB001,2,NULL,0);
			break;
		case 0: //disable
			hyn_7xxdata->prox_is_enable = 0;
			hyn_7xxdata->prox_state = 0;
			ret = hyn_wr_reg(hyn_7xxdata,0xB000,2,NULL,0);
			break;
		default:
			break;
	}
	return ret;
}

static void cst7xx_rst(void)
{
	if(hyn_7xxdata->work_mode==ENTER_BOOT_MODE){
		hyn_set_i2c_addr(hyn_7xxdata,MAIN_I2C_ADDR);
	}
	gpiod_direction_output(hyn_7xxdata->plat_data.reset_gpio,0);
	msleep(10);
	gpiod_direction_output(hyn_7xxdata->plat_data.reset_gpio,1);
}



static int cst7xx_supend(void)
{
	HYN_ENTER();
	cst7xx_set_workmode(DEEPSLEEP,0);
	return 0;
}

static int cst7xx_resum(void)
{
	cst7xx_rst();
	msleep(50);
	cst7xx_set_workmode(NOMAL_MODE,0);
	return 0;
}

static int cst7xx_report(void)
{
	uint8_t i = 0;
	uint8_t i2c_buf[3+6*MAX_POINTS_REPORT] = {0};
	uint8_t id = 0,index = 0;
	struct hyn_plat_data *dt = &hyn_7xxdata->plat_data;

	memset(&hyn_7xxdata->rp_buf,0,sizeof(hyn_7xxdata->rp_buf));
	hyn_7xxdata->rp_buf.report_need = REPORT_NONE;


	if(hyn_7xxdata->work_mode == GESTURE_MODE){
		static const uint8_t ges_map[][2] = {{0x24,IDX_POWER},{0x22,IDX_UP},{0x23,IDX_DOWN},{0x20,IDX_LEFT},{0x21,IDX_RIGHT},
		{0x34,IDX_C},{0x33,IDX_e},{0x32,IDX_M},{0x30,IDX_O},{0x46,IDX_S},{0x54,IDX_V},{0x31,IDX_W},{0x65,IDX_Z}};
		if(hyn_wr_reg(hyn_7xxdata,0xD3,1,i2c_buf,1)){
			goto FAILD_END;
		}
		index = sizeof(ges_map)/2;
		hyn_7xxdata->gesture_id  = IDX_NULL;
		for(i=0; i<index; i++){
			if(ges_map[i][0] == i2c_buf[0]){
				hyn_7xxdata->gesture_id = ges_map[i][1];
				hyn_7xxdata->rp_buf.report_need = REPORT_GES;
				break;
			}
		}
		return TRUE;
	}
	else{
		int ret = -1,retry = 3;
		u8 event = 0;
		while(--retry){
			ret = hyn_wr_reg(hyn_7xxdata,0x00,1,i2c_buf,(3+6*2));
			if(ret == 0 && i2c_buf[2] < 3) break;
			ret = -1;
		}
		if(ret){
			goto FAILD_END;
		}
		if(hyn_7xxdata->prox_is_enable){
			u8 state=0;
			if(i2c_buf[1]==0xE0 || i2c_buf[1]==0){
				state = PS_FAR_AWAY;
			}
			else if(i2c_buf[1]==0xC0){
				state = PS_NEAR;
			}
			if(hyn_7xxdata->prox_state != state){
				hyn_7xxdata->prox_state = state;
				hyn_7xxdata->rp_buf.report_need |= REPORT_PROX;
			}
		}
		hyn_7xxdata->rp_buf.rep_num  = i2c_buf[2]&0x0F;
		for(i = 0 ; i < 2 ; i++){
			id = (i2c_buf[5 + i*6] & 0xf0)>>4;
			event = i2c_buf[3 + i*6]&0xC0;
			if(id > 1 || event==0xC0) continue;
			hyn_7xxdata->rp_buf.pos_info[index].pos_id = id;
			hyn_7xxdata->rp_buf.pos_info[index].event = 0x40 == event ? 0:1;
			hyn_7xxdata->rp_buf.pos_info[index].pos_x = ((u16)(i2c_buf[3 + i*6] & 0x0f)<<8) + i2c_buf[4 + i*6];
			hyn_7xxdata->rp_buf.pos_info[index].pos_y = ((u16)(i2c_buf[5 + i*6] & 0x0f)<<8) + i2c_buf[6 + i*6];
			// hyn_7xxdata->rp_buf.pos_info[index].pres_z = (i2c_buf[7 + i*6] <<8) + i2c_buf[8 + i*6] ;
			hyn_7xxdata->rp_buf.pos_info[index].pres_z = 3+(hyn_7xxdata->rp_buf.pos_info[index].pos_x&0x03); //press mast chang
			index++;
		}
		if(index){
			hyn_7xxdata->rp_buf.report_need |= REPORT_POS;
		}

		if(dt->key_num){
			i = dt->key_num;
			while(i){
				i--;
				if(dt->key_y_coords ==hyn_7xxdata->rp_buf.pos_info[0].pos_y && dt->key_x_coords[i] == hyn_7xxdata->rp_buf.pos_info[0].pos_x){
					hyn_7xxdata->rp_buf.key_id = i;
					hyn_7xxdata->rp_buf.key_state = hyn_7xxdata->rp_buf.pos_info[0].event;
					hyn_7xxdata->rp_buf.report_need = REPORT_KEY;
				}
			}
		}
	}
	return TRUE;
	FAILD_END:
	HYN_ERROR("read report data failed");
	return FALSE;
}

static u32 cst7xx_check_esd(void)
{
	int ret = -1;
	u8 buf[4];
	ret = hyn_wr_reg(hyn_7xxdata,0xAE,1,buf,2);
	return ret ? -1:U8TO16(buf[1],buf[0]);
}



static int cst7xx_get_dbg_data(u8 *buf, u16 len)
{
	int ret = -1,read_len = len;
	switch (hyn_7xxdata->work_mode){
		case DIFF_MODE:
			ret = hyn_wr_reg(hyn_7xxdata, 0x41, 1,buf,read_len);
			break;
		case RAWDATA_MODE:
			ret = hyn_wr_reg(hyn_7xxdata, 0x61, 1,buf,read_len);
			break;
		default:
			HYN_ERROR("work_mode:%d",hyn_7xxdata->work_mode);
			break;
	}
	return ret==0 ? read_len:-1;
}


static int cst7xx_get_test_result(u8 *buf, u16 len)
{
	return 0;
}



const struct hyn_ts_fuc hyn_cst7xx_fuc = {
	.tp_rest = cst7xx_rst,
	.tp_report = cst7xx_report,
	.tp_supend = cst7xx_supend,
	.tp_resum = cst7xx_resum,
	.tp_chip_init = cst7xx_init,
	.tp_updata_fw = cst7xx_updata_fw,
	.tp_set_workmode = cst7xx_set_workmode,
	.tp_check_esd = cst7xx_check_esd,
	.tp_prox_handle = cst7xx_prox_handle,
	.tp_get_dbg_data = cst7xx_get_dbg_data,
	.tp_get_test_result = cst7xx_get_test_result
};
