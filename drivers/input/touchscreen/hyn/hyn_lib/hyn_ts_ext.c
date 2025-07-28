/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_lib/hyn_ts_ext.c
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

int hyn_power_source_ctrl(struct hyn_ts_data *ts_data, int enable)
{
	int ret = 0;
	HYN_ENTER();
	if(IS_ERR_OR_NULL(ts_data) || IS_ERR_OR_NULL(ts_data->plat_data.vdd_ana)){
		return ret;
	}
	if(ts_data->power_is_on != enable){
		if(enable){
			if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_ana)){
				ret |= regulator_enable(ts_data->plat_data.vdd_ana);
			}
			if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_i2c)){
				ret |= regulator_enable(ts_data->plat_data.vdd_i2c);
			}
		}
		else{
			if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_ana)){
				ret |= regulator_disable(ts_data->plat_data.vdd_ana);
			}
			if(!IS_ERR_OR_NULL(ts_data->plat_data.vdd_i2c)){
				ret |= regulator_disable(ts_data->plat_data.vdd_i2c);
			}
		}
		ts_data->power_is_on = enable;
	}
	if(ret)
		HYN_ERROR("set vdd %s regulator failed,ret=%d",enable ? "off":"on",ret);
	return ret;
}

void hyn_irq_set(struct hyn_ts_data *ts_data, u8 value)
{
	// HYN_ENTER();
	if(atomic_read(&ts_data->irq_is_disable) != value){
		if(value ==0){
			disable_irq(ts_data->gpio_irq);
			msleep(1); //wait switch
		}
		else{
			enable_irq(ts_data->gpio_irq);
		}
		atomic_set(&ts_data->irq_is_disable,value);
		// HYN_INFO("IRQ %d",value);
	}
}

void hyn_set_i2c_addr(struct hyn_ts_data *ts_data,u8 addr)
{
#ifdef I2C_PORT
	ts_data->client->addr = addr;
#endif
}

u16 hyn_sum16(int val, u8* buf,u16 len)
{
	u16 sum = val;
	while(len--) sum += *buf++;
	return sum;
}

u32 hyn_sum32(int val, u32* buf,u16 len)
{
	u32 sum = val;
	while(len--) sum += *buf++;
	return sum;
}

void hyn_esdcheck_switch(struct hyn_ts_data *ts_data, u8 enable)
{
#if ESD_CHECK_EN
	if(IS_ERR_OR_NULL(ts_data->hyn_workqueue) || IS_ERR_OR_NULL(&ts_data->esdcheck_work)) return;
	if(enable){
		ts_data->esd_fail_cnt = 0;
		queue_delayed_work(ts_data->hyn_workqueue, &ts_data->esdcheck_work,
						msecs_to_jiffies(1000));
	}
	else{
		cancel_delayed_work_sync(&ts_data->esdcheck_work);
	}
#endif
}

int hyn_copy_for_updata(struct hyn_ts_data *ts_data,u8 *buf,u32 offset,u16 len)
{
	int ret = -1;
#if (HYN_GKI_VER==0)
	struct file *fp;
	loff_t pos;
	u8 *pdata;

	// HYN_ENTER();

	if(strlen(ts_data->fw_file_name) == 0)
		return ret;
	fp = filp_open(ts_data->fw_file_name,O_RDONLY, 0);//
	if (IS_ERR(fp)) {
		HYN_INFO("open file %s failed \r\n", ts_data->fw_file_name);
		return -EIO;
	}

	// fp->f_op->llseek(fp, offset, 0);
	fp->f_op->llseek(fp, 0, 0);
	pdata = kzalloc(len, GFP_KERNEL);
	if(IS_ERR_OR_NULL(pdata)){
		HYN_ERROR("kzalloc failed");
		filp_close(fp, NULL);
		return -1;
	}
	pos = offset;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	{
	mm_segment_t old_fs;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret=vfs_read(fp, pdata, len, &pos);
	set_fs(old_fs);
	}
#else
	ret = hyn_fs_read(fp,pdata, len,&pos);
#endif
	filp_close(fp, NULL);
	// HYN_INFO("rlen = %d nlen = %d",ret,len);
	if(ret <= len){
		memcpy(buf,pdata,len);
		ret = 0;
	}
	else{
		ret = -3;
	}
	kfree(pdata);
#endif
	return ret;
}

int hyn_wait_irq_timeout(struct hyn_ts_data *ts_data,int msec)
{
	atomic_set(&ts_data->hyn_irq_flg,0);
	while(msec--){
		msleep(1);
		if(atomic_read(&ts_data->hyn_irq_flg)==1){
			atomic_set(&ts_data->hyn_irq_flg,0);
			msec = -1;
			break;
		}
	}
	return msec == -1 ? 0:-1;
}

#define MAX_WROD_LEN	(64)
int hyn_get_word(u8**sc_str, u8* ds_str)
{
	u8 ch,cnt = 0,nul_flg = 0;
	while(1){
		ch = **sc_str;
		*sc_str += 1;
		if((ch==' '&& nul_flg) || ch=='\t' || ch=='\0' || (ch=='\r' && **sc_str == '\n') || ch=='\n'|| ch==','|| ch=='=' || cnt>=MAX_WROD_LEN){
			*ds_str++ = '\0';
			break;
		}
		if(ch >= 'A' && ch <= 'Z') ch = ch + 'a' - 'A';
		if(ch!=' '){
			*ds_str++ = ch;
			nul_flg = 1;
		}
		cnt++;
	}
	return cnt;
}

int hyn_str_2_num(char *str,u8 type)
{
	int ret = 0,step = 0,flg = 0,cnt = 15;
	char ch;
	while(*str != '\0' && --cnt){
		ch = *str++;
		if(ch==' ') continue;
		else if(ch=='-' && step==0){
			flg = 1;
			continue;
		}
		if(type == 10){
			if(ch <= '9' && ch >= '0'){
				step++;
				ret *= 10;
				ret += (ch - '0');
			}
			else{
				cnt = 0;
				break;
			}
		}
		else{
			if(ch <= '9' && ch >= '0'){
				step++;
				ret *= 16;
				ret += (ch - '0');
			}
			else if(ch <= 'f' && ch >= 'a'){
				step++;
				ret *= 16;
				ret += (10 + ch - 'a');
			}
			else{
				cnt = 0;
				break;
			}
		}
	}
	// if(cnt == 0 || step==0)HYN_ERROR("failed");
	return (cnt == 0 || step==0) ? 0xAC5AC5AC : (flg ? -ret:ret);
}

static int hyn_get_threshold(char *filename,char *match_string,s16 *pstore, u16 len)
{
	int ret = 0;
	HYN_ENTER();
#if (HYN_GKI_VER==0)
	{
	u8 *buf = NULL,*ptr = NULL;
	u8 tmp_word[66];
	u16 i;
	int dec,cnt;
	off_t fsize;
	struct file *fp;
	loff_t pos;

	if(strlen(filename) == 0)
		return ret;

	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		HYN_INFO("error occurred while opening file %s.\n", filename);
		return -EIO;
	}
	fp->f_op->llseek(fp, 0, 0);
	fsize = fp->f_inode->i_size;
	buf = vmalloc(fsize);
	if(IS_ERR_OR_NULL(buf)){
		HYN_ERROR("vmalloc failed");
		filp_close(fp, NULL);
		return -1;
	}
	pos = 0;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	{mm_segment_t old_fs;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret=vfs_read(fp,buf, fsize, &pos);
	set_fs(old_fs);}
#else
	ret = hyn_fs_read(fp,buf,fsize,&pos);
#endif
	HYN_INFO("read %s %s.ret:%d.\n",filename,ret==fsize ? "success":"failed",ret);
	filp_close(fp, NULL);

////read finsh start match key string
	i = fsize;
	ptr = buf;
	while(--i){
		if(*ptr++ == '\n'){
			cnt = strlen(match_string);
			while(--cnt){
				if(ptr[cnt]!= match_string[cnt]){
					break;
				}
			}
			if(cnt == 0 && ptr[0]== match_string[0]){
				// HYN_INFO("match %s",match_string);
				break; //ignor idx 0,1
			}
		}
	}
	if(i == 0){
		ret = -1;
		goto MATCH_FAIL1;
	}

///////match key string end
	i = 0;
	while(i<10){
		cnt = hyn_get_word(&ptr,tmp_word);
		if(cnt==0){
			i++;
			continue;
		}
		// HYN_INFO("@@%s",tmp_word);
		dec = hyn_str_2_num(tmp_word,10);
		if(dec == 0xAC5AC5AC) continue;
		*pstore = dec;
		pstore++;
		i = 0;
		if(--len == 0) break;
	}
	ret = len ? -1:0;

MATCH_FAIL1:
	if(!IS_ERR_OR_NULL(buf)) vfree(buf);
	}
#endif
	return ret;
}


int hyn_factory_multitest(struct hyn_ts_data *ts_data ,char *cfg_path, u8 *data,s16 *test_th,u8 test_item)
{
	int ret = 0;
	struct tp_info *ic = &ts_data->hw_info;
	u16 mt_len = ic->fw_sensor_rxnum*ic->fw_sensor_txnum,i=0;
	u16 st_len = ic->fw_sensor_txnum + ic->fw_sensor_rxnum;
	s16 *raw_h,*raw_l,*raw_s,*scap_p,tmp=0;
	raw_h = (s16*)data;
	raw_l = raw_h + mt_len;
	raw_s = raw_l + mt_len;
	scap_p = raw_s + st_len;

	if(test_item & MULTI_OPEN_TEST){
	//judge low TH
		HYN_INFO("check MULTI_OPEN_TEST OpenMin");
		if(hyn_get_threshold(cfg_path,"TX0OpenMin",test_th,mt_len)){
			HYN_ERROR("read threshold failed");
			return FAC_GET_CFG_FAIL;
		}
		for(i = mt_len; i< mt_len; i++){
			if(*(raw_h+i)-*(raw_l+i) < test_th[i]) break;
		}
		if(i == mt_len){
			HYN_INFO("open low pass");
		}
		else{
			HYN_ERROR("open low test failed row= %d clo= %d %d<(th)%d"
					,i/ic->fw_sensor_rxnum,i%ic->fw_sensor_rxnum,*(raw_h+i)-*(raw_l+i),test_th[i]);
			return FAC_TEST_OPENL_FAIL;
		}
	//judge high TH
		HYN_INFO("check MULTI_OPEN_TEST OpenMax");
		if(hyn_get_threshold(cfg_path,"TX0OpenMax",test_th,mt_len)){
			HYN_ERROR("read threshold failed");
			return FAC_GET_CFG_FAIL;
		}
		for(i = 0; i< mt_len; i++){
			if(*(raw_h+i)-*(raw_l+i) > test_th[i]) break;
		}
		if(i == mt_len){
			HYN_INFO("open high pass");
		}
		else{
			HYN_ERROR("open high test failed row= %d clo= %d %d>(th)%d"
					,i/ic->fw_sensor_rxnum,i%ic->fw_sensor_rxnum,*(raw_h+i)-*(raw_l+i),test_th[i]);
			return FAC_TEST_OPENH_FAIL;
		}
	}

	if(test_item & MULTI_SHORT_TEST){
	//judge short TH
		if(hyn_get_threshold(cfg_path,"FactoryTxShortTh",&tmp,1)){
			HYN_ERROR("read threshold failed");
			return FAC_GET_CFG_FAIL;
		}
		for(i = 0; i< st_len; i++){
			if(*(raw_s+i) < tmp) break;
		}
		HYN_INFO("%s,shortth = %d",i == st_len ? "short test pass":"short test failed",tmp);
		if(i != st_len){
			return FAC_TEST_SHORT_FAIL;
		}
	}

	if(test_item & MULTI_SCAP_TEST){
	// self captest
		ret = hyn_get_threshold(cfg_path,"RxSCapScanMin",test_th,ic->fw_sensor_rxnum);
		ret |= hyn_get_threshold(cfg_path,"TxSCapScanMin",test_th+ic->fw_sensor_rxnum,ic->fw_sensor_txnum);
		ret |= hyn_get_threshold(cfg_path,"RxSCapScanMax",test_th+st_len,ic->fw_sensor_rxnum);
		ret |= hyn_get_threshold(cfg_path,"TxSCapScanMax",test_th+st_len+ic->fw_sensor_rxnum,ic->fw_sensor_txnum);
		if(ret){
			HYN_ERROR("read threshold failed");
			return FAC_GET_CFG_FAIL;
		}
		else{
			// scap_fix = 128*(test_th[st_len+2] - test_th[2])/(abs(*(scap_p+2))+1);
			for(i = 0; i< st_len; i++){
				tmp = *(scap_p+i);//*scap_fix/128;
				if(tmp < test_th[i] || tmp > test_th[st_len+i]) break;
			}
			if(i == st_len){
				HYN_INFO("scap test pass");
			}
			else{
				HYN_ERROR("scap test failed[%d] %d<%d>%d",i,test_th[i],tmp,test_th[st_len+i]);
				return FAC_TEST_SCAP_FAIL;
			}
		}
	}
	return 0;
}

#if (HYN_GKI_VER==0)
static int arry_to_string(s16 *from,u16 cnt,u8 *des,u16 maxlen)
{
	int str_cnt = 0;
	int ret;
	while(cnt--){
		ret = snprintf(&des[str_cnt],maxlen, "%d,",*from++);
		str_cnt += ret;
		if(str_cnt > maxlen-ret){
			str_cnt = maxlen;
			HYN_ERROR("str full");
			break;
		}
	}
	return str_cnt;
}
#endif

int hyn_fac_test_log_save(char *log_name,struct hyn_ts_data *ts_data,s16 *test_data, int test_ret, u8 test_item)
{
	HYN_ENTER();
#if (HYN_GKI_VER==0)
	{
	struct file *fp;
	u8 w_buf[256],i;
	int ret = -1;
	if(strlen(log_name) == 0)
		return ret;

	fp = filp_open(log_name, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		HYN_INFO("error occurred while opening file %s.\n", log_name);
		return -EIO;
	}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	{mm_segment_t old_fs;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	#undef hyn_fs_write
	#define hyn_fs_write  vfs_write
#endif
	ret = snprintf(w_buf,sizeof(w_buf), test_ret ==0 ? "factory test pass\n":"factory test ng\n");
	hyn_fs_write(fp, w_buf, min_t(size_t, ret, sizeof(w_buf)-1), &fp->f_pos);
	if(test_ret == FAC_GET_DATA_FAIL){
		ret = snprintf(w_buf,sizeof(w_buf), "read fac_test data fail\n");
		hyn_fs_write(fp, w_buf, ret, &fp->f_pos);
	}
	else{

		if(test_item & MULTI_OPEN_TEST){
			ret = snprintf(w_buf,sizeof(w_buf), "open high test data\n");
			hyn_fs_write(fp, w_buf, ret, &fp->f_pos);
			for(i = 0; i< ts_data->hw_info.fw_sensor_txnum; i++){
				ret = arry_to_string(test_data,ts_data->hw_info.fw_sensor_rxnum,w_buf,sizeof(w_buf)-2);
				w_buf[ret+1] = '\n';
				hyn_fs_write(fp, w_buf, ret+2, &fp->f_pos);
				test_data += ts_data->hw_info.fw_sensor_rxnum;
			}

			ret = snprintf(w_buf,sizeof(w_buf), "open low test data\n");
			hyn_fs_write(fp, w_buf, ret, &fp->f_pos);
			for(i = 0; i< ts_data->hw_info.fw_sensor_txnum; i++){
				ret = arry_to_string(test_data,ts_data->hw_info.fw_sensor_rxnum,w_buf,sizeof(w_buf)-2);
				w_buf[ret+1] = '\n';
				hyn_fs_write(fp, w_buf, ret+2, &fp->f_pos);
				test_data += ts_data->hw_info.fw_sensor_rxnum;
			}
		}

		if(test_item & MULTI_SHORT_TEST){
			ret = snprintf(w_buf,sizeof(w_buf), "short test data\n");
			hyn_fs_write(fp, w_buf, ret, &fp->f_pos);
			ret = arry_to_string(test_data,ts_data->hw_info.fw_sensor_rxnum,w_buf,sizeof(w_buf)-2);
			w_buf[ret+1] = '\n';
			hyn_fs_write(fp, w_buf, ret+2, &fp->f_pos);
			test_data += ts_data->hw_info.fw_sensor_rxnum;
			ret = arry_to_string(test_data,ts_data->hw_info.fw_sensor_txnum,w_buf,sizeof(w_buf)-2);
			w_buf[ret+1] = '\n';
			hyn_fs_write(fp, w_buf, ret+2, &fp->f_pos);
			test_data += ts_data->hw_info.fw_sensor_txnum;
		}

		if(test_item & MULTI_SCAP_TEST){
			ret = snprintf(w_buf,sizeof(w_buf), "scap test data\n");
			hyn_fs_write(fp, w_buf, ret, &fp->f_pos);
			ret = arry_to_string(test_data,ts_data->hw_info.fw_sensor_rxnum,w_buf,sizeof(w_buf)-2);
			w_buf[ret+1] = '\n';
			hyn_fs_write(fp, w_buf, ret+2, &fp->f_pos);
			test_data += ts_data->hw_info.fw_sensor_rxnum;
			ret = arry_to_string(test_data,ts_data->hw_info.fw_sensor_txnum,w_buf,sizeof(w_buf)-2);
			w_buf[ret+1] = '\n';
			hyn_fs_write(fp, w_buf, ret+2, &fp->f_pos);
			// test_data += ts_data->hw_info.fw_sensor_txnum;
		}
	}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	set_fs(old_fs);}
#endif
	filp_close(fp, NULL);
	}
#endif
	return 0;
}
