/*
 * pt_debug.c
 * Parade TrueTouch(TM) Standard Product Debug Module.
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

#define PT_DEBUG_NAME "pt_debug"

struct pt_debug_data {
	struct device *dev;
	struct pt_sysinfo *si;
	uint32_t interrupt_count;
	uint32_t formated_output;
	struct mutex sysfs_lock;
	u8 pr_buf[PT_MAX_PRBUF_SIZE];
};

static struct pt_core_commands *cmd;

static struct pt_module debug_module;

/*******************************************************************************
 * FUNCTION: pt_get_debug_data
 *
 * SUMMARY: Inline function to get pt_debug_data pointer from debug module.
 *
 * RETURN:
 *	 pointer to pt_debug_data structure
 *
 * PARAMETERS:
 *	*dev          - pointer to device structure
 ******************************************************************************/
static inline struct pt_debug_data *pt_get_debug_data(
		struct device *dev)
{
	return pt_get_module_data(dev, &debug_module);
}

/*******************************************************************************
 * FUNCTION: pt_pr_buf_op_mode
 *
 * SUMMARY: Formats touch/button report to pr_buf that combined xy_mode and
 *  xy_data. The feature is required by TTHE.
 *
 * PARAMETERS:
 *  *dd         - pointer to pt_debug_data structure
 *  *pr_buf     - pointer to print buffer
 *  *si         - pointer to sysinfo structure
 *   cur_touch  - number of current touch
 ******************************************************************************/
static void pt_pr_buf_op_mode(struct pt_debug_data *dd, u8 *pr_buf,
		struct pt_sysinfo *si, u8 cur_touch)
{
	const char fmt[] = "%02X ";
	int max = (PT_MAX_PRBUF_SIZE - 1) - sizeof(PT_PR_TRUNCATED);
	u8 report_id = si->xy_mode[2];
	int header_size = 0;
	int report_size = 0;
	int total_size = 0;
	int i, k;

	if (report_id == si->desc.tch_report_id) {
		header_size = si->desc.tch_header_size;
		report_size = cur_touch * si->desc.tch_record_size;
	} else if (report_id == si->desc.btn_report_id) {
		header_size = BTN_INPUT_HEADER_SIZE;
		report_size = BTN_REPORT_SIZE;
	}
	total_size = header_size + report_size;

	pr_buf[0] = 0;
	for (i = k = 0; i < header_size && i < max; i++, k += 3)
		scnprintf(pr_buf + k, PT_MAX_PRBUF_SIZE, fmt, si->xy_mode[i]);

	for (i = 0; i < report_size && i < max; i++, k += 3)
		scnprintf(pr_buf + k, PT_MAX_PRBUF_SIZE, fmt, si->xy_data[i]);

	pr_info("%s=%s%s\n", "pt_OpModeData", pr_buf,
			total_size <= max ? "" : PT_PR_TRUNCATED);
}

/*******************************************************************************
 * FUNCTION: pt_debug_print
 *
 * SUMMARY: This function prints header to show data size and data_name and
 *  content of "pr_buf" with hex base.
 *
 * PARAMETERS:
 *  *dev        - pointer to device structure
 *  *pr_buf     - pointer to input buffer which stores the formated data
 *  *sptr       - pointer to the buffer to print
 *   size       - size of data elements
 *  *data_name  - data name to print
 ******************************************************************************/
static void pt_debug_print(struct device *dev, u8 *pr_buf, u8 *sptr,
		int size, const char *data_name)
{
	int i, j;
	int elem_size = sizeof("XX ") - 1;
	int max = (PT_MAX_PRBUF_SIZE - 1) / elem_size;
	int limit = size < max ? size : max;

	if (limit < 0)
		limit = 0;

	pr_buf[0] = 0;
	for (i = j = 0; i < limit; i++, j += elem_size)
		scnprintf(pr_buf + j, PT_MAX_PRBUF_SIZE - j, "%02X ", sptr[i]);

	if (size)
		pr_info("%s[0..%d]=%s%s\n", data_name, size - 1, pr_buf,
			size <= max ? "" : PT_PR_TRUNCATED);
	else
		pr_info("%s[]\n", data_name);
}

/*******************************************************************************
 * FUNCTION: pt_debug_formated
 *
 * SUMMARY: Formats and prints touch & button report.
 *
 * PARAMETERS:
 *  *dev        - pointer to device structure
 *  *pr_buf     - pointer to print buffer
 *  *si         - pointer to sysinfo structure
 *   cur_touch  - number of current touch
 ******************************************************************************/
static void pt_debug_formated(struct device *dev, u8 *pr_buf,
		struct pt_sysinfo *si, u8 num_cur_tch)
{
	u8 report_id = si->xy_mode[2];
	int header_size = 0;
	int report_size = 0;
	u8 data_name[] = "touch[99]";
	int max_print_length = 20;
	int i;

	if (report_id == si->desc.tch_report_id) {
		header_size = si->desc.tch_header_size;
		report_size = num_cur_tch * si->desc.tch_record_size;
	} else if (report_id == si->desc.btn_report_id) {
		header_size = BTN_INPUT_HEADER_SIZE;
		report_size = BTN_REPORT_SIZE;
	}

	/* xy_mode */
	pt_debug_print(dev, pr_buf, si->xy_mode, header_size, "xy_mode");

	/* xy_data */
	if (report_size > max_print_length) {
		pr_info("xy_data[0..%d]:\n", report_size);
		for (i = 0; i < report_size - max_print_length;
				i += max_print_length) {
			pt_debug_print(dev, pr_buf, si->xy_data + i,
					max_print_length, " ");
		}
		if (report_size - i)
			pt_debug_print(dev, pr_buf, si->xy_data + i,
					report_size - i, " ");
	} else {
		pt_debug_print(dev, pr_buf, si->xy_data, report_size,
				"xy_data");
	}

	/* touches */
	if (report_id == si->desc.tch_report_id) {
		for (i = 0; i < num_cur_tch; i++) {
			scnprintf(data_name, sizeof(data_name) - 1,
					"touch[%u]", i);
			pt_debug_print(dev, pr_buf,
				si->xy_data + (i * si->desc.tch_record_size),
				si->desc.tch_record_size, data_name);
		}
	}

	/* buttons */
	if (report_id == si->desc.btn_report_id)
		pt_debug_print(dev, pr_buf, si->xy_data, report_size,
				"button");
}

/*******************************************************************************
 * FUNCTION: pt_xy_worker
 *
 * SUMMARY: Read xy_data for all touches for debug.
 *
 * RETURN:
 *   0 = success
 *
 * PARAMETERS:
 *  *dd - pointer to pt_debug_data structure
 ******************************************************************************/
static int pt_xy_worker(struct pt_debug_data *dd)
{
	struct device *dev = dd->dev;
	struct pt_sysinfo *si = dd->si;
	u8 report_reg = si->xy_mode[TOUCH_COUNT_BYTE_OFFSET];
	u8 num_cur_tch = GET_NUM_TOUCHES(report_reg);
	uint32_t formated_output;

	mutex_lock(&dd->sysfs_lock);
	dd->interrupt_count++;
	formated_output = dd->formated_output;
	mutex_unlock(&dd->sysfs_lock);

	/* Interrupt */
	pr_info("Interrupt(%u)\n", dd->interrupt_count);

	if (formated_output)
		pt_debug_formated(dev, dd->pr_buf, si, num_cur_tch);
	else
		/* print data for TTHE */
		pt_pr_buf_op_mode(dd, dd->pr_buf, si, num_cur_tch);

	pr_info("\n");

	return 0;
}

/*******************************************************************************
 * FUNCTION: pt_debug_attention
 *
 * SUMMARY: Wrapper funtion for pt_xy_worker() to subcribe into TTDL attention
 *  list.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 ******************************************************************************/
static int pt_debug_attention(struct device *dev)
{
	struct pt_debug_data *dd = pt_get_debug_data(dev);
	struct pt_sysinfo *si = dd->si;
	u8 report_id = si->xy_mode[2];
	int rc = 0;

	if (report_id != si->desc.tch_report_id
			&& report_id != si->desc.btn_report_id)
		return 0;

	/* core handles handshake */
	rc = pt_xy_worker(dd);
	if (rc < 0)
		pt_debug(dev, DL_ERROR, "%s: xy_worker error r=%d\n",
			__func__, rc);

	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_status_show
 *
 * SUMMARY: The show method for the int_count sysfs node. This node displays
 *	the count of interrupt.
 *
 * PARAMETERS:
 *      *dev  - pointer to Device structure
 *      *attr - pointer to the device attribute structure
 *      *buf  - pointer to buffer to print
 ******************************************************************************/
static ssize_t pt_interrupt_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_debug_data *dd = pt_get_debug_data(dev);
	int val;

	mutex_lock(&dd->sysfs_lock);
	val = dd->interrupt_count;
	mutex_unlock(&dd->sysfs_lock);

	return scnprintf(buf, PT_MAX_PRBUF_SIZE, "Interrupt Count: %d\n", val);
}

/*******************************************************************************
 * FUNCTION: pt_interrupt_count_store
 *
 * SUMMARY: The store method for the int_count sysfs node that allows the count
 *	of interrput to be cleared.
 *
 * RETURN: Size of passed in buffer
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes
 *	*buf  - pointer to buffer that hold the command parameters
 *	 size - size of buf
 ******************************************************************************/
static ssize_t pt_interrupt_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_debug_data *dd = pt_get_debug_data(dev);

	mutex_lock(&dd->sysfs_lock);
	dd->interrupt_count = 0;
	mutex_unlock(&dd->sysfs_lock);
	return size;
}

static DEVICE_ATTR(int_count, 0600,
	pt_interrupt_count_show, pt_interrupt_count_store);

/*******************************************************************************
 * FUNCTION: pt_formated_output_show
 *
 * SUMMARY: Show method for the formated_output sysfs node that will show
 *  whether to format data to buffer or print directly.
 *
 * RETURN:
 *	 0 = success
 *	!0 = failure
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes
 *	*buf  - pointer to output buffer
 ******************************************************************************/
static ssize_t pt_formated_output_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pt_debug_data *dd = pt_get_debug_data(dev);
	int val;

	mutex_lock(&dd->sysfs_lock);
	val = dd->formated_output;
	mutex_unlock(&dd->sysfs_lock);

	return scnprintf(buf, PT_MAX_PRBUF_SIZE,
			"Formated debug output: %x\n", val);
}

/*******************************************************************************
 * FUNCTION: pt_formated_output_store
 *
 * SUMMARY: The store method for the formated_output sysfs node. Allows the
 *  setting to format data to buffer or print directly.
 *
 * RETURN: Size of passed in buffer
 *
 * PARAMETERS:
 *	*dev  - pointer to device structure
 *	*attr - pointer to device attributes
 *	*buf  - pointer to buffer that hold the command parameters
 *	 size - size of buf
 ******************************************************************************/
static ssize_t pt_formated_output_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct pt_debug_data *dd = pt_get_debug_data(dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, 10, &value);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: Invalid value\n", __func__);
		return size;
	}

	/* Expecting only 0 or 1 */
	if (value != 0 && value != 1) {
		pt_debug(dev, DL_ERROR, "%s: Invalid value %lu\n",
					__func__, value);
		return size;
	}

	mutex_lock(&dd->sysfs_lock);
	dd->formated_output = value;
	mutex_unlock(&dd->sysfs_lock);
	return size;
}

static DEVICE_ATTR(formated_output, 0600,
	pt_formated_output_show, pt_formated_output_store);

/*******************************************************************************
 * FUNCTION: pt_mt_probe
 *
 * SUMMARY: The probe function for debug module to create sysfs nodes and
 *  subscribe attention list.
 *
 * RETURN:
 *	 0 = success
 *
 * PARAMETERS:
 *   *dev   - pointer to device structure
 *  **data  - double pointer tothe pt_debug_data structure to be created here
 ******************************************************************************/
static int pt_debug_probe(struct device *dev, void **data)
{
	struct pt_debug_data *dd;
	int rc;

	/* get context and debug print buffers */
	dd = kzalloc(sizeof(*dd), GFP_KERNEL);
	if (!dd) {
		rc = -ENOMEM;
		goto pt_debug_probe_alloc_failed;
	}

	rc = device_create_file(dev, &dev_attr_int_count);
	if (rc) {
		pt_debug(dev, DL_ERROR, "%s: Error, could not create int_count\n",
					__func__);
		goto pt_debug_probe_create_int_count_failed;
	}

	rc = device_create_file(dev, &dev_attr_formated_output);
	if (rc) {
		pt_debug(dev, DL_ERROR, "%s: Error, could not create formated_output\n",
					__func__);
		goto pt_debug_probe_create_formated_failed;
	}

	mutex_init(&dd->sysfs_lock);
	dd->dev = dev;
	*data = dd;

	dd->si = cmd->request_sysinfo(dev);
	if (!dd->si) {
		pt_debug(dev, DL_ERROR, "%s: Fail get sysinfo pointer from core\n",
					__func__);
		rc = -ENODEV;
		goto pt_debug_probe_sysinfo_failed;
	}

	rc = cmd->subscribe_attention(dev, PT_ATTEN_IRQ, PT_DEBUG_NAME,
		pt_debug_attention, PT_MODE_OPERATIONAL);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: Error, could not subscribe attention cb\n",
					__func__);
		goto pt_debug_probe_subscribe_failed;
	}

	return 0;

pt_debug_probe_subscribe_failed:
pt_debug_probe_sysinfo_failed:
	device_remove_file(dev, &dev_attr_formated_output);
pt_debug_probe_create_formated_failed:
	device_remove_file(dev, &dev_attr_int_count);
pt_debug_probe_create_int_count_failed:
	kfree(dd);
pt_debug_probe_alloc_failed:
	pt_debug(dev, DL_ERROR, "%s failed.\n", __func__);
	return rc;
}

/*******************************************************************************
 * FUNCTION: pt_debug_release
 *
 * SUMMARY: Remove function for debug module that does following cleanup:
 *	- Unsubscibe all registered attention tasks
 *	- Removes all created sysfs nodes
 *
 * PARAMETERS:
 *	*dev   - pointer to device structure
 *	*data  - pointer to the pt_debug_data structure
 ******************************************************************************/
static void pt_debug_release(struct device *dev, void *data)
{
	struct pt_debug_data *dd = data;
	int rc;

	rc = cmd->unsubscribe_attention(dev, PT_ATTEN_IRQ, PT_DEBUG_NAME,
		pt_debug_attention, PT_MODE_OPERATIONAL);
	if (rc < 0) {
		pt_debug(dev, DL_ERROR, "%s: Error, could not un-subscribe attention\n",
					__func__);
		goto pt_debug_release_exit;
	}

pt_debug_release_exit:
	device_remove_file(dev, &dev_attr_formated_output);
	device_remove_file(dev, &dev_attr_int_count);
	kfree(dd);
}

static struct pt_module debug_module = {
	.name = PT_DEBUG_NAME,
	.probe = pt_debug_probe,
	.release = pt_debug_release,
};

/*******************************************************************************
 * FUNCTION: pt_debug_init
 *
 * SUMMARY: Initialize function for debug module which to register
 *  debug_module into TTDL module list.
 *
 * RETURN:
 *	 0 = success
 ******************************************************************************/
static int __init pt_debug_init(void)
{
	int rc;

	cmd = pt_get_commands();
	if (!cmd)
		return -EINVAL;

	rc = pt_register_module(&debug_module);
	if (rc < 0) {
		pr_err("%s: Error, failed registering module\n",
			__func__);
			return rc;
	}

	pr_info("%s: Parade TTSP Debug Driver (Built %s) rc=%d\n",
		 __func__, PT_DRIVER_VERSION, rc);
	return 0;
}
module_init(pt_debug_init);

/*******************************************************************************
 * FUNCTION: pt_debug_exit
 *
 * SUMMARY: Exit function for debug module which to unregister debug_module
 * from TTDL module list.
 *
 ******************************************************************************/
static void __exit pt_debug_exit(void)
{
	pt_unregister_module(&debug_module);
}
module_exit(pt_debug_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Parade TrueTouch(R) Standard Product Debug Driver");
MODULE_AUTHOR("Parade Technologies <ttdrivers@paradetech.com>");
