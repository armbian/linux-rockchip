// SPDX-License-Identifier: GPL-2.0
/* aw_device.c   aw codec driver
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
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/syscalls.h>
#include <sound/control.h>
#include <linux/uaccess.h>

#include "aw_device.h"
#include "aw_data_type.h"
#include "aw_bin_parse.h"
#include "aw_log.h"

#define AW_DEV_SYSST_CHECK_MAX (10)

static unsigned int g_fade_in_time = AW_1000_US / 10;
static unsigned int g_fade_out_time = AW_1000_US >> 1;
static LIST_HEAD(g_dev_list);

int aw_dev_get_list_head(struct list_head **head)
{
	if (list_empty(&g_dev_list))
		return -EINVAL;

	*head = &g_dev_list;

	return 0;
}

void aw_dev_add_dev_list(struct aw_device *aw_dev)
{
	list_add(&aw_dev->list_node, &g_dev_list);
}

static int aw_dev_reg_dump(struct aw_device *aw_dev)
{
	int reg_num = aw_dev->ops.aw_get_reg_num();
	int i = 0;
	u16 reg_val = 0;

	for (i = 0; i < reg_num; i++) {
		if (aw_dev->ops.aw_check_rd_access(i)) {
			aw_dev->ops.aw_reg_read(aw_dev, i, &reg_val);
			aw_dev_info(aw_dev->dev,
				    "read: reg = 0x%02x, val = 0x%04x",
				    i, reg_val);
		}
	}

	return 0;
}

static int aw_dev_set_volume(struct aw_device *aw_dev, u32 set_vol)
{
	u16 hw_vol = 0;
	int ret = -1;
	struct aw_volume_desc *vol_desc = &aw_dev->volume_desc;

	hw_vol = set_vol + vol_desc->init_volume;

	ret = aw_dev->ops.aw_set_hw_volume(aw_dev, hw_vol);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "set volume failed");
		return ret;
	}

	return 0;
}

static int aw_dev_get_volume(struct aw_device *aw_dev, u32 *get_vol)
{
	int ret = -1;
	u16 hw_vol = 0;
	struct aw_volume_desc *vol_desc = &aw_dev->volume_desc;

	ret = aw_dev->ops.aw_get_hw_volume(aw_dev, &hw_vol);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read volume failed");
		return ret;
	}

	*get_vol = (u32)(hw_vol - vol_desc->init_volume);

	return 0;
}

static void aw_dev_fade_in(struct aw_device *aw_dev)
{
	int i = 0;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;
	int fade_step = aw_dev->fade_step;
	int fade_in_vol = desc->ctl_volume;

	if (!aw_dev->fade_en)
		return;

	if (fade_step == 0 || g_fade_in_time == 0) {
		aw_dev_set_volume(aw_dev, fade_in_vol);
		return;
	}
	/*volume up*/
	for (i = desc->mute_volume; i >= fade_in_vol; i -= fade_step) {
		aw_dev_set_volume(aw_dev, i);
		usleep_range(g_fade_in_time, g_fade_in_time + 10);
	}
	if (i != fade_in_vol)
		aw_dev_set_volume(aw_dev, fade_in_vol);
}

static void aw_dev_fade_out(struct aw_device *aw_dev)
{
	int i = 0;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;
	int fade_step = aw_dev->fade_step;

	if (!aw_dev->fade_en)
		return;

	if (fade_step == 0 || g_fade_out_time == 0) {
		aw_dev_set_volume(aw_dev, desc->mute_volume);
		return;
	}

	for (i = desc->ctl_volume; i <= desc->mute_volume; i += fade_step) {
		aw_dev_set_volume(aw_dev, i);
		usleep_range(g_fade_out_time, g_fade_out_time + 10);
	}
	if (i != desc->mute_volume) {
		aw_dev_set_volume(aw_dev, desc->mute_volume);
		usleep_range(g_fade_out_time, g_fade_out_time + 10);
	}
}

static int aw_dev_get_fade_vol_step(struct aw_device *aw_dev, u32 *fade_step)
{
	*fade_step = aw_dev->fade_step;

	return 0;
}

static int aw_dev_set_fade_vol_step(struct aw_device *aw_dev, unsigned int step)
{
	aw_dev->fade_step = step;

	return 0;
}

static int aw_dev_get_fade_time(unsigned int *time, fade_time_t fade_type)
{
	if (fade_type == AW_FADE_IN_TIME) {
		*time = g_fade_in_time;
	} else if (fade_type == AW_FADE_OUT_TIME) {
		*time = g_fade_out_time;
	} else {
		aw_pr_err("unsupported fade type:%d", fade_type);
		return -EINVAL;
	}

	return 0;
}

static int aw_dev_set_fade_time(unsigned int time, fade_time_t fade_type)
{
	if (fade_type == AW_FADE_IN_TIME) {
		g_fade_in_time = time;
	} else if (fade_type == AW_FADE_OUT_TIME) {
		g_fade_out_time = time;
	} else {
		aw_pr_err("unsupported fade type:%d", fade_type);
		return -EINVAL;
	}

	return 0;
}

static void aw_dev_pwd(struct aw_device *aw_dev, bool pwd)
{
	struct aw_pwd_desc *pwd_desc = &aw_dev->pwd_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	if (pwd)
		aw_dev->ops.aw_reg_write_bits(aw_dev, pwd_desc->reg,
					       pwd_desc->mask, pwd_desc->enable);
	else
		aw_dev->ops.aw_reg_write_bits(aw_dev, pwd_desc->reg,
					       pwd_desc->mask, pwd_desc->disable);

	aw_dev_info(aw_dev->dev, "done");
}

static void aw_dev_amppd(struct aw_device *aw_dev, bool amppd)
{
	struct aw_amppd_desc *amppd_desc = &aw_dev->amppd_desc;

	aw_dev_dbg(aw_dev->dev, "enter");
	if (amppd)
		aw_dev->ops.aw_reg_write_bits(aw_dev, amppd_desc->reg,
					       amppd_desc->mask,
					       amppd_desc->enable);
	else
		aw_dev->ops.aw_reg_write_bits(aw_dev, amppd_desc->reg,
					       amppd_desc->mask,
					       amppd_desc->disable);
	aw_dev_info(aw_dev->dev, "done");
}

static int aw_dev_set_hmute(struct aw_device *aw_dev, u32 mute)
{
	int ret = -1;
	struct aw_mute_desc *mute_desc = &aw_dev->mute_desc;

	aw_dev_dbg(aw_dev->dev, "enter");
	if (mute == AW_DEV_HMUTE_ENABLE) {
		aw_dev_fade_out(aw_dev);
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev, mute_desc->reg,
						     mute_desc->mask,
						     mute_desc->enable);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write mute enable failed");
			return ret;
		}
	} else if (mute == AW_DEV_HMUTE_DISABLE) {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev, mute_desc->reg,
						     mute_desc->mask,
						     mute_desc->disable);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write mute disable failed");
			return ret;
		}
		aw_dev_fade_in(aw_dev);
	} else {
		aw_dev_err(aw_dev->dev, "unsupported mute state: %d", mute);
		return -EINVAL;
	}

	aw_dev_info(aw_dev->dev, "done");

	return 0;
}

static int aw_dev_get_hmute(struct aw_device *aw_dev, u32 *status)
{
	u16 reg_val = 0;
	int ret = -1;
	struct aw_mute_desc *desc = &aw_dev->mute_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	ret = aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &reg_val);
	if (ret < 0)
		return ret;

	if (reg_val & (~desc->mask))
		*status = AW_DEV_HMUTE_ENABLE;
	else
		*status = AW_DEV_HMUTE_DISABLE;

	return 0;
}

static int aw_dev_get_icalk(struct aw_device *aw_dev, int16_t *icalk)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 icalkh_val = 0;
	u16 icalkl_val = 0;
	u16 icalk_val = 0;
	struct aw_efuse_check_desc *efuse_check_desc = &aw_dev->efuse_check_desc;
	struct aw_vcalb_desc *vcalb_desc = &aw_dev->vcalb_desc;

	ret = aw_dev->ops.aw_reg_read(aw_dev, vcalb_desc->icalkh_reg, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read icalkh_reg failed");
		return ret;
	}
	icalkh_val = reg_val & (~vcalb_desc->icalkh_reg_mask);

	ret = aw_dev->ops.aw_reg_read(aw_dev, vcalb_desc->icalkl_reg, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read icalkl_reg failed");
		return ret;
	}
	icalkl_val = reg_val & (~vcalb_desc->icalkl_reg_mask);

	if (efuse_check_desc->check_val == AW_EF_AND_CHECK) {
		icalk_val = (icalkh_val >> vcalb_desc->icalkh_shift) &
			    (icalkl_val >> vcalb_desc->icalkl_shift);
	} else if (efuse_check_desc->check_val == AW_EF_OR_CHECK) {
		icalk_val = (icalkh_val >> vcalb_desc->icalkh_shift) |
			    (icalkl_val >> vcalb_desc->icalkl_shift);
	} else {
		aw_dev_err(aw_dev->dev, "unsupported check type:%d",
			   efuse_check_desc->check_val);
		return -EINVAL;
	}

	if (icalk_val & (~vcalb_desc->icalk_sign_mask))
		icalk_val = icalk_val | vcalb_desc->icalk_neg_mask;

	*icalk = (int16_t)icalk_val;
	return 0;
}

static int aw_dev_get_vcalk(struct aw_device *aw_dev, int16_t *vcalk)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 vcalkh_val = 0;
	u16 vcalkl_val = 0;
	u16 vcalk_val = 0;
	struct aw_efuse_check_desc *efuse_check_desc = &aw_dev->efuse_check_desc;
	struct aw_vcalb_desc *vcalb_desc = &aw_dev->vcalb_desc;

	ret = aw_dev->ops.aw_reg_read(aw_dev, vcalb_desc->vcalkh_reg, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read vcalkh_reg failed");
		return ret;
	}
	vcalkh_val = reg_val & (~vcalb_desc->vcalkh_reg_mask);

	ret = aw_dev->ops.aw_reg_read(aw_dev, vcalb_desc->vcalkl_reg, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read vcalkl_reg failed");
		return ret;
	}
	vcalkl_val = reg_val & (~vcalb_desc->vcalkl_reg_mask);

	if (efuse_check_desc->check_val == AW_EF_AND_CHECK) {
		vcalk_val = (vcalkh_val >> vcalb_desc->vcalkh_shift) &
			    (vcalkl_val >> vcalb_desc->vcalkl_shift);
	} else if (efuse_check_desc->check_val == AW_EF_OR_CHECK) {
		vcalk_val = (vcalkh_val >> vcalb_desc->vcalkh_shift) |
			    (vcalkl_val >> vcalb_desc->vcalkl_shift);
	} else {
		aw_dev_err(aw_dev->dev, "unsupported check type:%d",
			   efuse_check_desc->check_val);
		return -EINVAL;
	}

	if (vcalk_val & (~vcalb_desc->vcalk_sign_mask))
		vcalk_val = vcalk_val | vcalb_desc->vcalk_neg_mask;

	*vcalk = (int16_t)vcalk_val;
	return 0;
}

static int aw_dev_get_internal_vcalk(struct aw_device *aw_dev, int16_t *vcalk)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 vcalkh_val = 0;
	u16 vcalkl_val = 0;
	u16 vcalk_val = 0;
	struct aw_efuse_check_desc *efuse_check_desc = &aw_dev->efuse_check_desc;
	struct aw_vcalb_desc *vcalb_desc = &aw_dev->vcalb_desc;

	ret = aw_dev->ops.aw_reg_read(aw_dev, vcalb_desc->vcalkh_dac_reg,
				       &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read vcalkh_dac_reg failed");
		return ret;
	}
	vcalkh_val = reg_val & (~vcalb_desc->vcalkh_dac_reg_mask);

	ret = aw_dev->ops.aw_reg_read(aw_dev, vcalb_desc->vcalkl_dac_reg,
				       &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read vcalkl_dac_reg failed");
		return ret;
	}
	vcalkl_val = reg_val & (~vcalb_desc->vcalkl_dac_reg_mask);

	if (efuse_check_desc->check_val == AW_EF_AND_CHECK) {
		vcalk_val = (vcalkh_val >> vcalb_desc->vcalkh_dac_shift) &
			    (vcalkl_val >> vcalb_desc->vcalkl_dac_shift);
	} else if (efuse_check_desc->check_val == AW_EF_OR_CHECK) {
		vcalk_val = (vcalkh_val >> vcalb_desc->vcalkh_dac_shift) |
			    (vcalkl_val >> vcalb_desc->vcalkl_dac_shift);
	} else {
		aw_dev_err(aw_dev->dev, "unsupported check type:%d",
			   efuse_check_desc->check_val);
		return -EINVAL;
	}

	if (vcalk_val & (~vcalb_desc->vcalk_dac_sign_mask))
		vcalk_val = vcalk_val | vcalb_desc->vcalk_dac_neg_mask;

	*vcalk = (int16_t)vcalk_val;
	return 0;
}

static int aw_dev_vsense_select(struct aw_device *aw_dev, int *vsense_select)
{
	int ret = -1;
	struct aw_vsense_desc *desc = &aw_dev->vsense_desc;
	u16 vsense_reg_val = 0;

	ret = aw_dev->ops.aw_reg_read(aw_dev, desc->vcalb_vsense_reg,
				       &vsense_reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read vsense_reg_val failed");
		return ret;
	}
	aw_dev_dbg(aw_dev->dev, "vsense_reg_val = 0x%x", vsense_reg_val);

	if (vsense_reg_val & (~desc->vcalk_vdsel_mask)) {
		*vsense_select = AW_DEV_VDSEL_VSENSE;
		aw_dev_info(aw_dev->dev, "vsense external");
		return 0;
	}

	*vsense_select = AW_DEV_VDSEL_DAC;
	aw_dev_info(aw_dev->dev, "vsense internal");
	return 0;
}

static int aw_dev_set_vcalb_to_hw(struct aw_device *aw_dev)
{
	int ret = -1;
	u16 reg_val = 0;
	int16_t icalk = 0;
	int16_t vcalk = 0;
	int32_t ical_k = 0;
	int32_t vcal_k = 0;
	int32_t vcalb = 0;
	int vsense_select = -1;
	struct aw_vcalb_desc *vcalb_desc = &aw_dev->vcalb_desc;

	if (aw_dev->vsense_desc.vcalb_vsense_reg) {
		ret = aw_dev_vsense_select(aw_dev, &vsense_select);
		if (ret < 0)
			return ret;
		aw_dev_dbg(aw_dev->dev, "vsense_select = %d", vsense_select);
	} else {
		vsense_select = aw_dev->vsense_desc.vcalb_vsense_default;
	}

	ret = aw_dev_get_icalk(aw_dev, &icalk);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get icalk failed");
		return ret;
	}
	aw_dev_info(aw_dev->dev, "icalk = %d", icalk);

	ical_k = icalk * vcalb_desc->icalk_value_factor +
		 vcalb_desc->cabl_base_value;

	if (vsense_select == AW_DEV_VDSEL_VSENSE) {
		ret = aw_dev_get_vcalk(aw_dev, &vcalk);
		if (ret < 0)
			return ret;

		vcal_k = vcalk * vcalb_desc->vcalk_value_factor +
			 vcalb_desc->cabl_base_value;
		vcalb = vcalb_desc->vcalb_accuracy *
			vcalb_desc->vscal_factor /
			vcalb_desc->iscal_factor * ical_k /
			vcal_k * vcalb_desc->init_value;

		aw_dev_info(aw_dev->dev, "icalk=%d, vcalk=%d", icalk, vcalk);
	} else if (vsense_select == AW_DEV_VDSEL_DAC) {
		ret = aw_dev_get_internal_vcalk(aw_dev, &vcalk);
		if (ret < 0)
			return ret;

		vcal_k = vcalk * vcalb_desc->vcalk_dac_value_factor +
			 vcalb_desc->cabl_base_value;
		vcalb = vcalb_desc->vcalb_accuracy *
			vcalb_desc->vscal_dac_factor /
			vcalb_desc->iscal_dac_factor * ical_k /
			vcal_k * vcalb_desc->init_value;

		aw_dev_info(aw_dev->dev, "icalk=%d, vcalk=%d", icalk, vcalk);
	} else {
		aw_dev_err(aw_dev->dev, "unsupport vsense status");
		return -EINVAL;
	}

	vcalb = vcalb >> vcalb_desc->vcalb_adj_shift;
	reg_val = (u32)vcalb;

	aw_dev_info(aw_dev->dev, "vcalb = %d, vcalb_adj = %d",
		    reg_val, vcalb_desc->init_value >>
		    vcalb_desc->vcalb_adj_shift);

	ret = aw_dev->ops.aw_reg_write(aw_dev, vcalb_desc->vcalb_reg, reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write vcalb_reg failed");
		return ret;
	}
	return 0;
}

static int aw_dev_set_vcalb(struct aw_device *aw_dev)
{
	int ret = -1;

	if (aw_dev->ops.aw_set_vcalb)
		ret = aw_dev->ops.aw_set_vcalb(aw_dev);
	else
		ret = aw_dev_set_vcalb_to_hw(aw_dev);

	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "set_vcalb failed");
		return ret;
	}

	return 0;
}

static int aw_dev_get_cali_f0_delay(struct aw_device *aw_dev)
{
	struct aw_cali_delay_desc *desc = &aw_dev->cali_desc.cali_delay_desc;
	u32 cali_delay = 0;
	int ret = -1;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->dsp_reg, &cali_delay,
				       desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read cali delay failed, ret=%d",
			   ret);
		return ret;
	}

	desc->delay = AW_CALI_DELAY_CACL(cali_delay);
	aw_dev_info(aw_dev->dev, "read cali delay: %d ms", desc->delay);

	return 0;
}

static int aw_dev_get_int_status(struct aw_device *aw_dev, u32 *int_val)
{
	int ret = -1;
	u16 reg_val = 0;

	ret = aw_dev->ops.aw_reg_read(aw_dev, aw_dev->int_desc.st_reg,
				       &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read interrupt reg fail, ret=%d",
			   ret);
		return ret;
	}

	*int_val = reg_val;
	aw_dev_dbg(aw_dev->dev, "read interrupt reg = 0x%04x", reg_val);

	return 0;
}

static int aw_dev_clear_int_status(struct aw_device *aw_dev)
{
	u32 reg_val = 0;

	/*read int status and clear*/
	aw_dev_get_int_status(aw_dev, &reg_val);
	/*make sure int status is clear*/
	aw_dev_get_int_status(aw_dev, &reg_val);
	aw_dev_info(aw_dev->dev, "done");

	return 0;
}

static int aw_dev_get_sysst_value(struct aw_device *aw_dev, u32 *value)
{
	int ret = -1;
	u16 reg_val = 0;
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;

	ret = aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read sysst failed");
		return ret;
	}

	*value = reg_val;

	return 0;
}

static bool aw_dev_get_iis_status(struct aw_device *aw_dev)
{
	int ret = -1;
	bool status = false;
	u16 reg_val = 0;
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	ret = aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read pll failed");
		return false;
	}

	if ((reg_val & desc->pll_check) == desc->pll_check)
		status = true;
	else
		status = false;

	if (!status)
		aw_dev_err(aw_dev->dev, "check pll lock fail,reg_val:0x%04x",
			   reg_val);

	return status;
}

static int aw_dev_mode1_pll_check(struct aw_device *aw_dev)
{
	int ret = -1;
	u16 i = 0;

	for (i = 0; i < AW_DEV_SYSST_CHECK_MAX; i++) {
		if (!(aw_dev_get_iis_status(aw_dev))) {
			aw_dev_err(aw_dev->dev,
				   "mode1 iis signal check error, ret:%d",
				   ret);
			usleep_range(AW_2000_US, AW_2000_US + 10);
		} else {
			return 0;
		}
	}

	return ret;
}

static int aw_dev_mode2_pll_check(struct aw_device *aw_dev)
{
	int ret = -1;
	u16 i = 0;
	u16 reg_val = 0;
	struct aw_cco_mux_desc *cco_mux_desc = &aw_dev->cco_mux_desc;

	aw_dev->ops.aw_reg_read(aw_dev, cco_mux_desc->reg, &reg_val);
	reg_val &= (~cco_mux_desc->mask);
	if (reg_val == cco_mux_desc->divider) {
		aw_dev_dbg(aw_dev->dev, "CCO_MUX is already divider");
		return ret;
	}

	/* change mode2 */
	aw_dev->ops.aw_reg_write_bits(aw_dev, cco_mux_desc->reg,
				       cco_mux_desc->mask,
				       cco_mux_desc->divider);

	for (i = 0; i < AW_DEV_SYSST_CHECK_MAX; i++) {
		if (!(aw_dev_get_iis_status(aw_dev))) {
			aw_dev_err(aw_dev->dev,
				   "mode2 iis signal check error");
			usleep_range(AW_2000_US, AW_2000_US + 10);
			ret = -EINVAL;
		} else {
			ret = 0;
			break;
		}
	}

	/* change mode1*/
	aw_dev->ops.aw_reg_write_bits(aw_dev, cco_mux_desc->reg,
				       cco_mux_desc->mask,
				       cco_mux_desc->bypass);

	if (ret == 0) {
		usleep_range(AW_2000_US, AW_2000_US + 10);
		for (i = 0; i < AW_DEV_SYSST_CHECK_MAX; i++) {
			ret = aw_dev_mode1_pll_check(aw_dev);
			if (ret < 0) {
				aw_dev_err(aw_dev->dev,
					   "mode2 switch to mode1, iis signal check error");
				usleep_range(AW_2000_US, AW_2000_US + 10);
			} else {
				break;
			}
		}
	}

	return ret;
}

static int aw_dev_syspll_check(struct aw_device *aw_dev)
{
	int ret = -1;

	ret = aw_dev_mode1_pll_check(aw_dev);
	if (ret < 0) {
		aw_dev_info(aw_dev->dev,
			    "mode1 check iis failed try switch to mode2 check");
		ret = aw_dev_mode2_pll_check(aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "mode2 check iis failed");
			return ret;
		}
	}

	return ret;
}

static int aw_dev_sysst_check(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned char i = 0;
	u16 reg_val = 0;
	unsigned int check_value = 0;
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;
	struct aw_noise_gate_en *noise_gate_desc = &aw_dev->noise_gate_en;

	check_value = desc->st_check;

	if (noise_gate_desc->reg != AW_REG_NONE) {
		aw_dev->ops.aw_reg_read(aw_dev, noise_gate_desc->reg,
					 &reg_val);
		if (reg_val & (~noise_gate_desc->noise_gate_mask))
			check_value = desc->st_check;
		else
			check_value = desc->st_sws_check;
	}

	for (i = 0; i < AW_DEV_SYSST_CHECK_MAX; i++) {
		aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &reg_val);
		if (((reg_val & (~desc->st_mask)) & check_value) !=
		    check_value) {
			aw_dev_dbg(aw_dev->dev,
				   "check fail, cnt=%d, reg_val=0x%04x",
				   i, reg_val);
			usleep_range(AW_2000_US, AW_2000_US + 10);
		} else {
			ret = 0;
			break;
		}
	}

	if (ret < 0)
		aw_dev_err(aw_dev->dev, "check fail");

	return ret;
}

static int aw_dev_get_monitor_sysint_st(struct aw_device *aw_dev)
{
	int ret = 0;
	struct aw_int_desc *desc = &aw_dev->int_desc;

	if ((desc->intst_mask) & (desc->sysint_st)) {
		aw_dev_err(aw_dev->dev, "monitor check fail:0x%04x",
			   desc->sysint_st);
		ret = -EINVAL;
	}
	desc->sysint_st = 0;

	return ret;
}

static int aw_dev_sysint_check(struct aw_device *aw_dev)
{
	int ret = 0;
	u32 reg_val = 0;
	struct aw_int_desc *desc = &aw_dev->int_desc;

	ret = aw_dev_get_int_status(aw_dev, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get int status fail ret:%d", ret);
		return ret;
	}

	if (reg_val & (desc->intst_mask)) {
		aw_dev_err(aw_dev->dev, "int check fail:0x%04x", reg_val);
		ret = -EINVAL;
	}

	return ret;
}

static void aw_dev_get_cur_mode_st(struct aw_device *aw_dev)
{
	u16 reg_val = 0;
	struct aw_profctrl_desc *profctrl_desc = &aw_dev->profctrl_desc;

	aw_dev->ops.aw_reg_read(aw_dev, aw_dev->pwd_desc.reg, &reg_val);

	if ((reg_val & (~profctrl_desc->mask)) == profctrl_desc->rcv_mode_val)
		profctrl_desc->cur_mode = AW_RCV_MODE;
	else
		profctrl_desc->cur_mode = AW_NOT_RCV_MODE;
}

static int aw_dev_set_intmask(struct aw_device *aw_dev, u32 flag)
{
	int ret = -1;
	struct aw_int_desc *desc = &aw_dev->int_desc;

	if (flag == AW_DEV_UNMASK_INT_VAL) {
		ret = aw_dev->ops.aw_reg_write(aw_dev, desc->mask_reg,
						desc->int_mask);
	} else if (flag == AW_DEV_MASK_INT_VAL) {
		ret = aw_dev->ops.aw_reg_write(aw_dev, desc->mask_reg,
						desc->mask_default);
	} else {
		aw_dev_err(aw_dev->dev, "unsupported flag : %d", flag);
		return -EINVAL;
	}

	aw_dev_info(aw_dev->dev, "done");

	return ret;
}

static int aw_dev_dsp_enable(struct aw_device *aw_dev, u32 dsp)
{
	int ret = -1;
	struct aw_dsp_en_desc *desc = &aw_dev->dsp_en_desc;

	if (dsp == AW_DEV_DSP_WORK) {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev, desc->reg,
						     desc->mask, desc->enable);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "enable dsp failed");
			return ret;
		}
	} else if (dsp == AW_DEV_DSP_BYPASS) {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev, desc->reg,
						     desc->mask, desc->disable);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "disable dsp failed");
			return ret;
		}
	} else {
		aw_dev_err(aw_dev->dev, "unsupported flag : %d", dsp);
		return -EINVAL;
	}

	aw_dev_info(aw_dev->dev, "done");
	return 0;
}

static void aw_dev_memclk_select(struct aw_device *aw_dev, unsigned char flag)
{
	struct aw_memclk_desc *desc = &aw_dev->memclk_desc;
	int ret = -1;

	if (flag == AW_DEV_MEMCLK_PLL) {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev, desc->reg,
						     desc->mask,
						     desc->mcu_hclk);
		if (ret < 0)
			aw_dev_err(aw_dev->dev, "memclk select pll failed");

	} else if (flag == AW_DEV_MEMCLK_OSC) {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev, desc->reg,
						     desc->mask, desc->osc_clk);
		if (ret < 0)
			aw_dev_err(aw_dev->dev, "memclk select OSC failed");
	} else {
		aw_dev_err(aw_dev->dev, "unknown memclk config, flag=0x%x",
			   flag);
	}

	aw_dev_info(aw_dev->dev, "done");
}

static int aw_dev_get_dsp_status(struct aw_device *aw_dev)
{
	int i = 0;
	int ret = -1;
	u16 reg_val = 0;
	struct aw_watch_dog_desc *desc = &aw_dev->watch_dog_desc;

	aw_dev_info(aw_dev->dev, "enter");

	for (i = 0; i < AW_DEV_DSP_CHECK_MAX; i++) {
		aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &reg_val);
		if (reg_val & (~desc->mask))
			return 0;

		usleep_range(AW_1000_US, AW_1000_US + 10);
	}

	return ret;
}

/******************************************************
 *
 * aw_dev update cfg
 *
 ******************************************************/

static int aw_dev_get_crc_flag(struct aw_device *aw_dev)
{
	return aw_dev->dsp_crc_st;
}

static int aw_dev_set_crc_flag(struct aw_device *aw_dev, u32 status)
{
	aw_dev->dsp_crc_st = status;
	return 0;
}

static int aw_dev_cfg_crc_check(struct aw_device *aw_dev)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 check_val = 0;
	u16 cfg_len_val = 0;
	struct aw_crc_check_desc *crc_check_desc = &aw_dev->crc_check_desc;

	/*calculate cfg_end_addr*/
	cfg_len_val = ((aw_dev->dsp_cfg_len / AW_FW_ADDR_LEN) - 1) +
		      crc_check_desc->crc_cfg_base_addr;
	aw_dev_info(aw_dev->dev, "cfg_end_addr 0x%x", cfg_len_val);

	/*write cfg_end_addr to crc_end_addr*/
	ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
					    crc_check_desc->crc_ctrl_reg,
					    crc_check_desc->crc_end_addr_mask,
					    cfg_len_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write cfg_len_val failed");
		return ret;
	}

	/*enable cfg crc check*/
	ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
					    crc_check_desc->crc_ctrl_reg,
					    crc_check_desc->crc_cfg_check_en_mask,
					    crc_check_desc->crc_cfgcheck_enable);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write crc_check_enable failed");
		return ret;
	}

	usleep_range(AW_1000_US, AW_1000_US + 10);

	/*read crc check result*/
	ret = aw_dev->ops.aw_reg_read(aw_dev, crc_check_desc->crc_check_reg,
				       &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read crc_check_reg failed");
		return ret;
	}

	check_val = (reg_val & (~crc_check_desc->crc_check_mask)) >>
		    crc_check_desc->crc_check_bits_shift;

	/*disable cfg crc check*/
	ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
					    crc_check_desc->crc_ctrl_reg,
					    crc_check_desc->crc_cfg_check_en_mask,
					    crc_check_desc->crc_cfgcheck_disable);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write crc_check_disable failed");
		return ret;
	}

	/*compare crc check result and pass value*/
	if (check_val != crc_check_desc->crc_check_pass) {
		aw_dev_err(aw_dev->dev,
			   "crc_check failed,check val %x != %x",
			   check_val, crc_check_desc->crc_check_pass);
		return -EINVAL;
	}

	return 0;
}

static int aw_dev_fw_crc_check(struct aw_device *aw_dev)
{
	int ret = -1;
	u16 reg_val = 0;
	u16 check_val = 0;
	u16 fw_len_val = 0;
	struct aw_crc_check_desc *crc_check_desc = &aw_dev->crc_check_desc;

	/*calculate fw_end_addr*/
	fw_len_val = ((aw_dev->dsp_fw_len / AW_FW_ADDR_LEN) - 1) +
		     crc_check_desc->crc_fw_base_addr;
	aw_dev_info(aw_dev->dev, "fw_end_addr 0x%x", fw_len_val);

	/*write fw_end_addr to crc_end_addr*/
	ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
					    crc_check_desc->crc_ctrl_reg,
					    crc_check_desc->crc_end_addr_mask,
					    fw_len_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write fw_len_val failed");
		return ret;
	}
	/*enable fw crc check*/
	ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
					    crc_check_desc->crc_ctrl_reg,
					    crc_check_desc->crc_fw_check_en_mask,
					    crc_check_desc->crc_fwcheck_enable);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write crc_check_enable failed");
		return ret;
	}

	usleep_range(AW_1500_US, AW_1500_US + 10);

	/*read crc check result*/
	ret = aw_dev->ops.aw_reg_read(aw_dev, crc_check_desc->crc_check_reg,
				       &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read crc_check_reg failed");
		return ret;
	}

	check_val = (reg_val & (~crc_check_desc->crc_check_mask)) >>
		    crc_check_desc->crc_check_bits_shift;

	/*disable fw crc check*/
	ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
					    crc_check_desc->crc_ctrl_reg,
					    crc_check_desc->crc_fw_check_en_mask,
					    crc_check_desc->crc_fwcheck_disable);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write crc_check_disable failed");
		return ret;
	}

	/*compare crc check result and pass value*/
	if (check_val != crc_check_desc->crc_check_pass) {
		aw_dev_err(aw_dev->dev,
			   "crc_check failed,check val %x != %x",
			   check_val, crc_check_desc->crc_check_pass);
		return -EINVAL;
	}

	return 0;
}

static int aw_dev_hw_crc_check(struct aw_device *aw_dev)
{
	int ret = -1;
	struct aw_crc_check_desc *crc_check_desc = &aw_dev->crc_check_desc;

	/*force RAM clock on*/
	ret = aw_dev->ops.aw_reg_write_bits(aw_dev,
					    crc_check_desc->ram_clk_reg,
					    crc_check_desc->ram_clk_mask,
					    crc_check_desc->ram_clk_on);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write ram_clk_on failed");
		return ret;
	}

	ret = aw_dev_fw_crc_check(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, " fw_crc_check failed");
		goto ram_clk_off;
	}

	ret = aw_dev_cfg_crc_check(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, " cfg_crc_check failed");
		goto ram_clk_off;
	}

	/*write bin value to crc reg when fw & cfg crc check pass*/
	ret = aw_dev->ops.aw_reg_write(aw_dev, crc_check_desc->crc_ctrl_reg,
					crc_check_desc->crc_init_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write crc_init_val failed");
		goto ram_clk_off;
	}

	/*disable force RAM clock*/
	aw_dev->ops.aw_reg_write_bits(aw_dev, crc_check_desc->ram_clk_reg,
				       crc_check_desc->ram_clk_mask,
				       crc_check_desc->ram_clk_off);

	return 0;
ram_clk_off:
	aw_dev->ops.aw_reg_write_bits(aw_dev, crc_check_desc->ram_clk_reg,
				       crc_check_desc->ram_clk_mask,
				       crc_check_desc->ram_clk_off);
	return ret;
}

static int aw_dev_crc_check(struct aw_device *aw_dev)
{
	int ret = -1;

	if (aw_dev_get_crc_flag(aw_dev) == AW_DSP_CRC_BYPASS) {
		aw_dev_info(aw_dev->dev,
			    "CRC Bypass in driver debug process");
		return 0;
	}
	if (aw_dev->crc_type == AW_SW_CRC_CHECK) {
		if (aw_dev->ops.aw_sw_crc_check) {
			ret = aw_dev->ops.aw_sw_crc_check(aw_dev);
			if (ret < 0) {
				aw_dev_err(aw_dev->dev, "dsp crc check failed");
				return ret;
			}
		} else {
			aw_dev_err(aw_dev->dev, "sw_crc_check ops is NULL");
			return ret;
		}
	} else if (aw_dev->crc_type == AW_HW_CRC_CHECK) {
		ret = aw_dev_hw_crc_check(aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "hw_crc_check failed");
			return ret;
		}
	}
	return 0;
}

static int aw_dev_get_firmware_ver(struct aw_device *aw_dev)
{
	int ret = -1;
	struct aw_fw_ver_desc *fw_ver_desc = &aw_dev->fw_ver_desc;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, fw_ver_desc->version_reg,
				       &aw_dev->fw_ver, fw_ver_desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read firmware version failed, ret=%d",
			   ret);
		return ret;
	}

	return 0;
}

static int aw_dev_reg_container_update(struct aw_device *aw_dev, u8 *data,
					u32 len)
{
	int i, ret = -1;
	u8 reg_addr = 0;
	u16 reg_val = 0;
	u16 read_vol = 0;
	struct aw_int_desc *int_desc = &aw_dev->int_desc;
	struct aw_volume_desc *vol_desc = &aw_dev->volume_desc;
	u16 *reg_data = NULL;
	int data_len = 0;

	aw_dev_dbg(aw_dev->dev, "enter");
	reg_data = (u16 *)data;
	data_len = len >> 1;

	if (data_len % 2 != 0) {
		aw_dev_err(aw_dev->dev, "data len:%d unsupported", data_len);
		return -EINVAL;
	}

	for (i = 0; i < data_len; i += 2) {
		reg_addr = reg_data[i];
		reg_val = reg_data[i + 1];
		aw_dev_dbg(aw_dev->dev, "reg = 0x%02x, val = 0x%04x",
			   reg_addr, reg_val);
		if (reg_addr == int_desc->mask_reg) {
			int_desc->int_mask = reg_val;
			reg_val = int_desc->mask_default;
		}
		if (reg_addr == aw_dev->mute_desc.reg) {
			reg_val &= aw_dev->mute_desc.mask;
			reg_val |= aw_dev->mute_desc.enable;
		}

		if (reg_addr == aw_dev->pwd_desc.reg) {
			reg_val &= aw_dev->pwd_desc.mask;
			reg_val |= aw_dev->pwd_desc.enable;
		}

		if (reg_addr == aw_dev->dsp_crc_desc.ctl_reg) {
			reg_val &= aw_dev->dsp_crc_desc.ctl_mask;
		}

		if (reg_addr == aw_dev->tx_en_desc.tx_en_reg) {
			/*get bin value*/
			aw_dev->txen_st = reg_val &
					  (~aw_dev->tx_en_desc.tx_en_mask);
			aw_dev_dbg(aw_dev->dev, "txen_st=0x%04x",
				   aw_dev->txen_st);

			/*close tx*/
			reg_val &= aw_dev->tx_en_desc.tx_en_mask;
			reg_val |= aw_dev->tx_en_desc.tx_disable;
		}

		if (reg_addr == aw_dev->volume_desc.reg) {
			read_vol = (reg_val & (~aw_dev->volume_desc.mask)) >>
				    aw_dev->volume_desc.shift;
			aw_dev->volume_desc.init_volume =
				aw_dev->ops.aw_reg_val_to_db(read_vol);
		}

		if (reg_addr == aw_dev->efuse_check_desc.reg) {
			if ((reg_val & (~aw_dev->efuse_check_desc.mask)) ==
			    aw_dev->efuse_check_desc.or_val)
				aw_dev->efuse_check_desc.check_val =
					AW_EF_OR_CHECK;
			else
				aw_dev->efuse_check_desc.check_val =
					AW_EF_AND_CHECK;
		}

		if (reg_addr == aw_dev->crc_check_desc.crc_ctrl_reg)
			aw_dev->crc_check_desc.crc_init_val = reg_val;

		if (reg_addr == aw_dev->vcalb_desc.vcalb_reg) {
			aw_dev->vcalb_desc.init_value = reg_val;
			continue;
		}

		if (reg_addr == aw_dev->cali_desc.hw_cali_re_desc.hbits_reg) {
			aw_dev->cali_desc.hw_cali_re_desc.re_hbits = reg_val;
			continue;
		}

		if (reg_addr == aw_dev->cali_desc.hw_cali_re_desc.lbits_reg) {
			aw_dev->cali_desc.hw_cali_re_desc.re_lbits = reg_val;
			continue;
		}

		if (reg_addr == aw_dev->dsp_en_desc.reg) {
			if (reg_val & (~aw_dev->dsp_en_desc.mask))
				aw_dev->dsp_cfg = AW_DEV_DSP_BYPASS;
			else
				aw_dev->dsp_cfg = AW_DEV_DSP_WORK;

			reg_val &= aw_dev->dsp_en_desc.mask;
			reg_val |= aw_dev->dsp_en_desc.disable;
		}

		if (reg_addr == aw_dev->dither_desc.reg) {
			aw_dev->dither_st = reg_val &
					     (~aw_dev->dither_desc.mask);
			aw_dev_info(aw_dev->dev, "dither_st=0x%04x",
				    aw_dev->dither_st);
		}

		ret = aw_dev->ops.aw_reg_write(aw_dev, reg_addr, reg_val);
		if (ret < 0)
			break;
	}

	aw_dev_pwd(aw_dev, false);
	usleep_range(AW_1000_US, AW_1000_US + 10);

	aw_spin_hold_chan_params(&aw_dev->spin_desc, AW_REG_SPIN_ST);

	aw_dev_get_cur_mode_st(aw_dev);

	if (aw_dev->cur_prof != aw_dev->set_prof) {
		/*clear control volume when PA change profile*/
		vol_desc->ctl_volume = 0;
	} else {
		/*keep control volume when PA start with sync mode*/
		aw_dev_set_volume(aw_dev, vol_desc->ctl_volume);
	}

	/*keep min volume*/
	if (aw_dev->fade_en)
		aw_dev_set_volume(aw_dev, vol_desc->mute_volume);

	aw_monitor_update_st(&aw_dev->monitor_desc, AW_HW_MONITOR_ST);

	aw_dev_dbg(aw_dev->dev, "exit");

	return ret;
}

static int aw_dev_reg_update(struct aw_device *aw_dev, u8 *data, u32 len)
{
	aw_dev_dbg(aw_dev->dev, "reg len:%d", len);

	if (len && (data != NULL)) {
		aw_dev_reg_container_update(aw_dev, data, len);
	} else {
		aw_dev_err(aw_dev->dev, "reg data is null or len is 0");
		return -EPERM;
	}

	return 0;
}

static int aw_dev_dsp_container_update(struct aw_device *aw_dev, u8 *data,
					u32 len, u16 base)
{
	aw_dev_dbg(aw_dev->dev, "enter");

	aw_dev->ops.aw_dsp_writes(aw_dev, data, len, base);

	aw_dev_dbg(aw_dev->dev, "exit");

	return 0;
}

static int aw_dev_dsp_fw_update(struct aw_device *aw_dev, u8 *data, u32 len)
{
	struct aw_dsp_mem_desc *dsp_mem_desc = &aw_dev->dsp_mem_desc;

	aw_dev_info(aw_dev->dev, "dsp firmware len:%d", len);

	if (len && (data != NULL)) {
		aw_dev_dsp_container_update(aw_dev, data, len,
					     dsp_mem_desc->dsp_fw_base_addr);
		aw_dev->dsp_fw_len = len;
	} else {
		aw_dev_err(aw_dev->dev,
			   "dsp firmware data is null or len is 0");
		return -EPERM;
	}

	return 0;
}

static void aw_dev_init_vmax_update(struct aw_device *aw_dev, backup_sec_t flag)
{
	if (flag == AW_RECOVERY_SEC_DATA)
		aw_monitor_update_st(&aw_dev->monitor_desc, AW_SET_INIT_VMAX);
	else if (flag == AW_RECORD_SEC_DATA)
		aw_monitor_update_st(&aw_dev->monitor_desc, AW_GET_INIT_VMAX);
	else
		aw_dev_err(aw_dev->dev, "unsupported type:%d", flag);
}

static int aw_dev_init_vcalb_update(struct aw_device *aw_dev, backup_sec_t flag)
{
	int ret = -1;
	struct aw_vcalb_desc *vcalb_desc = &aw_dev->vcalb_desc;

	if (aw_dev->ops.aw_init_vcalb_update) {
		ret = aw_dev->ops.aw_init_vcalb_update(aw_dev, flag);
		if (ret < 0)
			aw_dev_err(aw_dev->dev, "write vcalb_reg failed");
		return ret;
	}

	if (flag == AW_RECOVERY_SEC_DATA) {
		ret = aw_dev->ops.aw_reg_write(aw_dev, vcalb_desc->vcalb_reg,
						vcalb_desc->last_value);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write vcalb_reg failed");
			return ret;
		}
	} else if (flag == AW_RECORD_SEC_DATA) {
		ret = aw_dev->ops.aw_reg_read(aw_dev, vcalb_desc->vcalb_reg,
					       &vcalb_desc->last_value);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "write vcalb_reg failed");
			return ret;
		}
	} else {
		aw_dev_err(aw_dev->dev, "unsupported type:%d", flag);
		return -EINVAL;
	}

	return 0;
}

static int aw_dev_crc_realtime_get(struct aw_device *aw_dev, u32 *status)
{
	int ret = -1;
	struct aw_crc_check_realtime_desc *desc = &aw_dev->crc_check_realtime_desc;
	unsigned int crc_realtime_val = 0;

	ret = aw_dev->ops.aw_dsp_read(aw_dev, desc->addr, &crc_realtime_val,
				       desc->data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get crc_realtime failed");
		return ret;
	}

	if (crc_realtime_val & (~desc->mask))
		*status = aw_dev->crc_check_realtime_desc.enable;
	else
		*status = aw_dev->crc_check_realtime_desc.disable;

	aw_dev_dbg(aw_dev->dev, "get %d", *status);

	return 0;
}

static int aw_dev_crc_realtime_set(struct aw_device *aw_dev, u32 enable)
{
	int ret = -1;
	struct aw_crc_check_realtime_desc *crc_desc =
		&aw_dev->crc_check_realtime_desc;

	aw_dev_dbg(aw_dev->dev, "set %d", enable);

	if (enable) {
		ret = aw_dev->ops.aw_dsp_write_bits(aw_dev, crc_desc->addr,
						     crc_desc->mask,
						     crc_desc->enable,
						     crc_desc->data_type);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "set enable failed");
			return ret;
		}
	} else {
		ret = aw_dev->ops.aw_dsp_write_bits(aw_dev, crc_desc->addr,
						     crc_desc->mask,
						     crc_desc->disable,
						     crc_desc->data_type);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "set disable failed");
			return ret;
		}
	}

	return 0;
}

int aw_crc_realtime_check(struct aw_device *aw_dev, u32 *status)
{
	int ret = -1;
	u16 sysst_val;
	u32 crc_check_val;
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;
	struct aw_int_desc *int_desc = &aw_dev->int_desc;
	struct aw_crc_check_realtime_desc *crc_desc =
		&aw_dev->crc_check_realtime_desc;

	if (aw_dev_get_crc_flag(aw_dev) == AW_DSP_CRC_BYPASS) {
		int_desc->int_mask = int_desc->mask_default;
		aw_dev_info(aw_dev->dev,
			    "CRC Bypass in driver debug process");
		return 0;
	}

	ret = aw_dev->ops.aw_reg_read(aw_dev, desc->reg, &sysst_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read reg0x%x failed", desc->reg);
		return ret;
	}

	ret = aw_dev->ops.aw_dsp_read(aw_dev, crc_desc->check_addr,
				       &crc_check_val,
				       crc_desc->check_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get crc_check_val failed");
		return ret;
	}

	if (((sysst_val & desc->dsp_mask) != desc->dsp_check) &&
	    (crc_check_val & (~crc_desc->check_mask)) ==
		aw_dev->crc_check_realtime_desc.check_abnormal) {
		*status = AW_REALTIME_CRC_CHECK_ABNORMAL;
	} else {
		*status = AW_REALTIME_CRC_CHECK_NORMAL;
	}

	aw_dev_dbg(aw_dev->dev, "check status %d", *status);

	return 0;
}

static int aw_set_dsp_noise_gate_params(struct aw_device *aw_dev, u32 enable)
{
	int ret = -1;
	struct aw_dsp_ng_desc *dsp_ng_desc = &aw_dev->dsp_ng_desc;

	if (dsp_ng_desc->reg == AW_REG_NONE) {
		aw_dev_dbg(aw_dev->dev, "unsupport");
		return 0;
	}

	aw_dev_dbg(aw_dev->dev, "%s dsp noise gate",
		   enable ? "enable" : "disable");
	if (enable) {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev, dsp_ng_desc->reg,
						     dsp_ng_desc->mask,
						     dsp_ng_desc->enable);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "set enable failed");
			return ret;
		}
	} else {
		ret = aw_dev->ops.aw_reg_write_bits(aw_dev, dsp_ng_desc->reg,
						     dsp_ng_desc->mask,
						     dsp_ng_desc->disable);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "set disable failed");
			return ret;
		}
	}

	return 0;
}

static int aw_set_dsp_lowe_power_params(struct aw_device *aw_dev, u32 val)
{
	int ret = -1;
	struct aw_dsp_ng_desc *dsp_ng_desc = &aw_dev->dsp_ng_desc;

	if (dsp_ng_desc->dsp_lp_reg == AW_REG_NONE) {
		aw_dev_dbg(aw_dev->dev, "unsupport");
		return 0;
	}

	aw_dev_dbg(aw_dev->dev, "set 0x%x", val);
	ret = aw_dev->ops.aw_dsp_write_bits(aw_dev, dsp_ng_desc->dsp_lp_reg,
					     dsp_ng_desc->dsp_lp_mask, val,
					     dsp_ng_desc->dsp_lp_data_type);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "set val failed");
		return ret;
	}

	return 0;
}

static void aw_dev_backup_sec_record(struct aw_device *aw_dev)
{
	aw_dev_init_vcalb_update(aw_dev, AW_RECORD_SEC_DATA);
	aw_dev_init_re_update(&aw_dev->cali_desc, AW_RECORD_SEC_DATA);
	aw_dev_init_vmax_update(aw_dev, AW_RECORD_SEC_DATA);
	aw_dev_crc_realtime_get(aw_dev,
				&aw_dev->crc_check_realtime_desc.init_switch);
}

static void aw_dev_backup_sec_recovery(struct aw_device *aw_dev)
{
	aw_dev_init_vcalb_update(aw_dev, AW_RECOVERY_SEC_DATA);
	aw_dev_init_re_update(&aw_dev->cali_desc, AW_RECOVERY_SEC_DATA);
	aw_dev_init_vmax_update(aw_dev, AW_RECOVERY_SEC_DATA);
}

static int aw_dev_dsp_cfg_update(struct aw_device *aw_dev, u8 *data, u32 len)
{
	struct aw_dsp_mem_desc *dsp_mem_desc = &aw_dev->dsp_mem_desc;

	aw_dev_info(aw_dev->dev, "dsp config len:%d", len);

	if (len && (data != NULL)) {
		aw_dev_dsp_container_update(aw_dev, data, len,
					     dsp_mem_desc->dsp_cfg_base_addr);
		aw_dev->dsp_cfg_len = len;

		aw_cali_get_ra(&aw_dev->cali_desc);
		aw_dev_get_cali_f0_delay(aw_dev);
		aw_dev_get_firmware_ver(aw_dev);
		aw_dev_info(aw_dev->dev, "fw_ver: [%04x]", aw_dev->fw_ver);
		aw_monitor_update_st(&aw_dev->monitor_desc, AW_DSP_MONITOR_ST);
		aw_dev_backup_sec_record(aw_dev);

	} else {
		aw_dev_err(aw_dev->dev,
			   "dsp config data is null or len is 0");
		return -EPERM;
	}

	return 0;
}

static int aw_dev_sram_check(struct aw_device *aw_dev)
{
	int ret = -1;
	u32 reg_val = 0;
	struct aw_dsp_mem_desc *dsp_mem_desc = &aw_dev->dsp_mem_desc;

	/*read dsp_rom_check_reg*/
	aw_dev->ops.aw_dsp_read(aw_dev, dsp_mem_desc->dsp_rom_check_reg,
				 &reg_val, AW_DSP_16_DATA);
	if (dsp_mem_desc->dsp_rom_check_data != reg_val) {
		aw_dev_err(aw_dev->dev,
			   "check reg 0x40 failed, read[0x%x] does not match [0x%x]",
			   reg_val, dsp_mem_desc->dsp_rom_check_data);
		goto error;
	}

	/*write dsp_cfg_base_addr*/
	aw_dev->ops.aw_dsp_write(aw_dev, dsp_mem_desc->dsp_cfg_base_addr,
				  AW_DSP_ODD_NUM_BIT_TEST, AW_DSP_16_DATA);

	/*read dsp_cfg_base_addr*/
	aw_dev->ops.aw_dsp_read(aw_dev, dsp_mem_desc->dsp_cfg_base_addr,
				 &reg_val, AW_DSP_16_DATA);
	if (reg_val != AW_DSP_ODD_NUM_BIT_TEST) {
		aw_dev_err(aw_dev->dev,
			   "check dsp cfg failed, read[0x%x] does not match write[0x%x]",
			   reg_val, AW_DSP_ODD_NUM_BIT_TEST);
		goto error;
	}

	return 0;

error:
	return ret;
}

static int __maybe_unused aw_init_value_update(struct aw_device *aw_dev)
{
	int ret = -1;
	struct aw_vcalb_desc *vcalb_desc = &aw_dev->vcalb_desc;
	struct aw_hw_cali_re_desc *hw_cali_re_desc =
		&aw_dev->cali_desc.hw_cali_re_desc;

	ret = aw_dev->ops.aw_reg_write(aw_dev, vcalb_desc->vcalb_reg,
					vcalb_desc->init_value);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write vcalb_reg failed");
		return ret;
	}

	ret = aw_dev->ops.aw_reg_write(aw_dev, hw_cali_re_desc->hbits_reg,
					hw_cali_re_desc->re_hbits);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write cali re failed");
		return ret;
	}

	ret = aw_dev->ops.aw_reg_write(aw_dev, hw_cali_re_desc->lbits_reg,
					hw_cali_re_desc->re_lbits);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write cali re failed");
		return ret;
	}

	return 0;
}

int aw_device_fw_update(struct aw_device *aw_dev, bool up_dsp_fw_en,
			bool force_up_en)
{
	int ret = -1;
	struct aw_prof_desc *set_prof_desc = NULL;
	struct aw_sec_data_desc *sec_desc = NULL;
	char *prof_name = NULL;

	if ((aw_dev->cur_prof == aw_dev->set_prof) &&
	    (force_up_en == AW_FORCE_UPDATE_OFF)) {
		aw_dev_info(aw_dev->dev, "scene no change, not update");
		return 0;
	}

	if (aw_dev->fw_status == AW_DEV_FW_FAILED) {
		aw_dev_err(aw_dev->dev, "fw status[%d] error",
			   aw_dev->fw_status);
		return -EPERM;
	}

	prof_name = aw_dev_get_prof_name(aw_dev, aw_dev->set_prof);
	if (!prof_name)
		return -ENOMEM;

	aw_dev_info(aw_dev->dev, "start update %s", prof_name);

	ret = aw_dev_get_prof_data(aw_dev, aw_dev->set_prof, &set_prof_desc);
	if (ret < 0)
		return ret;

	/*update reg*/
	sec_desc = set_prof_desc->sec_desc;
	ret = aw_dev_reg_update(aw_dev, sec_desc[AW_DATA_TYPE_REG].data,
				sec_desc[AW_DATA_TYPE_REG].len);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "update reg failed");
		return ret;
	}

	aw_dev_set_hmute(aw_dev, AW_DEV_HMUTE_ENABLE);

	if (aw_dev->dsp_cfg == AW_DEV_DSP_WORK)
		aw_dev_dsp_enable(aw_dev, AW_DEV_DSP_BYPASS);

	aw_dev_memclk_select(aw_dev, AW_DEV_MEMCLK_OSC);

	ret = aw_dev_sram_check(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "check sram failed");
		goto error;
	}

	aw_dev_backup_sec_recovery(aw_dev);

	if (up_dsp_fw_en) {
		/*update dsp firmware*/
		ret = aw_dev_dsp_fw_update(aw_dev,
					    sec_desc[AW_DATA_TYPE_DSP_FW].data,
					    sec_desc[AW_DATA_TYPE_DSP_FW].len);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "update dsp fw failed");
			goto error;
		}
	}

	/*update dsp config*/
	ret = aw_dev_dsp_cfg_update(aw_dev,
				     sec_desc[AW_DATA_TYPE_DSP_CFG].data,
				     sec_desc[AW_DATA_TYPE_DSP_CFG].len);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "update dsp cfg failed");
		goto error;
	}

	aw_dev_memclk_select(aw_dev, AW_DEV_MEMCLK_PLL);

	aw_dev->cur_prof = aw_dev->set_prof;

	aw_dev_info(aw_dev->dev, "load %s done", prof_name);
	return 0;

error:
	aw_dev_memclk_select(aw_dev, AW_DEV_MEMCLK_PLL);

	return ret;
}

static int aw_dev_dsp_check(struct aw_device *aw_dev)
{
	int ret = -1;
	u16 i = 0;

	aw_dev_dbg(aw_dev->dev, "enter");

	if (aw_dev->dsp_cfg == AW_DEV_DSP_BYPASS) {
		aw_dev_dbg(aw_dev->dev, "dsp bypass");
		return 0;
	} else if (aw_dev->dsp_cfg == AW_DEV_DSP_WORK) {
		aw_dev_dsp_enable(aw_dev, AW_DEV_DSP_BYPASS);
		aw_dev_dsp_enable(aw_dev, AW_DEV_DSP_WORK);
		usleep_range(AW_1000_US, AW_1000_US + 10);
		for (i = 0; i < AW_DEV_DSP_CHECK_MAX; i++) {
			ret = aw_dev_get_dsp_status(aw_dev);
			if (ret < 0) {
				aw_dev_err(aw_dev->dev,
					   "dsp wdt status error=%d", ret);
				usleep_range(AW_2000_US, AW_2000_US + 10);
			} else {
				return 0;
			}
		}
	} else {
		aw_dev_err(aw_dev->dev, "unknown dsp cfg=%d",
			   aw_dev->dsp_cfg);
		return -EINVAL;
	}

	return -EINVAL;
}

static void aw_dev_cali_re_update(struct aw_cali_desc *cali_desc)
{
	struct aw_device *aw_dev = container_of(cali_desc, struct aw_device,
						cali_desc);

	if (aw_dev->cali_desc.cali_re < aw_dev->re_range.re_max &&
	    aw_dev->cali_desc.cali_re > aw_dev->re_range.re_min)
		aw_cali_set_cali_re(&aw_dev->cali_desc);
	else
		aw_dev_err(aw_dev->dev, "cali_re:%d out of range, no set",
			   aw_dev->cali_desc.cali_re);
}

static bool aw_dev_get_pll_wdt_status(struct aw_device *aw_dev)
{
	int ret = -1;

	ret = aw_dev_syspll_check(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "pll abnormal");
		return false;
	}

	ret = aw_dev_get_dsp_status(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "dsp not work");
		return false;
	}

	return true;
}

static int aw_dev_set_channel_mode(struct aw_device *aw_pa, u32 spin_angle)
{
	int ret = -1;
	struct aw_chansel_desc *chansel_desc = &aw_pa->chansel_desc;
	struct aw_spin_ch *spin_ch = &aw_pa->spin_desc.spin_table[spin_angle];

	ret = aw_pa->ops.aw_reg_write_bits(aw_pa, chansel_desc->rxchan_reg,
					    chansel_desc->rxchan_mask,
					    spin_ch->rx_val);
	if (ret < 0) {
		aw_dev_err(aw_pa->dev, "set rx failed");
		return ret;
	}
	aw_dev_dbg(aw_pa->dev, "set channel mode done!");

	return 0;
}

static void aw_dev_set_dither(struct aw_device *aw_dev, bool dither)
{
	struct aw_dither_desc *dither_desc = &aw_dev->dither_desc;

	aw_dev_dbg(aw_dev->dev, "enter, dither: %d", dither);

	if (dither_desc->reg == AW_REG_NONE)
		return;

	if (dither)
		aw_dev->ops.aw_reg_write_bits(aw_dev, dither_desc->reg,
					       dither_desc->mask,
					       dither_desc->enable);
	else
		aw_dev->ops.aw_reg_write_bits(aw_dev, dither_desc->reg,
					       dither_desc->mask,
					       dither_desc->disable);

	aw_dev_info(aw_dev->dev, "done");
}

static int aw_dev_set_params(struct aw_device *aw_dev, dev_params_t type,
			     u32 *params, u32 len)
{
	if (!params || len == 0) {
		aw_dev_err(aw_dev->dev, "input params error");
		return -EINVAL;
	}

	switch (type) {
	case AW_DEV_VOLUME_PARAMS:
		return aw_dev_set_volume(aw_dev, params[0]);
	case AW_DEV_FADE_STEP_PARAMS:
		return aw_dev_set_fade_vol_step(aw_dev, params[0]);
	case AW_DEV_FADE_IN_TIME_PARAMS:
		return aw_dev_set_fade_time(params[0], AW_FADE_IN_TIME);
	case AW_DEV_FADE_OUT_TIME_PARAMS:
		return aw_dev_set_fade_time(params[0], AW_FADE_OUT_TIME);
	case AW_DEV_DSP_PARAMS:
		return aw_dev_dsp_enable(aw_dev, params[0]);
	case AW_DEV_HMUTE_PARAMS:
		return aw_dev_set_hmute(aw_dev, params[0]);
	case AW_DEV_INT_PARAMS:
		return aw_dev_set_intmask(aw_dev, params[0]);
	case AW_DEV_CHANNEL_MODE_PARAMS:
		return aw_dev_set_channel_mode(aw_dev, params[0]);
	case AW_DEV_CRC_FLAG_PARAMS:
		return aw_dev_set_crc_flag(aw_dev, params[0]);
	case AW_DEV_REALTIME_CRC_SET_PARAMS:
		return aw_dev_crc_realtime_set(aw_dev, params[0]);
	case AW_DEV_HOLD_SPIN_PARAMS:
		return aw_spin_hold_chan_params(&aw_dev->spin_desc, params[0]);
	case AW_DEV_DSP_NOISE_GATE_PARAMS:
		return aw_set_dsp_noise_gate_params(aw_dev, params[0]);
	case AW_DEV_DSP_LOW_POWER_PARAMS:
		return aw_set_dsp_lowe_power_params(aw_dev, params[0]);
	default:
		aw_dev_err(aw_dev->dev, "unsupported type:%d", type);
		return -EINVAL;
	}
}

static int aw_dev_get_params(struct aw_device *aw_dev, dev_params_t type,
			     u32 *params, u32 len)
{
	if (!params || len == 0) {
		aw_dev_err(aw_dev->dev, "input params error");
		return -EINVAL;
	}

	switch (type) {
	case AW_DEV_VOLUME_PARAMS:
		aw_dev_get_volume(aw_dev, params);
		break;
	case AW_DEV_FADE_STEP_PARAMS:
		aw_dev_get_fade_vol_step(aw_dev, params);
		break;
	case AW_DEV_FADE_IN_TIME_PARAMS:
		aw_dev_get_fade_time(params, AW_FADE_IN_TIME);
		break;
	case AW_DEV_FADE_OUT_TIME_PARAMS:
		aw_dev_get_fade_time(params, AW_FADE_OUT_TIME);
		break;
	case AW_DEV_INT_PARAMS:
		aw_dev_get_int_status(aw_dev, params);
		break;
	case AW_DEV_HMUTE_PARAMS:
		aw_dev_get_hmute(aw_dev, params);
		break;
	case AW_DEV_SYSST_PARAMS:
		aw_dev_get_sysst_value(aw_dev, params);
		break;
	case AW_DEV_REALTIME_CRC_GET_PARAMS:
		aw_dev_crc_realtime_get(aw_dev, params);
		break;
	case AW_DEV_CALI_RE_PARAMS:
		aw_cali_get_cali_re(&aw_dev->cali_desc, params,
				    AW_GET_RE_FROM_PA);
		break;
	case AW_DEV_BIN_RE_PARAMS:
		aw_cali_get_cali_re(&aw_dev->cali_desc, params,
				    AW_GET_RE_FROM_BIN);
		break;
	case AW_DEV_TE_PARAMS:
		aw_cali_get_te(&aw_dev->cali_desc, params);
		break;
	default:
		aw_dev_err(aw_dev->dev, "unsupported type:%d", type);
		return -EINVAL;
	}

	return 0;
}

int aw_device_params(struct aw_device *aw_dev, dev_params_t params_type,
		     void *data, u32 size, params_option_t params_ops)
{
	int ret = 0;

	if (params_ops == AW_SET_DEV_PARAMS) {
		ret = aw_dev_set_params(aw_dev, params_type, (u32 *)data,
					size);
	} else if (params_ops == AW_GET_DEV_PARAMS) {
		ret = aw_dev_get_params(aw_dev, params_type, (u32 *)data,
					size);
	} else {
		aw_dev_err(aw_dev->dev, "unsupported params_ops:%d",
			   params_ops);
		ret = -EINVAL;
	}

	return ret;
}

static bool aw_dev_get_status(struct aw_device *aw_dev, dev_status_t type)
{
	switch (type) {
	case AW_DEV_PLL_WDT_STATUS:
		return aw_dev_get_pll_wdt_status(aw_dev);
	default:
		aw_dev_err(aw_dev->dev, "unsupported type:%d", type);
		return false;
	}
}

static void aw_dev_set_status(struct aw_device *aw_dev, dev_status_t type)
{
	switch (type) {
	case AW_DEV_CLEAR_INT_STATUS:
		aw_dev_clear_int_status(aw_dev);
		break;
	case AW_DEV_CHECK_SPIN_MODE_STATUS:
		aw_spin_check_mode(&aw_dev->spin_desc);
		break;
	default:
		aw_dev_err(aw_dev->dev, "unsupported type:%d", type);
		return;
	}
}

bool aw_device_status(struct aw_device *aw_dev, dev_status_t type,
		      status_option_t status_ops)
{
	bool ret = false;

	if (status_ops == AW_SET_DEV_STATUS) {
		aw_dev_set_status(aw_dev, type);
		ret = true;
	} else if (status_ops == AW_GET_DEV_STATUS) {
		ret = aw_dev_get_status(aw_dev, type);
	} else {
		aw_dev_err(aw_dev->dev, "unsupported status_ops:%d",
			   status_ops);
		ret = false;
	}

	return ret;
}

int aw_device_start(struct aw_device *aw_dev)
{
	int ret = -1;
	struct aw_dither_desc *dither_desc = &aw_dev->dither_desc;

	aw_dev_info(aw_dev->dev, "enter");

	if (aw_dev->status == AW_DEV_PW_ON) {
		aw_dev_info(aw_dev->dev, "already power on");
		return 0;
	}

	aw_dev_set_dither(aw_dev, false);

	/*power on*/
	aw_dev_pwd(aw_dev, false);
	usleep_range(AW_2000_US, AW_2000_US + 10);

	ret = aw_dev_syspll_check(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "pll check failed cannot start");
		aw_dev_reg_dump(aw_dev);
		goto pll_check_fail;
	}

	/*amppd on*/
	aw_dev_amppd(aw_dev, false);
	usleep_range(AW_1000_US, AW_1000_US + 50);

	/*check i2s status*/
	ret = aw_dev_sysst_check(aw_dev);
	if (ret < 0) {
		/*check failed*/
		aw_dev_reg_dump(aw_dev);
		goto sysst_check_fail;
	}

	if (aw_dev->dsp_cfg == AW_DEV_DSP_WORK) {
		aw_dev_backup_sec_recovery(aw_dev);
		ret = aw_dev_crc_check(aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "crc check failed");
			aw_dev_reg_dump(aw_dev);
			goto crc_check_fail;
		}

		/*dsp bypass*/
		aw_dev_dsp_enable(aw_dev, AW_DEV_DSP_BYPASS);

		aw_dev_set_vcalb(aw_dev);
		aw_dev_cali_re_update(&aw_dev->cali_desc);

		ret = aw_dev_dsp_check(aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "check dsp status failed");
			aw_dev_reg_dump(aw_dev);
			goto dsp_check_fail;
		}
	} else {
		aw_dev_dbg(aw_dev->dev, "start pa with dsp bypass");
	}

	/*enable tx feedback*/
	if (aw_dev->ops.aw_i2s_tx_enable) {
		if (aw_dev->txen_st)
			aw_dev->ops.aw_i2s_tx_enable(aw_dev, true);
	}

	if (aw_dev->dither_st == dither_desc->enable)
		aw_dev_set_dither(aw_dev, true);

	/*close mute*/
	if (aw_cali_check_result(&aw_dev->cali_desc))
		aw_dev_set_hmute(aw_dev, AW_DEV_HMUTE_DISABLE);
	else
		aw_dev_set_hmute(aw_dev, AW_DEV_HMUTE_ENABLE);

	/*clear inturrupt*/
	aw_dev_clear_int_status(aw_dev);
	/*set inturrupt mask*/
	aw_dev_set_intmask(aw_dev, AW_DEV_UNMASK_INT_VAL);

	aw_monitor_start(&aw_dev->monitor_desc);

	aw_dev->status = AW_DEV_PW_ON;

	aw_dev_info(aw_dev->dev, "done");

	return 0;

dsp_check_fail:
crc_check_fail:
	aw_dev_dsp_enable(aw_dev, AW_DEV_DSP_BYPASS);
sysst_check_fail:
	/*clear interrupt*/
	aw_dev_clear_int_status(aw_dev);
	aw_dev_amppd(aw_dev, true);
pll_check_fail:
	aw_dev_pwd(aw_dev, true);
	aw_dev->status = AW_DEV_PW_OFF;
	return ret;
}

int aw_device_stop(struct aw_device *aw_dev)
{
	int int_st = 0;
	int monitor_int_st = 0;
	u16 reg_data = 0;
	struct aw_sec_data_desc *dsp_cfg =
		&aw_dev->prof_info.prof_desc[aw_dev->cur_prof].sec_desc[AW_DATA_TYPE_DSP_CFG];
	struct aw_sec_data_desc *dsp_fw =
		&aw_dev->prof_info.prof_desc[aw_dev->cur_prof].sec_desc[AW_DATA_TYPE_DSP_FW];

	aw_dev_dbg(aw_dev->dev, "enter");

	aw_dev->ops.aw_reg_read(aw_dev, aw_dev->pwd_desc.reg, &reg_data);

	if ((aw_dev->status == AW_DEV_PW_OFF) &&
	    (reg_data & aw_dev->pwd_desc.enable)) {
		aw_dev_info(aw_dev->dev, "already power off");
		return 0;
	}

	aw_dev->status = AW_DEV_PW_OFF;

	aw_monitor_stop(&aw_dev->monitor_desc);

	/*set mute*/
	aw_dev_set_hmute(aw_dev, AW_DEV_HMUTE_ENABLE);
	usleep_range(AW_4000_US, AW_4000_US + 100);

	/*close tx feedback*/
	if (aw_dev->ops.aw_i2s_tx_enable)
		aw_dev->ops.aw_i2s_tx_enable(aw_dev, false);

	usleep_range(AW_1000_US, AW_1000_US + 100);

	/*set defaut int mask*/
	aw_dev_set_intmask(aw_dev, AW_DEV_MASK_INT_VAL);

	/*check sysint state*/
	int_st = aw_dev_sysint_check(aw_dev);

	/*close dsp*/
	aw_dev_dsp_enable(aw_dev, AW_DEV_DSP_BYPASS);

	/*enable amppd*/
	aw_dev_amppd(aw_dev, true);

	/*check monitor process sysint state*/
	monitor_int_st = aw_dev_get_monitor_sysint_st(aw_dev);

	if (int_st < 0 || monitor_int_st < 0) {
		/*system status anomaly*/
		aw_dev_memclk_select(aw_dev, AW_DEV_MEMCLK_OSC);
		aw_dev_sram_check(aw_dev);
		aw_dev_dsp_fw_update(aw_dev, dsp_fw->data, dsp_fw->len);
		aw_dev_dsp_cfg_update(aw_dev, dsp_cfg->data, dsp_cfg->len);
		aw_dev_memclk_select(aw_dev, AW_DEV_MEMCLK_PLL);
	}

	/*set power down*/
	aw_dev_pwd(aw_dev, true);

	aw_dev_info(aw_dev->dev, "done");
	return 0;
}

/*deinit aw_device*/
void aw_device_deinit(struct aw_device *aw_dev)
{
	if (!aw_dev)
		return;

	if (aw_dev->prof_info.prof_desc) {
		devm_kfree(aw_dev->dev, aw_dev->prof_info.prof_desc);
		aw_dev->prof_info.prof_desc = NULL;
	}
	aw_dev->prof_info.count = 0;
}

/*init aw_device*/
int aw_device_init(struct aw_device *aw_dev, struct aw_container *aw_cfg)
{
	int ret = -1;

	if (!aw_dev || !aw_cfg) {
		aw_pr_err("aw_dev is NULL or aw_cfg is NULL");
		return -ENOMEM;
	}

	ret = aw_dev_cfg_load(aw_dev, aw_cfg);
	if (ret < 0) {
		aw_device_deinit(aw_dev);
		aw_dev_err(aw_dev->dev, "aw_dev acf parse failed");
		return -EINVAL;
	}

	aw_dev->cur_prof = aw_dev->prof_info.prof_desc[0].id;
	aw_dev->set_prof = aw_dev->prof_info.prof_desc[0].id;
	ret = aw_device_fw_update(aw_dev, AW_FORCE_UPDATE_ON,
				   AW_DSP_FW_UPDATE_ON);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "fw update failed");
		return ret;
	}

	aw_dev_set_intmask(aw_dev, AW_DEV_MASK_INT_VAL);

	/*set mute*/
	aw_dev_set_hmute(aw_dev, AW_DEV_HMUTE_ENABLE);

	/*close tx feedback*/
	if (aw_dev->ops.aw_i2s_tx_enable)
		aw_dev->ops.aw_i2s_tx_enable(aw_dev, false);

	usleep_range(AW_1000_US, AW_1000_US + 100);

	/*enable amppd*/
	aw_dev_amppd(aw_dev, true);
	/*close dsp*/
	aw_dev_dsp_enable(aw_dev, AW_DEV_DSP_BYPASS);
	/*set power down*/
	aw_dev_pwd(aw_dev, true);

	aw_dev_info(aw_dev->dev, "init done");

	return 0;
}

static int aw_parse_channel_dt(struct aw_device *aw_dev)
{
	int ret = -1;
	u32 channel_value = 0;
	struct device_node *np = aw_dev->dev->of_node;
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;

	ret = of_property_read_u32(np, "sound-channel", &channel_value);
	if (ret < 0) {
		aw_dev_info(aw_dev->dev,
			    "read sound-channel failed,use default 0");
		channel_value = AW_DEV_DEFAULT_CH;
	}

	/* when dev_num > 0, get dev list to compare*/
	if (aw_dev->ops.aw_get_dev_num() > 0) {
		ret = aw_dev_get_list_head(&dev_list);
		if (ret) {
			aw_dev_err(aw_dev->dev, "get dev list failed");
			return -EINVAL;
		}
		list_for_each(pos, dev_list) {
			local_dev = container_of(pos, struct aw_device,
						 list_node);
			if (local_dev->channel == channel_value) {
				aw_dev_err(local_dev->dev,
					   "sound-channel:%d already exists",
					   channel_value);
				return -EINVAL;
			}
		}
	}

	aw_dev->channel = channel_value;

	aw_dev_dbg(aw_dev->dev, "read sound-channel value is: %d",
		   aw_dev->channel);

	return 0;
}

static void aw_parse_fade_enable_dt(struct aw_device *aw_dev)
{
	int ret = -1;
	struct device_node *np = aw_dev->dev->of_node;
	u32 fade_en = 0;

	ret = of_property_read_u32(np, "fade-enable", &fade_en);
	if (ret < 0) {
		aw_dev_info(aw_dev->dev,
			    "read fade-enable failed, close fade_in_out");
		fade_en = AW_FADE_IN_OUT_DEFAULT;
	} else {
		aw_dev_info(aw_dev->dev, "read fade-enable value is: %d",
			    fade_en);
	}

	aw_dev->fade_en = fade_en;
}

static void aw_parse_re_range_dt(struct aw_device *aw_dev)
{
	int ret = -1;
	u32 re_max = 0;
	u32 re_min = 0;
	struct device_node *np = aw_dev->dev->of_node;

	ret = of_property_read_u32(np, "re-min", &re_min);
	if (ret < 0) {
		aw_dev->re_range.re_min = aw_dev->re_range.re_min_default;
		aw_dev_info(aw_dev->dev,
			    "read re-min value failed, set default value:[%d]mohm",
			    aw_dev->re_range.re_min);
	} else {
		aw_dev_info(aw_dev->dev, "parse re-min:[%d]", re_min);
		aw_dev->re_range.re_min = re_min;
	}

	ret = of_property_read_u32(np, "re-max", &re_max);
	if (ret < 0) {
		aw_dev->re_range.re_max = aw_dev->re_range.re_max_default;
		aw_dev_info(aw_dev->dev,
			    "read re-max failed, set default value:[%d]mohm",
			    aw_dev->re_range.re_max);
	} else {
		aw_dev_info(aw_dev->dev, "parse re-max:[%d]", re_max);
		aw_dev->re_range.re_max = re_max;
	}
}

static int aw_device_parse_dt(struct aw_device *aw_dev)
{
	int ret = -1;

	aw_parse_fade_enable_dt(aw_dev);
	aw_parse_re_range_dt(aw_dev);
	ret = aw_parse_channel_dt(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "parse channel failed");
		return ret;
	}
	return 0;
}

int aw_device_probe(struct aw_device *aw_dev)
{
	int ret = -1;

	INIT_LIST_HEAD(&aw_dev->list_node);

	ret = aw_device_parse_dt(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "device parse failed");
		return ret;
	}

	aw_cali_init(&aw_dev->cali_desc);

	aw_monitor_init(&aw_dev->monitor_desc);

	aw_spin_init(&aw_dev->spin_desc);

	return 0;
}

int aw_device_remove(struct aw_device *aw_dev)
{
	aw_monitor_deinit(&aw_dev->monitor_desc);

	aw_cali_deinit(&aw_dev->cali_desc);

	return 0;
}
