// SPDX-License-Identifier: GPL-2.0
/* aw_init.c   aw codec driver
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 *
 *  Author: Bruce zhao <zhaolei@awinic.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

/*#define DEBUG*/
#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/syscalls.h>
#include <sound/control.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

#include "aw.h"
#include "aw_device.h"
#include "aw_bin_parse.h"
#include "aw_log.h"

static int aw_dev_reg_read(struct aw_device *aw_dev, u8 reg_addr, u16 *reg_data)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	return aw_reg_read(aw, reg_addr, reg_data);
}

static int aw_dev_reg_write(struct aw_device *aw_dev, u8 reg_addr, u16 reg_data)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	return aw_reg_write(aw, reg_addr, reg_data);
}

static int aw_dev_reg_write_bits(struct aw_device *aw_dev, u8 reg_addr,
				 u16 mask, u16 reg_data)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	return aw_reg_write_bits(aw, reg_addr, mask, reg_data);
}

static int aw_dev_dsp_write_bits(struct aw_device *aw_dev, u16 dsp_addr,
				 u32 dsp_mask, u32 dsp_data, u8 data_type)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	return aw_dsp_write_bits(aw, dsp_addr, dsp_mask, dsp_data, data_type);
}

static int aw_dev_dsp_write(struct aw_device *aw_dev, u16 dsp_addr,
			    u32 dsp_data, u8 data_type)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	return aw_dsp_write(aw, dsp_addr, dsp_data, data_type);
}

static int aw_dev_dsp_writes(struct aw_device *aw_dev, u8 *data, u32 len,
			     u16 base)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	return aw_dsp_writes(aw, data, len, base);
}

static int aw_dev_dsp_read(struct aw_device *aw_dev, u16 dsp_addr,
			   u32 *dsp_data, u8 data_type)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	return aw_dsp_read(aw, dsp_addr, dsp_data, data_type);
}

static int aw_dev_common_init(struct aw_device *aw_pa)
{
	struct aw *aw = (struct aw *)aw_pa->private_data;

	aw_pa->prof_info.prof_desc = NULL;
	aw_pa->prof_info.count = 0;
	aw_pa->prof_info.prof_type = AW_DEV_NONE_TYPE_ID;
	aw_pa->channel = 0;
	aw_pa->i2c_lock = &aw->i2c_lock;
	aw_pa->i2c = aw->i2c;
	aw_pa->fw_status = AW_DEV_FW_FAILED;
	aw_pa->chip_id = aw->chip_id;
	aw_pa->dev = aw->dev;
	aw_pa->ops.aw_get_version = aw_get_version;
	aw_pa->ops.aw_reg_write = aw_dev_reg_write;
	aw_pa->ops.aw_reg_write_bits = aw_dev_reg_write_bits;
	aw_pa->ops.aw_reg_read = aw_dev_reg_read;
	aw_pa->ops.aw_dsp_write_bits = aw_dev_dsp_write_bits;
	aw_pa->ops.aw_dsp_read = aw_dev_dsp_read;
	aw_pa->ops.aw_dsp_write = aw_dev_dsp_write;
	aw_pa->ops.aw_dsp_writes = aw_dev_dsp_writes;
	aw_pa->ops.aw_get_dev_num = aw_get_dev_num;

	return 0;
}

int aw_init_check_chipid(struct aw *aw, u16 chipid)
{
	switch (chipid) {
	case AW_PID_2049:
	case AW_PID_2066:
	case AW_PID_2183:
		aw->chip_id = chipid;
		break;
	default:
		aw_dev_err(aw->dev, "unsupported chip_id 0x%04x", chipid);
		return -EINVAL;
	}
	return 0;
}

int aw_init(struct aw *aw)
{
	int ret = -1;
	struct aw_device *aw_pa = NULL;

	aw_pa = devm_kzalloc(aw->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (!aw_pa)
		return -ENOMEM;

	aw_pa->private_data = (void *)aw;
	aw->aw_pa = aw_pa;

	aw_dev_common_init(aw_pa);

	switch (aw->chip_id) {
	case AW_PID_2049:
		aw_pid_2049_dev_init(aw_pa);
		break;
	case AW_PID_2066:
		aw_pid_2066_dev_init(aw_pa);
		break;
	case AW_PID_2183:
		aw_pid_2183_dev_init(aw_pa);
		break;
	default:
		aw_dev_err(aw->dev, "unsupported chip_id 0x%04x", aw->chip_id);
		return -EINVAL;
	}

	ret = aw_device_probe(aw_pa);
	if (ret < 0) {
		aw_dev_err(aw->dev, "device_probe failed");
		return -EINVAL;
	}

	return 0;
}
