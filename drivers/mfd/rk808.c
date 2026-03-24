// SPDX-License-Identifier: GPL-2.0-only
/*
 * MFD core driver for Rockchip RK808/RK818
 *
 * Copyright (c) 2014-2018, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Author: Chris Zhong <zyw@rock-chips.com>
 * Author: Zhang Qing <zhangqing@rock-chips.com>
 *
 * Copyright (C) 2016 PHYTEC Messtechnik GmbH
 *
 * Author: Wadim Egorov <w.egorov@phytec.de>
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/list_sort.h>
#include <linux/mfd/rk808.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/reboot.h>
#include <linux/syscore_ops.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/devinfo.h>

/* Reboot command list for register-only reset mode */
static const char * const pmic_rst_reg_only_cmd[] = {
	"loader", "bootloader", "fastboot", "recovery",
	"ums", "panic", "watchdog", "charge",
};

/* Global variables definition */
static LIST_HEAD(rk808_pmic_list);                  /* Global PMIC registry list */
static DEFINE_MUTEX(rk808_pmic_mutex);              /* Mutex for thread safety */
static atomic_t rk808_pmic_count = ATOMIC_INIT(0);  /* Atomic counter for PMIC count */

static bool rk801_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RK801_SYS_STS_REG:
	case RK801_INT_STS0_REG:
	case RK801_SYS_CFG0_REG:
	case RK801_SYS_CFG1_REG:
	case RK801_SYS_CFG2_REG:
	case RK801_SYS_CFG3_REG:
	case RK801_SYS_CFG4_REG:
	case RK801_SLEEP_CFG_REG:
		return true;
	}

	return false;
}

static bool rk808_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/*
	 * Notes:
	 * - Technically the ROUND_30s bit makes RTC_CTRL_REG volatile, but
	 *   we don't use that feature.  It's better to cache.
	 * - It's unlikely we care that RK808_DEVCTRL_REG is volatile since
	 *   bits are cleared in case when we shutoff anyway, but better safe.
	 */

	switch (reg) {
	case RK808_SECONDS_REG ... RK808_WEEKS_REG:
	case RK808_RTC_STATUS_REG:
	case RK808_VB_MON_REG:
	case RK808_THERMAL_REG:
	case RK808_DCDC_UV_STS_REG:
	case RK808_LDO_UV_STS_REG:
	case RK808_DCDC_PG_REG:
	case RK808_LDO_PG_REG:
	case RK808_DEVCTRL_REG:
	case RK808_INT_STS_REG1:
	case RK808_INT_STS_REG2:
		return true;
	}

	return false;
}

static bool rk817_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/*
	 * Notes:
	 * - Technically the ROUND_30s bit makes RTC_CTRL_REG volatile, but
	 *   we don't use that feature.  It's better to cache.
	 */

	switch (reg) {
	case RK817_SECONDS_REG ... RK817_WEEKS_REG:
	case RK817_RTC_STATUS_REG:
	case RK817_ADC_CONFIG0 ... RK817_CURE_ADC_K0:
	case RK817_PMIC_CHRG_STS:
	case RK817_PMIC_CHRG_OUT:
	case RK817_PMIC_CHRG_IN:
	case RK817_SYS_STS:
	case RK817_INT_STS_REG0:
	case RK817_INT_STS_REG1:
	case RK817_INT_STS_REG2:
		return true;
	}

	return false;
}

static bool rk818_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/*
	 * Notes:
	 * - Technically the ROUND_30s bit makes RTC_CTRL_REG volatile, but
	 *   we don't use that feature.  It's better to cache.
	 * - It's unlikely we care that RK808_DEVCTRL_REG is volatile since
	 *   bits are cleared in case when we shutoff anyway, but better safe.
	 */

	switch (reg) {
	case RK808_SECONDS_REG ... RK808_WEEKS_REG:
	case RK808_RTC_STATUS_REG:
	case RK808_VB_MON_REG:
	case RK808_THERMAL_REG:
	case RK808_DCDC_EN_REG:
	case RK808_LDO_EN_REG:
	case RK808_DCDC_UV_STS_REG:
	case RK808_LDO_UV_STS_REG:
	case RK808_DCDC_PG_REG:
	case RK808_LDO_PG_REG:
	case RK808_DEVCTRL_REG:
	case RK808_INT_STS_REG1:
	case RK808_INT_STS_REG2:
	case RK808_INT_STS_MSK_REG1:
	case RK808_INT_STS_MSK_REG2:
	case RK816_INT_STS_REG1:
	case RK816_INT_STS_MSK_REG1:
	case RK818_SUP_STS_REG ... RK818_SAVE_DATA19:
		return true;
	}

	return false;
}

static const struct regmap_config rk818_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RK818_SAVE_DATA19,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = rk818_is_volatile_reg,
};

static const struct regmap_config rk801_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RK801_SYS_CFG3_OTP_REG,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = rk801_is_volatile_reg,
};

static const struct regmap_config rk805_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RK805B_SYS_CFG3,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = rk808_is_volatile_reg,
};

static const struct regmap_config rk808_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RK808_IO_POL_REG,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = rk808_is_volatile_reg,
};

static const struct regmap_config rk816_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RK816_DATA18_REG,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = rk818_is_volatile_reg,
};

static const struct regmap_config rk817_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RK817_GPIO_INT_CFG,
	.num_reg_defaults_raw = RK817_GPIO_INT_CFG + 1,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = rk817_is_volatile_reg,
};

static const struct resource rtc_resources[] = {
	DEFINE_RES_IRQ(RK808_IRQ_RTC_ALARM),
};

static const struct resource rk816_rtc_resources[] = {
	DEFINE_RES_IRQ(RK816_IRQ_RTC_ALARM),
};

static const struct resource rk817_rtc_resources[] = {
	DEFINE_RES_IRQ(RK817_IRQ_RTC_ALARM),
};

static const struct resource rk801_key_resources[] = {
	DEFINE_RES_IRQ(RK801_IRQ_PWRON_FALL),
	DEFINE_RES_IRQ(RK801_IRQ_PWRON_RISE),
};

static const struct resource rk805_key_resources[] = {
	DEFINE_RES_IRQ(RK805_IRQ_PWRON_FALL),
	DEFINE_RES_IRQ(RK805_IRQ_PWRON_RISE),
};

static const struct resource rk816_pwrkey_resources[] = {
	DEFINE_RES_IRQ(RK816_IRQ_PWRON_FALL),
	DEFINE_RES_IRQ(RK816_IRQ_PWRON_RISE),
};

static const struct resource rk817_pwrkey_resources[] = {
	DEFINE_RES_IRQ(RK817_IRQ_PWRON_FALL),
	DEFINE_RES_IRQ(RK817_IRQ_PWRON_RISE),
};

static const struct mfd_cell rk801s[] = {
	{ .name = "rk801-regulator", },
	{
		.name = "rk801-pwrkey",
		.num_resources = ARRAY_SIZE(rk801_key_resources),
		.resources = &rk801_key_resources[0],
	},
};

static const struct mfd_cell rk805s[] = {
	{ .name = "rk805-clkout", },
	{ .name = "rk805-regulator", },
	{
		.name = "rk805-rtc",
		.num_resources = ARRAY_SIZE(rtc_resources),
		.resources = &rtc_resources[0],
	},
	{	.name = "rk805-pwrkey",
		.num_resources = ARRAY_SIZE(rk805_key_resources),
		.resources = &rk805_key_resources[0],
	},
};

static const struct mfd_cell rk808s[] = {
	{ .name = "rk808-clkout", },
	{ .name = "rk808-regulator", },
	{
		.name = "rk808-rtc",
		.num_resources = ARRAY_SIZE(rtc_resources),
		.resources = rtc_resources,
	},
};

static const struct mfd_cell rk816s[] = {
	{ .name = "rk816-clkout", },
	{ .name = "rk816-regulator", },
	{ .name = "rk816-battery", .of_compatible = "rk816-battery", },
	{
		.name = "rk816-pwrkey",
		.num_resources = ARRAY_SIZE(rk816_pwrkey_resources),
		.resources = &rk816_pwrkey_resources[0],
	},
	{
		.name = "rk816-rtc",
		.num_resources = ARRAY_SIZE(rk816_rtc_resources),
		.resources = &rk816_rtc_resources[0],
	},
};

static const struct mfd_cell rk817s[] = {
	{ .name = "rk817-clkout",},
	{ .name = "rk817-regulator",},
	{ .name = "rk817-battery", .of_compatible = "rk817,battery", },
	{ .name = "rk817-charger", .of_compatible = "rk817,charger", },
	{
		.name = "rk817-pwrkey",
		.num_resources = ARRAY_SIZE(rk817_pwrkey_resources),
		.resources = &rk817_pwrkey_resources[0],
	},
	{
		.name = "rk817-rtc",
		.num_resources = ARRAY_SIZE(rk817_rtc_resources),
		.resources = &rk817_rtc_resources[0],
	},
	{ .name = "rk817-codec", .of_compatible = "rockchip,rk817-codec", },
};

static const struct mfd_cell rk818s[] = {
	{ .name = "rk818-clkout", },
	{ .name = "rk818-regulator", },
	{ .name = "rk818-battery", .of_compatible = "rk818-battery", },
	{ .name = "rk818-charger", },
	{
		.name = "rk818-rtc",
		.num_resources = ARRAY_SIZE(rtc_resources),
		.resources = rtc_resources,
	},
};

static const struct rk808_reg_data rk801_pre_init_reg[] = {
	{ RK801_SLEEP_CFG_REG, RK801_SLEEP_FUN_MSK, RK801_NONE_FUN },
	{ RK801_SYS_CFG2_REG, RK801_RST_MSK, RK801_RST_RESTART_REG_RESETB },
	{ RK801_INT_CONFIG_REG, RK801_INT_POL_MSK, RK801_INT_ACT_L },
	{ RK801_POWER_FPWM_EN_REG, RK801_PLDO_HRDEC_EN, RK801_PLDO_HRDEC_EN },
	{ RK801_BUCK_DEBUG5_REG, 0xff, 0x54 },
	{ RK801_CON_BACK1_REG, 0xff, 0x18 },
};

static const struct rk808_reg_data rk805_pre_init_reg[] = {
	{RK805_BUCK4_CONFIG_REG, BUCK_ILMIN_MASK, BUCK_ILMIN_400MA},
	{RK805_GPIO_IO_POL_REG, SLP_SD_MSK, SLEEP_FUN},
	{RK805_THERMAL_REG, TEMP_HOTDIE_MSK, TEMP115C},
};

static struct rk808_reg_data rk805_suspend_reg[] = {
	{RK805_BUCK3_CONFIG_REG, PWM_MODE_MSK, AUTO_PWM_MODE},
};

static struct rk808_reg_data rk805_resume_reg[] = {
	{RK805_BUCK3_CONFIG_REG, PWM_MODE_MSK, FPWM_MODE},
};

static const struct rk808_reg_data rk808_pre_init_reg[] = {
	{ RK808_BUCK3_CONFIG_REG, BUCK_ILMIN_MASK,  BUCK_ILMIN_150MA },
	{ RK808_BUCK4_CONFIG_REG, BUCK_ILMIN_MASK,  BUCK_ILMIN_200MA },
	{ RK808_BOOST_CONFIG_REG, BOOST_ILMIN_MASK, BOOST_ILMIN_100MA },
	{ RK808_BUCK1_CONFIG_REG, BUCK1_RATE_MASK,  BUCK_ILMIN_200MA },
	{ RK808_BUCK2_CONFIG_REG, BUCK2_RATE_MASK,  BUCK_ILMIN_200MA },
	{ RK808_DCDC_UV_ACT_REG,  BUCK_UV_ACT_MASK, BUCK_UV_ACT_DISABLE},
	{ RK808_VB_MON_REG,       MASK_ALL,         VB_LO_ACT |
						    VB_LO_SEL_3500MV },
};

static const struct rk808_reg_data rk816_pre_init_reg[] = {
	/* buck4 Max ILMIT*/
	{ RK816_BUCK4_CONFIG_REG, REG_WRITE_MSK, BUCK4_MAX_ILIMIT },
	/* hotdie temperature: 105c*/
	{ RK816_THERMAL_REG, REG_WRITE_MSK, TEMP105C },
	/* set buck 12.5mv/us */
	{ RK816_BUCK1_CONFIG_REG, BUCK_RATE_MSK, BUCK_RATE_12_5MV_US },
	{ RK816_BUCK2_CONFIG_REG, BUCK_RATE_MSK, BUCK_RATE_12_5MV_US },
	/* enable RTC_PERIOD & RTC_ALARM int */
	{ RK816_INT_STS_MSK_REG2, REG_WRITE_MSK, RTC_PERIOD_ALARM_INT_EN },
	/* set bat 3.0 low and act shutdown */
	{ RK816_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK816_VBAT_LOW_3V0 | EN_VABT_LOW_SHUT_DOWN },
	/* enable PWRON rising/faling int */
	{ RK816_INT_STS_MSK_REG1, REG_WRITE_MSK, RK816_PWRON_FALL_RISE_INT_EN },
	/* enable PLUG IN/OUT int */
	{ RK816_INT_STS_MSK_REG3, REG_WRITE_MSK, PLUGIN_OUT_INT_EN },
	/* clear int flags */
	{ RK816_INT_STS_REG1, REG_WRITE_MSK, ALL_INT_FLAGS_ST },
	{ RK816_INT_STS_REG2, REG_WRITE_MSK, ALL_INT_FLAGS_ST },
	{ RK816_INT_STS_REG3, REG_WRITE_MSK, ALL_INT_FLAGS_ST },
	{ RK816_DCDC_EN_REG2, BOOST_EN_MASK, BOOST_DISABLE },
	/* set write mask bit 1, otherwise 'is_enabled()' get wrong status */
	{ RK816_LDO_EN_REG1, REGS_WMSK, REGS_WMSK },
	{ RK816_LDO_EN_REG2, REGS_WMSK, REGS_WMSK },
};

static const struct rk808_reg_data rk817_pre_init_reg[] = {
	{RK817_SYS_CFG(3), RK817_SLPPOL_MSK, RK817_SLPPOL_L},
	/* Codec specific registers */
	{ RK817_CODEC_DTOP_VUCTL, MASK_ALL, 0x03 },
	{ RK817_CODEC_DTOP_VUCTIME, MASK_ALL, 0x00 },
	{ RK817_CODEC_DTOP_LPT_SRST, MASK_ALL, 0x00 },
	{ RK817_CODEC_DTOP_DIGEN_CLKE, MASK_ALL, 0x00 },
	/* from vendor driver, CODEC_AREF_RTCFG0 not defined in data sheet */
	{ RK817_CODEC_AREF_RTCFG0, MASK_ALL, 0x00 },
	{ RK817_CODEC_AREF_RTCFG1, MASK_ALL, 0x06 },
	{ RK817_CODEC_AADC_CFG0, MASK_ALL, 0xc8 },
	/* from vendor driver, CODEC_AADC_CFG1 not defined in data sheet */
	{ RK817_CODEC_AADC_CFG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_VOLL, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_VOLR, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_SR_ACL0, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_ALC1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_ALC2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_NG, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_HPF, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_RVOLL, MASK_ALL, 0xff },
	{ RK817_CODEC_DADC_RVOLR, MASK_ALL, 0xff },
	{ RK817_CODEC_AMIC_CFG0, MASK_ALL, 0x70 },
	{ RK817_CODEC_AMIC_CFG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_PGA_GAIN, MASK_ALL, 0x66 },
	{ RK817_CODEC_DMIC_LMT1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_LMT2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_NG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_NG2, MASK_ALL, 0x00 },
	/* from vendor driver, CODEC_ADAC_CFG0 not defined in data sheet */
	{ RK817_CODEC_ADAC_CFG0, MASK_ALL, 0x00 },
	{ RK817_CODEC_ADAC_CFG1, MASK_ALL, 0x07 },
	{ RK817_CODEC_DDAC_POPD_DACST, MASK_ALL, 0x82 },
	{ RK817_CODEC_DDAC_VOLL, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_VOLR, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_SR_LMT0, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_LMT1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_LMT2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_MUTE_MIXCTL, MASK_ALL, 0xa0 },
	{ RK817_CODEC_DDAC_RVOLL, MASK_ALL, 0xff },
	{ RK817_CODEC_DADC_RVOLR, MASK_ALL, 0xff },
	{ RK817_CODEC_AMIC_CFG0, MASK_ALL, 0x70 },
	{ RK817_CODEC_AMIC_CFG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_PGA_GAIN, MASK_ALL, 0x66 },
	{ RK817_CODEC_DMIC_LMT1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_LMT2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_NG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_NG2, MASK_ALL, 0x00 },
	/* from vendor driver, CODEC_ADAC_CFG0 not defined in data sheet */
	{ RK817_CODEC_ADAC_CFG0, MASK_ALL, 0x00 },
	{ RK817_CODEC_ADAC_CFG1, MASK_ALL, 0x07 },
	{ RK817_CODEC_DDAC_POPD_DACST, MASK_ALL, 0x82 },
	{ RK817_CODEC_DDAC_VOLL, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_VOLR, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_SR_LMT0, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_LMT1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_LMT2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_MUTE_MIXCTL, MASK_ALL, 0xa0 },
	{ RK817_CODEC_DDAC_RVOLL, MASK_ALL, 0xff },
	{ RK817_CODEC_DDAC_RVOLR, MASK_ALL, 0xff },
	{ RK817_CODEC_AHP_ANTI0, MASK_ALL, 0x00 },
	{ RK817_CODEC_AHP_ANTI1, MASK_ALL, 0x00 },
	{ RK817_CODEC_AHP_CFG0, MASK_ALL, 0xe0 },
	{ RK817_CODEC_AHP_CFG1, MASK_ALL, 0x1f },
	{ RK817_CODEC_AHP_CP, MASK_ALL, 0x09 },
	{ RK817_CODEC_ACLASSD_CFG1, MASK_ALL, 0x69 },
	{ RK817_CODEC_ACLASSD_CFG2, MASK_ALL, 0x44 },
	{ RK817_CODEC_APLL_CFG0, MASK_ALL, 0x04 },
	{ RK817_CODEC_APLL_CFG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_APLL_CFG2, MASK_ALL, 0x30 },
	{ RK817_CODEC_APLL_CFG3, MASK_ALL, 0x19 },
	{ RK817_CODEC_APLL_CFG4, MASK_ALL, 0x65 },
	{ RK817_CODEC_APLL_CFG5, MASK_ALL, 0x01 },
	{ RK817_CODEC_DI2S_CKM, MASK_ALL, 0x01 },
	{ RK817_CODEC_DI2S_RSD, MASK_ALL, 0x00 },
	{ RK817_CODEC_DI2S_RXCR1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DI2S_RXCR2, MASK_ALL, 0x17 },
	{ RK817_CODEC_DI2S_RXCMD_TSD, MASK_ALL, 0x00 },
	{ RK817_CODEC_DI2S_TXCR1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DI2S_TXCR2, MASK_ALL, 0x17 },
	{ RK817_CODEC_DI2S_TXCR3_TXCMD, MASK_ALL, 0x00 },
	{RK817_GPIO_INT_CFG, RK817_INT_POL_MSK, RK817_INT_POL_L},
	{RK817_SYS_CFG(1), RK817_HOTDIE_TEMP_MSK | RK817_TSD_TEMP_MSK,
					   RK817_HOTDIE_105 | RK817_TSD_140},
};

static const struct rk808_reg_data rk818_pre_init_reg[] = {
	/* improve efficiency */
	{ RK818_BUCK2_CONFIG_REG, BUCK2_RATE_MASK,  BUCK_ILMIN_250MA },
	{ RK818_BUCK4_CONFIG_REG, BUCK_ILMIN_MASK,  BUCK_ILMIN_250MA },
	{ RK818_BOOST_CONFIG_REG, BOOST_ILMIN_MASK, BOOST_ILMIN_100MA },
	{ RK818_USB_CTRL_REG,	  RK818_USB_ILIM_SEL_MASK,
						    RK818_USB_ILMIN_2000MA },
	/* close charger when usb lower then 3.4V */
	{ RK818_USB_CTRL_REG,	  RK818_USB_CHG_SD_VSEL_MASK,
						    (0x7 << 4) },
	/* no action when vref */
	{ RK818_H5V_EN_REG,	  BIT(1),	    RK818_REF_RDY_CTRL },
	/* enable HDMI 5V */
	{ RK818_H5V_EN_REG,	  BIT(0),	    RK818_H5V_EN },
	{ RK808_VB_MON_REG,	  MASK_ALL,	    VB_LO_ACT |
						    VB_LO_SEL_3500MV },
	{RK808_CLK32OUT_REG, CLK32KOUT2_FUNC_MASK, CLK32KOUT2_FUNC},
};

static const struct regmap_irq rk801_irqs[] = {
	[RK801_IRQ_PWRON_FALL] = {
		.mask = RK801_IRQ_PWRON_FALL_MSK,
		.reg_offset = 0,
	},
	[RK801_IRQ_PWRON_RISE] = {
		.mask = RK801_IRQ_PWRON_RISE_MSK,
		.reg_offset = 0,
	},
	[RK801_IRQ_PWRON] = {
		.mask = RK801_IRQ_PWRON_MSK,
		.reg_offset = 0,
	},
	[RK801_IRQ_PWRON_LP] = {
		.mask = RK801_IRQ_PWRON_LP_MSK,
		.reg_offset = 0,
	},
	[RK801_IRQ_HOTDIE] = {
		.mask = RK801_IRQ_HOTDIE_MSK,
		.reg_offset = 0,
	},
	[RK801_IRQ_VDC_RISE] = {
		.mask = RK801_IRQ_VDC_RISE_MSK,
		.reg_offset = 0,
	},
	[RK801_IRQ_VDC_FALL] = {
		.mask = RK801_IRQ_VDC_FALL_MSK,
		.reg_offset = 0,
	},
};

static const struct regmap_irq rk805_irqs[] = {
	[RK805_IRQ_PWRON_RISE] = {
		.mask = RK805_IRQ_PWRON_RISE_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_VB_LOW] = {
		.mask = RK805_IRQ_VB_LOW_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_PWRON] = {
		.mask = RK805_IRQ_PWRON_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_PWRON_LP] = {
		.mask = RK805_IRQ_PWRON_LP_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_HOTDIE] = {
		.mask = RK805_IRQ_HOTDIE_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_RTC_ALARM] = {
		.mask = RK805_IRQ_RTC_ALARM_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_RTC_PERIOD] = {
		.mask = RK805_IRQ_RTC_PERIOD_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_PWRON_FALL] = {
		.mask = RK805_IRQ_PWRON_FALL_MSK,
		.reg_offset = 0,
	},
};

static const struct regmap_irq rk808_irqs[] = {
	/* INT_STS */
	[RK808_IRQ_VOUT_LO] = {
		.mask = RK808_IRQ_VOUT_LO_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_VB_LO] = {
		.mask = RK808_IRQ_VB_LO_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_PWRON] = {
		.mask = RK808_IRQ_PWRON_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_PWRON_LP] = {
		.mask = RK808_IRQ_PWRON_LP_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_HOTDIE] = {
		.mask = RK808_IRQ_HOTDIE_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_RTC_ALARM] = {
		.mask = RK808_IRQ_RTC_ALARM_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_RTC_PERIOD] = {
		.mask = RK808_IRQ_RTC_PERIOD_MSK,
		.reg_offset = 0,
	},

	/* INT_STS2 */
	[RK808_IRQ_PLUG_IN_INT] = {
		.mask = RK808_IRQ_PLUG_IN_INT_MSK,
		.reg_offset = 1,
	},
	[RK808_IRQ_PLUG_OUT_INT] = {
		.mask = RK808_IRQ_PLUG_OUT_INT_MSK,
		.reg_offset = 1,
	},
};

static struct rk808_reg_data rk816_suspend_reg[] = {
	/* set bat 3.4v low and act irq */
	{ RK816_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK816_VBAT_LOW_3V4 | EN_VBAT_LOW_IRQ },
};

static struct rk808_reg_data rk816_resume_reg[] = {
	/* set bat 3.0v low and act shutdown */
	{ RK816_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK816_VBAT_LOW_3V0 | EN_VABT_LOW_SHUT_DOWN },
};

static const struct regmap_irq rk816_irqs[] = {
	/* INT_STS */
	[RK816_IRQ_PWRON_FALL] = {
		.mask = RK816_IRQ_PWRON_FALL_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_PWRON_RISE] = {
		.mask = RK816_IRQ_PWRON_RISE_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_VB_LOW] = {
		.mask = RK816_IRQ_VB_LOW_MSK,
		.reg_offset = 1,
	},
	[RK816_IRQ_PWRON] = {
		.mask = RK816_IRQ_PWRON_MSK,
		.reg_offset = 1,
	},
	[RK816_IRQ_PWRON_LP] = {
		.mask = RK816_IRQ_PWRON_LP_MSK,
		.reg_offset = 1,
	},
	[RK816_IRQ_HOTDIE] = {
		.mask = RK816_IRQ_HOTDIE_MSK,
		.reg_offset = 1,
	},
	[RK816_IRQ_RTC_ALARM] = {
		.mask = RK816_IRQ_RTC_ALARM_MSK,
		.reg_offset = 1,
	},
	[RK816_IRQ_RTC_PERIOD] = {
		.mask = RK816_IRQ_RTC_PERIOD_MSK,
		.reg_offset = 1,
	},
	[RK816_IRQ_USB_OV] = {
		.mask = RK816_IRQ_USB_OV_MSK,
		.reg_offset = 1,
	},
};

static struct rk808_reg_data rk818_suspend_reg[] = {
	/* set bat 3.4v low and act irq */
	{ RK808_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK808_VBAT_LOW_3V4 | EN_VBAT_LOW_IRQ },
};

static struct rk808_reg_data rk818_resume_reg[] = {
	/* set bat 3.0v low and act shutdown */
	{ RK808_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK808_VBAT_LOW_3V0 | EN_VABT_LOW_SHUT_DOWN },
};

static const struct regmap_irq rk818_irqs[] = {
	/* INT_STS */
	[RK818_IRQ_VOUT_LO] = {
		.mask = RK818_IRQ_VOUT_LO_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_VB_LO] = {
		.mask = RK818_IRQ_VB_LO_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_PWRON] = {
		.mask = RK818_IRQ_PWRON_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_PWRON_LP] = {
		.mask = RK818_IRQ_PWRON_LP_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_HOTDIE] = {
		.mask = RK818_IRQ_HOTDIE_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_RTC_ALARM] = {
		.mask = RK818_IRQ_RTC_ALARM_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_RTC_PERIOD] = {
		.mask = RK818_IRQ_RTC_PERIOD_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_USB_OV] = {
		.mask = RK818_IRQ_USB_OV_MSK,
		.reg_offset = 0,
	},

	/* INT_STS2 */
	[RK818_IRQ_PLUG_IN] = {
		.mask = RK818_IRQ_PLUG_IN_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_PLUG_OUT] = {
		.mask = RK818_IRQ_PLUG_OUT_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_CHG_OK] = {
		.mask = RK818_IRQ_CHG_OK_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_CHG_TE] = {
		.mask = RK818_IRQ_CHG_TE_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_CHG_TS1] = {
		.mask = RK818_IRQ_CHG_TS1_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_TS2] = {
		.mask = RK818_IRQ_TS2_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_CHG_CVTLIM] = {
		.mask = RK818_IRQ_CHG_CVTLIM_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_DISCHG_ILIM] = {
		.mask = RK818_IRQ_DISCHG_ILIM_MSK,
		.reg_offset = 1,
	},
};

static const struct regmap_irq rk817_irqs[RK817_IRQ_END] = {
	REGMAP_IRQ_REG_LINE(0, 8),
	REGMAP_IRQ_REG_LINE(1, 8),
	REGMAP_IRQ_REG_LINE(2, 8),
	REGMAP_IRQ_REG_LINE(3, 8),
	REGMAP_IRQ_REG_LINE(4, 8),
	REGMAP_IRQ_REG_LINE(5, 8),
	REGMAP_IRQ_REG_LINE(6, 8),
	REGMAP_IRQ_REG_LINE(7, 8),
	REGMAP_IRQ_REG_LINE(8, 8),
	REGMAP_IRQ_REG_LINE(9, 8),
	REGMAP_IRQ_REG_LINE(10, 8),
	REGMAP_IRQ_REG_LINE(11, 8),
	REGMAP_IRQ_REG_LINE(12, 8),
	REGMAP_IRQ_REG_LINE(13, 8),
	REGMAP_IRQ_REG_LINE(14, 8),
	REGMAP_IRQ_REG_LINE(15, 8),
	REGMAP_IRQ_REG_LINE(16, 8),
	REGMAP_IRQ_REG_LINE(17, 8),
	REGMAP_IRQ_REG_LINE(18, 8),
	REGMAP_IRQ_REG_LINE(19, 8),
	REGMAP_IRQ_REG_LINE(20, 8),
	REGMAP_IRQ_REG_LINE(21, 8),
	REGMAP_IRQ_REG_LINE(22, 8),
	REGMAP_IRQ_REG_LINE(23, 8)
};

static const struct regmap_irq_chip rk801_irq_chip = {
	.name = "rk801",
	.irqs = rk801_irqs,
	.num_irqs = ARRAY_SIZE(rk801_irqs),
	.num_regs = 1,
	.status_base = RK801_INT_STS0_REG,
	.mask_base = RK801_INT_MASK0_REG,
	.ack_base = RK801_INT_STS0_REG,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk805_irq_chip = {
	.name = "rk805",
	.irqs = rk805_irqs,
	.num_irqs = ARRAY_SIZE(rk805_irqs),
	.num_regs = 1,
	.status_base = RK805_INT_STS_REG,
	.mask_base = RK805_INT_STS_MSK_REG,
	.ack_base = RK805_INT_STS_REG,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk808_irq_chip = {
	.name = "rk808",
	.irqs = rk808_irqs,
	.num_irqs = ARRAY_SIZE(rk808_irqs),
	.num_regs = 2,
	.irq_reg_stride = 2,
	.status_base = RK808_INT_STS_REG1,
	.mask_base = RK808_INT_STS_MSK_REG1,
	.ack_base = RK808_INT_STS_REG1,
	.init_ack_masked = true,
};

static const struct regmap_irq rk816_battery_irqs[] = {
	/* INT_STS */
	[RK816_IRQ_PLUG_IN] = {
		.mask = RK816_IRQ_PLUG_IN_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_PLUG_OUT] = {
		.mask = RK816_IRQ_PLUG_OUT_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_CHG_OK] = {
		.mask = RK816_IRQ_CHG_OK_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_CHG_TE] = {
		.mask = RK816_IRQ_CHG_TE_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_CHG_TS] = {
		.mask = RK816_IRQ_CHG_TS_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_CHG_CVTLIM] = {
		.mask = RK816_IRQ_CHG_CVTLIM_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_DISCHG_ILIM] = {
		.mask = RK816_IRQ_DISCHG_ILIM_MSK,
		.reg_offset = 0,
	},
};

static const struct regmap_irq_chip rk816_irq_chip = {
	.name = "rk816",
	.irqs = rk816_irqs,
	.num_irqs = ARRAY_SIZE(rk816_irqs),
	.num_regs = 2,
	.irq_reg_stride = 3,
	.status_base = RK816_INT_STS_REG1,
	.mask_base = RK816_INT_STS_MSK_REG1,
	.ack_base = RK816_INT_STS_REG1,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk816_battery_irq_chip = {
	.name = "rk816_battery",
	.irqs = rk816_battery_irqs,
	.num_irqs = ARRAY_SIZE(rk816_battery_irqs),
	.num_regs = 1,
	.status_base = RK816_INT_STS_REG3,
	.mask_base = RK816_INT_STS_MSK_REG3,
	.ack_base = RK816_INT_STS_REG3,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk817_irq_chip = {
	.name = "rk817",
	.irqs = rk817_irqs,
	.num_irqs = ARRAY_SIZE(rk817_irqs),
	.num_regs = 3,
	.irq_reg_stride = 2,
	.status_base = RK817_INT_STS_REG0,
	.mask_base = RK817_INT_STS_MSK_REG0,
	.ack_base = RK817_INT_STS_REG0,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk818_irq_chip = {
	.name = "rk818",
	.irqs = rk818_irqs,
	.num_irqs = ARRAY_SIZE(rk818_irqs),
	.num_regs = 2,
	.irq_reg_stride = 2,
	.status_base = RK818_INT_STS_REG1,
	.mask_base = RK818_INT_STS_MSK_REG1,
	.ack_base = RK818_INT_STS_REG1,
	.init_ack_masked = true,
};

static inline int rk801_act_pol(bool act_low)
{
	return act_low ? RK801_SLEEP_ACT_L : RK801_SLEEP_ACT_H;
}

static inline int rk801_inact_pol(bool act_low)
{
	return act_low ? RK801_SLEEP_ACT_H : RK801_SLEEP_ACT_L;
}

static void rk801_device_reboot(struct rk808 *rk808)
{
	int ret, act_pol;

	if (!rk808 || !rk808->pins || !rk808->pins->reset)
		return;

	regmap_update_bits(rk808->regmap, RK801_SLEEP_CFG_REG,
			   RK801_SLEEP_FUN_MSK, RK801_NONE_FUN);

	ret = pinctrl_select_state(rk808->pins->p, rk808->pins->reset);
	if (ret)
		pr_err("failed to pmic-reset pinctrl state, ret=%d\n", ret);

	/* raw value ! */
	act_pol = gpiod_get_raw_value(rk808->pwrctrl.gpio) ?
				RK801_SLEEP_ACT_L : RK801_SLEEP_ACT_H;
	regmap_update_bits(rk808->regmap, RK801_SYS_CFG2_REG,
			   RK801_SLEEP_POL_MSK, act_pol);

	/* pmic rst func: register + 5ms-npor-signal */
	regmap_update_bits(rk808->regmap, RK801_SYS_CFG2_REG,
			   RK801_RST_MSK, RK801_RST_RESTART_REG_RESETB);
	regmap_update_bits(rk808->regmap, RK801_SLEEP_CFG_REG,
			   RK801_SLEEP_FUN_MSK, RK801_RESET_FUN);

	dev_info(&rk808->i2c->dev, "rk801 system reboot ready\n");
}

static int rk801_device_shutdown_prepare(struct sys_off_data *data)
{
	int ret = 0;
	struct rk808 *rk808 = data->cb_data;

	if (!rk808)
		return -1;

	ret = regmap_update_bits(rk808->regmap, RK801_SYS_CFG2_REG,
				 RK801_SLEEP_POL_MSK,
				 rk801_act_pol(rk808->pwrctrl.act_low));
	if (ret < 0)
		return ret;

	return regmap_update_bits(rk808->regmap, RK801_SLEEP_CFG_REG,
				  RK801_SLEEP_FUN_MSK, RK801_SHUTDOWN_FUN);
}

static int rk805_device_shutdown_prepare(struct sys_off_data *data)
{
	int ret = 0;
	struct rk808 *rk808 = data->cb_data;

	if (!rk808)
		return -1;

	ret = regmap_update_bits(rk808->regmap,
				 RK805_GPIO_IO_POL_REG,
				 SLP_SD_MSK, SHUTDOWN_FUN);
	if (ret)
		dev_err(&rk808->i2c->dev, "Failed to shutdown device!\n");
	return ret;
}

static int rk817_shutdown_prepare(struct sys_off_data *data)
{
	int ret = 0;
	struct rk808 *rk808 = data->cb_data;

	/* close rtc int when power off */
	regmap_update_bits(rk808->regmap,
			   RK817_INT_STS_MSK_REG0,
			   (0x3 << 5), (0x3 << 5));
	regmap_update_bits(rk808->regmap,
			   RK817_RTC_INT_REG,
			   (0x3 << 2), (0x0 << 2));

	if (rk808->pins && rk808->pins->p && rk808->pins->power_off) {
		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_SLPPIN_FUNC_MSK,
					 SLPPIN_NULL_FUN);
		if (ret)
			pr_err("shutdown: config SLPPIN_NULL_FUN error!\n");

		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_SLPPOL_MSK,
					 RK817_SLPPOL_H);
		if (ret)
			pr_err("shutdown: config RK817_SLPPOL_H error!\n");

		ret = pinctrl_select_state(rk808->pins->p,
					   rk808->pins->power_off);
		if (ret)
			pr_info("%s:failed to activate pwroff state\n",
				__func__);
		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_SLPPIN_FUNC_MSK,
					 SLPPIN_DN_FUN);
		if (ret)
			pr_err("shutdown: config SLPPIN_DN_FUN error!\n");
	} else {
		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_SLPPIN_FUNC_MSK,
					 SLPPIN_NULL_FUN);
		if (ret)
			pr_err("shutdown: config SLPPIN_NULL_FUN error!\n");

		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_SLPPOL_MSK,
					 RK817_SLPPOL_H);
		if (ret)
			pr_err("shutdown: config RK817_SLPPOL_H error!\n");
		/* pmic sleep shutdown function */
		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_SLPPIN_FUNC_MSK, SLPPIN_DN_FUN);
		if (ret)
			dev_err(&rk808->i2c->dev, "Failed to shutdown device!\n");
		/* pmic need the SCL clock to synchronize register */
		mdelay(2);
	}
	return ret;
}

/* PMIC-specific implementations */
static int rk801_shutdown(struct rk808 *rk808)
{
	int ret = 0;

	if (system_state == SYSTEM_POWER_OFF) {
		ret = regmap_update_bits(rk808->regmap, RK801_SYS_CFG2_REG,
					 DEV_OFF, DEV_OFF);
		if (ret)
			dev_err(&rk808->i2c->dev, "Failed to shutdown device!\n");
		while (1)
			;
	} else if (system_state == SYSTEM_RESTART) {
		rk801_device_reboot(rk808);
	}

	return ret;
}

static int rk805_shutdown(struct rk808 *rk808)
{
	int ret = 0;

	/* close rtc int when power off */
	regmap_update_bits(rk808->regmap,
			   RK817_INT_STS_MSK_REG0,
			   (0x3 << 5), (0x3 << 5));
	regmap_update_bits(rk808->regmap,
			   RK817_RTC_INT_REG,
			   (0x3 << 2), (0x0 << 2));

	if (system_state != SYSTEM_POWER_OFF)
		return ret;

	mdelay(200);

	ret = regmap_update_bits(rk808->regmap, RK805_DEV_CTRL_REG,
				 DEV_OFF, DEV_OFF);
	if (ret)
		dev_err(&rk808->i2c->dev, "Failed to shutdown device!\n");

	while (1)
		;
}

static int rk808_shutdown(struct rk808 *rk808)
{
	int ret = 0;

	/* close rtc int when power off */
	regmap_update_bits(rk808->regmap,
			   RK817_INT_STS_MSK_REG0,
			   (0x3 << 5), (0x3 << 5));
	regmap_update_bits(rk808->regmap,
			   RK817_RTC_INT_REG,
			   (0x3 << 2), (0x0 << 2));

	if (system_state != SYSTEM_POWER_OFF)
		return ret;

	ret = regmap_update_bits(rk808->regmap, RK808_DEVCTRL_REG,
				 DEV_OFF_RST, DEV_OFF_RST);
	if (ret)
		dev_err(&rk808->i2c->dev, "Failed to shutdown device!\n");
	while (1)
		;
}

static int rk816_shutdown(struct rk808 *rk808)
{
	int ret = 0;

	/* close rtc int when power off */
	regmap_update_bits(rk808->regmap,
			   RK817_INT_STS_MSK_REG0,
			   (0x3 << 5), (0x3 << 5));
	regmap_update_bits(rk808->regmap,
			   RK817_RTC_INT_REG,
			   (0x3 << 2), (0x0 << 2));

	if (system_state != SYSTEM_POWER_OFF)
		return ret;

	ret = regmap_update_bits(rk808->regmap, RK816_DEV_CTRL_REG,
				 DEV_OFF, DEV_OFF);
	if (ret)
		dev_err(&rk808->i2c->dev, "Failed to shutdown device!\n");
	while (1)
		;
}

static int rk817_shutdown(struct rk808 *rk808)
{
	int ret = 0;

	/* close rtc int when power off */
	regmap_update_bits(rk808->regmap,
			   RK817_INT_STS_MSK_REG0,
			   (0x3 << 5), (0x3 << 5));
	regmap_update_bits(rk808->regmap,
			   RK817_RTC_INT_REG,
			   (0x3 << 2), (0x0 << 2));

	if (system_state != SYSTEM_POWER_OFF)
		return ret;

	ret = regmap_update_bits(rk808->regmap, RK817_SYS_CFG(3),
				 DEV_OFF, DEV_OFF);
	if (ret)
		dev_err(&rk808->i2c->dev, "Failed to shutdown device!\n");

	while (1)
		;
	return ret;
}

static int rk818_shutdown(struct rk808 *rk808)
{
	int ret = 0;

	/* close rtc int when power off */
	regmap_update_bits(rk808->regmap,
			   RK817_INT_STS_MSK_REG0,
			   (0x3 << 5), (0x3 << 5));
	regmap_update_bits(rk808->regmap,
			   RK817_RTC_INT_REG,
			   (0x3 << 2), (0x0 << 2));

	if (system_state != SYSTEM_POWER_OFF)
		return ret;

	ret = regmap_update_bits(rk808->regmap, RK818_DEVCTRL_REG,
				 DEV_OFF, DEV_OFF);

	if (ret)
		dev_err(&rk808->i2c->dev, "Failed to shutdown device!\n");
	while (1)
		;
}

static int rk8xx_generic_shutdown(struct rk808 *rk808)
{
	dev_warn(&rk808->i2c->dev,
		 "Using generic shutdown for variant 0x%lx\n",
		 rk808->variant);
	return 0;
}

/* Shutdown function resolver */
static int (*rk808_get_shutdown_func(long variant))(struct rk808 *)
{
	switch (variant) {
	case RK801_ID:
		return rk801_shutdown;
	case RK805_ID:
		return rk805_shutdown;
	case RK808_ID:
		return rk808_shutdown;
	case RK809_ID:
	case RK817_ID:
		return rk817_shutdown;
	case RK816_ID:
		return rk816_shutdown;
	case RK818_ID:
		return rk818_shutdown;
	default:
		return rk8xx_generic_shutdown;
	}
}

static int rk8xx_apply_reg_reset_config(struct rk808 *rk808)
{
	int pmic_id, value;
	int ret = 0;

	if (!rk808->need_reg_reset) {
		dev_info(&rk808->i2c->dev, "No register reset required for PMIC variant 0x%lx\n",
			 rk808->variant);
		return 0;
	}
	/* Configure PMIC based on variant */
	switch (rk808->variant) {
	case RK805_ID:
		ret = regmap_read(rk808->regmap, RK808_ID_LSB, &pmic_id);
		if (ret) {
			dev_err(&rk808->i2c->dev, "reboot: failed to read RK808_ID_LSB\n");
			return false;
		}

		if ((pmic_id & RK805B_CHIP_VER_MSK) >= RK805B_CHIP_VER_NUM) {
			ret = regmap_update_bits(rk808->regmap,
						 RK805B_SYS_CFG2,
						 RK805B_RST_FUNC_MSK,
						 RK805B_RST_FUNC_REG);
			if (ret)
				dev_err(&rk808->i2c->dev, "reboot: failed to set RK805B_SYS_CFG2\n");
			else
				dev_info(&rk808->i2c->dev, "reboot: RK805B register reset mode configured\n");
		}
		break;
	case RK809_ID:
	case RK817_ID:
		/*
		 * Poll the RK817_SYS_STS register, waiting for the power key to be
		 * released (RK817_PWRON_STS bit set). The polling interval is 10 us,
		 * and the total timeout is 12 seconds. If a timeout occurs, an error
		 * message is logged with the last read register value.
		 */
		dev_info(&rk808->i2c->dev, "Polling RK817_SYS_STS for power key release\n");
		ret = regmap_read_poll_timeout(rk808->regmap, RK817_SYS_STS, value,
					       (value & RK817_PWRON_STS),
					       10,
					       12 * 1000 * 1000);
		if (ret)
			dev_err(&rk808->i2c->dev,
				"Timeout waiting for power key release (RK817_SYS_STS=0x%x)\n",
				value);

		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_RST_FUNC_MSK,
					 RK817_RST_FUNC_REG);
		if (ret)
			dev_err(&rk808->i2c->dev, "reboot: failed to set RK817_RST_FUNC_REG\n");
		else
			dev_info(&rk808->i2c->dev, "reboot: RK817/RK809 register reset mode configured\n");
		break;
	default:
		/* Other variants don't support register-only reset */
		dev_dbg(&rk808->i2c->dev, "PMIC variant 0x%lx doesn't support register-only reset\n",
			rk808->variant);
		rk808->need_reg_reset = false;
		break;
	}

	return ret;
}

static void rk808_syscore_shutdown(void)
{
	struct rk808_pmic_entry *entry, *tmp;
	int processed_count = 0;
	int skipped_count = 0;
	int ret;

	if (system_state != SYSTEM_POWER_OFF && system_state != SYSTEM_RESTART) {
		pr_info("RK808: Not a shutdown/reboot state, skipping\n");
		return;
	}

	mutex_lock(&rk808_pmic_mutex);

	list_for_each_entry_safe(entry, tmp, &rk808_pmic_list, list) {
		if (!entry->rk808) {
			pr_warn("RK808: Invalid PMIC entry\n");
			continue;
		}

		if (system_state == SYSTEM_RESTART) {
			ret = rk8xx_apply_reg_reset_config(entry->rk808);
			if (ret)
				pr_err("RK808: Failed to apply register reset config for variant 0x%lx: %d\n",
				       entry->rk808->variant, ret);
		}

		if (entry->priority == 0) {
			pr_info("RK808: Skipping PMIC variant 0x%lx (priority=0)\n",
				entry->rk808->variant);
			skipped_count++;
			continue;
		}

		if (entry->rk808->shutdown) {
			pr_warn("RK808: No shutdown function for PMIC variant 0x%lx\n",
				entry->rk808->variant);
			continue;
		}

		pr_info("RK808: Shutting down PMIC variant 0x%lx (priority: %d)\n",
			entry->rk808->variant, entry->priority);

		/* Note: Must ensure the PMIC's shutdown() function does not attempt to acquire
		 * rk808_pmic_mutex, otherwise a deadlock will occur. Typically it doesn't.
		 */
		mutex_unlock(&rk808_pmic_mutex);
		ret = entry->rk808->shutdown(entry->rk808);
		mutex_lock(&rk808_pmic_mutex);

		if (ret)
			pr_err("RK808: PMIC shutdown failed: %d\n", ret);
		else
			processed_count++;
	}

	mutex_unlock(&rk808_pmic_mutex);

	pr_info("RK808: Syscore shutdown completed. Processed: %d, Skipped: %d\n",
		processed_count, skipped_count);
}

/* Syscore operations structure */
static struct syscore_ops rk808_syscore_ops = {
	.shutdown = rk808_syscore_shutdown,
};

static void rk808_pmic_list_insert_sorted(struct rk808_pmic_entry *new_entry)
{
	struct rk808_pmic_entry *entry;
	struct list_head *pos;
	bool inserted = false;

	mutex_lock(&rk808_pmic_mutex);

	/* Traverse the global list to find the correct insertion point */
	list_for_each(pos, &rk808_pmic_list) {
		entry = list_entry(pos, struct rk808_pmic_entry, list);

		/* Rule: Entries with priority=0 (skip shutdown) are always
		 * placed at the end of the list
		 */
		if (new_entry->priority == 0) {
			if (entry->priority > 0) {
				list_add_tail(&new_entry->list, pos);
				inserted = true;
				break;
			}
		} else {
			/* New entry needs to execute shutdown */
			if (entry->priority == 0) {
				list_add(&new_entry->list, pos->prev);
				inserted = true;
				break;
			} else if (new_entry->priority < entry->priority) {
				/* Both are execution entries, insert in ascending priority order */
				list_add_tail(&new_entry->list, pos);
				inserted = true;
				break;
			}
		/* Priority >= current entry, continue searching forward */
		}
	}

	/* List is empty or new entry should be inserted at the end of the list */
	if (!inserted)
		list_add_tail(&new_entry->list, &rk808_pmic_list);

	atomic_inc(&rk808_pmic_count);

	/* If this is the first PMIC, register syscore operations */
	if (atomic_read(&rk808_pmic_count) == 1) {
		register_syscore_ops(&rk808_syscore_ops);
		new_entry->rk808->syscore_registered = true;
		pr_info("RK808: Registered syscore operations for %d PMIC(s)\n",
			atomic_read(&rk808_pmic_count));
	}

	mutex_unlock(&rk808_pmic_mutex);
}

/*
 * RK8xx PMICs would do real power off in syscore shutdown, if "pm_power_off"
 * is not assigned(e.g. PSCI is not enabled), we have to provide a dummy
 * callback for it, otherwise there comes a halt in Reboot system call:
 *
 * if ((cmd == LINUX_REBOOT_CMD_POWER_OFF) && !pm_power_off)
 *		cmd = LINUX_REBOOT_CMD_HALT;
 */
static void rk808_pm_power_off_dummy(void)
{
	pr_info("Dummy power off for RK8xx PMICs, should never reach here!\n");

	while (1)
		;
}

#ifdef CONFIG_MFD_RK808_SYSFS
static ssize_t rk8xx_dbg_store_common(struct rk808 *rk808,
				      const char *buf,
				      size_t count)
{
	int ret;
	char cmd;
	u32 input[2], addr, data;

	ret = sscanf(buf, "%c ", &cmd);
	if (ret != 1) {
		pr_err("Unknown command\n");
		goto out;
	}
	switch (cmd) {
	case 'w':
		ret = sscanf(buf, "%c %x %x ", &cmd, &input[0], &input[1]);
		if (ret != 3) {
			pr_err("error! cmd format: echo w [addr] [value]\n");
			goto out;
		};
		addr = input[0] & 0xff;
		data = input[1] & 0xff;
		pr_info("cmd : %c %x %x\n\n", cmd, input[0], input[1]);
		regmap_write(rk808->regmap, addr, data);
		regmap_read(rk808->regmap, addr, &data);
		pr_info("new: %x %x\n", addr, data);
		break;
	case 'r':
		ret = sscanf(buf, "%c %x ", &cmd, &input[0]);
		if (ret != 2) {
			pr_err("error! cmd format: echo r [addr]\n");
			goto out;
		};
		pr_info("cmd : %c %x\n\n", cmd, input[0]);
		addr = input[0] & 0xff;
		regmap_read(rk808->regmap, addr, &data);
		pr_info("%x %x\n", input[0], data);
		break;
	default:
		pr_err("Unknown command\n");
		break;
	}

out:
	return count;
}

static ssize_t rk8xx_dbg_store_per_device(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  const char *buf, size_t count)
{
	/* 1. Get the rk808 struct from the device_attribute using container_of */
	struct rk808 *rk808 = container_of(attr, struct rk808, dbg_attr);

	if (!rk808 || !rk808->regmap || !rk808->i2c) {
		pr_err("rk8xx_dbg_store: Invalid rk808 context retrieved!\n");
		if (rk808)
			pr_err("rk808->regmap=%p, rk808->i2c=%p\n", rk808->regmap, rk808->i2c);

		return -EINVAL;
	}
	/* 2. rk808 now points to the device instance for this sysfs file */
	/* 3. Call the common implementation */
	return rk8xx_dbg_store_common(rk808, buf, count);
}
#endif

static void rk805_of_property_prepare(struct rk808 *rk808, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret, func;

	ret = of_property_read_u32(np, "sleep-pin-polarity", &func);
	if (!ret) {
		if (func == 0)
			ret = regmap_update_bits(rk808->regmap, RK805_GPIO_IO_POL_REG,
						 RK805_SLP_POL_MASK,
						 func << RK805_SLP_POL_SHIFT);
		else
			ret = regmap_update_bits(rk808->regmap, RK805_GPIO_IO_POL_REG,
						 RK805_SLP_POL_MASK,
						 1 << RK805_SLP_POL_SHIFT);
		if (ret)
			dev_err(dev, "failed to update RK805_GPIO_IO_POL_REG!\n");
	} else {
		dev_info(dev, "failed to get sleep-pin-polarity\n");
	}
}

/*
 * Helper function to check if a reboot command matches any in the given list
 * and set a flag for later PMIC register-only reset configuration.
 */
static bool rk8xx_check_reg_reset_cmd(struct device *dev,
				      struct rk808 *rk808,
				      const char *cmd,
				      const char * const cmd_list[],
				      int cmd_count)
{
	bool handled = false;
	int i;

	rk808->need_reg_reset = false;

	for (i = 0; i < cmd_count; i++) {
		if (!strcmp(cmd, cmd_list[i])) {
			dev_info(dev,
				 "Reboot command '%s' matched, will configure register-only reset later\n",
				 cmd);
			rk808->need_reg_reset = true;
			handled = true;
			break;
		}
	}

	if (!handled)
		dev_info(dev, "Reboot command '%s' not matched for register reset\n", cmd);

	return handled;
}

/*
 * Helper function to check reboot command against both extended and built-in
 * command lists Returns true if command is matched and register-only reset is
 * configured
 */
static bool rk8xx_check_all_commands_and_set_reg_reset(struct device *dev,
						       struct rk808 *rk808,
						       const char *cmd)
{
	struct cmd_list {
		const char * const *list;
		int count;
	} cmd_lists[2];
	int num_lists = 0;
	bool handled = false;
	int i;

	/* Build array of command lists to check */
	if (rk808->ext_reg_only_cmds) {
		cmd_lists[num_lists].list = rk808->ext_reg_only_cmds;
		cmd_lists[num_lists].count = rk808->num_ext_reg_only_cmds;
		num_lists++;
	}

	cmd_lists[num_lists].list = pmic_rst_reg_only_cmd;
	cmd_lists[num_lists].count = ARRAY_SIZE(pmic_rst_reg_only_cmd);
	num_lists++;

	/* Check all command lists in order */
	for (i = 0; i < num_lists; i++) {
		handled = rk8xx_check_reg_reset_cmd(dev, rk808, cmd,
							cmd_lists[i].list,
							cmd_lists[i].count);
		if (handled)
			break;
	}

	return handled;
}

/*
 * Common reboot notifier handler function
 * When system restart, there are two rst actions of PMIC sleep if
 * board hardware support:
 *
 *	0b'00: reset the PMIC itself completely.
 *	0b'01: reset the 'RST' related register only.
 *
 * To use mode 0b‘01 for specific commands (e.g., reboot loader), we need to
 * program the PMIC accordingly before shutdown. This function:
 * 1. Checks the reboot command against a list to decide if register-only reset is needed.
 * 2. For RK817/RK809, restores the POWER_EN registers from saved values.
 * 3. Sets a flag (`need_reg_reset`) if register-only reset is required.
 * The actual register programming is done later in `rk8xx_apply_reg_reset_config`.
 */
static int rk808_reboot_notifier_handler(struct notifier_block *nb,
					 unsigned long action, void *cmd)
{
	int value, power_en_active0, power_en_active1;
	struct rk808_reboot_data_t *data;
	bool handled = false;
	struct device *dev;

	data = container_of(nb, struct rk808_reboot_data_t, reboot_notifier);
	dev = &data->rk808->i2c->dev;

	dev_info(dev, "PMIC reboot notifier called for variant 0x%lx, action: %lu, cmd: %s\n",
		 data->rk808->variant, action, cmd ? (char *)cmd : "NULL");

	/* Execute specific reboot handling based on PMIC variant */
	switch (data->rk808->variant) {
	case RK805_ID:
		if (action != SYS_RESTART || !cmd)
			return NOTIFY_OK;
		break;
	case RK809_ID:
	case RK817_ID:
		/* RK817/RK809 specific reboot handling */
		regmap_read(data->rk808->regmap, RK817_POWER_EN_SAVE0,
			    &power_en_active0);
		if (power_en_active0 != 0) {
			regmap_read(data->rk808->regmap, RK817_POWER_EN_SAVE1,
				    &power_en_active1);
			value = power_en_active0 & 0x0f;
			regmap_write(data->rk808->regmap,
				     RK817_POWER_EN_REG(0),
				     value | 0xf0);
			value = (power_en_active0 & 0xf0) >> 4;
			regmap_write(data->rk808->regmap,
				     RK817_POWER_EN_REG(1),
				     value | 0xf0);
			value = power_en_active1 & 0x0f;
			regmap_write(data->rk808->regmap,
				     RK817_POWER_EN_REG(2),
				     value | 0xf0);
			value = (power_en_active1 & 0xf0) >> 4;
			regmap_write(data->rk808->regmap,
				     RK817_POWER_EN_REG(3),
				     value | 0xf0);
		} else {
			dev_info(dev, "reboot: not restoring POWER_EN\n");
		}

		if (action != SYS_RESTART || !cmd)
			return NOTIFY_OK;

		break;
	case RK801_ID:
	case RK808_ID:
	case RK816_ID:
	case RK818_ID:
		/* Other PMIC variants can add specific reboot handling logic here */
		dev_dbg(dev, "PMIC variant 0x%lx received reboot notifier (action: %lu, cmd: %s)\n",
			data->rk808->variant, action, cmd ? (char *)cmd : "NULL");
		handled = true;
		break;

	default:
		dev_warn(dev, "Unhandled PMIC variant 0x%lx in reboot notifier\n",
			 data->rk808->variant);
		break;
	}

	/* Check all command lists (extended from DT and built-in) */
	handled = rk8xx_check_all_commands_and_set_reg_reset(dev,
							     data->rk808,
							     cmd);
	if (handled) {
		dev_info(dev, "PMIC reboot command check completed, will apply in syscore_shutdown\n");
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

/* Register PMIC to reboot notifier list */
static int rk808_register_reboot_notifier(struct rk808 *rk808)
{
	struct rk808_reboot_data_t *reboot_data;
	int ret;

	reboot_data = devm_kzalloc(&rk808->i2c->dev,
				   sizeof(*reboot_data),
				   GFP_KERNEL);
	if (!reboot_data)
		return -ENOMEM;

	reboot_data->rk808 = rk808;
	reboot_data->reboot_notifier.notifier_call = rk808_reboot_notifier_handler;

	ret = devm_register_reboot_notifier(&rk808->i2c->dev, &reboot_data->reboot_notifier);
	if (ret) {
		dev_err(&rk808->i2c->dev, "failed to register reboot notifier: %d\n", ret);
		devm_kfree(&rk808->i2c->dev, reboot_data);
		return ret;
	}

	rk808->reboot_data = reboot_data;
	dev_info(&rk808->i2c->dev, "Registered reboot notifier for PMIC variant 0x%lx\n",
		 rk808->variant);

	return 0;
}

/**
 * rk805b_configure_shutdown_voltage_threshold - Configure the battery under-voltage
 *                                               shutdown threshold for RK805B.
 * @rk808: Pointer to the main RK808 PMIC data structure.
 *
 * This function programs the RK805B_THERMAL_REG register to set the voltage level
 * at which the PMIC will force a system shutdown when the battery voltage drops
 * below the configured threshold. The threshold is read from the device tree property
 * `rockchip,shutdown-voltage-threshold-mv` (in millivolts).
 *
 * The valid threshold range is 2700mV to 3400mV. Values are programmed in steps of
 * 100mV starting from 2700mV (register value 0).
 *
 * If `rk808->force_shutdown_enable` is false or the threshold is invalid,
 * the function does nothing.
 *
 * Return: 0 on success, or a negative error code from regmap_update_bits.
 */
static int rk805b_configure_shutdown_voltage_threshold(struct rk808 *rk808)
{
	int vb_uv_sel;
	int ret = 0;

	if (!rk808->force_shutdown_enable)
		return 0;

	if (rk808->shutdown_voltage_threshold <= 2700)
		vb_uv_sel = VB_UV_SEL_2700;
	else
		vb_uv_sel = (rk808->shutdown_voltage_threshold - 2700) / 100;

	ret = regmap_update_bits(rk808->regmap,
				 RK805_THERMAL_REG,
				 RK805B_VB_UV_SEL_MSK,
				 (vb_uv_sel << RK805B_VB_UV_SEL_SFT));
	if (ret) {
		dev_err(&rk808->i2c->dev,
			"Failed to set RK805B shutdown voltage threshold (0x%x)\n",
			RK805_THERMAL_REG);
	}
	return ret;
}

/**
 * rk817_configure_shutdown_voltage_threshold - Configure the battery under-voltage
 *                                               shutdown threshold for RK809/RK817.
 * @rk808: Pointer to the main RK808 PMIC data structure.
 *
 * This function programs the RK817_SYS_CFG(0) register to set the voltage level
 * at which the PMIC will force a system shutdown when the battery voltage drops
 * below the configured threshold. The threshold is read from the device tree property
 * `rockchip,shutdown-voltage-threshold-mv` (in millivolts).
 *
 * The valid threshold range is 2700mV to 3400mV. Values are programmed in steps of
 * 100mV starting from 2700mV (register value 0).
 *
 * If `rk808->force_shutdown_enable` is false or the threshold is invalid,
 * the function does nothing.
 *
 * Return: 0 on success, or a negative error code from regmap_update_bits.
 */
static int rk817_configure_shutdown_voltage_threshold(struct rk808 *rk808)
{
	int vb_uv_sel;
	int ret = 0;

	if (!rk808->force_shutdown_enable)
		return 0;

	if (rk808->shutdown_voltage_threshold <= 2700)
		vb_uv_sel = VB_UV_SEL_2700;
	else
		vb_uv_sel = (rk808->shutdown_voltage_threshold - 2700) / 100;

	ret = regmap_update_bits(rk808->regmap,
				 RK817_SYS_CFG(0),
				 RK817_VB_UV_SEL_MSK,
				 (vb_uv_sel << RK817_VB_UV_SEL_SFT));
	if (ret) {
		dev_err(&rk808->i2c->dev,
			"Failed to set RK817 shutdown voltage threshold (0x%x)\n",
			RK817_SYS_CFG(0));
	}
	return ret;
}

/*
 * rk808_setup_system_power_control - Configure a PMIC as a system-wide power controller.
 * @rk808: Pointer to the main RK808 PMIC data structure.
 * @np: Device tree node pointer for the PMIC device.
 * @entry: Pointer to the PMIC's global registry entry.
 *
 * This function completes the setup for a PMIC that is designated as the
 * system power controller (via the 'rockchip,system-power-controller' DT property).
 * Its responsibilities include:
 * 1. Registering the appropriate shutdown/reboot prepare handler based on the PMIC variant.
 * 2. Inserting the PMIC into a globally managed, sorted list for coordinated shutdown.
 *
 * Return: 0 on success, or a negative error code from registering the sys-off handler.
 */
static int rk808_setup_system_power_control(struct rk808 *rk808,
					    struct device_node *np,
					    struct rk808_pmic_entry *entry)
{
	struct device *dev = &rk808->i2c->dev;
	int ret = 0;

	/* 1. Register the appropriate shutdown prepare handler based on PMIC variant */
	switch (rk808->variant) {
	case RK801_ID:
		ret = devm_register_sys_off_handler(dev,
						    SYS_OFF_MODE_POWER_OFF_PREPARE,
						    SYS_OFF_PRIO_DEFAULT,
						    rk801_device_shutdown_prepare,
						    rk808);
		break;
	case RK805_ID:
		ret = rk805b_configure_shutdown_voltage_threshold(rk808);
		if (ret)
			return ret;

		ret = devm_register_sys_off_handler(dev,
						    SYS_OFF_MODE_POWER_OFF_PREPARE,
						    SYS_OFF_PRIO_DEFAULT,
						    rk805_device_shutdown_prepare,
						    rk808);
		break;
	case RK809_ID:
	case RK817_ID:
		ret = rk817_configure_shutdown_voltage_threshold(rk808);
		if (ret)
			return ret;
		ret = devm_register_sys_off_handler(dev,
						    SYS_OFF_MODE_POWER_OFF_PREPARE,
						    SYS_OFF_PRIO_DEFAULT,
						    rk817_shutdown_prepare,
						    rk808);
		break;
	default:
		/* Other variants may not require or have a specific sys-off handler yet */
		dev_dbg(dev, "No specific sys-off handler for variant 0x%lx\n",
			rk808->variant);
		break;
	}

	if (ret) {
		dev_err(dev, "failed to register sys-off handler: %d\n", ret);
		/* On registration failure, do NOT add it to the global management list */
		return ret;
	}

	/* 2. Insert the PMIC entry into the global, sorted management list */
	rk808_pmic_list_insert_sorted(entry);
	dev_info(dev, "Registered as system power controller (variant: 0x%lx)\n",
		 rk808->variant);

	return 0;
}
static void rk817_of_property_prepare(struct rk808 *rk808, struct device *dev)
{
	u32 inner;
	int ret, func, msk, val;
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32_index(np, "fb-inner-reg-idxs", 0, &inner);
	if (!ret && inner == RK817_ID_DCDC3)
		regmap_update_bits(rk808->regmap, RK817_POWER_CONFIG,
				   RK817_BUCK3_FB_RES_MSK,
				   RK817_BUCK3_FB_RES_INTER);
	else
		regmap_update_bits(rk808->regmap, RK817_POWER_CONFIG,
				   RK817_BUCK3_FB_RES_MSK,
				   RK817_BUCK3_FB_RES_EXT);
	dev_info(dev, "support dcdc3 fb mode:%d, %d\n", ret, inner);

	ret = of_property_read_u32(np, "pmic-reset-func", &func);

	msk = RK817_SLPPIN_FUNC_MSK | RK817_RST_FUNC_MSK;
	val = SLPPIN_NULL_FUN;

	if (!ret && func < RK817_RST_FUNC_CNT) {
		val |= RK817_RST_FUNC_MSK &
		       (func << RK817_RST_FUNC_SFT);
	} else {
		val |= RK817_RST_FUNC_REG;
	}

	regmap_update_bits(rk808->regmap, RK817_SYS_CFG(3), msk, val);
	dev_info(dev, "support pmic reset mode:%d,%d\n", ret, func);
}

/*
 * rk8xx_pinctrl_parse_dt - Parse pinctrl states from Device Tree for RK8xx PMIC
 * @rk808: Pointer to the main RK808 PMIC data structure
 *
 * This function attempts to obtain and configure the pinctrl states for the PMIC.
 * It is designed to be resilient: failure to obtain the pinctrl handle or any
 * specific state is treated as a non-fatal condition (the feature is simply disabled),
 * with appropriate debug messages logged.
 *
 * Return: 0 on success (or if pinctrl is not available/fully configured),
 *         or a negative error code on critical resource allocation failure.
 */
static int rk8xx_pinctrl_parse_dt(struct rk808 *rk808)
{
	struct device *dev = &rk808->i2c->dev;
	struct pinctrl_state *default_st;
	int ret;

	/* 1. Allocate the pin info structure */
	rk808->pins = devm_kzalloc(dev, sizeof(struct rk808_pin_info), GFP_KERNEL);
	if (!rk808->pins)
		return -ENOMEM;

	/* 2. Obtain the pinctrl handle */
	rk808->pins->p = devm_pinctrl_get(dev);
	if (IS_ERR(rk808->pins->p)) {
		/* pinctrl is an optional feature for this driver.
		 * If not available, free the allocated structure and continue.
		 */
		dev_info(dev, "no pinctrl handle available\n");
		devm_kfree(dev, rk808->pins);
		rk808->pins = NULL;
		return 0;
	}

	/* 3. Look up and activate the default state (if it exists) */
	default_st = pinctrl_lookup_state(rk808->pins->p, PINCTRL_STATE_DEFAULT);
	if (!IS_ERR(default_st)) {
		ret = pinctrl_select_state(rk808->pins->p, default_st);
		if (ret)
			dev_info(dev, "failed to activate default pinctrl state\n");
	} else {
		dev_info(dev, "no default pinctrl state\n");
	}

	/* 4. Look up optional, PMIC-specific states */
	rk808->pins->power_off = pinctrl_lookup_state(rk808->pins->p, "pmic-power-off");
	if (IS_ERR(rk808->pins->power_off)) {
		rk808->pins->power_off = NULL;
		dev_info(dev, "no power-off pinctrl state\n");
	}

	rk808->pins->sleep = pinctrl_lookup_state(rk808->pins->p, "pmic-sleep");
	if (IS_ERR(rk808->pins->sleep)) {
		rk808->pins->sleep = NULL;
		dev_info(dev, "no sleep pinctrl state\n");
	}

	rk808->pins->reset = pinctrl_lookup_state(rk808->pins->p, "pmic-reset");
	if (IS_ERR(rk808->pins->reset)) {
		rk808->pins->reset = NULL;
		dev_info(dev, "no reset pinctrl state\n");
	}

	return 0;
}

static int rk8xx_parse_dt(struct rk808 *rk808)
{
	struct device *dev = &rk808->i2c->dev;
	int i, ret;

	ret = device_property_read_string_array(dev, "rockchip,pmic-reg-only-cmds",
						NULL, 0);
	if (ret > 0) {
		rk808->num_ext_reg_only_cmds = ret;
		rk808->ext_reg_only_cmds = devm_kzalloc(dev,
							ret * sizeof(char *),
							GFP_KERNEL);
		if (!rk808->ext_reg_only_cmds)
			return -ENOMEM;

		/* Read strings into array */
		device_property_read_string_array(dev, "rockchip,pmic-reg-only-cmds",
						  rk808->ext_reg_only_cmds, ret);

		dev_info(dev,
			 "Loaded %d extended register-only reset commands from DT\n",
			 ret);

		/* Debug: print all extended commands */
		for (i = 0; i < ret; i++) {
			dev_info(dev, "  Extended command[%d]: %s\n",
				 i, rk808->ext_reg_only_cmds[i]);
		}
	} else if (ret == -EINVAL) {
		/* Property doesn't exist, this is normal */
		rk808->num_ext_reg_only_cmds = 0;
		rk808->ext_reg_only_cmds = NULL;
	} else if (ret < 0) {
		/* Read error */
		dev_err(dev,
			"Failed to read rockchip,pmic-reg-only-cmds property: %d\n", ret);
		return ret;
	}

	rk808->force_shutdown_enable = true;
	ret = device_property_read_u32(dev,
				       "rockchip,shutdown-voltage-threshold-mv",
				       &rk808->shutdown_voltage_threshold);
	if (ret < 0) {
		rk808->force_shutdown_enable = false;
		dev_info(dev, "rockchip,shutdown-voltage-threshold-mv missing!\n");
	} else {
		if ((rk808->shutdown_voltage_threshold > 3400) ||
		     (rk808->shutdown_voltage_threshold < 2700)) {
			dev_err(dev, "rockchip,shutdown-voltage-threshold-mv out [2700 3400]!\n");
			rk808->shutdown_voltage_threshold = 2700;
		}
	}

	ret = rk8xx_pinctrl_parse_dt(rk808);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id rk808_of_match[] = {
	{ .compatible = "rockchip,rk801" },
	{ .compatible = "rockchip,rk805" },
	{ .compatible = "rockchip,rk808" },
	{ .compatible = "rockchip,rk809" },
	{ .compatible = "rockchip,rk816" },
	{ .compatible = "rockchip,rk817" },
	{ .compatible = "rockchip,rk818" },
	{ },
};
MODULE_DEVICE_TABLE(of, rk808_of_match);

static int rk808_probe(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct rk808 *rk808;
	const struct rk808_reg_data *pre_init_reg;
	const struct regmap_irq_chip *battery_irq_chip = NULL;
	const struct mfd_cell *cells;
	u8 on_source = 0, off_source = 0;
	unsigned int on, off;
	u32 pmic_id_mask = RK8XX_ID_MSK;
	int nr_pre_init_regs;
	int nr_cells;
	int pmic_id, voutsel_flag;
	int msb, lsb;
	unsigned char pmic_id_msb, pmic_id_lsb;
	int ret;
	int i;
	void (*of_property_prepare_fn)(struct rk808 *rk808,
				       struct device *dev) = NULL;
	struct rk808_pmic_entry *entry;
	int priority = 100; /* Default priority */
	bool is_primary = false;

	rk808 = devm_kzalloc(&client->dev, sizeof(*rk808), GFP_KERNEL);
	if (!rk808)
		return -ENOMEM;

	/* Allocate PMIC registry entry */
	entry = devm_kzalloc(&client->dev, sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	if (of_device_is_compatible(np, "rockchip,rk817") ||
	    of_device_is_compatible(np, "rockchip,rk809")) {
		pmic_id_msb = RK817_ID_MSB;
		pmic_id_lsb = RK817_ID_LSB;
	} else if (of_device_is_compatible(np, "rockchip,rk801")) {
		pmic_id_msb = RK801_ID_MSB;
		pmic_id_lsb = RK801_ID_LSB;
		pmic_id_mask = RK801_ID_MSK;
	} else {
		pmic_id_msb = RK808_ID_MSB;
		pmic_id_lsb = RK808_ID_LSB;
	}

	/* Read chip variant */
	msb = i2c_smbus_read_byte_data(client, pmic_id_msb);
	if (msb < 0) {
		dev_err(&client->dev, "failed to read the chip id at 0x%x\n",
			RK808_ID_MSB);
		return msb;
	}

	lsb = i2c_smbus_read_byte_data(client, pmic_id_lsb);
	if (lsb < 0) {
		dev_err(&client->dev, "failed to read the chip id at 0x%x\n",
			RK808_ID_LSB);
		return lsb;
	}

	pmic_id = (msb << 8) | lsb;
	rk808->variant = pmic_id & RK8XX_ID_MSK;
	dev_info(&client->dev, "chip id: 0x%x\n", pmic_id & pmic_id_mask);

	switch (rk808->variant) {
	case RK801_ID:
		rk808->pwrctrl.req_pwrctrl_dvs = (lsb & 0x0f) < 3;
		rk808->regmap_cfg = &rk801_regmap_config;
		rk808->regmap_irq_chip = &rk801_irq_chip;
		pre_init_reg = rk801_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk801_pre_init_reg);
		cells = rk801s;
		nr_cells = ARRAY_SIZE(rk801s);
		on_source = RK801_ON_SOURCE_REG;
		off_source = RK801_OFF_SOURCE_REG;
		break;
	case RK805_ID:
		rk808->regmap_cfg = &rk805_regmap_config;
		rk808->regmap_irq_chip = &rk805_irq_chip;
		pre_init_reg = rk805_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk805_pre_init_reg);
		cells = rk805s;
		nr_cells = ARRAY_SIZE(rk805s);
		on_source = RK805_ON_SOURCE_REG;
		off_source = RK805_OFF_SOURCE_REG;
		rk808->suspend_reg = rk805_suspend_reg;
		rk808->suspend_reg_num = ARRAY_SIZE(rk805_suspend_reg);
		rk808->resume_reg = rk805_resume_reg;
		rk808->resume_reg_num = ARRAY_SIZE(rk805_resume_reg);
		of_property_prepare_fn = rk805_of_property_prepare;
		if ((pmic_id & RK805B_CHIP_VER_MSK) >= RK805B_CHIP_VER_NUM) {
			voutsel_flag = i2c_smbus_read_byte_data(client, RK805B_VSELTABLE_REG);
			if (voutsel_flag < 0) {
				dev_err(&client->dev, "failed to read the voutsel_flag at 0x%x\n",
					RK805B_VSELTABLE_REG);
				return voutsel_flag;
			}
			rk808->vsel_table = voutsel_flag & RK805B_VSELTABLE_4OR8;
		}
		break;
	case RK808_ID:
		rk808->regmap_cfg = &rk808_regmap_config;
		rk808->regmap_irq_chip = &rk808_irq_chip;
		pre_init_reg = rk808_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk808_pre_init_reg);
		cells = rk808s;
		nr_cells = ARRAY_SIZE(rk808s);
		break;
	case RK816_ID:
		rk808->regmap_cfg = &rk816_regmap_config;
		rk808->regmap_irq_chip = &rk816_irq_chip;
		battery_irq_chip = &rk816_battery_irq_chip;
		pre_init_reg = rk816_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk816_pre_init_reg);
		cells = rk816s;
		nr_cells = ARRAY_SIZE(rk816s);
		on_source = RK816_ON_SOURCE_REG;
		off_source = RK816_OFF_SOURCE_REG;
		rk808->suspend_reg = rk816_suspend_reg;
		rk808->suspend_reg_num = ARRAY_SIZE(rk816_suspend_reg);
		rk808->resume_reg = rk816_resume_reg;
		rk808->resume_reg_num = ARRAY_SIZE(rk816_resume_reg);
		break;
	case RK818_ID:
		rk808->regmap_cfg = &rk818_regmap_config;
		rk808->regmap_irq_chip = &rk818_irq_chip;
		pre_init_reg = rk818_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk818_pre_init_reg);
		cells = rk818s;
		nr_cells = ARRAY_SIZE(rk818s);
		on_source = RK818_ON_SOURCE_REG;
		off_source = RK818_OFF_SOURCE_REG;
		rk808->suspend_reg = rk818_suspend_reg;
		rk808->suspend_reg_num = ARRAY_SIZE(rk818_suspend_reg);
		rk808->resume_reg = rk818_resume_reg;
		rk808->resume_reg_num = ARRAY_SIZE(rk818_resume_reg);
		break;
	case RK809_ID:
	case RK817_ID:
		rk808->regmap_cfg = &rk817_regmap_config;
		rk808->regmap_irq_chip = &rk817_irq_chip;
		pre_init_reg = rk817_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk817_pre_init_reg);
		cells = rk817s;
		nr_cells = ARRAY_SIZE(rk817s);
		on_source = RK817_ON_SOURCE_REG;
		off_source = RK817_OFF_SOURCE_REG;
		priority = 0;
		of_property_prepare_fn = rk817_of_property_prepare;
		break;
	default:
		dev_err(&client->dev, "Unsupported RK8XX ID %lu\n",
			rk808->variant);
		return -EINVAL;
	}

	/* Get configuration from device tree */
	device_property_read_u32(&client->dev, "rockchip,shutdown-priority", &priority);
	is_primary = device_property_read_bool(&client->dev, "rockchip,primary-pmic");

	/* Initialize registry entry */
	entry->rk808 = rk808;
	entry->priority = priority;
	entry->is_primary = is_primary;

	/* Set shutdown and reboot functions based on variant */
	rk808->shutdown = rk808_get_shutdown_func(rk808->variant);
	rk808->pmic_entry = entry;
	rk808->priority = priority;
	rk808->is_primary = is_primary;

	rk808->i2c = client;
	i2c_set_clientdata(client, rk808);

	rk808->regmap = devm_regmap_init_i2c(client, rk808->regmap_cfg);
	if (IS_ERR(rk808->regmap)) {
		dev_err(&client->dev, "regmap initialization failed\n");
		return PTR_ERR(rk808->regmap);
	}

	if (on_source && off_source) {
		ret = regmap_read(rk808->regmap, on_source, &on);
		if (ret) {
			dev_err(&client->dev, "read 0x%x failed\n", on_source);
			return ret;
		}

		ret = regmap_read(rk808->regmap, off_source, &off);
		if (ret) {
			dev_err(&client->dev, "read 0x%x failed\n", off_source);
			return ret;
		}

		dev_info(&client->dev, "source: on=0x%02x, off=0x%02x\n",
			 on, off);
	}

	if (!client->irq) {
		dev_err(&client->dev, "No interrupt support, no core IRQ\n");
		return -EINVAL;
	}

	if (of_property_prepare_fn)
		of_property_prepare_fn(rk808, &client->dev);
	ret = rk8xx_parse_dt(rk808);
	if (ret)
		return ret;

	for (i = 0; i < nr_pre_init_regs; i++) {
		ret = regmap_update_bits(rk808->regmap,
					 pre_init_reg[i].addr,
					 pre_init_reg[i].mask,
					 pre_init_reg[i].value);
		if (ret) {
			dev_err(&client->dev,
				"0x%x write err\n",
				pre_init_reg[i].addr);
			return ret;
		}
	}

	ret = regmap_add_irq_chip(rk808->regmap, client->irq,
				  IRQF_ONESHOT | IRQF_SHARED, -1,
				  rk808->regmap_irq_chip, &rk808->irq_data);
	if (ret) {
		dev_err(&client->dev, "Failed to add irq_chip %d\n", ret);
		return ret;
	}

	if (battery_irq_chip) {
		ret = regmap_add_irq_chip(rk808->regmap, client->irq,
					  IRQF_ONESHOT | IRQF_SHARED, -1,
					  battery_irq_chip,
					  &rk808->battery_irq_data);
		if (ret) {
			dev_err(&client->dev,
				"Failed to add batterry irq_chip %d\n", ret);
			regmap_del_irq_chip(client->irq, rk808->irq_data);
			return ret;
		}
	}

	ret = devm_mfd_add_devices(&client->dev, PLATFORM_DEVID_NONE,
				   cells, nr_cells, NULL, 0,
				   regmap_irq_get_domain(rk808->irq_data));
	if (ret) {
		dev_err(&client->dev, "failed to add MFD devices %d\n", ret);
		goto err_irq;
	}

	if (of_property_read_bool(np, "rockchip,system-power-controller")) {
		ret = rk808_setup_system_power_control(rk808, np, entry);
		if (ret)
			return ret;
	}

	ret = rk808_register_reboot_notifier(rk808);
	if (ret)
		dev_err(&client->dev, "Failed to register reboot notifier: %d\n", ret);

#ifdef CONFIG_MFD_RK808_SYSFS
	snprintf(rk808->sysfs_dir_name, sizeof(rk808->sysfs_dir_name),
		 "%s_%d_%04x", np->name, client->adapter->nr, client->addr);

	rk808->sysfs_kobj = kobject_create_and_add(rk808->sysfs_dir_name, NULL);
	if (!rk808->sysfs_kobj) {
		dev_warn(&client->dev, "failed to create sysfs kobject at /sys/%s/\n",
			 rk808->sysfs_dir_name);
	} else {
		/* Initialize device-specific attribute */
		sysfs_attr_init(&rk808->dbg_attr.attr);
		rk808->dbg_attr.attr.name = "rk8xx_dbg";
		rk808->dbg_attr.attr.mode = 0200; /* Write-only */
		rk808->dbg_attr.store = rk8xx_dbg_store_per_device;

		ret = sysfs_create_file(rk808->sysfs_kobj, &rk808->dbg_attr.attr);
		if (ret) {
			dev_err(&client->dev, "failed to create debug sysfs file, ret=%d\n", ret);
			kobject_put(rk808->sysfs_kobj);
			rk808->sysfs_kobj = NULL;
		} else {
			dev_info(&client->dev, "debug sysfs node at /sys/%s/rk8xx_dbg\n",
			rk808->sysfs_dir_name);
		}
	}
#endif

	if (!pm_power_off)
		pm_power_off = rk808_pm_power_off_dummy;

	return 0;

err_irq:
	regmap_del_irq_chip(client->irq, rk808->irq_data);
	if (battery_irq_chip)
		regmap_del_irq_chip(client->irq, rk808->battery_irq_data);
	return ret;
}

static void rk808_remove(struct i2c_client *client)
{
	struct rk808 *rk808 = i2c_get_clientdata(client);

	regmap_del_irq_chip(client->irq, rk808->irq_data);

	if (rk808->pmic_entry) {
		mutex_lock(&rk808_pmic_mutex);

		/* Remove PMIC from registry */
		list_del(&rk808->pmic_entry->list);
		atomic_dec(&rk808_pmic_count);

		/* If registry is empty, unregister syscore operations */
		if (atomic_read(&rk808_pmic_count) == 0 && rk808->syscore_registered) {
			unregister_syscore_ops(&rk808_syscore_ops);
			pr_info("RK808: Unregistered syscore operations\n");
		}

		mutex_unlock(&rk808_pmic_mutex);

		rk808->pmic_entry = NULL;
	}

#ifdef CONFIG_MFD_RK808_SYSFS
	if (rk808->sysfs_kobj) {
		sysfs_remove_file(rk808->sysfs_kobj, &rk808->dbg_attr.attr);
		kobject_put(rk808->sysfs_kobj);
		rk808->sysfs_kobj = NULL;
	}
#endif
	/**
	 * pm_power_off may points to a function from another module.
	 * Check if the pointer is set by us and only then overwrite it.
	 */
	if (pm_power_off == rk808_pm_power_off_dummy)
		pm_power_off = NULL;
}

static int __maybe_unused rk8xx_suspend(struct device *dev)
{
	struct rk808 *rk808 = dev_get_drvdata(dev);
	int i, ret = 0;
	int value;

	for (i = 0; i < rk808->suspend_reg_num; i++) {
		ret = regmap_update_bits(rk808->regmap,
					 rk808->suspend_reg[i].addr,
					 rk808->suspend_reg[i].mask,
					 rk808->suspend_reg[i].value);
		if (ret) {
			dev_err(dev, "0x%x write err\n",
				rk808->suspend_reg[i].addr);
			return ret;
		}
	}

	switch (rk808->variant) {
	case RK801_ID:
		ret = regmap_update_bits(rk808->regmap, RK801_SYS_CFG2_REG,
					 RK801_SLEEP_POL_MSK,
					 rk801_act_pol(rk808->pwrctrl.act_low));
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(rk808->regmap, RK801_SLEEP_CFG_REG,
					 RK801_SLEEP_FUN_MSK, RK801_SLEEP_FUN);
		break;
	case RK805_ID:
		ret = regmap_update_bits(rk808->regmap,
					 RK805_GPIO_IO_POL_REG,
					 SLP_SD_MSK,
					 SLEEP_FUN);
		break;
	case RK809_ID:
	case RK817_ID:
		if (rk808->pins && rk808->pins->p && rk808->pins->sleep) {
			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPIN_FUNC_MSK,
						 SLPPIN_NULL_FUN);
			if (ret) {
				dev_err(dev, "suspend: config SLPPIN_NULL_FUN error!\n");
				return ret;
			}

			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPOL_MSK,
						 RK817_SLPPOL_H);
			if (ret) {
				dev_err(dev, "suspend: config RK817_SLPPOL_H error!\n");
				return ret;
			}

			/* pmic need the SCL clock to synchronize register */
			regmap_read(rk808->regmap, RK817_SYS_STS, &value);
			mdelay(2);
			ret = pinctrl_select_state(rk808->pins->p, rk808->pins->sleep);
			if (ret) {
				dev_err(dev, "failed to act slp pinctrl state\n");
				return ret;
			}

			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPIN_FUNC_MSK,
						 SLPPIN_SLP_FUN);
			if (ret) {
				dev_err(dev, "suspend: config SLPPIN_SLP_FUN error!\n");
				return ret;
			}
		}
		break;
	default:
		break;
	}

	return ret;
}

static int __maybe_unused rk8xx_resume(struct device *dev)
{
	struct rk808 *rk808 = dev_get_drvdata(dev);
	int i, ret = 0;
	int value;

	for (i = 0; i < rk808->resume_reg_num; i++) {
		ret = regmap_update_bits(rk808->regmap,
					 rk808->resume_reg[i].addr,
					 rk808->resume_reg[i].mask,
					 rk808->resume_reg[i].value);
		if (ret) {
			dev_err(dev, "0x%x write err\n",
				rk808->resume_reg[i].addr);
			return ret;
		}
	}

	switch (rk808->variant) {
	case RK809_ID:
	case RK817_ID:
		if (rk808->pins && rk808->pins->p && rk808->pins->reset) {
			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPIN_FUNC_MSK,
						 SLPPIN_NULL_FUN);
			if (ret) {
				dev_err(dev, "resume: config SLPPIN_NULL_FUN error!\n");
				return ret;
			}

			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPOL_MSK,
						 RK817_SLPPOL_L);
			if (ret) {
				dev_err(dev, "resume: config RK817_SLPPOL_L error!\n");
				return ret;
			}

			/* pmic need the SCL clock to synchronize register */
			regmap_read(rk808->regmap, RK817_SYS_STS, &value);
			mdelay(2);
			ret = pinctrl_select_state(rk808->pins->p, rk808->pins->reset);
			if (ret)
				dev_dbg(dev, "failed to act reset pinctrl state\n");

			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPIN_FUNC_MSK,
						 SLPPIN_RST_FUN);
			if (ret) {
				dev_err(dev, "resume: config SLPPIN_RST_FUN error!\n");
				return ret;
			}
		}
		break;
	default:
		break;
	}

	return ret;
}
static SIMPLE_DEV_PM_OPS(rk8xx_pm_ops, rk8xx_suspend, rk8xx_resume);

static struct i2c_driver rk808_i2c_driver = {
	.driver = {
		.name = "rk808",
		.of_match_table = rk808_of_match,
		.pm = &rk8xx_pm_ops,
	},
	.probe    = rk808_probe,
	.remove   = rk808_remove,
};

#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT
static int __init rk808_i2c_driver_init(void)
{
	return i2c_add_driver(&rk808_i2c_driver);
}
subsys_initcall(rk808_i2c_driver_init);

static void __exit rk808_i2c_driver_exit(void)
{
	i2c_del_driver(&rk808_i2c_driver);
}
module_exit(rk808_i2c_driver_exit);
#else
module_i2c_driver(rk808_i2c_driver);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris Zhong <zyw@rock-chips.com>");
MODULE_AUTHOR("Zhang Qing <zhangqing@rock-chips.com>");
MODULE_AUTHOR("Wadim Egorov <w.egorov@phytec.de>");
MODULE_DESCRIPTION("RK808/RK818 PMIC driver");
