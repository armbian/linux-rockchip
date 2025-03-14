// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:Mark Yao <mark.yao@rock-chips.com>
 */

#include <linux/kernel.h>
#include <linux/devfreq.h>

#include <drm/drm.h>
#include <drm/drm_atomic.h>
#include <drm/drm_damage_helper.h>
#include <drm/display/drm_dp_mst_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_probe_helper.h>
#include <soc/rockchip/rockchip_dmc.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_fb.h"
#include "rockchip_drm_gem.h"
#include "rockchip_drm_logo.h"

static bool is_rockchip_logo_fb(struct drm_framebuffer *fb)
{
	return fb->flags & ROCKCHIP_DRM_MODE_LOGO_FB ? true : false;
}

static void __rockchip_drm_fb_destroy(struct drm_framebuffer *fb)
{
	int i = 0;

	drm_framebuffer_cleanup(fb);

	if (is_rockchip_logo_fb(fb)) {
		struct rockchip_drm_logo_fb *rockchip_logo_fb = to_rockchip_logo_fb(fb);

#ifndef MODULE
		rockchip_free_loader_memory(fb->dev);
#endif
		drm_gem_object_release(rockchip_logo_fb->fb.obj[0]);
		kfree(rockchip_logo_fb);
	} else {
		for (i = 0; i < 4; i++) {
			if (fb->obj[i])
				drm_gem_object_put(fb->obj[i]);
		}

		kfree(fb);
	}
}

static void rockchip_drm_fb_destroy_work(struct work_struct *work)
{
	struct rockchip_drm_logo_fb *fb;

	fb = container_of(to_delayed_work(work), struct rockchip_drm_logo_fb, destroy_work);

	__rockchip_drm_fb_destroy(&fb->fb);
}

static void rockchip_drm_fb_destroy(struct drm_framebuffer *fb)
{

	if (is_rockchip_logo_fb(fb)) {
		struct rockchip_drm_logo_fb *rockchip_logo_fb = to_rockchip_logo_fb(fb);

		schedule_delayed_work(&rockchip_logo_fb->destroy_work, HZ);
	} else {
		__rockchip_drm_fb_destroy(fb);
	}
}

static int rockchip_drm_gem_fb_create_handle(struct drm_framebuffer *fb,
					     struct drm_file *file,
					     unsigned int *handle)
{
	if (is_rockchip_logo_fb(fb))
		return -EOPNOTSUPP;

	return drm_gem_fb_create_handle(fb, file, handle);
}

static const struct drm_framebuffer_funcs rockchip_drm_fb_funcs = {
	.destroy       = rockchip_drm_fb_destroy,
	.create_handle = rockchip_drm_gem_fb_create_handle,
};

struct drm_framebuffer *
rockchip_fb_alloc(struct drm_device *dev, const struct drm_mode_fb_cmd2 *mode_cmd,
		  struct drm_gem_object **obj, unsigned int num_planes)
{
	struct drm_framebuffer *fb;
	int ret;
	int i;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(dev, fb, mode_cmd);

	for (i = 0; i < num_planes; i++)
		fb->obj[i] = obj[i];

	ret = drm_framebuffer_init(dev, fb, &rockchip_drm_fb_funcs);
	if (ret) {
		DRM_DEV_ERROR(dev->dev,
			      "Failed to initialize framebuffer: %d\n",
			      ret);
		kfree(fb);
		return ERR_PTR(ret);
	}

	return fb;
}

struct drm_framebuffer *
rockchip_drm_logo_fb_alloc(struct drm_device *dev, const struct drm_mode_fb_cmd2 *mode_cmd,
			   struct rockchip_logo *logo)
{
	int ret = 0;
	struct rockchip_drm_logo_fb *rockchip_logo_fb;
	struct drm_framebuffer *fb;

	rockchip_logo_fb = kzalloc(sizeof(*rockchip_logo_fb), GFP_KERNEL);
	if (!rockchip_logo_fb)
		return ERR_PTR(-ENOMEM);
	fb = &rockchip_logo_fb->fb;

	drm_helper_mode_fill_fb_struct(dev, fb, mode_cmd);

	ret = drm_framebuffer_init(dev, fb, &rockchip_drm_fb_funcs);
	if (ret) {
		DRM_DEV_ERROR(dev->dev,
			      "Failed to initialize rockchip logo fb: %d\n",
			      ret);
		kfree(rockchip_logo_fb);
		return ERR_PTR(ret);
	}

	fb->flags |= ROCKCHIP_DRM_MODE_LOGO_FB;
	rockchip_logo_fb->logo = logo;
	rockchip_logo_fb->fb.obj[0] = &rockchip_logo_fb->rk_obj.base;
	rockchip_logo_fb->fb.obj[0]->funcs = &rockchip_gem_object_funcs;
	drm_gem_object_init(dev, rockchip_logo_fb->fb.obj[0], PAGE_ALIGN(logo->size));
	rockchip_logo_fb->rk_obj.dma_addr = logo->dma_addr;
	rockchip_logo_fb->rk_obj.kvaddr = logo->kvaddr;
	logo->count++;
	INIT_DELAYED_WORK(&rockchip_logo_fb->destroy_work, rockchip_drm_fb_destroy_work);
	return &rockchip_logo_fb->fb;
}

static int rockchip_drm_bandwidth_atomic_check(struct drm_device *dev,
					       struct drm_atomic_state *state,
					       struct dmcfreq_vop_info *vop_bw_info)
{
	struct rockchip_drm_private *priv = dev->dev_private;
	struct drm_crtc_state *old_crtc_state;
	const struct rockchip_crtc_funcs *funcs;
	struct drm_crtc *crtc;
	int i;

	vop_bw_info->line_bw_mbyte = 0;
	vop_bw_info->frame_bw_mbyte = 0;
	vop_bw_info->plane_num = 0;
	vop_bw_info->plane_num_4k = 0;

	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
		funcs = priv->crtc_funcs[drm_crtc_index(crtc)];

		if (funcs && funcs->bandwidth)
			funcs->bandwidth(crtc, old_crtc_state, vop_bw_info);
	}

	return 0;
}

static int rockchip_drm_aclk_adjust(struct drm_device *dev,
				    struct drm_atomic_state *state,
				    struct dmcfreq_vop_info *vop_bw_info)
{
	struct rockchip_drm_private *priv = dev->dev_private;
	const struct rockchip_crtc_funcs *funcs;
	struct drm_crtc *crtc;
	int crtc_num = 0;

	drm_for_each_crtc(crtc, dev) {
		if (!crtc->state->active)
			continue;
		crtc_num++;
	}

	drm_for_each_crtc(crtc, dev) {
		if (!crtc->state->active)
			continue;

		funcs = priv->crtc_funcs[drm_crtc_index(crtc)];
		if (funcs && funcs->set_aclk) {
			struct drm_display_mode *mode = &crtc->state->adjusted_mode;
			int linedur_ns = div_u64((u64) mode->crtc_htotal * 1000000, mode->crtc_clock);

			if (vop_bw_info->plane_num_4k || crtc_num > 1 ||
			    crtc->state->adjusted_mode.crtc_hdisplay > 2560 ||
			    crtc->state->adjusted_mode.crtc_vdisplay > 2560 ||
			    linedur_ns < 7500) {/* 4kp60 linedur_ns roughly equal to 7500 ns */
				funcs->set_aclk(crtc, ROCKCHIP_VOP_ACLK_ADVANCED_MODE, vop_bw_info);
				priv->aclk_adjust_frame_num = 2;
			} else {
				if (priv->aclk_adjust_frame_num >= 1) {
					funcs->set_aclk(crtc, ROCKCHIP_VOP_ACLK_ADVANCED_MODE, vop_bw_info);
					priv->aclk_adjust_frame_num--;
				} else {
					funcs->set_aclk(crtc, ROCKCHIP_VOP_ACLK_NORMAL_MODE, vop_bw_info);
				}
			}
		}
	}

	return 0;
}

static void
rockchip_drm_atomic_helper_connector_commit(struct drm_device *dev,
					    struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, connector, new_conn_state, i) {
		const struct drm_connector_helper_funcs *funcs;

		funcs = connector->helper_private;
		if (!funcs->atomic_commit)
			continue;

		funcs->atomic_commit(connector, old_state);
	}
}

static int rockchip_drm_atomic_helper_get_crc(struct drm_device *dev,
					      struct drm_atomic_state *state)
{
	struct rockchip_drm_private *priv = dev->dev_private;
	const struct rockchip_crtc_funcs *funcs;
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, dev) {
		if (!crtc->state->active || !crtc->crc.opened)
			continue;

		funcs = priv->crtc_funcs[drm_crtc_index(crtc)];
		if (funcs && funcs->get_crc)
			funcs->get_crc(crtc);
	}

	return 0;
}

/**
 * rockchip_drm_atomic_helper_commit_tail_rpm - commit atomic update to hardware
 * @old_state: new modeset state to be committed
 *
 * This is an alternative implementation for the
 * &drm_mode_config_helper_funcs.atomic_commit_tail hook, for drivers
 * that support runtime_pm or need the CRTC to be enabled to perform a
 * commit. Otherwise, one should use the default implementation
 * drm_atomic_helper_commit_tail().
 */
static void rockchip_drm_atomic_helper_commit_tail_rpm(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;
	struct rockchip_drm_private *prv = dev->dev_private;
	struct dmcfreq_vop_info vop_bw_info;

#ifdef CONFIG_DRM_DISPLAY_DP_HELPER
	drm_dp_mst_atomic_wait_for_dependencies(old_state);
#endif

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	rockchip_drm_bandwidth_atomic_check(dev, old_state, &vop_bw_info);

	rockchip_drm_aclk_adjust(dev, old_state, &vop_bw_info);

	rockchip_dmcfreq_vop_bandwidth_update(&vop_bw_info);

	if (prv->need_ovl_lock)
		mutex_lock(&prv->ovl_lock);
	drm_atomic_helper_commit_planes(dev, old_state, DRM_PLANE_COMMIT_ACTIVE_ONLY);
	if (prv->need_ovl_lock)
		mutex_unlock(&prv->ovl_lock);

	drm_atomic_helper_fake_vblank(old_state);

	rockchip_drm_atomic_helper_connector_commit(dev, old_state);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_wait_for_vblanks(dev, old_state);

	rockchip_drm_atomic_helper_get_crc(dev, old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);
}

static const struct drm_mode_config_helper_funcs rockchip_mode_config_helpers = {
	.atomic_commit_tail = rockchip_drm_atomic_helper_commit_tail_rpm,
#ifdef CONFIG_DRM_DISPLAY_DP_HELPER
	.atomic_commit_setup = drm_dp_mst_atomic_setup_commit,
#endif
};

static struct drm_framebuffer *
rockchip_fb_create(struct drm_device *dev, struct drm_file *file,
		   const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_afbc_framebuffer *afbc_fb;
	const struct drm_format_info *info;
	int ret, i;

	info = drm_get_format_info(dev, mode_cmd);
	if (!info)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < info->num_planes; ++i) {
		if (mode_cmd->pitches[i] % 4) {
			DRM_DEV_ERROR_RATELIMITED(dev->dev,
				"fb pitch[%d] must be 4 byte aligned: %d\n", i, mode_cmd->pitches[i]);
			return ERR_PTR(-EINVAL);
		}
	}

	afbc_fb = kzalloc(sizeof(*afbc_fb), GFP_KERNEL);
	if (!afbc_fb)
		return ERR_PTR(-ENOMEM);

	ret = drm_gem_fb_init_with_funcs(dev, &afbc_fb->base, file, mode_cmd,
					 &rockchip_drm_fb_funcs);
	if (ret) {
		kfree(afbc_fb);
		return ERR_PTR(ret);
	}

	if (drm_is_afbc(mode_cmd->modifier[0])) {
		ret = drm_gem_fb_afbc_init(dev, mode_cmd, afbc_fb);
		if (ret) {
			struct drm_gem_object **obj = afbc_fb->base.obj;

			for (i = 0; i < info->num_planes; ++i)
				drm_gem_object_put(obj[i]);

			kfree(afbc_fb);
			return ERR_PTR(ret);
		}
	}

	return &afbc_fb->base;
}

static int rockchip_atomic_check(struct drm_device *dev, struct drm_atomic_state *state)
{
	int ret;

	ret = drm_atomic_helper_check(dev, state);
	if (ret)
		return ret;

#ifdef CONFIG_DRM_DISPLAY_DP_HELPER
	ret = drm_dp_mst_atomic_check(state);
	if (ret)
		return ret;
#endif
	return 0;
}

static const struct drm_mode_config_funcs rockchip_drm_mode_config_funcs = {
	.fb_create = rockchip_fb_create,
	.atomic_check = rockchip_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

struct drm_framebuffer *
rockchip_drm_framebuffer_init(struct drm_device *dev,
			      const struct drm_mode_fb_cmd2 *mode_cmd,
			      struct drm_gem_object *obj)
{
	struct drm_framebuffer *fb;

	fb = rockchip_fb_alloc(dev, mode_cmd, &obj, 1);
	if (IS_ERR(fb))
		return ERR_CAST(fb);

	return fb;
}

void rockchip_drm_mode_config_init(struct drm_device *dev)
{
	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * set max width and height as default value(16384x16384).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	dev->mode_config.max_width = 16384;
	dev->mode_config.max_height = 16384;
	dev->mode_config.async_page_flip = true;

	dev->mode_config.funcs = &rockchip_drm_mode_config_funcs;
	dev->mode_config.helper_private = &rockchip_mode_config_helpers;

	dev->mode_config.normalize_zpos = true;
}
