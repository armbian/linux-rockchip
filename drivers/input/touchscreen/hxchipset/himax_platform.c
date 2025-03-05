/* Himax Android Driver Sample Code for QCT platform
 *
 * Copyright (C) 2021 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "himax.h"
#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83192)
DECLARE_HIMAX_CHIP(hx83192_chip, hx83192_chip_detect);
#endif

/* FIXME */
static int mmi_refcnt;

extern void notify_tp_procfile(const int exsited_flag);

static ssize_t himax_i2c_trc_read(struct file *filp, struct kobject *kobj,
				  struct bin_attribute *bin_attr, char *buf, loff_t off,
				  size_t bytes);
static BIN_ATTR(i2c_trace, 0444, himax_i2c_trc_read, NULL, sizeof_field(struct trc_lines, lines));

/*******************************************************************************
 * Provides the data for the SysFS 'i2c_trace' binary file.  This is the data
 * sent and received via i2c.  If a transfer fails, we cannot say here at
 * which byte the failure occurred so this is not a replacement for a logic
 * analyser.
 *
 * Returns number of bytes stored.
 ******************************************************************************/
static ssize_t himax_i2c_trc_read(struct file *filp, struct kobject *kobj,
				  struct bin_attribute *bin_attr, char *buf, loff_t off,
				  size_t bytes)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	struct trc_line *line;
	int offset = 0;

	/* mutex_lock(&ts->trc_lines.mutex);*/

	if (ts->trc_lines.fill_status >= TRC_NUM_LINES) {
		/* Some debug lines have been lost so say how many */
		offset = scnprintf(buf, bytes, "Discarded %d lines\n",
				   ts->trc_lines.fill_status - TRC_NUM_LINES);

		/* Now we've said how many lines have been lost, we only need
		 * to know the buffer is now completely full. */
		ts->trc_lines.fill_status = TRC_NUM_LINES;
	}

	while ((ts->trc_lines.fill_status > 0) && (offset != bytes)) {
		line = &ts->trc_lines.lines[ts->trc_lines.read_idx];
		/* line_start = line->text + line->read_offset; */

		/* Copy as much of the line as possible into the buffer
		 * (excluding terminating null.
		 */
		while ((offset != bytes) && (line->text[line->read_offset] != '\0')) {
			buf[offset++] = line->text[line->read_offset++];
		}

		if (offset != bytes) {
			/* There's still space in the buffer so move
			 * to the next line in the ring buffer */
			ts->trc_lines.read_idx++;
			ts->trc_lines.read_idx %= TRC_NUM_LINES;
			ts->trc_lines.fill_status--;
		}
	}

	/*	mutex_unlock(&ts->trc_lines.mutex);*/
	return offset;
}

/*******************************************************************************
 * Converts binary data into ASCII hex.  This is a helper function for
 * himax_dbg_add.
 *
 * buf - the buffer to hold the ASCII hex.
 * avail - the number of characters available in buf
 * data - pointer to the binary data
 * len - length of the binary data
 * consumed - pointer (may be NULL) to a variable to hold the number of bytes of
 *            binary data that were consumed.
 *
 * Returns the number of ASCII characters written (not including terminating \0)
 ******************************************************************************/
static int himax_dbg_write_hex(char *buf, size_t avail, const u8 *data, size_t len, int *consumed)
{
	int offset = 0;
	int min_length = 0;
	char buffer[5];

	if (consumed)
		*consumed = 0;

	while ((len > 0) && (avail >= min_length)) {
		min_length = scnprintf(buffer, avail, "%02x%s", *data++, --len > 0 ? " " : "");

		/* Make sure there is enough space to store a full ASCII hex
		 * representation of the byte */
		if (avail >= min_length) {
			memcpy(buf + offset, buffer, min_length);
			offset += min_length;
			avail -= min_length;
			if (consumed)
				(*consumed)++;
		}
	}

	return offset;
}

/*******************************************************************************
 * Add a log message to the debug log.
 *
 * msg - A text message to store.
 * ptr - pointer (may be NULL) to the binary data to convert to hex.
 * data_len - length of the binary data
 ******************************************************************************/
static void himax_dbg_add(struct himax_ts_data *ts, const char *msg, const u8 *ptr, int data_len)
{
	struct trc_line *line;
	int length;
	int consumed;
	int offset;
	int idx;
	/*	struct timespec  ts_nsec;
	getnstimeofday(&ts_nsec);*/

	/*	if (!ts->trc_lines.enabled)
		return;*/

	/*	mutex_lock(&ts->trc_lines.mutex);*/

	do {
		offset = 0;
		idx = ts->trc_lines.write_idx;
		line = &ts->trc_lines.lines[idx];

		line->read_offset = 0;
		/*
		offset = scnprintf(line->text,
				   TRC_TEXT_LENGTH,
				   "[%5llu.%06llu] ",
				   ts_nsec.tv_nsec / 1000000000,
				   (ts_nsec.tv_nsec % 1000000000) / 1000);
		*/
		length = scnprintf(line->text + offset, TRC_TEXT_LENGTH - offset, "%s", msg);

		offset += length;
		if (offset == TRC_TEXT_LENGTH)
			/* We didn't copy all of the message */
			msg += length;
		else
			/* Message was completely copied but prepare for having
			 * too much binary data for one line */
			msg = "|->";

		if (ptr) {
			length = himax_dbg_write_hex(line->text + offset, TRC_TEXT_LENGTH - offset,
						     ptr, data_len, &consumed);

			offset += length;
			ptr += consumed;
			data_len -= consumed;
		}

		/* Add the end-of-line string.
		 * We don't need to check for space because we've used
		 * TRC_TEXT_LENGTH so far which leaves space for the end-of-line
		 * string and terminating NUL.
		 */
		scnprintf(&line->text[offset], TRC_EOL_LENGTH, TRC_EOL);

		ts->trc_lines.write_idx = (++idx) % TRC_NUM_LINES;

		/* If we've filled the ring buffer and have wrapped round,
		 * move the read index forward so that it always points to the
		 * oldest unread entry that still exists. */
		if (++ts->trc_lines.fill_status > TRC_NUM_LINES) {
			ts->trc_lines.read_idx++;
			ts->trc_lines.read_idx %= TRC_NUM_LINES;
		}
	} while (data_len > 0);

	/* mutex_unlock(&ts->trc_lines.mutex);*/
}

/*******************************************************************************
 * Creates the SysFS file entries.
 ******************************************************************************/
int himax_sysfs_init(struct himax_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	int error;
	/*
	error = sysfs_create_group(&client->dev.kobj, &himax_attr_group);
	if (error) {
		dev_err(&client->dev,
			"Failed to create sysfs sysfs group (%d)\n",
			error);
		return error;
	}
	*/
	error = sysfs_create_bin_file(&client->dev.kobj, &bin_attr_i2c_trace);

	/* This is only for debugging purposes so failing to create the
	 * file for whatever reason is not a critical error. */
	if (error)
		dev_warn(&client->dev, "Failed to create %s (%d)\n", bin_attr_i2c_trace.attr.name,
			 error);

	return error;
}

void himax_sysfs_deinit(struct himax_ts_data *ts)
{
	struct i2c_client *client = ts->client;

	sysfs_remove_bin_file(&client->dev.kobj, &bin_attr_i2c_trace);
}

int himax_dev_set(struct himax_ts_data *ts)
{
	int ret = 0;

	ts->input_dev = input_allocate_device();

	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device-input_dev\n", __func__);
		return ret;
	}

	if (ts->location) {
		ts->input_dev->name = ts->location;
		/* NOTE: To identify each display. */
		ts->input_dev->phys =
			devm_kasprintf(ts->dev, GFP_KERNEL, "himax-%s/input0", dev_name(ts->dev));
	} else {
		ts->input_dev->name = "himax-touchscreen";
	}

	return ret;
}
int himax_input_register_device(struct input_dev *input_dev)
{
	return input_register_device(input_dev);
}

int himax_parse_dt(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata)
{
	int rc, coords_size = 0;
	uint32_t coords[4] = { 0 };
	struct property *prop;
	struct device_node *dt = ts->client->dev.of_node;
	u32 data = 0;
	int ret = 0;
	const char *name;
	struct pinctrl *p;
	struct pinctrl_state *default_state;

	of_property_read_string(dt, "himax,location", &ts->location);

	prop = of_find_property(dt, "himax,panel-coords", NULL);

	if (prop) {
		coords_size = prop->length / sizeof(u32);

		if (coords_size != 4)
			D(" %s:Invalid panel coords size %d\n", __func__, coords_size);
	}

	ret = of_property_read_u32_array(dt, "himax,panel-coords", coords, coords_size);
	if (ret == 0) {
		pdata->abs_x_min = coords[0];
		pdata->abs_x_max = (coords[1] - 1);
		pdata->abs_y_min = coords[2];
		pdata->abs_y_max = (coords[3] - 1);
		I(" DT-%s:panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
		  pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
	}

	prop = of_find_property(dt, "himax,display-coords", NULL);

	if (prop) {
		coords_size = prop->length / sizeof(u32);

		if (coords_size != 4)
			D(" %s:Invalid display coords size %d\n", __func__, coords_size);
	}

	rc = of_property_read_u32_array(dt, "himax,display-coords", coords, coords_size);

	if (rc && (rc != -EINVAL)) {
		D(" %s:Fail to read display-coords %d\n", __func__, rc);
		return rc;
	}

	pdata->screenWidth = coords[1];
	pdata->screenHeight = coords[3];
	I(" DT-%s:display-coords = (%d, %d)\n", __func__, pdata->screenWidth, pdata->screenHeight);
	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_irq))
		I(" DT:gpio_irq value is not valid\n");

	pdata->fail_det = of_get_named_gpio(dt, "himax,fail-det", 0);

	if (!gpio_is_valid(pdata->fail_det))
		I(" DT:fail_det value is not valid\n");

	pdata->gpio_reset = of_get_named_gpio(dt, "himax,rst-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_reset))
		I(" DT:gpio_rst value is not valid\n");

	pdata->gpio_pon = of_get_named_gpio(dt, "himax,pon-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_pon))
		I(" DT:gpio_pon value is not valid\n");

	pdata->lcm_rst = of_get_named_gpio(dt, "himax,lcm-rst", 0);

	if (!gpio_is_valid(pdata->lcm_rst))
		I(" DT:tp-rst value is not valid\n");

	I(" DT:pdata->gpio_pon=%d, pdata->lcm_rst=%d\n", pdata->gpio_pon, pdata->lcm_rst);

	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_3v3_en))
		I(" DT:gpio_3v3_en value is not valid\n");

	I(" DT:gpio_irq=%d, gpio_rst=%d, gpio_3v3_en=%d\n", pdata->gpio_irq, pdata->gpio_reset,
	  pdata->gpio_3v3_en);
	I(" DT:fail_det=%d", pdata->fail_det);

	if (of_property_read_u32(dt, "report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d\n", pdata->protocol_type);
	}

	if (of_property_read_string(dt, "himax,tp-fw-bin", &name) < 0) {
		I("DT:fail to get himax tp fw bin\n");
	} else {
		pdata->fw_name = name;
		I("DT:himax tp fw bin:%s\n", pdata->fw_name);
	}

	if (of_property_read_string(dt, "himax,hx-criteria", &name) < 0) {
		I("DT:fail to get himax,hx-criteria file\n");
	} else {
		pdata->criteria_file_name = name;
		I("DT:himax,hx-criteria:%s\n", pdata->criteria_file_name);
	}

	p = devm_pinctrl_get(&ts->client->dev);
	default_state = pinctrl_lookup_state(p, "default");
	pinctrl_select_state(p, default_state);
	I("%s: active pinctrl!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(himax_parse_dt);

int himax_bus_read(struct himax_ts_data *ts, uint8_t command, uint8_t *data, uint32_t length,
		   uint8_t toRetry)
{
	int retry;
	char buf[50];
	int i;
	int ret = 0;
	struct i2c_client *client = ts->client;
	struct i2c_msg msg[] = { {
					 .addr = client->addr,
					 .flags = 0,
					 .len = 1,
					 .buf = &command,
				 },
				 {
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = length,
					 .buf = ts->rw_buf,
				 } };
	mutex_lock(&ts->rw_lock);

	for (retry = 0; retry < toRetry; retry++) {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (ret == 2) {
			memcpy(data, ts->rw_buf, length);
			break;
		}
		msleep(20);
	}

	for (i = 0; i < ret; i++) {
		himax_dbg_add(ts, msg[i].flags & I2C_M_RD ? "[R]" : "[W]", (u8 *)msg[i].buf,
			      msg[i].len);
	}
	scnprintf(buf, sizeof(buf), "Messages sent: %d/%d", ret, 2);
	buf[sizeof(buf) - 1] = '\0';
	himax_dbg_add(ts, buf, 0, 0);

	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n", __func__, toRetry);
		mutex_unlock(&ts->rw_lock);
		return -EIO;
	}

	mutex_unlock(&ts->rw_lock);
	return 0;
}
EXPORT_SYMBOL(himax_bus_read);

int himax_bus_write(struct himax_ts_data *ts, uint8_t command, uint8_t *data, uint32_t length,
		    uint8_t toRetry)
{
	int retry;
	char buf[50];
	int i;
	int ret = 0;
	struct i2c_client *client = ts->client;
	struct i2c_msg msg[] = { {
		.addr = client->addr,
		.flags = 0,
		.len = length + 1,
		.buf = ts->rw_buf,
	} };

	mutex_lock(&ts->rw_lock);
	ts->rw_buf[0] = command;
	if (data != NULL)
		memcpy(ts->rw_buf + 1, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret == 1)
			break;
		msleep(20);
	}

	for (i = 0; i < ret; i++) {
		himax_dbg_add(ts, msg[i].flags & I2C_M_RD ? "[R]" : "[W]", (u8 *)msg[i].buf,
			      msg[i].len);
	}

	scnprintf(buf, sizeof(buf), "Messages sent: %d/%d", ret, 1);
	buf[sizeof(buf) - 1] = '\0';
	himax_dbg_add(ts, buf, 0, 0);

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n", __func__, toRetry);
		mutex_unlock(&ts->rw_lock);
		return -EIO;
	}

	mutex_unlock(&ts->rw_lock);
	return 0;
}
EXPORT_SYMBOL(himax_bus_write);

void himax_int_enable(struct himax_ts_data *ts, int enable)
{
	unsigned long irqflags = 0;
	int irqnum = ts->client->irq;

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	I("%s: Entering!\n", __func__);
	if (enable == 1 && atomic_read(&ts->irq_state) == 0) {
		atomic_set(&ts->irq_state, 1);
		enable_irq(irqnum);
		ts->irq_enabled = 1;
	} else if (enable == 0 && atomic_read(&ts->irq_state) == 1) {
		atomic_set(&ts->irq_state, 0);
		disable_irq_nosync(irqnum);
		ts->irq_enabled = 0;
	}

	I("enable = %d\n", enable);
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}
EXPORT_SYMBOL(himax_int_enable);

#if defined(HX_RST_PIN_FUNC)
void himax_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}
EXPORT_SYMBOL(himax_gpio_set);
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

int himax_gpio_power_config(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata)
{
	int error = 0;
	struct i2c_client *client = ts->client;
#if defined(HX_RST_PIN_FUNC)

	if (pdata->gpio_reset >= 0) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");

		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			goto err_gpio_reset_req;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_reset);
			goto err_gpio_reset_dir;
		}
	}

#endif

	if (pdata->lcm_rst >= 0) {
		error = gpio_request(pdata->lcm_rst, "lcm-reset");

		if (error < 0) {
			E("%s: request lcm-reset pin failed\n", __func__);
			goto err_lcm_rst_req;
		}

		error = gpio_direction_output(pdata->lcm_rst, 0);
		if (error) {
			E("unable to set direction for lcm_rst [%d]\n", pdata->lcm_rst);
			goto err_lcm_rst_dir;
		}
	}

	if (gpio_is_valid(pdata->gpio_pon)) {
		error = gpio_request(pdata->gpio_pon, "hmx_pon_gpio");

		if (error) {
			E("unable to request scl gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_req;
		}

		error = gpio_direction_output(pdata->gpio_pon, 0);

		I("gpio_pon LOW [%d]\n", pdata->gpio_pon);

		if (error) {
			E("unable to set direction for pon gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_dir;
		}
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_irq);
			goto err_gpio_irq_req;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_irq);
			goto err_gpio_irq_set_input;
		}

		client->irq = gpio_to_irq(pdata->gpio_irq);
		ts->hx_irq = client->irq;
	} else {
		E("irq gpio not provided\n");
		goto err_gpio_irq_req;
	}

	if (gpio_is_valid(pdata->fail_det)) {
		/* configure touchscreen fail_det gpio */
		error = gpio_request(pdata->fail_det, "himax_fail_det");

		if (error)
			E("unable to request gpio [%d]\n", pdata->fail_det);

		error = gpio_direction_input(pdata->fail_det);

		if (error)
			E("unable to set direction for gpio [%d]\n", pdata->fail_det);

		ts->hx_fail_det = gpio_to_irq(pdata->fail_det);

	} else {
		I("fail_det not provided\n");
	}

	msleep(10);

	if (pdata->lcm_rst >= 0) {
		error = gpio_direction_output(pdata->lcm_rst, 1);

		if (error) {
			E("lcm_rst unable to set direction for gpio [%d]\n", pdata->lcm_rst);
			goto err_lcm_reset_set_high;
		}
	}
	msleep(10);

#if defined(HX_RST_PIN_FUNC)

	if (pdata->gpio_reset >= 0) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_reset);
			goto err_gpio_reset_set_high;
		}
	}
#endif

	msleep(100);

	if (gpio_is_valid(pdata->gpio_pon)) {
		error = gpio_direction_output(pdata->gpio_pon, 1);

		I("gpio_pon HIGH [%d]\n", pdata->gpio_pon);

		if (error) {
			E("gpio_pon unable to set direction for gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_set_high;
		}
	}

	return error;

err_gpio_pon_set_high:
#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_set_high:
#endif
err_lcm_reset_set_high:
err_gpio_irq_set_input:
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);
err_gpio_irq_req:
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en);
err_gpio_pon_dir:
	if (gpio_is_valid(pdata->gpio_pon))
		gpio_free(pdata->gpio_pon);
err_gpio_pon_req:
err_lcm_rst_dir:
	if (gpio_is_valid(pdata->lcm_rst))
		gpio_free(pdata->lcm_rst);
err_lcm_rst_req:

#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_dir:
	if (pdata->gpio_reset >= 0)
		gpio_free(pdata->gpio_reset);
err_gpio_reset_req:
#endif
	return error;
}

void himax_gpio_power_deconfig(struct himax_i2c_platform_data *pdata)
{
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

#if defined(HX_RST_PIN_FUNC)
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
#endif
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en);

	if (gpio_is_valid(pdata->gpio_pon))
		gpio_free(pdata->gpio_pon);
}

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

static irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	himax_ts_isr_func((struct himax_ts_data *)ptr);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work);

	himax_ts_work(ts);
}

static irqreturn_t himax_fail_det_thread(int irq, void *ptr)
{
	struct himax_ts_data *ts = (struct himax_ts_data *)ptr;

	himax_fail_det_work(ts);

	return IRQ_HANDLED;
}

static int himax_int_register_trigger(struct himax_ts_data *ts)
{
	int ret = 0;
	struct i2c_client *client = ts->client;

	if (ts->ic_data->HX_INT_IS_EDGE) {
		I("%s edge triiger falling\n ", __func__);
		ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, ts);
	} else {
		I("%s level trigger low\n ", __func__);
		ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
					   IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts);
	}

	return ret;
}

static int himax_fail_det_int_register_trigger(struct himax_ts_data *ts)
{
	int ret = 0;

	char *hx_fail_det_name = "hx_fail_det";

	I("%s level trigger high\n ", __func__);
	ret = request_threaded_irq(ts->hx_fail_det, NULL, himax_fail_det_thread,
				   IRQF_TRIGGER_RISING | IRQF_ONESHOT, hx_fail_det_name,
				   (void *)ts);

	return ret;
}

int himax_int_en_set(struct himax_ts_data *ts)
{
	int ret = NO_ERR;

	ret = himax_int_register_trigger(ts);
	return ret;
}

int himax_fail_det_register_interrupt(struct himax_ts_data *ts)
{
	int ret = 0;

	if (ts->hx_fail_det) { /*INT mode*/
		ret = himax_fail_det_int_register_trigger(ts);

		if (ret == 0)
			I("%s: irq enabled at gpio: %d\n", __func__, ts->hx_fail_det);
		else
			E("%s: request_irq failed\n", __func__);

	} else {
		I("%s: hx_fail_det is empty.\n", __func__);
	}

	return ret;
}

int himax_ts_register_interrupt(struct himax_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	int ret = 0;

	ts->irq_enabled = 0;

	/* Work functon */
	if (client->irq && ts->hx_irq) { /*INT mode*/
		ts->use_irq = 1;
		ret = himax_int_register_trigger(ts);

		if (ret == 0) {
			ts->irq_enabled = 1;
			atomic_set(&ts->irq_state, 1);
			I("%s: irq enabled at gpio: %d\n", __func__, client->irq);
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else {
		I("%s: client->irq is empty, use polling mode.\n", __func__);
	}

	/*if use polling mode need to disable HX_ESD_RECOVERY function*/
	if (!ts->use_irq) {
		ts->himax_wq = create_singlethread_workqueue("himax_touch");
		INIT_WORK(&ts->work, himax_ts_work_func);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}

	return ret;
}

int himax_ts_unregister_interrupt(struct himax_ts_data *ts)
{
	int ret = 0;

	I("%s: entered.\n", __func__);

	/* Work functon */
	if (ts->hx_irq && ts->use_irq) { /*INT mode*/

		free_irq(ts->hx_irq, ts);
		I("%s: irq disabled at qpio: %d\n", __func__, ts->hx_irq);
	}

	/*if use polling mode need to disable HX_ESD_RECOVERY function*/
	if (!ts->use_irq) {
		hrtimer_cancel(&ts->timer);
		cancel_work_sync(&ts->work);
		if (ts->himax_wq != NULL)
			destroy_workqueue(ts->himax_wq);
		I("%s: polling mode destroyed", __func__);
	}

	return ret;
}

static int himax_common_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);
	struct pinctrl *p;
	struct pinctrl_state *suspend_state;

	I("%s: enter\n", __func__);
#if defined(HX_CONFIG_DRM) && !defined(HX_CONFIG_FB)
	if (!ts->initialized)
		return -ECANCELED;
#endif
	himax_chip_common_suspend(ts);
	if (gpio_is_valid(ts->pdata->gpio_reset))
		gpio_direction_output(ts->pdata->gpio_reset, 0);
	I("%s: set tp_rst 0\n", __func__);
	p = devm_pinctrl_get(&ts->client->dev);
	suspend_state = pinctrl_lookup_state(p, "sleep");
	pinctrl_select_state(p, suspend_state);
	I("%s: suspend mode pinctrl!\n", __func__);
	return 0;
}

static int himax_common_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);
	struct pinctrl *p;
	struct pinctrl_state *default_state;

	I("%s: enter\n", __func__);
#if defined(HX_CONFIG_DRM) && !defined(HX_CONFIG_FB)
	/*
	 *	wait until device resume for TDDI
	 *	TDDI: Touch and display Driver IC
	 */
	if (!ts->initialized)
		if (himax_chip_common_init(ts))
			return -ECANCELED;
#endif
	himax_chip_common_resume(ts);
	p = devm_pinctrl_get(&ts->client->dev);
	default_state = pinctrl_lookup_state(p, "default");
	pinctrl_select_state(p, default_state);
	I("%s: active pinctrl!\n", __func__);
	return 0;
}

#if defined(HX_CONFIG_FB)
int himax_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts = container_of(self, struct himax_ts_data, fb_notif);

	I(" %s\n", __func__);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts->client) {
		blank = evdata->data;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
			himax_common_resume(&ts->client->dev);
			break;
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			himax_common_suspend(&ts->client->dev);
			break;
		}
	}

	return 0;
}
#elif defined(HX_CONFIG_DRM)
int himax_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct himax_ts_data *ts = container_of(self, struct himax_ts_data, fb_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	D("DRM  %s\n", __func__);

	if (evdata->data && event == MSM_DRM_EARLY_EVENT_BLANK && ts->client) {
		blank = evdata->data;
		switch (*blank) {
		case MSM_DRM_BLANK_POWERDOWN:
			if (!ts->initialized)
				return -ECANCELED;
			himax_common_suspend(&ts->client->dev);
			break;
		}
	}

	if (evdata->data && event == MSM_DRM_EVENT_BLANK && ts->client) {
		blank = evdata->data;
		switch (*blank) {
		case MSM_DRM_BLANK_UNBLANK:
			himax_common_resume(&ts->client->dev);
			ts->touch_num = 50;
			break;
		}
	}

	return 0;
}
#endif

static int himax_2nd_probe(struct himax_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	int i, ret = 0;

	if (ts->probe_flag) {
		pr_err("[HiMax_Touch][ERROR] have initialed already!!\n\n");
		return 0;
	}

	for (i = 0; i < 3; i++) {
		if (i2c_smbus_read_byte_data(client, 0x40) < 0) {
			E("can't read ic data!\n");
			msleep(100);
		} else {
			break;
		}
	}

	ret = himax_chip_common_init(ts);
	if (ret < 0) {
		E("%s: ret=%d\n", __func__, ret);
		return ret;
	}

	ts->probe_flag = true;

	return ret;
}

/*static void himax_probe_work(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, probe_work);

	himax_2nd_probe(ts);
}

static void himax_connected_work(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data,
						connected_work);

	himax_2nd_probe(ts);
	if (ts->probe_flag)
		himax_common_resume(&ts->client->dev);
	ts->touch_num = 50;
}

static void himax_disconnected_work(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data,
						disconnected_work);

	if (ts->probe_flag)
		himax_common_suspend(&ts->client->dev);
}

int himax_hotplug_notifier(struct notifier_block *nb,
			   unsigned long event, void *data)
{
	struct himax_ts_data *ts = container_of(nb, struct himax_ts_data,
						cable_hotplug_nb);

	I("%s:Enter\n", __func__);

	if (extcon_get_state(ts->edev, EXTCON_JACK_VIDEO_OUT)) {
		cancel_work_sync(&ts->connected_work);
		schedule_work(&ts->connected_work);
	} else {
		cancel_work_sync(&ts->disconnected_work);
		schedule_work(&ts->disconnected_work);
	}

	return 0;
}

int tp_diag_himax(void)
{
	int flag = 1;
	// int flag = 0;

	// if(tp_client) {
	// 	if( i2c_smbus_read_byte_data(tp_client, 0x10) < 0) {
	// 		E("[DIAG] himax i2c bus connection error\n");
	// 	} else {
	// 		flag = 1;
	// 		I("[DIAG]PASS!\n");
	// 	}
	// 	return flag;
	// } else {
	// 	E("[DIAG]himax probe error\n");
	// }
	return flag;
}
EXPORT_SYMBOL(tp_diag_himax);
*/

static int himax_chip_common_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	//struct device_node *node = client->dev.of_node;
	int ret = 0;
	struct himax_ts_data *ts;

	I("%s:Enter\n", __func__);

	/* Check I2C functionality */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s: i2c check functionality error\n", __func__);
		return -ENODEV;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	i2c_set_clientdata(client, ts);
	ts->client = client;
	ts->dev = &client->dev;
	mutex_init(&ts->rw_lock);

	/* FIXME */
	INIT_LIST_HEAD(&ts->chips);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83192)
	list_add_tail(&hx83192_chip.list, &ts->chips);
#endif

	ts->rw_buf = kcalloc(BUS_RW_MAX_LEN, sizeof(uint8_t), GFP_KERNEL);
	if (!ts->rw_buf) {
		E("Allocate I2C RW Buffer failed\n");
		ret = -ENODEV;
		goto err_alloc_rw_buf_failed;
	}

	ts->initialized = false;
	ts->touch_num = 50;

	himax_2nd_probe(ts);

	/*ts->edev = extcon_find_edev_by_node(node->parent->parent);
	if (IS_ERR(ts->edev)) {
		if (PTR_ERR(ts->edev) != -EPROBE_DEFER)
			dev_err(ts->dev, "Invalid or missing extcon\n");
		return PTR_ERR(ts->edev);
	}

	INIT_WORK(&ts->connected_work, himax_connected_work);
	INIT_WORK(&ts->disconnected_work, himax_disconnected_work);

	ts->cable_hotplug_nb.notifier_call = himax_hotplug_notifier;
	ret = extcon_register_notifier(ts->edev, EXTCON_JACK_VIDEO_OUT,
					       &ts->cable_hotplug_nb);
	if (ret < 0) {
		dev_err(ts->dev, "failed to register notifier for SDP\n");
		return ret;
	}

	ts->probe_flag = false;
	ret = extcon_get_state(ts->edev, EXTCON_JACK_VIDEO_OUT);
	if (ret) {
		I("[HiMax_Touch]%s:touch is connected\n",__func__);
		INIT_WORK(&ts->probe_work, himax_probe_work);
		schedule_work(&ts->probe_work);
	}*/

	// tp_client = client;
	return 0;

err_alloc_rw_buf_failed:
	kfree(ts);
err_alloc_data_failed:

	return ret;
}

static void himax_chip_common_remove(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);

	if (ts->hx_chip_inited)
		himax_chip_common_deinit(ts);

	/* FIXME */
	// himax_hx83192_remove();
}

static const struct i2c_device_id himax_common_ts_id[] = {
	{ HIMAX_common_NAME, 0 },
	{}
};

static const struct dev_pm_ops himax_common_pm_ops = {
#if (!defined(HX_CONFIG_FB)) && (!defined(HX_CONFIG_DRM))
	.suspend = himax_common_suspend,
	.resume = himax_common_resume,
#endif
};

#if defined(CONFIG_OF)
static const struct of_device_id himax_match_table[] = {
	{ .compatible = "himax,hxcommon" },
	{},
};
#else
#define himax_match_table NULL
#endif

static struct i2c_driver himax_common_driver = {
	.id_table = himax_common_ts_id,
	.probe = himax_chip_common_probe,
	.remove = himax_chip_common_remove,
	.driver = {
		.name = HIMAX_common_NAME,
		.owner = THIS_MODULE,
		.of_match_table = himax_match_table,
#if defined(CONFIG_PM)
		.pm = &himax_common_pm_ops,
#endif
	},
};

static int __init himax_common_init(void)
{
	I("Himax common touch panel driver init\n");
	D("Himax check double loading\n");
	if (mmi_refcnt++ > 0) {
		I("Himax driver has been loaded! ignoring....\n");

		return 0;
	}
	i2c_add_driver(&himax_common_driver);

	return 0;
}

static void __exit himax_common_exit(void)
{
	i2c_del_driver(&himax_common_driver);
}

late_initcall(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");
