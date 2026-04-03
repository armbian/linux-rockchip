/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU Priority Queue Header
 */

#ifndef __RKNPU3_PQUEUE_H__
#define __RKNPU3_PQUEUE_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include "rknpu3_ioctl.h"

/* Pre-allocated node count */
#define RKNPU3_PQUEUE_MAX_NODES 1024

/**
 * struct pq_node - Priority queue node structure
 * @task: Task pointer
 * @next: Next node
 * @in_use: Whether node is in use
 */
struct pq_node {
	struct rknpu3_task *task;
	struct pq_node *next;
	bool in_use;
};

/**
 * struct rknpu3_pqueue - Priority queue structure
 * @head: Queue head
 * @size: Queue size
 * @node_pool: Pre-allocated node pool
 * @pool_size: Node pool size
 * @spinlock: Spinlock for interrupt context
 */
struct rknpu3_pqueue {
	struct pq_node *head;
	uint32_t size;

	/* Pre-allocated node pool */
	struct pq_node node_pool[RKNPU3_PQUEUE_MAX_NODES];
	uint32_t pool_size;

	spinlock_t spinlock;
};

/**
 * rknpu3_pqueue_init() - Initialize priority queue
 * @queue: Queue pointer
 * @name: Queue name
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_init(struct rknpu3_pqueue *queue, const char *name);

/**
 * rknpu3_pqueue_deinit() - Destroy priority queue
 * @queue: Queue pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_deinit(struct rknpu3_pqueue *queue);

/**
 * rknpu3_pqueue_push() - Add task to priority queue
 * @queue: Queue pointer
 * @task: Task pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_push(struct rknpu3_pqueue *queue, struct rknpu3_task *task);

/**
 * rknpu3_pqueue_pop() - Remove highest priority task from queue
 * @queue: Queue pointer
 * @task: Pointer to task pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_pop(struct rknpu3_pqueue *queue, struct rknpu3_task **task);

/**
 * rknpu3_pqueue_find() - Find task by ID
 * @queue: Queue pointer
 * @task_id: Task ID
 * @task: Pointer to task pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_find(struct rknpu3_pqueue *queue, uint32_t task_id,
		      struct rknpu3_task **task);

/**
 * rknpu3_pqueue_remove() - Remove task by ID from queue
 * @queue: Queue pointer
 * @task_id: Task ID
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_remove(struct rknpu3_pqueue *queue, uint32_t task_id);

/**
 * rknpu3_pqueue_is_empty() - Check if queue is empty
 * @queue: Queue pointer
 *
 * Return: true if empty, false otherwise
 */
bool rknpu3_pqueue_is_empty(struct rknpu3_pqueue *queue);

/**
 * rknpu3_pqueue_size() - Get queue size
 * @queue: Queue pointer
 *
 * Return: Queue size
 */
uint32_t rknpu3_pqueue_size(struct rknpu3_pqueue *queue);

/**
 * rknpu3_pqueue_peek() - Peek highest priority task without removing
 * @queue: Queue pointer
 * @task: Pointer to task pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_peek(struct rknpu3_pqueue *queue, struct rknpu3_task **task);

/**
 * rknpu3_pqueue_get_available_nodes() - Get available node count in queue
 * @queue: Queue pointer
 *
 * Return: Available node count
 */
uint32_t rknpu3_pqueue_get_available_nodes(struct rknpu3_pqueue *queue);

/**
 * rknpu3_pqueue_debug_info() - Print queue integrity and statistics
 * @queue: Queue pointer
 * @name: Queue name (for printing)
 */
void rknpu3_pqueue_debug_info(struct rknpu3_pqueue *queue, const char *name);

#endif /* __RKNPU3_PQUEUE_H__ */
