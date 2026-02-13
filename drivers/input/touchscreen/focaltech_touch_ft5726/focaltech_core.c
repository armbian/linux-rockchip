/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
 *
 * File Name: focaltech_core.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-08-08
 *
 * Abstract: entrance for focaltech ts driver
 *
 * Version: V1.0
 *
 *****************************************************************************/

/*****************************************************************************
 * Included header files
 *****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_DRM)
#if defined(CONFIG_DRM_PANEL)
#include <drm/drm_panel.h>
#else
#include <linux/msm_drm_notify.h>
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif
#include "focaltech_core.h"

#include <linux/suspend.h>
#include <linux/pm_wakeup.h>

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/
#define FTS_DRIVER_NAME						"focaltech,ft5726"
#define FTS_DRIVER_PEN_NAME					"ft_ts,pen"
#define INTERVAL_READ_REG					200  /* unit:ms */
#define TIMEOUT_READ_REG					1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV						2800000
#define FTS_VTG_MAX_UV						3300000
#define FTS_I2C_VTG_MIN_UV					1800000
#define FTS_I2C_VTG_MAX_UV					1800000
#endif

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/
struct fts_ts_data *fts_data;

/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);

int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h)
{
	int i = 0;
	struct ft_chip_id_t *cid = &ts_data->ic_info.cid;
	u8 cid_h = 0x0;

	if (cid->type == 0)
		return -ENODATA;

	for (i = 0; i < FTS_MAX_CHIP_IDS; i++) {
		cid_h = ((cid->chip_ids[i] >> 8) & 0x00FF);
		if (cid_h && (id_h == cid_h))
			return 0;
	}

	return -ENODATA;
}

/*****************************************************************************
 *  Name: fts_wait_tp_to_valid
 *  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
 *         need call when reset/power on/resume...
 *  Input:
 *  Output:
 *  Return: return 0 if tp valid, otherwise return error code
 *****************************************************************************/
int fts_wait_tp_to_valid(void)
{
	int ret = 0;
	int cnt = 0;
	u8 idh = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 chip_idh = ts_data->ic_info.ids.chip_idh;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
		if ((idh == chip_idh) || (fts_check_cid(ts_data, idh) == 0)) {
			FTS_INFO("TP Ready,Device ID:0x%02x", idh);
			return 0;
		} else {
			FTS_DEBUG("TP Not Ready,ReadData:0x%02x,ret:%d", idh, ret);
		}

		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	return -EIO;
}

/*****************************************************************************
 *  Name: fts_tp_state_recovery
 *  Brief: Need execute this function when reset
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	/* wait tp stable */
	fts_wait_tp_to_valid();
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(ts_data);
	/* recover TP gesture state 0xD0 */
	fts_gesture_recovery(ts_data);
	FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
	FTS_DEBUG("tp reset");
	gpiod_set_value(fts_data->pdata->reset_gpio, 0);
	msleep(10);
	gpiod_set_value(fts_data->pdata->reset_gpio, 1);
	if (hdelayms)
		msleep(hdelayms);

	return 0;
}

void fts_irq_disable(void)
{
	unsigned long irqflags;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (!fts_data->irq_disabled) {
		disable_irq_nosync(fts_data->irq);
		fts_data->irq_disabled = true;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_irq_enable(void)
{
	unsigned long irqflags = 0;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (fts_data->irq_disabled) {
		enable_irq(fts_data->irq);
		fts_data->irq_disabled = false;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_hid2std(void)
{
	int ret = 0;
	u8 buf[3] = {0xEB, 0xAA, 0x09};

	if (fts_data->bus_type != BUS_TYPE_I2C)
		return;

	ret = fts_write(buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd write fail");
	} else {
		msleep(10);
		buf[0] = buf[1] = buf[2] = 0;
		ret = fts_read(NULL, 0, buf, 3);
		if (ret < 0)
			FTS_ERROR("hid2std cmd read fail");
		else if ((0xEB == buf[0]) && (0xAA == buf[1]) && (0x08 == buf[2]))
			FTS_DEBUG("hidi2c change to stdi2c successful");
		else
			FTS_DEBUG("hidi2c change to stdi2c not support or fail");
	}
}

static int fts_match_cid(struct fts_ts_data *ts_data, u16 type, u8 id_h, u8 id_l, bool force)
{
#ifdef FTS_CHIP_ID_MAPPING
	u32 i = 0;
	u32 j = 0;
	struct ft_chip_id_t chip_id_list[] = FTS_CHIP_ID_MAPPING;
	u32 cid_entries = sizeof(chip_id_list) / sizeof(struct ft_chip_id_t);
	u16 id = (id_h << 8) + id_l;

	memset(&ts_data->ic_info.cid, 0, sizeof(struct ft_chip_id_t));
	for (i = 0; i < cid_entries; i++) {
		if (!force && (type == chip_id_list[i].type)) {
			break;
		} else if (force && (type == chip_id_list[i].type)) {
			FTS_INFO("match cid,type:0x%x", (int)chip_id_list[i].type);
			ts_data->ic_info.cid = chip_id_list[i];
			return 0;
		}
	}

	if (i >= cid_entries)
		return -ENODATA;

	for (j = 0; j < FTS_MAX_CHIP_IDS; j++) {
		if (id == chip_id_list[i].chip_ids[j]) {
			FTS_DEBUG("cid:%x==%x", id, chip_id_list[i].chip_ids[j]);
			FTS_INFO("match cid,type:0x%x", (int)chip_id_list[i].type);
			ts_data->ic_info.cid = chip_id_list[i];
			return 0;
		}
	}

	return -ENODATA;
#else
	return -EINVAL;
#endif
}

static int fts_get_chip_types(struct fts_ts_data *ts_data, u8 id_h, u8 id_l, bool fw_valid)
{
	u32 i = 0;
	struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
	u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

	if ((0x0 == id_h) || (0x0 == id_l)) {
		FTS_ERROR("id_h/id_l is 0");
		return -EINVAL;
	}

	FTS_INFO("verify id:0x%02x%02x", id_h, id_l);
	for (i = 0; i < ctype_entries; i++) {
		if (VALID == fw_valid) {
			if (((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl)) || (!fts_match_cid(ts_data, ctype[i].type, id_h, id_l, 0)))
				break;
		} else {
			if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl)) || ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl)) || ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl)))
				break;
		}
	}

	if (i >= ctype_entries)
		return -ENODATA;

	fts_match_cid(ts_data, ctype[i].type, id_h, id_l, 1);
	ts_data->ic_info.ids = ctype[i];
	return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
	int ret = 0;
	u8 chip_id[2] = {0};
	u8 id_cmd[4] = {0};
	u32 id_cmd_len = 0;

	id_cmd[0] = FTS_CMD_START1;
	id_cmd[1] = FTS_CMD_START2;
	ret = fts_write(id_cmd, 2);
	if (ret < 0) {
		FTS_ERROR("start cmd write fail");
		return ret;
	}

	msleep(FTS_CMD_START_DELAY);
	id_cmd[0] = FTS_CMD_READ_ID;
	id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
	if (ts_data->ic_info.is_incell)
		id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
	else
		id_cmd_len = FTS_CMD_READ_ID_LEN;
	ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
	if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
		FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
		return -EIO;
	}

	id[0] = chip_id[0];
	id[1] = chip_id[1];
	return 0;
}

/*****************************************************************************
 * Name: fts_get_ic_information
 * Brief: read chip id to get ic information, after run the function, driver w-
 *        ill know which IC is it.
 *        If cant get the ic information, maybe not focaltech's touch IC, need
 *        unregister the driver
 * Input:
 * Output:
 * Return: return 0 if get correct ic information, otherwise return error code
 *****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 ver = 0;
	u8 chip_id[2] = {0};

	ts_data->ic_info.is_incell = FTS_CHIP_IDC;
	ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
		ret |= fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_DEBUG("chip id read invalid, read:0x%02x%02x", chip_id[0], chip_id[1]);
		} else {
			ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
			if (!ret)
				break;
			else
				FTS_DEBUG("TP not ready, read:0x%02x%02x", chip_id[0], chip_id[1]);
		}

		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
		FTS_INFO("fw is invalid, need read boot id");
		if (ts_data->ic_info.hid_supported)
			fts_hid2std();

		ret = fts_read_bootid(ts_data, &chip_id[0]);
		if (ret < 0) {
			FTS_ERROR("read boot id fail");
			return ret;
		}

		ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
		if (ret < 0) {
			FTS_ERROR("can't get ic informaton");
			return ret;
		}
	}

	ret = fts_read_reg(FTS_REG_FW_VER, &ver);
	if (ret < 0) {
		FTS_ERROR("read fw ver from tp fail");
		return ret;
	}
	ts_data->input_dev->id.version = ver;

	FTS_INFO("get ic information, chip id = 0x%02x%02x(cid type=0x%x) ver = %d", ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl, ts_data->ic_info.cid.type, ver);

	return 0;
}

/*****************************************************************************
 *  Reprot related
 *****************************************************************************/
static void fts_show_touch_buffer(u8 *data, u32 datalen)
{
	u32 i = 0;
	u32 count = 0;
	char *tmpbuf = NULL;

	tmpbuf = kzalloc(1024, GFP_KERNEL);
	if (!tmpbuf) {
		FTS_ERROR("tmpbuf zalloc fail");
		return;
	}

	for (i = 0; i < datalen; i++) {
		count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
		if (count >= 1024)
			break;
	}
	FTS_DEBUG("touch_buf:%s", tmpbuf);

	kfree(tmpbuf);
	tmpbuf = NULL;
}

void fts_release_all_finger(void)
{
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
	u32 finger_count = 0;
	u32 max_touches = ts_data->pdata->max_touch_number;
#endif

	mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
	for (finger_count = 0; finger_count < max_touches; finger_count++) {
		input_mt_slot(input_dev, finger_count);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
#else
	input_mt_sync(input_dev);
#endif
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_sync(input_dev);

#if FTS_PEN_EN
	input_report_key(ts_data->pen_dev, BTN_TOOL_PEN, 0);
	input_report_key(ts_data->pen_dev, BTN_TOUCH, 0);
	input_sync(ts_data->pen_dev);
#endif

	ts_data->touch_points = 0;
	ts_data->key_state = 0;
	mutex_unlock(&ts_data->report_mutex);
}

/*****************************************************************************
 * Name: fts_input_report_key
 * Brief: process key events,need report key-event if key enable.
 *        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
 *        need report it to key event.
 *        x_dim: parse from dts, means key x_coordinate, dimension:+-50
 *        y_dim: parse from dts, means key y_coordinate, dimension:+-50
 * Input:
 * Output:
 * Return: return 0 if it's key event, otherwise return error code
 *****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *ts_data, struct ts_event *kevent)
{
	int i = 0;
	int x = kevent->x;
	int y = kevent->y;
	int *x_dim = &ts_data->pdata->key_x_coords[0];
	int *y_dim = &ts_data->pdata->key_y_coords[0];

	if (!ts_data->pdata->have_key)
		return -EINVAL;
	for (i = 0; i < ts_data->pdata->key_number; i++) {
		if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
			(y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
			if (EVENT_DOWN(kevent->flag) && !(ts_data->key_state & (1 << i))) {
				input_report_key(ts_data->input_dev, ts_data->pdata->keys[i], 1);
				ts_data->key_state |= (1 << i);
				FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
			} else if (EVENT_UP(kevent->flag) && (ts_data->key_state & (1 << i))) {
				input_report_key(ts_data->input_dev, ts_data->pdata->keys[i], 0);
				ts_data->key_state &= ~(1 << i);
				FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
			}
			return 0;
		}
	}
	return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *ts_data, struct ts_event *events)
{
	int i = 0;
	int touch_down_point_cur = 0;
	int touch_point_pre = ts_data->touch_points;
	u32 max_touch_num = ts_data->pdata->max_touch_number;
	bool touch_event_coordinate = false;
	struct input_dev *input_dev = ts_data->input_dev;
	int x, y;

	for (i = 0; i < ts_data->touch_event_num; i++) {
		if (fts_input_report_key(ts_data, &events[i]) == 0)
			continue;

		touch_event_coordinate = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_mt_slot(input_dev, events[i].id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
#if FTS_REPORT_PRESSURE_EN
			input_report_abs(input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			x = events[i].x;
			y = events[i].y;
			if (ts_data->pdata->exchange_x_y_flag) {
				int t = x;
				x = y;
				y = t;
			}
			if (ts_data->pdata->revert_x_flag)
				x = ts_data->pdata->x_max - x;
			if (ts_data->pdata->revert_y_flag)
				y = ts_data->pdata->y_max - y;
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
			input_report_abs(input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, y);

			touch_down_point_cur |= (1 << events[i].id);
			touch_point_pre |= (1 << events[i].id);

			if ((ts_data->log_level >= 2) ||
				((1 == ts_data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
				FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x, events[i].y, events[i].p, events[i].area);
			}
		} else {
			input_mt_slot(input_dev, events[i].id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
			touch_point_pre &= ~(1 << events[i].id);
			if (ts_data->log_level >= 1)
				FTS_DEBUG("[B]P%d UP!", events[i].id);
		}
	}

	if (unlikely(touch_point_pre ^ touch_down_point_cur)) {
		for (i = 0; i < max_touch_num; i++) {
			if ((1 << i) & (touch_point_pre ^ touch_down_point_cur)) {
				if (ts_data->log_level >= 1)
					FTS_DEBUG("[B]P%d UP!", i);
				input_mt_slot(input_dev, i);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
			}
		}
	}

	if (touch_down_point_cur) {
		input_report_key(input_dev, BTN_TOUCH, 1);
	} else if (touch_event_coordinate || ts_data->touch_points) {
		if (ts_data->touch_points && (ts_data->log_level >= 1))
			FTS_DEBUG("[B]Points All Up!");
		input_report_key(input_dev, BTN_TOUCH, 0);
	}

	ts_data->touch_points = touch_down_point_cur;
	input_sync(input_dev);
	return 0;
}
#else
static int fts_input_report_a(struct fts_ts_data *ts_data, struct ts_event *events)
{
	int i = 0;
	int touch_down_point_num_cur = 0;
	bool touch_event_coordinate = false;
	struct input_dev *input_dev = ts_data->input_dev;

	for (i = 0; i < ts_data->touch_event_num; i++) {
		if (fts_input_report_key(ts_data, &events[i]) == 0)
			continue;

		touch_event_coordinate = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_report_abs(input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
			input_report_abs(input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
			input_report_abs(input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, events[i].y);
			input_mt_sync(input_dev);

			touch_down_point_num_cur++;
			if ((ts_data->log_level >= 2) ||
				((1 == ts_data->log_level) && (FTS_TOUCH_DOWN == events[i].flag)))
				FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x, events[i].y, events[i].p, events[i].area);
		}
	}

	if (touch_down_point_num_cur) {
		input_report_key(input_dev, BTN_TOUCH, 1);
	} else if (touch_event_coordinate || ts_data->touch_points) {
		if (ts_data->touch_points && (ts_data->log_level >= 1))
			FTS_DEBUG("[A]Points All Up!");
		input_report_key(input_dev, BTN_TOUCH, 0);
		input_mt_sync(input_dev);
	}

	ts_data->touch_points = touch_down_point_num_cur;
	input_sync(input_dev);
	return 0;
}
#endif

#if FTS_PEN_EN
static int fts_input_pen_report(struct fts_ts_data *ts_data, u8 *pen_buf)
{
	struct input_dev *pen_dev = ts_data->pen_dev;
	struct pen_event *pevt = &ts_data->pevent;

	/*get information of stylus*/
	pevt->inrange = (pen_buf[2] & 0x20) ? 1 : 0;
	pevt->tip = (pen_buf[2] & 0x01) ? 1 : 0;
	pevt->flag = pen_buf[3] >> 6;
	pevt->id = pen_buf[5] >> 4;
	pevt->x = ((pen_buf[3] & 0x0F) << 8) + pen_buf[4];
	pevt->y = ((pen_buf[5] & 0x0F) << 8) + pen_buf[6];
	pevt->p = ((pen_buf[7] & 0x0F) << 8) + pen_buf[8];
	pevt->tilt_x = (short)((pen_buf[9] << 8) + pen_buf[10]);
	pevt->tilt_y = (short)((pen_buf[11] << 8) + pen_buf[12]);
	pevt->azimuth = ((pen_buf[13] << 8) + pen_buf[14]);
	pevt->tool_type = BTN_TOOL_PEN;

	input_report_key(pen_dev, BTN_STYLUS, !!(pen_buf[2] & 0x02));
	input_report_key(pen_dev, BTN_STYLUS2, !!(pen_buf[2] & 0x08));

	switch (ts_data->pen_etype) {
	case STYLUS_DEFAULT:
		if (pevt->tip && pevt->p) {
			if ((ts_data->log_level >= 2) || (!pevt->down))
				FTS_DEBUG("[PEN]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d DOWN", pevt->x, pevt->y, pevt->p, pevt->tip, pevt->flag, pevt->tilt_x, pevt->tilt_y);
			input_report_abs(pen_dev, ABS_X, pevt->x);
			input_report_abs(pen_dev, ABS_Y, pevt->y);
			input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
			input_report_abs(pen_dev, ABS_TILT_X, pevt->tilt_x);
			input_report_abs(pen_dev, ABS_TILT_Y, pevt->tilt_y);
			input_report_key(pen_dev, BTN_TOUCH, 1);
			input_report_key(pen_dev, BTN_TOOL_PEN, 1);
			pevt->down = 1;
		} else if (!pevt->tip && pevt->down) {
			FTS_DEBUG("[PEN]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d UP", pevt->x, pevt->y, pevt->p, pevt->tip, pevt->flag, pevt->tilt_x, pevt->tilt_y);
			input_report_abs(pen_dev, ABS_X, pevt->x);
			input_report_abs(pen_dev, ABS_Y, pevt->y);
			input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
			input_report_key(pen_dev, BTN_TOUCH, 0);
			input_report_key(pen_dev, BTN_TOOL_PEN, 0);
			pevt->down = 0;
		}
		input_sync(pen_dev);
		break;
	case STYLUS_HOVER:
		if (ts_data->log_level >= 1)
			FTS_DEBUG("[PEN][%02X]x:%d,y:%d,p:%d,tip:%d,flag:%d,tilt:%d,%d,%d", pen_buf[2], pevt->x, pevt->y, pevt->p, pevt->tip, pevt->flag, pevt->tilt_x, pevt->tilt_y, pevt->azimuth);
		input_report_abs(pen_dev, ABS_X, pevt->x);
		input_report_abs(pen_dev, ABS_Y, pevt->y);
		input_report_abs(pen_dev, ABS_Z, pevt->azimuth);
		input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);
		input_report_abs(pen_dev, ABS_TILT_X, pevt->tilt_x);
		input_report_abs(pen_dev, ABS_TILT_Y, pevt->tilt_y);
		input_report_key(pen_dev, BTN_TOOL_PEN, EVENT_DOWN(pevt->flag));
		input_report_key(pen_dev, BTN_TOUCH, pevt->tip);
		input_sync(pen_dev);
		break;
	default:
		FTS_ERROR("Unknown stylus event");
		break;
	}

	return 0;
}
#endif

static int fts_read_touchdata(struct fts_ts_data *ts_data, u8 *buf)
{
	int ret = 0;
	u32 touch_max_size = 0;
	u32 max_touch_num = ts_data->pdata->max_touch_number;
	u8 event = 0xFF;

	ts_data->touch_addr = 0x01;
	ret = fts_read(&ts_data->touch_addr, 1, buf, ts_data->touch_size);
	if (ret < 0) {
		FTS_ERROR("read touchdata fails,ret:%d", ret);
		return ret;
	}

	event = (buf[FTS_TOUCH_E_NUM] >> 4) & 0x0F;
	if (event == TOUCH_DEFAULT) {
		if (buf[ts_data->touch_size - 1] != 0xFF)
			touch_max_size = max_touch_num * FTS_ONE_TCH_LEN + 2;
	} else if (event == TOUCH_EVENT_NUM) {
		touch_max_size = (buf[FTS_TOUCH_E_NUM] & 0x0F) * FTS_ONE_TCH_LEN + 2;
	}
#if FTS_PEN_EN
	else if (event == TOUCH_PEN) {
		touch_max_size = FTS_SIZE_PEN;
		if (touch_max_size > ts_data->touch_size)
			FTS_INFO("read next touch message of pen,size:%d-%d", touch_max_size, ts_data->touch_size);
	}
#endif
	else if (event == TOUCH_EXTRA_MSG) {
		touch_max_size = (buf[FTS_TOUCH_E_NUM] & 0x0F) * FTS_ONE_TCH_LEN + 4 + ((buf[2] << 8) + buf[3]);
		if (touch_max_size > FTS_MAX_TOUCH_BUF)
			touch_max_size = FTS_MAX_TOUCH_BUF;
	}

	if (touch_max_size > ts_data->touch_size) {
		ts_data->ta_size = touch_max_size;
		ts_data->touch_addr += ts_data->touch_size;
		ret = fts_read(&ts_data->touch_addr, 1, buf + ts_data->touch_size, touch_max_size - ts_data->touch_size);
		if (ret < 0) {
			FTS_ERROR("read touchdata2 fails,ret:%d", ret);
			return ret;
		}
	}

	return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *ts_data, u8 *touch_buf)
{
	int ret = 0;
	u8 gesture_en = 0xFF;

	memset(touch_buf, 0xFF, FTS_MAX_TOUCH_BUF);
	ts_data->ta_size = ts_data->touch_size;

	/*read touch data*/
	ret = fts_read_touchdata(ts_data, touch_buf);
	if (ret < 0) {
		FTS_ERROR("read touch data fails");
		return TOUCH_ERROR;
	}

	if (ts_data->log_level >= 3)
		fts_show_touch_buffer(touch_buf, ts_data->ta_size);

	if (ret)
		return TOUCH_IGNORE;


	/*gesture*/
	if (ts_data->suspended && ts_data->gesture_support) {
		ret = fts_read_reg(FTS_REG_GESTURE_EN, &gesture_en);
		if ((ret >= 0) && (gesture_en == ENABLE))
			return TOUCH_GESTURE;
		else
			FTS_DEBUG("gesture not enable in fw, don't process gesture");
	}

	if ((touch_buf[1] == 0xFF) && (touch_buf[2] == 0xFF) && (touch_buf[3] == 0xFF) && (touch_buf[4] == 0xFF)) {
		FTS_INFO("touch buff is 0xff, need recovery state");
		return TOUCH_FW_INIT;
	}

	return ((touch_buf[FTS_TOUCH_E_NUM] >> 4) & 0x0F);
}

static int fts_irq_read_report(struct fts_ts_data *ts_data)
{
	int i = 0;
	int max_touch_num = ts_data->pdata->max_touch_number;
	int touch_etype = 0;
	u8 event_num = 0;
	u8 finger_num = 0;
	u8 pointid = 0;
	u8 base = 0;
	u8 *touch_buf = ts_data->touch_buf;
	struct ts_event *events = ts_data->events;

	touch_etype = fts_read_parse_touchdata(ts_data, touch_buf);
	switch (touch_etype) {
	case TOUCH_DEFAULT:
		finger_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
		if (finger_num > max_touch_num) {
			FTS_ERROR("invalid point_num(%d)", finger_num);
			return -EIO;
		}

		for (i = 0; i < max_touch_num; i++) {
			base = FTS_ONE_TCH_LEN * i + 2;
			pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
			if (pointid >= FTS_MAX_ID) {
				break;
			} else if (pointid >= max_touch_num) {
				FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
				return -EINVAL;
			}

			events[i].id = pointid;
			events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;
			events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 8) + (touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
			events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 8) + (touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
			events[i].p = touch_buf[FTS_TOUCH_OFF_PRE + base];
			events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
			if (events[i].p <= 0)
				events[i].p = 0x3F;
			if (events[i].area <= 0)
				events[i].area = 0x09;

			event_num++;
			if (EVENT_DOWN(events[i].flag) && (finger_num == 0)) {
				FTS_INFO("abnormal touch data from fw");
				return -EIO;
			}
		}

		if (event_num == 0) {
			FTS_INFO("no touch point information(%02x)", touch_buf[2]);
			return -EIO;
		}
		ts_data->touch_event_num = event_num;

		mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
		fts_input_report_b(ts_data, events);
#else
		fts_input_report_a(ts_data, events);
#endif
		mutex_unlock(&ts_data->report_mutex);
		break;

#if FTS_PEN_EN
	case TOUCH_PEN:
		mutex_lock(&ts_data->report_mutex);
		fts_input_pen_report(ts_data, touch_buf);
		mutex_unlock(&ts_data->report_mutex);
		break;
#endif

	case TOUCH_EVENT_NUM:
		event_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
		if (!event_num || (event_num > max_touch_num)) {
			FTS_ERROR("invalid touch event num(%d)", event_num);
			return -EIO;
		}

		ts_data->touch_event_num = event_num;
		for (i = 0; i < event_num; i++) {
			base = FTS_ONE_TCH_LEN * i + 2;
			pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
			if (pointid >= max_touch_num) {
				FTS_ERROR("touch point ID(%d) beyond max_touch_number(%d)", pointid, max_touch_num);
				return -EINVAL;
			}

			events[i].id = pointid;
			events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;
			events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 8) + (touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
			events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 8) + (touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
			events[i].p = touch_buf[FTS_TOUCH_OFF_PRE + base];
			events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
			if (events[i].p <= 0)
				events[i].p = 0x3F;
			if (events[i].area <= 0)
				events[i].area = 0x09;
		}

		mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
		fts_input_report_b(ts_data, events);
#else
		fts_input_report_a(ts_data, events);
#endif
		mutex_unlock(&ts_data->report_mutex);
		break;

	case TOUCH_EXTRA_MSG:
		if (!ts_data->touch_analysis_support) {
			FTS_ERROR("touch_analysis is disabled");
			return -EINVAL;
		}

		event_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
		if (!event_num || (event_num > max_touch_num)) {
			FTS_ERROR("invalid touch event num(%d)", event_num);
			return -EIO;
		}

		ts_data->touch_event_num = event_num;
		for (i = 0; i < event_num; i++) {
			base = FTS_ONE_TCH_LEN * i + 4;
			pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
			if (pointid >= max_touch_num) {
				FTS_ERROR("touch point ID(%d) beyond max_touch_number(%d)", pointid, max_touch_num);
				return -EINVAL;
			}

			events[i].id = pointid;
			events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >> 6;
			events[i].x = ((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F) << 8) + (touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
			events[i].y = ((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F) << 8) + (touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
			events[i].p = touch_buf[FTS_TOUCH_OFF_PRE + base];
			events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
			if (events[i].p <= 0)
				events[i].p = 0x3F;
			if (events[i].area <= 0)
				events[i].area = 0x09;
		}

		mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
		fts_input_report_b(ts_data, events);
#else
		fts_input_report_a(ts_data, events);
#endif
		mutex_unlock(&ts_data->report_mutex);
		break;

	case TOUCH_GESTURE:
		if (0 == fts_gesture_readdata(ts_data, touch_buf))
			FTS_INFO("succuss to get gesture data in irq handler");
		break;

	case TOUCH_FW_INIT:
		fts_release_all_finger();
		fts_tp_state_recovery(ts_data);
		break;

	case TOUCH_IGNORE:
	case TOUCH_ERROR:
		break;

	default:
		FTS_INFO("unknown touch event(%d)", touch_etype);
		break;
	}

	return 0;
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
	struct fts_ts_data *ts_data = fts_data;
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	int ret = 0;

	if ((ts_data->suspended) && (ts_data->pm_suspend)) {
		ret = wait_for_completion_timeout(&ts_data->pm_completion, msecs_to_jiffies(FTS_TIMEOUT_COMERR_PM));
		if (!ret) {
			FTS_ERROR("Bus don't resume from pm(deep),timeout,skip irq");
			return IRQ_HANDLED;
		}
	}
#endif

	ts_data->intr_jiffies = jiffies;
#if FTS_POINT_REPORT_CHECK_EN
	fts_prc_queue_work(ts_data);
#endif
	fts_irq_read_report(ts_data);
	if (ts_data->touch_analysis_support && ts_data->ta_flag) {
		ts_data->ta_flag = 0;
		if (ts_data->ta_buf && ts_data->ta_size)
			memcpy(ts_data->ta_buf, ts_data->touch_buf, ts_data->ta_size);
		wake_up_interruptible(&ts_data->ts_waitqueue);
	}

	return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;

	ts_data->irq = gpiod_to_irq(pdata->irq_gpio);
	pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND;
	FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
	ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler, pdata->irq_gpio_flags, FTS_DRIVER_NAME, ts_data);

	return ret;
}

#if FTS_PEN_EN
static int fts_input_pen_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct input_dev *pen_dev;
	struct fts_ts_platform_data *pdata = ts_data->pdata;

	FTS_FUNC_ENTER();
	pen_dev = input_allocate_device();
	if (!pen_dev) {
		FTS_ERROR("Failed to allocate memory for input_pen device");
		return -ENOMEM;
	}

	pen_dev->dev.parent = ts_data->dev;
	pen_dev->name = FTS_DRIVER_PEN_NAME;
	pen_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	__set_bit(ABS_X, pen_dev->absbit);
	__set_bit(ABS_Y, pen_dev->absbit);
	__set_bit(BTN_STYLUS, pen_dev->keybit);
	__set_bit(BTN_STYLUS2, pen_dev->keybit);
	__set_bit(BTN_TOUCH, pen_dev->keybit);
	__set_bit(BTN_TOOL_PEN, pen_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
	input_set_abs_params(pen_dev, ABS_X, pdata->x_min, pdata->x_max, 0, 0);
	input_set_abs_params(pen_dev, ABS_Y, pdata->y_min, pdata->y_max, 0, 0);
	input_set_abs_params(pen_dev, ABS_PRESSURE, 0, 4096, 0, 0);
	input_set_abs_params(pen_dev, ABS_TILT_X, -9000, 9000, 0, 0);
	input_set_abs_params(pen_dev, ABS_TILT_Y, -9000, 9000, 0, 0);
	input_set_abs_params(pen_dev, ABS_Z, 0, 36000, 0, 0);

	ret = input_register_device(pen_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		input_free_device(pen_dev);
		pen_dev = NULL;
		return ret;
	}

	ts_data->pen_dev = pen_dev;
	ts_data->pen_etype = STYLUS_DEFAULT;
	FTS_FUNC_EXIT();
	return 0;
}
#endif

static int fts_input_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int key_num = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	struct input_dev *input_dev;

	FTS_FUNC_ENTER();
	input_dev = input_allocate_device();
	if (!input_dev) {
		FTS_ERROR("Failed to allocate memory for input device");
		return -ENOMEM;
	}

	/* Init and register Input device */
	input_dev->name = FTS_DRIVER_NAME;
	if (ts_data->bus_type == BUS_TYPE_I2C)
		input_dev->id.bustype = BUS_I2C;
	else
		input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = ts_data->dev;

	input_set_drvdata(input_dev, ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	if (pdata->have_key) {
		FTS_INFO("set key capabilities");
		for (key_num = 0; key_num < pdata->key_number; key_num++)
			input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
	}

#if FTS_MT_PROTOCOL_B_EN
	input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif

	ret = input_register_device(input_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		input_set_drvdata(input_dev, NULL);
		input_free_device(input_dev);
		input_dev = NULL;
		return ret;
	}

#if FTS_PEN_EN
	ret = fts_input_pen_init(ts_data);
	if (ret) {
		FTS_ERROR("Input-pen device registration failed");
		input_set_drvdata(input_dev, NULL);
		input_free_device(input_dev);
		input_dev = NULL;
		return ret;
	}
#endif

	ts_data->input_dev = input_dev;
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_buffer_init(struct fts_ts_data *ts_data)
{
	ts_data->touch_buf = (u8 *)kzalloc(FTS_MAX_TOUCH_BUF, GFP_KERNEL);
	if (!ts_data->touch_buf) {
		FTS_ERROR("failed to alloc memory for touch buf");
		return -ENOMEM;
	}

	ts_data->touch_size = FTS_SIZE_DEFAULT;

	ts_data->touch_analysis_support = 0;
	ts_data->ta_flag = 0;
	ts_data->ta_size = 0;

    return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
 * Power Control
 *****************************************************************************/
#if FTS_PINCTRL_EN
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
	int ret = 0;

	ts->pinctrl = devm_pinctrl_get(ts->dev);
	if (IS_ERR_OR_NULL(ts->pinctrl)) {
		FTS_ERROR("Failed to get pinctrl, please check dts");
		ret = PTR_ERR(ts->pinctrl);
		goto err_pinctrl_get;
	}

	ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(ts->pins_active)) {
		FTS_ERROR("Pin state[active] not found");
		ret = PTR_ERR(ts->pins_active);
		goto err_pinctrl_lookup;
	}

	ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(ts->pins_suspend)) {
		FTS_ERROR("Pin state[suspend] not found");
		ret = PTR_ERR(ts->pins_suspend);
		goto err_pinctrl_lookup;
	}

	ts->pins_release = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_release");
	if (IS_ERR_OR_NULL(ts->pins_release)) {
		FTS_ERROR("Pin state[release] not found");
		ret = PTR_ERR(ts->pins_release);
	}

	return 0;
err_pinctrl_lookup:
	if (ts->pinctrl)
		devm_pinctrl_put(ts->pinctrl);
err_pinctrl_get:
	ts->pinctrl = NULL;
	ts->pins_release = NULL;
	ts->pins_suspend = NULL;
	ts->pins_active = NULL;
	return ret;
}

static int fts_pinctrl_select_normal(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_active) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
		if (ret < 0)
			FTS_ERROR("Set normal pin state error:%d", ret);
	}

	return ret;
}

static int fts_pinctrl_select_suspend(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_suspend) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
		if (ret < 0)
			FTS_ERROR("Set suspend pin state error:%d", ret);
	}

	return ret;
}

static int fts_pinctrl_select_release(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl) {
		if (IS_ERR_OR_NULL(ts->pins_release)) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts->pinctrl, ts->pins_release);
			if (ret < 0)
				FTS_ERROR("Set gesture pin state error:%d", ret);
		}
	}

	return ret;
}
#endif /* FTS_PINCTRL_EN */

static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		FTS_ERROR("vdd is invalid");
		return -EINVAL;
	}

	FTS_FUNC_ENTER();
	if (enable) {
		if (ts_data->power_disabled) {
			FTS_DEBUG("regulator enable !");
			gpiod_set_value(ts_data->pdata->reset_gpio, 0);
			msleep(1);
			ret = regulator_enable(ts_data->vdd);
			if (ret)
				FTS_ERROR("enable vdd regulator failed,ret=%d", ret);

			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_enable(ts_data->vcc_i2c);
				if (ret)
					FTS_ERROR("enable vcc_i2c regulator failed,ret=%d", ret);
			}
			ts_data->power_disabled = false;
		}
	} else {
		if (!ts_data->power_disabled) {
			FTS_DEBUG("regulator disable !");
			gpiod_set_value(ts_data->pdata->reset_gpio, 0);
			msleep(1);
			ret = regulator_disable(ts_data->vdd);
			if (ret)
				FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_disable(ts_data->vcc_i2c);
				if (ret)
					FTS_ERROR("disable vcc_i2c regulator failed,ret=%d", ret);
			}
			ts_data->power_disabled = true;
		}
	}

	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
 * Name: fts_power_source_init
 * Brief: Init regulator power:vdd/vcc_io(if have), generally, no vcc_io
 *        vdd---->vdd-supply in dts, kernel will auto add "-supply" to parse
 *        Must be call after fts_gpio_configure() execute,because this function
 *        will operate reset-gpio which request gpio in fts_gpio_configure()
 * Input:
 * Output:
 * Return: return 0 if init power successfully, otherwise return error code
 *****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ts_data->vdd = regulator_get(ts_data->dev, "vdd");
	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		ret = PTR_ERR(ts_data->vdd);
		FTS_ERROR("get vdd regulator failed,ret=%d", ret);
		return ret;
	}

	if (regulator_count_voltages(ts_data->vdd) > 0) {
		ret = regulator_set_voltage(ts_data->vdd, FTS_VTG_MIN_UV, FTS_VTG_MAX_UV);
		if (ret) {
			FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
			regulator_put(ts_data->vdd);
			return ret;
		}
	}

	ts_data->vcc_i2c = regulator_get(ts_data->dev, "vcc_i2c");
	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
			ret = regulator_set_voltage(ts_data->vcc_i2c, FTS_I2C_VTG_MIN_UV, FTS_I2C_VTG_MAX_UV);
			if (ret) {
				FTS_ERROR("vcc_i2c regulator set_vtg failed,ret=%d", ret);
				regulator_put(ts_data->vcc_i2c);
			}
		}
	}

#if FTS_PINCTRL_EN
	fts_pinctrl_init(ts_data);
	fts_pinctrl_select_normal(ts_data);
#endif

	ts_data->power_disabled = true;
	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret)
		FTS_ERROR("fail to enable power(regulator)");

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
#if FTS_PINCTRL_EN
	fts_pinctrl_select_release(ts_data);
#endif

	fts_power_source_ctrl(ts_data, DISABLE);

	if (!IS_ERR_OR_NULL(ts_data->vdd)) {
		if (regulator_count_voltages(ts_data->vdd) > 0)
			regulator_set_voltage(ts_data->vdd, 0, FTS_VTG_MAX_UV);
		regulator_put(ts_data->vdd);
	}

	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0)
			regulator_set_voltage(ts_data->vcc_i2c, 0, FTS_I2C_VTG_MAX_UV);
		regulator_put(ts_data->vcc_i2c);
	}

	return 0;
}

static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_suspend(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, DISABLE);
	if (ret < 0)
		FTS_ERROR("power off fail, ret=%d", ret);

	return ret;
}

static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_normal(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret < 0)
		FTS_ERROR("power on fail, ret=%d", ret);

	return ret;
}
#endif /* FTS_POWER_SOURCE_CUST_EN */

static int fts_gpio_configure(struct fts_ts_data *data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	/* request irq gpio */
	data->pdata->irq_gpio = devm_gpiod_get(data->dev, "t_irq", GPIOD_IN);
	if (IS_ERR(data->pdata->irq_gpio)) {
		ret = PTR_ERR(data->pdata->irq_gpio);
		FTS_ERROR("Failed to get irq GPIO: %d", ret);
		goto err_gpio;
	}

	/* request reset gpio */
	data->pdata->reset_gpio = devm_gpiod_get(data->dev, "t_rst", GPIOD_OUT_HIGH);
	if (IS_ERR(data->pdata->reset_gpio)) {
		ret = PTR_ERR(data->pdata->reset_gpio);
		FTS_ERROR("Failed to get reset GPIO: %d", ret);
		goto err_gpio;
	}

	FTS_FUNC_EXIT();
	return 0;

err_gpio:
	FTS_FUNC_EXIT();
	return ret;
}

static int fts_get_dt_coords(struct device *dev, char *name, struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	u32 coords[FTS_COORDS_ARR_SIZE] = {0};
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FTS_COORDS_ARR_SIZE) {
		FTS_ERROR("invalid:%s, size:%d", name, coords_size);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret < 0) {
		FTS_ERROR("Unable to read %s, please check dts", name);
		pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
		pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
		pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
		pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;
		return -ENODATA;
	} else {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	}

	FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max, pdata->y_min, pdata->y_max);
	return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;
	u32 val;

	FTS_FUNC_ENTER();

	ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (ret < 0)
		FTS_ERROR("Unable to get display-coords");

	pdata->revert_x_flag = 0;
	if (!of_property_read_u32(dev->of_node, "focaltech,revert_x_flag", &val))
		pdata->revert_x_flag = !!val;
	pdata->revert_y_flag = 0;
	if (!of_property_read_u32(dev->of_node, "focaltech,revert_y_flag", &val))
		pdata->revert_y_flag = !!val;
	pdata->exchange_x_y_flag = 0;
	if (!of_property_read_u32(dev->of_node, "focaltech,exchange_x_y_flag", &val))
		pdata->exchange_x_y_flag = !!val;

	/* key */
	pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
	if (pdata->have_key) {
		ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key number undefined!");

		ret = of_property_read_u32_array(np, "focaltech, keys", pdata->keys, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Keys undefined!");
		else if (pdata->key_number > FTS_MAX_KEYS)
			pdata->key_number = FTS_MAX_KEYS;

		ret = of_property_read_u32_array(np, "focaltech,key-x-coords", pdata->key_x_coords, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key Y Coords undefined!");

		ret = of_property_read_u32_array(np, "focaltech,key-y-coords", pdata->key_y_coords, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key X Coords undefined!");

		FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
				"coords:(%d,%d),(%d,%d),(%d,%d)",
				pdata->key_number,
				pdata->keys[0], pdata->keys[1], pdata->keys[2],
				pdata->key_x_coords[0], pdata->key_y_coords[0],
				pdata->key_x_coords[1], pdata->key_y_coords[1],
				pdata->key_x_coords[2], pdata->key_y_coords[2]);
	}

	ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (ret < 0) {
		FTS_ERROR("Unable to get max-touch-number, please check dts");
		pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	} else {
		if (temp_val < 2)
			pdata->max_touch_number = 2; /* max_touch_number must >= 2 */
		else if (temp_val > FTS_MAX_POINTS_SUPPORT)
			pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
		else
			pdata->max_touch_number = temp_val;
	}


	FTS_INFO("max touch number:%d", pdata->max_touch_number);

	FTS_FUNC_EXIT();
	return 0;
}

static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data, resume_work);

	fts_ts_resume(ts_data->dev);
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank = NULL;
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data, fb_notif);

	if (!evdata) {
		FTS_ERROR("evdata is null");
		return 0;
	}

	if (event != FB_EVENT_BLANK) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	FTS_INFO("FB event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case FB_BLANK_UNBLANK:
		if (FB_EVENT_BLANK == event)
			queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
		break;
	case FB_BLANK_POWERDOWN:
		if (FB_EVENT_BLANK == event) {
			cancel_work_sync(&fts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		}
		break;
	default:
		FTS_INFO("FB BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}
#elif defined(CONFIG_DRM)
#if defined(CONFIG_DRM_PANEL)
static struct drm_panel *active_panel;

static int drm_check_dt(struct device_node *np)
{
	int i = 0;
	int count = 0;
	struct device_node *node = NULL;
	struct drm_panel *panel = NULL;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0) {
		FTS_ERROR("find drm_panel count(%d) fail", count);
		return -ENODEV;
	}

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			FTS_INFO("find drm_panel successfully");
			active_panel = panel;
			return 0;
		}
	}

	FTS_ERROR("no find drm_panel");
	return -ENODEV;
}

static int drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
#if 0
	struct msm_drm_notifier *evdata = data;
	int *blank = NULL;
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data, fb_notif);

	if (!evdata) {
		FTS_ERROR("evdata is null");
		return 0;
	}

	if (!((event == DRM_PANEL_EARLY_EVENT_BLANK) || (event == DRM_PANEL_EVENT_BLANK))) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	FTS_INFO("DRM event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case DRM_PANEL_BLANK_UNBLANK:
		if (DRM_PANEL_EARLY_EVENT_BLANK == event)
			FTS_INFO("resume: event = %lu, not care\n", event);
		else if (DRM_PANEL_EVENT_BLANK == event)
			queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
		break;
	case DRM_PANEL_BLANK_POWERDOWN:
		if (DRM_PANEL_EARLY_EVENT_BLANK == event) {
			cancel_work_sync(&fts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		} else if (DRM_PANEL_EVENT_BLANK == event) {
			FTS_INFO("suspend: event = %lu, not care\n", event);
		}
		break;
	default:
		FTS_INFO("DRM BLANK(%d) do not need process\n", *blank);
		break;
	}

#endif
	return 0;
}
#else
static int drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int *blank = NULL;
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data, fb_notif);

	if (!evdata) {
		FTS_ERROR("evdata is null");
		return 0;
	}

	if (!((event == MSM_DRM_EARLY_EVENT_BLANK) || (event == MSM_DRM_EVENT_BLANK))) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	FTS_INFO("DRM event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case MSM_DRM_BLANK_UNBLANK:
		if (MSM_DRM_EARLY_EVENT_BLANK == event)
			FTS_INFO("resume: event = %lu, not care\n", event);
		else if (MSM_DRM_EVENT_BLANK == event)
			queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
		break;
	case MSM_DRM_BLANK_POWERDOWN:
		if (MSM_DRM_EARLY_EVENT_BLANK == event) {
			cancel_work_sync(&fts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		} else if (MSM_DRM_EVENT_BLANK == event) {
			FTS_INFO("suspend: event = %lu, not care\n", event);
		}
		break;
	default:
		FTS_INFO("DRM BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void fts_ts_early_suspend(struct early_suspend *handler)
{
	struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data, early_suspend);

	cancel_work_sync(&fts_data->resume_work);
	fts_ts_suspend(ts_data->dev);
}

static void fts_ts_late_resume(struct early_suspend *handler)
{
	struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data, early_suspend);

	queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
}
#endif

static int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int pdata_size = sizeof(struct fts_ts_platform_data);

	FTS_FUNC_ENTER();
	FTS_INFO("%s", FTS_DRIVER_VERSION);
	ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
	if (!ts_data->pdata) {
		FTS_ERROR("allocate memory for platform_data fail");
		return -ENOMEM;
	}

	if (ts_data->dev->of_node) {
		ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
		if (ret)
			FTS_ERROR("device-tree parse fail");

#if defined(CONFIG_FB)
#elif defined(CONFIG_DRM)
#if defined(CONFIG_DRM_PANEL)
		ret = drm_check_dt(ts_data->dev->of_node);
		if (ret)
			FTS_ERROR("parse drm-panel fail");
#endif
#endif
	} else {
		if (ts_data->dev->platform_data) {
			memcpy(ts_data->pdata, ts_data->dev->platform_data, pdata_size);
		} else {
			FTS_ERROR("platform_data is null");
			return -ENODEV;
		}
	}

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (!ts_data->ts_workqueue)
		FTS_ERROR("create fts workqueue fail");

	spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->report_mutex);
	mutex_init(&ts_data->bus_lock);
	init_waitqueue_head(&ts_data->ts_waitqueue);

	/* Init communication interface */
	ret = fts_bus_init(ts_data);
	if (ret) {
		FTS_ERROR("bus initialize fail");
		goto err_bus_init;
	}

	ret = fts_input_init(ts_data);
	if (ret) {
		FTS_ERROR("input initialize fail");
		goto err_input_init;
	}

	ret = fts_buffer_init(ts_data);
	if (ret) {
		FTS_ERROR("buffer init fail");
		goto err_buffer_init;
	}

	ret = fts_gpio_configure(ts_data);
	if (ret) {
		FTS_ERROR("configure the gpios fail");
		goto err_gpio_config;
	}

#if FTS_POWER_SOURCE_CUST_EN
	ret = fts_power_source_init(ts_data);
	if (ret) {
		FTS_ERROR("fail to get power(regulator)");
		goto err_power_init;
	}
#endif

#if (!FTS_CHIP_IDC)
	fts_reset_proc(200);
#endif

	ret = fts_get_ic_information(ts_data);
	if (ret) {
		FTS_ERROR("not focal IC, unregister driver");
		goto err_irq_req;
	}

	ret = fts_create_apk_debug_channel(ts_data);
	if (ret)
		FTS_ERROR("create apk debug node fail");

	ret = fts_create_sysfs(ts_data);
	if (ret)
		FTS_ERROR("create sysfs node fail");

#if FTS_POINT_REPORT_CHECK_EN
	ret = fts_point_report_check_init(ts_data);
	if (ret)
		FTS_ERROR("init point report check fail");
#endif

	ret = fts_ex_mode_init(ts_data);
	if (ret)
		FTS_ERROR("init glove/cover/charger fail");

#if FTS_GESTURE_EN
	ret = fts_gesture_init(ts_data);
	if (ret)
		FTS_ERROR("init gesture fail");
#endif

#if FTS_ESDCHECK_EN
	ret = fts_esdcheck_init(ts_data);
	if (ret)
		FTS_ERROR("init esd check fail");
#endif

	ret = fts_irq_registration(ts_data);
	if (ret) {
		FTS_ERROR("request irq failed");
		goto err_irq_req;
	}

	ret = fts_fwupg_init(ts_data);
	if (ret)
		FTS_ERROR("init fw upgrade fail");

	if (ts_data->ts_workqueue)
		INIT_WORK(&ts_data->resume_work, fts_resume_work);

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	init_completion(&ts_data->pm_completion);
	ts_data->pm_suspend = false;
#endif

#if defined(CONFIG_FB)
	ts_data->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts_data->fb_notif);
	if (ret)
		FTS_ERROR("[FB]Unable to register fb_notifier: %d", ret);
#elif defined(CONFIG_DRM)
	ts_data->fb_notif.notifier_call = drm_notifier_callback;
#if defined(CONFIG_DRM_PANEL)
	if (active_panel) {
		/*
		ret = drm_panel_notifier_register(active_panel, &ts_data->fb_notif);
		if (ret)
			FTS_ERROR("[DRM]drm_panel_notifier_register fail: %d\n", ret);
		*/
	}
#else
	ret = msm_drm_register_client(&ts_data->fb_notif);
	if (ret)
		FTS_ERROR("[DRM]Unable to register fb_notifier: %d\n", ret);
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
	ts_data->early_suspend.suspend = fts_ts_early_suspend;
	ts_data->early_suspend.resume = fts_ts_late_resume;
	register_early_suspend(&ts_data->early_suspend);
#endif

	FTS_FUNC_EXIT();
	return 0;

err_irq_req:
#if FTS_POWER_SOURCE_CUST_EN
err_power_init:
	fts_power_source_exit(ts_data);
#endif
err_gpio_config:
	kfree_safe(ts_data->touch_buf);
err_buffer_init:
	input_unregister_device(ts_data->input_dev);
#if FTS_PEN_EN
	input_unregister_device(ts_data->pen_dev);
#endif
err_input_init:
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);
err_bus_init:
	kfree_safe(ts_data->bus_tx_buf);
	kfree_safe(ts_data->bus_rx_buf);
	kfree_safe(ts_data->pdata);

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();

	cancel_work_sync(&fts_data->resume_work);
#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_exit(ts_data);
#endif

	fts_release_apk_debug_channel(ts_data);
	fts_remove_sysfs(ts_data);
	fts_ex_mode_exit(ts_data);
	fts_fwupg_exit(ts_data);

#if FTS_ESDCHECK_EN
	fts_esdcheck_exit(ts_data);
#endif

	fts_gesture_exit(ts_data);
	free_irq(ts_data->irq, ts_data);
	fts_bus_exit(ts_data);
	input_unregister_device(ts_data->input_dev);
#if FTS_PEN_EN
	input_unregister_device(ts_data->pen_dev);
#endif

	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

#if defined(CONFIG_FB)
	if (fb_unregister_client(&ts_data->fb_notif))
		FTS_ERROR("[FB]Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_DRM)
#if defined(CONFIG_DRM_PANEL)
	/*
	if (active_panel)
		drm_panel_notifier_unregister(active_panel, &ts_data->fb_notif);
	*/
#else
	if (msm_drm_unregister_client(&ts_data->fb_notif))
		FTS_ERROR("[DRM]Error occurred while unregistering fb_notifier.\n");
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts_data->early_suspend);
#endif

#if FTS_POWER_SOURCE_CUST_EN
	fts_power_source_exit(ts_data);
#endif

	kfree_safe(ts_data->touch_buf);
	kfree_safe(ts_data->pdata);
	kfree_safe(ts_data);

	FTS_FUNC_EXIT();

	return 0;
}

static int fts_ts_ebc_suspend(struct tp_device *tp_d)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	FTS_INFO("[FTS]%s\n", __func__);
	ret = fts_ts_suspend(ts_data->dev);

	return ret;
}

static int fts_ts_ebc_resume(struct tp_device *tp_d)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	FTS_INFO("[FTS]%s\n", __func__);
	ret = fts_ts_resume(ts_data->dev);

	return ret;
}

static int fts_ts_suspend(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	FTS_FUNC_ENTER();
	if (ts_data->suspended) {
		FTS_INFO("Already in suspend state");
		return 0;
	}

	if (ts_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		return 0;
	}

#if FTS_ESDCHECK_EN
	fts_esdcheck_suspend(ts_data);
#endif

	if (ts_data->gesture_support) {
		fts_gesture_suspend(ts_data);
	} else {
		fts_irq_disable();

		FTS_INFO("make TP enter into sleep mode");
		ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
		if (ret < 0)
			FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);

		/*if (!ts_data->ic_info.is_incell) {*/
#if FTS_POWER_SOURCE_CUST_EN
		ret = fts_power_source_suspend(ts_data);
		if (ret < 0)
			FTS_ERROR("power enter suspend fail");
#endif
		/*}*/
	}

	fts_release_all_finger();
	ts_data->suspended = true;
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = fts_data;

	FTS_FUNC_ENTER();
	if (!ts_data->suspended) {
		FTS_DEBUG("Already in awake state");
		return 0;
	}

	fts_release_all_finger();

	/*if (!ts_data->ic_info.is_incell) {*/
#if FTS_POWER_SOURCE_CUST_EN
	fts_power_source_resume(ts_data);
#endif
	fts_reset_proc(200);
	/*}*/

	fts_wait_tp_to_valid();
	fts_ex_mode_recovery(ts_data);

#if FTS_ESDCHECK_EN
	fts_esdcheck_resume(ts_data);
#endif

	if (ts_data->gesture_support)
		fts_gesture_resume(ts_data);
	else
		fts_irq_enable();

	ts_data->suspended = false;
	FTS_FUNC_EXIT();
	return 0;
}

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
static int fts_pm_suspend(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	FTS_INFO("system enters into pm_suspend");
	ts_data->pm_suspend = true;
	reinit_completion(&ts_data->pm_completion);

	if (device_may_wakeup(&ts_data->client->dev)) {
		if (!enable_irq_wake(ts_data->irq))
			FTS_INFO("enable irq wake");
	} else {
		FTS_INFO("Device MAY NOT wakeup");
	}

	return 0;
}

static int fts_pm_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	FTS_INFO("system resumes from pm_suspend");
	ts_data->pm_suspend = false;
	complete(&ts_data->pm_completion);

	if (device_may_wakeup(&ts_data->client->dev)) {
		disable_irq_wake(ts_data->irq);
		FTS_INFO("disable irq wake");
	} else {
		FTS_INFO("Device MAY NOT wakeup");
	}

	return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
	.suspend = fts_pm_suspend,
	.resume = fts_pm_resume,
};
#endif

/*****************************************************************************
 * TP Driver
 *****************************************************************************/
static int fts_ts_probe(struct i2c_client *client)
{
	int ret = 0;
	struct fts_ts_data *ts_data = NULL;

	FTS_INFO("Touch Screen(I2C BUS) driver prboe...");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FTS_ERROR("I2C not supported");
		return -ENODEV;
	}

	/* malloc memory for global struct variable */
	ts_data = (struct fts_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		FTS_ERROR("allocate memory for fts_data fail");
		return -ENOMEM;
	}

	fts_data = ts_data;
	ts_data->client = client;
	ts_data->dev = &client->dev;
	ts_data->log_level = 1;
	ts_data->fw_is_running = 0;
	ts_data->bus_type = BUS_TYPE_I2C;
	i2c_set_clientdata(client, ts_data);

	ret = fts_ts_probe_entry(ts_data);
	if (ret) {
		FTS_ERROR("Touch Screen(I2C BUS) driver probe fail");
		kfree_safe(ts_data);
		return ret;
	}

	ts_data->tp.tp_resume = fts_ts_ebc_resume;
	ts_data->tp.tp_suspend = fts_ts_ebc_suspend;
	tp_register_fb(&ts_data->tp);

	ret = device_init_wakeup(&ts_data->client->dev, 1);
	if (ret < 0)
		FTS_ERROR("Error, device init wakeup rc:%d", ret);

	FTS_INFO("Touch Screen(I2C BUS) driver prboe successfully");
	return 0;
}

static void fts_ts_remove(struct i2c_client *client)
{
	fts_ts_remove_entry(i2c_get_clientdata(client));
}

static const struct i2c_device_id fts_ts_id[] = {
	{FTS_DRIVER_NAME, 0},
	{},
};
static const struct of_device_id fts_dt_match[] = {
	{
		.compatible = "focaltech,ft5726",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct i2c_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.driver = {
		.name = FTS_DRIVER_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
		.pm = &fts_dev_pm_ops,
#endif
		.of_match_table = of_match_ptr(fts_dt_match),
	},
	.id_table = fts_ts_id,
};

static int __init fts_ts_init(void)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ret = i2c_add_driver(&fts_ts_driver);
	if (ret != 0)
		FTS_ERROR("Focaltech touch screen driver init failed!");
	FTS_FUNC_EXIT();
	return ret;
}

static void __exit fts_ts_exit(void)
{
	i2c_del_driver(&fts_ts_driver);
}
late_initcall(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
