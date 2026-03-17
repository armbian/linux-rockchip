// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU Task Management Module
 */

#include <linux/atomic.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>

#include "rknpu3_task.h"
#include "rknpu3_core.h"
#include "rknpu3_reset.h"
#include "rknpu3_pqueue.h"
#include "rknpu3_config.h"
#include "rknpu3_utils.h"
#include "rknpu3_memory.h"
#include "rknpu3_drv.h"

/**
 * rknpu3_task_module_init() - Initialize task management module
 * @rknpu3_dev: RKNPU device
 * @config: Configuration info
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_module_init(struct rknpu3_device *rknpu3_dev,
			   struct rknpu3_config *config)
{
	struct device *dev;
	uint32_t core_count;
	uint32_t i;

	if (!rknpu3_dev || !config)
		return RKNPU3_ERROR_INVALID_PARAM;
	if (rknpu3_dev->num_cores <= 0 || rknpu3_dev->num_cores > RKNPU3_MAX_CORES)
		return RKNPU3_ERROR_INVALID_PARAM;

	dev = rknpu3_dev->dev;
	core_count = rknpu3_dev->num_cores;

	/* Allocate global task queue */
	rknpu3_dev->global_queue = devm_kzalloc(dev, sizeof(struct rknpu3_pqueue),
					       GFP_KERNEL);
	if (!rknpu3_dev->global_queue)
		return RKNPU3_ERROR_NO_MEMORY;

	rknpu3_pqueue_init(rknpu3_dev->global_queue, "global_queue");

	/* Allocate core queue pointer array */
	rknpu3_dev->core_queues = devm_kcalloc(dev, core_count,
					       sizeof(struct rknpu3_pqueue *),
					       GFP_KERNEL);
	if (!rknpu3_dev->core_queues)
		return RKNPU3_ERROR_NO_MEMORY;

	/* Allocate and initialize each core's task queue */
	for (i = 0; i < core_count; i++) {
		char queue_name[32];

		rknpu3_dev->core_queues[i] = devm_kzalloc(dev,
							  sizeof(struct rknpu3_pqueue),
							  GFP_KERNEL);
		if (!rknpu3_dev->core_queues[i])
			return RKNPU3_ERROR_NO_MEMORY;

		snprintf(queue_name, sizeof(queue_name), "core%d_queue", i);
		rknpu3_pqueue_init(rknpu3_dev->core_queues[i], queue_name);
	}

	/* Initialize task ID, starting from 1, 0 reserved as invalid */
	rknpu3_dev->next_task_id = 1;

	return RKNPU3_SUCCESS;
}

/**
 * rknpu3_task_module_reset() - Reset task management module
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_module_reset(struct rknpu3_device *rknpu3_dev)
{
	uint32_t core_count;
	uint32_t i;
	struct rknpu3_task *task;

	if (rknpu3_dev == NULL)
		return RKNPU3_ERROR_INVALID_PARAM;
	if (rknpu3_dev->num_cores <= 0 || rknpu3_dev->num_cores > RKNPU3_MAX_CORES)
		return RKNPU3_ERROR_INVALID_PARAM;

	core_count = rknpu3_dev->num_cores;

	/* Clear global queue */
	while (!rknpu3_pqueue_is_empty(rknpu3_dev->global_queue))
		rknpu3_pqueue_pop(rknpu3_dev->global_queue, &task);

	/* Clear each core's queue */
	for (i = 0; i < core_count; i++) {
		while (!rknpu3_pqueue_is_empty(rknpu3_dev->core_queues[i]))
			rknpu3_pqueue_pop(rknpu3_dev->core_queues[i], &task);
	}

	/* Reset task ID */
	rknpu3_dev->next_task_id = 1;

	return RKNPU3_SUCCESS;
}

/**
 * rknpu3_task_module_deinit() - Deinitialize task management module
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_module_deinit(struct rknpu3_device *rknpu3_dev)
{
	struct rknpu3_task *task;
	uint32_t core_count;
	uint32_t i;

	if (!rknpu3_dev)
		return RKNPU3_ERROR_INVALID_PARAM;

	core_count = (rknpu3_dev->num_cores > 0 &&
		      rknpu3_dev->num_cores <= RKNPU3_MAX_CORES) ?
		     rknpu3_dev->num_cores : 0;

	/* Clear remaining tasks from core queues */
	if (rknpu3_dev->core_queues) {
		for (i = 0; i < core_count; i++) {
			if (!rknpu3_dev->core_queues[i])
				continue;
			while (!rknpu3_pqueue_is_empty(rknpu3_dev->core_queues[i]))
				rknpu3_pqueue_pop(rknpu3_dev->core_queues[i], &task);
			rknpu3_pqueue_deinit(rknpu3_dev->core_queues[i]);
		}
	}

	/* Clear remaining tasks from global queue */
	if (rknpu3_dev->global_queue) {
		while (!rknpu3_pqueue_is_empty(rknpu3_dev->global_queue))
			rknpu3_pqueue_pop(rknpu3_dev->global_queue, &task);
		rknpu3_pqueue_deinit(rknpu3_dev->global_queue);
	}

	/* Memory is freed automatically by devm */
	return RKNPU3_SUCCESS;
}

/**
 * rknpu3_task_get() - Get task by ID
 * @rknpu3_dev: RKNPU device
 * @task_id: Task ID
 * @task: Pointer to task descriptor pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_get(struct rknpu3_device *rknpu3_dev, uint32_t task_id,
		   struct rknpu3_task **task)
{
	int ret;
	uint32_t core_count;
	uint32_t i;

	if (rknpu3_dev == NULL || task == NULL)
		return RKNPU3_ERROR_INVALID_PARAM;
	if (rknpu3_dev->num_cores <= 0 || rknpu3_dev->num_cores > RKNPU3_MAX_CORES)
		return RKNPU3_ERROR_INVALID_PARAM;

	core_count = rknpu3_dev->num_cores;

	/* First search in global queue */
	ret = rknpu3_pqueue_find(rknpu3_dev->global_queue, task_id, task);
	if (ret == RKNPU3_SUCCESS)
		return RKNPU3_SUCCESS;

	/* Then search in each core queue */
	for (i = 0; i < core_count; i++) {
		ret = rknpu3_pqueue_find(rknpu3_dev->core_queues[i], task_id, task);
		if (ret == RKNPU3_SUCCESS)
			return RKNPU3_SUCCESS;
	}

	return RKNPU3_ERROR_INVALID_TASK;
}

/**
 * rknpu3_push_to_global_queue() - Push task to global queue with error logging
 */
static int rknpu3_push_to_global_queue(struct rknpu3_device *rknpu3_dev,
				       struct rknpu3_task *task_obj)
{
	int ret;

	ret = rknpu3_pqueue_push(rknpu3_dev->global_queue, task_obj);
	if (ret == RKNPU3_SUCCESS)
		return RKNPU3_SUCCESS;

	if (ret == RKNPU3_ERROR_QUEUE_FULL)
		LOG_DEV_ERROR(rknpu3_dev->dev,
			      "global queue full, size: %d, task %d\n",
			      rknpu3_pqueue_size(rknpu3_dev->global_queue),
			      task_obj->task_id);
	else
		LOG_DEV_ERROR(rknpu3_dev->dev,
			      "push task %d to global queue failed, ret: %d\n",
			      task_obj->task_id, ret);
	return ret;
}

/**
 * rknpu3_push_to_core_queue() - Push task to core queue, fallback to global
 */
static int rknpu3_push_to_core_queue(struct rknpu3_device *rknpu3_dev,
				     struct rknpu3_task *task_obj,
				     uint32_t core_id)
{
	int ret;

	if (core_id >= rknpu3_dev->num_cores) {
		LOG_DEV_ERROR(rknpu3_dev->dev, "invalid core: %d\n", core_id);
		return RKNPU3_ERROR_INVALID_PARAM;
	}

	ret = rknpu3_pqueue_push(rknpu3_dev->core_queues[core_id], task_obj);
	if (ret == RKNPU3_SUCCESS)
		return RKNPU3_SUCCESS;

	if (ret != RKNPU3_ERROR_QUEUE_FULL) {
		LOG_DEV_ERROR(rknpu3_dev->dev,
			      "push task %d to core %d failed, ret: %d\n",
			      task_obj->task_id, core_id, ret);
		return ret;
	}

	/* Core queue full, try global queue as fallback */
	LOG_DEV_ERROR(rknpu3_dev->dev, "core %d queue full, size: %d\n",
		      core_id,
		      rknpu3_pqueue_size(rknpu3_dev->core_queues[core_id]));

	task_obj->core_id = 0xFFFFFFFF;
	ret = rknpu3_push_to_global_queue(rknpu3_dev, task_obj);
	if (ret == RKNPU3_SUCCESS)
		LOG_DEV_INFO(rknpu3_dev->dev, "task %d moved to global queue\n",
			     task_obj->task_id);

	return ret;
}

/**
 * rknpu3_tasks_submit() - Submit one or more tasks
 * @rknpu3_dev: RKNPU device
 * @task_submit: Task submit descriptor
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_tasks_submit(struct rknpu3_device *rknpu3_dev,
		       struct rknpu3_task_submit *task_submit)
{
	struct rknpu3_task *task_array_kaddr;
	struct rknpu3_task *task_obj;
	unsigned long flags;
	uint32_t i;
	int ret;

	if (!rknpu3_dev || !task_submit)
		return RKNPU3_ERROR_INVALID_PARAM;

	if (task_submit->task_count == 0 || task_submit->task_array_kaddr == 0)
		return RKNPU3_ERROR_INVALID_PARAM;

	task_array_kaddr = (struct rknpu3_task *)(task_submit->task_array_kaddr);
	task_submit->start_time_us = rknpu3_get_time_us();

	spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);

	for (i = 0; i < task_submit->task_count; i++) {
		task_obj = task_array_kaddr + task_submit->task_start + i;

		/* Validate userspace addresses */
		if (task_obj->bin_kaddr == 0 || task_obj->bin_dma_addr == 0) {
			LOG_DEV_ERROR(rknpu3_dev->dev,
				"invalid bin_kaddr(0x%llx) or bin_dma_addr(0x%x) for task %d\n",
				task_obj->bin_kaddr, task_obj->bin_dma_addr, i);
			ret = RKNPU3_ERROR_INVALID_PARAM;
			break;
		}

		/* Setup task */
		task_obj->enable_cycle_count = task_submit->enable_cycle_count;
		task_obj->disable_nn_dcache = task_submit->disable_nn_dcache;
		if (rknpu3_dev->next_task_id == 0)
			rknpu3_dev->next_task_id = 1;
		task_obj->task_id = rknpu3_dev->next_task_id++;
		task_obj->state = RKNPU3_TASK_STATE_READY;

		/* Push to queue */
		if (task_obj->core_id != 0xFFFFFFFF)
			ret = rknpu3_push_to_core_queue(rknpu3_dev, task_obj,
						       task_obj->core_id);
		else
			ret = rknpu3_push_to_global_queue(rknpu3_dev, task_obj);

		if (ret != RKNPU3_SUCCESS)
			break;

		ret = RKNPU3_SUCCESS;
	}

	/* Release lock */
	spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);

	/* If enqueue succeeded, try to schedule tasks to available cores */
	if (ret == RKNPU3_SUCCESS) {
#ifdef RKNPU3_DEBUG_QUEUE_STATUS
		/* Print queue status for debugging */
		LOG_DEV_INFO(rknpu3_dev->dev,
			"task submit - Global queue: %d/%d\n",
			rknpu3_pqueue_size(rknpu3_dev->global_queue),
			rknpu3_pqueue_get_available_nodes(rknpu3_dev->global_queue));
		for (uint32_t j = 0; j < rknpu3_dev->num_cores; j++) {
			LOG_DEV_INFO(rknpu3_dev->dev, "Core[%d]: %d/%d\n", j,
				rknpu3_pqueue_size(rknpu3_dev->core_queues[j]),
				rknpu3_pqueue_get_available_nodes(rknpu3_dev->core_queues[j]));
		}
#endif
		ret = rknpu3_task_schedule(rknpu3_dev);
	} else {
		LOG_DEV_ERROR(rknpu3_dev->dev,
			      "tasks submit failed with ret: %d, submitted: %d/%d\n",
			      ret, i, task_submit->task_count);
#ifdef RKNPU3_DEBUG_QUEUE_STATUS
		/* Print queue status on failure */
		LOG_DEV_INFO(rknpu3_dev->dev, "Queue status on submit failure:\n");
		rknpu3_pqueue_debug_info(rknpu3_dev->global_queue, "global");
		for (uint32_t j = 0; j < rknpu3_dev->num_cores; j++) {
			char core_name[16];

			snprintf(core_name, sizeof(core_name), "core%d", j);
			rknpu3_pqueue_debug_info(rknpu3_dev->core_queues[j], core_name);
		}
#endif
	}

	return ret;
}

/* Mark task as error and wake up waiters */
static void rknpu3_task_set_error(struct rknpu3_device *dev,
				  struct rknpu3_task *task)
{
	task->state = RKNPU3_TASK_STATE_ERROR;
	atomic_inc(&dev->irq_event_seq);
	wake_up(&dev->wait_queue);
}

/* Check if task was already processed by IRQ handler */
static bool rknpu3_task_already_done(struct rknpu3_task *task)
{
	return task->state == RKNPU3_TASK_STATE_COMPLETED ||
	       task->state == RKNPU3_TASK_STATE_ERROR;
}

/*
 * Schedule task from core queue to its designated core.
 * Called with dev_lock held, may temporarily release it.
 */
static void rknpu3_schedule_core_queue(struct rknpu3_device *dev,
				       uint32_t local_id,
				       unsigned long *flags)
{
	struct rknpu3_pqueue *queue = dev->core_queues[local_id];
	uint32_t core_id = rknpu3_core_to_global_id(local_id);
	struct rknpu3_task *task;
	uint32_t task_id;
	int ret;

	if (!dev->core_status[local_id].is_available)
		return;
	if (rknpu3_pqueue_is_empty(queue))
		return;
	if (rknpu3_pqueue_peek(queue, &task) != RKNPU3_SUCCESS || !task)
		return;

	task_id = task->task_id;

	/* Remove invalid task */
	if (task->state != RKNPU3_TASK_STATE_READY) {
		LOG_DEV_WARN(dev->dev, "task %d invalid state %d, removing\n",
			     task_id, task->state);
		rknpu3_pqueue_pop(queue, &task);
		return;
	}

	/* Submit task (release lock during HW access) */
	spin_unlock_irqrestore(&dev->dev_lock, *flags);
	ret = rknpu3_core_submit_task(dev, core_id, task);
	spin_lock_irqsave(&dev->dev_lock, *flags);

	/* Task may have been processed by IRQ while lock was released */
	if (rknpu3_task_already_done(task)) {
		LOG_DEV_DEBUG(dev->dev, "task %d already done, state=%d\n",
			      task_id, task->state);
		return;
	}

	if (ret == RKNPU3_SUCCESS)
		return;

	/* Core busy - keep in queue for retry */
	if (ret == RKNPU3_ERROR_CORE_BUSY) {
		LOG_DEV_INFO(dev->dev, "core %d busy, task %d retry later\n",
			     core_id, task_id);
		return;
	}

	/* Other errors - remove task and mark error */
	LOG_DEV_ERROR(dev->dev, "task %d submit to core %d failed, ret %d\n",
		      task_id, core_id, ret);
	rknpu3_pqueue_pop(queue, &task);
	rknpu3_task_set_error(dev, task);
}

/*
 * Try to assign a global queue task to an available core.
 * Returns: 1 = task assigned/handled, 0 = no core available, -1 = queue empty
 */
static int rknpu3_try_assign_global_task(struct rknpu3_device *dev,
					 unsigned long *flags)
{
	struct rknpu3_task *task;
	uint32_t core_count;
	uint32_t task_id, i, core_id;
	int ret;

	core_count = dev->num_cores;

	if (rknpu3_pqueue_peek(dev->global_queue, &task) != RKNPU3_SUCCESS)
		return -1;
	if (!task)
		return -1;

	task_id = task->task_id;

	/* Remove invalid task */
	if (task->state != RKNPU3_TASK_STATE_READY) {
		LOG_DEV_WARN(dev->dev, "global task %d invalid state %d\n",
			     task_id, task->state);
		rknpu3_pqueue_pop(dev->global_queue, &task);
		return 1;
	}

	/* Find available core */
	for (i = 0; i < core_count; i++) {
		if (!dev->core_status[i].is_available)
			continue;

		core_id = rknpu3_core_to_global_id(i);
		task->core_id = core_id;

		/* Submit task (release lock during HW access) */
		spin_unlock_irqrestore(&dev->dev_lock, *flags);
		ret = rknpu3_core_submit_task(dev, core_id, task);
		spin_lock_irqsave(&dev->dev_lock, *flags);

		/* Task processed by IRQ while lock was released */
		if (rknpu3_task_already_done(task)) {
			LOG_DEV_DEBUG(dev->dev, "task %d done after submit\n",
				      task_id);
			rknpu3_pqueue_remove(dev->global_queue, task_id);
			return 1;
		}

		if (ret == RKNPU3_SUCCESS) {
			/* Move task from global queue to core queue */
			rknpu3_pqueue_pop(dev->global_queue, &task);
			ret = rknpu3_pqueue_push(dev->core_queues[i], task);
			if (ret != RKNPU3_SUCCESS) {
				LOG_DEV_ERROR(dev->dev,
					      "failed to track task %d\n",
					      task_id);
				rknpu3_task_set_error(dev, task);
			}
			return 1;
		}

		/* Core busy or operation failed - try next core */
		if (ret == RKNPU3_ERROR_CORE_BUSY ||
		    ret == RKNPU3_ERROR_OPERATION_FAILED) {
			task->core_id = 0xFFFFFFFF;
			continue;
		}

		/* Fatal error - remove task */
		LOG_DEV_ERROR(dev->dev, "task %d failed, ret %d\n",
			      task_id, ret);
		rknpu3_pqueue_pop(dev->global_queue, &task);
		rknpu3_task_set_error(dev, task);
		return 1;
	}

	return 0; /* No available core */
}

/**
 * rknpu3_task_schedule() - Schedule tasks to available cores
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_schedule(struct rknpu3_device *rknpu3_dev)
{
	unsigned long flags;
	uint32_t core_count;
	uint32_t i;

	if (!rknpu3_dev)
		return RKNPU3_ERROR_INVALID_PARAM;
	if (rknpu3_dev->num_cores <= 0 || rknpu3_dev->num_cores > RKNPU3_MAX_CORES)
		return RKNPU3_ERROR_INVALID_PARAM;

	core_count = rknpu3_dev->num_cores;

	spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);

	/* Process core queues */
	for (i = 0; i < core_count; i++)
		rknpu3_schedule_core_queue(rknpu3_dev, i, &flags);

	/* Process global queue */
	while (rknpu3_try_assign_global_task(rknpu3_dev, &flags) > 0)
		;

	spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);

	return RKNPU3_SUCCESS;
}

/* Check if task is done (completed or error) */
static inline bool rknpu3_task_is_done(struct rknpu3_task *task)
{
	return task->state == RKNPU3_TASK_STATE_COMPLETED ||
	       task->state == RKNPU3_TASK_STATE_ERROR;
}

/* Count pending tasks and build pending mask */
static uint32_t rknpu3_count_pending_tasks(struct rknpu3_task *tasks,
					   uint32_t start, uint32_t count,
					   uint32_t *mask)
{
	uint32_t pending = 0;
	uint32_t i;

	*mask = 0;
	for (i = 0; i < count; i++) {
		if (!rknpu3_task_is_done(&tasks[start + i])) {
			*mask |= (1 << i);
			pending++;
		}
	}
	return pending;
}

/* Update pending status after IRQ event */
static void rknpu3_update_pending_tasks(struct rknpu3_task *tasks,
					uint32_t start, uint32_t count,
					uint32_t *mask, uint32_t *pending)
{
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (!(*mask & (1 << i)))
			continue;
		if (rknpu3_task_is_done(&tasks[start + i])) {
			*mask &= ~(1 << i);
			(*pending)--;
		}
	}
}

/* Handle timeout: mark tasks as error and reset cores */
static void rknpu3_handle_timeout(struct rknpu3_device *dev,
				  struct rknpu3_task *tasks,
				  uint32_t start, uint32_t count,
				  uint32_t pending_mask)
{
	uint64_t reset_bin_kaddr[RKNPU3_MAX_CORES] = {0};
	bool reset_core[RKNPU3_MAX_CORES] = {false};
	uint32_t core_count;
	struct rknpu3_task *task;
	unsigned long flags;
	uint32_t core_id, i;

	if (dev->num_cores <= 0 || dev->num_cores > RKNPU3_MAX_CORES)
		return;

	core_count = dev->num_cores;

	spin_lock_irqsave(&dev->dev_lock, flags);

	for (i = 0; i < count; i++) {
		if (!(pending_mask & (1 << i)))
			continue;

		task = &tasks[start + i];
		core_id = task->core_id;

		LOG_DEV_INFO(dev->dev, "Task %d timeout, core %d, state: %d\n",
			     task->task_id, core_id, task->state);

		task->state = RKNPU3_TASK_STATE_ERROR;

		if (core_id >= core_count)
			continue;

		/* Mark core for reset */
		if (!reset_core[core_id]) {
			reset_core[core_id] = true;
			reset_bin_kaddr[core_id] = task->bin_kaddr;
		}

		/* Reset core status */
		dev->core_status[core_id].current_task_id = 0;
		dev->core_status[core_id].current_task = NULL;

		/* Remove from queue */
		rknpu3_pqueue_remove(dev->core_queues[core_id], task->task_id);
	}

	spin_unlock_irqrestore(&dev->dev_lock, flags);

	/* Reset cores outside lock */
	for (i = 0; i < core_count; i++) {
		if (reset_core[i])
			rknpu3_core_reset(dev, i, reset_bin_kaddr[i]);
	}
}

/* Clean up completed tasks from queues */
static void rknpu3_cleanup_completed_tasks(struct rknpu3_device *dev,
					   struct rknpu3_task *tasks,
					   uint32_t start, uint32_t count)
{
	unsigned long flags;
	uint32_t i, core_id;

	spin_lock_irqsave(&dev->dev_lock, flags);

	for (i = 0; i < count; i++) {
		struct rknpu3_task *task = &tasks[start + i];

		if (!rknpu3_task_is_done(task))
			continue;

		core_id = task->core_id;
		if (core_id < dev->num_cores)
			rknpu3_pqueue_remove(dev->core_queues[core_id],
					     task->task_id);
	}

	spin_unlock_irqrestore(&dev->dev_lock, flags);
}

/**
 * rknpu3_tasks_wait() - Wait for one or more tasks to complete
 * @rknpu3_dev: RKNPU device
 * @task_submit: Task submit descriptor
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_tasks_wait(struct rknpu3_device *rknpu3_dev,
		     struct rknpu3_task_submit *task_submit)
{
	struct rknpu3_task *tasks;
	uint32_t pending_mask, tasks_pending;
	uint64_t start_time, timeout_us, elapsed_us;
	unsigned int event_seq;
	unsigned long flags;
	int ret = RKNPU3_SUCCESS;

	if (!rknpu3_dev || !task_submit)
		return RKNPU3_ERROR_INVALID_PARAM;
	if (task_submit->task_count == 0 || task_submit->task_array_kaddr == 0)
		return RKNPU3_ERROR_INVALID_PARAM;
	if (task_submit->timeout_ms == 0)
		return RKNPU3_ERROR_INVALID_PARAM;
	if (task_submit->task_count > RKNPU3_MAX_PENDING_TASKS)
		return RKNPU3_ERROR_INVALID_PARAM;

	tasks = (struct rknpu3_task *)(task_submit->task_array_kaddr);
	timeout_us = task_submit->timeout_ms * 1000ULL;
	start_time = rknpu3_get_time_us();
	event_seq = atomic_read(&rknpu3_dev->irq_event_seq);

	/* Count pending tasks */
	spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);
	tasks_pending = rknpu3_count_pending_tasks(tasks, task_submit->task_start,
						   task_submit->task_count,
						   &pending_mask);
	spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);

	/* Wait loop */
	while (tasks_pending > 0) {
		uint64_t remaining_ms;
		long wait_ret;

		elapsed_us = rknpu3_get_time_us() - start_time;
		if (elapsed_us >= timeout_us) {
			ret = RKNPU3_ERROR_TIMEOUT;
			break;
		}

		remaining_ms = (timeout_us - elapsed_us) / 1000;
		if (remaining_ms == 0) {
			ret = RKNPU3_ERROR_TIMEOUT;
			break;
		}

		wait_ret = wait_event_timeout(rknpu3_dev->wait_queue,
			atomic_read(&rknpu3_dev->irq_event_seq) != event_seq,
			msecs_to_jiffies(remaining_ms));
		if (wait_ret == 0) {
			ret = RKNPU3_ERROR_TIMEOUT;
			break;
		}

		event_seq = atomic_read(&rknpu3_dev->irq_event_seq);

		spin_lock_irqsave(&rknpu3_dev->dev_lock, flags);
		rknpu3_update_pending_tasks(tasks, task_submit->task_start,
					    task_submit->task_count,
					    &pending_mask, &tasks_pending);
		spin_unlock_irqrestore(&rknpu3_dev->dev_lock, flags);
	}

	/* Post-wait handling */
	if (ret == RKNPU3_ERROR_TIMEOUT && tasks_pending > 0)
		rknpu3_handle_timeout(rknpu3_dev, tasks, task_submit->task_start,
				      task_submit->task_count, pending_mask);
	else if (tasks_pending == 0)
		rknpu3_cleanup_completed_tasks(rknpu3_dev, tasks,
					       task_submit->task_start,
					       task_submit->task_count);

	task_submit->exec_time_us = rknpu3_get_time_us() - task_submit->start_time_us;
	return ret;
}
