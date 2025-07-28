/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_lib/hyn_tool.c
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

#if HYN_APK_DEBUG_EN

#define HYN_PROC_DIR_NAME		"hyn_apk_tool"
#define HYN_PROC_NODE_NAME		"fops_nod"

static struct hyn_ts_data *hyn_tool_data = NULL;
static const struct hyn_ts_fuc* hyn_tool_fun = NULL;

static struct proc_dir_entry *g_tool_dir = NULL;
static struct proc_dir_entry *g_tool_node = NULL;

static int misc_state = -1;

#define READ_TPINF 		(0x0A)
#define WRITE_IIC 		(0x1A)  //len
#define READ_REG 		(0x2A)
#define READ_IIC 		(0x3A)
#define SET_WORK_MODE   (0x7A)
#define RESET_IC 		(0xCA)
#define UPDATA_FW		(0xAC)


#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0))
static int hyn_proc_tool_read(char *user_buf, char **start, off_t off, int count, int *eof, void *f_pos)
{
	unsigned char *buf = NULL;
	int ret_len = 0,ret = 0,copy_len = 0;
	int mt_total_len = hyn_tool_data->hw_info.fw_sensor_rxnum*hyn_tool_data->hw_info.fw_sensor_txnum*2
					+ (hyn_tool_data->hw_info.fw_sensor_rxnum + hyn_tool_data->hw_info.fw_sensor_txnum)*2;
	u8 *cmd = hyn_tool_data->host_cmd_save;
	mutex_lock(&hyn_tool_data->mutex_fs);

	if(*f_pos != 0){
		goto tool_read_end;
	}
	ret_len = (mt_total_len + sizeof(struct ts_frame))*2;
	// if(ret_len > count){
	// 	ret_len = count;
	// }
	if(hyn_tool_data->work_mode == FAC_TEST_MODE){
		ret_len += mt_total_len;
	}
	copy_len = ret_len > count ? count:ret_len;
	buf = kzalloc(ret_len, GFP_KERNEL);
	// buf = kzalloc(ret_len, GFP_KERNEL);
	if(buf == NULL){
		HYN_ERROR("zalloc GFP_KERNEL memory failed.\n");
		goto tool_read_end;
	}
	if(DIFF_MODE <= hyn_tool_data->work_mode && FAC_TEST_MODE >= hyn_tool_data->work_mode){
		wait_event_interruptible_timeout(hyn_tool_data->wait_irq,(atomic_read(&hyn_tool_data->hyn_irq_flg)==1),msecs_to_jiffies(500)); //
		atomic_set(&hyn_tool_data->hyn_irq_flg,0);
		if(hyn_tool_data->work_mode == FAC_TEST_MODE){
			if(NULL != hyn_tool_fun->tp_get_test_result){
				u8 restut[4];
				if(buf == NULL){
					HYN_ERROR("zalloc GFP_KERNEL memory failed.\n");
					goto tool_read_end;
				}
				*(int*)&restut = hyn_tool_fun->tp_get_test_result(buf,ret_len);
				HYN_INFO("test ret = %d",ret);
				memcpy(user_buf,restut,4);
				memcpy(user_buf+4,buf,copy_len-4);
			}
		}
		else{
			ret = hyn_tool_fun->tp_get_dbg_data(buf,ret_len);
			if(ret > 0)
				memcpy(user_buf,buf,copy_len);
		}
	}
	else{
		if(cmd[0] == READ_REG){  //0:head 1~4:reg 5:reg_len  6~7:read_len
			u32 reg_addr = U8TO32(cmd[4],cmd[3],cmd[2],cmd[1]); //eg (read A1 A2 len 256):2A 00 00 A1 A2 02 FF 00
			ret = hyn_wr_reg(hyn_tool_data,reg_addr,cmd[5],buf,copy_len);
			memcpy(user_buf,buf,copy_len);
		}
		else if(cmd[0] == READ_IIC){ //0:head 1~2:read_len
			ret = hyn_read_data(hyn_tool_data,buf,copy_len);
			memcpy(user_buf,buf,copy_len);
		}
		else if(cmd[0] == READ_TPINF){
			memcpy(user_buf,&hyn_tool_data->hw_info,sizeof(hyn_tool_data->hw_info));
		}
		else if(cmd[0] == UPDATA_FW){
			ret_len = sizeof(hyn_tool_data->fw_updata_process);
			memcpy(user_buf,&hyn_tool_data->fw_updata_process,copy_len);
			msleep(1);
		}
	}
tool_read_end:
	if(!IS_ERR_OR_NULL(buf)){
		kfree(buf);
	}
	mutex_unlock(&hyn_tool_data->mutex_fs);
	// HYN_INFO("count = %d copy len = %d ret = %d\r\n",(int)count,ret_len,ret);
	return count;
}
#else

static int  hyn_proc_tool_open(struct inode *pinode, struct file *pfile)
{
	HYN_ENTER();
	return 0;
}

static ssize_t hyn_proc_tool_read(struct file *page,char __user *user_buf, size_t count, loff_t *f_pos)
{
	unsigned char *buf = NULL;
	int ret_len = 0,ret = 0,copy_len = 0;
	int mt_total_len = hyn_tool_data->hw_info.fw_sensor_rxnum*hyn_tool_data->hw_info.fw_sensor_txnum*2
					+ (hyn_tool_data->hw_info.fw_sensor_rxnum + hyn_tool_data->hw_info.fw_sensor_txnum)*2;
	u8 *cmd = hyn_tool_data->host_cmd_save;
	mutex_lock(&hyn_tool_data->mutex_fs);

	if(*f_pos != 0){
		goto tool_read_end;
	}
	ret_len = (mt_total_len + sizeof(struct ts_frame))*2;
	// if(ret_len > count){
	// 	ret_len = count;
	// }
	if(hyn_tool_data->work_mode == FAC_TEST_MODE){
		ret_len += mt_total_len;
	}
	copy_len = ret_len > count ? count:ret_len;
	buf = kzalloc(ret_len, GFP_KERNEL);
	// buf = kzalloc(ret_len, GFP_KERNEL);
	if(buf == NULL){
		HYN_ERROR("zalloc GFP_KERNEL memory failed.\n");
		goto tool_read_end;
	}
	if(DIFF_MODE <= hyn_tool_data->work_mode && FAC_TEST_MODE >= hyn_tool_data->work_mode){
		wait_event_interruptible_timeout(hyn_tool_data->wait_irq,(atomic_read(&hyn_tool_data->hyn_irq_flg)==1),msecs_to_jiffies(500)); //
		atomic_set(&hyn_tool_data->hyn_irq_flg,0);
		if(hyn_tool_data->work_mode == FAC_TEST_MODE){
			if(NULL != hyn_tool_fun->tp_get_test_result){
				u8 restut[4];
				if(buf == NULL){
					HYN_ERROR("zalloc GFP_KERNEL memory failed.\n");
					goto tool_read_end;
				}
				*(int*)&restut = hyn_tool_fun->tp_get_test_result(buf,ret_len);
				HYN_INFO("test ret = %d",ret);
				ret = copy_to_user(user_buf,restut,4);
				ret |= copy_to_user(user_buf+4,buf,copy_len-4);
				if (ret)
					HYN_INFO("FAC_TEST_MODE copy_to_user ret = %d",ret);
			}
		}
		else{
			ret = hyn_tool_fun->tp_get_dbg_data(buf,ret_len);
			if(ret > 0) {
				ret = copy_to_user(user_buf,buf,copy_len);
				if (ret)
					HYN_INFO("other work_mode copy_to_user ret = %d",ret);
			}
		}
	}
	else{
		if(cmd[0] == READ_REG){  //0:head 1~4:reg 5:reg_len  6~7:read_len
			u32 reg_addr = U8TO32(cmd[4],cmd[3],cmd[2],cmd[1]); //eg (read A1 A2 len 256):2A 00 00 A1 A2 02 FF 00
			hyn_wr_reg(hyn_tool_data,reg_addr,cmd[5],buf,copy_len);
			ret = copy_to_user(user_buf,buf,copy_len);
			if (ret)
				HYN_INFO("READ_REG copy_to_user ret = %d",ret);
		}
		else if(cmd[0] == READ_IIC){ //0:head 1~2:read_len
			hyn_read_data(hyn_tool_data,buf,copy_len);
			ret = copy_to_user(user_buf,buf,copy_len);
			if (ret)
				HYN_INFO("READ_IIC copy_to_user ret = %d",ret);
		}
		else if(cmd[0] == READ_TPINF){
			ret = copy_to_user(user_buf,&hyn_tool_data->hw_info,sizeof(hyn_tool_data->hw_info));
			if (ret)
				HYN_INFO("READ_TPINF copy_to_user ret = %d",ret);
		}
		else if(cmd[0] == UPDATA_FW){
			// ret_len = sizeof(hyn_tool_data->fw_updata_process);
			ret = copy_to_user(user_buf,&hyn_tool_data->fw_updata_process,copy_len);
			if (ret)
				HYN_INFO("UPDATA_FW copy_to_user ret = %d",ret);
			msleep(1);
		}
	}
tool_read_end:
	if(!IS_ERR_OR_NULL(buf)){
		kfree(buf);
	}
	mutex_unlock(&hyn_tool_data->mutex_fs);
	// HYN_INFO("count = %d copy len = %d ret = %d\r\n",(int)count,ret_len,ret);
	return count;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0))
static int hyn_proc_tool_write(struct file *file, const char __user *buffer,size_t count, void *data)
#else
static ssize_t hyn_proc_tool_write(struct file *file, const char __user *buffer,size_t count, loff_t *data)
#endif
{
	u8 *cmd = NULL;
	int ret = 0;
	HYN_ENTER();
	mutex_lock(&hyn_tool_data->mutex_fs);
	#define MAX_W_SIZE (1024+8)
	if(count > MAX_W_SIZE){
		ret = -1;
		HYN_ERROR("count > MAX_W_SIZE\n");
		goto ERRO_WEND;
	}
	cmd = kzalloc(count, GFP_KERNEL);
	if(IS_ERR_OR_NULL(cmd)){
		ret = -1;
		HYN_ERROR("kzalloc failed\n");
		goto ERRO_WEND;
	}
	if(copy_from_user(cmd, buffer, count)){
		ret = -1;
		HYN_ERROR("copy data from user space failed.\n");
		goto ERRO_WEND;
	}
	memcpy(hyn_tool_data->host_cmd_save,cmd,count <sizeof(hyn_tool_data->host_cmd_save) ? count:sizeof(hyn_tool_data->host_cmd_save));
	HYN_INFO("cmd:0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x***len:%d ", cmd[0], cmd[1],cmd[2], cmd[3],cmd[4], cmd[5],cmd[6], cmd[7],(u16)count);

	switch(cmd[0]){
		case UPDATA_FW: // apk download bin
			HYN_INFO("updata file_name =%s",&cmd[1]);
			snprintf(hyn_tool_data->fw_file_name, sizeof(hyn_tool_data->fw_file_name), "%127.127s", &cmd[1]);
			hyn_tool_data->fw_updata_process = 0;
			if(0==queue_work(hyn_tool_data->hyn_workqueue,&hyn_tool_data->work_updata_fw)){
				HYN_ERROR("queue_work work_updata_fw failed");
			}
			break;
		case SET_WORK_MODE://
			atomic_set(&hyn_tool_data->hyn_irq_flg,0);
			ret = hyn_tool_fun->tp_set_workmode(cmd[1],0);
			break;
		case RESET_IC:
			hyn_tool_fun->tp_rest();
			break;
		case WRITE_IIC: //
			ret = hyn_write_data(hyn_tool_data,&cmd[1],2,count-1);
			break;
		default:
			break;
	}
ERRO_WEND:
	if(!IS_ERR_OR_NULL(cmd)){
		kfree(cmd);
	}
	mutex_unlock(&hyn_tool_data->mutex_fs);
	return ret == 0 ? count:-1;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0))
static const struct file_operations proc_tool_fops = {
	.owner		= THIS_MODULE,
	.open	   = hyn_proc_tool_open,
	.read		= hyn_proc_tool_read,
	.write		= hyn_proc_tool_write,
};

static struct miscdevice  hyn_tool_dev =  {
	.fops  =  &proc_tool_fops,
	.minor =  MISC_DYNAMIC_MINOR,
	.name  =  HYN_PROC_DIR_NAME,
	.mode = S_IFREG|S_IRWXUGO,
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops proc_tool_ops = { ////>=5.06
	.proc_read = hyn_proc_tool_read,
	.proc_write = hyn_proc_tool_write,
};
#endif
#endif



int hyn_tool_fs_int(struct hyn_ts_data *ts_data)
{
	int ret = 0;
	hyn_tool_data = ts_data;
	hyn_tool_fun = ts_data->hyn_fuc_used;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0))
	ret = misc_register(&hyn_tool_dev);
	misc_state = ret;
	HYN_INFO("hyn_tool_dev register %s",ret ? "failed":"success");
#endif
	g_tool_dir = proc_mkdir(HYN_PROC_DIR_NAME, NULL);
	if (IS_ERR_OR_NULL(g_tool_dir)){
		HYN_INFO("proc_mkdir %s failed.\n",HYN_PROC_DIR_NAME);
		return -1;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0))
	g_tool_node = create_proc_entry(HYN_PROC_NODE_NAME, 0644, g_tool_dir);
	g_tool_node->write_proc = hyn_proc_tool_write;
	g_tool_node->read_proc = hyn_proc_tool_read;
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)) //>=3.10
	g_tool_node = proc_create(HYN_PROC_NODE_NAME, 0644|S_IFREG, g_tool_dir, &proc_tool_fops);
	//proc_create_data(HYN_PROC_NODE_NAME, 0644 | S_IFREG, g_tool_dir, (void *)&proc_tool_fops, (void *)ts_data->client);
#else
	g_tool_node = proc_create(HYN_PROC_NODE_NAME, 0644|S_IFREG, g_tool_dir, &proc_tool_ops);
#endif
	if (IS_ERR_OR_NULL(g_tool_node)) {
		HYN_INFO("proc_create %s failed.\n",HYN_PROC_NODE_NAME);
		remove_proc_entry(HYN_PROC_DIR_NAME, NULL);
		return -ENOMEM;
	}
	HYN_INFO("apk node creat success");
	return 0;
}

void hyn_tool_fs_exit(void)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0))
	if(!IS_ERR_OR_NULL(g_tool_dir)){
		remove_proc_entry(HYN_PROC_DIR_NAME, NULL);
	}
#else
	if(!IS_ERR_OR_NULL(g_tool_dir)){
		proc_remove(g_tool_dir);
	}
	if(0==misc_state){
		misc_deregister(&hyn_tool_dev);
	}
#endif
}

#endif
