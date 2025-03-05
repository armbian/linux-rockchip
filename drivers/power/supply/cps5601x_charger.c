// SPDX-License-Identifier: GPL-2.0
/*
 * Chrager driver for cps5601x
 *
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 *
 * Author: Chen Shunqing <csq@rock-chips.com>
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

static int dbg_enable;

module_param_named(dbg_level, dbg_enable, int, 0644);

#define DBG(args...) \
	do { \
		if (dbg_enable) { \
			pr_info(args); \
		} \
	} while (0)

#define CPS5601X_MANUFACTURER		"ConvenientPower"
#define CPS5601X_MODEL_NAME		"cps5601x"

#define UPDATE(x, h, l)			(((x) << (l)) & GENMASK((h), (l)))

/* Register 00h */
#define CPS5601X_REG_00			0x00
#define CPS5601X_PRODUCT_ID_MASK	GENMASK(7, 0)
/* default 0xA9=CPS5601 */

/* Register 01h */
#define CPS5601X_REG_01			0x01

/* Register 02h */
#define CPS5601X_REG_02			0x02

/* Register 03h */
#define CPS5601X_REG_03			0x03
#define VREG_MASK			GENMASK(6, 0)
#define VREG_BASE			3600000
#define VREG_LSB			10000
#define VREG_MAXVAL			0x6e

/* Register 04h */
#define CPS5601X_REG_04			0x04
#define ICHG_MASK			GENMASK(6, 0)
#define ICHG_BASE			0
#define ICHG_LSB			25000
#define ICHG_MINVAL			0x4
#define ICHG_MAXVAL			0x78

/* Register 05h */
#define CPS5601X_REG_05			0x05
#define EN_TERM_MASK			BIT(6)
#define EN_TERM_ENABLE(x)		UPDATE(x, 6, 6)
#define IPRECHG_MASK			GENMASK(5, 0)
#define IPRECHG_BASE			0
#define IPRECHG_LSB			12500
#define IPRECHG_MINVAL			0x1
#define IPRECHG_MAXVAL			0x3c

/* Register 06h */
#define CPS5601X_REG_06			0x06
#define ITERM_MASK			GENMASK(5, 0)
#define ITERM_BASE			0
#define ITERM_LSB			10000
#define ITERM_MINVAL			0x5
#define ITERM_MAXVAL			0x3c

/* Register 07h */
#define CPS5601X_REG_07			0x07
#define VINDPM_MASK			GENMASK(5, 0)
#define VINDPM_BASE			3400000
#define VINDPM_LSB			100000
#define VINDPM_MINVAL			0x4
#define VINDPM_MAXVAL			0x3e

/* Register 08h */
#define CPS5601X_REG_08			0x08
#define IINDPM_MASK			GENMASK(5, 0)
#define IINDPM_BASE			50000
#define IINDPM_LSB			50000
#define IINDPM_MINVAL			0x1

/* Register 09h */
#define CPS5601X_REG_09			0x09
#define VOTG_MASK			GENMASK(5, 0)
#define VOTG_BASE			3400000
#define VOTG_LSB			100000
#define VOTG_MAXVAL			0x3e

/* Register 0Ah */
#define CPS5601X_REG_0A			0x0A
#define IOTG_MASK			GENMASK(5, 0)
#define IOTG_BASE			50000
#define IOTG_LSB			50000
#define IOTG_MINVAL			0x1

/* Register 0Bh */
#define CPS5601X_REG_0B			0x0B
#define WATCHDOG_MASK			GENMASK(7, 6)
#define WATCHDOG_TIME(x)		UPDATE(x, 7, 6)
#define WATCHDOG_BASE			0
#define WATCHDOG_LSB			40
#define WD_RST_MASK			BIT(5)
#define WD_RST(x)			UPDATE(x, 5, 5)
#define EN_CHG_MASK			BIT(3)
#define EN_CHG(x)			UPDATE(x, 3, 3)

/* Register 0Ch */
#define CPS5601X_REG_0C			0x0C
#define EN_OTG_MASK			BIT(3)
#define EN_OTG(x)			UPDATE(x, 3, 3)

/* Register 0Dh */
#define CPS5601X_REG_0D			0x0D

/* Register 0Eh */
#define CPS5601X_REG_0E			0x0E
#define TS_IGNORE_MASK			BIT(0)
#define EN_TS_IGNORE(x)			UPDATE(x, 0, 0)

/* Register 0Fh */
#define CPS5601X_REG_0F			0x0F
#define PG_STAT_MASK			BIT(3)

/* Register 10h */
#define CPS5601X_REG_10			0x10
#define CHG_STAT_MASK			GENMASK(7, 5)
#define CHG_STAT_SHIFT			5
#define CHG_STAT_NOTCHG			0
#define CHG_STAT_TRICKLECHG		1
#define CHG_STAT_PRECHG			2
#define CHG_STAT_FASTCHG		3
#define CHG_STAT_TAPERCHG		4
#define CHG_STAT_RESERVED		5
#define CHG_STAT_TOTACHG		6
#define CHG_STAT_CHGTERM		7
#define VBUS_STAT_MASK			GENMASK(4, 1)
#define VBUS_STAT_SHIFT			1
#define VBUS_STAT_NOT			0
#define VBUS_STAT_USBSDP		1
#define VBUS_STAT_USBCDP		2
#define VBUS_STAT_USBDCP		3
#define VBUS_STAT_HVDCP			4
#define VBUS_STAT_UNKNOWN		5
#define VBUS_STAT_NONSTANDARD		6
#define VBUS_STAT_OTGMODE		7
#define VBUS_STAT_NOTQUALIFIED		8

/* Register 11h */
#define CPS5601X_REG_11			0x11

/* Register 12h */
#define CPS5601X_REG_12			0x12

/* Register 13h */
#define CPS5601X_REG_13			0x13

/* Register 14h */
#define CPS5601X_REG_14			0x14

/* Register 15h */
#define CPS5601X_REG_15			0x15

/* Register 16h */
#define CPS5601X_REG_16			0x16

/* Register 17h */
#define CPS5601X_REG_17			0x17

/* Register 18h */
#define CPS5601X_REG_18			0x18

/* Register 19h */
#define CPS5601X_REG_19			0x19
#define TREG_MK_MASK			BIT(7)

/* Register 1Ah */
#define CPS5601X_REG_1A			0x1A

/* Register 1Bh */
#define CPS5601X_REG_1B			0x1B

#define CPS5601X_ICHRG_I_DEF_uA		2040000
#define CPS5601X_VREG_V_DEF_uV		4208000
#define CPS5601X_PRECHRG_I_DEF_uA	180000
#define CPS5601X_TERMCHRG_I_DEF_uA	180000
#define CPS5601X_ICHRG_I_MIN_uA		100000
#define CPS5601X_ICHRG_I_MAX_uA		3000000
#define CPS5601X_VINDPM_DEF_uV		4500000
#define CPS5601X_VINDPM_V_MIN_uV	3800000
#define CPS5601X_VINDPM_V_MAX_uV	9600000
#define CPS5601X_IINDPM_DEF_uA		2400000
#define CPS5601X_IINDPM_I_MIN_uA	100000
#define CPS5601X_IINDPM_I_MAX_uA	3200000
#define DEFAULT_INPUT_CURRENT		(500 * 1000)

static char *charge_state_str[] = {
	"No Charge",
	"Trickle Charge",
	"Pre-Charge",
	"Fast charge",
	"Taper charge",
	"Unknown",
	"Top-off timer active charging",
	"Charge terminated",
};

static char *charge_type_str[] = {
	"No Input",
	"USB SDP",
	"USB CDP",
	"USB DCP",
	"HVDCP",
	"Unknown adaptor",
	"Non-standard adapter",
	"OTG",
	"Not qualified adaptor",
};

struct cps5601x_init_data {
	int ichg;	/* charge current */
	int ilim;	/* input current */
	int vreg;	/* regulation voltage */
	int iterm;	/* termination current */
	int iprechg;	/* precharge current */
	int vlim;	/* minimum system voltage limit */
	int max_ichg;
	int max_ilim;
	int max_vreg;
};

struct cps5601x_state {
	bool vsys_stat;
	bool therm_stat;
	bool online;
	bool term_en;
	u8 chrg_stat;
	u8 chrg_type;
};

struct cps5601x {
	struct device *dev;
	struct i2c_client *client;
	struct mutex lock;

	struct regmap *regmap;
	struct cps5601x_state state;
	struct cps5601x_init_data init_data;
	struct regulator_dev *otg_rdev;

	struct power_supply *charger;
	struct power_supply *psy;

	struct workqueue_struct *cps_monitor_wq;
	struct delayed_work cps_delay_work;
	bool watchdog_enable;
	int part_no;
	int irq;
};

static int cps5601x_read(struct cps5601x *cps, int reg, int *val)
{
	int ret;

	ret = regmap_read(cps->regmap, reg, val);
	if (ret)
		dev_err(cps->dev, "%s: read 0x%x error!\n", __func__, reg);

	return ret;
}

static int cps5601x_update_bits(struct cps5601x *cps, int reg, int mask, int val)
{
	int ret;

	ret = regmap_update_bits(cps->regmap, reg, mask, val);
	if (ret)
		dev_err(cps->dev, "update reg: 0x%x mask:0x%x val: 0x%x error!\n",
			reg, mask, val);

	return ret;
}

static int cps5601x_detect_device(struct cps5601x *cps)
{
	int ret;
	int data;

	ret = cps5601x_read(cps, CPS5601X_REG_00, &data);
	if (!ret)
		cps->part_no = data & CPS5601X_PRODUCT_ID_MASK;
	if (cps->part_no != 0xa9) {
		dev_err(cps->dev, "cps5601x not detect device %x !\n", cps->part_no);
		return -1;
	}

	return ret;
}

static bool cps5601x_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CPS5601X_REG_00 ... CPS5601X_REG_1B:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config cps5601x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = CPS5601X_REG_1B,

	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = cps5601x_is_volatile_reg,
};

static int cps5601x_enable_charger(struct cps5601x *cps)
{
	return cps5601x_update_bits(cps, CPS5601X_REG_0B, EN_CHG_MASK, EN_CHG(1));
}

static int cps5601x_disable_charger(struct cps5601x *cps)
{
	return cps5601x_update_bits(cps, CPS5601X_REG_0B, EN_CHG_MASK, EN_CHG(0));
}

static int cps5601x_set_chargecurrent(struct cps5601x *cps, int curr)
{
	int ichg;

	if (curr < (ICHG_BASE + (ICHG_MINVAL * ICHG_LSB)))
		curr = ICHG_BASE + (ICHG_MINVAL * ICHG_LSB);
	else if (curr > (ICHG_BASE + (ICHG_MAXVAL * ICHG_LSB)))
		curr = ICHG_BASE + (ICHG_MAXVAL * ICHG_LSB);

	ichg = (curr - ICHG_BASE) / ICHG_LSB;

	return cps5601x_update_bits(cps, CPS5601X_REG_04, ICHG_MASK, ichg);
}

static int cps5601x_get_chargecurrent(struct cps5601x *cps, int *curr)
{
	int ret, val, icurr;


	ret = cps5601x_read(cps, CPS5601X_REG_04, &val);
	if (!ret) {
		icurr = val & ICHG_MASK;
		*curr = icurr * ICHG_LSB + ICHG_BASE;
	}

	return ret;
}

static int cps5601x_set_chargevolt(struct cps5601x *cps, int volt)
{
	int val;

	if (volt < VREG_BASE)
		volt = VREG_BASE;
	else if (volt > (VREG_BASE + (VREG_MAXVAL * VREG_LSB)))
		volt = VREG_BASE + (VREG_MAXVAL * VREG_LSB);

	val = (volt - VREG_BASE) / VREG_LSB;

	return cps5601x_update_bits(cps, CPS5601X_REG_03, VREG_MASK, val);
}

static int cps5601x_get_chargevol(struct cps5601x *cps, int *volt)
{
	int reg_val;
	int vchg;
	int ret;

	ret = cps5601x_read(cps, CPS5601X_REG_03, &reg_val);
	if (!ret) {
		vchg = reg_val & VREG_MASK;
		*volt = vchg * VREG_LSB + VREG_BASE;
	}

	return ret;
}

static int cps5601x_set_input_volt_limit(struct cps5601x *cps, int volt)
{
	int val;

	if (volt < (VINDPM_BASE + (VINDPM_MINVAL * VINDPM_LSB)))
		volt = VINDPM_BASE + (VINDPM_MINVAL * VINDPM_LSB);
	else if (volt > (VINDPM_BASE + (VINDPM_MAXVAL * VINDPM_LSB)))
		volt = VINDPM_BASE + (VINDPM_MAXVAL * VINDPM_LSB);

	val = (volt - VINDPM_BASE) / VINDPM_LSB;

	return cps5601x_update_bits(cps, CPS5601X_REG_07, VINDPM_MASK, val);
}

static int cps5601x_set_input_current_limit(struct cps5601x *cps, int curr)
{
	int val;

	if (curr < IINDPM_BASE + (IINDPM_MINVAL * IINDPM_LSB))
		curr = IINDPM_BASE + (IINDPM_MINVAL * IINDPM_LSB);

	val = (curr - IINDPM_BASE) / IINDPM_LSB;

	return cps5601x_update_bits(cps, CPS5601X_REG_08, IINDPM_MASK, val);
}

static int cps5601x_get_input_volt_limit(struct cps5601x *cps, u32 *volt)
{
	int reg_val;
	int vchg;
	int ret;

	ret = cps5601x_read(cps, CPS5601X_REG_07, &reg_val);
	if (!ret) {
		vchg = reg_val & VINDPM_MASK;
		*volt = vchg * VINDPM_LSB + VINDPM_BASE;
	}

	return ret;
}

static int cps5601x_get_input_current_limit(struct  cps5601x *cps)
{
	int reg_val;
	int icl, curr = 0;
	int ret;

	ret = cps5601x_read(cps, CPS5601X_REG_08, &reg_val);
	if (!ret) {
		icl = reg_val & IINDPM_MASK;
		curr = icl * IINDPM_LSB + IINDPM_BASE;
	}

	return curr;
}

static int cps5601x_set_iprechg(struct cps5601x *cps, int curr)
{
	int iprechg;

	if (curr < (IPRECHG_BASE + (IPRECHG_MINVAL * IPRECHG_LSB)))
		curr = IPRECHG_BASE + (IPRECHG_MINVAL * IPRECHG_LSB);
	else if (curr > (IPRECHG_BASE + (IPRECHG_MAXVAL * IPRECHG_LSB)))
		curr = IPRECHG_BASE + (IPRECHG_MAXVAL * IPRECHG_LSB);

	iprechg = (curr - IPRECHG_BASE) / IPRECHG_LSB;

	return cps5601x_update_bits(cps, CPS5601X_REG_05, IPRECHG_MASK, iprechg);
}

static int cps5601x_enable_term(struct cps5601x *cps, bool enable)
{
	int val;
	int ret;

	val = EN_TERM_ENABLE(enable);
	ret = cps5601x_update_bits(cps, CPS5601X_REG_05, EN_TERM_MASK, val);

	return ret;
}

static int cps5601x_set_term_current(struct cps5601x *cps, int curr)
{
	int iterm;

	if (curr < (ITERM_BASE + (ITERM_MINVAL * ITERM_LSB)))
		curr = ITERM_BASE + (ITERM_MINVAL * ITERM_LSB);
	else if (curr > (ITERM_BASE + (ITERM_MAXVAL * ITERM_LSB)))
		curr = ITERM_BASE + (ITERM_MAXVAL * ITERM_LSB);

	iterm = (curr - ITERM_BASE) / ITERM_LSB;

	return cps5601x_update_bits(cps, CPS5601X_REG_06, ITERM_MASK, iterm);

}

static int cps5601x_get_term_current(struct cps5601x *cps, int *curr)
{
	int reg_val;
	int iterm;
	int ret;

	ret = cps5601x_read(cps, CPS5601X_REG_06, &reg_val);
	if (!ret) {
		iterm = reg_val & ITERM_MASK;
		*curr = iterm * ITERM_LSB + ITERM_BASE;
	}

	return ret;
}

static int cps5601x_reset_watchdog_timer(struct cps5601x *cps)
{
	return cps5601x_update_bits(cps, CPS5601X_REG_0B, WD_RST_MASK, WD_RST(1));
}

static int cps5601x_set_watchdog_timer(struct cps5601x *cps, int timeout)
{
	int val, ret;

	val = (timeout - WATCHDOG_BASE) / WATCHDOG_LSB;
	ret = cps5601x_update_bits(cps, CPS5601X_REG_0B, WATCHDOG_MASK, WATCHDOG_TIME(val));
	if (ret) {
		dev_err(cps->dev, "cps5601x set watchdog fail\n");
		return ret;
	}

	if (timeout) {
		DBG("cps5601x: enable watchdog\n");
		if (!cps->watchdog_enable)
			queue_delayed_work(cps->cps_monitor_wq,
					   &cps->cps_delay_work,
					   msecs_to_jiffies(1000 * 5));
		cps->watchdog_enable = true;
	} else {
		DBG("cps5601x: disable watchdog\n");
		cps->watchdog_enable = false;
		cps5601x_reset_watchdog_timer(cps);
	}

	return ret;
}

static int cps5601x_ts_ignore(struct cps5601x *cps, bool en)
{
	return  cps5601x_update_bits(cps, CPS5601X_REG_0E, TS_IGNORE_MASK, EN_TS_IGNORE(en));
}

static int cps5601x_enable_otg(struct cps5601x *cps)
{
	return cps5601x_update_bits(cps, CPS5601X_REG_0C, EN_OTG_MASK, EN_OTG(1));
}

static int cps5601x_disable_otg(struct cps5601x *cps)
{
	return cps5601x_update_bits(cps, CPS5601X_REG_0C, EN_OTG_MASK, EN_OTG(0));
}

static int cps5601x_set_boost_current(struct cps5601x *cps, int curr)
{
	int val;

	if (curr < IOTG_BASE + (IOTG_MINVAL * IOTG_LSB))
		val = IOTG_BASE + (IOTG_MINVAL * IOTG_LSB);
	else
		val = ((curr - IOTG_BASE) / IOTG_LSB);

	return cps5601x_update_bits(cps, CPS5601X_REG_0A, IOTG_MASK, val);
}

static int cps5601x_set_boost_voltage(struct cps5601x *cps, int volt)
{
	int val = 0;

	if (volt < VOTG_BASE)
		volt = VOTG_BASE;

	if (volt > VOTG_BASE + (VOTG_MAXVAL * VOTG_LSB))
		volt = VOTG_BASE + (VOTG_MAXVAL * VOTG_LSB);

	val = (volt - VOTG_BASE) / VOTG_LSB;

	return cps5601x_update_bits(cps, CPS5601X_REG_09, VOTG_MASK, val);
}

static int cps5601x_get_state(struct cps5601x *cps,
			      struct cps5601x_state *state)
{
	int val, ret;

	ret = cps5601x_read(cps, CPS5601X_REG_10, &val);
	if (ret) {
		dev_err(cps->dev, "read CPS5601X_CHRG_STAT fail\n");
		return ret;
	}

	DBG("CPS5601X_CHRG_STAT[0x%x]: 0x%x\n", CPS5601X_REG_10, val);
	state->chrg_type = (val & VBUS_STAT_MASK) >> VBUS_STAT_SHIFT;
	state->chrg_stat = (val & CHG_STAT_MASK) >> CHG_STAT_SHIFT;

	ret = cps5601x_read(cps, CPS5601X_REG_0F, &val);
	if (ret) {
		dev_err(cps->dev, "read CPS5601X_PG fail\n");
		return ret;
	}
	state->online = !!(val & PG_STAT_MASK);

	ret = cps5601x_read(cps, CPS5601X_REG_19, &val);
	if (ret) {
		dev_err(cps->dev, "read CPS5601X_THERMAL fail\n");
		return ret;
	}
	state->therm_stat = !!(val & TREG_MK_MASK);

	ret = cps5601x_read(cps, CPS5601X_REG_05, &val);
	if (ret) {
		dev_err(cps->dev, "read CPS5601X_EN_TERM fail\n");
		return ret;
	}
	state->term_en = !!(val & EN_TERM_MASK);

	DBG("chrg_type: 0x%x\n", state->chrg_type);
	DBG("chrg_stat: 0x%x\n", state->chrg_stat);
	DBG("online: 0x%x\n", state->online);
	DBG("term_en: 0x%x\n", state->term_en);

	return ret;
}

static int cps5601x_property_is_writeable(struct power_supply *psy,
					  enum power_supply_property prop)
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

static int cps5601x_charger_set_property(struct power_supply *psy,
					 enum power_supply_property prop,
					 const union power_supply_propval *val)
{
	struct cps5601x *cps = power_supply_get_drvdata(psy);
	int ret = -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval) {
			ret = cps5601x_enable_charger(cps);
			cps5601x_set_watchdog_timer(cps, 40);
		} else {
			cps5601x_set_watchdog_timer(cps, 0);
			ret = cps5601x_disable_charger(cps);
		}

		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = cps5601x_set_input_current_limit(cps, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = cps5601x_set_chargecurrent(cps, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = cps5601x_set_chargevolt(cps, val->intval);
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static int cps5601x_charger_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	struct cps5601x *cps = power_supply_get_drvdata(psy);
	struct cps5601x_state state;
	int ret = 0;

	mutex_lock(&cps->lock);
	ret = cps5601x_get_state(cps, &state);
	if (ret) {
		dev_err(cps->dev, "get state error!\n");
		mutex_unlock(&cps->lock);
		return ret;
	}
	cps->state = state;
	mutex_unlock(&cps->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!state.chrg_type || (state.chrg_type == VBUS_STAT_OTGMODE))
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (!state.chrg_stat)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (state.chrg_stat == CHG_STAT_CHGTERM)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (state.chrg_stat) {
		case CHG_STAT_TRICKLECHG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case CHG_STAT_FASTCHG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		case CHG_STAT_CHGTERM:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case CHG_STAT_NOTCHG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = CPS5601X_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = CPS5601X_MODEL_NAME;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = true;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = cps->init_data.max_vreg;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = CPS5601X_ICHRG_I_MAX_uA;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		val->intval = CPS5601X_VINDPM_V_MAX_uV;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		 val->intval = cps5601x_get_input_current_limit(cps);
		if (val->intval < 0)
			return  -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static enum power_supply_property cps5601x_power_supply_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_PRESENT
};

static char *cps5601x_charger_supplied_to[] = {
	"usb",
};

static struct power_supply_desc cps5601x_power_supply_desc = {
	.name = "cps5601x-charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = cps5601x_power_supply_props,
	.num_properties = ARRAY_SIZE(cps5601x_power_supply_props),
	.get_property = cps5601x_charger_get_property,
	.set_property = cps5601x_charger_set_property,
	.property_is_writeable = cps5601x_property_is_writeable,
};

static irqreturn_t cps5601x_irq_handler_thread(int irq, void *private)
{
	struct cps5601x *cps = private;
	struct cps5601x_state state;
	int ret;
	u8 addr;
	int val;

	for (addr = 0x0; addr <= CPS5601X_REG_1B; addr++) {
		ret = cps5601x_read(cps, addr, &val);
		if (ret)
			dev_err(cps->dev, "read addr[0x%x] error!\n", addr);
		DBG("[0x%x]: 0x%x\n", addr, val);
	}
	ret = cps5601x_get_state(cps, &state);
	if (ret) {
		dev_err(cps->dev, "get state error!\n");
		return IRQ_NONE;
	}
	cps->state = state;
	power_supply_changed(cps->charger);

	return IRQ_HANDLED;
}

static int cps5601x_power_supply_init(struct cps5601x *cps, struct device *dev)
{
	struct power_supply_config psy_cfg = { .drv_data = cps,
					       .of_node = dev->of_node, };

	psy_cfg.supplied_to = cps5601x_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(cps5601x_charger_supplied_to);
	psy_cfg.of_node = dev->of_node;
	cps->charger = devm_power_supply_register(cps->dev,
						  &cps5601x_power_supply_desc,
						  &psy_cfg);
	if (IS_ERR(cps->charger))
		return -EINVAL;

	return 0;
}

static int cps5601x_hw_init(struct cps5601x *cps)
{
	struct power_supply_battery_info *bat_info;
	struct cps5601x_state state;
	int ret = 0;

	ret = power_supply_get_battery_info(cps->charger, &bat_info);
	if (ret) {
		/* Allocate an empty battery */
		bat_info = devm_kzalloc(cps->dev, sizeof(*bat_info), GFP_KERNEL);
		if (!bat_info)
			return -ENOMEM;
		dev_info(cps->dev, "cps5601x: no battery information is supplied\n");
		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 120 mA, and default
		 * charge termination voltage to 4.35V.
		 */
		bat_info->constant_charge_current_max_ua =
			CPS5601X_ICHRG_I_DEF_uA;
		bat_info->constant_charge_voltage_max_uv =
			CPS5601X_VREG_V_DEF_uV;
		bat_info->precharge_current_ua =
			CPS5601X_PRECHRG_I_DEF_uA;
		bat_info->charge_term_current_ua =
			CPS5601X_TERMCHRG_I_DEF_uA;
		cps->init_data.max_vreg = CPS5601X_VREG_V_DEF_uV;
	}
	if (!bat_info->constant_charge_current_max_ua)
		bat_info->constant_charge_current_max_ua = CPS5601X_ICHRG_I_MAX_uA;
	if (!bat_info->constant_charge_voltage_max_uv)
		bat_info->constant_charge_voltage_max_uv = CPS5601X_VREG_V_DEF_uV;
	if (!bat_info->precharge_current_ua)
		bat_info->precharge_current_ua = CPS5601X_PRECHRG_I_DEF_uA;
	if (!bat_info->charge_term_current_ua)
		bat_info->charge_term_current_ua = CPS5601X_TERMCHRG_I_DEF_uA;
	if (!cps->init_data.max_ichg)
		cps->init_data.max_ichg = CPS5601X_ICHRG_I_MAX_uA;

	if (bat_info->constant_charge_voltage_max_uv)
		cps->init_data.max_vreg = bat_info->constant_charge_voltage_max_uv;

	ret = cps5601x_set_watchdog_timer(cps, 0);
	if (ret)
		return ret;

	ret = cps5601x_set_iprechg(cps, bat_info->precharge_current_ua);
	if (ret)
		return ret;

	ret = cps5601x_set_chargevolt(cps, cps->init_data.max_vreg);
	if (ret)
		return ret;

	cps5601x_set_term_current(cps, bat_info->charge_term_current_ua);
	cps5601x_enable_term(cps, true);

	ret = cps5601x_set_input_volt_limit(cps, cps->init_data.vlim);
	if (ret)
		return ret;

	ret = cps5601x_get_state(cps, &state);
	if (ret || !state.online) {
		ret = cps5601x_set_input_current_limit(cps, DEFAULT_INPUT_CURRENT);
		if (ret)
			return ret;
		ret = cps5601x_set_chargecurrent(cps,
			      bat_info->constant_charge_current_max_ua);
		if (ret)
			return ret;

		ret = cps5601x_disable_charger(cps);
		if (ret)
			return ret;
	}
	cps5601x_ts_ignore(cps, true);

	DBG("ichrg_curr:%d\n"
	    "prechrg_curr:%d\n"
	    "chrg_vol:%d\n"
	    "term_curr:%d\n"
	    "input_curr_lim:%d\n",
	    bat_info->constant_charge_current_max_ua,
	    bat_info->precharge_current_ua,
	    bat_info->constant_charge_voltage_max_uv,
	    bat_info->charge_term_current_ua,
	    cps->init_data.ilim);

	return ret;
}

static int cps5601x_parse_dt(struct cps5601x *cps)
{
	int ret;

	ret = device_property_read_u32(cps->dev,
				       "input-voltage-limit-microvolt",
				       &cps->init_data.vlim);
	if (ret)
		cps->init_data.vlim = CPS5601X_VINDPM_DEF_uV;

	if (cps->init_data.vlim > CPS5601X_VINDPM_V_MIN_uV ||
	    cps->init_data.vlim < CPS5601X_VINDPM_V_MAX_uV)
		return -EINVAL;

	ret = device_property_read_u32(cps->dev,
				       "input-current-limit-microamp",
				       &cps->init_data.ilim);
	if (ret)
		cps->init_data.ilim = CPS5601X_IINDPM_DEF_uA;

	if (cps->init_data.ilim > CPS5601X_IINDPM_I_MIN_uA ||
	    cps->init_data.ilim < CPS5601X_IINDPM_I_MAX_uA)
		return -EINVAL;

	return 0;
}

static void cps_charger_work(struct work_struct *work)
{
	struct cps5601x *cps = container_of(work, struct cps5601x,
					   cps_delay_work.work);

	cps5601x_reset_watchdog_timer(cps);
	if (cps->watchdog_enable)
		queue_delayed_work(cps->cps_monitor_wq,
				   &cps->cps_delay_work,
				   msecs_to_jiffies(1000 * 5));
}

static int cps5601x_enable_vbus(struct regulator_dev *rdev)
{
	struct cps5601x *cps = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = cps5601x_enable_otg(cps);
	if (ret) {
		dev_err(cps->dev, "set OTG enable error!\n");
		return ret;
	}

	return ret;
}

static int cps5601x_disable_vbus(struct regulator_dev *rdev)
{
	struct cps5601x *cps = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = cps5601x_disable_otg(cps);
	if (ret) {
		dev_err(cps->dev, "set OTG disable error!\n");
		return ret;
	}

	return ret;
}

static int cps5601x_is_enabled_vbus(struct regulator_dev *rdev)
{
	struct cps5601x *cps = rdev_get_drvdata(rdev);
	int val, ret = 0;

	ret = cps5601x_read(cps, CPS5601X_REG_0C, &val);
	if (ret) {
		dev_err(cps->dev, "get vbus status error!\n");
		return ret;
	}

	return (val & EN_OTG_MASK) ? 1 : 0;
}

static const struct regulator_ops cps5601x_vbus_ops = {
	.enable = cps5601x_enable_vbus,
	.disable = cps5601x_disable_vbus,
	.is_enabled = cps5601x_is_enabled_vbus,
};

static struct regulator_desc cps5601x_otg_rdesc = {
	.of_match = "otg-vbus",
	.name = "otg-vbus",
	.regulators_node = of_match_ptr("regulators"),
	.ops = &cps5601x_vbus_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int cps5601x_vbus_regulator_register(struct cps5601x *cps)
{
	struct device_node *np;
	struct regulator_config config = {};
	int ret = 0;

	np = of_get_child_by_name(cps->dev->of_node, "regulators");
	if (!np) {
		dev_warn(cps->dev, "cannot find regulators node\n");
		return -ENXIO;
	}

	/* otg regulator */
	config.dev = cps->dev;
	config.driver_data = cps;
	cps->otg_rdev = devm_regulator_register(cps->dev,
						&cps5601x_otg_rdesc,
						&config);
	if (IS_ERR(cps->otg_rdev))
		ret = PTR_ERR(cps->otg_rdev);

	return ret;
}

static ssize_t registers_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct cps5601x *cps = dev_get_drvdata(dev);
	u8 tmpbuf[30];
	int idx = 0;
	u8 addr;
	int val;
	int len;
	int ret;

	for (addr = 0x0; addr <= CPS5601X_REG_1B; addr++) {
		ret = regmap_read(cps->regmap, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, 30,
				       "Reg[%.2X] = 0x%.2x\n",
				       addr,
				       val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct cps5601x *cps = dev_get_drvdata(dev);
	unsigned int reg;
	int ret;
	int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= CPS5601X_REG_1B)
		regmap_write(cps->regmap, (unsigned char)reg, val);

	return count;
}
static DEVICE_ATTR_RW(registers);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cps5601x *cps = dev_get_drvdata(dev);
	int chrg_curr = 0, chrg_volt = 0, icurr = 0, ivolt = 0, term_curr = 0;
	int chrg_state, chrg_type;
	struct cps5601x_state state;

	cps5601x_get_state(cps, &state);
	chrg_state = state.chrg_stat <= 7 ? state.chrg_stat : 0;
	chrg_type = state.chrg_type <= 8 ? state.chrg_type : 0;
	cps5601x_get_chargecurrent(cps, &chrg_curr);
	cps5601x_get_chargevol(cps, &chrg_volt);
	icurr = cps5601x_get_input_current_limit(cps);
	cps5601x_get_input_volt_limit(cps, &ivolt);
	cps5601x_get_term_current(cps, &term_curr);

	return snprintf(buf, PAGE_SIZE,
			"online: %d\n"
			"charge state: %s\n"
			"charge type: %s\n"
			"charge current: %d uA\n"
			"charge voltage: %d uV\n"
			"input current: %d uA\n"
			"input voltage: %d uV\n"
			"term current: %d uA\n",
			state.online,
			charge_state_str[chrg_state],
			charge_type_str[chrg_type],
			chrg_curr, chrg_volt,
			icurr,
			ivolt,
			term_curr);
}
static DEVICE_ATTR_RO(status);

static void cps5601x_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
	device_create_file(dev, &dev_attr_status);
}

static int cps5601x_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct cps5601x *cps;
	int ret;

	cps = devm_kzalloc(dev, sizeof(*cps), GFP_KERNEL);
	if (!cps)
		return -ENOMEM;

	cps->client = client;
	cps->dev = dev;

	mutex_init(&cps->lock);

	cps->regmap = devm_regmap_init_i2c(client, &cps5601x_regmap_config);
	if (IS_ERR(cps->regmap)) {
		dev_err(dev, "Failed to allocate register map\n");
		return PTR_ERR(cps->regmap);
	}

	i2c_set_clientdata(client, cps);

	ret = cps5601x_detect_device(cps);
	if (ret) {
		dev_err(cps->dev, "No cps5601x device found!\n");
		return -ENODEV;
	}

	cps5601x_parse_dt(cps);

	device_init_wakeup(dev, 1);

	ret = cps5601x_power_supply_init(cps, dev);
	if (ret) {
		dev_err(dev, "Failed to register power supply\n");
		return ret;
	}

	cps5601x_hw_init(cps);

	/* OTG setting 5V/1.2A */
	ret = cps5601x_set_boost_voltage(cps, 5000000);
	if (ret) {
		dev_err(cps->dev, "set OTG voltage error!\n");
		return ret;
	}
	ret = cps5601x_set_boost_current(cps, 1200000);
	if (ret) {
		dev_err(cps->dev, "set OTG current error!\n");
		return ret;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						cps5601x_irq_handler_thread,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT,
						"cps5601x-irq", cps);
		if (ret)
			return ret;
		enable_irq_wake(client->irq);
	}

	cps->cps_monitor_wq = alloc_ordered_workqueue("%s",
			WQ_MEM_RECLAIM | WQ_FREEZABLE, "cps-monitor-wq");
	if (!cps->cps_monitor_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&cps->cps_delay_work, cps_charger_work);

	cps5601x_vbus_regulator_register(cps);
	cps5601x_create_device_node(cps->dev);

	return 0;
}

static void cps5601x_charger_remove(struct i2c_client *client)
{
	struct cps5601x *cps = i2c_get_clientdata(client);

	device_remove_file(cps->dev, &dev_attr_registers);
	device_remove_file(cps->dev, &dev_attr_status);
	destroy_workqueue(cps->cps_monitor_wq);
	mutex_destroy(&cps->lock);
}

static void cps5601x_charger_shutdown(struct i2c_client *client)
{
	struct cps5601x *cps = i2c_get_clientdata(client);
	int ret = 0;

	cps5601x_set_iprechg(cps, CPS5601X_PRECHRG_I_DEF_uA);
	ret = cps5601x_disable_charger(cps);
	if (ret)
		dev_err(cps->dev, "Failed to disable charger, ret = %d\n", ret);
}

static const struct i2c_device_id cps5601x_i2c_ids[] = {
	{ "cps5601x", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, cps5601x_i2c_ids);

static const struct of_device_id cps5601x_of_match[] = {
	{ .compatible = "cps,cps5601x", },
	{ }
};
MODULE_DEVICE_TABLE(of, cps5601x_of_match);

static struct i2c_driver cps5601x_driver = {
	.driver = {
		.name = "cps5601x-charger",
		.of_match_table = cps5601x_of_match,
	},
	.probe = cps5601x_probe,
	.remove = cps5601x_charger_remove,
	.shutdown = cps5601x_charger_shutdown,
	.id_table = cps5601x_i2c_ids,
};
module_i2c_driver(cps5601x_driver);

MODULE_AUTHOR("Chen Shunqing <csq@rock-chips.com>");
MODULE_DESCRIPTION("cps5601x charger driver");
MODULE_LICENSE("GPL");
