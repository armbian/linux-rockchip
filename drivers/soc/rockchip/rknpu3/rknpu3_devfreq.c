// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU3 devfreq support
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/devfreq_cooling.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/version.h>
#include <../drivers/devfreq/governor.h>

#include <soc/rockchip/rockchip_opp_select.h>
#include <soc/rockchip/rockchip_system_monitor.h>
#include <soc/rockchip/rockchip_ipa.h>

#include "rknpu3_drv.h"
#include "rknpu3_devfreq.h"

#define POWER_DOWN_FREQ 200000000

static int npu_devfreq_target(struct device *dev, unsigned long *freq,
			      u32 flags);

static struct monitor_dev_profile npu_mdevp = {
	.type = MONITOR_TYPE_DEV,
	.low_temp_adjust = rockchip_monitor_dev_low_temp_adjust,
	.high_temp_adjust = rockchip_monitor_dev_high_temp_adjust,
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	.check_rate_volt = rockchip_monitor_check_rate_volt,
#else
	.update_volt = rockchip_monitor_check_rate_volt,
#endif
};

static int npu_devfreq_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	return 0;
}

static int npu_devfreq_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);

	*freq = rknpu3_dev->current_freq;

	return 0;
}

static struct devfreq_dev_profile npu_devfreq_profile = {
	.polling_ms = 50,
	.target = npu_devfreq_target,
	.get_dev_status = npu_devfreq_get_dev_status,
	.get_cur_freq = npu_devfreq_get_cur_freq,
};

static int devfreq_rknpu3_ondemand_func(struct devfreq *df, unsigned long *freq)
{
	struct rknpu3_device *rknpu3_dev = df->data;

	if (rknpu3_dev && rknpu3_dev->ondemand_freq)
		*freq = rknpu3_dev->ondemand_freq;
	else
		*freq = df->previous_freq;

	return 0;
}

static int devfreq_rknpu3_ondemand_handler(struct devfreq *devfreq,
					  unsigned int event, void *data)
{
	return 0;
}

static struct devfreq_governor devfreq_rknpu3_ondemand = {
	.name = "rknpu3_ondemand",
	.get_target_freq = devfreq_rknpu3_ondemand_func,
	.event_handler = devfreq_rknpu3_ondemand_handler,
};

static int rk3572_npu_set_read_margin(struct device *dev,
				      struct rockchip_opp_info *opp_info,
				      u32 rm)
{
	if (!opp_info->grf || !opp_info->volt_rm_tbl)
		return 0;

	if (rm == opp_info->current_rm || rm == UINT_MAX)
		return 0;

	LOG_DEV_DEBUG(dev, "set rm to %d\n", rm);

	regmap_write(opp_info->grf, 0x04, 0x00020002);
	regmap_write(opp_info->grf, 0x20, 0x001c0000 | (rm << 2));
	regmap_write(opp_info->grf, 0x24, 0x001c0000 | (rm << 2));
	regmap_write(opp_info->grf, 0x2c, 0x001e0000 | (rm << 2));

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int npu_opp_config_regulators(struct device *dev,
				     struct dev_pm_opp *old_opp,
				     struct dev_pm_opp *new_opp,
				     struct regulator **regulators,
				     unsigned int count)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);

	return rockchip_opp_config_regulators(dev, old_opp, new_opp, regulators,
					      count, &rknpu3_dev->opp_info);
}

static int npu_opp_config_clks(struct device *dev, struct opp_table *opp_table,
			       struct dev_pm_opp *opp, void *data,
			       bool scaling_down)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);

	return rockchip_opp_config_clks(dev, opp_table, opp, data, scaling_down,
					&rknpu3_dev->opp_info);
}
#endif

static const struct rockchip_opp_data rk3572_npu_opp_data = {
	.set_read_margin = rk3572_npu_set_read_margin,
	.set_soc_info = rockchip_opp_set_low_length,
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	.config_regulators = npu_opp_config_regulators,
	.config_clks = npu_opp_config_clks,
#endif
};

static const struct of_device_id rockchip_npu_of_match[] = {
	{
		.compatible = "rockchip,rk3572",
		.data = (void *)&rk3572_npu_opp_data,
	},
	{},
};

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
void rknpu3_devfreq_lock(struct rknpu3_device *rknpu3_dev)
{
	if (rknpu3_dev->devfreq)
		rockchip_opp_dvfs_lock(&rknpu3_dev->opp_info);
}

void rknpu3_devfreq_unlock(struct rknpu3_device *rknpu3_dev)
{
	if (rknpu3_dev->devfreq)
		rockchip_opp_dvfs_unlock(&rknpu3_dev->opp_info);
}

static int npu_devfreq_target(struct device *dev, unsigned long *freq,
			      u32 flags)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu3_dev->opp_info;
	struct dev_pm_opp *opp;
	unsigned long opp_volt;
	int ret = 0;

	if (!opp_info->is_rate_volt_checked)
		return -EINVAL;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp))
		return PTR_ERR(opp);
	opp_volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	if (*freq == rknpu3_dev->current_freq)
		return 0;

	rockchip_opp_dvfs_lock(opp_info);
	if (pm_runtime_active(dev))
		opp_info->is_runtime_active = true;
	else
		opp_info->is_runtime_active = false;
	ret = dev_pm_opp_set_rate(dev, *freq);
	if (!ret) {
		rknpu3_dev->current_freq = *freq;
		if (rknpu3_dev->devfreq)
			rknpu3_dev->devfreq->last_status.current_frequency =
				*freq;
		rknpu3_dev->current_volt = opp_volt;
		LOG_DEV_DEBUG(dev, "set rknpu3 freq: %lu, volt: %lu\n",
			      rknpu3_dev->current_freq, rknpu3_dev->current_volt);
	}
	rockchip_opp_dvfs_unlock(opp_info);

	return ret;
}

static const struct rockchip_opp_data rockchip_npu_opp_data = {
	.config_clks = npu_opp_config_clks,
};

int rknpu3_devfreq_init(struct rknpu3_device *rknpu3_dev)
{
	struct rockchip_opp_info *info = &rknpu3_dev->opp_info;
	struct device *dev = rknpu3_dev->dev;
	struct devfreq_dev_profile *dp;
	struct dev_pm_opp *opp;
	unsigned int dyn_power_coeff = 0;
	int ret = 0;

	info->data = &rockchip_npu_opp_data;
	rockchip_get_opp_data(rockchip_npu_of_match, info);
	ret = rockchip_init_opp_table(dev, info, "clk_npu", "rknpu");
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to init_opp_table\n");
		return -EINVAL;
	}

	rknpu3_dev->current_freq = clk_get_rate(rknpu3_dev->clks[0].clk);
	opp = devfreq_recommended_opp(dev, &rknpu3_dev->current_freq, 0);
	if (IS_ERR(opp)) {
		ret = PTR_ERR(opp);
		goto err_uinit_table;
	}
	dev_pm_opp_put(opp);

	dp = &npu_devfreq_profile;
	dp->initial_freq = rknpu3_dev->current_freq;
	of_property_read_u32(dev->of_node, "dynamic-power-coefficient",
			     &dyn_power_coeff);
	if (dyn_power_coeff)
		dp->is_cooling_device = true;

	ret = devfreq_add_governor(&devfreq_rknpu3_ondemand);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to add rknpu3_ondemand governor\n");
		goto err_uinit_table;
	}

	rknpu3_dev->devfreq = devm_devfreq_add_device(dev, dp, "rknpu3_ondemand",
						     (void *)rknpu3_dev);
	if (IS_ERR(rknpu3_dev->devfreq)) {
		LOG_DEV_ERROR(dev, "failed to add devfreq\n");
		ret = PTR_ERR(rknpu3_dev->devfreq);
		rknpu3_dev->devfreq = NULL;
		goto err_remove_governor;
	}

	npu_mdevp.data = rknpu3_dev->devfreq;
	npu_mdevp.opp_info = &rknpu3_dev->opp_info;
	rknpu3_dev->mdev_info =
		rockchip_system_monitor_register(dev, &npu_mdevp);
	if (IS_ERR(rknpu3_dev->mdev_info)) {
		dev_dbg(dev, "without system monitor\n");
		rknpu3_dev->mdev_info = NULL;
	}

	rknpu3_dev->current_freq = clk_get_rate(rknpu3_dev->clks[0].clk);
	rknpu3_dev->ondemand_freq = rknpu3_dev->current_freq;
	if (rknpu3_dev->vdd)
		rknpu3_dev->current_volt = regulator_get_voltage(rknpu3_dev->vdd);

	rknpu3_dev->devfreq->previous_freq = rknpu3_dev->current_freq;
	if (rknpu3_dev->devfreq->suspend_freq)
		rknpu3_dev->devfreq->resume_freq = rknpu3_dev->current_freq;
	rknpu3_dev->devfreq->last_status.current_frequency =
		rknpu3_dev->current_freq;
	rknpu3_dev->devfreq->last_status.total_time = 1;
	rknpu3_dev->devfreq->last_status.busy_time = 1;

	LOG_DEV_INFO(dev, "devfreq init success, freq: %lu Hz\n",
		     rknpu3_dev->current_freq);

	return 0;

err_remove_governor:
	devfreq_remove_governor(&devfreq_rknpu3_ondemand);
err_uinit_table:
#if KERNEL_VERSION(5, 10, 198) <= LINUX_VERSION_CODE
	rockchip_uninit_opp_table(dev, info);
#else
	dev_pm_opp_of_remove_table(dev);
#endif

	return ret;
}

int rknpu3_devfreq_runtime_suspend(struct device *dev)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu3_dev->opp_info;

	if (rockchip_opp_is_use_pvtpll(opp_info)) {
		if (clk_set_rate(opp_info->clk, POWER_DOWN_FREQ))
			LOG_DEV_ERROR(dev, "failed to restore clk rate\n");
	}
	opp_info->current_rm = UINT_MAX;

	return 0;
}

int rknpu3_devfreq_runtime_resume(struct device *dev)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu3_dev->opp_info;
	int ret = 0;

	if (!rknpu3_dev->current_freq || !rknpu3_dev->current_volt)
		return 0;

	ret = clk_bulk_prepare_enable(opp_info->nclocks, opp_info->clocks);
	if (ret) {
		LOG_DEV_INFO(dev, "failed to enable opp clks\n");
		return ret;
	}

	if (opp_info->data && opp_info->data->set_read_margin)
		opp_info->data->set_read_margin(dev, opp_info,
						opp_info->target_rm);
	if (rockchip_opp_is_use_pvtpll(opp_info)) {
		if (clk_set_rate(opp_info->clk, rknpu3_dev->current_freq))
			LOG_DEV_ERROR(dev, "failed to set power down rate\n");
	}

	clk_bulk_disable_unprepare(opp_info->nclocks, opp_info->clocks);

	return ret;
}

#else /* KERNEL_VERSION < 6.1.0 */
void rknpu3_devfreq_lock(struct rknpu3_device *rknpu3_dev)
{
	rockchip_monitor_volt_adjust_lock(rknpu3_dev->mdev_info);
}

void rknpu3_devfreq_unlock(struct rknpu3_device *rknpu3_dev)
{
	rockchip_monitor_volt_adjust_unlock(rknpu3_dev->mdev_info);
}

static int npu_opp_helper(struct dev_pm_set_opp_data *data)
{
	struct device *dev = data->dev;
	struct dev_pm_opp_supply *old_supply_vdd = &data->old_opp.supplies[0];
	struct dev_pm_opp_supply *old_supply_mem = &data->old_opp.supplies[1];
	struct dev_pm_opp_supply *new_supply_vdd = &data->new_opp.supplies[0];
	struct dev_pm_opp_supply *new_supply_mem = &data->new_opp.supplies[1];
	struct regulator *vdd_reg = data->regulators[0];
	struct regulator *mem_reg = data->regulators[1];
	struct clk *clk = data->clk;
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu3_dev->opp_info;
	unsigned long old_freq = data->old_opp.rate;
	unsigned long new_freq = data->new_opp.rate;
	bool is_set_rm = true;
	bool is_set_clk = true;
	u32 target_rm = UINT_MAX;
	int ret = 0;

	if (!pm_runtime_active(dev)) {
		is_set_rm = false;
		if (opp_info->scmi_clk)
			is_set_clk = false;
	}

	ret = clk_bulk_prepare_enable(opp_info->num_clks, opp_info->clks);
	if (ret < 0) {
		LOG_DEV_ERROR(dev, "failed to enable opp clks\n");
		return ret;
	}
	rockchip_get_read_margin(dev, opp_info, new_supply_vdd->u_volt,
				 &target_rm);

	/* Change frequency */
	LOG_DEV_DEBUG(dev, "switching OPP: %lu Hz --> %lu Hz\n", old_freq,
		      new_freq);
	/* Scaling up? Scale voltage before frequency */
	if (new_freq >= old_freq) {
		rockchip_set_intermediate_rate(dev, opp_info, clk, old_freq,
					       new_freq, true, is_set_clk);
		ret = regulator_set_voltage(mem_reg, new_supply_mem->u_volt,
					    INT_MAX);
		if (ret) {
			LOG_DEV_ERROR(dev,
				      "failed to set volt %lu uV for mem reg\n",
				      new_supply_mem->u_volt);
			goto restore_voltage;
		}
		ret = regulator_set_voltage(vdd_reg, new_supply_vdd->u_volt,
					    INT_MAX);
		if (ret) {
			LOG_DEV_ERROR(dev,
				      "failed to set volt %lu uV for vdd reg\n",
				      new_supply_vdd->u_volt);
			goto restore_voltage;
		}
		rockchip_set_read_margin(dev, opp_info, target_rm, is_set_rm);
		if (is_set_clk && clk_set_rate(clk, new_freq)) {
			ret = -EINVAL;
			LOG_DEV_ERROR(dev, "failed to set clk rate: %d\n", ret);
			goto restore_rm;
		}
		/* Scaling down? Scale voltage after frequency */
	} else {
		rockchip_set_intermediate_rate(dev, opp_info, clk, old_freq,
					       new_freq, false, is_set_clk);
		rockchip_set_read_margin(dev, opp_info, target_rm, is_set_rm);
		if (is_set_clk && clk_set_rate(clk, new_freq)) {
			ret = -EINVAL;
			LOG_DEV_ERROR(dev, "failed to set clk rate: %d\n", ret);
			goto restore_rm;
		}
		ret = regulator_set_voltage(vdd_reg, new_supply_vdd->u_volt,
					    INT_MAX);
		if (ret) {
			LOG_DEV_ERROR(dev,
				      "failed to set volt %lu uV for vdd reg\n",
				      new_supply_vdd->u_volt);
			goto restore_freq;
		}
		ret = regulator_set_voltage(mem_reg, new_supply_mem->u_volt,
					    INT_MAX);
		if (ret) {
			LOG_DEV_ERROR(dev,
				      "failed to set volt %lu uV for mem reg\n",
				      new_supply_mem->u_volt);
			goto restore_freq;
		}
	}

	clk_bulk_disable_unprepare(opp_info->num_clks, opp_info->clks);

	return 0;

restore_freq:
	if (is_set_clk && clk_set_rate(clk, old_freq))
		LOG_DEV_ERROR(dev, "failed to restore old-freq %lu Hz\n",
			      old_freq);
restore_rm:
	rockchip_get_read_margin(dev, opp_info, old_supply_vdd->u_volt,
				 &target_rm);
	rockchip_set_read_margin(dev, opp_info, opp_info->current_rm,
				 is_set_rm);
restore_voltage:
	regulator_set_voltage(mem_reg, old_supply_mem->u_volt, INT_MAX);
	regulator_set_voltage(vdd_reg, old_supply_vdd->u_volt, INT_MAX);
	clk_bulk_disable_unprepare(opp_info->num_clks, opp_info->clks);

	return ret;
}

static int npu_devfreq_target(struct device *dev, unsigned long *freq,
			      u32 flags)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	unsigned long opp_volt;
	int ret = 0;

	if (!npu_mdevp.is_checked)
		return -EINVAL;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp))
		return PTR_ERR(opp);
	opp_volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	rockchip_monitor_volt_adjust_lock(rknpu3_dev->mdev_info);

	ret = dev_pm_opp_set_rate(dev, *freq);
	if (!ret) {
		rknpu3_dev->current_freq = *freq;
		if (rknpu3_dev->devfreq)
			rknpu3_dev->devfreq->last_status.current_frequency =
				*freq;
		rknpu3_dev->current_volt = opp_volt;
		LOG_DEV_DEBUG(dev, "set rknpu3 freq: %lu, volt: %lu\n",
			      rknpu3_dev->current_freq, rknpu3_dev->current_volt);
	}

	rockchip_monitor_volt_adjust_unlock(rknpu3_dev->mdev_info);

	return ret;
}

static unsigned long npu_get_static_power(struct devfreq *devfreq,
					  unsigned long voltage)
{
	struct device *dev = devfreq->dev.parent;
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);

	if (!rknpu3_dev->model_data)
		return 0;

	return rockchip_ipa_get_static_power(rknpu3_dev->model_data, voltage);
}

static struct devfreq_cooling_power npu_cooling_power = {
	.get_static_power = &npu_get_static_power,
};

int rknpu3_devfreq_init(struct rknpu3_device *rknpu3_dev)
{
	struct device *dev = rknpu3_dev->dev;
	struct devfreq_dev_profile *dp = &npu_devfreq_profile;
	struct dev_pm_opp *opp;
	struct opp_table *reg_table = NULL;
	struct opp_table *opp_table = NULL;
	const char *const reg_names[] = { "rknpu", "mem" };
	int ret = -EINVAL;

	if (strstr(__clk_get_name(rknpu3_dev->clks[0].clk), "scmi"))
		rknpu3_dev->opp_info.scmi_clk = rknpu3_dev->clks[0].clk;

	if (of_find_property(dev->of_node, "rknpu-supply", NULL) &&
	    of_find_property(dev->of_node, "mem-supply", NULL)) {
		reg_table = dev_pm_opp_set_regulators(dev, reg_names, 2);
		if (IS_ERR(reg_table))
			return PTR_ERR(reg_table);
		opp_table =
			dev_pm_opp_register_set_opp_helper(dev, npu_opp_helper);
		if (IS_ERR(opp_table)) {
			dev_pm_opp_put_regulators(reg_table);
			return PTR_ERR(opp_table);
		}
	} else {
		reg_table = dev_pm_opp_set_regulators(dev, reg_names, 1);
		if (IS_ERR(reg_table))
			return PTR_ERR(reg_table);
	}

	rockchip_get_opp_data(rockchip_npu_of_match, &rknpu3_dev->opp_info);
	ret = rockchip_init_opp_table(dev, &rknpu3_dev->opp_info, "npu_leakage",
				      "rknpu");
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to init_opp_table\n");
		return ret;
	}

	rknpu3_dev->current_freq = clk_get_rate(rknpu3_dev->clks[0].clk);

	opp = devfreq_recommended_opp(dev, &rknpu3_dev->current_freq, 0);
	if (IS_ERR(opp)) {
		ret = PTR_ERR(opp);
		goto err_remove_table;
	}
	dev_pm_opp_put(opp);
	dp->initial_freq = rknpu3_dev->current_freq;

	ret = devfreq_add_governor(&devfreq_rknpu3_ondemand);
	if (ret) {
		LOG_DEV_ERROR(dev, "failed to add rknpu3_ondemand governor\n");
		goto err_remove_table;
	}

	rknpu3_dev->devfreq = devm_devfreq_add_device(dev, dp, "rknpu3_ondemand",
						     (void *)rknpu3_dev);
	if (IS_ERR(rknpu3_dev->devfreq)) {
		LOG_DEV_ERROR(dev, "failed to add devfreq\n");
		ret = PTR_ERR(rknpu3_dev->devfreq);
		goto err_remove_governor;
	}

	npu_mdevp.data = rknpu3_dev->devfreq;
	npu_mdevp.opp_info = &rknpu3_dev->opp_info;
	rknpu3_dev->mdev_info =
		rockchip_system_monitor_register(dev, &npu_mdevp);
	if (IS_ERR(rknpu3_dev->mdev_info)) {
		LOG_DEV_DEBUG(dev, "without system monitor\n");
		rknpu3_dev->mdev_info = NULL;
		npu_mdevp.is_checked = true;
	}
	rknpu3_dev->current_freq = clk_get_rate(rknpu3_dev->clks[0].clk);
	rknpu3_dev->ondemand_freq = rknpu3_dev->current_freq;
	if (rknpu3_dev->vdd)
		rknpu3_dev->current_volt = regulator_get_voltage(rknpu3_dev->vdd);

	rknpu3_dev->devfreq->previous_freq = rknpu3_dev->current_freq;
	if (rknpu3_dev->devfreq->suspend_freq)
		rknpu3_dev->devfreq->resume_freq = rknpu3_dev->current_freq;
	rknpu3_dev->devfreq->last_status.current_frequency =
		rknpu3_dev->current_freq;
	rknpu3_dev->devfreq->last_status.total_time = 1;
	rknpu3_dev->devfreq->last_status.busy_time = 1;

	of_property_read_u32(dev->of_node, "dynamic-power-coefficient",
			     (u32 *)&npu_cooling_power.dyn_power_coeff);
	rknpu3_dev->model_data =
		rockchip_ipa_power_model_init(dev, "npu_leakage");
	if (IS_ERR_OR_NULL(rknpu3_dev->model_data)) {
		rknpu3_dev->model_data = NULL;
		LOG_DEV_ERROR(dev, "failed to initialize power model\n");
	} else if (rknpu3_dev->model_data->dynamic_coefficient) {
		npu_cooling_power.dyn_power_coeff =
			rknpu3_dev->model_data->dynamic_coefficient;
	}
	if (!npu_cooling_power.dyn_power_coeff) {
		LOG_DEV_ERROR(dev, "failed to get dynamic-coefficient\n");
		goto out;
	}

	rknpu3_dev->devfreq_cooling = of_devfreq_cooling_register_power(
		dev->of_node, rknpu3_dev->devfreq, &npu_cooling_power);
	if (IS_ERR_OR_NULL(rknpu3_dev->devfreq_cooling))
		LOG_DEV_ERROR(dev, "failed to register cooling device\n");

out:
	LOG_DEV_INFO(dev, "devfreq init success, freq: %lu Hz\n",
		     rknpu3_dev->current_freq);
	return 0;

err_remove_governor:
	devfreq_remove_governor(&devfreq_rknpu3_ondemand);
err_remove_table:
#if KERNEL_VERSION(5, 10, 198) <= LINUX_VERSION_CODE
	rockchip_uninit_opp_table(dev, &rknpu3_dev->opp_info);
#else
	dev_pm_opp_of_remove_table(dev);
#endif

	rknpu3_dev->devfreq = NULL;

	return ret;
}

int rknpu3_devfreq_runtime_suspend(struct device *dev)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu3_dev->opp_info;

	if (opp_info->scmi_clk) {
		if (clk_set_rate(opp_info->scmi_clk, POWER_DOWN_FREQ))
			LOG_DEV_ERROR(dev, "failed to restore clk rate\n");
	}
	opp_info->current_rm = UINT_MAX;

	return 0;
}

int rknpu3_devfreq_runtime_resume(struct device *dev)
{
	struct rknpu3_device *rknpu3_dev = dev_get_drvdata(dev);
	struct rockchip_opp_info *opp_info = &rknpu3_dev->opp_info;
	int ret = 0;

	if (!rknpu3_dev->current_freq || !rknpu3_dev->current_volt)
		return 0;

	ret = clk_bulk_prepare_enable(opp_info->num_clks, opp_info->clks);
	if (ret) {
		LOG_DEV_INFO(dev, "failed to enable opp clks\n");
		return ret;
	}

	if (opp_info->data && opp_info->data->set_read_margin)
		opp_info->data->set_read_margin(dev, opp_info,
						opp_info->target_rm);
	if (opp_info->scmi_clk) {
		if (clk_set_rate(opp_info->scmi_clk, rknpu3_dev->current_freq))
			LOG_DEV_ERROR(dev, "failed to set power down rate\n");
	}

	clk_bulk_disable_unprepare(opp_info->num_clks, opp_info->clks);

	return ret;
}
#endif /* KERNEL_VERSION */

void rknpu3_devfreq_remove(struct rknpu3_device *rknpu3_dev)
{
	if (rknpu3_dev->mdev_info) {
		rockchip_system_monitor_unregister(rknpu3_dev->mdev_info);
		rknpu3_dev->mdev_info = NULL;
	}
	if (rknpu3_dev->devfreq)
		devfreq_remove_governor(&devfreq_rknpu3_ondemand);
#if KERNEL_VERSION(5, 10, 198) <= LINUX_VERSION_CODE
	rockchip_uninit_opp_table(rknpu3_dev->dev, &rknpu3_dev->opp_info);
#else
	dev_pm_opp_of_remove_table(rknpu3_dev->dev);
#endif
}
