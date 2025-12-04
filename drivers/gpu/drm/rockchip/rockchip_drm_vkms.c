// SPDX-License-Identifier: GPL-2.0+
/*
 * Virtual display driver based on vkms
 *
 */

#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <drm/drm_gem.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#define DRIVER_NAME	"rockchip-vkms"

#define XRES_MIN	32
#define YRES_MIN	32

#define XRES_DEF	1024
#define YRES_DEF	768

#define XRES_MAX	8192
#define YRES_MAX	8192

#define VKMS_MAX_CRTC	8

static struct platform_device *vkms_pdev;

struct rockchip_vkms_connector {
	struct drm_connector connector;
	bool disconnected;
};

struct rockchip_vkms_crtc {
	struct drm_crtc crtc;
	struct drm_plane plane;
	struct drm_encoder encoder;
	struct rockchip_vkms_connector vconn;
	struct hrtimer vblank_hrtimer;
	ktime_t period_ns;
	struct drm_pending_vblank_event *event;

	struct drm_property *is_virtual_prop;
	struct drm_property *soc_id_prop;
};

struct rockchip_vkms {
	struct device *dev;
	struct drm_device *drm_dev;
	struct platform_device *pdev;

	struct rockchip_vkms_crtc vcrtc[VKMS_MAX_CRTC];
	u8 vconn_disconnected[VKMS_MAX_CRTC];

	uint32_t crtc_mask;
	uint32_t crtc_num;
};

static const u32 rockchip_vkms_formats[] = {
	DRM_FORMAT_XRGB8888,
};

#define drm_crtc_to_rockchip_vkms_crtc(crtc) \
	container_of(crtc, struct rockchip_vkms_crtc, crtc)


static const struct drm_plane_funcs rockchip_vkms_plane_funcs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= drm_plane_cleanup,
	.reset			= drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_plane_destroy_state,
};

static void rockchip_vkms_plane_atomic_update(struct drm_plane *plane, struct drm_atomic_state *state)
{
}

static const struct drm_plane_helper_funcs rockchip_vkms_plane_helper_funcs = {
	.atomic_update		= rockchip_vkms_plane_atomic_update,
};

static int rockchip_vkms_plane_init(struct drm_device *dev, struct drm_plane *primary)
{
	int ret;

	ret = drm_universal_plane_init(dev, primary, 0,
				       &rockchip_vkms_plane_funcs,
				       rockchip_vkms_formats, ARRAY_SIZE(rockchip_vkms_formats),
				       NULL, DRM_PLANE_TYPE_PRIMARY, NULL);
	if (ret)
		return ret;

	drm_plane_helper_add(primary, &rockchip_vkms_plane_helper_funcs);

	return 0;
}

static enum hrtimer_restart rockchip_vkms_vblank_simulate(struct hrtimer *timer)
{
	struct rockchip_vkms_crtc *vcrtc = container_of(timer, struct rockchip_vkms_crtc, vblank_hrtimer);
	struct drm_crtc *crtc = &vcrtc->crtc;
	bool ret;

	hrtimer_forward_now(&vcrtc->vblank_hrtimer, vcrtc->period_ns);

	ret = drm_crtc_handle_vblank(crtc);
	/* Don't queue timer again when vblank is disabled. */
	if (!ret) {
		drm_dbg(crtc->dev, "vblank is already disabled\n");
		return HRTIMER_NORESTART;
	}

	return HRTIMER_RESTART;
}

static int rockchip_vkms_enable_vblank(struct drm_crtc *crtc)
{
	struct rockchip_vkms_crtc *vcrtc = drm_crtc_to_rockchip_vkms_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	drm_calc_timestamping_constants(crtc, &crtc->mode);

	vcrtc->period_ns = ktime_set(0, vblank->framedur_ns);
	hrtimer_start(&vcrtc->vblank_hrtimer, vcrtc->period_ns, HRTIMER_MODE_REL);

	return 0;
}

static void rockchip_vkms_disable_vblank(struct drm_crtc *crtc)
{
	struct rockchip_vkms_crtc *vcrtc = drm_crtc_to_rockchip_vkms_crtc(crtc);

	hrtimer_try_to_cancel(&vcrtc->vblank_hrtimer);
}

static void rockchip_vkms_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static enum drm_connector_status
rockchip_vkms_connector_detect(struct drm_connector *connector, bool force)
{
	struct rockchip_vkms_connector *vconn = container_of(connector,
							     struct rockchip_vkms_connector,
							     connector);

	if (vconn->disconnected)
		return connector_status_disconnected;
	else
		return connector_status_connected;
}

static const struct drm_connector_funcs rockchip_vkms_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = rockchip_vkms_connector_detect,
	.destroy = rockchip_vkms_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_encoder_funcs rockchip_vkms_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static struct drm_display_mode rockchip_vkms_modes_builtin[] = {
	/* 1280x720@30Hz */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 37125, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1920x1080@30Hz */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2560x1440@30Hz */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 120750, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1481, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 3840x2160@30Hz */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 4096x2160@30Hz */
	{ DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 297000, 4096, 4184,
		   4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 720x1280@30Hz */
	{ DRM_MODE("720x1280", DRM_MODE_TYPE_DRIVER, 37125, 720, 725,
		   730, 750, 0, 1280, 1390, 1430, 1650, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1080x1920@30Hz */
	{ DRM_MODE("1080x1920", DRM_MODE_TYPE_DRIVER, 74250, 1080, 1084,
		   1089, 1125, 0, 1920, 2008, 2052, 2200, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1440x2560@30Hz */
	{ DRM_MODE("1440x2560", DRM_MODE_TYPE_DRIVER, 120750, 1440, 1443,
		   1448, 1481, 0, 2560, 2608, 2640, 2720, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 2160x3840@30Hz */
	{ DRM_MODE("2160x3840", DRM_MODE_TYPE_DRIVER, 297000, 2160, 2168,
		   2178, 2250, 0, 3840, 4016, 4104, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2160x4096@30Hz */
	{ DRM_MODE("2160x4096", DRM_MODE_TYPE_DRIVER, 297000, 2160, 2168,
		   2178, 2250, 0, 4096, 4184, 4272, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },

	/* 1280x720@60Hz */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1920x1080@60Hz */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2560x1440@60Hz */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 241500, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1481, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 3840x2160@60Hz */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 4096x2160@60Hz */
	{ DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 594000, 4096, 4184,
		   4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 720x1280@60Hz */
	{ DRM_MODE("720x1280", DRM_MODE_TYPE_DRIVER, 74250, 720, 725,
		   730, 750, 0, 1280, 1390, 1430, 1650, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1080x1920@60Hz */
	{ DRM_MODE("1080x1920", DRM_MODE_TYPE_DRIVER, 148500, 1080, 1084,
		   1089, 1125, 0, 1920, 2008, 2052, 2200, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1440x2560@60Hz */
	{ DRM_MODE("1440x2560", DRM_MODE_TYPE_DRIVER, 241500, 1440, 1443,
		   1448, 1481, 0, 2560, 2608, 2640, 2720, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 2160x3840@60Hz */
	{ DRM_MODE("2160x3840", DRM_MODE_TYPE_DRIVER, 594000, 2160, 2168,
		   2178, 2250, 0, 3840, 4016, 4104, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2160x4096@60Hz */
	{ DRM_MODE("2160x4096", DRM_MODE_TYPE_DRIVER, 594000, 2160, 2168,
		   2178, 2250, 0, 4096, 4184, 4272, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },

	/* 1280x720@90Hz */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 111375, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1920x1080@90Hz */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 222750, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2560x1440@90Hz */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 362250, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1481, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 3840x2160@90Hz */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 891000, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 4096x2160@90Hz */
	{ DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 891000, 4096, 4184,
		   4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 720x1280@90Hz */
	{ DRM_MODE("720x1280", DRM_MODE_TYPE_DRIVER, 111375, 720, 725,
		   730, 750, 0, 1280, 1390, 1430, 1650, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1080x1920@90Hz */
	{ DRM_MODE("1080x1920", DRM_MODE_TYPE_DRIVER, 222750, 1080, 1084,
		   1089, 1125, 0, 1920, 2008, 2052, 2200, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1440x2560@90Hz */
	{ DRM_MODE("1440x2560", DRM_MODE_TYPE_DRIVER, 362250, 1440, 1443,
		   1448, 1481, 0, 2560, 2608, 2640, 2720, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 2160x3840@90Hz */
	{ DRM_MODE("2160x3840", DRM_MODE_TYPE_DRIVER, 891000, 2160, 2168,
		   2178, 2250, 0, 3840, 4016, 4104, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2160x4096@90Hz */
	{ DRM_MODE("2160x4096", DRM_MODE_TYPE_DRIVER, 891000, 2160, 2168,
		   2178, 2250, 0, 4096, 4184, 4272, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },

	/* 1280x720@120Hz */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1920x1080@120Hz */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2560x1440@120Hz */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 483000, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1481, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 3840x2160@120Hz */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 1188000, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 4096x2160@120Hz */
	{ DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 1188000, 4096, 4184,
		   4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 720x1280@120Hz */
	{ DRM_MODE("720x1280", DRM_MODE_TYPE_DRIVER, 148500, 720, 725,
		   730, 750, 0, 1280, 1390, 1430, 1650, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1080x1920@120Hz */
	{ DRM_MODE("1080x1920", DRM_MODE_TYPE_DRIVER, 297000, 1080, 1084,
		   1089, 1125, 0, 1920, 2008, 2052, 2200, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1440x2560@120Hz */
	{ DRM_MODE("1440x2560", DRM_MODE_TYPE_DRIVER, 483000, 1440, 1443,
		   1448, 1481, 0, 2560, 2608, 2640, 2720, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 2160x3840@120Hz */
	{ DRM_MODE("2160x3840", DRM_MODE_TYPE_DRIVER, 1188000, 2160, 2168,
		   2178, 2250, 0, 3840, 4016, 4104, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2160x4096@120Hz */
	{ DRM_MODE("2160x4096", DRM_MODE_TYPE_DRIVER, 1188000, 2160, 2168,
		   2178, 2250, 0, 4096, 4184, 4272, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },

	/* 1280x720@144Hz */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 178200, 1280, 1390,
		   1430, 1650, 0, 720, 725, 730, 750, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1920x1080@144Hz */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 356400, 1920, 2008,
		   2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2560x1440@144Hz */
	{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 483000, 2560, 2608,
		   2640, 2720, 0, 1440, 1443, 1448, 1481, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 3840x2160@144Hz */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 1425600, 3840, 4016,
		   4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 4096x2160@144Hz */
	{ DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 1425600, 4096, 4184,
		   4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 720x1280@144Hz */
	{ DRM_MODE("720x1280", DRM_MODE_TYPE_DRIVER, 178200, 720, 725,
		   730, 750, 0, 1280, 1390, 1430, 1650, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1080x1920@144Hz */
	{ DRM_MODE("1080x1920", DRM_MODE_TYPE_DRIVER, 356400, 1080, 1084,
		   1089, 1125, 0, 1920, 2008, 2052, 2200, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 1440x2560@144Hz */
	{ DRM_MODE("1440x2560", DRM_MODE_TYPE_DRIVER, 580000, 1440, 1443,
		   1448, 1481, 0, 2560, 2608, 2640, 2720, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) },
	/* 2160x3840@144Hz */
	{ DRM_MODE("2160x3840", DRM_MODE_TYPE_DRIVER, 1425600, 2160, 2168,
		   2178, 2250, 0, 3840, 4016, 4104, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
	/* 2160x4096@144Hz */
	{ DRM_MODE("2160x4096", DRM_MODE_TYPE_DRIVER, 1425600, 2160, 2168,
		   2178, 2250, 0, 4096, 4184, 4272, 4400, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
};

static int rockchip_vkms_conn_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode = NULL;
	struct drm_display_mode *bmode;
	int count = 0;
	int i;

	count += drm_add_modes_noedid(connector, XRES_MAX, YRES_MAX);
	for (i = 0; i < ARRAY_SIZE(rockchip_vkms_modes_builtin); i++) {
		bmode = &rockchip_vkms_modes_builtin[i];

		mode = drm_mode_duplicate(connector->dev, bmode);
		if (!mode)
			return 0;

		drm_mode_probed_add(connector, mode);
		count++;
	}
	drm_set_preferred_mode(connector, XRES_DEF, YRES_DEF);

	return count;
}

static const struct drm_connector_helper_funcs rockchip_vkms_conn_helper_funcs = {
	.get_modes    = rockchip_vkms_conn_get_modes,
};

static const struct drm_crtc_funcs rockchip_vkms_crtc_funcs = {
	.set_config		= drm_atomic_helper_set_config,
	.destroy		= drm_crtc_cleanup,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state	= drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_crtc_destroy_state,
	.enable_vblank		= rockchip_vkms_enable_vblank,
	.disable_vblank		= rockchip_vkms_disable_vblank,
};

static void rockchip_vkms_crtc_atomic_enable(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
	drm_crtc_vblank_on(crtc);
}

static void rockchip_vkms_crtc_atomic_disable(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
	unsigned long flags;

	drm_crtc_vblank_off(crtc);
	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

		crtc->state->event = NULL;
	}
}

static void rockchip_vkms_crtc_atomic_flush(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
	unsigned long flags;

	if (crtc->state->event) {
		spin_lock_irqsave(&crtc->dev->event_lock, flags);

		if (drm_crtc_vblank_get(crtc) != 0)
			drm_crtc_send_vblank_event(crtc, crtc->state->event);
		else
			drm_crtc_arm_vblank_event(crtc, crtc->state->event);

		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

		crtc->state->event = NULL;
	}
}

static const struct drm_crtc_helper_funcs rockchip_vkms_crtc_helper_funcs = {
	.atomic_flush	= rockchip_vkms_crtc_atomic_flush,
	.atomic_enable	= rockchip_vkms_crtc_atomic_enable,
	.atomic_disable	= rockchip_vkms_crtc_atomic_disable,
};

static u64 rockchip_vkms_get_soc_id(void)
{
	if (of_machine_is_compatible("rockchip,rk3588"))
		return 0x3588;
	else if (of_machine_is_compatible("rockchip,rk3576"))
		return 0x3576;
	else if (of_machine_is_compatible("rockchip,rk3568"))
		return 0x3568;
	else if (of_machine_is_compatible("rockchip,rk3566"))
		return 0x3566;
	else if (of_machine_is_compatible("rockchip,rk3562"))
		return 0x3562;
	else if (of_machine_is_compatible("rockchip,rk3528"))
		return 0x3528;
	else
		return 0;
}

static void rockchip_vkms_crtc_deinit(struct drm_crtc *crtc)
{
	struct rockchip_vkms_crtc *vcrtc = drm_crtc_to_rockchip_vkms_crtc(crtc);

	hrtimer_cancel(&vcrtc->vblank_hrtimer);
	drm_crtc_cleanup(crtc);
}

static int rockchip_vkms_crtc_init(struct drm_device *dev, struct drm_crtc *crtc,
			  struct drm_plane *primary, struct drm_plane *cursor)
{
	struct rockchip_vkms_crtc *vcrtc = drm_crtc_to_rockchip_vkms_crtc(crtc);
	int ret;

	ret = drm_crtc_init_with_planes(dev, crtc, primary, cursor,
					&rockchip_vkms_crtc_funcs, NULL);
	if (ret) {
		DRM_ERROR("Failed to init CRTC\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, &rockchip_vkms_crtc_helper_funcs);

	vcrtc->is_virtual_prop = drm_property_create_object(dev,
							    DRM_MODE_PROP_ATOMIC |
							    DRM_MODE_PROP_IMMUTABLE,
							    "IS_VIRTUAL", DRM_MODE_OBJECT_CRTC);
	drm_object_attach_property(&crtc->base, vcrtc->is_virtual_prop, 1);

	vcrtc->soc_id_prop = drm_property_create_object(dev,
							DRM_MODE_PROP_ATOMIC |
							DRM_MODE_PROP_IMMUTABLE,
							"SOC_ID", DRM_MODE_OBJECT_CRTC);
	drm_object_attach_property(&crtc->base, vcrtc->soc_id_prop, rockchip_vkms_get_soc_id());

	hrtimer_init(&vcrtc->vblank_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vcrtc->vblank_hrtimer.function = &rockchip_vkms_vblank_simulate;

	return ret;
}

static int rockchip_vkms_create_crtc(struct rockchip_vkms *rockchip_vkms, int index)
{
	struct drm_crtc *crtc = &rockchip_vkms->vcrtc[index].crtc;
	struct rockchip_vkms_connector *vconn = &rockchip_vkms->vcrtc[index].vconn;
	struct drm_connector *connector = &vconn->connector;
	struct drm_encoder *encoder = &rockchip_vkms->vcrtc[index].encoder;
	struct drm_plane *primary = &rockchip_vkms->vcrtc[index].plane;
	int ret;

	ret = rockchip_vkms_plane_init(rockchip_vkms->drm_dev, primary);
	if (ret) {
		DRM_ERROR("Failed to init primary plane for crtc-%d\n", index);
		return ret;
	}

	ret = rockchip_vkms_crtc_init(rockchip_vkms->drm_dev, crtc, primary, NULL);
	if (ret) {
		DRM_ERROR("Failed to init crtc-%d\n", index);
		goto err_crtc;
	}

	vconn->disconnected = rockchip_vkms->vconn_disconnected[index];

	ret = drm_connector_init(rockchip_vkms->drm_dev, connector, &rockchip_vkms_connector_funcs,
				 DRM_MODE_CONNECTOR_VIRTUAL);
	if (ret) {
		DRM_ERROR("Failed to init connector-%d\n", index);
		goto err_connector;
	}
	drm_connector_helper_add(connector, &rockchip_vkms_conn_helper_funcs);

	ret = drm_connector_register(connector);
	if (ret) {
		DRM_ERROR("Failed to register connector-%d\n", index);
		goto err_connector_register;
	}

	ret = drm_encoder_init(rockchip_vkms->drm_dev, encoder, &rockchip_vkms_encoder_funcs,
			       DRM_MODE_ENCODER_VIRTUAL, NULL);
	if (ret) {
		DRM_ERROR("Failed to init encoder-%d\n", index);
		goto err_encoder;
	}
	encoder->possible_crtcs = BIT(index);

	ret = drm_connector_attach_encoder(connector, encoder);
	if (ret) {
		DRM_ERROR("Failed to attach connector-%d to encoder-%d\n", index, index);
		goto err_attach;
	}

	rockchip_vkms->crtc_mask |= drm_crtc_mask(crtc);

	return 0;

err_attach:
	drm_encoder_cleanup(encoder);
err_encoder:
	drm_connector_unregister(connector);
err_connector_register:
	drm_connector_cleanup(connector);
err_connector:
	rockchip_vkms_crtc_deinit(crtc);
err_crtc:
	drm_plane_cleanup(primary);

	return ret;
}

static int rockchip_vkms_create_crtcs(struct rockchip_vkms *rockchip_vkms)
{
	int ret;
	int i;

	for (i = 0; i < rockchip_vkms->crtc_num; i++) {
		ret = rockchip_vkms_create_crtc(rockchip_vkms, i);
		if (ret) {
			DRM_WARN("Failed to create virtual crtc, index = %d\n", i);
			break;
		}
		DRM_INFO("Create %d(total: %d) virtual crtcs\n", i, rockchip_vkms->crtc_num);
	}

	return 0;
}

static int rockchip_vkms_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct rockchip_vkms *rockchip_vkms;
	struct device_node *np = NULL;
	u32 disconnected[VKMS_MAX_CRTC];
	int len, i;

	rockchip_vkms = devm_kzalloc(dev, sizeof(*rockchip_vkms), GFP_KERNEL);
	if (!rockchip_vkms)
		return -ENOMEM;

	rockchip_vkms->crtc_num = VKMS_MAX_CRTC;

	np = of_find_compatible_node(NULL, NULL, "rockchip,vkms");
	if (np) {
		of_node_put(np);

		of_property_read_u32(np, "rockchip,vkms-crtc-num", &rockchip_vkms->crtc_num);
		if (rockchip_vkms->crtc_num > VKMS_MAX_CRTC) {
			DRM_ERROR("rockchip,vkms-crtc-num = %d in dts exceeds VKMS_MAX_CRTC(%d)\n",
				  rockchip_vkms->crtc_num, VKMS_MAX_CRTC);
			rockchip_vkms->crtc_num = VKMS_MAX_CRTC;
		}

		len = of_property_read_variable_u32_array(np, "rockchip,vkms-conn-disconnected",
							  disconnected, 1, VKMS_MAX_CRTC);
		for (i = 0; i < len; i++) {
			if (disconnected[i] < rockchip_vkms->crtc_num)
				rockchip_vkms->vconn_disconnected[disconnected[i]] = 1;
			else
				DRM_ERROR("vkms-conn-disconnected index %d exceeds crtc_num(%d)\n",
					  i, rockchip_vkms->crtc_num);
		}
	}

	rockchip_vkms->dev = dev;
	rockchip_vkms->drm_dev = drm_dev;
	dev_set_drvdata(dev, rockchip_vkms);

	rockchip_vkms_create_crtcs(rockchip_vkms);

	return 0;
}

static void rockchip_vkms_unbind(struct device *dev, struct device *master, void *data)
{
	struct rockchip_vkms *rockchip_vkms = dev_get_drvdata(dev);
	struct drm_device *drm_dev = rockchip_vkms->drm_dev;
	struct list_head *crtc_list = &drm_dev->mode_config.crtc_list;
	struct drm_crtc *crtc, *tmp_crtc;
	struct rockchip_vkms_crtc *vcrtc;

	list_for_each_entry_safe(crtc, tmp_crtc, crtc_list, head) {
		if (rockchip_vkms->crtc_mask & drm_crtc_mask(crtc)) {
			vcrtc = drm_crtc_to_rockchip_vkms_crtc(crtc);

			drm_encoder_cleanup(&vcrtc->encoder);
			drm_connector_unregister(&vcrtc->vconn.connector);
			drm_connector_cleanup(&vcrtc->vconn.connector);
			drm_plane_cleanup(&vcrtc->plane);
			rockchip_vkms->crtc_mask &= ~(drm_crtc_mask(crtc));
			rockchip_vkms_crtc_deinit(crtc);
		}
	}
}

const struct component_ops rockchip_vkms_component_ops = {
	.bind = rockchip_vkms_bind,
	.unbind = rockchip_vkms_unbind,
};

static int rockchip_vkms_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	DRM_DEV_INFO(dev, "virtual vop probe\n");

	return component_add(dev, &rockchip_vkms_component_ops);
}

static int rockchip_vkms_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rockchip_vkms_component_ops);

	return 0;
}

struct platform_driver rockchip_vkms_platform_driver = {
	.probe = rockchip_vkms_probe,
	.remove = rockchip_vkms_remove,
	.driver = {
		.name = DRIVER_NAME,
	},
};

static int __init rockchip_vkms_init(void)
{
	vkms_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
	if (IS_ERR(vkms_pdev)) {
		DRM_ERROR("failed to register platform device %s\n", DRIVER_NAME);
		return PTR_ERR(vkms_pdev);
	}

	return 0;
}

static void __exit rockchip_vkms_exit(void)
{
	platform_device_unregister(vkms_pdev);
}

rootfs_initcall(rockchip_vkms_init);
module_exit(rockchip_vkms_exit);

MODULE_AUTHOR("Andy Yan <rock-chips@.com>");
MODULE_LICENSE("GPL");
