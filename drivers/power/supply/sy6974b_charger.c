// SPDX-License-Identifier: GPL-2.0
/*
 * Chrager driver for sy6974b
 *
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 *
 * Author: Xu Shengfei <xsf@rock-chips.com>
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>

static int dbg_enable = 1;

module_param_named(dbg_level, dbg_enable, int, 0644);

#define CH_DBG(fmt, arg...) \
	do { \
		if (dbg_enable) { \
			pr_info("Charger->" fmt, ##arg); \
		} \
	} while (0)

#define MANUFACTURER	"SILERGY"
#define NAME				"sy6974b"
#define PN_ID				BIT(6)  // SY6974B 1000

/* define register */
#define REG_0		0x00
#define REG_1		0x01
#define REG_2		0x02
#define REG_3		0x03
#define REG_4		0x04
#define REG_5		0x05
#define REG_6		0x06
#define REG_7		0x07
#define REG_8		0x08
#define REG_9		0x09
#define REG_A		0x0a
#define REG_B		0x0b

/* charge control */
#define CHRG_EN_MASK			BIT(4)
#define CHRG_EN				BIT(4)
#define HIZ_EN_MASK			BIT(7)
#define TERM_EN_MASK			BIT(7)
#define VAC_OVP_MASK			GENMASK(7, 6)
#define VBUS_GOOD_MASK			BIT(7)
#define BOOSTV_MASK			GENMASK(5, 4)
#define BOOST_LIM_MIN_MASK		BIT(7)
#define OTG_EN_MASK			BIT(5)
#define OTG_EN				BIT(5)
/* charge config */
#define BATFET_DIS_MASK			BIT(5)
#define BATFET_DIS_DELAY_MASK		BIT(3)
#define BATFET_RST_EN_MASK		BIT(2)

/* Part ID */
#define PN_MASK				GENMASK(6, 3)
/* WDT TIMER SET */
#define WDT_TIMER_MASK			GENMASK(5, 4)
#define WDT_TIMER_DISABLE		0
#define WDT_TIMER_40S			BIT(4)
#define WDT_TIMER_80S			BIT(5)
#define WDT_TIMER_160S			(BIT(4) | BIT(5))
#define WDT_RST_MASK			BIT(6)
#define WDT_RST				BIT(6)
/* recharge voltage */
#define VRECHARGE_MASK			BIT(0)
#define VRECHRG_STEP			100  // mv
#define VRECHRG_OFFSET			100  // mv
#define VRECHRG_DEF			200  // mv
/* charge status */
#define VSYS_STAT_MASK			BIT(0)
#define THERM_STAT_MASK			BIT(1)
#define PG_STAT_MASK			BIT(2)
#define CHG_STAT_MASK			GENMASK(4, 3)
#define PRECHRG_STAT			BIT(3)
#define FAST_CHRG_STAT			BIT(4)
#define TERM_CHRG_STAT			(BIT(3) | BIT(4))
#define NOT_CHRGING_STAT		0
#define CHG_FAULT_MASK			GENMASK(5, 4)
/* charge type */
#define VBUS_STAT_MASK			GENMASK(7, 5)
#define USB_SDP				BIT(5)
#define USB_CDP				BIT(6)
#define USB_DCP				(BIT(5) | BIT(6))
#define UNKNOWN				(BIT(7) | BIT(5))
#define NON_STANDARD			(BIT(7) | BIT(6))
#define OTG_MODE			(BIT(7) | BIT(6) | BIT(5))
/* TEMP Status */
#define TEMP_STAT_MASK			GENMASK(2, 0)
#define TEMP_NORMAL			BIT(0)
#define TEMP_WARM			BIT(1)
#define TEMP_COOL			(BIT(0) | BIT(1))
#define TEMP_COLD			(BIT(0) | BIT(2))
#define TEMP_HOT			(BIT(1) | BIT(2))
/* precharge current */
#define PRECHRG_I_LIM_MASK		GENMASK(7, 4)
#define PRECHRG_I_LIM_STEP		60000   // uA
#define PRECHRG_I_LIM_MIN		60000   // uA
#define PRECHRG_I_LIM_MAX		780000  // uA
#define PRECHRG_I_LIM_DEF		180000  // uA
/* termination current */
#define TERMCHRG_I_LIM_MASK		GENMASK(3, 0)
#define TERMCHRG_I_LIM_STEP		60000   // uA
#define TERMCHRG_I_LIM_MIN		60000   // uA
#define TERMCHRG_I_LIM_MAX		960000  // uA
#define TERMCHRG_I_LIM_DEF		240000  // uA
/* charge current */
#define ICHRG_I_MASK			GENMASK(5, 0)
#define ICHRG_I_STEP			60000    // uA
#define ICHRG_I_MIN			0        // uA
#define ICHRG_I_MAX			2000000  // uA
#define ICHRG_I_DEF			500000   // uA
#define ICHRG_I_JEITA_MASK		BIT(0)
/* charge voltage */
#define VREG_V_MASK			GENMASK(7, 3)
#define VREG_V_MAX			4500000  // uV
#define VREG_V_MIN			3856000  // uV
#define VREG_V_DEF			4500000  // uV
#define VREG_V_STEP			32000    // uV
/* iindpm current */
#define IINDPM_I_MASK			GENMASK(4, 0)
#define IINDPM_I_MIN			100000   // uA
#define IINDPM_I_MAX			3200000  // uA
#define IINDPM_STEP			100000   // uA
#define IINDPM_DEF			500000   // uA
#define VINDPM_INT_MASK			BIT(1)
#define VINDPM_INT_DIS			BIT(1)
#define IINDPM_INT_MASK			BIT(0)
#define IINDPM_INT_DIS			BIT(0)
/* vindpm voltage */
#define VINDPM_V_MASK			GENMASK(3, 0)
#define VINDPM_V_MIN			3900000  // uV
#define VINDPM_OFFSET			3900000  // uV
#define VINDPM_V_MAX			5400000  // uV
#define VINDPM_STEP			100000   // uV
#define VINDPM_DEF			4500000  // uV
/* otg */
#define DEFAULT_OTG_VOLT		5000000  // uV
#define DEFAULT_OTG_CURRENT		1200000  // uV

enum chgerctrl_modes {
	CHGCTL_AUTO = 0,
	CHGCTL_MANUAL,
};

enum manual_modes {
	CHG_NOACTION = 0,
	CHG_DISABLE,
};

struct sy6974b_init_data {
	int ichg;     /* charge current */
	int vreg;     /* charge voltage */
	int iterm;    /* charge termination current */
	int iprechg;  /* precharge current */
	int ilim;     /* input max current for IINDPM */
	int vlim;     /* input minimum voltage for VINDPM */
	int max_ichg; /* battery max charge current */
	int max_vreg; /* battery max charge volt */
};

struct sy6974b_state {
	u8 stat; /* REG 8 state */
	u8 chg_type;
	u8 chg_stat;
	bool online;
	u8 param0; /* REG 0 param */
	u8 param1; /* REG 5 param */
	u8 param2; /* REG a param */
	bool bus_stat;
	u8 fault; /* REG 9 fault */
	u8 ntc_fault;
};

struct sy6974b_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;
	struct mutex prop_lock;
	struct mutex chgen_lock;
	struct regmap *regmap;
	int device_id;
	struct sy6974b_init_data init_data;
	struct sy6974b_state state;
	struct regulator_dev *otg_rdev;
	struct notifier_block pm_nb;
	struct gpio_desc *gpiod_otg_en;
	int input_current;
	int con_current;
	int con_volot;
	atomic_t chgctrl_mode;
	int chgctrl_icurrent_backup;
	bool sy6974b_suspend_flag;
	bool pmic_vdc_fall_irq_flag;
	bool watchdog_enable;
	struct workqueue_struct *sil_monitor_wq;
	struct delayed_work sil_watchdog_work;
};

/* SY6974x REG06 BOOST_LIM[5:4], uV */
static const unsigned int BOOST_VOLT_LIMIT[] = {
	4850000,
	5000000,
	5150000,
	5300000,
};

static const unsigned int BOOST_CURRENT_LIMIT[] = {
	500000,
	1200000,
};

enum VINDPM_OS {
	VINDPM_OS_3900mV,
	VINDPM_OS_5900mV,
	VINDPM_OS_7500mV,
	VINDPM_OS_10500mV,
};

static int sy6974b_set_term_curr(struct sy6974b_device *sil, int uA)
{
	int reg_val;
	int ret;

	if (uA < TERMCHRG_I_LIM_MIN)
		uA = TERMCHRG_I_LIM_MIN;
	else if (uA > TERMCHRG_I_LIM_MAX)
		uA = TERMCHRG_I_LIM_MAX;

	reg_val = (uA - TERMCHRG_I_LIM_MIN) / TERMCHRG_I_LIM_STEP;
	ret = regmap_update_bits(sil->regmap, REG_3, TERMCHRG_I_LIM_MASK, reg_val);
	if (ret)
		dev_err(sil->dev, "set term current error!\n");

	return ret;
}

static int sy6974b_set_prechrg_curr(struct sy6974b_device *sil, int uA)
{
	int reg_val;
	int ret;

	if (uA < PRECHRG_I_LIM_MIN)
		uA = PRECHRG_I_LIM_MIN;
	else if (uA > PRECHRG_I_LIM_MAX)
		uA = PRECHRG_I_LIM_MAX;

	reg_val = (uA - PRECHRG_I_LIM_MIN) / PRECHRG_I_LIM_STEP;
	reg_val = reg_val << 4;
	ret = regmap_update_bits(sil->regmap, REG_3, PRECHRG_I_LIM_MASK, reg_val);
	if (ret)
		dev_err(sil->dev, "set precharge current error!\n");

	return ret;
}

static int sy6974b_get_chrg_curr(struct sy6974b_device *sil)
{
	unsigned int reg_val;
	unsigned int ichg_reg_code;
	int ret;

	ret = regmap_read(sil->regmap, REG_2, &reg_val);
	if (ret) {
		dev_err(sil->dev, "get chrg current error!\n");
		return -EINVAL;
	}

	ichg_reg_code = reg_val & ICHRG_I_MASK;

	return ichg_reg_code * ICHRG_I_STEP;
}

static int sy6974b_set_chrg_curr(struct sy6974b_device *sil, int iuA)
{
	int reg_val;
	int ret;

	if (iuA < ICHRG_I_MIN) {
		iuA = ICHRG_I_MIN;
	} else {
		if ((sil->init_data.max_ichg > 0) && (iuA > sil->init_data.max_ichg))
			iuA = sil->init_data.max_ichg;
		iuA = min(iuA, ICHRG_I_MAX);
	}

	reg_val = iuA / ICHRG_I_STEP;
	ret = regmap_update_bits(sil->regmap, REG_2, ICHRG_I_MASK, reg_val);
	if (ret)
		dev_err(sil->dev, "set charge current error!\n");
	else
		sil->con_current = iuA;

	return ret;
}

static int sy6974b_get_chrg_volt(struct sy6974b_device *sil)
{
	unsigned int reg_val;
	unsigned int vchg_reg_code;
	int ret;

	ret = regmap_read(sil->regmap, REG_4, &reg_val);
	if (ret) {
		dev_err(sil->dev, "get chrg volt error!\n");
		return -EINVAL;
	}

	vchg_reg_code = reg_val & VREG_V_MASK;
	vchg_reg_code = vchg_reg_code >> 3;

	return VREG_V_MIN + vchg_reg_code * VREG_V_STEP;
}

static int sy6974b_set_chrg_volt(struct sy6974b_device *sil, int vuV)
{
	int reg_val;
	int ret;

	if (vuV < VREG_V_MIN) {
		vuV = VREG_V_MIN;
	} else {
		if ((sil->init_data.max_vreg > 0) && (vuV > sil->init_data.max_vreg))
			vuV = sil->init_data.max_vreg;
		vuV = min(vuV, VREG_V_MAX);
	}

	reg_val = (vuV - VREG_V_MIN) / VREG_V_STEP;
	reg_val = reg_val << 3;
	ret = regmap_update_bits(sil->regmap, REG_4, VREG_V_MASK, reg_val);
	if (ret)
		dev_err(sil->dev, "set charge voltage error!\n");
	else
		sil->con_volot = vuV;

	return ret;
}

static int sy6974b_set_input_volt_lim(struct sy6974b_device *sil, int vinput)
{
	unsigned int reg_val;
	int ret;

	if (vinput < VINDPM_V_MIN || vinput > VINDPM_V_MAX) {
		dev_err(sil->dev, "input volt: %duV out of range limit!\n", vinput);
		return -EINVAL;
	}

	vinput = clamp(vinput, VINDPM_V_MIN, VINDPM_V_MAX);
	reg_val = (vinput - VINDPM_OFFSET) / VINDPM_STEP;
	ret = regmap_update_bits(sil->regmap, REG_6, VINDPM_V_MASK, reg_val);
	if (ret) {
		dev_err(sil->dev, "set input voltage error!\n");
		return ret;
	}

	return ret;
}

static int sy6974b_set_input_curr_lim(struct sy6974b_device *sil, int iinput)
{
	enum chgerctrl_modes ctrl_mode = (enum chgerctrl_modes)atomic_read(&sil->chgctrl_mode);
	int reg_val;
	int ret;

	if (CHGCTL_MANUAL == ctrl_mode)
		iinput = IINDPM_I_MIN;

	dev_info(sil->dev, "ctrl mode: %d input current: %duA\n", ctrl_mode, iinput);
	if (iinput < IINDPM_I_MIN) {
		dev_err(sil->dev, "input curr: %duA out of range limit!\n", iinput);
		return -EINVAL;
	}

	if ((iinput > IINDPM_I_MAX) || (iinput > sil->init_data.ilim)) {
		dev_warn(sil->dev, "input curr: %d exceeding the limit, so fixed to: %d\n", iinput,
			 sil->init_data.ilim);
		iinput = min(IINDPM_I_MAX, sil->init_data.ilim);
	}

	sil->input_current = iinput;
	reg_val = (iinput - IINDPM_I_MIN) / IINDPM_STEP;

	ret = regmap_update_bits(sil->regmap, REG_0, IINDPM_I_MASK, reg_val);
	if (ret) {
		dev_err(sil->dev, "set input current limit error!\n");
		return ret;
	}

	return ret;
}

static int sy6974b_get_input_curr_lim(struct sy6974b_device *sil)
{
	int ret;
	unsigned int reg_ilim;

	ret = regmap_read(sil->regmap, REG_0, &reg_ilim);
	if (ret) {
		dev_err(sil->dev, "get input curr limit error!\n");
		return ret;
	}
	if (IINDPM_I_MASK == (reg_ilim & IINDPM_I_MASK))
		return IINDPM_I_MAX;

	reg_ilim = reg_ilim & IINDPM_I_MASK;

	return IINDPM_I_MIN + reg_ilim * IINDPM_STEP;
}

static int sy6974b_watchdog_timer_reset(struct sy6974b_device *sil)
{
	int ret;

	ret = regmap_update_bits(sil->regmap, REG_1, WDT_RST_MASK, WDT_RST);
	if (ret)
		dev_err(sil->dev, "reset watchdog timer error!\n");

	return ret;
}

static int sy6974b_set_watchdog_timer(struct sy6974b_device *sil, int time)
{
	u8 reg_val;
	int ret;

	if (time == 0)
		reg_val = WDT_TIMER_DISABLE;
	else if (time == 40)
		reg_val = WDT_TIMER_40S;
	else if (time == 80)
		reg_val = WDT_TIMER_80S;
	else
		reg_val = WDT_TIMER_160S;

	ret = regmap_update_bits(sil->regmap, REG_5, WDT_TIMER_MASK, reg_val);
	if (ret) {
		dev_err(sil->dev, "set watchdog timer error!\n");
		return ret;
	}

	if (time) {
		if (!sil->watchdog_enable)
			queue_delayed_work(sil->sil_monitor_wq, &sil->sil_watchdog_work,
					   msecs_to_jiffies(1000 * 5));
		sil->watchdog_enable = true;
	} else {
		sil->watchdog_enable = false;
		sy6974b_watchdog_timer_reset(sil);
	}

	return ret;
}

static int sy6974b_get_state(struct sy6974b_device *sil, struct sy6974b_state *state)
{
	int chrg_param_0 = 0, chrg_param_1 = 0, chrg_param_2 = 0;
	int chrg_stat = 0, fault = 0;
	int ret;

	ret = regmap_read(sil->regmap, REG_8, &chrg_stat);
	if (ret) {
		dev_err(sil->dev, "read state reg error!\n");
		return ret;
	}
	state->chg_type = chrg_stat & VBUS_STAT_MASK;
	state->chg_stat = chrg_stat & CHG_STAT_MASK;
	state->online = !!(chrg_stat & PG_STAT_MASK);
	state->stat = chrg_stat;

	ret = regmap_read(sil->regmap, REG_9, &fault);
	if (ret) {
		dev_err(sil->dev, "read fault reg error!\n");
		return ret;
	}
	state->ntc_fault = fault & TEMP_STAT_MASK;
	state->fault = fault;

	ret = regmap_read(sil->regmap, REG_0, &chrg_param_0);
	if (ret) {
		dev_err(sil->dev, "read hiz stat error!\n");
		return ret;
	}
	state->param0 = chrg_param_0;

	ret = regmap_read(sil->regmap, REG_5, &chrg_param_1);
	if (ret) {
		dev_err(sil->dev, "read term stat error!\n");
		return ret;
	}
	state->param1 = chrg_param_1;

	ret = regmap_read(sil->regmap, REG_A, &chrg_param_2);
	if (ret) {
		dev_err(sil->dev, "read vbus stat error!\n");
		return ret;
	}
	state->bus_stat = !!(chrg_param_2 & VBUS_GOOD_MASK);
	state->param2 = chrg_param_2;

	return ret;
}

static int sy6974b_enable_charger(struct sy6974b_device *sil, enum chgerctrl_modes mode)
{
	int ret = 0;
	enum chgerctrl_modes lastctrl_mode = (enum chgerctrl_modes)atomic_read(&sil->chgctrl_mode);

	mutex_lock(&sil->chgen_lock);
	if (CHGCTL_AUTO == mode && CHGCTL_MANUAL == lastctrl_mode) {
		dev_info(sil->dev, "In manual control state, auto will ignore.\n");
		mutex_unlock(&sil->chgen_lock);
		return 0;
	}
	// enable bat eft
	ret = regmap_update_bits(sil->regmap, REG_1, CHRG_EN_MASK, CHRG_EN);
	if (ret)
		dev_err(sil->dev, "enable charger error!\n");
	mutex_unlock(&sil->chgen_lock);

	return ret;
}

static int sy6974b_disable_charger(struct sy6974b_device *sil, enum chgerctrl_modes mode)
{
	int ret = 0;

	mutex_lock(&sil->chgen_lock);
	if (CHGCTL_MANUAL == mode)
		atomic_set(&sil->chgctrl_mode, CHGCTL_MANUAL);

	// disable charger
	ret = regmap_update_bits(sil->regmap, REG_1, CHRG_EN_MASK, 0);
	if (ret)
		dev_err(sil->dev, "disable charger error!\n");
	mutex_unlock(&sil->chgen_lock);

	return ret;
}

static int sy6974b_set_vac_ovp(struct sy6974b_device *sil)
{
	int reg_val;
	int ret;

	reg_val = 0xFF & VAC_OVP_MASK;  //  FF->11: 14V(12V input); 7F->01 6.5V(5V input)
	ret = regmap_update_bits(sil->regmap, REG_6, VAC_OVP_MASK, reg_val);
	if (ret)
		dev_err(sil->dev, "set vac ovp error!\n");

	return ret;
}

static int sy6974b_set_recharge_volt(struct sy6974b_device *sil, int recharge_volt)
{
	int reg_val;
	int ret;

	reg_val = (recharge_volt - VRECHRG_OFFSET) / VRECHRG_STEP;
	ret = regmap_update_bits(sil->regmap, REG_4, VRECHARGE_MASK, reg_val);
	if (ret)
		dev_err(sil->dev, "set recharger volt error!\n");

	return ret;
}

static int sy6974b_property_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_ONLINE:
		return true;
	default:
		return false;
	}
}

static void sy6974b_charger_external_power_changed(struct power_supply *psy)
{
	struct sy6974b_device *sil = power_supply_get_drvdata(psy);
	union power_supply_propval val;
	int ret;

	ret = power_supply_get_property_from_supplier(psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
	if (ret) {
		dev_err(sil->dev, "Get prop usb type form supply error!\n");
		return;
	}

	CH_DBG("external power changed...\n");
	switch (val.intval) {
	case POWER_SUPPLY_USB_TYPE_DCP:
		sy6974b_set_input_curr_lim(sil, 2000000);
		break;
	case POWER_SUPPLY_USB_TYPE_CDP:
	case POWER_SUPPLY_USB_TYPE_ACA:
		sy6974b_set_input_curr_lim(sil, 1500000);
		break;
	case POWER_SUPPLY_USB_TYPE_SDP:
	default:
		sy6974b_set_input_curr_lim(sil, 500000);
		break;
	}

	power_supply_changed(psy);
}

static int sy6974b_charger_set_property(struct power_supply *psy, enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct sy6974b_device *sil = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		dev_info(sil->dev, "set charger %s\n", val->intval ? "enable" : "disable");
		if (val->intval)
			ret = sy6974b_enable_charger(sil, CHGCTL_AUTO);
		else
			ret = sy6974b_disable_charger(sil, CHGCTL_AUTO);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sy6974b_set_chrg_curr(sil, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sy6974b_set_chrg_volt(sil, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sy6974b_set_input_curr_lim(sil, val->intval);
		sil->chgctrl_icurrent_backup = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int sy6974b_charger_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sy6974b_device *sil = power_supply_get_drvdata(psy);
	struct sy6974b_state state = {0};
	int ret = 0, regval = 0;

	mutex_lock(&sil->prop_lock);
	ret = sy6974b_get_state(sil, &state);
	if (ret) {
		dev_err(sil->dev, "get state error!\n");
		mutex_unlock(&sil->prop_lock);
		return ret;
	}
	sil->state = state;
	mutex_unlock(&sil->prop_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!state.chg_type || (state.chg_type == OTG_MODE))
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (!state.chg_stat)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (state.chg_stat == TERM_CHRG_STAT)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (state.chg_stat) {
		case PRECHRG_STAT:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case FAST_CHRG_STAT:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		case TERM_CHRG_STAT:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case NOT_CHRGING_STAT:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = state.bus_stat;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = sy6974b_get_chrg_curr(sil);
		if (val->intval < 0) {
			dev_err(sil->dev, "get chrge current failed!\n");
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = sy6974b_get_chrg_volt(sil);
		if (val->intval < 0) {
			dev_err(sil->dev, "get chrge volt failed!\n");
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = ICHRG_I_MAX;
		if (sil->state.ntc_fault == TEMP_COOL) {
			ret = regmap_read(sil->regmap, REG_5, &regval);
			if (!ret) {
				regval &= ICHRG_I_JEITA_MASK;
				dev_info(sil->dev, "temperature is cool will limit charging current!\n");
				/* When temperature is too low, charge current is decreased */
				if (regval)
					val->intval /= 5;
				else
					val->intval /= 2;
			}
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = sy6974b_get_input_curr_lim(sil);
		if (val->intval < 0) {
			dev_err(sil->dev, "get chrge input current failed!\n");
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		val->intval = sil->init_data.vlim;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = NAME;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = sy6974b_get_chrg_volt(sil);
		dev_info(sil->dev,
			 "stat: 0x%x charge [online: 0x%x status: 0x%x conV: %d conI: %d], fault: 0x%x, param0: 0x%x, param1: 0x%x, param2: 0x%x\n",
			 state.stat, state.online, state.chg_stat, sil->con_volot, sil->con_current,
			 state.fault, state.param0, state.param1, state.param2);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static enum power_supply_property sy6974b_power_supply_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static char *sy6974b_charger_supplied_to[] = {
	"usb",
};

static struct power_supply_desc sy6974b_power_supply_desc = {
	.name = "sy6974b-charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = sy6974b_power_supply_props,
	.num_properties = ARRAY_SIZE(sy6974b_power_supply_props),
	.get_property = sy6974b_charger_get_property,
	.set_property = sy6974b_charger_set_property,
	.property_is_writeable = sy6974b_property_is_writeable,
	.external_power_changed = sy6974b_charger_external_power_changed,
};

static int sy6974b_power_supply_init(struct sy6974b_device *sil, struct device *dev)
{
	struct power_supply_config psy_cfg = {
		.drv_data = sil,
		.of_node = dev->of_node,
	};

	psy_cfg.supplied_to = sy6974b_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sy6974b_charger_supplied_to);
	sil->charger = devm_power_supply_register(sil->dev, &sy6974b_power_supply_desc, &psy_cfg);
	if (IS_ERR(sil->charger))
		return -EINVAL;

	return 0;
}

static ssize_t registers_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sy6974b_device *sil = dev_get_drvdata(dev);
	u8 tmpbuf[30];
	int idx = 0;
	u8 addr;
	int val = 0, len = 0, ret = 0;

	for (addr = 0x0; addr <= REG_B; addr++) {
		ret = regmap_read(sil->regmap, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, 30, "Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t registers_store(struct device *dev, struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct sy6974b_device *sil = dev_get_drvdata(dev);
	unsigned int reg;
	int ret;
	int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= REG_B)
		regmap_write(sil->regmap, (unsigned char)reg, val);

	return count;
}
static DEVICE_ATTR_RW(registers);

static ssize_t manualctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sy6974b_device *sil = dev_get_drvdata(dev);
	enum chgerctrl_modes ctrl_mode = (enum chgerctrl_modes)atomic_read(&sil->chgctrl_mode);

	return sprintf(buf, "%d\n", ctrl_mode);
}

static ssize_t manualctrl_store(struct device *dev, struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct sy6974b_device *sil = dev_get_drvdata(dev);
	struct sy6974b_state state;
	int ret = 0, manual_chgmode = CHG_NOACTION;

	ret = sscanf(buf, "%d", &manual_chgmode);
	if (ret == 1) {
		switch (manual_chgmode) {
		case CHG_DISABLE:
			sil->chgctrl_icurrent_backup = sil->input_current;
			ret = sy6974b_disable_charger(sil, CHGCTL_MANUAL);
			ret |= sy6974b_set_input_curr_lim(sil, IINDPM_I_MIN);
			if (ret)
				return -EINVAL;
			dev_info(sil->dev, "manual set charger disable.\n");
			break;
		case CHG_NOACTION:
			atomic_set(&sil->chgctrl_mode, CHGCTL_AUTO);
			ret = sy6974b_get_state(sil, &state);
			if (ret) {
				dev_err(sil->dev, "get state error when set manual charger mode no action!\n");
				return -EINVAL;
			}
			if (state.bus_stat) {
				ret = sy6974b_set_input_curr_lim(sil, sil->chgctrl_icurrent_backup);
				ret |= sy6974b_enable_charger(sil, CHGCTL_AUTO);
				if (ret) {
					dev_err(sil->dev, "set charger enable error when disable manual!\n");
					return -EINVAL;
				}
			}
			dev_info(sil->dev, "exit charger manual mode.\n");
			break;
		default:
			dev_err(sil->dev, "manual charger mode: %d ignore!\n", manual_chgmode);
			return -EINVAL;
		}
	}

	return count;
}
static DEVICE_ATTR_RW(manualctrl);

static int sil_set_batfet_disable(struct sy6974b_device *sil, int disable)
{
	int ret = 0;
	int reg_val;

	if (disable) {
		reg_val = 0x6C;
		ret = regmap_write(sil->regmap, REG_7, reg_val);
	} else {
		reg_val = 0x4C;
		ret = regmap_write(sil->regmap, REG_7, reg_val);
	}
	if (ret) {
		dev_err(sil->dev, "set batfet_disable error!\n");
		return ret;
	}
	return 0;
}

static ssize_t batfetdis_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sy6974b_device *sil = dev_get_drvdata(dev);
	int val = 0, ret = 0;

	ret = regmap_read(sil->regmap, REG_7, &val);
	if (ret == 0)
		return sprintf(buf, "%.2X\n", val);
	return sprintf(buf, "error:%d\n", ret);
}

static ssize_t batfetdis_store(struct device *dev, struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct sy6974b_device *sil = dev_get_drvdata(dev);
	int ret = 0, batfetdis = 0;

	ret = sscanf(buf, "%d", &batfetdis);
	if (ret == 1) {
		ret = sil_set_batfet_disable(sil, 1);
		if (ret)
			return -EINVAL;
		dev_info(sil->dev, "batfetdis set 1.\n");
	} else {
		ret = sil_set_batfet_disable(sil, 0);
		if (ret)
			return -EINVAL;
		dev_info(sil->dev, "batfetdis set 0.\n");
	}

	return count;
}
static DEVICE_ATTR_RW(batfetdis);

static int sy6974b_create_device_node(struct device *dev)
{
	int ret = 0;

	ret = device_create_file(dev, &dev_attr_registers);
	if (ret)
		return ret;
	ret = device_create_file(dev, &dev_attr_manualctrl);
	if (ret) {
		device_remove_file(dev, &dev_attr_registers);
		return ret;
	}
	ret = device_create_file(dev, &dev_attr_batfetdis);
	if (ret) {
		device_remove_file(dev, &dev_attr_manualctrl);
		device_remove_file(dev, &dev_attr_registers);
	}

	return ret;
}

static void sy6974b_remove_device_node(struct device *dev)
{
	device_remove_file(dev, &dev_attr_registers);
	device_remove_file(dev, &dev_attr_manualctrl);
	device_remove_file(dev, &dev_attr_batfetdis);
}

static irqreturn_t sy6974b_irq_handler_thread(int irq, void *private)
{
	struct sy6974b_device *sil = private;
	struct sy6974b_state state;
	int ret;

	CH_DBG("charge ok irq!\n");
	ret = sy6974b_get_state(sil, &state);
	if (ret) {
		dev_err(sil->dev, "get state error!\n");
		return IRQ_NONE;
	}
	sil->state = state;
	if (state.bus_stat) {
		if (sil->input_current >= IINDPM_I_MIN) {
			ret = sy6974b_set_input_curr_lim(sil, sil->input_current);
			if (ret) {
				dev_err(sil->dev, "set input current error!\n");
				return IRQ_NONE;
			}
		}
	}
	power_supply_changed(sil->charger);

	return IRQ_HANDLED;
}

static bool sy6974b_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_0 ... REG_B:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config sy6974b_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_B,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = sy6974b_is_volatile_reg,
};

static int sy6974b_hw_init(struct sy6974b_device *sil)
{
	struct power_supply_battery_info *bat_info;
	int chrg_stat, ret = 0;

	ret = power_supply_get_battery_info(sil->charger, &bat_info);
	if (ret) {
		/* Allocate an empty battery */
		bat_info = devm_kzalloc(sil->dev, sizeof(*bat_info), GFP_KERNEL);
		if (!bat_info)
			return -ENOMEM;
		dev_info(sil->dev, "no battery information is supplied, use dummy!\n");
		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 120 mA, and default
		 * charge termination voltage to 4.35V.
		 */
		bat_info->constant_charge_current_max_ua = ICHRG_I_DEF;
		bat_info->constant_charge_voltage_max_uv = VREG_V_DEF;
		bat_info->precharge_current_ua = PRECHRG_I_LIM_DEF;
		bat_info->charge_term_current_ua = TERMCHRG_I_LIM_DEF;
	}
	if (!bat_info->constant_charge_current_max_ua)
		bat_info->constant_charge_current_max_ua = ICHRG_I_MAX;
	if (!bat_info->constant_charge_voltage_max_uv)
		bat_info->constant_charge_voltage_max_uv = VREG_V_DEF;
	if (!bat_info->precharge_current_ua)
		bat_info->precharge_current_ua = PRECHRG_I_LIM_DEF;
	if (!bat_info->charge_term_current_ua)
		bat_info->charge_term_current_ua = TERMCHRG_I_LIM_DEF;
	if (bat_info->constant_charge_current_max_ua)
		sil->init_data.max_ichg = bat_info->constant_charge_current_max_ua;
	if (bat_info->constant_charge_voltage_max_uv)
		sil->init_data.max_vreg = bat_info->constant_charge_voltage_max_uv;

	ret = sy6974b_set_watchdog_timer(sil, 0);
	if (ret)
		goto err_out;

	ret = sy6974b_set_prechrg_curr(sil, bat_info->precharge_current_ua);
	if (ret)
		goto err_out;

	ret = sy6974b_set_chrg_volt(sil, sil->init_data.max_vreg);
	if (ret)
		goto err_out;

	ret = sy6974b_set_term_curr(sil, bat_info->charge_term_current_ua);
	if (ret)
		goto err_out;

	ret = sy6974b_set_input_volt_lim(sil, sil->init_data.vlim);
	if (ret)
		goto err_out;

	ret = regmap_read(sil->regmap, REG_8, &chrg_stat);
	if (ret) {
		dev_err(sil->dev, "get state fail when hw init!\n");
		goto err_out;
	}

	if (!(chrg_stat & PG_STAT_MASK)) {
		ret = sy6974b_set_input_curr_lim(sil, sil->init_data.ilim);
		if (ret)
			goto err_out;
		ret = sy6974b_set_chrg_curr(sil, IINDPM_DEF);
		if (ret)
			goto err_out;
		ret = sy6974b_disable_charger(sil, CHGCTL_AUTO);
		if (ret)
			goto err_out;
		dev_info(sil->dev, "init power not good set ilim: %duA, ichrg: %duA\n",
			 sil->init_data.ilim, IINDPM_DEF);
	}
	ret = sy6974b_set_vac_ovp(sil);
	if (ret)
		goto err_out;

	regmap_update_bits(sil->regmap, REG_A, IINDPM_INT_MASK, IINDPM_INT_DIS);
	regmap_update_bits(sil->regmap, REG_A, VINDPM_INT_MASK, VINDPM_INT_DIS);

	ret = sy6974b_set_recharge_volt(sil, VRECHRG_DEF);
	if (ret)
		goto err_out;

	sil->con_current = sy6974b_get_chrg_curr(sil);

	dev_info(sil->dev,
		 "init iprechrg: %duA, iterm :%duA, vilim: %duV, vcharg: %duV, ovp: 6.5V, vrecharg: 200mV\n",
		 bat_info->precharge_current_ua, bat_info->charge_term_current_ua,
		 sil->init_data.vlim, sil->init_data.max_vreg);

	return 0;

err_out:
	dev_err(sil->dev, "hw init error!");
	return ret;
}

static int sy6974b_parse_dt(struct sy6974b_device *sil)
{
	int ret;

	ret = device_property_read_u32(sil->dev, "input-voltage-limit-microvolt",
				       &sil->init_data.vlim);
	if (ret) {
		dev_warn(sil->dev, "get config max charge input volt limit fail, use default!");
		sil->init_data.vlim = VINDPM_DEF;
	}
	if (sil->init_data.vlim > VINDPM_V_MAX || sil->init_data.vlim < VINDPM_V_MIN) {
		dev_err(sil->dev, "config max charge input volt limit out of support range, use default!\n");
		sil->init_data.vlim = VINDPM_DEF;
	}

	ret = device_property_read_u32(sil->dev, "input-current-limit-microamp",
				       &sil->init_data.ilim);
	if (ret) {
		dev_warn(sil->dev, "get config max charge input curr limit fail, use default!");
		sil->init_data.ilim = IINDPM_DEF;
	}
	if (sil->init_data.ilim > IINDPM_I_MAX || sil->init_data.ilim < IINDPM_I_MIN) {
		dev_err(sil->dev, "config max charge input curr limit out of support range, use default!");
		sil->init_data.ilim = IINDPM_DEF;
	}

	return 0;
}

static int sy6974b_set_otg_voltage(struct sy6974b_device *sil, int otguV)
{
	int ret = 0;
	int reg_val = -1;
	int i = 0;

	while (i < 4) {
		if (otguV == BOOST_VOLT_LIMIT[i]) {
			reg_val = i;
			break;
		}
		i++;
	}
	if (reg_val < 0) {
		dev_err(sil->dev, "otg volt not support when set!\n");
		return reg_val;
	}
	reg_val = reg_val << 4;
	ret = regmap_update_bits(sil->regmap, REG_6, BOOSTV_MASK, reg_val);
	if (ret) {
		dev_err(sil->dev, "set otg volt error!\n");
		return ret;
	}
	CH_DBG("set otg volt: %duV!\n", otguV);

	return ret;
}

static int sy6974b_set_otg_current(struct sy6974b_device *sil, int otguA)
{
	int ret = 0;

	if (otguA == BOOST_CURRENT_LIMIT[0]) {
		ret = regmap_update_bits(sil->regmap, REG_2, BOOST_LIM_MIN_MASK, 0);
		if (ret) {
			dev_err(sil->dev, "set otg current limit 500mA error!\n");
			return ret;
		}
	} else if (otguA == BOOST_CURRENT_LIMIT[1]) {
		ret = regmap_update_bits(sil->regmap, REG_2, BOOST_LIM_MIN_MASK, BIT(7));
		if (ret) {
			dev_err(sil->dev, "set otg current limit 1.2A error!\n");
			return ret;
		}
	}
	CH_DBG("set otg curr: %duA!\n", otguA);

	return ret;
}

static int sy6974b_enable_vbus(struct regulator_dev *rdev)
{
	struct sy6974b_device *sil = rdev_get_drvdata(rdev);
	int ret = 0;

	dev_info(sil->dev, "set OTG enable Vbus\n");
	ret = regmap_update_bits(sil->regmap, REG_1, OTG_EN_MASK, OTG_EN);
	if (ret) {
		dev_err(sil->dev, "set OTG enable error!\n");
		return ret;
	}

	return ret;
}

static int sy6974b_disable_vbus(struct regulator_dev *rdev)
{
	struct sy6974b_device *sil = rdev_get_drvdata(rdev);
	int ret = 0;

	dev_info(sil->dev, "set OTG disable Vbus\n");
	ret = regmap_update_bits(sil->regmap, REG_1, OTG_EN_MASK, 0);
	if (ret) {
		dev_err(sil->dev, "set OTG disable error!\n");
		return ret;
	}

	return ret;
}

static int sy6974b_is_enabled_vbus(struct regulator_dev *rdev)
{
	struct sy6974b_device *sil = rdev_get_drvdata(rdev);
	int temp = 0;
	int ret = 0;

	ret = regmap_read(sil->regmap, REG_1, &temp);
	if (ret) {
		dev_err(sil->dev, "get vbus status error!\n");
		return ret;
	}

	return (temp & OTG_EN) ? 1 : 0;
}

static int sy6974b_set_suspend_disable_vbus(struct regulator_dev *rdev)
{
	int ret = 0;

	if (sy6974b_is_enabled_vbus(rdev))
		ret = sy6974b_disable_vbus(rdev);
	return ret;
}

static const struct regulator_ops sy6974b_vbus_ops = {
	.enable = sy6974b_enable_vbus,
	.disable = sy6974b_disable_vbus,
	.is_enabled = sy6974b_is_enabled_vbus,
	.set_suspend_disable = sy6974b_set_suspend_disable_vbus,
};

static struct regulator_desc sy6974b_otg_rdesc = {
	.of_match = of_match_ptr("otg-vbus"),
	.id = 0,
	.name = "otg-vbus",
	.ops = &sy6974b_vbus_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.enable_reg = REG_1,
	.enable_mask = OTG_EN_MASK,
	.regulators_node = of_match_ptr("regulators"),
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sy6974b_vbus_regulator_register(struct sy6974b_device *sil)
{
	struct device_node *np;
	struct regulator_config config = {};
	int ret = 0;

	np = of_get_child_by_name(sil->dev->of_node, "regulators");
	if (!np) {
		dev_err(sil->dev, "cannot find regulators node!\n");
		return -ENXIO;
	}

	sil->gpiod_otg_en = devm_gpiod_get_optional(sil->dev, "otg-mode-en", GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(sil->gpiod_otg_en))
		dev_warn(sil->dev, "failed to request GPIO otg en pin!\n");

	/* otg regulator */
	config.dev = sil->dev;
	config.driver_data = sil;
	sil->otg_rdev = devm_regulator_register(sil->dev, &sy6974b_otg_rdesc, &config);
	if (IS_ERR(sil->otg_rdev)) {
		dev_err(sil->dev, "register vbus regulator failed!\n");
		ret = PTR_ERR(sil->otg_rdev);
	}

	of_node_put(np);

	return ret;
}

static int sy6974b_suspend_notifier(struct notifier_block *nb, unsigned long event, void *dummy)
{
	struct sy6974b_device *sil = container_of(nb, struct sy6974b_device, pm_nb);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		sil->sy6974b_suspend_flag = 1;
		/* changed tower: for otg disable vbus when deep/ultra sleep */
		if (sy6974b_is_enabled_vbus(sil->otg_rdev)) {
			//rk806_config_vdc_fall_irq(false);
			sil->pmic_vdc_fall_irq_flag = true;
			dev_info(sil->dev, "otg vbus enabled, disable pmic vdc fall irq when screen off suspend.\n");
		}
		/* changed end. */
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		sil->sy6974b_suspend_flag = 0;
		/* changed tower: for otg disable vbus when deep/ultra sleep */
		if (sil->pmic_vdc_fall_irq_flag) {
			//rk806_config_vdc_fall_irq(true);
			sil->pmic_vdc_fall_irq_flag = false;
			dev_info(sil->dev, "restore pmic vdc fall irq state when system resume.\n");
		}
		/* changed end. */
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static int sy6974b_hw_chipid_detect(struct sy6974b_device *sil)
{
	int ret = 0;
	int val = 0;

	ret = regmap_read(sil->regmap, REG_B, &val);
	if (ret < 0)
		return ret;

	return val;
}

static void sil_feeddog_work(struct work_struct *work)
{
	struct sy6974b_device *sil =
		container_of(work, struct sy6974b_device, sil_watchdog_work.work);

	sy6974b_watchdog_timer_reset(sil);
	if (sil->watchdog_enable)
		queue_delayed_work(sil->sil_monitor_wq, &sil->sil_watchdog_work,
				   msecs_to_jiffies(1000 * 5));
}

static int sy6974b_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sy6974b_device *sil;
	int ret;

	sil = devm_kzalloc(dev, sizeof(*sil), GFP_KERNEL);
	if (!sil)
		return -ENOMEM;

	sil->client = client;
	sil->dev = dev;

	mutex_init(&sil->prop_lock);
	mutex_init(&sil->chgen_lock);
	sil->regmap = devm_regmap_init_i2c(client, &sy6974b_regmap_config);
	if (IS_ERR(sil->regmap)) {
		dev_err(dev, "Failed to allocate register map\n");
		return PTR_ERR(sil->regmap);
	}
	atomic_set(&sil->chgctrl_mode, CHGCTL_AUTO);
	i2c_set_clientdata(client, sil);

	ret = sy6974b_parse_dt(sil);
	if (ret) {
		dev_err(dev, "Failed to read device tree properties%d\n", ret);
		return ret;
	}

	ret = sy6974b_hw_chipid_detect(sil);
	if ((ret & PN_MASK) != PN_ID) {
		dev_err(dev, "[%s] device not found !\n", __func__);
		return ret;
	}

	device_init_wakeup(dev, true);

	if (client->irq) {
		ret = devm_request_threaded_irq(
			dev, client->irq, NULL, sy6974b_irq_handler_thread,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_AUTOEN, "sy6974b-irq", sil);
		if (ret)
			goto error_out;
	}

	ret = sy6974b_power_supply_init(sil, dev);
	if (ret) {
		dev_err(dev, "Failed to register power supply\n");
		goto error_out;
	}

	ret = sy6974b_hw_init(sil);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		goto error_out;
	}

	/* OTG setting 5V/1.2A */
	ret = sy6974b_set_otg_voltage(sil, DEFAULT_OTG_VOLT);
	if (ret) {
		dev_err(sil->dev, "set OTG voltage error!\n");
		goto error_out;
	}

	ret = sy6974b_set_otg_current(sil, DEFAULT_OTG_CURRENT);
	if (ret) {
		dev_err(sil->dev, "set OTG current error!\n");
		goto error_out;
	}

	sil->sil_monitor_wq =
		alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_FREEZABLE, "sil-monitor-wq");
	if (!sil->sil_monitor_wq) {
		dev_err(sil->dev, "Failed to create workqueue\n");
		goto error_out;
	}
	INIT_DELAYED_WORK(&sil->sil_watchdog_work, sil_feeddog_work);

	ret = sy6974b_vbus_regulator_register(sil);
	if (ret)
		goto error_workqueue;

	sil->pm_nb.notifier_call = sy6974b_suspend_notifier;
	ret = register_pm_notifier(&sil->pm_nb);
	if (ret) {
		dev_err(sil->dev, "Failed to register PM notifier!\n");
		goto error_workqueue;
	}
	enable_irq(client->irq);
	enable_irq_wake(client->irq);
	ret = sy6974b_create_device_node(sil->dev);
	if (ret) {
		dev_err(sil->dev, "Failed to create device node!\n");
		goto error_register_pm;
	}

	return ret;

error_register_pm:
	unregister_pm_notifier(&sil->pm_nb);
error_workqueue:
	destroy_workqueue(sil->sil_monitor_wq);
error_out:
	return ret;
}

static void sy6974b_charger_remove(struct i2c_client *client)
{
	struct sy6974b_device *sil = i2c_get_clientdata(client);

	sy6974b_remove_device_node(sil->dev);
	unregister_pm_notifier(&sil->pm_nb);
	destroy_workqueue(sil->sil_monitor_wq);
	mutex_destroy(&sil->prop_lock);
	mutex_destroy(&sil->chgen_lock);
}

static void sy6974b_charger_shutdown(struct i2c_client *client)
{
	struct sy6974b_device *sil = i2c_get_clientdata(client);
	int ret = 0;

	sy6974b_set_prechrg_curr(sil, PRECHRG_I_LIM_DEF);
	ret = sy6974b_disable_charger(sil, CHGCTL_AUTO);
	if (ret)
		pr_err("Failed to disable charger, ret = %d\n", ret);
}

static const struct i2c_device_id sy6974b_i2c_ids[] = {
	{"sy6974b", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, sy6974b_i2c_ids);

static const struct of_device_id sy6974b_of_match[] = {
	{
		.compatible = "sil,sy6974b",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sy6974b_of_match);

static struct i2c_driver sy6974b_driver = {
	.driver = {
	.name = "sy6974b-charger",
		.of_match_table = sy6974b_of_match,
	},
	.probe = sy6974b_probe,
	.remove = sy6974b_charger_remove,
	.shutdown = sy6974b_charger_shutdown,
	.id_table = sy6974b_i2c_ids,
};
module_i2c_driver(sy6974b_driver);

MODULE_AUTHOR("Xu Shengfei <xsf@rock-chips.com>");
MODULE_DESCRIPTION("sy6974b charger driver");
MODULE_LICENSE("GPL");
