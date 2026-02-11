// SPDX-License-Identifier: GPL-2.0
/* aw_calib.c   aw codec driver
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

#include <linux/module.h>
#include <asm/ioctls.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "aw_calib.h"
#include "aw_device.h"
#include "aw_log.h"
#include "aw.h"

static bool is_single_cali; /*if mutli_dev cali false, single dev true*/
static unsigned int g_cali_re_time = AW_CALI_RE_DEFAULT_TIMER;
static unsigned int g_dev_select;
static struct miscdevice *g_misc_dev;
static unsigned int g_msic_wr_flag = CALI_STR_NONE;
static DEFINE_MUTEX(g_cali_lock);
static const char *cali_str[CALI_STR_MAX] = {"none", "start_cali", "cali_re",
	"cali_f0", "store_re", "show_re", "show_r0", "show_cali_f0", "show_f0",
	"show_te", "dev_sel", "get_ver", "get_re_range"};

/******************************cali re store example start***********************************/
#ifdef AW_CALI_STORE_EXAMPLE
/*write cali to persist file example*/
#define AWINIC_CALI_FILE "/mnt/vendor/persist/factory/audio/aw_cali.bin"
#define AW_INT_DEC_DIGIT (10)

static void aw_fs_read(struct file *file, char *buf, size_t count, loff_t *pos)
{
#ifdef AW_KERNEL_VER_OVER_5_4_0
	kernel_read(file, buf, count, pos);
#else
	vfs_read(file, buf, count, pos);
#endif
}

static void aw_fs_write(struct file *file, char *buf, size_t count, loff_t *pos)
{
#ifdef AW_KERNEL_VER_OVER_5_4_0
	kernel_write(file, buf, count, pos);
#else
	vfs_write(file, buf, count, pos);
#endif
}

static int aw_cali_write_cali_re_to_file(int32_t cali_re, int channel)
{
	struct file *fp = NULL;
	char buf[50] = {0};
	loff_t pos = 0;
#if !defined AW_KERNEL_VER_OVER_6_1_0
	mm_segment_t fs;
#endif

	fp = filp_open(AWINIC_CALI_FILE, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		aw_pr_err("channel:%d open %s failed, error=%ld", channel,
			  AWINIC_CALI_FILE, PTR_ERR(fp));
		return -EINVAL;
	}

	pos = AW_INT_DEC_DIGIT * channel;

	snprintf(buf, sizeof(buf), "%10d", cali_re);

#ifdef AW_KERNEL_VER_OVER_6_1_0
#elif defined AW_KERNEL_VER_OVER_5_10_0
	fs = force_uaccess_begin();
#else
	fs = get_fs();
	set_fs(KERNEL_DS);
#endif

	aw_fs_write(fp, buf, strlen(buf), &pos);

#ifdef AW_KERNEL_VER_OVER_6_1_0
#elif defined AW_KERNEL_VER_OVER_5_10_0
	force_uaccess_end(fs);
#else
	set_fs(fs);
#endif

	aw_pr_info("channel:%d buf:%s cali_re:%d", channel, buf, cali_re);

	filp_close(fp, NULL);
	return 0;
}

static int aw_cali_get_cali_re_from_file(int32_t *cali_re, int channel)
{
	struct file *fp = NULL;
	int f_size;
	char *buf = NULL;
	int32_t int_cali_re = 0;
	loff_t pos = 0;
#if !defined AW_KERNEL_VER_OVER_6_1_0
	mm_segment_t fs;
#endif
	char *re_buf = NULL;

	fp = filp_open(AWINIC_CALI_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		aw_pr_err("channel:%d open %s failed, error=%ld", channel,
			  AWINIC_CALI_FILE, PTR_ERR(fp));
		return -EINVAL;
	}

	pos = AW_INT_DEC_DIGIT * channel;

	f_size = AW_INT_DEC_DIGIT;

	buf = kzalloc(f_size + 1, GFP_ATOMIC);
	if (!buf) {
		filp_close(fp, NULL);
		return -ENOMEM;
	}
#ifdef AW_KERNEL_VER_OVER_6_1_0
#elif defined AW_KERNEL_VER_OVER_5_10_0
	fs = force_uaccess_begin();
#else
	fs = get_fs();
	set_fs(KERNEL_DS);
#endif

	aw_fs_read(fp, buf, f_size, &pos);

#ifdef AW_KERNEL_VER_OVER_6_1_0
#elif defined AW_KERNEL_VER_OVER_5_10_0
	force_uaccess_end(fs);
#else
	set_fs(fs);
#endif
	re_buf = skip_spaces(buf);

	if (kstrtoint(re_buf, 10, &int_cali_re) == 0)
		*cali_re = int_cali_re;
	else
		*cali_re = AW_ERRO_CALI_RE_VALUE;

	re_buf = NULL;
	aw_pr_info("channel:%d buf:%s int_cali_re: %d", channel, buf,
		   int_cali_re);

	kfree(buf);
	buf = NULL;
	filp_close(fp, NULL);

	return 0;
}
#endif
/********************cali re store example end*****************************/

/*custom need add to set/get cali_re form/to nv*/
static int aw_cali_write_re_to_nvram(int32_t cali_re, int32_t channel)
{
#ifdef AW_CALI_STORE_EXAMPLE
	return aw_cali_write_cali_re_to_file(cali_re, channel);
#else
	return 0;
#endif
}

static int aw_cali_read_re_from_nvram(int32_t *cali_re, int32_t channel)
{
/*custom add, if success return value is 0 , else -1*/
#ifdef AW_CALI_STORE_EXAMPLE
	return aw_cali_get_cali_re_from_file(cali_re, channel);
#else
	return 0;
#endif
}

bool aw_cali_check_result(struct aw_cali_desc *cali_desc)
{
	if (cali_desc->cali_check_st &&
	    (cali_desc->cali_result == CALI_RESULT_ERROR))
		return false;
	else
		return true;
}

static int aw_cali_svc_get_iv_st(struct aw_device *aw_dev)
{
	struct aw_cali_iv_desc *iv_desc = &aw_dev->cali_desc.iv_desc;
	int ret = -1;
	u16 reg_data = 0;
	int i = 0;

	aw_dev_info(aw_dev->dev, "enter");

	for (i = 0; i < AW_GET_IV_CNT_MAX; i++) {
		ret = aw_dev->ops.aw_reg_read(aw_dev, iv_desc->reg, &reg_data);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read 0x%x failed",
				   iv_desc->reg);
			return ret;
		}

		reg_data &= (~iv_desc->reabs_mask);
		if (!reg_data)
			return 0;

		msleep(30);
	}

	aw_dev_err(aw_dev->dev, "IV data abnormal, please check");

	return -EINVAL;
}

static int aw_cali_set_mute_status(struct aw_device *aw_dev, u32 mute)
{
	int ret = -1;
	u32 msg_len = sizeof(u32);

	ret = aw_device_params(aw_dev, AW_DEV_HMUTE_PARAMS, (void *)&mute,
			       msg_len, AW_SET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write_params failed");
		return ret;
	}

	return 0;
}

static void aw_cali_svc_run_mute(struct aw_device *aw_dev, int8_t cali_result)
{
	aw_dev_dbg(aw_dev->dev, "enter");

	if (aw_dev->cali_desc.cali_check_st) {
		if (cali_result == CALI_RESULT_ERROR)
			aw_cali_set_mute_status(aw_dev, AW_DEV_HMUTE_ENABLE);
		else if (cali_result == CALI_RESULT_NORMAL)
			aw_cali_set_mute_status(aw_dev, AW_DEV_HMUTE_DISABLE);
	} else {
		aw_dev_info(aw_dev->dev, "cali check disable");
	}

	aw_dev_info(aw_dev->dev, "done");
}

static int aw_cali_svc_get_dev_re_range(struct aw_device *aw_dev,
					u32 *data_buf)
{
	data_buf[RE_MIN_FLAG] = aw_dev->re_range.re_min;
	data_buf[RE_MAX_FLAG] = aw_dev->re_range.re_max;

	return 0;
}

static int aw_cali_svc_get_devs_re_range(struct aw_device *aw_dev,
					 u32 *data_buf, int num)
{
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1, cnt = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			data_buf[RE_MIN_FLAG + local_dev->channel * 2] =
				local_dev->re_range.re_min;
			data_buf[RE_MAX_FLAG + local_dev->channel * 2] =
				local_dev->re_range.re_max;
			cnt++;
		} else {
			aw_dev_err(local_dev->dev,
				   "channel num[%d] overflow buf num[%d]",
				   local_dev->channel, num);
			return -EINVAL;
		}
	}

	return cnt;
}

static int aw_cali_store_cali_re(struct aw_device *aw_dev, int32_t re)
{
	int ret = -1;

	if ((re > aw_dev->re_range.re_min) && (re < aw_dev->re_range.re_max)) {
		aw_dev->cali_desc.cali_re = re;
		ret = aw_cali_write_re_to_nvram(re, aw_dev->channel);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write re to nvram failed!");
			return ret;
		}
	} else {
		aw_dev_err(aw_dev->dev, "invalid cali re %d!", re);
		return -EINVAL;
	}

	return 0;
}

static int aw_cali_svc_get_cali_re_from_bin(struct aw_cali_desc *cali_desc,
					    u32 *re)
{
	int ret = -1;
	int32_t cali_re = 0;
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);

	if (cali_desc->mode == AW_CALI_MODE_NONE) {
		aw_dev_dbg(aw_dev->dev, "cali mode is NONE");
		return 0;
	}

	ret = aw_cali_read_re_from_nvram(&cali_re, aw_dev->channel);
	if (ret < 0) {
		*re = AW_ERRO_CALI_RE_VALUE;
		cali_desc->cali_result = CALI_RESULT_NONE;
		aw_dev_err(aw_dev->dev, "get re failed");
		return ret;
	}

	if (cali_re < aw_dev->re_range.re_min ||
	    cali_re > aw_dev->re_range.re_max) {
		aw_dev_err(aw_dev->dev, "out range re value: %d", cali_re);
		*re = AW_ERRO_CALI_RE_VALUE;
		/*cali_result is error when aw-cali-check enable*/
		if (aw_dev->cali_desc.cali_check_st)
			cali_desc->cali_result = CALI_RESULT_ERROR;

		return -EINVAL;
	}
	*re = cali_re;

	/*cali_result is normal when aw-cali-check enable*/
	if (aw_dev->cali_desc.cali_check_st)
		cali_desc->cali_result = CALI_RESULT_NORMAL;

	aw_dev_info(aw_dev->dev, "get cali re %d", *re);

	return 0;
}

static int aw_cali_svc_get_cali_re_from_hw(struct aw_cali_desc *cali_desc,
					   u32 *re)
{
	int ret = -1;
	u32 cali_re = 0;
	u16 re_hbit = 0;
	u16 re_lbit = 0;
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);
	struct aw_hw_cali_re_desc *hw_cali_re_desc =
		&cali_desc->hw_cali_re_desc;

	ret = aw_dev->ops.aw_reg_read(aw_dev, hw_cali_re_desc->hbits_reg,
				       &re_hbit);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read re_hbit failed");
		return ret;
	}

	ret = aw_dev->ops.aw_reg_read(aw_dev, hw_cali_re_desc->lbits_reg,
				       &re_lbit);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read re_hbit failed");
		return ret;
	}

	cali_re = (re_hbit << hw_cali_re_desc->hbits_shift) | re_lbit;

	*re = AW_DSP_RE_TO_SHOW_RE(cali_re, hw_cali_re_desc->cali_re_shift) -
	      cali_desc->ra;

	return 0;
}

static int aw_cali_svc_read_cali_re_from_pa(struct aw_cali_desc *cali_desc,
					    u32 *re)
{
	int ret = -1;
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);

	if (aw_dev->ops.aw_get_cali_re) {
		ret = aw_dev->ops.aw_get_cali_re(aw_dev, re);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read cali re error");
			return ret;
		}
	} else {
		ret = aw_cali_svc_get_cali_re_from_hw(cali_desc, re);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read cali re error");
			return ret;
		}
	}

	aw_dev_info(aw_dev->dev, "get cali re:%d", *re);

	return 0;
}

int aw_cali_get_cali_re(struct aw_cali_desc *cali_desc, u32 *re, u32 type)
{
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);

	if (!re)
		return -ENOMEM;

	switch (type) {
	case AW_GET_RE_FROM_BIN:
		return aw_cali_svc_get_cali_re_from_bin(cali_desc, re);
	case AW_GET_RE_FROM_PA:
		return aw_cali_svc_read_cali_re_from_pa(cali_desc, re);
	default:
		aw_dev_err(aw_dev->dev, "unsupported type :%d", type);
		return -EINVAL;
	}
}

static int aw_cali_set_cali_re_to_hw(struct aw_cali_desc *cali_desc)
{
	int ret = -1;
	u32 cali_re = 0;
	u16 re_hbits = 0;
	u16 re_lbits = 0;
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);
	struct aw_hw_cali_re_desc *hw_cali_re_desc =
		&cali_desc->hw_cali_re_desc;

	cali_re = AW_SHOW_RE_TO_DSP_RE((aw_dev->cali_desc.cali_re +
					 aw_dev->cali_desc.ra),
					 hw_cali_re_desc->cali_re_shift);
	re_hbits = (cali_re & (~hw_cali_re_desc->hbits_mask)) >>
		    hw_cali_re_desc->hbits_shift;
	re_lbits = (cali_re & (~hw_cali_re_desc->lbits_mask)) >>
		    hw_cali_re_desc->lbits_shift;

	ret = aw_dev->ops.aw_reg_write(aw_dev, hw_cali_re_desc->hbits_reg,
					re_hbits);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write cali re failed");
		return ret;
	}

	ret = aw_dev->ops.aw_reg_write(aw_dev, hw_cali_re_desc->lbits_reg,
					re_lbits);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write cali re failed");
		return ret;
	}

	return 0;
}

int aw_cali_set_cali_re(struct aw_cali_desc *cali_desc)
{
	int ret = -1;
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);

	if (cali_desc->mode == AW_CALI_MODE_NONE) {
		aw_dev_dbg(aw_dev->dev, "cali mode is NONE");
		return 0;
	}

	if (aw_dev->ops.aw_set_cali_re) {
		ret = aw_dev->ops.aw_set_cali_re(aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "set cali re failed");
			return ret;
		}
	} else {
		ret = aw_cali_set_cali_re_to_hw(cali_desc);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "set cali re failed");
			return ret;
		}
	}

	return 0;
}

int aw_dev_init_re_update(struct aw_cali_desc *cali_desc, backup_sec_t flag)
{
	int ret = -1;
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);
	struct aw_hw_cali_re_desc *hw_cali_re_desc =
		&cali_desc->hw_cali_re_desc;

	if (aw_dev->ops.aw_init_re_update) {
		ret = aw_dev->ops.aw_init_re_update(aw_dev, flag);
		if (ret < 0)
			aw_dev_err(aw_dev->dev, "read or write init re failed");
		return ret;
	}

	if (flag == AW_RECOVERY_SEC_DATA) {
		ret = aw_dev->ops.aw_reg_write(aw_dev,
						hw_cali_re_desc->hbits_reg,
						hw_cali_re_desc->re_hbits);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write cali re failed");
			return ret;
		}

		ret = aw_dev->ops.aw_reg_write(aw_dev,
						hw_cali_re_desc->lbits_reg,
						hw_cali_re_desc->re_lbits);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write cali re failed");
			return ret;
		}
	} else if (flag == AW_RECORD_SEC_DATA) {
		ret = aw_dev->ops.aw_reg_read(aw_dev,
					       hw_cali_re_desc->hbits_reg,
					       &hw_cali_re_desc->re_hbits);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read cali re failed");
			return ret;
		}

		ret = aw_dev->ops.aw_reg_read(aw_dev,
					       hw_cali_re_desc->lbits_reg,
					       &hw_cali_re_desc->re_lbits);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read cali re failed");
			return ret;
		}
	} else {
		aw_dev_err(aw_dev->dev, "unsupported type:%d", flag);
		return -EINVAL;
	}

	return 0;
}

int aw_cali_get_ra(struct aw_cali_desc *cali_desc)
{
	int ret = -1;
	u32 dsp_ra = 0;
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);
	struct aw_ra_desc *desc = &cali_desc->ra_desc;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->dsp_reg, &dsp_ra,
				       desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read ra error");
		return ret;
	}

	cali_desc->ra = AW_DSP_RE_TO_SHOW_RE(dsp_ra, desc->shift);
	aw_dev_info(aw_dev->dev, "get ra:%d", cali_desc->ra);
	return 0;
}

static int aw_cali_get_r0_from_dsp(struct aw_cali_desc *cali_desc, u32 *r0)
{
	int ret = -1;
	u32 dsp_re = 0;
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);
	struct aw_r0_desc *r0_desc = &cali_desc->r0_desc;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, r0_desc->dsp_reg, &dsp_re,
				       r0_desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get_dsp_realtime_re failed");
		return ret;
	}
	*r0 = AW_DSP_RE_TO_SHOW_RE(dsp_re, r0_desc->shift);

	return 0;
}

/*************Calibration base function************/

static int aw_cali_svc_get_dev_r0(struct aw_device *aw_dev, u32 *re)
{
	int ret = -1;
	struct aw_cali_desc *cali_desc = &aw_dev->cali_desc;

	if (aw_dev->ops.aw_get_r0) {
		ret = aw_dev->ops.aw_get_r0(aw_dev, re);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "get_r0 failed");
			return ret;
		}
	} else {
		ret = aw_cali_get_r0_from_dsp(cali_desc, re);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "get_r0 failed");
			return ret;
		}
	}
	return 0;
}

static int aw_cali_svc_get_dev_re(struct aw_device *aw_dev, u32 *re)
{
	int ret = -1;
	struct aw_dsp_cali_re_desc *desc = &aw_dev->cali_desc.dsp_re_desc;
	u32 dsp_re = 0;
	u32 show_re = 0;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->dsp_reg, &dsp_re,
				       desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read re error");
		return ret;
	}

	show_re = AW_DSP_RE_TO_SHOW_RE(dsp_re, desc->shift);

	*re = (u32)(show_re - aw_dev->cali_desc.ra);
	aw_dev_dbg(aw_dev->dev, "real_r0:[%d]", *re);

	return 0;
}

static int aw_cali_svc_get_devs_r0(struct aw_device *aw_dev, int32_t *r0_buf,
				   int num)
{
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1, cnt = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			ret = aw_cali_svc_get_dev_r0(local_dev,
						      &r0_buf[local_dev->channel]);
			if (ret) {
				aw_dev_err(local_dev->dev, "get r0 failed!");
				return ret;
			}
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev,
				   "channel num[%d] overflow buf num[%d] ",
				   local_dev->channel, num);
		}
	}
	return cnt;
}

static int aw_cali_svc_get_dev_f0(struct aw_device *aw_dev, u32 *f0)
{
	struct aw_f0_desc *f0_desc = &aw_dev->cali_desc.f0_desc;
	u32 dsp_val = 0;
	int ret = -1;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, f0_desc->dsp_reg, &dsp_val,
				       f0_desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read f0 failed");
		return ret;
	}

	*f0 = dsp_val >> f0_desc->shift;
	aw_dev_dbg(aw_dev->dev, "real_f0:[%d]", *f0);

	return 0;
}

static int aw_cali_svc_get_devs_f0(struct aw_device *aw_dev, int32_t *f0_buf,
				   int num)
{
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1, cnt = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			ret = aw_cali_svc_get_dev_f0(local_dev,
						      &f0_buf[local_dev->channel]);
			if (ret) {
				aw_dev_err(local_dev->dev, "get f0 failed!");
				return ret;
			}
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev,
				   "channel num[%d] overflow buf num[%d] ",
				   local_dev->channel, num);
		}
	}
	return cnt;
}

static int aw_cali_svc_get_dev_q(struct aw_device *aw_dev, u32 *q)
{
	struct aw_q_desc *q_desc = &aw_dev->cali_desc.q_desc;
	u32 dsp_val = 0;
	int ret;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, q_desc->dsp_reg, &dsp_val,
				       q_desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read q failed");
		return ret;
	}

	*q = ((dsp_val * 1000) >> q_desc->shift);

	return 0;
}

int aw_cali_get_te(struct aw_cali_desc *cali_desc, int32_t *te)
{
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);
	u16 reg_val = 0;
	int ret = -1;

	ret = aw_dev->ops.aw_reg_read(aw_dev, cali_desc->spkr_temp_desc.reg,
				       &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read temperature failed");
		return ret;
	}

	*te = (int32_t)((int16_t)reg_val);
	aw_dev_info(aw_dev->dev, "real_te:[%d]", *te);

	return 0;
}

static int aw_cali_svc_get_dev_te(struct aw_cali_desc *cali_desc, int32_t *te)
{
	return aw_cali_get_te(cali_desc, te);
}

static int aw_cali_svc_get_devs_te(struct aw_device *aw_dev, int32_t *te_buf,
				   int num)
{
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1, cnt = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			ret = aw_cali_svc_get_dev_te(&local_dev->cali_desc,
						      &te_buf[local_dev->channel]);
			if (ret) {
				aw_dev_err(local_dev->dev,
					   "get temperature failed!");
				return ret;
			}
			cnt++;
		} else {
			aw_dev_err(aw_dev->dev,
				   "channel num[%d] overflow buf num[%d]",
				   local_dev->channel, num);
		}
	}
	return cnt;
}

static void aw_cali_svc_bubble_sort(u32 *data, int data_size)
{
	int loop_num = data_size - 1;
	u16 temp_store = 0;
	int i = 0;
	int j = 0;

	if (!data) {
		aw_pr_err("data is NULL");
		return;
	}

	for (i = 0; i < loop_num; i++) {
		for (j = 0; j < loop_num - i; j++) {
			if (data[j] > data[j + 1]) {
				temp_store = data[j];
				data[j] = data[j + 1];
				data[j + 1] = temp_store;
			}
		}
	}
}

static int aw_cali_svc_del_max_min_ave_algo(u32 *data, int data_size)
{
	int sum = 0;
	int ave = 0;
	int i = 0;

	aw_cali_svc_bubble_sort(data, data_size);
	for (i = 1; i < data_size - 1; i++)
		sum += data[i];

	if ((data_size - AW_CALI_DATA_SUM_RM) == 0) {
		aw_pr_err("data_size id :%d less than 2", data_size);
		return -EINVAL;
	}

	ave = sum / (data_size - AW_CALI_DATA_SUM_RM);

	return ave;
}

static void aw_cali_svc_set_cali_status(struct aw_device *aw_dev, bool status)
{
	aw_dev->cali_desc.status = status;

	if (status)
		aw_monitor_stop(&aw_dev->monitor_desc);
	else
		aw_monitor_start(&aw_dev->monitor_desc);

	aw_dev_info(aw_dev->dev, "cali %s",
		    (status == 0) ? ("disable") : ("enable"));
}

static void aw_cali_disable_dsp_ng(struct aw_device *aw_dev, bool status)
{
	struct aw_dsp_ng_desc *dsp_ng_desc = &aw_dev->dsp_ng_desc;

	if (status) {
		aw_device_params(aw_dev, AW_DEV_DSP_NOISE_GATE_PARAMS,
				 (void *)&dsp_ng_desc->disable,
				 sizeof(unsigned int), AW_SET_DEV_PARAMS);
		aw_device_params(aw_dev, AW_DEV_DSP_LOW_POWER_PARAMS,
				 (void *)&dsp_ng_desc->dsp_lp_disable,
				 sizeof(unsigned int), AW_SET_DEV_PARAMS);
	} else {
		aw_device_params(aw_dev, AW_DEV_DSP_NOISE_GATE_PARAMS,
				 (void *)&dsp_ng_desc->enable,
				 sizeof(unsigned int), AW_SET_DEV_PARAMS);
		aw_device_params(aw_dev, AW_DEV_DSP_LOW_POWER_PARAMS,
				 (void *)&dsp_ng_desc->dsp_lp_enable,
				 sizeof(unsigned int), AW_SET_DEV_PARAMS);
	}
}

bool aw_cali_get_cali_status(struct aw_cali_desc *cali_desc)
{
	return cali_desc->status;
}

static int aw_cali_svc_cali_init_check(struct aw_device *aw_dev)
{
	int ret = -1;
	bool status = false;
	u32 mute_status = AW_DEV_HMUTE_ENABLE;

	aw_dev_dbg(aw_dev->dev, "enter");

	/* get pa status */
	status = aw_device_status(aw_dev, AW_DEV_PLL_WDT_STATUS,
				  AW_GET_DEV_STATUS);
	if (!status) {
		aw_dev_err(aw_dev->dev, "get pa status failed");
		return -EINVAL;
	}

	/* get hmute status */
	ret = aw_device_params(aw_dev, AW_DEV_HMUTE_PARAMS,
			       (void *)&mute_status, sizeof(mute_status),
			       AW_GET_DEV_PARAMS);
	if ((ret < 0) || (mute_status == AW_DEV_HMUTE_ENABLE)) {
		aw_dev_err(aw_dev->dev, "get mute status:%d, ret=%d",
			   mute_status, ret);
		return -EINVAL;
	}

	return 0;
}

static int aw_cali_svc_get_cali_cfg(struct aw_device *aw_dev)
{
	int ret = -1;
	struct aw_cali_cfg_desc *desc = &aw_dev->cali_desc.cali_cfg_desc;
	struct cali_cfg *cali_cfg = &aw_dev->cali_desc.cali_cfg;

	aw_dev_dbg(aw_dev->dev, "enter");

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->actampth_reg,
				       &cali_cfg->data[0],
				       desc->actampth_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read reg0x%x error",
			   desc->actampth_reg);
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->noiseampth_reg,
				       &cali_cfg->data[1],
				       desc->noiseampth_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read reg0x%x error",
			   desc->noiseampth_reg);
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->ustepn_reg,
				       &cali_cfg->data[2],
				       desc->ustepn_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read reg0x%x error",
			   desc->ustepn_reg);
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->alphan_reg,
				       &cali_cfg->data[3],
				       desc->alphan_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read reg0x%x error",
			   desc->alphan_reg);
		return ret;
	}

	return 0;
}

static void aw_cali_svc_backup_info(struct aw_device *aw_dev)
{
	u16 reg_val = 0;
	u32 dsp_val = 0;
	struct aw_cali_backup_desc *backup_desc = &aw_dev->cali_desc.backup_info;
	struct aw_dsp_ng_desc *dsp_ng_desc = &aw_dev->dsp_ng_desc;

	/* record dsp ng info */
	if (dsp_ng_desc->reg != AW_REG_NONE) {
		aw_dev->ops.aw_reg_read(aw_dev, dsp_ng_desc->reg, &reg_val);
		backup_desc->dsp_ng_cfg = reg_val & (~dsp_ng_desc->mask);

		aw_dev->ops.aw_dsp_read(aw_dev, dsp_ng_desc->dsp_lp_reg,
					&dsp_val, dsp_ng_desc->dsp_lp_data_type);

		backup_desc->dsp_lp_cfg =
			dsp_val & (~dsp_ng_desc->dsp_lp_mask);
		aw_dev_dbg(aw_dev->dev, "backup dsp ng 0x%x, dsp lp 0x%x",
			   backup_desc->dsp_ng_cfg, backup_desc->dsp_lp_cfg);
	}
}

static void aw_cali_svc_recover_info(struct aw_device *aw_dev)
{
	struct aw_cali_backup_desc *backup_desc = &aw_dev->cali_desc.backup_info;
	struct aw_dsp_ng_desc *dsp_ng_desc = &aw_dev->dsp_ng_desc;

	/* recover back info */
	if (dsp_ng_desc->reg != AW_REG_NONE) {
		aw_dev->ops.aw_reg_write_bits(aw_dev, dsp_ng_desc->reg,
					       dsp_ng_desc->mask,
					       backup_desc->dsp_ng_cfg);

		aw_dev->ops.aw_dsp_write_bits(aw_dev, dsp_ng_desc->dsp_lp_reg,
					       dsp_ng_desc->dsp_lp_mask,
					       backup_desc->dsp_lp_cfg,
					       dsp_ng_desc->dsp_lp_data_type);
		aw_dev_dbg(aw_dev->dev, "recover dsp ng 0x%x, dsp lp 0x%x",
			   backup_desc->dsp_ng_cfg, backup_desc->dsp_lp_cfg);
	}
}

static int aw_cali_svc_set_cali_cfg(struct aw_device *aw_dev,
				    struct cali_cfg cali_cfg)
{
	int ret = -1;
	struct aw_cali_cfg_desc *desc = &aw_dev->cali_desc.cali_cfg_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	ret = aw_dev->ops.aw_dsp_write(aw_dev, desc->actampth_reg,
					cali_cfg.data[0],
					desc->actampth_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp write reg0x%x error",
			   desc->actampth_reg);
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_write(aw_dev, desc->noiseampth_reg,
					cali_cfg.data[1],
					desc->noiseampth_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp write reg0x%x error",
			   desc->noiseampth_reg);
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_write(aw_dev, desc->ustepn_reg,
					cali_cfg.data[2],
					desc->ustepn_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp write reg0x%x error",
			   desc->ustepn_reg);
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_write(aw_dev, desc->alphan_reg,
					cali_cfg.data[3],
					desc->alphan_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp write reg0x%x error",
			   desc->alphan_reg);
		return ret;
	}

	return 0;
}

static int aw_cali_svc_set_dsp_status(struct aw_device *aw_dev, u32 status)
{
	int ret = -1;
	u32 msg_len = sizeof(u32);

	ret = aw_device_params(aw_dev, AW_DEV_DSP_PARAMS, (void *)&status,
			       msg_len, AW_SET_DEV_PARAMS);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write_params failed");
		return ret;
	}

	return 0;
}

static int aw_cali_svc_get_smooth_cali_re(struct aw_device *aw_dev)
{
	int ret = -1;
	int i = 0;
	u32 re_temp[AW_CALI_READ_CNT_MAX] = {0};
	u32 dsp_re = 0;

	aw_dev_dbg(aw_dev->dev, "enter");

	for (i = 0; i < AW_CALI_READ_CNT_MAX; i++) {
		ret = aw_cali_svc_get_dev_re(aw_dev, &re_temp[i]);
		if (ret < 0)
			goto cali_re_fail;

		msleep(30); /*delay 30 ms*/
	}
	dsp_re = aw_cali_svc_del_max_min_ave_algo(re_temp,
						  AW_CALI_READ_CNT_MAX);

	ret = aw_cali_svc_get_iv_st(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get iv data failed");
		goto cali_re_fail;
	}

	if (dsp_re < aw_dev->re_range.re_min ||
	    dsp_re > aw_dev->re_range.re_max) {
		aw_dev_err(aw_dev->dev, "out range re value: [%d]mohm", dsp_re);
		aw_dev->cali_desc.cali_re = dsp_re;
		if (aw_dev->cali_desc.cali_check_st) {
			aw_dev->cali_desc.cali_result = CALI_RESULT_ERROR;
			ret = aw_cali_write_re_to_nvram(dsp_re,
							 aw_dev->channel);
			if (ret < 0)
				aw_dev_err(aw_dev->dev, "write re failed");
		}
		aw_cali_svc_run_mute(aw_dev, aw_dev->cali_desc.cali_result);
		return 0;
	}

	ret = aw_cali_write_re_to_nvram(dsp_re, aw_dev->channel);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write re failed");
		goto cali_re_fail;
	}

	if (aw_dev->cali_desc.cali_check_st)
		aw_dev->cali_desc.cali_result = CALI_RESULT_NORMAL;

	aw_dev->cali_desc.cali_re = dsp_re;
	aw_dev_info(aw_dev->dev, "re[%d]mohm", aw_dev->cali_desc.cali_re);

	aw_cali_svc_set_dsp_status(aw_dev, AW_DEV_DSP_BYPASS);
	aw_cali_set_cali_re(&aw_dev->cali_desc);
	aw_cali_svc_set_dsp_status(aw_dev, AW_DEV_DSP_WORK);

	return 0;

cali_re_fail:
	if (aw_dev->cali_desc.cali_check_st)
		aw_dev->cali_desc.cali_result = CALI_RESULT_ERROR;

	aw_cali_svc_run_mute(aw_dev, aw_dev->cali_desc.cali_result);
	return -EINVAL;
}

static int aw_cali_svc_cali_en(struct aw_device *aw_dev, bool cali_en)
{
	int ret = 0;
	struct cali_cfg set_cfg;

	aw_dev_info(aw_dev->dev, "cali_en:%d", cali_en);

	aw_cali_svc_set_dsp_status(aw_dev, AW_DEV_DSP_BYPASS);
	if (cali_en) {
		aw_cali_disable_dsp_ng(aw_dev, true);
		ret = aw_cali_svc_get_cali_cfg(aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "get cali cfg failed");
			aw_cali_svc_set_dsp_status(aw_dev, AW_DEV_DSP_WORK);
			return ret;
		}
		set_cfg.data[0] = 0;
		set_cfg.data[1] = 0;
		set_cfg.data[2] = -1;
		set_cfg.data[3] = 1;

		ret = aw_cali_svc_set_cali_cfg(aw_dev, set_cfg);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "set cali cfg failed");
			aw_cali_svc_set_cali_cfg(aw_dev,
						 aw_dev->cali_desc.cali_cfg);
			aw_cali_svc_set_dsp_status(aw_dev, AW_DEV_DSP_WORK);
			return ret;
		}
	} else {
		aw_cali_svc_set_cali_cfg(aw_dev, aw_dev->cali_desc.cali_cfg);
	}
	aw_cali_svc_set_dsp_status(aw_dev, AW_DEV_DSP_WORK);

	return 0;
}

static int aw_cali_svc_cali_run_dsp_vol(struct aw_device *aw_dev, int type,
					bool enable)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 set_vol = 0;
	struct aw_dsp_vol_desc *desc = &aw_dev->dsp_vol_desc;

	aw_dev_info(aw_dev->dev, "type:%d, enable:%d", type, enable);

	if (enable) {
		/*set dsp vol*/
		if (type == CALI_TYPE_RE)
			set_vol = desc->mute_st;
		else if (type == CALI_TYPE_F0)
			set_vol = desc->noise_st;
		else
			return -EINVAL;

		ret = aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &reg_val);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read reg 0x%x failed",
				   desc->reg);
			return ret;
		}

		aw_dev->cali_desc.store_vol = reg_val & (~desc->mask);
		reg_val &= desc->mask;
		reg_val |= set_vol;

		ret = aw_dev->ops.aw_reg_write(aw_dev, desc->reg, reg_val);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write reg 0x%x failed",
				   desc->reg);
			return ret;
		}
	} else {
		/*reset dsp vol*/
		ret = aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &reg_val);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read reg 0x%x failed",
				   desc->reg);
			return ret;
		}

		reg_val &= desc->mask;
		reg_val |= aw_dev->cali_desc.store_vol;

		ret = aw_dev->ops.aw_reg_write(aw_dev, desc->reg, reg_val);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write reg 0x%x failed",
				   desc->reg);
			return ret;
		}
	}

	return 0;
}

static int aw_cali_svc_set_white_noise(struct aw_device *aw_dev,
				       bool noise_enable)
{
	int ret = -1;
	u32 reg_val = 0;
	struct aw_noise_desc *desc = &aw_dev->cali_desc.noise_desc;

	aw_dev_info(aw_dev->dev, "set noise %s",
		    (noise_enable == 0) ? ("disable") : ("enable"));

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->dsp_reg, &reg_val,
				       desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read dsp reg 0x%x failed",
			   desc->dsp_reg);
		return ret;
	}

	if (noise_enable)
		reg_val |= (~desc->mask);
	else
		reg_val &= desc->mask;

	ret = aw_dev->ops.aw_dsp_write(aw_dev, desc->dsp_reg, reg_val,
					desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write dsp reg 0x%x failed",
			   desc->dsp_reg);
		return ret;
	}

	return 0;
}

static int aw_cali_svc_cali_f0_en(struct aw_device *aw_dev, bool f0_enable)
{
	int ret = -1;
	struct aw_cali_delay_desc *desc = &aw_dev->cali_desc.cali_delay_desc;

	aw_dev_info(aw_dev->dev, "cali f0 %s",
		    (f0_enable == 0) ? ("disable") : ("enable"));

	if (f0_enable) {
		ret = aw_cali_svc_cali_run_dsp_vol(aw_dev, CALI_TYPE_F0, true);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev,
				   "run dsp volume error, ret=%d", ret);
			return ret;
		}

		msleep(desc->delay);

		ret = aw_cali_svc_set_white_noise(aw_dev, true);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev,
				   "write white noise error, ret=%d", ret);
			aw_cali_svc_cali_run_dsp_vol(aw_dev, CALI_TYPE_F0,
						     false);
			return ret;
		}
	} else {
		aw_cali_svc_set_white_noise(aw_dev, false);
		aw_cali_svc_cali_run_dsp_vol(aw_dev, CALI_TYPE_F0, false);
	}

	return 0;
}

static int aw_cali_svc_get_cali_f0_q(struct aw_device *aw_dev)
{
	int ret = -1;
	u32 cnt = 0;
	u32 f0 = 0;
	u32 q = 0;
	u32 f0_sum = 0;
	u32 q_sum = 0;
	struct aw_cali_desc *cali_desc = &aw_dev->cali_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	for (cnt = 0; cnt < F0_READ_CNT_MAX; cnt++) {
		/*f0*/
		ret = aw_cali_svc_get_dev_f0(aw_dev, &f0);
		if (ret < 0)
			return ret;

		f0_sum += f0;

		/*q*/
		ret = aw_cali_svc_get_dev_q(aw_dev, &q);
		if (ret < 0)
			return ret;

		q_sum += q;
		msleep(30);
	}

	cali_desc->f0 = f0_sum / cnt;
	cali_desc->q = q_sum / cnt;

	ret = aw_cali_svc_get_iv_st(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev,
			   "get iv data failed, set default f0: 2600 q: 2600");
		cali_desc->f0 = 2600;
		cali_desc->q = 2600;
	}

	aw_dev_info(aw_dev->dev, "f0[%d] q[%d]", cali_desc->f0, cali_desc->q);
	return 0;
}

static int aw_cali_svc_cali_mode_enable(struct aw_device *aw_dev, int type,
					unsigned int flag, bool is_enable)
{
	int ret = -1;
	int32_t crc_switch_set = -1;

	aw_dev_info(aw_dev->dev, "type:%d, flag:0x%x, is_enable:%d",
		    type, flag, is_enable);

	if (is_enable) {
		if (aw_dev->crc_check_realtime_desc.init_switch) {
			crc_switch_set = aw_dev->crc_check_realtime_desc.disable;
			ret = aw_device_params(aw_dev,
					       AW_DEV_REALTIME_CRC_SET_PARAMS,
					       (void *)&crc_switch_set,
					       sizeof(crc_switch_set),
					       AW_SET_DEV_PARAMS);
		}

		ret = aw_cali_svc_cali_init_check(aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "init check failed");
			return ret;
		}

		aw_cali_svc_backup_info(aw_dev);
		aw_cali_svc_set_cali_status(aw_dev, true);

		ret = aw_cali_svc_cali_en(aw_dev, true);
		if (ret < 0) {
			aw_cali_svc_set_cali_status(aw_dev, false);
			return ret;
		}

		if ((type == CALI_TYPE_RE) && (flag & CALI_OPS_HMUTE)) {
			ret = aw_cali_svc_cali_run_dsp_vol(aw_dev,
							    CALI_TYPE_RE,
							    true);
			if (ret < 0) {
				aw_cali_svc_cali_en(aw_dev, false);
				aw_cali_svc_set_cali_status(aw_dev, false);
				return ret;
			}
		} else if ((type == CALI_TYPE_F0) && (flag & CALI_OPS_NOISE)) {
			ret = aw_cali_svc_cali_f0_en(aw_dev, true);
			if (ret < 0) {
				aw_cali_svc_cali_en(aw_dev, false);
				aw_cali_svc_set_cali_status(aw_dev, false);
				return ret;
			}
		}
	} else {
		if ((type == CALI_TYPE_RE) && (flag & CALI_OPS_HMUTE))
			aw_cali_svc_cali_run_dsp_vol(aw_dev, CALI_TYPE_RE,
						     false);
		else if ((type == CALI_TYPE_F0) && (flag & CALI_OPS_NOISE))
			aw_cali_svc_cali_f0_en(aw_dev, false);

		aw_cali_svc_recover_info(aw_dev);
		aw_cali_svc_cali_en(aw_dev, false);
		aw_device_status(aw_dev, AW_DEV_CLEAR_INT_STATUS,
				 AW_SET_DEV_STATUS);
		aw_cali_svc_set_cali_status(aw_dev, false);

		if (aw_dev->crc_check_realtime_desc.init_switch) {
			crc_switch_set = aw_dev->crc_check_realtime_desc.enable;
			ret = aw_device_params(aw_dev,
					       AW_DEV_REALTIME_CRC_SET_PARAMS,
					       (void *)&crc_switch_set,
					       sizeof(crc_switch_set),
					       AW_SET_DEV_PARAMS);
		}
	}

	return 0;
}

static int aw_cali_svc_devs_cali_mode_enable(struct list_head *dev_list,
					     int type, unsigned int flag,
					     bool is_enable)
{
	int ret = -1;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (is_enable)
			aw_cali_svc_run_mute(local_dev, CALI_RESULT_NORMAL);

		ret = aw_cali_svc_cali_mode_enable(local_dev, type, flag,
						   is_enable);
		if (ret < 0)
			return ret;

		if (!is_enable && (type == CALI_TYPE_F0))
			aw_cali_svc_run_mute(local_dev,
					     local_dev->cali_desc.cali_result);
	}

	return ret;
}

static int aw_cali_svc_dev_cali_re(struct aw_device *aw_dev, unsigned int flag)
{
	int ret = -1;

	aw_dev_info(aw_dev->dev, "enter");

	aw_cali_svc_run_mute(aw_dev, CALI_RESULT_NORMAL);

	ret = aw_cali_svc_cali_mode_enable(aw_dev, CALI_TYPE_RE, flag, true);
	if (ret < 0)
		return ret;

	msleep(g_cali_re_time);

	ret = aw_cali_svc_get_smooth_cali_re(aw_dev);
	if (ret < 0)
		aw_dev_err(aw_dev->dev, "cali re failed");

	aw_cali_svc_cali_mode_enable(aw_dev, CALI_TYPE_RE, flag, false);

	return ret;
}

static int aw_cali_svc_devs_get_cali_re(struct list_head *dev_list)
{
	int ret = -1;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		ret = aw_cali_svc_get_smooth_cali_re(local_dev);
		if (ret < 0) {
			aw_dev_err(local_dev->dev, "get re failed");
			return ret;
		}
	}

	return ret;
}

static int aw_cali_svc_devs_cali_re(struct aw_device *aw_dev, unsigned int flag)
{
	int ret = -1;
	struct list_head *dev_list = NULL;

	aw_dev_info(aw_dev->dev, "enter");

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, " get dev list failed");
		return ret;
	}

	ret = aw_cali_svc_devs_cali_mode_enable(dev_list, CALI_TYPE_RE,
						flag, true);
	if (ret < 0)
		goto error;

	msleep(g_cali_re_time);

	ret = aw_cali_svc_devs_get_cali_re(dev_list);
	if (ret < 0)
		goto error;

	aw_cali_svc_devs_cali_mode_enable(dev_list, CALI_TYPE_RE, flag, false);

	return 0;

error:
	aw_cali_svc_devs_cali_mode_enable(dev_list, CALI_TYPE_RE, flag, false);
	return ret;
}

static int aw_cali_svc_cali_re(struct aw_device *aw_dev, bool is_single,
			       unsigned int flag)
{
	if (is_single)
		return aw_cali_svc_dev_cali_re(aw_dev, flag);
	else
		return aw_cali_svc_devs_cali_re(aw_dev, flag);
}

static int aw_cali_svc_set_devs_re_str(struct aw_device *aw_dev,
				       const char *re_str)
{
	struct list_head *dev_list = NULL, *pos = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1, cnt = 0;
	int re_data[AW_DEV_CH_MAX] = {0};
	char str_data[32] = {0};
	int i = 0, len = 0;
	int dev_num = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	dev_num = aw_dev->ops.aw_get_dev_num();

	for (i = 0; i < dev_num; i++) {
		memset(str_data, 0, sizeof(str_data));
		snprintf(str_data, sizeof(str_data), "dev[%d]:%s ", i, "%d");
		ret = sscanf(re_str + len, str_data, &re_data[i]);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "unsupported str: %s", re_str);
			return -EINVAL;
		}
		len += snprintf(str_data, sizeof(str_data), "dev[%d]:%d ",
				i, re_data[i]);
		if (len > strlen(re_str)) {
			aw_dev_err(aw_dev->dev, "%s: unsupported", re_str);
			return -EINVAL;
		}
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < AW_DEV_CH_MAX) {
			ret = aw_cali_store_cali_re(local_dev,
						    re_data[local_dev->channel]);
			if (ret < 0) {
				aw_dev_err(local_dev->dev,
					   "store cali re failed");
				return ret;
			}
			cnt++;
		}
	}

	return cnt;
}

static int aw_cali_svc_dev_cali_f0_q(struct aw_device *aw_dev,
				     unsigned int flag)
{
	int ret = -1;

	aw_cali_svc_run_mute(aw_dev, CALI_RESULT_NORMAL);

	ret = aw_cali_svc_cali_mode_enable(aw_dev, CALI_TYPE_F0, flag, true);
	if (ret < 0)
		return ret;

	msleep(AW_CALI_F0_TIME);

	ret = aw_cali_svc_get_cali_f0_q(aw_dev);
	if (ret < 0)
		aw_dev_err(aw_dev->dev, "get f0 q failed");

	aw_cali_svc_cali_mode_enable(aw_dev, CALI_TYPE_F0, flag, false);

	aw_cali_svc_run_mute(aw_dev, aw_dev->cali_desc.cali_result);

	return ret;
}

static int aw_cali_svc_devs_get_cali_f0_q(struct list_head *dev_list)
{
	int ret = -1;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		ret = aw_cali_svc_get_cali_f0_q(local_dev);
		if (ret < 0) {
			aw_dev_err(local_dev->dev, "get f0 q failed");
			return ret;
		}
	}

	return ret;
}

static int aw_cali_svc_devs_cali_f0_q(struct aw_device *aw_dev,
				      unsigned int flag)
{
	int ret = -1;
	struct list_head *dev_list = NULL;

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, " get dev list failed");
		return ret;
	}

	ret = aw_cali_svc_devs_cali_mode_enable(dev_list, CALI_TYPE_F0,
						flag, true);
	if (ret < 0)
		goto error;

	msleep(AW_CALI_F0_TIME);

	ret = aw_cali_svc_devs_get_cali_f0_q(dev_list);
	if (ret < 0)
		goto error;

	aw_cali_svc_devs_cali_mode_enable(dev_list, CALI_TYPE_F0, flag, false);

	return 0;

error:
	aw_cali_svc_devs_cali_mode_enable(dev_list, CALI_TYPE_F0, flag, false);
	return ret;
}

static int aw_cali_svc_cali_f0_q(struct aw_device *aw_dev, bool is_single,
				 unsigned int flag)
{
	if (is_single)
		return aw_cali_svc_dev_cali_f0_q(aw_dev, flag);
	else
		return aw_cali_svc_devs_cali_f0_q(aw_dev, flag);
}

static int aw_cali_svc_get_dev_cali_val(struct aw_device *aw_dev, int type,
					u32 *data_buf)
{
	switch (type) {
	case GET_RE_TYPE:
		*data_buf = aw_dev->cali_desc.cali_re;
		break;
	case GET_F0_TYPE:
		*data_buf = aw_dev->cali_desc.f0;
		break;
	case GET_Q_TYPE:
		*data_buf = aw_dev->cali_desc.q;
		break;
	default:
		aw_dev_err(aw_dev->dev, "type:%d not support", type);
		return -EINVAL;
	}

	return 0;
}

static int aw_cali_svc_get_devs_cali_val(struct aw_device *aw_dev, int type,
					 u32 *data_buf, int num)
{
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1, cnt = 0;

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel < num) {
			switch (type) {
			case GET_RE_TYPE:
				data_buf[local_dev->channel] =
					local_dev->cali_desc.cali_re;
				break;
			case GET_F0_TYPE:
				data_buf[local_dev->channel] =
					local_dev->cali_desc.f0;
				break;
			case GET_Q_TYPE:
				data_buf[local_dev->channel] =
					local_dev->cali_desc.q;
				break;
			default:
				aw_dev_err(local_dev->dev,
					   "type:%d not support", type);
				return -EINVAL;
			}
			cnt++;
		} else {
			aw_dev_err(local_dev->dev,
				   "channel num[%d] overflow buf num[%d]",
				   local_dev->channel, num);
			return -EINVAL;
		}
	}

	return cnt;
}

static int aw_cali_svc_cali_re_f0_q(struct aw_device *aw_dev, bool is_single,
				    unsigned int flag)
{
	int ret = -1;

	ret = aw_cali_svc_cali_re(aw_dev, is_single, flag);
	if (ret < 0)
		return ret;

	ret = aw_cali_svc_cali_f0_q(aw_dev, is_single, flag);
	if (ret < 0)
		return ret;

	return 0;
}

static int aw_cali_svc_cali_cmd(struct aw_device *aw_dev, int cali_cmd,
				bool is_single, unsigned int flag)
{
	switch (cali_cmd) {
	case AW_CALI_CMD_RE:
		return aw_cali_svc_cali_re(aw_dev, is_single, flag);
	case AW_CALI_CMD_F0:
	case AW_CALI_CMD_F0_Q:
		return aw_cali_svc_cali_f0_q(aw_dev, is_single, flag);
	case AW_CALI_CMD_RE_F0:
	case AW_CALI_CMD_RE_F0_Q:
		return aw_cali_svc_cali_re_f0_q(aw_dev, is_single, flag);
	default:
		aw_dev_err(aw_dev->dev, "unsupported cmd %d", cali_cmd);
		return -EINVAL;
	}
	return 0;
}

static int aw_cali_svc_get_cmd_form_str(struct aw_device *aw_dev,
					const char *buf)
{
	int i = 0;

	for (i = 0; i < CALI_STR_MAX; i++) {
		if (!strncmp(cali_str[i], buf, strlen(cali_str[i])))
			break;
	}

	if (i == CALI_STR_MAX) {
		aw_dev_err(aw_dev->dev, "supported cmd [%s]!", buf);
		return -EINVAL;
	}

	aw_dev_dbg(aw_dev->dev, "find str [%s]", cali_str[i]);
	return i;
}

/*****************************attr cali***************************************************/
static ssize_t cali_time_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret = -1;
	u32 time = 0;
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;

	ret = kstrtoint(buf, 0, &time);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read buf %s failed", buf);
		return ret;
	}

	if (time < 1000) {
		aw_dev_err(aw_dev->dev, "time:%d is too short, no set", time);
		return -EINVAL;
	}

	g_cali_re_time = time;
	aw_dev_dbg(aw_dev->dev, "time:%u", time);

	return count;
}

static ssize_t cali_time_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "time: %u\n",
			g_cali_re_time);

	return len;
}

static ssize_t cali_re_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	int ret = -1;
	int re = 0;

	if (is_single_cali) {
		ret = kstrtoint(buf, 0, &re);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read buf %s failed", buf);
			return ret;
		}

		ret = aw_cali_store_cali_re(aw_dev, re);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "store cali re failed!");
			return ret;
		}
	} else {
		ret = aw_cali_svc_set_devs_re_str(aw_dev, buf);
		if (ret <= 0) {
			aw_pr_err("set re str %s failed", buf);
			return -EPERM;
		}
	}

	return count;
}

static ssize_t cali_re_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret = -1, i = 0;
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	ssize_t len = 0;
	int32_t re[AW_DEV_CH_MAX] = {0};

	ret = aw_cali_svc_cali_re(aw_dev, is_single_cali, CALI_OPS_HMUTE);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "cali re failed");
		return ret;
	}

	if (is_single_cali) {
		len += snprintf(buf + len, PAGE_SIZE - len, "dev[%u]: %umOhms\n",
				aw_dev->channel, aw_dev->cali_desc.cali_re);
	} else {
		ret = aw_cali_svc_get_devs_cali_val(aw_dev, GET_RE_TYPE, re,
						    AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get re failed");
		} else {
			for (i = 0; i < ret; i++) {
				len += snprintf(buf + len, PAGE_SIZE - len,
						"dev[%d]: %umOhms ", i, re[i]);
			}

			len += snprintf(buf + len, PAGE_SIZE - len, "\n");
		}
	}

	return len;
}

static ssize_t cali_f0_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret = -1, i = 0;
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	ssize_t len = 0;
	u32 f0[AW_DEV_CH_MAX] = {0};

	ret = aw_cali_svc_cali_f0_q(aw_dev, is_single_cali, CALI_OPS_NOISE);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "cali f0 failed");
		return ret;
	}

	if (is_single_cali) {
		len += snprintf(buf + len, PAGE_SIZE - len, "dev[%u]:%u Hz\n",
				aw_dev->channel, aw_dev->cali_desc.f0);
	} else {
		ret = aw_cali_svc_get_devs_cali_val(aw_dev, GET_F0_TYPE, f0,
						    AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get re failed");
		} else {
			for (i = 0; i < ret; i++) {
				len += snprintf(buf + len, PAGE_SIZE - len,
						"dev[%d]:%u Hz", i, f0[i]);
			}

			len += snprintf(buf + len, PAGE_SIZE - len, "\n");
		}
	}

	return len;
}

static ssize_t cali_f0_q_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int ret = -1, i = 0;
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	ssize_t len = 0;
	u32 f0[AW_DEV_CH_MAX] = {0};
	u32 q[AW_DEV_CH_MAX] = {0};

	ret = aw_cali_svc_cali_f0_q(aw_dev, is_single_cali, CALI_OPS_NOISE);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "cali f0 q failed");
		return ret;
	}

	if (is_single_cali) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"dev[%u]f0:%u Hz q:%u\n", aw_dev->channel,
				aw_dev->cali_desc.f0, aw_dev->cali_desc.q);
	} else {
		ret = aw_cali_svc_get_devs_cali_val(aw_dev, GET_F0_TYPE, f0,
						    AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get f0 failed");
			return -EINVAL;
		}

		ret = aw_cali_svc_get_devs_cali_val(aw_dev, GET_Q_TYPE, q,
						    AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get q failed");
			return -EINVAL;
		}

		for (i = 0; i < ret; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"dev[%d]:f0:%u Hz q:%u ", i, f0[i],
					q[i]);
		}

		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	return len;
}

static ssize_t re_range_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw *aw = dev_get_drvdata(dev);
	struct aw_device *aw_dev = aw->aw_pa;
	u32 range_buf[RE_RANGE_NUM] = {0};

	aw_cali_svc_get_dev_re_range(aw_dev, range_buf);

	len += snprintf(buf + len, PAGE_SIZE - len, "re_min value: [%d]\n",
			range_buf[RE_MIN_FLAG]);
	len += snprintf(buf + len, PAGE_SIZE - len, "re_max value: [%d]\n",
			range_buf[RE_MAX_FLAG]);

	return len;
}

static DEVICE_ATTR_RW(cali_time);
static DEVICE_ATTR_RW(cali_re);
static DEVICE_ATTR_RO(cali_f0);
static DEVICE_ATTR_RO(cali_f0_q);
static DEVICE_ATTR_RO(re_range);

static struct attribute *aw_cali_attr[] = {
	&dev_attr_cali_time.attr,
	&dev_attr_cali_re.attr,
	&dev_attr_cali_f0.attr,
	&dev_attr_cali_f0_q.attr,
	&dev_attr_re_range.attr,
	NULL,
};

static struct attribute_group aw_cali_attr_group = {
	.attrs = aw_cali_attr,
	NULL,
};

static void aw_cali_attr_init(struct aw_device *aw_dev)
{
	int ret;

	ret = sysfs_create_group(&aw_dev->dev->kobj, &aw_cali_attr_group);
	if (ret < 0)
		aw_dev_err(aw_dev->dev,
			   "error creating sysfs cali attr files");
}

static void aw_cali_attr_deinit(struct aw_device *aw_dev)
{
	sysfs_remove_group(&aw_dev->dev->kobj, &aw_cali_attr_group);
	aw_dev_info(aw_dev->dev, "attr files deinit");
}

/*****************************class node******************************************************/
static ssize_t aw_cali_class_time_show(const struct class *class,
				       const struct class_attribute *attr,
				       char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "time: %u\n",
			g_cali_re_time);

	return len;
}

static ssize_t aw_cali_class_time_store(const struct class *class,
					const struct class_attribute *attr,
					const char *buf, size_t len)
{
	int ret;
	u32 time;

	ret = kstrtoint(buf, 0, &time);
	if (ret < 0) {
		aw_pr_err("read buf %s failed", buf);
		return ret;
	}

	if (time < 1000) {
		aw_pr_err("time:%d is too short, no set", time);
		return -EINVAL;
	}

	g_cali_re_time = time;
	aw_pr_dbg("time:%d", time);

	return len;
}

static ssize_t aw_cali_class_cali_re_show(const struct class *class,
					  const struct class_attribute *attr,
					  char *buf)
{
	struct list_head *dev_list;
	struct aw_device *local_dev;
	int ret = -1, i = 0;
	ssize_t len = 0;
	u32 cali_re[AW_DEV_CH_MAX] = {0};

	aw_pr_info("enter");

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_cali_re(local_dev, false, CALI_OPS_HMUTE);
	if (ret < 0)
		return ret;

	ret = aw_cali_svc_get_devs_cali_val(local_dev, GET_RE_TYPE, cali_re,
					    AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get re failed");
	} else {
		for (i = 0; i < ret; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"dev[%d]:%u mOhms ", i, cali_re[i]);
		}

		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	return len;
}

static ssize_t aw_cali_class_cali_re_store(const struct class *class,
					   const struct class_attribute *attr,
					   const char *buf, size_t len)
{
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1;

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_set_devs_re_str(local_dev, buf);
	if (ret <= 0) {
		aw_pr_err("set re str %s failed", buf);
		return -EPERM;
	}

	return len;
}

static ssize_t aw_cali_class_cali_f0_show(const struct class *class,
					  const struct class_attribute *attr,
					  char *buf)
{
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1;
	int i = 0;
	ssize_t len = 0;
	u32 f0[AW_DEV_CH_MAX] = {0};

	aw_pr_info("enter");

	ret = aw_dev_get_list_head(&dev_list);
	if (ret < 0) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_cali_f0_q(local_dev, is_single_cali, CALI_OPS_NOISE);
	if (ret < 0) {
		aw_pr_err("cali f0 failed");
		return ret;
	}

	ret = aw_cali_svc_get_devs_cali_val(local_dev, GET_F0_TYPE, f0,
					    AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_pr_err("get f0 failed");
	} else {
		for (i = 0; i < ret; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"dev[%d]:%u Hz", i, f0[i]);
		}

		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	return len;
}

static ssize_t aw_cali_class_cali_f0_q_show(const struct class *class,
					    const struct class_attribute *attr,
					    char *buf)
{
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1, i = 0;
	ssize_t len = 0;
	u32 f0[AW_DEV_CH_MAX] = {0};
	u32 q[AW_DEV_CH_MAX] = {0};

	aw_pr_info("enter");

	ret = aw_dev_get_list_head(&dev_list);
	if (ret < 0) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	ret = aw_cali_svc_cali_f0_q(local_dev, is_single_cali, CALI_OPS_NOISE);
	if (ret < 0) {
		aw_dev_err(local_dev->dev, "cali f0 q failed");
		return ret;
	}

	ret = aw_cali_svc_get_devs_cali_val(local_dev, GET_F0_TYPE, f0,
					    AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get f0 failed");
		return -EINVAL;
	}

	ret = aw_cali_svc_get_devs_cali_val(local_dev, GET_Q_TYPE, q,
					    AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get q failed");
		return -EINVAL;
	}

	for (i = 0; i < ret; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"dev[%d]:f0:%u Hz q:%u ", i, f0[i], q[i]);
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t aw_class_re_range_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	int ret = -1, i = 0;
	ssize_t len = 0;
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	u32 re_value[AW_DEV_RE_RANGE] = {0};

	aw_pr_info("enter");

	ret = aw_dev_get_list_head(&dev_list);
	if (ret < 0) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);
	ret = aw_cali_svc_get_devs_re_range(local_dev, re_value,
					    AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get re range failed");
		return -EINVAL;
	}

	for (i = 0; i < ret; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"dev[%d]:re_min:%d re_max:%d ", i,
				re_value[RE_MIN_FLAG + i * RE_RANGE_NUM],
				re_value[RE_MAX_FLAG + i * RE_RANGE_NUM]);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static struct class_attribute class_attr_cali_time =
	__ATTR(cali_time,  S_IWUSR | S_IRUGO, aw_cali_class_time_show,
	       aw_cali_class_time_store);

static struct class_attribute class_attr_re25_calib =
	__ATTR(re25_calib, S_IWUSR | S_IRUGO, aw_cali_class_cali_re_show,
	       aw_cali_class_cali_re_store);

static struct class_attribute class_attr_f0_calib =
	__ATTR(f0_calib, S_IRUGO, aw_cali_class_cali_f0_show, NULL);

static struct class_attribute class_attr_f0_q_calib =
	__ATTR(f0_q_calib, S_IRUGO, aw_cali_class_cali_f0_q_show, NULL);

static struct class_attribute class_att_re_range =
	__ATTR(re_range, S_IRUGO, aw_class_re_range_show, NULL);

static struct class aw_cali_class = {
	.name = "smartpa",
};

static void aw_cali_class_attr_init(struct aw_device *aw_dev)
{
	int ret = -1;

	if (aw_dev->channel != 0) {
		aw_dev_err(aw_dev->dev, "class node already register");
		return;
	}

	ret = class_register(&aw_cali_class);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "error creating class node");
		return;
	}

	ret = class_create_file(&aw_cali_class, &class_attr_cali_time);
	if (ret)
		aw_dev_err(aw_dev->dev, "creat class_attr_cali_time fail");

	ret = class_create_file(&aw_cali_class, &class_attr_re25_calib);
	if (ret)
		aw_dev_err(aw_dev->dev, "creat class_attr_re25_calib fail");

	ret = class_create_file(&aw_cali_class, &class_attr_f0_calib);
	if (ret)
		aw_dev_err(aw_dev->dev, "creat class_attr_f0_calib fail");

	ret = class_create_file(&aw_cali_class, &class_attr_f0_q_calib);
	if (ret)
		aw_dev_err(aw_dev->dev, "creat class_attr_f0_q_calib fail");

	ret = class_create_file(&aw_cali_class, &class_att_re_range);
	if (ret)
		aw_dev_err(aw_dev->dev, "creat class_att_re_range fail");
}

static void aw_cali_class_attr_deinit(struct aw_device *aw_dev)
{
	class_remove_file(&aw_cali_class, &class_att_re_range);
	class_remove_file(&aw_cali_class, &class_attr_f0_q_calib);
	class_remove_file(&aw_cali_class, &class_attr_f0_calib);
	class_remove_file(&aw_cali_class, &class_attr_re25_calib);
	class_remove_file(&aw_cali_class, &class_attr_cali_time);

	class_unregister(&aw_cali_class);
	aw_dev_info(aw_dev->dev, "unregister class node");
}
/*****************************class node******************************************************/

/*****************************misc cali******************************************************/
static int aw_cali_misc_open(struct inode *inode, struct file *file)
{
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1;

	aw_pr_dbg("misc open success");

	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed");
		file->private_data = NULL;
		return -EINVAL;
	}

	/*find select dev*/
	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel == g_dev_select)
			break;
	}

	if (!local_dev) {
		aw_pr_err("get dev failed");
		return -EINVAL;
	}

	/*cannot find sel dev, use list first dev*/
	if (local_dev->channel != g_dev_select) {
		local_dev = list_first_entry(dev_list, struct aw_device,
					     list_node);
		aw_dev_dbg(local_dev->dev,
			   "can not find dev[%d], use default", g_dev_select);
	}

	file->private_data = (void *)local_dev;

	aw_dev_dbg(local_dev->dev, "misc open success");

	return 0;
}

static int aw_cali_misc_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	aw_pr_dbg("misc release success");

	return 0;
}

static int aw_cali_misc_ops_write(struct aw_device *aw_dev, unsigned int cmd,
				  unsigned long arg)
{
	unsigned int data_len = _IOC_SIZE(cmd);
	char *data_ptr = NULL;
	int ret = 0;

	data_ptr = devm_kzalloc(aw_dev->dev, data_len, GFP_KERNEL);
	if (!data_ptr)
		return -ENOMEM;

	if (copy_from_user(data_ptr, (void __user *)arg, data_len)) {
		ret = -EFAULT;
		goto exit;
	}

	switch (cmd) {
	case AW_IOCTL_SET_CALI_RE:
		aw_cali_store_cali_re(aw_dev, *((int32_t *)data_ptr));
		break;
	default:
		aw_dev_err(aw_dev->dev, "unsupported  cmd %d", cmd);
		ret = -EINVAL;
		break;
	}

exit:
	devm_kfree(aw_dev->dev, data_ptr);
	data_ptr = NULL;
	return ret;
}

static int aw_cali_misc_ops_read(struct aw_device *aw_dev, unsigned int cmd,
				 unsigned long arg)
{
	int16_t data_len = _IOC_SIZE(cmd);
	char *data_ptr = NULL;
	int32_t *data_32_ptr = NULL;
	int ret = 0;

	data_ptr = devm_kzalloc(aw_dev->dev, data_len, GFP_KERNEL);
	if (!data_ptr)
		return -ENOMEM;

	data_32_ptr = (int32_t *)data_ptr;
	switch (cmd) {
	case AW_IOCTL_GET_RE:
		ret = aw_cali_svc_dev_cali_re(aw_dev, CALI_OPS_HMUTE);
		if (ret < 0)
			goto exit;

		ret = aw_cali_svc_get_dev_cali_val(aw_dev, GET_RE_TYPE,
						   data_32_ptr);
		break;
	case AW_IOCTL_GET_CALI_F0:
		ret = aw_cali_svc_dev_cali_f0_q(aw_dev, CALI_OPS_NOISE);
		if (ret < 0)
			goto exit;

		ret = aw_cali_svc_get_dev_cali_val(aw_dev, GET_F0_TYPE,
						   data_32_ptr);
		break;
	case AW_IOCTL_GET_F0:
		ret = aw_cali_svc_get_dev_f0(aw_dev, data_32_ptr);
		break;
	case AW_IOCTL_GET_TE:
		ret = aw_cali_svc_get_dev_te(&aw_dev->cali_desc, data_32_ptr);
		break;
	case AW_IOCTL_GET_REAL_R0:
		ret = aw_cali_svc_get_dev_r0(aw_dev, data_32_ptr);
		break;
	case AW_IOCTL_GET_RE_RANGE:
		ret = aw_cali_svc_get_dev_re_range(aw_dev, data_32_ptr);
		break;
	default:
		aw_dev_err(aw_dev->dev, "unsupported  cmd %d", cmd);
		ret = -EINVAL;
		break;
	}

exit:
	if (copy_to_user((void __user *)arg, data_ptr, data_len))
		ret = -EFAULT;

	devm_kfree(aw_dev->dev, data_ptr);
	data_ptr = NULL;
	return ret;
}

static int aw_cali_misc_ops(struct aw_device *aw_dev, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
	case AW_IOCTL_SET_CALI_RE:
		return aw_cali_misc_ops_write(aw_dev, cmd, arg);
	case AW_IOCTL_GET_F0:
	case AW_IOCTL_GET_CALI_F0:
	case AW_IOCTL_GET_RE:
	case AW_IOCTL_GET_REAL_R0:
	case AW_IOCTL_GET_TE:
	case AW_IOCTL_GET_RE_RANGE:
		return aw_cali_misc_ops_read(aw_dev, cmd, arg);
	default:
		aw_dev_err(aw_dev->dev, "unsupported  cmd %d", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long aw_cali_misc_unlocked_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	int ret = 0;
	struct aw_device *aw_dev = NULL;

	if (((_IOC_TYPE(cmd)) != (AW_IOCTL_MAGIC))) {
		aw_pr_err(" cmd magic err");
		return -EINVAL;
	}
	aw_dev = (struct aw_device *)file->private_data;
	ret = aw_cali_misc_ops(aw_dev, cmd, arg);
	if (ret < 0)
		return -EINVAL;

	return 0;
}

#ifdef CONFIG_COMPAT
static long aw_cali_misc_compat_ioctl(struct file *file, unsigned int cmd,
				      unsigned long arg)
{
	int ret = 0;
	struct aw_device *aw_dev = NULL;

	if (((_IOC_TYPE(cmd)) != (AW_IOCTL_MAGIC))) {
		aw_pr_err("cmd magic err");
		return -EINVAL;
	}
	aw_dev = (struct aw_device *)file->private_data;
	ret = aw_cali_misc_ops(aw_dev, cmd, arg);
	if (ret < 0)
		return -EINVAL;

	return 0;
}
#endif

static ssize_t aw_cali_misc_read(struct file *filp, char __user *buf,
				 size_t size, loff_t *pos)
{
	int len = 0;
	int i = 0, ret = -1;
	struct aw_device *aw_dev = (struct aw_device *)filp->private_data;
	char local_buf[512] = {0};
	u32 temp_data[AW_DEV_CH_MAX] = {0};
	u32 re_value[AW_DEV_RE_RANGE] = {0};

	aw_dev_info(aw_dev->dev, "enter");

	if (*pos) {
		*pos = 0;
		return 0;
	}

	switch (g_msic_wr_flag) {
	case CALI_STR_SHOW_RE:
		ret = aw_cali_svc_get_devs_cali_val(aw_dev, GET_RE_TYPE,
						    temp_data, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get re failed");
			return -EINVAL;
		}
		for (i = 0; i < ret; i++) {
			len += snprintf(local_buf + len,
					sizeof(local_buf) - len,
					"dev[%d]:%u ", i, temp_data[i]);
		}

		len += snprintf(local_buf + len, sizeof(local_buf) - len,
				"\n");
		break;
	case CALI_STR_SHOW_R0:
		ret = aw_cali_svc_get_devs_r0(aw_dev, temp_data,
					      AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get r0 failed");
			return -EINVAL;
		}
		for (i = 0; i < ret; i++) {
			len += snprintf(local_buf + len,
					sizeof(local_buf) - len,
					"dev[%d]:%d ", i, temp_data[i]);
		}
		len += snprintf(local_buf + len, sizeof(local_buf) - len,
				"\n");
		break;
	case CALI_STR_SHOW_CALI_F0:
		ret = aw_cali_svc_get_devs_cali_val(aw_dev, GET_F0_TYPE,
						    temp_data, AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get cali f0 failed");
			return -EINVAL;
		}
		for (i = 0; i < ret; i++) {
			len += snprintf(local_buf + len,
					sizeof(local_buf) - len,
					"dev[%d]:%d ", i, temp_data[i]);
		}

		len += snprintf(local_buf + len, sizeof(local_buf) - len,
				"\n");
		break;
	case CALI_STR_SHOW_F0:
		ret = aw_cali_svc_get_devs_f0(aw_dev, temp_data,
					      AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get f0 failed");
			return -EINVAL;
		}
		for (i = 0; i < ret; i++) {
			len += snprintf(local_buf + len,
					sizeof(local_buf) - len,
					"dev[%d]:%d ", i, temp_data[i]);
		}

		len += snprintf(local_buf + len, sizeof(local_buf) - len,
				"\n");
		break;
	case CALI_STR_SHOW_TE:
		ret = aw_cali_svc_get_devs_te(aw_dev, temp_data,
					      AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get te failed");
			return -EINVAL;
		}
		for (i = 0; i < ret; i++) {
			len += snprintf(local_buf + len,
					sizeof(local_buf) - len,
					"dev[%d]:%d ", i, temp_data[i]);
		}
		len += snprintf(local_buf + len, sizeof(local_buf) - len,
				"\n");
		break;
	case CALI_STR_VER:
		if (aw_dev->ops.aw_get_version) {
			len = aw_dev->ops.aw_get_version(local_buf,
							 sizeof(local_buf));
			if (len < 0) {
				aw_dev_err(aw_dev->dev, "get version failed");
				return -EINVAL;
			}
			len += snprintf(local_buf + len,
					sizeof(local_buf) - len, "\n");
		} else {
			aw_dev_err(aw_dev->dev, "get version is NULL");
			return -EINVAL;
		}
		break;
	case CALI_STR_SHOW_RE_RANGE:
		ret = aw_cali_svc_get_devs_re_range(aw_dev, re_value,
						    AW_DEV_CH_MAX);
		if (ret <= 0) {
			aw_dev_err(aw_dev->dev, "get re range failed");
			return -EINVAL;
		}
		for (i = 0; i < ret; i++) {
			len += snprintf(local_buf + len,
					sizeof(local_buf) - len,
					"dev[%d]:re_min:%d re_max:%d\n", i,
					re_value[RE_MIN_FLAG + i *
						 RE_RANGE_NUM],
					re_value[RE_MAX_FLAG + i *
						 RE_RANGE_NUM]);
		}
		break;
	default:
		if (g_msic_wr_flag == CALI_STR_NONE) {
			aw_dev_info(aw_dev->dev, "please write cmd first");
			return -EINVAL;
		}
		aw_dev_err(aw_dev->dev, "unsupported flag [%d]",
			   g_msic_wr_flag);
		g_msic_wr_flag = CALI_STR_NONE;
		return -EINVAL;
	}

	if (copy_to_user((void __user *)buf, local_buf, len)) {
		aw_dev_err(aw_dev->dev, "copy_to_user error");
		g_msic_wr_flag = CALI_STR_NONE;
		return -EFAULT;
	}

	g_msic_wr_flag = CALI_STR_NONE;
	*pos += len;
	return len;
}

static int aw_cali_misc_switch_dev(struct file *filp, struct aw_device *aw_dev,
				   char *cmd_buf)
{
	u32 dev_select_num = 0;
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;
	int ret = -1;

	if (!cmd_buf) {
		aw_dev_err(aw_dev->dev, "cmd_buf is NULL");
		return -EINVAL;
	}

	/*get sel dev str*/
	sscanf(cmd_buf, "dev_sel:dev[%u]", &dev_select_num);

	if (dev_select_num >= AW_DEV_CH_MAX) {
		aw_dev_err(aw_dev->dev, "unsupport str [%s]", cmd_buf);
		return -EINVAL;
	}

	/*get dev list*/
	ret = aw_dev_get_list_head(&dev_list);
	if (ret) {
		aw_dev_err(aw_dev->dev, "get dev list failed");
		return ret;
	}

	/*find sel dev*/
	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		if (local_dev->channel == dev_select_num) {
			filp->private_data = (void *)local_dev;
			g_dev_select = dev_select_num;
			aw_dev_info(local_dev->dev, "switch to dev[%u]",
				    dev_select_num);
			return 0;
		}
	}
	aw_dev_err(aw_dev->dev, " unsupport [%s]", cmd_buf);
	return -EINVAL;
}

static ssize_t aw_cali_misc_write(struct file *filp, const char __user *buf,
				  size_t size, loff_t *pos)
{
	char *kernel_buf = NULL;
	struct aw_device *aw_dev = (struct aw_device *)filp->private_data;
	int ret = 0;

	aw_dev_info(aw_dev->dev, "enter, write size:%d", (int)size);

	kernel_buf = kzalloc(size + 1, GFP_KERNEL);
	if (!kernel_buf)
		return -ENOMEM;

	if (copy_from_user(kernel_buf, (void __user *)buf, size)) {
		ret = -EFAULT;
		goto exit;
	}

	ret = aw_cali_svc_get_cmd_form_str(aw_dev, kernel_buf);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "upported cmd [%s]!", kernel_buf);
		ret = -EINVAL;
		goto exit;
	}

	switch (ret) {
	case CALI_STR_CALI_RE_F0:
		ret = aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_RE_F0,
					   is_single_cali,
					   CALI_OPS_HMUTE | CALI_OPS_NOISE);
		break;
	case CALI_STR_CALI_RE:
		ret = aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_RE,
					   is_single_cali, CALI_OPS_HMUTE);
		break;
	case CALI_STR_CALI_F0:
		ret = aw_cali_svc_cali_cmd(aw_dev, AW_CALI_CMD_F0,
					   is_single_cali,
					   CALI_OPS_HMUTE | CALI_OPS_NOISE);
		break;
	case CALI_STR_SET_RE:
		/*skip store_re*/
		kernel_buf[size] = '\0';
		ret = aw_cali_svc_set_devs_re_str(aw_dev,
						  kernel_buf +
						  strlen(cali_str[CALI_STR_SET_RE]) + 1);
		break;
	case CALI_STR_DEV_SEL:
		ret = aw_cali_misc_switch_dev(filp, aw_dev, kernel_buf);
		break;
	case CALI_STR_SHOW_RE:      /*show cali_re*/
	case CALI_STR_SHOW_R0:      /*show real r0*/
	case CALI_STR_SHOW_CALI_F0: /*GET DEV CALI_F0*/
	case CALI_STR_SHOW_F0:      /*SHOW REAL F0*/
	case CALI_STR_SHOW_TE:
	case CALI_STR_VER:
	case CALI_STR_SHOW_RE_RANGE:
		g_msic_wr_flag = ret;
		ret = 0;
		break;
	default:
		aw_dev_err(aw_dev->dev, "unsupported [%s]!", kernel_buf);
		ret = -EINVAL;
		break;
	};

exit:
	aw_dev_dbg(aw_dev->dev, "cmd [%s]!", kernel_buf);

	kfree(kernel_buf);
	kernel_buf = NULL;

	if (ret < 0)
		return -EINVAL;
	else
		return size;
}

static const struct file_operations aw_cali_misc_fops = {
	.owner = THIS_MODULE,
	.open = aw_cali_misc_open,
	.read = aw_cali_misc_read,
	.write = aw_cali_misc_write,
	.release = aw_cali_misc_release,
	.unlocked_ioctl = aw_cali_misc_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = aw_cali_misc_compat_ioctl,
#endif
};

static struct miscdevice misc_cali = {
	.name = "aw_smartpa",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &aw_cali_misc_fops,
};

static int aw_cali_misc_init(struct aw_device *aw_dev)
{
	int ret = -1;

	mutex_lock(&g_cali_lock);
	if (!g_misc_dev) {
		ret = misc_register(&misc_cali);
		if (ret) {
			aw_dev_err(aw_dev->dev, "misc register fail: %d\n",
				   ret);
			mutex_unlock(&g_cali_lock);
			return -EINVAL;
		}
		g_misc_dev = &misc_cali;
		aw_dev_dbg(aw_dev->dev, "misc register success");
	} else {
		aw_dev_dbg(aw_dev->dev, "misc already register");
	}
	mutex_unlock(&g_cali_lock);

	return 0;
}

static void aw_cali_misc_deinit(struct aw_device *aw_dev)
{
	mutex_lock(&g_cali_lock);
	if (g_misc_dev) {
		misc_deregister(g_misc_dev);
		g_misc_dev = NULL;
	}
	mutex_unlock(&g_cali_lock);
	aw_dev_dbg(aw_dev->dev, " misc unregister done");
}
/*****************************misc cali******************************************************/

static void aw_cali_parse_dt(struct aw_device *aw_dev)
{
	struct device_node *np = aw_dev->dev->of_node;
	int ret = -1;
	u32 cali_check = CALI_CHECK_DISABLE;
	const char *cali_mode_str = NULL;
	struct aw_cali_desc *desc = &aw_dev->cali_desc;

	ret = of_property_read_u32(np, "aw-cali-check", &cali_check);
	if (ret < 0) {
		aw_dev_info(aw_dev->dev,
			    " cali-check get failed ,default turn off");
		cali_check = CALI_CHECK_DISABLE;
	}

	desc->cali_check_st = cali_check;
	aw_dev_info(aw_dev->dev, "cali check :%s",
		    (desc->cali_check_st) ? "enable" : "disable");

	ret = of_property_read_string(np, "aw-cali-mode", &cali_mode_str);
	if (ret < 0) {
		desc->mode = AW_CALI_MODE_ALL;
		aw_dev_info(aw_dev->dev, "cali function in use");
		return;
	}

	if (!strcmp(cali_mode_str, "none"))
		desc->mode = AW_CALI_MODE_NONE;
	else
		desc->mode = AW_CALI_MODE_ALL;
}

void aw_cali_init(struct aw_cali_desc *cali_desc)
{
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);

	aw_cali_parse_dt(aw_dev);

	if (cali_desc->mode == AW_CALI_MODE_NONE) {
		aw_dev_info(aw_dev->dev, "cali function disable");
		return;
	}

	aw_cali_attr_init(aw_dev);

	aw_cali_class_attr_init(aw_dev);

	aw_cali_misc_init(aw_dev);

	cali_desc->cali_result = CALI_RESULT_NONE;
}

void aw_cali_deinit(struct aw_cali_desc *cali_desc)
{
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);

	aw_cali_attr_deinit(aw_dev);

	aw_cali_class_attr_deinit(aw_dev);

	aw_cali_misc_deinit(aw_dev);
}
