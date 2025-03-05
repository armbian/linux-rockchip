/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 * Author: Zhang Yubing <yubing.zhang@rock-chips.com>
 */

#ifndef _ROCKCHIP_DP_MST_AUX_CLIENT_H_
#define _ROCKCHIP_DP_MST_AUX_CLIENT_H_

#include <linux/types.h>
#include <drm/display/drm_dp_helper.h>

struct rockchip_dp_aux_client {
	struct device *dev;
	struct list_head head;
	void *mst_ctx;
	void *host_dev;
	void (*hpd_irq_cb)(void *host_dev);
	ssize_t (*transfer_cb)(struct drm_dp_aux *drm_aux,
			       struct drm_dp_aux_msg *msg);
	int (*register_hpd_irq)(struct rockchip_dp_aux_client *aux_client,
				void (*hpd_irq_cb)(void *), void *dev);
	int (*register_transfer)(struct rockchip_dp_aux_client *aux_client,
				 ssize_t (*transfer)(struct drm_dp_aux *, struct drm_dp_aux_msg *));
	ssize_t (*transfer)(struct rockchip_dp_aux_client *aux_client,
			struct drm_dp_aux *drm_aux,
			struct drm_dp_aux_msg *msg);
};

#if IS_REACHABLE(CONFIG_ROCKCHIP_DP_MST_AUX_CLIENT)
struct rockchip_dp_aux_client *rockchip_dp_get_aux_client(struct device_node *node,
							  char *handle_name);
#else
static inline struct rockchip_dp_aux_client *rockchip_dp_get_aux_client(struct device_node *node,
									char *handle_name)
{
	return NULL;
}
#endif

#endif
