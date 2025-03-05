// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip DP MST simulation axu client
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Zhang Yubing <yubing.zhang@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <drm/drm_edid.h>
#include <drm/drm_modes.h>
#include <drm/display/drm_dp_helper.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include "rockchip_dp_mst_aux_client_helper.h"
#include "rockchip_dp_mst_aux_client.h"

static DEFINE_MUTEX(aux_client_lock);
static LIST_HEAD(aux_client_list);

#define to_rockchip_dp_aux_client(x) container_of((x), struct rockchip_dp_aux_client, bridge)

static const struct rockchip_dp_mst_sim_port output_port = {
	false, false, true, 3, false, 0x12,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	0, 0, 2520, 2520, NULL, 0
};

struct rockchip_dp_aux_client *rockchip_dp_get_aux_client(struct device_node *node,
							  char *handle_name)
{
	struct rockchip_dp_aux_client *aux_client;
	struct device_node *dev_node;

	dev_node = of_parse_phandle(node, handle_name, 0);
	if (!dev_node)
		return NULL;

	mutex_lock(&aux_client_lock);
	list_for_each_entry(aux_client, &aux_client_list, head)
		if (aux_client->dev->of_node == dev_node)
			goto out;
	aux_client = ERR_PTR(-EPROBE_DEFER);

out:
	mutex_unlock(&aux_client_lock);
	of_node_put(dev_node);

	return aux_client;
}
EXPORT_SYMBOL(rockchip_dp_get_aux_client);

static void rockchip_register_dp_aux_client(struct rockchip_dp_aux_client *aux_client)
{

	mutex_lock(&aux_client_lock);
	list_add(&aux_client->head, &aux_client_list);
	mutex_unlock(&aux_client_lock);
}

static void rockchip_unregister_dp_aux_client(struct rockchip_dp_aux_client *aux_client)
{

	mutex_lock(&aux_client_lock);
	list_del(&aux_client->head);
	mutex_unlock(&aux_client_lock);
}

static int rockchip_dp_aux_client_register_hpd_irq(struct rockchip_dp_aux_client *aux_client,
	void (*hpd_irq_cb)(void *), void *dev)
{
	aux_client->host_dev = dev;
	aux_client->hpd_irq_cb = hpd_irq_cb;

	return 0;
}

static int rockchip_dp_aux_client_register_transfer(struct rockchip_dp_aux_client *aux_client,
	ssize_t (*transfer_cb)(struct drm_dp_aux *, struct drm_dp_aux_msg *))
{
	aux_client->transfer_cb = transfer_cb;

	return 0;
}

static ssize_t rockchip_dp_sim_transfer(struct rockchip_dp_aux_client *aux_client,
	struct drm_dp_aux *drm_aux,
	struct drm_dp_aux_msg *msg)
{
	int ret;

	ret = rockchip_dp_mst_sim_transfer(aux_client->mst_ctx, msg);
	if (ret < 0) {
		if (aux_client->transfer_cb)
			ret = aux_client->transfer_cb(drm_aux, msg);
		else
			ret = -EIO;
	} else {
		ret = msg->size;
	}

	return ret;
}

static void rockchip_dp_sim_host_hpd_irq(void *host_dev)
{
	struct rockchip_dp_aux_client *aux_client = host_dev;

	if (aux_client->hpd_irq_cb)
		aux_client->hpd_irq_cb(aux_client->host_dev);
}

static void rockchip_dp_sim_update_dtd(struct edid *edid,
				       struct drm_display_mode *mode)
{
	struct detailed_timing *dtd = &edid->detailed_timings[0];
	struct detailed_pixel_timing *pd = &dtd->data.pixel_data;
	u32 h_blank = mode->htotal - mode->hdisplay;
	u32 v_blank = mode->vtotal - mode->vdisplay;
	u32 h_img = 0, v_img = 0;

	dtd->pixel_clock = cpu_to_le16(mode->clock / 10);

	pd->hactive_lo = mode->hdisplay & 0xFF;
	pd->hblank_lo = h_blank & 0xFF;
	pd->hactive_hblank_hi = ((h_blank >> 8) & 0xF) |
			((mode->hdisplay >> 8) & 0xF) << 4;

	pd->vactive_lo = mode->vdisplay & 0xFF;
	pd->vblank_lo = v_blank & 0xFF;
	pd->vactive_vblank_hi = ((v_blank >> 8) & 0xF) |
			((mode->vdisplay >> 8) & 0xF) << 4;

	pd->hsync_offset_lo =
		(mode->hsync_start - mode->hdisplay) & 0xFF;
	pd->hsync_pulse_width_lo =
		(mode->hsync_end - mode->hsync_start) & 0xFF;
	pd->vsync_offset_pulse_width_lo =
		(((mode->vsync_start - mode->vdisplay) & 0xF) << 4) |
		((mode->vsync_end - mode->vsync_start) & 0xF);

	pd->hsync_vsync_offset_pulse_width_hi =
		((((mode->hsync_start - mode->hdisplay) >> 8) & 0x3) << 6) |
		((((mode->hsync_end - mode->hsync_start) >> 8) & 0x3) << 4) |
		((((mode->vsync_start - mode->vdisplay) >> 4) & 0x3) << 2) |
		((((mode->vsync_end - mode->vsync_start) >> 4) & 0x3) << 0);

	pd->width_mm_lo = h_img & 0xFF;
	pd->height_mm_lo = v_img & 0xFF;
	pd->width_height_mm_hi = (((h_img >> 8) & 0xF) << 4) |
		((v_img >> 8) & 0xF);

	pd->hborder = 0;
	pd->vborder = 0;
	pd->misc = 0;
}

static void rockchip_dp_sim_update_checksum(struct edid *edid)
{
	u8 *data = (u8 *)edid;
	u32 i, sum = 0;

	for (i = 0; i < EDID_LENGTH - 1; i++)
		sum += data[i];

	edid->checksum = 0x100 - (sum & 0xFF);
}

static int rockchip_dp_aux_client_parse(struct rockchip_dp_aux_client *aux_client)
{
	struct device_node *of_node = aux_client->dev->of_node;
	struct device_node *node;
	struct rockchip_dp_mst_sim_port *ports;
	struct drm_display_mode mode_buf, *mode = &mode_buf;
	int rc, port_num, i;
	struct edid *edid;

	const u8 edid_buf[EDID_LENGTH] = {
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x44, 0x6D,
		0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1B, 0x10, 0x01, 0x03,
		0x80, 0x50, 0x2D, 0x78, 0x0A, 0x0D, 0xC9, 0xA0, 0x57, 0x47,
		0x98, 0x27, 0x12, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01,
	};

	port_num = of_get_child_count(of_node);

	if (!port_num)
		return 0;

	if (port_num >= 15)
		return -EINVAL;

	ports = kcalloc(port_num, sizeof(*ports), GFP_KERNEL);
	if (!ports)
		return -ENOMEM;

	i = 0;
	for_each_child_of_node(of_node, node) {
		struct display_timing dt;
		struct videomode vm;

		edid = kzalloc(sizeof(*edid), GFP_KERNEL);
		if (!edid) {
			rc = -ENOMEM;
			goto fail;
		}
		memcpy(edid, edid_buf, sizeof(edid_buf));

		if (!of_get_display_timing(node, "display-timings", &dt)) {
			videomode_from_timing(&dt, &vm);
			drm_display_mode_from_videomode(&vm, mode);
			rockchip_dp_sim_update_dtd(edid, mode);
		}

		rockchip_dp_sim_update_checksum(edid);
		memcpy(&ports[i], &output_port, sizeof(*ports));
		ports[i].peer_guid[0] = i;
		ports[i].edid = (u8 *)edid;
		ports[i].edid_size = sizeof(*edid);
		i++;
	}

	rc = rockchip_dp_mst_sim_update(aux_client->mst_ctx, port_num, ports);

fail:
	for (i = 0; i < port_num; i++)
		kfree(ports[i].edid);
	kfree(ports);

	return rc;
}

static int rockchip_dp_aux_client_probe(struct platform_device *pdev)
{
	struct rockchip_dp_aux_client *aux_client;
	struct rockchip_dp_mst_sim_cfg cfg;
	int ret;

	aux_client = devm_kzalloc(&pdev->dev, sizeof(*aux_client), GFP_KERNEL);
	if (!aux_client)
		return -ENOMEM;

	aux_client->dev = &pdev->dev;
	aux_client->register_hpd_irq = rockchip_dp_aux_client_register_hpd_irq;
	aux_client->register_transfer = rockchip_dp_aux_client_register_transfer;
	aux_client->transfer = rockchip_dp_sim_transfer;

	memset(&cfg, 0, sizeof(cfg));
	cfg.host_dev = aux_client;
	cfg.host_hpd_irq = rockchip_dp_sim_host_hpd_irq;
	cfg.guid[0] = 0xff;

	ret = rockchip_dp_mst_sim_create(&cfg, &aux_client->mst_ctx);
	if (ret)
		return ret;

	ret = rockchip_dp_aux_client_parse(aux_client);
	if (ret)
		goto fail;

	platform_set_drvdata(pdev, aux_client);

	rockchip_register_dp_aux_client(aux_client);

	return 0;

fail:
	rockchip_dp_mst_sim_destroy(aux_client->mst_ctx);
	return ret;
}

static int rockchip_dp_aux_client_remove(struct platform_device *pdev)
{
	struct rockchip_dp_aux_client *aux_client;

	aux_client = platform_get_drvdata(pdev);
	if (!aux_client)
		return 0;

	rockchip_unregister_dp_aux_client(aux_client);
	rockchip_dp_mst_sim_destroy(aux_client->mst_ctx);

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "rockchip,dp-mst-sim" },
	{},
};

static struct platform_driver rockchip_dp_aux_client_driver = {
	.probe = rockchip_dp_aux_client_probe,
	.remove = rockchip_dp_aux_client_remove,
	.driver = {
		.name = "rockchip_dp_aux_client",
		.of_match_table = dt_match,
	},
};

static int __init rockchip_dp_aux_client_register(void)
{
	return platform_driver_register(&rockchip_dp_aux_client_driver);
}

static void __exit rockchip_dp_aux_client_unregister(void)
{
	platform_driver_unregister(&rockchip_dp_aux_client_driver);
}

module_init(rockchip_dp_aux_client_register);
module_exit(rockchip_dp_aux_client_unregister);

MODULE_AUTHOR("Zhang Yubing <yubing.zhang@rock-chips.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rockchip DP Simulate Aux Client Driver");
