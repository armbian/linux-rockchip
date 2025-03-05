// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd.
 *
 * Author: Guochun Huang <hero.huang@rock-chips.com>
 */

#include <linux/debugfs.h>
#include "linux/printk.h"
#include "rk628.h"
#include "rk628_config.h"
#include "rk628_combtxphy.h"
#include "rk628_gvi.h"
#include "panel.h"

#define GVI_RETRY_TIMEOUT	10

int rk628_gvi_parse(struct rk628 *rk628, struct device_node *gvi_np)
{
	const char *string;
	u32 val;
	int ret;

	if (!of_device_is_available(gvi_np))
		return -EINVAL;

	if (!of_property_read_u32(gvi_np, "gvi,lanes", &val))
		rk628->gvi.lanes = val;

	if (of_property_read_bool(gvi_np, "rockchip,division-mode"))
		rk628->gvi.division_mode = true;
	else
		rk628->gvi.division_mode = false;

	if (of_property_read_bool(gvi_np, "rockchip,gvi-frm-rst"))
		rk628->gvi.frm_rst = true;
	else
		rk628->gvi.frm_rst = false;

	if (!of_property_read_string(gvi_np, "bus-format", &string)) {
		if (!strcmp(string, "rgb666"))
			rk628->gvi.bus_format = GVI_MEDIA_BUS_FMT_RGB666_1X18;
		else if (!strcmp(string, "rgb101010"))
			rk628->gvi.bus_format = GVI_MEDIA_BUS_FMT_RGB101010_1X30;
		else if (!strcmp(string, "yuyv8"))
			rk628->gvi.bus_format = GVI_MEDIA_BUS_FMT_YUYV8_1X16;
		else if (!strcmp(string, "yuyv10"))
			rk628->gvi.bus_format = GVI_MEDIA_BUS_FMT_YUYV10_1X20;
		else
			rk628->gvi.bus_format = GVI_MEDIA_BUS_FMT_RGB888_1X24;
	}

	ret = rk628_panel_info_get(rk628, gvi_np);
	if (ret)
		return ret;

	return 0;
}

static void rk628_gvi_get_info(struct rk628_gvi *gvi)
{
	switch (gvi->bus_format) {
	case GVI_MEDIA_BUS_FMT_RGB666_1X18:
		gvi->byte_mode = 3;
		gvi->color_depth = COLOR_DEPTH_RGB_YUV444_18BIT;
		break;
	case GVI_MEDIA_BUS_FMT_RGB888_1X24:
		gvi->byte_mode = 4;
		gvi->color_depth = COLOR_DEPTH_RGB_YUV444_24BIT;
		break;
	case GVI_MEDIA_BUS_FMT_RGB101010_1X30:
		gvi->byte_mode = 4;
		gvi->color_depth = COLOR_DEPTH_RGB_YUV444_30BIT;
		break;
	case GVI_MEDIA_BUS_FMT_YUYV8_1X16:
		gvi->byte_mode = 3;
		gvi->color_depth = COLOR_DEPTH_YUV422_16BIT;
		break;
	case GVI_MEDIA_BUS_FMT_YUYV10_1X20:
		gvi->byte_mode = 3;
		gvi->color_depth = COLOR_DEPTH_YUV422_20BIT;
		break;
	default:
		gvi->byte_mode = 3;
		gvi->color_depth = COLOR_DEPTH_RGB_YUV444_24BIT;
		pr_err("unsupported bus_format: 0x%x\n", gvi->bus_format);
		break;
	}
}

static unsigned int rk628_gvi_get_lane_rate(struct rk628 *rk628)
{
	struct rk628_gvi *gvi = &rk628->gvi;
	u32 lane_bit_rate, min_lane_rate = 500000, max_lane_rate = 4000000;
	u64 total_bw;
	struct rk628_display_mode *src = &rk628->src_mode;
	const struct rk628_display_mode *dst = &rk628->dst_mode;
	u64 dst_rate, src_rate;

	src_rate = src->clock * 1000;
	dst_rate = src_rate * dst->vtotal * dst->htotal;
	do_div(dst_rate, (src->vtotal * src->htotal));
	do_div(dst_rate, 1000);

	/**
	 * [ENCODER TOTAL BIT-RATE](bps) = [byte mode](byte) x 10 / [pixel clock](HZ)
	 *
	 * lane_bit_rate = [total bit-rate](bps) / [lane number]
	 *
	 * 500Mbps <= lane_bit_rate <= 4Gbps
	 */
	total_bw = (u64)gvi->byte_mode * 10 * dst_rate;/* Kbps */
	do_div(total_bw, gvi->lanes);
	lane_bit_rate = total_bw;

	if (lane_bit_rate < min_lane_rate)
		lane_bit_rate = min_lane_rate;
	if (lane_bit_rate > max_lane_rate)
		lane_bit_rate = max_lane_rate;

	return lane_bit_rate;
}

static void rk628_gvi_pre_enable(struct rk628 *rk628, struct rk628_gvi *gvi)
{
	/* gvi reset */
	rk628_i2c_update_bits(rk628, GVI_SYS_RST, SYS_RST_SOFT_RST,
			      SYS_RST_SOFT_RST);
	udelay(10);
	rk628_i2c_update_bits(rk628, GVI_SYS_RST, SYS_RST_SOFT_RST, 0);
	udelay(10);

	rk628_i2c_write(rk628, GRF_SCALER_CON0, SCL_8_PIXEL_ALIGN(1));

	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL0, SYS_CTRL0_LANE_NUM_MASK,
			      SYS_CTRL0_LANE_NUM(gvi->lanes - 1));
	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL0, SYS_CTRL0_BYTE_MODE_MASK,
			      SYS_CTRL0_BYTE_MODE(gvi->byte_mode ==
			      3 ? 0 : (gvi->byte_mode == 4 ? 1 : 2)));
	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL0, SYS_CTRL0_SECTION_NUM_MASK,
			      SYS_CTRL0_SECTION_NUM(gvi->division_mode));
	rk628_i2c_update_bits(rk628, GRF_POST_PROC_CON, SW_SPLIT_EN,
			      gvi->division_mode ? SW_SPLIT_EN : 0);
	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL1, SYS_CTRL1_DUAL_PIXEL_EN,
			      gvi->division_mode ? SYS_CTRL1_DUAL_PIXEL_EN : 0);

	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL0, SYS_CTRL0_FRM_RST_EN,
			      gvi->frm_rst ? SYS_CTRL0_FRM_RST_EN : 0);
	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL1, SYS_CTRL1_LANE_ALIGN_EN, 0);

	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL1, SYS_CTRL1_COLOR_DEPTH_MASK,
			      SYS_CTRL1_COLOR_DEPTH(gvi->color_depth));
}

static void rk628_gvi_enable_color_bar(struct rk628 *rk628,
				       struct rk628_gvi *gvi)
{
	const struct rk628_display_mode *mode = &rk628->dst_mode;
	u16 vm_hactive, vm_hback_porch, vm_hsync_len;
	u16 vm_vactive, vm_vback_porch, vm_vsync_len;
	u16 hsync_len, hact_st, hact_end, htotal;
	u16 vsync_len, vact_st, vact_end, vtotal;

	vm_hactive = mode->hdisplay;
	vm_hsync_len = mode->hsync_end - mode->hsync_start;
	vm_hback_porch = mode->htotal - mode->hsync_end;

	vm_vactive = mode->vdisplay;
	vm_vsync_len = mode->vsync_end - mode->vsync_start;
	vm_vback_porch = mode->vtotal - mode->vsync_end;

	if (gvi->division_mode) {
		hsync_len = vm_hsync_len / 2;
		hact_st = (vm_hsync_len + vm_hback_porch) / 2;
		hact_end = (vm_hsync_len + vm_hback_porch + vm_hactive) / 2;
		htotal = mode->htotal / 2;
	} else {
		hsync_len = vm_hsync_len;
		hact_st = vm_hsync_len + vm_hback_porch;
		hact_end = vm_hsync_len + vm_hback_porch + vm_hactive;
		htotal = mode->htotal;
	}
	vsync_len = vm_vsync_len;
	vact_st = vsync_len + vm_vback_porch;
	vact_end = vact_st + vm_vactive;
	vtotal = mode->vtotal;

	rk628_i2c_write(rk628, GVI_COLOR_BAR_HTIMING0,
			hact_st << 16 | hsync_len);
	rk628_i2c_write(rk628, GVI_COLOR_BAR_HTIMING1,
			(htotal - 1) << 16 | hact_end);
	rk628_i2c_write(rk628, GVI_COLOR_BAR_VTIMING0,
			vact_st << 16 | vsync_len);
	rk628_i2c_write(rk628, GVI_COLOR_BAR_VTIMING1,
			(vtotal - 1) << 16 | vact_end);
	rk628_i2c_update_bits(rk628, GVI_COLOR_BAR_CTRL, COLOR_BAR_EN, 0);
}

static int rk628_gvi_color_bar_show(struct seq_file *s, void *data)
{
	seq_puts(s, "  Enable color bar:\n");
	seq_puts(s, "      example: echo 1 > /sys/kernel/debug/rk628/2-0050/gvi_color_bar\n");
	seq_puts(s, "  Disable color bar:\n");
	seq_puts(s, "      example: echo 0 > /sys/kernel/debug/rk628/2-0050/gvi_color_bar\n");

	return 0;
}

static int rk628_gvi_color_bar_open(struct inode *inode, struct file *file)
{
	return single_open(file, rk628_gvi_color_bar_show, inode->i_private);
}

static ssize_t rk628_gvi_color_bar_write(struct file *file, const char __user *ubuf,
					 size_t len, loff_t *offp)
{
	struct rk628 *rk628 = ((struct seq_file *)file->private_data)->private;
	u8 mode;

	if (kstrtou8_from_user(ubuf, len, 0, &mode))
		return -EFAULT;

	rk628_i2c_update_bits(rk628, GVI_COLOR_BAR_CTRL, COLOR_BAR_EN, mode ? 1 : 0);

	return len;
}

static const struct file_operations rk628_gvi_color_bar_fops = {
	.owner = THIS_MODULE,
	.open = rk628_gvi_color_bar_open,
	.read = seq_read,
	.write = rk628_gvi_color_bar_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void rk628_gvi_create_debugfs_file(struct rk628 *rk628)
{
	if (rk628_output_is_gvi(rk628))
		debugfs_create_file("gvi_color_bar", 0600, rk628->debug_dir,
				    rk628, &rk628_gvi_color_bar_fops);
}

static void rk628_gvi_post_enable(struct rk628 *rk628, struct rk628_gvi *gvi)
{
	u32 val;

	val = SYS_CTRL0_GVI_EN | SYS_CTRL0_AUTO_GATING;
	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL0, val, 3);
}

void rk628_gvi_enable(struct rk628 *rk628)
{
	struct rk628_gvi *gvi = &rk628->gvi;
	unsigned int rate;
	u32 mask = SW_OUTPUT_MODE_MASK;
	u32 val = SW_OUTPUT_MODE(OUTPUT_MODE_GVI);
	int i = 0;

	rk628_gvi_get_info(gvi);
	rate = rk628_gvi_get_lane_rate(rk628);

	/* set gvi_hpd and gvi_lock mux */
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x06000600);

	if (rk628->version == RK628F_VERSION) {
		mask = SW_OUTPUT_COMBTX_MODE_MASK;
		val = SW_OUTPUT_COMBTX_MODE(OUTPUT_MODE_GVI);
	}

	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, mask, val);
	rk628_combtxphy_set_bus_width(rk628, rate);
	rk628_combtxphy_set_gvi_division_mode(rk628, gvi->division_mode);
	rk628_combtxphy_set_mode(rk628, PHY_MODE_VIDEO_GVI);
	rate = rk628_combtxphy_get_bus_width(rk628);
	rk628_combtxphy_power_on(rk628);
	rk628_gvi_pre_enable(rk628, gvi);
	rk628_panel_prepare(rk628);
	rk628_gvi_enable_color_bar(rk628, gvi);
	rk628_gvi_post_enable(rk628, gvi);

	for (i = 0; i < 100; i++) {
		rk628_i2c_read(rk628, GVI_STATUS, &val);
		if ((val & GVI_LOCKED_MASK) == GVI_LOCKED_STATUS)
			break;
		usleep_range(1000, 1100);
	}

	if (i == 100 && gvi->retry_times < GVI_RETRY_TIMEOUT) {
		dev_info(rk628->dev, "GVI Lock failed: 0x%x, try again: %d\n", val, gvi->retry_times);
		gvi->retry_times++;
		rk628_gvi_disable(rk628);
		usleep_range(50000, 51000);
		rk628_gvi_enable(rk628);
	}

	if (gvi->retry_times >= GVI_RETRY_TIMEOUT && i == 100) {
		dev_info(rk628->dev, "GVI Lock failed, please check hardware!\n");
		return;
	}
	rk628_panel_enable(rk628);

	if (i != 100)
		gvi->retry_times = 0;
	dev_info(rk628->dev,
		 "GVI-Link bandwidth: %d x %d Mbps, Byte mode: %d, Color Depty: %d, %s division mode\n",
		 rate, gvi->lanes, gvi->byte_mode, gvi->color_depth,
		 gvi->division_mode ? "two" : "one");
}

void rk628_gvi_disable(struct rk628 *rk628)
{
	rk628_panel_disable(rk628);
	rk628_panel_unprepare(rk628);
	rk628_i2c_update_bits(rk628, GVI_SYS_CTRL0, SYS_CTRL0_GVI_EN, 0);
	rk628_combtxphy_power_off(rk628);
}

