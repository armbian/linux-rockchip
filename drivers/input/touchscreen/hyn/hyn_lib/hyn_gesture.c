/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_lib/hyn_gesture.c
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

static const u8 gesture_key[] = {KEY_U,KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT,
				 KEY_O,KEY_E, KEY_M,   KEY_L,   KEY_W,
				 KEY_S,KEY_V, KEY_C,   KEY_Z,   KEY_POWER};

static struct hyn_ts_data *gesture_data = NULL;


/* sysfs gesture node
 *   read example: cat  hyn_gesture_mode		---read gesture mode
 *   write example:echo 01 > hyn_gesture_mode   ---write gesture mode to 01
 */

static  ssize_t hyn_gesture_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;
	HYN_ENTER();
	mutex_lock(&gesture_data->mutex_fs);
	count = sprintf(buf, "Gesture Mode: %s\n", gesture_data->gesture_is_enable ? "On" : "Off");
	count += sprintf(buf + count, "Reg = %d\n", gesture_data->gesture_id);
	mutex_unlock(&gesture_data->mutex_fs);
	return count;
}

static  ssize_t hyn_gesture_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	HYN_ENTER();
	mutex_lock(&gesture_data->mutex_fs);
	if (buf[0]==1){
		HYN_INFO("[GESTURE]enable gesture");
		gesture_data->gesture_is_enable= ENABLE;
	}else if (buf[0]==0){
		HYN_INFO("[GESTURE]disable gesture");
		gesture_data->gesture_is_enable = DISABLE;
	}
	mutex_unlock(&gesture_data->mutex_fs);
	return count;
}

static DEVICE_ATTR (hyn_gesture_mode, S_IRUGO|S_IWUSR, hyn_gesture_show, hyn_gesture_store);
static struct attribute *hyn_gesture_mode_attrs[] =
{
	&dev_attr_hyn_gesture_mode.attr,
	NULL,
};

static struct attribute_group hyn_gesture_group =
{
	.attrs = hyn_gesture_mode_attrs,
};

int hyn_gesture_init(struct hyn_ts_data *ts_data)
{
	u8 i;
	int ret = -1;
	struct input_dev *dev = ts_data->input_dev;
	HYN_ENTER();
	gesture_data = ts_data;
	for(i = 0; i< sizeof(gesture_key); i++){
		input_set_capability(dev, EV_KEY, gesture_key[i]);
		__set_bit(gesture_key[i], dev->keybit);
	}

	ts_data->gesture_id = 0xFF;
	ts_data->gesture_is_enable = ENABLE;

	ret = sysfs_create_group(&ts_data->dev->kobj, &hyn_gesture_group);
	if(ret){
		HYN_ERROR("ges_sys_node creat failed");
	}
	return ret;
}

void hyn_gesture_report(struct hyn_ts_data *ts_data)
{
	struct input_dev *dev = ts_data->input_dev;//ges_input_dev;//ts_data->input_dev;
	if(ts_data->gesture_id < sizeof(gesture_key)){
		input_report_key(dev, gesture_key[ts_data->gesture_id], 1);
		input_sync(dev);
		input_report_key(dev, gesture_key[ts_data->gesture_id], 0);
		input_sync(dev);
		HYN_INFO("fw gesture idx:%d key value:%d",ts_data->gesture_id,gesture_key[ts_data->gesture_id]);
	}
}

void hyn_gesture_exit(struct hyn_ts_data *ts_data)
{
	sysfs_remove_group(&ts_data->dev->kobj, &hyn_gesture_group);
}
