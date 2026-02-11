// SPDX-License-Identifier: GPL-2.0
/* aw_pid_2183_init.c   aw codec driver
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
#include "aw_pid_2183_reg.h"
#include "aw_log.h"

static void aw_pid_2183_i2s_tx_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw *aw = (struct aw *)aw_dev->private_data;

	aw_dev_dbg(aw->dev, "enter");

	if (flag) {
		aw_reg_write_bits(aw, AW_PID_2183_I2SCTRL3_REG,
				  AW_PID_2183_I2STXEN_MASK,
				  AW_PID_2183_I2STXEN_ENABLE_VALUE);
	} else {
		aw_reg_write_bits(aw, AW_PID_2183_I2SCTRL3_REG,
				  AW_PID_2183_I2STXEN_MASK,
				  AW_PID_2183_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_2183_check_rd_access(int reg)
{
	if (reg >= AW_PID_2183_REG_MAX)
		return false;

	if (aw_pid_2183_reg_access[reg] & REG_RD_ACCESS)
		return true;
	else
		return false;
}

static bool aw_pid_2183_check_wr_access(int reg)
{
	if (reg >= AW_PID_2183_REG_MAX)
		return false;

	if (aw_pid_2183_reg_access[reg] & REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_2183_get_reg_num(void)
{
	return AW_PID_2183_REG_MAX;
}

static unsigned int aw_pid_2183_reg_val_to_db(unsigned int reg_val)
{
	return reg_val;
}

static int aw_pid_2183_db_to_reg_val(u16 db_val)
{
	return db_val;
}

static int aw_pid_2183_set_volume(struct aw_device *aw_pa, u16 volume)
{
	int ret = -1;
	u16 reg_value = 0;
	u16 real_value = 0;
	u16 reg_vol = 0;
	u16 min_vol = 0;
	struct aw *aw = (struct aw *)aw_pa->private_data;
	struct aw_volume_desc *vol_desc = &aw->aw_pa->volume_desc;

	min_vol = AW_GET_MIN_VALUE(volume, vol_desc->mute_volume);
	reg_vol = aw_pid_2183_db_to_reg_val(min_vol);

	ret = aw_reg_read(aw, AW_PID_2183_SYSCTRL2_REG, &reg_value);
	if (ret < 0) {
		aw_dev_err(aw_pa->dev, "read vol reg failed");
		return ret;
	}

	real_value = (reg_vol << AW_PID_2183_VOL_START_BIT) |
		     (reg_value & AW_PID_2183_VOL_MASK);

	ret = aw_reg_write(aw, AW_PID_2183_SYSCTRL2_REG, real_value);
	if (ret < 0) {
		aw_dev_err(aw_pa->dev, "write vol reg failed");
		return ret;
	}

	return 0;
}

static int aw_pid_2183_get_volume(struct aw_device *aw_pa, u16 *volume)
{
	int ret = -1;
	u16 reg_value = 0;
	u16 reg_vol = 0;
	u16 real_vol = 0;
	struct aw *aw = (struct aw *)aw_pa->private_data;

	ret = aw_reg_read(aw, AW_PID_2183_SYSCTRL2_REG, &reg_value);
	if (ret < 0) {
		aw_dev_err(aw_pa->dev, "read vol reg failed");
		return ret;
	}

	reg_vol = reg_value & (~AW_PID_2183_VOL_MASK);
	real_vol = aw_pid_2183_reg_val_to_db(reg_vol);

	*volume = real_vol;

	return 0;
}

int aw_pid_2183_dev_init(struct aw_device *aw_pa)
{
	/*call aw device init func*/
	aw_pa->fade_step = AW_PID_2183_VOLUME_STEP_DB;
	aw_pa->re_range.re_min_default = AW_RE_MIN;
	aw_pa->re_range.re_max_default = AW_RE_MAX;
	aw_pa->crc_type = AW_HW_CRC_CHECK;

	aw_pa->ops.aw_set_vcalb = NULL;
	aw_pa->ops.aw_set_cali_re = NULL;
	aw_pa->ops.aw_get_cali_re = NULL;
	aw_pa->ops.aw_get_r0 = NULL;
	aw_pa->ops.aw_sw_crc_check = NULL;
	aw_pa->ops.aw_init_vcalb_update = NULL;

	aw_pa->ops.aw_get_hw_volume = aw_pid_2183_get_volume;
	aw_pa->ops.aw_set_hw_volume = aw_pid_2183_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2183_reg_val_to_db;

	aw_pa->ops.aw_check_rd_access = aw_pid_2183_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2183_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2183_get_reg_num;

	aw_pa->ops.aw_i2s_tx_enable = aw_pid_2183_i2s_tx_enable;

	aw_pa->dsp_crc_desc.ctl_reg = AW_REG_NONE;

	aw_pa->int_desc.mask_reg = AW_PID_2183_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2183_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2183_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2183_SYSINT_REG;
	aw_pa->int_desc.intst_mask = AW_PID_2183_BIT_SYSINT_CHECK;

	aw_pa->pwd_desc.reg = AW_PID_2183_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2183_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2183_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2183_PWDN_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2183_SYSCTRL_REG;
	aw_pa->mute_desc.mask = AW_PID_2183_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2183_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2183_HMUTE_DISABLE_VALUE;

	aw_pa->vsense_desc.vcalb_vsense_reg = AW_PID_2183_VSNCTRL1_REG;
	aw_pa->vsense_desc.vcalk_vdsel_mask = AW_PID_2183_VDSEL_MASK;

	aw_pa->efuse_check_desc.reg = AW_PID_2183_DBGCTRL_REG;
	aw_pa->efuse_check_desc.mask = AW_PID_2183_EF_DBMD_MASK;
	aw_pa->efuse_check_desc.or_val = AW_PID_2183_EF_DBMD_OR_VALUE;
	aw_pa->efuse_check_desc.and_val = AW_PID_2183_EF_DBMD_AND_VALUE;

	aw_pa->vcalb_desc.icalkh_reg = AW_PID_2183_EFRH4_REG;
	aw_pa->vcalb_desc.icalkh_reg_mask = AW_PID_2183_EF_ISN_GESLP_H_MASK;
	aw_pa->vcalb_desc.icalkh_shift = AW_PID_2183_EF_ISN_GESLP_H_START_BIT;

	aw_pa->vcalb_desc.icalkl_reg = AW_PID_2183_EFRL4_REG;
	aw_pa->vcalb_desc.icalkl_reg_mask = AW_PID_2183_EF_ISN_GESLP_L_MASK;
	aw_pa->vcalb_desc.icalkl_shift = AW_PID_2183_EF_ISN_GESLP_L_START_BIT;

	aw_pa->vcalb_desc.icalk_sign_mask = AW_PID_2183_EF_ISN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.icalk_neg_mask = AW_PID_2183_EF_ISN_GESLP_SIGN_NEG;
	aw_pa->vcalb_desc.icalk_value_factor = AW_PID_2183_ICABLK_FACTOR;

	aw_pa->vcalb_desc.vcalkh_reg = AW_PID_2183_EFRH3_REG;
	aw_pa->vcalb_desc.vcalkh_reg_mask = AW_PID_2183_EF_VSN_GESLP_H_MASK;
	aw_pa->vcalb_desc.vcalkh_shift = AW_PID_2183_EF_VSN_GESLP_H_START_BIT;

	aw_pa->vcalb_desc.vcalkl_reg = AW_PID_2183_EFRL3_REG;
	aw_pa->vcalb_desc.vcalkl_reg_mask = AW_PID_2183_EF_VSN_GESLP_L_MASK;
	aw_pa->vcalb_desc.vcalkl_shift = AW_PID_2183_EF_VSN_GESLP_L_START_BIT;

	aw_pa->vcalb_desc.vcalk_sign_mask = AW_PID_2183_EF_VSN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_neg_mask = AW_PID_2183_EF_VSN_GESLP_SIGN_NEG;
	aw_pa->vcalb_desc.vcalk_value_factor = AW_PID_2183_VCABLK_FACTOR;

	aw_pa->vcalb_desc.vscal_factor = AW_PID_2183_VSCAL_FACTOR;
	aw_pa->vcalb_desc.iscal_factor = AW_PID_2183_ISCAL_FACTOR;

	aw_pa->vcalb_desc.vcalb_adj_shift = AW_PID_2183_VCALB_ADJ_FACTOR;
	aw_pa->vcalb_desc.cabl_base_value = AW_PID_2183_CABL_BASE_VALUE;
	aw_pa->vcalb_desc.vcalb_accuracy = AW_PID_2183_VCALB_ACCURACY;
	aw_pa->vcalb_desc.vcalb_reg = AW_PID_2183_DSPVCALB_REG;

	/*Vsense data select bit: DAC*/
	aw_pa->vcalb_desc.vcalkh_dac_reg = AW_PID_2183_EFRH2_REG;
	aw_pa->vcalb_desc.vcalkh_dac_reg_mask =
		AW_PID_2183_INTERNAL_VSN_TRIM_H_MASK;
	aw_pa->vcalb_desc.vcalkh_dac_shift =
		AW_PID_2183_INTERNAL_VSN_TRIM_H_START_BIT;

	aw_pa->vcalb_desc.vcalkl_dac_reg = AW_PID_2183_EFRL2_REG;
	aw_pa->vcalb_desc.vcalkl_dac_reg_mask =
		AW_PID_2183_INTERNAL_VSN_TRIM_L_MASK;
	aw_pa->vcalb_desc.vcalkl_dac_shift =
		AW_PID_2183_INTERNAL_VSN_TRIM_L_START_BIT;

	aw_pa->vcalb_desc.vcalk_dac_sign_mask = AW_PID_2183_TEM4_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_dac_neg_mask = AW_PID_2183_TEM4_SIGN_NEG;
	aw_pa->vcalb_desc.vcalk_dac_value_factor =
		AW_PID_2183_VCABLK_DAC_FACTOR;

	aw_pa->vcalb_desc.vscal_dac_factor = AW_PID_2183_VSCAL_DAC_FACTOR;
	aw_pa->vcalb_desc.iscal_dac_factor = AW_PID_2183_ISCAL_DAC_FACTOR;

	aw_pa->sysst_desc.reg = AW_PID_2183_SYSST_REG;
	aw_pa->sysst_desc.st_check = AW_PID_2183_BIT_SYSST_NOSWS_CHECK;
	aw_pa->sysst_desc.st_mask = AW_PID_2183_BIT_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.pll_check = AW_PID_2183_BIT_PLL_CHECK;
	aw_pa->sysst_desc.dsp_check = AW_PID_2183_DSPS_NORMAL_VALUE;
	aw_pa->sysst_desc.dsp_mask = AW_PID_2183_DSPS_MASK;
	aw_pa->sysst_desc.st_sws_check = AW_PID_2183_BIT_SYSST_SWS_CHECK;

	aw_pa->noise_gate_en.reg = AW_PID_2183_PWMCTRL3_REG;
	aw_pa->noise_gate_en.noise_gate_mask = AW_PID_2183_NOISE_GATE_EN_MASK;

	aw_pa->profctrl_desc.reg = AW_PID_2183_SYSCTRL_REG;
	aw_pa->profctrl_desc.mask = AW_PID_2183_RCV_MODE_MASK;
	aw_pa->profctrl_desc.rcv_mode_val = AW_PID_2183_RCV_MODE_RECEIVER_VALUE;

	aw_pa->volume_desc.reg = AW_PID_2183_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2183_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2183_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2183_MUTE_VOLUME;
	aw_pa->volume_desc.max_volume = AW_PID_2183_VOL_DEFAULT_VALUE;
	aw_pa->volume_desc.ctl_volume = AW_PID_2183_VOL_DEFAULT_VALUE;

	aw_pa->dsp_en_desc.reg = AW_PID_2183_SYSCTRL_REG;
	aw_pa->dsp_en_desc.mask = AW_PID_2183_DSPBY_MASK;
	aw_pa->dsp_en_desc.enable = AW_PID_2183_DSPBY_WORKING_VALUE;
	aw_pa->dsp_en_desc.disable = AW_PID_2183_DSPBY_BYPASS_VALUE;

	aw_pa->memclk_desc.reg = AW_PID_2183_DBGCTRL_REG;
	aw_pa->memclk_desc.mask = AW_PID_2183_MEM_CLKSEL_MASK;
	aw_pa->memclk_desc.mcu_hclk = AW_PID_2183_MEM_CLKSEL_DAPHCLK_VALUE;
	aw_pa->memclk_desc.osc_clk = AW_PID_2183_MEM_CLKSEL_OSCCLK_VALUE;

	aw_pa->watch_dog_desc.reg = AW_PID_2183_WDT_REG;
	aw_pa->watch_dog_desc.mask = AW_PID_2183_WDT_CNT_MASK;

	aw_pa->dsp_mem_desc.dsp_madd_reg = AW_PID_2183_DSPMADD_REG;
	aw_pa->dsp_mem_desc.dsp_mdat_reg = AW_PID_2183_DSPMDAT_REG;
	aw_pa->dsp_mem_desc.dsp_cfg_base_addr = AW_PID_2183_DSP_CFG_ADDR;
	aw_pa->dsp_mem_desc.dsp_fw_base_addr = AW_PID_2183_DSP_FW_ADDR;
	aw_pa->dsp_mem_desc.dsp_rom_check_reg = AW_PID_2183_DSP_ROM_CHECK_ADDR;
	aw_pa->dsp_mem_desc.dsp_rom_check_data = AW_PID_2183_DSP_ROM_CHECK_DATA;

	aw_pa->soft_rst.reg = AW_PID_2183_ID_REG;
	aw_pa->soft_rst.reg_value = AW_PID_2183_SOFT_RESET_VALUE;

	aw_pa->dsp_vol_desc.reg = AW_PID_2183_DSPCFG_REG;
	aw_pa->dsp_vol_desc.mask = AW_PID_2183_DSP_VOL_MASK;
	aw_pa->dsp_vol_desc.mute_st = AW_PID_2183_DSP_VOL_MUTE;
	aw_pa->dsp_vol_desc.noise_st = AW_PID_2183_DSP_VOL_NOISE_ST;

	aw_pa->amppd_desc.reg = AW_PID_2183_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2183_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2183_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2183_AMPPD_WORKING_VALUE;

	aw_pa->cali_desc.spkr_temp_desc.reg = AW_PID_2183_ASR2_REG;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.ra_desc.dsp_reg = AW_PID_2183_DSP_REG_CFG_ADPZ_RA;
	aw_pa->cali_desc.ra_desc.data_type = AW_DSP_32_DATA;
	aw_pa->cali_desc.ra_desc.shift = AW_PID_2183_DSP_CFG_ADPZ_RA_SHIFT;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.cali_cfg_desc.actampth_reg =
		AW_PID_2183_DSP_REG_CFG_MBMEC_ACTAMPTH;
	aw_pa->cali_desc.cali_cfg_desc.actampth_data_type = AW_DSP_32_DATA;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.cali_cfg_desc.noiseampth_reg =
		AW_PID_2183_DSP_REG_CFG_MBMEC_NOISEAMPTH;
	aw_pa->cali_desc.cali_cfg_desc.noiseampth_data_type = AW_DSP_32_DATA;

	aw_pa->cali_desc.cali_cfg_desc.ustepn_reg =
		AW_PID_2183_DSP_REG_CFG_ADPZ_USTEPN;
	aw_pa->cali_desc.cali_cfg_desc.ustepn_data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.cali_cfg_desc.alphan_reg =
		AW_PID_2183_DSP_REG_CFG_RE_ALPHA;
	aw_pa->cali_desc.cali_cfg_desc.alphan_data_type = AW_DSP_16_DATA;

	/*32-bit data types need bypass dsp*/
	aw_pa->cali_desc.r0_desc.dsp_reg = AW_PID_2183_DSP_REG_CFG_ADPZ_RE;
	aw_pa->cali_desc.r0_desc.data_type = AW_DSP_16_DATA;
	aw_pa->cali_desc.r0_desc.shift = AW_PID_2183_ADPZ_RE_SHIFT;

	aw_pa->cali_desc.dsp_re_desc.dsp_reg = AW_PID_2183_DSP_REG_CALRE;
	aw_pa->cali_desc.dsp_re_desc.shift = AW_PID_2183_DSP_REG_CALRE_SHIFT;
	aw_pa->cali_desc.dsp_re_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.hw_cali_re_desc.hbits_reg = AW_PID_2183_ACR1_REG;
	aw_pa->cali_desc.hw_cali_re_desc.hbits_mask =
		AW_PID_2183_CALI_RE_HBITS_MASK;
	aw_pa->cali_desc.hw_cali_re_desc.hbits_shift =
		AW_PID_2183_CALI_RE_HBITS_SHIFT;
	aw_pa->cali_desc.hw_cali_re_desc.lbits_reg = AW_PID_2183_ACR2_REG;
	aw_pa->cali_desc.hw_cali_re_desc.lbits_mask =
		AW_PID_2183_CALI_RE_LBITS_MASK;
	aw_pa->cali_desc.hw_cali_re_desc.lbits_shift =
		AW_PID_2183_CALI_RE_LBITS_SHIFT;
	aw_pa->cali_desc.hw_cali_re_desc.cali_re_shift =
		AW_PID_2183_DSP_RE_SHIFT;

	aw_pa->cali_desc.noise_desc.dsp_reg =
		AW_PID_2183_DSP_REG_CFG_MBMEC_GLBCFG;
	aw_pa->cali_desc.noise_desc.data_type = AW_DSP_16_DATA;
	aw_pa->cali_desc.noise_desc.mask = AW_PID_2183_DSP_REG_NOISE_MASK;

	aw_pa->cali_desc.f0_desc.dsp_reg = AW_PID_2183_DSP_REG_RESULT_F0;
	aw_pa->cali_desc.f0_desc.shift = AW_PID_2183_DSP_F0_SHIFT;
	aw_pa->cali_desc.f0_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.q_desc.dsp_reg = AW_PID_2183_DSP_REG_RESULT_Q;
	aw_pa->cali_desc.q_desc.shift = AW_PID_2183_DSP_Q_SHIFT;
	aw_pa->cali_desc.q_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cali_desc.iv_desc.reg = AW_PID_2183_ASR1_REG;
	aw_pa->cali_desc.iv_desc.reabs_mask = AW_PID_2183_REABS_MASK;

	aw_pa->cali_desc.cali_delay_desc.dsp_reg =
		AW_PID_2183_DSP_CALI_F0_DELAY;
	aw_pa->cali_desc.cali_delay_desc.data_type = AW_DSP_16_DATA;

	aw_pa->cco_mux_desc.reg = AW_PID_2183_PLLCTRL2_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2183_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divider = AW_PID_2183_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass = AW_PID_2183_CCO_MUX_BYPASS_VALUE;

	aw_pa->monitor_desc.dsp_monitor_delay = AW_DSP_MONITOR_DELAY;

	aw_pa->monitor_desc.voltage_desc.reg = AW_PID_2183_VBAT_REG;
	aw_pa->monitor_desc.voltage_desc.vbat_range = AW_PID_2183_VBAT_RANGE;
	aw_pa->monitor_desc.voltage_desc.int_bit = AW_PID_2183_INT_10BIT;

	aw_pa->monitor_desc.temp_desc.reg = AW_PID_2183_TEMP_REG;
	aw_pa->monitor_desc.temp_desc.sign_mask = AW_PID_2183_TEMP_SIGN_MASK;
	aw_pa->monitor_desc.temp_desc.neg_mask = AW_PID_2183_TEMP_NEG_MASK;

	aw_pa->monitor_desc.vmax_desc.dsp_reg = AW_PID_2183_DSP_REG_VMAX;
	aw_pa->monitor_desc.vmax_desc.data_type = AW_DSP_16_DATA;

	aw_pa->monitor_desc.ipeak_desc.reg = AW_PID_2183_BSTCTRL2_REG;
	aw_pa->monitor_desc.ipeak_desc.mask = AW_PID_2183_BST_IPEAK_MASK;

	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_reg =
		AW_PID_2183_DSP_REG_EXTERN_TEMP_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_reg =
		AW_PID_2183_DSP_MONITOR_TEMPEN_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_mask =
		AW_PID_2183_DSP_MONITOR_EXTERN_TEMP_MASK;
	aw_pa->monitor_desc.dsp_monitor_desc.extn_temp_en_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_reg =
		AW_PID_2183_DSP_MONITOR_TEMPEN_ADDR;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_en_mask =
		AW_PID_2183_DSP_MONITOR_TEMPEN_MASK;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_temp_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_reg =
		AW_PID_2183_DSP_REG_CFG_MBMEC_GLBCFG;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_en_mask =
		AW_PID_2183_DSP_MONITOR_VOLTEN_MASK;
	aw_pa->monitor_desc.dsp_monitor_desc.dsp_volt_reg_type =
		AW_DSP_16_DATA;

	aw_pa->monitor_desc.hw_monitor_desc.reg = AW_PID_2183_SYSCTRL2_REG;
	aw_pa->monitor_desc.hw_monitor_desc.bop_mask =
		AW_PID_2183_BOP_EN_MASK;
	aw_pa->monitor_desc.hw_monitor_desc.vol_mask =
		AW_PID_2183_BOP_VOL_EN_MASK;
	aw_pa->monitor_desc.hw_monitor_desc.ipeak_mask =
		AW_PID_2183_BOP_IPEAK_EN_MASK;

	aw_pa->chansel_desc.rxchan_reg = AW_PID_2183_I2SCTRL1_REG;
	aw_pa->chansel_desc.rxchan_mask = AW_PID_2183_CHSEL_MASK;

	aw_pa->chansel_desc.rx_left = AW_PID_2183_CHSEL_LEFT_VALUE;
	aw_pa->chansel_desc.rx_right = AW_PID_2183_CHSEL_RIGHT_VALUE;

	aw_pa->tx_en_desc.tx_en_reg = AW_PID_2183_I2SCTRL3_REG;
	aw_pa->tx_en_desc.tx_en_mask = AW_PID_2183_I2STXEN_MASK;
	aw_pa->tx_en_desc.tx_disable = AW_PID_2183_I2STXEN_DISABLE_VALUE;

	aw_pa->dsp_st_desc.dsp_reg_s1 = AW_PID_2183_DSP_ST_S1;
	aw_pa->dsp_st_desc.dsp_reg_e1 = AW_PID_2183_DSP_ST_E1;
	aw_pa->dsp_st_desc.dsp_reg_s2 = AW_PID_2183_DSP_ST_S2;
	aw_pa->dsp_st_desc.dsp_reg_e2 = AW_PID_2183_DSP_ST_E2;

	aw_pa->crc_check_desc.ram_clk_reg = AW_PID_2183_I2SCFG1_REG;
	aw_pa->crc_check_desc.ram_clk_mask = AW_PID_2183_RAM_CG_BYP_MASK;
	aw_pa->crc_check_desc.ram_clk_on =
		AW_PID_2183_RAM_CG_BYP_BYPASS_VALUE;
	aw_pa->crc_check_desc.ram_clk_off =
		AW_PID_2183_RAM_CG_BYP_WORK_VALUE;

	aw_pa->crc_check_desc.crc_cfg_base_addr =
		AW_PID_2183_CRC_CFG_BASE_ADDR;
	aw_pa->crc_check_desc.crc_fw_base_addr =
		AW_PID_2183_CRC_FW_BASE_ADDR;

	aw_pa->crc_check_desc.crc_ctrl_reg = AW_PID_2183_CRCCTRL_REG;
	aw_pa->crc_check_desc.crc_end_addr_mask =
		AW_PID_2183_CRC_END_ADDR_MASK;
	aw_pa->crc_check_desc.crc_cfg_check_en_mask =
		AW_PID_2183_CRC_CFG_EN_MASK;
	aw_pa->crc_check_desc.crc_cfgcheck_disable =
		AW_PID_2183_CRC_CFG_EN_DISABLE_VALUE;
	aw_pa->crc_check_desc.crc_cfgcheck_enable =
		AW_PID_2183_CRC_CFG_EN_ENABLE_VALUE;
	aw_pa->crc_check_desc.crc_fw_check_en_mask =
		AW_PID_2183_CRC_CODE_EN_MASK;
	aw_pa->crc_check_desc.crc_fwcheck_disable =
		AW_PID_2183_CRC_CODE_EN_DISABLE_VALUE;
	aw_pa->crc_check_desc.crc_fwcheck_enable =
		AW_PID_2183_CRC_CODE_EN_ENABLE_VALUE;

	aw_pa->crc_check_desc.crc_check_reg = AW_PID_2183_HAGCST_REG;
	aw_pa->crc_check_desc.crc_check_mask =
		AW_PID_2183_CRC_CHECK_BITS_MASK;
	aw_pa->crc_check_desc.crc_check_pass =
		AW_PID_2183_CRC_CHECK_PASS_VAL;
	aw_pa->crc_check_desc.crc_check_bits_shift =
		AW_PID_2183_CRC_CHECK_START_BIT;

	aw_pa->crc_check_realtime_desc.addr = AW_PID_2183_CRC_REALTIME_ADDR;
	aw_pa->crc_check_realtime_desc.mask = AW_PID_2183_CRC_REALTIME_MASK;
	aw_pa->crc_check_realtime_desc.data_type = AW_DSP_16_DATA;
	aw_pa->crc_check_realtime_desc.enable =
		AW_PID_2183_CRC_REALTIME_ENABLE;
	aw_pa->crc_check_realtime_desc.disable =
		AW_PID_2183_CRC_REALTIME_DISABLE;

	aw_pa->crc_check_realtime_desc.check_addr =
		AW_PID_2183_CRC_REALTIME_CHECK_ADDR;
	aw_pa->crc_check_realtime_desc.check_mask =
		AW_PID_2183_CRC_REALTIME_CHECK_MASK;
	aw_pa->crc_check_realtime_desc.check_abnormal =
		AW_PID_2183_CRC_REALTIME_CHECK_ABNORMAL;
	aw_pa->crc_check_realtime_desc.check_normal =
		AW_PID_2183_CRC_REALTIME_CHECK_NORMAL;
	aw_pa->crc_check_realtime_desc.check_data_type = AW_DSP_16_DATA;

	aw_pa->fw_ver_desc.version_reg = AW_PID_2183_FIRMWARE_VERSION_REG;
	aw_pa->fw_ver_desc.data_type = AW_DSP_16_DATA;

	aw_pa->dither_desc.reg = AW_PID_2183_DBGCTRL_REG;
	aw_pa->dither_desc.mask = AW_PID_2183_DITHER_EN_MASK;
	aw_pa->dither_desc.enable = AW_PID_2183_DITHER_EN_ENABLE_VALUE;
	aw_pa->dither_desc.disable = AW_PID_2183_DITHER_EN_DISABLE_VALUE;

	aw_pa->dsp_ng_desc.reg = AW_PID_2183_DBGCTRL_REG;
	aw_pa->dsp_ng_desc.mask = AW_PID_2183_DSP_NG_EN_MASK;
	aw_pa->dsp_ng_desc.enable = AW_PID_2183_DSP_NG_EN_ENABLE;
	aw_pa->dsp_ng_desc.disable = AW_PID_2183_DSP_NG_EN_DISABLE;
	aw_pa->dsp_ng_desc.dsp_lp_reg = AW_PID_2183_DSP_LOW_POWER_SWITCH_CFG_ADDR;
	aw_pa->dsp_ng_desc.dsp_lp_data_type = AW_DSP_16_DATA;
	aw_pa->dsp_ng_desc.dsp_lp_mask =
		AW_PID_2183_DSP_LOW_POWER_SWITCH_CFG_MASK;
	aw_pa->dsp_ng_desc.dsp_lp_enable =
		AW_PID_2183_DSP_LOW_POWER_SWITCH_CFG_ENABLE;
	aw_pa->dsp_ng_desc.dsp_lp_disable =
		AW_PID_2183_DSP_LOW_POWER_SWITCH_CFG_DISABLE;

	return 0;
}
