// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * maxim-max96749.c  --  I2C register interface access for max96749 serdes chip
 *
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 *
 * Author: ZITONG CAI <zitong.cai@rock-chips.com>
 */

#include "../core.h"
#include "maxim-max96749.h"

static struct regmap_config max96749_regmap_config = {
	.name = "max96749",
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0x8000,
	.cache_type = REGCACHE_NONE,
};

static struct pinctrl_pin_desc max96749_pins_desc[] = {
	PINCTRL_PIN(MAXIM_MAX96749_MFP0, "MAX96749_MFP0"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP1, "MAX96749_MFP1"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP2, "MAX96749_MFP2"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP3, "MAX96749_MFP3"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP4, "MAX96749_MFP4"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP5, "MAX96749_MFP5"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP6, "MAX96749_MFP6"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP7, "MAX96749_MFP7"),

	PINCTRL_PIN(MAXIM_MAX96749_MFP8, "MAX96749_MFP8"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP9, "MAX96749_MFP9"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP10, "MAX96749_MFP10"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP11, "MAX96749_MFP11"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP12, "MAX96749_MFP12"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP13, "MAX96749_MFP13"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP14, "MAX96749_MFP14"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP15, "MAX96749_MFP15"),

	PINCTRL_PIN(MAXIM_MAX96749_MFP16, "MAX96749_MFP16"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP17, "MAX96749_MFP17"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP18, "MAX96749_MFP18"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP19, "MAX96749_MFP19"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP20, "MAX96749_MFP20"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP21, "MAX96749_MFP21"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP22, "MAX96749_MFP22"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP23, "MAX96749_MFP23"),

	PINCTRL_PIN(MAXIM_MAX96749_MFP24, "MAX96749_MFP24"),
	PINCTRL_PIN(MAXIM_MAX96749_MFP25, "MAX96749_MFP25"),
};

static struct group_desc max96749_groups_desc[] = {
	GROUP_DESC(MAX96749_MFP0),
	GROUP_DESC(MAX96749_MFP1),
	GROUP_DESC(MAX96749_MFP2),
	GROUP_DESC(MAX96749_MFP3),
	GROUP_DESC(MAX96749_MFP4),
	GROUP_DESC(MAX96749_MFP5),
	GROUP_DESC(MAX96749_MFP6),
	GROUP_DESC(MAX96749_MFP7),

	GROUP_DESC(MAX96749_MFP8),
	GROUP_DESC(MAX96749_MFP9),
	GROUP_DESC(MAX96749_MFP10),
	GROUP_DESC(MAX96749_MFP11),
	GROUP_DESC(MAX96749_MFP12),
	GROUP_DESC(MAX96749_MFP13),
	GROUP_DESC(MAX96749_MFP14),
	GROUP_DESC(MAX96749_MFP15),

	GROUP_DESC(MAX96749_MFP16),
	GROUP_DESC(MAX96749_MFP17),
	GROUP_DESC(MAX96749_MFP18),
	GROUP_DESC(MAX96749_MFP19),
	GROUP_DESC(MAX96749_MFP20),
	GROUP_DESC(MAX96749_MFP21),
	GROUP_DESC(MAX96749_MFP22),
	GROUP_DESC(MAX96749_MFP23),

	GROUP_DESC(MAX96749_MFP24),
	GROUP_DESC(MAX96749_MFP25),

	GROUP_DESC(MAX96749_I2C),
	GROUP_DESC(MAX96749_UART),
};

static struct function_desc max96749_functions_desc[] = {
	FUNCTION_DESC_GPIO_INPUT_A(0),
	FUNCTION_DESC_GPIO_INPUT_A(1),
	FUNCTION_DESC_GPIO_INPUT_A(2),
	FUNCTION_DESC_GPIO_INPUT_A(3),
	FUNCTION_DESC_GPIO_INPUT_A(4),
	FUNCTION_DESC_GPIO_INPUT_A(5),
	FUNCTION_DESC_GPIO_INPUT_A(6),
	FUNCTION_DESC_GPIO_INPUT_A(7),

	FUNCTION_DESC_GPIO_INPUT_A(8),
	FUNCTION_DESC_GPIO_INPUT_A(9),
	FUNCTION_DESC_GPIO_INPUT_A(10),
	FUNCTION_DESC_GPIO_INPUT_A(11),
	FUNCTION_DESC_GPIO_INPUT_A(12),
	FUNCTION_DESC_GPIO_INPUT_A(13),
	FUNCTION_DESC_GPIO_INPUT_A(14),
	FUNCTION_DESC_GPIO_INPUT_A(15),

	FUNCTION_DESC_GPIO_INPUT_A(16),
	FUNCTION_DESC_GPIO_INPUT_A(17),
	FUNCTION_DESC_GPIO_INPUT_A(18),
	FUNCTION_DESC_GPIO_INPUT_A(19),
	FUNCTION_DESC_GPIO_INPUT_A(20),
	FUNCTION_DESC_GPIO_INPUT_A(21),
	FUNCTION_DESC_GPIO_INPUT_A(22),
	FUNCTION_DESC_GPIO_INPUT_A(23),

	FUNCTION_DESC_GPIO_INPUT_A(24),
	FUNCTION_DESC_GPIO_INPUT_A(25),

	FUNCTION_DESC_GPIO_OUTPUT_A(0),
	FUNCTION_DESC_GPIO_OUTPUT_A(1),
	FUNCTION_DESC_GPIO_OUTPUT_A(2),
	FUNCTION_DESC_GPIO_OUTPUT_A(3),
	FUNCTION_DESC_GPIO_OUTPUT_A(4),
	FUNCTION_DESC_GPIO_OUTPUT_A(5),
	FUNCTION_DESC_GPIO_OUTPUT_A(6),
	FUNCTION_DESC_GPIO_OUTPUT_A(7),

	FUNCTION_DESC_GPIO_OUTPUT_A(8),
	FUNCTION_DESC_GPIO_OUTPUT_A(9),
	FUNCTION_DESC_GPIO_OUTPUT_A(10),
	FUNCTION_DESC_GPIO_OUTPUT_A(11),
	FUNCTION_DESC_GPIO_OUTPUT_A(12),
	FUNCTION_DESC_GPIO_OUTPUT_A(13),
	FUNCTION_DESC_GPIO_OUTPUT_A(14),
	FUNCTION_DESC_GPIO_OUTPUT_A(15),

	FUNCTION_DESC_GPIO_OUTPUT_A(16),
	FUNCTION_DESC_GPIO_OUTPUT_A(17),
	FUNCTION_DESC_GPIO_OUTPUT_A(18),
	FUNCTION_DESC_GPIO_OUTPUT_A(19),
	FUNCTION_DESC_GPIO_OUTPUT_A(20),
	FUNCTION_DESC_GPIO_OUTPUT_A(21),
	FUNCTION_DESC_GPIO_OUTPUT_A(22),
	FUNCTION_DESC_GPIO_OUTPUT_A(23),

	FUNCTION_DESC_GPIO_OUTPUT_A(24),
	FUNCTION_DESC_GPIO_OUTPUT_A(25),

	FUNCTION_DESC_GPIO_INPUT_B(0),
	FUNCTION_DESC_GPIO_INPUT_B(1),
	FUNCTION_DESC_GPIO_INPUT_B(2),
	FUNCTION_DESC_GPIO_INPUT_B(3),
	FUNCTION_DESC_GPIO_INPUT_B(4),
	FUNCTION_DESC_GPIO_INPUT_B(5),
	FUNCTION_DESC_GPIO_INPUT_B(6),
	FUNCTION_DESC_GPIO_INPUT_B(7),

	FUNCTION_DESC_GPIO_INPUT_B(8),
	FUNCTION_DESC_GPIO_INPUT_B(9),
	FUNCTION_DESC_GPIO_INPUT_B(10),
	FUNCTION_DESC_GPIO_INPUT_B(11),
	FUNCTION_DESC_GPIO_INPUT_B(12),
	FUNCTION_DESC_GPIO_INPUT_B(13),
	FUNCTION_DESC_GPIO_INPUT_B(14),
	FUNCTION_DESC_GPIO_INPUT_B(15),

	FUNCTION_DESC_GPIO_INPUT_B(16),
	FUNCTION_DESC_GPIO_INPUT_B(17),
	FUNCTION_DESC_GPIO_INPUT_B(18),
	FUNCTION_DESC_GPIO_INPUT_B(19),
	FUNCTION_DESC_GPIO_INPUT_B(20),
	FUNCTION_DESC_GPIO_INPUT_B(21),
	FUNCTION_DESC_GPIO_INPUT_B(22),
	FUNCTION_DESC_GPIO_INPUT_B(23),

	FUNCTION_DESC_GPIO_INPUT_B(24),
	FUNCTION_DESC_GPIO_INPUT_B(25),

	FUNCTION_DESC_GPIO_OUTPUT_B(0),
	FUNCTION_DESC_GPIO_OUTPUT_B(1),
	FUNCTION_DESC_GPIO_OUTPUT_B(2),
	FUNCTION_DESC_GPIO_OUTPUT_B(3),
	FUNCTION_DESC_GPIO_OUTPUT_B(4),
	FUNCTION_DESC_GPIO_OUTPUT_B(5),
	FUNCTION_DESC_GPIO_OUTPUT_B(6),
	FUNCTION_DESC_GPIO_OUTPUT_B(7),

	FUNCTION_DESC_GPIO_OUTPUT_B(8),
	FUNCTION_DESC_GPIO_OUTPUT_B(9),
	FUNCTION_DESC_GPIO_OUTPUT_B(10),
	FUNCTION_DESC_GPIO_OUTPUT_B(11),
	FUNCTION_DESC_GPIO_OUTPUT_B(12),
	FUNCTION_DESC_GPIO_OUTPUT_B(13),
	FUNCTION_DESC_GPIO_OUTPUT_B(14),
	FUNCTION_DESC_GPIO_OUTPUT_B(15),

	FUNCTION_DESC_GPIO_OUTPUT_B(16),
	FUNCTION_DESC_GPIO_OUTPUT_B(17),
	FUNCTION_DESC_GPIO_OUTPUT_B(18),
	FUNCTION_DESC_GPIO_OUTPUT_B(19),
	FUNCTION_DESC_GPIO_OUTPUT_B(20),
	FUNCTION_DESC_GPIO_OUTPUT_B(21),
	FUNCTION_DESC_GPIO_OUTPUT_B(22),
	FUNCTION_DESC_GPIO_OUTPUT_B(23),

	FUNCTION_DESC_GPIO_OUTPUT_B(24),
	FUNCTION_DESC_GPIO_OUTPUT_B(25),

	FUNCTION_DESC_GPIO_OUTPUT_AB(0),
	FUNCTION_DESC_GPIO_OUTPUT_AB(1),
	FUNCTION_DESC_GPIO_OUTPUT_AB(2),
	FUNCTION_DESC_GPIO_OUTPUT_AB(3),
	FUNCTION_DESC_GPIO_OUTPUT_AB(4),
	FUNCTION_DESC_GPIO_OUTPUT_AB(5),
	FUNCTION_DESC_GPIO_OUTPUT_AB(6),
	FUNCTION_DESC_GPIO_OUTPUT_AB(7),

	FUNCTION_DESC_GPIO_OUTPUT_AB(8),
	FUNCTION_DESC_GPIO_OUTPUT_AB(9),
	FUNCTION_DESC_GPIO_OUTPUT_AB(10),
	FUNCTION_DESC_GPIO_OUTPUT_AB(11),
	FUNCTION_DESC_GPIO_OUTPUT_AB(12),
	FUNCTION_DESC_GPIO_OUTPUT_AB(13),
	FUNCTION_DESC_GPIO_OUTPUT_AB(14),
	FUNCTION_DESC_GPIO_OUTPUT_AB(15),

	FUNCTION_DESC_GPIO_OUTPUT_AB(16),
	FUNCTION_DESC_GPIO_OUTPUT_AB(17),
	FUNCTION_DESC_GPIO_OUTPUT_AB(18),
	FUNCTION_DESC_GPIO_OUTPUT_AB(19),
	FUNCTION_DESC_GPIO_OUTPUT_AB(20),
	FUNCTION_DESC_GPIO_OUTPUT_AB(21),
	FUNCTION_DESC_GPIO_OUTPUT_AB(22),
	FUNCTION_DESC_GPIO_OUTPUT_AB(23),

	FUNCTION_DESC_GPIO_OUTPUT_AB(24),
	FUNCTION_DESC_GPIO_OUTPUT_AB(25),

	FUNCTION_DESC_GPIO(),

	FUNCTION_DESC(MAX96749_I2C),
	FUNCTION_DESC(MAX96749_UART),
};

static struct serdes_chip_pinctrl_info max96749_pinctrl_info = {
	.pins = max96749_pins_desc,
	.num_pins = ARRAY_SIZE(max96749_pins_desc),
	.groups = max96749_groups_desc,
	.num_groups = ARRAY_SIZE(max96749_groups_desc),
	.functions = max96749_functions_desc,
	.num_functions = ARRAY_SIZE(max96749_functions_desc),
};

static bool max96749_vid_tx_active(struct serdes *serdes)
{
	u32 val;
	int i = 0, ret = 0;

	for (i = 0; i < 5; i++) {
		ret = serdes_reg_read(serdes, 0x0107, &val);
		if (!ret)
			break;

		SERDES_DBG_CHIP("serdes %s: false val=%d i=%d ret=%d\n", __func__, val, i, ret);
		msleep(20);
	}

	if (ret) {
		SERDES_DBG_CHIP("serdes %s: false val=%d ret=%d\n", __func__, val, ret);
		return false;
	}

	if (!FIELD_GET(VID_TX_ACTIVE_A | VID_TX_ACTIVE_B, val)) {
		SERDES_DBG_CHIP("serdes %s: false val=%d\n", __func__, val);
		return false;
	}

	return true;
}

static bool max96749_bridge_link_locked(struct serdes *serdes)
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

static int max96749_bridge_init(struct serdes *serdes)
{
	bool status;
	int loop = 0;
	struct device *dev = serdes->dev;

	if (max96749_vid_tx_active(serdes)) {
		extcon_set_state(serdes->extcon, EXTCON_JACK_VIDEO_OUT, true);
		pr_info("serdes %s, extcon is true state=%d\n", __func__, serdes->extcon->state);
	} else {
		pr_info("serdes %s, extcon is false\n", __func__);
	}

	for (loop = 0; loop < 3; loop++) {
		if (loop != 0)
			msleep(20);

		status = max96749_bridge_link_locked(serdes);
		if (status)
			break;
	}

	if (!status)
		dev_err(dev, "serdes %s link unlocked\n", serdes->chip_data->name);

	return 0;
}

static int max96749_select(struct serdes *serdes, int link)
{
	int ret;
	u32 i, status;
	struct serdes *deser;
	struct drm_panel *panel;
	struct serdes_panel *serdes_panel;

	/*0076 for linkA and 0086 for linkB*/
	if (link == SER_DUAL_LINK) {
		panel = serdes->serdes_bridge->panel;
		serdes_panel = container_of(panel, struct serdes_panel, panel);
		deser = serdes_panel->parent;

		serdes_reg_write(deser, 0x10, 0x00);
		serdes_set_bits(serdes, 0x45, DUAL_LINK_MODE,
					FIELD_PREP(DUAL_LINK_MODE, 1));
		serdes_set_bits(serdes, 0x0076, DIS_REM_CC,
				   FIELD_PREP(DIS_REM_CC, 0));
		serdes_set_bits(serdes, 0x0086, DIS_REM_CC,
				   FIELD_PREP(DIS_REM_CC, 0));
		SERDES_DBG_CHIP("%s: serdes %s change to use dual link\n",
			__func__, serdes->chip_data->name);
	} else if (link == SER_LINKA) {
		serdes_set_bits(serdes, 0x0076, DIS_REM_CC,
				   FIELD_PREP(DIS_REM_CC, 0));
		serdes_set_bits(serdes, 0x0086, DIS_REM_CC,
				   FIELD_PREP(DIS_REM_CC, 1));
		SERDES_DBG_CHIP("%s: only enable %s remote i2c of linkA\n", __func__,
				serdes->chip_data->name);
	} else if (link == SER_LINKB) {
		serdes_set_bits(serdes, 0x0076, DIS_REM_CC,
				   FIELD_PREP(DIS_REM_CC, 1));
		serdes_set_bits(serdes, 0x0086, DIS_REM_CC,
				   FIELD_PREP(DIS_REM_CC, 0));
		SERDES_DBG_CHIP("%s: only enable %s remote i2c of linkB\n", __func__,
				serdes->chip_data->name);
	} else if (link == SER_SPLITTER_MODE) {
		serdes_set_bits(serdes, 0x0076, DIS_REM_CC,
				   FIELD_PREP(DIS_REM_CC, 0));
		serdes_set_bits(serdes, 0x0086, DIS_REM_CC,
				   FIELD_PREP(DIS_REM_CC, 0));
		SERDES_DBG_CHIP("%s: enable %s remote i2c of linkA and linkB\n", __func__,
				serdes->chip_data->name);
	}

	for (i = 0; i < 20; i++) {
		msleep(20);
		ret = serdes_reg_read(serdes, 0x0021, &status);
		if (ret)
			continue;

		if (serdes->dual_link && link != SER_DUAL_LINK)
			return 0;

		switch (link) {
		case SER_DUAL_LINK:
		case SER_SPLITTER_MODE:
			if ((status & LINKA_LOCKED) &&
			    (status & LINKB_LOCKED))
				goto out;
			break;
		case SER_LINKA:
			if (status & LINKA_LOCKED)
				goto out;
			break;
		case SER_LINKB:
			if (status & LINKB_LOCKED)
				goto out;
			break;
		}
	}

	dev_info(serdes->dev, "%s: link lock timeout, mode=%d val=0x%x\n",
		__func__, link, status);
	return -1;

out:
	dev_info(serdes->dev, "%s: link locked, mode=%d, val=0x%x\n",
		__func__, link, status);

	return 0;
}

static int max96749_deselect(struct serdes *serdes, int link)
{
	struct serdes *deser;
	struct drm_panel *panel;
	struct serdes_panel *serdes_panel;
	struct serdes_bridge *serdes_bridge = serdes->serdes_bridge;

	if (link == SER_DUAL_LINK) {
		panel = serdes_bridge->panel;
		serdes_panel = container_of(panel, struct serdes_panel, panel);
		deser = serdes_panel->parent;

		serdes_reg_write(deser, 0x10, 0x11);
		serdes_set_bits(serdes, 0x45, DUAL_LINK_MODE,
						FIELD_PREP(DUAL_LINK_MODE, 0));

		SERDES_DBG_CHIP("%s: serdes %s disable dual link\n", __func__,
					serdes->chip_data->name);

	}

	return 0;
}

static struct serdes_chip_split_ops max96749_split_ops = {
	.select = max96749_select,
	.deselect = max96749_deselect,
};

static int max96749_bridge_attach(struct serdes *serdes)
{
	int ret;
	enum drm_connector_status status;

	if (max96749_bridge_link_locked(serdes))
		status = connector_status_connected;
	else {
		status = connector_status_disconnected;
		if (serdes->dual_link) {
			dev_info(serdes->dev, "serdes disconnect, try to change dual link\n");

			ret = max96749_select(serdes, SER_DUAL_LINK);
			if (ret) {
				dev_info(serdes->dev, "serdes disconnect, close dual link\n");
				max96749_deselect(serdes, SER_DUAL_LINK);
			} else {
				status = connector_status_connected;
			}
		}
	}

	serdes->serdes_bridge->status = status;

	return 0;
}

static enum drm_connector_status
max96749_bridge_detect(struct serdes *serdes)
{
	struct serdes_bridge *serdes_bridge = serdes->serdes_bridge;
	enum drm_connector_status status = connector_status_connected;

	if (!drm_kms_helper_is_poll_worker())
		return serdes_bridge->status;

	if (!max96749_bridge_link_locked(serdes)) {
		status = connector_status_disconnected;
		goto out;
	}

	if (extcon_get_state(serdes->extcon, EXTCON_JACK_VIDEO_OUT)) {
		u32 dprx_trn_status2;

		if (atomic_cmpxchg(&serdes_bridge->triggered, 1, 0)) {
			status = connector_status_disconnected;
			SERDES_DBG_CHIP("1 status=%d state=%d\n", status, serdes->extcon->state);
			goto out;
		}

		if (serdes_reg_read(serdes, 0x641a, &dprx_trn_status2)) {
			status = connector_status_disconnected;
			SERDES_DBG_CHIP("2 status=%d state=%d\n", status, serdes->extcon->state);
			goto out;
		}

		if ((dprx_trn_status2 & DPRX_TRAIN_STATE) != DPRX_TRAIN_STATE) {
			dev_err(serdes->dev, "Training State: 0x%lx\n",
				FIELD_GET(DPRX_TRAIN_STATE, dprx_trn_status2));
			status = connector_status_disconnected;
			SERDES_DBG_CHIP("3 status=%d state=%d\n", status, serdes->extcon->state);
			goto out;
		}
	} else {
		atomic_set(&serdes_bridge->triggered, 0);
		SERDES_DBG_CHIP("4 status=%d state=%d\n", status, serdes->extcon->state);
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

static int max96749_bridge_pre_enable(struct serdes *serdes)
{
	int ret = 0;
	struct serdes_bridge *serdes_bridge = serdes->serdes_bridge;

	if (serdes->dual_link) {
		ret = max96749_select(serdes, SER_DUAL_LINK);
		if (ret)
			atomic_set(&serdes_bridge->triggered, 1);
	}

	SERDES_DBG_CHIP("%s: serdes chip %s ret=%d\n", __func__, serdes->chip_data->name, ret);
	return ret;
}

static int max96749_bridge_enable(struct serdes *serdes)
{
	int ret = 0;

	return ret;
}

static int max96749_bridge_disable(struct serdes *serdes)
{
	int ret = 0;

	if (serdes->dual_link)
		max96749_deselect(serdes, SER_DUAL_LINK);

	SERDES_DBG_CHIP("%s: serdes chip %s ret=%d\n", __func__, serdes->chip_data->name, ret);
	return ret;
}

static int max96749_bridge_post_disable(struct serdes *serdes)
{
	int ret = 0;

	return ret;
}

static struct serdes_chip_bridge_ops max96749_bridge_ops = {
	.init = max96749_bridge_init,
	.attach = max96749_bridge_attach,
	.detect = max96749_bridge_detect,
	.pre_enable = max96749_bridge_pre_enable,
	.enable = max96749_bridge_enable,
	.disable = max96749_bridge_disable,
	.post_disable = max96749_bridge_post_disable,
};

static int max96749_chip_init(struct serdes *serdes)
{
	if (serdes->enable_gpio) {
		gpiod_direction_output(serdes->enable_gpio, 1);
		msleep(50);
	}

	SERDES_DBG_CHIP("%s serdes %s chip init\n",
			dev_name(serdes->dev), serdes->chip_data->name);

	return 0;
}

static int max96749_pinctrl_set_mux(struct serdes *serdes,
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
		struct serdes_function_data *data = func->data;

		for (i = 0; i < npins; i++) {
#if KERNEL_VERSION(6, 12, 0) > LINUX_VERSION_CODE
			offset = grp->pins[i] - pinctrl->pin_base;
#else
			offset = grp->grp.pins[i] - pinctrl->pin_base;
#endif

			serdes_set_bits(serdes, GPIO_A_REG(offset), GPIO_OUT_DIS,
					FIELD_PREP(GPIO_OUT_DIS, data->gpio_out_dis));
			serdes_set_bits(serdes, GPIO_B_REG(offset), OUT_TYPE,
					FIELD_PREP(OUT_TYPE, 1));
			if (data->gpio_tx_en_a || data->gpio_tx_en_b)
				serdes_set_bits(serdes, GPIO_B_REG(offset), GPIO_TX_ID,
						FIELD_PREP(GPIO_TX_ID, data->gpio_tx_id));
			if (data->gpio_rx_en_a || data->gpio_rx_en_b)
				serdes_set_bits(serdes, GPIO_C_REG(offset), GPIO_RX_ID,
						FIELD_PREP(GPIO_RX_ID, data->gpio_rx_id));
			serdes_set_bits(serdes, GPIO_D_REG(offset),
					GPIO_TX_EN_A | GPIO_TX_EN_B | GPIO_IO_RX_EN |
					GPIO_RX_EN_A | GPIO_RX_EN_B,
					FIELD_PREP(GPIO_TX_EN_A, data->gpio_tx_en_a) |
					FIELD_PREP(GPIO_TX_EN_B, data->gpio_tx_en_b) |
					FIELD_PREP(GPIO_RX_EN_A, data->gpio_rx_en_a) |
					FIELD_PREP(GPIO_RX_EN_B, data->gpio_rx_en_b) |
					FIELD_PREP(GPIO_IO_RX_EN, data->gpio_io_rx_en));
		}
	}

	return 0;
}

static int max96749_pinctrl_config_get(struct serdes *serdes,
				       unsigned int pin, unsigned long *config)
{
	enum pin_config_param param = pinconf_to_config_param(*config);
	unsigned int gpio_a_reg, gpio_b_reg;
	u16 arg = 0;

	serdes_reg_read(serdes, GPIO_A_REG(pin), &gpio_a_reg);
	serdes_reg_read(serdes, GPIO_B_REG(pin), &gpio_b_reg);

	SERDES_DBG_CHIP("%s: serdes chip %s pin=%d param=%d\n",
		__func__, serdes->chip_data->name, pin, param);

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

static int max96749_pinctrl_config_set(struct serdes *serdes,
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

static struct serdes_chip_pinctrl_ops max96749_pinctrl_ops = {
	.pin_config_get = max96749_pinctrl_config_get,
	.pin_config_set = max96749_pinctrl_config_set,
	.set_mux = max96749_pinctrl_set_mux,
};

static int max96749_gpio_direction_input(struct serdes *serdes, int gpio)
{
	return 0;
}

static int max96749_gpio_direction_output(struct serdes *serdes, int gpio, int value)
{
	return 0;
}

static int max96749_gpio_get_level(struct serdes *serdes, int gpio)
{
	return 0;
}

static int max96749_gpio_set_level(struct serdes *serdes, int gpio, int value)
{
	return 0;
}

static int max96749_gpio_set_config(struct serdes *serdes, int gpio, unsigned long config)
{
	return 0;
}

static int max96749_gpio_to_irq(struct serdes *serdes, int gpio)
{
	return 0;
}

static struct serdes_chip_gpio_ops max96749_gpio_ops = {
	.direction_input = max96749_gpio_direction_input,
	.direction_output = max96749_gpio_direction_output,
	.get_level = max96749_gpio_get_level,
	.set_level = max96749_gpio_set_level,
	.set_config = max96749_gpio_set_config,
	.to_irq = max96749_gpio_to_irq,
};

static const struct check_reg_data max96749_improtant_reg[10] = {
	{
		"MAX96749 LINK LOCK",
		{ 0x0013, (1 << 3) },
	}, {
		"MAX96749 LINKA LOCK",
		{ 0x002A, (1 << 0) },
	}, {
		"MAX96749 LINKB LOCK",
		{ 0x0034, (1 << 0) },
	}, {
		"MAX96749 X PCLK DET",
		{ 0x0102, (1 << 7) },
	}, {
		"MAX96749 Y PCLK DET",
		{ 0x0112, (1 << 7) },
	},
};

static int max96749_check_hw_state(struct serdes *serdes)
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

static int max96749_check_reg(struct serdes *serdes)
{
	int i =  0, ret = 0;
	unsigned int val = 0;

	for (i = 0; i < ARRAY_SIZE(max96749_improtant_reg); i++) {
		if (!max96749_improtant_reg[i].seq.reg)
			break;

		ret = serdes_reg_read(serdes, max96749_improtant_reg[i].seq.reg, &val);
		if (!ret && !(val & max96749_improtant_reg[i].seq.def)
		    && (!atomic_read(&serdes->flag_early_suspend)))
			dev_info(serdes->dev, "warning %s %s reg[0x%x] = 0x%x\n", __func__,
				 max96749_improtant_reg[i].name,
				 max96749_improtant_reg[i].seq.reg, val);
	}

	return 0;
}

static struct serdes_check_state_ops max96749_check_ops = {
	.check_hw  = max96749_check_hw_state,
	.check_reg = max96749_check_reg,
};

static int max96749_pm_suspend(struct serdes *serdes)
{
	return 0;
}

static int max96749_pm_resume(struct serdes *serdes)
{
	return 0;
}

static struct serdes_chip_pm_ops max96749_pm_ops = {
	.suspend = max96749_pm_suspend,
	.resume = max96749_pm_resume,
};

static int max96749_irq_lock_handle(struct serdes *serdes)
{
	return IRQ_HANDLED;
}

static int max96749_irq_err_handle(struct serdes *serdes)
{
	return IRQ_HANDLED;
}

static struct serdes_chip_irq_ops max96749_irq_ops = {
	.lock_handle = max96749_irq_lock_handle,
	.err_handle = max96749_irq_err_handle,
};

struct serdes_chip_data serdes_max96749_data = {
	.name		= "max96749",
	.serdes_type	= TYPE_SER,
	.serdes_id	= MAXIM_ID_MAX96749,
	.connector_type	= DRM_MODE_CONNECTOR_eDP,
	.chip_init	= max96749_chip_init,
	.regmap_config	= &max96749_regmap_config,
	.pinctrl_info	= &max96749_pinctrl_info,
	.bridge_ops	= &max96749_bridge_ops,
	.pinctrl_ops	= &max96749_pinctrl_ops,
	.gpio_ops	= &max96749_gpio_ops,
	.split_ops	= &max96749_split_ops,
	.check_ops	= &max96749_check_ops,
	.pm_ops		= &max96749_pm_ops,
	.irq_ops	= &max96749_irq_ops,
};
EXPORT_SYMBOL_GPL(serdes_max96749_data);

MODULE_LICENSE("GPL");
