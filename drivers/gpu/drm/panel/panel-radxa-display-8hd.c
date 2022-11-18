// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022 Radxa Limited
 *
 * Author:
 * - Stephen Chen <stephen@radxa.com>
 */

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>

#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#define JADARD_JD9365DA_INIT_CMD_LEN	2

struct jadard_jd9365da_init_cmd {
	u8 data[JADARD_JD9365DA_INIT_CMD_LEN];
};

struct jadard_jd9365da_panel_desc {
	const struct drm_display_mode mode;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	const struct jadard_jd9365da_init_cmd *init_cmds;
	u32 num_init_cmds;
};

struct jadard_jd9365da {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct jadard_jd9365da_panel_desc *desc;

	struct regulator *vdd;
	struct regulator *vccio;
	struct gpio_desc *reset;
};

static inline struct jadard_jd9365da *panel_to_jadard_jd9365da(struct drm_panel *panel)
{
	return container_of(panel, struct jadard_jd9365da, panel);
}

static int jadard_jd9365da_prepare(struct drm_panel *panel)
{
	struct jadard_jd9365da *jadard_jd9365da = panel_to_jadard_jd9365da(panel);
	int ret;

	ret = regulator_enable(jadard_jd9365da->vccio);
	if (ret)
		return ret;

	ret = regulator_enable(jadard_jd9365da->vdd);
	if (ret)
		return ret;

	gpiod_set_value(jadard_jd9365da->reset, 1);
	msleep(120);

	return 0;
}

static int jadard_jd9365da_enable(struct drm_panel *panel)
{
	struct device *dev = panel->dev;
	struct jadard_jd9365da *jadard_jd9365da = panel_to_jadard_jd9365da(panel);
	const struct jadard_jd9365da_panel_desc *desc = jadard_jd9365da->desc;
	struct mipi_dsi_device *dsi = jadard_jd9365da->dsi;
	unsigned int i;
	int err;

	msleep(10);

	for (i = 0; i < desc->num_init_cmds; i++) {
		const struct jadard_jd9365da_init_cmd *cmd = &desc->init_cmds[i];

		err = mipi_dsi_dcs_write_buffer(dsi, cmd->data, JADARD_JD9365DA_INIT_CMD_LEN);
		if (err < 0)
			return err;
	}

	msleep(120);

	err = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (err < 0)
		DRM_DEV_ERROR(dev, "failed to exit sleep mode ret = %d\n", err);

	err =  mipi_dsi_dcs_set_display_on(dsi);
	if (err < 0)
		DRM_DEV_ERROR(dev, "failed to set display on ret = %d\n", err);

	return 0;
}

static int jadard_jd9365da_disable(struct drm_panel *panel)
{
	struct device *dev = panel->dev;
	struct jadard_jd9365da *jadard_jd9365da = panel_to_jadard_jd9365da(panel);
	int ret;

	ret = mipi_dsi_dcs_set_display_off(jadard_jd9365da->dsi);
	if (ret < 0)
		DRM_DEV_ERROR(dev, "failed to set display off: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(jadard_jd9365da->dsi);
	if (ret < 0)
		DRM_DEV_ERROR(dev, "failed to enter sleep mode: %d\n", ret);

	return 0;
}

static int jadard_jd9365da_unprepare(struct drm_panel *panel)
{
	struct jadard_jd9365da *jadard_jd9365da = panel_to_jadard_jd9365da(panel);

	gpiod_set_value(jadard_jd9365da->reset, 1);
	msleep(120);

	regulator_disable(jadard_jd9365da->vdd);
	regulator_disable(jadard_jd9365da->vccio);

	return 0;
}

static int jadard_jd9365da_get_modes(struct drm_panel *panel,
			    struct drm_connector *connector)
{
	struct jadard_jd9365da *jadard_jd9365da = panel_to_jadard_jd9365da(panel);
	const struct drm_display_mode *desc_mode = &jadard_jd9365da->desc->mode;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, desc_mode);
	if (!mode) {
		DRM_DEV_ERROR(&jadard_jd9365da->dsi->dev, "failed to add mode %ux%ux@%u\n",
			      desc_mode->hdisplay, desc_mode->vdisplay,
			      drm_mode_vrefresh(desc_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	return 1;
}

static const struct drm_panel_funcs jadard_jd9365da_funcs = {
	.prepare = jadard_jd9365da_prepare,
	.enable = jadard_jd9365da_enable,
	.disable = jadard_jd9365da_disable,
	.unprepare = jadard_jd9365da_unprepare,
	.get_modes = jadard_jd9365da_get_modes,
};

static const struct jadard_jd9365da_init_cmd radxa_display_8hd_init_cmds[] = {
	{ .data = { 0xE0, 0x00 } },
	{ .data = { 0xE1, 0x93 } },
	{ .data = { 0xE2, 0x65 } },
	{ .data = { 0xE3, 0xF8 } },
	{ .data = { 0x80, 0x03 } },
	{ .data = { 0xE0, 0x01 } },
	{ .data = { 0x00, 0x00 } },
	{ .data = { 0x01, 0x7E } },
	{ .data = { 0x03, 0x00 } },
	{ .data = { 0x04, 0x65 } },
	{ .data = { 0x0C, 0x74 } },
	{ .data = { 0x17, 0x00 } },
	{ .data = { 0x18, 0xB7 } },
	{ .data = { 0x19, 0x00 } },
	{ .data = { 0x1A, 0x00 } },
	{ .data = { 0x1B, 0xB7 } },
	{ .data = { 0x1C, 0x00 } },
	{ .data = { 0x24, 0xFE } },
	{ .data = { 0x37, 0x19 } },
	{ .data = { 0x38, 0x05 } },
	{ .data = { 0x39, 0x00 } },
	{ .data = { 0x3A, 0x01 } },
	{ .data = { 0x3B, 0x01 } },
	{ .data = { 0x3C, 0x70 } },
	{ .data = { 0x3D, 0xFF } },
	{ .data = { 0x3E, 0xFF } },
	{ .data = { 0x3F, 0xFF } },
	{ .data = { 0x40, 0x06 } },
	{ .data = { 0x41, 0xA0 } },
	{ .data = { 0x43, 0x1E } },
	{ .data = { 0x44, 0x0F } },
	{ .data = { 0x45, 0x28 } },
	{ .data = { 0x4B, 0x04 } },
	{ .data = { 0x55, 0x02 } },
	{ .data = { 0x56, 0x01 } },
	{ .data = { 0x57, 0xA9 } },
	{ .data = { 0x58, 0x0A } },
	{ .data = { 0x59, 0x0A } },
	{ .data = { 0x5A, 0x37 } },
	{ .data = { 0x5B, 0x19 } },
	{ .data = { 0x5D, 0x78 } },
	{ .data = { 0x5E, 0x63 } },
	{ .data = { 0x5F, 0x54 } },
	{ .data = { 0x60, 0x49 } },
	{ .data = { 0x61, 0x45 } },
	{ .data = { 0x62, 0x38 } },
	{ .data = { 0x63, 0x3D } },
	{ .data = { 0x64, 0x28 } },
	{ .data = { 0x65, 0x43 } },
	{ .data = { 0x66, 0x41 } },
	{ .data = { 0x67, 0x43 } },
	{ .data = { 0x68, 0x62 } },
	{ .data = { 0x69, 0x50 } },
	{ .data = { 0x6A, 0x57 } },
	{ .data = { 0x6B, 0x49 } },
	{ .data = { 0x6C, 0x44 } },
	{ .data = { 0x6D, 0x37 } },
	{ .data = { 0x6E, 0x23 } },
	{ .data = { 0x6F, 0x10 } },
	{ .data = { 0x70, 0x78 } },
	{ .data = { 0x71, 0x63 } },
	{ .data = { 0x72, 0x54 } },
	{ .data = { 0x73, 0x49 } },
	{ .data = { 0x74, 0x45 } },
	{ .data = { 0x75, 0x38 } },
	{ .data = { 0x76, 0x3D } },
	{ .data = { 0x77, 0x28 } },
	{ .data = { 0x78, 0x43 } },
	{ .data = { 0x79, 0x41 } },
	{ .data = { 0x7A, 0x43 } },
	{ .data = { 0x7B, 0x62 } },
	{ .data = { 0x7C, 0x50 } },
	{ .data = { 0x7D, 0x57 } },
	{ .data = { 0x7E, 0x49 } },
	{ .data = { 0x7F, 0x44 } },
	{ .data = { 0x80, 0x37 } },
	{ .data = { 0x81, 0x23 } },
	{ .data = { 0x82, 0x10 } },
	{ .data = { 0xE0, 0x02 } },
	{ .data = { 0x00, 0x47 } },
	{ .data = { 0x01, 0x47 } },
	{ .data = { 0x02, 0x45 } },
	{ .data = { 0x03, 0x45 } },
	{ .data = { 0x04, 0x4B } },
	{ .data = { 0x05, 0x4B } },
	{ .data = { 0x06, 0x49 } },
	{ .data = { 0x07, 0x49 } },
	{ .data = { 0x08, 0x41 } },
	{ .data = { 0x09, 0x1F } },
	{ .data = { 0x0A, 0x1F } },
	{ .data = { 0x0B, 0x1F } },
	{ .data = { 0x0C, 0x1F } },
	{ .data = { 0x0D, 0x1F } },
	{ .data = { 0x0E, 0x1F } },
	{ .data = { 0x0F, 0x5F } },
	{ .data = { 0x10, 0x5F } },
	{ .data = { 0x11, 0x57 } },
	{ .data = { 0x12, 0x77 } },
	{ .data = { 0x13, 0x35 } },
	{ .data = { 0x14, 0x1F } },
	{ .data = { 0x15, 0x1F } },
	{ .data = { 0x16, 0x46 } },
	{ .data = { 0x17, 0x46 } },
	{ .data = { 0x18, 0x44 } },
	{ .data = { 0x19, 0x44 } },
	{ .data = { 0x1A, 0x4A } },
	{ .data = { 0x1B, 0x4A } },
	{ .data = { 0x1C, 0x48 } },
	{ .data = { 0x1D, 0x48 } },
	{ .data = { 0x1E, 0x40 } },
	{ .data = { 0x1F, 0x1F } },
	{ .data = { 0x20, 0x1F } },
	{ .data = { 0x21, 0x1F } },
	{ .data = { 0x22, 0x1F } },
	{ .data = { 0x23, 0x1F } },
	{ .data = { 0x24, 0x1F } },
	{ .data = { 0x25, 0x5F } },
	{ .data = { 0x26, 0x5F } },
	{ .data = { 0x27, 0x57 } },
	{ .data = { 0x28, 0x77 } },
	{ .data = { 0x29, 0x35 } },
	{ .data = { 0x2A, 0x1F } },
	{ .data = { 0x2B, 0x1F } },
	{ .data = { 0x58, 0x40 } },
	{ .data = { 0x59, 0x00 } },
	{ .data = { 0x5A, 0x00 } },
	{ .data = { 0x5B, 0x10 } },
	{ .data = { 0x5C, 0x06 } },
	{ .data = { 0x5D, 0x40 } },
	{ .data = { 0x5E, 0x01 } },
	{ .data = { 0x5F, 0x02 } },
	{ .data = { 0x60, 0x30 } },
	{ .data = { 0x61, 0x01 } },
	{ .data = { 0x62, 0x02 } },
	{ .data = { 0x63, 0x03 } },
	{ .data = { 0x64, 0x6B } },
	{ .data = { 0x65, 0x05 } },
	{ .data = { 0x66, 0x0C } },
	{ .data = { 0x67, 0x73 } },
	{ .data = { 0x68, 0x09 } },
	{ .data = { 0x69, 0x03 } },
	{ .data = { 0x6A, 0x56 } },
	{ .data = { 0x6B, 0x08 } },
	{ .data = { 0x6C, 0x00 } },
	{ .data = { 0x6D, 0x04 } },
	{ .data = { 0x6E, 0x04 } },
	{ .data = { 0x6F, 0x88 } },
	{ .data = { 0x70, 0x00 } },
	{ .data = { 0x71, 0x00 } },
	{ .data = { 0x72, 0x06 } },
	{ .data = { 0x73, 0x7B } },
	{ .data = { 0x74, 0x00 } },
	{ .data = { 0x75, 0xF8 } },
	{ .data = { 0x76, 0x00 } },
	{ .data = { 0x77, 0xD5 } },
	{ .data = { 0x78, 0x2E } },
	{ .data = { 0x79, 0x12 } },
	{ .data = { 0x7A, 0x03 } },
	{ .data = { 0x7B, 0x00 } },
	{ .data = { 0x7C, 0x00 } },
	{ .data = { 0x7D, 0x03 } },
	{ .data = { 0x7E, 0x7B } },
	{ .data = { 0xE0, 0x04 } },
	{ .data = { 0x00, 0x0E } },
	{ .data = { 0x02, 0xB3 } },
	{ .data = { 0x09, 0x60 } },
	{ .data = { 0x0E, 0x2A } },
	{ .data = { 0x36, 0x59 } },
	{ .data = { 0xE0, 0x00 } },
};

static const struct jadard_jd9365da_panel_desc radxa_display_8hd_desc = {
	.mode = {
		.clock		= 70000,

		.hdisplay	= 800,
		.hsync_start	= 800 + 40,
		.hsync_end	= 800 + 40 + 18,
		.htotal		= 800 + 40 + 18 + 20,

		.vdisplay	= 1280,
		.vsync_start	= 1280 + 20,
		.vsync_end	= 1280 + 20 + 4,
		.vtotal		= 1280 + 20 + 4 + 20,

		.width_mm	= 127,
		.height_mm	= 199,
		.type		= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.init_cmds = radxa_display_8hd_init_cmds,
	.num_init_cmds = ARRAY_SIZE(radxa_display_8hd_init_cmds),
};

static int radxa_display_8hd_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct jadard_jd9365da_panel_desc *desc;
	struct jadard_jd9365da *jadard_jd9365da;
	int ret;

	jadard_jd9365da = devm_kzalloc(&dsi->dev, sizeof(jadard_jd9365da), GFP_KERNEL);
	if (!jadard_jd9365da)
		return -ENOMEM;

	desc = of_device_get_match_data(dev);
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_EOT_PACKET;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;

	jadard_jd9365da->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(jadard_jd9365da->reset)) {
		DRM_DEV_ERROR(&dsi->dev, "failed to get reset GPIO\n");
		return PTR_ERR(jadard_jd9365da->reset);
	}

	jadard_jd9365da->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(jadard_jd9365da->vdd)) {
		DRM_DEV_ERROR(&dsi->dev, "failed to get vdd regulator\n");
		return PTR_ERR(jadard_jd9365da->vdd);
	}

	jadard_jd9365da->vccio = devm_regulator_get(dev, "vccio");
	if (IS_ERR(jadard_jd9365da->vccio)) {
		DRM_DEV_ERROR(&dsi->dev, "failed to get vccio regulator");
		return PTR_ERR(jadard_jd9365da->vccio);
	}

	drm_panel_init(&jadard_jd9365da->panel, dev, &jadard_jd9365da_funcs,
				DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&jadard_jd9365da->panel);
	if (ret)
		return ret;

	drm_panel_add(&jadard_jd9365da->panel);

	mipi_dsi_set_drvdata(dsi, jadard_jd9365da);
	jadard_jd9365da->dsi = dsi;
	jadard_jd9365da->desc = desc;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&jadard_jd9365da->panel);

	return ret;
}

static int radxa_display_8hd_remove(struct mipi_dsi_device *dsi)
{
	struct jadard_jd9365da *jadard_jd9365da = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&jadard_jd9365da->panel);

	return 0;
}

static const struct of_device_id radxa_display_8hd_of_match[] = {
	{ .compatible = "radxa,display-8hd", .data = &radxa_display_8hd_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, radxa_display_8hd_of_match);

static struct mipi_dsi_driver radxa_display_8hd_driver = {
	.probe = radxa_display_8hd_probe,
	.remove = radxa_display_8hd_remove,
	.driver = {
		.name = "radxa_display_8hd",
		.of_match_table = radxa_display_8hd_of_match,
	},
};
module_mipi_dsi_driver(radxa_display_8hd_driver);

MODULE_AUTHOR("Stephen Chen <stephen@radxa.com>");
MODULE_DESCRIPTION("Radxa Display 8HD panel driver");
MODULE_LICENSE("GPL v2");
