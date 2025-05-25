/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 * Author: Sandy Huang <hjc@rock-chips.com>
 */

#ifndef ROCKCHIP_DRM_DEBUGFS_H
#define ROCKCHIP_DRM_DEBUGFS_H

struct vop_dump_info;

/**
 * @DUMP_DISABLE: Disable dump and do not record plane info into list.
 * @DUMP_KEEP: Record plane info into list and keep to dump plane.
 */
enum vop_dump_status {
	DUMP_DISABLE = 0,
	DUMP_KEEP
};

#if defined(CONFIG_ROCKCHIP_DRM_DEBUG)
#if defined(CONFIG_NO_GKI)
int rockchip_drm_add_dump_buffer(struct drm_crtc *crtc, struct dentry *root);
int rockchip_drm_crtc_dump_plane_buffer(struct drm_crtc *crtc);
#else
static inline int
rockchip_drm_add_dump_buffer(struct drm_crtc *crtc, struct dentry *root)
{
	return 0;
}

static inline int
rockchip_drm_crtc_dump_plane_buffer(struct drm_crtc *crtc)
{
	return 0;
}
#endif
int rockchip_drm_debugfs_add_color_bar(struct drm_crtc *crtc, struct dentry *root);
int rockchip_drm_debugfs_add_regs_write(struct drm_crtc *crtc, struct dentry *root);
int rockchip_drm_debugfs_add_dclk_rate(struct drm_crtc *crtc, struct dentry *root);
int rockchip_drm_debugfs_add_dovi_mode(struct drm_crtc *crtc, struct dentry *root);
#else
static inline int
rockchip_drm_add_dump_buffer(struct drm_crtc *crtc, struct dentry *root)
{
	return 0;
}

static inline int
rockchip_drm_crtc_dump_plane_buffer(struct drm_crtc *crtc)
{
	return 0;
}

static inline int
rockchip_drm_debugfs_add_color_bar(struct drm_crtc *crtc, struct dentry *root)
{
	return 0;
}

static inline int
rockchip_drm_debugfs_add_regs_write(struct drm_crtc *crtc, struct dentry *root)
{
	return 0;
}

static inline int
rockchip_drm_debugfs_add_dclk_rate(struct drm_crtc *crtc, struct dentry *root)
{
	return 0;
}

static inline int
rockchip_drm_debugfs_add_dovi_mode(struct drm_crtc *crtc, struct dentry *root)
{
	return 0;
}
#endif

#endif
