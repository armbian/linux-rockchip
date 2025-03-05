// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Zhibin Huang <zhibin.huang@rock-chips.com>
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/rockchip-panel-notifier.h>

#include <drm/drm_panel.h>

struct rockchip_panel_notifier_client {
	struct list_head list;
	struct rockchip_panel_notifier *pn;
	struct notifier_block *nb;
};

static DEFINE_MUTEX(notifier_list_lock);
static LIST_HEAD(notifier_list);
static DEFINE_MUTEX(notifier_client_list_lock);
static LIST_HEAD(notifier_client_list);

static int
rockchip_panel_notifier_register_client(struct device *dev,
					struct rockchip_panel_notifier_client **client)
{
	struct rockchip_panel_notifier_client *this = *client;
	struct rockchip_panel_notifier *pn;
	struct device_node *node;
	struct drm_panel *panel;
	int ret;

	if (!dev || !dev->of_node || !this->nb)
		return -EINVAL;

	node = of_parse_phandle(dev->of_node, "rockchip,panel-notifier", 0);
	if (!node)
		return -ENODEV;

	panel = of_drm_find_panel(node);
	if (IS_ERR(panel)) {
		of_node_put(node);
		return PTR_ERR(panel);
	}

	of_node_put(node);

	mutex_lock(&notifier_list_lock);
	list_for_each_entry(pn, &notifier_list, list) {
		if (pn->panel == panel)
			goto find;
	}
	pn = NULL;
find:
	mutex_unlock(&notifier_list_lock);
	if (!pn)
		return -ENODEV;

	ret = blocking_notifier_chain_register(&pn->nh, this->nb);
	if (ret)
		return ret;

	this->pn = pn;

	mutex_lock(&notifier_client_list_lock);
	list_add_tail(&this->list, &notifier_client_list);
	mutex_unlock(&notifier_client_list_lock);

	return 0;
}

static void
rockchip_panel_notifier_unregister_client(struct rockchip_panel_notifier_client *client)
{
	if (!client)
		return;

	blocking_notifier_chain_unregister(&client->pn->nh, client->nb);
}

static void
devm_rockchip_panel_notifier_unreg_client(struct device *dev, void *res)
{
	struct rockchip_panel_notifier_client *pn_client, *next, *client = res;

	mutex_lock(&notifier_client_list_lock);
	list_for_each_entry_safe(pn_client, next, &notifier_client_list, list) {
		if (pn_client != client)
			continue;

		rockchip_panel_notifier_unregister_client(pn_client);
		list_del_init(&pn_client->list);
	}
	mutex_unlock(&notifier_client_list_lock);
}

int devm_rockchip_panel_notifier_register_client(struct device *dev,
						 struct notifier_block *nb)
{
	int ret;
	struct rockchip_panel_notifier_client *pn_client;

	if (!dev || !nb)
		return -EINVAL;

	pn_client = devres_alloc(devm_rockchip_panel_notifier_unreg_client,
				 sizeof(*pn_client), GFP_KERNEL);
	if (!pn_client)
		return -ENOMEM;

	pn_client->nb = nb;

	ret = rockchip_panel_notifier_register_client(dev, &pn_client);
	if (ret) {
		devres_free(pn_client);
		return ret;
	}

	devres_add(dev, pn_client);

	return 0;
}
EXPORT_SYMBOL(devm_rockchip_panel_notifier_register_client);

void devm_rockchip_panel_notifier_unregister_client(struct device *dev)
{
	WARN_ON(devres_release(dev, devm_rockchip_panel_notifier_unreg_client,
			       NULL, NULL));
}
EXPORT_SYMBOL(devm_rockchip_panel_notifier_unregister_client);

static int rockchip_panel_notifier_register(struct drm_panel *panel,
					    struct rockchip_panel_notifier *pn)
{
	struct rockchip_panel_notifier *panel_notifier, *next;
	int ret = 0;

	if (!panel || !pn)
		return -EINVAL;

	mutex_lock(&notifier_list_lock);
	list_for_each_entry_safe(panel_notifier, next, &notifier_list, list) {
		if (panel_notifier->panel != panel)
			continue;

		ret = -EEXIST;
	}
	if (!ret) {
		pn->panel = panel;
		BLOCKING_INIT_NOTIFIER_HEAD(&pn->nh);

		list_add_tail(&pn->list, &notifier_list);
	}
	mutex_unlock(&notifier_list_lock);

	return ret;
}

static void rockchip_panel_notifier_unregister(struct rockchip_panel_notifier *pn)
{
	struct rockchip_panel_notifier_client *pn_client, *next;

	if (!pn)
		return;

	mutex_lock(&notifier_client_list_lock);
	list_for_each_entry_safe(pn_client, next, &notifier_client_list, list) {
		if (pn_client->pn != pn)
			continue;

		rockchip_panel_notifier_unregister_client(pn_client);
		list_del_init(&pn_client->list);
	}
	mutex_unlock(&notifier_client_list_lock);

	mutex_lock(&notifier_list_lock);
	list_del_init(&pn->list);
	mutex_unlock(&notifier_list_lock);
}

static void devm_rockchip_panel_notifier_unreg(struct device *dev, void *res)
{
	rockchip_panel_notifier_unregister(*(struct rockchip_panel_notifier **)res);
}

void devm_rockchip_panel_notifier_unregister(struct device *dev)
{
	WARN_ON(devres_release(dev, devm_rockchip_panel_notifier_unreg,
			       NULL, NULL));
}
EXPORT_SYMBOL(devm_rockchip_panel_notifier_unregister);

int devm_rockchip_panel_notifier_register(struct device *dev,
					  struct drm_panel *panel,
					  struct rockchip_panel_notifier *pn)
{
	int ret;
	struct rockchip_panel_notifier **ptr;

	ptr = devres_alloc(devm_rockchip_panel_notifier_unreg, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = rockchip_panel_notifier_register(panel, pn);
	if (ret) {
		devres_free(ptr);
		return ret;
	}

	*ptr = pn;
	devres_add(dev, ptr);

	return 0;
}
EXPORT_SYMBOL(devm_rockchip_panel_notifier_register);

int rockchip_panel_notifier_call_chain(struct rockchip_panel_notifier *pn,
				       enum rockchip_panel_event panel_event,
				       struct rockchip_panel_edata *panel_edata)
{
	if (!pn)
		return -EINVAL;

	return blocking_notifier_call_chain(&pn->nh, (unsigned long)panel_event,
					    (void *)panel_edata);
}
EXPORT_SYMBOL(rockchip_panel_notifier_call_chain);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhibin Huang <zhibin.huang@rock-chips.com>");
MODULE_DESCRIPTION("rockchip panel notifier");
