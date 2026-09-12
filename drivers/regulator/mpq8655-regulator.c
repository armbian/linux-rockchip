// SPDX-License-Identifier: GPL-2.0
/*
 * Regulator driver for MPQ8655
 *
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 *
 * Author: Xu Shengfei <xsf@rock-chips.com>
 */

#include <linux/bits.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/gpio/consumer.h>

/* Debug switch, dynamically controllable via sysfs */
static bool mpq8655_dbg_enable;
module_param_named(dbg_enable, mpq8655_dbg_enable, bool, 0600);
MODULE_PARM_DESC(dbg_enable, "Enable MPQ8655 debug output (default: disabled)");

/*
 * Debug macro: mpq_dbg maps to dev_info when debug is enabled
 *
 * Usage:
 *   Enable debug:  echo 1 > /sys/module/mpq8655_regulator/parameters/dbg_enable
 *   Disable debug: echo 0 > /sys/module/mpq8655_regulator/parameters/dbg_enable
 */
#define mpq_dbg(dev, fmt, ...)						\
do {										\
	if (unlikely(mpq8655_dbg_enable))				\
		dev_info(dev, fmt, ##__VA_ARGS__);			\
} while (0)

/* MPQ8655 Register Address Definitions - Categorized by bit width */
/* 8-bit Registers */
#define MPQ8655_OPERATION			0x01	/* Operation control register */
#define MPQ8655_ON_OFF_CONFIG			0x02	/* Power on/off configuration */
#define MPQ8655_STATUS_BYTE			0x78	/* Status byte register */
#define MPQ8655_MFR_CTRL_VOUT			0xD1	/* Manufacturer voltage control */
#define MPQ8655_MFR_CTRL_OPS			0xD2	/* Manufacturer operations settings */
#define MPQ8655_MFR_OC_PHASE_SET		0xD7	/* Overcurrent phase setting */
#define MPQ8655_MFR_VOUT_STEP			0xDA	/* sets the slew rate of the VOUT */

/* 16-bit Registers */
#define MPQ8655_VOUT_COMMAND			0x21	/* Output voltage command register */
#define MPQ8655_VOUT_MAX			0x24	/* Maximum output voltage limit */
#define MPQ8655_VOUT_MARGIN_HIGH		0x25	/* Output voltage high margin */
#define MPQ8655_VOUT_MARGIN_LOW			0x26	/* Output voltage low margin */
#define MPQ8655_VOUT_SCALE_LOOP			0x29	/* Output voltage scaling factor */
#define MPQ8655_VOUT_MIN			0x2B	/* Minimum output voltage limit */
#define MPQ8655_IOUT_OC_FAULT_LIMIT		0x46	/* Output overcurrent fault limit */
#define MPQ8655_IOUT_OC_WARN_LIMIT		0x4A	/* Output overcurrent warning limit */
#define MPQ8655_OT_FAULT_LIMIT                  0x4F    /* Overtemperature fault limit */
#define MPQ8655_OT_WARN_LIMIT			0x51	/* Overtemperature warning limit */
#define MPQ8655_STATUS_WORD			0x79	/* Status word register */
#define MPQ8655_READ_VOUT			0x8B	/* Read output voltage value */
#define MPQ8655_READ_IOUT			0x8C	/* Read output current value */
#define MPQ8655_READ_TEMPERATURE		0x8D	/* Read temperature value */
#define MPQ8655_MFR_CTRL			0xEA	/* Phase / OSM / OCP hiccup control */

#define MPQ8655_MFR_ID				0x99	/* the company identification */

/* Operation command values */
#define MPQ8655_OP_IMMEDIATE_OFF		0x00
#define MPQ8655_OP_ON				0x80

/* Status bit definitions */
#define MPQ8655_STATUS_OFF			BIT(6)
#define MPQ8655_STATUS_VOUT_OV			BIT(5)
#define MPQ8655_STATUS_IOUT_OC			BIT(4)
#define MPQ8655_STATUS_VIN_UV			BIT(3)
#define MPQ8655_STATUS_OT			BIT(2)

/* Manufacturer control register bit definitions */
#define MPQ8655_VOUT_RANGE_MASK			GENMASK(1, 0)
#define MPQ8655_FREQ_MASK			GENMASK(2, 1)
#define MPQ8655_PS_MODE				BIT(0)

/* MFR_CTRL(EAh) bit definitions */
#define MPQ8655_PHASE_MULTI			BIT(3)

/* Default parameter settings */
#define MPQ8655_OT_FAULT_DEFAULT		145
#define MPQ8655_SWITCHING_FREQ_DEFAULT		800000

#define MPQ8655_VOUT_CMD_MAX			0x0FFF

/* Voltage range configuration table */
static const unsigned int mpq8655_voltage_ranges[][2] = {
	{400000, 672000},	/* VREF/VOUT = 1, 0.4V-0.672V */
	{400000, 1344000},	/* VREF/VOUT = 0.5, 0.4V-1.344V */
	{700000, 2688000},	/* VREF/VOUT = 0.25, 0.7V-2.688V */
	{1300000, 5376000},	/* VREF/VOUT = 0.125, 1.3V-5.376V */
};

/* Chip type enumeration */
enum chip_type {
	CHIP_MPQ8655 = 0,
	CHIP_SC812C0 = 1,
	CHIP_IS6608  = 2,
	CHIP_ES5330  = 3,
};

/* Device private data structure */
struct mpq8655_regulator {
	struct regulator_dev *rdev;
	struct regmap *regmap_8bit;	/* 8-bit register regmap */
	struct regmap *regmap_16bit;	/* 16-bit register regmap */
	struct device *dev;
	struct mutex lock;
	struct gpio_desc *enable_gpio;
	u8 vout_range;
	u8 ot_fault_limit;
	u32 switching_freq;

	/* Fields for DT parsed values */
	bool switching_freq_set;	/* True if DT provided switching frequency */
	bool ot_fault_limit_set;	/* True if DT provided overtemperature limit */
	bool multi_phase;		/* True if multi-phase mode (MFR_CTRL[3]) */

	/* Fields for external voltage divider support */
	bool use_external_div;		/* Flag: using external voltage divider */
	u32 kdiv_x1000;			/* External divider ratio * 1000 */
	u32 vref_step_adj_x1000;
	u32 init_uV;			/* Initial output voltage in microvolts */

	/* VOUT_COMMAND write interval tracking */
	ktime_t last_vcmd_time;		/* Timestamp of last successful write */
	u32 vcmd_interval_us;		/* Min interval between writes (128us) */
	int mfr_id;
};

/*
 * Volatile register detection for 8-bit register map.
 * These registers contain status or real-time values that must be read
 * from hardware every time, bypassing the regmap cache.
 */
static bool mpq8655_8bit_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MPQ8655_STATUS_BYTE:
	case MPQ8655_OPERATION:
		return true;
	default:
		return false;
	}
}

/*
 * Volatile register detection for 16-bit register map.
 * Most entries are telemetry or status values that must be read
 * from hardware every time, bypassing the regmap cache.
 *
 * VOUT_SCALE_LOOP is marked volatile for cross-chip compatibility:
 * some chips (e.g., SC812C0) have special behavior for this register
 * where writes may not take effect or are unsupported. Marking it
 * volatile ensures the driver always reads back the actual hardware
 * value rather than relying on potentially stale cached data.
 */
static bool mpq8655_16bit_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MPQ8655_READ_VOUT:
	case MPQ8655_READ_IOUT:
	case MPQ8655_READ_TEMPERATURE:
	case MPQ8655_VOUT_SCALE_LOOP:
	case MPQ8655_STATUS_WORD:
	case MPQ8655_MFR_CTRL:
		return true;
	default:
		return false;
	}
}

/* 8-bit register regmap configuration */
static const struct regmap_config mpq8655_regmap_8bit_config = {
	.name = "mpq8655-8bit",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = mpq8655_8bit_volatile_reg,
};

/* 16-bit register regmap configuration */
static const struct regmap_config mpq8655_regmap_16bit_config = {
	.name = "mpq8655-16bit",
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = 0xFF,
	.cache_type = REGCACHE_RBTREE,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,
	.volatile_reg = mpq8655_16bit_volatile_reg,
};

/* Register access validation functions */
static bool mpq8655_8bit_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MPQ8655_OPERATION:
	case MPQ8655_ON_OFF_CONFIG:
	case MPQ8655_STATUS_BYTE:
	case MPQ8655_MFR_CTRL_VOUT:
	case MPQ8655_MFR_CTRL_OPS:
	case MPQ8655_MFR_OC_PHASE_SET:
	case MPQ8655_MFR_VOUT_STEP:
		return true;
	default:
		return false;
	}
}

static bool mpq8655_16bit_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MPQ8655_VOUT_COMMAND:
	case MPQ8655_VOUT_MAX:
	case MPQ8655_VOUT_MARGIN_HIGH:
	case MPQ8655_VOUT_MARGIN_LOW:
	case MPQ8655_VOUT_SCALE_LOOP:
	case MPQ8655_VOUT_MIN:
	case MPQ8655_IOUT_OC_FAULT_LIMIT:
	case MPQ8655_IOUT_OC_WARN_LIMIT:
	case MPQ8655_OT_FAULT_LIMIT:
	case MPQ8655_OT_WARN_LIMIT:
	case MPQ8655_STATUS_WORD:
	case MPQ8655_READ_VOUT:
	case MPQ8655_READ_IOUT:
	case MPQ8655_READ_TEMPERATURE:
	case MPQ8655_MFR_CTRL:
		return true;
	default:
		return false;
	}
}

static bool mpq8655_8bit_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MPQ8655_OPERATION:
	case MPQ8655_ON_OFF_CONFIG:
	case MPQ8655_MFR_CTRL_VOUT:
	case MPQ8655_MFR_CTRL_OPS:
	case MPQ8655_MFR_OC_PHASE_SET:
	case MPQ8655_MFR_VOUT_STEP:
		return true;
	default:
		return false;
	}
}

static bool mpq8655_16bit_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MPQ8655_VOUT_COMMAND:
	case MPQ8655_VOUT_MAX:
	case MPQ8655_VOUT_MARGIN_HIGH:
	case MPQ8655_VOUT_MARGIN_LOW:
	case MPQ8655_VOUT_SCALE_LOOP:
	case MPQ8655_VOUT_MIN:
	case MPQ8655_IOUT_OC_FAULT_LIMIT:
	case MPQ8655_IOUT_OC_WARN_LIMIT:
	case MPQ8655_OT_FAULT_LIMIT:
	case MPQ8655_OT_WARN_LIMIT:
	case MPQ8655_MFR_CTRL:
		return true;
	default:
		return false;
	}
}

/* Smart register access function - automatically selects correct regmap */
static int mpq8655_reg_read(struct mpq8655_regulator *mpq8655, u8 reg, unsigned int *val)
{
	int ret;

	if (mpq8655_8bit_readable_reg(mpq8655->dev, reg)) {
		ret = regmap_read(mpq8655->regmap_8bit, reg, val);
		if (ret == 0) {
			mpq_dbg(mpq8655->dev,
				"Read 8-bit register 0x%02x: 0x%02x\n", reg, *val);
		}
	} else if (mpq8655_16bit_readable_reg(mpq8655->dev, reg)) {
		ret = regmap_read(mpq8655->regmap_16bit, reg, val);
		if (ret == 0) {
			mpq_dbg(mpq8655->dev,
				"Read 16-bit register 0x%02x: 0x%04x\n", reg, *val);
		}
	} else {
		dev_err(mpq8655->dev, "Invalid register address 0x%02x\n", reg);
		return -EINVAL;
	}

	if (ret < 0)
		dev_err(mpq8655->dev, "Failed to read register 0x%02x: %d\n", reg, ret);

	return ret;
}

static int mpq8655_reg_write(struct mpq8655_regulator *mpq8655, u8 reg, unsigned int val)
{
	int ret;

	if (mpq8655_8bit_writeable_reg(mpq8655->dev, reg)) {
		if (val > 0xFF) {
			dev_err(mpq8655->dev, "Value 0x%x exceeds 8-bit range\n", val);
			return -EINVAL;
		}
		ret = regmap_write(mpq8655->regmap_8bit, reg, val);
		if (ret == 0)
			mpq_dbg(mpq8655->dev, "Write 8-bit register 0x%02x: 0x%02x\n", reg, val);
	} else if (mpq8655_16bit_writeable_reg(mpq8655->dev, reg)) {
		if (val > 0xFFFF) {
			dev_err(mpq8655->dev, "Value 0x%x exceeds 16-bit range\n", val);
			return -EINVAL;
		}
		ret = regmap_write(mpq8655->regmap_16bit, reg, val);
		if (ret == 0)
			mpq_dbg(mpq8655->dev, "Write 16-bit register 0x%02x: 0x%04x\n", reg, val);
	} else {
		dev_err(mpq8655->dev, "Invalid or read-only register address 0x%02x\n", reg);
		return -EINVAL;
	}

	if (ret < 0)
		dev_err(mpq8655->dev, "Failed to write register 0x%02x: %d\n", reg, ret);

	return ret;
}

/* Get voltage range configuration */
static int mpq8655_get_voltage_range(struct mpq8655_regulator *mpq8655)
{
	unsigned int val;
	int ret;

	ret = mpq8655_reg_read(mpq8655, MPQ8655_MFR_CTRL_VOUT, &val);
	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to read voltage range configuration: %d\n", ret);
		return ret;
	}

	return val & MPQ8655_VOUT_RANGE_MASK;
}

/*
 * Set output voltage slew rate via MFR_VOUT_STEP(DAh).
 *
 * Framework passes ramp_delay in uV/us (microvolts per microsecond).
 * DAh register controls us/mV (microseconds per millivolt).
 *
 * Conversion:
 *   us_per_mV = 1000 / ramp_delay
 *   DAh_val = (us_per_mV - 10) / 2.5
 *
 * DTS configuration examples:
 *   regulator-ramp-delay = <100>;   100 uV/us = 10us/mV  = DAh=0x00
 *   regulator-ramp-delay = <50>;    50 uV/us  = 20us/mV  = DAh=0x04
 *   regulator-ramp-delay = <33>;    33 uV/us  = 30us/mV  = DAh=0x08
 *   regulator-ramp-delay = <25>;    25 uV/us  = 40us/mV  = DAh=0x0C
 *   regulator-ramp-delay = <21>;    21 uV/us  = 47.5us/mV = DAh=0x0F
 *
 * Note:
 *   - DAh cannot achieve faster than 10us/mV (ramp_delay > 100 uV/us).
 *     For faster ramp rates, framework will use software delay.
 *   - DAh cannot achieve slower than 47.5us/mV (ramp_delay < 22 uV/us).
 *     Values below 22 uV/us will be clamped to 0x0F.
 */
static int mpq8655_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	int us_per_mV, dah_val;
	int ret;

	/* Sanity check: ramp_delay must be positive */
	if (ramp_delay <= 0) {
		dev_err(mpq8655->dev, "Invalid ramp_delay: %d uV/us\n", ramp_delay);
		return -EINVAL;
	}

	/* Convert uV/us to us/mV */
	us_per_mV = DIV_ROUND_CLOSEST(1000, ramp_delay);

	/*
	 * Calculate DAh register value.
	 * DAh = (us_per_mV - 10) / 2.5
	 * Since we're dealing with integers, multiply numerator by 2:
	 *   dah_val * 2.5 = us_per_mV - 10
	 *   dah_val * 5 / 2 = us_per_mV - 10
	 *   dah_val = (us_per_mV - 10) * 2 / 5
	 */
	if (us_per_mV <= 10) {
		/* Fastest possible: 10us/mV or faster */
		dah_val = 0;
		mpq_dbg(mpq8655->dev,
			"ramp_delay %d uV/us (%d us/mV) exceeds HW capability, using fastest setting 10us/mV\n",
			ramp_delay, us_per_mV);
	} else {
		/* Clamp to valid range */
		dah_val = ((us_per_mV - 10) * 2 + 4) / 5;
		if (dah_val > 0x0F) {
			dah_val = 0x0F;
			mpq_dbg(mpq8655->dev,
				"ramp_delay %d uV/us (%d us/mV) exceeds HW range, using slowest setting 47.5us/mV\n",
				ramp_delay, us_per_mV);
		}
	}

	/* Write to MFR_VOUT_STEP(DAh) */
	mutex_lock(&mpq8655->lock);
	ret = mpq8655_reg_write(mpq8655, MPQ8655_MFR_VOUT_STEP, (u8)dah_val);
	mutex_unlock(&mpq8655->lock);

	if (ret < 0) {
		dev_err(mpq8655->dev,
			"Failed to set VOUT step slew rate: %d\n", ret);
		return ret;
	}

	mpq_dbg(mpq8655->dev,
		"Set ramp_delay: requested=%d uV/us, us_per_mV=%d, DAh=0x%x (%d us/mV)\n",
		ramp_delay, us_per_mV, dah_val,
		10 + dah_val * 5 / 2);

	return 0;
}

/* Set overtemperature fault limit */
static int mpq8655_set_ot_fault_limit(struct mpq8655_regulator *mpq8655, u8 temperature)
{
	int ret;

	if (temperature < 100 || temperature > 160) {
		dev_warn(mpq8655->dev,
			 "Overtemperature fault limit %d, out of range [100-160]\n",
			 temperature);
		temperature = clamp_val(temperature, 100, 160);
	}

	ret = mpq8655_reg_write(mpq8655, MPQ8655_OT_FAULT_LIMIT, temperature);
	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to set overtemperature fault limit: %d\n", ret);
		return ret;
	}

	mpq8655->ot_fault_limit = temperature;
	mpq_dbg(mpq8655->dev, "Overtemperature protection set to %d\n", temperature);

	return 0;
}

/* Set switching frequency */
static int mpq8655_set_switching_freq(struct mpq8655_regulator *mpq8655, u32 freq)
{
	unsigned int val, freq_val;
	int ret;

	/* Convert frequency to register value */
	if (freq <= 400000)
		freq_val = 0;
	else if (freq <= 600000)
		freq_val = 1;
	else if (freq <= 800000)
		freq_val = 2;
	else
		freq_val = 3;

	mutex_lock(&mpq8655->lock);
	/* Read current configuration */
	ret = mpq8655_reg_read(mpq8655, MPQ8655_MFR_CTRL_OPS, &val);
	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to read MPQ8655_MFR_CTRL_OPS: %d\n", ret);
		goto unlock;
	}
	/* Update frequency configuration bits */
	val &= ~MPQ8655_FREQ_MASK;
	val |= (freq_val << 1) & MPQ8655_FREQ_MASK;

	/* Write new configuration */
	ret = mpq8655_reg_write(mpq8655, MPQ8655_MFR_CTRL_OPS, val);
	if (ret == 0) {
		mpq8655->switching_freq = freq;
		mpq_dbg(mpq8655->dev, "Switching frequency set to %ukHz\n", freq / 1000);
	}
unlock:
	mutex_unlock(&mpq8655->lock);

	return ret;
}

/*
 * Enable multi-phase operation by setting MFR_CTRL(EAh) bit[3] (PHASE).
 *
 * MFR_CTRL bit[3] (PHASE), from the MPQ8655 datasheet:
 *   0: single-phase operation
 *   1: multi-phase operation
 *
 * The selection of this bit affects the actual ramp amplitude set through
 * MFR_CTRL_COMP(D0h) bits[3:1], so it has to be programmed before the output
 * voltage is changed (or before the output is enabled) to obtain the expected
 * slew rate behaviour.
 *
 * The register is read-modify-written so the OSM / OCP hiccup interval bits
 * stay untouched. Only the multi-phase bit is set; clearing the bit (to fall
 * back to single-phase) is intentionally not supported, because the DT uses
 * a boolean "mps,multi-phase" property whose absence already keeps the
 * hardware default (MTP / pin strapping).
 *
 * Note: enabling multi-phase also requires the phases to be paralleled and
 * phase-shifted in hardware (pin strapping / PCB design).
 */
static int mpq8655_enable_multi_phase(struct mpq8655_regulator *mpq8655)
{
	unsigned int val;
	int ret;

	mutex_lock(&mpq8655->lock);

	/* Read-modify-write: keep OSM / OCP hiccup interval bits untouched. */
	ret = mpq8655_reg_read(mpq8655, MPQ8655_MFR_CTRL, &val);
	if (ret < 0)
		goto unlock;

	val |= MPQ8655_PHASE_MULTI;

	ret = mpq8655_reg_write(mpq8655, MPQ8655_MFR_CTRL, val);
	if (ret < 0)
		goto unlock;

	mpq_dbg(mpq8655->dev, "MFR_CTRL(EAh) phase set to multi-phase (0x%04x)\n", val);

unlock:
	mutex_unlock(&mpq8655->lock);

	return ret;
}

/*
 * Get enable time (time required for regulator to stabilize after enable).
 * Value comes from DT property regulator-enable-ramp-delay, or defaults to 2000us.
 */
static int mpq8655_enable_time(struct regulator_dev *rdev)
{
	/*
	 * Return the enable ramp delay from DT if specified,
	 * otherwise use the hardware default soft-start time (2ms).
	 */
	if (rdev->constraints && rdev->constraints->enable_time)
		return rdev->constraints->enable_time;

	return 2000;  /* Default TON_RISE: 2ms */
}

/* Get current voltage selector value */
static int mpq8655_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	mutex_lock(&mpq8655->lock);
	ret = mpq8655_reg_read(mpq8655, MPQ8655_VOUT_COMMAND, &val);
	mutex_unlock(&mpq8655->lock);

	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to read output voltage: %d\n", ret);
		return ret;
	}

	/*
	 * VOUT_COMMAND value is returned directly as the selector.
	 * Each LSB corresponds to desc->uV_step (typically 2mV
	 * when VOUT_SCALE_LOOP matches the divider ratio).
	 */
	return val & rdev->desc->vsel_mask;
}

/*
 * Set voltage selector value.
 *
 * Vendor requirement: minimum 128us interval between consecutive
 * VOUT_COMMAND writes. Writing within 128us after the previous
 * write may cause the new command to be discarded by the hardware.
 */
static int mpq8655_set_voltage_sel(struct regulator_dev *rdev, unsigned int sel)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	ktime_t now, min_next;
	s64 remaining_us;
	int ret;

	/* Validate voltage selector range */
	if (sel > MPQ8655_VOUT_CMD_MAX) {
		dev_err(mpq8655->dev,
			"Voltage selector 0x%x exceeds 12-bit range\n", sel);
		return -EINVAL;
	}

	mutex_lock(&mpq8655->lock);

	/*
	 * Enforce minimum VOUT_COMMAND write interval (128us).
	 * Busy wait in a single critical section without releasing the mutex.
	 * 128us is short enough that this is acceptable.
	 */
	if ((mpq8655->mfr_id == CHIP_SC812C0) && mpq8655->vcmd_interval_us) {
		now = ktime_get_boottime();
		min_next = ktime_add_us(mpq8655->last_vcmd_time,
					mpq8655->vcmd_interval_us);

		remaining_us = ktime_us_delta(min_next, now);
		if (remaining_us > 0) {
			mpq_dbg(mpq8655->dev,
				"VOUT_CMD interval guard: delay %lld us (sel=%u)\n",
				remaining_us, sel);
			udelay((unsigned long)remaining_us);
		}
	}

	/* Write new voltage to VOUT_COMMAND register */
	ret = mpq8655_reg_write(mpq8655, MPQ8655_VOUT_COMMAND, sel);
	if (ret == 0)
		mpq8655->last_vcmd_time = ktime_get_boottime();

	mutex_unlock(&mpq8655->lock);

	if (ret < 0) {
		dev_err(mpq8655->dev,
			"Failed to set output voltage: %d\n", ret);
		return ret;
	}

	mpq_dbg(mpq8655->dev,
		"Output voltage set to sel=%u (0x%04x)\n", sel, sel);

	return 0;
}

/* Get current overcurrent fault limit */
static int mpq8655_get_current_limit(struct regulator_dev *rdev)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	mutex_lock(&mpq8655->lock);
	ret = mpq8655_reg_read(mpq8655, MPQ8655_IOUT_OC_FAULT_LIMIT, &val);
	mutex_unlock(&mpq8655->lock);

	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to read overcurrent limit: %d\n", ret);
		return ret;
	}

	return val * 250000;
}

/* Set output current limit */
static int mpq8655_set_current_limit(struct regulator_dev *rdev, int min_uA, int max_uA)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	/* IOUT_OC_FAULT_LIMIT uses 250mA increments */
	val = max_uA / 250000;
	/* Validate that min_uA is achievable with this step */
	if (min_uA > val * 250000) {
		dev_err(mpq8655->dev,
			 "Requested min current %d uA exceeds achievable value %u uA (sel=%u)\n",
			 min_uA, val * 250000, val);
		return -EINVAL;
	}

	mutex_lock(&mpq8655->lock);
	ret = mpq8655_reg_write(mpq8655, MPQ8655_IOUT_OC_FAULT_LIMIT, val);
	mutex_unlock(&mpq8655->lock);

	if (ret < 0)
		dev_err(mpq8655->dev, "Failed to set overcurrent limit: %d\n", ret);
	else
		mpq_dbg(mpq8655->dev, "Overcurrent limit set to %umA\n", val * 250);

	return ret;
}

/* Enable regulator */
static int mpq8655_enable(struct regulator_dev *rdev)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	int ret;

	mutex_lock(&mpq8655->lock);

	/* Enable GPIO if available */
	if (mpq8655->enable_gpio) {
		gpiod_set_value_cansleep(mpq8655->enable_gpio, 1);
		usleep_range(1000, 2000); /* 1-2ms delay for power-up */
	}

	ret = mpq8655_reg_write(mpq8655, MPQ8655_OPERATION, MPQ8655_OP_ON);
	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to enable regulator: %d\n", ret);
		if (mpq8655->enable_gpio)
			gpiod_set_value_cansleep(mpq8655->enable_gpio, 0);
	} else {
		mpq_dbg(mpq8655->dev, "Regulator enabled successfully\n");
	}

	mutex_unlock(&mpq8655->lock);

	return ret;
}

/* Disable regulator */
static int mpq8655_disable(struct regulator_dev *rdev)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	int ret;

	mutex_lock(&mpq8655->lock);

	ret = mpq8655_reg_write(mpq8655, MPQ8655_OPERATION, MPQ8655_OP_IMMEDIATE_OFF);
	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to disable regulator: %d\n", ret);
		mutex_unlock(&mpq8655->lock);
		return ret;
	}

	/* Disable GPIO if available */
	if (mpq8655->enable_gpio)
		gpiod_set_value_cansleep(mpq8655->enable_gpio, 0);

	mutex_unlock(&mpq8655->lock);

	mpq_dbg(mpq8655->dev, "Regulator disabled successfully\n");
	return 0;
}

/* Check if regulator is enabled */
static int mpq8655_is_enabled(struct regulator_dev *rdev)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	mutex_lock(&mpq8655->lock);

	/* Check enable GPIO first */
	if (mpq8655->enable_gpio && !gpiod_get_value_cansleep(mpq8655->enable_gpio)) {
		mutex_unlock(&mpq8655->lock);
		return 0;
	}

	ret = mpq8655_reg_read(mpq8655, MPQ8655_OPERATION, &val);

	mutex_unlock(&mpq8655->lock);

	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to read enable status: %d\n", ret);
		return ret;
	}

	return (val & MPQ8655_OP_ON) ? 1 : 0;
}

/* Get regulator status */
static int mpq8655_get_status(struct regulator_dev *rdev)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	mutex_lock(&mpq8655->lock);
	ret = mpq8655_reg_read(mpq8655, MPQ8655_STATUS_BYTE, &val);
	mutex_unlock(&mpq8655->lock);

	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to read status register: %d\n", ret);
		return ret;
	}

	if (val & MPQ8655_STATUS_OFF)
		return REGULATOR_STATUS_OFF;

	if (val & (MPQ8655_STATUS_VOUT_OV | MPQ8655_STATUS_IOUT_OC |
		   MPQ8655_STATUS_VIN_UV | MPQ8655_STATUS_OT))
		return REGULATOR_STATUS_ERROR;

	return REGULATOR_STATUS_ON;
}

/* Get current operation mode */
static unsigned int mpq8655_get_mode(struct regulator_dev *rdev)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	mutex_lock(&mpq8655->lock);
	ret = mpq8655_reg_read(mpq8655, MPQ8655_MFR_CTRL_OPS, &val);
	mutex_unlock(&mpq8655->lock);

	if (ret < 0) {
		dev_err(mpq8655->dev, "Failed to read operation mode: %d\n", ret);
		return REGULATOR_MODE_INVALID;
	}

	return (val & MPQ8655_PS_MODE) ? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

/* Set operation mode */
static int mpq8655_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct mpq8655_regulator *mpq8655 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	mutex_lock(&mpq8655->lock);

	/* Read current configuration */
	ret = mpq8655_reg_read(mpq8655, MPQ8655_MFR_CTRL_OPS, &val);
	if (ret < 0)
		goto unlock;

	/* Set PS_MODE bit based on operation mode */
	switch (mode) {
	case REGULATOR_MODE_FAST:
		/* Forced Continuous Conduction Mode (FCCM) */
		val |= MPQ8655_PS_MODE;
		break;
	case REGULATOR_MODE_NORMAL:
		/* Pulse Skip Mode (PSM) */
		val &= ~MPQ8655_PS_MODE;
		break;
	default:
		ret = -EINVAL;
		goto unlock;
	}

	/* Write new configuration */
	ret = mpq8655_reg_write(mpq8655, MPQ8655_MFR_CTRL_OPS, val);

unlock:
	mutex_unlock(&mpq8655->lock);
	return ret;
}

/* regulator_ops operations structure */
static const struct regulator_ops mpq8655_regulator_ops = {
	.enable = mpq8655_enable,
	.disable = mpq8655_disable,
	.is_enabled = mpq8655_is_enabled,
	.get_voltage_sel = mpq8655_get_voltage_sel,
	.set_voltage_sel = mpq8655_set_voltage_sel,
	.list_voltage = regulator_list_voltage_linear,
	.get_current_limit = mpq8655_get_current_limit,
	.set_current_limit = mpq8655_set_current_limit,
	.set_ramp_delay = mpq8655_set_ramp_delay,
	.get_status = mpq8655_get_status,
	.set_mode = mpq8655_set_mode,
	.get_mode = mpq8655_get_mode,
	.enable_time = mpq8655_enable_time,
};

static int mpq8655_parse_dt(struct device *dev, struct mpq8655_regulator *mpq8655)
{
	struct device_node *np = dev->of_node;
	u32 freq, temp, rfb1, rfb2, init_uV;
	int ret;

	/*
	 * Parse switching frequency configuration.
	 * Unit: Hz. Example: 800000 = 800kHz.
	 */
	if (!of_property_read_u32(np, "mps,switching-frequency-hz", &freq)) {
		mpq8655->switching_freq = freq;
		mpq8655->switching_freq_set = true;
		mpq_dbg(dev, "Switching frequency from DT: %u Hz (%u kHz)\n",
			freq, freq / 1000);
	} else {
		mpq8655->switching_freq_set = false;
		mpq_dbg(dev, "No switching frequency in DT, will use default\n");
	}

	/*
	 * Parse overtemperature protection point.
	 * Unit: Celsius. Example: 145 = 145C.
	 */
	if (!of_property_read_u32(np, "mps,overtemperature-limit-celsius", &temp)) {
		if (temp < 100 || temp > 160) {
			dev_warn(dev,
				 "Overtemperature limit %u out of range [100-160], clamping\n",
				 temp);
			temp = clamp_val(temp, 100, 160);
		}
		mpq8655->ot_fault_limit = (u8)temp;
		mpq8655->ot_fault_limit_set = true;
		mpq_dbg(dev, "Overtemperature limit from DT: %u C\n", temp);
	} else {
		mpq8655->ot_fault_limit_set = false;
		mpq_dbg(dev, "No overtemperature limit in DT, will use default\n");
	}

	/* Parse enable GPIO */
	mpq8655->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(mpq8655->enable_gpio)) {
		ret = PTR_ERR(mpq8655->enable_gpio);
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_warn(dev, "Failed to get enable GPIO: %d\n", ret);
		mpq8655->enable_gpio = NULL;
	} else if (mpq8655->enable_gpio) {
		mpq_dbg(dev, "Enable GPIO configured successfully\n");
	}

	/*
	 * Check for external voltage divider configuration.
	 *
	 * When external divider is used, the following properties are REQUIRED:
	 *   1. mps,external-voltage-divider - boolean flag to enable
	 *   2. mps,rfp1-ohms and mps,rfp2-ohms - resistor values
	 *   3. regulator-init-microvolt - default output voltage (from regulator subnode)
	 */
	mpq8655->use_external_div = false;
	mpq8655->kdiv_x1000 = 1000; /* Default: Kdiv = 1.0 (no division) */
	mpq8655->init_uV = 0;       /* Default: no initial voltage set */

	if (of_property_read_bool(np, "mps,external-voltage-divider")) {
		/*
		 * External divider mode requires resistor values.
		 * Both mps,rfp1-ohms and mps,rfp2-ohms must be specified.
		 */
		if (of_property_read_u32(np, "mps,rfp1-ohms", &rfb1) ||
		    of_property_read_u32(np, "mps,rfp2-ohms", &rfb2)) {
			dev_err(dev, "External voltage divider requires both mps,rfp1-ohms and mps,rfp2-ohms properties\n");
			return -EINVAL;
		}

		if (rfb1 == 0 || rfb2 == 0) {
			dev_err(dev, "Invalid external divider resistor values (RFB1=%u, RFB2=%u), both must be non-zero\n",
				rfb1, rfb2);
			return -EINVAL;
		}

		/*
		 * Parse regulator-init-microvolt from the regulators subnode.
		 *
		 * Why we need this here (before hw_init):
		 *
		 * 1. U-Boot regulator framework compatibility:
		 *    U-Boot's regulator framework reads regulator-init-microvolt from the
		 *    regulator subnode and uses it to set the initial voltage. We must
		 *    replicate this same behavior in the kernel driver to ensure consistent
		 *    voltage configuration across boot stages.
		 *
		 * 2. MPQ8655 External Voltage Divider hardware requirements:
		 *    When using an external voltage divider (D1h bits[1:0] = 2'b00),
		 *    the initialization sequence MUST follow this exact order per datasheet
		 *    (Section "VOUT Set by External Resistor Divider"):
		 *
		 *    Step 1: Set D1h bits[1:0] = 2'b00 (external voltage divider mode)
		 *    Step 2: Set VOUT_SCALE_LOOP (29h) to match the external divider ratio
		 *            (VOUT_SCALE_LOOP = RFB2 / (RFB1 + RFB2), 1LSB = 0.001)
		 *    Step 3: Set VOUT_COMMAND (21h) to the desired output voltage
		 *            (VOUT_COMMAND = VFB / 2mV, where VFB = target VOUT * VOUT_SCALE_LOOP)
		 *
		 *    CRITICAL: "Send VOUT_COMMAND (21h) after VOUT_SCALE_LOOP (29h) and D1h.
		 *    Otherwise, changing the settings of VOUT_SCALE_LOOP (29h) and D1h
		 *    is not effective." -- MPQ8655 Datasheet
		 *
		 *    Therefore, we must parse regulator-init-microvolt NOW so that hw_init()
		 *    can execute the complete initialization sequence in the correct order
		 *    before the regulator framework registers the device.
		 */
		struct device_node *regulators_node __free(device_node) =
			of_get_child_by_name(np, "regulators");
		if (!regulators_node) {
			dev_err(dev, "Missing 'regulators' subnode\n");
			return -EINVAL;
		}

		struct device_node *regulator_node __free(device_node) =
			of_get_child_by_name(regulators_node, "regulator");
		if (!regulator_node) {
			dev_err(dev, "Missing 'regulator' node under 'regulators'\n");
			return -EINVAL;
		}

		if (of_property_read_u32(regulator_node, "regulator-init-microvolt", &init_uV)) {
			dev_err(dev, "External voltage divider requires regulator-init-microvolt in regulator subnode\n");
			return -EINVAL;
		}

		if (init_uV == 0) {
			dev_err(dev, "regulator-init-microvolt must be non-zero\n");
			return -EINVAL;
		}

		mpq8655->use_external_div = true;
		mpq8655->init_uV = init_uV;

		/*
		 * Calculate external divider ratio Kdiv = RFB2 / (RFB1 + RFB2)
		 * Stored as integer with 1000 multiplier for precision.
		 * Kdiv_x1000 = RFB2 * 1000 / (RFB1 + RFB2)
		 */
		mpq8655->kdiv_x1000 = rfb2 * 1000 / (rfb1 + rfb2);
		if (mpq8655->kdiv_x1000 == 0) {
			dev_err(dev, "Invalid external divider resistor values (RFB1=%u, RFB2=%u), kdiv_x1000 must be non-zero\n",
				rfb1, rfb2);
			return -EINVAL;
		}
		mpq_dbg(dev, "External voltage divider enabled:\n"
			"  RFB1=%u Ohm, RFB2=%u Ohm\n"
			"  Kdiv=%u.%03u (ratio=%u/%u)\n"
			"  Initial VOUT=%u uV (%u mV)\n",
			rfb1, rfb2,
			mpq8655->kdiv_x1000 / 1000,
			mpq8655->kdiv_x1000 % 1000,
			rfb2, rfb1 + rfb2,
			init_uV, init_uV / 1000);
	} /* regulators_node and regulator_node are auto-freed here */

	/*
	 * Parse the operation phase configuration (MFR_CTRL(EAh) bit[3]).
	 *
	 *   mps,multi-phase (boolean) -> multi-phase  (bit[3] = 1)
	 *   absent                     -> keep hardware default (MTP / pin strapping)
	 *
	 * When the property is absent, MFR_CTRL(EAh) is left untouched and
	 * the hardware default (MTP / pin strapping) is kept, so that existing
	 * boards and pin-compatible second sources (SC812C0 / IS6608 / ES5330)
	 * that may not implement this register are not affected.
	 */
	mpq8655->multi_phase = of_property_read_bool(np, "mps,multi-phase");

	/* Set default VOUT_COMMAND write interval (128us per vendor requirement) */
	mpq8655->vcmd_interval_us = 128;
	if (of_property_read_u32(np, "mps,vout-cmd-interval-us",
				 &mpq8655->vcmd_interval_us)) {
		mpq_dbg(dev,
			"No mps,vout-cmd-interval-us in DT, using default %uus\n",
			mpq8655->vcmd_interval_us);
	}

	return 0;
}

/* Hardware initialization function */
static int mpq8655_hw_init(struct mpq8655_regulator *mpq8655)
{
	int ret;

	mpq8655->vref_step_adj_x1000 = 1000;

	/*
	 * Step 1: Set overtemperature protection point.
	 * Use DT value if provided, otherwise use default.
	 */
	if (mpq8655->ot_fault_limit_set) {
		ret = mpq8655_set_ot_fault_limit(mpq8655, mpq8655->ot_fault_limit);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to set DT overtemperature limit %d: %d\n",
				mpq8655->ot_fault_limit, ret);
			return ret;
		}
	} else {
		ret = mpq8655_set_ot_fault_limit(mpq8655, MPQ8655_OT_FAULT_DEFAULT);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to set default overtemperature limit: %d\n", ret);
			return ret;
		}
	}

	/*
	 * Step 2: Set switching frequency.
	 * Use DT value if provided, otherwise use default.
	 */
	if (mpq8655->switching_freq_set) {
		ret = mpq8655_set_switching_freq(mpq8655, mpq8655->switching_freq);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to set DT switching frequency %u Hz: %d\n",
				mpq8655->switching_freq, ret);
			return ret;
		}
	} else {
		ret = mpq8655_set_switching_freq(mpq8655, MPQ8655_SWITCHING_FREQ_DEFAULT);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to set default switching frequency: %d\n", ret);
			return ret;
		}
	}

	/*
	 * Step 3: Configure multi-phase operation.
	 *
	 * MFR_CTRL(EAh) bit[3] selects the ramp amplitude applied from
	 * MFR_CTRL_COMP(D0h) bits[3:1], therefore it must be programmed
	 * before VOUT_COMMAND(21h) is written below, so that the initial
	 * output voltage transition uses the correct ramp setting.
	 *
	 * The bit is only set when the DT requests multi-phase operation
	 * (mps,multi-phase), otherwise the hardware default (MTP / pin
	 * strapping) is kept.
	 */
	if (mpq8655->multi_phase) {
		ret = mpq8655_enable_multi_phase(mpq8655);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to configure multi-phase mode: %d\n", ret);
			return ret;
		}
	}

	/* Step 4: Configure external voltage divider if used. */
	if (mpq8655->use_external_div) {
		unsigned int d1_val;
		unsigned int vout_scale_loop;
		u16 vout_command;

		/*
		 * Step 4a: Read-modify-write MFR_CTRL_VOUT(D1h).
		 * Clear bits[1:0] to select external voltage divider mode (2'b00).
		 * Keep other bits unchanged (VOUT_DISCHARGE, PG_DELAY).
		 */
		ret = mpq8655_reg_read(mpq8655, MPQ8655_MFR_CTRL_VOUT, &d1_val);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to read MFR_CTRL_VOUT: %d\n", ret);
			return ret;
		}

		d1_val &= ~MPQ8655_VOUT_RANGE_MASK;  /* Clear bits[1:0] */
		/* bits[1:0] = 2'b00: External voltage divider mode */

		ret = mpq8655_reg_write(mpq8655, MPQ8655_MFR_CTRL_VOUT, d1_val);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to set external divider mode: %d\n", ret);
			return ret;
		}

		/*
		 * Step 4b: Set VOUT_SCALE_LOOP(29h) to match external divider ratio.
		 *
		 * VOUT_SCALE_LOOP = RFB2 / (RFB1 + RFB2)
		 * The value is stored as integer with 1LSB = 0.001.
		 *
		 * The ratio was already calculated during DT parsing as:
		 *   mpq8655->kdiv_x1000 = RFB2 * 1000 / (RFB1 + RFB2)
		 *
		 * So VOUT_SCALE_LOOP = kdiv_x1000 (already in 0.001 units)
		 */
		vout_scale_loop = (u16)mpq8655->kdiv_x1000;

		ret = mpq8655_reg_write(mpq8655, MPQ8655_VOUT_SCALE_LOOP,
					vout_scale_loop);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to set VOUT_SCALE_LOOP: %d\n", ret);
			return ret;
		}

		mpq_dbg(mpq8655->dev,
			"VOUT_SCALE_LOOP set to 0x%04x (ratio=%u.%03u)\n",
			vout_scale_loop,
			vout_scale_loop / 1000,
			vout_scale_loop % 1000);

		ret = mpq8655_reg_read(mpq8655, MPQ8655_VOUT_SCALE_LOOP,
				       &vout_scale_loop);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to verify VOUT_SCALE_LOOP: %d\n", ret);
			return ret;
		}

		/*
		 * Guard against division by zero: vout_scale_loop must be > 0.
		 * If hardware returns 0, use kdiv_x1000 as fallback (no scaling adjustment).
		 */
		if (vout_scale_loop == 0) {
			dev_warn(mpq8655->dev,
				 "VOUT_SCALE_LOOP readback is 0, using Kdiv=%u as fallback\n",
				 mpq8655->kdiv_x1000);
			vout_scale_loop = mpq8655->kdiv_x1000;
		}

		mpq8655->vref_step_adj_x1000 =
			1000 * vout_scale_loop / mpq8655->kdiv_x1000;

		/*
		 * Step 4c: Set VOUT_COMMAND(21h) to the initial voltage.
		 *
		 * VOUT_COMMAND = VREF / 2mV
		 *              = VOUT * VOUT_SCALE_LOOP / 2mV
		 *              = init_uV * kdiv_x1000 / (vout_scale_loop * 2000)
		 */
		vout_command = (u16)(mpq8655->init_uV *
			mpq8655->kdiv_x1000 / vout_scale_loop / 2000);

		/* Clamp to valid range (0-4095 for 12-bit VOUT_COMMAND) */
		if (vout_command > MPQ8655_VOUT_CMD_MAX) {
			dev_warn(mpq8655->dev,
				 "Calculated VOUT_COMMAND 0x%04x exceeds 12-bit range, clamping to 0x0FFF\n",
				 vout_command);
			vout_command = MPQ8655_VOUT_CMD_MAX;
		}

		ret = mpq8655_reg_write(mpq8655, MPQ8655_VOUT_COMMAND, vout_command);
		if (ret < 0) {
			dev_err(mpq8655->dev,
				"Failed to set initial VOUT_COMMAND: %d\n", ret);
			return ret;
		}

		mpq_dbg(mpq8655->dev,
			"Initial voltage set: VOUT_COMMAND=0x%04x (%u)\n"
			"  Target VOUT=%u uV (%u mV)\n"
			"  VREF=%u uV (%u mV)\n",
			vout_command, vout_command,
			mpq8655->init_uV, mpq8655->init_uV / 1000,
			(u32)vout_command * 2000, (u32)vout_command * 2);
	} else {
		/*
		 * Internal divider mode: MFR_CTRL_VOUT bits[1:0] are read from
		 * hardware (configured via MTP or pin strapping).
		 * VOUT_SCALE_LOOP should match the internal divider setting.
		 *
		 * Table 1 in datasheet shows the mapping:
		 *   bits[1:0] = 2'b00 -> VOUT_SCALE_LOOP = 0x03E8 (1.0)
		 *   bits[1:0] = 2'b01 -> VOUT_SCALE_LOOP = 0x01F4 (0.5)
		 *   bits[1:0] = 2'b10 -> VOUT_SCALE_LOOP = 0x00FA (0.25)
		 *   bits[1:0] = 2'b11 -> VOUT_SCALE_LOOP = 0x007D (0.125)
		 *
		 * For internal divider, we trust the hardware default/MTP settings
		 * and don't override them here.
		 */
		mpq_dbg(mpq8655->dev,
			"Using internal voltage divider (range %d)\n",
			mpq8655->vout_range);
	}

	mpq8655->last_vcmd_time = ktime_set(0, 0);  /* Allow first write immediately */

	mpq_dbg(mpq8655->dev, "Hardware initialization completed successfully\n");
	return 0;
}

/**
 * mpq8655_identify_chip - Identify the chip model (SC812C0, MPQ8655, or IS6608)
 * @client: I2C client device
 *
 * Reads MFR_ID (0x99) via i2c_smbus_read_i2c_block_data and identifies
 * the manufacturer based on Character1 (buf[1]) and Character2 (buf[2]).
 *
 * SC812C0 (Southchip): buf[1]='S'(0x53), buf[2]='C'(0x43)
 * MPQ8655 (MPS):       buf[1]='M'(0x4D), buf[2]='P'(0x50)
 * IS6608 (iSoftStone): buf[1]='I'(0x49), buf[2]='S'(0x53)
 *
 * Returns: chip_type enum on success, negative errno on failure.
 */
static int mpq8655_identify_manufacturer(struct i2c_client *client)
{
	u8 buf[4] = {0};
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, MPQ8655_MFR_ID, 4, buf);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read MFR_ID: %d\n", ret);
		return ret;
	}

	/*
	 * Verify that we received enough bytes to identify the chip.
	 * MFR_ID contains 4 characters; we need at least 3 bytes
	 * (Character0, Character1, Character2) for reliable identification.
	 *
	 * If fewer than 3 bytes were read, buf[1] and buf[2] remain 0
	 * from initialization, which would incorrectly match ES5330.
	 */
	if (ret < 3) {
		dev_err(&client->dev,
			"MFR_ID read returned only %d bytes, need at least 3\n", ret);
		return -ENODATA;
	}

	/*
	 * Identify manufacturer using only Character1 (buf[1]) and Character2 (buf[2])
	 * SC812C0: 'S'(0x53), 'C'(0x43)
	 * MPQ8655: 'M'(0x4D), 'P'(0x50)
	 * IS6608:  'I'(0x49), 'S'(0x53)
	 * ES5330:   0
	 */
	if (buf[1] == 0x53 && buf[2] == 0x43) {
		dev_info(&client->dev, "Detected Southchip SC812C0\n");
		return CHIP_SC812C0;
	} else if (buf[1] == 0x4D && buf[2] == 0x50) {
		dev_info(&client->dev, "Detected MPS MPQ8655\n");
		return CHIP_MPQ8655;
	} else if (buf[1] == 0x49 && buf[2] == 0x53) {
		dev_info(&client->dev, "Detected iSoftStone IS6608\n");
		return CHIP_IS6608;
	} else if (buf[1] == 0x00 && buf[2] == 0x00) {
		dev_info(&client->dev, "Detected EINNO ES5330\n");
		return CHIP_ES5330;
	}

	dev_err(&client->dev,
		"Unknown MFR_ID: %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3]);
	return -ENODEV;
}

static int mpq8655_regulator_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct regulator_config config = {};
	struct mpq8655_regulator *mpq8655;
	struct regulator_desc *desc;
	struct regulator_dev *rdev;
	u32 kdiv, min_uV, max_uV;
	u32 dummy_val;
	int mfr_id, range, ret;

	mfr_id = mpq8655_identify_manufacturer(client);
	if (mfr_id < 0) {
		dev_err(dev, "Failed to identify manufacturer chip: %d\n", mfr_id);
		return mfr_id;
	}

	/* Step 1: Allocate device private data structure. */
	mpq8655 = devm_kzalloc(dev, sizeof(*mpq8655), GFP_KERNEL);
	if (!mpq8655)
		return -ENOMEM;

	mpq8655->dev = dev;
	mpq8655->mfr_id = mfr_id;
	mutex_init(&mpq8655->lock);

	/* Step 2: Initialize regmap interfaces. */
	mpq8655->regmap_8bit = devm_regmap_init_i2c(client,
						    &mpq8655_regmap_8bit_config);
	if (IS_ERR(mpq8655->regmap_8bit)) {
		ret = PTR_ERR(mpq8655->regmap_8bit);
		dev_err(dev, "8-bit regmap initialization failed: %d\n", ret);
		return ret;
	}

	mpq8655->regmap_16bit = devm_regmap_init_i2c(client,
						     &mpq8655_regmap_16bit_config);
	if (IS_ERR(mpq8655->regmap_16bit)) {
		ret = PTR_ERR(mpq8655->regmap_16bit);
		dev_err(dev, "16-bit regmap initialization failed: %d\n", ret);
		return ret;
	}

	/* Step 3: Verify device communication. */
	ret = mpq8655_reg_read(mpq8655, MPQ8655_OPERATION, &dummy_val);
	if (ret < 0) {
		dev_err(dev, "Failed to communicate with MPQ8655: %d\n", ret);
		return ret;
	}

	/* Step 4: Parse device tree configuration. */
	ret = mpq8655_parse_dt(dev, mpq8655);
	if (ret < 0) {
		dev_err(dev, "Device tree parsing failed: %d\n", ret);
		return ret;
	}

	/* Step 5: Perform hardware initialization. */
	ret = mpq8655_hw_init(mpq8655);
	if (ret < 0) {
		dev_err(dev, "Hardware initialization failed: %d\n", ret);
		return ret;
	}

	/* Step 6: Get voltage range configuration from hardware. */
	range = mpq8655_get_voltage_range(mpq8655);
	if (range < 0) {
		dev_err(dev, "Failed to get voltage range: %d\n", range);
		return range;
	}

	if (range >= ARRAY_SIZE(mpq8655_voltage_ranges)) {
		dev_err(dev, "Invalid voltage range: %d\n", range);
		return -EINVAL;
	}

	mpq8655->vout_range = range;

	/* Step 7: Allocate and configure regulator descriptor. */
	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->name = "mpq8655-regulator";
	desc->id = 0;
	desc->ops = &mpq8655_regulator_ops;
	desc->type = REGULATOR_VOLTAGE;
	desc->owner = THIS_MODULE;
	desc->supply_name = "vin";
	desc->of_match = of_match_ptr("regulator");
	desc->regulators_node = of_match_ptr("regulators");
	desc->vsel_reg = MPQ8655_VOUT_COMMAND;
	desc->vsel_mask = 0x0FFF;
	desc->ramp_delay = 100;  /* 100 uV/us = 10us/mV, matches DAh=0x00 default */

	/*
	 * When an external voltage divider is used and VOUT_SCALE_LOOP is
	 * programmed to reflect the actual divider ratio, the effective step
	 * becomes:
	 *   uV_step = 2mV * (1000 * VOUT_SCALE_LOOP / kdiv_x1000)
	 *   vref_step_adj_x1000 = 1000 * vout_scale_loop / kdiv_x1000
	 *   uV_step = 2mV * vref_step_adj_x1000
	 * This adjustment ensures correct voltage mapping for devices such as
	 * SC812C0 that rely on the regulator framework's linear voltage table
	 * while supporting external dividers.
	 */
	desc->uV_step = 2 * mpq8655->vref_step_adj_x1000;

	/*
	 * Configure voltage range based on divider mode.
	 */
	if (mpq8655->use_external_div) {
		kdiv = mpq8655->kdiv_x1000;

		/*
		 * Guard against division by zero: kdiv must be > 0.
		 * If kdiv is 0 (should not happen with valid DT), use 1000 as fallback.
		 */
		if (kdiv == 0) {
			dev_warn(dev, "Kdiv is 0, using 1.0 as fallback\n");
			kdiv = 1000;
		}

		min_uV = mpq8655_voltage_ranges[range][0] * 1000 / kdiv;
		max_uV = mpq8655_voltage_ranges[range][1] * 1000 / kdiv;

		desc->min_uV = min_uV;

		/*
		 * Guard against division by zero: desc->uV_step must be > 0.
		 * If uV_step is 0 (should not happen), use 2000 as fallback.
		 */
		if (desc->uV_step == 0) {
			dev_warn(dev, "uV_step is 0, using 2000 as fallback\n");
			desc->uV_step = 2000;
		}

		desc->n_voltages = max_uV / desc->uV_step + 1;
		desc->linear_min_sel = min_uV / desc->uV_step;

		mpq_dbg(dev, "External divider: VREF range %u-%u uV, Kdiv=%u.%03u, VOUT range %u-%u uV, %u levels\n",
			mpq8655_voltage_ranges[range][0],
			mpq8655_voltage_ranges[range][1],
			kdiv / 1000, kdiv % 1000,
			min_uV, max_uV, desc->n_voltages);
	} else {
		desc->min_uV = mpq8655_voltage_ranges[range][0];
		/*
		 * Guard against division by zero: desc->uV_step must be > 0.
		 */
		if (desc->uV_step == 0) {
			dev_warn(dev, "uV_step is 0, using 2000 as fallback\n");
			desc->uV_step = 2000;
		}

		desc->n_voltages = mpq8655_voltage_ranges[range][1] / desc->uV_step + 1;
		desc->linear_min_sel = desc->min_uV / desc->uV_step;

		mpq_dbg(dev, "Internal divider: voltage range %u-%u uV, %u levels\n",
			mpq8655_voltage_ranges[range][0],
			mpq8655_voltage_ranges[range][1],
			desc->n_voltages);
	}

	/* Step 8: Register regulator device. */
	config.dev = dev;
	config.driver_data = mpq8655;
	config.regmap = mpq8655->regmap_16bit;
	config.of_node = dev->of_node;

	rdev = devm_regulator_register(dev, desc, &config);
	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(dev, "Regulator registration failed: %ld\n",
			PTR_ERR(rdev));
		return ret;
	}

	mpq8655->rdev = rdev;
	i2c_set_clientdata(client, mpq8655);

	mpq_dbg(dev, "MPQ8655 regulator registered successfully\n"
		"  Divider mode: %s\n"
		"  Voltage range: %u-%u uV\n"
		"  linear min sel: %d\n"
		"  Number of levels: %u\n"
		"  Step size: %u uV\n",
		mpq8655->use_external_div ? "EXTERNAL" : "INTERNAL",
		desc->min_uV,
		desc->n_voltages * desc->uV_step,
		desc->linear_min_sel,
		desc->n_voltages,
		desc->uV_step);

	return 0;
}

/* Device tree match table */
static const struct of_device_id mpq8655_of_match[] = {
	{ .compatible = "mps,mpq8655" },
	{ .compatible = "sc,sc812c0" },
	{ .compatible = "ifs,is6608" },
	{ .compatible = "ein,es5330" },
	{ }
};
MODULE_DEVICE_TABLE(of, mpq8655_of_match);

/* I2C device ID table */
static const struct i2c_device_id mpq8655_id[] = {
	{ "mpq8655", 0 },
	{ "sc812c0", 0 },
	{ "is6608",  0 },
	{ "es5330",  0  },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mpq8655_id);

/* I2C driver structure */
static struct i2c_driver mpq8655_regulator_driver = {
	.driver = {
		.name = "mpq8655-regulator",
		.of_match_table = of_match_ptr(mpq8655_of_match),
	},
	.probe = mpq8655_regulator_probe,
	.id_table = mpq8655_id,
};

module_i2c_driver(mpq8655_regulator_driver);

MODULE_AUTHOR("Xu Shengfei <xsf@rock-chips.com>");
MODULE_DESCRIPTION("MPQ8655 multi-phase buck regulator driver");
MODULE_LICENSE("GPL");
