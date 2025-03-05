/*
 * pt_proximity.c
 * Parade TrueTouch(TM) Standard Product Proximity Module.
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
 *
 */

#include "pt_regs.h"

#define PT_PROXIMITY_NAME "pt_proximity"

/* Timeout value in ms. */
#define PT_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT		1000

#define PT_PROXIMITY_ON 0
#define PT_PROXIMITY_OFF 1

/*******************************************************************************
 * FUNCTION: get_prox_data
 *
 * SUMMARY: Gets pointer of proximity data from core data structure
 *
 * RETURN:
 *	 pointer of pt_proximity_data structure in core data structure
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static inline struct pt_proximity_data *get_prox_data(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);

	return &cd->pd;
}

/*******************************************************************************
 * FUNCTION: pt_report_proximity
 *
 * SUMMARY: Reports proximity event
 *
 * PARAMETERS:
 *     *pd  - pointer to proximity data structure
 *      on  - state of proximity(true:on; false:off)
 ******************************************************************************/
static void pt_report_proximity(struct pt_proximity_data *pd,
	bool on)
{
	int val = on ? PT_PROXIMITY_ON : PT_PROXIMITY_OFF;

	input_report_abs(pd->input, ABS_DISTANCE, val);
	input_sync(pd->input);
}

/*******************************************************************************
 * FUNCTION: pt_get_touch_axis
 *
 * SUMMARY: Calculates touch axis
 *
 * PARAMETERS:
 *     *pd      - pointer to proximity data structure
 *     *axis    - pointer to axis calculation result
 *      size    - size in bytes
 *      max     - max value of result
 *     *xy_data - pointer to input data to be parsed
 *      bofs    - bit offset
 ******************************************************************************/
static void pt_get_touch_axis(struct pt_proximity_data *pd,
	int *axis, int size, int max, u8 *xy_data, int bofs)
{
	int nbyte;
	int next;

	for (nbyte = 0, *axis = 0, next = 0; nbyte < size; nbyte++) {
		pt_debug(pd->dev, DL_INFO,
			"%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p xy_data[%d]=%02X(%d) bofs=%d\n",
			__func__, *axis, *axis, size, max, xy_data, next,
			xy_data[next], xy_data[next], bofs);
		*axis = *axis + ((xy_data[next] >> bofs) << (nbyte * 8));
		next++;
	}

	*axis &= max - 1;

	pt_debug(pd->dev, DL_INFO,
		"%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p xy_data[%d]=%02X(%d)\n",
		__func__, *axis, *axis, size, max, xy_data, next,
		xy_data[next], xy_data[next]);
}

/*******************************************************************************
 * FUNCTION: pt_get_touch_hdr
 *
 * SUMMARY: Gets header of touch report
 *
 * PARAMETERS:
 *     *pd      - pointer to proximity data structure
 *     *touch   - pointer to pt_touch structure
 *     *xy_mode - pointer to input mode data
 ******************************************************************************/
static void pt_get_touch_hdr(struct pt_proximity_data *pd,
	struct pt_touch *touch, u8 *xy_mode)
{
	struct device *dev = pd->dev;
	struct pt_sysinfo *si = pd->si;
	enum pt_tch_hdr hdr;

	for (hdr = PT_TCH_TIME; hdr < PT_TCH_NUM_HDR; hdr++) {
		if (!si->tch_hdr[hdr].report)
			continue;
		pt_get_touch_axis(pd, &touch->hdr[hdr],
			si->tch_hdr[hdr].size,
			si->tch_hdr[hdr].max,
			xy_mode + si->tch_hdr[hdr].ofs,
			si->tch_hdr[hdr].bofs);
		pt_debug(dev, DL_INFO, "%s: get %s=%04X(%d)\n",
			__func__, pt_tch_hdr_string[hdr],
			touch->hdr[hdr], touch->hdr[hdr]);
	}
}

/*******************************************************************************
 * FUNCTION: pt_get_touch
 *
 * SUMMARY: Parse proximity touch event
 *
 * PARAMETERS:
 *     *pd       - pointer to proximity data structure
 *     *touch    - pointer to touch structure
 *      xy_data  - pointer to touch data
 ******************************************************************************/
static void pt_get_touch(struct pt_proximity_data *pd,
	struct pt_touch *touch, u8 *xy_data)
{
	struct device *dev = pd->dev;
	struct pt_sysinfo *si = pd->si;
	enum pt_tch_abs abs;

	for (abs = PT_TCH_X; abs < PT_TCH_NUM_ABS; abs++) {
		if (!si->tch_abs[abs].report)
			continue;
		pt_get_touch_axis(pd, &touch->abs[abs],
			si->tch_abs[abs].size,
			si->tch_abs[abs].max,
			xy_data + si->tch_abs[abs].ofs,
			si->tch_abs[abs].bofs);
		pt_debug(dev, DL_INFO, "%s: get %s=%04X(%d)\n",
			__func__, pt_tch_abs_string[abs],
			touch->abs[abs], touch->abs[abs]);
	}

	pt_debug(dev, DL_INFO, "%s: x=%04X(%d) y=%04X(%d)\n",
		__func__, touch->abs[PT_TCH_X], touch->abs[PT_TCH_X],
		touch->abs[PT_TCH_Y], touch->abs[PT_TCH_Y]);
}

/*******************************************************************************
 * FUNCTION: pt_get_proximity_touch
 *
 * SUMMARY: Parse and report proximity touch event
 *
 * PARAMETERS:
 *     *pd      - pointer to proximity data structure
 *     *touch   - pointer to pt_touch structure
 ******************************************************************************/
static void pt_get_proximity_touch(struct pt_proximity_data *pd,
		struct pt_touch *tch, int num_cur_tch)
{
	struct pt_sysinfo *si = pd->si;
	int i;

	for (i = 0; i < num_cur_tch; i++) {
		pt_get_touch(pd, tch, si->xy_data +
			(i * si->desc.tch_record_size));

		/* Check for proximity event */
		if (tch->abs[PT_TCH_O] == PT_OBJ_PROXIMITY) {
			if (tch->abs[PT_TCH_E] == PT_EV_TOUCHDOWN)
				pt_report_proximity(pd, true);
			else if (tch->abs[PT_TCH_E] == PT_EV_LIFTOFF)
				pt_report_proximity(pd, false);
			break;
		}
	}
}

/*******************************************************************************
 * FUNCTION: pt_xy_worker
 *
 * SUMMARY: Read xy_data for all current touches
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *pd - pointer to proximity data structure
 ******************************************************************************/
static int pt_xy_worker(struct pt_proximity_data *pd)
{
	struct device *dev = pd->dev;
	struct pt_sysinfo *si = pd->si;
	struct pt_touch tch;
	u8 num_cur_tch;

	pt_get_touch_hdr(pd, &tch, si->xy_mode + 3);

	num_cur_tch = tch.hdr[PT_TCH_NUM];
	if (num_cur_tch > si->sensing_conf_data.max_tch) {
		pt_debug(dev, DL_ERROR, "%s: Num touch err detected (n=%d)\n",
			__func__, num_cur_tch);
		num_cur_tch = si->sensing_conf_data.max_tch;
	}

	if (tch.hdr[PT_TCH_LO])
		pt_debug(dev, DL_WARN, "%s: Large area detected\n",
		__func__);

	/* extract xy_data for all currently reported touches */
	pt_debug(dev, DL_INFO, "%s: extract data num_cur_rec=%d\n",
		__func__, num_cur_tch);
	if (num_cur_tch)
		pt_get_proximity_touch(pd, &tch, num_cur_tch);
	else
		pt_report_proximity(pd, false);

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_mt_attention
 *
 * SUMMARY: Wrapper function for pt_xy_worker() that subscribe into the TTDL
 *  attention list.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_proximity_attention(struct device *dev)
{
	struct pt_proximity_data *pd = get_prox_data(dev);
	int rc = 0;

	if (pd->si->xy_mode[2] != pd->si->desc.tch_report_id)
		return 0;

	mutex_lock(&pd->prox_lock);
	rc = pt_xy_worker(pd);
	mutex_unlock(&pd->prox_lock);
	if (rc < 0)
		pt_debug(dev, DL_ERROR, "%s: xy_worker error r=%d\n",
			__func__, rc);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_startup_attention
 *
 * SUMMARY: Wrapper function for pt_report_proximity() that subcribe into the
 *  TTDL attention list.
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_startup_attention(struct device *dev)
{
	struct pt_proximity_data *pd = get_prox_data(dev);

	mutex_lock(&pd->prox_lock);
	pt_report_proximity(pd, false);
	mutex_unlock(&pd->prox_lock);

	return 0;
}

/*******************************************************************************
 * FUNCTION: _pt_set_proximity_via_touchmode_enabled
 *
 * SUMMARY: Enable/Disable proximity via touchmode parameter
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *pd     - pointer to proximity data structure
 *      enable - enable or disable proximity(true:enable; false:disable)
 ******************************************************************************/
static int _pt_set_proximity_via_touchmode_enabled(
		struct pt_proximity_data *pd, bool enable)
{
	struct device *dev = pd->dev;
	u32 touchmode_enabled;
	int rc;

	rc = _pt_request_pip_get_param(dev, 0,
			PT_RAM_ID_TOUCHMODE_ENABLED, &touchmode_enabled);
	if (rc)
		return rc;

	if (enable)
		touchmode_enabled |= 0x80;
	else
		touchmode_enabled &= 0x7F;

	rc = _pt_request_pip_set_param(dev, 0,
			PT_RAM_ID_TOUCHMODE_ENABLED, touchmode_enabled,
			PT_RAM_ID_TOUCHMODE_ENABLED_SIZE);

	return rc;
}

/*******************************************************************************
 * FUNCTION: _pt_set_proximity_via_proximity_enable
 *
 * SUMMARY: Enable/Disable proximity via proximity parameter
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *pd     - pointer to proximity data structure
 *      enable - enable or disable proximity(true:enable; false:disable)
 ******************************************************************************/
static int _pt_set_proximity_via_proximity_enable(
		struct pt_proximity_data *pd, bool enable)
{
	struct device *dev = pd->dev;
	u32 proximity_enable;
	int rc;

	rc = _pt_request_pip_get_param(dev, 0,
			PT_RAM_ID_PROXIMITY_ENABLE, &proximity_enable);
	if (rc)
		return rc;

	if (enable)
		proximity_enable |= 0x01;
	else
		proximity_enable &= 0xFE;

	rc = _pt_request_pip_set_param(dev, 0,
			PT_RAM_ID_PROXIMITY_ENABLE, proximity_enable,
			PT_RAM_ID_PROXIMITY_ENABLE_SIZE);

	return rc;
}

/*******************************************************************************
 * FUNCTION: _pt_set_proximity
 *
 * SUMMARY: Set proximity mode via touchmode parameter or proximity parameter.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *pd     - pointer to proximity data structure
 *      enable - enable or disable proximity(true:enable; false:disable)
 ******************************************************************************/
static int _pt_set_proximity(struct pt_proximity_data *pd,
		bool enable)
{
	if (!IS_PIP_VER_GE(pd->si, 1, 4))
		return _pt_set_proximity_via_touchmode_enabled(pd,
				enable);

	return _pt_set_proximity_via_proximity_enable(pd, enable);
}

/*******************************************************************************
 * FUNCTION: _pt_set_proximity
 *
 * SUMMARY: Enable proximity mode and subscribe into IRQ and STARTUP TTDL
 *  attention list.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *pd     - pointer to proximity data structure
 ******************************************************************************/
static int _pt_proximity_enable(struct pt_proximity_data *pd)
{
	struct device *dev = pd->dev;
	int rc = 0;

	pm_runtime_get_sync(dev);

	rc = pt_request_exclusive(dev,
			PT_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto exit;
	}

	rc = _pt_set_proximity(pd, true);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: Error on request enable proximity scantype r=%d\n",
				__func__, rc);
		goto exit_release;
	}

	pt_debug(dev, DL_INFO, "%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	_pt_subscribe_attention(dev, PT_ATTEN_IRQ, PT_PROXIMITY_NAME,
		pt_proximity_attention, PT_MODE_OPERATIONAL);

	/* set up startup call back */
	_pt_subscribe_attention(dev, PT_ATTEN_STARTUP,
		PT_PROXIMITY_NAME, pt_startup_attention, 0);

exit_release:
	pt_release_exclusive(dev);
exit:
	return rc;
}

/*******************************************************************************
 * FUNCTION: _pt_proximity_disable
 *
 * SUMMARY: Disable proximity mode and unsubscribe from IRQ and STARTUP TTDL
 *  attention list.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *pd     - pointer to proximity data structure
 ******************************************************************************/
static int _pt_proximity_disable(struct pt_proximity_data *pd,
		bool force)
{
	struct device *dev = pd->dev;
	int rc = 0;

	rc = pt_request_exclusive(dev,
			PT_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: Error on request exclusive r=%d\n",
				__func__, rc);
		goto exit;
	}

	rc = _pt_set_proximity(pd, false);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: Error on request disable proximity scan r=%d\n",
				__func__, rc);
		goto exit_release;
	}

exit_release:
	pt_release_exclusive(dev);

exit:
	if (!rc || force) {
		_pt_unsubscribe_attention(dev, PT_ATTEN_IRQ,
			PT_PROXIMITY_NAME, pt_proximity_attention,
			PT_MODE_OPERATIONAL);

		_pt_unsubscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_PROXIMITY_NAME, pt_startup_attention, 0);
	}

	pm_runtime_put(dev);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_proximity_enable_show
 *
 * SUMMARY: Show method for the prox_enable sysfs node that will show the
 *	enable_count of proximity
 *
 * RETURN: Size of printed buffer
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes
 *	*buf  - pointer to output buffer
 ******************************************************************************/
static ssize_t pt_proximity_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_proximity_data *pd = get_prox_data(dev);
	int val = 0;

	mutex_lock(&pd->sysfs_lock);
	val = pd->enable_count;
	mutex_unlock(&pd->sysfs_lock);

	return scnprintf(buf, PT_MAX_PRBUF_SIZE, "%d\n", val);
}

/*******************************************************************************
 * FUNCTION: pt_proximity_enable_store
 *
 * SUMMARY: The store method for the prox_enable sysfs node that allows to
 *  enable or disable proxmity mode.
 *
 * RETURN: Size of passed in buffer
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes
 *	*buf  - pointer to buffer that hold the command parameters
 *	 size - size of buf
 ******************************************************************************/
static ssize_t pt_proximity_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_proximity_data *pd = get_prox_data(dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, 10, &value);
	if (rc < 0 || (value != 0 && value != 1)) {
		pt_debug(dev, DL_ERROR, "%s: Invalid value\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pd->sysfs_lock);
	if (value) {
		if (pd->enable_count++) {
			pt_debug(dev, DL_WARN, "%s: '%s' already enabled\n",
				__func__, pd->input->name);
		} else {
			rc = _pt_proximity_enable(pd);
			if (rc)
				pd->enable_count--;
		}
	} else {
		if (--pd->enable_count) {
			if (pd->enable_count < 0) {
				pt_debug(dev, DL_ERROR, "%s: '%s' unbalanced disable\n",
					__func__, pd->input->name);
				pd->enable_count = 0;
			}
		} else {
			rc = _pt_proximity_disable(pd, false);
			if (rc)
				pd->enable_count++;
		}
	}
	mutex_unlock(&pd->sysfs_lock);

	if (rc)
		return rc;

	return size;
}

static DEVICE_ATTR(prox_enable, 0600,
		pt_proximity_enable_show,
		pt_proximity_enable_store);

/*******************************************************************************
 * FUNCTION: pt_setup_input_device_and_sysfs
 *
 * SUMMARY: Create sysnode, set event signal capabilities and register input
 *  device for proximity.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *     *dev - pointer to device structure
 ******************************************************************************/
static int pt_setup_input_device_and_sysfs(struct device *dev)
{
	struct pt_proximity_data *pd = get_prox_data(dev);
	int signal = PT_IGNORE_VALUE;
	int i;
	int rc;

	rc = device_create_file(dev, &dev_attr_prox_enable);
	if (rc) {
		pt_debug(dev, DL_ERROR, "%s: Error, could not create enable\n",
				__func__);
		goto exit;
	}

	pt_debug(dev, DL_INFO, "%s: Initialize event signals\n",
				__func__);

	__set_bit(EV_ABS, pd->input->evbit);

	/* set event signal capabilities */
	for (i = 0; i < NUM_SIGNALS(pd->pdata->frmwrk); i++) {
		signal = PARAM_SIGNAL(pd->pdata->frmwrk, i);
		if (signal != PT_IGNORE_VALUE) {
			input_set_abs_params(pd->input, signal,
				PARAM_MIN(pd->pdata->frmwrk, i),
				PARAM_MAX(pd->pdata->frmwrk, i),
				PARAM_FUZZ(pd->pdata->frmwrk, i),
				PARAM_FLAT(pd->pdata->frmwrk, i));
		}
	}

	rc = input_register_device(pd->input);
	if (rc) {
		pt_debug(dev, DL_ERROR, "%s: Error, failed register input device r=%d\n",
			__func__, rc);
		goto unregister_enable;
	}

	pd->input_device_registered = true;
	return rc;

unregister_enable:
	device_remove_file(dev, &dev_attr_prox_enable);
exit:
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_setup_input_attention
 *
 * SUMMARY: Wrapper function for pt_setup_input_device_and_sysfs() that
 *  subscribe into TTDL attention list.
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
	struct pt_proximity_data *pd = get_prox_data(dev);
	int rc;

	pd->si = _pt_request_sysinfo(dev);
	if (!pd->si)
		return -EINVAL;

	rc = pt_setup_input_device_and_sysfs(dev);
	if (!rc)
		rc = _pt_set_proximity(pd, false);

	_pt_unsubscribe_attention(dev, PT_ATTEN_STARTUP,
		PT_PROXIMITY_NAME, pt_setup_input_attention, 0);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_proximity_probe
 *
 * SUMMARY: The probe function for proximity input device
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev   - pointer to device structure
 ******************************************************************************/
int pt_proximity_probe(struct device *dev)
{
	struct pt_core_data *cd = dev_get_drvdata(dev);
	struct pt_proximity_data *pd = &cd->pd;
	struct pt_platform_data *pdata = dev_get_platdata(dev);
	struct pt_proximity_platform_data *prox_pdata;
	int rc = 0;

	if (!pdata ||  !pdata->prox_pdata) {
		pt_debug(dev, DL_ERROR,
			"%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}
	prox_pdata = pdata->prox_pdata;

	mutex_init(&pd->prox_lock);
	mutex_init(&pd->sysfs_lock);
	pd->dev = dev;
	pd->pdata = prox_pdata;

	/* Create the input device and register it. */
	pt_debug(dev, DL_INFO,
		"%s: Create the input device and register it\n", __func__);
	pd->input = input_allocate_device();
	if (!pd->input) {
		pt_debug(dev, DL_ERROR, "%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENODEV;
		goto error_alloc_failed;
	} else
		pd->input_device_allocated = true;

	if (pd->pdata->inp_dev_name)
		pd->input->name = pd->pdata->inp_dev_name;
	else
		pd->input->name = PT_PROXIMITY_NAME;
	scnprintf(pd->phys, sizeof(pd->phys), "%s/input%d", dev_name(dev),
			cd->phys_num++);
	pd->input->phys = pd->phys;
	pd->input->dev.parent = pd->dev;
	input_set_drvdata(pd->input, pd);

	/* get sysinfo */
	pd->si = _pt_request_sysinfo(dev);

	if (pd->si) {
		rc = pt_setup_input_device_and_sysfs(dev);
		if (rc)
			goto error_init_input;

		rc = _pt_set_proximity(pd, false);
	} else {
		pt_debug(dev, DL_ERROR, "%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, pd->si);
		_pt_subscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_PROXIMITY_NAME, pt_setup_input_attention,
			0);
	}

	return 0;

error_init_input:
	input_free_device(pd->input);
	pd->input_device_allocated = false;
error_alloc_failed:
error_no_pdata:
	pt_debug(dev, DL_ERROR, "%s failed.\n", __func__);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_proximity_release
 *
 * SUMMARY: The release function for proximity input device
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *	*dev   - pointer to device structure
 ******************************************************************************/
int pt_proximity_release(struct device *dev)
{
	struct pt_proximity_data *pd;

	/* Ensure valid pointers before de-referencing them */
	if (dev)
		pd = get_prox_data(dev);
	else
		return 0;

	/*
	 * Second call this function may cause kernel panic if probe fail.
	 * Use input_device_registered & input_device_allocated variable to
	 * avoid unregister or free unavailable devive.
	 */
	if (pd && pd->input_device_registered) {
		/* Disable proximity sensing */
		pd->input_device_registered = false;
		mutex_lock(&pd->sysfs_lock);
		if (pd->enable_count)
			_pt_proximity_disable(pd, true);
		mutex_unlock(&pd->sysfs_lock);
		device_remove_file(dev, &dev_attr_prox_enable);
		input_unregister_device(pd->input);
		/* Unregistering device will free the device too */
		pd->input_device_allocated = false;
	} else if (pd && pd->input_device_allocated) {
		pd->input_device_allocated = false;
		input_free_device(pd->input);
		_pt_unsubscribe_attention(dev, PT_ATTEN_STARTUP,
			PT_PROXIMITY_NAME, pt_setup_input_attention,
			0);
	}

	return 0;
}
