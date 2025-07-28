/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_lib/hyn_fs_node.c
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
#include <linux/string.h>
#include <linux/kernel.h>


static struct hyn_ts_data *hyn_fs_data = NULL;

//echo fd /sdcard/app.bin
//echo w d1 01
//echo w d1 01 r 2
//echo w 1 2 3 4 5
//echo rst
#define DEBUG_BUF_SIZE (256)

static  ssize_t hyn_dbg_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	ssize_t len = 0;
	int ret = -1;
	u8 *kbuf =NULL;
	u16 i;
	HYN_ENTER();
	mutex_lock(&hyn_fs_data->mutex_fs);
	kbuf = kzalloc(DEBUG_BUF_SIZE, GFP_KERNEL);

	if(hyn_fs_data->host_cmd_save[0]==0x5A){
		if(hyn_fs_data->host_cmd_save[1] > 4) hyn_fs_data->host_cmd_save[1] = 4;
		len = hyn_fs_data->host_cmd_save[6];
		if(len != 0 && hyn_fs_data->host_cmd_save[1] !=0){
			u32 reg;
			reg = hyn_fs_data->host_cmd_save[2];
			i = 0;
			while(++i < hyn_fs_data->host_cmd_save[1]){
				reg <<= 8;
				reg |= hyn_fs_data->host_cmd_save[2+i];
			}
			ret = hyn_wr_reg(hyn_fs_data,reg,hyn_fs_data->host_cmd_save[1],kbuf,(u16)len);
		}
	}
	else if(hyn_fs_data->host_cmd_save[0]==0x7A){
		len = hyn_fs_data->host_cmd_save[6];
		ret = hyn_read_data(hyn_fs_data,kbuf,(u16)len);
	}

	if(ret ==0){
		ssize_t cnt_char = 0;
		for(i = 0; i< len; i++){
			ret = sprintf(buf+cnt_char,"0x%02x ",*(kbuf+i));
			cnt_char += ret;
		}
		buf[cnt_char] = '\n';
		ret = cnt_char+1;
	}
	else{
		sprintf(buf,"fs read failed\n");
		ret = 13;
	}

	if(!IS_ERR_OR_NULL(kbuf)) kfree(kbuf);
	mutex_unlock(&hyn_fs_data->mutex_fs);

	return ret;
}

static  ssize_t hyn_dbg_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	u16 rec_len;
	u8 str[128];
	u8 *next_ptr;
	int val;
	const struct hyn_ts_fuc* hyn_fun = hyn_fs_data->hyn_fuc_used;
	int ret = 0;
	HYN_ENTER();
	mutex_lock(&hyn_fs_data->mutex_fs);
	rec_len = strlen(buf);
	HYN_INFO("rec_len =%d",rec_len);
	// HYN_INFO("%ddata:0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",(int)count,buf[0], buf[1],buf[2], buf[3],buf[4], buf[5],buf[6],buf[7]);
	if(rec_len < 1 || rec_len > DEBUG_BUF_SIZE){
		HYN_INFO("erro write len");
		goto END;
	}
	next_ptr = (u8*)buf;
	str[0] = '\0';
	hyn_get_word(&next_ptr,str);
	if(0 == strcmp(str,"fd")){ //updata fw
		hyn_get_word(&next_ptr,str);
		if(strlen(str)<4){
			strcpy(str,"/sdcard/app.bin");
		}
		HYN_INFO("filename = %s",str);
		strcpy(hyn_fs_data->fw_file_name,str);
		hyn_fun->tp_updata_fw(hyn_fs_data->fw_updata_addr,hyn_fs_data->fw_updata_len);
		// if(0 == hyn_read_fw_file(str, &pdata, &bin_len)){
		// 	ret = hyn_fun->tp_updata_fw(pdata,bin_len);
		// 	kfree(pdata);
		// }
	}
	else if(0 == strcmp(str,"w")){
		u8 i = 0;
		u8 *kbuf = kzalloc(count/2, GFP_KERNEL);
		hyn_fs_data->host_cmd_save[0] = 0;
		hyn_fs_data->host_cmd_save[1] = 0xA5;
		hyn_fs_data->host_cmd_save[6] = 0;
		while(1){
			str[0] = '\0';
			ret = hyn_get_word(&next_ptr,str);
			// next_ptr += ret;
			HYN_INFO("read =%s ,len =%d",str,(int)strlen(str));
			if(0 == strcmp(str,"r")){
				ret = hyn_get_word(&next_ptr,str);
				val = hyn_str_2_num(str,16);
				if(ret < 0x100){
					hyn_fs_data->host_cmd_save[0] = 0x5A;
					hyn_fs_data->host_cmd_save[1] = i>4 ? 4:i;
					hyn_fs_data->host_cmd_save[6] = val;
					memcpy(&hyn_fs_data->host_cmd_save[2],kbuf,4);
					i = 0;
				}
				break;
			}
			if(ret == 0 || strlen(str)==0 || strlen(str)>2){
				break;
			}
			val = hyn_str_2_num(str,16);
			if(val > 0xFF) break;
			*(kbuf+i) = val;
			HYN_INFO(" %02x",val);
			i++;
		}
		if(i){
			hyn_write_data(hyn_fs_data,kbuf,2,i);
		}
		kfree(kbuf);
	}
	else if(0 == strcmp(str,"r")){
		hyn_get_word(&next_ptr,str);
		val = hyn_str_2_num(str,16);
		if(val < 0x100){
			hyn_fs_data->host_cmd_save[0] = 0x7A;
			hyn_fs_data->host_cmd_save[6] = val;
		}
	}
	else if(0 == strcmp(str,"rst")){
		hyn_fun->tp_rest();
	}
	else if(0 == strcmp(str,"log")){
		hyn_get_word(&next_ptr,str);
		hyn_log_level = (u8)(str[0]-'0');
	}
	else if(0 == strcmp(str,"workmode")){
		ret = hyn_get_word(&next_ptr,str);
		if(ret){
			u8 mode = (u8)(str[0]-'0');
			ret = hyn_get_word(&next_ptr,str);
			if(ret) hyn_fun->tp_set_workmode(mode,str[0]=='0'? 0:1);
		}
	}
END:
	mutex_unlock(&hyn_fs_data->mutex_fs);
	return count;
}

/////
///cat hyntpfwver
static  ssize_t hyn_tpfwver_show(struct device *dev,	struct device_attribute *attr,char *buf)
{
	ssize_t num_read = 0;
	HYN_ENTER();
	num_read = snprintf(buf, 128, "fw_version:0x%02X,chip_type:0x%02X,checksum:0x%02X,project_id:%04x\n",
			   hyn_fs_data->hw_info.fw_ver,
			   hyn_fs_data->hw_info.fw_chip_type,
			   hyn_fs_data->hw_info.ic_fw_checksum,
			   hyn_fs_data->hw_info.fw_project_id
			   );
	return num_read;
}

static  ssize_t hyn_tpfwver_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	/*place holder for future use*/
	return -EPERM;
}

///cat hynselftest
static  ssize_t hyn_selftest_show(struct device *dev,	struct device_attribute *attr,char *buf)
{
	ssize_t num_read = 0;
	u8 *rbuf = NULL;
	const struct hyn_ts_fuc* hyn_fun = hyn_fs_data->hyn_fuc_used;
	int ret = 0;
	int max_len = hyn_fs_data->hw_info.fw_sensor_rxnum*hyn_fs_data->hw_info.fw_sensor_txnum*2
					+ (hyn_fs_data->hw_info.fw_sensor_rxnum + hyn_fs_data->hw_info.fw_sensor_txnum)*4;
	HYN_ENTER();
	max_len = max_len*3;
	rbuf = kzalloc(max_len, GFP_KERNEL);
	if(rbuf == NULL){
		HYN_ERROR("zalloc GFP_KERNEL memory[%d] failed.\n",max_len);
		return -ENOMEM;
	}
	hyn_fun->tp_set_workmode(FAC_TEST_MODE,0);
	ret = hyn_fun->tp_get_test_result(rbuf,max_len);
	num_read = snprintf(buf, 128, "module selftest %s ret:%d\r\n",
			   ret==0 ? "pass":"failed", ret);
	if(!IS_ERR_OR_NULL(rbuf)){
		kfree(rbuf);
	}
	return num_read;
}

static  ssize_t hyn_selftest_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	/*place reserver*/
	return -EPERM;
}

//hyndumpfw
static  ssize_t hyn_dumpfw_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	HYN_ENTER();
	return (ssize_t)snprintf(buf, 128, "place reserver");
}

static  ssize_t hyn_dumpfw_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	u8 str[16]={'\0'};
	int max_len = hyn_fs_data->fw_updata_len+64;
	if(count<10){
		memcpy(str,buf,count);
		if(str[count-1]==0x0A) str[count-1] = '\0';
	}
	// HYN_INFO("state[%d]: %d,%s",(int)count,hyn_fs_data->fw_dump_state,str);
	if(0 == strcmp(str,"fwstart")){
		hyn_fs_data->fw_dump_state = 0;
		hyn_fs_data->fw_updata_addr = vmalloc(max_len);
		if(hyn_fs_data->fw_updata_addr == NULL){
			HYN_ERROR("vmalloc memory[%d] failed.\n",max_len);
			hyn_fs_data->fw_dump_state=-1;
			return -EPERM;
		}
		else{
			HYN_INFO("vmalloc memory[%d] success",max_len);
		}
	}
	else if(0 == strcmp(str,"fwend")){
		if(hyn_fs_data->fw_dump_state >= 0){
			const struct hyn_ts_fuc* hyn_fun = hyn_fs_data->hyn_fuc_used;
			memset(hyn_fs_data->fw_file_name,0,sizeof(hyn_fs_data->fw_file_name));
			hyn_fun->tp_updata_fw(hyn_fs_data->fw_updata_addr,hyn_fs_data->fw_updata_len);
			vfree(hyn_fs_data->fw_updata_addr);
		}
		hyn_fs_data->fw_dump_state = -2;
	}
	else if(hyn_fs_data->fw_dump_state >=0 && (hyn_fs_data->fw_dump_state + count) < max_len){
		memcpy(hyn_fs_data->fw_updata_addr+hyn_fs_data->fw_dump_state,buf,count);
		hyn_fs_data->fw_dump_state += count;
	}
	return count;
}

/////
///cat hyntpfwver
static  ssize_t hyn_mode_show(struct device *dev,	struct device_attribute *attr,char *buf)
{
	ssize_t num_read = 0;
	HYN_ENTER();
	num_read = snprintf(buf, 128, "is_charge:%d, is_glove:%d",hyn_fs_data->charge_is_enable,hyn_fs_data->glove_is_enable);
	return num_read;
}

static  ssize_t hyn_mode_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	const struct hyn_ts_fuc* hyn_fun = hyn_fs_data->hyn_fuc_used;
	HYN_ENTER();
	HYN_INFO("wlen:%d,%c%c",(int)count,buf[0],buf[1]);
	if(buf[0]=='c' || buf[0]=='C'){
		hyn_fun->tp_set_workmode(buf[1]=='1' ? CHARGE_ENTER:CHARGE_EXIT,1);
	}
	else if(buf[0]=='g' || buf[0]=='G'){
		hyn_fun->tp_set_workmode(buf[1]=='1' ? GLOVE_ENTER:GLOVE_EXIT,1);
	}
	return count;
}


static DEVICE_ATTR(hyntpfwver, S_IRUGO | S_IWUSR, hyn_tpfwver_show, hyn_tpfwver_store);
static DEVICE_ATTR(hyntpdbg, S_IRUGO | S_IWUSR, hyn_dbg_show, hyn_dbg_store);
static DEVICE_ATTR(hynselftest, S_IRUGO | S_IWUSR, hyn_selftest_show, hyn_selftest_store);
static DEVICE_ATTR(hyndumpfw, S_IRUGO | S_IWUSR, hyn_dumpfw_show, hyn_dumpfw_store);
static DEVICE_ATTR(hynswitchmode, S_IRUGO | S_IWUSR, hyn_mode_show, hyn_mode_store);

static struct attribute *hyn_attributes[] = {
	&dev_attr_hynswitchmode.attr,
	&dev_attr_hyndumpfw.attr,
	&dev_attr_hyntpdbg.attr,
	&dev_attr_hynselftest.attr,
	&dev_attr_hyntpfwver.attr,
	NULL
};

static struct attribute_group hyn_attribute_group = {
	.attrs = hyn_attributes
};

int hyn_create_sysfs(struct hyn_ts_data *ts_data)
{
	int ret = 0;
	hyn_fs_data = ts_data;
	hyn_fs_data->fw_dump_state = -1;
	ts_data->sys_node = &ts_data->dev->kobj;//kobject_create_and_add("hynitron_debug", NULL);
	if(IS_ERR_OR_NULL(ts_data->sys_node)){
		HYN_ERROR("kobject_create_and_add failed");
		return -1;
	}
	ret = sysfs_create_group(ts_data->sys_node, &hyn_attribute_group);
	// if(ret){
	// 	kobject_put(ts_data->sys_node);
	// 	return -1;
	// }
	HYN_INFO("sys_node creat success ,GKI version is [%s]",HYN_GKI_VER ? "enable":"disable");
	return ret;
}

void hyn_release_sysfs(struct hyn_ts_data *ts_data)
{
	if(!IS_ERR_OR_NULL(ts_data->sys_node)){
		sysfs_remove_group(ts_data->sys_node, &hyn_attribute_group);
		//kobject_put(ts_data->sys_node);
		ts_data->sys_node = NULL;
	}
}
