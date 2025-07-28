// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * serdes-i2c.c  --  Control screen brightness
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 *
 * Author: ZITONG CAI <zitong.cai@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>

#define PWM_MAX_LEVEL                            0x64

#define DISPLAY_STATUS                           0x40
#define LVDS_LOCK_STATUS                         0x41
#define CUR_BRIGHTNESS_LEVEL                     0x42
#define OLED_FAULT_RECORD                        0x43
#define PCB_TEMP_STATUS                          0x44
#define OLED_TEMP_STATUS                         0x45
#define CID_POWER_STATUS                         0x46
#define CID_HARDWARE_VERSION                     0x47
#define CID_SOFT_APP_VERSION                     0x48
#define CID_BOOTLOADER_VERSION                   0x49
#define CID_FUALT_RECORD                         0x4a
#define CID_VOLTAGE_VALUE                        0x4b
#define CID_CURRENT_MODE_STATUS                  0x4c
#define CID_ENTER_AUTO_CAUSE                     0x4d
#define CID_CAN_STATUS                           0x4e


#define REQUEST_DISPLAY_STATUS                   0x80
#define REQUEST_LVDS_LOCK_STATUS                 0x81
#define REQUEST_BRIGHTNESS_LEVEL                 0x82
#define REQUEST_OLED_FAULT_RECORD                0x83
#define REQUEST_PCB_TEMP_STATUS                  0x84
#define REQUEST_OLED_TEMP_STATUS                 0x85
#define REQUEST_CID_POWER_STATE                  0x86
#define REQUEST_CID_HARDWARE_VERSION             0x87
#define REQUEST_CID_SOFT_APP_VERSION             0x88
#define REQUEST_CID_BOOTLOADER_VERSION           0x89
#define REQUEST_CID_FUALT_RECORD                 0x8a
#define REQUEST_CID_VOLTAGE_VALUE                0x8b
#define REQUEST_CID_CURRENT_MODE_STATUS          0x8c
#define REQUEST_CID_ENTER_AUTO_CAUSE             0x8d
#define REQUEST_DISPLAY_STATUS_SET               0x8e
#define REQUEST_CID_BRIGHTNESS_SET               0x8f
#define REQUEST_IDCM_WRITE_HEART                 0x90
#define REQUEST_CID_CAN_STATUS                   0x91
#define REQUEST_IDCM_SEND_CRC                    0x92

enum {
	DISPLAY_OFF,
	DISPLAY_ON
};

struct r7f701_pwm_chip {
	struct pwm_chip	chip;
	struct device	*dev;
	struct regmap *regmap;

};

static bool r7f701_is_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x80 ... 0x92:
		return true;
	}
	return false;
}

static bool r7f701_is_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x40 ... 0x4e:
		return true;
	}
	return false;
}

static bool r7f701_is_volatile_reg(struct device *dev, unsigned int reg)
{
	return true;
}


static const struct regmap_config r7f701_regmap_config = {
	.name = "r7f701",
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = r7f701_is_writeable_reg,
	.readable_reg = r7f701_is_readable_reg,
	.volatile_reg = r7f701_is_volatile_reg,
	.cache_type = REGCACHE_RBTREE,
	.max_register = 0x92,
};

static inline struct r7f701_pwm_chip *to_r7f701_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct r7f701_pwm_chip, chip);
}

static int r7f701_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
	u64 duty_ns, u64 period_ns)
{
	u8 reg = 0;
	u64 div = 0;
	u8 level = 0;
	int ret = 0;
	u8 data[7] = {0};
	struct r7f701_pwm_chip *r7f701 = to_r7f701_pwm_chip(chip);

	div = duty_ns * PWM_MAX_LEVEL;
	level = DIV_ROUND_CLOSEST_ULL(div, period_ns);

	reg = REQUEST_DISPLAY_STATUS_SET;
	data[0] = DISPLAY_ON;
	data[1] = level;
	data[6] = reg ^ data[0] ^ data[1];
	ret |= regmap_bulk_write(r7f701->regmap, reg, data, ARRAY_SIZE(data));
	memset(data, 0, sizeof(data));

	reg = REQUEST_CID_BRIGHTNESS_SET;
	data[0] = level;
	data[6] = reg ^ data[0];
	ret |= regmap_bulk_write(r7f701->regmap, reg, data, ARRAY_SIZE(data));

	dev_dbg(chip->dev, "%s: pwm chip BRIGHTNESS_SET level 0x%x ret=%d\n", __func__, level, ret);

	return 0;
}

static int r7f701_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	dev_dbg(chip->dev, "%s: pwm chip\n", __func__);

	return 0;
}

static void r7f701_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct r7f701_pwm_chip *r7f701 = to_r7f701_pwm_chip(chip);
	int ret = 0;
	u8 reg = 0;
	u8 data[7] = {0};

	reg = REQUEST_DISPLAY_STATUS_SET;
	data[0] = DISPLAY_OFF;
	data[6] = reg ^ data[0];
	ret = regmap_bulk_write(r7f701->regmap, reg, data, ARRAY_SIZE(data));

	dev_dbg(chip->dev, "%s: pwm chip ret=%d\n", __func__, ret);
}

static int r7f701_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
	const struct pwm_state *state)
{
	int err;

	if (state->polarity != PWM_POLARITY_NORMAL)
		return -EINVAL;

	if (!state->enabled) {
		if (pwm->state.enabled)
			r7f701_pwm_disable(chip, pwm);

		return 0;
	}

	err = r7f701_pwm_config(chip, pwm, state->duty_cycle, state->period);
	if (err)
		return err;

	if (!pwm->state.enabled)
		return r7f701_pwm_enable(chip, pwm);

	return 0;
}

static int r7f701_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				 struct pwm_state *state)
{
	state->enabled = true;
	state->polarity = PWM_POLARITY_NORMAL;

	dev_dbg(chip->dev, "%s: pwm chip\n", __func__);

	return 0;
}

static const struct pwm_ops r7f701_pwm_ops = {
	.apply = r7f701_pwm_apply,
	.get_state = r7f701_pwm_get_state,
	.owner = THIS_MODULE,
};

static const struct of_device_id pwm_of_match[] = {
	{ .compatible = "r7f701-pwm", .data = 0},
	{ }
};

static int pwm_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct r7f701_pwm_chip *r7f701;
	int ret = 0;

	r7f701 = devm_kzalloc(dev, sizeof(*r7f701), GFP_KERNEL);
	if (!r7f701)
		return -ENOMEM;

	r7f701->dev = dev;
	r7f701->chip.dev = dev;
	r7f701->chip.ops = &r7f701_pwm_ops;
	r7f701->chip.npwm = 1;

	i2c_set_clientdata(client, r7f701);
	dev_set_drvdata(dev, r7f701);

	r7f701->regmap = devm_regmap_init_i2c(client, &r7f701_regmap_config);
	if (IS_ERR(r7f701->regmap)) {
		dev_err(dev, "%s: Failed to allocate r7f701 register map\n", __func__);
		return PTR_ERR(r7f701->regmap);
	}

	ret = devm_pwmchip_add(dev, &r7f701->chip);
	if (ret < 0) {
		dev_err(dev, "%s: pwmchip_add() failed: %d\n", __func__, ret);
		return ret;
	}

	dev_dbg(dev, "%s successful\n", __func__);

	return 0;
}

static struct i2c_driver r7f701_i2c_driver = {
	.driver		= {
		.name	= "r7f701-pwm",
		.of_match_table = of_match_ptr(pwm_of_match),
	},
	.probe		= pwm_probe,
};

static int __init r7f701_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&r7f701_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register r7f701 I2C driver: %d\n", ret);

	return ret;
}

static void __exit r7f701_i2c_exit(void)
{
	i2c_del_driver(&r7f701_i2c_driver);
}

subsys_initcall(r7f701_i2c_init);
module_exit(r7f701_i2c_exit);

MODULE_AUTHOR("ZITONG CAI <zitong.cai@rock-chips.com>");
MODULE_DESCRIPTION("display pwm interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:r7f701-PWM");
