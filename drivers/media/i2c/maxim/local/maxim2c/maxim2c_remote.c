// SPDX-License-Identifier: GPL-2.0
/*
 * Maxim Dual GMSL Deserializer Remode Device Manage
 *
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */
#include <linux/module.h>
#include <linux/of_graph.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#include "maxim2c_api.h"

int maxim2c_remote_devices_power(maxim2c_t *maxim2c, u8 link_mask, int on)
{
	struct device *dev = &maxim2c->client->dev;
	maxim2c_gmsl_link_t *gmsl_link = &maxim2c->gmsl_link;
	struct maxim2c_link_cfg *link_cfg = NULL;
	struct device_node *remote_cam_node = NULL;
	struct i2c_client *remote_cam_client = NULL;
	struct v4l2_subdev *remote_cam_sd = NULL;
	int ret = 0, error = 0, i = 0;

	dev_dbg(dev, "%s: link mask = 0x%02x, on = %d\n", __func__, link_mask, on);

	for (i = 0; i < MAXIM2C_LINK_ID_MAX; i++) {
		if ((link_mask & BIT(i)) == 0) {
			dev_dbg(dev, "link id = %d mask is disabled\n", i);
			continue;
		}

		link_cfg = &gmsl_link->link_cfg[i];
		if (link_cfg->link_enable == 0) {
			dev_info(dev, "link id = %d is disabled\n", i);
			continue;
		}

		remote_cam_node = link_cfg->remote_cam_node;
		if (IS_ERR_OR_NULL(remote_cam_node)) {
			dev_info(dev, "link id = %d remote camera node error\n", i);
			continue;
		}
		remote_cam_client = of_find_i2c_device_by_node(remote_cam_node);
		if (IS_ERR_OR_NULL(remote_cam_client)) {
			dev_info(dev, "link id = %d remote camera client error\n", i);
			continue;
		}
		remote_cam_sd = i2c_get_clientdata(remote_cam_client);
		if (IS_ERR_OR_NULL(remote_cam_sd)) {
			dev_info(dev, "link id = %d remote camera v4l2_subdev error\n", i);
			continue;
		}

		dev_info(dev, "link id = %d remote camera power = %d\n", i, on);
		error = v4l2_subdev_call(remote_cam_sd, core, s_power, on);
		if (error < 0) {
			ret |= error;
			dev_err(dev, "link id = %d remote camera power error = %d\n", i, error);
		}
	}

	return ret;
}
EXPORT_SYMBOL(maxim2c_remote_devices_power);

int maxim2c_remote_devices_s_stream(maxim2c_t *maxim2c, u8 link_mask, int enable)
{
	struct device *dev = &maxim2c->client->dev;
	maxim2c_gmsl_link_t *gmsl_link = &maxim2c->gmsl_link;
	struct maxim2c_link_cfg *link_cfg = NULL;
	struct device_node *remote_cam_node = NULL;
	struct i2c_client *remote_cam_client = NULL;
	struct v4l2_subdev *remote_cam_sd = NULL;
	int ret = 0, error = 0, i = 0;

	dev_dbg(dev, "%s: link mask = 0x%02x, enable = %d\n", __func__, link_mask, enable);

	for (i = 0; i < MAXIM2C_LINK_ID_MAX; i++) {
		if ((link_mask & BIT(i)) == 0) {
			dev_dbg(dev, "link id = %d mask is disabled\n", i);
			continue;
		}

		link_cfg = &gmsl_link->link_cfg[i];
		if (link_cfg->link_enable == 0) {
			dev_info(dev, "link id = %d is disabled\n", i);
			continue;
		}

		remote_cam_node = link_cfg->remote_cam_node;
		if (IS_ERR_OR_NULL(remote_cam_node)) {
			dev_info(dev, "link id = %d remote camera node error\n", i);
			continue;
		}
		remote_cam_client = of_find_i2c_device_by_node(remote_cam_node);
		if (IS_ERR_OR_NULL(remote_cam_client)) {
			dev_info(dev, "link id = %d remote camera client error\n", i);
			continue;
		}
		remote_cam_sd = i2c_get_clientdata(remote_cam_client);
		if (IS_ERR_OR_NULL(remote_cam_sd)) {
			dev_info(dev, "link id = %d remote camera v4l2_subdev error\n", i);
			continue;
		}

		dev_info(dev, "link id = %d remote camera s_stream = %d\n", i, enable);
		error = v4l2_subdev_call(remote_cam_sd, video, s_stream, enable);
		if (error < 0) {
			ret |= error;
			dev_err(dev, "link id = %d remote camera s_stream error = %d\n", i, error);
		}
	}

	return ret;
}
EXPORT_SYMBOL(maxim2c_remote_devices_s_stream);
