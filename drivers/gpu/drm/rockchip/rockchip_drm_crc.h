/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2026 Rockchip Electronics Co., Ltd.
 *
 * Shared types of the rockchip region CRC framework.  The hardware
 * access, locking and CRTC/window integration stay in the CRTC driver;
 * the debugfs interface in rockchip_drm_crc.c reaches it only through
 * the rockchip_crtc_crc_funcs callbacks.
 */
#ifndef _ROCKCHIP_DRM_CRC_H_
#define _ROCKCHIP_DRM_CRC_H_

#include <linux/types.h>

struct drm_crtc;
struct dentry;

/* Number of CRC regions per CRC block */
#define ROCKCHIP_CRC_REGION_NR				4

/* Size of the KEY_VALUE0~3 pool shared by all regions of a CRC block */
#define ROCKCHIP_CRC_KEY_VALUE_NR			4

/*
 * Region CRC software config, exchanged between the CRTC driver (which
 * owns the hardware access and locking) and the debugfs interface in
 * rockchip_drm_crc.c (which only parses input and prints snapshots).
 */
enum rockchip_crc_key_mode {
	ROCKCHIP_CRC_KEY_OFF = 0,
	ROCKCHIP_CRC_KEY_ALPHA,
	ROCKCHIP_CRC_KEY_COLOR,
};

struct rockchip_crc_region {
	bool enable;
	bool check_en;
	enum rockchip_crc_key_mode key_mode;
	bool freeze_en;
	u16 x1;
	u16 y1;
	u16 x2;
	u16 y2;
	u32 expected_crc;
};

#if IS_ENABLED(CONFIG_ROCKCHIP_DRM_DEBUG)
void rockchip_drm_crc_debugfs_init(struct drm_crtc *crtc, struct dentry *root);
#else
static inline void rockchip_drm_crc_debugfs_init(struct drm_crtc *crtc, struct dentry *root)
{
}
#endif

#endif /* _ROCKCHIP_DRM_CRC_H_ */
