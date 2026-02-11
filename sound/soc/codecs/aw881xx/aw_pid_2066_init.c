// SPDX-License-Identifier: GPL-2.0
/* aw_pid_2066_init.c   aw codec driver
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
#include "aw_pid_2066_reg.h"
#include "aw_log.h"

static void aw_pid_2066_i2s_tx_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	aw_dev_dbg(aw->dev, "enter");

	if (flag) {
		aw_reg_write_bits(aw, AW_PID_2066_I2SCTRL3_REG,
				  AW_PID_2066_I2STXEN_MASK,
				  AW_PID_2066_I2STXEN_ENABLE_VALUE);
	} else {
		aw_reg_write_bits(aw, AW_PID_2066_I2SCTRL3_REG,
				  AW_PID_2066_I2STXEN_MASK,
				  AW_PID_2066_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_2066_check_rd_access(int reg)
{
	if (reg >= AW_PID_2066_REG_MAX)
		return false;

	if (aw_pid_2066_reg_access[reg] & REG_RD_ACCESS)
		return true;
	else
		return false;
}

static bool aw_pid_2066_check_wr_access(int reg)
{
	if (reg >= AW_PID_2066_REG_MAX)
		return false;

	if (aw_pid_2066_reg_access[reg] & REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_2066_get_reg_num(void)
{
	return AW_PID_2066_REG_MAX;
}

static unsigned int aw_pid_2066_reg_val_to_db(unsigned int reg_val)
{
	return reg_val;
}

static int aw_pid_2066_db_to_reg_val(u16 db_val)
{
	return db_val;
}

static int aw_pid_2066_set_volume(struct aw_device *aw_pa, u16 volume)
{
	int ret = -1;
	u16 reg_value = 0;
	u16 real_value = 0;
	u16 reg_vol = 0;
	u16 min_vol = 0;
	struct aw *aw = (struct aw *)aw_pa->private_data;
	struct aw_volume_desc *vol_desc = &aw->aw_pa->volume_desc;

	min_vol = AW_GET_MIN_VALUE(volume, vol_desc->mute_volume);
	reg_vol = aw_pid_2066_db_to_reg_val(min_vol);

	ret = aw_reg_read(aw, AW_PID_2066_SYSCTRL2_REG, &reg_value);
	if (ret < 0) {
		aw_dev_err(aw_pa->dev, "read vol reg failed");
		return ret;
	}

	real_value = (reg_vol << AW_PID_2066_VOL_START_BIT) |
		     (reg_value & AW_PID_2066_VOL_MASK);

	ret = aw_reg_write(aw, AW_PID_2066_SYSCTRL2_REG, real_value);
	if (ret < 0) {
		aw_dev_err(aw_pa->dev, "write vol reg failed");
		return ret;
	}

	return 0;
}

static int aw_pid_2066_get_volume(struct aw_device *aw_pa, u16 *volume)
{
	int ret = -1;
	u16 reg_value = 0;
	u16 reg_vol = 0;
	u16 real_vol = 0;
	struct aw *aw = (struct aw *)aw_pa->private_data;

	ret = aw_reg_read(aw, AW_PID_2066_SYSCTRL2_REG, &reg_value);
	if (ret < 0) {
		aw_dev_err(aw_pa->dev, "read vol reg failed");
		return ret;
	}

	reg_vol = reg_value & (~AW_PID_2066_VOL_MASK);
	real_vol = aw_pid_2066_reg_val_to_db(reg_vol);

	*volume = real_vol;

	return 0;
}

static int aw_pid_2066_get_icalk(struct aw_device *aw_dev, int16_t *icalk)
{
	int ret = -1;
	u16 efrm_reg_val = 0;
	u16 efrl_reg_val = 0;
	u16 ef_isn_geslp = 0;
	u16 ef_isn_h5bits = 0;
	u16 icalk_val = 0;
	struct aw_efuse_check_desc *efuse_check_desc = &aw_dev->efuse_check_desc;

	ret = aw_dev->ops.aw_reg_read(aw_dev, AW_PID_2066_EFRM2_REG,
				       &efrm_reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read efrm_reg failed");
		return ret;
	}

	ef_isn_geslp = (efrm_reg_val & (~AW_PID_2066_EF_ISN_GESLP_MASK)) >>
		       AW_PID_2066_EF_ISN_GESLP_SHIFT;

	ret = aw_dev->ops.aw_reg_read(aw_dev, AW_PID_2066_EFRL_REG,
				       &efrl_reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read efrl_reg failed");
		return ret;
	}

	ef_isn_h5bits = (efrl_reg_val & (~AW_PID_2066_EF_ISN_H5BITS_MASK)) >>
			AW_PID_2066_EF_ISN_H5BITS_SHIFT;

	if (efuse_check_desc->check_val == AW_EF_AND_CHECK) {
		icalk_val = ef_isn_geslp & (ef_isn_h5bits |
			    AW_PID_2066_EF_ISN_H5BITS_SIGN_MASK);
	} else if (efuse_check_desc->check_val == AW_EF_OR_CHECK) {
		icalk_val = ef_isn_geslp | (ef_isn_h5bits &
			    (~AW_PID_2066_EF_ISN_H5BITS_SIGN_MASK));
	} else {
		aw_dev_err(aw_dev->dev, "unsupported check type:%d",
			   efuse_check_desc->check_val);
		return -EINVAL;
	}
	if (icalk_val & (~AW_PID_2066_ICALK_SIGN_MASK))
		icalk_val = icalk_val | AW_PID_2066_ICALK_NEG_MASK;

	*icalk = (int16_t)icalk_val;
	return 0;
}

static int aw_pid_2066_get_vcalk(struct aw_device *aw_dev, int16_t *vcalk)
{
	int ret = -1;
	u16 efrm_reg_val = 0;
	u16 efrl_reg_val = 0;
	u16 ef_vsn_geslp = 0;
	u16 ef_vsn_h3bits = 0;
	u16 vcalk_val = 0;
	struct aw_efuse_check_desc *efuse_check_desc = &aw_dev->efuse_check_desc;

	ret = aw_dev->ops.aw_reg_read(aw_dev, AW_PID_2066_EFRM2_REG,
				       &efrm_reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read efrm_reg failed");
		return ret;
	}

	ef_vsn_geslp = (efrm_reg_val & (~AW_PID_2066_EF_VSN_GESLP_MASK)) >>
		       AW_PID_2066_EF_VSN_GESLP_SHIFT;

	ret = aw_dev->ops.aw_reg_read(aw_dev, AW_PID_2066_EFRL_REG,
				       &efrl_reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read efrl_reg failed");
		return ret;
	}

	ef_vsn_h3bits = (efrl_reg_val & (~AW_PID_2066_EF_VSN_H3BITS_MASK)) >>
			AW_PID_2066_EF_VSN_H3BITS_SHIFT;

	if (efuse_check_desc->check_val == AW_EF_AND_CHECK) {
		vcalk_val = ef_vsn_geslp & (ef_vsn_h3bits |
			    AW_PID_2066_EF_VSN_H3BITS_SIGN_MASK);
	} else if (efuse_check_desc->check_val == AW_EF_OR_CHECK) {
		vcalk_val = ef_vsn_geslp | (ef_vsn_h3bits &
			    (~AW_PID_2066_EF_VSN_H3BITS_SIGN_MASK));
	} else {
		aw_dev_err(aw_dev->dev, "unsupported check type:%d",
			   efuse_check_desc->check_val);
		return -EINVAL;
	}
	if (vcalk_val & (~AW_PID_2066_VCALK_SIGN_MASK))
		vcalk_val = vcalk_val | AW_PID_2066_VCALK_NEG_MASK;

	*vcalk = (int16_t)vcalk_val;
	return 0;
}

static int aw_pid_2066_set_vcalb(struct aw_device *aw_dev)
{
	int ret = -1;
	u16 reg_val = 0;
	int16_t icalk = 0;
	int16_t vcalk = 0;
	int32_t ical_k = 0;
	int32_t vcal_k = 0;
	int32_t vcalb = 0;
	struct aw_vcalb_desc *vcalb_desc = &aw_dev->vcalb_desc;

	ret = aw_pid_2066_get_vcalk(aw_dev, &vcalk);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get vcalk failed");
		return ret;
	}
	aw_dev_info(aw_dev->dev, "vcalk = %d", vcalk);

	ret = aw_pid_2066_get_icalk(aw_dev, &icalk);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "get icalk failed");
		return ret;
	}
	aw_dev_info(aw_dev->dev, "icalk = %d", icalk);

	ical_k = icalk * AW_PID_2066_ICABLK_FACTOR +
		 AW_PID_2066_CABL_BASE_VALUE;
	vcal_k = vcalk * AW_PID_2066_VCABLK_FACTOR +
		 AW_PID_2066_CABL_BASE_VALUE;

	aw_dev_info(aw_dev->dev, "ical_k = %d, vcal_k = %d", ical_k, vcal_k);
	vcalb = AW_PID_2066_VCALB_ACCURACY * AW_PID_2066_VSCAL_FACTOR /
		AW_PID_2066_ISCAL_FACTOR * ical_k / vcal_k *
		vcalb_desc->init_value;

	vcalb = vcalb >> AW_PID_2066_VCALB_ADJ_FACTOR;
	reg_val = (u32)vcalb;

	aw_dev_info(aw_dev->dev, "vcalb = %d, vcalb_adj = %d",
		    reg_val, vcalb_desc->init_value >>
		    AW_PID_2066_VCALB_ADJ_FACTOR);

	ret = aw_dev->ops.aw_reg_write(aw_dev, AW_PID_2066_DSPVCALB_REG,
					reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "write vcalb_reg failed");
		return ret;
	}

	return 0;
}

int aw_pid_2066_dev_init(struct aw_device *aw_pa)
{

	/*call aw device init func*/
	aw_pa->fade_step = AW_PID_2066_VOLUME_STEP_DB;
	aw_pa->re_range.re_min_default = AW_RE_MIN;
	aw_pa->re_range.re_max_default = AW_RE_MAX;
	aw_pa->crc_type = AW_HW_CRC_CHECK;

	aw_pa->ops.aw_set_cali_re = NULL;
	aw_pa->ops.aw_get_cali_re = NULL;
	aw_pa->ops.aw_get_r0 = NULL;
	aw_pa->ops.aw_sw_crc_check = NULL;
	aw_pa->ops.aw_init_vcalb_update = NULL;
	aw_pa->dsp_crc_desc.ctl_reg = AW_REG_NONE;

	aw_pa->ops.aw_get_hw_volume = aw_pid_2066_get_volume;
	aw_pa->ops.aw_set_hw_volume = aw_pid_2066_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2066_reg_val_to_db;

	aw_pa->ops.aw_check_rd_access = aw_pid_2066_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2066_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2066_get_reg_num;

	aw_pa->ops.aw_i2s_tx_enable = aw_pid_2066_i2s_tx_enable;
	aw_pa->ops.aw_set_vcalb = aw_pid_2066_set_vcalb;

	aw_pa->int_desc.mask_reg = AW_PID_2066_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2066_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2066_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2066_SYSINT_REG;
	aw_pa->int_desc.intst_mask = AW_PID_2066_BIT_SYSINT_CHECK;

	aw_pa->pwd_desc.reg = AW_PID_2066_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2066_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2066_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2066_PWDN_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2066_SYSCTRL_REG;
	aw_pa->mute_desc.mask = AW_PID_2066_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2066_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2066_HMUTE_DISABLE_VALUE;

	aw_pa->vcalb_desc.vcalb_reg = AW_PID_2066_DSPVCALB_REG;

	aw_pa->efuse_check_desc.reg = AW_PID_2066_DBGCTRL_REG;
	aw_pa->efuse_check_desc.mask = AW_PID_2066_EF_DBMD_MASK;
	aw_pa->efuse_check_desc.or_val = AW_PID_2066_EF_DBMD_OR_VALUE;
	aw_pa->efuse_check_desc.and_val = AW_PID_2066_EF_DBMD_AND_VALUE;

	aw_pa->sysst_desc.reg = AW_PID_2066_SYSST_REG;
	aw_pa->sysst_desc.st_check = AW_PID_2066_BIT_SYSST_NOSWS_CHECK;
	aw_pa->sysst_desc.st_mask = AW_PID_2066_BIT_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.pll_check = AW_PID_2066_BIT_PLL_CHECK;
	aw_pa->sysst_desc.dsp_check = AW_PID_2066_DSPS_NORMAL_VALUE;
	aw_pa->sysst_desc.dsp_mask = AW_PID_2066_DSPS_MASK;
	aw_pa->sysst_desc.st_sws_check = AW_PID_2066_BIT_SYSST_SWS_CHECK;

	aw_pa->noise_gate_en.reg = AW_PID_2066_PWMCTRL3_REG;
	aw_pa->noise_gate_en.noise_gate_mask = AW_PID_2066_NOISE_GATE_EN_MASK;

	aw_pa->profctrl_desc.reg = AW_PID_2066_SYSCTRL_REG;
	aw_pa->profctrl_desc.mask = AW_PID_2066_EN_TRAN_MASK;
	aw_pa->profctrl_desc.rcv_mode_val = AW_PID_2066_EN_TRAN_RCV_VALUE;

	aw_pa->volume_desc.reg = AW_PID_2066_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2066_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2066_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2066_MUTE_VOLUME;
	aw_pa->volume_desc.max_volume = AW_PID_2066_VOL_DEFAULT_VALUE;
	aw_pa->volume_desc.ctl_volume = AW_PID_2066_VOL_DEFAULT_VALUE;

	aw_pa->dsp_en_desc.reg = AW_PID_2066_SYSCTRL_REG;
	aw_pa->dsp_en_desc.mask = AW_PID_2066_DSPBY_MASK;
	aw_pa->dsp_en_desc.enable = AW_PID_2066_DSPBY_WORKING_VALUE;
	aw_pa->dsp_en_desc.disable = AW_PID_2066_DSPBY_BYPASS_VALUE;

	aw_pa->memclk_desc.reg = AW_PID_2066_DBGCTRL_REG;
	aw_pa->memclk_desc.mask = AW_PID_2066_MEM_CLKSEL_MASK;
	aw_pa->memclk_desc.mcu_hclk = AW_PID_2066_MEM_CLKSEL_DAP_HCLK_VALUE;
	aw_pa->memclk_desc.osc_clk = AW_PID_2066_MEM_CLKSEL_OSC_CLK_VALUE;

	aw_pa->watch_dog_desc.reg = AW_PID_2066_WDT_REG;
	aw_pa->watch_dog_desc.mask = AW_PID_2066_WDT_CNT_MASK;

	aw_pa->dsp_mem_desc.dsp_madd_reg = AW_PID_2066_DSPMADD_REG;
	aw_pa->dsp_mem_desc.dsp_mdat_reg = AW_PID_2066_DSPMDAT_REG;
	aw_pa->dsp_mem_desc.dsp_cfg_base_addr = AW_PID_2066_DSP_CFG_ADDR;
	aw_pa->dsp_mem_desc.dsp_fw_base_addr = AW_PID_2066_DSP_FW_ADDR;
	aw_pa->dsp_mem_desc.dsp_rom_check_reg = AW_PID_2066_DSP_ROM_CHECK_ADDR;
	aw_pa->dsp_mem_desc.dsp_rom_check_data = AW_PID_2066_DSP_ROM_CHECK_DATA;

	aw_pa->soft_rst.reg = AW_PID_2066_ID_REG;
	aw_pa->soft_rst.reg_value = AW_PID_2066_SOFT_RESET_VALUE;

	aw_pa->dsp_vol_desc.reg = AW_PID_2066_DSPCFG_REG;
	aw_pa->dsp_vol_desc.mask = AW_PID_2066_DSP_VOL_MASK;
	aw_pa->dsp_vol_desc.mute_st = AW_PID_2066_DSP_VOL_MUTE;
	aw_pa->dsp_vol_desc.noise_st = AW_PID_2066_DSP_VOL_NOISE_ST;

	aw_pa->amppd_desc.reg = AW_PID_2066_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2066_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2066_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2066_AMPPD_WORKING_VALUE;

	aw_pa->cali_desc.spkr_temp_desc.reg = AW_PID_2066_ASR2_REG;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.ra_desc.dsp_reg = AW_PID_2066_DSP_REG_CFG_ADPZ_RA;
	aw_pa->cali_desc.ra_desc.data_type = AW_DSP_32_DATA;
	aw_pa->cali_desc.ra_desc.shift = AW_PID_2066_DSP_CFG_ADPZ_RA_SHIFT;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.cali_cfg_desc.actampth_reg =
		AW_PID_2066_DSP_REG_CFG_MBMEC_ACTAMPTH;
	aw_pa->cali_desc.cali_cfg_desc.actampth_data_type = AW_DSP_32_DATA;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.cali_cfg_desc.noiseampth_reg =
		AW_PID_2066_DSP_REG_CFG_MBMEC_NOISEAMPTH;
	aw_pa->cali_desc.cali_cfg_desc.noiseampth_data_type = AW_DSP_32_DATA;

	aw_pa->cali_desc.cali_cfg_desc.ustepn_reg =
		AW_PID_2066_DSP_REG_CFG_ADPZ_USTEPN;
	aw_pa->cali_desc.cali_cfg_desc.ustepn_data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.cali_cfg_desc.alphan_reg =
		AW_PID_2066_DSP_REG_CFG_RE_ALPHA;
	aw_pa->cali_desc.cali_cfg_desc.alphan_data_type = AW_DSP_16_DATA;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.r0_desc.dsp_reg = AW_PID_2066_DSP_REG_CFG_ADPZ_RE;
	aw_pa->cali_desc.r0_desc.data_type = AW_DSP_16_DATA;
	aw_pa->cali_desc.r0_desc.shift = AW_PID_2066_ADPZ_RE_SHIFT;

	aw_pa->cali_desc.dsp_re_desc.shift = AW_PID_2066_DSP_REG_CALRE_SHIFT;
	aw_pa->cali_desc.dsp_re_desc.dsp_reg = AW_PID_2066_DSP_REG_CALRE;
	aw_pa->cali_desc.dsp_re_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.hw_cali_re_desc.hbits_reg = AW_PID_2066_ACR1_REG;
	aw_pa->cali_desc.hw_cali_re_desc.hbits_mask =
		AW_PID_2066_CALI_RE_HBITS_MASK;
	aw_pa->cali_desc.hw_cali_re_desc.hbits_shift =
		AW_PID_2066_CALI_RE_HBITS_SHIFT;
	aw_pa->cali_desc.hw_cali_re_desc.lbits_reg = AW_PID_2066_ACR2_REG;
	aw_pa->cali_desc.hw_cali_re_desc.lbits_mask =
		AW_PID_2066_CALI_RE_LBITS_MASK;
	aw_pa->cali_desc.hw_cali_re_desc.lbits_shift =
		AW_PID_2066_CALI_RE_LBITS_SHIFT;
	aw_pa->cali_desc.hw_cali_re_desc.cali_re_shift =
		AW_PID_2066_DSP_RE_SHIFT;

	aw_pa->cali_desc.noise_desc.dsp_reg =
		AW_PID_2066_DSP_REG_CFG_MBMEC_GLBCFG;
	aw_pa->cali_desc.noise_desc.data_type = AW_DSP_16_DATA;
	aw_pa->cali_desc.noise_desc.mask = AW_PID_2066_DSP_REG_NOISE_MASK;

	aw_pa->cali_desc.f0_desc.dsp_reg = AW_PID_2066_DSP_REG_RESULT_F0;
	aw_pa->cali_desc.f0_desc.shift = AW_PID_2066_DSP_F0_SHIFT;
	aw_pa->cali_desc.f0_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.q_desc.dsp_reg = AW_PID_2066_DSP_REG_RESULT_Q;
	aw_pa->cali_desc.q_desc.shift = AW_PID_2066_DSP_Q_SHIFT;
	aw_pa->cali_desc.q_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.iv_desc.reg = AW_PID_2066_ASR1_REG;
	aw_pa->cali_desc.iv_desc.reabs_mask = AW_PID_2066_REABS_MASK;

	aw_pa->cali_desc.cali_delay_desc.dsp_reg =
		AW_PID_2066_DSP_CALI_F0_DELAY;
	aw_pa->cali_desc.cali_delay_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cco_mux_desc.reg = AW_PID_2066_PLLCTRL2_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2066_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divider = AW_PID_2066_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass = AW_PID_2066_CCO_MUX_BYPASS_VALUE;

	aw_pa->monitor_desc.dsp_monitor_delay = AW_DSP_MONITOR_DELAY;

	aw_pa->monitor_desc.voltage_desc.reg = AW_PID_2066_VBAT_REG;
	aw_pa->monitor_desc.voltage_desc.vbat_range = AW_PID_2066_VBAT_RANGE;
	aw_pa->monitor_desc.voltage_desc.int_bit = AW_PID_2066_INT_10BIT;

	aw_pa->monitor_desc.temp_desc.reg = AW_PID_2066_TEMP_REG;
	aw_pa->monitor_desc.temp_desc.sign_mask = AW_PID_2066_TEMP_SIGN_MASK;
	aw_pa->monitor_desc.temp_desc.neg_mask = AW_PID_2066_TEMP_NEG_MASK;

	aw_pa->monitor_desc.vmax_desc.dsp_reg = AW_PID_2066_DSP_REG_VMAX;
	aw_pa->monitor_desc.vmax_desc.data_type = AW_DSP_16_DATA;

	aw_pa->monitor_desc.ipeak_desc.reg = AW_PID_2066_BSTCTRL2_REG;
	aw_pa->monitor_desc.ipeak_desc.mask = AW_PID_2066_BST_IPEAK_MASK;

	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_reg =
		AW_PID_2066_DSP_REG_EXTERN_TEMP_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_reg =
		AW_PID_2066_DSP_MONITOR_TEMPEN_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_mask =
		AW_PID_2066_DSP_MONITOR_EXTERN_TEMP_MASK;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_reg =
		AW_PID_2066_DSP_MONITOR_TEMPEN_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_en_mask =
		AW_PID_2066_DSP_MONITOR_TEMPEN_MASK;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_reg =
		AW_PID_2066_DSP_REG_CFG_MBMEC_GLBCFG;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_en_mask =
		AW_PID_2066_DSP_MONITOR_VOLTEN_MASK;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.hw_monitor_desc.reg = AW_PID_2066_SYSCTRL2_REG;
	aw_pa->monitor_desc.hw_monitor_desc.bop_mask =
		AW_PID_2066_BOP_EN_MASK;
	aw_pa->monitor_desc.hw_monitor_desc.vol_mask =
		AW_PID_2066_BOP_VOL_EN_MASK;
	aw_pa->monitor_desc.hw_monitor_desc.ipeak_mask =
		AW_PID_2066_BOP_IPEAK_EN_MASK;

	aw_pa->chansel_desc.rxchan_reg = AW_PID_2066_I2SCTRL1_REG;
	aw_pa->chansel_desc.rxchan_mask = AW_PID_2066_CHSEL_MASK;

	aw_pa->chansel_desc.rx_left = AW_PID_2066_CHSEL_LEFT_VALUE;
	aw_pa->chansel_desc.rx_right = AW_PID_2066_CHSEL_RIGHT_VALUE;

	aw_pa->tx_en_desc.tx_en_reg = AW_PID_2066_I2SCTRL3_REG;
	aw_pa->tx_en_desc.tx_en_mask = AW_PID_2066_I2STXEN_MASK;
	aw_pa->tx_en_desc.tx_disable = AW_PID_2066_I2STXEN_DISABLE_VALUE;

	aw_pa->dsp_st_desc.dsp_reg_s1 = AW_PID_2066_DSP_ST_S1;
	aw_pa->dsp_st_desc.dsp_reg_e1 = AW_PID_2066_DSP_ST_E1;
	aw_pa->dsp_st_desc.dsp_reg_s2 = AW_PID_2066_DSP_ST_S2;
	aw_pa->dsp_st_desc.dsp_reg_e2 = AW_PID_2066_DSP_ST_E2;

	aw_pa->crc_check_desc.ram_clk_reg = AW_PID_2066_I2SCFG1_REG;
	aw_pa->crc_check_desc.ram_clk_mask = AW_PID_2066_RAM_CG_BYP_MASK;
	aw_pa->crc_check_desc.ram_clk_on =
		AW_PID_2066_RAM_CG_BYP_BYPASS_VALUE;
	aw_pa->crc_check_desc.ram_clk_off =
		AW_PID_2066_RAM_CG_BYP_WORK_VALUE;

	aw_pa->crc_check_desc.crc_cfg_base_addr =
		AW_PID_2066_CRC_CFG_BASE_ADDR;
	aw_pa->crc_check_desc.crc_fw_base_addr =
		AW_PID_2066_CRC_FW_BASE_ADDR;

	aw_pa->crc_check_desc.crc_ctrl_reg = AW_PID_2066_CRCCTRL_REG;
	aw_pa->crc_check_desc.crc_end_addr_mask =
		AW_PID_2066_CRC_END_ADDR_MASK;
	aw_pa->crc_check_desc.crc_cfg_check_en_mask =
		AW_PID_2066_CRC_CFG_EN_MASK;
	aw_pa->crc_check_desc.crc_cfgcheck_disable =
		AW_PID_2066_CRC_CFGCHECK_DISABLE_VAL;
	aw_pa->crc_check_desc.crc_cfgcheck_enable =
		AW_PID_2066_CRC_CFGCHECK_ENABLE_VAL;
	aw_pa->crc_check_desc.crc_fw_check_en_mask =
		AW_PID_2066_CRC_CODE_EN_MASK;
	aw_pa->crc_check_desc.crc_fwcheck_disable =
		AW_PID_2066_CRC_FWCHECK_DISABLE_VAL;
	aw_pa->crc_check_desc.crc_fwcheck_enable =
		AW_PID_2066_CRC_FWCHECK_ENABLE_VAL;

	aw_pa->crc_check_desc.crc_check_reg = AW_PID_2066_HAGCST_REG;
	aw_pa->crc_check_desc.crc_check_mask =
		AW_PID_2066_CRC_CHECK_BITS_MASK;
	aw_pa->crc_check_desc.crc_check_pass =
		AW_PID_2066_CRC_CHECK_PASS_VAL;
	aw_pa->crc_check_desc.crc_check_bits_shift =
		AW_PID_2066_CRC_CHECK_START_BIT;

	aw_pa->crc_check_realtime_desc.addr = AW_PID_2066_CRC_REALTIME_ADDR;
	aw_pa->crc_check_realtime_desc.mask = AW_PID_2066_CRC_REALTIME_MASK;
	aw_pa->crc_check_realtime_desc.data_type = AW_DSP_16_DATA;
	aw_pa->crc_check_realtime_desc.enable =
		AW_PID_2066_CRC_REALTIME_ENABLE;
	aw_pa->crc_check_realtime_desc.disable =
		AW_PID_2066_CRC_REALTIME_DISABLE;

	aw_pa->crc_check_realtime_desc.check_addr =
		AW_PID_2066_CRC_REALTIME_CHECK_ADDR;
	aw_pa->crc_check_realtime_desc.check_mask =
		AW_PID_2066_CRC_REALTIME_CHECK_MASK;
	aw_pa->crc_check_realtime_desc.check_abnormal =
		AW_PID_2066_CRC_REALTIME_CHECK_ABNORMAL;
	aw_pa->crc_check_realtime_desc.check_normal =
		AW_PID_2066_CRC_REALTIME_CHECK_NORMAL;
	aw_pa->crc_check_realtime_desc.check_data_type = AW_DSP_16_DATA;

	aw_pa->fw_ver_desc.version_reg = AW_PID_2066_FIRMWARE_VERSION_REG;
	aw_pa->fw_ver_desc.data_type = AW_DSP_16_DATA;

	aw_pa->dither_desc.reg = AW_PID_2066_DBGCTRL_REG;
	aw_pa->dither_desc.mask = AW_PID_2066_DITHER_EN_MASK;
	aw_pa->dither_desc.enable = AW_PID_2066_DITHER_EN_ENABLE_VALUE;
	aw_pa->dither_desc.disable = AW_PID_2066_DITHER_EN_DISABLE_VALUE;

	aw_pa->dsp_ng_desc.reg = AW_REG_NONE;
	aw_pa->dsp_ng_desc.dsp_lp_reg = AW_REG_NONE;

	return 0;
}
