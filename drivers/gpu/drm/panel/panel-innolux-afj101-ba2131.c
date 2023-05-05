// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019-2020 Kali Prasad <kprasadvnsi@protonmail.com>
 */

#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define _INIT_CMD(...) { \
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

struct panel_init_cmd {
	u8 dtype;
	u8 wait;
	u8 dlen;
	const char *data;
};

static const char * const regulator_names[] = {
	"dvdd",
	"avdd",
	"cvdd"
};

struct afj101_ba2131 {
	struct drm_panel	panel;
	struct mipi_dsi_device	*dsi;

	struct regulator_bulk_data supplies[ARRAY_SIZE(regulator_names)];
	struct gpio_desc	*reset;
	struct gpio_desc	*enable;
};

static inline struct afj101_ba2131 *panel_to_sl101_pn27d1665(struct drm_panel *panel)
{
	return container_of(panel, struct afj101_ba2131, panel);
}

/*
 * Display manufacturer failed to provide init sequencing according to
 * https://chromium-review.googlesource.com/c/chromiumos/third_party/coreboot/+/892065/
 * so the init sequence stems from a register dump of a working panel.
 */
static const struct panel_init_cmd afj101_ba2131_init_cmds[] = {
	{ .dtype = 0x39, .wait =  0x00, .dlen =  0x04, .data = (char[]){ 0xFF, 0x98, 0x81, 0x03 }},

	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x01, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x02, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x03, 0x53 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x04, 0xD3 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x05, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x06, 0x0D }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x07, 0x08 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x08, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x09, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x0a, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x0b, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x0c, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x0d, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x0e, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x0f, 0x28 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x10, 0x28 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x11, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x12, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x13, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x14, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x15, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x16, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x17, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x18, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x19, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x1a, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x1b, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x1c, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x1d, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x1e, 0x40 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x1f, 0x80 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x20, 0x06 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x21, 0x01 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x22, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x23, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x24, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x25, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x26, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x27, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x28, 0x33 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x29, 0x33 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x2a, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x2b, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x2c, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x2d, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x2e, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x2f, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x30, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x31, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x32, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x33, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x34, 0x03 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x35, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x36, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x37, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x38, 0x96 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x39, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x3a, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x3b, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x3c, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x3d, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x3e, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x3f, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x40, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x41, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x42, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x43, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x44, 0x00 }},

	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x50, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x51, 0x23 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x52, 0x45 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x53, 0x67 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x54, 0x89 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x55, 0xAB }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x56, 0x01 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x57, 0x23 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x58, 0x45 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x59, 0x67 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x5a, 0x89 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x5b, 0xAB }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x5c, 0xCD }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x5d, 0xEF }},

	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x5e, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x5f, 0x08 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x60, 0x08 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x61, 0x06 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x62, 0x06 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x63, 0x01 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x64, 0x01 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x65, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x66, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x67, 0x02 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x68, 0x15 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x69, 0x15 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x6a, 0x14 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x6b, 0x14 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x6c, 0x0D }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x6d, 0x0D }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x6e, 0x0C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x6f, 0x0C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x70, 0x0F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x71, 0x0F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x72, 0x0E }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x73, 0x0E }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x74, 0x02 }},

	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x75, 0x08 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x76, 0x08 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x77, 0x06 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x78, 0x06 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x79, 0x01 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x7a, 0x01 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x7b, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x7c, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x7d, 0x02 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x7e, 0x15 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x7f, 0x15 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x80, 0x14 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x81, 0x14 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x82, 0x0D }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x83, 0x0D }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x84, 0x0C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x85, 0x0C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x86, 0x0F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x87, 0x0F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x88, 0x0E }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x89, 0x0E }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x8A, 0x02 }},

	{ .dtype = 0x39, .wait =  0x00, .dlen =  0x04, .data = (char[]){ 0xFF, 0x98, 0x81, 0x04 }},

	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x6E, 0x2B }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x6F, 0x37 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x3A, 0xA4 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x8D, 0x1A }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x87, 0xBA }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xB2, 0xD1 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x88, 0x0B }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x38, 0x01 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x39, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xB5, 0x07 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x31, 0x75 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x3B, 0x98 }},

	{ .dtype = 0x39, .wait =  0x00, .dlen =  0x04, .data = (char[]){ 0xFF, 0x98, 0x81, 0x01 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x43, 0x33 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x22, 0x0A }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x31, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x53, 0x48 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x55, 0x48 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x50, 0x99 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x51, 0x94 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x60, 0x10 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x62, 0x20 }},

	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA0, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA1, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA2, 0x15 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA3, 0x14 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA4, 0x1B }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA5, 0x2F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA6, 0x25 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA7, 0x24 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA8, 0x80 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xA9, 0x1F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xAA, 0x2C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xAB, 0x6C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xAC, 0x16 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xAD, 0x14 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xAE, 0x4D }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xAF, 0x20 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xB0, 0x29 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xB1, 0x4F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xB2, 0x5F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xB3, 0x23 }},

	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC0, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC1, 0x2E }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC2, 0x3B }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC3, 0x15 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC4, 0x16 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC5, 0x28 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC6, 0x1A }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC7, 0x1C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC8, 0xA7 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xC9, 0x1B }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xCA, 0x28 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xCB, 0x92 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xCC, 0x1F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xCD, 0x1C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xCE, 0x4B }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xCF, 0x1F }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xD0, 0x28 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xD1, 0x4E }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xD2, 0x5C }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0xD3, 0x23 }},

	{ .dtype = 0x39, .wait =  0x00, .dlen =  0x04, .data = (char[]){ 0xFF, 0x98, 0x81, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x11, 0x00 }},
	{ .dtype = 0x15, .wait =  0x78, .dlen =  0x02, .data = (char[]){ 0x29, 0x00 }},
	{ .dtype = 0x15, .wait =  0x00, .dlen =  0x02, .data = (char[]){ 0x35, 0x00 }},
};


static int afj101_ba2131_prepare(struct drm_panel *panel)
{
	struct afj101_ba2131 *ctx = panel_to_sl101_pn27d1665(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	unsigned int i;
	int ret;

	gpiod_set_value(ctx->enable, 1);
	msleep(25);

	gpiod_set_value(ctx->reset, 1);
	msleep(25);

	gpiod_set_value(ctx->reset, 0);
	msleep(200);

	for (i = 0; i < ARRAY_SIZE(afj101_ba2131_init_cmds); i++) {
		const struct panel_init_cmd *cmd = &afj101_ba2131_init_cmds[i];

		switch (cmd->dtype) {
			case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
			case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
			case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
			case MIPI_DSI_GENERIC_LONG_WRITE:
				ret = mipi_dsi_generic_write(dsi, cmd->data,
								cmd->dlen);
				break;
			case MIPI_DSI_DCS_SHORT_WRITE:
			case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
			case MIPI_DSI_DCS_LONG_WRITE:
				ret = mipi_dsi_dcs_write_buffer(dsi, cmd->data,
								cmd->dlen);
				break;
			default:
				return -EINVAL;
		}

		if (ret < 0)
			goto powerdown;

		if (cmd->wait)
				msleep(cmd->wait);
	}

	return 0;

powerdown:
	gpiod_set_value(ctx->reset, 1);
	msleep(50);

	// return regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	return 0;
}

static int afj101_ba2131_enable(struct drm_panel *panel)
{
	struct afj101_ba2131 *ctx = panel_to_sl101_pn27d1665(panel);
	int ret;

	msleep(150);

	ret = mipi_dsi_dcs_set_display_on(ctx->dsi);
	if (ret < 0)
		return ret;

	msleep(50);

	return 0;
}

static int afj101_ba2131_disable(struct drm_panel *panel)
{
	struct afj101_ba2131 *ctx = panel_to_sl101_pn27d1665(panel);

	return mipi_dsi_dcs_set_display_off(ctx->dsi);
}

static int afj101_ba2131_unprepare(struct drm_panel *panel)
{
	struct afj101_ba2131 *ctx = panel_to_sl101_pn27d1665(panel);
	int ret;

	ret = mipi_dsi_dcs_set_display_off(ctx->dsi);
	if (ret < 0)
		dev_err(panel->dev, "failed to set display off: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
	if (ret < 0)
		dev_err(panel->dev, "failed to enter sleep mode: %d\n", ret);

	msleep(200);

	gpiod_set_value(ctx->reset, 1);
	msleep(20);

	gpiod_set_value(ctx->enable, 0);
	msleep(20);

	// return regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	return 0;
}

static const struct drm_display_mode afj101_ba2131_default_mode = {
	.clock = 62100,

	.hdisplay    = 800,
	.hsync_start = 800 + 40,
	.hsync_end   = 800 + 40 + 5,
	.htotal      = 800 + 40 + 5 + 20,

	.vdisplay    = 1280,
	.vsync_start = 1280 + 30,
	.vsync_end   = 1280 + 30 + 5,
	.vtotal      = 1280 + 30 + 5 + 12,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
	.width_mm	= 136,
	.height_mm	= 217,
};

static int afj101_ba2131_get_modes(struct drm_panel *panel,
				  struct drm_connector *connector)
{
	struct afj101_ba2131 *ctx = panel_to_sl101_pn27d1665(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &afj101_ba2131_default_mode);
	if (!mode) {
		dev_err(&ctx->dsi->dev, "failed to add mode %ux%u@%u\n",
			afj101_ba2131_default_mode.hdisplay,
			afj101_ba2131_default_mode.vdisplay,
			drm_mode_vrefresh(&afj101_ba2131_default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs afj101_ba2131_funcs = {
	.disable = afj101_ba2131_disable,
	.unprepare = afj101_ba2131_unprepare,
	.prepare = afj101_ba2131_prepare,
	.enable = afj101_ba2131_enable,
	.get_modes = afj101_ba2131_get_modes,
};

static int afj101_ba2131_dsi_probe(struct mipi_dsi_device *dsi)
{
	struct afj101_ba2131 *ctx;
	int ret;

	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	ctx->enable = devm_gpiod_get(&dsi->dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enable)) {
		dev_err(&dsi->dev, "Couldn't get our enable GPIO\n");
		return PTR_ERR(ctx->enable);
	}

	ctx->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset)) {
		dev_err(&dsi->dev, "Couldn't get our reset GPIO\n");
		return PTR_ERR(ctx->reset);
	}

	drm_panel_init(&ctx->panel, &dsi->dev, &afj101_ba2131_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret){
		return ret;
	}

	drm_panel_add(&ctx->panel);

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_LPM;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static int afj101_ba2131_dsi_remove(struct mipi_dsi_device *dsi)
{
	struct afj101_ba2131 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id afj101_ba2131_of_match[] = {
	{ .compatible = "innolux,afj101-ba2131", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, afj101_ba2131_of_match);

static struct mipi_dsi_driver afj101_ba2131_driver = {
	.probe = afj101_ba2131_dsi_probe,
	.remove = afj101_ba2131_dsi_remove,
	.driver = {
		.name = "innolux-afj101-ba2131",
		.of_match_table = afj101_ba2131_of_match,
	},
};
module_mipi_dsi_driver(afj101_ba2131_driver);

MODULE_AUTHOR("Kali Prasad <kprasadvnsi@protonmail.com>");
MODULE_DESCRIPTION("Innolux AFJ101 BA2131 MIPI-DSI LCD panel");
MODULE_LICENSE("GPL");
