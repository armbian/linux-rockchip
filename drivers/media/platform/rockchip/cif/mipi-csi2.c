// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip MIPI CSI2 Driver
 *
 * Copyright (C) 2019 Rockchip Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/rk-camera-module.h>
#include <media/v4l2-ioctl.h>
#include "mipi-csi2.h"
#include <linux/regulator/consumer.h>

static int csi2_debug;
module_param_named(debug_csi2, csi2_debug, int, 0644);
MODULE_PARM_DESC(debug_csi2, "Debug level (0-1)");

#define write_csihost_reg(base, addr, val)  writel(val, (addr) + (base))
#define read_csihost_reg(base, addr) readl((addr) + (base))

static ATOMIC_NOTIFIER_HEAD(g_csi_host_chain);

int rkcif_csi2_register_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&g_csi_host_chain, nb);
}

int rkcif_csi2_unregister_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&g_csi_host_chain, nb);
}

static inline struct csi2_dev *sd_to_dev(struct v4l2_subdev *sdev)
{
	return container_of(sdev, struct csi2_dev, sd);
}

static struct csi2_sensor_info *sd_to_sensor(struct csi2_dev *csi2,
					struct v4l2_subdev *sd)
{
	int i;

	for (i = 0; i < csi2->num_sensors; ++i)
		if (csi2->sensors[i].sd == sd)
			return &csi2->sensors[i];

	return NULL;
}

static struct v4l2_subdev *get_remote_sensor(struct v4l2_subdev *sd)
{
	struct media_pad *local, *remote;
	struct media_entity *sensor_me;

	local = &sd->entity.pads[RK_CSI2_PAD_SINK];
	remote = media_pad_remote_pad_first(local);
	if (!remote) {
		v4l2_warn(sd, "No link between dphy and sensor\n");
		return NULL;
	}

	sensor_me = media_pad_remote_pad_first(local)->entity;
	return media_entity_to_v4l2_subdev(sensor_me);
}

static void get_remote_terminal_sensor(struct v4l2_subdev *sd,
				       struct v4l2_subdev **sensor_sd)
{
	struct media_graph graph;
	struct media_entity *entity = &sd->entity;
	struct media_device *mdev = entity->graph_obj.mdev;
	int ret;

	/* Walk the graph to locate sensor nodes. */
	mutex_lock(&mdev->graph_mutex);
	ret = media_graph_walk_init(&graph, mdev);
	if (ret) {
		mutex_unlock(&mdev->graph_mutex);
		*sensor_sd = NULL;
		return;
	}

	media_graph_walk_start(&graph, entity);
	while ((entity = media_graph_walk_next(&graph))) {
		if (entity->function == MEDIA_ENT_F_CAM_SENSOR)
			break;
	}
	mutex_unlock(&mdev->graph_mutex);
	media_graph_walk_cleanup(&graph);

	if (entity)
		*sensor_sd = media_entity_to_v4l2_subdev(entity);
	else
		*sensor_sd = NULL;
}

static void csi2_update_sensor_info(struct csi2_dev *csi2)
{
	struct v4l2_subdev *terminal_sensor_sd = NULL;
	struct csi2_sensor_info *sensor = &csi2->sensors[0];
	struct v4l2_mbus_config mbus;
	int ret = 0;

	ret = v4l2_subdev_call(sensor->sd, pad, get_mbus_config, 0, &mbus);
	if (ret) {
		v4l2_err(&csi2->sd, "update sensor info failed!\n");
		return;
	}

	get_remote_terminal_sensor(&csi2->sd, &terminal_sensor_sd);
	ret = v4l2_subdev_call(terminal_sensor_sd, core, ioctl,
				RKMODULE_GET_CSI_DSI_INFO, &csi2->dsi_input_en);
	if (ret) {
		v4l2_dbg(1, csi2_debug, &csi2->sd, "get CSI/DSI sel failed, default csi!\n");
		csi2->dsi_input_en = 0;
	}

	csi2->mbus = mbus;

}

static void csi2_hw_do_reset(struct csi2_hw *csi2_hw)
{

	if (!csi2_hw->rsts_bulk)
		return;

	reset_control_assert(csi2_hw->rsts_bulk);

	udelay(5);

	reset_control_deassert(csi2_hw->rsts_bulk);
}

static int csi2_enable_clks(struct csi2_hw *csi2_hw)
{
	int ret = 0;

	if (!csi2_hw->clks_bulk) {
		dev_info(csi2_hw->dev, "clks is NULL, please check it if needs\n");
		return 0;
	}

	ret = clk_bulk_prepare_enable(csi2_hw->clks_num, csi2_hw->clks_bulk);
	if (ret)
		dev_err(csi2_hw->dev, "failed to enable clks\n");

	return ret;
}

static void csi2_disable_clks(struct csi2_hw *csi2_hw)
{
	if (!csi2_hw->clks_bulk)
		return;
	clk_bulk_disable_unprepare(csi2_hw->clks_num,  csi2_hw->clks_bulk);
}

static void csi2_disable(struct csi2_hw *csi2_hw)
{
	write_csihost_reg(csi2_hw->base, CSIHOST_RESETN, 0);
	write_csihost_reg(csi2_hw->base, CSIHOST_MSK1, 0xffffffff);
	write_csihost_reg(csi2_hw->base, CSIHOST_MSK2, 0xffffffff);
}

static void csi2_enable(struct csi2_hw *csi2_hw,
			enum host_type_t host_type)
{
	void __iomem *base = csi2_hw->base;
	struct csi2_dev *csi2 = csi2_hw->csi2;
	int lanes = csi2->mbus.bus.mipi_csi2.num_data_lanes;
	struct v4l2_mbus_config mbus = csi2->mbus;
	u32 val = 0;
	u32 mask1 = 0;
	struct v4l2_subdev *terminal_sensor_sd = NULL;
	struct rkmodule_hdr_cfg hdr_cfg = {0};
	int ret = 0;

	if (mbus.type == V4L2_MBUS_CSI2_DPHY)
		val = SW_CPHY_EN(0);
	else if (mbus.type == V4L2_MBUS_CSI2_CPHY)
		val = SW_CPHY_EN(1);

	write_csihost_reg(base, CSIHOST_N_LANES, lanes - 1);

	if (csi2->sw_dbg)
		val |= BIT(6);

	get_remote_terminal_sensor(&csi2->sd, &terminal_sensor_sd);
	if (terminal_sensor_sd) {
		ret = v4l2_subdev_call(terminal_sensor_sd,
				       core, ioctl,
				       RKMODULE_GET_HDR_CFG,
				       &hdr_cfg);
		if (ret != 0)
			hdr_cfg.hdr_mode = NO_HDR;
		if (strstr(terminal_sensor_sd->name, "sc") &&
		    (hdr_cfg.hdr_mode == HDR_X2 || hdr_cfg.hdr_mode == HDR_X3)) {
			mask1 = CSIHOST_ERR1_ERR_BNDRY_MATCH;
			csi2->is_detect_fs_fe = false;
		} else {
			csi2->is_detect_fs_fe = true;
		}
	} else {
		csi2->is_detect_fs_fe = true;
	}
	if (host_type == RK_DSI_RXHOST) {
		val |= SW_DSI_EN(1) | SW_DATATYPE_FS(0x01) |
		       SW_DATATYPE_FE(0x11) | SW_DATATYPE_LS(0x21) |
		       SW_DATATYPE_LE(0x31);
		write_csihost_reg(base, CSIHOST_CONTROL, val);
		/* Disable some error interrupt when HOST work on DSI RX mode */
		write_csihost_reg(base, CSIHOST_MSK1, 0xe00000f0);
		write_csihost_reg(base, CSIHOST_MSK2, 0xff00);
	} else {
		val |= SW_DSI_EN(0) | SW_DATATYPE_FS(0x0) |
		       SW_DATATYPE_FE(0x01) | SW_DATATYPE_LS(0x02) |
		       SW_DATATYPE_LE(0x03);
		write_csihost_reg(base, CSIHOST_CONTROL, val);
		write_csihost_reg(base, CSIHOST_MSK1, mask1);
		write_csihost_reg(base, CSIHOST_MSK2, 0xf000);
		csi2->is_check_sot_sync = true;
	}

	write_csihost_reg(base, CSIHOST_RESETN, 1);
}

static int csi2_hw_start(struct csi2_hw *csi2_hw, enum host_type_t host_type)
{
	int ret = 0;

	if (atomic_inc_return(&csi2_hw->stream_count) > 1)
		return ret;
	csi2_hw_do_reset(csi2_hw);
	ret = csi2_enable_clks(csi2_hw);
	if (ret) {
		dev_err(csi2_hw->dev, "%s: enable clks failed, %s\n",
			 __func__, csi2_hw->dev_name);
		return ret;
	}
	enable_irq(csi2_hw->irq1);
	enable_irq(csi2_hw->irq2);
	csi2_enable(csi2_hw, host_type);
	return ret;
}

static void csi2_hw_stop(struct csi2_hw *csi2_hw)
{
	if (atomic_dec_return(&csi2_hw->stream_count) > 0)
		return;
	disable_irq(csi2_hw->irq1);
	disable_irq(csi2_hw->irq2);
	csi2_disable(csi2_hw);
	csi2_disable_clks(csi2_hw);
}

static int csi2_start(struct csi2_dev *csi2)
{
	enum host_type_t host_type;
	int ret = 0, i;
	int csi_idx = 0;

	atomic_set(&csi2->frm_sync_seq, 0);

	csi2_update_sensor_info(csi2);

	if (csi2->mbus.type != V4L2_MBUS_CSI2_DPHY &&
	    csi2->mbus.type != V4L2_MBUS_CSI2_CPHY)
		return 0;

	if (csi2->dsi_input_en == RKMODULE_DSI_INPUT)
		host_type = RK_DSI_RXHOST;
	else
		host_type = RK_CSI_RXHOST;

	csi2->irq1_timestamp = 0;
	csi2->irq2_timestamp = 0;
	for (i = 0; i < csi2->csi_info.csi_num; i++) {
		csi_idx = csi2->csi_info.csi_idx[i];
		ret |= csi2_hw_start(csi2->csi2_hw[csi_idx], host_type);
	}

	pr_debug("stream sd: %s\n", csi2->src_sd->name);
	ret = v4l2_subdev_call(csi2->src_sd, video, s_stream, 1);
	ret = (ret && ret != -ENOIOCTLCMD) ? ret : 0;
	if (ret)
		goto err_assert_reset;

	for (i = 0; i < RK_CSI2_ERR_MAX; i++)
		csi2->err_list[i].cnt = 0;

	return 0;

err_assert_reset:
	for (i = 0; i < csi2->csi_info.csi_num; i++) {
		csi_idx = csi2->csi_info.csi_idx[i];
		csi2_hw_stop(csi2->csi2_hw[csi_idx]);
	}

	return ret;
}

static void csi2_stop(struct csi2_dev *csi2)
{
	int i = 0;
	int csi_idx = 0;

	if (csi2->mbus.type != V4L2_MBUS_CSI2_DPHY &&
	    csi2->mbus.type != V4L2_MBUS_CSI2_CPHY)
		return;

	/* stop upstream */
	v4l2_subdev_call(csi2->src_sd, video, s_stream, 0);

	for (i = 0; i < csi2->csi_info.csi_num; i++) {
		csi_idx = csi2->csi_info.csi_idx[i];
		csi2_hw_stop(csi2->csi2_hw[csi_idx]);
	}
}

/*
 * V4L2 subdev operations.
 */

static int csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct csi2_dev *csi2 = sd_to_dev(sd);
	int ret = 0;

	mutex_lock(&csi2->lock);

	dev_err(csi2->dev, "stream %s, src_sd: %p, sd_name:%s\n",
		enable ? "on" : "off",
		csi2->src_sd, csi2->src_sd->name);

	/*
	 * enable/disable streaming only if stream_count is
	 * going from 0 to 1 / 1 to 0.
	 */
	if (csi2->stream_count != !enable)
		goto update_count;

	dev_err(csi2->dev, "stream %s\n", enable ? "ON" : "OFF");

	if (enable)
		ret = csi2_start(csi2);
	else
		csi2_stop(csi2);
	if (ret)
		goto out;

update_count:
	csi2->stream_count += enable ? 1 : -1;
	if (csi2->stream_count < 0)
		csi2->stream_count = 0;
out:
	mutex_unlock(&csi2->lock);

	return ret;
}

static int csi2_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct csi2_dev *csi2 = sd_to_dev(sd);
	struct v4l2_subdev *remote_sd;
	int ret = 0;

	remote_sd = media_entity_to_v4l2_subdev(remote->entity);

	mutex_lock(&csi2->lock);

	if (local->flags & MEDIA_PAD_FL_SOURCE) {
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (csi2->sink_linked[local->index - 1]) {
				ret = -EBUSY;
				goto out;
			}
			csi2->sink_linked[local->index - 1] = true;
		} else {
			csi2->sink_linked[local->index - 1] = false;
		}
	} else {
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (csi2->src_sd) {
				ret = -EBUSY;
				goto out;
			}
			csi2->src_sd = remote_sd;
		} else {
			csi2->src_sd = NULL;
		}
	}

out:
	mutex_unlock(&csi2->lock);
	return ret;
}

static int csi2_media_init(struct v4l2_subdev *sd)
{
	struct csi2_dev *csi2 = sd_to_dev(sd);
	int i = 0, num_pads = 0;

	num_pads = csi2->match_data->num_pads;

	for (i = 0; i < num_pads; i++) {
		csi2->pad[i].flags = (i == CSI2_SINK_PAD) ?
		MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;
	}

	csi2->pad[RK_CSI2X_PAD_SOURCE0].flags =
		MEDIA_PAD_FL_SOURCE | MEDIA_PAD_FL_MUST_CONNECT;
	csi2->pad[RK_CSI2_PAD_SINK].flags =
		MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_MUST_CONNECT;

	/* set a default mbus format  */
	csi2->format_mbus.code =  MEDIA_BUS_FMT_UYVY8_2X8;
	csi2->format_mbus.field = V4L2_FIELD_NONE;
	csi2->format_mbus.width = RKCIF_DEFAULT_WIDTH;
	csi2->format_mbus.height = RKCIF_DEFAULT_HEIGHT;
	csi2->crop.top = 0;
	csi2->crop.left = 0;
	csi2->crop.width = RKCIF_DEFAULT_WIDTH;
	csi2->crop.height = RKCIF_DEFAULT_HEIGHT;
	csi2->mbus.bus.mipi_csi2.num_data_lanes = 4;

	return media_entity_pads_init(&sd->entity, num_pads, csi2->pad);
}

/* csi2 accepts all fmt/size from sensor */
static int csi2_get_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	int ret;
	struct csi2_dev *csi2 = sd_to_dev(sd);
	struct v4l2_subdev *sensor = get_remote_sensor(sd);

	/*
	 * Do not allow format changes and just relay whatever
	 * set currently in the sensor.
	 */
	ret = v4l2_subdev_call(sensor, pad, get_fmt, NULL, fmt);
	if (!ret)
		csi2->format_mbus = fmt->format;

	return ret;
}

static struct v4l2_rect *mipi_csi2_get_crop(struct csi2_dev *csi2,
						 struct v4l2_subdev_state *sd_state,
						 enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_crop(&csi2->sd, sd_state, RK_CSI2_PAD_SINK);
	else
		return &csi2->crop;
}

static int csi2_get_selection(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_selection *sel)
{
	struct csi2_dev *csi2 = sd_to_dev(sd);
	struct v4l2_subdev *sensor = get_remote_sensor(sd);
	struct v4l2_subdev_format fmt;
	int ret = 0;

	if (!sel) {
		v4l2_dbg(1, csi2_debug, &csi2->sd, "sel is null\n");
		goto err;
	}

	if (sel->pad > RK_CSI2X_PAD_SOURCE3) {
		v4l2_dbg(1, csi2_debug, &csi2->sd, "pad[%d] isn't matched\n", sel->pad);
		goto err;
	}

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
		if (sel->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
			sel->pad = 0;
			ret = v4l2_subdev_call(sensor, pad, get_selection,
					       sd_state, sel);
			if (ret) {
				fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
				fmt.pad = 0;
				ret = v4l2_subdev_call(sensor, pad, get_fmt, NULL, &fmt);
				if (!ret) {
					csi2->format_mbus = fmt.format;
					sel->r.top = 0;
					sel->r.left = 0;
					sel->r.width = csi2->format_mbus.width;
					sel->r.height = csi2->format_mbus.height;
					csi2->crop = sel->r;
				} else {
					sel->r = csi2->crop;
				}
			} else {
				csi2->crop = sel->r;
			}
		} else {
			sel->r = *v4l2_subdev_get_try_crop(&csi2->sd, sd_state, sel->pad);
		}
		break;

	case V4L2_SEL_TGT_CROP:
		sel->r = *mipi_csi2_get_crop(csi2, sd_state, sel->which);
		break;

	default:
		return -EINVAL;
	}

	return 0;
err:
	return -EINVAL;
}

static int csi2_set_selection(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_selection *sel)
{
	struct csi2_dev *csi2 = sd_to_dev(sd);
	struct v4l2_subdev *sensor = get_remote_sensor(sd);
	int ret = 0;

	ret = v4l2_subdev_call(sensor, pad, set_selection,
			       sd_state, sel);
	if (!ret)
		csi2->crop = sel->r;

	return ret;
}

static int csi2_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
			      struct v4l2_mbus_config *mbus)
{
	struct csi2_dev *csi2 = sd_to_dev(sd);
	struct v4l2_subdev *sensor_sd = get_remote_sensor(sd);
	int ret;

	ret = v4l2_subdev_call(sensor_sd, pad, get_mbus_config, 0, mbus);
	if (ret) {
		mbus->type = V4L2_MBUS_CSI2_DPHY;
		mbus->bus.mipi_csi2.flags = csi2->mbus.bus.mipi_csi2.flags;
		mbus->bus.mipi_csi2.flags |= BIT(csi2->mbus.bus.mipi_csi2.num_data_lanes - 1);
	}

	return 0;
}

static const struct media_entity_operations csi2_entity_ops = {
	.link_setup = csi2_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

void rkcif_csi2_event_reset_pipe(struct csi2_dev *csi2_dev, int reset_src)
{
	if (csi2_dev) {
		struct v4l2_event event = {
			.type = V4L2_EVENT_RESET_DEV,
			.reserved[0] = reset_src,
		};
		v4l2_event_queue(csi2_dev->sd.devnode, &event);
	}
}

void rkcif_csi2_event_inc_sof(struct csi2_dev *csi2_dev)
{
	if (csi2_dev) {
		struct v4l2_event event = {
			.type = V4L2_EVENT_FRAME_SYNC,
			.u.frame_sync.frame_sequence =
				atomic_inc_return(&csi2_dev->frm_sync_seq) - 1,
		};
		v4l2_event_queue(csi2_dev->sd.devnode, &event);
	}
}

u32 rkcif_csi2_get_sof(struct csi2_dev *csi2_dev)
{
	if (csi2_dev)
		return atomic_read(&csi2_dev->frm_sync_seq) - 1;

	return 0;
}

void rkcif_csi2_set_sof(struct csi2_dev *csi2_dev, u32 seq)
{
	if (csi2_dev)
		atomic_set(&csi2_dev->frm_sync_seq, seq);
}

static int rkcif_csi2_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
					     struct v4l2_event_subscription *sub)
{
	if (sub->type == V4L2_EVENT_FRAME_SYNC ||
	    sub->type == V4L2_EVENT_RESET_DEV)
		return v4l2_event_subscribe(fh, sub, RKCIF_V4L2_EVENT_ELEMS, NULL);
	else
		return -EINVAL;
}

static int rkcif_csi2_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static void csi2_quick_stream_on(struct csi2_dev *csi2)
{
	int csi_idx = 0;
	int i = 0;

	for (i = 0; i < csi2->csi_info.csi_num; i++) {
		csi_idx = csi2->csi_info.csi_idx[i];
		write_csihost_reg(csi2->csi2_hw[csi_idx]->base, CSIHOST_RESETN, 1);
	}
}

static void csi2_quick_stream_off(struct csi2_dev *csi2)
{
	int csi_idx = 0;
	int i = 0;

	for (i = 0; i < csi2->csi_info.csi_num; i++) {
		csi_idx = csi2->csi_info.csi_idx[i];
		write_csihost_reg(csi2->csi2_hw[csi_idx]->base, CSIHOST_RESETN, 0);
	}
}

static int csi2_get_error_info(struct csi2_dev *csi2,
			       struct rkmodule_error_info *err_info)
{
	int count = 0;

	memset(err_info, 0, sizeof(*err_info));

	count = snprintf(err_info->detail, sizeof(err_info->detail), "%s:", csi2->dev_name);

#define APPEND_STAT(field) \
	do {\
		ssize_t remaining = sizeof(err_info->detail) - count;\
		if (remaining > 0) { \
			int written = snprintf(err_info->detail + count, remaining, "%u,", csi2->err_list[field].cnt); \
			if (written >= 0) { \
				count += written; \
			} \
		} \
	} while (0)

	APPEND_STAT(RK_CSI2_ERR_SOTSYN);
	APPEND_STAT(RK_CSI2_ERR_FS_FE_MIS);
	APPEND_STAT(RK_CSI2_ERR_FRM_SEQ_ERR);
	APPEND_STAT(RK_CSI2_ERR_CRC_ONCE);
	APPEND_STAT(RK_CSI2_ERR_CRC);
	APPEND_STAT(RK_CSI2_ERR_ECC2);
	APPEND_STAT(RK_CSI2_ERR_CTRL);
	APPEND_STAT(RK_CSI2_ERR_ULPM);
	APPEND_STAT(RK_CSI2_ERR_SOT);
	APPEND_STAT(RK_CSI2_ERR_ECC);
	APPEND_STAT(RK_CSI2_ERR_ID);
	APPEND_STAT(RK_CSI2_ERR_CODE);

	err_info->err_code = csi2->err_list[RK_CSI2_ERR_ALL].cnt ? BIT(0) : 0;

	return 0;
}

static long rkcif_csi2_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct csi2_dev *csi2 = sd_to_dev(sd);
	struct v4l2_subdev *sensor = get_remote_sensor(sd);
	long ret = 0;
	int i = 0;
	struct rkmodule_error_info *err_info;

	switch (cmd) {
	case RKCIF_CMD_SET_CSI_IDX:
		csi2->csi_info = *((struct rkcif_csi_info *)arg);
		for (i = 0; i < csi2->csi_info.csi_num; i++)
			csi2->csi2_hw[csi2->csi_info.csi_idx[i]]->csi2 = csi2;
		if (csi2->match_data->chip_id > CHIP_RV1126_CSI2)
			ret = v4l2_subdev_call(sensor, core, ioctl,
					       RKCIF_CMD_SET_CSI_IDX,
						   arg);
		break;
	case RKCIF_CMD_SET_PPI_DATA_DEBUG:
		csi2->sw_dbg = *((u32 *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		if (*(int *)arg)
			csi2_quick_stream_on(csi2);
		else
			csi2_quick_stream_off(csi2);
		break;
	case RKMODULE_GET_ERROR_INFO:
		err_info = (struct rkmodule_error_info *)arg;
		ret = csi2_get_error_info(csi2, err_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long rkcif_csi2_compat_ioctl32(struct v4l2_subdev *sd,
				      unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkcif_csi_info csi_info;
	int sw_dbg = 0;
	long ret;
	struct rkmodule_error_info *err_info;

	switch (cmd) {
	case RKCIF_CMD_SET_CSI_IDX:
		if (copy_from_user(&csi_info, up, sizeof(struct rkcif_csi_info)))
			return -EFAULT;

		ret = rkcif_csi2_ioctl(sd, cmd, &csi_info);
		break;
	case RKCIF_CMD_SET_PPI_DATA_DEBUG:
		if (copy_from_user(&sw_dbg, up, sizeof(int)))
			return -EFAULT;

		ret = rkcif_csi2_ioctl(sd, cmd, &sw_dbg);
		break;
	case RKMODULE_GET_ERROR_INFO:
		err_info = kzalloc(sizeof(*err_info), GFP_KERNEL);
		if (!err_info) {
			ret = -ENOMEM;
			return ret;
		}

		ret = rkcif_csi2_ioctl(sd, cmd, err_info);
		if (!ret) {
			if (copy_to_user(up, err_info, sizeof(*err_info))) {
				kfree(err_info);
				return -EFAULT;
			}
		}
		kfree(err_info);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static const struct v4l2_subdev_core_ops csi2_core_ops = {
	.subscribe_event = rkcif_csi2_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
	.s_power = rkcif_csi2_s_power,
	.ioctl = rkcif_csi2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = rkcif_csi2_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops csi2_video_ops = {
	.s_stream = csi2_s_stream,
};

static const struct v4l2_subdev_pad_ops csi2_pad_ops = {
	.get_fmt = csi2_get_set_fmt,
	.set_fmt = csi2_get_set_fmt,
	.get_selection = csi2_get_selection,
	.set_selection = csi2_set_selection,
	.get_mbus_config = csi2_g_mbus_config,
};

static const struct v4l2_subdev_ops csi2_subdev_ops = {
	.core = &csi2_core_ops,
	.video = &csi2_video_ops,
	.pad = &csi2_pad_ops,
};

/* The .bound() notifier callback when a match is found */
static int
csi2_notifier_bound(struct v4l2_async_notifier *notifier,
		    struct v4l2_subdev *sd,
		    struct v4l2_async_subdev *asd)
{
	struct csi2_dev *csi2 = container_of(notifier,
			struct csi2_dev,
			notifier);
	struct csi2_sensor_info *sensor;
	struct media_link *link;
	unsigned int pad, ret;

	if (csi2->num_sensors == ARRAY_SIZE(csi2->sensors)) {
		v4l2_err(&csi2->sd,
			 "%s: the num of sd is beyond:%d\n",
			 __func__, csi2->num_sensors);
		return -EBUSY;
	}
	sensor = &csi2->sensors[csi2->num_sensors++];
	sensor->sd = sd;

	for (pad = 0; pad < sd->entity.num_pads; pad++)
		if (sensor->sd->entity.pads[pad].flags
					& MEDIA_PAD_FL_SOURCE)
			break;

	if (pad == sensor->sd->entity.num_pads) {
		dev_err(csi2->dev,
			"failed to find src pad for %s\n",
			sd->name);

		return -ENXIO;
	}

	ret = media_create_pad_link(&sensor->sd->entity, pad,
				    &csi2->sd.entity, RK_CSI2_PAD_SINK,
				    0/* csi2->num_sensors != 1 ? 0 : MEDIA_LNK_FL_ENABLED */);
	if (ret) {
		dev_err(csi2->dev,
			"failed to create link for %s\n",
			sd->name);
		return ret;
	}

	link = list_first_entry(&csi2->sd.entity.links, struct media_link, list);
	ret = media_entity_setup_link(link, MEDIA_LNK_FL_ENABLED);
	if (ret) {
		dev_err(csi2->dev,
			"failed to create link for %s\n",
			sensor->sd->name);
		return ret;
	}

	return 0;
}

/* The .unbind callback */
static void csi2_notifier_unbind(struct v4l2_async_notifier *notifier,
				 struct v4l2_subdev *sd,
				 struct v4l2_async_subdev *asd)
{
	struct csi2_dev *csi2 = container_of(notifier,
						  struct csi2_dev,
						  notifier);
	struct csi2_sensor_info *sensor = sd_to_sensor(csi2, sd);

	if (sensor)
		sensor->sd = NULL;
}

static const struct
v4l2_async_notifier_operations csi2_async_ops = {
	.bound = csi2_notifier_bound,
	.unbind = csi2_notifier_unbind,
};

static void csi2_find_err_vc(int val, char *vc_info)
{
	int i;
	char cur_str[CSI_VCINFO_LEN] = {0};

	memset(vc_info, 0, sizeof(*vc_info));
	for (i = 0; i < 4; i++) {
		if ((val >> i) & 0x1) {
			snprintf(cur_str, CSI_VCINFO_LEN, " %d", i);
			if (strlen(vc_info) + strlen(cur_str) < CSI_VCINFO_LEN)
				strncat(vc_info, cur_str, strlen(cur_str));
		}
	}
}

#define csi2_err_strncat(dst_str, src_str) {\
	if (strlen(dst_str) + strlen(src_str) < CSI_ERRSTR_LEN)\
		strncat(dst_str, src_str, strlen(src_str)); }

static irqreturn_t rk_csirx_irq1_handler(int irq, void *ctx)
{
	struct device *dev = ctx;
	struct csi2_hw *csi2_hw = dev_get_drvdata(dev);
	struct csi2_dev *csi2 = NULL;
	struct csi2_err_stats *err_list = NULL;
	unsigned long err_stat = 0;
	u32 val;
	char err_str[CSI_ERRSTR_LEN] = {0};
	char cur_str[CSI_ERRSTR_LEN] = {0};
	char vc_info[CSI_VCINFO_LEN] = {0};
	bool is_add_cnt = false;
	u64 cur_timestamp = ktime_get_ns();

	if (!csi2_hw) {
		disable_irq_nosync(irq);
		return IRQ_HANDLED;
	}

	csi2 = csi2_hw->csi2;
	if (!csi2) {
		disable_irq_nosync(irq);
		return IRQ_HANDLED;
	}
	val = read_csihost_reg(csi2_hw->base, CSIHOST_ERR1);
	if (val) {
		if (val & CSIHOST_ERR1_PHYERR_SPTSYNCHS) {
			err_list = &csi2->err_list[RK_CSI2_ERR_SOTSYN];
			err_list->cnt++;
			if (csi2->match_data->chip_id == CHIP_RK3588_CSI2) {
				if (err_list->cnt > 3 &&
				    csi2->err_list[RK_CSI2_ERR_ALL].cnt <= err_list->cnt) {
					csi2->is_check_sot_sync = false;
					write_csihost_reg(csi2_hw->base, CSIHOST_MSK1, 0xf);
				}
				if (csi2->is_check_sot_sync) {
					csi2_find_err_vc(val & 0xf, vc_info);
					snprintf(cur_str, CSI_ERRSTR_LEN, "(sot sync,lane:%s) ", vc_info);
					csi2_err_strncat(err_str, cur_str);
				}
			} else {
				csi2_find_err_vc(val & 0xf, vc_info);
				snprintf(cur_str, CSI_ERRSTR_LEN, "(sot sync,lane:%s) ", vc_info);
				csi2_err_strncat(err_str, cur_str);
				is_add_cnt = true;
			}
		}

		if (val & CSIHOST_ERR1_ERR_BNDRY_MATCH && csi2->is_detect_fs_fe) {
			err_list = &csi2->err_list[RK_CSI2_ERR_FS_FE_MIS];
			err_list->cnt++;
			csi2_find_err_vc((val >> 4) & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(fs/fe mis,vc:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
			if (csi2->match_data->chip_id < CHIP_RK3588_CSI2)
				is_add_cnt = true;
		}

		if (val & CSIHOST_ERR1_ERR_SEQ) {
			err_list = &csi2->err_list[RK_CSI2_ERR_FRM_SEQ_ERR];
			err_list->cnt++;
			csi2_find_err_vc((val >> 8) & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(f_seq,vc:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
		}

		if (val & CSIHOST_ERR1_ERR_FRM_DATA) {
			err_list = &csi2->err_list[RK_CSI2_ERR_CRC_ONCE];
			is_add_cnt = true;
			err_list->cnt++;
			csi2_find_err_vc((val >> 12) & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(err_data,vc:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
		}

		if (val & CSIHOST_ERR1_ERR_CRC) {
			err_list = &csi2->err_list[RK_CSI2_ERR_CRC];
			err_list->cnt++;
			is_add_cnt = true;
			csi2_find_err_vc((val >> 24) & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(crc,vc:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
		}

		if (val & CSIHOST_ERR1_ERR_ECC2) {
			err_list = &csi2->err_list[RK_CSI2_ERR_ECC2];
			err_list->cnt++;
			is_add_cnt = true;
			snprintf(cur_str, CSI_ERRSTR_LEN, "(ecc2) ");
			csi2_err_strncat(err_str, cur_str);
		}

		if (val & CSIHOST_ERR1_ERR_CTRL) {
			err_list = &csi2->err_list[RK_CSI2_ERR_CTRL];
			err_list->cnt++;
			csi2_find_err_vc((val >> 16) & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(ctrl,vc:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
		}

		if (csi2->irq1_timestamp == 0 || cur_timestamp - csi2->irq1_timestamp > 1000000000) {
			csi2->irq1_timestamp = cur_timestamp;
			pr_err("(0x%x)MIPI_CSI2 ERR1:0x%x %s\n", (u32)csi2_hw->res->start, val, err_str);
		}

		if (is_add_cnt) {
			csi2->err_list[RK_CSI2_ERR_ALL].cnt++;
			err_stat = ((csi2->err_list[RK_CSI2_ERR_FS_FE_MIS].cnt & 0xff) << 8) |
				    ((csi2->err_list[RK_CSI2_ERR_ALL].cnt) & 0xff);

			atomic_notifier_call_chain(&g_csi_host_chain,
						   err_stat,
						   &csi2->csi_info.csi_idx[csi2->csi_info.csi_num - 1]);
		}

	}

	return IRQ_HANDLED;
}

static irqreturn_t rk_csirx_irq2_handler(int irq, void *ctx)
{
	struct device *dev = ctx;
	struct csi2_hw *csi2_hw = dev_get_drvdata(dev);
	struct csi2_dev *csi2 = NULL;
	u32 val;
	char cur_str[CSI_ERRSTR_LEN] = {0};
	char err_str[CSI_ERRSTR_LEN] = {0};
	char vc_info[CSI_VCINFO_LEN] = {0};
	u64 cur_timestamp = ktime_get_ns();
	struct csi2_err_stats *err_list = NULL;

	if (!csi2_hw) {
		disable_irq_nosync(irq);
		return IRQ_HANDLED;
	}

	csi2 = csi2_hw->csi2;
	if (!csi2) {
		disable_irq_nosync(irq);
		return IRQ_HANDLED;
	}

	val = read_csihost_reg(csi2_hw->base, CSIHOST_ERR2);
	if (val) {
		if (val & CSIHOST_ERR2_PHYERR_ESC) {
			err_list = &csi2->err_list[RK_CSI2_ERR_ULPM];
			err_list->cnt++;
			csi2_find_err_vc(val & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(ULPM,lane:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
		}
		if (val & CSIHOST_ERR2_PHYERR_SOTHS) {
			err_list = &csi2->err_list[RK_CSI2_ERR_SOT];
			err_list->cnt++;
			csi2_find_err_vc((val >> 4) & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(sot,lane:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
		}
		if (val & CSIHOST_ERR2_ECC_CORRECTED) {
			err_list = &csi2->err_list[RK_CSI2_ERR_ECC];
			err_list->cnt++;
			csi2_find_err_vc((val >> 8) & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(ecc,vc:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
		}
		if (val & CSIHOST_ERR2_ERR_ID) {
			err_list = &csi2->err_list[RK_CSI2_ERR_ID];
			err_list->cnt++;
			csi2_find_err_vc((val >> 12) & 0xf, vc_info);
			snprintf(cur_str, CSI_ERRSTR_LEN, "(err id,vc:%s) ", vc_info);
			csi2_err_strncat(err_str, cur_str);
		}
		if (val & CSIHOST_ERR2_PHYERR_CODEHS) {
			err_list = &csi2->err_list[RK_CSI2_ERR_CODE];
			err_list->cnt++;
			snprintf(cur_str, CSI_ERRSTR_LEN, "(err code) ");
			csi2_err_strncat(err_str, cur_str);
		}

		if (csi2->irq2_timestamp == 0 || cur_timestamp - csi2->irq2_timestamp > 1000000000) {
			csi2->irq2_timestamp = cur_timestamp;
			pr_err("(0x%x)MIPI_CSI2 ERR2:0x%x %s\n", (u32)csi2_hw->res->start, val, err_str);
		}
	}

	return IRQ_HANDLED;
}

/* Parse fwnode with port0, if an empty function is used, each node will parse
 * all ports, causing the device to repeatedly join the link and unable to
 * complete the link
 */
static int csi2_fwnode_parse(struct csi2_dev *csi2)
{
	struct device *dev = csi2->dev;
	struct fwnode_handle *ep = NULL;
	struct v4l2_async_subdev *s_asd = NULL;
	struct fwnode_handle *remote_ep = NULL;
	struct v4l2_fwnode_endpoint vep = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret = 0;

	fwnode_graph_for_each_endpoint(dev_fwnode(dev), ep) {
		ret = v4l2_fwnode_endpoint_parse(ep, &vep);
		if (ret)
			goto err_parse;

		/* only add fwnode form port 0 to notifier list */
		if (vep.base.port != 0)
			continue;

		remote_ep = fwnode_graph_get_remote_port_parent(ep);
		/* skip device dts status is disabled */
		if (!fwnode_device_is_available(remote_ep)) {
			fwnode_handle_put(remote_ep);
			continue;
		}

		s_asd = v4l2_async_nf_add_fwnode(&csi2->notifier, remote_ep,
						 struct
						 v4l2_async_subdev);
		fwnode_handle_put(remote_ep);
		if (IS_ERR(s_asd)) {
			ret = PTR_ERR(s_asd);
			goto err_parse;
		}
	}
	return 0;

err_parse:
	fwnode_handle_put(ep);
	return ret;
}

static int csi2_notifier(struct csi2_dev *csi2)
{
	struct v4l2_async_notifier *ntf = &csi2->notifier;
	int ret;

	v4l2_async_nf_init(ntf);

	ret = csi2_fwnode_parse(csi2);
	if (ret < 0)
		return ret;

	csi2->sd.subdev_notifier = &csi2->notifier;
	csi2->notifier.ops = &csi2_async_ops;
	ret = v4l2_async_subdev_nf_register(&csi2->sd, &csi2->notifier);
	if (ret) {
		v4l2_err(&csi2->sd,
			 "failed to register async notifier : %d\n",
			 ret);
		v4l2_async_nf_cleanup(&csi2->notifier);
		return ret;
	}

	ret = v4l2_async_register_subdev(&csi2->sd);

	return ret;
}

static const struct csi2_match_data rk1808_csi2_match_data = {
	.chip_id = CHIP_RK1808_CSI2,
	.num_pads = CSI2_NUM_PADS,
	.num_hw = 1,
};

static const struct csi2_match_data rk3288_csi2_match_data = {
	.chip_id = CHIP_RK3288_CSI2,
	.num_pads = CSI2_NUM_PADS_SINGLE_LINK,
	.num_hw = 1,
};

static const struct csi2_match_data rv1126_csi2_match_data = {
	.chip_id = CHIP_RV1126_CSI2,
	.num_pads = CSI2_NUM_PADS,
	.num_hw = 1,
};

static const struct csi2_match_data rk3568_csi2_match_data = {
	.chip_id = CHIP_RK3568_CSI2,
	.num_pads = CSI2_NUM_PADS,
	.num_hw = 1,
};

static const struct csi2_match_data rk3588_csi2_match_data = {
	.chip_id = CHIP_RK3588_CSI2,
	.num_pads = CSI2_NUM_PADS_MAX,
	.num_hw = 6,
};

static const struct csi2_match_data rv1106_csi2_match_data = {
	.chip_id = CHIP_RV1106_CSI2,
	.num_pads = CSI2_NUM_PADS_MAX,
	.num_hw = 2,
};

static const struct csi2_match_data rk3562_csi2_match_data = {
	.chip_id = CHIP_RK3562_CSI2,
	.num_pads = CSI2_NUM_PADS_MAX,
	.num_hw = 4,
};

static const struct csi2_match_data rk3576_csi2_match_data = {
	.chip_id = CHIP_RK3576_CSI2,
	.num_pads = CSI2_NUM_PADS_MAX,
	.num_hw = 5,
};

static const struct csi2_match_data rv1103b_csi2_match_data = {
	.chip_id = CHIP_RV1103B_CSI2,
	.num_pads = CSI2_NUM_PADS_MAX,
	.num_hw = 2,
};

static const struct csi2_match_data rv1126b_csi2_match_data = {
	.chip_id = CHIP_RV1126B_CSI2,
	.num_pads = CSI2_NUM_PADS_MAX,
	.num_hw = 4,
};

static const struct of_device_id csi2_dt_ids[] = {
#ifdef CONFIG_CPU_RK1808
	{
		.compatible = "rockchip,rk1808-mipi-csi2",
		.data = &rk1808_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3288
	{
		.compatible = "rockchip,rk3288-mipi-csi2",
		.data = &rk3288_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3568
	{
		.compatible = "rockchip,rk3568-mipi-csi2",
		.data = &rk3568_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RV1126
	{
		.compatible = "rockchip,rv1126-mipi-csi2",
		.data = &rv1126_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3588
	{
		.compatible = "rockchip,rk3588-mipi-csi2",
		.data = &rk3588_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RV1106
	{
		.compatible = "rockchip,rv1106-mipi-csi2",
		.data = &rv1106_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3562
	{
		.compatible = "rockchip,rk3562-mipi-csi2",
		.data = &rk3562_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3576
	{
		.compatible = "rockchip,rk3576-mipi-csi2",
		.data = &rk3576_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RV1103B
	{
		.compatible = "rockchip,rv1103b-mipi-csi2",
		.data = &rv1103b_csi2_match_data,
	},
#endif
#ifdef CONFIG_CPU_RV1126B
	{
		.compatible = "rockchip,rv1126b-mipi-csi2",
		.data = &rv1126b_csi2_match_data,
	},
#endif
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, csi2_dt_ids);

static int csi2_attach_hw(struct csi2_dev *csi2)
{
	struct device_node *np;
	struct platform_device *pdev;
	struct csi2_hw *hw;
	int i = 0;

	for (i = 0; i < csi2->match_data->num_hw; i++) {
		np = of_parse_phandle(csi2->dev->of_node, "rockchip,hw", i);
		if (!np || !of_device_is_available(np)) {
			dev_err(csi2->dev, "failed to get csi2 hw node\n");
			return -ENODEV;
		}

		pdev = of_find_device_by_node(np);
		of_node_put(np);
		if (!pdev) {
			dev_err(csi2->dev, "failed to get csi2 hw from node\n");
			return -ENODEV;
		}

		hw = platform_get_drvdata(pdev);
		if (!hw) {
			dev_err(csi2->dev, "failed attach csi2 hw\n");
			return -EINVAL;
		}

		hw->csi2 = csi2;
		csi2->csi2_hw[i] = hw;
	}
	dev_info(csi2->dev, "attach to csi2 hw node\n");

	return 0;
}

static int csi2_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device_node *node = pdev->dev.of_node;
	struct csi2_dev *csi2 = NULL;
	const struct csi2_match_data *data;
	int ret;

	match = of_match_node(csi2_dt_ids, node);
	if (IS_ERR(match))
		return PTR_ERR(match);
	data = match->data;

	csi2 = devm_kzalloc(&pdev->dev, sizeof(*csi2), GFP_KERNEL);
	if (!csi2)
		return -ENOMEM;

	csi2->dev = &pdev->dev;
	csi2->match_data = data;

	csi2->dev_name = node->name;
	v4l2_subdev_init(&csi2->sd, &csi2_subdev_ops);
	v4l2_set_subdevdata(&csi2->sd, &pdev->dev);
	csi2->sd.entity.ops = &csi2_entity_ops;
	csi2->sd.dev = &pdev->dev;
	csi2->sd.owner = THIS_MODULE;
	csi2->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	ret = strscpy(csi2->sd.name, DEVICE_NAME, sizeof(csi2->sd.name));
	if (ret < 0)
		v4l2_err(&csi2->sd, "failed to copy name\n");
	platform_set_drvdata(pdev, &csi2->sd);

	ret = csi2_attach_hw(csi2);
	if (ret) {
		v4l2_err(&csi2->sd, "must enable all mipi csi2 hw node\n");
		return -EINVAL;
	}
	mutex_init(&csi2->lock);

	ret = csi2_media_init(&csi2->sd);
	if (ret < 0)
		goto rmmutex;
	ret = csi2_notifier(csi2);
	if (ret)
		goto rmmutex;

	v4l2_info(&csi2->sd, "probe success, v4l2_dev:%s!\n", csi2->sd.v4l2_dev->name);

	return 0;

rmmutex:
	mutex_destroy(&csi2->lock);
	return ret;
}

static int csi2_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct csi2_dev *csi2 = sd_to_dev(sd);

	v4l2_async_unregister_subdev(sd);
	mutex_destroy(&csi2->lock);
	media_entity_cleanup(&sd->entity);

	return 0;
}

static struct platform_driver csi2_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.of_match_table = csi2_dt_ids,
	},
	.probe = csi2_probe,
	.remove = csi2_remove,
};

int rkcif_csi2_plat_drv_init(void)
{
	return platform_driver_register(&csi2_driver);
}

void rkcif_csi2_plat_drv_exit(void)
{
	platform_driver_unregister(&csi2_driver);
}

static const struct csi2_hw_match_data rk1808_csi2_hw_match_data = {
	.chip_id = CHIP_RK1808_CSI2,
};

static const struct csi2_hw_match_data rk3288_csi2_hw_match_data = {
	.chip_id = CHIP_RK3288_CSI2,
};

static const struct csi2_hw_match_data rv1126_csi2_hw_match_data = {
	.chip_id = CHIP_RV1126_CSI2,
};

static const struct csi2_hw_match_data rk3568_csi2_hw_match_data = {
	.chip_id = CHIP_RK3568_CSI2,
};

static const struct csi2_hw_match_data rk3588_csi2_hw_match_data = {
	.chip_id = CHIP_RK3588_CSI2,
};

static const struct csi2_hw_match_data rv1106_csi2_hw_match_data = {
	.chip_id = CHIP_RV1106_CSI2,
};

static const struct csi2_hw_match_data rk3562_csi2_hw_match_data = {
	.chip_id = CHIP_RK3562_CSI2,
};

static const struct csi2_hw_match_data rk3576_csi2_hw_match_data = {
	.chip_id = CHIP_RK3576_CSI2,
};

static const struct csi2_hw_match_data rv1103b_csi2_hw_match_data = {
	.chip_id = CHIP_RV1103B_CSI2,
};

static const struct csi2_hw_match_data rv1126b_csi2_hw_match_data = {
	.chip_id = CHIP_RV1126B_CSI2,
};

static const struct of_device_id csi2_hw_ids[] = {
#ifdef CONFIG_CPU_RK1808
	{
		.compatible = "rockchip,rk1808-mipi-csi2-hw",
		.data = &rk1808_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3288
	{
		.compatible = "rockchip,rk3288-mipi-csi2-hw",
		.data = &rk3288_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3568
	{
		.compatible = "rockchip,rk3568-mipi-csi2-hw",
		.data = &rk3568_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RV1126
	{
		.compatible = "rockchip,rv1126-mipi-csi2-hw",
		.data = &rv1126_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3588
	{
		.compatible = "rockchip,rk3588-mipi-csi2-hw",
		.data = &rk3588_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RV1106
	{
		.compatible = "rockchip,rv1106-mipi-csi2-hw",
		.data = &rv1106_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3562
	{
		.compatible = "rockchip,rk3562-mipi-csi2-hw",
		.data = &rk3562_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RK3576
	{
		.compatible = "rockchip,rk3576-mipi-csi2-hw",
		.data = &rk3576_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RV1103B
	{
		.compatible = "rockchip,rv1103b-mipi-csi2-hw",
		.data = &rv1103b_csi2_hw_match_data,
	},
#endif
#ifdef CONFIG_CPU_RV1126B
	{
		.compatible = "rockchip,rv1126b-mipi-csi2-hw",
		.data = &rv1126b_csi2_hw_match_data,
	},
#endif
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, csi2_hw_ids);

static int csi2_hw_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct csi2_hw *csi2_hw = NULL;
	struct resource *res;
	const struct csi2_hw_match_data *data;
	int ret, irq;

	dev_info(&pdev->dev, "enter mipi csi2 hw probe!\n");
	match = of_match_node(csi2_hw_ids, node);
	if (IS_ERR(match))
		return PTR_ERR(match);
	data = match->data;

	csi2_hw = devm_kzalloc(&pdev->dev, sizeof(*csi2_hw), GFP_KERNEL);
	if (!csi2_hw)
		return -ENOMEM;

	csi2_hw->dev = &pdev->dev;
	csi2_hw->match_data = data;

	csi2_hw->dev_name = node->name;

	csi2_hw->clks_num = devm_clk_bulk_get_all(dev, &csi2_hw->clks_bulk);
	if (csi2_hw->clks_num < 0) {
		csi2_hw->clks_num = 0;
		dev_err(dev, "failed to get csi2 clks\n");
	}

	csi2_hw->rsts_bulk = devm_reset_control_array_get_optional_exclusive(dev);
	if (IS_ERR(csi2_hw->rsts_bulk)) {
		if (PTR_ERR(csi2_hw->rsts_bulk) != -EPROBE_DEFER)
			dev_err(dev, "failed to get csi2 reset\n");
		csi2_hw->rsts_bulk = NULL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	csi2_hw->base = devm_ioremap_resource(&pdev->dev, res);
	csi2_hw->res = res;
	if (IS_ERR(csi2_hw->base)) {
		resource_size_t offset = res->start;
		resource_size_t size = resource_size(res);

		dev_warn(&pdev->dev, "avoid secondary mipi resource check!\n");

		csi2_hw->base = devm_ioremap(&pdev->dev, offset, size);
		if (IS_ERR(csi2_hw->base)) {
			dev_err(&pdev->dev, "Failed to ioremap resource\n");

			return PTR_ERR(csi2_hw->base);
		}
	}

	irq = platform_get_irq_byname(pdev, "csi-intr1");
	if (irq > 0) {
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_irq(&pdev->dev, irq,
				       rk_csirx_irq1_handler, 0,
				       dev_driver_string(&pdev->dev),
				       &pdev->dev);
		if (ret < 0)
			dev_err(&pdev->dev, "request csi-intr1 irq failed: %d\n",
				 ret);
		csi2_hw->irq1 = irq;
	} else {
		dev_err(&pdev->dev, "No found irq csi-intr1\n");
	}

	irq = platform_get_irq_byname(pdev, "csi-intr2");
	if (irq > 0) {
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		ret = devm_request_irq(&pdev->dev, irq,
				       rk_csirx_irq2_handler, 0,
				       dev_driver_string(&pdev->dev),
				       &pdev->dev);
		if (ret < 0)
			dev_err(&pdev->dev, "request csi-intr2 failed: %d\n",
				 ret);
		csi2_hw->irq2 = irq;
	} else {
		dev_err(&pdev->dev, "No found irq csi-intr2\n");
	}
	platform_set_drvdata(pdev, csi2_hw);
	atomic_set(&csi2_hw->stream_count, 0);
	dev_info(&pdev->dev, "probe success, v4l2_dev:%s!\n", csi2_hw->dev_name);

	return 0;
}

static int csi2_hw_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver csi2_hw_driver = {
	.driver = {
		.name = DEVICE_NAME_HW,
		.of_match_table = csi2_hw_ids,
	},
	.probe = csi2_hw_probe,
	.remove = csi2_hw_remove,
};

int rkcif_csi2_hw_plat_drv_init(void)
{
	return platform_driver_register(&csi2_hw_driver);
}

void rkcif_csi2_hw_plat_drv_exit(void)
{
	platform_driver_unregister(&csi2_hw_driver);
}

MODULE_DESCRIPTION("Rockchip MIPI CSI2 driver");
MODULE_AUTHOR("Macrofly.xu <xuhf@rock-chips.com>");
MODULE_LICENSE("GPL");
