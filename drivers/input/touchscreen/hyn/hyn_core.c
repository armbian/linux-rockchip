/*
 * drivers/input/touchscreen/hyn_driver_release/hyn_core.c
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
#include "hyn_core.h"
#define HYN_DRIVER_NAME  "hyn_ts"

u8 hyn_log_level;
static struct hyn_ts_data *hyn_data = NULL;
static const struct hyn_ts_fuc* hyn_fun = NULL;
static const struct of_device_id hyn_of_match_table[] = {
	{.compatible = "hyn,66xx", .data = &hyn_cst66xx_fuc,},   /*support 36xx 35xx 66xx 68xx 148E*/
	{.compatible = "hyn,36xxes", .data = &hyn_cst36xxes_fuc,}, /*support 154es 3654es 3640es*/
	{.compatible = "hyn,3240", .data = &hyn_cst3240_fuc,},   /*support 3240 */
	{.compatible = "hyn,92xx", .data = &hyn_cst92xx_fuc,},   /*support 9217、9220 */
	{.compatible = "hyn,3xx",  .data = &hyn_cst3xx_fuc,},	/*support 340 348 328 128 140 148*/
	{.compatible = "hyn,7xx",  .data = &hyn_cst7xx_fuc,},	/*support 726 826 836u*/
	{.compatible = "hyn,8xxt", .data = &hyn_cst8xxT_fuc,},   /*support 816t 816d 820 08C*/
	{.compatible = "hyn,226se", .data = &hyn_cst226se_fuc,}, /*support 226se 8922*/
	{},
};
MODULE_DEVICE_TABLE(of, hyn_of_match_table);

static int hyn_check_ic(struct hyn_ts_data *ts_data)
{
	const struct of_device_id *of_dev;
	of_dev = of_match_device(hyn_of_match_table,ts_data->dev);
	if (!of_dev) {
		return -1;
	}
	hyn_fun = of_dev->data;
	if (IS_ERR_OR_NULL(hyn_fun) || IS_ERR_OR_NULL(hyn_fun->tp_chip_init)
		|| hyn_fun->tp_chip_init(ts_data)) {
		return -2;
	}
	ts_data->hyn_fuc_used = hyn_fun;
	return 0;
}

static int hyn_parse_dt(struct hyn_ts_data *ts_data)
{
	int ret = 0;
	struct device *dev = ts_data->dev;
	struct hyn_plat_data* dt = &ts_data->plat_data;
	HYN_ENTER();
	if (dev->of_node) {
		u32 buf[8];
		struct device_node *np = dev->of_node;
//vcc supply
		dt->vdd_i2c = NULL;
		dt->vdd_ana = NULL;
		if (of_property_read_bool(np,"vdd_ana-supply")) {
		   dt->vdd_ana = regulator_get(dev, "vdd_ana");
			if (IS_ERR_OR_NULL(dt->vdd_ana)) {
				dt->vdd_ana = NULL;
				HYN_ERROR("regulator_get vdd_ana failed");
			}
		}
		else{
			HYN_INFO("vdd_ana not config");
		}

//iovcc supply
		if (of_property_read_bool(np,"vcc_i2c-supply")) {
			dt->vdd_i2c = regulator_get(dev, "vcc_i2c");
			if (IS_ERR_OR_NULL(dt->vdd_i2c)) {
				dt->vdd_i2c = NULL;
				HYN_ERROR("regulator_get vdd_i2c failed");
			}
		}
		else{
			HYN_INFO("vdd_i2c not config");
		}

//gpio info
		dt->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
		if (IS_ERR(dt->reset_gpio)) {
			ret = PTR_ERR(dt->reset_gpio);
			HYN_ERROR("failed to request reset GPIO: %d\n", ret);
			return ret;
		}

		dt->irq_gpio = devm_gpiod_get(dev, "irq", GPIOD_ASIS);
		if (IS_ERR(dt->irq_gpio)) {
			ret = PTR_ERR(dt->irq_gpio);
			HYN_ERROR("failed to request irq GPIO: %d\n", ret);
			return ret;
		}
//pin_ctl
		ret =-1;
		dt->pinctl = devm_pinctrl_get(dev);
		if (!IS_ERR_OR_NULL(dt->pinctl)) {
			dt->pin_active = pinctrl_lookup_state(dt->pinctl, "ts_active");
			dt->pin_suspend= pinctrl_lookup_state(dt->pinctl, "ts_suspend");
			if (!IS_ERR_OR_NULL(dt->pin_active) && !IS_ERR_OR_NULL(dt->pin_suspend)) {
				ret = 0;
			}
		}
		if (ret) {
			HYN_INFO("pinctrl not config or can't find \"ts-active\" \"ts_suspend\"");
			dt->pinctl = NULL;
			dt->pin_active = NULL;
			dt->pin_suspend= NULL;
		}

// point info
		ret = of_property_read_u32(np, "max-touch-number", &dt->max_touch_num);
		ret |= of_property_read_u32(np, "pos-swap", &dt->swap_xy);
		ret |= of_property_read_u32(np, "posx-reverse", &dt->reverse_x);
		ret |= of_property_read_u32(np, "posy-reverse", &dt->reverse_y);
		ret |= of_property_read_u32_array(np, "display-coords", buf, 4);
		dt->x_resolution = buf[2];
		dt->y_resolution = buf[3];
		HYN_INFO("dts x_res = %d,y_res = %d,touch-number = %d swap_xy = %d reversex = %d reversey = %d",
					dt->x_resolution,dt->y_resolution,dt->max_touch_num,dt->swap_xy,dt->reverse_x,dt->reverse_y);
		if (ret < 0) {
			HYN_ERROR("dts get screen failed");
			return -EINVAL;
		}

// key info
		ret = of_property_read_u32(np, "key-number", &dt->key_num);
		if (ret>=0 && dt->key_num && dt->key_num<=8) {
			ret |= of_property_read_u32(np, "key-y-coord", &dt->key_y_coords);
			ret |= of_property_read_u32_array(np, "key-x-coords", dt->key_x_coords, dt->key_num);
			ret |= of_property_read_u32_array(np, "keys", dt->key_code, dt->key_num);
			if (ret < 0) {
				HYN_ERROR("dts get screen failed");
				return -EINVAL;
			}
		}
		else{
			HYN_INFO("key not config");
			dt->key_num = 0;
		}
		return 0;
	}
	else{
		HYN_ERROR("dts match failed");
		return -ENODEV;
	}
}

static int hyn_poweron(struct hyn_ts_data *ts_data)
{
	int ret = 0;
	struct hyn_plat_data* dt = &ts_data->plat_data;
	if (!IS_ERR_OR_NULL(hyn_data->plat_data.pinctl)) {
		if (pinctrl_select_state(dt->pinctl, dt->pin_active)) {
			HYN_ERROR("pin active set failed");
			ret = -EPROBE_DEFER;
			goto GPIO_SET_FAILE;
		}
	}

	if (!IS_ERR_OR_NULL(dt->vdd_ana)) {
		if (regulator_count_voltages(dt->vdd_ana) > 0) {
			if (regulator_set_voltage(dt->vdd_ana, 2800000, 3300000)) {
				HYN_ERROR("set voltage vdd_ana failed");
			}
		}
	}

	if (!IS_ERR_OR_NULL(dt->vdd_i2c)) {
		if (regulator_count_voltages(dt->vdd_i2c) > 0) {
			if (regulator_set_voltage(dt->vdd_i2c, 1800000, 1800000)) {
				HYN_ERROR("set voltage vdd_i2c failed");
			}
		}
	}
	if (hyn_power_source_ctrl(hyn_data, 1)) {
		HYN_ERROR("enable power failed");
	}
	mdelay(5);
	gpiod_direction_output(dt->reset_gpio, 1);
	return 0;
GPIO_SET_FAILE:
	return ret;
}

static int hyn_input_dev_init(struct hyn_ts_data *ts_data)
{
	int key_num = 0;//,ret =0;
	struct hyn_plat_data *dt = &ts_data->plat_data;
	struct input_dev *input_dev;

	HYN_ENTER();
	input_dev = input_allocate_device();
	if (!input_dev) {
		HYN_ERROR("Failed to allocate memory for input device");
		return -ENOMEM;
	}
	input_dev->name = HYN_DRIVER_NAME;
	input_dev->id.bustype = ts_data->bus_type;
	input_dev->dev.parent = ts_data->dev;
	input_set_drvdata(input_dev, ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	for (key_num = 0; key_num < dt->key_num; key_num++)
			input_set_capability(input_dev, EV_KEY, dt->key_code[key_num]);
#if HYN_MT_PROTOCOL_B_EN
	set_bit(BTN_TOOL_FINGER,input_dev->keybit);
	//input_mt_init_slots(input_dev, dt->max_touch_num+1);
	input_mt_init_slots(input_dev, dt->max_touch_num+1, INPUT_MT_DIRECT);
#else
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif

	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,  dt->max_touch_num, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, dt->x_resolution,0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, dt->y_resolution,0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);

	ts_data->input_dev = input_dev;

	return 0;
}

static void release_all_finger(struct hyn_ts_data *ts_data)
{
	HYN_ENTER();
	if (ts_data->report_id_flg) {
#if HYN_MT_PROTOCOL_B_EN
		u8 i;
		for(i=0; i< ts_data->plat_data.max_touch_num; i++) {
			if (ts_data->report_id_flg & (1UL<<i)) {
				// HYN_INFO("release %d",i);
				input_mt_slot(ts_data->input_dev, i);
				input_report_abs(ts_data->input_dev, ABS_MT_TRACKING_ID, -1);
				input_mt_report_slot_state(ts_data->input_dev, MT_TOOL_FINGER, false);
			}
		}
		input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
#else
		input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
		input_mt_sync(ts_data->input_dev);
#endif
	}
	ts_data->report_id_flg = 0;
}

static void touch_updata(u8 idx,u8 event)
{
	struct ts_frame *rep_frame = &hyn_data->rp_buf;
	struct input_dev *dev = hyn_data->input_dev;
	u16 zpress = rep_frame->pos_info[idx].pres_z;
	if (zpress < 10) {
		zpress = zpress+(rep_frame->pos_info[idx].pos_x&0x03) + (rep_frame->pos_info[idx].pos_y&0x03);
	}
	if (event) {
		hyn_data->report_id_flg |= (1UL<< rep_frame->pos_info[idx].pos_id);
	}
#if HYN_MT_PROTOCOL_B_EN
	if (event) {
		input_report_key(dev, BTN_TOUCH, 1);
		input_mt_slot(dev, rep_frame->pos_info[idx].pos_id);
		input_mt_report_slot_state(dev, MT_TOOL_FINGER, 1);
		input_report_abs(dev, ABS_MT_TRACKING_ID, rep_frame->pos_info[idx].pos_id);
		input_report_abs(dev, ABS_MT_POSITION_X, rep_frame->pos_info[idx].pos_x);
		input_report_abs(dev, ABS_MT_POSITION_Y, rep_frame->pos_info[idx].pos_y);
		input_report_abs(dev, ABS_MT_TOUCH_MAJOR, zpress>>3);
		input_report_abs(dev, ABS_MT_WIDTH_MAJOR, zpress>>3);
		input_report_abs(dev, ABS_MT_PRESSURE, zpress);
	}
	else{
		input_mt_slot(dev, rep_frame->pos_info[idx].pos_id);
		input_report_abs(dev, ABS_MT_TRACKING_ID, -1);
		input_mt_report_slot_state(dev, MT_TOOL_FINGER, 0);
	}
#else
	if (event) {
		input_report_key(dev, BTN_TOUCH, 1);
		input_report_abs(dev, ABS_MT_PRESSURE, zpress);
		input_report_abs(dev, ABS_MT_TRACKING_ID, rep_frame->pos_info[idx].pos_id);
		input_report_abs(dev, ABS_MT_TOUCH_MAJOR, zpress>>3);
		// input_report_abs(dev, ABS_MT_WIDTH_MAJOR, zpress>>3);
		input_report_abs(dev, ABS_MT_POSITION_X, rep_frame->pos_info[idx].pos_x);
		input_report_abs(dev, ABS_MT_POSITION_Y, rep_frame->pos_info[idx].pos_y);
		input_mt_sync(dev);
	}
#endif
}

static void hyn_irq_report_work(struct work_struct *work)
{
	struct hyn_ts_data *ts_data = hyn_data;
	struct ts_frame *rep_frame = &hyn_data->rp_buf;
	struct input_dev *dev = hyn_data->input_dev;
	struct hyn_plat_data *dt = &hyn_data->plat_data;
	u16 xpos,ypos;
	int reprot_state_clr = 0;
	if (hyn_fun->tp_report()) {
		HYN_INFO("Ignore illegal data");
		return;
	}
	mutex_lock(&ts_data->mutex_report);
	if (rep_frame->report_need & REPORT_KEY) { //key
#if KEY_USED_POS_REPORT
		rep_frame->pos_info[0].pos_id = 0;
		rep_frame->pos_info[0].pos_x = dt->key_x_coords[rep_frame->key_id];
		rep_frame->pos_info[0].pos_y = dt->key_y_coords;
		rep_frame->pos_info[0].pres_z = 100;
		touch_updata(0,rep_frame->key_state ? 1:0);
#else
		input_report_key(dev,dt->key_code[rep_frame->key_id],rep_frame->key_state ? 1:0);
#endif
		if (rep_frame->key_state==0) {
			reprot_state_clr = 1;
		}
		input_sync(dev);
		HYN_INFO2("report keyid:%d keycode:%d",rep_frame->key_id,dt->key_code[rep_frame->key_id]);
	}

	if (rep_frame->report_need & REPORT_POS) { //pos
		u8 i;
		if (rep_frame->rep_num == 0) {
			release_all_finger(ts_data);
			reprot_state_clr = 1;
		}
		else{
			u8 touch_down = 0;
			ts_data->report_id_flg = 0;
			for(i = 0; i < rep_frame->rep_num; i++) {
				HYN_INFO2("id,%d,xy,%d,%d",rep_frame->pos_info[i].pos_id,rep_frame->pos_info[i].pos_x,rep_frame->pos_info[i].pos_y);
				if (dt->swap_xy) {
					xpos = rep_frame->pos_info[i].pos_y;
					ypos = rep_frame->pos_info[i].pos_x;
				}
				else{
					xpos = rep_frame->pos_info[i].pos_x;
					ypos = rep_frame->pos_info[i].pos_y;
				}
				if (ypos > dt->y_resolution || xpos > dt->x_resolution) {
					HYN_ERROR("Please check dts or FW config,maybe resolution or origin issue!!!");
				}
				if (rep_frame->pos_info[i].pos_id >= ts_data->plat_data.max_touch_num) {
					continue;
				}
				if (dt->reverse_x) {
					xpos = dt->x_resolution-xpos;
				}
				if (dt->reverse_y) {
					ypos = dt->y_resolution-ypos;
				}
				rep_frame->pos_info[i].pos_x = xpos;
				rep_frame->pos_info[i].pos_y = ypos;
				touch_updata(i,rep_frame->pos_info[i].event? 1:0);
				if (rep_frame->pos_info[i].event) touch_down++;
			}
			if (touch_down==0) {
				reprot_state_clr = 1;
				input_report_key(dev, BTN_TOUCH, 0);
#if HYN_MT_PROTOCOL_B_EN==0
				input_mt_sync(dev);
#endif
			}
		}
		input_sync(dev);
	}
	if (rep_frame->report_need & REPORT_PROX) {
		hyn_proximity_report(ts_data->prox_state);
		reprot_state_clr = 1;
	}
#if (HYN_GESTURE_EN)
	if (rep_frame->report_need & REPORT_GES) {
		hyn_gesture_report(ts_data);
		reprot_state_clr = 1;
	}
#endif
	if (reprot_state_clr) {
		rep_frame->report_need = REPORT_NONE;
	}
	mutex_unlock(&ts_data->mutex_report);
}

static void hyn_esdcheck_work(struct work_struct *work)
{
#if ESD_CHECK_EN
	int ret;
	HYN_ENTER();
	if (hyn_data->esd_block_cnt==0) {
		ret = hyn_fun->tp_check_esd();
		HYN_INFO("esd:%04x",ret);
		if (hyn_data->esd_last_value != ret) {
			hyn_data->esd_fail_cnt = 0;
			hyn_data->esd_last_value = ret;
		}
		else{
			hyn_data->esd_fail_cnt++;
			if (hyn_data->esd_fail_cnt > 2) {
				hyn_data->esd_fail_cnt = 0;
				hyn_power_source_ctrl(hyn_data,0);
				mdelay(1);
				hyn_power_source_ctrl(hyn_data,1);
				hyn_fun->tp_rest();
				if (hyn_data->prox_is_enable) {
					hyn_fun->tp_prox_handle(1);
				}
				else if (hyn_data->gesture_is_enable) {
					hyn_fun->tp_set_workmode(GESTURE_MODE,1);
				}
				if (hyn_data->charge_is_enable) {
					hyn_fun->tp_set_workmode(CHARGE_ENTER,1);
				}
				if (hyn_data->glove_is_enable) {
					hyn_fun->tp_set_workmode(GLOVE_ENTER,1);
				}
			}
		}
	}
	else{
		hyn_data->esd_block_cnt--;
	}
	queue_delayed_work(hyn_data->hyn_workqueue, &hyn_data->esdcheck_work,
						   msecs_to_jiffies(1000));
#endif
}

static void hyn_resume(struct device *dev)
{
	int ret = 0;
	struct ts_frame *rep_frame;
	struct hyn_plat_data *dt;
	HYN_ENTER();
	if (IS_ERR_OR_NULL(hyn_data)) {
		return;
	}
	rep_frame = &hyn_data->rp_buf;
	dt = &hyn_data->plat_data;
#if (HYN_WAKE_LOCK_EN==1)
	wake_unlock(&hyn_data->tp_wakelock);
#endif
	if (!IS_ERR_OR_NULL(dt->pinctl)) {
	  pinctrl_select_state(dt->pinctl, dt->pin_active);
	}
	hyn_power_source_ctrl(hyn_data, 1);
	hyn_fun->tp_resum();
	//Condition recovery
	if (hyn_data->prox_is_enable) {
		hyn_fun->tp_prox_handle(1);
	} else if (hyn_data->gesture_is_enable) {
		hyn_irq_set(hyn_data,DISABLE);
		ret = disable_irq_wake(hyn_data->gpio_irq);
		ret |= irq_set_irq_type(hyn_data->gpio_irq,dt->irq_flags);
		if (ret < 0) {
			HYN_ERROR("gesture disable_irq_wake failed");
		}
	}
	if (hyn_data->charge_is_enable) {
		hyn_fun->tp_set_workmode(CHARGE_ENTER,1);
	}
	if (hyn_data->glove_is_enable) {
		hyn_fun->tp_set_workmode(GLOVE_ENTER,1);
	}
	//compensate for lifting
	if (rep_frame->report_need & REPORT_KEY) {
		input_report_key(hyn_data->input_dev,dt->key_code[rep_frame->key_id],0);
	}
	if (rep_frame->report_need & REPORT_POS) {
		release_all_finger(hyn_data);
		input_sync(hyn_data->input_dev);
	}
	rep_frame->report_need = REPORT_NONE;
	hyn_irq_set(hyn_data,ENABLE);
}

static __maybe_unused void hyn_suspend(struct device *dev)
{
	int ret = 0;
	HYN_ENTER();
	if (IS_ERR_OR_NULL(hyn_data)) {
		return;
	}
#if (HYN_WAKE_LOCK_EN==1)
	wake_lock(&hyn_data->tp_wakelock);
#endif
	if (hyn_data->prox_is_enable ==1) {
	} else if (hyn_data->gesture_is_enable) {
		hyn_irq_set(hyn_data,DISABLE);
		ret = enable_irq_wake(hyn_data->gpio_irq);
		ret |= irq_set_irq_type(hyn_data->gpio_irq,IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND|IRQF_ONESHOT);
		if (ret < 0) {
			HYN_ERROR("gesture irq_set_irq failed");
		}
		hyn_fun->tp_set_workmode(GESTURE_MODE,1);
		hyn_irq_set(hyn_data,ENABLE);
		hyn_power_source_ctrl(hyn_data, 1);
	} else {
		hyn_irq_set(hyn_data,DISABLE);
		hyn_fun->tp_supend();
		if (!IS_ERR_OR_NULL(hyn_data->plat_data.pinctl)) {
			pinctrl_select_state(hyn_data->plat_data.pinctl, hyn_data->plat_data.pin_suspend);
		}
		hyn_power_source_ctrl(hyn_data, 0);
	}
}

static void hyn_updata_fw_work(struct work_struct *work)
{
	if (!IS_ERR_OR_NULL(hyn_data->fw_updata_addr)) {
		hyn_fun->tp_updata_fw(hyn_data->fw_updata_addr,hyn_data->fw_updata_len);
	}
	else{
		HYN_ERROR("fw_updata_addr is erro");
	}
}

static void hyn_resume_work(struct work_struct *work)
{
	if (IS_ERR_OR_NULL(hyn_data)) {
		return;
	}
	hyn_resume(hyn_data->dev);
}

static irqreturn_t hyn_irq_handler(int irq, void *data)
{
	atomic_set(&hyn_data->hyn_irq_flg,1);
	if (hyn_data->work_mode < DIFF_MODE) {
		// queue_work(hyn_data->hyn_workqueue,&hyn_data->work_report);
		hyn_data->esd_block_cnt = 2;
		flush_workqueue(hyn_data->hyn_workqueue);
		hyn_irq_report_work(NULL);
	}
	else{
		wake_up(&hyn_data->wait_irq);
	}
	return IRQ_HANDLED;
}

#if defined(CONFIG_FB)// || defined(CONFIG_DRM)
#ifndef FB_EARLY_EVENT_BLANK
#define FB_EARLY_EVENT_BLANK  FB_EVENT_BLANK
#endif
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	if (IS_ERR_OR_NULL(data)) {
		HYN_INFO("data is invlid don't handle");
		return 0;
	}
	{
#if defined(CONFIG_FB)
		int blank_value = *((int *)(((struct fb_event *)data)->data));
		const unsigned long event_enum[2] = {FB_EARLY_EVENT_BLANK, FB_EVENT_BLANK};
		const int blank_enum[2] = {FB_BLANK_POWERDOWN, FB_BLANK_UNBLANK};
// #elif defined(CONFIG_DRM)
//	 #if defined(CONFIG_DRM_PANEL)
//		 int blank_value = *((int *)(((struct drm_panel_notifier *)data)->data));
//		 const unsigned long event_enum[2] = {DRM_PANEL_EARLY_EVENT_BLANK, DRM_PANEL_EVENT_BLANK};
//		 const int blank_enum[2] = {DRM_PANEL_BLANK_POWERDOWN, DRM_PANEL_BLANK_UNBLANK};
//	 #else //CONFIG_DRM_PANEL
//		 int blank_value = *((int *)(((struct msm_drm_notifier *)data)->data));
//		 const unsigned long event_enum[2] = {MSM_DRM_EARLY_EVENT_BLANK, MSM_DRM_EVENT_BLANK};
//		 const int blank_enum[2] = {MSM_DRM_BLANK_POWERDOWN, MSM_DRM_BLANK_UNBLANK};
//	 #endif //CONFIG_DRM_PANEL
#endif //CONFIG_DRM

	HYN_INFO("notifier,event:%lu,blank:%d", event, blank_value);
	if (hyn_data->old_fb_state == blank_value || (event != event_enum[0] && event != event_enum[1])) {
		HYN_INFO("don't care");
		return 0;
	}

	if (blank_enum[1] == blank_value) {
		queue_work(hyn_data->hyn_workqueue, &hyn_data->work_resume);
	} else{
		cancel_work_sync(&hyn_data->work_resume);
		hyn_suspend(hyn_data->dev);
	}

	hyn_data->old_fb_state = blank_value;
	return 0;
	}
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void hyn_ts_early_suspend(struct early_suspend *handler)
{
	hyn_suspend(hyn_data->dev);
}
static void hyn_ts_late_resume(struct early_suspend *handler)
{
	hyn_resume(hyn_data->dev);
}
#endif

#ifdef I2C_PORT
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void hyn_ts_remove(struct i2c_client *client);
#else
static int hyn_ts_remove(struct i2c_client *client);
#endif
static int hyn_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
#else

#ifdef CONFIG_BUS_SPI
static struct mt_chip_conf hyn_mt_spi_conf = {
	.setuptime = 100,
	.holdtime = 100,
	.high_time = 25,
	.low_time = 25,
	.cs_idletime = 2,
	.ulthgh_thrsh = 0,
	.cpol = 0,
	.cpha = 0,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.tx_endian = 0,
	.rx_endian = 0,
	.com_mod = DMA_TRANSFER,/*FIFO_TRANSFER,*/
	.pause = 1, /*0 : twice cs   1: one cs*/
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void hyn_ts_remove(struct spi_device *client);
#else
static int hyn_ts_remove(struct spi_device *client);
#endif
static int hyn_ts_probe(struct spi_device *client)
#endif
{
	int ret = 0;
	u16 bus_type;
	struct hyn_ts_data *ts_data = 0;

	HYN_ENTER();
	HYN_INFO(HYN_DRIVER_VERSION);
#ifdef I2C_PORT
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		HYN_ERROR("I2C not supported");
		return -ENODEV;
	}
	bus_type = BUS_I2C;
#else
	client->mode = SPI_MODE;
	client->max_speed_hz = SPI_CLOCK_FREQ;
	client->bits_per_word = 8;

#ifdef CONFIG_BUS_SPI
	client->controller_data = (void *)&hyn_mt_spi_conf;
#endif

	if (spi_setup(client) < 0) {
		HYN_ERROR("SPI not supported");
		return -ENODEV;
	}
	bus_type = BUS_SPI;
#endif
	if (!IS_ERR_OR_NULL(hyn_data)) {
		HYN_ERROR("other dev is insmode");
		return -ENOMEM;
	}
	ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		HYN_ERROR("alloc ts data failed");
		return -ENOMEM;
	}
	ts_data->bus_type = bus_type;
	ts_data->rp_buf.key_id = 0xFF;
	ts_data->work_mode = NOMAL_MODE;
	hyn_log_level = 0;

	hyn_data = ts_data;
	ts_data->client = client;
	ts_data->dev = &client->dev;
	dev_set_drvdata(ts_data->dev, ts_data);

	#if (I2C_USE_DMA==2)
		ts_data->dev->coherent_dma_mask = DMA_BIT_MASK(32);
		ts_data->dma_buff_va = (u8 *)dma_alloc_coherent(ts_data->dev,
						2048, &ts_data->dma_buff_pa, GFP_KERNEL);
		if (!ts_data->dma_buff_va) {
			HYN_ERROR("Allocate I2C DMA Buffer fail");
		}
	#endif
	ret = hyn_parse_dt(ts_data);
	if (ret) {
		HYN_ERROR("hyn_parse_dt failed");
		goto FREE_RESOURCE;
	}

	ret = hyn_poweron(ts_data);
	if (ret) {
		HYN_ERROR("hyn_poweron failed");
		goto FREE_RESOURCE;
	}
	// spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->mutex_report);
	mutex_init(&ts_data->mutex_bus);
	mutex_init(&ts_data->mutex_fs);
	init_waitqueue_head(&ts_data->wait_irq);

	ret = hyn_check_ic(ts_data);
	if (ret) {
		HYN_ERROR("hyn_check_ic failed");
		goto FREE_RESOURCE;
	}

	INIT_WORK(&ts_data->work_report,hyn_irq_report_work);
	INIT_WORK(&ts_data->work_updata_fw,hyn_updata_fw_work);
	INIT_WORK(&ts_data->work_resume,hyn_resume_work);
	INIT_DELAYED_WORK(&ts_data->esdcheck_work,hyn_esdcheck_work);

	ts_data->hyn_workqueue = create_singlethread_workqueue("hyn_wq");
	if (IS_ERR_OR_NULL(ts_data->hyn_workqueue)) {
		HYN_ERROR("create work queue failed");
		ret = -ENOMEM;
		goto FREE_RESOURCE;
	}

	ret = hyn_input_dev_init(ts_data);
	if (ret) {
		if (!IS_ERR_OR_NULL(ts_data->input_dev)) {
			input_set_drvdata(ts_data->input_dev, NULL);
			input_free_device(ts_data->input_dev);
			ts_data->input_dev = NULL;
		}
		HYN_ERROR("hyn_input_dev_init failed");
		goto FREE_RESOURCE;
	}
	ret = input_register_device(ts_data->input_dev);
	if (ret) {
		HYN_ERROR("input_register_device failed");
		goto FREE_RESOURCE;
	}

#if (HYN_WAKE_LOCK_EN==1)
	wake_lock_init(&ts_data->tp_wakelock, WAKE_LOCK_SUSPEND, "suspend_tp_lock");
#endif

#if (HYN_GESTURE_EN)
	ret = hyn_gesture_init(ts_data);
	if (ret) {
		HYN_ERROR("gesture_init failed");
		goto FREE_RESOURCE;
	}
#endif

	ts_data->gpio_irq = gpiod_to_irq(ts_data->plat_data.irq_gpio);
	if (ts_data->gpio_irq < 0) {
		HYN_ERROR("failed to get gpio irq\n");
		ret = ts_data->gpio_irq;
		goto FREE_RESOURCE;
	}
	ts_data->plat_data.irq_flags = irqd_get_trigger_type(irq_get_irq_data(ts_data->gpio_irq));
	ret = devm_request_threaded_irq(ts_data->dev, ts_data->gpio_irq, NULL, hyn_irq_handler,
					(IRQF_TRIGGER_FALLING | IRQF_ONESHOT), HYN_DRIVER_NAME,
					ts_data);
	if (ret) {
		HYN_ERROR("devm_request_threaded_irq failed");
		goto FREE_RESOURCE;
	}
	atomic_set(&ts_data->irq_is_disable,ENABLE);
	hyn_irq_set(ts_data , DISABLE);

#if defined(CONFIG_FB)
	HYN_INFO("fb_notif_register");
	ts_data->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts_data->fb_notif);
	if (ret) {
		HYN_ERROR("register fb_notifier failed: %d", ret);
	}
// #elif defined(CONFIG_DRM)
//	 ts_data->fb_notif.notifier_call = fb_notifier_callback;
// #if defined(CONFIG_DRM_PANEL)
//	 HYN_INFO("drm_panel register");
//	 {
//		 int i,count;
//		 struct device_node *node;
//		 struct drm_panel *panel;
//		 struct device_node *np = ts_data->dev->of_node;
//		 count = of_count_phandle_with_args(np, "panel", NULL);
//		 ts_data->active_panel = NULL;
//		 if (count > 0) {
//			 for (i = 0; i < count; i++) {
//				 node = of_parse_phandle(np, "panel", i);
//				 panel = of_drm_find_panel(node);
//				 of_node_put(node);
//				 if (!IS_ERR(panel)) {
//					 ts_data->active_panel = panel;
//					 break;
//				 }
//			 }
//		 }
//		 if (IS_ERR_OR_NULL(ts_data->active_panel)) {
//			 HYN_ERROR("node can't not find DRM panel ");
//		 }
//	 }
//	 if (!IS_ERR_OR_NULL(ts_data->active_panel)) {
//		 ret = drm_panel_notifier_register(ts_data->active_panel,&ts_data->fb_notif);
//		 if (ret < 0) {
//			  HYN_ERROR("drm_panel_notifier_register failed: %d", ret);
//		 }
//	 }
// #else
//	 HYN_INFO("msm_drm_register");
//	 ret = msm_drm_register_client(&ts_data->fb_notif);
//	 if (ret < 0) {
//		 HYN_ERROR("msm_drm_register_client failed: %d", ret);
//	 }
// #endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts_data->early_suspend.suspend = hyn_ts_early_suspend;
	ts_data->early_suspend.resume = hyn_ts_late_resume;
	register_early_suspend(&ts_data->early_suspend);
#endif
	hyn_create_sysfs(ts_data);
#if (HYN_APK_DEBUG_EN)
	hyn_tool_fs_int(ts_data);
#endif
	hyn_proximity_int(ts_data);
	hyn_irq_set(ts_data,ENABLE);
	hyn_esdcheck_switch(ts_data,ENABLE);

#if HYN_POWER_ON_UPDATA
	if (ts_data->need_updata_fw) {
		queue_work(ts_data->hyn_workqueue,&ts_data->work_updata_fw);
	}
#endif
	return 0;
FREE_RESOURCE:
	hyn_ts_remove(client);
	return ret;
}


#ifdef I2C_PORT
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void hyn_ts_remove(struct i2c_client *client)
#else
static int hyn_ts_remove(struct i2c_client *client)
#endif
#else
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static void hyn_ts_remove(struct spi_device *client)
#else
static int hyn_ts_remove(struct spi_device *client)
#endif
#endif
{
	struct hyn_ts_data *ts_data = hyn_data;
	HYN_ENTER();
	if (!IS_ERR_OR_NULL(ts_data)) {
		if (ts_data->hyn_workqueue) {
			flush_workqueue(ts_data->hyn_workqueue);
			hyn_esdcheck_switch(ts_data,DISABLE);
			destroy_workqueue(ts_data->hyn_workqueue);
		}
		HYN_INFO2("ts_remove1");
#if (HYN_APK_DEBUG_EN)
		hyn_tool_fs_exit();
#endif
		hyn_proximity_exit();
#if (HYN_GESTURE_EN)
		hyn_gesture_exit(ts_data);
#endif
		hyn_release_sysfs(ts_data);
		HYN_INFO2("ts_remove2");
		if (!IS_ERR_OR_NULL(ts_data->input_dev)) {
			input_unregister_device(ts_data->input_dev);
		}
		HYN_INFO2("ts_remove3");
#if 0
#if defined(CONFIG_FB)
		fb_unregister_client(&ts_data->fb_notif);
// #elif defined(CONFIG_DRM)
// #if defined(CONFIG_DRM_PANEL)
//	 if (!IS_ERR_OR_NULL(ts_data->active_panel))
//			 drm_panel_notifier_unregister(ts_data->active_panel, &ts_data->fb_notif);
// #else
//	 msm_drm_unregister_client(&ts_data->fb_notif);
// #endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&ts_data->early_suspend);
#endif
#endif
		hyn_power_source_ctrl(hyn_data, 0);
		if (!IS_ERR_OR_NULL(ts_data->plat_data.vdd_ana)) {
			regulator_put(ts_data->plat_data.vdd_ana);
		}
		if (!IS_ERR_OR_NULL(ts_data->plat_data.vdd_i2c)) {
			regulator_put(ts_data->plat_data.vdd_i2c);
		}
		HYN_INFO2("ts_remove4");
#if (I2C_USE_DMA==2)
		if (!IS_ERR_OR_NULL(ts_data->dma_buff_va)) {
			dma_free_coherent(NULL, 2048, ts_data->dma_buff_va, ts_data->dma_buff_pa);
		}
#endif
		kfree(ts_data);
		hyn_data = NULL;
		HYN_INFO2("ts_remove5");
	}
#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

#ifdef I2C_PORT
static const struct i2c_device_id hyn_id_table[] = {
	{.name = HYN_DRIVER_NAME, .driver_data = 0,},
	{},
};

static struct i2c_driver hyn_ts_driver = {
	.probe = hyn_ts_probe,
	.remove = hyn_ts_remove,
	.driver = {
		.name = HYN_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = hyn_of_match_table,
	},
	.id_table = hyn_id_table,
};
#else
static struct spi_driver hyn_ts_driver = {
	.driver = {
		   .name = HYN_DRIVER_NAME,
		   .of_match_table = hyn_of_match_table,
		   .owner = THIS_MODULE,
		   },
	.probe = hyn_ts_probe,
	.remove = hyn_ts_remove,
};
#endif

static int __init hyn_ts_init(void)
{
	int ret = 0;
	HYN_ENTER();
#ifdef I2C_PORT
	ret = i2c_add_driver(&hyn_ts_driver);
#else
	ret = spi_register_driver(&hyn_ts_driver);
#endif
	if (ret) {
		HYN_ERROR("add i2c driver failed");
		return -ENODEV;
	}
	return 0;
}

static void __exit hyn_ts_exit(void)
{
	HYN_ENTER();
#ifdef I2C_PORT
	i2c_del_driver(&hyn_ts_driver);
#else
	spi_unregister_driver(&hyn_ts_driver);
#endif
}

late_initcall(hyn_ts_init);
// module_init(hyn_ts_init);
module_exit(hyn_ts_exit);

MODULE_AUTHOR("Hynitron Driver Team");
MODULE_DESCRIPTION("Hynitron Touchscreen Driver");
MODULE_LICENSE("GPL v2");
