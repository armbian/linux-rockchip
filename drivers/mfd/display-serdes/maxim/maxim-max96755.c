// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * maxim-max96755.c  --  I2C register interface access for max96755 serdes chip
 *
 * Copyright (c) 2023-2028 Rockchip Electronics Co., Ltd.
 *
 * Author:
 */

#include "../core.h"
#include "maxim-max96755.h"

static struct regmap_config max96755_regmap_config = {
	.name = "max96755",
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x2000,
	.cache_type = REGCACHE_NONE,
};

static struct pinctrl_pin_desc max96755_pins_desc[] = {
	PINCTRL_PIN(MAXIM_MAX96755_MFP0, "MAX96755_MFP0"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP1, "MAX96755_MFP1"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP2, "MAX96755_MFP2"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP3, "MAX96755_MFP3"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP4, "MAX96755_MFP4"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP5, "MAX96755_MFP5"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP6, "MAX96755_MFP6"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP7, "MAX96755_MFP7"),

	PINCTRL_PIN(MAXIM_MAX96755_MFP8, "MAX96755_MFP8"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP9, "MAX96755_MFP9"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP10, "MAX96755_MFP10"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP11, "MAX96755_MFP11"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP12, "MAX96755_MFP12"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP13, "MAX96755_MFP13"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP14, "MAX96755_MFP14"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP15, "MAX96755_MFP15"),

	PINCTRL_PIN(MAXIM_MAX96755_MFP16, "MAX96755_MFP16"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP17, "MAX96755_MFP17"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP18, "MAX96755_MFP18"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP19, "MAX96755_MFP19"),
	PINCTRL_PIN(MAXIM_MAX96755_MFP20, "MAX96755_MFP20"),
};

static struct group_desc max96755_groups_desc[] = {
	GROUP_DESC_CONFIG(MAX96755_MFP0),
	GROUP_DESC_CONFIG(MAX96755_MFP1),
	GROUP_DESC(MAX96755_MFP2),
	GROUP_DESC(MAX96755_MFP3),
	GROUP_DESC_CONFIG(MAX96755_MFP4),
	GROUP_DESC_CONFIG(MAX96755_MFP5),
	GROUP_DESC(MAX96755_MFP6),
	GROUP_DESC_CONFIG(MAX96755_MFP7),

	GROUP_DESC_CONFIG(MAX96755_MFP8),
	GROUP_DESC_CONFIG(MAX96755_MFP9),
	GROUP_DESC_CONFIG(MAX96755_MFP10),
	GROUP_DESC_CONFIG(MAX96755_MFP11),
	GROUP_DESC_CONFIG(MAX96755_MFP12),
	GROUP_DESC_CONFIG(MAX96755_MFP13),
	GROUP_DESC_CONFIG(MAX96755_MFP14),
	GROUP_DESC_CONFIG(MAX96755_MFP15),

	GROUP_DESC_CONFIG(MAX96755_MFP16),
	GROUP_DESC_CONFIG(MAX96755_MFP17),
	GROUP_DESC_CONFIG(MAX96755_MFP18),
	GROUP_DESC(MAX96755_MFP19),
	GROUP_DESC(MAX96755_MFP20),
	GROUP_DESC(MAX96755_I2C),
	GROUP_DESC(MAX96755_UART),
};

static struct function_desc max96755_functions_desc[] = {
	FUNCTION_DESC_GPIO_INPUT(0),
	FUNCTION_DESC_GPIO_INPUT(1),
	FUNCTION_DESC_GPIO_INPUT(2),
	FUNCTION_DESC_GPIO_INPUT(3),
	FUNCTION_DESC_GPIO_INPUT(4),
	FUNCTION_DESC_GPIO_INPUT(5),
	FUNCTION_DESC_GPIO_INPUT(6),
	FUNCTION_DESC_GPIO_INPUT(7),

	FUNCTION_DESC_GPIO_INPUT(8),
	FUNCTION_DESC_GPIO_INPUT(9),
	FUNCTION_DESC_GPIO_INPUT(10),
	FUNCTION_DESC_GPIO_INPUT(11),
	FUNCTION_DESC_GPIO_INPUT(12),
	FUNCTION_DESC_GPIO_INPUT(13),
	FUNCTION_DESC_GPIO_INPUT(14),
	FUNCTION_DESC_GPIO_INPUT(15),

	FUNCTION_DESC_GPIO_INPUT(16),
	FUNCTION_DESC_GPIO_INPUT(17),
	FUNCTION_DESC_GPIO_INPUT(18),
	FUNCTION_DESC_GPIO_INPUT(19),
	FUNCTION_DESC_GPIO_INPUT(20),

	FUNCTION_DESC_GPIO_OUTPUT(0),
	FUNCTION_DESC_GPIO_OUTPUT(1),
	FUNCTION_DESC_GPIO_OUTPUT(2),
	FUNCTION_DESC_GPIO_OUTPUT(3),
	FUNCTION_DESC_GPIO_OUTPUT(4),
	FUNCTION_DESC_GPIO_OUTPUT(5),
	FUNCTION_DESC_GPIO_OUTPUT(6),
	FUNCTION_DESC_GPIO_OUTPUT(7),

	FUNCTION_DESC_GPIO_OUTPUT(8),
	FUNCTION_DESC_GPIO_OUTPUT(9),
	FUNCTION_DESC_GPIO_OUTPUT(10),
	FUNCTION_DESC_GPIO_OUTPUT(11),
	FUNCTION_DESC_GPIO_OUTPUT(12),
	FUNCTION_DESC_GPIO_OUTPUT(13),
	FUNCTION_DESC_GPIO_OUTPUT(14),
	FUNCTION_DESC_GPIO_OUTPUT(15),

	FUNCTION_DESC_GPIO_OUTPUT(16),
	FUNCTION_DESC_GPIO_OUTPUT(17),
	FUNCTION_DESC_GPIO_OUTPUT(18),
	FUNCTION_DESC_GPIO_OUTPUT(19),
	FUNCTION_DESC_GPIO_OUTPUT(20),

	FUNCTION_DESC(MAX96755_I2C),
	FUNCTION_DESC(MAX96755_UART),
};

static struct serdes_chip_pinctrl_info max96755_pinctrl_info = {
	.pins = max96755_pins_desc,
	.num_pins = ARRAY_SIZE(max96755_pins_desc),
	.groups = max96755_groups_desc,
	.num_groups = ARRAY_SIZE(max96755_groups_desc),
	.functions = max96755_functions_desc,
	.num_functions = ARRAY_SIZE(max96755_functions_desc),
};

static bool max96755_bridge_link_locked(struct serdes *serdes)
{
	u32 val = 0, i;

	if (serdes->lock_gpio) {
		for (i = 0; i < 3; i++) {
			val = gpiod_get_value_cansleep(serdes->lock_gpio);
			if (val)
				break;
			msleep(20);
		}

		SERDES_DBG_CHIP("%s:%s-%s, gpio %s\n", __func__, dev_name(serdes->dev),
		       serdes->chip_data->name, (val) ? "locked" : "unlocked");
		if (val)
			return true;
	}

	if (serdes_reg_read(serdes, 0x0013, &val)) {
		SERDES_DBG_CHIP("serdes %s: unlocked val=0x%x\n", __func__, val);
		return false;
	}

	if (!FIELD_GET(LOCKED, val)) {
		SERDES_DBG_CHIP("serdes %s: unlocked val=0x%x\n", __func__, val);
		return false;
	}

	SERDES_DBG_CHIP("%s: serdes reg locked 0x%x\n", __func__, val);

	return true;
}

static int max96755_bridge_init(struct serdes *serdes)
{
	bool status;
	int loop = 0;
	struct device *dev = serdes->dev;

	for (loop = 0; loop < 3; loop++) {
		if (loop != 0)
			msleep(20);

		status = max96755_bridge_link_locked(serdes);
		if (status)
			break;
	}

	if (!status)
		dev_err(dev, "serdes %s link unlocked\n", serdes->chip_data->name);

	return 0;
}

static int max96755_bridge_attach(struct serdes *serdes)
{
	if (max96755_bridge_link_locked(serdes))
		serdes->serdes_bridge->status = connector_status_connected;
	else
		serdes->serdes_bridge->status = connector_status_disconnected;

	return 0;
}

static enum drm_connector_status
max96755_bridge_detect(struct serdes *serdes)
{
	struct serdes_bridge *serdes_bridge = serdes->serdes_bridge;
	enum drm_connector_status status = connector_status_connected;

	if (!drm_kms_helper_is_poll_worker())
		return serdes_bridge->status;

	if (!max96755_bridge_link_locked(serdes)) {
		status = connector_status_disconnected;
		goto out;
	}

	if (extcon_get_state(serdes->extcon, EXTCON_JACK_VIDEO_OUT)) {
		if (atomic_cmpxchg(&serdes_bridge->triggered, 1, 0)) {
			status = connector_status_disconnected;
			goto out;
		}

	} else {
		atomic_set(&serdes_bridge->triggered, 0);
	}

	if (serdes_bridge->next_bridge && (serdes_bridge->next_bridge->ops & DRM_BRIDGE_OP_DETECT))
		return drm_bridge_detect(serdes_bridge->next_bridge);

out:
	serdes_bridge->status = status;
	SERDES_DBG_CHIP("%s:%s %s, status=%d state=%d\n", __func__, dev_name(serdes->dev),
			serdes->chip_data->name,
			status, serdes->extcon->state);
	return status;
}

static int max96755_bridge_enable(struct serdes *serdes)
{
	int ret = 0;

	SERDES_DBG_CHIP("%s: serdes chip %s ret=%d\n", __func__, serdes->chip_data->name, ret);
	return ret;
}

static int max96755_bridge_disable(struct serdes *serdes)
{
	int ret = 0;

	return ret;
}

static struct serdes_chip_bridge_ops max96755_bridge_ops = {
	.init = max96755_bridge_init,
	.attach = max96755_bridge_attach,
	.detect = max96755_bridge_detect,
	.enable = max96755_bridge_enable,
	.disable = max96755_bridge_disable,
};

static int max96755_chip_init(struct serdes *serdes)
{
	if (serdes->enable_gpio) {
		gpiod_direction_output(serdes->enable_gpio, 1);
		msleep(50);
	}

	SERDES_DBG_CHIP("%s serdes %s chip init\n",
			dev_name(serdes->dev), serdes->chip_data->name);

	return 0;
}

static int max96755_pinctrl_set_mux(struct serdes *serdes,
				    unsigned int function, unsigned int group)
{
	struct serdes_pinctrl *pinctrl = serdes->pinctrl;
	struct function_desc *func;
	struct group_desc *grp;
	unsigned int i, offset, npins;
	const char *func_name, *grp_name;

	func = pinmux_generic_get_function(pinctrl->pctl, function);
	if (!func)
		return -EINVAL;

	grp = pinctrl_generic_get_group(pinctrl->pctl, group);
	if (!grp)
		return -EINVAL;

#if KERNEL_VERSION(6, 12, 0) > LINUX_VERSION_CODE
	npins = grp->num_pins;
	func_name = func->name;
	grp_name = grp->name;
#else
	npins = grp->grp.npins;
	func_name = func->func.name;
	grp_name = grp->grp.name;
#endif

	SERDES_DBG_CHIP("%s: serdes chip %s func=%s data=%p group=%s data=%p, num_pin=%d\n",
			__func__, serdes->chip_data->name, func_name,
			func->data, grp_name, grp->data, npins);

	if (func->data) {
		struct serdes_function_data *fdata = func->data;

		for (i = 0; i < npins; i++) {
#if KERNEL_VERSION(6, 12, 0) > LINUX_VERSION_CODE
			offset = grp->pins[i] - pinctrl->pin_base;
#else
			offset = grp->grp.pins[i] - pinctrl->pin_base;
#endif

			serdes_set_bits(serdes, GPIO_A_REG(offset),
					GPIO_OUT_DIS | GPIO_RX_EN | GPIO_TX_EN,
					FIELD_PREP(GPIO_OUT_DIS, fdata->gpio_out_dis) |
					FIELD_PREP(GPIO_RX_EN, fdata->gpio_rx_en) |
					FIELD_PREP(GPIO_TX_EN, fdata->gpio_tx_en));

			if (fdata->gpio_tx_en)
				serdes_set_bits(serdes, GPIO_B_REG(offset), GPIO_TX_ID,
						FIELD_PREP(GPIO_TX_ID, fdata->gpio_tx_id));

			if (fdata->gpio_rx_en)
				serdes_set_bits(serdes, GPIO_C_REG(offset), GPIO_RX_ID,
						FIELD_PREP(GPIO_RX_ID, fdata->gpio_rx_id));
		}
	}

	if (grp->data) {
		struct serdes_group_data *gdata = grp->data;

		for (i = 0; i < gdata->num_configs; i++) {
			const struct config_desc *config = &gdata->configs[i];

			serdes_set_bits(serdes, config->reg,
					config->mask, config->val);
		}
	}

	return 0;
}

static int max96755_pinctrl_config_get(struct serdes *serdes,
				       unsigned int pin, unsigned long *config)
{
	enum pin_config_param param = pinconf_to_config_param(*config);
	unsigned int gpio_a_reg, gpio_b_reg;
	u16 arg = 0;

	serdes_reg_read(serdes, GPIO_A_REG(pin), &gpio_a_reg);
	serdes_reg_read(serdes, GPIO_B_REG(pin), &gpio_b_reg);

	SERDES_DBG_CHIP("%s: serdes chip %s pin=%d param=%d\n", __func__,
			serdes->chip_data->name, pin, param);

	switch (param) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		if (FIELD_GET(OUT_TYPE, gpio_b_reg))
			return -EINVAL;
		break;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		if (!FIELD_GET(OUT_TYPE, gpio_b_reg))
			return -EINVAL;
		break;
	case PIN_CONFIG_BIAS_DISABLE:
		if (FIELD_GET(PULL_UPDN_SEL, gpio_b_reg) != 0)
			return -EINVAL;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		if (FIELD_GET(PULL_UPDN_SEL, gpio_b_reg) != 1)
			return -EINVAL;
		switch (FIELD_GET(RES_CFG, gpio_a_reg)) {
		case 0:
			arg = 40000;
			break;
		case 1:
			arg = 10000;
			break;
		}
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		if (FIELD_GET(PULL_UPDN_SEL, gpio_b_reg) != 2)
			return -EINVAL;
		switch (FIELD_GET(RES_CFG, gpio_a_reg)) {
		case 0:
			arg = 40000;
			break;
		case 1:
			arg = 10000;
			break;
		}
		break;
	case PIN_CONFIG_OUTPUT:
		if (FIELD_GET(GPIO_OUT_DIS, gpio_a_reg))
			return -EINVAL;

		arg = FIELD_GET(GPIO_OUT, gpio_a_reg);
		break;
	default:
		return -EOPNOTSUPP;
	}

	*config = pinconf_to_config_packed(param, arg);

	return 0;
}

static int max96755_pinctrl_config_set(struct serdes *serdes,
				       unsigned int pin, unsigned long *configs,
				       unsigned int num_configs)
{
	enum pin_config_param param;
	u32 arg;
	u8 res_cfg;
	int i;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		SERDES_DBG_CHIP("%s: serdes chip %s pin=%d param=%d\n", __func__,
				serdes->chip_data->name, pin, param);

		switch (param) {
		case PIN_CONFIG_DRIVE_OPEN_DRAIN:
			serdes_set_bits(serdes, GPIO_B_REG(pin),
					OUT_TYPE, FIELD_PREP(OUT_TYPE, 0));
			break;
		case PIN_CONFIG_DRIVE_PUSH_PULL:
			serdes_set_bits(serdes, GPIO_B_REG(pin),
					OUT_TYPE, FIELD_PREP(OUT_TYPE, 1));
			break;
		case PIN_CONFIG_BIAS_DISABLE:
			serdes_set_bits(serdes, GPIO_C_REG(pin),
					PULL_UPDN_SEL,
					FIELD_PREP(PULL_UPDN_SEL, 0));
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			switch (arg) {
			case 40000:
				res_cfg = 0;
				break;
			case 1000000:
				res_cfg = 1;
				break;
			default:
				return -EINVAL;
			}

			serdes_set_bits(serdes, GPIO_A_REG(pin),
					RES_CFG, FIELD_PREP(RES_CFG, res_cfg));
			serdes_set_bits(serdes, GPIO_C_REG(pin),
					PULL_UPDN_SEL,
					FIELD_PREP(PULL_UPDN_SEL, 1));
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			switch (arg) {
			case 40000:
				res_cfg = 0;
				break;
			case 1000000:
				res_cfg = 1;
				break;
			default:
				return -EINVAL;
			}

			serdes_set_bits(serdes, GPIO_A_REG(pin),
					RES_CFG, FIELD_PREP(RES_CFG, res_cfg));
			serdes_set_bits(serdes, GPIO_C_REG(pin),
					PULL_UPDN_SEL,
					FIELD_PREP(PULL_UPDN_SEL, 2));
			break;
		case PIN_CONFIG_OUTPUT:
			serdes_set_bits(serdes, GPIO_A_REG(pin),
					GPIO_OUT_DIS | GPIO_OUT,
					FIELD_PREP(GPIO_OUT_DIS, 0) |
					FIELD_PREP(GPIO_OUT, arg));
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

static struct serdes_chip_pinctrl_ops max96755_pinctrl_ops = {
	.pin_config_get = max96755_pinctrl_config_get,
	.pin_config_set = max96755_pinctrl_config_set,
	.set_mux = max96755_pinctrl_set_mux,
};

static int max96755_gpio_direction_input(struct serdes *serdes, int gpio)
{
	return 0;
}

static int max96755_gpio_direction_output(struct serdes *serdes, int gpio, int value)
{
	return 0;
}

static int max96755_gpio_get_level(struct serdes *serdes, int gpio)
{
	return 0;
}

static int max96755_gpio_set_level(struct serdes *serdes, int gpio, int value)
{
	return 0;
}

static int max96755_gpio_set_config(struct serdes *serdes, int gpio, unsigned long config)
{
	return 0;
}

static int max96755_gpio_to_irq(struct serdes *serdes, int gpio)
{
	return 0;
}

static struct serdes_chip_gpio_ops max96755_gpio_ops = {
	.direction_input = max96755_gpio_direction_input,
	.direction_output = max96755_gpio_direction_output,
	.get_level = max96755_gpio_get_level,
	.set_level = max96755_gpio_set_level,
	.set_config = max96755_gpio_set_config,
	.to_irq = max96755_gpio_to_irq,
};

static int max96755_check_hw_state(struct serdes *serdes)
{
	int ret = 0, retry = 0;
	unsigned int chipid = 0;
	struct device *dev = serdes->dev;

	for (retry = 0; retry < 10; retry++) {
		if (retry != 0) {
			SERDES_DBG_CHIP("check serdes %s hw state retry=%d",
					serdes->chip_data->name, retry);
			msleep(20);
		}

		ret = serdes_reg_read(serdes, MAXIM_SERDES_REG_CHIP_ID, &chipid);
		if (!ret) {
			dev_info(dev, "%s is Detected\n", serdes->chip_data->name);
			return 0;
		}
	}

	dev_err(dev, "serdes %s check hw state error, ret=%d\n", serdes->chip_data->name, ret);

	return -ENODEV;
}

static struct serdes_check_state_ops max96755_check_ops = {
	.check_hw  = max96755_check_hw_state,
};

static int max96755_pm_suspend(struct serdes *serdes)
{
	return 0;
}

static int max96755_pm_resume(struct serdes *serdes)
{
	return 0;
}

static struct serdes_chip_pm_ops max96755_pm_ops = {
	.suspend = max96755_pm_suspend,
	.resume = max96755_pm_resume,
};

static int max96755_irq_lock_handle(struct serdes *serdes)
{
	return IRQ_HANDLED;
}

static int max96755_irq_err_handle(struct serdes *serdes)
{
	return IRQ_HANDLED;
}

static struct serdes_chip_irq_ops max96755_irq_ops = {
	.lock_handle = max96755_irq_lock_handle,
	.err_handle = max96755_irq_err_handle,
};

struct serdes_chip_data serdes_max96755_data = {
	.name		= "max96755",
	.serdes_type	= TYPE_SER,
	.serdes_id	= MAXIM_ID_MAX96755,
	.connector_type	= DRM_MODE_CONNECTOR_LVDS,
	.chip_init	= max96755_chip_init,
	.check_ops	= &max96755_check_ops,
	.regmap_config	= &max96755_regmap_config,
	.pinctrl_info	= &max96755_pinctrl_info,
	.bridge_ops	= &max96755_bridge_ops,
	.pinctrl_ops	= &max96755_pinctrl_ops,
	.gpio_ops	= &max96755_gpio_ops,
	.pm_ops		= &max96755_pm_ops,
	.irq_ops	= &max96755_irq_ops,
};
EXPORT_SYMBOL_GPL(serdes_max96755_data);

MODULE_LICENSE("GPL");
