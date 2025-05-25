/*
 * Rockchip CPUFreq Driver
 *
 * Copyright (C) 2017 Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpuidle.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/rockchip/cpu.h>
#include <soc/rockchip/rockchip_opp_select.h>
#include <soc/rockchip/rockchip_system_monitor.h>

#include "cpufreq-dt.h"
#include "rockchip-cpufreq.h"

struct cluster_info {
	struct list_head list_head;
	struct monitor_dev_info *mdev_info;
	struct rockchip_opp_info opp_info;
	struct freq_qos_request bus_qos_req;
	cpumask_t cpus;
	unsigned int idle_threshold_freq;
	unsigned int cpu_freq_percent;
	bool is_idle_disabled;
	bool is_opp_shared_cpu_bus;
	unsigned long rate;
	unsigned long volt, mem_volt;
};
static LIST_HEAD(cluster_info_list);

static struct cluster_info *rockchip_cluster_info_lookup(int cpu);

static int px30_get_soc_info(struct device *dev, struct device_node *np,
			     int *bin, int *process)
{
	int ret = 0;
	u8 value = 0;

	if (!bin)
		return 0;

	if (of_property_match_string(np, "nvmem-cell-names",
				     "performance") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np, "performance", &value);
		if (ret) {
			dev_err(dev, "Failed to get soc performance value\n");
			return ret;
		}
		*bin = value;
	}
	if (*bin >= 0)
		dev_info(dev, "bin=%d\n", *bin);

	return ret;
}

static int rk3288_get_soc_info(struct device *dev, struct device_node *np,
			       int *bin, int *process)
{
	int ret = 0;
	u8 value = 0;
	char *name;

	if (!bin)
		goto next;
	if (of_property_match_string(np, "nvmem-cell-names", "special") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np, "special", &value);
		if (ret) {
			dev_err(dev, "Failed to get soc special value\n");
			goto out;
		}
		if (value == 0xc)
			*bin = 0;
		else
			*bin = 1;
	}

	if (soc_is_rk3288w())
		name = "performance-w";
	else
		name = "performance";

	if (of_property_match_string(np, "nvmem-cell-names", name) >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np, name, &value);
		if (ret) {
			dev_err(dev, "Failed to get soc performance value\n");
			goto out;
		}
		if (value & 0x2)
			*bin = 3;
		else if (value & 0x01)
			*bin = 2;
	}
	if (*bin >= 0)
		dev_info(dev, "bin=%d\n", *bin);

next:
	if (!process)
		goto out;
	if (of_property_match_string(np, "nvmem-cell-names",
				     "process") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np, "process", &value);
		if (ret) {
			dev_err(dev, "Failed to get soc process version\n");
			goto out;
		}
		if (soc_is_rk3288() && (value == 0 || value == 1))
			*process = 0;
	}
	if (*process >= 0)
		dev_info(dev, "process=%d\n", *process);

out:
	return ret;
}

static int rk3399_get_soc_info(struct device *dev, struct device_node *np,
			       int *bin, int *process)
{
	int ret = 0;
	u8 value = 0;

	if (!bin)
		return 0;

	if (of_property_match_string(np, "nvmem-cell-names",
				     "specification_serial_number") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np,
						  "specification_serial_number",
						  &value);
		if (ret) {
			dev_err(dev,
				"Failed to get specification_serial_number\n");
			goto out;
		}

		if (value == 0xb) {
			*bin = 0;
		} else if (value == 0x1) {
			if (of_property_match_string(np, "nvmem-cell-names",
						     "customer_demand") >= 0) {
				ret = rockchip_nvmem_cell_read_u8(np,
								  "customer_demand",
								  &value);
				if (ret) {
					dev_err(dev, "Failed to get customer_demand\n");
					goto out;
				}
				if (value == 0x0)
					*bin = 0;
				else
					*bin = 1;
			}
		} else if (value == 0x10) {
			*bin = 1;
		}
	}

out:
	if (*bin >= 0)
		dev_info(dev, "bin=%d\n", *bin);

	return ret;
}

static int rk3576_cpu_get_soc_info(struct device *dev, struct device_node *np,
				   int *bin, int *process)
{
	int ret = 0;
	u8 spec = 0, test_version = 0;

	if (!bin)
		return 0;

	if (of_property_match_string(np, "nvmem-cell-names",
				     "specification_serial_number") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np,
						  "specification_serial_number",
						  &spec);
		if (ret) {
			dev_err(dev,
				"Failed to get specification_serial_number\n");
			return ret;
		}

	}
	if (of_property_match_string(np, "nvmem-cell-names", "test_version") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np, "test_version", &test_version);
		if (ret) {
			dev_err(dev, "Failed to get test_version\n");
			return ret;
		}
	}
	/* RK3576M */
	if (spec == 0xd) {
		*bin = 1;
	/* RK3576J */
	} else if (spec == 0xa) {
		*bin = 2;
	/* RK3576S */
	} else if (spec == 0x13) {
		if (test_version == 0) {
			*bin = 3;
		} else {
			*bin = 0;
			dev_info(dev, "bin=%d (3)\n", *bin);
			return 0;
		}
	}

	if (*bin < 0)
		*bin = 0;
	dev_info(dev, "bin=%d\n", *bin);

	return ret;
}

static int rk3576_cpu_set_read_margin(struct device *dev,
				      struct rockchip_opp_info *opp_info,
				      u32 rm)
{
	if (!opp_info->volt_rm_tbl)
		return 0;
	if (rm == opp_info->current_rm || rm  == UINT_MAX)
		return 0;

	dev_dbg(dev, "set rm to %d\n", rm);
	if (opp_info->grf) {
		regmap_write(opp_info->grf, 0x3c, 0x001c0000 | (rm << 2));
		regmap_write(opp_info->grf, 0x44, 0x001c0000 | (rm << 2));
		regmap_write(opp_info->grf, 0x38, 0x00020002);
		udelay(1);
		regmap_write(opp_info->grf, 0x38, 0x00020000);
	}
	if (opp_info->cci_grf)
		regmap_write(opp_info->cci_grf, 0x54, 0x001c0000 | (rm << 2));

	opp_info->current_rm = rm;

	return 0;
}

static int rk3588_get_soc_info(struct device *dev, struct device_node *np,
			       int *bin, int *process)
{
	int ret = 0;
	u8 value = 0;

	if (!bin)
		return 0;

	if (of_property_match_string(np, "nvmem-cell-names",
				     "specification_serial_number") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np,
						  "specification_serial_number",
						  &value);
		if (ret) {
			dev_err(dev,
				"Failed to get specification_serial_number\n");
			return ret;
		}
		/* RK3588M */
		if (value == 0xd)
			*bin = 1;
		/* RK3588J */
		else if (value == 0xa)
			*bin = 2;
	}
	if (of_property_match_string(np, "nvmem-cell-names", "customer_demand") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np, "customer_demand", &value);
		if (ret) {
			dev_err(dev, "Failed to get customer_demand\n");
			return ret;
		}
		if (value == 0x3)
			*bin = 4;
	}
	if (*bin < 0)
		*bin = 0;
	dev_info(dev, "bin=%d\n", *bin);

	return ret;
}

static int rk3588_change_length(struct device *dev, struct device_node *np,
				struct rockchip_opp_info *opp_info)
{
	struct clk *clk;
	unsigned long old_rate;
	unsigned int low_len_sel;
	u32 opp_flag = 0;
	int ret = 0;

	if (opp_info->volt_sel < 0)
		return 0;

	clk = clk_get(dev, NULL);
	if (IS_ERR(clk)) {
		dev_warn(dev, "failed to get cpu clk\n");
		return PTR_ERR(clk);
	}

	/* RK3588 low speed grade should change to low length */
	if (of_property_read_u32(np, "rockchip,pvtm-low-len-sel",
				 &low_len_sel))
		goto out;
	if (opp_info->volt_sel > low_len_sel)
		goto out;
	opp_flag = OPP_LENGTH_LOW;

	old_rate = clk_get_rate(clk);
	ret = clk_set_rate(clk, old_rate | opp_flag);
	if (ret) {
		dev_err(dev, "failed to change length\n");
		goto out;
	}
	clk_set_rate(clk, old_rate);
out:
	clk_put(clk);

	return ret;
}

static int rk3588_set_supported_hw(struct device *dev, struct device_node *np,
				   struct rockchip_opp_info *opp_info)
{
	int bin = opp_info->bin;

	if (!of_property_read_bool(np, "rockchip,supported-hw"))
		return 0;

	if (bin < 0)
		bin = 0;

	/* SoC Version */
	opp_info->supported_hw[0] = BIT(bin);
	/* Speed Grade */
	opp_info->supported_hw[1] = BIT(opp_info->volt_sel);

	return 0;
}

static int rk3588_set_soc_info(struct device *dev, struct device_node *np,
			       struct rockchip_opp_info *opp_info)
{
	rk3588_change_length(dev, np, opp_info);
	rk3588_set_supported_hw(dev, np, opp_info);

	return 0;
}

static int rk3588_cpu_set_read_margin(struct device *dev,
				      struct rockchip_opp_info *opp_info,
				      u32 rm)
{
	if (!opp_info->volt_rm_tbl)
		return 0;
	if (rm == opp_info->current_rm || rm  == UINT_MAX)
		return 0;

	dev_dbg(dev, "set rm to %d\n", rm);
	if (opp_info->grf) {
		regmap_write(opp_info->grf, 0x20, 0x001c0000 | (rm << 2));
		regmap_write(opp_info->grf, 0x28, 0x003c0000 | (rm << 2));
		regmap_write(opp_info->grf, 0x2c, 0x003c0000 | (rm << 2));
		regmap_write(opp_info->grf, 0x30, 0x00200020);
		udelay(1);
		regmap_write(opp_info->grf, 0x30, 0x00200000);
	}
	if (opp_info->dsu_grf) {
		regmap_write(opp_info->dsu_grf, 0x20, 0x001c0000 | (rm << 2));
		regmap_write(opp_info->dsu_grf, 0x28, 0x003c0000 | (rm << 2));
		regmap_write(opp_info->dsu_grf, 0x2c, 0x003c0000 | (rm << 2));
		regmap_write(opp_info->dsu_grf, 0x30, 0x001c0000 | (rm << 2));
		regmap_write(opp_info->dsu_grf, 0x38, 0x001c0000 | (rm << 2));
		regmap_write(opp_info->dsu_grf, 0x18, 0x40004000);
		udelay(1);
		regmap_write(opp_info->dsu_grf, 0x18, 0x40000000);
	}

	opp_info->current_rm = rm;

	return 0;
}

static int cpu_opp_config_regulators(struct device *dev,
				     struct dev_pm_opp *old_opp,
				     struct dev_pm_opp *new_opp,
				     struct regulator **regulators,
				     unsigned int count)
{
	struct cluster_info *cluster;

	cluster = rockchip_cluster_info_lookup(dev->id);
	if (!cluster)
		return -EINVAL;

	return rockchip_opp_config_regulators(dev, old_opp, new_opp, regulators,
					      count, &cluster->opp_info);
}

static int rv1126_get_soc_info(struct device *dev, struct device_node *np,
			       int *bin, int *process)
{
	int ret = 0;
	u8 value = 0;

	if (of_property_match_string(np, "nvmem-cell-names", "performance") >= 0) {
		ret = rockchip_nvmem_cell_read_u8(np, "performance", &value);
		if (ret) {
			dev_err(dev, "Failed to get soc performance value\n");
			return ret;
		}
		if (value == 0x1)
			*bin = 1;
		else
			*bin = 0;
	}
	if (*bin >= 0)
		dev_info(dev, "bin=%d\n", *bin);

	return ret;
}

static const struct rockchip_opp_data px30_cpu_opp_data = {
	.get_soc_info = px30_get_soc_info,
};

static const struct rockchip_opp_data rk3288_cpu_opp_data = {
	.get_soc_info = rk3288_get_soc_info,
};

static const struct rockchip_opp_data rk3399_cpu_opp_data = {
	.get_soc_info = rk3399_get_soc_info,
};

static const struct rockchip_opp_data rk3588_cpu_opp_data = {
	.get_soc_info = rk3588_get_soc_info,
	.set_soc_info = rk3588_set_soc_info,
	.set_read_margin = rk3588_cpu_set_read_margin,
	.config_regulators = cpu_opp_config_regulators,
};

static const struct rockchip_opp_data rk3576_cpu_opp_data = {
	.get_soc_info = rk3576_cpu_get_soc_info,
	.set_read_margin = rk3576_cpu_set_read_margin,
	.set_soc_info = rockchip_opp_set_low_length,
	.config_regulators = cpu_opp_config_regulators,
};

static const struct rockchip_opp_data rv1126_cpu_opp_data = {
	.get_soc_info = rv1126_get_soc_info,
};

static const struct of_device_id rockchip_cpufreq_of_match[] = {
	{
		.compatible = "rockchip,px30",
		.data = (void *)&px30_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rk3288",
		.data = (void *)&rk3288_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rk3288w",
		.data = (void *)&rk3288_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rk3326",
		.data = (void *)&px30_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rk3399",
		.data = (void *)&rk3399_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rk3576",
		.data = (void *)&rk3576_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rk3576s",
		.data = (void *)&rk3576_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rk3588",
		.data = (void *)&rk3588_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rv1109",
		.data = (void *)&rv1126_cpu_opp_data,
	},
	{
		.compatible = "rockchip,rv1126",
		.data = (void *)&rv1126_cpu_opp_data,
	},
	{},
};

static struct cluster_info *rockchip_cluster_info_lookup(int cpu)
{
	struct cluster_info *cluster;

	list_for_each_entry(cluster, &cluster_info_list, list_head) {
		if (cpumask_test_cpu(cpu, &cluster->cpus))
			return cluster;
	}

	return NULL;
}

static int rockchip_cpufreq_cluster_init(int cpu, struct cluster_info *cluster)
{
	struct rockchip_opp_info *opp_info = &cluster->opp_info;
	struct device_node *np;
	struct device *dev;
	char *reg_name;
	int ret = 0;
	u32 freq = 0;

	dev = get_cpu_device(cpu);
	if (!dev)
		return -ENODEV;

	np = of_parse_phandle(dev->of_node, "operating-points-v2", 0);
	if (!np) {
		dev_warn(dev, "OPP-v2 not supported\n");
		return -ENOENT;
	}
	ret = dev_pm_opp_of_get_sharing_cpus(dev, &cluster->cpus);
	if (ret) {
		dev_err(dev, "Failed to get sharing cpus\n");
		of_node_put(np);
		return ret;
	}
	if (of_property_read_bool(np, "rockchip,opp-shared-dsu") ||
	    of_property_read_bool(np, "rockchip,opp-shared-cci"))
		cluster->is_opp_shared_cpu_bus = true;
	of_property_read_u32(np, "rockchip,cpu-freq-percent",
			     &cluster->cpu_freq_percent);
	if (!of_property_read_u32(np, "rockchip,idle-threshold-freq", &freq))
		cluster->idle_threshold_freq = freq;
	of_node_put(np);

	if (of_find_property(dev->of_node, "cpu-supply", NULL))
		reg_name = "cpu";
	else if (of_find_property(dev->of_node, "cpu0-supply", NULL))
		reg_name = "cpu0";
	else
		return -ENOENT;
	rockchip_get_opp_data(rockchip_cpufreq_of_match, opp_info);
	ret = rockchip_init_opp_info(dev, opp_info, NULL, reg_name);
	if (ret)
		dev_err(dev, "failed to init opp info\n");

	return ret;
}

int rockchip_cpufreq_adjust_table(struct device *dev)
{
	struct cluster_info *cluster;

	cluster = rockchip_cluster_info_lookup(dev->id);
	if (!cluster)
		return -EINVAL;

	return rockchip_adjust_opp_table(dev, &cluster->opp_info);
}
EXPORT_SYMBOL_GPL(rockchip_cpufreq_adjust_table);

int rockchip_cpufreq_opp_set_rate(struct device *dev, unsigned long target_freq)
{
	struct cluster_info *cluster;
	struct dev_pm_opp *opp;
	struct rockchip_opp_info *opp_info;
	struct dev_pm_opp_supply supplies[2] = {0};
	unsigned long freq;
	int ret = 0;

	cluster = rockchip_cluster_info_lookup(dev->id);
	if (!cluster)
		return -EINVAL;
	opp_info = &cluster->opp_info;

	rockchip_opp_dvfs_lock(opp_info);
	ret = dev_pm_opp_set_rate(dev, target_freq);
	if (!ret) {
		cluster->rate = freq = target_freq;
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (!IS_ERR(opp)) {
			dev_pm_opp_get_supplies(opp, supplies);
			cluster->volt = supplies[0].u_volt;
			if (opp_info->regulator_count > 1)
				cluster->mem_volt = supplies[1].u_volt;
			dev_pm_opp_put(opp);
		}
	}
	rockchip_opp_dvfs_unlock(opp_info);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_cpufreq_opp_set_rate);

static int rockchip_cpufreq_suspend(struct cpufreq_policy *policy)
{
	int ret = 0;

	ret = cpufreq_generic_suspend(policy);
	if (!ret)
		rockchip_monitor_suspend_low_temp_adjust(policy->cpu);

	return ret;
}

int rockchip_cpufreq_online(int cpu)
{
	struct cluster_info *cluster;
	struct rockchip_opp_info *opp_info;

	cluster = rockchip_cluster_info_lookup(cpu);
	if (!cluster)
		return -EINVAL;
	opp_info = &cluster->opp_info;

	opp_info->is_runtime_active = true;
	if (opp_info->data && opp_info->data->set_read_margin)
		opp_info->data->set_read_margin(opp_info->dev, opp_info,
						opp_info->target_rm);

	return 0;
}
EXPORT_SYMBOL_GPL(rockchip_cpufreq_online);

int rockchip_cpufreq_offline(int cpu)
{
	struct cluster_info *cluster;
	struct rockchip_opp_info *opp_info;

	cluster = rockchip_cluster_info_lookup(cpu);
	if (!cluster)
		return -EINVAL;
	opp_info = &cluster->opp_info;

	opp_info->is_runtime_active = false;
	opp_info->current_rm = UINT_MAX;

	return 0;
}
EXPORT_SYMBOL_GPL(rockchip_cpufreq_offline);

static int rockchip_cpufreq_add_monitor(struct cluster_info *cluster,
					struct cpufreq_policy *policy)
{
	struct device *dev = cluster->opp_info.dev;
	struct monitor_dev_profile *mdevp = NULL;
	struct monitor_dev_info *mdev_info = NULL;

	mdevp = kzalloc(sizeof(*mdevp), GFP_KERNEL);
	if (!mdevp)
		return -ENOMEM;

	mdevp->type = MONITOR_TYPE_CPU;
	mdevp->low_temp_adjust = rockchip_monitor_cpu_low_temp_adjust;
	mdevp->high_temp_adjust = rockchip_monitor_cpu_high_temp_adjust;
	mdevp->check_rate_volt = rockchip_monitor_check_rate_volt;
	mdevp->data = (void *)policy;
	mdevp->opp_info = &cluster->opp_info;
	cpumask_copy(&mdevp->allowed_cpus, policy->cpus);
	mdev_info = rockchip_system_monitor_register(dev, mdevp);
	if (IS_ERR(mdev_info)) {
		kfree(mdevp);
		dev_err(dev, "failed to register system monitor\n");
		return -EINVAL;
	}
	mdev_info->devp = mdevp;
	cluster->mdev_info = mdev_info;

	return 0;
}

static int rockchip_cpufreq_remove_monitor(struct cluster_info *cluster)
{
	if (cluster->mdev_info) {
		kfree(cluster->mdev_info->devp);
		rockchip_system_monitor_unregister(cluster->mdev_info);
		cluster->mdev_info = NULL;
	}

	return 0;
}

static int rockchip_cpufreq_remove_bus_qos(struct cluster_info *cluster)
{
	struct cluster_info *ci;

	if (!cluster->is_opp_shared_cpu_bus)
		return 0;

	list_for_each_entry(ci, &cluster_info_list, list_head) {
		if (ci->is_opp_shared_cpu_bus)
			continue;
		if (freq_qos_request_active(&ci->bus_qos_req))
			freq_qos_remove_request(&ci->bus_qos_req);
	}

	return 0;
}

static int rockchip_cpufreq_add_bus_qos_req(struct cluster_info *cluster,
					    struct cpufreq_policy *policy)
{
	struct device *dev = cluster->opp_info.dev;
	struct cluster_info *ci;
	int ret;

	if (!cluster->is_opp_shared_cpu_bus)
		return 0;

	list_for_each_entry(ci, &cluster_info_list, list_head) {
		if (ci->is_opp_shared_cpu_bus)
			continue;
		ret = freq_qos_add_request(&policy->constraints,
					   &ci->bus_qos_req,
					   FREQ_QOS_MIN,
					   FREQ_QOS_MIN_DEFAULT_VALUE);
		if (ret < 0) {
			dev_err(dev, "failed to add cpu bus freq constraint\n");
			goto error;
		}
	}

	return 0;

error:
	rockchip_cpufreq_remove_bus_qos(cluster);

	return ret;
}

static int rockchip_cpufreq_notifier(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	struct cluster_info *cluster;

	cluster = rockchip_cluster_info_lookup(policy->cpu);
	if (!cluster)
		return NOTIFY_BAD;

	if (event == CPUFREQ_CREATE_POLICY) {
		if (rockchip_cpufreq_add_monitor(cluster, policy))
			return NOTIFY_BAD;
		if (rockchip_cpufreq_add_bus_qos_req(cluster, policy))
			return NOTIFY_BAD;
	} else if (event == CPUFREQ_REMOVE_POLICY) {
		rockchip_cpufreq_remove_monitor(cluster);
		rockchip_cpufreq_remove_bus_qos(cluster);
	}

	return NOTIFY_OK;
}

static struct notifier_block rockchip_cpufreq_notifier_block = {
	.notifier_call = rockchip_cpufreq_notifier,
};

#ifdef MODULE
static struct pm_qos_request idle_pm_qos;
static int idle_disable_refcnt;
static DEFINE_MUTEX(idle_disable_lock);

static int rockchip_cpufreq_idle_state_disable(struct cpumask *cpumask,
					       int index, bool disable)
{
	mutex_lock(&idle_disable_lock);

	if (disable) {
		if (idle_disable_refcnt == 0)
			cpu_latency_qos_update_request(&idle_pm_qos, 0);
		idle_disable_refcnt++;
	} else {
		if (--idle_disable_refcnt == 0)
			cpu_latency_qos_update_request(&idle_pm_qos,
						       PM_QOS_DEFAULT_VALUE);
	}

	mutex_unlock(&idle_disable_lock);

	return 0;
}
#else
static int rockchip_cpufreq_idle_state_disable(struct cpumask *cpumask,
					       int index, bool disable)
{
	unsigned int cpu;

	for_each_cpu(cpu, cpumask) {
		struct cpuidle_device *dev = per_cpu(cpuidle_devices, cpu);
		struct cpuidle_driver *drv = cpuidle_get_cpu_driver(dev);

		if (!dev || !drv)
			continue;
		if (index >= drv->state_count)
			continue;
		cpuidle_driver_state_disabled(drv, index, disable);
	}

	if (disable) {
		preempt_disable();
		for_each_cpu(cpu, cpumask) {
			if (cpu != smp_processor_id() && cpu_online(cpu))
				wake_up_if_idle(cpu);
		}
		preempt_enable();
	}

	return 0;
}
#endif

#define cpu_to_bus_freq(freq)  ((freq) * 4 / 5)

static int rockchip_cpufreq_update_bus_req(struct cluster_info *cluster,
					   unsigned int freq)
{
	struct device *dev = cluster->opp_info.dev;
	unsigned int bus_freq = 0;

	if (cluster->is_opp_shared_cpu_bus ||
	    !freq_qos_request_active(&cluster->bus_qos_req))
		return 0;

	if (cluster->cpu_freq_percent)
		bus_freq = rounddown(freq * cluster->cpu_freq_percent / 100, 100000);
	else
		bus_freq = rounddown(cpu_to_bus_freq(freq), 100000);

	dev_dbg(dev, "cpu to bus: %u -> %u\n", freq, bus_freq);

	return freq_qos_update_request(&cluster->bus_qos_req, bus_freq);
}

static int rockchip_cpufreq_transition_notifier(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct cpufreq_freqs *freqs = data;
	struct cpufreq_policy *policy = freqs->policy;
	struct cluster_info *cluster;

	cluster = rockchip_cluster_info_lookup(policy->cpu);
	if (!cluster)
		return NOTIFY_BAD;

	if (event == CPUFREQ_PRECHANGE) {
		if (cluster->idle_threshold_freq &&
		    freqs->new >= cluster->idle_threshold_freq &&
		    !cluster->is_idle_disabled) {
			rockchip_cpufreq_idle_state_disable(policy->cpus, 1,
							    true);
			cluster->is_idle_disabled = true;
		}
	} else if (event == CPUFREQ_POSTCHANGE) {
		if (cluster->idle_threshold_freq &&
		    freqs->new < cluster->idle_threshold_freq &&
		    cluster->is_idle_disabled) {
			rockchip_cpufreq_idle_state_disable(policy->cpus, 1,
							    false);
			cluster->is_idle_disabled = false;
		}
		rockchip_cpufreq_update_bus_req(cluster, freqs->new);
	}

	return NOTIFY_OK;
}

static struct notifier_block rockchip_cpufreq_transition_notifier_block = {
	.notifier_call = rockchip_cpufreq_transition_notifier,
};

static int rockchip_cpufreq_panic_notifier(struct notifier_block *nb,
					   unsigned long v, void *p)
{
	struct cluster_info *ci;
	struct rockchip_opp_info *opp_info;

	list_for_each_entry(ci, &cluster_info_list, list_head) {
		opp_info = &ci->opp_info;

		if (opp_info->regulator_count > 1)
			dev_info(opp_info->dev,
				 "cur_freq: %lu Hz, volt_vdd: %lu uV, volt_mem: %lu uV\n",
				 ci->rate, ci->volt, ci->mem_volt);
		else
			dev_info(opp_info->dev, "cur_freq: %lu Hz, volt: %lu uV\n",
				 ci->rate, ci->volt);
	}

	return 0;
}

static struct notifier_block rockchip_cpufreq_panic_notifier_block = {
	.notifier_call = rockchip_cpufreq_panic_notifier,
};

static int __init rockchip_cpufreq_driver_init(void)
{
	struct cluster_info *cluster, *pos;
	struct cpufreq_dt_platform_data pdata = {0};
	int cpu, ret;
	bool is_opp_shared_cpu_bus = false;

	for_each_possible_cpu(cpu) {
		cluster = rockchip_cluster_info_lookup(cpu);
		if (cluster)
			continue;

		cluster = kzalloc(sizeof(*cluster), GFP_KERNEL);
		if (!cluster) {
			ret = -ENOMEM;
			goto release_cluster_info;
		}

		ret = rockchip_cpufreq_cluster_init(cpu, cluster);
		if (ret) {
			pr_err("Failed to initialize dvfs info cpu%d\n", cpu);
			goto release_cluster_info;
		}
		list_add(&cluster->list_head, &cluster_info_list);
		if (cluster->is_opp_shared_cpu_bus)
			is_opp_shared_cpu_bus = true;
	}

	pdata.have_governor_per_policy = true;
	pdata.suspend = rockchip_cpufreq_suspend;

	ret = cpufreq_register_notifier(&rockchip_cpufreq_notifier_block,
					CPUFREQ_POLICY_NOTIFIER);
	if (ret) {
		pr_err("failed to register cpufreq notifier\n");
		goto release_cluster_info;
	}

	if (is_opp_shared_cpu_bus) {
		ret = cpufreq_register_notifier(&rockchip_cpufreq_transition_notifier_block,
						CPUFREQ_TRANSITION_NOTIFIER);
		if (ret) {
			cpufreq_unregister_notifier(&rockchip_cpufreq_notifier_block,
						    CPUFREQ_POLICY_NOTIFIER);
			pr_err("failed to register cpufreq notifier\n");
			goto release_cluster_info;
		}
#ifdef MODULE
		cpu_latency_qos_add_request(&idle_pm_qos, PM_QOS_DEFAULT_VALUE);
#endif
	}

	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &rockchip_cpufreq_panic_notifier_block);
	if (ret)
		pr_err("failed to register cpufreq panic notifier\n");

	return PTR_ERR_OR_ZERO(platform_device_register_data(NULL, "cpufreq-dt",
			       -1, (void *)&pdata,
			       sizeof(struct cpufreq_dt_platform_data)));

release_cluster_info:
	list_for_each_entry_safe(cluster, pos, &cluster_info_list, list_head) {
		list_del(&cluster->list_head);
		kfree(cluster);
	}
	return ret;
}
module_init(rockchip_cpufreq_driver_init);

MODULE_AUTHOR("Finley Xiao <finley.xiao@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip cpufreq driver");
MODULE_LICENSE("GPL v2");
