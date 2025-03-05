// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include "linux/notifier.h"
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/types.h>

/* Module parameters. */
static int debug;
module_param_named(debug, debug, int, 0644);
MODULE_PARM_DESC(debug, "Set to one to enable debugging messages.");

#define DBG(args...) \
	do { \
		if (debug) { \
			pr_info(args); \
		} \
	} while (0)

#define SC89601_MANUFACTURER		"SOUTHCHIP"
#define SC89601_IRQ			"sc89601_irq"
#define SC89601_ID			3
#define SC89601_DEBUG_BUF_LEN		30
enum sc89601_fields {
	F_EN_HIZ, F_EN_STAT_PIN, F_IILIM, /* Reg00 */
	F_PFM_DIS, F_WD_RST, F_OTG_CFG, F_CHG_CFG, F_VSYSMIN, F_VBATLOW_OTG, /* Reg01 */
	F_BOOST_LIM, F_ICC, /* Reg02 */
	F_ITC, F_ITERM, /* Reg03 */
	F_VBAT_REG, F_TOPOFF_TIMER, F_VRECHG, /* Reg04 */
	F_EN_TERM, F_TWD, F_EN_TIMER, F_CHG_TIMER, F_TREG, F_JEITA_COOL_IS_ET1, /* Reg05 */
	F_VAC_OVP, F_BOOSTV12, F_VINDPM, /* Reg06 */
	F_FORCE_DPDM, F_TMR2X_EN, F_BATFET_DIS, FJEITA_WARM_VSET1,
	F_BATFET_DLY, F_BATFET_RST_EN, F_VIMDPM_TRACK, /* Reg07 */
	F_VBUS_STAT, F_CHG_STAT, F_PG_STAT, F_THERM_STAT, F_VSYS_STAT, /* Reg08 */
	F_WD_FALUT, F_BOOST_FAULT, F_CHG_FAULT, F_BAT_FAULT, F_NTC_FAULT, /* Reg09 */
	F_VBUS_GD, F_VINDPM_STAT, F_IINDPM_STAT, F_CV_STAT, F_TOPOFF_ACTIVE,
	F_ACOV_STAT, F_VIMDPM_INT_MASK, F_IINDPM_INT_MASK, /* Reg0A */
	F_REG_RST, F_PN, F_DEV_VER, /* Reg0B */
	F_JEITA_COOL_ISET2, F_JEITA_WARM_VSET2, F_JEITA_WARM_ISET,
	F_JEITA_COOL_TEMP, F_JEITA_WARM_TEMP, /* Reg0C */
	F_VBAT_REG_FT, F_BOOST_NTC_HOT_TEMP,
	F_BOOST_NTC_COLD_TEMP, F_BOOSTV03, F_ISHORT, /* Reg0D */
	F_VTC, F_INPUT_DET_DONE, F_AUTO_DPDM_EN, F_BUCK_FREQ,
	F_BOOST_FREQ, F_VSYSOVP, F_NTC_DIS, /* Reg0E */

	F_MAX_FIELDS
};

/* initial field values, converted to register values */
struct sc89601_init_data {
	u8 ichg;	/* charge current		*/
	u8 vreg;	/* regulation voltage		*/
	u8 iterm;	/* termination current		*/
	u8 iprechg;	/* precharge current		*/
	u8 sysvmin;	/* minimum system voltage limit */
	u8 boostv;	/* boost regulation voltage	*/
	u8 boosti;	/* boost current limit		*/
	u8 boostf;	/* boost frequency		*/
	u8 stat_pin_en;	/* enable STAT pin		*/
};

struct sc89601_state {
	u8 online;
	u8 chrg_status;
	u8 chrg_fault;
	u8 vsys_status;
	u8 boost_fault;
	u8 bat_fault;
};

struct sc89601_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;
	struct power_supply *tcpm_psy;
	struct regulator_dev *otg_vbus_reg;
	unsigned long usb_event;
	struct gpio_desc *gpiod_otg_en;
	struct regmap *rmap;
	struct regmap_field *rmap_fields[F_MAX_FIELDS];
	struct sc89601_init_data init_data;
	struct sc89601_state state;
	struct mutex lock; /* protect state data */
	struct delayed_work charger_phandle_work;
	struct notifier_block nb;
	int vbus_flag;
};

static const struct regmap_range sc89601_readonly_reg_ranges[] = {
	regmap_reg_range(0x08, 0x09),
};

static const struct regmap_access_table sc89601_writeable_regs = {
	.no_ranges = sc89601_readonly_reg_ranges,
	.n_no_ranges = ARRAY_SIZE(sc89601_readonly_reg_ranges),
};

static const struct regmap_range sc89601_volatile_reg_ranges[] = {
	regmap_reg_range(0x00, 0x00),
	regmap_reg_range(0x02, 0x02),
	regmap_reg_range(0x09, 0x09),
	regmap_reg_range(0x0b, 0x0b),
	regmap_reg_range(0x0c, 0x0c),
	regmap_reg_range(0x0d, 0x14),
};

static const struct regmap_access_table sc89601_volatile_regs = {
	.yes_ranges = sc89601_volatile_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(sc89601_volatile_reg_ranges),
};

static const struct regmap_config sc89601_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0x0E,
	.cache_type = REGCACHE_RBTREE,

	.wr_table = &sc89601_writeable_regs,
	.volatile_table = &sc89601_volatile_regs,
};

static const struct reg_field sc89601_reg_fields[] = {
	/* REG00 */
	[F_EN_HIZ]		= REG_FIELD(0x00, 7, 7),
	[F_EN_STAT_PIN]		= REG_FIELD(0x00, 5, 6),
	[F_IILIM]		= REG_FIELD(0x00, 0, 4),
	/* REG01 */
	[F_PFM_DIS]		= REG_FIELD(0x01, 7, 7),
	[F_WD_RST]		= REG_FIELD(0x01, 6, 6),
	[F_OTG_CFG]		= REG_FIELD(0x01, 5, 5),
	[F_CHG_CFG]		= REG_FIELD(0x01, 4, 4),
	[F_VSYSMIN]		= REG_FIELD(0x01, 1, 3),
	[F_VBATLOW_OTG]		= REG_FIELD(0x01, 0, 0),
	/* REG02 */
	[F_BOOST_LIM]		= REG_FIELD(0x02, 7, 7),
	[F_ICC]			= REG_FIELD(0x02, 0, 5),
	/* REG03 */
	[F_ITC]			= REG_FIELD(0x03, 4, 7),
	[F_ITERM]		= REG_FIELD(0x03, 0, 3),
	/* REG04 */
	[F_VBAT_REG]		= REG_FIELD(0x04, 3, 7),
	[F_TOPOFF_TIMER]	= REG_FIELD(0x04, 1, 2),
	[F_VRECHG]		= REG_FIELD(0x04, 0, 0),
	/* REG05 */
	[F_EN_TERM]		= REG_FIELD(0x05, 7, 7),
	[F_TWD]			= REG_FIELD(0x05, 4, 5),
	[F_EN_TIMER]		= REG_FIELD(0x05, 3, 3),
	[F_CHG_TIMER]		= REG_FIELD(0x05, 2, 2),
	[F_TREG]		= REG_FIELD(0x05, 1, 1),
	[F_JEITA_COOL_IS_ET1]	= REG_FIELD(0x05, 0, 0),
	/* REG06 */
	[F_VAC_OVP]		= REG_FIELD(0x06, 6, 7),
	[F_BOOSTV12]		= REG_FIELD(0x06, 4, 5),
	[F_VINDPM]		= REG_FIELD(0x06, 0, 3),
	/* REG07 */
	[F_FORCE_DPDM]		= REG_FIELD(0x07, 7, 7),
	[F_TMR2X_EN]		= REG_FIELD(0x07, 6, 6),
	[F_BATFET_DIS]		= REG_FIELD(0x07, 5, 5),
	[FJEITA_WARM_VSET1]	= REG_FIELD(0x07, 4, 4),
	[F_BATFET_DLY]		= REG_FIELD(0x07, 3, 3),
	[F_BATFET_RST_EN]	= REG_FIELD(0x07, 2, 2),
	[F_VIMDPM_TRACK]	= REG_FIELD(0x07, 0, 1),
	/* REG08 */
	[F_VBUS_STAT]		= REG_FIELD(0x08, 5, 7),
	[F_CHG_STAT]		= REG_FIELD(0x08, 3, 4),
	[F_PG_STAT]		= REG_FIELD(0x08, 2, 2),
	[F_THERM_STAT]		= REG_FIELD(0x08, 1, 1),
	[F_VSYS_STAT]		= REG_FIELD(0x08, 0, 0),
	/* REG09 */
	[F_WD_FALUT]		= REG_FIELD(0x09, 7, 7),
	[F_BOOST_FAULT]		= REG_FIELD(0x09, 6, 6),
	[F_CHG_FAULT]		= REG_FIELD(0x09, 4, 5),
	[F_BAT_FAULT]		= REG_FIELD(0x09, 3, 3),
	[F_NTC_FAULT]		= REG_FIELD(0x09, 0, 1),
	/* REG0A */
	[F_VBUS_GD]		= REG_FIELD(0x0A, 7, 7),
	[F_VINDPM_STAT]		= REG_FIELD(0x0A, 6, 6),
	[F_IINDPM_STAT]		= REG_FIELD(0x0A, 5, 5),
	[F_CV_STAT]		= REG_FIELD(0x0A, 4, 4),
	[F_TOPOFF_ACTIVE]	= REG_FIELD(0x0A, 3, 3),
	[F_ACOV_STAT]		= REG_FIELD(0x0A, 2, 2),
	[F_VIMDPM_INT_MASK]	= REG_FIELD(0x0A, 1, 1),
	[F_IINDPM_INT_MASK]	= REG_FIELD(0x0A, 0, 0),
	/* REG0B */
	[F_REG_RST]		= REG_FIELD(0x0B, 7, 7),
	[F_PN]			= REG_FIELD(0x0B, 3, 6),
	[F_DEV_VER]		= REG_FIELD(0x0B, 0, 1),
	/* REG0C */
	[F_JEITA_COOL_ISET2]	= REG_FIELD(0x0C, 7, 7),
	[F_JEITA_WARM_VSET2]	= REG_FIELD(0x0C, 6, 6),
	[F_JEITA_WARM_ISET]	= REG_FIELD(0x0C, 4, 5),
	[F_JEITA_COOL_TEMP]	= REG_FIELD(0x0C, 2, 3),
	[F_JEITA_WARM_TEMP]	= REG_FIELD(0x0C, 0, 1),
	/* REG0D */
	[F_VBAT_REG_FT]		= REG_FIELD(0x0D, 6, 7),
	[F_BOOST_NTC_HOT_TEMP]	= REG_FIELD(0x0D, 4, 5),
	[F_BOOST_NTC_COLD_TEMP]	= REG_FIELD(0x0D, 3, 3),
	[F_BOOSTV03]		= REG_FIELD(0x0D, 1, 2),
	[F_ISHORT]		= REG_FIELD(0x0D, 0, 0),
	/* REG0E */
	[F_VTC]			= REG_FIELD(0x0E, 7, 7),
	[F_INPUT_DET_DONE]	= REG_FIELD(0x0E, 6, 6),
	[F_AUTO_DPDM_EN]	= REG_FIELD(0x0E, 5, 5),
	[F_BUCK_FREQ]		= REG_FIELD(0x0E, 4, 4),
	[F_BOOST_FREQ]		= REG_FIELD(0x0E, 3, 3),
	[F_VSYSOVP]		= REG_FIELD(0x0E, 1, 2),
	[F_NTC_DIS]		= REG_FIELD(0x0E, 0, 0),
};

enum sc89601_status {
	STATUS_NOT_CHARGING,
	STATUS_PRE_CHARGING,
	STATUS_FAST_CHARGING,
	STATUS_TERMINATION_DONE,
};

enum sc89601_chrg_fault {
	CHRG_FAULT_NORMAL,
	CHRG_FAULT_INPUT,
	CHRG_FAULT_THERMAL_SHUTDOWN,
	CHRG_FAULT_TIMER_EXPIRED,
};

/*
 * Most of the val -> idx conversions can be computed, given the minimum,
 * maximum and the step between values. For the rest of conversions, we use
 * lookup tables.
 */
enum sc89601_table_ids {
	/* range tables */
	TBL_ICHG,
	TBL_ITERM,
	TBL_IILIM,
	TBL_VREG,
	TBL_BOOSTV,
	TBL_ITC,
	TBL_VINDPM,
	TBL_SYSVMIN,
	TBL_BOOSTI,
};

/* Boost mode current limit lookup table, in uA */
static const u32 sc89601_boosti_tbl[] = {
	500000, 1200000
};

#define SC89601_BOOSTI_TBL_SIZE		ARRAY_SIZE(sc89601_boosti_tbl)

/* sys min voltage lookup table, in uV */
static const u32 sc89601_vsys_tbl[] = {
	2600000, 2800000, 3000000, 3200000, 3400000, 3500000, 3600000, 3700000
};
#define SC89601_VSYS_TBL_SIZE		ARRAY_SIZE(sc89601_vsys_tbl)

struct sc89601_range {
	u32 min;
	u32 max;
	u32 step;
};

struct sc89601_lookup {
	const u32 *tbl;
	u32 size;
};

static const union {
	struct sc89601_range rt;
	struct sc89601_lookup lt;
} sc89601_tables[] = {
	/* range tables */
	[TBL_ICHG] = { .rt = {0, 3000000, 60000} }, /* uA */
	[TBL_ITERM] = { .rt = {60000, 960000, 60000} }, /* uA */
	[TBL_IILIM] = { .rt = {100000, 3200000, 100000} }, /* uA */
	[TBL_VREG] = { .rt = {3848000, 4864000, 32000} }, /* uV */
	[TBL_BOOSTV] = { .rt = {3900000, 5400000, 100000} }, /* uV */
	[TBL_ITC] = { .rt = {60000, 960000, 60000} }, /* uA */
	[TBL_VINDPM] = { .rt = { 3900000, 5100000, 100000 } }, /* mV */
	[TBL_SYSVMIN] = { .lt = {sc89601_vsys_tbl, SC89601_VSYS_TBL_SIZE} }, /* uV */
	[TBL_BOOSTI] = { .lt = {sc89601_boosti_tbl, SC89601_BOOSTI_TBL_SIZE} }
};

static const struct sc89601_range sc89601_vindpm_supplyment[] = {
	{ 8000000, 8400000, 200000 },
};

static int sc89601_field_read(struct sc89601_device *sc89601,
			      enum sc89601_fields field_id)
{
	int ret;
	int val;

	ret = regmap_field_read(sc89601->rmap_fields[field_id], &val);
	if (ret < 0)
		return ret;

	return val;
}

static int sc89601_field_write(struct sc89601_device *sc89601,
			       enum sc89601_fields field_id, u8 val)
{
	return regmap_field_write(sc89601->rmap_fields[field_id], val);
}

static u8 sc89601_find_idx(u32 value, enum sc89601_table_ids id)
{
	u8 idx;

	if (id >= TBL_SYSVMIN) {
		const u32 *tbl = sc89601_tables[id].lt.tbl;
		u32 tbl_size = sc89601_tables[id].lt.size;

		for (idx = 1; idx < tbl_size && tbl[idx] <= value; idx++)
			;
	} else {
		const struct sc89601_range *rtbl = &sc89601_tables[id].rt;
		u8 rtbl_size;

		if (id == TBL_VINDPM && value >= 8000000)
			rtbl = sc89601_vindpm_supplyment;

		rtbl_size = (rtbl->max - rtbl->min) / rtbl->step + 1;

		for (idx = 1;
		     idx < rtbl_size && (idx * rtbl->step + rtbl->min <= value);
		     idx++)
			;
		if (id == TBL_VINDPM && value >= 8000000)
			idx += 13;
	}

	return idx - 1;
}

static u32 sc89601_find_val(u8 idx, enum sc89601_table_ids id)
{
	const struct sc89601_range *rtbl;

	/* lookup table? */
	if (id >= TBL_SYSVMIN)
		return sc89601_tables[id].lt.tbl[idx];

	/* range table */
	rtbl = &sc89601_tables[id].rt;

	return (rtbl->min + idx * rtbl->step);
}

static int sc89601_get_chip_state(struct sc89601_device *sc89601,
				  struct sc89601_state *state)
{
	int i, ret;

	struct {
		enum sc89601_fields id;
		u8 *data;
	} state_fields[] = {
		{F_CHG_STAT, &state->chrg_status},
		{F_VBUS_GD, &state->online},
		{F_VSYS_STAT, &state->vsys_status},
		{F_BOOST_FAULT, &state->boost_fault},
		{F_BAT_FAULT, &state->bat_fault},
		{F_CHG_FAULT, &state->chrg_fault}
	};

	for (i = 0; i < ARRAY_SIZE(state_fields); i++) {
		ret = sc89601_field_read(sc89601, state_fields[i].id);
		if (ret < 0)
			return ret;

		*state_fields[i].data = ret;
	}

	DBG("SC89601: S:CHG/PG/VSYS=%d/%d/%d, F:CHG/BOOST/BAT=%d/%d/%d\n",
	    state->chrg_status, state->online, state->vsys_status,
	    state->chrg_fault, state->boost_fault, state->bat_fault);

	return 0;
}

static irqreturn_t __sc89601_handle_irq(struct sc89601_device *sc89601)
{
	struct sc89601_state new_state;
	int ret;

	ret = sc89601_get_chip_state(sc89601, &new_state);
	if (ret < 0)
		goto error;

	if (!memcmp(&sc89601->state, &new_state, sizeof(new_state)))
		return IRQ_HANDLED;

	sc89601->state = new_state;
	power_supply_changed(sc89601->charger);

	return IRQ_HANDLED;
error:
	dev_err(sc89601->dev, "Error communicating with the chip: %pe\n",
		ERR_PTR(ret));
	return IRQ_HANDLED;
}

static int sc89601_power_supply_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct sc89601_device *sc89601 = NULL;
	struct sc89601_state state;
	int ret;

	sc89601 = power_supply_get_drvdata(psy);
	mutex_lock(&sc89601->lock);
	__sc89601_handle_irq(sc89601);
	mutex_unlock(&sc89601->lock);
	state = sc89601->state;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!state.online)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (state.chrg_status == STATUS_NOT_CHARGING)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (state.chrg_status == STATUS_PRE_CHARGING ||
			 state.chrg_status == STATUS_FAST_CHARGING)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (state.chrg_status == STATUS_TERMINATION_DONE)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;

		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!state.online || state.chrg_status == STATUS_NOT_CHARGING ||
		    state.chrg_status == STATUS_TERMINATION_DONE)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else if (state.chrg_status == STATUS_PRE_CHARGING)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_STANDARD;
		else if (state.chrg_status == STATUS_FAST_CHARGING)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else /* unreachable */
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = SC89601_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "SC89601";
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = !!state.chrg_status;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (!state.chrg_fault && !state.bat_fault && !state.boost_fault)
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		else if (state.bat_fault)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else if (state.chrg_fault == CHRG_FAULT_TIMER_EXPIRED)
			val->intval = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
		else if (state.chrg_fault == CHRG_FAULT_THERMAL_SHUTDOWN)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = sc89601_find_val(sc89601->init_data.ichg, TBL_ICHG);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (!state.online) {
			val->intval = 0;
			break;
		}

		ret = sc89601_field_read(sc89601, F_VBAT_REG);
		if (ret < 0)
			return ret;

		val->intval = 3848000 + ret * 32000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = sc89601_find_val(sc89601->init_data.vreg, TBL_VREG);
		break;

	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		val->intval = sc89601_find_val(sc89601->init_data.iprechg, TBL_ITC);
		break;

	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		val->intval = sc89601_find_val(sc89601->init_data.iterm, TBL_ITERM);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc89601_field_read(sc89601, F_IILIM);
		if (ret < 0)
			return ret;

		val->intval = sc89601_find_val(ret, TBL_IILIM);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		val->intval = 13500000; /* uV */
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sc89601_enable_charger(struct sc89601_device *sc89601)
{
	return sc89601_field_write(sc89601, F_CHG_CFG, 1);
}

static int sc89601_disable_charger(struct sc89601_device *sc89601)
{
	return sc89601_field_write(sc89601, F_CHG_CFG, 0);
}

static int sc89601_power_supply_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct sc89601_device *sc89601 = power_supply_get_drvdata(psy);
	int index, ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		DBG("POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX\n");
		index = sc89601_find_idx(val->intval, TBL_ICHG);
		ret = sc89601_field_write(sc89601, F_ICC, index);
		if (ret < 0)
			dev_err(sc89601->dev, "set input voltage limit failed\n");
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		DBG("POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT---value:%d\n", val->intval);
		index = sc89601_find_idx(val->intval, TBL_IILIM);
		ret = sc89601_field_write(sc89601, F_IILIM, index);

		if (ret < 0)
			dev_err(sc89601->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		DBG("POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT--value:%d\n", val->intval);
		index = sc89601_find_idx(val->intval, TBL_VINDPM);
		ret = sc89601_field_write(sc89601, F_VINDPM, index);

		if (ret < 0)
			dev_err(sc89601->dev, "set input voltage limit failed\n");

		break;

	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval) {
			DBG("POWER_SUPPLY_PROP_ONLINE\n");
			ret = sc89601_enable_charger(sc89601);
			if (ret < 0)
				dev_err(sc89601->dev, "enable charge failed\n");
		} else {
			ret = sc89601_disable_charger(sc89601);
			if (ret < 0)
				dev_err(sc89601->dev, "disable charge failed\n");
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static irqreturn_t sc89601_irq_handler_thread(int irq, void *private)
{
	struct sc89601_device *sc89601 = private;
	irqreturn_t ret;

	mutex_lock(&sc89601->lock);
	ret = __sc89601_handle_irq(sc89601);
	mutex_unlock(&sc89601->lock);

	return ret;
}

static int sc89601_chip_reset(struct sc89601_device *sc89601)
{
	int ret;

	ret = sc89601_field_write(sc89601, F_REG_RST, 1);
	if (ret < 0)
		dev_err(sc89601->dev, "write reg rst failed\n");

	return ret;
}

static int sc89601_hw_init(struct sc89601_device *sc89601)
{
	int ret;
	int i;

	const struct {
		enum sc89601_fields id;
		u32 value;
	} init_data[] = {
		{F_ICC, sc89601->init_data.ichg},
		{F_VBAT_REG, sc89601->init_data.vreg},
		{F_ITERM, sc89601->init_data.iterm},
		{F_ITC, sc89601->init_data.iprechg},
		{F_VSYSMIN, sc89601->init_data.sysvmin},
		{F_BOOSTV12, sc89601->init_data.boostv},
		{F_BOOST_LIM, sc89601->init_data.boosti},
		{F_BOOST_FREQ, sc89601->init_data.boostf},
		{F_EN_STAT_PIN, sc89601->init_data.stat_pin_en},
		{F_NTC_DIS, 1},
	};

	/* disable watchdog */
	ret = sc89601_field_write(sc89601, F_TWD, 0);
	if (ret < 0) {
		dev_err(sc89601->dev, "Disabling watchdog failed %d\n", ret);
		return ret;
	}

	/* initialize currents/voltages and other parameters */
	for (i = 0; i < ARRAY_SIZE(init_data); i++) {
		if (init_data[i].id == F_BOOSTV12) {
			ret = sc89601_field_write(sc89601, F_BOOSTV12,
				(sc89601->init_data.boostv & 0x06) >> 1);
			ret |= sc89601_field_write(sc89601, F_BOOSTV03,
				((sc89601->init_data.boostv & 0x08) >> 2) |
				(sc89601->init_data.boostv & 0x01));
		} else {
			ret = sc89601_field_write(sc89601, init_data[i].id,
				init_data[i].value);
		}
		if (ret < 0) {
			dev_err(sc89601->dev, "Writing init data failed %d\n", ret);
			return ret;
		}
	}

	ret = sc89601_field_write(sc89601, F_AUTO_DPDM_EN, 0);
	if (ret < 0) {
		dev_err(sc89601->dev, "Config F_AUTO_DPDM_EN failed %d\n", ret);
		return ret;
	}
	ret = sc89601_field_write(sc89601, F_VAC_OVP, 3);
	if (ret < 0)
		dev_err(sc89601->dev, "Field write failed %d\n", ret);

	ret = sc89601_get_chip_state(sc89601, &sc89601->state);
	if (ret < 0) {
		dev_err(sc89601->dev, "Get state failed %d\n", ret);
		return ret;
	}

	return 0;
}

static const enum power_supply_property sc89601_power_supply_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_PRECHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static char *sc89601_charger_supplied_to[] = {
	"usb",
};

static const struct power_supply_desc sc89601_power_supply_desc = {
	.name = "sc89601-charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = sc89601_power_supply_props,
	.num_properties = ARRAY_SIZE(sc89601_power_supply_props),
	.set_property = sc89601_power_supply_set_property,
	.get_property = sc89601_power_supply_get_property,
};

static int sc89601_power_supply_init(struct sc89601_device *sc89601)
{
	struct power_supply_config psy_cfg = { .drv_data = sc89601, };

	psy_cfg.of_node = sc89601->dev->of_node;
	psy_cfg.supplied_to = sc89601_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sc89601_charger_supplied_to);

	sc89601->charger = devm_power_supply_register(sc89601->dev,
						      &sc89601_power_supply_desc,
						      &psy_cfg);

	if (PTR_ERR_OR_ZERO(sc89601->charger)) {
		dev_err(sc89601->dev, "failed to register power supply\n");
		return PTR_ERR(sc89601->charger);
	}

	return 0;
}

static int sc89601_get_chip_version(struct sc89601_device *sc89601)
{
	int id;

	id = sc89601_field_read(sc89601, F_PN);
	if (id < 0) {
		dev_err(sc89601->dev, "Cannot read chip ID.\n");
		return id;
	} else if (id != SC89601_ID) {
		dev_err(sc89601->dev, "Unknown chip ID %d\n", id);
		return -ENODEV;
	}

	DBG("charge IC: SC89601\n");

	return 0;
}

static void sc89601_set_otg_vbus(struct sc89601_device *sc, bool enable)
{
	sc89601_field_write(sc, F_OTG_CFG, enable);
}

static int sc89601_otg_vbus_enable(struct regulator_dev *dev)
{
	struct sc89601_device *sc = rdev_get_drvdata(dev);

	if (!IS_ERR_OR_NULL(sc->gpiod_otg_en))
		gpiod_direction_output(sc->gpiod_otg_en, 0x1);

	sc89601_disable_charger(sc);
	sc89601_set_otg_vbus(sc, true);

	return 0;
}

static int sc89601_otg_vbus_disable(struct regulator_dev *dev)
{
	struct sc89601_device *sc = rdev_get_drvdata(dev);

	if (!IS_ERR_OR_NULL(sc->gpiod_otg_en))
		gpiod_direction_output(sc->gpiod_otg_en, 0x0);

	sc89601_set_otg_vbus(sc, false);

	return 0;
}

static int sc89601_otg_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sc89601_device *sc = rdev_get_drvdata(dev);
	u8 val;

	val = sc89601_field_read(sc, F_OTG_CFG);

	return val;
}

static const struct regulator_ops sc89601_otg_vbus_ops = {
	.enable = sc89601_otg_vbus_enable,
	.disable = sc89601_otg_vbus_disable,
	.is_enabled = sc89601_otg_vbus_is_enabled,
};

static const struct regulator_desc sc89601_otg_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.regulators_node = of_match_ptr("regulators"),
	.owner = THIS_MODULE,
	.ops = &sc89601_otg_vbus_ops,
	.type = REGULATOR_VOLTAGE,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sc89601_register_otg_vbus_regulator(struct sc89601_device *sc)
{
	struct regulator_config config = { };
	struct device_node *np;

	np = of_get_child_by_name(sc->dev->of_node, "regulators");
	if (!np) {
		dev_warn(sc->dev, "cannot find regulators node\n");
		return 0;
	}

	sc->gpiod_otg_en = devm_gpiod_get_optional(sc->dev, "otg-en", GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(sc->gpiod_otg_en))
		dev_warn(sc->dev, "failed to request GPIO otg en pin\n");
	else
		gpiod_direction_output(sc->gpiod_otg_en, 0x0);

	sc89601_set_otg_vbus(sc, false);

	config.dev = sc->dev;
	config.driver_data = sc;

	sc->otg_vbus_reg = devm_regulator_register(sc->dev,
						   &sc89601_otg_vbus_desc,
						   &config);
	if (IS_ERR(sc->otg_vbus_reg))
		return PTR_ERR(sc->otg_vbus_reg);

	return 0;
}

static ssize_t registers_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct sc89601_device *sc89601 = dev_get_drvdata(dev);
	u8 tmpbuf[SC89601_DEBUG_BUF_LEN];
	int idx = 0;
	u8 addr;
	int val;
	int len;
	int ret;

	for (addr = 0x0; addr <= 0x0E; addr++) {
		ret = regmap_read(sc89601->rmap, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, SC89601_DEBUG_BUF_LEN,
					"Reg[%.2X] = 0x%.2x\n", addr, val);
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
	struct sc89601_device *sc89601 = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x0E)
		regmap_write(sc89601->rmap, (unsigned char)reg, val);

	return count;
}

static DEVICE_ATTR_RW(registers);

static void sc89601_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}

static int sc89601_fw_read_u32_props(struct sc89601_device *sc89601)
{
	struct sc89601_init_data *init = &sc89601->init_data;
	u32 property;
	int ret;
	int i;
	struct {
		char *name;
		bool optional;
		enum sc89601_table_ids tbl_id;
		u8 *conv_data; /* holds converted value from given property */
	} props[] = {
		/* required properties */
		{"sc,charge-current", false, TBL_ICHG, &init->ichg},
		{"sc,battery-regulation-voltage", false, TBL_VREG, &init->vreg},
		{"sc,termination-current", false, TBL_ITERM, &init->iterm},
		{"sc,precharge-current", false, TBL_ITERM, &init->iprechg},
		{"sc,minimum-sys-voltage", false, TBL_SYSVMIN, &init->sysvmin},
		{"sc,boost-voltage", false, TBL_BOOSTV, &init->boostv},
		{"sc,boost-max-current", false, TBL_BOOSTI, &init->boosti},
	};

	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = device_property_read_u32(sc89601->dev,
					       props[i].name,
					       &property);
		if (ret < 0) {
			if (props[i].optional)
				continue;

			dev_err(sc89601->dev, "Unable to read property %d %s\n", ret,
				props[i].name);

			return ret;
		}

		*props[i].conv_data = sc89601_find_idx(property,
						       props[i].tbl_id);
	}

	return 0;
}

static int sc89601_fw_probe(struct sc89601_device *sc89601)
{
	int ret;
	struct sc89601_init_data *init = &sc89601->init_data;

	ret = sc89601_fw_read_u32_props(sc89601);
	if (ret < 0)
		return ret;

	init->stat_pin_en = device_property_read_bool(sc89601->dev, "sc,use-stat-pin");
	init->boostf = device_property_read_bool(sc89601->dev, "sc,boost-low-freq");

	return 0;
}

static int sc89601_pd_notifier_call(struct notifier_block *nb,
				    unsigned long val, void *v)
{
	struct sc89601_device *sc89601 =
		container_of(nb, struct sc89601_device, nb);
	union power_supply_propval value;
	int voltage, index, ret;

	if (val != PSY_EVENT_PROP_CHANGED) {
		DBG("%s: unexpected psy prop %ld\n", __func__, val);
		return NOTIFY_OK;
	}

	if (!sc89601->tcpm_psy)
		return NOTIFY_OK;

	ret = power_supply_get_property(sc89601->tcpm_psy,
					POWER_SUPPLY_PROP_ONLINE,
					&value);
	if (ret)
		return NOTIFY_OK;

	if (!value.intval) {
		DBG("%s: discharger!!!\n", __func__);
		return NOTIFY_OK;
	}

	ret = power_supply_get_property(sc89601->tcpm_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW,
					&value);
	if (ret)
		return NOTIFY_OK;
	if (value.intval) {
		/* Set Voltage */
		voltage = value.intval;

		index = sc89601_find_idx(voltage, TBL_VINDPM);
		ret = sc89601_field_write(sc89601, F_VINDPM, index);
		if (ret < 0)
			dev_err(sc89601->dev, "set input voltage failed\n");

		DBG("%s: charger voltage = %d\n", __func__, value.intval);
		ret = power_supply_get_property(sc89601->tcpm_psy,
						POWER_SUPPLY_PROP_CURRENT_NOW,
						&value);
		if (ret)
			return NOTIFY_OK;
		if (!value.intval) {
			value.intval = 500000; /* 500mA */
			if (voltage == 5000000)
				DBG("%s: set safety 5V 500mA\n", __func__);
			else
				pr_warn("%s: no current found, set low current 500mA\n", __func__);
		}
		index = sc89601_find_idx(value.intval, TBL_IILIM);
		sc89601_field_write(sc89601, F_IILIM, index);
		DBG("%s: charger current = %d\n", __func__, value.intval);
	} else {
		pr_warn("%s: No Found voltage\n", __func__);
	}

	return NOTIFY_OK;
}

static void sc89601_charger_phandle_work(struct work_struct *data)
{
	struct sc89601_device *sc89601 =
		container_of(data, struct sc89601_device, charger_phandle_work.work);

	if (IS_ERR_OR_NULL(sc89601->tcpm_psy)) {
		sc89601->tcpm_psy =
			devm_power_supply_get_by_phandle(sc89601->dev, "charger-phandle");
		if (IS_ERR_OR_NULL(sc89601->tcpm_psy)) {
			pr_err("chargers-phandle is error\n");
			sc89601->vbus_flag = 0;
		} else {
			sc89601->vbus_flag = 1;
		}
	}
	if (!sc89601->vbus_flag) {
		queue_delayed_work(system_wq, &sc89601->charger_phandle_work,
				   msecs_to_jiffies(200));
	}
}

static int sc89601_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc89601_device *sc89601;
	int ret;

	sc89601 = devm_kzalloc(dev, sizeof(*sc89601), GFP_KERNEL);
	if (!sc89601)
		return -ENOMEM;

	sc89601->client = client;
	sc89601->dev = dev;

	mutex_init(&sc89601->lock);
	sc89601->rmap = devm_regmap_init_i2c(client, &sc89601_regmap_config);
	if (IS_ERR(sc89601->rmap)) {
		dev_err(dev, "failed to allocate register map\n");
		return PTR_ERR(sc89601->rmap);
	}

	ret = devm_regmap_field_bulk_alloc(dev, sc89601->rmap, sc89601->rmap_fields,
					   sc89601_reg_fields, ARRAY_SIZE(sc89601_reg_fields));
	if (ret) {
		dev_err(dev, "cannot bulk allocate regmap fields\n");
		return ret;
	}

	i2c_set_clientdata(client, sc89601);

	ret = sc89601_get_chip_version(sc89601);
	if (ret) {
		dev_err(dev, "Cannot read chip ID or unknown chip.\n");
		return ret;
	}

	ret = sc89601_power_supply_init(sc89601);
	if (ret < 0) {
		dev_err(dev, "Failed to register power supply\n");
		return ret;
	}
	if (!dev->platform_data) {
		ret = sc89601_fw_probe(sc89601);
		if (ret < 0) {
			dev_err(dev, "Cannot read device properties.\n");
			return ret;
		}
	} else {
		return -ENODEV;
	}
	ret = sc89601_hw_init(sc89601);
	if (ret < 0) {
		dev_err(dev, "Cannot initialize the chip.\n");
		return ret;
	}

	sc89601->nb.notifier_call = sc89601_pd_notifier_call;
	ret = power_supply_reg_notifier(&sc89601->nb);
	if (ret) {
		pr_err("failed to reg notifier: %d\n", ret);
		return ret;
	}

	if (client->irq < 0) {
		dev_err(dev, "No irq resource found.\n");
		ret = client->irq;
		goto err;
	}

	ret = devm_request_threaded_irq(dev, client->irq, NULL,
					sc89601_irq_handler_thread,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					SC89601_IRQ, sc89601);
	if (ret)
		goto err;
	ret = sc89601_register_otg_vbus_regulator(sc89601);
	if (ret)
		goto err;
	sc89601_create_device_node(sc89601->dev);

	INIT_DELAYED_WORK(&sc89601->charger_phandle_work, sc89601_charger_phandle_work);
	queue_delayed_work(system_wq, &sc89601->charger_phandle_work,
			   msecs_to_jiffies(3000));

	return 0;
err:
	power_supply_unreg_notifier(&sc89601->nb);
	return ret;
}

static void sc89601_remove(struct i2c_client *client)
{
	struct sc89601_device *sc89601 = i2c_get_clientdata(client);

	power_supply_unreg_notifier(&sc89601->nb);
	cancel_delayed_work_sync(&sc89601->charger_phandle_work);

	/* reset all registers to default values */
	sc89601_chip_reset(sc89601);
}

#ifdef CONFIG_PM_SLEEP
static int sc89601_suspend(struct device *dev)
{
	struct sc89601_device *sc89601 = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&sc89601->lock);
	ret = sc89601_get_chip_state(sc89601, &sc89601->state);
	mutex_unlock(&sc89601->lock);
	return ret;
}

static int sc89601_resume(struct device *dev)
{
	int ret;
	struct sc89601_device *sc89601 = dev_get_drvdata(dev);

	mutex_lock(&sc89601->lock);

	ret = sc89601_get_chip_state(sc89601, &sc89601->state);
	if (ret < 0)
		goto unlock;

	/* signal userspace, maybe state changed while suspended */
	power_supply_changed(sc89601->charger);

unlock:
	mutex_unlock(&sc89601->lock);

	return ret;
}
#endif

static const struct dev_pm_ops sc89601_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(sc89601_suspend, sc89601_resume)
};

static const struct i2c_device_id sc89601_i2c_ids[] = {
	{ "sc89601", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sc89601_i2c_ids);

static const struct of_device_id sc89601_of_match[] = {
	{ .compatible = "sc,sc89601", },
	{ },
};
MODULE_DEVICE_TABLE(of, sc89601_of_match);

#ifdef CONFIG_ACPI
static const struct acpi_device_id sc89601_acpi_match[] = {
	{"SC89601", 0},
	{},
};
MODULE_DEVICE_TABLE(acpi, sc89601_acpi_match);
#endif

static struct i2c_driver sc89601_driver = {
	.driver = {
		.name = "sc89601-charger",
		.of_match_table = of_match_ptr(sc89601_of_match),
		.acpi_match_table = ACPI_PTR(sc89601_acpi_match),
		.pm = &sc89601_pm,
	},
	.probe = sc89601_probe,
	.remove = sc89601_remove,
	.id_table = sc89601_i2c_ids,
};
module_i2c_driver(sc89601_driver);

MODULE_DESCRIPTION("SC SC89601 Charger Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("South Chip <boyu-wen@southchip.com>");
