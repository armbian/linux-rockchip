// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd.
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include "rk628_csi.h"
#include "rk628_dsi.h"
#include "rk628_mipi_dphy.h"

static inline void testif_testclk_assert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? GRF_MIPI_TX1_CON : GRF_MIPI_TX0_CON,
			   PHY_TESTCLK, PHY_TESTCLK);
	udelay(1);
}

static inline void testif_testclk_deassert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? GRF_MIPI_TX1_CON : GRF_MIPI_TX0_CON,
			   PHY_TESTCLK, 0);
	udelay(1);
}

void rk628_testif_testclr_assert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? GRF_MIPI_TX1_CON : GRF_MIPI_TX0_CON,
			   PHY_TESTCLR, PHY_TESTCLR);
	udelay(1);
}
EXPORT_SYMBOL(rk628_testif_testclr_assert);

void rk628_testif_testclr_deassert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? GRF_MIPI_TX1_CON : GRF_MIPI_TX0_CON,
			   PHY_TESTCLR, 0);
	udelay(1);
}
EXPORT_SYMBOL(rk628_testif_testclr_deassert);

static inline void testif_testen_assert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? GRF_MIPI_TX1_CON : GRF_MIPI_TX0_CON,
			   PHY_TESTEN, PHY_TESTEN);
	udelay(1);
}

static inline void testif_testen_deassert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? GRF_MIPI_TX1_CON : GRF_MIPI_TX0_CON,
			   PHY_TESTEN, 0);
	udelay(1);
}

static inline void testif_set_data(struct rk628 *rk628, u8 data, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? GRF_MIPI_TX1_CON : GRF_MIPI_TX0_CON,
			   PHY_TESTDIN_MASK, PHY_TESTDIN(data));
	udelay(1);
}

static inline u8 testif_get_data(struct rk628 *rk628, uint8_t mipi_id)
{
	u32 data = 0;

	rk628_i2c_read(rk628, mipi_id ? GRF_DPHY1_STATUS : GRF_DPHY0_STATUS, &data);

	return data >> PHY_TESTDOUT_SHIFT;
}

static void testif_test_code_write(struct rk628 *rk628, u8 test_code, uint8_t mipi_id)
{
	testif_testclk_assert(rk628, mipi_id);
	testif_set_data(rk628, test_code, mipi_id);
	testif_testen_assert(rk628, mipi_id);
	testif_testclk_deassert(rk628, mipi_id);
	testif_testen_deassert(rk628, mipi_id);
}

static void testif_test_data_write(struct rk628 *rk628, u8 test_data, uint8_t mipi_id)
{
	testif_testclk_deassert(rk628, mipi_id);
	testif_set_data(rk628, test_data, mipi_id);
	testif_testclk_assert(rk628, mipi_id);
}

u8 rk628_testif_write(struct rk628 *rk628, u8 test_code, u8 test_data, uint8_t mipi_id)
{
	u8 monitor_data;

	testif_test_code_write(rk628, test_code, mipi_id);
	testif_test_data_write(rk628, test_data, mipi_id);
	monitor_data = testif_get_data(rk628, mipi_id);

	dev_dbg(rk628->dev, "test_code=0x%02x, mipi dphy%x", test_code, mipi_id);
	dev_dbg(rk628->dev, "test_data=0x%02x, mipi dphy%x", test_data, mipi_id);
	dev_dbg(rk628->dev, "monitor_data=0x%02x, mipi dphy%x\n", monitor_data, mipi_id);

	return monitor_data;
}
EXPORT_SYMBOL(rk628_testif_write);

static void rk628_testif_set_timing(struct rk628 *rk628, u8 addr,
			      u8 max, u8 val, uint8_t mipi_id)
{
	if (val > max)
		return;

	rk628_testif_write(rk628, addr, (max + 1) | val, mipi_id);
}

u8 rk628_testif_read(struct rk628 *rk628, u8 test_code, uint8_t mipi_id)
{
	u8 test_data;

	testif_test_code_write(rk628, test_code, mipi_id);
	test_data = testif_get_data(rk628, mipi_id);
	testif_test_data_write(rk628, test_data, mipi_id);

	return test_data;
}
EXPORT_SYMBOL(rk628_testif_read);

static inline void mipi_dphy_enablelane_assert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? CSITX1_DPHY_CTRL : CSITX_DPHY_CTRL,
				CSI_DPHY_EN_MASK, CSI_DPHY_EN(rk628->dphy_lane_en));
	udelay(1);
}

static inline void mipi_dphy_enablelane_deassert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? CSITX1_DPHY_CTRL : CSITX_DPHY_CTRL,
				CSI_DPHY_EN_MASK, 0);
	udelay(1);
}

static inline void mipi_dphy_enableclk_assert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? CSITX1_DPHY_CTRL : CSITX_DPHY_CTRL,
				DPHY_ENABLECLK, DPHY_ENABLECLK);
	udelay(1);
}

static inline void mipi_dphy_enableclk_deassert(struct rk628 *rk628, uint8_t mipi_id)
{
	rk628_i2c_update_bits(rk628, mipi_id ? CSITX1_DPHY_CTRL : CSITX_DPHY_CTRL,
				DPHY_ENABLECLK, 0);
	udelay(1);
}

static inline void mipi_dphy_shutdownz_assert(struct rk628 *rk628)
{
	rk628_i2c_update_bits(rk628, GRF_MIPI_TX0_CON, CSI_PHYSHUTDOWNZ, 0);
	udelay(1);
}

static inline void mipi_dphy_shutdownz_deassert(struct rk628 *rk628)
{
	rk628_i2c_update_bits(rk628, GRF_MIPI_TX0_CON, CSI_PHYSHUTDOWNZ,
			CSI_PHYSHUTDOWNZ);
	udelay(1);
}

static inline void mipi_dphy_rstz_assert(struct rk628 *rk628)
{
	rk628_i2c_update_bits(rk628, GRF_MIPI_TX0_CON, CSI_PHYRSTZ, 0);
	udelay(1);
}

static inline void mipi_dphy_rstz_deassert(struct rk628 *rk628)
{
	rk628_i2c_update_bits(rk628, GRF_MIPI_TX0_CON, CSI_PHYRSTZ,
			CSI_PHYRSTZ);
	udelay(1);
}

static void rk628_mipi_dphy_set_timing(struct rk628 *rk628, int lane_mbps, uint8_t mipi_id)
{
	const struct {
		unsigned int min_lane_mbps;
		unsigned int max_lane_mbps;
		u8 clk_lp;
		u8 clk_hs_prepare;
		u8 clk_hs_zero;
		u8 clk_hs_trail;
		u8 clk_post;
		u8 data_lp;
		u8 data_hs_prepare;
		u8 data_hs_zero;
		u8 data_hs_trail;
	} timing_table[] = {
		{800, 899, 0x07, 0x30, 0x25, 0x3c, 0x0f, 0x07, 0x40, 0x09, 0x40},
		{1100, 1249, 0x0a, 0x43, 0x2c, 0x50, 0x0f, 0x0a, 0x43, 0x10, 0x55},
		{1250, 1349, 0x0b, 0x43, 0x2c, 0x50, 0x0f, 0x0b, 0x53, 0x10, 0x5b},
		{1350, 1449, 0x0c, 0x43, 0x36, 0x60, 0x0f, 0x0c, 0x53, 0x10, 0x65},
		{1450, 1500, 0x0f, 0x60, 0x31, 0x60, 0x0f, 0x0e, 0x60, 0x11, 0x6a},
		{1750, 2050, 0x10, 0x70, 0x3f, 0x7f, 0x1f, 0x10, 0x70, 0x1c, 0x7f},
	};
	unsigned int index;

	// These ranges use the controller's internal automatically calculated timing.
	if (lane_mbps < 800 || (lane_mbps >= 900 && lane_mbps < 1100))
		return;

	if (lane_mbps < timing_table[0].min_lane_mbps)
		return;

	for (index = 0; index < ARRAY_SIZE(timing_table); index++)
		if (lane_mbps >= timing_table[index].min_lane_mbps &&
		    lane_mbps < timing_table[index].max_lane_mbps)
			break;

	if (index == ARRAY_SIZE(timing_table))
		--index;

	rk628_testif_set_timing(rk628, 0x60, 0x3f, timing_table[index].clk_lp, mipi_id);
	rk628_testif_set_timing(rk628, 0x61, 0x7f, timing_table[index].clk_hs_prepare, mipi_id);
	rk628_testif_set_timing(rk628, 0x62, 0x3f, timing_table[index].clk_hs_zero, mipi_id);
	rk628_testif_set_timing(rk628, 0x63, 0x7f, timing_table[index].clk_hs_trail, mipi_id);
	rk628_testif_set_timing(rk628, 0x65, 0x0f, timing_table[index].clk_post, mipi_id);
	rk628_testif_set_timing(rk628, 0x70, 0x3f, timing_table[index].data_lp, mipi_id);
	rk628_testif_set_timing(rk628, 0x71, 0x7f, timing_table[index].data_hs_prepare, mipi_id);
	rk628_testif_set_timing(rk628, 0x72, 0x3f, timing_table[index].data_hs_zero, mipi_id);
	rk628_testif_set_timing(rk628, 0x73, 0x7f, timing_table[index].data_hs_trail, mipi_id);
}

void rk628_mipi_dphy_init_hsfreqrange(struct rk628 *rk628, int lane_mbps, uint8_t mipi_id)
{
	const struct {
		unsigned long max_lane_mbps;
		u8 hsfreqrange;
	} hsfreqrange_table[] = {
		{  90, 0x00}, { 100, 0x10}, { 110, 0x20}, { 130, 0x01},
		{ 140, 0x11}, { 150, 0x21}, { 170, 0x02}, { 180, 0x12},
		{ 200, 0x22}, { 220, 0x03}, { 240, 0x13}, { 250, 0x23},
		{ 270, 0x04}, { 300, 0x14}, { 330, 0x05}, { 360, 0x15},
		{ 400, 0x25}, { 450, 0x06}, { 500, 0x16}, { 550, 0x07},
		{ 600, 0x17}, { 650, 0x08}, { 700, 0x18}, { 750, 0x09},
		{ 800, 0x19}, { 850, 0x29}, { 900, 0x39}, { 950, 0x0a},
		{1000, 0x1a}, {1050, 0x2a}, {1100, 0x3a}, {1150, 0x0b},
		{1200, 0x1b}, {1250, 0x2b}, {1300, 0x3b}, {1350, 0x0c},
		{1400, 0x1c}, {1450, 0x2c}, {1500, 0x3c}
	};
	u8 hsfreqrange;
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(hsfreqrange_table); index++)
		if (lane_mbps <= hsfreqrange_table[index].max_lane_mbps)
			break;

	if (index == ARRAY_SIZE(hsfreqrange_table))
		--index;

	hsfreqrange = hsfreqrange_table[index].hsfreqrange;
	rk628_testif_write(rk628, 0x44, HSFREQRANGE(hsfreqrange), mipi_id);
	rk628_mipi_dphy_set_timing(rk628, lane_mbps, mipi_id);
}
EXPORT_SYMBOL(rk628_mipi_dphy_init_hsfreqrange);

int rk628_mipi_dphy_reset_assert(struct rk628 *rk628)
{
	rk628_i2c_write(rk628, CSITX_SYS_CTRL0_IMD, 0x1);
	if (rk628->version >= RK628F_VERSION)
		rk628_i2c_write(rk628, CSITX1_SYS_CTRL0_IMD, 0x1);
	mipi_dphy_enableclk_deassert(rk628, 0);
	if (rk628->version >= RK628F_VERSION)
		mipi_dphy_enableclk_deassert(rk628, 1);
	mipi_dphy_shutdownz_assert(rk628);
	mipi_dphy_rstz_assert(rk628);
	rk628_testif_testclr_assert(rk628, 0);
	if (rk628->version >= RK628F_VERSION)
		rk628_testif_testclr_assert(rk628, 1);

	/* Set all REQUEST inputs to zero */
	rk628_i2c_update_bits(rk628, GRF_MIPI_TX0_CON,
			FORCETXSTOPMODE_MASK | FORCERXMODE_MASK,
			FORCETXSTOPMODE(0) | FORCERXMODE(0));
	if (rk628->version >= RK628F_VERSION)
		rk628_i2c_update_bits(rk628, GRF_MIPI_TX1_CON,
			FORCETXSTOPMODE_MASK | FORCERXMODE_MASK,
			FORCETXSTOPMODE(0) | FORCERXMODE(0));
	udelay(1);
	rk628_testif_testclr_deassert(rk628, 0);
	if (rk628->version >= RK628F_VERSION)
		rk628_testif_testclr_deassert(rk628, 1);
	mipi_dphy_enableclk_assert(rk628, 0);
	if (rk628->version >= RK628F_VERSION)
		mipi_dphy_enableclk_assert(rk628, 1);

	return 0;
}
EXPORT_SYMBOL(rk628_mipi_dphy_reset_assert);

int rk628_mipi_dphy_reset_deassert(struct rk628 *rk628)
{
	mipi_dphy_shutdownz_deassert(rk628);
	mipi_dphy_rstz_deassert(rk628);
	rk628_i2c_write(rk628, CSITX_SYS_CTRL0_IMD, 0x0);
	if (rk628->version >= RK628F_VERSION)
		rk628_i2c_write(rk628, CSITX1_SYS_CTRL0_IMD, 0x0);
	usleep_range(10000, 11000);

	return 0;
}
EXPORT_SYMBOL(rk628_mipi_dphy_reset_deassert);
