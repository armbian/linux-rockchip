// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 * Author: Damon Ding <damon.ding@rock-chips.com>
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/rockchip-panel-notifier.h>
#include <linux/spi/spi.h>

#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>

#include "rockchip_drm_drv.h"

#define MAX_DIMMING_PANELS	8

static DECLARE_BITMAP(allocated_dimming_panels, MAX_DIMMING_PANELS);

static struct class *rockchip_dimming_class;

struct rockchip_dimming_panel {
	struct device *dev;
	struct device *dimming_dev;
	struct drm_crtc *crtc;
	struct drm_panel base;
	uint8_t id;

	const struct drm_display_mode *modes;
	uint32_t num_modes;

	/** @delay: Structure containing various delay values for this panel. */
	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @hpd_absent_delay: Add this to the prepare delay if we know Hot
	 *                    Plug Detect isn't used.
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 * @reset: the time (in milliseconds) that it takes for the panel
	 *         to reset itself completely
	 * @init: the time (in milliseconds) that it takes for the panel to
	 *        send init command sequence after reset deassert
	 * @vsync_hold: the time (in microseconds) that it takes for the panel to
	 *              hold the vsync signal high
	 * @vysnc_back: the time (in microseconds) that it takes for the panel to
	 *              delay the vsync signal
	 */
	struct {
		uint32_t prepare;
		uint32_t enable;
		uint32_t disable;
		uint32_t unprepare;
		uint32_t reset;
		uint32_t init;
		uint32_t vsync_hold;
		uint32_t vsync_back;
	} delay;

	struct regulator *supply;

	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;

	struct gpio_desc *lden_gpio;
	struct gpio_desc *blen_gpio;
	struct gpio_desc *sync_gpio;
	struct gpio_desc *dbcl_gpio;

	struct kthread_worker *dimming_worker;
	struct kthread_delayed_work dimming_delayed_work;

	struct rockchip_panel_notifier panel_notifier;

	struct rockchip_drm_sub_dev sub_dev;

	/** @bus_format: See MEDIA_BUS_FMT_... defines. */
	uint32_t bus_format;

	/** @bus_flags: See DRM_BUS_FLAG_... defines. */
	uint32_t bus_flags;

	void *data;

	bool enabled;
	bool prepared;

	uint32_t checksum;
	uint32_t hzone_num;
	uint32_t vzone_num;
	uint32_t zone_max;
	uint32_t brightness_max;
	uint32_t brightness_min;
	uint32_t brightness_bpc;

	uint32_t cmd_element_size;
	uint32_t cmd_header_len;
	uint32_t cmd_tail_len;
	uint8_t *cmd_header;
	uint8_t *cmd_tail;
};

static ssize_t crtc_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	return sprintf(buf, "%d\n", dimming_panel->crtc ? dimming_panel->crtc->base.id : 0);
}

static ssize_t checksum_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	return sprintf(buf, "%d\n", dimming_panel->checksum);
}

static ssize_t hzone_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	return sprintf(buf, "%d\n", dimming_panel->hzone_num);
}

static ssize_t vzone_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	return sprintf(buf, "%d\n", dimming_panel->vzone_num);
}

static ssize_t zone_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	return sprintf(buf, "%d\n", dimming_panel->zone_max);
}

static ssize_t brightness_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	return sprintf(buf, "%d\n", dimming_panel->brightness_max);
}

static ssize_t brightness_min_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	return sprintf(buf, "%d\n", dimming_panel->brightness_min);
}

static ssize_t brightness_bpc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spi_device *spi = dev_get_drvdata(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	return sprintf(buf, "%d\n", dimming_panel->brightness_bpc);
}

static DEVICE_ATTR_RO(crtc_id);
static DEVICE_ATTR_RO(checksum);
static DEVICE_ATTR_RO(hzone_num);
static DEVICE_ATTR_RO(vzone_num);
static DEVICE_ATTR_RO(zone_max);
static DEVICE_ATTR_RO(brightness_max);
static DEVICE_ATTR_RO(brightness_min);
static DEVICE_ATTR_RO(brightness_bpc);
/*
 * The above attributes can be read via the following paths:
 *   (X means the index of dimming panel device)
 *   /sys/class/dimming/dimming_X/crtc_id
 *   /sys/class/dimming/dimming_X/checksum
 *   /sys/class/dimming/dimming_X/hzone_num
 *   /sys/class/dimming/dimming_X/vzone_num
 *   /sys/class/dimming/dimming_X/zone_max
 *   /sys/class/dimming/dimming_X/brightness_max
 *   /sys/class/dimming/dimming_X/brightness_min
 *   /sys/class/dimming/dimming_X/brightness_bpc
 */

static struct attribute *rockchip_dimming_attrs[] = {
	&dev_attr_crtc_id.attr,
	&dev_attr_checksum.attr,
	&dev_attr_hzone_num.attr,
	&dev_attr_vzone_num.attr,
	&dev_attr_zone_max.attr,
	&dev_attr_brightness_max.attr,
	&dev_attr_brightness_min.attr,
	&dev_attr_brightness_bpc.attr,
	NULL,
};

static struct attribute_group rockchip_dimming_attr_group = {
	.attrs = rockchip_dimming_attrs,
};

static inline void rockchip_dimming_panel_msleep(uint32_t msecs)
{
	usleep_range(msecs * 1000, msecs * 1000 + 100);
}

static inline struct rockchip_dimming_panel *to_rockchip_dimming_panel(struct drm_panel *panel)
{
	return container_of(panel, struct rockchip_dimming_panel, base);
}

static bool of_child_node_is_present(const struct device_node *node, const char *name)
{
	struct device_node *child;

	child = of_get_child_by_name(node, name);
	of_node_put(child);

	return !!child;
}

static int rockchip_dimming_panel_spi_write_data(struct device *dev, const void *data)
{
	struct spi_device *spi = to_spi_device(dev);
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);
	struct spi_transfer t;
	struct spi_message m;
	uint32_t offset = 0;
	uint32_t element_bytes = dimming_panel->brightness_bpc / 8;
	uint16_t checksum = 0;
	uint8_t *txbuf = NULL;
	int len;
	int i = 0, ret;

	t.len = dimming_panel->cmd_element_size * element_bytes;
	t.bits_per_word = dimming_panel->brightness_bpc;

	txbuf = kzalloc(t.len * element_bytes, GFP_KERNEL);
	if (!txbuf)
		return -ENOMEM;

	/* set sequence header */
	memcpy(txbuf, dimming_panel->cmd_header, dimming_panel->cmd_header_len);

	/* set brightness data */
	offset += dimming_panel->cmd_header_len;
	memcpy(txbuf + offset, data, dimming_panel->zone_max * element_bytes);

	/* calculate magic code */
	len = dimming_panel->cmd_header_len + dimming_panel->zone_max * element_bytes;
	for (i = 0; i < len; i++) {
		if (dimming_panel->brightness_bpc == 16) {
			checksum ^= (uint16_t)(txbuf[i] << 8 | txbuf[i + 1]);
			i++;
		} else {
			checksum ^= txbuf[i];
		}
	}

	/* set magic code */
	offset += dimming_panel->zone_max * element_bytes;
	memcpy(txbuf + offset, &checksum, element_bytes);
	dimming_panel->checksum = checksum;

	/* set sequence tail */
	offset += element_bytes;
	memcpy(txbuf + offset, dimming_panel->cmd_tail,
	       dimming_panel->cmd_tail_len - element_bytes);

	t.tx_buf = txbuf;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ret = spi_sync(spi, &m);

	kfree(txbuf);

	return ret;
}

static void dimming_delayed_work_func(struct kthread_work *work)
{
	struct rockchip_dimming_panel *dimming_panel =
		container_of(work, struct rockchip_dimming_panel, dimming_delayed_work.work);
	struct device *dev = dimming_panel->base.dev;
	struct drm_crtc *crtc = dimming_panel->crtc;
	struct rockchip_crtc_state *vcstate = to_rockchip_crtc_state(crtc->state);
	const struct drm_display_mode *mode = &dimming_panel->modes[0];
	struct drm_vblank_crtc *vblank = &crtc->dev->vblank[drm_crtc_index(crtc)];
	unsigned int delay_ms = 0;
	uint32_t timeout = DIV_ROUND_CLOSEST_ULL(1000, drm_mode_vrefresh(mode));
	uint32_t element_bytes = dimming_panel->brightness_bpc / 8;
	uint64_t last;
	int pipe = 0;
	int ret = 0;

	if (!vcstate->dimming_changed || !vcstate->dimming_data || !vcstate->dimming_data->data) {
		dev_dbg(dev, "dimming data may be unprepared\n");
		delay_ms = timeout;
		goto out;
	}
	vcstate->dimming_changed = false;

	memcpy(dimming_panel->data, vcstate->dimming_data->data,
	       dimming_panel->zone_max * element_bytes);

	ret = drm_crtc_vblank_get(crtc);
	if (ret) {
		dev_err(dev, "failed to get vblank on crtc-%d\n", pipe);
		delay_ms = timeout;
		goto out;
	}
	delay_ms = 0;

	last = drm_crtc_vblank_count(crtc);
	ret = wait_event_timeout(vblank->queue, last != drm_crtc_vblank_count(crtc),
				 msecs_to_jiffies(timeout));
	drm_crtc_vblank_put(crtc);
	if (!ret) {
		dev_err(dev, "failed to wait for vblank on crtc-%d\n", pipe);
		goto out;
	}

	gpiod_direction_output(dimming_panel->sync_gpio, 1);
	udelay(dimming_panel->delay.vsync_hold);
	gpiod_direction_output(dimming_panel->sync_gpio, 0);
	udelay(dimming_panel->delay.vsync_back);

	ret = rockchip_dimming_panel_spi_write_data(dimming_panel->base.dev,
						    dimming_panel->data);
	if (ret)
		dev_err(dev, "failed to write dimming data on crtc-%d\n", pipe);

out:
	kthread_queue_delayed_work(dimming_panel->dimming_worker,
				   &dimming_panel->dimming_delayed_work,
				   msecs_to_jiffies(delay_ms));
};

static int rockchip_dimming_panel_regulator_enable(struct rockchip_dimming_panel *dimming_panel)
{
	int ret;

	ret = regulator_enable(dimming_panel->supply);
	if (ret < 0)
		return ret;

	return 0;
}

static int rockchip_dimming_panel_regulator_disable(struct rockchip_dimming_panel *dimming_panel)
{
	regulator_disable(dimming_panel->supply);

	return 0;
}

static int rockchip_dimming_panel_prepare(struct drm_panel *panel)
{
	struct rockchip_dimming_panel *dimming_panel = to_rockchip_dimming_panel(panel);
	int ret;

	if (dimming_panel->prepared)
		return 0;

	ret = rockchip_dimming_panel_regulator_enable(dimming_panel);
	if (ret < 0) {
		dev_err(panel->dev, "failed to enable regulator: %d\n", ret);
		return ret;
	}

	gpiod_direction_output(dimming_panel->lden_gpio, 0);
	gpiod_direction_output(dimming_panel->blen_gpio, 1);
	gpiod_direction_output(dimming_panel->dbcl_gpio, 1);
	gpiod_direction_output(dimming_panel->sync_gpio, 0);

	gpiod_direction_output(dimming_panel->enable_gpio, 1);

	if (dimming_panel->delay.prepare)
		rockchip_dimming_panel_msleep(dimming_panel->delay.prepare);

	gpiod_direction_output(dimming_panel->reset_gpio, 1);

	if (dimming_panel->delay.reset)
		rockchip_dimming_panel_msleep(dimming_panel->delay.reset);

	gpiod_direction_output(dimming_panel->reset_gpio, 0);

	if (dimming_panel->delay.init)
		rockchip_dimming_panel_msleep(dimming_panel->delay.init);

	dimming_panel->prepared = true;

	return 0;
}

static int rockchip_dimming_panel_enable(struct drm_panel *panel)
{
	struct rockchip_dimming_panel *dimming_panel = to_rockchip_dimming_panel(panel);

	if (dimming_panel->enabled)
		return 0;

	if (dimming_panel->delay.enable)
		rockchip_dimming_panel_msleep(dimming_panel->delay.enable);

	gpiod_direction_output(dimming_panel->lden_gpio, 1);
	usleep_range(10 * 1000, 10 * 1000 + 500);

	kthread_queue_delayed_work(dimming_panel->dimming_worker,
				   &dimming_panel->dimming_delayed_work, 0);

	dimming_panel->enabled = true;

	/*
	 * notify other devices (such as TP) to perform the action after the
	 * panel is enabled.
	 */
	rockchip_panel_notifier_call_chain(&dimming_panel->panel_notifier,
					   PANEL_ENABLED, NULL);

	return 0;
}

static int rockchip_dimming_panel_disable(struct drm_panel *panel)
{
	struct rockchip_dimming_panel *dimming_panel = to_rockchip_dimming_panel(panel);

	/*
	 * notify other devices (such as TP) to perform the action before the
	 * panel is disabled.
	 */
	rockchip_panel_notifier_call_chain(&dimming_panel->panel_notifier,
					   PANEL_PRE_DISABLE, NULL);

	if (!dimming_panel->enabled)
		return 0;

	if (dimming_panel->delay.disable)
		rockchip_dimming_panel_msleep(dimming_panel->delay.disable);

	kthread_cancel_delayed_work_sync(&dimming_panel->dimming_delayed_work);

	gpiod_direction_output(dimming_panel->lden_gpio, 0);

	dimming_panel->enabled = false;

	return 0;
}

static int rockchip_dimming_panel_unprepare(struct drm_panel *panel)
{
	struct rockchip_dimming_panel *dimming_panel = to_rockchip_dimming_panel(panel);

	/* Unpreparing when already unprepared is a no-op */
	if (!dimming_panel->prepared)
		return 0;

	gpiod_direction_output(dimming_panel->reset_gpio, 1);
	gpiod_direction_output(dimming_panel->enable_gpio, 0);

	rockchip_dimming_panel_regulator_disable(dimming_panel);

	if (dimming_panel->delay.unprepare)
		rockchip_dimming_panel_msleep(dimming_panel->delay.unprepare);

	dimming_panel->prepared = false;

	return 0;
}

static int rockchip_dimming_panel_find_possible_crtc(struct drm_panel *panel,
						     struct drm_connector *connector)
{
	struct rockchip_dimming_panel *dimming_panel = to_rockchip_dimming_panel(panel);
	struct drm_encoder *encoder;
	struct drm_crtc *crtc;

	drm_connector_for_each_possible_encoder(connector, encoder)
		break;

	if (!encoder)
		return -EINVAL;

	drm_for_each_crtc(crtc, encoder->dev) {
		if (crtc->index == ffs(encoder->possible_crtcs) - 1) {
			dimming_panel->crtc = crtc;
			return 0;
		}
	}

	return -EINVAL;
}

static int rockchip_dimming_panel_get_modes(struct drm_panel *panel,
					    struct drm_connector *connector)
{
	struct rockchip_dimming_panel *dimming_panel = to_rockchip_dimming_panel(panel);
	struct drm_display_mode *mode;
	uint32_t i, num = 0;

	if (rockchip_dimming_panel_find_possible_crtc(panel, connector))
		return 0;

	for (i = 0; i < dimming_panel->num_modes; i++) {
		const struct drm_display_mode *m = &dimming_panel->modes[i];

		mode = drm_mode_duplicate(connector->dev, m);
		if (!mode) {
			dev_err(dimming_panel->base.dev, "failed to add mode %ux%u@%u\n",
				m->hdisplay, m->vdisplay,
				drm_mode_vrefresh(m));
			continue;
		}

		mode->type |= DRM_MODE_TYPE_DRIVER;

		if (dimming_panel->num_modes == 1)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_set_name(mode);
		drm_mode_probed_add(connector, mode);
		num++;
	}

	if (dimming_panel->bus_format)
		drm_display_info_set_bus_formats(&connector->display_info,
						 &dimming_panel->bus_format, 1);
	if (dimming_panel->bus_flags)
		connector->display_info.bus_flags = dimming_panel->bus_flags;

	return num;
}

static const struct drm_panel_funcs rockchip_dimming_panel_funcs = {
	.prepare = rockchip_dimming_panel_prepare,
	.enable = rockchip_dimming_panel_enable,
	.disable = rockchip_dimming_panel_disable,
	.unprepare = rockchip_dimming_panel_unprepare,
	.get_modes = rockchip_dimming_panel_get_modes,
};

static int rockchip_dimming_panel_loader_protect(struct rockchip_drm_sub_dev *sub_dev, bool on)
{
	struct rockchip_dimming_panel *dimming_panel = container_of(sub_dev,
								    struct rockchip_dimming_panel,
								    sub_dev);
	int ret;

	if (on) {
		ret = rockchip_dimming_panel_regulator_enable(dimming_panel);
		if (ret < 0) {
			dev_err(dimming_panel->base.dev, "failed to enable regulator: %d\n", ret);
			return ret;
		}

		kthread_queue_delayed_work(dimming_panel->dimming_worker,
					   &dimming_panel->dimming_delayed_work, 0);

		dimming_panel->enabled = true;
		dimming_panel->prepared = true;
	} else {
		dimming_panel->enabled = false;
		dimming_panel->prepared = false;

		kthread_cancel_delayed_work_sync(&dimming_panel->dimming_delayed_work);

		rockchip_dimming_panel_regulator_disable(dimming_panel);
	}

	return 0;
}

static int rockchip_dimming_panel_of_get_data(struct rockchip_dimming_panel *dimming_panel)
{
	struct device *dev = dimming_panel->dev;
	struct device_node *np;
	struct drm_display_mode *mode;
	const void *data;
	uint32_t bus_flags;
	uint32_t element_bytes;
	int len;
	int ret;

	dimming_panel->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(dimming_panel->supply))
		return dev_err_probe(dev, PTR_ERR(dimming_panel->supply),
				     "failed to get power regulator\n");

	dimming_panel->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_ASIS);
	if (IS_ERR(dimming_panel->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(dimming_panel->enable_gpio),
				     "failed to get enable GPIO\n");

	dimming_panel->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(dimming_panel->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(dimming_panel->reset_gpio),
				     "failed to get reset GPIO\n");

	dimming_panel->lden_gpio = devm_gpiod_get_optional(dev, "lden", GPIOD_ASIS);
	if (IS_ERR(dimming_panel->lden_gpio))
		return dev_err_probe(dev, PTR_ERR(dimming_panel->lden_gpio),
				     "failed to get lden GPIO\n");

	dimming_panel->blen_gpio = devm_gpiod_get_optional(dev, "blen", GPIOD_ASIS);
	if (IS_ERR(dimming_panel->blen_gpio)) {
		return dev_err_probe(dev, PTR_ERR(dimming_panel->blen_gpio),
				     "failed to get blen GPIO\n");
	}

	dimming_panel->dbcl_gpio = devm_gpiod_get_optional(dev, "dbcl", GPIOD_ASIS);
	if (IS_ERR(dimming_panel->dbcl_gpio))
		return dev_err_probe(dev, PTR_ERR(dimming_panel->dbcl_gpio),
				     "failed to get dbcl GPIO\n");

	dimming_panel->sync_gpio = devm_gpiod_get_optional(dev, "sync", GPIOD_ASIS);
	if (IS_ERR(dimming_panel->sync_gpio))
		return dev_err_probe(dev, PTR_ERR(dimming_panel->sync_gpio),
				     "failed to get sync GPIO\n");

	np = dimming_panel->dev->of_node;
	if (of_child_node_is_present(np, "display-timings")) {
		mode = devm_kzalloc(dev, sizeof(*mode), GFP_KERNEL);
		if (!mode)
			return -ENOMEM;

		if (!of_get_drm_display_mode(np, mode, &bus_flags, OF_USE_NATIVE_MODE)) {
			dimming_panel->modes = mode;
			dimming_panel->num_modes = 1;
			dimming_panel->bus_flags = bus_flags;

			of_property_read_u32(np, "bus-format", &dimming_panel->bus_format);

			of_property_read_u32(np, "prepare-delay-ms", &dimming_panel->delay.prepare);
			of_property_read_u32(np, "enable-delay-ms", &dimming_panel->delay.enable);
			of_property_read_u32(np, "disable-delay-ms", &dimming_panel->delay.disable);
			of_property_read_u32(np, "unprepare-delay-ms",
					     &dimming_panel->delay.unprepare);
			of_property_read_u32(np, "reset-delay-ms", &dimming_panel->delay.reset);
			of_property_read_u32(np, "init-delay-ms", &dimming_panel->delay.init);
			of_property_read_u32(np, "vsync-hold-us", &dimming_panel->delay.vsync_hold);
			of_property_read_u32(np, "vsync-back-us", &dimming_panel->delay.vsync_back);
		} else {
			devm_kfree(dev, mode);
		}
	}

	/* Parameters to report */
	ret = of_property_read_u32(np, "hzone-num", &dimming_panel->hzone_num);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get horizontal zone number\n");
	ret = of_property_read_u32(np, "vzone-num", &dimming_panel->vzone_num);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get vertical zone number\n");
	dimming_panel->zone_max = dimming_panel->hzone_num * dimming_panel->vzone_num;

	ret = of_property_read_u32(np, "brightness-max", &dimming_panel->brightness_max);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get brightness max value\n");
	ret = of_property_read_u32(np, "brightness-min", &dimming_panel->brightness_min);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get brightness min value\n");
	ret = of_property_read_u32(np, "brightness-bpc", &dimming_panel->brightness_bpc);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get brightness bpc value\n");

	if (dimming_panel->brightness_bpc != 8 && dimming_panel->brightness_bpc != 16)
		return dev_err_probe(dev, -EINVAL, "brightness bpc value should be 8 or 16\n");

	element_bytes = dimming_panel->brightness_bpc / 8;
	dimming_panel->data = devm_kzalloc(dev, element_bytes * dimming_panel->zone_max,
					   GFP_KERNEL);
	if (!dimming_panel->data)
		return -ENOMEM;

	dimming_panel->cmd_element_size = dimming_panel->zone_max;
	data = of_get_property(np, "command-header", &len);
	if (data) {
		dimming_panel->cmd_header = devm_kzalloc(dev, len, GFP_KERNEL);
		if (!dimming_panel->cmd_header)
			return -ENOMEM;

		memcpy(dimming_panel->cmd_header, data, len);
		dimming_panel->cmd_header_len = len;
	}
	dimming_panel->cmd_element_size += dimming_panel->cmd_header_len / element_bytes;

	data = of_get_property(np, "command-tail", &len);
	if (data) {
		dimming_panel->cmd_tail = devm_kzalloc(dev, len, GFP_KERNEL);
		if (!dimming_panel->cmd_tail)
			return -ENOMEM;

		memcpy(dimming_panel->cmd_tail, data, len);
		dimming_panel->cmd_tail_len = len;
	}
	dimming_panel->cmd_element_size += dimming_panel->cmd_tail_len / element_bytes;

	return 0;
}

static int rockchip_dimming_panel_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct device *dimming_dev;
	struct rockchip_dimming_panel *dimming_panel;
	int ret;

	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to setup spi\n");

	dimming_panel = devm_kzalloc(dev, sizeof(*dimming_panel), GFP_KERNEL);
	if (!dimming_panel)
		return -ENOMEM;
	dimming_panel->dev = dev;

	ret = rockchip_dimming_panel_of_get_data(dimming_panel);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get dimming panel configs\n");

	dev_set_drvdata(dev, dimming_panel);

	drm_panel_init(&dimming_panel->base, dev, &rockchip_dimming_panel_funcs,
		       DRM_MODE_CONNECTOR_Unknown);

	ret = drm_panel_of_backlight(&dimming_panel->base);
	if (ret)
		return dev_err_probe(dev, ret, "failed to find backlight\n");

	drm_panel_add(&dimming_panel->base);

	dimming_panel->id = bitmap_find_next_zero_area(allocated_dimming_panels,
						       MAX_DIMMING_PANELS, 0, 1, 0);
	dimming_dev = device_create(rockchip_dimming_class, dev, MKDEV(0, 0), spi,
				    "dimming_%d", dimming_panel->id);
	if (IS_ERR(dimming_dev)) {
		dev_err(dev, "Failed to create rockchip dimming device\n");
		ret = PTR_ERR(dimming_dev);
		goto remove_panel;
	}
	dimming_panel->dimming_dev = dimming_dev;
	bitmap_set(allocated_dimming_panels, dimming_panel->id, 1);

	dev_set_drvdata(dimming_dev, spi);

	ret = sysfs_create_group(&dimming_dev->kobj, &rockchip_dimming_attr_group);
	if (ret)
		goto destroy_device;

	devm_rockchip_panel_notifier_register(dev, &dimming_panel->base,
					      &dimming_panel->panel_notifier);

	dimming_panel->dimming_worker = kthread_create_worker(0, dev_name(dimming_dev));
	if (IS_ERR(dimming_panel->dimming_worker)) {
		dev_err(dimming_dev, "Failed to create rockchip dimming worker\n");
		ret = PTR_ERR(dimming_panel->dimming_worker);
		goto remove_group;
	}

	kthread_init_delayed_work(&dimming_panel->dimming_delayed_work, dimming_delayed_work_func);

	dimming_panel->sub_dev.of_node = dev->of_node;
	dimming_panel->sub_dev.loader_protect = rockchip_dimming_panel_loader_protect;
	rockchip_drm_register_sub_dev(&dimming_panel->sub_dev);

	return 0;

remove_group:
	sysfs_remove_group(&dimming_dev->kobj, &rockchip_dimming_attr_group);
destroy_device:
	bitmap_clear(allocated_dimming_panels, dimming_panel->id, 1);
	device_destroy(rockchip_dimming_class, dimming_dev->devt);
remove_panel:
	drm_panel_remove(&dimming_panel->base);

	return ret;
}

static void rockchip_dimming_panel_remove(struct spi_device *spi)
{
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);
	struct device *dimming_dev = dimming_panel->dimming_dev;

	rockchip_drm_unregister_sub_dev(&dimming_panel->sub_dev);

	kthread_destroy_worker(dimming_panel->dimming_worker);

	sysfs_remove_group(&dimming_dev->kobj, &rockchip_dimming_attr_group);

	bitmap_clear(allocated_dimming_panels, dimming_panel->id, 1);
	device_destroy(rockchip_dimming_class, dimming_dev->devt);

	drm_panel_remove(&dimming_panel->base);
	drm_panel_disable(&dimming_panel->base);
	drm_panel_unprepare(&dimming_panel->base);
}

static void rockchip_dimming_panel_shutdown(struct spi_device *spi)
{
	struct rockchip_dimming_panel *dimming_panel = dev_get_drvdata(&spi->dev);

	drm_panel_disable(&dimming_panel->base);
	drm_panel_unprepare(&dimming_panel->base);
}

static const struct spi_device_id rockchip_dimming_panel_ids[] = {
	{ .name = "rockchip,dimming-panel" },
	{},
};
MODULE_DEVICE_TABLE(spi, rockchip_dimming_panel_ids);

static const struct of_device_id rockchip_dimming_panel_of_match[] = {
	{ .compatible = "rockchip,dimming-panel" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rockchip_dimming_panel_of_match);

static struct spi_driver rockchip_dimming_panel_driver = {
	.probe			= rockchip_dimming_panel_probe,
	.remove			= rockchip_dimming_panel_remove,
	.shutdown		= rockchip_dimming_panel_shutdown,
	.id_table		= rockchip_dimming_panel_ids,
	.driver	= {
		.name		= "rockchip-dimming-panel",
		.of_match_table = rockchip_dimming_panel_of_match,
	},
};

static int __init rockchip_dimming_panel_init(void)
{
	rockchip_dimming_class = class_create(THIS_MODULE, "dimming");
	if (IS_ERR(rockchip_dimming_class)) {
		pr_err("Failed to create rockchip dimming class\n");
		return PTR_ERR(rockchip_dimming_class);
	}

	return spi_register_driver(&rockchip_dimming_panel_driver);
}
module_init(rockchip_dimming_panel_init);

static void __exit rockchip_dimming_panel_exit(void)
{
	spi_unregister_driver(&rockchip_dimming_panel_driver);
	class_destroy(rockchip_dimming_class);
}
module_exit(rockchip_dimming_panel_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Damon Ding <damon.ding@rock-chips.com>");
MODULE_DESCRIPTION("rockchip dimming panel");
MODULE_SOFTDEP("pre: rockchipdrm");
