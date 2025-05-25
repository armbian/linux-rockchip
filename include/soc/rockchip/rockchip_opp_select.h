/*
 * Copyright (c) 2017 Fuzhou Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef __SOC_ROCKCHIP_OPP_SELECT_H
#define __SOC_ROCKCHIP_OPP_SELECT_H

#include <linux/pm_opp.h>

#define VOLT_RM_TABLE_END	~1

/*
 * [0]:      set intermediate rate
 *           [1]: scaling up rate or scaling down rate
 * [1]:      add length for pvtpll
 *           [2:5]: length
 * [2]:      use low length for pvtpll
 * [3:5]:    reserved
 */
#define OPP_RATE_MASK		0x3f

/* Set intermediate rate */
#define OPP_INTERMEDIATE_RATE	BIT(0)
#define OPP_SCALING_UP_RATE	BIT(1)
#define OPP_SCALING_UP_INTER	(OPP_INTERMEDIATE_RATE | OPP_SCALING_UP_RATE)
#define OPP_SCALING_DOWN_INTER	OPP_INTERMEDIATE_RATE

/* Add length for pvtpll */
#define OPP_ADD_LENGTH		BIT(1)
#define OPP_LENGTH_MASK		0xf
#define OPP_LENGTH_SHIFT	2

/* Use low length for pvtpll */
#define OPP_LENGTH_LOW		BIT(2)

struct rockchip_opp_info;

struct volt_rm_table {
	int volt;
	int rm;
};

struct rockchip_opp_data {
	config_clks_t config_clks;
	config_regulators_t config_regulators;
	bool is_use_pvtpll;

	int (*get_soc_info)(struct device *dev, struct device_node *np,
			    int *bin, int *process);
	int (*set_soc_info)(struct device *dev, struct device_node *np,
			    struct rockchip_opp_info *info);
	int (*set_read_margin)(struct device *dev,
			       struct rockchip_opp_info *info,
			       u32 rm);
};

struct pvtpll_opp_table {
	unsigned long rate;
	unsigned long u_volt;
	unsigned long u_volt_min;
	unsigned long u_volt_max;
	unsigned long u_volt_mem;
	unsigned long u_volt_mem_min;
	unsigned long u_volt_mem_max;
};

/**
 * struct rockchip_opp_info - Rockchip opp info structure
 * @dev:		The device.
 * @dvfs_mutex:		Mutex to protect changing volage and scmi clock rate.
 * @data:		Device-specific opp data.
 * @opp_table:		Temporary opp table only used when enable pvtpll calibration.
 * @pvtpll_avg_offset:	Register offset of pvtm value.
 * @pvtpll_min_rate:	Minimum frequency which needs calibration.
 * @pvtpll_volt_step:	Voltage step of pvtpll calibration.
 * @volt_rm_tbl:	Pointer to voltage to memory read margin conversion table.
 * @grf:		General Register Files regmap.
 * @dsu_grf:		DSU General Register Files regmap.
 * @cci_grf:		CCI General Register Files regmap.
 * @clocks:		Pvtpll clocks.
 * @nclocks:		Number of pvtpll clock.
 * @intermediate_threshold_freq: The frequency threshold of intermediate rate.
 * @low_rm:		The read margin threshold of intermediate rate.
 * @current_rm:		Current memory read margin.
 * @target_rm:		Target memory read margin.
 * @is_runtime_active:	Marks if device's pd is power on.
 * @opp_token:		Integer replacement for opp_table.
 * @scale:		Frequency scale.
 * @bin:		Soc version.
 * @process:		Process version.
 * @volt_sel:		Speed grade.
 * @supported_hw:	Array of version number to support.
 * @clk:		Device's clock handle.
 * @is_scmi_clk:	Marks if device's clock is scmi clock.
 * @regulators:		Supply regulators.
 * @regulator_count:	Number of power supply regulators.
 * @init_freq:		Set the initial frequency when init opp table.
 * @is_rate_volt_checked: Marks if device has checked initial rate and voltage.
 * @pvtpll_clk_id:      Device's clock id.
 * @pvtpll_smc:		Marks if smc call of pvtpll is available.
 * @pvtpll_low_temp:    Marks if device has low temperature pvtpll config.
 */
struct rockchip_opp_info {
	struct device *dev;
	struct mutex dvfs_mutex;
	const struct rockchip_opp_data *data;
	struct pvtpll_opp_table *opp_table;
	struct regmap *pvtpll_base;
	unsigned int pvtpll_avg_offset;
	unsigned int pvtpll_min_rate;
	unsigned int pvtpll_volt_step;

	struct volt_rm_table *volt_rm_tbl;
	struct regmap *grf;
	union {
		struct regmap *dsu_grf;
		struct regmap *cci_grf;
	};
	struct clk_bulk_data *clocks;
	int nclocks;
	unsigned long intermediate_threshold_freq;
	u32 low_rm;
	u32 current_rm;
	u32 target_rm;
	bool is_runtime_active;

	int opp_token;
	int scale;
	int bin;
	int process;
	int volt_sel;
	u32 supported_hw[2];

	struct clk *clk;
	bool is_scmi_clk;
	struct regulator **regulators;
	int regulator_count;
	unsigned int init_freq;
	bool is_rate_volt_checked;

	u32 pvtpll_clk_id;
	bool pvtpll_smc;
	bool pvtpll_low_temp;
};

static inline bool rockchip_opp_is_use_pvtpll(struct rockchip_opp_info *info)
{
	return (info->is_scmi_clk || (info->data && info->data->is_use_pvtpll));
}

#if IS_ENABLED(CONFIG_ROCKCHIP_OPP)
int rockchip_of_get_leakage(struct device *dev, char *lkg_name, int *leakage);
int rockchip_nvmem_cell_read_u8(struct device_node *np, const char *cell_id,
				u8 *val);
int rockchip_nvmem_cell_read_u16(struct device_node *np, const char *cell_id,
				 u16 *val);
void rockchip_get_opp_data(const struct of_device_id *matches,
			   struct rockchip_opp_info *info);
void rockchip_opp_dvfs_lock(struct rockchip_opp_info *info);
void rockchip_opp_dvfs_unlock(struct rockchip_opp_info *info);
int rockchip_init_opp_info(struct device *dev, struct rockchip_opp_info *info,
			   char *clk_name, char *reg_name);
void rockchip_uninit_opp_info(struct device *dev, struct rockchip_opp_info *info);
int rockchip_adjust_opp_table(struct device *dev, struct rockchip_opp_info *info);
int rockchip_get_read_margin(struct device *dev,
			     struct rockchip_opp_info *info,
			     unsigned long volt, u32 *target_rm);
int rockchip_set_read_margin(struct device *dev,
			     struct rockchip_opp_info *info, u32 rm,
			     bool is_set_rm);
int rockchip_set_intermediate_rate(struct device *dev,
				   struct rockchip_opp_info *info,
				   struct clk *clk, unsigned long old_freq,
				   unsigned long new_freq, bool is_scaling_up,
				   bool is_set_clk);
int rockchip_opp_set_low_length(struct device *dev, struct device_node *np,
				struct rockchip_opp_info *opp_info);
int rockchip_opp_config_regulators(struct device *dev,
				     struct dev_pm_opp *old_opp,
				     struct dev_pm_opp *new_opp,
				     struct regulator **regulators,
				     unsigned int count,
				     struct rockchip_opp_info *info);
int rockchip_opp_config_clks(struct device *dev, struct opp_table *opp_table,
			     struct dev_pm_opp *opp, void *data,
			     bool scaling_down, struct rockchip_opp_info *info);
int rockchip_opp_check_rate_volt(struct device *dev, struct rockchip_opp_info *info);
int rockchip_init_opp_table(struct device *dev, struct rockchip_opp_info *info,
			    char *clk_name, char *reg_name);
void rockchip_uninit_opp_table(struct device *dev,
			       struct rockchip_opp_info *info);
#else
static inline int rockchip_of_get_leakage(struct device *dev, char *lkg_name,
					  int *leakage)
{
	return -EOPNOTSUPP;
}

static inline int rockchip_nvmem_cell_read_u8(struct device_node *np,
					      const char *cell_id, u8 *val)
{
	return -EOPNOTSUPP;
}

static inline int rockchip_nvmem_cell_read_u16(struct device_node *np,
					       const char *cell_id, u16 *val)
{
	return -EOPNOTSUPP;
}

static inline void rockchip_get_opp_data(const struct of_device_id *matches,
					 struct rockchip_opp_info *info)
{
}

static inline void rockchip_opp_dvfs_lock(struct rockchip_opp_info *info)
{
}

static inline void rockchip_opp_dvfs_unlock(struct rockchip_opp_info *info)
{
}

static inline int
rockchip_init_opp_info(struct device *dev, struct rockchip_opp_info *info,
		       char *clk_name, char *reg_name)
{
	return -EOPNOTSUPP;
}

static inline void
rockchip_uninit_opp_info(struct device *dev, struct rockchip_opp_info *info)
{
}

static inline int
rockchip_adjust_opp_table(struct device *dev, struct rockchip_opp_info *info)
{
	return -EOPNOTSUPP;
}

static inline int rockchip_get_read_margin(struct device *dev,
					   struct rockchip_opp_info *info,
					   unsigned long volt, u32 *target_rm)
{
	return -EOPNOTSUPP;
}
static inline int rockchip_set_read_margin(struct device *dev,
					   struct rockchip_opp_info *info,
					   u32 rm, bool is_set_rm)
{
	return -EOPNOTSUPP;
}

static inline int
rockchip_set_intermediate_rate(struct device *dev,
			       struct rockchip_opp_info *opp_info,
			       struct clk *clk, unsigned long old_freq,
			       unsigned long new_freq, bool is_scaling_up,
			       bool is_set_clk)
{
	return -EOPNOTSUPP;
}

static inline int
rockchip_opp_set_low_length(struct device *dev, struct device_node *np,
			    struct rockchip_opp_info *opp_info)
{
	return -EOPNOTSUPP;
}

static inline int
rockchip_opp_config_regulators(struct device *dev,
			       struct dev_pm_opp *old_opp,
			       struct dev_pm_opp *new_opp,
			       struct regulator **regulators,
			       unsigned int count,
			       struct rockchip_opp_info *info)
{
	return -EOPNOTSUPP;
}

static inline int rockchip_opp_config_clks(struct device *dev,
					   struct opp_table *opp_table,
					   struct dev_pm_opp *opp, void *data,
					   bool scaling_down,
					   struct rockchip_opp_info *info)
{
	return -EOPNOTSUPP;
}

static inline int rockchip_opp_check_rate_volt(struct device *dev,
					       struct rockchip_opp_info *info)
{
	return -EOPNOTSUPP;
}

static inline int
rockchip_init_opp_table(struct device *dev, struct rockchip_opp_info *info,
			char *clk_name, char *reg_name)
{
	return -EOPNOTSUPP;
}

static inline void rockchip_uninit_opp_table(struct device *dev,
					     struct rockchip_opp_info *info)
{
}

#endif /* CONFIG_ROCKCHIP_OPP */

#endif
