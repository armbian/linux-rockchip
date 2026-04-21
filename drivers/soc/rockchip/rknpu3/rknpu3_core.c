// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/iommu.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "rknpu3_config.h"
#include "rknpu3_core.h"
#include "rknpu3_reset.h"
#include "rknpu3_task.h"
#include "rknpu3_pqueue.h"
#include "rknpu3_utils.h"
#include "boot_params.h"
#include "rknpu3_drv.h"

/* RKNPU data register base offset and step */
#define RKNPU3_PORT_DATA_BASE_OFFSET 0x2450
#define RKNPU3_PORT_DATA_STEP        0x10
#define RKNPU3_PORT_FD_WRITE_OFFSET  0x00
#define RKNPU3_PORT_FD_READ_OFFSET   0x04
#define RKNPU3_PORT_WT_READ_OFFSET   0x08

#define RKNPU3_CYCLE_COUNT_START_OFFSET 0x1220
#define RKNPU3_CYCLE_COUNT_START_VALUE  0x1
#define RKNPU3_CYCLE_COUNT_CLEAR_OFFSET 0x1220
#define RKNPU3_CYCLE_COUNT_CLEAR_VALUE  0x2
#define RKNPU3_CYCLE_COUNT_CNT0 0x1224
#define RKNPU3_CYCLE_COUNT_CNT1 0x1228

#define RKNPU3_VERSION_OFFSET 0x0
#define RKNPU3_VERSION_NUM_OFFSET 0x4

#define RKNPU3_PC_MASK_TRIGGER_OFFSET 0x003c
#define RKNPU3_PC_MASK_TRIGGER_VALUE 0x0

#define RKNPU3_NN_DCACHE_CTRL_OFFSET 0x2604
#define RKNPU3_NN_DCACHE_DISABLE_VALUE 0x3000
#define RKNPU3_NN_ENTRY_ADDR_OFFSET 0x2608
#define RKNPU3_NN_TASK_TRIGGER_OFFSET 0x2600
#define RKNPU3_NN_TASK_TRIGGER_VALUE 0x1

/* FPGA debug registers (only used in rknpu3_core.c) */
#ifdef FPGA_PLATFORM
#define RKNPU3_FPGA_SET_AXI_OFFSET_OFFSET 0x2404
#define RKNPU3_FPGA_SET_AXI_OFFSET_VALUE 0x00aaaaa4
#define RKNPU3_FPGA_DISABLE_DCACHE_OFFSET 0x2604
#define RKNPU3_FPGA_DISABLE_DCACHE_VALUE 0x3000
#define RKNPU3_FPGA_ENABLE_PREDICTION_OFFSET 0x2604
#define RKNPU3_FPGA_ENABLE_PREDICTION_VALUE 0x10
#define RKNPU3_FPGA_REG_MIN_SIZE (RKNPU3_RESET_OFFSET + sizeof(u32))
#endif

/**
 * rknpu3_hw_submit_task() - Submit task to NPU hardware
 * @rknpu3_dev: RKNPU device
 * @core_id: Core ID
 * @entry_dma_addr: Entry DMA address (bin_kaddr DMA address + entry_point)
 * @enable_cycle_count: Whether to enable cycle counting
 * @disable_nn_dcache: Whether to disable NN dcache
 *
 * Return: 0 on success, negative error code on failure
 */
static int rknpu3_hw_submit_task(struct rknpu3_device *rknpu3_dev, uint32_t core_id,
				uint32_t entry_dma_addr, uint32_t enable_cycle_count,
				uint32_t disable_nn_dcache)
{
	void __iomem *base_addr;
	struct iommu_domain *domain;
	int ret;

	if (!rknpu3_dev)
		return -EINVAL;
	if (core_id >= rknpu3_dev->num_cores)
		return -EINVAL;

	base_addr = rknpu3_dev->base[core_id];
	if (!base_addr)
		return -EINVAL;

	ret = rknpu3_hw_core_reset(rknpu3_dev, core_id, false);
	if (ret)
		return ret;

	domain = iommu_get_domain_for_dev(rknpu3_dev->dev);
	if (domain)
		iommu_flush_iotlb_all(domain);

	if (enable_cycle_count)
		rknpu3_reg_write(base_addr, RKNPU3_CYCLE_COUNT_START_OFFSET,
				 RKNPU3_CYCLE_COUNT_START_VALUE);

	LOG_DEV_DEBUG(rknpu3_dev->dev, "submit task to core %d, entry_dma_addr: 0x%x\n",
		      core_id, entry_dma_addr);
	if (disable_nn_dcache == 1)
		rknpu3_reg_write(base_addr, RKNPU3_NN_DCACHE_CTRL_OFFSET,
				 RKNPU3_NN_DCACHE_DISABLE_VALUE);
	rknpu3_reg_write(base_addr, RKNPU3_NN_ENTRY_ADDR_OFFSET, entry_dma_addr);
	rknpu3_reg_write(base_addr, RKNPU3_NN_TASK_TRIGGER_OFFSET,
			 RKNPU3_NN_TASK_TRIGGER_VALUE);
	return 0;
}

/**
 * rknpu3_hw_clear_irq() - Clear interrupt
 * @rknpu3_dev: RKNPU device
 * @core_id: Core ID
 *
 * Return: 0 on success, negative error code on failure
 */
static int rknpu3_hw_clear_irq(struct rknpu3_device *rknpu3_dev, uint32_t core_id)
{
	void __iomem *base_addr;

	base_addr = rknpu3_dev->base[core_id];
	rknpu3_reg_write(base_addr, 0x60, 0x0);

	return 0;
}

/* Check if NPU task is completed */
static inline int rknpu3_check_status(uint64_t bin_kaddr)
{
	struct rknrv_boot_params *boot_params = (struct rknrv_boot_params *)bin_kaddr;

	return (boot_params->exit_status == 0xCAFFE000) ? 1 : 0;
}

/* Mark core as available and clear current task */
static inline void rknpu3_core_set_idle(struct rknpu3_device *dev, int core_id)
{
	dev->core_status[core_id].is_available = true;
	dev->core_status[core_id].current_task_id = 0;
	dev->core_status[core_id].current_task = NULL;
}

/* Set core busy with task */
static inline void rknpu3_core_set_busy(struct rknpu3_device *dev, int core_id,
					struct rknpu3_task *task)
{
	dev->core_status[core_id].is_available = false;
	dev->core_status[core_id].current_task_id = task->task_id;
	dev->core_status[core_id].current_task = task;
	dev->core_status[core_id].last_state_change_time = rknpu3_get_time_us();
}

/* Try to submit next task from queue in IRQ context */
static void rknpu3_irq_submit_next(struct rknpu3_device *dev, int core_id)
{
	struct rknpu3_task *task;
	struct rknrv_boot_params *boot_params;
	uint32_t entry_dma_addr;
	int ret;

	if (rknpu3_pqueue_is_empty(dev->core_queues[core_id]))
		return;

	if (rknpu3_pqueue_pop(dev->core_queues[core_id], &task) != 0)
		return;

	if (!task || task->state != RKNPU3_TASK_STATE_READY) {
		LOG_DEV_WARN(dev->dev, "next task invalid state\n");
		if (task)
			task->state = RKNPU3_TASK_STATE_ERROR;
		return;
	}

	if (!task->bin_kaddr) {
		LOG_DEV_ERROR(dev->dev, "task %d has invalid bin_kaddr\n",
			      task->task_id);
		task->state = RKNPU3_TASK_STATE_ERROR;
		return;
	}

	/* Prepare submission */
	boot_params = (struct rknrv_boot_params *)task->bin_kaddr;
	entry_dma_addr = task->bin_dma_addr + boot_params->entry_point;

	/* Set core busy before HW submit */
	rknpu3_core_set_busy(dev, core_id, task);
	task->state = RKNPU3_TASK_STATE_RUNNING;

	/* Submit to hardware */
	ret = rknpu3_hw_submit_task(dev, core_id, entry_dma_addr,
				    task->enable_cycle_count,
				    task->disable_nn_dcache);
	if (ret) {
		LOG_DEV_ERROR(dev->dev, "submit task %d to core %d failed\n",
			      task->task_id, core_id);
		rknpu3_core_set_idle(dev, core_id);
		task->state = RKNPU3_TASK_STATE_ERROR;
	}
}

/* Complete current task in IRQ context */
static void rknpu3_irq_complete_task(struct rknpu3_device *dev, int core_id,
				     struct rknpu3_task *task)
{
	struct rknpu3_core_status *status = &dev->core_status[core_id];
	uint64_t now_us = rknpu3_get_time_us();

	/* Update duty cycle stats */
	if (!status->is_available && now_us > status->last_state_change_time) {
		status->busy_time_accum_us += now_us - status->last_state_change_time;
		status->last_state_change_time = now_us;
	}

	/* Mark task completed */
	task->state = RKNPU3_TASK_STATE_COMPLETED;

	/* Mark core idle */
	rknpu3_core_set_idle(dev, core_id);

	/* Remove from queue */
	rknpu3_pqueue_remove(dev->core_queues[core_id], task->task_id);

	/* Try submit next task */
	rknpu3_irq_submit_next(dev, core_id);

	/* Wake waiters */
	atomic_inc(&dev->irq_event_seq);
	wake_up(&dev->wait_queue);
}

/* NPU IRQ handler */
static irqreturn_t rknpu3_irq_handler(int irq, void *param, int core_id)
{
	struct rknpu3_device *rknpu3_dev = param;
	struct rknpu3_task *task;
	unsigned long flags;

	if (!rknpu3_dev)
		return IRQ_NONE;
	if (core_id < 0 || core_id >= rknpu3_dev->num_cores)
		return IRQ_NONE;

	spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);

	rknpu3_hw_clear_irq(rknpu3_dev, core_id);

	task = rknpu3_dev->core_status[core_id].current_task;
	if (!task) {
		LOG_DEV_WARN(rknpu3_dev->dev,
			     "IRQ for core %d but no current task\n", core_id);
		goto unlock;
	}

	/* Validate task state */
	if (task->state != RKNPU3_TASK_STATE_RUNNING &&
	    task->state != RKNPU3_TASK_STATE_READY) {
		LOG_DEV_WARN(rknpu3_dev->dev,
			     "IRQ core %d task %d unexpected state %d\n",
			     core_id, task->task_id, task->state);
	}

	rknpu3_irq_complete_task(rknpu3_dev, core_id, task);

unlock:
	spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);

	return IRQ_HANDLED;
}

irqreturn_t rknpu3_core0_irq_handler(int irq, void *data)
{
	return rknpu3_irq_handler(irq, data, 0);
}

irqreturn_t rknpu3_core1_irq_handler(int irq, void *data)
{
	return rknpu3_irq_handler(irq, data, 1);
}

int rknpu3_core_module_init(struct rknpu3_device *rknpu3_dev)
{
	struct platform_device *pdev;
	struct resource *res;
	const struct rknpu3_config *config;
	int i, ret;
	int irq;

	if (!rknpu3_dev || !rknpu3_dev->dev || !rknpu3_dev->config)
		return -EINVAL;

	pdev = to_platform_device(rknpu3_dev->dev);
	config = rknpu3_dev->config;

	/* Get core count from config (IRQ count equals core count) */
	rknpu3_dev->num_cores = config->num_irqs;

	if (rknpu3_dev->num_cores <= 0 || rknpu3_dev->num_cores > RKNPU3_MAX_CORES) {
		dev_err(rknpu3_dev->dev, "invalid num_cores: %d\n", rknpu3_dev->num_cores);
		return -EINVAL;
	}

	dev_info(rknpu3_dev->dev, "initializing %d cores\n", rknpu3_dev->num_cores);

	/* Get and map register resources for each core */
	for (i = 0; i < rknpu3_dev->num_cores; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(rknpu3_dev->dev,
				"failed to get memory resource for core %d\n", i);
			ret = -ENXIO;
			goto err_unmap_bases;
		}

#ifdef FPGA_PLATFORM
		/* FPGA platform: use fixed 0x20000 size, ignore DT size */
		#define FPGA_REG_MAP_SIZE 0x20000

		dev_info(rknpu3_dev->dev,
			 "core %d: FPGA platform, using fixed size 0x%x (DT size: 0x%llx)\n",
			 i, FPGA_REG_MAP_SIZE, (u64)resource_size(res));

		rknpu3_dev->base[i] = devm_ioremap(rknpu3_dev->dev, res->start,
						  FPGA_REG_MAP_SIZE);
		if (!rknpu3_dev->base[i]) {
			dev_err(rknpu3_dev->dev, "failed to ioremap for core %d\n", i);
			ret = -ENOMEM;
			goto err_unmap_bases;
		}

		dev_info(rknpu3_dev->dev,
			 "core %d: mapped base 0x%llx (fixed size: 0x%x)\n",
			 i, (u64)res->start, FPGA_REG_MAP_SIZE);
#else
		rknpu3_dev->base[i] = devm_ioremap(rknpu3_dev->dev, res->start,
						  resource_size(res));
		if (!rknpu3_dev->base[i]) {
			dev_err(rknpu3_dev->dev, "failed to ioremap for core %d\n", i);
			ret = -ENOMEM;
			goto err_unmap_bases;
		}

		dev_info(rknpu3_dev->dev,
			 "core %d: mapped base 0x%llx (size: 0x%llx)\n",
			 i, (u64)res->start, (u64)resource_size(res));
#endif
	}

#ifndef FPGA_PLATFORM
	rknpu3_dev->npu_grf = syscon_regmap_lookup_by_phandle(rknpu3_dev->dev->of_node,
							      "rockchip,grf");
	if (IS_ERR(rknpu3_dev->npu_grf)) {
		ret = PTR_ERR(rknpu3_dev->npu_grf);
		dev_err(rknpu3_dev->dev, "failed to get NPU GRF regmap: %d\n", ret);
		goto err_unmap_bases;
	}
	dev_info(rknpu3_dev->dev, "NPU GRF regmap from rockchip,grf phandle\n");
#endif

	/* Initialize reset control resources */
	ret = rknpu3_reset_init(rknpu3_dev);
	if (ret) {
		dev_warn(rknpu3_dev->dev, "failed to init reset controls: %d\n", ret);
		/* Reset control init failure doesn't affect core functionality */
	}

	/* Allocate core status array */
	rknpu3_dev->core_status = devm_kcalloc(rknpu3_dev->dev,
					       rknpu3_dev->num_cores,
					       sizeof(struct rknpu3_core_status),
					       GFP_KERNEL);
	if (!rknpu3_dev->core_status)
		return -ENOMEM;

	/* Initialize core status */
	for (i = 0; i < rknpu3_dev->num_cores; i++) {
		rknpu3_dev->core_status[i].core_id = i;
		rknpu3_dev->core_status[i].is_available = true;
		rknpu3_dev->core_status[i].current_task_id = 0;
		rknpu3_dev->core_status[i].current_task = NULL;
		/* Initialize utilization statistics fields */
		rknpu3_dev->core_status[i].last_query_time = rknpu3_get_time_us();
		rknpu3_dev->core_status[i].last_state_change_time = rknpu3_get_time_us();
		rknpu3_dev->core_status[i].busy_time_accum_us = 0;
	}

	/* Initialize wait queue */
	init_waitqueue_head(&rknpu3_dev->wait_queue);
	atomic_set(&rknpu3_dev->irq_event_seq, 0);

	/* Register IRQ handler for each core */
	for (i = 0; i < rknpu3_dev->num_cores; i++) {
		/* First try to get IRQ by name */
		irq = platform_get_irq_byname(pdev, config->irqs[i].name);
		if (irq < 0) {
			/* If getting by name fails, try by index */
			irq = platform_get_irq(pdev, i);
			if (irq < 0) {
				dev_err(rknpu3_dev->dev,
					"failed to get IRQ %s for core %d\n",
					config->irqs[i].name, i);
				return irq;
			}
		}

		ret = devm_request_irq(rknpu3_dev->dev, irq, config->irqs[i].irq_hdl,
				       IRQF_SHARED, dev_name(rknpu3_dev->dev), rknpu3_dev);
		if (ret) {
			dev_err(rknpu3_dev->dev,
				"failed to request IRQ %d (%s) for core %d: %d\n",
				irq, config->irqs[i].name, i, ret);
			return ret;
		}

		rknpu3_dev->irq[i] = irq;
		dev_info(rknpu3_dev->dev, "core %d: registered IRQ %d (%s)\n",
			 i, irq, config->irqs[i].name);
	}

	dev_info(rknpu3_dev->dev, "core module initialized successfully\n");
	return 0;

err_unmap_bases:
	return ret;
}

void rknpu3_core_module_deinit(struct rknpu3_device *rknpu3_dev)
{
}

/**
 * rknpu3_core_submit_task() - Submit task to core
 * @rknpu3_dev: RKNPU device
 * @core_id: Core ID
 * @task: Task descriptor
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_core_submit_task(struct rknpu3_device *rknpu3_dev, uint32_t core_id,
			   struct rknpu3_task *task)
{
	struct rknrv_boot_params *boot_params;
	uint32_t entry_dma_addr;
	unsigned long flags;
	int ret;

	if (!rknpu3_dev || !task)
		return RKNPU3_ERROR_INVALID_PARAM;

	if (core_id >= rknpu3_dev->num_cores)
		return RKNPU3_ERROR_INVALID_PARAM;

	/* Prepare entry address */
	boot_params = (struct rknrv_boot_params *)task->bin_kaddr;
	entry_dma_addr = task->bin_dma_addr + boot_params->entry_point;

	spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);

	/* Check if core is available */
	if (!rknpu3_dev->core_status[core_id].is_available) {
		spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);
		return RKNPU3_ERROR_CORE_BUSY;
	}

	/* Set core busy before HW submit */
	rknpu3_core_set_busy(rknpu3_dev, core_id, task);
	task->state = RKNPU3_TASK_STATE_RUNNING;

	/* Submit task to NPU */
	ret = rknpu3_hw_submit_task(rknpu3_dev, core_id, entry_dma_addr,
				    task->enable_cycle_count,
				    task->disable_nn_dcache);
	if (ret) {
		rknpu3_core_set_idle(rknpu3_dev, core_id);
		task->state = RKNPU3_TASK_STATE_ERROR;
		spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);
		return RKNPU3_ERROR_OPERATION_FAILED;
	}

	spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);
	return RKNPU3_SUCCESS;
}

/**
 * rknpu3_core_get_load() - Get load information for all cores
 * @rknpu3_dev: RKNPU device
 * @load: Load info structure pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_core_get_load(struct rknpu3_device *rknpu3_dev, struct rknpu3_load *load)
{
	uint32_t i;
	uint64_t current_time;
	unsigned long flags;
	struct rknpu3_core_status *core_status;
	struct rknpu3_core_load *core_util;
	uint64_t period_us;
	uint64_t busy_us;
	uint32_t load_scaled;

	if (rknpu3_dev == NULL || load == NULL)
		return RKNPU3_ERROR_INVALID_PARAM;
	if (rknpu3_dev->num_cores <= 0 || rknpu3_dev->num_cores > RKNPU3_MAX_CORES)
		return RKNPU3_ERROR_INVALID_PARAM;

	current_time = rknpu3_get_time_us();
	load->actual_core_count = rknpu3_dev->num_cores;

	/* Acquire lock (interrupt safe) */
	spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);

	/* Calculate utilization (duty cycle) for each core */
	for (i = 0; i < rknpu3_dev->num_cores; i++) {
		core_status = &rknpu3_dev->core_status[i];
		core_util = &load->core_load[i];

		core_util->core_id = core_status->core_id;
		/* Calculate time since last query */
		period_us = current_time - core_status->last_query_time;
		if ((int64_t)period_us <= 0) {
			core_util->load = 0;
		} else {
			busy_us = core_status->busy_time_accum_us;
			if (!core_status->is_available) {
				/* Currently busy, temporarily add time until now */
				if (current_time > core_status->last_state_change_time)
					busy_us += (current_time - core_status->last_state_change_time);
			}

			/* Load percentage * 100 (0-10000) */
			load_scaled = (uint32_t)((busy_us * 10000ULL) / period_us);
			if (load_scaled > 10000)
				load_scaled = 10000;
			core_util->load = load_scaled;

			/* Prepare for next stat period: clear accumulated and update timestamp */
			core_status->busy_time_accum_us = 0;
			core_status->last_query_time = current_time;
			if (!core_status->is_available) {
				/* Keep busy state start at current time to avoid double counting */
				core_status->last_state_change_time = current_time;
			}
		}
	}

	/* Release lock */
	spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);
	return RKNPU3_SUCCESS;
}

/**
 * rknpu3_core_get_hw_version() - Get hardware version
 * @rknpu3_dev: RKNPU device
 * @hw_version: Hardware version output pointer (64-bit)
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_core_get_hw_version(struct rknpu3_device *rknpu3_dev, uint64_t *hw_version)
{
	if (!rknpu3_dev || !hw_version)
		return -EINVAL;

	if (!rknpu3_dev->base[0]) {
		LOG_DEV_ERROR(rknpu3_dev->dev, "base[0] is NULL\n");
		return -ENODEV;
	}

	*hw_version = readl(rknpu3_dev->base[0] + RKNPU3_VERSION_OFFSET) +
		      readl(rknpu3_dev->base[0] + RKNPU3_VERSION_NUM_OFFSET);

	return RKNPU3_SUCCESS;
}

/**
 * rknpu3_core_get_bw_info() - Get bandwidth information for all cores
 * @rknpu3_dev: RKNPU device
 * @bw_info: Bandwidth info structure pointer
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_core_get_bw_info(struct rknpu3_device *rknpu3_dev,
			   struct rknpu3_bw_info *bw_info)
{
	uint32_t i, j;
	uint32_t port_num;
	void __iomem *base_addr;
	uint32_t port_offset;

	if (!rknpu3_dev || !bw_info)
		return -EINVAL;

	if (!rknpu3_dev->config)
		return -EINVAL;

	port_num = rknpu3_dev->config->axi_port_num;

	bw_info->actual_core_count = rknpu3_dev->num_cores;
	bw_info->actual_port_count = port_num;

	/* Get bandwidth info for each core */
	for (i = 0; i < rknpu3_dev->num_cores; i++) {
		bw_info->core_bw[i].core_id = i;

		base_addr = rknpu3_dev->base[i];
		if (!base_addr)
			return -ENODEV;

		/* Read bandwidth data for each port */
		for (j = 0; j < port_num; j++) {
			port_offset = RKNPU3_PORT_DATA_BASE_OFFSET +
				      j * RKNPU3_PORT_DATA_STEP;
			bw_info->core_bw[i].port[j].fd_write =
				rknpu3_reg_read(base_addr, port_offset + RKNPU3_PORT_FD_WRITE_OFFSET) *
				rknpu3_dev->config->bus_bit / 8;
			bw_info->core_bw[i].port[j].fd_read =
				rknpu3_reg_read(base_addr, port_offset + RKNPU3_PORT_FD_READ_OFFSET) *
				rknpu3_dev->config->bus_bit / 8;
			bw_info->core_bw[i].port[j].wt_read =
				rknpu3_reg_read(base_addr, port_offset + RKNPU3_PORT_WT_READ_OFFSET) *
				rknpu3_dev->config->bus_bit / 8;
		}
	}

	return RKNPU3_SUCCESS;
}

/**
 * rknpu3_core_get_cycle_info() - Get cycle count for all cores and clear
 * @rknpu3_dev: RKNPU device
 * @cycle_info: Cycle count info structure pointer
 *
 * Return: 0 on success, negative error code on failure
 */
int rknpu3_core_get_cycle_info(struct rknpu3_device *rknpu3_dev,
			      struct rknpu3_cycle_info *cycle_info)
{
	uint32_t i;
	void __iomem *base_addr;
	uint32_t low, high;

	if (!rknpu3_dev || !cycle_info)
		return -EINVAL;

	cycle_info->actual_core_count = rknpu3_dev->num_cores;

	/* Read cycle count for all cores */
	for (i = 0; i < rknpu3_dev->num_cores; i++) {
		base_addr = rknpu3_dev->base[i];
		if (!base_addr) {
			cycle_info->core_cycle[i].core_id = i;
			cycle_info->core_cycle[i].cycle_count = 0;
			continue;
		}

		cycle_info->core_cycle[i].core_id = i;

		low = rknpu3_reg_read(base_addr, RKNPU3_CYCLE_COUNT_CNT0);
		high = rknpu3_reg_read(base_addr, RKNPU3_CYCLE_COUNT_CNT1);
		cycle_info->core_cycle[i].cycle_count = ((uint64_t)high << 32) | low;

		/* Clear cycle count */
		rknpu3_reg_write(base_addr, RKNPU3_CYCLE_COUNT_CLEAR_OFFSET,
				 RKNPU3_CYCLE_COUNT_CLEAR_VALUE);
	}

	return RKNPU3_SUCCESS;
}
