/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Zhibin Huang <zhibin.huang@rock-chips.com>
 */

#ifndef __LINUX_ROCKCHIP_PANEL_NOTIFIER_H__
#define __LINUX_ROCKCHIP_PANEL_NOTIFIER_H__

#include <linux/of.h>
#include <linux/notifier.h>

struct rockchip_panel_notifier {
	struct list_head list;
	struct drm_panel *panel;
	struct blocking_notifier_head nh;
};

enum rockchip_panel_event {
	PANEL_PRE_ENABLE,
	PANEL_ENABLED,
	PANEL_PRE_DISABLE,
	PANEL_DISABLED,
};

struct rockchip_panel_edata {
	void *data;
};

#if IS_REACHABLE(CONFIG_ROCKCHIP_PANEL_NOTIFIER)
int devm_rockchip_panel_notifier_register(struct device *dev,
					  struct drm_panel *panel,
					  struct rockchip_panel_notifier *pn);
void devm_rockchip_panel_notifier_unregister(struct device *dev);
int rockchip_panel_notifier_call_chain(struct rockchip_panel_notifier *pn,
				       enum rockchip_panel_event panel_event,
				       struct rockchip_panel_edata *panel_edata);
int devm_rockchip_panel_notifier_register_client(struct device *dev, struct notifier_block *nb);
void devm_rockchip_panel_notifier_unregister_client(struct device *dev);
#else
static inline int
devm_rockchip_panel_notifier_register(struct device *dev,
				      struct drm_panel *panel,
				      struct rockchip_panel_notifier *pn)
{
	return 0;
}

static inline void devm_rockchip_panel_notifier_unregister(struct device *dev)
{
}

static inline int
rockchip_panel_notifier_call_chain(struct rockchip_panel_notifier *pn,
				   enum rockchip_panel_event panel_event,
				   struct rockchip_panel_edata *panel_edata)
{
	return 0;
}

static inline int
devm_rockchip_panel_notifier_register_client(struct device *dev,
					     struct notifier_block *nb)
{
	return 0;
}

static inline void
devm_rockchip_panel_notifier_unregister_client(struct device *dev)
{
}
#endif /* CONFIG_ROCKCHIP_PANEL_NOTIFIER */

#endif /* __LINUX_ROCKCHIP_PANEL_NOTIFIER_H__ */
