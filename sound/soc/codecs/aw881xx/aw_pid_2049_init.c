// SPDX-License-Identifier: GPL-2.0
/* aw_pid_2049_init.c   aw codec driver
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

#include "aw_data_type.h"
#include "aw.h"
#include "aw_device.h"
#include "aw_bin_parse.h"
#include "aw_pid_2049_reg.h"
#include "aw_log.h"

#define AW_FW_CHECK_PART (10)

/*[9 : 6]: -6DB ; [5 : 0]: -0.125DB  real_value = value * 8 : 0.125db --> 1*/
static unsigned int aw_pid_2049_reg_val_to_db(unsigned int value)
{
	return (((value >> AW_PID_2049_VOL_6DB_START) *
		 AW_PID_2049_VOLUME_STEP_DB) +
		((value & 0x3f) % AW_PID_2049_VOLUME_STEP_DB));
}

/*[9 : 6]: -6DB ; [5 : 0]: -0.125DB reg_value = value / step << 6 + value % step ; step = 6 * 8*/
static u16 aw_pid_2049_db_val_to_reg(u16 value)
{
	return (((value / AW_PID_2049_VOLUME_STEP_DB) <<
		 AW_PID_2049_VOL_6DB_START) +
		(value % AW_PID_2049_VOLUME_STEP_DB));
}

static int aw_pid_2049_set_volume(struct aw_device *aw_dev, u16 value)
{
	u16 reg_value = 0;
	u16 real_value = 0;
	u16 volume = 0;
	struct aw *aw = (struct aw *)aw_dev->private_data;
	struct aw_volume_desc *vol_desc = &aw->aw_pa->volume_desc;

	volume = AW_GET_MIN_VALUE(value, vol_desc->mute_volume);
	real_value = aw_pid_2049_db_val_to_reg(volume);

	/* cal real value */
	aw_reg_read(aw, AW_PID_2049_SYSCTRL2_REG, &reg_value);

	aw_dev_dbg(aw->dev, "value 0x%x , reg:0x%x", value, real_value);

	/*[15 : 6] volume*/
	real_value = (real_value << AW_PID_2049_VOL_START_BIT) |
		     (reg_value & AW_PID_2049_VOL_MASK);

	/* write value */
	aw_reg_write(aw, AW_PID_2049_SYSCTRL2_REG, real_value);

	return 0;
}

static int aw_pid_2049_get_volume(struct aw_device *aw_dev, u16 *value)
{
	u16 reg_value = 0;
	u16 real_value = 0;
	struct aw *aw = (struct aw *)aw_dev->private_data;

	/* read value */
	aw_reg_read(aw, AW_PID_2049_SYSCTRL2_REG, &reg_value);

	/*[15 : 6] volume*/
	real_value = reg_value >> AW_PID_2049_VOL_START_BIT;

	real_value = aw_pid_2049_reg_val_to_db(real_value);

	*value = real_value;

	return 0;
}

static void aw_pid_2049_i2s_tx_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	aw_dev_dbg(aw->dev, "enter");

	if (flag) {
		aw_reg_write_bits(aw, AW_PID_2049_I2SCFG1_REG,
				  AW_PID_2049_I2STXEN_MASK,
				  AW_PID_2049_I2STXEN_ENABLE_VALUE);
	} else {
		aw_reg_write_bits(aw, AW_PID_2049_I2SCFG1_REG,
				  AW_PID_2049_I2STXEN_MASK,
				  AW_PID_2049_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_2049_check_rd_access(int reg)
{
	if (reg >= AW_PID_2049_REG_MAX)
		return false;

	if (aw_pid_2049_reg_access[reg] & REG_RD_ACCESS)
		return true;
	else
		return false;
}

static bool aw_pid_2049_check_wr_access(int reg)
{
	if (reg >= AW_PID_2049_REG_MAX)
		return false;

	if (aw_pid_2049_reg_access[reg] & REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_2049_get_reg_num(void)
{
	return AW_PID_2049_REG_MAX;
}

static u64 aw_pid_2049_dsp_crc32_reflect(u64 ref, u8 ch)
{
	int i;
	u64 value = 0;

	for (i = 1; i < (ch + 1); i++) {
		if (ref & 1)
			value |= 1 << (ch - i);

		ref >>= 1;
	}

	return value;
}

static u32 aw_pid_2049_calc_dsp_cfg_crc32(u8 *buf, u32 len)
{
	u8 i;
	u32 crc = 0xffffffff;

	while (len--) {
		for (i = 1; i != 0; i <<= 1) {
			if ((crc & 0x80000000) != 0) {
				crc <<= 1;
				crc ^= 0x1EDC6F41;
			} else {
				crc <<= 1;
			}

			if ((*buf & i) != 0)
				crc ^= 0x1EDC6F41;
		}
		buf++;
	}

	return (aw_pid_2049_dsp_crc32_reflect(crc, 32) ^ 0xffffffff);
}

static int aw_pid_2049_copy_to_crc_dsp_cfg(struct aw_device *aw_dev, u8 *data,
					   u32 size)
{
	struct aw_sec_data_desc *crc_dsp_cfg = &aw_dev->crc_dsp_cfg;
	int ret = -1;

	if (!crc_dsp_cfg->data) {
		crc_dsp_cfg->data = devm_kzalloc(aw_dev->dev, size,
						  GFP_KERNEL);
		if (!crc_dsp_cfg->data)
			return -ENOMEM;

		crc_dsp_cfg->len = size;
	} else {
		if (crc_dsp_cfg->len != size) {
			devm_kfree(aw_dev->dev, crc_dsp_cfg->data);
			crc_dsp_cfg->data = NULL;
			crc_dsp_cfg->data = devm_kzalloc(aw_dev->dev, size,
							  GFP_KERNEL);
			if (!crc_dsp_cfg->data) {
				aw_dev_err(aw_dev->dev,
					   "error allocating memory");
				return -ENOMEM;
			}
			crc_dsp_cfg->len = size;
		} else {
			aw_dev_info(aw_dev->dev, "crc memory exist");
		}
	}

	memcpy(crc_dsp_cfg->data, data, size);
	ret = aw_dev_dsp_data_order(aw_dev, crc_dsp_cfg->data, size);

	return ret;
}

static int aw_pid_2049_set_dsp_crc32(struct aw_device *aw_dev)
{
	u32 crc_value = 0;
	u32 crc_data_len = 0;
	int ret = -1;
	struct aw_sec_data_desc *crc_dsp_cfg = NULL;
	struct aw_prof_desc *set_prof_desc = NULL;
	struct aw_dsp_crc_desc *desc = &aw_dev->dsp_crc_desc;

	ret = aw_dev_get_prof_data(aw_dev, aw_dev->cur_prof, &set_prof_desc);
	if (ret < 0)
		return ret;

	ret = aw_pid_2049_copy_to_crc_dsp_cfg(aw_dev,
			set_prof_desc->sec_desc[AW_DATA_TYPE_DSP_CFG].data,
			set_prof_desc->sec_desc[AW_DATA_TYPE_DSP_CFG].len);
	if (ret < 0)
		return ret;

	crc_dsp_cfg = &aw_dev->crc_dsp_cfg;

	/*get crc data len*/
	crc_data_len = (u32)((desc->dsp_reg -
			      aw_dev->dsp_mem_desc.dsp_cfg_base_addr) * 2);
	if (crc_data_len > crc_dsp_cfg->len) {
		aw_dev_err(aw_dev->dev,
			   "crc data len :%d > cfg_data len:%d",
			   crc_data_len, crc_dsp_cfg->len);
		return -EINVAL;
	}

	if (crc_data_len % 4 != 0) {
		aw_dev_err(aw_dev->dev, "The crc data len :%d unsupport",
			   crc_data_len);
		return -EINVAL;
	}

	crc_value = aw_pid_2049_calc_dsp_cfg_crc32(crc_dsp_cfg->data,
						    crc_data_len);

	aw_dev_info(aw_dev->dev, "crc_value:0x%x", crc_value);
	ret = aw_dev->ops.aw_dsp_write(aw_dev, desc->dsp_reg, crc_value,
				       desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "set dsp crc value failed");
		return ret;
	}

	return 0;
}

static int aw_pid_2049_dsp_crc_check_enable(struct aw_device *aw_dev,
					    bool flag)
{
	struct aw_dsp_crc_desc *dsp_crc_desc = &aw_dev->dsp_crc_desc;
	int ret;

	aw_dev_info(aw_dev->dev, "enter,flag:%d", flag);
	if (flag) {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
						    dsp_crc_desc->ctl_reg,
						    dsp_crc_desc->ctl_mask,
						    dsp_crc_desc->ctl_enable);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "enable dsp crc failed");
			return ret;
		}
	} else {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
						    dsp_crc_desc->ctl_reg,
						    dsp_crc_desc->ctl_mask,
						    dsp_crc_desc->ctl_disable);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "close dsp crc failed");
			return ret;
		}
	}

	return 0;
}

static int aw_pid_2049_dsp_st_check(struct aw_device *aw_dev)
{
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;
	struct aw_dsp_crc_desc *crc_desc = &aw_dev->dsp_crc_desc;
	int ret = -1;
	u16 reg_val = 0;
	u32 crc_val = 0;
	int i;

	for (i = 0; i < AW_DSP_ST_CHECK_MAX; i++) {
		ret = aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &reg_val);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read reg0x%x failed",
				   desc->reg);
			continue;
		}

		if ((reg_val & (~desc->dsp_mask)) != desc->dsp_check) {
			aw_dev_err(aw_dev->dev,
				   "check dsp st fail,reg_val:0x%04x",
				   reg_val);
			ret = -EINVAL;
			aw_dev->ops.aw_dsp_read(aw_dev, crc_desc->check_reg,
						&crc_val,
						crc_desc->check_reg_data_type);
			aw_dev_err(aw_dev->dev, "read crc value:0x%04x",
				   crc_val);
			usleep_range(AW_1000_US, AW_1000_US + 100);
			continue;
		} else {
			aw_dev_info(aw_dev->dev,
				    "dsp st check ok, reg_val:0x%04x", reg_val);
			return 0;
		}
	}

	return ret;
}

static int aw_pid_2049_dsp_crc32_check(struct aw_device *aw_dev)
{
	int ret;
	struct aw_dsp_en_desc *desc = &aw_dev->dsp_en_desc;

	if (aw_dev->fw_ver < AW_PID_2049_FIRMWARE_VERSION) {
		ret = aw_pid_2049_set_dsp_crc32(aw_dev);
		if (ret < 0) {
			aw_dev_info(aw_dev->dev, "set dsp crc32 failed");
			return ret;
		}
	}

	aw_pid_2049_dsp_crc_check_enable(aw_dev, true);

	/*dsp enable*/
	ret = aw_dev->ops.aw_reg_write_bits(aw_dev, desc->reg, desc->mask,
					    desc->enable);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "enable dsp failed");
		return ret;
	}

	usleep_range(AW_4000_US, AW_4000_US + 100);

	ret = aw_pid_2049_dsp_st_check(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "check crc32 fail");
		return ret;
	}

	aw_pid_2049_dsp_crc_check_enable(aw_dev, false);

	return 0;
}

static int aw_pid_2049_get_convert_r0(struct aw_device *aw_dev, u32 *re)
{
	int ret;
	struct aw_r0_desc *re_desc = &aw_dev->cali_desc.r0_desc;
	struct aw_ra_desc *ra_desc = &aw_dev->cali_desc.ra_desc;
	u32 dsp_re = 0;
	u32 show_re = 0;
	u32 re_cacl = 0;
	u32 ra = 0;
	u32 t0 = 0;
	int32_t te = 0;
	int32_t te_cacl = 0;
	u32 coil_alpha = 0;
	u16 pst_rpt = 0;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, re_desc->dsp_reg, &dsp_re,
				      re_desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read re error");
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_read(aw_dev, ra_desc->dsp_reg, &ra,
				      ra_desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read ra error");
		return ret;
	}

	re_cacl = dsp_re - ra;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, AW_PID_2049_DSP_CFG_ADPZ_T0,
				      &t0, AW_DSP_16_DATA);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read t0 error");
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_read(aw_dev,
				      AW_PID_2049_DSP_CFG_ADPZ_COILALPHA,
				      &coil_alpha, AW_DSP_16_DATA);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp read coil_alpha error");
		return ret;
	}

	ret = aw_dev->ops.aw_reg_read(aw_dev,
				      aw_dev->cali_desc.spkr_temp_desc.reg,
				      &pst_rpt);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "reg read pst_rpt error");
		return ret;
	}

	te = (int32_t)((u32)pst_rpt - t0);
	te_cacl = AW_TE_CACL_VALUE(te, (u16)coil_alpha);
	show_re = AW_RE_REALTIME_VALUE((int32_t)re_cacl, te_cacl);
	*re = AW_DSP_RE_TO_SHOW_RE(show_re, re_desc->shift);
	aw_dev_dbg(aw_dev->dev, "real_r0:[%d]", *re);

	return 0;
}

static int aw_pid_2049_set_cali_re_to_dsp(struct aw_device *aw_dev)
{
	struct aw_cali_desc *cali_desc = &aw_dev->cali_desc;
	struct aw_r0_desc *adpz_re_desc = &cali_desc->r0_desc;
	u32 cali_re = 0;
	int ret;

	cali_re = AW_SHOW_RE_TO_DSP_RE((cali_desc->cali_re + cali_desc->ra),
					adpz_re_desc->shift);

	/* set cali re to aw */
	ret = aw_dev->ops.aw_dsp_write(aw_dev, adpz_re_desc->dsp_reg, cali_re,
				       adpz_re_desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "set cali re error");
		return ret;
	}

	return 0;
}

static int aw_pid_2049_get_cali_re_from_dsp(struct aw_device *aw_dev, u32 *re)
{
	struct aw_r0_desc *desc = &aw_dev->cali_desc.r0_desc;
	int ret;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->dsp_reg, re,
				      desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read cali re error");
		return ret;
	}

	*re = AW_DSP_RE_TO_SHOW_RE(*re, desc->shift);
	*re -= aw_dev->cali_desc.ra;

	aw_dev_info(aw_dev->dev, "get dsp re:%d", *re);

	return 0;
}

static int aw_pid_2049_get_icalk(struct aw_device *aw_dev, int16_t *icalk)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 reg_icalk = 0;

	ret = aw_dev->ops.aw_reg_read(aw_dev, AW_PID_2049_EFRM2_REG, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "reg read failed");
		return ret;
	}

	reg_icalk = reg_val & (~AW_PID_2049_EF_ISN_GESLP_MASK);

	if (reg_icalk & (~AW_PID_2049_EF_ISN_GESLP_SIGN_MASK))
		reg_icalk = reg_icalk | AW_PID_2049_EF_ISN_GESLP_SIGN_NEG;

	*icalk = (int16_t)reg_icalk;

	return 0;
}

static int aw_pid_2049_get_vcalk(struct aw_device *aw_dev, int16_t *vcalk)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 reg_vcalk = 0;

	ret = aw_dev->ops.aw_reg_read(aw_dev, AW_PID_2049_EFRH_REG, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "reg read failed");
		return ret;
	}

	reg_val = reg_val >> AW_PID_2049_EF_VSENSE_GAIN_SHIFT;
	reg_vcalk = (u16)reg_val & (~AW_PID_2049_EF_VSN_GESLP_MASK);
	if (reg_vcalk & (~AW_PID_2049_EF_VSN_GESLP_SIGN_MASK))
		reg_vcalk = reg_vcalk | AW_PID_2049_EF_VSN_GESLP_SIGN_NEG;

	*vcalk = (int16_t)reg_vcalk;

	return 0;
}

static int aw_pid_2049_get_vcalk_dac(struct aw_device *aw_dev, int16_t *vcalk)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 reg_vcalk = 0;

	ret = aw_dev->ops.aw_reg_read(aw_dev, AW_PID_2049_EFRM2_REG, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "reg read failed");
		return ret;
	}

	reg_vcalk = reg_val >> AW_PID_2049_EF_DAC_GESLP_SHIFT;
	if (reg_vcalk & AW_PID_2049_EF_DAC_GESLP_SIGN_MASK)
		reg_vcalk = reg_vcalk | AW_PID_2049_EF_DAC_GESLP_SIGN_NEG;

	*vcalk = (int16_t)reg_vcalk;

	return 0;
}

static int aw_pid_2049_vsense_select(struct aw_device *aw_dev,
				     int *vsense_select)
{
	int ret = -1;
	struct aw_vsense_desc *desc = &aw_dev->vsense_desc;
	u16 vsense_reg_val;

	ret = aw_dev->ops.aw_reg_read(aw_dev, desc->vcalb_vsense_reg,
				      &vsense_reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read vsense_reg_val failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "vsense_reg = 0x%x", vsense_reg_val);

	if (vsense_reg_val & (~desc->vcalk_vdsel_mask)) {
		*vsense_select = AW_DEV_VDSEL_VSENSE;
		aw_dev_info(aw_dev->dev, "vsense outside");
		return 0;
	}

	*vsense_select = AW_DEV_VDSEL_DAC;
	aw_dev_info(aw_dev->dev, "vsense inside");
	return 0;
}

static int aw_pid_2049_init_re_update(struct aw_device *aw_dev,
				      backup_sec_t flag)
{
	int ret = -1;
	struct aw_cali_desc *cali_desc = &aw_dev->cali_desc;
	struct aw_r0_desc *re_desc = &cali_desc->r0_desc;

	if (flag == AW_RECOVERY_SEC_DATA) {
		ret = aw_dev->ops.aw_dsp_write(aw_dev, re_desc->dsp_reg,
					       re_desc->init_value,
					       re_desc->data_type);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "set cali re failed");
			return ret;
		}
	} else if (flag == AW_RECORD_SEC_DATA) {
		ret = aw_dev->ops.aw_dsp_read(aw_dev, re_desc->dsp_reg,
					      &re_desc->init_value,
					      re_desc->data_type);
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

static int aw_pid_2049_init_vcalb_update(struct aw_device *aw_dev,
					 backup_sec_t flag)
{
	int ret = -1;
	struct aw_vcalb_desc *desc = &aw_dev->vcalb_desc;

	if (flag == AW_RECOVERY_SEC_DATA) {
		ret = aw_dev->ops.aw_dsp_write(aw_dev,
					       AW_PID_2049_DSP_REG_VCALB,
					       (u32)desc->last_value,
					       AW_DSP_16_DATA);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write vcalb failed");
			return ret;
		}
	} else if (flag == AW_RECORD_SEC_DATA) {
		ret = aw_dev->ops.aw_dsp_read(aw_dev,
					      AW_PID_2049_DSP_REG_VCALB,
					      (u32 *)&desc->last_value,
					      AW_DSP_16_DATA);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "read vcalb failed");
			return ret;
		}
	} else {
		aw_dev_err(aw_dev->dev, "unsupported type:%d", flag);
		return -EINVAL;
	}

	return 0;
}

static int aw_pid_2049_set_vcalb(struct aw_device *aw_dev)
{
	int ret = -1;
	u32 reg_val = 0;
	int vcalb;
	int icalk;
	int vcalk;
	int16_t icalk_val = 0;
	int16_t vcalk_val = 0;
	u32 vcalb_adj;
	int vsense_select = -1;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, AW_PID_2049_DSP_REG_VCALB,
				      &vcalb_adj, AW_DSP_16_DATA);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read vcalb_adj failed");
		return ret;
	}

	ret = aw_pid_2049_vsense_select(aw_dev, &vsense_select);
	if (ret < 0)
		return ret;
	aw_dev_dbg(aw_dev->dev, "vsense_select = %d", vsense_select);

	ret = aw_pid_2049_get_icalk(aw_dev, &icalk_val);
	if (ret < 0)
		return ret;
	icalk = AW_PID_2049_CABL_BASE_VALUE +
		AW_PID_2049_ICABLK_FACTOR * icalk_val;

	if (vsense_select == AW_DEV_VDSEL_VSENSE) {
		ret = aw_pid_2049_get_vcalk(aw_dev, &vcalk_val);
		if (ret < 0)
			return ret;

		vcalk = (int)(AW_PID_2049_CABL_BASE_VALUE +
			      AW_PID_2049_VCABLK_FACTOR * vcalk_val);
		vcalb = (int)(AW_PID_2049_VCAL_FACTOR *
			      AW_PID_2049_VSCAL_FACTOR /
			      AW_PID_2049_ISCAL_FACTOR *
			      icalk / vcalk * vcalb_adj);

		aw_dev_info(aw_dev->dev,
			    "vcalk_factor=%d, vscal_factor=%d, icalk=%d, vcalk=%d",
			    AW_PID_2049_VCABLK_FACTOR,
			    AW_PID_2049_VSCAL_FACTOR, icalk, vcalk);
	} else if (vsense_select == AW_DEV_VDSEL_DAC) {
		ret = aw_pid_2049_get_vcalk_dac(aw_dev, &vcalk_val);
		if (ret < 0)
			return ret;

		vcalk = (int)(AW_PID_2049_CABL_BASE_VALUE +
			      AW_PID_2049_VCABLK_FACTOR_DAC * vcalk_val);
		vcalb = (int)(AW_PID_2049_VCAL_FACTOR *
			      AW_PID_2049_VSCAL_FACTOR_DAC /
			      AW_PID_2049_ISCAL_FACTOR *
			      icalk / vcalk * vcalb_adj);

		aw_dev_info(aw_dev->dev,
			    "vcalk_dac_factor=%d, vscal_dac_factor=%d, icalk=%d, vcalk=%d",
			    AW_PID_2049_VCABLK_FACTOR_DAC,
			    AW_PID_2049_VSCAL_FACTOR_DAC, icalk, vcalk);
	} else {
		aw_dev_err(aw_dev->dev, "unsupport vsense status");
		return -EINVAL;
	}

	if ((vcalk == 0) || (AW_PID_2049_ISCAL_FACTOR == 0)) {
		aw_dev_err(aw_dev->dev,
			   "vcalk:%d or desc->iscal_factor:%d unsupported",
			   vcalk, AW_PID_2049_ISCAL_FACTOR);
		return -EINVAL;
	}

	vcalb = vcalb >> AW_PID_2049_VCALB_ADJ_FACTOR;
	reg_val = (u32)vcalb;

	aw_dev_info(aw_dev->dev, "vcalb=%d, reg_val=0x%x, vcalb_adj =0x%x",
		    vcalb, reg_val, vcalb_adj);

	ret = aw_dev->ops.aw_dsp_write(aw_dev, AW_PID_2049_DSP_REG_VCALB,
				      reg_val, AW_DSP_16_DATA);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write vcalb failed");
		return ret;
	}

	aw_dev_info(aw_dev->dev, "done");

	return 0;
}

int aw_pid_2049_dev_init(struct aw_device *aw_pa)
{

	/*call aw device init func*/
	aw_pa->fade_step = AW_PID_2049_VOLUME_STEP_DB;
	aw_pa->re_range.re_min_default = AW_RE_MIN;
	aw_pa->re_range.re_max_default = AW_RE_MAX;
	aw_pa->monitor_desc.dsp_monitor_delay = AW_DSP_MONITOR_DELAY;
	aw_pa->crc_type = AW_SW_CRC_CHECK;

	aw_pa->ops.aw_get_hw_volume = aw_pid_2049_get_volume;
	aw_pa->ops.aw_set_hw_volume = aw_pid_2049_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2049_reg_val_to_db;

	aw_pa->ops.aw_check_rd_access = aw_pid_2049_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2049_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2049_get_reg_num;

	aw_pa->ops.aw_i2s_tx_enable = aw_pid_2049_i2s_tx_enable;

	aw_pa->ops.aw_set_vcalb = aw_pid_2049_set_vcalb;
	aw_pa->ops.aw_set_cali_re = aw_pid_2049_set_cali_re_to_dsp;
	aw_pa->ops.aw_get_cali_re = aw_pid_2049_get_cali_re_from_dsp;
	aw_pa->ops.aw_get_r0 = aw_pid_2049_get_convert_r0;
	aw_pa->ops.aw_sw_crc_check = aw_pid_2049_dsp_crc32_check;
	aw_pa->ops.aw_init_vcalb_update = aw_pid_2049_init_vcalb_update;
	aw_pa->ops.aw_init_re_update = aw_pid_2049_init_re_update;

	aw_pa->efuse_check_desc.reg = AW_REG_NONE;
	aw_pa->crc_check_desc.crc_ctrl_reg = AW_REG_NONE;

	aw_pa->int_desc.mask_reg = AW_PID_2049_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2049_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2049_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2049_SYSINT_REG;
	aw_pa->int_desc.intst_mask = AW_PID_2049_BIT_SYSINT_CHECK;

	aw_pa->pwd_desc.reg = AW_PID_2049_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2049_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2049_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2049_PWDN_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2049_SYSCTRL_REG;
	aw_pa->mute_desc.mask = AW_PID_2049_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2049_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2049_HMUTE_DISABLE_VALUE;

	aw_pa->vcalb_desc.vcalb_reg = AW_REG_NONE;

	aw_pa->vsense_desc.vcalb_vsense_reg = AW_PID_2049_I2SCFG3_REG;
	aw_pa->vsense_desc.vcalk_vdsel_mask = AW_PID_2049_VDSEL_MASK;

	aw_pa->sysst_desc.reg = AW_PID_2049_SYSST_REG;
	aw_pa->sysst_desc.st_check = AW_PID_2049_BIT_SYSST_CHECK;
	aw_pa->sysst_desc.st_mask = AW_PID_2049_BIT_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.pll_check = AW_PID_2049_BIT_PLL_CHECK;
	aw_pa->sysst_desc.dsp_check = AW_PID_2049_DSPS_NORMAL_VALUE;
	aw_pa->sysst_desc.dsp_mask = AW_PID_2049_DSPS_MASK;

	aw_pa->noise_gate_en.reg = AW_REG_NONE;

	aw_pa->profctrl_desc.reg = AW_PID_2049_SYSCTRL_REG;
	aw_pa->profctrl_desc.mask = AW_PID_2049_RCV_MODE_MASK;
	aw_pa->profctrl_desc.rcv_mode_val = AW_PID_2049_RCV_MODE_RECEIVER_VALUE;

	aw_pa->volume_desc.reg = AW_PID_2049_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2049_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2049_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2049_MUTE_VOL;
	aw_pa->volume_desc.max_volume = AW_PID_2049_VOL_DEFAULT_VALUE;
	aw_pa->volume_desc.ctl_volume = AW_PID_2049_VOL_DEFAULT_VALUE;

	aw_pa->dsp_en_desc.reg = AW_PID_2049_SYSCTRL_REG;
	aw_pa->dsp_en_desc.mask = AW_PID_2049_DSPBY_MASK;
	aw_pa->dsp_en_desc.enable = AW_PID_2049_DSPBY_WORKING_VALUE;
	aw_pa->dsp_en_desc.disable = AW_PID_2049_DSPBY_BYPASS_VALUE;

	aw_pa->memclk_desc.reg = AW_PID_2049_DBGCTRL_REG;
	aw_pa->memclk_desc.mask = AW_PID_2049_MEM_CLKSEL_MASK;
	aw_pa->memclk_desc.mcu_hclk = AW_PID_2049_MEM_CLKSEL_DAP_HCLK_VALUE;
	aw_pa->memclk_desc.osc_clk = AW_PID_2049_MEM_CLKSEL_OSC_CLK_VALUE;

	aw_pa->watch_dog_desc.reg = AW_PID_2049_WDT_REG;
	aw_pa->watch_dog_desc.mask = AW_PID_2049_WDT_CNT_MASK;

	aw_pa->dsp_mem_desc.dsp_madd_reg = AW_PID_2049_DSPMADD_REG;
	aw_pa->dsp_mem_desc.dsp_mdat_reg = AW_PID_2049_DSPMDAT_REG;
	aw_pa->dsp_mem_desc.dsp_cfg_base_addr = AW_PID_2049_DSP_CFG_ADDR;
	aw_pa->dsp_mem_desc.dsp_fw_base_addr = AW_PID_2049_DSP_FW_ADDR;
	aw_pa->dsp_mem_desc.dsp_rom_check_reg = AW_PID_2049_DSP_ROM_CHECK_ADDR;
	aw_pa->dsp_mem_desc.dsp_rom_check_data = AW_PID_2049_DSP_ROM_CHECK_DATA;

	aw_pa->monitor_desc.voltage_desc.reg = AW_PID_2049_VBAT_REG;
	aw_pa->monitor_desc.voltage_desc.vbat_range = AW_PID_2049_VBAT_RANGE;
	aw_pa->monitor_desc.voltage_desc.int_bit = AW_PID_2049_INT_10BIT;

	aw_pa->monitor_desc.temp_desc.reg = AW_PID_2049_TEMP_REG;
	aw_pa->monitor_desc.temp_desc.sign_mask = AW_PID_2049_TEMP_SIGN_MASK;
	aw_pa->monitor_desc.temp_desc.neg_mask = AW_PID_2049_TEMP_NEG_MASK;

	aw_pa->monitor_desc.vmax_desc.dsp_reg = AW_PID_2049_DSP_REG_VMAX;
	aw_pa->monitor_desc.vmax_desc.data_type = AW_DSP_16_DATA;

	aw_pa->monitor_desc.ipeak_desc.reg = AW_PID_2049_SYSCTRL2_REG;
	aw_pa->monitor_desc.ipeak_desc.mask = AW_PID_2049_BST_IPEAK_MASK;

	aw_pa->soft_rst.reg = AW_PID_2049_ID_REG;
	aw_pa->soft_rst.reg_value = AW_PID_2049_SOFT_RESET_VALUE;

	aw_pa->dsp_vol_desc.reg = AW_PID_2049_DSPCFG_REG;
	aw_pa->dsp_vol_desc.mask = AW_PID_2049_DSP_VOL_MASK;
	aw_pa->dsp_vol_desc.mute_st = AW_PID_2049_DSP_VOL_MUTE;
	aw_pa->dsp_vol_desc.noise_st = AW_PID_2049_DSP_VOL_NOISE_ST;

	aw_pa->amppd_desc.reg = AW_PID_2049_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2049_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2049_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2049_AMPPD_WORKING_VALUE;

	aw_pa->cali_desc.spkr_temp_desc.reg = AW_PID_2049_ASR2_REG;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.ra_desc.dsp_reg = AW_PID_2049_DSP_REG_CFG_ADPZ_RA;
	aw_pa->cali_desc.ra_desc.data_type = AW_DSP_32_DATA;
	aw_pa->cali_desc.ra_desc.shift = AW_PID_2049_DSP_CFG_ADPZ_RA_SHIFT;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.cali_cfg_desc.actampth_reg =
		AW_PID_2049_DSP_REG_CFG_MBMEC_ACTAMPTH;
	aw_pa->cali_desc.cali_cfg_desc.actampth_data_type = AW_DSP_32_DATA;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.cali_cfg_desc.noiseampth_reg =
		AW_PID_2049_DSP_REG_CFG_MBMEC_NOISEAMPTH;
	aw_pa->cali_desc.cali_cfg_desc.noiseampth_data_type = AW_DSP_32_DATA;

	aw_pa->cali_desc.cali_cfg_desc.ustepn_reg =
		AW_PID_2049_DSP_REG_CFG_ADPZ_USTEPN;
	aw_pa->cali_desc.cali_cfg_desc.ustepn_data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.cali_cfg_desc.alphan_reg =
		AW_PID_2049_DSP_REG_CFG_RE_ALPHA;
	aw_pa->cali_desc.cali_cfg_desc.alphan_data_type = AW_DSP_16_DATA;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.r0_desc.dsp_reg = AW_PID_2049_DSP_REG_CFG_ADPZ_RE;
	aw_pa->cali_desc.r0_desc.data_type = AW_DSP_32_DATA;
	aw_pa->cali_desc.r0_desc.shift = AW_PID_2049_DSP_RE_SHIFT;

	aw_pa->cali_desc.dsp_re_desc.shift = AW_PID_2049_DSP_REG_CALRE_SHIFT;
	aw_pa->cali_desc.dsp_re_desc.dsp_reg = AW_PID_2049_DSP_REG_CALRE;
	aw_pa->cali_desc.dsp_re_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.noise_desc.dsp_reg =
		AW_PID_2049_DSP_REG_CFG_MBMEC_GLBCFG;
	aw_pa->cali_desc.noise_desc.data_type = AW_DSP_16_DATA;
	aw_pa->cali_desc.noise_desc.mask = AW_PID_2049_DSP_REG_NOISE_MASK;

	aw_pa->cali_desc.f0_desc.dsp_reg = AW_PID_2049_DSP_REG_RESULT_F0;
	aw_pa->cali_desc.f0_desc.shift = AW_PID_2049_DSP_F0_SHIFT;
	aw_pa->cali_desc.f0_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.q_desc.dsp_reg = AW_PID_2049_DSP_REG_RESULT_Q;
	aw_pa->cali_desc.q_desc.shift = AW_PID_2049_DSP_Q_SHIFT;
	aw_pa->cali_desc.q_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.iv_desc.reg = AW_PID_2049_ASR1_REG;
	aw_pa->cali_desc.iv_desc.reabs_mask = AW_PID_2049_ReAbs_MASK;

	/*32-bit data types need bypass dsp*/
	aw_pa->dsp_crc_desc.dsp_reg = AW_PID_2049_DSP_REG_CRC_ADDR;
	aw_pa->dsp_crc_desc.data_type = AW_DSP_32_DATA;

	aw_pa->dsp_crc_desc.check_reg = AW_PID_2049_CRC_VALUE_CHECK_REG;
	aw_pa->dsp_crc_desc.check_reg_data_type = AW_DSP_16_DATA;

	aw_pa->dsp_crc_desc.ctl_reg = AW_PID_2049_HAGCCFG7_REG;
	aw_pa->dsp_crc_desc.ctl_mask = AW_PID_2049_AGC_DSP_CTL_MASK;
	aw_pa->dsp_crc_desc.ctl_enable = AW_PID_2049_AGC_DSP_CTL_ENABLE_VALUE;
	aw_pa->dsp_crc_desc.ctl_disable = AW_PID_2049_AGC_DSP_CTL_DISABLE_VALUE;

	aw_pa->cco_mux_desc.reg = AW_PID_2049_PLLCTRL1_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2049_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divider = AW_PID_2049_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass = AW_PID_2049_CCO_MUX_BYPASS_VALUE;

	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_reg =
		AW_PID_2049_DSP_REG_TEMP_SWITCH_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_mask =
		AW_PID_2049_DSP_EXTN_TEMP_EN_MASK;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_reg =
		AW_PID_2049_DSP_REG_TEMP_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_reg_type = AW_DSP_16_DATA;

	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_reg =
		AW_PID_2049_DSP_REG_TEMP_SWITCH_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_reg_type = AW_DSP_16_DATA;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_en_mask =
		AW_PID_2049_DSP_MONITOR_TEMP_EN_MASK;

	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_reg =
		AW_PID_2049_DSP_REG_CFG_MBMEC_GLBCFG;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_en_mask =
		AW_PID_2049_DSP_MONITOR_VOLT_EN_MASK;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_reg_type = AW_DSP_16_DATA;

	aw_pa->monitor_desc.hw_monitor_desc.reg = AW_REG_NONE;

	aw_pa->chansel_desc.rxchan_reg = AW_PID_2049_I2SCTRL_REG;
	aw_pa->chansel_desc.rxchan_mask = AW_PID_2049_CHSEL_MASK;
	aw_pa->chansel_desc.rx_left = AW_PID_2049_CHSEL_LEFT_VALUE;
	aw_pa->chansel_desc.rx_right = AW_PID_2049_CHSEL_RIGHT_VALUE;

	aw_pa->tx_en_desc.tx_en_reg = AW_PID_2049_I2SCFG1_REG;
	aw_pa->tx_en_desc.tx_en_mask = AW_PID_2049_I2STXEN_MASK;
	aw_pa->tx_en_desc.tx_disable = AW_PID_2049_I2STXEN_DISABLE_VALUE;

	aw_pa->cali_desc.cali_delay_desc.dsp_reg =
		AW_PID_2049_DSP_CALI_F0_DELAY;
	aw_pa->cali_desc.cali_delay_desc.data_type = AW_DSP_16_DATA;

	aw_pa->dsp_st_desc.dsp_reg_s1 = AW_PID_2049_DSP_ST_S1;
	aw_pa->dsp_st_desc.dsp_reg_e1 = AW_PID_2049_DSP_ST_E1;
	aw_pa->dsp_st_desc.dsp_reg_s2 = AW_PID_2049_DSP_ST_S2;
	aw_pa->dsp_st_desc.dsp_reg_e2 = AW_PID_2049_DSP_ST_E2;

	aw_pa->fw_ver_desc.version_reg = AW_PID_2049_FIRMWARE_VERSION_REG;
	aw_pa->fw_ver_desc.data_type = AW_DSP_16_DATA;

	aw_pa->dither_desc.reg = AW_PID_2049_DBGCTRL_REG;
	aw_pa->dither_desc.mask = AW_PID_2049_DITHER_EN_MASK;
	aw_pa->dither_desc.enable = AW_PID_2049_DITHER_EN_ENABLE_VALUE;
	aw_pa->dither_desc.disable = AW_PID_2049_DITHER_EN_DISABLE_VALUE;

	aw_pa->dsp_ng_desc.reg = AW_REG_NONE;
	aw_pa->dsp_ng_desc.dsp_lp_reg = AW_REG_NONE;

	return 0;
}
