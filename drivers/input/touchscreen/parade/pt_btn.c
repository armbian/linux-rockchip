/*
 * pt_btn.c
 * Parade TrueTouch(TM) Standard Product CapSense Reports Module.
 * For use with Parade touchscreen controllers.
 * Supported parts include:
 * TMA5XX
 * TMA448
 * TMA445A
 * TT21XXX
 * TT31XXX
 * TT4XXXX
 * TT7XXX
 * TC3XXX
 *
 * Copyright (C) 2015-2020 Parade Technologies
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Parade Technologies at www.paradetech.com <ttdrivers@paradetech.com>
 */

#include "pt_regs.h"

/*******************************************************************************
 * FUNCTION: pt_btn_key_action
 *
 * SUMMARY: Reports key event
 *
 * PARAMETERS:
 *     *bd        - pointer to button data structure
 *      btn_no    - number of button
 *      btn_state - state of button
 ******************************************************************************/
static inline void pt_btn_key_action(struct pt_btn_data *bd,
	int btn_no, int btn_state)
{
	struct device *dev = bd->dev;
	struct pt_sysinfo *si = bd->si;

	if (!si->btn[btn_no].enabled ||
			si->btn[btn_no].state == btn_state)
		return;

	si->btn[btn_no].state = btn_state;
	input_report_key(bd->input, si->btn[btn_no].key_code, btn_state);
	input_sync(bd->input);

	pt_debug(dev, DL_INFO, "%s: btn=%d key_code=%d %s\n",
		__func__, btn_no, si->btn[btn_no].key_code,
		btn_state == PT_BTN_PRESSED ?
			"PRESSED" : "RELEASED");
}

/*******************************************************************************
 * FUNCTION: pt_get_btn_touches
 *
 * SUMMARY: Parse and report key event
 *
 * PARAMETERS:
 *     *bd  - pointer to button data structure
 ******************************************************************************/
static void pt_get_btn_touches(struct pt_btn_data *bd)
{
	struct pt_sysinfo *si = bd->si;
	int num_btns = si->num_btns;
	int cur_btn;
	int cur_btn_state;

	for (cur_btn = 0; cur_btn < num_btns; cur_btn++) {
		/* Get current button state */
		cur_btn_state = (si->xy_data[0] >> (cur_btn * PT_BITS_PER_BTN))
				& PT_NUM_BTN_EVENT_ID;

		pt_btn_key_action(bd, cur_btn, cur_btn_state);
	}
}

/*******************************************************************************
 * FUNCTION: pt_btn_lift_all
 *
 * SUMMARY: Reports button liftoff action
 *
 * PARAMETERS:
 *     *bd  - pointer to button data structure
 ******************************************************************************/
static void pt_btn_lift_all(struct pt_btn_data *bd)
{
	struct pt_sysinfo *si = bd->si;
	int i;

	if (!si || si->num_btns == 0)
		return;

	for (i = 0; i < si->num_btns; i++)
		pt_btn_key_action(bd, i, PT_BTN_RELEASED);
}

/*******************************************************************************
 * FUNCTION: pt_xy_worker
 *
 * SUMMARY: Read xy_data for all current CapSense button touches
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *     *bd  - pointer to button data structure
 ******************************************************************************/
static int pt_xy_worker(struct pt_btn_data *bd)
{
	struct pt_sysinfo *si = bd->si;

	/* extract button press/release touch information */
	if (si->num_btns > 0)
		pt_get_btn_touches(bd);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_btn_attention
 *
 * SUMMARY: Wrapper function for pt_xy_worker() that register to TTDL attention
 *  list.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_btn_attention(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;
	int rc;

	if (bd->si->xy_mode[2] != bd->si->desc.btn_report_id)
		return 0;

	/* core handles handshake */
	mutex_lock(&bd->btn_lock);
	rc = pt_xy_worker(bd);
	mutex_unlock(&bd->btn_lock);
	if (rc < 0)
		pt_debug(dev, DL_ERROR,
			"%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_startup_attention
 *
 * SUMMARY: Wrapper function for pt_btn_lift_all() that register to TTDL
 *  attention list.
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_startup_attention(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;

	mutex_lock(&bd->btn_lock);
	pt_btn_lift_all(bd);
	mutex_unlock(&bd->btn_lock);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_btn_suspend_attention
 *
 * SUMMARY: Function for button to enter suspend state that as following steps:
 *          1) Lift all button
 *          2) Set flag with suspend state
 *          3) Decrese pm system count
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_btn_suspend_attention(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;

	mutex_lock(&bd->btn_lock);
	pt_btn_lift_all(bd);
	bd->is_suspended = true;
	mutex_unlock(&bd->btn_lock);

	pm_runtime_put(dev);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_btn_resume_attention
 *
 * SUMMARY: Function for button to leave suspend state that as following steps:
 *          1) Increse pm system count
 *          2) Clear suspend state flag
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_btn_resume_attention(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;

	pm_runtime_get(dev);

	mutex_lock(&bd->btn_lock);
	bd->is_suspended = false;
	mutex_unlock(&bd->btn_lock);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_btn_open
 *
 * SUMMARY: Open method for input device(button) that sets up call back
 *  functions to TTDL attention list
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *     *input - pointer to input_dev structure
 ******************************************************************************/
static int pt_btn_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;

	pm_runtime_get_sync(dev);

	mutex_lock(&bd->btn_lock);
	bd->is_suspended = false;
	mutex_unlock(&bd->btn_lock);

	pt_debug(dev, DL_INFO, "%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	_pt_subscribe_attention(dev, PT_ATTEN_IRQ, PT_BTN_NAME,
		pt_btn_attention, PT_MODE_OPERATIONAL);

	/* set up startup call back */
	_pt_subscribe_attention(dev, PT_ATTEN_STARTUP, PT_BTN_NAME,
		pt_startup_attention, 0);

	/* set up suspend call back */
	_pt_subscribe_attention(dev, PT_ATTEN_SUSPEND, PT_BTN_NAME,
		pt_btn_suspend_attention, 0);

	/* set up resume call back */
	_pt_subscribe_attention(dev, PT_ATTEN_RESUME, PT_BTN_NAME,
		pt_btn_resume_attention, 0);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_btn_close
 *
 * SUMMARY: Close method for input device(button) that clears call back
 *  functions from TTDL attention list.
 *
 * PARAMETERS:
 *     *input - pointer to input_dev structure
 ******************************************************************************/
static void pt_btn_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;

	_pt_unsubscribe_attention(dev, PT_ATTEN_IRQ, PT_BTN_NAME,
		pt_btn_attention, PT_MODE_OPERATIONAL);

	_pt_unsubscribe_attention(dev, PT_ATTEN_STARTUP, PT_BTN_NAME,
		pt_startup_attention, 0);

	_pt_unsubscribe_attention(dev, PT_ATTEN_SUSPEND, PT_BTN_NAME,
		pt_btn_suspend_attention, 0);

	_pt_unsubscribe_attention(dev, PT_ATTEN_RESUME, PT_BTN_NAME,
		pt_btn_resume_attention, 0);

	mutex_lock(&bd->btn_lock);
	if (!bd->is_suspended) {
		pm_runtime_put(dev);
		bd->is_suspended = true;
	}
	mutex_unlock(&bd->btn_lock);
}

/*******************************************************************************
 * FUNCTION: pt_setup_input_device
 *
 * SUMMARY: Set up resolution, event signal capabilities and register input
 *  device for button.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_setup_input_device(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;
	int i;
	int rc;

	pt_debug(dev, DL_INFO, "%s: Initialize event signals\n",
		__func__);
	__set_bit(EV_KEY, bd->input->evbit);
	pt_debug(dev, DL_INFO, "%s: Number of buttons %d\n",
		__func__, bd->si->num_btns);
	for (i = 0; i < bd->si->num_btns; i++) {
		pt_debug(dev, DL_INFO, "%s: btn:%d keycode:%d\n",
			__func__, i, bd->si->btn[i].key_code);
		__set_bit(bd->si->btn[i].key_code, bd->input->keybit);
	}

	rc = input_register_device(bd->input);
	if (rc < 0)
		pt_debug(dev, DL_ERROR,
			"%s: Error, failed register input device r=%d\n",
			__func__, rc);
	else
		bd->input_device_registered = true;

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_setup_input_attention
 *
 * SUMMARY: Wrapper function for pt_setup_input_device() register to TTDL
 *  attention list.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_setup_input_attention(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;
	int rc;

	bd->si = _pt_request_sysinfo(dev);
	if (!bd->si)
		return -1;

	rc = pt_setup_input_device(dev);

	_pt_unsubscribe_attention(dev, PT_ATTEN_STARTUP, PT_BTN_NAME,
		pt_setup_input_attention, 0);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_btn_probe
 *
 * SUMMARY: The probe function for button input device
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev   - pointer to device structure
 ******************************************************************************/
int pt_btn_probe(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_btn_data *bd = &cd->bd;
	struct pt_platform_data *pdata = dev_get_platdata(dev);
	struct pt_btn_platform_data *btn_pdata;
	int rc = 0;

	if (!pdata || !pdata->btn_pdata) {
		pt_debug(dev, DL_ERROR,
			"%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}
	btn_pdata = pdata->btn_pdata;

	mutex_init(&bd->btn_lock);
	bd->dev = dev;
	bd->pdata = btn_pdata;

	/* Create the input device and register it. */
	pt_debug(dev, DL_INFO,
		"%s: Create the input device and register it\n", __func__);
	bd->input = input_allocate_device();
	if (!bd->input) {
		pt_debug(dev, DL_ERROR,
			"%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENODEV;
		goto error_alloc_failed;
	} else
		bd->input_device_allocated = true;

	if (bd->pdata->inp_dev_name)
		bd->input->name = bd->pdata->inp_dev_name;
	else
		bd->input->name = PT_BTN_NAME;
	scnprintf(bd->phys, sizeof(bd->phys), "%s/input%d", dev_name(dev),
			cd->phys_num++);
	bd->input->phys = bd->phys;
	bd->input->dev.parent = bd->dev;
	bd->input->open = pt_btn_open;
	bd->input->close = pt_btn_close;
	input_set_drvdata(bd->input, bd);

	/* get sysinfo */
	bd->si = _pt_request_sysinfo(dev);

	if (bd->si) {
		rc = pt_setup_input_device(dev);
		if (rc)
			goto error_init_input;
	} else {
		pt_debug(dev, DL_ERROR,
			"%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, bd->si);
		_pt_subscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_BTN_NAME, pt_setup_input_attention, 0);
	}

	return 0;

error_init_input:
	input_free_device(bd->input);
	bd->input_device_allocated = false;
error_alloc_failed:
error_no_pdata:
	pt_debug(dev, DL_ERROR, "%s failed.\n", __func__);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_btn_release
 *
 * SUMMARY: The release function for button input device
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *	*dev   - pointer to device structure
 ******************************************************************************/
int pt_btn_release(struct device *dev)
{
	struct pt_core_data *cd;
	struct pt_btn_data *bd;

	/* Ensure valid pointers before de-referencing them */
	if (dev) {
		cd = dev_get_drvdata(dev);
		if (cd)
			bd = &cd->bd;
		else
			return 0;
	} else {
		return 0;
	}

	/*
	 * Second call this function may cause kernel panic if probe fail.
	 * Use input_device_registered & input_device_allocated variable to
	 * avoid unregister or free unavailable devive.
	 */
	if (bd && bd->input_device_registered) {
		bd->input_device_registered = false;
		input_unregister_device(bd->input);
		/* Unregistering device will free the device too */
		bd->input_device_allocated = false;
	} else if (bd && bd->input_device_allocated) {
		bd->input_device_allocated = false;
		input_free_device(bd->input);
		_pt_unsubscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_BTN_NAME, pt_setup_input_attention, 0);
	}

	return 0;
}
