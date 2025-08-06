// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include "clk.h"

#define MHz	1000000

#define RV1103B_PVTPLL_GCK_CFG			0x20
#define RV1103B_PVTPLL_GCK_LEN			0x24
#define RV1103B_GCK_START			BIT(0)
#define RV1103B_GCK_EN				BIT(1)
#define RV1103B_GCK_MODE			BIT(5)
#define RV1103B_GCK_RING_LEN_SEL_OFFSET		0
#define RV1103B_GCK_RING_LEN_SEL_MASK		0x1ff
#define RV1103B_GCK_RING_SEL_OFFSET		10
#define RV1103B_GCK_RING_SEL_MASK		0x07
#define RV1103B_PVTPLL_MAX_LENGTH		0x1ff
#define RV1103B_PVTPLL_GCK_CNT_AVG		0x54

#define RV1126B_PVTPLL_NORMAL_MODE		0x1
#define RV1126B_PVTPLL_SLOW_MODE		0
#define RV1126B_PVTPLL_MODE_SHIFT		0
#define RV1126B_PVTPLL_MODE_MASK		0x1

#define RK3506_GRF_CORE_PVTPLL_CON0_L		0x00
#define RK3506_GRF_CORE_PVTPLL_CON0_H		0x04
#define RK3506_OSC_RING_SEL_OFFSET		8
#define RK3506_OSC_RING_SEL_MASK		0x03
#define RK3506_OSC_EN				BIT(1)
#define RK3506_START				BIT(0)
#define RK3506_RING_LENGTH_SEL_OFFSET		0
#define RK3506_RING_LENGTH_SEL_MASK		0x7f
#define RK3506_PVTPLL_MAX_LENGTH		0x7f

struct rockchip_clock_pvtpll;

struct pvtpll_table {
	unsigned int rate;
	u32 length;
	u32 length_frac;
	u32 ring_sel;
	u32 volt_sel_thr;
};

struct rockchip_clock_pvtpll_info {
	const struct clk_ops *clk_ops;
	unsigned int table_size;
	struct pvtpll_table *table;
	unsigned int jm_table_size;
	struct pvtpll_table *jm_table;
	unsigned int pvtpll_adjust_factor;
	unsigned int calibrate_length_step;
	unsigned int calibrate_freq_per_step;
	unsigned int mode_offset;
	bool is_always_on;
	int (*config)(struct rockchip_clock_pvtpll *pvtpll,
		      struct pvtpll_table *table);
	int (*pvtpll_calibrate)(struct rockchip_clock_pvtpll *pvtpll);
	int (*pvtpll_volt_sel_adjust)(struct rockchip_clock_pvtpll *pvtpll,
				      u32 clock_id,
				      u32 volt_sel);
};

struct rockchip_clock_pvtpll {
	struct device *dev;
	struct list_head list_head;
	struct rockchip_clock_pvtpll_info *info;
	struct regmap *regmap;
	struct regmap *regmap_cru;
	struct clk_hw hw;
	struct clk *main_clk;
	struct clk *sclk;
	struct clk *pvtpll_clk;
	struct clk *pvtpll_out;
	struct notifier_block pvtpll_nb;
	struct delayed_work pvtpll_calibrate_work;
	unsigned long cur_rate;
	u32 pvtpll_clk_id;
};

static LIST_HEAD(rockchip_clock_pvtpll_list);
static DEFINE_MUTEX(pvtpll_list_mutex);

struct otp_opp_info {
	u16 min_freq;
	u16 max_freq;
	u8 volt;
	u8 length;
} __packed;

#define ROCKCHIP_PVTPLL(_rate, _sel, _len)			\
{								\
	.rate = _rate##U,					\
	.ring_sel = _sel,					\
	.length = _len,						\
}

#define ROCKCHIP_PVTPLL_VOLT_SEL(_rate, _sel, _len, _volt_sel_thr)	\
{								\
	.rate = _rate##U,					\
	.ring_sel = _sel,					\
	.length = _len,						\
	.volt_sel_thr = _volt_sel_thr,				\
}

static struct pvtpll_table rv1103b_core_pvtpll_table[] = {
	/* rate_hz, ring_sel, length */
	ROCKCHIP_PVTPLL_VOLT_SEL(1608000000, 1, 6, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1512000000, 1, 6, 6),
	ROCKCHIP_PVTPLL_VOLT_SEL(1416000000, 1, 6, 6),
	ROCKCHIP_PVTPLL_VOLT_SEL(1296000000, 1, 6, 5),
	ROCKCHIP_PVTPLL_VOLT_SEL(1200000000, 1, 6, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(1008000000, 1, 26, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(816000000, 1, 50, 3),
};

static struct pvtpll_table rv1103b_enc_pvtpll_table[] = {
	/* rate_hz, ring_se, length */
	ROCKCHIP_PVTPLL(500000000, 1, 80),
};

static struct pvtpll_table rv1103b_isp_pvtpll_table[] = {
	/* rate_hz, ring_se, length */
	ROCKCHIP_PVTPLL(400000000, 1, 160),
};

static struct pvtpll_table rv1103b_npu_pvtpll_table[] = {
	/* rate_hz, ring_se, length */
	ROCKCHIP_PVTPLL_VOLT_SEL(1000000000, 1, 12, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(900000000, 1, 12, 6),
	ROCKCHIP_PVTPLL_VOLT_SEL(800000000, 1, 12, 4),
	ROCKCHIP_PVTPLL_VOLT_SEL(700000000, 1, 32, 4),
};

static struct pvtpll_table rv1126b_aisp_pvtpll_table[] = {
	/* rate_hz, ring_se, length */
	ROCKCHIP_PVTPLL(835000000, 0, 8),
	ROCKCHIP_PVTPLL(805000000, 0, 8),
	ROCKCHIP_PVTPLL(775000000, 0, 8),
};

static struct pvtpll_table rv1126b_core_pvtpll_table[] = {
	/* rate_hz, ring_sel, length */
	ROCKCHIP_PVTPLL_VOLT_SEL(1900000000, 0, 30, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1850000000, 0, 30, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1800000000, 0, 30, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1700000000, 0, 30, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1608000000, 0, 30, 5),
	ROCKCHIP_PVTPLL_VOLT_SEL(1512000000, 0, 30, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(1416000000, 0, 34, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(1296000000, 0, 38, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(1200000000, 0, 38, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(1008000000, 0, 52, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(816000000, 0, 84, 3),
};

static struct pvtpll_table rv1126b_enc_pvtpll_table[] = {
	/* rate_hz, ring_se, length */
	ROCKCHIP_PVTPLL(600000000, 0, 80),
	ROCKCHIP_PVTPLL(555000000, 0, 80),
	ROCKCHIP_PVTPLL(550000000, 0, 88),
	ROCKCHIP_PVTPLL(520000000, 0, 88),
	ROCKCHIP_PVTPLL(500000000, 0, 88),
	ROCKCHIP_PVTPLL(480000000, 0, 88),
};

static struct pvtpll_table rv1126b_isp_pvtpll_table[] = {
	/* rate_hz, ring_se, length */
	ROCKCHIP_PVTPLL(530000000, 0, 100),
	ROCKCHIP_PVTPLL(510000000, 0, 100),
	ROCKCHIP_PVTPLL(490000000, 0, 100),
};

static struct pvtpll_table rv1126b_npu_pvtpll_table[] = {
	/* rate_hz, ring_se, length, volt_sel_thr */
	ROCKCHIP_PVTPLL_VOLT_SEL(1100000000, 0, 12, 6),
	ROCKCHIP_PVTPLL_VOLT_SEL(1050000000, 0, 12, 6),
	ROCKCHIP_PVTPLL_VOLT_SEL(1000000000, 0, 12, 5),
	ROCKCHIP_PVTPLL_VOLT_SEL(950000000, 0, 12, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(900000000, 0, 14, 2),
	ROCKCHIP_PVTPLL_VOLT_SEL(800000000, 0, 18, 2),
	ROCKCHIP_PVTPLL_VOLT_SEL(700000000, 0, 36, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(600000000, 0, 60, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(510000000, 0, 108, 3),
};

static struct pvtpll_table rk3506_core_pvtpll_table[] = {
	/* rate_hz, ring_sel, length, volt_sel_thr */
	ROCKCHIP_PVTPLL_VOLT_SEL(1608000000, 0, 6, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1512000000, 0, 6, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1416000000, 0, 6, 5),
	ROCKCHIP_PVTPLL_VOLT_SEL(1296000000, 0, 6, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(1200000000, 0, 6, 2),
	ROCKCHIP_PVTPLL_VOLT_SEL(1008000000, 0, 10, 4),
	ROCKCHIP_PVTPLL_VOLT_SEL(800000000, 0, 18, 4),
};

static struct pvtpll_table rk3506j_core_pvtpll_table[] = {
	/* rate_hz, ring_sel, length, volt_sel_thr */
	ROCKCHIP_PVTPLL_VOLT_SEL(1608000000, 0, 6, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1512000000, 0, 7, 7),
	ROCKCHIP_PVTPLL_VOLT_SEL(1416000000, 0, 7, 5),
	ROCKCHIP_PVTPLL_VOLT_SEL(1296000000, 0, 7, 3),
	ROCKCHIP_PVTPLL_VOLT_SEL(1200000000, 0, 7, 2),
	ROCKCHIP_PVTPLL_VOLT_SEL(1008000000, 0, 11, 2),
	ROCKCHIP_PVTPLL_VOLT_SEL(800000000, 0, 19, 2),
};

static struct pvtpll_table
*rockchip_get_pvtpll_settings(struct rockchip_clock_pvtpll *pvtpll,
			      unsigned long rate)
{
	const struct rockchip_clock_pvtpll_info *info = pvtpll->info;
	int i;

	for (i = 0; i < info->table_size; i++) {
		if (rate == info->table[i].rate)
			return &info->table[i];
	}
	return NULL;
}

static int rv1103b_pvtpll_configs(struct rockchip_clock_pvtpll *pvtpll,
				  struct pvtpll_table *table)
{
	u32 val;
	int ret = 0;

	val = HIWORD_UPDATE(table->ring_sel, RV1103B_GCK_RING_SEL_MASK,
			    RV1103B_GCK_RING_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_LEN, val);
	if (ret)
		return ret;

	val = HIWORD_UPDATE(table->length, RV1103B_GCK_RING_LEN_SEL_MASK,
			    RV1103B_GCK_RING_LEN_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_LEN, val);
	if (ret)
		return ret;

	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_CFG,
			   RV1103B_GCK_EN | (RV1103B_GCK_EN << 16) |
			   RV1103B_GCK_MODE | (RV1103B_GCK_MODE << 16));
	if (ret)
		return ret;

	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_CFG,
			   RV1103B_GCK_START | (RV1103B_GCK_START << 16));
	if (ret)
		return ret;

	return ret;
}

static int rv1126b_pvtpll_configs(struct rockchip_clock_pvtpll *pvtpll,
				  struct pvtpll_table *table)
{
	u32 val;
	int ret = 0;

	ret = rv1103b_pvtpll_configs(pvtpll, table);
	if (ret)
		return ret;

	if (!pvtpll->regmap_cru)
		return 0;

	val = HIWORD_UPDATE(RV1126B_PVTPLL_NORMAL_MODE, RV1126B_PVTPLL_MODE_MASK,
			    RV1126B_PVTPLL_MODE_SHIFT);
	return regmap_write(pvtpll->regmap_cru, pvtpll->info->mode_offset, val);
}

static int rk3506_pvtpll_configs(struct rockchip_clock_pvtpll *pvtpll,
				  struct pvtpll_table *table)
{
	u32 val;
	int ret = 0;

	val = HIWORD_UPDATE(table->ring_sel, RK3506_OSC_RING_SEL_MASK,
			    RK3506_OSC_RING_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RK3506_GRF_CORE_PVTPLL_CON0_L, val);
	if (ret)
		return ret;

	val = HIWORD_UPDATE(table->length, RK3506_RING_LENGTH_SEL_MASK,
			    RK3506_RING_LENGTH_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RK3506_GRF_CORE_PVTPLL_CON0_H, val);
	if (ret)
		return ret;

	ret = regmap_write(pvtpll->regmap, RK3506_GRF_CORE_PVTPLL_CON0_L,
			   RK3506_START | (RK3506_START << 16) |
			   RK3506_OSC_EN | (RK3506_OSC_EN << 16));
	if (ret)
		return ret;

	return ret;
}

static int rockchip_clock_pvtpll_set_rate(struct clk_hw *hw,
					  unsigned long rate,
					  unsigned long parent_rate)
{
	struct rockchip_clock_pvtpll *pvtpll;
	struct pvtpll_table *table;
	int ret = 0;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);

	if (!pvtpll)
		return 0;

	/*
	 * The calibration is only for the init frequency of pvtpll on the platform
	 * which regulator is fixed, if the frequency will be change, we assume that
	 * dvfs is working, so just cancel the calibration work and use the pvtpll
	 * configuration from pvtpll_table, it will match the opp-table.
	 */
	if (pvtpll->info->pvtpll_calibrate)
		cancel_delayed_work_sync(&pvtpll->pvtpll_calibrate_work);

	table = rockchip_get_pvtpll_settings(pvtpll, rate);
	if (!table)
		return 0;

	ret = pvtpll->info->config(pvtpll, table);

	pvtpll->cur_rate = rate;
	return ret;
}

static unsigned long
rockchip_clock_pvtpll_recalc_rate(struct clk_hw *hw,
				  unsigned long parent_rate)
{
	struct rockchip_clock_pvtpll *pvtpll;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);

	if (!pvtpll)
		return 0;

	return pvtpll->cur_rate;
}

static long rockchip_clock_pvtpll_round_rate(struct clk_hw *hw, unsigned long rate,
					     unsigned long *prate)
{
	struct rockchip_clock_pvtpll *pvtpll;
	struct pvtpll_table *table;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);

	if (!pvtpll)
		return 0;

	table = rockchip_get_pvtpll_settings(pvtpll, rate);
	if (!table)
		return 0;

	return rate;
}

static int rv1126b_pvtpll_enable(struct clk_hw *hw)
{
	struct rockchip_clock_pvtpll *pvtpll;
	struct pvtpll_table *table;
	u32 val = 0;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);
	table = rockchip_get_pvtpll_settings(pvtpll, pvtpll->cur_rate);
	if (!table)
		return 0;

	regmap_read(pvtpll->regmap, RV1103B_PVTPLL_GCK_CFG, &val);
	if (!(val & RV1103B_GCK_EN))
		return pvtpll->info->config(pvtpll, table);

	return 0;
}

static void rv1126b_pvtpll_disable(struct clk_hw *hw)
{
	struct rockchip_clock_pvtpll *pvtpll;
	u32 val;
	int ret;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);
	if (!pvtpll->regmap_cru)
		return;

	val = HIWORD_UPDATE(RV1126B_PVTPLL_SLOW_MODE, RV1126B_PVTPLL_MODE_MASK,
			    RV1126B_PVTPLL_MODE_SHIFT);
	ret = regmap_write(pvtpll->regmap_cru,  pvtpll->info->mode_offset, val);
	if (ret)
		return;
	regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_CFG, RV1103B_GCK_EN << 16);
}

static int rv1126b_pvtpll_is_enabled(struct clk_hw *hw)
{
	struct rockchip_clock_pvtpll *pvtpll;
	u32 val = 0;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);
	regmap_read(pvtpll->regmap, RV1103B_PVTPLL_GCK_CFG, &val);

	return (val & RV1103B_GCK_EN);
}

static const struct clk_ops clock_pvtpll_ops = {
	.recalc_rate = rockchip_clock_pvtpll_recalc_rate,
	.round_rate = rockchip_clock_pvtpll_round_rate,
	.set_rate = rockchip_clock_pvtpll_set_rate,
};

static const struct clk_ops rv1126b_pvtpll_ops = {
	.recalc_rate = rockchip_clock_pvtpll_recalc_rate,
	.round_rate = rockchip_clock_pvtpll_round_rate,
	.set_rate = rockchip_clock_pvtpll_set_rate,
	.enable = rv1126b_pvtpll_enable,
	.disable = rv1126b_pvtpll_disable,
	.is_enabled = rv1126b_pvtpll_is_enabled,
};

/* Remove is_enabled for fixing clk_summary issue */
static const struct clk_ops rv1126b_npu_pvtpll_ops = {
	.recalc_rate = rockchip_clock_pvtpll_recalc_rate,
	.round_rate = rockchip_clock_pvtpll_round_rate,
	.set_rate = rockchip_clock_pvtpll_set_rate,
	.enable = rv1126b_pvtpll_enable,
	.disable = rv1126b_pvtpll_disable,
};

static int clock_pvtpll_regitstor(struct device *dev,
				  struct rockchip_clock_pvtpll *pvtpll)
{
	struct clk_init_data init = {};

	init.parent_names = NULL;
	init.num_parents = 0;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.name = "pvtpll";
	if (pvtpll->info->clk_ops)
		init.ops = pvtpll->info->clk_ops;
	else
		init.ops = &clock_pvtpll_ops;

	pvtpll->hw.init = &init;

	/* optional override of the clockname */
	of_property_read_string_index(dev->of_node, "clock-output-names",
				      0, &init.name);
	pvtpll->pvtpll_out = devm_clk_register(dev, &pvtpll->hw);
	if (IS_ERR(pvtpll->pvtpll_out))
		return PTR_ERR(pvtpll->pvtpll_out);

	return of_clk_add_provider(dev->of_node, of_clk_src_simple_get,
				   pvtpll->pvtpll_out);
}

static int pvtpll_volt_sel_adjust_linear(struct rockchip_clock_pvtpll *pvtpll,
					 u32 clock_id,
					 u32 volt_sel)
{
	struct pvtpll_table *table = pvtpll->info->table;
	unsigned int size = pvtpll->info->table_size;
	unsigned int factor = pvtpll->info->pvtpll_adjust_factor;
	uint32_t delta_len = 0;
	int i;

	for (i = 0; i < size; i++) {
		if (!table[i].volt_sel_thr)
			continue;
		if (volt_sel >= table[i].volt_sel_thr) {
			delta_len = (volt_sel - table[i].volt_sel_thr + 1) * factor;
			table[i].length += delta_len;
			if (table[i].length > RK3506_PVTPLL_MAX_LENGTH)
				table[i].length = RK3506_PVTPLL_MAX_LENGTH;

			/* update new pvtpll config for current rate */
			if (table[i].rate == pvtpll->cur_rate)
				pvtpll->info->config(pvtpll, table + i);
		}
	}

	return 0;
}

int rockchip_pvtpll_volt_sel_adjust(u32 clock_id, u32 volt_sel)
{
	struct rockchip_clock_pvtpll *pvtpll;
	int ret = -ENODEV;

	mutex_lock(&pvtpll_list_mutex);
	list_for_each_entry(pvtpll, &rockchip_clock_pvtpll_list, list_head) {
		if ((pvtpll->pvtpll_clk_id == clock_id) && pvtpll->info->pvtpll_volt_sel_adjust) {
			ret = pvtpll->info->pvtpll_volt_sel_adjust(pvtpll, clock_id, volt_sel);
			break;
		}
	}
	mutex_unlock(&pvtpll_list_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_pvtpll_volt_sel_adjust);

static int rockchip_pvtpll_get_otp_info(struct device *dev,
					struct otp_opp_info *opp_info)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len = 0;

	cell = nvmem_cell_get(dev, "opp-info");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}
	if (len != sizeof(*opp_info)) {
		kfree(buf);
		nvmem_cell_put(cell);
		return -EINVAL;
	}

	memcpy(opp_info, buf, sizeof(*opp_info));
	kfree(buf);
	nvmem_cell_put(cell);

	return 0;
}

static void rockchip_switch_pvtpll_table(struct device *dev,
					 struct rockchip_clock_pvtpll *pvtpll)
{
	u8 spec = 0;

	if (!pvtpll->info->jm_table)
		return;

	if (!nvmem_cell_read_u8(dev, "specification_serial_number", &spec)) {
		/* M = 0xd, J = 0xa */
		if ((spec == 0xd) || (spec == 0xa)) {
			pvtpll->info->table = pvtpll->info->jm_table;
			pvtpll->info->table_size = pvtpll->info->jm_table_size;
		}
	}
}

static void rockchip_adjust_pvtpll_by_otp(struct device *dev,
					  struct rockchip_clock_pvtpll *pvtpll)
{
	struct otp_opp_info opp_info = { 0 };
	struct pvtpll_table *table = pvtpll->info->table;
	unsigned int size = pvtpll->info->table_size;
	u32 min_freq, max_freq;
	int i;

	if (rockchip_pvtpll_get_otp_info(dev, &opp_info))
		return;

	if (!opp_info.length)
		return;

	dev_info(dev, "adjust opp-table by otp: min=%uM, max=%uM, length=%u\n",
		 opp_info.min_freq, opp_info.max_freq, opp_info.length);

	min_freq = opp_info.min_freq * MHz;
	max_freq = opp_info.max_freq * MHz;

	for (i = 0; i < size; i++) {
		if (table[i].rate < min_freq)
			continue;
		if (table[i].rate > max_freq)
			continue;

		table[i].length += opp_info.length;
		if (table[i].length > RK3506_PVTPLL_MAX_LENGTH)
			table[i].length = RK3506_PVTPLL_MAX_LENGTH;
	}
}

static int rv1103b_pvtpll_calibrate(struct rockchip_clock_pvtpll *pvtpll)
{
	unsigned int rate, delta, length, length_ori, val, i = 0;
	unsigned int length_step = pvtpll->info->calibrate_length_step;
	unsigned int freq_per_step = pvtpll->info->calibrate_freq_per_step;
	unsigned long target_rate = pvtpll->cur_rate / MHz;
	int ret;

	ret = regmap_read(pvtpll->regmap, RV1103B_PVTPLL_GCK_CNT_AVG, &rate);
	if (ret)
		return ret;

	if (rate < target_rate)
		return 0;

	/* delta < (6.25% * target_rate) */
	if ((rate - target_rate) < (target_rate >> 4))
		return 0;

	ret = regmap_read(pvtpll->regmap, RV1103B_PVTPLL_GCK_LEN, &val);
	if (ret)
		return ret;
	length_ori = (val >> RV1103B_GCK_RING_LEN_SEL_OFFSET) & RV1103B_GCK_RING_LEN_SEL_MASK;
	length = length_ori;
	delta = rate - target_rate;
	length += (delta / freq_per_step) * length_step;
	val = HIWORD_UPDATE(length, RV1103B_GCK_RING_LEN_SEL_MASK,
			    RV1103B_GCK_RING_LEN_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_LEN, val);
	if (ret)
		return ret;
	usleep_range(2000, 2100);
	ret = regmap_read(pvtpll->regmap, RV1103B_PVTPLL_GCK_CNT_AVG, &rate);
	if (ret)
		return ret;

	while ((rate < target_rate) || ((rate - target_rate) > (target_rate >> 4))) {
		if (i++ > 20)
			break;

		if (rate > target_rate)
			length += length_step;
		else
			length -= length_step;
		if (length < length_ori)
			break;

		val = HIWORD_UPDATE(length, RV1103B_GCK_RING_LEN_SEL_MASK,
				    RV1103B_GCK_RING_LEN_SEL_OFFSET);
		ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_LEN, val);
		if (ret)
			return ret;
		usleep_range(2000, 2100);
		ret = regmap_read(pvtpll->regmap, RV1103B_PVTPLL_GCK_CNT_AVG, &rate);
		if (ret)
			return ret;
	}

	/* update pvtpll table */
	if (length > length_ori) {
		delta = length - length_ori;
		for (i = 0; i < pvtpll->info->table_size; i++)
			pvtpll->info->table[i].length += delta;
	}

	return 0;
}

static void rockchip_pvtpll_calibrate(struct work_struct *work)
{
	struct rockchip_clock_pvtpll *pvtpll;
	int ret;

	pvtpll = container_of(work, struct rockchip_clock_pvtpll, pvtpll_calibrate_work.work);

	if (pvtpll->info->pvtpll_calibrate) {
		ret = pvtpll->info->pvtpll_calibrate(pvtpll);
		if (ret)
			dev_warn(pvtpll->dev, "%s: calibrate error, ret %d\n", __func__, ret);
	}
}

static const struct rockchip_clock_pvtpll_info rv1103b_core_pvtpll_data = {
	.config = rv1103b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1103b_core_pvtpll_table),
	.table = rv1103b_core_pvtpll_table,
	.pvtpll_adjust_factor = 4,
	.pvtpll_volt_sel_adjust = pvtpll_volt_sel_adjust_linear,
	.calibrate_length_step = 2,
	.calibrate_freq_per_step = 30,
	.pvtpll_calibrate = rv1103b_pvtpll_calibrate,
};

static const struct rockchip_clock_pvtpll_info rv1103b_enc_pvtpll_data = {
	.config = rv1103b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1103b_enc_pvtpll_table),
	.table = rv1103b_enc_pvtpll_table,
	.calibrate_length_step = 8,
	.calibrate_freq_per_step = 25,
	.pvtpll_calibrate = rv1103b_pvtpll_calibrate,
};

static const struct rockchip_clock_pvtpll_info rv1103b_isp_pvtpll_data = {
	.config = rv1103b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1103b_isp_pvtpll_table),
	.table = rv1103b_isp_pvtpll_table,
};

static const struct rockchip_clock_pvtpll_info rv1103b_npu_pvtpll_data = {
	.config = rv1103b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1103b_npu_pvtpll_table),
	.table = rv1103b_npu_pvtpll_table,
	.pvtpll_adjust_factor = 6,
	.pvtpll_volt_sel_adjust = pvtpll_volt_sel_adjust_linear,
	.calibrate_length_step = 4,
	.calibrate_freq_per_step = 25,
	.pvtpll_calibrate = rv1103b_pvtpll_calibrate,
};

static const struct rockchip_clock_pvtpll_info rv1126b_aisp_pvtpll_data = {
	.clk_ops = &rv1126b_pvtpll_ops,
	.config = rv1126b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1126b_aisp_pvtpll_table),
	.table = rv1126b_aisp_pvtpll_table,
	.calibrate_length_step = 4,
	.calibrate_freq_per_step = 20,
	.pvtpll_calibrate = rv1103b_pvtpll_calibrate,
	.mode_offset = 0xb0300,
};

static const struct rockchip_clock_pvtpll_info rv1126b_core_pvtpll_data = {
	.config = rv1126b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1126b_core_pvtpll_table),
	.table = rv1126b_core_pvtpll_table,
	.pvtpll_adjust_factor = 4,
	.pvtpll_volt_sel_adjust = pvtpll_volt_sel_adjust_linear,
	.mode_offset = 0x30300,
	.is_always_on = true,
};

static const struct rockchip_clock_pvtpll_info rv1126b_enc_pvtpll_data = {
	.config = rv1126b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1126b_enc_pvtpll_table),
	.table = rv1126b_enc_pvtpll_table,
	.calibrate_length_step = 8,
	.calibrate_freq_per_step = 20,
	.pvtpll_calibrate = rv1103b_pvtpll_calibrate,
	.mode_offset = 0x80300,
	.is_always_on = true,
};

static const struct rockchip_clock_pvtpll_info rv1126b_isp_pvtpll_data = {
	.clk_ops = &rv1126b_pvtpll_ops,
	.config = rv1126b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1126b_isp_pvtpll_table),
	.table = rv1126b_isp_pvtpll_table,
	.calibrate_length_step = 8,
	.calibrate_freq_per_step = 20,
	.pvtpll_calibrate = rv1103b_pvtpll_calibrate,
	.mode_offset = 0x70300,
};

static const struct rockchip_clock_pvtpll_info rv1126b_npu_pvtpll_data = {
	.clk_ops = &rv1126b_npu_pvtpll_ops,
	.config = rv1126b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1126b_npu_pvtpll_table),
	.table = rv1126b_npu_pvtpll_table,
	.pvtpll_adjust_factor = 6,
	.pvtpll_volt_sel_adjust = pvtpll_volt_sel_adjust_linear,
	.mode_offset = 0x90300,
};

static const struct rockchip_clock_pvtpll_info rk3506_core_pvtpll_data = {
	.config = rk3506_pvtpll_configs,
	.table_size = ARRAY_SIZE(rk3506_core_pvtpll_table),
	.table = rk3506_core_pvtpll_table,
	.jm_table_size = ARRAY_SIZE(rk3506j_core_pvtpll_table),
	.jm_table = rk3506j_core_pvtpll_table,
	.pvtpll_adjust_factor = 1,
	.pvtpll_volt_sel_adjust = pvtpll_volt_sel_adjust_linear,
};

static const struct of_device_id rockchip_clock_pvtpll_match[] = {
	{
		.compatible = "rockchip,rv1103b-core-pvtpll",
		.data = (void *)&rv1103b_core_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1103b-enc-pvtpll",
		.data = (void *)&rv1103b_enc_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1103b-isp-pvtpll",
		.data = (void *)&rv1103b_isp_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1103b-npu-pvtpll",
		.data = (void *)&rv1103b_npu_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1126b-aisp-pvtpll",
		.data = (void *)&rv1126b_aisp_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1126b-core-pvtpll",
		.data = (void *)&rv1126b_core_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1126b-enc-pvtpll",
		.data = (void *)&rv1126b_enc_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1126b-isp-pvtpll",
		.data = (void *)&rv1126b_isp_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1126b-npu-pvtpll",
		.data = (void *)&rv1126b_npu_pvtpll_data,
	},
	{
		.compatible = "rockchip,rk3506-core-pvtpll",
		.data = (void *)&rk3506_core_pvtpll_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, rockchip_clock_pvtpll_match);

static int rockchip_clock_pvtpll_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct rockchip_clock_pvtpll *pvtpll;
	struct of_phandle_args clkspec = { 0 };
	int error = 0;

	pvtpll = devm_kzalloc(dev, sizeof(*pvtpll), GFP_KERNEL);
	if (!pvtpll)
		return -ENOMEM;

	pvtpll->info = (struct rockchip_clock_pvtpll_info *)device_get_match_data(&pdev->dev);
	if (!pvtpll->info)
		return -EINVAL;

	pvtpll->regmap = device_node_to_regmap(np);
	if (IS_ERR(pvtpll->regmap))
		return PTR_ERR(pvtpll->regmap);

	pvtpll->regmap_cru = syscon_regmap_lookup_by_phandle_optional(np, "rockchip,cru");

	pvtpll->dev = dev;
	pvtpll->pvtpll_clk_id = UINT_MAX;
	INIT_DELAYED_WORK(&pvtpll->pvtpll_calibrate_work, rockchip_pvtpll_calibrate);

	error = of_parse_phandle_with_args(np, "clocks", "#clock-cells",
					   0, &clkspec);
	if (!error) {
		pvtpll->pvtpll_clk_id = clkspec.args[0];
		of_node_put(clkspec.np);
	}

	rockchip_switch_pvtpll_table(dev, pvtpll);

	rockchip_adjust_pvtpll_by_otp(dev, pvtpll);

	platform_set_drvdata(pdev, pvtpll);

	error = clock_pvtpll_regitstor(&pdev->dev, pvtpll);
	if (error) {
		dev_err(&pdev->dev, "failed to register clock: %d\n", error);
		return error;
	}

	if (pvtpll->info->pvtpll_calibrate)
		queue_delayed_work(system_freezable_wq,
				   &pvtpll->pvtpll_calibrate_work,
				   0);

	mutex_lock(&pvtpll_list_mutex);
	list_add(&pvtpll->list_head, &rockchip_clock_pvtpll_list);
	mutex_unlock(&pvtpll_list_mutex);

	return 0;
}

static int rockchip_clock_pvtpll_remove(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	of_clk_del_provider(np);

	return 0;
}

static int rockchip_clock_pvtpll_resume(struct device *dev)
{
	struct rockchip_clock_pvtpll *pvtpll = dev_get_drvdata(dev);
	struct pvtpll_table *table;

	if (!pvtpll->info->is_always_on)
		return 0;

	table = rockchip_get_pvtpll_settings(pvtpll, pvtpll->cur_rate);
	if (!table)
		return 0;

	return pvtpll->info->config(pvtpll, table);
}

static DEFINE_SIMPLE_DEV_PM_OPS(rockchip_clock_pvtpll_pm_ops, NULL,
				rockchip_clock_pvtpll_resume);

static struct platform_driver rockchip_clock_pvtpll_driver = {
	.driver = {
		.name = "rockchip-clcok-pvtpll",
		.pm = pm_sleep_ptr(&rockchip_clock_pvtpll_pm_ops),
		.of_match_table = rockchip_clock_pvtpll_match,
	},
	.probe = rockchip_clock_pvtpll_probe,
	.remove = rockchip_clock_pvtpll_remove,
};

module_platform_driver(rockchip_clock_pvtpll_driver);

MODULE_DESCRIPTION("Rockchip Clock Pvtpll Driver");
MODULE_LICENSE("GPL");
