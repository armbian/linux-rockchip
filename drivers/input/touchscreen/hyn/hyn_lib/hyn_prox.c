/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_lib/hyn_prox.c
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


static struct hyn_ts_data *hyn_prox_data = NULL;
static const struct hyn_ts_fuc* hyn_prox_fun = NULL;


#if (HYN_PROX_TYEP==4) //Spread

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>

#define CTP_IOCTL_MAGIC				0x13
#define CTP_IOCTL_PROX_ON  			_IOW(CTP_IOCTL_MAGIC, 1, int)
#define CTP_IOCTL_PROX_OFF			_IOW(CTP_IOCTL_MAGIC, 2, int)
#define CTP_PROXIMITY_DEVICE_NAME	"ltr_558als"				 //match HAL name

static long hyn_ioctl_operate(struct file *file, unsigned int cmd, unsigned long arg)
{
	HYN_INFO("prox_ioctl_operate %d!\n", cmd);
	switch(cmd)
	{
		case CTP_IOCTL_PROX_ON:
			hyn_prox_fun->tp_prox_handle(1);
			break;
		case CTP_IOCTL_PROX_OFF:
			hyn_prox_fun->tp_prox_handle(0);
			break;
		default:
			HYN_ERROR("%s: invalid cmd %d\n", __func__, _IOC_NR(cmd));
			return -EINVAL;
	}
	return 0;
}

static struct file_operations hyn_proximity_fops = {
	.owner = THIS_MODULE,
	.open = NULL,
	.release = NULL,
	.unlocked_ioctl = hyn_ioctl_operate
};

struct miscdevice hyn_proximity_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = CTP_PROXIMITY_DEVICE_NAME,		//match the hal's name
	.fops = &hyn_proximity_fops,
	.mode = 0x666,
};

int hyn_proximity_int(struct hyn_ts_data *ts_data)
{
	HYN_ENTER();
	int ret;
	hyn_prox_data = ts_data;
	hyn_prox_fun = ts_data->hyn_fuc_used;
	hyn_prox_data->prox_is_enable = 0;
	ret = misc_register(&hyn_proximity_misc);
	if (ret < 0){
		HYN_ERROR("%s: could not register misc device\n", __func__);
		goto misc_device_failed;
	}
	input_set_abs_params(hyn_prox_data->input_dev, ABS_DISTANCE, 0, 1, 0, 0);
	return 0;
	misc_device_failed:
	return ret;
}

int hyn_proximity_report(u8 proximity_value)
{
	HYN_ENTER();
	input_report_abs(hyn_prox_data->input_dev, ABS_DISTANCE, proximity_value);
	input_mt_sync(hyn_prox_data->input_dev);
	input_sync(hyn_prox_data->input_dev);
	return 0;
}

int hyn_proximity_exit(void)
{
	HYN_ENTER();
	misc_deregister(&hyn_proximity_misc);
	return 0;
}

#elif (HYN_PROX_TYEP==3) //alps sensor
#include <hwmsensor.h>
//#include <hwmsen_dev.h>
#include <sensors_io.h>
#include <alsps.h>

static int ps_open_report_data(int open)
{
	/* should queue work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this type of enable , Psensor only enabled but not report inputEvent to HAL */
static int ps_enable_nodata(int en)
{
	int ret = 0;
	HYN_INFO("[PROXIMITY]SENSOR_ENABLE value = %d", en);
	/* Enable proximity */
	ret = hyn_prox_fun->tp_prox_handle(en);
	return ret;
}

static int ps_set_delay(u64 ns)
{
	return 0;
}

static int ps_flush(void)
{
	return ps_flush_report();
}

static int ps_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	int value = 0;

	value = (int)samplingPeriodNs / 1000 / 1000;
	return 0;
}

static int ps_get_data(int *value, int *status)
{
	*value = (int)hyn_prox_data->prox_state;
	HYN_INFO("fts_proximity_data.detect = %d\n", *value);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return 0;
}

static int ps_calibration(int type, int value)
{
	return 0;
}

static int ps_threshold_setting(int type, int value[2])
{
	return 0;
}

int ps_local_init(void)
{
	int err = 0;
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };
	HYN_ENTER();
	ps_ctl.is_use_common_factory = false;
	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.batch = ps_batch;
	ps_ctl.flush = ps_flush;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.ps_calibration = ps_calibration;
	ps_ctl.ps_threshold_setting = ps_threshold_setting;
	ps_ctl.is_report_input_direct = true;
	ps_ctl.is_support_batch = false;
	err = ps_register_control_path(&ps_ctl);
	HYN_INFO("psregister control_path %s = %d\n",err? "fail":"success", err);
	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	HYN_INFO("psregister data_path %s = %d\n",err? "fail":"success", err);

	return err;
}
int ps_local_uninit(void)
{
	return 0;
}

struct alsps_init_info ps_init_info = {
	.name = "hyn_ts",
	.init = ps_local_init,
	.uninit = ps_local_uninit,
};


int hyn_proximity_int(struct hyn_ts_data *ts_data)
{
	int ret = 0;
	HYN_ENTER();
	hyn_prox_data = ts_data;
	hyn_prox_fun = ts_data->hyn_fuc_used;
	hyn_prox_data->prox_is_enable = 0;
	hyn_prox_data->prox_state = PS_FAR_AWAY;
	alsps_driver_add(&ps_init_info);
	// ps_local_init();
	HYN_INFO("hyn_proximity_int exit");
	return ret;
}

int hyn_proximity_report(u8 proximity_value)
{
	int ret;
	ret = ps_report_interrupt_data(proximity_value);
	return ret;
}

int hyn_proximity_exit(void)
{
	HYN_ENTER();
	return 0;
}

#elif (HYN_PROX_TYEP==2)//mtk sensor

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>

#define SENSOR_DELAY	  				 0
#define SENSOR_ENABLE	 				 0
#define SENSOR_GET_DATA   				 0
#define SENSOR_STATUS_ACCURACY_MEDIUM	0

struct hwmsen_object obj_ps;

static int hyn_proximiy_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0,value;
	HYN_INFO("prox command = 0x%02X\n", command);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int))){
				HYN_ERROR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int))){
				HYN_ERROR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}else{
				value = *(int *)buff_in;
				hyn_prox_fun->tp_prox_handle(value > 0 ? 1 : 0);
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL)){
				HYN_ERROR("get sensor data parameter error!\n");
				err = -EINVAL;
			}else{
				hyn_proximity_report(hyn_prox_data->prox_state);
			}
			break;
		default:
			HYN_ERROR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	return err;
}

int hyn_proximity_int(struct hyn_ts_data *ts_data)
{
	HYN_ENTER();
	int ret;
	hyn_prox_data = ts_data;
	hyn_prox_fun = ts_data->hyn_fuc_used;
	hyn_prox_data->prox_is_enable = 0;
	obj_ps.polling = 0;//interrupt mode
	obj_ps.sensor_operate = hyn_proximiy_operate;
	ret = hwmsen_attach(ID_PROXIMITY, &obj_ps);
	return ret;
}

int hyn_proximity_report(u8 proximity_value)
{
	HYN_ENTER();
	int ret=0;
	struct hwm_sensor_data sensor_data;
	sensor_data.values[0] = proximity_value;
	sensor_data.value_divide = 1;
	sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
	ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);
	if (ret){
		HYN_ERROR(" proxi call hwmsen_get_interrupt_data failed= %d\n", err);
	}
	return ret;
}

int hyn_proximity_exit(void)
{
	HYN_ENTER();
	return 0;
}

#elif (HYN_PROX_TYEP==1)//default sensor

static ssize_t hyn_prox_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "proximity is %s value:%d\n",hyn_prox_data->prox_is_enable ? "enable":"disable" ,hyn_prox_data->prox_state);
}

/* Allow users to enable/disable prox */
static ssize_t hyn_prox_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	unsigned long	enable;
	enable = simple_strtoul(buf, NULL, 10);
	hyn_prox_fun->tp_prox_handle(enable > 0 ? 1 : 0);
	return count;
}

static DEVICE_ATTR (hyn_prox_enable, S_IRUGO|S_IWUSR, hyn_prox_show, hyn_prox_store);
static struct attribute *hyn_prox_enable_attrs[] =
{
	&dev_attr_hyn_prox_enable.attr,
	NULL,
};

static struct attribute_group hyn_prox_group =
{
	.attrs = hyn_prox_enable_attrs,
};

int hyn_proximity_int(struct hyn_ts_data *ts_data)
{
	HYN_ENTER();
	int ret;
	hyn_prox_data = ts_data;
	hyn_prox_fun = ts_data->hyn_fuc_used;
	hyn_prox_data->prox_is_enable = 0;
	__set_bit(KEY_SLEEP, hyn_prox_data->input_dev->evbit);
	__set_bit(KEY_WAKEUP, hyn_prox_data->input_dev->evbit);

	ret = sysfs_create_group(&ts_data->dev->kobj, &hyn_prox_group);
	if(ret){
		HYN_ERROR("prox_sys_node creat failed");
	}
	return ret;
}

int hyn_proximity_report(u8 proximity_value)
{
	HYN_ENTER();
	int keycode = proximity_value ? KEY_SLEEP:KEY_WAKEUP;
	input_report_key(hyn_prox_data->input_dev, keycode, 1);
	input_sync(hyn_prox_data->input_dev);
	input_report_key(hyn_prox_data->input_dev, keycode, 0);
	input_sync(hyn_prox_data->input_dev);
	return 0;
}

int hyn_proximity_exit(void)
{
	HYN_ENTER();
	sysfs_remove_group(&hyn_prox_data->dev->kobj, &hyn_prox_group);
	return 0;
}
#elif (HYN_PROX_TYEP==0)
int hyn_proximity_int(struct hyn_ts_data *ts_data)
{
	HYN_ENTER();
	hyn_prox_data = ts_data;
	hyn_prox_fun = ts_data->hyn_fuc_used;
	return 0;
}

int hyn_proximity_report(u8 proximity_value)
{
	HYN_ENTER();
	return 0;
}

int hyn_proximity_exit(void)
{
	HYN_ENTER();
	return 0;
}

#endif
