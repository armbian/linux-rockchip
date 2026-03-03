/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 */

#ifndef __RKNPU3_TYPES_H__
#define __RKNPU3_TYPES_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/miscdevice.h>
#include <linux/dma-buf.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/devfreq.h>
#include <linux/workqueue.h>

#ifndef FPGA_PLATFORM
#include <soc/rockchip/rockchip_opp_select.h>
#include <soc/rockchip/rockchip_system_monitor.h>
#include <soc/rockchip/rockchip_ipa.h>
#endif

#include "rknpu3_ioctl.h"

/* Maximum core count */
#define RKNPU3_MAX_CORES 8

/**
 * struct rknpu3_core_status - Core status structure
 * @core_id: Core ID
 * @is_available: Whether core is available
 * @current_task_id: Currently running task ID
 * @current_task: Currently running task pointer
 * @last_query_time: Last query time (microseconds)
 * @last_state_change_time: Last state change time (microseconds)
 * @busy_time_accum_us: Accumulated busy time since last query (microseconds)
 */
struct rknpu3_core_status {
	uint32_t core_id;
	bool is_available;
	uint32_t current_task_id;
	struct rknpu3_task *current_task;

	/* Utilization statistics fields */
	uint64_t last_query_time;
	uint64_t last_state_change_time;
	uint64_t busy_time_accum_us;
};

/**
 * struct rknpu3_irqs_data - IRQ data structure
 * @name: IRQ name
 * @irq_hdl: IRQ handler
 */
struct rknpu3_irqs_data {
	const char *name;
	irq_handler_t irq_hdl;
};

/**
 * struct rknpu3_reset_data - Reset data structure
 */
struct rknpu3_reset_data {
	const char *srst_a_npu_name;
	const char *srst_a_nrv_name;
};

/**
 * struct rknpu3_config - Driver configuration structure
 * @iommu_en: Whether IOMMU is enabled
 * @enable_profiling: Whether profiling is enabled
 * @dma_mask: DMA mask
 * @irqs: IRQ configuration array
 * @resets: Reset configuration array
 * @num_cores: Physical core count
 * @axi_port_num: AXI port count
 * @num_irqs: IRQ count
 * @num_resets: Reset count
 * @bus_bit: Bus width
 */
struct rknpu3_config {
	int iommu_en;
	bool enable_profiling;
	__u64 dma_mask;
	const struct rknpu3_irqs_data *irqs;
	const struct rknpu3_reset_data *resets;
	int num_cores;
	int axi_port_num;
	int num_irqs;
	int num_resets;
	int bus_bit;
};

/**
 * struct rknpu3_device - RKNPU device structure
 * @dev: Device pointer
 * @miscdev: Misc device
 * @base: IO mapped base address array
 * @npu_grf: NPU GRF regmap (from syscon)
 * @dev_lock: Device spinlock
 * @core_status: Core status array
 * @num_cores: Number of cores
 * @next_task_id: Next task ID
 * @global_queue: Global task queue
 * @core_queues: Per-core task queues
 * @total_memory: Total memory
 * @used_memory: Used memory
 * @core_memory_usage: Per-core memory usage
 * @wait_queue: Wait queue
 * @irq_event_seq: IRQ event sequence number
 * @irq: IRQ number array
 * @config: Configuration info
 * @clks: Clock array
 * @num_clks: Clock count
 * @vdd: Power regulator
 * @mem: Memory power regulator
 * @srst_a: AXI reset control
 * @srst_h: AHB reset control
 * @iommu_en: Whether IOMMU is enabled
 * @reserved_mem_attached: Whether reserved memory is attached
 * @iommu_domain: IOMMU domain
 * @iommu_dev: IOMMU device for runtime PM
 * @mdev_info: Monitor device info
 * @model_data: IPA power model data
 * @devfreq_cooling: Devfreq cooling device
 * @devfreq: Devfreq
 * @ondemand_freq: On-demand frequency
 * @opp_info: OPP info
 * @current_freq: Current frequency
 * @current_volt: Current voltage
 */
struct rknpu3_device {
	struct device *dev;
	struct miscdevice miscdev;

	void __iomem *base[RKNPU3_MAX_CORES];
	struct regmap *npu_grf;

	spinlock_t dev_lock;

	/* Core management */
	struct rknpu3_core_status *core_status;
	int num_cores;

	/* Task management */
	uint32_t next_task_id;
	struct rknpu3_pqueue *global_queue;
	struct rknpu3_pqueue **core_queues;

	/* Memory management */
	uint64_t total_memory;
	uint64_t used_memory;
	uint64_t core_memory_usage[RKNPU3_MAX_CORES];

	/* IRQ handling */
	wait_queue_head_t wait_queue;
	atomic_t irq_event_seq;
	int irq[RKNPU3_MAX_CORES];

	/* Hardware resources */
	const struct rknpu3_config *config;
	struct clk_bulk_data *clks;
	int num_clks;
	struct regulator *vdd;
	struct regulator *mem;
	struct reset_control *srst_a_npu[RKNPU3_MAX_CORES];
	struct reset_control *srst_a_nrv[RKNPU3_MAX_CORES];
	bool iommu_en;
	bool reserved_mem_attached;
	struct iommu_domain *iommu_domain;
	struct device *iommu_dev;

	/* NBUF (NPU internal buffer) mapping */
	phys_addr_t nbuf_phyaddr;
	size_t nbuf_size;
	bool nbuf_mapped;

	/* Devfreq frequency management */
	struct monitor_dev_info *mdev_info;
	struct ipa_power_model_data *model_data;
	struct thermal_cooling_device *devfreq_cooling;
	struct devfreq *devfreq;
	unsigned long ondemand_freq;
#ifndef FPGA_PLATFORM
	struct rockchip_opp_info opp_info;
#endif
	unsigned long current_freq;
	unsigned long current_volt;

	/* Power management */
	struct mutex power_lock;
	atomic_t power_refcount;
	struct delayed_work power_off_work;
	struct workqueue_struct *power_off_wq;
	unsigned long power_put_delay;
};

/**
 * struct rknpu3_session - Per-file session structure for resource tracking
 * @rknpu3_dev: Pointer to RKNPU device
 * @imported_bufs_lock: Mutex for protecting imported_bufs list
 * @imported_bufs: List of imported DMA-BUFs (rknpu3_imported_buf nodes)
 */
struct rknpu3_session {
	struct rknpu3_device *rknpu3_dev;
	struct mutex imported_bufs_lock;
	struct list_head imported_bufs;
};

#endif /* __RKNPU3_TYPES_H__ */
