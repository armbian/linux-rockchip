// SPDX-License-Identifier: GPL-2.0
/*
 * Crypto acceleration support for Rockchip crypto engine
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Lin Jinhan <troy.lin@rock-chips.com>
 *
 */

#define RKCE_MODULE_TAG		"MONITOR"
#define RKCE_MODULE_OFFSET	18

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "rkce_debug.h"
#include "rkce_monitor.h"

#define TIMER_INTERVAL_MS 100

struct rkce_monitor_data {
	void			*td;
	request_cb_func		callback;
	unsigned long		timeout;

	struct list_head	list;
};

static int g_active;
static struct timer_list g_timer;
static struct work_struct g_timeout_work;
static DEFINE_MUTEX(g_monitor_lock);
static LIST_HEAD(g_monitor_list);

static void timeout_work_handler(struct work_struct *work)
{
	struct rkce_monitor_data *monitor_data = NULL;
	struct list_head *pos = NULL, *q = NULL;

	mutex_lock(&g_monitor_lock);

	list_for_each_safe(pos, q, &g_monitor_list) {
		monitor_data = list_entry(pos, struct rkce_monitor_data, list);

		if (monitor_data &&
		    monitor_data->callback &&
		    time_after(jiffies, monitor_data->timeout)) {
			rk_debug("!!!!!!!!!!!!!!!!!! trigger timeout for (%p)\n", monitor_data->td);
			monitor_data->callback(-ETIMEDOUT, 0, monitor_data->td);
			list_del(&monitor_data->list);
		}
	}

	mutex_unlock(&g_monitor_lock);
}

static void timer_callback(struct timer_list *t)
{
	mutex_lock(&g_monitor_lock);

	if (g_active)
		mod_timer(t, jiffies + msecs_to_jiffies(TIMER_INTERVAL_MS));

	mutex_unlock(&g_monitor_lock);

	schedule_work(&g_timeout_work);
}

static void start_timer(void)
{
	rk_trace("enter.\n");

	g_active = 1;

	if (g_active) {
		rk_debug("reload timer.\n");

		mod_timer(&g_timer, jiffies + msecs_to_jiffies(TIMER_INTERVAL_MS));
	}

	rk_trace("exit.\n");
}

static void stop_timer(void)
{
	rk_trace("enter.\n");

	if (g_active) {
		del_timer_sync(&g_timer);
		g_active = 0;
		rk_debug("Timer stopped\n");
	}

	rk_trace("exit.\n");
}

int rkce_monitor_add(void *td, request_cb_func callback)
{
	struct rkce_monitor_data *monitor_data;

	rk_trace("enter.\n");

	if (!td)
		return -EINVAL;

	monitor_data = kzalloc(sizeof(*monitor_data), GFP_KERNEL);
	if (!monitor_data)
		return -ENOMEM;

	monitor_data->td       = td;
	monitor_data->callback = callback;
	monitor_data->timeout  = jiffies + 3 * HZ;

	rk_debug("add %p to monitor, timeout = %u.\n",
		 td, jiffies_to_msecs(monitor_data->timeout));

	mutex_lock(&g_monitor_lock);

	list_add(&monitor_data->list, &g_monitor_list);

	start_timer();

	mutex_unlock(&g_monitor_lock);

	rk_trace("exit.\n");

	return 0;
}

void rkce_monitor_del(void *td)
{
	struct rkce_monitor_data *monitor_data = NULL;
	struct list_head *pos = NULL, *q = NULL;

	rk_trace("enter.\n");

	mutex_lock(&g_monitor_lock);

	list_for_each_safe(pos, q, &g_monitor_list) {
		monitor_data = list_entry(pos, struct rkce_monitor_data, list);
		if (monitor_data->td == td) {
			list_del(&monitor_data->list);
			kfree(monitor_data);
			break;
		}
	}

	if (list_empty(&g_monitor_list))
		stop_timer();

	mutex_unlock(&g_monitor_lock);

	rk_trace("exit.\n");
}

int rkce_monitor_init(void)
{
	rk_debug("Initializing timer module\n");

	INIT_WORK(&g_timeout_work, timeout_work_handler);

	timer_setup(&g_timer, timer_callback, 0);

	return 0;
}

void rkce_monitor_deinit(void)
{
	struct rkce_monitor_data *monitor_data = NULL;
	struct list_head *pos = NULL, *q = NULL;

	rk_debug("Exiting timer module\n");

	mutex_lock(&g_monitor_lock);

	list_for_each_safe(pos, q, &g_monitor_list) {
		monitor_data = list_entry(pos, struct rkce_monitor_data, list);
		list_del(&monitor_data->list);
		kfree(monitor_data);
	}

	stop_timer();

	mutex_unlock(&g_monitor_lock);
}
