// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 */

#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/sched/isolation.h>
#include <linux/rockchip/rockchip_sip.h>

#define CORTEX_A72_CPUACTLR_EL1		S3_1_C15_C2_0
#define CORTEX_A72_ECTLR_EL1		S3_1_C15_C2_1
#define CORTEX_A72_DISABLE_PREFETCH	BIT(56)

#define RK3572_CCI_MAX_PRIO		7

static void rk3572_set_cci_qos(void)
{
	struct cpumask isolated;
	int ret, cpu, cluster_id;

	cpumask_andnot(&isolated, cpu_possible_mask, housekeeping_cpumask(HK_TYPE_DOMAIN));
	if (!cpumask_empty(&isolated)) {
		for_each_cpu(cpu, &isolated) {
			cluster_id = topology_cluster_id(cpu);

			pr_info("%s: cpu: %d, cluster_id: %d, qos=%d\n",
				__func__, cpu, cluster_id, RK3572_CCI_MAX_PRIO);
			ret = sip_smc_cci_config(CCI_SLV_AR_QOS_CFG, cluster_id,
						 RK3572_CCI_MAX_PRIO);
			if (ret)
				pr_err("%s: cpu%d set CCI_SLV_AR_QOS_CFG err, ret: %d\n",
					__func__, cpu, ret);
			ret = sip_smc_cci_config(CCI_SLV_AW_QOS_CFG, cluster_id,
						 RK3572_CCI_MAX_PRIO);
			if (ret)
				pr_err("%s: cpu%d set CCI_SLV_AW_QOS_CFG err, ret: %d\n",
					__func__, cpu, ret);
			cpumask_andnot(&isolated, &isolated, topology_cluster_cpumask(cpu));
		}
	}
}

static void rk3576_disable_a72_prefetch(void *arg)
{
#ifdef CONFIG_ARM64
	unsigned long val;
	int ret;

	val = read_sysreg(CORTEX_A72_CPUACTLR_EL1);
	pr_info("%s: cpu%d default CORTEX_A72_CPUACTLR_EL1: %lx\n",
		__func__, smp_processor_id(), val);
	val |= CORTEX_A72_DISABLE_PREFETCH;
	ret = sip_smc_access_cpu_reg(RK_MEM_OS_REG_WRITE, CPU_REG_A72_CPUACTLR_EL1, &val);
	if (ret)
		pr_err("%s: set cpu%d CPU_REG_A72_CPUACTLR_EL1 err, ret: %d\n",
		       __func__, smp_processor_id(), ret);
	val = read_sysreg(CORTEX_A72_CPUACTLR_EL1);
	pr_info("%s: cpu%d updated CORTEX_A72_CPUACTLR_EL1: %lx\n",
		__func__, smp_processor_id(), val);
#else
	pr_warn("%s: A72 prefetch disable not supported on ARM32\n", __func__);
#endif
}

static void rk3576_disable_a72_prefetch_all(void)
{
	struct device_node *of_node;
	struct cpumask mask;
	int cpu;

	cpumask_clear(&mask);
	for_each_online_cpu(cpu) {
		of_node = of_get_cpu_node(cpu, NULL);
		if (of_node) {
			if (of_device_is_compatible(of_node, "arm,cortex-a72")) {
				if (cpu == smp_processor_id())
					rk3576_disable_a72_prefetch(NULL);
				else
					cpumask_set_cpu(cpu, &mask);
			}
			of_node_put(of_node);
		}
	}
	smp_call_function_many(&mask, rk3576_disable_a72_prefetch, NULL, true);
}

static int __init rockchip_rt_init(void)
{
	if (IS_ENABLED(CONFIG_CPU_RK3572) && of_machine_is_compatible("rockchip,rk3572"))
		rk3572_set_cci_qos();

	if (IS_ENABLED(CONFIG_CPU_RK3576) && of_machine_is_compatible("rockchip,rk3576"))
		rk3576_disable_a72_prefetch_all();

	return 0;
}
late_initcall_sync(rockchip_rt_init);
