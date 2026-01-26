// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * RKNPU Priority Queue Implementation
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "rknpu3_pqueue.h"

/**
 * rknpu3_pqueue_init() - Initialize priority queue
 * @queue: Queue pointer
 * @name: Queue name
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_init(struct rknpu3_pqueue *queue, const char *name)
{
	uint32_t i;

	if (queue == NULL)
		return -EINVAL;

	queue->head = NULL;
	queue->size = 0;

	/* Initialize node pool */
	for (i = 0; i < RKNPU3_PQUEUE_MAX_NODES; i++) {
		queue->node_pool[i].task = NULL;
		queue->node_pool[i].next = NULL;
		queue->node_pool[i].in_use = false;
	}
	queue->pool_size = RKNPU3_PQUEUE_MAX_NODES;

	/* Initialize lock */
	spin_lock_init(&queue->spinlock);

	return 0;
}

/**
 * rknpu3_pqueue_deinit() - Destroy priority queue
 * @queue: Queue pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_deinit(struct rknpu3_pqueue *queue)
{
	if (queue == NULL)
		return -EINVAL;

	/* Clear linked list and node pool */
	queue->head = NULL;
	queue->size = 0;

	return 0;
}

/**
 * alloc_node_from_pool() - Allocate node from pool
 * @queue: Queue pointer
 *
 * Note: Must hold lock before calling this function
 *
 * Return: Node pointer, or NULL if no available node
 */
static struct pq_node *alloc_node_from_pool(struct rknpu3_pqueue *queue)
{
	uint32_t i;

	for (i = 0; i < queue->pool_size; i++) {
		if (!queue->node_pool[i].in_use) {
			queue->node_pool[i].in_use = true;
			queue->node_pool[i].next = NULL;
			return &queue->node_pool[i];
		}
	}

	return NULL; /* Pool is full */
}

/**
 * free_node_to_pool() - Release node back to pool
 * @queue: Queue pointer
 * @node: Node to release
 *
 * Note: Must hold lock before calling this function
 */
static void free_node_to_pool(struct rknpu3_pqueue *queue, struct pq_node *node)
{
	uint32_t idx;

	if (node == NULL || queue == NULL)
		return;

	/* Check if node belongs to node pool */
	idx = node - queue->node_pool;
	if (idx < queue->pool_size) {
		node->in_use = false;
		node->task = NULL;
		node->next = NULL;
	}
}

/**
 * rknpu3_pqueue_push() - Add task to priority queue
 * @queue: Queue pointer
 * @task: Task pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_push(struct rknpu3_pqueue *queue, struct rknpu3_task *task)
{
	struct pq_node *new_node, *_current, *prev;
	unsigned long flags;

	if (queue == NULL || task == NULL)
		return -EINVAL;

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	/* Check if queue is full */
	if (queue->size >= RKNPU3_PQUEUE_MAX_NODES) {
		/* Queue full, release lock and return error */
		spin_unlock_irqrestore(&queue->spinlock, flags);
		pr_warn("%s: queue is full, size=%d\n", __func__, queue->size);
		return -ENOSPC;
	}

	/* Allocate node */
	new_node = alloc_node_from_pool(queue);
	if (new_node == NULL) {
		/* Release lock and return error */
		spin_unlock_irqrestore(&queue->spinlock, flags);
		pr_err("%s: failed to allocate node from pool\n", __func__);
		return -ENOMEM;
	}

	new_node->task = task;

	/* If queue is empty, insert directly */
	if (queue->head == NULL) {
		queue->head = new_node;
		queue->size++;
	} else {
		/* Insert by priority */
		if (task->priority < queue->head->task->priority) {
			/* If new task has higher priority than head, insert at head */
			new_node->next = queue->head;
			queue->head = new_node;
		} else {
			/* Otherwise, find appropriate position to insert */
			_current = queue->head;
			prev = NULL;

			while (_current != NULL &&
			       task->priority >= _current->task->priority) {
				prev = _current;
				_current = _current->next;
			}

			/* Insert between prev and _current */
			prev->next = new_node;
			new_node->next = _current;
		}
		queue->size++;
	}

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	return 0;
}

/**
 * rknpu3_pqueue_pop() - Remove highest priority task from queue
 * @queue: Queue pointer
 * @task: Pointer to task pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_pop(struct rknpu3_pqueue *queue, struct rknpu3_task **task)
{
	struct pq_node *node;
	unsigned long flags;

	if (queue == NULL || task == NULL)
		return -EINVAL;

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	/* Check if queue is empty */
	if (queue->head == NULL) {
		/* Release lock and return error */
		spin_unlock_irqrestore(&queue->spinlock, flags);
		return -ENOENT;
	}

	/* Remove head node from queue */
	node = queue->head;
	queue->head = node->next;
	queue->size--;

	/* Return task pointer */
	*task = node->task;

	/* Release node back to pool */
	free_node_to_pool(queue, node);

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	return 0;
}

/**
 * rknpu3_pqueue_find() - Find task by ID
 * @queue: Queue pointer
 * @task_id: Task ID
 * @task: Pointer to task pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_find(struct rknpu3_pqueue *queue, uint32_t task_id,
		      struct rknpu3_task **task)
{
	struct pq_node *_current;
	bool found = false;
	unsigned long flags;

	if (queue == NULL || task == NULL)
		return -EINVAL;

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	_current = queue->head;

	while (_current != NULL) {
		if (_current->task->task_id == task_id) {
			*task = _current->task;
			found = true;
			break;
		}
		_current = _current->next;
	}

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	if (!found)
		return -ENOENT;

	return 0;
}

/**
 * rknpu3_pqueue_remove() - Remove task by ID from queue
 * @queue: Queue pointer
 * @task_id: Task ID
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_remove(struct rknpu3_pqueue *queue, uint32_t task_id)
{
	struct pq_node *_current, *prev;
	bool found = false;
	unsigned long flags;

	if (queue == NULL)
		return -EINVAL;

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	/* Check if queue is empty */
	if (queue->head == NULL) {
		/* Release lock and return error */
		spin_unlock_irqrestore(&queue->spinlock, flags);
		return -ENOENT;
	}

	/* If it's the head */
	if (queue->head->task->task_id == task_id) {
		_current = queue->head;
		queue->head = _current->next;
		free_node_to_pool(queue, _current);
		queue->size--;
		found = true;
	} else {
		/* Traverse queue to find task */
		prev = queue->head;
		_current = prev->next;

		while (_current != NULL) {
			if (_current->task->task_id == task_id) {
				prev->next = _current->next;
				free_node_to_pool(queue, _current);
				queue->size--;
				found = true;
				break;
			}
			prev = _current;
			_current = _current->next;
		}
	}

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	if (!found)
		return -ENOENT;

	return 0;
}

/**
 * rknpu3_pqueue_is_empty() - Check if queue is empty
 * @queue: Queue pointer
 *
 * Return: true if empty, false otherwise
 */
bool rknpu3_pqueue_is_empty(struct rknpu3_pqueue *queue)
{
	bool is_empty;
	unsigned long flags;

	if (queue == NULL)
		return true;

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	is_empty = (queue->head == NULL);

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	return is_empty;
}

/**
 * rknpu3_pqueue_size() - Get queue size
 * @queue: Queue pointer
 *
 * Return: Queue size
 */
uint32_t rknpu3_pqueue_size(struct rknpu3_pqueue *queue)
{
	uint32_t size;
	unsigned long flags;

	if (queue == NULL)
		return 0;

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	size = queue->size;

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	return size;
}

/**
 * rknpu3_pqueue_peek() - Peek highest priority task without removing
 * @queue: Queue pointer
 * @task: Pointer to task pointer
 *
 * Return: 0 on success, error code on failure
 */
int rknpu3_pqueue_peek(struct rknpu3_pqueue *queue, struct rknpu3_task **task)
{
	unsigned long flags;

	if (queue == NULL || task == NULL)
		return -EINVAL;

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	/* Check if queue is empty */
	if (queue->head == NULL) {
		/* Release lock and return error */
		spin_unlock_irqrestore(&queue->spinlock, flags);
		return -ENOENT;
	}

	/* Return head node's task without removing */
	*task = queue->head->task;

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	return 0;
}

/**
 * rknpu3_pqueue_get_available_nodes() - Get available node count in queue
 * @queue: Queue pointer
 *
 * Return: Available node count
 */
uint32_t rknpu3_pqueue_get_available_nodes(struct rknpu3_pqueue *queue)
{
	uint32_t available_count = 0;
	uint32_t i;
	unsigned long flags;

	if (queue == NULL)
		return 0;

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	/* Count unused nodes */
	for (i = 0; i < queue->pool_size; i++) {
		if (!queue->node_pool[i].in_use)
			available_count++;
	}

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	return available_count;
}

/**
 * rknpu3_pqueue_debug_info() - Print queue integrity and statistics
 * @queue: Queue pointer
 * @name: Queue name (for printing)
 */
void rknpu3_pqueue_debug_info(struct rknpu3_pqueue *queue, const char *name)
{
	unsigned long flags;
	uint32_t linked_count = 0;
	uint32_t used_pool_count = 0;
	uint32_t available_count = 0;
	struct pq_node *_current;
	uint32_t i;

	if (queue == NULL) {
		pr_info("Queue %s: NULL pointer\n", name ? name : "unknown");
		return;
	}

	/* Acquire lock to protect queue operations */
	spin_lock_irqsave(&queue->spinlock, flags);

	/* Count linked list nodes */
	_current = queue->head;
	while (_current != NULL) {
		linked_count++;
		_current = _current->next;
		/* Prevent infinite loop */
		if (linked_count > RKNPU3_PQUEUE_MAX_NODES) {
			pr_info("Queue %s: Potential infinite loop detected!\n",
				name ? name : "unknown");
			break;
		}
	}

	/* Count node pool usage */
	for (i = 0; i < queue->pool_size; i++) {
		if (queue->node_pool[i].in_use)
			used_pool_count++;
		else
			available_count++;
	}

	/* Release lock */
	spin_unlock_irqrestore(&queue->spinlock, flags);

	/* Print debug info */
	pr_info("Queue %s Debug Info:\n", name ? name : "unknown");
	pr_info("  Queue size: %d\n", queue->size);
	pr_info("  Linked nodes: %d\n", linked_count);
	pr_info("  Pool total: %d\n", queue->pool_size);
	pr_info("  Pool used: %d\n", used_pool_count);
	pr_info("  Pool available: %d\n", available_count);

	/* Check consistency */
	if (queue->size != linked_count) {
		pr_info("  WARNING: Queue size mismatch! (size=%d, linked=%d)\n",
			queue->size, linked_count);
	}

	if (queue->size != used_pool_count) {
		pr_info("  WARNING: Pool usage mismatch! (size=%d, used=%d)\n",
			queue->size, used_pool_count);
	}

	if (used_pool_count + available_count != queue->pool_size) {
		pr_info("  ERROR: Pool count error! (used+available=%d, total=%d)\n",
			used_pool_count + available_count, queue->pool_size);
	}
}
