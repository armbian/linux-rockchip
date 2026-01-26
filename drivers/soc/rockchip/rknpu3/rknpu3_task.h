/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU Task Management Header
 */

#ifndef __RKNPU3_TASK_H__
#define __RKNPU3_TASK_H__

#include "rknpu3_types.h"

/**
 * rknpu3_task_module_init() - Initialize task management module
 * @rknpu3_dev: RKNPU device
 * @config: Configuration info
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_module_init(struct rknpu3_device *rknpu3_dev,
			   struct rknpu3_config *config);

/**
 * rknpu3_task_module_reset() - Reset task management module
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_module_reset(struct rknpu3_device *dev);

/**
 * rknpu3_task_module_deinit() - Deinitialize task management module
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_module_deinit(struct rknpu3_device *rknpu3_dev);

/**
 * rknpu3_task_get() - Get task by ID
 * @rknpu3_dev: RKNPU device
 * @task_id: Task ID
 * @task: Pointer to task descriptor pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_get(struct rknpu3_device *rknpu3_dev, uint32_t task_id,
		   struct rknpu3_task **task);

/**
 * rknpu3_tasks_submit() - Submit one or more tasks
 * @rknpu3_dev: RKNPU device
 * @task_submit: Task submit descriptor
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_tasks_submit(struct rknpu3_device *rknpu3_dev,
		       struct rknpu3_task_submit *task_submit);

/**
 * rknpu3_tasks_wait() - Wait for one or more tasks to complete
 * @rknpu3_dev: RKNPU device
 * @task_submit: Task submit descriptor
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_tasks_wait(struct rknpu3_device *rknpu3_dev,
		     struct rknpu3_task_submit *task_submit);

/**
 * rknpu3_task_schedule() - Schedule tasks to available cores
 * @rknpu3_dev: RKNPU device
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_task_schedule(struct rknpu3_device *rknpu3_dev);

#endif /* __RKNPU3_TASK_H__ */
